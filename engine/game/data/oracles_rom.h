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
    /* The prediction of the collisions of the room a scroll enters (docs/ROM_DATA_FORMATS.md, section 17): the tables
     * applyAllTileSubstitutions reads, the lists of the rooms whose own code changes their tiles before
     * (applyRoomSpecificTileChanges) and after the graphics (applyRoomSpecificTileChangesAfterGfxLoad), the Gasha
     * spots' rooms, and Ages' time warps' replacements ({ 0, 0 } in Seasons). */
    OraclesSym single_tile_change_group_table;
    OraclesSym standard_tile_substitutions;
    OraclesSym chest_data_group_table;
    OraclesSym room_tile_changer_code_group_table;
    OraclesSym tile_changes_after_gfx_group_table;
    OraclesSym gasha_spot_rooms;
    OraclesSym timewarp_entry_tile_replacement_dict;
    OraclesSym timewarp_return_tile_replacement_dict;
    /* replacePollutionWithWaterIfPollutionFixed's two lists, above and under water (Ages' engine; { 0, 0 } in
     * Seasons), and replaceOpenedChest's rooms of group 0 that keep their chest closed, where an image has them. */
    OraclesSym pollution_above_water;
    OraclesSym pollution_below_water;
    OraclesSym opened_chest_exceptions;
    /* The expanded tilesets (docs/ROM_DATA_FORMATS.md, section 3.5), where an image replaces the tileset layouts with
     * them: the tilesets' mappings and collisions (its large rooms then stored raw, section 2.3), and loadRoomLayout's
     * layout group of each group; { 0, 0 } in the original games. */
    OraclesSym expanded_tileset_mappings_table;
    OraclesSym layout_group_table;
    /* The continuous transitions' swim: an object's move a frame by its speed and angle (getPositionOffsetForVelocity:
     * $50 bytes a row of 5 speed units, each row the 8.8 moves along y of the 32 angles then 8 more, x read 16 bytes
     * on). */
    OraclesSym object_speed_table;
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

/* ---- address spaces ----------------------------------------------------- */

/* An image of two games side by side (Oracles Combined: Ages in banks $000-$0ff, Seasons in $100-$1ff, the MBC5's ninth
 * bank bit choosing the half): each half is a game of its own, its symbols in banks $00-$ff relative to it.  A half's
 * reader sees it alone, from its bank 0, never more than ORACLES_ROM_HALF_SIZE; the other images are a single half,
 * their banks based at 0, and see the whole image as before (none is larger than a half). */
#define ORACLES_ROM_HALF_SIZE (4u * 1024u * 1024u)

static inline const uint8_t *oracles_rom_half(const uint8_t *image, size_t size, unsigned rom_bank_base, size_t *half_size)
{
    const size_t start = (size_t)rom_bank_base * 0x4000u;
    if (!image || start >= size) { if (half_size) *half_size = 0; return NULL; }
    const size_t left = size - start;
    if (half_size) *half_size = left < ORACLES_ROM_HALF_SIZE ? left : ORACLES_ROM_HALF_SIZE;
    return image + start;
}

/* Code that runs from the cartridge's RAM: a game that copies a ROM bank of its own there and executes it at
 * $a000-$bfff (Oracles Combined's fillBootstrappedSram: bank $41 of the half, into the RAM's bank 1).  Its symbols
 * name that ROM bank, at their address in $a000-$bfff; the copy's bytes are the bank's first 8 KiB. */
#define ORACLES_SRAM_CODE_START 0xa000u
#define ORACLES_SRAM_CODE_END 0xc000u
/* The bank it is a copy of: a code address there read in the ROM (a call's target, which names no bank). */
#define ORACLES_SRAM_CODE_BANK 0x41u

static inline int oracles_rom_in_sram(unsigned addr) { return addr >= ORACLES_SRAM_CODE_START && addr < ORACLES_SRAM_CODE_END; }

