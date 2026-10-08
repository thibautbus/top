#include "home_games.h"

#include "bps.h"
#include "compositor.h"
#include "file_dialog.h"
#include "guest_tables.h"
#include "mod_folder.h"
#include "pause.h"
#include "rom.h"
#include "ui_page_layout.h"
#include "ui_page_nav.h"

#include <SDL3/SDL.h>
#include <sys/stat.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The surfaces a game plays at, for the window's scale: Faithful, --enhanced's band, the view drawn back. */
/* Room left on the display for the window's frame and title bar, which its usable area does not count. */
#define WINDOW_FRAME 64

void oracles_home_format_date(time_t when, char *out, size_t capacity)
{
    static const char *const months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    const struct tm *local = localtime(&when);
    if (!local) { snprintf(out, capacity, "?"); return; }
    snprintf(out, capacity, "%d %s %d", local->tm_mday, months[local->tm_mon % 12], local->tm_year + 1900);
}

/* Reads and identifies a ROM. Returns 1 with *info, or 0 with the loader's reason. */
static int identify(const char *path, OraclesRomInfo *info, char *error, size_t capacity)
{
    size_t size = 0;
    uint8_t *rom = oracles_rom_read_file(path, &size, error, capacity);
    if (!rom) return 0;
    const int ok = oracles_rom_identify(rom, size, info, error, capacity) == 0;
    free(rom);
    return ok;
}

static OraclesGame game_of(OraclesSettingsGame game) { return game == ORACLES_SETTINGS_AGES ? ORACLES_GAME_AGES : ORACLES_GAME_SEASONS; }

/* The file's name and its folder, whichever separator the path uses. */
static void split_path(const char *path, char *file, size_t file_capacity, char *folder, size_t folder_capacity)
{
    const char *slash = strrchr(path, '/'), *backslash = strrchr(path, '\\');
    if (backslash && (!slash || backslash > slash)) slash = backslash;
    const char *name = slash ? slash + 1 : path;
    /* A name or a folder longer than its field loses its middle, not its end. */
    oracles_ui_copy_middle(file, file_capacity, name, strlen(name));
    oracles_ui_copy_middle(folder, folder_capacity, path, slash ? (size_t)(slash - path) : 0);
}

static void find_save(const char *path, OraclesHomeGame *state);

/* The image each fan game's patch is made to give, by fan game. */
static const OraclesRomRevision fan_revisions[ORACLES_HOME_FAN_GAMES] = {
    [ORACLES_HOME_FAN_KINOMI] = ORACLES_ROM_REVISION_KINOMI_1_1_2, [ORACLES_HOME_FAN_MOONRISE] = ORACLES_ROM_REVISION_MOONRISE_1_0_6,
    [ORACLES_HOME_FAN_TEMPLE] = ORACLES_ROM_REVISION_TEMPLE_1_073 };
static unsigned fan_index(const OraclesHomeFanGame *fan) { return (unsigned)(fan - oracles_home_fan_games); }
static const char *fan_title(const OraclesHomeFanGame *fan) { return oracles_home_title(fan->hero); }
/* The setting a game's page chooses the ROM of: a fan game's base ROM is its Oracle's. */
static OraclesSettingsGame rom_setting(int game)
{
    const OraclesHomeFanGame *fan = oracles_home_fan_game(game);
    return (fan ? fan->base : game) == ORACLES_HOME_GAME_SEASONS ? ORACLES_SETTINGS_SEASONS : ORACLES_SETTINGS_AGES;
}

/* A game as the launcher finds it: its ROM identified, its save, its item hotkeys' mode. */
static void inspect(const OraclesHomeGames *games, OraclesSettingsGame g, OraclesHomeGame *state)
{
    const oracles_settings *settings = games->settings;
    memset(state, 0, sizeof *state);
    state->hotkeys = (int)settings->item_hotkeys[g];
    if (games->refused[g].path[0]) {
        /* A refused choice, shown and not kept: the ROM of settings.txt waits behind it. */
        split_path(games->refused[g].path, state->rom_file, sizeof state->rom_file, state->rom_folder, sizeof state->rom_folder);
        snprintf(state->rom_reason, sizeof state->rom_reason, "%s", games->refused[g].reason);
        state->rom = ORACLES_ROM_REFUSED;
        return;
    }
    const char *path = settings->rom[g];
    if (!path[0]) return;
    state->hotkeys_failed = !strcmp(games->hotkeys_failed[g], path);
    split_path(path, state->rom_file, sizeof state->rom_file, state->rom_folder, sizeof state->rom_folder);
    OraclesRomInfo info;
    if (!identify(path, &info, state->rom_reason, sizeof state->rom_reason)) state->rom = ORACLES_ROM_REFUSED;
    else if (info.game != game_of(g)) {
        /* A ROM of the other game, chosen under this one (or written so in settings.txt). */
        snprintf(state->rom_reason, sizeof state->rom_reason, "the header says %s, not %s", oracles_game_name(info.game), oracles_game_name(game_of(g)));
        state->rom = ORACLES_ROM_REFUSED;
    } else {
        state->rom = oracles_rom_is_original(&info) ? ORACLES_ROM_ORIGINAL : ORACLES_ROM_UNRECOGNISED;
        state->usable = 1;
    }
    find_save(path, state);
}

