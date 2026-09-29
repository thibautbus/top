#include "view_internal.h"

typedef struct edge_sprite { uint8_t y, x, tile, attr; } edge_sprite;

/* Link's sprite is two 8x16 columns, one OAM entry each, on the same line,
 * 8 px apart, drawn from his own graphics, which the game copies to tiles
 * $70-$73 of VRAM bank 1 (loadLinkAndCompanionAnimationFrame queues them
 * to $8701, the low byte the bank; attribute bit 3): tiles t and t+2
 * (the flipped pose swaps them) walking and swimming across, one tile twice,
 * the second flipped, swimming up or down (his pose's block of sprite data,
 * object_sprites.h, says which).  In the water the game draws him lower, up
 * to 6 px.  Past the edge of the game's screen the game writes no entry for
 * the outer column: during a scrolling transition, when exactly one column
 * stands at Link's place, the other is rebuilt from it, so that the
 * continuous transitions can walk or swim him past the edge
 * without cutting him in two. */
#define LINK_TILES 0x70u               /* his graphics' first tile */
#define OAM_ATTR_VRAM_BANK1 0x08u
/* Whether an OAM entry stands at Link's place: his left column (0) or his
 * right one (1), else -1.  The OAM lags Link's position by the moves of up
 * to two logic frames (the game's own and the policy's), two pixels either
 * way on the routes measured: each column is taken 4 px either side of its
 * place, the two windows meeting half-way. */
static int link_column(const OraclesEnhancedView *v, const edge_sprite *e)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    const uint8_t *link = oracles_guest_object(v->guest, 0, 0);
    if (!link || (e->tile & 0xfcu) != LINK_TILES || !(e->attr & OAM_ATTR_VRAM_BANK1)) return -1;
    const unsigned ax = (unsigned)(link[ORACLES_OBJ_XH] - ob->drawn_camera_x) & 0xffu;
    const unsigned ay = (unsigned)(link[ORACLES_OBJ_YH] - ob->drawn_camera_y + 24) & 0xffu;
    const int dy = (int8_t)(uint8_t)(e->y - ay), dx = (int8_t)(uint8_t)(e->x - ax);
    if (dy < -2 || dy > 2 + (int)ORACLES_LINK_SWIM_LOWER) return -1;
    if (dx >= -4 && dx <= 3) return 0;
    if (dx >= 4 && dx <= 11) return 1;
    return -1;
}

/* Whether Link's pose is one tile twice, flipped (swimming up or down). */
static int link_pose_mirrored(OraclesEnhancedView *v)
{
    const uint8_t *link = oracles_guest_object(v->guest, 0, 0);
    if (!link || !v->rom) return 0;
    OraclesObjectSprite sprites[ORACLES_OBJECT_SPRITES_MAX];
    const int built = oracles_object_sprites_build(v->guest, link, v->rom, v->rom_size, sprites, ORACLES_OBJECT_SPRITES_MAX);
    int own[2], n = 0;
    for (int i = 0; i < built && n < 2; i++) if (sprites[i].effect == ORACLES_SPRITE_OWN) own[n++] = i;
    return n == 2 && sprites[own[0]].tile == sprites[own[1]].tile;
}

static unsigned rebuild_link_column(OraclesEnhancedView *v, edge_sprite *entries, unsigned count)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    if (!ob->in_scroll) return count;
    int found = -1, found_left = 0, others = 0;
    for (unsigned i = 0; i < count; i++) {
        const int column = link_column(v, &entries[i]);
        if (column < 0) continue;
        if (found >= 0) others++; else { found = (int)i; found_left = column == 0; }
    }
    if (found < 0 || others) return count;   /* none, or both columns there: nothing to rebuild */
    const edge_sprite *e = &entries[found];
    const int flipped = (e->attr & 0x20u) != 0;
    edge_sprite twin = *e;
    twin.x = (uint8_t)(found_left ? e->x + 8u : e->x - 8u);
    if (link_pose_mirrored(v)) twin.attr = (uint8_t)(e->attr ^ 0x20u);
    else twin.tile = (uint8_t)(found_left ? (flipped ? e->tile - 2 : e->tile + 2) : (flipped ? e->tile + 2 : e->tile - 2));
    entries[count] = twin;
    return count + 1u;
}

