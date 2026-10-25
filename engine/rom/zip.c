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
#define FAST_BITS 9u   /* the bits a code's first lookup reads */

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
    uint64_t buffer;
    unsigned count;   /* bits in `buffer`: the rest of the byte being read, then whole bytes read ahead */
    int over;         /* read past the end */
} bits;

/* As many whole bytes into the buffer as it holds, or as are left. */
static void refill(bits *b)
{
    while (b->count <= 56u && b->pos < b->size) {
        b->buffer |= (uint64_t)b->in[b->pos++] << b->count;
        b->count += 8;
    }
}

/* The next `n` bits (at most 16), the first in the lowest. */
static unsigned take(bits *b, unsigned n)
{
    if (b->count < n) refill(b);
    if (b->count < n) { b->over = 1; return 0; }
    const unsigned value = (unsigned)(b->buffer & ((1u << n) - 1u));
    b->buffer >>= n;
    b->count -= n;
    return value;
}

/* A canonical Huffman code: the number of codes of each length, the symbols in the order of their codes, and by the
 * next FAST_BITS bits, the code of FAST_BITS bits or fewer they begin with (its symbol << 4 | its length; 0 for none). */
typedef struct huffman {
    uint16_t count[16];
    uint16_t symbol[288];
    uint16_t fast[1u << FAST_BITS];
} huffman;

/* The code of `n` symbols' lengths, with its table of short codes when `table` (decode's; decode_bits reads without).
 * 0, or -1 for lengths no code has (more codes than a length leaves room for). */
static int build(huffman *h, const uint8_t *lengths, unsigned n, int table)
{
    uint16_t offsets[16];
    unsigned next[16];
    memset(h->count, 0, sizeof h->count);
    if (table) memset(h->fast, 0, sizeof h->fast);
    for (unsigned s = 0; s < n; s++) h->count[lengths[s]]++;
    int left = 1;
    for (unsigned length = 1; length < 16u; length++) {
        left = (left << 1) - h->count[length];
        if (left < 0) return -1;
    }
    offsets[1] = 0;
    for (unsigned length = 1; length < 15u; length++) offsets[length + 1] = (uint16_t)(offsets[length] + h->count[length]);
    /* Each length's first code follows the shorter lengths' last (RFC 1951, 3.2.2). */
    for (unsigned length = 1, code = 0; length < 16u; length++) {
        next[length] = code;
        code = (code + h->count[length]) << 1;
    }
    for (unsigned s = 0; s < n; s++) {
        const unsigned length = lengths[s];
        if (!length) continue;
        h->symbol[offsets[length]++] = (uint16_t)s;
        const unsigned code = next[length]++;
        if (!table || length > FAST_BITS) continue;
        /* The code is read first bit first, its highest: in the buffer, reversed, under every value of the bits after. */
        unsigned reversed = 0;
        for (unsigned i = 0; i < length; i++) reversed |= (code >> i & 1u) << (length - 1u - i);
        for (unsigned i = reversed; i < 1u << FAST_BITS; i += 1u << length) h->fast[i] = (uint16_t)(s << 4 | length);
    }
    return 0;
}

/* The next symbol of code `h` read one bit at a time, or -1 for a code it does not have or that the input cuts short. */
static int decode_bits(bits *b, const huffman *h)
{
    if (b->count < 15u) refill(b);
    int code = 0, first = 0, index = 0;
    for (unsigned length = 1; length < 16u && length <= b->count; length++) {
        code |= (int)(b->buffer >> (length - 1u) & 1u);
        const int count = h->count[length];
        if (code - first < count) {
            b->buffer >>= length;
            b->count -= length;
            return h->symbol[index + code - first];
        }
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    return -1;
}

/* The next symbol of code `h`, built with its table: a short code looked up, a longer one read bit by bit; -1 as
 * decode_bits. */
static int decode(bits *b, const huffman *h)
{
    if (b->count < 15u) refill(b);
    const unsigned entry = h->fast[b->buffer & ((1u << FAST_BITS) - 1u)];
    if (!entry) return decode_bits(b, h);
    const unsigned length = entry & 15u;
    if (length > b->count) return -1;
    b->buffer >>= length;
    b->count -= length;
    return (int)(entry >> 4);
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
        if (symbol < 0) return -1;
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
        uint8_t *to = out->data + out->pos;
        const uint8_t *from = to - distance;
        for (size_t i = 0; i < length; i++) to[i] = from[i];   /* byte after byte: a copy may repeat its own bytes */
        out->pos += length;
    }
}