/* A fan game as the launcher finds it: its base, its Oracle's ROM as the Oracle's page shows it; its patch, read; the
 * image the patch makes of the base, identified, as a session builds it at each start; its save, beside the patch. */
static void inspect_fan(const OraclesHomeGames *games, const OraclesHomeFanGame *fan, OraclesHomeGame *state)
{
    const oracles_settings *settings = games->settings;
    const unsigned f = fan_index(fan);
    const OraclesSettingsGame base = rom_setting(fan->base);
    inspect(games, base, state);
    const int base_usable = state->usable;
    state->usable = 0;
    state->fan = fan;
    state->hotkeys = state->hotkeys_failed = 0;   /* no fan game's profile allows the item hotkeys yet */
    state->save_file[0] = state->save_written[0] = state->last_session[0] = 0;
    const char *patch = games->refused_patch[f].path[0] ? games->refused_patch[f].path : settings->patch[f];
    if (!patch[0]) return;
    split_path(patch, state->patch_file, sizeof state->patch_file, state->patch_folder, sizeof state->patch_folder);
    if (games->refused_patch[f].path[0]) {
        snprintf(state->patch_reason, sizeof state->patch_reason, "%s", games->refused_patch[f].reason);
        state->patch = ORACLES_ROM_REFUSED;
        return;
    }
    if (oracles_bps_check_file(patch, state->patch_reason, sizeof state->patch_reason) != 0) { state->patch = ORACLES_ROM_REFUSED; return; }
    state->patch = ORACLES_ROM_ORIGINAL;
    find_save(patch, state);
    if (!base_usable) return;
    size_t size = 0;
    uint8_t *image = oracles_bps_apply_files(settings->rom[base], patch, &size, state->image_reason, sizeof state->image_reason);
    OraclesRomInfo info;
    if (!image || oracles_rom_identify(image, size, &info, state->image_reason, sizeof state->image_reason) != 0) state->image = ORACLES_ROM_REFUSED;
    else {
        state->image = info.revision == fan_revisions[f] ? ORACLES_ROM_ORIGINAL : ORACLES_ROM_UNRECOGNISED;
        state->usable = 1;
    }
    free(image);
}

/* The save a game's file has beside it, and its dates. */
static void find_save(const char *path, OraclesHomeGame *state)
{
    char save[ORACLES_SETTINGS_PATH_LENGTH], folder[ORACLES_SETTINGS_PATH_LENGTH];
    oracles_session_default_save_path(path, save, sizeof save);
    struct stat file;
    if (stat(save, &file) != 0) return;
    split_path(save, state->save_file, sizeof state->save_file, folder, sizeof folder);
    oracles_home_format_date(file.st_mtime, state->last_session, sizeof state->last_session);
    const struct tm *local = localtime(&file.st_mtime);
    snprintf(state->save_written, sizeof state->save_written, "%s, %02d:%02d", state->last_session, local ? local->tm_hour : 0, local ? local->tm_min : 0);
}

/* Display's choices from the settings, and the size of the display the sizes are shown for: the window's, as Play
 * reduces the window on it. */
static void inspect_display(const oracles_settings *settings, OraclesHomeDisplay *display, struct SDL_Window *window)
{
    display->profile = settings->profile;
    display->transitions = settings->transitions;
    display->view = settings->view;
    display->window = settings->window_scale ? settings->window_scale - 2 : 3;
    display->colour = settings->colour_correction;
    display->vsync = !strcmp(settings->vsync, "on") ? 1 : !strcmp(settings->vsync, "off") ? 2 : 0;
    /* Without a window (a capture drawn offscreen), a 1080p screen. */
    SDL_Rect usable;
    const SDL_DisplayID index = window ? SDL_GetDisplayForWindow(window) : 0;
    const SDL_DisplayMode *mode = index ? SDL_GetDesktopDisplayMode(index) : NULL;
    if (mode && SDL_GetDisplayUsableBounds(index, &usable)) {
        /* In pixels, as the game's window is sized: SDL gives points on a Retina display; the frame is counted in
         * points of the launcher (oracles_sdl_point_scale). */
        const float ratio = oracles_sdl_pixel_ratio(window), frame = WINDOW_FRAME * oracles_sdl_point_scale(window);
        display->screen_w = (int)((float)mode->w * ratio + 0.5f);
        display->screen_h = (int)((float)mode->h * ratio + 0.5f);
        display->room_w = (int)((float)usable.w * ratio + 0.5f);
        display->room_h = (int)(((float)usable.h - frame) * ratio + 0.5f);   /* as fitting_scale reduces the window at Play */
    } else {
        display->screen_w = 1920;
        display->screen_h = 1080;
        display->room_w = 1920;
        display->room_h = 1080 - WINDOW_FRAME;
    }
}

