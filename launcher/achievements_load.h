/* The home screen's Achievements page: the game's list loaded with no game running.  A client of its own logs in with the
 * account kept, then reads the game's achievements and the account's unlocks (achievements_catalogue.h) on the transport's
 * thread, polled from the home screen's loop, never waited for.  No game is loaded in the client: opening the page is no
 * session of the game for the server, no startsession and no ping are sent, nothing is evaluated, nothing is awarded.
 * Only a game's session sends the unlocks kept offline and writes achievements-pending.txt.  The badges go into the folder
 * a session's go to, and the client goes once the list and its badges are in. */
#ifndef ORACLES_LAUNCHER_ACHIEVEMENTS_LOAD_H
#define ORACLES_LAUNCHER_ACHIEVEMENTS_LOAD_H

#include "achievements_account.h"
#include "achievements_screens.h"

/* No answer in this long ends a load, and the wait for the badges after it. */
#define ORACLES_ACHIEVEMENTS_LOAD_WAIT_MS 30000.0

typedef struct OraclesAchievementsLoad OraclesAchievementsLoad;

/* The game's identity from its ROM's file, a zip's included, as a session reads it: 1 with `hash`, 0 when it cannot be read. */
int oracles_achievements_load_hash(const char *rom_path, char hash[33]);
/* A load begun: the login with `account` on its way, the game `hash` read once it is in.  The transport is the load's from
 * here.  `account_path` (NULL: nothing written) keeps a token the server changes, and forgets one it refuses; `badges` the
 * folder of the badges, which exists (empty: none fetched).  NULL out of memory. */
OraclesAchievementsLoad *oracles_achievements_load_begin(const OraclesAchievementsTransport *transport, const char *user_agent, const OraclesAchievementsAccount *account,
                                                         const char *account_path, const char *hash, const char *badges, double now_ms);
/* The answers that came: the state (OraclesAchievementsListState), the list once there, and whether the client still works.
 * Done, it is destroyed here. */
OraclesAchievementsListState oracles_achievements_load_poll(OraclesAchievementsLoad *load, double now_ms, const OraclesAchievementsList **list, int *busy);
void oracles_achievements_load_end(OraclesAchievementsLoad *load);

/* The page's load begun for `game`, the host's part: NULL when it cannot start. */
typedef OraclesAchievementsLoad *(*OraclesAchievementsLoadBegin)(void *opaque, OraclesHomeNav *nav, int game);
/* The load the page wants now, called by the home screen's loop before each frame: begun as the page opens on a game with
 * achievements, logged in and On, polled while it runs, let go with nav's list as soon as it is not wanted or the game
 * changes (`*load`, `*game` its state, the host's to keep).  A load that failed stays so until the page opens again.
 * 1 when what the page shows changed. */
int oracles_achievements_page_load(OraclesAchievementsLoad **load, int *game, OraclesHomeNav *nav, OraclesAchievementsLoadBegin begin, void *opaque, double now_ms);

#endif
