/* A game session (session.h), moved out of the launcher's main so that the
 * home screen can start one in its window.
 *
 * --zoom-out (implies --enhanced) draws the view back: a surface of
 * 480x270, four times 1920x1080, that shows three rooms across and two down
 * outdoors.  A savestate taken with it loads only with it, and the other way
 * round: the camera's state is a place of the surface.
 *
 * --continuous-transitions (both games, implies --enhanced) keeps Link walking
 * through the game's scrolling transitions by writing, at one point of each
 * frame, the scroll step and Link's position and gait; the game's
 * state then differs from a native session's.  A route records the option in
 * its header and a replay runs with it; the savestate records it among the
 * mods, and a state saved with it is refused without it.  --continuous-swim
 * (implies it) takes Link swimming through them too; a route records it
 * beside the transitions, a savestate does not (attach).
 *
 * Vsync: "auto" (the default) presents in step with the display when it
 * refreshes at 59 to 61 Hz, close enough to the Game Boy's 59.7275 Hz for
 * the audio rate control to absorb the difference; the host then does not
 * pace, so a scroll has no periodic hiccup from the two clocks beating.  On
 * any other display the host paces, as with "off".  Under vsync the host
 * checks that the presentation waits for the display, and paces itself from
 * the first second it did not.  The launcher owns every file: the
 * core and the host only see buffers. */
#include "session_internal.h"

#include "guest_fingerprint.h"
#include "hotkeys_session.h"
#include "view.h"
#include "guest.h"
#include "bps.h"
#include "rom.h"
#include "route.h"
#include "sha1.h"
#include "state_refusal.h"
#include "ui_version.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define SAVE_INTERVAL_FRAMES 300u /* about five seconds */
#define SAMPLE_RATE_HZ 48000u

/* ---- files ------------------------------------------------------------------ */

static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* ---- SRAM --------------------------------------------------------------------- */

static int store_save(void *opaque, const uint8_t *buffer, size_t size)
{
    return oracles_session_write_file(((const save_file *)opaque)->path, buffer, size);
}

/* Loads an SRAM image into the core. Returns 1 when there is no file or it loaded, 0 when it is unusable. */
static int load_sram(OraclesCore *core, const char *path, char sha1[41])
{
    snprintf(sha1, 41, "none");
    uint8_t *data = NULL;
    size_t size = 0;
    if (!file_exists(path)) return 1;
    if (!oracles_session_read_file(path, &data, &size)) return 0;
    const int ok = size == oracles_core_sram_size(core) && oracles_core_load_sram(core, data, size) == 0;
    if (ok) oracles_sha1_hex(data, size, sha1);
    free(data);
    return ok;
}

/* ---- session ----------------------------------------------------------------- */

static void frame_begin(void *opaque, uint32_t frame)
{
    session *s = opaque;
    oracles_guest_set_frame(s->guest, frame);
    if (s->diag) oracles_diagnostics_frame_begin(s->diag, frame);
    oracles_hotkeys_session_frame(s->hotkeys, (struct OraclesEnhancedView *)s->view);
    if (s->check) oracles_frame_check_frame_begin(s->check, frame);
}

static void frame_end(void *opaque, uint32_t frame)
{
    session *s = opaque;
    if (s->fingerprints && s->recording) oracles_guest_write_fingerprint(s->guest, s->core, frame, s->fingerprints);
    if (s->recording) oracles_mod_session_frame_end(s->mod, frame);
    if (!s->playing) oracles_mod_session_storage_sync(s->mod);   /* a game saved this frame: its mods' storage beside it */
    if (s->diag) oracles_diagnostics_frame_end(s->diag, frame);
    /* The register journal is read by the view during the frame; the
     * diagnostics panel and the native renderer clear it themselves, else it
     * is cleared here so that it never fills. */
    else if (s->guest && !s->check) oracles_guest_journal_clear(s->guest);
}

static const uint32_t *native_frame(void *opaque)
{
    return oracles_frame_check_native(opaque);
}

void oracles_session_stop_recording(session *s, const char *why)
{
    if (!s->recording) return;
    s->recording = 0;
    oracles_hotkeys_session_record(s->hotkeys, NULL);
    if (s->fingerprints) { fclose(s->fingerprints); s->fingerprints = NULL; }
    if (oracles_route_writer_close(&s->record) != 0) fprintf(stderr, "oracles: cannot finish writing %s\n", s->record_path);
    else { fprintf(stderr, "oracles: route recording %s, written to %s\n", why, s->record_path); s->route_written = 1; }
}

static void on_command(void *opaque, int command)
{
    session *s = opaque;
    char message[256];
    switch (command) {
        case ORACLES_COMMAND_TOGGLE_COLOUR_CORRECTION:
            /* A choice made in game is remembered: what the player now sees. */
            s->colour_applied = s->colour_setting = s->settings->colour_correction = !s->colour_applied;
            oracles_core_set_colour_correction(s->core, s->colour_applied);
            oracles_settings_store(s->settings);
            fprintf(stderr, "oracles: colour correction %s\n", s->colour_applied ? "on" : "off");
            break;
        case ORACLES_COMMAND_TOGGLE_ENHANCED:
            if (s->view) {
                oracles_enhanced_view_toggle(s->view);
                fprintf(stderr, "oracles: Enhanced %s\n", oracles_enhanced_view_framed_only(s->view) ? "shows the framed core" : "shows the wide world");
            }
            break;
        case ORACLES_COMMAND_SAVE_STATE: oracles_session_save_state(s, message, sizeof message); break;
        case ORACLES_COMMAND_LOAD_STATE: oracles_session_load_state(s, message, sizeof message); break;
        default: oracles_hotkeys_session_command(s->hotkeys, command); break;
    }
}

