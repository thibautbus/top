#include "view_internal.h"

/* ---- the cache key ------------------------------------------------------------------ */

/* A key byte an entry's substitutions read is tracked, except the active
 * group and room: they are the entry's identity, and change at every
 * transition. */
int ev_tracked_read(const OraclesEnhancedView *v, uint16_t a)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    if (a == t->active_group.addr || a == t->active_room.addr) return 0;
    for (unsigned k = 0; k < v->key_count; k++)
        if (a >= v->key[k].address && a < v->key[k].address + v->key[k].length) return 1;
    return 0;
}

uint8_t ev_live_byte(OraclesEnhancedView *v, uint16_t a)
{
    const OraclesGuestSym sym = { 0, a };
    return oracles_guest_read8(v->guest, sym);
}

/* The whole key, less the active group and room, which are the identity of
 * an entry: the layout the ghost's results carry (`oracles_ghost_key_snapshot`). */
size_t ev_read_key(OraclesEnhancedView *v, uint8_t *out)
{
    return oracles_ghost_key_snapshot(v->guest, out, ORACLES_GHOST_KEY_BYTES);
}

/* The offset of a key byte in a key snapshot (the ranges in order, the
 * identity left out), or -1 when the address is not in the key. */
long ev_key_offset(const OraclesEnhancedView *v, uint16_t a)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    size_t n = 0;
    for (unsigned k = 0; k < v->key_count; k++) {
        if (v->key[k].address == t->active_group.addr || v->key[k].address == t->active_room.addr) continue;
        if (a >= v->key[k].address && a < v->key[k].address + v->key[k].length) return (long)(n + (a - v->key[k].address));
        n += v->key[k].length;
    }
    return -1;
}

void ev_drop_entry(entry *e)
{
    e->valid = 0; e->failed = 0; e->not_a_start = 0; e->not_a_start_apart = 0; e->waits_start = 0; e->strict_parent = 0; e->retry_when_link_moves = 0; e->retry_after_fade = 0; e->live_valid = 0; e->settled_size = 0; e->refresh = 0; e->refresh_season = 0; e->refresh_failures = 0; e->rerun = 0;
    e->refresh_objects = 0; e->refresh_terrain = 0; e->terrain_stale_since = 0; e->object_read_count = 0; e->entry_side_read = 0;
}

void ev_invalidate_all(OraclesEnhancedView *v)
{
    for (unsigned i = 0; i < v->slot_count; i++) ev_drop_entry(&v->slots[i]);
    v->generation++;
}

/* An entry stays valid while the key bytes its substitutions read keep the
 * values they had: a chest opened three rooms away, a flag of another room,
 * do not throw it away; its own flag byte, or a global its tiles depend on, do. */
static void log_drop(OraclesEnhancedView *v, uint16_t a)
{
    v->drops++;
    for (unsigned i = 0; i < DROP_LOG; i++) {
        if (v->drop_count[i] && v->drop_addr[i] != a) continue;
        v->drop_addr[i] = a;
        v->drop_count[i]++;
        return;
    }
}

/* The address of a room's own flags (getRoomFlags: flagLocationGroupTable
 * gives the page of each group, the room the offset). */
uint16_t ev_room_flags_address(const OraclesEnhancedView *v, uint8_t group, uint8_t room)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const uint8_t page = oracles_compat_room_flag_page(oracles_guest_profile(v->guest), group);
    return (uint16_t)(t->group0_room_flags.addr + page * 0x100u + room);
}

/* The coarse list: a byte the load around the substitutions reads, which no
 * entry records, changed since the last frame of normal play, and the whole
 * cache goes, as at a savestate's load; a change scoped to one room (Seasons'
 * end of the intro, the wagon west of Din's troupe) throws that room alone,
 * black until the ghost runs it again rather than drawn with the wagon gone.
 * Compared in normal play only: during a room's load, a warp, a fade or a
 * file's loading the live state is half way; the change is seen on the first
 * frame of play after it. */