enum { ROWS_BELOW_BAR, ROWS_ALL, ROWS_UNDER_BAR };

/* One sprite's pixels outside the core's window, drawn into the world band
 * at their place, with the sprite's palette and flips: the part inside the
 * window is already in the core's image.  `sx`, `sy`: its top left in the
 * game area.  The lines above the game area, under the status bar, are drawn
 * for ROWS_ALL, alone for ROWS_UNDER_BAR, not for ROWS_BELOW_BAR. */
static void draw_sprite_outside(OraclesEnhancedView *v, int sx, int sy, uint8_t tile, uint8_t attr, unsigned height, int rows,
                                int32_t camera_x, int32_t camera_y)
{
    const uint8_t *palettes = oracles_guest_obj_palettes(v->guest);
    const uint8_t *vram0 = oracles_guest_vram(v->guest, 0), *vram1 = oracles_guest_vram(v->guest, 1);
    if (!palettes || !vram0 || !vram1) return;
    const OraclesEnhancedObservation *ob = &v->observation;
    const uint8_t *bank = (attr & 0x08u) ? vram1 : vram0;
    const uint8_t *palette = palettes + (attr & 0x07u) * 8u;
    for (unsigned row = 0; row < height; row++) {
        const int gy = sy + (int)row;
        if (gy < 0 ? rows == ROWS_BELOW_BAR : rows == ROWS_UNDER_BAR) continue;
        const int inside_y = gy >= 0 && gy < (int)ORACLES_ENHANCED_AREA_HEIGHT;
        const unsigned line = (attr & 0x40u) ? height - 1u - row : row;
        const unsigned t = height == 16u ? ((tile & 0xfeu) + (line >> 3)) : tile;
        const uint8_t lo = bank[t * 16u + (line & 7u) * 2u], hi = bank[t * 16u + (line & 7u) * 2u + 1u];
        for (unsigned col = 0; col < 8; col++) {
            const int gx = sx + (int)col;
            if (inside_y && gx >= 0 && gx < (int)ORACLES_ENHANCED_CORE_WIDTH) continue;     /* inside the window: the core drew it */
            const unsigned bit = (attr & 0x20u) ? col : 7u - col;
            const unsigned colour = ((lo >> bit) & 1u) | (((hi >> bit) & 1u) << 1);
            if (!colour) continue;
            /* A ripple moves the room under the sprite, not the sprite: it keeps its place on the screen. */
            const int32_t py = ob->window_top + gy - camera_y;
            const int32_t px = ob->window_left + gx - camera_x;
            if (px < 0 || px >= (int32_t)v->size.width || py < 0 || py >= (int32_t)v->band_height) continue;
            const uint16_t rgb = (uint16_t)(palette[colour * 2u] | (palette[colour * 2u + 1u] << 8));
            *ev_band_pixel(v, (unsigned)px, (unsigned)py) = v->colours[rgb & 0x7fffu];
        }
    }
}

/* The sprites the game placed beyond the edges of its 160x128 game area:
 * the OAM still holds them (the game culls objects only past the room's
 * edge), the LCD does not show them, or hides them under the status bar.
 * They are drawn into the world band at their place, over the neighbours'
 * terrain, in OAM order (the first entry on top), with the object's palette
 * and flips; the part inside the window is already in the core's image.
 * Link walking past an edge during a continuous transition, an
 * enemy half out of the window, keep their whole sprite. */
