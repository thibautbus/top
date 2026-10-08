#include "ghost_check.h"

#include <inttypes.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define SCROLL_MODE_TRANSITION_LOAD 0x08u   /* cutscene00 sets it before loadTilesetAndRoomLayout (bank1.s) */
#define GHOST_MAX_FRAMES 16u
#define MAX_DECISION_TO_LOAD_FRAMES 16u      /* a scrolling transition loads its room a few frames after the decision */
#define FRAMES_HIST 16u
#define DIFF_LIST 24u
#define HOST_FRAME_US 16742.0                /* 59.7275 Hz */

typedef struct snapshot {
    uint32_t frame;
    uint8_t group, room;
    uint8_t *state;
} snapshot;

struct OraclesGhostCheck {
    OraclesCore *live;
    OraclesGuest *guest;
    OraclesGhost *ghost;
    size_t state_size;
    unsigned lead;
    int threaded, trace;
    OraclesGhostKeyRange key[ORACLES_GHOST_KEY_RANGES], exempt[ORACLES_GHOST_KEY_RANGES];
    unsigned key_count, exempt_count;
    uint32_t *outside_counts;            /* reads outside the key and the exempt list, per address, over the run */
    unsigned outside_addresses;          /* distinct such addresses */
    unsigned outside_transitions;        /* transitions with at least one such read */
    unsigned exempt_reads;               /* reads of the exempt list (oracles_ghost_key_exempt_ranges) */
    uint64_t other_thread_reads;         /* reads by another thread while the substitutions waited, set aside */
    unsigned entry_dependent_tiles;      /* differing tiles that are entry-dependent, not counted as differences */
    unsigned untraced_runs;              /* traced runs that recorded no read at all: the trace was not armed */
    unsigned reads_dropped;              /* reads a run could not record, its list full: unseen, possibly outside the key */
    snapshot *ring;
    unsigned ring_size, ring_head, ring_count;
    /* a transition in progress */
    int pending;
    uint32_t enter_frame;
    uint8_t from_group, from_room, direction;
    /* output */
    FILE *tsv;
    /* statistics */
    unsigned transitions, equal, differing, wrong_room, no_snapshot, load_failed, not_primeable, timeouts;
    unsigned saves;
    double save_us_total;
    const char *reasons[16];             /* why frames were not primeable, with counts */
    unsigned reason_counts[16], reason_n;
    double begin_us_total, step_us_total, latency_us_total, latency_us_max;
    unsigned frames_hist[FRAMES_HIST];
    uint64_t diff_tiles_total;
    uint32_t pair_counts[256][256];
    uint32_t pos_counts[ORACLES_GHOST_LAYOUT_BYTES];
};

static double now_us(void)
{
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1e6 / (double)frequency.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
#endif
}

OraclesGhostCheck *oracles_ghost_check_start(const uint8_t *rom, size_t rom_size, const OraclesCompatProfile *profile,
                                             OraclesCore *live, OraclesGuest *live_guest,
                                             const char *dir, unsigned lead, int threaded, int trace)
{
    OraclesGhostCheck *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->live = live;
    c->guest = live_guest;
    c->lead = lead;
    c->threaded = threaded;
    c->trace = trace;
    c->ghost = oracles_ghost_create(rom, rom_size, profile, oracles_core_kind(live));
    if (!c->ghost) { free(c); return NULL; }
    if (trace) {
        oracles_ghost_set_trace(c->ghost, 1);
        c->outside_counts = calloc(65536u, sizeof *c->outside_counts);
    }
    c->key_count = oracles_ghost_key_ranges(oracles_guest_profile(live_guest), c->key, sizeof c->key / sizeof c->key[0]);
    c->exempt_count = oracles_ghost_key_exempt_ranges(oracles_guest_profile(live_guest), c->exempt, sizeof c->exempt / sizeof c->exempt[0]);
    c->state_size = oracles_core_state_size(live);
    if (c->state_size != oracles_ghost_state_size(c->ghost)) { oracles_ghost_destroy(c->ghost); free(c); return NULL; }
    c->ring_size = lead + 2u;
    c->ring = calloc(c->ring_size, sizeof *c->ring);
    if (!c->ring) { oracles_ghost_destroy(c->ghost); free(c); return NULL; }
    char path[4096];
    snprintf(path, sizeof path, "%s/transitions.tsv", dir);
    c->tsv = fopen(path, "w");
    if (c->tsv) fprintf(c->tsv, "frame\tfrom\tdir\tto\tghost_to\tsnapshot_age\tghost_frames\tbegin_us\tstep_us\tlatency_us\tstatus\tdiff_tiles\tentry_dependent_tiles\treads_outside_key\ttraced_reads\tdiffs\n");
    return c;
}

