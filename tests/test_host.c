/* The host loop drives the core through the null backend, without any ROM:
 * a synthetic image with an Oracle header and a two-byte program. */
#include "backends.h"
#include "core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

static int stores;

static int store_save(void *opaque, const uint8_t *buffer, size_t size)
{
    (void)opaque;
    stores++;
    return size == 8192 && buffer[123] == 0x5a;
}

/* Pokes the cartridge RAM on the 50th poll, i.e. during the run, through the core's direct access. */
static OraclesCore *poked_core;
static int (*original_poll)(void *, oracles_host_event *, int *);
static int polls;

/* A route of two events: A pressed at frame 10, released at frame 20 (the route ends there). */
static int replay_source(void *opaque, uint32_t frame, unsigned *mask)
{
    (void)opaque;
    if (frame >= 20) return 0;
    *mask = frame >= 10 ? ORACLES_KEY_A : 0;
    return 1;
}

static unsigned recorded[64];
static size_t recorded_count;

static void record_sink(void *opaque, uint32_t frame, unsigned mask)
{
    (void)opaque;
    if (frame < 64) { recorded[frame] = mask; recorded_count = frame + 1; }
}

static int poking_poll_event(void *opaque, oracles_host_event *event, int *has_event)
{
    if (++polls == 50) {
        size_t size = 0;
        uint16_t bank = 0;
        uint8_t *sram = oracles_core_memory(poked_core, ORACLES_CORE_CART_RAM, &size, &bank);
        if (sram && size > 123) sram[123] = 0x5a;
    }
    return original_poll(opaque, event, has_event);
}

/* A display under vsync: the present waits display_period_ns on the null backend's clock (0: it does not wait). */
static int (*original_present)(void *, const oracles_host_video_frame *);
static int (*original_sleep)(void *, uint64_t);
static uint64_t display_period_ns;
static int unpaced_calls;
/* The presents so far; before the one numbered display_waits_from, and from the one numbered display_stops_waiting on
 * (0: never), the present returns at once; before the frame display_pause_before (0: none), the player asks for a
 * pause, which lasts two seconds. */
static uint32_t display_presents, display_waits_from, display_stops_waiting, display_pause_before;
static int display_paused;
static int (*display_base_poll)(void *, oracles_host_event *, int *);
static int (*original_display_poll)(void *, oracles_host_event *, int *);
static uint64_t loop_overhead_ns;   /* spent polling events, outside a frame's timed work */

static int overhead_poll(void *opaque, oracles_host_event *event, int *has_event)
{
    return original_display_poll(opaque, event, has_event) && (*has_event || loop_overhead_ns == 0 || original_sleep(opaque, loop_overhead_ns));
}

static int display_present(void *opaque, const oracles_host_video_frame *frame)
{
    const int waits = display_period_ns != 0 && display_presents >= display_waits_from && (display_stops_waiting == 0 || display_presents < display_stops_waiting);
    display_presents++;
    return original_present(opaque, frame) && (!waits || original_sleep(opaque, display_period_ns));
}

static int display_poll(void *opaque, oracles_host_event *event, int *has_event)
{
    if (display_pause_before && !display_paused && display_presents == display_pause_before) {
        display_paused = 1;
        memset(event, 0, sizeof *event);
        event->type = ORACLES_HOST_EVENT_PAUSE;
        *has_event = 1;
        return 1;
    }
    return display_base_poll(opaque, event, has_event);
}

static oracles_host_pause_result two_second_pause(void *opaque)
{
    original_sleep(opaque, UINT64_C(2000000000));
    return ORACLES_HOST_PAUSE_RESUME;
}

static int count_unpaced(void *opaque) { (void)opaque; unpaced_calls++; return 1; }

/* A display that refreshes every refresh_ns (0: a present that never waits): until the host asks it not to wait
 * (refresh_unwaited), the present returns at its next refresh, as a renderer under vsync whose presentation blocks
 * does, after present_cost_ns of drawing; each frame's work but its presentation takes frame_work_ns before it, as the
 * frame source's and the view's do, and heavy_ns more every heavy_every frames. */
static uint64_t (*original_clock)(void *);
static uint64_t refresh_ns, frame_work_ns, hitch_ns, present_cost_ns, heavy_ns;
static uint32_t hitch_frame;   /* the frame whose work takes hitch_ns more (0: none) */
static uint32_t heavy_every;
static int refresh_unwaited, waits_after_pause = -1, rewait_calls;
static int refresh_forced;   /* a driver that keeps vsync: the present goes on waiting, present_unpaced fails */
static uint32_t refresh_late_waiting;   /* the last run's frames_late_waiting */
static uint32_t refresh_rewaited;       /* its present_rewaited_frame */
static int refresh_waits_end;           /* and its present_waits_end */
static OraclesCore *refresh_core;
static void *refresh_opaque;