/* ---- the pause menu (pause.h) ------------------------------------------------------ */

static int pause_save(void *opaque, char *message, size_t capacity) { return oracles_session_save_state(opaque, message, capacity); }
static int pause_load(void *opaque, char *message, size_t capacity) { return oracles_session_load_state(opaque, message, capacity); }

static void pause_state_time(void *opaque, char *out, size_t capacity)
{
    const session *s = opaque;
    struct stat file;
    const struct tm *local = stat(s->state_path, &file) == 0 ? localtime(&file.st_mtime) : NULL;
    if (!local || !strftime(out, capacity, "%H:%M", local)) { if (capacity) out[0] = 0; }
}

/* Controls or Display changed the settings: the keys and the colour correction apply at once, the rest at the next Play.
 * The colour correction applies when Display changed it: --colour-correction holds until then. */
static void pause_settings_changed(void *opaque)
{
    session *s = opaque;
    if (s->settings->colour_correction != s->colour_setting) {
        s->colour_applied = s->colour_setting = s->settings->colour_correction;
        oracles_core_set_colour_correction(s->core, s->colour_applied);
        fprintf(stderr, "oracles: colour correction %s\n", s->colour_applied ? "on" : "off");
    }
    oracles_sdl_backend_rebind(&s->backend, &s->sdl);   /* s->sdl's names are the settings' own */
}

static void session_started(void *opaque)
{
    session *s = opaque;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *frame;
    int width, height;
    oracles_sdl_backend_pause_view(&s->backend, &window, &renderer, &frame, &width, &height);
    oracles_pause_prepare(s->pause, renderer);
}

static oracles_host_pause_result session_pause(void *opaque)
{
    session *s = opaque;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *frame;
    int width, height;
    oracles_sdl_backend_pause_view(&s->backend, &window, &renderer, &frame, &width, &height);
    oracles_sdl_backend_suspend_audio(&s->backend);
    oracles_sdl_backend_hold(&s->backend, 1);
    const OraclesPauseSession services = { s, pause_save, pause_load, pause_state_time, pause_settings_changed };
    const OraclesPauseResult result = oracles_pause_run(s->pause, window, renderer, frame, width, height, &services);
    if (result == ORACLES_PAUSE_RESUME) oracles_sdl_backend_hold(&s->backend, 0);   /* a pause that ends the session is past the last present */
    if (result == ORACLES_PAUSE_WINDOW_CLOSED) oracles_sdl_backend_set_window_closed(&s->backend);
    return result == ORACLES_PAUSE_RESUME ? ORACLES_HOST_PAUSE_RESUME : ORACLES_HOST_PAUSE_QUIT;
}

static int input_source(void *opaque, uint32_t frame, unsigned *mask)
{
    session *s = opaque;
    if (!s->playing) return 0;
    if (oracles_route_mask_at(&s->play, frame, mask)) return 1;
    s->playing = 0;
    fprintf(stderr, "oracles: route finished at frame %u, the player has the controls\n", frame);
    oracles_hotkeys_session_route_over(s->hotkeys);
    return 0;
}

static void input_sink(void *opaque, uint32_t frame, unsigned mask)
{
    session *s = opaque;
    if (s->recording) oracles_route_writer_record(&s->record, frame, mask);
}

static int write_screenshot(const char *path, const uint32_t *pixels, uint32_t width, uint32_t height)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fprintf(f, "P6\n%u %u\n255\n", width, height);
    for (size_t i = 0; i < (size_t)width * height; i++) {
        const uint8_t rgb[3] = { (uint8_t)(pixels[i] >> 16), (uint8_t)(pixels[i] >> 8), (uint8_t)pixels[i] };
        fwrite(rgb, 1, 3, f);
    }
    return fclose(f) == 0;
}

/* ---- the session's steps --------------------------------------------------------- */

static int failed(OraclesSessionResult *result, const char *format, ...)
{
    /* The whole reason to stderr, as --rom always printed it; the result keeps what its field holds (a toast). */
    va_list arguments, copy;
    va_start(arguments, format);
    va_copy(copy, arguments);
    vsnprintf(result->error, sizeof result->error, format, arguments);
    fputs("oracles: ", stderr);
    vfprintf(stderr, format, copy);
    fputc('\n', stderr);
    va_end(copy);
    va_end(arguments);
    return 0;
}

/* The ROM, the core and the starting SRAM.  A replayed route brings its own
 * copy and never touches the session's save; a recorded route stores a copy next to it. */
