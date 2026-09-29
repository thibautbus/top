#include "neighbour_check.h"

#include "ghost.h"
#include "objects.h"
#include "sprites.h"

#include <stdlib.h>
#include <string.h>

#define SCROLL_MODE_TRANSITION_LOAD 0x08u   /* cutscene01 sets it before loadTilesetAndRoomLayout (bank1.s) */
#define SCROLL_MODE_NORMAL 0x01u
#define TRANSITION_STATE_IDLE 0x02u
#define MAX_DECISION_TO_LOAD_FRAMES 16u
#define SETTLE_FRAMES 160u                  /* a load and a scroll, with margin: the view's own budget */
#define RING 64u   /* primeable frames kept: the witness is the newest, the aged run the oldest of the room */

typedef struct snapshot {
    uint32_t frame;
    uint8_t group, room;
    uint8_t *state;
} snapshot;

struct OraclesNeighbourCheck {
    OraclesCore *live;
    OraclesGuest *guest;
    OraclesGhost *ghost;
    OraclesObjects *live_objects;
    OraclesSprites *live_sprites;   /* the sprites of each frame, tagged by object (T2) */
    size_t state_size;
    snapshot ring[RING];
    unsigned ring_head, ring_count;
    unsigned lead;             /* frames of age asked of the snapshot: 0 the newest (the witness) */
    FILE *tsv;
    /* the transition in progress */
    int pending, awaiting_settle;
    uint32_t enter_frame;
    uint8_t from_group, from_room, direction, to_group, to_room;
    unsigned snapshot_age;
    OraclesGhostResult result;
    int have_result;
    /* totals */
    unsigned transitions, ran, no_snapshot, failed, wrong_room, large_room, key_changed, layout_differs;
    unsigned layout_different_transitions;
    unsigned equal, position_only, state_only, animation_phase, graphics_slot, other_difference, live_only, ghost_only;
    unsigned random_live_only, unnumbered_live_only, random_ghost_only, unnumbered_ghost_only;
    /* the capture: a second run of the same transition, objects frozen (T2) */
    int capture;
    unsigned captures, capture_equal, capture_objects, capture_different, capture_missing, capture_undrawn;
};

OraclesNeighbourCheck *oracles_neighbour_check_start(const uint8_t *rom, size_t rom_size, const OraclesCompatProfile *profile,
                                                     OraclesCore *live, OraclesGuest *live_guest, const char *dir, unsigned lead, int capture)
{
    OraclesNeighbourCheck *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->live = live;
    c->guest = live_guest;
    c->lead = lead < RING - 1u ? lead : RING - 2u;
    c->capture = capture != 0;
    c->ghost = oracles_ghost_create(rom, rom_size, profile);
    c->live_objects = oracles_objects_create(live_guest);
    c->live_sprites = oracles_sprites_create(live_guest);
    c->state_size = oracles_core_state_size(live);
    if (!c->ghost || !c->live_objects || !c->live_sprites || c->state_size != oracles_ghost_state_size(c->ghost)) {
        oracles_neighbour_check_stop(c);
        return NULL;
    }
    if (oracles_objects_add_hooks(live_guest) != 0 || oracles_sprites_add_hooks(live_guest) != 0) {
        oracles_neighbour_check_stop(c);
        return NULL;
    }
    char path[4096];
    snprintf(path, sizeof path, "%s/objects.tsv", dir);
    c->tsv = fopen(path, "w");
    if (c->tsv) fprintf(c->tsv, "frame\tfrom\tdir\tto\tsnapshot_age\tkind\tnumber\tid\tsubid\tvar03\tverdict\tcause\tlive_pos\tghost_pos\tfields\n");
    return c;
}

void oracles_neighbour_check_stop(OraclesNeighbourCheck *c)
{
    if (!c) return;
    if (c->tsv) fclose(c->tsv);
    for (unsigned i = 0; i < RING; i++) free(c->ring[i].state);
    oracles_objects_destroy(c->live_objects);
    oracles_sprites_destroy(c->live_sprites);
    if (c->ghost) oracles_ghost_destroy(c->ghost);
    free(c);
}

static void compare_now(OraclesNeighbourCheck *c, uint32_t frame);

/* The capture of the design (6.2): the same transition run again with the
 * objects frozen from the return of initializeRoom.  What it must produce is
 * the same objects as the run that let them live: freezing must not change
 * what the room creates, only stop it from moving. */
