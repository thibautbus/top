#include "view_internal.h"

/* ---- the ghost's runs ---------------------------------------------------------------------- */

/* The rooms worth a slot now: the reference room, its four neighbours, the
 * two beyond left and right, the four diagonals. */
static unsigned wanted_rooms(const OraclesEnhancedView *v, uint8_t ref, uint8_t *wanted)
{
    unsigned count = 0;
    uint8_t first[4];
    int have_first[4];
    wanted[count++] = ref;
    for (unsigned d = 0; d < 4; d++) {
        have_first[d] = ev_room_toward(v, ref, (OraclesGhostDirection)d, &first[d]);
        if (have_first[d]) wanted[count++] = first[d];
    }
    for (unsigned s = 0; s < 2; s++) {
        const OraclesGhostDirection side = s == 0 ? ORACLES_DIR_LEFT : ORACLES_DIR_RIGHT;
        uint8_t room;
        if (have_first[side] && ev_room_toward(v, first[side], side, &room)) wanted[count++] = room;
        if (have_first[ORACLES_DIR_UP] && ev_room_toward(v, first[ORACLES_DIR_UP], side, &room)) wanted[count++] = room;
        if (have_first[ORACLES_DIR_DOWN] && ev_room_toward(v, first[ORACLES_DIR_DOWN], side, &room)) wanted[count++] = room;
    }
    return count;
}


/* A blind pre-run's result belongs beside the room the ghost primed in,
 * whichever room the load reached; anything else (a map edge, a warp) is
 * dropped.  Takes the slot; returns 0 when the result is dropped. */
static int file_blind_result(OraclesEnhancedView *v, const OraclesGhostResult *r, int rc)
{
    v->pending_blind = 0;
    uint8_t target;
    if (rc != 1 || r->status != ORACLES_GHOST_OK || !r->settled || (r->from_group & 7u) > 1u
        || ev_self_routed(v, r->from_group, r->from_room)   /* the room it primed in routes its own transitions: its neighbours are asked from it, one by one */
        || !ev_room_toward(v, r->from_room, v->pending_dir, &target) || r->group != r->from_group || r->room != target) { v->blind_dropped++; return 0; }
    uint8_t wanted[11];
    const unsigned wanted_count = wanted_rooms(v, r->from_room, wanted);
    entry *e = ev_take_slot(v, r->group, r->room, wanted, wanted_count);
    if (!e) { v->blind_dropped++; return 0; }
    v->pending_slot = (unsigned)(e - v->slots);
    v->pending_group = e->group;
    v->pending_room = e->room;
    v->blind_results++;
    return 1;
}


/* A scroll that came back into the room it left (the game's loop, see
 * record_failure): not the room expected that way, but the room itself,
 * whole, which a loop's scroll shows entering; after a load inside the room
 * nothing else would compute it.  Returns the entry to fill with it, or NULL
 * when the room's own entry is already current. */
static entry *loop_return_entry(OraclesEnhancedView *v, entry *e, const OraclesGhostResult *r)
{
    e->valid = 0; e->failed = 1; e->failed_at = v->frame;
    v->failed++;
    v->failure_reasons[3]++;
    const uint8_t wanted[2] = { r->room, v->pending_room };
    entry *own = ev_take_slot(v, r->group, r->room, wanted, 2);
    if (!own || (own->valid && ev_entry_key_current(v, own, r->key, r->key_len, own->group, own->room))) return NULL;
    v->pending_room = r->room;
    return own;
}

/* Seasons: a room delivered in another season than the live one, the band's
 * (a run started before the season changed, or from a state kept in the old
 * one).  Not kept; the live capture or the flash stands meanwhile.  A run
 * that kept the season of its start saw the season change since (the rod, a
 * frame after): tried again at once; otherwise at the pace of a failure.
 * Returns 1 when the result is rejected. */
