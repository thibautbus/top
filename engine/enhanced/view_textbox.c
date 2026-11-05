#include "view_internal.h"

/* ---- the text box of a dialogue, centred in the band ----------------------------------------- */

/* The game writes its text box into its own background map, over the room,
 * at the top or the bottom of its window (saveTilesUnderTextbox, then the
 * rows built in w7TextboxMap); the room's map as the game keeps it to copy
 * into the VRAM (w3VramTiles, w3VramAttributes) is left untouched, and it is
 * from there that the game restores the room when the box closes.  A cell of
 * the displayed map that differs from that image is therefore the box's, on
 * every frame of its opening, its text and its closing, whatever its size.
 *
 * The box was drawn where the game's window stands, to the left or the right
 * of the band as the camera frames Link.  The view now
 * draws the window again with the room's own cells under the box, and the
 * box's cells, as the core drew them, centred across the band at the same
 * height as on the game's screen.  Nothing is written into the game. */

int oracles_enhanced_text_box_holds(int active, unsigned cells, unsigned *trail)
{
    if (active) { *trail = ORACLES_ENHANCED_TEXT_BOX_TRAIL_FRAMES; return 1; }
    if (!*trail || !cells) { *trail = 0; return 0; }   /* the room's cells restored: the box is gone for good */
    (*trail)--;
    return 1;
}

#define MAP_CELLS 32u
#define SMALL_ROOM_COLS 20u
#define SMALL_ROOM_ROWS 16u

/* The cells of the box, in the displayed map, as a bitmap of the room's
 * cells; returns how many.  The game clears wTextIsActive one frame before it
 * restores the room's cells under the box, and the frame on screen still
 * holds the box: that frame keeps the box it had, or it would show at its
 * place in the window for a frame, where the game drew it. */
static unsigned text_box_cells(OraclesEnhancedView *v, uint8_t cells[TEXT_BOX_ROWS][TEXT_BOX_COLS], unsigned *rows, unsigned *cols)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const OraclesEnhancedObservation *ob = &v->observation;
    *rows = ob->large ? TEXT_BOX_ROWS : SMALL_ROOM_ROWS;
    *cols = ob->large ? TEXT_BOX_COLS : SMALL_ROOM_COLS;
    const int active = oracles_guest_read8(v->guest, t->text_is_active) != 0;
    if (!active && !v->text_box_trail) return 0;
    const uint8_t *io = oracles_guest_io(v->guest), *vram0 = oracles_guest_vram(v->guest, 0), *vram1 = oracles_guest_vram(v->guest, 1);
    const uint8_t *tiles = oracles_guest_ptr(v->guest, t->vram_tiles, MAP_CELLS * TEXT_BOX_ROWS);
    const uint8_t *attributes = oracles_guest_ptr(v->guest, t->vram_attributes, MAP_CELLS * TEXT_BOX_ROWS);
    if (!io || !vram0 || !vram1 || !tiles || !attributes) return 0;
    /* The room's first cell sits in the map at the screen offsets (SCX is
     * the camera plus wScreenOffsetX, SCY the camera plus wScreenOffsetY - 16). */
    const unsigned map = (io[IO_LCDC] & 0x08u) ? 0x1c00u : 0x1800u;
    const unsigned row0 = ((unsigned)ob->drawn_offset_y >> 3) & (MAP_CELLS - 1u), col0 = ((unsigned)ob->drawn_offset_x >> 3) & (MAP_CELLS - 1u);
    unsigned count = 0;
    for (unsigned r = 0; r < *rows; r++)
        for (unsigned c = 0; c < *cols; c++) {
            const unsigned shown = map + ((row0 + r) & (MAP_CELLS - 1u)) * MAP_CELLS + ((col0 + c) & (MAP_CELLS - 1u)), kept = r * MAP_CELLS + c;
            cells[r][c] = vram0[shown] != tiles[kept] || vram1[shown] != attributes[kept];
            count += cells[r][c];
        }
    return oracles_enhanced_text_box_holds(active, count, &v->text_box_trail) ? count : 0;
}

/* Where the view places the box: its cells' pixels in the game's window
 * (gx, gy), moved by (dx, dy) so that the box is centred across the part of
 * the band shown, against the edge of it the game drew the box against: dy
 * is 0 but in the drawn-back view's taller band. */
static int text_box_shift(OraclesEnhancedView *v, uint8_t cells[TEXT_BOX_ROWS][TEXT_BOX_COLS], unsigned rows, unsigned cols, int32_t camera_x, int32_t *dx, int32_t *dy,
                          int box[4])
{
    const OraclesEnhancedObservation *ob = &v->observation;
    int left = INT32_MAX, right = INT32_MIN, top = INT32_MAX, bottom = INT32_MIN;
    for (unsigned r = 0; r < rows; r++)
        for (unsigned c = 0; c < cols; c++) {
            if (!cells[r][c]) continue;
            const int gx = (int)c * 8 - ob->drawn_camera_x;
            if (gx + 8 <= 0 || gx >= (int)ORACLES_ENHANCED_CORE_WIDTH) continue;
            if (gx < left) left = gx;
            if (gx + 8 > right) right = gx + 8;
            if ((int)r * 8 - ob->drawn_camera_y < top) top = (int)r * 8 - ob->drawn_camera_y;
            if ((int)r * 8 + 8 - ob->drawn_camera_y > bottom) bottom = (int)r * 8 + 8 - ob->drawn_camera_y;
        }
    if (left > right) return 0;
    const int32_t band_left = ob->window_left + left - camera_x;
    *dx = ((int32_t)v->size.width - (right - left)) / 2 - band_left;
    *dy = (int32_t)(v->band_height - v->shown_height) / 2 + (top >= (int)ORACLES_ENHANCED_AREA_HEIGHT / 2 ? (int32_t)(v->shown_height - ORACLES_ENHANCED_AREA_HEIGHT) : 0);
    box[0] = left; box[1] = top; box[2] = right; box[3] = bottom;
    return 1;
}

