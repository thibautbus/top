/* The folder of the player's data and the portable mode (portable.h). */
#include "portable.h"

#include <SDL3/SDL.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#define PORTABLE_PATH 4096
#define COPYING ".copying"   /* a copy's folder, inside the folder it goes to */

/* The names the marker may have: Windows hides the extension of a text document named portable.txt, portable.txt.txt. */
static const char *const markers[2] = { ORACLES_PORTABLE_MARKER, ORACLES_PORTABLE_MARKER ".txt" };

static OraclesUserDirKind chosen = ORACLES_USER_DIR_SYSTEM;

static int separator(char c) { return c == '/' || c == '\\'; }

/* `dir` and `name` into `out`, a separator between them when `dir` ends without one (the one `dir` uses, '/' if
 * none); 0 when it does not fit. */
static int join(const char *dir, const char *name, char *out, size_t capacity)
{
    const size_t length = strlen(dir);
    const char sep = strchr(dir, '\\') && !strchr(dir, '/') ? '\\' : '/';
    const int written = length && !separator(dir[length - 1]) ? snprintf(out, capacity, "%s%c%s", dir, sep, name) : snprintf(out, capacity, "%s%s", dir, name);
    return written >= 0 && (size_t)written < capacity;
}

/* `dir` without its trailing separator. */
static int bare(const char *dir, char *out, size_t capacity)
{
    size_t length = strlen(dir);
    while (length > 1 && separator(dir[length - 1])) length--;
    if (length >= capacity) return 0;
    memcpy(out, dir, length);
    out[length] = 0;
    return 1;
}

static int is_type(const char *path, SDL_PathType type)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == type;
}

/* A file, or what Windows keeps elsewhere until it is read (a file of OneDrive's that is only online). */
static int is_file(const char *path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && (info.type == SDL_PATHTYPE_FILE || info.type == SDL_PATHTYPE_OTHER);
}

#ifdef _WIN32
static int wide(const char *path, wchar_t *out) { return MultiByteToWideChar(CP_UTF8, 0, path, -1, out, PORTABLE_PATH) > 0; }
#endif

/* 1 when `path` is a link (on Windows, a symbolic link or a junction), which a copy and a removal never go through. */
static int is_link(const char *path)
{
#ifdef _WIN32
    wchar_t name[PORTABLE_PATH];
    WIN32_FIND_DATAW found;
    if (!wide(path, name)) return 0;
    HANDLE search = FindFirstFileW(name, &found);
    if (search == INVALID_HANDLE_VALUE) return 0;
    FindClose(search);
    return (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && (found.dwReserved0 == IO_REPARSE_TAG_SYMLINK || found.dwReserved0 == IO_REPARSE_TAG_MOUNT_POINT);
#else
    struct stat info;
    return lstat(path, &info) == 0 && S_ISLNK(info.st_mode);
#endif
}

/* Windows keeps a copied file's read-only attribute, and removes no file that has it. */
static void clear_read_only(const char *path)
{
#ifdef _WIN32
    wchar_t name[PORTABLE_PATH];
    if (!wide(path, name)) return;
    const DWORD attributes = GetFileAttributesW(name);
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY)) SetFileAttributesW(name, attributes & ~(DWORD)FILE_ATTRIBUTE_READONLY);
#else
    (void)path;
#endif
}

/* A rename on Windows fails for a moment while the antivirus reads a file just written. */
static int rename_patiently(const char *from, const char *to)
{
#ifdef _WIN32
    for (int attempt = 0; attempt < 10; attempt++) {
        if (SDL_RenamePath(from, to)) return 1;
        SDL_Delay(100);
    }
    return 0;
#else
    return SDL_RenamePath(from, to);
#endif
}

