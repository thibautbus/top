/* The hotbar of the item hotkeys in the view: what the
 * launcher says of the slots, what the game says of the buttons, and the
 * icons, which are the game's own.
 *
 * An icon is never drawn by the host: the status bar draws the item of B
 * with the first two entries of the OAM and that of A with the next two.
 * At a sound vblank the view reads those entries, their tiles in the VRAM and
 * their object palette, and keeps the opaque pixels, centred in 16 x 16, as
 * the icon of that item and variant.  None of the game's display logic is
 * redone (treasureDisplayData and its level tables stay the game's), so an
 * item that changes level, or a modified ROM, is followed by itself. */
#include "view_internal.h"

#include "hotbar.h"

#include <stdlib.h>
#include <string.h>

#define IO_LCDC 0x40u
/* The same item on the button, the status bar up to date, for that long before its sprites are believed: the slot
 * changes at once, but the bar reloads the icon's graphics at a later turn, and until then its sprites still show the
 * item that was there. */
#define SOUND_FRAMES 12u
#define REFUSED_FRAMES 12u
#define BOX_W 24u                /* the two sprites of an icon, side by side with room to spare */

void oracles_enhanced_view_set_hotbar(OraclesEnhancedView *v, const OraclesEnhancedHotbar *hotbar)
{
    v->hotbar_shown = hotbar != NULL;
    if (hotbar) v->hotbar = *hotbar;
}

void oracles_enhanced_view_hotbar_counts(const OraclesEnhancedView *v, unsigned *captured, unsigned *shown)
{
    if (captured) *captured = v->hotbar_icons_captured;
    if (shown) *shown = v->hotbar_icons_shown;
}

void oracles_enhanced_view_hotbar_verify(OraclesEnhancedView *v, int enabled) { v->hotbar_verify = enabled != 0; }

void oracles_enhanced_view_hotbar_proof(const OraclesEnhancedView *v, unsigned *checked, unsigned *wrong, unsigned *outside)
{
    if (checked) *checked = v->hotbar_icons_checked;
    if (wrong) *wrong = v->hotbar_icon_wrong;
    if (outside) *outside = v->hotbar_outside_gutters;
}

void ev_hotbar_event(OraclesEnhancedView *v, const OraclesGuestEvent *event)
{
    if (event->type == ORACLES_EVENT_TREASURE) v->hotbar_icons_stale = 1;   /* how an item changes level, and icon (fact 8) */
    /* The status bar reloads the icons' graphics: what its sprites show is not to be believed again until the transfer
     * to the VRAM is done, a few frames later (the hook is armed with the inventory's policy, as the hotbar is shown). */
    if (event->type == ORACLES_EVENT_EQUIPPED_GFX_LOADED) v->hotbar_on_button[0].frames = v->hotbar_on_button[1].frames = 0;
}

static unsigned variant_index(uint8_t variant) { return variant == 0xffu ? 0u : (unsigned)variant + 1u < HOTBAR_ICON_VARIANTS ? (unsigned)variant + 1u : 0u; }

/* The pixels of one 8 x 8 or 8 x 16 object into the box, colour 0 transparent. */
static void draw_object(uint16_t box[16][BOX_W], const uint8_t *entry, int left, unsigned height, const uint8_t *vram0, const uint8_t *vram1, const uint8_t *palettes)
{
    const uint8_t attr = entry[3];
    const uint8_t *vram = (attr & 0x08u) ? vram1 : vram0;
    const int x0 = (int)entry[1] - 8 - left, y0 = (int)entry[0] - 16;
    for (unsigned row = 0; row < height; row++) {
        unsigned line = (attr & 0x40u) ? height - 1u - row : row;
        const unsigned tile = height == 16u ? ((entry[2] & 0xfeu) | (line >= 8u)) : entry[2];
        line &= 7u;
        const uint8_t lo = vram[tile * 16u + line * 2u], hi = vram[tile * 16u + line * 2u + 1u];
        for (unsigned col = 0; col < 8u; col++) {
            const unsigned bit = (attr & 0x20u) ? col : 7u - col;
            const unsigned index = ((lo >> bit) & 1u) | (((hi >> bit) & 1u) << 1);
            const int x = x0 + (int)col, y = y0 + (int)row;
            if (!index || x < 0 || x >= (int)BOX_W || y < 0 || y >= 16) continue;
            const uint8_t *colour = palettes + ((attr & 7u) * 4u + index) * 2u;
            box[y][x] = (uint16_t)(((colour[0] | (colour[1] << 8)) & 0x7fffu) | ORACLES_HOTBAR_ICON_OPAQUE);
        }
    }
}

