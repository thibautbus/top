/* The sprites of an object, built from its fields and the game's sprite data
 * (docs/GAME_HOOKS.md, section 4.2).
 *
 * In a large room the game draws only what falls inside its 160x128 window:
 * drawAllSprites@drawObject places each sprite of an object's block and drops
 * the ones whose OAM y is $a0 or more or whose x is $a8 or more, so an object
 * beyond the window's edge has no OAM entry to take.  The wide view builds
 * them from the same data, by a deliberate exception that
 * relaxes, for this conversion alone, the rule of no drawing logic
 * reimplemented: the object's position, its oamFlags, oamTileIndexBase and
 * oamDataAddress, and the block of the ROM that address names (a count, then
 * per sprite y and x offsets, a tile added to the base, flags xored with the
 * object's); and its terrain effects (_drawObjectTerrainEffects): the grass
 * or the puddle under an object on the ground, written with it, and the
 * shadow of an object in the air, one frame in two, written after the loop
 * of objects.  The harness proves it against the game, object by object and
 * frame by frame: the entries built for the window are the entries it writes. */
#ifndef ORACLES_NEIGHBOURS_OBJECT_SPRITES_H
#define ORACLES_NEIGHBOURS_OBJECT_SPRITES_H

#include "guest.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ORACLES_OBJECT_SPRITES_MAX 64u   /* sprites of one object, its terrain effects included (the count is a byte; the game's blocks are far shorter) */

enum { ORACLES_SPRITE_OWN = 0, ORACLES_SPRITE_GROUND = 1, ORACLES_SPRITE_SHADOW = 2 };

typedef struct OraclesObjectSprite {
    int16_t y, x;          /* top left on the game's screen (OAM y less 16, x less 8; the status bar is rows 0-15), unwrapped */
    uint8_t oam_y, oam_x;  /* the bytes the game computes, wrapped */
    uint8_t tile, attr;
    uint8_t in_window;     /* the game writes it as far as its position goes: an own sprite below OAM y $a0 and x $a8; a terrain effect always */
    uint8_t effect;        /* ORACLES_SPRITE_OWN, _GROUND (grass or puddle, written before the object's own), _SHADOW (written after every object) */
    uint8_t written;       /* oracles_object_sprites_all: the game writes it, room in its OAM included */
    uint8_t oam_index;     /* when written: its entry, 0 to 39 */
} OraclesObjectSprite;

/* The sprites of the object at `object` (64 bytes of the instance given) as
 * @drawObject writes them, with the camera and the terrain as they stand:
 * first its grass or puddle, then its own sprites in the order of its block,
 * then the shadow it queues, if any.  An object on the ground in a puddle is
 * drawn a pixel lower.  The object's fields are taken as they are: Ages'
 * wLinkRaisedFloorOffset, which drawAllSprites adds to Link's yh for the
 * drawing alone, is in them inside the drawing, not outside.  Returns how many
 * were written, 0 for an object not drawn (not enabled, visible bit 7 clear,
 * an empty block), -1 when the data cannot be read. */
int oracles_object_sprites_build(OraclesGuest *guest, const uint8_t *object, const uint8_t *rom, size_t rom_size,
                                 OraclesObjectSprite out[], unsigned max);

typedef struct OraclesDrawnObject {
    uint8_t kind;          /* the object's place in its slot: 0 item or special, 1 interaction, 2 enemy, 3 part */
    uint8_t slot;          /* 0 to 15 */
    uint8_t first, count;  /* its sprites in the list, its shadow excluded */
} OraclesDrawnObject;

/* A whole drawAllSprites, out of it (at its entry or its return): every
 * object the game queues (queueDrawEverything, then Link, the companion and
 * the parent items; Link alone of those with the text box's alternative
 * palette), in the order it draws them (the four priorities of the visible
 * byte, sixteen objects at most each), Ages' raised floor applied to Link,
 * then the shadows in the order they were queued.  `first_entry` is hOamTail
 * / 4 when the drawing starts (the status bar's entries before): each sprite
 * says whether the game writes it and where.  Returns how many objects were
 * written to `objects`, or -1 when data cannot be read. */
int oracles_object_sprites_all(OraclesGuest *guest, const uint8_t *rom, size_t rom_size, unsigned first_entry,
                               OraclesDrawnObject objects[], unsigned max_objects, OraclesObjectSprite out[], unsigned max, unsigned *sprites);

#ifdef __cplusplus
}
#endif

#endif
