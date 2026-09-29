/* The fingerprint of a frame's live state, one line a frame: the screen, the live WRAM, the HRAM, the OAM and the VRAM.
 * The harness writes it for a replay (--out) and the launcher for a recorded session, in the same words, so that a
 * session and the replay of its route are compared frame for frame (oracles-harness --compare). */
#ifndef ORACLES_GUEST_FINGERPRINT_H
#define ORACLES_GUEST_FINGERPRINT_H

#include "guest.h"

#include <stdio.h>

void oracles_guest_write_fingerprint(OraclesGuest *guest, OraclesCore *core, uint32_t frame, FILE *out);

#endif
