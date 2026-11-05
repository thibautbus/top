/* A game session (session.h), moved out of the launcher's main so that the
 * home screen can start one in its window.
 *
 * --zoom-out (implies --enhanced) draws the view back: a surface of
 * 480x270, four times 1920x1080, that shows three rooms across and two down
 * outdoors, or 480x360 on a 4:3 screen.  A savestate taken with it loads only with it, and the other way
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
 * the first second it did not.  On Android "auto" is off: the system
 * composes the frames, and presenting in step with the display there only
 * added latency on a handheld.  The launcher owns every file: the
 * core and the host only see buffers. */
#include "session_internal.h"

#include "guest_fingerprint.h"
#include "home_choices.h"
#include "home_input.h"
#include "hotkeys_session.h"
#include "view.h"
#include "guest.h"
#include "bps.h"
#include "rom.h"
#include "route.h"
#include "sha1.h"
#include "state_refusal.h"
#include "text_box.h"
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

/* A change from the pause gives a replayed route's controls back to the player. */
static void replay_stopped(session *s, uint32_t frame, const char *why)
{
    if (!s->playing) return;
    s->playing = 0;
    fprintf(stderr, "oracles: route replay stopped by %s at frame %u, the player has the controls\n", why, frame);
    oracles_hotkeys_session_route_over(s->hotkeys);
}

