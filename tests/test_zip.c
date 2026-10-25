/* The zip reader on archives built here: stored and deflated files (stored, fixed and dynamic blocks, alone and in one
 * stream, a large one among them), damaged streams and code headers, the file each reader asks for (a ROM or a patch)
 * chosen among others, whether a zip holds a patch, the refusals, and the ROM loader and the patch reader reading
 * through it. */
#include "bps.h"
#include "rom.h"
#include "zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

/* Raw deflate streams, made once with Python's zlib.compressobj(level, zlib.DEFLATED, -15): of `text` and of
 * content(512) at level 9, of content(40) at level 0. */
static const char text[] = "Oracle of Ages, Oracle of Ages, Oracle of Seasons";
static const uint8_t fixed_stream[] = {   /* one block of the fixed code */
    0xf3, 0x2f, 0x4a, 0x4c, 0xce, 0x49, 0x55, 0xc8, 0x4f, 0x53, 0x70, 0x4c, 0x4f, 0x2d, 0xd6, 0x51, 0xf0, 0xc7, 0xc9, 0x0f,
    0x4e, 0x4d, 0x2c, 0xce, 0xcf, 0x2b, 0x06, 0x00 };
static const uint8_t dynamic_stream[] = {   /* one block of its own code */
    0x25, 0x91, 0x01, 0x12, 0x84, 0x30, 0x0c, 0x02, 0xdf, 0x1a, 0x48, 0x42, 0xfe, 0xff, 0x82, 0xdb, 0x7a, 0xe3, 0xe8, 0x68,
    0x1b, 0x61, 0xa1, 0x89, 0xba, 0xdb, 0x3d, 0xd9, 0x9b, 0xd1, 0x71, 0x25, 0xbe, 0xbb, 0x9c, 0x5c, 0x23, 0x87, 0xdd, 0x55,
    0xcd, 0x2c, 0x6b, 0xdb, 0x36, 0xef, 0xed, 0xb0, 0xad, 0x37, 0x95, 0xae, 0x5d, 0x3e, 0x3d, 0xc9, 0x68, 0x73, 0x51, 0x55,
    0x9f, 0x6a, 0x6b, 0x35, 0xbd, 0xee, 0x92, 0x51, 0xc8, 0x99, 0xb1, 0xf7, 0xa1, 0x8c, 0x4b, 0xe3, 0xac, 0x82, 0x00, 0xff,
    0xe0, 0xb3, 0x35, 0x37, 0xea, 0xb7, 0x54, 0x20, 0xf4, 0xf1, 0x8a, 0xf0, 0x6e, 0xf7, 0xe2, 0x95, 0xd4, 0xc4, 0x90, 0x62,
    0x7e, 0xed, 0x2a, 0x3d, 0x8b, 0x73, 0x6f, 0x81, 0xb1, 0xde, 0x62, 0x44, 0x3d, 0x87, 0x69, 0xdc, 0xa9, 0x42, 0x9a, 0x48,
    0x28, 0xf7, 0xa3, 0x19, 0xbf, 0x9f, 0x80, 0x20, 0xc4, 0xb0, 0xd6, 0xd8, 0xf7, 0x3c, 0x3b, 0x03, 0xb8, 0x4d, 0x66, 0x0d,
    0x89, 0x08, 0x06, 0xf1, 0x94, 0x10, 0xa3, 0x0f, 0x13, 0x11, 0xba, 0xd5, 0xfa, 0xbe, 0x0c, 0x0b, 0x07, 0xe3, 0x43, 0xb2,
    0x03, 0x64, 0xbf, 0xde, 0x24, 0x22, 0x1f, 0x26, 0xd0, 0x16, 0x8b, 0xf3, 0x20, 0x43, 0xea, 0x7b, 0x75, 0x3d, 0xb9, 0x90,
    0x79, 0xed, 0xd9, 0x3c, 0x0b, 0xa1, 0x45, 0x57, 0xb4, 0xc0, 0x2c, 0xb7, 0x1d, 0xa7, 0xf6, 0xb5, 0x07, 0x30, 0xc2, 0x4d,
    0xea, 0x57, 0x2b, 0xa9, 0x95, 0x6a, 0xfa, 0x40, 0x81, 0x9c, 0xef, 0xc1, 0xce, 0x77, 0x10, 0x78, 0xad, 0xee, 0xa9, 0x8c,
    0x8f, 0xb4, 0x24, 0x78, 0x69, 0x8e, 0x0e, 0xc0, 0x86, 0x50, 0x57, 0xd4, 0xf2, 0xaa, 0xfe, 0x1f, 0x1a, 0xc7, 0x03, 0x1e,
    0xb8, 0x2d, 0x0e, 0xe6, 0x0a, 0x54, 0x1a, 0xd1, 0x0f };
/* Of text, noise(300), content(512) and text again, at level 9 with a Z_SYNC_FLUSH after the first two: a fixed block,
 * an empty stored block from its last bit (mid-byte), a stored block, an empty one, then a dynamic block whose copies
 * reach back into the first. */
