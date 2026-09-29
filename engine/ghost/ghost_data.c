/* The tables the ghost reads from the user's ROM once, at its creation: the
 * areas of Seasons whose season never changes, and the rooms of the open sea
 * of Ages. */
#include "ghost_internal.h"

/* Seasons: the room packs whose season never changes.  The rod of seasons
 * works only on a stump (rodOfSeasons.s: TILETYPE_STUMP, tile $20 of the
 * overworld), so an area without one in any of its rooms keeps the season
 * roomPackSeasonTable gives it, and packs $f0 and up hold no season at all
 * (determineCompanionRegionSeason: Natzu by the animal companion).  Horon
 * village (pack 0) draws its season at random and is never fixed.  Read from
 * the user's ROM: roomPackData and the rooms' layouts. */
void oracles_ghost_find_fixed_seasons(OraclesGhost *g, const uint8_t *rom, size_t rom_size)
{
    for (unsigned p = 0xf0u; p < 256u; p++) g->fixed_season[p] = 1;
    const OraclesCompatProfile *profile = oracles_guest_profile(g->guest);
    const OraclesGuestTables *t = oracles_guest_tables(g->guest);
    const OraclesTables *data = oracles_compat_data_tables(profile);
    if (!oracles_compat_seasons_rules(profile)
        || !data || t->room_pack_data.bank == ORACLES_GUEST_ABSENT) return;
    const OraclesRom r = { rom, rom_size };
    const size_t packs = oracles_rom_offset(t->room_pack_data.bank, t->room_pack_data.addr);
    uint8_t has_room[256] = { 0 }, has_stump[256] = { 0 };
    for (unsigned room = 0; room < 256u; room++) {
        uint8_t pack, layout[ORACLES_ROOM_LAYOUT_BYTES];
        int large = 0;
        if (oracles_rom_read8(&r, packs + room, &pack) != ORACLES_OK) return;
        g->room_pack_of[room] = pack;
        if (oracles_decode_room_layout(&r, data, 0, room, layout, &large) != ORACLES_OK || large) return;
        has_room[pack] = 1;
        for (unsigned row = 0; row < ORACLES_SMALL_ROOM_HEIGHT; row++)
            for (unsigned col = 0; col < ORACLES_SMALL_ROOM_WIDTH; col++)
                if (layout[row * 16u + col] == 0x20u) has_stump[pack] = 1;
    }
    for (unsigned p = 1; p < 0xf0u; p++) g->fixed_season[p] = has_room[p] && !has_stump[p];
}

/* Ages: the rooms of the open sea, by their tileset's flags: under water
 * (TILESETFLAG_UNDERWATER) and outdoors, neither indoors nor a large indoor
 * room, which is what separates the sea from the houses under it (the
 * mermaid's, the sunken rooms: flags $44) and from Jabu-Jabu's halls ($d1).
 * The sea makes a map of rooms side by side inside its group, whose extent
 * the view needs: a room of the sea has its neighbours beside it, one at the
 * sea's edge has none; a house under water stands alone like any house. */
void oracles_ghost_find_open_water_rooms(OraclesGhost *g, const uint8_t *rom, size_t rom_size)
{
    const OraclesCompatProfile *profile = oracles_guest_profile(g->guest);
    const OraclesTables *data = oracles_compat_data_tables(profile);
    if (!oracles_compat_open_water_rules(profile) || !data) return;
    const OraclesRom r = { rom, rom_size };
    for (unsigned group = 0; group < 8u; group++)
        for (unsigned room = 0; room < 256u; room++) {
            uint8_t index = 0;
            OraclesTilesetEntry entry;
            if (oracles_room_tileset(&r, data, group, room, &index) != ORACLES_OK) continue;   /* an unreadable entry is no sea */
            if (oracles_read_tileset(&r, data, index & 0x7fu, 0, &entry) != ORACLES_OK) continue;
            g->open_water_room[group][room] = (entry.flags & 0x55u) == 0x41u;   /* under water, outdoors, not indoors nor large indoors */
        }
}

int oracles_ghost_room_open_water(const OraclesGhost *g, uint8_t group, uint8_t room) { return g->open_water_room[group & 7u][room] != 0; }

/* The rooms of the overworld groups on their map, by their tileset's flags
 * and their place: the game decides outdoors by TILESETFLAG_OUTDOORS (and, in
 * Ages' engine, blocks the scrolls at the map's edges, bank1.s), where the
 * engine's grid took every room of groups 0 and 1 for the map.  Ages' groups
 * are outdoors across its 14 x 14 (its Maku tree screen has its own rule) and
 * keep the grid; Moonrise files houses, a dungeon and outer areas under
 * them, and Subrosia's rooms past its 11 x 8 are none of Subrosia's map.  A
 * seasonal tileset is read in its first season: Holodrum's are outdoors in
 * all four. */
