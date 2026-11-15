/* RetroAchievements through rc_client (achievements.h). */
#include "achievements.h"

#include "achievements_files.h"

#include "rc_api_runtime.h"
#include "rc_client.h"
#include "rc_consoles.h"
#include "rc_hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- the memory ----------------------------------------------------------------------- */

void oracles_achievements_memory_of(OraclesCore *core, OraclesAchievementsMemory *memory)
{
    memset(memory, 0, sizeof *memory);
    memory->core = core;
    if (!core) return;
    size_t size = 0;
    memory->rom = oracles_core_memory(core, ORACLES_CORE_ROM, &memory->rom_size, &memory->rom_bank);
    memory->wram = oracles_core_memory(core, ORACLES_CORE_WRAM, &size, NULL);
    if (size < 0x8000u) memory->wram = NULL;
    memory->vram = oracles_core_memory(core, ORACLES_CORE_VRAM, &size, &memory->vram_bank);
    if (size < 0x4000u) memory->vram = NULL;
    memory->oam = oracles_core_memory(core, ORACLES_CORE_OAM, &size, NULL);
    if (size < 0xa0u) memory->oam = NULL;
    memory->hram = oracles_core_memory(core, ORACLES_CORE_HRAM, &size, NULL);
    if (size < 0x7fu) memory->hram = NULL;
    memory->cart_ram = oracles_core_memory(core, ORACLES_CORE_CART_RAM, &memory->cart_ram_size, NULL);
}

static int read_byte(const OraclesAchievementsMemory *m, uint32_t address, uint8_t *out)
{
    const uint8_t *from = NULL;
    size_t offset = 0, size = 0;
    if (address < 0x4000u) { from = m->rom; offset = address; size = m->rom_size; }
    else if (address < 0x8000u) { from = m->rom; offset = (size_t)m->rom_bank * 0x4000u + (address - 0x4000u); size = m->rom_size; }
    else if (address < 0xa000u) { from = m->vram; offset = (size_t)(m->vram_bank & 1u) * 0x2000u + (address - 0x8000u); size = 0x4000u; }
    else if (address < 0xc000u) { from = m->cart_ram; offset = address - 0xa000u; size = m->cart_ram_size; }
    else if (address < 0xe000u) { from = m->wram; offset = address - 0xc000u; size = 0x8000u; }
    else if (address < 0xfe00u) { from = m->wram; offset = address - 0xe000u; size = 0x8000u; }   /* the echo of $C000 */
    else if (address < 0xfea0u) { from = m->oam; offset = address - 0xfe00u; size = 0xa0u; }
    else if (address < 0xff00u) { *out = 0; return 1; }                                           /* unused */
    else if (address < 0xff80u || address == 0xffffu) { *out = m->core ? oracles_core_peek(m->core, (uint16_t)address) : 0; return 1; }
    else if (address < 0xffffu) { from = m->hram; offset = address - 0xff80u; size = 0x7fu; }
    else if (address < 0x16000u) { from = m->wram; offset = address - 0x10000u + 0x2000u; size = 0x8000u; }
    else if (address < 0x34000u) { from = m->cart_ram; offset = address - 0x16000u + 0x2000u; size = m->cart_ram_size; }
    if (!from || offset >= size) return 0;
    *out = from[offset];
    return 1;
}

uint32_t oracles_achievements_read(const OraclesAchievementsMemory *memory, uint32_t address, uint8_t *buffer, uint32_t count)
{
    uint32_t done = 0;
    while (done < count && read_byte(memory, address + done, buffer + done)) done++;
    return done;
}

uint32_t oracles_achievements_address(uint8_t wram_bank, uint16_t address)
{
    if (address >= 0xd000u && address < 0xe000u && wram_bank >= 2u && wram_bank <= 7u)
        return 0x10000u + (uint32_t)(wram_bank - 2u) * 0x1000u + (address - 0xd000u);
    return address;
}

