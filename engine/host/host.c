#include "oracles_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAME_TICKS UINT64_C(70224)
#define CPU_HZ UINT64_C(4194304)
#define FRAME_DURATION_NUMERATOR (FRAME_TICKS * UINT64_C(1000000000))
#define FRAME_NOMINAL_NS (FRAME_DURATION_NUMERATOR / CPU_HZ)
#define MAX_EVENTS_PER_FRAME 4096u
#define MAX_PACE_CALLS 4096u
#define AUDIO_CHUNK_SAMPLES 4096u
/* Rate control: keep about four frames of audio queued at the device by
 * producing up to two percent more or fewer samples per second. */
#define AUDIO_TARGET_MS 70u
#define AUDIO_RATE_MAX_DEVIATION 0.02

/* Checking a display said to pace.  The floor lies above the 61 Hz
 * of any display the launcher lets pace, so that such a display never meets
 * it, and keeps a presentation that does not wait near the Game Boy's rate
 * until the check has seen a window of frames.  It is a deadline, like the
 * pacer's: the loop's own time between two frames does not add to it, and a
 * presentation that does not wait runs at 62.5 Hz exactly. */
#define DISPLAY_FLOOR_NS UINT64_C(16000000)   /* 62.5 Hz */
#define DISPLAY_WINDOW_FRAMES 60u
#define DISPLAY_MAX_MILLIHZ UINT64_C(61500)

typedef struct pacer {
    uint64_t deadline_ns;
    uint64_t remainder;
} pacer;

typedef struct display_watch {
    uint64_t floor_ns;   /* the earliest end of the current frame */
    uint64_t window_start_ns;
    uint32_t window_frames;
} display_watch;

static void set_error(char *error, size_t capacity, const char *message)
{
    if (error && capacity) snprintf(error, capacity, "%s", message);
}

static int valid_backend(const oracles_host_backend *backend, int pace, int audio)
{
    if (!backend || !backend->start || !backend->poll_event || !backend->present_frame || !backend->stop) return 0;
    if (pace && (!backend->monotonic_ns || !backend->sleep_ns)) return 0;
    if (audio && !backend->queue_audio) return 0;
    return 1;
}

static int valid_button(unsigned button)
{
    return button != 0 && (button & (button - 1u)) == 0 && (button & ~0xffu) == 0;
}

/* Exact 59.7275 Hz pacing: the fractional nanoseconds are carried over. */
static void pacer_advance(pacer *p)
{
    const uint64_t whole = FRAME_DURATION_NUMERATOR / CPU_HZ;
    const uint64_t fraction = FRAME_DURATION_NUMERATOR % CPU_HZ;
    const uint64_t remainder = p->remainder + fraction;
    p->deadline_ns += whole + remainder / CPU_HZ;
    p->remainder = remainder % CPU_HZ;
}

/* Sleeps until the deadline.  A deadline already passed counts a late frame;
 * once the backlog exceeds the budget the deadline is resynchronised so the
 * loop drops the backlog instead of fast-forwarding through it. */
static int pace_frame(const oracles_host_backend *backend, pacer *p, uint64_t budget_ns,
                      uint32_t *frames_late, uint32_t *resyncs)
{
    for (unsigned calls = 0; calls < MAX_PACE_CALLS; calls++) {
        const uint64_t now = backend->monotonic_ns(backend->opaque);
        if (now == 0) return 0;
        if (now < p->deadline_ns) {
            if (!backend->sleep_ns(backend->opaque, p->deadline_ns - now)) return 0;
            continue;
        }
        if (calls != 0) return 1;
        (*frames_late)++;
        if (now - p->deadline_ns > budget_ns) {
            p->deadline_ns = now;
            p->remainder = 0;
            (*resyncs)++;
        }
        return 1;
    }
    return 0;
}

/* Holds the frame to the floor, 16 ms after the previous frame's (the first
 * frame's start, frame_started_ns, for the first), then counts it in the
 * window, from the end of the first second on (`presented` counts this frame):
 * a window just opened, or being sized to the game's surface, may not wait for
 * the display yet (macOS), and the takeover is for the whole session.  Returns
 * 2 when a full window ran faster than a display that paces, 1 otherwise, 0 on
 * a clock failure. */
