/* The launcher's state and navigation, without SDL: the home screen (the
 * games stacked on the left) and
 * the game's page and Display (screens 2 and 3, ui_page_nav.c and ui_page_display.c).
 *
 * Three entries: Oracle of Ages, Oracle of Seasons, Fan games.  The chosen
 * entry fills the right half with its title and its menu, the two others are
 * stacked on the left.  Left and right change the entry; up and down move in
 * the menu, which wraps (through the newer version's notice, when there is
 * one); OK activates the highlighted item, Back leaves the
 * list of fan games.  A disabled item is highlighted like the others and
 * shows its reason.  Fan games opens the list of fan games in the menu;
 * a fan game picked there (oracles_home_fan_games) has the menu of a game.  Cartridge opens
 * the game's page (its ROM, or a fan game's base ROM and patch; its save; an
 * Oracle's European ROM and language; Play), with the language beside it when
 * it is not English, Display the profile and the window's choices, the same for every
 * game, Controls the keys and buttons, Mods the mods of an Oracle (ui_page_nav.h),
 * greyed for a fan game, and Cheats an Oracle's cheats, which a fan game's menu and an unrecognised ROM's do not have. */
#ifndef ORACLES_UI_HOME_NAV_H
#define ORACLES_UI_HOME_NAV_H

#include "ui_layout.h"

#include <stddef.h>
#include <stdint.h>

typedef enum OraclesHomeEntry {
    ORACLES_HOME_AGES,
    ORACLES_HOME_SEASONS,
    ORACLES_HOME_FAN,
    ORACLES_HOME_ENTRIES
} OraclesHomeEntry;

/* What the right half shows: an entry, or a fan game picked in the list. */
typedef enum OraclesHomeHero {
    ORACLES_HOME_HERO_AGES,
    ORACLES_HOME_HERO_SEASONS,
    ORACLES_HOME_HERO_FAN,
    ORACLES_HOME_HERO_KINOMI,
    ORACLES_HOME_HERO_MOONRISE,
    ORACLES_HOME_HERO_TEMPLE,
    ORACLES_HOME_HEROES
} OraclesHomeHero;

typedef enum OraclesHomeItemId {
    ORACLES_HOME_ITEM_START,
    ORACLES_HOME_ITEM_GAME,
    ORACLES_HOME_ITEM_CONTROLS,
    ORACLES_HOME_ITEM_DISPLAY,
    ORACLES_HOME_ITEM_MODS,
    ORACLES_HOME_ITEM_CHEATS,
    ORACLES_HOME_ITEM_EXIT,
    ORACLES_HOME_ITEM_KINOMI,
    ORACLES_HOME_ITEM_MOONRISE,
    ORACLES_HOME_ITEM_TEMPLE,
    /* The pause menu's (screen 6). */
    ORACLES_HOME_ITEM_RESUME,
    ORACLES_HOME_ITEM_SAVE_STATE,
    ORACLES_HOME_ITEM_LOAD_STATE,
    ORACLES_HOME_ITEM_QUIT_GAME
} OraclesHomeItemId;

typedef enum OraclesHomeAction {
    ORACLES_HOME_UP,
    ORACLES_HOME_DOWN,
    ORACLES_HOME_LEFT,
    ORACLES_HOME_RIGHT,
    ORACLES_HOME_OK,
    ORACLES_HOME_BACK
} OraclesHomeAction;