/* The launcher's own data in its folder: settings.txt, the mods folder and the language packs. */
static int is_data_name(const char *name)
{
    const size_t length = strlen(name);
    return !strcmp(name, "settings.txt") || !strcmp(name, "mods")
        || (!strncmp(name, "language-", 9) && length > 14 && !strcmp(name + length - 5, ".pack"));
}

static SDL_EnumerationResult SDLCALL any_folder(void *opaque, const char *dir, const char *name)
{
    char path[PORTABLE_PATH];
    if (!join(dir, name, path, sizeof path) || !(is_link(path) || is_type(path, SDL_PATHTYPE_DIRECTORY))) return SDL_ENUM_CONTINUE;
    *(int *)opaque = 1;
    return SDL_ENUM_SUCCESS;
}

static SDL_EnumerationResult SDLCALL data_entry(void *opaque, const char *dir, const char *name)
{
    char path[PORTABLE_PATH];
    if (!is_data_name(name) || !join(dir, name, path, sizeof path)) return SDL_ENUM_CONTINUE;
    int any = 0;
    if (strcmp(name, "mods")) any = is_file(path);
    else if (is_type(path, SDL_PATHTYPE_DIRECTORY)) SDL_EnumerateDirectory(path, any_folder, &any);
    if (!any) return SDL_ENUM_CONTINUE;
    *(int *)opaque = 1;
    return SDL_ENUM_SUCCESS;
}

/* 1 when the folder `dir` holds the launcher's data to copy: settings.txt, a mod or a language pack (not a
 * Thumbs.db, nor a mods folder that holds no mod). */
static int has_data(const char *dir)
{
    int any = 0;
    if (is_type(dir, SDL_PATHTYPE_DIRECTORY)) SDL_EnumerateDirectory(dir, data_entry, &any);
    return any;
}

/* 1 when the folder `dir` has settings.txt: the data the launcher starts from, which a copy never replaces. */
static int has_settings(const char *dir)
{
    char path[PORTABLE_PATH];
    return join(dir, "settings.txt", path, sizeof path) && is_file(path);
}

/* A list of names that grows. */
typedef struct name_list {
    char (*names)[512];
    unsigned count, capacity;
    int failed;
} name_list;

static int add_name(name_list *list, const char *name)
{
    if (strlen(name) >= sizeof list->names[0]) { list->failed = 1; return 0; }
    if (list->count == list->capacity) {
        const unsigned capacity = list->capacity ? list->capacity * 2u : 16u;
        void *grown = SDL_realloc(list->names, capacity * sizeof list->names[0]);
        if (!grown) { list->failed = 1; return 0; }
        list->names = grown;
        list->capacity = capacity;
    }
    snprintf(list->names[list->count++], sizeof list->names[0], "%s", name);
    return 1;
}

static void free_names(name_list *list)
{
    SDL_free(list->names);
    memset(list, 0, sizeof *list);
}

typedef struct name_scan {
    name_list *list;
    int data_only;
} name_scan;

static SDL_EnumerationResult SDLCALL scan_name(void *opaque, const char *dir, const char *name)
{
    (void)dir;
    name_scan *scan = opaque;
    if (scan->data_only && !is_data_name(name)) return SDL_ENUM_CONTINUE;
    return add_name(scan->list, name) ? SDL_ENUM_CONTINUE : SDL_ENUM_FAILURE;
}

/* The names in the folder `dir` (the launcher's data alone when `data_only`), settings.txt last; 0 on a failure. */
static int list_names(const char *dir, int data_only, name_list *list)
{
    memset(list, 0, sizeof *list);
    name_scan scan = { list, data_only };
    if (is_type(dir, SDL_PATHTYPE_DIRECTORY)) SDL_EnumerateDirectory(dir, scan_name, &scan);
    for (unsigned i = 0; i + 1 < list->count; i++)
        if (!strcmp(list->names[i], "settings.txt")) {
            memmove(list->names[i], list->names[i + 1], (list->count - i - 1) * sizeof list->names[0]);
            snprintf(list->names[list->count - 1], sizeof list->names[0], "settings.txt");
            break;
        }
    return !list->failed;
}