static int refresh_present(void *opaque, const oracles_host_video_frame *frame)
{
    display_presents++;
    if (!original_present(opaque, frame)) return 0;
    if (present_cost_ns && !original_sleep(opaque, present_cost_ns)) return 0;
    if (refresh_unwaited || refresh_ns == 0) return 1;
    const uint64_t now = original_clock(opaque);
    return original_sleep(opaque, refresh_ns - now % refresh_ns);
}

static int refresh_unpaced(void *opaque) { (void)opaque; unpaced_calls++; if (!refresh_forced) refresh_unwaited = 1; return !refresh_forced; }
static int refresh_waits_again(void *opaque) { (void)opaque; rewait_calls++; refresh_unwaited = 0; return 1; }
static const uint32_t *frame_work(void *opaque)
{
    (void)opaque;
    const uint32_t frame = display_presents;
    if (frame_work_ns) original_sleep(refresh_opaque, frame_work_ns);
    if (hitch_frame && frame == hitch_frame) original_sleep(refresh_opaque, hitch_ns);
    if (heavy_every && frame % heavy_every == heavy_every - 1u) original_sleep(refresh_opaque, heavy_ns);
    return oracles_core_pixels(refresh_core);
}

/* The pause turned vsync on again (1), the presents waiting again, or off (0); -1: it left it. */
static int waits_now(void *opaque) { (void)opaque; if (waits_after_pause == 1) refresh_unwaited = 0; return waits_after_pause; }

/* A run of 600 frames on that display, the host pacing (`host_paces`) or watching the display pace: the frame the host
 * took over from (0: none), its late frames, the frame from which it asked the presents not to wait (`unwaited`), and
 * how long the run took on the null backend's clock. */
static uint32_t refresh_run(oracles_host_backend *backend, OraclesCore *core, int host_paces, uint32_t *late, uint32_t *unwaited, uint64_t *run_ns)
{
    oracles_host_run_config config;
    oracles_host_run_report report;
    char error[128];
    memset(&config, 0, sizeof config);
    config.core = core;
    config.quit_after_frames = 600;
    config.pace_frames = host_paces;
    config.display_paces = !host_paces;
    config.present_waits = host_paces;
    config.measure_frames = 1;
    config.frame_source = frame_work;
    config.on_pause = two_second_pause;
    config.present_waits_now = waits_now;
    config.pause_opaque = backend->opaque;
    display_presents = 0;
    display_paused = 0;
    unpaced_calls = rewait_calls = 0;
    refresh_unwaited = 0;
    const uint64_t start = original_clock(backend->opaque);
    CHECK(oracles_host_run(&config, backend, &report, error, sizeof error) == ORACLES_HOST_OK && report.frames_presented == 600);
    *run_ns = original_clock(backend->opaque) - start;
    *late = report.frames_late;
    *unwaited = report.present_unwaited_frame;
    refresh_late_waiting = report.frames_late_waiting;
    refresh_rewaited = report.present_rewaited_frame;
    refresh_waits_end = report.present_waits_end;
    if (getenv("HOST_DEBUG")) fprintf(stderr, "DEBUG refresh %.0f Hz work %.1f ms: unpaced %u calls %d/%d unwaited %u rewaited %u late %u waiting %u end %d run %.3f s\n",
        refresh_ns ? 1e9 / (double)refresh_ns : 0.0, frame_work_ns / 1e6, report.display_unpaced_frame, unpaced_calls, rewait_calls, *unwaited, refresh_rewaited, *late,
        refresh_late_waiting, refresh_waits_end, *run_ns / 1e9);
    return report.display_unpaced_frame;
}

/* A run under vsync whose presentation waits display_period_ns per frame: the display paces. */
static uint32_t unpaced_frame(oracles_host_backend *backend, OraclesCore *core, uint64_t period_ns, uint32_t *late)
{
    oracles_host_run_config config;
    oracles_host_run_report report;
    char error[128];
    memset(&config, 0, sizeof config);
    config.core = core;
    config.quit_after_frames = 300;
    config.display_paces = 1;
    display_period_ns = period_ns;
    display_presents = 0;
    unpaced_calls = 0;
    CHECK(oracles_host_run(&config, backend, &report, error, sizeof error) == ORACLES_HOST_OK && report.frames_presented == 300);
    CHECK(unpaced_calls == (report.display_unpaced_frame && backend->present_unpaced ? 1 : 0));
    *late = report.frames_late;
    return report.display_unpaced_frame;
}

