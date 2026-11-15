/* The home screen's list of a game's achievements (achievements_load.h). */
#include "achievements_load.h"

#include "achievements_badges.h"
#include "rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct OraclesAchievementsLoad {
    OraclesAchievements *client;
    OraclesAchievementsAccount account;
    char account_path[4096], badges_folder[4096];
    char hash[33];
    OraclesAchievementsCatalogue *catalogue;   /* asked for once logged in */
    OraclesAchievementsListState state;
    double started_ms, ready_ms;
    int made;
    OraclesAchievementsBadges badges;
    OraclesAchievementsList list;
};

int oracles_achievements_load_hash(const char *rom_path, char hash[33])
{
    char error[256];
    size_t size = 0;
    uint8_t *rom = oracles_rom_read_file(rom_path, &size, error, sizeof error);
    if (!rom) { fprintf(stderr, "oracles: achievements: %s cannot be read: %s\n", rom_path, error); return 0; }
    oracles_achievements_hash(rom, size, hash);
    free(rom);
    return 1;
}

/* The account as a session keeps it, a changed token written, a refused one forgotten; logged in, the game's list asked
 * for with the token the server gave; the state. */
static void on_event(void *opaque, const OraclesAchievementsEvent *event)
{
    OraclesAchievementsLoad *l = opaque;
    if (event->kind == ORACLES_ACHIEVEMENTS_LOGGED_IN && l->account_path[0]) {
        OraclesAchievementsAccount kept = l->account;
        snprintf(kept.user, sizeof kept.user, "%s", event->user && event->user[0] ? event->user : l->account.user);
        snprintf(kept.token, sizeof kept.token, "%s", event->token ? event->token : "");
        if ((strcmp(kept.user, l->account.user) != 0 || strcmp(kept.token, l->account.token) != 0) && !oracles_achievements_account_write(l->account_path, &kept))
            fprintf(stderr, "oracles: achievements: the account's token could not be written\n");
    } else if (event->kind == ORACLES_ACHIEVEMENTS_LOGIN_FAILED && event->token_refused && l->account_path[0]) {
        OraclesAchievementsAccount kept;
        oracles_achievements_account_read(l->account_path, &kept);
        kept.token[0] = 0;
        oracles_achievements_account_write(l->account_path, &kept);
        fprintf(stderr, "oracles: achievements: the token is forgotten; log in again\n");
    }
    if (event->kind == ORACLES_ACHIEVEMENTS_LOGGED_IN && !l->catalogue) {
        l->catalogue = oracles_achievements_catalogue_begin(l->client, event->user && event->user[0] ? event->user : l->account.user,
                                                            event->token && event->token[0] ? event->token : l->account.token, l->hash);
        if (!l->catalogue) l->state = ORACLES_ACHIEVEMENTS_LIST_FAILED;
    } else if (event->kind == ORACLES_ACHIEVEMENTS_LOGIN_FAILED && l->state == ORACLES_ACHIEVEMENTS_LIST_LOADING) {
        l->state = event->unreachable ? ORACLES_ACHIEVEMENTS_LIST_UNREACHABLE : ORACLES_ACHIEVEMENTS_LIST_FAILED;
    }
}

/* What the requests answered, the list made and its badges asked for once. */
static void badge_wanted(void *opaque, const char *name, const char *unlocked_url, const char *locked_url)
{
    OraclesAchievementsLoad *l = opaque;
    oracles_achievements_fetch_badge(l->client, l->badges_folder, &l->badges, name, unlocked_url, locked_url);
}

static void catalogue_followed(OraclesAchievementsLoad *l)
{
    if (!l->catalogue || l->state != ORACLES_ACHIEVEMENTS_LIST_LOADING) return;
    switch (oracles_achievements_catalogue_state(l->catalogue)) {
        case ORACLES_ACHIEVEMENTS_CATALOGUE_WAITING: break;
        case ORACLES_ACHIEVEMENTS_CATALOGUE_UNREACHABLE: l->state = ORACLES_ACHIEVEMENTS_LIST_UNREACHABLE; break;
        case ORACLES_ACHIEVEMENTS_CATALOGUE_FAILED: l->state = ORACLES_ACHIEVEMENTS_LIST_FAILED; break;
        case ORACLES_ACHIEVEMENTS_CATALOGUE_DONE:
            l->state = ORACLES_ACHIEVEMENTS_LIST_READY;
            oracles_achievements_catalogue_list(l->catalogue, l->badges_folder, &l->list);
            l->made = 1;
            if (l->badges_folder[0]) oracles_achievements_catalogue_badges(l->catalogue, badge_wanted, l);
            break;
    }
}