/* The host's box drawn into the surface with its top-left pixel at (x, y), its pixels outside the surface's `left` to
 * `right` and `top` to `bottom` left out. */
/* `w` x `h` pixels of `src` (`stride` apart) drawn `scale` times as large at (x, y) in the surface, within its columns
 * `left` to `right` and its lines `top` to `bottom`: a line of blocks drawn once, its copies copied. */
static void put_scaled(OraclesEnhancedView *v, const uint32_t *src, unsigned stride, unsigned w, unsigned h, unsigned scale, int32_t x, int32_t y,
                       int32_t left, int32_t right, int32_t top, int32_t bottom)
{
    const int32_t from = left > x ? left - x : 0, to = right - x < (int32_t)(scale * w) ? right - x : (int32_t)(scale * w);
    if (from >= to) return;
    uint32_t *done = NULL;
    for (unsigned j = 0; j < scale * h; j++) {
        const int32_t sy = y + (int32_t)j;
        if (sy < top || sy >= bottom) { done = NULL; continue; }
        uint32_t *row = v->surface + (size_t)sy * v->size.width + (size_t)x;
        if (done && j % scale) memcpy(row + from, done + from, (size_t)(to - from) * sizeof *row);
        else {
            const uint32_t *line = src + (size_t)(j / scale) * stride;
            for (int32_t i = from; i < to; i++) row[i] = line[(unsigned)i / scale];
        }
        done = row;
    }
}

static void put_host_box(OraclesEnhancedView *v, const OraclesEnhancedHostTextBox *box, unsigned scale, int32_t x, int32_t y, int32_t left, int32_t right,
                         int32_t top, int32_t bottom)
{
    put_scaled(v, box->pixels, box->width, box->width, box->height, scale, x, y, left, right, top, bottom);
    v->wide_shown = 1;
    v->wide_x = x;
    v->wide_y = y;
    v->wide_scale = scale;
    v->wide_box = *box;
}

/* The scale the box is drawn at, `width` pixels wide at the game's: twice as large when Text size is Large and the view
 * holds it so across, else the game's. */
static unsigned box_scale_at(const OraclesEnhancedView *v, unsigned width, unsigned size) { return size == ORACLES_ENHANCED_TEXT_XLARGE && 2u * width <= v->size.width ? 2u : 1u; }
static unsigned box_scale(const OraclesEnhancedView *v, unsigned width) { return box_scale_at(v, width, v->text_size); }

/* Over the framed core: the same, but at its size with the large menus, the presenter enlarging it at every size within
 * the part shown alone. */
static unsigned framed_scale(const OraclesEnhancedView *v, unsigned width) { return v->large_menus ? 1u : box_scale(v, width); }

/* The box drawn at its size at (x, y), its scale chosen for `width`: with Text size Large (or Very large where it does
 * not fit), the presenter shows it again larger, at whole screen pixels; with Normal, noted for a change of the size from
 * the pause, the game not running. */
static void note_enlarged(OraclesEnhancedView *v, int32_t x, int32_t y, unsigned w, unsigned h, int bottom, int from_core, int32_t sx, int32_t sy, int framed,
                          unsigned width)
{
    if (x < 0 || y < 0 || x + (int32_t)w > (int32_t)v->size.width || y + (int32_t)h > (int32_t)v->size.height) return;
    v->enlarged_noted = 1;
    v->enlarged_framed = framed;
    v->enlarged_width = width;
    v->enlarged_rect[0] = x; v->enlarged_rect[1] = y; v->enlarged_rect[2] = (int32_t)w; v->enlarged_rect[3] = (int32_t)h;
    v->enlarged_bottom = bottom;
    v->enlarged_from_core = from_core;
    v->enlarged_src[0] = sx; v->enlarged_src[1] = sy;
}

/* The core's image of `w` x `h` pixels at (sx, sy) drawn `scale` times as large at (x, y) in the surface, within its
 * `top` to `bottom` lines; noted for a check. */
static void put_core_box(OraclesEnhancedView *v, const uint32_t *core, int32_t sx, int32_t sy, unsigned w, unsigned h, unsigned scale, int32_t x, int32_t y,
                         int32_t top, int32_t bottom)
{
    if (sx < 0 || sy < 0 || sx + (int32_t)w > (int32_t)ORACLES_PPU_WIDTH || sy + (int32_t)h > (int32_t)ORACLES_PPU_HEIGHT) return;
    put_scaled(v, core + (size_t)sy * ORACLES_PPU_WIDTH + (size_t)sx, ORACLES_PPU_WIDTH, w, h, scale, x, y, 0, (int32_t)v->size.width, top, bottom);
    v->scaled_shown = 1;
    v->scaled_x = x; v->scaled_y = y;
    v->scaled_src_x = sx; v->scaled_src_y = sy;
    v->scaled_w = w; v->scaled_h = h;
}

/* The box's size at the game's scale, and the part of the band (x, y, w, h) it takes at `scale`. */
static void box_size(const OraclesEnhancedView *v, unsigned *w, unsigned *h)
{
    *w = v->box_wide ? v->box_host.width : (unsigned)(v->box_rect[2] - v->box_rect[0]);
    *h = v->box_wide ? v->box_host.height : (unsigned)(v->box_rect[3] - v->box_rect[1]);
}

