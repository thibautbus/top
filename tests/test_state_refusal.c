/* The refusal of a savestate whose continuous transitions differ from the session's, without a ROM: in Faithful, where
 * Display greys the transitions, it points to the profile; in Enhanced, to the transitions; a state that matches loads.
 * And the refusal of a state taken on the other core, by its name, and with another text, classic or compact, in the
 * game's box or the wide one. */
#include "state_refusal.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); failures++; } } while (0)

int main(void)
{
    char message[256], detail[320];

    /* Taken in Enhanced with the transitions, loaded in Faithful: the profile, not the greyed transitions. */
    CHECK(oracles_state_transitions_refusal(1, 0, 0, message, sizeof message, detail, sizeof detail) == 1);
    CHECK(!strcmp(message, "Refused: taken in Enhanced: choose Enhanced in Display's Profile from the home screen"));
    CHECK(strstr(detail, "this session is Faithful") && strstr(detail, "the Enhanced profile in the launcher's Display"));
    CHECK(!strstr(detail, "Continuous transitions in"));
    /* Faithful with the transitions asked for (a command line's leftover): still Faithful's refusal. */
    CHECK(oracles_state_transitions_refusal(1, 0, 1, message, sizeof message, detail, sizeof detail) == 1);
    CHECK(strstr(message, "in Enhanced") != NULL);

    /* In Enhanced, the transitions are what differs. */
    CHECK(oracles_state_transitions_refusal(1, 1, 0, message, sizeof message, detail, sizeof detail) == 1);
    CHECK(!strcmp(message, "Refused: taken with transitions on: choose On in Display \xe2\x80\xba Advanced \xe2\x80\xba Continuous transitions from the home screen"));
    CHECK(strstr(detail, "Continuous transitions in the launcher's Display") && strstr(detail, "loads only with them on"));
    CHECK(oracles_state_transitions_refusal(0, 1, 1, message, sizeof message, detail, sizeof detail) == 1);
    CHECK(!strcmp(message, "Refused: taken with transitions off: choose Off in Display \xe2\x80\xba Advanced \xe2\x80\xba Continuous transitions from the home screen"));

    /* The same transitions: it loads, and says nothing. */
    CHECK(oracles_state_transitions_refusal(1, 1, 1, message, sizeof message, detail, sizeof detail) == 0 && !message[0] && !detail[0]);
    CHECK(oracles_state_transitions_refusal(0, 1, 0, message, sizeof message, detail, sizeof detail) == 0);
    CHECK(oracles_state_transitions_refusal(0, 0, 0, message, sizeof message, detail, sizeof detail) == 0);

    /* Taken on the other core: refused by the core's name, the core to choose said; the same core, another version
     * included, passes (the load refuses another version itself). */
    CHECK(oracles_state_core_refusal("mgba-c3c8e5e8", "sameboy-1.0.3", message, sizeof message, detail, sizeof detail) == 1);
    CHECK(!strcmp(message, "Refused: taken on Fast (mGBA): choose Fast in Display \xe2\x80\xba Advanced \xe2\x80\xba Core from the home screen"));
    CHECK(strstr(detail, "choose Fast (mGBA) in Display's Core, or --core mgba") != NULL);
    CHECK(oracles_state_core_refusal("sameboy-1.0.3", "mgba-c3c8e5e8", message, sizeof message, detail, sizeof detail) == 1);
    CHECK(!strcmp(message, "Refused: taken on Accurate (SameBoy): choose Accurate in Display \xe2\x80\xba Advanced \xe2\x80\xba Core from the home screen")
          && strstr(detail, "--core sameboy"));
    CHECK(oracles_state_core_refusal("sameboy-1.0.2", "sameboy-1.0.3", message, sizeof message, detail, sizeof detail) == 0 && !message[0] && !detail[0]);
    CHECK(oracles_state_core_refusal("mgba-c3c8e5e8", "mgba-c3c8e5e8", message, sizeof message, detail, sizeof detail) == 0);

    /* Taken with the compact text and loaded with the classic one, or the other way round: the text it was taken with
     * and where to choose it, not the digest; another layout of the compact text said as such; the same text loads. */
    char text_detail[400];
    const char *compact = "continuous-transitions,text-compact@0123456789abcdef0123456789abcdef01234567";
    CHECK(oracles_state_text_refusal(compact, "continuous-transitions", 1, 0, 0, message, sizeof message, text_detail, sizeof text_detail) == 1);
    CHECK(!strcmp(message, "Refused: taken with the Compact text: choose Compact in Display's Text from the home screen"));
    CHECK(strstr(text_detail, "--text compact") && strstr(text_detail, "text=compact") && !strstr(message, "@"));
    CHECK(oracles_state_text_refusal("", "language-fr@ffff,text-compact@0123456789abcdef0123456789abcdef01234567", 1, 1, 0, message, sizeof message,
                                     text_detail, sizeof text_detail) == 1);
    CHECK(!strcmp(message, "Refused: taken with the Classic text: choose Classic in Display's Text from the home screen") && strstr(text_detail, "--text classic"));
    CHECK(oracles_state_text_refusal(compact, "text-compact@ffffffffffffffffffffffffffffffffffffffff", 1, 1, 0, message, sizeof message, text_detail,
                                     sizeof text_detail) == 1);
    CHECK(!strcmp(message, "Refused: taken with another version of the compact text"));
    CHECK(oracles_state_text_refusal(compact, "text-compact@0123456789abcdef0123456789abcdef01234567", 1, 1, 0, message, sizeof message, text_detail,
                                     sizeof text_detail) == 0 && !message[0] && !text_detail[0]);
    /* Loaded in Faithful, whose text is classic whatever Text says: the profile to choose, Text too when it is not Compact. */
    CHECK(oracles_state_text_refusal(compact, "", 0, 1, 0, message, sizeof message, text_detail, sizeof text_detail) == 1);
    CHECK(!strcmp(message, "Refused: taken in Enhanced with the Compact text: choose Enhanced in Display's Profile from the home screen")
          && strstr(text_detail, "--enhanced --text compact"));
    CHECK(oracles_state_text_refusal(compact, "", 0, 0, 0, message, sizeof message, text_detail, sizeof text_detail) == 1);
    CHECK(!strcmp(message, "Refused: taken in Enhanced with the Compact text: choose Enhanced and Compact in Display from the home screen"));
    CHECK(oracles_state_text_refusal("", "continuous-transitions", 1, 0, 0, message, sizeof message, text_detail, sizeof text_detail) == 0);
    CHECK(oracles_state_text_refusal("my-text-compact@x", "", 1, 0, 0, message, sizeof message, text_detail, sizeof text_detail) == 0);   /* a word of its own */
    CHECK(oracles_state_text_refusal("text-speed", "", 1, 0, 0, message, sizeof message, text_detail, sizeof text_detail) == 0);

    /* The four texts against each other in Enhanced, as the session names them (language_session.c): the same loads;
     * each other one names the settings that differ, Text, Text box or both, the one the session plays never. */
    static const char *const texts[4] = {
        "language-de@abcd",
        "language-de@abcd,text-compact@0123456789abcdef0123456789abcdef01234567",
        "language-de@abcd,text-classic-192x2@1111111111111111111111111111111111111111",
        "language-de@abcd,text-compact-192x2@2222222222222222222222222222222222222222",
    };
    static const char *const expected[4][4] = {
        { NULL, "Refused: taken with the Classic text: choose Classic in Display's Text from the home screen",
          "Refused: taken with the Standard box: choose Standard in Display's Text box from the home screen",
          "Refused: taken with the Classic text in the Standard box: choose Classic and Standard in Display's Text from the home screen" },
        { "Refused: taken with the Compact text: choose Compact in Display's Text from the home screen", NULL,
          "Refused: taken with the Compact text in the Standard box: choose Compact and Standard in Display's Text from the home screen",
          "Refused: taken with the Standard box: choose Standard in Display's Text box from the home screen" },
        { "Refused: taken with the Wide box: choose Wide in Display's Text box from the home screen",
          "Refused: taken with the Classic text in the Wide box: choose Classic and Wide in Display's Text from the home screen", NULL,
          "Refused: taken with the Classic text: choose Classic in Display's Text from the home screen" },
        { "Refused: taken with the Compact text in the Wide box: choose Compact and Wide in Display's Text from the home screen",
          "Refused: taken with the Wide box: choose Wide in Display's Text box from the home screen",
          "Refused: taken with the Compact text: choose Compact in Display's Text from the home screen", NULL },
    };
    for (unsigned taken = 0; taken < 4u; taken++)
        for (unsigned now = 0; now < 4u; now++) {
            char text_message[256];
            const int refused = oracles_state_text_refusal(texts[taken], texts[now], 1, now & 1u, now >> 1, text_message, sizeof text_message,
                                                           text_detail, sizeof text_detail);
            if (taken == now) { CHECK(refused == 0 && !text_message[0]); continue; }
            CHECK(refused == 1 && !strcmp(text_message, expected[taken][now]) && !strstr(text_message, "@") && strlen(text_detail) < 320u);
            /* The detail's options: those of the settings that differ, as the state's. */
            const int letters = (taken & 1u) != (now & 1u), box = (taken >> 1) != (now >> 1);
            CHECK(!letters || strstr(text_detail, (taken & 1u) ? "--text compact" : "--text classic"));
            CHECK(!box || strstr(text_detail, (taken >> 1) ? "--text-box wide" : "--text-box standard"));
            CHECK(letters || !strstr(text_detail, "--text "));
            CHECK(box || !strstr(text_detail, "--text-box"));
        }
    /* Another layout of the wide box's text, the same letters and box: said as such. */
    CHECK(oracles_state_text_refusal(texts[2], "text-classic-192x2@ffffffffffffffffffffffffffffffffffffffff", 1, 0, 1, message, sizeof message,
                                     text_detail, sizeof text_detail) == 1);
    CHECK(!strcmp(message, "Refused: taken with another version of the Wide box's text") && strstr(text_detail, "text-classic-192x2@1111"));

    /* In Faithful, the classic text in the game's box whatever is chosen: the profile, and the text and the box chosen
     * where they are not the state's; a state of the classic text in the game's box loads. */
    char faithful_message[256];
    CHECK(oracles_state_text_refusal(texts[0], "", 0, 1, 1, faithful_message, sizeof faithful_message, text_detail, sizeof text_detail) == 0);
    CHECK(oracles_state_text_refusal(texts[2], "", 0, 0, 1, faithful_message, sizeof faithful_message, text_detail, sizeof text_detail) == 1);
    CHECK(!strcmp(faithful_message, "Refused: taken in Enhanced with the Wide box: choose Enhanced in Display's Profile from the home screen")
          && strstr(text_detail, "--enhanced --text-box wide)"));
    CHECK(oracles_state_text_refusal(texts[2], "", 0, 1, 0, faithful_message, sizeof faithful_message, text_detail, sizeof text_detail) == 1);
    CHECK(!strcmp(faithful_message, "Refused: taken in Enhanced with the Classic text in the Wide box: choose Enhanced, Classic and Wide in Display "
                                    "from the home screen"));
    CHECK(oracles_state_text_refusal(texts[3], "", 0, 1, 0, faithful_message, sizeof faithful_message, text_detail, sizeof text_detail) == 1);
    CHECK(!strcmp(faithful_message, "Refused: taken in Enhanced with the Compact text in the Wide box: choose Enhanced and Wide in Display from the "
                                    "home screen") && strstr(text_detail, "--enhanced --text compact --text-box wide)") && strlen(text_detail) < 320u);
    CHECK(oracles_state_text_refusal(texts[1], "", 0, 1, 1, faithful_message, sizeof faithful_message, text_detail, sizeof text_detail) == 1);
    CHECK(!strcmp(faithful_message, "Refused: taken in Enhanced with the Compact text in the Standard box: choose Enhanced and Standard in Display "
                                    "from the home screen"));

    /* The text each was taken with, as a game started again for it reads it. */
    static const int taken_texts[4][3] = { { 0, 0, 0 }, { 1, 1, 0 }, { 1, 0, 1 }, { 1, 1, 1 } };
    for (unsigned i = 0; i < 4u; i++) {
        int compact = -1, wide = -1;
        CHECK(oracles_state_text_taken(texts[i], &compact, &wide) == taken_texts[i][0] && compact == taken_texts[i][1] && wide == taken_texts[i][2]);
    }

    /* A game started again with other choices of Display loads a state that differs only in them: the transitions and
     * the text's word aside, the same mods and language, and the word the game started again would lay its text with. */
    const char *word = "text-compact@0123456789abcdef0123456789abcdef01234567";
    CHECK(oracles_state_display_only("continuous-transitions,mod-a@1,language-de@abcd,text-compact@0123456789abcdef0123456789abcdef01234567",
                                     "mod-a@1,language-de@abcd", word));
    CHECK(oracles_state_display_only("mod-a@1", "continuous-transitions,mod-a@1,text-compact@ffff", ""));
    CHECK(oracles_state_display_only("", "continuous-transitions", ""));
    CHECK(!oracles_state_display_only("continuous-transitions,language-fr@abcd", "continuous-transitions", ""));   /* another language */
    CHECK(!oracles_state_display_only("mod-a@1", "mod-b@1", ""));                                                   /* other mods */
    CHECK(!oracles_state_display_only("mod-a@1", "mod-a@2", ""));                                                   /* another version of it */
    CHECK(!oracles_state_display_only("text-compact@ffffffffffffffffffffffffffffffffffffffff", "", word));          /* another layout's digest */
    CHECK(!oracles_state_display_only(texts[1], "language-de@abcd", ""));                                            /* the classic text where it was compact */

    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("state refusal: Faithful points to the profile, Enhanced to the transitions, another core to Display's Core, another text or box to Display's Text\n");
    return 0;
}