static int reject_other_season(OraclesEnhancedView *v, entry *e, const OraclesGhostResult *r)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    if (!(oracles_compat_seasons_rules(oracles_guest_profile(v->guest)) && (r->group & 7u) == 0u && oracles_ghost_area_holds_season(v->ghost, r->room_pack)
          && (oracles_guest_read8(v->guest, t->active_group) & 7u) == 0u
          && oracles_guest_read8(v->guest, t->room_state_modifier) != r->room_state_modifier && v->fade_effective == 0)) return 0;
    const long at = ev_key_offset(v, t->room_state_modifier.addr);
    e->failed = !(at >= 0 && (size_t)at < r->key_len && r->key[at] == r->room_state_modifier); e->failed_at = v->frame;
    /* A room run again whose runs keep coming back in another season gives up like one whose runs fail. */
    if (e->refresh && ++e->refresh_failures >= 2u) { e->refresh = 0; e->valid = 0; v->refresh_given_up++; }
    v->failed++;
    v->failure_reasons[4]++;
    v->season_rejected++;
    return 1;
}

/* The key bytes its substitutions read, with the values the ghost saw when
 * it primed: if the live instance has moved on since, the next frame's
 * invalidation says so, and no result is ever kept under values it did not
 * see.  A result is only accepted whole: if the trace overflowed, every key
 * byte counts (the entry cannot know which it missed).  Seasons: the season
 * is read by the room's load (its layout, tileset and palettes), before the
 * substitutions the trace covers; the ghost holds the live season across
 * areas, every overworld room depends on it, ev_invalidate_by_reads compares. */
static void record_entry_reads(OraclesEnhancedView *v, entry *e, const OraclesGhostResult *r)
{
    e->read_count = 0;
    int overflow = r->reads_dropped != 0;
    for (unsigned i = 0; i < r->read_count && !overflow; i++) {
        const uint16_t a = r->reads[i];
        if (!ev_tracked_read(v, a)) continue;
        if (e->read_count == ENTRY_READS) { overflow = 1; break; }
        const long at = ev_key_offset(v, a);
        e->read_addr[e->read_count] = a;
        e->read_value[e->read_count] = at >= 0 && (size_t)at < r->key_len ? r->key[at] : ev_live_byte(v, a);
        e->read_count++;
    }
    e->season = r->room_state_modifier;
    /* Not the rooms of an area whose season never changes (no stump, or packs $f0 and up: Natzu's byte is its companion). */
    e->season_dependent = oracles_compat_seasons_rules(oracles_guest_profile(v->guest)) && (r->group & 7u) == 0u && oracles_ghost_area_holds_season(v->ghost, r->room_pack);
    e->key_len = r->key_len;
    memcpy(e->key_snapshot, r->key, r->key_len);
    if (overflow) e->read_count = ENTRY_READS + 1u;   /* compare the whole key instead */
}

