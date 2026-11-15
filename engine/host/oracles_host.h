/* Host facade: the only place where the engine meets a window system, an
 * audio device and a clock, through a backend the launcher provides.
 *
 * The loop drives one OraclesCore.  The contract
 * uses no window-system, renderer or audio-library types.  oracles_host_run
 * and every callback are synchronous on the calling thread.  Every pointer
 * passed to a callback is borrowed and valid only until that callback returns.
 * A backend owns the resources created by start() and releases them from
 * stop(); stop() is called once whenever start() was attempted. */
#ifndef ORACLES_HOST_H
#define ORACLES_HOST_H

#include "core.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ORACLES_HOST_DEFAULT_CATCH_UP_FRAMES 8u
#define ORACLES_HOST_MAX_CATCH_UP_FRAMES 3600u

typedef enum oracles_host_event_type {
    ORACLES_HOST_EVENT_QUIT = 1,
    ORACLES_HOST_EVENT_BUTTON = 2,
    /* A user command outside the game (a setting toggle); the host hands it to
     * the launcher through on_command without interpreting it. */
    ORACLES_HOST_EVENT_COMMAND = 3,
    /* The player asked for the pause menu (Escape); without on_pause it ends the run, as QUIT. */
    ORACLES_HOST_EVENT_PAUSE = 4,
    /* The system suspended the application (Android's background), polled once it runs again: the host stores the
     * cartridge RAM if it changed (watch_suspend stored it at the suspension), then pauses as for PAUSE when there is an
     * on_pause, so that the player comes back to the pause's menu, else goes on. */
    ORACLES_HOST_EVENT_SUSPEND = 5
} oracles_host_event_type;

/* What on_pause returns. */
typedef enum oracles_host_pause_result {
    ORACLES_HOST_PAUSE_RESUME = 0,
    ORACLES_HOST_PAUSE_QUIT = 1
} oracles_host_pause_result;

typedef struct oracles_host_event {
    oracles_host_event_type type;
    unsigned button;   /* one ORACLES_KEY_* bit */
    int pressed;
    int command;       /* launcher-defined, for ORACLES_HOST_EVENT_COMMAND */
} oracles_host_event;

/* A frame's world band moved by less than a pixel when it is shown (the Enhanced sub-pixel camera): lines
 * `band_top` to `band_top + band_height` drawn `x` and `y` 256ths of a frame pixel left and up (negative: right and
 * down), at the screen's resolution rounded down, the lines above and below unmoved; then `patch` moved with the band
 * and `layer` over it (0xAARRGGBB, alpha 0 where they show nothing), at `layer_x`, `layer_y` of the frame, the layer
 * moved `layer_fx`, `layer_fy` 256ths of a pixel right and down, inside the band; then the `fixed` rectangles of the
 * frame (x, y, w, h) unmoved over them, and the panels.  The frame itself is whole: shown as it is, it is the picture at whole pixels. */
#define ORACLES_HOST_SUBPIXEL_FIXED 16u
#define ORACLES_HOST_SUBPIXEL_PANELS 8u
typedef struct oracles_host_video_subpixel {
    int32_t x, y;
    uint32_t band_top, band_height;
    const uint32_t *layer;
    int32_t layer_x, layer_y;
    uint32_t layer_w, layer_h, layer_pitch;   /* layer_pitch in pixels */
    int32_t layer_fx, layer_fy;
    const uint32_t *patch;                     /* the layer's size and pitch; NULL: none */
    uint32_t fixed_count;
    uint32_t fixed[ORACLES_HOST_SUBPIXEL_FIXED][4];
    /* Last, unmoved over all of it, the status bar's floating panels: `panels` the frame's size and pitch, premultiplied
     * 0xAARRGGBB, transparent but within its `panels_count` rects (x, y, w, h, apart from each other); NULL: none. */
    const uint32_t *panels;
    uint32_t panels_count;
    uint32_t panels_rects[ORACLES_HOST_SUBPIXEL_PANELS][4];
    /* `panels_scale` 2: the near view's panels at half the game's scale, `panels` and its rects twice the frame's size
     * and pitch, each panel drawn after the picture through its filter, at a whole number of the screen's pixels a pixel, kept
     * to the corner `panels_anchor` names (oracles_floating_screen_place); the move may then be none.  0 or 1: as above. */
    uint32_t panels_scale;
    uint8_t panels_anchor[ORACLES_HOST_SUBPIXEL_PANELS];
} oracles_host_video_subpixel;

