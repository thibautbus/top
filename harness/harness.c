/* Differential harness: replays a route without a window and
 * writes one fingerprint line per frame, with or without the guest hooks, so
 * that two runs can be compared frame by frame.
 *
 *   oracles-harness --rom ROM --route ROUTE [--frames N] [--hooks on|off]
 *                   [--out FINGERPRINTS.tsv] [--corrupt-thread1-at FRAME]
 *   oracles-harness --compare A.tsv B.tsv
 *
 * Columns: frame, framebuffer, live WRAM (dead stack bytes excluded), HRAM,
 * OAM, VRAM.  --corrupt-thread1-at writes one byte into the saved context of
 * thread 1 (the game thread) at that frame: the harness's sensitivity test expects
 * the fingerprints to diverge from the next frame on.
 *
 * --ghost-check DIR runs the ghost instance against every scrolling
 * transition of the route (ghost_check.h). */
#include "backends.h"
#include "core.h"
#include "enhanced_check.h"
#include "room_transition.h"
#include "animation_check.h"
#include "object_sprites_check.h"
#include "hotkeys_check.h"
#include "harness_options.h"
#include "coverage.h"
#include "neighbour_check.h"
#include "frame_check.h"
#include "ghost_check.h"
#include "guest.h"
#include "guest_fingerprint.h"
#include "guest_struct_offsets.h"
#include "mod_session.h"
#include "bps.h"
#include "rom.h"
#include "route.h"
#include "sha1.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct run {
    OraclesCore *core;
    const OraclesCompatProfile *profile;
    OraclesGuest *guest;      /* NULL with --hooks off */
    OraclesFrameCheck *check; /* NULL without --render-check */
    OraclesGhostCheck *ghost; /* NULL without --ghost-check */
    OraclesEnhancedCheck *enhanced; /* NULL without --enhanced-check */
    OraclesRoomTransition *transition; /* NULL without --continuous-transitions */
    OraclesCoverage *coverage; /* what the route covers, with a guest */
    OraclesAnimationCheck *animation; /* the neighbours' animation reader against the live game, with the Enhanced check */
    OraclesObjectSpritesCheck *object_sprites; /* the objects' sprites built from the game's data against the live OAM, with the Enhanced check */
    OraclesNeighbourCheck *neighbours; /* NULL without --neighbour-objects-check */
    OraclesHotkeysCheck *hotkeys; /* with --hotkeys-check, or when the route carries inventory exchanges */
    OraclesHotkeysLive *hotkeys_live; /* with --hotkeys-live: scripted keys over the route's inputs */
    unsigned dumps;               /* --dump-at, as many times as wanted */
    uint32_t dump_at[HARNESS_MAX_DUMPS];
    const char *dump_path[HARNESS_MAX_DUMPS];
    OraclesModSession *mod;   /* --mods */
    OraclesGame game;
    OraclesRoute route;
    FILE *out;
    FILE *positions;          /* --positions */
    FILE *keys_read;          /* --keys-read */
    uint32_t keys_polls;      /* the game's input polls counted by the guest at the last frame's end */
    uint32_t corrupt_at;      /* 0: never */
    unsigned events;
    size_t journal_writes;
} run;

static int input_source(void *opaque, uint32_t frame, unsigned *mask)
{
    run *r = opaque;
    const int have = oracles_route_mask_at(&r->route, frame, mask);
    if (!r->hotkeys_live) return have;
    *mask = oracles_hotkeys_live_keys(r->hotkeys_live, frame, have ? *mask : 0u);   /* the scripted keys play over the route's inputs */
    return 1;
}

static void on_event(void *opaque, const OraclesGuestEvent *event)
{
    run *r = opaque;
    r->events++;
    if (r->coverage) oracles_coverage_event(r->coverage, event);
    if (r->neighbours) oracles_neighbour_check_event(r->neighbours, event);
    if (r->ghost) oracles_ghost_check_event(r->ghost, event);
    if (r->enhanced) oracles_enhanced_check_event(r->enhanced, event);
    if (r->object_sprites) oracles_object_sprites_check_event(r->object_sprites, event);
    if (r->hotkeys) oracles_hotkeys_check_event(r->hotkeys, event);
}

static void on_frame_begin(void *opaque, uint32_t frame)
{
    run *r = opaque;
    if (r->hotkeys_live && r->enhanced) oracles_hotkeys_live_hotbar(r->hotkeys_live, oracles_enhanced_check_view(r->enhanced));
    if (r->guest) oracles_guest_set_frame(r->guest, frame);
    if (r->check) oracles_frame_check_frame_begin(r->check, frame);
}

