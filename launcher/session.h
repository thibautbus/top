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
#include "state_display.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct OraclesPause;
struct OraclesRoomTransitionStats;
struct OraclesEnhancedSize;

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
    /* --language, the European ROM it comes from (a file or a directory) and its pack (read, or written once extracted):
     * for the session alone, never stored; NULL for none. */
    const char *language, *eu_rom_path, *language_pack;
    /* The command line's --colour-correction (on, off) and --vsync (auto, on, off), or NULL for the settings': for
     * the session alone, never stored, as camera_profile. */
    const char *colour_option, *vsync_option;
    uint32_t frames;                 /* 0: no limit */
    int no_window, mute, diagnostics, native_renderer;
    int enhanced, zoom_out, continuous_transitions;
    int view;                        /* the Enhanced view's level plus one (an OraclesEnhancedLevel; --view, Display's View); 0: far with zoom_out, else near */
    int screen_4_3;                  /* the view takes the 4:3 shape (oracles_session_view_4_3) */
    unsigned screen_w, screen_h;     /* the screen the far view fits to (oracles_session_far_screen); 0: far at its size */
    unsigned screen_margin_lines;    /* and the lines of it the screen margin keeps the game out of: far fits to the band left */
    int aspect;                      /* --aspect: an OraclesAspect plus one, for the run, never stored; 0: the settings' */
    int ghosts;                      /* --ghosts: auto, 1 or 2 as 1, 2 or 3, for the run, never stored; 0: the settings' ghosts= */
    int core;                        /* --core: an OraclesCoreKind plus one, for the run, never stored; 0: the settings' core= */
    int screen_margin;               /* --screen-margin: an OraclesScreenMargin plus one, for the run, never stored; 0: the settings' screen_margin= */
    int continuous_swim;             /* --continuous-swim: the transitions with Link swimming at the surface too (implies them) */
    int continuous_diagonal;         /* --continuous-diagonal: a diagonal held carries Link across the transitions' axis too (implies them) */
    int continuous_steps;            /* --continuous-steps: Link takes a step a frame through them as the keys give it (implies them) */
    int continuous_line;             /* --continuous-line: a scroll starts at the line between two rooms, and Link stops where the keys leave him (implies the steps) */
    int swim_hold;                   /* --swim-hold, or settings.txt's swim_hold=on for a game the home screen starts: a direction held
                                      * starts the swim's strokes (A's with the flippers, A held too; the direction's with Ages' mermaid suit) */
    unsigned cheats;                 /* --cheats, or settings.txt's cheats_<game>= for a game the home screen starts: a mask of OraclesGuestCheat */
    int cheats_given;                /* --cheats was given: its list wins over the settings' for the games the home screen starts */
    int compact_text;                /* --text: 1 classic, 2 compact (the text box's lines in the font's own widths, in Enhanced alone);
                                      * 0 for a game the home screen starts: settings.txt's text= */
    int text_size;                   /* --text-size: 1 normal, 2 large, 3 xlarge (the text box three halves or twice as large where the view holds
                                      * it); 0: settings.txt's text_size= */
    int text_instant;                /* --text-speed: 1 classic, 2 instant (each line of the text box whole, in either profile); 0 for a game
                                      * the home screen starts: settings.txt's text_speed= */
    int unvisited_hidden;            /* --unvisited-rooms: 1 shown, 2 hidden (the rooms never entered kept dark, in Enhanced), for the run;
                                      * 0: settings.txt's unvisited_rooms= */
    int text_box_wide;               /* --text-box: 1 standard, 2 wide (the host's box, 192 pixels of text, in Enhanced alone); 0 for a game
                                      * the home screen starts: settings.txt's text_box= */
    int neighbour_objects;           /* Enhanced shows a neighbour's objects */
    int camera_profile;              /* 0: the settings' camera; 1 or 2: this one for the session, not stored */
    int pretend_slow;                /* --pretend-slow: the game counts as one that ran slowly, to try the home screen's suggestion */
    /* The window: its scale and fullscreen state, and the launcher's own window when the home screen lends it. */
    oracles_sdl_options sdl;
    /* A game the home screen started: Escape opens this pause menu (pause.h) rather than ending the session. */
    struct OraclesPause *pause;
    /* A game the home screen started: the command line's options, which win over the settings, and whether its ROM plays
     * in Enhanced; a savestate the pause's Load state finds taken with other Display settings then ends the session for
     * the game to start again with them (OraclesSessionResult's state_restart).  NULL for --rom's. */
    const struct OraclesSessionOptions *command_line;
    int enhanced_allowed;
    /* The game started again so: its savestate loaded before the first frame, and `state_notice` shown over the game;
     * refused after all, Display's choices put back as `state_previous` holds them. */
    int load_state;
    const char *state_notice;
    const OraclesStateDisplay *state_previous;
} OraclesSessionOptions;