void oracles_achievements_hash(const uint8_t *rom, size_t size, char hash[33])
{
    if (!rc_hash_generate_from_buffer(hash, RC_CONSOLE_GAMEBOY_COLOR, rom, size)) hash[0] = 0;
}

void oracles_achievements_user_agent(const char *version, const char *platform, const char *core_version, char *out, size_t capacity)
{
    /* "sameboy-1.0.3" is the core's product and version: "sameboy/1.0.3". */
    char core[64];
    snprintf(core, sizeof core, "%s", core_version ? core_version : "");
    char *dash = strchr(core, '-');
    if (dash) *dash = '/';
    char clause[64] = "";
    rc_client_get_user_agent_clause(NULL, clause, sizeof clause);
    snprintf(out, capacity, "%s/%s (%s)%s%s%s%s", ORACLES_ACHIEVEMENTS_PRODUCT, version, platform, core[0] ? " " : "", core, clause[0] ? " " : "", clause);
}

/* ---- the client --------------------------------------------------------------------------- */

struct OraclesAchievements {
    rc_client_t *client;
    OraclesAchievementsTransport transport;
    char user_agent[160];
    OraclesAchievementsNotify notify;
    void *notify_opaque;
    OraclesAchievementsMemory memory;
    OraclesCore *core;
    int watching;
    uint32_t frames;   /* evaluated since the game was asked for */
    char suspended[192];   /* why the achievements are no longer evaluated, or empty */
    char host[128];        /* the server, the tests' own; empty: RetroAchievements' */
    /* The unlocks whose award was settled before their event was raised (a transport that answers in send), and the
     * unlocks raised: two rings, the oldest written over. */
    struct { uint32_t id; int delivered; } settled[16];
    uint32_t raised[16];
    unsigned settled_next, raised_next;
};

static int was_raised(const OraclesAchievements *a, uint32_t id)
{
    for (unsigned i = 0; i < 16; i++) if (a->raised[i] == id) return 1;
    return 0;
}

static void tell(OraclesAchievements *a, const OraclesAchievementsEvent *event)
{
    if (a->notify) a->notify(a->notify_opaque, event);
}

static uint32_t read_memory(uint32_t address, uint8_t *buffer, uint32_t count, rc_client_t *client)
{
    const OraclesAchievements *a = rc_client_get_userdata(client);
    return oracles_achievements_read(&a->memory, address, buffer, count);
}

/* A request sent: rc_client's, whose callback has its answer, or a stored unlock sent again (no callback). */
typedef struct pending {
    OraclesAchievements *a;
    rc_client_server_callback_t callback;
    void *data;
    uint32_t award;   /* the achievement of an awardachievement, else 0 */
} pending;

/* rc_client's rule (rc_client_should_retry): no answer, the server or its proxy overloaded or down, or an empty body. */
static int retried(const rc_api_server_response_t *response)
{
    const int s = response->http_status_code;
    if (s == RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR || s == 429 || (s >= 502 && s <= 504) || (s >= 521 && s <= 525)) return 1;
    if (s == RC_API_SERVER_RESPONSE_CLIENT_ERROR) return 0;
    return !response->body || !response->body_length || !response->body[0];
}

/* An unlock's award answered for good, by the server itself in its JSON: earned (an achievement already earned counts),
 * or refused (for the token: HTTP 401 or 403).  Anything else (a page of a captive portal, an error of the server or of
 * a proxy, an answer cut) settles nothing: the unlock is kept and sent again. */
