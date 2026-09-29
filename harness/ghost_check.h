/* Ghost check: along a replayed route, every scrolling transition
 * of the live instance is preceded, from a snapshot taken `lead` primeable
 * frames earlier, by a ghost run towards the same neighbour; the ghost's
 * committed layout is compared tile for tile with the layout the live
 * instance commits after the real entry.  Writes DIR/transitions.tsv,
 * DIR/reads.tsv (addresses read by applyAllTileSubstitutions in the ghost)
 * and prints the report. */
#ifndef ORACLES_GHOST_CHECK_H
#define ORACLES_GHOST_CHECK_H

#include "ghost.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OraclesGhostCheck OraclesGhostCheck;

OraclesGhostCheck *oracles_ghost_check_start(const uint8_t *rom, size_t rom_size, const OraclesCompatProfile *profile,
                                             OraclesCore *live, OraclesGuest *live_guest,
                                             const char *dir, unsigned lead, int threaded, int trace);
void oracles_ghost_check_event(OraclesGhostCheck *check, const OraclesGuestEvent *event);
void oracles_ghost_check_frame_end(OraclesGhostCheck *check, uint32_t frame);
/* With the trace on: also count every read of each run's whole load, and report
 * the file region's addresses read outside the traced account. */
void oracles_ghost_check_trace_load(OraclesGhostCheck *check);
void oracles_ghost_check_report(OraclesGhostCheck *check, FILE *out);
/* The figures as key=value lines (ghost.*), for tools/check_routes.py. */
void oracles_ghost_check_summary(const OraclesGhostCheck *check, FILE *out);
/* DIR/reads.tsv: the addresses read by applyAllTileSubstitutions in the ghost, with counts. */
void oracles_ghost_check_write_reads(OraclesGhostCheck *check, const char *dir);
void oracles_ghost_check_stop(OraclesGhostCheck *check);

#ifdef __cplusplus
}
#endif

#endif
