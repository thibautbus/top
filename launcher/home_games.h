/* The games behind the home screen: each entry's ROM from settings.txt and
 * what the launcher finds there (an original, an unrecognised ROM, refused
 * with the loader's reason), its save and its date, Display's and Controls'
 * choices kept in settings.txt; each fan game's patch, applied to its
 * Oracle's ROM in memory, the image it makes identified, its save beside the
 * patch; a ROM or a patch chosen in the
 * system's dialog, or dropped on the window, identified by engine/rom and
 * filed under its game, or refused: then shown under its game with the
 * reason and not kept; the folder of its save; and the session Start game or
 * Play plays in the launcher's window. */
#ifndef ORACLES_LAUNCHER_HOME_GAMES_H
#define ORACLES_LAUNCHER_HOME_GAMES_H

#include "home.h"
#include "session.h"
#include "settings.h"

#include <time.h>

typedef struct OraclesHomeGames {
    oracles_settings *settings;
    /* The command line's session options: an option given there wins over the pages' choices. */
    const OraclesSessionOptions *options;
    int window_from_command_line;    /* --scale or --fullscreen given: Display's window does not apply */
    int route_recorded;              /* --record given to the home screen: its first session wrote the route */
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
} OraclesHomeGames;

/* The host of the home screen for these games. */
void oracles_home_games_host(OraclesHomeGames *games, OraclesHomeHost *host);

/* "21 Sep 2026": a date as the home screen shows it, in English whatever the locale. */
void oracles_home_format_date(time_t when, char *out, size_t capacity);

#endif
