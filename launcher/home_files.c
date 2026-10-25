#include "home_files.h"

#include "bps.h"
#include "rom.h"

#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const uint8_t *oracles_home_file(OraclesHomeFiles *files, const char *path, OraclesZipKind kind, size_t *size, char *reason,
                                 size_t capacity)
{
    struct stat file;
    const int there = stat(path, &file) == 0;
    const long long file_size = there ? (long long)file.st_size : -1, modified = there ? (long long)file.st_mtime : -1;
    unsigned slot = 0;
    for (unsigned i = 0; i < ORACLES_HOME_FILES; i++) {
        if (files->file[i].path[0] && !strcmp(files->file[i].path, path) && files->file[i].kind == kind) {
            slot = i;   /* the same file, or the file changed since: read again in its place */
            if (there && files->file[i].size == file_size && files->file[i].modified == modified) {
                files->file[i].used = ++files->clock;
                snprintf(reason, capacity, "%s", files->file[i].reason);
                *size = files->file[i].data_size;
                return files->file[i].data;
            }
            break;
        }
        if (files->file[i].used < files->file[slot].used) slot = i;   /* a free slot is one never used */
    }
    free(files->file[slot].data);
    snprintf(files->file[slot].path, sizeof files->file[slot].path, "%s", path);
    files->file[slot].kind = kind;
    files->file[slot].size = file_size;
    files->file[slot].modified = modified;
    files->file[slot].used = ++files->clock;
    files->file[slot].data_size = 0;
    files->file[slot].data = kind == ORACLES_ZIP_ROM ? oracles_rom_read_file(path, &files->file[slot].data_size, reason, capacity)
                                                    : oracles_bps_read_file(path, &files->file[slot].data_size, reason, capacity);
    snprintf(files->file[slot].reason, sizeof files->file[slot].reason, "%s", files->file[slot].data ? "" : reason);
    *size = files->file[slot].data_size;
    return files->file[slot].data;
}

void oracles_home_files_forget(OraclesHomeFiles *files)
{
    for (unsigned i = 0; i < ORACLES_HOME_FILES; i++) {
        free(files->file[i].data);
        files->file[i].data = NULL;
        files->file[i].path[0] = 0;
        files->file[i].used = 0;
    }
}
