/* What a route covers, so that its composition can be judged before
 * it joins the suite.  The cases the objects
 * of the neighbouring rooms need are counted while the route replays, and
 * written as `coverage.*` lines of the harness summary: scrolling transitions
 * by direction, the state Link was in when he left a room (riding a companion,
 * carrying an object, an item of his in flight), a room entered again soon
 * after an enemy was killed in it and without a warp in between, and
 * turnarounds through the same edge.
 *
 * It observes only: no write reaches the guest. */
#ifndef ORACLES_HARNESS_COVERAGE_H
#define ORACLES_HARNESS_COVERAGE_H

#include "guest.h"

#include <stdio.h>

typedef struct OraclesCoverage OraclesCoverage;

OraclesCoverage *oracles_coverage_create(OraclesGuest *guest);
void oracles_coverage_destroy(OraclesCoverage *c);

/* Every guest event of the run, and the end of every frame. */
void oracles_coverage_event(OraclesCoverage *c, const OraclesGuestEvent *event);
void oracles_coverage_frame_end(OraclesCoverage *c, uint32_t frame);

void oracles_coverage_summary(const OraclesCoverage *c, FILE *out);

#endif
