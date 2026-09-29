#include "view_internal.h"

/* ---- rooms of the map -------------------------------------------------------------------- */

/* The room next to the reference room in a dungeon: the floor's 8x8 layout
 * (w2DungeonLayout, $40 bytes a floor) at the map position beside the
 * room's (wDungeonMapPosition, sampled by the observer), the game's own
 * lookup for its transitions (getRoomInDungeon); 0 in the layout is no
 * room.  A dungeon's rooms are not placed by their index. */
static int dungeon_room_toward(const OraclesEnhancedView *v, uint8_t room, OraclesGhostDirection dir, uint8_t *out)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const OraclesEnhancedObserver *o = &v->observer;
    if (!o->have_ref_cell || o->ref_cell_group != o->ref_group || o->ref_cell_room != room) return 0;
    unsigned col = o->ref_cell & 7u, row = (o->ref_cell >> 4u) & 7u;
    switch (dir) {
    case ORACLES_DIR_LEFT: if (col == 0) return 0; col--; break;
    case ORACLES_DIR_RIGHT: if (col == 7u) return 0; col++; break;
    case ORACLES_DIR_UP: if (row == 0) return 0; row--; break;
    case ORACLES_DIR_DOWN: if (row == 7u) return 0; row++; break;
    default: return 0;
    }
    const unsigned floor = oracles_guest_read8(v->guest, t->dungeon_floor) & 3u;
    const OraclesGuestSym cell = { t->dungeon_layout.bank, (uint16_t)(t->dungeon_layout.addr + floor * 0x40u + row * 8u + col) };
    const uint8_t beside = oracles_guest_read8(v->guest, cell);
    if (beside == 0) return 0;
    *out = beside;
    return 1;
}

/* The room next to `room` in a direction on the reference room's map, within
 * its width and height; 0 when there is none. */
int ev_room_toward(const OraclesEnhancedView *v, uint8_t room, OraclesGhostDirection dir, uint8_t *out)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    if (v->observation.large_grid && oracles_guest_read8(v->guest, t->dungeon_index) != 0xffu) return dungeon_room_toward(v, room, dir, out);
    return ev_room_toward_in(v, v->observer.ref_group, room, dir, out);
}

/* The tileset map rules apply with the ghost, which read the rooms on each map from the ROM: without it (it could
 * not be made), the grid of main, every room of groups 0 and 1 on one map. */
static int map_rules(const OraclesEnhancedView *v)
{
    return v->ghost && oracles_compat_tileset_map_rules(oracles_guest_profile(v->guest));
}

int ev_off_map(const OraclesEnhancedView *v, uint8_t group, uint8_t room)
{
    if ((group & 7u) > 1u || !map_rules(v)) return 0;
    return !oracles_ghost_room_on_map(v->ghost, group, room);
}

unsigned ev_map_width(const OraclesEnhancedView *v, uint8_t group)
{
    const OraclesCompatProfile *profile = oracles_guest_profile(v->guest);
    return map_rules(v) ? oracles_compat_group_map_width(profile, group) : oracles_compat_map_width(profile);
}

unsigned ev_map_height(const OraclesEnhancedView *v, uint8_t group)
{
    const OraclesCompatProfile *profile = oracles_guest_profile(v->guest);
    return map_rules(v) ? oracles_compat_group_map_height(profile, group) : oracles_compat_map_height(profile);
}