static const uint8_t multi_stream[] = {
    0xf2, 0x2f, 0x4a, 0x4c, 0xce, 0x49, 0x55, 0xc8, 0x4f, 0x53, 0x70, 0x4c, 0x4f, 0x2d, 0xd6, 0x51, 0xf0, 0xc7, 0xc9, 0x0f,
    0x4e, 0x4d, 0x2c, 0xce, 0xcf, 0x2b, 0x06, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x2c, 0x01, 0xd3, 0xfe, 0x48, 0xa1, 0x29,
    0x7c, 0xa7, 0xe5, 0x46, 0xdc, 0xff, 0xcc, 0x76, 0x52, 0x58, 0xd3, 0x2b, 0x42, 0x70, 0x83, 0x7d, 0x5f, 0x4e, 0x61, 0x27,
    0x5a, 0xe8, 0xa2, 0x68, 0x16, 0x09, 0x1e, 0xcc, 0x1c, 0x95, 0x7d, 0xe7, 0xd7, 0xf1, 0x98, 0xa2, 0xc6, 0x1b, 0x8c, 0x81,
    0xa5, 0xc0, 0xfc, 0x17, 0x4e, 0x5a, 0x7e, 0x95, 0xc6, 0x21, 0x27, 0x13, 0xb9, 0x63, 0xc5, 0x0e, 0x59, 0x7f, 0x24, 0x42,
    0x67, 0xa0, 0x88, 0xc3, 0xd6, 0xfe, 0x1b, 0x10, 0x50, 0x26, 0x10, 0x86, 0x88, 0xac, 0xb3, 0x52, 0x2e, 0x3c, 0x9d, 0xea,
    0xe9, 0xe0, 0xdd, 0x9d, 0x9b, 0x11, 0xac, 0xb3, 0xfa, 0xc4, 0x19, 0x51, 0x1a, 0xa0, 0xb3, 0xf5, 0x86, 0x11, 0x23, 0x54,
    0x11, 0xca, 0x42, 0xe9, 0x56, 0x05, 0x10, 0x7a, 0xb9, 0x93, 0xa1, 0x6c, 0x50, 0x75, 0xe2, 0x9e, 0xf8, 0xa0, 0xd2, 0x32,
    0x92, 0x24, 0x85, 0x6d, 0x29, 0xd8, 0x0f, 0xa6, 0x6f, 0x3c, 0x3d, 0xde, 0xdd, 0x3b, 0xa9, 0x7d, 0x66, 0xfc, 0x8b, 0x5b,
    0x81, 0xe6, 0x67, 0xf8, 0x03, 0x95, 0x74, 0x9e, 0x00, 0x4c, 0x4b, 0xd2, 0xb7, 0x3a, 0x4b, 0x37, 0x46, 0x95, 0xc7, 0xe6,
    0x95, 0x5a, 0xd6, 0xc8, 0xc4, 0x3b, 0x67, 0x7e, 0x0b, 0x11, 0xf4, 0xea, 0xd6, 0xcb, 0xea, 0x54, 0x84, 0xc2, 0xad, 0xcd,
    0x21, 0xdc, 0xc6, 0x46, 0xf9, 0xe9, 0xab, 0x7c, 0xdc, 0x32, 0x22, 0xb4, 0x78, 0x12, 0x33, 0xdb, 0x11, 0x18, 0x3b, 0x95,
    0x96, 0x0e, 0x7b, 0x4b, 0xc1, 0xe2, 0x3f, 0x36, 0xd1, 0x4a, 0x7f, 0x09, 0x04, 0xa4, 0x91, 0xad, 0xe6, 0x62, 0x42, 0x34,
    0x16, 0x16, 0x84, 0xf9, 0x15, 0xd9, 0x78, 0x8a, 0x4d, 0xab, 0x7f, 0xd7, 0x50, 0x0a, 0xa8, 0xcb, 0x0a, 0xd7, 0x96, 0x3d,
    0x63, 0x47, 0xbe, 0xdb, 0x91, 0x83, 0x94, 0x92, 0x07, 0x22, 0x08, 0x84, 0xd6, 0xb2, 0x6c, 0x8d, 0xe3, 0xee, 0xa3, 0xf5,
    0x3a, 0x59, 0x29, 0xc5, 0x81, 0x69, 0x62, 0xe6, 0x32, 0x62, 0x3f, 0xf5, 0x32, 0x9f, 0x16, 0x79, 0xe7, 0xe4, 0x53, 0xed,
    0xe1, 0xf3, 0x4f, 0xbc, 0xbf, 0x85, 0x77, 0x30, 0xd7, 0x5d, 0x91, 0xe7, 0xda, 0x13, 0xcd, 0x4d, 0x6d, 0x00, 0x00, 0x00,
    0xff, 0xff, 0x95, 0x93, 0x81, 0x8d, 0x80, 0x30, 0x0c, 0x03, 0x47, 0xfc, 0x19, 0x62, 0x27, 0x71, 0xf6, 0x9f, 0xe0, 0xaf,
    0xfc, 0x04, 0x2f, 0x04, 0x2a, 0x6d, 0xb1, 0x7d, 0xae, 0x48, 0xd4, 0xdd, 0xee, 0xc9, 0xde, 0x8c, 0x8e, 0x2b, 0xf1, 0xdd,
    0xe5, 0xe4, 0x1a, 0x39, 0xac, 0xae, 0x6a, 0x66, 0x99, 0xdb, 0xb6, 0x19, 0xb7, 0xc3, 0xb2, 0xde, 0xae, 0x74, 0xed, 0xf2,
    0xea, 0x49, 0x46, 0x9b, 0x8b, 0xaa, 0xfa, 0x54, 0x5b, 0xab, 0xe9, 0x75, 0x97, 0x8c, 0x42, 0xce, 0x6c, 0x7b, 0x2f, 0xca,
    0xb8, 0x34, 0xce, 0x2a, 0x08, 0xf0, 0x0d, 0x3e, 0x5b, 0x73, 0xa3, 0x7e, 0x53, 0x45, 0x84, 0x3e, 0x86, 0x08, 0xef, 0x76,
    0x2f, 0x5e, 0x49, 0x4d, 0x4c, 0x52, 0xcc, 0xaf, 0x5d, 0xa5, 0x67, 0x71, 0xee, 0x2d, 0x62, 0xac, 0xb7, 0xee, 0x81, 0xcc,
    0x61, 0x1a, 0x77, 0xaa, 0x90, 0x06, 0x09, 0xe5, 0x7e, 0x69, 0xc6, 0xef, 0x23, 0x42, 0x00, 0x31, 0xcc, 0x35, 0xf6, 0x3d,
    0xcf, 0xce, 0x04, 0xdc, 0x86, 0x59, 0x03, 0x11, 0x60, 0x24, 0x9e, 0x12, 0x62, 0xf4, 0x61, 0x10, 0x49, 0xb7, 0x5a, 0xdf,
    0xc7, 0xb0, 0xe4, 0x60, 0xfb, 0x40, 0x76, 0x04, 0xd9, 0xaf, 0x37, 0x09, 0xe4, 0xc3, 0x84, 0xb4, 0xc5, 0xe4, 0xbc, 0x90,
    0x81, 0xfa, 0x5e, 0x5d, 0x4f, 0x2e, 0x30, 0xaf, 0x3d, 0x9b, 0x67, 0x21, 0xb4, 0xe8, 0x8a, 0x16, 0xd8, 0xcb, 0x6d, 0xc7,
    0xa9, 0x7d, 0xed, 0x11, 0x18, 0xe1, 0x86, 0xfa, 0xd5, 0x0a, 0xb5, 0x52, 0x4d, 0x1f, 0x28, 0xc0, 0xf9, 0x1e, 0xac, 0x7c,
    0x07, 0x81, 0xd7, 0xea, 0x9e, 0xca, 0xf8, 0xa0, 0x85, 0xe0, 0xd1, 0x1c, 0x1d, 0x10, 0x9b, 0x84, 0xba, 0xa2, 0x96, 0x57,
    0xf5, 0xdf, 0xa1, 0x71, 0x3c, 0xc4, 0x23, 0x6e, 0x8b, 0x83, 0xb9, 0x22, 0x2a, 0x8d, 0xe8, 0xe7, 0xbf, 0x3f, 0xd7, 0x2f };
