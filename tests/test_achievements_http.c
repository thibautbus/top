/* RetroAchievements' transport (achievements_http.h), no server of the network asked: its queue and its thread on a
 * fetch of the test's own (the answers given by poll alone, on the caller's thread; the POSTs before the GETs; at the
 * end every request not answered answered as given up, -1, the thread waited for; no answer as status 0; an image
 * written to its file by the thread, none asked when the file is there); and on Linux the system's libcurl against a server of
 * the test's on 127.0.0.1: a POST's body, Content-Type and User-Agent as sent, a GET's bytes whole, a timeout, an
 * http:// address refused without the tests' flag, and a whole session of rc_client (login, game, unlock, its award)
 * through it. */
#if defined(__linux__) && !defined(__ANDROID__)
#define _GNU_SOURCE   /* strcasestr */
#endif
#include "achievements_http.h"

#include "achievements_files.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

/* ---- the queue, on a fetch of the test's own -------------------------------------------------------------- */

static SDL_Mutex *order_lock;
static char order[8][64];
static int order_count;
static SDL_Semaphore *first_held;

static int fake_fetch(const OraclesHttpRequest *request, char **body, size_t *length, int *status, char *error, size_t capacity)
{
    SDL_LockMutex(order_lock);
    const int index = order_count;
    if (order_count < 8) snprintf(order[order_count++], sizeof order[0], "%s %s", request->body ? "POST" : "GET", request->url);
    SDL_UnlockMutex(order_lock);
    if (index == 0) SDL_WaitSemaphore(first_held);   /* the others queue meanwhile */
    if (strstr(request->url, "/none")) { snprintf(error, capacity, "no server"); return 0; }
    static const char image[6] = { 'P', 0, 'N', 0, 'G', 0 };
    static const char png[12] = { (char)0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n', 0, 0, 0, 0 };
    const int is_png = strstr(request->url, "/png") != NULL;
    const char *from = request->body ? request->body : is_png ? png : image;
    const size_t n = request->body ? request->body_length : is_png ? sizeof png : sizeof image;
    *body = SDL_malloc(n + 1);
    memcpy(*body, from, n);
    (*body)[n] = 0;
    *length = n;
    *status = 200;
    return 1;
}

typedef struct got {
    int calls, status;
    size_t length;
    char body[256];
    SDL_ThreadID thread;
} got;

static void answer(void *opaque, int http_status, const uint8_t *body, size_t length)
{
    got *g = opaque;
    g->calls++;
    g->status = http_status;
    g->length = length;
    if (body && length < sizeof g->body) memcpy(g->body, body, length);
    g->thread = SDL_GetCurrentThreadID();
}

/* Polls until `g` has an answer, twenty seconds at most: a loaded machine is slow, never wrong. */
static void poll_until(OraclesAchievementsTransport *t, const got *g)
{
    for (int i = 0; i < 2000 && !g->calls; i++) { SDL_Delay(10); t->poll(t->opaque); }
}

/* A request that takes its time, as a client without cancelling does. */
static SDL_AtomicInt slow_started;

static int slow_fetch(const OraclesHttpRequest *request, char **body, size_t *length, int *status, char *error, size_t capacity)
{
    (void)request; (void)body; (void)length; (void)error; (void)capacity;
    SDL_SetAtomicInt(&slow_started, 1);
    SDL_Delay(1200);
    *status = 0;
    return 1;
}

/* The page's transport leaves a request under way at its end, the thread finishing it on its own: the interface does not
 * wait for it; a session's waits, as it did. */
static void test_leave_at_stop(void)
{
    for (int leave = 0; leave < 2; leave++) {
        OraclesAchievementsHttpOptions options = { slow_fetch, 5000, 0, leave };
        OraclesAchievementsTransport t;
        got g;
        memset(&g, 0, sizeof g);
        SDL_SetAtomicInt(&slow_started, 0);
        CHECK(oracles_achievements_http_transport(&t, &options));
        t.send(t.opaque, "https://example.invalid/slow", "r=startsession", "application/x-www-form-urlencoded", "agent", answer, &g);
        while (!SDL_GetAtomicInt(&slow_started)) SDL_Delay(1);
        const Uint64 before = SDL_GetTicks();
        t.destroy(t.opaque);
        const Uint64 waited = SDL_GetTicks() - before;
        /* The request under way: given up as the end waited for it, dropped with the thread when it did not. */
        CHECK(leave ? g.calls == 0 : g.calls == 1 && g.status == -1);
        CHECK(leave ? waited < 300u : waited >= 300u);
    }
    SDL_Delay(1300);   /* the thread left behind gone before the next test */
}

static void test_queue(const char *folder)
{
    order_lock = SDL_CreateMutex();
    first_held = SDL_CreateSemaphore(0);
    const OraclesAchievementsHttpOptions options = { fake_fetch, 5000, 0, 0 };
    OraclesAchievementsTransport t;
    CHECK(oracles_achievements_http_transport(&t, &options));
    got first = { 0 }, image = { 0 }, post = { 0 }, none = { 0 };
    t.send(t.opaque, "https://example.invalid/first", "r=first", "application/x-www-form-urlencoded", "agent", answer, &first);
    SDL_Delay(50);   /* the thread holds the first */
    t.send(t.opaque, "https://example.invalid/image.png", NULL, NULL, "agent", answer, &image);
    t.send(t.opaque, "https://example.invalid/post", "r=second", "application/x-www-form-urlencoded", "agent", answer, &post);
    t.send(t.opaque, "https://example.invalid/none", "r=none", "application/x-www-form-urlencoded", "agent", answer, &none);
    CHECK(!first.calls && !image.calls && !post.calls);   /* never from send */
    SDL_SignalSemaphore(first_held);
    poll_until(&t, &image);
    poll_until(&t, &none);
    CHECK(first.calls == 1 && first.status == 200 && !strcmp(first.body, "r=first") && first.thread == SDL_GetCurrentThreadID());
    CHECK(post.calls == 1 && !strcmp(post.body, "r=second"));
    CHECK(image.calls == 1 && image.length == 6 && !memcmp(image.body, "P\0N\0G\0", 6));
    CHECK(none.calls == 1 && none.status == 0 && none.length == 0);
    CHECK(order_count == 4 && !strcmp(order[1], "POST https://example.invalid/post") && !strcmp(order[2], "POST https://example.invalid/none")
          && !strcmp(order[3], "GET https://example.invalid/image.png"));   /* the POSTs first */

    /* At the end, what waits and what the thread was sending are answered as given up, -1, before destroy returns. */
    got held = { 0 }, dropped = { 0 }, after = { 0 };
    order_count = 0;
    t.send(t.opaque, "https://example.invalid/held", "r=held", "application/x-www-form-urlencoded", "agent", answer, &held);
    t.send(t.opaque, "https://example.invalid/dropped", "r=dropped", "application/x-www-form-urlencoded", "agent", answer, &dropped);
    SDL_Delay(50);
    SDL_SignalSemaphore(first_held);   /* the request being sent ends within the wait */
    t.destroy(t.opaque);
    CHECK(held.calls == 1 && held.status == -1 && dropped.calls == 1 && dropped.status == -1);
    (void)after;

    /* An image the thread writes itself; none asked for when its file is there. */
    char path[1024];
    snprintf(path, sizeof path, "%s/test_achievements_http.png", folder);
    remove(path);
    CHECK(oracles_achievements_http_transport(&t, &options));
    got saved = { 0 }, again = { 0 };
    order_count = 0;
    SDL_SignalSemaphore(first_held);   /* the first request is not held this time */
    t.save(t.opaque, "https://example.invalid/png", path, "agent", answer, &saved);
    poll_until(&t, &saved);
    CHECK(saved.calls == 1 && saved.status == 200 && saved.length == 0 && oracles_achievements_file_exists(path));
    t.save(t.opaque, "https://example.invalid/png", path, "agent", answer, &again);
    poll_until(&t, &again);
    CHECK(again.status == 200 && order_count == 1);
    t.destroy(t.opaque);
    remove(path);
}

/* ---- the system's libcurl against a server on 127.0.0.1 ------------------------------------------------------ */

#if defined(__linux__) && !defined(__ANDROID__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "achievements.h"

static int listener, port;
static SDL_AtomicInt serving;
static char seen_head[2048], seen_body[1024];
static SDL_Mutex *seen_lock;

static const char *game_reply =
    "{\"Success\":true,\"GameId\":1,\"Title\":\"Test\",\"ConsoleId\":6,\"ImageIconUrl\":\"\",\"RichPresenceGameId\":1,\"RichPresencePatch\":\"\","
    "\"Sets\":[{\"AchievementSetId\":1,\"GameId\":1,\"Title\":null,\"Type\":\"core\",\"ImageIconUrl\":\"\",\"Achievements\":["
    "{\"ID\":77,\"Title\":\"Byte\",\"Description\":\"\",\"Flags\":3,\"Points\":5,\"MemAddr\":\"0xHc001=3_d0xHc001=0\",\"Author\":\"t\","
    "\"BadgeName\":\"00077\",\"Created\":1,\"Modified\":1}],\"Leaderboards\":[]}]}";

static void reply(int fd, int code, const char *type, const char *body, size_t length)
{
    char head[256];
    const int n = snprintf(head, sizeof head, "HTTP/1.1 %d X\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", code, type, length);
    if (write(fd, head, (size_t)n) < 0 || (length && write(fd, body, length) < 0)) return;
}

static int serve(void *opaque)
{
    (void)opaque;
    while (SDL_GetAtomicInt(&serving)) {
        const int fd = accept(listener, NULL, NULL);
        if (fd < 0) continue;
        char request[8192];
        size_t used = 0;
        char *end = NULL;
        while (used < sizeof request - 1) {   /* the head, then the body its Content-Length says */
            const ssize_t got = read(fd, request + used, sizeof request - 1 - used);
            if (got <= 0) break;
            used += (size_t)got;
            request[used] = 0;
            if (!end) end = strstr(request, "\r\n\r\n");
            if (end) {
                const char *field = strcasestr(request, "Content-Length:");
                const size_t want = field && field < end ? strtoul(field + 15, NULL, 10) : 0;
                if (used >= (size_t)(end - request) + 4 + want) break;
            }
        }
        request[used] = 0;
        const char *body = end ? end + 4 : "";
        SDL_LockMutex(seen_lock);
        snprintf(seen_head, sizeof seen_head, "%.*s", end ? (int)(end - request) : (int)used, request);
        snprintf(seen_body, sizeof seen_body, "%s", body);
        SDL_UnlockMutex(seen_lock);
        static const char image[8] = { (char)0x89, 'P', 'N', 'G', 0, 0, 1, 2 };
        if (strstr(request, "GET /slow")) { SDL_Delay(1500); reply(fd, 200, "text/plain", "late", 4); }
        else if (strstr(request, "GET /busy")) reply(fd, 503, "text/plain", "", 0);
        else if (!strncmp(request, "GET ", 4)) reply(fd, 200, "image/png", image, sizeof image);
        else if (strstr(body, "r=login2")) {
            const char *r = "{\"Success\":true,\"User\":\"Player\",\"Token\":\"T0K\",\"Score\":0,\"SoftcoreScore\":0,\"Messages\":0}";
            reply(fd, 200, "application/json", r, strlen(r));
        } else if (strstr(body, "r=achievementsets")) reply(fd, 200, "application/json", game_reply, strlen(game_reply));
        else if (strstr(body, "r=awardachievement")) {
            const char *r = "{\"Success\":true,\"Score\":0,\"SoftcoreScore\":5,\"AchievementID\":77,\"AchievementsRemaining\":0}";
            reply(fd, 200, "application/json", r, strlen(r));
        } else {
            const char *r = "{\"Success\":true,\"Unlocks\":[],\"HardcoreUnlocks\":[],\"ServerNow\":1700000000}";
            reply(fd, 200, "application/json", r, strlen(r));
        }
        close(fd);
    }
    return 0;
}

typedef struct session_events {
    int logged_in, loaded, unlocked, settled, delivered;
} session_events;

static void events(void *opaque, const OraclesAchievementsEvent *e)
{
    session_events *s = opaque;
    if (e->kind == ORACLES_ACHIEVEMENTS_LOGGED_IN) s->logged_in = 1;
    if (e->kind == ORACLES_ACHIEVEMENTS_GAME_LOADED) s->loaded = 1;
    if (e->kind == ORACLES_ACHIEVEMENTS_UNLOCKED) s->unlocked = 1;
    if (e->kind == ORACLES_ACHIEVEMENTS_AWARD_SETTLED) { s->settled = 1; s->delivered = e->delivered; }
}

static void test_local_server(void)
{
    listener = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t size = sizeof address;
    if (listener < 0 || bind(listener, (struct sockaddr *)&address, sizeof address) != 0 || listen(listener, 8) != 0
        || getsockname(listener, (struct sockaddr *)&address, &size) != 0) { fprintf(stderr, "test_achievements_http: no local server, skipped\n"); return; }
    port = ntohs(address.sin_port);
    seen_lock = SDL_CreateMutex();
    SDL_SetAtomicInt(&serving, 1);
    SDL_Thread *server = SDL_CreateThread(serve, "server", NULL);
    char base[64], url[128];
    snprintf(base, sizeof base, "http://127.0.0.1:%d", port);

    const OraclesAchievementsHttpOptions options = { NULL, 5000, 1, 0 }, hasty = { NULL, 500, 1, 0 };
    OraclesAchievementsTransport t, quick;
    CHECK(oracles_achievements_http_transport(&t, &options));
    got post = { 0 }, image = { 0 }, slow = { 0 }, busy = { 0 };
    snprintf(url, sizeof url, "%s/dorequest.php", base);
    t.send(t.opaque, url, "r=startsession&u=Player&t=T0K&g=1", "application/x-www-form-urlencoded", "the-oracles-project/1.5.0 (Linux) rcheevos/12.5", answer, &post);
    poll_until(&t, &post);
    SDL_LockMutex(seen_lock);
    CHECK(post.calls == 1 && post.status == 200 && strstr(post.body, "\"Success\":true"));
    CHECK(!strncmp(seen_head, "POST /dorequest.php ", 20) && strcasestr(seen_head, "Content-Type: application/x-www-form-urlencoded")
          && strcasestr(seen_head, "User-Agent: the-oracles-project/1.5.0 (Linux) rcheevos/12.5") && !strcmp(seen_body, "r=startsession&u=Player&t=T0K&g=1"));
    SDL_UnlockMutex(seen_lock);
    snprintf(url, sizeof url, "%s/Badge/00077.png", base);
    t.send(t.opaque, url, NULL, NULL, "agent", answer, &image);
    poll_until(&t, &image);
    CHECK(image.status == 200 && image.length == 8 && !memcmp(image.body, "\x89PNG\0\0\1\2", 8));   /* bytes whole, zeros too */
    snprintf(url, sizeof url, "%s/slow", base);
    CHECK(oracles_achievements_http_transport(&quick, &hasty));
    quick.send(quick.opaque, url, NULL, NULL, "agent", answer, &slow);
    poll_until(&quick, &slow);
    CHECK(slow.calls == 1 && slow.status == 0);   /* past the timeout: as no server */
    quick.destroy(quick.opaque);
    SDL_Delay(1500);   /* the server, one request at a time, done with it */
    snprintf(url, sizeof url, "%s/busy", base);
    t.send(t.opaque, url, NULL, NULL, "agent", answer, &busy);
    poll_until(&t, &busy);
    CHECK(busy.status == 503);
    t.destroy(t.opaque);

    /* http:// without the tests' flag: refused before anything is sent. */
    const OraclesAchievementsHttpOptions strict = { NULL, 5000, 0, 0 };
    CHECK(oracles_achievements_http_transport(&t, &strict));
    got refused = { 0 };
    snprintf(url, sizeof url, "%s/dorequest.php", base);
    t.send(t.opaque, url, "r=login2", "application/x-www-form-urlencoded", "agent", answer, &refused);
    poll_until(&t, &refused);
    CHECK(refused.calls == 1 && refused.status == 0);
    t.destroy(t.opaque);

    /* rc_client through it all: login, the game, an unlock at the frame its byte changes, the award acknowledged. */
    CHECK(oracles_achievements_http_transport(&t, &options));
    session_events s = { 0 };
    OraclesAchievements *a = oracles_achievements_create(&t, "the-oracles-project/1.5.0 (Linux) rcheevos/12.5", events, &s);
    oracles_achievements_set_host(a, base);
    static uint8_t wram[0x8000];
    oracles_achievements_login(a, "Player", NULL, "T0K");
    oracles_achievements_load_game(a, "00000000000000000000000000000001", NULL);
    OraclesAchievementsMemory *m = oracles_achievements_memory(a);
    m->wram = wram;
    for (int i = 0; i < 300 && !oracles_achievements_watching(a); i++) { SDL_Delay(10); oracles_achievements_frame(a); }
    CHECK(s.logged_in && s.loaded);
    oracles_achievements_frame(a);
    wram[1] = 3;
    oracles_achievements_frame(a);
    for (int i = 0; i < 300 && !s.settled; i++) { SDL_Delay(10); oracles_achievements_frame(a); }
    CHECK(s.unlocked && s.settled && s.delivered);
    SDL_LockMutex(seen_lock);
    CHECK(strstr(seen_body, "r=awardachievement") && strstr(seen_body, "a=77") && strstr(seen_body, "h=0"));
    SDL_UnlockMutex(seen_lock);
    oracles_achievements_destroy(a);

    SDL_SetAtomicInt(&serving, 0);
    const int wake = socket(AF_INET, SOCK_STREAM, 0);   /* the server's accept returns */
    if (wake >= 0) { connect(wake, (struct sockaddr *)&address, sizeof address); close(wake); }
    SDL_WaitThread(server, NULL);
    close(listener);
}
#endif

int main(int argc, char **argv)
{
    test_queue(argc > 1 ? argv[1] : ".");
    test_leave_at_stop();
#if defined(__linux__) && !defined(__ANDROID__)
    test_local_server();
#endif
    if (failures) return 1;
    puts("test_achievements_http: ok");
    return 0;
}