static void box_place(const OraclesEnhancedView *v, unsigned scale, int32_t place[4])
{
    unsigned w, h;
    box_size(v, &w, &h);
    const int lower = v->box_rect[1] >= (int)ORACLES_ENHANCED_AREA_HEIGHT / 2;
    place[0] = ((int32_t)v->size.width - (int32_t)(scale * w)) / 2;
    place[1] = v->box_rect[1] + v->box_dy - (lower ? (int32_t)((scale - 1u) * h) : 0);
    place[2] = (int32_t)(scale * w); place[3] = (int32_t)(scale * h);
}

/* The band under the box at the largest size the view draws it at, which holds the others (all centred across, against
 * the same edge), kept before the box is drawn; with the sub-pixel camera's rects held still so far. */
static void keep_under_box(OraclesEnhancedView *v)
{
    int32_t place[4];
    /* A game whose box keeps its size (oracles_enhanced_view_set_text_size) draws it at the game's size only. */
    const unsigned largest = oracles_compat_host_text_box(oracles_guest_profile(v->guest)) ? ORACLES_ENHANCED_TEXT_XLARGE : ORACLES_ENHANCED_TEXT_NORMAL;
    box_place(v, box_scale_at(v, v->box_wide ? v->box_host.width : 8u * ORACLES_TEXT_BOX_GAME_COLUMNS, largest), place);
    int32_t x0 = place[0], y0 = place[1], x1 = place[0] + place[2], y1 = place[1] + place[3];
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int32_t)v->size.width) x1 = (int32_t)v->size.width;
    if (y1 > (int32_t)v->band_height) y1 = (int32_t)v->band_height;
    v->box_kept = x1 > x0 && y1 > y0 && (unsigned)(y1 - y0) <= BOX_UNDER_LINES;
    if (!v->box_kept) return;
    v->box_under_x = x0; v->box_under_y = y0;
    v->box_under_w = (unsigned)(x1 - x0); v->box_under_h = (unsigned)(y1 - y0);
    for (unsigned y = 0; y < v->box_under_h; y++)
        memcpy(v->box_under + y * v->box_under_w, ev_band_pixel(v, (unsigned)x0, (unsigned)y0 + y), v->box_under_w * sizeof *v->box_under);
    v->box_fixed_from = v->sub_fixed_count;
    if (v->sub_fixed_count) memcpy(v->box_fixed_last, v->sub_fixed[v->sub_fixed_count - 1u], sizeof v->box_fixed_last);
}

/* The box centred at the Text size's scale: the game's at its size drawn here, noted for the presenter to show again
 * larger; else left to ev_overlay_text_box_above, above all. */
static void place_box(OraclesEnhancedView *v)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    const uint32_t *core = oracles_core_pixels(v->core);
    if (!core) return;
    const int *rect = v->box_rect;
    const int top = rect[1], lower = top >= (int)ORACLES_ENHANCED_AREA_HEIGHT / 2;
    const int wide = v->box_wide;
    const int32_t dx = v->box_dx, dy = v->box_dy, camera_x = v->box_camera_x;
    const unsigned scale = box_scale(v, wide ? v->box_host.width : 8u * ORACLES_TEXT_BOX_GAME_COLUMNS);
    if (!wide && scale == 1u)
        for (unsigned row = 0; row < v->box_rows; row++)
            for (unsigned col = 0; col < v->box_cols; col++) {
                if (!v->box_cells[row][col]) continue;
                {   /* the box stands on the screen: the sub-pixel camera shows it unmoved */
                    const int gx = (int)(col * 8u) - ob->drawn_camera_x, gy = (int)(row * 8u) - ob->drawn_camera_y;
                    const int x0 = gx < 0 ? 0 : gx, y0 = gy < 0 ? 0 : gy;
                    const int x1 = gx + 8 > (int)ORACLES_ENHANCED_CORE_WIDTH ? (int)ORACLES_ENHANCED_CORE_WIDTH : gx + 8;
                    const int y1 = gy + 8 > (int)ORACLES_ENHANCED_AREA_HEIGHT ? (int)ORACLES_ENHANCED_AREA_HEIGHT : gy + 8;
                    if (x1 > x0 && y1 > y0) ev_subpixel_fixed(v, ob->window_left + x0 - camera_x + dx, y0 + dy, x1 - x0, y1 - y0);
                }
                for (unsigned y = 0; y < 8u; y++)
                    for (unsigned x = 0; x < 8u; x++) {
                        const int gx = (int)(col * 8u + x) - ob->drawn_camera_x, gy = (int)(row * 8u + y) - ob->drawn_camera_y;
                        if (gx < 0 || gx >= (int)ORACLES_ENHANCED_CORE_WIDTH || gy < 0 || gy >= (int)ORACLES_ENHANCED_AREA_HEIGHT) continue;
                        /* the box at the same height on the screen as the game drew it, moved across */
                        const int32_t px = ob->window_left + gx - camera_x + dx, py = gy + dy;
                        if (px < 0 || px >= (int32_t)v->size.width || py < 0 || py >= (int32_t)v->band_height) continue;
                        *ev_band_pixel(v, (unsigned)px, (unsigned)py) = core[(ORACLES_ENHANCED_HUD_HEIGHT + (unsigned)gy) * ORACLES_PPU_WIDTH + (unsigned)gx];
                    }
            }
    /* The box drawn by the host, or the game's twice as large: above all, the black outside the part of the band shown
     * included (ev_overlay_text_box_above). */
    const int32_t hud = (int32_t)ORACLES_ENHANCED_HUD_HEIGHT;
    if (!wide && scale == 1u) {
        note_enlarged(v, ((int32_t)v->size.width - (rect[2] - rect[0])) / 2, hud + top + dy, (unsigned)(rect[2] - rect[0]), (unsigned)(rect[3] - rect[1]), lower, 1,
                      rect[0], hud + top, 0, 8u * ORACLES_TEXT_BOX_GAME_COLUMNS);
        return;
    }
    v->above = wide ? 2 : 1;
    if (wide) v->above_box = v->box_host;
    v->above_scale = scale;
    v->above_lower = lower;
    v->above_source[0] = rect[0];
    v->above_source[1] = hud + top;
    v->above_size[0] = wide ? v->box_host.width : (unsigned)(rect[2] - rect[0]);
    v->above_size[1] = wide ? v->box_host.height : (unsigned)(rect[3] - rect[1]);
    v->above_at[0] = ((int32_t)v->size.width - (int32_t)(scale * v->above_size[0])) / 2;
    v->above_at[1] = hud + top + dy - (lower ? (int32_t)((scale - 1u) * v->above_size[1]) : 0);
}

