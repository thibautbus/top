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
static int text_box_shift(OraclesEnhancedView *v, uint8_t cells[TEXT_BOX_ROWS][TEXT_BOX_COLS], unsigned rows, unsigned cols, int32_t camera_x, int32_t *dx, int32_t *dy)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    int left = INT32_MAX, right = INT32_MIN, top = INT32_MAX;
    for (unsigned r = 0; r < rows; r++)
        for (unsigned c = 0; c < cols; c++) {
            if (!cells[r][c]) continue;
            const int gx = (int)c * 8 - ob->drawn_camera_x;
            if (gx + 8 <= 0 || gx >= (int)ORACLES_ENHANCED_CORE_WIDTH) continue;
            if (gx < left) left = gx;
            if (gx + 8 > right) right = gx + 8;
            if ((int)r * 8 - ob->drawn_camera_y < top) top = (int)r * 8 - ob->drawn_camera_y;
        }
    if (left > right) return 0;
    const int32_t band_left = ob->window_left + left - camera_x;
    *dx = ((int32_t)v->size.width - (right - left)) / 2 - band_left;
    *dy = (int32_t)(v->band_height - v->shown_height) / 2 + (top >= (int)ORACLES_ENHANCED_AREA_HEIGHT / 2 ? (int32_t)(v->shown_height - ORACLES_ENHANCED_AREA_HEIGHT) : 0);
    return 1;
}

void ev_overlay_text_box(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y)
{
    uint8_t cells[TEXT_BOX_ROWS][TEXT_BOX_COLS];
    unsigned rows = 0, cols = 0;
    if (!text_box_cells(v, cells, &rows, &cols)) return;
    int32_t dx = 0, dy = 0;
    if (!text_box_shift(v, cells, rows, cols, camera_x, &dx, &dy)) return;
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
    for (int pass = 0; pass < 2; pass++)
        for (unsigned row = 0; row < rows; row++)
            for (unsigned col = 0; col < cols; col++) {
                if (!cells[row][col]) continue;
                for (unsigned y = 0; y < 8u; y++)
                    for (unsigned x = 0; x < 8u; x++) {
                        const int gx = (int)(col * 8u + x) - ob->drawn_camera_x, gy = (int)(row * 8u + y) - ob->drawn_camera_y;
                        if (gx < 0 || gx >= (int)ORACLES_ENHANCED_CORE_WIDTH || gy < 0 || gy >= (int)ORACLES_ENHANCED_AREA_HEIGHT) continue;
                        const unsigned source = (ORACLES_ENHANCED_HUD_HEIGHT + (unsigned)gy) * ORACLES_PPU_WIDTH + (unsigned)gx;
                        /* The room at the window's place; the box at the same
                         * height on the screen as the game drew it, moved across. */
                        const int32_t px = pass == 0 ? ob->window_left + gx - camera_x : ob->window_left + gx - camera_x + dx;
                        const int32_t py = pass == 0 ? ob->window_top + gy - camera_y : gy + dy;
                        if (px < 0 || px >= (int32_t)v->size.width || py < 0 || py >= (int32_t)v->band_height) continue;
                        *ev_band_pixel(v, (unsigned)px, (unsigned)py) = pass == 0 ? v->popup_frame[source] : core[source];
                    }
            }
}
