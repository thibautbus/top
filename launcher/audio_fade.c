#include "audio_fade.h"

void oracles_audio_recent_put(OraclesAudioRecent *recent, const int16_t *samples, size_t frames)
{
    if (frames > ORACLES_AUDIO_RECENT_FRAMES) {
        samples += (frames - ORACLES_AUDIO_RECENT_FRAMES) * 2;
        frames = ORACLES_AUDIO_RECENT_FRAMES;
    }
    for (size_t i = 0; i < frames; i++) {
        recent->samples[recent->end * 2] = samples[i * 2];
        recent->samples[recent->end * 2 + 1] = samples[i * 2 + 1];
        recent->end = (recent->end + 1) % ORACLES_AUDIO_RECENT_FRAMES;
    }
    recent->kept = recent->kept + frames < ORACLES_AUDIO_RECENT_FRAMES ? recent->kept + frames : ORACLES_AUDIO_RECENT_FRAMES;
}

size_t oracles_audio_fade_out(const OraclesAudioRecent *recent, size_t queued, size_t frames, int16_t *out)
{
    if (!queued || queued > recent->kept || !frames) return 0;
    const size_t head = (recent->end + ORACLES_AUDIO_RECENT_FRAMES - queued) % ORACLES_AUDIO_RECENT_FRAMES;
    for (size_t i = 0; i < frames; i++) {
        const size_t index = (head + (i < queued ? i : queued - 1)) % ORACLES_AUDIO_RECENT_FRAMES;
        const double left = 1.0 - (double)i / frames;
        const double gain = left * left * left;
        out[i * 2] = (int16_t)(recent->samples[index * 2] * gain);
        out[i * 2 + 1] = (int16_t)(recent->samples[index * 2 + 1] * gain);
    }
    return frames;
}

void oracles_audio_fade_in(int16_t *samples, size_t frames, size_t *done, size_t length)
{
    for (size_t i = 0; i < frames && *done < length; i++, (*done)++) {
        const double t = (double)*done / length;
        const double gain = t * t * t;
        samples[i * 2] = (int16_t)(samples[i * 2] * gain);
        samples[i * 2 + 1] = (int16_t)(samples[i * 2 + 1] * gain);
    }
}
