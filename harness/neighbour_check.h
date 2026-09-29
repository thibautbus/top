/* Neighbour objects check.  Along a
 * replayed route, every scrolling transition of the live instance is preceded
 * by a ghost run towards the same neighbour, started from the live savestate
 * of the last primeable frame before the transition and settled like the view
 * settles its own runs; the objects the ghost's room holds then are compared
 * with the objects the live instance holds at the end of the real entry.
 *
 * It is the control of the measurement: with the same starting state and the
 * same direction, the two sides must agree on every object that does not
 * depend on the RNG.  Writes DIR/objects.tsv, one line per object compared,
 * and reports the totals by verdict and by cause. */
#ifndef ORACLES_NEIGHBOUR_CHECK_H
#define ORACLES_NEIGHBOUR_CHECK_H

#include "core.h"
#include "guest.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OraclesNeighbourCheck OraclesNeighbourCheck;

OraclesNeighbourCheck *oracles_neighbour_check_start(const uint8_t *rom, size_t rom_size, const OraclesCompatProfile *profile,
                                                     OraclesCore *live, OraclesGuest *live_guest, const char *dir, unsigned lead, int capture);
void oracles_neighbour_check_event(OraclesNeighbourCheck *check, const OraclesGuestEvent *event);
void oracles_neighbour_check_frame_end(OraclesNeighbourCheck *check, uint32_t frame);
void oracles_neighbour_check_report(OraclesNeighbourCheck *check, FILE *out);
/* The figures as key=value lines (objects.*), for tools/check_routes.py. */
void oracles_neighbour_check_summary(const OraclesNeighbourCheck *check, FILE *out);
void oracles_neighbour_check_stop(OraclesNeighbourCheck *check);

#ifdef __cplusplus
}
#endif

#endif
