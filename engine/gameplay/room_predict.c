#include "room_predict.h"

#include <string.h>

#define SCROLL_MODE_LOADED 8u            /* cutscene01 writes it after loadTilesetData and before loadTilesetAndRoomLayout */
#define SMALL_GROUPS 4u                  /* NUM_SMALL_GROUPS: groups 4 to 7 are the dungeons' and the side views' */
#define TILESETFLAG_OUTDOORS 0x01u
#define SINGLE_TILE_UNLINKED 0xf0u       /* applySingleTileChanges' masks of Ages' engine */
#define SINGLE_TILE_LINKED 0xf1u
#define SINGLE_TILE_FINISHED 0xf2u
#define STANDARD_LISTS_AGES 6u           /* standardTileSubstitutions: a list per collision mode in Ages, per group in Seasons */
#define STANDARD_LISTS_SEASONS 8u
#define LIST_LIMIT 256u                  /* entries read at most in a list before it is taken as unreadable */

/* applyRoomSpecificTileChangesAfterGfxLoad's routines that draw alone (w3VramTiles, graphics, wInShop,
 * wDiggingUpEnemiesForbidden), by bit of their index; those that write the layout and the collisions only under a
 * condition, the room's flags or a global flag, tested first: the Gasha spot's where something is planted, Ages' $00
 * (dungeon 2's entrance collapsed: bit 7), Seasons' $00 and $01 (the pirate ship docked), $03 (Din's troupe there:
 * bit 6 clear) and $0e (the wagon until the intro is done); any other writes them (roomGfxChanges.s of each game). */
#define AFTER_GFX_DRAWN_AGES 0x029eu     /* $01 to $04 the trees and the shops, $07 Crown Dungeon's entrance, $09 the mermaid statue */
#define AFTER_GFX_DRAWN_SEASONS 0xbef0u  /* $04 to $07 the shops, King Moblin's house, Blaino's gym, Vasu's sign, $09 to $0d the trees, $0f */
#define AFTER_GFX_GASHA 0x08u
#define AFTER_GFX_WING_DUNGEON 0x00u     /* Ages */
#define AFTER_GFX_PIRATE_SHIP_BOW 0x00u  /* Seasons */
#define AFTER_GFX_PIRATE_SHIP_MIDDLE 0x01u
#define AFTER_GFX_DINS_TROUPE 0x03u
#define AFTER_GFX_WAGON 0x0eu
#define AFTER_GFX_ROUTINES_AGES 0x0bu
#define AFTER_GFX_ROUTINES_SEASONS 0x10u

static int read8(const OraclesRom *rom, unsigned bank, unsigned addr, uint8_t *out)
{
    if (addr < 0x4000u || addr >= 0x8000u) return 0;
    return oracles_rom_read8(rom, oracles_rom_offset(bank, addr), out) == ORACLES_OK;
}

static int read16(const OraclesRom *rom, unsigned bank, unsigned addr, unsigned *out)
{
    if (addr < 0x4000u || addr >= 0x7fffu) return 0;
    return oracles_rom_read16le(rom, oracles_rom_offset(bank, addr), out) == ORACLES_OK;
}

static int bound(OraclesSym sym) { return sym.addr != 0; }

/* A byte of a table of bank 0 or of a switchable bank. */
static int read8_any(const OraclesRom *rom, OraclesSym table, unsigned index, uint8_t *out)
{
    return oracles_rom_read8(rom, oracles_rom_offset(table.bank, table.addr + index), out) == ORACLES_OK;
}

static int flag_set(const uint8_t *flags, unsigned size, uint8_t flag)
{
    return flag != 0xffu && (unsigned)(flag >> 3) < size && flags && ((flags[flag >> 3] >> (flag & 7u)) & 1u);
}

/* A byte of the pages of room flags (getRoomFlags, or a room's by name); 0 when they are not given. */
static int flags_byte(const OraclesGuestTransitionState *s, unsigned at, uint8_t *out)
{
    if (!s->load.room_flags_all || at >= s->load.room_flags_all_size) return 0;
    *out = s->load.room_flags_all[at];
    return 1;
}

/* findRoomSpecificData: the group's list of pairs of room and value, ended by a room $00; -1 when the room is not in
 * it, -2 when the list cannot be read. */
static int room_value(const OraclesRom *rom, OraclesSym table, unsigned group, unsigned room)
{
    unsigned list = 0;
    if (!read16(rom, table.bank, table.addr + 2u * group, &list)) return -2;
    for (unsigned i = 0; i < LIST_LIMIT; i++, list += 2u) {
        uint8_t key = 0, value = 0;
        if (!read8(rom, table.bank, list, &key)) return -2;
        if (key == 0u) return -1;
        if (!read8(rom, table.bank, list + 1u, &value)) return -2;
        if (key == room) return value;
    }
    return -2;
}

/* replaceTiles: pairs of the tile to put and the tile it replaces, everywhere in the room's 176 bytes, ended by $00. */
static int replace_tiles(const OraclesRom *rom, unsigned bank, unsigned list, uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES])
{
    for (unsigned i = 0; i < LIST_LIMIT; i++, list += 2u) {
        uint8_t put = 0, old = 0;
        if (!read8(rom, bank, list, &put)) return 0;
        if (put == 0u) return 1;
        if (!read8(rom, bank, list + 1u, &old)) return 0;
        for (unsigned k = 0; k < ORACLES_ROOM_LAYOUT_BYTES; k++)
            if (layout[k] == old) layout[k] = put;
    }
    return 0;
}

static void replace_pair(uint8_t put, uint8_t old, uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES])
{
    for (unsigned k = 0; k < ORACLES_ROOM_LAYOUT_BYTES; k++)
        if (layout[k] == old) layout[k] = put;
}

/* lookupKey on a dictionary of pairs of key and value ended by a key $00: the tile at `pos` replaced by its value. */
static int replace_by_key(const OraclesRom *rom, OraclesSym dict, unsigned pos, uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES])
{
    if (!bound(dict) || pos >= ORACLES_ROOM_LAYOUT_BYTES) return 0;
    unsigned at = dict.addr;
    for (unsigned i = 0; i < LIST_LIMIT; i++, at += 2u) {
        uint8_t key = 0, value = 0;
        if (!read8(rom, dict.bank, at, &key)) return 0;
        if (key == 0u) return 1;
        if (!read8(rom, dict.bank, at + 1u, &value)) return 0;
        if (key == layout[pos]) { layout[pos] = value; return 1; }
    }
    return 0;
}

/* applySingleTileChanges (commonTileSubstitutions.s): the group's entries of room, mask of the room's flags (in Ages'
 * engine $f0 unlinked, $f1 linked, $f2 the game finished, whose absence ends the list), position and tile. */
static int single_tile_changes(const OraclesRom *rom, const OraclesTables *data, const OraclesGuestTransitionState *s, uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES])
{
    const OraclesSym t = data->single_tile_change_group_table;
    unsigned at = 0;
    if (!read16(rom, t.bank, t.addr + 2u * s->active_group, &at)) return 0;
    for (unsigned i = 0; i < LIST_LIMIT; i++) {
        uint8_t room = 0, mask = 0, pos = 0, tile = 0;
        if (!read8(rom, t.bank, at, &room) || !read8(rom, t.bank, at + 1u, &mask)) return 0;
        int match = room == s->active_room && (mask & s->load.room_flags) != 0;
        if (room == s->active_room && s->game == ORACLES_GAME_AGES) {
            if (mask == SINGLE_TILE_UNLINKED) match = !s->load.is_linked_game;
            else if (mask == SINGLE_TILE_LINKED) match = s->load.is_linked_game != 0;
            else if (mask == SINGLE_TILE_FINISHED) {
                if (s->load.finished_game_flag == 0xffu) return 0;
                if (!flag_set(s->load.global_flags, s->load.global_flags_size, s->load.finished_game_flag)) return 1;
                match = 1;
            }
        }
        if (match) {
            if (!read8(rom, t.bank, at + 2u, &pos) || !read8(rom, t.bank, at + 3u, &tile)) return 0;
            if (pos >= ORACLES_ROOM_LAYOUT_BYTES) return 0;
            layout[pos] = tile;
        } else if (mask == 0u) return 1;
        at += 4u;
    }
    return 0;
}

/* applyStandardTileSubstitutions: for bits 0 to 3 and 7 of the room's flags, the list of that bit for the collision
 * mode (Ages) or the group (Seasons). */
