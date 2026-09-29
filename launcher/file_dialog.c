#include "file_dialog.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

#ifdef __ANDROID__
#include <jni.h>
#endif

/* A dialog's request, then its answer, carried by the event SDL's queue brings back to the launcher's loop. */
typedef struct dialog_answer {
    OraclesDialogKind kind;
    int game;
    OraclesDialogResult result;
    char *path;
} dialog_answer;

static Uint32 answer_event;   /* registered by the first dialog, on the launcher's thread */

#ifdef __ANDROID__
/* The name Android's picker shows for `uri` (OraclesActivity.displayName), into `name`; else the URI's last segment,
 * percent-decoded, after its last '/' or ':'.  Never empty, never a path. */
static void display_name(const char *uri, char *name, size_t capacity)
{
    name[0] = 0;
    JNIEnv *env = SDL_GetAndroidJNIEnv();
    jobject activity = SDL_GetAndroidActivity();
    if (env && activity) {
        jclass class = (*env)->GetObjectClass(env, activity);
        jmethodID method = (*env)->GetStaticMethodID(env, class, "displayName", "(Ljava/lang/String;)Ljava/lang/String;");
        jstring argument = (*env)->NewStringUTF(env, uri);
        jstring shown = method && argument ? (*env)->CallStaticObjectMethod(env, class, method, argument) : NULL;
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        if (shown) {
            const char *text = (*env)->GetStringUTFChars(env, shown, NULL);
            if (text) { snprintf(name, capacity, "%s", text); (*env)->ReleaseStringUTFChars(env, shown, text); }
            (*env)->DeleteLocalRef(env, shown);
        }
        if (argument) (*env)->DeleteLocalRef(env, argument);
        (*env)->DeleteLocalRef(env, class);
        (*env)->DeleteLocalRef(env, activity);
    }
    if (!name[0]) {
        const char *segment = strrchr(uri, '/');
        size_t n = 0;
        for (const char *p = segment ? segment + 1 : uri; *p && n + 1 < capacity; p++) {
            unsigned value;
            if (p[0] == '%' && sscanf(p + 1, "%2x", &value) == 1) { name[n++] = (char)value; p += 2; }
            else name[n++] = *p;
        }
        name[n] = 0;
        const char *last = strrchr(name, '/');
        if (!last) last = strrchr(name, ':');
        if (last) memmove(name, last + 1, strlen(last + 1) + 1);
    }
    for (char *p = name; *p; p++) if (*p == '/' || *p == '\\') *p = '_';
    if (!name[0] || !strcmp(name, ".") || !strcmp(name, "..")) snprintf(name, capacity, "chosen");
}

/* Android's picker answers a content:// URI, which the engine's fopen cannot open: the file is copied into the
 * application's folder on the shared storage (Android/data/<package>/files/roms or patches), where its save then goes
 * too, beside it, reachable over USB (oracles_dialog_store_copy).  On the launcher's thread, as the answer is read, not on
 * Android's interface thread, which a slow document would hold; at most MAX_COPY bytes, a ROM or a patch being far
 * smaller.  1 with the copy's path, else 0 with SDL's reason. */
#define MAX_COPY (16 * 1024 * 1024)

static int local_copy(const char *uri, OraclesDialogKind kind, char *path, size_t capacity, int *fresh)
{
    const char *base = SDL_GetAndroidExternalStoragePath();
    if (!base) return 0;
    char name[256], folder[1024];
    display_name(uri, name, sizeof name);
    snprintf(folder, sizeof folder, "%s/%s", base, kind == ORACLES_DIALOG_PATCH ? "patches" : "roms");
    SDL_IOStream *in = SDL_IOFromFile(uri, "rb");
    if (!in) return 0;
    const Sint64 length = SDL_GetIOSize(in);
    if (length < 0 || length > MAX_COPY) {
        SDL_CloseIO(in);
        return SDL_SetError("%s is larger than %d MiB, no ROM or patch", name, MAX_COPY / (1024 * 1024)), 0;
    }
    size_t size = 0;
    void *data = SDL_LoadFile_IO(in, &size, true);
    if (!data) return 0;
    const int ok = oracles_dialog_store_copy(folder, name, data, size, path, capacity, fresh);
    SDL_free(data);
    return ok;
}
#endif

/* SDL's answer, on the thread the system gave it on: a list of paths, an empty one (or an empty path, from zenity)
 * when the player cancelled, NULL when no dialog could open. */
static void SDLCALL answered(void *userdata, const char *const *files, int filter)
{
    (void)filter;
    dialog_answer *answer = userdata;
    if (!files) {
        answer->result = ORACLES_DIALOG_UNAVAILABLE;
        SDL_Log("no file dialog: %s", SDL_GetError());
    } else if (!files[0] || !files[0][0]) {
        answer->result = ORACLES_DIALOG_CANCELLED;
    } else {
        answer->result = ORACLES_DIALOG_CHOSEN;
        answer->path = SDL_strdup(files[0]);   /* Android's content:// URI copied when the answer is read */
    }
    SDL_Event e;
    SDL_zero(e);
    e.type = answer_event;
    e.user.data1 = answer;
    if (!SDL_PushEvent(&e)) { SDL_free(answer->path); SDL_free(answer); }
}

/* Each kind's title and filters. */
static const struct {
    const char *title;
    SDL_DialogFileFilter filters[2];
} kinds[2] = {
    { "Choose ROM", { { "Game Boy Color ROM", "gbc;gb" }, { "All files", "*" } } },
    { "Choose patch", { { "BPS patch", "bps" }, { "All files", "*" } } },
};