static const uint8_t stored_stream[] = {   /* one stored block */
    0x01, 0x28, 0x00, 0xd7, 0xff, 0x67, 0x67, 0x62, 0x64, 0x64, 0x64, 0x63, 0x64, 0x65, 0x67, 0x66, 0x68, 0x65, 0x65, 0x62,
    0x68, 0x62, 0x68, 0x62, 0x67, 0x67, 0x63, 0x68, 0x68, 0x68, 0x67, 0x68, 0x62, 0x63, 0x61, 0x65, 0x62, 0x63, 0x67, 0x64,
    0x64, 0x63, 0x66, 0x62, 0x61 };

/* Bytes of every value from a linear congruential generator, as that Python draws them. */
static void noise(uint8_t *out, size_t n)
{
    uint32_t x = 5;
    for (size_t i = 0; i < n; i++) {
        x = x * 1103515245u + 12345u;
        out[i] = (uint8_t)(x >> 24);
    }
}

/* Eight letters from a linear congruential generator, as the Python that made the streams draws them. */
static void content(uint8_t *out, size_t n)
{
    uint32_t x = 1;
    for (size_t i = 0; i < n; i++) {
        x = (x * 1103515245u + 12345u) & 0x7fffffffu;
        out[i] = (uint8_t)('a' + (x >> 16) % 8u);
    }
}

typedef struct entry {
    const char *name;
    unsigned method, flags;
    const uint8_t *packed;
    size_t packed_size, size;
    uint32_t crc;
} entry;

typedef struct archive { uint8_t data[(1u << 20) + 4096u]; size_t size; } archive;

static void bytes(archive *a, const void *p, size_t n) { memcpy(a->data + a->size, p, n); a->size += n; }
static void u16(archive *a, unsigned v) { const uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) }; bytes(a, b, 2); }
static void u32(archive *a, uint32_t v) { u16(a, v & 0xffffu); u16(a, v >> 16); }

