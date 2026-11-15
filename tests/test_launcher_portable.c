/* The folder of the player's data and the portable mode (portable.h), in folders made in the one the test's argument
 * names, emptied first: the mode unavailable without an executable's folder, in a macOS application and in an AppImage
 * (not in a program an AppImage starts); the marker, portable.txt or portable.txt.txt, a file; ORACLES_SETTINGS_DIR
 * before the marker, the marker before the system's folder, user-data made, and the system's folder when user-data
 * cannot be written or the executable is in a macOS application; turned on, the launcher's data copied into user-data
 * and nothing else, the system's left as it was, nothing under a temporary name; on again, nothing done; off, with
 * data on both sides, nothing copied, and with none in the system's folder, copied back and both markers removed; a
 * Thumbs.db or an empty mods folder no data; a copy that fails midway, and a folder of the executable that cannot be
 * written, leaving the mode and both folders as they were; a copy cut short while it was copied made again, one cut
 * while it was moved out made again at a switch and finished at a launch, but not when its settings.txt was not
 * whole; a pack and a mod already there kept, a mods that is a file refused; links copied as links, a relative one
 * made absolute, a system's folder or a mods folder that is one kept one, and never gone through to remove what they
 * lead to, nor a system's mods that is a link merged into user-data's, whether the marker is written after or not;
 * on Windows, a mods folder that is a junction refused in the folder copied, kept
 * in the folder copied to, and never gone through. */
#include "portable.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

static char root[2048], exe[2200], system_dir[2200], data[2300], next[4096], error[512];

static void path(char *out, size_t capacity, const char *dir, const char *name) { snprintf(out, capacity, "%s%s", dir, name); }

static int put(const char *dir, const char *name, const char *text)
{
    char file[2600];
    path(file, sizeof file, dir, name);
    return SDL_SaveFile(file, text, strlen(text));
}

static int holds(const char *dir, const char *name, const char *text)
{
    char file[2600];
    path(file, sizeof file, dir, name);
    size_t length = 0;
    void *there = SDL_LoadFile(file, &length);
    const int same = there && length == strlen(text) && !memcmp(there, text, length);
    SDL_free(there);
    return same;
}

static int exists(const char *dir, const char *name)
{
    char file[2600];
    path(file, sizeof file, dir, name);
    return SDL_GetPathInfo(file, NULL);
}

static SDL_EnumerationResult SDLCALL removed(void *opaque, const char *dir, const char *name)
{
    (void)opaque;
    char file[2600];
    snprintf(file, sizeof file, "%s%s", dir, name);
    SDL_PathInfo info;
#ifdef _WIN32
    wchar_t wide[2600];
    if (MultiByteToWideChar(CP_UTF8, 0, file, -1, wide, 2600)) {
        const DWORD attributes = GetFileAttributesW(wide);
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) { RemoveDirectoryW(wide); return SDL_ENUM_CONTINUE; }
    }
#else
    struct stat link;
    if (lstat(file, &link) == 0 && S_ISLNK(link.st_mode)) { unlink(file); return SDL_ENUM_CONTINUE; }
#endif
    if (SDL_GetPathInfo(file, &info) && info.type == SDL_PATHTYPE_DIRECTORY) SDL_EnumerateDirectory(file, removed, NULL);
    SDL_RemovePath(file);
    return SDL_ENUM_CONTINUE;
}

static void empty(const char *dir)
{
    SDL_EnumerateDirectory(dir, removed, NULL);
    SDL_CreateDirectory(dir);
}

static void reset(void)
{
    empty(root);
    CHECK(SDL_CreateDirectory(exe) && SDL_CreateDirectory(system_dir));
}

/* The system's folder as a player who played before has it: settings.txt and a mod. */
static void played(void)
{
    char mod[2400];
    snprintf(mod, sizeof mod, "%smods/a mod", system_dir);
    CHECK(put(system_dir, "settings.txt", "profile=enhanced\n") && SDL_CreateDirectory(mod) && put(system_dir, "mods/a mod/mod.lua", "return {}\n"));
}

