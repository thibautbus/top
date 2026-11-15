/* RetroAchievements without a server (achievements.h): the Game Boy Color map over the core's regions, the game's
 * hash, the User-Agent, and a session against a transport answering canned replies: the login and its token, the game
 * and its two achievements on Ages' own addresses, an achievement earned when the memory says so and its award asked
 * with the right fields, a token refused.  The pause's list (its order, progress and dates) and an unlock no server
 * acknowledges told offline; the toasts a session's events tell (achievements_screens.h), the server unreachable as
 * the game starts from the developer's offline answers; the home screen's login on those answers
 * (achievements_account.h).  With --bench, the cost of a frame's evaluation of a set the size of Ages' (55
 * achievements). */
#include "achievements.h"
#include "achievements_account.h"
#include "achievements_badges.h"
#include "achievements_load.h"
#include "achievements_pending.h"
#include "achievements_screens.h"
#include "achievements_transport.h"

#include "guest_tables.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

/* ---- a memory of buffers ------------------------------------------------------------------------ */

static uint8_t rom[0x10000], wram[0x8000], vram[0x4000], oam[0xa0], hram[0x7f], cart_ram[0x2000];

static void memory_of_buffers(OraclesAchievementsMemory *m)
{
    memset(m, 0, sizeof *m);
    m->rom = rom; m->rom_size = sizeof rom; m->rom_bank = 2;
    m->wram = wram; m->vram = vram; m->vram_bank = 1; m->oam = oam; m->hram = hram;
    m->cart_ram = cart_ram; m->cart_ram_size = sizeof cart_ram;
}

static uint8_t at(const OraclesAchievementsMemory *m, uint32_t address, uint32_t *count)
{
    uint8_t b = 0xee;
    *count = oracles_achievements_read(m, address, &b, 1);
    return b;
}

static void test_map(void)
{
    OraclesAchievementsMemory m;
    memory_of_buffers(&m);
    for (size_t i = 0; i < sizeof wram; i++) wram[i] = (uint8_t)(i >> 12);   /* each byte its bank */
    rom[0x0150] = 0x11; rom[2 * 0x4000 + 0x10] = 0x22;
    vram[0x2000 + 5] = 0x33; oam[0x9f] = 0x44; hram[0x7e] = 0x55; cart_ram[0x1fff] = 0x66;
    uint32_t n = 0;
    CHECK(at(&m, 0x0150, &n) == 0x11 && n == 1);
    CHECK(at(&m, 0x4010, &n) == 0x22 && n == 1);        /* the bank mapped at $4000 */
    CHECK(at(&m, 0x8005, &n) == 0x33 && n == 1);        /* the VRAM bank VBK maps */
    CHECK(at(&m, 0xbfff, &n) == 0x66 && n == 1);
    CHECK(at(&m, 0xc000, &n) == 0 && n == 1);
    CHECK(at(&m, 0xd000, &n) == 1 && n == 1);           /* bank 1, whatever SVBK maps */
    CHECK(at(&m, 0xf000, &n) == 1 && n == 1);           /* the echo of $D000 */
    CHECK(at(&m, 0xfe9f, &n) == 0x44 && n == 1);
    CHECK(at(&m, 0xfea0, &n) == 0 && n == 1);
    CHECK(at(&m, 0xff00, &n) == 0 && n == 1);           /* no core: the registers read 0 */
    CHECK(at(&m, 0xfffe, &n) == 0x55 && n == 1);
    for (uint32_t bank = 2; bank <= 7; bank++) CHECK(at(&m, 0x10000u + (bank - 2u) * 0x1000u, &n) == bank && n == 1);
    at(&m, 0x16000, &n);
    CHECK(n == 0);                                       /* the cartridge RAM's bank 1: an 8 KiB cartridge has none */
    at(&m, 0x34000, &n);
    CHECK(n == 0);
    uint8_t four[4];
    CHECK(oracles_achievements_read(&m, 0xbffe, four, 4) == 4 && four[1] == 0x66 && four[2] == 0);   /* across regions */
    CHECK(oracles_achievements_address(5, 0xd123) == 0x13123u);
    CHECK(oracles_achievements_address(1, 0xd123) == 0xd123u);
    CHECK(oracles_achievements_address(0, 0xc456) == 0xc456u);
}

static void test_identity(void)
{
    static uint8_t zeros[1024];
    char hash[33];
    oracles_achievements_hash(zeros, sizeof zeros, hash);
    CHECK(!strcmp(hash, "0f343b0931126a20f133d67c2b018a3b"));   /* the MD5 of the whole image */
    char agent[160];
    oracles_achievements_user_agent("1.5.0", "Linux", "sameboy-1.0.3", agent, sizeof agent);
    CHECK(!strcmp(agent, "the-oracles-project/1.5.0 (Linux) sameboy/1.0.3 rcheevos/12.5"));
    oracles_achievements_user_agent("1.5.0", "Android", NULL, agent, sizeof agent);
    CHECK(!strcmp(agent, "the-oracles-project/1.5.0 (Android) rcheevos/12.5"));
}

/* ---- a server of canned replies ---------------------------------------------------------------------- */

#define REQUESTS 32

typedef struct server {
    char posted[REQUESTS][1024];
    char agents[REQUESTS][160];
    unsigned count;
    const char *login, *game, *session, *award;   /* award: a format of the achievement's id and those left */
    const char *unlocks, *hardcore_unlocks;       /* the unlocks' answers, casual and hardcore */
    unsigned award_left, awarded;
    int images;   /* the badges' images answered */
    unsigned images_asked;
    const char *award_page;   /* an award answered with this page and award_status in place of the server's JSON */
    int award_status;
    OraclesAchievementsEvent events[16];
    char texts[16][128], token[64], user[64];
    unsigned event_count;
} server;

static int field_is(const char *fields, const char *name, const char *value)
{
    char wanted[256];
    snprintf(wanted, sizeof wanted, "%s=%s", name, value);
    const size_t length = strlen(wanted);
    for (const char *p = fields; p; p = strchr(p, '&') ? strchr(p, '&') + 1 : NULL)
        if (!strncmp(p, wanted, length) && (p[length] == '&' || p[length] == 0)) return 1;
    return 0;
}

static void serve(void *opaque, const char *url, const char *post_data, const char *content_type, const char *user_agent,
                 OraclesAchievementsAnswer answer, void *answer_opaque)
{
    server *s = opaque;
    (void)content_type;
    if (s->count < REQUESTS) {
        snprintf(s->posted[s->count], sizeof s->posted[0], "%s", post_data ? post_data : "");
        snprintf(s->agents[s->count], sizeof s->agents[0], "%s", user_agent);
        s->count++;
    }
    static const uint8_t png[12] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n', 0, 0, 0, 0 };
    if (!post_data) {   /* a badge's image */
        s->images_asked++;
        if (s->images && strstr(url, "/Badge/")) answer(answer_opaque, 200, png, sizeof png);
        else answer(answer_opaque, 0, NULL, 0);
        return;
    }
    if (s->award_page && field_is(post_data, "r", "awardachievement")) {
        answer(answer_opaque, s->award_status, (const uint8_t *)s->award_page, strlen(s->award_page));
        return;
    }
    const char *reply = !post_data ? NULL
                      : field_is(post_data, "r", "login2") ? s->login
                      : field_is(post_data, "r", "achievementsets") ? s->game
                      : field_is(post_data, "r", "startsession") ? s->session
                      : field_is(post_data, "r", "unlocks") ? (field_is(post_data, "h", "1") ? s->hardcore_unlocks : s->unlocks)
                      : field_is(post_data, "r", "awardachievement") ? s->award : NULL;
    if (!reply) { answer(answer_opaque, 0, NULL, 0); return; }
    char awarded[256];
    if (reply == s->award) {   /* the achievement asked for, and those left */
        char id[16] = "";
        const char *a = strstr(post_data, "&a=");
        if (a) snprintf(id, sizeof id, "%.*s", (int)strcspn(a + 3, "&"), a + 3);
        s->awarded++;
        snprintf(awarded, sizeof awarded, s->award, id, s->award_left - s->awarded);
        reply = awarded;
    }
    const int refused = strstr(reply, "\"Success\":false") != NULL;
    answer(answer_opaque, refused ? 401 : 200, (const uint8_t *)reply, strlen(reply));
}