int oracles_enhanced_view_redraw_text_box(OraclesEnhancedView *v)
{
    if (!v->have_view || !v->box_kept) return 0;
    for (unsigned y = 0; y < v->box_under_h; y++)
        memcpy(ev_band_pixel(v, (unsigned)v->box_under_x, (unsigned)v->box_under_y + y), v->box_under + y * v->box_under_w, v->box_under_w * sizeof *v->box_under);
    v->sub_fixed_count = v->box_fixed_from;
    if (v->sub_fixed_count) memcpy(v->sub_fixed[v->sub_fixed_count - 1u], v->box_fixed_last, sizeof v->box_fixed_last);
    v->wide_shown = v->scaled_shown = v->enlarged_noted = 0;
    v->above = 0;
    const unsigned scaled_world = v->scaled_frames_world, wide_world = v->wide_frames_world;   /* the compositions' counts */
    place_box(v);
    ev_black_outside_shown(v);
    ev_overlay_text_box_above(v);
    v->scaled_frames_world = scaled_world; v->wide_frames_world = wide_world;
    return 1;
}

void ev_overlay_text_box(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y)
{
    v->box_kept = 0;
    uint8_t cells[TEXT_BOX_ROWS][TEXT_BOX_COLS];
    unsigned rows = 0, cols = 0;
    if (!text_box_cells(v, cells, &rows, &cols)) return;
    int32_t dx = 0, dy = 0;
    int rect[4];
    if (!text_box_shift(v, cells, rows, cols, camera_x, &dx, &dy, rect)) return;
    /* The host's wide box, in place of the core's: the core's cells get the room under them, the host's box goes
     * centred across the band at the height the game drew its box at. */
    OraclesEnhancedHostTextBox host;
    const int wide = v->wide_pixels && oracles_enhanced_view_host_text_box(v, v->wide_pixels, v->wide_lines, &host) == 1;
    if (v->wide_pixels && !wide) v->wide_uncovered++;
    const OraclesEnhancedObservation *ob = &v->observation;
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const uint8_t *io = oracles_guest_io(v->guest), *vram0 = oracles_guest_vram(v->guest, 0), *vram1 = oracles_guest_vram(v->guest, 1);
    const uint8_t *tiles = oracles_guest_ptr(v->guest, t->vram_tiles, MAP_CELLS * TEXT_BOX_ROWS);
    const uint8_t *attributes = oracles_guest_ptr(v->guest, t->vram_attributes, MAP_CELLS * TEXT_BOX_ROWS);
    const uint8_t *regs = oracles_guest_ptr(v->guest, t->gfx_regs3, 6);
    const uint32_t *core = oracles_core_pixels(v->core);
    if (!regs || !core) return;
    v->text_box_frames++;
    /* The window again, the room's own cells under the box: the core's
     * registers, its sprites and its palettes, only the map patched. */
    const unsigned map = (io[IO_LCDC] & 0x08u) ? 0x1c00u : 0x1800u;
    const unsigned row0 = ((unsigned)ob->drawn_offset_y >> 3) & (MAP_CELLS - 1u), col0 = ((unsigned)ob->drawn_offset_x >> 3) & (MAP_CELLS - 1u);
    memcpy(v->hybrid_vram, vram0, 0x2000u);
    memcpy(v->hybrid_vram + 0x2000u, vram1, 0x2000u);
    for (unsigned r = 0; r < rows; r++)
        for (unsigned c = 0; c < cols; c++) {
            if (!cells[r][c]) continue;
            const unsigned shown = map + ((row0 + r) & (MAP_CELLS - 1u)) * MAP_CELLS + ((col0 + c) & (MAP_CELLS - 1u));
            v->hybrid_vram[shown] = tiles[r * MAP_CELLS + c];
            v->hybrid_vram[0x2000u + shown] = attributes[r * MAP_CELLS + c];
        }
    OraclesPpuInput in;
    in.vram = v->hybrid_vram;
    in.oam = oracles_guest_oam(v->guest);
    in.bg_palettes = oracles_guest_bg_palettes(v->guest);
    in.obj_palettes = oracles_guest_obj_palettes(v->guest);
    in.colours = v->colours;
    if (!in.oam || !in.bg_palettes || !in.obj_palettes) return;
    const OraclesPpuRegs r = { (uint8_t)(regs[0] | 0x80u), (uint8_t)(ob->drawn_camera_y + ob->drawn_offset_y - (int)ORACLES_ENHANCED_HUD_HEIGHT),
                               (uint8_t)(ob->drawn_camera_x + ob->drawn_offset_x), regs[3], regs[4] };
    for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) in.lines[ly] = r;
    oracles_ppu_render(&in, v->popup_frame);
    /* First the room under the box at its place, then the box centred: the
     * box stays above all, the neighbours' objects included. */
    for (unsigned row = 0; row < rows; row++)
        for (unsigned col = 0; col < cols; col++) {
            if (!cells[row][col]) continue;
            for (unsigned y = 0; y < 8u; y++)
                for (unsigned x = 0; x < 8u; x++) {
                    const int gx = (int)(col * 8u + x) - ob->drawn_camera_x, gy = (int)(row * 8u + y) - ob->drawn_camera_y;
                    if (gx < 0 || gx >= (int)ORACLES_ENHANCED_CORE_WIDTH || gy < 0 || gy >= (int)ORACLES_ENHANCED_AREA_HEIGHT) continue;
                    const int32_t px = ob->window_left + gx - camera_x, py = ob->window_top + gy - camera_y;
                    if (px < 0 || px >= (int32_t)v->size.width || py < 0 || py >= (int32_t)v->band_height) continue;
                    *ev_band_pixel(v, (unsigned)px, (unsigned)py) = v->popup_frame[(ORACLES_ENHANCED_HUD_HEIGHT + (unsigned)gy) * ORACLES_PPU_WIDTH + (unsigned)gx];
                }
        }
    memcpy(v->box_cells, cells, sizeof v->box_cells);
    v->box_rows = rows; v->box_cols = cols;
    v->box_camera_x = camera_x; v->box_dx = dx; v->box_dy = dy;
    memcpy(v->box_rect, rect, sizeof v->box_rect);
    v->box_wide = wide;
    if (wide) v->box_host = host;
    keep_under_box(v);
    place_box(v);
}