static void availability(void)
{
    CHECK(!oracles_portable_available(NULL) && !oracles_portable_available(""));
    CHECK(!oracles_portable_available("/Applications/The Oracles Project.app/Contents/Resources/"));
    CHECK(oracles_portable_available(exe));
    SDL_setenv_unsafe("APPIMAGE", "the-oracles-project.AppImage", 1);
    SDL_setenv_unsafe("APPDIR", root, 1);
    CHECK(!oracles_portable_available(exe));
    SDL_setenv_unsafe("APPDIR", "/tmp/.mount_frontend", 1);
    CHECK(oracles_portable_available(exe));
    SDL_unsetenv_unsafe("APPDIR");
    CHECK(oracles_portable_available(exe));
    SDL_unsetenv_unsafe("APPIMAGE");
}

static void marker(void)
{
    reset();
    CHECK(!oracles_portable_marked(exe));
    CHECK(put(exe, "portable.txt", "") && oracles_portable_marked(exe));
    reset();
    CHECK(put(exe, "portable.txt.txt", "") && oracles_portable_marked(exe));
    reset();
    char folder[2400];
    path(folder, sizeof folder, exe, "portable.txt");
    CHECK(SDL_CreateDirectory(folder) && !oracles_portable_marked(exe));
}

static void resolve(void)
{
    char out[4096], expected[4096], bundle[2400];
    reset();
    CHECK(oracles_user_dir(NULL, exe, out, sizeof out) == ORACLES_USER_DIR_SYSTEM && !out[0]);
    CHECK(oracles_user_dir("", exe, out, sizeof out) == ORACLES_USER_DIR_SYSTEM && !out[0]);
    CHECK(!exists(exe, "user-data"));
    CHECK(put(exe, "portable.txt", ""));
    CHECK(oracles_user_dir("/forced", exe, out, sizeof out) == ORACLES_USER_DIR_FORCED && !strcmp(out, "/forced/"));
    CHECK(oracles_user_dir("C:\\forced", exe, out, sizeof out) == ORACLES_USER_DIR_FORCED && !strcmp(out, "C:\\forced\\"));
    CHECK(oracles_user_dir("C:\\forced\\", exe, out, sizeof out) == ORACLES_USER_DIR_FORCED && !strcmp(out, "C:\\forced\\"));
    CHECK(oracles_user_dir_chosen() == ORACLES_USER_DIR_FORCED);
    CHECK(!exists(exe, "user-data"));
    snprintf(expected, sizeof expected, "%suser-data/", exe);
    CHECK(oracles_user_dir(NULL, exe, out, sizeof out) == ORACLES_USER_DIR_PORTABLE && !strcmp(out, expected));
    CHECK(oracles_user_dir_chosen() == ORACLES_USER_DIR_PORTABLE);
    CHECK(exists(exe, "user-data") && !exists(exe, "user-data/.write-test"));
    CHECK(oracles_user_dir(NULL, exe, out, 8) == ORACLES_USER_DIR_NONE && !out[0]);
    /* A macOS application with the marker in it. */
    snprintf(bundle, sizeof bundle, "%sThe Oracles Project.app/Contents/Resources/", root);
    CHECK(SDL_CreateDirectory(bundle) && put(bundle, "portable.txt", "") && !oracles_portable_marked(bundle));
    CHECK(oracles_user_dir(NULL, bundle, out, sizeof out) == ORACLES_USER_DIR_SYSTEM && !out[0] && !exists(bundle, "user-data"));
    CHECK(oracles_user_dir_chosen() == ORACLES_USER_DIR_SYSTEM);
}