/* The same on the grid of a group of small rooms, the reference's or the other side of the sea's. */
int ev_room_toward_in(const OraclesEnhancedView *v, uint8_t group, uint8_t room, OraclesGhostDirection dir, uint8_t *out)
{
    const unsigned col = room & 0x0fu, row = room >> 4u;
    /* The interiors use the whole 16x16 grid, and so do an overworld group's rooms off its map. */
    const int interior = (group & 7u) >= 2u || ev_off_map(v, group, room);
    const unsigned map_width = ev_map_width(v, group), map_height = ev_map_height(v, group);
    const unsigned width = interior ? 16u : map_width ? map_width : 16u;
    const unsigned height = interior ? 16u : map_height ? map_height : 16u;
    uint8_t beside;
    switch (dir) {
    case ORACLES_DIR_LEFT: if (col == 0) return 0; beside = (uint8_t)((row << 4) | (col - 1u)); break;
    case ORACLES_DIR_RIGHT: if (col + 1u >= width) return 0; beside = (uint8_t)((row << 4) | (col + 1u)); break;
    case ORACLES_DIR_UP: if (row == 0) return 0; beside = (uint8_t)(((row - 1u) << 4) | col); break;
    case ORACLES_DIR_DOWN: if (row + 1u >= height) return 0; beside = (uint8_t)(((row + 1u) << 4) | col); break;
    default: return 0;
    }
    /* On the map, a room off it beside (Subrosia's that are not outdoors) is none of the map's. */
    if (!interior && ev_off_map(v, group, beside)) return 0;
    *out = beside;
    return 1;
}

int ev_room_beside(const OraclesEnhancedView *v, uint8_t room, unsigned side, uint8_t *out)
{
    return ev_room_toward(v, room, side == 0 ? ORACLES_DIR_LEFT : ORACLES_DIR_RIGHT, out);
}

/* The small rooms of groups 0 to 3 are indexed on a 16-wide grid.  The
 * overworld maps (groups 0 and 1) are a map: every adjacent index is the
 * room beside.  The interiors (groups 2 and 3) are not: adjacent indices are
 * unrelated rooms unless the two rooms open onto each other, which the
 * game's scrolling transition takes at face value (it goes to the adjacent
 * index whenever Link crosses the edge), so an open shared edge is the
 * criterion, read from both rooms' collisions. */
/* Ages' Maku tree is a column of group 2 rooms the game files under minimap
 * group 2 (bank2.s, the minimap popup: every other interior keeps the
 * overworld's group), reached by warps and climbs: its rooms beside on the
 * grid are not what a player expects beside the tree.  They stand alone,
 * centred, without neighbours, deliberately. */

/* The Maku tree's screen, in both games: the room holds the tree itself,
 * an interaction of id $87 (INTERAC_MAKU_TREE) in the object table; and, in
 * Ages, the tree's inside, the rooms the game files under minimap group 2.
 * Alone, centred, without neighbours, deliberately, as the tree's inside. */
int ev_isolated_room(const OraclesEnhancedView *v)
{
    if (!v->observer.have_reference) return 0;
    if (v->ref_maku && v->ref_maku_group == v->observer.ref_group && v->ref_maku_room == v->observer.ref_room) return 1;
    if (!oracles_compat_maku_isolation_rules(oracles_guest_profile(v->guest))
        || (v->observer.ref_group & 7u) != 2u) return 0;
    /* The open sea is filed under that minimap group too, and it is a map of
     * rooms, not a room standing alone; a house under water is one. */
    if (ev_open_water(v, v->observer.ref_group, v->observer.ref_room)) return 0;
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    return oracles_guest_read8(v->guest, t->minimap_group) == 2u;
}

static int maku_tree_here(OraclesEnhancedView *v)
{
    if (!oracles_compat_maku_tree_rules(oracles_guest_profile(v->guest))) return 0;
    for (unsigned i = 0; i < 16u; i++) {
        const uint8_t *obj = oracles_guest_object(v->guest, i, 1);   /* the interaction of slot i */
        /* Subid 0 is the tree itself (both games); the other subids are its parts elsewhere (the gate south of it in Seasons). */
        if (obj && obj[ORACLES_OBJ_ENABLED] != 0 && obj[ORACLES_OBJ_ID] == 0x87u && obj[ORACLES_OBJ_SUBID] == 0u) return 1;
    }
    return 0;
}

/* A room of the grid, small or large, shown in the world (alone or with neighbours). */
int ev_on_grid(const OraclesEnhancedView *v)
{
    return v->observer.have_reference && ((v->observation.grid && (v->observer.ref_group & 7u) <= 3u) || v->observation.large_grid);
}

/* On the ghost's map: neighbours are run and shown. */
int ev_on_map(const OraclesEnhancedView *v)
{
    return ev_on_grid(v) && !ev_isolated_room(v);
}