static int standard_substitutions(const OraclesRom *rom, const OraclesTables *data, const OraclesGuestTransitionState *s, uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES])
{
    static const uint8_t bits[5] = { 0, 1, 2, 3, 7 };
    const OraclesSym t = data->standard_tile_substitutions;
    const unsigned lists = s->game == ORACLES_GAME_AGES ? STANDARD_LISTS_AGES : STANDARD_LISTS_SEASONS;
    const unsigned index = s->game == ORACLES_GAME_AGES ? s->active_collisions : s->active_group;
    for (unsigned b = 0; b < 5u; b++) {
        if (!((s->load.room_flags >> bits[b]) & 1u)) continue;
        if (index >= lists) return 0;
        unsigned list = 0;
        if (!read16(rom, t.bank, t.addr + 2u * (b * lists + index), &list) || !replace_tiles(rom, t.bank, list, layout)) return 0;
    }
    return 1;
}

#define CHEST_EXCEPTIONS_LIFTED 0x120u   /* replaceOpenedChest's exceptions: wPastRoomFlags+$20, its bit 0 (the layout swapped) */

/* replaceOpenedChest and getChestData: the group's entries of position, room and contents (two bytes), ended by a
 * position $ff; a room without one has its opened chest put past the layout.  Where the image has them, the rooms of
 * group 0 its exceptions list (findByteAtHl: their low bytes, ended by $00) keep their chest closed, unless bit 0 of
 * the past's room $20 is set. */
static int opened_chest(const OraclesRom *rom, const OraclesTables *data, const OraclesGuestTransitionState *s, uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES])
{
    if (s->load.roomflag_item == 0xffu || s->load.chest_opened_tile == 0xffu) return 0;
    if (!(s->load.room_flags & s->load.roomflag_item)) return 1;
    if (bound(data->opened_chest_exceptions) && s->active_group == 0u) {
        uint8_t lifted = 0;
        if (!flags_byte(s, CHEST_EXCEPTIONS_LIFTED, &lifted)) return 0;
        if (!(lifted & 0x01u))
            for (unsigned i = 0;; i++) {
                uint8_t room = 0;
                if (i == LIST_LIMIT || !read8(rom, data->opened_chest_exceptions.bank, data->opened_chest_exceptions.addr + i, &room)) return 0;
                if (room == 0u) break;
                if (room == s->active_room) return 1;
            }
    }
    const OraclesSym t = data->chest_data_group_table;
    unsigned at = 0;
    if (!read16(rom, t.bank, t.addr + 2u * s->active_group, &at)) return 0;
    for (unsigned i = 0; i < LIST_LIMIT; i++, at += 4u) {
        uint8_t pos = 0, room = 0;
        if (!read8(rom, t.bank, at, &pos)) return 0;
        if (pos == 0xffu) return 1;
        if (!read8(rom, t.bank, at + 1u, &room)) return 0;
        if (room != s->active_room) continue;
        if (pos < ORACLES_ROOM_LAYOUT_BYTES) layout[pos] = s->load.chest_opened_tile;
        return 1;
    }
    return 0;
}

/* applyRoomSpecificTileChanges (roomSpecificTileChanges.s of each game): the routines of the rooms of groups 0 to 3,
 * replayed.  The room's value in roomTileChangerCodeGroupTable is the routine's index in the jump table; each index
 * here belongs to one room, and is replayed for that room alone, with the bytes it reads besides the room flags.  Those
 * of groups 4 to 7 are never reached: their rooms are large or a dungeon's. */
#define READS_GLOBAL_FLAGS 0x100u          /* wGlobalFlags */
#define READS_TREASURES 0x200u             /* wObtainedTreasureFlags */
#define READS_GB ORACLES_LOAD_GAMEBOY_TYPE
#define READS_RICKY ORACLES_LOAD_RICKY_STATE
#define READS_ESSENCES ORACLES_LOAD_ESSENCES_OBTAINED
#define READS_COMPANION ORACLES_LOAD_ANIMAL_COMPANION
#define READS_BOUGHT ORACLES_LOAD_BOUGHT_SHOP_ITEMS1
#define READS_TREE ORACLES_LOAD_SEED_TREE_REFILLED

typedef struct RoomCode {
    uint8_t index, group, room;
    uint16_t reads;
} RoomCode;

static const RoomCode room_code_ages[] = {
    { 0x02, 2, 0x7e, READS_BOUGHT }, { 0x08, 0, 0x38, READS_GLOBAL_FLAGS }, { 0x09, 1, 0x38, 0 }, { 0x14, 0, 0x5c, 0 }, { 0x15, 2, 0xf7, READS_TREE },
    { 0x16, 0, 0x73, 0 }, { 0x17, 0, 0x48, READS_GLOBAL_FLAGS }, { 0x18, 0, 0xac, 0 }, { 0x19, 0, 0x2c, 0 }, { 0x1a, 0, 0x1c, 0 }, { 0x1b, 0, 0xba, 0 },
    { 0x1c, 0, 0xaa, 0 }, { 0x1d, 0, 0xcc, 0 }, { 0x1e, 0, 0xbc, 0 }, { 0x1f, 0, 0xda, 0 }, { 0x20, 0, 0xca, 0 }, { 0x21, 0, 0x61, 0 }, { 0x22, 0, 0x51, 0 },
    { 0x23, 0, 0x54, 0 }, { 0x24, 0, 0x25, READS_GLOBAL_FLAGS }, { 0x25, 0, 0x3a, READS_GLOBAL_FLAGS }, { 0x26, 0, 0x0b, 0 }, { 0x28, 1, 0x27, 0 },
    { 0x2b, 2, 0x90, 0 }, { 0x2c, 1, 0x8c, 0 }, { 0x2f, 2, 0x9e, 0 }, { 0x30, 0, 0xe0, READS_ESSENCES }, { 0x31, 0, 0xe1, READS_ESSENCES },
    { 0x32, 0, 0xe2, READS_ESSENCES }, { 0x34, 1, 0x58, READS_GB }, { 0x35, 0, 0x98, READS_RICKY | READS_TREASURES }, { 0x36, 0, 0x76, 0 }, { 0x37, 0, 0xa5, 0 },
};
static const RoomCode room_code_seasons[] = {
    { 0x00, 0, 0xc5, READS_GB }, { 0x01, 0, 0xd9, 0 }, { 0x03, 3, 0xb0, READS_BOUGHT }, { 0x08, 0, 0xf6, READS_GLOBAL_FLAGS }, { 0x10, 0, 0x54, READS_RICKY },
    { 0x11, 0, 0x7f, 0 }, { 0x12, 0, 0x62, 0 }, { 0x13, 0, 0x60, 0 }, { 0x14, 0, 0x61, 0 }, { 0x15, 0, 0x70, 0 }, { 0x16, 0, 0x71, 0 }, { 0x17, 0, 0x81, 0 },
    { 0x18, 0, 0x0d, 0 }, { 0x19, 0, 0x1d, 0 }, { 0x1c, 3, 0xb5, 0 }, { 0x1d, 0, 0x4b, READS_GLOBAL_FLAGS }, { 0x1e, 0, 0x63, 0 }, { 0x1f, 0, 0xf4, 0 },
    { 0x20, 0, 0x6f, READS_GLOBAL_FLAGS | READS_TREE }, { 0x21, 0, 0x42, 0 }, { 0x22, 0, 0xfc, 0 }, { 0x23, 1, 0x64, READS_GLOBAL_FLAGS },
    { 0x24, 1, 0x74, READS_GLOBAL_FLAGS }, { 0x25, 0, 0xee, READS_GLOBAL_FLAGS }, { 0x26, 0, 0xe4, 0 }, { 0x27, 1, 0x35, 0 }, { 0x28, 0, 0x56, READS_COMPANION },
    { 0x29, 3, 0xa4, 0 }, { 0x2a, 3, 0xab, 0 },
};

#define AGES_FLAG_INTRO_DONE 0x0au          /* the routines' global flags (globalFlags.s) */
#define AGES_FLAG_MAKU_TREE_SAVED 0x12u
#define AGES_FLAG_SYMMETRY_BRIDGE_BUILT 0x25u
#define SEASONS_FLAG_MOBLIN_HOUSE_REPAIRED 0x12u
#define SEASONS_FLAG_MOBLINS_KEEP_DESTROYED 0x16u
#define SEASONS_FLAG_PIRATE_SHIP_DOCKED 0x17u
#define SEASONS_FLAG_FINISHED_GAME 0x28u
#define AGES_TREASURE_RICKY_GLOVES 0x48u
#define SEASONS_DIMITRI 0x0cu               /* SPECIALOBJECT_DIMITRI, wAnimalCompanion */
#define SEASONS_WINTER 0x03u
#define ROOM_FLAGS_PAGE 0x100u              /* wGroup0RoomFlags, then groups 1 (Ages' past, Seasons' Subrosia), 4 and 5 */
#define AGES_VINE_POSITIONS 0x1f0u          /* wVinePositions: wPastRoomFlags+$f0, from wGroup0RoomFlags */

