#include "view_internal.h"

/* ---- the order of the ghost's runs ----------------------------------------------------------- */

/* The margin around the band within which a room is about to be shown: some
 * thirty frames of Link's walk or swim, more than a run takes. */
#define SOON_SHOWN_MARGIN 32

/* Whether the world band of the last composition shows part of a room of the
 * grid, or, `reach` px wider on every side, is about to. */
static int room_in_view(const OraclesEnhancedView *v, uint8_t room, int32_t reach)
{
    if (!v->have_view) return 0;
    if (v->observation.large_grid) return 1;   /* a large room's neighbours are not placed by their index; all one step away */
    const int32_t left = (int32_t)(room & 0x0fu) * SMALL_ROOM_W, top = (int32_t)(room >> 4u) * (int32_t)ORACLES_GHOST_AREA_HEIGHT;
    return ev_shown_meets(v, v->view_left, v->view_top, left, top, SMALL_ROOM_W, (int32_t)ORACLES_GHOST_AREA_HEIGHT, reach);
}

int ev_room_in_view(const OraclesEnhancedView *v, uint8_t room, int32_t reach) { return room_in_view(v, room, reach); }

int oracles_enhanced_scroll_brings(int32_t left, int32_t top, int32_t width, int32_t height, uint8_t room)
{
    const int32_t h = (int32_t)ORACLES_GHOST_AREA_HEIGHT, room_left = (int32_t)(room & 0x0fu) * SMALL_ROOM_W, room_top = (int32_t)(room >> 4u) * h;
    const int column = room_left < left + width && room_left + SMALL_ROOM_W > left && room_top - h < top + height && room_top + 2 * h > top;
    const int row = room_top < top + height && room_top + h > top && room_left - SMALL_ROOM_W < left + width && room_left + 2 * SMALL_ROOM_W > left;
    return column || row;
}

/* Whether the band of the last composition, moved as a scroll moves it, would show part of a room of the grid. */
static int room_a_scroll_away(const OraclesEnhancedView *v, uint8_t room)
{
    return v->have_view && oracles_enhanced_scroll_brings(v->view_left + (int32_t)(v->size.width - v->shown_width) / 2, v->view_top + (int32_t)(v->band_height - v->shown_height) / 2,
                                                          (int32_t)v->shown_width, (int32_t)v->shown_height, room);
}

/* Whether an entry needs a run now: none, or failed long enough ago; a run
 * put off until Link walks on (a door under him, a text box on his way)
 * waits for him to be 16 px from where he stood. */
static int entry_wanting(OraclesEnhancedView *v, entry *e)
{
    if (!e || (e->valid && !e->refresh && !e->rerun) || e->in_flight) return 0;   /* in flight: in another ghost */
    /* A room still drawn, run again for its objects alone, waits for the
     * rooms of the band still black. */
    if (v->defer_objects_refresh && e->valid && e->refresh_objects && !e->rerun) { v->objects_refresh_deferred = 1; return 0; }
    /* Nor while a text box, a menu or a scene holds the live game, which a
     * run from it would play (a treasure's text): it starts once the game is
     * back in play, for the story a moment after, the event's own text and
     * scene come and gone. */
    if (e->valid && e->refresh_objects && !e->rerun && !oracles_enhanced_objects_refresh_may_start(e->refresh_story, v->live_quiet)) return 0;
    /* A terrain run again so often that a byte keeps moving: the end of the period first, still drawn. */
    if (e->valid && e->refresh_terrain && !e->rerun && oracles_enhanced_terrain_refresh_waits(e->terrain_refresh_burst, v->frame - e->terrain_refresh_at)) return 0;
    if (e->retry_when_link_moves && e->retry_after_fade && v->fade_effective == 0) { e->retry_when_link_moves = 0; e->retry_after_fade = 0; }
    if (e->retry_when_link_moves) {
        const int dx = v->observation.world.link_x - e->link_x, dy = v->observation.world.link_y - e->link_y;
        if (dx * dx + dy * dy < 16 * 16) return 0;
        e->retry_when_link_moves = 0;
        e->retry_after_fade = 0;
    }
    if (e->failed && v->frame - e->failed_at < FAILED_RETRY_FRAMES) return 0;
    return 1;
}

/* A run from the live state, if it can be primed. */
static int run_from_live(OraclesEnhancedView *v, entry *e, OraclesGhostDirection dir, int live_primeable)
{
    if (!live_primeable || e->in_flight) return 0;
    e->last_use = v->frame;
    if (oracles_core_save_state(v->core, v->snapshot, v->state_size) != 0) return 0;
    return ev_start_job(v, e, v->snapshot, v->state_size, dir, NULL);
}

/* A room whose parent is a room the game routes itself (Seasons' 0:b9, above
 * the sword upgrade room 0:c9 and entered only through it): the scroll out of
 * the parent goes elsewhere.  The drawn-back band asks for it from another
 * room beside it that the ghost has delivered, going toward it, only while
 * that room holds for the live key and the enemies killed, never run again
 * for it.  Returns 1 when a run was started. */
static int run_from_other_side(OraclesEnhancedView *v, entry *e, const entry *routed, const uint8_t *key, size_t key_len)
{
    for (unsigned d = 0; d < 4u; d++) {
        uint8_t room;
        if (!ev_room_toward(v, e->room, (OraclesGhostDirection)d, &room) || room == routed->room) continue;
        entry *side = ev_find_entry(v, e->group, room);
        if (!side || !side->valid || !side->settled_size || side->refresh || side->rerun || side->not_a_start || ev_self_routed(v, side->group, side->room)) continue;
        int visits = 0;
        if (!ev_entry_key_current(v, side, key, key_len, e->group, e->room, e->strict_parent || !side->settled_primeable ? NULL : &visits)) continue;
        if (v->neighbour_objects && oracles_objects_killed_in_list(side->killed_list, e->room) != oracles_objects_killed_enemies(v->guest, e->room)) continue;
        e->last_use = v->frame;
        if (!ev_start_job(v, e, side->settled_state, side->settled_size, (OraclesGhostDirection)((d + 2u) & 3u), side)) return 0;
        if (visits) { v->pending_beside = 1; v->pending_visits = 1; }
        return 1;
    }
    return 0;
}

/* A run from a parent's settled state, if the parent is valid and its state
 * does not predate the key; when it does, the parent is run again first,
 * its terrain staying on screen meanwhile: from the live state when it is
 * beside the reference room (`parent_dir` its direction from the reference
 * room), else (`two_steps`: a diagonal) from its own parent `via`, the room
 * above or below it (`parent_dir` is then the way from `via` to it,
 * `via_dir` the direction of `via` from the reference room), or not at all.  A diagonal run
 * from the live state would scroll into the room above or below.  A parent
 * whose state refused to prime (not_a_start) is no start while it holds: the
 * room waits for another parent; one that no longer holds is run again. */