static void notify(void *opaque, const OraclesAchievementsEvent *event)
{
    server *s = opaque;
    if (s->event_count >= 16) return;
    s->events[s->event_count] = *event;
    snprintf(s->texts[s->event_count], sizeof s->texts[0], "%s", event->text ? event->text : "");
    if (event->token) snprintf(s->token, sizeof s->token, "%s", event->token);
    if (event->user) snprintf(s->user, sizeof s->user, "%s", event->user);
    s->event_count++;
}

static const OraclesAchievementsEvent *find(const server *s, OraclesAchievementsEventKind kind)
{
    for (unsigned i = 0; i < s->event_count; i++) if (s->events[i].kind == kind) return &s->events[i];
    return NULL;
}

static const char *find_request(const server *s, const char *r)
{
    for (unsigned i = 0; i < s->count; i++) if (field_is(s->posted[i], "r", r)) return s->posted[i];
    return NULL;
}

static OraclesAchievements *client(server *s)
{
    const OraclesAchievementsTransport transport = { serve, NULL, NULL, s, NULL };
    return oracles_achievements_create(&transport, "the-oracles-project/1.5.0 (Linux) sameboy/1.0.3 rcheevos/12.5", notify, s);
}

/* Ages' first essence (bit 0 of wEssencesObtained) and Link in a room of the present's overworld, in rcheevos' map. */
static uint32_t essences, group, room;

static void ages_addresses(void)
{
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    essences = oracles_achievements_address(t->essences_obtained.bank, t->essences_obtained.addr);
    group = oracles_achievements_address(t->active_group.bank, t->active_group.addr);
    room = oracles_achievements_address(t->active_room.bank, t->active_room.addr);
}

/* A WRAM byte at an address of rcheevos' map below $E000 or in the banks above. */
static uint8_t *wram_at(uint32_t address)
{
    return address >= 0x10000u ? &wram[address - 0x10000u + 0x2000u] : &wram[address - 0xc000u];
}

static char game_reply[2048];

static void make_game_reply(void)
{
    snprintf(game_reply, sizeof game_reply,
             "{\"Success\":true,\"GameId\":710,\"Title\":\"Oracle of Ages (test set)\",\"ConsoleId\":6,"
             "\"ImageIconUrl\":\"http://server/Images/000001.png\",\"RichPresenceGameId\":710,\"RichPresencePatch\":\"\","
             "\"Sets\":[{\"AchievementSetId\":1,\"GameId\":710,\"Title\":null,\"Type\":\"core\","
             "\"ImageIconUrl\":\"http://server/Images/000001.png\",\"Achievements\":["
             "{\"ID\":9001,\"Title\":\"First essence\",\"Description\":\"Take the first essence\",\"Flags\":3,\"Points\":10,"
             "\"MemAddr\":\"0xM%x=1_d0xM%x=0\",\"Author\":\"test\",\"BadgeName\":\"00001\",\"Created\":1,\"Modified\":1},"
             "{\"ID\":9002,\"Title\":\"A room\",\"Description\":\"Enter room 0x8a of the present\",\"Flags\":3,\"Points\":5,"
             "\"MemAddr\":\"0xH%x=0_0xH%x=138_d0xH%x!=138\",\"Author\":\"test\",\"BadgeName\":\"00002\",\"Created\":1,\"Modified\":1},"
             "{\"ID\":101000001,\"Title\":\"Warning: Unknown Emulator\",\"Description\":\"Hardcore unlocks cannot be earned\",\"Flags\":3,\"Points\":0,"
             "\"MemAddr\":\"1=1\",\"Author\":\"\",\"BadgeName\":\"00003\",\"Created\":1,\"Modified\":1}"
             "],\"Leaderboards\":[]}]}",
             essences, essences, group, room, room);
}

static void test_session(void)
{
    ages_addresses();
    make_game_reply();
    memset(wram, 0, sizeof wram);
    server s;
    memset(&s, 0, sizeof s);
    s.login = "{\"Success\":true,\"User\":\"Player\",\"Token\":\"TOKEN123\",\"Score\":0,\"SoftcoreScore\":0,\"Messages\":0}";
    s.game = game_reply;
    s.session = "{\"Success\":true,\"Unlocks\":[],\"HardcoreUnlocks\":[],\"ServerNow\":1700000000}";
    s.award = "{\"Success\":true,\"Score\":0,\"SoftcoreScore\":10,\"AchievementID\":%s,\"AchievementsRemaining\":%u}";
    s.award_left = 2;
    OraclesAchievements *a = client(&s);
    CHECK(a != NULL);
    if (!a) return;

    /* The password goes in the login's form, once; the token comes back. */
    oracles_achievements_login(a, "player", "pa$$ word", NULL);
    const char *login = find_request(&s, "login2");
    CHECK(login && field_is(login, "u", "player") && field_is(login, "p", "pa%24%24+word"));
    CHECK(find(&s, ORACLES_ACHIEVEMENTS_LOGGED_IN) && !strcmp(s.token, "TOKEN123") && !strcmp(s.user, "Player"));   /* the account's own name */
    CHECK(!strcmp(s.agents[0], "the-oracles-project/1.5.0 (Linux) sameboy/1.0.3 rcheevos/12.5"));

    /* The game by its hash, in casual mode. */
    const char hash[33] = "c4639cc61c049e5a085526bb6cac03bb";
    oracles_achievements_load_game(a, hash, NULL);
    memory_of_buffers(oracles_achievements_memory(a));
    oracles_achievements_frame(a);   /* the game's activation, its addresses checked, between frames */
    const char *sets = find_request(&s, "achievementsets"), *start = find_request(&s, "startsession");
    CHECK(sets && field_is(sets, "m", hash) && field_is(sets, "t", "TOKEN123"));
    CHECK(start && field_is(start, "h", "0"));
    const OraclesAchievementsEvent *loaded = find(&s, ORACLES_ACHIEVEMENTS_GAME_LOADED);
    CHECK(loaded && loaded->count == 2 && loaded->unlocked == 0);
    CHECK(oracles_achievements_watching(a));
    for (unsigned f = 0; f < 10; f++) oracles_achievements_frame(a);
    CHECK(!find(&s, ORACLES_ACHIEVEMENTS_UNLOCKED));

    /* The essence taken: earned at that frame, its award asked for in casual mode. */
    *wram_at(essences) |= 1u;
    oracles_achievements_frame(a);
    const OraclesAchievementsEvent *earned = find(&s, ORACLES_ACHIEVEMENTS_UNLOCKED);
    CHECK(earned && earned->id == 9001 && earned->points == 10 && !strcmp(earned->badge, "00001"));
    const OraclesAchievementsEvent *settled = find(&s, ORACLES_ACHIEVEMENTS_AWARD_SETTLED);
    CHECK(settled && settled->id == 9001 && settled->delivered && earned && earned->delivered);   /* answered in send: before its event */
    const char *award = find_request(&s, "awardachievement");
    CHECK(award && field_is(award, "a", "9001") && field_is(award, "h", "0") && field_is(award, "m", hash) && field_is(award, "u", "Player")
          && field_is(award, "t", "TOKEN123"));
    unsigned awards = 0;
    for (unsigned i = 0; i < s.count; i++) awards += field_is(s.posted[i], "r", "awardachievement");
    CHECK(awards == 1);

    /* The room: its byte changes to 0x8a in the present; once only. */
    *wram_at(room) = 0x8a;
    oracles_achievements_frame(a);
    oracles_achievements_frame(a);
    unsigned earned_count = 0;
    for (unsigned i = 0; i < s.event_count; i++) earned_count += s.events[i].kind == ORACLES_ACHIEVEMENTS_UNLOCKED;
    CHECK(earned_count == 2);
    CHECK(find(&s, ORACLES_ACHIEVEMENTS_GAME_COMPLETED));
    oracles_achievements_destroy(a);

    /* A token the server refuses: the caller is told to forget it. */
    server refused;
    memset(&refused, 0, sizeof refused);
    refused.login = "{\"Success\":false,\"Error\":\"Invalid User/Token combination.\",\"Status\":401,\"Code\":\"invalid_credentials\"}";
    a = client(&refused);
    oracles_achievements_login(a, "Player", NULL, "OLD");
    const char *with_token = find_request(&refused, "login2");
    CHECK(with_token && field_is(with_token, "t", "OLD") && !strstr(with_token, "p="));
    const OraclesAchievementsEvent *failed = find(&refused, ORACLES_ACHIEVEMENTS_LOGIN_FAILED);
    CHECK(failed && failed->token_refused);
    oracles_achievements_destroy(a);

    /* No server at all: the login fails, nothing is earned. */
    server none;
    memset(&none, 0, sizeof none);
    a = client(&none);
    oracles_achievements_login(a, "Player", NULL, "TOKEN123");
    oracles_achievements_frame(a);
    CHECK(!find(&none, ORACLES_ACHIEVEMENTS_LOGGED_IN));
    oracles_achievements_destroy(a);
}

