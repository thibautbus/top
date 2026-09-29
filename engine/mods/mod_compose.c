/* The houses of the mods, composed into the image before the core exists:
 * the game then loads them as its own rooms.
 *
 * A house is a facade of three by three metatiles stamped on an overworld
 * room, whose door warps into an interior of the same index in the interior
 * group of its era, where a keeper stands behind a counter.  The houses of
 * every mod loaded are composed together, the mods in the order of their
 * names; the composition writes data only, never code:
 *   - the image grows to 2 MiB; the small layout groups of the rooms move
 *     into the extension (table and streams copied, their root repointed), and
 *     the new layouts, stored raw, follow the copied streams;
 *   - each interior's music and object list (the keeper, in the tail of the
 *     objects' bank) are written at its index;
 *   - each house takes a destination in each group (one no source sends to,
 *     or a new one: the table grows), the interior group's source list gains
 *     the exit and the overworld room's door joins its sources; the warp
 *     data of bank 04 is then laid out again (mod_warps.c), and the game's
 *     lookups of every room checked: each leads where it led, but the doors
 *     and the exits of the houses.
 * One house a room: the interior takes the room's index, whose room flags are
 * the room's own (the interior group's flags are the overworld's), so a
 * second house there would have no index that marks no other room visited.
 * Every place is checked in the user's image before it is written: a room of
 * two houses, an interior index that is a destination, has sources, objects
 * or a tileset of its own, or warp lists that do not fit, is refused with the
 * mod and the house.  Formats: ROM_DATA_FORMATS, sections 2.5 (layouts), 5.1
 * (objects) and 6 (warps). */
#include "mod_internal.h"

#include "mod_warps.h"
#include "oracles_rom.h"

#include <stdlib.h>
#include <string.h>

#define BANK 0x4000u
#define IMAGE_SIZE (2u * 1024u * 1024u)
#define SMALL_LAYOUT 80u
#define READ_AHEAD 176u
#define MAX_LAYOUT_GROUPS 16u

/* A game's house: a facade of three by three metatiles and the ground in front of its door, an interior of ten by
 * eight (walls, floor, a counter across the middle, the door at the bottom), its tileset and group, the interior whose
 * music it plays, the exit's flags (the bottom edge's quadrants, as the game's own houses have them), where its
 * index comes from, and the keeper behind the counter (an object stream: $f2 interaction id subid y x, $ff end).
 * The houses are the mods' own compositions; their metatiles are chosen from the tileset by their index. */
typedef struct house_model {
    uint8_t facade[9], path;
    uint8_t facade_tileset;                        /* metatile indices are the tileset's: the facade stands in its rooms only */
    uint8_t interior[SMALL_LAYOUT];
    uint8_t interior_tileset, interior_group, music_like, leave;
    unsigned first_free;                           /* 0: the interior takes its room's index; else the first index tried */
    uint8_t keeper[6];
} house_model;

/* Ages: a facade of tileset $00's house block (its roof over a wall, a door and a wall) and an interior of the wall,
 * floor, door and counter metatiles of tileset $28, with the music of one of that tileset's interiors; the interior
 * group 2 shares the overworld's room flags, so the interior takes its room's index.  The keeper: INTERAC_MALE_VILLAGER subid $03, a villager of the
 * present whatever the game's progress. */
static const house_model ages_house = {
    { 0x86, 0x87, 0x88, 0x96, 0x97, 0x98, 0xf6, 0xde, 0xf6 }, 0x0c, 0x00,
    {
        0xb8, 0x80, 0x5a, 0x6a, 0x5a, 0x5a, 0x6a, 0x5a, 0x80, 0xb9,
        0xb3, 0x81, 0x6c, 0x6b, 0x6c, 0x6c, 0x6b, 0x6c, 0x81, 0xb1,
        0xb3, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xb1,
        0xb3, 0xa1, 0xa1, 0x8e, 0x8d, 0x8d, 0x8f, 0xa1, 0xa1, 0xb1,
        0xb3, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xb1,
        0xb3, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xb1,
        0xb3, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xb1,
        0xba, 0xb2, 0xb2, 0xb2, 0xe0, 0xe1, 0xb2, 0xb2, 0xb2, 0xbb,
    },
    0x28, 2, 0xee, 0x0c, 0, { 0xf2, 0x3a, 0x03, 0x28, 0x50, 0xff },
};

