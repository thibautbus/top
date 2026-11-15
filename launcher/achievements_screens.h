/* What the player sees of a session's RetroAchievements (achievements.h, ui_achievements.h), without the session or SDL:
 * the toasts its client's events tell, and the pause's list of the game's achievements. */
#ifndef ORACLES_LAUNCHER_ACHIEVEMENTS_SCREENS_H
#define ORACLES_LAUNCHER_ACHIEVEMENTS_SCREENS_H

#include "achievements.h"
#include "ui_achievements.h"

#include <stdint.h>

/* What was told already: the game's set loaded (a suspension is told from then), the suspension's reason, the offline
 * toast (again only after another unlock), the server unreachable as the game started (once a session). */
typedef struct OraclesAchievementsTold {
    int loaded, suspension, offline, unreachable;
} OraclesAchievementsTold;

/* The suspension's reason as the screens name it (OraclesAchievementsSuspension). */
int oracles_achievements_suspension_kind(const OraclesAchievements *a);
/* The toasts of `event` of client `a` for `game`: the game's progress at its start (or that a cheat or a mod suspends
 * them already), each unlock with its badge, the game completed, an unlock kept offline, the server unreachable at the
 * login or as the game loads. */
void oracles_achievements_tell(OraclesAchievementsToasts *toasts, OraclesAchievementsTold *told, OraclesAchievements *a, const char *game,
                               const OraclesAchievementsEvent *event);
/* After a frame: a suspension once the game runs, told once. */
void oracles_achievements_tell_suspension(OraclesAchievementsToasts *toasts, OraclesAchievementsTold *told, const OraclesAchievements *a);

/* An unlock's date as the list shows it ("21 Sep 2026"). */
typedef void (*OraclesAchievementsDate)(int64_t when, char *out, size_t capacity);
/* The list the pause shows, made now: each achievement of the set (oracles_achievements_each) with its date and its
 * progress, the badges in `badges`; 0, the list left empty, before the set is loaded. */
int oracles_achievements_list_make(OraclesAchievements *a, const char *badges, OraclesAchievementsDate date, OraclesAchievementsList *list);

#endif