static int remove_tree(const char *path);

static SDL_EnumerationResult SDLCALL remove_entry(void *opaque, const char *dir, const char *name)
{
    char path[PORTABLE_PATH];
    if (!join(dir, name, path, sizeof path) || !remove_tree(path)) *(int *)opaque = 0;
    return SDL_ENUM_CONTINUE;
}

/* `path` removed, a folder with all it holds, a link and not what it leads to; 1 when it is gone. */
static int remove_tree(const char *path)
{
    if (is_link(path)) return SDL_RemovePath(path);
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info)) return 1;
    int ok = 1;
    if (info.type == SDL_PATHTYPE_DIRECTORY) SDL_EnumerateDirectory(path, remove_entry, &ok);
    else clear_read_only(path);
    return SDL_RemovePath(path) && ok;
}

/* What a copy put in the folder `dir`, the names in `moved`, removed: nothing that was there before it. */
static void undo_moves(const char *dir, const name_list *moved)
{
    char path[PORTABLE_PATH];
    for (unsigned i = moved->count; i-- > 0;) if (join(dir, moved->names[i], path, sizeof path)) remove_tree(path);
}

typedef struct copy_task {
    const char *to;
    char *error;
    size_t capacity;
    int ok;
} copy_task;

static int copy_tree(const char *from, const char *to, char *error, size_t capacity);

/* The link `from` made again as `to`, leading where it leads: a relative one made absolute, so that it leads there
 * from another folder too. */
static int copy_link(const char *from, const char *to, char *error, size_t capacity)
{
#ifdef _WIN32
    (void)to;
    snprintf(error, capacity, "%s is a link, which is not copied", from);
    return 0;
#else
    char target[PORTABLE_PATH], absolute[PORTABLE_PATH * 2];
    const ssize_t length = readlink(from, target, sizeof target - 1);
    if (length < 0) { snprintf(error, capacity, "the link %s cannot be read: %s", from, strerror(errno)); return 0; }
    target[length] = 0;
    const char *slash = strrchr(from, '/');
    if (target[0] == '/' || !slash) snprintf(absolute, sizeof absolute, "%s", target);
    else snprintf(absolute, sizeof absolute, "%.*s%s", (int)(slash - from + 1), from, target);
    if (symlink(absolute, to) != 0) { snprintf(error, capacity, "the link %s cannot be copied: %s", from, strerror(errno)); return 0; }
    return 1;
#endif
}

static int copy_entry(const char *from, const char *to, char *error, size_t capacity)
{
    if (is_link(from)) return copy_link(from, to, error, capacity);
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(from, &info)) { snprintf(error, capacity, "%s cannot be read: %s", from, SDL_GetError()); return 0; }
    if (info.type == SDL_PATHTYPE_DIRECTORY) return copy_tree(from, to, error, capacity);
    if (!SDL_CopyFile(from, to)) { snprintf(error, capacity, "%s cannot be copied: %s", from, SDL_GetError()); return 0; }
    clear_read_only(to);
    return 1;
}

static SDL_EnumerationResult SDLCALL copy_each(void *opaque, const char *dir, const char *name)
{
    copy_task *task = opaque;
    char from[PORTABLE_PATH], to[PORTABLE_PATH];
    if (!join(dir, name, from, sizeof from) || !join(task->to, name, to, sizeof to)) {
        snprintf(task->error, task->capacity, "the path of %s is too long", name);
        task->ok = 0;
    } else task->ok = copy_entry(from, to, task->error, task->capacity);
    return task->ok ? SDL_ENUM_CONTINUE : SDL_ENUM_FAILURE;
}

