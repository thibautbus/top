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
    int enhanced, continuous_transitions, continuous_swim, zoom_out;   /* zoom_out: the view draws back (medium or far) */
    int view_level, view_4_3;                    /* the Enhanced view's level (OraclesEnhancedLevel) and its shape: 4:3, else 16:9 */   /* the options, as a replayed route or the profile may change them */
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
    char state_mods[1100];   /* the savestate's identity of options and mods */
    FILE *fingerprints;      /* while recording with a guest: the session's live state frame by frame, to compare with the replay of its route */
    uint8_t host_state[1024];
    OraclesPause *pause;     /* a game the home screen started */
    int colour_applied;      /* the colour correction the core has: the option's, or the settings' */
    int colour_setting;      /* the settings' as the session last saw it: a change in the pause's Display applies */
    const char *vsync;       /* the vsync asked for: the option's, or the settings' */
} session;

/* Writes a file through a temporary name (1 when done); reads a whole file into a buffer the caller frees (1, or 0
 * when it does not exist or cannot be read). */
int oracles_session_write_file(const char *path, const uint8_t *data, size_t size);
int oracles_session_read_file(const char *path, uint8_t **data, size_t *size);
void oracles_session_stop_recording(session *s, const char *why);
/* F5 and F7: 1 when done; otherwise 0, and *message says why, shortly (the pause menu shows it). */
int oracles_session_save_state(session *s, char *message, size_t capacity);
int oracles_session_load_state(session *s, char *message, size_t capacity);

#endif
