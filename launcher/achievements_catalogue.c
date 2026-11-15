/* A game's achievements read with no session (achievements_catalogue.h). */
#include "achievements_catalogue.h"

#include "rc_api_runtime.h"
#include "rc_api_user.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct OraclesAchievementsCatalogue {
    OraclesAchievements *client;
    char user[128], token[128];
    OraclesAchievementsCatalogueState state;
    rc_api_fetch_game_sets_response_t game;
    int has_game;
    uint32_t game_id;
    uint32_t *unlocked;       /* the achievements' ids, as each answer lists them */
    size_t unlocked_count;
    int hardcore_asked;
};

/* Sent, its strings the transport's to copy. */
static void dispatch(OraclesAchievementsCatalogue *c, rc_api_request_t *request, OraclesAchievementsAnswer answer)
{
    oracles_achievements_send(c->client, request->url, request->post_data, request->content_type, answer, c);
    rc_api_destroy_request(request);
}

/* The host the client's requests go to, the tests' (oracles_achievements_set_host) when it has one. */
static rc_api_host_t host_of(const OraclesAchievementsCatalogue *c)
{
    const rc_api_host_t host = { oracles_achievements_host(c->client), NULL };
    return host;
}

/* What an answer that is not the wanted one leaves: no server, or an error of its proxy, can come back; the rest will not. */
static void failed(OraclesAchievementsCatalogue *c, int http_status)
{
    c->state = http_status == 0 || http_status >= 500 ? ORACLES_ACHIEVEMENTS_CATALOGUE_UNREACHABLE : ORACLES_ACHIEVEMENTS_CATALOGUE_FAILED;
}

static void unlocks_answered(void *opaque, int http_status, const uint8_t *body, size_t length);

/* The account's unlocks of one kind: casual mode shows what either kind earned, as a session's answer does. */
static void ask_unlocks(OraclesAchievementsCatalogue *c, int hardcore)
{
    rc_api_fetch_user_unlocks_request_t params;
    memset(&params, 0, sizeof params);
    params.username = c->user;
    params.api_token = c->token;
    params.game_id = c->game_id;
    params.hardcore = (uint32_t)hardcore;
    c->hardcore_asked = hardcore;
    rc_api_request_t request;
    const rc_api_host_t host = host_of(c);
    if (rc_api_init_fetch_user_unlocks_request_hosted(&request, &params, &host) != RC_OK) { c->state = ORACLES_ACHIEVEMENTS_CATALOGUE_FAILED; rc_api_destroy_request(&request); return; }
    dispatch(c, &request, unlocks_answered);
}

static void unlocks_answered(void *opaque, int http_status, const uint8_t *body, size_t length)
{
    OraclesAchievementsCatalogue *c = opaque;
    if (http_status < 0) return;   /* given up */
    if (http_status == 0) { failed(c, 0); return; }
    const rc_api_server_response_t answer = { (const char *)body, length, http_status };
    rc_api_fetch_user_unlocks_response_t unlocks;
    const int result = rc_api_process_fetch_user_unlocks_server_response(&unlocks, &answer);
    if (result != RC_OK || !unlocks.response.succeeded) failed(c, http_status);
    else {
        uint32_t *grown = realloc(c->unlocked, (c->unlocked_count + unlocks.num_achievement_ids + 1u) * sizeof *grown);
        if (grown) {
            c->unlocked = grown;
            memcpy(c->unlocked + c->unlocked_count, unlocks.achievement_ids, unlocks.num_achievement_ids * sizeof *grown);
            c->unlocked_count += unlocks.num_achievement_ids;
            if (c->hardcore_asked) c->state = ORACLES_ACHIEVEMENTS_CATALOGUE_DONE;
            else ask_unlocks(c, 1);
        } else c->state = ORACLES_ACHIEVEMENTS_CATALOGUE_FAILED;
    }
    rc_api_destroy_fetch_user_unlocks_response(&unlocks);
}