void ev_overlay_text_box_above(OraclesEnhancedView *v)
{
    const int kind = v->above;
    v->above = 0;
    if (!kind) return;
    const int32_t hud = (int32_t)ORACLES_ENHANCED_HUD_HEIGHT;
    if (kind == 1) {
        const uint32_t *core = oracles_core_pixels(v->core);
        if (!core) return;
        put_core_box(v, core, v->above_source[0], v->above_source[1], v->above_size[0], v->above_size[1], v->above_scale, v->above_at[0], v->above_at[1],
                     hud, hud + (int32_t)v->band_height);
        ev_subpixel_fixed(v, v->above_at[0], v->above_at[1] - hud, (int32_t)(v->above_scale * v->above_size[0]), (int32_t)(v->above_scale * v->above_size[1]));
        v->scaled_frames_world++;
        return;
    }
    put_host_box(v, &v->above_box, v->above_scale, v->above_at[0], v->above_at[1], 0, (int32_t)v->size.width, hud, hud + (int32_t)v->band_height);
    ev_subpixel_fixed(v, v->above_at[0], v->above_at[1] - hud, (int32_t)(v->above_scale * v->above_size[0]), (int32_t)(v->above_scale * v->above_size[1]));
    if (v->above_scale == 1u) note_enlarged(v, v->above_at[0], v->above_at[1], v->above_box.width, v->above_box.height, v->above_lower, 0, 0, 0, 0, v->above_box.width);
    v->wide_frames_world++;
    if (v->above_scale > 1u) v->scaled_frames_world++;
}

/* The registers each line of the frame just scanned was drawn with: the vblank's writes, then the scan's (a write in
 * mode 0 lands on the next line). */
static void line_registers(OraclesEnhancedView *v, OraclesPpuRegs regs[ORACLES_PPU_HEIGHT])
{
    const uint8_t *io = oracles_guest_io(v->guest);
    OraclesPpuRegs r = { io[IO_LCDC], io[0x42], io[0x43], io[0x4a], io[0x4b] };
    size_t count = 0;
    const OraclesGuestRegWrite *j = oracles_guest_journal(v->guest, &count);
    for (int pass = 0; pass < 2; pass++)
        for (unsigned ly = 0; ly < (pass ? ORACLES_PPU_HEIGHT : 1u); ly++) {
            for (size_t i = 0; i < count; i++) {
                const unsigned at = j[i].stat_mode == 0 ? (unsigned)j[i].ly + 1u : j[i].ly;
                if (pass ? (j[i].ly >= ORACLES_PPU_HEIGHT || at != ly) : j[i].ly < ORACLES_PPU_HEIGHT) continue;
                if (j[i].reg == IO_LCDC) r.lcdc = j[i].value;
                else if (j[i].reg == 0x42) r.scy = j[i].value;
                else if (j[i].reg == 0x43) r.scx = j[i].value;
                else if (j[i].reg == 0x4a) r.wy = j[i].value;
                else if (j[i].reg == 0x4b) r.wx = j[i].value;
            }
            if (pass) regs[ly] = r;
        }
}

/* The game's box on the screen of the frame just scanned: the first line of its first row and the screen's column of its
 * left margin, its map's place (`map`, `row`, `column`); 0 when no line shows it. */
static int box_on_screen(const OraclesPpuRegs regs[ORACLES_PPU_HEIGHT], unsigned map, unsigned row, unsigned column, int32_t *top, int32_t *left)
{
    for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) {
        const OraclesPpuRegs *r = &regs[ly];
        if (!(r->lcdc & 0x80u) || ((r->lcdc & 0x08u) ? 0x1c00u : 0x1800u) != map) continue;
        const unsigned ym = (ly + r->scy) & 0xffu, at = ((ym >> 3) - row) & (MAP_CELLS - 1u);
        if (at >= ORACLES_TEXT_BOX_GAME_ROWS) continue;
        *top = (int32_t)ly - (int32_t)(at * 8u + (ym & 7u));
        *left = (int32_t)((column * 8u - r->scx) & 0xffu);
        if (*left > 0xc0) *left -= 0x100;
        return 1;
    }
    return 0;
}