static int run_from_parent_as(OraclesEnhancedView *v, entry *e, entry *parent, OraclesGhostDirection dir, OraclesGhostDirection parent_dir,
                              int two_steps, entry *via, OraclesGhostDirection via_dir, const uint8_t *key, size_t key_len, int live_primeable, int whole_key)
{
    if (!parent || !parent->valid || !parent->settled_size) return 0;
    /* A room the game routes itself is no start for the room beyond it: the
     * scroll out of it goes where the game decides and not where the grid
     * says (Seasons' sword upgrade room sends Link into the Lost Woods), so
     * the run would deliver another room and be thrown away.  The room
     * beyond waits for its other parent, the one the diagonals fall back
     * to, or stays black.  Inside Ages' forest every room is of the table,
     * and the diagonals and the rooms beyond would have no parent at all:
     * such a room is a start when the room in play is one of the table too
     * and has not been seen routing elsewhere (the forest unscrambled routes
     * the standard way), or, in the drawn-back band, in the order's last
     * pass, when nothing else is to run (Link beside the forest); the parent
     * has not been seen routing elsewhere either: the result is taken only
     * if it is the room asked for, and one delivered elsewhere teaches the
     * routing (record_wrong_room), which ends the chaining from that room,
     * one run lost at most per room of the table (Seasons' sword upgrade
     * room and Lost Woods). */
    /* Ages' table has the forest alone outdoors; Seasons' rooms of it (the
     * Lost Woods, the sword upgrade room) route elsewhere for good. */
    const int in_forest = oracles_compat_forest_scrambler_rules(oracles_guest_profile(v->guest)) && ev_routed_candidate(v) && !ev_routes_elsewhere(v, v->observer.ref_group, v->observer.ref_room);
    const int inside_standard_zone = (in_forest || v->chain_routed_idle) && !ev_routes_elsewhere(v, parent->group, parent->room);
    if (ev_self_routed(v, parent->group, parent->room) && !in_forest && ev_drawn_back(v) && run_from_other_side(v, e, parent, key, key_len)) return 1;
    if (ev_self_routed(v, parent->group, parent->room) && !inside_standard_zone) return 0;
    /* The key leaves out the enemies killed: a parent settled before a kill
     * in the room asked for would create it with the enemy alive. */
    const int killed_current = !v->neighbour_objects || oracles_objects_killed_in_list(parent->killed_list, e->room) == oracles_objects_killed_enemies(v->guest, e->room);
    /* A diagonal's own key is the one the ghost read in the settled state of
     * the room above or below it, that room's own bytes in it (its
     * wRoomStateModifier, the visited bit the ghost set): never the live
     * key.  It stands for the live state through that room, whose key it is
     * judged on, the diagonal delivered since that room's last run.  In the
     * drawn-back band the visited bits of the other rooms are aside: a first
     * entry into a room sets Link's, and the rooms a chain crossed carry
     * those the ghost set, so that after every transition into new ground
     * no parent held, and the whole chain ran again from the live state
     * while its farthest rooms, shown there, waited.  A run from such a
     * parent has its reads checked on delivery (beside_reads_hold).  Not a
     * parent whose settled state was kept out of normal play (Link walking
     * on into the next transition as the run settled, the intro's 0:79 left
     * as Link went up from 0:7a): a run from it plays that transition first
     * and comes back with another room.  It is judged on the whole key and
     * run again as before, and so is the room above or below it when it is
     * a diagonal (`whole_key`): run again through that room as it stands,
     * the diagonal came back out of normal play each time, and the room
     * waiting for it asked again at every frame (Moonrise Regalia's 0:47,
     * for 0:57 on mGBA). */
    int visits = 0;
    int *const aside = ev_drawn_back(v) && !whole_key && !e->strict_parent && parent->settled_primeable ? &visits : NULL;
    const int key_current = two_steps ? (via && ev_entry_key_current(v, via, key, key_len, e->group, e->room, aside) && parent->accepted_at >= via->accepted_at)
                                      : ev_entry_key_current(v, parent, key, key_len, e->group, e->room, aside);
    if (parent->refresh || parent->rerun || !killed_current || !key_current) {
        /* A parent whose run for its state failed stays drawn (record_failure): tried again at the pace of a failure,
         * as is one whose state was no start for this room (waits_start). */
        if (parent->failed && v->frame - parent->failed_at < FAILED_RETRY_FRAMES) return 0;
        if (parent->refresh_terrain && oracles_enhanced_terrain_refresh_waits(parent->terrain_refresh_burst, v->frame - parent->terrain_refresh_at)) return 0;   /* its terrain's burst first, as its own step waits */
        if (!oracles_enhanced_parent_rerun_due(e->waits_start, e->waits_parent, e->waits_ref, e->waits_since,
                                               (unsigned)parent->group << 8 | parent->room, (unsigned)v->observer.ref_group << 8 | v->observer.ref_room, v->frame)) return 0;
        /* A parent two steps away is run again from `via` alone, which must
         * then agree on the enemies of the room asked for too: otherwise
         * `via` itself first, from the live state. */
        int started = 0;
        if (!two_steps) started = run_from_live(v, parent, parent_dir, live_primeable);
        else if (via && via->valid) {
            const int via_current = !v->neighbour_objects
                || oracles_objects_killed_in_list(via->killed_list, e->room) == oracles_objects_killed_enemies(v->guest, e->room);
            started = via_current ? run_from_parent_as(v, parent, via, parent_dir, via_dir, 0, NULL, via_dir, key, key_len, live_primeable, aside == NULL)
                                  : run_from_live(v, via, via_dir, live_primeable);
        }
        if (started) { v->refreshed++; if (!killed_current) v->parents_run_for_kills++; return 1; }
        return 0;
    }
    if (parent->not_a_start) { ev_wait_for_start(v, e, parent); return 0; }
    e->last_use = v->frame;
    if (!ev_start_job(v, e, parent->settled_state, parent->settled_size, dir, parent)) return 0;
    if (visits) { v->pending_beside = 1; v->pending_visits = 1; }
    return 1;
}

static int run_from_parent(OraclesEnhancedView *v, entry *e, entry *parent, OraclesGhostDirection dir, OraclesGhostDirection parent_dir,
                           int two_steps, entry *via, OraclesGhostDirection via_dir, const uint8_t *key, size_t key_len, int live_primeable)
{
    return run_from_parent_as(v, e, parent, dir, parent_dir, two_steps, via, via_dir, key, key_len, live_primeable, 0);
}

/* The rooms a frame may run, around the reference room: the four beside it,
 * the two beyond left and right, and the four diagonals (an interior's
 * neighbours are known one room deep: the rooms beyond would need the
 * neighbour's own edges, and a house rarely goes further). */
