/* Why a savestate does not load in this session, as the launcher and the command line say it: the continuous
 * transitions a state was taken with must be those of the session that loads it, and they come only with
 * Enhanced; so must its core and its text, classic or compact, in the game's box or the wide one. */
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

/* 1 when a state whose set of mods and gameplay options is `taken_mods` is refused by a session whose set is `mods`
 * because of the text: its letters, compact or classic, or its box, the wide one or the game's, are not the session's
 * (*message names the settings that would load it, from the home screen, a running game keeping both: "Refused: taken
 * with the Compact text: choose Compact in Display's Text from the home screen", "Refused: taken with the Wide box:
 * choose Wide in Display's Text box from the home screen", or both; or the profile when the session is Faithful
 * (`enhanced` 0), which plays the classic text in the game's box whatever is chosen, and with it the text and the box
 * where those chosen (`compact_chosen`, `wide_chosen`) are not the state's), or both have them laid out by another
 * version.  0, both empty, when their texts agree. */
int oracles_state_text_refusal(const char *taken_mods, const char *mods, int enhanced, int compact_chosen, int wide_chosen, char *message,
                               size_t message_capacity, char *detail, size_t detail_capacity);

/* The text a set of mods and gameplay options lays out, as oracles_state_text_refusal reads it: 1 for the host's (the
 * compact text, the wide box, or both, *compact and *wide saying which), 0 for the classic text in the game's box. */
int oracles_state_text_taken(const char *mods, int *compact, int *wide);

/* 1 when a state whose set of mods and gameplay options is `taken_mods` differs from a session's `mods` only in what
 * Display chooses, so that a game started again with other choices of Display could load it: the same mods and the
 * same language, the continuous transitions aside, and its text's word (text-...@digest, "" for the classic text in the
 * game's box) `text_word`, that of the layout the game started again would play. */
int oracles_state_display_only(const char *taken_mods, const char *mods, const char *text_word);

#endif