void oracles_ghost_check_stop(OraclesGhostCheck *c)
{
    if (!c) return;
    if (c->tsv) fclose(c->tsv);
    for (unsigned i = 0; i < c->ring_size; i++) free(c->ring[i].state);
    free(c->ring);
    free(c->outside_counts);
    oracles_ghost_destroy(c->ghost);
    free(c);
}

/* ---- snapshots ------------------------------------------------------------------ */

void oracles_ghost_check_frame_end(OraclesGhostCheck *c, uint32_t frame)
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    const char *reason = NULL;
    if (!oracles_ghost_primeable(c->guest, &reason)) {
        unsigned i = 0;
        while (i < c->reason_n && c->reasons[i] != reason) i++;
        if (i == c->reason_n && i < 16) { c->reasons[i] = reason; c->reason_n++; }
        if (i < 16) c->reason_counts[i]++;
        return;
    }
    snapshot *s = &c->ring[c->ring_head];
    if (!s->state) { s->state = malloc(c->state_size); if (!s->state) return; }
    const double t0 = now_us();
    if (oracles_core_save_state(c->live, s->state, c->state_size) != 0) return;
    c->save_us_total += now_us() - t0;
    c->saves++;
    s->frame = frame;
    s->group = oracles_guest_read8(c->guest, t->active_group);
    s->room = oracles_guest_read8(c->guest, t->active_room);
    c->ring_head = (c->ring_head + 1u) % c->ring_size;
    if (c->ring_count < c->ring_size) c->ring_count++;
}

/* The snapshot `lead` primeable frames before the newest, staying in the room the transition left. */
static const snapshot *pick_snapshot(const OraclesGhostCheck *c)
{
    const snapshot *chosen = NULL;
    for (unsigned back = 0; back < c->ring_count && back <= c->lead; back++) {
        const unsigned index = (c->ring_head + c->ring_size - 1u - back) % c->ring_size;
        const snapshot *s = &c->ring[index];
        if (!s->state || s->group != c->from_group || s->room != c->from_room) break;
        chosen = s;
    }
    return chosen;
}

/* ---- transitions ------------------------------------------------------------------ */

static const char *status_name(OraclesGhostStatus s)
{
    switch (s) {
    case ORACLES_GHOST_OK: return "ok";
    case ORACLES_GHOST_LOAD_FAILED: return "load-failed";
    case ORACLES_GHOST_NOT_PRIMEABLE: return "not-primeable";
    case ORACLES_GHOST_TIMEOUT: return "timeout";
    case ORACLES_GHOST_INTERRUPTED: return "interrupted";
    }
    return "?";
}

