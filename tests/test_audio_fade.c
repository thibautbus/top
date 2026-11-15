/* The SDL backend's fades without SDL (launcher/audio_fade.c): the fade out starts on the first frame the device has
 * not taken, the ring wrapped or not, falls with no step to silence, and holds the queue's last frame past its end;
 * nothing is faded when nothing is queued or when the queue is older than the frames kept; the fade in rises from
 * silence with no step across the chunks it spans and leaves the frames after it as they were. */
#include "audio_fade.h"

#include <stdio.h>
#include <stdlib.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

enum { TONE = 8000, FADE = 480 };

static OraclesAudioRecent recent;
static int16_t chunk[(ORACLES_AUDIO_RECENT_FRAMES + 5000) * 2];
static int16_t faded[FADE * 2];

/* A square wave of `frames` frames from frame `from` on, left and right opposite. */
static void square(int16_t *out, size_t from, size_t frames)
{
    for (size_t i = 0; i < frames; i++) {
        const int16_t v = ((from + i) / 50) % 2 ? TONE : -TONE;
        out[i * 2] = v;
        out[i * 2 + 1] = (int16_t)-v;
    }
}

/* Puts `total` frames of the wave in chunks of `size`, then fades the last `queued` out. */
static size_t put_and_fade(size_t total, size_t size, size_t queued)
{
    for (size_t at = 0; at < total; at += size) {
        const size_t frames = total - at < size ? total - at : size;
        square(chunk, at, frames);
        oracles_audio_recent_put(&recent, chunk, frames);
    }
    return oracles_audio_fade_out(&recent, queued, FADE, faded);
}

static void fade_out_from_head(size_t total, size_t size, size_t queued)
{
    recent = (OraclesAudioRecent){ { 0 }, 0, 0 };
    CHECK(put_and_fade(total, size, queued) == FADE);
    int16_t head[2];
    square(head, total - queued, 1);
    CHECK(faded[0] == head[0] && faded[1] == head[1]);
    for (size_t i = 1; i < FADE; i++) {
        int16_t source[2];
        square(source, total - queued + (i < queued ? i : queued - 1), 1);
        const double left = 1.0 - (double)i / FADE;
        CHECK(abs(faded[i * 2] - (int)(source[0] * left * left * left)) <= 1);
        CHECK(abs(faded[i * 2]) <= abs(source[0]) && abs(faded[i * 2 + 1]) <= abs(source[1]));
    }
    CHECK(abs(faded[FADE * 2 - 2]) < 2 && abs(faded[FADE * 2 - 1]) < 2);
}

int main(void)
{
    fade_out_from_head(3000, 800, 3000 - 1234);
    fade_out_from_head(3000, 800, 3000);
    fade_out_from_head(ORACLES_AUDIO_RECENT_FRAMES + 5000, 800, 4000);   /* the ring wrapped */
    fade_out_from_head(ORACLES_AUDIO_RECENT_FRAMES + 5000, ORACLES_AUDIO_RECENT_FRAMES + 5000, 4000);
    fade_out_from_head(3000, 800, 100);   /* fewer queued than the fade: the last held */
    fade_out_from_head(ORACLES_AUDIO_RECENT_FRAMES * 2, 1000, ORACLES_AUDIO_RECENT_FRAMES);

    recent = (OraclesAudioRecent){ { 0 }, 0, 0 };
    CHECK(put_and_fade(3000, 800, 0) == 0);
    CHECK(oracles_audio_fade_out(&recent, 3001, FADE, faded) == 0);
    recent = (OraclesAudioRecent){ { 0 }, 0, 0 };
    CHECK(put_and_fade(ORACLES_AUDIO_RECENT_FRAMES + 10, 700, ORACLES_AUDIO_RECENT_FRAMES + 1) == 0);

    size_t done = 0;
    square(chunk, 0, 1000);
    oracles_audio_fade_in(chunk, 300, &done, FADE);
    CHECK(done == 300 && chunk[0] == 0 && chunk[1] == 0);
    oracles_audio_fade_in(chunk + 600, 700, &done, FADE);
    CHECK(done == FADE);
    int16_t source[2000];
    square(source, 0, 1000);
    for (size_t i = 0; i < 1000; i++) {
        const double t = i < FADE ? (double)i / FADE : 1.0;
        CHECK(abs(chunk[i * 2] - (int)(source[i * 2] * t * t * t)) <= 1);
        CHECK(abs(chunk[i * 2 + 1]) <= abs(source[i * 2 + 1]));
        if (i >= FADE) CHECK(chunk[i * 2] == source[i * 2] && chunk[i * 2 + 1] == source[i * 2 + 1]);
    }
    oracles_audio_fade_in(chunk, 1000, &done, FADE);
    CHECK(done == FADE && chunk[FADE * 2] == source[FADE * 2]);

    if (failures) return 1;
    puts("test_audio_fade: ok");
    return 0;
}
