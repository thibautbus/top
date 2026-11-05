/* The Enhanced check: the camera, the scrolls, the window's place and the
 * savestate reload (measures of enhanced_check.c). */
#include "enhanced_check_internal.h"

static int reload_save_state(OraclesEnhancedCheck *c, int32_t camera_x, const int shown[4], const uint8_t room[4])
{
    const OraclesEnhancedObservation *ob = oracles_enhanced_view_observation(c->view);
    c->saved_core_size = oracles_core_state_size(c->core);
    c->saved_core = malloc(c->saved_core_size);
    if (!c->saved_core || oracles_core_save_state(c->core, c->saved_core, c->saved_core_size) != 0) return 0;
    if (oracles_enhanced_view_save_state(c->view, c->saved_view, sizeof c->saved_view, &c->saved_view_size) != 0) return 0;
    c->saved_transition_size = oracles_room_transition_save_state(c->transition, c->saved_transition, sizeof c->saved_transition);
    c->saved_compact_size = oracles_guest_compact_save_state(c->guest, c->saved_compact, sizeof c->saved_compact);
    c->saved_text_box_size = oracles_guest_text_box_save_state(c->guest, c->saved_text_box, sizeof c->saved_text_box);   /* the host's lines, empty unarmed */
    c->saved_camera = camera_x;
    memcpy(c->saved_shown, shown, sizeof c->saved_shown);
    memcpy(c->saved_room, room, sizeof c->saved_room);
    c->reload_saved_in_scroll = ob && ob->playing && ob->in_scroll;
    if (c->reload_saved_in_scroll) {
        /* During a scroll the view still references the room left, while
         * the guest's active identity already names the arrival room. */
        c->reload_destination_group = ob->group;
        c->reload_destination_room = ob->room;
    }
    /* Non-scroll reloads retain the old contract exactly: the target is
     * the topology saved at F. A scroll defers choosing its target until
     * the natural run reaches the destination immediately before load. */
    c->reload_expected_valid = !c->reload_saved_in_scroll;
    memcpy(c->reload_expected_shown, c->saved_shown, sizeof c->reload_expected_shown);
    memcpy(c->reload_expected_room, c->saved_room, sizeof c->reload_expected_room);
    c->reload_saved = 1;
    return 1;
}

static int reload_load_state(OraclesEnhancedCheck *c, const int shown[4], const uint8_t room[4])
{
    if (c->reload_saved_in_scroll) {
        const OraclesEnhancedObservation *ob = oracles_enhanced_view_observation(c->view);
        /* This is the natural pre-load observation. Do not accept a
         * topology from the room left, from another room reached after
         * the save, or from a transition still in flight; an unavailable
         * arrival target is a failed check. */
        c->reload_expected_valid = ob && ob->playing && !ob->in_transition && !ob->in_scroll
            && ob->group == c->reload_destination_group && ob->room == c->reload_destination_room;
        if (c->reload_expected_valid) {
            memcpy(c->reload_expected_shown, shown, sizeof c->reload_expected_shown);
            memcpy(c->reload_expected_room, room, sizeof c->reload_expected_room);
        }
    }
    if (oracles_core_load_state(c->core, c->saved_core, c->saved_core_size) != 0) return 0;
    oracles_guest_reset_execution_state(c->guest);
    if (c->saved_transition_size && oracles_room_transition_load_state(c->transition, c->saved_transition, c->saved_transition_size) != 0) return 0;
    if (c->saved_compact_size && oracles_guest_compact_load_state(c->guest, c->saved_compact, c->saved_compact_size) != 0) return 0;
    if (c->saved_text_box_size && oracles_guest_text_box_load_state(c->guest, c->saved_text_box, c->saved_text_box_size) != 0) return 0;
    if (oracles_enhanced_view_load_state(c->view, c->saved_view, c->saved_view_size) != 0) return 0;
    if (c->link_sprites) oracles_sprites_forget(c->link_sprites);
    c->have_drawn_link = 0;
    c->blurb_drawn_count = 0;   /* the frames drawn before are about another moment */
    return 1;
}

/* A state saved in a transition: Link's place at each frame after the save, to the frame the transition ends on, in
 * the run before the load, against the same frames after it, which the transitions' progress loaded back with the
 * rest takes through it as if no load had come between. */
static void reload_track_transition(OraclesEnhancedCheck *c, uint32_t frame)
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    const uint8_t state = oracles_guest_read8(c->guest, t->screen_transition_state);
    const uint8_t *link = oracles_guest_object(c->guest, 0, 0);
    if (!link) return;
    const uint32_t place = (uint32_t)link[ORACLES_OBJ_Y] | (uint32_t)link[ORACLES_OBJ_YH] << 8 | (uint32_t)link[ORACLES_OBJ_X] << 16 | (uint32_t)link[ORACLES_OBJ_XH] << 24;
    if (frame == c->reload_at) { c->reload_track_open = state >= 3u && state <= 5u; return; }
    if (!c->reload_done) {
        if (!c->reload_track_open || c->reload_tracked == sizeof c->reload_track / sizeof c->reload_track[0]) return;
        c->reload_track[c->reload_tracked++] = place;
        if (state < 3u || state > 5u) c->reload_track_open = 0;
        return;
    }
    if (frame > c->reload_frame && frame - c->reload_frame <= c->reload_tracked) {
        c->reload_track_compared++;
        if (place != c->reload_track[frame - c->reload_frame - 1u]) c->reload_track_differing++;
    }
}

void ec_reload_step(OraclesEnhancedCheck *c, uint32_t frame, int32_t camera_x, const int shown[4], const uint8_t room[4])
{
    if (!c->reload_at) return;
    reload_track_transition(c, frame);
    if (frame == c->reload_at && !c->reload_saved) {
        if (!reload_save_state(c, camera_x, shown, room)) return;
        return;
    }
    if (c->reload_saved && !c->reload_done && frame == c->reload_at + 120u) {
        if (!reload_load_state(c, shown, room)) return;
        c->reload_done = 1;
        c->reload_frame = frame;
        c->have_camera = 0;   /* the load puts the camera back where it stood at the save, as a teleport snaps it */
        return;
    }
    if (c->reload_done && !c->reload_settled) {
        if (frame == c->reload_frame + 1u) c->reload_camera_delta = camera_x - c->saved_camera;
        const int same = c->reload_expected_valid
            && memcmp(shown, c->reload_expected_shown, sizeof c->reload_expected_shown) == 0
            && memcmp(room, c->reload_expected_room, sizeof c->reload_expected_room) == 0;
        if (same || frame > c->reload_frame + RELOAD_SETTLE_TIMEOUT) {
            c->reload_settled_after = frame - c->reload_frame;
            c->reload_settled = same ? 1 : 2;
        }
    }
    /* A camera that is back at the saved point but frozen would pass the test above: measure whether
     * it still follows Link over the 300 frames after the load. */
    if (c->reload_done && frame > c->reload_frame && frame <= c->reload_frame + RELOAD_SETTLE_TIMEOUT) {
        const OraclesEnhancedObservation *ob = oracles_enhanced_view_observation(c->view);
        if (frame == c->reload_frame + 1u) { c->reload_link_start = ob->world.link_x; c->reload_camera_start = camera_x; }
        const int32_t lt = ob->world.link_x - c->reload_link_start, ct = camera_x - c->reload_camera_start;
        const int32_t alt = lt < 0 ? -lt : lt, act = ct < 0 ? -ct : ct;
        if (alt > c->reload_link_travel) c->reload_link_travel = alt;
        if (act > c->reload_camera_travel) c->reload_camera_travel = act;
    }
}