/* Reads of this run outside the key and the exempt list: counted, never silently accepted. */
static unsigned reads_outside_key(OraclesGhostCheck *c, const OraclesGhostResult *r)
{
    unsigned outside = 0;
    for (unsigned i = 0; i < r->read_count; i++) {
        const uint16_t a = r->reads[i];
        int inside = 0, exempt = 0;
        for (unsigned k = 0; k < c->key_count && !inside; k++)
            if (a >= c->key[k].address && a < c->key[k].address + c->key[k].length) inside = 1;
        for (unsigned k = 0; k < c->exempt_count && !inside && !exempt; k++)
            if (a >= c->exempt[k].address && a < c->exempt[k].address + c->exempt[k].length) exempt = 1;
        if (inside) continue;
        if (exempt) { c->exempt_reads++; continue; }
        outside++;
        if (c->outside_counts) { if (!c->outside_counts[a]) c->outside_addresses++; c->outside_counts[a]++; }
    }
    if (outside) c->outside_transitions++;
    c->reads_dropped += r->reads_dropped;
    return outside + r->reads_dropped;
}

static void compare_layouts(OraclesGhostCheck *c, uint32_t frame, const snapshot *s, const OraclesGhostResult *r,
                            uint8_t to_group, uint8_t to_room, const uint8_t *live_layout,
                            double begin_us, double step_us, double latency_us)
{
    char diffs[DIFF_LIST * 12 + 16] = "";
    unsigned diff_tiles = 0, entry_tiles = 0, outside = 0;
    const char *status = status_name(r->status);
    if (r->status == ORACLES_GHOST_OK) {
        if (r->group != to_group || r->room != to_room) { status = "wrong-room"; c->wrong_room++; }
        else {
            size_t used = 0;
            if (c->trace) { outside = reads_outside_key(c, r); c->other_thread_reads += r->reads_other_thread; if (r->traced_reads == 0) c->untraced_runs++; }
            for (unsigned i = 0; i < ORACLES_GHOST_LAYOUT_BYTES; i++) {
                if ((i & 0x0fu) >= 15u) continue;   /* the sixteenth column of the stride is not part of any room */
                if (live_layout[i] == r->layout[i]) continue;
                if (oracles_ghost_entry_dependent_tile(oracles_guest_tables(c->guest), live_layout[i]) || oracles_ghost_entry_dependent_tile(oracles_guest_tables(c->guest), r->layout[i])) {
                    entry_tiles++;
                    c->entry_dependent_tiles++;
                    continue;
                }
                diff_tiles++;
                c->pair_counts[live_layout[i]][r->layout[i]]++;
                c->pos_counts[i]++;
                if (diff_tiles <= DIFF_LIST)
                    used += (size_t)snprintf(diffs + used, sizeof diffs - used, "%s%02x=%02x/%02x", used ? ";" : "", i, live_layout[i], r->layout[i]);
            }
            if (diff_tiles) { status = "different"; c->differing++; c->diff_tiles_total += diff_tiles; }
            else { status = "equal"; c->equal++; }
        }
        c->frames_hist[r->guest_frames < FRAMES_HIST ? r->guest_frames : FRAMES_HIST - 1u]++;
        c->begin_us_total += begin_us;
        c->step_us_total += step_us;
        c->latency_us_total += latency_us;
        if (latency_us > c->latency_us_max) c->latency_us_max = latency_us;
    } else if (r->status == ORACLES_GHOST_LOAD_FAILED) c->load_failed++;
    else if (r->status == ORACLES_GHOST_NOT_PRIMEABLE) c->not_primeable++;
    else c->timeouts++;
    if (c->tsv)
        fprintf(c->tsv, "%u\t%u:%02x\t%u\t%u:%02x\t%u:%02x\t%u\t%u\t%.0f\t%.0f\t%.0f\t%s\t%u\t%u\t%u\t%" PRIu32 "\t%s\n",
                frame, c->from_group, c->from_room, c->direction, to_group, to_room, r->group, r->room,
                frame - s->frame, r->guest_frames, begin_us, step_us, latency_us, status, diff_tiles, entry_tiles, outside, r->traced_reads, diffs);
}

/* A scrolling transition begins: the room left is the one of the newest
 * snapshot (wActiveRoom already names the room entered when initializeRoom
 * runs, and the frames between the decision and the load are not primeable). */
