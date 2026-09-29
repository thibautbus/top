#include "object_sprites.h"

#include "guest_struct_offsets.h"

#include <string.h>

#define WINDOW_OAM_Y 0xa0u   /* @drawObject skips a sprite whose OAM y reaches $a0 */
#define WINDOW_OAM_X 0xa8u   /* or whose x reaches $a8 */
#define OAM_ENTRIES 40u
#define PRIORITIES 4u
#define PER_PRIORITY 16u     /* hObjectPriority0Counter and the next: sixteen objects a level */
#define FIRST_ITEM_SLOT 6u   /* FIRST_ITEM_INDEX ($d6): the slots below hold Link, the companion and the parent items */
#define TILESETFLAG_SIDESCROLL 0x20u   /* tilesetFlags.s */
#define TEXTBOXFLAG_ALTPALETTE1 0x04u  /* textboxFlags.s: drawAllSprites then draws Link alone of the special objects */
#define SCROLL_MODE_TRANSITION 0x08u   /* a scrolling transition: no grass nor puddle */
#define TERRAIN_CAMERA_LIMIT 0x97u     /* _drawObjectTerrainEffects returns when hCameraY is $97 or more */
#define GRASS_OTHER_FRAME 0x24u        /* the grass's second frame, for one position in two */
/* The room's pixels go to 240 x 176; an object past its left or top edge
 * wraps its byte to the far end: past these it stands before the edge. */
#define WRAP_X 248
#define WRAP_Y 208

static size_t rom_offset(unsigned bank, unsigned address)
{
    return (size_t)bank * 0x4000u + ((address | 0x4000u) & 0x7fffu) - 0x4000u;
}

/* A block of raw OAM data (func_0eda): a count, then y and x added to the
 * position, tile and attributes as they are. */
static int raw_block(const uint8_t *rom, size_t rom_size, size_t at, uint8_t base_y, uint8_t base_x, int wide_y, int wide_x,
                     uint8_t effect, OraclesObjectSprite out[], unsigned max)
{
    if (at >= rom_size) return -1;
    const unsigned count = rom[at];
    if (at + 1u + (size_t)count * 4u > rom_size) return -1;
    unsigned n = 0;
    for (unsigned i = 0; i < count && n < max; i++) {
        const uint8_t *s = rom + at + 1u + i * 4u;
        OraclesObjectSprite *o = &out[n++];
        memset(o, 0, sizeof *o);
        o->oam_y = (uint8_t)(base_y + s[0]);
        o->oam_x = (uint8_t)(base_x + s[1]);
        o->y = (int16_t)(wide_y + (int8_t)s[0]);
        o->x = (int16_t)(wide_x + (int8_t)s[1]);
        o->tile = s[2];
        o->attr = s[3];
        o->in_window = 1;
        o->effect = effect;
    }
    return (int)n;
}

/* _drawObjectTerrainEffects, for an object whose visible byte asks for them
 * (bit 6), out of a side-scrolling room and with the camera above $97.  In
 * the air, a shadow one frame in two (the frame counter's parity against the
 * object's page), queued for after the loop.  On the ground, out of a
 * scrolling transition, by the room's tile under its feet (y + 5): grass, one
 * of two frames by position; or a puddle, of the frame's animation, and the
 * object's sprites a pixel lower.  Writes the grass or puddle's sprites and
 * the shadow's; `lower` is 1 for a puddle. */
