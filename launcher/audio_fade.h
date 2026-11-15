/* The SDL backend's own fades of the game's sound (sdl_audio.c), without SDL: the frames it queued last, kept so that
 * what the device is about to play can be faded out where it plays, and the fade back in after a gap. */
#ifndef ORACLES_AUDIO_FADE_H
#define ORACLES_AUDIO_FADE_H

#include <stddef.h>
#include <stdint.h>

/* Stereo frames kept: more than the queue holds unless the device stalls (its target is about 70 ms). */
#define ORACLES_AUDIO_RECENT_FRAMES 16384u

typedef struct OraclesAudioRecent {
    int16_t samples[ORACLES_AUDIO_RECENT_FRAMES * 2];   /* interleaved stereo, a ring */
    size_t end;    /* the frame after the last put */
    size_t kept;   /* the frames held, at most ORACLES_AUDIO_RECENT_FRAMES */
} OraclesAudioRecent;

/* `frames` stereo frames queued after the others. */
void oracles_audio_recent_put(OraclesAudioRecent *recent, const int16_t *samples, size_t frames);
/* The `queued` frames put last are the ones the device has not taken yet: `frames` frames written to `out` that start
 * on the first of them and fade to silence, as the time-reversed fade in of the core (a cube), the last of them held
 * past the queue's end.  Returns the frames written: `frames`, or 0 when nothing is queued or the frames queued are
 * older than those kept. */
size_t oracles_audio_fade_out(const OraclesAudioRecent *recent, size_t queued, size_t frames, int16_t *out);
/* The next `frames` stereo frames of a fade in from silence `length` frames long, *done of them already faded: their
 * gain rises as a cube, as the core's; *done moves on, the frames past the fade untouched. */
void oracles_audio_fade_in(int16_t *samples, size_t frames, size_t *done, size_t length);

#endif
