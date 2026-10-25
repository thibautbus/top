/* The games behind the home screen: each entry's ROM from settings.txt and
 * what the launcher finds there (an original, an unrecognised ROM, refused
 * with the loader's reason), its save and its date, Display's and Controls'
 * choices kept in settings.txt; each fan game's patch, applied to its
 * Oracle's ROM in memory, the image it makes identified, its save beside the
 * patch; a ROM or a patch chosen in the
 * system's dialog, or dropped on the window, identified by engine/rom and
 * filed under its game, or refused: then shown under its game with the
 * reason and not kept; an Oracle's European ROM, chosen or dropped, kept
 * and recognized by its SHA-1, and the language its game plays in; the
 * folder of its save; and the session Start game or Play plays in the
 * launcher's window, in that language. */
#ifndef ORACLES_LAUNCHER_HOME_GAMES_H
#define ORACLES_LAUNCHER_HOME_GAMES_H

#include "home.h"
#include "home_files.h"
#include "save.h"
#include "session.h"
#include "settings.h"

#include <time.h>

/* What a file was found to be, with the file as it was then: its path (empty: nothing found yet), size and
 * modification time. */
typedef struct OraclesHomeFound {
    char path[ORACLES_SETTINGS_PATH_LENGTH];
    long long size, modified;
    OraclesRomState state;
    char reason[ORACLES_HOME_TEXT_LENGTH];
} OraclesHomeFound;

typedef struct OraclesHomeGames {
    oracles_settings *settings;
    /* The command line's session options: an option given there wins over the pages' choices. */
    const OraclesSessionOptions *options;
    int window_from_command_line;    /* --scale or --fullscreen given: Display's window does not apply */
    int route_recorded;              /* --record given to the home screen: its first session wrote the route */
    /* The fan games' languages (nav->fan_languages): their Language row, their language read from settings.txt, its
     * share counted and the game started in it.  The launcher leaves them off: a fan game starts in English. */
    int fan_languages;
    char hotkeys_failed[ORACLES_SETTINGS_GAMES][ORACLES_SETTINGS_PATH_LENGTH];   /* the ROM the item hotkeys could not attach to */
    /* A ROM chosen or dropped for a game and refused: shown on the game's page with the reason (the
     * layout reference's frame 1j), and not kept in settings.txt; a ROM accepted for the game replaces it. */
    struct {
        char path[ORACLES_SETTINGS_PATH_LENGTH];
        char reason[ORACLES_HOME_TEXT_LENGTH];
    } refused[ORACLES_SETTINGS_GAMES];
    /* A patch chosen or dropped for a fan game and refused (not a BPS patch, damaged): the same, by fan game. */
    struct {
        char path[ORACLES_SETTINGS_PATH_LENGTH];
        char reason[ORACLES_HOME_TEXT_LENGTH];
    } refused_patch[ORACLES_HOME_FAN_GAMES];
    /* What each file was found to be, kept while it is the same: a refresh, at each return to the home screen and each
     * pause, reads none again.  Each Oracle's ROM and European ROM; each fan game's patch, and the image it makes of its
     * Oracle's ROM (kept with that ROM, and forgotten with the patch). */
    OraclesHomeFound rom_found[ORACLES_SETTINGS_GAMES], eu_found[ORACLES_SETTINGS_GAMES];
    OraclesHomeFound patch_found[ORACLES_HOME_FAN_GAMES], image_found[ORACLES_HOME_FAN_GAMES];
    /* The files a refresh, and the choice or the drop before it, read: each once, forgotten at its end. */
    OraclesHomeFiles files;
    /* Each Oracle's text box font, read with its original ROM (rom_found), for Display › Text's preview. */
    OraclesHomeFont rom_font[ORACLES_SETTINGS_GAMES];
    /* A save chosen to import, read and checked, set to English for an Oracle: for the game whose page chose it, while
     * its confirmation is open there. */
    struct {
        int game, english;
        uint8_t data[ORACLES_SAVE_SIZE];
    } import;
} OraclesHomeGames;

/* The host of the home screen for these games. */
void oracles_home_games_host(OraclesHomeGames *games, OraclesHomeHost *host);
/* The Save row's part of it (home_save.c): Open folder, Import save… and Export save…. */
void oracles_home_save_host(OraclesHomeGames *games, OraclesHomeHost *host);
/* The save of game `game` (ORACLES_HOME_GAME_*), beside its ROM or a fan game's patch; empty without that file. */
void oracles_home_games_save_path(const OraclesHomeGames *games, int game, char *path, size_t capacity);

/* "21 Sep 2026": a date as the home screen shows it, in English whatever the locale. */
void oracles_home_format_date(time_t when, char *out, size_t capacity);

#endif