static void compare_capture(OraclesNeighbourCheck *c, const snapshot *s)
{
    OraclesGhostResult frozen;
    oracles_ghost_set_capture(c->ghost, 1);
    const int rc = oracles_ghost_run_ex(c->ghost, s->state, c->state_size, (OraclesGhostDirection)c->direction, 1, NULL, SETTLE_FRAMES, &frozen);
    oracles_ghost_set_capture(c->ghost, 0);
    if (rc != 1 || !frozen.settled || frozen.group != c->to_group || frozen.room != c->to_room) return;
    c->captures++;
    /* An object the capture kept but whose sprites are in no tag was not drawn:
     * the game stopped at its forty OAM entries, or it is invisible. */
    for (unsigned k = 0; k < frozen.object_count; k++) {
        int drawn = 0;
        for (unsigned j = 0; j < frozen.tag_count && !drawn; j++)
            if (!frozen.tags[j].terrain_effect && frozen.tags[j].kind == frozen.objects[k].kind && frozen.tags[j].slot == frozen.objects[k].slot) drawn = 1;
        if (!drawn) c->capture_undrawn++;
    }
    for (unsigned i = 0; i < c->result.object_count; i++) {
        const OraclesObjectRecord *a = &c->result.objects[i];
        const OraclesObjectRecord *b = NULL;
        for (unsigned k = 0; k < frozen.object_count; k++)
            if (frozen.objects[k].kind == a->kind && frozen.objects[k].slot == a->slot) { b = &frozen.objects[k]; break; }
        c->capture_objects++;
        if (!b) { c->capture_missing++; continue; }
        /* The pose may differ: the frozen run stops the animation where the
         * other one had gone on.  Identity, number and place must not. */
        if (a->id == b->id && a->subid == b->subid && a->var03 == b->var03 && a->number == b->number
            && a->yh == b->yh && a->xh == b->xh && a->y == b->y && a->x == b->x) c->capture_equal++;
        else c->capture_different++;
    }
}

/* ---- snapshots ------------------------------------------------------------------ */

void oracles_neighbour_check_frame_end(OraclesNeighbourCheck *c, uint32_t frame)
{
    if (!c) return;
    oracles_objects_latch(c->live_objects);   /* each object as it stands once initialised */
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    /* The end of the real entry: the game is back in normal play, its objects
     * have run their initialisation and are the ones the entry produced. */
    if (c->awaiting_settle
        && (oracles_guest_read8(c->guest, t->scroll_mode) & 0x7fu) == SCROLL_MODE_NORMAL
        && oracles_guest_read8(c->guest, t->screen_transition_state) == TRANSITION_STATE_IDLE) {
        compare_now(c, frame);
        c->awaiting_settle = 0;
    }
    if (!oracles_ghost_primeable(c->guest, NULL)) return;
    snapshot *s = &c->ring[c->ring_head];
    if (!s->state) { s->state = malloc(c->state_size); if (!s->state) return; }
    if (oracles_core_save_state(c->live, s->state, c->state_size) != 0) return;
    s->frame = frame;
    s->group = oracles_guest_read8(c->guest, t->active_group);
    s->room = oracles_guest_read8(c->guest, t->active_room);
    c->ring_head = (c->ring_head + 1u) % RING;
    if (c->ring_count < RING) c->ring_count++;
}

/* ---- comparison ------------------------------------------------------------------ */

static int same_key(const OraclesObjectRecord *a, const OraclesObjectRecord *b)
{
    return a->kind == b->kind && a->id == b->id && a->subid == b->subid && a->var03 == b->var03;
}

/* Everything but the position, which is judged apart: an object equal in every
 * other field and misplaced is the random placement's doing, not a difference
 * of state. */
/* Which fields differ, told apart: the pose of the moment (the blink bit of
 * `visible`, the direction, the animation block) moves with the frames the two
 * sides have run; the graphics slot depends on what the room loaded before it;
 * the rest is the object itself. */
typedef enum { FIELD_ANIMATION, FIELD_GFX_SLOT, FIELD_OTHER } field_class;

