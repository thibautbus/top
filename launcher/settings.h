/* What the launcher remembers from one run to the next, in settings.txt under
 * the per-user settings directory: the display choices, the keys and the
 * controller buttons, the item hotkeys, and for the home screen
 * each game's ROM, European ROM and language, item hotkeys' mode and active mods, Display's profile,
 * transitions and window, and the launcher window's size. */
#ifndef ORACLES_LAUNCHER_SETTINGS_H
#define ORACLES_LAUNCHER_SETTINGS_H

#include "backends.h"
#include "item_hotkeys.h"
#include "touch_controls.h"
#include "ui_controls_nav.h"

#include <stdint.h>

#define ORACLES_SETTINGS_NAME_LENGTH 32
#define ORACLES_SETTINGS_PATH_LENGTH 4096
/* The mods a game plays together (ORACLES_MOD_SET_MAX), and a mod's name: its folder's, a-z, 0-9, - and _. */
#define ORACLES_SETTINGS_MODS_MAX 8
#define ORACLES_SETTINGS_MOD_NAME 64
#define ORACLES_SETTINGS_MODS_LENGTH (ORACLES_SETTINGS_MODS_MAX * ORACLES_SETTINGS_MOD_NAME)
/* The unknown keys kept, at most this many bytes and lines (oracles_settings.unknown). */
#define ORACLES_SETTINGS_UNKNOWN_LENGTH 16384
#define ORACLES_SETTINGS_UNKNOWN_LINES 128

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
extern const char *const oracles_settings_hud_names[3];   /* hud=, by the value of oracles_settings.hud */
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
/* The languages' codes in settings.txt (language.h's), in the order of Cartridge's Language row: "en", "en-eu", "fr",
 * "de", "it", "es". */
#define ORACLES_SETTINGS_LANGUAGES 6
extern const char *const oracles_settings_language_names[ORACLES_SETTINGS_LANGUAGES];



