/* Oracles Combined as the home screen's host finds it, on the user's ROMs found in ORACLES_ROM_DIR (skipped without
 * them): its base, the European ROMs of Ages and Seasons, each on its row, with what is missing or wrong; a ROM chosen
 * or dropped on its page filed as the European ROM of its game, or refused; its patch dropped on the home screen found
 * to be its own on the two joined; another made for other ROMs refused; its image recognised, and built again when
 * Seasons' European ROM alone changes.  An Oracle's editions: its Japanese ROM and Kaxus's patch chosen, dropped and
 * removed, and the command line's --eu-rom taken for Japanese or Polish only when it is that language's source. */
#include "home_games.h"
#include "bps.h"
#include "file_dialog.h"
#include "rom.h"
#include "ui_home_nav.h"
#include "ui_page_nav.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utime.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); failures++; } } while (0)

typedef struct found_files {
    char eu_ages[1024], eu_seasons[1024], us_seasons[1024], combined[1024], other[1024];
    char us_ages[1024], jp_ages[1024], jp_seasons[1024], pl_ages[1024], pl_seasons[1024];   /* the editions' test: Kaxus's patches, found by name */
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
                     : revision == ORACLES_ROM_REVISION_SEASONS_US ? f->us_seasons : revision == ORACLES_ROM_REVISION_AGES_US ? f->us_ages
                     : revision == ORACLES_ROM_REVISION_AGES_JP ? f->jp_ages : revision == ORACLES_ROM_REVISION_SEASONS_JP ? f->jp_seasons : NULL;
        if (into) snprintf(into, sizeof f->eu_ages, "%s", path);
    }
    if (listing) closedir(listing);
    listing = opendir(dir);
    while (listing && (entry = readdir(listing)))
        if (!strcmp(entry->d_name, "Oracle of Ages - Patch PL.bps")) snprintf(f->pl_ages, sizeof f->pl_ages, "%s/%s", dir, entry->d_name);
        else if (!strcmp(entry->d_name, "Oracle of Seasons - Patch PL.bps")) snprintf(f->pl_seasons, sizeof f->pl_seasons, "%s/%s", dir, entry->d_name);
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
        printf("home-games: no English patch of Oracles Combined 1.5 in %s: its image not checked, and an Oracle's editions skipped\n", dir);
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

    /* Cartridge › Editions of Ages: its Japanese ROM and Kaxus's patch chosen in their dialogs, kept and recognized,
     * another file said not to be the game's and setting the language back to English; removed on the page and stored,
     * gone from settings.txt with the language they gave. */
    if (f.us_ages[0] && f.jp_ages[0] && f.jp_seasons[0] && f.pl_ages[0]) {
        snprintf(settings.rom[ORACLES_SETTINGS_AGES], sizeof settings.rom[0], "%s", f.us_ages);
        host.file_chosen(host.opaque, ORACLES_HOME_GAME_AGES, ORACLES_DIALOG_JP_ROM, ORACLES_DIALOG_CHOSEN, f.jp_ages, 0, message, sizeof message);
        host.file_chosen(host.opaque, ORACLES_HOME_GAME_AGES, ORACLES_DIALOG_PL_ROM, ORACLES_DIALOG_CHOSEN, f.pl_ages, 0, message, sizeof message);
        CHECK(!strcmp(settings.jp_rom[ORACLES_SETTINGS_AGES], f.jp_ages) && !strcmp(settings.pl_rom[ORACLES_SETTINGS_AGES], f.pl_ages));
        /* --eu-rom without --language, Ages in Japanese or Polish: taken when it is that language's ROM or patch, by its
         * SHA-1 (copies elsewhere); a European ROM, the other language's source or the other game's, set aside for
         * jp_rom_ages= or pl_rom_ages=, and nothing without them. */
        char jp_copy[1100], pl_copy[1100];
        snprintf(jp_copy, sizeof jp_copy, "%s/ages-jp-copy.gbc", argv[1]);
        snprintf(pl_copy, sizeof pl_copy, "%s/ages-pl-copy.bps", argv[1]);
        CHECK(copy_file(f.jp_ages, jp_copy) && copy_file(f.pl_ages, pl_copy));
        const char *const jp_set = settings.jp_rom[ORACLES_SETTINGS_AGES], *const pl_set = settings.pl_rom[ORACLES_SETTINGS_AGES];
        CHECK(oracles_home_own_source(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_JAPANESE, NULL) == jp_set);
        CHECK(oracles_home_own_source(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_JAPANESE, jp_copy) == jp_copy);
        CHECK(oracles_home_own_source(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_JAPANESE, f.eu_ages) == jp_set);
        CHECK(oracles_home_own_source(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_JAPANESE, pl_copy) == jp_set);
        CHECK(oracles_home_own_source(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_JAPANESE, f.jp_seasons) == jp_set);
        CHECK(oracles_home_own_source(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_POLISH, NULL) == pl_set);
        CHECK(oracles_home_own_source(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_POLISH, pl_copy) == pl_copy);
        CHECK(oracles_home_own_source(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_POLISH, f.eu_ages) == pl_set);
        CHECK(oracles_home_own_source(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_POLISH, jp_copy) == pl_set);
        settings.jp_rom[ORACLES_SETTINGS_AGES][0] = 0;
        CHECK(oracles_home_own_source(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_JAPANESE, f.eu_ages) == NULL);
        CHECK(oracles_home_own_source(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_JAPANESE, jp_copy) == jp_copy);
        snprintf(settings.jp_rom[ORACLES_SETTINGS_AGES], sizeof settings.jp_rom[0], "%s", f.jp_ages);
        /* The Polish ROM Kaxus's patch makes, given as a .gbc: Polish's source, not Japanese's. */
        char pl_gbc[1100], refused[160];
        snprintf(pl_gbc, sizeof pl_gbc, "%s/ages-pl.gbc", argv[1]);
        size_t us_size = 0, patch_size = 0, made_size = 0;
        uint8_t *us = oracles_rom_read_file(f.us_ages, &us_size, refused, sizeof refused);
        uint8_t *patch = oracles_bps_read_file(f.pl_ages, &patch_size, refused, sizeof refused);
        uint8_t *made = us && patch ? oracles_bps_apply(us, us_size, patch, patch_size, &made_size, refused, sizeof refused) : NULL;
        FILE *out = made ? fopen(pl_gbc, "wb") : NULL;
        CHECK(out && fwrite(made, 1, made_size, out) == made_size);
        if (out) fclose(out);
        free(us);
        free(patch);
        free(made);
        CHECK(oracles_home_own_source(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_POLISH, pl_gbc) == pl_gbc);
        CHECK(oracles_home_own_source(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_JAPANESE, pl_gbc) == jp_set);
        /* Start game's decision: the session's source and language, a European ROM set aside, --language with --eu-rom
         * left as given; another file in settings.txt, or none, said and played in English. */
        char note[ORACLES_HOME_TEXT_LENGTH + 128] = "";
        OraclesSessionOptions o = { .eu_rom_path = f.eu_ages };
        oracles_home_own_language(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_JAPANESE, 1, &o, note, sizeof note);
        CHECK(o.eu_rom_path == jp_set && o.language && !strcmp(o.language, "ja") && !note[0]);
        o = (OraclesSessionOptions){ .eu_rom_path = jp_copy };
        oracles_home_own_language(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_JAPANESE, 1, &o, note, sizeof note);
        CHECK(o.eu_rom_path == jp_copy && o.language && !strcmp(o.language, "ja") && !note[0]);
        o = (OraclesSessionOptions){ .eu_rom_path = f.eu_ages };
        oracles_home_own_language(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_POLISH, 1, &o, note, sizeof note);
        CHECK(o.eu_rom_path == pl_set && o.language && !strcmp(o.language, "pl") && !note[0]);
        o = (OraclesSessionOptions){ .eu_rom_path = pl_gbc };
        oracles_home_own_language(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_POLISH, 1, &o, note, sizeof note);
        CHECK(o.eu_rom_path == pl_gbc && o.language && !strcmp(o.language, "pl") && !note[0]);
        o = (OraclesSessionOptions){ .language = "ja", .eu_rom_path = f.eu_ages };
        oracles_home_own_language(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_JAPANESE, 1, &o, note, sizeof note);
        CHECK(o.eu_rom_path == f.eu_ages && !strcmp(o.language, "ja"));
        o = (OraclesSessionOptions){ .language = "ja" };
        oracles_home_own_language(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_JAPANESE, 1, &o, note, sizeof note);
        CHECK(o.eu_rom_path == jp_set && o.language && !strcmp(o.language, "ja"));
        o = (OraclesSessionOptions){ .eu_rom_path = f.eu_ages };
        oracles_home_own_language(&games, ORACLES_SETTINGS_AGES, 0, 1, &o, note, sizeof note);
        CHECK(o.eu_rom_path == f.eu_ages && !o.language);
        oracles_home_own_language(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_JAPANESE, 0, &o, note, sizeof note);
        CHECK(o.eu_rom_path == jp_set && !o.language && !note[0]);
        char pl_kept[ORACLES_SETTINGS_PATH_LENGTH];
        snprintf(pl_kept, sizeof pl_kept, "%s", settings.pl_rom[ORACLES_SETTINGS_AGES]);
        snprintf(settings.pl_rom[ORACLES_SETTINGS_AGES], sizeof settings.pl_rom[0], "%s", f.jp_ages);
        o = (OraclesSessionOptions){ .eu_rom_path = f.eu_ages };
        oracles_home_own_language(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_POLISH, 1, &o, note, sizeof note);
        CHECK(o.eu_rom_path == pl_set && !o.language && strstr(note, "is not the Polish ROM of Oracle of Ages nor its patch: the game played in English."));
        settings.pl_rom[ORACLES_SETTINGS_AGES][0] = 0;
        o = (OraclesSessionOptions){ .eu_rom_path = f.eu_ages };
        oracles_home_own_language(&games, ORACLES_SETTINGS_AGES, ORACLES_SETTINGS_POLISH, 1, &o, note, sizeof note);
        CHECK(!o.eu_rom_path && !o.language && !strncmp(note, "No Polish ROM of Oracle of Ages: the game played in English.", 60));
        snprintf(settings.pl_rom[ORACLES_SETTINGS_AGES], sizeof settings.pl_rom[0], "%s", pl_kept);
        /* Seasons alike: its Japanese ROM and Kaxus's patch of it taken, Ages' set aside, nothing in its settings. */
        if (f.pl_seasons[0]) {
            CHECK(oracles_home_own_source(&games, ORACLES_SETTINGS_SEASONS, ORACLES_SETTINGS_JAPANESE, f.jp_seasons) == f.jp_seasons);
            CHECK(oracles_home_own_source(&games, ORACLES_SETTINGS_SEASONS, ORACLES_SETTINGS_JAPANESE, f.jp_ages) == NULL);
            CHECK(oracles_home_own_source(&games, ORACLES_SETTINGS_SEASONS, ORACLES_SETTINGS_POLISH, f.pl_seasons) == f.pl_seasons);
            CHECK(oracles_home_own_source(&games, ORACLES_SETTINGS_SEASONS, ORACLES_SETTINGS_POLISH, f.pl_ages) == NULL);
            CHECK(oracles_home_own_source(&games, ORACLES_SETTINGS_SEASONS, ORACLES_SETTINGS_POLISH, f.eu_seasons) == NULL);
            o = (OraclesSessionOptions){ .eu_rom_path = f.pl_seasons };
            oracles_home_own_language(&games, ORACLES_SETTINGS_SEASONS, ORACLES_SETTINGS_POLISH, 1, &o, note, sizeof note);
            CHECK(o.eu_rom_path == f.pl_seasons && o.language && !strcmp(o.language, "pl") && !note[0]);
            o = (OraclesSessionOptions){ .eu_rom_path = f.pl_ages };
            oracles_home_own_language(&games, ORACLES_SETTINGS_SEASONS, ORACLES_SETTINGS_POLISH, 1, &o, note, sizeof note);
            CHECK(!o.eu_rom_path && !o.language && !strncmp(note, "No Polish ROM of Oracle of Seasons", 34));
        } else printf("home-games: no Kaxus's patch of Seasons in %s: Seasons' editions skipped\n", dir);
        remove(pl_gbc);
        oracles_home_files_forget(&games.files);
        remove(jp_copy);
        remove(pl_copy);
        settings.language[ORACLES_SETTINGS_AGES] = ORACLES_SETTINGS_JAPANESE;
        refresh();
        OraclesHomeGame *ages = &nav.games[ORACLES_HOME_GAME_AGES];
        CHECK(ages->editions[0].state == ORACLES_ROM_ORIGINAL && ages->editions[1].state == ORACLES_ROM_ORIGINAL && ages->editions[1].patch == 1);
        CHECK(!strcmp(ages->editions[0].file, strrchr(f.jp_ages, '/') + 1) && oracles_page_language(&nav, ages) == ORACLES_PAGE_JAPANESE);
        nav.entry = ORACLES_HOME_AGES;
        OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
        oracles_home_items(&nav, items);
        CHECK(items[1].note && !strcmp(items[1].note, "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"));
        /* Seasons' Japanese ROM for Ages: not its own, Japanese back to English. */
        host.file_chosen(host.opaque, ORACLES_HOME_GAME_AGES, ORACLES_DIALOG_JP_ROM, ORACLES_DIALOG_CHOSEN, f.jp_seasons, 0, message, sizeof message);
        refresh();
        CHECK(ages->editions[0].state == ORACLES_ROM_UNRECOGNISED && settings.language[ORACLES_SETTINGS_AGES] == 0);
        /* Polski chosen, then its source removed on Cartridge › Editions: the setting and the language go when stored. */
        settings.language[ORACLES_SETTINGS_AGES] = ORACLES_SETTINGS_POLISH;
        refresh();
        nav.entry = ORACLES_HOME_AGES;
        nav.screen = ORACLES_SCREEN_GAME;
        nav.editions = 1;
        nav.row = ORACLES_ROW_PL_ROM;
        nav.eu_action = 1;
        CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && ages->language == 0);
        host.store(host.opaque, &nav);
        CHECK(!settings.pl_rom[ORACLES_SETTINGS_AGES][0] && settings.language[ORACLES_SETTINGS_AGES] == 0 && settings.jp_rom[ORACLES_SETTINGS_AGES][0]);
        /* Japanese chosen without its source (written by hand, or the file not chosen yet): kept when stored, the game
         * in English meanwhile, Japanese back with the source. */
        settings.jp_rom[ORACLES_SETTINGS_AGES][0] = 0;
        settings.language[ORACLES_SETTINGS_AGES] = ORACLES_SETTINGS_JAPANESE;
        refresh();
        CHECK(ages->editions[0].state == ORACLES_ROM_NONE && oracles_page_language(&nav, ages) == 0);
        host.store(host.opaque, &nav);
        CHECK(settings.language[ORACLES_SETTINGS_AGES] == ORACLES_SETTINGS_JAPANESE);
        /* Dropped on Ages' page, Cartridge's or Cartridge › Editions': the Japanese ROM goes to its Japanese ROM, never to
         * rom_ages=, a toast saying so; and Japanese comes back with it. */
        message[0] = 0;
        CHECK(host.drop(host.opaque, f.jp_ages, ORACLES_HOME_GAME_AGES, message, sizeof message) == -1);
        CHECK(!strcmp(settings.rom[ORACLES_SETTINGS_AGES], f.us_ages) && !strcmp(settings.jp_rom[ORACLES_SETTINGS_AGES], f.jp_ages));
        CHECK(!strcmp(message, "Oracle of Ages Japanese ROM set in Cartridge \xe2\x80\xba Editions"));
        refresh();
        CHECK(oracles_page_language(&nav, ages) == ORACLES_PAGE_JAPANESE);
        /* Kaxus's patch dropped on the home screen: Ages' Polish source, the home screen then on Ages. */
        message[0] = 0;
        CHECK(host.drop(host.opaque, f.pl_ages, -1, message, sizeof message) == ORACLES_HOME_AGES);
        CHECK(!strcmp(settings.pl_rom[ORACLES_SETTINGS_AGES], f.pl_ages) && !strcmp(message, "Oracle of Ages Polish ROM or patch set in Cartridge \xe2\x80\xba Editions"));
        CHECK(!strcmp(settings.rom[ORACLES_SETTINGS_AGES], f.us_ages));
        /* Seasons' Japanese ROM dropped on Ages' page: Seasons' Japanese ROM, Ages' untouched. */
        CHECK(host.drop(host.opaque, f.jp_seasons, ORACLES_HOME_GAME_AGES, message, sizeof message) == -1);
        CHECK(!strcmp(settings.jp_rom[ORACLES_SETTINGS_SEASONS], f.jp_seasons) && !strcmp(settings.jp_rom[ORACLES_SETTINGS_AGES], f.jp_ages));
        CHECK(!strcmp(settings.rom[ORACLES_SETTINGS_AGES], f.us_ages) && !settings.rom[ORACLES_SETTINGS_SEASONS][0]);
        CHECK(!strcmp(message, "Oracle of Seasons Japanese ROM set in Cartridge \xe2\x80\xba Editions: it plays with the US ROM, still to choose"));
        /* Choose ROM… given the Japanese ROM: never Ages' ROM, filed in Cartridge › Editions, the toast saying so. */
        settings.jp_rom[ORACLES_SETTINGS_AGES][0] = 0;
        message[0] = 0;
        host.file_chosen(host.opaque, ORACLES_HOME_GAME_AGES, ORACLES_DIALOG_ROM, ORACLES_DIALOG_CHOSEN, f.jp_ages, 0, message, sizeof message);
        CHECK(!strcmp(settings.rom[ORACLES_SETTINGS_AGES], f.us_ages) && !strcmp(settings.jp_rom[ORACLES_SETTINGS_AGES], f.jp_ages));
        CHECK(!strcmp(message, "Oracle of Ages Japanese ROM set in Cartridge \xe2\x80\xba Editions"));
        /* Seasons' European ROM dropped on Ages' page: Seasons' European ROM, a toast saying so, Ages' untouched. */
        settings.eu_rom[ORACLES_SETTINGS_AGES][0] = settings.eu_rom[ORACLES_SETTINGS_SEASONS][0] = 0;
        message[0] = 0;
        CHECK(host.drop(host.opaque, f.eu_seasons, ORACLES_HOME_GAME_AGES, message, sizeof message) == -1);
        CHECK(!strcmp(settings.eu_rom[ORACLES_SETTINGS_SEASONS], f.eu_seasons) && !settings.eu_rom[ORACLES_SETTINGS_AGES][0]);
        CHECK(!strcmp(message, "Oracle of Seasons European ROM set"));
        message[0] = 0;
        CHECK(host.drop(host.opaque, f.eu_ages, ORACLES_HOME_GAME_AGES, message, sizeof message) == -1 && !message[0]);
        CHECK(!strcmp(settings.eu_rom[ORACLES_SETTINGS_AGES], f.eu_ages));
        settings.eu_rom[ORACLES_SETTINGS_AGES][0] = settings.eu_rom[ORACLES_SETTINGS_SEASONS][0] = 0;
        settings.rom[ORACLES_SETTINGS_AGES][0] = settings.jp_rom[ORACLES_SETTINGS_AGES][0] = settings.pl_rom[ORACLES_SETTINGS_AGES][0] = 0;
        settings.jp_rom[ORACLES_SETTINGS_SEASONS][0] = 0;
        settings.language[ORACLES_SETTINGS_AGES] = 0;
    } else {
        printf("home-games: no US ROM of Ages, Japanese ROMs of Ages and Seasons or Kaxus's patch of Ages in %s: the editions skipped\n", dir);
    }
    oracles_home_files_forget(&games.files);
    printf("home-games: Oracles Combined's base, its ROMs chosen and dropped, its patch and its image, and an Oracle's editions checked\n");
    if (failures) fprintf(stderr, "%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
