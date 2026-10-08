/* The refusal of a savestate whose continuous transitions differ from the session's, without a ROM: in Faithful, where
 * Display greys the transitions, it points to the profile; in Enhanced, to the transitions; a state that matches loads.
 * And the refusal of a state taken on the other core, by its name. */
#include "state_refusal.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); failures++; } } while (0)

int main(void)
{
    char message[128], detail[320];

    /* Taken in Enhanced with the transitions, loaded in Faithful: the profile, not the greyed transitions. */
    CHECK(oracles_state_transitions_refusal(1, 0, 0, message, sizeof message, detail, sizeof detail) == 1);
    CHECK(!strcmp(message, "Refused: the savestate was taken in Enhanced, with continuous transitions"));
    CHECK(strstr(detail, "this session is Faithful") && strstr(detail, "the Enhanced profile in the launcher's Display"));
    CHECK(!strstr(detail, "Continuous transitions in"));
    /* Faithful with the transitions asked for (a command line's leftover): still Faithful's refusal. */
    CHECK(oracles_state_transitions_refusal(1, 0, 1, message, sizeof message, detail, sizeof detail) == 1);
    CHECK(strstr(message, "in Enhanced") != NULL);

    /* In Enhanced, the transitions are what differs. */
    CHECK(oracles_state_transitions_refusal(1, 1, 0, message, sizeof message, detail, sizeof detail) == 1);
    CHECK(!strcmp(message, "Refused: the savestate was taken with continuous transitions on"));
    CHECK(strstr(detail, "Continuous transitions in the launcher's Display") && strstr(detail, "loads only with them on"));
    CHECK(oracles_state_transitions_refusal(0, 1, 1, message, sizeof message, detail, sizeof detail) == 1);
    CHECK(!strcmp(message, "Refused: the savestate was taken with continuous transitions off"));

    /* The same transitions: it loads, and says nothing. */
    CHECK(oracles_state_transitions_refusal(1, 1, 1, message, sizeof message, detail, sizeof detail) == 0 && !message[0] && !detail[0]);
    CHECK(oracles_state_transitions_refusal(0, 1, 0, message, sizeof message, detail, sizeof detail) == 0);
    CHECK(oracles_state_transitions_refusal(0, 0, 0, message, sizeof message, detail, sizeof detail) == 0);

    /* Taken on the other core: refused by the core's name, the core to choose said; the same core, another version
     * included, passes (the load refuses another version itself). */
    CHECK(oracles_state_core_refusal("mgba-c3c8e5e8", "sameboy-1.0.3", message, sizeof message, detail, sizeof detail) == 1);
    CHECK(!strcmp(message, "Refused: taken on Fast (mGBA)"));
    CHECK(strstr(detail, "choose Fast (mGBA) in Display's Core, or --core mgba") != NULL);
    CHECK(oracles_state_core_refusal("sameboy-1.0.3", "mgba-c3c8e5e8", message, sizeof message, detail, sizeof detail) == 1);
    CHECK(!strcmp(message, "Refused: taken on Accurate (SameBoy)") && strstr(detail, "--core sameboy"));
    CHECK(oracles_state_core_refusal("sameboy-1.0.2", "sameboy-1.0.3", message, sizeof message, detail, sizeof detail) == 0 && !message[0] && !detail[0]);
    CHECK(oracles_state_core_refusal("mgba-c3c8e5e8", "mgba-c3c8e5e8", message, sizeof message, detail, sizeof detail) == 0);

    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("state refusal: Faithful points to the profile, Enhanced to the transitions, another core to Display's Core\n");
    return 0;
}
