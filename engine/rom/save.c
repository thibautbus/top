#include "save.h"

#include <string.h>

/* docs/ROM_DATA_FORMATS.md, section 14: a copy is $550 bytes; its checksum, the first two, is the sum of the $2a7
 * little-endian words after it; the game's string follows it; the language sits at +$7a. */
const unsigned oracles_save_copies[ORACLES_SAVE_COPIES] = { 0x0010, 0x0560, 0x0ab0, 0x1000, 0x1550, 0x1aa0 };
#define FILE_WORDS 0x2a7u
#define STRING_AT 2u
#define LANGUAGE_AT 0x7au

unsigned oracles_save_checksum(const uint8_t *sram, unsigned at)
{
    unsigned sum = 0;
    for (unsigned i = 0; i < FILE_WORDS; i++) sum += sram[at + 2u + 2u * i] | sram[at + 3u + 2u * i] << 8;
    return sum & 0xffffu;
}

int oracles_save_copy_valid(const uint8_t *sram, unsigned at, OraclesGame game)
{
    const char *string = game == ORACLES_GAME_AGES ? "Z21216-0" : game == ORACLES_GAME_SEASONS ? "Z11216-0" : NULL;
    return string && !memcmp(sram + at + STRING_AT, string, 8) && (unsigned)(sram[at] | sram[at + 1u] << 8) == oracles_save_checksum(sram, at);
}

void oracles_save_seal(uint8_t *sram, unsigned at)
{
    const unsigned sum = oracles_save_checksum(sram, at);
    sram[at] = (uint8_t)sum;
    sram[at + 1u] = (uint8_t)(sum >> 8);
}

unsigned oracles_save_valid(const uint8_t *sram, size_t size, OraclesGame game)
{
    unsigned valid = 0;
    for (unsigned i = 0; size == ORACLES_SAVE_SIZE && i < ORACLES_SAVE_COPIES; i++)
        if (oracles_save_copy_valid(sram, oracles_save_copies[i], game)) valid |= 1u << i;
    return valid;
}

unsigned oracles_save_combined_valid(const uint8_t *sram, size_t size)
{
    if (size != ORACLES_SAVE_COMBINED_SIZE) return 0;
    return oracles_save_valid(sram + 2u * ORACLES_SAVE_SIZE, ORACLES_SAVE_SIZE, ORACLES_GAME_AGES)
           | oracles_save_valid(sram + 3u * ORACLES_SAVE_SIZE, ORACLES_SAVE_SIZE, ORACLES_GAME_SEASONS) << ORACLES_SAVE_COPIES;
}

unsigned oracles_save_set_english(uint8_t *sram, size_t size, OraclesGame game)
{
    const unsigned valid = oracles_save_valid(sram, size, game);
    unsigned changed = 0;
    for (unsigned i = 0; i < ORACLES_SAVE_COPIES; i++) {
        const unsigned at = oracles_save_copies[i];
        if (!(valid & 1u << i) || !sram[at + LANGUAGE_AT]) continue;
        sram[at + LANGUAGE_AT] = 0;
        oracles_save_seal(sram, at);
        changed |= 1u << i;
    }
    return changed;
}