/* The same over 600 frames, the present stopping to wait at the frame stops_waiting (0: never), with a pause of two
 * seconds before the frame pause_before (0: none). */
static uint32_t paused_unpaced_frame(oracles_host_backend *backend, OraclesCore *core, uint64_t period_ns, uint32_t stops_waiting, uint32_t pause_before)
{
    oracles_host_run_config config;
    oracles_host_run_report report;
    char error[128];
    memset(&config, 0, sizeof config);
    config.core = core;
    config.quit_after_frames = 600;
    config.display_paces = 1;
    config.on_pause = two_second_pause;
    config.pause_opaque = backend->opaque;
    display_period_ns = period_ns;
    display_stops_waiting = stops_waiting;
    display_pause_before = pause_before;
    display_presents = 0;
    display_paused = 0;
    CHECK(oracles_host_run(&config, backend, &report, error, sizeof error) == ORACLES_HOST_OK && report.frames_presented == 600);
    CHECK(report.pauses == (pause_before ? 1u : 0u));
    display_stops_waiting = display_pause_before = 0;
    return report.display_unpaced_frame;
}

/* Vsync changed from the pause: the display paces from then on (1) or the host does (0). */
static int paces_after_pause;
static int paces_now(void *opaque) { (void)opaque; return paces_after_pause; }

/* A run of 600 frames whose present returns at once, started under vsync (`vsync`) or paced by the host, vsync turned
 * on or off (`after`) by a pause before the frame pause_before: the frame the host took over from a display that did not
 * pace, 0 for none. */
static uint32_t vsync_changed_frame(oracles_host_backend *backend, OraclesCore *core, int vsync, int after, uint32_t pause_before)
{
    oracles_host_run_config config;
    oracles_host_run_report report;
    char error[128];
    memset(&config, 0, sizeof config);
    config.core = core;
    config.quit_after_frames = 600;
    config.pace_frames = !vsync;
    config.display_paces = vsync;
    config.on_pause = two_second_pause;
    config.display_paces_now = paces_now;
    config.pause_opaque = backend->opaque;
    paces_after_pause = after;
    display_period_ns = 0;
    display_pause_before = pause_before;
    display_presents = 0;
    display_paused = 0;
    unpaced_calls = 0;
    CHECK(oracles_host_run(&config, backend, &report, error, sizeof error) == ORACLES_HOST_OK && report.frames_presented == 600);
    CHECK(unpaced_calls == (report.display_unpaced_frame ? 1 : 0));
    display_pause_before = 0;
    return report.display_unpaced_frame;
}

static unsigned press_b_filter(void *opaque, uint32_t frame, unsigned mask) { (void)opaque; (void)frame; return mask | ORACLES_KEY_B; }

/* The player holds Right, asks for the pause before the 16th frame, and nothing else. */
static int (*plain_poll)(void *, oracles_host_event *, int *);
static int pause_polls, pauses_seen, started_calls;
static oracles_host_pause_result pause_answer;

static int pausing_poll_event(void *opaque, oracles_host_event *event, int *has_event)
{
    pause_polls++;
    if (pause_polls == 1) { memset(event, 0, sizeof *event); event->type = ORACLES_HOST_EVENT_BUTTON; event->button = ORACLES_KEY_RIGHT; event->pressed = 1; *has_event = 1; return 1; }
    if (pause_polls == 32) { memset(event, 0, sizeof *event); event->type = ORACLES_HOST_EVENT_PAUSE; *has_event = 1; return 1; }
    return plain_poll(opaque, event, has_event);
}

static oracles_host_pause_result on_pause(void *opaque) { (void)opaque; pauses_seen++; return pause_answer; }

/* The game changes its cartridge RAM on the 5th poll, and the system suspends the application on the 10th: the
 * backend calls the host's store at once, as SDL's event watch does inside a poll or a present, then the suspension's
 * event is polled once the application runs again. */
static int suspend_polls, stores_at_pause, stores_at_suspension;
static void (*suspend_callback)(void *);
static void *suspend_opaque;

static void watch_suspend(void *opaque, void (*callback)(void *), void *callback_opaque)
{
    (void)opaque;
    suspend_callback = callback;
    suspend_opaque = callback_opaque;
}

static int suspending_poll_event(void *opaque, oracles_host_event *event, int *has_event)
{
    suspend_polls++;
    if (suspend_polls == 5) {
        size_t size = 0;
        uint16_t bank = 0;
        uint8_t *sram = oracles_core_memory(poked_core, ORACLES_CORE_CART_RAM, &size, &bank);
        if (sram && size > 124) sram[124] ^= 1;
    }
    if (suspend_polls == 10) {
        if (suspend_callback) { suspend_callback(suspend_opaque); stores_at_suspension = stores; }
        memset(event, 0, sizeof *event);
        event->type = ORACLES_HOST_EVENT_SUSPEND;
        *has_event = 1;
        return 1;
    }
    return plain_poll(opaque, event, has_event);
}

