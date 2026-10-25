/* The home screen's file cache (home_files.c) on files written here: a path read as a ROM and as a patch kept apart, a
 * refusal's reason given again, a file rewritten with another size read again, the file asked for longest ago giving
 * its place to another once every slot is taken while the others stay, and every file freed when forgotten. */
#include "home_files.h"

#include "bps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

static int write_file(const char *path, const void *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    const int written = f && fwrite(data, 1, size, f) == size;
    if (f) fclose(f);
    return written;
}

/* A zip of two stored files, `names` holding `texts`. */
static size_t stored_zip(uint8_t *out, const char *const names[2], const char *const texts[2])
{
    size_t at = 0, local[2];
#define U16(v) do { out[at++] = (uint8_t)(v); out[at++] = (uint8_t)((v) >> 8); } while (0)
#define U32(v) do { U16((uint32_t)(v) & 0xffffu); U16((uint32_t)(v) >> 16); } while (0)
    for (int i = 0; i < 2; i++) {
        const size_t n = strlen(names[i]), size = strlen(texts[i]);
        local[i] = at;
        U32(0x04034b50u); U16(20); U16(0); U16(0); U16(0); U16(0);
        U32(oracles_crc32((const uint8_t *)texts[i], size)); U32(size); U32(size); U16(n); U16(0);
        memcpy(out + at, names[i], n); at += n;
        memcpy(out + at, texts[i], size); at += size;
    }
    const size_t directory = at;
    for (int i = 0; i < 2; i++) {
        const size_t n = strlen(names[i]), size = strlen(texts[i]);
        U32(0x02014b50u); U16(20); U16(20); U16(0); U16(0); U16(0); U16(0);
        U32(oracles_crc32((const uint8_t *)texts[i], size)); U32(size); U32(size); U16(n); U16(0); U16(0); U16(0); U16(0);
        U32(0); U32(local[i]);
        memcpy(out + at, names[i], n); at += n;
    }
    const size_t directory_size = at - directory;
    U32(0x06054b50u); U16(0); U16(0); U16(2); U16(2); U32(directory_size); U32(directory); U16(0);
#undef U16
#undef U32
    return at;
}

/* `data` of `size` bytes is `text`. */
static int holds(const uint8_t *data, size_t size, const char *text)
{
    return data && size == strlen(text) && memcmp(data, text, size) == 0;
}

int main(int argc, char **argv)
{
    const char *work = argc > 1 ? argv[1] : ".";
    static OraclesHomeFiles files;
    char reason[ORACLES_HOME_TEXT_LENGTH], path[ORACLES_HOME_FILES + 1][1024], zip_path[1024], archive_path[1024];
    size_t size = 0;

    /* One path as a ROM and as a patch: each reader's file, in a slot of its own. */
    static const char *const names[2] = { "Oracle of Ages.gbc", "Moonrise Regalia.bps" }, *const texts[2] = { "the ROM", "the patch" };
    uint8_t zip[512];
    snprintf(zip_path, sizeof zip_path, "%s/home_files_both.zip", work);
    CHECK(write_file(zip_path, zip, stored_zip(zip, names, texts)));
    const uint8_t *rom = oracles_home_file(&files, zip_path, ORACLES_ZIP_ROM, &size, reason, sizeof reason);
    CHECK(holds(rom, size, "the ROM"));
    const uint8_t *patch = oracles_home_file(&files, zip_path, ORACLES_ZIP_PATCH, &size, reason, sizeof reason);
    CHECK(holds(patch, size, "the patch") && patch != rom);
    CHECK(oracles_home_file(&files, zip_path, ORACLES_ZIP_ROM, &size, reason, sizeof reason) == rom && holds(rom, size, "the ROM"));

    /* A refused file, its reason given again. */
    static const uint8_t seven[8] = { '7', 'z', 0xbc, 0xaf, 0x27, 0x1c, 0, 4 };
    snprintf(archive_path, sizeof archive_path, "%s/home_files.7z", work);
    CHECK(write_file(archive_path, seven, sizeof seven));
    CHECK(!oracles_home_file(&files, archive_path, ORACLES_ZIP_ROM, &size, reason, sizeof reason) && strstr(reason, "7z archive"));
    snprintf(reason, sizeof reason, "not given");
    CHECK(!oracles_home_file(&files, archive_path, ORACLES_ZIP_ROM, &size, reason, sizeof reason) && strstr(reason, "7z archive"));

    /* A file rewritten with another size, read again. */
    for (unsigned i = 0; i <= ORACLES_HOME_FILES; i++) snprintf(path[i], sizeof path[i], "%s/home_files_%u.gbc", work, i);
    CHECK(write_file(path[0], "short", 5));
    const uint8_t *data = oracles_home_file(&files, path[0], ORACLES_ZIP_ROM, &size, reason, sizeof reason);
    CHECK(holds(data, size, "short"));
    CHECK(write_file(path[0], "much longer", 11));
    data = oracles_home_file(&files, path[0], ORACLES_ZIP_ROM, &size, reason, sizeof reason);
    CHECK(holds(data, size, "much longer"));

    /* Every slot taken by a file of its own, the first asked for again: a ninth takes the place of the second, which
     * comes back read again, and every other file stays as it was given. */
    oracles_home_files_forget(&files);
    const uint8_t *given[ORACLES_HOME_FILES + 1];
    size_t sizes[ORACLES_HOME_FILES + 1];
    char text[ORACLES_HOME_FILES + 1][16];
    for (unsigned i = 0; i <= ORACLES_HOME_FILES; i++) {
        snprintf(text[i], sizeof text[i], "file %u", i);
        CHECK(write_file(path[i], text[i], strlen(text[i])));
    }
    for (unsigned i = 0; i < ORACLES_HOME_FILES; i++) given[i] = oracles_home_file(&files, path[i], ORACLES_ZIP_ROM, &sizes[i], reason, sizeof reason);
    CHECK(oracles_home_file(&files, path[0], ORACLES_ZIP_ROM, &size, reason, sizeof reason) == given[0]);
    given[ORACLES_HOME_FILES] = oracles_home_file(&files, path[ORACLES_HOME_FILES], ORACLES_ZIP_ROM, &sizes[ORACLES_HOME_FILES], reason, sizeof reason);
    for (unsigned i = 0; i <= ORACLES_HOME_FILES; i++) if (i != 1) CHECK(holds(given[i], sizes[i], text[i]));
    data = oracles_home_file(&files, path[1], ORACLES_ZIP_ROM, &size, reason, sizeof reason);
    CHECK(holds(data, size, "file 1"));

    /* Forgotten: every slot free, every file freed. */
    oracles_home_files_forget(&files);
    for (unsigned i = 0; i < ORACLES_HOME_FILES; i++) CHECK(!files.file[i].path[0] && !files.file[i].data);
    for (unsigned i = 0; i <= ORACLES_HOME_FILES; i++) remove(path[i]);
    remove(zip_path);
    remove(archive_path);

    if (failures) return 1;
    puts("test_launcher_home_files: ok");
    return 0;
}