void oracles_ghost_find_map_rooms(OraclesGhost *g, const uint8_t *rom, size_t rom_size)
{
    const OraclesCompatProfile *profile = oracles_guest_profile(g->guest);
    const OraclesTables *data = oracles_compat_data_tables(profile);
    for (unsigned group = 0; group < 2u; group++)
        for (unsigned room = 0; room < 256u; room++) g->map_room[group][room] = 1;
    if (!oracles_compat_tileset_map_rules(profile)) return;
    const OraclesRom r = { rom, rom_size };
    for (unsigned group = 0; group < 2u; group++) {
        const unsigned width = oracles_compat_group_map_width(profile, group), height = oracles_compat_group_map_height(profile, group);
        const unsigned left = oracles_compat_group_map_left(profile, group), top = oracles_compat_group_map_top(profile, group);
        for (unsigned room = 0; room < 256u; room++) {
            uint8_t index = 0;
            OraclesTilesetEntry entry;
            g->map_room[group][room] = 0;
            if ((room & 0x0fu) < left || (room & 0x0fu) >= width || (room >> 4u) < top || (room >> 4u) >= height) continue;
            if (!data || oracles_room_tileset(&r, data, group, room, &index) != ORACLES_OK) continue;   /* unreadable: no room of the map */
            if (oracles_read_tileset(&r, data, index & 0x7fu, 0, &entry) != ORACLES_OK) continue;
            g->map_room[group][room] = (entry.flags & 0x01u) != 0;   /* TILESETFLAG_OUTDOORS */
        }
    }
}

int oracles_ghost_room_on_map(const OraclesGhost *g, uint8_t group, uint8_t room)
{
    return (group & 7u) < 2u && g->map_room[group & 7u][room] != 0;
}

int oracles_ghost_open_water_extent(const OraclesGhost *g, uint8_t group, uint8_t extent[4])
{
    unsigned left = 16u, top = 16u, right = 0, bottom = 0;
    for (unsigned room = 0; room < 256u; room++) {
        if (!g->open_water_room[group & 7u][room]) continue;
        const unsigned col = room & 0x0fu, row = room >> 4u;
        if (col < left) left = col;
        if (row < top) top = row;
        if (col + 1u > right) right = col + 1u;
        if (row + 1u > bottom) bottom = row + 1u;
    }
    if (right == 0) return 0;
    extent[0] = (uint8_t)left; extent[1] = (uint8_t)top; extent[2] = (uint8_t)(right - left); extent[3] = (uint8_t)(bottom - top);
    return 1;
}

int oracles_ghost_area_holds_season(const OraclesGhost *g, uint8_t room_pack) { return !g->fixed_season[room_pack]; }

/* The rooms whose transitions the game decides itself (getNextActiveRoom,
 * docs/GAME_HOOKS.md, section 4.3): before its standard transition the game looks the
 * active room up in mapTransitionGroupTable, and the room it then loads
 * depends on live state — in Seasons the Lost Woods sends Link back to its
 * entrance until he strings the right directions together
 * (wLostWoodsTransitionCounter1 and 2), in Ages the forest scrambler reads a
 * table of its own.  Such a room's neighbours are not the ones the grid
 * gives, and each direction has to be asked of the game itself.  The table
 * is eight pointers, one a group, each to pairs of room and case ended by a
 * null room; read from the user's ROM at the ghost's creation, as the sea
 * is.  The case says which routine routes the room: the ghost runs it, so
 * the host never has to know it. */
void oracles_ghost_find_self_routed_rooms(OraclesGhost *g, const uint8_t *rom, size_t rom_size)
{
    const OraclesGuestTables *t = oracles_guest_tables(g->guest);
    if (t->map_transition_group_table.bank == ORACLES_GUEST_ABSENT) return;
    const OraclesRom r = { rom, rom_size };
    const unsigned bank = t->map_transition_group_table.bank;
    const size_t table = oracles_rom_offset(bank, t->map_transition_group_table.addr);
    for (unsigned group = 0; group < 8u; group++) {
        unsigned addr;
        if (oracles_rom_read16le(&r, table + group * 2u, &addr) != ORACLES_OK) return;
        size_t at = oracles_rom_offset(bank, addr);
        for (unsigned n = 0; n < 256u; n++) {
            uint8_t room, routine;
            if (oracles_rom_read8(&r, at, &room) != ORACLES_OK || room == 0 || oracles_rom_read8(&r, at + 1u, &routine) != ORACLES_OK) break;
            g->self_routed_room[group][room] = (uint8_t)(routine + 1u);   /* 0: not routed */
            at += 2u;
        }
    }
}

int oracles_ghost_room_self_routed(const OraclesGhost *g, uint8_t group, uint8_t room) { return g->self_routed_room[group & 7u][room] != 0; }
int oracles_ghost_room_routine(const OraclesGhost *g, uint8_t group, uint8_t room) { return (int)g->self_routed_room[group & 7u][room] - 1; }
