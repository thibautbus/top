/* The folder a file is written in, made when it is not there: --record's route and the files beside it
 * (ROUTE.sram, ROUTE.session.tsv...), whose path may name folders that do not exist yet. */
#ifndef ORACLES_LAUNCHER_FILE_FOLDER_H
#define ORACLES_LAUNCHER_FILE_FOLDER_H

#include <stddef.h>

/* The folder the file `path` (UTF-8, `/` or, on Windows, `\` between its names) sits in, made with those above it
 * (SDL_CreateDirectory) when it is not there.  A path without a folder is in the current one.  Returns 1 when the
 * folder is there; else 0, `error` naming the folder that could not be made, the standard error why. */
int oracles_file_folder_make(const char *path, char *error, size_t capacity);

#endif
