/* The folder a file is written in, made when it is not there (file_folder.h). */
#include "file_folder.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int separator(char c)
{
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

int oracles_file_folder_make(const char *path, char *error, size_t capacity)
{
    size_t end = strlen(path);
    while (end && !separator(path[end - 1])) end--;
    if (!end) return 1;   /* a bare name: the current folder's */
    size_t length = end - 1;
    /* The root keeps its separator: `/`, and on Windows `C:\` (`C:` alone is the drive's current folder). */
    if (!length || (length == 2 && path[1] == ':')) length++;
    char *folder = malloc(length + 1);
    if (!folder) { snprintf(error, capacity, "cannot make the folder of %s", path); return 0; }
    memcpy(folder, path, length);
    folder[length] = 0;
    SDL_PathInfo info;
    int there = SDL_GetPathInfo(folder, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
    if (!there) {
        there = SDL_CreateDirectory(folder);
        if (there) fprintf(stderr, "oracles: made the folder %s\n", folder);
        else {
            fprintf(stderr, "oracles: cannot make the folder %s: %s\n", folder, SDL_GetError());
            snprintf(error, capacity, "cannot make the folder %s", folder);
        }
    }
    free(folder);
    return there;
}
