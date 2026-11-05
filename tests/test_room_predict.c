/* The prediction of the collisions of the room a scroll enters, on the synthetic tables of room_predict_fixture.h:
 * the collisions of a layout, each substitution of applyAllTileSubstitutions, the rooms left to the game's load, and
 * the whole prediction. */
#include "room_predict.h"
#include "room_predict_fixture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint8_t global_flags[0x10];
static uint8_t gasha_planted[2];

/* A frame of the load of room 0:12 of Ages: wScrollMode 8, collision mode 0, the overworld's flags. */
static OraclesGuestTransitionState loading(void)
{
    OraclesGuestTransitionState s;
    memset(&s, 0, sizeof s);
    s.game = ORACLES_GAME_AGES;
    s.scroll_mode = 8; s.transition_state = 3;
    s.active_group = 0; s.active_room = 0x12; s.tileset_flags = 0x01;
    s.load.global_flags = global_flags; s.load.global_flags_size = sizeof global_flags;
    s.load.gasha_spots_planted = gasha_planted; s.load.gasha_spots_planted_size = sizeof gasha_planted;
    s.load.chest_opened_tile = 0xf0; s.load.roomflag_item = 0x20; s.load.timewarp_transition = 0x06;
    s.load.pollution_fixed_flag = 0x30; s.load.finished_game_flag = 0x14; s.load.temple_lava_flag = 0xff;
    s.load.pirate_ship_flag = 0x40; s.load.intro_done_flag = 0x0a;
    s.load.portal_group = 0xff;
    s.load.given = 0x3f; s.load.room_code_replay = 1;
    s.moblin_keep_bank = FIXTURE_BANK; s.moblin_keep_table = FIXTURE_MOBLIN_KEEP;
    return s;
}

/* The room with a transition of its own (mapTransitionGroupTable), $ff none. */
static uint8_t special = 0xff;
static int special_room(const OraclesGuestTransitionState *state, uint8_t room, void *opaque)
{
    (void)state; (void)opaque;
    return room == special;
}

static int count(const uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES], uint8_t tile)
{
    int n = 0;
    for (unsigned i = 0; i < ORACLES_ROOM_LAYOUT_BYTES; i++) n += layout[i] == tile;
    return n;
}