static void turn(void)
{
    char expected[4096];
    /* On: the launcher's data copied into user-data, the system's left as it was. */
    reset();
    played();
    CHECK(put(system_dir, "language-ages.pack", "pack") && put(system_dir, "Thumbs.db", "thumbs") && put(system_dir, "settings.txt.tmp", "half"));
    CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_COPIED && !error[0]);
    CHECK(!strcmp(next, data));
    CHECK(oracles_portable_marked(exe) && exists(exe, "portable.txt"));
    CHECK(holds(data, "settings.txt", "profile=enhanced\n") && holds(data, "mods/a mod/mod.lua", "return {}\n") && holds(data, "language-ages.pack", "pack"));
    CHECK(!exists(data, "Thumbs.db") && !exists(data, "settings.txt.tmp") && !exists(data, ".copying"));
    CHECK(holds(system_dir, "settings.txt", "profile=enhanced\n") && holds(system_dir, "mods/a mod/mod.lua", "return {}\n"));
    CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_UNCHANGED);
    /* Off, data on both sides: nothing copied over either. */
    CHECK(put(data, "settings.txt", "profile=faithful\n"));
    CHECK(oracles_portable_set(0, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_KEPT && !strcmp(next, system_dir));
    CHECK(!oracles_portable_marked(exe) && holds(system_dir, "settings.txt", "profile=enhanced\n") && holds(data, "settings.txt", "profile=faithful\n"));
    CHECK(oracles_portable_set(0, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_UNCHANGED);
    /* On, data on both sides: user-data's kept. */
    CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_KEPT);
    CHECK(oracles_portable_marked(exe) && holds(data, "settings.txt", "profile=faithful\n") && holds(system_dir, "settings.txt", "profile=enhanced\n"));
    /* Off, the system's folder without data (a Thumbs.db, an empty mods folder), both markers there: copied back, both
     * removed. */
    empty(system_dir);
    snprintf(expected, sizeof expected, "%smods", system_dir);
    CHECK(put(system_dir, "Thumbs.db", "thumbs") && SDL_CreateDirectory(expected) && put(exe, "portable.txt.txt", ""));
    CHECK(oracles_portable_set(0, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_COPIED);
    CHECK(!exists(exe, "portable.txt") && !exists(exe, "portable.txt.txt") && !oracles_portable_marked(exe));
    CHECK(holds(system_dir, "settings.txt", "profile=faithful\n") && holds(system_dir, "mods/a mod/mod.lua", "return {}\n") && holds(system_dir, "Thumbs.db", "thumbs"));
    CHECK(holds(data, "settings.txt", "profile=faithful\n"));
    /* On with data only in user-data: user-data's kept. */
    empty(system_dir);
    CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_KEPT && !exists(system_dir, "settings.txt"));
    /* On with no data anywhere: the marker alone. */
    reset();
    CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_NOTHING_TO_COPY && oracles_portable_marked(exe));
    /* A copy cut short while it was copied (no settings.txt in .copying): removed, the copy made again. */
    reset();
    played();
    char partial[2400];
    path(partial, sizeof partial, data, ".copying");
    CHECK(SDL_CreateDirectory(partial) && put(data, ".copying/stale.txt", "stale"));
    CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_COPIED);
    CHECK(!exists(data, ".copying") && !exists(data, "stale.txt") && holds(data, "settings.txt", "profile=enhanced\n"));
    /* A copy cut short while it was moved out (the mods moved, settings.txt still in .copying), then a switch: the
     * data may have changed since, so .copying is removed and the data copied again. */
    reset();
    played();
    snprintf(expected, sizeof expected, "%smods/a mod", data);
    CHECK(SDL_CreateDirectory(expected) && put(data, "mods/a mod/mod.lua", "return {}\n") && SDL_CreateDirectory(partial));
    CHECK(put(data, ".copying/settings.txt", "profile=stale\n") && put(data, ".copying/language-ages.pack", "stale pack"));
    CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_COPIED && oracles_portable_marked(exe));
    CHECK(!exists(data, ".copying") && holds(data, "settings.txt", "profile=enhanced\n") && !exists(data, "language-ages.pack"));
    CHECK(holds(data, "mods/a mod/mod.lua", "return {}\n"));
    /* At a launch, in the folder it reads: the move finished. */
    reset();
    CHECK(SDL_CreateDirectory(partial) && put(data, ".copying/settings.txt", "profile=faithful\n") && put(data, ".copying/language-ages.pack", "pack"));
    oracles_portable_tidy(data);
    CHECK(!exists(data, ".copying") && holds(data, "settings.txt", "profile=faithful\n") && holds(data, "language-ages.pack", "pack"));
    /* One cut while settings.txt was copied in (still under its other name): removed, not taken for a whole copy. */
    reset();
    CHECK(SDL_CreateDirectory(partial) && put(data, ".copying/settings.txt.part", "profile=fai") && put(data, ".copying/language-ages.pack", "pack"));
    oracles_portable_tidy(data);
    CHECK(!exists(data, ".copying") && !exists(data, "settings.txt") && !exists(data, "settings.txt.part") && !exists(data, "language-ages.pack"));
    /* A user-data whose mods is a file: refused, nothing changed. */
    reset();
    played();
    CHECK(SDL_CreateDirectory(data) && put(data, "mods", "a file"));
    CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_COPY_FAILED && error[0]);
    CHECK(!oracles_portable_marked(exe) && holds(data, "mods", "a file") && !exists(data, "settings.txt") && !exists(data, ".copying"));
    /* Into a user-data with settings.txt missing but a pack and a mod of its own: those kept, the rest brought. */
    reset();
    played();
    snprintf(expected, sizeof expected, "%smods/b mod", system_dir);
    CHECK(SDL_CreateDirectory(expected) && put(system_dir, "language-ages.pack", "system pack") && put(system_dir, "mods/b mod/mod.lua", "b\n"));
    snprintf(expected, sizeof expected, "%smods/a mod", data);
    CHECK(SDL_CreateDirectory(expected) && put(data, "mods/a mod/mod.lua", "mine\n") && put(data, "language-ages.pack", "my pack"));
    CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_COPIED);
    CHECK(holds(data, "mods/a mod/mod.lua", "mine\n") && holds(data, "language-ages.pack", "my pack"));
    CHECK(holds(data, "mods/b mod/mod.lua", "b\n") && holds(data, "settings.txt", "profile=enhanced\n"));
    /* A mods folder that holds no mod (a desktop.ini) is no data. */
    reset();
    snprintf(expected, sizeof expected, "%smods", system_dir);
    CHECK(SDL_CreateDirectory(expected) && put(system_dir, "mods/desktop.ini", "ini"));
    CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_NOTHING_TO_COPY && !exists(data, "mods"));
    /* Not here. */
    CHECK(oracles_portable_set(1, "/Applications/The Oracles Project.app/Contents/Resources/", system_dir, next, sizeof next, error, sizeof error)
          == ORACLES_PORTABLE_UNAVAILABLE);
}