/* A server for the game of make_game_reply, as test_session's. */
static void start_server(server *s, int award)
{
    memset(s, 0, sizeof *s);
    s->login = "{\"Success\":true,\"User\":\"Player\",\"Token\":\"TOKEN123\",\"Score\":0,\"SoftcoreScore\":0,\"Messages\":0}";
    s->game = game_reply;
    s->session = "{\"Success\":true,\"Unlocks\":[],\"HardcoreUnlocks\":[],\"ServerNow\":1700000000}";
    if (award) s->award = "{\"Success\":true,\"Score\":0,\"SoftcoreScore\":10,\"AchievementID\":%s,\"AchievementsRemaining\":%u}";
    s->award_left = 2;
    s->unlocks = s->hardcore_unlocks = "{\"Success\":true,\"UserUnlocks\":[],\"GameID\":710,\"HardcoreMode\":false}";
}

static OraclesAchievements *playing(server *s)
{
    OraclesAchievements *a = client(s);
    oracles_achievements_login(a, "Player", NULL, "TOKEN123");
    oracles_achievements_load_game(a, "c4639cc61c049e5a085526bb6cac03bb", NULL);
    memory_of_buffers(oracles_achievements_memory(a));
    oracles_achievements_frame(a);
    return a;
}

/* An unlock no server acknowledges: kept on disk with its time, sent again at the next login with the time since, and
 * forgotten once acknowledged. */
static void test_offline_unlock(const char *folder)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/test_achievements_pending.txt", folder);
    remove(path);
    memset(wram, 0, sizeof wram);
    server offline;
    start_server(&offline, 0);
    OraclesAchievements *a = playing(&offline);
    oracles_achievements_frame(a);
    *wram_at(essences) |= 1u;
    oracles_achievements_frame(a);
    const OraclesAchievementsEvent *earned = find(&offline, ORACLES_ACHIEVEMENTS_UNLOCKED);
    CHECK(earned && earned->id == 9001 && !earned->delivered && !find(&offline, ORACLES_ACHIEVEMENTS_AWARD_SETTLED));
    for (unsigned i = 0; i < offline.event_count; i++) oracles_achievements_pending_follow(path, "Player", "c4639cc61c049e5a085526bb6cac03bb", &offline.events[i], 1000);
    oracles_achievements_destroy(a);
    OraclesAchievementsPendingUnlock kept[4];
    CHECK(oracles_achievements_pending_read(path, kept, 4) == 1 && kept[0].id == 9001 && kept[0].when == 1000 && !strcmp(kept[0].user, "Player")
          && !strcmp(kept[0].hash, "c4639cc61c049e5a085526bb6cac03bb"));

    /* The next game, the server there: sent again, 60 seconds after. */
    server online;
    start_server(&online, 1);
    a = client(&online);
    oracles_achievements_login(a, "Player", NULL, "TOKEN123");
    CHECK(oracles_achievements_pending_resend(a, path, "Player", "TOKEN123", 1060) == 1);
    const char *award = find_request(&online, "awardachievement");
    CHECK(award && field_is(award, "a", "9001") && field_is(award, "h", "0") && field_is(award, "o", "60") && field_is(award, "m", "c4639cc61c049e5a085526bb6cac03bb"));
    const OraclesAchievementsEvent *settled = find(&online, ORACLES_ACHIEVEMENTS_AWARD_SETTLED);
    CHECK(settled && settled->id == 9001 && settled->delivered);
    if (settled) oracles_achievements_pending_follow(path, "Player", "c4639cc61c049e5a085526bb6cac03bb", settled, 1060);
    CHECK(oracles_achievements_pending_read(path, kept, 4) == 0);
    oracles_achievements_destroy(a);

    /* A refusal for the token keeps it; another refusal drops it; one line an account and achievement. */
    OraclesAchievementsPendingUnlock u = { "Player", "c4639cc61c049e5a085526bb6cac03bb", 9002, 5 };
    CHECK(oracles_achievements_pending_add(path, &u) && oracles_achievements_pending_add(path, &u));
    CHECK(oracles_achievements_pending_read(path, kept, 4) == 1);
    OraclesAchievementsEvent refused = { ORACLES_ACHIEVEMENTS_AWARD_SETTLED, "token", NULL, NULL, NULL, 9002, 0, 0, 0, 1, 0, 0 };
    oracles_achievements_pending_follow(path, "Player", u.hash, &refused, 6);
    CHECK(oracles_achievements_pending_read(path, kept, 4) == 1);
    refused.token_refused = 0;
    oracles_achievements_pending_follow(path, "Player", u.hash, &refused, 6);
    CHECK(oracles_achievements_pending_read(path, kept, 4) == 0);

    /* A captive portal's page, an error of the server in HTML or in JSON with a 5xx: nothing settled, the line stays. */
    static const char *const pages[3] = { "<html><body>Hotel WiFi: accept the terms</body></html>", "<html>500 Internal Server Error</html>",
                                          "{\"Success\":false,\"Error\":\"Database unavailable\"}" };
    static const int statuses[3] = { 200, 500, 503 };
    for (unsigned k = 0; k < 3; k++) {
        CHECK(oracles_achievements_pending_add(path, &u));
        server portal;
        start_server(&portal, 1);
        portal.award_page = pages[k];
        portal.award_status = statuses[k];
        a = client(&portal);
        oracles_achievements_login(a, "Player", NULL, "TOKEN123");
        oracles_achievements_pending_resend(a, path, "Player", "TOKEN123", 10);
        CHECK(find_request(&portal, "awardachievement") && !find(&portal, ORACLES_ACHIEVEMENTS_AWARD_SETTLED));
        for (unsigned i = 0; i < portal.event_count; i++) oracles_achievements_pending_follow(path, "Player", u.hash, &portal.events[i], 10);
        CHECK(oracles_achievements_pending_read(path, kept, 4) == 1);
        oracles_achievements_destroy(a);
    }
    remove(path);
}

/* Suspended (a cheat): nothing unlocks any more, whatever the memory says. */
static void test_suspension(void)
{
    memset(wram, 0, sizeof wram);
    server s;
    start_server(&s, 1);
    OraclesAchievements *a = playing(&s);
    CHECK(oracles_achievements_suspension(a) == NULL);
    oracles_achievements_suspend(a, ORACLES_ACHIEVEMENTS_CHEATS_REASON);
    oracles_achievements_suspend(a, "another");
    CHECK(oracles_achievements_suspension(a) && !strcmp(oracles_achievements_suspension(a), ORACLES_ACHIEVEMENTS_CHEATS_REASON));
    *wram_at(essences) |= 1u;
    *wram_at(room) = 0x8a;
    for (unsigned f = 0; f < 5; f++) oracles_achievements_frame(a);
    CHECK(!find(&s, ORACLES_ACHIEVEMENTS_UNLOCKED) && !find_request(&s, "awardachievement"));
    oracles_achievements_destroy(a);
}