typedef struct run_plan {
    uint8_t group, ref;
    uint8_t first[4], second[SIDES], diagonal[2][SIDES], wanted[PLAN_ROOMS];
    int have_first[4], have_second[SIDES], have_diagonal[2][SIDES];
    /* The second ring, run once the rest is ready, so that the rooms a
     * transition brings into the band are there when it happens:
     * the diagonals of the rooms beyond left and right, the rooms beyond
     * above and below, and their diagonals. */
    uint8_t second_diagonal[2][SIDES], beyond[2], beyond_diagonal[2][SIDES];
    int have_second_diagonal[2][SIDES], have_beyond[2], have_beyond_diagonal[2][SIDES];
    /* The drawn-back band's corners, two rows and two columns away: the
     * camera's look-ahead shows a few of their lines before a vertical
     * transition makes them the second ring (the 4:3 band, taller, more of
     * them). */
    uint8_t corner[2][SIDES];
    int have_corner[2][SIDES];
    unsigned wanted_count;
    unsigned near_count;             /* the first of `wanted`: the room, its neighbours, the two beyond left and right, the diagonals */
    int routed_ask;                  /* the room in play is one the game may route: its four directions are asked, not taken from the grid */
    int routed;                      /* and it has been seen routing elsewhere: four runs, one a direction, and nothing else */
    int overworld;                   /* the rooms beyond are wanted: an overworld, or the sea, which runs its rooms like one */
    int sea;                         /* the plan of the other side of the sea (Ages): its runs from the live state cross the surface first */
    uint8_t key[ORACLES_GHOST_KEY_BYTES];
    size_t key_len;
    int live_primeable;
} run_plan;

static const OraclesGhostDirection plan_sides[SIDES] = { ORACLES_DIR_LEFT, ORACLES_DIR_RIGHT };
static const OraclesGhostDirection plan_verticals[2] = { ORACLES_DIR_UP, ORACLES_DIR_DOWN };

/* The room beside on the plan's map: the reference room's (a dungeon's by its
 * floor's layout), or the other side of the sea's grid. */
static int plan_toward(const OraclesEnhancedView *v, const run_plan *p, uint8_t room, OraclesGhostDirection dir, uint8_t *out)
{
    return p->sea ? ev_room_toward_in(v, p->group, room, dir, out) : ev_room_toward(v, room, dir, out);
}

/* The plan around the reference room, or (`sea`) around the room of the same
 * index on the other side of the sea, in group `group`: a map like the
 * overworld, whose rooms are asked from the live state across the surface. */
static void plan_runs(const OraclesEnhancedView *v, run_plan *p, uint8_t group, int sea)
{
    p->group = group; p->ref = v->observer.ref_room; p->sea = sea;
    p->wanted_count = 0;
    p->wanted[p->wanted_count++] = p->ref;
    /* A room the game routes itself has no neighbour on the grid to plan
     * for: its four directions are asked of the ghost, and nothing is
     * chained from what they bring back, which holds for one sequence of
     * directions only. */
    p->routed_ask = !sea && ev_routed_candidate(v);
    p->routed = p->routed_ask && ev_in_routed_zone(v);
    if (p->routed) {
        memset(p->have_first, 0, sizeof p->have_first);
        memset(p->have_second, 0, sizeof p->have_second);
        memset(p->have_diagonal, 0, sizeof p->have_diagonal);
        memset(p->have_second_diagonal, 0, sizeof p->have_second_diagonal);
        memset(p->have_beyond, 0, sizeof p->have_beyond);
        memset(p->have_beyond_diagonal, 0, sizeof p->have_beyond_diagonal);
        memset(p->have_corner, 0, sizeof p->have_corner);
        p->overworld = 0;
        /* The rooms beyond an answer that is an ordinary room keep their slots too. */
        for (unsigned i = 0; i < v->slot_count; i++) {
            const entry *r = &v->slots[i];
            if (!r->used || !r->routed || !r->valid || r->group != group || r->routed_from != p->ref) continue;
            uint8_t rooms[5];
            int have[5];
            if (!oracles_enhanced_routed_beyond(p->ref, r->routed_dir, r->room, ev_self_routed(v, group, r->room), ev_map_width(v, group), ev_map_height(v, group), rooms, have)) continue;
            for (unsigned k = 0; k < 5u && p->wanted_count < PLAN_ROOMS; k++) if (have[k]) p->wanted[p->wanted_count++] = rooms[k];
        }
        p->near_count = p->wanted_count;
        return;
    }
    for (unsigned d = 0; d < 4; d++) {
        p->have_first[d] = plan_toward(v, p, p->ref, (OraclesGhostDirection)d, &p->first[d]);
        if (p->have_first[d]) p->wanted[p->wanted_count++] = p->first[d];
    }
    p->overworld = sea || ev_on_overworld(v) || ev_open_water(v, p->group, p->ref);
    for (unsigned s = 0; s < SIDES; s++) {
        p->have_second[s] = p->overworld && p->have_first[plan_sides[s]] && plan_toward(v, p, p->first[plan_sides[s]], plan_sides[s], &p->second[s]);
        if (p->have_second[s]) p->wanted[p->wanted_count++] = p->second[s];
        for (unsigned u = 0; u < 2; u++) {
            p->have_diagonal[u][s] = p->overworld && p->have_first[plan_verticals[u]] && plan_toward(v, p, p->first[plan_verticals[u]], plan_sides[s], &p->diagonal[u][s]);
            if (p->have_diagonal[u][s]) p->wanted[p->wanted_count++] = p->diagonal[u][s];
        }
    }
    p->near_count = p->wanted_count;
    for (unsigned u = 0; u < 2; u++) {
        p->have_beyond[u] = p->overworld && p->have_first[plan_verticals[u]] && plan_toward(v, p, p->first[plan_verticals[u]], plan_verticals[u], &p->beyond[u]);
        if (p->have_beyond[u]) p->wanted[p->wanted_count++] = p->beyond[u];
        for (unsigned s = 0; s < SIDES; s++) {
            p->have_second_diagonal[u][s] = p->have_second[s] && plan_toward(v, p, p->second[s], plan_verticals[u], &p->second_diagonal[u][s]);
            if (p->have_second_diagonal[u][s]) p->wanted[p->wanted_count++] = p->second_diagonal[u][s];
            p->have_beyond_diagonal[u][s] = p->have_beyond[u] && plan_toward(v, p, p->beyond[u], plan_sides[s], &p->beyond_diagonal[u][s]);
            if (p->have_beyond_diagonal[u][s]) p->wanted[p->wanted_count++] = p->beyond_diagonal[u][s];
            p->have_corner[u][s] = ev_drawn_back(v) && p->have_second_diagonal[u][s]
                && plan_toward(v, p, p->second_diagonal[u][s], plan_verticals[u], &p->corner[u][s]);
            if (p->have_corner[u][s]) p->wanted[p->wanted_count++] = p->corner[u][s];
        }
    }
}

/* The neighbours the compose shows this frame in each direction, for the harness. */
static void mark_shown_sides(OraclesEnhancedView *v, const run_plan *p)
{
    for (unsigned d = 0; d < 4; d++) {
        entry *e = p->routed_ask ? ev_find_routed_entry(v, p->group, p->ref, (OraclesGhostDirection)d) : NULL;
        if (!e && p->have_first[d]) e = ev_find_entry(v, p->group, p->first[d]);
        if (!e) continue;
        e->last_use = v->frame;
        if (ev_entry_drawable(v, e) && ev_entry_connected(v, e, NULL)) v->shown_slot[d] = (unsigned)(e - v->slots);
    }
}