static void frame_begin(void *opaque, uint32_t frame)
{
    session *s = opaque;
    oracles_session_follow_subpixel(s);
    /* The pause's, when they differ from the game's (one turned on and off again changes nothing): a route recorded
     * says so; a replay, which has no pause today (--play is --rom's), would give the controls back. */
    if (s->cheats_pending) {
        s->cheats_pending = 0;
        if (s->cheats_next != s->cheats) {
            replay_stopped(s, frame, "the cheats changed");
            oracles_session_change_cheats(s, frame, s->cheats_next);
        }
    }
    if (s->swim_pending) {
        s->swim_pending = 0;
        if (s->swim_next != s->swim_hold) {
            replay_stopped(s, frame, "the swim's hold changed");
            oracles_session_change_swim_hold(s, frame, s->swim_next);
        }
    }
    if (s->text_speed_pending) {
        s->text_speed_pending = 0;
        if (s->text_speed_next != s->text_instant) {
            replay_stopped(s, frame, "the text speed changed");
            oracles_session_change_text_speed(s, frame, s->text_speed_next);
        }
    }
    if (s->hotkeys_pending) {
        s->hotkeys_pending = 0;
        if (s->hotkeys_next != (s->hotkeys ? oracles_hotkeys_session_next_mode(s->hotkeys) : s->hotkeys_mode)) {
            replay_stopped(s, frame, "the item hotkeys changed");
            oracles_session_change_hotkeys(s, frame, s->hotkeys_next);
        }
    }
    if (s->window_line >= 0) {   /* the window the pause changed, presentation alone: a replay goes on */
        if (s->recording) oracles_route_writer_set(&s->record, frame, ORACLES_ROUTE_SET_WINDOW, s->window_line);
        s->window_line = -1;
    }
    size_t first = 0;
    const size_t changes = s->playing ? oracles_route_cheats_at(&s->play, frame, &first) : 0;
    for (size_t i = first; i < first + changes; i++) {
        unsigned bit = 0;
        oracles_cheats_parse(s->play.cheats[i].name, NULL, &bit);   /* each known: the replay refused the route otherwise */
        oracles_session_change_cheats(s, frame, s->play.cheats[i].on ? s->cheats | bit : s->cheats & ~bit);
    }
    /* A replayed route's `set` lines: the swim's hold and the text speed change the game; the item hotkeys' exchanges are the route's
     * `equip` lines, their mode the one the player gets after the replay; the window is presentation alone.  An
     * extension writes each again. */
    const size_t sets = s->playing ? oracles_route_settings_at(&s->play, frame, &first) : 0;
    for (size_t i = first; i < first + sets; i++) {
        const OraclesRouteSetting *set = &s->play.settings[i];
        if (set->name == ORACLES_ROUTE_SET_SWIM_HOLD) { oracles_session_change_swim_hold(s, frame, set->value); continue; }
        if (set->name == ORACLES_ROUTE_SET_TEXT_SPEED) { oracles_session_change_text_speed(s, frame, set->value); continue; }
        if (set->name == ORACLES_ROUTE_SET_ITEM_HOTKEYS) {
            oracles_hotkeys_session_set_mode(s->hotkeys, (OraclesHotkeysMode)set->value);
            s->hotkeys_mode = (OraclesHotkeysMode)set->value;
        } else oracles_session_apply_window(s, set->value);
        if (s->recording) oracles_route_writer_set(&s->record, frame, set->name, set->value);
    }
    if (s->guest) oracles_guest_set_frame(s->guest, frame);
    if (s->diag) oracles_diagnostics_frame_begin(s->diag, frame);
    if (s->hotkeys) oracles_session_hotkeys_settled(s, frame, oracles_hotkeys_session_settle(s->hotkeys, stderr));
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

static unsigned input_filter(void *opaque, uint32_t frame, unsigned mask)
{
    const session *s = opaque;
    return s->hotkeys ? oracles_hotkeys_session_filter(s->hotkeys, frame, mask) : mask;
}

static void input_sink(void *opaque, uint32_t frame, unsigned mask)
{
    session *s = opaque;
    if (s->recording) oracles_route_writer_record(&s->record, frame, mask);
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

/* The compact text lays the texts out again where the language is composed, before the route's replay and the
 * profile's checks settle the option: decided here as they will settle it, from a replayed route's options (the
 * compact text and, which turn Enhanced on, the transitions'); English (US) is then composed too.  A compact text the
 * profile or Faithful refuses lays nothing out: check_profile refuses it or turns it off. */
static int lay_out_texts(session *s, const OraclesSessionOptions *o, OraclesSessionResult *result)
{
    int compact = s->compact_text, wide = s->text_box_wide, enhanced = s->enhanced;
    OraclesRoute recorded;
    char error[256];
    if (o->play_path && oracles_route_read(o->play_path, &recorded, error, sizeof error) == 0) {
        compact = oracles_route_has_option(&recorded.header, ORACLES_ROUTE_OPTION_TEXT_COMPACT);
        wide = oracles_route_has_option(&recorded.header, ORACLES_ROUTE_OPTION_TEXT_BOX_WIDE);
        enhanced |= oracles_route_has_option(&recorded.header, ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS)
                  || oracles_route_has_option(&recorded.header, ORACLES_ROUTE_OPTION_CONTINUOUS_SWIM)
                  || oracles_route_has_option(&recorded.header, ORACLES_ROUTE_OPTION_CONTINUOUS_DIAGONAL)
                  || oracles_route_has_option(&recorded.header, ORACLES_ROUTE_OPTION_CONTINUOUS_STEPS);
        oracles_route_free(&recorded);
    }
    OraclesLanguageLayout layout = { 0u, 0, 0u, NULL };
    if (!oracles_session_text_layout(oracles_compat_compact_text(s->profile), oracles_compat_host_text_box(s->profile), enhanced, compact, wide,
                                     &layout.width, &layout.fixed)) return 1;
    if (oracles_language_session_lay_out(&s->language, &s->info, &layout, error, sizeof error) != 0) return failed(result, "%s", error);
    return 1;
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
    /* A language's pack comes from the US ROM as read; the language is composed after the mods' houses. */
    if (o->language && oracles_language_session_start(&s->language, o->language, o->eu_rom_path, o->language_pack, o->patch_path ? o->rom_path : NULL, &s->info, s->rom, s->rom_size, error, sizeof error) != 0)
        return failed(result, "%s", error);
    if (oracles_language_session_note(s->language)[0]) {
        fprintf(stderr, "oracles: %s\n", oracles_language_session_note(s->language));
        snprintf(result->language_note, sizeof result->language_note, "%s", oracles_language_session_note(s->language));
    }
    /* A mod's houses are in the image before the core exists: every reader of the ROM sees them. */
    if (o->mods_count) {
        s->mod = oracles_mod_session_start(o->mods_dirs, o->mods_count, s->info.game, s->rom, s->rom_size, error, sizeof error);
        if (!s->mod || oracles_mod_session_compose(s->mod, oracles_rom_is_original(&s->info), &s->rom, &s->rom_size, error, sizeof error) != 0) return failed(result, "%s", error);
    }
    if (!lay_out_texts(s, o, result)) return 0;
    if (s->language && oracles_language_session_compose(s->language, &s->rom, &s->rom_size, error, sizeof error) != 0) return failed(result, "%s", error);
    /* The command line's --core, else the core a replayed route was recorded on, else the settings' core= (Display's Core). */
    OraclesCoreKind kind = o->core ? (OraclesCoreKind)(o->core - 1) : (OraclesCoreKind)oracles_settings_core(s->settings);
    if (o->play_path && !o->core) {
        OraclesRoute recorded;
        char ignored[128];
        if (oracles_route_read(o->play_path, &recorded, ignored, sizeof ignored) == 0) {
            const OraclesCoreKind on = oracles_route_core_mgba(&recorded.header) ? ORACLES_CORE_MGBA : ORACLES_CORE_SAMEBOY;
            if (on != kind) fprintf(stderr, "oracles: the route was recorded on %s: it replays on it\n", on == ORACLES_CORE_MGBA ? "mGBA" : "SameBoy");
            kind = on;
            oracles_route_free(&recorded);
        }
    }
    OraclesCoreOptions core_options = { o->mute || o->no_window ? 0u : SAMPLE_RATE_HZ, s->colour_applied, kind, 0 };
    s->core = oracles_core_create(s->rom, s->rom_size, &core_options);
    if (!s->core) return failed(result, "the core could not start");
    fprintf(stderr, "oracles: core %s\n", oracles_core_version(s->core));
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

/* The frame shows the game's status bar in play (oracles_enhanced_bar_in_play), in the wide world in Enhanced, not a
 * framed screen; a ROM without a profile, whose bar is not known, from its first frame. */
static int bar_in_play(session *s)
{
    OraclesGuest *reader = s->guest ? s->guest : s->bar_reader;
    uint32_t framed[4];
    if (!s->profile) return 1;
    if (s->view && (oracles_enhanced_view_framed_only(s->view) || oracles_enhanced_view_framed_crop(s->view, framed))) return 0;
    return oracles_enhanced_bar_in_play(reader, s->core);
}

/* The backend's overlay: the pause's reminder over the frame about to be presented, once the status bar has been in play
 * for two seconds of frames in a row (a flash of it before a cutscene does not start it), the device of the last input
 * in the game naming its key, under the bar of the image as the session presents it. */
static int reminder_overlay(void *opaque, SDL_Renderer *renderer)
{
    session *s = opaque;
    s->bar_frames = bar_in_play(s) ? s->bar_frames + 1u : 0u;
    OraclesPauseFrame frame = { s->bar_frames >= ORACLES_ENHANCED_BAR_SETTLE, oracles_home_menu_device((int)oracles_sdl_backend_last_input(&s->backend)), 0, 0, 0 };
    oracles_sdl_backend_pause_size(&s->backend, &frame.width, &frame.height);
    SDL_Window *window = SDL_GetRenderWindow(renderer);
    frame.fill = s->sdl.fill && window && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN);
    return oracles_pause_overlay(s->pause, renderer, &frame);
}

/* The window's options from the settings: vsync (unless --vsync gave it), keys, buttons, and the hotkeys' when they are on. */
static void window_options(session *s, const OraclesSessionOptions *o)
{
    s->sdl = o->sdl;
    oracles_settings *prefs = s->settings;
    if (!o->no_window) {
        s->display_hz = oracles_sdl_display_refresh_hz();
        if (oracles_session_vsync_on(s->vsync, s->display_hz)) s->sdl.vsync = 1;
    }
    s->sdl.pause_menu = s->pause != NULL && !o->no_window;
    /* Over the first frames in play, the pause's reminder when one is due (oracles_pause_reminder); dropped at once when
     * none is. */
    s->sdl.overlay = s->sdl.pause_menu ? reminder_overlay : NULL;
    s->sdl.overlay_opaque = s;
    s->sdl.fill = prefs->scaling_fill;
    s->sdl.screen_margin = oracles_session_screen_margin(o->screen_margin, prefs);
    for (unsigned i = 0; i < ORACLES_BINDINGS; i++) s->sdl.key_names[i] = prefs->key_names[i];
    for (unsigned i = 0; i < 4; i++) s->sdl.pad_names[i] = prefs->pad_names[i];
    s->sdl.menu_pad_name = prefs->menu_pad;
    if (s->hotkeys_mode != ORACLES_HOTKEYS_OFF) oracles_session_hotkey_keys(s, 1);
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
    const int from_home = o->sdl.window != NULL;
    /* The swim's hold, a setting of every game: one the home screen started where it is not qualified plays without
     * it; --rom refuses it, as the other options. */
    if (s->swim_hold && !oracles_compat_swim_hold(s->profile)) {
        if (!from_home) return failed(result, "%s does not support --swim-hold", s->profile ? oracles_compat_name(s->profile) : "this image");
        fprintf(stderr, "oracles: the swim's hold is not qualified for %s; this game plays without it\n", s->profile ? oracles_compat_name(s->profile) : "this image");
        s->swim_hold = 0;
    }
    /* The instant text, a setting of every game: the same. */
    if (s->text_instant && !oracles_compat_text_instant(s->profile)) {
        if (!from_home) return failed(result, "%s does not support --text-speed instant", s->profile ? oracles_compat_name(s->profile) : "this image");
        fprintf(stderr, "oracles: the instant text is not qualified for %s; this game plays with the classic text\n", s->profile ? oracles_compat_name(s->profile) : "this image");
        s->text_instant = 0;
    }
    /* The cheats, a setting of each Oracle: the same, a fan game playing without them. */
    if (s->cheats && !oracles_compat_cheats(s->profile)) {
        if (!from_home) return failed(result, "%s does not support --cheats", s->profile ? oracles_compat_name(s->profile) : "this image");
        fprintf(stderr, "oracles: the cheats are not qualified for %s; this game plays without them\n", s->profile ? oracles_compat_name(s->profile) : "this image");
        s->cheats = 0;
    }
    /* The compact text changes what the core draws: never in Faithful, where the core's image is shown as it is. */
    if (s->compact_text && (!oracles_compat_compact_text(s->profile) || !s->enhanced)) {
        if (!from_home) return failed(result, "--text compact plays in Enhanced alone, on Ages, Seasons and the fan games made on them");
        fprintf(stderr, "oracles: the compact text is not qualified for %s; this game plays with the classic text\n", s->profile ? oracles_compat_name(s->profile) : "this image");
        s->compact_text = 0;
    }
    /* The wide text box, which the host draws: never in Faithful either, nor where the host's box is not qualified (the
     * fan games, which keep the game's box). */
    if (s->text_box_wide && (!oracles_compat_host_text_box(s->profile) || !s->enhanced)) {
        if (!from_home) return failed(result, "--text-box wide plays in Enhanced alone, on Ages and Seasons");
        fprintf(stderr, "oracles: the wide text box is not qualified for %s; this game plays with the game's box\n", s->profile ? oracles_compat_name(s->profile) : "this image");
        s->text_box_wide = 0;
    }
    if (!s->profile) return 1;   /* no hooks at all: attach says so when they are asked for */
    const char *name = oracles_compat_name(s->profile);
    if (s->continuous_transitions && !oracles_compat_continuous_transitions(s->profile))
        return failed(result, "%s does not support --continuous-transitions", name);
    if (s->continuous_swim && !oracles_compat_continuous_swim(s->profile)) {
        if (!from_home) return failed(result, "%s does not support --continuous-swim", name);
        fprintf(stderr, "oracles: %s does not support the continuous transitions when swimming; they stay on foot\n", name);
        s->continuous_swim = 0;
    }
    if (s->continuous_diagonal && !oracles_compat_continuous_diagonal(s->profile)) {
        if (!from_home) return failed(result, "%s does not support --continuous-diagonal", name);
        fprintf(stderr, "oracles: %s does not support the continuous transitions' diagonal; they stay along their axis\n", name);
        s->continuous_diagonal = 0;
    }
    if (s->continuous_steps && !oracles_compat_continuous_steps(s->profile)) {
        if (!from_home) return failed(result, "%s does not support --continuous-steps", name);
        fprintf(stderr, "oracles: %s does not support the continuous transitions' steps; Link walks through them a pixel a frame\n", name);
        s->continuous_steps = 0;
    }
    if (s->continuous_line && (!s->continuous_steps || !oracles_compat_continuous_line(s->profile))) {
        if (!from_home) return failed(result, "%s does not support --continuous-line", name);
        fprintf(stderr, "oracles: %s does not support the transitions at the line; they start where the game starts them\n", name);
        s->continuous_line = 0;
    }
    if (s->zoom_out && !oracles_compat_zoom_out(s->profile)) {
        if (!from_home) return failed(result, "%s does not support a view drawn back (--view medium or far, --zoom-out) yet", name);
        fprintf(stderr, "oracles: %s does not support the drawn-back view yet; the near view plays instead\n", name);
        s->zoom_out = 0;
        s->view_level = ORACLES_ENHANCED_NEAR;
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
    if (o->diagnostics || o->native_renderer || s->enhanced || s->hotkeys_mode != ORACLES_HOTKEYS_OFF || route_exchanges || s->mod || s->language
        || s->swim_hold || s->swim_route || s->cheats || s->cheats_route || s->compact_text || s->text_box_wide || s->text_instant || s->text_speed_route) {
        s->guest = oracles_guest_attach(s->core, s->profile);
        const int for_hotkeys_alone = !o->diagnostics && !o->native_renderer && !s->enhanced && !route_exchanges && !s->mod && !s->language && !s->swim_hold
                                   && !s->swim_route && !s->cheats && !s->cheats_route && !s->compact_text && !s->text_box_wide && !s->text_instant
                                   && !s->text_speed_route;
        if (!s->guest && from_home && for_hotkeys_alone) drop_hotkeys(s, result, "the hooks cannot be armed in this ROM");
        else if (!s->guest) return failed(result, "cannot arm the hooks for this game");
    }
    /* A reminder over the game waits for its status bar, read without hooks where no guest is attached. */
    if (!s->guest && s->sdl.pause_menu && oracles_pause_reminder_pending(s->pause)) s->bar_reader = oracles_guest_reader(s->core, s->profile);
    if (o->diagnostics) s->diag = oracles_diagnostics_start(s->guest, stderr);
    if (o->native_renderer) {
        s->check = oracles_frame_check_start(s->guest, s->core, NULL, 0);
        fprintf(stderr, "oracles: native renderer on screen; the core's frame is compared with it every vblank\n");
    }
    /* The gameplay options that change the guest state are part of a savestate's identity, like the mods; the swim
     * is not: a state saved with it is a state of the game without it too (the policy gives a doubled step back to the
     * game when it may not go on), and the states taken with the transitions before it load as they did. */
    snprintf(s->state_mods, sizeof s->state_mods, "%s%s%s%s%s", s->continuous_transitions ? ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS : "",
             s->continuous_transitions && s->mod ? "," : "", oracles_mod_session_identity(s->mod),
             s->language && (s->continuous_transitions || s->mod) ? "," : "", oracles_language_session_identity(s->language));
    s->state_info.mods = s->state_mods;
    char error[256];
    if (s->mod && oracles_mod_session_attach(s->mod, s->guest, error, sizeof error) != 0) return failed(result, "%s", error);
    if (s->language && oracles_language_session_attach(s->language, s->guest, error, sizeof error) != 0) return failed(result, "%s", error);
    if (s->swim_hold || s->swim_route) {   /* the tests armed for a replayed route that turns it on later too, answering once on */
        if (oracles_guest_set_swim_hold(s->guest, 1) != 0 || oracles_guest_set_swim_hold(s->guest, s->swim_hold) != 0)
            return failed(result, "the swim's hold cannot find its strokes in this ROM");
        if (s->swim_hold) fprintf(stderr, "oracles: swim's hold on: a direction held swims as the best tapping does\n");
    }
    if (s->text_instant || s->text_speed_route) {   /* the points armed for a replayed route that turns it on later too, as the swim's hold */
        if (oracles_guest_set_text_instant(s->guest, 1) != 0 || oracles_guest_set_text_instant(s->guest, s->text_instant) != 0)
            return failed(result, "the instant text cannot find its points in this ROM");
        if (s->text_instant) fprintf(stderr, "oracles: text speed instant: each line of the text box whole\n");
    }
    if (s->cheats || s->cheats_route) {   /* the points of those a replayed route turns on later armed too, answering once on */
        char names[64];
        if (oracles_guest_set_cheats(s->guest, s->cheats | s->cheats_route) != 0 || oracles_guest_set_cheats(s->guest, s->cheats) != 0)
            return failed(result, "the cheats cannot find their points in this ROM");
        if (s->cheats) fprintf(stderr, "oracles: cheats on: %s\n", oracles_cheats_format(s->cheats, NULL, names, sizeof names));
        s->cheats_used = s->cheats;
    }
    if (s->text_box_wide) {   /* after the language: the font's widths are read in the image it composed */
        if (oracles_guest_arm_wide_text_box(s->guest, ORACLES_TEXT_BOX_WIDE_PIXELS, !s->compact_text) != 0)
            return failed(result, "the wide text box cannot find its points in this ROM");
        unsigned pages = 0;   /* the pages laid out at its width; the others, the game's, centred */
        const uint32_t *laid = oracles_language_session_laid_pages(s->language, &pages);
        if (oracles_guest_set_text_box_pages(s->guest, laid, pages) != 0) return failed(result, "out of memory");
        fprintf(stderr, "oracles: wide text box on: %u pixels of text a line, the %s text\n", ORACLES_TEXT_BOX_WIDE_PIXELS, s->compact_text ? "compact" : "classic");
    } else if (s->compact_text) {
        if (oracles_guest_arm_compact_text(s->guest) != 0) return failed(result, "the compact text cannot find its points in this ROM");
        fprintf(stderr, "oracles: compact text on: the text box's lines in the font's own widths\n");
    }
    if (s->enhanced) {
        s->view = oracles_enhanced_view_start(s->core, s->guest, s->rom, s->rom_size);
        if (!s->view) return failed(result, "cannot start the Enhanced view");
        oracles_enhanced_view_set_size(s->view, oracles_session_view_size(o, s->view_level));
        if (s->view_level == ORACLES_ENHANCED_FAR) {   /* the far view's size, and why */
            char far[192];
            if (o->screen_w) oracles_enhanced_far_fitting_words(o->screen_w, o->screen_h, far, sizeof far);
            else snprintf(far, sizeof far, "the far view at %ux%u, %s", oracles_enhanced_view_width(s->view), oracles_enhanced_view_height(s->view),
                          s->settings->far_fixed ? "far=fixed in the settings" : o->aspect || s->settings->aspect ? "the shape --aspect or aspect= names"
                                                                                                  : "no screen to fit it to");
            fprintf(stderr, "oracles: %s\n", far);
        }
        const int camera = o->camera_profile ? o->camera_profile : s->settings->camera;
        oracles_enhanced_view_set_camera_profile(s->view, (unsigned)camera);
        /* Display's Camera: the platform's default at the scale the screen shows the view at, which the first frame
         * knows (oracles_session_follow_subpixel), else the player's choice; the pause changes it at once. */
        oracles_enhanced_view_set_subpixel(s->view, oracles_settings_subpixel_camera(s->settings, 0));
        s->subpixel_followed = !o->no_window;
        if (!s->subpixel_followed)   /* in a window, the first frame says it, with the scale */
            fprintf(stderr, "oracles: camera %s (%s)\n", oracles_enhanced_view_subpixel_on(s->view) ? "smooth" : "pixel",
                    s->settings->subpixel_camera >= 0 ? "subpixel_camera= in the settings" : "the platform's default");
        oracles_enhanced_view_set_link_phase(s->view, s->settings->link_phase);
        if (o->neighbour_objects) oracles_enhanced_view_set_neighbour_objects(s->view, 1);
        const int frozen = oracles_settings_quality_of(s->view_level, (int)oracles_core_kind(s->core), s->ghosts) == ORACLES_QUALITY_LOW;
        oracles_enhanced_view_set_neighbour_animation(s->view, !frozen);   /* the Low quality leaves them in their captured poses */
        oracles_enhanced_view_set_unvisited_edge(s->view, !frozen);        /* and the rooms never visited cut straight */
        oracles_enhanced_view_set_hud(s->view, s->settings->hud);
        /* The rooms never entered: the command line's, else the settings'; presentation alone. */
        oracles_enhanced_view_set_unvisited_hidden(s->view, o->unvisited_hidden ? o->unvisited_hidden == 2 : s->settings->unvisited_hidden);
        if (oracles_enhanced_view_unvisited_hidden(s->view))
            fprintf(stderr, "oracles: unvisited rooms hidden: the rooms around Link that he has never entered stay dark (%s)\n",
                    o->unvisited_hidden ? "--unvisited-rooms" : "unvisited_rooms= in the settings");
        if (s->text_box_wide) oracles_enhanced_view_set_text_box(s->view, ORACLES_TEXT_BOX_WIDE_PIXELS, ORACLES_TEXT_BOX_GAME_LINES);
        /* Text size: the command line's, else the settings'; presentation alone, which the pause changes at once.  A
         * fan game keeps the game's box at its size, as it keeps the box standard. */
        const int text_size = o->text_size ? o->text_size - 1 : s->settings->text_size;
        oracles_enhanced_view_set_text_size(s->view, (unsigned)text_size);
        if (text_size && !oracles_enhanced_view_text_size(s->view))
            fprintf(stderr, "oracles: text size is not qualified for %s; this game keeps the game's box at its size\n",
                    s->profile ? oracles_compat_name(s->profile) : "this image");
        /* The ghosts: the command line's, else the settings'; auto, two where four cores leave one to each besides the
         * game's own threads (the sound, the display). */
        const int ghosts = s->ghosts;
        const int cores = SDL_GetNumLogicalCPUCores();
        const int wanted = ghosts == 1 || ghosts == 2 ? ghosts : cores >= 4 ? 2 : 1;
        fprintf(stderr, "oracles: neighbour ghosts: %d (%s, %d processor cores)\n", oracles_enhanced_view_set_ghosts(s->view, (unsigned)wanted),
                ghosts == 1 || ghosts == 2 ? "chosen" : "auto", cores);
        fprintf(stderr, "oracles: Enhanced profile on screen (%ux%u): the wide world band, neighbours from the ghost instance with their objects%s, the framed fallback; F3 switches to the framed core; camera profile %d\n",
                oracles_enhanced_view_width(s->view), oracles_enhanced_view_height(s->view), !o->neighbour_objects ? " off" : frozen ? " frozen (Low quality)" : " animated in place", camera);
    }
    if (s->continuous_transitions && s->guest) {
        s->transition = oracles_room_transition_start(s->guest, s->rom, s->rom_size, s->continuous_swim);
        oracles_room_transition_set_diagonal(s->transition, s->continuous_diagonal);
        oracles_room_transition_set_steps(s->transition, s->continuous_steps);
        if (s->continuous_line && !oracles_room_transition_set_line(s->transition, 1)) {
            if (!from_home) return failed(result, "--continuous-line cannot find the screen's edge tests in this ROM");
            fprintf(stderr, "oracles: the transitions at the line cannot find the screen's edge tests in this ROM; they start where the game starts them\n");
            s->continuous_line = 0;
        }
        if (s->transition) fprintf(stderr, "oracles: continuous transitions on%s: Link keeps walking through scrolling transitions; the game's state differs from a native session's\n", s->continuous_swim ? ", swimming too" : "");
        else fprintf(stderr, "oracles: continuous transitions need the hooks of a recognised game; the native transitions stay\n");
    }
    if (!s->transition) { free(s->rom); s->rom = NULL; }   /* the policy reads the animation data from the ROM */
    if (s->guest && (s->hotkeys_mode != ORACLES_HOTKEYS_OFF || route_exchanges)) {
        s->hotkeys = oracles_hotkeys_session_start(s->guest, s->info.game, s->hotkeys_mode, s->settings, o->play_path ? &s->play : NULL);
        /* A full table of hooks is not the ROM's doing: say which. */
        const char *why = oracles_guest_hooks_refused(s->guest) ? "the item hotkeys could not be hooked: the guest's table of hooks is full"
                                                                 : "the item hotkeys cannot find the inventory's write point in this ROM";
        if (!s->hotkeys && from_home && !route_exchanges) drop_hotkeys(s, result, why);
        else if (!s->hotkeys) return failed(result, "%s; --item-hotkeys=off to play without them", why);
        if (s->hotkeys && s->recording) oracles_hotkeys_session_record(s->hotkeys, &s->record);
        if (s->hotkeys_mode != ORACLES_HOTKEYS_OFF)
            fprintf(stderr, "oracles: item hotkeys on, mode %s: keys %s %s %s %s; held with %s or %s a key takes the item of B or of A, in the inventory the item under the cursor\n",
                    o->hotkeys_option, s->settings->hotkey_key_names[0], s->settings->hotkey_key_names[1], s->settings->hotkey_key_names[2],
                    s->settings->hotkey_key_names[3], s->settings->hotkey_bind_b, s->settings->hotkey_bind_a);
    }
    if (s->guest && oracles_guest_hooks_refused(s->guest))
        fprintf(stderr, "oracles: WARNING: the guest's table of hooks is full: %u hook(s) not installed (%u installed); a feature asked for may be missing its points\n",
                oracles_guest_hooks_refused(s->guest), oracles_guest_hook_count(s->guest));
    if (s->view) {
        char degraded[256];
        if (oracles_enhanced_view_degraded(s->view, degraded, sizeof degraded)) fprintf(stderr, "oracles: WARNING: the wide view runs without some of its hooks: %s\n", degraded);
    }
    if (s->guest && s->recording) {
        /* What the session was, frame by frame: `oracles-harness --compare` sets it against the replay of the route. */
        char path[PATH_MAX_LENGTH + 16];
        snprintf(path, sizeof path, "%s.session.tsv", o->record_path);
        s->fingerprints = fopen(path, "wb");
    }
    return 1;
}

/* The host's frame_subpixel: the Enhanced sub-pixel camera's move of the frame just composed. */
static int view_subpixel(void *opaque, oracles_host_video_subpixel *out)
{
    memset(out, 0, sizeof *out);
    if (!oracles_enhanced_view_subpixel(opaque, &out->x, &out->y, &out->band_top, &out->band_height, &out->layer, &out->layer_x, &out->layer_y,
                                        &out->layer_w, &out->layer_h, &out->layer_fx, &out->layer_fy)) return 0;
    out->layer_pitch = ORACLES_ENHANCED_SUBPIXEL_LAYER_PITCH;
    out->patch = oracles_enhanced_view_subpixel_patch(opaque);
    const uint32_t (*fixed)[4] = NULL;
    out->fixed_count = oracles_enhanced_view_subpixel_fixed(opaque, &fixed);
    if (out->fixed_count > ORACLES_HOST_SUBPIXEL_FIXED) out->fixed_count = ORACLES_HOST_SUBPIXEL_FIXED;
    if (out->fixed_count) memcpy(out->fixed, fixed, out->fixed_count * sizeof out->fixed[0]);
    return 1;
}

/* The host's frame_crop: the framed core with the large menus. */
static int menus_crop(void *opaque, uint32_t rect[4])
{
    const session *s = opaque;
    return s->menus_large && oracles_enhanced_view_framed_crop(s->view, rect);
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
    /* The item hotkeys' filter for a game that has them, or whose pause may turn them on. */
    if (s->hotkeys || s->hotkeys_game >= 0) { config->input_filter = input_filter; config->input_filter_opaque = s; }
    /* A game whose pause may change its settings, or whose replayed route does, has the frames' callbacks before it has
     * a guest. */
    if (s->guest || s->sdl.pause_menu || s->play.setting_count) { config->on_frame_begin = frame_begin; config->on_frame_end = frame_end; config->frame_opaque = s; }
    if (s->check) { config->frame_source = native_frame; config->frame_source_opaque = s->check; }
    if (s->sdl.pause_menu) {
        config->on_started = oracles_session_started;
        config->on_pause = oracles_session_pause;
        config->display_paces_now = oracles_session_display_paces_now;
        config->pause_opaque = s;
    }
    if (s->view) {
        config->frame_source = oracles_enhanced_view_frame_source;
        config->frame_source_opaque = s->view;
        /* The game's menus enlarged: the framed core shown alone, at its own whole scale, while the pause leaves them so. */
        s->menus_large = s->settings->menus_large;
        config->frame_crop = menus_crop;
        config->frame_crop_opaque = s;
        config->frame_subpixel = view_subpixel;   /* none while the camera keeps whole pixels */
        config->frame_subpixel_opaque = s->view;
        oracles_enhanced_view_set_large_menus(s->view, s->menus_large);
        /* Text size: the text box shown again larger by the presenter, in whole screen pixels. */
        config->frame_box = oracles_enhanced_view_text_box_enlarged;
        config->frame_box_opaque = s->view;
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

/* The frames late as the slow game's suggestion counts them (OraclesSessionResult). */
int oracles_session_view_4_3(const OraclesSessionOptions *o, const oracles_settings *settings, struct SDL_Window *window)
{
    const int aspect = o->aspect ? o->aspect - 1 : settings->aspect;
    return aspect == ORACLES_ASPECT_4_3 ? 1 : aspect == ORACLES_ASPECT_16_9 ? 0 : oracles_sdl_screen_4_3(window);
}

int oracles_session_run(const OraclesSessionOptions *o, oracles_settings *settings, OraclesSessionResult *result)
{
    memset(result, 0, sizeof *result);
    session *s = calloc(1, sizeof *s);
    if (!s) { snprintf(result->error, sizeof result->error, "out of memory"); return 1; }
    s->settings = settings;
    s->pause = o->pause;
    s->screen_margin_option = o->screen_margin;
    s->text_size_option = o->text_size;
    s->unvisited_option = o->unvisited_hidden;
    /* --colour-correction and --vsync hold for this session and are never stored, as camera_profile. */
    s->colour_setting = settings->colour_correction;
    s->colour_applied = o->colour_option ? !strcmp(o->colour_option, "on") : settings->colour_correction;
    s->vsync = o->vsync_option ? o->vsync_option : settings->vsync;
    s->vsync_setting = !strcmp(settings->vsync, "on") ? 1 : !strcmp(settings->vsync, "off") ? 2 : 0;
    s->ghosts = o->ghosts ? o->ghosts - 1 : settings->ghosts;
    s->record_path = o->record_path;
    s->enhanced = o->enhanced || o->zoom_out || o->view || o->continuous_transitions || o->continuous_swim || o->continuous_diagonal || o->continuous_steps || o->continuous_line;
    s->continuous_transitions = o->continuous_transitions || o->continuous_swim || o->continuous_diagonal || o->continuous_steps || o->continuous_line;
    s->continuous_swim = o->continuous_swim;
    s->continuous_diagonal = o->continuous_diagonal;
    s->continuous_steps = o->continuous_steps || o->continuous_line;
    s->continuous_line = o->continuous_line;
    s->swim_hold = o->swim_hold;
    s->text_instant = o->text_instant == 2;
    s->text_speed_setting = -1;
    s->cheats = o->cheats;
    s->cheats_game = -1;
    s->swim_setting = s->hotkeys_game = s->window_setting = s->window_line = -1;
    s->compact_text = o->compact_text == 2;
    s->text_box_wide = o->text_box_wide == 2;
    /* The view's level (--view, Display's View; --zoom-out is far, --enhanced alone near); its size is the options'
     * shape, the screen's, a window taking the surface's, and far fitted to the screen (docs/REFERENCE.md). */
    s->view_level = o->view ? o->view - 1 : o->zoom_out ? ORACLES_ENHANCED_FAR : ORACLES_ENHANCED_NEAR;
    s->zoom_out = s->view_level != ORACLES_ENHANCED_NEAR;
    s->hotkeys_mode = oracles_settings_hotkeys_mode(o->hotkeys_option);   /* for this run alone; the slots and the keys are remembered */
    /* The profile's refusals come before the recording opens: a refused session writes no route. */
    char why[1024];
    int ok = load_game(s, o, result) && (!o->play_path || oracles_session_replay_route(s, o, why, sizeof why) || failed(result, "%s", why))
          && check_profile(s, o, result) && (!o->record_path || oracles_session_record_route(s, o, why, sizeof why) || failed(result, "%s", why));
    /* The pause's Cheats change the cheats of an Oracle the home screen started, from the next frame; Controls its
     * swim's hold and item hotkeys, where its profile has them; Display, or a replayed route's `set window` lines, the
     * window of any game that has one. */
    const int settings_game = s->info.game == ORACLES_GAME_SEASONS ? ORACLES_SETTINGS_SEASONS : ORACLES_SETTINGS_AGES;
    if (ok && s->pause && oracles_compat_cheats(s->profile)) s->cheats_game = settings_game;
    if (ok && s->pause && oracles_compat_swim_hold(s->profile)) s->swim_setting = settings->swim_hold;
    if (ok && s->pause && oracles_compat_text_instant(s->profile)) s->text_speed_setting = settings->text_instant;
    if (ok && s->pause && oracles_compat_item_hotkeys(s->profile) && s->info.game != ORACLES_GAME_UNKNOWN) {
        s->hotkeys_game = settings_game;
        s->hotkeys_setting = (int)settings->item_hotkeys[settings_game];
    }
    if (ok && !o->no_window && !oracles_sdl_fullscreen_only()) s->window_setting = settings->window_scale;
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
        oracles_session_report_frames(s, &report);
        result->enhanced = s->enhanced;
        result->frames_played = report.frames_presented;
        result->frames_late = oracles_session_late_frames(&report);
        if (o->pretend_slow && s->enhanced) {
            if (result->frames_played < ORACLES_SLOW_FRAMES) result->frames_played = ORACLES_SLOW_FRAMES;
            result->frames_late = result->frames_played;
            fprintf(stderr, "oracles: --pretend-slow: the game counts as one that ran slowly\n");
        }
        if (!o->no_window) result->window_closed = oracles_sdl_backend_window_closed(&s->backend);
    }
    oracles_session_finish(s, o);
    result->route_written = s->route_written;
    free(s);
    return ok ? 0 : 1;
}