static int stored_block(bits *b, output *out)
{
    b->pos -= b->count / 8u;   /* to the next byte: the whole bytes read ahead given back, the rest of this one left */
    b->buffer = 0;
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

/* The fixed code's literals and lengths, and its distances (RFC 1951, 3.2.6). */
static void fixed_codes(huffman *literal, huffman *distance)
{
    uint8_t lengths[288];
    memset(lengths, 8, 144);
    memset(lengths + 144, 9, 112);
    memset(lengths + 256, 7, 24);
    memset(lengths + 280, 8, 8);
    build(literal, lengths, 288, 1);
    memset(lengths, 5, 30);
    build(distance, lengths, 30, 1);
}

static int dynamic_block(bits *b, output *out)
{
    static const uint8_t order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    uint8_t lengths[320] = { 0 };
    huffman literal, distance;
    const unsigned literals = take(b, 5) + 257u, distances = take(b, 5) + 1u, code_lengths = take(b, 4) + 4u;
    if (literals > 286u || distances > 30u) return -1;
    for (unsigned i = 0; i < code_lengths; i++) lengths[order[i]] = (uint8_t)take(b, 3);
    if (b->over || build(&literal, lengths, 19, 0) != 0) return -1;   /* 19 symbols: read without a table */
    for (unsigned i = 0; i < literals + distances;) {
        const int symbol = decode_bits(b, &literal);
        if (symbol < 0) return -1;
        if (symbol < 16) { lengths[i++] = (uint8_t)symbol; continue; }
        uint8_t repeated = 0;
        unsigned times;
        if (symbol == 16) {
            if (i == 0) return -1;
            repeated = lengths[i - 1];
            times = 3u + take(b, 2);
        } else times = symbol == 17 ? 3u + take(b, 3) : 11u + take(b, 7);
        if (b->over || i + times > literals + distances) return -1;
        while (times--) lengths[i++] = repeated;
    }
    if (lengths[256] == 0 || build(&literal, lengths, literals, 1) != 0 || build(&distance, lengths + literals, distances, 1) != 0) return -1;
    return codes(b, out, &literal, &distance);
}

/* `in` inflated into exactly `size` bytes.  0, or -1 for a damaged stream or one of another size. */
static int inflate(const uint8_t *in, size_t in_size, uint8_t *data, size_t size)
{
    bits b = { in, in_size, 0, 0, 0, 0 };
    output out = { data, size, 0 };
    huffman fixed_literal, fixed_distance;   /* built at the stream's first fixed block, for all of them */
    int fixed = 0, last;
    do {
        last = (int)take(&b, 1);
        const unsigned type = take(&b, 2);
        if (b.over) return -1;
        if (type == 1 && !fixed) { fixed_codes(&fixed_literal, &fixed_distance); fixed = 1; }
        const int result = type == 0 ? stored_block(&b, &out) : type == 1 ? codes(&b, &out, &fixed_literal, &fixed_distance)
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

/* The archive `data` begins as among those the port does not read, 7z and RAR (4 and 5); NULL for another file. */
static const char *other_archive(const uint8_t *data, size_t size)
{
    if (size >= 6u && memcmp(data, "7z\xbc\xaf\x27\x1c", 6) == 0) return "7z";
    if (size >= 6u && memcmp(data, "Rar!\x1a\x07", 6) == 0) return "RAR";
    return NULL;
}

/* Whether a name of `length` bytes ends with `extension` (".gbc"), in any case. */
static int has_extension(const char *name, size_t length, const char *extension)
{
    const size_t n = strlen(extension);
    if (length <= n) return 0;
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)name[length - n + i]) != extension[i]) return 0;
    return 1;
}