void ev_overlay_edge_sprites(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y)
{
    const uint8_t *oam = oracles_guest_oam(v->guest), *io = oracles_guest_io(v->guest);
    if (!oam || !io || !(io[IO_LCDC] & 0x02u)) return;
    const unsigned height = (io[IO_LCDC] & 0x04u) ? 16u : 8u;
    const OraclesEnhancedObservation *ob = &v->observation;
    edge_sprite entries[41];
    unsigned count = 0;
    for (unsigned i = 0; i < 40u; i++) {
        /* x 0 is the game's hidden column as much as y 0 or 224: the LCD shows
         * neither, and a column pushed past the left edge lands on 0 whatever
         * its true place. */
        if (oam[i * 4] == 0 || oam[i * 4] >= 160u || oam[i * 4 + 1] == 0) continue;
        if (v->blurb_mask & (1ull << i)) continue;   /* the area blurb: drawn apart (ev_overlay_area_blurb) */
        if (v->large_objects_entries & (1ull << i)) continue;   /* a large room's object: drawn from its model (ev_overlay_large_objects) */
        /* During a transition, the objects of the two rooms beyond the window
         * belong to the capture of the room entered and to the image of the
         * room left (docs/ARCHITECTURE.md, who draws an object during an entry): only Link and what persists across
         * rooms is drawn from the live OAM there. */
        if (v->neighbour_objects && ob->in_transition && ev_live_entry_is_room_object(v, i)) continue;
        entries[count].y = oam[i * 4]; entries[count].x = oam[i * 4 + 1]; entries[count].tile = oam[i * 4 + 2]; entries[count].attr = oam[i * 4 + 3];
        count++;
    }
    if (height == 16u) count = rebuild_link_column(v, entries, count);
    for (int i = (int)count - 1; i >= 0; i--) {
        const uint8_t oy = entries[i].y, ox = entries[i].x;
        /* The x byte wraps: past 208 it stands for the left of the window. */
        const int sx = ox >= 208u ? (int)ox - 8 - 256 : (int)ox - 8;
        const int sy = (int)oy - 16 - (int)ORACLES_ENHANCED_HUD_HEIGHT;          /* game-area row of its top line */
        if (sx >= 0 && sx + 8 <= (int)ORACLES_ENHANCED_CORE_WIDTH && sy >= 0 && sy + (int)height <= (int)ORACLES_ENHANCED_AREA_HEIGHT) continue;   /* wholly inside the window */
        /* The lines under the status bar: the bar's own sprites (the item
         * icons) live there, so only Link's sprite, walking up past the
         * room's top, is drawn into the world above the room. */
        const int link_entry = sy < 0 && link_column(v, &entries[i]) >= 0;
        draw_sprite_outside(v, sx, sy, entries[i].tile, entries[i].attr, height, link_entry ? ROWS_ALL : ROWS_BELOW_BAR, camera_x, camera_y);
    }
}

/* The blurb the game shows at the top of the screen on entering an area
 * (INTERAC_ERA_OR_SEASON_INFO, eraOrSeasonInfo.s: the season in Seasons,
 * the era in Ages).  It is an object of the room: y $0a, x sliding from $b0
 * to $10, then away to the left; with the room's camera at 0 its place in the
 * room is its place on the screen.  In the wide world the room is wherever
 * the camera puts it, and the blurb with it, off the band as often as not.
 * It is taken out of the core's window (its pixels repainted from the
 * software PPU without its sprites) and drawn at the top left of the band,
 * its slide in stretched so that it enters from the band's right edge
 * (deliberately, as the game slides it in from its own screen's right edge). */
void ev_find_area_blurb(OraclesEnhancedView *v)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    const uint8_t *blurb = NULL;
    for (unsigned k = 0; k < 16u && !blurb; k++) {
        const uint8_t *o = oracles_guest_object(v->guest, k, 1);
        if (o && o[ORACLES_OBJ_ENABLED] && o[ORACLES_OBJ_ID] == INTERAC_ERA_OR_SEASON_INFO) blurb = o;
    }
    /* The OAM on screen was built by the frame before, where the slide stood
     * (4 or 6 px further right): its x then is the one the last composition saw. */
    int built_x = !blurb ? 0 : v->blurb_seen ? v->blurb_x : blurb[ORACLES_OBJ_XH];
    /* The slide out (state 3) runs the byte past 0 to the left; the slide in starts at $b0, to the right. */
    if (blurb && blurb[ORACLES_OBJ_STATE] == 3u && built_x >= 128) built_x -= 256;
    v->blurb_seen = blurb != NULL;
    if (blurb) v->blurb_x = blurb[ORACLES_OBJ_XH];
    v->blurb = blurb;
    v->blurb_built_x = built_x;
    v->blurb_mask = 0;
    const uint8_t *oam = oracles_guest_oam(v->guest);
    if (!blurb || !oam) return;
    /* Its sprites: one 8x16 column every 8 px from OAM x = x (screen x - 8),
     * on the OAM line y + 24, less the camera the OAM was built with; a column
     * past the left edge is not written. */
    const unsigned oy = (unsigned)(blurb[ORACLES_OBJ_YH] - ob->drawn_camera_y + 24) & 0xffu;
    const int left = built_x - ob->drawn_camera_x - 8;   /* the OAM x of its first column */
    for (unsigned i = 0; i < 40u; i++) {
        const uint8_t *e = oam + i * 4u;
        const int dx = (int)e[1] - left;
        if (e[0] == oy && e[1] != 0 && dx >= 0 && dx < 32 && dx % 8 == 0) v->blurb_mask |= 1ull << i;
    }
}