#define GLOBAL_FLAGS_READ 6u                /* the bytes of wGlobalFlags the routines' flags lie in, $28 the last */

/* The routine `index` of the room, where the image's code is the original game's (`room_code_replay`, the US profiles:
 * a fan game's or a hack's routines may differ) and every byte it reads is given; NULL otherwise. */
static const RoomCode *room_code_known(const OraclesGuestTransitionState *s, unsigned index, unsigned group, unsigned room)
{
    if (!s->load.room_code_replay) return NULL;
    const int ages = s->game == ORACLES_GAME_AGES;
    const RoomCode *codes = ages ? room_code_ages : room_code_seasons;
    const unsigned count = ages ? sizeof room_code_ages / sizeof *codes : sizeof room_code_seasons / sizeof *codes;
    for (unsigned i = 0; i < count; i++) {
        if (codes[i].index != index) continue;
        if (codes[i].group != group || codes[i].room != room) return NULL;
        const unsigned reads = codes[i].reads;
        if ((reads & 0xffu & ~(unsigned)s->load.given)
            || ((reads & READS_GLOBAL_FLAGS) && (!s->load.global_flags || s->load.global_flags_size < GLOBAL_FLAGS_READ))
            || ((reads & READS_TREASURES) && (!s->load.obtained_treasures || s->load.obtained_treasures_size * 8u <= AGES_TREASURE_RICKY_GLOVES)))
            return NULL;
        return &codes[i];
    }
    return NULL;
}

static void put(uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES], unsigned pos, uint8_t tile)
{
    if (pos < ORACLES_ROOM_LAYOUT_BYTES) layout[pos] = tile;
}

/* fillRectInRoomLayout: `rows` rows of `cols` tiles from `pos`. */
static void fill(uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES], unsigned pos, unsigned rows, unsigned cols, uint8_t tile)
{
    for (unsigned r = 0; r < rows; r++)
        for (unsigned c = 0; c < cols; c++) put(layout, pos + 16u * r + c, tile);
}

/* drawRectInRoomLayout and the routines' own loops: `rows` rows of `cols` tiles from `pos`, read in `tiles`. */
static void draw(uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES], unsigned pos, unsigned rows, unsigned cols, const uint8_t *tiles)
{
    for (unsigned r = 0; r < rows; r++)
        for (unsigned c = 0; c < cols; c++) put(layout, pos + 16u * r + c, tiles[r * cols + c]);
}

static int global(const OraclesGuestTransitionState *s, uint8_t flag) { return flag_set(s->load.global_flags, s->load.global_flags_size, flag); }

/* setTileToWitheredVine: the tile at the vine's position, where its collision is 0, a withered vine. */
static void withered_vine(const uint8_t table[ORACLES_TILESET_COLLISIONS_BYTES], unsigned pos, uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES])
{
    if (pos < ORACLES_ROOM_LAYOUT_BYTES && table[layout[pos]] == 0u) layout[pos] = 0x8cu;
}

/* replaceVineTiles: the sides of the vine, a pair a row from `pos` two columns apart; the vine's tiles put for their
 * placeholders; the tile below its base ($d6, the last in the room) completed. */
static void vine_tiles(unsigned pos, const uint8_t *sides, unsigned rows, uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES])
{
    for (unsigned r = 0; r < rows; r++, pos = (pos + 16u) & 0xffu) {
        put(layout, pos, sides[2u * r]);
        put(layout, (pos + 2u) & 0xffu, sides[2u * r + 1u]);
    }
    replace_pair(0xd4, 0x05, layout);
    replace_pair(0xd5, 0x8e, layout);
    replace_pair(0xd6, 0x8f, layout);
    for (unsigned k = ORACLES_ROOM_LAYOUT_BYTES; k-- > 0;)
        if (layout[k] == 0xd6u) { put(layout, k + 16u, 0x8du); break; }
}

/* The routine `index`, known to be the room's, on `layout`; ORACLES_ROOM_PREDICT_ROOM_CODE where a byte it reads is
 * not given.  `table`, the tileset's collisions (w3TileCollisions, loaded before), may be NULL where none reads it. */
