/* Continuous room transitions: an opt-in gameplay policy
 * that keeps Link walking through the game's own scrolling transition.
 *
 * The game freezes Link for the whole transition (linkState01 skips his
 * movement while wScrollMode is in transition) and moves him itself by
 * 15 px over about forty frames, one tile column every two 4 px steps; in
 * the wide world of the Enhanced profile that freeze is the only thing the
 * player sees of the transition.  The policy, at the return of
 * drawAllSprites on every frame of the transition (states 3 to 5), asks the
 * guest's closed transaction for three things: the scroll step doubled to
 * 8 px (a column per step, twenty steps), Link moved so that he advances
 * one pixel a frame in the direction of the transition, his walking
 * animation advanced from the ROM's animation data, as animateLinkWalking
 * would.  The extra distance is stopped short of any tile of the room being
 * entered that has a collision, so Link never arrives inside a wall.  The
 * core and the audio keep their cadence; the guest state of a session with
 * the option differs from a native one by design, and a route recorded
 * without it diverges after the first transition.  Both games, the small
 * rooms of groups 0 to 5 (the overworlds and the interiors), Link on foot;
 * the large rooms (dungeons) and the sidescrolling areas deliberately keep
 * their native transition (room_transition.c says why).  With `swim` (--continuous-swim) Link
 * swimming at the surface goes on too, in his normal swimming state, diving
 * or not: at his own speed (w1Link.speed, frozen by the game through the
 * transition), his swimming or diving animation advanced, and beyond the
 * game's own distance only into water (the ROM's tile types); a current's
 * push is not reproduced.  On Ages' sea floor, where the game has him walk in
 * his mermaid suit's gait, he goes on at his own speed too, his walking
 * animation advanced while a direction is held (standing without one, as the
 * game moves him there), onto tiles without a collision beyond the game's
 * distance, as on foot.  With it, the cap beyond the game's distance holds
 * leftward and upward too, on foot as swimming; without it, the distance
 * wraps there below zero and the cap never holds, as the routes recorded
 * with the transitions alone were played; and on foot, beyond the game's
 * distance, he walks onto stairs and onto a bridge along its way, which the
 * rule of no collision alone took for walls; with it or `diagonal`, the cap
 * counts the machine's own steps still to come, which nothing holds back, and
 * tests the game's own points: the tile it takes as his (y+5), his leading
 * edge, and none of the points it tests for walls newly in one.  With `diagonal`
 * (--continuous-diagonal), on foot, a diagonal held toward the edge goes on
 * as the game walks it, 0xb5 a frame along each axis: across the axis too,
 * onto tiles of the room entered without a collision, where the game holds
 * him for the whole scroll and the view would slide along the axis alone,
 * only where every point the game tests for walls, and the tile it takes as
 * his, stay as clear as the straight path's wherever the scroll may leave
 * him;
 * swimming, or on Ages' sea floor, his motion goes on at its own angle and
 * speed, which the game freezes through the scroll and takes up after it,
 * across the axis only into water swimming; while the room entered loads,
 * onto its collisions as predicted from the ROM's tables (room_predict.h),
 * and where they are not predicted the carry the load withholds is caught
 * up once they are known, at his own ratio across and along, within the
 * bus's bound a frame; a route
 * recorded without it replays without it, its diagonal crossings elsewhere
 * otherwise.  With `steps` (--continuous-steps) Link takes a step a frame of
 * the game, as the keys give it: on foot he walks along the axis only with a
 * direction held toward the edge (straight or diagonal), across it with one
 * held across (a diagonal, or square across with the diagonal on), turned to
 * face it as the game turns him (w1Link.direction), and never past the room's
 * edge on that side, which the next transition at a corner would put him back
 * from; with none along it, from the frame it is let go, the game's own move
 * of the transition, its 15 or 16 px from where it began spread evenly over
 * the frames left, never faster than he walked, without his walk animated;
 * the frame that starts the
 * transition, in which the game made his
 * step and put him back on the edge, completes that step instead of adding
 * one; and the frame whose logic ends the scroll, which the game leaves
 * without a move (linkState01 skipped, the next frame's the first it moves
 * him in), gets his step, at its drawAllSprites, by the same rules, his walk
 * left to the game from the next frame.  In the
 * water his motion is the game's, its momentum frozen through the scroll as
 * without the steps. */