static void note_transition_start(OraclesGhostCheck *c, const OraclesGuestTables *t, const OraclesGuestEvent *event)
{
    if (oracles_guest_read8(c->guest, t->scroll_mode) != SCROLL_MODE_TRANSITION_LOAD || c->ring_count == 0) return;
    const snapshot *newest = &c->ring[(c->ring_head + c->ring_size - 1u) % c->ring_size];
    if (event->frame - newest->frame > MAX_DECISION_TO_LOAD_FRAMES) return;
    c->pending = 1;
    c->enter_frame = event->frame;
    c->from_group = newest->group;
    c->from_room = newest->room;
    c->direction = (uint8_t)(oracles_guest_read8(c->guest, t->screen_transition_direction) & 3u);
}

void oracles_ghost_check_event(OraclesGhostCheck *c, const OraclesGuestEvent *event)
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    if (event->type == ORACLES_EVENT_ROOM_ENTER) { note_transition_start(c, t, event); return; }
    if (event->type != ORACLES_EVENT_ROOM_INITIALIZED || !c->pending) return;
    c->pending = 0;
    c->transitions++;
    const uint8_t to_group = oracles_guest_read8(c->guest, t->active_group);
    const uint8_t to_room = oracles_guest_read8(c->guest, t->active_room);
    uint8_t live_layout[ORACLES_GHOST_LAYOUT_BYTES];
    const uint8_t *layout = oracles_guest_ptr(c->guest, t->room_layout, ORACLES_GHOST_LAYOUT_BYTES);
    if (!layout) return;
    memcpy(live_layout, layout, sizeof live_layout);

    const snapshot *s = pick_snapshot(c);
    if (!s) {
        c->no_snapshot++;
        if (c->tsv) fprintf(c->tsv, "%u\t%u:%02x\t%u\t%u:%02x\t-\t-\t-\t-\t-\t-\tno-snapshot\t-\t-\t-\t-\t\n",
                            event->frame, c->from_group, c->from_room, c->direction, to_group, to_room);
        return;
    }
    OraclesGhostResult r;
    double begin_us = 0, step_us = 0, latency_us = 0;
    const OraclesGhostDirection dir = (OraclesGhostDirection)c->direction;
    if (c->threaded) {
        const double t0 = now_us();
        if (oracles_ghost_request(c->ghost, s->state, c->state_size, dir, GHOST_MAX_FRAMES) != 0) return;
        while (oracles_ghost_poll(c->ghost, &r) == 0) sched_yield();
        latency_us = now_us() - t0;
    } else {
        const double t0 = now_us();
        const int begun = oracles_ghost_begin(c->ghost, s->state, c->state_size, dir, &r);
        const double t1 = now_us();
        begin_us = t1 - t0;
        if (begun == 0) oracles_ghost_step(c->ghost, GHOST_MAX_FRAMES, GHOST_MAX_FRAMES, &r);
        step_us = now_us() - t1;
        latency_us = begin_us + step_us;
    }
    compare_layouts(c, event->frame, s, &r, to_group, to_room, live_layout, begin_us, step_us, latency_us);
}

/* ---- the whole load's reads (--ghost-trace-load) ----------------------------------- */

/* The save file's region in the Ages layout (wFileStart $c5b0 to the end of
 * the room flags, $cb00): the state that lasts, where a byte the load reads
 * and no account names could change a neighbour's terrain under its entry. */
#define FILE_REGION_START 0xc5b0u
#define FILE_REGION_END 0xcb00u

void oracles_ghost_check_trace_load(OraclesGhostCheck *c)
{
    if (c && c->trace) oracles_ghost_set_trace_load(c->ghost, 1);
}

static int in_ranges(const OraclesGhostKeyRange *r, unsigned n, unsigned a)
{
    for (unsigned k = 0; k < n; k++) if (a >= r[k].address && a < r[k].address + r[k].length) return 1;
    return 0;
}

