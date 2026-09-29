/* The Enhanced check: the band's pixels judged against the core's image,
 * the text box, the window's lines and the curtain of a warp. */
#include "enhanced_check_internal.h"

/* The window's image in the band, line by line: every line of the core's
 * game area must stand at the row the camera puts it, whatever the scroll
 * of that line (a ripple moves the room inside the window, not the window).
 * The frames where the area blurb was taken out of the window are skipped:
 * its pixels are repainted. */
/* The cells of a dialogue's text box in the game's window, judged from the
 * game's memory: a cell of the displayed background map that differs from
 * the room's map as the game keeps it for the VRAM (w3VramTiles and
 * w3VramAttributes, which the box never touches).  Marks the window pixels
 * of those cells in `box` (160 x 128) and returns how many cells. */
#define BOX_MAP_CELLS 32u
unsigned ec_text_box_pixels(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, uint8_t box[ORACLES_ENHANCED_AREA_HEIGHT][ORACLES_ENHANCED_CORE_WIDTH])
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    memset(box, 0, ORACLES_ENHANCED_AREA_HEIGHT * ORACLES_ENHANCED_CORE_WIDTH);
    /* The frame the game's flag falls still shows the box (view.h). */
    const int active = oracles_guest_read8(c->guest, t->text_is_active) != 0;
    if (!active && !c->text_box_trail) return 0;
    const unsigned rows = ob->large ? 22u : 16u, cols = ob->large ? 30u : 20u;
    const uint8_t *io = oracles_guest_io(c->guest), *vram0 = oracles_guest_vram(c->guest, 0), *vram1 = oracles_guest_vram(c->guest, 1);
    const uint8_t *tiles = oracles_guest_ptr(c->guest, t->vram_tiles, BOX_MAP_CELLS * 22u), *attributes = oracles_guest_ptr(c->guest, t->vram_attributes, BOX_MAP_CELLS * 22u);
    if (!io || !vram0 || !vram1 || !tiles || !attributes) return 0;
    const unsigned map = (io[0x40] & 0x08u) ? 0x1c00u : 0x1800u;
    const unsigned row0 = ((unsigned)ob->drawn_offset_y >> 3) & 31u, col0 = ((unsigned)ob->drawn_offset_x >> 3) & 31u;
    unsigned cells = 0;
    for (unsigned r = 0; r < rows; r++)
        for (unsigned col = 0; col < cols; col++) {
            const unsigned shown = map + ((row0 + r) & 31u) * BOX_MAP_CELLS + ((col0 + col) & 31u);
            if (vram0[shown] == tiles[r * BOX_MAP_CELLS + col] && vram1[shown] == attributes[r * BOX_MAP_CELLS + col]) continue;
            cells++;
            for (unsigned y = 0; y < 8u; y++)
                for (unsigned x = 0; x < 8u; x++) {
                    const int gx = (int)(col * 8u + x) - ob->drawn_camera_x, gy = (int)(r * 8u + y) - ob->drawn_camera_y;
                    if (gx >= 0 && gx < (int)ORACLES_ENHANCED_CORE_WIDTH && gy >= 0 && gy < (int)ORACLES_ENHANCED_AREA_HEIGHT) box[gy][gx] = 1;
                }
        }
    return oracles_enhanced_text_box_holds(active, cells, &c->text_box_trail) ? cells : 0;
}

/* The box where the view puts it: its pixels, as the
 * core drew them in its window, centred across the band at the height the
 * game drew them on its screen, from the edge of the part of the band shown
 * it drew them against (the drawn-back view's taller band). */