typedef struct OraclesSessionResult {
    int window_closed;               /* the window was closed, rather than the game left with Escape */
    char error[256];                 /* why the session could not run or stopped, empty otherwise */
    int route_written;               /* --record's route was written */
    int hotkeys_dropped;             /* a game the home screen started played without the item hotkeys: they could not attach */
    /* How the game ran, for the home screen to suggest something lighter (oracles_settings_slow_hint): in Enhanced or
     * not, the frames it played outside its pauses, and those late: the host's late frames, or under vsync, where the
     * host counts none, the frames whose work took over one frame past the first second, whichever are more. */
    int enhanced;
    uint32_t frames_played, frames_late;
    char language_note[256];         /* what the language's session says beside playing, such as a pack it could not write */
    /* The pause's Load state changed Display's settings to the savestate's (oracles_state_display_settings) and ended the
     * session: the game starts again with them and loads it, saying what changed. */
    int state_restart;
    char state_notice[192];
    OraclesStateDisplay state_previous;   /* the choices it changed, as they were */
    int state_unloaded;              /* a game started again for its savestate stopped before its first frame, which loads it */
} OraclesSessionResult;

/* Defaults: Faithful, the neighbours' objects shown in Enhanced, a window at scale 4. */
void oracles_session_defaults(OraclesSessionOptions *options);
/* Plays the session with the player's settings, the options overriding them for the session alone; F2 changes and
 * stores the colour correction, the pause's Display and Controls the settings. Returns 0, or 1 on failure. */
int oracles_session_run(const OraclesSessionOptions *options, oracles_settings *settings, OraclesSessionResult *result);
/* Whether the view takes the 4:3 shape: the command line's --aspect, else the settings' aspect=, auto deciding by the
 * screen of `window` (the primary display without one). */
int oracles_session_view_4_3(const OraclesSessionOptions *options, const oracles_settings *settings, struct SDL_Window *window);
/* The screen the far view fits to: that of `window` (the primary display without one) where the settings' far= is
 * screen and the view takes the screen's shape (no --aspect, aspect=auto); 0x0 otherwise.  `margin_lines`: the lines
 * of it the screen margin the game plays with keeps it out of, above and below together (oracles_touch_band). */
void oracles_session_far_screen(const OraclesSessionOptions *options, const oracles_settings *settings, struct SDL_Window *window,
                                unsigned *width, unsigned *height, unsigned *margin_lines);
/* Display's part of a game the home screen starts with `settings`, into `o`, the command line's options (which win):
 * the profile, Enhanced where the ROM plays it (`enhanced_allowed`), Faithful otherwise; the continuous transitions,
 * the compact text and the wide box (Enhanced alone); the view's level, its shape and far fitted to the screen of
 * `window`.  Returns the profile, *surface the view's (160x144 in Faithful). */
OraclesProfile oracles_session_display_options(OraclesSessionOptions *o, const oracles_settings *settings, int enhanced_allowed,
                                               struct SDL_Window *window, struct OraclesEnhancedSize *surface);
/* The surface of the view's level (an OraclesEnhancedLevel): the far view fitted to the band the options' screen
 * margin leaves of their screen (oracles_enhanced_far_fitting_band), else the level's size in the options' shape. */
struct OraclesEnhancedSize oracles_session_view_size(const OraclesSessionOptions *options, int level);

/* The screen margin the games play with, an OraclesScreenMargin: the command line's (`option`, --screen-margin's as
 * OraclesSessionOptions holds it, 0 for none), else the settings' screen_margin=. */
int oracles_session_screen_margin(int option, const oracles_settings *settings);
/* Whether a game presents in step with the display for vsync= `vsync` ("auto", "on" or "off") on a display of
 * `display_hz`: on, off, or auto's (oracles_settings_auto_vsync, never on Android). */
int oracles_session_vsync_on(const char *vsync, unsigned display_hz);