static void on_frame_end(void *opaque, uint32_t frame)
{
    run *r = opaque;
    /* Fingerprints always go through a guest for the bus; a run without hooks attaches one silently here. */
    OraclesGuest *bus = r->guest;
    int transient = 0;
    if (!bus) { bus = oracles_guest_attach(r->core, r->profile); transient = 1; }
    if (!bus) return;
    if (r->corrupt_at && frame == r->corrupt_at) {
        /* The saved context of thread 1 (resumeThreadInAFrames, bank0.s): BC, DE,
         * HL then the resume address, from the saved SP up.  The resume address
         * is the byte that matters: one bit flipped, the thread resumes elsewhere
         * and the game diverges from the next frame on. */
        const uint16_t sp = oracles_guest_thread_sp(bus, 1);
        const OraclesGuestSym sym = { 0, (uint16_t)(sp + 6u) };
        uint8_t *byte = (uint8_t *)oracles_guest_ptr(bus, sym, 1);
        if (byte) *byte ^= 0x01;
    }
    if (r->out) oracles_guest_write_fingerprint(bus, r->core, frame, r->out);
    if (r->positions) {
        /* What the game is at, whatever the core: the fingerprints, hashes of the memory, differ from one core to the other. */
        const OraclesGuestTables *t = oracles_guest_tables(bus);
        const uint8_t *link = oracles_guest_object(bus, 0, 0);
        fprintf(r->positions, "%u\t%u\t%u\t%u\t%u\n", frame, oracles_guest_read8(bus, t->active_group), oracles_guest_read8(bus, t->active_room),
                link ? link[ORACLES_OBJ_Y + 1] : 0u, link ? link[ORACLES_OBJ_X + 1] : 0u);
    }
    if (r->keys_read) {
        /* What the game's input poll (pollInput, the one writer of wKeysPressed) read in this frame, or in the last one it
         * ran (buttons in the low nibble and directions in the high one there, the other way round in a route), and
         * whether it ran in this frame: the guest counts its writes, which a run without hooks does not see ("-"). */
        const uint8_t pressed = oracles_guest_read8(bus, oracles_guest_tables(bus)->keys_pressed);
        const uint32_t polls = oracles_guest_keys_polls(r->guest);
        fprintf(r->keys_read, "%u\t%02x\t%s\n", frame, (unsigned)(((pressed & 0x0fu) << 4) | (pressed >> 4)),
                !r->guest ? "-" : polls != r->keys_polls ? "1" : "0");
        r->keys_polls = polls;
    }
    for (unsigned d = 0; d < r->dumps; d++) if (frame == r->dump_at[d]) {   /* the live state at the end of a frame (oracles_guest_live_dump): to name the bytes two replays differ by */
        FILE *dump = fopen(r->dump_path[d], "wb");
        uint8_t live[ORACLES_GUEST_LIVE_DUMP_BYTES];
        if (dump) { if (oracles_guest_live_dump(bus, live) == 0) fwrite(live, 1, sizeof live, dump); fclose(dump); }
    }
    oracles_mod_session_frame_end(r->mod, frame);
    if (r->coverage) oracles_coverage_frame_end(r->coverage, frame);
    if (r->hotkeys_live) oracles_hotkeys_live_frame_end(r->hotkeys_live, frame);
    if (r->hotkeys) { unsigned keys = 0; oracles_route_mask_at(&r->route, frame, &keys); oracles_hotkeys_check_frame_end(r->hotkeys, frame, keys); }
    if (r->neighbours) oracles_neighbour_check_frame_end(r->neighbours, frame);
    if (r->ghost) oracles_ghost_check_frame_end(r->ghost, frame);
    if (r->enhanced) oracles_enhanced_check_frame_end(r->enhanced, frame);   /* reads the frame's journal: before it is cleared */
    if (r->animation && r->enhanced && oracles_enhanced_check_reloaded_at(r->enhanced, frame)) oracles_animation_check_reset(r->animation);
    if (r->animation) oracles_animation_check_frame(r->animation, frame);
    if (r->object_sprites && r->enhanced && oracles_enhanced_check_reloaded_at(r->enhanced, frame)) oracles_object_sprites_check_reset(r->object_sprites);
    if (r->guest && !r->check) {
        size_t count = 0;
        oracles_guest_journal(r->guest, &count);
        r->journal_writes += count;
        oracles_guest_journal_clear(r->guest);
    }
    if (transient) oracles_guest_detach(bus);
}


/* A session against the replay of its route: the game's state, frame for frame, over the frames the route
 * lasts.  The session goes on after the route's last line, and its screen carries the player's colour correction,
 * which is not the game's state: the screen is compared too, and said apart. */