/* Seasons: a facade of tileset $00's house block (the same metatiles in the four seasons' layouts; tileset $01's
 * would draw water in a room of $00) and an interior of the wall, back wall, floor, door and counter metatiles of
 * the houses' tileset $2c.  Groups 1 to 3 share one
 * table of tilesets and one page of room flags: Subrosia is $00-$7f, the interiors $80-$bf, and $c0-$ff is unused
 * (tileset $61), where the interior takes the first free index.  The exit is the bottom edge's $04, as the game's
 * houses have it; the music, that of one of those houses.  The keeper: INTERAC_MR_WRITE, his look alone (his script runs when Link talks
 * to him, which the keeper's trigger holds from the game). */
static const house_model seasons_house = {
    { 0xa3, 0xa4, 0xa5, 0xb3, 0xb4, 0xb5, 0xf6, 0xea, 0xf6 }, 0x2b, 0x00,
    {
        0xb8, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0xb9,
        0xb3, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0xb1,
        0xb3, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xb1,
        0xb3, 0xa0, 0xa0, 0x8e, 0x8d, 0x8d, 0x8f, 0xa0, 0xa0, 0xb1,
        0xb3, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xb1,
        0xb3, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xb1,
        0xb3, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xb1,
        0xba, 0xb2, 0xb2, 0xb2, 0xe0, 0xe1, 0xb2, 0xb2, 0xb2, 0xbb,
    },
    0x2c, 3, 0x83, 0x04, 0xc0, { 0xf2, 0x2c, 0x00, 0x28, 0x50, 0xff },
};

typedef struct image {
    uint8_t *data;
    size_t size;
    size_t next_free;                              /* bump allocation in the extension */
} image;

static size_t phys(unsigned bank, unsigned addr) { return bank ? (size_t)bank * BANK + (addr - BANK) : addr; }
static unsigned u16(const image *im, size_t at) { return im->data[at] | im->data[at + 1] << 8; }
static void put16(image *im, size_t at, unsigned v) { im->data[at] = (uint8_t)v; im->data[at + 1] = (uint8_t)(v >> 8); }

static int fail(char *error, size_t capacity, const char *message)
{
    snprintf(error, capacity, "%s", message);
    return -1;
}

/* ---- layouts ------------------------------------------------------------------------ */

typedef struct layout_group { size_t root, table, base, copied; } layout_group;

static void group_of(const image *im, const OraclesTables *t, unsigned group, layout_group *g)
{
    g->root = phys(t->room_layout_group_table.bank, t->room_layout_group_table.addr) + group * 8u;
    const uint8_t *r = im->data + g->root;
    g->table = phys(r[1], r[2] | r[3] << 8);
    g->base = phys(r[4], r[5] | r[6] << 8);
}

/* The 80 metatiles of a small room as loadRoomLayout decodes them: raw, or by blocks of 8 or 16 with a common byte. */
static int decode(const image *im, const layout_group *g, unsigned room, uint8_t out[SMALL_LAYOUT])
{
    const unsigned word = u16(im, g->table + room * 2u);
    const size_t start = g->base + (word & 0x3fffu);
    if (start + READ_AHEAD > im->size) return -1;
    const uint8_t *in = im->data + start;
    if (!(word & 0xc000u)) { memcpy(out, in, SMALL_LAYOUT); return 0; }
    const unsigned block = (word & 0x8000u) ? 16u : 8u;
    size_t i = 0, o = 0;
    for (unsigned n = 0; n < SMALL_LAYOUT / block; n++) {
        unsigned key = in[i++];
        if (block == 16u) key |= (unsigned)in[i++] << 8;
        if (!key) { memcpy(out + o, in + i, block); i += block; o += block; continue; }
        const uint8_t common = in[i++];
        for (unsigned bit = 0; bit < block; bit++) out[o++] = (key >> bit & 1u) ? common : in[i++];
        if (i > READ_AHEAD) return -1;
    }
    return 0;
}