/* The offset of a code address of `bank` in the image, and the end of the region its code runs on in (bank 0, its
 * bank, or the RAM's copy of its bank); 0 when the address is not one of code (WRAM, HRAM, I/O). */
static inline int oracles_rom_code_offset(unsigned bank, unsigned addr, size_t *offset, size_t *end)
{
    if (addr < 0x4000u) { *offset = addr; *end = 0x4000u; return 1; }
    if (addr < 0x8000u) { *offset = (size_t)bank * 0x4000u + (addr - 0x4000u); *end = (size_t)bank * 0x4000u + 0x4000u; return 1; }
    if (oracles_rom_in_sram(addr)) {
        *offset = (size_t)bank * 0x4000u + (addr - ORACLES_SRAM_CODE_START);
        *end = (size_t)bank * 0x4000u + (ORACLES_SRAM_CODE_END - ORACLES_SRAM_CODE_START);
        return 1;
    }
    return 0;
}

/* Reads through the sequential-read convention of the game: a stream that
 * crosses 0x7fff continues at 0x4000 of the next bank, i.e. physical offsets
 * simply increase. */
int oracles_rom_read8(const OraclesRom *rom, size_t offset, uint8_t *out);
int oracles_rom_read16le(const OraclesRom *rom, size_t offset, unsigned *out);
int oracles_rom_read16be(const OraclesRom *rom, size_t offset, unsigned *out);

/* ---- the extended image ----------------------------------------------- */

/* An image composed for the mods' houses or a language is 2 MiB (docs/ROM_DATA_FORMATS.md, section 16): the houses
 * take the extension from bank $40 up to ORACLES_IMAGE_LANGUAGE_BANK, excluded; a language takes the banks from it to
 * the end, its index at the start of the first. */
#define ORACLES_IMAGE_SIZE (2u * 1024u * 1024u)
#define ORACLES_IMAGE_LANGUAGE_BANK 0x70u
/* The language's index: the magic, a version byte, the number of internal text groups, the language's code ("fr",
 * "en-eu": ORACLES_LANGUAGE_INDEX_CODE_BYTES, its characters then zeros);
 * from ORACLES_LANGUAGE_INDEX_GROUPS, 4 bytes a group: the offset of its entries from the bank's start and their
 * number (16 bits each); an entry, 3 bytes a text: the bank and the CPU address of the language's text, or bank 0 for
 * a text the game reads in English: address ORACLES_LANGUAGE_KEPT when the language keeps it, 0 when it has none. */
#define ORACLES_LANGUAGE_INDEX_MAGIC "ORACLES LANGUAGE"
#define ORACLES_LANGUAGE_INDEX_MAGIC_BYTES 16u
#define ORACLES_LANGUAGE_INDEX_VERSION 2u
#define ORACLES_LANGUAGE_INDEX_CODE 18u
#define ORACLES_LANGUAGE_INDEX_CODE_BYTES 8u
#define ORACLES_LANGUAGE_INDEX_GROUPS 32u
#define ORACLES_LANGUAGE_KEPT 1u
#define ORACLES_LANGUAGE_LAID_OUT 0x8000u   /* in an entry's address: the text laid out again for the compact text */

/* A composed image's header: its size byte (6 for 2 MiB, 7 for 4 MiB), and its two sums. */
void oracles_image_finish(uint8_t *image, size_t size);

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

/* The collisions of tileset `index` (bits 0-6 read) in the expanded tilesets, `season` (wRoomStateModifier, the whole
 * byte) choosing a seasonal one's entry; `*seasonal`, unless NULL, whether they depend on the season (a seasonal
 * tileset whose four seasons' collisions differ, or a season past the four).  ORACLES_ERR_FORMAT where the image has
 * none, or an entry leaves its bank. */
int oracles_expanded_tileset_collisions(const OraclesRom *rom, const OraclesTables *t, unsigned index, unsigned season,
                                        uint8_t collisions[ORACLES_TILESET_COLLISIONS_BYTES], int *seasonal);

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