/* What a running game shows of the settings the pause applies while it runs: the colour correction as the settings
 * had it when the game last took it, Scaling's Fill, the HUD (OraclesEnhancedHud, -1 without the Enhanced view, which alone
 * draws it), the screen margin (an OraclesScreenMargin), the text box's size; and as the settings had them when the game last
 * took them, the swim's hold (-1 where the game's profile has none), the item hotkeys' mode (an OraclesHotkeysMode) of
 * the settings' game it plays (an OraclesSettingsGame, -1 for a game whose profile has none), the instant text, vsync= (0 auto,
 * 1 on, 2 off: what --vsync gave holds until the pause changes it), the large menus and the rooms never visited kept dark
 * (-1 without the Enhanced view).  The
 * window is not among
 * them: the pause applies it when its row is chosen (OraclesPauseSession's window_chosen). */
typedef struct OraclesSessionApplied {
    int colour, fill, hud, margin;
    int text_size;   /* Text size, 0 normal, 1 large, 2 xlarge (-1 without the Enhanced view, or for a fan game, which keeps the game's box) */
    int swim_hold, hotkeys_game, hotkeys;
    int text_instant;   /* the instant text (-1 where the game's profile has none), as the settings had it when the game last took it */
    int vsync;
    int menus_large;   /* menus=large (-1 without the Enhanced view) */
    int unvisited_hidden;   /* unvisited_rooms=hidden (-1 without the Enhanced view, or while --unvisited-rooms holds it for the run) */
} OraclesSessionApplied;
#define ORACLES_SESSION_CHANGED_COLOUR 1u
#define ORACLES_SESSION_CHANGED_FILL 2u
#define ORACLES_SESSION_CHANGED_HUD 4u
#define ORACLES_SESSION_CHANGED_MARGIN 8u
#define ORACLES_SESSION_CHANGED_SWIM 16u
#define ORACLES_SESSION_CHANGED_HOTKEYS 32u
#define ORACLES_SESSION_CHANGED_TEXT_SIZE 64u
#define ORACLES_SESSION_CHANGED_TEXT_SPEED 128u
#define ORACLES_SESSION_CHANGED_VSYNC 256u
#define ORACLES_SESSION_CHANGED_MENUS 512u
#define ORACLES_SESSION_CHANGED_UNVISITED 1024u
/* The settings after the pause's Display changed them, set against what the game shows: `applied` takes them, and the
 * ORACLES_SESSION_CHANGED_* bits say which changed.  The margin is --screen-margin's for the run when `margin_option`
 * gives one (oracles_session_screen_margin), and the text size --text-size's when `text_size_option` gives one (the
 * size plus one), whatever the settings say. */
unsigned oracles_session_settings_changed(const oracles_settings *settings, int margin_option, int text_size_option, OraclesSessionApplied *applied);
/* The end-of-session report's line on the continuous transitions, to `out`: what the policy took over and moved, and
 * the room entered's collisions predicted while the game loaded them, those not as the game then loaded them, and those
 * left to the load, by reason. */
void oracles_session_report_transitions(const struct OraclesRoomTransitionStats *stats, FILE *out);
/* The window a game has, as window_scale= counts it, F11's toggle included: -1 where `setting` (the window Display chose
 * for it) is; 0 fullscreen; else the setting when the window is the `setting_scale` it opens at (lowered to what the
 * display holds), else the scale of 2 to 4 the window's `width` x `height` pixels are exactly, else -2: none. */
int oracles_session_window_now(int setting, unsigned setting_scale, int fullscreen, int width, int height, int surface_w, int surface_h);
/* The windows (1 << Display's Window value) whose pause would keep only its own entries (oracles_ui_layout_narrow),
 * Display gone with them: `scales`, those 2x, 3x and 4x open at (lowered to what the display holds), of a surface of
 * surface_w x surface_h, under `aspect` (the settings' aspect=). */
unsigned oracles_session_windows_refused(const unsigned scales[3], int surface_w, int surface_h, int aspect);

/* The layout of the texts a session plays (language.h, OraclesLanguageLayout): the compact text where the profile has
 * it (`profile_compact`), the wide box where the profile has the host's text box (`profile_host_box`; elsewhere the
 * game's box plays, the wide box turned off), in Enhanced alone.  1 with the line's `width` and whether it is laid out
 * at the game's `fixed` advance; 0: the game's own layout. */
int oracles_session_text_layout(int profile_compact, int profile_host_box, int enhanced, int compact, int wide, unsigned *width, int *fixed);

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