static int compare_session(const char *session_path, const char *replay_path)
{
    FILE *a = fopen(session_path, "rb"), *b = fopen(replay_path, "rb");
    if (!a || !b) { fprintf(stderr, "harness: cannot open the fingerprint files\n"); if (a) fclose(a); if (b) fclose(b); return 2; }
    char la[256], lb[256];
    unsigned frames = 0, screens = 0;
    int result = 0;
    while (fgets(la, sizeof la, a) && fgets(lb, sizeof lb, b)) {
        /* frame, screen, then the state: live WRAM, HRAM, OAM, VRAM */
        const char *sa = strchr(la, '\t'), *sb = strchr(lb, '\t');
        const char *ta = sa ? strchr(sa + 1, '\t') : NULL, *tb = sb ? strchr(sb + 1, '\t') : NULL;
        if (!ta || !tb || (size_t)(sa - la) != (size_t)(sb - lb) || strncmp(la, lb, (size_t)(sa - la)) != 0 || strcmp(ta, tb) != 0) {
            printf("harness: the session and its replay part at frame line %u\nsession: %sreplay:  %s", frames, la, lb);
            result = 1;
            break;
        }
        if ((size_t)(ta - sa) != (size_t)(tb - sb) || strncmp(sa, sb, (size_t)(ta - sa)) != 0) screens++;
        frames++;
    }
    /* The session outlasts its route, never the reverse: a session file cut short proves nothing past its end. */
    if (result == 0 && (frames == 0 || (feof(a) && fgets(lb, sizeof lb, b)))) { printf("harness: the session's fingerprints end at frame line %u, before the replay of its route\n", frames); result = 1; }
    if (result == 0) printf("harness: the session and its replay are the same game over %u frames (live WRAM, HRAM, OAM, VRAM); the screen differs on %u of them%s\n",
                            frames, screens, screens ? " (the player's colour correction: replay with --colour-correction on to compare it)" : "");
    fclose(a);
    fclose(b);
    return result;
}

static int compare(const char *a_path, const char *b_path)
{
    FILE *a = fopen(a_path, "rb"), *b = fopen(b_path, "rb");
    if (!a || !b) { fprintf(stderr, "harness: cannot open the fingerprint files\n"); if (a) fclose(a); if (b) fclose(b); return 2; }
    char la[256], lb[256];
    unsigned line = 0;
    int result = 0;
    for (;;) {
        const char *ra = fgets(la, sizeof la, a), *rb = fgets(lb, sizeof lb, b);
        if (!ra && !rb) break;
        if (!ra || !rb) { printf("harness: the runs have different lengths at line %u\n", line); result = 1; break; }
        if (strcmp(la, lb) != 0) {
            printf("harness: first divergence at frame line %u\nA: %sB: %s", line, la, lb);
            result = 1;
            break;
        }
        line++;
    }
    if (result == 0) printf("harness: %u identical frames\n", line);
    fclose(a);
    fclose(b);
    return result;
}

/* "normal,lcd-0-1,..." into a bit set of classes that must not be empty. */
static unsigned parse_expected_classes(const char *list)
{
    unsigned mask = 0;
    while (list && *list) {
        const char *comma = strchr(list, ',');
        const size_t length = comma ? (size_t)(comma - list) : strlen(list);
        for (unsigned i = 0; i < ORACLES_FRAME_CLASSES; i++) {
            const char *name = oracles_frame_class_name((OraclesFrameClass)i);
            if (strlen(name) == length && strncmp(name, list, length) == 0) mask |= 1u << i;
        }
        list = comma ? comma + 1 : list + length;
    }
    return mask;
}

/* The route replayed through the host, and the line that says what it cost. */
static int replay_route(run *r, const harness_options *o, oracles_host_backend *backend, oracles_host_run_report *report)
{
    oracles_host_run_config config;
    memset(&config, 0, sizeof config);
    config.core = r->core;
    config.quit_after_frames = o->frames;
    config.input_source = input_source;
    config.input_source_opaque = r;
    config.on_frame_begin = on_frame_begin;
    config.on_frame_end = on_frame_end;
    config.frame_opaque = r;
    if (r->mod) oracles_mod_session_configure(r->mod, &config);
    char error[256];
    const clock_t started = clock();
    const int result = oracles_host_run(&config, backend, report, error, sizeof error);
    const double seconds = (double)(clock() - started) / CLOCKS_PER_SEC;
    if (result != ORACLES_HOST_OK) fprintf(stderr, "harness: %s\n", error);
    fprintf(stderr, "harness: %u frames, hooks %s, %.3f s, %.3f ms per frame, %u events, %zu journaled register writes\n",
            report->frames_presented, o->hooks ? "on" : "off", seconds, seconds * 1000.0 / (report->frames_presented ? report->frames_presented : 1),
            r->events, r->journal_writes);
    if (r->out) fclose(r->out);
    if (r->positions) fclose(r->positions);
    if (r->keys_read) fclose(r->keys_read);
    return result;
}