/* Moves a small layout group into the extension: its table into one bank, its streams from the next bank's start,
 * with every read-ahead and room for `layouts` raw layouts after them; the root points at the copies. */
static int relocate(image *im, const OraclesTables *t, unsigned group, unsigned layouts, layout_group *g, char *error, size_t capacity)
{
    group_of(im, t, group, g);
    if (im->data[g->root] != 1u) return fail(error, capacity, "a house's room is not in a small-room layout group");
    size_t end = 0;
    for (unsigned room = 0; room < 256u; room++) {
        const size_t reach = (u16(im, g->table + room * 2u) & 0x3fffu) + READ_AHEAD;
        if (reach > end) end = reach;
    }
    const size_t table = (im->next_free + BANK - 1u) / BANK * BANK, streams = table + BANK;
    if (streams + end + SMALL_LAYOUT * layouts + READ_AHEAD > im->size || end + SMALL_LAYOUT * layouts > 0x4000u) {
        snprintf(error, capacity, "too many houses: the rooms of layout group %u have room for %zu new layouts, %u are asked", group,
                 end < 0x4000u ? (0x4000u - end) / SMALL_LAYOUT : 0u, layouts);
        return -1;
    }
    memcpy(im->data + table, im->data + g->table, 512u);
    memcpy(im->data + streams, im->data + g->base, end);
    uint8_t *r = im->data + g->root;
    r[1] = (uint8_t)(table / BANK); r[2] = 0x00; r[3] = 0x40;
    r[4] = (uint8_t)(streams / BANK); r[5] = 0x00; r[6] = 0x40;
    g->table = table;
    g->base = streams;
    g->copied = end;
    im->next_free = streams + end + SMALL_LAYOUT * layouts + READ_AHEAD;
    return 0;
}

/* A raw layout after the copied streams, and the room's table entry pointing at it. */
static void store(image *im, layout_group *g, unsigned room, const uint8_t layout[SMALL_LAYOUT])
{
    memcpy(im->data + g->base + g->copied, layout, SMALL_LAYOUT);
    put16(im, g->table + room * 2u, (unsigned)g->copied);   /* bits 14-15 clear: raw */
    g->copied += SMALL_LAYOUT;
}

/* ---- the houses ------------------------------------------------------------------------ */

typedef struct placed {
    OraclesMod *mod;
    mod_house *house;
    int appended;                                  /* the room had no door: its first warp tile is the house's */
} placed;

static int refuse(char *error, size_t capacity, const placed *p, const char *message)
{
    snprintf(error, capacity, "mod %s: house %s in 0/%02x: %s", p->mod->id, p->house->name, p->house->room, message);
    return -1;
}

static size_t table_entry(const image *im, OraclesSym table, unsigned group)
{
    return phys(table.bank, u16(im, phys(table.bank, table.addr) + group * 2u));
}

/* The layout groups a room of `group` is read from: its tileset's (tilesetData, byte 6), or in Seasons, a seasonal
 * tileset ($ff, then a pointer to four entries), each season's.  Their count. */
static unsigned layouts_of(const image *im, const OraclesTables *t, unsigned group, unsigned room, unsigned out[4])
{
    const unsigned tileset = im->data[table_entry(im, t->room_tilesets_group_table, group) + room] & 0x7fu;
    const size_t entry = phys(t->tileset_data.bank, t->tileset_data.addr) + tileset * 8u;
    if (im->data[entry] != 0xffu) { out[0] = im->data[entry + 6u]; return 1; }
    const size_t seasons = phys(t->tileset_data.bank, u16(im, entry + 1u));
    for (unsigned season = 0; season < 4u; season++) out[season] = im->data[seasons + season * 8u + 6u];
    return 4;
}

/* Whether room `index` of every group that shares the interior group's tilesets (and so its room flags) is unused:
 * no warp to it or from it, no objects. */
