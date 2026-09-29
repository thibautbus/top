/* ROM identification and SHA-1, without any ROM. */
#include "rom.h"
#include "sha1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

static void write_header(uint8_t *rom, const char *title)
{
    memset(rom + 0x134, 0, 16);
    memcpy(rom + 0x134, title, strlen(title));
    rom[0x143] = 0xc0;
    rom[0x147] = 0x1b;
    rom[0x148] = 0x05;
    rom[0x149] = 0x02;
}

int main(void)
{
    char hex[41];
    oracles_sha1_hex((const uint8_t *)"", 0, hex);
    CHECK(strcmp(hex, "da39a3ee5e6b4b0d3255bfef95601890afd80709") == 0);
    oracles_sha1_hex((const uint8_t *)"abc", 3, hex);
    CHECK(strcmp(hex, "a9993e364706816aba3e25717850c26c9cd0d89d") == 0);
    oracles_sha1_hex((const uint8_t *)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, hex);
    CHECK(strcmp(hex, "84983e441c3bd26ebaae4aa1f95129e5e54670f1") == 0);

    const size_t size = 1024u * 1024u;
    uint8_t *rom = calloc(size, 1);
    OraclesRomInfo info;
    char error[128];

    write_header(rom, "ZELDA NAYRU");
    CHECK(oracles_rom_identify(rom, size, &info, error, sizeof error) == 0);
    CHECK(info.game == ORACLES_GAME_AGES);
    CHECK(info.known == 0);
    CHECK(info.revision == ORACLES_ROM_REVISION_UNKNOWN);
    CHECK(strcmp(info.title, "ZELDA NAYRU") == 0);

    write_header(rom, "ZELDA DIN");
    CHECK(oracles_rom_identify(rom, size, &info, error, sizeof error) == 0);
    CHECK(info.game == ORACLES_GAME_SEASONS);
    CHECK(info.revision == ORACLES_ROM_REVISION_UNKNOWN);

    write_header(rom, "ZELDA DIN");
    CHECK(oracles_rom_identify(rom, size - 1, &info, error, sizeof error) == -1); /* not a power of two */
    CHECK(oracles_rom_identify(rom, size / 2, &info, error, sizeof error) == -1); /* too small */

    write_header(rom, "TETRIS");
    CHECK(oracles_rom_identify(rom, size, &info, error, sizeof error) == -1);

    write_header(rom, "ZELDA NAYRU");
    rom[0x147] = 0x03; /* MBC1 */
    CHECK(oracles_rom_identify(rom, size, &info, error, sizeof error) == -1);

    CHECK(oracles_rom_identify(rom, 0x100, &info, error, sizeof error) == -1);

    free(rom);
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_rom: ok\n");
    return 0;
}