/* Between two world frames of the same epoch (a teleport snaps by design):
 * the camera's jumps on both axes, and outside cutscenes the feel of the
 * follow (steps, stillness while Link walks, step changes, Link's offset
 * from the centre). */
void ec_measure_camera(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, int32_t camera_x, int32_t camera_y, int blank)
{
    if (mode != ORACLES_ENHANCED_WORLD) {
        c->framed_frames++;
        c->have_camera_delta = 0;
        if (!c->have_mode || c->last_mode != ORACLES_ENHANCED_FRAMED) c->fallback_runs++;
        c->last_mode = mode;
        c->have_mode = 1;
        return;
    }
    c->world_frames++;
    unsigned shown_w = 0, shown_h = 0;   /* the part of the band shown: all of it, but where the drawn-back view keeps the normal band */
    oracles_enhanced_view_shown(c->view, &shown_w, &shown_h);
    if (shown_w < c->width) c->normal_band_frames++;
    const int32_t shown_left = camera_x + (int32_t)(c->width - shown_w) / 2;
    if (c->have_camera && c->have_mode && c->last_mode == ORACLES_ENHANCED_WORLD && ob->epoch == c->epochs) {
        const int32_t delta = camera_x - c->last_camera;
        const int32_t magnitude = delta < 0 ? -delta : delta;
        /* a large room's scroll slides the band with the game's window; a move to or from a screen of one colour (a
         * fade's end, where a band narrower than a room takes the window the game sets as it fades in) is not seen */
        const int unseen = (ob->large_grid && ob->in_scroll) || blank || c->last_blank;
        if (magnitude > MAX_CAMERA_STEP && !unseen) { c->camera_jumps++; if (magnitude > c->largest_jump) c->largest_jump = magnitude; }
        const int32_t delta_y = camera_y - c->last_camera_y;
        const int32_t magnitude_y = delta_y < 0 ? -delta_y : delta_y;
        if (magnitude_y > MAX_CAMERA_STEP && !unseen) { c->camera_jumps_y++; if (magnitude_y > c->largest_jump_y) c->largest_jump_y = magnitude_y; }
        if (!ob->cutscene) {
            c->camera_steps[magnitude > 4 ? 4 : magnitude]++;
            /* Still while Link walks, the camera not clamped at the row's edge: the dead zone at work. */
            const int clamped = shown_left <= ob->world.bounds_origin_x || shown_left >= ob->world.bounds_origin_x + ob->world.bounds_width - (int32_t)shown_w;
            if (magnitude == 0 && !clamped && c->have_last_observation && ob->world.link_x != c->last_link_x) c->camera_still_link_moving++;
            if (c->have_camera_delta && magnitude != c->last_camera_delta) c->camera_step_changes++;
            c->last_camera_delta = magnitude;
            c->have_camera_delta = 1;
        }
    } else c->have_camera_delta = 0;
    if (!ob->cutscene) {
        const int32_t offset = ob->world.link_x - (shown_left + (int32_t)shown_w / 2);
        const int32_t magnitude = offset < 0 ? -offset : offset;
        c->offset_sum += (uint64_t)magnitude;
        if (magnitude > c->offset_max) c->offset_max = magnitude;
        c->offset_frames++;
        /* The room the camera leaves Link: his distance to the nearer edge of
         * the band, outside a transition, where the camera is the reducer's
         * own and not the game's window sliding.  A camera that stops
         * following him under one pixel a frame lets it fall to nothing. */
        if (!ob->in_transition) {
            const int32_t margin = (int32_t)shown_w / 2 - magnitude;
            if (margin < c->link_edge_margin_min) c->link_edge_margin_min = margin;
            if (margin < 8) c->link_near_edge_frames++;
        }
    }
    c->last_camera = camera_x;
    c->last_camera_y = camera_y;
    c->last_blank = blank;
    c->have_camera = 1;
    if (!c->have_mode || c->last_mode != ORACLES_ENHANCED_WORLD) c->world_runs++;
    c->last_mode = mode;
    c->have_mode = 1;
}

/* World frames with pixels no source covered. */
void ec_measure_coverage(OraclesEnhancedCheck *c, uint32_t frame, unsigned uncovered)
{
    c->uncovered_frames++;
    c->uncovered_pixels += uncovered;
    if (uncovered > c->uncovered_max) c->uncovered_max = uncovered;
    const size_t used = strlen(c->uncovered_list);
    if (used + 16 < sizeof c->uncovered_list) snprintf(c->uncovered_list + used, sizeof c->uncovered_list - used, "%s%u:%u", used ? " " : "", frame, uncovered);
}

/* The scrolling transitions themselves, from state 3 to normal play: the
 * frames they take, those where Link's world position did not move, the
 * arrival tile; and, in a large room, the frames of a scroll with the room
 * entered missing beyond the gutters. */