typedef struct oracles_host_video_frame {
    const uint32_t *pixels;   /* numeric 0xAARRGGBB */
    uint32_t width;
    uint32_t height;
    size_t pitch_bytes;
    /* The part of the frame to show alone, at its own largest whole scale (the game's menus enlarged); crop_w 0: the
     * whole frame. */
    uint32_t crop_x, crop_y, crop_w, crop_h;
    const oracles_host_video_subpixel *subpixel;   /* NULL: shown as it is */
    /* A part of the frame shown again above it larger, in whole screen pixels (the text box with Text size Large or
     * Very large): drawn at box_size's factor of k, the frame's whole scale (text_box_enlarge.h: 1 Large, k + ceil(k / 2);
     * 2 Very large, 2 k), or the next size down, centred on that part across and against its bottom edge when
     * box_bottom, else its top, where it fits the frame shown; box_w 0: none. */
    uint32_t box_x, box_y, box_w, box_h;
    int box_bottom;
    /* With the part shown alone (crop): the width of a box the backend may show so, whether one shows or not, which the
     * part takes in too where its own scale holds it, so that the part's scale neither drops nor changes when a box
     * opens; 0: none. */
    uint32_t box_reserve_w;
    unsigned box_size;
} oracles_host_video_frame;

typedef struct oracles_host_audio_chunk {
    const int16_t *samples;   /* interleaved stereo */
    size_t sample_count;
    uint32_t sample_rate_hz;
} oracles_host_audio_chunk;

typedef struct oracles_host_backend {
    void *opaque;
    int (*start)(void *opaque, uint32_t width, uint32_t height, uint32_t sample_rate_hz, int audio_enabled);
    /* Fills at most one event per call; *has_event says whether it did. */
    int (*poll_event)(void *opaque, oracles_host_event *event, int *has_event);
    int (*present_frame)(void *opaque, const oracles_host_video_frame *frame);
    int (*queue_audio)(void *opaque, const oracles_host_audio_chunk *chunk);
    /* Stereo sample frames still queued at the device, for rate control; NULL
     * when the backend cannot tell. */
    uint32_t (*audio_queued_frames)(void *opaque);
    /* Monotonic nanoseconds; zero reports a clock error. */
    uint64_t (*monotonic_ns)(void *opaque);
    int (*sleep_ns)(void *opaque, uint64_t duration_ns);
    /* The display did not pace (display_paces), or, the host pacing, waiting
     * for its refresh slowed the game (present_waits): present without
     * waiting for it from now on, the host pacing; NULL when there is nothing
     * to turn off.  Returns 0 when the presentation keeps waiting (a driver
     * that forces vsync). */
    int (*present_unpaced)(void *opaque);
    /* After present_unpaced under present_waits, not waiting having made the
     * game no faster, or at the pause: wait for the display's refresh again,
     * as before; 0 when it could not.  The host watches a presentation that
     * waits (present_waits) only with both. */
    int (*present_waits_again)(void *opaque);
    /* Asks to be called back on the host's thread the moment the system is about to suspend the application (Android's
     * background), whatever the backend is doing then, a present or a poll, and before the system may block the thread
     * or end the process: the host stores the save there.  A NULL callback withdraws it.  NULL: no system suspends. */
    void (*watch_suspend)(void *opaque, void (*callback)(void *callback_opaque), void *callback_opaque);
    /* The pause and a suspension, before the pause's menu if any: the sound the device has not played yet is dropped,
     * faded out where it plays, and the device starts again from silence with the next chunks, which the host fades in
     * (oracles_core_audio_cleared).  NULL: the device's sound goes on, and the core's with it. */
    void (*cut_audio)(void *opaque);
    void (*stop)(void *opaque);
} oracles_host_backend;

typedef int (*oracles_host_store_save_fn)(void *opaque, const uint8_t *buffer, size_t size);