/* The icon of the item on `button` (0 B, 1 A) from the status bar's sprites; 1 when one was kept. */
static int capture_icon(OraclesEnhancedView *v, unsigned button, uint8_t item, uint8_t variant)
{
    const uint8_t *oam = oracles_guest_oam(v->guest), *io = oracles_guest_io(v->guest), *palettes = oracles_guest_obj_palettes(v->guest);
    const uint8_t *vram0 = oracles_guest_vram(v->guest, 0), *vram1 = oracles_guest_vram(v->guest, 1);
    if (!oam || !io || !palettes || !vram0 || !vram1 || !(io[IO_LCDC] & 0x02u)) return 0;
    const unsigned height = (io[IO_LCDC] & 0x04u) ? 16u : 8u;
    const uint8_t *first = oam + button * 8u, *second = first + 4u;
    /* Both entries in the status bar's sixteen lines: a hidden one ($e0), or one the game gave to something else, is no icon. */
    for (unsigned i = 0; i < 2u; i++) { const uint8_t y = first[i * 4u]; if (y < 16u || y >= 32u) return 0; }
    const int left = (int)(first[1] < second[1] ? first[1] : second[1]) - 8;
    uint16_t box[16][BOX_W];
    memset(box, 0, sizeof box);
    draw_object(box, second, left, height, vram0, vram1, palettes);
    draw_object(box, first, left, height, vram0, vram1, palettes);   /* the lower index on top, as the hardware has it */
    unsigned min_x = BOX_W, max_x = 0, min_y = 16, max_y = 0;
    for (unsigned y = 0; y < 16u; y++) for (unsigned x = 0; x < BOX_W; x++) if (box[y][x]) {
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
    }
    if (min_x > max_x || max_x - min_x >= ORACLES_HOTBAR_ICON) return 0;   /* nothing drawn, or wider than an icon: not one */
    if (!v->hotbar_icons) v->hotbar_icons = calloc(ORACLES_GUEST_ITEM_LABELS, sizeof *v->hotbar_icons);
    if (!v->hotbar_icons) return 0;
    uint16_t *icon = v->hotbar_icons[item][variant_index(variant)];
    memset(icon, 0, 256u * sizeof *icon);
    const unsigned dx = (ORACLES_HOTBAR_ICON - (max_x - min_x + 1u)) / 2u, dy = (ORACLES_HOTBAR_ICON - (max_y - min_y + 1u)) / 2u;
    for (unsigned y = min_y; y <= max_y; y++) for (unsigned x = min_x; x <= max_x; x++) icon[(y - min_y + dy) * ORACLES_HOTBAR_ICON + (x - min_x + dx)] = box[y][x];
    v->hotbar_icon_origin[item][variant_index(variant)][0] = (int8_t)((int)min_x - (int)dx);
    v->hotbar_icon_origin[item][variant_index(variant)][1] = (int8_t)((int)min_y - (int)dy);
    if (!v->hotbar_have_icon[item][variant_index(variant)]) v->hotbar_icons_captured++;
    v->hotbar_have_icon[item][variant_index(variant)] = 1;
    return 1;
}

static uint32_t screen_colour(void *opaque, uint16_t rgb555);

/* The status bar's own pixel on its sixteen lines, from the maps the game keeps of its bar and the LCDC it gives those
 * lines, and whether it hides a sprite there: the tile's priority bit set and its colour not 0, as the hardware
 * decides it.  The bar writes an item's count over its icon this way. */
