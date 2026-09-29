/* The sprites of a frame, each tagged with the object that wrote it.
 *
 * The game draws its objects one after another into wOam: an object's grass
 * or puddle inside its own drawing, before its sprites; the shadows it queued
 * on the way (wTerrainEffectsBuffer), after the loop.  The host follows that
 * with hooks: what lies between hOamTail at the entry of an object's drawing
 * and hOamTail at its return belongs to that object.  Knowing which entries belong to which object is what lets the view
 * draw a neighbour's objects one by one, hide the ones the entry does not
 * show, and leave to the core the ones it draws itself.
 *
 * The collector observes only; it never writes into the instance it reads. */
#ifndef ORACLES_NEIGHBOURS_SPRITES_H
#define ORACLES_NEIGHBOURS_SPRITES_H

#include "guest.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ORACLES_SPRITE_TAGS 48u

typedef struct OraclesSpriteTag {
    uint8_t first;             /* index of the first OAM entry written for it */
    uint8_t count;             /* entries written */
    uint8_t kind;              /* object kind, as in objects.h; 0: a special object (Link, the companion, an item) */
    uint8_t slot;              /* 0 to 15 */
    uint8_t terrain_effect;    /* 1: the shadow, grass or puddle of that object, written after the loop */
} OraclesSpriteTag;

typedef struct OraclesSprites OraclesSprites;

OraclesSprites *oracles_sprites_create(OraclesGuest *guest);
void oracles_sprites_destroy(OraclesSprites *s);

/* The hooks the tagging needs, to be added to the instance that is read.
 * Returns 0, or -1 when the instance's hook table is full. */
int oracles_sprites_add_hooks(OraclesGuest *guest);

/* Every event of that instance's bus. */
void oracles_sprites_event(OraclesSprites *s, const OraclesGuestEvent *event);

/* The tags of the OAM given, the one the LCD showed, which lags the frame just
 * drawn: found among the last frames drawn by the content of their wOam.
 * Returns 0 when none matches (the tags are then unknown, not empty). */
unsigned oracles_sprites_tags_for_oam(const OraclesSprites *s, const uint8_t oam[160], OraclesSpriteTag out[], unsigned max);

/* The frame drawn just before the one whose wOam is `oam`: its OAM and its
 * tags.  The game draws some objects on one frame in two (the spurt of a
 * fountain, visible on even frames), and two frames in a row hold both.
 * Returns 0 when the history has no such pair. */
unsigned oracles_sprites_frame_before(const OraclesSprites *s, const uint8_t oam[160], uint8_t out_oam[160], OraclesSpriteTag out[], unsigned max);

/* A savestate loaded into the instance: the frames drawn before are about another moment. */
void oracles_sprites_forget(OraclesSprites *s);

/* The tags of the frame just drawn; returns how many were written. */
unsigned oracles_sprites_tags(const OraclesSprites *s, OraclesSpriteTag out[], unsigned max);

/* The check of the tagging itself, over the frames seen: frames where the
 * tagged entries did not add up to hOamTail, and the worst gap. */
void oracles_sprites_stats(const OraclesSprites *s, unsigned *frames, unsigned *frames_uncovered, unsigned *worst_gap, unsigned *tags_dropped);

/* Frames where the game filled its forty OAM entries: past that it draws no
 * more, and a capture loses what it would have drawn. */
unsigned oracles_sprites_frames_oam_full(const OraclesSprites *s);

/* The check of the shadows' owners: blocks written after the loop of
 * objects (the shadows of wTerrainEffectsBuffer), those no queued owner was
 * left for, owners queued that no block took, and owners not in the air. */
void oracles_sprites_shadow_stats(const OraclesSprites *s, unsigned *shadows, unsigned *unowned, unsigned *left, unsigned *grounded);

#ifdef __cplusplus
}
#endif

#endif
