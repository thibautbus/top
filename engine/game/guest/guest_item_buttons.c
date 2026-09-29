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

/* checkUseItems (parentItemUsage.s), branch for branch, after what stops
 * Link's update before it (link.s: wDisabledObjects & $81, a mount). */
unsigned oracles_guest_item_buttons(const OraclesGuestInventoryState *s)
{
    const unsigned both = ORACLES_BUTTON_A | ORACLES_BUTTON_B;
    if (s->disabled_objects & 0x81u) return 0u;
    /* A scroll between two rooms, or the palette changing (a fade into a dark room, a season): Link's update returns
     * before it uses items (linkState01), and a press begun there is no longer new when it ends. */
    if ((s->scroll_mode & 0x0eu) || s->palette_thread_mode) return 0u;
    /* A minecart, and the raft of Ages, carry Link as a mount does (wLinkObjectIndex $d1), but his update goes on to
     * checkUseItems in them (link.s): seeds are shot and the sword swung from a minecart.  On an animal B dismounts and A
     * is the animal's action. */
    const int vehicle = s->companion_id == s->specialobject_minecart || (s->game == ORACLES_GAME_AGES && s->companion_id == s->specialobject_raft);
    if ((s->link_object_index & 0x01u) && !vehicle) return 0u;
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

uint8_t oracles_guest_item_variant(const OraclesGuestInventoryState *s, uint8_t item)
{
    if (!item) return 0xffu;
    if (item == s->item_seed_satchel) return s->satchel_seeds;
    if (item == s->item_seed_shooter) return s->shooter_seeds;
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
    if (cursor >= ORACLES_INVENTORY_SLOTS - 2u) return 0;
    if (slot) *slot = cursor + 2u;
    return 1;
}
