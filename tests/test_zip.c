/* The zip reader on archives built here: stored and deflated files (stored, fixed and dynamic blocks), the file chosen
 * among others, the refusals, and the ROM loader and the patch reader reading through it. */
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
static const uint8_t stored_stream[] = {   /* one stored block */
    0x01, 0x28, 0x00, 0xd7, 0xff, 0x67, 0x67, 0x62, 0x64, 0x64, 0x64, 0x63, 0x64, 0x65, 0x67, 0x66, 0x68, 0x65, 0x65, 0x62,
    0x68, 0x62, 0x68, 0x62, 0x67, 0x67, 0x63, 0x68, 0x68, 0x68, 0x67, 0x68, 0x62, 0x63, 0x61, 0x65, 0x62, 0x63, 0x67, 0x64,
    0x64, 0x63, 0x66, 0x62, 0x61 };

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

typedef struct archive { uint8_t data[2048]; size_t size; } archive;

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

/* The archive's one file is `expected`. */
static int extracts(const archive *a, const uint8_t *expected, size_t size)
{
    char error[160] = "";
    size_t out_size = 0;
    uint8_t *out = oracles_zip_extract(a->data, a->size, 1u << 20, &out_size, error, sizeof error);
    const int same = out && out_size == size && memcmp(out, expected, size) == 0;
    if (!same) fprintf(stderr, "  extracted %zu bytes, expected %zu: %s\n", out_size, size, error);
    free(out);
    return same;
}

/* The archive is refused with a reason that holds `reason`. */
static int refused(const archive *a, const char *reason)
{
    char error[160] = "";
    size_t out_size = 0;
    uint8_t *out = oracles_zip_extract(a->data, a->size, 1u << 20, &out_size, error, sizeof error);
    free(out);
    if (out || !strstr(error, reason)) fprintf(stderr, "  expected a refusal with \"%s\", got \"%s\"\n", reason, error);
    return !out && strstr(error, reason) != NULL;
}