static field_class class_of_fields(const OraclesObjectRecord *a, const OraclesObjectRecord *b)
{
    if (a->enabled != b->enabled || a->state != b->state || a->oam_flags != b->oam_flags || a->zh != b->zh) return FIELD_OTHER;
    if (a->oam_tile_base != b->oam_tile_base) return FIELD_GFX_SLOT;
    return FIELD_ANIMATION;   /* visible, direction, oam_data_address */
}

static unsigned differing_fields(const OraclesObjectRecord *a, const OraclesObjectRecord *b, char *out, size_t capacity)
{
    unsigned n = 0;
    size_t used = 0;
    #define FIELD(name, member) do { \
        if (a->member != b->member) { \
            n++; \
            if (used < capacity) used += (size_t)snprintf(out + used, capacity - used, "%s%s=%02x/%02x", used ? ";" : "", name, a->member, b->member); \
        } \
    } while (0)
    FIELD("enabled", enabled);
    FIELD("state", state);
    FIELD("direction", direction);
    FIELD("visible", visible);
    FIELD("oam_flags", oam_flags);
    FIELD("oam_tile_base", oam_tile_base);
    FIELD("zh", zh);
    #undef FIELD
    if (a->oam_data_address != b->oam_data_address) {
        n++;
        if (used < capacity) used += (size_t)snprintf(out + used, capacity - used, "%soam_data=%04x/%04x", used ? ";" : "", a->oam_data_address, b->oam_data_address);
    }
    return n;
}

static int same_position(const OraclesObjectRecord *a, const OraclesObjectRecord *b)
{
    return a->y == b->y && a->yh == b->yh && a->x == b->x && a->xh == b->xh;
}

static const char *cause_of(OraclesNeighbourCheck *c, OraclesObjects *side, const OraclesObjectRecord *r)
{
    if (oracles_objects_randomly_placed(side, r)) return "random";
    if (!r->number) return "created-later";
    if (c->layout_differs) return "layout-differs";
    if (c->key_changed) return "key-changed";
    return "none";
}

static void row(OraclesNeighbourCheck *c, uint32_t frame, const OraclesObjectRecord *r,
                const char *verdict, const char *cause, const char *live_pos, const char *ghost_pos, const char *fields)
{
    if (!c->tsv) return;
    fprintf(c->tsv, "%u\t%u:%02x\t%u\t%u:%02x\t%u\t%u\t%u\t%02x\t%02x\t%02x\t%s\t%s\t%s\t%s\t%s\n",
            frame, c->from_group, c->from_room, c->direction, c->to_group, c->to_room, c->snapshot_age,
            r->kind, r->number, r->id, r->subid, r->var03, verdict, cause, live_pos, ghost_pos, fields);
}

/* Whether the state the ghost started from is still the state the entry
 * happened in, and whether the terrain differs: a room's own code reads
 * its layout to create some of its objects (the eye statues of Seasons scan
 * it), so a terrain that differs is the cause of an object one side has and
 * the other has not. */
static void note_entry_state(OraclesNeighbourCheck *c)
{
    /* The key of the terrain cache also tells whether the state the ghost
     * started from is still the state the entry happened in. */
    uint8_t key[ORACLES_GHOST_KEY_BYTES];
    const size_t key_len = oracles_ghost_key_snapshot(c->guest, key, sizeof key);
    c->key_changed = !(key_len && key_len == c->result.key_len && memcmp(key, c->result.key, key_len) == 0);
    /* The room's own code reads its layout to create some of its objects (the
     * eye statues of Seasons scan it for their statues): a terrain that differs
     * is the cause of an object one side has and the other has not. */
    c->layout_differs = 0;
    {
        const uint8_t *layout = oracles_guest_ptr(c->guest, oracles_guest_tables(c->guest)->room_layout, ORACLES_GHOST_LAYOUT_BYTES);
        if (layout)
            for (unsigned i = 0; i < ORACLES_GHOST_LAYOUT_BYTES && !c->layout_differs; i++) {
                if ((i & 0x0fu) >= 15u) continue;   /* the sixteenth column of the stride belongs to no room */
                if (layout[i] == c->result.layout[i]) continue;
                if (oracles_ghost_entry_dependent_tile(oracles_guest_tables(c->guest), layout[i]) || oracles_ghost_entry_dependent_tile(oracles_guest_tables(c->guest), c->result.layout[i])) continue;
                c->layout_differs = 1;
            }
    }
    if (c->layout_differs) c->layout_different_transitions++;
}