#ifndef _WIN32
/* Links: one to the player's checkout of a mod copied as a link; a system's folder that leads to a synchronised folder
 * stays a link, the data copied where it leads; a copy's folder left as a link, removed without what it leads to. */
static void links(void)
{
    char outside[2400], link[2600], target[2600], real[2400];
    reset();
    played();
    snprintf(outside, sizeof outside, "%scheckout", root);
    CHECK(SDL_CreateDirectory(outside) && put(outside, "/main.lua", "return {}\n") && put(outside, "/.git", "git"));
    path(link, sizeof link, system_dir, "mods/linked");
    CHECK(symlink(outside, link) == 0);
    CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_COPIED);
    path(link, sizeof link, data, "mods/linked");
    const ssize_t length = readlink(link, target, sizeof target - 1);
    CHECK(length > 0 && (target[length] = 0, !strcmp(target, outside)));
    CHECK(holds(outside, "/main.lua", "return {}\n"));
    /* Off into a system's folder that is a link to an empty folder. */
    snprintf(real, sizeof real, "%ssynchronised", root);
    CHECK(SDL_CreateDirectory(real));
    empty(system_dir);
    char bare_system[2200];
    snprintf(bare_system, sizeof bare_system, "%.*s", (int)strlen(system_dir) - 1, system_dir);
    CHECK(SDL_RemovePath(bare_system) && symlink(real, bare_system) == 0);
    CHECK(oracles_portable_set(0, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_COPIED);
    struct stat info;
    CHECK(lstat(bare_system, &info) == 0 && S_ISLNK(info.st_mode));
    CHECK(holds(real, "/settings.txt", "profile=enhanced\n") && !exists(real, "/.copying"));
    /* A user-data whose mods folder is a link to an empty folder: the mods put in it, the link kept. */
    CHECK(unlink(bare_system) == 0 && SDL_CreateDirectory(system_dir));
    played();
    char empty_mods[2400], mods_link[2600];
    snprintf(empty_mods, sizeof empty_mods, "%sshared-mods", root);
    CHECK(SDL_CreateDirectory(empty_mods));
    empty(data);
    path(mods_link, sizeof mods_link, data, "mods");
    CHECK(symlink(empty_mods, mods_link) == 0);
    CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_COPIED);
    CHECK(lstat(mods_link, &info) == 0 && S_ISLNK(info.st_mode) && holds(empty_mods, "/a mod/mod.lua", "return {}\n"));
    CHECK(oracles_portable_set(0, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_KEPT);
    /* A relative link: made absolute, leading where it led. */
    empty(system_dir);
    played();
    path(link, sizeof link, system_dir, "mods/relative");
    CHECK(symlink("../../checkout", link) == 0);
    empty(data);
    CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_COPIED);
    path(link, sizeof link, data, "mods/relative");
    const ssize_t relative = readlink(link, target, sizeof target - 1);
    CHECK(relative > 0 && (target[relative] = 0, target[0] == '/') && holds(link, "/main.lua", "return {}\n"));
    CHECK(oracles_portable_set(0, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_KEPT);
    /* The system's mods a link to the player's folder, user-data with a mods folder of its own: kept, the link never
     * gone through, the player's folder whole; with the marker that cannot be written after the copy, the same. */
    char shared[2400], shared_link[2600], blocked[2600], expected_mods[2600];
    snprintf(shared, sizeof shared, "%sshared", root);
    CHECK(SDL_CreateDirectory(shared));
    snprintf(blocked, sizeof blocked, "%s/mine", shared);
    CHECK(SDL_CreateDirectory(blocked) && put(shared, "/mine/main.lua", "mine\n"));
    path(blocked, sizeof blocked, exe, "portable.txt");
    for (int marker_fails = 0; marker_fails < 2; marker_fails++) {
        empty(system_dir);
        CHECK(put(system_dir, "settings.txt", "profile=enhanced\n"));
        path(shared_link, sizeof shared_link, system_dir, "mods");
        CHECK(symlink(shared, shared_link) == 0);
        empty(data);
        snprintf(expected_mods, sizeof expected_mods, "%smods/own", data);
        CHECK(SDL_CreateDirectory(expected_mods) && put(data, "mods/own/main.lua", "own\n"));
        if (marker_fails) CHECK(SDL_CreateDirectory(blocked));
        CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error)
              == (marker_fails ? ORACLES_PORTABLE_READ_ONLY : ORACLES_PORTABLE_COPIED));
        CHECK(holds(shared, "/mine/main.lua", "mine\n") && holds(data, "mods/own/main.lua", "own\n") && !exists(data, "mods/mine"));
        CHECK(lstat(shared_link, &info) == 0 && S_ISLNK(info.st_mode));
        CHECK(marker_fails ? !exists(data, "settings.txt") : holds(data, "settings.txt", "profile=enhanced\n"));
        if (marker_fails) CHECK(SDL_RemovePath(blocked));
        else CHECK(oracles_portable_set(0, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_KEPT);
    }
    unlink(shared_link);
    /* A copy's folder left as a link to the checkout: removed, the checkout whole. */
    empty(system_dir);
    played();
    empty(data);
    path(link, sizeof link, data, ".copying");
    CHECK(symlink(outside, link) == 0);
    CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_COPIED);
    CHECK(holds(outside, "/main.lua", "return {}\n") && holds(outside, "/.git", "git") && !exists(data, ".copying"));
}
#else
/* `dir` + `name` made a junction to the folder `target`, which mklink makes without an administrator's rights. */
static int junction(const char *dir, const char *name, const char *target)
{
    char link[2600], to[2600], command[5400];
    snprintf(link, sizeof link, "%s%s", dir, name);
    snprintf(to, sizeof to, "%s", target);
    for (char *c = link; *c; c++) if (*c == '/') *c = '\\';
    for (char *c = to; *c; c++) if (*c == '/') *c = '\\';
    snprintf(command, sizeof command, "mklink /J \"%s\" \"%s\" >nul", link, to);
    return system(command) == 0;
}