/* The room delivered: its terrain, what the view needs to render it again, and its settled state for the rooms beyond it. */
static void accept_result(OraclesEnhancedView *v, entry *e, const OraclesGhostResult *r)
{
    memcpy(e->game_area, r->game_area, sizeof e->game_area);
    memcpy(e->layout, r->layout, sizeof e->layout);
    memcpy(e->collisions, r->collisions, sizeof e->collisions);
    memcpy(e->tiles, r->tiles, sizeof e->tiles);
    memcpy(e->bg_palettes, r->bg_palettes, sizeof e->bg_palettes);
    memcpy(e->base_bg_palettes, r->base_bg_palettes, sizeof e->base_bg_palettes);
    e->palette_offset = r->palette_offset;
    /* Its objects and their sprites, when the run captured them. */
    memcpy(e->oam, r->oam, sizeof e->oam);
    memcpy(e->obj_palettes, r->obj_palettes, sizeof e->obj_palettes);
    e->tag_count = r->tag_count < ORACLES_SPRITE_TAGS ? r->tag_count : ORACLES_SPRITE_TAGS;
    memcpy(e->tags, r->tags, e->tag_count * sizeof e->tags[0]);
    memcpy(e->oam_before, r->oam_before, sizeof e->oam_before);
    e->tag_count_before = r->tag_count_before < ORACLES_SPRITE_TAGS ? r->tag_count_before : ORACLES_SPRITE_TAGS;
    memcpy(e->tags_before, r->tags_before, e->tag_count_before * sizeof e->tags_before[0]);
    e->object_count = r->object_count < ORACLES_OBJECT_RECORDS ? r->object_count : ORACLES_OBJECT_RECORDS;
    memcpy(e->objects, r->objects, e->object_count * sizeof e->objects[0]);
    memcpy(e->killed_list, v->pending_killed_list, sizeof e->killed_list);
    /* The run inserts the rooms it crosses into its own list of killed
     * enemies: entering a parent can push the room's own entry out, and the
     * room then comes with every enemy back while the live list still counts
     * some killed.  Its terrain holds; its objects do not, and are dropped. */
    if (v->neighbour_objects && r->killed_enemies != oracles_objects_killed_in_list(e->killed_list, e->room)) {
        e->tag_count = 0; e->tag_count_before = 0; e->object_count = 0;
        v->objects_outdated++;
    }
    e->camera_x = r->camera_x; e->camera_y = r->camera_y;
    e->large = r->room_is_large != 0;
    memcpy(e->bg_map, r->bg_map, sizeof e->bg_map);
    memcpy(e->regs3, r->regs3, sizeof e->regs3);
    ev_entry_used_tiles(e);
    e->tileset_gfx = r->tileset_gfx; e->tileset_palette = r->tileset_palette; e->tileset_unique_gfx = r->tileset_unique_gfx;
    e->animation = r->animation;
    e->animation_version = 0;
    memset(e->own_animated, 0, sizeof e->own_animated);
    e->align_key = 0;
    e->image_count = v->rom ? oracles_animation_images(&e->animation, oracles_guest_tables(v->guest), v->rom, v->rom_size, e->image_tiles, e->image_sources, ENTRY_ANIMATION_IMAGES) : 0u;
    record_entry_reads(v, e, r);
    if (!e->settled_state) e->settled_state = malloc(v->state_size);
    e->settled_size = e->settled_state ? oracles_ghost_settled_state(v->ghost, e->settled_state, v->state_size) : 0;
    e->valid = 1; e->failed = 0; e->live_valid = 0; e->refresh = 0; e->refresh_season = 0; e->refresh_failures = 0; e->rerun = 0;
    e->accepted_at = v->frame;
    v->completed++;
}

/* The ghost settled in another room than the one asked for.  The forced
 * transition moved Link off the warp he had just come out of, and the door
 * under him fired: not a failure of the neighbour, a matter of timing.
 * From the live state, try again once he has walked away; from a parent's
 * state, later.  A scroll that comes back into the room it left is the
 * game's own loop (Seasons' eye statue puzzle, the Lost Woods, Ages' forest
 * scrambler: screenTransitionEyePuzzle and its kind), not a door: it is
 * tried again at the pace of a failure. */
static void record_wrong_room(OraclesEnhancedView *v, entry *e, const OraclesGhostResult *r)
{
    const int loop = r->group == r->from_group && r->room == r->from_room;
    /* A chained run out of a room the game routes itself (Ages' forest, from inside it in both bands; beside
     * it in the drawn-back band's last pass, view_schedule.c): where it went is the routing. */
    if (v->pending_chained && r->group == v->pending_group && ev_self_routed(v, r->group, v->pending_parent_room))
        ev_note_routing(v, r->group, v->pending_parent_room, v->pending_dir, r->room);
    if (v->pending_chained && r->prerun_frames) {
        /* The parent's settled state was not a start in its own room (Link
         * left past an edge there, the game on its way to another room
         * before the run could prime): the parent is run again from the
         * live state, still drawn, and this room right after. */
        entry *parent = &v->slots[v->pending_parent];
        if (parent->used && parent->valid && parent->group == v->pending_group && parent->room == v->pending_parent_room
            && (!parent->rerun_at || v->frame - parent->rerun_at >= FAILED_RETRY_FRAMES)) {   /* once a retry period: the live state may give the same */
            parent->rerun = 1; parent->rerun_at = v->frame; e->failed = 0;
        }
    }
    if (!v->pending_chained && !loop) {
        e->failed = 0;
        e->retry_when_link_moves = 1;
        e->link_x = v->observation.world.link_x; e->link_y = v->observation.world.link_y;
    }
    v->failure_reasons[3]++;
    const size_t used = strlen(v->failure_log);
    if (used + 24 < sizeof v->failure_log)
        snprintf(v->failure_log + used, sizeof v->failure_log - used, "%s%u:%02x>%u:%02x=%u:%02x", used ? " " : "",
                 v->observer.ref_group, v->observer.ref_room, v->pending_group, v->pending_room, r->group, r->room);
}

