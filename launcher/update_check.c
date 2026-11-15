#include "update_check.h"

#include "update_fetch.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

#ifndef ORACLES_UPDATE_REPOSITORY
#error "ORACLES_UPDATE_REPOSITORY, config/version.json's repository, is set by CMake"
#endif

/* ---- the answer ------------------------------------------------------------------------------------------------ */

/* A JSON text read from `p` to `end`: only what a release needs, the values the check does not read skipped whole. */
typedef struct reader {
    const char *p, *end;
} reader;

#define MAX_DEPTH 64   /* objects and arrays nested deeper are refused: GitHub's go three deep */

static void skip_space(reader *r)
{
    while (r->p < r->end && (*r->p == ' ' || *r->p == '\t' || *r->p == '\n' || *r->p == '\r')) r->p++;
}

static int hex4(const char *p, unsigned *value)
{
    *value = 0;
    for (int i = 0; i < 4; i++) {
        const char c = p[i];
        const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (digit < 0) return 0;
        *value = *value * 16u + (unsigned)digit;
    }
    return 1;
}

/* A string at r->p, its escapes decoded into `out` (NULL: skipped): 1, or 0 when it is malformed or longer than
 * `capacity`. */
static int string(reader *r, char *out, size_t capacity)
{
    size_t n = 0;
    if (r->p >= r->end || *r->p != '"') return 0;
    r->p++;
    while (r->p < r->end && *r->p != '"') {
        unsigned char c = (unsigned char)*r->p++;
        char bytes[4];
        size_t count = 1;
        bytes[0] = (char)c;
        if (c < 0x20) return 0;
        if (c == '\\') {
            if (r->p >= r->end) return 0;
            const char e = *r->p++;
            if (e == 'u') {
                unsigned code;
                if (r->end - r->p < 4 || !hex4(r->p, &code)) return 0;
                r->p += 4;
                /* UTF-16's surrogates, by their top six bits: 110110 a pair's first, 110111 its second. */
                if (code >> 10 == 0x36) {
                    unsigned low;
                    if (r->end - r->p < 6 || r->p[0] != '\\' || r->p[1] != 'u' || !hex4(r->p + 2, &low) || low >> 10 != 0x37) return 0;
                    r->p += 6;
                    code = 0x10000u + ((code & 0x3ffu) << 10) + (low & 0x3ffu);
                } else if (code >> 10 == 0x37) return 0;
                if (code < 0x80) bytes[0] = (char)code;
                else if (code < 0x800) { bytes[0] = (char)(0xc0 | code >> 6); bytes[1] = (char)(0x80 | (code & 0x3f)); count = 2; }
                else if (code < 0x10000) {
                    bytes[0] = (char)(0xe0 | code >> 12); bytes[1] = (char)(0x80 | (code >> 6 & 0x3f)); bytes[2] = (char)(0x80 | (code & 0x3f));
                    count = 3;
                } else {
                    bytes[0] = (char)(0xf0 | code >> 18); bytes[1] = (char)(0x80 | (code >> 12 & 0x3f));
                    bytes[2] = (char)(0x80 | (code >> 6 & 0x3f)); bytes[3] = (char)(0x80 | (code & 0x3f));
                    count = 4;
                }
            } else {
                const char *from = "\"\\/bfnrt", *to = "\"\\/\b\f\n\r\t";
                const char *found = strchr(from, e);
                if (!e || !found) return 0;
                bytes[0] = to[found - from];
            }
        }
        if (out) {
            if (n + count >= capacity) return 0;
            memcpy(out + n, bytes, count);
            n += count;
        }
    }
    if (r->p >= r->end) return 0;
    r->p++;
    if (out) out[n] = 0;
    return 1;
}

static int literal(reader *r, const char *word)
{
    const size_t length = strlen(word);
    if ((size_t)(r->end - r->p) < length || memcmp(r->p, word, length) != 0) return 0;
    r->p += length;
    return 1;
}

static int number(reader *r)
{
    const char *start = r->p;
    if (r->p < r->end && *r->p == '-') r->p++;
    const char *digits = r->p;
    while (r->p < r->end && ((*r->p >= '0' && *r->p <= '9') || *r->p == '.' || *r->p == 'e' || *r->p == 'E' || *r->p == '+' || *r->p == '-')) r->p++;
    return r->p > digits && r->p > start;
}