/* The checks the options ask for, armed on the guest: the neighbours' objects,
 * the renderer, the ghost, the Enhanced view and its two checks.  Returns 0,
 * or 1 when one of them could not start. */
static int start_checks(run *r, const harness_options *o, uint8_t *rom, size_t rom_size)
{
    r->guest = oracles_guest_attach(r->core, r->profile);
    if (!r->guest) { fprintf(stderr, "harness: cannot attach the guest\n"); return 1; }
    oracles_guest_set_event_sink(r->guest, on_event, r);
    r->coverage = oracles_coverage_create(r->guest);   /* what the route covers, written to the summary */
    if (o->objects_dir) {
        r->neighbours = oracles_neighbour_check_start(rom, rom_size, r->profile, r->core, r->guest, o->objects_dir, o->objects_lead, o->objects_capture);
        if (!r->neighbours) { fprintf(stderr, "harness: the neighbour objects check could not start\n"); return 1; }
    }
    if (o->samples_dir) r->check = oracles_frame_check_start(r->guest, r->core, o->samples_dir, 10);
    if (o->ghost_dir) {
        r->ghost = oracles_ghost_check_start(rom, rom_size, r->profile, r->core, r->guest, o->ghost_dir, o->ghost_lead, o->ghost_threaded, o->ghost_trace);
        if (r->ghost && o->ghost_trace_load) oracles_ghost_check_trace_load(r->ghost);
        if (!r->ghost) { fprintf(stderr, "harness: the ghost instance could not start\n"); return 1; }
    }
    if (o->enhanced_dir) {
        r->enhanced = oracles_enhanced_check_start(r->core, r->guest, rom, rom_size, o->enhanced_dir, o->enhanced_threaded ? 0u : o->enhanced_budget);
        if (!r->enhanced) { fprintf(stderr, "harness: the Enhanced view could not start\n"); return 1; }
        if (o->enhanced_reload_at) oracles_enhanced_check_reload_at(r->enhanced, o->enhanced_reload_at);
        for (unsigned i = 0; i < o->surfaces; i++) oracles_enhanced_check_surface_at(r->enhanced, o->surface_at[i], o->surface_path[i]);
        /* Slots given without --hotkeys-live: the hotbar of a replay, its icons checked against the core's image. */
        int slots = 0;
        for (unsigned n = 0; n < 4u; n++) slots |= o->hotkeys_live.slots[n] != NULL;
        if (slots && !o->hotkeys_live.mode && !oracles_hotkeys_slots_hotbar(&o->hotkeys_live, oracles_enhanced_check_view(r->enhanced))) { fprintf(stderr, "harness: a --hotkey-slot is not <b|a>:<item>:<variant or -->\n"); return 1; }
        oracles_enhanced_check_set_zoom_out(r->enhanced, o->enhanced_zoom_out);
        oracles_enhanced_check_set_camera_profile(r->enhanced, o->enhanced_camera);
        oracles_enhanced_check_set_paced(r->enhanced, o->enhanced_paced);
        if (o->enhanced_objects) oracles_enhanced_check_set_neighbour_objects(r->enhanced, 1);
        r->animation = oracles_animation_check_start(r->guest, rom, rom_size);
        r->object_sprites = oracles_object_sprites_check_start(r->guest, rom, rom_size);
    }
    return 0;
}

/* The renderer's verdict against the core, written out and to the summary. */
static int report_renderer(run *r, const harness_options *o, FILE *summary)
{
    int verdict_failed = 0;
    if (r->check) {
        oracles_frame_check_report(r->check, stderr);
        char reason[256];
        const int pass = oracles_frame_check_verdict(r->check, parse_expected_classes(o->render_expect), reason, sizeof reason);
        if (!pass) {
            fprintf(stderr, "renderer: FAIL: %s\n", reason);
            verdict_failed = 1;
        } else fprintf(stderr, "renderer: PASS (no mismatch in a compared class, expected classes present, ceilings held)\n");
        if (summary) {
            const OraclesFrameStats *stats = oracles_frame_check_stats(r->check);
            fprintf(summary, "render.verdict=%s\n", pass ? "pass" : "fail");
            for (unsigned k = 0; k < ORACLES_FRAME_CLASSES; k++)
                fprintf(summary, "render.%s.frames=%u\nrender.%s.mismatches=%u\n", oracles_frame_class_name((OraclesFrameClass)k), stats->frames[k], oracles_frame_class_name((OraclesFrameClass)k), stats->mismatches[k]);
            fprintf(summary, "render.late_scroll_lines=%u\n", stats->late_scroll_lines);
            fprintf(summary, "render.first_tile_lines=%u\n", stats->first_tile_lines);
        }
        oracles_frame_check_stop(r->check);
    }
    return verdict_failed;
}