static void award_settled(OraclesAchievements *a, uint32_t id, const rc_api_server_response_t *response, int by_client)
{
    rc_api_award_achievement_response_t award;
    memset(&award, 0, sizeof award);
    const int json = rc_api_process_award_achievement_server_response(&award, response) == RC_OK;
    const int s = response->http_status_code;
    if (json && s >= 200 && s < 500) {
        OraclesAchievementsEvent e;
        memset(&e, 0, sizeof e);
        e.kind = ORACLES_ACHIEVEMENTS_AWARD_SETTLED;
        e.id = id;
        e.delivered = award.response.succeeded;
        e.token_refused = !e.delivered && (s == 401 || s == 403);
        e.text = award.response.error_message ? award.response.error_message : "";
        if (by_client && !was_raised(a, id)) {
            a->settled[a->settled_next % 16].id = id;
            a->settled[a->settled_next % 16].delivered = e.delivered || !e.token_refused;   /* nothing left to send */
            a->settled_next++;
        }
        tell(a, &e);
    }
    rc_api_destroy_award_achievement_response(&award);
}

static void answered(void *opaque, int http_status, const uint8_t *body, size_t length)
{
    pending *p = opaque;
    rc_api_server_response_t response;
    memset(&response, 0, sizeof response);
    response.body = (const char *)body;
    response.body_length = body ? length : 0;
    /* 0: no server answered, which rc_client tries again; -1: the transport gave the request up (the session ends). */
    response.http_status_code = http_status > 0 ? http_status : http_status < 0 ? RC_API_SERVER_RESPONSE_CLIENT_ERROR
                                                                              : RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;
    if (p->callback) p->callback(&response, p->data);
    if (p->award && !retried(&response)) award_settled(p->a, p->award, &response, p->callback != NULL);
    free(p);
}

/* The achievement of a form's a= when its r= is awardachievement, else 0. */
static uint32_t award_of(const char *post_data)
{
    if (!post_data || strncmp(post_data, "r=awardachievement&", 19) != 0) return 0;
    const char *at = strstr(post_data, "&a=");
    return at ? (uint32_t)strtoul(at + 3, NULL, 10) : 0;
}

static void send_request(OraclesAchievements *a, const rc_api_request_t *request, rc_client_server_callback_t callback, void *data)
{
    pending *p = malloc(sizeof *p);
    if (!p) {
        const rc_api_server_response_t none = { NULL, 0, RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR };
        if (callback) callback(&none, data);
        return;
    }
    p->a = a;
    p->callback = callback;
    p->data = data;
    p->award = award_of(request->post_data);
    a->transport.send(a->transport.opaque, request->url, request->post_data, request->content_type, a->user_agent, answered, p);
}

static void server_call(const rc_api_request_t *request, rc_client_server_callback_t callback, void *data, rc_client_t *client)
{
    send_request(rc_client_get_userdata(client), request, callback, data);
}

static void log_message(const char *message, const rc_client_t *client)
{
    (void)client;
    fprintf(stderr, "oracles: achievements: rcheevos: %s\n", message);
}

static void on_event(const rc_client_event_t *event, rc_client_t *client)
{
    OraclesAchievements *a = rc_client_get_userdata(client);
    OraclesAchievementsEvent e;
    memset(&e, 0, sizeof e);
    switch (event->type) {
        case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
            e.kind = ORACLES_ACHIEVEMENTS_UNLOCKED;
            e.text = event->achievement->title;
            e.badge = event->achievement->badge_name;
            e.id = event->achievement->id;
            e.points = event->achievement->points;
            for (unsigned i = 0; i < 16; i++)
                if (a->settled[i].id == e.id) { e.delivered = a->settled[i].delivered; a->settled[i].id = 0; break; }
            a->raised[a->raised_next++ % 16] = e.id;
            fprintf(stderr, "oracles: achievements: unlocked \"%s\" (%u points) at the frame %u evaluated\n", e.text, e.points, a->frames);
            break;
        case RC_CLIENT_EVENT_GAME_COMPLETED: {
            const rc_client_game_t *game = rc_client_get_game_info(client);
            e.kind = ORACLES_ACHIEVEMENTS_GAME_COMPLETED;
            e.text = game ? game->title : "";
            fprintf(stderr, "oracles: achievements: every achievement of %s earned\n", e.text);
            break;
        }
        case RC_CLIENT_EVENT_SERVER_ERROR:
            e.kind = ORACLES_ACHIEVEMENTS_SERVER_ERROR;
            e.text = event->server_error->error_message;
            fprintf(stderr, "oracles: achievements: the server refused %s: %s\n", event->server_error->api, e.text);
            break;
        case RC_CLIENT_EVENT_DISCONNECTED:
            e.kind = ORACLES_ACHIEVEMENTS_OFFLINE;
            fprintf(stderr, "oracles: achievements: an unlock could not be sent; it is sent again when the server answers\n");
            break;
        case RC_CLIENT_EVENT_RECONNECTED:
            fprintf(stderr, "oracles: achievements: the unlocks waiting are sent\n");
            return;
        default: return;   /* the indicators, the leaderboards' trackers: no interface yet */
    }
    tell(a, &e);
}

