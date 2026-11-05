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

/* The grid the band shows: the overworld's (1), not where the band shows a
 * large room, a room alone or the zone the game routes itself, or Ages' open
 * sea under its surface (2), which the drawn-back band shows as a map; 0 none. */
enum { GRID_NONE, GRID_OVERWORLD, GRID_SEA };
static int grid_shown(const OraclesEnhancedView *v)
{
    if (v->observation.large_grid || ev_isolated_room(v)) return GRID_NONE;
    if (ev_on_overworld(v)) return ev_in_routed_zone(v) ? GRID_NONE : GRID_OVERWORLD;
    return ev_on_map(v) && ev_open_water(v, v->observer.ref_group, v->observer.ref_room) ? GRID_SEA : GRID_NONE;
}

/* Whether a place of that grid is a room drawn there: on the map, or of the sea. */
static int grid_room(const OraclesEnhancedView *v, int grid, int32_t col, int32_t row)
{
    const uint8_t group = v->observer.ref_group;
    const unsigned width = grid == GRID_SEA ? 16u : ev_map_width(v, group), height = grid == GRID_SEA ? 16u : ev_map_height(v, group);
    if (row < 0 || col < 0 || (unsigned)col >= width || (unsigned)row >= height) return 0;
    const uint8_t room = (uint8_t)(row * 16 + col);
    return grid == GRID_SEA ? ev_open_water(v, group, room) : !ev_off_map(v, group, room);
}

/* The part of the band shown, in the world. */
static void shown_rect(const OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y, int32_t *left, int32_t *top)
{
    *left = camera_x + (int32_t)(v->size.width - v->shown_width) / 2;
    *top = camera_y + (int32_t)(v->band_height - v->shown_height) / 2;
}

/* The rooms of the overworld's grid (`sea`: or of the open sea's) the band
 * shows, the room in play aside, where the composition drew no terrain: a
 * whole room black, but for a room whose part shown is under the game's
 * window (a scroll's). */
static unsigned black_places(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y, uint8_t *rooms, unsigned capacity, int sea)
{
    const int grid = grid_shown(v);
    if (grid == GRID_NONE || (grid == GRID_SEA && !sea)) return 0;
    const int32_t h = (int32_t)ORACLES_GHOST_AREA_HEIGHT;
    const int32_t col0 = camera_x / SMALL_ROOM_W - 1, row0 = camera_y / h - 1;
    int32_t shown_left, shown_top;
    shown_rect(v, camera_x, camera_y, &shown_left, &shown_top);
    const int32_t shown_right = shown_left + (int32_t)v->shown_width, shown_bottom = shown_top + (int32_t)v->shown_height;
    const int32_t window_left = v->observation.window_left, window_top = v->observation.window_top;
    unsigned count = 0;
    for (int32_t row = row0; row <= row0 + 4; row++)
        for (int32_t col = col0; col <= col0 + 5; col++) {
            if (!grid_room(v, grid, col, row)) continue;   /* never drawn there */
            const uint8_t room = (uint8_t)(row * 16 + col);
            if (room == v->observer.ref_room || !ev_shown_meets(v, camera_x, camera_y, col * SMALL_ROOM_W, row * h, SMALL_ROOM_W, h, 0)) continue;
            if (v->drawn_places[room >> 5u] & (1u << (room & 31u))) continue;   /* what the composition drew there, whichever entry */
            const int32_t l = col * SMALL_ROOM_W > shown_left ? col * SMALL_ROOM_W : shown_left, t = row * h > shown_top ? row * h : shown_top;
            const int32_t r = (col + 1) * SMALL_ROOM_W < shown_right ? (col + 1) * SMALL_ROOM_W : shown_right;
            const int32_t b = (row + 1) * h < shown_bottom ? (row + 1) * h : shown_bottom;
            if (l >= window_left && t >= window_top && r <= window_left + SMALL_ROOM_W && b <= window_top + h) continue;
            if (count < capacity) rooms[count++] = room;
        }
    return count;
}

