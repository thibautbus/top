/* Room layout decoding: loadRoomLayout and its helpers in code/bank0.s.
 * Formats: docs/ROM_DATA_FORMATS.md, section 2. */
#include "oracles_rom.h"

/* Small rooms are written with a stride of 16: after column 9 the cursor
 * jumps to the next row (@checkDeNextLayoutRow). */
static void small_put(uint8_t out[ORACLES_ROOM_LAYOUT_BYTES], unsigned *pos, uint8_t value)
{
    if (*pos < ORACLES_ROOM_LAYOUT_BYTES) out[*pos] = value;
    (*pos)++;
    if ((*pos & 0x0fu) >= ORACLES_SMALL_ROOM_WIDTH) *pos += 16u - ORACLES_SMALL_ROOM_WIDTH;
}

/* Modes 1 and 2 of compressData_commonByte: a key of `key_bytes` bytes, then
 * the common byte if the key is non-zero, then the differing bytes.  Bit i of
 * the key, least significant first, marks byte i of the block as common. */
static int small_common_byte(const OraclesRom *rom, size_t *src, unsigned key_bytes,
                             uint8_t out[ORACLES_ROOM_LAYOUT_BYTES])
{
    const unsigned block = 8u * key_bytes;
    unsigned pos = 0;
    for (unsigned b = 0; b < (ORACLES_SMALL_ROOM_WIDTH * ORACLES_SMALL_ROOM_HEIGHT) / block; b++) {
        unsigned key = 0;
        for (unsigned k = 0; k < key_bytes; k++) {
            uint8_t byte;
            int rc = oracles_rom_read8(rom, (*src)++, &byte);
            if (rc) return rc;
            key |= (unsigned)byte << (8u * k);
        }
        uint8_t common = 0;
        if (key) {
            int rc = oracles_rom_read8(rom, (*src)++, &common);
            if (rc) return rc;
        }
        for (unsigned i = 0; i < block; i++) {
            uint8_t value = common;
            if (!((key >> i) & 1u)) {
                int rc = oracles_rom_read8(rom, (*src)++, &value);
                if (rc) return rc;
            }
            small_put(out, &pos, value);
        }
    }
    return ORACLES_OK;
}

static int decode_small(const OraclesRom *rom, size_t table, size_t base, unsigned room,
                        uint8_t out[ORACLES_ROOM_LAYOUT_BYTES])
{
    unsigned word;
    int rc = oracles_rom_read16le(rom, table + room * 2u, &word);
    if (rc) return rc;
    size_t src = base + (word & 0x3fffu);
    const unsigned mode = word >> 14;
    if (mode == 0) {
        unsigned pos = 0;
        for (unsigned i = 0; i < ORACLES_SMALL_ROOM_WIDTH * ORACLES_SMALL_ROOM_HEIGHT; i++) {
            uint8_t value;
            rc = oracles_rom_read8(rom, src++, &value);
            if (rc) return rc;
            small_put(out, &pos, value);
        }
        return ORACLES_OK;
    }
    if (mode == 1) return small_common_byte(rom, &src, 1, out);
    if (mode == 2) return small_common_byte(rom, &src, 2, out);
    return ORACLES_ERR_FORMAT;
}

/* Large rooms: a 4 KiB dictionary precedes the per-room words at `table`; the
 * per-room word is an offset from `base` plus 0x200.  Flag bytes are read least
 * significant bit first; a set bit is a 2-byte reference: bits 0-11 offset in
 * the dictionary, bits 12-15 length minus 3.  Output stops at 176 bytes. */
static int decode_large(const OraclesRom *rom, size_t table, size_t base, unsigned room,
                        uint8_t out[ORACLES_ROOM_LAYOUT_BYTES])
{
    unsigned word;
    int rc = oracles_rom_read16le(rom, table + 0x1000u + room * 2u, &word);
    if (rc) return rc;
    if (word < 0x200u) return ORACLES_ERR_FORMAT;
    size_t src = base + word - 0x200u;
    unsigned pos = 0;
    while (pos < ORACLES_ROOM_LAYOUT_BYTES) {
        uint8_t flags;
        rc = oracles_rom_read8(rom, src++, &flags);
        if (rc) return rc;
        for (unsigned bit = 0; bit < 8 && pos < ORACLES_ROOM_LAYOUT_BYTES; bit++) {
            if (!((flags >> bit) & 1u)) {
                rc = oracles_rom_read8(rom, src++, &out[pos++]);
                if (rc) return rc;
                continue;
            }
            uint8_t lo, hi;
            rc = oracles_rom_read8(rom, src++, &lo);
            if (rc) return rc;
            rc = oracles_rom_read8(rom, src++, &hi);
            if (rc) return rc;
            const unsigned offset = ((unsigned)(hi & 0x0fu) << 8) | lo;
            const unsigned length = (unsigned)(hi >> 4) + 3u;
            for (unsigned i = 0; i < length && pos < ORACLES_ROOM_LAYOUT_BYTES; i++) {
                rc = oracles_rom_read8(rom, table + offset + i, &out[pos++]);
                if (rc) return rc;
            }
        }
    }
    return ORACLES_OK;
}

int oracles_decode_room_layout(const OraclesRom *rom, const OraclesTables *t,
                               unsigned group, unsigned room,
                               uint8_t out[ORACLES_ROOM_LAYOUT_BYTES], int *is_large)
{
    if (group > 7 || room > 255) return ORACLES_ERR_FORMAT;
    const size_t entry = oracles_rom_offset(t->room_layout_group_table.bank,
                                            t->room_layout_group_table.addr) + group * 8u;
    uint8_t mode, table_bank, base_bank;
    unsigned table_addr, base_addr;
    int rc;
    if ((rc = oracles_rom_read8(rom, entry, &mode))) return rc;
    if ((rc = oracles_rom_read8(rom, entry + 1, &table_bank))) return rc;
    if ((rc = oracles_rom_read16le(rom, entry + 2, &table_addr))) return rc;
    if ((rc = oracles_rom_read8(rom, entry + 4, &base_bank))) return rc;
    if ((rc = oracles_rom_read16le(rom, entry + 5, &base_addr))) return rc;
    const size_t table = oracles_rom_offset(table_bank, table_addr);
    const size_t base = oracles_rom_offset(base_bank, base_addr);
    if (mode == 1) {
        if (is_large) *is_large = 0;
        return decode_small(rom, table, base, room, out);
    }
    if (mode == 0) {
        if (is_large) *is_large = 1;
        return decode_large(rom, table, base, room, out);
    }
    return ORACLES_ERR_FORMAT;
}