static OraclesRoomPredictVerdict room_code_ages_replay(const OraclesGuestTransitionState *s, unsigned index, const uint8_t *table, uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES])
{
    const uint8_t f = s->load.room_flags;
    uint8_t other = 0;
    switch (index) {
    case 0x02:   /* tileReplacement_group2Map7e: the secret shop emptied */
        if ((s->load.bought_shop_items1 & 0x0fu) == 0x0fu) {
            fill(layout, 0x13, 3, 6, 0xa0);
            put(layout, 0x25, 0xf1); put(layout, 0x27, 0xf1); put(layout, 0x32, 0xa0);
        }
        return ORACLES_ROOM_PREDICTED;
    case 0x08:   /* tileReplacement_group0Map38 and group1Map38: the Maku Tree's barrier cleared */
    case 0x09:
        if (index == 0x08 ? global(s, AGES_FLAG_MAKU_TREE_SAVED) : (f & 0x80u) != 0u) fill(layout, 0x73, 1, 4, 0xf9);
        return ORACLES_ROOM_PREDICTED;
    case 0x14:   /* tileReplacement_group0Map5c */
        if (f & 0x80u) { put(layout, 0x34, 0x3a); fill(layout, 0x43, 1, 3, 0x3a); }
        return ORACLES_ROOM_PREDICTED;
    case 0x15:   /* tileReplacement_group2Mapf7: the chests opened (setTile), or the wall until the tree is harvested */
        if (f & 0x20u) { put(layout, 0x14, 0xf0); put(layout, 0x16, 0xf0); }
        else if ((f & 0xc0u) != 0xc0u && !((f & 0x40u) && (s->load.seed_tree_refilled & 0x01u))) {
            static const uint8_t wall[20] = { 0xb9, 0xa7, 0xa7, 0xa7, 0xb8, 0xb1, 0xa7, 0xa7, 0xa7, 0xb3, 0xb1, 0xa7, 0xa7, 0xa7, 0xb3, 0xb6, 0xb0, 0xb0, 0xb0, 0xb7 };
            draw(layout, 0x03, 4, 5, wall);
        }
        return ORACLES_ROOM_PREDICTED;
    case 0x16:   /* tileReplacement_group0Map73: the rubble cleared */
        if (f & 0x80u) { static const uint8_t row[5] = { 0x3a, 0x10, 0x11, 0x12, 0x3a }; draw(layout, 0x73, 1, 5, row); }
        return ORACLES_ROOM_PREDICTED;
    case 0x17:   /* tileReplacement_group0Map48 */
        if (global(s, AGES_FLAG_MAKU_TREE_SAVED)) fill(layout, 0x03, 1, 4, 0x3a);
        return ORACLES_ROOM_PREDICTED;
    case 0x18:   /* tileReplacement_group0Mapac: the scent tree removed until planted */
        if (!(f & 0x80u)) { fill(layout, 0x33, 1, 2, 0xaf); fill(layout, 0x43, 1, 2, 0xaf); }
        return ORACLES_ROOM_PREDICTED;
    case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f: case 0x20: {
        /* The vines' screens (getVinePosition): the vine, its sides at `pos`, where it is grown; withered where it
         * stands otherwise (the screens with the vine, not those above them). */
        static const struct { uint8_t vine, at, pos, rows, withers, sides[4]; } vines[8] = {
            { 0, 0x17, 0x06, 1, 1, { 0x5d, 0x60 } },              /* group0Map2c */
            { 0, 0x17, 0x66, 2, 0, { 0x5b, 0x45, 0x4d, 0x55 } },  /* group0Map1c */
            { 2, 0x18, 0x07, 1, 1, { 0x61, 0x60 } },              /* group0Mapba */
            { 2, 0x18, 0x77, 1, 0, { 0x46, 0x45 } },              /* group0Mapaa */
            { 3, 0x11, 0x00, 1, 1, { 0x61, 0x5d } },              /* group0Mapcc */
            { 3, 0x11, 0x70, 1, 0, { 0x46, 0x5c } },              /* group0Mapbc */
            { 4, 0x18, 0x07, 1, 1, { 0x61, 0x60 } },              /* group0Mapda */
            { 4, 0x18, 0x77, 1, 0, { 0x46, 0x45 } },              /* group0Mapca */
        };
        const unsigned v = index - 0x19u;
        if (!flags_byte(s, AGES_VINE_POSITIONS + vines[v].vine, &other)) return ORACLES_ROOM_PREDICT_ROOM_CODE;
        if (other != vines[v].at) {
            if (vines[v].withers) {
                if (!table) return ORACLES_ROOM_PREDICT_ROOM_CODE;
                withered_vine(table, other, layout);
            }
            return ORACLES_ROOM_PREDICTED;
        }
        vine_tiles(vines[v].pos, vines[v].sides, vines[v].rows, layout);
        if (index == 0x1b) put(layout, 0x18, 0x8b);
        return ORACLES_ROOM_PREDICTED;
    }
    case 0x21: case 0x22: {   /* tileReplacement_group0Map61 (Talus Peaks' two vines) and group0Map51 (the screen above) */
        static const uint8_t vine1[7] = { 0x56, 0xd5, 0x4d, 0x61, 0xd6, 0x5d, 0x8d }, vine2[7] = { 0x4d, 0xd5, 0x55, 0x5d, 0xd6, 0x60, 0x8d };
        static const uint8_t top1[3] = { 0x46, 0xd4, 0x5c }, top2[3] = { 0x5b, 0xd4, 0x45 };
        if (!flags_byte(s, AGES_VINE_POSITIONS + 1u, &other)) return ORACLES_ROOM_PREDICT_ROOM_CODE;
        if (other != 0x22u && other != 0x27u) {
            if (index == 0x22) return ORACLES_ROOM_PREDICTED;
            if (!table) return ORACLES_ROOM_PREDICT_ROOM_CODE;
            withered_vine(table, other, layout);
            return ORACLES_ROOM_PREDICTED;
        }
        const unsigned at = other == 0x22u ? 0x01u : 0x06u;
        if (index == 0x22) { draw(layout, 0x70u + at, 1, 3, other == 0x22u ? top1 : top2); return ORACLES_ROOM_PREDICTED; }
        const uint8_t *t = other == 0x22u ? vine1 : vine2;
        draw(layout, at, 2, 3, t);
        put(layout, 0x21u + at, t[6]);
        return ORACLES_ROOM_PREDICTED;
    }
    case 0x23:   /* tileReplacement_group0Map54: the bridge to Nuun Highlands */
        if (f & 0x40u) { fill(layout, 0x43, 1, 3, 0x1d); fill(layout, 0x53, 1, 3, 0x1e); put(layout, 0x68, 0x9e); }
        return ORACLES_ROOM_PREDICTED;
    case 0x24:   /* tileReplacement_group0Map25: the bridge to Symmetry City */
        if (global(s, AGES_FLAG_SYMMETRY_BRIDGE_BUILT)) { fill(layout, 0x50, 1, 3, 0x1d); fill(layout, 0x60, 1, 3, 0x1e); }
        return ORACLES_ROOM_PREDICTED;
    case 0x25:   /* tileReplacement_group0Map3a: Impa's door */
        if (global(s, AGES_FLAG_INTRO_DONE)) put(layout, 0x23, 0xee);
        return ORACLES_ROOM_PREDICTED;
    case 0x26:   /* tileReplacement_group0Map0b: the cave by dungeon 5, wPresentRoomFlags+$0a */
        if (!flags_byte(s, 0x0au, &other)) return ORACLES_ROOM_PREDICT_ROOM_CODE;
        if (other & 0x40u) put(layout, 0x43, 0xdd);
        return ORACLES_ROOM_PREDICTED;
    case 0x28: {   /* tileReplacement_group1Map27: the boulders, by four rooms' flags of the group */
        static const uint8_t rooms[4] = { 0x15, 0x17, 0x35, 0x37 }, clear[4] = { 0x33, 0x34, 0x35, 0x36 }, below[4] = { 0x43, 0x24, 0x45, 0x26 };
        if (!s->load.room_flags_page) return ORACLES_ROOM_PREDICT_ROOM_CODE;
        for (unsigned i = 0; i < 4u; i++)
            if (s->load.room_flags_page[rooms[i]] & 0x80u) { put(layout, clear[i], 0x3a); put(layout, below[i], 0x02); }
        return ORACLES_ROOM_PREDICTED;
    }
    case 0x2b:   /* tileReplacement_group2Map90 */
        if (f & 0x02u) {
            static const uint8_t rect[12] = { 0xdd, 0xde, 0xdf, 0xed, 0xee, 0xef, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe };
            draw(layout, 0x42, 2, 6, rect);
        }
        return ORACLES_ROOM_PREDICTED;
    case 0x2c:   /* tileReplacement_group1Map8c */
        if (f & 0x80u) {
            put(layout, 0x04, 0x30); put(layout, 0x05, 0x32); put(layout, 0x14, 0x3a); put(layout, 0x15, 0x3a);
            put(layout, 0x34, 0x02); put(layout, 0x35, 0x3a);
        }
        return ORACLES_ROOM_PREDICTED;
    case 0x2f:   /* tileReplacement_group2Map9e */
        if (f & 0x40u) { fill(layout, 0x13, 1, 3, 0x6d); fill(layout, 0x16, 1, 3, 0x6d); }
        return ORACLES_ROOM_PREDICTED;
    case 0x30:   /* tileReplacement_group0Mape0 to e2: the Maku Tree's top, a door by essence */
        if (s->load.essences_obtained & 0x10u) put(layout, 0x46, 0xdd);
        return ORACLES_ROOM_PREDICTED;
    case 0x31:
        if (s->load.essences_obtained & 0x01u) put(layout, 0x26, 0xdd);
        if (s->load.essences_obtained & 0x02u) put(layout, 0x53, 0xdd);
        return ORACLES_ROOM_PREDICTED;
    case 0x32:
        if (s->load.essences_obtained & 0x04u) put(layout, 0x54, 0xdd);
        return ORACLES_ROOM_PREDICTED;
    case 0x34:   /* tileReplacement_group1Map58: the advance shop, on a Game Boy Advance */
        if (s->load.gameboy_type & 0x80u) put(layout, 0x35, 0xde);
        return ORACLES_ROOM_PREDICTED;
    case 0x35:   /* tileReplacement_group0Map98: the dirt removed, unless Ricky waits for his gloves */
        if ((s->load.ricky_state & 0x20u) || !(s->load.ricky_state & 0x01u)
            || flag_set(s->load.obtained_treasures, s->load.obtained_treasures_size, AGES_TREASURE_RICKY_GLOVES)) put(layout, 0x24, 0x3a);
        return ORACLES_ROOM_PREDICTED;
    case 0x36: {   /* tileReplacement_group0Map76: the Black Tower's entrance at getBlackTowerProgress 1 */
        uint8_t far = 0, near = 0;
        if (!flags_byte(s, 0x90u, &far) || !flags_byte(s, 0xbau, &near)) return ORACLES_ROOM_PREDICT_ROOM_CODE;
        if (s->load.is_linked_game && !(far & 0x40u) && (near & 0x40u)) fill(layout, 0x54, 1, 2, 0xa7);
        return ORACLES_ROOM_PREDICTED;
    }
    case 0x37:   /* tileReplacement_group0Mapa5: the present library, wPastRoomFlags+$a5 */
        if (!flags_byte(s, ROOM_FLAGS_PAGE + 0xa5u, &other)) return ORACLES_ROOM_PREDICT_ROOM_CODE;
        if (other & 0x80u) { put(layout, 0x22, 0xee); put(layout, 0x23, 0xef); }
        return ORACLES_ROOM_PREDICTED;
    default:
        return ORACLES_ROOM_PREDICT_ROOM_CODE;
    }
}