typedef struct oracles_host_run_config {
    OraclesCore *core;
    uint32_t sample_rate_hz;      /* 0: no audio */
    uint32_t quit_after_frames;   /* 0: no limit */
    int pace_frames;              /* 1: real time, 0: as fast as possible */
    /* With pace_frames 0: the presentation is meant to wait for the display
     * (vsync).  The host checks that it does: frames at least
     * 16 ms apart, and past the first second, which is not judged, from the
     * first second of frames run above 61.5 Hz on, the host paces itself, as
     * with pace_frames. */
    int display_paces;
    int measure_frames;     /* time each frame's work with the backend clock even when the display paces (vsync): the report's figures and the frame budget */
    /* With measure_frames, each frame's phases as they were timed: the core (its hooks included), the frame source (the
     * Enhanced view), the presentation and the frame's end (on_frame_end), in nanoseconds. */
    void (*on_frame_timed)(void *opaque, uint32_t frame, uint64_t core_ns, uint64_t source_ns, uint64_t present_ns, uint64_t end_ns);
    void *frame_timed_opaque;
    uint32_t max_catch_up_frames; /* 0: ORACLES_HOST_DEFAULT_CATCH_UP_FRAMES */
    /* store_save receives the cartridge RAM every save_interval_frames frames
     * when it changed, and once at stop.  NULL disables saving. */
    uint32_t save_interval_frames;
    oracles_host_store_save_fn store_save;
    void *store_save_opaque;
    /* Receives ORACLES_HOST_EVENT_COMMAND events between two frames; NULL ignores them. */
    void (*on_command)(void *opaque, int command);
    void *on_command_opaque;
    /* Input replay: when it returns 1, *mask replaces the player's keys for
     * this frame (frames count from 0 at the first frame run). */
    int (*input_source)(void *opaque, uint32_t frame, unsigned *mask);
    void *input_source_opaque;
    /* A gameplay policy's say on the player's keys (the simulated press of the item hotkeys): applied to
     * the player's mask, never to a replayed one, which already carries it; what it returns is what is recorded. */
    unsigned (*input_filter)(void *opaque, uint32_t frame, unsigned mask);
    void *input_filter_opaque;
    /* Input record: the mask actually used for every frame. */
    void (*input_sink)(void *opaque, uint32_t frame, unsigned mask);
    void *input_sink_opaque;
    /* A host scene's hold on the keys (a mod's conversation or minigame): it receives the mask just recorded or
     * replayed, the player's, and returns the one the core receives.  Applied live and in a replay alike, after the
     * record: a route carries the keys the scene saw, and its replay gives the scene the same keys again. */
    unsigned (*input_hold)(void *opaque, uint32_t frame, unsigned mask);
    void *input_hold_opaque;
    /* Frame boundaries: before the core runs frame `frame`, and after the
     * frame ran and was presented, before any save or pacing. */
    void (*on_frame_begin)(void *opaque, uint32_t frame);
    void (*on_frame_end)(void *opaque, uint32_t frame);
    void *frame_opaque;
    /* When set, the 160x144 buffer it returns is presented instead of the
     * core's framebuffer (the native renderer). */
    const uint32_t *(*frame_source)(void *opaque);
    void *frame_source_opaque;
    /* When set and it returns 1, the part of the frame the backend shows alone (x, y, width, height), at its own
     * largest whole scale: the game's menus, which the Enhanced view shows framed, enlarged on a small screen. */
    int (*frame_crop)(void *opaque, uint32_t rect[4]);
    void *frame_crop_opaque;
    /* When set and it returns 1, the frame's band is moved by less than a pixel when it is shown (an uncropped frame). */
    int (*frame_subpixel)(void *opaque, oracles_host_video_subpixel *out);
    void *frame_subpixel_opaque;
    /* When set and it returns 1, the part of the frame the backend shows again above it larger (oracles_host_video_frame's
     * box): x, y, width, height, and whether it stands against its bottom edge; whatever it returns, the box's
     * reserve and size. */
    int (*frame_box)(void *opaque, uint32_t rect[4], int *bottom, uint32_t *reserve_w, unsigned *size);
    void *frame_box_opaque;
    /* The surface the frame source fills; 0 means the core screen (160x144). */
    uint32_t frame_width, frame_height;
    /* Once the backend has started (its window is open and sized), before the first frame: what must be ready before
     * the game runs, and so outside the frames' measure. */
    void (*on_started)(void *opaque);
    /* The pause (ORACLES_HOST_EVENT_PAUSE), between two frames: it holds the loop until it returns what to do.  The
     * core does not run meanwhile and no frame is counted, so a recorded route holds no frame of the pause; at the
     * resume the player's keys are released and the pacing starts again from the clock. */
    oracles_host_pause_result (*on_pause)(void *opaque);
    /* After a pause that resumes, when set: 1 when the presentation waits for the display from now on (vsync turned on
     * there), the host then watching that it does as with display_paces; 0 when it no longer does (turned off), the host
     * pacing; -1 when the pause left it as it was. */
    int (*display_paces_now)(void *opaque);
    /* With pace_frames: the presentation waits for the display's refresh (vsync on a display the host paces), which the
     * host watches does not slow the game (present_unpaced, present_waits_again).  present_waits_now, after a pause: whether it waits
     * from now on, vsync turned on again or off there; -1 when it stays. */
    int present_waits;
    int (*present_waits_now)(void *opaque);
    void *pause_opaque;   /* for on_started, on_pause, display_paces_now and present_waits_now */
} oracles_host_run_config;