void ec_measure_scroll(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, unsigned uncovered)
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    const uint8_t state = oracles_guest_read8(c->guest, t->screen_transition_state);
    const uint8_t scroll = oracles_guest_read8(c->guest, t->scroll_mode);
    const int scrolling = ob->playing && state >= 3u && state <= 5u && (scroll == 4u || scroll == 8u);
    unsigned shown_w = 0, shown_h = 0;
    oracles_enhanced_view_shown(c->view, &shown_w, &shown_h);
    /* The normal band counts the room's gutters as black; the drawn-back view counts the rooms' own pixels only, and
     * so does a band narrower than the room (the near view in 4:3, 213 wide), which has no gutter. */
    const unsigned decided = shown_w > 256u || shown_w <= 240u ? 0u : (shown_w - 240u) * shown_h;
    if (scrolling && ob->large_grid && mode == ORACLES_ENHANCED_WORLD && uncovered > decided) c->large_scroll_black++;
    /* Walking diagonally into a scroll: his place moved 3 px or more both ways over the 8 frames before it.  The
     * scroll's frames where it stands across the axis two frames running are the view sliding along it alone. */
    const int across_x = !(ob->scroll_direction & 1u);
    if (scrolling && !c->in_transition) {
        const unsigned back = c->recent_count < 8u ? c->recent_count : 8u;
        const unsigned oldest = (c->recent_count - back) % 8u;
        const int32_t dx = back ? ob->world.link_x - c->recent_x[oldest] : 0, dy = back ? ob->world.link_y - c->recent_y[oldest] : 0;
        c->scroll_diagonal = back == 8u && c->recent_epoch == ob->epoch && (dx >= 3 || dx <= -3) && (dy >= 3 || dy <= -3);
        if (c->scroll_diagonal) c->scrolls_diagonal++;
        c->scroll_across_last = across_x ? ob->world.link_x : ob->world.link_y;
        c->scroll_across_step = 1;
    } else if (scrolling && c->scroll_diagonal) {
        const int32_t across = across_x ? ob->world.link_x : ob->world.link_y;
        const int32_t step = across - c->scroll_across_last;
        if (!step && !c->scroll_across_step) c->scroll_across_stalled++;
        c->scroll_across_step = step;
        c->scroll_across_last = across;
    }
    if (!ob->in_transition) {
        if (c->recent_epoch != ob->epoch) c->recent_count = 0;
        c->recent_epoch = ob->epoch;
        c->recent_x[c->recent_count % 8u] = ob->world.link_x; c->recent_y[c->recent_count % 8u] = ob->world.link_y;
        c->recent_count++;
    }
    if (scrolling && !c->in_transition) { c->in_transition = 1; c->transition_frames = 0; c->transition_frozen = 0; c->transition_last_x = ob->world.link_x; c->transition_last_y = ob->world.link_y; c->transition_frozen_at[0] = 0; }
    else if (scrolling) {
        c->transition_frames++;
        if (ob->world.link_x == c->transition_last_x && ob->world.link_y == c->transition_last_y) {
            c->transition_frozen++;
            const size_t used = strlen(c->transition_frozen_at);
            if (used + 5 < sizeof c->transition_frozen_at) snprintf(c->transition_frozen_at + used, sizeof c->transition_frozen_at - used, "%s%u", used ? "," : "", c->transition_frames);
        }
        c->transition_last_x = ob->world.link_x; c->transition_last_y = ob->world.link_y;
    } else if (c->in_transition) {
        c->in_transition = 0;
        c->transitions_seen++;
        c->transition_frames_total += c->transition_frames;
        c->transition_frozen_total += c->transition_frozen;
        if (c->transition_frames > c->transition_frames_max) c->transition_frames_max = c->transition_frames;
        const uint8_t *link = oracles_guest_object(c->guest, 0, 0);
        const uint8_t *collisions = oracles_guest_ptr(c->guest, t->room_collisions, 176);
        const unsigned xh = link ? link[ORACLES_OBJ_XH] : 0u, yh = link ? link[ORACLES_OBJ_YH] : 0u;
        const int solid = collisions && xh < 160u && yh < 128u && collisions[(yh >> 4) * 16 + (xh >> 4)] != 0;
        if (solid) c->transition_arrivals_solid++;
        const size_t used = strlen(c->transition_list);
        if (used + 40 < sizeof c->transition_list)
            snprintf(c->transition_list + used, sizeof c->transition_list - used, "%s%02x:%u/%u[%s]@%u,%u%s", used ? " " : "",
                     oracles_guest_read8(c->guest, t->active_room), c->transition_frames, c->transition_frozen, c->transition_frozen_at, xh, yh, solid ? "!" : "");
    }
}

/* Between consecutive world frames of one epoch, outside cutscenes: a jump
 * of Link's world position or of the game's window is a seam that is not
 * continuous. */
void ec_measure_seams(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode)
{
    if (c->have_last_observation && ob->epoch == c->epochs && mode == ORACLES_ENHANCED_WORLD && c->last_mode_world && !ob->cutscene) {
        const int32_t dl = ob->world.link_x - c->last_link_x, dw = ob->window_left - c->last_window_left;
        const int32_t al = dl < 0 ? -dl : dl, aw = dw < 0 ? -dw : dw;
        if (al > 8) { c->link_jumps++; if (al > c->largest_link_jump) c->largest_link_jump = al; }
        if (aw > 8) { c->window_jumps++; if (aw > c->largest_window_jump) c->largest_window_jump = aw; }
        const int32_t dy = ob->world.link_y - c->last_link_y, ay = dy < 0 ? -dy : dy;
        if (ay > 8) { c->link_jumps_y++; if (ay > c->largest_link_jump_y) c->largest_link_jump_y = ay; }
    }
    c->epochs = ob->epoch;
    c->last_link_x = ob->world.link_x;
    c->last_link_y = ob->world.link_y;
    c->last_window_left = ob->window_left;
    c->last_playing = ob->playing;
    c->last_mode_world = mode == ORACLES_ENHANCED_WORLD;
    c->have_last_observation = 1;
}

/* Walking diagonally in normal play (Link's sprite as drawn moved 3 px or more both ways over the last 8 frames, never
 * back on either axis): the frames, and those where the camera steps across the walk the other way from its last step
 * across, the band going through the zigzag of his gait (x and y crossing their pixels on frames of their own). */
static int sign(int32_t v) { return (v > 0) - (v < 0); }

#define MEASURE_SCALE 4   /* the far view on a 1920x1080 screen */

/* 256ths of a pixel as whole pixels of the screen at MEASURE_SCALE, rounded to the nearest (the band's) or down
 * (Link's own), as the backend does. */
static int32_t shown_pixels(int32_t fraction)
{
    const int32_t f = fraction * MEASURE_SCALE;
    return f >= 0 ? (f + 128) / 256 : -((-f + 128) / 256);
}

static int32_t shown_pixels_down(int32_t fraction)
{
    const int32_t f = fraction * MEASURE_SCALE;
    return f >= 0 ? f / 256 : -((-f + 255) / 256);
}

/* Link's own fraction as the backend moves his layer by it: rounded down, but at the band's own fraction (the band
 * taking his, a dungeon's large room), on the band's pixel. */
