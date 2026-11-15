/* The SDL 3 backend's audio: a stream bound to the default device, which pulls from it, the device started once a
 * prebuffer is queued and paused again when the queue runs dry or the host cuts the sound (the pause, a suspension),
 * the sound faded out and in around those; with RetroAchievements the unlock's sound mixed on top. */
#include "sdl_backend_internal.h"
#ifdef ORACLES_RETROACHIEVEMENTS
#include "ui_achievements_sound.h"
#endif

#define FRAME_BYTES (2u * sizeof(int16_t))
/* The sound's fade out where the device plays it, against a click. */
#define AUDIO_FADE_MS 10u
#define AUDIO_FADE_MAX_FRAMES 1024u
/* The buffers a device holds past the stream, those it mixed ahead and those the system has (SDL's AAudio: two mixed
 * ahead of the one played, two more in AAudio's), with some to spare. */
#define DEVICE_BUFFERS 6u
/* The longest a suspension waits for the device to play its fade: a device that does not pull is not waited for. */
#define SUSPEND_WAIT_MS 250u

static Uint32 audio_bytes_for_ms(const sdl_backend *backend, unsigned ms)
{
    return (Uint32)((uint64_t)backend->sample_rate_hz * 2u * sizeof(int16_t) * ms / 1000u);
}

#ifdef ORACLES_RETROACHIEVEMENTS
/* The unlock's sound at the stream's rate, once; none without the memory for it. */
static void unlock_render(sdl_backend *backend)
{
    const size_t frames = oracles_sound_frames(&oracles_unlock_sound, backend->sample_rate_hz);
    backend->unlock = malloc(frames * sizeof *backend->unlock);
    backend->unlock_frames = backend->unlock ? oracles_sound_render(&oracles_unlock_sound, backend->sample_rate_hz, backend->unlock, frames) : 0;
    backend->unlock_cursor = backend->unlock_frames;
}

/* The chunk as it is queued: the core's, or while the unlock's sound plays a copy with the sound mixed on top. */
static const int16_t *unlock_mix(sdl_backend *backend, const oracles_host_audio_chunk *chunk)
{
    if (backend->unlock_cursor >= backend->unlock_frames) return chunk->samples;
    if (chunk->sample_count > backend->unlock_mixed_capacity) {
        int16_t *grown = realloc(backend->unlock_mixed, chunk->sample_count * sizeof *grown);
        if (!grown) { backend->unlock_cursor = backend->unlock_frames; return chunk->samples; }
        backend->unlock_mixed = grown;
        backend->unlock_mixed_capacity = chunk->sample_count;
    }
    memcpy(backend->unlock_mixed, chunk->samples, chunk->sample_count * sizeof *backend->unlock_mixed);
    oracles_sound_mix_s16(backend->unlock, backend->unlock_frames, &backend->unlock_cursor, backend->unlock_mixed, chunk->sample_count / 2u, 2u);
    return backend->unlock_mixed;
}

void oracles_sdl_backend_unlock_sound(const oracles_host_backend *backend)
{
    sdl_backend *state = backend->opaque;
    if (state && state->audio) state->unlock_cursor = 0;
}
#endif

int sb_audio_open(sdl_backend *backend, uint32_t sample_rate_hz)
{
    /* The device's buffer SDL 2 was asked for, 512 frames; the stream converts to what the device plays. */
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "512");
    const SDL_AudioSpec wanted = { SDL_AUDIO_S16, 2, (int)sample_rate_hz };
    backend->audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &wanted, NULL, NULL);
    if (!backend->audio) return 0;
    backend->sample_rate_hz = sample_rate_hz;
    backend->ramp_done = SIZE_MAX;
#ifdef ORACLES_RETROACHIEVEMENTS
    unlock_render(backend);
#endif
    /* Paused until the prebuffer is queued, as SDL opens it; see sb_sdl_queue_audio. */
    return 1;
}

void sb_audio_close(sdl_backend *backend)
{
    if (backend->audio) { SDL_DestroyAudioStream(backend->audio); backend->audio = NULL; }   /* its device closes with it */
    free(backend->ramped);
    backend->ramped = NULL;
    backend->ramped_capacity = 0;
#ifdef ORACLES_RETROACHIEVEMENTS
    free(backend->unlock);
    free(backend->unlock_mixed);
    backend->unlock = NULL; backend->unlock_mixed = NULL;
    backend->unlock_frames = backend->unlock_cursor = backend->unlock_mixed_capacity = 0;
#endif
}

/* What the stream holds that the device has not taken yet, in bytes of the chunks put: SDL 2's queued audio. */
static Uint32 queued_bytes(const sdl_backend *backend)
{
    const int queued = SDL_GetAudioStreamQueued(backend->audio);
    return queued > 0 ? (Uint32)queued : 0u;
}