typedef enum OraclesHomeCommand {
    ORACLES_HOME_STAY,
    ORACLES_HOME_START_AGES,
    ORACLES_HOME_START_SEASONS,
    ORACLES_HOME_START_KINOMI,
    ORACLES_HOME_START_MOONRISE,
    ORACLES_HOME_START_TEMPLE,
    ORACLES_HOME_EXIT,
    ORACLES_HOME_CHOOSE_ROM,     /* the game's page: the system's file dialog for the page's game (a fan game's base ROM) */
    ORACLES_HOME_CHOOSE_PATCH,   /* a fan game's page: the system's file dialog for its patch */
    ORACLES_HOME_CHOOSE_EU_ROM,  /* an Oracle's page: the system's file dialog for its European ROM */
    ORACLES_HOME_OPEN_FOLDER,    /* the game's page: the folder of its save in the system's file manager */
    ORACLES_HOME_OPEN_MODS,      /* Mods: the mods folder in the system's file manager, made if it is not there */
    ORACLES_HOME_START_AGES_MODS,      /* Mods' Play: the game with the mods active on its page, on its own save */
    ORACLES_HOME_START_SEASONS_MODS,
    ORACLES_HOME_MODS_LIMIT,     /* Mods: a ninth mod was switched on, and stays off; the screen says why */
    ORACLES_HOME_STORE,          /* a choice of the page changed: the settings are written */
    ORACLES_HOME_OPEN_UPDATE,    /* the newer version's notice: its release's page in the system's browser, the launcher staying open */
    ORACLES_HOME_IMPORT_SAVE,    /* the game's page: the system's file dialog for a save to import (ui_page_nav.h) */
    ORACLES_HOME_REPLACE_SAVE,   /* its confirmation's Replace (Import without a save): the save imported, the one it replaces kept as a backup */
    ORACLES_HOME_EXPORT_SAVE,    /* Android: the document picker for a copy of the save */
    /* The pause menu's. */
    ORACLES_HOME_RESUME,
    ORACLES_HOME_SAVE_STATE,
    ORACLES_HOME_LOAD_STATE,
    ORACLES_HOME_QUIT_GAME       /* Quit to launcher: the session ends as Escape ended it before the pause */
} OraclesHomeCommand;

typedef enum OraclesScreen {
    ORACLES_SCREEN_HOME,
    ORACLES_SCREEN_GAME,
    ORACLES_SCREEN_DISPLAY,
    ORACLES_SCREEN_CONTROLS,
    ORACLES_SCREEN_MODS,
    ORACLES_SCREEN_CHEATS,
    ORACLES_SCREEN_PAUSE,        /* the menu over a paused game */
    ORACLES_SCREEN_ABOUT         /* the home screen's version number opened */
} OraclesScreen;

/* What the launcher found at a game's ROM path (the statuses of the design, screen 2). */
typedef enum OraclesRomState {
    ORACLES_ROM_NONE,            /* none chosen */
    ORACLES_ROM_ORIGINAL,        /* the SHA-1 of an original US ROM: all profiles */
    ORACLES_ROM_UNRECOGNISED,    /* an Oracle header, another image (a hack, a randomizer seed): Faithful only */
    ORACLES_ROM_REFUSED          /* the loader's reason, or the file gone */
} OraclesRomState;

/* The launcher's two profiles, chosen in Display for every game: Faithful, the core alone at
 * 160x144, and Enhanced, the world drawn back at 480x270 (--zoom-out) with the continuous transitions on by default and
 * the smooth camera.  The 256x144 band stays --enhanced's on the command line. */
typedef enum OraclesProfile {
    ORACLES_PROFILE_FAITHFUL,
    ORACLES_PROFILE_ENHANCED,
    ORACLES_PROFILES
} OraclesProfile;


#define ORACLES_HOME_MAX_ITEMS 7
#define ORACLES_HOME_MAX_HINTS 4
#define ORACLES_HOME_STATE_LENGTH 64

#define ORACLES_HOME_TEXT_LENGTH 256

/* The games of the launcher, as nav->games holds them: the two Oracles, then the fan games, each played from an
 * Oracle's ROM and its BPS patch (oracles_home_fan_games).  Only the two Oracles have item hotkeys (no fan game's
 * profile allows them yet). */
