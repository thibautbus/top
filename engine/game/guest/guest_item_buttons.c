/* Item hotkeys, the `use` mode: which of the two item buttons the
 * game reads in the state Link is in, and where the inventory's cursor is. */
#include "guest_internal.h"

#include <string.h>

void oracles_guest_item_buttons_state(OraclesGuest *guest, OraclesGuestInventoryState *s)
{
    const OraclesGuestTables *t = guest->tables;
    s->items_disabled = oracles_guest_read8(guest, t->items_disabled);
    s->in_shop = oracles_guest_read8(guest, t->in_shop);
    s->link_in_air = oracles_guest_read8(guest, t->link_in_air);
    s->link_in_spinner = oracles_guest_read8(guest, t->link_in_spinner);
    s->link_grabbed = oracles_guest_read8(guest, t->link_grabbed);
    s->link_grab_state = oracles_guest_read8(guest, t->link_grab_state);
    s->link_climbing_vine = oracles_guest_read8(guest, t->link_climbing_vine);
    s->tileset_flags = oracles_guest_read8(guest, t->tileset_flags);
    s->link_swimming_state = oracles_guest_read8(guest, t->link_swimming_state);
    s->disabled_objects = oracles_guest_read8(guest, t->disabled_objects);
    s->link_object_index = oracles_guest_read8(guest, t->link_object_index);
    s->palette_thread_mode = oracles_guest_read8(guest, t->palette_thread_mode);
    const uint8_t *link = oracles_guest_object(guest, 0, 0);   /* w1Link */
    s->link_var2f = link ? link[ORACLES_OBJ_VAR2F] : 0u;
    const uint8_t *companion = oracles_guest_object(guest, 1, 0);   /* w1Companion */
    s->companion_id = companion ? companion[ORACLES_OBJ_ID] : 0u;
    s->menu_inventory = t->menu_inventory;
    s->specialobject_minecart = t->specialobject_minecart;
    s->specialobject_raft = t->specialobject_raft;
    s->tilesetflag_bit_sidescroll = t->tilesetflag_bit_sidescroll;
    s->tilesetflag_bit_underwater = t->tilesetflag_bit_underwater;
}

/* What stops Link's update before checkUseItems (link.s: wDisabledObjects & $81, a mount). */
static int update_stops(const OraclesGuestInventoryState *s)
{
    if (s->disabled_objects & 0x81u) return 1;
    /* A scroll between two rooms, or the palette changing (a fade into a dark room, a season): Link's update returns
     * before it uses items (linkState01), and a press begun there is no longer new when it ends. */
    if ((s->scroll_mode & 0x0eu) || s->palette_thread_mode) return 1;
    /* A minecart, and the raft of Ages, carry Link as a mount does (wLinkObjectIndex $d1), but his update goes on to
     * checkUseItems in them (link.s): seeds are shot and the sword swung from a minecart.  On an animal B dismounts and A
     * is the animal's action. */
    const int vehicle = s->companion_id == s->specialobject_minecart || (s->game == ORACLES_GAME_AGES && s->companion_id == s->specialobject_raft);
    return (s->link_object_index & 0x01u) && !vehicle;
}

/* checkUseItems (parentItemUsage.s), branch for branch, after what stops
 * Link's update before it. */
unsigned oracles_guest_item_buttons(const OraclesGuestInventoryState *s)
{
    const unsigned both = ORACLES_BUTTON_A | ORACLES_BUTTON_B;
    if (update_stops(s)) return 0u;
    if (s->items_disabled & 0x80u) return 0u;
    if (s->in_shop) return 0u;                                                         /* A and B take the article on the counter */
    if ((s->link_in_air | s->link_in_spinner) & 0x80u) return 0u;
    if (s->link_grabbed | s->link_grab_state) return 0u;
    if (s->link_climbing_vine == 0xffu) return 0u;
    if (s->tileset_flags & (1u << s->tilesetflag_bit_sidescroll)) {
        if (!s->link_swimming_state) return both;
        if (s->game == ORACLES_GAME_AGES) return (s->link_var2f & 0x80u) ? both : ORACLES_BUTTON_B;
        return ORACLES_BUTTON_B;
    }
    if (s->tilesetflag_bit_underwater != 0xffu && (s->tileset_flags & (1u << s->tilesetflag_bit_underwater))) return ORACLES_BUTTON_A;
    return s->link_swimming_state ? 0u : both;
}

/* Holding: wLinkGrabState $83 with the bombs or the bracelet, the items that lift, in their state 3 (a lever pulled is
 * $83 too, in state 5; another item in its state 3, the sword charged, holds nothing).  The
 * parent items still update under @itemsDisabled, not in a shop; in the air (wLinkInAir bit 7) state 3 waits. */