/* A zip of `n` entries, as a zip tool writes one: the local headers and data, the directory, its end.  With bit 3 (8)
 * of the flags, the local header's sizes and CRC are zero, as for a stream written without seeking. */
static void zip(archive *a, const entry *e, size_t n)
{
    uint32_t offsets[5];
    a->size = 0;
    for (size_t i = 0; i < n; i++) {
        const int later = (e[i].flags & 8u) != 0;
        offsets[i] = (uint32_t)a->size;
        u32(a, 0x04034b50u); u16(a, 20); u16(a, e[i].flags); u16(a, e[i].method); u16(a, 0); u16(a, 0);
        u32(a, later ? 0 : e[i].crc); u32(a, later ? 0 : (uint32_t)e[i].packed_size); u32(a, later ? 0 : (uint32_t)e[i].size);
        u16(a, (unsigned)strlen(e[i].name)); u16(a, 4);
        bytes(a, e[i].name, strlen(e[i].name)); u32(a, 0);   /* an extra field the reader steps over */
        bytes(a, e[i].packed, e[i].packed_size);
        if (later) { u32(a, 0x08074b50u); u32(a, e[i].crc); u32(a, (uint32_t)e[i].packed_size); u32(a, (uint32_t)e[i].size); }
    }
    const size_t directory = a->size;
    for (size_t i = 0; i < n; i++) {
        u32(a, 0x02014b50u); u16(a, 20); u16(a, 20); u16(a, e[i].flags); u16(a, e[i].method); u16(a, 0); u16(a, 0);
        u32(a, e[i].crc); u32(a, (uint32_t)e[i].packed_size); u32(a, (uint32_t)e[i].size);
        u16(a, (unsigned)strlen(e[i].name)); u16(a, 0); u16(a, 0); u16(a, 0); u16(a, 0); u32(a, 0); u32(a, offsets[i]);
        bytes(a, e[i].name, strlen(e[i].name));
    }
    const size_t directory_size = a->size - directory;
    u32(a, 0x06054b50u); u16(a, 0); u16(a, 0); u16(a, (unsigned)n); u16(a, (unsigned)n);
    u32(a, (uint32_t)directory_size); u32(a, (uint32_t)directory);
    u16(a, 7); bytes(a, "comment", 7);
}

/* A deflate stream written bit by bit, the first bit of each byte its lowest. */
typedef struct writer { uint8_t *data; size_t size; unsigned bit; } writer;

static void put(writer *w, unsigned value, unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        if (w->bit == 0) w->data[w->size++] = 0;
        w->data[w->size - 1] |= (uint8_t)((value >> i & 1u) << w->bit);
        w->bit = (w->bit + 1u) & 7u;
    }
}

/* A Huffman code, its highest bit first. */
static void code(writer *w, unsigned value, unsigned length)
{
    while (length--) put(w, value >> length & 1u, 1);
}

/* A symbol of the fixed literal and length code (RFC 1951, 3.2.6). */
static void fixed_symbol(writer *w, unsigned symbol)
{
    if (symbol < 144u) code(w, 0x30u + symbol, 8);
    else if (symbol < 256u) code(w, 0x190u + symbol - 144u, 9);
    else if (symbol < 280u) code(w, symbol - 256u, 7);
    else code(w, 0xc0u + symbol - 280u, 8);
}

/* A stored block of `n` bytes, from the next byte. */
static void stored_bytes(writer *w, int last, const uint8_t *bytes, size_t n)
{
    put(w, (unsigned)last, 1);
    put(w, 0, 2);
    w->bit = 0;
    const uint8_t header[4] = { (uint8_t)n, (uint8_t)(n >> 8), (uint8_t)~n, (uint8_t)(~n >> 8) };
    memcpy(w->data + w->size, header, 4);
    memcpy(w->data + w->size + 4, bytes, n);
    w->size += 4u + n;
}

/* A stream of the `size` bytes it writes to `out`: fixed blocks of literals and runs (a copy at distance 1, from the
 * block before at a block's start), each followed by a stored block from where it ends, the last one final. */
static void large_stream(writer *w, uint8_t *out, size_t size)
{
    uint32_t x = 7;
    size_t n = 0;
    for (;;) {
        put(w, 0, 1);
        put(w, 1, 2);
        const size_t end = size - n > 60000u ? n + 60000u : size;
        while (n < end) {
            x = x * 1103515245u + 12345u;
            if (n > 0 && end - n >= 258u && (x >> 16) % 4u == 0) {
                fixed_symbol(w, 285);   /* 258 bytes */
                code(w, 0, 5);          /* at distance 1 */
                memset(out + n, out[n - 1], 258);
                n += 258;
            } else {
                out[n] = (uint8_t)(x >> 24);
                fixed_symbol(w, out[n++]);
            }
        }
        fixed_symbol(w, 256);
        const size_t raw = size - n < 1000u ? size - n : 1000u;
        for (size_t i = 0; i < raw; i++) { x = x * 1103515245u + 12345u; out[n + i] = (uint8_t)(x >> 24); }
        stored_bytes(w, n + raw == size, out + n, raw);
        n += raw;
        if (n == size) return;
    }
}