/* The ghost's object paired with a live one, by key: in the order of the
 * creation numbers, the rank among the objects of that key, not the slot
 * (docs/ARCHITECTURE.md, pairing).  NULL when the ghost has no such object. */
static const OraclesObjectRecord *ghost_paired_with(const OraclesObjectRecord *live, unsigned i,
                                                    const OraclesObjectRecord *ghost, unsigned ghost_count, unsigned *gi)
{
    const OraclesObjectRecord *l = &live[i];
    unsigned rank = 0;
    for (unsigned k = 0; k < i; k++) if (same_key(&live[k], l)) rank++;
    unsigned seen = 0;
    for (unsigned k = 0; k < ghost_count; k++) {
        if (!same_key(&ghost[k], l)) continue;
        if (seen++ != rank) continue;
        *gi = k;
        return &ghost[k];
    }
    return NULL;
}

/* A live object and the ghost's: equal, apart by one step of its own machine,
 * apart by its place alone, or different in its fields. */
static void judge_the_pair(OraclesNeighbourCheck *c, uint32_t frame, const OraclesObjectRecord *l, const OraclesObjectRecord *g)
{
    char fields[256], live_pos[16], ghost_pos[16];
    fields[0] = '\0';
    const unsigned differing = differing_fields(l, g, fields, sizeof fields);
    const int moved = !same_position(l, g);
    snprintf(live_pos, sizeof live_pos, "%02x,%02x", l->yh, l->xh);
    snprintf(ghost_pos, sizeof ghost_pos, "%02x,%02x", g->yh, g->xh);
    const char *cause = cause_of(c, c->live_objects, l);
    /* The ghost leaves Link in the room it enters, where the live entry has
     * him elsewhere: an object that waits for him in state 0 has started in
     * the ghost and not in the game.  Same identity, same place, one step of
     * its own machine apart: counted apart, not as a difference of the room. */
    /* Its own state 0 also gives an object its position when the room's
     * list gave none (a door controller): started on one side only, it
     * differs in place as well. */
    const int state_only = l->state == 0 && g->state != 0;
    if (!differing && !moved) { c->equal++; row(c, frame, l, "equal", cause, live_pos, ghost_pos, ""); }
    else if (state_only) { c->state_only++; row(c, frame, l, "state-only", "link-in-ghost", live_pos, ghost_pos, fields); }
    else if (!differing) { c->position_only++; row(c, frame, l, "position-only", cause, live_pos, ghost_pos, ""); }
    else {
        const field_class fc = class_of_fields(l, g);
        const char *field_cause = !strcmp(cause, "none")
            ? (fc == FIELD_ANIMATION ? "animation-phase" : fc == FIELD_GFX_SLOT ? "graphics-slot" : "none")
            : cause;
        if (fc == FIELD_ANIMATION && !moved) c->animation_phase++;
        else if (fc == FIELD_GFX_SLOT && !moved) c->graphics_slot++;
        else c->other_difference++;
        row(c, frame, l, "different", field_cause, live_pos, ghost_pos, fields);
    }
}

/* Each live object against the ghost's, paired by key and by rank. */
static void compare_live_objects(OraclesNeighbourCheck *c, uint32_t frame, const OraclesObjectRecord *live, unsigned live_count,
                                 const OraclesObjectRecord *ghost, unsigned ghost_count, int matched_ghost[ORACLES_OBJECT_RECORDS])
{
    char live_pos[16];
    for (unsigned i = 0; i < live_count; i++) {
        const OraclesObjectRecord *l = &live[i];
        unsigned gi = 0;
        const OraclesObjectRecord *g = ghost_paired_with(live, i, ghost, ghost_count, &gi);
        if (!g) {
            c->live_only++;
            const char *cause = cause_of(c, c->live_objects, l);
            if (!strcmp(cause, "random")) c->random_live_only++;
            else if (!strcmp(cause, "created-later")) c->unnumbered_live_only++;
            snprintf(live_pos, sizeof live_pos, "%02x,%02x", l->yh, l->xh);
            row(c, frame, l, "live-only", cause, live_pos, "-", "");
            continue;
        }
        matched_ghost[gi] = 1;
        judge_the_pair(c, frame, l, g);
    }
}