void ev_overlay_area_blurb(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    const uint8_t *blurb = v->blurb;
    const uint8_t *oam = oracles_guest_oam(v->guest), *io = oracles_guest_io(v->guest), *palettes = oracles_guest_obj_palettes(v->guest);
    const uint8_t *vram0 = oracles_guest_vram(v->guest, 0), *vram1 = oracles_guest_vram(v->guest, 1);
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const uint8_t *regs = oracles_guest_ptr(v->guest, t->gfx_regs3, 6);
    if (!blurb || !v->blurb_mask || !oam || !io || !palettes || !vram0 || !vram1 || !regs || !(io[IO_LCDC] & 0x02u)) return;
    const int built_x = v->blurb_built_x;
    uint8_t without[160];
    memcpy(without, oam, sizeof without);
    edge_sprite entries[40];
    unsigned count = 0;
    for (unsigned i = 0; i < 40u; i++) {
        if (!(v->blurb_mask & (1ull << i))) continue;
        const uint8_t *e = oam + i * 4u;
        entries[count].y = e[0]; entries[count].x = e[1]; entries[count].tile = e[2]; entries[count].attr = e[3];
        count++;
        without[i * 4u] = 0;   /* y 0: hidden */
    }
    if (!count) return;
    const unsigned height = (io[IO_LCDC] & 0x04u) ? 16u : 8u;
    /* The window's pixels under it: the frame as the LCD drew it, without its sprites. */
    OraclesPpuInput in;
    in.vram = v->hybrid_vram;
    memcpy(v->hybrid_vram, vram0, 0x2000u);
    memcpy(v->hybrid_vram + 0x2000u, vram1, 0x2000u);
    in.oam = without;
    in.bg_palettes = oracles_guest_bg_palettes(v->guest);
    in.obj_palettes = palettes;
    in.colours = v->colours;
    const OraclesPpuRegs r = { (uint8_t)(regs[0] | 0x80u), (uint8_t)(ob->drawn_camera_y + ob->drawn_offset_y - (int)ORACLES_ENHANCED_HUD_HEIGHT),
                               (uint8_t)(ob->drawn_camera_x + ob->drawn_offset_x), regs[3], regs[4] };
    if (!in.bg_palettes) return;
    for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) in.lines[ly] = r;
    oracles_ppu_render(&in, v->popup_frame);
    for (unsigned n = 0; n < count; n++) {
        const int sx = (int)entries[n].x - 8, sy = (int)entries[n].y - 16 - (int)ORACLES_ENHANCED_HUD_HEIGHT;
        for (unsigned row = 0; row < height; row++)
            for (unsigned col = 0; col < 8u; col++) {
                const int gx = sx + (int)col, gy = sy + (int)row;
                if (gx < 0 || gx >= (int)ORACLES_ENHANCED_CORE_WIDTH || gy < 0 || gy >= (int)ORACLES_ENHANCED_AREA_HEIGHT) continue;
                const int32_t px = ob->window_left + gx - camera_x, py = ob->window_top + gy - camera_y;
                if (px < 0 || px >= (int32_t)v->size.width || py < 0 || py >= (int32_t)v->band_height) continue;
                *ev_band_pixel(v, (unsigned)px, (unsigned)py) =
                    v->popup_frame[(ORACLES_ENHANCED_HUD_HEIGHT + (unsigned)gy) * ORACLES_PPU_WIDTH + (unsigned)gx];
            }
    }
    /* Drawn at the top left of the band: the room's x at rest ($10) is the
     * band's; on the way in, beyond it, the distance is stretched from the
     * game's 160 px to the band's width, so that it enters from the band's edge. */
    const int x = built_x;
    const int band_x = x <= 16 ? x : 16 + (x - 16) * (int)v->size.width / (int)ORACLES_ENHANCED_CORE_WIDTH;
    const int shift = band_x - (x - ob->drawn_camera_x);   /* band column minus screen column */
    for (unsigned n = 0; n < count; n++) {
        /* The line its room y gives on the LCD with the room's camera at 0: the top of the game area. */
        const int sx = (int)entries[n].x - 8 + shift, sy = (int)blurb[ORACLES_OBJ_YH] + 24 - 16 - (int)ORACLES_ENHANCED_HUD_HEIGHT;
        const uint8_t tile = entries[n].tile, attr = entries[n].attr;
        const uint8_t *bank = (attr & 0x08u) ? vram1 : vram0;
        const uint8_t *palette = palettes + (attr & 0x07u) * 8u;
        for (unsigned row = 0; row < height; row++) {
            const int py = sy + (int)row;
            if (py < 0 || py >= (int)v->band_height) continue;
            const unsigned line = (attr & 0x40u) ? height - 1u - row : row;
            const unsigned tt = height == 16u ? ((tile & 0xfeu) + (line >> 3)) : tile;
            const uint8_t lo = bank[tt * 16u + (line & 7u) * 2u], hi = bank[tt * 16u + (line & 7u) * 2u + 1u];
            for (unsigned col = 0; col < 8u; col++) {
                const int px = sx + (int)col;
                if (px < 0 || px >= (int)v->size.width) continue;
                const unsigned bit = (attr & 0x20u) ? col : 7u - col;
                const unsigned colour = ((lo >> bit) & 1u) | (((hi >> bit) & 1u) << 1);
                if (!colour) continue;
                const uint16_t rgb = (uint16_t)(palette[colour * 2u] | (palette[colour * 2u + 1u] << 8));
                *ev_band_pixel(v, (unsigned)px, (unsigned)py) = v->colours[rgb & 0x7fffu];
            }
        }
    }
}

