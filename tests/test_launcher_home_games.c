/* Oracles Combined as the home screen's host finds it, on the user's ROMs found in ORACLES_ROM_DIR (skipped without
 * them): its base, the European ROMs of Ages and Seasons, each on its row, with what is missing or wrong; a ROM chosen
 * or dropped on its page filed as the European ROM of its game, or refused; its patch dropped on the home screen found
 * to be its own on the two joined; another made for other ROMs refused; its image recognised, and built again when
 * Seasons' European ROM alone changes. */
#include "home_games.h"
#include "bps.h"
#include "file_dialog.h"
#include "rom.h"
#include "ui_home_nav.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utime.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); failures++; } } while (0)

typedef struct found_files {
    char eu_ages[1024], eu_seasons[1024], us_seasons[1024], combined[1024], other[1024];
} found_files;

static OraclesRomRevision revision_of(const char *path)
{
    char error[160];
    size_t size = 0;
    uint8_t *data = oracles_rom_read_file(path, &size, error, sizeof error);
    OraclesRomInfo info;
    const OraclesRomRevision revision = data && oracles_rom_identify(data, size, &info, error, sizeof error) == 0 ? info.revision : ORACLES_ROM_REVISION_UNKNOWN;
    free(data);
    return revision;
}

static void find_files(const char *dir, found_files *f)
{
    memset(f, 0, sizeof *f);
    DIR *listing = opendir(dir);
    struct dirent *entry;
    while (listing && (entry = readdir(listing))) {
        const size_t n = strlen(entry->d_name);
        if (n < 4 || strcmp(entry->d_name + n - 4, ".gbc")) continue;
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", dir, entry->d_name);
        const OraclesRomRevision revision = revision_of(path);
        char *into = revision == ORACLES_ROM_REVISION_AGES_EU ? f->eu_ages : revision == ORACLES_ROM_REVISION_SEASONS_EU ? f->eu_seasons
                     : revision == ORACLES_ROM_REVISION_SEASONS_US ? f->us_seasons : NULL;
        if (into) snprintf(into, sizeof f->eu_ages, "%s", path);
    }
    if (listing) closedir(listing);
    /* The English patch of Oracles Combined 1.5: the one whose image of the two joined is it; another, made for other
     * ROMs. */
    listing = f->eu_ages[0] && f->eu_seasons[0] ? opendir(dir) : NULL;
    while (listing && (entry = readdir(listing))) {
        const size_t n = strlen(entry->d_name);
        if (n < 4 || strcmp(entry->d_name + n - 4, ".bps")) continue;
        char path[1024], error[160];
        snprintf(path, sizeof path, "%s/%s", dir, entry->d_name);
        size_t size = 0;
        uint8_t *image = oracles_bps_apply_joined_files(f->eu_ages, f->eu_seasons, path, &size, error, sizeof error);
        OraclesRomInfo info;
        if (image && oracles_rom_identify(image, size, &info, error, sizeof error) == 0 && info.revision == ORACLES_ROM_REVISION_COMBINED_1_5_EN)
            snprintf(f->combined, sizeof f->combined, "%s", path);
        else if (!image && !strcmp(error, oracles_bps_other_base)) snprintf(f->other, sizeof f->other, "%s", path);
        free(image);
    }
    if (listing) closedir(listing);
}

static int copy_file(const char *from, const char *to)
{
    char error[160];
    size_t size = 0;
    uint8_t *data = oracles_rom_read_file(from, &size, error, sizeof error);
    FILE *out = data ? fopen(to, "wb") : NULL;
    const int copied = out && fwrite(data, 1, size, out) == size;
    if (out) fclose(out);
    free(data);
    return copied;
}

static OraclesHomeGames games;
static oracles_settings settings;
static OraclesHomeHost host;
static OraclesHomeNav nav;

static const OraclesHomeGame *refresh(void)
{
    oracles_home_init(&nav);
    host.refresh(host.opaque, &nav, NULL);
    return &nav.games[ORACLES_HOME_GAME_COMBINED];
}