static void invalidate_by_coarse_list(OraclesEnhancedView *v)
{
    if (!v->observation.playing || v->observation.in_transition) return;
    uint8_t now[ORACLES_GHOST_COARSE_BYTES];
    const size_t len = oracles_ghost_coarse_snapshot(v->guest, now, sizeof now);
    uint16_t address = 0;
    int room = -1;
    if (v->coarse_len && len == v->coarse_len && oracles_ghost_coarse_changed(oracles_guest_tables(v->guest), v->coarse, now, len, &address, &room)) {
        log_drop(v, address);
        if (room >= 0) {
            entry *e = ev_find_entry(v, 0, (uint8_t)room);
            if (e) ev_drop_entry(e);
        } else {
            v->coarse_drops++;
            ev_invalidate_all(v);
        }
    }
    memcpy(v->coarse, now, len);
    v->coarse_len = len;
}

/* The time portal (Ages): its group, room and position, which the game reads
 * to ask whether the portal is in the room it loads (replaceBreakableTileOverPortal,
 * the portal's interaction).  Set where Link arrives after a time warp and
 * cleared when he steps into it, they change for one room: an entry that the
 * portal was in neither before nor now is the same, and is kept (thrown, it
 * was black until the ghost ran it again, and so was every room around). */
static int portal_elsewhere(OraclesEnhancedView *v, const entry *e, uint16_t address)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const OraclesGuestSym portal = t->portal_group, portal_room = t->portal_room;
    /* The group, the room and the position, in a row (wPortalPos follows wPortalRoom). */
    if (portal.bank == ORACLES_GUEST_ABSENT || address < portal.addr || address > portal_room.addr + 1u) return 0;
    int group = -1, room = -1;
    for (unsigned r = 0; r < e->read_count; r++) {
        if (e->read_addr[r] == portal.addr) group = e->read_value[r];
        else if (e->read_addr[r] == portal_room.addr) room = e->read_value[r];
    }
    const int was_here = group == e->group && room == e->room;
    const int is_here = ev_live_byte(v, portal.addr) == e->group && ev_live_byte(v, portal_room.addr) == e->room;
    return !was_here && !is_here;
}

/* The same two exemptions on a whole key: byte `i` of a key taken with
 * `snapshot`, now `live`, for the terrain of `group`:`room` (a room chained
 * from a parent's settled state, or an entry judged on the whole key). */
int ev_key_byte_exempt(const OraclesEnhancedView *v, long i, const uint8_t *snapshot, const uint8_t *live, uint8_t group, uint8_t room)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    if (t->portal_group.bank != ORACLES_GUEST_ABSENT) {
        const long at_group = ev_key_offset(v, t->portal_group.addr), at_room = ev_key_offset(v, t->portal_room.addr);
        const long at_pos = ev_key_offset(v, (uint16_t)(t->portal_room.addr + 1u));
        if (at_group >= 0 && at_room >= 0 && (i == at_group || i == at_room || i == at_pos)) {
            const int was_here = snapshot[at_group] == group && snapshot[at_room] == room;
            const int is_here = live[at_group] == group && live[at_room] == room;
            return !was_here && !is_here;
        }
    }
    if (t->link_time_warp_tile.bank != ORACLES_GUEST_ABSENT && i == ev_key_offset(v, t->link_time_warp_tile.addr)) return snapshot[i] == 0;
    return 0;
}

int oracles_enhanced_object_reads_hold(unsigned count, const uint16_t *addresses, const uint8_t *values, const uint8_t *live, uint16_t flags_start,
                                       uint16_t flags_length, uint8_t visited)
{
    for (unsigned i = 0; i < count; i++) {
        uint8_t differs = (uint8_t)(values[i] ^ live[i]);
        if (addresses[i] >= flags_start && addresses[i] - flags_start < flags_length) differs &= (uint8_t)~visited;
        if (differs) return 0;
    }
    return 1;
}