/* The addresses the load read outside the substitutions, in the file region
 * or in the key, that neither the coarse list nor the exempt list names: an
 * entry records none of them, so a change there would not drop it. */
static unsigned load_outside_account(const OraclesGhostCheck *c, uint16_t *out, unsigned max)
{
    const uint32_t *reads = oracles_ghost_load_read_counts(c->ghost);
    if (!reads) return 0;
    OraclesGhostKeyRange coarse[ORACLES_GHOST_KEY_RANGES];
    const unsigned coarse_count = oracles_ghost_key_coarse_ranges(oracles_guest_tables(c->guest), coarse, ORACLES_GHOST_KEY_RANGES);
    unsigned n = 0;
    for (unsigned a = 0xc000u; a < 0x10000u; a++) {
        const int watched = (a >= FILE_REGION_START && a < FILE_REGION_END) || in_ranges(c->key, c->key_count, a);
        if (!reads[a] || !watched || in_ranges(c->exempt, c->exempt_count, a) || in_ranges(coarse, coarse_count, a)) continue;
        if (out && n < max) out[n] = (uint16_t)a;
        n++;
    }
    return n;
}

/* ---- report ---------------------------------------------------------------------- */

void oracles_ghost_check_summary(const OraclesGhostCheck *c, FILE *out)
{
    fprintf(out, "ghost.transitions=%u\nghost.equal=%u\nghost.different=%u\nghost.different_tiles=%" PRIu64 "\nghost.wrong_room=%u\nghost.no_snapshot=%u\nghost.load_failed=%u\nghost.not_primeable=%u\nghost.timeout=%u\n",
            c->transitions, c->equal, c->differing, c->diff_tiles_total, c->wrong_room, c->no_snapshot, c->load_failed, c->not_primeable, c->timeouts);
    /* With --ghost-trace: distinct addresses read outside the key and the exempt list (the closed account), and reads of the exempt list. */
    if (c->trace) fprintf(out, "ghost.reads_outside_key=%u\nghost.exempt_reads=%u\nghost.other_thread_reads=%" PRIu64 "\nghost.untraced_runs=%u\nghost.reads_dropped=%u\n",
                          c->outside_addresses, c->exempt_reads, c->other_thread_reads, c->untraced_runs, c->reads_dropped);
    if (oracles_ghost_load_read_counts(c->ghost)) fprintf(out, "ghost.load_outside_account=%u\n", load_outside_account(c, NULL, 0));
    unsigned overflow = 0, purged = 0;
    oracles_ghost_dropped_returns(c->ghost, &overflow, &purged);
    fprintf(out, "ghost.returns_overflow=%u\nghost.returns_purged=%u\n", overflow, purged);   /* return hooks the ghost's guest lost */
}