/* What each check found, written to stderr, to its own file and to the summary. */
/* The continuous transitions: what the policy did, and the writes it made
 * at the vblank of a frame the game did not finish, by transition state. */
static void report_transitions(run *r, FILE *summary, uint8_t *rom)
{
    if (r->transition) {
        OraclesRoomTransitionStats stats;
        oracles_room_transition_stats(r->transition, &stats);
        fprintf(stderr, "continuous transitions: %u taken over, Link moved on %u frames, held back by a wall ahead on %u, left alone on %u (game/room %u, state %u, Link's state %u, not walking %u, airborne %u, swimming %u, under water %u, holding %u, item or hurt %u, animation data %u), no animation %u\n",
                stats.transitions, stats.walked_frames, stats.capped_frames, stats.refused_frames, stats.refused_by[0], stats.refused_by[1], stats.refused_by[2],
                stats.refused_by[3], stats.refused_by[4], stats.refused_by[8], stats.refused_by[9], stats.refused_by[5], stats.refused_by[6], stats.refused_by[7], stats.unanimated_frames);
        fprintf(stderr, "continuous transitions: palettes refreshed after a palette transition on %u\n", stats.palette_refreshes);
        fprintf(stderr, "continuous transitions: swimming (--continuous-swim): %u taken over, Link moved on %u frames; %u started by his momentum at an edge, the direction not held\n", stats.swim_transitions, stats.swum_frames, stats.forced_transitions);
        unsigned by_state[8];
        oracles_guest_vblank_write_counts(r->guest, by_state);
        fprintf(stderr, "continuous transitions: writes at the vblank of a frame the game did not finish: %u during the load (states 3 and 4), %u during the scroll (state 5), %u elsewhere\n",
                by_state[3] + by_state[4], by_state[5], by_state[0] + by_state[1] + by_state[2] + by_state[6] + by_state[7]);
        if (summary) fprintf(summary, "transitions.vblank_policy_skipped=%u\n", oracles_guest_vblank_policy_skipped(r->guest));
        if (summary) fprintf(summary, "transitions.swim_taken=%u\ntransitions.swum_frames=%u\ntransitions.swim_forced=%u\n", stats.swim_transitions, stats.swum_frames, stats.forced_transitions);
        if (summary) fprintf(summary, "transitions.taken=%u\ntransitions.walked_frames=%u\ntransitions.refused_frames=%u\ntransitions.unanimated_frames=%u\ntransitions.palette_refreshes=%u\ntransitions.vblank_load_writes=%u\ntransitions.vblank_scroll_writes=%u\ntransitions.vblank_other_writes=%u\n",
                             stats.transitions, stats.walked_frames, stats.refused_frames, stats.unanimated_frames, stats.palette_refreshes,
                             by_state[3] + by_state[4], by_state[5], by_state[0] + by_state[1] + by_state[2] + by_state[6] + by_state[7]);
        oracles_room_transition_stop(r->transition);
        free(rom);
    }
}

static void report_checks(run *r, const harness_options *o, FILE *summary, uint8_t *rom)
{
    report_transitions(r, summary, rom);
    if (r->guest) {
        unsigned overflow = 0, purged = 0;
        oracles_guest_dropped_returns(r->guest, &overflow, &purged);
        if (overflow || oracles_guest_journal_dropped(r->guest))
            fprintf(stderr, "harness: hooks lost %u return captures to overflow; the journal dropped %zu writes\n", overflow, oracles_guest_journal_dropped(r->guest));
    }
    if (r->enhanced) {
        oracles_enhanced_check_report(r->enhanced, stderr);
        char path[4096];
        snprintf(path, sizeof path, "%s/report.txt", o->enhanced_dir);
        FILE *f = fopen(path, "w");
        if (f) { oracles_enhanced_check_report(r->enhanced, f); fclose(f); }
        if (summary) oracles_enhanced_check_summary(r->enhanced, summary);
        oracles_enhanced_check_stop(r->enhanced);
    }
    if (r->ghost) {
        oracles_ghost_check_report(r->ghost, stderr);
        oracles_ghost_check_write_reads(r->ghost, o->ghost_dir);
        char path[4096];
        snprintf(path, sizeof path, "%s/report.txt", o->ghost_dir);
        FILE *f = fopen(path, "w");
        if (f) { oracles_ghost_check_report(r->ghost, f); fclose(f); }
        if (summary) oracles_ghost_check_summary(r->ghost, summary);
        oracles_ghost_check_stop(r->ghost);
    }
    if (r->neighbours) {
        oracles_neighbour_check_report(r->neighbours, stdout);
        if (summary) oracles_neighbour_check_summary(r->neighbours, summary);
        oracles_neighbour_check_stop(r->neighbours);
    }
    if (r->animation) {
        oracles_animation_check_report(r->animation, stderr);
        if (summary) oracles_animation_check_summary(r->animation, summary);
        oracles_animation_check_stop(r->animation);
        oracles_object_sprites_check_report(r->object_sprites, stderr);
        if (summary) oracles_object_sprites_check_summary(r->object_sprites, summary);
        oracles_object_sprites_check_stop(r->object_sprites);
    }
    if (r->coverage) {
        if (summary) oracles_coverage_summary(r->coverage, summary);
        oracles_coverage_destroy(r->coverage);
    }
}