/* The bytes of the key a delivered room's objects read, with the values of
 * the live key its run started under.  A byte that moved in the live game
 * while the run was in flight is seen at the next frame, and the room is run
 * again.  A trace past the capacity keeps the bytes it listed first (the
 * room's own objects read a handful). */
void ev_record_object_reads(OraclesEnhancedView *v, entry *e, const OraclesGhostResult *r)
{
    e->object_read_count = 0;
    for (unsigned i = 0; i < r->object_read_count && e->object_read_count < ENTRY_OBJECT_READS; i++) {
        const long at = ev_key_offset(v, r->object_reads[i]);
        if (at < 0 || (size_t)at >= v->pending_key_len) continue;
        e->object_read_addr[e->object_read_count] = r->object_reads[i];
        e->object_read_value[e->object_read_count] = v->pending_key[at];
        e->object_read_count++;
    }
}

/* Whether a room's objects hold for the live game: the bytes they read keep
 * their values, the visited bits of the rooms' flags aside. */
static int objects_hold(OraclesEnhancedView *v, const entry *e)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    uint8_t live[ENTRY_OBJECT_READS];
    for (unsigned i = 0; i < e->object_read_count; i++) live[i] = ev_live_byte(v, e->object_read_addr[i]);
    return oracles_enhanced_object_reads_hold(e->object_read_count, e->object_read_addr, e->object_read_value, live, t->group0_room_flags.addr,
                                              (uint16_t)t->room_flags_size, t->roomflag_visited);
}

int oracles_enhanced_entry_side_holds(int side_read, unsigned entered_side, int beside)
{
    return !side_read || beside < 0 || (unsigned)beside == entered_side;
}

int oracles_enhanced_left_entry_side_holds(int side_read, unsigned entered_side, int live_side)
{
    return !side_read || (live_side >= 0 && (unsigned)live_side == entered_side);
}

int oracles_enhanced_left_entry_yields(int side_objects, unsigned entered_side, int live_side, int in_transition, int out_of_date_beside)
{
    return !oracles_enhanced_left_entry_side_holds(side_objects, entered_side, live_side) && (in_transition || out_of_date_beside);
}

int ev_beside_ref(const OraclesEnhancedView *v, uint8_t group, uint8_t room)
{
    if (group != v->observer.ref_group) return 0;
    for (unsigned d = 0; d < 4u; d++) {
        uint8_t beside;
        if (ev_room_toward(v, v->observer.ref_room, (OraclesGhostDirection)d, &beside) && beside == room) return 1;
    }
    return 0;
}

int oracles_enhanced_parent_mark_lifts(int marked_apart, int beside_ref)
{
    return marked_apart && beside_ref;
}

/* A parent found no start while it was not beside the room in play
 * (record_wrong_room), now beside it: the mark goes, and the parent is run
 * again from the live state, which may give another state. */
void ev_lift_parent_marks(OraclesEnhancedView *v)
{
    for (unsigned i = 0; i < v->slot_count; i++) {
        entry *e = &v->slots[i];
        if (!e->used || !e->valid || !e->not_a_start || e->routed) continue;
        if (!oracles_enhanced_parent_mark_lifts(e->not_a_start_apart, ev_beside_ref(v, e->group, e->room))) continue;
        e->not_a_start = 0; e->not_a_start_apart = 0;
        e->rerun = 1; e->rerun_at = v->frame;
    }
}

int oracles_enhanced_objects_refresh_may_start(int story, unsigned live_quiet)
{
    return live_quiet >= (story ? OBJECT_REFRESH_QUIET : 1u);
}

int oracles_enhanced_objects_refresh_put_off(int live_in_play)
{
    return !live_in_play;
}

/* Whether a room whose objects read the side it was entered by was run from
 * another side than the one the game would enter it by from the room in play,
 * beside it in the same group (Ages' 0:39 run from the Maku Tree's room, with
 * its Ralph, and drawn above 0:49, from where the game creates none). */