OraclesAchievementsLoad *oracles_achievements_load_begin(const OraclesAchievementsTransport *transport, const char *user_agent, const OraclesAchievementsAccount *account,
                                                         const char *account_path, const char *hash, const char *badges, double now_ms)
{
    OraclesAchievementsLoad *l = calloc(1, sizeof *l);
    if (!l) { if (transport->destroy) transport->destroy(transport->opaque); return NULL; }
    l->account = *account;
    snprintf(l->account_path, sizeof l->account_path, "%s", account_path ? account_path : "");
    snprintf(l->badges_folder, sizeof l->badges_folder, "%s", badges ? badges : "");
    snprintf(l->hash, sizeof l->hash, "%s", hash);
    l->state = ORACLES_ACHIEVEMENTS_LIST_LOADING;
    l->started_ms = now_ms;
    l->client = oracles_achievements_create(transport, user_agent, on_event, l);
    if (!l->client) {
        if (transport->destroy) transport->destroy(transport->opaque);
        free(l);
        return NULL;
    }
    oracles_achievements_login(l->client, account->user, NULL, account->token);
    return l;
}

/* All the badges asked for answered. */
static int badges_answered(const OraclesAchievementsBadges *badges)
{
    for (unsigned i = 0; i < badges->count; i++) if (!badges->jobs[i].done) return 0;
    return 1;
}

OraclesAchievementsListState oracles_achievements_load_poll(OraclesAchievementsLoad *l, double now_ms, const OraclesAchievementsList **list, int *busy)
{
    if (l->client) {
        oracles_achievements_idle(l->client);
        const OraclesAchievementsListState before = l->state;
        catalogue_followed(l);
        if (before != ORACLES_ACHIEVEMENTS_LIST_READY && l->state == ORACLES_ACHIEVEMENTS_LIST_READY) l->ready_ms = now_ms;
        if (l->state == ORACLES_ACHIEVEMENTS_LIST_LOADING && now_ms - l->started_ms >= ORACLES_ACHIEVEMENTS_LOAD_WAIT_MS) l->state = ORACLES_ACHIEVEMENTS_LIST_UNREACHABLE;
        /* The list there and its badges in, or nothing to wait for: no ping of a game that does not run. */
        const int ready = l->state == ORACLES_ACHIEVEMENTS_LIST_READY;
        if (l->state != ORACLES_ACHIEVEMENTS_LIST_LOADING
            && (!ready || badges_answered(&l->badges) || now_ms - l->ready_ms >= ORACLES_ACHIEVEMENTS_LOAD_WAIT_MS)) {
            oracles_achievements_destroy(l->client);
            l->client = NULL;
        }
    }
    *busy = l->client != NULL;
    *list = l->made ? &l->list : NULL;
    return l->state;
}

void oracles_achievements_load_end(OraclesAchievementsLoad *l)
{
    if (!l) return;
    if (l->client) oracles_achievements_destroy(l->client);   /* the transport first: no answer comes after */
    oracles_achievements_catalogue_free(l->catalogue);
    free(l);
}

int oracles_achievements_page_load(OraclesAchievementsLoad **load, int *game, OraclesHomeNav *nav, OraclesAchievementsLoadBegin begin, void *opaque, double now_ms)
{
    OraclesHomeAchievements *a = &nav->achievements;
    const int g = oracles_home_game(nav);
    const int wanted = nav->screen == ORACLES_SCREEN_ACHIEVEMENTS && a->on && a->user[0] && !a->login
                       && (g == ORACLES_HOME_GAME_AGES || g == ORACLES_HOME_GAME_SEASONS) && nav->games[g].rom == ORACLES_ROM_ORIGINAL;
    if (!wanted || (*load && *game != g)) {
        const int shown = a->list_state != ORACLES_ACHIEVEMENTS_LIST_NONE;
        a->list = NULL;   /* before the load holding it goes */
        a->detail = 0;    /* the next list opens closed */
        a->list_state = ORACLES_ACHIEVEMENTS_LIST_NONE;
        a->list_busy = 0;
        oracles_achievements_load_end(*load);
        *load = NULL;
        return shown;
    }
    if (a->list_state == ORACLES_ACHIEVEMENTS_LIST_NONE) {
        *game = g;
        *load = begin(opaque, nav, g);
        a->list_state = *load ? ORACLES_ACHIEVEMENTS_LIST_LOADING : ORACLES_ACHIEVEMENTS_LIST_FAILED;
        a->list_busy = *load != NULL;
        return 1;
    }
    if (!*load) return 0;
    const OraclesAchievementsList *list = NULL;
    int busy = 0;
    const OraclesAchievementsListState state = oracles_achievements_load_poll(*load, now_ms, &list, &busy);
    const int moved = (int)state != a->list_state || list != a->list || busy != a->list_busy;
    a->list_state = (int)state;
    a->list = list;
    a->list_busy = busy;
    return moved;
}
