/* The hotbar of the item hotkeys in the view: what the
 * launcher says of the slots, what the game says of the buttons, and the
 * icons, which are the game's own.
 *
 * The status bar draws the item of B with the first two entries of the OAM
 * and that of A with the next two.  A slot's icon is the one the bar would
 * draw for its item: the guest finds its sprites as the bar's routines do, in
 * treasureDisplayData and the icons' graphics of the player's ROM, at the
 * item's level as the game holds it, and the view keeps their opaque pixels,
 * centred in 16 x 16, from the start of play.  Where the tables cannot say (a
 * modified ROM), the view reads the entries of an item on a button and their
 * tiles in the VRAM at a sound vblank instead.  Each pixel is the object
 * palette and colour it is drawn with, whose colour is read from the live
 * palettes when the icon is shown, so that it fades, flashes and changes with
 * the status bar's own; an item on a button is checked against what the LCD
 * shows. */
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
    /* The bar's reload of its icons says when the variables they follow may have changed: armed with the hotbar, with or
     * without the hotkeys' policy, which arms it too. */
    if (hotbar && v->guest && !v->hotbar_hooked)
        v->hotbar_hooked = oracles_guest_add_hook(v->guest, oracles_guest_tables(v->guest)->load_equipped_item_gfx, ORACLES_EVENT_EQUIPPED_GFX_LOADED, 0) == 0;
}

void oracles_enhanced_view_hotbar_counts(const OraclesEnhancedView *v, unsigned *captured, unsigned *shown)
{
    if (captured) *captured = v->hotbar_icons_captured;
    if (shown) *shown = v->hotbar_icons_shown;
}

unsigned oracles_enhanced_view_hotbar_decoded(const OraclesEnhancedView *v) { return v->hotbar_icons_decoded; }

unsigned oracles_enhanced_view_hotbar_refound(const OraclesEnhancedView *v) { return v->hotbar_icons_refound; }

void oracles_enhanced_view_hotbar_pictures(const OraclesEnhancedView *v, unsigned *checked, unsigned *wrong)
{
    if (checked) *checked = v->hotbar_pictures_checked;
    if (wrong) *wrong = v->hotbar_picture_wrong;
}

void oracles_enhanced_view_hotbar_verify(OraclesEnhancedView *v, int enabled) { v->hotbar_verify = enabled != 0; }

void oracles_enhanced_view_hotbar_proof(const OraclesEnhancedView *v, unsigned *checked, unsigned *wrong, unsigned *outside)
{
    if (checked) *checked = v->hotbar_icons_checked;
    if (wrong) *wrong = v->hotbar_icon_wrong;
    if (outside) *outside = v->hotbar_outside_gutters;
}

unsigned oracles_enhanced_view_hotbar_fade_checked(const OraclesEnhancedView *v) { return v->hotbar_fade_checked; }
int oracles_enhanced_view_hotbar_drawn(const OraclesEnhancedView *v) { return v->hotbar_drawn; }

void ev_hotbar_event(OraclesEnhancedView *v, const OraclesGuestEvent *event)
{
    if (event->type == ORACLES_EVENT_TREASURE) v->hotbar_icons_stale = 1;   /* how an item changes level, and icon (fact 8) */
    /* The status bar reloads the icons' graphics: what its sprites show is not to be believed again until the transfer
     * to the VRAM is done, a few frames later (the hook is armed with the inventory's policy, as the hotbar is shown). */
    if (event->type == ORACLES_EVENT_EQUIPPED_GFX_LOADED) {
        v->hotbar_on_button[0].frames = v->hotbar_on_button[1].frames = 0;
        /* and the variables the icons follow may have changed with it (a seed chosen, the gloves' polarity): found again */
        if (v->hotbar_icons_decoded) v->hotbar_icons_stale = 1;
    }
}

static unsigned variant_index(uint8_t variant) { return variant == 0xffu ? 0u : (unsigned)variant + 1u < HOTBAR_ICON_VARIANTS ? (unsigned)variant + 1u : 0u; }

/* The pixels of one 8 x 8 or 8 x 16 object into the box, colour 0 transparent: its palette and colour, 4 x palette +
 * colour. */