static int load_game(session *s, const OraclesSessionOptions *o, OraclesSessionResult *result)
{
    char error[256];
    /* A fan game: the base ROM and its patch, the image built here at each start and never written. */
    s->rom = o->patch_path ? oracles_bps_apply_files(o->rom_path, o->patch_path, &s->rom_size, error, sizeof error)
                           : oracles_rom_read_file(o->rom_path, &s->rom_size, error, sizeof error);
    if (!s->rom) return failed(result, "%s", error);
    if (oracles_rom_identify(s->rom, s->rom_size, &s->info, error, sizeof error) != 0) return failed(result, "%s", error);
    s->profile = oracles_compat_find(&s->info);
    fprintf(stderr, "oracles: %s, %s ROM (%s)\n", oracles_game_name(s->info.game),
            s->profile ? oracles_compat_name(s->profile) : s->info.known ? "authenticated image" : "unknown, header accepted", s->info.sha1);
    /* A mod's houses are in the image before the core exists: every reader of the ROM sees them. */
    if (o->mods_count) {
        s->mod = oracles_mod_session_start(o->mods_dirs, o->mods_count, s->info.game, s->rom, s->rom_size, error, sizeof error);
        if (!s->mod || oracles_mod_session_compose(s->mod, oracles_rom_is_original(&s->info), &s->rom, &s->rom_size, error, sizeof error) != 0) return failed(result, "%s", error);
    }
    OraclesCoreOptions core_options = { o->mute || o->no_window ? 0u : SAMPLE_RATE_HZ, s->colour_applied };
    s->core = oracles_core_create(s->rom, s->rom_size, &core_options);
    if (!s->core) return failed(result, "the core could not start");
    oracles_session_save_path(o, s->save.path, sizeof s->save.path);
    /* A modded game's own save starts as a copy of the vanilla one: the vanilla save never holds what a mod did (its
     * houses, its items), and a game played without the mods finds it as it was. */
    if (s->mod && !o->save_path) {
        char vanilla[sizeof s->save.path];
        oracles_session_default_save_path(o->patch_path ? o->patch_path : o->rom_path, vanilla, sizeof vanilla);
        uint8_t *data = NULL;
        size_t size = 0;
        if (!o->play_path && !file_exists(s->save.path) && oracles_session_read_file(vanilla, &data, &size)) {
            if (oracles_session_write_file(s->save.path, data, size)) fprintf(stderr, "oracles: first game with mods: %s starts as a copy of %s, which the mods never write\n", s->save.path, vanilla);
            else { free(data); return failed(result, "cannot write %s", s->save.path); }
        }
        free(data);
    }
    /* With both routes, the one recorded extends the one replayed: it starts from the replay's SRAM. */
    if (o->play_path) snprintf(s->route_sram_path, sizeof s->route_sram_path, "%s.sram", o->play_path);
    if (o->record_path) snprintf(s->record_sram_path, sizeof s->record_sram_path, "%s.sram", o->record_path);
    const char *sram_source = o->play_path ? s->route_sram_path : s->save.path;
    if (!load_sram(s->core, sram_source, s->save.sha1)) return failed(result, "%s is not a raw SRAM image of the right size", sram_source);
    /* The mods' storage beside the SRAM the game starts from: the save's, written back; a replayed route's, read only. */
    if (s->mod) {
        char path[PATH_MAX_LENGTH + 16];
        if (o->play_path) snprintf(path, sizeof path, "%s.store", o->play_path);
        else oracles_mod_session_storage_path(s->save.path, path, sizeof path);
        if (oracles_mod_session_storage_open(s->mod, path, !o->play_path, error, sizeof error) != 0) return failed(result, "%s: %s", path, error);
        if (o->start_house) {
            if (oracles_mod_session_start_at_house(s->mod, o->start_house, s->core, error, sizeof error) != 0) return failed(result, "--start-at-house: %s", error);
            /* The SRAM the game starts from is the one moved in front of the door: a recorded route names that one. */
            const size_t size = oracles_core_sram_size(s->core);
            uint8_t *sram = malloc(size);
            if (!sram || oracles_core_save_sram(s->core, sram, size) != 0) { free(sram); return failed(result, "--start-at-house: the save cannot be read back"); }
            oracles_sha1_hex(sram, size, s->save.sha1);
            free(sram);
        }
    }
    s->state_info.game = s->info.game == ORACLES_GAME_AGES ? "ages" : s->info.game == ORACLES_GAME_SEASONS ? "seasons" : "unknown";
    s->state_info.rom_sha1 = s->info.sha1;
    s->state_info.mods = "";   /* set once the route's options are known */
    snprintf(s->state_path, sizeof s->state_path, "%s", s->save.path);
    char *dot = strrchr(s->state_path, '.');   /* <save>.sav -> <save>.state */
    if (dot && strcmp(dot, ".sav") == 0) *dot = 0;
    const size_t length = strlen(s->state_path);
    snprintf(s->state_path + length, sizeof s->state_path - length, ".state");
    return 1;
}

