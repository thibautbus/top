#include "save_transfer.h"

#include "file_dialog.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

int oracles_save_transfer_check(const uint8_t *data, size_t size, OraclesGame engine, int oracle, uint8_t *out, int *english)
{
    *english = 0;
    if (!oracles_save_valid(data, size, engine)) return 0;
    memcpy(out, data, ORACLES_SAVE_SIZE);
    *english = oracle && oracles_save_set_english(out, ORACLES_SAVE_SIZE, engine) != 0;
    return 1;
}

int oracles_save_transfer_read(const char *path, uint8_t *out, size_t *size, char *error, size_t error_capacity)
{
    *size = 0;
    SDL_IOStream *in = SDL_IOFromFile(path, "rb");
    if (!in) { snprintf(error, error_capacity, "%s", SDL_GetError()); return 0; }
    if (SDL_GetIOSize(in) > (Sint64)ORACLES_SAVE_SIZE) {
        SDL_CloseIO(in);
        snprintf(error, error_capacity, "larger than a save");
        return 0;
    }
    /* Its size unknown (a stream), no more than a save and a byte. */
    for (size_t got; *size < ORACLES_SAVE_TRANSFER_READ && (got = SDL_ReadIO(in, out + *size, ORACLES_SAVE_TRANSFER_READ - *size)) != 0;) *size += got;
    const int ok = SDL_GetIOStatus(in) != SDL_IO_STATUS_ERROR;
    if (!ok) snprintf(error, error_capacity, "%s", SDL_GetError());
    SDL_CloseIO(in);
    return ok;
}

/* The save's folder and its name, whichever separator the path uses. */
static void split(const char *path, char *folder, size_t folder_capacity, const char **name)
{
    const char *slash = strrchr(path, '/'), *backslash = strrchr(path, '\\');
    if (backslash && (!slash || backslash > slash)) slash = backslash;
    *name = slash ? slash + 1 : path;
    snprintf(folder, folder_capacity, "%.*s", slash ? (int)(slash - path) : 1, slash ? path : ".");
}

/* The backup's name: the save's, the local time before its extension. */
static void backup_name(const char *name, time_t now, char *out, size_t capacity)
{
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name) dot = name + strlen(name);
    const struct tm *local = localtime(&now);
    char stamp[32] = "";
    if (local) strftime(stamp, sizeof stamp, " %Y-%m-%d %H-%M", local);
    snprintf(out, capacity, "%.*s%s%s", (int)(dot - name), name, stamp, *dot ? dot : ".sav");
}

void oracles_save_transfer_beside(const char *save, const char *extension, char *out, size_t capacity)
{
    const char *dot = strrchr(save, '.');
    const size_t length = dot && !strcmp(dot, ".sav") ? (size_t)(dot - save) : strlen(save);
    snprintf(out, capacity, "%.*s%s", (int)length, save, extension);
}

void oracles_save_transfer_exported(const char *save, int mods, char *out, size_t capacity)
{
    if (mods) oracles_save_transfer_beside(save, ".mods.sav", out, capacity);
    else snprintf(out, capacity, "%s", save);
}

int oracles_save_transfer_has_mods_save(const char *save)
{
    char path[4200];
    oracles_save_transfer_beside(save, ".mods.sav", path, sizeof path);
    return SDL_GetPathInfo(path, NULL);
}

int oracles_save_transfer_has_mods(const char *save)
{
    static const char *const extensions[3] = { ".mods.sav", ".mods.state", ".mods.store" };
    char path[4200];
    for (int i = 0; i < 3; i++) {
        oracles_save_transfer_beside(save, extensions[i], path, sizeof path);
        if (SDL_GetPathInfo(path, NULL)) return 1;
    }
    return 0;
}

/* The file at `path`, if there is one, copied into `backups` under its name and the time, *kept its path (empty when
 * there is none).  1, or 0 with SDL's reason. */
static int keep(const char *path, const char *backups, time_t now, char *kept, size_t capacity)
{
    kept[0] = 0;
    if (!SDL_GetPathInfo(path, NULL)) return 1;
    size_t size = 0;
    void *bytes = SDL_LoadFile(path, &size);
    if (!bytes) return 0;
    char folder[4096], name[512];
    const char *file;
    int fresh;
    split(path, folder, sizeof folder, &file);
    backup_name(file, now, name, sizeof name);
    const int ok = oracles_dialog_store_copy(backups, name, bytes, size, kept, capacity, &fresh);
    SDL_free(bytes);
    if (!ok) kept[0] = 0;
    return ok;
}

int oracles_save_transfer_import(const char *save, const uint8_t *data, size_t size, time_t now, OraclesSaveKept *kept, char *error,
                                 size_t error_capacity)
{
    char folder[4096], backups[4200], partial[4200];
    const char *file;
    split(save, folder, sizeof folder, &file);
    snprintf(backups, sizeof backups, "%s/Backups", folder);
    memset(kept, 0, sizeof *kept);
    if (!keep(save, backups, now, kept->save, sizeof kept->save)) { snprintf(error, error_capacity, "the save cannot be kept in Backups: %s", SDL_GetError()); return 0; }
    /* The files that hold the SRAM of the save it replaces go to Backups too, each set aside while the new save is
     * written and put back if that fails: its savestate, which a Load state would bring back and the game write over the
     * new one; the game with mods' save, which the Mods page's Play would play on rather than the new one (it starts as a
     * copy of the save when there is none), and its savestate and mods' storage, which go with it. */
    static const char *const extensions[4] = { ".state", ".mods.sav", ".mods.state", ".mods.store" };
    static const char *const names[4] = { "savestate", "save with mods", "savestate with mods", "mods' storage" };
    char *const moved[4] = { kept->state, kept->mods_save, kept->mods_state, kept->mods_store };
    char paths[4][4200], aside[4][4300];
    int set = 0;
    for (; set < 4; set++) {
        oracles_save_transfer_beside(save, extensions[set], paths[set], sizeof paths[set]);
        snprintf(aside[set], sizeof aside[set], "%.*s.replaced", (int)sizeof paths[set] - 1, paths[set]);
        if (!keep(paths[set], backups, now, moved[set], ORACLES_SAVE_TRANSFER_PATH) || (moved[set][0] && !SDL_RenamePath(paths[set], aside[set]))) {
            snprintf(error, error_capacity, "the %s cannot be moved to Backups: %s", names[set], SDL_GetError());
            moved[set][0] = 0;
            break;
        }
    }
    int ok = set == 4;
    snprintf(partial, sizeof partial, "%s.partial", save);
    if (ok && (!SDL_SaveFile(partial, data, size) || !SDL_RenamePath(partial, save))) {
        snprintf(error, error_capacity, "the save cannot be written: %s", SDL_GetError());
        SDL_RemovePath(partial);
        ok = 0;
    }
    for (int i = 0; i < set; i++) {
        if (!moved[i][0]) continue;
        if (ok) SDL_RemovePath(aside[i]);
        else SDL_RenamePath(aside[i], paths[i]);
    }
    return ok;
}

int oracles_save_transfer_export(const char *save, const char *destination, char *error, size_t error_capacity)
{
    size_t size = 0;
    void *data = SDL_LoadFile(save, &size);
    if (!data) { snprintf(error, error_capacity, "the save cannot be read: %s", SDL_GetError()); return 0; }
    const int ok = SDL_SaveFile(destination, data, size);
    SDL_free(data);
    if (!ok) snprintf(error, error_capacity, "the copy cannot be written: %s", SDL_GetError());
    return ok;
}