/* A run that did not deliver the room asked for: by kind, with what the
 * next attempt waits for.  A room run again keeps its terrain until a run
 * succeeds. */
static void record_failure(OraclesEnhancedView *v, entry *e, const OraclesGhostResult *r)
{
    /* A room run again deliberately keeps its old terrain for one failed
     * attempt, so that a single failure does not flash black; past that, black, as any room the
     * ghost has not delivered, rather than a season or objects that may stay
     * wrong for as long as the runs fail.  A room still valid, run again
     * only for its settled state (run_from_parent: the key moved on in
     * another room's bytes, which its own reads do not hold), keeps its
     * terrain: the reads checked every frame stand for it (Seasons' 0:a6,
     * run from 0:a7 while the intro's script holds Link, never settling). */
    if (e->refresh && ++e->refresh_failures >= 2u) { e->refresh = 0; e->valid = 0; v->refresh_given_up++; }
    if (!e->refresh && !(e->valid && !e->rerun)) e->valid = 0;
    e->failed = 1; e->failed_at = v->frame;
    v->failed++;
    if (r->status == ORACLES_GHOST_OK && (r->group != v->pending_group || r->room != v->pending_room)) { record_wrong_room(v, e, r); return; }
    if (r->status == ORACLES_GHOST_NOT_PRIMEABLE) { v->failure_reasons[0]++; return; }
    if (r->status == ORACLES_GHOST_LOAD_FAILED) { v->failure_reasons[1]++; return; }
    if (r->status == ORACLES_GHOST_TIMEOUT) {
        v->failure_reasons[2]++;
        const size_t used = strlen(v->failure_log);
        if (used + 24 < sizeof v->failure_log)
            snprintf(v->failure_log + used, sizeof v->failure_log - used, "%s%u:%02x>%u:%02x?", used ? " " : "",
                     v->observer.ref_group, v->observer.ref_room, v->pending_group, v->pending_room);
        return;
    }
    v->failure_reasons[4]++;
    if (r->status != ORACLES_GHOST_INTERRUPTED) return;
    /* The text box an interaction on Link's way opens (a dungeon's name):
     * the live Link crosses it too, and the interaction goes; the run is
     * tried again once he has walked on. */
    if (!v->pending_chained) {
        e->failed = 0;
        e->retry_when_link_moves = 1;
        e->link_x = v->observation.world.link_x; e->link_y = v->observation.world.link_y;
    }
    const size_t used = strlen(v->failure_log);
    if (used + 24 < sizeof v->failure_log)
        snprintf(v->failure_log + used, sizeof v->failure_log - used, "%s%u:%02x>%u:%02x#text", used ? " " : "",
                 v->observer.ref_group, v->observer.ref_room, v->pending_group, v->pending_room);
}

/* A capture of no terrain is never kept (oracles_enhanced_capture_blank):
 * the ghost primed while the game faded (Seasons' intro fades to white as
 * Din starts dancing, and a run of 59 frames settles in the white), or the
 * LCD was off.  A large room's window is a part of the room and may be one
 * tile (4:e9 of Seasons): its palettes alone say.  The room is run again,
 * a room run again keeping its old terrain as after a failure, not on the
 * clock alone, which would bring the same capture back for as long as its
 * cause holds: once Link has moved, or the fade the live game showed too is
 * over (a cutscene's, where he does not move), and a failure's wait is over. */