static OraclesRoomPredictVerdict room_code_seasons_replay(const OraclesGuestTransitionState *s, unsigned index, uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES])
{
    const uint8_t f = s->load.room_flags;
    uint8_t other = 0;
    switch (index) {
    case 0x00:   /* tileReplacement_group0Mapc5: the GBA shop */
        if (s->load.gameboy_type & 0x80u) put(layout, 0x14, 0xea);
        return ORACLES_ROOM_PREDICTED;
    case 0x01:   /* tileReplacement_group0Mapd9: the Maku Tree's gates */
        if (f & 0x80u) { fill(layout, 0x14, 1, 2, 0xbf); put(layout, 0x24, 0xa9); put(layout, 0x25, 0xaa); }
        return ORACLES_ROOM_PREDICTED;
    case 0x03:   /* tileReplacement_group2_3Mapb0: the member's shop emptied */
        if ((s->load.bought_shop_items1 & 0x0fu) == 0x0fu) {
            fill(layout, 0x13, 3, 6, 0xa0);
            put(layout, 0x25, 0xf1); put(layout, 0x27, 0xf1); put(layout, 0x32, 0xa0);
        }
        return ORACLES_ROOM_PREDICTED;
    case 0x08:   /* tileReplacement_group0Mapf6: the sign outside the couple's house */
        if (global(s, SEASONS_FLAG_FINISHED_GAME)) put(layout, 0x33, 0xf2);
        return ORACLES_ROOM_PREDICTED;
    case 0x10:   /* tileReplacement_group0Map54: Ricky's sign */
        if (s->load.ricky_state & 0x40u) put(layout, 0x34, 0xf2);
        return ORACLES_ROOM_PREDICTED;
    case 0x11:   /* tileReplacement_group0Map7f: Holly's door in winter */
        if (s->load.room_state_modifier == SEASONS_WINTER) put(layout, 0x47, 0xea);
        return ORACLES_ROOM_PREDICTED;
    case 0x12:   /* tileReplacement_group0Map62: the water outside the floodgate keeper's house, wSubrosiaRoomFlags+$b5 */
        if (!flags_byte(s, ROOM_FLAGS_PAGE + 0xb5u, &other)) return ORACLES_ROOM_PREDICT_ROOM_CODE;
        if (!(other & 0x40u)) { fill(layout, 0x26, 2, 3, 0xfa); put(layout, 0x27, 0xfd); }
        return ORACLES_ROOM_PREDICTED;
    case 0x13: case 0x14: case 0x15: case 0x16:   /* group0Map60, 61, 70, 71: the swamp flooded until room $81's bit 7 */
        if (!s->load.room_flags_page) return ORACLES_ROOM_PREDICT_ROOM_CODE;
        if (!(s->load.room_flags_page[0x81] & 0x80u)) {
            if (index == 0x13) { fill(layout, 0x24, 6, 3, 0xfd); fill(layout, 0x47, 4, 3, 0xfd); }
            else if (index == 0x14) fill(layout, 0x40, 4, 7, 0xfd);
            else if (index == 0x15) fill(layout, 0x04, 4, 6, 0xfd);
            else { fill(layout, 0x00, 4, 7, 0xfd); fill(layout, 0x44, 4, 3, 0xfd); }
        }
        return ORACLES_ROOM_PREDICTED;
    case 0x17:   /* tileReplacement_group0Map81: the keylock's screen */
        if (!(f & 0x80u)) fill(layout, 0x04, 1, 3, 0xfd);
        else fill(layout, 0x14, 1, 3, s->load.room_state_modifier == SEASONS_WINTER ? 0xdc : 0xfa);
        return ORACLES_ROOM_PREDICTED;
    case 0x18:   /* tileReplacement_group0Map0d and 1d: the waterfall until bit 7 */
        if (!(f & 0x80u)) fill(layout, 0x62, 2, 3, 0xff);
        return ORACLES_ROOM_PREDICTED;
    case 0x19:
        if (!(f & 0x80u)) { fill(layout, 0x02, 3, 3, 0xff); put(layout, 0x13, 0xfe); fill(layout, 0x22, 1, 3, 0xfe); }
        return ORACLES_ROOM_PREDICTED;
    case 0x1c:   /* tileReplacement_group2_3Mapb5: the puddle inside the floodgate keeper's house */
        if (!(f & 0x40u)) put(layout, 0x37, 0xfa);
        return ORACLES_ROOM_PREDICTED;
    case 0x1d:   /* tileReplacement_group0Map4b: the keep's roof */
        if (global(s, SEASONS_FLAG_MOBLINS_KEEP_DESTROYED)) fill(layout, 0x73, 1, 3, 0x40);
        return ORACLES_ROOM_PREDICTED;
    case 0x1e:   /* tileReplacement_group0Map63: Tarm's gates */
        if (f & 0x80u) { fill(layout, 0x14, 1, 2, 0xad); fill(layout, 0x24, 1, 2, 0xad); }
        return ORACLES_ROOM_PREDICTED;
    case 0x1f:   /* tileReplacement_group0Mapf4 */
        if (f & 0x40u) {
            static const uint8_t rows[5] = { 0x0f, 0x11, 0x11, 0x0f, 0x11 };
            if (!(f & 0x20u)) put(layout, 0x45, 0xf1);
            draw(layout, 0x22, 1, 2, rows);
            draw(layout, 0x32, 1, 3, rows + 2);
        }
        return ORACLES_ROOM_PREDICTED;
    case 0x20:   /* tileReplacement_group0Map6f: the Moblins' house, ruined once repaired or the tree harvested */
        if (f & 0xc0u) {
            static const uint8_t destroyed[9] = { 0xfd, 0xfd, 0xfc, 0xfb, 0xfd, 0xfc, 0xfd, 0xfb, 0xfd };
            static const uint8_t ruined[9] = { 0xa8, 0xeb, 0xa5, 0xb3, 0xb4, 0xb5, 0xa3, 0xee, 0xa4 };
            draw(layout, 0x36, 3, 3, global(s, SEASONS_FLAG_MOBLIN_HOUSE_REPAIRED) || (s->load.seed_tree_refilled & 0x02u) ? ruined : destroyed);
        }
        return ORACLES_ROOM_PREDICTED;
    case 0x21:   /* tileReplacement_group0Map42: Tarm's statues */
        put(layout, 0x44, 0x9c);
        if (f & 0x80u) put(layout, 0x55, 0xbc); else put(layout, 0x54, 0xd6);
        if (f & 0x40u) put(layout, 0x65, 0xbc); else put(layout, 0x64, 0xd6);
        return ORACLES_ROOM_PREDICTED;
    case 0x22:   /* tileReplacement_group0Mapfc: the pirates' door */
        if (f & 0x80u) { fill(layout, 0x03, 1, 4, 0xaf); fill(layout, 0x13, 1, 4, 0xaf); }
        return ORACLES_ROOM_PREDICTED;
    case 0x23: case 0x24:   /* tileReplacement_group1Map64 and 74: the pirate ship gone once docked */
        if (global(s, SEASONS_FLAG_PIRATE_SHIP_DOCKED)) fill(layout, index == 0x23 ? 0x44 : 0x04, 4, 5, 0x0f);
        return ORACLES_ROOM_PREDICTED;
    case 0x25:   /* tileReplacement_group0Mapee: the ship's quicksand */
        if (global(s, SEASONS_FLAG_PIRATE_SHIP_DOCKED)) {
            static const uint8_t sand[25] = { 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xad, 0xad, 0xae, 0xae, 0xaf, 0xad, 0xad, 0xae,
                                              0xae, 0xaf, 0xbd, 0xbd, 0xbe, 0xbe, 0xaf, 0xbd, 0xbd, 0xbe, 0xbe, 0xaf };
            draw(layout, 0x23, 5, 5, sand);
        }
        return ORACLES_ROOM_PREDICTED;
    case 0x26:   /* tileReplacement_group0Mape4: the bridge to the moldorm */
        if (f & 0x40u) put(layout, 0x77, 0xa1);
        else { fill(layout, 0x65, 1, 2, 0xfd); fill(layout, 0x75, 1, 2, 0xfd); }
        return ORACLES_ROOM_PREDICTED;
    case 0x27:   /* tileReplacement_group1Map35: the linked doors, wGroup4RoomFlags+$f9 */
        if (!flags_byte(s, 2u * ROOM_FLAGS_PAGE + 0xf9u, &other)) return ORACLES_ROOM_PREDICT_ROOM_CODE;
        if (other & 0x04u) put(layout, 0x43, 0xe8);
        return ORACLES_ROOM_PREDICTED;
    case 0x28:   /* tileReplacement_group0Map56: the bridge into Natzu, but with Dimitri */
        if (s->load.animal_companion == SEASONS_DIMITRI) return ORACLES_ROOM_PREDICTED;
        if (f & 0x40u) put(layout, 0x66, 0xb0);
        else { fill(layout, 0x43, 1, 2, 0xfd); fill(layout, 0x53, 1, 2, 0xfd); }
        return ORACLES_ROOM_PREDICTED;
    case 0x29:   /* tileReplacement_group2_3Mapa4: Mr. Write's torch */
        if (f & 0x40u) put(layout, 0x36, 0x09);
        return ORACLES_ROOM_PREDICTED;
    case 0x2a:   /* tileReplacement_group2_3Mapab: the bridges */
        if (f & 0x40u) {
            fill(layout, 0x14, 1, 3, 0x6d);
            for (unsigned pos = 0x17; pos <= 0x47u; pos += 0x10u) put(layout, pos, 0x6a);
        }
        return ORACLES_ROOM_PREDICTED;
    default:
        return ORACLES_ROOM_PREDICT_ROOM_CODE;
    }
}