static int bar_pixel(OraclesEnhancedView *v, unsigned x, unsigned y, uint16_t *rgb555, int *hides_sprite)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const uint8_t *tiles = oracles_guest_ptr(v->guest, t->status_bar_tile_map, 0x40u), *attrs = oracles_guest_ptr(v->guest, t->status_bar_attribute_map, 0x40u);
    const uint8_t *regs = oracles_guest_ptr(v->guest, t->gfx_regs1, 1u), *palettes = oracles_guest_bg_palettes(v->guest);
    if (!tiles || !attrs || !regs || !palettes) return 0;
    const unsigned cell = (y / 8u) * 32u + x / 8u;
    const uint8_t attr = attrs[cell];
    const unsigned tile = (regs[0] & 0x10u) ? tiles[cell] : 256u + (uint8_t)(tiles[cell] + 128u) - 128u;
    const unsigned line = (attr & 0x40u) ? 7u - y % 8u : y % 8u, bit = (attr & 0x20u) ? x % 8u : 7u - x % 8u;
    const uint8_t *data = oracles_guest_vram(v->guest, (attr & 0x08u) ? 1 : 0) + tile * 16u + line * 2u;
    const unsigned index = ((data[0] >> bit) & 1u) | (((data[1] >> bit) & 1u) << 1);
    const uint8_t *colour = palettes + ((attr & 7u) * 4u + index) * 2u;
    *rgb555 = (uint16_t)((colour[0] | (colour[1] << 8)) & 0x7fffu);
    *hides_sprite = (regs[0] & 0x01u) && (attr & 0x80u) && index;
    return 1;
}

/* The icon kept for the item on `button` against the core's own image, both ways, over the whole box of the icon where
 * the OAM puts it: a pixel of the icon must be the colour the LCD shows there, unless the bar hides the sprite (the
 * hotbar keeps the whole icon, without the count the bar writes over it); and where the icon has no pixel the LCD must
 * show the bar's own, so that a sprite the decoding lost is seen.  The OAM says where, the core's image says what: the
 * decoding is judged by what the hardware made of the sprites, not by itself. */
static int verify_icon(OraclesEnhancedView *v, unsigned button, uint8_t item, uint8_t variant)
{
    const uint8_t *oam = oracles_guest_oam(v->guest);
    const uint32_t *core = oracles_core_pixels(v->core);
    if (!oam || !oracles_guest_vram(v->guest, 0) || !oracles_guest_vram(v->guest, 1) || !core || !v->hotbar_icons || v->fade_effective != 0) return 0;
    const uint8_t *first = oam + button * 8u, *second = first + 4u;
    for (unsigned i = 0; i < 2u; i++) { const uint8_t y = first[i * 4u]; if (y < 16u || y >= 32u) return 0; }
    const unsigned vi = variant_index(variant);
    const int left = (int)(first[1] < second[1] ? first[1] : second[1]) - 8;
    const int x0 = left + v->hotbar_icon_origin[item][vi][0], y0 = v->hotbar_icon_origin[item][vi][1];
    const uint16_t *icon = v->hotbar_icons[item][vi];
    unsigned wrong = 0;
    for (unsigned y = 0; y < ORACLES_HOTBAR_ICON; y++)
        for (unsigned x = 0; x < ORACLES_HOTBAR_ICON; x++) {
            const uint16_t colour = icon[y * ORACLES_HOTBAR_ICON + x];
            const int opaque = (colour & ORACLES_HOTBAR_ICON_OPAQUE) != 0;
            const int sx = x0 + (int)x, sy = y0 + (int)y;
            if (sx < 0 || sx >= (int)ORACLES_ENHANCED_CORE_WIDTH || sy < 0 || sy >= 16) { wrong += (unsigned)opaque; continue; }
            uint16_t bar = 0;
            int hidden = 0;
            if (!bar_pixel(v, (unsigned)sx, (unsigned)sy, &bar, &hidden)) { wrong++; continue; }
            const uint16_t expected = opaque && !hidden ? (uint16_t)(colour & 0x7fffu) : bar;
            if (core[(unsigned)sy * ORACLES_ENHANCED_CORE_WIDTH + (unsigned)sx] != screen_colour(v, expected)) wrong++;
        }
    v->hotbar_icons_checked++;
    if (wrong) v->hotbar_icon_wrong++;
    return 1;
}

/* A sound vblank for the status bar's sprites: normal play with the bar up to date, and the same item and variant on
 * the button for a little while. */