int main(int argc, char **argv)
{
    const char *dir = getenv("ORACLES_ROM_DIR");
    if (!dir || !*dir) { printf("home-games: ORACLES_ROM_DIR is not set: skipped\n"); return 77; }
    if (argc < 2) { fprintf(stderr, "usage: oracles-test-launcher-home-games WORK_DIR\n"); return 2; }
    found_files f;
    find_files(dir, &f);
    if (!f.eu_ages[0] || !f.eu_seasons[0] || !f.us_seasons[0]) {
        printf("home-games: no European ROMs of Ages and Seasons and US ROM of Seasons in %s: skipped\n", dir);
        return 77;
    }
    oracles_settings_defaults(&settings);
    snprintf(settings.path, sizeof settings.path, "%s/settings.txt", argv[1]);
    memset(&games, 0, sizeof games);
    games.settings = &settings;
    oracles_home_games_host(&games, &host);
    char state[ORACLES_HOME_STATE_LENGTH], message[256];

    /* Its base: what is missing, then what is wrong, Ages' first. */
    const OraclesHomeGame *game = refresh();
    CHECK(game->rom == ORACLES_ROM_NONE && !strcmp(game->rom_reason, "No European ROM of Oracle of Ages chosen") && !game->usable);
    CHECK(game->joined[0].state == ORACLES_ROM_NONE && game->joined[1].state == ORACLES_ROM_NONE && !game->joined[0].file[0]);
    snprintf(settings.eu_rom[ORACLES_SETTINGS_AGES], sizeof settings.eu_rom[0], "%s", f.eu_ages);
    game = refresh();
    CHECK(game->rom == ORACLES_ROM_NONE && !strcmp(game->rom_reason, "No European ROM of Oracle of Seasons chosen"));
    /* Each on its row: Ages' recognized, with its file's name, Seasons' missing. */
    CHECK(game->joined[0].state == ORACLES_ROM_ORIGINAL && !strcmp(game->joined[0].file, strrchr(f.eu_ages, '/') + 1)
          && game->joined[1].state == ORACLES_ROM_NONE);
    oracles_home_state(&nav, ORACLES_HOME_HERO_COMBINED, state);
    CHECK(!strcmp(state, "ROM not found"));
    snprintf(settings.eu_rom[ORACLES_SETTINGS_SEASONS], sizeof settings.eu_rom[0], "%s", f.us_seasons);
    game = refresh();
    CHECK(game->rom == ORACLES_ROM_UNRECOGNISED && !strcmp(game->rom_reason, "Not the European ROM of Oracle of Seasons") && !game->usable);
    CHECK(game->joined[0].state == ORACLES_ROM_ORIGINAL && game->joined[1].state == ORACLES_ROM_UNRECOGNISED
          && !strcmp(game->joined[1].file, strrchr(f.us_seasons, '/') + 1));
    oracles_home_state(&nav, ORACLES_HOME_HERO_COMBINED, state);
    CHECK(!strcmp(state, "ROM refused"));
    settings.eu_rom[ORACLES_SETTINGS_SEASONS][0] = 0;

    /* Seasons' row's dialog, a European ROM's: another ROM refused with a toast, nothing kept. */
    message[0] = 0;
    host.file_chosen(host.opaque, ORACLES_HOME_GAME_COMBINED, ORACLES_DIALOG_EU_ROM, ORACLES_DIALOG_CHOSEN, f.us_seasons, 0, message, sizeof message);
    CHECK(!strcmp(message, "Refused: Oracles Combined is made of the European ROMs of Ages and Seasons") && !settings.eu_rom[ORACLES_SETTINGS_SEASONS][0]);

    /* A ROM chosen on its page: the US ROM of Seasons refused with a toast, the European ROM kept as Seasons'. */
    message[0] = 0;
    host.file_chosen(host.opaque, ORACLES_HOME_GAME_COMBINED, ORACLES_DIALOG_ROM, ORACLES_DIALOG_CHOSEN, f.us_seasons, 0, message, sizeof message);
    CHECK(!strcmp(message, "Refused: Oracles Combined is made of the European ROMs of Ages and Seasons") && !settings.eu_rom[ORACLES_SETTINGS_SEASONS][0]);
    message[0] = 0;
    host.file_chosen(host.opaque, ORACLES_HOME_GAME_COMBINED, ORACLES_DIALOG_ROM, ORACLES_DIALOG_CHOSEN, f.eu_seasons, 0, message, sizeof message);
    CHECK(!strcmp(message, "Oracle of Seasons European ROM set") && !strcmp(settings.eu_rom[ORACLES_SETTINGS_SEASONS], f.eu_seasons));
    /* Dropped on its page, the same. */
    message[0] = 0;
    CHECK(host.drop(host.opaque, f.us_seasons, ORACLES_HOME_GAME_COMBINED, message, sizeof message) == -1);
    CHECK(!strcmp(message, "Refused: Oracles Combined is made of the European ROMs of Ages and Seasons")
          && !strcmp(settings.eu_rom[ORACLES_SETTINGS_SEASONS], f.eu_seasons));
    game = refresh();
    CHECK(game->rom == ORACLES_ROM_ORIGINAL && game->patch == ORACLES_ROM_NONE && !game->usable);
    oracles_home_state(&nav, ORACLES_HOME_HERO_COMBINED, state);
    CHECK(!strcmp(state, "Patch not found"));

    if (!f.combined[0]) {
        printf("home-games: no English patch of Oracles Combined 1.5 in %s: its image not checked\n", dir);
        return failures ? 1 : 0;
    }
    /* Its patch dropped on the home screen: Oracles Combined's, made of the two joined. */
    message[0] = 0;
    CHECK(host.drop(host.opaque, f.combined, -1, message, sizeof message) == ORACLES_HOME_FAN);
    CHECK(!strcmp(message, "Oracles Combined patch set") && !strcmp(settings.patch[ORACLES_HOME_FAN_COMBINED], f.combined));
    game = refresh();
    CHECK(game->image == ORACLES_ROM_ORIGINAL && game->image_language && !strcmp(game->image_language, "English") && game->usable);
    CHECK(!strcmp(games.joined_found.path, f.eu_seasons));
    /* A patch made for other ROMs: refused, saying which it needs. */
    if (f.other[0]) {
        snprintf(settings.patch[ORACLES_HOME_FAN_COMBINED], sizeof settings.patch[0], "%s", f.other);
        game = refresh();
        CHECK(game->image == ORACLES_ROM_REFUSED && !strcmp(game->image_reason, "this patch is not made for the European ROMs of Ages and Seasons"));
        oracles_home_state(&nav, ORACLES_HOME_HERO_COMBINED, state);
        CHECK(!strcmp(state, "Patch refused"));
        snprintf(settings.patch[ORACLES_HOME_FAN_COMBINED], sizeof settings.patch[0], "%s", f.combined);
        game = refresh();
        CHECK(game->image == ORACLES_ROM_ORIGINAL);
    }
    /* Seasons' European ROM alone changed (another copy of it): the image is built again with it. */
    char copy[1100];
    snprintf(copy, sizeof copy, "%s/seasons-eu-copy.gbc", argv[1]);
    CHECK(copy_file(f.eu_seasons, copy));
    snprintf(settings.eu_rom[ORACLES_SETTINGS_SEASONS], sizeof settings.eu_rom[0], "%s", copy);
    game = refresh();
    CHECK(game->image == ORACLES_ROM_ORIGINAL && game->usable && !strcmp(games.joined_found.path, copy));
    /* The same file changed in place into another ROM (dated otherwise, the second it was copied in being the same):
     * read again, refused. */
    CHECK(copy_file(f.us_seasons, copy));
    struct utimbuf dated = { 1000000000, 1000000000 };
    CHECK(utime(copy, &dated) == 0);
    game = refresh();
    CHECK(game->rom == ORACLES_ROM_UNRECOGNISED && !game->usable);
    remove(copy);
    oracles_home_files_forget(&games.files);
    printf("home-games: Oracles Combined's base, its ROMs chosen and dropped, its patch and its image checked\n");
    if (failures) fprintf(stderr, "%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