static int32_t floor_div(int32_t a, int32_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

/* A room of the grid beside the band whose terrain a ghost is running or the thread's will run, black for a few frames
 * more if shown; or, a corner's in a transition (`corner`), one whose terrain is not there yet. */
static int coming(OraclesEnhancedView *v, int grid, int32_t col, int32_t row, int corner)
{
    if (!grid_room(v, grid, col, row)) return 0;
    const uint8_t room = (uint8_t)(row * 16 + col);
    const entry *e = room == v->observer.ref_room ? NULL : ev_find_entry(v, v->observer.ref_group, room);
    const int scroll = corner && v->observation.in_transition;
    return room != v->observer.ref_room && (e ? !ev_entry_drawable(v, e) && (e->in_flight || e->queued || scroll) : scroll);
}

void ev_dark_limits(OraclesEnhancedView *v, int32_t limits[4])
{
    limits[0] = INT32_MIN; limits[1] = INT32_MAX; limits[2] = INT32_MIN; limits[3] = INT32_MAX;
    if (!v->ghost || !v->have_view) return;
    /* Each side whose edge the band showed black: the edge held there, to show no more of it. */
    if (v->uncovered_edges & 1u) limits[0] = v->view_left;
    if (v->uncovered_edges & 2u) limits[1] = v->view_left;
    if (v->uncovered_edges & 4u) limits[2] = v->view_top;
    if (v->uncovered_edges & 8u) limits[3] = v->view_top;
    const int grid = grid_shown(v);
    if (grid == GRID_NONE) return;
    const int32_t h = (int32_t)ORACLES_GHOST_AREA_HEIGHT;
    const int32_t inset_x = (int32_t)(v->size.width - v->shown_width) / 2, inset_y = (int32_t)(v->band_height - v->shown_height) / 2;
    int32_t left, top;
    shown_rect(v, v->view_left, v->view_top, &left, &top);
    const int32_t right = left + (int32_t)v->shown_width, bottom = top + (int32_t)v->shown_height;
    const int32_t col0 = floor_div(left, SMALL_ROOM_W), col1 = floor_div(right - 1, SMALL_ROOM_W);
    const int32_t row0 = floor_div(top, h), row1 = floor_div(bottom - 1, h);
    /* Each side beyond which a room is on its way, the corners included: the edge no further than that room's. */
    int right_coming = 0, left_coming = 0, below_coming = 0, above_coming = 0;
    for (int32_t row = row0; row <= row1; row++) {
        right_coming |= coming(v, grid, col1 + 1, row, 0);
        left_coming |= coming(v, grid, col0 - 1, row, 0);
    }
    for (int32_t col = col0; col <= col1; col++) {
        below_coming |= coming(v, grid, col, row1 + 1, 0);
        above_coming |= coming(v, grid, col, row0 - 1, 0);
    }
    /* A corner room on its way holds both sides it lies past. */
    for (int dc = 0; dc < 2; dc++)
        for (int dr = 0; dr < 2; dr++)
            if (coming(v, grid, dc ? col1 + 1 : col0 - 1, dr ? row1 + 1 : row0 - 1, 1)) {
                if (dc) right_coming = 1; else left_coming = 1;
                if (dr) below_coming = 1; else above_coming = 1;
            }
    if (right_coming && limits[1] == INT32_MAX) limits[1] = (col1 + 1) * SMALL_ROOM_W - (int32_t)v->shown_width - inset_x;
    if (left_coming && limits[0] == INT32_MIN) limits[0] = col0 * SMALL_ROOM_W - inset_x;
    if (below_coming && limits[3] == INT32_MAX) limits[3] = (row1 + 1) * h - (int32_t)v->shown_height - inset_y;
    if (above_coming && limits[2] == INT32_MIN) limits[2] = row0 * h - inset_y;
}

/* Whether the band shows a place of no room of its grid: past the map's
 * edge, a place the map never draws, the sea's shore. */
static int past_edge(const OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y)
{
    const int grid = grid_shown(v);
    if (grid == GRID_NONE) return 0;
    const int32_t h = (int32_t)ORACLES_GHOST_AREA_HEIGHT;
    int32_t left, top;
    shown_rect(v, camera_x, camera_y, &left, &top);
    const int32_t right = left + (int32_t)v->shown_width, bottom = top + (int32_t)v->shown_height;
    if (left < 0 || top < 0) return 1;
    for (int32_t row = top / h; row <= (bottom - 1) / h; row++)
        for (int32_t col = left / SMALL_ROOM_W; col <= (right - 1) / SMALL_ROOM_W; col++)
            if (!grid_room(v, grid, col, row)) return 1;
    return 0;
}

/* An episode of a kind ends at `frame`, its last frame the one before. */
static void end_episode(OraclesEnhancedBlack *b, unsigned kind, uint32_t since, uint32_t frame, int in_transition)
{
    OraclesEnhancedBlackKind *k = &b->kinds[kind];
    k->episodes++;
    if (frame - since > k->longest) k->longest = frame - since;
    if (in_transition) k->in_transition++;
}

static void end_room_episode(OraclesEnhancedView *v, unsigned i, uint32_t frame)
{
    const unsigned kind = v->black_episodes[i].kind;
    end_episode(&v->black, kind, v->black_episodes[i].since, frame, v->black_episodes[i].in_transition);
    const size_t used = strlen(v->black_log);
    if (kind != ORACLES_ENHANCED_BLACK_COLD && used + 24u < sizeof v->black_log)
        snprintf(v->black_log + used, sizeof v->black_log - used, "%s%c %u:%02x %u+%u%s", used ? ", " : "", kind == ORACLES_ENHANCED_BLACK_CORNER ? 'c' : 'n',
                 v->black_episodes[i].group, v->black_episodes[i].room, v->black_episodes[i].since, frame - v->black_episodes[i].since,
                 v->black_episodes[i].in_transition ? "t" : "");
    v->black_episodes[i] = v->black_episodes[--v->black_episode_count];
}

/* The episodes by kind, a scroll's frames included: the rooms black this
 * frame (none in a frame not of the world), the band past the map's edge,
 * and what is black besides, a ripple's fringe or else. */
static void count_kinds(OraclesEnhancedView *v, int world, int32_t camera_x, int32_t camera_y, uint32_t frame)
{
    if (world) {
        /* The band fills from nothing at the first world frame, after a load
         * and after a warp: the room in play not beside the one before. */
        const uint8_t group = v->observer.ref_group, room = v->observer.ref_room;
        const int dc = (int)(room & 0x0fu) - (int)(v->black_ref_room & 0x0fu), dr = (int)(room >> 4u) - (int)(v->black_ref_room >> 4u);
        if (!v->black_have_ref || v->black_loaded || group != v->black_ref_group || dc < -1 || dc > 1 || dr < -1 || dr > 1) v->black_cold_from = frame;
        v->black_have_ref = 1; v->black_loaded = 0;
        v->black_ref_group = group; v->black_ref_room = room;
    }
    uint8_t rooms[32];
    const unsigned count = world && v->uncovered ? black_places(v, camera_x, camera_y, rooms, 32u, 1) : 0u;
    const int transition = v->observation.in_transition;
    for (unsigned i = 0; i < v->black_episode_count;) {
        int still = 0;
        for (unsigned k = 0; k < count; k++) still |= rooms[k] == v->black_episodes[i].room && v->black_episodes[i].group == v->observer.ref_group;
        if (still) i++; else end_room_episode(v, i, frame);
    }
    unsigned present = 0;
    for (unsigned k = 0; k < count; k++) {
        unsigned i = 0;
        while (i < v->black_episode_count && !(v->black_episodes[i].room == rooms[k] && v->black_episodes[i].group == v->observer.ref_group)) i++;
        if (i == sizeof v->black_episodes / sizeof v->black_episodes[0]) continue;
        if (i == v->black_episode_count) {
            const int dc = (int)(rooms[k] & 0x0fu) - (int)(v->observer.ref_room & 0x0fu), dr = (int)(rooms[k] >> 4u) - (int)(v->observer.ref_room >> 4u);
            v->black_episodes[i].group = v->observer.ref_group;
            v->black_episodes[i].room = rooms[k];
            v->black_episodes[i].kind = (uint8_t)(frame - v->black_cold_from < ORACLES_ENHANCED_BLACK_COLD_FRAMES ? ORACLES_ENHANCED_BLACK_COLD
                                                  : (dc == 2 || dc == -2) && (dr == 2 || dr == -2) ? ORACLES_ENHANCED_BLACK_CORNER : ORACLES_ENHANCED_BLACK_NEIGHBOUR);
            v->black_episodes[i].in_transition = (uint8_t)transition;
            v->black_episodes[i].since = frame;
            v->black_episode_count++;
        }
        present |= 1u << v->black_episodes[i].kind;
    }
    const int edge = world && v->uncovered && past_edge(v, camera_x, camera_y);
    const int ripple = world && v->uncovered && !count && !edge && v->have_wave;
    const int alone = world && v->uncovered && !count && !edge && !ripple && ev_isolated_room(v);
    const int places[4] = { edge, ripple, alone, world && v->uncovered && !count && !edge && !ripple && !alone };
    for (unsigned p = 0; p < 4u; p++) {
        const unsigned kind = ORACLES_ENHANCED_BLACK_EDGE + p;
        if (places[p]) {
            present |= 1u << kind;
            if (!v->black_place_since[p]) { v->black_place_since[p] = frame + 1u; v->black_place_transition[p] = (uint8_t)transition; }
        } else if (v->black_place_since[p]) {
            end_episode(&v->black, kind, v->black_place_since[p] - 1u, frame, v->black_place_transition[p]);
            v->black_place_since[p] = 0;
        }
    }
    for (unsigned kind = 0; kind < ORACLES_ENHANCED_BLACK_KINDS; kind++) if (present & (1u << kind)) v->black.kinds[kind].frames++;
}

void ev_measure_black(OraclesEnhancedView *v, OraclesEnhancedMode mode, int32_t camera_x, int32_t camera_y)
{
    OraclesEnhancedBlack *b = &v->black;
    const uint32_t frame = v->frame - 1u;   /* the frame composed, numbered as the route's */
    count_kinds(v, mode == ORACLES_ENHANCED_WORLD, camera_x, camera_y, frame);
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
    const unsigned count = black_places(v, camera_x, camera_y, rooms, 32u, 0);
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

/* The figures so far, the runs and the episodes still going counted as they stand. */
void oracles_enhanced_view_black(const OraclesEnhancedView *v, OraclesEnhancedBlack *out)
{
    *out = v->black;
    for (unsigned i = 0; i < v->black_run_count; i++) keep_run(out, v->black_runs[i].group, v->black_runs[i].room, v->black_runs[i].since, v->frame);
    for (unsigned i = 0; i < v->black_episode_count; i++) end_episode(out, v->black_episodes[i].kind, v->black_episodes[i].since, v->frame, v->black_episodes[i].in_transition);
    for (unsigned p = 0; p < 4u; p++)
        if (v->black_place_since[p]) end_episode(out, ORACLES_ENHANCED_BLACK_EDGE + p, v->black_place_since[p] - 1u, v->frame, v->black_place_transition[p]);
}

const char *oracles_enhanced_black_kind_name(unsigned kind)
{
    static const char *const names[ORACLES_ENHANCED_BLACK_KINDS] = { "corner", "neighbour", "cold", "edge", "ripple", "alone", "other" };
    return kind < ORACLES_ENHANCED_BLACK_KINDS ? names[kind] : "?";
}

const char *oracles_enhanced_view_black_log(const OraclesEnhancedView *v) { return v->black_log; }

/* ---- the rooms never visited ----------------------------------------------------------- */

/* The game's own record of the rooms Link has entered, the one its map draws from: the visited bit of each room's
 * flags (setVisitedRoomFlag, at the load of a scroll's room and at the first frame of play after any other load).
 * The reference room and the active one, the room a scroll enters, are shown whatever it says, and so is a room that
 * was the reference in this epoch: the game's own cutscenes load their rooms without marking them. */
int ev_room_hidden(OraclesEnhancedView *v, uint8_t group, uint8_t room)
{
    if (!v->unvisited_hidden) return 0;
    if (group == v->observer.ref_group && room == v->observer.ref_room) return 0;
    if (group == v->observation.group && room == v->observation.room) return 0;
    const int was_reference = v->have_hidden_places && group == v->hidden_group && (v->referenced[room >> 5u] & (1u << (room & 31u)));
    const int unmarked = !(ev_live_byte(v, ev_room_flags_address(v, group, room)) & oracles_guest_tables(v->guest)->roomflag_visited);
    return unmarked && !was_reference;
}

unsigned oracles_enhanced_reveal_veil(uint32_t age)
{
    return age >= ORACLES_ENHANCED_REVEAL_FRAMES ? 0u : 256u - age * 256u / ORACLES_ENHANCED_REVEAL_FRAMES;
}

void ev_hidden_forget(OraclesEnhancedView *v)
{
    v->have_hidden_places = 0;
    v->reveal_count = 0;
    v->edge_fade_count = 0;
    v->reference_settled = 0;
}

/* A new epoch (a warp, a time travel, a load) or another group shows its rooms as they are, none fading in. */
void ev_hidden_begin(OraclesEnhancedView *v)
{
    v->hidden_count = 0;
    memset(v->hidden_now, 0, sizeof v->hidden_now);
    memset(v->shown_now, 0, sizeof v->shown_now);
    memset(v->bare_now, 0, sizeof v->bare_now);
    if (!v->unvisited_hidden) return;
    if (!v->have_hidden_places || v->hidden_epoch != v->observation.epoch || v->hidden_group != v->observer.ref_group) {
        memset(v->hidden_places, 0, sizeof v->hidden_places);
        memset(v->referenced, 0, sizeof v->referenced);
        memset(v->bare_places, 0, sizeof v->bare_places);
        v->reveal_count = 0;
        v->edge_fade_count = 0;
    }
    v->have_hidden_places = 1;
    v->hidden_epoch = v->observation.epoch;
    v->hidden_group = v->observer.ref_group;
    v->referenced[v->observer.ref_room >> 5u] |= 1u << (v->observer.ref_room & 31u);
}

/* A room of the reference's group: kept dark, or fading in from the frame it is shown after a composition that kept
 * it dark. */
unsigned ev_room_veil(OraclesEnhancedView *v, uint8_t room)
{
    if (!v->unvisited_hidden) return 0;
    const uint32_t bit = 1u << (room & 31u);
    if (ev_room_hidden(v, v->observer.ref_group, room)) {
        if (!(v->hidden_now[room >> 5u] & bit)) v->hidden_count++;
        v->hidden_now[room >> 5u] |= bit;
        return 256u;
    }
    v->shown_now[room >> 5u] |= bit;
    unsigned i = 0;
    while (i < v->reveal_count && v->reveals[i].room != room) i++;
    if (i == v->reveal_count) {
        if (!(v->hidden_places[room >> 5u] & bit) || v->reveal_count == sizeof v->reveals / sizeof v->reveals[0]) return 0;
        v->reveals[v->reveal_count].room = room;
        v->reveals[v->reveal_count++].since = v->frame;
        v->hidden_places[room >> 5u] &= ~bit;
        v->reveal_fades++;
    }
    return oracles_enhanced_reveal_veil(v->frame - v->reveals[i].since + 1u);
}

/* A room kept dark drawn with its pixels: its edge, coming in over ORACLES_ENHANCED_REVEAL_FRAMES compositions from the
 * one its pixels came in after a composition that laid it dark without them (the ghost's render arriving), whole
 * otherwise. */
unsigned ev_room_edge_veil(OraclesEnhancedView *v, uint8_t room)
{
    const uint32_t bit = 1u << (room & 31u);
    unsigned i = 0;
    while (i < v->edge_fade_count && v->edge_fades[i].room != room) i++;
    if (i == v->edge_fade_count) {
        if (!(v->bare_places[room >> 5u] & bit) || v->edge_fade_count == sizeof v->edge_fades / sizeof v->edge_fades[0]) return 0;
        v->edge_fades[v->edge_fade_count].room = room;
        v->edge_fades[v->edge_fade_count++].since = v->frame;
        v->bare_places[room >> 5u] &= ~bit;
        v->edge_fades_counted++;
    }
    return oracles_enhanced_reveal_veil(v->frame - v->edge_fades[i].since + 1u);
}

/* The places of the grid the band shows that no source covers, the game's map holding a room there never visited:
 * dark as decided, not as waiting for the ghost, whose rooms are run as before. */
unsigned ev_add_hidden_places(OraclesEnhancedView *v, OraclesEnhancedNeighbour *nbs, unsigned count, unsigned capacity,
                              int32_t camera_x, int32_t camera_y, int32_t reach)
{
    const int grid = grid_shown(v);
    if (!v->unvisited_hidden || grid == GRID_NONE) return count;
    const int32_t h = (int32_t)ORACLES_GHOST_AREA_HEIGHT;
    const int32_t col0 = camera_x / SMALL_ROOM_W - 1, row0 = camera_y / h - 1;
    for (int32_t row = row0; row <= row0 + 4; row++)
        for (int32_t col = col0; col <= col0 + 5 && count < capacity; col++) {
            if (!grid_room(v, grid, col, row)) continue;
            const uint8_t room = (uint8_t)(row * 16 + col);
            if (v->drawn_places[room >> 5u] & (1u << (room & 31u))) continue;
            if (!ev_shown_meets(v, camera_x, camera_y, col * SMALL_ROOM_W, row * h, SMALL_ROOM_W, h, reach)) continue;
            if (!ev_room_hidden(v, v->observer.ref_group, room)) continue;
            ev_room_veil(v, room);
            OraclesEnhancedNeighbour *nb = &nbs[count++];
            memset(nb, 0, sizeof *nb);
            nb->world_left = col * SMALL_ROOM_W; nb->world_top = row * h;
            nb->veil = 256u;
            v->drawn_places[room >> 5u] |= 1u << (room & 31u);
            v->bare_now[room >> 5u] |= 1u << (room & 31u);
        }
    return count;
}

void ev_hidden_end(OraclesEnhancedView *v)
{
    if (!v->unvisited_hidden) return;
    memcpy(v->hidden_places, v->hidden_now, sizeof v->hidden_places);
    memcpy(v->shown_places, v->shown_now, sizeof v->shown_places);
    memcpy(v->bare_places, v->bare_now, sizeof v->bare_places);
    for (unsigned i = 0; i < v->reveal_count;)
        if (v->frame - v->reveals[i].since + 1u >= ORACLES_ENHANCED_REVEAL_FRAMES) v->reveals[i] = v->reveals[--v->reveal_count];
        else i++;
    for (unsigned i = 0; i < v->edge_fade_count;)
        if (v->frame - v->edge_fades[i].since + 1u >= ORACLES_ENHANCED_REVEAL_FRAMES) v->edge_fades[i] = v->edge_fades[--v->edge_fade_count];
        else i++;
    if (v->hidden_count) v->hidden_frames++;
    if (v->hidden_count > v->hidden_rooms_max) v->hidden_rooms_max = v->hidden_count;
}

/* The game marks the room it plays visited as it gives Link the controls back after a load or a scroll
 * (setVisitedRoomFlag, cutscene00 a few frames after the load's end): counted past REFERENCE_SETTLE frames of play in a
 * row, a cutscene of the game's own (wCutsceneIndex above 1, which loads its rooms itself) aside. */
#define REFERENCE_SETTLE 8u
void ev_check_reference_visited(OraclesEnhancedView *v)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    const int settled = ob->playing && !ob->in_transition && !ob->cutscene && v->observer.have_reference;
    v->reference_settled = settled ? v->reference_settled + 1u : 0u;
    if (v->reference_settled > REFERENCE_SETTLE
        && !(ev_live_byte(v, ev_room_flags_address(v, v->observer.ref_group, v->observer.ref_room)) & oracles_guest_tables(v->guest)->roomflag_visited)) {
        if (!v->unvisited_reference++) {
            v->unvisited_reference_first = v->frame;
            v->unvisited_reference_group = v->observer.ref_group;
            v->unvisited_reference_room = v->observer.ref_room;
        }
    }
}

void oracles_enhanced_view_set_unvisited_hidden(OraclesEnhancedView *v, int hidden)
{
    if (v->unvisited_hidden == (hidden != 0)) return;
    v->unvisited_hidden = hidden != 0;
    v->have_hidden_places = 0;   /* switched: the rooms as they are, none fading in */
    v->reveal_count = 0;
    v->edge_fade_count = 0;
}

int oracles_enhanced_view_unvisited_hidden(const OraclesEnhancedView *v) { return v->unvisited_hidden; }
void oracles_enhanced_view_set_unvisited_edge(OraclesEnhancedView *v, int soft) { v->unvisited_hard = !soft; }
void oracles_enhanced_view_check_dark(OraclesEnhancedView *v, int enabled) { v->dark_check = enabled != 0; }
unsigned oracles_enhanced_view_dark_lit(const OraclesEnhancedView *v) { return v->dark_lit; }
unsigned oracles_enhanced_view_dark_stray(const OraclesEnhancedView *v) { return v->dark_stray; }

int oracles_enhanced_view_drawn_rooms(const OraclesEnhancedView *v, uint8_t *group, uint32_t dark[8], uint32_t shown[8])
{
    if (!v->unvisited_hidden || !v->have_hidden_places) return 0;
    *group = v->hidden_group;
    memcpy(dark, v->hidden_places, sizeof v->hidden_places);
    memcpy(shown, v->shown_places, sizeof v->shown_places);
    return 1;
}

/* The room drawn that way, else the grid's: kept dark by the last composition. */
int oracles_enhanced_view_neighbour_dark(const OraclesEnhancedView *v, unsigned direction, uint8_t *room_out)
{
    uint8_t room;
    if (!v->unvisited_hidden || !v->have_hidden_places || direction > 3u) return 0;
    if (v->shown_slot[direction] < v->slot_count && v->slots[v->shown_slot[direction]].used) room = v->slots[v->shown_slot[direction]].room;
    else if (!ev_on_map(v) || !ev_room_toward(v, v->observer.ref_room, (OraclesGhostDirection)direction, &room)) return 0;
    if (room_out) *room_out = room;
    return (v->hidden_places[room >> 5u] & (1u << (room & 31u))) != 0;
}

void oracles_enhanced_view_unvisited_counts(const OraclesEnhancedView *v, OraclesEnhancedUnvisitedCounts *out)
{
    out->hidden_frames = v->hidden_frames;
    out->hidden_rooms_max = v->hidden_rooms_max;
    out->reveal_fades = v->reveal_fades;
    out->edge_fades = v->edge_fades_counted;
    out->unvisited_reference = v->unvisited_reference;
    out->first_frame = v->unvisited_reference_first ? v->unvisited_reference_first - 1u : 0u;
    out->first_group = v->unvisited_reference_group;
    out->first_room = v->unvisited_reference_room;
}