#define ORACLES_HOME_GAMES 5
#define ORACLES_HOME_GAME_AGES 0
#define ORACLES_HOME_GAME_SEASONS 1
#define ORACLES_HOME_GAME_KINOMI 2
#define ORACLES_HOME_GAME_MOONRISE 3
#define ORACLES_HOME_GAME_TEMPLE 4
#define ORACLES_HOME_HOTKEY_GAMES 2
#define ORACLES_HOME_FAN_GAMES 3
/* Each fan game's place in oracles_home_fan_games, which every table of fan games is indexed by: in alphabetical order,
 * the order the launcher lists them in. */
#define ORACLES_HOME_FAN_KINOMI 0
#define ORACLES_HOME_FAN_MOONRISE 1
#define ORACLES_HOME_FAN_TEMPLE 2

/* A fan game: its game, hero, line of the list and start command, the Oracle whose ROM is its base, and the texts of
 * its pages. */
typedef struct OraclesHomeFanGame {
    const char *key;             /* its name in settings.txt: patch_<key>= */
    int game;                    /* ORACLES_HOME_GAME_* */
    OraclesHomeHero hero;
    OraclesHomeItemId item;
    OraclesHomeCommand start;
    int base;                    /* ORACLES_HOME_GAME_AGES or _SEASONS: the ROM its patch applies to, that Oracle's own */
    const char *base_name;       /* "Oracle of Ages (USA)" */
    const char *image_name;      /* the image its patch makes: "Moonrise Regalia 1.0.6" */
    const char *item_heading;    /* Controls' item column: "In Moonrise Regalia" */
} OraclesHomeFanGame;

/* The fan games, in the order of the list, each at its ORACLES_HOME_FAN_* place: the one table of them. */
extern const OraclesHomeFanGame oracles_home_fan_games[ORACLES_HOME_FAN_GAMES];
/* The fan game of `game` (ORACLES_HOME_GAME_*), NULL for an Oracle or none. */
const OraclesHomeFanGame *oracles_home_fan_game(int game);
/* The fan game a start command plays, NULL for an Oracle's or another command. */
const OraclesHomeFanGame *oracles_home_fan_game_started(OraclesHomeCommand command);

/* The text box's font as a game's original ROM holds it, its printable ASCII characters, for Display › Text's preview:
 * each character's cell of 8 x 16 pixels, a byte a row, bit 7 the left column, a set bit ink (the ROM's clear bits), and
 * the compact text's measures of it (oracles_text_font): its first column of ink, its width and how far it moves the
 * pen.  `read` 0: no original ROM, the preview draws its words as bars. */
#define ORACLES_HOME_FONT_FIRST 0x20
#define ORACLES_HOME_FONT_CHARACTERS 95
typedef struct OraclesHomeFont {
    int read;
    uint8_t rows[ORACLES_HOME_FONT_CHARACTERS][16];
    uint8_t left[ORACLES_HOME_FONT_CHARACTERS], width[ORACLES_HOME_FONT_CHARACTERS], advance[ORACLES_HOME_FONT_CHARACTERS];
} OraclesHomeFont;