/* Ages' open seas (a tileset under water and outdoors) are maps of rooms side
 * by side inside the interior groups: two rooms of the sea are neighbours by
 * their index, a rock between them and all, where an interior asks its rooms
 * to open onto each other.  The sea stops where its rooms stop, and the houses
 * under it (their tilesets indoors) stand alone like any house. */
int ev_open_water(const OraclesEnhancedView *v, uint8_t group, uint8_t room)
{
    return v->ghost && oracles_ghost_room_open_water(v->ghost, group, room);
}

int ev_on_overworld(const OraclesEnhancedView *v)
{
    return ev_on_map(v) && (v->observer.ref_group & 7u) <= 1u && !ev_off_map(v, v->observer.ref_group, v->observer.ref_room);
}

/* Ages: the group of the other side of the sea at the reference room, the
 * room of the same index there (a dive takes Link two groups down, a return
 * to the surface two up: checkForUnderwaterTransition, docs/GAME_HOOKS.md, section 3).  From
 * the surface while Link swims, over a room whose other side is the open
 * sea; from the open sea always.  Returns 0 elsewhere. */
int ev_sea_other_side(const OraclesEnhancedView *v, uint8_t *group)
{
    if (!oracles_compat_sea_level_rules(oracles_guest_profile(v->guest)) || !ev_on_map(v) || v->observation.large_grid) return 0;
    const uint8_t g = v->observer.ref_group & 7u, room = v->observer.ref_room;
    if (g <= 1u) {
        if (!ev_open_water(v, (uint8_t)(g + SEA_LEVEL_GROUPS), room)) return 0;
        if (oracles_guest_read8(v->guest, oracles_guest_tables(v->guest)->link_swimming_state) == 0) return 0;
        *group = (uint8_t)(g + SEA_LEVEL_GROUPS);
        return 1;
    }
    if (g <= 3u && ev_open_water(v, g, room)) { *group = (uint8_t)(g - SEA_LEVEL_GROUPS); return 1; }
    return 0;
}

/* A room whose transitions the game routes itself (mapTransitionGroupTable,
 * read from the user's ROM at the ghost's creation; docs/GAME_HOOKS.md, section 4.3). */
int ev_self_routed(const OraclesEnhancedView *v, uint8_t group, uint8_t room)
{
    return v->ghost && oracles_ghost_room_self_routed(v->ghost, group, room);
}

/* Whether the room in play is one of them, and the band a map of small rooms
 * (a large room deliberately shows no neighbour anyway): its four
 * directions are then asked of the ghost from the room in play and from
 * nowhere else, never chained, because the answer can depend on the live
 * state (the sequence of directions the Lost Woods counts). */
int ev_routed_candidate(const OraclesEnhancedView *v)
{
    return ev_on_map(v) && !v->observation.large_grid && ev_self_routed(v, v->observer.ref_group, v->observer.ref_room);
}

/* Whether a room has been seen routing somewhere else than the grid's
 * neighbour.  A room of the table routes only under the game's own
 * condition — Ages' forest scrambler routes nothing once the forest is
 * unscrambled — and the host does not read that condition: it asks the
 * ghost, and holds what the answer showed. */
int ev_routes_elsewhere(const OraclesEnhancedView *v, uint8_t group, uint8_t room)
{
    /* Seasons' rooms of the table route elsewhere for good, under no condition
     * (the Lost Woods sends Link back into itself three ways of four, the sword
     * upgrade room into the Lost Woods): known before any answer, so that the
     * grid's rooms are never drawn around them.  So are the rooms of a table
     * whose routines are the game's own (Kinomi's): the view knows none of
     * their conditions, and a room it chained through would be delivered
     * elsewhere (its forest scrambler, beside its seasons). */
    const OraclesCompatProfile *profile = oracles_guest_profile(v->guest);
    if ((oracles_compat_lost_woods_rules(profile) || oracles_compat_own_routing_rules(profile)) && ev_self_routed(v, group, room)) return 1;
    return v->routes_elsewhere[group & 7u][room] != 0;
}