/* Controls' names from the settings, and each game's slots by their items' names. */
static void inspect_controls(const oracles_settings *settings, OraclesHomeControls *c)
{
    for (int i = 0; i < ORACLES_HOME_BUTTONS; i++) snprintf(c->keys[i], sizeof c->keys[i], "%s", settings->key_names[i]);
    for (int i = 0; i < ORACLES_HOME_PAD_BUTTONS; i++) snprintf(c->pads[i], sizeof c->pads[i], "%s", settings->pad_names[i]);
    for (int i = 0; i < ORACLES_HOME_SLOTS; i++) {
        snprintf(c->hotkey_keys[i], sizeof c->hotkey_keys[i], "%s", settings->hotkey_key_names[i]);
        snprintf(c->hotkey_pads[i], sizeof c->hotkey_pads[i], "%s", settings->hotkey_pad_names[i]);
    }
    snprintf(c->hotkey_keys[ORACLES_HOME_SLOTS], sizeof c->hotkey_keys[0], "%s", settings->hotkey_bind_b);
    snprintf(c->hotkey_keys[ORACLES_HOME_SLOTS + 1], sizeof c->hotkey_keys[0], "%s", settings->hotkey_bind_a);
    for (int g = 0; g < 2; g++) {
        const OraclesGuestTables *tables = g == 0 ? &oracles_guest_tables_ages : &oracles_guest_tables_seasons;
        for (int i = 0; i < ORACLES_HOME_SLOTS; i++) {
            const OraclesHotkeySlot *slot = &settings->hotkeys[g][i];
            const char *name = slot->set && slot->item < ORACLES_GUEST_ITEM_LABELS ? tables->item_names[slot->item] : NULL;
            snprintf(c->items[g][i], sizeof c->items[g][i], "%s", name ? name : "");
        }
    }
}

static void store_controls(const OraclesHomeControls *c, oracles_settings *settings)
{
    for (int i = 0; i < ORACLES_HOME_BUTTONS; i++) snprintf(settings->key_names[i], sizeof settings->key_names[i], "%s", c->keys[i]);
    for (int i = 0; i < ORACLES_HOME_PAD_BUTTONS; i++) snprintf(settings->pad_names[i], sizeof settings->pad_names[i], "%s", c->pads[i]);
    for (int i = 0; i < ORACLES_HOME_SLOTS; i++) {
        snprintf(settings->hotkey_key_names[i], sizeof settings->hotkey_key_names[i], "%s", c->hotkey_keys[i]);
        snprintf(settings->hotkey_pad_names[i], sizeof settings->hotkey_pad_names[i], "%s", c->hotkey_pads[i]);
    }
    snprintf(settings->hotkey_bind_b, sizeof settings->hotkey_bind_b, "%s", c->hotkey_keys[ORACLES_HOME_SLOTS]);
    snprintf(settings->hotkey_bind_a, sizeof settings->hotkey_bind_a, "%s", c->hotkey_keys[ORACLES_HOME_SLOTS + 1]);
}

/* The mods of the mods folder, for each Oracle as its Mods page lists them, active as settings.txt says. */
static void read_mods(void *opaque, OraclesHomeNav *nav)
{
    const OraclesHomeGames *games = opaque;
    char folder[ORACLES_SETTINGS_PATH_LENGTH];
    oracles_settings_mods_folder(games->settings, folder, sizeof folder);
    oracles_ui_copy_middle(nav->mods_folder, sizeof nav->mods_folder, folder, strlen(folder));
    for (int g = 0; g < ORACLES_SETTINGS_GAMES; g++) {
        OraclesModEntry entries[ORACLES_HOME_MODS];
        OraclesHomeMods *mods = &nav->mods[g];
        mods->count = folder[0] ? (unsigned)oracles_mod_folder_list(folder, game_of((OraclesSettingsGame)g), entries, ORACLES_HOME_MODS) : 0u;
        for (unsigned i = 0; i < mods->count; i++) {
            OraclesHomeMod *m = &mods->mods[i];
            const char *line = entries[i].refusal[0] ? entries[i].refusal : entries[i].description;
            oracles_ui_copy_middle(m->name, sizeof m->name, entries[i].name, strnlen(entries[i].name, sizeof entries[i].name));
            oracles_ui_copy_middle(m->line, sizeof m->line, line, strlen(line));
            m->refused = entries[i].refusal[0] != 0;
            for (int h = 0; h < ORACLES_HOME_HOTKEY_GAMES; h++) m->houses[h] = (int)entries[i].houses[h];
            m->active = !m->refused && oracles_settings_mod_active(games->settings->mods[g], m->name);
        }
    }
}