static int terrain_effects(OraclesGuest *guest, const OraclesGuestTables *t, const uint8_t *object, unsigned slot, uint8_t camera_y,
                           uint8_t base_y, uint8_t base_x, int wide_y, int wide_x, const uint8_t *rom, size_t rom_size,
                           OraclesObjectSprite ground[], unsigned *ground_count, OraclesObjectSprite shadow[], unsigned *shadow_count, int *lower)
{
    *ground_count = *shadow_count = 0;
    *lower = 0;
    if (!(object[ORACLES_OBJ_VISIBLE] & 0x40u) || camera_y >= TERRAIN_CAMERA_LIMIT) return 0;
    if (oracles_guest_read8(guest, t->tileset_flags) & TILESETFLAG_SIDESCROLL) return 0;
    const unsigned bank = t->shadow_animation.bank;   /* func_0eda's bank: all the terrain effects' data */
    if (object[ORACLES_OBJ_ZH] & 0x80u) {
        const uint8_t page = (uint8_t)((ORACLES_OBJECTS_BASE >> 8) + slot);
        if (!((oracles_guest_read8(guest, t->frame_counter) ^ page) & 1u)) return 0;
        const int n = raw_block(rom, rom_size, rom_offset(bank, t->shadow_animation.addr), base_y, base_x, wide_y, wide_x,
                                ORACLES_SPRITE_SHADOW, shadow, ORACLES_OBJECT_SPRITES_MAX);
        if (n < 0) return -1;
        *shadow_count = (unsigned)n;
        return 0;
    }
    if (oracles_guest_read8(guest, t->scroll_mode) == SCROLL_MODE_TRANSITION) return 0;
    const uint8_t *layout = oracles_guest_ptr(guest, t->room_layout, 0x100u);
    if (!layout) return 0;
    const uint8_t yh = object[ORACLES_OBJ_YH], xh = object[ORACLES_OBJ_XH];
    const uint8_t tile = layout[((yh + 5u) & 0xf0u) | (xh >> 4)];
    size_t at;
    /* _drawObjectTerrainEffects: hack-base's Seasons draws no grass on $f9 outside group 0 (the Cane of Somaria's block). */
    const uint8_t grass_last = oracles_guest_read8(guest, t->active_group) ? t->grass_tile_last_other_groups : t->grass_tile_last;
    if (tile >= t->grass_tile_first && tile <= grass_last) {
        unsigned offset = oracles_guest_read8(guest, t->grass_animation_modifier);
        if ((xh ^ yh) & 0x04u) offset += GRASS_OTHER_FRAME;
        at = rom_offset(bank, t->green_grass_animation.addr + (offset & 0xffu));
    } else if (tile >= t->puddle_tile_first && tile <= t->puddle_tile_last) {
        at = rom_offset(bank, oracles_guest_read16(guest, t->puddle_animation_pointer));
        *lower = 1;
    } else return 0;
    const int n = raw_block(rom, rom_size, at, base_y, base_x, wide_y, wide_x, ORACLES_SPRITE_GROUND, ground, ORACLES_OBJECT_SPRITES_MAX);
    if (n < 0) return -1;
    *ground_count = (unsigned)n;
    return 0;
}