static int is_junction(const char *dir, const char *name)
{
    char file[2600];
    wchar_t wide[2600];
    path(file, sizeof file, dir, name);
    if (!MultiByteToWideChar(CP_UTF8, 0, file, -1, wide, 2600)) return 0;
    const DWORD attributes = GetFileAttributesW(wide);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
}

/* Junctions: a system's mods folder that is one refused, kept, what it leads to whole; a user-data whose mods folder
 * is one to an empty folder, the mods put in it, the junction kept; a copy's folder left as one removed without what
 * it leads to. */
static void junctions(void)
{
    char outside[2400], mods[2400], shared[2400];
    reset();
    snprintf(outside, sizeof outside, "%scheckout", root);
    snprintf(mods, sizeof mods, "%scheckout/a mod", root);
    CHECK(SDL_CreateDirectory(outside) && SDL_CreateDirectory(mods) && put(outside, "/a mod/mod.lua", "return {}\n") && put(outside, "/.git", "git"));
    CHECK(put(system_dir, "settings.txt", "profile=enhanced\n") && junction(system_dir, "mods", outside) && is_junction(system_dir, "mods"));
    CHECK(holds(system_dir, "mods/a mod/mod.lua", "return {}\n"));
    CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_COPY_FAILED && error[0]);
    CHECK(!oracles_portable_marked(exe) && !exists(exe, "user-data"));
    CHECK(is_junction(system_dir, "mods") && holds(outside, "/a mod/mod.lua", "return {}\n") && holds(outside, "/.git", "git"));
    /* Into a user-data whose mods folder is a junction to an empty folder. */
    empty(system_dir);
    CHECK(!exists(system_dir, "mods") && holds(outside, "/a mod/mod.lua", "return {}\n"));
    played();
    snprintf(shared, sizeof shared, "%sshared-mods", root);
    CHECK(SDL_CreateDirectory(shared) && SDL_CreateDirectory(data) && junction(data, "mods", shared));
    CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_COPIED);
    CHECK(is_junction(data, "mods") && holds(shared, "/a mod/mod.lua", "return {}\n") && holds(data, "settings.txt", "profile=enhanced\n"));
    CHECK(oracles_portable_set(0, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_KEPT);
    CHECK(is_junction(data, "mods") && holds(shared, "/a mod/mod.lua", "return {}\n"));
    /* A copy's folder left as a junction to the checkout. */
    empty(system_dir);
    played();
    empty(data);
    CHECK(holds(shared, "/a mod/mod.lua", "return {}\n"));
    CHECK(junction(data, ".copying", outside));
    CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_COPIED);
    CHECK(!exists(data, ".copying") && holds(outside, "/a mod/mod.lua", "return {}\n") && holds(outside, "/.git", "git"));
}
#endif

