/* The colour of an RGB555 value, as SameBoy gives it, for the cores that do
 * not: raw (each channel's 5 bits widened to 8 by repeating the high ones) or
 * with SameBoy's "modern balanced" correction for a CGB.  Equal to SameBoy's
 * on every one of the 32768 colours, in both modes (tests/test_colour.c), so
 * that the image is the same whatever the core. */
#ifndef ORACLES_COLOUR_H
#define ORACLES_COLOUR_H

#include <stdint.h>

#define ORACLES_COLOURS 32768u

/* 0xAARRGGBB, opaque. */
uint32_t oracles_colour_convert(uint16_t rgb555, int correction);
/* All of them at once: table[rgb555]; a few milliseconds with the correction (it takes powers). */
void oracles_colour_fill(uint32_t table[ORACLES_COLOURS], int correction);

#endif