int ev_entered_by_other_side(const OraclesEnhancedView *v, const entry *e)
{
    if (!e->entry_side_read || e->routed || e->group != v->observer.ref_group) return 0;
    int beside = -1;
    for (unsigned d = 0; d < 4u && beside < 0; d++) {
        uint8_t room;
        if (ev_room_toward(v, v->observer.ref_room, (OraclesGhostDirection)d, &room) && room == e->room) beside = (int)d;
    }
    return !oracles_enhanced_entry_side_holds(e->entry_side_read, e->entered_side, beside);
}

/* A room run again whose runs keep failing: at the second, a refresh for
 * its objects drops them and keeps its terrain, which its own reads still
 * hold, and takes the live values as those they stand for (no people rather
 * than the wrong ones, and no run again until the story moves on); any other
 * drops its terrain too, black until the ghost delivers it. */
void ev_refresh_failed(OraclesEnhancedView *v, entry *e)
{
    if (!e->refresh || ++e->refresh_failures < 2u) return;
    if (e->refresh_objects && !e->refresh_season) {
        e->tag_count = 0; e->tag_count_before = 0; e->object_count = 0;
        for (unsigned i = 0; i < e->object_read_count; i++) e->object_read_value[i] = ev_live_byte(v, e->object_read_addr[i]);
        e->entry_side_read = 0;
        e->refresh = 0; e->refresh_objects = 0; e->refresh_failures = 0;
        v->objects_story_dropped++;
        return;
    }
    e->refresh = 0; e->refresh_terrain = 0; e->valid = 0; v->refresh_given_up++;
}

