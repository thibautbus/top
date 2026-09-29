#include "oracles_rom.h"

int oracles_rom_read8(const OraclesRom *rom, size_t offset, uint8_t *out)
{
    if (offset >= rom->size) return ORACLES_ERR_RANGE;
    *out = rom->data[offset];
    return ORACLES_OK;
}

int oracles_rom_read16le(const OraclesRom *rom, size_t offset, unsigned *out)
{
    if (offset + 1 >= rom->size) return ORACLES_ERR_RANGE;
    *out = (unsigned)rom->data[offset] | ((unsigned)rom->data[offset + 1] << 8);
    return ORACLES_OK;
}

int oracles_rom_read16be(const OraclesRom *rom, size_t offset, unsigned *out)
{
    if (offset + 1 >= rom->size) return ORACLES_ERR_RANGE;
    *out = ((unsigned)rom->data[offset] << 8) | (unsigned)rom->data[offset + 1];
    return ORACLES_OK;
}

int oracles_room_tileset(const OraclesRom *rom, const OraclesTables *t,
                         unsigned group, unsigned room, uint8_t *tileset)
{
    unsigned table;
    if (group > 7 || room > 255) return ORACLES_ERR_FORMAT;
    const size_t base = oracles_rom_offset(t->room_tilesets_group_table.bank,
                                           t->room_tilesets_group_table.addr);
    int rc = oracles_rom_read16le(rom, base + group * 2u, &table);
    if (rc) return rc;
    return oracles_rom_read8(rom, oracles_rom_offset(t->room_tilesets_group_table.bank, table) + room, tileset);
}