static void capture_icons(OraclesEnhancedView *v, const OraclesGuestInventoryState *s)
{
    if (v->hotbar_icons_stale) {
        memset(v->hotbar_have_icon, 0, sizeof v->hotbar_have_icon);
        v->hotbar_on_button[0].frames = v->hotbar_on_button[1].frames = 0;   /* the bar may be about to redraw an item that changed level */
        v->hotbar_icons_stale = 0;
    }
    const int sound = oracles_guest_inventory_refusal(s) == ORACLES_INVENTORY_APPLIED && !s->dont_update_status_bar && !(s->status_bar_needs_refresh & 1u);
    for (unsigned b = 0; b < 2u; b++) {
        const uint8_t item = s->slots[b], variant = oracles_guest_item_variant(s, item);
        if (sound && v->hotbar_on_button[b].item == item && v->hotbar_on_button[b].variant == variant) v->hotbar_on_button[b].frames++;
        else { v->hotbar_on_button[b].item = item; v->hotbar_on_button[b].variant = variant; v->hotbar_on_button[b].frames = 0; v->hotbar_on_button[b].verified = 0; }
        if (!item || item >= ORACLES_GUEST_ITEM_LABELS || v->hotbar_on_button[b].frames < SOUND_FRAMES) continue;
        if (!v->hotbar_have_icon[item][variant_index(variant)]) capture_icon(v, b, item, variant);
        /* once a stay on the button, a few frames after the capture, or at the first frame after that where it can be
         * judged (no fade): the icon kept against what the LCD shows */
        else if (v->hotbar_verify && !v->hotbar_on_button[b].verified && v->hotbar_on_button[b].frames >= SOUND_FRAMES + 4u) v->hotbar_on_button[b].verified = verify_icon(v, b, item, variant);
    }
}

static uint32_t screen_colour(void *opaque, uint16_t rgb555)
{
    const OraclesEnhancedView *v = opaque;
    unsigned colour = 0;
    for (unsigned shift = 0; shift < 15u; shift += 5u) {
        const int c = (int)((rgb555 >> shift) & 31u) + v->fade_effective;   /* the game's fade, as the gutters take it */
        colour |= (unsigned)(c < 0 ? 0 : c > 31 ? 31 : c) << shift;
    }
    return v->colours[colour & (ORACLES_PPU_COLOURS - 1u)];
}

#define RGB555(r, g, b) ((uint16_t)((r) | ((g) << 5) | ((b) << 10)))

/* The status bar's own background, where it stands now: the colour most of its top line has, fade and all. */
static uint32_t status_bar_background(const OraclesEnhancedView *v)
{
    const uint32_t *line = v->surface + v->hud_top * v->size.width + oracles_enhanced_hud_x(v->size);
    uint32_t best = line[0];
    unsigned best_count = 0;
    for (unsigned x = 0; x < ORACLES_ENHANCED_CORE_WIDTH; x += 8u) {
        unsigned count = 0;
        for (unsigned k = 0; k < ORACLES_ENHANCED_CORE_WIDTH; k += 8u) count += line[k] == line[x];
        if (count > best_count) { best_count = count; best = line[x]; }
    }
    return best;
}

/* The surface but for the four slots: what drawing them must leave as it was. */
static uint64_t hash_outside_slots(const OraclesEnhancedView *v)
{
    uint64_t h = ORACLES_HASH_SEED;
    const uint32_t *surface = v->surface;
    const unsigned width = v->size.width, gutter = oracles_enhanced_hud_x(v->size), core = ORACLES_ENHANCED_CORE_WIDTH, top = v->hud_top;
    h = oracles_guest_hash((const uint8_t *)surface, (size_t)top * width * sizeof *surface, h);   /* above the status bar: the framed drawn-back surface's border */
    for (unsigned y = top; y < top + ORACLES_ENHANCED_HUD_HEIGHT; y++)
        h = oracles_guest_hash((const uint8_t *)(surface + y * width + gutter), core * sizeof *surface, h);
    return oracles_guest_hash((const uint8_t *)(surface + (top + ORACLES_ENHANCED_HUD_HEIGHT) * width),
                              (size_t)(v->size.height - top - ORACLES_ENHANCED_HUD_HEIGHT) * width * sizeof *surface, h);
}