/* The folder `from` and all it holds copied as `to`, which is made; 1, or 0 with the reason in `error`. */
static int copy_tree(const char *from, const char *to, char *error, size_t capacity)
{
    if (!SDL_CreateDirectory(to)) { snprintf(error, capacity, "%s cannot be made: %s", to, SDL_GetError()); return 0; }
    copy_task task = { to, error, capacity, 1 };
    if (!SDL_EnumerateDirectory(from, copy_each, &task) && task.ok) {
        snprintf(error, capacity, "%s cannot be read: %s", from, SDL_GetError());
        task.ok = 0;
    }
    return task.ok;
}

/* `from` moved to `to`, which is not there: renamed, or where that fails (`to` in a folder that is a link to another
 * drive) copied then removed. */
static int move_entry(const char *from, const char *to, char *error, size_t capacity)
{
    if (rename_patiently(from, to)) return 1;
    /* Not one that another launcher put there meanwhile. */
    if (is_link(to) || SDL_GetPathInfo(to, NULL)) { snprintf(error, capacity, "%s is there already", to); return 0; }
    if (copy_entry(from, to, error, capacity)) { remove_tree(from); return 1; }
    remove_tree(to);
    return 0;
}

/* What the folder `staging` holds moved into the folder `to`, settings.txt last, each name added to `moved`; what
 * `to` has already kept: a language pack of that name, a mod of that name in its mods folder, which may be a link,
 * and its mods folder when the copy's is a link, never gone through.  A mods in `to` that is no folder (a file, a
 * broken link) fails it. */
static int move_out(const char *staging, const char *to, name_list *moved, char *error, size_t capacity)
{
    name_list names, mods;
    char source[PORTABLE_PATH], target[PORTABLE_PATH], name[600];
    if (!list_names(staging, 0, &names)) { free_names(&names); snprintf(error, capacity, "%s cannot be read", staging); return 0; }
    int ok = 1;
    for (unsigned i = 0; i < names.count && ok; i++) {
        if (!join(staging, names.names[i], source, sizeof source) || !join(to, names.names[i], target, sizeof target)) {
            snprintf(error, capacity, "the path of %s is too long", names.names[i]);
            ok = 0;
        } else if (!strcmp(names.names[i], "mods") && (is_link(target) || SDL_GetPathInfo(target, NULL)) && !is_type(target, SDL_PATHTYPE_DIRECTORY)) {
            snprintf(error, capacity, "%s is not a folder: the mods cannot be put there", target);
            ok = 0;
        } else if (!strcmp(names.names[i], "mods") && !is_link(source) && is_type(target, SDL_PATHTYPE_DIRECTORY)) {
            ok = list_names(source, 0, &mods);
            for (unsigned m = 0; m < mods.count && ok; m++) {
                char mod_from[PORTABLE_PATH], mod_to[PORTABLE_PATH];
                snprintf(name, sizeof name, "mods/%s", mods.names[m]);
                if (!join(source, mods.names[m], mod_from, sizeof mod_from) || !join(target, mods.names[m], mod_to, sizeof mod_to)) ok = 0;
                else if (is_link(mod_to) || SDL_GetPathInfo(mod_to, NULL)) continue;
                else ok = move_entry(mod_from, mod_to, error, capacity) && add_name(moved, name);
            }
            free_names(&mods);
        } else if (is_link(target) || SDL_GetPathInfo(target, NULL)) continue;
        else ok = move_entry(source, target, error, capacity) && add_name(moved, names.names[i]);
    }
    free_names(&names);
    if (!ok && !error[0]) snprintf(error, capacity, "the data cannot be put in %s", to);
    return ok;
}

/* A .copying left in the folder `dir` by a copy cut short removed; with `finish`, one that holds settings.txt, whole
 * since settings.txt is put in it last and moved out of it last, has its move finished first. */
