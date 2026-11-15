/* RetroAchievements' account as the launcher keeps it (achievements_account.h). */
#include "achievements_account.h"

#include "achievements_files.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void oracles_achievements_account_read(const char *path, OraclesAchievementsAccount *a)
{
    memset(a, 0, sizeof *a);
    FILE *f = path && path[0] ? fopen(path, "r") : NULL;
    if (!f) return;
    char line[ORACLES_ACHIEVEMENTS_ACCOUNT_FIELD + 16];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!strncmp(line, "user=", 5)) snprintf(a->user, sizeof a->user, "%.*s", (int)sizeof a->user - 1, line + 5);
        else if (!strncmp(line, "token=", 6)) snprintf(a->token, sizeof a->token, "%.*s", (int)sizeof a->token - 1, line + 6);
    }
    fclose(f);
}

/* Readable by the player alone (0600 where the system has it): the token logs in as them. */
int oracles_achievements_account_write(const char *path, const OraclesAchievementsAccount *a)
{
    if (!path || !path[0]) return 0;
    char text[3 * ORACLES_ACHIEVEMENTS_ACCOUNT_FIELD];
    const int length = snprintf(text, sizeof text, "# RetroAchievements: the account the games log in with, its token and never its password.\n"
                                                   "# Delete this file to log out.\nuser=%s\ntoken=%s\n", a->user, a->token);
    return length > 0 && (size_t)length < sizeof text && oracles_achievements_write_file(path, text, (size_t)length, 1);
}

struct OraclesAchievementsHomeLogin {
    OraclesAchievements *client;
    char path[4096];   /* empty: nothing written */
    int done, ok, refused;
    char user[ORACLES_ACHIEVEMENTS_ACCOUNT_FIELD];
    double started_ms;
};

static void on_login(void *opaque, const OraclesAchievementsEvent *event)
{
    OraclesAchievementsHomeLogin *l = opaque;
    if (event->kind != ORACLES_ACHIEVEMENTS_LOGGED_IN && event->kind != ORACLES_ACHIEVEMENTS_LOGIN_FAILED) return;
    l->done = 1;
    l->ok = event->kind == ORACLES_ACHIEVEMENTS_LOGGED_IN && event->user && event->user[0] && event->token && event->token[0];
    l->refused = event->kind == ORACLES_ACHIEVEMENTS_LOGIN_FAILED && event->token_refused;
    if (!l->ok) return;
    snprintf(l->user, sizeof l->user, "%s", event->user);
    if (!l->path[0]) { fprintf(stderr, "oracles: achievements: logged in as %s with the offline answers: nothing kept\n", event->user); return; }
    OraclesAchievementsAccount a;
    snprintf(a.user, sizeof a.user, "%s", event->user);   /* the account's own name, as the server writes it */
    snprintf(a.token, sizeof a.token, "%s", event->token);
    if (!oracles_achievements_account_write(l->path, &a)) { fprintf(stderr, "oracles: achievements: the account could not be written\n"); l->ok = 0; }
}

OraclesAchievementsHomeLogin *oracles_achievements_home_login_begin(const OraclesAchievementsTransport *transport, const char *user_agent, const char *path,
                                                                    const char *user, const char *password, double now_ms)
{
    OraclesAchievementsHomeLogin *l = calloc(1, sizeof *l);
    if (!l) { if (transport && transport->destroy) transport->destroy(transport->opaque); return NULL; }
    snprintf(l->path, sizeof l->path, "%s", path ? path : "");
    l->started_ms = now_ms;
    l->client = transport ? oracles_achievements_create(transport, user_agent, on_login, l) : NULL;
    if (!l->client) {
        if (transport && transport->destroy) transport->destroy(transport->opaque);
        l->done = 1;
        return l;
    }
    oracles_achievements_login(l->client, user, password, NULL);
    return l;
}

int oracles_achievements_home_login_poll(OraclesAchievementsHomeLogin *l, double now_ms, char *user, size_t capacity)
{
    if (!l->done && l->client) oracles_achievements_idle(l->client);
    if (!l->done && now_ms - l->started_ms < ORACLES_ACHIEVEMENTS_LOGIN_WAIT_MS) return -2;
    if (l->ok) { snprintf(user, capacity, "%.*s", (int)capacity - 1, l->user); return 1; }
    return l->refused ? 0 : -1;
}

void oracles_achievements_home_login_end(OraclesAchievementsHomeLogin *l)
{
    if (!l) return;
    if (l->client) oracles_achievements_destroy(l->client);
    free(l);
}
