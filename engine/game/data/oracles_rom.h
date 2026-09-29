/* Native readers for the data of Oracle of Ages / Seasons (US).
 *
 * Every function reads the user's ROM only.  Formats are documented in
 * docs/ROM_DATA_FORMATS.md; table addresses come from the generated
 * oracles_tables_<game>.c, never from this source.
 */
#ifndef ORACLES_DATA_ROM_H
#define ORACLES_DATA_ROM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OraclesRom {
    const uint8_t *data;
    size_t size;
} OraclesRom;

typedef struct OraclesSym {
    unsigned bank;
    unsigned addr;
} OraclesSym;

typedef struct OraclesTables {
    OraclesSym room_layout_group_table;
    OraclesSym tileset_data;
    OraclesSym tileset_layout_table;
    OraclesSym tileset_layout_dictionary_table;
    OraclesSym tile_mapping_table;
    OraclesSym tile_mapping_index_data_pointer;
    OraclesSym tile_mapping_attribute_data_pointer;
    OraclesSym gfx_header_table;
    OraclesSym unique_gfx_headers_start;
    OraclesSym room_tilesets_group_table;
    OraclesSym palette_header_table;
    OraclesSym animation_group_table;
    OraclesSym animation_gfx_headers;
    OraclesSym object_gfx_header_table;
    /* A mod's composition: warp sources and destinations (bank 04), the objects' group table, the music of
     * each room, and parseObjectData, whose bank is the one the object streams are read in. */
    OraclesSym warp_sources_table;
    OraclesSym warp_dest_table;
    OraclesSym object_data_group_table;
    OraclesSym music_assignment_group_table;
    OraclesSym parse_object_data;
    /* Where the free tail of the warp lists' bank and of the object streams' bank begins (the end of the bank's last
     * section, from the symbol file's map); it runs to $7fff. */
    OraclesSym warp_bank_free;
    OraclesSym object_bank_free;
    unsigned num_gfx_headers;
    unsigned num_unique_gfx_headers;
    unsigned num_palette_headers;
    unsigned num_tileset_layouts;
} OraclesTables;

extern const OraclesTables oracles_tables_ages;
extern const OraclesTables oracles_tables_seasons;

enum {
    ORACLES_OK = 0,
    ORACLES_ERR_RANGE = -1,     /* a read left the ROM */
    ORACLES_ERR_FORMAT = -2,    /* data inconsistent with the documented format */
    ORACLES_ERR_CAPACITY = -3   /* output buffer too small */
};

/* Physical offset of a CPU address in a ROM bank (bank 0 covers 0x0000-0x3fff). */
static inline size_t oracles_rom_offset(unsigned bank, unsigned addr)
{
    return bank ? (size_t)bank * 0x4000u + (addr - 0x4000u) : (size_t)addr;
}

/* Reads through the sequential-read convention of the game: a stream that
 * crosses 0x7fff continues at 0x4000 of the next bank, i.e. physical offsets
 * simply increase. */
int oracles_rom_read8(const OraclesRom *rom, size_t offset, uint8_t *out);
int oracles_rom_read16le(const OraclesRom *rom, size_t offset, unsigned *out);
int oracles_rom_read16be(const OraclesRom *rom, size_t offset, unsigned *out);

/* ---- rooms ------------------------------------------------------------ */

#define ORACLES_ROOM_LAYOUT_BYTES 176u   /* 11 rows of 16, row stride 16 in RAM */
#define ORACLES_SMALL_ROOM_WIDTH 10u
#define ORACLES_SMALL_ROOM_HEIGHT 8u
#define ORACLES_LARGE_ROOM_WIDTH 15u
#define ORACLES_LARGE_ROOM_HEIGHT 11u

/* Decodes the layout of room `room` of group `group` into `out`, laid out as
 * the game keeps it in RAM (stride 16).  Rows beyond the room height are left
 * untouched.  *is_large reports the group's room size. */
int oracles_decode_room_layout(const OraclesRom *rom, const OraclesTables *t,
                               unsigned group, unsigned room,
                               uint8_t out[ORACLES_ROOM_LAYOUT_BYTES], int *is_large);

/* Tileset index of a room (roomTilesetsGroupTable). */
int oracles_room_tileset(const OraclesRom *rom, const OraclesTables *t,
                         unsigned group, unsigned room, uint8_t *tileset);

/* ---- tilesets --------------------------------------------------------- */

#define ORACLES_TILESET_MAPPINGS_BYTES 2048u  /* 256 metatiles x (4 tile indices + 4 attributes) */
#define ORACLES_TILESET_COLLISIONS_BYTES 256u

typedef struct OraclesTilesetEntry {
    uint8_t collisions_mode;   /* bits 4-7 of byte 0 */
    uint8_t dungeon_index;     /* bits 0-3 of byte 0, 0xf = none */
    uint8_t flags;
    uint8_t unique_gfx;
    uint8_t gfx;
    uint8_t palette;
    uint8_t layout;
    uint8_t layout_group;
    uint8_t animation;
} OraclesTilesetEntry;

/* Reads the 8-byte tileset definition.  Seasons: `season` selects the entry of
 * a seasonal tileset (0 spring .. 3 winter); ignored for non-seasonal ones. */
int oracles_read_tileset(const OraclesRom *rom, const OraclesTables *t, unsigned index,
                         unsigned season, OraclesTilesetEntry *out);

/* Decodes the mappings (expanded to 8 bytes per metatile) and collisions of a
 * tileset layout index.  A NULL output skips that part.  *have_* report which
 * parts the header list provides. */
int oracles_decode_tileset_layout(const OraclesRom *rom, const OraclesTables *t, unsigned layout,
                                  uint8_t *mappings, uint8_t *collisions,
                                  int *have_mappings, int *have_collisions);

/* ---- graphics --------------------------------------------------------- */

typedef struct OraclesGfxEntry {
    unsigned src_bank;
    unsigned src_addr;
    unsigned mode;        /* 0 raw, 1 short LZ, 2 common byte, 3 long LZ */
    unsigned dest_addr;   /* multiple of 16 */
    unsigned dest_bank;   /* VRAM or WRAM bank */
    unsigned size_byte;   /* blocks of 16 bytes minus 1 */
    int has_next;
} OraclesGfxEntry;

/* Entry `entry` of graphics header `header` (gfxHeaderTable). */
int oracles_gfx_header_entry(const OraclesRom *rom, const OraclesTables *t,
                             unsigned header, unsigned entry, OraclesGfxEntry *out);

/* Decompresses graphics as decompressGraphics does.  Returns the number of
 * bytes written, or a negative error. */
long oracles_decompress_gfx(const OraclesRom *rom, unsigned src_bank, unsigned src_addr,
                            unsigned mode, unsigned size_byte, uint8_t *out, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