static void tidy(const char *dir, int finish)
{
    char staging[PORTABLE_PATH], error[256] = "";
    if (!join(dir, COPYING, staging, sizeof staging) || !(is_link(staging) || SDL_GetPathInfo(staging, NULL))) return;
    if (finish && !is_link(staging) && has_settings(staging) && !has_settings(dir)) {
        name_list moved = { 0 };
        if (move_out(staging, dir, &moved, error, sizeof error)) fprintf(stderr, "oracles: portable mode: a copy cut short in %s finished\n", dir);
        else fprintf(stderr, "oracles: portable mode: a copy cut short in %s cannot be finished: %s\n", dir, error);
        free_names(&moved);
    }
    remove_tree(staging);
}

/* The folder `path` made, and not found: a .copying that another launcher is filling is not taken. */
static int make_new_folder(const char *path)
{
#ifdef _WIN32
    wchar_t name[PORTABLE_PATH];
    return wide(path, name) && CreateDirectoryW(name, NULL);
#else
    return mkdir(path, 0777) == 0;
#endif
}

enum { COPIED, NOT_MADE, NOT_COPIED };

/* The launcher's data in the folder `from` copied into the folder `to` (without its trailing separator), which has no
 * settings.txt or is not there: first into `to`'s .copying, then moved out of it, settings.txt last, without
 * replacing what `to` holds; a folder `to` that is a link stays one.  What it moved is added to `moved`.  NOT_MADE
 * when nothing can be written in `to`, NOT_COPIED when the copy fails after, each leaving nothing of it; the reason in
 * `error`. */
static int copy_data(const char *from, const char *to, name_list *moved, char *error, size_t capacity)
{
    char staging[PORTABLE_PATH], source[PORTABLE_PATH], target[PORTABLE_PATH];
    name_list list;
    const int existed = is_type(to, SDL_PATHTYPE_DIRECTORY);
    if (!join(to, COPYING, staging, sizeof staging)) { snprintf(error, capacity, "the path of %s is too long", to); return NOT_MADE; }
    if (!list_names(from, 1, &list)) { free_names(&list); snprintf(error, capacity, "%s cannot be read", from); return NOT_COPIED; }
    if (!existed && !SDL_CreateDirectory(to)) { free_names(&list); snprintf(error, capacity, "%s cannot be made: %s", to, SDL_GetError()); return NOT_MADE; }
    int status = COPIED;
    const int ours = make_new_folder(staging);
    if (!ours && (is_link(staging) || SDL_GetPathInfo(staging, NULL))) {
        snprintf(error, capacity, "%s is there already: another copy is under way", staging);
        status = NOT_COPIED;
    } else if (!ours) {
        snprintf(error, capacity, "%s cannot be made", staging);
        status = NOT_MADE;
    }
    for (unsigned i = 0; i < list.count && status == COPIED; i++) {
        /* settings.txt under another name until it is whole: a .copying that holds it holds the whole copy. */
        const int settings = !strcmp(list.names[i], "settings.txt");
        char part[PORTABLE_PATH + 8];
        if (!join(from, list.names[i], source, sizeof source) || !join(staging, list.names[i], target, sizeof target)) {
            snprintf(error, capacity, "the path of %s is too long", list.names[i]);
            status = NOT_COPIED;
        } else if (snprintf(part, sizeof part, "%s%s", target, settings ? ".part" : "") < 0 || !copy_entry(source, part, error, capacity)) status = NOT_COPIED;
        else if (settings && !SDL_RenamePath(part, target)) {
            snprintf(error, capacity, "%s cannot be renamed: %s", part, SDL_GetError());
            status = NOT_COPIED;
        }
    }
    free_names(&list);
    if (status == COPIED && !move_out(staging, to, moved, error, capacity)) {
        undo_moves(to, moved);
        status = NOT_COPIED;
    }
    if (ours) remove_tree(staging);
    if (status != COPIED && !existed) SDL_RemovePath(to);
    return status;
}