void oracles_choose_file(struct SDL_Window *window, OraclesDialogKind kind, const char *folder, int game)
{
    if (!answer_event) answer_event = SDL_RegisterEvents(1);
    dialog_answer *answer = SDL_calloc(1, sizeof *answer);
    if (!answer) return;
    answer->kind = kind;
    answer->game = game;
    /* The folder with a separator after it: zenity opens in it rather than on a file of that name. */
    char location[4200] = "";
    if (folder && folder[0]) {
        const size_t length = strlen(folder);
        const int separated = folder[length - 1] == '/' || folder[length - 1] == '\\';
        snprintf(location, sizeof location, "%s%s", folder, separated ? "" : "/");
    }
    const SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetPointerProperty(properties, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, (void *)kinds[kind].filters);
#ifdef __APPLE__
    /* A pattern "*" makes macOS's panel take any file: there the kind's filter alone, as osascript had it. */
    SDL_SetNumberProperty(properties, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, 1);
#else
    SDL_SetNumberProperty(properties, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, 2);
#endif
    SDL_SetPointerProperty(properties, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, window);   /* modal over the launcher */
    if (location[0]) SDL_SetStringProperty(properties, SDL_PROP_FILE_DIALOG_LOCATION_STRING, location);
    SDL_SetStringProperty(properties, SDL_PROP_FILE_DIALOG_TITLE_STRING, kinds[kind].title);
    if (answer_event) SDL_ShowFileDialogWithProperties(SDL_FILEDIALOG_OPENFILE, answered, answer, properties);
    else answered(answer, NULL, -1);
    SDL_DestroyProperties(properties);
}

int oracles_dialog_answer(const union SDL_Event *event, OraclesDialogResult *result, OraclesDialogKind *kind, int *game,
                          char *path, size_t capacity, int *copied)
{
    if (!answer_event || event->type != answer_event) return 0;
    dialog_answer *answer = event->user.data1;
    *result = answer->result;
    *kind = answer->kind;
    *game = answer->game;
    *copied = 0;
    snprintf(path, capacity, "%s", answer->path ? answer->path : "");
#ifdef __ANDROID__
    if (*result == ORACLES_DIALOG_CHOSEN && strncmp(path, "content://", 10) == 0 && !local_copy(answer->path, answer->kind, path, capacity, copied)) {
        SDL_Log("%s cannot be copied into the application's folder: %s", answer->path, SDL_GetError());
        *result = ORACLES_DIALOG_FAILED;
        path[0] = 0;
    }
#endif
    SDL_free(answer->path);
    SDL_free(answer);
    return 1;
}

/* folder/name, or with " (2)"... before the extension: the first that is free or holds these very bytes. */
static void numbered(const char *folder, const char *name, int n, char *out, size_t capacity)
{
    if (n == 1) { snprintf(out, capacity, "%s/%s", folder, name); return; }
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name) dot = name + strlen(name);
    snprintf(out, capacity, "%s/%.*s (%d)%s", folder, (int)(dot - name), name, n, dot);
}

static int holds(const char *path, const void *data, size_t size)
{
    size_t length = 0;
    void *there = SDL_LoadFile(path, &length);
    const int same = there && length == size && memcmp(there, data, size) == 0;
    SDL_free(there);
    return same;
}

int oracles_dialog_store_copy(const char *folder, const char *name, const void *data, size_t size, char *path, size_t capacity,
                              int *fresh)
{
    *fresh = 0;
    if (!SDL_CreateDirectory(folder)) return 0;
    char candidate[4096], partial[4200];
    for (int n = 1; n < 100; n++) {
        numbered(folder, name, n, candidate, sizeof candidate);
        SDL_PathInfo info;
        if (SDL_GetPathInfo(candidate, &info)) {
            if (info.type == SDL_PATHTYPE_FILE && holds(candidate, data, size)) { snprintf(path, capacity, "%s", candidate); return 1; }
            continue;   /* another file of that name, a save perhaps: never written over */
        }
        /* Written beside it then renamed: a copy cut short (a full storage) leaves no half file under the name. */
        snprintf(partial, sizeof partial, "%s.partial", candidate);
        if (!SDL_SaveFile(partial, data, size) || !SDL_RenamePath(partial, candidate)) { SDL_RemovePath(partial); return 0; }
        snprintf(path, capacity, "%s", candidate);
        *fresh = 1;
        return 1;
    }
    return SDL_SetError("no free name for %s in %s", name, folder), 0;
}

int oracles_open_folder(const char *folder, char *error, size_t capacity)
{
    /* A file URL: the path's bytes outside the unreserved set percent-encoded, a Windows drive after a slash. */
    char url[8192];
    size_t n = (size_t)snprintf(url, sizeof url, "file://%s", folder[0] == '/' ? "" : "/");
    static const char hex[] = "0123456789ABCDEF";
    for (const unsigned char *p = (const unsigned char *)folder; *p && n + 4 < sizeof url; p++) {
        const unsigned char c = *p == '\\' ? '/' : *p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || strchr("-._~/:", c)) url[n++] = (char)c;
        else { url[n++] = '%'; url[n++] = hex[c >> 4]; url[n++] = hex[c & 15]; }
    }
    url[n] = 0;
    if (SDL_OpenURL(url)) return 0;
    snprintf(error, capacity, "%s", SDL_GetError());
    return -1;
}
