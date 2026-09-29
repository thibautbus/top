/* Per-game addresses the host hooks and reads.
 *
 * Every value comes from gen_guest_tables.py and the pinned disasm; nothing
 * here is written by hand.  Ages and Seasons have different addresses, so the
 * host always goes through the table of the game it runs. */
#ifndef ORACLES_GUEST_TABLES_H
#define ORACLES_GUEST_TABLES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ORACLES_GUEST_ABSENT 0xffu

#define ORACLES_GUEST_ITEM_LABELS 0x20u   /* NUM_INVENTORY_ITEMS of both games; the generator fails if a game has more */
#define ORACLES_GUEST_RINGS 0x40u         /* NUM_RINGS of both games; the generator fails otherwise */
#define ORACLES_GUEST_TREASURES 0x80u     /* treasure identifiers a mod may name */
#define ORACLES_GUEST_SOUNDS 0x100u       /* sound identifiers (music and effects) */
#define ORACLES_GUEST_GLOBAL_FLAGS 0x80u  /* global flags */

typedef struct OraclesGuestSym {
    uint8_t bank;     /* ROM bank for code, WRAM bank for $d000-$dfff, 0 otherwise; ORACLES_GUEST_ABSENT if the game has no such symbol */
    uint16_t addr;    /* CPU address */
} OraclesGuestSym;