/* The badges: each image asked for once, kept as <badge>.png and <badge>_lock.png, never asked for again. */
static void test_badges(const char *folder)
{
    static const char *const names[4] = { "00001.png", "00001_lock.png", "00002.png", "00002_lock.png" };
    char path[1024];
    for (unsigned i = 0; i < 4; i++) { snprintf(path, sizeof path, "%s/%s", folder, names[i]); remove(path); }
    CHECK(!oracles_achievements_badge_path(folder, "../x", 0, path, sizeof path) && !oracles_achievements_badge_path(folder, "", 0, path, sizeof path));
    server s;
    start_server(&s, 1);
    s.images = 1;
    OraclesAchievements *a = playing(&s);
    static OraclesAchievementsBadges badges;
    memset(&badges, 0, sizeof badges);
    oracles_achievements_fetch_badges(a, folder, &badges);
    CHECK(badges.count == 4);
    for (unsigned i = 0; i < 4; i++) {
        snprintf(path, sizeof path, "%s/%s", folder, names[i]);
        FILE *f = fopen(path, "rb");
        uint8_t head[4] = { 0 };
        CHECK(f && fread(head, 1, 4, f) == 4 && head[1] == 'P');
        if (f) fclose(f);
    }
    const unsigned asked = s.images_asked;
    memset(&badges, 0, sizeof badges);
    oracles_achievements_fetch_badges(a, folder, &badges);
    CHECK(asked == 4 && s.images_asked == 4);   /* the files there: none asked again */
    oracles_achievements_destroy(a);
    for (unsigned i = 0; i < 4; i++) { snprintf(path, sizeof path, "%s/%s", folder, names[i]); remove(path); }
}


/* ---- the pause's list, the toasts, the home screen's login ---------------------------------------------------- */

/* make_game_reply's two, an older answer's with neither type nor rarity, and a third measured on HRAM's first byte (rcheevos'
 * $FF80), five to reach, missable, its casual and hardcore rarities apart. */
static char list_reply[2560];

static void make_list_reply(void)
{
    make_game_reply();
    char *end = strstr(game_reply, "],\"Leaderboards\"");
    if (!end) { snprintf(list_reply, sizeof list_reply, "%s", game_reply); return; }
    snprintf(list_reply, sizeof list_reply, "%.*s,{\"ID\":9003,\"Title\":\"Five\",\"Description\":\"Reach\\nfive\",\"Flags\":3,\"Points\":20,"
             "\"MemAddr\":\"M:0xHff80>=5\",\"Author\":\"test\",\"BadgeName\":\"00003\",\"Created\":1,\"Modified\":1,"
             "\"Type\":\"missable\",\"Rarity\":12.4,\"RarityHardcore\":3.1}%s",
             (int)(end - game_reply), game_reply, end);
}

static void date_of(int64_t when, char *out, size_t capacity) { snprintf(out, capacity, "at %lld", (long long)when); }

/* The rarity and the tag as the list keeps them: the casual figure, not the hardcore's; none said for an answer without them. */
static void check_details(const OraclesAchievementsItem *with, const OraclesAchievementsItem *without)
{
    char text[32];
    oracles_achievements_rarity_text(with->rarity, text, sizeof text);
    CHECK(!strcmp(with->title, "Five") && !strcmp(text, "12.4% of players") && with->tag == ORACLES_ACHIEVEMENT_TAG_MISSABLE);
    CHECK(!strcmp(with->description, "Reach five"));   /* its line break a space */
    oracles_achievements_rarity_text(without->rarity, text, sizeof text);
    CHECK(!text[0] && without->tag == ORACLES_ACHIEVEMENT_TAG_NONE);
}

/* The list: the unlocked first, then those in progress, then the locked; how many, their points; and an unlock no
 * server acknowledges: OFFLINE. */
static void test_list(void)
{
    make_list_reply();
    memset(wram, 0, sizeof wram);
    memset(hram, 0, sizeof hram);
    server s;
    start_server(&s, 0);
    s.game = list_reply;
    OraclesAchievements *a = playing(&s);
    static OraclesAchievementsList list;
    memset(&list, 0, sizeof list);
    hram[0] = 2;
    oracles_achievements_frame(a);
    *wram_at(room) = 0x8a;
    oracles_achievements_frame(a);
    oracles_achievements_frame(a);
    const OraclesAchievementsEvent *earned = find(&s, ORACLES_ACHIEVEMENTS_UNLOCKED);
    CHECK(earned && earned->id == 9002 && find(&s, ORACLES_ACHIEVEMENTS_OFFLINE));
    unsigned count, unlocked, points, points_unlocked;
    oracles_achievements_progress(a, &count, &unlocked, &points, &points_unlocked);
    CHECK(count == 3 && unlocked == 1 && points == 35 && points_unlocked == 5);
    CHECK(oracles_achievements_list_make(a, "badges", date_of, &list) && list.generation == 1);
    CHECK(list.count == 3 && list.unlocked == 1 && list.points == 35 && list.points_unlocked == 5 && !strcmp(list.badges, "badges"));
    CHECK(!strcmp(list.items[0].title, "A room") && list.items[0].unlocked && !strncmp(list.items[0].date, "at ", 3) && !strcmp(list.items[0].badge, "00002"));
    CHECK(!strcmp(list.items[1].title, "Five") && !list.items[1].unlocked && !strcmp(list.items[1].progress, "2/5") && list.items[1].percent > 0.39f
          && list.items[1].percent < 0.41f);
    CHECK(!strcmp(list.items[2].title, "First essence") && !list.items[2].unlocked && !list.items[2].progress[0] && !list.items[2].date[0]);
    check_details(&list.items[1], &list.items[2]);
    oracles_achievements_destroy(a);
    /* Before the set is loaded: none. */
    server none;
    memset(&none, 0, sizeof none);
    a = client(&none);
    CHECK(!oracles_achievements_list_make(a, "badges", date_of, &list) && list.count == 0 && list.generation == 2);
    oracles_achievements_destroy(a);
}

/* ---- the home page's list --------------------------------------------------------------------------------- */

static void write_answer(const char *folder, const char *name, const char *body);
static const char login_answer[] = "{\"Success\":true,\"User\":\"Player\",\"Token\":\"TOKEN123\",\"Score\":0,\"SoftcoreScore\":0,\"Messages\":0}";

static OraclesAchievementsTransport canned_transport(server *s)
{
    const OraclesAchievementsTransport t = { serve, NULL, NULL, s, NULL };
    return t;
}

static const OraclesAchievementsAccount the_account = { "Player", "TOKEN123" };

/* The whole of a file, NUL ended; empty when it is not there. */
static void read_all(const char *path, char *out, size_t capacity)
{
    FILE *f = fopen(path, "rb");
    size_t n = f ? fread(out, 1, capacity - 1, f) : 0;
    out[n] = 0;
    if (f) fclose(f);
}

/* The list loaded with no game: its state, the list, the unlocked first with no date, either kind of unlock counting, the
 * client gone once it is in; no session on the server: neither startsession nor ping nor award, whatever waits; the
 * unlocks waiting not touched, only a game's session does that; a refused token forgotten, a changed one kept; no
 * server, no answer. */