static int boolean(reader *r, int *value)
{
    if (literal(r, "true")) { *value = 1; return 1; }
    if (literal(r, "false")) { *value = 0; return 1; }
    return 0;
}

/* Any value at r->p, skipped. */
static int value(reader *r, int depth)
{
    skip_space(r);
    if (r->p >= r->end) return 0;
    const char c = *r->p;
    if (c == '"') return string(r, NULL, 0);
    if (c == 't') return literal(r, "true");
    if (c == 'f') return literal(r, "false");
    if (c == 'n') return literal(r, "null");
    if (c != '{' && c != '[') return number(r);
    if (depth >= MAX_DEPTH) return 0;
    const char close = c == '{' ? '}' : ']';
    r->p++;
    skip_space(r);
    if (r->p < r->end && *r->p == close) { r->p++; return 1; }
    for (;;) {
        if (c == '{') {
            skip_space(r);
            if (!string(r, NULL, 0)) return 0;
            skip_space(r);
            if (r->p >= r->end || *r->p != ':') return 0;
            r->p++;
        }
        if (!value(r, depth + 1)) return 0;
        skip_space(r);
        if (r->p >= r->end) return 0;
        if (*r->p == close) { r->p++; return 1; }
        if (*r->p != ',') return 0;
        r->p++;
    }
}

int oracles_update_parse_release(const char *json, size_t length, OraclesRelease *release)
{
    reader r = { json, json + length };
    memset(release, 0, sizeof *release);
    skip_space(&r);
    if (r.p >= r.end || *r.p != '{') return 0;
    r.p++;
    skip_space(&r);
    if (r.p < r.end && *r.p == '}') r.p++;
    else for (;;) {
        char key[32];
        skip_space(&r);
        const char *start = r.p;
        if (!string(&r, key, sizeof key)) {
            r.p = start;
            if (!string(&r, NULL, 0)) return 0;   /* a key longer than any the check reads */
            key[0] = 0;
        }
        skip_space(&r);
        if (r.p >= r.end || *r.p != ':') return 0;
        r.p++;
        skip_space(&r);
        if (!strcmp(key, "tag_name")) {
            if (!string(&r, release->tag, sizeof release->tag)) return 0;
            release->has_tag = 1;
        } else if (!strcmp(key, "html_url")) {
            if (!string(&r, release->url, sizeof release->url)) return 0;
            release->has_url = 1;
        } else if (!strcmp(key, "draft")) {
            if (!boolean(&r, &release->draft)) return 0;
        } else if (!strcmp(key, "prerelease")) {
            if (!boolean(&r, &release->prerelease)) return 0;
        } else if (!value(&r, 1)) return 0;
        skip_space(&r);
        if (r.p >= r.end) return 0;
        if (*r.p == '}') { r.p++; break; }
        if (*r.p != ',') return 0;
        r.p++;
    }
    skip_space(&r);
    return r.p == r.end;
}

int oracles_update_parse_version(const char *text, unsigned version[3])
{
    const char *p = text;
    if (*p == 'v') p++;
    for (int i = 0; i < 3; i++) {
        unsigned n = 0;
        const char *digits = p;
        while (*p >= '0' && *p <= '9') {
            if (n > 99999u) return 0;
            n = n * 10u + (unsigned)(*p++ - '0');
        }
        if (p == digits || (p - digits > 1 && *digits == '0')) return 0;
        version[i] = n;
        if (i < 2 && *p++ != '.') return 0;
    }
    if (*p == '-') return p[1] ? -1 : 0;
    return *p == 0 || (*p == '+' && p[1]) ? 1 : 0;
}

int oracles_update_compare(const unsigned a[3], const unsigned b[3])
{
    for (int i = 0; i < 3; i++)
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}

