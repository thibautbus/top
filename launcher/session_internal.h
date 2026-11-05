/* The state session.c and session_state.c share: not a public interface, the
 * launcher sees session.h. */
#ifndef ORACLES_SESSION_INTERNAL_H
#define ORACLES_SESSION_INTERNAL_H

#include "session.h"

#include "pause.h"

#include "core.h"
#include "diagnostics.h"
#include "frame_check.h"
#include "room_transition.h"
#include "hotkeys_session.h"
#include "view.h"
#include "guest.h"
#include "rom.h"
#include "route.h"
#include "state.h"
#include "mod_session.h"
#include "language_session.h"

#include <stdio.h>

#define PATH_MAX_LENGTH 4096

typedef struct save_file {
    char path[PATH_MAX_LENGTH];
    char sha1[41];   /* of the SRAM loaded before the run, or "none" */
} save_file;

typedef struct session {
    OraclesCore *core;
    uint8_t *rom;            /* until the view and the policies have read what they keep */
    size_t rom_size;
    OraclesRomInfo info;
    const OraclesCompatProfile *profile;   /* NULL for an image the loader accepted by its header alone */
    save_file save;
    char route_sram_path[PATH_MAX_LENGTH], record_sram_path[PATH_MAX_LENGTH];
    int enhanced, continuous_transitions, continuous_swim, continuous_diagonal, continuous_steps, zoom_out;   /* zoom_out: the view draws back (medium or far) */
    int swim_hold;                            /* the swim's hold: the option's, as a replayed route or the profile may change it */
    int compact_text;                         /* the compact text: the option's, as a replayed route or the profile may change it */
    int text_box_wide;                        /* the host's wide text box: the same */
    int text_instant;                         /* the instant text: the option's, as a replayed route or the pause may change it */
    unsigned cheats;                          /* the cheats on (OraclesGuestCheat): the option's, as a replayed route or the profile may change them */
    /* The pause's Cheats: the settings' game whose cheats the game plays with (an OraclesSettingsGame), -1 when its
     * profile has none or no pause can change them; the cheats the next frame begins with, once the pause changed them;
     * the cheats a replayed route's `cheat` lines change; and every cheat that was on, for the session's report. */
    int cheats_game;
    unsigned cheats_next, cheats_route, cheats_used;
    int cheats_pending;
    /* The other settings the pause changes while the game runs (pause_settings_changed), each as the settings had it
     * when the game last took it, -1 when the pause cannot change the game's (OraclesSessionApplied): the swim's hold,
     * which the next frame begins with; the item hotkeys' mode of the settings' game the game plays (hotkeys_game, -1
     * for none), asked for at the next frame and taken once their policy is at rest; and the window, changed at once,
     * whose `set` line the next frame writes in a route recorded (window_line, -1 for none). */
    int swim_setting, swim_next, swim_pending;
    int hotkeys_game, hotkeys_setting, hotkeys_pending;
    OraclesHotkeysMode hotkeys_next;
    int window_setting, window_line;
    int swim_route;                           /* a replayed route's `set swim-hold` lines: the hold's tests armed from the start */
    int text_speed_setting, text_speed_next, text_speed_pending;   /* the pause's Text speed, as the swim's hold's (-1: none) */
    int text_speed_route;                     /* a replayed route's `set text-speed` lines: the instant text's points armed from the start */
    int view_level;                              /* the Enhanced view's level (OraclesEnhancedLevel), its size the options' (oracles_session_view_size) */   /* the options, as a replayed route or the profile may change them */
    int ghosts;                                  /* the neighbour ghosts chosen, --ghosts' else the settings': 0 auto, 1, 2 */
    OraclesHotkeysMode hotkeys_mode;
    oracles_sdl_options sdl;
    unsigned display_hz;
    oracles_host_backend backend;
    int backend_ready;
    int ran;                 /* the host ran: the end-of-session reports have something to say */
    uint32_t display_unpaced_frame;   /* under vsync, the frame from which the host paced; 0: the display paced */
    OraclesRoomTransition *transition;
    oracles_settings *settings;
    OraclesRoute play;
    int playing;
    OraclesRouteWriter record;
    int recording, route_written;
    const char *record_path;
    OraclesStateInfo state_info;
    char state_path[PATH_MAX_LENGTH];
    OraclesGuest *guest;
    OraclesDiagnostics *diag;
    OraclesFrameCheck *check;
    OraclesEnhancedView *view;
    OraclesHotkeysSession *hotkeys;
    OraclesModSession *mod;  /* --mods */
    OraclesLanguageSession *language;   /* --language */
    char state_mods[1100];   /* the savestate's identity of options and mods */
    FILE *fingerprints;      /* while recording with a guest: the session's live state frame by frame, to compare with the replay of its route */
    uint8_t host_state[1024];
    OraclesPause *pause;     /* a game the home screen started */
    int colour_applied;      /* the colour correction the core has: the option's, or the settings' */
    int colour_setting;      /* the settings' as the session last saw it: a change in the pause's Display applies */
    const char *vsync;       /* the vsync asked for: the option's, or the settings' */
    int vsync_setting;       /* the settings' vsync= as the session last saw it (0 auto, 1 on, 2 off): a change in the pause's Display applies */
    int paces_changed;       /* the pause turned vsync on or off: the host learns it at the resume (display_paces_now) */
    int menus_large;         /* the Enhanced view's menus shown alone at their own scale (menus=large), as the pause may change it */
    int screen_margin_option;   /* --screen-margin's, an OraclesScreenMargin plus one, which holds for the run; 0: the settings' */
    int text_size_option;       /* --text-size's, the size plus one, which holds for the run; 0: the settings' */
    int unvisited_option;       /* --unvisited-rooms', 1 shown, 2 hidden, which holds for the run; 0: the settings' */
    int subpixel_followed;      /* in a window: the view's sub-pixel camera follows the settings at the scale the screen shows it */
} session;