static void test_load(const char *folder)
{
    char account[1024], pending[1024], before[1024], after[1024];
    snprintf(account, sizeof account, "%s/achievements.txt", folder);
    snprintf(pending, sizeof pending, "%s/achievements-pending.txt", folder);
    remove(pending);
    /* An unlock kept offline, beside the account, as a session leaves it. */
    OraclesAchievementsEvent earned;
    memset(&earned, 0, sizeof earned);
    earned.kind = ORACLES_ACHIEVEMENTS_UNLOCKED;
    earned.id = 9001;
    oracles_achievements_pending_follow(pending, "Player", "0123456789abcdef0123456789abcdef", &earned, 1000);
    read_all(pending, before, sizeof before);
    CHECK(before[0]);
    CHECK(oracles_achievements_account_write(account, &the_account));

    make_list_reply();
    server s;
    start_server(&s, 1);
    s.game = list_reply;
    s.unlocks = "{\"Success\":true,\"UserUnlocks\":[9002],\"GameID\":710,\"HardcoreMode\":false}";
    s.hardcore_unlocks = "{\"Success\":true,\"UserUnlocks\":[9001,9002],\"GameID\":710,\"HardcoreMode\":true}";
    OraclesAchievementsTransport t = canned_transport(&s);
    OraclesAchievementsLoad *l = oracles_achievements_load_begin(&t, "ua", &the_account, account, "0123456789abcdef0123456789abcdef", "", 1000.0);
    CHECK(l != NULL);
    const OraclesAchievementsList *list = NULL;
    int busy = 1;
    CHECK(oracles_achievements_load_poll(l, 1100.0, &list, &busy) == ORACLES_ACHIEVEMENTS_LIST_READY);
    CHECK(list && list->count == 3 && list->points == 35 && !busy);   /* no badge to wait for: the client is gone */
    CHECK(list && list->unlocked == 2 && list->points_unlocked == 15 && list->items[0].unlocked && list->items[1].unlocked && !list->items[2].unlocked);
    CHECK(list && !list->items[0].date[0] && !list->items[2].progress[0]);
    if (list) check_details(&list->items[2], &list->items[0]);
    CHECK(find_request(&s, "login2") && find_request(&s, "achievementsets") && find_request(&s, "unlocks"));
    CHECK(!find_request(&s, "startsession") && !find_request(&s, "ping") && !find_request(&s, "awardachievement"));
    const unsigned requests = s.count;
    CHECK(oracles_achievements_load_poll(l, 1200.0, &list, &busy) == ORACLES_ACHIEVEMENTS_LIST_READY && list && !busy && s.count == requests);
    oracles_achievements_load_end(l);
    read_all(pending, after, sizeof after);
    CHECK(!strcmp(before, after));

    /* A token changed by the server is kept. */
    server renewed;
    start_server(&renewed, 0);
    renewed.game = list_reply;
    renewed.login = "{\"Success\":true,\"User\":\"Player\",\"Token\":\"NEWTOKEN\",\"Score\":0,\"SoftcoreScore\":0,\"Messages\":0}";
    t = canned_transport(&renewed);
    l = oracles_achievements_load_begin(&t, "ua", &the_account, account, "0123456789abcdef0123456789abcdef", "", 1000.0);
    oracles_achievements_load_poll(l, 1100.0, &list, &busy);
    oracles_achievements_load_end(l);
    OraclesAchievementsAccount kept;
    oracles_achievements_account_read(account, &kept);
    CHECK(!strcmp(kept.token, "NEWTOKEN"));

    /* The token refused: failed, not unreachable, and forgotten; the list never there. */
    server refused;
    start_server(&refused, 0);
    refused.login = "{\"Success\":false,\"Error\":\"Invalid User/Token combination.\",\"Status\":401,\"Code\":\"invalid_credentials\"}";
    t = canned_transport(&refused);
    l = oracles_achievements_load_begin(&t, "ua", &kept, account, "0123456789abcdef0123456789abcdef", "", 1000.0);
    CHECK(oracles_achievements_load_poll(l, 1100.0, &list, &busy) == ORACLES_ACHIEVEMENTS_LIST_FAILED && !list && !busy);
    oracles_achievements_load_end(l);
    oracles_achievements_account_read(account, &kept);
    CHECK(!strcmp(kept.user, "Player") && !kept.token[0]);

    /* No account path (the offline answers): nothing written; no server: unreachable. */
    server none;
    memset(&none, 0, sizeof none);
    t = canned_transport(&none);
    l = oracles_achievements_load_begin(&t, "ua", &the_account, NULL, "0123456789abcdef0123456789abcdef", "", 1000.0);
    CHECK(oracles_achievements_load_poll(l, 1100.0, &list, &busy) == ORACLES_ACHIEVEMENTS_LIST_UNREACHABLE && !list && !busy);
    oracles_achievements_load_end(l);
    remove(account);
    remove(pending);
}

/* The whole path on the offline transport, every answer there, startsession's and ping's too: its log holds each request
 * sent, in order. */
static void test_load_log(const char *folder)
{
    static const char *const answers[5] = { "achievementsets", "unlocks", "startsession", "ping", "awardachievement" };
    char log[1024], before[1024];
    snprintf(log, sizeof log, "%s/requests.log", folder);
    remove(log);
    make_list_reply();
    write_answer(folder, "login2", login_answer);
    write_answer(folder, answers[0], list_reply);
    write_answer(folder, answers[1], "{\"Success\":true,\"UserUnlocks\":[9002],\"GameID\":710,\"HardcoreMode\":false}");
    write_answer(folder, answers[2], "{\"Success\":true,\"Unlocks\":[],\"HardcoreUnlocks\":[],\"ServerNow\":1700000000}");
    write_answer(folder, answers[3], "{\"Success\":true}");
    write_answer(folder, answers[4], "{\"Success\":true,\"Score\":0,\"SoftcoreScore\":10,\"AchievementID\":9001,\"AchievementsRemaining\":2}");
    OraclesAchievementsTransport t;
    CHECK(oracles_achievements_offline_transport(folder, &t));
    OraclesAchievementsLoad *l = oracles_achievements_load_begin(&t, "ua", &the_account, NULL, "0123456789abcdef0123456789abcdef", "", 1000.0);
    const OraclesAchievementsList *list = NULL;
    int busy = 1;
    /* The page left open for ten minutes: a session's pings would be due. */
    CHECK(oracles_achievements_load_poll(l, 1100.0, &list, &busy) == ORACLES_ACHIEVEMENTS_LIST_READY && list && list->unlocked == 1);
    for (double now = 1200.0; now < 600000.0; now += 30000.0) oracles_achievements_load_poll(l, now, &list, &busy);
    oracles_achievements_load_end(l);
    read_all(log, before, sizeof before);
    CHECK(!strcmp(before, "login2\nachievementsets\nunlocks\nunlocks\n"));
    remove(log);
    char path[1024];
    for (unsigned i = 0; i < 5; i++) { snprintf(path, sizeof path, "%s/%s.json", folder, answers[i]); remove(path); }
    snprintf(path, sizeof path, "%s/login2.json", folder);
    remove(path);
}

