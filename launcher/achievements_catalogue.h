/* A game's achievements and the account's unlocks read from the server with no session: rcheevos' own requests, the game's
 * sets by its hash (achievementsets), then its unlocks (unlocks, casual and hardcore), through a client's transport, on
 * its thread, answered by its polls.  Neither startsession nor ping nor an award is sent: the server sees no game started,
 * and rc_client has no game loaded.  The server gives these unlocks no date. */
#ifndef ORACLES_LAUNCHER_ACHIEVEMENTS_CATALOGUE_H
#define ORACLES_LAUNCHER_ACHIEVEMENTS_CATALOGUE_H

#include "achievements.h"

typedef enum OraclesAchievementsCatalogueState {
    ORACLES_ACHIEVEMENTS_CATALOGUE_WAITING,
    ORACLES_ACHIEVEMENTS_CATALOGUE_DONE,
    ORACLES_ACHIEVEMENTS_CATALOGUE_UNREACHABLE,   /* no server answered, or its proxy failed */
    ORACLES_ACHIEVEMENTS_CATALOGUE_FAILED         /* the server refused, or a game it does not know */
} OraclesAchievementsCatalogueState;

typedef struct OraclesAchievementsCatalogue OraclesAchievementsCatalogue;

/* Asked of `a`'s transport for the account `user` and `token`, `hash` the game's identity.  It outlives `a`'s transport: a
 * request given up answers it.  NULL out of memory. */
OraclesAchievementsCatalogue *oracles_achievements_catalogue_begin(OraclesAchievements *a, const char *user, const char *token, const char *hash);
OraclesAchievementsCatalogueState oracles_achievements_catalogue_state(const OraclesAchievementsCatalogue *catalogue);
/* Each achievement of the game once done, the unlocked first, then the locked, each in the set's order: no date, no
 * progress, which only a session has. */
void oracles_achievements_catalogue_each(const OraclesAchievementsCatalogue *catalogue, OraclesAchievementsEach each, void *opaque);
/* Each achievement with a badge: its name and its two images' addresses. */
void oracles_achievements_catalogue_badges(const OraclesAchievementsCatalogue *catalogue, OraclesAchievementsBadge badge, void *opaque);
void oracles_achievements_catalogue_free(OraclesAchievementsCatalogue *catalogue);

#endif