#ifndef ORACLES_GAMEPLAY_ROOM_TRANSITION_H
#define ORACLES_GAMEPLAY_ROOM_TRANSITION_H

#include "guest.h"
#include "room_predict.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OraclesRoomTransition OraclesRoomTransition;

/* Installs the policy on the guest; `rom` must outlive it (the animation data, the tile types). NULL when the guest's
 * game has no tables, or `swim` is asked of a profile without it. */
OraclesRoomTransition *oracles_room_transition_start(OraclesGuest *guest, const uint8_t *rom, size_t rom_size, int swim);
void oracles_room_transition_stop(OraclesRoomTransition *transition);
/* --continuous-diagonal: a diagonal held carries Link across the transition's axis too (off at the start), where the
 * guest's profile qualifies it; returns whether it is on. */
int oracles_room_transition_set_diagonal(OraclesRoomTransition *transition, int on);
/* --continuous-steps: Link takes a step a frame of the game through a transition, as the keys give it (off at the
 * start), where the guest's profile qualifies it; returns whether it is on. */
int oracles_room_transition_set_steps(OraclesRoomTransition *transition, int on);
/* --continuous-line, with the steps: the scroll starts at the line between two rooms and Link stands anywhere in a room
 * (the guest's transitions at the line, armed here), and through a scroll he stops where the keys leave him, the
 * machine's own move undone, or walks back toward the line he crossed; a diagonal carries him on past the line across
 * into the room beside the room entered, where its collisions are predicted (oracles_room_predict_beside), the scroll
 * into it started at the frame the first one ends; in the water (with `swim`), at the surface or on Ages' sea floor,
 * he goes through the scroll as the game's swim routines move him in the middle of a room
 * (oracles_room_transition_swim_step), his swim's state written back, a glide that takes him across the line starting
 * the scroll; where the guest's profile qualifies it (off at the start, and with the steps); returns whether it is on. */
int oracles_room_transition_set_line(OraclesRoomTransition *transition, int on);
/* getNextActiveRoom: whether `room` of `group` has a transition of its own in mapTransitionGroupTable (the Lost Woods,
 * the scrambled forest, the eye puzzle, Onox's castle...), its scroll possibly leading elsewhere than the room beside
 * it; 1 when the table cannot be read. */
int oracles_room_transition_special_room(const uint8_t *rom, size_t rom_size, const OraclesGuestTransitionState *state, uint8_t group, uint8_t room);

