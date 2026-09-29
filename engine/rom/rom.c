#include "rom.h"
#include "sha1.h"
#include "../game/profiles/moonrise-regalia/identity.h"
#include "../game/profiles/temple-of-seasons/identity.h"
#include "../game/profiles/gifts-of-kinomi/identity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Authenticated images; each fan game's exact identity is generated from its manifest. */
static const struct {
    const char *sha1;
    OraclesGame game;
    OraclesRomRevision revision;
} known_roms[] = {
    { "880374fb978b18af4aa529e2e32f7ffb4d7dd2f4", ORACLES_GAME_AGES, ORACLES_ROM_REVISION_AGES_US },
    { "ba1268290fb2b1b70505d2d7b5825fc8a4816a4b", ORACLES_GAME_SEASONS, ORACLES_ROM_REVISION_SEASONS_US },
    { ORACLES_MOONRISE_SHA1, ORACLES_GAME_AGES, ORACLES_ROM_REVISION_MOONRISE_1_0_6 },
    { ORACLES_TEMPLE_SHA1, ORACLES_GAME_SEASONS, ORACLES_ROM_REVISION_TEMPLE_1_073 },
    { ORACLES_KINOMI_SHA1, ORACLES_GAME_AGES, ORACLES_ROM_REVISION_KINOMI_1_1_2 },
};

#define HEADER_TITLE 0x134u
#define HEADER_CGB_FLAG 0x143u
#define HEADER_CART_TYPE 0x147u
#define HEADER_ROM_SIZE 0x148u
#define HEADER_RAM_SIZE 0x149u
#define MIN_ROM_SIZE (1024u * 1024u)
#define MAX_ROM_SIZE (8u * 1024u * 1024u)

static void set_error(char *error, size_t capacity, const char *message)
{
    if (error && capacity) snprintf(error, capacity, "%s", message);
}

uint8_t *oracles_rom_read_file(const char *path, size_t *size, char *error, size_t error_capacity)
{
    FILE *f = fopen(path, "rb");
    if (!f) { set_error(error, error_capacity, "cannot open the ROM file"); return NULL; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); set_error(error, error_capacity, "cannot read the ROM file"); return NULL; }
    const long length = ftell(f);
    if (length <= 0 || (unsigned long)length > MAX_ROM_SIZE) { fclose(f); set_error(error, error_capacity, "the ROM file has an impossible size"); return NULL; }
    rewind(f);
    uint8_t *data = malloc((size_t)length);
    if (!data || fread(data, 1, (size_t)length, f) != (size_t)length) {
        fclose(f); free(data);
        set_error(error, error_capacity, "cannot read the ROM file");
        return NULL;
    }
    fclose(f);
    *size = (size_t)length;
    return data;
}

const char *oracles_game_name(OraclesGame game)
{
    switch (game) {
        case ORACLES_GAME_AGES: return "Oracle of Ages";
        case ORACLES_GAME_SEASONS: return "Oracle of Seasons";
        default: return "unknown";
    }
}

int oracles_rom_identify(const uint8_t *rom, size_t size, OraclesRomInfo *info, char *error, size_t error_capacity)
{
    memset(info, 0, sizeof *info);
    info->size = size;
    if (size < 0x150u) { set_error(error, error_capacity, "the file is too small to be a Game Boy ROM"); return -1; }
    memcpy(info->title, rom + HEADER_TITLE, 11);
    info->title[11] = 0;
    oracles_sha1_hex(rom, size, info->sha1);

    for (size_t i = 0; i < sizeof known_roms / sizeof known_roms[0]; i++) {
        if (strcmp(info->sha1, known_roms[i].sha1) == 0) {
            info->game = known_roms[i].game;
            info->revision = known_roms[i].revision;
            info->known = 1;
            return 0;
        }
    }

    if (strncmp(info->title, "ZELDA NAYRU", 11) == 0) info->game = ORACLES_GAME_AGES;
    else if (strncmp(info->title, "ZELDA DIN", 9) == 0) info->game = ORACLES_GAME_SEASONS;
    else { set_error(error, error_capacity, "the header title is not that of an Oracle game"); return -1; }
    if (!(rom[HEADER_CGB_FLAG] & 0x80u)) { set_error(error, error_capacity, "the header has no Game Boy Color flag"); return -1; }
    /* MBC5 with RAM and battery, with or without rumble; the RAM size is the core's business. */
    if (rom[HEADER_CART_TYPE] != 0x1bu && rom[HEADER_CART_TYPE] != 0x1eu) { set_error(error, error_capacity, "the cartridge type is not MBC5 with battery"); return -1; }
    if (size < MIN_ROM_SIZE || size > MAX_ROM_SIZE || (size & (size - 1)) != 0) {
        set_error(error, error_capacity, "the ROM size is not a power of two between 1 and 8 MiB");
        return -1;
    }
    (void)HEADER_ROM_SIZE; /* an extended image may declare a larger size; the file size is what counts */
    (void)HEADER_RAM_SIZE;
    return 0;
}

int oracles_rom_is_original(const OraclesRomInfo *info)
{
    return info && info->known
        && (info->revision == ORACLES_ROM_REVISION_AGES_US || info->revision == ORACLES_ROM_REVISION_SEASONS_US);
}