#define CURTAIN_SCROLL_MODE 0x02u        /* wScrollMode while the screen opens (screenTransitionState1) */
#define CURTAIN_TRANSITION_STATE 0x01u   /* wScreenTransitionState */
#define CURTAIN_SUBSTATE 0x02u           /* wScreenTransitionState2: the substate that draws a column a frame */
#define CURTAIN_COLUMNS 32u              /* the columns of the game's map, the width of the world band */
#define CURTAIN_SAMPLES 8u               /* lines sampled in a column of the window to read the curtain's colour */

/* ---- the curtain of a warp ------------------------------------------------------------------- */

/* Coming out of a house, out of a cave, back from a portal, the game does not
 * show the room at once: in state 1 of its screen transition it redraws the
 * thirty-two columns of its map outward from the centre of its window, one
 * per frame and one side at a time, and a column not yet drawn shows the
 * blank tile.  Only twenty of those columns are on the core's screen, so the
 * curtain stopped at the window's edges and the band around it stayed drawn
 * in full.  The view paints the columns the game has not
 * drawn yet over the whole band, in the colour the blank tile has on screen:
 * the curtain then opens across the band, and ends with the game's own.
 *
 * `wScreenScrollRow` holds the next column to draw on the left, its
 * counterpart `wScreenScrollDirection` the next one on the right (the two
 * variables of a scrolling transition, used here as the two edges); the
 * columns still to draw run from the right one forward to the left one. */
