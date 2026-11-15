/* The unlocks earned that RetroAchievements has not yet acknowledged, kept on disk until it does: a line each, the
 * account, the game's hash, the achievement and when it was earned (seconds since 1970).  rc_client sends an unlock
 * again while the game runs; one still unacknowledged when the game ends is sent at the next login (achievements.h,
 * oracles_achievements_resend), with the time since it was earned. */
#ifndef ORACLES_LAUNCHER_ACHIEVEMENTS_PENDING_H
#define ORACLES_LAUNCHER_ACHIEVEMENTS_PENDING_H

#include "achievements.h"

#include <stddef.h>
#include <stdint.h>

#define ORACLES_ACHIEVEMENTS_PENDING_MAX 256   /* the lines kept; past them, the oldest go */

typedef struct OraclesAchievementsPendingUnlock {
    char user[64], hash[33];
    uint32_t id;
    int64_t when;
} OraclesAchievementsPendingUnlock;

/* Reads the file at `path` (none: no unlock) into `out`, at most `capacity`: the number read. */
size_t oracles_achievements_pending_read(const char *path, OraclesAchievementsPendingUnlock *out, size_t capacity);
/* Adds `unlock` (once: the same account and achievement are one line), or removes the line of `user`'s achievement
 * `id`; the file written again whole (oracles_achievements_write_file, private), under its lock so that two launchers
 * never lose each other's lines.  1 when written, or when nothing changed. */
int oracles_achievements_pending_add(const char *path, const OraclesAchievementsPendingUnlock *unlock);
int oracles_achievements_pending_remove(const char *path, const char *user, uint32_t id);

/* What a session's event changes in the file at `path`, for the account `user` playing the game `hash`, at `now`: an
 * unlock not yet acknowledged added; an award settled removed, unless it was refused for the token (it is sent again
 * once the account logs in again). */
void oracles_achievements_pending_follow(const char *path, const char *user, const char *hash, const OraclesAchievementsEvent *event, int64_t now);
/* The account's unlocks of the file sent again through `a`, logged in as `user` with `token`, at `now`: their number. */
unsigned oracles_achievements_pending_resend(OraclesAchievements *a, const char *path, const char *user, const char *token, int64_t now);

#endif
