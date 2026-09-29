/* What a session plays with, from its options and the settings (session.h): its defaults, its save, and the mods of
 * the Mods page. */
#include "session.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

void oracles_session_defaults(OraclesSessionOptions *options)
{
    memset(options, 0, sizeof *options);
    options->neighbour_objects = 1;
    options->sdl.scale = 4;
}

void oracles_session_default_save_path(const char *rom_path, char *out, size_t capacity)
{
    snprintf(out, capacity, "%s", rom_path);
    char *dot = strrchr(out, '.');
    const char *slash = strrchr(out, '/');
    const char *backslash = strrchr(out, '\\');
    if (backslash && (!slash || backslash > slash)) slash = backslash;
    if (dot && (!slash || dot > slash)) *dot = 0;
    const size_t length = strlen(out);
    snprintf(out + length, capacity - length, ".sav");
}

void oracles_session_save_path(const OraclesSessionOptions *o, char *out, size_t capacity)
{
    if (o->save_path) { snprintf(out, capacity, "%s", o->save_path); return; }
    oracles_session_default_save_path(o->patch_path ? o->patch_path : o->rom_path, out, capacity);
    const size_t length = strlen(out);
    if (o->mods_count && length >= 4u) snprintf(out + length - 4u, capacity - (length - 4u), ".mods.sav");   /* for ".sav" */
}

unsigned oracles_session_home_mods(OraclesSessionOptions *o, const oracles_settings *settings, OraclesSettingsGame g,
                                   char dirs[ORACLES_SETTINGS_MODS_MAX][ORACLES_SETTINGS_PATH_LENGTH])
{
    char folder[ORACLES_SETTINGS_PATH_LENGTH], names[ORACLES_SETTINGS_MODS_MAX][ORACLES_SETTINGS_MOD_NAME];
    oracles_settings_mods_folder(settings, folder, sizeof folder);
    const size_t count = folder[0] ? oracles_settings_mod_names(settings->mods[g], names, ORACLES_SETTINGS_MODS_MAX) : 0;
    o->mods_count = 0;
    for (size_t i = 0; i < count; i++) {
        struct stat info;
        char *dir = dirs[o->mods_count];
        if (snprintf(dir, ORACLES_SETTINGS_PATH_LENGTH, "%s/%s", folder, names[i]) >= ORACLES_SETTINGS_PATH_LENGTH) continue;
        if (stat(dir, &info) != 0 || !S_ISDIR(info.st_mode)) { fprintf(stderr, "oracles: mod %s is no longer in %s: it plays without it\n", names[i], folder); continue; }
        o->mods_dirs[o->mods_count++] = dir;
    }
    return o->mods_count;
}