static void refresh(void *opaque, OraclesHomeNav *nav, struct SDL_Window *window)
{
    const OraclesHomeGames *games = opaque;
    inspect_display(games->settings, &nav->display, window);
    inspect_controls(games->settings, &nav->controls);
    for (int g = 0; g < ORACLES_SETTINGS_GAMES; g++) {
        inspect(games, (OraclesSettingsGame)g, &nav->games[g]);
        if (nav->games[g].rom == ORACLES_ROM_REFUSED && !games->refused[g].path[0])
            fprintf(stderr, "oracles: %s ROM %s refused: %s\n", oracles_game_name(game_of((OraclesSettingsGame)g)), games->settings->rom[g], nav->games[g].rom_reason);
    }
    for (unsigned f = 0; f < ORACLES_HOME_FAN_GAMES; f++) {
        const OraclesHomeFanGame *fan = &oracles_home_fan_games[f];
        OraclesHomeGame *state = &nav->games[fan->game];
        inspect_fan(games, fan, state);
        if (state->patch == ORACLES_ROM_REFUSED && !games->refused_patch[f].path[0])
            fprintf(stderr, "oracles: %s patch %s refused: %s\n", fan_title(fan), games->settings->patch[f], state->patch_reason);
        if (state->image == ORACLES_ROM_REFUSED) fprintf(stderr, "oracles: %s does not build: %s\n", fan_title(fan), state->image_reason);
    }
    read_mods(opaque, nav);
}

static void store(void *opaque, const OraclesHomeNav *nav)
{
    OraclesHomeGames *games = opaque;
    oracles_settings *prefs = games->settings;
    for (int g = 0; g < ORACLES_SETTINGS_GAMES; g++) prefs->item_hotkeys[g] = (OraclesHotkeysMode)nav->games[g].hotkeys;
    store_controls(&nav->controls, prefs);
    prefs->profile = nav->display.profile;
    prefs->transitions = nav->display.transitions;
    prefs->view = nav->display.view;
    static const char *const vsync_names[3] = { "auto", "on", "off" };
    prefs->window_scale = nav->display.window < 3 ? nav->display.window + 2 : 0;
    prefs->colour_correction = nav->display.colour;
    snprintf(prefs->vsync, sizeof prefs->vsync, "%s", vsync_names[nav->display.vsync]);
    /* Each Oracle's active mods, those its Mods page switched on. */
    for (int g = 0; g < ORACLES_SETTINGS_GAMES; g++) {
        prefs->mods[g][0] = 0;
        for (unsigned i = 0; i < nav->mods[g].count; i++)
            if (nav->mods[g].mods[i].active) oracles_settings_mod_set(prefs->mods[g], nav->mods[g].mods[i].name, 1);
    }
    oracles_settings_store(games->settings);
}

/* A ROM for game `g`: kept in settings.txt when it is the game's, else shown refused under the game and not kept. */
/* Returns 1 when the ROM is kept. */
static int offer(OraclesHomeGames *games, OraclesSettingsGame g, const char *path)
{
    OraclesRomInfo info;
    char error[ORACLES_HOME_TEXT_LENGTH];
    int ok = identify(path, &info, error, sizeof error);
    if (ok && info.game != game_of(g)) {
        snprintf(error, sizeof error, "the header says %s, not %s", oracles_game_name(info.game), oracles_game_name(game_of(g)));
        ok = 0;
    }
    if (!ok) {
        snprintf(games->refused[g].path, sizeof games->refused[g].path, "%s", path);
        snprintf(games->refused[g].reason, sizeof games->refused[g].reason, "%s", error);
        fprintf(stderr, "oracles: %s ROM %s refused, not kept: %s\n", oracles_game_name(game_of(g)), path, error);
        return 0;
    }
    games->refused[g].path[0] = 0;
    snprintf(games->settings->rom[g], sizeof games->settings->rom[g], "%s", path);
    oracles_settings_store(games->settings);
    fprintf(stderr, "oracles: %s ROM set to %s (%s)\n", oracles_game_name(info.game), path, oracles_rom_is_original(&info) ? "original US" : "not an original, header accepted");
    return 1;
}