static int watch_display(const oracles_host_backend *backend, display_watch *w, uint64_t frame_started_ns, uint32_t presented)
{
    if (w->floor_ns == 0) w->floor_ns = frame_started_ns;
    if (w->window_frames == 0) w->window_start_ns = w->floor_ns;
    w->floor_ns += DISPLAY_FLOOR_NS;
    uint64_t now = backend->monotonic_ns(backend->opaque);
    if (now == 0) return 0;
    if (now < w->floor_ns) {
        if (!backend->sleep_ns(backend->opaque, w->floor_ns - now)) return 0;
        now = backend->monotonic_ns(backend->opaque);
        if (now == 0) return 0;
    } else {
        w->floor_ns = now;   /* past its floor: the display waited, or the work was long; the next floor counts from here */
    }
    if (presented <= ORACLES_HOST_WARMUP_FRAMES) return 1;
    if (++w->window_frames < DISPLAY_WINDOW_FRAMES) return 1;
    w->window_frames = 0;
    const uint64_t shortest_ns = (uint64_t)DISPLAY_WINDOW_FRAMES * UINT64_C(1000000000000) / DISPLAY_MAX_MILLIHZ;
    return now - w->window_start_ns < shortest_ns ? 2 : 1;
}

/* After a pause, which makes a frame slow and never fast: the floor restarts from now, and the window goes on as if
 * the pause took no time, so that it neither hides a presentation that does not wait nor counts the pause as the
 * display's.  0 on a clock failure. */
static int watch_resume(const oracles_host_backend *backend, display_watch *w)
{
    const uint64_t now = backend->monotonic_ns(backend->opaque);
    if (now == 0) return 0;
    if (w->window_frames && now > w->floor_ns) w->window_start_ns += now - w->floor_ns;
    w->floor_ns = now;
    return 1;
}

typedef struct save_state {
    uint8_t *current;
    uint8_t *last_stored;
    size_t size;
    int has_last;
} save_state;

/* Stores the SRAM when it differs from what was last stored. Returns 0 on success, 1 when nothing changed, -1 on failure. */
static int store_if_changed(const oracles_host_run_config *config, save_state *save)
{
    if (!config->store_save || save->size == 0) return 1;
    if (oracles_core_save_sram(config->core, save->current, save->size) != 0) return -1;
    if (save->has_last && memcmp(save->current, save->last_stored, save->size) == 0) return 1;
    if (!config->store_save(config->store_save_opaque, save->current, save->size)) return -1;
    memcpy(save->last_stored, save->current, save->size);
    save->has_last = 1;
    return 0;
}

/* The store the backend calls when the system is about to suspend the application (watch_suspend). */
typedef struct suspend_store {
    const oracles_host_run_config *config;
    save_state *save;
    oracles_host_run_report *report;
    int failed;
} suspend_store;

static void store_on_suspend(void *opaque)
{
    suspend_store *s = opaque;
    const int stored = store_if_changed(s->config, s->save);
    if (stored < 0) s->failed = 1;
    if (stored == 0) s->report->saves_written++;
}

