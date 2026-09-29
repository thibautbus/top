/* Reading and identifying the user's ROM.
 *
 * An authenticated supported image is recognised by SHA-1.  Any other image whose header is
 * that of an Oracle game (title, CGB flag, MBC5 with battery, plausible size)
 * is accepted for the Faithful profile as an unknown ROM: hacks and randomizer
 * seeds play, and the later steps decide what else they get. */
#ifndef ORACLES_ROM_H
#define ORACLES_ROM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OraclesGame {
    ORACLES_GAME_UNKNOWN = 0,
    ORACLES_GAME_AGES = 1,
    ORACLES_GAME_SEASONS = 2
} OraclesGame;

/* Exact authenticated image, independent of the family named by its header. */
typedef enum OraclesRomRevision {
    ORACLES_ROM_REVISION_UNKNOWN = 0,
    ORACLES_ROM_REVISION_AGES_US,
    ORACLES_ROM_REVISION_SEASONS_US,
    ORACLES_ROM_REVISION_MOONRISE_1_0_6,
    ORACLES_ROM_REVISION_TEMPLE_1_073,
    ORACLES_ROM_REVISION_KINOMI_1_1_2
} OraclesRomRevision;

typedef struct OraclesRomInfo {
    OraclesGame game;
    OraclesRomRevision revision;
    int known;          /* 1: SHA-1 of an authenticated supported image */
    char title[12];     /* header title, NUL-terminated */
    char sha1[41];
    size_t size;
} OraclesRomInfo;

/* Reads a whole file; the caller frees the buffer. Returns NULL and an error message on failure. */
uint8_t *oracles_rom_read_file(const char *path, size_t *size, char *error, size_t error_capacity);

/* Identifies an image. Returns 0 when it can be played, -1 with an error message otherwise. */
int oracles_rom_identify(const uint8_t *rom, size_t size, OraclesRomInfo *info, char *error, size_t error_capacity);

const char *oracles_game_name(OraclesGame game);
/* One of the two original US ROMs, not another authenticated image of their family (a fan game's). */
int oracles_rom_is_original(const OraclesRomInfo *info);

#ifdef __cplusplus
}
#endif

#endif