void ev_invalidate_by_reads(OraclesEnhancedView *v)
{
    invalidate_by_coarse_list(v);
    int stale_shown = 0, terrain_stale_shown = 0;
    for (unsigned i = 0; i < v->slot_count; i++) {
        entry *e = &v->slots[i];
        if (!e->used || !e->valid || e->read_count > ENTRY_READS) continue;
        /* In normal play only: across an area's edge the game sets the new area before its season (the fade between them). */
        if (e->season_dependent && (!e->refresh || e->refresh_objects) && v->observation.playing && !v->observation.in_transition) {
            const OraclesGuestTables *t = oracles_guest_tables(v->guest);
            /* The band is in the live season (the ghost holds it across areas):
             * the rod of seasons, or the flash into an area of another season,
             * changes it on the frame the flash starts.  The room stays drawn
             * while the flash lasts, whitening with the screen, while the ghost
             * runs it again (ev_entry_drawable). */
            if ((oracles_guest_read8(v->guest, t->active_group) & 7u) == (e->group & 7u)
                && oracles_guest_read8(v->guest, t->room_state_modifier) != e->season) { log_drop(v, t->room_state_modifier.addr); e->refresh = 1; e->refresh_season = 1; e->refresh_objects = 0; continue; }
        }
        /* Its objects hold for the enemies counted killed in its room then: one
         * killed since (in the live room, which is cached too), or back after
         * eight other rooms, and the capture is out of date.  The objects go
         * at once, the terrain stays while the ghost runs the room again. */
        if (v->neighbour_objects && !e->refresh && oracles_objects_killed_enemies(v->guest, e->room) != oracles_objects_killed_in_list(e->killed_list, e->room)) {
            e->tag_count = 0; e->tag_count_before = 0; e->object_count = 0;
            e->refresh = 1;
            v->objects_outdated++;
            continue;
        }
        /* A terrain kept out of date, its run again not delivered (no start
         * for it, the burst's wait) or delivered out of date again each time,
         * for TERRAIN_STALE_FRAMES: black, until the ghost delivers it. */
        if (e->terrain_stale_since && v->frame - e->terrain_stale_since >= TERRAIN_STALE_FRAMES) {
            ev_drop_entry(e);
            v->terrain_given_up++;
            continue;
        }
        const OraclesGuestTables *tables = oracles_guest_tables(v->guest);
        int terrain_holds = 1;
        for (unsigned r = 0; r < e->read_count && !e->refresh_terrain; r++) {
            uint8_t differs = (uint8_t)(ev_live_byte(v, e->read_addr[r]) ^ e->read_value[r]);
            /* The visited bit of a room's flags: the transition that enters
             * the room sets it (updateActiveRoom, setVisitedRoomFlag, before
             * the substitutions; the special transitions, the Lost Woods or
             * the eye statue puzzle, later in cutscene00), and no tile
             * substitution reads it.  The live game sets it only when Link
             * enters: a room computed ahead of a first visit was thrown at
             * that very frame, and so was a room run from a parent's state
             * whose own visit the ghost had made (Ages' 0:27 run from 0:28,
             * Link in 0:38 below it, thrown and run again every few frames). */
            if (e->read_addr[r] >= tables->group0_room_flags.addr && e->read_addr[r] - tables->group0_room_flags.addr < tables->room_flags_size)
                differs &= (uint8_t)~tables->roomflag_visited;
            if (differs && portal_elsewhere(v, e, e->read_addr[r])) differs = 0;
            /* The spot of a time warp's arrival (Ages): set while Link arrives
             * and cleared as he gets his control back, it changes the room he
             * arrives in alone; a neighbour computed with none is the room he
             * will scroll into once it is cleared. */
            const OraclesGuestSym warp_tile = tables->link_time_warp_tile;
            if (differs && warp_tile.bank != ORACLES_GUEST_ABSENT && e->read_addr[r] == warp_tile.addr && e->read_value[r] == 0) differs = 0;
            /* Drawn as it was until the ghost delivers it again, as a room
             * run again for its season or its story is: rather than black.  A
             * room so run again OBJECT_REFRESH_BURST times within
             * FAILED_RETRY_FRAMES waits for the end of that period
             * (entry_wanting), still drawn, black past TERRAIN_STALE_FRAMES.
             * Not the room in play's: the live game draws it, then the image
             * of the room left, which its old terrain would hide (a chest
             * opened there drawn closed again behind Link). */
            if (differs && e->group == v->observer.ref_group && e->room == v->observer.ref_room) {
                log_drop(v, e->read_addr[r]);
                ev_drop_entry(e);
                break;
            }
            if (differs) {
                log_drop(v, e->read_addr[r]);
                const int recent = v->frame - e->terrain_refresh_at < FAILED_RETRY_FRAMES;
                e->terrain_refresh_burst = recent ? e->terrain_refresh_burst + 1u : 1u;
                e->terrain_refresh_at = v->frame;
                if (!e->terrain_stale_since) e->terrain_stale_since = v->frame ? v->frame : 1u;
                e->refresh = 1; e->refresh_terrain = 1; e->refresh_objects = 0;
                v->terrain_refreshes++;
                terrain_holds = 0;
                break;
            }
        }
        if (terrain_holds && !e->refresh_terrain) e->terrain_stale_since = 0;
        if (e->refresh_terrain && e->group == v->observer.ref_group && e->room != v->observer.ref_room && ev_room_in_view(v, e->room, 0)
            && !(v->left_valid && e->group == v->left_group && e->room == v->left_room)) terrain_stale_shown = 1;   /* the room left shows its image */
        if (!e->valid || e->refresh_terrain) continue;
        /* Its objects hold for the story state they read as the room created
         * them: a global flag, a room's flags, a treasure (Ages' intro: the
         * object of 0:39 that creates Nayru and the animals until Impa sets
         * GLOBALFLAG_INTRO_DONE).  One changed since, and the room is run
         * again; its terrain and objects stay drawn until the new ones come.
         * Compared in normal play only, as the season is: during a load, a
         * warp or a fade the live state is half way.  A room run again so
         * OBJECT_REFRESH_BURST times within FAILED_RETRY_FRAMES waits for the
         * end of that period: a byte that keeps toggling does not hold the
         * ghost. */
        /* So are they for the side the room was entered by, when they read it:
         * beside the room in play, the side the game would enter it by. */
        const int compared = v->observation.playing && !v->observation.in_transition;
        const int other_side = compared && !e->refresh && ev_entered_by_other_side(v, e);
        const int objects_stale = v->neighbour_objects && (e->refresh_objects || (compared && !e->refresh && (other_side || !objects_hold(v, e))));
        if (objects_stale && !e->refresh) {
            const int recent = v->frame - e->objects_refresh_at < FAILED_RETRY_FRAMES;
            if (!recent || e->objects_refresh_burst < OBJECT_REFRESH_BURST) {
                e->objects_refresh_burst = recent ? e->objects_refresh_burst + 1u : 1u;
                e->objects_refresh_at = v->frame;
                e->refresh = 1;
                e->refresh_objects = 1;
                e->refresh_story = !other_side;
                if (other_side) v->objects_side_refreshes++; else v->objects_story_refreshes++;
            }
        }
        if (objects_stale && e->object_count && e->group == v->observer.ref_group && e->room != v->observer.ref_room && ev_room_in_view(v, e->room, 0)) stale_shown = 1;
    }
    if (stale_shown) v->objects_stale_frames++;
    if (terrain_stale_shown) v->terrain_stale_frames++;
}