/* The game's standard box, read without the host's lines: where its variables put it in the map, its margins the box's
 * blank; while a text of the standard box shows, or the frames after it. */
static int standard_box(OraclesEnhancedView *v, unsigned *map, unsigned *row, unsigned *column)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const int active = oracles_guest_read8(v->guest, t->text_is_active) != 0 && oracles_guest_read8(v->guest, t->text_display_mode) <= 1u;
    if (active) v->scaled_trail = ORACLES_ENHANCED_TEXT_BOX_TRAIL_FRAMES;
    else if (v->scaled_trail) v->scaled_trail--;
    else return 0;
    const unsigned at = oracles_guest_read16(v->guest, t->textbox_vram_pos);
    const uint8_t *vram = oracles_guest_vram(v->guest, 0);
    if (at < 0x9800u || at >= 0xa000u || !vram) return 0;
    *map = (at - 0x8000u) & 0x1c00u;
    *row = (at >> 5) & (MAP_CELLS - 1u);
    *column = (oracles_guest_read8(v->guest, t->textbox_column) + 1u) & (MAP_CELLS - 1u);
    for (unsigned r = 0; r < ORACLES_TEXT_BOX_GAME_ROWS; r++)
        for (unsigned side = 0; side < 2u; side++) {
            if (side && r + 1u == ORACLES_TEXT_BOX_GAME_ROWS) continue;   /* the arrow's corner */
            const unsigned c = (*column + (side ? ORACLES_TEXT_BOX_GAME_COLUMNS - 1u : 0u)) & (MAP_CELLS - 1u);
            const unsigned cell = *map + ((*row + r) & (MAP_CELLS - 1u)) * MAP_CELLS + c;
            if (vram[cell] != ORACLES_TEXT_BOX_BLANK_TILE || vram[0x2000u + cell] != ORACLES_TEXT_BOX_BLANK_ATTRIBUTE) return 0;
        }
    return 1;
}

void ev_overlay_text_box_framed(OraclesEnhancedView *v)
{
    const uint32_t *core = oracles_core_pixels(v->core);
    const int32_t frame_x = (int32_t)oracles_enhanced_hud_x(v->size), frame_y = (int32_t)(v->size.height - ORACLES_PPU_HEIGHT) / 2;
    const unsigned game_width = 8u * ORACLES_TEXT_BOX_GAME_COLUMNS, game_height = 8u * ORACLES_TEXT_BOX_GAME_ROWS;
    if (!v->wide_pixels) {
        /* Text size Large, the game's box: its pixels from the core's image, twice as large, centred on it, against the
         * edge of the screen it stands nearer. */
        unsigned map = 0, row = 0, column = 0;
        if (!core || !standard_box(v, &map, &row, &column)) return;
        OraclesPpuRegs regs[ORACLES_PPU_HEIGHT];
        line_registers(v, regs);
        int32_t top = 0, left = 0;
        if (!box_on_screen(regs, map, row, column, &top, &left)) return;
        const int lower = top + (int32_t)game_height / 2 >= (int32_t)ORACLES_PPU_HEIGHT / 2;
        if (framed_scale(v, game_width) == 1u) {   /* Large: the core's own box, which the presenter enlarges */
            note_enlarged(v, frame_x + left, frame_y + top, game_width, game_height, lower, 1, left, top, 1, game_width);
            return;
        }
        put_core_box(v, core, left, top, game_width, game_height, 2u, frame_x + left + (int32_t)game_width / 2 - (int32_t)game_width,
                     frame_y + top - (lower ? (int32_t)game_height : 0), 0, (int32_t)v->size.height);
        v->scaled_frames_framed++;
        return;
    }
    OraclesEnhancedHostTextBox host;
    const int drawn = oracles_enhanced_view_host_text_box(v, v->wide_pixels, v->wide_lines, &host);
    if (drawn < 0) v->wide_uncovered++;
    if (drawn != 1 || !core) return;
    /* Where the game's box stands on the screen, line by line, and the host's box centred on it: each of its rows at
     * the screen's line that shows the game's, the window left over it where it covers the frame.  Large: the box
     * twice as large, centred on the game's, against the edge of the screen it stands nearer. */
    OraclesPpuRegs regs[ORACLES_PPU_HEIGHT];
    line_registers(v, regs);
    const unsigned scale = framed_scale(v, host.width);
    if (scale > 1u) {
        int32_t top = 0, left = 0;
        if (!box_on_screen(regs, host.map, host.row, host.column, &top, &left)) return;
        const int lower = top + (int32_t)game_height / 2 >= (int32_t)ORACLES_PPU_HEIGHT / 2;
        put_host_box(v, &host, scale, frame_x + left + (int32_t)game_width / 2 - (int32_t)host.width, frame_y + top - (lower ? (int32_t)host.height : 0),
                     0, (int32_t)v->size.width, 0, (int32_t)v->size.height);
        v->wide_frames_framed++;
        v->scaled_frames_framed++;
        return;
    }
    const int32_t margin = ((int32_t)host.width - (int32_t)game_width) / 2;
    int any = 0;
    int32_t first_x = 0, first_y = 0;
    for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) {
        const OraclesPpuRegs *r = &regs[ly];
        if (!(r->lcdc & 0x80u) || ((r->lcdc & 0x08u) ? 0x1c00u : 0x1800u) != host.map) continue;
        const unsigned ym = (ly + r->scy) & 0xffu, row = ((ym >> 3) - host.row) & (MAP_CELLS - 1u);
        if (row >= ORACLES_TEXT_BOX_GAME_ROWS) continue;
        int32_t left = (int32_t)((host.column * 8u - r->scx) & 0xffu);
        if (left > 0xc0) left -= 0x100;
        const int32_t x = frame_x + left - margin, by = (int32_t)(row * 8u + (ym & 7u)), sy = frame_y + (int32_t)ly;
        if (!any) { any = 1; first_x = x; first_y = sy - by; }
        const int window = (r->lcdc & 0x20u) && r->wy <= ly && r->wx <= 166u;
        for (unsigned bx = 0; bx < host.width; bx++) {
            const int32_t sx = x + (int32_t)bx, fx = sx - frame_x;
            if (sx < 0 || sx >= (int32_t)v->size.width) continue;
            if (window && fx >= 0 && fx < (int32_t)ORACLES_ENHANCED_CORE_WIDTH && fx + 7 >= (int32_t)r->wx) continue;
            v->surface[(size_t)sy * v->size.width + (size_t)sx] = host.pixels[(unsigned)by * host.width + bx];
        }
    }
    if (!any) return;
    v->wide_shown = 1;
    v->wide_x = first_x;
    v->wide_y = first_y;
    v->wide_scale = 1;
    note_enlarged(v, first_x, first_y, host.width, host.height, first_y - frame_y + (int32_t)game_height / 2 >= (int32_t)ORACLES_PPU_HEIGHT / 2, 0, 0, 0, 1, host.width);
    v->wide_box = host;
    v->wide_frames_framed++;
}