OraclesAchievements *oracles_achievements_create(const OraclesAchievementsTransport *transport, const char *user_agent,
                                                 OraclesAchievementsNotify notify, void *notify_opaque)
{
    OraclesAchievements *a = calloc(1, sizeof *a);
    if (!a) return NULL;
    a->transport = *transport;
    snprintf(a->user_agent, sizeof a->user_agent, "%s", user_agent);
    a->notify = notify;
    a->notify_opaque = notify_opaque;
    a->client = rc_client_create(read_memory, server_call);
    if (!a->client) { free(a); return NULL; }
    rc_client_set_userdata(a->client, a);
    rc_client_enable_logging(a->client, RC_CLIENT_LOG_LEVEL_WARN, log_message);
    rc_client_set_event_handler(a->client, on_event);
    /* Casual mode: hardcore needs RetroAchievements' approval of the application, which it does not have. */
    rc_client_set_hardcore_enabled(a->client, 0);
    /* The memory is read between frames, on the game's thread, never while the core runs. */
    rc_client_set_allow_background_memory_reads(a->client, 0);
    return a;
}

void oracles_achievements_destroy(OraclesAchievements *a)
{
    if (!a) return;
    /* The transport first: an answer it still holds is dropped, never called on a client gone. */
    if (a->transport.destroy) a->transport.destroy(a->transport.opaque);
    a->transport.poll = NULL;
    rc_client_destroy(a->client);
    free(a);
}

static void logged_in(int result, const char *message, rc_client_t *client, void *userdata)
{
    OraclesAchievements *a = userdata;
    OraclesAchievementsEvent e;
    memset(&e, 0, sizeof e);
    if (result == RC_OK) {
        const rc_client_user_t *user = rc_client_get_user_info(client);
        e.kind = ORACLES_ACHIEVEMENTS_LOGGED_IN;
        e.text = user ? user->display_name : "";
        e.user = user ? user->username : "";
        e.token = user ? user->token : "";
        fprintf(stderr, "oracles: achievements: logged in as %s (casual mode)\n", e.text);
    } else {
        e.kind = ORACLES_ACHIEVEMENTS_LOGIN_FAILED;
        e.text = message && message[0] ? message : rc_error_str(result);
        e.token_refused = result == RC_INVALID_CREDENTIALS || result == RC_EXPIRED_TOKEN || result == RC_ACCESS_DENIED;
        e.unreachable = result == RC_NO_RESPONSE;
        fprintf(stderr, "oracles: achievements: login refused: %s\n", e.text);
    }
    tell(a, &e);
}

void oracles_achievements_login(OraclesAchievements *a, const char *user, const char *password, const char *token)
{
    if (token) rc_client_begin_login_with_token(a->client, user, token, logged_in, a);
    else rc_client_begin_login_with_password(a->client, user, password, logged_in, a);
}