/* Whether the band shows the place beside the room in play that way. */
static int routed_place_in_view(const OraclesEnhancedView *v, uint8_t ref, OraclesGhostDirection dir)
{
    if (!v->have_view) return 0;
    int32_t left, top;
    ev_routed_place(ref, dir, &left, &top);
    return ev_shown_meets(v, v->view_left, v->view_top, left, top, SMALL_ROOM_W, (int32_t)ORACLES_GHOST_AREA_HEIGHT, 0);
}

/* The four directions of a room the game routes itself, each asked from the
 * live state: nothing is chained from what one of them brought back, whose
 * own answer would be the one for a sequence of directions Link has not
 * walked.  Returns 1 when a run was started. */
static int schedule_routed_pass(OraclesEnhancedView *v, run_plan *p, int visible_only)
{
    /* The first pass is empty when the frame before was not the world band
     * (the frame the room becomes primeable again after a transition often
     * is not): the order is then all that decides. */
    unsigned order[4];
    ev_routed_run_order(v, p->ref, order);
    const int routes = ev_routes_elsewhere(v, p->group, p->ref);
    for (unsigned i = 0; i < 4; i++) {
        if (visible_only && !routed_place_in_view(v, p->ref, (OraclesGhostDirection)order[i])) continue;
        /* A room never seen routing elsewhere, whose neighbour on the grid is
         * already there: the band is full that way, and asking again would
         * take the ghost from a room that has nothing.  The live game keeps
         * teaching (ev_note_routing): the first scroll out of a room that
         * does route turns every direction into a question again. */
        uint8_t target;
        if (!routes) {
            /* The map's edge: the standard transition leaves the map there,
             * where the camera never goes, and the answer would teach
             * nothing.  Asked all the same once the room is known to route. */
            if (!ev_room_toward(v, p->ref, (OraclesGhostDirection)order[i], &target)) continue;
            const entry *grid = ev_find_entry(v, p->group, target);
            if (grid && grid->valid) continue;
        }
        entry *e = ev_take_routed_slot(v, p->group, p->ref, (OraclesGhostDirection)order[i], p->wanted, p->wanted_count);
        if (!entry_wanting(v, e)) continue;
        if (run_from_live(v, e, (OraclesGhostDirection)order[i], p->live_primeable)) return 1;
    }
    return 0;
}

/* Beyond an answer of the room in play that is an ordinary room, the grid
 * again, read through that answer (oracles_enhanced_routed_beyond): the room
 * beyond it and the two beside it from its settled state, the two beside the
 * room beyond from that room's.  Returns 1 when a run was started. */
static int schedule_routed_beyond(OraclesEnhancedView *v, run_plan *p, int visible_only)
{
    for (unsigned d = 0; d < 4u; d++) {
        entry *r = ev_find_routed_entry(v, p->group, p->ref, (OraclesGhostDirection)d);
        if (!r || !r->valid) continue;
        uint8_t rooms[5];
        int have[5];
        if (!oracles_enhanced_routed_beyond(p->ref, d, r->room, ev_self_routed(v, p->group, r->room), ev_map_width(v, p->group), ev_map_height(v, p->group), rooms, have)) continue;
        const OraclesGhostDirection dir = (OraclesGhostDirection)d;
        const OraclesGhostDirection across[2] = { d & 1u ? ORACLES_DIR_UP : ORACLES_DIR_LEFT, d & 1u ? ORACLES_DIR_DOWN : ORACLES_DIR_RIGHT };
        for (unsigned k = 0; k < 5u; k++) {
            if (!have[k] || (visible_only && !room_in_view(v, rooms[k], 0))) continue;
            entry *beyond = k >= 3u ? ev_find_entry(v, p->group, rooms[0]) : NULL;
            if (k >= 3u && (!beyond || !beyond->valid)) continue;
            entry *e = ev_take_slot(v, p->group, rooms[k], p->wanted, p->wanted_count);
            if (!entry_wanting(v, e)) continue;
            const int started = k == 0 ? run_from_parent(v, e, r, dir, dir, 0, NULL, dir, p->key, p->key_len, p->live_primeable)
                              : k <= 2u ? run_from_parent(v, e, r, across[k - 1u], dir, 0, NULL, dir, p->key, p->key_len, p->live_primeable)
                              : run_from_parent(v, e, beyond, across[k - 3u], dir, 1, r, dir, p->key, p->key_len, p->live_primeable);
            if (started) return 1;
        }
    }
    return 0;
}

/* Whether the rooms around the reference room (its neighbours, the two
 * beyond left and right, the diagonals) are there, failed or under way: the
 * band's fill after a warp or a load is over. */
static int near_ready(OraclesEnhancedView *v, const run_plan *p)
{
    for (unsigned w = 1; w < p->near_count; w++) {
        const entry *e = ev_find_entry(v, p->group, p->wanted[w]);
        if (!e || !oracles_enhanced_fill_settled(e->valid, e->in_flight, e->failed, e->waits_start, e->waits_ref,
                                                 (unsigned)v->observer.ref_group << 8 | v->observer.ref_room)) return 0;
    }
    return 1;
}

int oracles_enhanced_fill_settled(int valid, int in_flight, int failed, int waits_start, unsigned waits_ref, unsigned ref)
{
    return valid || in_flight || failed || (waits_start && waits_ref == ref);
}

int oracles_enhanced_parent_rerun_due(int waits_start, unsigned waits_parent, unsigned waits_ref, uint32_t waits_since,
                                      unsigned parent, unsigned ref, uint32_t frame)
{
    return !waits_start || waits_parent != parent || waits_ref != ref || frame - waits_since >= FAILED_RETRY_FRAMES;
}

/* A step of the run order: a room of the plan and the way it is run. */
enum { STEP_SIDE, STEP_SECOND, STEP_REF, STEP_VERTICAL, STEP_DIAGONAL, STEP_BEYOND, STEP_SECOND_DIAGONAL, STEP_BEYOND_DIAGONAL, STEP_CORNER };
typedef struct run_step { uint8_t kind, u, s, room; int32_t shown, distance; } run_step;

/* A parent to run a room from as it stands, the live state unable to run it
 * again first: its key the live one but for the flags of other rooms than
 * the room run (a room visited since it settled: 0:98, where Din dances,
 * after the rooms around it were computed), and the enemies killed the
 * same.  A room that does read another's flags is refused on delivery by
 * the reads its run recorded, taken from the parent's state, against the
 * live ones (beside_reads_hold): never kept under values it did not see.
 * One read escapes the trace: Ages' roomTileChangesAfterLoad06 (the present's
 * Maku tree, a linked game) reads wGroup4RoomFlags+$fc outside its window,
 * but changes wRoomLayout alone (a staircase), not the tiles drawn. */
int oracles_enhanced_parent_key_holds(const uint8_t *parent_key, const uint8_t *live_key, size_t length, long flags_at, size_t flags_length, long own)
{
    for (size_t i = 0; i < length; i++) {
        if (parent_key[i] == live_key[i]) continue;
        if (flags_at < 0 || own < 0 || (long)i < flags_at || (long)i >= flags_at + (long)flags_length || (long)i == own) return 0;
    }
    return 1;
}