/* The kind of file an entry's name is, -1 for neither: a folder, another file, one of macOS's resource files
 * (__MACOSX/, or ._NAME beside NAME). */
static int entry_kind(const uint8_t *entry, size_t length)
{
    const char *name = (const char *)entry;
    if (length >= 9u && memcmp(name, "__MACOSX/", 9) == 0) return -1;
    size_t base = length;
    while (base > 0 && name[base - 1] != '/') base--;
    if (length - base >= 2u && name[base] == '.' && name[base + 1] == '_') return -1;
    if (has_extension(name, length, ".gbc") || has_extension(name, length, ".gb")) return ORACLES_ZIP_ROM;
    return has_extension(name, length, ".bps") ? ORACLES_ZIP_PATCH : -1;
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

/* The zip's directory read: the ROMs and the patches it names, counted in `found` by kind, and the directory entry of
 * the last of kind `kind` in `*chosen`.  0, or -1 with the reason when the directory cannot be read. */
static int walk(const uint8_t *zip, size_t size, OraclesZipKind kind, unsigned found[2], const uint8_t **chosen, char *error,
                size_t error_capacity)
{
    const uint8_t *end = end_record(zip, size);
    if (!end) { set_error(error, error_capacity, "the zip file is damaged (no directory)"); return -1; }
    const unsigned entries = read16le(end + 10);
    const uint32_t directory_size = read32le(end + 12), directory = read32le(end + 16);
    if (entries == 0xffffu || directory == 0xffffffffu) { set_error(error, error_capacity, "the zip file is a Zip64 archive, which the port does not read"); return -1; }
    if (directory > (size_t)(end - zip) || directory_size > (size_t)(end - zip) - directory) {
        set_error(error, error_capacity, "the zip file is damaged (directory out of the file)");
        return -1;
    }
    size_t at = directory;
    for (unsigned i = 0; i < entries; i++) {
        if (directory_size - (at - directory) < CENTRAL_HEADER || read32le(zip + at) != 0x02014b50u) {
            set_error(error, error_capacity, "the zip file is damaged (directory cut short)");
            return -1;
        }
        const size_t name = read16le(zip + at + 28), next = CENTRAL_HEADER + name + read16le(zip + at + 30) + read16le(zip + at + 32);
        if (directory_size - (at - directory) < next) { set_error(error, error_capacity, "the zip file is damaged (directory cut short)"); return -1; }
        const int entry = entry_kind(zip + at + CENTRAL_HEADER, name);
        if (entry >= 0) {
            found[entry]++;
            if (entry == (int)kind) *chosen = zip + at;
        }
        at += next;
    }
    return 0;
}

int oracles_zip_holds_patch(const uint8_t *zip, size_t size)
{
    unsigned found[2] = { 0, 0 };
    const uint8_t *chosen = NULL;
    if (walk(zip, size, ORACLES_ZIP_PATCH, found, &chosen, NULL, 0) != 0 || found[ORACLES_ZIP_ROM] + found[ORACLES_ZIP_PATCH] == 0) return -1;
    return found[ORACLES_ZIP_ROM] == 0;
}

uint8_t *oracles_zip_extract(const uint8_t *zip, size_t size, OraclesZipKind kind, size_t max_size, size_t *out_size,
                             char *error, size_t error_capacity)
{
    const int rom = kind == ORACLES_ZIP_ROM;
    char message[128];
    unsigned found[2] = { 0, 0 };
    const uint8_t *chosen = NULL;
    if (walk(zip, size, kind, found, &chosen, error, error_capacity) != 0) return NULL;
    if (found[kind] > 1) { set_error(error, error_capacity, rom ? "the zip file holds several ROMs; keep one in it" : "the zip file holds several patches; keep one in it"); return NULL; }
    if (!chosen) {
        set_error(error, error_capacity, rom ? found[ORACLES_ZIP_PATCH] ? "the zip file holds a patch (.bps), not a ROM" : "the zip file holds no ROM (.gbc, .gb)"
                                             : found[ORACLES_ZIP_ROM] ? "the zip file holds a ROM, not a patch (.bps)" : "the zip file holds no patch (.bps)");
        return NULL;
    }

    const unsigned flags = read16le(chosen + 8), method = read16le(chosen + 10);
    const uint32_t crc = read32le(chosen + 16), packed = read32le(chosen + 20), unpacked = read32le(chosen + 24);
    const uint32_t local = read32le(chosen + 42);
    if (flags & 1u) { set_error(error, error_capacity, "the zip file is encrypted"); return NULL; }
    if (method != 0 && method != 8) { set_error(error, error_capacity, "the zip file is compressed in a way the port does not read; zip it again with the usual compression (deflate)"); return NULL; }
    if (packed == 0xffffffffu || unpacked == 0xffffffffu) { set_error(error, error_capacity, "the zip file is a Zip64 archive, which the port does not read"); return NULL; }
    if (unpacked == 0) { set_error(error, error_capacity, rom ? "the ROM in the zip is empty" : "the patch in the zip is empty"); return NULL; }
    if (unpacked > max_size) {
        snprintf(message, sizeof message, "the %s in the zip is too large: over %u MiB", rom ? "ROM" : "patch", (unsigned)(max_size >> 20));
        set_error(error, error_capacity, message);
        return NULL;
    }
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

/* ---- the files ------------------------------------------------------------------------------------------------- */

/* The file at `path` read whole, at most `max_size` bytes: NULL with the reason otherwise, a 7z or RAR archive refused
 * whatever its size, and a zip over `max_size` refused as one. */
static uint8_t *read_whole(const char *path, const char *what, size_t max_size, size_t *size, char *error, size_t error_capacity)
{
    char message[160];
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(message, sizeof message, "cannot open the %s file", what);
        set_error(error, error_capacity, message);
        return NULL;
    }
    uint8_t start[6] = { 0 };
    const size_t begun = fread(start, 1, sizeof start, f);
    long length = -1;
    if (fseek(f, 0, SEEK_END) == 0) length = ftell(f);
    const char *archive = other_archive(start, begun);
    if (archive) snprintf(message, sizeof message, "the file is a %s archive, which the port does not read; use a zip or the file itself", archive);
    else if (length > 0 && (unsigned long)length > max_size && oracles_zip_is_zip(start, begun))
        snprintf(message, sizeof message, "the zip file is too large: over %u MiB; keep only the ROM or the patch in it", (unsigned)(max_size >> 20));
    else if (length <= 0 || (unsigned long)length > max_size) snprintf(message, sizeof message, "the %s file has an impossible size", what);
    else message[0] = 0;
    uint8_t *data = message[0] || fseek(f, 0, SEEK_SET) != 0 ? NULL : malloc((size_t)length);
    if (data && fread(data, 1, (size_t)length, f) != (size_t)length) { free(data); data = NULL; }
    fclose(f);
    if (!data) {
        if (!message[0]) snprintf(message, sizeof message, "cannot read the %s file", what);
        set_error(error, error_capacity, message);
        return NULL;
    }
    *size = (size_t)length;
    return data;
}

uint8_t *oracles_zip_read_file(const char *path, OraclesZipKind kind, size_t max_size, size_t *size, char *error,
                               size_t error_capacity)
{
    size_t length = 0;
    uint8_t *data = read_whole(path, kind == ORACLES_ZIP_ROM ? "ROM" : "patch", max_size, &length, error, error_capacity);
    if (!data || !oracles_zip_is_zip(data, length)) {
        if (data) *size = length;
        return data;
    }
    uint8_t *inner = oracles_zip_extract(data, length, kind, max_size, size, error, error_capacity);
    free(data);
    return inner;
}

int oracles_zip_is_patch_file(const char *path)
{
    if (has_extension(path, strlen(path), ".bps")) return 1;
    size_t size = 0;
    uint8_t *data = read_whole(path, "patch", ORACLES_BPS_MAX_PATCH_SIZE, &size, NULL, 0);
    if (!data) return -1;   /* unread: a 7z or RAR archive, a file too large or that does not open */
    const int patch = oracles_zip_is_zip(data, size) ? oracles_zip_holds_patch(data, size) : size >= 4u && memcmp(data, "BPS1", 4) == 0;
    free(data);
    return patch;
}
