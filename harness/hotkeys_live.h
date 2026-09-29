/* The live policy of the item hotkeys in the harness:
 * scripted key presses on top of a route's inputs, as a player would make
 * them, and the route that a recording of that session would be.  Replaying
 * that route must give the same game, frame for frame: the recording and the
 * replay are checked against each other on the real game. */
#ifndef ORACLES_HARNESS_HOTKEYS_LIVE_H
#define ORACLES_HARNESS_HOTKEYS_LIVE_H

#include "guest.h"
#include "route.h"

#include <stdio.h>

#define HOTKEYS_LIVE_MAX_PRESSES 256u

typedef struct OraclesHotkeysLiveOptions {
    const char *mode;                    /* use or equip; NULL: the live policy is not asked */
    const char *slots[4];                /* a slot's value as in settings.txt (`b:17:--`), or NULL */
    struct { uint32_t frame, frames; unsigned slot; } presses[HOTKEYS_LIVE_MAX_PRESSES];
    unsigned press_count;
    const char *record_path;             /* the route of the session, written in format 2 */
} OraclesHotkeysLiveOptions;

typedef struct OraclesHotkeysLive OraclesHotkeysLive;

/* The hotbar of a replay: the slots given to the harness, with no policy behind them (a route's exchanges are replayed,
 * its keys are not), so that the hotbar's icons are checked on recorded routes.  0 when a slot is malformed. */
struct OraclesEnhancedView;
int oracles_hotkeys_slots_hotbar(const OraclesHotkeysLiveOptions *options, struct OraclesEnhancedView *view);

/* `--hotkey-press FRAME:SLOT:FRAMES` into the options; 0 when malformed or too many. */
int oracles_hotkeys_live_parse_press(OraclesHotkeysLiveOptions *options, const char *text);
OraclesHotkeysLive *oracles_hotkeys_live_start(OraclesGuest *guest, const OraclesHotkeysLiveOptions *options, const OraclesRouteHeader *header);
/* The keys handed to the core for this frame: the route's, the scripted keys played, the policy's press added; recorded. */
unsigned oracles_hotkeys_live_keys(OraclesHotkeysLive *live, uint32_t frame, unsigned route_mask);
void oracles_hotkeys_live_frame_end(OraclesHotkeysLive *live, uint32_t frame);
/* Hands the view under the Enhanced check the hotbar's model, once a frame; `view` may be NULL. */
void oracles_hotkeys_live_hotbar(OraclesHotkeysLive *live, struct OraclesEnhancedView *view);
void oracles_hotkeys_live_summary(OraclesHotkeysLive *live, FILE *out);
/* 1 when the inventory ended inconsistent or the route could not be written. */
int oracles_hotkeys_live_stop(OraclesHotkeysLive *live, FILE *report);

#endif