unsigned oracles_guest_item_throw(const OraclesGuestInventoryState *s)
{
    if (update_stops(s) || s->in_shop || (s->link_in_air & 0x80u) || s->link_grab_state != 0x83u) return 0u;
    for (unsigned i = 0; i < 4u; i++) {   /* the parent items, not w1WeaponItem */
        if (!s->parents[i].enabled || s->parents[i].state != 3u || (s->parents[i].id != s->item_bomb && s->parents[i].id != s->item_bracelet)) continue;
        return (s->parents[i].button & ORACLES_BUTTON_A) && !(s->parents[i].button & ORACLES_BUTTON_B) ? ORACLES_BUTTON_A : ORACLES_BUTTON_B;
    }
    return 0u;
}

uint8_t oracles_guest_item_variant(const OraclesGuestInventoryState *s, uint8_t item)
{
    if (!item) return 0xffu;
    if (item == s->item_seed_satchel) return s->satchel_seeds;
    if (item == s->item_seed_shooter) return s->shooter_seeds;
    if (item == s->item_slingshot && s->slingshot_seeds != 0xffu) return s->slingshot_seeds;
    if (item == s->item_harp && s->harp_song != 0xffu) return s->harp_song;
    return 0xffu;
}

int oracles_guest_inventory_cursor(OraclesGuest *guest, unsigned *slot)
{
    const OraclesGuestTables *t = guest->tables;
    if (oracles_guest_read8(guest, t->opened_menu_type) != t->menu_inventory) return 0;
    if (oracles_guest_read8(guest, t->menu_load_state) != 1u || oracles_guest_read8(guest, t->menu_active_state) != 1u) return 0;
    if (oracles_guest_read8(guest, t->inventory_submenu) != 0u || oracles_guest_read8(guest, t->palette_thread_mode) != 0u) return 0;
    const unsigned cursor = oracles_guest_read8(guest, t->inventory_submenu0_cursor_pos);
    if (cursor >= t->inventory_storage_size || cursor + 2u >= ORACLES_INVENTORY_SLOTS) return 0;
    if (slot) *slot = cursor + 2u;
    return 1;
}

/* One sprite's graphics as loadItemIconGfx copies them: the index doubled on eight bits, times sixteen, from
 * spr_item_icons_1 in its bank; in Ages a harp song's index past $a3 moves two on, to the small version of its icon.  In
 * Oracles Combined, in either half, a song's index from $a3 to $ae does, and the gloves' icon is spr_extra_mags. */
static const uint8_t *icon_graphics(const OraclesGuestTables *t, const OraclesGuestRom *rom, int ages, int both, int gloves, uint8_t index)
{
    if (both && gloves) return t->spr_extra_mags.bank == ORACLES_GUEST_ABSENT ? NULL : oracles_guest_rom_at(rom, t->spr_extra_mags, 32u);
    if (both ? index >= 0xa3u && index < 0xafu : ages && index >= 0xa3u) index = (uint8_t)(index + 2u);
    const unsigned offset = (unsigned)(uint8_t)(index * 2u) * 16u;
    if ((unsigned)t->spr_item_icons_1.addr + offset + 32u > 0x8000u) return NULL;
    return oracles_guest_rom_at(rom, (OraclesGuestSym){ t->spr_item_icons_1.bank, (uint16_t)(t->spr_item_icons_1.addr + offset) }, 32u);
}

/* Oracles Combined's loadTreasureDisplayData (treasureAndDrops.s): the items whose sub-table starts at level 1 show it at
 * level 0, the shield, the sword, the boomerang, the switch hook, the bracelet and the feather (TREASURE_*, the same in
 * both halves). */
static int starts_at_level_1(uint8_t item)
{
    return item == 0x01u || item == 0x05u || item == 0x06u || item == 0x0au || item == 0x16u || item == 0x17u;
}