static int32_t shown_link(int32_t layer_fraction, int32_t band_fraction)
{
    return layer_fraction ? shown_pixels_down(layer_fraction + band_fraction) : shown_pixels(band_fraction);
}

/* The band's place (the world's pixel at the screen's left and top) and Link's on the screen, in pixels of the
 * screen at MEASURE_SCALE, as the backend shows them: the band moved by its fraction, Link drawn apart at his own
 * where the view hands a layer, else with the band. */
static void shown_places(OraclesEnhancedCheck *c, const int32_t world[2], int32_t camera_x, int32_t camera_y, int32_t band[2], int32_t link[2])
{
    int32_t sx = 0, sy = 0, layer_x = 0, layer_y = 0, layer_fx = 0, layer_fy = 0;
    uint32_t band_top = 0, band_height = 0, layer_w = 0, layer_h = 0;
    const uint32_t *layer = NULL;
    oracles_enhanced_view_subpixel(c->view, &sx, &sy, &band_top, &band_height, &layer, &layer_x, &layer_y, &layer_w, &layer_h, &layer_fx, &layer_fy);
    band[0] = camera_x * MEASURE_SCALE + shown_pixels(sx); band[1] = camera_y * MEASURE_SCALE + shown_pixels(sy);
    link[0] = world[0] * MEASURE_SCALE - band[0] + (layer ? shown_link(layer_fx, sx) : 0);
    link[1] = world[1] * MEASURE_SCALE - band[1] + (layer ? shown_link(layer_fy, sy) : 0);
}

/* The band and Link as the screen at MEASURE_SCALE shows them, each world frame Link is drawn, transitions included:
 * the change of a step from one frame to the next is a jolt, which a band or a Link moving smoothly keeps under a
 * pixel or two of the screen.  Those where the picture switches (a transition starting or ending, a text box opening
 * or closing, Link drawn apart or no longer: the frame of the switch, the one before and the one after) are counted
 * apart from the others. */
static void measure_switch_jolts(OraclesEnhancedCheck *c, uint32_t frame, const OraclesEnhancedObservation *ob, const int32_t world[2], int32_t camera_x, int32_t camera_y)
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    int32_t sx = 0, sy = 0, layer_x = 0, layer_y = 0, layer_fx = 0, layer_fy = 0;
    uint32_t band_top = 0, band_height = 0, layer_w = 0, layer_h = 0;
    const uint32_t *layer = NULL;
    oracles_enhanced_view_subpixel(c->view, &sx, &sy, &band_top, &band_height, &layer, &layer_x, &layer_y, &layer_w, &layer_h, &layer_fx, &layer_fy);
    const int state = (ob->in_transition ? 1 : 0) | (oracles_guest_read8(c->guest, t->text_is_active) ? 2 : 0) | (layer ? 4 : 0);
    int32_t band[2], link[2];
    shown_places(c, world, camera_x, camera_y, band, link);
    const int32_t place[2] = { ob->world.link_x * 256 + ob->link_sub_x, ob->world.link_y * 256 + ob->link_sub_y };
    const int scroll_next = ob->in_scroll && ob->grid && c->scroll_back_have && c->scroll_back_frame + 1u == frame && c->scroll_back_epoch == ob->epoch;
    for (unsigned a = 0; a < 2u; a++) {
        const int32_t step = link[a] - c->scroll_back_link[a], own = place[a] - c->scroll_back_place[a];
        if (scroll_next && c->scroll_back_dip[a] && own && step && (step > 0) == (own > 0)) c->scroll_link_dips++;
        c->scroll_back_dip[a] = scroll_next && own && (step >= 3 || step <= -3) && (step > 0) != (own > 0);
    }
    c->scroll_back_have = 1; c->scroll_back_frame = frame; c->scroll_back_epoch = ob->epoch;
    memcpy(c->scroll_back_link, link, sizeof link); memcpy(c->scroll_back_place, place, sizeof place);
    /* An interior's room shown whole, or the band still in it, its edge opened by a scroll's start (a door left through
     * the edge), the game's window still, the game in that room still: the band as shown stands where it stood
     * (enhanced.alone_scroll_band_moves, of enhanced.alone_scroll_frames). */
    if (!ob->in_transition) {
        c->alone_whole = oracles_enhanced_view_room_whole(c->view);
        c->alone_play = c->alone_whole || (ob->world.alone && c->alone_have && c->alone_frame + 1u == frame && c->alone_epoch == ob->epoch
                                           && band[0] == c->alone_band[0] && band[1] == c->alone_band[1]);
        c->alone_group = ob->group; c->alone_room = ob->room;
    }
    if (c->alone_play && ob->group == c->alone_group && ob->room == c->alone_room && ob->in_scroll && ob->world.alone && ob->window_left == ob->scroll_start_left && ob->window_top == ob->scroll_start_top) {
        if (c->alone_have && c->alone_frame + 1u == frame && c->alone_epoch == ob->epoch) {
            c->alone_scroll_frames++;
            if (band[0] != c->alone_band[0] || band[1] != c->alone_band[1]) c->alone_scroll_band_moves++;
        }
    }
    c->alone_have = 1; c->alone_frame = frame; c->alone_epoch = ob->epoch; c->alone_band[0] = band[0]; c->alone_band[1] = band[1];
    /* A camera that jumps (enhanced.camera_jumps, a place the view cuts to), Link put elsewhere, start a new measure. */
    const int32_t largest = MAX_CAMERA_STEP * MEASURE_SCALE;
    int contiguous = c->jolt_have && c->jolt_frame + 1u == frame && c->jolt_epoch == ob->epoch;
    for (unsigned a = 0; a < 2u && contiguous; a++)
        contiguous = band[a] - c->jolt_band[a] <= largest && c->jolt_band[a] - band[a] <= largest && link[a] - c->jolt_link[a] <= 2 * largest && c->jolt_link[a] - link[a] <= 2 * largest;
    const int edge = contiguous && state != c->jolt_state;
    if (edge) {
        c->switch_edges++;
        c->jolt_edge_frames = 2u;
        if (c->jolt_pending) {   /* the frame before the switch was a step into it: the switch's too */
            if (c->jolt_pending_band > c->switch_band_jolt_max) c->switch_band_jolt_max = c->jolt_pending_band;
            if (c->jolt_pending_link > c->switch_link_jolt_max) c->switch_link_jolt_max = c->jolt_pending_link;
            c->jolt_pending = 0;
        }
    }
    if (c->jolt_pending) {
        if (c->jolt_pending_band > c->band_jolt_max) c->band_jolt_max = c->jolt_pending_band;
        if (c->jolt_pending_link > c->link_jolt_max) c->link_jolt_max = c->jolt_pending_link;
        c->jolt_pending = 0;
    }
    /* A band in whole pixels over the three frames (no fraction there: a cutscene, a shake, a large room) steps as
     * the camera does, as without the sub-pixel camera: its jolts are not the camera's switches. */
    c->jolt_fraction_frames = ((contiguous ? c->jolt_fraction_frames << 1 : 0u) | (sx || sy ? 1u : 0u)) & 7u;
    if (!contiguous) { c->jolt_have_step = 0; c->jolt_edge_frames = 0; }
    else {
        int32_t band_step[2], link_step[2], band_jolt = 0, link_jolt = 0;
        for (unsigned a = 0; a < 2u; a++) {
            band_step[a] = band[a] - c->jolt_band[a]; link_step[a] = link[a] - c->jolt_link[a];
            if (c->jolt_have_step) {
                const int32_t jb = band_step[a] - c->jolt_band_step[a], jl = link_step[a] - c->jolt_link_step[a];
                if ((jb < 0 ? -jb : jb) > band_jolt) band_jolt = jb < 0 ? -jb : jb;
                if ((jl < 0 ? -jl : jl) > link_jolt) link_jolt = jl < 0 ? -jl : jl;
            }
            c->jolt_band_step[a] = band_step[a]; c->jolt_link_step[a] = link_step[a];
        }
        if (!c->jolt_fraction_frames) band_jolt = 0;
        if (c->jolt_have_step && c->jolt_edge_frames) {
            if (band_jolt > c->switch_band_jolt_max) c->switch_band_jolt_max = band_jolt;
            if (link_jolt > c->switch_link_jolt_max) c->switch_link_jolt_max = link_jolt;
            const size_t used = strlen(c->jolt_list);
            if (band_jolt >= 2 && used + 24 < sizeof c->jolt_list)
                snprintf(c->jolt_list + used, sizeof c->jolt_list - used, "%s%u:%" PRId32 "/%" PRId32, used ? " " : "", frame, band_jolt, link_jolt);
        } else if (c->jolt_have_step) { c->jolt_pending = 1; c->jolt_pending_band = band_jolt; c->jolt_pending_link = link_jolt; }
        if (c->jolt_edge_frames) c->jolt_edge_frames--;
        c->jolt_have_step = 1;
    }
    int32_t ground[2];
    for (unsigned a = 0; a < 2u; a++) ground[a] = link[a] + band[a] - world[a] * MEASURE_SCALE;
    if (contiguous && (layer != NULL) != c->ground_layer && (ob->in_transition || c->ground_transition)) {
        c->transition_layer_switches++;
        for (unsigned a = 0; a < 2u; a++) {
            const int32_t shift = ground[a] > c->ground_offset[a] ? ground[a] - c->ground_offset[a] : c->ground_offset[a] - ground[a];
            if (shift > c->transition_layer_shift_max) c->transition_layer_shift_max = shift;
        }
    }
    memcpy(c->ground_offset, ground, sizeof ground);
    c->ground_layer = layer != NULL; c->ground_transition = ob->in_transition;
    memcpy(c->jolt_band, band, sizeof band); memcpy(c->jolt_link, link, sizeof link);
    c->jolt_frame = frame; c->jolt_epoch = ob->epoch; c->jolt_state = state; c->jolt_have = 1;
}

