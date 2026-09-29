/* Dumps every room layout, tileset layout and graphics header entry decoded
 * from a ROM, so that check_against_disasm.py can compare them with the
 * decompressed assets of oracles-disasm and with its Python decompressor. */
#define _POSIX_C_SOURCE 200809L
#include "oracles_rom.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int mkdir_p(const char *path)
{
    char buffer[1024];
    snprintf(buffer, sizeof buffer, "%s", path);
    for (char *p = buffer + 1; *p; p++) {
        if (*p == '/') { *p = 0; if (mkdir(buffer, 0755) && errno != EEXIST) return -1; *p = '/'; }
    }
    return (mkdir(buffer, 0755) && errno != EEXIST) ? -1 : 0;
}

static int write_file(const char *path, const void *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    const int ok = fwrite(data, 1, size, f) == size;
    fclose(f);
    return ok ? 0 : -1;
}

int main(int argc, char **argv)
{
    const char *rom_path = NULL, *game = NULL, *out_dir = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--rom") && i + 1 < argc) rom_path = argv[++i];
        else if (!strcmp(argv[i], "--game") && i + 1 < argc) game = argv[++i];
        else if (!strcmp(argv[i], "--out") && i + 1 < argc) out_dir = argv[++i];
        else { fprintf(stderr, "unknown argument %s\n", argv[i]); return 2; }
    }
    if (!rom_path || !game || !out_dir) {
        fprintf(stderr, "usage: datacheck --rom ROM --game ages|seasons --out DIR\n");
        return 2;
    }
    const OraclesTables *t = !strcmp(game, "ages") ? &oracles_tables_ages
                           : !strcmp(game, "seasons") ? &oracles_tables_seasons : NULL;
    if (!t) { fprintf(stderr, "unknown game %s\n", game); return 2; }

    FILE *f = fopen(rom_path, "rb");
    if (!f) { perror(rom_path); return 1; }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc((size_t)size);
    if (!data || fread(data, 1, (size_t)size, f) != (size_t)size) { fprintf(stderr, "cannot read ROM\n"); return 1; }
    fclose(f);
    const OraclesRom rom = { data, (size_t)size };

    char path[1024];
    unsigned rooms = 0, room_errors = 0, tilesets = 0, tileset_errors = 0, gfx = 0, gfx_errors = 0;

    snprintf(path, sizeof path, "%s/rooms", out_dir); mkdir_p(path);
    for (unsigned group = 0; group < 8; group++) {
        for (unsigned room = 0; room < 256; room++) {
            uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES];
            int is_large = 0;
            memset(layout, 0, sizeof layout);
            if (oracles_decode_room_layout(&rom, t, group, room, layout, &is_large)) { room_errors++; continue; }
            uint8_t packed[ORACLES_ROOM_LAYOUT_BYTES];
            size_t n = 0;
            if (is_large) { memcpy(packed, layout, sizeof layout); n = sizeof layout; }
            else for (unsigned y = 0; y < ORACLES_SMALL_ROOM_HEIGHT; y++)
                for (unsigned x = 0; x < ORACLES_SMALL_ROOM_WIDTH; x++) packed[n++] = layout[y * 16 + x];
            snprintf(path, sizeof path, "%s/rooms/room%02x%02x.bin", out_dir, group, room);
            if (write_file(path, packed, n)) { room_errors++; continue; }
            rooms++;
        }
    }

    snprintf(path, sizeof path, "%s/tilesets", out_dir); mkdir_p(path);
    for (unsigned layout = 0; layout < t->num_tileset_layouts; layout++) {
        uint8_t mappings[ORACLES_TILESET_MAPPINGS_BYTES], collisions[ORACLES_TILESET_COLLISIONS_BYTES];
        int have_mappings = 0, have_collisions = 0;
        if (oracles_decode_tileset_layout(&rom, t, layout, mappings, collisions, &have_mappings, &have_collisions)) { tileset_errors++; continue; }
        if (have_mappings) {
            snprintf(path, sizeof path, "%s/tilesets/tilesetMappings%02x.bin", out_dir, layout);
            if (write_file(path, mappings, sizeof mappings)) tileset_errors++; else tilesets++;
        }
        if (have_collisions) {
            snprintf(path, sizeof path, "%s/tilesets/tilesetCollisions%02x.bin", out_dir, layout);
            if (write_file(path, collisions, sizeof collisions)) tileset_errors++; else tilesets++;
        }
    }

    snprintf(path, sizeof path, "%s/gfx", out_dir); mkdir_p(path);
    snprintf(path, sizeof path, "%s/gfx/manifest.tsv", out_dir);
    FILE *manifest = fopen(path, "w");
    if (!manifest) { perror(path); return 1; }
    static uint8_t buffer[64 * 1024];
    for (unsigned header = 0; header < t->num_gfx_headers; header++) {
        for (unsigned entry = 0; entry < 64; entry++) {
            OraclesGfxEntry e;
            if (oracles_gfx_header_entry(&rom, t, header, entry, &e)) { gfx_errors++; break; }
            const long n = oracles_decompress_gfx(&rom, e.src_bank, e.src_addr, e.mode, e.size_byte, buffer, sizeof buffer);
            if (n < 0) { gfx_errors++; if (!e.has_next) break; continue; }
            snprintf(path, sizeof path, "%s/gfx/h%02x-e%02u.bin", out_dir, header, entry);
            if (write_file(path, buffer, (size_t)n)) gfx_errors++; else gfx++;
            fprintf(manifest, "%02x\t%u\t%02x\t%04x\t%u\t%02x\t%ld\n", header, entry, e.src_bank, e.src_addr, e.mode, e.size_byte, n);
            if (!e.has_next) break;
        }
    }
    fclose(manifest);
    printf("rooms=%u room_errors=%u tilesets=%u tileset_errors=%u gfx=%u gfx_errors=%u\n",
           rooms, room_errors, tilesets, tileset_errors, gfx, gfx_errors);
    free(data);
    return 0;
}