typedef struct OraclesRoomTransitionStats {
    unsigned transitions;        /* transitions the policy took over (the step doubled) */
    unsigned swim_transitions;   /* of them, those Link swam through, at the surface or on the sea floor */
    unsigned walked_frames;      /* frames Link was moved by the policy */
    unsigned swum_frames;        /* of them, frames he swam */
    unsigned forced_transitions; /* transitions a swim's momentum took Link to at an edge, the direction not held (--continuous-swim) */
    unsigned capped_frames;      /* frames the extra move was withheld: a collision ahead in the room entered */
    unsigned refused_frames;     /* frames in a transition the policy left alone (Link not on foot, an item out) */
    unsigned simulated_frames;   /* of them, those of a scene walking him with the game's own keys */
    unsigned refused_by[11];     /* by reason: 0 game/room, 1 state, 2 Link's state, 3 not walking, 4 airborne, 5 holding/pushing, 6 item, shield, seeds, knocked or stunned, 7 animation data, 8 swimming, 9 under water, 10 a savestate loaded in the transition without the policy's progress */
    unsigned unanimated_frames;  /* frames Link was not moved because his animation stream could not be read */
    unsigned palette_refreshes;  /* palettes refreshed after a palette transition's fade outlived the doubled scroll */
    unsigned carry_asked_frames; /* frames the policy asked to move Link across the axis too (--continuous-diagonal): a diagonal held on foot, his motion in the water; those the bus applied are oracles_guest_transition_carried */
    unsigned carry_withheld_frames; /* frames it did not: a collision across in the room entered, or its collisions not loaded */
    unsigned carry_unknown_frames;  /* of them, those the room entered's collisions were neither loaded nor predicted */
    unsigned carry_unnamed_frames;  /* of those, the ones before the game named the room (wScrollMode not 8 yet), predicted a frame later */
    unsigned carry_predicted_frames; /* frames a carry was tested on the room entered's collisions as predicted, the game not having loaded them yet */
    /* The prediction of the room entered's collisions (room_predict.h), at the first frame of a transition the policy takes
     * over whose load the game has not finished: */
    unsigned predicted_transitions; /* those it predicted */
    unsigned predicted_matched;     /* of them, those whose layout and collisions are, byte for byte, what the game then loaded */
    unsigned predicted_mismatched;  /* and those whose are not */
    unsigned predicted_pending;     /* the one still waiting for the game's load (1 or 0), a replay ending during it */
    unsigned predict_fallbacks;     /* those left to the game's load, its collisions unknown until it ends */
    unsigned predict_fallback_by[ORACLES_ROOM_PREDICT_VERDICTS];   /* by reason (OraclesRoomPredictVerdict) */
    /* A step a frame (--continuous-steps), and what goes against it without: */
    unsigned carry_edge_frames;     /* frames a carry was slowed so as to reach the edge across no sooner than the scroll ends, never past it: the next transition, at a
                                       corner, would undo it (--continuous-steps) */
    unsigned end_stalls;            /* scrolls the policy moved Link through to their last step whose last frame, the one that ends them, left him still, going on
                                       (a direction held, his motion in the water) */
    unsigned doubled_starts;        /* transitions whose first frame moved him further on an axis than a step, the game's and the policy's together */
    unsigned released_frames;       /* frames of a transition the policy took him through on foot with no direction held toward the edge: the game's own move (--continuous-steps) */
    unsigned stopped_frames;        /* of those with --continuous-line, none held at all: he stands, the machine's move undone */
    unsigned standing_frames;       /* of the frames he stood, those he was given the game's standing pose (--continuous-line) */
    unsigned back_frames;           /* and with one held away from the edge: he walks back toward the line (--continuous-line) */
    unsigned back_line_frames;      /* of those, the frames the line held him, the room entered's edge (--continuous-line) */
    unsigned back_past_frames;      /* frames he walked back past the line into the room left, on its collisions (--continuous-line) */
    unsigned back_wall_frames;      /* frames a wall or a hole of the room left held that walk */
    unsigned back_reversals;        /* scrolls that ended with him back in the room left, the transition back forced from where he stood */
    unsigned back_lowered;          /* of those, the transitions back the next frame did not take, lowered: Link no longer past the line, or the transitions disabled */
    unsigned back_kept_frames;      /* frames of normal play a transition back not taken yet was kept, Link past the line (a menu, a text) */
    /* --continuous-line: the room beside the room entered across the axis (room_predict.h), and the carry past the line into it: */
    unsigned beside_predicted;      /* rooms beside predicted, at the first frame a carry toward them asked for them in a transition */
    unsigned beside_fallbacks;      /* those not predicted, the carry held short of the line as without them */
    unsigned beside_fallback_by[ORACLES_ROOM_PREDICT_VERDICTS];   /* by reason (OraclesRoomPredictVerdict) */
    unsigned beside_matched;        /* of the predicted, those the next scroll entered, whose layout and collisions are, byte for byte, what the game loaded */
    unsigned beside_mismatched;     /* and those whose are not */
    unsigned beside_unchecked;      /* those the next scroll did not enter (another way, or no scroll), or made before the last one's was compared */
    unsigned beside_past_frames;    /* frames a carry took Link past the line across, into the room beside */
    unsigned beside_held_frames;    /* frames it was held BESIDE_PAST_LINE past it */
    unsigned beside_wall_frames;    /* frames a wall, a hole or water of the room beside held a step of his, across or along, the carry included */
    unsigned beside_crossings;      /* scrolls that ended with him past the line across, the scroll into the room beside started from where he stood */
    unsigned beside_returned_frames; /* frames he walked back across to the line, the transitions disabled since he went past it */
    /* --continuous-line, in the water: the game's swim routines as the policy steps them (oracles_room_transition_swim_step). */
    unsigned swim_model_frames;     /* frames of normal play a swim's step of the game was compared with the policy's from the frame before */
    unsigned swim_model_mismatched; /* of them, those whose motion differed in a field */
    unsigned swim_model_first_mismatch; /* the frame of the first, 0 for none */
    unsigned swim_line_frames;      /* frames of a scroll the policy stepped his swim and moved him by it, the machine's own move undone */
    unsigned swim_held_frames;      /* of them, a move on an axis held: land or a wall ahead, the line across, the room left's land behind */
    unsigned swim_untested_frames;  /* moves across made while the room entered's collisions were neither loaded nor predicted */
    unsigned swim_lifted_frames;    /* frames of an upward scroll that took him over a swimmer's line out of the room entered's bottom rows */
    /* --continuous-line, on foot, a direction held toward the edge, past the place the game's own scroll would have left him: */
    unsigned line_wall_frames;      /* frames a wall of the room entered held his step along the axis, as the game's walk would */
    unsigned line_unknown_frames;   /* frames its collisions, neither loaded nor predicted, held him at that place */
} OraclesRoomTransitionStats;
/* The policy's progress through the transition in progress, for a savestate: ORACLES_ROOM_TRANSITION_STATE_SIZE bytes
 * written into `out`, their count returned; 0 when `capacity` is short. */