static int reject_blank_capture(OraclesEnhancedView *v, entry *e, const OraclesGhostResult *r)
{
    if (!oracles_enhanced_capture_blank(r->bg_palettes, r->base_bg_palettes, r->palette_offset, r->room_is_large ? NULL : r->game_area, ORACLES_GHOST_AREA_WIDTH * ORACLES_GHOST_AREA_HEIGHT)) return 0;
    record_failure(v, e, r);
    e->retry_when_link_moves = 1;
    e->retry_after_fade = v->fade_effective != 0;
    e->link_x = v->observation.world.link_x; e->link_y = v->observation.world.link_y;
    v->plain_renders++;
    const size_t used = strlen(v->plain_log);
    if (used + 8 < sizeof v->plain_log) snprintf(v->plain_log + used, sizeof v->plain_log - used, "%s%u:%02x", used ? " " : "", r->group, r->room);
    return 1;
}

int oracles_enhanced_reads_hold(unsigned count, const uint16_t *addresses, const uint8_t *seen, const uint8_t *live, uint16_t own_flags, uint8_t visited)
{
    for (unsigned i = 0; i < count; i++) {
        uint8_t differs = (uint8_t)(seen[i] ^ live[i]);
        if (addresses[i] == own_flags) differs &= (uint8_t)~visited;   /* the entry into the room sets it, no load reads it (ev_invalidate_by_reads) */
        if (differs) return 0;
    }
    return 1;
}

/* A room run from a room beside it while a cutscene holds the game starts
 * from that room's state, whose key may differ from the live one by other
 * rooms' flags: the bytes its run read, as it saw them, against the live
 * ones, before it is kept.  Refused as a failed run (again on the clock)
 * when one differs, rather than drawn for a frame and thrown by the next
 * frame's invalidation, then asked again at once from the same parent for
 * as long as the scene lasts.  A trace that overflowed cannot say: refused. */
static int beside_reads_hold(OraclesEnhancedView *v, const OraclesGhostResult *r)
{
    if (r->reads_dropped) return 0;
    uint16_t addresses[ORACLES_GHOST_RUN_READS];
    uint8_t seen[ORACLES_GHOST_RUN_READS], live[ORACLES_GHOST_RUN_READS];
    unsigned count = 0;
    for (unsigned i = 0; i < r->read_count && count < ORACLES_GHOST_RUN_READS; i++) {
        const uint16_t a = r->reads[i];
        const long at = ev_tracked_read(v, a) ? ev_key_offset(v, a) : -1;
        if (at < 0 || (size_t)at >= r->key_len) continue;
        addresses[count] = a; seen[count] = r->key[at]; live[count] = ev_live_byte(v, a);
        count++;
    }
    return oracles_enhanced_reads_hold(count, addresses, seen, live, ev_room_flags_address(v, r->group, r->room), oracles_guest_tables(v->guest)->roomflag_visited);
}

/* A direction asked from a room the game routes itself: the room the ghost
 * loaded is the answer, whichever it is — the room in play again when the
 * Lost Woods sends Link back to its entrance.  Only a run that did not
 * settle, or one that left the group (a warp fired under Link), fails. */
static void finish_routed_job(OraclesEnhancedView *v, entry *e, const OraclesGhostResult *r, int rc)
{
    v->pending_room = e->routed_from;   /* the room asked from: what a failure names */
    if (rc != 1 || r->status != ORACLES_GHOST_OK || !r->settled || r->group != v->pending_group) { record_failure(v, e, r); return; }
    e->room = r->room;
    v->pending_room = r->room;
    if (reject_other_season(v, e, r) || reject_blank_capture(v, e, r)) return;
    ev_note_routing(v, e->group, e->routed_from, (OraclesGhostDirection)e->routed_dir, r->room);
    /* The game routed this direction the ordinary way and routes no other
     * one elsewhere either: the room delivered is the grid's neighbour, so
     * the entry is that room's, chained from and placed like any other, and
     * the rooms beyond it are planned again.  While any direction of the
     * room does route elsewhere, every answer stays a direction's own — the
     * grid is no longer what the band shows around it. */
    if (!ev_routes_elsewhere(v, e->group, e->routed_from)) {
        e->routed = 0;
        for (unsigned i = 0; i < v->slot_count; i++) {
            entry *c = &v->slots[i];
            if (c != e && c->used && !c->routed && c->group == e->group && c->room == e->room) c->used = 0;   /* one entry a room */
        }
    } else v->routed_delivered++;
    accept_result(v, e, r);
}

