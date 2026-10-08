/* The launcher's state and navigation, without SDL: the home screen (the
 * games stacked on the left) and
 * the game's page and Display (screens 2 and 3, ui_page_nav.c).
 *
 * Three entries: Oracle of Ages, Oracle of Seasons, Fan games.  The chosen
 * entry fills the right half with its title and its menu, the two others are
 * stacked on the left.  Left and right change the entry; up and down move in
 * the menu, which wraps; OK activates the highlighted item, Back leaves the
 * list of fan games.  A disabled item is highlighted like the others and
 * shows its reason.  Fan games opens the list of fan games in the menu;
 * a fan game picked there (oracles_home_fan_games) has the menu of a game.  Cartridge opens
 * the game's page (its ROM, or a fan game's base ROM and patch; its save;
 * Play), Display the profile and the window's choices, the same for every
 * game, Controls the keys and buttons, Mods the mods of an Oracle (ui_page_nav.h),
 * greyed for a fan game. */
#ifndef ORACLES_UI_HOME_NAV_H
#define ORACLES_UI_HOME_NAV_H

#include <stddef.h>

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
    ORACLES_HOME_OPEN_FOLDER,    /* the game's page: the folder of its save in the system's file manager */
    ORACLES_HOME_OPEN_MODS,      /* Mods: the mods folder in the system's file manager, made if it is not there */
    ORACLES_HOME_START_AGES_MODS,      /* Mods' Play: the game with the mods active on its page, on its own save */
    ORACLES_HOME_START_SEASONS_MODS,
    ORACLES_HOME_MODS_LIMIT,     /* Mods: a ninth mod was switched on, and stays off; the screen says why */
    ORACLES_HOME_STORE,          /* a choice of the page changed: the settings are written */
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
    ORACLES_SCREEN_PAUSE         /* the menu over a paused game */
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


#define ORACLES_HOME_MAX_ITEMS 6
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
} OraclesHomeGame;

/* The choices of Display, the same for every game. */
typedef struct OraclesHomeDisplay {
    OraclesProfile profile;     /* the one chosen; a game whose ROM allows only Faithful plays in Faithful */
    int transitions;            /* continuous transitions, on by default, in Enhanced only */
    int view;                   /* the Enhanced view: 0 near, 1 medium, 2 far (the default), in Enhanced only */
    int window;                 /* 0, 1, 2: the surface at 2x, 3x, 4x; 3: fullscreen */
    int colour;                 /* colour correction */
    int vsync;                  /* 0 auto, 1 on, 2 off */
    int core;                   /* 0 Accurate (SameBoy), 1 Fast (mGBA); in a game, the running game's, which does not change */
    int screen_w, screen_h;     /* the display's size, for the sizes the page shows */
    int aspect;                 /* the settings' aspect= (an OraclesAspect): the shape of those sizes, the screen's when auto */
    int room_w, room_h;         /* the room a window has there: the usable area, less the title bar */
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
    char items[2][ORACLES_HOME_SLOTS][ORACLES_HOME_NAME_LENGTH];   /* the item in each slot, empty when none */
    int column, row;             /* the highlighted cell (ui_controls_nav.h) */
    int capturing;               /* the highlighted cell waits for a key or a button */
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

typedef struct OraclesHomeNav {
    OraclesScreen screen;
    OraclesHomeEntry entry;
    OraclesHomeHero fan_pick;          /* ORACLES_HOME_HERO_FAN while the list shows */
    unsigned focus;
    unsigned row;                      /* the highlighted row of the game's page, or of Display */
    OraclesHomeGame games[ORACLES_HOME_GAMES];   /* Ages, Seasons, the fan games */
    OraclesHomeDisplay display;
    OraclesHomeControls controls;
    OraclesHomeMods mods[ORACLES_HOME_HOTKEY_GAMES];   /* Ages', Seasons' */
    char mods_folder[ORACLES_HOME_TEXT_LENGTH * 4];
    /* In a game (the pause menu, and Controls and Display opened from it): the running game's profile, the pause
     * menu reduced in a window under 960 pixels wide, and Load state's note ("F7 · from 22:14"). */
    int in_game, narrow;
    OraclesProfile playing;
    char load_note[ORACLES_HOME_NAME_LENGTH];
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
} OraclesHomeHint;

/* Ages chosen, the first item highlighted, no ROM known. */
void oracles_home_init(OraclesHomeNav *nav);
/* The pause menu over `game`'s session (ORACLES_HOME_GAME_*) running in `playing`, on Resume. */
void oracles_home_pause(OraclesHomeNav *nav, int game, OraclesProfile playing, int narrow);

OraclesHomeHero oracles_home_hero(const OraclesHomeNav *nav);
/* The menu of what the right half shows; returns the number of items. */
unsigned oracles_home_items(const OraclesHomeNav *nav, OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS]);
/* The highlighted item, within the menu. */
unsigned oracles_home_focus(const OraclesHomeNav *nav);
/* The entries of the left stack, in order. */
void oracles_home_others(const OraclesHomeNav *nav, OraclesHomeEntry others[2]);
OraclesHomeHero oracles_home_entry_hero(OraclesHomeEntry entry);
/* The words above a title, the title, and the state below it. */
const char *oracles_home_over(OraclesHomeHero hero);
const char *oracles_home_title(OraclesHomeHero hero);
void oracles_home_state(const OraclesHomeNav *nav, OraclesHomeHero hero, char out[ORACLES_HOME_STATE_LENGTH]);
unsigned oracles_home_hints(const OraclesHomeNav *nav, OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS]);

OraclesHomeCommand oracles_home_act(OraclesHomeNav *nav, OraclesHomeAction action);
/* The game of what the right half shows (ORACLES_HOME_GAME_*), or -1 for the list of fan games. */
int oracles_home_game(const OraclesHomeNav *nav);
/* The game a session plays, from Start game or Play: ORACLES_HOME_START_AGES, _SEASONS or a fan game's. */
OraclesHomeCommand oracles_home_start_command(int game);

/* The pointer: an entry of the left stack clicked, an item hovered or clicked. */
void oracles_home_select(OraclesHomeNav *nav, OraclesHomeEntry entry);
void oracles_home_hover(OraclesHomeNav *nav, unsigned item);
OraclesHomeCommand oracles_home_click(OraclesHomeNav *nav, unsigned item);

#endif