/* A ROM in a zip: the same identity as the ROM alone, as a session reads it. */
static void test_load_hash(const char *folder)
{
    static const uint8_t rom[64] = "not a whole ROM, a few bytes to hash";
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < sizeof rom; i++) {
        crc ^= rom[i];
        for (int b = 0; b < 8; b++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    crc = ~crc;
    uint8_t zip[256];
    size_t n = 0;
#define PUT16(v) do { zip[n++] = (uint8_t)((v) & 0xffu); zip[n++] = (uint8_t)(((v) >> 8) & 0xffu); } while (0)
#define PUT32(v) do { PUT16((v) & 0xffffu); PUT16(((uint32_t)(v) >> 16) & 0xffffu); } while (0)
    static const char name[] = "game.gbc";
    const uint32_t size = sizeof rom, name_size = sizeof name - 1;
    PUT32(0x04034b50u); PUT16(20); PUT16(0); PUT16(0); PUT16(0); PUT16(0); PUT32(crc); PUT32(size); PUT32(size); PUT16(name_size); PUT16(0);
    memcpy(zip + n, name, name_size); n += name_size;
    memcpy(zip + n, rom, size); n += size;
    const uint32_t directory = (uint32_t)n;
    PUT32(0x02014b50u); PUT16(20); PUT16(20); PUT16(0); PUT16(0); PUT16(0); PUT16(0); PUT32(crc); PUT32(size); PUT32(size); PUT16(name_size);
    PUT16(0); PUT16(0); PUT16(0); PUT16(0); PUT32(0); PUT32(0);
    memcpy(zip + n, name, name_size); n += name_size;
    const uint32_t directory_size = (uint32_t)n - directory;
    PUT32(0x06054b50u); PUT16(0); PUT16(0); PUT16(1); PUT16(1); PUT32(directory_size); PUT32(directory); PUT16(0);
    char path[1024], plain[1024], hash[33], wanted[33], from_plain[33];
    snprintf(path, sizeof path, "%s/test_achievements_rom.zip", folder);
    snprintf(plain, sizeof plain, "%s/test_achievements_rom.gbc", folder);
    FILE *f = fopen(path, "wb");
    CHECK(f && fwrite(zip, 1, n, f) == n);
    if (f) fclose(f);
    f = fopen(plain, "wb");
    CHECK(f && fwrite(rom, 1, sizeof rom, f) == sizeof rom);
    if (f) fclose(f);
    oracles_achievements_hash(rom, sizeof rom, wanted);
    CHECK(oracles_achievements_load_hash(plain, from_plain) && !strcmp(from_plain, wanted));
    CHECK(oracles_achievements_load_hash(path, hash) && !strcmp(hash, wanted));
    CHECK(!oracles_achievements_load_hash("no such file.gbc", hash));
    remove(path);
    remove(plain);
}

/* The page's load, as the home screen's loop calls it: nothing before the page opens; begun once, then its list; let go
 * with nav's list as the game changes or the page closes, or Off, or logged out; a load that cannot start stays failed
 * until the page opens again; the login open over the page holds it. */
typedef struct page_host {
    server *s;
    unsigned begun;
    int last_game, fail;
    OraclesAchievementsAccount account;
} page_host;

static OraclesAchievementsLoad *page_begin(void *opaque, OraclesHomeNav *nav, int game)
{
    page_host *h = opaque;
    (void)nav;
    h->begun++;
    h->last_game = game;
    if (h->fail) return NULL;
    const OraclesAchievementsTransport t = canned_transport(h->s);
    return oracles_achievements_load_begin(&t, "ua", &h->account, NULL, "0123456789abcdef0123456789abcdef", "", 1000.0);
}

static void test_page_load(void)
{
    make_list_reply();
    server s;
    start_server(&s, 0);
    s.game = list_reply;
    page_host host = { &s, 0, -1, 0, the_account };
    static OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.games[ORACLES_HOME_GAME_AGES].rom = nav.games[ORACLES_HOME_GAME_SEASONS].rom = ORACLES_ROM_ORIGINAL;
    OraclesHomeAchievements *a = &nav.achievements;
    a->shown = a->on = 1;
    snprintf(a->user, sizeof a->user, "Player");
    OraclesAchievementsLoad *load = NULL;
    int game = -1;
#define STEP() oracles_achievements_page_load(&load, &game, &nav, page_begin, &host, 1100.0)
    /* On the home menu: nothing. */
    CHECK(!STEP() && !load && host.begun == 0 && a->list_state == ORACLES_ACHIEVEMENTS_LIST_NONE);
    oracles_achievements_open(&nav);
    CHECK(STEP() && load && host.begun == 1 && host.last_game == ORACLES_HOME_GAME_AGES && a->list_state == ORACLES_ACHIEVEMENTS_LIST_LOADING && a->list_busy);
    CHECK(STEP() && a->list_state == ORACLES_ACHIEVEMENTS_LIST_READY && a->list && a->list->count == 3 && !a->list_busy);
    CHECK(oracles_achievements_page_list(&nav) && oracles_achievements_page_places(&nav) == ORACLES_ACHIEVEMENTS_ROWS + 3u);
    CHECK(!STEP() && host.begun == 1);
    /* Another game under the page, the detail's panel open: the list goes at once, the panel with it, then the other's. */
    a->detail = 1;
    nav.entry = ORACLES_HOME_SEASONS;
    CHECK(STEP() && !load && !a->list && a->list_state == ORACLES_ACHIEVEMENTS_LIST_NONE && !a->detail);
    CHECK(STEP() && load && host.begun == 2 && host.last_game == ORACLES_HOME_GAME_SEASONS);
    /* Off, then logged out, then Back: the same. */
    STEP();
    a->on = 0;
    CHECK(STEP() && !load && !a->list && !a->list_busy);
    a->on = 1;
    STEP();
    CHECK(load && host.begun == 3);
    a->user[0] = 0;
    CHECK(STEP() && !load && !a->list);
    snprintf(a->user, sizeof a->user, "Player");
    STEP();
    a->login = ORACLES_LOGIN_TYPING;
    CHECK(STEP() && !load);
    a->login = ORACLES_LOGIN_CLOSED;
    STEP();
    nav.screen = ORACLES_SCREEN_HOME;
    CHECK(STEP() && !load && !a->list && a->list_state == ORACLES_ACHIEVEMENTS_LIST_NONE);
    /* A load that cannot start: failed, not asked again until the page opens again. */
    const unsigned begun = host.begun;
    host.fail = 1;
    nav.screen = ORACLES_SCREEN_ACHIEVEMENTS;
    CHECK(STEP() && !load && a->list_state == ORACLES_ACHIEVEMENTS_LIST_FAILED && !a->list_busy);
    CHECK(!STEP() && host.begun == begun + 1u);
    nav.screen = ORACLES_SCREEN_HOME;
    STEP();
    host.fail = 0;
    nav.screen = ORACLES_SCREEN_ACHIEVEMENTS;
    CHECK(STEP() && load);
    nav.screen = ORACLES_SCREEN_HOME;
    STEP();
    CHECK(!load);
    /* A game without achievements: nothing asked, the page saying nothing. */
    nav.games[ORACLES_HOME_GAME_SEASONS].rom = ORACLES_ROM_UNRECOGNISED;
    nav.screen = ORACLES_SCREEN_ACHIEVEMENTS;
    const unsigned asked = host.begun;
    CHECK(!STEP() && !load && host.begun == asked && !oracles_achievements_page_line(&nav));
#undef STEP
}

/* A session's toasts, as its client's events tell them. */
typedef struct telling {
    OraclesAchievementsToasts toasts;
    OraclesAchievementsTold told;
    OraclesAchievements *client;
} telling;

static void tell_event(void *opaque, const OraclesAchievementsEvent *event)
{
    telling *t = opaque;
    oracles_achievements_tell(&t->toasts, &t->told, t->client, "Oracle of Ages", event);
}

static const OraclesAchievementsToast *toast_at(const telling *t, unsigned i) { return &t->toasts.queue[(t->toasts.first + i) % ORACLES_ACHIEVEMENTS_TOASTS]; }

static void test_tell(void)
{
    make_game_reply();
    memset(wram, 0, sizeof wram);
    server s;
    start_server(&s, 1);
    static telling t;
    memset(&t, 0, sizeof t);
    const OraclesAchievementsTransport transport = { serve, NULL, NULL, &s, NULL };
    t.client = oracles_achievements_create(&transport, "the-oracles-project/1.5.0 (Linux)", tell_event, &t);
    oracles_achievements_login(t.client, "Player", NULL, "TOKEN123");
    oracles_achievements_load_game(t.client, "c4639cc61c049e5a085526bb6cac03bb", NULL);
    memory_of_buffers(oracles_achievements_memory(t.client));
    oracles_achievements_frame(t.client);
    CHECK(t.toasts.count == 1 && !strcmp(toast_at(&t, 0)->over, "RetroAchievements")
          && !strcmp(toast_at(&t, 0)->text, "Oracle of Ages \xc2\xb7 0 of 2 achievements \xc2\xb7 Casual") && t.told.loaded);
    for (unsigned f = 0; f < 3; f++) oracles_achievements_frame(t.client);
    *wram_at(essences) |= 1u;
    oracles_achievements_frame(t.client);
    CHECK(t.toasts.count == 2 && !strcmp(toast_at(&t, 1)->over, "Achievement unlocked \xc2\xb7 10 points") && !strcmp(toast_at(&t, 1)->text, "First essence")
          && !strcmp(toast_at(&t, 1)->badge, "00001"));
    /* Offline: told once, again after another unlock. */
    const OraclesAchievementsEvent offline = { ORACLES_ACHIEVEMENTS_OFFLINE, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0 };
    tell_event(&t, &offline);
    tell_event(&t, &offline);
    CHECK(t.toasts.count == 3 && !strcmp(toast_at(&t, 2)->text, "Offline: unlocks will be sent later"));
    *wram_at(room) = 0x8a;
    oracles_achievements_frame(t.client);
    oracles_achievements_frame(t.client);
    CHECK(t.toasts.count == 5 && !strcmp(toast_at(&t, 3)->text, "A room")
          && !strcmp(toast_at(&t, 4)->text, "Oracle of Ages \xc2\xb7 all 2 achievements \xc2\xb7 Completed \xc2\xb7 Casual"));
    tell_event(&t, &offline);
    CHECK(t.toasts.count == 6);
    /* A cheat turned on: told once after a frame. */
    oracles_achievements_suspend(t.client, ORACLES_ACHIEVEMENTS_CHEATS_REASON);
    oracles_achievements_tell_suspension(&t.toasts, &t.told, t.client);
    oracles_achievements_tell_suspension(&t.toasts, &t.told, t.client);
    CHECK(t.toasts.count == 7 && !strcmp(toast_at(&t, 6)->text, "Achievements suspended: a cheat is on"));
    oracles_achievements_destroy(t.client);
    /* A mod's change before the game is loaded: the start says so, and nothing more. */
    memset(&t, 0, sizeof t);
    start_server(&s, 1);
    t.client = oracles_achievements_create(&transport, "the-oracles-project/1.5.0 (Linux)", tell_event, &t);
    oracles_achievements_suspend(t.client, ORACLES_ACHIEVEMENTS_MODS_REASON);
    oracles_achievements_login(t.client, "Player", NULL, "TOKEN123");
    oracles_achievements_load_game(t.client, "c4639cc61c049e5a085526bb6cac03bb", NULL);
    oracles_achievements_frame(t.client);
    oracles_achievements_tell_suspension(&t.toasts, &t.told, t.client);
    CHECK(t.toasts.count == 1 && !strcmp(toast_at(&t, 0)->text, "Achievements are paused until the game starts again: a mod changed the game."));
    oracles_achievements_destroy(t.client);
}

static void write_answer(const char *folder, const char *name, const char *body)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.json", folder, name);
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f) { fputs(body, f); fclose(f); }
}