int oracles_host_run(const oracles_host_run_config *config, const oracles_host_backend *backend,
                     oracles_host_run_report *report, char *error, size_t error_capacity)
{
    oracles_host_run_report local;
    memset(&local, 0, sizeof local);
    save_state save = { NULL, NULL, 0, 0 };
    pacer p = { 0, 0 };
    display_watch watch = { 0, 0, 0 };
    uint64_t budget_ns = 0;
    unsigned buttons = 0;
    int started = 0, quit = 0;
    int result = ORACLES_HOST_BACKEND_FAILED;
    static int16_t audio_chunk[AUDIO_CHUNK_SAMPLES];

    if (report) memset(report, 0, sizeof *report);
    if (error && error_capacity) error[0] = 0;
    const int audio = config && config->sample_rate_hz != 0;
    if (!config || !config->core || !valid_backend(backend, config->pace_frames || config->display_paces, audio)) {
        set_error(error, error_capacity, "invalid host configuration");
        return ORACLES_HOST_INVALID_ARGUMENT;
    }
    int pacing = config->pace_frames, watching = !config->pace_frames && config->display_paces;
    if (config->store_save) {
        save.size = oracles_core_sram_size(config->core);
        if (save.size) {
            save.current = malloc(save.size);
            save.last_stored = malloc(save.size);
            if (!save.current || !save.last_stored) {
                free(save.current); free(save.last_stored);
                set_error(error, error_capacity, "out of memory");
                return ORACLES_HOST_SAVE_FAILED;
            }
            /* The SRAM loaded before the run is the baseline: no write until it changes. */
            if (oracles_core_save_sram(config->core, save.last_stored, save.size) == 0) save.has_last = 1;
        }
    }

    const uint32_t frame_width = config->frame_width ? config->frame_width : ORACLES_SCREEN_WIDTH;
    const uint32_t frame_height = config->frame_height ? config->frame_height : ORACLES_SCREEN_HEIGHT;
    started = 1;
    if (!backend->start(backend->opaque, frame_width, frame_height, config->sample_rate_hz, audio)) {
        set_error(error, error_capacity, "backend initialisation failed");
        goto done;
    }
    if (pacing || watching) {
        uint32_t frames = config->max_catch_up_frames ? config->max_catch_up_frames : ORACLES_HOST_DEFAULT_CATCH_UP_FRAMES;
        if (frames > ORACLES_HOST_MAX_CATCH_UP_FRAMES) frames = ORACLES_HOST_MAX_CATCH_UP_FRAMES;
        budget_ns = (uint64_t)frames * FRAME_NOMINAL_NS;
    }
    if (config->on_started) config->on_started(config->pause_opaque);
    suspend_store suspend = { config, &save, &local, 0 };
    if (backend->watch_suspend && config->store_save) backend->watch_suspend(backend->opaque, store_on_suspend, &suspend);
    if (config->pace_frames) {
        p.deadline_ns = backend->monotonic_ns(backend->opaque);
        if (p.deadline_ns == 0) { set_error(error, error_capacity, "backend clock failed"); goto done; }
    }

    while (!quit) {
        if (suspend.failed) { result = ORACLES_HOST_SAVE_FAILED; set_error(error, error_capacity, "save failed at the suspension"); goto done; }
        for (unsigned events = 0;; events++) {
            oracles_host_event event;
            int has_event = 0;
            memset(&event, 0, sizeof event);
            if (!backend->poll_event(backend->opaque, &event, &has_event)) {
                set_error(error, error_capacity, "backend event polling failed");
                goto done;
            }
            if (!has_event) break;
            if (events >= MAX_EVENTS_PER_FRAME) {
                set_error(error, error_capacity, "backend event queue did not drain");
                goto done;
            }
            if (event.type == ORACLES_HOST_EVENT_QUIT) { quit = 1; break; }
            if (event.type == ORACLES_HOST_EVENT_PAUSE || event.type == ORACLES_HOST_EVENT_SUSPEND) {
                /* The cartridge RAM is stored before the loop is held: a suspended application may be ended by the
                 * system, and the pause's menu may end the run. */
                const int stored = store_if_changed(config, &save);
                if (stored < 0) { result = ORACLES_HOST_SAVE_FAILED; set_error(error, error_capacity, "save failed"); goto done; }
                if (stored == 0) local.saves_written++;
                if (!config->on_pause) {
                    if (event.type == ORACLES_HOST_EVENT_SUSPEND) continue;
                    quit = 1;
                    break;
                }
                local.pauses++;
                if (config->on_pause(config->pause_opaque) == ORACLES_HOST_PAUSE_QUIT) { quit = 1; break; }
                /* Between two frames: no frame ran, none is counted.  The keys held when the pause came were let go
                 * during it; the pacing takes the clock as it finds it rather than catching up the pause, and the display's
                 * watch leaves the pause out of its window. */
                buttons = 0;
                if (pacing) {
                    p.deadline_ns = backend->monotonic_ns(backend->opaque);
                    p.remainder = 0;
                    if (p.deadline_ns == 0) { set_error(error, error_capacity, "backend clock failed"); goto done; }
                }
                if (watching && !watch_resume(backend, &watch)) { set_error(error, error_capacity, "backend clock failed"); goto done; }
                continue;
            }
            if (event.type == ORACLES_HOST_EVENT_COMMAND) {
                if (config->on_command) config->on_command(config->on_command_opaque, event.command);
                continue;
            }
            if (event.type != ORACLES_HOST_EVENT_BUTTON || !valid_button(event.button)) {
                set_error(error, error_capacity, "backend produced an invalid event");
                goto done;
            }
            if (event.pressed) buttons |= event.button; else buttons &= ~event.button;
        }
        if (quit) break;

        if (audio && backend->audio_queued_frames) {
            const double target = (double)config->sample_rate_hz * AUDIO_TARGET_MS / 1000.0;
            const double queued = (double)backend->audio_queued_frames(backend->opaque);
            double deviation = (queued - target) / target * AUDIO_RATE_MAX_DEVIATION;
            if (deviation > AUDIO_RATE_MAX_DEVIATION) deviation = AUDIO_RATE_MAX_DEVIATION;
            if (deviation < -AUDIO_RATE_MAX_DEVIATION) deviation = -AUDIO_RATE_MAX_DEVIATION;
            oracles_core_set_sample_rate(config->core, (unsigned)((double)config->sample_rate_hz * (1.0 - deviation)));
        }
        const uint64_t frame_started_ns = (pacing || watching || (config->measure_frames && backend->monotonic_ns)) ? backend->monotonic_ns(backend->opaque) : 0;
        if (config->on_frame_begin) config->on_frame_begin(config->frame_opaque, local.frames_presented);
        unsigned keys = buttons;
        int replaying = 0;
        if (config->input_source) {
            unsigned replayed = 0;
            if (config->input_source(config->input_source_opaque, local.frames_presented, &replayed)) { keys = replayed & 0xffu; replaying = 1; }
        }
        if (config->input_filter && !replaying) keys = config->input_filter(config->input_filter_opaque, local.frames_presented, keys) & 0xffu;
        if (config->input_sink) config->input_sink(config->input_sink_opaque, local.frames_presented, keys);
        if (config->input_hold) keys = config->input_hold(config->input_hold_opaque, local.frames_presented, keys) & 0xffu;
        oracles_core_set_keys(config->core, keys);
        oracles_core_run_frame(config->core);
        const uint64_t core_done_ns = frame_started_ns ? backend->monotonic_ns(backend->opaque) : 0;

        if (audio) {
            for (;;) {
                const size_t count = oracles_core_take_audio(config->core, audio_chunk, AUDIO_CHUNK_SAMPLES);
                if (count == 0) break;
                const oracles_host_audio_chunk chunk = { audio_chunk, count, config->sample_rate_hz };
                if (!backend->queue_audio(backend->opaque, &chunk)) {
                    set_error(error, error_capacity, "backend audio queue failed");
                    goto done;
                }
            }
        }

        const uint64_t source_started_ns = frame_started_ns ? backend->monotonic_ns(backend->opaque) : 0;
        oracles_host_video_frame frame = {
            config->frame_source ? config->frame_source(config->frame_source_opaque) : oracles_core_pixels(config->core),
            frame_width, frame_height, frame_width * sizeof(uint32_t), 0, 0, 0, 0
        };
        uint32_t crop[4];
        if (config->frame_crop && config->frame_crop(config->frame_crop_opaque, crop) && crop[2] && crop[3]
            && crop[0] + crop[2] <= frame_width && crop[1] + crop[3] <= frame_height) {
            frame.crop_x = crop[0]; frame.crop_y = crop[1]; frame.crop_w = crop[2]; frame.crop_h = crop[3];
        }
        const uint64_t source_done_ns = frame_started_ns ? backend->monotonic_ns(backend->opaque) : 0;
        if (!backend->present_frame(backend->opaque, &frame)) {
            set_error(error, error_capacity, "backend frame presentation failed");
            goto done;
        }
        const uint64_t present_done_ns = frame_started_ns ? backend->monotonic_ns(backend->opaque) : 0;
        if (config->on_frame_end) config->on_frame_end(config->frame_opaque, local.frames_presented);
        if (frame_started_ns) {
            const uint64_t now = backend->monotonic_ns(backend->opaque);
            const uint64_t spent = now > frame_started_ns ? now - frame_started_ns : 0;
            const uint64_t core_ns = core_done_ns - frame_started_ns, source_ns = source_done_ns - source_started_ns;
            const uint64_t present_ns = present_done_ns - source_done_ns, end_ns = now - present_done_ns;
            local.frame_ns_total += spent;
            local.present_ns_total += present_ns;
            if (spent > local.frame_ns_max) {
                local.frame_ns_max = spent;
                local.frame_max_index = local.frames_presented;
                local.max_frame_core_ns = core_ns; local.max_frame_source_ns = source_ns;
                local.max_frame_present_ns = present_ns; local.max_frame_end_ns = end_ns;
            }
            if (core_ns > local.core_ns_max) local.core_ns_max = core_ns;
            if (source_ns > local.source_ns_max) local.source_ns_max = source_ns;
            if (present_ns > local.present_ns_max) local.present_ns_max = present_ns;
            if (end_ns > local.end_ns_max) local.end_ns_max = end_ns;
            if (config->on_frame_timed) config->on_frame_timed(config->frame_timed_opaque, local.frames_presented, core_ns, source_ns, present_ns, end_ns);
            if (spent > 12000000u) local.frames_over_12ms++;
            if (spent > 16742706u) local.frames_over_16ms++;   /* one frame at 59.7275 Hz */
            if (spent - present_ns > 16742706u && local.frames_presented >= ORACLES_HOST_WARMUP_FRAMES) local.frames_over_budget++;
        }
        local.frames_presented++;

        if (config->save_interval_frames && local.frames_presented % config->save_interval_frames == 0) {
            const int stored = store_if_changed(config, &save);
            if (stored < 0) { result = ORACLES_HOST_SAVE_FAILED; set_error(error, error_capacity, "save failed"); goto done; }
            if (stored == 0) local.saves_written++;
        }
        if (config->quit_after_frames && local.frames_presented >= config->quit_after_frames) quit = 1;
        if (watching) {
            const int watched = watch_display(backend, &watch, frame_started_ns, local.frames_presented);
            if (!watched) { set_error(error, error_capacity, "backend pacing failed"); goto done; }
            if (watched == 2) {
                /* The presentation did not wait for the display: the host paces from here, as without vsync. */
                watching = 0;
                pacing = 1;
                local.display_unpaced_frame = local.frames_presented;
                if (backend->present_unpaced) backend->present_unpaced(backend->opaque);
                p.deadline_ns = backend->monotonic_ns(backend->opaque);
                p.remainder = 0;
                if (p.deadline_ns == 0) { set_error(error, error_capacity, "backend clock failed"); goto done; }
            }
        } else if (pacing) {
            pacer_advance(&p);
            if (!pace_frame(backend, &p, budget_ns, &local.frames_late, &local.pacing_resyncs)) {
                set_error(error, error_capacity, "backend pacing failed");
                goto done;
            }
        }
    }

    {
        const int stored = store_if_changed(config, &save);
        if (stored < 0) { result = ORACLES_HOST_SAVE_FAILED; set_error(error, error_capacity, "final save failed"); goto done; }
        if (stored == 0) local.saves_written++;
    }
    result = ORACLES_HOST_OK;

done:
    local.final_buttons = buttons;
    if (report) *report = local;
    if (started && backend->watch_suspend) backend->watch_suspend(backend->opaque, NULL, NULL);
    if (started) backend->stop(backend->opaque);
    free(save.current);
    free(save.last_stored);
    return result;
}
