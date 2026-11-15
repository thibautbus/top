/* The badges' images of the game loaded, kept on disk: fetched in the background once the game is known, the earned
 * and the locked one of each achievement, as `directory`/<badge>.png and <badge>_lock.png, never again once there.
 * What shows an unlock before its badge is there shows it without (the file is looked for when it is shown). */
#ifndef ORACLES_LAUNCHER_ACHIEVEMENTS_BADGES_H
#define ORACLES_LAUNCHER_ACHIEVEMENTS_BADGES_H

#include "achievements.h"

#define ORACLES_ACHIEVEMENTS_BADGES_MAX 512   /* images fetched in a session, two an achievement */

/* The image file of badge `name` ([A-Za-z0-9_-], 1 to 16 of them), earned or `locked`, in `directory`: 1, or 0 for
 * a name that cannot be a file's. */
int oracles_achievements_badge_path(const char *directory, const char *name, int locked, char *out, size_t capacity);

typedef struct OraclesAchievementsBadgeJob {
    char path[1024];
    int done;
} OraclesAchievementsBadgeJob;

typedef struct OraclesAchievementsBadges {
    OraclesAchievementsBadgeJob jobs[ORACLES_ACHIEVEMENTS_BADGES_MAX];
    unsigned count;
} OraclesAchievementsBadges;

/* Asks, through `a`'s transport, for each image of the game's badges `directory` (which exists) does not have yet
 * (the transport looks, off the game's thread); each answered with a PNG is written there.  `badges` holds the requests until their answers: it outlives `a`'s
 * transport. */
void oracles_achievements_fetch_badges(OraclesAchievements *a, const char *directory, OraclesAchievementsBadges *badges);

#endif