/* A dynamic block's header (literals 257, distances 1) and its code-length code: `count` lengths of 3 bits, in the
 * format's order. */
static void dynamic_header(writer *w, unsigned literals, const uint8_t *code_lengths, unsigned count)
{
    put(w, 1, 1);
    put(w, 2, 2);
    put(w, literals - 257u, 5);
    put(w, 0, 5);
    put(w, count - 4u, 4);
    for (unsigned i = 0; i < count; i++) put(w, code_lengths[i], 3);
}

/* A dynamic block of `literals` literal lengths whose code is incomplete: 'A' of 1 bit (0), the end of 2 (10), the
 * code 11 left unused; it writes 'A', then the end, or with `unused` the code that is none. */
static void incomplete_block(writer *w, unsigned literals, int unused)
{
    static const uint8_t code_lengths[18] = { [3] = 1, [15] = 2, [17] = 2 };   /* 0: 0, 1: 10, 2: 11 */
    dynamic_header(w, literals, code_lengths, 18);
    for (unsigned i = 0; i < literals; i++) {
        if (i == 'A') code(w, 2, 2);
        else if (i == 256u) code(w, 3, 2);
        else code(w, 0, 1);
    }
    code(w, 2, 2);   /* the one distance, of 1 bit */
    code(w, 0, 1);
    code(w, unused ? 3u : 2u, 2);
    put(w, 0, 16);
}

/* The archive of one entry deflated as `w`, said to give `expected`. */
static void deflated(archive *a, const writer *w, const uint8_t *expected, size_t size)
{
    const entry e = { "Oracle of Ages.gbc", 8, 0, w->data, w->size, size, oracles_crc32(expected, size) };
    zip(a, &e, 1);
}

static int write_file(const char *path, const void *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    const int written = f && fwrite(data, 1, size, f) == size;
    if (f) fclose(f);
    return written;
}

/* The archive's one file of kind `kind` is `expected`. */
static int extracts(const archive *a, OraclesZipKind kind, const uint8_t *expected, size_t size)
{
    char error[160] = "";
    size_t out_size = 0;
    uint8_t *out = oracles_zip_extract(a->data, a->size, kind, 1u << 20, &out_size, error, sizeof error);
    const int same = out && out_size == size && memcmp(out, expected, size) == 0;
    if (!same) fprintf(stderr, "  extracted %zu bytes, expected %zu: %s\n", out_size, size, error);
    free(out);
    return same;
}

/* The archive is refused to a reader of kind `kind` with a reason that holds `reason`. */
static int refused(const archive *a, OraclesZipKind kind, const char *reason)
{
    char error[160] = "";
    size_t out_size = 0;
    uint8_t *out = oracles_zip_extract(a->data, a->size, kind, 1u << 20, &out_size, error, sizeof error);
    free(out);
    if (out || !strstr(error, reason)) fprintf(stderr, "  expected a refusal with \"%s\", got \"%s\"\n", reason, error);
    return !out && strstr(error, reason) != NULL;
}

