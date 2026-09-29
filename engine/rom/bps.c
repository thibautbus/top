#include "bps.h"
#include "rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TARGET_SIZE (8u * 1024u * 1024u)   /* as the loader's largest ROM */
#define MAX_PATCH_SIZE (16u * 1024u * 1024u)
#define FOOTER 12u                               /* the three CRC32 */

static void set_error(char *error, size_t capacity, const char *message)
{
    if (error && capacity) snprintf(error, capacity, "%s", message);
}

uint32_t oracles_crc32(const uint8_t *data, size_t size)
{
    static uint32_t table[256];
    static int ready;
    if (!ready) {
        for (uint32_t n = 0; n < 256u; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++) c = c & 1u ? 0xedb88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        ready = 1;
    }
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < size; i++) crc = table[(crc ^ data[i]) & 0xffu] ^ (crc >> 8);
    return crc ^ 0xffffffffu;
}

static uint32_t read32le(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* A number of the format: seven bits a byte, the last one's top bit set, each
 * continuation adding one to what follows (no two encodings of one value).
 * Returns 0 past `end` or beyond 2^48, which no field of a ROM's patch reaches. */
static int number(const uint8_t *patch, size_t *pos, size_t end, uint64_t *value)
{
    uint64_t v = 0, shift = 1;
    for (;;) {
        if (*pos >= end || shift > (1ull << 48)) return 0;
        const uint8_t byte = patch[(*pos)++];
        v += (uint64_t)(byte & 0x7fu) * shift;
        if (byte & 0x80u) break;
        shift <<= 7;
        v += shift;
    }
    *value = v;
    return 1;
}

int oracles_bps_check(const uint8_t *patch, size_t patch_size, char *error, size_t error_capacity)
{
    if (patch_size < 4u + 3u + FOOTER || memcmp(patch, "BPS1", 4) != 0) { set_error(error, error_capacity, "the patch is not a BPS patch"); return -1; }
    if (read32le(patch + patch_size - 4u) != oracles_crc32(patch, patch_size - 4u)) {
        set_error(error, error_capacity, "the patch file is damaged (its checksum does not match)");
        return -1;
    }
    return 0;
}

uint8_t *oracles_bps_apply(const uint8_t *source, size_t source_size, const uint8_t *patch, size_t patch_size,
                           size_t *target_size, char *error, size_t error_capacity)
{
    if (oracles_bps_check(patch, patch_size, error, error_capacity) != 0) return NULL;
    const size_t end = patch_size - FOOTER;   /* the actions stop before the checksums */
    size_t pos = 4;
    uint64_t declared_source, declared_target, metadata;
    if (!number(patch, &pos, end, &declared_source) || !number(patch, &pos, end, &declared_target) || !number(patch, &pos, end, &metadata)
        || metadata > end - pos) { set_error(error, error_capacity, "the patch's header is cut short"); return NULL; }
    pos += (size_t)metadata;
    if (declared_source != source_size || read32le(patch + end) != oracles_crc32(source, source_size)) {
        set_error(error, error_capacity, "this patch is not for this ROM (the base's size or checksum differs)");
        return NULL;
    }
    if (declared_target == 0 || declared_target > MAX_TARGET_SIZE) { set_error(error, error_capacity, "the patch makes an image of an impossible size"); return NULL; }
    const size_t size = (size_t)declared_target;
    uint8_t *out = malloc(size);
    if (!out) { set_error(error, error_capacity, "out of memory"); return NULL; }
    size_t written = 0;
    int64_t source_relative = 0, target_relative = 0;
    while (pos < end) {
        uint64_t action;
        if (!number(patch, &pos, end, &action)) goto cut;
        const unsigned kind = (unsigned)(action & 3u);
        const uint64_t length = (action >> 2) + 1u;
        if (length > size - written) goto bounds;
        const size_t n = (size_t)length;
        switch (kind) {
        case 0:   /* source read: the source's bytes at the same place */
            if (written + n > source_size) goto bounds;
            memcpy(out + written, source + written, n);
            break;
        case 1:   /* target read: bytes carried by the patch */
            if (n > end - pos) goto cut;
            memcpy(out + written, patch + pos, n);
            pos += n;
            break;
        case 2:   /* source copy: from anywhere in the source, the offset relative to the last one */
        case 3: { /* target copy: from what is written already, byte by byte (a run may overlap) */
            uint64_t delta;
            if (!number(patch, &pos, end, &delta)) goto cut;
            const int64_t offset = (int64_t)(delta >> 1) * ((delta & 1u) ? -1 : 1);
            if (kind == 2) {
                source_relative += offset;
                if (source_relative < 0 || (uint64_t)source_relative + n > source_size) goto bounds;
                memcpy(out + written, source + source_relative, n);
                source_relative += (int64_t)n;
            } else {
                target_relative += offset;
                if (target_relative < 0 || (uint64_t)target_relative >= written) goto bounds;
                for (size_t i = 0; i < n; i++) out[written + i] = out[target_relative++];
            }
            break;
        }
        }
        written += n;
    }
    if (written != size) { free(out); set_error(error, error_capacity, "the patch ends before its image does"); return NULL; }
    if (read32le(patch + end + 4u) != oracles_crc32(out, size)) {
        free(out);
        set_error(error, error_capacity, "the patched image's checksum does not match the patch's");
        return NULL;
    }
    *target_size = size;
    return out;
cut:
    free(out);
    set_error(error, error_capacity, "the patch is cut short");
    return NULL;
bounds:
    free(out);
    set_error(error, error_capacity, "the patch reads or writes outside its images");
    return NULL;
}

static uint8_t *read_patch(const char *path, size_t *size, char *error, size_t error_capacity)
{
    FILE *f = fopen(path, "rb");
    if (!f) { set_error(error, error_capacity, "cannot open the patch file"); return NULL; }
    long length = -1;
    if (fseek(f, 0, SEEK_END) == 0) length = ftell(f);
    if (length <= 0 || (unsigned long)length > MAX_PATCH_SIZE || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        set_error(error, error_capacity, "the patch file has an impossible size");
        return NULL;
    }
    uint8_t *data = malloc((size_t)length);
    if (!data || fread(data, 1, (size_t)length, f) != (size_t)length) {
        fclose(f); free(data);
        set_error(error, error_capacity, "cannot read the patch file");
        return NULL;
    }
    fclose(f);
    *size = (size_t)length;
    return data;
}

int oracles_bps_check_file(const char *patch_path, char *error, size_t error_capacity)
{
    size_t size = 0;
    uint8_t *patch = read_patch(patch_path, &size, error, error_capacity);
    if (!patch) return -1;
    const int status = oracles_bps_check(patch, size, error, error_capacity);
    free(patch);
    return status;
}

uint8_t *oracles_bps_apply_files(const char *base_path, const char *patch_path, size_t *size, char *error, size_t error_capacity)
{
    size_t base_size = 0, patch_size = 0;
    uint8_t *base = oracles_rom_read_file(base_path, &base_size, error, error_capacity);
    if (!base) return NULL;
    uint8_t *patch = read_patch(patch_path, &patch_size, error, error_capacity);
    if (!patch) { free(base); return NULL; }
    uint8_t *image = oracles_bps_apply(base, base_size, patch, patch_size, size, error, error_capacity);
    free(base);
    free(patch);
    return image;
}