typedef struct OraclesGuestTables {
    /* functions */
    OraclesGuestSym draw_all_sprites, update_all_objects, initialize_room, load_tileset_and_room_layout,
        object_create_interaction, get_free_interaction_slot, get_free_enemy_slot, enemy_standard_update,
        screen_transition_state2, apply_warp_dest, show_text, give_treasure, open_menu, save_file, poll_input,
        main_thread_start, apply_all_tile_substitutions, check_room_pack, load_object_gfx_header_to_slot4,
        /* Every enemy created (parseObjectData allocates through the uncounted
         * variant alone for obj_SpecificEnemyB and obj_ItemDrop), the parts, and the
         * sprites of wOam tagged by the object that wrote them. */
        get_free_enemy_slot_uncounted, get_free_part_slot,
        object_data_op6, object_data_op7,   /* the random-placement opcode of a room's object list, and the next one, which bounds it */
        parse_given_object_data,            /* the list's parser, which every opcode jumps back to: the end of the random placement */
        draw_object, draw_object_terrain_effects, draw_raw_oam_block, update_animations;
    /* variables */
    OraclesGuestSym active_group, active_room, loading_room, room_is_large, room_state_modifier, room_layout,
        room_collisions, tileset_flags, dungeon_index, toggle_blocks_state, portal_group, portal_room,
        scroll_mode, screen_transition_direction,
        disabled_objects,   /* the bits that freeze the objects: the ghost raises them to capture the room as the game creates it */
        screen_transition_state, screen_transition_substate, screen_transition_phase, screen_scroll_delta, screen_scroll_counter,
        screen_scroll_row, screen_scroll_column_right,
        vram_tiles, vram_attributes,
        tileset_animation, animation_counters, animation_queue, animation_queue_head, animation_queue_tail,
        animation_group_table, animation_gfx_headers,
        link_grab_state, link_swimming_state, magnet_glove_state, link_immobilized, link_pushing_direction,
        pegasus_seed_counter, link_playing_instrument, link_turning_disabled, force_link_push_animation, using_shield,
        link_id, link_state, link_visible, link_anim_counter, link_anim_parameter, link_anim_pointer, link_anim_mode,
        link_animation_frame, link_invincibility_counter, link_knockback_counter, link_stun_counter,
        parent_item2_enabled, parent_item3_enabled, parent_item4_enabled, parent_item5_enabled, weapon_item_enabled,
        special_object_animation_table,
        tile_types_table,   /* ROM: the tile types of each collision list (--continuous-swim: water ahead of Link swimming) */
        screen_transition_delay, active_tile_index,   /* --continuous-swim: the transition a swim's momentum takes to the edge */
        screen_transition_boundary_x, screen_transition_boundary_y,
        disable_screen_transitions, disable_warp_tiles, link_force_state, link_in_air, palette_thread_mode, palette_thread_parameter, cutscene_trigger,
        room_width, room_height, tileset_unique_gfx, active_collisions, dungeon_floor, dungeon_map_position, dungeon_layout,
        warp_transition, warp_dest_pos, warp_dest_group, warp_dest_room, warp_transition2, active_tile_pos, lost_woods_transition_counter1, lost_woods_transition_counter2, link_time_warp_tile, is_linked_game, switch_state, rom_bank, jabu_water_level, gameboy_type,
        twinrova_tile_replacement_mode, seed_tree_refilled_bitset, ricky_state, essences_obtained, animal_companion, num_placed_slates,
        changed_tile_queue_head, changed_tile_queue_tail, tileset_layout_group, gasha_spots_planted,
        tileset_gfx, tileset_palette, loaded_tileset_unique_gfx,
        loaded_tileset_palette, dirty_bg_palettes, room_pack, loading_room_pack, room_pack_data, map_transition_group_table,
        season_reload, link_angle, hole_or_conveyor,
        screen_offset_x, screen_offset_y, camera_x, camera_y,
        keys_pressed, keys_just_pressed, frame_counter, text_is_active, selected_text_option, opened_menu_type, minimap_group, global_flags, bought_shop_items1,
        group0_room_flags, link_object_index, objects_to_draw, oam, oam_tail, terrain_effects_used, link_raised_floor_offset, textbox_flags, enemies_killed_list,
        shadow_animation, green_grass_animation, puddle_animation_pointer, grass_animation_modifier, animation_state,
        gfx_regs1, gfx_regs2, gfx_regs3, vblank_checker, lcd_interrupt_behaviour, big_buffer,
        tile_mapping_data, tile_mapping_indices, tile_collisions, bg_palettes_buffer, spr_palettes_buffer, tileset_bg_palettes,
        status_bar_tile_map, status_bar_attribute_map, textbox_map, thread_state_buffer,
        main_stack, main_stack_top, thread0_stack, thread0_stack_top, thread1_stack, thread1_stack_top,
        thread2_stack, thread2_stack_top, thread3_stack, thread3_stack_top;
    /* Item hotkeys: the write point's function and the sign of the status bar's refresh, the eighteen
     * inventory slots (B, A, then sixteen of storage, contiguous), the variants, and the conditions of the inventory menu. */
    OraclesGuestSym check_reload_status_bar_graphics, load_equipped_item_gfx,
        inventory_b, inventory_a, inventory_storage, satchel_selected_seeds, shooter_selected_seeds, selected_harp_song,
        obtained_treasure_flags, status_bar_needs_refresh, dont_update_status_bar, menu_disabled,
        disable_link_collisions_and_menu, link_death_trigger, use_simulated_input, in_boxing_match,
        vblank_function_queue, vblank_function_queue_tail,   /* the harness's live-state dump */
        general_purpose_hram, general_purpose_hram_end, tmp_cec0, tmp_cec0_end;
    /* The `use` mode: where the game reads the item buttons, what it reads to choose them (fact 10), and the inventory's cursor (point 7). */
    OraclesGuestSym check_use_items, initialize_parent_item, items_disabled, in_shop, link_in_spinner, link_grabbed, link_climbing_vine,
        menu_load_state, menu_active_state, inventory_submenu, inventory_submenu0_cursor_pos;
    /* The call transaction (a mod's host scene): the game's routine that takes rupees away (giveTreasure is above), the
     * counts a mod reads before it pays or gives, and the font of the text box (ROM: character c at font_start + 16 c). */
    OraclesGuestSym remove_rupee_value, lose_treasure, play_sound, set_global_flag, unset_global_flag, num_rupees, link_health, link_max_health, num_gasha_seeds, seed_satchel_level, num_ember_seeds,
        font_start;
    /* A mod's storage: the entry of the game's four file operations (c: 0 create, 1 save, 2 load, 3 erase) and the slot
     * of the file concerned. */
    OraclesGuestSym file_management_function, active_file_slot;
    /* defines */
    OraclesGuestSym game_state, cutscene_index;
    /* one room's flag byte, by the room's constant (ROOM_FLAG_BYTES of the generator) */
    OraclesGuestSym maku_tree_tileset_room_flags, veran_room_flags;
    /* sizes, from the _sizeof_ entries of the symbol table */
    uint16_t vblank_function_queue_size;
    uint16_t global_flags_size, room_flags_size;   /* wGlobalFlags; the four pages of room flags */
    uint16_t obtained_treasure_flags_size, gasha_spots_planted_size;
    /* constants of the build, from the same entries */
    uint8_t oam_data_bank;                          /* BASE_OAM_DATA_BANK: the first bank of the objects' sprite data */
    uint8_t puddle_tile_first, puddle_tile_last;    /* the tiles of a puddle (tileIndices.s), where an object is drawn a pixel lower */
    uint8_t shutter_tile_first, shutter_tile_last;  /* the shutters replaceShutterForLinkEntering opens under Link: `sub first ; cp count` */
    uint8_t grass_tile_first, grass_tile_last;      /* the tiles of grass */
    uint8_t grass_tile_last_other_groups;           /* the last of them outside group 0: hack-base's Seasons takes $f9 there for the
                                                       Cane of Somaria's block and draws no grass on it */
    /* Constants of the disassembly's constants/ files; 0xff when the game has none. */
    uint8_t num_inventory_items, item_biggoron_sword, item_seed_satchel, item_shooter, item_slingshot, item_harp,
        treasure_ember_seeds, treasure_tune_of_echoes, globalflag_intro_done, cutscene_ingame, cutscene_loading_room, cutscene_onox_final_form, cutscene_pregame_intro,
        cutscene_din_imprisoned, cutscene_temple_sinking, cutscene_onox_taunting,
        interac_wild_tokay_controller, interac_golden_cave_subrosian,
        tilesetflag_bit_sidescroll, tilesetflag_bit_underwater, menu_inventory, specialobject_minecart, specialobject_raft,
        first_dynamic_interaction_index, roomflag_visited,
        /* the call transaction: the treasures the high-level prizes give, and the number of rings */
        treasure_rupees, treasure_heart_refill, treasure_ring, treasure_ring_box, treasure_gasha_seed, treasure_seed_satchel,
        num_rings;
    /* The global flags the load around the substitutions reads, 0xff for the game whose load does not (ghost_key.c, the coarse list). */
    uint8_t coarse_flag_finished_game, coarse_flag_temple_lava, coarse_flag_moblins_keep, coarse_flag_pirate_ship, coarse_flag_intro_done;
    /* The one overworld room (group 0) whose terrain GLOBALFLAG_INTRO_DONE changes, 0xff for none: Seasons' after-load
     * tile changes of the room west of Din's troupe draw the wagon until it is set (roomTileChangesAfterLoad0e); the
     * flag's change throws that room alone, the rest of the cache stands (ghost_key.c, oracles_ghost_coarse_changed). */
    uint8_t coarse_flag_intro_done_room;
    /* The hotbar's label of four letters and the name of each item a button can hold, by identifier; empty for the others. */
    char item_labels[ORACLES_GUEST_ITEM_LABELS][5];
    const char *item_names[ORACLES_GUEST_ITEM_LABELS];
    /* Each ring's name as the disassembly's constant, lower case ("power_ring_l1"), by identifier (the call transaction). */
    const char *ring_names[ORACLES_GUEST_RINGS];
    /* The treasures, sounds and global flags a mod names (the call transaction), "" for an identifier without a name. */
    const char *treasure_names[ORACLES_GUEST_TREASURES];
    const char *sound_names[ORACLES_GUEST_SOUNDS];
    const char *global_flag_names[ORACLES_GUEST_GLOBAL_FLAGS];
    /* The tables the call transaction reads from the ROM at the time of a call (guest_call.c; docs/ROM_DATA_FORMATS.md,
     * section 5.4): how giveTreasure applies each treasure's parameter (three bytes a treasure), the treasures' objects
     * with the parameters the game gives them (four bytes a treasure, or a pointer to its subids' list, all of them before
     * treasure_object_lists_end), and getRupeeValue's amounts (a BCD word a value); the number of entries of the first
     * two and of rupee values. */
    OraclesGuestSym treasure_collection_behaviours, treasure_object_data, treasure_object_lists_end, rupee_values;
    uint8_t treasure_collection_behaviour_count, treasure_object_count;
    uint8_t rupee_value_count;
} OraclesGuestTables;

extern const OraclesGuestTables oracles_guest_tables_ages;
extern const OraclesGuestTables oracles_guest_tables_seasons;

#ifdef __cplusplus
}
#endif

#endif