#define ORACLES_ROOM_TRANSITION_STATE_SIZE (99u + 8u * ORACLES_ROOM_LAYOUT_BYTES)
size_t oracles_room_transition_save_state(const OraclesRoomTransition *transition, uint8_t *out, size_t capacity);
/* That progress given back after a savestate's load reset the guest (oracles_guest_reset_execution_state): the policy
 * goes on with the transition the state was saved in as if no load had come between.  0, or -1 for data of another
 * size or version, which leaves that transition to the machine alone, its step as the policy had it. */
int oracles_room_transition_load_state(OraclesRoomTransition *transition, const uint8_t *data, size_t size);
/* Where the straight path of the transition in progress, or of the last one, runs across its axis (8.8: x for a
 * vertical transition, y for a horizontal one; Link's own less this, what the diagonal carried him off it) and its
 * direction; 0 when none was seen since the start or a savestate's load. */
int oracles_room_transition_straight_cross(const OraclesRoomTransition *transition, unsigned *direction, uint16_t *cross);
/* The first reason a state is not eligible (the index of refused_by), -1 when eligible; `swim` as at the start. */
int oracles_room_transition_refusal(const OraclesGuestTransitionState *state, int swim);
/* Whether the game drives Link with its own keys (wUseSimulatedInput), a scene's walk: its transitions are left to it. */
int oracles_room_transition_simulated(const OraclesGuestTransitionState *state);
void oracles_room_transition_stats(const OraclesRoomTransition *transition, OraclesRoomTransitionStats *out);

/* Pure helpers, testable without a ROM. */
/* Whether the policy applies to this state at all (game, group, room size, transition, Link on foot or swimming). */
int oracles_room_transition_eligible(const OraclesGuestTransitionState *state, int swim);
/* The transition the game would start now if the player held the direction Link swims in at the surface, during a
 * stroke of the flippers (screenTransitionState2@transition, its checks in its order, the direction held replaced by
 * his motion's angle), when the direction held does not take him there already: 0 up, 1 right, 2 down, 3 left, -1 none. */
