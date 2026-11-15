/* The pops of the sound circuits are not heard, on either core, without any ROM: a synthetic cartridge boots the
 * embedded boot ROM, whose third frame turns the DACs on (a step of thousands, brought back by the high-pass within a
 * few frames), then plays a steady tone.  The frames until ORACLES_CORE_POWER_ON_QUIET_FRAMES are silence (their
 * samples zeroed, not dropped) and the boot sound after them is heard.  A state loaded in the tone, into a core just
 * created (whose sound circuits start again from nothing), in its power-on silence, into the running core with samples
 * still queued and again in the fade, fades in from the last sample the host had, with no step, and the tone is back at
 * its level once the fade is over; after the host cleared its audio (the pause), with or without a load, it fades in
 * from silence.  The host loop does that clearing at the pause, before its menu, and at a suspension without one
 * (--rom), when the backend cuts its sound; with a backend that cannot, the sound goes on. */
#include "backends.h"
#include "core.h"
#include "core_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

enum { RATE = 48000, BOOT_SOUND_BY = 90, TONE_FROM = 300 };

static int16_t samples[ORACLES_CORE_AUDIO_FRAMES * 2];

/* Runs `frames` frames, then takes their audio: its loudest sample; the stereo frames taken in *taken. */
static int run_peak(OraclesCore *core, unsigned frames, size_t *taken)
{
    for (unsigned i = 0; i < frames; i++) oracles_core_run_frame(core);
    const size_t count = oracles_core_take_audio(core, samples, sizeof samples / sizeof *samples);
    if (taken) *taken = count / 2;
    int peak = 0;
    for (size_t i = 0; i < count; i++) {
        const int v = samples[i] < 0 ? -samples[i] : samples[i];
        if (v > peak) peak = v;
    }
    return peak;
}

/* After a load (`state`) or the host's audio cleared (none), the next frame taken: past the samples still queued before
 * a load, within a tenth of the tone of the last sample the host had (`before`, or the last of those queued; silence
 * once the audio was cleared, the samples queued dropped with it), the gain ending that frame at 1/27 where a step
 * moves by the tone or more; no frame of the fade louder than the tone, which a mix of the last sample and the tone
 * cannot pass; then the tone at its level. */
static void check_fade(const char *what, OraclesCore *core, const uint8_t *state, size_t state_size, int tone_peak,
                       const int16_t before[2])
{
    const size_t pending = state ? core->audio_count : 0;
    if (state) CHECK(oracles_core_load_state(core, state, state_size) == 0);
    else oracles_core_audio_cleared(core);
    oracles_core_run_frame(core);
    const size_t count = oracles_core_take_audio(core, samples, sizeof samples / sizeof *samples) / 2;
    CHECK(count > pending);
    const int16_t from[2] = { pending ? samples[pending * 2 - 2] : before[0],
                              pending ? samples[pending * 2 - 1] : before[1] };
    int drift = 0, fade_peak = 0;
    for (size_t i = pending * 2; i < count * 2; i++) {
        const int d = abs(samples[i] - from[i % 2]);
        if (d > drift) drift = d;
        if (abs(samples[i]) > fade_peak) fade_peak = abs(samples[i]);
    }
    for (unsigned i = 1; i < ORACLES_CORE_FADE_IN_FRAMES; i++) {
        const int peak = run_peak(core, 1, NULL);
        if (peak > fade_peak) fade_peak = peak;
    }
    const int after_peak = run_peak(core, 4, NULL);
    printf("%s: %s, first frame within %d of the last sample, fade %d, after %d\n", oracles_core_version(core), what, drift,
           fade_peak, after_peak);
    CHECK(drift <= tone_peak / 10);
    CHECK(fade_peak <= tone_peak);
    CHECK(after_peak > tone_peak * 9 / 10 && after_peak <= tone_peak * 11 / 10);
}

/* The host's run of a new core, its chunks heard, the backend asking for `asked` at its poll PAUSE_AT, inside the tone. */
enum { PAUSE_AT = 400, HOST_FRAMES = 420, FRAME_SAMPLES = RATE * 2 / 59 };
static int16_t heard[HOST_FRAMES * FRAME_SAMPLES + RATE];
static size_t heard_count, heard_at_pause;
static unsigned host_polls, cuts, cuts_at_pause, host_pauses;
static oracles_host_event_type asked;

