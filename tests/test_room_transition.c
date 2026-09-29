/* The continuous transition policy's pure parts, without a ROM: eligibility, the
 * walking animation decoder on synthetic animation data, the collision cap, and
 * for --continuous-swim the swimming states, the speed and the water ahead. */
#include "room_transition.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static OraclesGuestTransitionState eligible(void)
{
    OraclesGuestTransitionState s;
    memset(&s, 0, sizeof s);
    s.game = ORACLES_GAME_AGES;
    s.game_state = 2; s.cutscene_index = 1;
    s.scroll_mode = 8; s.transition_state = 5; s.transition_substate = 2; s.transition_phase = 2; s.transition_direction = 1;
    s.screen_scroll_delta = 4; s.screen_scroll_counter = 0x14;
    s.link_object_index = 0xd0; s.link_state = 1; s.link_visible = 0x80; s.link_anim_mode = 0x10;
    s.link_pushing_direction = 0xff; s.force_link_push_animation = 0x80;   /* Ages holds the push logic off this way in a scroll */
    s.link_x = 156 << 8; s.link_y = 80 << 8;
    s.animation_data_bank = 6;
    return s;
}

int main(void)
{
    OraclesGuestTransitionState s = eligible();
    CHECK(oracles_room_transition_eligible(&s, 0));
    OraclesGuestTransitionState x;
    x = s; x.game = ORACLES_GAME_SEASONS; CHECK(oracles_room_transition_eligible(&x, 0));   /* the same transition machine in both games */
    x = s; x.game = (OraclesGame)99; CHECK(!oracles_room_transition_eligible(&x, 0));
    x = s; x.active_group = 4; CHECK(oracles_room_transition_eligible(&x, 0));   /* a dungeon group: the same machine */
    x = s; x.active_group = 2; CHECK(oracles_room_transition_eligible(&x, 0));   /* an interior: on the grid too */
    x = s; x.room_is_large = 1; CHECK(!oracles_room_transition_eligible(&x, 0));   /* a large room: the game's own transition, deliberately */
    x = s; x.active_group = 5; x.room_is_large = 1; CHECK(!oracles_room_transition_eligible(&x, 0));   /* a dungeon */
    x = s; x.active_group = 6; CHECK(!oracles_room_transition_eligible(&x, 0));
    x = s; x.tileset_flags = 0x20; CHECK(oracles_room_transition_refusal(&x, 0) == 0);   /* a side view, whatever its group */
    x = s; x.transition_state = 2; CHECK(!oracles_room_transition_eligible(&x, 0));
    x = s; x.transition_state = 3; x.scroll_mode = 4; CHECK(oracles_room_transition_eligible(&x, 0));   /* the preparation, before the load */
    x = s; x.link_anim_mode = 0; CHECK(!oracles_room_transition_eligible(&x, 0));
    x = s; x.link_in_air = 1; CHECK(oracles_room_transition_refusal(&x, 0) == 4);
    x = s; x.link_swimming_state = 3; CHECK(oracles_room_transition_refusal(&x, 0) == 8);
    x = s; x.tileset_flags = 0x41; CHECK(oracles_room_transition_refusal(&x, 0) == 9);   /* Ages' sea floor */
    /* Swimming, with --continuous-swim: the normal swimming state (3), diving or not, its own animations. */
    x = s; x.link_swimming_state = 3; x.link_anim_mode = 0x0b;
    CHECK(oracles_room_transition_refusal(&x, 0) == 8 && oracles_room_transition_eligible(&x, 1));
    x.link_swimming_state = 0x83; x.link_anim_mode = 0x0c; CHECK(oracles_room_transition_eligible(&x, 1));   /* diving */
    x.link_swimming_state = 0x01; CHECK(oracles_room_transition_refusal(&x, 1) == 8);                       /* entering the water */
    x.link_swimming_state = 0x02; CHECK(oracles_room_transition_refusal(&x, 1) == 8);                       /* speed locked after the splash */
    x.link_swimming_state = 0x04; CHECK(oracles_room_transition_refusal(&x, 1) == 8);                       /* drowning */
    x.link_swimming_state = 0x43; CHECK(oracles_room_transition_refusal(&x, 1) == 8);                       /* lava */
    x.link_swimming_state = 0x03; x.link_anim_mode = 0x10; CHECK(oracles_room_transition_refusal(&x, 1) == 3);   /* not a swimming animation */
    x = s; CHECK(oracles_room_transition_eligible(&x, 1));                                                  /* on foot, as without it */
    /* Ages' sea floor, with --continuous-swim: the game has him walk there, in his mermaid suit's gait, at his own speed. */
    x = s; x.tileset_flags = 0x41; x.link_speed = 0x2d;
    CHECK(oracles_room_transition_refusal(&x, 0) == 9 && oracles_room_transition_eligible(&x, 1));
    CHECK(oracles_room_transition_in_water(&x) && !oracles_room_transition_swimming(&x) && oracles_room_transition_step(&x) == 0x120);
    x.link_anim_mode = 0x0b; CHECK(oracles_room_transition_refusal(&x, 1) == 3);                            /* not his gait there */
    /* A stroke of the flippers carries him to the edge without the direction held (--continuous-swim): the
     * transition the game would start with the direction held, its checks kept. */
    {
        OraclesGuestTransitionState e = eligible();
        e.game = ORACLES_GAME_SEASONS; e.transition_state = 2; e.scroll_mode = 1;
        e.link_swimming_state = 3; e.link_anim_mode = 0x0b; e.link_enabled = 1; e.link_speed = 0x14;
        e.link_move_angle = 0x00; e.link_input_angle = 0xff; e.link_stroke = 1;
        e.link_x = 80 << 8; e.link_y = 6 << 8; e.boundary_x = 154; e.boundary_y = 121;
        e.tileset_flags = 0x01; e.active_room = 0x96; e.map_width = 16; e.map_height = 16;
        OraclesGuestTransitionState f;
        CHECK(oracles_room_transition_swim_edge(&e) == 0);                                   /* up, a pixel short of the game's test */
        f = e; f.link_y = 7 << 8; CHECK(oracles_room_transition_swim_edge(&f) < 0);          /* not at the edge */
        f = e; f.link_input_angle = 0x00; CHECK(oracles_room_transition_swim_edge(&f) < 0);  /* up held: the game's own */
        f = e; f.link_input_angle = 0x08; CHECK(oracles_room_transition_swim_edge(&f) == 0); /* right held, across the top edge: the momentum carries him */
        f = e; f.link_input_angle = 0x10; CHECK(oracles_room_transition_swim_edge(&f) < 0);  /* down held, away from it: the player's wish */
        f = e; f.link_input_angle = 0x0c; CHECK(oracles_room_transition_swim_edge(&f) < 0);  /* down-right, away from it too */
        f = e; f.link_input_angle = 0x1c; CHECK(oracles_room_transition_swim_edge(&f) < 0);  /* up-left, toward it: the game's own */
        f = e; f.link_speed = 0; CHECK(oracles_room_transition_swim_edge(&f) < 0);           /* not moving */
        f = e; f.link_stroke = 2; CHECK(oracles_room_transition_swim_edge(&f) == 0);         /* the stroke slowing down */
        f = e; f.link_stroke = 3; CHECK(oracles_room_transition_swim_edge(&f) < 0);
        f = e; f.scroll_mode = 4; CHECK(oracles_room_transition_swim_edge(&f) < 0);          /* a transition decided */
        f = e; f.link_state = 0x0a; CHECK(oracles_room_transition_swim_edge(&f) < 0);        /* not his normal state */
        f = e; f.link_x = 6 << 8; f.link_move_angle = 0x1c; CHECK(oracles_room_transition_swim_edge(&f) == 3);   /* a corner: the horizontal, as the game */
        /* His motion toward an edge: within 5 steps of its angle, on both sides alike. */
        f = e; f.link_y = 60 << 8; f.link_x = 154 << 8;
        f.link_move_angle = 0x03; CHECK(oracles_room_transition_swim_edge(&f) == 1);
        f.link_move_angle = 0x0d; CHECK(oracles_room_transition_swim_edge(&f) == 1);
        f.link_move_angle = 0x02; CHECK(oracles_room_transition_swim_edge(&f) < 0);
        f.link_move_angle = 0x0f; CHECK(oracles_room_transition_swim_edge(&f) < 0);
        f = e; f.link_y = 60 << 8; f.link_x = 6 << 8;
        f.link_move_angle = 0x13; CHECK(oracles_room_transition_swim_edge(&f) == 3);
        f.link_move_angle = 0x1d; CHECK(oracles_room_transition_swim_edge(&f) == 3);
        f.link_move_angle = 0x11; CHECK(oracles_room_transition_swim_edge(&f) < 0);
        f.link_move_angle = 0x1f; CHECK(oracles_room_transition_swim_edge(&f) < 0);
        f = e; f.link_stroke = 0; CHECK(oracles_room_transition_swim_edge(&f) < 0);          /* a glide, no stroke */
        f = e; f.link_move_angle = 0x10; CHECK(oracles_room_transition_swim_edge(&f) < 0);   /* swimming down at the top */
        f = e; f.screen_transition_delay = 2; CHECK(oracles_room_transition_swim_edge(&f) < 0);
        f = e; f.disable_screen_transitions = 1; CHECK(oracles_room_transition_swim_edge(&f) < 0);
        f = e; f.hole_or_conveyor = 0x80; CHECK(oracles_room_transition_swim_edge(&f) < 0);
        f = e; f.link_enabled = 0; CHECK(oracles_room_transition_swim_edge(&f) < 0);
        f = e; f.transition_state = 3; CHECK(oracles_room_transition_swim_edge(&f) < 0);
        f = e; f.link_swimming_state = 0; f.link_anim_mode = 0x10; CHECK(oracles_room_transition_swim_edge(&f) < 0);   /* on foot */
        f = e; f.link_y = 121 << 8; f.link_move_angle = 0x10; CHECK(oracles_room_transition_swim_edge(&f) == 2);   /* down, at the boundary */
        f = e; f.link_y = 60 << 8; f.link_x = 154 << 8; f.link_move_angle = 0x08; CHECK(oracles_room_transition_swim_edge(&f) == 1);
        f = e; f.link_y = 60 << 8; f.link_x = 6 << 8; f.link_move_angle = 0x18; CHECK(oracles_room_transition_swim_edge(&f) == 3);
        /* Ages: the mermaid suit is left to the game, the overworld does not loop, deep water needs the suit. */
        f = e; f.game = ORACLES_GAME_AGES; f.map_width = 14; f.map_height = 14; CHECK(oracles_room_transition_swim_edge(&f) == 0);
        f.link_var2f = 0x40; CHECK(oracles_room_transition_swim_edge(&f) < 0);
        f = e; f.game = ORACLES_GAME_AGES; f.map_width = 14; f.map_height = 14; f.active_room = 0x6d; f.link_y = 60 << 8; f.link_x = 154 << 8; f.link_move_angle = 0x08;
        CHECK(oracles_room_transition_swim_edge(&f) < 0);                                    /* the map's rightmost column */
        f.tileset_flags = 0; CHECK(oracles_room_transition_swim_edge(&f) == 1);              /* not outdoors: no such check */
        f = e; f.game = ORACLES_GAME_AGES; f.map_width = 14; f.map_height = 14; f.active_tile_index = 0xfc; CHECK(oracles_room_transition_swim_edge(&f) < 0);
        f = e; f.game = ORACLES_GAME_AGES; f.map_width = 14; f.map_height = 14; f.active_room = 0xd5; f.link_y = 121 << 8; f.link_move_angle = 0x10;
        CHECK(oracles_room_transition_swim_edge(&f) < 0);                                    /* the map's bottom row */
        f.active_room = 0xc5; CHECK(oracles_room_transition_swim_edge(&f) == 2);
        f = e; f.game = ORACLES_GAME_AGES; f.map_width = 14; f.map_height = 14; f.active_room = 0x60; f.link_y = 60 << 8; f.link_x = 6 << 8; f.link_move_angle = 0x18;
        CHECK(oracles_room_transition_swim_edge(&f) < 0);                                    /* the map's leftmost column */
        f.active_room = 0x61; CHECK(oracles_room_transition_swim_edge(&f) == 3);
    }
    /* His own move a frame: a pixel on foot; swimming, objectSpeedTable's axis value, $20 a row of five. */
    x = s; CHECK(oracles_room_transition_step(&x) == 0x100);
    x.link_swimming_state = 3; x.link_speed = 0x14; CHECK(oracles_room_transition_step(&x) == 0x80);      /* SPEED_80, the flippers */
    x.link_speed = 0x2d; CHECK(oracles_room_transition_step(&x) == 0x120);                                  /* SPEED_120, the mermaid suit */
    x.link_speed = 0x37; CHECK(oracles_room_transition_step(&x) == 0x160);                                  /* SPEED_160, with the swimmer's ring */
    x.link_speed = 0; CHECK(oracles_room_transition_step(&x) == 0);
    x = s; x.active_item = 1; CHECK(!oracles_room_transition_eligible(&x, 0));
    x = s; x.text_is_active = 1; CHECK(!oracles_room_transition_eligible(&x, 0));
    x = s; x.link_state = 0x0a; CHECK(!oracles_room_transition_eligible(&x, 0));

    /* The animation stream: duration, base frame, parameter; $ff then a negative offset loops. */
    const size_t rom_size = 8u * 0x4000u;
    uint8_t *rom = calloc(rom_size, 1);
    CHECK(rom != NULL);
    if (!rom) return 1;
    const size_t base = 6u * 0x4000u;                       /* bank 6, $4000 */
    rom[base + 0] = 2; rom[base + 1] = 0x54; rom[base + 2] = 0;
    OraclesGuestTransitionMutation m;
    memset(&m, 0, sizeof m);
    s.link_anim_pointer = 0x4000; s.link_anim_counter = 3;
    CHECK(oracles_room_transition_next_walk_frame(rom, rom_size, &s, &m));
    CHECK(m.update_walk_animation && m.link_anim_counter == 2 && m.link_anim_pointer == 0x4000 && m.link_animation_frame == 0);
    s.link_anim_counter = 1;
    CHECK(oracles_room_transition_next_walk_frame(rom, rom_size, &s, &m));
    CHECK(m.link_anim_counter == 2 && m.link_animation_frame == 0x54 && m.link_anim_parameter == 0 && m.link_anim_pointer == 0x4003);
    rom[base + 0x100] = 0xff; rom[base + 0x101] = 0xfc;      /* at $4100: loop back to $40fd */
    rom[base + 0xfd] = 5; rom[base + 0xfe] = 0x61; rom[base + 0xff] = 2;
    s.link_anim_pointer = 0x4100; s.link_anim_counter = 1;
    CHECK(oracles_room_transition_next_walk_frame(rom, rom_size, &s, &m));
    CHECK(m.link_anim_counter == 5 && m.link_animation_frame == 0x61 && m.link_anim_parameter == 2 && m.link_anim_pointer == 0x4100);
    s.link_anim_pointer = 0x8000;
    CHECK(!oracles_room_transition_next_walk_frame(rom, rom_size, &s, &m));
    free(rom);

    /* The collision cap: Link at x=156 of the room he leaves, scrolling right;
     * a pixel further, his leading edge (x+4) is at 161 - 160 = 1 in the room
     * entered, tile column 0, row 5. */
    uint8_t collisions[176];
    memset(collisions, 0, sizeof collisions);
    s = eligible();
    s.room_collisions = NULL;
    CHECK(oracles_room_transition_walkable(&s, 0x100));         /* not loaded yet: nothing to check */
    s.room_collisions = collisions;
    CHECK(oracles_room_transition_walkable(&s, 0x100));
    collisions[5 * 16 + 0] = 0x0f;                              /* a wall at the entry tile */
    CHECK(!oracles_room_transition_walkable(&s, 0x100));
    collisions[5 * 16 + 0] = 0;
    s.link_x = 170 << 8;                                        /* already 10 px into the room entered: tile column 0 */
    CHECK(oracles_room_transition_walkable(&s, 0x100));
    collisions[5 * 16 + 1] = 0x0f;                              /* the next tile column is a wall: his edge at 15 is still in column 0 */
    CHECK(oracles_room_transition_walkable(&s, 0x100));
    s.link_x = 172 << 8;                                        /* his edge would reach column 1 */
    CHECK(!oracles_room_transition_walkable(&s, 0x100));
    /* Leftward: the room entered is to the left, Link's x runs below zero. */
    s = eligible(); s.transition_direction = 3; s.link_x = (uint16_t)(2 << 8); s.room_collisions = collisions;
    memset(collisions, 0, sizeof collisions);
    CHECK(oracles_room_transition_walkable(&s, 0x100));          /* 1 - 4 = -3 -> 157 of the room entered, column 9 */
    collisions[5 * 16 + 9] = 0x0f;
    CHECK(!oracles_room_transition_walkable(&s, 0x100));
    /* Downward. */
    s = eligible(); s.transition_direction = 2; s.link_y = (uint16_t)(124 << 8); s.link_x = 80 << 8; s.room_collisions = collisions;
    memset(collisions, 0, sizeof collisions);
    CHECK(oracles_room_transition_walkable(&s, 0x100));          /* 125 + 4 = 129 -> 1 in the room entered: row 0, column 5 */
    collisions[0 * 16 + 5] = 0x0f;
    CHECK(!oracles_room_transition_walkable(&s, 0x100));
    /* With --continuous-swim, stairs and a bridge along the way are walked on; a bridge across it is not. */
    collisions[0 * 16 + 5] = 0x18; CHECK(!oracles_room_transition_walkable(&s, 0x100) && oracles_room_transition_passable(&s, 0x100));
    collisions[0 * 16 + 5] = 0x12; CHECK(oracles_room_transition_passable(&s, 0x100));    /* a vertical bridge, downward */
    collisions[0 * 16 + 5] = 0x1a; CHECK(!oracles_room_transition_passable(&s, 0x100));   /* a horizontal one */
    collisions[0 * 16 + 5] = 0x0f; CHECK(!oracles_room_transition_passable(&s, 0x100));
    s.transition_direction = 1; s.link_x = 156 << 8; s.link_y = 80 << 8; memset(collisions, 0, sizeof collisions);
    collisions[5 * 16 + 0] = 0x1b; CHECK(!oracles_room_transition_walkable(&s, 0x100) && oracles_room_transition_passable(&s, 0x100));   /* rightward, a bridge's end */
    collisions[5 * 16 + 0] = 0x11; CHECK(!oracles_room_transition_passable(&s, 0x100));

    /* Swimming beyond the game's own distance: water only, told by the tile
     * types of the room's collision list (tileTypesTable, bank 5 here). */
    uint8_t *types_rom = calloc(rom_size, 1);
    CHECK(types_rom != NULL);
    if (!types_rom) return 1;
    const size_t types = 5u * 0x4000u;
    types_rom[types + 0] = 0x10; types_rom[types + 1] = 0x40;          /* list 0 at $4010 */
    const uint8_t list[] = { 0xfd, 0x07, 0xf3, 0x01, 0xfc, 0x17, 0x40, 0x13, 0x00 };   /* water, hole, sea, a current */
    memcpy(types_rom + types + 0x10, list, sizeof list);
    uint8_t layout[176];
    memset(collisions, 0, sizeof collisions);
    memset(layout, 0, sizeof layout);
    s = eligible();
    s.link_swimming_state = 3; s.link_anim_mode = 0x0b; s.link_speed = 0x2d;
    s.tile_types_bank = 5; s.tile_types_table = 0x4000; s.active_collisions = 0;
    s.room_collisions = collisions; s.room_layout = layout;
    CHECK(oracles_room_transition_tile_type(types_rom, rom_size, &s, 0xfd) == 0x07);
    CHECK(oracles_room_transition_tile_type(types_rom, rom_size, &s, 0x12) == 0x00);   /* not listed: normal */
    s.tile_types_bank = 0xff; CHECK(oracles_room_transition_tile_type(types_rom, rom_size, &s, 0xfd) < 0);
    s.tile_types_bank = 5;
    collisions[5 * 16 + 0] = 0x10;                              /* the entry tile: holes', water's and lava's collision */
    layout[5 * 16 + 0] = 0xfd; CHECK(oracles_room_transition_swimmable(types_rom, rom_size, &s, 0x120));
    collisions[5 * 16 + 0] = 0x0f; CHECK(!oracles_room_transition_swimmable(types_rom, rom_size, &s, 0x120));   /* a water tile index with a wall's collision */
    collisions[5 * 16 + 0] = 0x10;
    layout[5 * 16 + 0] = 0x40; CHECK(oracles_room_transition_swimmable(types_rom, rom_size, &s, 0x120));   /* a current */
    layout[5 * 16 + 0] = 0xf3; CHECK(!oracles_room_transition_swimmable(types_rom, rom_size, &s, 0x120));  /* a hole */
    layout[5 * 16 + 0] = 0xfc; CHECK(!oracles_room_transition_swimmable(types_rom, rom_size, &s, 0x120));  /* the sea, no mermaid suit */
    s.link_var2f = 0x40; CHECK(oracles_room_transition_swimmable(types_rom, rom_size, &s, 0x120));
    collisions[5 * 16 + 0] = 0; layout[5 * 16 + 0] = 0x01;       /* land: walkable, not swimmable */
    CHECK(oracles_room_transition_walkable(&s, 0x120) && !oracles_room_transition_swimmable(types_rom, rom_size, &s, 0x120));
    s.room_collisions = NULL; CHECK(oracles_room_transition_swimmable(types_rom, rom_size, &s, 0x120));    /* not loaded yet */
    free(types_rom);

    if (failures) return 1;
    puts("test_room_transition: ok");
    return 0;
}
