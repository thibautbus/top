/* The update check: when the home screen opens, one request in the background, never waited for, to GitHub for the
 * latest release published of the public repository (config/version.json's `repository`), whose tag is compared with
 * the running version (config/version.json's `version`); a newer one is offered to the home screen, its version and
 * its page.  update_check=off in settings.txt asks nothing.  A capture, a game --rom starts, the harness and the tests
 * never ask the network: --update-check-url gives them a file (or another address) to read instead.
 *
 * Each platform asks with its own client and the system's TLS, its certificates checked (update_fetch.h). */
#ifndef ORACLES_LAUNCHER_UPDATE_CHECK_H
#define ORACLES_LAUNCHER_UPDATE_CHECK_H

#include <stddef.h>
#include <stdint.h>

/* The milliseconds a check may take, the connection included. */
#define ORACLES_UPDATE_TIMEOUT_MS 8000
/* The largest answer read: GitHub's release is some 15 KB, its notes included. */
#define ORACLES_UPDATE_MAX_BODY (1024 * 1024)

typedef enum OraclesUpdateState {
    ORACLES_UPDATE_UNKNOWN,    /* no check: update_check=off, or none started */
    ORACLES_UPDATE_CHECKING,   /* asked, no answer yet */
    ORACLES_UPDATE_CURRENT,    /* the latest release is this version or an older one */
    ORACLES_UPDATE_NEWER,      /* a newer release: its version and its page */
    ORACLES_UPDATE_FAILED      /* no answer that says: offline, an HTTP error, the rate limit, a malformed answer, a draft
                                * or a pre-release; nothing is shown, the standard error says why */
} OraclesUpdateState;

typedef struct OraclesUpdateStatus {
    OraclesUpdateState state;
    char version[24];   /* NEWER: the release's, "1.2.0", without the tag's v */
    char url[256];      /* NEWER: the release's page (its html_url), always https:// */
} OraclesUpdateStatus;

/* The fields of a release the check reads, from the top level of GitHub's answer; the others, and the nested objects'
 * fields of the same names (an author's html_url), are skipped. */
typedef struct OraclesRelease {
    char tag[64];       /* tag_name */
    char url[256];      /* html_url */
    int draft, prerelease;
    int has_tag, has_url;
} OraclesRelease;

/* Reads `json` (`length` bytes, not necessarily terminated) as a JSON object: 1 with *release, 0 when it is not one,
 * or is cut, or a field the check reads has the wrong type. */
int oracles_update_parse_release(const char *json, size_t length, OraclesRelease *release);
/* "X.Y.Z" or "vX.Y.Z", build metadata after a + ignored: 1 with version[3]; -1 for a pre-release ("1.2.0-rc.1");
 * 0 otherwise. */
int oracles_update_parse_version(const char *text, unsigned version[3]);
/* <0, 0 or >0 as `a` is older than, the same as or newer than `b`. */
int oracles_update_compare(const unsigned a[3], const unsigned b[3]);
/* What an answer says for the version `running` ("1.1.0"): `http_status` (0: no answer, `error` its reason), and
 * `body`.  Sets *status and, but for NEWER, the reason in `reason`; returns status->state. */
OraclesUpdateState oracles_update_evaluate(const char *running, int http_status, const char *error, const char *body, size_t length,
                                           OraclesUpdateStatus *status, char *reason, size_t capacity);

/* The project's repository on GitHub, OWNER/NAME (config/version.json's). */
const char *oracles_update_repository(void);
/* The address asked by default: https://api.github.com/repos/<repository>/releases/latest. */
const char *oracles_update_default_url(void);
/* Starts the check in a thread of its own, once a run: `source` an https:// address or a file to read (an answer as
 * GitHub gives it), `running` the version compared with.  Returns at once; the standard error says the result. */
void oracles_update_check_start(const char *source, const char *running);
/* The check's state now, from any thread. */
void oracles_update_check_status(OraclesUpdateStatus *status);
/* Waits at most `timeout_ms` for the check's end: 1 when it is not running. */
int oracles_update_check_wait(int timeout_ms);
/* The SDL event pushed when the check ends (0 before any start), and whether it is pushed: on while SDL's events
 * run (the home screen's loop), off before SDL_Quit.  A result that came while it was off is read with the status. */
uint32_t oracles_update_check_event(void);
void oracles_update_check_events(int on);
/* Asks a running check to give up and waits for it a moment, before the process ends. */
void oracles_update_check_close(void);
/* Opens a newer release's page in the system's browser (SDL_OpenURL): 1 when asked; 0 without a newer release, or with
 * the message for the screen in `message` when no browser takes it (a console without one): what
 * oracles_update_open_failed says. */
int oracles_update_check_open(char *message, size_t capacity);
/* That message for the page `url`: "No browser opened the release page: github.com/...", its address without https://. */
void oracles_update_open_failed(const char *url, char *message, size_t capacity);

#endif
