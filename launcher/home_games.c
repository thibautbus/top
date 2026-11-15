#include "filter_preview.h"
#include "home_games.h"

#include "home_choices.h"
#include "bps.h"
#include "compositor.h"
#include "file_dialog.h"
#include "guest.h"
#include "guest_tables.h"
#include "language.h"
#include "mod_folder.h"
#include "pause.h"
#include "profile.h"
#include "rom.h"
#include "save_transfer.h"
#include "sha1.h"
#include "ui_page_layout.h"
#include "ui_page_nav.h"
#ifdef ORACLES_RETROACHIEVEMENTS
#include "achievements_load.h"
#include "ui_achievements.h"
#endif

#include <SDL3/SDL.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void oracles_home_format_date(time_t when, char *out, size_t capacity)
{
    static const char *const months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    const struct tm *local = localtime(&when);
    if (!local) { snprintf(out, capacity, "?"); return; }
    snprintf(out, capacity, "%d %s %d", local->tm_mday, months[local->tm_mon % 12], local->tm_year + 1900);
}

/* The text box's font of an original ROM of Ages or Seasons, its printable ASCII characters (OraclesHomeFont), where the
 * game's tables say it starts. */
static void read_font(const uint8_t *data, size_t size, const OraclesRomInfo *info, OraclesHomeFont *out)
{
    memset(out, 0, sizeof *out);
    if (!oracles_rom_is_original(info)) return;
    const OraclesGuestTables *tables = info->game == ORACLES_GAME_SEASONS ? &oracles_guest_tables_seasons : &oracles_guest_tables_ages;
    const OraclesRom rom = { data, size };
    const size_t start = oracles_rom_offset(tables->font_start.bank, tables->font_start.addr);
    OraclesTextFont font;
    if (oracles_text_font(&rom, start, &font) != ORACLES_OK) return;
    for (unsigned i = 0; i < ORACLES_HOME_FONT_CHARACTERS; i++) {
        const unsigned c = ORACLES_HOME_FONT_FIRST + i;
        for (unsigned row = 0; row < 16u; row++) out->rows[i][row] = (uint8_t)(data[start + 16u * c + row] ^ 0xffu);
        out->left[i] = font.left[c];
        out->width[i] = font.width[c];
        out->advance[i] = font.advance[c];
    }
    out->read = 1;
}

/* Reads (once, home_files.h) and identifies a ROM, and reads its font when it is an original (`font`, NULL: not).
 * Returns 1 with *info, or 0 with the loader's reason. */
static int identify(OraclesHomeGames *games, const char *path, OraclesRomInfo *info, OraclesHomeFont *font, char *error, size_t capacity)
{
    size_t size = 0;
    const uint8_t *rom = oracles_home_file(&games->files, path, ORACLES_ZIP_ROM, &size, error, capacity);
    if (!rom) return 0;
    const int ok = oracles_rom_identify(rom, size, info, error, capacity) == 0;
    if (ok && font) read_font(rom, size, info, font);
    return ok;
}

/* A patch's file checked as a BPS patch, read once: 0, or -1 with the reason. */
static int check_patch(OraclesHomeGames *games, const char *path, char *error, size_t capacity)
{
    size_t size = 0;
    const uint8_t *patch = oracles_home_file(&games->files, path, ORACLES_ZIP_PATCH, &size, error, capacity);
    return patch ? oracles_bps_check(patch, size, error, capacity) : -1;
}

/* The image a patch makes of a base ROM, with the ROM at `joined_path` joined after it (NULL: none), as a session builds
 * it, the files read once; the caller frees it.  NULL with the reason. */
