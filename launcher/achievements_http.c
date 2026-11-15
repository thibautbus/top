/* The achievements' transport over the platform's HTTPS client (achievements_http.h). */
#include "achievements_http.h"

#include "achievements_files.h"
#include "achievements_transport.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

/* What the transport waits for its thread at the end: the request being sent cancelled (Linux) or finished. */
#define STOP_WAIT_MS 2000

typedef struct job {
    struct job *next;
    char *url, *post_data, *content_type, *user_agent;
    char *save_path;   /* a GET written there by the thread, rather than given to the answer */
    OraclesAchievementsAnswer answer;
    void *answer_opaque;
    char *body;
    size_t length;
    int status;
} job;

/* Shared by the thread and its owner, freed by the last of the two to let it go. */
typedef struct http {
    SDL_Mutex *lock;
    SDL_Condition *wake, *exited_signal;
    job *posts, *gets, *done;   /* the queues, oldest first; done, newest first */
    int stopping, exited, holders;
    SDL_AtomicInt cancel;
    SDL_Thread *thread;
    OraclesAchievementsHttpOptions options;
} http;

static char *copy(const char *text)
{
    return text ? SDL_strdup(text) : NULL;
}

/* Not left in freed memory: a form may hold a password. */
static void wipe(char *text)
{
    if (!text) return;
    volatile char *v = text;
    while (*v) *v++ = 0;
}

static void free_job(job *j)
{
    wipe(j->post_data);
    SDL_free(j->url); SDL_free(j->post_data); SDL_free(j->content_type); SDL_free(j->user_agent); SDL_free(j->save_path); SDL_free(j->body);
    SDL_free(j);
}

static void release(http *h)
{
    SDL_LockMutex(h->lock);
    const int last = --h->holders == 0;
    SDL_UnlockMutex(h->lock);
    if (!last) return;
    while (h->posts) { job *next = h->posts->next; free_job(h->posts); h->posts = next; }
    while (h->gets) { job *next = h->gets->next; free_job(h->gets); h->gets = next; }
    while (h->done) { job *next = h->done->next; free_job(h->done); h->done = next; }
    SDL_DestroyCondition(h->exited_signal);
    SDL_DestroyCondition(h->wake);
    SDL_DestroyMutex(h->lock);
    SDL_free(h);
}

static void push(job **queue, job *j)
{
    j->next = NULL;
    while (*queue) queue = &(*queue)->next;
    *queue = j;
}

/* One request sent, on the thread: a GET to keep written to its file (none asked when the file is there). */
static void perform(http *h, job *j)
{
    if (j->save_path && oracles_achievements_file_exists(j->save_path)) { j->status = 200; return; }
    const OraclesHttpRequest request = { j->url, j->post_data, j->post_data ? strlen(j->post_data) : 0, j->content_type, NULL, j->user_agent,
                                         h->options.timeout_ms, ORACLES_ACHIEVEMENTS_HTTP_MAX_BODY, &h->cancel, h->options.loopback_http };
    char error[256] = "";
    if (!h->options.fetch(&request, &j->body, &j->length, &j->status, error, sizeof error)) {
        j->status = 0;
        j->body = NULL;
        j->length = 0;
        if (!SDL_GetAtomicInt(&h->cancel)) fprintf(stderr, "oracles: achievements: no answer from the server: %s\n", error);
        return;
    }
    if (j->save_path) {
        if (j->status == 200 && !oracles_achievements_write_image(j->save_path, (const uint8_t *)j->body, j->length)) j->status = 0;
        SDL_free(j->body);
        j->body = NULL;
        j->length = 0;
    }
}

static int work(void *opaque)
{
    http *h = opaque;
    for (;;) {
        SDL_LockMutex(h->lock);
        while (!h->stopping && !h->posts && !h->gets) SDL_WaitCondition(h->wake, h->lock);
        job *j = NULL;
        if (!h->stopping) {
            job **queue = h->posts ? &h->posts : &h->gets;
            j = *queue;
            *queue = j->next;
        }
        SDL_UnlockMutex(h->lock);
        if (!j) break;
        perform(h, j);
        SDL_LockMutex(h->lock);
        j->next = h->done;   /* given up by stop if it came while the transport stopped */
        h->done = j;
        SDL_UnlockMutex(h->lock);
    }
    SDL_LockMutex(h->lock);
    h->exited = 1;
    SDL_SignalCondition(h->exited_signal);
    SDL_UnlockMutex(h->lock);
    release(h);
    return 0;
}

static job *new_job(const char *url, const char *post_data, const char *content_type, const char *user_agent, const char *save_path,
                    OraclesAchievementsAnswer answer, void *answer_opaque)
{
    job *j = SDL_calloc(1, sizeof *j);
    if (!j) return NULL;
    j->url = copy(url); j->post_data = copy(post_data); j->content_type = copy(content_type); j->user_agent = copy(user_agent);
    j->save_path = copy(save_path);
    j->answer = answer;
    j->answer_opaque = answer_opaque;
    if (!j->url || !j->user_agent || (post_data && !j->post_data) || (content_type && !j->content_type) || (save_path && !j->save_path)) {
        free_job(j);
        return NULL;
    }
    return j;
}

