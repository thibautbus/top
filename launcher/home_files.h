/* The files the home screen reads to say what they are (home_games.h): each Oracle's ROM and European ROM, each fan
 * game's patch, a file chosen or dropped.  Each is read once as the ROM loader or the patch reader reads it, a zip's
 * inside decompressed once, and kept with the file as it was (its path, size and modification time) until forgotten:
 * a refresh, and the choice or the drop before it, read no file twice, and keep none after. */
#ifndef ORACLES_LAUNCHER_HOME_FILES_H
#define ORACLES_LAUNCHER_HOME_FILES_H

#include "settings.h"
#include "ui_home_nav.h"
#include "zip.h"

#include <stddef.h>
#include <stdint.h>

#define ORACLES_HOME_FILES 8   /* two ROMs, two European ROMs, three patches and a file dropped */

typedef struct OraclesHomeFiles {
    struct {
        char path[ORACLES_SETTINGS_PATH_LENGTH];   /* empty: a free slot */
        OraclesZipKind kind;
        long long size, modified;
        uint8_t *data;                             /* NULL: refused, for `reason` */
        size_t data_size;
        char reason[ORACLES_HOME_TEXT_LENGTH];
        unsigned long used;                        /* when it was last asked for */
    } file[ORACLES_HOME_FILES];
    unsigned long clock;
} OraclesHomeFiles;

/* The bytes of the file at `path` read as a ROM or as a patch (`kind`), kept in `files`, which frees them; NULL with
 * the reader's reason.  With every slot taken, the file asked for longest ago gives its place: the bytes of the last
 * ORACLES_HOME_FILES - 1 files asked for stay where they are. */
const uint8_t *oracles_home_file(OraclesHomeFiles *files, const char *path, OraclesZipKind kind, size_t *size, char *reason,
                                 size_t capacity);

/* Every file kept, freed. */
void oracles_home_files_forget(OraclesHomeFiles *files);

#endif