static uint8_t *build_image(OraclesHomeGames *games, const char *base_path, const char *joined_path, const char *patch_path, size_t *size,
                            char *error, size_t capacity)
{
    size_t base_size = 0, second_size = 0, patch_size = 0;
    const uint8_t *base = oracles_home_file(&games->files, base_path, ORACLES_ZIP_ROM, &base_size, error, capacity);
    const uint8_t *second = base && joined_path ? oracles_home_file(&games->files, joined_path, ORACLES_ZIP_ROM, &second_size, error, capacity) : NULL;
    if (!base || (joined_path && !second)) return NULL;
    uint8_t *joined = second ? oracles_bps_join(base, base_size, second, second_size, &base_size, error, capacity) : NULL;
    if (second && !joined) return NULL;
    const uint8_t *patch = oracles_home_file(&games->files, patch_path, ORACLES_ZIP_PATCH, &patch_size, error, capacity);
    uint8_t *image = patch ? oracles_bps_apply(joined ? joined : base, base_size, patch, patch_size, size, error, capacity) : NULL;
    free(joined);
    return image;
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

static unsigned fan_index(const OraclesHomeFanGame *fan) { return (unsigned)(fan - oracles_home_fan_games); }
/* Whether an image is the one fan game `f`'s patch is made to give. */
static int fan_made(unsigned f, const OraclesRomInfo *info)
{
    if (f == ORACLES_HOME_FAN_COMBINED) return oracles_rom_combined_language(info) != NULL;
    return info->known && info->revision == oracles_home_fan_revisions[f];
}
static const char *fan_title(const OraclesHomeFanGame *fan) { return oracles_home_title(fan->hero); }
/* The setting a game's page chooses the ROM of: a fan game's base ROM is its Oracle's. */
static OraclesSettingsGame rom_setting(int game)
{
    const OraclesHomeFanGame *fan = oracles_home_fan_game(game);
    return (fan ? fan->base : game) == ORACLES_HOME_GAME_SEASONS ? ORACLES_SETTINGS_SEASONS : ORACLES_SETTINGS_AGES;
}
/* The file a fan game's patch applies to: its Oracle's ROM, or for one made on the two European ROMs joined Ages', with
 * Seasons' in *joined (else NULL). */
static const char *fan_base(const OraclesHomeGames *games, const OraclesHomeFanGame *fan, const char **joined)
{
    *joined = fan->joined ? games->settings->eu_rom[ORACLES_SETTINGS_SEASONS] : NULL;
    return fan->joined ? games->settings->eu_rom[ORACLES_SETTINGS_AGES] : games->settings->rom[rom_setting(fan->base)];
}

/* The file at `path` now (*there 0 when it is not), and whether `found` holds it as it is. */
static int still_found(const OraclesHomeFound *found, const char *path, struct stat *file, int *there)
{
    *there = stat(path, file) == 0;
    return *there && !strcmp(found->path, path) && found->size == (long long)file->st_size && found->modified == (long long)file->st_mtime;
}

/* What the file at `path` was found to be, kept in `found` while it is there, until it changes. */
static void keep_found(OraclesHomeFound *found, const char *path, const struct stat *file, int there, OraclesRomState state, const char *reason)
{
    found->path[0] = 0;
    if (!there) return;
    snprintf(found->path, sizeof found->path, "%s", path);
    found->size = (long long)file->st_size;
    found->modified = (long long)file->st_mtime;
    found->state = state;
    snprintf(found->reason, sizeof found->reason, "%s", reason);
    found->combined = 0;
    found->language = NULL;
}

/* Why a ROM identified as `info` is not one of `game`'s: the other game's, or an image of both, which plays under Ages. */
static void other_game(const OraclesRomInfo *info, OraclesGame game, char *reason, size_t capacity)
{
    if (oracles_rom_is_combined(info)) snprintf(reason, capacity, "an image of both games, which plays under %s", oracles_game_name(info->game));
    else snprintf(reason, capacity, "the header says %s, not %s", oracles_game_name(info->game), oracles_game_name(game));
}

/* A game's ROM: ORIGINAL, UNRECOGNISED for another ROM of the game, REFUSED with the reason (the loader's, or that it
 * is the other game's); kept in games->rom_found[g] while the file is the same, with whether it is an image of both
 * games (*combined). */
static OraclesRomState rom_found(OraclesHomeGames *games, OraclesSettingsGame g, const char *path, char *reason, size_t capacity, int *combined)
{
    struct stat file;
    int there;
    OraclesHomeFound *found = &games->rom_found[g];
    *combined = 0;
    if (!still_found(found, path, &file, &there)) {
        OraclesRomInfo info;
        OraclesRomState state = ORACLES_ROM_REFUSED;
        int both = 0;
        if (identify(games, path, &info, &games->rom_font[g], reason, capacity)) {
            if (info.game == game_of(g)) state = oracles_rom_is_original(&info) ? ORACLES_ROM_ORIGINAL : ORACLES_ROM_UNRECOGNISED;
            /* A ROM of the other game, chosen under this one (or written so in settings.txt). */
            else other_game(&info, game_of(g), reason, capacity);
            both = state == ORACLES_ROM_UNRECOGNISED && oracles_rom_is_combined(&info);
        }
        keep_found(found, path, &file, there, state, state == ORACLES_ROM_REFUSED ? reason : "");
        found->combined = both;
        if (!there) return state;
    }
    snprintf(reason, capacity, "%s", found->reason);
    *combined = found->combined;
    return found->state;
}

/* A game as the launcher finds it: its ROM identified, its save, its item hotkeys' mode. */
/* Cheats' rows are the guest's cheats, bit for bit. */
_Static_assert(ORACLES_CHEAT_NO_DAMAGE == 1u && ORACLES_CHEAT_AMMO == 2u && ORACLES_CHEAT_RUPEES == 4u && ORACLES_CHEATS == ORACLES_CHEATS_ROWS,
               "Cheats' rows are the cheats' bits");

static void inspect(OraclesHomeGames *games, OraclesSettingsGame g, OraclesHomeGame *state)
{
    const oracles_settings *settings = games->settings;
    memset(state, 0, sizeof *state);
    state->refuses = oracles_home_refusals(game_of(g), g == ORACLES_SETTINGS_AGES ? ORACLES_ROM_REVISION_AGES_US : ORACLES_ROM_REVISION_SEASONS_US);
    state->hotkeys = (int)settings->item_hotkeys[g];
    state->cheats = settings->cheats[g];
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
    state->rom = rom_found(games, g, path, state->rom_reason, sizeof state->rom_reason, &state->combined);
    state->usable = state->rom != ORACLES_ROM_REFUSED;
    if (state->rom == ORACLES_ROM_ORIGINAL) state->font = games->rom_font[g];
    find_save(path, state);
}

/* What a European ROM is for Oracle `game`: ORIGINAL when it is the game's own, UNRECOGNISED for another file, REFUSED
 * with the loader's reason when it cannot be read. */
static OraclesRomState eu_state(OraclesHomeGames *games, const char *path, OraclesGame game, char *reason, size_t capacity)
{
    size_t size = 0;
    const uint8_t *rom = oracles_home_file(&games->files, path, ORACLES_ZIP_ROM, &size, reason, capacity);
    if (!rom) return ORACLES_ROM_REFUSED;
    OraclesRomInfo info;
    char error[ORACLES_HOME_TEXT_LENGTH];
    const int own = oracles_rom_identify(rom, size, &info, error, sizeof error) == 0 && oracles_language_source_game(&info) == game;
    return own ? ORACLES_ROM_ORIGINAL : ORACLES_ROM_UNRECOGNISED;
}

/* What the file at `path` is for Oracle `game` in `language`, Japanese or Polish, as eu_state says it of a European ROM:
 * the language's ROM of the game, or for Polish Kaxus's patch that makes it (a .bps, or a zip of one), by its SHA-1. */
static OraclesRomState own_rom_state(OraclesHomeGames *games, const char *path, OraclesGame game, OraclesLanguage language, char *reason, size_t capacity)
{
    size_t size = 0;
    const int patch = language == ORACLES_LANGUAGE_POLISH && oracles_zip_is_patch_file(path) == 1;
    const uint8_t *data = oracles_home_file(&games->files, path, patch ? ORACLES_ZIP_PATCH : ORACLES_ZIP_ROM, &size, reason, capacity);
    if (!data) return ORACLES_ROM_REFUSED;
    if (patch) {
        char sha1[41];
        oracles_sha1_hex(data, size, sha1);
        return oracles_language_patch_game(sha1, language) == game ? ORACLES_ROM_ORIGINAL : ORACLES_ROM_UNRECOGNISED;
    }
    OraclesRomInfo info;
    char error[ORACLES_HOME_TEXT_LENGTH];
    const int own = oracles_rom_identify(data, size, &info, error, sizeof error) == 0 && oracles_language_source_game_of(&info, language) == game;
    return own ? ORACLES_ROM_ORIGINAL : ORACLES_ROM_UNRECOGNISED;
}

const char *oracles_home_own_source(OraclesHomeGames *games, OraclesSettingsGame g, int own, const char *given)
{
    const int polish = own == ORACLES_SETTINGS_POLISH;
    char reason[ORACLES_HOME_TEXT_LENGTH];
    if (given && own_rom_state(games, given, game_of(g), polish ? ORACLES_LANGUAGE_POLISH : ORACLES_LANGUAGE_JAPANESE, reason, sizeof reason)
                     == ORACLES_ROM_ORIGINAL)
        return given;
    const char *const own_rom = polish ? games->settings->pl_rom[g] : games->settings->jp_rom[g];
    return own_rom[0] ? own_rom : NULL;
}

void oracles_home_own_language(OraclesHomeGames *games, OraclesSettingsGame g, int own, int recognized, OraclesSessionOptions *o, char *note,
                               size_t capacity)
{
    if (!own || (o->language && o->eu_rom_path)) return;
    note[0] = 0;
    const int polish = own == ORACLES_SETTINGS_POLISH;
    const char *const edition = polish ? "Polish" : "Japanese", *const code = oracles_settings_language_names[own];
    const char *const oracle_name = oracles_game_name(game_of(g)), *const given = o->eu_rom_path;
    o->eu_rom_path = oracles_home_own_source(games, g, own, given);
    if (given && o->eu_rom_path != given)
        fprintf(stderr, "oracles: --eu-rom %s is not the %s ROM of %s%s, which language_%s=%s plays from: set aside\n", given, edition, oracle_name,
                polish ? " nor its patch" : "", g == ORACLES_SETTINGS_SEASONS ? "seasons" : "ages", code);
    oracles_home_files_forget(&games->files);
    if (!recognized) return;
    char reason[ORACLES_HOME_TEXT_LENGTH] = "";
    const char *const key = polish ? (game_of(g) == ORACLES_GAME_AGES ? "pl_rom_ages" : "pl_rom_seasons")
                                   : (game_of(g) == ORACLES_GAME_AGES ? "jp_rom_ages" : "jp_rom_seasons");
    const OraclesRomState found = o->eu_rom_path ? own_rom_state(games, o->eu_rom_path, game_of(g), polish ? ORACLES_LANGUAGE_POLISH : ORACLES_LANGUAGE_JAPANESE,
                                                                 reason, sizeof reason)
                                                 : ORACLES_ROM_NONE;
    o->language = found == ORACLES_ROM_ORIGINAL ? code : NULL;
    if (found == ORACLES_ROM_UNRECOGNISED)
        snprintf(note, capacity, "%s is not the %s ROM of %s%s: the game played in English.", o->eu_rom_path, edition, oracle_name, polish ? " nor its patch" : "");
    else if (found == ORACLES_ROM_NONE) {
        snprintf(note, capacity, "No %s ROM of %s: the game played in English. Choose it in Cartridge \xe2\x80\xba Editions.", edition, oracle_name);
        fprintf(stderr, "oracles: %s= in settings.txt names no %s ROM\n", key, edition);
    }
    else if (found != ORACLES_ROM_ORIGINAL)
        snprintf(note, capacity, "The %s ROM of %s cannot be read (%s): the game played in English.", edition, oracle_name, reason);
    if (note[0]) fprintf(stderr, "oracles: %s\n", note);
    oracles_home_files_forget(&games->files);
}

/* eu_state, kept in games->eu_found[g] while the file is the same: read again only once it changed. */
static OraclesRomState eu_found(OraclesHomeGames *games, OraclesSettingsGame g, const char *path, char *reason, size_t capacity)
{
    struct stat file;
    int there;
    OraclesHomeFound *found = &games->eu_found[g];
    if (still_found(found, path, &file, &there)) {
        snprintf(reason, capacity, "%s", found->reason);
        return found->state;
    }
    const OraclesRomState state = eu_state(games, path, game_of(g), reason, capacity);
    keep_found(found, path, &file, there, state, reason);
    return state;
}

/* The game a file is the European ROM of, ORACLES_GAME_UNKNOWN when it is none. */
static OraclesGame european_game(OraclesHomeGames *games, const char *path)
{
    OraclesRomInfo info;
    char error[ORACLES_HOME_TEXT_LENGTH];
    return identify(games, path, &info, NULL, error, sizeof error) ? oracles_language_source_game(&info) : ORACLES_GAME_UNKNOWN;
}

/* Oracle `g`'s Japanese ROM (`edition` 0) or Polish source (1) as settings.txt names it (jp_rom_<game>=, pl_rom_<game>=),
 * kept in games->edition_found while the file is the same; the Polish source as a patched ROM, Kaxus's .bps or its zip. */
static void inspect_edition(OraclesHomeGames *games, OraclesSettingsGame g, int edition, OraclesHomeEdition *e)
{
    const char *path = edition ? games->settings->pl_rom[g] : games->settings->jp_rom[g];
    memset(e, 0, sizeof *e);
    if (!path[0]) return;
    split_path(path, e->file, sizeof e->file, e->folder, sizeof e->folder);
    if (edition && oracles_zip_is_patch_file(path) == 1) {
        const size_t length = strlen(path);
        e->patch = length > 4 && !SDL_strcasecmp(path + length - 4, ".zip") ? 2 : 1;
    }
    struct stat file;
    int there;
    OraclesHomeFound *found = &games->edition_found[edition][g];
    if (still_found(found, path, &file, &there)) {
        snprintf(e->reason, sizeof e->reason, "%s", found->reason);
        e->state = found->state;
        return;
    }
    e->state = own_rom_state(games, path, game_of(g), edition ? ORACLES_LANGUAGE_POLISH : ORACLES_LANGUAGE_JAPANESE, e->reason, sizeof e->reason);
    keep_found(found, path, &file, there, e->state, e->reason);
}

/* An Oracle's European ROM as settings.txt names it, its Japanese ROM and its Polish source, and its language as
 * settings.txt keeps it, which it plays in once its source is recognized (oracles_page_language). */
static void inspect_eu(OraclesHomeGames *games, OraclesSettingsGame g, OraclesHomeGame *state)
{
    const char *path = games->settings->eu_rom[g];
    state->eu = ORACLES_ROM_NONE;
    state->eu_file[0] = state->eu_folder[0] = state->eu_reason[0] = 0;
    for (int e = 0; e < 2; e++) inspect_edition(games, g, e, &state->editions[e]);
    state->language = games->settings->language[g];
    if (!path[0]) return;
    split_path(path, state->eu_file, sizeof state->eu_file, state->eu_folder, sizeof state->eu_folder);
    state->eu = eu_found(games, g, path, state->eu_reason, sizeof state->eu_reason);
}

/* Oracles Combined's base, the European ROMs of Ages and Seasons as their pages show them, each in state->joined: ORIGINAL
 * when both are their game's, else the state of the first that is not, with its file and why in rom_reason (NONE: none
 * chosen). */
static void inspect_joined(OraclesHomeGames *games, OraclesHomeGame *state)
{
    memset(state, 0, sizeof *state);
    state->rom = ORACLES_ROM_ORIGINAL;
    for (int g = 0; g < ORACLES_SETTINGS_GAMES; g++) {
        OraclesHomeGame oracle;
        memset(&oracle, 0, sizeof oracle);
        inspect_eu(games, (OraclesSettingsGame)g, &oracle);
        OraclesHomeEuropean *half = &state->joined[g];
        half->state = oracle.eu;
        snprintf(half->file, sizeof half->file, "%s", oracle.eu_file);
        snprintf(half->reason, sizeof half->reason, "%s", oracle.eu_reason);
        /* Ages' file shown while both are theirs, else the first that is not. */
        if (state->rom != ORACLES_ROM_ORIGINAL || (oracle.eu == ORACLES_ROM_ORIGINAL && g)) continue;
        const char *name = oracles_game_name(game_of((OraclesSettingsGame)g));
        snprintf(state->rom_file, sizeof state->rom_file, "%s", oracle.eu_file);
        snprintf(state->rom_folder, sizeof state->rom_folder, "%s", oracle.eu_folder);
        state->rom = oracle.eu;
        if (oracle.eu == ORACLES_ROM_NONE) snprintf(state->rom_reason, sizeof state->rom_reason, "No European ROM of %s chosen", name);
        else if (oracle.eu == ORACLES_ROM_UNRECOGNISED) snprintf(state->rom_reason, sizeof state->rom_reason, "Not the European ROM of %s", name);
        else snprintf(state->rom_reason, sizeof state->rom_reason, "%s", oracle.eu_reason);
    }
    state->usable = state->rom == ORACLES_ROM_ORIGINAL;
}

/* A fan game as the launcher finds it: its base, its Oracle's ROM as the Oracle's page shows it; its patch, read; the
 * image the patch makes of the base, identified, as a session builds it at each start; its save, beside the patch; with
 * the fan games' languages on, its language, with the state of its Oracle's European ROM, and the share of its texts a
 * language translates. */
static void inspect_fan(OraclesHomeGames *games, const OraclesHomeFanGame *fan, OraclesHomeGame *state)
{
    const oracles_settings *settings = games->settings;
    const unsigned f = fan_index(fan);
    const OraclesSettingsGame base = rom_setting(fan->base);
    if (fan->joined) inspect_joined(games, state);
    else inspect(games, base, state);
    if (games->fan_languages && !fan->joined) {
        OraclesHomeGame oracle;
        inspect_eu(games, base, &oracle);
        state->eu = oracle.eu;
        state->language = oracle.eu == ORACLES_ROM_ORIGINAL ? settings->fan_language[f] : 0;
        memset(state->editions, 0, sizeof state->editions);
    }
    const int base_usable = state->usable;
    state->usable = 0;
    state->fan = fan;
    state->refuses = oracles_home_refusals(game_of(base), oracles_home_fan_revisions[f]);
    /* Its own item hotkeys' mode where it has slots of its own (Oracles Combined), off where its profile refuses them. */
    const int slots = oracles_home_hotkey_slots(fan->game);
    if (state->refuses & ORACLES_HOME_REFUSES_HOTKEYS) state->hotkeys = state->hotkeys_failed = 0;
    else if (slots >= 0) { state->hotkeys = (int)settings->item_hotkeys[slots]; state->hotkeys_failed = 0; }
    state->cheats = oracles_home_cheats(settings, fan->game);   /* its own, not its Oracle's */
    state->save_file[0] = state->save_written[0] = state->last_session[0] = 0;
    const char *patch = games->refused_patch[f].path[0] ? games->refused_patch[f].path : settings->patch[f];
    if (!patch[0]) return;
    split_path(patch, state->patch_file, sizeof state->patch_file, state->patch_folder, sizeof state->patch_folder);
    if (games->refused_patch[f].path[0]) {
        snprintf(state->patch_reason, sizeof state->patch_reason, "%s", games->refused_patch[f].reason);
        state->patch = ORACLES_ROM_REFUSED;
        return;
    }
    /* The patch, and the image it makes of the base, each read again once its files changed. */
    struct stat file;
    int there;
    OraclesHomeFound *found = &games->patch_found[f];
    if (!still_found(found, patch, &file, &there)) {
        state->patch = check_patch(games, patch, state->patch_reason, sizeof state->patch_reason) == 0 ? ORACLES_ROM_ORIGINAL : ORACLES_ROM_REFUSED;
        keep_found(found, patch, &file, there, state->patch, state->patch == ORACLES_ROM_REFUSED ? state->patch_reason : "");
        games->image_found[f].path[0] = 0;
    } else {
        state->patch = found->state;
        snprintf(state->patch_reason, sizeof state->patch_reason, "%s", found->reason);
    }
    if (state->patch == ORACLES_ROM_REFUSED) return;
    find_save(patch, state);
    if (!base_usable) return;
    /* The image, kept with its base: Oracles Combined's with both European ROMs, Seasons' in joined_found. */
    const char *joined, *base_path = fan_base(games, fan, &joined);
    found = &games->image_found[f];
    struct stat second;
    int second_there = 0;
    const int second_same = !joined || still_found(&games->joined_found, joined, &second, &second_there);
    if (!still_found(found, base_path, &file, &there) || !second_same) {
        size_t size = 0;
        uint8_t *image = build_image(games, base_path, joined, patch, &size, state->image_reason, sizeof state->image_reason);
        OraclesRomInfo info;
        state->image = ORACLES_ROM_REFUSED;
        if (image && oracles_rom_identify(image, size, &info, state->image_reason, sizeof state->image_reason) == 0) {
            state->image = fan_made(f, &info) ? ORACLES_ROM_ORIGINAL : ORACLES_ROM_UNRECOGNISED;
            if (state->image == ORACLES_ROM_ORIGINAL) state->image_language = oracles_rom_combined_language(&info);
            /* Another fan game's patch (written so in settings.txt): refused, the game it makes named. */
            for (unsigned o = 0; o < ORACLES_HOME_FAN_GAMES; o++)
                if (o != f && fan_made(o, &info)) {
                    state->image = ORACLES_ROM_REFUSED;
                    snprintf(state->image_reason, sizeof state->image_reason, "the patch of %s, not of %s", fan_title(&oracles_home_fan_games[o]), fan_title(fan));
                }
        }
        /* A patch made for other ROMs than the two joined says which it needs. */
        if (!image && joined && !strcmp(state->image_reason, oracles_bps_other_base))
            snprintf(state->image_reason, sizeof state->image_reason, "this patch is not made for the European ROMs of Ages and Seasons");
        free(image);
        keep_found(found, base_path, &file, there, state->image, state->image == ORACLES_ROM_REFUSED ? state->image_reason : "");
        found->language = state->image == ORACLES_ROM_ORIGINAL ? state->image_language : NULL;
        if (joined) keep_found(&games->joined_found, joined, &second, second_there, state->image, "");
    } else {
        state->image = found->state;
        state->image_language = found->language;
        snprintf(state->image_reason, sizeof state->image_reason, "%s", found->reason);
    }
    state->usable = state->image != ORACLES_ROM_REFUSED;
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

/* The mods of the mods folder, for each Oracle as its Mods page lists them, active as settings.txt says; and whether
 * the save its game with mods plays on is there, beside the ROM of settings.txt (none while a refused choice shows),
 * which Mods' Save row exports: read each time the page opens, the file being the player's to move meanwhile. */
static void read_mods(void *opaque, OraclesHomeNav *nav)
{
    const OraclesHomeGames *games = opaque;
    char folder[ORACLES_SETTINGS_PATH_LENGTH];
    oracles_settings_mods_folder(games->settings, folder, sizeof folder);
    oracles_ui_copy_middle(nav->mods_folder, sizeof nav->mods_folder, folder, strlen(folder));
    for (int g = 0; g < ORACLES_SETTINGS_GAMES; g++) {
        char save[ORACLES_SETTINGS_PATH_LENGTH];
        const char *rom = games->settings->rom[g];
        oracles_session_default_save_path(rom, save, sizeof save);
        nav->games[g].mods_save = rom[0] && !games->refused[g].path[0] && oracles_save_transfer_has_mods_save(save);
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

/* Settings' Portable mode: shown where the launcher's folder can keep its data and ORACLES_SETTINGS_DIR does not name
 * another (oracles_home_portable_shown); its switch, portable.txt; the games outside the folder, which stay where they
 * are. */
static void portable_state(const OraclesHomeGames *games, OraclesHomeNav *nav)
{
    const char *exe_dir = games->settings->exe_dir;
    nav->portable_shown = oracles_home_portable_shown(SDL_getenv("ORACLES_SETTINGS_DIR"), exe_dir);
    nav->portable = nav->portable_shown && oracles_portable_marked(exe_dir);
    nav->games_outside = nav->portable_shown ? oracles_settings_games_outside(games->settings, exe_dir) : 0u;
}

static void portable_turned(void *opaque, OraclesHomeNav *nav, int on, int result, const char *next_dir)
{
    OraclesHomeGames *games = opaque;
    const OraclesPortableResult r = (OraclesPortableResult)result;
    oracles_settings_portable_turned(games->settings, on, r, next_dir);
    nav->portable_status = oracles_home_portable_status(r);
    portable_state(games, nav);
}

#ifdef ORACLES_RETROACHIEVEMENTS
static int achievements_offline(const OraclesHomeGames *games) { return games->options && games->options->achievements_offline; }

/* RetroAchievements: their setting and the account kept (achievements.txt); with the offline answers, both in memory. */
static void achievements_state(const OraclesHomeGames *games, OraclesHomeNav *nav)
{
    OraclesHomeAchievements *a = &nav->achievements;
    a->shown = 1;
    a->on = (achievements_offline(games) ? games->achievements_on : games->settings->achievements) || (a->login && a->opened_by_on);
    a->sound = games->settings->achievements_sound;
    if (achievements_offline(games)) {
        snprintf(a->user, sizeof a->user, "%s", games->achievements_user);
        a->refused = 0;
        a->pending = 0;
    } else {
        oracles_session_achievements_account(games->settings, a->user, sizeof a->user, &a->refused, &a->pending);
    }
}

static int achievements_login(void *opaque, const char *user, const char *password)
{
    OraclesHomeGames *games = opaque;
    if (games->achievements_login) return 1;
    games->achievements_login = oracles_session_achievements_login_begin(games->settings, user, password,
                                                                         games->options ? games->options->achievements_offline : NULL);
    return games->achievements_login != NULL;
}

static int achievements_answer(void *opaque, char *user, size_t capacity)
{
    OraclesHomeGames *games = opaque;
    if (!games->achievements_login) return -1;
    const int answer = oracles_session_achievements_login_poll(games->achievements_login, user, capacity);
    if (answer == -2) return answer;
    oracles_session_achievements_login_end(games->achievements_login);
    games->achievements_login = NULL;
    if (answer == 1 && achievements_offline(games)) snprintf(games->achievements_user, sizeof games->achievements_user, "%s", user);
    fprintf(stderr, "oracles: achievements: login %s\n", answer == 1 ? "accepted" : answer == 0 ? "refused" : "unanswered");
    return answer;
}

static void achievements_cancel(void *opaque)
{
    OraclesHomeGames *games = opaque;
    if (!games->achievements_login) return;
    oracles_session_achievements_login_end(games->achievements_login);   /* its client and transport: no answer comes after */
    games->achievements_login = NULL;
    fprintf(stderr, "oracles: achievements: login given up\n");
}

/* The page opening on `game`: what the account's row shows read again, the list asked for. */
static OraclesAchievementsLoad *achievements_list_begin(void *opaque, OraclesHomeNav *nav, int game)
{
    OraclesHomeGames *games = opaque;
    achievements_state(games, nav);
    return oracles_session_achievements_load_begin(games->settings, games->settings->rom[rom_setting(game)], games->options ? games->options->achievements_offline : NULL);
}

/* The page's list (achievements_load.h). */
static int achievements_list(void *opaque, OraclesHomeNav *nav)
{
    OraclesHomeGames *games = opaque;
    const int moved = oracles_achievements_page_load(&games->achievements_load, &games->achievements_load_game, nav, achievements_list_begin, games, (double)SDL_GetTicks());
    /* A token the server refused is forgotten: the account's row says so. */
    if (moved && (nav->achievements.list_state == ORACLES_ACHIEVEMENTS_LIST_UNREACHABLE || nav->achievements.list_state == ORACLES_ACHIEVEMENTS_LIST_FAILED))
        achievements_state(games, nav);
    return moved;
}

static void achievements_logout(void *opaque, OraclesHomeNav *nav)
{
    OraclesHomeGames *games = opaque;
    if (!achievements_offline(games)) oracles_session_achievements_logout(games->settings);
    games->achievements_user[0] = 0;
    achievements_state(games, nav);
}
#endif

static void refresh(void *opaque, OraclesHomeNav *nav, struct SDL_Window *window)
{
    OraclesHomeGames *games = opaque;
    nav->fan_languages = games->fan_languages;
    oracles_home_inspect_display(games->settings, &nav->display, window);
    oracles_filter_preview_title(nav->display.last_picture, sizeof nav->display.last_picture);   /* Filter's preview: the last game's name */
    nav->display.margin = oracles_session_screen_margin(games->options ? games->options->screen_margin : 0, games->settings);   /* --screen-margin's, for the run */
    nav->display.margin_fixed = games->options && games->options->screen_margin;
    if (games->options && games->options->compact_text) {   /* --text's, for every game the home screen starts */
        nav->display.text = games->options->compact_text == 2;
        nav->display.text_fixed = 1;
    }
    if (games->options && games->options->text_box_wide) {   /* --text-box's, the same */
        nav->display.text_box = games->options->text_box_wide == 2;
        nav->display.text_box_fixed = 1;
    }
    if (games->options && games->options->text_size) {   /* --text-size's, the same */
        nav->display.text_size = games->options->text_size - 1;
        nav->display.text_size_fixed = 1;
    }
    if (games->options && games->options->unvisited_hidden) {   /* --unvisited-rooms', the same */
        nav->display.unvisited = games->options->unvisited_hidden - 1;
        nav->display.unvisited_fixed = 1;
    }
    nav->cheats_fixed = games->options && games->options->cheats_given;   /* --cheats', for every game it starts */
    nav->update_check = games->settings->update_check;
    portable_state(games, nav);
#ifdef ORACLES_RETROACHIEVEMENTS
    achievements_state(games, nav);
#endif
    nav->cheats_changed = 0;
    oracles_home_inspect_controls(games->settings, &nav->controls);
    for (int g = 0; g < ORACLES_SETTINGS_GAMES; g++) {
        inspect(games, (OraclesSettingsGame)g, &nav->games[g]);
        inspect_eu(games, (OraclesSettingsGame)g, &nav->games[g]);
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
    for (int g = 0; nav->cheats_fixed && g < ORACLES_HOME_GAMES; g++) nav->games[g].cheats = games->options->cheats;
    read_mods(opaque, nav);
    oracles_home_files_forget(&games->files);
}

static void store(void *opaque, const OraclesHomeNav *nav)
{
    OraclesHomeGames *games = opaque;
    oracles_settings *prefs = games->settings;
    for (int g = 0; g < ORACLES_HOME_GAMES; g++) {
        const int slots = oracles_home_hotkey_slots(g);
        if (slots >= 0 && !(nav->games[g].refuses & ORACLES_HOME_REFUSES_HOTKEYS)) prefs->item_hotkeys[slots] = (OraclesHotkeysMode)nav->games[g].hotkeys;
    }
    oracles_home_store_cheats(nav, prefs);   /* the page's, never --cheats' */
    oracles_home_store_controls(&nav->controls, prefs);
    oracles_home_store_display(&nav->display, nav->in_game, prefs);
    prefs->update_check = nav->update_check;   /* Settings': the next launch asks, or not */
#ifdef ORACLES_RETROACHIEVEMENTS
    /* RetroAchievements', On only once an account is in; with the offline answers, kept in memory alone. */
    oracles_achievements_store(nav, achievements_offline(games), &prefs->achievements, &games->achievements_on);
    prefs->achievements_sound = nav->achievements.sound;   /* the next unlock's toast, in the game the home screen starts */
#endif
    /* Each Oracle's active mods, those its Mods page switched on. */
    for (int g = 0; g < ORACLES_SETTINGS_GAMES; g++) {
        prefs->mods[g][0] = 0;
        for (unsigned i = 0; i < nav->mods[g].count; i++)
            if (nav->mods[g].mods[i].active) oracles_settings_mod_set(prefs->mods[g], nav->mods[g].mods[i].name, 1);
    }
    /* Each Oracle's European ROM removed on its page, with its language; the language chosen while the ROM is
     * recognized.  A ROM gone or another file keeps the choice settings.txt has.  So does a fan game's language: chosen
     * while its Oracle's European ROM is recognized, with its Oracle's original ROM. */
    for (int g = 0; g < ORACLES_SETTINGS_GAMES; g++) {
        const OraclesHomeGame *game = &nav->games[g];
        /* A source removed on Cartridge › Editions goes, with a language it gave; the language chosen is kept, one that
         * waits for its source too. */
        int language = game->rom == ORACLES_ROM_ORIGINAL ? game->language : prefs->language[g];
        if (game->eu == ORACLES_ROM_NONE) {
            prefs->eu_rom[g][0] = 0;
            if (language < ORACLES_SETTINGS_EUROPEAN_LANGUAGES) language = 0;
        }
        /* Japanese and Polish wait for their source: the game plays in English meanwhile, and the language comes back with
         * it.  Removing it on the page set the language back to English already. */
        if (game->editions[0].state == ORACLES_ROM_NONE) prefs->jp_rom[g][0] = 0;
        if (game->editions[1].state == ORACLES_ROM_NONE) prefs->pl_rom[g][0] = 0;
        prefs->language[g] = language;
    }
    for (unsigned f = 0; nav->fan_languages && f < ORACLES_HOME_FAN_GAMES; f++) {
        const OraclesHomeGame *game = &nav->games[oracles_home_fan_games[f].game];
        if (!oracles_home_fan_games[f].joined && oracles_page_language_available(game, 1)) prefs->fan_language[f] = game->language;
    }
    oracles_settings_store(games->settings);
}

/* A ROM for game `g`: kept in settings.txt when it is the game's, else shown refused under the game and not kept. */
/* Returns 1 when the ROM is kept. */
static int offer(OraclesHomeGames *games, OraclesSettingsGame g, const char *path)
{
    OraclesRomInfo info;
    char error[ORACLES_HOME_TEXT_LENGTH];
    int ok = identify(games, path, &info, NULL, error, sizeof error);
    if (ok && info.game != game_of(g)) {
        other_game(&info, game_of(g), error, sizeof error);
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
    fprintf(stderr, "oracles: %s ROM set to %s (%s)\n", oracles_game_name(info.game), path,
            oracles_rom_is_original(&info) ? "original US" : oracles_rom_is_combined(&info) ? "an image of both games, header accepted" : "not an original, header accepted");
    return 1;
}

/* A European ROM for Oracle `g`: kept in settings.txt whatever it is, its page saying whether it is the game's; another
 * file sets the language back to English.  Returns what it is. */
static OraclesRomState offer_eu(OraclesHomeGames *games, OraclesSettingsGame g, const char *path)
{
    char reason[ORACLES_HOME_TEXT_LENGTH];
    const OraclesRomState state = eu_found(games, g, path, reason, sizeof reason);
    snprintf(games->settings->eu_rom[g], sizeof games->settings->eu_rom[g], "%s", path);
    if (state != ORACLES_ROM_ORIGINAL && games->settings->language[g] < ORACLES_SETTINGS_EUROPEAN_LANGUAGES) games->settings->language[g] = 0;
    oracles_settings_store(games->settings);
    fprintf(stderr, "oracles: %s European ROM set to %s (%s)\n", oracles_game_name(game_of(g)), path,
            state == ORACLES_ROM_ORIGINAL ? "recognized" : state == ORACLES_ROM_UNRECOGNISED ? "not its European ROM" : reason);
    return state;
}

/* A ROM for Oracles Combined's base: the European ROM of the game it is, kept as that Oracle's, or refused with a toast
 * saying why.  Returns 1 when it is kept. */
static int offer_joined(OraclesHomeGames *games, const char *path, char *message, size_t capacity)
{
    const OraclesGame game = european_game(games, path);
    if (game == ORACLES_GAME_UNKNOWN) {
        snprintf(message, capacity, "Refused: Oracles Combined is made of the European ROMs of Ages and Seasons");
        fprintf(stderr, "oracles: %s refused for Oracles Combined: not a European ROM of Ages or Seasons\n", path);
        return 0;
    }
    /* Kept as its Oracle's, the row it was chosen on whichever: the toast says which, as a European ROM dropped does. */
    offer_eu(games, game == ORACLES_GAME_AGES ? ORACLES_SETTINGS_AGES : ORACLES_SETTINGS_SEASONS, path);
    snprintf(message, capacity, "%s European ROM set", oracles_game_name(game));
    return 1;
}

static const OraclesHomeFanGame *fan_of_patch(OraclesHomeGames *games, const char *path);

/* A patch for a fan game: kept in settings.txt when it is a BPS patch, else shown refused and not kept, and so is
 * another fan game's, which makes that game's image of its Oracle's ROM (unless `its_own`, the caller having found that
 * it makes this one's).  Whether it is made for the Oracle's ROM, the image's line says. */
static int offer_patch(OraclesHomeGames *games, const OraclesHomeFanGame *fan, int its_own, const char *path, char *error, size_t capacity)
{
    const unsigned f = fan_index(fan);
    int refused = check_patch(games, path, error, capacity) != 0;
    const OraclesHomeFanGame *made = refused || its_own ? NULL : fan_of_patch(games, path);
    if (made && made != fan) {
        snprintf(error, capacity, "the patch of %s, not of %s", fan_title(made), fan_title(fan));
        refused = 1;
    }
    if (refused) {
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
static const OraclesHomeFanGame *fan_of_patch(OraclesHomeGames *games, const char *path)
{
    for (unsigned f = 0; f < ORACLES_HOME_FAN_GAMES; f++) {
        const char *joined, *base = fan_base(games, &oracles_home_fan_games[f], &joined);
        if (!base[0] || (joined && !joined[0])) continue;
        char error[ORACLES_HOME_TEXT_LENGTH];
        size_t size = 0;
        uint8_t *image = build_image(games, base, joined, path, &size, error, sizeof error);
        OraclesRomInfo info;
        const int made = image && oracles_rom_identify(image, size, &info, error, sizeof error) == 0 && fan_made(f, &info);
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
    /* Oracles Combined's Ages ROM row: a European ROM's dialog, where Ages' European ROM is, or else where Ages' ROM is. */
    const OraclesHomeFanGame *fan = oracles_home_fan_game(game);
    const char *joined, *current = fan ? fan_base(games, fan, &joined) : "";
    if (!fan || !current[0]) current = games->settings->rom[rom_setting(game)];
    choose(window, fan && fan->joined ? ORACLES_DIALOG_EU_ROM : ORACLES_DIALOG_ROM, current, game);
}

static void choose_patch(void *opaque, int game, struct SDL_Window *window)
{
    const OraclesHomeGames *games = opaque;
    const OraclesHomeFanGame *fan = oracles_home_fan_game(game);
    if (!fan) return;
    /* The dialog opens where the patch is, or else where its Oracle's ROM is. */
    const unsigned f = fan_index(fan);
    const char *joined, *base = fan_base(games, fan, &joined);
    const char *current = games->settings->patch[f][0] ? games->settings->patch[f] : base[0] ? base : games->settings->rom[rom_setting(game)];
    choose(window, ORACLES_DIALOG_PATCH, current, game);
}

static void choose_eu_rom(void *opaque, int game, struct SDL_Window *window)
{
    const OraclesHomeGames *games = opaque;
    /* Oracles Combined's Seasons ROM row: where Seasons' European ROM is, or else Ages'. */
    const OraclesHomeFanGame *fan = oracles_home_fan_game(game);
    const OraclesSettingsGame g = fan ? ORACLES_SETTINGS_SEASONS : rom_setting(game);
    const char *other = fan ? games->settings->eu_rom[ORACLES_SETTINGS_AGES] : games->settings->rom[g];
    /* The dialog opens where the European ROM is, or else where the game's ROM is. */
    choose(window, ORACLES_DIALOG_EU_ROM, games->settings->eu_rom[g][0] ? games->settings->eu_rom[g] : other, game);
}

static void choose_edition(void *opaque, int game, int edition, struct SDL_Window *window)
{
    const OraclesHomeGames *games = opaque;
    if (oracles_home_fan_game(game) || edition < 1 || edition > 2) return;
    const OraclesSettingsGame g = rom_setting(game);
    const char *current = edition == 1 ? games->settings->jp_rom[g] : games->settings->pl_rom[g];
    /* The dialog opens where the file is, or else where the game's ROM is. */
    choose(window, edition == 1 ? ORACLES_DIALOG_JP_ROM : ORACLES_DIALOG_PL_ROM, current[0] ? current : games->settings->rom[g], game);
}

/* A Japanese ROM or a Polish source for Oracle `g`: kept in settings.txt whatever it is, its row saying whether it is the
 * game's; another file sets a language it gave back to English. */
static void offer_edition(OraclesHomeGames *games, OraclesSettingsGame g, int edition, const char *path)
{
    char *setting = edition == 1 ? games->settings->jp_rom[g] : games->settings->pl_rom[g];
    snprintf(setting, ORACLES_SETTINGS_PATH_LENGTH, "%s", path);
    OraclesHomeEdition e;
    inspect_edition(games, g, edition - 1, &e);
    const int language = edition == 1 ? ORACLES_SETTINGS_JAPANESE : ORACLES_SETTINGS_POLISH;
    if (e.state != ORACLES_ROM_ORIGINAL && games->settings->language[g] == language) games->settings->language[g] = 0;
    oracles_settings_store(games->settings);
    fprintf(stderr, "oracles: %s %s set to %s (%s)\n", oracles_game_name(game_of(g)), edition == 1 ? "Japanese ROM" : "Polish ROM or patch", path,
            e.state == ORACLES_ROM_ORIGINAL ? "recognized" : e.state == ORACLES_ROM_UNRECOGNISED ? "not its own" : e.reason);
}

static int edition_of(OraclesHomeGames *games, const char *path, int page_game, OraclesSettingsGame *g);

/* An edition filed for Oracle `g` in place of a ROM: kept in Cartridge › Editions, the toast saying so, and that the US
 * ROM it plays with is still to choose when it is not there. */
static void file_edition(OraclesHomeGames *games, OraclesSettingsGame g, int edition, const char *path, char *message, size_t capacity)
{
    offer_edition(games, g, edition, path);
    char reason[ORACLES_HOME_TEXT_LENGTH];
    int combined = 0;
    const int us = games->settings->rom[g][0] && rom_found(games, g, games->settings->rom[g], reason, sizeof reason, &combined) == ORACLES_ROM_ORIGINAL;
    snprintf(message, capacity, "%s %s set in Cartridge \xe2\x80\xba Editions%s", oracles_game_name(game_of(g)), edition == 1 ? "Japanese ROM" : "Polish ROM or patch",
             us ? "" : ": it plays with the US ROM, still to choose");
}

static void file_chosen(void *opaque, int game, int kind, int result, const char *path, int copied, char *message, size_t capacity)
{
    OraclesHomeGames *games = opaque;
    char error[ORACLES_HOME_TEXT_LENGTH];
    const char *what = kind == ORACLES_DIALOG_PATCH ? "patch" : kind == ORACLES_DIALOG_EU_ROM ? "European ROM" : kind == ORACLES_DIALOG_JP_ROM ? "Japanese ROM"
                     : kind == ORACLES_DIALOG_PL_ROM ? "Polish ROM or patch" : "ROM";
    switch ((OraclesDialogResult)result) {
        case ORACLES_DIALOG_UNAVAILABLE:
            snprintf(message, capacity, "No file dialog on this system: drop the %s on this window", what);
            return;
        case ORACLES_DIALOG_FAILED:
            snprintf(message, capacity, "The %s cannot be copied into the application's folder", what);
            return;
        case ORACLES_DIALOG_CANCELLED: return;
        case ORACLES_DIALOG_CHOSEN: break;
    }
    /* An Oracle's European ROM is kept whatever it is, its page saying whether it is the game's: a copy made for it stays;
     * so are its Japanese ROM and its Polish source. */
    const OraclesHomeFanGame *fan = oracles_home_fan_game(game);
    if ((kind == ORACLES_DIALOG_JP_ROM || kind == ORACLES_DIALOG_PL_ROM) && !fan) {
        offer_edition(games, rom_setting(game), kind == ORACLES_DIALOG_JP_ROM ? 1 : 2, path);
        return;
    }
    /* Choose ROM… given a Japanese ROM, a Polish ROM or Kaxus's patch: never the game's ROM, filed in Cartridge ›
     * Editions. */
    OraclesSettingsGame edition_game;
    const int edition = kind == ORACLES_DIALOG_ROM && !fan ? edition_of(games, path, game, &edition_game) : 0;
    if (edition) {
        file_edition(games, edition_game, edition, path, message, capacity);
        return;
    }
    if (kind == ORACLES_DIALOG_EU_ROM && !fan) {
        offer_eu(games, rom_setting(game), path);
        return;
    }
    /* Oracles Combined's rows take a European ROM of either Oracle, kept as that Oracle's. */
    const int kept = kind == ORACLES_DIALOG_PATCH ? fan && offer_patch(games, fan, 0, path, error, sizeof error)   /* the page's fan game */
                   : fan && fan->joined ? offer_joined(games, path, message, capacity)
                   : kind == ORACLES_DIALOG_EU_ROM ? 0
                                                   : offer(games, rom_setting(game), path);
    /* A copy made for this choice and refused goes (Android): the application's folder keeps only what it uses. */
    if (!kept && copied && remove(path) == 0) fprintf(stderr, "oracles: %s removed, refused\n", path);
}

void oracles_home_games_save_path(const OraclesHomeGames *games, int game, char *path, size_t capacity)
{
    const OraclesHomeFanGame *fan = oracles_home_fan_game(game);
    const char *file = fan ? games->settings->patch[fan_index(fan)] : games->settings->rom[rom_setting(game)];
    path[0] = 0;
    if (file[0]) oracles_session_default_save_path(file, path, capacity);
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
    char shown[ORACLES_SETTINGS_PATH_LENGTH];
    oracles_page_folder(1, folder, shown, sizeof shown);
    snprintf(message, capacity, "Mods go in %s, reachable over USB", shown);
#else
    if (oracles_open_folder(folder, error, sizeof error) != 0) snprintf(message, capacity, "The folder does not open: %s", error);
#endif
}

/* The Oracle and the edition (1 Japanese, 2 Polish) a file is the source of, the Oracle whose page is open looked at
 * first: a Japanese ROM, a Polish ROM, Kaxus's patch or its zip; 0 for none. */
static int edition_of(OraclesHomeGames *games, const char *path, int page_game, OraclesSettingsGame *g)
{
    const OraclesSettingsGame first = page_game == ORACLES_HOME_GAME_SEASONS ? ORACLES_SETTINGS_SEASONS : ORACLES_SETTINGS_AGES;
    for (int i = 0; i < ORACLES_SETTINGS_GAMES; i++) {
        const OraclesSettingsGame oracle = i ? (OraclesSettingsGame)(1 - first) : first;
        for (int edition = 1; edition <= 2; edition++) {
            char reason[ORACLES_HOME_TEXT_LENGTH];
            if (own_rom_state(games, path, game_of(oracle), edition == 1 ? ORACLES_LANGUAGE_JAPANESE : ORACLES_LANGUAGE_POLISH, reason, sizeof reason)
                != ORACLES_ROM_ORIGINAL)
                continue;
            *g = oracle;
            return edition;
        }
    }
    return 0;
}

static int drop(void *opaque, const char *path, int page_game, char *message, size_t capacity)
{
    OraclesHomeGames *games = opaque;
    char error[200];
    /* A Japanese ROM, a Polish ROM or Kaxus's patch is the source of its Oracle's Japanese or Polish (Cartridge ›
     * Editions), never a game's ROM: kept there, a toast naming it; the Oracle's page stays, else the home screen shows
     * that Oracle. */
    OraclesSettingsGame edition_game;
    const int edition = edition_of(games, path, page_game, &edition_game);
    if (edition) {
        file_edition(games, edition_game, edition, path, message, capacity);
        if (page_game >= 0) return -1;
        return edition_game == ORACLES_SETTINGS_AGES ? ORACLES_HOME_AGES : ORACLES_HOME_SEASONS;
    }
    /* A patch (zip.h: a .bps, or a zip of one) goes to the fan game whose page is open, which shows it, and so does a zip
     * whose directory cannot be read, there; elsewhere to the fan game whose image it makes of its Oracle's ROM, a toast
     * saying so, and the home screen then shows the list of fan games. */
    const int patch = oracles_zip_is_patch_file(path);
    if (patch > 0 || (patch < 0 && oracles_home_fan_game(page_game))) {
        const OraclesHomeFanGame *fan = oracles_home_fan_game(page_game);
        if (!fan && check_patch(games, path, error, sizeof error) != 0) {
            snprintf(message, capacity, "Refused: %s.", error);
            return -1;
        }
        const int found = !fan;   /* the patch's own fan game, found once */
        if (found) fan = fan_of_patch(games, path);
        if (!fan) {
            snprintf(message, capacity, "No fan game is made of this patch and the ROMs chosen: drop it on its page");
            return page_game >= 0 ? -1 : ORACLES_HOME_FAN;
        }
        const int kept = offer_patch(games, fan, found, path, error, sizeof error);
        if (page_game == fan->game) return -1;
        if (kept) snprintf(message, capacity, "%s patch set", fan_title(fan));
        else snprintf(message, capacity, "Refused: %s.", error);
        return page_game >= 0 ? -1 : ORACLES_HOME_FAN;
    }
    /* A European ROM is the European ROM of the game it is of, as the Japanese and the Polish sources are: on its own
     * Oracle's page the page says so, elsewhere a toast. */
    const OraclesGame eu_game = european_game(games, path);
    if (eu_game != ORACLES_GAME_UNKNOWN) {
        const OraclesSettingsGame g = eu_game == ORACLES_GAME_AGES ? ORACLES_SETTINGS_AGES : ORACLES_SETTINGS_SEASONS;
        offer_eu(games, g, path);
        /* On its own Oracle's page the page says it; elsewhere a toast. */
        if (page_game == (g == ORACLES_SETTINGS_AGES ? ORACLES_HOME_GAME_AGES : ORACLES_HOME_GAME_SEASONS)) return -1;
        snprintf(message, capacity, "%s European ROM set", oracles_game_name(game_of(g)));
        return page_game >= 0 ? -1 : g == ORACLES_SETTINGS_AGES ? ORACLES_HOME_AGES : ORACLES_HOME_SEASONS;
    }
    /* On a game's page a file dropped is a ROM for that game, as Choose ROM's (a fan game's base ROM): refused, it
     * shows there with why. */
    if (page_game >= 0) {
        const OraclesHomeFanGame *fan = oracles_home_fan_game(page_game);
        if (fan && fan->joined) offer_joined(games, path, message, capacity);
        else offer(games, rom_setting(page_game), path);
        return -1;
    }
    /* On the home screen it goes under the game it is, or a toast says why it is none. */
    OraclesRomInfo info;
    if (!identify(games, path, &info, NULL, error, sizeof error)) {
        fprintf(stderr, "oracles: %s refused: %s\n", path, error);
        snprintf(message, capacity, "Refused: %s.", error);
        return -1;
    }
    const OraclesSettingsGame g = info.game == ORACLES_GAME_AGES ? ORACLES_SETTINGS_AGES : ORACLES_SETTINGS_SEASONS;
    offer(games, g, path);
    snprintf(message, capacity, "%s ROM set%s", oracles_game_name(info.game),
             oracles_rom_is_original(&info) ? "" : oracles_rom_is_combined(&info) ? ": Oracles Combined, Faithful only; see Fan games" : ": unrecognized, Faithful only");
    return g == ORACLES_SETTINGS_AGES ? ORACLES_HOME_AGES : ORACLES_HOME_SEASONS;
}

static int start(void *opaque, OraclesHomeCommand game, OraclesHomeNav *nav, struct SDL_Window *window, struct SDL_Renderer *renderer,
                 OraclesUiDraw *draw, char *message, size_t capacity, int *slow)
{
    OraclesHomeGames *games = opaque;
    /* A fan game plays from its Oracle's ROM and its patch, the image built by the session. */
    const OraclesHomeFanGame *fan = oracles_home_fan_game_started(game);
    const int with_mods = game == ORACLES_HOME_START_AGES_MODS || game == ORACLES_HOME_START_SEASONS_MODS;
    const OraclesSettingsGame g = fan ? rom_setting(fan->base)
                                : game == ORACLES_HOME_START_SEASONS || game == ORACLES_HOME_START_SEASONS_MODS ? ORACLES_SETTINGS_SEASONS : ORACLES_SETTINGS_AGES;
    const char *name = fan ? fan_title(fan) : oracles_game_name(game_of(g));
    OraclesSessionOptions o = *games->options;
    o.last_picture = name;
    o.rom_path = games->settings->rom[g];
    if (fan && fan->joined) o.rom_path = fan_base(games, fan, &o.joined_path);
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
    else {
        inspect(games, g, &state);
        inspect_eu(games, g, &state);
    }
    /* The language of the game's page: an Oracle's on its original ROM, with its European ROM recognized; with the fan
     * games' languages on, a fan game's on its recognized image, with its Oracle's European ROM; else English.
     * Japanese and Polish, which settings.txt alone sets yet (language_<game>=ja, =pl), play with the Oracle's Japanese
     * ROM (jp_rom_<game>=), its Polish ROM or Kaxus's patch (pl_rom_<game>=), found as a session finds them; so do
     * --language ja and pl without --eu-rom, never with the European ROM.
     * The command line's --language and --eu-rom win, for the session alone; --eu-rom without --language serves
     * Japanese or Polish only when it is that language's ROM or patch, by its SHA-1, a European ROM there set aside for
     * jp_rom_<game>= or pl_rom_<game>= (oracles_home_own_language).  The pack is the game's own, beside
     * settings.txt, extracted at the first game in a language and read after; never --language-pack's, which names one
     * game's pack: the home screen starts several, and a session extracting another game's would write it over that
     * file.  A European ROM gone, or another file there since, plays in English, and the toast says why. */
    char pack[ORACLES_SETTINGS_PATH_LENGTH], language_note[ORACLES_HOME_TEXT_LENGTH + 128] = "";
    OraclesHomeGame oracle = state;
    if (fan && games->fan_languages && !fan->joined) inspect_eu(games, g, &oracle);
    oracles_home_files_forget(&games->files);   /* the files read for the page: none kept through the session */
    const int language = !fan ? games->settings->language[g] : games->fan_languages && !fan->joined ? games->settings->fan_language[fan_index(fan)] : 0;
    const int recognized = fan ? state.image == ORACLES_ROM_ORIGINAL : state.rom == ORACLES_ROM_ORIGINAL;
    const char *oracle_name = oracles_game_name(game_of(g));
    /* A language of a ROM of its own, which the European ROM does not give: Japanese or Polish, its settings index. */
    int own = 0;
    for (int l = ORACLES_SETTINGS_EUROPEAN_LANGUAGES; !fan && !own && l < ORACLES_SETTINGS_LANGUAGES; l++)
        if (o.language ? !strcmp(o.language, oracles_settings_language_names[l]) : language == l) own = l;
    if (fan) oracles_settings_fan_language_pack(games->settings, fan_index(fan), pack, sizeof pack);
    else oracles_settings_language_pack(games->settings, g, own, pack, sizeof pack);
    o.language_pack = pack[0] ? pack : NULL;
    oracles_home_own_language(games, g, own, recognized, &o, language_note, sizeof language_note);
    if (!own && !o.eu_rom_path && games->settings->eu_rom[g][0]) o.eu_rom_path = games->settings->eu_rom[g];
    if (!o.language && !own && language > 0 && language < ORACLES_SETTINGS_EUROPEAN_LANGUAGES && recognized) {
        if (oracle.eu == ORACLES_ROM_ORIGINAL) o.language = oracles_settings_language_names[language];
        else if (oracle.eu == ORACLES_ROM_UNRECOGNISED)
            snprintf(language_note, sizeof language_note, "%s is not the European ROM of %s: the game played in English.", oracle.eu_file, oracle_name);
        else if (oracle.eu == ORACLES_ROM_NONE)
            snprintf(language_note, sizeof language_note, "No European ROM of %s: the game played in English. Choose it in the Cartridge of %s.",
                     oracle_name, oracle_name);
        else
            snprintf(language_note, sizeof language_note, "The European ROM of %s cannot be read (%s): the game played in English.", oracle_name,
                     oracle.eu_reason);
        if (language_note[0]) fprintf(stderr, "oracles: %s\n", language_note);
    }
    /* Display's part: the profile the ROM plays, the transitions, the texts and the view's surface. */
    const OraclesProfile chosen = o.enhanced || o.zoom_out || o.view || o.continuous_transitions ? ORACLES_PROFILE_ENHANCED : games->settings->profile;
    OraclesEnhancedSize surface;
    const OraclesProfile profile = oracles_session_display_options(&o, games->settings, oracles_page_profile_allowed(&state, ORACLES_PROFILE_ENHANCED),
                                                                   window, &surface);
    if (profile != chosen) fprintf(stderr, "oracles: an unrecognized ROM plays in the Faithful profile, not %s\n", oracles_settings_profile_name(chosen));
    static const char *const hotkey_names[3] = { "off", "use", "equip" };
    /* The swim's hold, in either profile: the command line's, else settings.txt's swim_hold=. */
    o.swim_hold = o.swim_hold || games->settings->swim_hold;
    /* The text speed, in either profile, a fan game's too: the command line's, else settings.txt's text_speed=. */
    if (!o.text_instant) o.text_instant = games->settings->text_instant ? 2 : 1;
    /* The game's cheats: the command line's, else settings.txt's cheats_<game>= (a fan game's own); none where its
     * profile refuses them (an unrecognised ROM's). */
    if (state.refuses & ORACLES_HOME_REFUSES_CHEATS) o.cheats = 0;
    else if (!o.cheats_given) o.cheats = oracles_home_cheats(games->settings, fan ? fan->game : (int)g);   /* an Oracle's index is its settings' */
    /* The item hotkeys: off where the game's profile refuses them (a fan game's), whatever the command line gave. */
    if (state.refuses & ORACLES_HOME_REFUSES_HOTKEYS) o.hotkeys_option = hotkey_names[0];
    else if (!o.hotkeys_option) o.hotkeys_option = hotkey_names[state.hotkeys];
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
    o.sdl.scale = oracles_home_fitting_scale(window, o.sdl.scale, surface);
    /* Fill at one size, where every window comes to the one size the screen holds: fullscreen, as Display's diagram
     * shows it (oracles_display_fullscreen); Sharp there keeps the window, as large. */
    if (!o.sdl.fullscreen && !games->window_from_command_line && games->settings->scaling_fill) {
        OraclesHomeDisplay display;
        oracles_home_inspect_display(games->settings, &display, window);
        o.sdl.fullscreen = oracles_display_one_size_for(&display, (int)surface.width, (int)surface.height);
    }
    char window_text[48];
    if (o.sdl.fullscreen) snprintf(window_text, sizeof window_text, "fullscreen");
    else snprintf(window_text, sizeof window_text, "windowed at scale %u", o.sdl.scale);
    char surface_text[24];
    snprintf(surface_text, sizeof surface_text, "%ux%u", surface.width, surface.height);
    fprintf(stderr, "oracles: starting %s in the %s profile (%s), %s, continuous transitions %s, item hotkeys %s, %u mod%s, language %s\n", name,
            oracles_settings_profile_name(profile), surface_text,
            window_text, o.continuous_swim ? "on, swimming too" : o.continuous_transitions ? "on" : "off", o.hotkeys_option, o.mods_count,
            o.mods_count == 1 ? "" : "s", o.language ? o.language : "en");
    /* Escape opens the pause menu over the game, its settings read and written as the home screen's are. */
    OraclesHomeHost pause_host;
    oracles_home_games_host(games, &pause_host);
    const int home_game = fan ? fan->game : g == ORACLES_SETTINGS_AGES ? ORACLES_HOME_GAME_AGES : ORACLES_HOME_GAME_SEASONS;
    OraclesPause *pause = draw ? oracles_pause_create(draw, &pause_host, home_game, profile, games->settings->aspect,
                                                                oracles_session_screen_margin(o.screen_margin, games->settings), nav) : NULL;
    o.pause = pause;
    /* The pause's Load state may ask for the game again with a savestate's Display: the command line's options win there
     * too, and Enhanced only where the ROM plays it. */
    o.command_line = games->options;
    o.enhanced_allowed = oracles_page_profile_allowed(&state, ORACLES_PROFILE_ENHANCED);
    o.load_state = games->state_restart;
    o.state_notice = games->state_notice;
    o.state_previous = games->state_restart ? &games->state_previous : NULL;
    games->state_restart = 0;
    /* Each game started says which key opens the menu, until the player opened it by its key, ten games at most: counted
     * once it was drawn over the game, not when a session that failed to start, or ended before its status bar showed,
     * never showed it.  The pause opened by its key marks it done (pause.c). */
    if (pause && oracles_controls_menu_tip_due(games->settings->menu_tip)) oracles_pause_reminder(pause, games->settings->menu_pad);
    OraclesSessionResult result;
    oracles_session_run(&o, games->settings, &result);
    if (oracles_pause_reminder_drawn(pause) && games->settings->menu_tip != ORACLES_TIP_OFF) {
        games->settings->menu_tip = oracles_controls_tip_shown(games->settings->menu_tip, ORACLES_MENU_TIPS);
        oracles_settings_store(games->settings);
    }
    if (pause) oracles_pause_input(pause, nav);
    oracles_pause_destroy(pause);
    if (result.route_written) games->route_recorded = 1;
    /* Item hotkeys that could not attach to this ROM: Cartridge says so under its status, while it is the game's ROM. */
    if (result.hotkeys_dropped && !fan) snprintf(games->hotkeys_failed[g], sizeof games->hotkeys_failed[g], "%s", o.rom_path);
    /* Back on the home screen, a toast: why the game stopped, else that it played without the item hotkeys, and where
     * --record wrote its route. */
    static const char dropped[] = "Item hotkeys could not attach to this ROM: the game played without them.";
    if (result.error[0]) snprintf(message, capacity, "The game stopped: %s.", result.error);
    /* A game started again for its savestate that stopped before loading it: Display's choices put back, as when the
     * savestate is refused after all. */
    if (o.state_previous && result.state_unloaded) {
        oracles_state_display_restore(games->settings, o.state_previous);
        oracles_ui_append_middle(message, capacity, "Display is back as it was.");
        fprintf(stderr, "oracles: the game started again for its savestate stopped before loading it: Display's choices put back as they were\n");
    }
    else if (result.hotkeys_dropped && result.route_written) snprintf(message, capacity, "%s Route written to %s", dropped, o.record_path);
    else if (result.hotkeys_dropped) snprintf(message, capacity, "%s", dropped);
    else if (result.route_written) snprintf(message, capacity, "Route written to %s", o.record_path);
    if (!result.error[0] && language_note[0]) oracles_ui_append_middle(message, capacity, language_note);
    if (!result.error[0] && result.language_note[0]) oracles_ui_append_middle(message, capacity, result.language_note);
    /* A game that ran slowly: the home screen suggests something lighter, once for the quality it ran at. */
    *slow = !result.error[0] && oracles_settings_slow_hint(games->settings, result.enhanced, result.frames_played, result.frames_late);
    if (*slow) oracles_settings_store(games->settings);
    /* Its savestate taken with other Display settings, which the pause's Load state chose: the game again with them, the
     * savestate loaded at its start, once the home screen has taken the window back. */
    if (result.state_restart && !result.window_closed) {
        games->state_restart = 1;
        snprintf(games->state_notice, sizeof games->state_notice, "%s", result.state_notice);
        games->state_previous = result.state_previous;
        message[0] = 0;
        return 2;
    }
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
    host->choose_eu_rom = choose_eu_rom;
    host->choose_edition = choose_edition;
    host->file_chosen = file_chosen;
    host->read_mods = read_mods;
    host->open_mods = open_mods;
    host->portable_turned = portable_turned;
#ifdef ORACLES_RETROACHIEVEMENTS
    host->achievements_login = achievements_login;
    host->achievements_answer = achievements_answer;
    host->achievements_cancel = achievements_cancel;
    host->achievements_logout = achievements_logout;
    host->achievements_list = achievements_list;
#endif
    host->start = start;
    oracles_home_save_host(games, host);
}