/* What an answer taught: the room the ghost loaded leaving `from_room` that
 * way is the grid's neighbour, or it is not. */
void ev_note_routing(OraclesEnhancedView *v, uint8_t group, uint8_t from_room, OraclesGhostDirection dir, uint8_t got)
{
    if (!ev_self_routed(v, group, from_room)) return;   /* only a room of the table routes; anything else is a warp or a door */
    uint8_t target;
    /* Nothing on the grid that way: the answer has nothing to be measured
     * against, and the standard transition itself would leave the map (it
     * wraps to the row before, where the camera never goes).  Such a
     * direction teaches nothing, either way. */
    if (!ev_room_toward(v, from_room, dir, &target)) return;
    const int elsewhere = target != got;
    uint8_t *const byte = &v->routes_elsewhere[group & 7u][from_room];
    const uint8_t bit = (uint8_t)(1u << (unsigned)dir);
    if (elsewhere) *byte |= bit; else *byte &= (uint8_t)~bit;
}

/* The rooms around a room known to route elsewhere are a zone of their own:
 * the band shows what the game would load in each direction, where Link
 * would arrive, and not the grid's neighbours, where he would not. */
int ev_in_routed_zone(const OraclesEnhancedView *v)
{
    return ev_routed_candidate(v) && ev_routes_elsewhere(v, v->observer.ref_group, v->observer.ref_room);
}

void oracles_enhanced_routed_order(const int32_t gap[4], const int on_map[4], unsigned out[4])
{
    enum { UP = 0, RIGHT = 1, DOWN = 2, LEFT = 3 };
    const unsigned near_side = gap[LEFT] < gap[RIGHT] ? LEFT : RIGHT, near_row = gap[UP] < gap[DOWN] ? UP : DOWN;
    const unsigned ranked[4] = { near_side, near_side == LEFT ? RIGHT : LEFT, near_row, near_row == UP ? DOWN : UP };
    unsigned n = 0;
    for (unsigned pass = 0; pass < 2; pass++)
        for (unsigned i = 0; i < 4; i++)
            if ((on_map[ranked[i]] != 0) == (pass == 0)) out[n++] = ranked[i];
}

/* A step on a grid of `width` columns and `height` rows; 0 off it. */
static int grid_step(uint8_t room, unsigned dir, unsigned width, unsigned height, uint8_t *out)
{
    const unsigned col = room & 0x0fu, row = room >> 4u;
    switch (dir & 3u) {
    case ORACLES_DIR_UP: if (row == 0) return 0; *out = (uint8_t)(room - 0x10u); return 1;
    case ORACLES_DIR_RIGHT: if (col + 1u >= width) return 0; *out = (uint8_t)(room + 1u); return 1;
    case ORACLES_DIR_DOWN: if (row + 1u >= height) return 0; *out = (uint8_t)(room + 0x10u); return 1;
    default: if (col == 0) return 0; *out = (uint8_t)(room - 1u); return 1;
    }
}

unsigned oracles_enhanced_routed_beyond(uint8_t ref, unsigned dir, uint8_t answer, int answer_routed, unsigned map_width, unsigned map_height,
                                        uint8_t out[5], int have[5])
{
    for (unsigned i = 0; i < 5u; i++) have[i] = 0;
    uint8_t beside;
    /* An answer the game routes itself (the Lost Woods again) goes on by its
     * own sequence, and one that is not the grid's neighbour has no grid around it here. */
    if (answer_routed || !grid_step(ref, dir, map_width, map_height, &beside) || beside != answer) return 0;
    const unsigned across[2] = { dir & 1u ? ORACLES_DIR_UP : ORACLES_DIR_LEFT, dir & 1u ? ORACLES_DIR_DOWN : ORACLES_DIR_RIGHT };
    have[0] = grid_step(answer, dir, map_width, map_height, &out[0]);
    for (unsigned k = 0; k < 2u; k++) {
        have[1u + k] = grid_step(answer, across[k], map_width, map_height, &out[1u + k]);
        have[3u + k] = have[0] && grid_step(out[0], across[k], map_width, map_height, &out[3u + k]);
    }
    unsigned n = 0;
    for (unsigned i = 0; i < 5u; i++) n += have[i] != 0;
    return n;
}

