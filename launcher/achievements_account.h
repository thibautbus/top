/* RetroAchievements' account as the launcher keeps it (achievements.h): achievements.txt, its name and token, never its
 * password; and a login with a password, its answer looked for from a loop (the home screen's). */
#ifndef ORACLES_LAUNCHER_ACHIEVEMENTS_ACCOUNT_H
#define ORACLES_LAUNCHER_ACHIEVEMENTS_ACCOUNT_H

#include "achievements.h"

#include <stddef.h>

#define ORACLES_ACHIEVEMENTS_ACCOUNT_FIELD 256
typedef struct OraclesAchievementsAccount {
    char user[ORACLES_ACHIEVEMENTS_ACCOUNT_FIELD], token[ORACLES_ACHIEVEMENTS_ACCOUNT_FIELD];
} OraclesAchievementsAccount;

/* The account kept in `path`, empty without the file; written there, readable by the player alone: 1 when written. */
void oracles_achievements_account_read(const char *path, OraclesAchievementsAccount *account);
int oracles_achievements_account_write(const char *path, const OraclesAchievementsAccount *account);

/* A login with `user`'s password over `transport` (the login's from here) on a client of its own, at `now_ms`; once in,
 * the account and its token written to `path` (NULL: nothing kept, the developer's offline answers).  Its answer:
 * 1 in, the account's own name in `user`; 0 the name or the password refused; -1 no answer, in 30 s at most or none
 * possible; -2 not yet.  Ended (its client and transport destroyed, an answer still to come dropped) whatever it
 * answered. */
#define ORACLES_ACHIEVEMENTS_LOGIN_WAIT_MS 30000.0
typedef struct OraclesAchievementsHomeLogin OraclesAchievementsHomeLogin;
OraclesAchievementsHomeLogin *oracles_achievements_home_login_begin(const OraclesAchievementsTransport *transport, const char *user_agent, const char *path,
                                                                    const char *user, const char *password, double now_ms);
int oracles_achievements_home_login_poll(OraclesAchievementsHomeLogin *login, double now_ms, char *user, size_t capacity);
void oracles_achievements_home_login_end(OraclesAchievementsHomeLogin *login);

#endif