void oracles_ghost_check_report(OraclesGhostCheck *c, FILE *out)
{
    const unsigned ran = c->equal + c->differing + c->wrong_room;
    fprintf(out, "ghost: %u scrolling transitions on the route; snapshot %zu bytes, saved %u times in %.0f us on average, lead %u frames, %s mode, read trace %s\n",
            c->transitions, c->state_size, c->saves, c->saves ? c->save_us_total / c->saves : 0.0, c->lead, c->threaded ? "threaded" : "synchronous", c->trace ? "on" : "off");
    fprintf(out, "  equal %u, different %u (%" PRIu64 " tiles), entry-dependent tiles set aside %u, wrong room %u, no snapshot %u, load failed %u, not primeable %u, timeout %u\n",
            c->equal, c->differing, c->diff_tiles_total, c->entry_dependent_tiles, c->wrong_room, c->no_snapshot, c->load_failed, c->not_primeable, c->timeouts);
    if (c->reason_n) {
        fprintf(out, "  frames not primeable:");
        for (unsigned i = 0; i < c->reason_n; i++) fprintf(out, "%s %u because %s", i ? ";" : "", c->reason_counts[i], c->reasons[i]);
        fprintf(out, "\n");
    }
    if (ran) {
        fprintf(out, "  per neighbour: load and prime %.0f us, frames %.0f us, latency %.0f us on average and %.0f us at most (%.2f host frames at 59.73 Hz)\n",
                c->begin_us_total / ran, c->step_us_total / ran, c->latency_us_total / ran, c->latency_us_max, c->latency_us_max / HOST_FRAME_US);
        fprintf(out, "  guest frames until the room is initialized:");
        for (unsigned i = 0; i < FRAMES_HIST; i++) if (c->frames_hist[i]) fprintf(out, " %u frames x%u", i, c->frames_hist[i]);
        fprintf(out, "\n");
    }
    unsigned pairs = 0;
    for (unsigned a = 0; a < 256; a++) for (unsigned b = 0; b < 256; b++) if (c->pair_counts[a][b]) pairs++;
    if (pairs) {
        fprintf(out, "  differing tiles, live/ghost pairs:");
        for (unsigned a = 0; a < 256; a++) for (unsigned b = 0; b < 256; b++)
            if (c->pair_counts[a][b]) fprintf(out, " %02x/%02x x%u", a, b, c->pair_counts[a][b]);
        fprintf(out, "\n");
    }
    const uint32_t *reads = oracles_ghost_read_counts(c->ghost);
    if (reads) {
        unsigned wram = 0, hram = 0, io = 0, rom = 0, other = 0;
        for (unsigned a = 0; a < 65536u; a++) {
            if (!reads[a]) continue;
            if (a < 0x8000u) rom++;
            else if (a >= 0xc000u && a < 0xe000u) wram++;
            else if (a >= 0xff80u && a < 0xffffu) hram++;
            else if (a >= 0xff00u && a < 0xff80u) io++;
            else other++;
        }
        fprintf(out, "  applyAllTileSubstitutions read %u distinct WRAM addresses, %u HRAM, %u IO, %u ROM, %u other (reads.tsv)\n", wram, hram, io, rom, other);
        unsigned outside_addresses = 0;
        for (unsigned a = 0; a < 65536u; a++) if (c->outside_counts && c->outside_counts[a]) outside_addresses++;
        fprintf(out, "  cache key: %u ranges, %u exempt (%u reads of them); reads outside both: %u addresses on %u transitions; reads by other threads while the substitutions waited, set aside: %" PRIu64 "; runs without a single traced read (anomaly) %u",
                c->key_count, c->exempt_count, c->exempt_reads, outside_addresses, c->outside_transitions, c->other_thread_reads, c->untraced_runs);
        if (outside_addresses) {
            fprintf(out, ":");
            for (unsigned a = 0; a < 65536u; a++) if (c->outside_counts[a]) fprintf(out, " %04x x%" PRIu32, a, c->outside_counts[a]);
        }
        fprintf(out, "\n");
    }
    const uint32_t *load = oracles_ghost_load_read_counts(c->ghost);
    if (load) {
        uint16_t outside[512];
        const unsigned n = load_outside_account(c, outside, 512);
        fprintf(out, "  outside the substitutions, the load read %u addresses of the file region ($c5b0-$caff) or of the key that the coarse and exempt lists do not name", n);
        if (n) fprintf(out, ":");
        for (unsigned i = 0; i < n && i < 512; i++) fprintf(out, " %04x x%" PRIu32, outside[i], load[outside[i]]);
        fprintf(out, "\n");
    }
}

void oracles_ghost_check_write_reads(OraclesGhostCheck *c, const char *dir)
{
    const uint32_t *reads = oracles_ghost_read_counts(c->ghost);
    if (!reads) return;
    char path[4096];
    snprintf(path, sizeof path, "%s/reads.tsv", dir);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "address\tcount\n");
    for (unsigned a = 0; a < 65536u; a++) if (reads[a]) fprintf(f, "%04x\t%" PRIu32 "\n", a, reads[a]);
    fclose(f);
}