static void draw_object(uint16_t box[16][BOX_W], const uint8_t *entry, int left, unsigned height, const uint8_t *vram0, const uint8_t *vram1)
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
            box[y][x] = (uint16_t)(((attr & 7u) * 4u + index) | ORACLES_HOTBAR_ICON_OPAQUE);
        }
    }
}

/* One 8 x 16 sprite from its 32 bytes of graphics into the box at column `x0`, colour 0 transparent. */
static void draw_sprite(uint16_t box[16][BOX_W], const uint8_t *graphics, uint8_t attr, int x0)
{
    for (unsigned row = 0; row < 16u; row++) {
        const unsigned line = (attr & 0x40u) ? 15u - row : row;
        const uint8_t lo = graphics[line * 2u], hi = graphics[line * 2u + 1u];
        for (unsigned col = 0; col < 8u; col++) {
            const unsigned bit = (attr & 0x20u) ? col : 7u - col;
            const unsigned index = ((lo >> bit) & 1u) | (((hi >> bit) & 1u) << 1);
            const int x = x0 + (int)col;
            if (!index || x < 0 || x >= (int)BOX_W) continue;
            box[row][x] = (uint16_t)(((attr & 7u) * 4u + index) | ORACLES_HOTBAR_ICON_OPAQUE);
        }
    }
}

/* The box's opaque pixels, centred in 16 x 16, kept as the icon of that item and variant, with where its corner stands
 * from the sprites' left edge; 0 when nothing is drawn or it is wider than an icon. */
static int keep_icon(OraclesEnhancedView *v, uint16_t box[16][BOX_W], uint8_t item, uint8_t variant)
{
    unsigned min_x = BOX_W, max_x = 0, min_y = 16, max_y = 0;
    for (unsigned y = 0; y < 16u; y++) for (unsigned x = 0; x < BOX_W; x++) if (box[y][x]) {
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
    }
    if (min_x > max_x || max_x - min_x >= ORACLES_HOTBAR_ICON) return 0;
    if (!v->hotbar_icons) v->hotbar_icons = calloc(ORACLES_GUEST_ITEM_LABELS, sizeof *v->hotbar_icons);
    if (!v->hotbar_icons) return 0;
    uint16_t *icon = v->hotbar_icons[item][variant_index(variant)];
    memset(icon, 0, 256u * sizeof *icon);
    const unsigned dx = (ORACLES_HOTBAR_ICON - (max_x - min_x + 1u)) / 2u, dy = (ORACLES_HOTBAR_ICON - (max_y - min_y + 1u)) / 2u;
    for (unsigned y = min_y; y <= max_y; y++) for (unsigned x = min_x; x <= max_x; x++) icon[(y - min_y + dy) * ORACLES_HOTBAR_ICON + (x - min_x + dx)] = box[y][x];
    v->hotbar_icon_origin[item][variant_index(variant)][0] = (int8_t)((int)min_x - (int)dx);
    v->hotbar_icon_origin[item][variant_index(variant)][1] = (int8_t)((int)min_y - (int)dy);
    v->hotbar_have_icon[item][variant_index(variant)] = 1;
    return 1;
}

/* The icon of an item from the player's ROM, as the status bar would draw it on a button; 1 when one was kept. */
static int find_icon(OraclesEnhancedView *v, uint8_t item, uint8_t variant)
{
    OraclesGuestItemSprites sprites;
    if (item >= ORACLES_GUEST_ITEM_LABELS || v->hotbar_icon_sought[item][variant_index(variant)]) return 0;
    v->hotbar_icon_sought[item][variant_index(variant)] = 1;   /* once until the next change: nothing per frame */
    if (!oracles_guest_item_sprites(v->guest, item, variant, &sprites)) return 0;
    uint16_t box[16][BOX_W];
    memset(box, 0, sizeof box);
    if (sprites.count == 2u) draw_sprite(box, sprites.graphics[1], sprites.attributes[1], 8);
    draw_sprite(box, sprites.graphics[0], sprites.attributes[0], 0);   /* the lower OAM index on top, as the hardware has it */
    if (!keep_icon(v, box, item, variant)) return 0;
    uint8_t *display = v->hotbar_icon_display[item][variant_index(variant)];
    display[0] = sprites.shift; display[1] = sprites.display_mode; display[2] = sprites.display_treasure;
    if (v->hotbar_icon_found_once[item][variant_index(variant)]) v->hotbar_icons_refound++;
    else { v->hotbar_icon_found_once[item][variant_index(variant)] = 1; v->hotbar_icons_decoded++; }
    return 1;
}