/* A game as the launcher found it, and the choice of its page. */
typedef struct OraclesHomeGame {
    int usable;               /* a ROM is set, is there and is recognised as this game (a fan game: its image is built) */
    char last_session[16];    /* the date of its save, "21 Sep 2026", or empty */
    OraclesRomState rom;      /* a fan game's: its base ROM, its Oracle's, ORIGINAL for the one its patch is made for */
    char rom_file[ORACLES_HOME_TEXT_LENGTH];     /* the file's name, empty without a ROM */
    char rom_folder[ORACLES_HOME_TEXT_LENGTH * 4];
    char rom_reason[ORACLES_HOME_TEXT_LENGTH];   /* the loader's words, for a refused ROM */
    /* A fan game (`fan`, NULL for an Oracle): its patch, ORIGINAL once read as a BPS patch, and the image the patch
     * makes of the base, ORIGINAL when it is the fan game's (all profiles), UNRECOGNISED for another image (Faithful),
     * REFUSED with the reason the patch does not apply, NONE while a file is missing. */
    const OraclesHomeFanGame *fan;
    OraclesRomState patch, image;
    char patch_file[ORACLES_HOME_TEXT_LENGTH];
    char patch_folder[ORACLES_HOME_TEXT_LENGTH * 4];
    char patch_reason[ORACLES_HOME_TEXT_LENGTH];
    char image_reason[ORACLES_HOME_TEXT_LENGTH];
    char save_file[ORACLES_HOME_TEXT_LENGTH];    /* the save's name, empty when it does not exist yet */
    char save_written[32];                       /* "21 Sep 2026, 22:14" */
    int hotkeys;                                 /* item hotkeys: 0 off, 1 use, 2 equip, as the policy numbers them */
    int hotkeys_failed;                          /* they could not attach to this ROM in a session: it plays without them */
    unsigned cheats;                             /* an Oracle's cheats on, settings.txt's cheats_<game>= (ui_page_nav.h) */
    /* An Oracle's European ROM, which its English and official translations come from: NONE, ORIGINAL when it is this
     * game's (recognized), UNRECOGNISED for another file, REFUSED with the loader's reason when it cannot be read; a fan
     * game's, its Oracle's (the state alone), NONE while the fan games' languages are off.  The language the game plays in, an index of oracles_page_language_names,
     * English (US) (0) unless the ROM is recognized. */
    OraclesRomState eu;
    char eu_file[ORACLES_HOME_TEXT_LENGTH];
    char eu_folder[ORACLES_HOME_TEXT_LENGTH * 4];
    char eu_reason[ORACLES_HOME_TEXT_LENGTH];
    int language;
    OraclesHomeFont font;                        /* its ROM's, a fan game's base ROM's */
} OraclesHomeGame;