/* A file that the copy cannot read, while `locked` holds; 0 when the system lets it be read all the same (root). */
#ifdef _WIN32
static HANDLE lock_handle = INVALID_HANDLE_VALUE;
static int lock(const char *file, int locked)
{
    if (!locked) { CloseHandle(lock_handle); lock_handle = INVALID_HANDLE_VALUE; return 1; }
    wchar_t name[2600];
    if (!MultiByteToWideChar(CP_UTF8, 0, file, -1, name, 2600)) return 0;
    lock_handle = CreateFileW(name, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    return lock_handle != INVALID_HANDLE_VALUE;
}
#else
static int lock(const char *file, int locked)
{
    if (!locked) return chmod(file, 0644) == 0;
    if (chmod(file, 0) != 0) return 0;
    SDL_IOStream *in = SDL_IOFromFile(file, "rb");
    if (in) SDL_CloseIO(in);
    return in == NULL;
}
#endif

static void failures_leave_all(void)
{
    char file[2600];
    /* A copy that fails midway: no marker, no user-data, nothing under a temporary name, the system's folder whole. */
    reset();
    played();
    CHECK(put(system_dir, "language-zz.pack", "pack"));
    path(file, sizeof file, system_dir, "language-zz.pack");
    if (lock(file, 1)) {
        CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_COPY_FAILED && error[0]);
        CHECK(!oracles_portable_marked(exe) && !exists(exe, "user-data"));
        lock(file, 0);
        CHECK(holds(system_dir, "settings.txt", "profile=enhanced\n") && holds(system_dir, "language-zz.pack", "pack"));
    } else fprintf(stderr, "portable: a file cannot be made unreadable here: the failed copy is not checked\n");
#ifndef _WIN32
    /* The executable's folder read only: on, nothing written; off, the copy back removed, the marker kept. */
    reset();
    played();
    char probe[2600];
    CHECK(chmod(exe, 0555) == 0);
    path(probe, sizeof probe, exe, "probe");
    if (!SDL_SaveFile(probe, "", 1)) {
        char out[4096];
        CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_READ_ONLY && error[0]);
        CHECK(!oracles_portable_marked(exe) && !exists(exe, "user-data"));
        CHECK(chmod(exe, 0755) == 0 && put(exe, "portable.txt", ""));
        CHECK(oracles_user_dir(NULL, exe, out, sizeof out) == ORACLES_USER_DIR_PORTABLE && put(data, "settings.txt", "profile=faithful\n"));
        empty(system_dir);
        CHECK(chmod(exe, 0555) == 0);
        CHECK(oracles_portable_set(0, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_READ_ONLY && error[0]);
        CHECK(oracles_portable_marked(exe) && !exists(system_dir, "settings.txt") && !exists(system_dir, ".copying") && exists(system_dir, ""));
        CHECK(holds(data, "settings.txt", "profile=faithful\n"));
        /* user-data that cannot be written: at a launch, the system's folder; turned on, refused. */
        CHECK(chmod(data, 0555) == 0);
        CHECK(oracles_user_dir(NULL, exe, out, sizeof out) == ORACLES_USER_DIR_SYSTEM && !out[0]);
        CHECK(oracles_portable_set(1, exe, system_dir, next, sizeof next, error, sizeof error) == ORACLES_PORTABLE_READ_ONLY && error[0]);
        CHECK(chmod(data, 0755) == 0);
    } else {
        SDL_RemovePath(probe);
        fprintf(stderr, "portable: a folder cannot be made read only here: the read-only folder is not checked\n");
    }
    chmod(exe, 0755);
#endif
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: %s FOLDER\n", argv[0]); return 2; }
    snprintf(root, sizeof root, "%s/", argv[1]);
    snprintf(exe, sizeof exe, "%sexe/", root);
    snprintf(system_dir, sizeof system_dir, "%ssystem/", root);
    snprintf(data, sizeof data, "%suser-data/", exe);
    SDL_unsetenv_unsafe("APPIMAGE");
    empty(root);
    availability();
    marker();
    resolve();
    turn();
#ifdef _WIN32
    junctions();
#else
    links();
#endif
    failures_leave_all();
    empty(root);
    if (failures) fprintf(stderr, "%d failure(s)\n", failures);
    else printf("portable: all checks passed\n");
    return failures != 0;
}