/* A patch for a fan game: kept in settings.txt when it is a BPS patch, else shown refused and not kept.  Whether it is
 * made for the Oracle's ROM, the image's line says. */
static int offer_patch(OraclesHomeGames *games, const OraclesHomeFanGame *fan, const char *path, char *error, size_t capacity)
{
    const unsigned f = fan_index(fan);
    if (oracles_bps_check_file(path, error, capacity) != 0) {
        snprintf(games->refused_patch[f].path, sizeof games->refused_patch[f].path, "%s", path);
        snprintf(games->refused_patch[f].reason, sizeof games->refused_patch[f].reason, "%s", error);
        fprintf(stderr, "oracles: %s patch %s refused, not kept: %s\n", fan_title(fan), path, error);
        return 0;
    }
    games->refused_patch[f].path[0] = 0;
    snprintf(games->settings->patch[f], sizeof games->settings->patch[f], "%s", path);
    oracles_settings_store(games->settings);
    fprintf(stderr, "oracles: %s patch set to %s\n", fan_title(fan), path);
    return 1;
}

/* The fan game a patch dropped on the home screen is for: the one whose image it makes of its Oracle's ROM, as chosen;
 * NULL when it makes none (no base ROM chosen yet, another patch). */
static const OraclesHomeFanGame *fan_of_patch(const OraclesHomeGames *games, const char *path)
{
    for (unsigned f = 0; f < ORACLES_HOME_FAN_GAMES; f++) {
        const char *base = games->settings->rom[rom_setting(oracles_home_fan_games[f].base)];
        if (!base[0]) continue;
        char error[ORACLES_HOME_TEXT_LENGTH];
        size_t size = 0;
        uint8_t *image = oracles_bps_apply_files(base, path, &size, error, sizeof error);
        OraclesRomInfo info;
        const int made = image && oracles_rom_identify(image, size, &info, error, sizeof error) == 0 && info.revision == fan_revisions[f];
        free(image);
        if (made) return &oracles_home_fan_games[f];
    }
    return NULL;
}

/* Opens the system's dialog of `kind` in the folder of `current`; the answer comes to file_chosen. */
static void choose(struct SDL_Window *window, OraclesDialogKind kind, const char *current, int game)
{
    char file[ORACLES_HOME_TEXT_LENGTH], folder[ORACLES_SETTINGS_PATH_LENGTH];
    split_path(current, file, sizeof file, folder, sizeof folder);
    oracles_choose_file(window, kind, folder, game);
}

static void choose_rom(void *opaque, int game, struct SDL_Window *window)
{
    const OraclesHomeGames *games = opaque;
    choose(window, ORACLES_DIALOG_ROM, games->settings->rom[rom_setting(game)], game);
}

static void choose_patch(void *opaque, int game, struct SDL_Window *window)
{
    const OraclesHomeGames *games = opaque;
    const OraclesHomeFanGame *fan = oracles_home_fan_game(game);
    if (!fan) return;
    /* The dialog opens where the patch is, or else where its Oracle's ROM is. */
    const unsigned f = fan_index(fan);
    const char *current = games->settings->patch[f][0] ? games->settings->patch[f] : games->settings->rom[rom_setting(game)];
    choose(window, ORACLES_DIALOG_PATCH, current, game);
}

static void file_chosen(void *opaque, int game, int kind, int result, const char *path, int copied, char *message, size_t capacity)
{
    OraclesHomeGames *games = opaque;
    char error[ORACLES_HOME_TEXT_LENGTH];
    switch ((OraclesDialogResult)result) {
        case ORACLES_DIALOG_UNAVAILABLE:
            snprintf(message, capacity, "No file dialog on this system: drop the %s on this window", kind == ORACLES_DIALOG_PATCH ? "patch" : "ROM");
            return;
        case ORACLES_DIALOG_FAILED:
            snprintf(message, capacity, "The %s cannot be copied into the application's folder", kind == ORACLES_DIALOG_PATCH ? "patch" : "ROM");
            return;
        case ORACLES_DIALOG_CANCELLED: return;
        case ORACLES_DIALOG_CHOSEN: break;
    }
    const OraclesHomeFanGame *fan = oracles_home_fan_game(game);
    const int kept = kind == ORACLES_DIALOG_PATCH ? fan && offer_patch(games, fan, path, error, sizeof error)   /* the page's fan game */
                                                  : offer(games, rom_setting(game), path);
    /* A copy made for this choice and refused goes (Android): the application's folder keeps only what it uses. */
    if (!kept && copied && remove(path) == 0) fprintf(stderr, "oracles: %s removed, refused\n", path);
}