static int build(OraclesGuest *guest, const uint8_t *object, unsigned slot, const uint8_t *rom, size_t rom_size,
                 OraclesObjectSprite out[], unsigned max)
{
    if (!object || !object[ORACLES_OBJ_ENABLED] || !(object[ORACLES_OBJ_VISIBLE] & 0x80u)) return 0;
    const OraclesGuestTables *t = oracles_guest_tables(guest);
    const uint8_t camera_y = oracles_guest_read8(guest, t->camera_y), camera_x = oracles_guest_read8(guest, t->camera_x);
    const int yh = object[ORACLES_OBJ_YH] >= WRAP_Y ? (int)object[ORACLES_OBJ_YH] - 256 : (int)object[ORACLES_OBJ_YH];
    const int xh = object[ORACLES_OBJ_XH] >= WRAP_X ? (int)object[ORACLES_OBJ_XH] - 256 : (int)object[ORACLES_OBJ_XH];
    /* _getObjectPositionOnScreen: y less the camera plus 16 (the OAM's own
     * offset), x less the camera; the terrain effects there, then z. */
    const uint8_t base_y = (uint8_t)(object[ORACLES_OBJ_YH] - camera_y + 0x10u);
    const uint8_t base_x = (uint8_t)(object[ORACLES_OBJ_XH] - camera_x);
    const int wide_y = yh - (int)camera_y, wide_x = xh - (int)camera_x - 8;
    OraclesObjectSprite ground[ORACLES_OBJECT_SPRITES_MAX], shadow[ORACLES_OBJECT_SPRITES_MAX];
    unsigned ground_count = 0, shadow_count = 0;
    int lower = 0;
    if (terrain_effects(guest, t, object, slot, camera_y, base_y, base_x, wide_y, wide_x, rom, rom_size,
                        ground, &ground_count, shadow, &shadow_count, &lower) != 0) return -1;
    unsigned n = 0;
    for (unsigned i = 0; i < ground_count && n < max; i++) out[n++] = ground[i];
    const uint8_t z = (uint8_t)(object[ORACLES_OBJ_ZH] + lower);
    /* The data address: its two high bits add to BASE_OAM_DATA_BANK, the
     * rest names the block in $4000-$7fff. */
    const unsigned address = (unsigned)(object[ORACLES_OBJ_OAM_DATA_ADDRESS] | object[ORACLES_OBJ_OAM_DATA_ADDRESS + 1] << 8);
    const size_t at = rom_offset(((address & 0xc000u) >> 14) + t->oam_data_bank, address);
    if (at >= rom_size) return -1;
    const unsigned count = rom[at];
    if (at + 1u + (size_t)count * 4u > rom_size) return -1;
    const uint8_t flags = object[ORACLES_OBJ_OAM_FLAGS], tile_base = object[ORACLES_OBJ_OAM_TILE_INDEX_BASE];
    for (unsigned i = 0; i < count && n < max; i++) {
        const uint8_t *s = rom + at + 1u + i * 4u;
        OraclesObjectSprite *o = &out[n++];
        memset(o, 0, sizeof *o);
        o->oam_y = (uint8_t)(base_y + z + s[0]);
        o->oam_x = (uint8_t)(base_x + s[1]);
        o->y = (int16_t)(wide_y + (int8_t)z + (int8_t)s[0]);
        o->x = (int16_t)(wide_x + (int8_t)s[1]);
        o->tile = (uint8_t)(tile_base + s[2]);
        o->attr = (uint8_t)(flags ^ s[3]);
        o->in_window = o->oam_y < WINDOW_OAM_Y && o->oam_x < WINDOW_OAM_X;
        o->effect = ORACLES_SPRITE_OWN;
    }
    for (unsigned i = 0; i < shadow_count && n < max; i++) out[n++] = shadow[i];
    return (int)n;
}

int oracles_object_sprites_build(OraclesGuest *guest, const uint8_t *object, const uint8_t *rom, size_t rom_size,
                                 OraclesObjectSprite out[], unsigned max)
{
    /* The slot's page, for the shadow's parity: the object stands in the objects' bank. */
    const uint8_t *base = oracles_guest_object(guest, 0, 0);
    const unsigned slot = base && object >= base && object < base + ORACLES_OBJECT_SLOTS * 0x100u ? (unsigned)((object - base) >> 8) : 0u;
    return build(guest, object, slot, rom, rom_size, out, max);
}

