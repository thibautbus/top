/* What the launcher remembers from one run to the next, in settings.txt under
 * the per-user settings directory: the display choices, the keys and the
 * controller buttons, the item hotkeys, and for the home screen
 * each game's ROM, item hotkeys' mode and active mods, Display's profile,
 * transitions and window, and the launcher window's size. */
#ifndef ORACLES_LAUNCHER_SETTINGS_H
#define ORACLES_LAUNCHER_SETTINGS_H

#include "backends.h"
#include "item_hotkeys.h"
#include "ui_controls_nav.h"

#define ORACLES_SETTINGS_NAME_LENGTH 32
#define ORACLES_SETTINGS_PATH_LENGTH 4096
/* The mods a game plays together (ORACLES_MOD_SET_MAX), and a mod's name: its folder's, a-z, 0-9, - and _. */
#define ORACLES_SETTINGS_MODS_MAX 8
#define ORACLES_SETTINGS_MOD_NAME 64
#define ORACLES_SETTINGS_MODS_LENGTH (ORACLES_SETTINGS_MODS_MAX * ORACLES_SETTINGS_MOD_NAME)

/* The games of the home screen's entries, in settings.txt's order. */
typedef enum OraclesSettingsGame {
    ORACLES_SETTINGS_AGES,
    ORACLES_SETTINGS_SEASONS,
    ORACLES_SETTINGS_GAMES
} OraclesSettingsGame;

/* The Enhanced view's shape: the screen's, nearer 4:3 or 16:9 (auto), or the one named whatever the screen. */
typedef enum OraclesAspect {
    ORACLES_ASPECT_AUTO,
    ORACLES_ASPECT_16_9,
    ORACLES_ASPECT_4_3
} OraclesAspect;
/* Their names in settings.txt and on the command line: "auto", "16:9", "4:3". */
extern const char *const oracles_settings_aspect_names[3];
/* The cores' names in settings.txt and on the command line, by OraclesCoreKind: "sameboy", "mgba". */
extern const char *const oracles_settings_core_names[2];

/* Display's Quality: a profile sets at once what weighs on the device, the view's level, the core and the neighbour
 * workers; Custom when the three match none.  Low: near, Fast, auto; Medium: medium, Fast, auto; High: far, Fast,
 * auto; Max: far, Accurate, two. */
typedef enum OraclesQuality {
    ORACLES_QUALITY_LOW,
    ORACLES_QUALITY_MEDIUM,
    ORACLES_QUALITY_HIGH,
    ORACLES_QUALITY_MAX,
    ORACLES_QUALITY_CUSTOM
} OraclesQuality;
#define ORACLES_QUALITIES 4   /* the profiles, Custom left out */
/* Their names in settings.txt: "low", "medium", "high", "max", "custom". */
extern const char *const oracles_settings_quality_names[5];



