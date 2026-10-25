/* A session's language (language_session.c), on the user's ROMs found by their SHA-1 in $ORACLES_ROM_DIR (skipped, exit
 * 77, without them): English gives no session; a pack is extracted from the European ROM found in a directory, whatever
 * the case of its extension, and written; read back; a pack of another fingerprint or of another game is extracted again
 * when the European ROM is given and refused without it; without the European ROM or its pack, or on another ROM, the
 * language is refused, with the loader's reason for a European ROM it refuses (a zip of two ROMs, a zip cut short); a
 * pack that cannot be written serves all the same, and the session says so.  The compact text's layout: English (US)
 * composed laid out, a language's identity with the layout's, the European ROM refused. */
#include "language_internal.h"
#include "language_session.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(path) _mkdir(path)
#else
#define make_dir(path) mkdir(path, 0777)
#endif

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

typedef struct rom_file { uint8_t *data; size_t size; } rom_file;

/* The ROM of `revision` in `dir`, or an empty one. */
static rom_file find(const char *dir, OraclesRomRevision revision)
{
    rom_file found = { NULL, 0 };
    DIR *listing = opendir(dir);
    struct dirent *entry;
    while (listing && !found.data && (entry = readdir(listing))) {
        const size_t n = strlen(entry->d_name);
        if (n < 4 || (strcmp(entry->d_name + n - 4, ".gbc") && strcmp(entry->d_name + n - 4, ".GBC"))) continue;
        char path[1024], error[128];
        snprintf(path, sizeof path, "%s/%s", dir, entry->d_name);
        size_t size = 0;
        uint8_t *data = oracles_rom_read_file(path, &size, error, sizeof error);
        OraclesRomInfo info;
        if (data && oracles_rom_identify(data, size, &info, error, sizeof error) == 0 && info.known && info.revision == revision) found = (rom_file){ data, size };
        else free(data);
    }
    if (listing) closedir(listing);
    return found;
}

static int write_file(const char *path, const uint8_t *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    const int written = fwrite(data, 1, size, f) == size;
    return (fclose(f) == 0) & written;
}

/* A zip of two stored ROMs of one byte, as the loader refuses it. */
static size_t two_rom_zip(uint8_t *out)
{
    static const char *const names[2] = { "a.gbc", "b.gbc" };
    size_t at = 0, local[2];
    const uint32_t crc = 0xe8b7be43u;   /* of "a" */
#define U16(v) do { out[at++] = (uint8_t)(v); out[at++] = (uint8_t)((v) >> 8); } while (0)
#define U32(v) do { U16((uint32_t)(v) & 0xffffu); U16((uint32_t)(v) >> 16); } while (0)
    for (int i = 0; i < 2; i++) {
        local[i] = at;
        U32(0x04034b50u); U16(20); U16(0); U16(0); U16(0); U16(0); U32(crc); U32(1); U32(1); U16(5); U16(0);
        memcpy(out + at, names[i], 5); at += 5;
        out[at++] = 'a';
    }
    const size_t directory = at;
    for (int i = 0; i < 2; i++) {
        U32(0x02014b50u); U16(20); U16(20); U16(0); U16(0); U16(0); U16(0); U32(crc); U32(1); U32(1); U16(5); U16(0); U16(0); U16(0); U16(0);
        U32(0); U32(local[i]);
        memcpy(out + at, names[i], 5); at += 5;
    }
    const size_t directory_size = at - directory;
    U32(0x06054b50u); U16(0); U16(0); U16(2); U16(2); U32(directory_size); U32(directory); U16(0);
#undef U16
#undef U32
    return at;
}

/* The session's report, where it says where its pack came from. */
static const char *report(const OraclesLanguageSession *session)
{
    static char text[4096];
    text[0] = 0;
    FILE *f = tmpfile();
    if (!f) return text;
    oracles_language_session_report(session, NULL, "", f);
    rewind(f);
    const size_t n = fread(text, 1, sizeof text - 1u, f);
    text[n] = 0;
    fclose(f);
    return text;
}

/* Starts Ages in French: the European ROM `european`, the pack `pack`; 0 or -1, the session in *session. */
static int start(OraclesLanguageSession **session, const char *european, const char *pack, const OraclesRomInfo *info, const rom_file *us,
                 char *error, size_t capacity)
{
    error[0] = 0;
    return oracles_language_session_start(session, "fr", european, pack, NULL, info, us->data, us->size, error, capacity);
}