int oracles_portable_available(const char *exe_dir)
{
#ifdef __ANDROID__
    (void)exe_dir;
    return 0;
#else
    if (!exe_dir || !exe_dir[0]) return 0;
    /* A macOS application's executable is in its bundle, whose files its signature covers. */
    if (strstr(exe_dir, ".app/Contents/")) return 0;
    /* An AppImage's is in its image, mounted read only at APPDIR; a program it starts inherits the variables, and runs
     * from elsewhere. */
    const char *appimage = SDL_getenv_unsafe("APPIMAGE"), *appdir = SDL_getenv_unsafe("APPDIR");
    return !(appimage && appimage[0] && appdir && appdir[0] && !strncmp(exe_dir, appdir, strlen(appdir)));
#endif
}

/* The marker's name beside the executable, or NULL. */
static const char *marker_found(const char *exe_dir)
{
    char path[PORTABLE_PATH];
    if (!exe_dir || !exe_dir[0]) return NULL;
    for (int i = 0; i < 2; i++) if (join(exe_dir, markers[i], path, sizeof path) && is_type(path, SDL_PATHTYPE_FILE)) return markers[i];
    return NULL;
}

int oracles_portable_marked(const char *exe_dir) { return oracles_portable_available(exe_dir) && marker_found(exe_dir); }

/* 1 when a file can be written in the folder `dir`. */
static int writable(const char *dir)
{
    char probe[PORTABLE_PATH];
    if (!join(dir, ".write-test", probe, sizeof probe) || !SDL_SaveFile(probe, "", 1)) return 0;
    SDL_RemovePath(probe);
    return 1;
}

OraclesUserDirKind oracles_user_dir(const char *forced, const char *exe_dir, char *out, size_t capacity)
{
    if (capacity) out[0] = 0;
    if (forced && forced[0]) return chosen = join(forced, "", out, capacity) ? ORACLES_USER_DIR_FORCED : ORACLES_USER_DIR_NONE;
    const char *marker = marker_found(exe_dir);
    if (marker && !oracles_portable_available(exe_dir)) fprintf(stderr, "oracles: %s is ignored here: the settings are in the user's folder\n", marker);
    else if (marker) {
        char dir[PORTABLE_PATH];
        if (!join(exe_dir, ORACLES_PORTABLE_FOLDER, dir, sizeof dir) || !join(dir, "", out, capacity)) { out[0] = 0; return chosen = ORACLES_USER_DIR_NONE; }
        if (SDL_CreateDirectory(dir) && writable(dir)) {
            fprintf(stderr, "oracles: portable mode (%s beside the executable): the settings are in %s\n", marker, out);
            return chosen = ORACLES_USER_DIR_PORTABLE;
        }
        fprintf(stderr, "oracles: portable mode: %s cannot be written (%s): the settings stay in the user's folder\n", dir, SDL_GetError());
        out[0] = 0;
    }
    return chosen = ORACLES_USER_DIR_SYSTEM;
}

OraclesUserDirKind oracles_user_dir_chosen(void) { return chosen; }

static const char marker_text[] = "While this file is beside the executable, The Oracles Project keeps its settings and mods in the folder "
                                  ORACLES_PORTABLE_FOLDER " beside it, not in the user's folder.\n";

