/* One HTTPS request with the platform's own client and the system's TLS, the server's certificate checked: WinHTTP on
 * Windows, NSURLSession on macOS (macos/update_fetch.m), the libcurl the system has on Linux, loaded at run time, none
 * otherwise, and HttpsURLConnection on Android (OraclesActivity.httpRequest).  Nothing is sent but the request: the
 * address, the method, its body and Content-Type, an Accept header when one is given and the User-Agent.  A
 * redirection is followed, to https:// only. */
#ifndef ORACLES_LAUNCHER_UPDATE_FETCH_H
#define ORACLES_LAUNCHER_UPDATE_FETCH_H

#include <SDL3/SDL_atomic.h>

#include <stddef.h>

#define ORACLES_UPDATE_USER_AGENT "the-oracles-project"
#define ORACLES_UPDATE_ACCEPT "application/vnd.github+json"

typedef struct OraclesHttpRequest {
    const char *url;               /* https:// */
    const char *body;              /* a POST of `body_length` bytes as `content_type`; NULL: a GET */
    size_t body_length;
    const char *content_type;
    const char *accept;            /* the Accept header, or NULL for none */
    const char *user_agent;
    int timeout_ms;                /* for the connection, and for the whole request, or about */
    size_t max_body;               /* the largest answer read */
    SDL_AtomicInt *cancel;         /* set to 1 from another thread, ends the request early where the client allows it (Linux) */
    int loopback_http;             /* the tests' local server: http://127.0.0.1 accepted too; never set otherwise */
} OraclesHttpRequest;

/* Sends `request` (the update check's GET, RetroAchievements' requests): 1 when a server answered, with *status its
 * HTTP status and *body what it sent, at most max_body bytes, zero-terminated (an image's bytes too, `length` of them),
 * to free with SDL_free; 0 otherwise, with the reason in `error`.  Blocks: called on a thread of its own. */
int oracles_http_request(const OraclesHttpRequest *request, char **body, size_t *length, int *status, char *error, size_t capacity);

#endif