/* The replay of a route's exchanges: 1 when it failed. */
static int report_hotkeys(run *r, FILE *summary)
{
    if (r->hotkeys_live) {
        if (summary) oracles_hotkeys_live_summary(r->hotkeys_live, summary);
        return oracles_hotkeys_live_stop(r->hotkeys_live, stderr);
    }
    if (!r->hotkeys) return 0;
    oracles_hotkeys_check_report(r->hotkeys, stderr);
    const int failed = oracles_hotkeys_check_failed(r->hotkeys);
    if (failed) fprintf(stderr, "item hotkeys: FAIL\n");
    if (summary) oracles_hotkeys_check_summary(r->hotkeys, summary);
    oracles_hotkeys_check_stop(r->hotkeys);
    return failed;
}

/* The route's starting SRAM, if any, sits next to it.  Returns 0, or 1 when
 * it is not the one the route was recorded with. */
static int load_starting_sram(run *r, const char *route_path)
{
    char sram_path[4096];
    snprintf(sram_path, sizeof sram_path, "%s.sram", route_path);
    FILE *f = fopen(sram_path, "rb");
    char sha1[41] = "none";
    if (f) {
        const size_t size = oracles_core_sram_size(r->core);
        uint8_t *sram = malloc(size);
        if (sram && fread(sram, 1, size, f) == size) { oracles_core_load_sram(r->core, sram, size); oracles_sha1_hex(sram, size, sha1); }
        free(sram);
        fclose(f);
    }
    if (strcmp(r->route.header.sram_sha1, sha1) != 0) { fprintf(stderr, "harness: the route's starting SRAM does not match %s\n", sram_path); return 1; }
    return 0;
}

/* The ROM, the core and the route: what the replay needs before its checks.
 * Returns 0, or 1 with the reason on stderr. */