static void game_loaded(int result, const char *message, rc_client_t *client, void *userdata)
{
    OraclesAchievements *a = userdata;
    OraclesAchievementsEvent e;
    memset(&e, 0, sizeof e);
    if (result == RC_OK) {
        const rc_client_game_t *game = rc_client_get_game_info(client);
        rc_client_user_game_summary_t summary;
        rc_client_get_user_game_summary(client, &summary);
        a->watching = 1;
        e.kind = ORACLES_ACHIEVEMENTS_GAME_LOADED;
        e.text = game ? game->title : "";
        e.count = summary.num_core_achievements;
        e.unlocked = summary.num_unlocked_achievements;
        fprintf(stderr, "oracles: achievements: %s, %u of %u earned (casual mode)\n", e.text, e.unlocked, e.count);
    } else {
        e.kind = ORACLES_ACHIEVEMENTS_GAME_FAILED;
        e.text = message && message[0] ? message : rc_error_str(result);
        e.unreachable = result == RC_NO_RESPONSE;
        fprintf(stderr, "oracles: achievements: none for this game: %s\n", e.text);
    }
    tell(a, &e);
}

void oracles_achievements_load_game(OraclesAchievements *a, const char hash[33], OraclesCore *memory)
{
    a->core = memory;
    oracles_achievements_memory_of(memory, &a->memory);
    rc_client_begin_load_game(a->client, hash, game_loaded, a);
}

OraclesAchievementsMemory *oracles_achievements_memory(OraclesAchievements *a) { return &a->memory; }

void oracles_achievements_frame(OraclesAchievements *a)
{
    if (a->transport.poll) a->transport.poll(a->transport.opaque);
    if (a->suspended[0]) { rc_client_idle(a->client); return; }   /* the unlocks waiting are still sent */
    if (a->core) oracles_achievements_memory_of(a->core, &a->memory);   /* the banks this frame left mapped */
    a->frames++;
    rc_client_do_frame(a->client);
}

void oracles_achievements_suspend(OraclesAchievements *a, const char *why)
{
    if (a->suspended[0]) return;
    snprintf(a->suspended, sizeof a->suspended, "%s", why);
    fprintf(stderr, "oracles: achievements: suspended for this game: %s\n", why);
}

const char *oracles_achievements_suspension(const OraclesAchievements *a) { return a->suspended[0] ? a->suspended : NULL; }

void oracles_achievements_resend(OraclesAchievements *a, const char *user, const char *token, const char *hash, uint32_t id, uint32_t seconds)
{
    rc_api_award_achievement_request_t params;
    memset(&params, 0, sizeof params);
    params.username = user;
    params.api_token = token;
    params.achievement_id = id;
    params.hardcore = 0;
    params.game_hash = hash;
    params.seconds_since_unlock = seconds ? seconds : 1u;   /* earned before now: never "now" */
    rc_api_request_t request;
    const rc_api_host_t host = { a->host[0] ? a->host : NULL, NULL };
    if (rc_api_init_award_achievement_request_hosted(&request, &params, &host) != RC_OK) return;
    fprintf(stderr, "oracles: achievements: sending again the unlock of achievement %u, earned %u s ago\n", id, params.seconds_since_unlock);
    send_request(a, &request, NULL, NULL);
    rc_api_destroy_request(&request);
}

void oracles_achievements_set_host(OraclesAchievements *a, const char *host)
{
    snprintf(a->host, sizeof a->host, "%s", host);
    rc_client_set_host(a->client, host);
}