/* The choices of Display, the same for every game. */
typedef struct OraclesHomeDisplay {
    OraclesProfile profile;     /* the one chosen; a game whose ROM allows only Faithful plays in Faithful */
    int transitions;            /* continuous transitions, on by default, in Enhanced only */
    int hud;                    /* the HUD: 0 Original, 1 Full width (the default), 2 Wide layout, the settings' hud=; in Enhanced only, from a game
                                 * too, where it applies at once */
    int text;                   /* the text: 0 Classic (the default), 1 Compact, the settings' text=; in Enhanced only; not from a
                                 * game, which keeps the texts it laid out at its start */
    int text_box;               /* the text box: 0 Standard (the default), 1 Wide, the settings' text_box=; in Enhanced only, Ages
                                 * and Seasons; not from a game */
    int text_size;              /* Text size: 0 Normal (the default), 1 Large, 2 Very large, the settings' text_size=; in Enhanced
                                 * only, Ages and Seasons; from a game too, where it applies at once */
    int text_speed;             /* Text speed: 0 Classic (the default), 1 Instant, the settings' text_speed=; in either profile, every
                                 * game; from a game too, where it applies when the game resumes */
    int text_fixed;             /* --text gave it for the run: the row shows it, dimmed and inert, and it is never stored */
    int text_box_fixed;         /* --text-box gave it, the same */
    int text_size_fixed;        /* --text-size gave it, the same */
    int view;                   /* the Enhanced view: 0 near, 1 medium, 2 far (the default), in Enhanced only */
    int window;                 /* 0, 1, 2: the surface at 2x, 3x, 4x; 3: fullscreen; from the pause, -1 for a window none of
                                 * them is (F11 out of fullscreen at another size, a window resized by hand) */
    int window_w, window_h;     /* that window's size, which the diagram shows */
    int window_chosen;          /* from the pause: Window was changed since the last store, which alone then stores it */
    int colour;                 /* colour correction */
    int vsync;                  /* 0 auto, 1 on, 2 off */
    int core;                   /* 0 Accurate (SameBoy), 1 Fast (mGBA); in a game, the running game's, which does not change */
    int workers;                /* the neighbour workers, the settings' ghosts=: 0 auto, 1, 2; in a game, the running game's */
    int scaling;                /* 0 Sharp, 1 Fill, the settings' scaling=; from a game too, where it applies at once */
    int camera;                 /* Camera: 0 Pixel, 1 Smooth, the settings' subpixel_camera=, -1 while it is absent (the platform's
                                 * default); in Enhanced only, from a game too, where it applies at once */
    int camera_from;            /* the whole scale from which the platform's default is Smooth, 0 for every scale (the desktops) */
    int margin;                 /* the screen margin, the settings' screen_margin= (or the command line's, for the run): 0 off,
                                 * 1 auto, 2 small, 3 large; from a game too, where it applies at once */
    int margin_fixed;           /* --screen-margin gave it for the run: the row shows it, dimmed and inert, and it is never stored */
    int margin_lines[4];        /* the screen's lines each margin (Off, Auto, Small, Large) keeps the game out of, above and
                                 * below together: 0 for none */
    int cores;                  /* the device's logical processor cores, which Auto's count follows */
    int screen_w, screen_h;     /* the display's size, for the sizes the page shows */
    int aspect;                 /* the settings' aspect= (an OraclesAspect): the shape of those sizes, the screen's when auto */
    int far_fixed;              /* Far size: 0 Fitted, 1 Fixed, the settings' far=fixed: far keeps its size, else fitted to the screen where it
                                 * fills it badly; in Enhanced only, not from a game */
    int menus;                  /* Menus: 0 View, 1 Large, the settings' menus= (Large Android's default); in Enhanced only, from a game
                                 * too, where it applies at once */
    int unvisited;              /* Unvisited rooms: 0 Shown (the default), 1 Hidden, the settings' unvisited_rooms=; in Enhanced only,
                                 * from a game too, where it applies when the game resumes */
    int unvisited_fixed;        /* --unvisited-rooms gave it for the run: the row shows it, dimmed and inert, and it is never stored */
    int room_w, room_h;         /* the room a window has there: the usable area, less the title bar */
    int fullscreen_only;        /* the platform plays every game fullscreen (Android): one size whatever Window says */
    unsigned windows_refused;   /* from the pause, the windows (1 << Window's value) whose pause would keep only its own
                                 * entries (oracles_ui_layout_narrow), Display among those it leaves: dimmed and inert */
} OraclesHomeDisplay;

/* Controls (ui_controls_nav.h): the names settings.txt writes, SDL's key names and
 * controller button names, an empty name for none; and the slots' items of each game, read-only. */
#define ORACLES_HOME_NAME_LENGTH 32
#define ORACLES_HOME_BUTTONS 8          /* Right, Left, Up, Down, A, B, Select, Start */
#define ORACLES_HOME_PAD_BUTTONS 4      /* A, B, Select, Start: the d-pad and the left stick always move */
#define ORACLES_HOME_HOTKEY_ROWS 6      /* the four slots, Bind to B, Bind to A */
#define ORACLES_HOME_SLOTS 4
typedef struct OraclesHomeControls {
    char keys[ORACLES_HOME_BUTTONS][ORACLES_HOME_NAME_LENGTH];
    char pads[ORACLES_HOME_PAD_BUTTONS][ORACLES_HOME_NAME_LENGTH];
    char hotkey_keys[ORACLES_HOME_HOTKEY_ROWS][ORACLES_HOME_NAME_LENGTH];
    char hotkey_pads[ORACLES_HOME_SLOTS][ORACLES_HOME_NAME_LENGTH];
    char menu_pad[ORACLES_HOME_NAME_LENGTH];   /* the controller's Escape: the pause menu in a game, Back here (settings.txt's menu_pad=) */
    char items[2][ORACLES_HOME_SLOTS][ORACLES_HOME_NAME_LENGTH];   /* the item in each slot, empty when none */
    int swim_hold;               /* Swimming, for every game: 0 Tap, 1 Hold (settings.txt's swim_hold=) */
    int column, row;             /* the highlighted cell (ui_controls_nav.h) */
    int capturing;               /* the highlighted cell waits for a key or a button */
    int tab;                     /* 4:3: the tab shown while the highlight is on the tabs (ORACLES_CONTROLS_TAB_*) */
} OraclesHomeControls;