typedef struct oracles_settings {
    char path[ORACLES_SETTINGS_PATH_LENGTH];   /* empty when no settings directory is available */
    char mods_path[ORACLES_SETTINGS_PATH_LENGTH];   /* the mods folder where it is not beside settings.txt (Android's, on the
                                                     * shared storage), else empty */
    int colour_correction;        /* on by default */
    char vsync[8];                /* auto, on, off */
    int camera;                   /* the Enhanced camera profile: 1 or 2 */
    int link_phase;               /* link_phase=1: Link drawn in phase on a diagonal walk (a trial, written only when on) */
    int subpixel_camera;          /* subpixel_camera=: the Enhanced camera to the 256th of a pixel (Camera's Smooth), 1 or 0 once
                                   * set either way, -1 absent: the platform's default (oracles_settings_subpixel_camera) */
    char key_names[ORACLES_BINDINGS][ORACLES_SETTINGS_NAME_LENGTH];
    char pad_names[4][ORACLES_SETTINGS_NAME_LENGTH];
    char menu_pad[ORACLES_SETTINGS_NAME_LENGTH];   /* menu_pad=: the controller's Escape in a game, Guide's too (leftstick by default; a
                                                    * trigger, lefttrigger or righttrigger, too; empty: Guide alone) */
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
    /* eu_rom_ages=, eu_rom_seasons=: the European ROM of each Oracle (Cartridge's European ROM), whose English and
     * official translations the game plays in; empty: none.  language_ages=, language_seasons=: the language each
     * Oracle plays in, an index of oracles_settings_language_names (en by default), which needs that ROM recognized. */
    char eu_rom[ORACLES_SETTINGS_GAMES][ORACLES_SETTINGS_PATH_LENGTH];
    int language[ORACLES_SETTINGS_GAMES];
    /* language_kinomi=, language_moonrise=, language_temple=: the language each fan game plays in, its texts that are its
     * Oracle's in a language of its Oracle's European ROM (eu_rom_ages or eu_rom_seasons), its own in its
     * English; en by default.  Read and written back as they are, and applied only with the home screen's fan game
     * languages on (OraclesHomeGames), which the launcher leaves off: from the home screen, a fan game starts in English. */
    int fan_language[ORACLES_HOME_FAN_GAMES];
    /* Display's profile and transitions, for every game the home screen starts: profile=faithful|
     * enhanced (enhanced by default: the view drawn back), transitions=off|on (on by default: they come with Enhanced). */
    OraclesProfile profile;
    int transitions;
    int swim_hold;                /* swim_hold=off|on: swimming strokes from a direction held, where the game wants a button tapped (A
                                   * with the flippers, A held too; the direction with Ages' mermaid suit);
                                   * off by default, in either profile, for every game the home screen starts: Controls' Swimming,
                                   * Tap or Hold */
    int compact_text;             /* text=classic|compact: the text box's lines in the font's own widths, in Enhanced alone; classic by
                                   * default, for every game the home screen starts that qualifies it (Ages and Seasons) */
    int text_size;                /* text_size=normal|large|xlarge (0, 1, 2): the text box larger where the view holds it, three halves in
                                   * whole screen pixels (large) or twice as large (xlarge), the next size down elsewhere, in Enhanced;
                                   * normal by default; it applies at once, from the pause too */
    int text_instant;             /* text_speed=classic|instant: the text box's lines letter by letter at the file screen's speed, or each
                                   * whole, as a press of A at its start shows it; classic by default, in either profile, for every game
                                   * the home screen starts; it applies when the game resumes, from the pause too */
    int text_box_wide;            /* text_box=standard|wide: the game's box, or the host's wide box (192 pixels of text, the texts laid out
                                   * again for it, classic or compact), in Enhanced alone; standard by default, Ages and Seasons */
    int view;                     /* view=near|medium|far, Display's View in Enhanced: an OraclesEnhancedLevel, far by default */
    int core;                     /* an OraclesCoreKind: core=sameboy|mgba, Display's Core; -1 until the player chooses one, the build's default
                                   * (ORACLES_DEFAULT_CORE) then playing, and the file holding no core= line */
    int menus_large;              /* menus=large|view: the game's menus, its map and its cutscenes, which the Enhanced view shows
                                   * framed, at their own largest whole scale (large) or the view's (view); large on Android */
    int hud;                      /* hud=original|full|wide, 0 to 2 as OraclesEnhancedHud: the status bar in Enhanced as the game draws
                                   * it, dark beside it (original), full width, beside it its own background (full, the default), or laid
                                   * out on that background, hearts left and items right (wide); it applies at once, from the pause too */
    int ghosts;                   /* ghosts=auto|1|2: the ghosts that prepare the rooms around, each on its own thread; 0 (auto, the
                                   * default) two on a device of four processor threads or more, else one; Display's Neighbour
                                   * workers, in its Advanced rows */
    int aspect;                   /* an OraclesAspect: aspect=auto|16:9|4:3, the view's shape: the screen's (auto, the default), or the one named; not in Display */
    int unvisited_hidden;         /* unvisited_rooms=shown|hidden: the rooms around Link he has never entered drawn (shown, the default), or
                                   * kept dark as the game's map keeps them (hidden), in Enhanced; not in Display yet */
    int far_fixed;                /* far=screen|fixed: the far view fitted to the screen where its size fills it badly (screen, the default;
                                   * compositor.h, oracles_enhanced_far_fitting), or at its size whatever the screen (fixed); not in Display */
    OraclesHotkeysMode item_hotkeys[ORACLES_SETTINGS_GAMES];   /* item_hotkeys_<game>=off|use (equip reads too) */
    /* mods_ages=, mods_seasons=: the mods the game's Mods page made active, their names in order and separated by
     * commas, eight at most; empty: none.  The Mods page's Play starts the game with them. */
    char mods[ORACLES_SETTINGS_GAMES][ORACLES_SETTINGS_MODS_LENGTH];
    /* cheats_ages=, cheats_seasons=: the cheats each Oracle plays with, their names separated by commas (no-damage, ammo,
     * rupees), a mask of OraclesGuestCheat; empty, none, by default.  Not for the fan games made on it. */
    unsigned cheats[ORACLES_SETTINGS_GAMES];
    int launcher_width, launcher_height;                      /* launcher_window=WxH: the home screen's window as the player left it */
    int window_scale;             /* window_scale=2|3|4|full, Display's window for the games the home screen starts: 2 to 4, 0 fullscreen (the default) */
    int scaling_fill;             /* scaling=sharp|fill, Display's Scaling: the game in fullscreen at its largest whole scale (sharp, the
                                   * default), or filling the screen in its proportions (fill); a window keeps its whole scales */
    int screen_margin;            /* an OraclesScreenMargin: screen_margin=off|auto|small|large, the band of the screen the game, the
                                   * touch controls and the launcher's pages keep to (oracles_touch_band); off, the whole screen, by
                                   * default; a value the launcher does not know is off */
    int menu_toast_shown;         /* menu_toast=due|shown: the toast over the first game started with a controller, which names the
                                   * button that opens the menu, was shown (once, whatever the game); due by default */
    int update_check;             /* update_check=on|off: when the home screen opens, one request to GitHub for the latest release
                                   * (update_check.h); on by default, off asks nothing */
    int quality_hint;             /* quality_hint=low|medium|high|max|custom: the quality a game that ran slowly was last suggested to leave,
                                   * -1 none: the suggestion is made once a quality, until the view, the core or the workers change */
    int first_run;                /* no settings.txt was read: the first opening, which takes the device's profile */
    /* The lines of settings.txt whose key this version does not know (a later version's, a test build's), as they were,
     * each after the name of the known key read before it ("" at the file's start), "name\tline\n": written back after
     * that key, or at the file's end when it is not written. */
    char unknown[ORACLES_SETTINGS_UNKNOWN_LENGTH];
} oracles_settings;

void oracles_settings_defaults(oracles_settings *settings);
void oracles_settings_load(oracles_settings *settings);
/* Writes every setting through a temporary file renamed over settings.txt, the unknown keys read kept after the key they
 * followed: 1 when done; 0 leaves the file as it was. */