int oracles_room_transition_swim_edge(const OraclesGuestTransitionState *state);
/* The direction across the transition's axis a diagonal held toward the edge adds (wLinkAngle, a step of 45 degrees
 * off the axis): 0 up, 1 right, 2 down, 3 left; -1 for none, straight or away. */
int oracles_room_transition_held_across(const OraclesGuestTransitionState *state);
/* Whether Link, moved by `along` (8.8) in the transition's direction and `delta` in the direction `across`, is free
 * there on that side at the two points the game tests for walls (calculateAdjacentWallsBitset), in the room being
 * entered: no wall, no hole or water; and the two of the side he goes along the axis, free without the move across,
 * stay free with it; while a point is still in the room he leaves along the axis, the room entered's first row or
 * column; and from there to `rest` (8.8, what remains of the game's own distance) further along the axis: Link
 * `offset` (8.8, signed along x or y) off the straight path across it before this move, carried, keeps every point
 * the game tests for walls, and the tile it takes as his own (y+5: a hole, water or lava), as clear as the straight
 * path does.  0 while the room entered's collisions are not loaded, or when a point leaves the room across. */
int oracles_room_transition_across_free(const OraclesGuestTransitionState *state, unsigned across, int delta, int along, int offset, int rest);
/* The same with --continuous-line, where the game lets Link stand up to the room's edge across: his points past it read
 * the collision buffer's border, neither a wall nor a hole. */
int oracles_room_transition_across_free_line(const OraclesGuestTransitionState *state, unsigned across, int delta, int along, int offset, int rest);
/* The same for Link swimming: its points on water he can swim in (as oracles_room_transition_swimmable), and his own
 * tile out of water he can swim in (land, a hole, lava, the sea without the mermaid suit) only where the straight
 * path's is. */
int oracles_room_transition_across_swimmable(const uint8_t *rom, size_t rom_size, const OraclesGuestTransitionState *state, unsigned across, int delta,
                                             int along, int offset, int rest);
/* In the water, his motion's angle (w1Link.angle, 32 steps) off the transition's direction by 1 to 7 steps: the
 * direction across it (0 up, 1 right, 2 down, 3 left), and `step` (8.8) split along and across it as
 * objectSpeedTable splits a speed; -1 for none, straight, along the edge or away. */
int oracles_room_transition_moving_across(const OraclesGuestTransitionState *state, int step, int *along, int *across_step);
/* What the machine still moves Link along the transition's axis after this frame (8.8): a step for each row or column
 * the scroll has yet to draw, the counter whole until its phase 1, decremented at each step landing the camera on a
 * multiple of 8, the last step taken at zero; with `doubled`, steps of 8 px, one a row; without, steps of 4 px, two a
 * row; before the scroll, all of them; 0 once it is over. */
int oracles_room_transition_machine_rest(const OraclesGuestTransitionState *state, int doubled);
/* Link's swim as the game steps it a frame (--continuous-line): w1Link.angle, speed, speedTmp, var12, var13, var35,
 * counter1, var3e, direction. */
typedef struct OraclesSwimMotion {
    uint8_t angle, speed, speed_tmp, counter, period, stroke, counter1, mermaid_stroke, direction;
} OraclesSwimMotion;
/* What the routines read besides: the keys (wLinkAngle, wGameKeysPressed, wGameKeysJustPressed), the mermaid suit worn
 * (w1Link.var2f bit 6), the swimmer's ring, the swim's hold, wLinkImmobilized. */
typedef struct OraclesSwimKeys {
    uint8_t input_angle, pressed, just_pressed, mermaid, swimmers_ring, swim_hold, immobilized;
} OraclesSwimKeys;
/* overworldSwimmingState3's step of his motion, a frame: updateLinkDirectionFromAngle, then the mermaid suit's
 * linkUpdateVelocity@mermaidSuit, or the flippers' linkUpdateFlippersSpeed and func_5933 (link.s), the swim's hold's
 * strokes included as its tests add them. */