/* The lane whose run the job fields are: its fields copied in, the lane
 * served before stored back. */
#define LANE_FIELDS(X) X(ghost) X(job) X(pending) X(pending_slot) X(pending_group) X(pending_room) X(pending_generation) \
    X(pending_chained) X(pending_beside) X(pending_parent) X(pending_parent_room) X(pending_parent_accepted) X(pending_serial) X(pending_blind) \
    X(pending_routed) X(pending_routed_from) X(pending_dir)
void ev_lane_serve(OraclesEnhancedView *v, unsigned lane)
{
    if (lane == v->lane_now || lane >= v->lane_count) return;
    struct ev_lane *out = &v->lanes[v->lane_now], *in = &v->lanes[lane];
#define STORE(f) out->f = v->f;
#define LOAD(f) v->f = in->f;
    LANE_FIELDS(STORE)
    memcpy(out->pending_killed_list, v->pending_killed_list, sizeof out->pending_killed_list);
    LANE_FIELDS(LOAD)
    memcpy(v->pending_killed_list, in->pending_killed_list, sizeof v->pending_killed_list);
#undef STORE
#undef LOAD
    v->lane_now = lane;
}

/* The run in flight is over: its result goes to the slot it was started for,
 * unless the world moved on under it. */
static void finish_job(OraclesEnhancedView *v, const OraclesGhostResult *r, int rc)
{
    v->pending = 0;
    /* The run's own entry, by its serial: a slot dropped and taken again
     * meanwhile (a routed direction asked again under another key, by the
     * other ghost) has another run's, or none. */
    entry *e = !v->pending_blind && v->pending_slot < v->slot_count ? &v->slots[v->pending_slot] : NULL;
    const int own = e && e->run_serial == v->pending_serial;
    if (own) e->in_flight = 0;
    if (v->pending_generation != v->generation) { v->stale++; return; }   /* the pipeline or a load changed everything */
    if (e && !own) { v->stale++; return; }
    if (v->pending_blind && !file_blind_result(v, r, rc)) return;
    e = &v->slots[v->pending_slot];
    if (v->pending_routed) {
        if (!e->used || !e->routed || e->group != v->pending_group
            || e->routed_from != v->pending_routed_from || e->routed_dir != (uint8_t)v->pending_dir) { v->stale++; return; }
        finish_routed_job(v, e, r, rc);
        return;
    }
    if (!e->used || e->routed || e->group != v->pending_group || e->room != v->pending_room) { v->stale++; return; }   /* the slot was taken */
    if (v->pending_chained) {
        const entry *parent = &v->slots[v->pending_parent];
        if (!parent->used || !parent->valid || parent->group != v->pending_group || parent->room != v->pending_parent_room
            || parent->accepted_at != v->pending_parent_accepted) { v->stale++; return; }   /* delivered again meanwhile, by another ghost */
    }
    const int delivered = rc == 1 && r->status == ORACLES_GHOST_OK && r->settled;
    if (delivered && !v->pending_chained && r->group == v->pending_group && r->room != v->pending_room
        && r->group == r->from_group && r->room == r->from_room) {
        e = loop_return_entry(v, e, r);
        if (!e) return;
    }
    if (delivered && r->group == v->pending_group && r->room == v->pending_room) {
        if (reject_other_season(v, e, r) || reject_blank_capture(v, e, r)) return;
        if (v->pending_beside && !beside_reads_hold(v, r)) { v->beside_refused++; record_failure(v, e, r); return; }
        accept_result(v, e, r);
    } else record_failure(v, e, r);
}


/* An entry whose trace overflowed is compared on the whole key, as is a
 * parent a room is chained from: for the terrain of `group`:`room` (the
 * entry's own, or the room chained), the time portal and a time warp's
 * arrival as in ev_invalidate_by_reads. */