static int open_the_run(run *r, harness_options *o, uint8_t **rom_out, size_t *rom_size_out)
{
    char error[256];
    size_t rom_size = 0;
    uint8_t *rom = o->patch_path ? oracles_bps_apply_files(o->rom_path, o->patch_path, &rom_size, error, sizeof error)
                                 : oracles_rom_read_file(o->rom_path, &rom_size, error, sizeof error);
    if (!rom) { fprintf(stderr, "harness: %s\n", error); return 1; }
    OraclesRomInfo info;
    if (oracles_rom_identify(rom, rom_size, &info, error, sizeof error) != 0) { fprintf(stderr, "harness: %s\n", error); free(rom); return 1; }
    memset(r, 0, sizeof *r);
    /* A mod's houses are in the image before the core exists. */
    if (o->mods_count) {
        r->mod = oracles_mod_session_start(o->mods_dirs, o->mods_count, info.game, rom, rom_size, error, sizeof error);
        if (!r->mod || oracles_mod_session_compose(r->mod, oracles_rom_is_original(&info), &rom, &rom_size, error, sizeof error) != 0) { fprintf(stderr, "harness: %s\n", error); free(rom); return 1; }
        if (o->route_path) {   /* the mods' storage the route starts from (ROUTE.store, as ROUTE.sram), read only */
            char store[4200];
            snprintf(store, sizeof store, "%s.store", o->route_path);
            if (oracles_mod_session_storage_open(r->mod, store, 0, error, sizeof error) != 0) { fprintf(stderr, "harness: %s: %s\n", store, error); free(rom); return 1; }
        }
    }
    const OraclesCoreOptions options = { o->sample_rate_hz, o->colour_correction, (OraclesCoreKind)o->core_kind };   /* no audio unless asked: --sample-rate proves the state does not depend on it */
    r->core = oracles_core_create(rom, rom_size, &options);
    r->profile = oracles_compat_find(&info);
    r->game = info.game;
    if (!r->core) { fprintf(stderr, "harness: the core could not start\n"); free(rom); return 1; }
    if (!r->profile && !o->hooks && o->out_path) {
        fprintf(stderr, "harness: --out fingerprints require an authenticated original ROM even with hooks off\n");
        oracles_core_destroy(r->core);
        free(rom);
        return 1;
    }
    if (oracles_route_read(o->route_path, &r->route, error, sizeof error) != 0) { fprintf(stderr, "harness: %s\n", error); return 1; }
    if (strcmp(r->route.header.rom_sha1, info.sha1) != 0) { fprintf(stderr, "harness: the route was recorded with another ROM\n"); return 1; }
    /* A route recorded before the core's joypad bouncing was cut (the header's core line) ran with it and only replays with it. */
    if (oracles_route_joypad_bouncing(&r->route.header) && oracles_core_set_joypad_bouncing(r->core, 1) != 0)
        fprintf(stderr, "harness: the route was recorded with SameBoy's joypad bouncing, which %s does not emulate: replayed without it\n",
                oracles_core_version(r->core));
    /* The route's gameplay options are part of the replay (docs/ROUTES.md). */
    const int route_continuous = oracles_route_has_option(&r->route.header, ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS);
    if ((o->continuous_transitions || route_continuous) && r->profile
        && !oracles_compat_continuous_transitions(r->profile)) {
        fprintf(stderr, "harness: the selected profile does not support --continuous-transitions; refusing the request and route option\n");
        return 1;
    }
    if (route_continuous && !o->continuous_transitions) {
        o->continuous_transitions = 1;
        fprintf(stderr, "harness: the route was recorded with --continuous-transitions: the option is on\n");
    }
    const int route_swim = oracles_route_has_option(&r->route.header, ORACLES_ROUTE_OPTION_CONTINUOUS_SWIM);
    if ((o->continuous_swim || route_swim) && r->profile && !oracles_compat_continuous_swim(r->profile)) {
        fprintf(stderr, "harness: %s does not support --continuous-swim; refusing the request and route option\n", oracles_compat_name(r->profile));
        return 1;
    }
    if (route_swim && !o->continuous_swim) {
        o->continuous_transitions = o->continuous_swim = 1;
        fprintf(stderr, "harness: the route was recorded with --continuous-swim: the option is on\n");
    }
    if (r->profile && o->enhanced_zoom_out && !oracles_compat_zoom_out(r->profile)) {
        fprintf(stderr, "harness: %s does not support --zoom-out yet\n", oracles_compat_name(r->profile));
        return 1;
    }
    if (r->profile && (o->hotkeys_live.mode || o->hotkeys_dir || r->route.action_count) && !oracles_compat_item_hotkeys(r->profile)) {
        fprintf(stderr, "harness: %s does not support the item hotkeys yet\n", oracles_compat_name(r->profile));
        return 1;
    }
    if (!r->profile && (o->hooks || o->continuous_transitions || o->samples_dir || o->ghost_dir || o->enhanced_dir || o->objects_dir)) {
        fprintf(stderr, "harness: guest-dependent checks and transitions require an authenticated original ROM\n");
        return 1;
    }
    if (load_starting_sram(r, o->route_path) != 0) return 1;
    *rom_out = rom;
    *rom_size_out = rom_size;
    return 0;
}

/* The checks and the continuous transitions armed, and the run's own state
 * set: what each option asks of the replay.  Returns 0, or 1 with the reason
 * on stderr.  The ROM is kept only for the transitions, which read it; without
 * them it is freed and the caller's pointer cleared. */
