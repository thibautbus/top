/* The sound of an achievement unlocked, in a build with RetroAchievements (ui_achievements.h): a few notes synthesized
 * at the output's rate, without a sound file, rendered once and mixed on top of the game's sound after the emulation,
 * the core never hearing of it.  Without SDL, as the screens. */
#ifndef ORACLES_UI_ACHIEVEMENTS_SOUND_H
#define ORACLES_UI_ACHIEVEMENTS_SOUND_H

#include <stddef.h>
#include <stdint.h>

typedef struct OraclesSoundNote {
    float freq_hz;
    float start_s;
    float length_s;   /* silent again at start_s + length_s */
    float gain;       /* against the other notes, before the peak is set */
} OraclesSoundNote;

/* Notes in a soft square wave (odd harmonics 1, 3, 5 and 7, weaker than a square's, none above 0.45 times the rate),
 * each shaped by a raised-cosine rise, an exponential decay and a raised-cosine fall to exactly 0 at its end. */
typedef struct OraclesSound {
    const OraclesSoundNote *notes;
    unsigned note_count;
    float attack_s;    /* the rise */
    float decay_s;     /* the decay's time constant */
    float fade_s;      /* the fall at each note's end */
    float peak_dbfs;   /* the rendered sound's peak */
} OraclesSound;

/* The unlock's: D6 for 130 ms, then A6 from 85 ms for 430 ms, a rising fifth, about half a second, its peak at -12 dBFS. */
extern const OraclesSound oracles_unlock_sound;

/* The frames the sound lasts at `rate_hz`, its last one silent. */
size_t oracles_sound_frames(const OraclesSound *sound, uint32_t rate_hz);
/* The sound rendered mono into `out` (`capacity` frames), scaled so that its peak sits at its peak_dbfs; returns the
 * frames written, 0 when `out` is too small. */
size_t oracles_sound_render(const OraclesSound *sound, uint32_t rate_hz, float *out, size_t capacity);
/* The rendered sound (`sound_frames` frames) from *cursor on, added to `frames` interleaved frames of `channels`
 * channels, the same in each, saturating; *cursor moves on.  Returns the frames mixed: fewer than `frames` once the
 * sound ends, 0 after it. */
size_t oracles_sound_mix_s16(const float *sound, size_t sound_frames, size_t *cursor, int16_t *interleaved, size_t frames, unsigned channels);
/* The cursor once the output's queue is emptied (the pause), `cleared` frames of it never heard: a sound still all in the
 * queue starts again from its beginning when the frames go on, one partly heard stops (its end alone would follow a gap);
 * no sound playing, the cursor as it is. */
size_t oracles_sound_cursor_cleared(size_t cursor, size_t sound_frames, size_t cleared);

#endif
