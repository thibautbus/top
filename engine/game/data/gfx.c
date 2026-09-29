/* Graphics headers and decompression: loadGfxHeader and decompressGraphics in
 * code/bank0.s, cross-checked with tools/common.py of the disasm.
 * Formats: docs/ROM_DATA_FORMATS.md, sections 4 and 9. */
#include "oracles_rom.h"

int oracles_gfx_header_entry(const OraclesRom *rom, const OraclesTables *t,
                             unsigned header, unsigned entry, OraclesGfxEntry *out)
{
    const unsigned bank = t->gfx_header_table.bank;
    unsigned list;
    int rc = oracles_rom_read16le(rom, oracles_rom_offset(bank, t->gfx_header_table.addr) + header * 2u, &list);
    if (rc) return rc;
    const size_t e = oracles_rom_offset(bank, list) + entry * 6u;
    uint8_t b0, b5;
    unsigned src, dest;
    if ((rc = oracles_rom_read8(rom, e, &b0))) return rc;
    if ((rc = oracles_rom_read16be(rom, e + 1, &src))) return rc;
    if ((rc = oracles_rom_read16be(rom, e + 3, &dest))) return rc;
    if ((rc = oracles_rom_read8(rom, e + 5, &b5))) return rc;
    out->src_bank = b0 & 0x3fu;
    out->mode = b0 >> 6;
    out->src_addr = src;
    out->dest_addr = dest & 0xfff0u;
    out->dest_bank = dest & 0x000fu;
    out->size_byte = b5 & 0x7fu;
    out->has_next = (b5 & 0x80u) != 0;
    return ORACLES_OK;
}

typedef struct Writer {
    uint8_t *out;
    size_t capacity;
    size_t pos;
    int overflow;
} Writer;

static void put(Writer *w, uint8_t value)
{
    if (w->pos < w->capacity) w->out[w->pos] = value;
    else w->overflow = 1;
    w->pos++;
}

/* Mode 2: per 16-byte tile, a 16-bit key, the common byte, then the differing
 * bytes.  Key bits are consumed most significant bit first, low byte first. */
static int mode_common_byte(const OraclesRom *rom, size_t src, unsigned tiles, Writer *w)
{
    for (unsigned t = 0; t < tiles; t++) {
        uint8_t key_lo, key_hi, common = 0;
        int rc;
        if ((rc = oracles_rom_read8(rom, src++, &key_lo))) return rc;
        if ((rc = oracles_rom_read8(rom, src++, &key_hi))) return rc;
        if ((key_lo | key_hi) == 0) {
            for (unsigned i = 0; i < 16; i++) {
                uint8_t value;
                if ((rc = oracles_rom_read8(rom, src++, &value))) return rc;
                put(w, value);
            }
            continue;
        }
        if ((rc = oracles_rom_read8(rom, src++, &common))) return rc;
        const uint8_t keys[2] = { key_lo, key_hi };
        for (unsigned k = 0; k < 2; k++) {
            for (unsigned bit = 0; bit < 8; bit++) {
                uint8_t value = common;
                if (!((keys[k] >> (7u - bit)) & 1u)) {
                    if ((rc = oracles_rom_read8(rom, src++, &value))) return rc;
                }
                put(w, value);
            }
        }
    }
    return ORACLES_OK;
}

/* Modes 1 and 3: a flag byte then eight elements, most significant bit first.
 * A clear bit is a literal.  A set bit is a back-reference into the output:
 *   mode 1: one byte, bits 0-4 distance minus 1, bits 5-7 length minus 1, a
 *           following length byte when those bits are zero;
 *   mode 3: two bytes, bits 0-10 distance minus 1, bits 11-15 length minus 2,
 *           a following length byte when those bits are zero.
 * A length byte of zero means 256.  Bytes referenced before the start of the
 * output read as zero (the game reads whatever precedes the destination). */
static int mode_lz(const OraclesRom *rom, size_t src, unsigned total, int long_form, Writer *w)
{
    unsigned flags = 0, remaining_bits = 0;
    while (w->pos < total) {
        int rc;
        uint8_t byte;
        if (remaining_bits == 0) {
            if ((rc = oracles_rom_read8(rom, src++, &byte))) return rc;
            flags = byte;
            remaining_bits = 8;
        }
        const int reference = (flags & 0x80u) != 0;
        flags = (flags << 1) & 0xffu;
        remaining_bits--;
        if (!reference) {
            if ((rc = oracles_rom_read8(rom, src++, &byte))) return rc;
            put(w, byte);
            continue;
        }
        unsigned distance, length;
        if (!long_form) {
            if ((rc = oracles_rom_read8(rom, src, &byte))) return rc;
            distance = byte & 0x1fu;
            const unsigned high = byte & 0xe0u;
            if (high == 0) {
                src++;
                if ((rc = oracles_rom_read8(rom, src, &byte))) return rc;
                length = byte;
            } else {
                length = (high >> 5) + 1u;
            }
            src++;
        } else {
            uint8_t b1;
            if ((rc = oracles_rom_read8(rom, src++, &byte))) return rc;
            if ((rc = oracles_rom_read8(rom, src, &b1))) return rc;
            distance = (unsigned)byte | ((unsigned)(b1 & 0x07u) << 8);
            const unsigned high = b1 & 0xf8u;
            if (high == 0) {
                src++;
                if ((rc = oracles_rom_read8(rom, src, &b1))) return rc;
                length = b1;
            } else {
                length = (high >> 3) + 2u;
            }
            src++;
        }
        if (length == 0) length = 256;
        /* The game computes destination + ~distance, i.e. pos - distance - 1. */
        long from = (long)w->pos - (long)distance - 1;
        for (unsigned i = 0; i < length; i++, from++) {
            uint8_t value = 0;
            if (from >= 0 && (size_t)from < w->pos && (size_t)from < w->capacity) value = w->out[from];
            put(w, value);
        }
    }
    return ORACLES_OK;
}

long oracles_decompress_gfx(const OraclesRom *rom, unsigned src_bank, unsigned src_addr,
                            unsigned mode, unsigned size_byte, uint8_t *out, size_t capacity)
{
    Writer w = { out, capacity, 0, 0 };
    const size_t src = oracles_rom_offset(src_bank, src_addr);
    const unsigned blocks = (size_byte & 0x7fu) + 1u;
    int rc;
    switch (mode) {
        case 0:
            for (unsigned i = 0; i < blocks * 16u; i++) {
                uint8_t value;
                if ((rc = oracles_rom_read8(rom, src + i, &value))) return rc;
                put(&w, value);
            }
            break;
        case 1: rc = mode_lz(rom, src, blocks * 16u, 0, &w); if (rc) return rc; break;
        case 2: rc = mode_common_byte(rom, src, blocks, &w); if (rc) return rc; break;
        case 3: rc = mode_lz(rom, src, blocks * 16u, 1, &w); if (rc) return rc; break;
        default: return ORACLES_ERR_FORMAT;
    }
    if (w.overflow) return ORACLES_ERR_CAPACITY;
    return (long)w.pos;
}