void ec_check_text_box(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, const uint32_t *surface, int32_t camera_x, int32_t camera_y,
                           uint8_t box[ORACLES_ENHANCED_AREA_HEIGHT][ORACLES_ENHANCED_CORE_WIDTH], uint32_t frame)
{
    int left = INT32_MAX, right = INT32_MIN, top = INT32_MAX;
    for (unsigned gy = 0; gy < ORACLES_ENHANCED_AREA_HEIGHT; gy++)
        for (unsigned gx = 0; gx < ORACLES_ENHANCED_CORE_WIDTH; gx++)
            if (box[gy][gx]) { if ((int)gx < left) left = (int)gx; if ((int)gx + 1 > right) right = (int)gx + 1; if ((int)gy < top) top = (int)gy; }
    if (left > right) return;
    unsigned shown_w = 0, shown_h = 0;
    oracles_enhanced_view_shown(c->view, &shown_w, &shown_h);
    const unsigned dy = (c->band_height - shown_h) / 2u + (top >= (int)ORACLES_ENHANCED_AREA_HEIGHT / 2 ? shown_h - ORACLES_ENHANCED_AREA_HEIGHT : 0u);
    const unsigned W = c->width;
    c->text_box_frames++;
    const int32_t shift = ((int32_t)W - (right - left)) / 2 - (ob->window_left + left - camera_x);
    const uint32_t *core_pixels = oracles_core_pixels(c->core);
    unsigned wrong = 0, in_place = 0, place = 0;
    uint32_t first = 0;
    int varied = 0;   /* the box's pixels compared are not all of one colour: a black border matches a dark room */
    for (unsigned gy = 0; gy < ORACLES_ENHANCED_AREA_HEIGHT; gy++)
        for (unsigned gx = 0; gx < ORACLES_ENHANCED_CORE_WIDTH; gx++) {
            if (!box[gy][gx]) continue;
            /* Its own place in the window, where the moved box does not fall:
             * the room redrawn under it, not the box left there. */
            const int32_t home = ob->window_left + (int32_t)gx - camera_x;
            const int32_t moved_left = ob->window_left + left - camera_x + shift, moved_right = moved_left + (right - left);
            const int32_t home_y = ob->window_top + (int32_t)gy - camera_y;   /* the row of the window in the band, where the box's own place is */
            if (home >= 0 && home < (int32_t)W && home_y >= 0 && home_y < (int32_t)c->band_height
                && (home < moved_left || home >= moved_right)) {
                const uint32_t drawn = core_pixels[(ORACLES_ENHANCED_HUD_HEIGHT + gy) * ORACLES_ENHANCED_CORE_WIDTH + gx];
                if (!place) first = drawn;
                else if (drawn != first) varied = 1;
                place++;
                if (surface[(ORACLES_ENHANCED_HUD_HEIGHT + (unsigned)home_y) * W + (unsigned)home] == drawn) in_place++;
            }
            const int32_t x = ob->window_left + (int32_t)gx - camera_x + shift;
            if (x < 0 || x >= (int32_t)W) { wrong++; continue; }   /* the whole box must fit in the band */
            if (surface[(ORACLES_ENHANCED_HUD_HEIGHT + gy + dy) * W + (unsigned)x] != core_pixels[(ORACLES_ENHANCED_HUD_HEIGHT + gy) * ORACLES_ENHANCED_CORE_WIDTH + gx]) wrong++;
        }
    /* The box's own place, where the moved box does not fall: the view draws
     * the room there, and the room and the box share colours, so a pixel
     * matches here and there; every one of them matching is the box left
     * where the game drew it.  The two counts differ in what they are for:
     * the first says the box was not taken out of its place, the second that
     * the band showed it there although the view had to move it. */
    if (varied && place >= 16u && in_place == place) c->text_box_frames_in_place++;
    if (shift != 0 && varied && place >= 16u && in_place == place) c->text_box_frames_at_origin++;
    if (wrong) {
        c->text_box_frames_wrong++;
        c->text_box_pixels_wrong += wrong;
        const size_t used = strlen(c->text_box_list);
        if (used + 16 < sizeof c->text_box_list) snprintf(c->text_box_list + used, sizeof c->text_box_list - used, "%s%u:%u", used ? " " : "", frame, wrong);
    }
}

void ec_check_window_lines(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, const uint32_t *surface, int32_t camera_x, int32_t camera_y,
                               uint8_t box[ORACLES_ENHANCED_AREA_HEIGHT][ORACLES_ENHANCED_CORE_WIDTH], uint32_t frame)
{
    const uint32_t *core_pixels = oracles_core_pixels(c->core);
    const int32_t wx = ob->window_left - camera_x, wy = ob->window_top - camera_y;
    const unsigned W = c->width;
    for (unsigned r = 0; r < ORACLES_ENHANCED_AREA_HEIGHT; r++) {
        const int32_t y = (int32_t)r + wy;
        if (y < 0 || y >= (int32_t)c->band_height) continue;
        unsigned compared = 0, wrong = 0;
        for (unsigned col = 0; col < ORACLES_ENHANCED_CORE_WIDTH; col++) {
            const int32_t x = (int32_t)col + wx;
            if (x < 0 || x >= (int32_t)W) continue;
            /* A text box's cells: the view draws the room there, the box in the middle of the band. */
            if (box[r][col]) continue;
            compared++;
            if (surface[(ORACLES_ENHANCED_HUD_HEIGHT + (unsigned)y) * W + (unsigned)x]
                != core_pixels[(ORACLES_ENHANCED_HUD_HEIGHT + r) * ORACLES_ENHANCED_CORE_WIDTH + col]) wrong++;
        }
        if (!compared) continue;
        c->window_lines_checked++;
        if (wrong) {
            c->window_lines_wrong++;
            const size_t used = strlen(c->window_lines_list);
            if (used + 16 < sizeof c->window_lines_list) snprintf(c->window_lines_list + used, sizeof c->window_lines_list - used, "%s%u:%u/%u", used ? " " : "", frame, r, wrong);
        }
    }
}