/* The icon of the item on `button` (0 B, 1 A) from the status bar's sprites; 1 when one was kept. */
static int capture_icon(OraclesEnhancedView *v, unsigned button, uint8_t item, uint8_t variant)
{
    const uint8_t *oam = oracles_guest_oam(v->guest), *io = oracles_guest_io(v->guest);
    const uint8_t *vram0 = oracles_guest_vram(v->guest, 0), *vram1 = oracles_guest_vram(v->guest, 1);
    if (!oam || !io || !vram0 || !vram1 || !(io[IO_LCDC] & 0x02u)) return 0;
    const unsigned height = (io[IO_LCDC] & 0x04u) ? 16u : 8u;
    const uint8_t *first = oam + button * 8u, *second = first + 4u;
    /* Both entries in the status bar's sixteen lines: a hidden one ($e0), or one the game gave to something else, is no icon. */
    for (unsigned i = 0; i < 2u; i++) { const uint8_t y = first[i * 4u]; if (y < 16u || y >= 32u) return 0; }
    const int left = (int)(first[1] < second[1] ? first[1] : second[1]) - 8;
    uint16_t box[16][BOX_W];
    memset(box, 0, sizeof box);
    draw_object(box, second, left, height, vram0, vram1);
    draw_object(box, first, left, height, vram0, vram1);   /* the lower index on top, as the hardware has it */
    if (!keep_icon(v, box, item, variant)) return 0;   /* nothing drawn, or wider than an icon: not one */
    uint8_t *display = v->hotbar_icon_display[item][variant_index(variant)];
    display[0] = 0; display[1] = 0xffu; display[2] = 0;
    v->hotbar_icons_captured++;
    return 1;
}

static uint32_t screen_colour(void *opaque, uint16_t rgb555);

/* An icon's pixel in the live object palettes: RGB555 with the opaque bit, 0 transparent. */
static uint16_t icon_rgb555(const uint8_t *palettes, uint16_t pixel)
{
    if (!(pixel & ORACLES_HOTBAR_ICON_OPAQUE) || !palettes) return 0;
    const uint8_t *c = palettes + (pixel & 31u) * 2u;
    return (uint16_t)(((c[0] | (c[1] << 8)) & 0x7fffu) | ORACLES_HOTBAR_ICON_OPAQUE);
}

/* A colour of the live palettes as the core's image shows it (the game's fade is in the palettes already). */
static uint32_t live_colour(void *opaque, uint16_t rgb555) { return oracles_core_convert_rgb555(((OraclesEnhancedView *)opaque)->core, rgb555); }

/* The status bar's own pixel on its sixteen lines, and whether it hides a sprite there: the tile's priority bit set and
 * its colour not 0, as the hardware decides it.  The bar writes an item's count over its icon this way. */
static int bar_pixel(OraclesEnhancedView *v, unsigned x, unsigned y, uint16_t *rgb555, int *hides_sprite)
{
    OraclesEnhancedBar bar;
    if (!ev_status_bar(v, &bar)) return 0;
    *rgb555 = oracles_enhanced_bar_pixel(&bar, x, y, hides_sprite);
    return 1;
}

int oracles_enhanced_view_load_band(const OraclesEnhancedView *v, int32_t *area_left, int32_t *area_top)
{
    if (area_left) *area_left = v->load_area_left;
    if (area_top) *area_top = v->load_area_top;
    return v->load_band;
}