static int replay_route(session *s, const OraclesSessionOptions *o, OraclesSessionResult *result)
{
    char error[1024];
    if (oracles_route_read(o->play_path, &s->play, error, sizeof error) != 0) return failed(result, "%s", error);
    if (strcmp(s->play.header.rom_sha1, s->info.sha1) != 0 || strcmp(s->play.header.sram_sha1, s->save.sha1) != 0)
        return failed(result, "the route was recorded with another ROM or another starting SRAM (route: rom %s, sram %s; now: rom %s, sram %s from %s)",
                      s->play.header.rom_sha1, s->play.header.sram_sha1, s->info.sha1, s->save.sha1, s->route_sram_path);
    if (!oracles_mod_session_matches_route(s->mod, s->play.header.mods, error, sizeof error)
        || !oracles_mod_session_matches_store(o->play_path, s->play.header.store_sha1, error, sizeof error)) return failed(result, "%s", error);
    s->playing = 1;
    fprintf(stderr, "oracles: replaying %zu input changes from %s; the session's save file is not touched\n", s->play.count, o->play_path);
    /* The route's gameplay options are part of the replay: a route played
     * with --continuous-transitions diverges without it, and the other way round. */
    /* A route recorded before the core's joypad bouncing was cut (the header's core line) ran with it and only replays with it. */
    if (oracles_route_joypad_bouncing(&s->play.header)) {
        oracles_core_set_joypad_bouncing(s->core, 1);
        fprintf(stderr, "oracles: the route was recorded with joypad bouncing: it is on for the replay, which may still differ from the session it was\n");
    }
    const int route_continuous = oracles_route_has_option(&s->play.header, ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS);
    if (route_continuous && s->profile && !oracles_compat_continuous_transitions(s->profile))
        return failed(result, "the route was recorded with --continuous-transitions, which %s does not support", oracles_compat_name(s->profile));
    if (route_continuous && !s->continuous_transitions) { s->continuous_transitions = 1; s->enhanced = 1; fprintf(stderr, "oracles: the route was recorded with --continuous-transitions: the option is on for the replay\n"); }
    else if (!route_continuous && s->continuous_transitions) fprintf(stderr, "oracles: the route was recorded without --continuous-transitions: the replay diverges after its first transition\n");
    const int route_swim = oracles_route_has_option(&s->play.header, ORACLES_ROUTE_OPTION_CONTINUOUS_SWIM);
    if (route_swim && s->profile && !oracles_compat_continuous_swim(s->profile))
        return failed(result, "the route was recorded with --continuous-swim, which %s does not support", oracles_compat_name(s->profile));
    if (route_swim && !s->continuous_swim) { s->continuous_swim = s->continuous_transitions = 1; s->enhanced = 1; fprintf(stderr, "oracles: the route was recorded with --continuous-swim: the option is on for the replay\n"); }
    else if (!route_swim && s->continuous_swim) fprintf(stderr, "oracles: the route was recorded without --continuous-swim: the replay diverges after its first transition swum\n");
    return 1;
}

