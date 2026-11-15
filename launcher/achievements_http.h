/* The achievements' transport over the platform's HTTPS client (update_fetch.h): the requests queued, sent one at a
 * time by a thread of their own, the POSTs (the session's requests, the unlocks) before the GETs (the badges' images),
 * and their answers called by the transport's poll, on the game's thread.  A request no server answers in its time is
 * answered with status 0, which rc_client sends again when it must (an unlock, with a growing delay). */
#ifndef ORACLES_LAUNCHER_ACHIEVEMENTS_HTTP_H
#define ORACLES_LAUNCHER_ACHIEVEMENTS_HTTP_H

#include "achievements.h"
#include "update_fetch.h"

#define ORACLES_ACHIEVEMENTS_HTTP_TIMEOUT_MS 20000
#define ORACLES_ACHIEVEMENTS_HTTP_MAX_BODY (8 * 1024 * 1024)   /* a set's data; a badge is some kilobytes */

/* What sends one request: oracles_http_request, or the tests' own. */
typedef int (*OraclesHttpFetch)(const OraclesHttpRequest *request, char **body, size_t *length, int *status, char *error, size_t capacity);

typedef struct OraclesAchievementsHttpOptions {
    OraclesHttpFetch fetch;   /* NULL: oracles_http_request */
    int timeout_ms;           /* 0: ORACLES_ACHIEVEMENTS_HTTP_TIMEOUT_MS */
    int loopback_http;        /* the tests' local server (OraclesHttpRequest) */
} OraclesAchievementsHttpOptions;

/* The transport, its thread started: 1, or 0 when it cannot start.  `options` NULL: the platform's client. */
int oracles_achievements_http_transport(OraclesAchievementsTransport *transport, const OraclesAchievementsHttpOptions *options);

#endif