static int curtain_open_range(OraclesEnhancedView *v, uint8_t scx, int *left, int *right)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    if (oracles_guest_read8(v->guest, t->scroll_mode) != CURTAIN_SCROLL_MODE
        || oracles_guest_read8(v->guest, t->screen_transition_state) != CURTAIN_TRANSITION_STATE
        || oracles_guest_read8(v->guest, t->screen_transition_substate) != CURTAIN_SUBSTATE) return 0;
    /* The game starts from the column of the middle of its window and walks
     * outward, so the columns drawn run from the one left of the middle to
     * the one the right edge has reached; counted in columns rather than
     * read off the map, the two edges never wrap round it. */
    const unsigned next_left = oracles_guest_read8(v->guest, t->screen_scroll_row) & (CURTAIN_COLUMNS - 1u);
    const unsigned next_right = oracles_guest_read8(v->guest, t->screen_scroll_column_right) & (CURTAIN_COLUMNS - 1u);
    const unsigned middle = (((unsigned)scx + ORACLES_ENHANCED_CORE_WIDTH / 2u) >> 3) & (CURTAIN_COLUMNS - 1u);
    const unsigned drawn_left = ((middle - 1u - next_left) & (CURTAIN_COLUMNS - 1u));
    const unsigned drawn_right = ((next_right - middle) & (CURTAIN_COLUMNS - 1u));
    const int middle_px = (int)(((middle * 8u) - scx) & 0xffu);   /* the middle column's left edge, in the window */
    *left = middle_px - (int)drawn_left * 8;
    *right = middle_px + (int)drawn_right * 8;
    return 1;
}

/* The curtain's colour, taken from the core's own image rather than computed:
 * a column of the window the game has not drawn yet is the curtain, and the
 * colour that covers most of it is the blank tile's (a sprite may cross it).
 * Once the window is open the band still has columns to draw: the colour of
 * the frames before is kept for them. */
static uint32_t curtain_colour(OraclesEnhancedView *v, int left, int right, uint8_t scx)
{
    const uint32_t *core = oracles_core_pixels(v->core);
    (void)scx;
    for (unsigned gx = 4; core && gx < ORACLES_ENHANCED_CORE_WIDTH; gx += 8) {
        if ((int)gx >= left && (int)gx < right) continue;
        uint32_t seen[CURTAIN_SAMPLES]; unsigned times[CURTAIN_SAMPLES], kinds = 0;
        for (unsigned i = 0; i < CURTAIN_SAMPLES; i++) {
            const unsigned y = ORACLES_ENHANCED_HUD_HEIGHT + (i * ORACLES_ENHANCED_AREA_HEIGHT) / CURTAIN_SAMPLES;
            const uint32_t pixel = core[y * ORACLES_PPU_WIDTH + gx];
            unsigned k = 0;
            while (k < kinds && seen[k] != pixel) k++;
            if (k == kinds) { seen[kinds] = pixel; times[kinds++] = 1; } else times[k]++;
        }
        unsigned best = 0;
        for (unsigned k = 1; k < kinds; k++) if (times[k] > times[best]) best = k;
        v->curtain_colour = seen[best];
        v->curtain_colour_valid = 1;
        return v->curtain_colour;
    }
    return v->curtain_colour_valid ? v->curtain_colour : 0xff000000u;
}

void ev_overlay_warp_curtain(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    const uint8_t scx = (uint8_t)(ob->drawn_camera_x + ob->drawn_offset_x);
    int left = 0, right = 0;
    if (!curtain_open_range(v, scx, &left, &right)) return;
    v->curtain_frames++;
    const uint32_t colour = curtain_colour(v, left, right, scx);
    for (unsigned x = 0; x < v->size.width; x++) {
        const int window_x = (int)(camera_x + (int32_t)x - ob->window_left);
        const int inside_x = window_x >= 0 && window_x < (int)ORACLES_ENHANCED_CORE_WIDTH;
        if (inside_x && window_x >= left && window_x < right) continue;               /* drawn, and the core's */
        for (unsigned y = 0; y < v->band_height; y++) {
            /* The core draws its own window; the band around it, the rows
             * above and below it that the vertical camera shows included. */
            const int window_y = (int)(camera_y + (int32_t)y - ob->window_top);
            if (inside_x && window_y >= 0 && window_y < (int)ORACLES_ENHANCED_AREA_HEIGHT) continue;
            if (window_x >= left && window_x < right) continue;                       /* already drawn */
            *ev_band_pixel(v, x, y) = colour;
        }
    }
}

/* ---- the objects of a large room beyond the window ---------------------------- */