/* Whether two paths name the same file: the one to write would truncate the one being read. */
static int same_file(const char *a, const char *b)
{
    struct stat sa, sb;
    if (strcmp(a, b) == 0) return 1;
    return stat(a, &sa) == 0 && stat(b, &sb) == 0 && sa.st_ino != 0 && sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

static int record_route(session *s, const OraclesSessionOptions *o, OraclesSessionResult *result)
{
    if (o->play_path) {
        /* An extension records the replayed inputs, not the replayed inventory exchanges
         * (their effects are applied, never written): such a route would lose them. */
        if (same_file(o->play_path, o->record_path)) return failed(result, "--play and --record name the same route");
        if (s->play.action_count) return failed(result, "%s has inventory exchanges, which an extension would not carry: it cannot be extended", o->play_path);
    }
    if (strcmp(s->save.sha1, "none") != 0) {
        const size_t size = oracles_core_sram_size(s->core);
        uint8_t *sram = malloc(size);
        const int ok = sram && oracles_core_save_sram(s->core, sram, size) == 0 && oracles_session_write_file(s->record_sram_path, sram, size);
        free(sram);
        if (!ok) return failed(result, "cannot write the starting SRAM to %s", s->record_sram_path);
    } else {
        remove(s->record_sram_path);
    }
    OraclesRouteHeader header;
    snprintf(header.game, sizeof header.game, "%s", s->state_info.game);
    snprintf(header.rom_sha1, sizeof header.rom_sha1, "%s", s->info.sha1);
    snprintf(header.sram_sha1, sizeof header.sram_sha1, "%s", s->save.sha1);
    snprintf(header.options, sizeof header.options, "%s", s->continuous_swim ? ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS "," ORACLES_ROUTE_OPTION_CONTINUOUS_SWIM
                                                          : s->continuous_transitions ? ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS : "");
    snprintf(header.mods, sizeof header.mods, "%s", oracles_mod_session_identity(s->mod));
    header.store_sha1[0] = 0;
    if (s->mod) {   /* the mods' storage the route starts from, as ROUTE.sram, named in the header */
        char path[PATH_MAX_LENGTH + 16];
        snprintf(path, sizeof path, "%s.store", o->record_path);
        if (!oracles_mod_session_storage_copy(s->mod, path)) return failed(result, "cannot write %s", path);
        oracles_mod_session_file_sha1(path, header.store_sha1);
    }
    /* As every session runs, joypad bouncing off; an extension runs as the route it
     * extends, whose replay keeps the joypad bouncing of a route recorded before. */
    snprintf(header.core, sizeof header.core, "%s", o->play_path ? s->play.header.core : ORACLES_ROUTE_CORE_JOYPAD_BOUNCING_OFF);
    if (oracles_route_writer_open(&s->record, o->record_path, &header) != 0) return failed(result, "cannot write %s", o->record_path);
    if (s->mod) {
        char path[PATH_MAX_LENGTH + 16];
        snprintf(path, sizeof path, "%s.mod.tsv", o->record_path);
        if (!oracles_mod_session_trace(s->mod, path)) return failed(result, "cannot write %s", path);
    }
    s->recording = 1;
    return 1;
}

/* The window's options from the settings: vsync (unless --vsync gave it), keys, buttons, and the hotkeys' when they are on. */
static void window_options(session *s, const OraclesSessionOptions *o)
{
    s->sdl = o->sdl;
    oracles_settings *prefs = s->settings;
    if (!o->no_window) {
        s->display_hz = oracles_sdl_display_refresh_hz();
        if (!strcmp(s->vsync, "on")) s->sdl.vsync = 1;
        else if (!strcmp(s->vsync, "auto")) s->sdl.vsync = s->display_hz >= 59u && s->display_hz <= 61u;
    }
    s->sdl.pause_menu = s->pause != NULL && !o->no_window;
    for (unsigned i = 0; i < ORACLES_BINDINGS; i++) s->sdl.key_names[i] = prefs->key_names[i];
    for (unsigned i = 0; i < 4; i++) s->sdl.pad_names[i] = prefs->pad_names[i];
    if (s->hotkeys_mode != ORACLES_HOTKEYS_OFF) {
        for (unsigned i = 0; i < ORACLES_HOTKEY_SLOTS; i++) { s->sdl.hotkey_key_names[i] = prefs->hotkey_key_names[i]; s->sdl.hotkey_pad_names[i] = prefs->hotkey_pad_names[i]; }
        s->sdl.hotkey_bind_b = prefs->hotkey_bind_b;
        s->sdl.hotkey_bind_a = prefs->hotkey_bind_a;
    }
}

/* A game the home screen started plays on without the item hotkeys when they cannot attach to its ROM (one that is
 * not an original): the home screen then says so under the ROM's status.  --rom still stops, with its advice. */
static void drop_hotkeys(session *s, OraclesSessionResult *result, const char *why)
{
    fprintf(stderr, "oracles: %s; this game plays without the item hotkeys\n", why);
    s->hotkeys_mode = ORACLES_HOTKEYS_OFF;
    result->hotkeys_dropped = 1;
}

/* What the image's profile allows: the continuous transitions, the drawn-back
 * view and the item hotkeys read the game where they are qualified only.  A
 * game the home screen started plays without the view or the hotkeys it does
 * not allow; --rom refuses them, with the reason. */
static int check_profile(session *s, const OraclesSessionOptions *o, OraclesSessionResult *result)
{
    if (!s->profile) return 1;   /* no hooks at all: attach says so when they are asked for */
    const char *name = oracles_compat_name(s->profile);
    const int from_home = o->sdl.window != NULL;
    if (s->continuous_transitions && !oracles_compat_continuous_transitions(s->profile))
        return failed(result, "%s does not support --continuous-transitions", name);
    if (s->continuous_swim && !oracles_compat_continuous_swim(s->profile)) {
        if (!from_home) return failed(result, "%s does not support --continuous-swim", name);
        fprintf(stderr, "oracles: %s does not support the continuous transitions when swimming; they stay on foot\n", name);
        s->continuous_swim = 0;
    }
    if (s->zoom_out && !oracles_compat_zoom_out(s->profile)) {
        if (!from_home) return failed(result, "%s does not support --zoom-out yet", name);
        fprintf(stderr, "oracles: %s does not support the drawn-back view yet; the band plays instead\n", name);
        s->zoom_out = 0;
    }
    const int route_exchanges = o->play_path && s->play.action_count != 0;
    if ((s->hotkeys_mode != ORACLES_HOTKEYS_OFF || route_exchanges) && !oracles_compat_item_hotkeys(s->profile)) {
        if (!from_home || route_exchanges) return failed(result, "%s does not support the item hotkeys yet", name);
        drop_hotkeys(s, result, "the item hotkeys are not qualified for this game");
    }
    return 1;
}

/* The guest (hooks, bus, journal) serves the diagnostic panel, the native
 * renderer, Enhanced and the gameplay options; each is attached here. */
static int attach(session *s, const OraclesSessionOptions *o, OraclesSessionResult *result)
{
    /* The item hotkeys attach it in Faithful too: the image stays the core's; a replayed route that carries exchanges needs it as well. */
    const int route_exchanges = o->play_path && s->play.action_count != 0;
    const int from_home = o->sdl.window != NULL;
    if (o->diagnostics || o->native_renderer || s->enhanced || s->hotkeys_mode != ORACLES_HOTKEYS_OFF || route_exchanges || s->mod) {
        s->guest = oracles_guest_attach(s->core, s->profile);
        const int for_hotkeys_alone = !o->diagnostics && !o->native_renderer && !s->enhanced && !route_exchanges && !s->mod;
        if (!s->guest && from_home && for_hotkeys_alone) drop_hotkeys(s, result, "the hooks cannot be armed in this ROM");
        else if (!s->guest) return failed(result, "cannot arm the hooks for this game");
    }
    if (o->diagnostics) s->diag = oracles_diagnostics_start(s->guest, stderr);
    if (o->native_renderer) {
        s->check = oracles_frame_check_start(s->guest, s->core, NULL, 0);
        fprintf(stderr, "oracles: native renderer on screen; the core's frame is compared with it every vblank\n");
    }
    /* The gameplay options that change the guest state are part of a savestate's identity, like the mods; the swim
     * is not: a state saved with it is a state of the game without it too (the policy gives a doubled step back to the
     * game when it may not go on), and the states taken with the transitions before it load as they did. */
    snprintf(s->state_mods, sizeof s->state_mods, "%s%s%s", s->continuous_transitions ? ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS : "",
             s->continuous_transitions && s->mod ? "," : "", oracles_mod_session_identity(s->mod));
    s->state_info.mods = s->state_mods;
    if (s->mod) {
        char error[256];
        if (oracles_mod_session_attach(s->mod, s->guest, error, sizeof error) != 0) return failed(result, "%s", error);
    }
    if (s->enhanced) {
        s->view = oracles_enhanced_view_start(s->core, s->guest, s->rom, s->rom_size);
        if (!s->view) return failed(result, "cannot start the Enhanced view");
        oracles_enhanced_view_set_zoom_out(s->view, s->zoom_out);
        const int camera = o->camera_profile ? o->camera_profile : s->settings->camera;
        oracles_enhanced_view_set_camera_profile(s->view, (unsigned)camera);
        if (o->neighbour_objects) oracles_enhanced_view_set_neighbour_objects(s->view, 1);
        fprintf(stderr, "oracles: Enhanced profile on screen (%ux%u): the wide world band, neighbours from the ghost instance with their objects%s, the framed fallback; F3 switches to the framed core; camera profile %d\n",
                oracles_enhanced_view_width(s->view), oracles_enhanced_view_height(s->view), o->neighbour_objects ? "" : " off", camera);
    }
    if (s->continuous_transitions && s->guest) {
        s->transition = oracles_room_transition_start(s->guest, s->rom, s->rom_size, s->continuous_swim);
        if (s->transition) fprintf(stderr, "oracles: continuous transitions on%s: Link keeps walking through scrolling transitions; the game's state differs from a native session's\n", s->continuous_swim ? ", swimming too" : "");
        else fprintf(stderr, "oracles: continuous transitions need the hooks of a recognised game; the native transitions stay\n");
    }
    if (!s->transition) { free(s->rom); s->rom = NULL; }   /* the policy reads the animation data from the ROM */
    if (s->guest && (s->hotkeys_mode != ORACLES_HOTKEYS_OFF || route_exchanges)) {
        s->hotkeys = oracles_hotkeys_session_start(s->guest, s->info.game, s->hotkeys_mode, s->settings, o->play_path ? &s->play : NULL);
        if (!s->hotkeys && from_home && !route_exchanges) drop_hotkeys(s, result, "the item hotkeys cannot find the inventory's write point in this ROM");
        else if (!s->hotkeys) return failed(result, "the item hotkeys cannot find the inventory's write point in this ROM; --item-hotkeys=off to play without them");
        if (s->hotkeys && s->recording) oracles_hotkeys_session_record(s->hotkeys, &s->record);
        if (s->hotkeys_mode != ORACLES_HOTKEYS_OFF)
            fprintf(stderr, "oracles: item hotkeys on, mode %s: keys %s %s %s %s; held with %s or %s a key takes the item of B or of A, in the inventory the item under the cursor\n",
                    o->hotkeys_option, s->settings->hotkey_key_names[0], s->settings->hotkey_key_names[1], s->settings->hotkey_key_names[2],
                    s->settings->hotkey_key_names[3], s->settings->hotkey_bind_b, s->settings->hotkey_bind_a);
    }
    if (s->guest && s->recording) {
        /* What the session was, frame by frame: `oracles-harness --compare` sets it against the replay of the route. */
        char path[PATH_MAX_LENGTH + 16];
        snprintf(path, sizeof path, "%s.session.tsv", o->record_path);
        s->fingerprints = fopen(path, "wb");
    }
    return 1;
}

static void configure(session *s, const OraclesSessionOptions *o, oracles_host_run_config *config)
{
    memset(config, 0, sizeof *config);
    config->core = s->core;
    config->sample_rate_hz = o->mute || o->no_window ? 0u : SAMPLE_RATE_HZ;
    config->quit_after_frames = o->frames;
    config->pace_frames = !o->no_window && !s->sdl.vsync;   /* with vsync the display paces */
    config->display_paces = !o->no_window && s->sdl.vsync;  /* as long as the host sees it do so */
    config->measure_frames = !o->no_window;                   /* the work is timed either way */
    if (!o->play_path) {
        config->save_interval_frames = SAVE_INTERVAL_FRAMES;
        config->store_save = store_save;
        config->store_save_opaque = &s->save;
    }
    config->on_command = on_command;
    config->on_command_opaque = s;
    if (o->play_path) { config->input_source = input_source; config->input_source_opaque = s; }
    if (o->record_path) { config->input_sink = input_sink; config->input_sink_opaque = s; }
    if (s->hotkeys) { config->input_filter = oracles_hotkeys_session_filter; config->input_filter_opaque = s->hotkeys; }
    if (s->guest) { config->on_frame_begin = frame_begin; config->on_frame_end = frame_end; config->frame_opaque = s; }
    if (s->check) { config->frame_source = native_frame; config->frame_source_opaque = s->check; }
    if (s->sdl.pause_menu) { config->on_started = session_started; config->on_pause = session_pause; config->pause_opaque = s; }
    if (s->view) {
        config->frame_source = oracles_enhanced_view_frame_source;
        config->frame_source_opaque = s->view;
        config->frame_width = oracles_enhanced_view_width(s->view);
        config->frame_height = oracles_enhanced_view_height(s->view);
    }
    if (s->mod) oracles_mod_session_configure(s->mod, config);   /* over whichever frame source is set */
    fprintf(stderr, "oracles: colour correction %s (F2 to change); settings in %s; F5/F7 save and load %s\n",
            s->colour_applied ? "on" : "off", s->settings->path[0] ? s->settings->path : "(no settings directory)", s->state_path);
    if (!o->no_window)
        fprintf(stderr, "oracles: vsync %s (setting %s, display %u Hz): %s\n", s->sdl.vsync ? "on" : "off", s->vsync, s->display_hz,
                s->sdl.vsync ? "the display paces the frames" : "the host paces the frames at 59.7275 Hz");
}

static void report_frames(const session *s, const oracles_host_run_report *report)
{
    fprintf(stderr, "oracles: version %s, build %s\n", ORACLES_VERSION, ORACLES_BUILD);
    fprintf(stderr, "oracles: %u frames, %u late, %u resyncs, %u saves written to %s\n",
            report->frames_presented, report->frames_late, report->pacing_resyncs, report->saves_written, s->save.path);
    /* The pause holds the loop between frames: its time is in no frame's; none of its glyphs should be rasterised here. */
    if (s->pause)
        fprintf(stderr, "oracles: pause menu opened %u times; glyphs rasterised during the session's pauses: %u\n",
                report->pauses, oracles_pause_late_glyphs(s->pause));
    if (!report->frames_presented || !report->frame_ns_total) return;
    fprintf(stderr, "oracles: frame work %.2f ms on average, %.2f ms at most (budget 4 ms with hooks); %.2f ms of the average is the presentation%s\n",
            (double)report->frame_ns_total / report->frames_presented / 1e6, (double)report->frame_ns_max / 1e6,
            (double)report->present_ns_total / report->frames_presented / 1e6, s->sdl.vsync ? ", which waits for the display under vsync" : "");
    /* Where the time goes, for the peak a frame's budget allows (12 ms). */
    fprintf(stderr, "oracles: the worst frame, %u: core %.2f ms, view %.2f ms, present %.2f ms, end %.2f ms; each phase at most: core %.2f, view %.2f, present %.2f, end %.2f ms; frames over 12 ms: %u, over one frame (16.7 ms): %u\n",
            report->frame_max_index, (double)report->max_frame_core_ns / 1e6, (double)report->max_frame_source_ns / 1e6,
            (double)report->max_frame_present_ns / 1e6, (double)report->max_frame_end_ns / 1e6,
            (double)report->core_ns_max / 1e6, (double)report->source_ns_max / 1e6, (double)report->present_ns_max / 1e6, (double)report->end_ns_max / 1e6,
            report->frames_over_12ms, report->frames_over_16ms);
    /* The frame budget: past the first second, no
     * frame's work over one frame, and at most one frame in a thousand
     * late.  Said here so that a session judges it without arithmetic. */
    const int held = report->frames_over_budget == 0 && (uint64_t)report->frames_late * 1000u <= report->frames_presented;
    fprintf(stderr, "oracles: frame budget %s: %u frames whose work, presentation left out, took over one frame past the first second (0 allowed), %u late of %u (1 in 1000 allowed; none counted when the display paces)\n",
            held ? "held" : "NOT held", report->frames_over_budget, report->frames_late, report->frames_presented);
}

/* What the session's parts have to say at its end, and their release. */
static void finish(session *s, const OraclesSessionOptions *o)
{
    if (o->screenshot && s->ran) {
        const uint32_t width = s->view ? oracles_enhanced_view_width(s->view) : ORACLES_SCREEN_WIDTH;
        const uint32_t height = s->view ? oracles_enhanced_view_height(s->view) : ORACLES_SCREEN_HEIGHT;
        const uint32_t *pixels = s->view ? oracles_enhanced_view_surface(s->view) : oracles_core_pixels(s->core);
        const int ok = write_screenshot(o->screenshot, oracles_mod_session_screen(s->mod, pixels, width, height), width, height);
        if (!ok) fprintf(stderr, "oracles: cannot write %s\n", o->screenshot);
    }
    if (s->check) { if (s->ran) oracles_frame_check_report(s->check, stderr); oracles_frame_check_stop(s->check); }
    if (s->transition) {
        OraclesRoomTransitionStats stats;
        oracles_room_transition_stats(s->transition, &stats);
        fprintf(stderr, "oracles: continuous transitions: %u taken over (%u swum, %u started by a stroke at an edge), Link moved on %u frames (%u swimming), held back by a wall ahead on %u, left alone on %u, palette_refreshes %u (background palettes refreshed after a palette mix outlived the doubled scroll)\n",
                stats.transitions, stats.swim_transitions, stats.forced_transitions, stats.walked_frames, stats.swum_frames, stats.capped_frames, stats.refused_frames, stats.palette_refreshes);
        oracles_room_transition_stop(s->transition);
    }
    oracles_hotkeys_session_stop(s->hotkeys, stderr);
    if (s->view) {
        /* What the session showed black, counted as the harness counts it (enhanced.black_pixels_mean). */
        OraclesEnhancedBlack black;
        oracles_enhanced_view_black(s->view, &black);
        fprintf(stderr, "oracles: black in the band: %.0f pixels a world frame on average, %u at most, over %u world frames from the first full one (frame %u); rooms of the overworld's grid black whole (in the band by a pixel at least, nothing drawn there) for more than %u world frames in a row: %u",
                black.full_world_frames ? (double)black.pixels / black.full_world_frames : 0.0, black.max, black.full_world_frames, black.first_full,
                ORACLES_ENHANCED_BLACK_ROOM_FRAMES, black.rooms_over);
        for (unsigned i = 0; i < black.rooms_over && i < ORACLES_ENHANCED_BLACK_ROOMS_KEPT; i++)
            fprintf(stderr, "%s%u:%02x from frame %u, %u frames", i ? ", " : " (", black.longest[i].group, black.longest[i].room, black.longest[i].from, black.longest[i].frames);
        fprintf(stderr, "%s\n", black.rooms_over ? ")" : "");
        oracles_enhanced_view_stop(s->view);
    }
    if (s->diag) oracles_diagnostics_stop(s->diag);
    oracles_mod_session_stop(s->mod, NULL);
    if (s->guest) oracles_guest_detach(s->guest);
    if (s->ran && !o->no_window) {
        unsigned underruns = 0, drops = 0, peak_ms = 0, average_ms = 0;
        oracles_sdl_backend_audio_report(&s->backend, &underruns, &drops, &peak_ms, &average_ms);
        fprintf(stderr, "oracles: audio: %u underruns (gaps heard), %u chunks dropped, queue %u ms on average, %u ms at most (target 70 ms; the average is the latency)\n",
                underruns, drops, average_ms, peak_ms);
        const oracles_sdl_underrun *log = NULL;
        const unsigned logged = oracles_sdl_backend_underruns(&s->backend, &log);
        for (unsigned i = 0; i < logged; i++)
            fprintf(stderr, "oracles: audio underrun at frame %u: %.1f ms since the previous chunk, which left %u ms queued; the last presentation took %.1f ms\n",
                    log[i].frame, (double)log[i].since_previous_chunk_ns / 1e6, log[i].previous_chunk_left_ms, (double)log[i].last_present_ns / 1e6);
        if (underruns > logged) fprintf(stderr, "oracles: audio: %u more underruns not listed\n", underruns - logged);
        unsigned presents = 0;
        double seconds = 0.0;
        const char *renderer = NULL;
        int renderer_vsync = 0;
        oracles_sdl_backend_present_report(&s->backend, &presents, &seconds, &renderer, &renderer_vsync);
        fprintf(stderr, "oracles: vsync %s: renderer %s (vsync %s), %u frames presented in %.1f s, %.2f Hz; ",
                s->sdl.vsync ? "on" : "off", renderer, renderer_vsync ? "reported" : "not reported",
                presents, seconds, seconds > 0.0 ? (double)(presents - 1u) / seconds : 0.0);
        if (!s->sdl.vsync) fprintf(stderr, "the host paced the whole run\n");
        else if (s->display_unpaced_frame)
            fprintf(stderr, "the presentation did not wait for the display: vsync off and the host paced from frame %u\n", s->display_unpaced_frame);
        else fprintf(stderr, "the display paced the whole run\n");
    }
    oracles_session_stop_recording(s, "finished");
    if (s->fingerprints) fclose(s->fingerprints);
    oracles_route_free(&s->play);
    if (s->backend_ready) { if (o->no_window) oracles_null_backend_release(&s->backend); else oracles_sdl_backend_release(&s->backend); }
    if (s->core) oracles_core_destroy(s->core);
    free(s->rom);
}

int oracles_session_run(const OraclesSessionOptions *o, oracles_settings *settings, OraclesSessionResult *result)
{
    memset(result, 0, sizeof *result);
    session *s = calloc(1, sizeof *s);
    if (!s) { snprintf(result->error, sizeof result->error, "out of memory"); return 1; }
    s->settings = settings;
    s->pause = o->pause;
    /* --colour-correction and --vsync hold for this session and are never stored, as camera_profile. */
    s->colour_setting = settings->colour_correction;
    s->colour_applied = o->colour_option ? !strcmp(o->colour_option, "on") : settings->colour_correction;
    s->vsync = o->vsync_option ? o->vsync_option : settings->vsync;
    s->record_path = o->record_path;
    s->enhanced = o->enhanced || o->zoom_out || o->continuous_transitions || o->continuous_swim;
    s->continuous_transitions = o->continuous_transitions || o->continuous_swim;
    s->continuous_swim = o->continuous_swim;
    s->zoom_out = o->zoom_out;
    s->hotkeys_mode = oracles_settings_hotkeys_mode(o->hotkeys_option);   /* for this run alone; the slots and the keys are remembered */
    /* The profile's refusals come before the recording opens: a refused session writes no route. */
    int ok = load_game(s, o, result) && (!o->play_path || replay_route(s, o, result)) && check_profile(s, o, result)
          && (!o->record_path || record_route(s, o, result));
    if (ok) {
        window_options(s, o);
        s->backend_ready = o->no_window ? oracles_null_backend_init(&s->backend) : oracles_sdl_backend_init(&s->backend, &s->sdl);
        if (!s->backend_ready) ok = failed(result, "out of memory");
    }
    ok = ok && attach(s, o, result);
    if (ok) {
        oracles_host_run_config config;
        configure(s, o, &config);
        oracles_host_run_report report;
        char error[256];
        const int status = oracles_host_run(&config, &s->backend, &report, error, sizeof error);
        s->ran = 1;
        s->display_unpaced_frame = report.display_unpaced_frame;
        if (status != ORACLES_HOST_OK) ok = failed(result, "%s", error);
        report_frames(s, &report);
        if (!o->no_window) result->window_closed = oracles_sdl_backend_window_closed(&s->backend);
    }
    finish(s, o);
    result->route_written = s->route_written;
    free(s);
    return ok ? 0 : 1;
}