static void remove_answers(const char *folder)
{
    static const char *const names[3] = { "login2", "achievementsets", "startsession" };
    char path[1024];
    for (unsigned i = 0; i < 3; i++) { snprintf(path, sizeof path, "%s/%s.json", folder, names[i]); remove(path); }
}


/* The server unreachable as the game starts, the developer's offline answers missing: the login's, else the game's; told
 * once, the game going on without achievements. */
static void test_unreachable(const char *folder)
{
    for (int login_answered = 0; login_answered < 2; login_answered++) {
        remove_answers(folder);
        if (login_answered) write_answer(folder, "login2", login_answer);
        static telling t;
        memset(&t, 0, sizeof t);
        OraclesAchievementsTransport transport;
        CHECK(oracles_achievements_offline_transport(folder, &transport));
        t.client = oracles_achievements_create(&transport, "the-oracles-project/1.5.0 (Linux)", tell_event, &t);
        oracles_achievements_login(t.client, "Player", NULL, "TOKEN123");
        oracles_achievements_load_game(t.client, "c4639cc61c049e5a085526bb6cac03bb", NULL);
        memory_of_buffers(oracles_achievements_memory(t.client));
        for (unsigned f = 0; f < 5; f++) oracles_achievements_frame(t.client);
        CHECK(t.toasts.count == 1 && t.told.unreachable && !t.told.loaded && !oracles_achievements_watching(t.client));
        CHECK(!strcmp(toast_at(&t, 0)->text, "RetroAchievements unreachable: achievements are off for this game."));
        oracles_achievements_destroy(t.client);
    }
    remove_answers(folder);
}

/* A transport no server ever answers, its requests answered -1 as it goes. */
typedef struct silent {
    OraclesAchievementsAnswer answer;
    void *answer_opaque;
    int destroyed;
} silent;

static void silent_send(void *opaque, const char *url, const char *post_data, const char *content_type, const char *user_agent,
                        OraclesAchievementsAnswer answer, void *answer_opaque)
{
    silent *s = opaque;
    (void)url; (void)post_data; (void)content_type; (void)user_agent;
    s->answer = answer;
    s->answer_opaque = answer_opaque;
}

static void silent_destroy(void *opaque)
{
    silent *s = opaque;
    s->destroyed = 1;
    if (s->answer) s->answer(s->answer_opaque, -1, NULL, 0);
}

/* The home screen's login on the developer's offline answers: in, the account kept where it is given (nothing kept
 * without); the password refused; no answer; none in 30 s, and given up before. */
/* No answer: the page keeps loading, then says the server cannot be reached, its client gone; left before, gone too. */
static void test_load_silent(void)
{
    silent q;
    memset(&q, 0, sizeof q);
    OraclesAchievementsTransport t;
    memset(&t, 0, sizeof t);
    t.send = silent_send;
    t.destroy = silent_destroy;
    t.opaque = &q;
    OraclesAchievementsLoad *l = oracles_achievements_load_begin(&t, "ua", &the_account, NULL, "0123456789abcdef0123456789abcdef", "", 1000.0);
    const OraclesAchievementsList *list;
    int busy;
    CHECK(oracles_achievements_load_poll(l, 2000.0, &list, &busy) == ORACLES_ACHIEVEMENTS_LIST_LOADING && busy && !list && !q.destroyed);
    CHECK(oracles_achievements_load_poll(l, 1000.0 + ORACLES_ACHIEVEMENTS_LOAD_WAIT_MS, &list, &busy) == ORACLES_ACHIEVEMENTS_LIST_UNREACHABLE && !busy && q.destroyed);
    oracles_achievements_load_end(l);
    memset(&q, 0, sizeof q);
    l = oracles_achievements_load_begin(&t, "ua", &the_account, NULL, "0123456789abcdef0123456789abcdef", "", 1000.0);
    oracles_achievements_load_end(l);
    CHECK(q.destroyed);
}