int ev_entry_key_current(const OraclesEnhancedView *v, const entry *e, const uint8_t *key, size_t key_len, uint8_t group, uint8_t room)
{
    if (e->key_len != key_len) return 0;
    const long own = ev_key_offset(v, ev_room_flags_address(v, e->group, e->room));   /* its visited bit aside, as in ev_invalidate_by_reads */
    for (size_t i = 0; i < key_len; i++) {
        const uint8_t differs = (uint8_t)(e->key_snapshot[i] ^ key[i]);
        if (differs && !((long)i == own && !(differs & (uint8_t)~oracles_guest_tables(v->guest)->roomflag_visited))
            && !ev_key_byte_exempt(v, (long)i, e->key_snapshot, key, group, room)) return 0;
    }
    return 1;
}

/* Seasons: every run starts in the live season (the band in one season). */
static void hold_live_season(OraclesEnhancedView *v)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    oracles_ghost_set_held_season(v->ghost, oracles_compat_seasons_rules(oracles_guest_profile(v->guest)) ? oracles_guest_read8(v->guest, t->room_state_modifier) : -1);
}

/* The live wEnemiesKilledList a run from the live state stands for. */
static void note_live_killed_list(OraclesEnhancedView *v)
{
    const uint8_t *killed = oracles_guest_ptr(v->guest, oracles_guest_tables(v->guest)->enemies_killed_list, sizeof v->pending_killed_list);
    if (killed) memcpy(v->pending_killed_list, killed, sizeof v->pending_killed_list); else memset(v->pending_killed_list, 0, sizeof v->pending_killed_list);
}

int ev_start_job(OraclesEnhancedView *v, entry *e, const uint8_t *state, size_t size, OraclesGhostDirection dir, const entry *parent)
{
    if (e->in_flight) return 0;   /* the other ghost runs it (a parent run again for a room beyond): not a failure */
    hold_live_season(v);
    const int chained = parent != NULL;
    /* A run from the live state for the other side of the sea crosses it first. */
    const int across = !chained && v->level_change >= 0;
    oracles_ghost_set_level_change(v->ghost, across ? v->level_change : -1);
    int started;
    if (v->sync_budget) started = oracles_ghost_begin_ex(v->ghost, state, size, dir, 1, v->colours, v->job) == 0;
    /* In the thread the budget is the whole job's: a run across the surface plays its warp first, as a pre-run does. */
    else started = oracles_ghost_request_ex(v->ghost, state, size, dir, 1, v->colours, across ? GHOST_BLIND_SETTLE_FRAMES : GHOST_SETTLE_FRAMES) == 0;
    v->requested++;
    if (!started) {
        /* Synchronous: the state was refused (a parent's settled state that
         * cannot be primed); threaded: the worker is busy.  Tried again later. */
        v->failed++;
        e->failed = 1; e->failed_at = v->frame;
        if (v->sync_budget && v->job->status == ORACLES_GHOST_NOT_PRIMEABLE) {
            v->failure_reasons[0]++;
            const size_t used = strlen(v->failure_log);
            if (used + 40 < sizeof v->failure_log)
                snprintf(v->failure_log + used, sizeof v->failure_log - used, "%s%u:%02x>%u:%02x!%s", used ? " " : "",
                         v->observer.ref_group, v->observer.ref_room, e->group, e->room, oracles_ghost_last_reason(v->ghost));
        }
        else if (v->sync_budget && v->job->status == ORACLES_GHOST_LOAD_FAILED) v->failure_reasons[1]++;
        else v->failure_reasons[4]++;
        return 0;
    }
    if (chained) v->chained++;
    if (across) v->sea_runs++;
    /* An answer holds while the live bytes its routine reads keep these values. */
    if (e->routed) e->routing_key_len = (uint8_t)oracles_ghost_routing_key_snapshot(v->guest, oracles_ghost_room_routine(v->ghost, e->group, e->routed_from), e->routing_key, sizeof e->routing_key);
    if (chained) memcpy(v->pending_killed_list, parent->killed_list, sizeof v->pending_killed_list);
    else note_live_killed_list(v);
    v->pending = 1;
    v->pending_slot = (unsigned)(e - v->slots);
    v->pending_group = e->group;
    v->pending_room = e->room;
    v->pending_generation = v->generation;
    v->pending_chained = chained;
    v->pending_beside = 0;
    v->pending_parent = chained ? (unsigned)(parent - v->slots) : 0;
    v->pending_parent_room = chained ? parent->room : 0;
    v->pending_parent_accepted = chained ? parent->accepted_at : 0;
    v->pending_blind = 0;
    e->in_flight = 1;
    e->run_serial = v->pending_serial = ++v->run_serials;
    v->pending_routed = e->routed;
    v->pending_routed_from = e->routed_from;
    v->pending_dir = dir;
    return 1;
}

