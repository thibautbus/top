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
    if (oracles_enhanced_view_load_state(c->view, c->saved_view, c->saved_view_size) != 0) return 0;
    return 1;
}

void ec_reload_step(OraclesEnhancedCheck *c, uint32_t frame, int32_t camera_x, const int shown[4], const uint8_t room[4])
{
    if (!c->reload_at) return;
    if (frame == c->reload_at && !c->reload_saved) {
        if (!reload_save_state(c, camera_x, shown, room)) return;
        return;
    }
    if (c->reload_saved && !c->reload_done && frame == c->reload_at + 120u) {
        if (!reload_load_state(c, shown, room)) return;
        c->reload_done = 1;
        c->reload_frame = frame;
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