static int arm_the_run(run *r, harness_options *o, uint8_t **rom_ref, size_t rom_size)
{
    uint8_t *rom = *rom_ref;
    if (o->hooks && start_checks(r, o, rom, rom_size) != 0) return 1;
    if (o->continuous_transitions && r->guest) {
        r->transition = oracles_room_transition_start(r->guest, rom, rom_size, o->continuous_swim);
        if (!r->transition) { fprintf(stderr, "harness: --continuous-transitions needs the hooks on\n"); return 1; }
    }
    if (o->hotkeys_live.mode) {
        if (!r->guest || o->hotkeys_dir || r->route.action_count) { fprintf(stderr, "harness: --hotkeys-live needs the hooks on, and a route without exchanges of its own\n"); return 1; }
        r->hotkeys_live = oracles_hotkeys_live_start(r->guest, &o->hotkeys_live, &r->route.header);
        if (!r->hotkeys_live) return 1;
    }
    /* A route's exchanges (format 2, the item hotkeys) are part of its replay: applied whenever the route has some. */
    if (r->guest && (o->hotkeys_dir || r->route.action_count)) {
        r->hotkeys = oracles_hotkeys_check_start(r->guest, &r->route, o->hotkeys_dir);
        if (!r->hotkeys) { fprintf(stderr, "harness: the inventory's write point cannot be armed on this ROM\n"); return 1; }
    } else if (r->route.action_count) { fprintf(stderr, "harness: the route has inventory exchanges, which need the hooks on\n"); return 1; }
    /* The route's mod replays with it: its conversations hold the keys the route carries. */
    if (o->mods_count || r->route.header.mods[0]) {
        char error[256];
        if (!r->guest) { fprintf(stderr, "harness: a mod needs the hooks on\n"); return 1; }
        char message[4400];
        if (!oracles_mod_session_matches_route(r->mod, r->route.header.mods, error, sizeof error)
            || oracles_mod_session_attach(r->mod, r->guest, error, sizeof error) != 0) { fprintf(stderr, "harness: %s\n", error); return 1; }
        if (o->route_path && !oracles_mod_session_matches_store(o->route_path, r->route.header.store_sha1, message, sizeof message)) {
            fprintf(stderr, "harness: %s\n", message);
            return 1;
        }
        if (o->mod_trace_path && !oracles_mod_session_trace(r->mod, o->mod_trace_path)) { fprintf(stderr, "harness: cannot write %s\n", o->mod_trace_path); return 1; }
    }
    /* Only the transitions read the ROM afterwards; without them it goes, and
     * the caller's pointer with it, nothing being allowed to carry it once freed. */
    if (!r->transition) { free(rom); *rom_ref = NULL; }
    if (o->samples_dir && !r->check) { fprintf(stderr, "harness: --render-check needs the hooks on\n"); return 1; }
    if (o->ghost_dir && !r->ghost) { fprintf(stderr, "harness: --ghost-check needs the hooks on\n"); return 1; }
    if (o->enhanced_dir && !r->enhanced) { fprintf(stderr, "harness: --enhanced-check needs the hooks on\n"); return 1; }
    r->corrupt_at = o->corrupt_at;
    r->dumps = o->dumps; memcpy(r->dump_at, o->dump_at, sizeof r->dump_at); memcpy(r->dump_path, o->dump_path, sizeof r->dump_path);
    if (o->out_path) { r->out = fopen(o->out_path, "wb"); if (!r->out) { fprintf(stderr, "harness: cannot write %s\n", o->out_path); return 1; } }
    if (o->positions_path) { r->positions = fopen(o->positions_path, "wb"); if (!r->positions) { fprintf(stderr, "harness: cannot write %s\n", o->positions_path); return 1; } }
    if (o->keys_read_path) { r->keys_read = fopen(o->keys_read_path, "wb"); if (!r->keys_read) { fprintf(stderr, "harness: cannot write %s\n", o->keys_read_path); return 1; } }
    if (o->frames == 0) o->frames = r->route.count ? oracles_route_last_frame(&r->route) + 1 : 1;
    return 0;
}

int main(int argc, char **argv)
{
    harness_options o;
    if (harness_parse_options(argc, argv, &o) != 0) return harness_usage();
    if (o.compare_a) return o.compare_session ? compare_session(o.compare_a, o.compare_b) : compare(o.compare_a, o.compare_b);
    if (!o.rom_path || !o.route_path) return harness_usage();

    run r;
    uint8_t *rom = NULL;
    size_t rom_size = 0;
    if (open_the_run(&r, &o, &rom, &rom_size) != 0) return 1;
    if (arm_the_run(&r, &o, &rom, rom_size) != 0) return 1;

    oracles_host_backend backend;
    oracles_null_backend_init(&backend);
    oracles_host_run_report report;
    const int result = replay_route(&r, &o, &backend, &report);
    FILE *summary = NULL;
    if (o.summary_path) {
        summary = fopen(o.summary_path, "w");
        if (!summary) fprintf(stderr, "harness: cannot write %s\n", o.summary_path);
        else fprintf(summary, "frames=%u\nresult=%s\n", report.frames_presented, result == ORACLES_HOST_OK ? "ok" : "error");
    }
    const int renderer_failed = report_renderer(&r, &o, summary);
    const int verdict_failed = report_hotkeys(&r, summary) || renderer_failed;
    report_checks(&r, &o, summary, rom);
    oracles_mod_session_stop(r.mod, summary);
    if (summary) fclose(summary);
    if (r.guest) oracles_guest_detach(r.guest);
    oracles_null_backend_release(&backend);
    oracles_route_free(&r.route);
    oracles_core_destroy(r.core);
    return result == ORACLES_HOST_OK && !verdict_failed ? 0 : 1;
}
