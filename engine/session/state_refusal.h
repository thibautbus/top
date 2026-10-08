/* Why a savestate does not load in this session, as the launcher and the command line say it: the continuous
 * transitions a state was taken with must be those of the session that loads it, and they come only with
 * Enhanced. */
#ifndef ORACLES_STATE_REFUSAL_H
#define ORACLES_STATE_REFUSAL_H

#include <stddef.h>

/* 1 when a state taken with (1) or without (0) the transitions is refused by a session in Enhanced or not, with the
 * transitions or not: *message the short reason the pause menu shows, *detail the whole one for stderr, with the
 * option and the launcher's choice that make it load.  0, both empty, when it loads. */
int oracles_state_transitions_refusal(int taken_with_transitions, int enhanced, int transitions,
                                      char *message, size_t message_capacity, char *detail, size_t detail_capacity);

/* 1 when a state taken on the core of version `taken_on` ("sameboy-1.0.3", "mgba-...") is refused by a session on the
 * core of version `now` because they are not the same core: *message names the core it was taken on ("Refused: taken
 * on Fast (mGBA)"), *detail the core to choose, in Display's Core or with --core.  0, both empty, for the same core
 * (another version of it is refused by the load itself). */
int oracles_state_core_refusal(const char *taken_on, const char *now, char *message, size_t message_capacity, char *detail, size_t detail_capacity);

#endif