static void measure_camera_wobble(OraclesEnhancedCheck *c, uint32_t frame, const OraclesEnhancedObservation *ob, const int32_t world[2], int32_t camera_x, int32_t camera_y)
{
    const int contiguous = c->wobble_count && c->wobble_frame[(c->wobble_count - 1u) % 9u] + 1u == frame && !ob->in_transition;
    if (!contiguous) { c->wobble_count = 0; c->wobble_last_across = 0; }
    if (ob->in_transition) return;
    c->wobble_world[c->wobble_count % 9u][0] = world[0]; c->wobble_world[c->wobble_count % 9u][1] = world[1];
    c->wobble_frame[c->wobble_count % 9u] = frame;
    c->wobble_count++;
    const int camera_steps = c->wobble_camera_frame + 1u == frame;
    const int32_t dcx = camera_x - c->wobble_camera[0], dcy = camera_y - c->wobble_camera[1];
    c->wobble_camera[0] = camera_x; c->wobble_camera[1] = camera_y; c->wobble_camera_frame = frame;
    /* As shown at MEASURE_SCALE: the band at the camera plus its fraction, Link unmoved where the view draws him apart. */
    int32_t band[2], link[2];
    shown_places(c, world, camera_x, camera_y, band, link);
    const int shown_steps = c->shown_frame + 1u == frame;
    /* Each axis the band stops on between two of its steps (a jerk), an axis at a time. */
    unsigned shown_stop = 0;
    for (unsigned a = 0; a < 2u; a++) {
        const int moved = shown_steps && band[a] != c->shown_band[a];
        if (shown_steps && !moved && c->shown_band_moved[a]) shown_stop++;
        c->shown_band_moved[a] = moved;
    }
    int32_t link_step[2] = { 0, 0 }, band_step[2] = { 0, 0 };
    if (shown_steps) {
        link_step[0] = link[0] - c->shown_link[0]; link_step[1] = link[1] - c->shown_link[1];
        band_step[0] = band[0] - c->shown_band[0]; band_step[1] = band[1] - c->shown_band[1];
    }
    memcpy(c->shown_band, band, sizeof band); memcpy(c->shown_link, link, sizeof link); c->shown_frame = frame;
    if (c->wobble_count < 9u || !camera_steps) return;
    const int32_t *oldest = c->wobble_world[c->wobble_count % 9u];
    const int32_t mx = world[0] - oldest[0], my = world[1] - oldest[1];
    if (((mx == 8 || mx == -8) && my == 0) || ((my == 8 || my == -8) && mx == 0)) { c->straight_frames++; c->straight_band_stops += shown_stop; }
    int steady = (mx >= 3 || mx <= -3) && (my >= 3 || my <= -3);
    for (unsigned k = 1; k < 9u && steady; k++) {
        const int32_t *a = c->wobble_world[(c->wobble_count + k - 1u) % 9u], *b = c->wobble_world[(c->wobble_count + k) % 9u];
        if ((b[0] != a[0] && sign(b[0] - a[0]) != sign(mx)) || (b[1] != a[1] && sign(b[1] - a[1]) != sign(my))) steady = 0;
    }
    if (!steady) { c->wobble_last_across = 0; c->shown_last_across = 0; c->ground_last_across = 0; return; }
    c->diagonal_frames++;
    c->band_stops += shown_stop;
    /* The band as shown going back across the walk by 2 px of the screen or more (one is the rounding of a straight line). */
    const int32_t shown_across = band_step[0] * sign(my) - band_step[1] * sign(mx);
    if (shown_across >= 2 || shown_across <= -2) {
        if (c->shown_last_across && sign(shown_across) != c->shown_last_across) c->band_wobble++;
        c->shown_last_across = sign(shown_across);
    }
    if (shown_steps) {
        const int32_t ground_across = (link_step[0] + band_step[0]) * sign(my) - (link_step[1] + band_step[1]) * sign(mx);
        if (ground_across >= 2 || ground_across <= -2) {
            if (c->ground_last_across && sign(ground_across) != c->ground_last_across) c->link_wobble++;
            c->ground_last_across = sign(ground_across);
        }
    }
    if (shown_steps && (link_step[0] || link_step[1])) c->link_screen_moves++;
    for (unsigned a = 0; a < 2u; a++) {
        if (link_step[a] && c->shown_link_step[a] && (link_step[a] > 0) != (c->shown_link_step[a] > 0)) c->link_screen_backsteps++;
        if (link_step[a]) c->shown_link_step[a] = link_step[a];
    }
    const int across = sign(dcx * sign(my) - dcy * sign(mx));
    if (!across) return;
    if (c->wobble_last_across && across != c->wobble_last_across) c->camera_wobble++;
    c->wobble_last_across = across;
}