OraclesPortableResult oracles_portable_set(int on, const char *exe_dir, const char *system_dir, char *next_dir, size_t next_capacity, char *error,
                                           size_t capacity)
{
    if (capacity) error[0] = 0;
    if (next_capacity) next_dir[0] = 0;
    if (!oracles_portable_available(exe_dir)) return ORACLES_PORTABLE_UNAVAILABLE;
    char data[PORTABLE_PATH], marker[2][PORTABLE_PATH], system_bare[PORTABLE_PATH];
    if (!join(exe_dir, ORACLES_PORTABLE_FOLDER, data, sizeof data) || !join(exe_dir, markers[0], marker[0], sizeof marker[0])
        || !join(exe_dir, markers[1], marker[1], sizeof marker[1]) || !bare(system_dir, system_bare, sizeof system_bare)) {
        snprintf(error, capacity, "the path of %s is too long", exe_dir);
        return ORACLES_PORTABLE_READ_ONLY;
    }
    if (!join(on ? data : system_dir, "", next_dir, next_capacity) && next_capacity) next_dir[0] = 0;
    const int created = on && !is_type(data, SDL_PATHTYPE_DIRECTORY);
    /* On, user-data is made, or found, where it can be written: the folder of the next launches. */
    if (on && (!SDL_CreateDirectory(data) || !writable(data))) {
        snprintf(error, capacity, "%s cannot be written: %s", data, SDL_GetError());
        if (created) SDL_RemovePath(data);
        return ORACLES_PORTABLE_READ_ONLY;
    }
    if (oracles_portable_marked(exe_dir) == !!on) return ORACLES_PORTABLE_UNCHANGED;
    const char *from = on ? system_bare : data, *to = on ? data : system_bare;
    /* A copy cut short before is of data that may have changed since: copied again. */
    tidy(from, 0);
    tidy(to, 0);
    const int had = has_data(from), has = has_settings(to);
    name_list moved = { 0 };
    int copied = 0;
    if (had && !has) {
        const int status = copy_data(from, to, &moved, error, capacity);
        if (status != COPIED) {
            free_names(&moved);
            if (created) SDL_RemovePath(data);
            return status == NOT_MADE && on ? ORACLES_PORTABLE_READ_ONLY : ORACLES_PORTABLE_COPY_FAILED;
        }
        copied = 1;
    }
    int changed = 1, failed = 0;
    if (on) changed = SDL_SaveFile(marker[0], marker_text, sizeof marker_text - 1);
    else {
        int removed[2] = { 0, 0 };
        for (int i = 0; i < 2 && changed; i++) {
            if (!is_type(marker[i], SDL_PATHTYPE_FILE)) continue;
            changed = removed[i] = SDL_RemovePath(marker[i]);
            failed = i;
        }
        if (!changed) for (int i = 0; i < 2; i++) if (removed[i]) SDL_SaveFile(marker[i], marker_text, sizeof marker_text - 1);
    }
    if (!changed) {
        snprintf(error, capacity, "%s cannot be %s: %s", marker[failed], on ? "written" : "removed", SDL_GetError());
        undo_moves(to, &moved);
        free_names(&moved);
        if (created) SDL_RemovePath(data);
        return ORACLES_PORTABLE_READ_ONLY;
    }
    free_names(&moved);
    return copied ? ORACLES_PORTABLE_COPIED : has ? ORACLES_PORTABLE_KEPT : ORACLES_PORTABLE_NOTHING_TO_COPY;
}

void oracles_portable_tidy(const char *dir)
{
    char folder[PORTABLE_PATH];
    if (dir && dir[0] && bare(dir, folder, sizeof folder)) tidy(folder, 1);
}

OraclesPortableResult oracles_portable_switch(int on, char *next_dir, size_t next_capacity, char *error, size_t capacity)
{
    const char *exe_dir = SDL_GetBasePath(), *forced = SDL_getenv_unsafe("ORACLES_SETTINGS_DIR");
    if (capacity) error[0] = 0;
    if (next_capacity) next_dir[0] = 0;
    /* ORACLES_SETTINGS_DIR wins over the marker at each launch. */
    if ((forced && forced[0]) || !oracles_portable_available(exe_dir)) return ORACLES_PORTABLE_UNAVAILABLE;
    char *system_dir = SDL_GetPrefPath("", "the-oracles-project");
    if (!system_dir) { snprintf(error, capacity, "the user's folder is unavailable: %s", SDL_GetError()); return ORACLES_PORTABLE_COPY_FAILED; }
    const OraclesPortableResult result = oracles_portable_set(on, exe_dir, system_dir, next_dir, next_capacity, error, capacity);
    SDL_free(system_dir);
    return result;
}