static int index_unused(const image *im, const OraclesTables *t, const mod_warps *w, unsigned group, unsigned index)
{
    const unsigned tilesets = u16(im, phys(t->room_tilesets_group_table.bank, t->room_tilesets_group_table.addr) + group * 2u);
    for (unsigned g = 0; g < 8u; g++) {
        if (u16(im, phys(t->room_tilesets_group_table.bank, t->room_tilesets_group_table.addr) + g * 2u) != tilesets) continue;
        const size_t objects = phys(t->parse_object_data.bank, u16(im, table_entry(im, t->object_data_group_table, g) + index * 2u));
        if (im->data[objects] != 0xffu || mod_warps_is_destination(w, g, index) || mod_warps_has_sources(w, g, index)) return 0;
    }
    return 1;
}

/* The house's interior index.  Ages: its room's, which must have the interior's tileset and be unused.  Seasons: the
 * first unused index from the model's, with the tileset of the unused ones, and none a house before took. */
static int choose_interior(const image *im, const OraclesTables *t, const mod_warps *w, const house_model *m, placed *houses, unsigned n,
                           char *error, size_t capacity)
{
    placed *p = &houses[n];
    const size_t tilesets = table_entry(im, t->room_tilesets_group_table, m->interior_group);
    if (!m->first_free) {
        const unsigned room = p->house->room;
        if (im->data[tilesets + room] != m->interior_tileset) return refuse(error, capacity, p, "the interior's index has a room of its own (another tileset)");
        if (!index_unused(im, t, w, m->interior_group, room)) return refuse(error, capacity, p, "the interior's index is used by the game (objects or warps)");
        p->house->interior_room = (uint8_t)room;
        return 0;
    }
    const uint8_t unused = im->data[tilesets + 0xffu];
    for (unsigned index = m->first_free; index < 0x100u; index++) {
        int taken = 0;
        for (unsigned k = 0; k < n; k++) taken |= houses[k].house->interior_room == index;
        if (taken || im->data[tilesets + index] != unused || !index_unused(im, t, w, m->interior_group, index)) continue;
        p->house->interior_room = (uint8_t)index;
        return 0;
    }
    return refuse(error, capacity, p, "no unused interior index is left");
}

/* The door's tile: the middle of the facade's bottom row. */
static uint8_t door_of(const mod_house *h) { return (uint8_t)((h->row + 2u) << 4 | (h->col + 1u)); }

/* The house's warps: its two destinations, the exit at the interior's bottom edge, the door among the room's. */
static int place_warps(mod_warps *w, const house_model *m, placed *p, char *error, size_t capacity)
{
    const unsigned room = p->house->room, inside = p->house->interior_room, group = m->interior_group;
    const uint8_t door_yx = door_of(p->house);
    const uint8_t into[3] = { (uint8_t)inside, 0xff, 0x93 }, out_of[3] = { (uint8_t)room, door_yx, 0x01 };   /* centred from the bottom; on the door */
    uint8_t to_house = 0, to_town = 0;
    if (mod_warps_destination(w, group, into, &to_house) != 0 || mod_warps_destination(w, 0, out_of, &to_town) != 0)
        return refuse(error, capacity, p, "the warp destinations do not fit bank 04");
    const uint8_t leave[4] = { m->leave, (uint8_t)inside, to_town, 0x03 };
    const uint8_t door[4] = { 0x00, door_yx, to_house, (uint8_t)(group << 4 | 4u) };
    const int appended = mod_warps_door(w, room, door);
    if (mod_warps_append(w, group, leave) != 0 || appended < 0) return refuse(error, capacity, p, "the warp lists do not fit bank 04");
    p->appended = appended;
    fprintf(stderr, "oracles: mod %s: house %s in 0/%02x (door %02x), interior %u/%02x, warp destinations 0:%02x and %u:%02x\n",
            p->mod->id, p->house->name, room, door_yx, group, inside, to_town, group, to_house);
    return 0;
}