/* Mods: the mods of the mods folder as the page lists them, for each Oracle (a mod is loaded for the game to say
 * whether it is refused), and the folder. */
#define ORACLES_HOME_MODS 16             /* listed, in the order of their names */
#define ORACLES_HOME_MODS_ACTIVE 8       /* active together, at most */
#define ORACLES_HOME_MOD_NAME 64
typedef struct OraclesHomeMod {
    char name[ORACLES_HOME_MOD_NAME];    /* its folder's */
    char line[ORACLES_HOME_TEXT_LENGTH]; /* its description, or "Refused: " and the loader's reason */
    int refused;
    int houses[ORACLES_HOME_HOTKEY_GAMES];   /* the houses it adds in Ages, in Seasons */
    int active;                          /* switched on; never for a refused mod */
} OraclesHomeMod;
typedef struct OraclesHomeMods {
    unsigned count;
    OraclesHomeMod mods[ORACLES_HOME_MODS];
} OraclesHomeMods;

/* The device the player used last, which the help bar's keys follow (oracles_home_hints): before any input and after a
 * click or a touch, both devices' keys ("Enter / A"); after a key, the keyboard's alone; after a controller's button or
 * stick, the controller's alone. */
typedef enum OraclesInputDevice {
    ORACLES_INPUT_POINTER,
    ORACLES_INPUT_KEYBOARD,
    ORACLES_INPUT_GAMEPAD
} OraclesInputDevice;

/* Cartridge's Save row (ui_page_nav.h): the import's confirmation over the page, with the file chosen, its folder and
 * the button left and right chose; and the line an import or an export left in the row, success or refusal, until the
 * page is left. */
typedef enum OraclesSaveFlow {
    ORACLES_SAVE_FLOW_NONE,
    ORACLES_SAVE_FLOW_CONFIRM,
    ORACLES_SAVE_FLOW_DONE,
    ORACLES_SAVE_FLOW_REFUSED
} OraclesSaveFlow;
typedef struct OraclesHomeSaveFlow {
    OraclesSaveFlow state;
    int choice;                                  /* the confirmation's: 0 Replace (Import without a save), 1 Cancel */
    int replace;                                 /* 1: the game has a save, which the file replaces; 2: its game with mods has files */
    char file[ORACLES_HOME_TEXT_LENGTH], folder[ORACLES_HOME_TEXT_LENGTH * 4];
    char text[ORACLES_HOME_TEXT_LENGTH];         /* DONE or REFUSED: the row's status */
    char text2[ORACLES_HOME_TEXT_LENGTH];        /* and a line under it, empty without one */
} OraclesHomeSaveFlow;

