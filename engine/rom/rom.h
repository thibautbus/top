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
    ORACLES_ROM_REVISION_KINOMI_1_1_2,
    /* The European ROMs (English, French, German, Spanish, Italian): never played, the sources of a language for the
     * US ROM of their game (engine/language). */
    ORACLES_ROM_REVISION_AGES_EU,
    ORACLES_ROM_REVISION_SEASONS_EU,
    /* Oracles Combined 1.5, both games in one image, as its release's BPS patch of each language makes it of the two
     * European ROMs joined, Ages' first. */
    ORACLES_ROM_REVISION_COMBINED_1_5_EN,
    ORACLES_ROM_REVISION_COMBINED_1_5_FR,
    ORACLES_ROM_REVISION_COMBINED_1_5_DE,
    ORACLES_ROM_REVISION_COMBINED_1_5_ES,
    ORACLES_ROM_REVISION_COMBINED_1_5_IT
} OraclesRomRevision;

typedef struct OraclesRomInfo {
    OraclesGame game;
    OraclesRomRevision revision;
    int known;          /* 1: SHA-1 of an authenticated supported image */
    char title[12];     /* header title, NUL-terminated */
    char sha1[41];
    size_t size;
} OraclesRomInfo;

/* Reads a whole file, or the one ROM of a .zip file (zip.h); the caller frees the buffer. Returns NULL and an error message on failure. */
uint8_t *oracles_rom_read_file(const char *path, size_t *size, char *error, size_t error_capacity);

/* Identifies an image. Returns 0 when it can be played, -1 with an error message otherwise. */
int oracles_rom_identify(const uint8_t *rom, size_t size, OraclesRomInfo *info, char *error, size_t error_capacity);

/* The authenticated image of a SHA-1 (40 lower-case hexadecimal characters): its game and revision in `info`, as
 * oracles_rom_identify gives them.  0, or -1 for a SHA-1 the port does not know. */
int oracles_rom_known(const char *sha1, OraclesRomInfo *info);

const char *oracles_game_name(OraclesGame game);
/* An image of both games in one (Oracles Combined, or the Crossworld it continues), recognised or not: 8 MiB, Ages'
 * half first, the header's own title.  It plays as an Ages ROM, the half it starts in. */
int oracles_rom_is_combined(const OraclesRomInfo *info);
/* The language of a recognised Oracles Combined image, "English" to "Italian"; NULL for any other image. */
const char *oracles_rom_combined_language(const OraclesRomInfo *info);
/* One of the two original US ROMs, not another authenticated image of their family (a fan game's). */
int oracles_rom_is_original(const OraclesRomInfo *info);

#ifdef __cplusplus
}
#endif

#endif