void ev_large_objects_event(OraclesEnhancedView *v, const OraclesGuestEvent *event)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    if (event->type == ORACLES_EVENT_ANIMATIONS) { v->animation_steps++; return; }
    if (event->type == ORACLES_EVENT_FRAME_DONE) { v->large_objects_first = oracles_guest_read8(v->guest, t->oam_tail) / 4u; v->large_objects_drawn = 0; return; }
    if (event->type == ORACLES_EVENT_OBJECT_DRAW) { v->large_objects_drawn++; return; }
    if (event->type != ORACLES_EVENT_FRAME_DRAWN) return;
    /* drawAllSprites returns at once when wc4b6 says so, the OAM as it was:
     * a drawing rebuilt now would not be the one on screen (a shadow's
     * parity has turned meanwhile).  Nothing is kept, as the harness judges
     * nothing there. */
    if (!v->large_objects_drawn) return;
    const uint8_t *oam = oracles_guest_ptr(v->guest, t->oam, 160u);
    if (!oam || !oracles_guest_read8(v->guest, t->room_is_large) || oracles_guest_read8(v->guest, t->scroll_mode) == ORACLES_SCROLL_MODE_TRANSITION) return;   /* the game then places objects by another routine: nothing kept */
    /* The return of drawAllSprites: the objects stand where the game drew
     * them, the camera with them. */
    OraclesDrawnObject objects[64];
    unsigned count = 0;
    large_object_frame *f = &v->large_objects[v->large_objects_head];
    v->large_objects_head = (v->large_objects_head + 1u) % LARGE_OBJECT_FRAMES;
    f->valid = oracles_object_sprites_all(v->guest, v->rom, v->rom_size, v->large_objects_first, objects, 64u, f->sprites, LARGE_OBJECT_SPRITES, &count) >= 0;
    f->count = f->valid ? count : 0;
    f->entries = 0;
    for (unsigned i = 0; i < f->count; i++) if (f->sprites[i].written) f->entries |= 1ull << f->sprites[i].oam_index;
    f->group = oracles_guest_read8(v->guest, t->active_group);
    f->room = oracles_guest_read8(v->guest, t->active_room);
    memcpy(f->oam, oam, sizeof f->oam);
}

/* In a large room, every object's sprites outside the game's window, from
 * the drawing whose wOam is the OAM on screen, so that they move with the
 * ones the core shows: those the game did not write because they fall
 * beyond its window (an enemy of a wide dungeon room, Farore's book in the
 * Maku Tree), the parts beyond the window of those it wrote, the lines under
 * the status bar, the grass, puddles and shadows, at their true place (the
 * OAM's wrapped bytes would put an effect beyond the window elsewhere), in the
 * game's order, the first on top.  The edge overlay leaves them the entries
 * they account for. */
void ev_overlay_large_objects(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    const uint8_t *oam = oracles_guest_oam(v->guest), *io = oracles_guest_io(v->guest);
    v->large_objects_entries = 0;
    /* A warp's fade and the frames around it are no transition for the
     * objects: the game places them as in play, the drawings kept follow
     * them.  Only the scroll between two rooms places them otherwise, and
     * no drawing of it is kept. */
    if (!ob->large || ob->in_scroll || !oam || !io || !(io[IO_LCDC] & 0x02u) || !v->rom) return;
    const unsigned height = (io[IO_LCDC] & 0x04u) ? 16u : 8u;
    const large_object_frame *f = NULL;
    for (unsigned k = 0; k < LARGE_OBJECT_FRAMES && !f; k++) {
        const unsigned at = (v->large_objects_head + LARGE_OBJECT_FRAMES - 1u - k) % LARGE_OBJECT_FRAMES;
        const large_object_frame *g = &v->large_objects[at];
        /* A drawing of another room is not this one's, whatever its OAM. */
        if (g->valid && g->group == ob->group && g->room == ob->room && memcmp(g->oam, oam, 160u) == 0) f = g;
    }
    if (!f) { v->large_object_frames_unmatched++; return; }
    v->large_objects_entries = f->entries;
    for (int i = (int)f->count - 1; i >= 0; i--) {
        const OraclesObjectSprite *s = &f->sprites[i];
        draw_sprite_outside(v, s->x, s->y - (int)ORACLES_ENHANCED_HUD_HEIGHT, s->tile, s->attr, height, ROWS_ALL, camera_x, camera_y);
        if (!s->written) v->large_object_sprites_drawn++;
    }
}
