/* The check of the neighbours' animation reader: the
 * reader, started from the live game's animation state, must predict what the
 * game itself writes into the live VRAM, frame by frame, for the tiles its
 * copies touch.  Judged in windows of normal play of up to 240 frames, one
 * started at each room the live game settles in. */
#ifndef ORACLES_HARNESS_ANIMATION_CHECK_H
#define ORACLES_HARNESS_ANIMATION_CHECK_H

#include "guest.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct OraclesAnimationCheck OraclesAnimationCheck;

OraclesAnimationCheck *oracles_animation_check_start(OraclesGuest *guest, const uint8_t *rom, size_t rom_size);
void oracles_animation_check_stop(OraclesAnimationCheck *c);
/* After every frame of the live instance. */
void oracles_animation_check_frame(OraclesAnimationCheck *c, uint32_t frame);
/* The live state jumped (a savestate loaded back): the next frame starts afresh. */
void oracles_animation_check_reset(OraclesAnimationCheck *c);
void oracles_animation_check_summary(const OraclesAnimationCheck *c, FILE *out);
void oracles_animation_check_report(const OraclesAnimationCheck *c, FILE *out);

#endif
