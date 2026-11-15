/* The folder of a file --record writes (file_folder.h), in the folder the test's argument names, emptied first: a route
 * in folders that are not there, which are made, the route and its starting SRAM then written in them; a name with
 * accents; a folder already there; a bare name; on Windows, `\` between the names; and a folder that cannot be made, a
 * file in its way, whose path the error names. */
#include "file_folder.h"
#include "route.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

static char root[2048];

static SDL_EnumerationResult SDLCALL removed(void *opaque, const char *dir, const char *name)
{
    (void)opaque;
    char file[2600];
    snprintf(file, sizeof file, "%s%s", dir, name);
    SDL_PathInfo info;
    if (SDL_GetPathInfo(file, &info) && info.type == SDL_PATHTYPE_DIRECTORY) SDL_EnumerateDirectory(file, removed, NULL);
    SDL_RemovePath(file);
    return SDL_ENUM_CONTINUE;
}

static int folder(const char *path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

static int holds(const char *path, const char *text)
{
    size_t length = 0;
    void *there = SDL_LoadFile(path, &length);
    const int same = there && length == strlen(text) && !memcmp(there, text, length);
    SDL_free(there);
    return same;
}

static void nested(void)
{
    char route[2600], sram[2700], made[2600], error[2800] = "";
    snprintf(route, sizeof route, "%sroutes/ages/temple.route", root);
    snprintf(made, sizeof made, "%sroutes/ages", root);
    CHECK(!folder(made));
    CHECK(oracles_file_folder_make(route, error, sizeof error) == 1);
    CHECK(folder(made));
    snprintf(sram, sizeof sram, "%s.sram", route);
    CHECK(SDL_SaveFile(sram, "sram", 4));
    CHECK(holds(sram, "sram"));
    OraclesRouteHeader header = { "ages", "880374fb978b18af4aa529e2e32f7ffb4d7dd2f4", "none", "", ORACLES_ROUTE_CORE_JOYPAD_BOUNCING_OFF, "", "" };
    OraclesRouteWriter writer;
    CHECK(oracles_route_writer_open_format(&writer, route, &header, ORACLES_ROUTE_FORMAT_LATEST) == 0);
    oracles_route_writer_record(&writer, 0, 0x01);
    CHECK(oracles_route_writer_close(&writer) == 0);
    OraclesRoute read;
    char reason[512];
    CHECK(oracles_route_read(route, &read, reason, sizeof reason) == 0 && read.count == 1);
    oracles_route_free(&read);
    CHECK(oracles_file_folder_make(route, error, sizeof error) == 1);   /* there already */
}

static void accents(void)
{
    char made[2600], file[2700], error[2800] = "";
    snprintf(made, sizeof made, "%sJeux \xc3\xa9t\xc3\xa9/Pok\xc3\xa9mon", root);
    snprintf(file, sizeof file, "%s/temple.route.sram", made);
    CHECK(oracles_file_folder_make(file, error, sizeof error) == 1);
    CHECK(folder(made));
    CHECK(SDL_SaveFile(file, "sram", 4) && holds(file, "sram"));
}

static void bare(void)
{
    char error[64] = "";
    CHECK(oracles_file_folder_make("temple.route", error, sizeof error) == 1 && !error[0]);
    CHECK(oracles_file_folder_make("/temple.route", error, sizeof error) == 1 && !error[0]);
}

#ifdef _WIN32
static void backslashes(void)
{
    char made[2400], route[2600], error[2800] = "";
    snprintf(made, sizeof made, "%sback\\slash", root);
    snprintf(route, sizeof route, "%s\\temple.route", made);
    CHECK(oracles_file_folder_make(route, error, sizeof error) == 1);
    CHECK(folder(made));
    CHECK(oracles_file_folder_make("C:\\temple.route", error, sizeof error) == 1);
}
#endif

static void blocked(void)
{
    char file[2200], made[2400], route[2600], error[2800] = "", wanted[2800];
    snprintf(file, sizeof file, "%sa file", root);
    CHECK(SDL_SaveFile(file, "x", 1));
    snprintf(made, sizeof made, "%s/routes", file);
    snprintf(route, sizeof route, "%s/temple.route", made);
    CHECK(oracles_file_folder_make(route, error, sizeof error) == 0);
    snprintf(wanted, sizeof wanted, "cannot make the folder %s", made);
    CHECK(strcmp(error, wanted) == 0);
    CHECK(!folder(made));
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: %s FOLDER\n", argv[0]); return 2; }
    snprintf(root, sizeof root, "%s/", argv[1]);
    SDL_EnumerateDirectory(root, removed, NULL);
    CHECK(SDL_CreateDirectory(root));
    nested();
    accents();
    bare();
#ifdef _WIN32
    backslashes();
#endif
    blocked();
    SDL_EnumerateDirectory(root, removed, NULL);
    if (failures) fprintf(stderr, "%d failure(s)\n", failures);
    else printf("file folder: all checks passed\n");
    return failures != 0;
}
