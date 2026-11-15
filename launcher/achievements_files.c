/* The achievements' own files (achievements_files.h). */
#include "achievements_files.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#if defined(_WIN32)
#include <process.h>
#include <windows.h>
#define process_id() ((unsigned long)_getpid())
#else
#include <fcntl.h>
#include <unistd.h>
#define process_id() ((unsigned long)getpid())
#endif

static FILE *create(const char *path, int private_file)
{
#if defined(_WIN32)
    (void)private_file;
    return fopen(path, "wb");
#else
    const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, private_file ? 0600 : 0644);
    return fd < 0 ? NULL : fdopen(fd, "wb");
#endif
}

static int replace(const char *from, const char *to)
{
#if defined(_WIN32)
    wchar_t wide_from[4096], wide_to[4096];
    return MultiByteToWideChar(CP_UTF8, 0, from, -1, wide_from, 4096) > 0 && MultiByteToWideChar(CP_UTF8, 0, to, -1, wide_to, 4096) > 0
           && MoveFileExW(wide_from, wide_to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
    return rename(from, to) == 0;
#endif
}

int oracles_achievements_write_file(const char *path, const void *data, size_t size, int private_file)
{
    if (!path || !path[0]) return 0;
    char temporary[4096 + 32];
    snprintf(temporary, sizeof temporary, "%s.%lu.tmp", path, process_id());
    FILE *f = create(temporary, private_file);
    if (!f) return 0;
    const int ok = fwrite(data, 1, size, f) == size && fflush(f) == 0;
    if (fclose(f) != 0 || !ok || !replace(temporary, path)) { remove(temporary); return 0; }
    return 1;
}

int oracles_achievements_file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

int oracles_achievements_write_image(const char *path, const uint8_t *body, size_t length)
{
    static const uint8_t png[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
    if (!body || length <= sizeof png || memcmp(body, png, sizeof png) != 0) return 0;
    return oracles_achievements_write_file(path, body, length, 0);
}

static void lock_path(const char *path, char *out, size_t capacity)
{
    snprintf(out, capacity, "%s.lock", path);
}

static void pause_a_little(void)
{
#if defined(_WIN32)
    Sleep(20);
#else
    const struct timespec t = { 0, 20 * 1000 * 1000 };
    nanosleep(&t, NULL);
#endif
}

int oracles_achievements_lock(const char *path)
{
    char lock[4096 + 8];
    lock_path(path, lock, sizeof lock);
    for (int i = 0; i < 50; i++) {
        FILE *f = fopen(lock, "wx");   /* C11's exclusive creation */
        if (f) { fprintf(f, "%lu\n", process_id()); fclose(f); return 1; }
        struct stat st;
        if (stat(lock, &st) == 0 && time(NULL) - st.st_mtime > 60) { remove(lock); continue; }
        pause_a_little();
    }
    return 0;
}

void oracles_achievements_unlock(const char *path)
{
    char lock[4096 + 8];
    lock_path(path, lock, sizeof lock);
    remove(lock);
}
