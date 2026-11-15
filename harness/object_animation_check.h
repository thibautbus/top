/* The check of the objects' animation the host advances
 * (object_animation.h) against the live game, which animates the objects of
 * its room by the same data.
 *
 * At every return of updateAllObjects in normal play, each object a
 * neighbour would show (numbered, not placed at random, drawn, of a kind
 * whose tables are known) is judged: one call of the host's step from its
 * state at the last update must give its state now.  What the game did
 * otherwise is told apart: no call (its code did not animate it), several,
 * an animation its code chose, a pose its code wrote, or anything else.
 * The pose changes the step predicts are counted against those the game
 * shows on the same update, for the objects a neighbour animates by the rule
 * on their capture (oracles_object_animation_animated, the capture's probe):
 * none may be wrong while Link has not come near the object, as in a
 * neighbour.  The objects the rule keeps in their captured pose, and those
 * no capture paired, are counted apart; the probe's verdict is held against
 * the game's first updates after the scroll.
 *
 * Each room entered by a scrolling transition is also captured as a
 * neighbour is (the ghost, objects frozen): the poses of each captured
 * object's loop, read from the ROM, and for each pose the live object shows,
 * its tiles in the capture against the live VRAM's; the state the capture
 * holds against the object's when the game animates it first; and the cost
 * of a pose's redraw, timed on the capture with the software PPU. */
#ifndef ORACLES_HARNESS_OBJECT_ANIMATION_CHECK_H
#define ORACLES_HARNESS_OBJECT_ANIMATION_CHECK_H

#include "ghost.h"
#include "guest.h"
#include "objects.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct OraclesObjectAnimationCheck OraclesObjectAnimationCheck;

/* `dir`, may be NULL: where object_animation.tsv is written, a line per captured object. */
OraclesObjectAnimationCheck *oracles_object_animation_check_start(OraclesGuest *guest, const uint8_t *rom, size_t rom_size, const char *dir);
/* The guest went to the other half of an image of two games: that half's banks. */
void oracles_object_animation_check_half_switched(OraclesObjectAnimationCheck *check, const uint8_t *rom, size_t rom_size);
void oracles_object_animation_check_stop(OraclesObjectAnimationCheck *c);
/* A return of updateAllObjects in the live instance, its objects as the collector numbers them. */
void oracles_object_animation_check_updated(OraclesObjectAnimationCheck *c, const OraclesObjectRecord *now, unsigned count, uint32_t frame);
/* The capture of the room the live game enters (the ghost's run, objects frozen). */
void oracles_object_animation_check_captured(OraclesObjectAnimationCheck *c, const OraclesGhostResult *frozen, uint32_t frame);
void oracles_object_animation_check_summary(OraclesObjectAnimationCheck *c, FILE *out);
void oracles_object_animation_check_report(OraclesObjectAnimationCheck *c, FILE *out);

#endif