void oracles_enhanced_view_load_counts(const OraclesEnhancedView *v, unsigned *band_frames, unsigned *blank_frames)
{
    if (band_frames) *band_frames = v->load_band_frames;
    if (blank_frames) *blank_frames = v->load_blank_frames;
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
    const uint8_t *palettes = oracles_guest_obj_palettes(v->guest);
    if (!oam || !oracles_guest_vram(v->guest, 0) || !oracles_guest_vram(v->guest, 1) || !core || !v->hotbar_icons || !palettes) return 0;
    const uint8_t *first = oam + button * 8u, *second = first + 4u;
    for (unsigned i = 0; i < 2u; i++) { const uint8_t y = first[i * 4u]; if (y < 16u || y >= 32u) return 0; }
    const unsigned vi = variant_index(variant);
    const int left = (int)(first[1] < second[1] ? first[1] : second[1]) - 8;
    const int x0 = left + v->hotbar_icon_origin[item][vi][0], y0 = v->hotbar_icon_origin[item][vi][1];
    const uint16_t *icon = v->hotbar_icons[item][vi];
    unsigned wrong = 0;
    for (unsigned y = 0; y < ORACLES_HOTBAR_ICON; y++)
        for (unsigned x = 0; x < ORACLES_HOTBAR_ICON; x++) {
            const uint16_t colour = icon_rgb555(palettes, icon[y * ORACLES_HOTBAR_ICON + x]);
            const int opaque = (colour & ORACLES_HOTBAR_ICON_OPAQUE) != 0;
            const int sx = x0 + (int)x, sy = y0 + (int)y;
            if (sx < 0 || sx >= (int)ORACLES_ENHANCED_CORE_WIDTH || sy < 0 || sy >= 16) { wrong += (unsigned)opaque; continue; }
            uint16_t bar = 0;
            int hidden = 0;
            if (!bar_pixel(v, (unsigned)sx, (unsigned)sy, &bar, &hidden)) { wrong++; continue; }
            const uint16_t expected = opaque && !hidden ? (uint16_t)(colour & 0x7fffu) : bar;
            if (core[(unsigned)sy * ORACLES_ENHANCED_CORE_WIDTH + (unsigned)sx] != live_colour(v, expected)) wrong++;
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
        memset(v->hotbar_icon_sought, 0, sizeof v->hotbar_icon_sought);
        v->hotbar_on_button[0].frames = v->hotbar_on_button[1].frames = 0;   /* the bar may be about to redraw an item that changed level */
        v->hotbar_icons_stale = 0;
    }
    const int sound = oracles_guest_inventory_refusal(s) == ORACLES_INVENTORY_APPLIED && !s->dont_update_status_bar && !(s->status_bar_needs_refresh & 1u);
    for (unsigned b = 0; b < 2u; b++) {
        const uint8_t item = s->slots[b], variant = oracles_guest_item_variant(s, item);
        if (sound && v->hotbar_on_button[b].item == item && v->hotbar_on_button[b].variant == variant) v->hotbar_on_button[b].frames++;
        else { v->hotbar_on_button[b].item = item; v->hotbar_on_button[b].variant = variant; v->hotbar_on_button[b].frames = 0; v->hotbar_on_button[b].verified = v->hotbar_on_button[b].pictured = 0; }
        if (!item || item >= ORACLES_GUEST_ITEM_LABELS || v->hotbar_on_button[b].frames < SOUND_FRAMES) continue;
        if (!v->hotbar_have_icon[item][variant_index(variant)] && !find_icon(v, item, variant)) capture_icon(v, b, item, variant);
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

/* The status bar's own background: the colour most of its top line has, fade and all, where it stands centred, or in
 * the core's image where it is laid out. */
static uint32_t status_bar_background(const OraclesEnhancedView *v)
{
    const uint32_t *line = v->hud_laid ? oracles_core_pixels(v->core) : v->surface + v->hud_top * v->size.width + oracles_enhanced_hud_x(v->size);
    uint32_t best = line[0];
    unsigned best_count = 0;
    for (unsigned x = 0; x < ORACLES_ENHANCED_CORE_WIDTH; x += 8u) {
        unsigned count = 0;
        for (unsigned k = 0; k < ORACLES_ENHANCED_CORE_WIDTH; k += 8u) count += line[k] == line[x];
        if (count > best_count) { best_count = count; best = line[x]; }
    }
    return best;
}

/* The surface but for the four slots: what drawing them must leave as it was (beside the bar centred, the bar's own
 * columns; laid out, the band's but the slots'). */
static uint64_t hash_outside_slots(const OraclesEnhancedView *v)
{
    uint64_t h = ORACLES_HASH_SEED;
    const uint32_t *surface = v->surface;
    const unsigned width = v->size.width, gutter = oracles_enhanced_hud_x(v->size), core = ORACLES_ENHANCED_CORE_WIDTH, top = v->hud_top;
    const unsigned slots_at = v->hud_place.hotbar_at, slots_end = slots_at + oracles_hotbar_width((OraclesHotbarLook)v->hotbar_look);
    h = oracles_guest_hash((const uint8_t *)surface, (size_t)top * width * sizeof *surface, h);   /* above the status bar: the framed drawn-back surface's border */
    for (unsigned y = top; y < top + ORACLES_ENHANCED_HUD_HEIGHT; y++) {
        if (!v->hud_laid) { h = oracles_guest_hash((const uint8_t *)(surface + y * width + gutter), core * sizeof *surface, h); continue; }
        h = oracles_guest_hash((const uint8_t *)(surface + y * width), slots_at * sizeof *surface, h);
        h = oracles_guest_hash((const uint8_t *)(surface + y * width + slots_end), (width - slots_end) * sizeof *surface, h);
    }
    return oracles_guest_hash((const uint8_t *)(surface + (top + ORACLES_ENHANCED_HUD_HEIGHT) * width),
                              (size_t)(v->size.height - top - ORACLES_ENHANCED_HUD_HEIGHT) * width * sizeof *surface, h);
}

/* The status bar's sixteen lines one colour in the core's image. */
static int bar_blank(const OraclesEnhancedView *v)
{
    const uint32_t *core = oracles_core_pixels(v->core);
    if (!core) return 1;
    for (unsigned i = 1; i < ORACLES_ENHANCED_HUD_HEIGHT * ORACLES_ENHANCED_CORE_WIDTH; i++) if (core[i] != core[0]) return 0;
    return 1;
}

/* The game's look of an item: what the bar draws on a button, from the column of its left sprite, 24 x 16 in RGB555 with
 * the opaque bit: the sprites where the bar puts them (not centred: an item of one sprite stands at the left, the harp
 * eight columns right), then over them the tiles drawTreasureExtraTiles writes beside them, their colour 0 letting the
 * sprites through as the tiles' priority does.  Read live: a count follows the game's at once.  0 without an icon. */
static int item_picture(OraclesEnhancedView *v, const uint8_t *live, uint8_t item, unsigned vi, uint16_t picture[HOTBAR_PICTURE_W * 16])
{
    if (item >= ORACLES_GUEST_ITEM_LABELS || !v->hotbar_icons || !v->hotbar_have_icon[item][vi]) return 0;
    memset(picture, 0, HOTBAR_PICTURE_W * 16u * sizeof *picture);
    const uint8_t *display = v->hotbar_icon_display[item][vi];
    const int ox = (int)display[0] + v->hotbar_icon_origin[item][vi][0], oy = v->hotbar_icon_origin[item][vi][1];
    const uint16_t *icon = v->hotbar_icons[item][vi];
    for (unsigned y = 0; y < ORACLES_HOTBAR_ICON; y++)
        for (unsigned x = 0; x < ORACLES_HOTBAR_ICON; x++) {
            const int px = ox + (int)x, py = oy + (int)y;
            const uint16_t colour = icon_rgb555(live, icon[y * ORACLES_HOTBAR_ICON + x]);
            if (colour && px >= 0 && px < (int)HOTBAR_PICTURE_W && py >= 0 && py < 16) picture[py * HOTBAR_PICTURE_W + (unsigned)px] = colour;
        }
    OraclesEnhancedBar bar;
    if (display[1] == 0xffu || !ev_status_bar(v, &bar)) return 1;
    OraclesGuestItemSprites sprites;
    memset(&sprites, 0, sizeof sprites);
    sprites.count = 1; sprites.display_mode = display[1]; sprites.display_treasure = display[2];
    OraclesGuestBarTile tiles[4];
    const unsigned n = oracles_guest_item_extra_tiles(v->guest, &sprites, tiles);
    for (unsigned k = 0; k < n; k++) {
        if (tiles[k].column < 1u || tiles[k].column > 3u || tiles[k].row > 1u) continue;
        const unsigned x0 = (tiles[k].column - 1u) * 8u, y0 = tiles[k].row * 8u;
        for (unsigned y = 0; y < 8u; y++)
            for (unsigned x = 0; x < 8u; x++) {
                unsigned index = 0;
                const uint16_t colour = oracles_enhanced_bar_tile_pixel(&bar, tiles[k].tile, tiles[k].attribute, x, y, &index);
                if (index) picture[(y0 + y) * HOTBAR_PICTURE_W + x0 + x] = (uint16_t)(colour | ORACLES_HOTBAR_ICON_OPAQUE);
            }
    }
    return 1;
}

/* Once a stay on a button: the item's game look against the core's image where the bar draws it, from the column of its
 * left sprite (the OAM's, the shift taken back): its pixels where opaque, the bar's own elsewhere. */
static void verify_picture(OraclesEnhancedView *v, unsigned button, uint8_t item, uint8_t variant, const uint8_t *live)
{
    const uint8_t *oam = oracles_guest_oam(v->guest);
    const uint32_t *core = oracles_core_pixels(v->core);
    const unsigned vi = variant_index(variant);
    if (!oam || !core || item >= ORACLES_GUEST_ITEM_LABELS || !v->hotbar_have_icon[item][vi] || v->hotbar_icon_display[item][vi][1] == 0xffu) return;
    const uint8_t *first = oam + button * 8u, *second = first + 4u;
    for (unsigned i = 0; i < 2u; i++) { const uint8_t y = first[i * 4u]; if (y < 16u || y >= 32u) return; }
    const int left = (int)(first[1] < second[1] ? first[1] : second[1]) - 8 - (int)v->hotbar_icon_display[item][vi][0];
    static uint16_t picture[HOTBAR_PICTURE_W * 16];
    if (!item_picture(v, live, item, vi, picture)) return;
    unsigned wrong = 0;
    for (unsigned y = 0; y < 16u; y++)
        for (unsigned x = 0; x < HOTBAR_PICTURE_W; x++) {
            const int sx = left + (int)x;
            if (sx < 0 || sx >= (int)ORACLES_ENHANCED_CORE_WIDTH) continue;
            const uint16_t colour = picture[y * HOTBAR_PICTURE_W + x];
            uint16_t bar = 0;
            int hidden = 0;
            if (!bar_pixel(v, (unsigned)sx, y, &bar, &hidden)) { wrong++; continue; }
            const uint16_t expected = (colour & ORACLES_HOTBAR_ICON_OPAQUE) ? (uint16_t)(colour & 0x7fffu) : bar;
            if (core[y * ORACLES_ENHANCED_CORE_WIDTH + (unsigned)sx] != live_colour(v, expected)) wrong++;
        }
    v->hotbar_pictures_checked++;
    if (wrong) v->hotbar_picture_wrong++;
}

void ev_draw_hotbar(OraclesEnhancedView *v)
{
    v->hotbar_drawn = 0;
    if (!v->hotbar_shown || !(v->hud_laid ? v->hud_place.hotbar : oracles_hotbar_fits(v->size.width, ORACLES_ENHANCED_CORE_WIDTH))) return;
    OraclesGuestInventoryState s;
    oracles_guest_inventory_state(v->guest, &s);
    capture_icons(v, &s);
    /* The hotbar belongs to the status bar in play: it is shown whenever the game's bar is on screen, a text, a scroll
     * and a room's load included, its icons in the live palettes, so that they fade and flash with the bar's own; not
     * over the title, the file select, a menu, nor in a cutscene (the view framing its picture, or the game's cutscene
     * index, a room's load apart), nor over the blank between two rooms, the bar one colour.  A key refused shows on
     * its slot alone.  Never with the core framed by the player's choice (F3, as in Faithful), whose loads and scrolls
     * are framed too. */
    const int room_load = v->load_band || (s.game_state == 2u && s.cutscene_index == s.cutscene_loading_room);
    if (v->framed_only || (v->hotbar_framed && !room_load) || (v->observation.cutscene && !room_load) || !ev_status_bar_shown(v) || bar_blank(v)) {
        v->hotbar_icons_shown = 0;
        return;
    }
    const uint8_t *live = oracles_guest_obj_palettes(v->guest);
    static uint16_t icons[ORACLES_HOTBAR_SLOTS][ORACLES_HOTBAR_ICON * ORACLES_HOTBAR_ICON];
    /* In a fade or a flash, each frame, the icons of B and A, the bar's sprites believed, against the bar's own (to
     * check that they follow it). */
    if (v->hotbar_verify && v->fade_effective != 0)
        for (unsigned b = 0; b < 2u; b++) {
            const uint8_t item = s.slots[b], variant = oracles_guest_item_variant(&s, item);
            if (item && item < ORACLES_GUEST_ITEM_LABELS && v->hotbar_icons && v->hotbar_have_icon[item][variant_index(variant)]
                && v->hotbar_on_button[b].item == item && v->hotbar_on_button[b].frames >= SOUND_FRAMES && verify_icon(v, b, item, variant))
                v->hotbar_fade_checked++;
        }
    /* The game's look of the item on B and A, once a stay, against the bar's own block. */
    if (v->hotbar_verify)
        for (unsigned b = 0; b < 2u; b++)
            if (!v->hotbar_on_button[b].pictured && v->hotbar_on_button[b].frames >= SOUND_FRAMES + 4u && v->hotbar_on_button[b].item == s.slots[b]) {
                v->hotbar_on_button[b].pictured = 1;
                verify_picture(v, b, s.slots[b], oracles_guest_item_variant(&s, s.slots[b]), live);
            }
    const OraclesHotbarLook look = v->hud_laid ? (OraclesHotbarLook)v->hotbar_look : oracles_hotbar_gutter_look(v->size.width, ORACLES_ENHANCED_CORE_WIDTH);
    static uint16_t pictures[ORACLES_HOTBAR_SLOTS][HOTBAR_PICTURE_W * 16];
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
        if (slot->pending && (v->frame & 8u)) { view->frame = ORACLES_HOTBAR_FRAME_SOLID; view->waiting = 1; }   /* waiting: the frame blinks, eight frames of sixteen */
        if (v->frame < v->hotbar_refused_until[n]) view->frame = ORACLES_HOTBAR_FRAME_REFUSED;
        const unsigned vi = variant_index(slot->variant);
        if (slot->item < ORACLES_GUEST_ITEM_LABELS && !v->hotbar_have_icon[slot->item][vi]) find_icon(v, slot->item, slot->variant);
        if (slot->item < ORACLES_GUEST_ITEM_LABELS && v->hotbar_icons && (v->hotbar_have_icon[slot->item][vi] || v->hotbar_have_icon[slot->item][0])) {
            const uint16_t *kept = v->hotbar_icons[slot->item][v->hotbar_have_icon[slot->item][vi] ? vi : 0u];
            for (unsigned i = 0; i < ORACLES_HOTBAR_ICON * ORACLES_HOTBAR_ICON; i++) icons[n][i] = icon_rgb555(live, kept[i]);
            view->icon = icons[n];
            if (look == ORACLES_HOTBAR_GAME && item_picture(v, live, slot->item, v->hotbar_have_icon[slot->item][vi] ? vi : 0u, pictures[n])) view->picture = pictures[n];
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
        screen_colour(v, RGB555(25, 5, 5)), screen_colour(v, RGB555(31, 30, 27)), screen_colour(v, RGB555(29, 5, 5)), live_colour, v };
    const uint64_t before = v->hotbar_verify ? hash_outside_slots(v) : 0;
    if (v->hud_laid) oracles_hotbar_draw_at(v->surface + v->hud_top * v->size.width, v->size.width, v->hud_place.hotbar_at, (OraclesHotbarLook)v->hotbar_look, views, &palette);
    else oracles_hotbar_draw(v->surface + v->hud_top * v->size.width, v->size.width, ORACLES_ENHANCED_CORE_WIDTH, v->hud == ORACLES_ENHANCED_HUD_ORIGINAL, views, &palette);
    if (v->hotbar_verify && hash_outside_slots(v) != before) v->hotbar_outside_gutters++;
    v->hotbar_drawn = 1;
}