int oracles_enhanced_terrain_refresh_waits(unsigned burst, uint32_t since_last)
{
    return burst > OBJECT_REFRESH_BURST && since_last < FAILED_RETRY_FRAMES;
}

/* ---- the colour table ------------------------------------------------------------------ */

void ev_refresh_colours(OraclesEnhancedView *v)
{
    const int pipeline = oracles_core_colour_correction(v->core);
    if (pipeline == v->colours_pipeline) return;
    v->colours_pipeline = pipeline;
    for (unsigned i = 0; i < ORACLES_PPU_COLOURS; i++) v->colours[i] = oracles_core_convert_rgb555(v->core, (uint16_t)i);
    /* The renders are in the old pipeline, the rooms the ghosts delivered are not: each is drawn again whole, and
     * the band keeps its neighbours and the room it follows (dropping them fell back to the framed core). */
    for (unsigned i = 0; i < v->slot_count; i++) v->slots[i].live_valid = 0;
}

/* An entry holding a room of the grid.  A routed entry holds the room the
 * game loads leaving another one, which is not that room's own place: it is
 * found by where it is asked from, never by the room it happens to hold. */
entry *ev_find_entry(OraclesEnhancedView *v, uint8_t group, uint8_t room)
{
    for (unsigned i = 0; i < v->slot_count; i++)
        if (v->slots[i].used && !v->slots[i].routed && v->slots[i].group == group && v->slots[i].room == room) return &v->slots[i];
    return NULL;
}

/* A free slot, else the least recently used one that is neither a room
 * wanted now, by this plan or by the other side of the sea's, nor a
 * direction of the room in play that the game routes.  The rooms beyond an
 * answer of the Lost Woods take ordinary slots: with the cache full, one may
 * evict the answer it hangs from, which costs a run and never shows a wrong
 * room (they are drawn only while that answer is). */
static entry *take_victim(OraclesEnhancedView *v, uint8_t group, const uint8_t *wanted, unsigned wanted_count, uint8_t routed_from)
{
    entry *victim = NULL;
    for (unsigned i = 0; i < v->slot_count; i++) {
        entry *c = &v->slots[i];
        if (!c->used) return c;
        if (c->in_flight) continue;   /* its run would come back to a room no longer there */
        if (c->routed && c->group == group && c->routed_from == routed_from) continue;
        int wanted_now = 0;
        if (!c->routed) for (unsigned w = 0; w < wanted_count; w++) if (c->group == group && c->room == wanted[w]) wanted_now = 1;
        /* Nor one of the other plan's: the same place on the other side of the sea. */
        if (!c->routed) for (unsigned w = 0; w < v->keep_count; w++) if (c->group == v->keep_group && c->room == v->keep[w]) wanted_now = 1;
        if (wanted_now) continue;
        /* A room of the second ring first, then a near one; the oldest first in each. */
        if (!victim || (victim->near && !c->near) || (victim->near == c->near && c->last_use < victim->last_use)) victim = c;
    }
    return victim;
}