int oracles_enhanced_routed_answer_holds(unsigned answer_dir, int taking, const uint8_t *asked_key, const uint8_t *live_key, size_t key_len)
{
    if (taking >= 0 && (unsigned)taking == (answer_dir & 3u)) return 1;
    return memcmp(asked_key, live_key, key_len) == 0;
}

/* Whether a room of the grid stands beyond an answer of the room in play, the
 * game routing it itself (oracles_enhanced_routed_beyond): shown while that
 * answer is drawn, the direction it hangs from in `dir_out`.  When the
 * direction is asked again or answers another room, the room leaves the band. */
int ev_routed_beyond_shown(const OraclesEnhancedView *v, uint8_t room, OraclesGhostDirection *dir_out)
{
    const uint8_t group = v->observer.ref_group, ref = v->observer.ref_room;
    for (unsigned i = 0; i < v->slot_count; i++) {
        const entry *r = &v->slots[i];
        if (!r->used || !r->routed || r->group != group || r->routed_from != ref || !ev_entry_drawable(v, r)) continue;
        uint8_t rooms[5];
        int have[5];
        if (!oracles_enhanced_routed_beyond(ref, r->routed_dir, r->room, ev_self_routed(v, group, r->room), ev_map_width(v, group), ev_map_height(v, group), rooms, have)) continue;
        for (unsigned k = 0; k < 5u; k++)
            if (have[k] && rooms[k] == room) { if (dir_out) *dir_out = (OraclesGhostDirection)r->routed_dir; return 1; }
    }
    return 0;
}

/* The order of the room in play's routed directions, from where Link stands in it. */
void ev_routed_run_order(const OraclesEnhancedView *v, uint8_t ref, unsigned out[4])
{
    const OraclesEnhancedWorld *w = &v->observation.world;
    const int32_t width = v->observation.large ? (int32_t)LARGE_ROOM_W : SMALL_ROOM_W;
    const int32_t height = v->observation.large ? (int32_t)LARGE_ROOM_H : (int32_t)ORACLES_GHOST_AREA_HEIGHT;
    const int32_t x = w->link_x - w->origin_x, y = w->link_y - w->origin_y;   /* in the room itself, not its bounds, which take in its neighbours */
    const int32_t gap[4] = { y, width - x, height - y, x };
    int on_map[4];
    for (unsigned d = 0; d < 4; d++) { uint8_t beyond; on_map[d] = ev_room_toward(v, ref, (OraclesGhostDirection)d, &beyond); }
    oracles_enhanced_routed_order(gap, on_map, out);
}

/* Where the band draws what the game would load leaving the room in play
 * that way: beside the room in play, where Link goes, and not at the place
 * the room's own index would give it on the grid. */
void ev_routed_place(uint8_t ref, OraclesGhostDirection dir, int32_t *left, int32_t *top)
{
    const int32_t w = SMALL_ROOM_W, h = (int32_t)ORACLES_GHOST_AREA_HEIGHT;
    *left = (int32_t)(ref & 0x0fu) * w + (dir == ORACLES_DIR_RIGHT ? w : dir == ORACLES_DIR_LEFT ? -w : 0);
    *top = (int32_t)(ref >> 4u) * h + (dir == ORACLES_DIR_DOWN ? h : dir == ORACLES_DIR_UP ? -h : 0);
}

/* Whether room A opens onto room B lying in direction `dir`: a free cell on
 * A's edge facing a free cell on B's edge (the same column or row). */
