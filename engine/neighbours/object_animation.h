/* An object's animation, advanced from the game's data (docs/GAME_HOOKS.md,
 * section 4.2).
 *
 * The game animates an interaction, an enemy or a part by the same routine
 * of bank 0 (interactionAnimate, enemyAnimate, partAnimate): animCounter is
 * counted down, and when it runs out _xxxNextAnimationFrame reads at
 * animPointer three bytes of the bank of the kind's animation table: the
 * frames to hold the next pose (or $ff then an offset, which loops back
 * once), the pose's index, and animParameter; animPointer moves past them,
 * and oamDataAddress takes the block that index names in the kind's OAM data
 * table for the object's id (its two high bits cut, $40 added for an
 * interaction and a part).  The object's own code calls it, once or more a
 * frame, and chooses the animation (xxxSetAnimation): the choice is not
 * followed here.
 *
 * A neighbour's objects are captured frozen; their animation is advanced
 * here, by a deliberate exception that relaxes, for this step alone, the
 * rule of no game logic reimplemented, the way the tiles' animation and the
 * objects' sprites relax it.  The harness proves it against the objects the
 * live game animates (--object-animation-check). */
#ifndef ORACLES_NEIGHBOURS_OBJECT_ANIMATION_H
#define ORACLES_NEIGHBOURS_OBJECT_ANIMATION_H

#include "guest.h"
#include "objects.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OraclesObjectAnimation {
    uint8_t kind;               /* ORACLES_OBJECT_KIND_* */
    uint8_t id;                 /* the object's id: its row of the OAM data table */
    uint8_t counter;            /* animCounter */
    uint8_t parameter;          /* animParameter */
    uint16_t pointer;           /* animPointer: the next entry of the animation's data */
    uint16_t oam_data_address;  /* oamDataAddress: the pose shown */
} OraclesObjectAnimation;

/* Whether the game's tables of that kind are known: a fan game whose tables
 * are not resolved keeps its objects in their captured pose. */
int oracles_object_animation_resolved(const OraclesGuestTables *t, unsigned kind);

/* An object's animation as a record holds it. */
void oracles_object_animation_from_record(OraclesObjectAnimation *a, const OraclesObjectRecord *r);

/* One call of xxxAnimate: the counter counted down, and at zero the next
 * entry read and the pose it names put in oam_data_address.  Returns 1 when
 * an entry was read (the pose may be the same), 0 when the counter only
 * moved, -1 when the data cannot be read or the kind is not resolved (the
 * state is then left as it was). */
int oracles_object_animation_step(OraclesObjectAnimation *a, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size);

#define ORACLES_OBJECT_ANIMATION_POSES 32u   /* the entries followed to find a loop: the game's longest are far shorter */

/* The poses the animation shows from this state on, each once, in the order
 * it shows them, the current one first, until it stands again where it
 * stood after an entry already read: its loop.  `period` receives the loop's
 * length in calls and `changes` the pose changes it makes over one loop
 * (0 for a loop of one pose, or an animation that does not come back within
 * ORACLES_OBJECT_ANIMATION_POSES entries, whose poses are given as far as
 * followed).  Returns how many poses were written, 0 when the data cannot be
 * read. */
unsigned oracles_object_animation_poses(const OraclesObjectAnimation *a, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size,
                                        uint16_t poses[], unsigned max, unsigned *period, unsigned *changes);

/* Whether `pointer` is the place xxxSetAnimation leaves an object at for one of
 * the animations of its id, the entry read (an animation chosen by its code). */
int oracles_object_animation_is_set(const OraclesObjectAnimation *a, uint16_t pointer, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size);

/* Whether one call of the step from `last` gives `now`: the object's code
 * advanced its animation by the step alone, as interactionAnimate does once
 * a frame. */
int oracles_object_animation_one_step(const OraclesObjectAnimation *last, const OraclesObjectAnimation *now, const OraclesGuestTables *t,
                                      const uint8_t *rom, size_t rom_size);

/* Whether, from `last`, the object's code set again the animation it plays
 * (`now` stands just past an entry of that loop, its pose the entry's): a
 * water-carrying Subrosian does so on its first update, choosing again the
 * animation it was created with, and only then animates it. */
int oracles_object_animation_restarted(const OraclesObjectAnimation *last, const OraclesObjectAnimation *now, const OraclesGuestTables *t,
                                       const uint8_t *rom, size_t rom_size);

/* Whether a neighbour animates the object: an interaction or a part, of a
 * kind whose tables are known, whose own code advanced its animation by one
 * call of the step at each update of its capture's probe, the first of which
 * may set again the animation it plays (the ghost lets the room's objects run
 * a little after the capture: ghost.h, ORACLES_GHOST_PROBE_UPDATES).  Every other object stays in its captured
 * pose: one its code holds or resets (Vasu's snakes), never animates (Tingle's
 * balloon, a seed shooter, a platform), animates in another state than the
 * one captured (a springbloom flower, still until Link touches it), turns or
 * animates otherwise than by one call a frame.  An enemy's code animates it as
 * it walks: in place it would walk without moving, which the game never
 * shows, so an enemy stays in its captured pose. */
int oracles_object_animation_animated(const OraclesGuestTables *t, const OraclesObjectRecord *r);
/* The kinds of object a capture's probe asks about, those a neighbour may animate: interactions and parts, never an
 * enemy, so that a capture of enemies alone runs no probe. */
int oracles_object_animation_probed(unsigned kind);

#ifdef __cplusplus
}
#endif

#endif
