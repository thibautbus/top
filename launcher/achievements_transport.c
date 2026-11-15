/* The developer's transport of the achievements' requests (achievements_transport.h). */
#include "achievements_transport.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct offline {
    char directory[1024];
} offline;

/* The value of `name`= in a form's `fields` (a=1&b=2), into `out`; empty when absent. */
static void field(const char *fields, const char *name, char *out, size_t capacity)
{
    const size_t length = strlen(name);
    out[0] = 0;
    for (const char *at = fields; at && *at; at = strchr(at, '&') ? strchr(at, '&') + 1 : NULL) {
        if (!strncmp(at, name, length) && at[length] == '=') {
            const char *value = at + length + 1;
            const size_t n = strcspn(value, "&");
            snprintf(out, capacity, "%.*s", (int)(n < capacity ? n : capacity - 1), value);
            return;
        }
    }
}

/* The fields without the password and the token, for the standard error. */
static void said(const char *fields, char *out, size_t capacity)
{
    size_t used = 0;
    out[0] = 0;
    for (const char *at = fields; at && *at && used + 1 < capacity;) {
        const size_t n = strcspn(at, "&");
        const int secret = (n >= 2 && (!strncmp(at, "p=", 2) || !strncmp(at, "t=", 2)));
        const int written = snprintf(out + used, capacity - used, "%s%.*s", used ? "&" : "", secret ? 2 : (int)n, at);
        if (written < 0 || (size_t)written >= capacity - used) break;
        used += (size_t)written;
        if (secret) used += (size_t)snprintf(out + used, capacity - used, "...");
        at += n;
        if (*at == '&') at++;
    }
}

static void offline_send(void *opaque, const char *url, const char *post_data, const char *content_type, const char *user_agent,
                         OraclesAchievementsAnswer answer, void *answer_opaque)
{
    const offline *o = opaque;
    (void)content_type;
    if (!post_data) {   /* an image: none offline */
        fprintf(stderr, "oracles: achievements (offline): GET %s: no server\n", url);
        answer(answer_opaque, 0, NULL, 0);
        return;
    }
    char name[64], shown[512];
    field(post_data, "r", name, sizeof name);
    said(post_data, shown, sizeof shown);
    fprintf(stderr, "oracles: achievements (offline): POST %s [%s] %s\n", url, user_agent, shown);
    /* Each request's name, in order, for what the tests say was never sent. */
    char log_path[sizeof o->directory + 16];
    snprintf(log_path, sizeof log_path, "%s/requests.log", o->directory);
    FILE *log = fopen(log_path, "ab");
    if (log) { fprintf(log, "%s\n", name); fclose(log); }
    for (const char *c = name; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9'))) name[0] = 0;
    char path[sizeof o->directory + 80];
    snprintf(path, sizeof path, "%s/%s.json", o->directory, name[0] ? name : "unnamed");
    FILE *f = name[0] ? fopen(path, "rb") : NULL;
    if (!f) { answer(answer_opaque, 0, NULL, 0); return; }
    uint8_t *body = NULL;
    size_t length = 0, capacity = 0;
    for (;;) {
        if (length + 1 >= capacity) {   /* room for the zero after the bytes */
            uint8_t *grown = realloc(body, capacity ? capacity * 2 : 4096);
            if (!grown) break;
            body = grown;
            capacity = capacity ? capacity * 2 : 4096;
        }
        const size_t got = fread(body + length, 1, capacity - length - 1, f);
        if (!got) break;
        length += got;
    }
    fclose(f);
    if (body) body[length] = 0;
    answer(answer_opaque, body ? 200 : 0, body, length);
    free(body);
}

int oracles_achievements_offline_transport(const char *directory, OraclesAchievementsTransport *transport)
{
    memset(transport, 0, sizeof *transport);
    offline *o = calloc(1, sizeof *o);
    if (!o) return 0;
    snprintf(o->directory, sizeof o->directory, "%s", directory);
    transport->send = offline_send;
    transport->destroy = free;
    transport->opaque = o;
    return 1;
}