static entry *claim_slot(entry *victim, uint8_t group, uint8_t room)
{
    if (!victim) return NULL;
    uint8_t *state = victim->settled_state;
    memset(victim, 0, sizeof *victim);
    victim->settled_state = state;
    victim->used = 1;
    victim->group = group;
    victim->room = room;
    return victim;
}

/* The entry of a room, or a slot for it: a free one, else the least recently
 * used one that is not one of the rooms wanted now. */
entry *ev_take_slot(OraclesEnhancedView *v, uint8_t group, uint8_t room, const uint8_t *wanted, unsigned wanted_count)
{
    entry *e = ev_find_entry(v, group, room);
    if (e) return e;
    return claim_slot(take_victim(v, group, wanted, wanted_count, 0xffu), group, room);
}

/* The entry holding what the game loads when Link leaves `from_room` that
 * way: the room in play routes its own transitions, so the pair
 * is the key and the room held is the answer, not the question. */
entry *ev_find_routed_entry(OraclesEnhancedView *v, uint8_t group, uint8_t from_room, OraclesGhostDirection dir)
{
    for (unsigned i = 0; i < v->slot_count; i++) {
        entry *c = &v->slots[i];
        if (c->used && c->routed && c->group == group && c->routed_from == from_room && c->routed_dir == (uint8_t)dir) return c;
    }
    return NULL;
}

entry *ev_take_routed_slot(OraclesEnhancedView *v, uint8_t group, uint8_t from_room, OraclesGhostDirection dir,
                           const uint8_t *wanted, unsigned wanted_count)
{
    entry *e = ev_find_routed_entry(v, group, from_room, dir);
    if (e) return e;
    e = claim_slot(take_victim(v, group, wanted, wanted_count, from_room), group, from_room);
    if (!e) return NULL;
    e->routed = 1;
    e->routed_from = from_room;
    e->routed_dir = (uint8_t)dir;
    e->near = 1;
    return e;
}

static void drop_routed(entry *c)
{
    uint8_t *state = c->settled_state;
    memset(c, 0, sizeof *c);
    c->settled_state = state;
}

/* The routed entries whose routine has no routing key, dropped: the room the
 * game would load that way depends on live state the cache key does not
 * cover, so a transition makes every one of them a question to ask again.
 * Those with a key hold while it does (ev_check_routing_keys). */
void ev_drop_routed_entries(OraclesEnhancedView *v)
{
    for (unsigned i = 0; i < v->slot_count; i++) {
        entry *c = &v->slots[i];
        if (c->used && c->routed && !c->routing_key_len) drop_routed(c);
    }
}

/* Every frame: an answer with a routing key goes as soon as the live key
 * differs from the one it was asked under (the Lost Woods' step in a sequence
 * moved), except, during a transition, the answer of the direction taken:
 * it is the room being entered, asked from the state before the transition
 * decided (and moved the key); it is judged again once Link has arrived. */
void ev_check_routing_keys(OraclesEnhancedView *v)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    for (unsigned i = 0; i < v->slot_count; i++) {
        entry *c = &v->slots[i];
        if (!c->used || !c->routed || !c->routing_key_len) continue;
        const int taking = ob->in_transition && c->routed_from == v->observer.ref_room ? (int)(ob->scroll_direction & 3u) : -1;
        uint8_t live[ORACLES_GHOST_ROUTING_KEY_BYTES];
        const size_t n = oracles_ghost_routing_key_snapshot(v->guest, oracles_ghost_room_routine(v->ghost, c->group, c->routed_from), live, sizeof live);
        if (n != c->routing_key_len || !oracles_enhanced_routed_answer_holds(c->routed_dir, taking, c->routing_key, live, n)) drop_routed(c);
    }
}