int mod_compose(OraclesMod *const *mods, size_t count, const uint8_t *base, size_t base_size, uint8_t **out, size_t *out_size, char *error, size_t capacity)
{
    *out = NULL;
    placed houses[ORACLES_MOD_SET_MAX * MOD_HOUSES];
    unsigned house_count = 0;
    for (size_t m = 0; m < count; m++)
        for (unsigned h = 0; h < mods[m]->house_count; h++) houses[house_count++] = (placed){ mods[m], &mods[m]->houses[h], 0 };
    if (!house_count) return 0;
    const OraclesGame game = houses[0].mod->game;
    for (unsigned i = 0; i < house_count; i++) {
        const placed *p = &houses[i];
        if (p->mod->game != game) return refuse(error, capacity, p, "the mods are for two games");
        if (p->house->col > 7u || p->house->row > 4u) return refuse(error, capacity, p, "the facade leaves the room (three by three, the door's front below it)");
        for (unsigned k = 0; k < i; k++) {
            if (houses[k].house->room != p->house->room) continue;
            snprintf(error, capacity, "mod %s: house %s and mod %s: house %s are both in 0/%02x: one house a room", houses[k].mod->id, houses[k].house->name,
                     p->mod->id, p->house->name, p->house->room);
            return -1;
        }
    }
    if (base_size >= IMAGE_SIZE) return fail(error, capacity, "the ROM is already extended");
    const OraclesTables *t = game == ORACLES_GAME_AGES ? &oracles_tables_ages : &oracles_tables_seasons;
    const house_model *model = game == ORACLES_GAME_AGES ? &ages_house : &seasons_house;
    image im = { calloc(1, IMAGE_SIZE), IMAGE_SIZE, base_size };
    if (!im.data) return fail(error, capacity, "out of memory");
    memcpy(im.data, base, base_size);
    mod_warps *w = NULL;
    uint64_t *before = NULL, *after = NULL;
    int status = -1;
    if (mod_warps_open(&w, im.data, im.size, t, game == ORACLES_GAME_AGES, error, capacity) != 0) goto done;
    if (!(before = mod_warps_lookups(w))) { fail(error, capacity, "out of memory"); goto done; }
    const size_t towns_tilesets = table_entry(&im, t->room_tilesets_group_table, 0);
    for (unsigned i = 0; i < house_count; i++) {
        const unsigned tileset = im.data[towns_tilesets + houses[i].house->room] & 0x7fu;
        if (tileset != model->facade_tileset) {
            char message[160];
            snprintf(message, sizeof message, "the facade is drawn for rooms of tileset $%02x, this room has $%02x (its metatiles would draw something else)",
                     model->facade_tileset, tileset);
            refuse(error, capacity, &houses[i], message);
            goto done;
        }
        if (choose_interior(&im, t, w, model, houses, i, error, capacity) != 0) goto done;
    }

    /* Layouts: the groups of the towns' rooms (one a season in Seasons) and of the interiors move, then the facades and
     * the interior are stored. */
    unsigned interiors[1];                         /* the interior tileset's layout group */
    interiors[0] = im.data[phys(t->tileset_data.bank, t->tileset_data.addr) + model->interior_tileset * 8u + 6u];
    unsigned stored[MAX_LAYOUT_GROUPS] = { 0 };
    layout_group groups[MAX_LAYOUT_GROUPS];
    for (unsigned i = 0; i < house_count; i++) {
        unsigned towns[4];
        const unsigned n = layouts_of(&im, t, 0, houses[i].house->room, towns);
        for (unsigned k = 0; k < n; k++) {
            if (towns[k] >= MAX_LAYOUT_GROUPS || interiors[0] >= MAX_LAYOUT_GROUPS) { refuse(error, capacity, &houses[i], "the room's layout group is unknown"); goto done; }
            if (towns[k] == interiors[0]) { refuse(error, capacity, &houses[i], "the town and the interior share a layout group"); goto done; }
            stored[towns[k]]++;
        }
    }
    stored[interiors[0]] = 1;                      /* the interiors share one layout */
    for (unsigned g = 0; g < MAX_LAYOUT_GROUPS; g++)
        if (stored[g] && relocate(&im, t, g, stored[g], &groups[g], error, capacity) != 0) goto done;
    layout_group *inside = &groups[interiors[0]];
    for (unsigned i = 0; i < house_count; i++) {
        const mod_house *h = houses[i].house;
        unsigned towns[4];
        const unsigned n = layouts_of(&im, t, 0, h->room, towns);
        for (unsigned k = 0; k < n; k++) {
            uint8_t layout[SMALL_LAYOUT];
            if (decode(&im, &groups[towns[k]], h->room, layout) != 0) { refuse(error, capacity, &houses[i], "the room's layout cannot be decoded"); goto done; }
            for (unsigned r = 0; r < 3u; r++) memcpy(layout + (h->row + r) * 10u + h->col, model->facade + r * 3u, 3);
            layout[(h->row + 3u) * 10u + h->col + 1u] = model->path;
            store(&im, &groups[towns[k]], h->room, layout);
        }
        if (i == 0) store(&im, inside, h->interior_room, model->interior);
        else put16(&im, inside->table + h->interior_room * 2u, u16(&im, inside->table + houses[0].house->interior_room * 2u));
    }

    /* The interiors' tileset, music and keeper (one object list in the tail of the objects' bank). */
    const size_t objects_at = phys(t->object_bank_free.bank, t->object_bank_free.addr);
    if (t->object_bank_free.bank != t->parse_object_data.bank || 0x8000u - t->object_bank_free.addr < sizeof model->keeper) {
        fail(error, capacity, "the objects' bank has no free tail");
        goto done;
    }
    memcpy(im.data + objects_at, model->keeper, sizeof model->keeper);
    const size_t tilesets = table_entry(&im, t->room_tilesets_group_table, model->interior_group);
    const size_t music = table_entry(&im, t->music_assignment_group_table, model->interior_group);
    const size_t objects = table_entry(&im, t->object_data_group_table, model->interior_group);
    for (unsigned i = 0; i < house_count; i++) {
        mod_house *h = houses[i].house;
        h->interior_group = model->interior_group;
        im.data[tilesets + h->interior_room] = model->interior_tileset;
        im.data[music + h->interior_room] = im.data[music + model->music_like];
        put16(&im, objects + h->interior_room * 2u, (unsigned)(objects_at % BANK + BANK));
        if (place_warps(w, model, &houses[i], error, capacity) != 0) goto done;
        for (unsigned k = 0; k < houses[i].mod->npc_count; k++) {   /* the keeper stands in the interior */
            mod_npc *npc = &houses[i].mod->npcs[k];
            if (npc->house >= 0 && &houses[i].mod->houses[npc->house] == h) { npc->group = h->interior_group; npc->room = h->interior_room; }
        }
    }
    if (mod_warps_commit(w, error, capacity) != 0) goto done;

    /* Every warp of the game leads where it led, but the houses' doors and exits. */
    mod_warps_house checked[ORACLES_MOD_SET_MAX * MOD_HOUSES];
    for (unsigned i = 0; i < house_count; i++)
        checked[i] = (mod_warps_house){ houses[i].house->room, door_of(houses[i].house), houses[i].house->interior_room, houses[i].appended };
    if (!(after = mod_warps_lookups(w))) { fail(error, capacity, "out of memory"); goto done; }
    if (mod_warps_check(before, after, checked, house_count, model->interior_group, model->leave, error, capacity) != 0) goto done;

    /* The header of a 2 MiB cartridge, and its checksums. */
    im.data[0x148] = 0x06;
    uint8_t x = 0;
    for (size_t i = 0x134; i < 0x14d; i++) x = (uint8_t)(x - im.data[i] - 1u);
    im.data[0x14d] = x;
    unsigned sum = 0;
    for (size_t i = 0; i < im.size; i++) if (i != 0x14e && i != 0x14f) sum += im.data[i];
    im.data[0x14e] = (uint8_t)(sum >> 8);
    im.data[0x14f] = (uint8_t)sum;
    fprintf(stderr, "oracles: %u house(s) composed, %u bytes of bank 04 left\n", house_count, mod_warps_free_bytes(w));
    *out = im.data;
    *out_size = im.size;
    im.data = NULL;
    status = 0;
done:
    free(before);
    free(after);
    mod_warps_close(w);
    free(im.data);
    return status;
}
