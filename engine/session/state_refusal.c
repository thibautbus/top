#include "state_refusal.h"

#include <stdio.h>

int oracles_state_transitions_refusal(int taken_with_transitions, int enhanced, int transitions,
                                      char *message, size_t message_capacity, char *detail, size_t detail_capacity)
{
    if (message_capacity) message[0] = 0;
    if (detail_capacity) detail[0] = 0;
    transitions = enhanced && transitions;
    if ((taken_with_transitions != 0) == (transitions != 0)) return 0;
    if (taken_with_transitions && !enhanced) {
        /* In Faithful the transitions are greyed: the profile is what to change. */
        snprintf(message, message_capacity, "Refused: the savestate was taken in Enhanced, with continuous transitions");
        snprintf(detail, detail_capacity, "the savestate was taken in Enhanced with continuous transitions, and this session is Faithful "
                 "(--continuous-transitions, or the Enhanced profile in the launcher's Display): it loads only there");
        return 1;
    }
    const char *taken = taken_with_transitions ? "on" : "off", *now = transitions ? "on" : "off";
    snprintf(message, message_capacity, "Refused: the savestate was taken with continuous transitions %s", taken);
    snprintf(detail, detail_capacity, "the savestate was taken with continuous transitions %s, and this session has them %s "
             "(--continuous-transitions, or Continuous transitions in the launcher's Display): it loads only with them %s", taken, now, taken);
    return 1;
}