static int pausing_poll(void *opaque, oracles_host_event *event, int *has_event)
{
    (void)opaque;
    *has_event = 0;
    if (++host_polls != PAUSE_AT) return 1;
    memset(event, 0, sizeof *event);
    event->type = asked;
    *has_event = 1;
    heard_at_pause = heard_count;
    return 1;
}

static int hearing_queue(void *opaque, const oracles_host_audio_chunk *chunk)
{
    (void)opaque;
    if (heard_count + chunk->sample_count > sizeof heard / sizeof *heard) return 0;
    memcpy(heard + heard_count, chunk->samples, chunk->sample_count * sizeof *heard);
    heard_count += chunk->sample_count;
    return 1;
}

static void cut(void *opaque) { (void)opaque; cuts++; }
static oracles_host_pause_result resume(void *opaque) { (void)opaque; cuts_at_pause = cuts; host_pauses++; return ORACLES_HOST_PAUSE_RESUME; }

/* The loudest of the samples heard from `from` on, `count` of them. */
static int heard_peak(size_t from, size_t count)
{
    int peak = 0;
    for (size_t i = from; i < from + count && i < heard_count; i++)
        if (abs(heard[i]) > peak) peak = abs(heard[i]);
    return peak;
}

static void check_host(const char *what, OraclesCoreKind kind, const uint8_t *rom, size_t size, int tone_peak, int menu, int cuts_sound)
{
    const OraclesCoreOptions options = { RATE, 0, kind, 0 };
    OraclesCore *core = oracles_core_create(rom, size, &options);
    oracles_host_backend backend;
    CHECK(core != NULL && oracles_null_backend_init(&backend));
    if (!core) return;
    backend.poll_event = pausing_poll;
    backend.queue_audio = hearing_queue;
    backend.cut_audio = cuts_sound ? cut : NULL;
    oracles_host_run_config config;
    memset(&config, 0, sizeof config);
    config.core = core;
    config.sample_rate_hz = RATE;
    config.quit_after_frames = HOST_FRAMES;
    config.on_pause = menu ? resume : NULL;
    heard_count = heard_at_pause = 0;
    host_polls = cuts = cuts_at_pause = host_pauses = 0;
    oracles_host_run_report report;
    char error[128];
    CHECK(oracles_host_run(&config, &backend, &report, error, sizeof error) == ORACLES_HOST_OK && report.frames_presented == HOST_FRAMES);
    const int before = heard_peak(heard_at_pause - FRAME_SAMPLES / 2, FRAME_SAMPLES / 2);
    const int first = heard_peak(heard_at_pause, FRAME_SAMPLES * 9 / 10);
    const int after = heard_peak(heard_at_pause + FRAME_SAMPLES * ORACLES_CORE_FADE_IN_FRAMES, FRAME_SAMPLES * 4);
    printf("%s: %s, before %d, first frame %d, after the fade %d\n", oracles_core_version(core), what, before, first, after);
    CHECK(heard_at_pause > FRAME_SAMPLES * 300 && heard_count > heard_at_pause + FRAME_SAMPLES * 10);
    CHECK(before > tone_peak * 9 / 10);
    CHECK(after > tone_peak * 9 / 10);
    CHECK(host_pauses == (menu ? 1u : 0u));
    CHECK(cuts == (cuts_sound ? 1u : 0u) && cuts_at_pause == (menu ? cuts : 0u));
    if (cuts_sound) CHECK(first <= tone_peak / 10);
    else CHECK(first > tone_peak * 9 / 10);
    oracles_null_backend_release(&backend);
    oracles_core_destroy(core);
}