/* The room's own code, if it has any: ORACLES_ROOM_PREDICTED once replayed, or why it is not. */
static OraclesRoomPredictVerdict room_code(const OraclesRom *rom, const OraclesTables *data, const OraclesGuestTransitionState *s, const uint8_t *table,
                                           uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES])
{
    const int ages = s->game == ORACLES_GAME_AGES;
    const unsigned group = s->active_group & 7u;
    if (group >= SMALL_GROUPS || (!ages && group == 2u)) return ORACLES_ROOM_PREDICTED;
    const int code = room_value(rom, data->room_tile_changer_code_group_table, group, s->active_room);
    if (code == -2) return ORACLES_ROOM_PREDICT_UNREADABLE;
    if (code < 0) return ORACLES_ROOM_PREDICTED;
    if (!room_code_known(s, (unsigned)code, group, s->active_room)) return ORACLES_ROOM_PREDICT_ROOM_CODE;
    return ages ? room_code_ages_replay(s, (unsigned)code, table, layout) : room_code_seasons_replay(s, (unsigned)code, layout);
}

OraclesRoomPredictVerdict oracles_room_predict_substitute(const OraclesRom *rom, const OraclesTables *data, const OraclesGuestTransitionState *s,
                                                          const uint8_t *table, uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES])
{
    if (!rom || !data || !s || !layout) return ORACLES_ROOM_PREDICT_UNQUALIFIED;
    const int ages = s->game == ORACLES_GAME_AGES;
    /* replacePollutionWithWaterIfPollutionFixed (Ages): its list above or under water, once the flag is set, outdoors. */
    if (ages && flag_set(s->load.global_flags, s->load.global_flags_size, s->load.pollution_fixed_flag) && (s->tileset_flags & TILESETFLAG_OUTDOORS)) {
        const OraclesSym list = (s->tileset_flags & ORACLES_TILESETFLAG_UNDERWATER) ? data->pollution_below_water : data->pollution_above_water;
        if (!bound(list) || !replace_tiles(rom, list.bank, list.addr, layout)) return ORACLES_ROOM_PREDICT_UNREADABLE;
    }
    if (!single_tile_changes(rom, data, s, layout) || !standard_substitutions(rom, data, s, layout) || !opened_chest(rom, data, s, layout))
        return ORACLES_ROOM_PREDICT_UNREADABLE;
    const OraclesRoomPredictVerdict code = room_code(rom, data, s, table, layout);
    if (code != ORACLES_ROOM_PREDICTED) return code;
    /* Groups 4 and 5 run the shutters, switches, toggle blocks and Jabu-Jabu's water here: never predicted.  The
     * overworld of Ages then clears the tile under the portal, under a time warp's arrival and under Link sent back. */
    if (ages && s->active_group < 2u) {
        if (s->load.portal_group == s->active_group && s->load.portal_room == s->active_room
            && !replace_by_key(rom, data->timewarp_entry_tile_replacement_dict, s->load.portal_pos, layout)) return ORACLES_ROOM_PREDICT_UNREADABLE;
        if (s->load.timewarp_transition != 0xffu && (s->load.warp_transition & 0x0fu) == s->load.timewarp_transition
            && !replace_by_key(rom, data->timewarp_entry_tile_replacement_dict, s->load.warp_dest_pos, layout)) return ORACLES_ROOM_PREDICT_UNREADABLE;
        if (s->load.link_time_warp_tile && !replace_by_key(rom, data->timewarp_return_tile_replacement_dict, s->load.link_time_warp_tile - 1u, layout))
            return ORACLES_ROOM_PREDICT_UNREADABLE;
    }
    return ORACLES_ROOM_PREDICTED;
}

/* checkIsTempleRemains (Seasons' loadTilesetData.s): rooms $14 to $17 and $24 to $27 of group 0. */
static int temple_remains(unsigned group, unsigned room)
{
    return group == 0u && room >= 0x14u && room - 4u < 0x30u && ((room - 4u) & 0x0fu) < 4u;
}

OraclesRoomPredictVerdict oracles_room_predict_excluded(const OraclesRom *rom, const OraclesTables *data, const OraclesGuestTransitionState *s)
{
    if (!rom || !data || !s) return ORACLES_ROOM_PREDICT_UNQUALIFIED;
    const int ages = s->game == ORACLES_GAME_AGES;
    const unsigned group = s->active_group & 7u, room = s->active_room;
    if (group >= SMALL_GROUPS) return ORACLES_ROOM_PREDICT_LARGE;
    /* applyRoomSpecificTileChanges: every small group in Ages, groups 0, 1 and 3 in Seasons (tileSubstitutions.s). */
    if (ages || group != 2u) {
        const int code = room_value(rom, data->room_tile_changer_code_group_table, group, room);
        if (code == -2) return ORACLES_ROOM_PREDICT_UNREADABLE;
        if (code >= 0 && !room_code_known(s, (unsigned)code, group, room)) return ORACLES_ROOM_PREDICT_ROOM_CODE;
    }
    const int after = room_value(rom, data->tile_changes_after_gfx_group_table, group, room);
    if (after == -2) return ORACLES_ROOM_PREDICT_UNREADABLE;
    if (after >= 0) {
        const unsigned routines = ages ? AFTER_GFX_ROUTINES_AGES : AFTER_GFX_ROUTINES_SEASONS;
        const unsigned drawn = ages ? AFTER_GFX_DRAWN_AGES : AFTER_GFX_DRAWN_SEASONS;
        if ((unsigned)after >= routines) return ORACLES_ROOM_PREDICT_AFTER_GFX;
        if (after == AFTER_GFX_GASHA) {
            /* getIndexOfGashaSpotInRoom_body: the spot's index in gashaSpotRooms (low bytes), its bit planted or not. */
            const unsigned spots = s->load.gasha_spots_planted_size * 8u;
            unsigned index = 0;
            for (; index < spots; index++) {
                uint8_t at = 0;
                if (!read8(rom, data->gasha_spot_rooms.bank, data->gasha_spot_rooms.addr + index, &at)) return ORACLES_ROOM_PREDICT_UNREADABLE;
                if (at == room) break;
            }
            if (index == spots) return ORACLES_ROOM_PREDICT_UNREADABLE;
            if (flag_set(s->load.gasha_spots_planted, s->load.gasha_spots_planted_size, (uint8_t)index)) return ORACLES_ROOM_PREDICT_AFTER_GFX;
        } else if (ages && after == AFTER_GFX_WING_DUNGEON) {
            if (s->load.room_flags & 0x80u) return ORACLES_ROOM_PREDICT_AFTER_GFX;
        } else if (!ages && (after == AFTER_GFX_PIRATE_SHIP_BOW || after == AFTER_GFX_PIRATE_SHIP_MIDDLE)) {
            if (s->load.pirate_ship_flag == 0xffu || flag_set(s->load.global_flags, s->load.global_flags_size, s->load.pirate_ship_flag)) return ORACLES_ROOM_PREDICT_AFTER_GFX;
        } else if (!ages && after == AFTER_GFX_DINS_TROUPE) {
            if (!(s->load.room_flags & 0x40u)) return ORACLES_ROOM_PREDICT_AFTER_GFX;
        } else if (!ages && after == AFTER_GFX_WAGON) {
            if (s->load.intro_done_flag == 0xffu || !flag_set(s->load.global_flags, s->load.global_flags_size, s->load.intro_done_flag)) return ORACLES_ROOM_PREDICT_AFTER_GFX;
        } else if (!((drawn >> after) & 1u)) return ORACLES_ROOM_PREDICT_AFTER_GFX;
    }
    /* @adjustLoadingRoomForTempleRemains: the lava-filled layouts are other rooms' (Seasons). */
    if (!ages && temple_remains(group, room) && (s->load.temple_lava_flag == 0xffu || flag_set(s->load.global_flags, s->load.global_flags_size, s->load.temple_lava_flag)))
        return ORACLES_ROOM_PREDICT_OTHER_LAYOUT;
    return ORACLES_ROOM_PREDICTED;
}

void oracles_room_predict_collisions(const uint8_t table[ORACLES_TILESET_COLLISIONS_BYTES], const uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES],
                                     int small, uint8_t collisions[ORACLES_ROOM_LAYOUT_BYTES])
{
    for (unsigned i = 0; i < ORACLES_ROOM_LAYOUT_BYTES; i++) collisions[i] = table[layout[i]];
    /* @blankDataAroundCollisions: the sixteenth column; in a small room the ninth row and the eleventh column. */
    for (unsigned row = 0; row < ORACLES_LARGE_ROOM_HEIGHT; row++) {
        collisions[row * 16u + 15u] = 0xffu;
        if (small) collisions[row * 16u + ORACLES_SMALL_ROOM_WIDTH] = 0xffu;
    }
    if (small)
        for (unsigned col = 0; col < 16u; col++) collisions[ORACLES_SMALL_ROOM_HEIGHT * 16u + col] = 0xffu;
}