int oracles_enhanced_parent_key_current(const uint8_t *parent_key, const uint8_t *live_key, size_t length, long flags_at, size_t flags_length, long own,
                                        uint8_t visited, int *visits)
{
    for (size_t i = 0; i < length; i++) {
        const uint8_t differs = (uint8_t)(parent_key[i] ^ live_key[i]);
        if (!differs) continue;
        if (differs & (uint8_t)~visited) return 0;
        if ((long)i == own) continue;
        if (!visits || flags_at < 0 || (long)i < flags_at || (long)i >= flags_at + (long)flags_length) return 0;
        *visits = 1;
    }
    return 1;
}

static int parent_holds(OraclesEnhancedView *v, const run_plan *p, const entry *parent, uint8_t group, uint8_t room)
{
    if (!parent || !parent->valid || !parent->settled_size || parent->refresh || parent->rerun || parent->not_a_start || parent->key_len != p->key_len) return 0;
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    /* The time portal and a time warp's arrival, for the room chained, as in ev_invalidate_by_reads: the live key with
     * the parent's values where they do not matter to it. */
    uint8_t live[ORACLES_GHOST_KEY_BYTES];
    if (p->key_len > sizeof live) return 0;
    memcpy(live, p->key, p->key_len);
    for (size_t i = 0; i < p->key_len; i++) if (live[i] != parent->key_snapshot[i] && ev_key_byte_exempt(v, (long)i, parent->key_snapshot, p->key, group, room)) live[i] = parent->key_snapshot[i];
    if (!oracles_enhanced_parent_key_holds(parent->key_snapshot, live, p->key_len, ev_key_offset(v, t->group0_room_flags.addr), t->room_flags_size,
                                           ev_key_offset(v, ev_room_flags_address(v, group, room)))) return 0;
    return !v->neighbour_objects || oracles_objects_killed_in_list(parent->killed_list, room) == oracles_objects_killed_enemies(v->guest, room);
}

/* A room beside the reference room while a cutscene holds the game and the
 * live state cannot prime the ghost (Din's dance in Seasons' intro lasts
 * some 4 000 frames; a text or a menu passes, and the live state is waited
 * for): from a room beside it already delivered, entered from that side as
 * a diagonal is.  Without it a neighbour lost at the scene's start
 * (a capture refused in the fade that opens it) stayed black to its end
 * while the rooms around it were drawn.  The same for a room beyond left or
 * right, above or below whose only parent's state is no start (not_a_start),
 * and for a room of the second ring chained through such a room or from such
 * a diagonal.  `beside` are the candidates, `ways` the direction from each
 * into the room. */
static int run_from_beside(OraclesEnhancedView *v, run_plan *p, entry *e, uint8_t room, const entry *const beside[3], const OraclesGhostDirection ways[3])
{
    for (unsigned k = 0; k < 3u; k++) {
        entry *parent = (entry *)beside[k];
        if (parent_holds(v, p, parent, p->group, room) && !ev_self_routed(v, parent->group, parent->room)
            && ev_start_job(v, e, parent->settled_state, parent->settled_size, ways[k], parent)) { v->pending_beside = 1; return 1; }
    }
    return 0;
}

/* A neighbour on the other side of the sea, from the room there under Link
 * once delivered and holding, as a room beside it: a run from the live state
 * plays the dive or the return to the surface first, three times as long. */
static int run_from_sea_reference(OraclesEnhancedView *v, run_plan *p, entry *e, OraclesGhostDirection dir)
{
    if (!p->sea) return 0;
    const entry *const beside[3] = { ev_find_entry(v, p->group, p->ref), NULL, NULL };
    const OraclesGhostDirection ways[3] = { dir, dir, dir };
    return run_from_beside(v, p, e, e->room, beside, ways);
}

