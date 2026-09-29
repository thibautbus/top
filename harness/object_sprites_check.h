/* The check of the sprites the wide view builds for the objects of a large
 * room beyond the game's window: for every object the
 * live game draws, the sprites built from its fields and the game's data, cut
 * as the game cuts them at the window's edges and at the end of its OAM, must
 * be the entries the game writes for it, entry for entry.  Judged at each
 * object's drawing (the hooks of drawAllSprites@drawObject), outside the
 * scrolling transitions, where the game places the objects by another
 * routine. */
#ifndef ORACLES_HARNESS_OBJECT_SPRITES_CHECK_H
#define ORACLES_HARNESS_OBJECT_SPRITES_CHECK_H

#include "guest.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct OraclesObjectSpritesCheck OraclesObjectSpritesCheck;

OraclesObjectSpritesCheck *oracles_object_sprites_check_start(OraclesGuest *guest, const uint8_t *rom, size_t rom_size);
void oracles_object_sprites_check_stop(OraclesObjectSpritesCheck *c);
/* Every event of the live instance's bus. */
void oracles_object_sprites_check_event(OraclesObjectSpritesCheck *c, const OraclesGuestEvent *event);
/* The live state jumped (a savestate loaded back): a drawing under way is dropped. */
void oracles_object_sprites_check_reset(OraclesObjectSpritesCheck *c);
void oracles_object_sprites_check_summary(const OraclesObjectSpritesCheck *c, FILE *out);
void oracles_object_sprites_check_report(const OraclesObjectSpritesCheck *c, FILE *out);

#endif