#define ROOMFLAG_LAYOUTSWAP 0x01u        /* getAdjustedRoomGroup (Ages): the room's layout and tileset of group 2 or 3 */

/* The layout group loadRoomLayout decodes the room in: wTilesetLayoutGroup; where the image names it by group (its
 * expanded tilesets, @getLayoutGroup), wLayoutGroupOverride unless $ff, else in Seasons' group 0 wRoomStateModifier
 * (the season), else the table's byte for the group, in Ages the group getAdjustedRoomGroup gives (groups 0 and 1 as
 * 2 and 3 under bit 0 of the room's flags).  -1 when it cannot be read or names no layout group. */
static int layout_group(const OraclesRom *rom, const OraclesTables *data, const OraclesGuestTransitionState *s)
{
    if (!bound(data->layout_group_table)) return s->load.tileset_layout_group & 7;
    unsigned group = s->active_group, value = s->load.tileset_layout_group;
    if (value == 0xffu && s->game == ORACLES_GAME_SEASONS && group == 0u) value = s->load.room_state_modifier;
    else if (value == 0xffu) {
        if (s->game == ORACLES_GAME_AGES && group < 2u && (s->load.room_flags & ROOMFLAG_LAYOUTSWAP)) group |= 2u;
        uint8_t byte = 0;
        if (group > 7u || !read8_any(rom, data->layout_group_table, group, &byte)) return -1;
        value = byte;
    }
    return value > 7u ? -1 : (int)value;
}

OraclesRoomPredictVerdict oracles_room_predict(const OraclesRom *rom, const OraclesTables *data, const OraclesGuestTransitionState *s,
                                               uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES], uint8_t collisions[ORACLES_ROOM_LAYOUT_BYTES])
{
    if (!rom || !data || !s || !layout || !collisions || (s->game != ORACLES_GAME_AGES && s->game != ORACLES_GAME_SEASONS)) return ORACLES_ROOM_PREDICT_UNQUALIFIED;
    if (!bound(data->room_layout_group_table) || (!bound(data->tileset_layout_table) && !bound(data->expanded_tileset_mappings_table)) || !bound(data->single_tile_change_group_table)
        || !bound(data->standard_tile_substitutions) || !bound(data->chest_data_group_table) || !bound(data->room_tile_changer_code_group_table)
        || !bound(data->tile_changes_after_gfx_group_table) || !bound(data->gasha_spot_rooms)) return ORACLES_ROOM_PREDICT_UNQUALIFIED;
    if (s->scroll_mode != SCROLL_MODE_LOADED) return ORACLES_ROOM_PREDICT_EARLY;
    const OraclesRoomPredictVerdict excluded = oracles_room_predict_excluded(rom, data, s);
    if (excluded != ORACLES_ROOM_PREDICTED) return excluded;
    /* loadRoomLayout: the 192 bytes cleared, the room of wLoadingRoom (wActiveRoom) decoded in its layout group's table. */
    const int group = layout_group(rom, data, s);
    if (group < 0) return ORACLES_ROOM_PREDICT_UNREADABLE;
    int large = 0;
    memset(layout, 0, ORACLES_ROOM_LAYOUT_BYTES);
    if (oracles_decode_room_layout(rom, data, (unsigned)group, s->active_room, layout, &large) != ORACLES_OK) return ORACLES_ROOM_PREDICT_UNREADABLE;
    if (large) return ORACLES_ROOM_PREDICT_LARGE;
    /* loadTilesetLayout, before the layout: w3TileCollisions, the tileset layout's own (the one loaded already when it
     * is the same), which a room's own code may read; with the expanded tilesets, the tileset's, by wTilesetIndex: a
     * seasonal one's of the season only where they are loaded again (wLoadedTilesetLayout another), those kept being
     * another season's maybe. */
    uint8_t table[ORACLES_TILESET_COLLISIONS_BYTES];
    if (bound(data->expanded_tileset_mappings_table)) {
        int seasonal = 0;
        if (oracles_expanded_tileset_collisions(rom, data, s->load.tileset_layout, s->load.room_state_modifier, table, &seasonal) != ORACLES_OK
            || (seasonal && s->load.loaded_tileset_layout == s->load.tileset_layout)) return ORACLES_ROOM_PREDICT_UNREADABLE;
    } else {
        int have = 0;
        if (data->num_tileset_layouts && s->load.tileset_layout >= data->num_tileset_layouts) return ORACLES_ROOM_PREDICT_UNREADABLE;
        if (oracles_decode_tileset_layout(rom, data, s->load.tileset_layout, NULL, table, NULL, &have) != ORACLES_OK || !have) return ORACLES_ROOM_PREDICT_UNREADABLE;
    }
    const OraclesRoomPredictVerdict substituted = oracles_room_predict_substitute(rom, data, s, table, layout);
    if (substituted != ORACLES_ROOM_PREDICTED) return substituted;
    oracles_room_predict_collisions(table, layout, 1, collisions);
    return ORACLES_ROOM_PREDICTED;
}

#define TILESETFLAG_DUNGEON 0x08u
#define ROOM_AGES_MAKU_TREE 0x38u        /* checkTilesetOverride@checkMakuTreeSaved: group 0, its tileset set by a flag of the past */
#define PACK_AGES_NUUN 0x7fu             /* checkTilesetOverride: Nuun Highlands, the tileset of the animal companion */

static int marked(const OraclesRoomScenes *scenes, unsigned kind, unsigned id, unsigned subid)
{
    if (kind >= 3u) return 0;
    if ((scenes->ids[kind][(id >> 3) & 31u] >> (id & 7u)) & 1u) return 1;
    if (kind != 0u || !scenes->subids) return 0;
    for (const OraclesRoomSceneSubids *p = scenes->subids; p->id; p++)
        if (p->id == id) return subid >= 64u || ((p->subids >> subid) & 1u);
    return 0;
}

int oracles_room_predict_scene(const OraclesRoomScenes *scenes, const uint8_t *objects)
{
    if (!scenes || !scenes->ids) return 1;
    if (!objects) return -1;
    for (unsigned slot = 0; slot < 16u; slot++)
        for (unsigned kind = 0; kind < 3u; kind++) {
            /* Interactions at $dx40, enemies at $dx80, parts at $dxc0; the room left's, enabled 2 since the load
             * (setObjectsEnabledTo2), go at the scroll's end. */
            const uint8_t *o = objects + slot * 0x100u + (kind + 1u) * 0x40u;
            const unsigned enabled = o[0] & 3u;
            if (enabled != 1u && enabled != 3u) continue;
            if (marked(scenes, kind, o[1], o[2])) return 1;
        }
    return 0;
}

/* getMoblinKeepScreenIndex (Seasons' engine): one of the five rooms of the image's moblinKeepRooms, which take a tileset
 * of their own in group 0 once the keep is destroyed (getMoblinKeepSeasonsTilesetData); -1 when the list is unreadable. */
static int moblin_keep(const OraclesRom *rom, const OraclesGuestTransitionState *s, unsigned room)
{
    for (unsigned i = 0; i < 5u; i++) {
        uint8_t keep = 0;
        if (!read8(rom, s->moblin_keep_bank, s->moblin_keep_table + i, &keep)) return -1;
        if (keep == room) return 1;
    }
    return 0;
}

/* loadScreenMusic: the room's pack (roomPackData), Ages' groups 0 and 1, Seasons' group 0. */
static int room_pack(const OraclesRom *rom, const OraclesGuestTransitionState *s, unsigned group, unsigned room, uint8_t *pack)
{
    return read8(rom, s->room_pack_bank, s->room_pack_table + group * 0x100u + room, pack);
}