/* The curtain of a warp, judged from the game's own variables and not from
 * the view's: in state 1 of a screen transition the game redraws the columns
 * of its map outward from the middle of its window, one a frame, and the
 * view paints the columns still to come over the whole band.  Every band
 * pixel outside the game's window whose
 * column has not been drawn must hold the curtain's colour, the one the
 * core itself shows in an undrawn column of its window; the frames where
 * the window has none left are counted apart, their colour unchecked. */
void ec_check_curtain(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, const uint32_t *surface, int32_t camera_x, int32_t camera_y, uint32_t frame)
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    if (oracles_guest_read8(c->guest, t->scroll_mode) != CURTAIN_SCROLL_MODE
        || oracles_guest_read8(c->guest, t->screen_transition_state) != CURTAIN_TRANSITION_STATE
        || oracles_guest_read8(c->guest, t->screen_transition_substate) != CURTAIN_SUBSTATE) return;
    const uint8_t scx = (uint8_t)(ob->drawn_camera_x + ob->drawn_offset_x);
    const unsigned next_left = oracles_guest_read8(c->guest, t->screen_scroll_row) & 31u;
    const unsigned next_right = oracles_guest_read8(c->guest, t->screen_scroll_column_right) & 31u;
    const unsigned middle = (((unsigned)scx + ORACLES_ENHANCED_CORE_WIDTH / 2u) >> 3) & 31u;
    const int middle_px = (int)(((middle * 8u) - scx) & 0xffu);
    const int left = middle_px - (int)((middle - 1u - next_left) & 31u) * 8;
    const int right = middle_px + (int)((next_right - middle) & 31u) * 8;
    c->curtain_frames++;
    /* The colour, read in the core's image: the middle line of the first
     * column of the window the game has not drawn. */
    const uint32_t *core_pixels = oracles_core_pixels(c->core);
    int sample = -1;
    for (int gx = 4; gx < (int)ORACLES_ENHANCED_CORE_WIDTH; gx += 8) if (gx < left || gx >= right) { sample = gx; break; }
    if (sample < 0 || !core_pixels) { c->curtain_frames_unchecked++; return; }
    const uint32_t colour = core_pixels[(ORACLES_ENHANCED_HUD_HEIGHT + ORACLES_ENHANCED_AREA_HEIGHT / 2u) * ORACLES_ENHANCED_CORE_WIDTH + (unsigned)sample];
    unsigned wrong = 0, window_wrong = 0, shown_w = 0, shown_h = 0;
    oracles_enhanced_view_shown(c->view, &shown_w, &shown_h);   /* the curtain over the part of the band shown, black around it */
    const unsigned W = c->width, shown_x = (W - shown_w) / 2u, shown_y = (c->band_height - shown_h) / 2u;
    for (unsigned x = shown_x; x < shown_x + shown_w; x++) {
        const int window_x = (int)(camera_x + (int32_t)x - ob->window_left);
        /* Inside the window the band is the core's image, drawn columns and
         * curtain alike: the view paints the curtain around it only (a
         * curtain over the whole band would pass the rule below alone).  The
         * area blurb, which the view takes out of the window, aside. */
        if (window_x >= 0 && window_x < (int)ORACLES_ENHANCED_CORE_WIDTH && !oracles_enhanced_view_blurb_drawn(c->view))
            for (unsigned y = 0; y < c->band_height; y++) {
                const int window_y = (int)(camera_y + (int32_t)y - ob->window_top);
                if (window_y < 0 || window_y >= (int)ORACLES_ENHANCED_AREA_HEIGHT) continue;
                if (surface[(ORACLES_ENHANCED_HUD_HEIGHT + y) * W + x]
                    != core_pixels[(ORACLES_ENHANCED_HUD_HEIGHT + (unsigned)window_y) * ORACLES_ENHANCED_CORE_WIDTH + (unsigned)window_x]) window_wrong++;
            }
        if (window_x >= left && window_x < right) continue;
        for (unsigned y = shown_y; y < shown_y + shown_h; y++) {
            const int window_y = (int)(camera_y + (int32_t)y - ob->window_top);
            if (window_x >= 0 && window_x < (int)ORACLES_ENHANCED_CORE_WIDTH && window_y >= 0 && window_y < (int)ORACLES_ENHANCED_AREA_HEIGHT) continue;
            if (surface[(ORACLES_ENHANCED_HUD_HEIGHT + y) * W + x] != colour) wrong++;
        }
    }
    if (window_wrong) c->curtain_window_frames_wrong++;
    if (wrong) {
        c->curtain_frames_wrong++;
        c->curtain_pixels_wrong += wrong;
        const size_t used = strlen(c->curtain_list);
        if (used + 16 < sizeof c->curtain_list) snprintf(c->curtain_list + used, sizeof c->curtain_list - used, "%s%u:%u", used ? " " : "", frame, wrong);
    }
}