static void check_core(OraclesCoreKind kind, const uint8_t *rom, size_t size)
{
    const OraclesCoreOptions options = { RATE, 0, kind, 0 };
    OraclesCore *core = oracles_core_create(rom, size, &options);
    CHECK(core != NULL);
    if (!core) return;
    int quiet_peak = 0, sound_peak = 0;
    unsigned frame = 0;
    size_t one = 0, two = 0;
    for (; frame < ORACLES_CORE_POWER_ON_QUIET_FRAMES - 2; frame++) {
        const int peak = run_peak(core, 1, &one);
        CHECK(one > 0);
        if (peak > quiet_peak) quiet_peak = peak;
    }
    /* The last two frames queued before they are taken. */
    const int queued_peak = run_peak(core, 2, &two);
    frame += 2;
    CHECK(two > one);
    for (; frame < BOOT_SOUND_BY; frame++) {
        const int peak = run_peak(core, 1, NULL);
        if (peak > sound_peak) sound_peak = peak;
    }
    for (; frame < TONE_FROM; frame++) run_peak(core, 1, NULL);
    const int tone_peak = run_peak(core, 4, NULL);
    printf("%s: power on %d, boot sound %d, tone %d\n", oracles_core_version(core), quiet_peak, sound_peak, tone_peak);
    CHECK(quiet_peak == 0);
    CHECK(queued_peak == 0);
    CHECK(sound_peak > 1000);
    CHECK(tone_peak > 1000);

    const size_t state_size = oracles_core_state_size(core);
    uint8_t *state = malloc(state_size);
    const int saved = state ? oracles_core_save_state(core, state, state_size) : -1;
    CHECK(saved == 0);
    if (saved == 0) {
        static const int16_t silence[2] = { 0, 0 };
        OraclesCore *fresh = oracles_core_create(rom, size, &options);
        CHECK(fresh != NULL);
        if (fresh) {
            check_fade("loaded into a new core", fresh, state, state_size, tone_peak, silence);
            oracles_core_destroy(fresh);
        }
        fresh = oracles_core_create(rom, size, &options);
        CHECK(fresh != NULL);
        if (fresh) {
            run_peak(fresh, 5, NULL);
            check_fade("loaded in the power-on silence", fresh, state, state_size, tone_peak, silence);
            oracles_core_destroy(fresh);
        }
        run_peak(core, 60, NULL);
        oracles_core_run_frame(core);
        check_fade("loaded into the running core, a frame queued", core, state, state_size, tone_peak, silence);
        CHECK(oracles_core_load_state(core, state, state_size) == 0);
        size_t taken;
        run_peak(core, 1, &taken);
        CHECK(taken > 0);
        if (taken) {
            const int16_t last[2] = { samples[taken * 2 - 2], samples[taken * 2 - 1] };
            check_fade("loaded again in the fade", core, state, state_size, tone_peak, last);
        }
        oracles_core_run_frame(core);
        check_fade("after the pause, a frame queued", core, NULL, 0, tone_peak, silence);
        oracles_core_audio_cleared(core);
        check_fade("loaded in the pause", core, state, state_size, tone_peak, silence);
    }
    free(state);
    oracles_core_destroy(core);
    asked = ORACLES_HOST_EVENT_PAUSE;
    check_host("the host's pause", kind, rom, size, tone_peak, 1, 1);
    check_host("the host's pause, the sound not cut", kind, rom, size, tone_peak, 1, 0);
    asked = ORACLES_HOST_EVENT_SUSPEND;
    check_host("a suspension without a pause menu", kind, rom, size, tone_peak, 0, 1);
    check_host("a suspension, the pause menu opened", kind, rom, size, tone_peak, 1, 1);
}

int main(void)
{
    const size_t size = 0x8000u;
    uint8_t *rom = calloc(size, 1);
    if (!rom) return 1;
    static const uint8_t entry[] = { 0xc3, 0x50, 0x01 };   /* jp $0150 */
    /* NR52 on, NR50 and NR51 channel 1 at full volume on both sides, NR11 half duty, NR12 volume 15 held, NR13/NR14
     * 512 Hz triggered without length; jr @. */
    static const uint8_t tone[] = {
        0x3e, 0x80, 0xe0, 0x26, 0x3e, 0x77, 0xe0, 0x24, 0x3e, 0x11, 0xe0, 0x25, 0x3e, 0x80, 0xe0, 0x11,
        0x3e, 0xf0, 0xe0, 0x12, 0x3e, 0x00, 0xe0, 0x13, 0x3e, 0x87, 0xe0, 0x14, 0x18, 0xfe,
    };
    memcpy(rom + 0x100, entry, sizeof entry);
    memcpy(rom + 0x150, tone, sizeof tone);
    rom[0x143] = 0xc0;
    rom[0x147] = 0x1b; rom[0x149] = 0x02;   /* MBC5 with cartridge RAM, which mGBA's states need */
    check_core(ORACLES_CORE_SAMEBOY, rom, size);
    check_core(ORACLES_CORE_MGBA, rom, size);
    free(rom);
    if (failures) return 1;
    puts("test_core_power_on: ok");
    return 0;
}