static void open_folder(void *opaque, int game, char *message, size_t capacity)
{
    const OraclesHomeGames *games = opaque;
    char file[ORACLES_HOME_TEXT_LENGTH], folder[ORACLES_SETTINGS_PATH_LENGTH], error[256];
    /* The folder of the save: beside the ROM, or a fan game's patch. */
    const OraclesHomeFanGame *fan = oracles_home_fan_game(game);
    split_path(fan ? games->settings->patch[fan_index(fan)] : games->settings->rom[rom_setting(game)], file, sizeof file, folder, sizeof folder);
#ifdef __ANDROID__
    /* Android opens no file manager on a folder: the toast says where the saves are, from the shared storage's root, as
     * a computer sees it over USB. */
    (void)error;
    const char *shown = strstr(folder, "Android/data/");
    snprintf(message, capacity, "Saves are in %s, reachable over USB", shown ? shown : folder);
#else
    if (oracles_open_folder(folder[0] ? folder : ".", error, sizeof error) != 0) snprintf(message, capacity, "The folder does not open: %s", error);
#endif
}

/* The mods folder, made when it is not there yet, in the system's file manager. */
static void open_mods(void *opaque, char *message, size_t capacity)
{
    const OraclesHomeGames *games = opaque;
    char folder[ORACLES_SETTINGS_PATH_LENGTH], error[256];
    oracles_settings_mods_folder(games->settings, folder, sizeof folder);
    if (!folder[0]) { snprintf(message, capacity, "No settings folder: the launcher has no mods folder"); return; }
    if (!SDL_CreateDirectory(folder)) { snprintf(message, capacity, "The mods folder cannot be made: %s", SDL_GetError()); return; }
#ifdef __ANDROID__
    (void)error;
    snprintf(message, capacity, "Mods go in %s", folder);
#else
    if (oracles_open_folder(folder, error, sizeof error) != 0) snprintf(message, capacity, "The folder does not open: %s", error);
#endif
}

/* A patch rather than a ROM: its name ends in .bps, or it begins as one. */
static int looks_like_patch(const char *path)
{
    const size_t length = strlen(path);
    if (length >= 4 && path[length - 4] == '.' && tolower((unsigned char)path[length - 3]) == 'b'
        && tolower((unsigned char)path[length - 2]) == 'p' && tolower((unsigned char)path[length - 1]) == 's') return 1;
    char magic[4] = { 0 };
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    const size_t read = fread(magic, 1, sizeof magic, f);
    fclose(f);
    return read == sizeof magic && !memcmp(magic, "BPS1", 4);
}

static int drop(void *opaque, const char *path, int page_game, char *message, size_t capacity)
{
    OraclesHomeGames *games = opaque;
    char error[200];
    /* A patch goes to the fan game whose page is open, which shows it; elsewhere to the fan game whose image it makes of
     * its Oracle's ROM, a toast saying so, and the home screen then shows the list of fan games. */
    if (looks_like_patch(path)) {
        const OraclesHomeFanGame *fan = oracles_home_fan_game(page_game);
        if (!fan && oracles_bps_check_file(path, error, sizeof error) != 0) {
            snprintf(message, capacity, "Refused: %s.", error);
            return -1;
        }
        if (!fan) fan = fan_of_patch(games, path);
        if (!fan) {
            snprintf(message, capacity, "No fan game is made of this patch and the ROMs chosen: drop it on its page");
            return page_game >= 0 ? -1 : ORACLES_HOME_FAN;
        }
        const int kept = offer_patch(games, fan, path, error, sizeof error);
        if (page_game == fan->game) return -1;
        if (kept) snprintf(message, capacity, "%s patch set", fan_title(fan));
        else snprintf(message, capacity, "Refused: %s.", error);
        return page_game >= 0 ? -1 : ORACLES_HOME_FAN;
    }
    /* On a game's page a file dropped is a ROM for that game, as Choose ROM's (a fan game's base ROM): refused, it
     * shows there with why. */
    if (page_game >= 0) { offer(games, rom_setting(page_game), path); return -1; }
    /* On the home screen it goes under the game it is, or a toast says why it is none. */
    OraclesRomInfo info;
    if (!identify(path, &info, error, sizeof error)) {
        fprintf(stderr, "oracles: %s refused: %s\n", path, error);
        snprintf(message, capacity, "Refused: %s.", error);
        return -1;
    }
    const OraclesSettingsGame g = info.game == ORACLES_GAME_AGES ? ORACLES_SETTINGS_AGES : ORACLES_SETTINGS_SEASONS;
    offer(games, g, path);
    snprintf(message, capacity, "%s ROM set%s", oracles_game_name(info.game), oracles_rom_is_original(&info) ? "" : ": unrecognised, Faithful only");
    return g == ORACLES_SETTINGS_AGES ? ORACLES_HOME_AGES : ORACLES_HOME_SEASONS;
}