int main(int argc, char **argv)
{
    const char *work = argc > 1 ? argv[1] : ".";
    char error[1024];
    OraclesLanguageSession *session = (OraclesLanguageSession *)&failures;
    OraclesRomInfo none = { 0 };
    /* English (US) is the cartridge's own: no session, whatever is given. */
    CHECK(oracles_language_session_start(&session, "en", NULL, NULL, NULL, &none, NULL, 0, error, sizeof error) == 0 && session == NULL);
    CHECK(oracles_language_session_start(&session, "klingon", NULL, NULL, NULL, &none, NULL, 0, error, sizeof error) == -1 && session == NULL);

    const char *dir = getenv("ORACLES_ROM_DIR");
    rom_file ages = { NULL, 0 }, ages_eu = { NULL, 0 }, seasons = { NULL, 0 }, seasons_eu = { NULL, 0 };
    if (dir && *dir) {
        ages = find(dir, ORACLES_ROM_REVISION_AGES_US);
        ages_eu = find(dir, ORACLES_ROM_REVISION_AGES_EU);
        seasons = find(dir, ORACLES_ROM_REVISION_SEASONS_US);
        seasons_eu = find(dir, ORACLES_ROM_REVISION_SEASONS_EU);
    }
    if (!ages.data || !ages_eu.data || !seasons.data || !seasons_eu.data) {
        printf("language-session: no US and European ROMs of both games in ORACLES_ROM_DIR: the ROMs' part skipped\n");
        free(ages.data); free(ages_eu.data); free(seasons.data); free(seasons_eu.data);
        if (failures) fprintf(stderr, "%d failure(s)\n", failures);
        return failures ? 1 : 77;
    }
    OraclesRomInfo info, eu_info;
    CHECK(oracles_rom_identify(ages.data, ages.size, &info, error, sizeof error) == 0);
    CHECK(oracles_rom_identify(ages_eu.data, ages_eu.size, &eu_info, error, sizeof error) == 0);
    char eu_dir[1024], empty[1024], pack[1100], eu_path[1100], unwritable[1100];
    snprintf(eu_dir, sizeof eu_dir, "%s/language-session-eu", work);
    snprintf(empty, sizeof empty, "%s/language-session-empty", work);
    snprintf(pack, sizeof pack, "%s/language-session-ages.pack", work);
    snprintf(eu_path, sizeof eu_path, "%s/AGES EU.GBC", eu_dir);
    snprintf(unwritable, sizeof unwritable, "%s/language-session-none/ages.pack", work);
    make_dir(eu_dir);
    make_dir(empty);
    CHECK(write_file(eu_path, ages_eu.data, ages_eu.size));
    remove(pack);
    /* English (US) on the US ROM, with all a language needs: no session, the image stays the ROM. */
    CHECK(oracles_language_session_start(&session, "en", eu_dir, pack, NULL, &info, ages.data, ages.size, error, sizeof error) == 0 && session == NULL);

    /* Neither the European ROM nor a pack; a directory without the European ROM; another ROM than the US one. */
    CHECK(start(&session, NULL, pack, &info, &ages, error, sizeof error) == -1 && session == NULL && strstr(error, "needs the European ROM"));
    CHECK(start(&session, empty, pack, &info, &ages, error, sizeof error) == -1 && strstr(error, "no European ROM"));
    /* A European ROM the loader refuses, given or in the directory given: its reason, the file named in a directory. */
    char refused_dir[1024], two_path[1100], cut_path[1100];
    snprintf(refused_dir, sizeof refused_dir, "%s/language-session-refused", work);
    snprintf(two_path, sizeof two_path, "%s/two.zip", refused_dir);
    snprintf(cut_path, sizeof cut_path, "%s/cut.ZIP", refused_dir);
    make_dir(refused_dir);
    uint8_t two[256];
    static const uint8_t cut[8] = { 'P', 'K', 3, 4 };
    CHECK(write_file(two_path, two, two_rom_zip(two)) && write_file(cut_path, cut, sizeof cut));
    CHECK(start(&session, two_path, pack, &info, &ages, error, sizeof error) == -1 && strstr(error, "no European ROM")
          && strstr(error, "the zip file holds several ROMs"));
    CHECK(start(&session, refused_dir, pack, &info, &ages, error, sizeof error) == -1
          && strstr(error, "two.zip: the zip file holds several ROMs") && strstr(error, "cut.ZIP: the zip file is damaged (no directory)"));
    remove(two_path);
    remove(cut_path);
    remove(refused_dir);
    CHECK(start(&session, eu_dir, pack, &eu_info, &ages_eu, error, sizeof error) == -1 && strstr(error, "US ROM of Ages or Seasons"));

    /* Extracted from the European ROM found in the directory (GBC in capitals), written; then read back. */
    CHECK(start(&session, eu_dir, pack, &info, &ages, error, sizeof error) == 0 && session != NULL);
    CHECK(strstr(report(session), "extracted from") && strstr(report(session), "written to") && !oracles_language_session_note(session)[0]);
    char identity[128];
    snprintf(identity, sizeof identity, "%s", oracles_language_session_identity(session));
    CHECK(!strncmp(identity, "language-fr@", 12) && strlen(identity) == 52u);
    oracles_language_session_stop(session);
    CHECK(start(&session, NULL, pack, &info, &ages, error, sizeof error) == 0 && session != NULL && strstr(report(session), "read from"));
    CHECK(!strcmp(oracles_language_session_identity(session), identity));   /* the same pack: a state taken with one loads with the other */
    oracles_language_session_stop(session);

    /* Another pack (a text changed, its fingerprint the same): another savestate identity. */
    OraclesLanguagePack *changed = oracles_language_pack_read(pack, error, sizeof error);
    CHECK(changed != NULL);
    if (changed) {
        changed->bytes[changed->texts[ORACLES_LANGUAGE_FRENCH][0].at] ^= 0x01u;
        CHECK(oracles_language_pack_write(changed, pack, error, sizeof error) == 0);
        oracles_language_pack_free(changed);
    }
    CHECK(start(&session, NULL, pack, &info, &ages, error, sizeof error) == 0 && session != NULL);
    CHECK(strcmp(oracles_language_session_identity(session), identity) != 0 && !strncmp(oracles_language_session_identity(session), "language-fr@", 12));
    oracles_language_session_stop(session);

    /* A pack of another fingerprint: refused without the European ROM, extracted again with it, and rewritten. */
    OraclesLanguagePack *stale = oracles_language_pack_read(pack, error, sizeof error);
    CHECK(stale != NULL);
    if (stale) {
        stale->fingerprint[0] = stale->fingerprint[0] == '0' ? '1' : '0';
        CHECK(oracles_language_pack_write(stale, pack, error, sizeof error) == 0);
        oracles_language_pack_free(stale);
    }
    CHECK(start(&session, NULL, pack, &info, &ages, error, sizeof error) == -1 && strstr(error, "another version"));
    CHECK(start(&session, eu_dir, pack, &info, &ages, error, sizeof error) == 0 && session != NULL);
    CHECK(strstr(report(session), "extracted from") && strstr(report(session), "another version"));
    oracles_language_session_stop(session);
    OraclesLanguagePack *again = oracles_language_pack_read(pack, error, sizeof error);
    CHECK(again != NULL);
    oracles_language_pack_free(again);

    /* The pack of another game: refused without the European ROM, extracted again with it. */
    OraclesLanguagePack *other = oracles_language_pack_extract(seasons_eu.data, seasons_eu.size, seasons.data, seasons.size, error, sizeof error);
    CHECK(other != NULL && oracles_language_pack_write(other, pack, error, sizeof error) == 0);
    oracles_language_pack_free(other);
    CHECK(start(&session, NULL, pack, &info, &ages, error, sizeof error) == -1 && strstr(error, "another game"));
    CHECK(start(&session, eu_dir, pack, &info, &ages, error, sizeof error) == 0 && session != NULL && strstr(report(session), "extracted from"));
    oracles_language_session_stop(session);

    /* A pack that cannot be written: the session plays with the pack in memory, and says so. */
    CHECK(start(&session, eu_path, unwritable, &info, &ages, error, sizeof error) == 0 && session != NULL);
    CHECK(strstr(oracles_language_session_note(session), "could not be written") && strstr(report(session), "not written to"));
    oracles_language_session_stop(session);

    /* The compact text's layout: English (US) gets a session of its own, its index served in "en"; a language keeps
     * its pack's identity and adds the layout's; the European ROM, which no layout plays on, is refused. */
    session = NULL;
    const OraclesLanguageLayout compact = { 128, 0, 0, NULL }, classic_wide = { 192, 1, 0, NULL }, compact_wide = { 192, 0, 0, NULL }, three = { 192, 0, 3, NULL };
    CHECK(oracles_language_session_lay_out(&session, &info, &compact, error, sizeof error) == 0 && session != NULL);
    CHECK(!strncmp(oracles_language_session_identity(session), "text-compact@", 13) && strlen(oracles_language_session_identity(session)) == 53u);
    uint8_t *image = malloc(ages.size);
    size_t size = ages.size;
    if (image) memcpy(image, ages.data, ages.size);
    CHECK(image && oracles_language_session_compose(session, &image, &size, error, sizeof error) == 0 && size == 2u * ages.size);
    CHECK(image && !memcmp(image + (size_t)ORACLES_IMAGE_LANGUAGE_BANK * 0x4000u + ORACLES_LANGUAGE_INDEX_CODE, "en\0", 3));
    CHECK(oracles_language_session_composed(session)->laid_out > 1000u && strstr(report(session), "compact text: the en texts laid out again"));
    free(image);
    oracles_language_session_stop(session);
    CHECK(start(&session, NULL, pack, &info, &ages, error, sizeof error) == 0 && session != NULL);
    snprintf(identity, sizeof identity, "%s", oracles_language_session_identity(session));
    CHECK(oracles_language_session_lay_out(&session, &info, &compact, error, sizeof error) == 0);
    CHECK(!strncmp(oracles_language_session_identity(session), identity, strlen(identity)) && strstr(oracles_language_session_identity(session), ",text-compact@"));
    /* The wide box's layouts: the mode, the width and the lines in the layout's name, each apart from the language's. */
    CHECK(oracles_language_session_lay_out(&session, &info, &classic_wide, error, sizeof error) == 0);
    CHECK(!strcmp(oracles_language_session_language_identity(session), identity) && !strncmp(oracles_language_session_layout_identity(session), "text-classic-192x2@", 19));
    char classic_name[96];
    snprintf(classic_name, sizeof classic_name, "%s", oracles_language_session_layout_identity(session));
    CHECK(oracles_language_session_lay_out(&session, &info, &compact_wide, error, sizeof error) == 0);
    CHECK(!strncmp(oracles_language_session_layout_identity(session), "text-compact-192x2@", 19) && strcmp(classic_name + 19, oracles_language_session_layout_identity(session) + 19) != 0);
    CHECK(strstr(oracles_language_session_identity(session), ",text-compact-192x2@") && !strncmp(oracles_language_session_identity(session), identity, strlen(identity)));
    CHECK(oracles_language_session_lay_out(&session, &info, &three, error, sizeof error) == -1);   /* three lines: not laid out yet */
    oracles_language_session_stop(session);
    /* Classic in the wide box, English (US): the US texts laid out at the fixed advance. */
    session = NULL;
    CHECK(oracles_language_session_lay_out(&session, &info, &classic_wide, error, sizeof error) == 0 && session != NULL);
    image = malloc(ages.size);
    size = ages.size;
    if (image) memcpy(image, ages.data, ages.size);
    CHECK(image && oracles_language_session_compose(session, &image, &size, error, sizeof error) == 0);
    CHECK(oracles_language_session_composed(session)->laid_out > 1000u && strstr(report(session), "classic text: the en texts laid out again on 192 pixels"));
    CHECK(oracles_language_session_composed(session)->boxes_laid_out < oracles_language_session_composed(session)->boxes);
    free(image);
    oracles_language_session_stop(session);
    session = NULL;
    CHECK(oracles_language_session_lay_out(&session, &eu_info, &compact, error, sizeof error) == -1 && session == NULL);
    /* A fan game made on Ages is laid out in its English alone, its kept texts its own. */
    OraclesRomInfo fan_info = info;
    fan_info.revision = ORACLES_ROM_REVISION_MOONRISE_1_0_6;
    CHECK(!oracles_rom_is_original(&fan_info) && oracles_language_session_lay_out(&session, &fan_info, &compact, error, sizeof error) == 0 && session != NULL);
    snprintf(identity, sizeof identity, "%s", oracles_language_session_identity(session));
    oracles_language_session_stop(session);
    session = NULL;
    CHECK(oracles_language_session_lay_out(&session, &fan_info, &classic_wide, error, sizeof error) == -1 && strstr(error, "game's text box alone") && !session);
    CHECK(oracles_language_session_lay_out(&session, &fan_info, &compact_wide, error, sizeof error) == -1 && !session);
    CHECK(oracles_language_session_lay_out(&session, &info, &compact, error, sizeof error) == 0 && strcmp(identity, oracles_language_session_identity(session)) != 0);
    oracles_language_session_stop(session);
    CHECK(start(&session, NULL, pack, &info, &ages, error, sizeof error) == 0 && session != NULL);
    CHECK(oracles_language_session_lay_out(&session, &fan_info, &compact, error, sizeof error) == -1 && strstr(error, "English alone"));
    oracles_language_session_stop(session);

    remove(pack);
    remove(eu_path);
    free(ages.data); free(ages_eu.data); free(seasons.data); free(seasons_eu.data);
    if (failures) fprintf(stderr, "%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