void oracles_room_transition_swim_step(OraclesSwimMotion *motion, const OraclesSwimKeys *keys);
/* His move a frame at `speed` and `angle` (8.8, signed), from objectSpeedTable as getPositionOffsetForVelocity reads it;
 * 0 when the table cannot be read. */
int oracles_room_transition_swim_velocity(const uint8_t *rom, size_t rom_size, const OraclesTables *data, uint8_t speed, uint8_t angle, int *dy, int *dx);
/* Whether Link swims at the surface (wLinkSwimmingState). */
int oracles_room_transition_swimming(const OraclesGuestTransitionState *state);
/* Whether he is in the water: swimming at the surface, or on Ages' sea floor. */
int oracles_room_transition_in_water(const OraclesGuestTransitionState *state);
/* Link's own move a frame along an axis, 8.8: a pixel on foot, his speed in the water (objectSpeedTable). */
int oracles_room_transition_step(const OraclesGuestTransitionState *state);
/* The tile type of `tile` in the list the room's collisions select (lookupCollisionTable over tileTypesTable), -1
 * when the table cannot be read. */
int oracles_room_transition_tile_type(const uint8_t *rom, size_t rom_size, const OraclesGuestTransitionState *state, uint8_t tile);
/* Whether the tile at `index` (16 a row) of the room's layout is water Link swims in, by its type alone, as
 * linkApplyTileTypes reads the one under him: the rule of oracles_room_transition_swimmable. */
int oracles_room_transition_water_tile(const uint8_t *rom, size_t rom_size, const OraclesGuestTransitionState *state, unsigned index);
/* The next frame of Link's walking animation from the ROM's animation data
 * (specialObjectNextAnimationFrame: duration, base frame, parameter, `$ff`
 * then a signed offset to loop).  Fills the animation fields of `mutation`;
 * 0 when the data cannot be read. */
int oracles_room_transition_next_walk_frame(const uint8_t *rom, size_t rom_size, const OraclesGuestTransitionState *state,
                                            OraclesGuestTransitionMutation *mutation);
/* The frame Link stands on with no direction held (animateLinkStanding: the walk's animation set again,
 * specialObjectAnimationTable's first entry for it), into the animation fields of `mutation`; 0 when the data cannot
 * be read. */
int oracles_room_transition_standing_frame(const uint8_t *rom, size_t rom_size, const OraclesGuestTransitionState *state,
                                           OraclesGuestTransitionMutation *mutation);
/* Whether Link, moved by `extra` (8.8) along the transition's axis, stands on
 * collision-free tiles of the room being entered (or still in the room he
 * leaves): the tile the game takes as his own (y+5) and the one his leading
 * edge reaches (y-3 up, y+7 down, x+4 right, x-5 left), and none of the eight
 * points the game tests for walls in a wall it is not in moved by `from`.  1
 * when the room's collisions are not loaded yet. */
int oracles_room_transition_walkable(const OraclesGuestTransitionState *state, int from, int extra);
/* How far into a wall Link stands at (x, y), whole pixels of a room `w` x `h` whose collisions are `collisions`
 * (wRoomCollisions, 16 a row; outside the room, no wall): the most any of the eight points calculateAdjacentWallsBitset
 * tests lies in a wall, counted back toward him on its own side, beyond the depth the game's walk can leave it at (2 px
 * up, 1 px on the other sides: the points of a side are tested only as he goes that way); 0 where his walk may stand
 * him, as the game's own transitions do. */
int oracles_room_transition_wall_excess(const uint8_t *collisions, int w, int h, int x, int y);
/* The same with --continuous-swim: stairs, and a bridge crossed along the transition's axis, are walked on too. */
int oracles_room_transition_passable(const OraclesGuestTransitionState *state, int from, int extra);
/* The same for Link swimming: onto water he can swim in, by its tile type alone as linkApplyTileTypes takes it (water, a
 * current; in Ages the sea with the mermaid suit, and a whirlpool). */
int oracles_room_transition_swimmable(const uint8_t *rom, size_t rom_size, const OraclesGuestTransitionState *state, int from, int extra);

#ifdef __cplusplus
}
#endif

#endif
