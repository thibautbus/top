#include "backends.h"

#include <stdlib.h>
#include <string.h>

typedef struct null_backend {
    uint64_t clock_ns;
    uint32_t frames;
    uint32_t width, height;
} null_backend;

static int null_start(void *opaque, uint32_t width, uint32_t height, uint32_t sample_rate_hz, int audio_enabled)
{
    null_backend *backend = opaque;
    (void)sample_rate_hz; (void)audio_enabled;
    backend->width = width;
    backend->height = height;
    return 1;
}

static int null_poll_event(void *opaque, oracles_host_event *event, int *has_event)
{
    (void)opaque; (void)event;
    *has_event = 0;
    return 1;
}

static int null_present_frame(void *opaque, const oracles_host_video_frame *frame)
{
    null_backend *backend = opaque;
    if (!frame || !frame->pixels || frame->width != backend->width || frame->height != backend->height) return 0;
    backend->frames++;
    return 1;
}

static int null_queue_audio(void *opaque, const oracles_host_audio_chunk *chunk)
{
    (void)opaque;
    return chunk && chunk->samples && (chunk->sample_count & 1u) == 0;
}

static uint64_t null_monotonic_ns(void *opaque)
{
    null_backend *backend = opaque;
    backend->clock_ns += 1000; /* time passes even without sleeping */
    return backend->clock_ns;
}

static int null_sleep_ns(void *opaque, uint64_t duration_ns)
{
    null_backend *backend = opaque;
    backend->clock_ns += duration_ns;
    return 1;
}

static void null_stop(void *opaque) { (void)opaque; }

int oracles_null_backend_init(oracles_host_backend *backend)
{
    null_backend *state = calloc(1, sizeof *state);
    if (!state) return 0;
    state->clock_ns = 1;
    memset(backend, 0, sizeof *backend);
    backend->opaque = state;
    backend->start = null_start;
    backend->poll_event = null_poll_event;
    backend->present_frame = null_present_frame;
    backend->queue_audio = null_queue_audio;
    backend->monotonic_ns = null_monotonic_ns;
    backend->sleep_ns = null_sleep_ns;
    backend->stop = null_stop;
    return 1;
}

void oracles_null_backend_release(oracles_host_backend *backend)
{
    free(backend->opaque);
    backend->opaque = NULL;
}
