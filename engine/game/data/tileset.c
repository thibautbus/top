/* Tileset definitions and layouts: loadTilesetData_body, loadTileset,
 * loadTilesetHlpr and loadTilesetLayout in code/bank0.s.
 * Formats: docs/ROM_DATA_FORMATS.md, section 3. */
#include "oracles_rom.h"

#include <string.h>

/* WRAM bank 3 destinations named in the tileset layout headers. */
#define W3_TILE_COLLISIONS 0xdb00u
#define W3_TILE_MAPPING_INDICES 0xdc00u

int oracles_read_tileset(const OraclesRom *rom, const OraclesTables *t, unsigned index,
                         unsigned season, OraclesTilesetEntry *out)
{
    size_t entry = oracles_rom_offset(t->tileset_data.bank, t->tileset_data.addr) + index * 8u;
    uint8_t bytes[8];
    int rc;
    for (unsigned i = 0; i < 8; i++)
        if ((rc = oracles_rom_read8(rom, entry + i, &bytes[i]))) return rc;
    if (bytes[0] == 0xffu) {
        /* Seasons: m_SeasonalTileset = $ff, pointer to 4 x 8 bytes, 5 zero bytes. */
        const unsigned pointer = (unsigned)bytes[1] | ((unsigned)bytes[2] << 8);
        entry = oracles_rom_offset(t->tileset_data.bank, pointer) + (season & 3u) * 8u;
        for (unsigned i = 0; i < 8; i++)
            if ((rc = oracles_rom_read8(rom, entry + i, &bytes[i]))) return rc;
    }
    out->collisions_mode = (uint8_t)(bytes[0] >> 4);
    out->dungeon_index = (uint8_t)(bytes[0] & 0x0fu);
    out->flags = bytes[1];
    out->unique_gfx = bytes[2];
    out->gfx = bytes[3];
    out->palette = bytes[4];
    out->layout = bytes[5];
    out->layout_group = bytes[6];
    out->animation = bytes[7];
    return ORACLES_OK;
}

/* LZ with a shared dictionary (loadTilesetHlpr).  Flag bits are consumed least
 * significant first.  A set bit is a reference into the dictionary: 2 bytes
 * (offset 12 bits, length = high nibble + 3) when the dictionary header's bit 7
 * is clear, 3 bytes (length, offset 16 bits) when it is set.  The output size
 * comes from the header and bounds every copy. */
static int lz_dictionary(const OraclesRom *rom, size_t src, size_t dictionary, int long_refs,
                         uint8_t *out, unsigned size)
{
    unsigned pos = 0;
    while (pos < size) {
        uint8_t flags;
        int rc = oracles_rom_read8(rom, src++, &flags);
        if (rc) return rc;
        for (unsigned bit = 0; bit < 8 && pos < size; bit++) {
            if (!((flags >> bit) & 1u)) {
                rc = oracles_rom_read8(rom, src++, &out[pos++]);
                if (rc) return rc;
                continue;
            }
            unsigned offset, length;
            if (!long_refs) {
                uint8_t lo, hi;
                if ((rc = oracles_rom_read8(rom, src++, &lo))) return rc;
                if ((rc = oracles_rom_read8(rom, src++, &hi))) return rc;
                offset = ((unsigned)(hi & 0x0fu) << 8) | lo;
                length = (unsigned)(hi >> 4) + 3u;
            } else {
                uint8_t len, lo, hi;
                if ((rc = oracles_rom_read8(rom, src++, &len))) return rc;
                if ((rc = oracles_rom_read8(rom, src++, &lo))) return rc;
                if ((rc = oracles_rom_read8(rom, src++, &hi))) return rc;
                length = len;
                offset = (unsigned)lo | ((unsigned)hi << 8);
            }
            for (unsigned i = 0; i < length && pos < size; i++) {
                rc = oracles_rom_read8(rom, dictionary + offset + i, &out[pos++]);
                if (rc) return rc;
            }
        }
    }
    return ORACLES_OK;
}

/* Expands 256 two-byte mapping-row indices into 8 bytes per metatile through
 * tileMappingTable (3 bytes per row: index-data offset low, high nibbles of
 * both offsets, attribute-data offset low), as loadTilesetLayout's helper does. */