static void game_answered(void *opaque, int http_status, const uint8_t *body, size_t length)
{
    OraclesAchievementsCatalogue *c = opaque;
    if (http_status < 0) return;   /* given up */
    if (http_status == 0) { failed(c, 0); return; }
    const rc_api_server_response_t answer = { (const char *)body, length, http_status };
    const int result = rc_api_process_fetch_game_sets_server_response(&c->game, &answer);
    c->has_game = 1;
    if (result != RC_OK || !c->game.response.succeeded || !c->game.id) { failed(c, http_status); return; }
    c->game_id = c->game.session_game_id ? c->game.session_game_id : c->game.id;
    ask_unlocks(c, 0);
}

OraclesAchievementsCatalogue *oracles_achievements_catalogue_begin(OraclesAchievements *a, const char *user, const char *token, const char *hash)
{
    OraclesAchievementsCatalogue *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->client = a;
    snprintf(c->user, sizeof c->user, "%s", user);
    snprintf(c->token, sizeof c->token, "%s", token);
    rc_api_fetch_game_sets_request_t params;
    memset(&params, 0, sizeof params);
    params.username = c->user;
    params.api_token = c->token;
    params.game_hash = hash;
    rc_api_request_t request;
    const rc_api_host_t host = host_of(c);
    if (rc_api_init_fetch_game_sets_request_hosted(&request, &params, &host) != RC_OK) { c->state = ORACLES_ACHIEVEMENTS_CATALOGUE_FAILED; rc_api_destroy_request(&request); return c; }
    dispatch(c, &request, game_answered);
    return c;
}

OraclesAchievementsCatalogueState oracles_achievements_catalogue_state(const OraclesAchievementsCatalogue *c) { return c->state; }

static int is_unlocked(const OraclesAchievementsCatalogue *c, uint32_t id)
{
    for (size_t i = 0; i < c->unlocked_count; i++) if (c->unlocked[i] == id) return 1;
    return 0;
}

void oracles_achievements_catalogue_each(const OraclesAchievementsCatalogue *c, OraclesAchievementsEach each, void *opaque)
{
    if (c->state != ORACLES_ACHIEVEMENTS_CATALOGUE_DONE) return;
    for (int locked = 0; locked < 2; locked++)
        for (uint32_t s = 0; s < c->game.num_sets; s++) {
            const rc_api_achievement_set_definition_t *set = &c->game.sets[s];
            for (uint32_t i = 0; i < set->num_achievements; i++) {
                const rc_api_achievement_definition_t *d = &set->achievements[i];
                if (d->category != RC_ACHIEVEMENT_CATEGORY_CORE || d->id >= ORACLES_ACHIEVEMENTS_WARNING_ID || is_unlocked(c, d->id) == locked) continue;
                OraclesAchievementsEntry e;
                memset(&e, 0, sizeof e);
                e.title = d->title ? d->title : "";
                e.description = d->description ? d->description : "";
                e.badge = d->badge_name;
                e.progress = "";
                e.id = d->id;
                e.points = d->points;
                e.unlocked = !locked;
                e.rarity = d->rarity;
                e.type = d->type;
                each(opaque, &e);
            }
        }
}

void oracles_achievements_catalogue_badges(const OraclesAchievementsCatalogue *c, OraclesAchievementsBadge badge, void *opaque)
{
    if (c->state != ORACLES_ACHIEVEMENTS_CATALOGUE_DONE) return;
    for (uint32_t s = 0; s < c->game.num_sets; s++) {
        const rc_api_achievement_set_definition_t *set = &c->game.sets[s];
        for (uint32_t i = 0; i < set->num_achievements; i++) {
            const rc_api_achievement_definition_t *d = &set->achievements[i];
            if (d->category == RC_ACHIEVEMENT_CATEGORY_CORE && d->id < ORACLES_ACHIEVEMENTS_WARNING_ID && d->badge_name && d->badge_name[0] && d->badge_url && d->badge_locked_url)
                badge(opaque, d->badge_name, d->badge_url, d->badge_locked_url);
        }
    }
}

void oracles_achievements_catalogue_free(OraclesAchievementsCatalogue *c)
{
    if (!c) return;
    if (c->has_game) rc_api_destroy_fetch_game_sets_response(&c->game);
    free(c->unlocked);
    free(c);
}