typedef struct oracles_settings {
    char path[ORACLES_SETTINGS_PATH_LENGTH];   /* empty when no settings directory is available */
    int colour_correction;        /* on by default */
    char vsync[8];                /* auto, on, off */
    int camera;                   /* the Enhanced camera profile: 1 or 2 */
    char key_names[ORACLES_BINDINGS][ORACLES_SETTINGS_NAME_LENGTH];
    char pad_names[4][ORACLES_SETTINGS_NAME_LENGTH];
    /* Item hotkeys: the key and the button of each slot, the two keys that bind from a button, and the slots of each
     * game (0 ages, 1 seasons).  With --rom the mode holds for the run that asks for it (deliberately:
     * a command line speaks for its own run); the home screen remembers each game's (item_hotkeys_<game>=off|use, Controls' Off or On; equip,
     * which only the command line offers, reads too). */
    char hotkey_key_names[ORACLES_HOTKEY_SLOTS][ORACLES_SETTINGS_NAME_LENGTH];
    char hotkey_pad_names[ORACLES_HOTKEY_SLOTS][ORACLES_SETTINGS_NAME_LENGTH];
    char hotkey_bind_b[ORACLES_SETTINGS_NAME_LENGTH], hotkey_bind_a[ORACLES_SETTINGS_NAME_LENGTH];
    OraclesHotkeySlot hotkeys[2][ORACLES_HOTKEY_SLOTS];
    /* rom_ages=, rom_seasons=: the ROM a game starts from, set by dropping it on the home screen or by hand; empty: none. */
    char rom[ORACLES_SETTINGS_GAMES][ORACLES_SETTINGS_PATH_LENGTH];
    /* patch_kinomi=, patch_moonrise=, patch_temple=: the BPS patch of each fan game, which plays from its Oracle's ROM
     * (Gifts of Kinomi and Moonrise Regalia from rom_ages, Temple of Seasons from rom_seasons): an Oracle's ROM is chosen
     * once, for the Oracle and for every game made on it; empty: none. */
    char patch[ORACLES_HOME_FAN_GAMES][ORACLES_SETTINGS_PATH_LENGTH];   /* by ORACLES_HOME_FAN_*, the key oracles_home_fan_games' */
    /* Display's profile and transitions, for every game the home screen starts: profile=faithful|
     * enhanced (enhanced by default: the view drawn back), transitions=off|on (on by default: they come with Enhanced). */
    OraclesProfile profile;
    int transitions;
    int view;                     /* view=near|medium|far, Display's View in Enhanced: an OraclesEnhancedLevel, far by default */
    int core;                     /* an OraclesCoreKind: core=sameboy|mgba, Display's Core; -1 until the player chooses one, the build's default
                                   * (ORACLES_DEFAULT_CORE) then playing, and the file holding no core= line */
    int menus_large;              /* menus=large|view: the game's menus, its map and its cutscenes, which the Enhanced view shows
                                   * framed, at their own largest whole scale (large) or the view's (view); large on Android */
    int ghosts;                   /* ghosts=auto|1|2: the ghosts that prepare the rooms around, each on its own thread; 0 (auto, the
                                   * default) two on a device of four processor threads or more, else one; Display's Neighbour
                                   * workers, in its Advanced rows */
    int aspect;                   /* an OraclesAspect: aspect=auto|16:9|4:3, the view's shape: the screen's (auto, the default), or the one named; not in Display */
    OraclesHotkeysMode item_hotkeys[ORACLES_SETTINGS_GAMES];   /* item_hotkeys_<game>=off|use (equip reads too) */
    /* mods_ages=, mods_seasons=: the mods the game's Mods page made active, their names in order and separated by
     * commas, eight at most; empty: none.  The Mods page's Play starts the game with them. */
    char mods[ORACLES_SETTINGS_GAMES][ORACLES_SETTINGS_MODS_LENGTH];
    int launcher_width, launcher_height;                      /* launcher_window=WxH: the home screen's window as the player left it */
    int window_scale;             /* window_scale=2|3|4|full, Display's window for the games the home screen starts: 2 to 4, 0 fullscreen (the default) */
    int quality_hint;             /* quality_hint=low|medium|high|max: the profile a game that ran slowly was last suggested to leave, -1 none:
                                   * the suggestion is made once a profile */
    int first_run;                /* no settings.txt was read: the first opening, which takes the device's profile */
} oracles_settings;

void oracles_settings_defaults(oracles_settings *settings);
void oracles_settings_load(oracles_settings *settings);
/* Writes every setting through a temporary file renamed over settings.txt: 1 when done; 0 leaves the file as it was. */
int oracles_settings_store(const oracles_settings *settings);
/* The core the games run on: the one chosen, else the build's default. */
int oracles_settings_core(const oracles_settings *settings);
/* The profile the view's level, the core the games run on and the neighbour workers make: Custom when none. */
OraclesQuality oracles_settings_quality(const oracles_settings *settings);
/* Sets the view's level, the core and the neighbour workers of `quality` (Custom changes nothing).  The core is
 * written only when it is not the build's default, the player having chosen none: a later build keeps its own. */
void oracles_settings_apply_quality(oracles_settings *settings, OraclesQuality quality);
/* The profile a first opening takes: Max on a desktop; where the window is always fullscreen (Android), High on a
 * device of 8 processor threads and 6 GB or more (`ram_mb` as SDL_GetSystemRAM gives it), Medium otherwise. */
OraclesQuality oracles_settings_default_quality(int fullscreen_only, int processor_threads, int ram_mb);
/* The profile just lighter than `quality`, the one a game that ran slowly is suggested: Custom for Low and Custom. */
OraclesQuality oracles_settings_lighter_quality(OraclesQuality quality);
/* The names of settings.txt: "faithful", "enhanced", and the games' "ages", "seasons". */
const char *oracles_settings_profile_name(OraclesProfile profile);
const char *oracles_settings_game_name(OraclesSettingsGame game);
/* off, use or equip, as the policy names them; NULL or anything else is off. */
OraclesHotkeysMode oracles_settings_hotkeys_mode(const char *text);

/* A mods_<game>= list: its names, in their order (at most `capacity`); whether it has `name`; and `name` made active
 * or not, the names kept in order: 0 when a ninth mod would be active, the list then unchanged. */
size_t oracles_settings_mod_names(const char *list, char names[][ORACLES_SETTINGS_MOD_NAME], size_t capacity);
int oracles_settings_mod_active(const char *list, const char *name);
int oracles_settings_mod_set(char list[ORACLES_SETTINGS_MODS_LENGTH], const char *name, int on);
/* The mods folder: mods beside settings.txt, one folder per mod; empty when there is no settings directory. */
void oracles_settings_mods_folder(const oracles_settings *settings, char *out, size_t capacity);

#endif