/* The objects the ghost has and the live game has not. */
static void report_ghost_only(OraclesNeighbourCheck *c, uint32_t frame, const OraclesObjectRecord *ghost, unsigned ghost_count,
                              const int matched_ghost[ORACLES_OBJECT_RECORDS])
{
    char ghost_pos[16];
    for (unsigned k = 0; k < ghost_count; k++) {
        if (matched_ghost[k]) continue;
        c->ghost_only++;
        const OraclesObjectRecord *g = &ghost[k];
        const char *cause = "none";
        if (!g->number) { cause = "created-later"; c->unnumbered_ghost_only++; }
        else if (c->layout_differs) cause = "layout-differs";
        else if (c->key_changed) cause = "key-changed";
        snprintf(ghost_pos, sizeof ghost_pos, "%02x,%02x", g->yh, g->xh);
        row(c, frame, g, "ghost-only", cause, "-", ghost_pos, "");
    }
}

static void compare_now(OraclesNeighbourCheck *c, uint32_t frame)
{
    if (!c->have_result) return;
    c->have_result = 0;
    OraclesObjectRecord live[ORACLES_OBJECT_RECORDS];
    oracles_objects_latch_remaining(c->live_objects);
    const unsigned live_count = oracles_objects_latched(c->live_objects, live, ORACLES_OBJECT_RECORDS);
    const OraclesObjectRecord *ghost = c->result.objects;
    const unsigned ghost_count = c->result.object_count;
    note_entry_state(c);
    int matched_ghost[ORACLES_OBJECT_RECORDS] = { 0 };
    compare_live_objects(c, frame, live, live_count, ghost, ghost_count, matched_ghost);
    report_ghost_only(c, frame, ghost, ghost_count, matched_ghost);
}

/* ---- transitions ------------------------------------------------------------------ */

void oracles_neighbour_check_event(OraclesNeighbourCheck *c, const OraclesGuestEvent *event)
{
    if (!c) return;
    oracles_objects_event(c->live_objects, event);
    oracles_sprites_event(c->live_sprites, event);
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    if (event->type == ORACLES_EVENT_ROOM_ENTER) {
        if (oracles_guest_read8(c->guest, t->scroll_mode) != SCROLL_MODE_TRANSITION_LOAD || c->ring_count == 0) return;
        const snapshot *newest = &c->ring[(c->ring_head + RING - 1u) % RING];
        if (event->frame - newest->frame > MAX_DECISION_TO_LOAD_FRAMES) return;
        c->pending = 1;
        c->enter_frame = event->frame;
        c->from_group = newest->group;
        c->from_room = newest->room;
        c->direction = (uint8_t)(oracles_guest_read8(c->guest, t->screen_transition_direction) & 3u);
        return;
    }
    if (event->type != ORACLES_EVENT_ROOM_INITIALIZED || !c->pending) return;
    c->pending = 0;
    c->transitions++;
    c->to_group = oracles_guest_read8(c->guest, t->active_group);
    c->to_room = oracles_guest_read8(c->guest, t->active_room);
    /* The snapshot `lead` primeable frames before the newest, staying in the
     * room the transition leaves: the view starts its own runs from states of
     * that age, and the placements that depend on the RNG may differ there. */
    const snapshot *s = NULL;
    for (unsigned back = 0; back < c->ring_count && back <= c->lead; back++) {
        const snapshot *candidate = &c->ring[(c->ring_head + RING - 1u - back) % RING];
        if (!candidate->state || candidate->group != c->from_group || candidate->room != c->from_room) break;
        s = candidate;
    }
    if (!s) { c->no_snapshot++; return; }
    c->snapshot_age = event->frame - s->frame;
    const OraclesGhostDirection dir = (OraclesGhostDirection)c->direction;
    if (oracles_ghost_run_ex(c->ghost, s->state, c->state_size, dir, 1, NULL, SETTLE_FRAMES, &c->result) != 1
        || !c->result.settled) { c->failed++; return; }
    if (c->result.group != c->to_group || c->result.room != c->to_room) { c->wrong_room++; return; }
    /* A large room (a dungeon, a large interior) is never shown as a neighbour:
     * the view deliberately shows it alone.  Its
     * objects are out of the measure, as they are out of the product. */
    if (c->result.room_is_large) { c->large_room++; return; }
    c->ran++;
    c->have_result = 1;
    if (c->capture) compare_capture(c, s);
    c->awaiting_settle = 1;   /* the live objects are read when the real entry settles */
}