/* Runs the step's room if it wants a run and its parent allows; returns 1 when a run was started. */
static int run_step_now(OraclesEnhancedView *v, run_plan *p, const run_step *st)
{
    const uint8_t group = p->group;
    const unsigned u = st->u, s = st->s;
    switch (st->kind) {
    case STEP_SIDE: {   /* left and right, from the live state, or from beside while it cannot prime */
        entry *e = ev_take_slot(v, group, p->first[plan_sides[s]], p->wanted, p->wanted_count);
        if (!entry_wanting(v, e)) return 0;
        if (run_from_sea_reference(v, p, e, plan_sides[s])) return 1;
        if (p->live_primeable || !v->observation.cutscene) return run_from_live(v, e, plan_sides[s], p->live_primeable);
        const entry *const beside[3] = { p->have_diagonal[0][s] ? ev_find_entry(v, group, p->diagonal[0][s]) : NULL,
                                         p->have_diagonal[1][s] ? ev_find_entry(v, group, p->diagonal[1][s]) : NULL,
                                         p->have_second[s] ? ev_find_entry(v, group, p->second[s]) : NULL };
        const OraclesGhostDirection ways[3] = { ORACLES_DIR_DOWN, ORACLES_DIR_UP, plan_sides[1u - s] };
        return run_from_beside(v, p, e, p->first[plan_sides[s]], beside, ways);
    }
    case STEP_SECOND: {   /* beyond them, from their settled states, or from the outer ring's diagonals while that state is no start */
        entry *parent = ev_find_entry(v, group, p->first[plan_sides[s]]);
        if (!parent || !parent->valid) return 0;
        entry *e = ev_take_slot(v, group, p->second[s], p->wanted, p->wanted_count);
        if (!entry_wanting(v, e)) return 0;
        if (parent->not_a_start) {
            const entry *const beside[3] = { p->have_second_diagonal[0][s] ? ev_find_entry(v, group, p->second_diagonal[0][s]) : NULL,
                                             p->have_second_diagonal[1][s] ? ev_find_entry(v, group, p->second_diagonal[1][s]) : NULL, NULL };
            const OraclesGhostDirection ways[3] = { ORACLES_DIR_DOWN, ORACLES_DIR_UP, plan_sides[s] };
            if (run_from_beside(v, p, e, p->second[s], beside, ways)) return 1;
        }
        return run_from_parent(v, e, parent, plan_sides[s], plan_sides[s], 0, NULL, plan_sides[s], p->key, p->key_len, p->live_primeable);
    }
    case STEP_REF: {   /* the reference room, back from a neighbour */
        entry *e = ev_take_slot(v, group, p->ref, p->wanted, p->wanted_count);
        if (!entry_wanting(v, e)) return 0;
        for (unsigned side = 0; side < SIDES; side++) {
            if (!p->have_first[plan_sides[side]]) continue;
            entry *parent = ev_find_entry(v, group, p->first[plan_sides[side]]);
            if (run_from_parent(v, e, parent, plan_sides[1u - side], plan_sides[side], 0, NULL, plan_sides[side], p->key, p->key_len, p->live_primeable)) return 1;
        }
        return 0;
    }
    case STEP_VERTICAL: {   /* above and below, from the live state, or from beside while it cannot prime */
        entry *e = ev_take_slot(v, group, p->first[plan_verticals[u]], p->wanted, p->wanted_count);
        if (!entry_wanting(v, e)) return 0;
        if (run_from_sea_reference(v, p, e, plan_verticals[u])) return 1;
        if (p->live_primeable || !v->observation.cutscene) return run_from_live(v, e, plan_verticals[u], p->live_primeable);
        const entry *const beside[3] = { p->have_diagonal[u][0] ? ev_find_entry(v, group, p->diagonal[u][0]) : NULL,
                                         p->have_diagonal[u][1] ? ev_find_entry(v, group, p->diagonal[u][1]) : NULL,
                                         p->have_beyond[u] ? ev_find_entry(v, group, p->beyond[u]) : NULL };
        const OraclesGhostDirection ways[3] = { plan_sides[1], plan_sides[0], plan_verticals[1u - u] };
        return run_from_beside(v, p, e, p->first[plan_verticals[u]], beside, ways);
    }
    case STEP_DIAGONAL: {
        /* From the room above or below, or, when that one is not ready yet,
         * from the room beside, going up or down (the same room: the row's
         * neighbour is the column's). */
        entry *vertical = p->have_first[plan_verticals[u]] ? ev_find_entry(v, group, p->first[plan_verticals[u]]) : NULL;
        entry *side = p->have_first[plan_sides[s]] ? ev_find_entry(v, group, p->first[plan_sides[s]]) : NULL;
        uint8_t through;
        const int side_leads = side && side->valid && plan_toward(v, p, p->first[plan_sides[s]], plan_verticals[u], &through) && through == p->diagonal[u][s];
        if (!(vertical && vertical->valid) && !side_leads) return 0;
        entry *e = ev_take_slot(v, group, p->diagonal[u][s], p->wanted, p->wanted_count);
        if (!entry_wanting(v, e)) return 0;
        if (vertical && vertical->valid && run_from_parent(v, e, vertical, plan_sides[s], plan_verticals[u], 0, NULL, plan_verticals[u], p->key, p->key_len, p->live_primeable)) return 1;
        return side_leads && run_from_parent(v, e, side, plan_verticals[u], plan_sides[s], 0, NULL, plan_sides[s], p->key, p->key_len, p->live_primeable);
    }
    case STEP_BEYOND: {   /* the second ring, each room from a neighbour of the first ring already delivered, or from the diagonals beyond while that state is no start */
        entry *parent = ev_find_entry(v, group, p->first[plan_verticals[u]]);
        if (!parent || !parent->valid) return 0;
        entry *e = ev_take_slot(v, group, p->beyond[u], p->wanted, p->wanted_count);
        if (!entry_wanting(v, e)) return 0;
        if (parent->not_a_start) {
            const entry *const beside[3] = { p->have_beyond_diagonal[u][0] ? ev_find_entry(v, group, p->beyond_diagonal[u][0]) : NULL,
                                             p->have_beyond_diagonal[u][1] ? ev_find_entry(v, group, p->beyond_diagonal[u][1]) : NULL, NULL };
            const OraclesGhostDirection ways[3] = { plan_sides[1], plan_sides[0], plan_verticals[u] };
            if (run_from_beside(v, p, e, p->beyond[u], beside, ways)) return 1;
        }
        return run_from_parent(v, e, parent, plan_verticals[u], plan_verticals[u], 0, NULL, plan_verticals[u], p->key, p->key_len, p->live_primeable);
    }
    case STEP_CORNER: {
        /* From the second ring's diagonal beside it, going up or down, only
         * while that one holds for the live key and for the enemies killed.
         * A corner never runs its parent again itself, which would need a run
         * three rooms back: a parent that no longer holds for the key while
         * the corner has nothing is marked to be run again, and its own step
         * runs it from its diagonal, the chain going back as far as it must
         * (a corner stayed black otherwise until Link left the room: Ages'
         * wSeedTreeRefilledBitset, changed since the parent's run).  Once
         * until it holds again: a parent run again that still does not hold
         * differs where a run cannot help (the flags its chain sets
         * in rooms Link has not entered); and not while the band fills, whose
         * rooms beside Link go first. */
        entry *parent = ev_find_entry(v, group, p->second_diagonal[u][s]);
        if (!parent || !parent->valid || parent->refresh || parent->rerun) return 0;
        const entry *corner = ev_find_entry(v, group, p->corner[u][s]);
        int visits = 0;
        if (!ev_entry_key_current(v, parent, p->key, p->key_len, group, p->corner[u][s], (corner && corner->strict_parent) || !parent->settled_primeable ? NULL : &visits)) {
            if (!(corner && corner->valid) && !parent->in_flight && !parent->corner_asked && near_ready(v, p)) {
                parent->rerun = 1;
                parent->rerun_at = v->frame;
                parent->corner_asked = 1;
            }
            return 0;
        }
        parent->corner_asked = 0;
        if (v->neighbour_objects && oracles_objects_killed_in_list(parent->killed_list, p->corner[u][s]) != oracles_objects_killed_enemies(v->guest, p->corner[u][s])) return 0;
        entry *e = ev_take_slot(v, group, p->corner[u][s], p->wanted, p->wanted_count);
        return entry_wanting(v, e) && run_from_parent(v, e, parent, plan_verticals[u], plan_verticals[u], 0, NULL, plan_verticals[u], p->key, p->key_len, p->live_primeable);
    }
    default: {
        /* A diagonal parent is two steps away: run again, if it must be, from the room above or below it.  While
         * that room's state is no start, the diagonal cannot be run again through it, and while the diagonal's
         * own is no start (Ages' grove 1:38 seen from 1:49), the room cannot be run from it: from beside instead,
         * the diagonal as it stands or the room of the second ring on the other side. */
        entry *vertical = p->have_first[plan_verticals[u]] ? ev_find_entry(v, group, p->first[plan_verticals[u]]) : NULL;
        entry *parent = ev_find_entry(v, group, p->diagonal[u][s]);
        if (!parent || !parent->valid) return 0;
        const int second = st->kind == STEP_SECOND_DIAGONAL;
        const uint8_t room = second ? p->second_diagonal[u][s] : p->beyond_diagonal[u][s];
        entry *e = ev_take_slot(v, group, room, p->wanted, p->wanted_count);
        if (!entry_wanting(v, e)) return 0;
        if ((vertical && vertical->valid && vertical->not_a_start) || parent->not_a_start) {
            const entry *other = second ? (p->have_second[s] ? ev_find_entry(v, group, p->second[s]) : NULL)
                                        : (p->have_beyond[u] ? ev_find_entry(v, group, p->beyond[u]) : NULL);
            const entry *const beside[3] = { parent, other, NULL };
            const OraclesGhostDirection ways[3] = { second ? plan_sides[s] : plan_verticals[u], second ? plan_verticals[u] : plan_sides[s], plan_sides[s] };
            if (run_from_beside(v, p, e, room, beside, ways)) return 1;
        }
        return run_from_parent(v, e, parent, second ? plan_sides[s] : plan_verticals[u], plan_sides[s], 1, vertical, plan_verticals[u], p->key, p->key_len, p->live_primeable);
    }
    }
}

