/* The objects' animation check, shared between its two files: the step judged
 * against the live game (object_animation_check.c), and the captures of the
 * rooms entered, their poses' tiles and the cost (object_animation_capture.c).
 * Nothing here is called from outside the harness. */
#ifndef ORACLES_HARNESS_OBJECT_ANIMATION_CHECK_INTERNAL_H
#define ORACLES_HARNESS_OBJECT_ANIMATION_CHECK_INTERNAL_H

#include "object_animation_check.h"

#include "guest_struct_offsets.h"
#include "object_animation.h"

#include <stdlib.h>
#include <string.h>

#define KINDS 3u
#define SLOTS 16u
#define SCROLL_MODE_NORMAL 0x01u
#define TRANSITION_STATE_IDLE 0x02u
#define INTERAC_ERA_OR_SEASON_INFO 0xe0u   /* constants/common/interactions.s: the blurb the view draws itself */
#define POSES ORACLES_OBJECT_ANIMATION_POSES
#define SEVERAL_CALLS_MAX 4u               /* interactionAnimate4Times: the most calls an object's code makes in a frame */
#define STEPS_TIMED 4096u                  /* calls of the step timed in a row, per captured object */
#define RENDERS_TIMED 8u                   /* renders timed in a row, per captured object */
#define AREA_FIRST_LINE 16u                /* the status bar's lines, above the game area */
#define FRAMES_PER_SECOND 59.7275
#define LINK_NEAR 0x28u                    /* npcFaceLinkAndAnimate's distance to Link (objectCheckLinkWithinDistance): its code turns an NPC there */

/* Why the game's state is not one call of the step from the last: its code did not animate the object, animated it
 * several times without reading an entry, read the next entry before its hold ran out (a counter it cut, or calls
 * enough), chose an animation, wrote a pose; or something else. */
enum { CAUSE_NOT_ANIMATED, CAUSE_SEVERAL_CALLS, CAUSE_POSE_ADVANCED, CAUSE_ANIMATION_SET, CAUSE_POSE_WRITTEN, CAUSE_OTHER, CAUSES };
/* An object counted by whether it kept its place while followed: a neighbour shows it where it was captured. */
enum { PLACE_FIXED, PLACE_MOVING, PLACES };
/* What a neighbour does with it, by the rule (oracles_object_animation_animated) on its capture: animated or kept in
 * its captured pose.  An object no capture paired (a room entered otherwise than by a scroll) is judged by the same
 * rule on the game itself, its first ORACLES_GHOST_PROBE_UPDATES updates (one more after a restart of its animation),
 * Link away; undecided otherwise. */
enum { RULE_ANIMATED, RULE_FROZEN, RULE_UNCAPTURED };

typedef struct tally {
    unsigned objects, multi_pose, changed_live, judged, step_equal, causes[CAUSES];
    unsigned predicted, matched, live, predicted_by[CAUSES], live_by[CAUSES];   /* pose changes */
    unsigned predicted_far, matched_far;   /* those predicted while Link had not yet come near the object, as in a neighbour */
} tally;

typedef struct track {
    int followed;                 /* its state at the last update is known */
    int active;                   /* an object is followed in the slot */
    uint8_t number, id, created_id, created_subid, created_var03, yh, xh;
    int moved;
    int rule;                     /* RULE_*: the animated ones' figures are the step's against the game, the others apart */
    int link_near;                /* Link has come within LINK_NEAR of it since it was followed: its code may answer him */
    unsigned first_judged, first_one_step;   /* its first ORACLES_GHOST_PROBE_UPDATES updates judged, and those that were one call of the step */
    int first_restarted;          /* the first set again the animation it plays: one update more judged */
    int first_near;               /* Link was near it by then */
    OraclesObjectAnimation last;
    unsigned captured;            /* its capture's index + 1, 0 none */
    tally t;                      /* added to the totals of its place when it ends */
} track;

typedef struct captured_object {
    OraclesObjectRecord record;
    uint16_t poses[POSES];
    unsigned pose_count, period, changes;
    uint8_t pose_verdict[POSES];  /* 0 not shown live, 1 its tiles equal, 2 different every time it was shown */
    int paired, entry_judged;
} captured_object;

struct OraclesObjectAnimationCheck {
    OraclesGuest *guest;
    uint8_t *rom;
    size_t rom_size;
    FILE *tsv, *misses;
    track tracks[KINDS][SLOTS];
    uint8_t group, room;
    unsigned updates_in_room;
    /* the capture of the room played */
    int have_capture;
    uint8_t capture_group, capture_room;
    uint32_t capture_frame;
    captured_object captured[ORACLES_OBJECT_RECORDS];
    unsigned captured_count;
    uint8_t capture_tiles[2u * ORACLES_GHOST_TILE_BYTES];
    /* the step against the game */
    unsigned updates_judged, updates_skipped, read_failures;
    tally totals[KINDS][PLACES];  /* the objects a neighbour animates */
    tally frozen;                 /* those it keeps in their captured pose, their code not advancing their animation at its probe */
    tally uncaptured;             /* those no capture paired, undecided */
    unsigned decided_live;        /* those no capture paired, the rule judged on the game */
    /* The probe of the captures (ghost.h) against the game's first updates after the scroll, for the captured objects
     * of several poses Link is not near: the same verdict, or animated by the probe and not by the game, or the other way. */
    unsigned probe_agree, probe_animated_live_not, probe_frozen_live_animated;
    char probe_frozen_list[400];  /* the captured objects of several poses the probe keeps still: room kind.id:subid */
    uint16_t by_id[KINDS][CAUSES][256];   /* the pose changes missed, by cause and by the object's id */
    /* the captures */
    unsigned captures, captured_shown[KINDS], captured_multi_pose[KINDS], captured_animated[KINDS], captured_unpaired, captured_unreadable;
    unsigned poses_checked, poses_equal, poses_different, poses_unseen;
    unsigned entry_equal, entry_different;
    char tiles_wrong_list[200];
    double rate_max;
    uint8_t rate_max_group, rate_max_room;
    /* the cost */
    double step_us, redraw_us, redraw_lines, full_us;
    unsigned steps_timed, redraws_timed, fulls_timed;
};

int oac_same_state(const OraclesObjectAnimation *a, const OraclesObjectAnimation *b);
/* What a neighbour shows of a record: a numbered object, not placed at random, not the era's blurb, drawn. */
int oac_shown(const OraclesGuestTables *t, const OraclesObjectRecord *r);
/* The followed objects' figures added to the totals (object_animation_check.c). */
void oac_end_tracks(OraclesObjectAnimationCheck *c);
/* The room's capture is over (the room left, or the run): its poses' verdicts counted (object_animation_capture.c). */
void oac_finish_capture(OraclesObjectAnimationCheck *c);
/* The capture's object paired with a live one, its index + 1, 0 none. */
unsigned oac_find_captured(const OraclesObjectAnimationCheck *c, const OraclesObjectRecord *r);
/* The tiles of the pose the live object shows, against the capture's, and its first state against the capture's. */
void oac_against_capture(OraclesObjectAnimationCheck *c, track *tr, const OraclesObjectRecord *r, const OraclesObjectAnimation *now);

#endif
