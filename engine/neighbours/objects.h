/* The objects of a room as the host reads them, and the number each received
 * when the room created it.
 *
 * A room's objects are created during its initialisation, in the order of its
 * object list; the host numbers them there, from the creation events of the
 * bus, and keeps the number by slot until the slot is freed.  Two instances
 * that created the same room the same way give the same sequence of numbers,
 * which is how an object of a neighbour is paired with the live object that
 * replaces it at the real entry.  An object created later, by another object,
 * has no number and is never paired.
 *
 * The collector observes only: it never writes into the instance it reads. */
#ifndef ORACLES_NEIGHBOURS_OBJECTS_H
#define ORACLES_NEIGHBOURS_OBJECTS_H

#include "guest.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ORACLES_OBJECT_KIND_INTERACTION 1u
#define ORACLES_OBJECT_KIND_ENEMY 2u
#define ORACLES_OBJECT_KIND_PART 3u
#define ORACLES_OBJECT_RECORDS 48u        /* sixteen slots of the three kinds */

/* What draws an object and animates it, on one frame. */
typedef struct OraclesObjectLook {
    uint8_t visible, zh, oam_flags, oam_tile_base, anim_counter, anim_parameter;
    uint16_t oam_data_address, anim_pointer;
} OraclesObjectLook;

typedef struct OraclesObjectRecord {
    uint8_t kind;              /* ORACLES_OBJECT_KIND_* */
    uint8_t slot;              /* 0 to 15 */
    uint8_t number;            /* rank among the objects the room's initialisation created, from 1; 0: created later */
    uint8_t enabled, id, subid, var03, state, direction, visible;
    /* id, subid and var03 as the room's initialisation left them, before the
     * object's own code runs: many rewrite their subid or var03 then, so the
     * pairing uses these, the same in every instance that created the room
     * alike, whatever moment each side reads its objects at. */
    uint8_t created_id, created_subid, created_var03;
    /* Its position on the frame whose OAM a capture keeps (oracles_objects_note_drawn):
     * an object may finish its initialisation over several frames, moving
     * after the frame it was latched on, and a neighbour shows it where it is drawn. */
    uint8_t drawn_y, drawn_yh, drawn_x, drawn_xh;
    /* And its look on that frame: the record is kept at the first frame its
     * state is not zero, before its code may have made it visible, set its
     * animation or its graphics. */
    OraclesObjectLook drawn;
    uint8_t oam_flags, oam_tile_base;
    uint8_t y, yh, x, xh, zh;
    uint16_t oam_data_address;
    uint8_t anim_counter, anim_parameter;   /* the animation's state (object_animation.h) */
    uint16_t anim_pointer;
    uint16_t created_pc;       /* the address the allocator returned to */
    uint8_t randomly_placed;   /* created by the random-placement opcode of the room's list */
    /* In a ghost's capture: 1 when the object's own code advanced its
     * animation by one call of the step at each update of the probe that
     * follows the capture (ghost.h), the rule by which a neighbour animates it. */
    uint8_t animated_by_code;
} OraclesObjectRecord;

typedef struct OraclesObjects OraclesObjects;

OraclesObjects *oracles_objects_create(OraclesGuest *guest);
void oracles_objects_destroy(OraclesObjects *o);

/* A savestate loaded into the instance: no object keeps a number until the next room's initialisation. */
void oracles_objects_forget(OraclesObjects *o);

/* Every event of that instance's bus: the numbering follows ROOM_ENTER, the
 * creations and ROOM_INITIALIZED. */
void oracles_objects_event(OraclesObjects *o, const OraclesGuestEvent *event);

/* The objects of the room as they stand now, numbers included; returns how
 * many were written (at most ORACLES_OBJECT_RECORDS). */
unsigned oracles_objects_snapshot(const OraclesObjects *o, OraclesObjectRecord out[], unsigned max);

/* Called once per iteration of the instance: each numbered object is kept as
 * it stands the first time its state is not zero, that is once its own
 * initialisation has run and before the game moves it.  That state is what a
 * neighbour shows and what the real entry must produce, so it is what the two
 * sides compare.  Reset by the next ROOM_ENTER. */
void oracles_objects_latch(OraclesObjects *o);
/* At the end of the entry: every numbered object not yet kept is kept as it
 * stands, initialised or not.  An object that waits in state 0 for Link (a
 * quicksand, a door controller) is then compared like the others instead of
 * being missing on one side. */
void oracles_objects_latch_remaining(OraclesObjects *o);
unsigned oracles_objects_latched(const OraclesObjects *o, OraclesObjectRecord out[], unsigned max);
/* A kept object with the key its room's initialisation gave it, as a snapshot
 * reads it.  One created in the room's first update (Seasons' troupe, which an
 * object of 0:98 creates there; Nayru and the animals of Ages' 0:39) may be
 * kept once its own code has run, before that update returns and its key is
 * read, when the update spans two frames; the key read since is the one the
 * live room pairs by.  A key not read yet (0) leaves the record's. */
OraclesObjectRecord oracles_objects_keyed(const OraclesObjectRecord *latched, const uint8_t key[3]);

/* Whether the object was created by the random-placement opcode of the room's
 * list (obj_RandomEnemy, objectDataOp6): its position depends on the RNG and
 * on the edge of entry, so a neighbour never shows it. */
int oracles_objects_randomly_placed(const OraclesObjects *o, const OraclesObjectRecord *record);

/* The hooks the numbering needs besides those every guest carries: the entry
 * of the random-placement opcode and that of the list's parser, which ends it.  To be added to the instance
 * whose objects are collected, live or ghost.  Returns 0, or -1 when the
 * instance's hook table is full. */
int oracles_objects_add_hooks(OraclesGuest *guest);

/* Each latched object's position and look now, for the frame a capture keeps: into `records`, matched by kind and slot. */
void oracles_objects_note_drawn(const OraclesObjects *o, OraclesObjectRecord records[], unsigned count);
/* A record as the frame its capture keeps draws it: its place and its look there, not as it was latched; its
 * animation as latched, but when no animation was set then. */
OraclesObjectRecord oracles_objects_as_drawn(const OraclesObjectRecord *record);

/* The enemies the game counts as killed in the room of that index
 * (wEnemiesKilledList: eight pairs of a room index, whatever the group, and a
 * bitset by the enemy's index; roomInitialization.s).  A room not in the list
 * has its enemies back: 0.  What a room creates depends on it
 * (checkEnemyKilled), so a capture of the room is valid for one value. */
uint8_t oracles_objects_killed_enemies(OraclesGuest *guest, uint8_t room);
/* The same, in a copy of the list (16 bytes). */
uint8_t oracles_objects_killed_in_list(const uint8_t list[16], uint8_t room);

#ifdef __cplusplus
}
#endif

#endif