/* While a room loads outside a scroll (a warp, the start of play), the
 * reference room is not yet the destination: the ghost is given the live
 * state as it is, runs the load and the fade-in itself, primes in the room
 * the load reaches and scrolls in the direction asked; one run per
 * direction per load, left and right first, the result filed beside the
 * room it primed in.  The neighbours are ready when the live fade-in ends. */
void ev_blind_prerun(OraclesEnhancedView *v)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    if (v->pending || !ob->playing || ob->in_scroll || !ob->grid || (ob->group & 7u) > 1u) return;
    if (oracles_ghost_primeable(v->guest, NULL) || !oracles_ghost_prerunnable(v->guest, NULL)) return;
    if (v->blind_epoch != ob->epoch || v->blind_room != ob->room) { v->blind_epoch = ob->epoch; v->blind_room = ob->room; v->blind_mask = 0; }
    static const OraclesGhostDirection order[4] = { ORACLES_DIR_LEFT, ORACLES_DIR_RIGHT, ORACLES_DIR_UP, ORACLES_DIR_DOWN };
    for (unsigned i = 0; i < 4; i++) {
        const OraclesGhostDirection dir = order[i];
        if (v->blind_mask & (1u << dir)) continue;
        /* If the destination is already known and its neighbour that way cached, nothing to do. */
        uint8_t target;
        if (v->observer.have_reference && v->observer.ref_room == ob->room) {
            if (!ev_room_toward(v, ob->room, dir, &target)) { v->blind_mask |= (uint8_t)(1u << dir); continue; }   /* the map's edge */
            const entry *e = ev_find_entry(v, ob->group, target);
            if (e && (e->valid || e->in_flight)) { v->blind_mask |= (uint8_t)(1u << dir); continue; }
        }
        v->blind_mask |= (uint8_t)(1u << dir);
        if (oracles_core_save_state(v->core, v->snapshot, v->state_size) != 0) return;
        int started;
        /* A pre-run plays the load the live game is in, whose season is not set yet
         * (checkRoomPackAfterWarp): it holds the one the ghost has once loaded, the game's. */
        oracles_ghost_set_held_season(v->ghost, oracles_compat_seasons_rules(oracles_guest_profile(v->guest)) ? ORACLES_GHOST_HOLD_OWN : -1);
        oracles_ghost_set_level_change(v->ghost, -1);
        if (v->sync_budget) started = oracles_ghost_begin_ex(v->ghost, v->snapshot, v->state_size, dir, 1, v->colours, v->job) == 0;
        else started = oracles_ghost_request_ex(v->ghost, v->snapshot, v->state_size, dir, 1, v->colours, GHOST_BLIND_SETTLE_FRAMES) == 0;
        v->requested++;
        if (!started) { v->failed++; v->failure_reasons[4]++; return; }
        note_live_killed_list(v);
        v->pending = 1;
        v->pending_blind = 1;
        v->pending_routed = 0;
        v->pending_dir = dir;
        v->pending_chained = 0;
        v->pending_beside = 0;
        v->pending_generation = v->generation;
        return;
    }
}

/* The run in flight, stepped (synchronous) or polled (threaded); its result filed. */
void ev_advance_pending_run(OraclesEnhancedView *v)
{
    if (!v->pending) return;
    if (v->sync_budget) {
        const int rc = oracles_ghost_step(v->ghost, v->sync_budget, v->pending_blind ? GHOST_BLIND_SETTLE_FRAMES : GHOST_SETTLE_FRAMES, v->job);
        if (rc != 0) finish_job(v, v->job, rc);
    } else {
        const int polled = oracles_ghost_poll(v->ghost, v->job);
        if (polled == 1) finish_job(v, v->job, 1);
        else if (polled == -1) finish_job(v, v->job, -1);
    }
}
