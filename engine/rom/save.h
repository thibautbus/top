/* The save of Ages and Seasons, the raw 8 KiB SRAM a `.sav` holds (docs/ROM_DATA_FORMATS.md, section 14): three game
 * files, each in two copies, a copy valid when its checksum and its game's string hold.  The European ROMs keep the
 * same files, with a header of their own before the first and their language in each file; a fan game made on an
 * Oracle keeps its Oracle's. */
#ifndef ORACLES_SAVE_H
#define ORACLES_SAVE_H

#include "rom.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ORACLES_SAVE_SIZE 0x2000u
#define ORACLES_SAVE_COPIES 6u

/* The offset in the `.sav` of each copy: file 1, 2 and 3, then their second copies. */
extern const unsigned oracles_save_copies[ORACLES_SAVE_COPIES];

/* The checksum a copy at `at` should hold. */
unsigned oracles_save_checksum(const uint8_t *sram, unsigned at);
/* The copy at `at` is valid for `game` (Ages' or Seasons' string, its checksum). */
int oracles_save_copy_valid(const uint8_t *sram, unsigned at, OraclesGame game);
/* The checksum of the copy at `at` written again, after a change of its bytes. */
void oracles_save_seal(uint8_t *sram, unsigned at);

/* The copies of an SRAM image of `size` bytes valid for `game`, as bits (1 << the copy's index); 0 for an image of
 * another size. */
unsigned oracles_save_valid(const uint8_t *sram, size_t size, OraclesGame game);
/* Oracles Combined's save, its cartridge's 32 KiB in four banks of 8 KiB: the copies it keeps of the game not played,
 * code, then Ages' save and Seasons' save, each laid out as the originals'. */
#define ORACLES_SAVE_COMBINED_SIZE 0x8000u
/* The copies of an Oracles Combined SRAM image valid, Ages' in bits 0 to 5 and Seasons' in bits 6 to 11; 0 for an image
 * of another size. */
unsigned oracles_save_combined_valid(const uint8_t *sram, size_t size);
/* Each valid copy whose language is not English set to it, its checksum written again: a save of the European ROM
 * played in German, Italian or Spanish reads the US game's texts at the wrong place.  The European header stays,
 * which the US game never reads.  Returns the copies changed. */
unsigned oracles_save_set_english(uint8_t *sram, size_t size, OraclesGame game);

#ifdef __cplusplus
}
#endif

#endif