/* Writes a file through a temporary name (1 when done); reads a whole file into a buffer the caller frees (1, or 0
 * when it does not exist or cannot be read). */
int oracles_session_write_file(const char *path, const uint8_t *data, size_t size);
int oracles_session_read_file(const char *path, uint8_t **data, size_t *size);
void oracles_session_stop_recording(session *s, const char *why);
/* session_route.c: --play's route read and its options taken on, --record's opened; 1, or 0 with the reason in `error`. */
int oracles_session_replay_route(session *s, const OraclesSessionOptions *o, char *error, size_t capacity);
int oracles_session_record_route(session *s, const OraclesSessionOptions *o, char *error, size_t capacity);
/* session_live.c: the settings a running game takes from `frame` on, before it runs: the guest attached at the first
 * that needs it (1 when there is one); the cheats, the swim's hold, the instant text, the item hotkeys' mode asked and the one the policy
 * settled on; the surface a window is a whole multiple of, and the window Display chose (window_scale=: 2 to 4, 0
 * fullscreen) opened as Play opens it. */
int oracles_session_ensure_guest(session *s, uint32_t frame);
void oracles_session_change_cheats(session *s, uint32_t frame, unsigned cheats);
void oracles_session_change_swim_hold(session *s, uint32_t frame, int on);
void oracles_session_change_text_speed(session *s, uint32_t frame, int instant);
void oracles_session_hotkeys_settled(session *s, uint32_t frame, int settled);
void oracles_session_change_hotkeys(session *s, uint32_t frame, OraclesHotkeysMode mode);
OraclesEnhancedSize oracles_session_window_surface(const session *s);
void oracles_session_apply_window(session *s, int window);
/* Before each frame: the sub-pixel camera Display's Camera gives the view at the whole scale the screen shows it at now
 * (oracles_settings_subpixel_camera), switched without a jolt where that changed (the pause's Camera, Scaling, Window or
 * screen margin). */
void oracles_session_follow_subpixel(session *s);
/* session_pause.c: the item hotkeys' keys taken from the game or given back to it; the pause's renderer prepared once the
 * window is open; the pause menu run over the game, and the pacing its vsync leaves. */
void oracles_session_hotkey_keys(session *s, int on);
void oracles_session_started(void *opaque);
oracles_host_pause_result oracles_session_pause(void *opaque);
int oracles_session_display_paces_now(void *opaque);
/* session_report.c: the frames a run presented late; its report on the standard error; the session ended, its save
 * written, its screenshot taken and its resources freed. */
uint32_t oracles_session_late_frames(const oracles_host_run_report *report);
void oracles_session_report_frames(const session *s, const oracles_host_run_report *report);
void oracles_session_finish(session *s, const OraclesSessionOptions *o);
/* F5 and F7: 1 when done; otherwise 0, and *message says why, shortly (the pause menu shows it). */
int oracles_session_save_state(session *s, char *message, size_t capacity);
int oracles_session_load_state(session *s, char *message, size_t capacity);

#endif