static int edge_connected(const uint8_t *a, const uint8_t *b, OraclesGhostDirection dir)
{
    if (!a || !b) return 0;
    switch (dir) {
    case ORACLES_DIR_RIGHT: for (unsigned r = 0; r < 8u; r++) if (a[r * 16u + 9u] == 0 && b[r * 16u] == 0) return 1; return 0;
    case ORACLES_DIR_LEFT: for (unsigned r = 0; r < 8u; r++) if (a[r * 16u] == 0 && b[r * 16u + 9u] == 0) return 1; return 0;
    case ORACLES_DIR_DOWN: for (unsigned c = 0; c < 10u; c++) if (a[7u * 16u + c] == 0 && b[c] == 0) return 1; return 0;
    case ORACLES_DIR_UP: for (unsigned c = 0; c < 10u; c++) if (a[c] == 0 && b[7u * 16u + c] == 0) return 1; return 0;
    default: return 0;
    }
}

/* On the overworld every cached room of the group is at its place; in an
 * interior only a room the reference room opens onto. */
int ev_entry_connected(const OraclesEnhancedView *v, const entry *e, OraclesGhostDirection *dir_out)
{
    /* A large room (a dungeon) deliberately shows no neighbour,
     * except the room a scroll is entering, whole, as the game's window
     * slides into it: the game's own transition, the room extended. */
    if (v->observation.large_grid) {
        if (!v->observation.in_scroll || e->room != v->observation.room || e->group != v->observation.group) return 0;
        if (dir_out) *dir_out = (OraclesGhostDirection)(v->observation.scroll_direction & 3u);
        return 1;
    }
    /* In the zone the game routes itself, a neighbour is what it would load
     * that way, and nothing else: the grid's rooms are where Link would not
     * arrive. */
    if (e->routed) {
        if (!ev_in_routed_zone(v) || e->group != v->observer.ref_group || e->routed_from != v->observer.ref_room) return 0;
        if (dir_out) *dir_out = (OraclesGhostDirection)e->routed_dir;
        return 1;
    }
    /* The grid again beyond an answer that is an ordinary room, read through it: Link
     * reaches those rooms from the room in play only through that answer. */
    if (ev_in_routed_zone(v)) return e->room == v->observer.ref_room || ev_routed_beyond_shown(v, e->room, dir_out);
    /* The room itself: what the window leaves behind; on the map, a room off it is none of the map's. */
    if (ev_on_overworld(v)) return e->room == v->observer.ref_room || !ev_off_map(v, e->group, e->room);
    if (e->room == v->observer.ref_room) return 1;
    if (ev_open_water(v, v->observer.ref_group, v->observer.ref_room)) {
        /* Under water: every room of the sea at its place on the grid, as on an
         * overworld (the sea's own extent keeps the camera inside it). */
        if (!ev_open_water(v, e->group, e->room)) return 0;
        for (unsigned d = 0; d < 4; d++) {
            uint8_t target;
            if (ev_room_toward(v, v->observer.ref_room, (OraclesGhostDirection)d, &target) && target == e->room && dir_out) *dir_out = (OraclesGhostDirection)d;
        }
        return 1;
    }
    if (v->have_came_from && e->room == v->came_from_room) { if (dir_out) *dir_out = v->came_from_dir; return 1; }
    if (!v->have_ref_collisions || v->ref_collisions_group != v->observer.ref_group || v->ref_collisions_room != v->observer.ref_room) return 0;
    for (unsigned d = 0; d < 4; d++) {
        uint8_t target;
        if (!ev_room_toward(v, v->observer.ref_room, (OraclesGhostDirection)d, &target) || target != e->room) continue;
        if (!edge_connected(v->ref_collisions, e->collisions, (OraclesGhostDirection)d)) return 0;
        if (dir_out) *dir_out = (OraclesGhostDirection)d;
        return 1;
    }
    return 0;
}

/* The reference room's collisions, in normal play, and the interior's open
 * edges for the camera: the directions whose cached room is connected. */