static void add_step(const OraclesEnhancedView *v, run_step *out, unsigned *n, unsigned kind, unsigned u, unsigned s, uint8_t room)
{
    run_step *st = &out[(*n)++];
    st->kind = (uint8_t)kind; st->u = (uint8_t)u; st->s = (uint8_t)s; st->room = room;
    /* What the band of the last composition showed of the room, and how far its middle stands from Link. */
    const int32_t left = (int32_t)(room & 0x0fu) * SMALL_ROOM_W, top = (int32_t)(room >> 4u) * (int32_t)ORACLES_GHOST_AREA_HEIGHT;
    const int32_t shown_left = v->view_left + (int32_t)(v->size.width - v->shown_width) / 2, shown_top = v->view_top + (int32_t)(v->band_height - v->shown_height) / 2;
    const int32_t w = (left + SMALL_ROOM_W < shown_left + (int32_t)v->shown_width ? left + SMALL_ROOM_W : shown_left + (int32_t)v->shown_width) - (left > shown_left ? left : shown_left);
    const int32_t h = (top + (int32_t)ORACLES_GHOST_AREA_HEIGHT < shown_top + (int32_t)v->shown_height ? top + (int32_t)ORACLES_GHOST_AREA_HEIGHT : shown_top + (int32_t)v->shown_height) - (top > shown_top ? top : shown_top);
    st->shown = v->have_view && w > 0 && h > 0 ? w * h : 0;
    const int32_t dx = left + SMALL_ROOM_W / 2 - v->observation.world.link_x, dy = top + (int32_t)ORACLES_GHOST_AREA_HEIGHT / 2 - v->observation.world.link_y;
    st->distance = dx * dx + dy * dy;
}

/* The passes of the run order: the rooms the band shows or is about to, the
 * rest, and the drawn-back band's corners a scroll would bring into it. */
enum { PASS_SHOWN, PASS_REST, PASS_CORNERS };

/* The steps of a pass, in the order of the normal band: left and right, the
 * rooms beyond them, the reference room back from a neighbour, above and
 * below, the diagonals, then the second ring outside the first pass.  The
 * drawn-back band outdoors shows up to five columns and three rows (four in
 * its 4:3 shape, 344 lines tall): its second ring is in the first pass too, and the steps go by how much of
 * their room the band shows, then by how near it stands to Link.  Its corners
 * are in the first pass alone, and in a last pass of their own. */
static unsigned list_steps(const OraclesEnhancedView *v, const run_plan *p, int pass, run_step out[28])
{
    const int by_band = ev_drawn_back(v), visible_only = pass == PASS_SHOWN;
    unsigned n = 0;
    if (pass == PASS_CORNERS) {
        for (unsigned u = 0; u < 2; u++)
            for (unsigned s = 0; s < SIDES; s++) if (p->have_corner[u][s]) add_step(v, out, &n, STEP_CORNER, u, s, p->corner[u][s]);
    }
    else {
        for (unsigned s = 0; s < SIDES && !p->routed_ask; s++) if (p->have_first[plan_sides[s]]) add_step(v, out, &n, STEP_SIDE, 0, s, p->first[plan_sides[s]]);
        for (unsigned s = 0; s < SIDES; s++) if (p->have_second[s]) add_step(v, out, &n, STEP_SECOND, 0, s, p->second[s]);
        if (p->overworld && !visible_only) add_step(v, out, &n, STEP_REF, 0, 0, p->ref);
        for (unsigned u = 0; u < 2 && !p->routed_ask; u++) if (p->have_first[plan_verticals[u]]) add_step(v, out, &n, STEP_VERTICAL, u, 0, p->first[plan_verticals[u]]);
        for (unsigned u = 0; u < 2; u++)
            for (unsigned s = 0; s < SIDES; s++) if (p->have_diagonal[u][s]) add_step(v, out, &n, STEP_DIAGONAL, u, s, p->diagonal[u][s]);
        if (!visible_only || by_band) {
            for (unsigned u = 0; u < 2; u++) if (p->have_beyond[u]) add_step(v, out, &n, STEP_BEYOND, u, 0, p->beyond[u]);
            for (unsigned u = 0; u < 2; u++)
                for (unsigned s = 0; s < SIDES; s++) {
                    if (p->have_diagonal[u][s] && p->have_second_diagonal[u][s]) add_step(v, out, &n, STEP_SECOND_DIAGONAL, u, s, p->second_diagonal[u][s]);
                    if (p->have_diagonal[u][s] && p->have_beyond_diagonal[u][s]) add_step(v, out, &n, STEP_BEYOND_DIAGONAL, u, s, p->beyond_diagonal[u][s]);
                    if (p->have_corner[u][s] && visible_only) add_step(v, out, &n, STEP_CORNER, u, s, p->corner[u][s]);
                }
        }
    }
    if (!by_band) return n;
    int32_t shown[28] = { 0 }, distance[28] = { 0 };
    unsigned order[28];
    run_step normal[28];
    for (unsigned i = 0; i < n; i++) { shown[i] = out[i].shown; distance[i] = out[i].distance; normal[i] = out[i]; }
    oracles_enhanced_band_order(shown, distance, n, order);
    for (unsigned i = 0; i < n; i++) out[i] = normal[order[i]];
    return n;
}

void oracles_enhanced_band_order(const int32_t *shown, const int32_t *distance, unsigned count, unsigned *out)
{
    for (unsigned i = 0; i < count; i++) out[i] = i;
    for (unsigned i = 1; i < count; i++)   /* stable: equal rooms keep the normal order */
        for (unsigned j = i; j > 0 && (shown[out[j]] > shown[out[j - 1]] || (shown[out[j]] == shown[out[j - 1]] && distance[out[j]] < distance[out[j - 1]])); j--) {
            const unsigned t = out[j]; out[j] = out[j - 1]; out[j - 1] = t;
        }
}

/* How far past the band the first pass looks: in the drawn-back band, once
 * the rooms around the reference room are there (near_ready), the margin;
 * else the band alone, so that a room about to show does not go before a
 * room beside Link the next scroll brings (the band's fill). */
static int32_t soon_reach(OraclesEnhancedView *v, const run_plan *p)
{
    return ev_drawn_back(v) && near_ready(v, p) ? SOON_SHOWN_MARGIN : 0;
}

/* Whether a room has its entry, or a free slot to take: a corner run while
 * the ghost is idle never takes the place of a room the cache holds, which
 * a return to it would have to wait for again. */
static int slot_to_spare(OraclesEnhancedView *v, uint8_t group, uint8_t room)
{
    if (ev_find_entry(v, group, room)) return 1;
    for (unsigned i = 0; i < v->slot_count; i++) if (!v->slots[i].used) return 1;
    return 0;
}

/* One pass of the run order; returns 1 when a run was started. */
static int schedule_pass(OraclesEnhancedView *v, run_plan *p, int pass)
{
    const int visible_only = pass == PASS_SHOWN;
    if (pass != PASS_CORNERS && p->routed_ask && schedule_routed_pass(v, p, visible_only)) return 1;
    if (pass != PASS_CORNERS && p->routed && schedule_routed_beyond(v, p, visible_only)) return 1;
    run_step steps[28];
    const unsigned n = list_steps(v, p, pass, steps);
    const int32_t reach = visible_only ? soon_reach(v, p) : 0;
    for (unsigned i = 0; i < n; i++) {
        /* The first pass: only what the band shows (a black patch on screen) or is about to.  The corners' pass: those
         * a scroll would bring into the band, so that they are there before it does. */
        if (visible_only && !room_in_view(v, steps[i].room, reach)) continue;
        if (pass == PASS_CORNERS && (!room_a_scroll_away(v, steps[i].room) || !slot_to_spare(v, p->group, steps[i].room))) continue;
        if (run_step_now(v, p, &steps[i])) return 1;
    }
    return 0;
}