int main(int argc, char **argv)
{
    static archive a;
    static uint8_t packed[1u << 20], large[1u << 20];
    uint8_t dynamic_text[512], stored_text[40], multi_text[910];
    content(dynamic_text, sizeof dynamic_text);
    content(stored_text, sizeof stored_text);
    const size_t text_size = sizeof text - 1u;
    memcpy(multi_text, text, text_size);
    noise(multi_text + text_size, 300);
    memcpy(multi_text + text_size + 300, dynamic_text, sizeof dynamic_text);
    memcpy(multi_text + text_size + 300 + sizeof dynamic_text, text, text_size);
    const entry fixed = { "Oracle of Ages.gbc", 8, 0, fixed_stream, sizeof fixed_stream, text_size, oracles_crc32((const uint8_t *)text, text_size) };
    const entry dynamic = { "roms/Oracle of Seasons.GB", 8, 0, dynamic_stream, sizeof dynamic_stream, sizeof dynamic_text, oracles_crc32(dynamic_text, sizeof dynamic_text) };
    const entry deflated_stored = { "Moonrise Regalia 1.0.6.bps", 8, 0, stored_stream, sizeof stored_stream, sizeof stored_text, oracles_crc32(stored_text, sizeof stored_text) };
    const entry stored = { "Oracle of Ages.gbc", 0, 0, stored_text, sizeof stored_text, sizeof stored_text, oracles_crc32(stored_text, sizeof stored_text) };
    const entry multi = { "Oracle of Ages.gbc", 8, 0, multi_stream, sizeof multi_stream, sizeof multi_text, oracles_crc32(multi_text, sizeof multi_text) };
    const entry readme = { "readme.txt", 0, 0, (const uint8_t *)"hi", 2, 2, oracles_crc32((const uint8_t *)"hi", 2) };
    const entry resource = { "__MACOSX/._Oracle of Ages.gbc", 0, 0, (const uint8_t *)"hi", 2, 2, oracles_crc32((const uint8_t *)"hi", 2) };
    const entry beside = { "roms/._Oracle of Seasons.GB", 0, 0, (const uint8_t *)"hi", 2, 2, oracles_crc32((const uint8_t *)"hi", 2) };
    const entry folder = { "roms/", 0, 0, (const uint8_t *)"", 0, 0, 0 };
    writer w = { packed, 0, 0 };

    /* Each kind of block, several in one stream, a stored file, a ROM among other files, sizes after the data. */
    zip(&a, &fixed, 1);
    CHECK(oracles_zip_is_zip(a.data, a.size));
    CHECK(extracts(&a, ORACLES_ZIP_ROM, (const uint8_t *)text, text_size));
    zip(&a, &dynamic, 1);
    CHECK(extracts(&a, ORACLES_ZIP_ROM, dynamic_text, sizeof dynamic_text));
    zip(&a, &deflated_stored, 1);
    CHECK(extracts(&a, ORACLES_ZIP_PATCH, stored_text, sizeof stored_text));
    zip(&a, &stored, 1);
    CHECK(extracts(&a, ORACLES_ZIP_ROM, stored_text, sizeof stored_text));
    zip(&a, &multi, 1);
    CHECK(extracts(&a, ORACLES_ZIP_ROM, multi_text, sizeof multi_text));
    const entry among[5] = { readme, folder, resource, dynamic, beside };
    zip(&a, among, 5);
    CHECK(extracts(&a, ORACLES_ZIP_ROM, dynamic_text, sizeof dynamic_text));
    entry later = dynamic;
    later.flags = 8;
    zip(&a, &later, 1);
    CHECK(extracts(&a, ORACLES_ZIP_ROM, dynamic_text, sizeof dynamic_text));

    /* A large stream, 1 MiB of fixed and stored blocks, the stored ones each from the middle of a byte. */
    large_stream(&w, large, sizeof large);
    deflated(&a, &w, large, sizeof large);
    CHECK(extracts(&a, ORACLES_ZIP_ROM, large, sizeof large));

    /* Damaged codes: a code-length code with more codes than its lengths hold, a repeat before any length, more literal
     * lengths than the format has, no end code; an incomplete code (which RFC 1951 lets a distance code be) read
     * through its codes, and refused at the one it does not have. */
    static const uint8_t ones[19] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    static const uint8_t repeat_first[4] = { 1, 0, 0, 1 };   /* 16: 1, 0: 0 */
    static const uint8_t eight_or_none[5] = { 0, 0, 0, 1, 1 };   /* 0: 0, 8: 1 */
    w = (writer){ packed, 0, 0 };
    dynamic_header(&w, 257, ones, 19);
    put(&w, 0, 32);
    deflated(&a, &w, (const uint8_t *)"A", 1);
    CHECK(refused(&a, ORACLES_ZIP_ROM, "does not decompress"));
    w = (writer){ packed, 0, 0 };
    dynamic_header(&w, 257, repeat_first, 4);
    code(&w, 1, 1);
    put(&w, 0, 32);
    deflated(&a, &w, (const uint8_t *)"A", 1);
    CHECK(refused(&a, ORACLES_ZIP_ROM, "does not decompress"));
    w = (writer){ packed, 0, 0 };
    incomplete_block(&w, 287, 0);
    deflated(&a, &w, (const uint8_t *)"A", 1);
    CHECK(refused(&a, ORACLES_ZIP_ROM, "does not decompress"));
    w = (writer){ packed, 0, 0 };
    dynamic_header(&w, 257, eight_or_none, 5);
    for (unsigned i = 0; i < 256u; i++) code(&w, 1, 1);   /* each byte of 8 bits */
    code(&w, 0, 1);   /* the end of none */
    code(&w, 0, 1);   /* no distance */
    put(&w, 0, 32);
    deflated(&a, &w, (const uint8_t *)"A", 1);
    CHECK(refused(&a, ORACLES_ZIP_ROM, "does not decompress"));
    w = (writer){ packed, 0, 0 };
    incomplete_block(&w, 257, 0);
    deflated(&a, &w, (const uint8_t *)"A", 1);
    CHECK(extracts(&a, ORACLES_ZIP_ROM, (const uint8_t *)"A", 1));
    w = (writer){ packed, 0, 0 };
    incomplete_block(&w, 257, 1);
    deflated(&a, &w, (const uint8_t *)"A", 1);
    CHECK(refused(&a, ORACLES_ZIP_ROM, "does not decompress"));

    /* Damaged fixed blocks: a copy from before the start, the lengths 286 and 287, the distances 30 and 31; then the
     * block they damage, 'A' and three copies of it. */
    for (unsigned bad_symbol = 0; bad_symbol < 6u; bad_symbol++) {
        w = (writer){ packed, 0, 0 };
        put(&w, 1, 1);
        put(&w, 1, 2);
        if (bad_symbol) fixed_symbol(&w, 'A');
        fixed_symbol(&w, bad_symbol == 1 ? 286u : bad_symbol == 2 ? 287u : 257u);
        code(&w, bad_symbol == 3 ? 30u : bad_symbol == 4 ? 31u : 0u, 5);
        fixed_symbol(&w, 256);
        put(&w, 0, 16);
        deflated(&a, &w, (const uint8_t *)"AAAA", bad_symbol ? 4 : 3);
        CHECK(bad_symbol == 5u ? extracts(&a, ORACLES_ZIP_ROM, (const uint8_t *)"AAAA", 4) : refused(&a, ORACLES_ZIP_ROM, "does not decompress"));
    }

    /* Each reader takes its own file: a zip of a patch holds a patch, one of a ROM or of a ROM and a patch a ROM, the
     * ROM reader taking the ROM and the patch reader the patch; one of two patches holds patches, which the patch
     * reader refuses; one of neither, or whose directory is lost, cannot say. */
    zip(&a, &deflated_stored, 1);
    CHECK(oracles_zip_holds_patch(a.data, a.size) == 1);
    CHECK(refused(&a, ORACLES_ZIP_ROM, "holds a patch (.bps), not a ROM"));
    zip(&a, &fixed, 1);
    CHECK(oracles_zip_holds_patch(a.data, a.size) == 0);
    CHECK(refused(&a, ORACLES_ZIP_PATCH, "holds a ROM, not a patch"));
    const entry both[2] = { fixed, deflated_stored };
    zip(&a, both, 2);
    CHECK(oracles_zip_holds_patch(a.data, a.size) == 0);
    CHECK(extracts(&a, ORACLES_ZIP_ROM, (const uint8_t *)text, text_size));
    CHECK(extracts(&a, ORACLES_ZIP_PATCH, stored_text, sizeof stored_text));
    entry second = deflated_stored;
    second.name = "Moonrise Regalia 1.0.6 (DE).BPS";
    const entry patches[2] = { deflated_stored, second };
    zip(&a, patches, 2);
    CHECK(oracles_zip_holds_patch(a.data, a.size) == 1);
    CHECK(refused(&a, ORACLES_ZIP_PATCH, "several patches"));
    const entry none[2] = { readme, resource };
    zip(&a, none, 2);
    CHECK(oracles_zip_holds_patch(a.data, a.size) == -1);
    CHECK(refused(&a, ORACLES_ZIP_ROM, "holds no ROM"));
    CHECK(refused(&a, ORACLES_ZIP_PATCH, "holds no patch"));
    zip(&a, &deflated_stored, 1);
    a.size -= 30;
    CHECK(oracles_zip_holds_patch(a.data, a.size) == -1);

    /* Two ROMs. */
    const entry two[2] = { fixed, dynamic };
    zip(&a, two, 2);
    CHECK(refused(&a, ORACLES_ZIP_ROM, "several ROMs"));

    /* A wrong checksum, encryption, another method, a stream cut short or of another size, a bad stored length, a file
     * too large for the reader or empty. */
    entry bad = fixed;
    bad.crc ^= 1u;
    zip(&a, &bad, 1);
    CHECK(refused(&a, ORACLES_ZIP_ROM, "checksum"));
    bad = fixed; bad.flags = 1;
    zip(&a, &bad, 1);
    CHECK(refused(&a, ORACLES_ZIP_ROM, "encrypted"));
    bad = fixed; bad.method = 14;
    zip(&a, &bad, 1);
    CHECK(refused(&a, ORACLES_ZIP_ROM, "deflate"));
    bad = dynamic; bad.packed_size = 100;
    zip(&a, &bad, 1);
    CHECK(refused(&a, ORACLES_ZIP_ROM, "does not decompress"));
    bad = dynamic; bad.size = 511;
    zip(&a, &bad, 1);
    CHECK(refused(&a, ORACLES_ZIP_ROM, "does not decompress"));
    uint8_t broken[sizeof stored_stream];
    memcpy(broken, stored_stream, sizeof broken);
    broken[3] ^= 0xffu;   /* the length's complement */
    bad = deflated_stored; bad.packed = broken;
    zip(&a, &bad, 1);
    CHECK(refused(&a, ORACLES_ZIP_PATCH, "does not decompress"));
    bad = stored; bad.size = 1u << 21;
    zip(&a, &bad, 1);
    CHECK(refused(&a, ORACLES_ZIP_ROM, "the ROM in the zip is too large: over 1 MiB"));
    bad = stored; bad.packed_size = bad.size = 0; bad.crc = 0;
    zip(&a, &bad, 1);
    CHECK(refused(&a, ORACLES_ZIP_ROM, "the ROM in the zip is empty"));

    /* A Zip64 archive: its end record's count of entries 0xffff, the real one elsewhere; an entry whose sizes say so. */
    zip(&a, &fixed, 1);
    a.data[a.size - 29 + 10] = a.data[a.size - 29 + 11] = 0xffu;
    CHECK(refused(&a, ORACLES_ZIP_ROM, "Zip64"));
    zip(&a, &fixed, 1);
    const uint8_t *record = a.data + a.size - 29;
    memset(a.data + (record[16] | record[17] << 8) + 20, 0xff, 8);   /* the directory's sizes */
    CHECK(refused(&a, ORACLES_ZIP_ROM, "Zip64"));

    /* An archive cut before its directory's end, or in its directory. */
    zip(&a, &fixed, 1);
    a.size -= 30;
    CHECK(refused(&a, ORACLES_ZIP_ROM, "no directory"));
    zip(&a, among, 5);
    const uint8_t *end = a.data + a.size - 29;   /* the end record and its 7-byte comment */
    memset(a.data + (end[16] | end[17] << 8), 0, 4);   /* the directory's first signature */
    CHECK(refused(&a, ORACLES_ZIP_ROM, "cut short"));

    /* The ROM loader: a zipped file's inside, a plain file as it is. */
    if (argc > 1) {
        char error[160] = "";
        size_t size = 0;
        zip(&a, &dynamic, 1);
        CHECK(write_file(argv[1], a.data, a.size));
        uint8_t *data = oracles_rom_read_file(argv[1], &size, error, sizeof error);
        CHECK(data && size == sizeof dynamic_text && memcmp(data, dynamic_text, size) == 0);
        free(data);
        CHECK(oracles_zip_is_patch_file(argv[1]) == 0);
        CHECK(write_file(argv[1], text, text_size));
        data = oracles_rom_read_file(argv[1], &size, error, sizeof error);
        CHECK(data && size == text_size && memcmp(data, text, size) == 0);
        free(data);

        /* The patch reader: a zipped patch (an empty one, its source and target empty) checks as the patch itself;
         * it, the patch itself and a file named .bps are patches, a zip whose directory is lost cannot say. */
        uint8_t bps[19] = { 'B', 'P', 'S', '1', 0x80, 0x80, 0x80 };
        const uint32_t crc = oracles_crc32(bps, 15);
        for (int i = 0; i < 4; i++) bps[15 + i] = (uint8_t)(crc >> (8 * i));
        const entry patch = { "Moonrise Regalia 1.0.6.bps", 0, 0, bps, sizeof bps, sizeof bps, oracles_crc32(bps, sizeof bps) };
        zip(&a, &patch, 1);
        CHECK(write_file(argv[1], a.data, a.size));
        CHECK(oracles_bps_check_file(argv[1], error, sizeof error) == 0);
        CHECK(oracles_zip_is_patch_file(argv[1]) == 1);
        CHECK(write_file(argv[1], a.data, a.size - 30));
        CHECK(oracles_zip_is_patch_file(argv[1]) == -1);
        CHECK(write_file(argv[1], bps, sizeof bps));
        CHECK(oracles_zip_is_patch_file(argv[1]) == 1);
        char named[1024];
        snprintf(named, sizeof named, "%s.BPS", argv[1]);
        CHECK(write_file(named, text, text_size));
        CHECK(oracles_zip_is_patch_file(named) == 1);
        remove(named);

        /* A 7z or RAR archive, refused by both readers as one; a zip larger than a reader reads, refused as one.  None
         * of them, nor a zip of neither a ROM nor a patch, nor a file that does not open, says whether it is a patch. */
        static const uint8_t seven[8] = { '7', 'z', 0xbc, 0xaf, 0x27, 0x1c, 0, 4 }, rar[8] = { 'R', 'a', 'r', '!', 0x1a, 0x07, 1, 0 };
        CHECK(write_file(argv[1], seven, sizeof seven));
        CHECK(!oracles_rom_read_file(argv[1], &size, error, sizeof error) && strstr(error, "a 7z archive, which the port does not read"));
        CHECK(oracles_bps_check_file(argv[1], error, sizeof error) != 0 && strstr(error, "a 7z archive"));
        CHECK(oracles_zip_is_patch_file(argv[1]) == -1);
        CHECK(write_file(argv[1], rar, sizeof rar));
        CHECK(!oracles_rom_read_file(argv[1], &size, error, sizeof error) && strstr(error, "a RAR archive"));
        CHECK(oracles_zip_is_patch_file(argv[1]) == -1);
        zip(&a, &patch, 1);
        FILE *f = fopen(argv[1], "wb");
        CHECK(f && fwrite(a.data, 1, a.size, f) == a.size && fseek(f, 16L << 20, SEEK_SET) == 0 && fputc(0, f) == 0);
        if (f) fclose(f);
        CHECK(!oracles_rom_read_file(argv[1], &size, error, sizeof error) && strstr(error, "the zip file is too large: over 8 MiB; keep only the ROM or the patch in it"));
        CHECK(oracles_bps_check_file(argv[1], error, sizeof error) != 0 && strstr(error, "the zip file is too large: over 16 MiB"));
        CHECK(oracles_zip_is_patch_file(argv[1]) == -1);
        const entry ips = { "Moonrise Regalia 1.0.6.ips", 0, 0, bps, sizeof bps, sizeof bps, oracles_crc32(bps, sizeof bps) };
        zip(&a, &ips, 1);
        CHECK(write_file(argv[1], a.data, a.size));
        CHECK(oracles_zip_is_patch_file(argv[1]) == -1);
        remove(argv[1]);
        CHECK(oracles_zip_is_patch_file(argv[1]) == -1);
    }

    if (failures) return 1;
    puts("test_zip: ok");
    return 0;
}