static oracles_host_pause_result pause_after_store(void *opaque) { (void)opaque; stores_at_pause = stores; pauses_seen++; return ORACLES_HOST_PAUSE_RESUME; }
static void on_started(void *opaque) { (void)opaque; started_calls++; }

int main(void)
{
    const size_t size = 1024u * 1024u;
    uint8_t *rom = calloc(size, 1);
    memcpy(rom + 0x134, "ZELDA NAYRU", 11);
    rom[0x143] = 0xc0; rom[0x147] = 0x1b; rom[0x148] = 0x05; rom[0x149] = 0x02;
    rom[0x100] = 0x00; rom[0x101] = 0xc3; rom[0x102] = 0x50; rom[0x103] = 0x01; /* nop ; jp $0150 */
    rom[0x150] = 0x18; rom[0x151] = 0xfe;                                       /* jr -2 */

    const OraclesCoreOptions options = { 48000, 0, ORACLES_CORE_SAMEBOY, 0 };
    OraclesCore *core = oracles_core_create(rom, size, &options);
    free(rom);
    CHECK(core != NULL);
    if (!core) return 1;
    CHECK(oracles_core_sram_size(core) == 8192);

    oracles_host_backend backend;
    CHECK(oracles_null_backend_init(&backend));
    oracles_host_run_config config;
    memset(&config, 0, sizeof config);
    config.core = core;
    config.sample_rate_hz = 48000;
    config.quit_after_frames = 300;
    config.pace_frames = 1;
    config.save_interval_frames = 100;
    config.store_save = store_save;

    oracles_host_run_report report;
    char error[128];
    const int result = oracles_host_run(&config, &backend, &report, error, sizeof error);
    CHECK(result == ORACLES_HOST_OK);
    if (result != ORACLES_HOST_OK) fprintf(stderr, "%s\n", error);
    CHECK(report.frames_presented == 300);
    CHECK(report.frames_late == 0);
    CHECK(stores == 0); /* the SRAM never changed */
    /* The frame budget: the null backend's clock moves a microsecond a reading, so no frame's work nears one frame. */
    CHECK(report.frame_ns_total > 0 && report.present_ns_total > 0 && report.present_ns_total < report.frame_ns_total);
    CHECK(report.frames_over_budget == 0);
    {   /* Without pacing (the display paces under vsync) the work is timed when asked, and only then. */
        oracles_host_run_config unpaced = config;
        oracles_host_run_report timed, untimed;
        unpaced.pace_frames = 0; unpaced.quit_after_frames = 10; unpaced.store_save = NULL; unpaced.save_interval_frames = 0;
        CHECK(oracles_host_run(&unpaced, &backend, &untimed, error, sizeof error) == ORACLES_HOST_OK && untimed.frame_ns_total == 0);
        unpaced.measure_frames = 1;
        CHECK(oracles_host_run(&unpaced, &backend, &timed, error, sizeof error) == ORACLES_HOST_OK && timed.frame_ns_total > 0 && timed.frames_late == 0);
    }

    {   /* Under vsync the host checks that the display paces. */
        oracles_host_backend display = backend;
        uint32_t late = 0;
        original_present = backend.present_frame;
        original_sleep = backend.sleep_ns;
        display.present_frame = display_present;
        display.present_unpaced = count_unpaced;
        display_base_poll = backend.poll_event;
        original_display_poll = display_poll;
        display.poll_event = overhead_poll;
        CHECK(unpaced_frame(&display, core, 16742706u, &late) == 0);   /* 59.7275 Hz */
        CHECK(unpaced_frame(&display, core, 16666667u, &late) == 0);   /* 60 Hz */
        CHECK(unpaced_frame(&display, core, 16393443u, &late) == 0);   /* 61 Hz, the fastest display vsync=auto accepts */
        CHECK(unpaced_frame(&display, core, 16000000u, &late) == 120 && late == 0);   /* a 62.5 Hz clock that is no display */
        CHECK(unpaced_frame(&display, core, 0, &late) == 120 && late == 0);           /* a present that returns at once */
        /* The first second is not judged: a window being opened and sized, whose presents do not wait yet, then a
         * display that paces; the floor holds those frames to 62.5 Hz meanwhile. */
        display_waits_from = 60;
        CHECK(unpaced_frame(&display, core, 16666667u, &late) == 0);
        display_waits_from = 0;
        /* A pause makes a frame slow, never fast: two seconds of it in the middle of a run neither make the host take
         * over from a display that paces, nor hide a presentation that stops waiting at the sixth second, whose
         * takeover comes at the frame it comes without a pause, the pause before or inside the window that finds it. */
        CHECK(paused_unpaced_frame(&display, core, 16666667u, 0, 300) == 0);
        const uint32_t takeover = paused_unpaced_frame(&display, core, 16666667u, 360, 0);
        CHECK(takeover == 420);
        CHECK(paused_unpaced_frame(&display, core, 16666667u, 360, 300) == takeover);
        CHECK(paused_unpaced_frame(&display, core, 16666667u, 360, 390) == takeover);
        CHECK(paused_unpaced_frame(&display, core, 0, 0, 90) == 120);   /* no present waits, the pause in the first window judged */
        loop_overhead_ns = 1000000u;   /* the floor is a deadline: the loop's own time between frames does not hide it */
        CHECK(unpaced_frame(&display, core, 0, &late) == 120);
        CHECK(unpaced_frame(&display, core, 16666667u, &late) == 0);
        CHECK(paused_unpaced_frame(&display, core, 16666667u, 360, 390) == takeover);
        loop_overhead_ns = 0;
        /* Vsync turned off from the pause: the host paces from the resume, and a present that does not wait is no
         * longer judged; turned on, the host watches the display from the resume, its first window judged there; a
         * pause that leaves it changes nothing. */
        CHECK(vsync_changed_frame(&display, core, 1, 0, 90) == 0);
        CHECK(vsync_changed_frame(&display, core, 0, 1, 300) == 360);
        CHECK(vsync_changed_frame(&display, core, 0, -1, 300) == 0);
        CHECK(vsync_changed_frame(&display, core, 1, -1, 90) == 120);
        display.present_unpaced = NULL;   /* optional */
        CHECK(unpaced_frame(&display, core, 0, &late) == 120);
    }

    {   /* A display of 144 Hz, whose presents under vsync return at its next refresh (6.94 ms at most).  Watched, it is
         * no display that paces at about 60 Hz: the floor's 62.5 Hz shows through, and the host takes over after the
         * window that ends at frame 120, which is why the launcher leaves a display above 61 Hz to the host. */
        oracles_host_backend display = backend;
        uint32_t late = 0, unwaited = 0;
        uint64_t run_ns = 0;
        const uint64_t game_ns = UINT64_C(600) * 16742706u;
        original_present = backend.present_frame;
        original_sleep = backend.sleep_ns;
        original_clock = backend.monotonic_ns;
        display.present_frame = refresh_present;
        display.present_unpaced = refresh_unpaced;
        display.present_waits_again = refresh_waits_again;
        refresh_core = core;
        refresh_opaque = display.opaque;
        refresh_ns = 6944444u;
        frame_work_ns = 4000000u;
        CHECK(refresh_run(&display, core, 0, &late, &unwaited, &run_ns) == 120 && unpaced_calls == 1 && unwaited == 0);
        /* Paced by the host, 144 and 120 Hz: with 4 ms of work a frame, and with 12, some frames late for the wait
         * but the next on time again, the game keeps the Game Boy's rate and vsync stays on; the frames late for the
         * wait are counted apart from the game's. */
        static const uint64_t refreshes[] = { 6944444u, 8333333u };
        static const uint64_t works[] = { 4000000u, 12000000u };
        for (size_t r = 0; r < 2; r++)
            for (size_t w = 0; w < 2; w++) {
                refresh_ns = refreshes[r];
                frame_work_ns = works[w];
                CHECK(refresh_run(&display, core, 1, &late, &unwaited, &run_ns) == 0 && unpaced_calls == 0 && unwaited == 0 && refresh_waits_end);
                CHECK(late == 0 && run_ns <= game_ns + UINT64_C(2) * 16742706u);
            }
        /* 15 ms at 144 Hz: each frame waits for the third refresh, 48 frames a second; the host turns vsync off after
         * its first window past the first second, and the game keeps its rate from there. */
        refresh_ns = 6944444u;
        frame_work_ns = 15000000u;
        CHECK(refresh_run(&display, core, 1, &late, &unwaited, &run_ns) == 0 && unpaced_calls == 1 && rewait_calls == 0 && !refresh_waits_end);
        CHECK(unwaited > 100u && unwaited <= 130u && late == 0 && refresh_late_waiting > 0 && refresh_rewaited == 0);
        CHECK(run_ns <= game_ns + UINT64_C(40) * 16742706u);
        /* A driver that keeps vsync: the host cannot turn it off, says nothing of it, and the frames late are the
         * game's. */
        refresh_forced = 1;
        CHECK(refresh_run(&display, core, 1, &late, &unwaited, &run_ns) == 0 && unpaced_calls == 1 && rewait_calls == 0 && unwaited == 0);
        CHECK(refresh_waits_end && late > 400u);
        refresh_forced = 0;
        /* 14 ms with a frame of 18 every 30: a heavy frame weighs no more than its work, and the host turns vsync off
         * as soon. */
        frame_work_ns = 14000000u;
        heavy_every = 30;
        heavy_ns = 4000000u;
        CHECK(refresh_run(&display, core, 1, &late, &unwaited, &run_ns) == 0 && unpaced_calls == 1 && rewait_calls == 0);
        CHECK(unwaited > 100u && unwaited <= 130u && run_ns <= game_ns + UINT64_C(40) * 16742706u);
        heavy_every = 0;
        /* A presentation slower than its share of a frame without waiting, 15 ms of work and 2 ms of drawing in a
         * present that returns at once: the game runs 1.5 % slow, too little to turn anything off, and its frames late
         * are the game's. */
        refresh_ns = 0;
        frame_work_ns = 15000000u;
        present_cost_ns = 2000000u;
        CHECK(refresh_run(&display, core, 1, &late, &unwaited, &run_ns) == 0 && unpaced_calls == 0 && unwaited == 0 && late > 400u);
        /* A presentation bound by the graphics processor, 12 ms of work and 6 of drawing, none of it waiting: the host
         * turns vsync off after the first window judged, the next runs no faster, and it turns vsync on again, the
         * frames late the game's. */
        frame_work_ns = 12000000u;
        present_cost_ns = 6000000u;
        CHECK(refresh_run(&display, core, 1, &late, &unwaited, &run_ns) == 0 && unpaced_calls == 1 && rewait_calls == 1 && refresh_waits_end);
        CHECK(unwaited > 100u && unwaited <= 130u && refresh_rewaited > unwaited && refresh_rewaited <= unwaited + 70u && late > 500u);
        /* The same, a pause leaving vsync as it was: watched again from the resume, the host turns vsync off and on
         * once more, each hook called twice. */
        display.poll_event = display_poll;
        display_base_poll = backend.poll_event;
        display_pause_before = 300;
        CHECK(refresh_run(&display, core, 1, &late, &unwaited, &run_ns) == 0 && unpaced_calls == 2 && rewait_calls == 2 && refresh_waits_end && late > 400u);
        display_pause_before = 0;
        display.poll_event = backend.poll_event;
        /* A presentation that takes two frames and more, 4 ms of work and 30 of drawing: the slots too long to be
         * judged, its frames late are the game's. */
        frame_work_ns = 4000000u;
        present_cost_ns = 30000000u;
        CHECK(refresh_run(&display, core, 1, &late, &unwaited, &run_ns) == 0 && unpaced_calls == 0 && unwaited == 0 && late > 500u);
        present_cost_ns = 0;
        refresh_ns = 6944444u;
        /* Work that does not fit in a frame, 18 ms, waiting for the third refresh at 144 Hz: the game is slow of
         * itself, and slower still for the waits, 48 frames a second against 55.5 without them; the host turns vsync
         * off, the frames late the game's. */
        frame_work_ns = 18000000u;
        CHECK(refresh_run(&display, core, 1, &late, &unwaited, &run_ns) == 0 && unpaced_calls == 1 && rewait_calls == 0 && unwaited > 100u && unwaited <= 130u);
        CHECK(late > 500u && !refresh_waits_end);
        /* A single long frame, 60 ms, and the frames that catch up after it: vsync stays, on a display whose presents
         * wait as on one whose presents queue. */
        frame_work_ns = 4000000u;
        hitch_frame = 300;
        hitch_ns = 60000000u;
        CHECK(refresh_run(&display, core, 1, &late, &unwaited, &run_ns) == 0 && unpaced_calls == 0 && unwaited == 0);
        refresh_unwaited = 1;   /* presents that queue, never waiting */
        {
            oracles_host_run_config config;
            oracles_host_run_report report;
            char error[128];
            memset(&config, 0, sizeof config);
            config.core = core;
            config.quit_after_frames = 600;
            config.pace_frames = config.present_waits = config.measure_frames = 1;
            config.frame_source = frame_work;
            unpaced_calls = 0;
            CHECK(oracles_host_run(&config, &display, &report, error, sizeof error) == ORACLES_HOST_OK && report.present_unwaited_frame == 0 && unpaced_calls == 0);
        }
        hitch_frame = 0;
        /* A display slower than the game, a television at 50 Hz, should vsync be on there: the game would run at the
         * display's rate; the host turns vsync off after its first window. */
        refresh_ns = 20000000u;
        CHECK(refresh_run(&display, core, 1, &late, &unwaited, &run_ns) == 0 && unpaced_calls == 1 && rewait_calls == 0 && unwaited > 100u && unwaited <= 130u);
        CHECK(run_ns <= game_ns + UINT64_C(40) * 16742706u);
        /* A pause after the host had turned vsync off: watched again, the presents waiting again, whether the pause
         * turned vsync on again or left it, the host turns it off again, the report keeping the first frame. */
        refresh_ns = 6944444u;
        frame_work_ns = 15000000u;
        display.poll_event = display_poll;
        display_base_poll = backend.poll_event;
        display_pause_before = 400;
        waits_after_pause = 1;
        CHECK(refresh_run(&display, core, 1, &late, &unwaited, &run_ns) == 0 && unpaced_calls == 2 && rewait_calls == 0 && unwaited <= 130u && !refresh_waits_end);
        waits_after_pause = -1;
        CHECK(refresh_run(&display, core, 1, &late, &unwaited, &run_ns) == 0 && unpaced_calls == 2 && rewait_calls == 1 && unwaited <= 130u && !refresh_waits_end);
        /* Vsync turned off from the pause while the host judged a window without the waits: the presents are not
         * asked to wait again, and the frames held late are the wait's. */
        display_pause_before = 150;
        waits_after_pause = 0;
        CHECK(refresh_run(&display, core, 1, &late, &unwaited, &run_ns) == 0 && unpaced_calls == 1 && rewait_calls == 0 && !refresh_waits_end);
        CHECK(unwaited <= 130u && late == 0 && refresh_late_waiting > 0 && refresh_rewaited == 0);
        waits_after_pause = -1;
        display_pause_before = 0;
        display.poll_event = backend.poll_event;
        /* Without present_waits (vsync off, the host pacing), nothing is asked and every late frame is the game's. */
        {
            oracles_host_run_config config;
            oracles_host_run_report report;
            char error[128];
            memset(&config, 0, sizeof config);
            config.core = core;
            config.quit_after_frames = 300;
            config.pace_frames = config.measure_frames = 1;
            config.frame_source = frame_work;
            unpaced_calls = 0;
            refresh_unwaited = 0;
            CHECK(oracles_host_run(&config, &display, &report, error, sizeof error) == ORACLES_HOST_OK && report.present_unwaited_frame == 0 && unpaced_calls == 0);
            CHECK(report.frames_late_waiting == 0 && report.frames_late > 0 && !report.present_waits_end);
        }
        /* Without the backend's present_unpaced, or its present_waits_again, nothing to ask, the presents waiting to
         * the end, and the frames late are the game's. */
        display.present_waits_again = NULL;
        CHECK(refresh_run(&display, core, 1, &late, &unwaited, &run_ns) == 0 && unwaited == 0 && unpaced_calls == 0 && refresh_late_waiting == 0 && late > 500u);
        CHECK(refresh_waits_end);
        display.present_waits_again = refresh_waits_again;
        display.present_unpaced = NULL;
        CHECK(refresh_run(&display, core, 1, &late, &unwaited, &run_ns) == 0 && unwaited == 0 && refresh_late_waiting == 0 && late > 500u && refresh_waits_end);
        frame_work_ns = 0;
    }

    /* The core has rendered frames: every pixel went through the encoder (opaque alpha). */
    const uint32_t *pixels = oracles_core_pixels(core);
    int encoded = 1;
    for (size_t i = 0; i < ORACLES_SCREEN_PIXELS && encoded; i++) if ((pixels[i] >> 24) != 0xffu) encoded = 0;
    CHECK(encoded);

    /* A change of cartridge RAM during the run is stored at the next interval and not again. */
    poked_core = core;
    original_poll = backend.poll_event;
    backend.poll_event = poking_poll_event;
    polls = 0;
    config.quit_after_frames = 250;
    memset(&report, 0, sizeof report);
    CHECK(oracles_host_run(&config, &backend, &report, error, sizeof error) == ORACLES_HOST_OK);
    CHECK(report.frames_presented == 250);
    CHECK(stores == 1);
    CHECK(report.saves_written == 1);

    /* Input replay and record through the loop: the source decides the keys, the sink sees them. */
    backend.poll_event = original_poll;
    config.input_source = replay_source;
    config.input_sink = record_sink;
    config.quit_after_frames = 40;
    recorded_count = 0;
    memset(&report, 0, sizeof report);
    CHECK(oracles_host_run(&config, &backend, &report, error, sizeof error) == ORACLES_HOST_OK);
    CHECK(recorded_count == 40);
    CHECK(recorded[0] == 0x00 && recorded[9] == 0x00);
    CHECK(recorded[10] == 0x10 && recorded[19] == 0x10);   /* A held from frame 10 */
    CHECK(recorded[20] == 0x00 && recorded[39] == 0x00);   /* route over at frame 20: back to the player, no keys */

    /* A gameplay policy's filter: it has its say on the player's keys, never on replayed ones, and what it returns is what is recorded. */
    config.input_filter = press_b_filter;
    recorded_count = 0;
    memset(&report, 0, sizeof report);
    CHECK(oracles_host_run(&config, &backend, &report, error, sizeof error) == ORACLES_HOST_OK);
    CHECK(recorded[10] == 0x10 && recorded[19] == 0x10);   /* the route's frames: untouched */
    CHECK(recorded[20] == 0x20 && recorded[39] == 0x20);   /* the player's: B pressed for him */

    /* The pause holds the loop between two frames: no frame is counted or recorded during it, the frames go on from
     * where they were, and the keys held before it are let go.  on_started comes once, before the first frame. */
    config.input_filter = NULL;
    config.input_source = NULL;
    config.on_started = on_started;
    config.on_pause = on_pause;
    plain_poll = original_poll;
    backend.poll_event = pausing_poll_event;
    pause_polls = pauses_seen = started_calls = 0;
    pause_answer = ORACLES_HOST_PAUSE_RESUME;
    recorded_count = 0;
    memset(recorded, 0xff, sizeof recorded);
    memset(&report, 0, sizeof report);
    CHECK(oracles_host_run(&config, &backend, &report, error, sizeof error) == ORACLES_HOST_OK);
    CHECK(started_calls == 1 && pauses_seen == 1 && report.pauses == 1);
    CHECK(report.frames_presented == 40 && recorded_count == 40);
    {
        int paused_at = -1, gaps = 0;
        for (int f = 0; f < 40; f++) {
            if (recorded[f] == 0xffu) gaps++;
            if (f > 0 && recorded[f - 1] == ORACLES_KEY_RIGHT && recorded[f] == 0) paused_at = f;
        }
        CHECK(gaps == 0);                                   /* every frame recorded once, none for the pause */
        CHECK(recorded[0] == ORACLES_KEY_RIGHT && paused_at > 0 && recorded[39] == 0);   /* Right let go at the pause */
    }
    /* Quit to launcher from the pause ends the run there, the save stored as at any end. */
    pause_polls = pauses_seen = 0;
    pause_answer = ORACLES_HOST_PAUSE_QUIT;
    memset(&report, 0, sizeof report);
    CHECK(oracles_host_run(&config, &backend, &report, error, sizeof error) == ORACLES_HOST_OK);
    CHECK(pauses_seen == 1 && report.pauses == 1 && report.frames_presented > 0 && report.frames_presented < 40);
    /* Without on_pause, the pause's event ends the run as Escape did. */
    config.on_pause = NULL;
    pause_polls = 0;
    memset(&report, 0, sizeof report);
    CHECK(oracles_host_run(&config, &backend, &report, error, sizeof error) == ORACLES_HOST_OK && report.frames_presented < 40);
    /* The system suspends the application: the cartridge RAM changed since the last store is stored at once, by the
     * backend's watch, then the suspension's event brings the pause's menu; without a menu (--rom) the game goes on.
     * A backend without a watch stores it when the event is polled. */
    backend.poll_event = suspending_poll_event;
    backend.watch_suspend = watch_suspend;
    config.on_pause = pause_after_store;
    suspend_polls = pauses_seen = stores = stores_at_pause = stores_at_suspension = 0;
    memset(&report, 0, sizeof report);
    CHECK(oracles_host_run(&config, &backend, &report, error, sizeof error) == ORACLES_HOST_OK);
    CHECK(stores_at_suspension == 1 && stores_at_pause == 1 && pauses_seen == 1 && report.pauses == 1);
    CHECK(report.saves_written == 1 && report.frames_presented == 40 && suspend_callback == NULL);   /* withdrawn at the end */
    config.on_pause = NULL;
    suspend_polls = stores = stores_at_suspension = 0;
    memset(&report, 0, sizeof report);
    CHECK(oracles_host_run(&config, &backend, &report, error, sizeof error) == ORACLES_HOST_OK);
    CHECK(stores_at_suspension == 1 && stores == 1 && report.saves_written == 1 && report.frames_presented == 40);
    backend.watch_suspend = NULL;
    suspend_polls = stores = 0;
    memset(&report, 0, sizeof report);
    CHECK(oracles_host_run(&config, &backend, &report, error, sizeof error) == ORACLES_HOST_OK);
    CHECK(stores == 1 && report.saves_written == 1 && report.frames_presented == 40);

    oracles_null_backend_release(&backend);
    oracles_core_destroy(core);
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_host: ok\n");
    return 0;
}
