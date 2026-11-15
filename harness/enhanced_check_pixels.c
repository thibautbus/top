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
    /* The host's wide box in place of the core's: the moved box is the host's (enhanced_check_text_box.c judges it). */
    int32_t dst[2], src[2];
    unsigned size[2];
    const int host = oracles_enhanced_view_text_box_shown(c->view, NULL, NULL, NULL) || oracles_enhanced_view_text_box_scaled(c->view, dst, src, size);
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
            if (host) continue;
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
    /* The host's wide box, wider than the game's: what it covers is its own. */
    OraclesEnhancedHostTextBox host;
    int32_t host_x = 0, host_y = 0;
    int wide = oracles_enhanced_view_text_box_shown(c->view, &host, &host_x, &host_y);
    unsigned host_w = host.width * oracles_enhanced_view_text_box_shown_scale(c->view), host_h = host.height * oracles_enhanced_view_text_box_shown_scale(c->view);
    {   /* the game's box twice as large (Text size Large): what it covers is its own too */
        int32_t dst[2], src[2];
        unsigned size[2];
        if (!wide && oracles_enhanced_view_text_box_scaled(c->view, dst, src, size)) {
            wide = 1;
            host_x = dst[0]; host_y = dst[1];
            host_w = 2u * size[0]; host_h = 2u * size[1];
        }
    }
    host_y -= (int32_t)ORACLES_ENHANCED_HUD_HEIGHT;
    for (unsigned r = 0; r < ORACLES_ENHANCED_AREA_HEIGHT; r++) {
        const int32_t y = (int32_t)r + wy;
        if (y < 0 || y >= (int32_t)c->band_height) continue;
        unsigned compared = 0, wrong = 0;
        for (unsigned col = 0; col < ORACLES_ENHANCED_CORE_WIDTH; col++) {
            const int32_t x = (int32_t)col + wx;
            if (x < 0 || x >= (int32_t)W) continue;
            /* A text box's cells: the view draws the room there, the box in the middle of the band. */
            if (box[r][col]) continue;
            if (wide && x >= host_x && x < host_x + (int32_t)host_w && y >= host_y && y < host_y + (int32_t)host_h) continue;
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
 * the window has none left are counted apart, their colour unchecked.  Out of a
 * large room (the last world frame without a curtain in one), the frames
 * before the first column whose window is the blank alone are the curtain
 * closed, every column of the band its colour (counted apart too). */
void ec_check_curtain(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, const uint32_t *surface, int32_t camera_x, int32_t camera_y, uint32_t frame)
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    const uint8_t mode = oracles_guest_read8(c->guest, t->scroll_mode);
    const uint8_t state = oracles_guest_read8(c->guest, t->screen_transition_state);
    const uint8_t substate = oracles_guest_read8(c->guest, t->screen_transition_substate);
    const uint32_t *core_pixels = oracles_core_pixels(c->core);
    /* Out of a large room, before the first column, with the window blank: closed, no column drawn. */
    const int closed = mode == CURTAIN_SCROLL_MODE && c->curtain_after_large && oracles_enhanced_curtain_closed(state, substate) && oracles_enhanced_window_blank(core_pixels);
    if (!closed && (mode != CURTAIN_SCROLL_MODE || state != CURTAIN_TRANSITION_STATE || substate != CURTAIN_SUBSTATE)) { c->curtain_after_large = ob->large; return; }
    const uint8_t scx = (uint8_t)(ob->drawn_camera_x + ob->drawn_offset_x);
    const unsigned next_left = oracles_guest_read8(c->guest, t->screen_scroll_row) & 31u;
    const unsigned next_right = oracles_guest_read8(c->guest, t->screen_scroll_column_right) & 31u;
    const unsigned middle = (((unsigned)scx + ORACLES_ENHANCED_CORE_WIDTH / 2u) >> 3) & 31u;
    const int middle_px = (int)(((middle * 8u) - scx) & 0xffu);
    const int left = closed ? 0 : middle_px - (int)((middle - 1u - next_left) & 31u) * 8;
    const int right = closed ? 0 : middle_px + (int)((next_right - middle) & 31u) * 8;
    c->curtain_frames++;
    if (closed) c->curtain_closed_frames++;
    /* The colour, read in the core's image: the middle line of the first
     * column of the window the game has not drawn. */
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

/* The area blurb (INTERAC_ERA_OR_SEASON_INFO), judged from the game's
 * objects and not from the view: at each frame the game draws, its wOam and
 * the blurb's slot then; the OAM on screen is one of those, found by its
 * content (the frame after the game deletes the blurb, at the end of its
 * slide out, it is the one drawn before, which still holds its last column).
 * The entries the hooks tag as the object at that slot, on the screen, are
 * the blurb's: while one of them is there, the view must take the blurb out
 * of the window and draw it apart (oracles_enhanced_view_blurb_drawn). */
#define ERA_OR_SEASON_INFO 0xe0u   /* constants/common/interactions.s */
void ec_note_blurb_drawn(OraclesEnhancedCheck *c)
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    const uint8_t *woam = oracles_guest_ptr(c->guest, t->oam, 160u);
    if (!woam) return;
    const unsigned at = c->blurb_drawn_head;
    c->blurb_drawn_head = (at + 1u) % 8u;
    if (c->blurb_drawn_count < 8u) c->blurb_drawn_count++;
    memcpy(c->blurb_drawn_oam[at], woam, 160u);
    c->blurb_drawn_slot[at] = -1;
    for (unsigned k = 0; k < 16u && c->blurb_drawn_slot[at] < 0; k++) {
        const uint8_t *o = oracles_guest_object(c->guest, k, 1);
        if (o && o[ORACLES_OBJ_ENABLED] && o[ORACLES_OBJ_ID] == ERA_OR_SEASON_INFO) c->blurb_drawn_slot[at] = (int)k;
    }
}

void ec_check_blurb(OraclesEnhancedCheck *c, OraclesEnhancedMode mode, uint32_t frame)
{
    const uint8_t *oam = oracles_guest_oam(c->guest);
    if (mode != ORACLES_ENHANCED_WORLD || !c->link_sprites || !oam) return;
    int slot = -1;
    for (unsigned k = 0; k < c->blurb_drawn_count; k++) {
        const unsigned at = (c->blurb_drawn_head + 7u - k) % 8u;
        if (memcmp(c->blurb_drawn_oam[at], oam, 160u) == 0) { slot = c->blurb_drawn_slot[at]; break; }
    }
    if (slot < 0) return;
    OraclesSpriteTag tags[ORACLES_SPRITE_TAGS];
    const unsigned count = oracles_sprites_tags_for_oam(c->link_sprites, oam, tags, ORACLES_SPRITE_TAGS);
    unsigned on_screen = 0;
    for (unsigned i = 0; i < count; i++) {
        if (tags[i].kind != 1u || tags[i].slot != (unsigned)slot || tags[i].terrain_effect) continue;
        for (unsigned n = 0; n < tags[i].count && tags[i].first + n < 40u; n++) {
            const uint8_t *e = oam + (tags[i].first + n) * 4u;
            if (e[0] > 0u && e[0] < 160u && e[1] > 0u && e[1] < 168u) on_screen++;
        }
    }
    if (!on_screen) return;
    c->blurb_frames++;
    if (oracles_enhanced_view_blurb_drawn(c->view)) return;
    c->blurb_frames_in_window++;
    const size_t used = strlen(c->blurb_list);
    if (used + 12 < sizeof c->blurb_list) snprintf(c->blurb_list + used, sizeof c->blurb_list - used, "%s%u", used ? " " : "", frame);
}

#define TINT_RING 24u              /* the band's columns and rows around the game's window compared with it */
#define TINT_DARKER_LUMA 40u       /* how much darker than the ring around it (mean luminance, 0..255) the window stands apart */

static unsigned pixel_luma(uint32_t p) { return (((p >> 16) & 0xffu) * 2u + ((p >> 8) & 0xffu) * 5u + (p & 0xffu)) / 8u; }

/* The game's window against the ring of the band around it: a scene that
 * darkens the game's screen (the night of Nayru's possession) and leaves the
 * rooms around in their colours shows a window much darker than the ring.
 * In play the window stands as bright as the ring or brighter (the room Link
 * walks in against the trees around it, 30 to 75 on the routes), never 40
 * darker.  Counted on the world frames with a ring of at least a quarter of
 * the window's pixels on screen; a blank surface (a fade's end), a warp's
 * curtain, a text box and a large room are not. */
void ec_measure_window_darker(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, const uint32_t *surface,
                           int32_t camera_x, int32_t camera_y, int blank, uint32_t frame)
{
    c->window_luma = c->ring_luma = 0;
    /* A large room is shown alone, the rest of it around the window, which
     * may well be darker: no other room to compare with. */
    if (mode != ORACLES_ENHANCED_WORLD || !surface || blank || ob->large) return;
    /* The curtain of a warp opens the window column by column over the band;
     * a text box darkens the window by itself. */
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    if (oracles_guest_read8(c->guest, t->scroll_mode) == CURTAIN_SCROLL_MODE && oracles_guest_read8(c->guest, t->screen_transition_state) == CURTAIN_TRANSITION_STATE) return;
    if (oracles_guest_read8(c->guest, t->text_is_active)) return;
    const int32_t left = ob->window_left - camera_x, top = ob->window_top - camera_y;
    const int32_t w = (int32_t)ORACLES_ENHANCED_CORE_WIDTH, h = (int32_t)ORACLES_ENHANCED_AREA_HEIGHT, ring = (int32_t)TINT_RING;
    uint64_t in_sum = 0, ring_sum = 0;
    unsigned in_count = 0, ring_count = 0;
    for (int32_t y = top - ring; y < top + h + ring; y++) {
        if (y < 0 || y >= (int32_t)c->band_height) continue;
        for (int32_t x = left - ring; x < left + w + ring; x++) {
            if (x < 0 || x >= (int32_t)c->width) continue;
            const unsigned luma = pixel_luma(surface[(ORACLES_ENHANCED_HUD_HEIGHT + (unsigned)y) * c->width + (unsigned)x]);
            if (x >= left && x < left + w && y >= top && y < top + h) { in_sum += luma; in_count++; }
            else { ring_sum += luma; ring_count++; }
        }
    }
    if (!in_count || ring_count < in_count / 4u) return;
    c->window_luma = (unsigned)(in_sum / in_count);
    c->ring_luma = (unsigned)(ring_sum / ring_count);
    if (c->ring_luma <= c->window_luma + TINT_DARKER_LUMA) return;
    if (!c->window_darker_frames) c->window_darker_first = frame;
    c->window_darker_last = frame;
    c->window_darker_frames++;
}

/* A room the band shows whole and still under the waves (oracles_enhanced_view_room_whole): its left edge on each line
 * of the band, the first pixel other than the band's own outside at the line's start.  The lines differ where the
 * waves ripple the room (enhanced.whole_room_rippled_frames); the median of the lines' edges is the room's place,
 * which must not move from one such frame to the next, the camera still (enhanced.whole_room_edge_moves): the room
 * swaying as a block reads as the camera shaking.  Line by line, the lines whose edge changed from the frame before,
 * on the frames the game's window moved inside the room and on those it stood still: a ripple anchored to the room
 * changes as many lines either way, one that crawls over it with the game's window more where it moves
 * (enhanced.whole_room_row_changes_moving and _still, per hundred frames; the most lines in one frame, _max).  The same
 * over every frame under the waves, the band's camera still from one to the next (enhanced.water_*).  Where a room of a
 * dungeon is drawn left of the room in play, its line's start is that room's, not the band's outside: the room's right
 * edge is measured instead, the last pixel other than the band's outside at the line's end, unless a room is drawn
 * right of it too (the frame not measured); the frames of the two rates are counted (_moving_frames, _still_frames). */
static void measure_ripple(OraclesEnhancedCheck *c, EcRipple *r, const OraclesEnhancedObservation *ob, const uint32_t *surface, int32_t camera_x, int32_t camera_y,
                           int right)
{
    static int32_t edges[512];
    static int16_t row_edge[512];
    unsigned count = 0;
    const unsigned height = oracles_enhanced_view_height(c->view), half = c->width / 2u;
    for (unsigned y = ORACLES_ENHANCED_HUD_HEIGHT; y < height && y < 512u; y++) {
        const uint32_t *row = surface + (size_t)y * c->width;
        if (right) {
            const uint32_t outside = row[c->width - 1u];
            unsigned x = c->width - 1u;
            while (x > half && row[x] == outside) x--;
            row_edge[y] = x > half && x < c->width - 1u ? (int16_t)x : -1;
        } else {
            const uint32_t outside = row[0];
            unsigned x = 0;
            while (x < half && row[x] == outside) x++;
            row_edge[y] = x > 0 && x < half ? (int16_t)x : -1;
        }
        if (row_edge[y] >= 0) edges[count++] = row_edge[y];
    }
    if (!count) { r->have_edge = 0; return; }
    const int same = r->have_edge && r->right == right && r->epoch == ob->epoch && r->camera_x == camera_x && r->camera_y == camera_y;
    if (same) {
        unsigned changed = 0;
        for (unsigned y = ORACLES_ENHANCED_HUD_HEIGHT; y < height && y < 512u; y++)
            changed += row_edge[y] >= 0 && r->row_edge[y] >= 0 && row_edge[y] != r->row_edge[y];
        if (ob->window_left != r->window_left || ob->window_top != r->window_top) { r->moving_frames++; r->moving_changes += changed; }
        else { r->still_frames++; r->still_changes += changed; }
        if (changed > r->changes_max) r->changes_max = changed;
    }
    memcpy(r->row_edge, row_edge, sizeof r->row_edge);
    r->window_left = ob->window_left; r->window_top = ob->window_top;
    for (unsigned i = 1; i < count; i++)
        for (unsigned j = i; j > 0 && edges[j - 1] > edges[j]; j--) { const int32_t t = edges[j]; edges[j] = edges[j - 1]; edges[j - 1] = t; }
    const int32_t edge = edges[count / 2u];
    r->wave_frames++;
    if (edges[0] != edges[count - 1u]) r->rippled_frames++;
    if (same && edge != r->edge) r->edge_moves++;
    r->have_edge = 1; r->right = right; r->edge = edge; r->epoch = ob->epoch; r->camera_x = camera_x; r->camera_y = camera_y;
}

void ec_measure_whole_room(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, const uint32_t *surface,
                           int32_t camera_x, int32_t camera_y)
{
    const unsigned waves = oracles_enhanced_view_wave_frames(c->view);
    const int rippled = waves != c->whole_last_waves;
    c->whole_last_waves = waves;
    const unsigned sides = oracles_enhanced_view_dungeon_sides(c->view);
    const int right = (sides & (1u << ORACLES_DIR_LEFT)) != 0;
    const int world = mode == ORACLES_ENHANCED_WORLD && surface && rippled && !ob->in_transition && !(right && (sides & (1u << ORACLES_DIR_RIGHT)));
    if (world && oracles_enhanced_view_room_whole(c->view)) measure_ripple(c, &c->whole, ob, surface, camera_x, camera_y, right);
    else c->whole.have_edge = 0;
    if (world) measure_ripple(c, &c->water, ob, surface, camera_x, camera_y, right);
    else c->water.have_edge = 0;
}

/* At a scroll's start out of a room the band showed whole, the game's window still, the game in that room still: the
 * part of the band shown (the rest black) holds the room's top line, the camera where it stood; a camera a pixel further
 * cuts it, and the band moved back by its fraction shows a black line over the room (enhanced.alone_scroll_cut_frames). */
void ec_measure_alone_rows(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, int32_t camera_y)
{
    if (mode != ORACLES_ENHANCED_WORLD || !c->alone_whole || !ob->in_scroll || !ob->world.alone || ob->window_left != ob->scroll_start_left
        || ob->window_top != ob->scroll_start_top || ob->group != c->alone_group || ob->room != c->alone_room) return;
    unsigned shown_w = 0, shown_h = 0;
    oracles_enhanced_view_shown(c->view, &shown_w, &shown_h);
    const int32_t band = (int32_t)(oracles_enhanced_view_height(c->view) - ORACLES_ENHANCED_HUD_HEIGHT);
    if (camera_y + (band - (int32_t)shown_h) / 2 > ob->world.origin_y) c->alone_scroll_cut_frames++;
}