static int expand_mappings(const OraclesRom *rom, const OraclesTables *t,
                           const uint8_t indices[512], uint8_t *mappings)
{
    const unsigned bank = t->tile_mapping_table.bank;
    unsigned index_base, attribute_base;
    int rc;
    if ((rc = oracles_rom_read16le(rom, oracles_rom_offset(bank, t->tile_mapping_index_data_pointer.addr), &index_base))) return rc;
    if ((rc = oracles_rom_read16le(rom, oracles_rom_offset(bank, t->tile_mapping_attribute_data_pointer.addr), &attribute_base))) return rc;
    const size_t table = oracles_rom_offset(bank, t->tile_mapping_table.addr);
    for (unsigned i = 0; i < 256; i++) {
        const unsigned row = (unsigned)indices[i * 2] | ((unsigned)indices[i * 2 + 1] << 8);
        uint8_t b0, b1, b2;
        if ((rc = oracles_rom_read8(rom, table + row * 3u, &b0))) return rc;
        if ((rc = oracles_rom_read8(rom, table + row * 3u + 1, &b1))) return rc;
        if ((rc = oracles_rom_read8(rom, table + row * 3u + 2, &b2))) return rc;
        const unsigned index_offset = (((unsigned)(b1 >> 4) & 0x0fu) << 8) | b0;
        const unsigned attribute_offset = ((unsigned)(b1 & 0x0fu) << 8) | b2;
        const size_t index_src = oracles_rom_offset(bank, index_base) + index_offset * 4u;
        const size_t attribute_src = oracles_rom_offset(bank, attribute_base) + attribute_offset * 4u;
        for (unsigned k = 0; k < 4; k++) {
            if ((rc = oracles_rom_read8(rom, index_src + k, &mappings[i * 8 + k]))) return rc;
            if ((rc = oracles_rom_read8(rom, attribute_src + k, &mappings[i * 8 + 4 + k]))) return rc;
        }
    }
    return ORACLES_OK;
}

int oracles_decode_tileset_layout(const OraclesRom *rom, const OraclesTables *t, unsigned layout,
                                  uint8_t *mappings, uint8_t *collisions,
                                  int *have_mappings, int *have_collisions)
{
    const unsigned bank = t->tileset_layout_table.bank;
    unsigned header;
    int rc = oracles_rom_read16le(rom, oracles_rom_offset(bank, t->tileset_layout_table.addr) + layout * 2u, &header);
    if (rc) return rc;
    if (have_mappings) *have_mappings = 0;
    if (have_collisions) *have_collisions = 0;
    size_t h = oracles_rom_offset(bank, header);
    for (;;) {
        uint8_t dict_index, src_bank, size_hi, size_lo;
        unsigned src_addr, dest;
        if ((rc = oracles_rom_read8(rom, h, &dict_index))) return rc;
        if ((rc = oracles_rom_read8(rom, h + 1, &src_bank))) return rc;
        if ((rc = oracles_rom_read16be(rom, h + 2, &src_addr))) return rc;
        if ((rc = oracles_rom_read16be(rom, h + 4, &dest))) return rc;
        if ((rc = oracles_rom_read8(rom, h + 6, &size_hi))) return rc;
        if ((rc = oracles_rom_read8(rom, h + 7, &size_lo))) return rc;
        const unsigned size = ((unsigned)(size_hi & 0x7fu) << 8) | size_lo;
        const unsigned dest_addr = dest & 0xfff0u;

        unsigned dict_entry;
        if ((rc = oracles_rom_read16le(rom, oracles_rom_offset(bank, t->tileset_layout_dictionary_table.addr) + dict_index * 2u, &dict_entry))) return rc;
        uint8_t dict_bank_mode;
        unsigned dict_addr;
        if ((rc = oracles_rom_read8(rom, oracles_rom_offset(bank, dict_entry), &dict_bank_mode))) return rc;
        if ((rc = oracles_rom_read16be(rom, oracles_rom_offset(bank, dict_entry) + 1, &dict_addr))) return rc;
        const size_t dictionary = oracles_rom_offset(dict_bank_mode & 0x3fu, dict_addr);
        const int long_refs = (dict_bank_mode & 0x80u) != 0;
        const size_t src = oracles_rom_offset(src_bank, src_addr);

        if (dest_addr == W3_TILE_MAPPING_INDICES) {
            if (size != 512u) return ORACLES_ERR_FORMAT;
            uint8_t indices[512];
            if ((rc = lz_dictionary(rom, src, dictionary, long_refs, indices, size))) return rc;
            if (mappings && (rc = expand_mappings(rom, t, indices, mappings))) return rc;
            if (have_mappings) *have_mappings = 1;
        } else if (dest_addr == W3_TILE_COLLISIONS) {
            if (size != ORACLES_TILESET_COLLISIONS_BYTES) return ORACLES_ERR_FORMAT;
            if (collisions && (rc = lz_dictionary(rom, src, dictionary, long_refs, collisions, size))) return rc;
            if (have_collisions) *have_collisions = 1;
        } else {
            return ORACLES_ERR_FORMAT;
        }
        if (!(size_hi & 0x80u)) break;
        h += 8;
    }
    return ORACLES_OK;
}