/* The rooms a slot taken for the other plan leaves alone: this plan's, or none. */
static void keep_plan(OraclesEnhancedView *v, const run_plan *p)
{
    v->keep_count = p ? p->wanted_count : 0;
    if (!p) return;
    v->keep_group = p->group;
    memcpy(v->keep, p->wanted, p->wanted_count);
}

/* Every frame: the terrains of the rooms around the reference room, so that
 * a transition finds the new neighbours ready.  One run at a time, in this
 * order: the rooms left and right, from the live state, when it can be primed
 * (normal play, no text, no cutscene); the rooms beyond them, from their
 * settled states; the reference room itself, back from a neighbour (it is the
 * neighbour of the next room when Link leaves); the rooms above and below,
 * from the live state; and their left and right neighbours, from their
 * settled states, so that a vertical transition finds its row ready.  Two
 * passes over the same order: first the rooms the world band shows right now
 * (a black patch on screen) or is about to show, then the rest, so that a room
 * Link walks toward after a warp comes before a room two away; then the
 * drawn-back band's corners a scroll would bring into it, while the ghost has
 * nothing else to run. */
static void schedule_idle(OraclesEnhancedView *v, run_plan *p, run_plan *q);

void ev_update_neighbours(OraclesEnhancedView *v)
{
    if (!v->ghost) return;
    ev_refresh_colours(v);
    /* 2 KiB of key: not on the stack of every frame, and the view's own. */
    if (!v->run_plan) v->run_plan = calloc(1, sizeof(run_plan));
    if (!v->run_plan) return;
    run_plan *const p = v->run_plan;
    p->key_len = ev_read_key(v, p->key);
    ev_invalidate_by_reads(v);
    ev_lift_parent_marks(v);
    ev_check_routing_keys(v);
    for (unsigned i = 0; i < v->slot_count; i++) {
        entry *e = &v->slots[i];
        if (e->used && e->valid && e->read_count > ENTRY_READS && !ev_entry_key_current(v, e, p->key, p->key_len, e->group, e->room, NULL)) ev_drop_entry(e);
    }
    /* Each ghost's run filed, then each idle ghost given a pre-run while a room loads. */
    for (unsigned l = 0; l < v->lane_count; l++) { ev_lane_serve(v, l); ev_advance_pending_run(v); }
    for (unsigned d = 0; d < 4; d++) v->shown_slot[d] = SLOTS;
    for (unsigned l = 0; l < v->lane_count; l++) { ev_lane_serve(v, l); ev_blind_prerun(v); }
    ev_lane_serve(v, 0);
    if (!ev_on_map(v)) { v->keep_count = 0; v->live_quiet = 0; return; }   /* a house's text and menu count as the game held */
    plan_runs(v, p, v->observer.ref_group, 0);
    /* Ages: the same place on the other side of the sea, planned too, so that
     * neither plan takes the other's slots. */
    uint8_t other = 0;
    const int sea = ev_sea_other_side(v, &other) && (v->sea_plan || (v->sea_plan = calloc(1, sizeof(run_plan))) != NULL);
    run_plan *const q = sea ? v->sea_plan : NULL;
    if (q) plan_runs(v, q, other, 1);
    keep_plan(v, q);
    /* Each cached room of the reference's group near it or in the second
     * ring: when the cache is full, a room of the second ring goes before a
     * near one, that of another place included (a room left for a moment,
     * returned to: the time portals of Ages). */
    for (unsigned i = 0; i < v->slot_count; i++) {
        entry *c = &v->slots[i];
        if (!c->used || c->routed || c->group != p->group) continue;   /* a routed entry stands for a direction, not for a room of the grid */
        for (unsigned w = 0; w < p->wanted_count; w++) if (p->wanted[w] == c->room) { c->near = w < p->near_count; break; }
    }
    mark_shown_sides(v, p);
    p->live_primeable = v->observation.playing && oracles_ghost_primeable(v->guest, NULL);
    v->live_quiet = p->live_primeable ? (v->live_quiet < OBJECT_REFRESH_QUIET ? v->live_quiet + 1u : v->live_quiet) : 0u;
    /* Each idle ghost takes the next run of the order; the one in flight in
     * another ghost is not wanted again (entry_wanting). */
    for (unsigned l = 0; l < v->lane_count; l++) {
        ev_lane_serve(v, l);
        if (!v->pending) schedule_idle(v, p, q);
    }
    ev_lane_serve(v, 0);
}

/* A pass of the other side of the sea's plan, which takes the key and the
 * live state of the reference room's. */
static int schedule_other_side(OraclesEnhancedView *v, run_plan *p, run_plan *q, int pass)
{
    memcpy(q->key, p->key, p->key_len);
    q->key_len = p->key_len;
    q->live_primeable = p->live_primeable;
    keep_plan(v, p);
    v->level_change = q->group;
    const int started = schedule_pass(v, q, pass);
    v->level_change = -1;
    keep_plan(v, q);
    return started;
}

/* The runs one idle ghost may start, in the order's passes: the band's
 * rooms, the rest, the rooms behind a routed one, the rooms the other side of
 * the sea shows, the corners a scroll would bring into the band, the rest of
 * the other side. */
static void schedule_idle(OraclesEnhancedView *v, run_plan *p, run_plan *q)
{
    /* The band's rooms, those still black before those run again for their
     * objects (still drawn, their people out of date), then the rest. */
    v->objects_refresh_deferred = 0;
    v->defer_objects_refresh = 1;
    const int shown = schedule_pass(v, p, PASS_SHOWN);
    v->defer_objects_refresh = 0;
    if (shown || (v->objects_refresh_deferred && schedule_pass(v, p, PASS_SHOWN)) || schedule_pass(v, p, PASS_REST)) return;
    /* Nothing else to run: the drawn-back band tries the rooms whose
     * only parent is a room the game may route itself, Ages' forest seen from
     * outside it; a run that lands elsewhere teaches the routing, and costs
     * the ghost a run it had no other use for. */
    if (ev_drawn_back(v)) {
        v->chain_routed_idle = 1;
        const int started = schedule_pass(v, p, PASS_REST);
        v->chain_routed_idle = 0;
        if (started) return;
    }
    /* Then the other side of the sea, in the same order, the rooms the band
     * shows first: after a dive or a return to the surface where Link stands,
     * the band finds them there.  Its runs from the live state cross the
     * surface first (ev_start_job); the rooms beyond are chained as on any map,
     * and so are the room's neighbours once the room under Link is there. */
    if (q && schedule_other_side(v, p, q, PASS_SHOWN)) return;
    if (schedule_pass(v, p, PASS_CORNERS) || !q) return;
    schedule_other_side(v, p, q, PASS_REST);
}