int main(int argc, char **argv)
{
    uint8_t dynamic_text[512], stored_text[40];
    content(dynamic_text, sizeof dynamic_text);
    content(stored_text, sizeof stored_text);
    const size_t text_size = sizeof text - 1u;
    const entry fixed = { "Oracle of Ages.gbc", 8, 0, fixed_stream, sizeof fixed_stream, text_size, oracles_crc32((const uint8_t *)text, text_size) };
    const entry dynamic = { "roms/Oracle of Seasons.GB", 8, 0, dynamic_stream, sizeof dynamic_stream, sizeof dynamic_text, oracles_crc32(dynamic_text, sizeof dynamic_text) };
    const entry deflated_stored = { "Moonrise Regalia 1.0.6.bps", 8, 0, stored_stream, sizeof stored_stream, sizeof stored_text, oracles_crc32(stored_text, sizeof stored_text) };
    const entry stored = { "Oracle of Ages.gbc", 0, 0, stored_text, sizeof stored_text, sizeof stored_text, oracles_crc32(stored_text, sizeof stored_text) };
    const entry readme = { "readme.txt", 0, 0, (const uint8_t *)"hi", 2, 2, oracles_crc32((const uint8_t *)"hi", 2) };
    const entry resource = { "__MACOSX/._Oracle of Ages.gbc", 0, 0, (const uint8_t *)"hi", 2, 2, oracles_crc32((const uint8_t *)"hi", 2) };
    const entry beside = { "roms/._Oracle of Seasons.GB", 0, 0, (const uint8_t *)"hi", 2, 2, oracles_crc32((const uint8_t *)"hi", 2) };
    const entry folder = { "roms/", 0, 0, (const uint8_t *)"", 0, 0, 0 };
    archive a;

    /* Each kind of block, a stored file, a ROM among other files, sizes after the data. */
    zip(&a, &fixed, 1);
    CHECK(oracles_zip_is_zip(a.data, a.size));
    CHECK(extracts(&a, (const uint8_t *)text, text_size));
    zip(&a, &dynamic, 1);
    CHECK(extracts(&a, dynamic_text, sizeof dynamic_text));
    zip(&a, &deflated_stored, 1);
    CHECK(extracts(&a, stored_text, sizeof stored_text));
    zip(&a, &stored, 1);
    CHECK(extracts(&a, stored_text, sizeof stored_text));
    const entry among[5] = { readme, folder, resource, dynamic, beside };
    zip(&a, among, 5);
    CHECK(extracts(&a, dynamic_text, sizeof dynamic_text));
    entry later = dynamic;
    later.flags = 8;
    zip(&a, &later, 1);
    CHECK(extracts(&a, dynamic_text, sizeof dynamic_text));

    /* No ROM, two ROMs. */
    const entry none[2] = { readme, resource };
    zip(&a, none, 2);
    CHECK(refused(&a, "holds no ROM"));
    const entry two[2] = { fixed, dynamic };
    zip(&a, two, 2);
    CHECK(refused(&a, "several ROMs"));

    /* A wrong checksum, encryption, another method, a stream cut short or of another size, a bad stored length. */
    entry bad = fixed;
    bad.crc ^= 1u;
    zip(&a, &bad, 1);
    CHECK(refused(&a, "checksum"));
    bad = fixed; bad.flags = 1;
    zip(&a, &bad, 1);
    CHECK(refused(&a, "encrypted"));
    bad = fixed; bad.method = 14;
    zip(&a, &bad, 1);
    CHECK(refused(&a, "deflate"));
    bad = dynamic; bad.packed_size = 100;
    zip(&a, &bad, 1);
    CHECK(refused(&a, "does not decompress"));
    bad = dynamic; bad.size = 511;
    zip(&a, &bad, 1);
    CHECK(refused(&a, "does not decompress"));
    uint8_t broken[sizeof stored_stream];
    memcpy(broken, stored_stream, sizeof broken);
    broken[3] ^= 0xffu;   /* the length's complement */
    bad = deflated_stored; bad.packed = broken;
    zip(&a, &bad, 1);
    CHECK(refused(&a, "does not decompress"));
    bad = stored; bad.size = 1u << 21;
    zip(&a, &bad, 1);
    CHECK(refused(&a, "impossible size"));
    bad = stored; bad.packed_size = bad.size = 0; bad.crc = 0;
    zip(&a, &bad, 1);
    CHECK(refused(&a, "impossible size"));

    /* A Zip64 archive: its end record's count of entries 0xffff, the real one elsewhere. */
    zip(&a, &fixed, 1);
    a.data[a.size - 29 + 10] = a.data[a.size - 29 + 11] = 0xffu;
    CHECK(refused(&a, "Zip64"));

    /* An archive cut before its directory's end, or in its directory. */
    zip(&a, &fixed, 1);
    a.size -= 30;
    CHECK(refused(&a, "no directory"));
    zip(&a, among, 5);
    const uint8_t *end = a.data + a.size - 29;   /* the end record and its 7-byte comment */
    memset(a.data + (end[16] | end[17] << 8), 0, 4);   /* the directory's first signature */
    CHECK(refused(&a, "cut short"));

    /* The ROM loader: a zipped file's inside, a plain file as it is. */
    if (argc > 1) {
        char error[160] = "";
        size_t size = 0;
        zip(&a, &dynamic, 1);
        FILE *f = fopen(argv[1], "wb");
        CHECK(f && fwrite(a.data, 1, a.size, f) == a.size);
        if (f) fclose(f);
        uint8_t *data = oracles_rom_read_file(argv[1], &size, error, sizeof error);
        CHECK(data && size == sizeof dynamic_text && memcmp(data, dynamic_text, size) == 0);
        free(data);
        f = fopen(argv[1], "wb");
        CHECK(f && fwrite(text, 1, text_size, f) == text_size);
        if (f) fclose(f);
        data = oracles_rom_read_file(argv[1], &size, error, sizeof error);
        CHECK(data && size == text_size && memcmp(data, text, size) == 0);
        free(data);

        /* The patch reader: a zipped patch (an empty one, its source and target empty) checks as the patch itself. */
        uint8_t bps[19] = { 'B', 'P', 'S', '1', 0x80, 0x80, 0x80 };
        const uint32_t crc = oracles_crc32(bps, 15);
        for (int i = 0; i < 4; i++) bps[15 + i] = (uint8_t)(crc >> (8 * i));
        const entry patch = { "Moonrise Regalia 1.0.6.bps", 0, 0, bps, sizeof bps, sizeof bps, oracles_crc32(bps, sizeof bps) };
        zip(&a, &patch, 1);
        f = fopen(argv[1], "wb");
        CHECK(f && fwrite(a.data, 1, a.size, f) == a.size);
        if (f) fclose(f);
        CHECK(oracles_bps_check_file(argv[1], error, sizeof error) == 0);
        remove(argv[1]);
    }

    if (failures) return 1;
    puts("test_zip: ok");
    return 0;
}