static void test_home_login(const char *folder)
{
    char path[1024], user[64];
    snprintf(path, sizeof path, "%s/test_achievements_account.txt", folder);
    remove(path);
    remove_answers(folder);
    write_answer(folder, "login2", login_answer);
    OraclesAchievementsTransport transport;
    CHECK(oracles_achievements_offline_transport(folder, &transport));
    OraclesAchievementsHomeLogin *l = oracles_achievements_home_login_begin(&transport, "ua", path, "player", "secret", 1000.0);
    CHECK(l && oracles_achievements_home_login_poll(l, 1000.0, user, sizeof user) == 1 && !strcmp(user, "Player"));
    oracles_achievements_home_login_end(l);
    OraclesAchievementsAccount kept;
    oracles_achievements_account_read(path, &kept);
    CHECK(!strcmp(kept.user, "Player") && !strcmp(kept.token, "TOKEN123"));
    remove(path);
    CHECK(oracles_achievements_offline_transport(folder, &transport));
    l = oracles_achievements_home_login_begin(&transport, "ua", NULL, "player", "secret", 1000.0);
    CHECK(l && oracles_achievements_home_login_poll(l, 1000.0, user, sizeof user) == 1);
    oracles_achievements_home_login_end(l);
    oracles_achievements_account_read(path, &kept);
    CHECK(!kept.user[0]);
    /* Refused. */
    write_answer(folder, "login2", "{\"Success\":false,\"Error\":\"Invalid User/Password combination. Please try again.\",\"Status\":401,"
                                   "\"Code\":\"invalid_credentials\"}");
    CHECK(oracles_achievements_offline_transport(folder, &transport));
    l = oracles_achievements_home_login_begin(&transport, "ua", path, "player", "wrong", 1000.0);
    CHECK(l && oracles_achievements_home_login_poll(l, 1000.0, user, sizeof user) == 0);
    oracles_achievements_home_login_end(l);
    oracles_achievements_account_read(path, &kept);
    CHECK(!kept.user[0]);
    /* No answer at all; no transport. */
    remove_answers(folder);
    CHECK(oracles_achievements_offline_transport(folder, &transport));
    l = oracles_achievements_home_login_begin(&transport, "ua", path, "player", "secret", 1000.0);
    CHECK(l && oracles_achievements_home_login_poll(l, 1000.0, user, sizeof user) == -1);
    oracles_achievements_home_login_end(l);
    l = oracles_achievements_home_login_begin(NULL, "ua", path, "player", "secret", 1000.0);
    CHECK(l && oracles_achievements_home_login_poll(l, 1000.0, user, sizeof user) == -1);
    oracles_achievements_home_login_end(l);
    /* An answer that never comes: not yet, then none after 30 s; given up while it waits, the transport destroyed. */
    silent quiet;
    memset(&quiet, 0, sizeof quiet);
    const OraclesAchievementsTransport never = { silent_send, NULL, silent_destroy, &quiet, NULL };
    l = oracles_achievements_home_login_begin(&never, "ua", path, "player", "secret", 1000.0);
    CHECK(l && oracles_achievements_home_login_poll(l, 2000.0, user, sizeof user) == -2);
    CHECK(oracles_achievements_home_login_poll(l, 1000.0 + ORACLES_ACHIEVEMENTS_LOGIN_WAIT_MS, user, sizeof user) == -1);
    oracles_achievements_home_login_end(l);
    CHECK(quiet.destroyed);
    memset(&quiet, 0, sizeof quiet);
    l = oracles_achievements_home_login_begin(&never, "ua", path, "player", "secret", 1000.0);
    CHECK(l && oracles_achievements_home_login_poll(l, 1100.0, user, sizeof user) == -2);
    oracles_achievements_home_login_end(l);
    CHECK(quiet.destroyed);
    oracles_achievements_account_read(path, &kept);
    CHECK(!kept.user[0]);
}

/* ---- the cost of a frame ------------------------------------------------------------------------------- */

/* 55 achievements of the kinds sets have: a flag's bit going up (a delta), a counter reaching a value with a hit
 * count, a room entered with a ResetIf, an AndNext chain with alternatives; and a rich presence of four lookups. */
static void bench(void)
{
    static char reply[64 * 1024];
    size_t used = (size_t)snprintf(reply, sizeof reply,
                                   "{\"Success\":true,\"GameId\":710,\"Title\":\"Bench\",\"ConsoleId\":6,\"ImageIconUrl\":\"\",\"RichPresenceGameId\":710,"
                                   "\"RichPresencePatch\":\"Lookup:Group\\n0=Present\\n1=Past\\n\\nLookup:Hearts\\n0=0\\n4=1\\n8=2\\n\\nDisplay:\\n"
                                   "In the @Group(0xH%x), room @Room(0xH%x), @Hearts(0xH%x) hearts, rupees @Rupees(0x %x)\","
                                   "\"Sets\":[{\"AchievementSetId\":1,\"GameId\":710,\"Title\":null,\"Type\":\"core\",\"ImageIconUrl\":\"\",\"Achievements\":[",
                                   group, room, 0xc6a2u, 0xc6a5u);
    for (unsigned i = 0; i < 55; i++) {
        const uint32_t base = 0xc600u + i * 3u, banked = 0x10000u + i * 7u;
        char logic[512];
        switch (i % 4) {
            case 0: snprintf(logic, sizeof logic, "0xM%x=1_d0xM%x=0_0xH%x=0", base, base, group); break;
            case 1: snprintf(logic, sizeof logic, "0xH%x>=%u.20._0xH%x=0_R:0xH%x=255", base, 100u + i, group, base + 1u); break;
            case 2: snprintf(logic, sizeof logic, "0xH%x=0_0xH%x=%u_d0xH%x!=%u_0xX%x>=100", group, room, 200u + i, room, 200u + i, banked); break;
            default: snprintf(logic, sizeof logic, "N:0xH%x=1_N:0xH%x=2_0xH%x=3SN:0xH%x=4_0xH%x=5S0xH%x=6_0xH%x=7", base, base + 1u, base + 2u, banked,
                              banked + 1u, banked + 2u, banked + 3u); break;
        }
        used += (size_t)snprintf(reply + used, sizeof reply - used,
                                 "%s{\"ID\":%u,\"Title\":\"A%u\",\"Description\":\"\",\"Flags\":3,\"Points\":5,\"MemAddr\":\"%s\",\"Author\":\"b\","
                                 "\"BadgeName\":\"00001\",\"Created\":1,\"Modified\":1}", i ? "," : "", 10000u + i, i, logic);
    }
    snprintf(reply + used, sizeof reply - used, "],\"Leaderboards\":[]}]}");
    server s;
    memset(&s, 0, sizeof s);
    s.login = "{\"Success\":true,\"User\":\"Player\",\"Token\":\"T\",\"Score\":0,\"SoftcoreScore\":0,\"Messages\":0}";
    s.game = reply;
    s.session = "{\"Success\":true,\"Unlocks\":[],\"HardcoreUnlocks\":[],\"ServerNow\":1700000000}";
    OraclesAchievements *a = client(&s);
    oracles_achievements_login(a, "Player", NULL, "T");
    oracles_achievements_load_game(a, "c4639cc61c049e5a085526bb6cac03bb", NULL);
    memset(wram, 0, sizeof wram);
    memory_of_buffers(oracles_achievements_memory(a));
    oracles_achievements_frame(a);
    const OraclesAchievementsEvent *loaded = find(&s, ORACLES_ACHIEVEMENTS_GAME_LOADED);
    if (!loaded || loaded->count != 55) { fprintf(stderr, "bench: the set did not load\n"); failures++; oracles_achievements_destroy(a); return; }
    const unsigned frames = 200000;
    uint32_t seed = 1;
    struct timespec start, end;
    timespec_get(&start, TIME_UTC);
    for (unsigned f = 0; f < frames; f++) {
        seed = seed * 1103515245u + 12345u;
        wram[0x600u + (seed >> 16) % 0x100u] = (uint8_t)(seed >> 8) & 0x0eu;   /* the memory moves, the set stays locked */
        oracles_achievements_frame(a);
    }
    timespec_get(&end, TIME_UTC);
    const double ns = ((double)(end.tv_sec - start.tv_sec) * 1e9 + (double)(end.tv_nsec - start.tv_nsec)) / frames;
    for (unsigned i = 0; i < s.event_count; i++) if (s.events[i].kind == ORACLES_ACHIEVEMENTS_UNLOCKED) { fprintf(stderr, "bench: an achievement was earned\n"); failures++; }
    printf("bench: %u frames, %.0f ns a frame with 55 achievements and a rich presence (%.3f %% of a 16.74 ms frame)\n", frames, ns,
           ns / 16742706.0 * 100.0);
    oracles_achievements_destroy(a);
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--bench")) { bench(); return failures ? 1 : 0; }
    const char *folder = argc > 1 ? argv[1] : ".";
    test_map();
    test_identity();
    test_session();
    test_offline_unlock(folder);
    test_suspension();
    test_badges(folder);
    test_list();
    test_tell();
    test_unreachable(folder);
    test_home_login(folder);
    test_load(folder);
    test_load_log(folder);
    test_load_hash(folder);
    test_page_load();
    test_load_silent();
    if (failures) return 1;
    puts("test_achievements: ok");
    return 0;
}