/* Link's sprite as the frame on screen draws it, in normal play: his tag (a
 * special object, slot 0) among the entries of the OAM that frame showed, its
 * corner placed in the band by the window's place and less the shown camera.
 * A frame where it goes back against the step it made the frame before, on
 * either axis, while his place in the world does not turn: Link shaking on
 * the screen, which a camera stepping on frames his sprite does not makes.
 * A camera catching up moves him one way at a time and is not counted.  In a
 * scroll, across its axis alone, where a carry moves him (--continuous-diagonal);
 * along it the band follows the game's own move. */
/* The special objects other than Link the sub-pixel camera's layer took, each frame, at a fraction of a pixel not
 * their own: drawn at Link's, they would move against the ground (a companion waiting, a bomb laid). */
static void measure_layer_objects(OraclesEnhancedCheck *c)
{
    const uint64_t taken = oracles_enhanced_view_subpixel_taken(c->view);
    const uint8_t *oam = oracles_guest_oam(c->guest), *link = oracles_guest_object(c->guest, 0, 0);
    if (!taken || !c->link_sprites || !oam || !link) return;
    OraclesSpriteTag tags[ORACLES_SPRITE_TAGS];
    const unsigned count = oracles_sprites_tags_for_oam(c->link_sprites, oam, tags, ORACLES_SPRITE_TAGS);
    for (unsigned i = 0; i < count; i++) {
        if (tags[i].kind != 0u || tags[i].slot == 0u) continue;
        int in_layer = 0;
        for (unsigned n = 0; n < tags[i].count && tags[i].first + n < 40u; n++) in_layer |= (taken >> (tags[i].first + n)) & 1u;
        if (!in_layer) continue;
        c->layer_objects++;
        const uint8_t *o = oracles_guest_object(c->guest, tags[i].slot, 0);
        if (!o || o[ORACLES_OBJ_X] != link[ORACLES_OBJ_X] || o[ORACLES_OBJ_Y] != link[ORACLES_OBJ_Y]) c->layer_objects_off++;
    }
}

/* Link's sprites as drawn (enhanced.link_drawn_zigzags): the top left of the OAM's entries of his graphics at his place
 * in the window drawn (his left column, or his right one 8 px left, the hidden column x 0 aside), or of those the view
 * built from his fields, in the world, then on the screen at MEASURE_SCALE as the backend moves them. */
static void measure_link_drawn(OraclesEnhancedCheck *c, uint32_t frame, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, int32_t camera_x, int32_t camera_y)
{
    const uint8_t *oam = oracles_guest_oam(c->guest);
    int32_t left = INT32_MAX, top = INT32_MAX;
    int built_x = 0, built_y = 0;
    const int shown = mode == ORACLES_ENHANCED_WORLD && ob->playing && !ob->cutscene && !(ob->in_transition && !ob->in_scroll);
    if (shown && oracles_enhanced_view_link_drawn(c->view, &built_x, &built_y) == 2) { left = built_x; top = built_y; }
    else if (shown && oam) {
        const unsigned ax = (unsigned)(ob->world.link_x - ob->window_left) & 0xffu, ay = (unsigned)(ob->world.link_y - ob->window_top + 24) & 0xffu;
        for (unsigned k = 0; k < 40u; k++) {
            const uint8_t *e = oam + k * 4u;
            if (e[0] == 0u || e[1] == 0u || (e[2] & 0xfcu) != ORACLES_LINK_TILES || !(e[3] & ORACLES_OAM_ATTR_VRAM_BANK1)) continue;
            const int dy = (int8_t)(uint8_t)(e[0] - ay), dx = (int8_t)(uint8_t)(e[1] - ax);
            if (dy < -2 || dy > 2 + (int)ORACLES_LINK_SWIM_LOWER || dx < -4 || dx > 11) continue;
            const int32_t x = (e[1] >= 208u ? (int32_t)e[1] - 256 : (int32_t)e[1]) - 8 - (dx >= 4 ? 8 : 0);
            const int32_t y = (e[0] >= 224u ? (int32_t)e[0] - 256 : (int32_t)e[0]) - 16 - (int32_t)ORACLES_ENHANCED_HUD_HEIGHT;
            if (x < left) left = x;
            if (y < top) top = y;
        }
    }
    if (!shown || left == INT32_MAX || top == INT32_MAX || (c->zig_have && (c->zig_frame + 1u != frame || c->zig_epoch != ob->epoch))) {
        c->zig_have = 0;
        if (!shown || left == INT32_MAX || top == INT32_MAX) return;
    }
    int32_t sx = 0, sy = 0, layer_x = 0, layer_y = 0, layer_fx = 0, layer_fy = 0;
    uint32_t band_top = 0, band_height = 0, layer_w = 0, layer_h = 0;
    const uint32_t *layer = NULL;
    oracles_enhanced_view_subpixel(c->view, &sx, &sy, &band_top, &band_height, &layer, &layer_x, &layer_y, &layer_w, &layer_h, &layer_fx, &layer_fy);
    const int32_t fx = shown_pixels(sx), fy = shown_pixels(sy);
    const int32_t place[2] = { (ob->window_left + left - camera_x) * MEASURE_SCALE + (layer ? (layer_fx ? shown_pixels_down(layer_fx + sx) - fx : 0) : -fx),
                               (ob->window_top + top - camera_y) * MEASURE_SCALE + (layer ? (layer_fy ? shown_pixels_down(layer_fy + sy) - fy : 0) : -fy) };
    const int32_t world[2] = { ob->world.link_x - ob->link_step_x, ob->world.link_y - ob->link_step_y };
    for (unsigned a = 0; a < 2u && c->zig_have; a++) {
        const int32_t step = place[a] - c->zig_place[a], world_step = world[a] - c->zig_world[a], last = c->zig_step[a];
        const int turned = world_step && c->zig_world_step[a] && (world_step > 0) != (c->zig_world_step[a] > 0);
        if (!turned && (last >= 2 || last <= -2) && (step >= 2 || step <= -2) && (step > 0) != (last > 0) && step < 16 && step > -16 && last < 16 && last > -16) {
            c->link_drawn_zigzags++;
            const size_t used = strlen(c->zig_list);
            if (used + 16 < sizeof c->zig_list) snprintf(c->zig_list + used, sizeof c->zig_list - used, "%s%u:%c", used ? " " : "", frame, a ? 'y' : 'x');
        }
    }
    for (unsigned a = 0; a < 2u; a++) {
        c->zig_step[a] = c->zig_have ? place[a] - c->zig_place[a] : 0;
        c->zig_world_step[a] = c->zig_have ? world[a] - c->zig_world[a] : 0;
    }
    memcpy(c->zig_place, place, sizeof place); memcpy(c->zig_world, world, sizeof world);
    c->zig_have = 1; c->zig_frame = frame; c->zig_epoch = ob->epoch;
}