typedef struct OraclesHomeNav {
    OraclesScreen screen;
    OraclesUiLayout layout;            /* the window's (oracles_ui_layout_choose), which its owner sets before each frame */
    OraclesHomeEntry entry;
    OraclesHomeHero fan_pick;          /* ORACLES_HOME_HERO_FAN while the list shows */
    unsigned focus;
    unsigned row;                      /* the highlighted row of the game's page, or of Display */
    int eu_action;                     /* Cartridge's European ROM row with a file: 0 Change, 1 Remove, as left and right set it */
    int save_action;                   /* Cartridge's Save row: 0 Open folder (Android: Export save…), 1 Import save… */
    OraclesHomeSaveFlow save_flow;
    int advanced;                      /* Display shows its Advanced rows in place of its own */
    int text_page;                     /* Display shows its Text rows in place of its own (Display › Text) */
    int picture_page;                  /* Display shows its Picture rows in place of its own (Display › Picture) */
    /* The fan games' Language row, their language beside Cartridge and their games started in it: 0 in the product,
     * whose host leaves them off (OraclesHomeGames); 1 lays them out as the mockup's fanLang does. */
    int fan_languages;
    OraclesHomeGame games[ORACLES_HOME_GAMES];   /* Ages, Seasons, the fan games */
    OraclesHomeDisplay display;
    OraclesHomeControls controls;
    OraclesHomeMods mods[ORACLES_HOME_HOTKEY_GAMES];   /* Ages', Seasons' */
    char mods_folder[ORACLES_HOME_TEXT_LENGTH * 4];
    /* Cheats: --cheats gave the cheats of every game the home screen starts, for the run (`cheats_fixed`): the page shows
     * them, dimmed and inert, and never stores them.  The games whose cheats the page changed (1 << ORACLES_HOME_GAME_*),
     * which alone the host writes. */
    int cheats_fixed;
    unsigned cheats_changed;
    /* In a game: its profile has no cheats (an unrecognised ROM, a fan game): the pause has no Cheats.  An Oracle's
     * whose profile has no swim's hold, no item hotkeys (an unrecognised ROM): Controls' Swimming, its Item hotkeys,
     * dimmed and inert, saying why; a game's whose profile has no instant text, Display › Text's Text speed (on the home
     * screen, a page whose ROM is unrecognised). */
    int cheats_refused, swim_refused, hotkeys_refused, text_speed_refused;
    /* In a game (the pause menu, and Controls and Display opened from it): the running game's profile, the pause
     * menu reduced in a 16:9 window under 960 pixels wide (oracles_ui_layout_narrow), and Load state's note
     * ("from 22:14"), empty while there is no savestate: Load state is then greyed, "No savestate yet". */
    int in_game, narrow;
    OraclesProfile playing;
    char load_note[ORACLES_HOME_NAME_LENGTH];
    /* After a game that ran slowly, the home screen's toast that suggests something lighter (oracles_home_slow_text),
     * until the first input. */
    int slow;
    /* The device used last (oracles_home_input), and whether a key was ever pressed, which shows Controls' keys in game
     * (oracles_controls_keys_shown); `android`, the platform's: always fullscreen, no F11, and its Back opens the menu
     * too. */
    OraclesInputDevice device;
    int keyboard_used;
    int android;
    /* The newer version the update check found (update_check.h), "1.3.0", empty while there is none: the home screen
     * alone shows it by the version, a place after the menu's last item for the highlight (oracles_home_update_shown),
     * while the update check is on (`update_check`, settings.txt's update_check=, About's row). */
    char update[ORACLES_HOME_NAME_LENGTH];
    int update_check;
} OraclesHomeNav;

typedef struct OraclesHomeItem {
    OraclesHomeItemId id;
    const char *label;
    const char *note;       /* always shown, or NULL */
    int disabled;
    const char *reason;     /* shown in place of the note while a disabled item is highlighted */
    char text[ORACLES_HOME_STATE_LENGTH];   /* a note made for the item (a fan game's state in the list), which `note` then points to */
} OraclesHomeItem;

typedef struct OraclesHomeHint {
    const char *key, *label;
    int back;               /* a click on it goes back */
    int chars;              /* its key's frame holds this many characters at least: its widest key over the devices */
} OraclesHomeHint;

/* Ages chosen, the first item highlighted, no ROM known. */
void oracles_home_init(OraclesHomeNav *nav);
/* The pause menu over `game`'s session (ORACLES_HOME_GAME_*) running in `playing`, on Resume. */
void oracles_home_pause(OraclesHomeNav *nav, int game, OraclesProfile playing, int narrow);

OraclesHomeHero oracles_home_hero(const OraclesHomeNav *nav);
/* The menu of what the right half shows; returns the number of items. */
unsigned oracles_home_items(const OraclesHomeNav *nav, OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS]);
/* The highlighted item, within the menu; past it, the newer version's notice or the version number while they have the
 * highlight (the menu's count, and the next). */
