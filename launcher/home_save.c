/* Cartridge's Save row for the home screen's games (home_games.h): Open folder, the system's file manager on the save's
 * folder (Android, which opens none, says where it is); Import save…, a save chosen in the system's dialog, checked
 * (save_transfer.h), confirmed, then written over the game's, the one it replaces, its savestate and the game with mods'
 * save kept in Backups; and on Android Export save…, a copy of the save where the document picker makes one, as Mods'
 * Save row's Export save with mods… makes one of the game with mods' save.  The page shows the outcome as the row's
 * status, and the standard error says it too. */
#include "home_games.h"

#include "file_dialog.h"
#include "save_transfer.h"
#include "ui_page_nav.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

/* The folder of a path, whichever separator it uses; "." for a bare name. */
static void folder_of(const char *path, char *out, size_t capacity)
{
    const char *slash = strrchr(path, '/'), *backslash = strrchr(path, '\\');
    if (backslash && (!slash || backslash > slash)) slash = backslash;
    snprintf(out, capacity, "%.*s", slash ? (int)(slash - path) : 1, slash ? path : ".");
}

/* The game's name as the row's refusal says it, and the engine its save is a save of. */
static const char *game_name(int game, OraclesGame *engine)
{
    const OraclesHomeFanGame *fan = oracles_home_fan_game(game);
    const int base = fan ? fan->base : game;
    *engine = base == ORACLES_HOME_GAME_SEASONS ? ORACLES_GAME_SEASONS : ORACLES_GAME_AGES;
    return fan ? oracles_home_title(fan->hero) : oracles_game_name(*engine);
}

static void status(OraclesHomeNav *nav, OraclesSaveFlow state, const char *text, const char *second)
{
    nav->save_flow.state = state;
    snprintf(nav->save_flow.text, sizeof nav->save_flow.text, "%s", text);
    snprintf(nav->save_flow.text2, sizeof nav->save_flow.text2, "%s", second ? second : "");
}

static void open_folder(void *opaque, int game, char *message, size_t capacity)
{
    const OraclesHomeGames *games = opaque;
    char save[ORACLES_SETTINGS_PATH_LENGTH], folder[ORACLES_SETTINGS_PATH_LENGTH];
    oracles_home_games_save_path(games, game, save, sizeof save);
    folder_of(save, folder, sizeof folder);
#ifdef __ANDROID__
    /* Android opens no file manager on a folder: the toast says where the saves are, from the shared storage's root, as
     * a computer sees it over USB, as the page shows it. */
    char shown[ORACLES_SETTINGS_PATH_LENGTH];
    oracles_page_folder(1, folder, shown, sizeof shown);
    snprintf(message, capacity, "Saves are in %s, reachable over USB", shown);
#else
    char error[256];
    if (oracles_open_folder(folder, error, sizeof error) != 0) snprintf(message, capacity, "The folder does not open: %s", error);
#endif
}

/* The file an export copies: the save, or for ORACLES_DIALOG_EXPORT_MODS the game with mods' save beside it. */
static void exported_path(const OraclesHomeGames *games, int game, int kind, char *path, size_t capacity)
{
    char save[ORACLES_SETTINGS_PATH_LENGTH];
    oracles_home_games_save_path(games, game, save, sizeof save);
    if (save[0]) oracles_save_transfer_exported(save, kind == ORACLES_DIALOG_EXPORT_MODS, path, capacity);
    else path[0] = 0;
}

static int choose_save(void *opaque, int game, int kind, struct SDL_Window *window)
{
    const OraclesHomeGames *games = opaque;
    char save[ORACLES_SETTINGS_PATH_LENGTH], folder[ORACLES_SETTINGS_PATH_LENGTH];
    if (kind != ORACLES_DIALOG_SAVE) {
        /* A file gone since the page showed it (moved over USB): no picker, which would leave an empty document behind;
         * the refresh then greys the action. */
        exported_path(games, game, kind, save, sizeof save);
        if (!save[0] || !SDL_GetPathInfo(save, NULL)) {
            fprintf(stderr, "oracles: the save %s is no longer there: nothing to export\n", save);
            return 0;
        }
        const char *slash = strrchr(save, '/');
        oracles_choose_export(slash ? slash + 1 : save, (OraclesDialogKind)kind, game);
        return 1;
    }
    oracles_home_games_save_path(games, game, save, sizeof save);
    folder_of(save, folder, sizeof folder);
    oracles_choose_file(window, ORACLES_DIALOG_SAVE, folder, game);
    return 1;
}

/* A copy of the save, or of the game with mods' save, written where the document picker made it. */
static void exported(const OraclesHomeGames *games, int game, int kind, const char *destination, OraclesHomeNav *nav)
{
    char save[ORACLES_SETTINGS_PATH_LENGTH], error[ORACLES_HOME_TEXT_LENGTH];
    exported_path(games, game, kind, save, sizeof save);
    if (!oracles_save_transfer_export(save, destination, error, sizeof error)) {
        fprintf(stderr, "oracles: the save %s is not exported: %s\n", save, error);
        status(nav, ORACLES_SAVE_FLOW_REFUSED, "The copy cannot be written.", NULL);
        return;
    }
    fprintf(stderr, "oracles: the save %s exported to %s\n", save, destination);
    status(nav, ORACLES_SAVE_FLOW_DONE, kind == ORACLES_DIALOG_EXPORT_MODS ? "Save with mods exported." : "Save exported.", NULL);
}