int oracles_guest_item_sprites(OraclesGuest *guest, uint8_t item, uint8_t variant, OraclesGuestItemSprites *out)
{
    memset(out, 0, sizeof *out);
    const OraclesGuestTables *t = guest ? guest->tables : NULL;
    if (!t || !item || t->treasure_display_data1.bank == ORACLES_GUEST_ABSENT || t->treasure_display_data2.bank == ORACLES_GUEST_ABSENT
        || t->c600_block.bank == ORACLES_GUEST_ABSENT || t->spr_item_icons_1.bank == ORACLES_GUEST_ABSENT) return 0;
    const OraclesGuestRom rom = oracles_guest_rom(guest);
    const int ages = oracles_compat_family(guest->profile) == ORACLES_GAME_AGES;
    const int both = oracles_compat_both_games_items(guest->profile);
    /* @getTableIndices: the item's row of the first table, or the closing one, three bytes each. */
    const uint8_t *row = NULL;
    for (unsigned i = 0; i < 32u; i++) {
        row = oracles_guest_rom_at(&rom, (OraclesGuestSym){ t->treasure_display_data1.bank, (uint16_t)(t->treasure_display_data1.addr + 3u * i) }, 6u);
        if (!row) return 0;
        if (!row[0] || row[0] == item) break;
    }
    if ((!ages || both) && row[0] && row[0] == t->item_slingshot && t->slingshot_level.bank != ORACLES_GUEST_ABSENT && oracles_guest_read8(guest, t->slingshot_level) == 2u) row += 3;
    uint8_t level = item;
    int gloves = 0;
    if (row[1]) {
        const OraclesGuestSym at = { t->c600_block.bank, (uint16_t)((t->c600_block.addr & 0xff00u) | row[1]) };
        const int seeds_or_song = (at.addr == t->satchel_selected_seeds.addr && item == t->item_seed_satchel)
                               || (at.addr == t->shooter_selected_seeds.addr && (item == t->item_shooter || item == t->item_slingshot))
                               || (t->slingshot_selected_seeds.bank != ORACLES_GUEST_ABSENT && at.addr == t->slingshot_selected_seeds.addr && item == t->item_slingshot)
                               || (t->selected_harp_song.bank != ORACLES_GUEST_ABSENT && at.addr == t->selected_harp_song.addr && item == t->item_harp);
        level = seeds_or_song && variant != 0xffu ? variant : oracles_guest_read8(guest, at);
        gloves = t->magnet_glove_polarity.bank != ORACLES_GUEST_ABSENT && at.addr == t->magnet_glove_polarity.addr;
        if (both && !level && starts_at_level_1(item)) level = 1u;
    }
    const uint8_t *word = oracles_guest_rom_at(&rom, (OraclesGuestSym){ t->treasure_display_data2.bank, (uint16_t)(t->treasure_display_data2.addr + 2u * row[2]) }, 2u);
    if (!word) return 0;
    const uint16_t table = (uint16_t)(word[0] | word[1] << 8);
    const uint8_t *entry = oracles_guest_rom_at(&rom, (OraclesGuestSym){ t->treasure_display_data2.bank, (uint16_t)(table + 7u * level) }, 7u);
    /* An item's icons are spr_item_icons' tiles, from $80: below, a filler row of an item with tables of its own, or of
     * an item the game does not have (Ages' slingshot), which the bar never draws. */
    if (!entry || entry[1] < 0x80u) return 0;
    /* loadEquippedItemSpriteData: the seeds' items below the threshold take the bar's palette for them. */
    uint8_t attribute = entry[2];
    if (entry[1] < (ages || both ? 0x84u : 0x86u)) attribute = (uint8_t)((attribute - 3u) | 1u);
    out->graphics[0] = icon_graphics(t, &rom, ages, both, gloves, entry[1]);
    out->attributes[0] = attribute;
    out->count = 1;
    out->shift = (ages || both) && item == t->item_harp ? 8u : 0u;   /* the bar's OAM routine: the harp eight pixels right */
    out->display_treasure = entry[0];
    out->display_mode = entry[5];
    if (entry[3]) { out->graphics[1] = icon_graphics(t, &rom, ages, both, gloves, entry[3]); out->attributes[1] = entry[4]; out->count = 2; }
    if (!out->graphics[0] || (out->count == 2u && !out->graphics[1])) { memset(out, 0, sizeof *out); return 0; }
    return 1;
}

/* checkTreasureObtained: whether the treasure is had, and the variable of wc600Block its treasureCollectionBehaviourTable
 * entry names, 0 for none. */
static int treasure_value(OraclesGuest *guest, uint8_t treasure, uint8_t *value)
{
    const OraclesGuestTables *t = guest->tables;
    *value = 0;
    if (!treasure) return 1;
    if (treasure >= t->first_upgrade_treasure) return 0;
    const OraclesGuestSym flag = { t->obtained_treasure_flags.bank, (uint16_t)(t->obtained_treasure_flags.addr + treasure / 8u) };
    if (!((oracles_guest_read8(guest, flag) >> (treasure % 8u)) & 1u)) return 0;
    const OraclesGuestRom rom = oracles_guest_rom(guest);
    const uint8_t *entry = oracles_guest_rom_at(&rom, (OraclesGuestSym){ t->treasure_collection_behaviours.bank, (uint16_t)(t->treasure_collection_behaviours.addr + 3u * treasure) }, 1u);
    if (!entry) return 0;
    if (entry[0]) *value = oracles_guest_read8(guest, (OraclesGuestSym){ t->c600_block.bank, (uint16_t)((t->c600_block.addr & 0xff00u) | entry[0]) });
    return 1;
}