/* The largest whole scale, up to the one asked for, at which the surface's window fits the display. */
static unsigned fitting_scale(struct SDL_Window *window, unsigned wanted, OraclesEnhancedSize surface)
{
    SDL_Rect usable;
    const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (!display || !SDL_GetDisplayUsableBounds(display, &usable)) return wanted;
    unsigned scale = wanted;
    /* The usable area in pixels, as the window is sized (points on a Retina display), less the window's frame. */
    const float ratio = oracles_sdl_pixel_ratio(window), frame = WINDOW_FRAME * oracles_sdl_point_scale(window);
    const int room_w = (int)((float)usable.w * ratio + 0.5f), room_h = (int)(((float)usable.h - frame) * ratio + 0.5f);
    while (scale > 1u && ((int)(scale * surface.width) > room_w || (int)(scale * surface.height) > room_h)) scale--;
    return scale;
}

static int start(void *opaque, OraclesHomeCommand game, struct SDL_Window *window, struct SDL_Renderer *renderer,
                 OraclesUiDraw *draw, char *message, size_t capacity)
{
    OraclesHomeGames *games = opaque;
    /* A fan game plays from its Oracle's ROM and its patch, the image built by the session. */
    const OraclesHomeFanGame *fan = oracles_home_fan_game_started(game);
    const int with_mods = game == ORACLES_HOME_START_AGES_MODS || game == ORACLES_HOME_START_SEASONS_MODS;
    const OraclesSettingsGame g = fan ? rom_setting(fan->base)
                                : game == ORACLES_HOME_START_SEASONS || game == ORACLES_HOME_START_SEASONS_MODS ? ORACLES_SETTINGS_SEASONS : ORACLES_SETTINGS_AGES;
    const char *name = fan ? fan_title(fan) : oracles_game_name(game_of(g));
    OraclesSessionOptions o = *games->options;
    o.rom_path = games->settings->rom[g];
    o.patch_path = fan ? games->settings->patch[fan_index(fan)] : NULL;
    /* --record given to the home screen records the first session it starts, then is spent. */
    if (games->route_recorded) o.record_path = NULL;
    /* Mods' Play: the game with the mods its page made active, on its own save (NAME.mods.sav); Start game plays it
     * without them, on its usual save.  Mods given on the command line win, as its other options do. */
    char mod_dirs[ORACLES_SETTINGS_MODS_MAX][ORACLES_SETTINGS_PATH_LENGTH];
    if (with_mods && !o.mods_count) oracles_session_home_mods(&o, games->settings, g, mod_dirs);
    /* Display's choices, for every game: the profile the ROM allows (an unrecognised ROM plays in Faithful), Enhanced
     * being the view drawn back, the transitions in Enhanced only; and the game's item hotkeys.
     * A view given on the command line wins, as --rom would play it: --enhanced alone is still the 256x144 band. */
    OraclesHomeGame state;
    if (fan) inspect_fan(games, fan, &state);
    else inspect(games, g, &state);
    const int command_line_view = o.enhanced || o.zoom_out || o.view || o.continuous_transitions;
    const OraclesProfile chosen = command_line_view ? ORACLES_PROFILE_ENHANCED : games->settings->profile;
    const int effective = oracles_page_profile(&state, chosen);
    const OraclesProfile profile = effective < 0 ? ORACLES_PROFILE_FAITHFUL : (OraclesProfile)effective;
    if (profile != chosen) fprintf(stderr, "oracles: an unrecognised ROM plays in the Faithful profile, not %s\n", oracles_settings_profile_name(chosen));
    static const char *const hotkey_names[3] = { "off", "use", "equip" };
    /* Display's Continuous transitions take Link swimming too; --continuous-transitions alone is
     * the command line's, for the routes recorded with it, and so is --continuous-swim. */
    o.continuous_swim = profile != ORACLES_PROFILE_FAITHFUL && (o.continuous_swim || games->settings->transitions);
    o.continuous_transitions = profile != ORACLES_PROFILE_FAITHFUL && (o.continuous_transitions || games->settings->transitions);
    /* No fan game's profile allows the item hotkeys yet: off, whatever the command line gave for the Oracles. */
    if (fan) o.hotkeys_option = hotkey_names[0];
    else if (!o.hotkeys_option) o.hotkeys_option = hotkey_names[state.hotkeys];
    if (profile == ORACLES_PROFILE_FAITHFUL) o.enhanced = o.zoom_out = o.view = 0;
    else if (!command_line_view) { o.enhanced = 1; o.view = games->settings->view + 1; }   /* Display's View */
    else o.enhanced = 1;
    /* Display shows no camera: a game started here in Enhanced takes profile 2, the smooth camera. The camera key of
     * settings.txt stays for the command line and hand editing, and --camera given to the launcher still wins. */
    if (!o.camera_profile) o.camera_profile = 2;
    o.sdl.window = window;
    o.sdl.renderer = renderer;
    /* Display's window, unless the command line gave one; a window scale is lowered to what the display holds. */
    if (!games->window_from_command_line) {
        o.sdl.fullscreen = games->settings->window_scale == 0;
        if (games->settings->window_scale) o.sdl.scale = (unsigned)games->settings->window_scale;
    }
    if (oracles_sdl_fullscreen_only()) o.sdl.fullscreen = 1;   /* Android: the whole screen, whatever Display says */
    /* The view's surface: its level in the screen's shape (as the session takes it). */
    o.screen_4_3 = oracles_sdl_screen_4_3(window);
    const OraclesEnhancedLevel level = o.view ? (OraclesEnhancedLevel)(o.view - 1) : o.zoom_out ? ORACLES_ENHANCED_FAR : ORACLES_ENHANCED_NEAR;
    const OraclesEnhancedSize surface = !o.enhanced ? (OraclesEnhancedSize){ 160u, 144u }
                                      : oracles_enhanced_view_size(level, o.screen_4_3 ? ORACLES_ENHANCED_4_3 : ORACLES_ENHANCED_16_9);
    o.sdl.scale = fitting_scale(window, o.sdl.scale, surface);
    char window_text[48];
    if (o.sdl.fullscreen) snprintf(window_text, sizeof window_text, "fullscreen");
    else snprintf(window_text, sizeof window_text, "windowed at scale %u", o.sdl.scale);
    char surface_text[24];
    snprintf(surface_text, sizeof surface_text, "%ux%u", surface.width, surface.height);
    fprintf(stderr, "oracles: starting %s in the %s profile (%s), %s, continuous transitions %s, item hotkeys %s, %u mod%s\n", name,
            oracles_settings_profile_name(profile), surface_text,
            window_text, o.continuous_swim ? "on, swimming too" : o.continuous_transitions ? "on" : "off", o.hotkeys_option, o.mods_count,
            o.mods_count == 1 ? "" : "s");
    /* Escape opens the pause menu over the game, its settings read and written as the home screen's are. */
    OraclesHomeHost pause_host;
    oracles_home_games_host(games, &pause_host);
    const int home_game = fan ? fan->game : g == ORACLES_SETTINGS_AGES ? ORACLES_HOME_GAME_AGES : ORACLES_HOME_GAME_SEASONS;
    OraclesPause *pause = draw ? oracles_pause_create(draw, &pause_host, home_game, profile) : NULL;
    o.pause = pause;
    OraclesSessionResult result;
    oracles_session_run(&o, games->settings, &result);
    oracles_pause_destroy(pause);
    if (result.route_written) games->route_recorded = 1;
    /* Item hotkeys that could not attach to this ROM: Cartridge says so under its status, while it is the game's ROM. */
    if (result.hotkeys_dropped && !fan) snprintf(games->hotkeys_failed[g], sizeof games->hotkeys_failed[g], "%s", o.rom_path);
    /* Back on the home screen, a toast: why the game stopped, else that it played without the item hotkeys, and where
     * --record wrote its route. */
    static const char dropped[] = "Item hotkeys could not attach to this ROM: the game played without them.";
    if (result.error[0]) snprintf(message, capacity, "The game stopped: %s.", result.error);
    else if (result.hotkeys_dropped && result.route_written) snprintf(message, capacity, "%s Route written to %s", dropped, o.record_path);
    else if (result.hotkeys_dropped) snprintf(message, capacity, "%s", dropped);
    else if (result.route_written) snprintf(message, capacity, "Route written to %s", o.record_path);
    return result.window_closed;
}

void oracles_home_games_host(OraclesHomeGames *games, OraclesHomeHost *host)
{
    memset(host, 0, sizeof *host);
    host->opaque = games;
    host->refresh = refresh;
    host->drop = drop;
    host->store = store;
    host->choose_rom = choose_rom;
    host->choose_patch = choose_patch;
    host->file_chosen = file_chosen;
    host->open_folder = open_folder;
    host->read_mods = read_mods;
    host->open_mods = open_mods;
    host->start = start;
}