static void save_chosen(void *opaque, int game, int kind, int result, const char *path, OraclesHomeNav *nav)
{
    OraclesHomeGames *games = opaque;
    if (result == ORACLES_DIALOG_CANCELLED) return;
    if (result != ORACLES_DIALOG_CHOSEN) {
        status(nav, ORACLES_SAVE_FLOW_REFUSED, kind != ORACLES_DIALOG_SAVE ? "No document picker on this system." : "No file dialog on this system.", NULL);
        return;
    }
    if (kind != ORACLES_DIALOG_SAVE) { exported(games, game, kind, path, nav); return; }
    /* The file read where it is (on Android through its content:// address), checked as a save of the game, then
     * confirmed on the page. */
    OraclesGame engine;
    const char *name = game_name(game, &engine);
    static uint8_t data[ORACLES_SAVE_TRANSFER_READ];
    size_t size = 0;
    char why[256];
    const int read = oracles_save_transfer_read(path, data, &size, why, sizeof why);
    const int save = read && oracles_save_transfer_check(data, size, engine, !oracles_home_fan_game(game), games->import.data, &games->import.english);
    if (!save) {
        char text[ORACLES_HOME_TEXT_LENGTH];
        snprintf(text, sizeof text, "This file is not a save of %s.", name);
        fprintf(stderr, "oracles: %s refused for %s: %s\n", path, name, read ? "not a save of its game" : why);
        status(nav, ORACLES_SAVE_FLOW_REFUSED, text, NULL);
        return;
    }
    games->import.game = game;
    nav->save_flow.state = ORACLES_SAVE_FLOW_CONFIRM;
    nav->save_flow.choice = 0;
    /* The game with mods' files go to Backups too: the confirmation says so. */
    char current[ORACLES_SETTINGS_PATH_LENGTH];
    oracles_home_games_save_path(games, game, current, sizeof current);
    nav->save_flow.replace = current[0] && oracles_save_transfer_has_mods(current) ? 2 : nav->games[game].save_file[0] != 0;
    oracles_dialog_describe(path, nav->save_flow.file, sizeof nav->save_flow.file, nav->save_flow.folder, sizeof nav->save_flow.folder);
}

static void replace_save(void *opaque, int game, OraclesHomeNav *nav)
{
    OraclesHomeGames *games = opaque;
    if (nav->save_flow.state != ORACLES_SAVE_FLOW_CONFIRM || games->import.game != game) return;
    char save[ORACLES_SETTINGS_PATH_LENGTH], error[200];
    static OraclesSaveKept kept;
    oracles_home_games_save_path(games, game, save, sizeof save);
    if (!save[0] || !oracles_save_transfer_import(save, games->import.data, ORACLES_SAVE_SIZE, time(NULL), &kept, error, sizeof error)) {
        fprintf(stderr, "oracles: the save %s is not imported: %s\n", save, save[0] ? error : "no ROM");
        char text[ORACLES_HOME_TEXT_LENGTH];
        snprintf(text, sizeof text, "Nothing replaced: %s.", save[0] ? error : "no ROM");
        status(nav, ORACLES_SAVE_FLOW_REFUSED, text, NULL);
        return;
    }
    fprintf(stderr, "oracles: save imported as %s%s%s%s%s%s%s%s\n", save, kept.save[0] ? ", the previous one kept as " : "", kept.save,
            kept.state[0] ? ", its savestate moved to " : "", kept.state, kept.mods_save[0] ? ", the save with mods moved to " : "", kept.mods_save,
            games->import.english ? ", its language set to English" : "");
    if (kept.mods_state[0] || kept.mods_store[0])
        fprintf(stderr, "oracles: with it, %s%s%s\n", kept.mods_state, kept.mods_state[0] && kept.mods_store[0] ? " and " : "", kept.mods_store);
    char done[ORACLES_HOME_TEXT_LENGTH], kept_line[ORACLES_HOME_TEXT_LENGTH];
    const unsigned moved = (kept.save[0] ? ORACLES_PAGE_KEPT_SAVE : 0u) | (kept.state[0] ? ORACLES_PAGE_KEPT_STATE : 0u)
                           | (kept.mods_save[0] ? ORACLES_PAGE_KEPT_MODS_SAVE : 0u) | (kept.mods_state[0] ? ORACLES_PAGE_KEPT_MODS_STATE : 0u)
                           | (kept.mods_store[0] ? ORACLES_PAGE_KEPT_MODS_STORE : 0u);
    oracles_page_import_done(moved, oracles_home_fan_game(game) != NULL, games->import.english, done, kept_line);
    status(nav, ORACLES_SAVE_FLOW_DONE, done, kept_line);
}

void oracles_home_save_host(OraclesHomeGames *games, OraclesHomeHost *host)
{
    (void)games;
    host->open_folder = open_folder;
    host->choose_save = choose_save;
    host->save_chosen = save_chosen;
    host->replace_save = replace_save;
}