/* ---- report ---------------------------------------------------------------------- */

void oracles_neighbour_check_summary(const OraclesNeighbourCheck *c, FILE *out)
{
    if (!c || !out) return;
    fprintf(out, "objects.transitions=%u\nobjects.compared=%u\nobjects.no_snapshot=%u\nobjects.failed=%u\nobjects.wrong_room=%u\nobjects.large_room=%u\n",
            c->transitions, c->ran, c->no_snapshot, c->failed, c->wrong_room, c->large_room);
    fprintf(out, "objects.equal=%u\nobjects.position_only=%u\nobjects.state_only=%u\nobjects.animation_phase=%u\nobjects.graphics_slot=%u\nobjects.different=%u\nobjects.live_only=%u\nobjects.ghost_only=%u\n",
            c->equal, c->position_only, c->state_only, c->animation_phase, c->graphics_slot, c->other_difference, c->live_only, c->ghost_only);
    fprintf(out, "objects.layout_different_transitions=%u\n", c->layout_different_transitions);
    if (c->capture) {
        unsigned frames = 0, uncovered = 0, gap = 0, dropped = 0;
        oracles_ghost_sprite_stats(c->ghost, &frames, &uncovered, &gap, &dropped);
        fprintf(out, "capture.runs=%u\ncapture.objects=%u\ncapture.equal=%u\ncapture.different=%u\ncapture.missing=%u\ncapture.undrawn=%u\n",
                c->captures, c->capture_objects, c->capture_equal, c->capture_different, c->capture_missing, c->capture_undrawn);
        fprintf(out, "capture.ghost_frames_oam_full=%u\n", oracles_ghost_frames_oam_full(c->ghost));
        fprintf(out, "capture.ghost_frames=%u\ncapture.ghost_frames_uncovered=%u\ncapture.ghost_worst_gap=%u\ncapture.ghost_tags_dropped=%u\n",
                frames, uncovered, gap, dropped);
    }
    {   /* The tagging of the sprites, checked against hOamTail on every frame drawn (T2). */
        unsigned frames = 0, uncovered = 0, gap = 0, dropped = 0;
        oracles_sprites_stats(c->live_sprites, &frames, &uncovered, &gap, &dropped);
        fprintf(out, "sprites.frames=%u\nsprites.frames_uncovered=%u\nsprites.worst_gap=%u\nsprites.tags_dropped=%u\n", frames, uncovered, gap, dropped);
    }
    fprintf(out, "objects.random_live_only=%u\nobjects.created_later_live_only=%u\nobjects.created_later_ghost_only=%u\n",
            c->random_live_only, c->unnumbered_live_only, c->unnumbered_ghost_only);
}

void oracles_neighbour_check_report(OraclesNeighbourCheck *c, FILE *out)
{
    if (!c || !out) return;
    fprintf(out, "neighbour objects: %u scrolling transitions, %u compared (no snapshot %u, run failed %u, wrong room %u, large room %u)\n",
            c->transitions, c->ran, c->no_snapshot, c->failed, c->wrong_room, c->large_room);
    {
        unsigned frames = 0, uncovered = 0, gap = 0, dropped = 0;
        oracles_sprites_stats(c->live_sprites, &frames, &uncovered, &gap, &dropped);
        fprintf(out, "  sprites tagged by object: %u frames drawn, %u where the tags did not add up to hOamTail (worst gap %u entries), %u tags dropped\n",
                frames, uncovered, gap, dropped);
    }
    if (c->capture)
        fprintf(out, "  capture (objects frozen): %u runs, %u objects, %u identical, %u different, %u missing\n",
                c->captures, c->capture_objects, c->capture_equal, c->capture_different, c->capture_missing);
    fprintf(out, "  objects: equal %u, position only %u, state only (Link in the ghost) %u, animation phase %u, graphics slot %u, otherwise different %u,"
                 " live only %u (random %u, created later %u), ghost only %u (created later %u)\n",
            c->equal, c->position_only, c->state_only, c->animation_phase, c->graphics_slot, c->other_difference,
            c->live_only, c->random_live_only, c->unnumbered_live_only, c->ghost_only, c->unnumbered_ghost_only);
}