static unsigned tile_at(OraclesGuestBarTile *out, unsigned n, uint8_t tile, uint8_t column, uint8_t row)
{
    out[n].tile = tile; out[n].attribute = 0x80u; out[n].column = column; out[n].row = row;   /* c = $80 on the buttons */
    return n + 1u;
}

/* The rod: each season had, its four tiles from `first`. */
static unsigned seasons_had(OraclesGuest *guest, OraclesGuestBarTile *out, uint8_t first)
{
    const OraclesGuestTables *t = guest->tables;
    unsigned n = 0;
    if (t->obtained_seasons.bank == ORACLES_GUEST_ABSENT) return 0;
    const uint8_t seasons = oracles_guest_read8(guest, t->obtained_seasons);
    if (seasons & 1u) n = tile_at(out, n, first, 2, 1);
    if (seasons & 2u) n = tile_at(out, n, (uint8_t)(first + 1u), 2, 0);
    if (seasons & 4u) n = tile_at(out, n, (uint8_t)(first + 2u), 3, 0);
    if (seasons & 8u) n = tile_at(out, n, (uint8_t)(first + 3u), 3, 1);
    return n;
}

unsigned oracles_guest_item_extra_tiles(OraclesGuest *guest, const OraclesGuestItemSprites *sprites, OraclesGuestBarTile out[4])
{
    const OraclesGuestTables *t = guest ? guest->tables : NULL;
    if (!t || !sprites->count || (sprites->display_mode & 0x80u) || t->treasure_collection_behaviours.bank == ORACLES_GUEST_ABSENT) return 0;
    uint8_t value = 0;
    if (!treasure_value(guest, sprites->display_treasure, &value)) return 0;   /* @drawItem draws from checkTreasureObtained's answer */
    const int ages = oracles_compat_family(guest->profile) == ORACLES_GAME_AGES;
    const int both = oracles_compat_both_games_items(guest->profile);
    unsigned n = 0;
    switch (sprites->display_mode) {
    case 1:   /* a quantity: its two BCD digits */
        n = tile_at(out, n, (uint8_t)(0x10u + (value >> 4)), 2, 1);
        return tile_at(out, n, (uint8_t)(0x10u + (value & 15u)), 3, 1);
    case 4:   /* a number after "x" (Ages' slates) */
        n = tile_at(out, n, 0x1bu, 2, 1);
        return tile_at(out, n, (uint8_t)(0x10u + (value & 15u)), 3, 1);
    case 2:
        if (ages || both) {   /* the harp: its notes, two by two, under the sprites moved right; Oracles Combined's Seasons half's from $5c */
            const uint8_t base = both && !ages ? 0x5cu : 0x1cu;
            n = tile_at(out, n, base, 1, 0); n = tile_at(out, n, (uint8_t)(base + 2u), 2, 0);
            n = tile_at(out, n, (uint8_t)(base + 1u), 1, 1); return tile_at(out, n, (uint8_t)(base + 3u), 2, 1);
        }
        return seasons_had(guest, out, 0x1cu);
    case 5:   /* Oracles Combined: the rod's seasons, from $2c in its Ages half */
        return both ? seasons_had(guest, out, ages ? 0x2cu : 0x1cu) : 0u;
    case 3:   /* Seasons: the gloves' N over the sprite's S; Oracles Combined's halves write the S too, Ages' from $30 */
        if ((ages && !both) || t->magnet_glove_polarity.bank == ORACLES_GUEST_ABSENT) return n;
        {
            const unsigned north = oracles_guest_read8(guest, t->magnet_glove_polarity) & 1u;
            if (both) return tile_at(out, n, ages ? (uint8_t)(0x30u + north) : north ? 0x0au : 0x5bu, 2, 1);
            if (north) n = tile_at(out, n, 0x0au, 2, 1);
        }
        return n;
    case 0:   /* a level: "L-" and its digit */
        n = tile_at(out, n, 0x1au, 2, 1);
        return tile_at(out, n, (uint8_t)(0x10u + (value & 15u)), 3, 1);
    default:
        return 0;
    }
}