void oracles_enhanced_view_set_text_size(OraclesEnhancedView *v, unsigned size)
{
    /* The fan games keep the game's box at its size, as they keep it standard: the host's box is not qualified there. */
    const int qualified = oracles_compat_host_text_box(oracles_guest_profile(v->guest));
    v->text_size = qualified && size <= ORACLES_ENHANCED_TEXT_XLARGE ? size : ORACLES_ENHANCED_TEXT_NORMAL;
}

unsigned oracles_enhanced_view_text_size(const OraclesEnhancedView *v) { return v->text_size; }

void oracles_enhanced_view_set_large_menus(OraclesEnhancedView *v, int large) { v->large_menus = large != 0; }

/* The box the last composition noted, which the presenter shows again larger at the size now: Large, or Very large where
 * the view would not draw it twice as large itself; a size the pause changed included. */
static int enlarged_now(const OraclesEnhancedView *v)
{
    return v->enlarged_noted && v->text_size != ORACLES_ENHANCED_TEXT_NORMAL
        && (v->enlarged_framed ? framed_scale(v, v->enlarged_width) : box_scale(v, v->enlarged_width)) == 1u;
}

int oracles_enhanced_view_text_box_enlarged(void *opaque, uint32_t rect[4], int *bottom, uint32_t *reserve_w, unsigned *size)
{
    const OraclesEnhancedView *v = opaque;
    const unsigned width = v->wide_pixels ? v->wide_pixels + 16u : 8u * ORACLES_TEXT_BOX_GAME_COLUMNS;
    const int shown = enlarged_now(v);
    *reserve_w = v->text_size != ORACLES_ENHANCED_TEXT_NORMAL && framed_scale(v, width) == 1u ? width : 0u;
    *size = (shown ? v->enlarged_framed : 1) && v->large_menus ? v->text_size : ORACLES_ENHANCED_TEXT_LARGE;
    if (!shown) return 0;
    for (unsigned i = 0; i < 4u; i++) rect[i] = (uint32_t)v->enlarged_rect[i];
    *bottom = v->enlarged_bottom;
    return 1;
}

int oracles_enhanced_view_text_box_enlarged_source(const OraclesEnhancedView *v, int32_t src[2], int *from_core)
{
    if (!enlarged_now(v)) return 0;
    src[0] = v->enlarged_src[0];
    src[1] = v->enlarged_src[1];
    *from_core = v->enlarged_from_core;
    return 1;
}

int oracles_enhanced_view_text_box_scaled(const OraclesEnhancedView *v, int32_t dst[2], int32_t src[2], unsigned size[2])
{
    if (!v->scaled_shown) return 0;
    dst[0] = v->scaled_x; dst[1] = v->scaled_y;
    src[0] = v->scaled_src_x; src[1] = v->scaled_src_y;
    size[0] = v->scaled_w; size[1] = v->scaled_h;
    return 1;
}

unsigned oracles_enhanced_view_text_box_shown_scale(const OraclesEnhancedView *v) { return v->wide_shown ? v->wide_scale : 0u; }

void oracles_enhanced_view_text_scale_counts(const OraclesEnhancedView *v, unsigned *world, unsigned *framed)
{
    *world = v->scaled_frames_world;
    *framed = v->scaled_frames_framed;
}

void oracles_enhanced_view_set_text_box(OraclesEnhancedView *v, unsigned pixels, unsigned lines)
{
    const int host = pixels > ORACLES_TEXT_BOX_GAME_PIXELS && pixels <= ORACLES_GUEST_TEXT_BOX_COLUMNS_MAX * 8u && !(pixels % 8u)
                  && oracles_guest_text_box_pixels(v->guest) == pixels;
    v->wide_pixels = host ? pixels : 0u;
    v->wide_lines = lines < ORACLES_TEXT_BOX_GAME_LINES ? ORACLES_TEXT_BOX_GAME_LINES : lines > ORACLES_TEXT_BOX_LINES_MAX ? ORACLES_TEXT_BOX_LINES_MAX : lines;
}

