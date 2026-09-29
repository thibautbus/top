/* A game session: the user's ROM in the core, played until the player quits,
 * from `the-oracles-project --rom PATH` or from the home screen's Start game.
 *
 * The SRAM is read from and written to the save file (default: the ROM path
 * with a .sav extension, the convention of most emulators, so existing saves
 * work).  The end-of-session report goes to stderr. */
#ifndef ORACLES_LAUNCHER_SESSION_H
#define ORACLES_LAUNCHER_SESSION_H

#include "backends.h"
#include "settings.h"

#include <stddef.h>
#include <stdint.h>

struct OraclesPause;

typedef struct OraclesSessionOptions {
    const char *rom_path;
    const char *patch_path;          /* a BPS patch applied to rom_path in memory (a fan game), or NULL */
    const char *save_path;           /* NULL: next to the ROM, or to the patch when there is one */
    const char *screenshot;          /* PATH.ppm of the last frame, or NULL */
    const char *record_path, *play_path;
    const char *hotkeys_option;      /* off, use, equip, or NULL */
    const char *mods_dirs[8];        /* the mods' directories: --mods, repeated, at most ORACLES_MOD_SET_MAX */
    unsigned mods_count;
    const char *start_house;         /* --start-at-house: every file of the save starts in front of this mod's house, or NULL */
    /* The command line's --colour-correction (on, off) and --vsync (auto, on, off), or NULL for the settings': for
     * the session alone, never stored, as camera_profile. */
    const char *colour_option, *vsync_option;
    uint32_t frames;                 /* 0: no limit */
    int no_window, mute, diagnostics, native_renderer;
    int enhanced, zoom_out, continuous_transitions;
    int continuous_swim;             /* --continuous-swim: the transitions with Link swimming at the surface too (implies them) */
    int neighbour_objects;           /* Enhanced shows a neighbour's objects */
    int camera_profile;              /* 0: the settings' camera; 1 or 2: this one for the session, not stored */
    /* The window: its scale and fullscreen state, and the launcher's own window when the home screen lends it. */
    oracles_sdl_options sdl;
    /* A game the home screen started: Escape opens this pause menu (pause.h) rather than ending the session. */
    struct OraclesPause *pause;
} OraclesSessionOptions;

typedef struct OraclesSessionResult {
    int window_closed;               /* the window was closed, rather than the game left with Escape */
    char error[256];                 /* why the session could not run or stopped, empty otherwise */
    int route_written;               /* --record's route was written */
    int hotkeys_dropped;             /* a game the home screen started played without the item hotkeys: they could not attach */
} OraclesSessionResult;

/* Defaults: Faithful, the neighbours' objects shown in Enhanced, a window at scale 4. */
void oracles_session_defaults(OraclesSessionOptions *options);
/* Plays the session with the player's settings, the options overriding them for the session alone; F2 changes and
 * stores the colour correction, the pause's Display and Controls the settings. Returns 0, or 1 on failure. */
int oracles_session_run(const OraclesSessionOptions *options, oracles_settings *settings, OraclesSessionResult *result);

/* The save file of a ROM: its path with the extension .sav. */
void oracles_session_default_save_path(const char *rom_path, char *out, size_t capacity);
/* The save a session reads and writes: --save's, else the ROM's (a fan game's patch's), NAME.mods.sav with mods, a
 * modded game's own. */
void oracles_session_save_path(const OraclesSessionOptions *options, char *out, size_t capacity);
/* The mods the home screen's Mods page made active for game `g` (settings.txt's mods_<game>=), in the order of their
 * names, as --mods gives them: their directories in the mods folder, written into `dirs`, those no longer there
 * left out.  Returns their number. */
unsigned oracles_session_home_mods(OraclesSessionOptions *options, const oracles_settings *settings, OraclesSettingsGame g,
                                   char dirs[ORACLES_SETTINGS_MODS_MAX][ORACLES_SETTINGS_PATH_LENGTH]);

#endif