unsigned oracles_home_focus(const OraclesHomeNav *nav);
/* A page closed: the highlight on its item in the menu shown now, or on the first (Resume) when that menu has lost it
 * (the pause's, made narrow while the page was open). */
void oracles_home_focus_item(OraclesHomeNav *nav, OraclesHomeItemId id);
/* The newer version's notice is shown (on the home screen, nav->update set, the update check on), and highlighted: down
 * from the menu's last item; OK opens its page.  The version number, on the home screen alone, is the place after it
 * (after the menu's last item without the notice), up from the menu's first, the menu no longer wrapping from one to the
 * other: OK opens About, which Back leaves for it. */
int oracles_home_update_shown(const OraclesHomeNav *nav);
int oracles_home_update_focused(const OraclesHomeNav *nav);
int oracles_home_version_focused(const OraclesHomeNav *nav);
/* The entries of the left stack, in order. */
void oracles_home_others(const OraclesHomeNav *nav, OraclesHomeEntry others[2]);
OraclesHomeHero oracles_home_entry_hero(OraclesHomeEntry entry);
/* The words above a title, the title, and the state below it. */
const char *oracles_home_over(OraclesHomeHero hero);
const char *oracles_home_title(OraclesHomeHero hero);
void oracles_home_state(const OraclesHomeNav *nav, OraclesHomeHero hero, char out[ORACLES_HOME_STATE_LENGTH]);
/* The help bar's hints, their keys the device's used last (nav->device): a controller's D-pad, A and B, and the menu's
 * button for Escape alone (Guide without one); the keyboard's Enter and Esc; both devices' after a click or a touch.
 * Each key's frame holds its widest label, so that the hints stay in place when the device changes. */
unsigned oracles_home_hints(const OraclesHomeNav *nav, OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS]);
/* An input from `device`: the hints follow it, and a key shows Controls' keys in game from then on.  1 when what the
 * screen shows may change. */
int oracles_home_input(OraclesHomeNav *nav, OraclesInputDevice device);
/* Whether the screens name keyboard keys (Controls' keys in game, F2 under Display's colour correction): once a key was
 * pressed, or on a desktop until a controller's input; on Android only once a key was pressed. */
int oracles_home_keys_shown(const OraclesHomeNav *nav);

OraclesHomeCommand oracles_home_act(OraclesHomeNav *nav, OraclesHomeAction action);
/* The game of what the right half shows (ORACLES_HOME_GAME_*), or -1 for the list of fan games. */
int oracles_home_game(const OraclesHomeNav *nav);
/* The game a session plays, from Start game or Play: ORACLES_HOME_START_AGES, _SEASONS or a fan game's. */
OraclesHomeCommand oracles_home_start_command(int game);

/* The toast after a game that ran slowly: its text, "The game ran slowly at High: Medium may play smoother.", from
 * Display's quality, or under Custom from its view ("... in the Far view: the Medium view ..."), and the row it leads to,
 * "Display › Quality" or "Display › View"; 0 when nothing is lighter (Low, Custom in the near view). */
int oracles_home_slow_text(const OraclesHomeNav *nav, char *text, size_t capacity, const char **path);
/* The toast clicked: Display, on its Quality row (View under Custom); the toast goes. */
void oracles_home_slow_open(OraclesHomeNav *nav);

/* The pointer: an entry of the left stack clicked, an item hovered or clicked; the newer version's notice is the item
 * after the menu's last, the version number the one after (the notice's place when it is not shown). */
void oracles_home_select(OraclesHomeNav *nav, OraclesHomeEntry entry);
void oracles_home_hover(OraclesHomeNav *nav, unsigned item);
OraclesHomeCommand oracles_home_click(OraclesHomeNav *nav, unsigned item);

#endif
