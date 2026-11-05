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
    /* A scene walking him across with the game's own keys (wUseSimulatedInput, $ff once read to their end): the game's
     * transition; 2 is the player's keys reversed (Ganon), play. */
    x = s; x.use_simulated_input = 1; CHECK(oracles_room_transition_refusal(&x, 0) == 2 && oracles_room_transition_simulated(&x));
    x = s; x.use_simulated_input = 0xff; CHECK(oracles_room_transition_refusal(&x, 1) == 2);
    x = s; x.use_simulated_input = 2; CHECK(oracles_room_transition_eligible(&x, 0) && !oracles_room_transition_simulated(&x));
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
    CHECK(oracles_room_transition_walkable(&s, 0, 0x100));         /* not loaded yet: nothing to check */
    s.room_collisions = collisions;
    CHECK(oracles_room_transition_walkable(&s, 0, 0x100));
    collisions[5 * 16 + 0] = 0x0f;                              /* a wall at the entry tile */
    CHECK(!oracles_room_transition_walkable(&s, 0, 0x100));
    collisions[5 * 16 + 0] = 0;
    s.link_x = 170 << 8;                                        /* already 10 px into the room entered: tile column 0 */
    CHECK(oracles_room_transition_walkable(&s, 0, 0x100));
    collisions[5 * 16 + 1] = 0x0f;                              /* the next tile column is a wall: his edge at 15 is still in column 0 */
    CHECK(oracles_room_transition_walkable(&s, 0, 0x100));
    s.link_x = 172 << 8;                                        /* his edge would reach column 1 */
    CHECK(!oracles_room_transition_walkable(&s, 0, 0x100));
    /* Leftward: the room entered is to the left, Link's x runs below zero. */
    s = eligible(); s.transition_direction = 3; s.link_x = (uint16_t)(2 << 8); s.room_collisions = collisions;
    memset(collisions, 0, sizeof collisions);
    CHECK(oracles_room_transition_walkable(&s, 0, 0x100));          /* 1 - 4 = -3 -> 157 of the room entered, column 9 */
    collisions[5 * 16 + 9] = 0x0f;
    CHECK(!oracles_room_transition_walkable(&s, 0, 0x100));
    /* Downward. */
    s = eligible(); s.transition_direction = 2; s.link_y = (uint16_t)(124 << 8); s.link_x = 80 << 8; s.room_collisions = collisions;
    memset(collisions, 0, sizeof collisions);
    CHECK(oracles_room_transition_walkable(&s, 0, 0x100));          /* 125 + 7 = 132 -> 4 in the room entered: row 0, column 5 */
    collisions[0 * 16 + 5] = 0x0f;
    CHECK(!oracles_room_transition_walkable(&s, 0, 0x100));
    /* The game's own points: his tile at y+5 and his feet at y+7 (x-3 and x+2), not y+4.  From y=120, a pixel down,
     * his feet reach row 0 of the room entered (128) while y+4 is still in the room left; from 122, his tile does. */
    s.link_y = (uint16_t)(120 << 8); CHECK(!oracles_room_transition_walkable(&s, 0, 0x100));
    collisions[0 * 16 + 5] = 0x10; s.link_y = (uint16_t)(122 << 8); CHECK(!oracles_room_transition_walkable(&s, 0, 0x100));   /* a hole under y+5 = 128 */
    s.link_y = (uint16_t)(119 << 8); CHECK(oracles_room_transition_walkable(&s, 0, 0x100));                                       /* y+7 = 127: not yet */
    /* And no point of his in a wall at his sides it is not in where he stands `from`: at x=84 and y=125, x-5 = 79 at
     * y+5 = 130 falls in column 4's top right quarter; from y=120 it was in the room left. */
    collisions[0 * 16 + 5] = 0; s.link_x = 84 << 8; s.link_y = (uint16_t)(120 << 8); CHECK(oracles_room_transition_walkable(&s, 0, 0x500));
    collisions[0 * 16 + 4] = 0x04; CHECK(!oracles_room_transition_walkable(&s, 0, 0x500));
    CHECK(oracles_room_transition_walkable(&s, 0x400, 0x500));   /* from y=124: there already (y+5 = 129) */
    collisions[0 * 16 + 4] = 0x08; CHECK(oracles_room_transition_walkable(&s, 0, 0x500));    /* its top left quarter */
    collisions[0 * 16 + 4] = 0; s.link_x = 80 << 8; s.link_y = (uint16_t)(124 << 8);
    /* With --continuous-swim, stairs and a bridge along the way are walked on; a bridge across it is not. */
    collisions[0 * 16 + 5] = 0x18; CHECK(!oracles_room_transition_walkable(&s, 0, 0x100) && oracles_room_transition_passable(&s, 0, 0x100));
    collisions[0 * 16 + 5] = 0x12; CHECK(oracles_room_transition_passable(&s, 0, 0x100));    /* a vertical bridge, downward */
    /* His feet reaching its rail (x 80 to 83: x+2 = 82) from the room left, not along it. */
    s.link_y = (uint16_t)(118 << 8); CHECK(!oracles_room_transition_passable(&s, 0, 0x600));
    s.link_x = 89 << 8; CHECK(oracles_room_transition_passable(&s, 0, 0x600));               /* x-5 = 84: past it */
    s.link_x = 80 << 8; s.link_y = (uint16_t)(124 << 8);
    collisions[0 * 16 + 5] = 0x1a; CHECK(!oracles_room_transition_passable(&s, 0, 0x100));   /* a horizontal one */
    collisions[0 * 16 + 5] = 0x0f; CHECK(!oracles_room_transition_passable(&s, 0, 0x100));
    s.transition_direction = 1; s.link_x = 156 << 8; s.link_y = 80 << 8; memset(collisions, 0, sizeof collisions);
    collisions[5 * 16 + 0] = 0x1b; CHECK(!oracles_room_transition_walkable(&s, 0, 0x100) && oracles_room_transition_passable(&s, 0, 0x100));   /* rightward, a bridge's end */
    collisions[5 * 16 + 0] = 0x11; CHECK(!oracles_room_transition_passable(&s, 0, 0x100));

    /* Swimming beyond the game's own distance: water only, told by the tile
     * types of the room's collision list (tileTypesTable, bank 5 here). */
    uint8_t *types_rom = calloc(rom_size, 1);
    CHECK(types_rom != NULL);
    if (!types_rom) return 1;
    const size_t types = 5u * 0x4000u;
    types_rom[types + 0] = 0x10; types_rom[types + 1] = 0x40;          /* list 0 at $4010 */
    const uint8_t list[] = { 0xfd, 0x07, 0xf3, 0x01, 0xfc, 0x17, 0x40, 0x13, 0xe9, 0x18, 0x00 };   /* water, hole, sea, a current, a whirlpool */
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
    layout[5 * 16 + 0] = 0xfd; CHECK(oracles_room_transition_swimmable(types_rom, rom_size, &s, 0, 0x120));
    collisions[5 * 16 + 0] = 0x0f; CHECK(!oracles_room_transition_swimmable(types_rom, rom_size, &s, 0, 0x120));   /* a water tile index with a wall's collision */
    collisions[5 * 16 + 0] = 0;                                 /* the type alone says water (Ages' currents have no collision) */
    layout[5 * 16 + 0] = 0x40; CHECK(oracles_room_transition_swimmable(types_rom, rom_size, &s, 0, 0x120));   /* a current */
    layout[5 * 16 + 0] = 0xe9; CHECK(oracles_room_transition_swimmable(types_rom, rom_size, &s, 0, 0x120));   /* a whirlpool */
    s.game = ORACLES_GAME_SEASONS; CHECK(!oracles_room_transition_swimmable(types_rom, rom_size, &s, 0, 0x120));   /* Seasons has no such type */
    s.game = ORACLES_GAME_AGES;
    collisions[5 * 16 + 0] = 0x10;
    layout[5 * 16 + 0] = 0xf3; CHECK(!oracles_room_transition_swimmable(types_rom, rom_size, &s, 0, 0x120));  /* a hole */
    layout[5 * 16 + 0] = 0xfc; CHECK(!oracles_room_transition_swimmable(types_rom, rom_size, &s, 0, 0x120));  /* the sea, no mermaid suit */
    s.link_var2f = 0x40; CHECK(oracles_room_transition_swimmable(types_rom, rom_size, &s, 0, 0x120));
    s.game = ORACLES_GAME_SEASONS; CHECK(!oracles_room_transition_swimmable(types_rom, rom_size, &s, 0, 0x120));
    s.game = ORACLES_GAME_AGES;
    collisions[5 * 16 + 0] = 0; layout[5 * 16 + 0] = 0x01;       /* land: walkable, not swimmable */
    CHECK(oracles_room_transition_walkable(&s, 0, 0x120) && !oracles_room_transition_swimmable(types_rom, rom_size, &s, 0, 0x120));
    s.room_collisions = NULL; CHECK(oracles_room_transition_swimmable(types_rom, rom_size, &s, 0, 0x120));    /* not loaded yet */
    /* In the water his motion goes on across the axis: its angle (32 steps) off the transition's direction, the speed
     * split as objectSpeedTable splits it; straight, along the edge or away, nothing. */
    {
        OraclesGuestTransitionState w = s;
        int along = 0, across_step = 0;
        w.transition_direction = 3; w.link_move_angle = 20;                    /* left, moving down-left */
        CHECK(oracles_room_transition_moving_across(&w, 0x120, &along, &across_step) == 2 && along == 0x120 * 181 / 256 && across_step == 0x120 * 181 / 256);
        w.link_move_angle = 25;                                                 /* left, a step up */
        CHECK(oracles_room_transition_moving_across(&w, 0x120, &along, &across_step) == 0 && along == 0x120 * 251 / 256 && across_step == 0x120 * 50 / 256);
        w.link_move_angle = 24; CHECK(oracles_room_transition_moving_across(&w, 0x120, &along, &across_step) == -1);   /* straight */
        w.link_move_angle = 16; CHECK(oracles_room_transition_moving_across(&w, 0x120, &along, &across_step) == -1);   /* along the edge */
        w.link_move_angle = 8; CHECK(oracles_room_transition_moving_across(&w, 0x120, &along, &across_step) == -1);    /* away */
        w.link_move_angle = 0x80; CHECK(oracles_room_transition_moving_across(&w, 0x120, &along, &across_step) == -1); /* not moving */
        w.transition_direction = 0; w.link_move_angle = 4;                      /* up, moving up-right */
        CHECK(oracles_room_transition_moving_across(&w, 0x100, &along, &across_step) == 1);
        /* Across, into water only, at the game's two points below him (y+7, x-3 and x+2): Link swimming right at x=170
         * (column 0 of the room entered), going down. */
        memset(collisions, 0, sizeof collisions); memset(layout, 0, sizeof layout);
        w = s; w.transition_direction = 1; w.link_x = 170 << 8; w.link_y = 59 << 8; w.room_collisions = collisions; w.room_layout = layout;
        CHECK(!oracles_room_transition_across_swimmable(types_rom, rom_size, &w, 2, 0x80, 0, 0, 0));   /* land */
        collisions[4 * 16 + 0] = 0x10; layout[4 * 16 + 0] = 0xfd;
        CHECK(oracles_room_transition_across_swimmable(types_rom, rom_size, &w, 2, 0x80, 0, 0, 0));    /* 59.5 + 7 in row 4: water */
        w.link_y = 40 << 8; CHECK(!oracles_room_transition_across_swimmable(types_rom, rom_size, &w, 2, 0x80, 0, 0, 0));   /* 47.5: row 2, land */
    }
    /* Swimming down from y=121 in open water, carried right from x=45.5 (x 46, then 48 in column 3): a tile of land in
     * row 1, column 3, which his own tile (y+5) reaches at y=11, out of the water where the straight path stays in it. */
    {
        OraclesGuestTransitionState w = s;
        memset(collisions, 0x10, sizeof collisions); memset(layout, 0xfd, sizeof layout);
        w.transition_direction = 2; w.link_x = (47 << 8) + 0x80; w.link_y = 121 << 8; w.room_collisions = collisions; w.room_layout = layout;
        w.link_var2f = 0;
        CHECK(oracles_room_transition_across_swimmable(types_rom, rom_size, &w, 1, 0x80, 0, 0, 18 << 8));
        collisions[1 * 16 + 3] = 0; layout[1 * 16 + 3] = 0x01;
        CHECK(oracles_room_transition_across_swimmable(types_rom, rom_size, &w, 1, 0x80, 0, 0, 16 << 8));   /* y=9: his tile at 14, row 0 */
        CHECK(!oracles_room_transition_across_swimmable(types_rom, rom_size, &w, 1, 0x80, 0, 0, 18 << 8));
        collisions[1 * 16 + 2] = 0; layout[1 * 16 + 2] = 0x01;
        CHECK(oracles_room_transition_across_swimmable(types_rom, rom_size, &w, 1, 0x80, 0, 0, 18 << 8));    /* the straight path's on land too */
    }
    free(types_rom);

    /* A diagonal held toward the edge (wLinkAngle, 32 steps, 0 up) goes on across the axis: up-left adds left. */
    s = eligible(); s.transition_direction = 0;
    s.link_input_angle = 28; CHECK(oracles_room_transition_held_across(&s) == 3);
    s.link_input_angle = 4; CHECK(oracles_room_transition_held_across(&s) == 1);
    s.link_input_angle = 0; CHECK(oracles_room_transition_held_across(&s) == -1);      /* straight */
    s.link_input_angle = 8; CHECK(oracles_room_transition_held_across(&s) == -1);      /* across alone */
    s.link_input_angle = 20; CHECK(oracles_room_transition_held_across(&s) == -1);     /* away from the edge */
    s.link_input_angle = 0x80; CHECK(oracles_room_transition_held_across(&s) == -1);   /* none */
    s.transition_direction = 1; s.link_input_angle = 12; CHECK(oracles_room_transition_held_across(&s) == 2);
    s.transition_direction = 3; s.link_input_angle = 28; CHECK(oracles_room_transition_held_across(&s) == 0);
    s.transition_direction = 2; s.link_input_angle = 20; CHECK(oracles_room_transition_held_across(&s) == 3);
    /* Across, at the two points the game tests for walls on that side (right: x+4 at y and y+5), on the room entered's
     * collisions as the game reads them (a quarter of a tile under $10, columns or rows of a special value): Link up
     * at y=2 of the room he leaves is tested on the room entered's bottom row; nothing before its collisions are
     * loaded, nor out of the room across, nor onto a hole or water on foot. */
    memset(collisions, 0, sizeof collisions);
    s = eligible(); s.transition_direction = 0; s.link_x = 90 << 8; s.link_y = 2 << 8; s.room_collisions = collisions;
    CHECK(oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 0));
    collisions[7 * 16 + 6] = 0x0f;                              /* column 6 of the bottom row */
    CHECK(oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 0));   /* 90.7 + 4: column 5 still */
    s.link_x = 92 << 8; CHECK(!oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 0));
    CHECK(oracles_room_transition_across_free(&s, 3, 0xb5, 0, 0, 0));   /* leftward, away from it */
    collisions[7 * 16 + 6] = 0x05; CHECK(oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 0));   /* its right half alone */
    collisions[7 * 16 + 6] = 0x0a; CHECK(!oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 0));  /* its left half */
    collisions[7 * 16 + 6] = 0x18; CHECK(oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 0));   /* stairs */
    collisions[7 * 16 + 6] = 0x19; CHECK(!oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 0));  /* a horizontal bridge's bottom rail */
    collisions[7 * 16 + 6] = 0x10; CHECK(!oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 0));  /* water or a hole */
    /* A vertical bridge's left end ($12): its columns 0 to 3 alone, bit 0 the first two (bitTable). */
    collisions[7 * 16 + 6] = 0x12;
    s.link_x = (92 << 8) + 0x60; CHECK(!oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 0));   /* x+4 = 97 */
    s.link_x = (103 << 8) + 0x60; CHECK(oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 0));   /* x+4 = 108 */
    collisions[7 * 16 + 6] = 0;
    s.link_x = 156 << 8; CHECK(!oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 0));            /* x+4 out of the room */
    /* With the line the game lets him on up to the room's edge across, his points past it on the collision buffer's
     * border: free to x=159 (x+4 = 164, column 10), beyond it no further; leftward the same below zero. */
    CHECK(oracles_room_transition_across_free_line(&s, 1, 0xb5, 0, 0, 0));
    s.link_x = 159 << 8; CHECK(oracles_room_transition_across_free_line(&s, 1, 0xb5, 0, 0, 0));
    s.link_x = 3 << 8; CHECK(!oracles_room_transition_across_free(&s, 3, 0xb5, 0, 0, 0) && oracles_room_transition_across_free_line(&s, 3, 0xb5, 0, 0, 0));
    collisions[7 * 16 + 0] = 0x0f; CHECK(oracles_room_transition_across_free_line(&s, 3, 0xb5, 0, 0, 0));   /* column 0's wall: his points are past it */
    s.link_x = 6 << 8; CHECK(!oracles_room_transition_across_free_line(&s, 3, 0xb5, 0, 0, 0));   /* x-5 on it: a wall in the room still holds him */
    collisions[7 * 16 + 0] = 0;
    s.link_x = 80 << 8; s.room_collisions = NULL; CHECK(!oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 0));
    /* Rightward, in the room entered already (x=170: 10 there), carried down: tested at y+7, row 4. */
    memset(collisions, 0, sizeof collisions);
    s = eligible(); s.transition_direction = 1; s.link_x = 170 << 8; s.link_y = 59 << 8; s.room_collisions = collisions;
    CHECK(oracles_room_transition_across_free(&s, 2, 0xb5, 0, 0, 0));
    collisions[3 * 16 + 0] = 0x0f; CHECK(oracles_room_transition_across_free(&s, 2, 0xb5, 0, 0, 0));   /* row 3: not reached */
    collisions[4 * 16 + 0] = 0x0f; CHECK(!oracles_room_transition_across_free(&s, 2, 0xb5, 0, 0, 0));  /* row 4 */
    collisions[4 * 16 + 0] = 0;
    s.link_y = 121 << 8; CHECK(!oracles_room_transition_across_free(&s, 2, 0xb5, 0, 0, 0));   /* y+7 out of the room */
    CHECK(oracles_room_transition_across_free_line(&s, 2, 0xb5, 0, 0, 0));                    /* row 8, the border, with the line */
    s.link_y = 59 << 8; collisions[4 * 16 + 0] = 0x0f;
    /* At the position the move along the axis takes him to as well: from x=150 of the room left, 12 px along reach
     * column 0 of the room entered, whose wall below holds him; without them, its first column is tested too. */
    s.link_x = 150 << 8; CHECK(!oracles_room_transition_across_free(&s, 2, 0xb5, 12 << 8, 0, 0));
    collisions[4 * 16 + 0] = 0; collisions[4 * 16 + 9] = 0x0f; CHECK(oracles_room_transition_across_free(&s, 2, 0xb5, 12 << 8, 0, 0));
    /* His leading corner along the axis: at x=12 of the room entered, carried from y=58.5 to 59.2, his right point
     * (x+4, y+5) reaches row 4 of column 1, a wall in its top left quarter, which the game's move would walk him into;
     * already a wall without the carry (row 3 too), it holds nothing. */
    memset(collisions, 0, sizeof collisions);
    s.link_x = 172 << 8; s.link_y = (58 << 8) + 0x80; CHECK(oracles_room_transition_across_free(&s, 2, 0xb5, 0, 0, 0));
    collisions[4 * 16 + 1] = 0x08; CHECK(!oracles_room_transition_across_free(&s, 2, 0xb5, 0, 0, 0));
    collisions[3 * 16 + 1] = 0x0f; CHECK(oracles_room_transition_across_free(&s, 2, 0xb5, 0, 0, 0));

    /* Over the rest of the game's own distance as well: Link going down from y=121 of the room left, carried right,
     * keeps every point the game tests for walls, and the tile it takes as his (y+5), as clear as the straight path
     * wherever the scroll may leave him.  From x=45.5, 0xb5 to the right puts x+2 at 48, column 3; straight at 47. */
    memset(collisions, 0, sizeof collisions);
    s = eligible(); s.transition_direction = 2; s.link_x = (45 << 8) + 0x80; s.link_y = 121 << 8; s.room_collisions = collisions;
    collisions[1 * 16 + 3] = 0x08;                                   /* row 1's top left quarter: his feet (y+7 = 16) at 9 */
    CHECK(oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 0));
    CHECK(!oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 16 << 8));
    CHECK(oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 8 << 8));    /* the scroll leaves him at 1 at the furthest: y+7 = 8 */
    CHECK(oracles_room_transition_across_free(&s, 3, 0xb5, 0, 0, 16 << 8));   /* leftward, away from it */
    /* The carries before this one count: from x=46 one pixel off the straight path, this one takes x+2 into column 3
     * where the straight path's stays in column 2; taken alone, it moves nothing that the straight path would not. */
    s.link_x = 46 << 8;
    CHECK(oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 16 << 8));
    CHECK(!oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0x100, 16 << 8));
    /* The tile under him: a hole in row 1, column 3, which the carried path's y+5 reaches at y=11 and the straight
     * path's (x=47) never does; not a wall, so only its own point tells. */
    collisions[1 * 16 + 3] = 0x10; s.link_x = (47 << 8) + 0x80;
    CHECK(oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 16 << 8));
    CHECK(!oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 18 << 8));
    collisions[1 * 16 + 2] = 0x10; CHECK(oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 18 << 8));   /* the straight path over a hole too: the game's own */
    /* No carry while the room entered's collisions are not loaded (the vblanks of the transition's state 3), whatever
     * collisions the room left had. */
    s.room_collisions = NULL; CHECK(!oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 18 << 8));
    memset(collisions, 0, sizeof collisions); s.link_x = 40 << 8; s.link_y = 100 << 8;
    CHECK(!oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 0));
    s.room_collisions = collisions; CHECK(oracles_room_transition_across_free(&s, 1, 0xb5, 0, 0, 0));

    /* Leftward his x runs below zero as far as the transition takes him: at $ea (-22, 138 in the room entered, column
     * 8) and beyond -16, carried down, his feet (y+7 = 57) are tested in column 8, not the room's last. */
    memset(collisions, 0, sizeof collisions);
    s = eligible(); s.transition_direction = 3; s.link_y = 50 << 8; s.room_collisions = collisions;
    for (int xh = 0xf4; xh >= 0xe0; xh--) {
        const int column = (xh - 256 + 160) >> 4;
        memset(collisions, 0, sizeof collisions);
        s.link_x = (uint16_t)(xh << 8);
        CHECK(oracles_room_transition_across_free(&s, 2, 0xb5, 0, 0, 0));
        collisions[3 * 16 + column] = 0x0f;
        CHECK(!oracles_room_transition_across_free(&s, 2, 0xb5, 0, 0, 0));
    }

    /* Rightward, his own tile (y+5) in another row than his middle: at x=170 (10 in the room entered) and y=60, a hole
     * under y+5 = 65, row 4, holds him; at y=58, y+5 = 63 is in row 3 with him. */
    memset(collisions, 0, sizeof collisions);
    s = eligible(); s.transition_direction = 1; s.link_x = 170 << 8; s.link_y = 60 << 8; s.room_collisions = collisions;
    CHECK(oracles_room_transition_walkable(&s, 0, 0x100));
    collisions[4 * 16 + 0] = 0x10; CHECK(!oracles_room_transition_walkable(&s, 0, 0x100));
    s.link_y = 58 << 8; CHECK(oracles_room_transition_walkable(&s, 0, 0x100));

    /* What the machine still moves Link (8.8): a step a row, $80 down; 16 rows before the scroll, 32 steps of 4 px
     * when it is not doubled; the counter whole in phase 1, then the step after it reaches zero the last; two native
     * steps a row, the one landing the camera on a multiple of 8 drawing it. */
    s = eligible(); s.transition_direction = 2; s.transition_state = 3; s.transition_substate = 0;
    CHECK(oracles_room_transition_machine_rest(&s, 1) == 16 * 0x80 && oracles_room_transition_machine_rest(&s, 0) == 32 * 0x80);
    s.transition_state = 5; s.transition_substate = 1; s.transition_phase = 1; s.screen_scroll_counter = 16;
    CHECK(oracles_room_transition_machine_rest(&s, 1) == 16 * 0x80 && oracles_room_transition_machine_rest(&s, 0) == 32 * 0x80);
    s.transition_phase = 2; s.screen_scroll_counter = 10; s.camera_y = 6 * 8;
    CHECK(oracles_room_transition_machine_rest(&s, 1) == 11 * 0x80);
    CHECK(oracles_room_transition_machine_rest(&s, 0) == 22 * 0x80);
    s.camera_y = 6 * 8 + 4; CHECK(oracles_room_transition_machine_rest(&s, 0) == 21 * 0x80);   /* between two rows */
    s.screen_scroll_counter = 0; s.camera_y = 31 * 4; CHECK(oracles_room_transition_machine_rest(&s, 0) == 1 * 0x80);
    s.transition_phase = 3; CHECK(oracles_room_transition_machine_rest(&s, 1) == 0 && oracles_room_transition_machine_rest(&s, 0) == 0);
    /* Leftward, $60 a step, 20 columns, the camera running below zero. */
    s = eligible(); s.transition_direction = 3; s.transition_substate = 2; s.transition_phase = 2; s.screen_scroll_counter = 5;
    s.camera_x = (uint16_t)-12; CHECK(oracles_room_transition_machine_rest(&s, 0) == 11 * 0x60);
    s.camera_x = (uint16_t)-16; CHECK(oracles_room_transition_machine_rest(&s, 0) == 12 * 0x60 && oracles_room_transition_machine_rest(&s, 1) == 6 * 0x60);

    if (failures) return 1;
    puts("test_room_transition: ok");
    return 0;
}
