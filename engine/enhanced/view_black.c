/* The black the band shows: what the harness holds under a
 * ceiling (enhanced.black_pixels_mean) and what the launcher says at the end
 * of a session, counted once, here, so that a played session
 * measures what the player sees on the same terms as the replays. */
#include "view_internal.h"

/* A run of black at a room's place ends at `frame`: kept when past the
 * threshold, among the longest, longest first. */
static void keep_run(OraclesEnhancedBlack *b, uint8_t group, uint8_t room, uint32_t since, uint32_t frame)
{
    const uint32_t frames = frame - since;
    if (frames <= ORACLES_ENHANCED_BLACK_ROOM_FRAMES) return;
    const unsigned kept = b->rooms_over < ORACLES_ENHANCED_BLACK_ROOMS_KEPT ? b->rooms_over : ORACLES_ENHANCED_BLACK_ROOMS_KEPT;
    b->rooms_over++;
    unsigned at = kept;
    while (at > 0 && b->longest[at - 1u].frames < frames) at--;
    if (at >= ORACLES_ENHANCED_BLACK_ROOMS_KEPT) return;
    for (unsigned i = kept < ORACLES_ENHANCED_BLACK_ROOMS_KEPT ? kept : ORACLES_ENHANCED_BLACK_ROOMS_KEPT - 1u; i > at; i--) b->longest[i] = b->longest[i - 1u];
    b->longest[at] = (OraclesEnhancedBlackRoom){ group, room, since, frames };
}

static void close_run(OraclesEnhancedView *v, unsigned i, uint32_t frame)
{
    keep_run(&v->black, v->black_runs[i].group, v->black_runs[i].room, v->black_runs[i].since, frame);
    v->black_runs[i] = v->black_runs[--v->black_run_count];
}

/* The rooms of the overworld's grid the band shows, the room in play aside,
 * where the composition drew no terrain: a whole room black.  Not where the band
 * shows no grid (a large room, a room shown alone, the zone the game routes
 * itself). */
static unsigned black_places(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y, uint8_t *rooms, unsigned capacity)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    if (!ev_on_overworld(v) || ob->large_grid || ev_isolated_room(v) || ev_in_routed_zone(v)) return 0;
    const int32_t h = (int32_t)ORACLES_GHOST_AREA_HEIGHT;
    const int32_t col0 = camera_x / SMALL_ROOM_W - 1, row0 = camera_y / h - 1;
    unsigned count = 0;
    for (int32_t row = row0; row <= row0 + 4; row++)
        for (int32_t col = col0; col <= col0 + 5; col++) {
            if (row < 0 || col < 0 || (unsigned)col >= ev_map_width(v, v->observer.ref_group) || (unsigned)row >= ev_map_height(v, v->observer.ref_group)) continue;
            const uint8_t room = (uint8_t)(row * 16 + col);
            if (ev_off_map(v, v->observer.ref_group, room)) continue;   /* never drawn on the map */
            if (room == v->observer.ref_room || !ev_shown_meets(v, camera_x, camera_y, col * SMALL_ROOM_W, row * h, SMALL_ROOM_W, h, 0)) continue;
            if (v->drawn_places[room >> 5u] & (1u << (room & 31u))) continue;   /* what the composition drew there, whichever entry */
            if (count < capacity) rooms[count++] = room;
        }
    return count;
}

void ev_measure_black(OraclesEnhancedView *v, OraclesEnhancedMode mode, int32_t camera_x, int32_t camera_y)
{
    OraclesEnhancedBlack *b = &v->black;
    const uint32_t frame = v->frame - 1u;   /* the frame composed, numbered as the route's */
    if (mode != ORACLES_ENHANCED_WORLD) {
        while (v->black_run_count) close_run(v, 0, frame);
        return;
    }
    if (!b->have_world) { b->have_world = 1; b->first_world = frame; }
    if (!b->have_full && v->uncovered == 0) { b->have_full = 1; b->first_full = frame; }
    if (b->have_full) {
        b->full_world_frames++;
        b->pixels += v->uncovered;
        if (v->uncovered > b->max) b->max = v->uncovered;
    }
    if (v->observation.in_transition) return;   /* the runs neither grow nor end across a transition */
    uint8_t rooms[32];
    const unsigned count = black_places(v, camera_x, camera_y, rooms, 32u);
    for (unsigned i = 0; i < v->black_run_count;) {
        int still = 0;
        for (unsigned k = 0; k < count; k++) still |= rooms[k] == v->black_runs[i].room && v->black_runs[i].group == v->observer.ref_group;
        if (still) i++; else close_run(v, i, frame);
    }
    for (unsigned k = 0; k < count; k++) {
        int known = 0;
        for (unsigned i = 0; i < v->black_run_count; i++) known |= v->black_runs[i].room == rooms[k] && v->black_runs[i].group == v->observer.ref_group;
        if (known || v->black_run_count == sizeof v->black_runs / sizeof v->black_runs[0]) continue;
        v->black_runs[v->black_run_count].group = v->observer.ref_group;
        v->black_runs[v->black_run_count].room = rooms[k];
        v->black_runs[v->black_run_count++].since = frame;
    }
}

/* The figures so far, the runs still going counted as they stand. */
void oracles_enhanced_view_black(const OraclesEnhancedView *v, OraclesEnhancedBlack *out)
{
    *out = v->black;
    for (unsigned i = 0; i < v->black_run_count; i++) keep_run(out, v->black_runs[i].group, v->black_runs[i].room, v->black_runs[i].since, v->frame);
}