void ec_measure_link_backsteps(OraclesEnhancedCheck *c, uint32_t frame, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, int32_t camera_x, int32_t camera_y)
{
    measure_link_drawn(c, frame, ob, mode, camera_x, camera_y);
    measure_layer_objects(c);
    int found = 0, left = 255, top = 255;
    const uint8_t *oam = oracles_guest_oam(c->guest);
    if (c->link_sprites && oam && mode == ORACLES_ENHANCED_WORLD && ob->playing && !ob->cutscene) {
        OraclesSpriteTag tags[ORACLES_SPRITE_TAGS];
        const unsigned count = oracles_sprites_tags_for_oam(c->link_sprites, oam, tags, ORACLES_SPRITE_TAGS);
        for (unsigned i = 0; i < count; i++) {
            if (tags[i].kind != 0u || tags[i].slot != 0u || tags[i].terrain_effect) continue;
            for (unsigned n = 0; n < tags[i].count && tags[i].first + n < 40u; n++) {
                const uint8_t *e = oam + (tags[i].first + n) * 4u;
                if (e[0] == 0u || e[0] >= 160u || e[1] == 0u || e[1] >= 168u) continue;   /* off the screen: not drawn */
                if (e[0] < top) top = e[0];
                if (e[1] < left) left = e[1];
                found = 1;
            }
        }
    }
    const int32_t world[2] = { ob->window_left + left, ob->window_top + top + oracles_enhanced_view_link_phase_dy(c->view) };
    /* His place as the frame on screen draws it, from his position rather than his sprites, which lose a column at the
     * screen's edges. */
    const int32_t drawn[2] = { ob->world.link_x - ob->link_step_x, ob->world.link_y - ob->link_step_y + oracles_enhanced_view_link_phase_dy(c->view) };
    if (mode == ORACLES_ENHANCED_WORLD) measure_switch_jolts(c, frame, ob, drawn, camera_x, camera_y);
    if (ob->in_transition && !ob->in_scroll) found = 0;
    if (!found || (c->have_drawn_link && ob->epoch != c->drawn_link_epoch)) { c->have_drawn_link = 0; if (!found) return; }
    measure_camera_wobble(c, frame, ob, world, camera_x, camera_y);
    const int32_t screen[2] = { world[0] - camera_x, world[1] - camera_y };
    if (c->have_drawn_link) {
        c->link_drawn_steps++;
        for (unsigned a = 0; a < 2u; a++) {
            const int32_t step = screen[a] - c->drawn_screen[a], world_step = world[a] - c->drawn_world[a];
            const int back = step && c->drawn_screen_step[a] && (step > 0) != (c->drawn_screen_step[a] > 0);
            const int turned = world_step && c->drawn_world_step[a] && (world_step > 0) != (c->drawn_world_step[a] > 0);
            if (ob->in_scroll && a != ((ob->scroll_direction & 1u) ? 1u : 0u)) { c->drawn_screen_step[a] = 0; c->drawn_world_step[a] = 0; continue; }
            if (back && !turned) {
                if (a) c->link_backsteps_y++; else c->link_backsteps++;
                const size_t used = strlen(c->link_backsteps_list);
                if (used + 16 < sizeof c->link_backsteps_list)
                    snprintf(c->link_backsteps_list + used, sizeof c->link_backsteps_list - used, "%s%u:%c", used ? " " : "", frame, a ? 'y' : 'x');
            }
            c->drawn_screen_step[a] = step; c->drawn_world_step[a] = world_step;
        }
    } else {
        c->drawn_screen_step[0] = c->drawn_screen_step[1] = 0;
        c->drawn_world_step[0] = c->drawn_world_step[1] = 0;
    }
    memcpy(c->drawn_screen, screen, sizeof screen);
    memcpy(c->drawn_world, world, sizeof world);
    c->drawn_link_epoch = ob->epoch;
    c->have_drawn_link = 1;
}

#define SWIM_AFTER_SCROLL 3u   /* the frames after a transition counted with it */