void ev_draw_hotbar(OraclesEnhancedView *v)
{
    if (!v->hotbar_shown) return;
    OraclesGuestInventoryState s;
    oracles_guest_inventory_state(v->guest, &s);
    capture_icons(v, &s);
    /* The hotbar is shown when a slot's key can do something, and only then: in normal play, with the status bar it
     * belongs to on screen as the game draws it.  Not over the title, the file select, a cutscene, a warp, a menu or a
     * text, where no item can be played, nor during a fade, where four slots on a white flash say nothing (as
     * the opening cutscene's white flashes show).  A scroll between two rooms keeps it: the
     * request only waits for the scroll to end. */
    if (oracles_guest_inventory_refusal(&s) != ORACLES_INVENTORY_APPLIED || v->fade_effective != 0) { v->hotbar_icons_shown = 0; return; }
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    OraclesHotbarSlotView views[ORACLES_HOTBAR_SLOTS];
    char labels[ORACLES_HOTBAR_SLOTS][5];
    v->hotbar_icons_shown = 0;
    for (unsigned n = 0; n < ORACLES_HOTBAR_SLOTS; n++) {
        const OraclesEnhancedHotbarSlot *slot = &v->hotbar.slots[n];
        OraclesHotbarSlotView *view = &views[n];
        memset(view, 0, sizeof *view);
        view->key = slot->key;
        if (slot->refusals != v->hotbar_refusals_seen[n]) { v->hotbar_refusals_seen[n] = slot->refusals; v->hotbar_refused_until[n] = v->frame + REFUSED_FRAMES; }
        if (!slot->set) { view->frame = ORACLES_HOTBAR_FRAME_DOTTED; continue; }
        int at = -1;
        for (unsigned i = 0; i < ORACLES_INVENTORY_SLOTS && at < 0; i++) if (s.slots[i] == slot->item) at = (int)i;
        view->dimmed = at < 0;
        /* On a button with the slot's variant, or any when the slot names none. */
        const int variant_there = slot->variant == 0xffu || oracles_guest_item_variant(&s, slot->item) == slot->variant;
        if ((at == (int)ORACLES_INVENTORY_SLOT_B || at == (int)ORACLES_INVENTORY_SLOT_A) && variant_there) { view->button = at == (int)ORACLES_INVENTORY_SLOT_B ? 'B' : 'A'; view->frame = ORACLES_HOTBAR_FRAME_SOLID; }
        if (slot->pending && (v->frame & 8u)) view->frame = ORACLES_HOTBAR_FRAME_SOLID;   /* waiting: the frame blinks, eight frames of sixteen */
        if (v->frame < v->hotbar_refused_until[n]) view->frame = ORACLES_HOTBAR_FRAME_REFUSED;
        const unsigned vi = variant_index(slot->variant);
        if (slot->item < ORACLES_GUEST_ITEM_LABELS && v->hotbar_icons && (v->hotbar_have_icon[slot->item][vi] || v->hotbar_have_icon[slot->item][0])) {
            view->icon = v->hotbar_icons[slot->item][v->hotbar_have_icon[slot->item][vi] ? vi : 0u];
            v->hotbar_icons_shown++;
        } else {
            /* No icon yet: the generated label, or the identifier of an item the tables do not know (a modified ROM). */
            static const char hex[] = "0123456789ABCDEF";
            if (slot->item < ORACLES_GUEST_ITEM_LABELS && t->item_labels[slot->item][0]) memcpy(labels[n], t->item_labels[slot->item], 5);
            else { labels[n][0] = hex[slot->item >> 4]; labels[n][1] = hex[slot->item & 15u]; labels[n][2] = 0; }
            view->label = labels[n];
        }
    }
    const OraclesHotbarPalette palette = {
        status_bar_background(v), screen_colour(v, RGB555(3, 3, 3)), screen_colour(v, RGB555(19, 16, 11)),
        screen_colour(v, RGB555(25, 5, 5)), screen_colour(v, RGB555(31, 30, 27)), screen_colour(v, RGB555(29, 5, 5)), screen_colour, v };
    const uint64_t before = v->hotbar_verify ? hash_outside_slots(v) : 0;
    oracles_hotbar_draw(v->surface + v->hud_top * v->size.width, v->size.width, ORACLES_ENHANCED_CORE_WIDTH, views, &palette);
    if (v->hotbar_verify && hash_outside_slots(v) != before) v->hotbar_outside_gutters++;
}