int oracles_enhanced_view_text_box_shown(const OraclesEnhancedView *v, OraclesEnhancedHostTextBox *box, int32_t *x, int32_t *y)
{
    if (!v->wide_shown) return 0;
    if (box) *box = v->wide_box;
    if (x) *x = v->wide_x;
    if (y) *y = v->wide_y;
    return 1;
}

const struct OraclesTextBoxModel *oracles_enhanced_view_text_box_model(const OraclesEnhancedView *v, const struct OraclesTextBoxSource **source)
{
    if (source) *source = &v->host_text_box_source;
    return &v->host_text_box_model;
}

void oracles_enhanced_view_text_box_counts(const OraclesEnhancedView *v, unsigned *world, unsigned *framed, unsigned *uncovered)
{
    *world = v->wide_frames_world;
    *framed = v->wide_frames_framed;
    *uncovered = v->wide_uncovered;
}

/* ---- the text box drawn by the host ---------------------------------------------------------- */

int oracles_enhanced_view_host_text_box(OraclesEnhancedView *v, unsigned pixels, unsigned lines, OraclesEnhancedHostTextBox *box)
{
    memset(box, 0, sizeof *box);
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    if (!oracles_guest_text_box_armed(v->guest)) return 0;
    if (oracles_guest_read8(v->guest, t->text_is_active)) v->host_text_box_trail = ORACLES_ENHANCED_TEXT_BOX_TRAIL_FRAMES;
    else if (v->host_text_box_trail) v->host_text_box_trail--;
    else return 0;
    if (oracles_guest_read8(v->guest, t->text_display_mode) > 1u) return 0;   /* the inventory's line, in its menu */
    const unsigned at = oracles_guest_read16(v->guest, t->textbox_vram_pos);
    const uint8_t *io = oracles_guest_io(v->guest), *vram = oracles_guest_vram(v->guest, 0), *palettes = oracles_guest_bg_palettes(v->guest);
    if (at < 0x9800u || at >= 0xa000u || !io || !vram || !palettes) return 0;
    box->map = (at - 0x8000u) & 0x1c00u;
    box->row = (at >> 5) & (MAP_CELLS - 1u);
    box->column = (oracles_guest_read8(v->guest, t->textbox_column) + 1u) & (MAP_CELLS - 1u);
    uint8_t tiles[ORACLES_TEXT_BOX_GAME_ROWS * ORACLES_TEXT_BOX_GAME_COLUMNS], attributes[ORACLES_TEXT_BOX_GAME_ROWS * ORACLES_TEXT_BOX_GAME_COLUMNS];
    for (unsigned r = 0; r < ORACLES_TEXT_BOX_GAME_ROWS; r++)
        for (unsigned c = 0; c < ORACLES_TEXT_BOX_GAME_COLUMNS; c++) {
            const unsigned cell = box->map + ((box->row + r) & (MAP_CELLS - 1u)) * MAP_CELLS + ((box->column + c) & (MAP_CELLS - 1u));
            tiles[r * ORACLES_TEXT_BOX_GAME_COLUMNS + c] = vram[cell];
            attributes[r * ORACLES_TEXT_BOX_GAME_COLUMNS + c] = vram[0x2000u + cell];
        }
    OraclesTextBoxSource *source = &v->host_text_box_source;
    memset(source, 0, sizeof *source);
    v->host_text_box_drawn = 0;
    uint8_t known[ORACLES_GUEST_TEXT_BOX_LINES];
    for (unsigned i = 0; i < ORACLES_GUEST_TEXT_BOX_LINES; i++) {
        source->lines[i] = oracles_guest_text_box_line(v->guest, i);
        known[i] = source->lines[i] && source->lines[i]->known;
    }
    box->read = oracles_text_box_read(tiles, attributes, known, &v->host_text_box_model);
    if (box->read == ORACLES_TEXT_BOX_NOT_SHOWN) return 0;
    if (box->read != ORACLES_TEXT_BOX_READ) return -1;
    if (pixels > ORACLES_TEXT_BOX_GAME_PIXELS) {   /* the map shows a line's first 128 pixels: the reveal is the text state's */
        OraclesGuestTextBoxReveal reveal;
        oracles_guest_text_box_reveal_state(v->guest, &reveal);
        oracles_text_box_reveal(&v->host_text_box_model, &reveal, source->lines);
    }
    source->vram = vram;
    source->signed_tiles = !(io[IO_LCDC] & 0x10u);
    source->bg_palettes = palettes;
    source->colours = v->colours;
    v->host_text_box_drawn = 1;
    const unsigned map = box->map, row = box->row, column = box->column;
    if (oracles_enhanced_view_host_text_box_again(v, pixels, lines, box) != 1) return 0;
    box->map = map;
    box->row = row;
    box->column = column;
    return 1;
}

int oracles_enhanced_view_host_text_box_again(OraclesEnhancedView *v, unsigned pixels, unsigned lines, OraclesEnhancedHostTextBox *box)
{
    memset(box, 0, sizeof *box);
    box->width = (pixels / 8u + 2u) * 8u;
    box->height = (2u * lines + 1u) * 8u;
    if (!v->host_text_box_drawn || pixels % 8u || box->width > ORACLES_TEXT_BOX_WIDTH_MAX || box->height > ORACLES_TEXT_BOX_HEIGHT_MAX) return 0;
    oracles_text_box_draw(&v->host_text_box_model, &v->host_text_box_source, pixels, lines, v->host_text_box, box->width);
    box->pixels = v->host_text_box;
    return 1;
}