/* Frames put in the stream, kept as the last queued. */
static int put_frames(sdl_backend *backend, const int16_t *samples, size_t frames)
{
    if (!SDL_PutAudioStreamData(backend->audio, samples, (int)(frames * FRAME_BYTES))) return 0;
    oracles_audio_recent_put(&backend->recent, samples, frames);
    return 1;
}

/* The DEVICE_BUFFERS buffers of the device, in the stream's frames. */
static size_t device_frames(const sdl_backend *backend)
{
    SDL_AudioSpec spec;
    int frames = 0;
    if (!SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(backend->audio), &spec, &frames) || frames <= 0 || spec.freq <= 0)
        return DEVICE_BUFFERS * 512u;
    return (size_t)((uint64_t)DEVICE_BUFFERS * (uint64_t)frames * backend->sample_rate_hz / (uint64_t)spec.freq);
}

/* What the stream holds, which the device has not played, replaced by its first AUDIO_FADE_MS faded out, then
 * `silence` frames of silence: the device goes on from where it plays down to silence.  A paused device (its prebuffer,
 * never heard) has the stream emptied only.  1 when the device plays on. */
static int fade_queue(sdl_backend *backend, size_t silence)
{
    SDL_LockAudioStream(backend->audio);   /* the device takes nothing between what is read and what is put */
    const size_t queued = queued_bytes(backend) / FRAME_BYTES;
#ifdef ORACLES_RETROACHIEVEMENTS
    /* The unlock's sound in what the queue drops: again from its start if none of it was heard yet, else none of it. */
    backend->unlock_cursor = oracles_sound_cursor_cleared(backend->unlock_cursor, backend->unlock_frames, queued);
#endif
    int16_t faded[AUDIO_FADE_MAX_FRAMES * 2];
    const size_t frames = backend->sample_rate_hz * AUDIO_FADE_MS / 1000u;
    const size_t fade = backend->audio_started ? oracles_audio_fade_out(&backend->recent, queued, frames < AUDIO_FADE_MAX_FRAMES ? frames : AUDIO_FADE_MAX_FRAMES, faded) : 0;
    SDL_ClearAudioStream(backend->audio);
    if (!backend->audio_started) { SDL_UnlockAudioStream(backend->audio); return 0; }
    if (fade) put_frames(backend, faded, fade);
    static const int16_t zeros[AUDIO_FADE_MAX_FRAMES * 2];
    while (silence) {
        const size_t part = silence < AUDIO_FADE_MAX_FRAMES ? silence : AUDIO_FADE_MAX_FRAMES;
        if (!put_frames(backend, zeros, part)) break;
        silence -= part;
    }
    SDL_UnlockAudioStream(backend->audio);
    return 1;
}

size_t sb_audio_fade_away(sdl_backend *backend)
{
    if (!backend->audio) return 0;
    const size_t device = device_frames(backend);
    return fade_queue(backend, 2 * device) ? device : 0;
}

void sb_audio_silenced(sdl_backend *backend, size_t playing)
{
    if (!backend->audio) return;
    const uint64_t since = sb_sdl_monotonic_ns(backend);
    while (playing && queued_bytes(backend) > playing * FRAME_BYTES && sb_sdl_monotonic_ns(backend) - since < SDL_MS_TO_NS(SUSPEND_WAIT_MS))
        SDL_Delay(1);
    SDL_PauseAudioStreamDevice(backend->audio);
    SDL_ClearAudioStream(backend->audio);
    backend->audio_started = 0;   /* sb_sdl_queue_audio starts it again once its prebuffer is queued */
    backend->audio_cut = 0;
}

/* The frames as they are queued: as given, or after an underrun a copy fading in over AUDIO_FADE_MS, against the click
 * of a device starting again at the sound's level (none without the memory for it). */
static const int16_t *ramp_in(sdl_backend *backend, const int16_t *samples, size_t frames)
{
    const size_t length = backend->sample_rate_hz * AUDIO_FADE_MS / 1000u;
    if (backend->ramp_done >= length) return samples;
    if (frames * 2 > backend->ramped_capacity) {
        int16_t *grown = realloc(backend->ramped, frames * 2 * sizeof *grown);
        if (!grown) { backend->ramp_done = length; return samples; }
        backend->ramped = grown;
        backend->ramped_capacity = frames * 2;
    }
    memcpy(backend->ramped, samples, frames * 2 * sizeof *samples);
    oracles_audio_fade_in(backend->ramped, frames, &backend->ramp_done, length);
    return backend->ramped;
}