static void enqueue(http *h, job *j, OraclesAchievementsAnswer answer, void *answer_opaque)
{
    if (!j) { answer(answer_opaque, 0, NULL, 0); return; }   /* out of memory: as no server */
    SDL_LockMutex(h->lock);
    const int stopping = h->stopping;
    if (!stopping) {
        push(j->post_data ? &h->posts : &h->gets, j);
        SDL_SignalCondition(h->wake);
    }
    SDL_UnlockMutex(h->lock);
    if (stopping) { free_job(j); answer(answer_opaque, -1, NULL, 0); }   /* rc_client's last try while it is destroyed */
}

static void queue_request(void *opaque, const char *url, const char *post_data, const char *content_type, const char *user_agent,
                          OraclesAchievementsAnswer answer, void *answer_opaque)
{
    enqueue(opaque, new_job(url, post_data, content_type, user_agent, NULL, answer, answer_opaque), answer, answer_opaque);
}

static void queue_save(void *opaque, const char *url, const char *path, const char *user_agent, OraclesAchievementsAnswer answer, void *answer_opaque)
{
    enqueue(opaque, new_job(url, NULL, NULL, user_agent, path, answer, answer_opaque), answer, answer_opaque);
}

/* The jobs of `list`, newest first, answered in the order they came, with their status or `status` when it is not 0. */
static void answer_all(job *list, int status)
{
    job *oldest = NULL;
    while (list) { job *next = list->next; list->next = oldest; oldest = list; list = next; }
    while (oldest) {
        job *next = oldest->next;
        oldest->answer(oldest->answer_opaque, status ? status : oldest->status, status ? NULL : (const uint8_t *)oldest->body, status ? 0 : oldest->length);
        free_job(oldest);
        oldest = next;
    }
}

static void poll_answers(void *opaque)
{
    http *h = opaque;
    SDL_LockMutex(h->lock);
    job *done = h->done;
    h->done = NULL;
    SDL_UnlockMutex(h->lock);
    answer_all(done, 0);
}

/* The end: the request being sent cancelled (where the client allows it) and waited for, at most STOP_WAIT_MS, so that
 * no request is left inside the client when the process exits; every request not answered yet answered as given up
 * (-1), which lets rc_client free what it kept for it. */
static void stop(void *opaque)
{
    http *h = opaque;
    SDL_LockMutex(h->lock);
    h->stopping = 1;
    job *left = h->done;
    h->done = NULL;
    for (job **queue = &h->posts; queue; queue = queue == &h->posts ? &h->gets : NULL)
        while (*queue) { job *j = *queue; *queue = j->next; j->next = left; left = j; }
    SDL_SignalCondition(h->wake);
    SDL_SetAtomicInt(&h->cancel, 1);
    const Uint64 until = SDL_GetTicks() + STOP_WAIT_MS;
    while (!h->exited && SDL_GetTicks() < until) SDL_WaitConditionTimeout(h->exited_signal, h->lock, 100);
    const int exited = h->exited;
    while (h->done) { job *j = h->done; h->done = j->next; j->next = left; left = j; }
    SDL_UnlockMutex(h->lock);
    if (exited) SDL_WaitThread(h->thread, NULL);
    else SDL_DetachThread(h->thread);   /* still in a request: it ends on its own and lets the state go */
    answer_all(left, -1);
    release(h);
}

int oracles_achievements_http_transport(OraclesAchievementsTransport *transport, const OraclesAchievementsHttpOptions *options)
{
    memset(transport, 0, sizeof *transport);
    http *h = SDL_calloc(1, sizeof *h);
    if (!h) return 0;
    if (options) h->options = *options;
    if (!h->options.fetch) h->options.fetch = oracles_http_request;
    if (!h->options.timeout_ms) h->options.timeout_ms = ORACLES_ACHIEVEMENTS_HTTP_TIMEOUT_MS;
    h->lock = SDL_CreateMutex();
    h->wake = SDL_CreateCondition();
    h->exited_signal = SDL_CreateCondition();
    h->holders = 2;
    h->thread = h->lock && h->wake && h->exited_signal ? SDL_CreateThread(work, "achievements", h) : NULL;
    if (!h->thread) {
        if (h->exited_signal) SDL_DestroyCondition(h->exited_signal);
        if (h->wake) SDL_DestroyCondition(h->wake);
        if (h->lock) SDL_DestroyMutex(h->lock);
        SDL_free(h);
        return 0;
    }
    transport->send = queue_request;
    transport->poll = poll_answers;
    transport->destroy = stop;
    transport->opaque = h;
    transport->save = queue_save;
    return 1;
}

int oracles_achievements_platform_transport(OraclesAchievementsTransport *transport)
{
    return oracles_achievements_http_transport(transport, NULL);
}