typedef struct oracles_host_run_report {
    uint32_t frames_presented;
    unsigned final_buttons;
    uint32_t frames_late;
    uint32_t pacing_resyncs;
    /* display_paces: the frame from which the host paced, the display having
     * not; 0 when the display paced the whole run. */
    uint32_t display_unpaced_frame;
    /* With present_waits: the first frame from which the host asked the presentation not to wait for the display any
     * more (present_unpaced), its waits for the display's refresh seeming to slow the game; 0 when it never asked.
     * present_rewaited_frame: the first frame from which it had it wait again (present_waits_again), the game no
     * faster without the waits; 0: never.  present_waits_end: whether the presentation still waited at the end (never
     * asked, or asked to wait again, by the host or a pause).  frames_late_waiting: the frames late while the
     * presentation waited whose work fitted in a frame, in a slot of two frames at most, the wait's (the frames
     * measured held the Game Boy's rate, or did once the presentation no longer waited), counted apart from
     * frames_late; the others are in frames_late. */
    uint32_t present_unwaited_frame;
    uint32_t present_rewaited_frame;
    int present_waits_end;
    uint32_t frames_late_waiting;
    uint32_t saves_written;
    /* Time from the start of a frame (events polled) to its presentation,
     * measured with the backend clock when pacing; zero otherwise. */
    uint64_t frame_ns_total;
    uint64_t present_ns_total;     /* of which the backend's presentation: under vsync it waits for the display */
    uint64_t frame_ns_max;
    uint32_t frame_max_index;      /* the frame that took frame_ns_max */
    /* The same frame's work in its phases, and each phase's own worst:
     * the core's frame (hooks and their listeners included), the frame
     * source (the Enhanced composition), the backend's presentation, and
     * the end-of-frame callback. */
    uint64_t max_frame_core_ns, max_frame_source_ns, max_frame_present_ns, max_frame_end_ns;
    uint64_t core_ns_max, source_ns_max, present_ns_max, end_ns_max;
    uint32_t frames_over_12ms, frames_over_16ms;
    /* The frame budget the launcher holds to: frames past the session's start
     * (ORACLES_HOST_WARMUP_FRAMES: the first frames open the window and fill
     * the caches) whose work, the presentation left out (under vsync it waits
     * for the display), took more than one frame. */
    uint32_t frames_over_budget;
    uint32_t pauses;               /* times the pause held the loop, the one that ended the run included */
} oracles_host_run_report;

#define ORACLES_HOST_WARMUP_FRAMES 60u

typedef enum oracles_host_result {
    ORACLES_HOST_OK = 0,
    ORACLES_HOST_INVALID_ARGUMENT = 1,
    ORACLES_HOST_BACKEND_FAILED = 3,
    ORACLES_HOST_SAVE_FAILED = 4
} oracles_host_result;

int oracles_host_run(const oracles_host_run_config *config, const oracles_host_backend *backend,
                     oracles_host_run_report *report, char *error, size_t error_capacity);

#ifdef __cplusplus
}
#endif

#endif