int oracles_settings_store(const oracles_settings *settings);
/* The core the games run on: the one chosen, else the build's default. */
int oracles_settings_core(const oracles_settings *settings);
/* The sub-pixel camera on (1) or off (0) for a game whose view the screen shows at the whole scale `scale` (Display ›
 * Advanced's Camera, Smooth or Pixel): the player's choice, else the platform's default. */
int oracles_settings_subpixel_camera(const oracles_settings *settings, unsigned scale);
/* That default, whatever the Quality: on on the desktops (Windows, macOS, Linux); where the game is only fullscreen
 * (Android), on from ORACLES_SETTINGS_SUBPIXEL_SCALE times on, off under it. */
#define ORACLES_SETTINGS_SUBPIXEL_SCALE 3u
int oracles_settings_default_subpixel_camera(int fullscreen_only, unsigned scale);
/* The whole scale from which this platform's default is on: 0 on the desktops (every scale), ORACLES_SETTINGS_SUBPIXEL_SCALE
 * on Android. */
unsigned oracles_settings_subpixel_default_from(void);
/* The profile the view's level, the core the games run on and the neighbour workers make: Custom when none. */
OraclesQuality oracles_settings_quality(const oracles_settings *settings);
/* The same for a session's own: its view's level (an OraclesEnhancedLevel), its core (an OraclesCoreKind) and its
 * workers (0 auto, 1, 2), the command line's over the settings'.  The Low quality leaves a neighbour's objects frozen. */
OraclesQuality oracles_settings_quality_of(int view, int core, int ghosts);
/* Sets the view's level, the core and the neighbour workers of `quality` (Custom changes nothing).  The core is
 * written only when it is not the build's default, the player having chosen none: a later build keeps its own. */
void oracles_settings_apply_quality(oracles_settings *settings, OraclesQuality quality);
/* The profile a first opening takes: Max on a desktop; where the window is always fullscreen (Android), High on a
 * device of 8 processor threads and 6 GB or more (`ram_mb` as SDL_GetSystemRAM gives it), Medium otherwise. */
OraclesQuality oracles_settings_default_quality(int fullscreen_only, int processor_threads, int ram_mb);
/* Whether vsync=auto presents in step with the display: on a desktop when the display refreshes at 59 to 61 Hz, close
 * enough to the Game Boy's 59.7275 Hz; never on Android (`android`), whose system composes the frames itself, where
 * presenting in step with the display only adds latency. */
int oracles_settings_auto_vsync(unsigned display_hz, int android);
/* The profile just lighter than `quality`, the one a game that ran slowly is suggested: Custom for Low and Custom. */
OraclesQuality oracles_settings_lighter_quality(OraclesQuality quality);
/* A game ran slowly when it played in Enhanced (`enhanced`) for a minute at least, ORACLES_SLOW_FRAMES of its `frames`,
 * with more than 2 % of them `late`: the home screen then suggests something lighter. */
#define ORACLES_SLOW_FRAMES 3584u   /* a minute at 59.7275 Hz */
int oracles_settings_ran_slowly(int enhanced, uint32_t frames, uint32_t late);
/* What a game that ran slowly suggests: 1 with *quality, the profile just lighter than the settings', or under Custom,
 * *quality Custom, with *view, the view's level just nearer than the settings'; 0 when nothing is lighter (Low, Custom
 * in the near view). */
int oracles_settings_lighter(const oracles_settings *settings, OraclesQuality *quality, int *view);
/* After a game of `frames` frames, `late` of them late: 1 when the home screen suggests something lighter, the game
 * having run slowly at a quality with something lighter, which quality_hint does not hold and then holds; 0 otherwise. */
int oracles_settings_slow_hint(oracles_settings *settings, int enhanced, uint32_t frames, uint32_t late);
/* Display's rows that make the quality were `view`, `core` (oracles_settings_core's) and `ghosts` before a change:
 * quality_hint is forgotten when they differ now, another quality picked or Custom changed by hand, for a game that runs
 * slowly to suggest again. */
void oracles_settings_quality_changed(oracles_settings *settings, int view, int core, int ghosts);
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
/* The mods folder, one folder per mod: mods_path when set, else mods beside settings.txt; empty when there is no
 * settings directory. */
void oracles_settings_mods_folder(const oracles_settings *settings, char *out, size_t capacity);
/* The language pack of game `g`, extracted from its European ROM at its first game in a language and read after:
 * language-ages.pack or language-seasons.pack beside settings.txt; empty when there is no settings directory. */
void oracles_settings_language_pack(const oracles_settings *settings, OraclesSettingsGame g, char *out, size_t capacity);
/* The language pack of fan game `f` (ORACLES_HOME_FAN_*), made at its first game in a language from its Oracle's:
 * language-kinomi.pack, language-moonrise.pack or language-temple.pack beside settings.txt; empty when there is no
 * settings directory. */
void oracles_settings_fan_language_pack(const oracles_settings *settings, unsigned f, char *out, size_t capacity);

#endif