int main(void)
{
    uint8_t *rom_bytes = calloc(FIXTURE_ROM_SIZE, 1);
    CHECK(rom_bytes != NULL);
    if (!rom_bytes) return 1;
    const OraclesRom rom = { rom_bytes, FIXTURE_ROM_SIZE };
    fixture f;
    uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES], collisions[ORACLES_ROOM_LAYOUT_BYTES], table[ORACLES_TILESET_COLLISIONS_BYTES];

    /* loadRoomCollisions: each tile's collision; past a small room's row 7 and column 9, and the sixteenth column, $ff. */
    memset(table, 0, sizeof table);
    table[0x3a] = 0x0f;
    memset(layout, 0, sizeof layout);
    layout[0x23] = 0x3a;
    oracles_room_predict_collisions(table, layout, 1, collisions);
    CHECK(collisions[0x23] == 0x0f && collisions[0x22] == 0 && collisions[0x79] == 0);
    CHECK(collisions[0x0a] == 0xff && collisions[0xaa] == 0xff && collisions[0x0f] == 0xff && collisions[0xaf] == 0xff);
    CHECK(collisions[0x80] == 0xff && collisions[0x8e] == 0xff && collisions[0x90] == 0 && collisions[0xa9] == 0);
    oracles_room_predict_collisions(table, layout, 0, collisions);
    CHECK(collisions[0x0a] == 0 && collisions[0x80] == 0 && collisions[0x0f] == 0xff);

    /* The pollution (Ages): $eb outdoors becomes $fc, $3b under water, once the flag is set. */
    fixture_init(&f, rom_bytes);
    OraclesGuestTransitionState s = loading();
    memset(layout, 0, sizeof layout);
    layout[0x11] = 0xeb; layout[0x45] = 0xeb;
    CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && count(layout, 0xeb) == 2);
    global_flags[0x30 >> 3] = 1u << (0x30 & 7u);
    CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x11] == 0xfc && layout[0x45] == 0xfc);
    layout[0x11] = 0xeb; s.tileset_flags = 0x41;
    CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x11] == 0x3b);
    layout[0x11] = 0xeb; s.tileset_flags = 0x00;   /* indoors */
    CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x11] == 0xeb);
    s.game = ORACLES_GAME_SEASONS; s.tileset_flags = 0x01;   /* Seasons has no pollution */
    CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x11] == 0xeb);
    s.game = ORACLES_GAME_AGES; f.tables.pollution_above_water = (OraclesSym){ 0, 0 };   /* its list not bound: refused */
    CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICT_UNREADABLE);
    memset(global_flags, 0, sizeof global_flags);

    /* Single tile changes: the room's flags against the mask; in Ages' engine $f0 unlinked, $f1 linked, and $f2 the game
     * finished, whose absence ends the list. */
    {
        const uint8_t list[] = { 0x13, 0x40, 0x22, 0xd7,   /* another room */
                                 0x12, 0x40, 0x23, 0xd7,   /* bit 6 of the room's flags */
                                 0x12, 0xf0, 0x24, 0x64,   /* unlinked */
                                 0x12, 0xf1, 0x25, 0x65,   /* linked */
                                 0x12, 0xf2, 0x26, 0xf2,   /* finished: the end of the list unless set */
                                 0x12, 0x01, 0x27, 0x3a,
                                 0x00, 0x00 };
        fixture_init(&f, rom_bytes);
        fixture_group_lists(&f, f.tables.single_tile_change_group_table.addr, (const uint8_t[]){ 0, 0 }, 2, 0, list, sizeof list);
        s = loading();
        s.load.room_flags = 0x41;
        memset(layout, 0, sizeof layout);
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED);
        CHECK(layout[0x22] == 0 && layout[0x23] == 0xd7 && layout[0x24] == 0x64 && layout[0x25] == 0 && layout[0x26] == 0 && layout[0x27] == 0);
        global_flags[0x14 >> 3] = 1u << (0x14 & 7u);
        s.load.is_linked_game = 1;
        memset(layout, 0, sizeof layout);
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED);
        CHECK(layout[0x24] == 0 && layout[0x25] == 0x65 && layout[0x26] == 0xf2 && layout[0x27] == 0x3a);
        /* Seasons reads every mask as one of the room's flags. */
        s.game = ORACLES_GAME_SEASONS; s.load.room_flags = 0x10;
        memset(layout, 0, sizeof layout);
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED);
        CHECK(layout[0x23] == 0 && layout[0x24] == 0x64 && layout[0x25] == 0x65 && layout[0x26] == 0xf2 && layout[0x27] == 0);
        memset(global_flags, 0, sizeof global_flags);
        /* A position past the room's 176 bytes: unreadable. */
        const uint8_t past_list[] = { 0x12, 0x01, 0xc0, 0x3a, 0x00, 0x00 };
        fixture_group_lists(&f, f.tables.single_tile_change_group_table.addr, (const uint8_t[]){ 0, 0 }, 2, 0, past_list, sizeof past_list);
        s = loading(); s.load.room_flags = 0x01;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICT_UNREADABLE);
    }

    /* Standard substitutions: each of bits 0 to 3 and 7 of the room's flags replaces its list's tiles, the list of the
     * collision mode in Ages, of the group in Seasons. */
    {
        fixture_init(&f, rom_bytes);
        const uint8_t bit1[] = { 0x34, 0x30, 0x35, 0x31, 0x00 }, bit7[] = { 0xa0, 0x1e, 0x00 };
        const unsigned at1 = fixture_bytes(&f, bit1, sizeof bit1), at7 = fixture_bytes(&f, bit7, sizeof bit7);
        fixture_word(&f, f.tables.standard_tile_substitutions.addr + 2u * (1u * 6u + 2u), at1);   /* Ages: bit 1, collision mode 2 */
        fixture_word(&f, f.tables.standard_tile_substitutions.addr + 2u * (4u * 6u + 2u), at7);   /* bit 7 */
        s = loading();
        s.active_collisions = 2; s.load.room_flags = 0x82;
        memset(layout, 0, sizeof layout);
        layout[0x05] = 0x30; layout[0x06] = 0x31; layout[0x15] = 0x30; layout[0x40] = 0x1e;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED);
        CHECK(layout[0x05] == 0x34 && layout[0x06] == 0x35 && layout[0x15] == 0x34 && layout[0x40] == 0xa0);
        s.load.room_flags = 0x02; s.active_collisions = 3;
        memset(layout, 0, sizeof layout);
        layout[0x05] = 0x30;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x05] == 0x30);
        /* Seasons: by the group, eight lists a bit. */
        fixture_word(&f, f.tables.standard_tile_substitutions.addr + 2u * (1u * 8u + 3u), at1);
        s.game = ORACLES_GAME_SEASONS; s.active_group = 3; s.active_collisions = 0;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x05] == 0x34);
        s.game = ORACLES_GAME_AGES; s.active_collisions = 6;   /* past Ages' six lists */
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICT_UNREADABLE);
    }

    /* The opened chest: the room's flag, at the chest's position for this room, past the layout where the group lists none. */
    {
        fixture_init(&f, rom_bytes);
        const uint8_t chests[] = { 0x33, 0x11, 0x28, 0x00, 0x47, 0x12, 0x29, 0x01, 0xff };
        fixture_group_lists(&f, f.tables.chest_data_group_table.addr, (const uint8_t[]){ 0xff }, 1, 0, chests, sizeof chests);
        s = loading();
        memset(layout, 0, sizeof layout);
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && count(layout, 0xf0) == 0);
        s.load.room_flags = 0x20;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x47] == 0xf0 && count(layout, 0xf0) == 1);
        s.active_room = 0x14;
        memset(layout, 0, sizeof layout);
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && count(layout, 0xf0) == 0);
    }

    /* Ages' overworld: the tile under the portal and under a time warp's arrival (the entry's dictionary), and under
     * Link sent back (the return's). */
    {
        fixture_init(&f, rom_bytes);
        const uint8_t entry[] = { 0xc5, 0x3a, 0x04, 0x3a, 0x00 }, back[] = { 0xc0, 0x3a, 0xf2, 0x3a, 0x00 };
        for (unsigned i = 0; i < sizeof entry; i++) fixture_put(&f, f.tables.timewarp_entry_tile_replacement_dict.addr + i, entry[i]);
        for (unsigned i = 0; i < sizeof back; i++) fixture_put(&f, f.tables.timewarp_return_tile_replacement_dict.addr + i, back[i]);
        s = loading();
        memset(layout, 0, sizeof layout);
        layout[0x34] = 0xc5; layout[0x35] = 0x04; layout[0x36] = 0xf2; layout[0x37] = 0xc5;
        s.load.portal_group = 0; s.load.portal_room = 0x12; s.load.portal_pos = 0x34;
        s.load.warp_transition = 0x86; s.load.warp_dest_pos = 0x35;
        s.load.link_time_warp_tile = 0x37;   /* position $36, plus one */
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED);
        CHECK(layout[0x34] == 0x3a && layout[0x35] == 0x3a && layout[0x36] == 0x3a && layout[0x37] == 0xc5);
        /* Another room's portal, a warp that is no time warp, a tile neither dictionary holds: nothing. */
        memset(layout, 0, sizeof layout);
        layout[0x34] = 0xc5; layout[0x35] = 0x04; layout[0x36] = 0xc5;
        s.load.portal_room = 0x13; s.load.warp_transition = 0x05;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED);
        CHECK(layout[0x34] == 0xc5 && layout[0x35] == 0x04 && layout[0x36] == 0xc5);
        /* Groups 2 and 3, and Seasons: none of them. */
        s.load.portal_room = 0x12; s.load.warp_transition = 0x06; s.active_group = 2; s.load.portal_group = 2;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x34] == 0xc5 && layout[0x35] == 0x04);
        s.active_group = 0; s.load.portal_group = 0; s.game = ORACLES_GAME_SEASONS;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x34] == 0xc5 && layout[0x35] == 0x04);
        /* A position past the room: unreadable. */
        s.game = ORACLES_GAME_AGES; s.load.portal_pos = 0xc4;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICT_UNREADABLE);
    }

    /* The rooms left to the game's load: their own code before the collisions (every small group in Ages, groups 0, 1
     * and 3 in Seasons); after the graphics, but where it only draws, or a Gasha spot with nothing planted. */
    {
        fixture_init(&f, rom_bytes);
        const uint8_t code[] = { 0x12, 0x08, 0x00 }, after[] = { 0x43, 0x01, 0x44, 0x06, 0x45, 0x08, 0x46, 0x0b, 0x00 };
        fixture_group_lists(&f, f.tables.room_tile_changer_code_group_table.addr, (const uint8_t[]){ 0 }, 1, 2, code, sizeof code);
        fixture_group_lists(&f, f.tables.tile_changes_after_gfx_group_table.addr, (const uint8_t[]){ 0 }, 1, 0, after, sizeof after);
        const uint8_t spots[] = { 0x05, 0x2c, 0x45, 0x7b };
        for (unsigned i = 0; i < sizeof spots; i++) fixture_put(&f, f.tables.gasha_spot_rooms.addr + i, spots[i]);
        s = loading();
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICTED);
        s.active_group = 2;
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICT_ROOM_CODE);
        s.game = ORACLES_GAME_SEASONS;   /* Seasons' group 2 runs no room code */
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICTED);
        s.active_group = 4;
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICT_LARGE);
        s = loading();
        s.active_room = 0x43;   /* Ages' $01: a tree's graphics */
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICTED);
        s.active_room = 0x44;   /* $06: the Maku tree's puddles, in the layout */
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICT_AFTER_GFX);
        s.game = ORACLES_GAME_SEASONS;   /* Seasons' $06: Blaino's gloves, drawn */
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICTED);
        s.active_room = 0x46;   /* $0b in Seasons, a tree drawn; past Ages' routines */
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICTED);
        s.game = ORACLES_GAME_AGES;
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICT_AFTER_GFX);
        s.active_room = 0x45;   /* the Gasha spot of index 2 */
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICTED);
        gasha_planted[0] = 0x04;
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICT_AFTER_GFX);
        gasha_planted[0] = 0;
        /* Those that change the layout under a condition: tested first. */
        {
            const uint8_t conditional[] = { 0x47, 0x00, 0x48, 0x01, 0x49, 0x03, 0x4a, 0x0e, 0x00 };
            fixture_group_lists(&f, f.tables.tile_changes_after_gfx_group_table.addr, (const uint8_t[]){ 0 }, 1, 0, conditional, sizeof conditional);
            s = loading();
            s.active_room = 0x47;   /* Ages' $00: dungeon 2's entrance, collapsed once bit 7 is set */
            CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICTED);
            s.load.room_flags = 0x80;
            CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICT_AFTER_GFX);
            s.game = ORACLES_GAME_SEASONS; s.load.room_flags = 0;   /* Seasons' $00 and $01: the pirate ship, once docked */
            CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICTED);
            s.active_room = 0x48;
            CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICTED);
            global_flags[0x40 >> 3] = 1u << (0x40 & 7u);
            CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICT_AFTER_GFX);
            s.active_room = 0x49;   /* $03: Din's troupe's tents while bit 6 is clear */
            CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICT_AFTER_GFX);
            s.load.room_flags = 0x40;
            CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICTED);
            s.active_room = 0x4a;   /* $0e: the wagon until the intro is done */
            CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICT_AFTER_GFX);
            global_flags[0x0a >> 3] |= 1u << (0x0a & 7u);
            CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICTED);
            memset(global_flags, 0, sizeof global_flags);
        }
        /* Seasons' Temple Remains filled with lava: its layouts are other rooms'. */
        s = loading();
        s.game = ORACLES_GAME_SEASONS; s.load.temple_lava_flag = 0x15; s.active_room = 0x25;
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICTED);
        global_flags[0x15 >> 3] = 1u << (0x15 & 7u);
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICT_OTHER_LAYOUT);
        s.active_room = 0x28;
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICTED);
        memset(global_flags, 0, sizeof global_flags);
    }

    /* The whole prediction: the room's layout from its group's table, its substitutions, and the collisions of the
     * tileset layout named. */
    {
        fixture_init(&f, rom_bytes);
        fixture_room_tile(&f, 0x12, 3, 2, 0x3a);
        fixture_room_tile(&f, 0x12, 9, 7, 0x30);
        fixture_collision(&f, 0x3a, 0x0f);
        fixture_collision(&f, 0x34, 0x05);
        fixture_collision(&f, 0x00, 0x00);
        const uint8_t bit0[] = { 0x34, 0x30, 0x00 };
        fixture_word(&f, f.tables.standard_tile_substitutions.addr, fixture_bytes(&f, bit0, sizeof bit0));
        s = loading();
        s.load.room_flags = 0x01;
        CHECK(oracles_room_predict(&rom, &f.tables, &s, layout, collisions) == ORACLES_ROOM_PREDICTED);
        CHECK(layout[0x23] == 0x3a && layout[0x79] == 0x34 && count(layout, 0) == ORACLES_ROOM_LAYOUT_BYTES - 2);
        CHECK(collisions[0x23] == 0x0f && collisions[0x79] == 0x05 && collisions[0x22] == 0 && collisions[0x8a] == 0xff && collisions[0x9a] == 0xff);
        /* The room not named yet, a group of large rooms, an image whose tables are not bound, a tileset layout past the table. */
        s.scroll_mode = 4;
        CHECK(oracles_room_predict(&rom, &f.tables, &s, layout, collisions) == ORACLES_ROOM_PREDICT_EARLY);
        s.scroll_mode = 8; s.active_group = 5;
        CHECK(oracles_room_predict(&rom, &f.tables, &s, layout, collisions) == ORACLES_ROOM_PREDICT_LARGE);
        s.active_group = 0;
        OraclesTables unbound = f.tables;
        unbound.tileset_layout_table = (OraclesSym){ 0, 0 };
        CHECK(oracles_room_predict(&rom, &unbound, &s, layout, collisions) == ORACLES_ROOM_PREDICT_UNQUALIFIED);
        s.game = (OraclesGame)99;
        CHECK(oracles_room_predict(&rom, &f.tables, &s, layout, collisions) == ORACLES_ROOM_PREDICT_UNQUALIFIED);
        s.game = ORACLES_GAME_AGES; s.load.tileset_layout = 64;
        CHECK(oracles_room_predict(&rom, &f.tables, &s, layout, collisions) == ORACLES_ROOM_PREDICT_UNREADABLE);
        /* A large room's group table (mode 0): left to the game. */
        s.load.tileset_layout = 0; s.load.tileset_layout_group = 3;
        fixture_put(&f, 0x4000u + 8u * 3u, 0);
        CHECK(oracles_room_predict(&rom, &f.tables, &s, layout, collisions) != ORACLES_ROOM_PREDICTED);
    }
    /* The room beside on Ages' sea floor: group 2, underwater tilesets, no map and no room pack; the room a row or a
     * column away by a byte's sum, as updateActiveRoom takes it. */
    {
        fixture_init(&f, rom_bytes);
        fixture_word(&f, 0x5d00u + 2u * 2u, 0x5800);
        for (unsigned r = 0; r < 256u; r++) fixture_put(&f, 0x5800u + r, 0x02);
        fixture_put(&f, 0x5e20, 0x0f); fixture_put(&f, 0x5e21, 0x40);   /* tileset 2: under water */
        fixture_room_tile(&f, 0x03, 3, 2, 0x3a);
        fixture_collision(&f, 0x3a, 0x0f);
        uint8_t flags[256], room = 0;
        memset(flags, 0, sizeof flags);
        static const uint8_t no_objects[0x1000];
        s = loading();
        s.active_group = 2; s.active_room = 0x13; s.tileset_flags = 0x40;
        s.load.room_flags_page = flags; s.load.objects = no_objects;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED);
        CHECK(room == 0x03 && layout[0x23] == 0x3a && collisions[0x23] == 0x0f);
        s.active_room = 0x10;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 3, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED && room == 0x0f);
        s.active_room = 0x13;
        fixture_put(&f, 0x5803, 0x00);   /* the room above outdoors */
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        fixture_put(&f, 0x5803, 0x02);
        s.tileset_flags = 0x48;   /* a dungeon under the sea */
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        s.tileset_flags = 0x40; s.game = ORACLES_GAME_SEASONS;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
    }
    /* The room beside the room entered across the scroll's axis: room 0:13 entered, the room the next scroll would
     * enter on each side, its tileset made again from the tables, its own flags, within the map and one room pack, no
     * room with a transition of its own. */
    {
        fixture_init(&f, rom_bytes);
        fixture_room_tile(&f, 0x03, 3, 2, 0x3a);
        fixture_room_tile(&f, 0x14, 9, 7, 0x3a);
        fixture_collision(&f, 0x3a, 0x0f);
        uint8_t flags[256], room = 0;
        memset(flags, 0, sizeof flags);
        s = loading();
        s.active_room = 0x13; s.map_width = 14; s.map_height = 14;
        s.room_pack_bank = FIXTURE_BANK; s.room_pack_table = FIXTURE_ROOM_PACKS;
        s.load.room_flags_page = flags;
        static const uint8_t no_objects[0x1000];
        s.load.objects = no_objects;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED);
        CHECK(room == 0x03 && layout[0x23] == 0x3a && collisions[0x23] == 0x0f && collisions[0x8a] == 0xff);
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED);
        CHECK(room == 0x14 && collisions[0x79] == 0x0f && collisions[0x23] == 0);
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 2, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED && room == 0x23);
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 3, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED && room == 0x12);
        /* Its own flags: bit 0 of the standard substitutions. */
        const uint8_t bit0[] = { 0x34, 0x3a, 0x00 };
        fixture_word(&f, f.tables.standard_tile_substitutions.addr, fixture_bytes(&f, bit0, sizeof bit0));
        s.load.room_flags = 0x01;   /* the room entered's, not the one beside's */
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED && layout[0x23] == 0x3a);
        flags[0x03] = 0x02;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED && layout[0x23] == 0x3a);
        flags[0x03] = 0x01;   /* in Ages bit 0 swaps its group too: group 2's tileset, here a dungeon's */
        fixture_word(&f, 0x5d00u + 2u * 2u, 0x5f00);
        fixture_put(&f, 0x5f03, 0x01);
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        s.game = ORACLES_GAME_SEASONS;   /* not in Seasons: the substitution alone */
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED && layout[0x23] == 0x34);
        s.game = ORACLES_GAME_AGES; flags[0x03] = 0;
        /* Another room pack: a fade, or another season; Ages compares their low seven bits. */
        fixture_put(&f, FIXTURE_ROOM_PACKS + 0x14u, 0x81);
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        fixture_put(&f, FIXTURE_ROOM_PACKS + 0x13u, 0x01); fixture_put(&f, FIXTURE_ROOM_PACKS + 0x14u, 0x81);
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED);
        s.game = ORACLES_GAME_SEASONS;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        s.game = ORACLES_GAME_AGES;
        /* Off the map, a room with a transition of its own, not the overworld, the room not named yet. */
        s.active_room = 0x0d;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        s.active_room = 0x13;
        special = 0x12;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 3, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, NULL, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        special = 0xff;
        s.tileset_flags = 0x04;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        s.tileset_flags = 0x01; s.active_group = 2;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        s.active_group = 0; s.scroll_mode = 4;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_EARLY);
        s.scroll_mode = 8; s.room_pack_bank = 0xff;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_UNQUALIFIED);
        s.room_pack_bank = FIXTURE_BANK;
        fixture_put(&f, FIXTURE_ROOM_PACKS + 0x13u, 0x00); fixture_put(&f, FIXTURE_ROOM_PACKS + 0x14u, 0x00);
        /* An object of the room entered that may leave the player without the controls (room_scenes.c), among its live
         * objects: an interaction marked for every subid, one marked for some subids alone, an enemy; the room left's,
         * enabled 2 since the load, and an object not marked count for nothing; the objects not given, not yet. */
        unsigned whole = 0, quiet = 0, enemy = 0;
        while (!((oracles_room_scenes_ages[0][whole >> 3] >> (whole & 7u)) & 1u)) whole++;
        for (int again = 1; again;) {
            again = 0;
            while (((oracles_room_scenes_ages[0][quiet >> 3] >> (quiet & 7u)) & 1u) || quiet == 0u) quiet++;
            for (const OraclesRoomSceneSubids *p = oracles_room_scene_subids_ages; p->id; p++) if (p->id == quiet) { quiet++; again = 1; }
        }
        while (!((oracles_room_scenes_ages[1][enemy >> 3] >> (enemy & 7u)) & 1u)) enemy++;
        const OraclesRoomSceneSubids part = oracles_room_scene_subids_ages[0];
        unsigned some = 0, other = 0;
        while (!((part.subids >> some) & 1u)) some++;
        while ((part.subids >> other) & 1u) other++;
        CHECK(part.id != 0 && other < 64u);
        static uint8_t objects[0x1000];
        memset(objects, 0, sizeof objects);
        const OraclesRoomScenes ages_scenes = { oracles_room_scenes_ages, oracles_room_scene_subids_ages };
        uint8_t *inter = objects + 0x240, *inter2 = objects + 0x340, *foe = objects + 0x480;
        inter[0] = 1; inter[1] = (uint8_t)quiet;
        inter2[0] = 1; inter2[1] = part.id; inter2[2] = (uint8_t)other;
        s.load.objects = objects;
        CHECK(oracles_room_predict_scene(&ages_scenes, objects) == 0);
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED);
        inter2[2] = (uint8_t)some;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_SCENE);
        inter2[0] = 2;   /* the room left's */
        CHECK(oracles_room_predict_scene(&ages_scenes, objects) == 0);
        inter[1] = (uint8_t)whole;
        CHECK(oracles_room_predict_scene(&ages_scenes, objects) == 1);
        inter[0] = 0;
        foe[0] = 1; foe[1] = (uint8_t)enemy;
        CHECK(oracles_room_predict_scene(&ages_scenes, objects) == 1);
        foe[0] = 0;
        s.load.objects = NULL;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_EARLY);
    }
    /* The rooms whose own code is replayed (applyRoomSpecificTileChanges): each routine for its own room, a byte it
     * reads that is not given refused; a routine given for another room refused. */
    {
        uint8_t all[0x400], page0[256];
        memset(all, 0, sizeof all);
        fixture_init(&f, rom_bytes);
        /* Ages' present library (0:a5, $37): its two tiles once wPastRoomFlags+$a5 has bit 7. */
        const uint8_t library[] = { 0xa5, 0x37, 0x2c, 0x19, 0x61, 0x21, 0x0b, 0x26, 0x13, 0x37, 0x00 };
        fixture_group_lists(&f, f.tables.room_tile_changer_code_group_table.addr, (const uint8_t[]){ 0 }, 1, 0, library, sizeof library);
        fixture_room_tile(&f, 0x25, 2, 2, 0x5a);
        s = loading();
        s.active_room = 0xa5;
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICTED);
        memset(layout, 0x11, sizeof layout);
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICT_ROOM_CODE);   /* its flags not given */
        s.load.room_flags_all = all; s.load.room_flags_all_size = sizeof all;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && count(layout, 0x11) == ORACLES_ROOM_LAYOUT_BYTES);
        all[0x100 + 0xa5] = 0x80;
        s.load.room_flags = 0x80;   /* the room's own flags are not the past's */
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x22] == 0xee && layout[0x23] == 0xef);
        CHECK(count(layout, 0x11) == ORACLES_ROOM_LAYOUT_BYTES - 2);
        all[0x100 + 0xa5] = 0; s.load.room_flags = 0;
        /* The cave by dungeon 5 (0:0b, $26): wPresentRoomFlags+$0a, bit 6. */
        s.active_room = 0x0b;
        memset(layout, 0, sizeof layout);
        all[0x0a] = 0x40;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x43] == 0xdd && count(layout, 0) == ORACLES_ROOM_LAYOUT_BYTES - 1);
        all[0x0a] = 0;
        /* A vine's screen (0:2c, $19): the vine grown at $17, its sides, its tiles for their placeholders, the tile under
         * its base; withered where it stands elsewhere and the tile there has no collision, which needs the tileset's. */
        s.active_room = 0x2c;
        memset(layout, 0, sizeof layout);
        layout[0x17] = 0x05; layout[0x27] = 0x8e; layout[0x37] = 0x8f;
        all[0x1f0] = 0x17;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED);
        CHECK(layout[0x06] == 0x5d && layout[0x08] == 0x60 && layout[0x17] == 0xd4 && layout[0x27] == 0xd5 && layout[0x37] == 0xd6 && layout[0x47] == 0x8d);
        all[0x1f0] = 0x33;
        memset(layout, 0, sizeof layout);
        layout[0x33] = 0x3a;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICT_ROOM_CODE);
        memset(table, 0, sizeof table);
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, table, layout) == ORACLES_ROOM_PREDICTED && layout[0x33] == 0x8c);
        layout[0x33] = 0x3a; table[0x3a] = 0x0f;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, table, layout) == ORACLES_ROOM_PREDICTED && layout[0x33] == 0x3a);
        /* Talus Peaks' two vines (0:61, $21): the second vine at $27. */
        s.active_room = 0x61;
        memset(layout, 0, sizeof layout);
        all[0x1f1] = 0x27;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, table, layout) == ORACLES_ROOM_PREDICTED);
        CHECK(layout[0x06] == 0x4d && layout[0x07] == 0xd5 && layout[0x08] == 0x55 && layout[0x16] == 0x5d && layout[0x17] == 0xd6 && layout[0x18] == 0x60 && layout[0x27] == 0x8d);
        CHECK(count(layout, 0) == ORACLES_ROOM_LAYOUT_BYTES - 7);
        /* $37 listed for another room: left to the game. */
        s.active_room = 0x13;
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICT_ROOM_CODE);
        /* The whole prediction of a room with its own code (0:25, $24, the bridge to Symmetry City once built): the
         * room's layout, its code, the collisions. */
        s.active_room = 0x25;
        fixture_group_lists(&f, f.tables.room_tile_changer_code_group_table.addr, (const uint8_t[]){ 0 }, 1, 0, (const uint8_t[]){ 0x25, 0x24, 0x00 }, 3);
        fixture_collision(&f, 0x1d, 0x0f);
        global_flags[0x25 >> 3] = 1u << (0x25 & 7u);   /* the bridge to Symmetry City built (0:25, $24) */
        CHECK(oracles_room_predict(&rom, &f.tables, &s, layout, collisions) == ORACLES_ROOM_PREDICTED);
        CHECK(layout[0x22] == 0x5a && layout[0x50] == 0x1d && layout[0x52] == 0x1d && layout[0x60] == 0x1e && collisions[0x51] == 0x0f && collisions[0x61] == 0);
        memset(global_flags, 0, sizeof global_flags);
        /* Seasons: the swamp flooded until room $81 of the group has bit 7 (0:60, $13); the bridge into Natzu but with
         * Dimitri (0:56, $28); Holly's door in winter (0:7f, $11); the linked doors of Subrosia by wGroup4RoomFlags+$f9
         * (1:35, $27). */
        fixture_group_lists(&f, f.tables.room_tile_changer_code_group_table.addr, (const uint8_t[]){ 0 }, 1, 0,
                            (const uint8_t[]){ 0x60, 0x13, 0x56, 0x28, 0x7f, 0x11, 0x00 }, 7);
        s = loading();
        s.game = ORACLES_GAME_SEASONS; s.active_room = 0x60;
        memset(page0, 0, sizeof page0);
        memset(layout, 0, sizeof layout);
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICT_ROOM_CODE);
        s.load.room_flags_page = page0;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED);
        CHECK(count(layout, 0xfd) == 18 + 12 && layout[0x24] == 0xfd && layout[0x76] == 0xfd && layout[0x47] == 0xfd && layout[0x79] == 0xfd && layout[0x23] == 0);
        page0[0x81] = 0x80;
        memset(layout, 0, sizeof layout);
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && count(layout, 0) == ORACLES_ROOM_LAYOUT_BYTES);
        s.active_room = 0x56;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x43] == 0xfd && layout[0x54] == 0xfd);
        memset(layout, 0, sizeof layout);
        s.load.animal_companion = 0x0c;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && count(layout, 0) == ORACLES_ROOM_LAYOUT_BYTES);
        s.load.animal_companion = 0x0b; s.load.room_flags = 0x40;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x66] == 0xb0 && layout[0x43] == 0);
        s.active_room = 0x7f; s.load.room_flags = 0;
        memset(layout, 0, sizeof layout);
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x47] == 0);
        s.load.room_state_modifier = 3;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x47] == 0xea);
        fixture_group_lists(&f, f.tables.room_tile_changer_code_group_table.addr, (const uint8_t[]){ 0 }, 1, 1, (const uint8_t[]){ 0x35, 0x27, 0x00 }, 3);
        s.active_group = 1; s.active_room = 0x35;
        memset(layout, 0, sizeof layout);
        s.load.room_flags_all = all; s.load.room_flags_all_size = sizeof all;
        all[0x2f9] = 0x04;
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x43] == 0xe8);
        /* Seasons' group 2 runs no room code; its group 3 does (3:a4, $29: Mr. Write's torch). */
        fixture_group_lists(&f, f.tables.room_tile_changer_code_group_table.addr, (const uint8_t[]){ 0 }, 1, 3, (const uint8_t[]){ 0xa4, 0x29, 0x00 }, 3);
        s.active_group = 3; s.active_room = 0xa4; s.load.room_flags = 0x40;
        memset(layout, 0, sizeof layout);
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x36] == 0x09);
    }
    /* Refused: an image whose code is not the original game's, a byte a routine reads that the image does not have (Dimitri,
     * the global flags). */
    {
        fixture_init(&f, rom_bytes);
        fixture_group_lists(&f, f.tables.room_tile_changer_code_group_table.addr, (const uint8_t[]){ 0 }, 1, 0, (const uint8_t[]){ 0x25, 0x24, 0x00 }, 3);
        s = loading();
        s.active_room = 0x25;
        memset(layout, 0, sizeof layout);
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICTED);
        s.load.room_code_replay = 0;
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICT_ROOM_CODE);
        CHECK(oracles_room_predict_substitute(&rom, &f.tables, &s, NULL, layout) == ORACLES_ROOM_PREDICT_ROOM_CODE);
        s.load.room_code_replay = 1; s.load.global_flags = NULL; s.load.global_flags_size = 0;
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICT_ROOM_CODE);
        fixture_group_lists(&f, f.tables.room_tile_changer_code_group_table.addr, (const uint8_t[]){ 0 }, 1, 0, (const uint8_t[]){ 0x56, 0x28, 0x00 }, 3);
        s = loading();
        s.game = ORACLES_GAME_SEASONS; s.active_room = 0x56;
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICTED);
        s.load.given &= (uint8_t)~ORACLES_LOAD_ANIMAL_COMPANION;
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICT_ROOM_CODE);
    }
    /* Seasons' Moblins' house (0:6f) beside a corner: its routine reads wSeedTreeRefilledBitset, which the scroll into it
     * may change before its load (updateSeedTreeRefillData), refused there; entered, predicted. */
    {
        fixture_init(&f, rom_bytes);
        fixture_group_lists(&f, f.tables.room_tile_changer_code_group_table.addr, (const uint8_t[]){ 0 }, 1, 0, (const uint8_t[]){ 0x6f, 0x20, 0x00 }, 3);
        uint8_t flags[256], room = 0;
        memset(flags, 0, sizeof flags);
        static const uint8_t no_objects[0x1000];
        s = loading();
        s.game = ORACLES_GAME_SEASONS; s.active_room = 0x6e; s.map_width = 16; s.map_height = 16;
        s.room_pack_bank = FIXTURE_BANK; s.room_pack_table = FIXTURE_ROOM_PACKS;
        s.load.room_flags_page = flags; s.load.objects = no_objects;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROOM_CODE && room == 0x6f);
        s.active_room = 0x6f;
        CHECK(oracles_room_predict_excluded(&rom, &f.tables, &s) == ORACLES_ROOM_PREDICTED);
    }
    /* Seasons' Subrosia (group 1): the room beside within its own map (11 x 8), no pack compared (checkRoomPack and
     * loadScreenMusic act in group 0 only); Ages' group 1 compares its packs; groups past 1 refused. */
    {
        fixture_init(&f, rom_bytes);
        fixture_room_tile(&f, 0x03, 3, 2, 0x3a);
        fixture_collision(&f, 0x3a, 0x0f);
        uint8_t flags[256], room = 0;
        memset(flags, 0, sizeof flags);
        static const uint8_t no_objects[0x1000];
        s = loading();
        s.game = ORACLES_GAME_SEASONS; s.active_group = 1; s.active_room = 0x13; s.map_width = 11; s.map_height = 8;
        s.room_pack_bank = FIXTURE_BANK; s.room_pack_table = FIXTURE_ROOM_PACKS;
        s.load.room_flags_page = flags; s.load.objects = no_objects;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED);
        CHECK(room == 0x03 && layout[0x23] == 0x3a && collisions[0x23] == 0x0f);
        s.game = ORACLES_GAME_AGES; s.map_width = 14; s.map_height = 14;   /* Ages' group 1, one pack */
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED && room == 0x03);
        fixture_put(&f, FIXTURE_ROOM_PACKS + 0x100u + 0x03u, 0x05);   /* another pack in group 1 */
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        s.game = ORACLES_GAME_SEASONS; s.map_width = 11; s.map_height = 8;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED && room == 0x03);
        s.active_group = 0; s.map_width = 16; s.map_height = 16;
        fixture_put(&f, FIXTURE_ROOM_PACKS + 0x03u, 0x05);
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        fixture_put(&f, FIXTURE_ROOM_PACKS + 0x03u, 0x00);
        /* The edges of Subrosia's map, not of Holodrum's. */
        s.active_group = 1; s.map_width = 11; s.map_height = 8;
        s.active_room = 0x7a;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 2, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 3, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED && room == 0x79);
        s.active_group = 2; s.active_room = 0x13;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
    }
    /* The Moblin's Keep (getMoblinKeepScreenIndex): the five rooms of the image's moblinKeepRooms, in group 0 alone (its
     * tileset chosen by getMoblinKeepSeasonsTilesetData), a list of another image's rooms followed as it is; the list
     * not bound, the image not qualified; unreadable, the room left to its load. */
    {
        fixture_init(&f, rom_bytes);
        uint8_t flags[256], room = 0;
        memset(flags, 0, sizeof flags);
        static const uint8_t no_objects[0x1000];
        s = loading();
        s.game = ORACLES_GAME_SEASONS; s.active_room = 0x5a; s.map_width = 16; s.map_height = 16;
        s.room_pack_bank = FIXTURE_BANK; s.room_pack_table = FIXTURE_ROOM_PACKS;
        s.load.room_flags_page = flags; s.load.objects = no_objects;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED && room == 0x5b);
        s.active_room = 0x6e;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 3, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED && room == 0x6d);
        static const uint8_t other[5] = { 0x6d, 0x5c, 0x7d, 0x6c, 0x7b };
        for (unsigned i = 0; i < 5u; i++) fixture_put(&f, FIXTURE_MOBLIN_KEEP + i, other[i]);
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 3, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED && room == 0x6d);
        s.active_room = 0x5a;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED && room == 0x5b);
        fixture_put(&f, FIXTURE_MOBLIN_KEEP, 0x03);   /* a room of Subrosia's map: listed for group 0 alone */
        s.active_room = 0x13;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED && room == 0x03);
        s.active_group = 1; s.map_width = 11; s.map_height = 8;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED && room == 0x03);
        s.active_group = 0; s.map_width = 16; s.map_height = 16;
        s.moblin_keep_table = 0x3ffe;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_UNREADABLE);
        s.moblin_keep_bank = 0xff;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_UNQUALIFIED);
        s.game = ORACLES_GAME_AGES; s.map_width = 14; s.map_height = 14;   /* Ages' engine has none */
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED);
    }
    /* A map walled inside the grid (Temple of Seasons): the room beside from its first column and row, none past them;
     * Subrosia's columns 4 to 10, Holodrum's columns 13 to 15 and rows 3 to 11. */
    {
        fixture_init(&f, rom_bytes);
        uint8_t flags[256], room = 0;
        memset(flags, 0, sizeof flags);
        static const uint8_t no_objects[0x1000];
        s = loading();
        s.game = ORACLES_GAME_SEASONS; s.active_group = 1; s.map_width = 11; s.map_height = 8; s.map_left = 4; s.map_top = 0;
        s.room_pack_bank = FIXTURE_BANK; s.room_pack_table = FIXTURE_ROOM_PACKS;
        s.load.room_flags_page = flags; s.load.objects = no_objects;
        s.active_room = 0x14;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 3, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED && room == 0x04);
        s.active_room = 0x15;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 3, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED && room == 0x14);
        s.active_group = 0; s.map_width = 16; s.map_height = 12; s.map_left = 13; s.map_top = 3;
        s.active_room = 0x3e;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        s.active_room = 0x4e;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 0, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED && room == 0x3e);
        s.active_room = 0x4d;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 3, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED && room == 0x4e);
        s.active_room = 0xbe;
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 2, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
    }
    /* The room beside at a corner, its own code replayed (0:25 right of 0:24); a routine listed for another room, refused. */
    {
        fixture_init(&f, rom_bytes);
        fixture_group_lists(&f, f.tables.room_tile_changer_code_group_table.addr, (const uint8_t[]){ 0 }, 1, 0, (const uint8_t[]){ 0x25, 0x37, 0x00 }, 3);
        uint8_t flags[256], all[0x400], room = 0;
        memset(flags, 0, sizeof flags); memset(all, 0, sizeof all);
        static const uint8_t no_objects[0x1000];
        s = loading();
        s.active_room = 0x24; s.map_width = 14; s.map_height = 14;
        s.room_pack_bank = FIXTURE_BANK; s.room_pack_table = FIXTURE_ROOM_PACKS;
        s.load.room_flags_page = flags; s.load.objects = no_objects;
        s.load.room_flags_all = all; s.load.room_flags_all_size = sizeof all;
        all[0x125] = 0x80;
        fixture_collision(&f, 0xee, 0x0f);
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROOM_CODE);   /* $37 is 0:a5's */
        fixture_group_lists(&f, f.tables.room_tile_changer_code_group_table.addr, (const uint8_t[]){ 0 }, 1, 0, (const uint8_t[]){ 0x25, 0x24, 0x00 }, 3);
        global_flags[0x25 >> 3] = 1u << (0x25 & 7u);
        CHECK(oracles_room_predict_beside(&rom, &f.tables, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED);
        CHECK(room == 0x25 && layout[0x50] == 0x1d && layout[0x60] == 0x1e);
        memset(global_flags, 0, sizeof global_flags);
    }
    /* The expanded tilesets: the collisions after the 2048 bytes of mappings of the tileset's entry (bank 2), a seasonal
     * one's by the season; the layout group overridden, or the table's for the group, swapped by bit 0 of the flags; the
     * large rooms raw; the rooms of group 0 whose chest stays closed until the past's room $20 has bit 0. */
    {
        fixture_init(&f, rom_bytes);
        OraclesTables *t = &f.tables;
        t->tileset_layout_table = (OraclesSym){ 0, 0 };
        t->expanded_tileset_mappings_table = (OraclesSym){ FIXTURE_BANK, 0x6000 };
        t->layout_group_table = (OraclesSym){ 0, 0x3000 };
        static const uint8_t entries[] = { 0x40, 0x00, 0x02, 0xff, 0x10, 0x60, 0x48, 0x00, 0x02 };
        for (unsigned i = 0; i < sizeof entries; i++) fixture_put(&f, 0x6000u + i, entries[i]);
        for (unsigned i = 0; i < 4u; i++) { fixture_put(&f, 0x6010u + 3u * i, i == 2u ? 0x40 : 0x50); fixture_put(&f, 0x6011u + 3u * i, 0x00); fixture_put(&f, 0x6012u + 3u * i, i == 2u ? 0x03 : 0x02); }
        rom_bytes[2u * 0x4000u + 0x800u + 0x3a] = 0x0f;          /* tileset 0 */
        rom_bytes[3u * 0x4000u + 0x800u + 0x3a] = 0x05;          /* tileset 1 in autumn */
        rom_bytes[2u * 0x4000u + 0x1000u + 0x800u + 0x3a] = 0x03;   /* tileset 1 in the other seasons */
        int seasonal = -1;
        CHECK(oracles_expanded_tileset_collisions(&rom, t, 0x80, 0, table, &seasonal) == ORACLES_OK && table[0x3a] == 0x0f && seasonal == 0);
        CHECK(oracles_expanded_tileset_collisions(&rom, t, 1, 2, table, &seasonal) == ORACLES_OK && table[0x3a] == 0x05 && seasonal == 1);
        CHECK(oracles_expanded_tileset_collisions(&rom, t, 1, 1, table, NULL) == ORACLES_OK && table[0x3a] == 0x03);
        /* The whole season byte, as the game indexes by it: a fifth entry past the four, read; one past the bank, refused. */
        fixture_put(&f, 0x601c, 0x40); fixture_put(&f, 0x601d, 0x00); fixture_put(&f, 0x601e, 0x03);
        CHECK(oracles_expanded_tileset_collisions(&rom, t, 1, 4, table, NULL) == ORACLES_OK && table[0x3a] == 0x05);
        const OraclesTables none = { 0 };
        CHECK(oracles_expanded_tileset_collisions(&rom, &none, 0, 0, table, NULL) == ORACLES_ERR_FORMAT);
        /* An entry whose data leaves its bank, a seasonal pointer out of the switchable bank: refused. */
        static const uint8_t bad[] = { 0x77, 0x80, 0x02, 0xff, 0x00, 0x30, 0xff, 0x00, 0x7f };
        for (unsigned i = 0; i < sizeof bad; i++) fixture_put(&f, 0x6006u + i, bad[i]);
        CHECK(oracles_expanded_tileset_collisions(&rom, t, 2, 0, table, NULL) == ORACLES_ERR_FORMAT);
        CHECK(oracles_expanded_tileset_collisions(&rom, t, 3, 0, table, NULL) == ORACLES_ERR_FORMAT);
        CHECK(oracles_expanded_tileset_collisions(&rom, t, 4, 0xff, table, NULL) == ORACLES_ERR_FORMAT);   /* its season's entry past $7fff */
        static const uint8_t groups[8] = { 5, 2, 1, 5, 4, 5, 4, 5 };
        memcpy(rom_bytes + 0x3000, groups, sizeof groups);
        fixture_put(&f, 0x4000u + 8u * 5u, 0x00);   /* layout group 5 large, raw: its rooms' words at $4400 from $4600 - $200 */
        fixture_room_tile(&f, 0x12, 3, 2, 0x3a);
        s = loading();
        s.load.tileset_layout = 0; s.load.tileset_layout_group = 0xff;
        CHECK(oracles_room_predict(&rom, t, &s, layout, collisions) == ORACLES_ROOM_PREDICT_LARGE);
        s.load.room_flags = 0x01;   /* the layout swapped: group 2, layout group 1 */
        CHECK(oracles_room_predict(&rom, t, &s, layout, collisions) == ORACLES_ROOM_PREDICTED && layout[0x23] == 0x3a && collisions[0x23] == 0x0f);
        s.load.room_flags = 0; s.load.tileset_layout_group = 0; s.load.tileset_layout = 1; s.load.room_state_modifier = 2;
        CHECK(oracles_room_predict(&rom, t, &s, layout, collisions) == ORACLES_ROOM_PREDICTED && collisions[0x23] == 0x05);
        s.load.loaded_tileset_layout = 1;   /* a seasonal tileset kept loaded: maybe another season's */
        CHECK(oracles_room_predict(&rom, t, &s, layout, collisions) == ORACLES_ROOM_PREDICT_UNREADABLE);
        s.load.tileset_layout = 0; s.load.loaded_tileset_layout = 0;   /* not seasonal: the same collisions kept */
        CHECK(oracles_room_predict(&rom, t, &s, layout, collisions) == ORACLES_ROOM_PREDICTED && collisions[0x23] == 0x0f);
        s.load.tileset_layout = 1; s.load.loaded_tileset_layout = 0;
        /* Seasons' engine: group 0's layout group the season (wRoomStateModifier), another group's the table's, no swap;
         * a season naming no layout group, refused. */
        s.game = ORACLES_GAME_SEASONS; s.load.tileset_layout_group = 0xff;
        CHECK(oracles_room_predict(&rom, t, &s, layout, collisions) == ORACLES_ROOM_PREDICTED && collisions[0x23] == 0x05);
        s.active_group = 1; s.load.room_flags = 0x01;   /* layout group 2; Ages' swap would give 3's, large */
        CHECK(oracles_room_predict(&rom, t, &s, layout, collisions) == ORACLES_ROOM_PREDICTED);
        s.game = ORACLES_GAME_AGES;
        CHECK(oracles_room_predict(&rom, t, &s, layout, collisions) == ORACLES_ROOM_PREDICT_LARGE);
        s.game = ORACLES_GAME_SEASONS; s.active_group = 0; s.load.room_flags = 0; s.load.room_state_modifier = 9;
        CHECK(oracles_room_predict(&rom, t, &s, layout, collisions) == ORACLES_ROOM_PREDICT_UNREADABLE);
        s.load.tileset_layout_group = 8;   /* an override past the groups */
        CHECK(oracles_room_predict(&rom, t, &s, layout, collisions) == ORACLES_ROOM_PREDICT_UNREADABLE);
        /* A seasonal tileset whose four seasons' collisions are the same, kept loaded: predicted. */
        s.load.tileset_layout_group = 0xff; s.load.room_state_modifier = 1; s.load.tileset_layout = 0x20; s.load.loaded_tileset_layout = 0x20;
        fixture_put(&f, 0x6060, 0xff); fixture_put(&f, 0x6061, 0x70); fixture_put(&f, 0x6062, 0x60);
        for (unsigned i = 0; i < 4u; i++) { fixture_put(&f, 0x6070u + 3u * i, 0x50); fixture_put(&f, 0x6071u + 3u * i, 0x00); fixture_put(&f, 0x6072u + 3u * i, 0x02); }
        CHECK(oracles_expanded_tileset_collisions(&rom, t, 0x20, 3, table, &seasonal) == ORACLES_OK && seasonal == 0);
        CHECK(oracles_room_predict(&rom, t, &s, layout, collisions) == ORACLES_ROOM_PREDICTED && collisions[0x23] == 0x03);
        s.game = ORACLES_GAME_AGES; s.load.tileset_layout_group = 0;
        int large = 0;
        fixture_put(&f, 0x4600u + 80u * 0x12u - 0x200u + 0x9eu, 0x3b);
        CHECK(oracles_decode_room_layout(&rom, t, 5, 0x12, layout, &large) == ORACLES_OK && large && layout[0x9e] == 0x3b);
        s = loading();
        uint8_t all[0x400];
        memset(all, 0, sizeof all);
        s.load.room_flags_all = all; s.load.room_flags_all_size = sizeof all;
        const uint8_t chests[] = { 0x33, 0x12, 0x28, 0x00, 0xff };
        fixture_group_lists(&f, t->chest_data_group_table.addr, (const uint8_t[]){ 0xff }, 1, 0, chests, sizeof chests);
        static const uint8_t exceptions[] = { 0x40, 0x12, 0x00 };
        for (unsigned i = 0; i < sizeof exceptions; i++) fixture_put(&f, 0x6100u + i, exceptions[i]);
        t->opened_chest_exceptions = (OraclesSym){ FIXTURE_BANK, 0x6100 };
        s.load.room_flags = 0x20;
        memset(layout, 0, sizeof layout);
        CHECK(oracles_room_predict_substitute(&rom, t, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && count(layout, 0xf0) == 0);
        all[0x120] = 0x01;
        CHECK(oracles_room_predict_substitute(&rom, t, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x33] == 0xf0);
        s.load.room_flags_all = NULL; s.load.room_flags_all_size = 0;
        CHECK(oracles_room_predict_substitute(&rom, t, &s, NULL, layout) == ORACLES_ROOM_PREDICT_UNREADABLE);
        s = loading(); s.active_group = 2; s.load.room_flags = 0x20;
        fixture_group_lists(&f, t->chest_data_group_table.addr, (const uint8_t[]){ 0xff }, 1, 2, chests, sizeof chests);
        memset(layout, 0, sizeof layout);
        CHECK(oracles_room_predict_substitute(&rom, t, &s, NULL, layout) == ORACLES_ROOM_PREDICTED && layout[0x33] == 0xf0);
        /* The room beside with the expanded tilesets: its tileset's own byte, the layout group by the group (Seasons' group 0
         * by the season); in Ages' engine a pack from $7f refused. */
        uint8_t room = 0, flags[256];
        static const uint8_t no_objects[0x1000];
        memset(flags, 0, sizeof flags);
        s = loading(); s.active_room = 0x11; s.map_width = 14; s.map_height = 14;
        s.room_pack_bank = FIXTURE_BANK; s.room_pack_table = FIXTURE_ROOM_PACKS;
        s.load.room_flags_page = flags; s.load.objects = no_objects; s.load.tileset_layout = 0x80; s.load.tileset_layout_group = 0xff;
        rom_bytes[0x3000] = 2;   /* group 0's layout group: 2, small */
        s.game = ORACLES_GAME_SEASONS; s.load.room_state_modifier = 2; fixture_room_tile(&f, 0x12, 3, 2, 0x3a);
        CHECK(oracles_room_predict_beside(&rom, t, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED
              && room == 0x12 && layout[0x23] == 0x3a && collisions[0x23] == 0x0f);
        s.load.room_state_modifier = 9;   /* a season naming no layout group */
        CHECK(oracles_room_predict_beside(&rom, t, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_UNREADABLE);
        s.game = ORACLES_GAME_AGES; s.load.room_state_modifier = 0;
        CHECK(oracles_room_predict_beside(&rom, t, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_UNQUALIFIED);   /* no profile's override */
        s.load.tileset_override_rules = ORACLES_TILESET_OVERRIDE_SEASONS;
        CHECK(oracles_room_predict_beside(&rom, t, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED
              && room == 0x12 && layout[0x23] == 0x3a && collisions[0x23] == 0x0f);
        fixture_put(&f, 0x5d10u + 0x12u, 1);   /* the room beside's tileset 1, seasonal: left to its load */
        fixture_put(&f, 0x5e18, 0x0f); fixture_put(&f, 0x5e19, 0x01);
        CHECK(oracles_room_predict_beside(&rom, t, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        fixture_put(&f, 0x5d10u + 0x12u, 0);
        fixture_put(&f, FIXTURE_ROOM_PACKS + 0x11, 0x80); fixture_put(&f, FIXTURE_ROOM_PACKS + 0x12, 0x80);
        CHECK(oracles_room_predict_beside(&rom, t, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
        s.load.tileset_override_rules = ORACLES_TILESET_OVERRIDE_BASE;   /* the base engine's: pack $80 keeps its tileset */
        CHECK(oracles_room_predict_beside(&rom, t, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICTED);
        fixture_put(&f, FIXTURE_ROOM_PACKS + 0x11, 0x7f); fixture_put(&f, FIXTURE_ROOM_PACKS + 0x12, 0x7f);   /* Nuun Highlands */
        CHECK(oracles_room_predict_beside(&rom, t, &s, 1, special_room, NULL, &room, layout, collisions, NULL) == ORACLES_ROOM_PREDICT_ROUTED);
    }
    CHECK(!strcmp(oracles_room_predict_verdict_name(ORACLES_ROOM_PREDICTED), "predicted"));
    CHECK(!strcmp(oracles_room_predict_verdict_name(ORACLES_ROOM_PREDICT_VERDICTS), "?"));

    free(rom_bytes);
    if (failures) return 1;
    puts("test_room_predict: ok");
    return 0;
}
