#include "zip.h"
#include "bps.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOCAL_HEADER 30u
#define CENTRAL_HEADER 46u
#define END_RECORD 22u
#define MAX_COMMENT 0xffffu

static void set_error(char *error, size_t capacity, const char *message)
{
    if (error && capacity) snprintf(error, capacity, "%s", message);
}

static unsigned read16le(const uint8_t *p) { return (unsigned)p[0] | (unsigned)p[1] << 8; }
static uint32_t read32le(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* ---- inflate (RFC 1951) ---------------------------------------------------------------------------------------- */

typedef struct bits {
    const uint8_t *in;
    size_t size, pos;
    uint32_t buffer;
    unsigned count;   /* bits in `buffer`, never more than 7 between two reads: the rest of the current byte */
    int over;         /* read past the end */
} bits;

/* The next `n` bits (at most 16), the first in the lowest. */
static unsigned take(bits *b, unsigned n)
{
    while (b->count < n) {
        if (b->pos >= b->size) { b->over = 1; return 0; }
        b->buffer |= (uint32_t)b->in[b->pos++] << b->count;
        b->count += 8;
    }
    const unsigned value = b->buffer & ((1u << n) - 1u);
    b->buffer >>= n;
    b->count -= n;
    return value;
}

/* A canonical Huffman code: the number of codes of each length, and the symbols in the order of their codes. */
typedef struct huffman {
    uint16_t count[16];
    uint16_t symbol[288];
} huffman;

/* The code of `n` symbols' lengths.  0, or -1 for lengths no code has (more codes than a length leaves room for). */
static int build(huffman *h, const uint8_t *lengths, unsigned n)
{
    uint16_t offsets[16];
    memset(h->count, 0, sizeof h->count);
    for (unsigned s = 0; s < n; s++) h->count[lengths[s]]++;
    int left = 1;
    for (unsigned length = 1; length < 16u; length++) {
        left = (left << 1) - h->count[length];
        if (left < 0) return -1;
    }
    offsets[1] = 0;
    for (unsigned length = 1; length < 15u; length++) offsets[length + 1] = (uint16_t)(offsets[length] + h->count[length]);
    for (unsigned s = 0; s < n; s++) if (lengths[s]) h->symbol[offsets[lengths[s]]++] = (uint16_t)s;
    return 0;
}

/* The next symbol of code `h`, or -1 for a code it does not have. */
static int decode(bits *b, const huffman *h)
{
    int code = 0, first = 0, index = 0;
    for (unsigned length = 1; length < 16u; length++) {
        code |= (int)take(b, 1);
        const int count = h->count[length];
        if (code - first < count) return h->symbol[index + code - first];
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    return -1;
}

typedef struct output { uint8_t *data; size_t size, pos; } output;

static const uint16_t length_base[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                          35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const uint8_t length_extra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const uint16_t distance_base[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
                                            1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const uint8_t distance_extra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

/* A block's literals and copies, up to its end symbol.  0, or -1 for a damaged stream. */
static int codes(bits *b, output *out, const huffman *lengths, const huffman *distances)
{
    for (;;) {
        int symbol = decode(b, lengths);
        if (b->over || symbol < 0) return -1;
        if (symbol < 256) {
            if (out->pos >= out->size) return -1;
            out->data[out->pos++] = (uint8_t)symbol;
            continue;
        }
        if (symbol == 256) return 0;
        symbol -= 257;
        if (symbol >= 29) return -1;
        const size_t length = length_base[symbol] + take(b, length_extra[symbol]);
        symbol = decode(b, distances);
        if (b->over || symbol < 0 || symbol >= 30) return -1;
        const size_t distance = distance_base[symbol] + take(b, distance_extra[symbol]);
        if (b->over || distance > out->pos || length > out->size - out->pos) return -1;
        for (size_t i = 0; i < length; i++, out->pos++) out->data[out->pos] = out->data[out->pos - distance];
    }
}

static int stored_block(bits *b, output *out)
{
    b->buffer = 0;   /* to the next byte: what is left in the buffer is the rest of this one */
    b->count = 0;
    if (b->size - b->pos < 4u) return -1;
    const unsigned length = read16le(b->in + b->pos), complement = read16le(b->in + b->pos + 2);
    b->pos += 4;
    if ((length ^ 0xffffu) != complement || length > b->size - b->pos || length > out->size - out->pos) return -1;
    memcpy(out->data + out->pos, b->in + b->pos, length);
    b->pos += length;
    out->pos += length;
    return 0;
}

static int fixed_block(bits *b, output *out)
{
    uint8_t lengths[288];
    huffman literal, distance;
    memset(lengths, 8, 144);
    memset(lengths + 144, 9, 112);
    memset(lengths + 256, 7, 24);
    memset(lengths + 280, 8, 8);
    build(&literal, lengths, 288);
    memset(lengths, 5, 30);
    build(&distance, lengths, 30);
    return codes(b, out, &literal, &distance);
}

static int dynamic_block(bits *b, output *out)
{
    static const uint8_t order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    uint8_t lengths[320] = { 0 };
    huffman literal, distance;
    const unsigned literals = take(b, 5) + 257u, distances = take(b, 5) + 1u, code_lengths = take(b, 4) + 4u;
    if (literals > 286u || distances > 30u) return -1;
    for (unsigned i = 0; i < code_lengths; i++) lengths[order[i]] = (uint8_t)take(b, 3);
    if (b->over || build(&literal, lengths, 19) != 0) return -1;
    for (unsigned i = 0; i < literals + distances;) {
        const int symbol = decode(b, &literal);
        if (b->over || symbol < 0) return -1;
        if (symbol < 16) { lengths[i++] = (uint8_t)symbol; continue; }
        uint8_t repeated = 0;
        unsigned times;
        if (symbol == 16) {
            if (i == 0) return -1;
            repeated = lengths[i - 1];
            times = 3u + take(b, 2);
        } else times = symbol == 17 ? 3u + take(b, 3) : 11u + take(b, 7);
        if (i + times > literals + distances) return -1;
        while (times--) lengths[i++] = repeated;
    }
    if (lengths[256] == 0 || build(&literal, lengths, literals) != 0 || build(&distance, lengths + literals, distances) != 0) return -1;
    return codes(b, out, &literal, &distance);
}

/* `in` inflated into exactly `size` bytes.  0, or -1 for a damaged stream or one of another size. */
static int inflate(const uint8_t *in, size_t in_size, uint8_t *data, size_t size)
{
    bits b = { in, in_size, 0, 0, 0, 0 };
    output out = { data, size, 0 };
    int last;
    do {
        last = (int)take(&b, 1);
        const unsigned type = take(&b, 2);
        if (b.over) return -1;
        const int result = type == 0 ? stored_block(&b, &out) : type == 1 ? fixed_block(&b, &out)
                         : type == 2 ? dynamic_block(&b, &out) : -1;
        if (result != 0) return -1;
    } while (!last);
    return out.pos == size ? 0 : -1;
}

/* ---- the archive ----------------------------------------------------------------------------------------------- */

int oracles_zip_is_zip(const uint8_t *data, size_t size)
{
    return size >= 4u && memcmp(data, "PK\3\4", 4) == 0;
}

/* Whether an entry's name is a ROM's or a patch's, not a folder's nor one of macOS's resource files (__MACOSX/,
 * or ._NAME beside NAME). */
static int is_game_file(const uint8_t *name, size_t length)
{
    static const char *const extensions[] = { ".gbc", ".gb", ".bps" };
    if (length >= 9u && memcmp(name, "__MACOSX/", 9) == 0) return 0;
    size_t base = length;
    while (base > 0 && name[base - 1] != '/') base--;
    if (length - base >= 2u && name[base] == '.' && name[base + 1] == '_') return 0;
    for (size_t e = 0; e < sizeof extensions / sizeof extensions[0]; e++) {
        const size_t n = strlen(extensions[e]);
        if (length <= n) continue;
        size_t i = 0;
        while (i < n && tolower(name[length - n + i]) == extensions[e][i]) i++;
        if (i == n) return 1;
    }
    return 0;
}

/* The end of central directory record: the last one, before at most a comment's length. */
static const uint8_t *end_record(const uint8_t *zip, size_t size)
{
    if (size < END_RECORD) return NULL;
    const size_t lowest = size - END_RECORD > MAX_COMMENT ? size - END_RECORD - MAX_COMMENT : 0;
    for (size_t at = size - END_RECORD + 1u; at-- > lowest;)
        if (read32le(zip + at) == 0x06054b50u) return zip + at;
    return NULL;
}

uint8_t *oracles_zip_extract(const uint8_t *zip, size_t size, size_t max_size, size_t *out_size, char *error,
                             size_t error_capacity)
{
    const uint8_t *end = end_record(zip, size);
    if (!end) { set_error(error, error_capacity, "the zip file is damaged (no directory)"); return NULL; }
    const unsigned entries = read16le(end + 10);
    const uint32_t directory_size = read32le(end + 12), directory = read32le(end + 16);
    if (entries == 0xffffu || directory == 0xffffffffu) { set_error(error, error_capacity, "the zip file is a Zip64 archive, which the port does not read"); return NULL; }
    if (directory > (size_t)(end - zip) || directory_size > (size_t)(end - zip) - directory) {
        set_error(error, error_capacity, "the zip file is damaged (directory out of the file)");
        return NULL;
    }

    const uint8_t *chosen = NULL;
    size_t at = directory;
    for (unsigned i = 0; i < entries; i++) {
        if (directory_size - (at - directory) < CENTRAL_HEADER || read32le(zip + at) != 0x02014b50u) {
            set_error(error, error_capacity, "the zip file is damaged (directory cut short)");
            return NULL;
        }
        const size_t name = read16le(zip + at + 28), next = CENTRAL_HEADER + name + read16le(zip + at + 30) + read16le(zip + at + 32);
        if (directory_size - (at - directory) < next) { set_error(error, error_capacity, "the zip file is damaged (directory cut short)"); return NULL; }
        if (is_game_file(zip + at + CENTRAL_HEADER, name)) {
            if (chosen) { set_error(error, error_capacity, "the zip file holds several ROMs or patches; keep one in it"); return NULL; }
            chosen = zip + at;
        }
        at += next;
    }
    if (!chosen) { set_error(error, error_capacity, "the zip file holds no ROM (.gbc, .gb) or patch (.bps)"); return NULL; }

    const unsigned flags = read16le(chosen + 8), method = read16le(chosen + 10);
    const uint32_t crc = read32le(chosen + 16), packed = read32le(chosen + 20), unpacked = read32le(chosen + 24);
    const uint32_t local = read32le(chosen + 42);
    if (flags & 1u) { set_error(error, error_capacity, "the zip file is encrypted"); return NULL; }
    if (method != 0 && method != 8) { set_error(error, error_capacity, "the zip file is compressed in a way the port does not read; zip it again with the usual compression (deflate)"); return NULL; }
    if (packed == 0xffffffffu || unpacked == 0xffffffffu) { set_error(error, error_capacity, "the zip file is a Zip64 archive, which the port does not read"); return NULL; }
    if (unpacked == 0 || unpacked > max_size) { set_error(error, error_capacity, "the file in the zip has an impossible size"); return NULL; }
    if (local > size || size - local < LOCAL_HEADER || read32le(zip + local) != 0x04034b50u) {
        set_error(error, error_capacity, "the zip file is damaged (file header missing)");
        return NULL;
    }
    /* The local header's sizes may be zero, the real ones after the data (bit 3, 8, of the flags): the directory's count. */
    const size_t start = (size_t)local + LOCAL_HEADER + read16le(zip + local + 26) + read16le(zip + local + 28);
    if (start > size || packed > size - start || (method == 0 && packed != unpacked)) {
        set_error(error, error_capacity, "the zip file is damaged (file cut short)");
        return NULL;
    }

    uint8_t *data = malloc(unpacked);
    if (!data) { set_error(error, error_capacity, "out of memory"); return NULL; }
    if (method == 0) memcpy(data, zip + start, unpacked);
    else if (inflate(zip + start, packed, data, unpacked) != 0) {
        free(data);
        set_error(error, error_capacity, "the zip file is damaged (file does not decompress)");
        return NULL;
    }
    if (oracles_crc32(data, unpacked) != crc) {
        free(data);
        set_error(error, error_capacity, "the zip file is damaged (checksum does not hold)");
        return NULL;
    }
    *out_size = unpacked;
    return data;
}

uint8_t *oracles_zip_unwrap(uint8_t *data, size_t *size, size_t max_size, char *error, size_t error_capacity)
{
    if (!oracles_zip_is_zip(data, *size)) return data;
    uint8_t *inner = oracles_zip_extract(data, *size, max_size, size, error, error_capacity);
    free(data);
    return inner;
}