OraclesUpdateState oracles_update_evaluate(const char *running, int http_status, const char *error, const char *body, size_t length,
                                           OraclesUpdateStatus *status, char *reason, size_t capacity)
{
    OraclesRelease release;
    unsigned latest[3], own[3];
    memset(status, 0, sizeof *status);
    status->state = ORACLES_UPDATE_FAILED;
    reason[0] = 0;
    if (http_status == 0) snprintf(reason, capacity, "no answer (%s)", error && error[0] ? error : "unknown");
    else if (http_status == 403 || http_status == 429) snprintf(reason, capacity, "refused, the rate limit reached (HTTP %d)", http_status);
    else if (http_status == 404) snprintf(reason, capacity, "no release published (HTTP 404)");
    else if (http_status != 200) snprintf(reason, capacity, "HTTP %d", http_status);
    else if (!body || !oracles_update_parse_release(body, length, &release)) snprintf(reason, capacity, "a malformed answer");
    else if (!release.has_tag) snprintf(reason, capacity, "an answer without tag_name");
    else if (release.draft) snprintf(reason, capacity, "the latest release, %s, is a draft: ignored", release.tag);
    else if (release.prerelease) snprintf(reason, capacity, "the latest release, %s, is a pre-release: ignored", release.tag);
    else {
        const int parsed = oracles_update_parse_version(release.tag, latest);
        if (parsed < 0) snprintf(reason, capacity, "the latest release's tag, %s, is a pre-release's: ignored", release.tag);
        else if (parsed == 0) snprintf(reason, capacity, "the latest release's tag, %s, is not a version", release.tag);
        else if (oracles_update_parse_version(running, own) != 1) snprintf(reason, capacity, "the running version, %s, is not one", running);
        else if (oracles_update_compare(latest, own) <= 0) {
            status->state = ORACLES_UPDATE_CURRENT;
            snprintf(reason, capacity, "up to date, the latest release is %s", release.tag);
        } else if (!release.has_url || strncmp(release.url, "https://", 8) != 0)
            snprintf(reason, capacity, "the latest release, %s, has no https:// page", release.tag);
        else {
            status->state = ORACLES_UPDATE_NEWER;
            snprintf(status->version, sizeof status->version, "%u.%u.%u", latest[0], latest[1], latest[2]);
            snprintf(status->url, sizeof status->url, "%s", release.url);
        }
    }
    return status->state;
}

/* ---- the check ------------------------------------------------------------------------------------------------- */

static struct {
    SDL_Mutex *lock;
    SDL_Condition *ended;
    OraclesUpdateStatus status;   /* under the lock, with running and events */
    int started, running, events;
    Uint32 event;
    SDL_AtomicInt cancel;
    char source[2048], version[24];
} check;

const char *oracles_update_repository(void) { return ORACLES_UPDATE_REPOSITORY; }

const char *oracles_update_default_url(void)
{
    return "https://api.github.com/repos/" ORACLES_UPDATE_REPOSITORY "/releases/latest";
}

/* The GET of GitHub's API: its Accept header, a User-Agent without the version. */
static int fetch(const char *url, char **body, size_t *length, int *status, char *error, size_t capacity)
{
    const OraclesHttpRequest request = { url, NULL, 0, NULL, ORACLES_UPDATE_ACCEPT, ORACLES_UPDATE_USER_AGENT, ORACLES_UPDATE_TIMEOUT_MS,
                                         ORACLES_UPDATE_MAX_BODY, &check.cancel, 0 };
    return oracles_http_request(&request, body, length, status, error, capacity);
}

static int run(void *opaque)
{
    (void)opaque;
    char *body = NULL, error[256] = "", reason[384];
    size_t length = 0;
    int http_status = 0;
    if (!strncmp(check.source, "https://", 8)) {
        if (!fetch(check.source, &body, &length, &http_status, error, sizeof error))
            http_status = 0;
    } else {
        /* --update-check-url's file, as GitHub would answer. */
        body = SDL_LoadFile(check.source, &length);
        if (!body) snprintf(error, sizeof error, "the file cannot be read: %s", SDL_GetError());
        else if (length > ORACLES_UPDATE_MAX_BODY) snprintf(error, sizeof error, "the file is larger than %d bytes", ORACLES_UPDATE_MAX_BODY);
        else http_status = 200;
    }
    OraclesUpdateStatus result;
    oracles_update_evaluate(check.version, http_status, error, body, length, &result, reason, sizeof reason);
    SDL_free(body);
    if (result.state == ORACLES_UPDATE_NEWER) fprintf(stderr, "oracles: update check: version %s available\n", result.version);
    else fprintf(stderr, "oracles: update check: %s%s\n", result.state == ORACLES_UPDATE_CURRENT ? "" : "nothing shown, ", reason);
    SDL_LockMutex(check.lock);
    check.status = result;
    check.running = 0;
    if (check.events && check.event) {
        SDL_Event e;
        SDL_zero(e);
        e.type = check.event;
        SDL_PushEvent(&e);
    }
    SDL_BroadcastCondition(check.ended);
    SDL_UnlockMutex(check.lock);
    return 0;
}