int oracles_object_sprites_all(OraclesGuest *guest, const uint8_t *rom, size_t rom_size, unsigned first_entry,
                               OraclesDrawnObject objects[], unsigned max_objects, OraclesObjectSprite out[], unsigned max, unsigned *sprites)
{
    const OraclesGuestTables *t = oracles_guest_tables(guest);
    /* The queue: queueDrawEverything takes the items, the enemies, the parts,
     * the interactions, then drawAllSprites Link, the companion and the parent
     * items (Link alone with the text box's alternative palette); each object
     * goes to the level of its visible byte. */
    const uint8_t last_special = (oracles_guest_read8(guest, t->textbox_flags) & TEXTBOXFLAG_ALTPALETTE1) ? 0u : FIRST_ITEM_SLOT - 1u;
    const struct { uint8_t kind, first, last; } order[] = {
        { 0, FIRST_ITEM_SLOT, 15 }, { 2, 0, 15 }, { 3, 0, 15 }, { 1, 0, 15 }, { 0, 0, last_special },
    };
    /* Ages raises Link by wLinkRaisedFloorOffset for the drawing alone
     * (drawAllSpritesUnconditionally): his yh as drawn. */
    const uint8_t raised = t->link_raised_floor_offset.bank == ORACLES_GUEST_ABSENT ? 0u : oracles_guest_read8(guest, t->link_raised_floor_offset);
    OraclesDrawnObject queued[PRIORITIES][PER_PRIORITY];
    unsigned queued_count[PRIORITIES] = { 0 };
    for (unsigned k = 0; k < sizeof order / sizeof order[0]; k++)
        for (unsigned slot = order[k].first; slot <= order[k].last; slot++) {
            const uint8_t *o = oracles_guest_object(guest, slot, order[k].kind);
            if (!o || !o[ORACLES_OBJ_ENABLED] || !(o[ORACLES_OBJ_VISIBLE] & 0x80u)) continue;
            const unsigned level = o[ORACLES_OBJ_VISIBLE] & 3u;
            if (queued_count[level] >= PER_PRIORITY) continue;
            OraclesDrawnObject *d = &queued[level][queued_count[level]++];
            d->kind = order[k].kind;
            d->slot = (uint8_t)slot;
        }
    OraclesObjectSprite shadows[ORACLES_OBJECT_SPRITES_MAX];
    unsigned shadow_count = 0, written = 0, used = 0, tail = first_entry;
    for (unsigned level = 0; level < PRIORITIES; level++)
        for (unsigned i = 0; i < queued_count[level] && written < max_objects; i++) {
            OraclesDrawnObject d = queued[level][i];
            const uint8_t *object = oracles_guest_object(guest, d.slot, d.kind);
            uint8_t link[ORACLES_OBJECT_SIZE];
            if (object && raised && d.kind == 0 && d.slot == 0) {
                memcpy(link, object, sizeof link);
                link[ORACLES_OBJ_YH] = (uint8_t)(link[ORACLES_OBJ_YH] + raised);
                object = link;
            }
            OraclesObjectSprite built[ORACLES_OBJECT_SPRITES_MAX];
            const int n = build(guest, object, d.slot, rom, rom_size, built, ORACLES_OBJECT_SPRITES_MAX);
            if (n < 0) return -1;
            d.first = (uint8_t)used;
            /* func_0eda writes the grass or puddle whole or not at all; then
             * @drawObject writes the sprites inside the window while the OAM
             * has room. */
            unsigned ground = 0;
            while (ground < (unsigned)n && built[ground].effect == ORACLES_SPRITE_GROUND) ground++;
            const int ground_fits = tail + ground <= OAM_ENTRIES;
            for (unsigned s = 0; s < (unsigned)n && used < max; s++) {
                OraclesObjectSprite sprite = built[s];
                if (sprite.effect == ORACLES_SPRITE_SHADOW) { if (shadow_count < ORACLES_OBJECT_SPRITES_MAX) shadows[shadow_count++] = sprite; continue; }
                if (sprite.effect == ORACLES_SPRITE_GROUND) sprite.written = (uint8_t)ground_fits;
                else sprite.written = (uint8_t)(sprite.in_window && tail < OAM_ENTRIES);
                if (sprite.written) sprite.oam_index = (uint8_t)tail++;
                out[used++] = sprite;
            }
            d.count = (uint8_t)(used - d.first);
            objects[written++] = d;
        }
    /* The shadows queued, after the loop, each block whole or not at all. */
    for (unsigned s = 0; s < shadow_count && used < max; ) {
        unsigned block = 1;   /* shadowAnimation: one sprite */
        const int fits = tail + block <= OAM_ENTRIES;
        for (unsigned b = 0; b < block && s < shadow_count && used < max; b++, s++) {
            OraclesObjectSprite sprite = shadows[s];
            sprite.written = (uint8_t)fits;
            if (fits) sprite.oam_index = (uint8_t)tail++;
            out[used++] = sprite;
        }
    }
    if (sprites) *sprites = used;
    return (int)written;
}
