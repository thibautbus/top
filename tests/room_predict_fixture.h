/* A ROM of the tables the prediction of a scroll's room reads, written by the tests in bank 1 of a buffer: a group
 * table of small rooms whose layouts are stored raw, one tileset layout whose collisions are stored as literals, and
 * the lists of the substitutions and of the rooms with their own code, each test adding its entries. */
#ifndef ORACLES_TEST_ROOM_PREDICT_FIXTURE_H
#define ORACLES_TEST_ROOM_PREDICT_FIXTURE_H

#include "oracles_rom.h"

#include <string.h>

#define FIXTURE_BANK 1u
#define FIXTURE_ROM_SIZE (4u * 0x4000u)

typedef struct fixture {
    uint8_t *rom;
    OraclesTables tables;
    unsigned next;               /* the next free address of bank 1 */
} fixture;

static inline void fixture_put(fixture *f, unsigned addr, uint8_t value) { f->rom[FIXTURE_BANK * 0x4000u + (addr - 0x4000u)] = value; }
static inline void fixture_word(fixture *f, unsigned addr, unsigned value) { fixture_put(f, addr, (uint8_t)value); fixture_put(f, addr + 1u, (uint8_t)(value >> 8)); }

/* `count` bytes at the next free address; their address. */
static inline unsigned fixture_bytes(fixture *f, const uint8_t *bytes, unsigned count)
{
    const unsigned at = f->next;
    for (unsigned i = 0; i < count; i++) fixture_put(f, at + i, bytes[i]);
    f->next += count;
    return at;
}

/* A list for each of the eight groups of a group table at `table`, each `empty` (its terminator) but group `group`'s. */
static inline void fixture_group_lists(fixture *f, unsigned table, const uint8_t *empty, unsigned empty_size, unsigned group, const uint8_t *list, unsigned size)
{
    const unsigned none = fixture_bytes(f, empty, empty_size);
    for (unsigned g = 0; g < 8u; g++) fixture_word(f, table + 2u * g, none);
    if (list) fixture_word(f, table + 2u * group, fixture_bytes(f, list, size));
}

/* The tables, every list empty; the rooms' layouts all tile 0 and the tileset's collisions all 0 until set. */
static inline void fixture_init(fixture *f, uint8_t *rom)
{
    memset(rom, 0, FIXTURE_ROM_SIZE);
    memset(f, 0, sizeof *f);
    f->rom = rom;
    OraclesTables *t = &f->tables;
    t->room_layout_group_table = (OraclesSym){ FIXTURE_BANK, 0x4000 };   /* 8 groups x 8 bytes */
    t->tileset_layout_table = (OraclesSym){ FIXTURE_BANK, 0x4040 };
    t->tileset_layout_dictionary_table = (OraclesSym){ FIXTURE_BANK, 0x4042 };
    t->single_tile_change_group_table = (OraclesSym){ FIXTURE_BANK, 0x4050 };
    t->standard_tile_substitutions = (OraclesSym){ FIXTURE_BANK, 0x4060 };   /* 5 bits x up to 8 lists */
    t->chest_data_group_table = (OraclesSym){ FIXTURE_BANK, 0x40b0 };
    t->room_tile_changer_code_group_table = (OraclesSym){ FIXTURE_BANK, 0x40c0 };
    t->tile_changes_after_gfx_group_table = (OraclesSym){ FIXTURE_BANK, 0x40d0 };
    t->gasha_spot_rooms = (OraclesSym){ FIXTURE_BANK, 0x40e0 };
    t->timewarp_entry_tile_replacement_dict = (OraclesSym){ FIXTURE_BANK, 0x40f0 };
    t->timewarp_return_tile_replacement_dict = (OraclesSym){ FIXTURE_BANK, 0x40f8 };
    t->num_tileset_layouts = 64;
    f->next = 0x4400;
    /* Every group small (mode 1), its rooms' words at $4400 (raw, mode 0), their bytes from $4600: room r at $4600 + 80 r
     * for the rooms below 64. */
    for (unsigned g = 0; g < 8u; g++) {
        const unsigned e = 0x4000u + 8u * g;
        fixture_put(f, e, 1); fixture_put(f, e + 1u, FIXTURE_BANK); fixture_word(f, e + 2u, 0x4400);
        fixture_put(f, e + 4u, FIXTURE_BANK); fixture_word(f, e + 5u, 0x4600);
    }
    for (unsigned r = 0; r < 64u; r++) fixture_word(f, 0x4400u + 2u * r, 80u * r);
    /* Tileset layout 0: one header, its collisions (to $db00, 256 bytes) as 32 runs of a flag byte and 8 literals at
     * $5a00, dictionary 0 short references in bank 1. */
    fixture_word(f, 0x4040, 0x4048);
    fixture_word(f, 0x4042, 0x4044);
    fixture_put(f, 0x4044, FIXTURE_BANK); fixture_put(f, 0x4045, 0x50); fixture_put(f, 0x4046, 0x00);
    const uint8_t header[8] = { 0x00, FIXTURE_BANK, 0x5a, 0x00, 0xdb, 0x00, 0x01, 0x00 };
    for (unsigned i = 0; i < 8u; i++) fixture_put(f, 0x4048u + i, header[i]);
    f->next = 0x5c40;
    static const uint8_t pair_end[2] = { 0, 0 }, chest_end[1] = { 0xff }, key_end[1] = { 0 };
    fixture_group_lists(f, t->single_tile_change_group_table.addr, pair_end, 2, 0, NULL, 0);
    const unsigned none = fixture_bytes(f, key_end, 1);
    for (unsigned i = 0; i < 40u; i++) fixture_word(f, t->standard_tile_substitutions.addr + 2u * i, none);
    fixture_group_lists(f, t->chest_data_group_table.addr, chest_end, 1, 0, NULL, 0);
    fixture_group_lists(f, t->room_tile_changer_code_group_table.addr, key_end, 1, 0, NULL, 0);
    fixture_group_lists(f, t->tile_changes_after_gfx_group_table.addr, key_end, 1, 0, NULL, 0);
    /* Every room's tileset 0 (roomTilesetsGroupTable at $5d00, each group's 256 bytes at $5d10), outdoors, tileset
     * layout 0 of layout group 0 (tilesetData at $5e10); tileset 1 a dungeon's. */
    t->room_tilesets_group_table = (OraclesSym){ FIXTURE_BANK, 0x5d00 };
    t->tileset_data = (OraclesSym){ FIXTURE_BANK, 0x5e10 };
    for (unsigned g = 0; g < 8u; g++) fixture_word(f, 0x5d00u + 2u * g, 0x5d10);
    fixture_put(f, 0x5e10, 0x0f);   /* no dungeon (Seasons reads the low nibble) */
    fixture_put(f, 0x5e11, 0x01);
    fixture_put(f, 0x5e19, 0x08);
}

/* The room packs of the groups 0 and 1 (roomPackData, its 512 bytes at FIXTURE_ROOM_PACKS), every room's 0 until set. */
#define FIXTURE_ROOM_PACKS 0x5f20u

/* Room `room`'s tile at (col, row) of its 10 x 8. */
static inline void fixture_room_tile(fixture *f, unsigned room, unsigned col, unsigned row, uint8_t tile) { fixture_put(f, 0x4600u + 80u * room + row * 10u + col, tile); }

/* The collision of `tile` in tileset layout 0. */
static inline void fixture_collision(fixture *f, uint8_t tile, uint8_t collision) { fixture_put(f, 0x5a00u + 9u * (tile >> 3) + 1u + (tile & 7u), collision); }

#endif