void oracles_update_open_failed(const char *url, char *message, size_t capacity)
{
    const char *shown = !strncmp(url, "https://", 8) ? url + 8 : url;
    snprintf(message, capacity, "No browser opened the release page: %s", shown);
}

void oracles_update_check_start(const char *source, const char *running)
{
    if (check.started) return;
    check.started = 1;
    check.status.state = ORACLES_UPDATE_FAILED;
    check.lock = SDL_CreateMutex();
    check.ended = SDL_CreateCondition();
    if (!check.lock || !check.ended || strlen(source) >= sizeof check.source) {
        fprintf(stderr, "oracles: update check: nothing shown, it cannot start\n");
        return;
    }
    check.event = SDL_RegisterEvents(1);
    snprintf(check.source, sizeof check.source, "%s", source);
    snprintf(check.version, sizeof check.version, "%s", running);
    check.status.state = ORACLES_UPDATE_CHECKING;
    check.running = 1;
    fprintf(stderr, "oracles: update check: asking for the latest release, version %s running\n", running);
    SDL_Thread *thread = SDL_CreateThread(run, "update check", NULL);
    if (!thread) {
        check.status.state = ORACLES_UPDATE_FAILED;
        check.running = 0;
        fprintf(stderr, "oracles: update check: nothing shown, its thread cannot start: %s\n", SDL_GetError());
        return;
    }
    SDL_DetachThread(thread);
}

void oracles_update_check_status(OraclesUpdateStatus *status)
{
    if (!check.lock) {
        memset(status, 0, sizeof *status);
        status->state = check.started ? ORACLES_UPDATE_FAILED : ORACLES_UPDATE_UNKNOWN;
        return;
    }
    SDL_LockMutex(check.lock);
    *status = check.status;
    SDL_UnlockMutex(check.lock);
}

int oracles_update_check_wait(int timeout_ms)
{
    if (!check.lock) return 1;
    const Uint64 until = SDL_GetTicks() + (Uint64)(timeout_ms > 0 ? timeout_ms : 0);
    SDL_LockMutex(check.lock);
    while (check.running) {
        const Uint64 now = SDL_GetTicks();
        if (now >= until) break;
        SDL_WaitConditionTimeout(check.ended, check.lock, (Sint32)(until - now));
    }
    const int ended = !check.running;
    SDL_UnlockMutex(check.lock);
    return ended;
}

uint32_t oracles_update_check_event(void)
{
    return check.event;
}

void oracles_update_check_events(int on)
{
    if (!check.lock) return;
    SDL_LockMutex(check.lock);
    check.events = on;
    SDL_UnlockMutex(check.lock);
}

void oracles_update_check_close(void)
{
    if (!check.lock) return;
    oracles_update_check_events(0);
    SDL_SetAtomicInt(&check.cancel, 1);
    oracles_update_check_wait(1000);
}

int oracles_update_check_open(char *message, size_t capacity)
{
    OraclesUpdateStatus status;
    if (capacity) message[0] = 0;
    oracles_update_check_status(&status);
    if (status.state != ORACLES_UPDATE_NEWER) return 0;
    if (!SDL_OpenURL(status.url)) {
        fprintf(stderr, "oracles: update check: the release page cannot be opened: %s\n", SDL_GetError());
        oracles_update_open_failed(status.url, message, capacity);
        return 0;
    }
    fprintf(stderr, "oracles: update check: the release page opened\n");
    return 1;
}
