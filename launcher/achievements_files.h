/* The achievements' own files: the account, the unlocks waiting, the badges' images. */
#ifndef ORACLES_LAUNCHER_ACHIEVEMENTS_FILES_H
#define ORACLES_LAUNCHER_ACHIEVEMENTS_FILES_H

#include <stddef.h>
#include <stdint.h>

/* `size` bytes written as `path` through a temporary file of this process's own, which then replaces it whole
 * (MoveFileExW on Windows): a write cut short leaves the file as it was, never none.  `private_file`: readable by the
 * user alone where the system has the notion (0600). 1 when written. */
int oracles_achievements_write_file(const char *path, const void *data, size_t size, int private_file);
int oracles_achievements_file_exists(const char *path);
/* A PNG (its signature checked) written as `path`: 1, or 0 for a body that is not one or a file not written. */
int oracles_achievements_write_image(const char *path, const uint8_t *body, size_t length);

/* `path`.lock made for this process (two launchers never change the file at once), waited for a second at most, one
 * older than a minute taken as left by a process gone: 1 when held, 0 when not (the caller goes on without). */
int oracles_achievements_lock(const char *path);
void oracles_achievements_unlock(const char *path);

#endif