void ev_update_reference_edges(OraclesEnhancedView *v)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    if (!ob->in_transition && ob->group == v->observer.ref_group && ob->room == v->observer.ref_room) {
        const OraclesGuestTables *t = oracles_guest_tables(v->guest);
        const uint8_t *collisions = oracles_guest_ptr(v->guest, t->room_collisions, ORACLES_GHOST_LAYOUT_BYTES);
        if (collisions) {
            memcpy(v->ref_collisions, collisions, sizeof v->ref_collisions);
            v->have_ref_collisions = 1;
            v->ref_collisions_group = ob->group; v->ref_collisions_room = ob->room;
        }
        v->ref_maku = maku_tree_here(v);
        v->ref_maku_group = ob->group; v->ref_maku_room = ob->room;
    }
    /* A change of reference within an epoch is a scrolling transition (a
     * warp opens a new epoch): the room left lies opposite to the scroll's
     * direction (a dungeon's rooms are not laid out by their index). */
    if (ob->in_scroll) { v->last_scroll_dir = ob->scroll_direction & 3u; v->have_last_scroll_dir = 1; }
    if (v->have_last_ref && (v->last_ref_group != v->observer.ref_group || v->last_ref_room != v->observer.ref_room)) {
        v->have_came_from = 0;
        if (v->last_ref_group == v->observer.ref_group && v->last_ref_epoch == ob->epoch && v->have_last_scroll_dir) {
            v->came_from_dir = (OraclesGhostDirection)((v->last_scroll_dir + 2u) & 3u);
            v->came_from_room = v->last_ref_room;
            v->have_came_from = 1;
        }
    } else if (!v->have_last_ref) v->have_came_from = 0;
    v->have_last_ref = 1;
    v->last_ref_group = v->observer.ref_group; v->last_ref_room = v->observer.ref_room; v->last_ref_epoch = ob->epoch;
    v->observer.isolated = ev_isolated_room(v) ? ORACLES_ENHANCED_ISOLATED
                         : ev_on_grid(v) && ev_off_map(v, v->observer.ref_group, v->observer.ref_room) ? ORACLES_ENHANCED_INDOORS : 0;
    /* Under the tileset map rules the camera stops at the reference group's own map (Subrosia's 11 x 8). */
    if (map_rules(v)) {
        v->observer.map_width = ev_map_width(v, v->observer.ref_group);
        v->observer.map_height = ev_map_height(v, v->observer.ref_group);
        v->observer.map_left = oracles_compat_group_map_left(oracles_guest_profile(v->guest), v->observer.ref_group);
        v->observer.map_top = oracles_compat_group_map_top(oracles_guest_profile(v->guest), v->observer.ref_group);
    }
    unsigned edges = 0;
    if (ev_on_grid(v) && !ev_on_overworld(v)) {
        /* The room a scroll came from, and the one a scroll goes to: the
         * extent takes them in so that the camera is continuous through the
         * scroll, neighbours shown or not. */
        if (v->have_came_from) edges |= 1u << v->came_from_dir;
        if (ob->in_scroll) edges |= 1u << (ob->scroll_direction & 3u);
    }
    /* In the drawn-back view the sea is the extent, as the map is outdoors
     * (oracles_enhanced_world_extend_to_sea); the normal band keeps the
     * rooms of the sea beside this one. */
    v->observer.sea = ev_on_map(v) && ev_open_water(v, v->observer.ref_group, v->observer.ref_room)
                   && oracles_ghost_open_water_extent(v->ghost, v->observer.ref_group, v->observer.sea_extent);
    if (ev_on_map(v) && ev_open_water(v, v->observer.ref_group, v->observer.ref_room)) {
        /* The sea's extent: the rooms of the sea beside this one, cached or not. */
        for (unsigned d = 0; d < 4; d++) {
            uint8_t target;
            if (ev_room_toward(v, v->observer.ref_room, (OraclesGhostDirection)d, &target) && ev_open_water(v, v->observer.ref_group, target)) edges |= 1u << d;
        }
    }
    if (ev_on_map(v) && !ev_on_overworld(v) && !ev_open_water(v, v->observer.ref_group, v->observer.ref_room)) {
        for (unsigned i = 0; i < v->slot_count; i++) {
            const entry *e = &v->slots[i];
            OraclesGhostDirection dir;
            if (e->used && e->valid && e->group == v->observer.ref_group && e->room != v->observer.ref_room && ev_entry_connected(v, e, &dir)) edges |= 1u << dir;
        }
    }
    v->observer.open_edges = edges;
}
