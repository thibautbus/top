/* The transports of the achievements' requests (achievements.h). */
#ifndef ORACLES_LAUNCHER_ACHIEVEMENTS_TRANSPORT_H
#define ORACLES_LAUNCHER_ACHIEVEMENTS_TRANSPORT_H

#include "achievements.h"

/* The platform's HTTPS client, POST and GET (achievements_http.c): 1 with *transport, 0 when its thread cannot start. */
int oracles_achievements_platform_transport(OraclesAchievementsTransport *transport);

/* For the developer, without the network: each request answered by the file `directory`/<r>.json, <r> the request's
 * r= (login2, achievementsets, startsession, awardachievement...), status 200, or by no server where there is none;
 * a GET (an image) by no server.  Each request is said on the standard error, its password and token left out.
 * 1 with *transport, 0 out of memory. */
int oracles_achievements_offline_transport(const char *directory, OraclesAchievementsTransport *transport);

#endif