void oracles_achievements_badges(OraclesAchievements *a, OraclesAchievementsBadge badge, void *opaque)
{
    rc_client_achievement_list_t *list = rc_client_create_achievement_list(a->client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
                                                                           RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
    if (!list) return;
    for (uint32_t b = 0; b < list->num_buckets; b++)
        for (uint32_t i = 0; i < list->buckets[b].num_achievements; i++) {
            const rc_client_achievement_t *achievement = list->buckets[b].achievements[i];
            char unlocked[256], locked[256];
            if (!achievement->badge_name[0]
                || rc_client_achievement_get_image_url(achievement, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED, unlocked, sizeof unlocked) != RC_OK
                || rc_client_achievement_get_image_url(achievement, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE, locked, sizeof locked) != RC_OK) continue;
            badge(opaque, achievement->badge_name, unlocked, locked);
        }
    rc_client_destroy_achievement_list(list);
}

typedef struct saved {
    char path[1024];
    OraclesAchievementsAnswer answer;
    void *answer_opaque;
} saved;

static void save_answered(void *opaque, int http_status, const uint8_t *body, size_t length)
{
    saved *s = opaque;
    if (http_status == 200 && !oracles_achievements_write_image(s->path, body, length)) http_status = 0;
    s->answer(s->answer_opaque, http_status, NULL, 0);
    free(s);
}

void oracles_achievements_save(OraclesAchievements *a, const char *url, const char *path, OraclesAchievementsAnswer answer, void *answer_opaque)
{
    if (a->transport.save) { a->transport.save(a->transport.opaque, url, path, a->user_agent, answer, answer_opaque); return; }
    if (oracles_achievements_file_exists(path)) { answer(answer_opaque, 200, NULL, 0); return; }
    saved *s = malloc(sizeof *s);
    if (!s) { answer(answer_opaque, 0, NULL, 0); return; }
    snprintf(s->path, sizeof s->path, "%s", path);
    s->answer = answer;
    s->answer_opaque = answer_opaque;
    a->transport.send(a->transport.opaque, url, NULL, NULL, a->user_agent, save_answered, s);
}

void oracles_achievements_idle(OraclesAchievements *a)
{
    if (a->transport.poll) a->transport.poll(a->transport.opaque);
    rc_client_idle(a->client);
}

void oracles_achievements_reset(OraclesAchievements *a) { rc_client_reset(a->client); }

int oracles_achievements_watching(const OraclesAchievements *a) { return a->watching; }

/* Where an achievement goes in the list: unlocked, in progress, locked. */
static int list_group(const rc_client_achievement_t *c)
{
    if (c->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE) return 0;
    return c->measured_progress[0] ? 1 : 2;
}

void oracles_achievements_each(OraclesAchievements *a, OraclesAchievementsEach each, void *opaque)
{
    /* rcheevos' buckets put the unlocked last: the unlocked taken first (the latest, then the set's order), then those in
     * progress (the nearest first), then the locked. */
    rc_client_achievement_list_t *list = rc_client_create_achievement_list(a->client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE,
                                                                           RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
    if (!list) return;
    for (int group = 0; group < 3; group++) {
        for (uint32_t b = 0; b < list->num_buckets; b++)
            for (uint32_t i = 0; i < list->buckets[b].num_achievements; i++) {
                const rc_client_achievement_t *c = list->buckets[b].achievements[i];
                if (list_group(c) != group) continue;
                OraclesAchievementsEntry e;
                e.title = c->title ? c->title : "";
                e.description = c->description ? c->description : "";
                e.badge = c->badge_name;
                e.progress = c->measured_progress;
                e.id = c->id;
                e.points = c->points;
                e.unlocked = (c->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE) != 0;
                e.unlock_time = (int64_t)c->unlock_time;
                e.percent = c->measured_percent / 100.0f;
                each(opaque, &e);
            }
    }
    rc_client_destroy_achievement_list(list);
}

void oracles_achievements_progress(OraclesAchievements *a, unsigned *count, unsigned *unlocked, unsigned *points, unsigned *points_unlocked)
{
    rc_client_user_game_summary_t summary;
    memset(&summary, 0, sizeof summary);
    rc_client_get_user_game_summary(a->client, &summary);
    *count = summary.num_core_achievements;
    *unlocked = summary.num_unlocked_achievements;
    *points = summary.points_core;
    *points_unlocked = summary.points_unlocked;
}