void ec_measure_swim(OraclesEnhancedCheck *c, uint32_t frame, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, int32_t camera_x, int32_t camera_y)
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    const uint8_t *link = oracles_guest_object(c->guest, 0, 0);
    if (!link) { c->swim_have = 0; return; }
    const uint8_t state = oracles_guest_read8(c->guest, t->link_swimming_state);
    const int floor = (oracles_guest_read8(c->guest, t->tileset_flags) & ORACLES_TILESETFLAG_UNDERWATER) != 0;
    const int swimming = (state & 0x4fu) == ORACLES_SWIMMING_NORMAL;
    if ((c->swim_last_state & 0x4fu) == ORACLES_SWIMMING_NORMAL && !state && !floor && !ob->large && ob->playing
        && (link[ORACLES_OBJ_YH] + 5u >= 128u || link[ORACLES_OBJ_XH] >= 160u)) c->swim_left_water_past_room++;
    c->swim_last_state = state;
    if (!ob->in_transition) c->swim_scroll_swimming = swimming;
    else if (c->swim_scroll_swimming && !floor && oracles_guest_read8(c->guest, t->link_anim_mode) == ORACLES_LINK_ANIM_MODE_WALK) c->swim_walked_scroll_frames++;
    const int window = ob->in_transition || c->swim_after;
    c->swim_after = ob->in_transition ? SWIM_AFTER_SCROLL : c->swim_after ? c->swim_after - 1u : 0u;
    int x = 0, y = 0;
    const int drawn = oracles_enhanced_view_link_drawn(c->view, &x, &y);
    const int measured = mode == ORACLES_ENHANCED_WORLD && ob->playing && !ob->cutscene && (swimming || floor);
    /* The same on foot (walk.drawn_*), measured apart; not while wScrollMode is 0, a transition that does not scroll,
     * where the world holds him at the room's edge while he walks on past it. */
    const int walking = mode == ORACLES_ENHANCED_WORLD && ob->playing && !ob->cutscene && !state && !floor
                     && oracles_guest_read8(c->guest, t->link_anim_mode) == ORACLES_LINK_ANIM_MODE_WALK && oracles_guest_read8(c->guest, t->scroll_mode) != 0u;
    const int kind = measured ? 1 : walking ? 2 : 0;
    const int contiguous = c->swim_have && c->swim_frame + 1u == frame && c->swim_epoch == ob->epoch;
    const uint16_t pose = (uint16_t)(link[ORACLES_OBJ_OAM_DATA_ADDRESS] | link[ORACLES_OBJ_OAM_DATA_ADDRESS + 1] << 8);
    if (kind && contiguous && drawn) {
        const int32_t off[2] = { ob->window_left + x - c->swim_last_world[0], ob->window_top + y - c->swim_last_world[1] };
        /* The pose the OAM draws is the frame before's: the same as the one before it, its sprites' offsets too. */
        if (c->swim_off_have == kind && c->swim_pose[0] == c->swim_pose[1]) {
            int32_t change = 0;
            for (unsigned a = 0; a < 2u; a++) { const int32_t d = off[a] - c->swim_off[a]; if ((d < 0 ? -d : d) > change) change = d < 0 ? -d : d; }
            OraclesEnhancedDrawnCounts *n = kind == 1 ? &c->swim_drawn : &c->walk_drawn;
            if (drawn == 2 || c->swim_last_drawn == 2) n->off_built_frames += change >= 1;
            else if (window) { n->scroll_frames++; n->off_scroll_frames += change >= 1; }
            else { n->mid_frames++; n->off_mid_frames += change >= 1; }
            if (change > n->off_max) n->off_max = change;
        }
        memcpy(c->swim_off, off, sizeof off);
        c->swim_off_have = kind;
    } else c->swim_off_have = 0;
    /* The band as the screen shows it, its step against the step before. */
    int32_t band[2], shown_link[2];
    const int32_t world[2] = { ob->world.link_x, ob->world.link_y };
    shown_places(c, world, camera_x, camera_y, band, shown_link);
    if (measured && contiguous && c->smooth && c->swim_band_have) {
        for (unsigned a = 0; a < 2u; a++) {
            const int32_t step = band[a] - c->swim_band[a], jerk = step - c->swim_band_step[a];
            if (c->swim_band_have > 1 && (jerk >= 3 || jerk <= -3)) { if (window) c->swim_band_jerks_scroll++; else c->swim_band_jerks_mid++; }
            c->swim_band_step[a] = step;
        }
        c->swim_band_have = 2;
    } else c->swim_band_have = measured ? 1 : 0;
    memcpy(c->swim_band, band, sizeof band);
    c->swim_pose[1] = c->swim_pose[0]; c->swim_pose[0] = pose;
    c->swim_last_drawn = drawn;
    c->swim_last_world[0] = world[0]; c->swim_last_world[1] = world[1];
    c->swim_have = 1; c->swim_frame = frame; c->swim_epoch = ob->epoch;
}

/* The scroll registers in effect at the first game-area line of the frame
 * just drawn (the journal; the vblank writes first, then the scan's up to
 * line 16); -1 when the journal has none. */
void ec_replay_scroll_registers(OraclesEnhancedCheck *c, int *scx, int *scy)
{
    size_t count = 0;
    const OraclesGuestRegWrite *j = oracles_guest_journal(c->guest, &count);
    *scx = -1; *scy = -1;
    for (size_t i = 0; i < count; i++) if (j[i].ly >= 144u) { if (j[i].reg == 0x43u) *scx = j[i].value; if (j[i].reg == 0x42u) *scy = j[i].value; }
    for (unsigned ly = 0; ly <= 16u; ly++)
        for (size_t i = 0; i < count; i++) {
            if (j[i].ly >= 144u) continue;
            const unsigned eff = j[i].stat_mode == 0 ? (unsigned)j[i].ly + 1u : j[i].ly;
            if (eff != ly) continue;
            if (j[i].reg == 0x43u) *scx = j[i].value;
            if (j[i].reg == 0x42u) *scy = j[i].value;
        }
}

/* The window against the drawn registers: SCX = camera + wScreenOffsetX,
 * SCY = camera + wScreenOffsetY - 16, the offsets like the camera those of
 * the previous frame's logic.  A room load draws with registers of its own
 * (the reveal), which the view tolerates: judged in play only; a rippled
 * frame places its window per line: the single check does not apply. */
void ec_check_window_place(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, int rippled, int scx, int scy, uint32_t frame)
{
    const int room_load = ob->in_transition && !ob->in_scroll;
    if (mode == ORACLES_ENHANCED_WORLD && !room_load && !rippled && scx >= 0 && scy >= 0 && c->have_last_offset) {
        const unsigned ex = (unsigned)(scx - ob->drawn_camera_x - c->last_offset_x) & 0xffu;
        const unsigned ey = (unsigned)(scy - ob->drawn_camera_y - c->last_offset_y + 16) & 0xffu;
        c->window_checked++;
        if (ex || ey) {
            c->window_misplaced++;
            const size_t used = strlen(c->window_misplaced_list);
            if (used + 16 < sizeof c->window_misplaced_list) snprintf(c->window_misplaced_list + used, sizeof c->window_misplaced_list - used, "%s%u:%u,%u", used ? " " : "", frame, ex, ey);
        }
    }
}
