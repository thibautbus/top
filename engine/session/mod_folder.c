/* The mods on disk (mod_folder.h). */
#include "mod_folder.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int ends_with(const char *text, const char *suffix)
{
    const size_t a = strlen(text), b = strlen(suffix);
    return a >= b && strcmp(text + a - b, suffix) == 0;
}

/* A file of a package, 1 MiB at most (engine/mods refuses a larger one). */
static char *read_file(const char *path, size_t *size)
{
    FILE *in = fopen(path, "rb");
    if (!in) return NULL;
    char *text = NULL;
    if (fseek(in, 0, SEEK_END) == 0) {
        const long length = ftell(in);
        if (length >= 0 && length <= 1024L * 1024L && fseek(in, 0, SEEK_SET) == 0 && (text = malloc((size_t)length + 1u)) != NULL) {
            if (fread(text, 1, (size_t)length, in) == (size_t)length) { text[length] = 0; *size = (size_t)length; }
            else { free(text); text = NULL; }
        }
    }
    fclose(in);
    return text;
}

OraclesMod *oracles_mod_folder_load(const char *dir, OraclesGame game, const uint8_t *rom, size_t rom_size, char *error, size_t capacity)
{
    DIR *listing = opendir(dir);
    if (!listing) { snprintf(error, capacity, "cannot open the mod directory %s", dir); return NULL; }
    OraclesModFile files[64];
    size_t count = 0;
    int ok = 1;
    struct dirent *entry;
    while (ok && (entry = readdir(listing)) != NULL) {
        if (!ends_with(entry->d_name, ".lua") || entry->d_name[0] == '.') continue;
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", dir, entry->d_name);
        char *name = malloc(strlen(entry->d_name) + 1u);
        size_t size = 0;
        char *text = read_file(path, &size);
        if (count == sizeof files / sizeof files[0] || !name || !text) {
            snprintf(error, capacity, count == sizeof files / sizeof files[0] ? "%s: too many files" : "cannot read %s", count == sizeof files / sizeof files[0] ? dir : path);
            free(name); free(text);
            ok = 0;
            break;
        }
        strcpy(name, entry->d_name);
        files[count++] = (OraclesModFile){ name, text, size };
    }
    closedir(listing);
    const char *base = dir + strlen(dir);
    while (base > dir && (base[-1] == '/' || base[-1] == '\\')) base--;
    const char *end = base;
    while (base > dir && base[-1] != '/' && base[-1] != '\\') base--;
    char id[128];
    snprintf(id, sizeof id, "%.*s", (int)(end - base), base);
    OraclesMod *mod = ok ? oracles_mod_load(id, files, count, game, rom, rom_size, error, capacity) : NULL;
    for (size_t i = 0; i < count; i++) { free((char *)files[i].name); free((char *)files[i].text); }
    return mod;
}

static int by_name(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static void describe(const char *folder, OraclesGame game, OraclesModEntry *entry)
{
    char dir[1100], error[512];
    snprintf(dir, sizeof dir, "%s/%s", folder, entry->name);
    for (int g = 0; g < 2; g++) {
        const OraclesGame each = g ? ORACLES_GAME_SEASONS : ORACLES_GAME_AGES;
        error[0] = 0;
        OraclesMod *mod = oracles_mod_folder_load(dir, each, NULL, 0, error, sizeof error);
        if (mod) entry->houses[g] = oracles_mod_house_count(mod);
        if (each == game) {
            if (mod) snprintf(entry->description, sizeof entry->description, "%s", oracles_mod_description(mod));
            else snprintf(entry->refusal, sizeof entry->refusal, "Refused: %s", error);
        }
        oracles_mod_free(mod);
    }
}

size_t oracles_mod_folder_list(const char *folder, OraclesGame game, OraclesModEntry *entries, size_t capacity)
{
    DIR *listing = opendir(folder);
    if (!listing) return 0;
    char **names = NULL;
    size_t found = 0, room = 0;
    struct dirent *entry;
    while ((entry = readdir(listing)) != NULL) {
        char path[1100];
        struct stat info;
        snprintf(path, sizeof path, "%s/%s", folder, entry->d_name);
        if (entry->d_name[0] == '.' || strlen(entry->d_name) >= sizeof entries->name || stat(path, &info) != 0 || !S_ISDIR(info.st_mode)) continue;
        if (found == room) {
            char **more = realloc(names, (room = room ? room * 2u : 16u) * sizeof *names);
            if (!more) break;
            names = more;
        }
        if ((names[found] = malloc(strlen(entry->d_name) + 1u)) == NULL) break;
        strcpy(names[found++], entry->d_name);
    }
    closedir(listing);
    if (found) qsort(names, found, sizeof *names, by_name);
    size_t count = 0;
    for (; count < found && count < capacity; count++) {
        memset(&entries[count], 0, sizeof entries[count]);
        snprintf(entries[count].name, sizeof entries[count].name, "%s", names[count]);
        describe(folder, game, &entries[count]);
    }
    for (size_t i = 0; i < found; i++) free(names[i]);
    free(names);
    return count;
}
