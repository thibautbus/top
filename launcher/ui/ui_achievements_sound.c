/* The sound of an achievement unlocked (ui_achievements_sound.h). */
#include "ui_achievements_sound.h"

#include <math.h>

#define SOUND_PI 3.14159265358979323846

static const OraclesSoundNote unlock_notes[] = {
    { 1174.66f, 0.000f, 0.130f, 0.80f },   /* D6 */
    { 1760.00f, 0.085f, 0.430f, 1.00f },   /* A6 */
};
const OraclesSound oracles_unlock_sound = { unlock_notes, 2u, 0.004f, 0.20f, 0.030f, -12.0f };

static double soft_square(double phase, double freq_hz, double limit_hz)
{
    static const double weight[4] = { 1.0, 0.28, 0.12, 0.05 };
    double sum = 0.0;
    for (int i = 0; i < 4 && (2 * i + 1) * freq_hz < limit_hz; i++) sum += weight[i] * sin((2 * i + 1) * phase);
    return sum;
}

static double envelope(const OraclesSound *sound, double t, double length_s)
{
    if (t <= 0.0 || t >= length_s) return 0.0;
    double env = exp(-t / sound->decay_s);
    if (t < sound->attack_s) env *= 0.5 - 0.5 * cos(SOUND_PI * t / sound->attack_s);
    const double left = length_s - t;
    if (left < sound->fade_s) env *= 0.5 - 0.5 * cos(SOUND_PI * left / sound->fade_s);
    return env;
}

size_t oracles_sound_frames(const OraclesSound *sound, uint32_t rate_hz)
{
    double end_s = 0.0;
    for (unsigned i = 0; i < sound->note_count; i++) {
        const double end = (double)sound->notes[i].start_s + sound->notes[i].length_s;
        if (end > end_s) end_s = end;
    }
    return (size_t)ceil(end_s * rate_hz) + 1u;
}

size_t oracles_sound_render(const OraclesSound *sound, uint32_t rate_hz, float *out, size_t capacity)
{
    const size_t frames = oracles_sound_frames(sound, rate_hz);
    if (!rate_hz || !out || capacity < frames) return 0;
    const double limit_hz = 0.45 * rate_hz;
    double peak = 0.0;
    for (size_t n = 0; n < frames; n++) {
        const double t = (double)n / rate_hz;
        double sum = 0.0;
        for (unsigned i = 0; i < sound->note_count; i++) {
            const OraclesSoundNote *note = &sound->notes[i];
            const double note_t = t - note->start_s, env = envelope(sound, note_t, note->length_s);
            if (env == 0.0) continue;
            sum += note->gain * env * soft_square(2.0 * SOUND_PI * note->freq_hz * note_t, note->freq_hz, limit_hz);
        }
        out[n] = (float)sum;
        if (fabs(sum) > peak) peak = fabs(sum);
    }
    if (peak > 0.0) {
        const float scale = (float)(pow(10.0, sound->peak_dbfs / 20.0) / peak);
        for (size_t n = 0; n < frames; n++) out[n] *= scale;
    }
    return frames;
}

size_t oracles_sound_cursor_cleared(size_t cursor, size_t sound_frames, size_t cleared)
{
    if (cursor >= sound_frames) return cursor;
    return cursor <= cleared ? 0u : sound_frames;
}

size_t oracles_sound_mix_s16(const float *sound, size_t sound_frames, size_t *cursor, int16_t *interleaved, size_t frames, unsigned channels)
{
    size_t mixed = 0;
    while (mixed < frames && *cursor < sound_frames) {
        const long add = lrintf(sound[*cursor] * 32767.0f);
        for (unsigned c = 0; c < channels; c++) {
            const long s = interleaved[mixed * channels + c] + add;
            interleaved[mixed * channels + c] = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
        }
        mixed++;
        (*cursor)++;
    }
    return mixed;
}