int sb_sdl_queue_audio(void *opaque, const oracles_host_audio_chunk *chunk)
{
    sdl_backend *backend = opaque;
    if (!backend->audio) return 1;
    if (backend->audio_cut) {   /* the fade of the cut played: no gap heard, the device waits for its prebuffer again */
        backend->audio_cut = 0;
        SDL_PauseAudioStreamDevice(backend->audio);
        SDL_ClearAudioStream(backend->audio);
        backend->audio_started = 0;
    }
    const Uint32 queued = queued_bytes(backend);
    if (queued > backend->audio_peak_bytes) backend->audio_peak_bytes = queued;
    backend->audio_queued_sum_bytes += queued;
    backend->audio_queued_samples_count++;
    const uint64_t now = sb_sdl_monotonic_ns(backend);
    if (backend->audio_started && queued == 0) {
        /* The device drained everything: a gap was heard. Refill before resuming. */
        if (backend->audio_underruns < ORACLES_SDL_UNDERRUNS_LOGGED) {
            oracles_sdl_underrun *u = &backend->underrun_log[backend->audio_underruns];
            u->frame = backend->presents;
            u->since_previous_chunk_ns = now - backend->last_chunk_ns;
            u->previous_chunk_left_ms = backend->last_chunk_queued_bytes / (audio_bytes_for_ms(backend, 1000u) / 1000u);
            u->last_present_ns = backend->last_present_ns_spent;
        }
        backend->audio_underruns++;
        SDL_PauseAudioStreamDevice(backend->audio);
        backend->audio_started = 0;
        backend->ramp_done = 0;   /* the sound starts again from silence, faded in */
    }
    if (queued > audio_bytes_for_ms(backend, AUDIO_QUEUE_LIMIT_MS)) { backend->audio_drops++; return 1; }
    if (chunk->sample_count > INT_MAX / sizeof(int16_t)) return 0;
    const int16_t *samples = chunk->samples;
#ifdef ORACLES_RETROACHIEVEMENTS
    samples = unlock_mix(backend, chunk);
#endif
    samples = ramp_in(backend, samples, chunk->sample_count / 2u);
    if (!put_frames(backend, samples, chunk->sample_count / 2u)) return 0;
    backend->last_chunk_ns = now;
    backend->last_chunk_queued_bytes = queued_bytes(backend);
    if (!backend->audio_started && queued_bytes(backend) >= audio_bytes_for_ms(backend, AUDIO_PREBUFFER_MS)) {
        SDL_ResumeAudioStreamDevice(backend->audio);
        backend->audio_started = 1;
    }
    return 1;
}

uint32_t sb_sdl_audio_queued_frames(void *opaque)
{
    const sdl_backend *backend = opaque;
    if (!backend->audio) return 0;
    return (uint32_t)(queued_bytes(backend) / (2u * sizeof(int16_t)));
}

void oracles_sdl_backend_audio_report(const oracles_host_backend *backend, unsigned *underruns, unsigned *drops,
                                      unsigned *peak_ms, unsigned *average_ms)
{
    const sdl_backend *state = backend->opaque;
    *underruns = state ? state->audio_underruns : 0;
    *drops = state ? state->audio_drops : 0;
    *peak_ms = 0;
    *average_ms = 0;
    if (!state || !state->sample_rate_hz) return;
    const uint64_t bytes_per_ms = (uint64_t)state->sample_rate_hz * 2u * sizeof(int16_t) / 1000u;
    *peak_ms = (unsigned)(state->audio_peak_bytes / bytes_per_ms);
    if (state->audio_queued_samples_count)
        *average_ms = (unsigned)(state->audio_queued_sum_bytes / state->audio_queued_samples_count / bytes_per_ms);
}

int oracles_sdl_backend_audio_plays(const oracles_host_backend *backend)
{
    const sdl_backend *state = backend->opaque;
    return state && state->audio && state->audio_started && !SDL_AudioStreamDevicePaused(state->audio);
}

unsigned oracles_sdl_backend_underruns(const oracles_host_backend *backend, const oracles_sdl_underrun **log)
{
    const sdl_backend *state = backend->opaque;
    *log = state ? state->underrun_log : NULL;
    if (!state) return 0;
    return state->audio_underruns < ORACLES_SDL_UNDERRUNS_LOGGED ? state->audio_underruns : ORACLES_SDL_UNDERRUNS_LOGGED;
}

void sb_sdl_cut_audio(void *opaque)
{
    sdl_backend *state = opaque;
    if (!state->audio) return;
    /* The device plays the fade, then silence; the next chunk pauses it and starts it again from its prebuffer. */
    state->audio_cut = fade_queue(state, 0);
}
