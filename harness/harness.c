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
#include "harness_internal.h"

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
    if (r->colour_toggle_at && frame == r->colour_toggle_at) oracles_core_set_colour_correction(r->core, !oracles_core_colour_correction(r->core));
    size_t first;
    for (size_t n = oracles_route_cheats_at(&r->route, frame, &first), i = first; i < first + n; i++) {   /* the route's cheats changed at this frame */
        unsigned bit = 0, cheats = oracles_guest_cheats(r->guest);
        oracles_cheats_parse(r->route.cheats[i].name, NULL, &bit);
        oracles_guest_set_cheats(r->guest, r->route.cheats[i].on ? cheats | bit : cheats & ~bit);   /* armed from the start: it cannot fail */
    }
    /* The route's settings changed at this frame: the swim's hold, the instant text; the item hotkeys' mode and the window are notes, the
     * exchanges being the `equip` lines and the window presentation alone. */
    for (size_t n = oracles_route_settings_at(&r->route, frame, &first), i = first; i < first + n; i++) {
        if (r->route.settings[i].name == ORACLES_ROUTE_SET_SWIM_HOLD) oracles_guest_set_swim_hold(r->guest, r->route.settings[i].value);   /* armed from the start */
        if (r->route.settings[i].name == ORACLES_ROUTE_SET_TEXT_SPEED) oracles_guest_set_text_instant(r->guest, r->route.settings[i].value);   /* the same */
    }
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
    if (r->guest && oracles_guest_text_instant_armed(r->guest)) oracles_text_speed_check_frame_end(&r->text_speed, r->guest);
    if (r->transition) {
        /* Link swimming into a transition swims through it: the game freezes him, and reads the tile it takes as his own
         * (y+5) again at the first frame it runs after the one the transition ends on.  Out of the water there, where
         * the straight path, off which the diagonal carried him, stays in it: a carry that landed him. */
        const OraclesGuestTables *t = oracles_guest_tables(bus);
        const unsigned state = oracles_guest_read8(bus, t->screen_transition_state), mode = oracles_guest_read8(bus, t->link_anim_mode);
        const int swimming = oracles_guest_read8(bus, t->link_swimming_state) != 0u && (mode == ORACLES_LINK_ANIM_MODE_SWIM || mode == ORACLES_LINK_ANIM_MODE_DIVE);
        const uint8_t *link = oracles_guest_object(bus, 0, 0);
        const uint16_t x = link ? (uint16_t)(link[ORACLES_OBJ_X] | (link[ORACLES_OBJ_XH] << 8)) : 0, y = link ? (uint16_t)(link[ORACLES_OBJ_Y] | (link[ORACLES_OBJ_YH] << 8)) : 0;
        if (state >= 3u && state <= 5u) {
            if (!r->in_transition) r->swam_into = r->swam_before;
            r->in_transition = 1;
            r->landing = 0;
        } else if (state == 2u && r->in_transition) {
            unsigned direction = 0;
            uint16_t straight = 0;
            r->in_transition = 0;
            r->landing = r->swam_into && link && oracles_room_transition_straight_cross(r->transition, &direction, &straight);
            r->carried_off = (int16_t)(uint16_t)(((direction & 1u) ? y : x) - straight);
            r->landing_direction = direction;
        } else if (state == 2u && r->landing) {
            r->landing = 0;
            const int sx = (r->landing_direction & 1u) ? x : x - r->carried_off, sy = (r->landing_direction & 1u) ? y - r->carried_off : y;
            const int column = ((sx >> 8) & 0xff) >> 4, row = (((sy >> 8) & 0xff) + 5) >> 4;
            OraclesGuestTransitionState under;
            memset(&under, 0, sizeof under);
            under.game = oracles_compat_family(r->profile);
            under.room_layout = oracles_guest_ptr(bus, t->room_layout, 176);
            under.active_collisions = oracles_guest_read8(bus, t->active_collisions);
            under.tile_types_bank = t->tile_types_table.bank == ORACLES_GUEST_ABSENT ? 0xffu : (uint8_t)t->tile_types_table.bank;
            under.tile_types_table = t->tile_types_table.addr;
            under.link_var2f = link[ORACLES_OBJ_VAR2F];
            if (!swimming && r->carried_off && oracles_room_transition_water_tile(r->rom, r->rom_size, &under, (unsigned)(row * 16 + column))) r->swim_dropped++;
        }
        if (state == 2u) r->swam_before = swimming;
    }
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
    for (unsigned p = 0; p < r->poke_count; p++) if (frame == r->pokes[p].frame) {   /* after the frame's checks: the next frame runs with it */
        const OraclesGuestSym sym = { r->pokes[p].bank, r->pokes[p].addr };
        uint8_t *byte = (uint8_t *)oracles_guest_ptr(bus, sym, 1);
        if (byte) *byte = r->pokes[p].value;
        else fprintf(stderr, "harness: --poke-at %u %x:%04x: no such byte\n", frame, r->pokes[p].bank, r->pokes[p].addr);
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

/* --frame-times reads the real clock: the null backend's own counts a microsecond a reading, to stay deterministic.  The
 * monotonic one, not the wall's, which the system sets by jumps of seconds under WSL. */
static uint64_t real_monotonic_ns(void *opaque)
{
    (void)opaque;
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((double)counter.QuadPart * 1e9 / (double)frequency.QuadPart);
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000u + (uint64_t)t.tv_nsec;
#endif
}

/* --frame-times: each frame's phases, in microseconds (the Enhanced check composes at the frame's end). */
static void on_frame_timed(void *opaque, uint32_t frame, uint64_t core_ns, uint64_t source_ns, uint64_t present_ns, uint64_t end_ns)
{
    run *r = opaque;
    fprintf(r->frame_times, "%u\t%llu\t%llu\t%llu\t%llu\n", frame, (unsigned long long)(core_ns / 1000u), (unsigned long long)(source_ns / 1000u),
            (unsigned long long)(present_ns / 1000u), (unsigned long long)(end_ns / 1000u));
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
    if (r->frame_times) { config.measure_frames = 1; config.on_frame_timed = on_frame_timed; config.frame_timed_opaque = r; }
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
    if (r->frame_times) fclose(r->frame_times);
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
    char error[256];
    if (r->language && oracles_language_session_attach(r->language, r->guest, error, sizeof error) != 0) { fprintf(stderr, "harness: %s\n", error); return 1; }
    r->coverage = oracles_coverage_create(r->guest);   /* what the route covers, written to the summary */
    if (o->objects_dir || o->object_animation) {
        r->neighbours = oracles_neighbour_check_start(rom, rom_size, r->profile, r->core, r->guest, o->objects_dir, o->objects_lead, o->objects_capture);
        if (!r->neighbours) { fprintf(stderr, "harness: the neighbour objects check could not start\n"); return 1; }
    }
    if (o->object_animation) {
        r->object_animation = oracles_object_animation_check_start(r->guest, rom, rom_size, o->objects_dir);
        if (!r->object_animation) { fprintf(stderr, "harness: the objects' animation check could not start\n"); return 1; }
        oracles_neighbour_check_set_animation(r->neighbours, r->object_animation);
    }
    if (o->samples_dir) r->check = oracles_frame_check_start(r->guest, r->core, o->samples_dir, 10);
    if (o->ghost_dir) {
        r->ghost = oracles_ghost_check_start(rom, rom_size, r->profile, r->core, r->guest, o->ghost_dir, o->ghost_lead, o->ghost_threaded, o->ghost_trace);
        if (r->ghost && o->ghost_trace_load) oracles_ghost_check_trace_load(r->ghost);
        if (!r->ghost) { fprintf(stderr, "harness: the ghost instance could not start\n"); return 1; }
    }
    if (o->unvisited_hidden && !o->enhanced_dir) { fprintf(stderr, "harness: --unvisited-rooms hidden is the Enhanced view's: it needs --enhanced-check\n"); return 1; }
    if (o->enhanced_dir) {
        r->enhanced = oracles_enhanced_check_start(r->core, r->guest, rom, rom_size, o->enhanced_dir, o->enhanced_threaded ? 0u : o->enhanced_budget);
        if (!r->enhanced) { fprintf(stderr, "harness: the Enhanced view could not start\n"); return 1; }
        if (o->enhanced_reload_at) oracles_enhanced_check_reload_at(r->enhanced, o->enhanced_reload_at);
        for (unsigned i = 0; i < o->surfaces; i++) oracles_enhanced_check_surface_at(r->enhanced, o->surface_at[i], o->surface_path[i]);
        /* Slots given without --hotkeys-live: the hotbar of a replay, its icons checked against the core's image. */
        int slots = 0;
        for (unsigned n = 0; n < 4u; n++) slots |= o->hotkeys_live.slots[n] != NULL;
        if (slots && !o->hotkeys_live.mode && !oracles_hotkeys_slots_hotbar(&o->hotkeys_live, oracles_enhanced_check_view(r->enhanced))) { fprintf(stderr, "harness: a --hotkey-slot is not <b|a>:<item>:<variant or -->\n"); return 1; }
        oracles_enhanced_check_set_size(r->enhanced, harness_view_size(o));
        if (o->screen_w && o->enhanced_level == ORACLES_ENHANCED_FAR) {
            char fitting[160];
            if (o->aspect_given) snprintf(fitting, sizeof fitting, "the far view at its size in the shape --aspect names");
            else oracles_enhanced_far_fitting_words(o->screen_w, o->screen_h, fitting, sizeof fitting);
            fprintf(stderr, "harness: %s\n", fitting);
        }
        oracles_enhanced_check_set_camera_profile(r->enhanced, o->enhanced_camera);
        oracles_enhanced_check_set_link_phase(r->enhanced, o->enhanced_link_phase);
        oracles_enhanced_check_set_subpixel(r->enhanced, o->enhanced_subpixel);
        oracles_enhanced_check_subpixel_switch_at(r->enhanced, o->enhanced_subpixel_switch, o->enhanced_subpixel_switches);
        oracles_enhanced_view_set_hud(oracles_enhanced_check_view(r->enhanced), o->enhanced_hud);
        oracles_enhanced_view_set_unvisited_hidden(oracles_enhanced_check_view(r->enhanced), o->unvisited_hidden);
        if (o->enhanced_ghosts > 1u) oracles_enhanced_view_set_ghosts(oracles_enhanced_check_view(r->enhanced), o->enhanced_ghosts);
        oracles_enhanced_check_set_paced(r->enhanced, o->enhanced_paced);
        if (o->enhanced_objects) oracles_enhanced_check_set_neighbour_objects(r->enhanced, 1);
        oracles_enhanced_view_set_neighbour_animation(oracles_enhanced_check_view(r->enhanced), !o->enhanced_frozen);
        oracles_enhanced_view_set_unvisited_edge(oracles_enhanced_check_view(r->enhanced), !o->enhanced_frozen);   /* as the Low quality */
        r->animation = oracles_animation_check_start(r->guest, rom, rom_size);
        r->object_sprites = oracles_object_sprites_check_start(r->guest, rom, rom_size);
    }
    return 0;
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
    /* A language's pack comes from the US ROM as read; the language is composed after the mods' houses. */
    if (o->language && oracles_language_session_start(&r->language, o->language, o->eu_rom_path, o->language_pack, o->patch_path ? o->rom_path : NULL, &info, rom, rom_size, error, sizeof error) != 0) {
        fprintf(stderr, "harness: %s\n", error);
        free(rom);
        return 1;
    }
    if (oracles_language_session_note(r->language)[0]) fprintf(stderr, "harness: %s\n", oracles_language_session_note(r->language));
    if (r->language && !o->hooks) { fprintf(stderr, "harness: --language needs the hooks on\n"); free(rom); return 1; }
    if (oracles_route_read(o->route_path, &r->route, error, sizeof error) != 0) { fprintf(stderr, "harness: %s\n", error); free(rom); return 1; }
    /* The compact text lays the texts out again where the language is composed: decided before it. */
    if (oracles_route_has_option(&r->route.header, ORACLES_ROUTE_OPTION_TEXT_COMPACT) && !o->compact_text) {
        o->compact_text = 1;
        fprintf(stderr, "harness: the route was recorded with --text compact: the option is on\n");
    }
    if (oracles_route_has_option(&r->route.header, ORACLES_ROUTE_OPTION_TEXT_BOX_WIDE) && !o->text_box_wide) {
        o->text_box_wide = 1;
        fprintf(stderr, "harness: the route was recorded with --text-box wide: the option is on\n");
    }
    if (o->compact_text && (!oracles_compat_compact_text(oracles_compat_find(&info)) || !o->enhanced_dir)) {   /* never armed in Faithful: it changes what the core draws */
        fprintf(stderr, "harness: the compact text plays in Enhanced alone (--enhanced-check), on Ages, Seasons and the fan games made on them; refusing the request and route option\n");
        free(rom);
        return 1;
    }
    if (o->text_box_wide && (!oracles_compat_host_text_box(oracles_compat_find(&info)) || !o->enhanced_dir)) {   /* the host draws the box: Enhanced alone */
        fprintf(stderr, "harness: the wide text box plays in Enhanced alone (--enhanced-check), on Ages and Seasons; refusing the request and route option\n");
        free(rom);
        return 1;
    }
    /* The texts laid out again: at the game's 128 pixels for the compact text, at the wide box's 192, classic or compact. */
    const OraclesLanguageLayout layout = { o->text_box_wide ? ORACLES_TEXT_BOX_WIDE_PIXELS : 128u, !o->compact_text, 0u, NULL };
    if ((o->compact_text || o->text_box_wide) && oracles_language_session_lay_out(&r->language, &info, &layout, error, sizeof error) != 0) {
        fprintf(stderr, "harness: %s\n", error);
        free(rom);
        return 1;
    }
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
    if (r->language && oracles_language_session_compose(r->language, &rom, &rom_size, error, sizeof error) != 0) { fprintf(stderr, "harness: %s\n", error); free(rom); return 1; }
    /* The core: --core's, else the one the route was recorded on. */
    const OraclesCoreKind kind = o->core_given ? (OraclesCoreKind)o->core_kind : oracles_route_core_mgba(&r->route.header) ? ORACLES_CORE_MGBA : ORACLES_CORE_SAMEBOY;
    const OraclesCoreOptions options = { o->sample_rate_hz, o->colour_correction, kind, 0 };   /* no audio unless asked: --sample-rate proves the state does not depend on it */
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
    const int route_diagonal = oracles_route_has_option(&r->route.header, ORACLES_ROUTE_OPTION_CONTINUOUS_DIAGONAL);
    if ((o->continuous_diagonal || route_diagonal) && r->profile && !oracles_compat_continuous_diagonal(r->profile)) {
        fprintf(stderr, "harness: %s does not support --continuous-diagonal; refusing the request and route option\n", oracles_compat_name(r->profile));
        return 1;
    }
    if (route_diagonal && !o->continuous_diagonal) {
        o->continuous_transitions = o->continuous_diagonal = 1;
        fprintf(stderr, "harness: the route was recorded with --continuous-diagonal: the option is on\n");
    }
    const int route_steps = oracles_route_has_option(&r->route.header, ORACLES_ROUTE_OPTION_CONTINUOUS_STEPS);
    if ((o->continuous_steps || route_steps) && r->profile && !oracles_compat_continuous_steps(r->profile)) {
        fprintf(stderr, "harness: %s does not support --continuous-steps; refusing the request and route option\n", oracles_compat_name(r->profile));
        return 1;
    }
    if (route_steps && !o->continuous_steps) {
        o->continuous_transitions = o->continuous_steps = 1;
        fprintf(stderr, "harness: the route was recorded with --continuous-steps: the option is on\n");
    }
    const int route_swim_hold = oracles_route_has_option(&r->route.header, ORACLES_ROUTE_OPTION_SWIM_HOLD);
    if (route_swim_hold && !oracles_compat_swim_hold(r->profile)) {
        fprintf(stderr, "harness: the route was recorded with --swim-hold, which %s does not support\n", r->profile ? oracles_compat_name(r->profile) : "this image");
        return 1;
    }
    if (o->swim_hold && !route_swim_hold && !oracles_compat_swim_hold(r->profile)) {
        fprintf(stderr, "harness: --swim-hold does nothing in %s\n", r->profile ? oracles_compat_name(r->profile) : "this image");
        o->swim_hold = 0;
    }
    if (route_swim_hold && !o->swim_hold) {
        o->swim_hold = 1;
        fprintf(stderr, "harness: the route was recorded with --swim-hold: the option is on\n");
    }
    r->swim_changed = oracles_route_sets(&r->route, ORACLES_ROUTE_SET_SWIM_HOLD);
    if (r->swim_changed && !oracles_compat_swim_hold(r->profile)) {
        fprintf(stderr, "harness: the route changes the swim's hold, which %s does not support\n", r->profile ? oracles_compat_name(r->profile) : "this image");
        return 1;
    }
    const int route_text_instant = oracles_route_has_option(&r->route.header, ORACLES_ROUTE_OPTION_TEXT_INSTANT);
    r->text_speed_changed = oracles_route_sets(&r->route, ORACLES_ROUTE_SET_TEXT_SPEED);
    if ((o->text_instant || route_text_instant || r->text_speed_changed) && !oracles_compat_text_instant(r->profile)) {
        fprintf(stderr, "harness: %s does not support the instant text; refusing the request and route option\n", r->profile ? oracles_compat_name(r->profile) : "this image");
        return 1;
    }
    if (route_text_instant && !o->text_instant) {
        o->text_instant = 1;
        fprintf(stderr, "harness: the route was recorded with --text-speed instant: the option is on\n");
    }
    unsigned route_cheats = 0;
    char cheat_names[64];
    if (oracles_cheats_parse(r->route.header.options, ORACLES_ROUTE_OPTION_CHEAT, &route_cheats) != 0) {
        fprintf(stderr, "harness: the route was recorded with a cheat this harness does not know (%s)\n", r->route.header.options);
        return 1;
    }
    for (size_t i = 0; i < r->route.cheat_count; i++) {   /* the cheats its lines change, each one this harness knows */
        unsigned bit = 0;
        if (oracles_cheats_parse(r->route.cheats[i].name, NULL, &bit) != 0) {
            fprintf(stderr, "harness: the route changes a cheat this harness does not know (%s)\n", r->route.cheats[i].name);
            return 1;
        }
        r->cheats_changed |= bit;
    }
    if ((o->cheats | route_cheats | r->cheats_changed) && !oracles_compat_cheats(r->profile)) {
        fprintf(stderr, "harness: %s does not support the cheats; refusing the request and route option\n", r->profile ? oracles_compat_name(r->profile) : "this image");
        return 1;
    }
    if (route_cheats & ~o->cheats) {
        o->cheats |= route_cheats;
        fprintf(stderr, "harness: the route was recorded with --cheats %s: they are on\n", oracles_cheats_format(route_cheats, NULL, cheat_names, sizeof cheat_names));
    }
    char recorded[8];   /* a route played in a language replays in it; one without plays in any, as the hand-written ones do */
    if (oracles_route_language(&r->route.header, recorded, sizeof recorded)[0] && !oracles_route_language_matches(&r->route.header, o->language)) {
        fprintf(stderr, "harness: the route was played in a language (%s): replay it with --language %s and its European ROM\n", recorded, recorded);
        return 1;
    }
    /* A view that draws back (medium or far) asks what --zoom-out asked of the game's profile. */
    if (r->profile && harness_view_size(o).width > ORACLES_ENHANCED_NARROW_WIDTH
        && !oracles_compat_zoom_out(r->profile)) {
        fprintf(stderr, "harness: %s does not support a view drawn back (--view medium or far, --zoom-out) yet\n", oracles_compat_name(r->profile));
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
        r->rom = rom;
        r->rom_size = rom_size;
        if (r->enhanced) oracles_enhanced_check_set_transition(r->enhanced, r->transition);
        oracles_room_transition_set_diagonal(r->transition, o->continuous_diagonal);
        oracles_room_transition_set_steps(r->transition, o->continuous_steps);
    }
    if ((o->swim_hold || r->swim_changed)
        && (!r->guest || oracles_guest_set_swim_hold(r->guest, 1) != 0 || oracles_guest_set_swim_hold(r->guest, o->swim_hold) != 0)) {
        fprintf(stderr, "harness: --swim-hold and a route's set swim-hold lines need the hooks on, and the swim's strokes found in the ROM\n");
        return 1;
    }
    if ((o->text_instant || r->text_speed_changed)
        && (!r->guest || oracles_guest_set_text_instant(r->guest, 1) != 0 || oracles_guest_set_text_instant(r->guest, o->text_instant) != 0)) {
        fprintf(stderr, "harness: --text-speed instant and a route's set text-speed lines need the hooks on, and the text's points found in the ROM\n");
        return 1;
    }
    /* The points of the cheats the route changes later are armed now, with those on from the start: a point once armed
     * answers only while its cheat is on. */
    if ((o->cheats || r->cheats_changed)
        && (!r->guest || oracles_guest_set_cheats(r->guest, o->cheats | r->cheats_changed) != 0 || oracles_guest_set_cheats(r->guest, o->cheats) != 0)) {
        fprintf(stderr, "harness: --cheats and a route's cheat lines need the hooks on, and the cheats' points found in the ROM\n");
        return 1;
    }
    if (o->text_box_wide) {   /* after the language: the font's widths are read in the image it composed */
        if (!r->guest || !r->enhanced || oracles_guest_arm_wide_text_box(r->guest, ORACLES_TEXT_BOX_WIDE_PIXELS, !o->compact_text) != 0) {
            fprintf(stderr, "harness: --text-box wide needs --enhanced-check, the hooks on, and the text box's points found in the ROM\n");
            return 1;
        }
        unsigned pages = 0;   /* the pages laid out at its width; the others, the game's, centred */
        const uint32_t *laid = oracles_language_session_laid_pages(r->language, &pages);
        if (oracles_guest_set_text_box_pages(r->guest, laid, pages) != 0) { fprintf(stderr, "harness: out of memory\n"); return 1; }
        oracles_enhanced_check_set_wide_text_box(r->enhanced, ORACLES_TEXT_BOX_WIDE_PIXELS, 2u);
    } else if (o->compact_text && (!r->guest || oracles_guest_arm_compact_text(r->guest) != 0)) {
        fprintf(stderr, "harness: --text compact needs the hooks on, and its points found in the ROM\n");
        return 1;
    }
    if (o->text_size) {
        if (!r->enhanced) { fprintf(stderr, "harness: --text-size large or xlarge needs --enhanced-check\n"); return 1; }
        oracles_enhanced_check_set_text_size(r->enhanced, (unsigned)o->text_size);
    }
    if (o->surface_scale > 1u && r->enhanced) oracles_enhanced_check_set_surface_scale(r->enhanced, o->surface_scale);
    if (o->menus_large) {
        if (!r->enhanced) { fprintf(stderr, "harness: --menus-large needs --enhanced-check\n"); return 1; }
        oracles_enhanced_check_set_large_menus(r->enhanced, 1);
    }
    if (o->text_box_check) {
        if (!r->enhanced || (!o->text_box_wide && oracles_guest_arm_text_box(r->guest, ORACLES_TEXT_BOX_GAME_PIXELS) != 0)) {
            fprintf(stderr, "harness: --text-box-check needs --enhanced-check, Ages or Seasons, and the text box's routines\n");
            return 1;
        }
        oracles_enhanced_check_set_text_box(r->enhanced, 1);
    }
    if (o->hotkeys_live.mode) {
        if (!r->guest || o->hotkeys_dir || r->route.action_count) { fprintf(stderr, "harness: --hotkeys-live needs the hooks on, and a route without exchanges of its own\n"); return 1; }
        /* The route it records says the core that ran, as the launcher's: mGBA's, or the replayed route's joypad bouncing. */
        OraclesRouteHeader header = r->route.header;
        if (oracles_core_kind(r->core) == ORACLES_CORE_MGBA)
            snprintf(header.core, sizeof header.core, "%s,%s", ORACLES_ROUTE_CORE_JOYPAD_BOUNCING_OFF, ORACLES_ROUTE_CORE_MGBA);
        else
            snprintf(header.core, sizeof header.core, "%s", oracles_route_joypad_bouncing(&r->route.header) ? "" : ORACLES_ROUTE_CORE_JOYPAD_BOUNCING_OFF);
        r->hotkeys_live = oracles_hotkeys_live_start(r->guest, &o->hotkeys_live, &header);
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
    r->colour_toggle_at = o->colour_toggle_at;
    r->dumps = o->dumps; memcpy(r->dump_at, o->dump_at, sizeof r->dump_at); memcpy(r->dump_path, o->dump_path, sizeof r->dump_path);
    r->poke_count = o->poke_count; memcpy(r->pokes, o->pokes, sizeof r->pokes);
    if (o->out_path) { r->out = fopen(o->out_path, "wb"); if (!r->out) { fprintf(stderr, "harness: cannot write %s\n", o->out_path); return 1; } }
    if (o->positions_path) { r->positions = fopen(o->positions_path, "wb"); if (!r->positions) { fprintf(stderr, "harness: cannot write %s\n", o->positions_path); return 1; } }
    if (o->frame_times_path) { r->frame_times = fopen(o->frame_times_path, "wb"); if (!r->frame_times) { fprintf(stderr, "harness: cannot write %s\n", o->frame_times_path); return 1; } fputs("frame\tcore_us\tsource_us\tpresent_us\tend_us\n", r->frame_times); }
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
    if (o.frame_times_path) backend.monotonic_ns = real_monotonic_ns;
    oracles_host_run_report report;
    const int result = replay_route(&r, &o, &backend, &report);
    /* The cartridge RAM at the end: its SHA-1 in the summary (a save the game made, the same on both cores), the bytes
     * with --sram-out. */
    int sram_failed = 0;
    char sram_sha1[41] = "none";
    {
        const size_t size = oracles_core_sram_size(r.core);
        uint8_t *sram = size ? malloc(size) : NULL;
        if (sram && oracles_core_save_sram(r.core, sram, size) == 0) {
            oracles_sha1_hex(sram, size, sram_sha1);
            if (o.sram_out_path) {
                FILE *f = fopen(o.sram_out_path, "wb");
                if (!f || fwrite(sram, 1, size, f) != size) sram_failed = 1;
                if (f && fclose(f) != 0) sram_failed = 1;
            }
        } else if (o.sram_out_path) sram_failed = 1;
        if (sram_failed) fprintf(stderr, "harness: cannot write the cartridge RAM to %s\n", o.sram_out_path);
        free(sram);
    }
    FILE *summary = NULL;
    if (o.summary_path) {
        summary = fopen(o.summary_path, "w");
        if (!summary) fprintf(stderr, "harness: cannot write %s\n", o.summary_path);
        else fprintf(summary, "frames=%u\nresult=%s\nsram.sha1=%s\n", report.frames_presented, result == ORACLES_HOST_OK ? "ok" : "error", sram_sha1);
    }
    const int renderer_failed = harness_report_renderer(&r, &o, summary);
    const int verdict_failed = harness_report_hotkeys(&r, summary) || renderer_failed;
    harness_report_checks(&r, &o, summary, rom);
    harness_report_language(&r, summary);
    oracles_mod_session_stop(r.mod, summary);
    if (summary) fclose(summary);
    if (r.guest) oracles_guest_detach(r.guest);
    oracles_null_backend_release(&backend);
    oracles_route_free(&r.route);
    oracles_core_destroy(r.core);
    return result == ORACLES_HOST_OK && !verdict_failed && !sram_failed ? 0 : 1;
}