OraclesRoomPredictVerdict oracles_room_predict_beside(const OraclesRom *rom, const OraclesTables *data, const OraclesGuestTransitionState *s,
                                                      unsigned across, int (*special)(const OraclesGuestTransitionState *state, uint8_t room, void *opaque),
                                                      void *opaque, uint8_t *room, uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES],
                                                      uint8_t collisions[ORACLES_ROOM_LAYOUT_BYTES], uint8_t *active_collisions)
{
    if (!rom || !data || !s || !room || !layout || !collisions || (s->game != ORACLES_GAME_AGES && s->game != ORACLES_GAME_SEASONS) || across > 3u)
        return ORACLES_ROOM_PREDICT_UNQUALIFIED;
    const int expanded = bound(data->expanded_tileset_mappings_table);
    if (s->room_pack_bank == 0xffu || (s->game == ORACLES_GAME_SEASONS && s->moblin_keep_bank == 0xffu) || !bound(data->room_tilesets_group_table)
        || !bound(data->tileset_data)) return ORACLES_ROOM_PREDICT_UNQUALIFIED;
    /* The expanded tilesets on Ages' engine: the checkTilesetOverride followed below must be the image's (its profile). */
    if (expanded && s->game == ORACLES_GAME_AGES && !s->load.tileset_override_rules) return ORACLES_ROOM_PREDICT_UNQUALIFIED;
    if (s->scroll_mode != SCROLL_MODE_LOADED) return ORACLES_ROOM_PREDICT_EARLY;
    const int ages = s->game == ORACLES_GAME_AGES;
    const unsigned group = s->active_group, from = s->active_room, col = from & 0x0fu, row = from >> 4;
    /* Ages' sea floor (groups 2 and 3, an underwater tileset): no map, no pack (checkRoomPack only acts in groups 0 and
     * 1), no edge refused (@transition refuses the map's edges outdoors only); updateActiveRoom's room a row or a
     * column away, a byte's sum (@smallMapDirectionTable). */
    const int sea_floor = ages && (group == 2u || group == 3u) && (s->tileset_flags & ORACLES_TILESETFLAG_UNDERWATER) && !(s->tileset_flags & TILESETFLAG_DUNGEON);
    /* updateActiveRoom: a room a row (16) or a column (1) away, within the rows and columns of the group's map (Seasons'
     * Subrosia, group 1, its own; a map walled inside the grid, from its first column and row). */
    if (!s->load.room_flags_page || (!sea_floor && (group > 1u || !s->map_width || !s->map_height))) return ORACLES_ROOM_PREDICT_ROUTED;
    if (!sea_floor && ((across == 0u && row <= s->map_top) || (across == 2u && row + 1u >= s->map_height) || (across == 3u && col <= s->map_left)
                       || (across == 1u && col + 1u >= s->map_width)))
        return ORACLES_ROOM_PREDICT_ROUTED;
    const unsigned beside = (across == 0u ? from - 16u : across == 2u ? from + 16u : across == 3u ? from - 1u : from + 1u) & 0xffu;
    *room = (uint8_t)beside;
    if ((!sea_floor && !(s->tileset_flags & TILESETFLAG_OUTDOORS)) || (s->tileset_flags & TILESETFLAG_DUNGEON) || !special || special(s, (uint8_t)from, opaque)
        || special(s, (uint8_t)beside, opaque))
        return ORACLES_ROOM_PREDICT_ROUTED;
    if (!sea_floor && (ages || group == 0u)) {
        /* checkRoomPack: a scroll into another pack fades (Ages, groups 0 and 1) or sets the season (Seasons, group 0
         * only: Subrosia's scrolls compare no pack). */
        uint8_t pack_from = 0, pack = 0;
        if (!room_pack(rom, s, group, from, &pack_from) || !room_pack(rom, s, group, beside, &pack)) return ORACLES_ROOM_PREDICT_UNREADABLE;
        if (ages ? ((pack ^ pack_from) & 0x7fu) != 0u || pack == PACK_AGES_NUUN : pack != pack_from) return ORACLES_ROOM_PREDICT_ROUTED;
        /* An image's checkTilesetOverride that gives every pack from $7f a tileset of the season (Ages' engine). */
        if (expanded && ages && s->load.tileset_override_rules == ORACLES_TILESET_OVERRIDE_SEASONS && pack >= PACK_AGES_NUUN) return ORACLES_ROOM_PREDICT_ROUTED;
        if (ages && group == 0u && beside == ROOM_AGES_MAKU_TREE) return ORACLES_ROOM_PREDICT_ROUTED;
        if (!ages) {
            const int keep = moblin_keep(rom, s, beside);
            if (keep < 0) return ORACLES_ROOM_PREDICT_UNREADABLE;
            if (keep) return ORACLES_ROOM_PREDICT_ROUTED;
        }
    }
    /* The room entered's objects, created by its load (its list and initializeRoom), run their first frame once the
     * scroll beside is decided. */
    const OraclesRoomScenes scenes = ages ? (OraclesRoomScenes){ oracles_room_scenes_ages, oracles_room_scene_subids_ages }
                                          : (OraclesRoomScenes){ oracles_room_scenes_seasons, oracles_room_scene_subids_seasons };
    const int scene = oracles_room_predict_scene(&scenes, s->load.objects);
    if (scene < 0) return ORACLES_ROOM_PREDICT_EARLY;
    if (scene) return ORACLES_ROOM_PREDICT_SCENE;
    const uint8_t flags = s->load.room_flags_page[beside];
    /* loadTilesetData: the room's entry in roomTilesetsGroupTable, Ages' group swapped by the room's flags. */
    uint8_t index = 0;
    OraclesTilesetEntry entry;
    const unsigned tileset_group = ages && (flags & ROOMFLAG_LAYOUTSWAP) ? group | 2u : group;
    if (oracles_room_tileset(rom, data, tileset_group, beside, &index) != ORACLES_OK
        || oracles_read_tileset(rom, data, index & 0x7fu, s->load.room_state_modifier, &entry) != ORACLES_OK) return ORACLES_ROOM_PREDICT_UNREADABLE;
    /* Ages' engine sets wRoomStateModifier for the room entered after its load (initializeRoom): the season the room
     * beside will be loaded in is not the one read here, a seasonal expanded tileset left to its load. */
    uint8_t first = 0;
    if (expanded && ages && (!read8_any(rom, data->expanded_tileset_mappings_table, (index & 0x7fu) * 3u, &first) || first == 0xffu))
        return ORACLES_ROOM_PREDICT_ROUTED;
    if (sea_floor ? !(entry.flags & ORACLES_TILESETFLAG_UNDERWATER) || (entry.flags & (TILESETFLAG_DUNGEON | ORACLES_TILESETFLAG_SIDESCROLL))
                  : !(entry.flags & TILESETFLAG_OUTDOORS) || (entry.flags & (TILESETFLAG_DUNGEON | ORACLES_TILESETFLAG_SIDESCROLL | ORACLES_TILESETFLAG_UNDERWATER))
                        || (!ages && entry.dungeon_index != 0x0fu)) return ORACLES_ROOM_PREDICT_ROUTED;
    OraclesGuestTransitionState at = *s;
    at.active_room = (uint8_t)beside;
    at.tileset_flags = entry.flags;
    at.active_collisions = (uint8_t)(ages ? entry.collisions_mode & 7u : entry.collisions_mode);
    /* With the expanded tilesets, the tileset's own byte (wTilesetIndex), the layout group by the group (no override:
     * the Maku Tree's screen is left above), and the collisions loaded those of the room entered. */
    at.load.tileset_layout = expanded ? index : entry.layout;
    at.load.tileset_layout_group = expanded ? 0xffu : entry.layout_group;
    if (expanded) at.load.loaded_tileset_layout = s->load.tileset_layout;
    at.load.room_flags = flags;
    if (active_collisions) *active_collisions = at.active_collisions;
    /* The scroll into it runs updateSeedTreeRefillData before its load, which may set a bit of wSeedTreeRefilledBitset
     * (Seasons' Moblins' house, 0:6f, among the seed trees' rooms): a routine that reads it is left to that load. */
    const int code = ages || at.active_group != 2u ? room_value(rom, data->room_tile_changer_code_group_table, at.active_group & 7u, at.active_room) : -1;
    if (code >= 0) {
        const RoomCode *known = room_code_known(&at, (unsigned)code, at.active_group & 7u, at.active_room);
        if (known && (known->reads & READS_TREE)) return ORACLES_ROOM_PREDICT_ROOM_CODE;
    }
    return oracles_room_predict(rom, data, &at, layout, collisions);
}

const char *oracles_room_predict_verdict_name(OraclesRoomPredictVerdict verdict)
{
    static const char *const names[ORACLES_ROOM_PREDICT_VERDICTS] = { "predicted", "image not qualified", "the room not named yet", "a large room or a dungeon's group",
                                                                      "the room's own code", "its own code after the graphics", "another room's layout", "a table unreadable",
                                                                      "another way out of the room", "objects that may take the controls" };
    return (unsigned)verdict < ORACLES_ROOM_PREDICT_VERDICTS ? names[verdict] : "?";
}
