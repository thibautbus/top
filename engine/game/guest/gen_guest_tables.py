#!/usr/bin/env python3
"""Generate the per-game guest tables of the engine from the pinned disasm.

Every address the host reads or hooks comes from here, resolved per game:
Ages and Seasons do not share WRAM, HRAM or code addresses (docs/GAME_HOOKS.md,
section 7).  Object structure offsets come from include/structs.s
and are the same for both games.

usage: gen_guest_tables.py [--disasm DIR]     writes guest_tables_ages.c, guest_tables_seasons.c, guest_struct_offsets.h
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent

# (C field, symbol) — functions hooked by the host (docs/GAME_HOOKS.md, section 6) and
# the variables its accessors read. A symbol absent from one game's table is an
# error unless it is listed in OPTIONAL.
FUNCTIONS = [
    ("draw_all_sprites", "drawAllSprites"),
    ("update_all_objects", "updateAllObjects"),
    ("initialize_room", "initializeRoom"),
    ("load_tileset_and_room_layout", "loadTilesetAndRoomLayout"),
    ("object_create_interaction", "objectCreateInteraction"),
    ("get_free_interaction_slot", "getFreeInteractionSlot"),
    ("get_free_enemy_slot", "getFreeEnemySlot"),
    # ParseObjectData allocates through the uncounted variant alone for
    # obj_SpecificEnemyB and obj_ItemDrop; getFreeEnemySlot calls it, so one
    # hook there sees every enemy created.
    ("get_free_enemy_slot_uncounted", "getFreeEnemySlot_uncounted"),
    ("get_free_part_slot", "getFreePartSlot"),
    # The opcode of a room's object list that places enemies at random,
    # bounded by the next one: an object created between the two is one of them.
    ("object_data_op6", "objectDataOp6"),
    ("object_data_op7", "objectDataOp7"),
    # The parser of a room's object list; every opcode jumps back to it
    # (none returns), so its entry ends the random-placement opcode.
    ("parse_given_object_data", "parseGivenObjectData"),
    # The sprites of wOam tagged by the object that wrote them.
    ("draw_object", "drawAllSpritesUnconditionally@drawObject"),
    ("draw_object_terrain_effects", "_drawObjectTerrainEffects"),
    # The game's step of its tile animation, counted to step the neighbours' as many times.
    ("update_animations", "updateAnimations"),
    ("draw_raw_oam_block", "func_0eda"),
    ("enemy_standard_update", "enemyStandardUpdate"),
    ("screen_transition_state2", "screenTransitionState2"),
    ("apply_warp_dest", "applyWarpDest"),
    ("show_text", "showText"),
    ("give_treasure", "giveTreasure"),
    ("open_menu", "openMenu"),
    ("save_file", "saveFile"),
    ("poll_input", "pollInput"),
    ("main_thread_start", "mainThreadStart"),
    ("apply_all_tile_substitutions", "applyAllTileSubstitutions"),   # ghost instance: read trace window
    ("check_room_pack", "checkRoomPack"),   # ghost instance: the area a scroll enters (Enhanced, Seasons: one season for the band)
    ("load_object_gfx_header_to_slot4", "loadObjectGfxHeaderToSlot4"),   # ghost instance: the object graphics load inside Seasons' substitutions, outside the read trace
    # Item hotkeys: the write point is the return of the one call of checkReloadStatusBarGraphics in mainThreadStart;
    # loadEquippedItemGfx proves the status bar followed an exchange.
    ("check_reload_status_bar_graphics", "checkReloadStatusBarGraphics"), ("load_equipped_item_gfx", "loadEquippedItemGfx"),
    # Item hotkeys, the `use` mode: where the game reads the two item buttons (the bank-0 entry Link's update calls).
    ("check_use_items", "checkUseItems"),
    # and where it makes an item start: initializeParentItem, e = the item, d = the button (what the harness checks to know that a simulated press fired).
    ("initialize_parent_item", "initializeParentItem"),
    # The call transaction (a mod's host scene): the game's routines a mod may have it run, beside giveTreasure;
    # each takes its parameter in a (bank0.s).
    ("remove_rupee_value", "removeRupeeValue"), ("lose_treasure", "loseTreasure"), ("play_sound", "playSound"),
    ("set_global_flag", "setGlobalFlag"), ("unset_global_flag", "unsetGlobalFlag"),
    # A mod's storage follows the game's files: the one entry of their four operations (c: create, save, load, erase).
    ("file_management_function", "fileManagementFunction"),
]
VARIABLES = [
    ("active_group", "wActiveGroup"), ("active_room", "wActiveRoom"), ("loading_room", "wLoadingRoom"),
    ("room_is_large", "wRoomIsLarge"), ("room_state_modifier", "wRoomStateModifier"),
    ("room_layout", "wRoomLayout"), ("room_collisions", "wRoomCollisions"),
    ("tileset_flags", "wTilesetFlags"), ("dungeon_index", "wDungeonIndex"),
    ("toggle_blocks_state", "wToggleBlocksState"),
    ("portal_group", "wPortalGroup"), ("portal_room", "wPortalRoom"),
    ("scroll_mode", "wScrollMode"), ("screen_transition_direction", "wScreenTransitionDirection"),
    # The ghost freezes the objects of the room it entered to capture
    # them in the state the game creates them in.
    ("disabled_objects", "wDisabledObjects"),
    # Ghost instance: priming a scrolling transition and judging whether the state allows one.
    ("screen_transition_state", "wScreenTransitionState"),
    # Continuous transitions: the scroll machine's substate, phase, step and counter, and Link's gait.
    ("screen_transition_substate", "wScreenTransitionState2"), ("screen_transition_phase", "wScreenTransitionState3"),
    ("screen_scroll_delta", "wcd14"), ("screen_scroll_counter", "wScreenScrollCounter"),
    # The curtain of a warp (screenTransitionState1) draws the columns of
    # its map outward from the centre, one per frame; these two hold the next
    # column to draw on each side.
    ("screen_scroll_row", "wScreenScrollRow"), ("screen_scroll_column_right", "wScreenScrollDirection"),
    ("link_grab_state", "wLinkGrabState"), ("link_swimming_state", "wLinkSwimmingState"),
    ("magnet_glove_state", "wMagnetGloveState"), ("link_immobilized", "wLinkImmobilized"),
    ("link_pushing_direction", "wLinkPushingDirection"), ("pegasus_seed_counter", "wPegasusSeedCounter"),
    ("link_playing_instrument", "wLinkPlayingInstrument"), ("link_turning_disabled", "wLinkTurningDisabled"),
    ("force_link_push_animation", "wForceLinkPushAnimation"), ("using_shield", "wUsingShield"),
    ("link_id", "w1Link.id"), ("link_state", "w1Link.state"), ("link_visible", "w1Link.visible"),
    ("link_anim_counter", "w1Link.animCounter"), ("link_anim_parameter", "w1Link.animParameter"),
    ("link_anim_pointer", "w1Link.animPointer"), ("link_anim_mode", "w1Link.animMode"),
    ("link_animation_frame", "w1Link.var31"), ("link_invincibility_counter", "w1Link.invincibilityCounter"),
    ("link_knockback_counter", "w1Link.knockbackCounter"), ("link_stun_counter", "w1Link.stunCounter"),
    ("parent_item2_enabled", "w1ParentItem2.enabled"), ("parent_item3_enabled", "w1ParentItem3.enabled"),
    ("parent_item4_enabled", "w1ParentItem4.enabled"), ("parent_item5_enabled", "w1ParentItem5.enabled"),
    ("weapon_item_enabled", "w1WeaponItem.enabled"),
    ("special_object_animation_table", "specialObjectAnimationTable"),
    ("tile_types_table", "tileTypesTable"),   # ROM: continuous swim, the water Link may swim into beyond the game's own distance
    # Continuous swim: what screenTransitionState2@transition reads before a transition, besides the direction held.
    ("screen_transition_delay", "wScreenTransitionDelay"), ("active_tile_index", "wActiveTileIndex"),
    ("screen_transition_boundary_x", "wScreenTransitionBoundaryX"), ("screen_transition_boundary_y", "wScreenTransitionBoundaryY"),
    ("disable_screen_transitions", "wDisableScreenTransitions"), ("disable_warp_tiles", "wDisableWarpTiles"),
    ("link_force_state", "wLinkForceState"),
    ("link_in_air", "wLinkInAir"), ("palette_thread_mode", "wPaletteThread_mode"), ("palette_thread_parameter", "wPaletteThread_parameter"), ("cutscene_trigger", "wCutsceneTrigger"),
    ("room_width", "wRoomWidth"), ("room_height", "wRoomHeight"), ("tileset_unique_gfx", "wTilesetUniqueGfx"),
    ("active_collisions", "wActiveCollisions"), ("dungeon_floor", "wDungeonFloor"), ("dungeon_map_position", "wDungeonMapPosition"),
    ("dungeon_layout", "w2DungeonLayout"),
    ("warp_transition", "wWarpTransition"), ("warp_dest_pos", "wWarpDestPos"), ("link_time_warp_tile", "wLinkTimeWarpTile"),
    # Ghost: the warp a dive into the sea or a return to its surface sets (checkForUnderwaterTransition, link.s), which a run ahead of it sets itself.
    ("warp_dest_group", "wWarpDestGroup"), ("warp_dest_room", "wWarpDestRoom"), ("warp_transition2", "wWarpTransition2"), ("active_tile_pos", "wActiveTilePos"),
    # Ghost: the counters the Lost Woods routes its transitions by (screenTransitionLostWoods, Seasons), the routing key of its answers.
    ("lost_woods_transition_counter1", "wLostWoodsTransitionCounter1"), ("lost_woods_transition_counter2", "wLostWoodsTransitionCounter2"),
    ("active_file_slot", "hActiveFileSlot"),   # the file an operation of fileManagementFunction concerns (a mod's storage)
    ("is_linked_game", "wIsLinkedGame"), ("switch_state", "wSwitchState"), ("rom_bank", "hRomBank"),
    # Ghost: Jabu-Jabu's water level decides its tiles (Ages); the hardware type opens the GBA shops (read by the substitutions, constant).
    ("jabu_water_level", "wJabuWaterLevel"), ("gameboy_type", "hGameboyType"),
    # Ghost: the other bytes the substitutions read, in the cache key or exempt from it (ghost_key.c).
    ("twinrova_tile_replacement_mode", "wTwinrovaTileReplacementMode"), ("seed_tree_refilled_bitset", "wSeedTreeRefilledBitset"),
    ("ricky_state", "wRickyState"), ("essences_obtained", "wEssencesObtained"), ("animal_companion", "wAnimalCompanion"),
    # A fan game's own (absent from both originals): the slates Gifts of Kinomi's substitutions read in its slate room.
    ("num_placed_slates", "wNumPlacedSlates"),
    ("changed_tile_queue_head", "wChangedTileQueueHead"), ("changed_tile_queue_tail", "wChangedTileQueueTail"),
    ("tileset_layout_group", "wTilesetLayoutGroup"),
    # Ghost: the load around the substitutions (the coarse list, ghost_key.c): the Gasha seeds planted.
    ("gasha_spots_planted", "wGashaSpotsPlantedBitset"),
    # Enhanced: the tileset a room has loaded, to render a neighbour's map with the live tiles when they are the same.
    ("tileset_gfx", "wTilesetGfx"), ("tileset_palette", "wTilesetPalette"), ("loaded_tileset_unique_gfx", "wLoadedTilesetUniqueGfx"),
    # Continuous transitions: a palette fade that outlives the shortened scroll, refreshed as the scroll's end would.
    ("loaded_tileset_palette", "wLoadedTilesetPalette"), ("dirty_bg_palettes", "hDirtyBgPalettes"),
    # Enhanced: the area (room pack) a room belongs to, whose season a room of the same area keeps (Seasons).
    ("room_pack", "wRoomPack"), ("loading_room_pack", "wLoadingRoomPack"),
    ("room_pack_data", "roomPackData"),   # ROM: the room pack of each overworld room (group 0)
    # The rooms whose transitions the game routes itself (getNextActiveRoom,
    # docs/GAME_HOOKS.md, section 4.3).  ROM: eight pointers by group, each to pairs of room and case
    # ended by a null room.
    ("map_transition_group_table", "mapTransitionGroupTable"),
    # Ghost: a season change pending its room reload (setSeason, bank0.s; the main loop fades out before any transition).
    ("season_reload", "wcc4c"),
    # Ghost: the direction Link moves in, which the game checks before a transition at an edge (convertLinkAngleToDirectionButtons).
    ("link_angle", "wLinkAngle"),
    # Ghost: over a hole (bit 7) or on a conveyor (bits 0-6), which decide a transition at an edge without input.
    ("hole_or_conveyor", "wcc92"),
    ("screen_offset_x", "wScreenOffsetX"), ("screen_offset_y", "wScreenOffsetY"),
    ("camera_x", "hCameraX"), ("camera_y", "hCameraY"),
    ("keys_pressed", "wKeysPressed"), ("keys_just_pressed", "wKeysJustPressed"),
    ("frame_counter", "wFrameCounter"),
    ("text_is_active", "wTextIsActive"), ("selected_text_option", "wSelectedTextOption"),
    ("opened_menu_type", "wOpenedMenuType"), ("minimap_group", "wMinimapGroup"),
    ("global_flags", "wGlobalFlags"), ("bought_shop_items1", "wBoughtShopItems1"), ("group0_room_flags", "wGroup0RoomFlags"),
    ("link_object_index", "wLinkObjectIndex"),
    ("objects_to_draw", "wObjectsToDraw"), ("oam", "wOam"), ("oam_tail", "hOamTail"),
    # The shadows an object queues while drawn (the tagging of the blocks
    # written after the loop), Link's raised floor (Ages) and the text box's
    # Link-only drawing (drawAllSpritesUnconditionally), for the sprites built.
    ("terrain_effects_used", "hTerrainEffectsBufferUsedSize"), ("link_raised_floor_offset", "wLinkRaisedFloorOffset"),
    ("textbox_flags", "wTextboxFlags"),
    # The terrain effects' data (data/terrainEffects.s) and the frame's
    # animation of grass and puddles, for the sprites built.
    ("shadow_animation", "shadowAnimation"), ("green_grass_animation", "greenGrassAnimationFrame0"),
    ("puddle_animation_pointer", "wPuddleAnimationPointer"), ("grass_animation_modifier", "wGrassAnimationModifier"),
    # The enemies killed in the last eight rooms, which a neighbour's captured objects depend on.
    ("enemies_killed_list", "wEnemiesKilledList"),
    ("animation_state", "wAnimationState"),
    # A room's tile animation, to advance a neighbour's from the game's
    # data (the four streams, the queue of graphics indices, and the ROM tables).
    ("tileset_animation", "wTilesetAnimation"), ("animation_counters", "wAnimationCounter1"),
    ("animation_queue", "w2AnimationQueue"), ("animation_queue_head", "wAnimationQueueHead"), ("animation_queue_tail", "wAnimationQueueTail"),
    ("animation_group_table", "animationGroupTable"), ("animation_gfx_headers", "animationGfxHeaders"),
    ("gfx_regs1", "wGfxRegs1"), ("gfx_regs2", "wGfxRegs2"), ("gfx_regs3", "wGfxRegs3"),
    ("vblank_checker", "wVBlankChecker"), ("lcd_interrupt_behaviour", "hLcdInterruptBehaviour"),
    ("big_buffer", "wBigBuffer"),
    ("tile_mapping_data", "w3TileMappingData"), ("tile_mapping_indices", "w3TileMappingIndices"),
    ("tile_collisions", "w3TileCollisions"),
    # The room's background map as the game keeps it to copy into the
    # VRAM; a text box is written over the VRAM only, so the cells that differ
    # from this image are the box's.
    ("vram_tiles", "w3VramTiles"), ("vram_attributes", "w3VramAttributes"),
    ("bg_palettes_buffer", "w2BgPalettesBuffer"), ("spr_palettes_buffer", "w2SprPalettesBuffer"),
    ("tileset_bg_palettes", "w2TilesetBgPalettes"),
    ("status_bar_tile_map", "w4StatusBarTileMap"), ("status_bar_attribute_map", "w4StatusBarAttributeMap"), ("textbox_map", "w7TextboxMap"),
    ("thread_state_buffer", "wThreadStateBuffer"),
    ("main_stack", "wMainStack"), ("main_stack_top", "wMainStackTop"),
    ("thread0_stack", "wThread0Stack"), ("thread0_stack_top", "wThread0StackTop"),
    ("thread1_stack", "wThread1Stack"), ("thread1_stack_top", "wThread1StackTop"),
    ("thread2_stack", "wThread2Stack"), ("thread2_stack_top", "wThread2StackTop"),
    ("thread3_stack", "wThread3Stack"), ("thread3_stack_top", "wThread3StackTop"),
    # Item hotkeys: the eighteen inventory slots (B, A, sixteen of storage, contiguous), the variants, the status bar's
    # refresh bit, and what says whether the player could open the inventory.
    ("inventory_b", "wInventoryB"), ("inventory_a", "wInventoryA"), ("inventory_storage", "wInventoryStorage"),
    ("satchel_selected_seeds", "wSatchelSelectedSeeds"), ("shooter_selected_seeds", "wShooterSelectedSeeds"),
    ("selected_harp_song", "wSelectedHarpSong"), ("obtained_treasure_flags", "wObtainedTreasureFlags"),
    ("status_bar_needs_refresh", "wStatusBarNeedsRefresh"), ("dont_update_status_bar", "wDontUpdateStatusBar"),
    ("menu_disabled", "wMenuDisabled"), ("disable_link_collisions_and_menu", "wDisableLinkCollisionsAndMenu"),
    ("link_death_trigger", "wLinkDeathTrigger"), ("use_simulated_input", "wUseSimulatedInput"), ("in_boxing_match", "wInBoxingMatch"),
    # The `use` mode: what checkUseItems reads to decide which of A and B use an item in this state.
    ("items_disabled", "wcc63"), ("in_shop", "wInShop"), ("link_in_spinner", "wcc95"),
    ("link_grabbed", "wccd8"), ("link_climbing_vine", "wLinkClimbingVine"),   # wLinkInAir, wLinkGrabState, wLinkSwimmingState and wDisabledObjects are above
    # The assignment in the inventory (point 7): the item submenu waiting for an input, and its cursor.
    ("menu_load_state", "wMenuLoadState"), ("menu_active_state", "wMenuActiveState"), ("inventory_submenu", "wInventorySubmenu"),
    ("inventory_submenu0_cursor_pos", "wInventorySubmenu0CursorPos"),   # wPaletteThread_mode is above
    # The harness's live-state dump: the vblank function queue, whose entries past its tail are already consumed.
    ("vblank_function_queue", "wVBlankFunctionQueue"), ("vblank_function_queue_tail", "hVBlankFunctionQueueTail"),
    # and the scratch the game documents as such: hFF8A to hFF93 (hram.s, "General-purpose variables"), wTmpcec0 to $ceff (wram.s, "several different uses depending on context").
    # The call transaction: what a mod reads before it pays or gives (the counts the game's routines change).
    ("num_rupees", "wNumRupees"), ("link_health", "wLinkHealth"), ("link_max_health", "wLinkMaxHealth"),
    ("num_gasha_seeds", "wNumGashaSeeds"), ("seed_satchel_level", "wSeedSatchelLevel"), ("num_ember_seeds", "wNumEmberSeeds"),
    ("general_purpose_hram", "hFF8A"), ("general_purpose_hram_end", "hRng1"), ("tmp_cec0", "wTmpcec0"), ("tmp_cec0_end", "wRoomLayout"),
]
# Defines of wram.s expressed as symbol + offset.
DEFINES = [("game_state", "wGameState"), ("cutscene_index", "wCutsceneIndex")]
# The flag byte of one room, read by name by the load around the substitutions (the coarse list, ghost_key.c): the
# room's page (flagLocationGroupTable, bank0.s) and its constant (constants/common/rooms.s), whose low byte is the
# offset; absent from the game that has no such room.
ROOM_FLAG_BYTES = [("maku_tree_tileset_room_flags", "wGroup1RoomFlags", "ROOM_AGES_148"),   # Ages: bit 0, the Maku Tree saved, 0:38's tileset
                   ("veran_room_flags", "wGroup4RoomFlags", "ROOM_AGES_4fc")]              # Ages: bit 7, Veran beaten, 0:38's staircase
OPTIONAL = {"wPortalGroup", "wPortalRoom", "wLinkTimeWarpTile", "wJabuWaterLevel", "wLinkRaisedFloorOffset", "wSelectedHarpSong",   # Ages only
            "wInBoxingMatch",   # Seasons only
            "wNumPlacedSlates"}   # neither original: a fan game's (Gifts of Kinomi)
STRUCT_FIELDS = ["enabled", "id", "subid", "var03", "state", "substate", "counter1", "counter2", "direction",
                 "angle", "y", "yh", "x", "xh", "z", "zh", "speed", "speedZ", "relatedObj1", "relatedObj2",
                 "visible", "oamFlagsBackup", "oamFlags", "oamTileIndexBase", "oamDataAddress", "animCounter",
                 "animParameter", "animPointer", "collisionType", "enemyCollisionMode", "collisionRadiusY",
                 "collisionRadiusX", "damage", "health", "invincibilityCounter", "stunCounter", "var30", "var31",
                 "var2f", "var32", "var33", "var34", "var35", "var3f"]


def find_disasm() -> Path:
    for ancestor in HERE.parents:
        candidate = ancestor / "oracles-disasm"
        if (candidate / "ages.sym").exists():
            return candidate
    return Path("oracles-disasm")


# A symbol defined at two addresses (a bank-0 wrapper and its body in another
# bank) is resolved only by an explicit choice: the bank the host must hook.
# "banked": the body outside bank 0, whatever its bank in each game.
DUPLICATES = {"saveFile": 0,   # the bank-0 wrapper the game calls (bank0.s)
              "checkUseItems": 0,   # the bank-0 entry Link's update calls
              "playSound": 0,   # the bank-0 entry, which calls the sound bank itself (the call transaction)
              "parseGivenObjectData": "banked"}   # the loop objectLoading.s's opcodes jump back to, not the bank-0 wrapper
# Sizes the host needs, from the `_sizeof_` entries of the symbol table.
SIZES = [("global_flags_size", "_sizeof_wGlobalFlags", 1), ("room_flags_size", "_sizeof_wGroup0RoomFlags", 4),
         ("obtained_treasure_flags_size", "_sizeof_wObtainedTreasureFlags", 1), ("gasha_spots_planted_size", "_sizeof_wGashaSpotsPlantedBitset", 1),
         ("vblank_function_queue_size", "_sizeof_wVBlankFunctionQueue", 1)]
# Constants of the build, from the same entries of the symbol table:
# the first ROM bank of the objects' sprite data (drawAllSprites@drawObject).
CONSTANTS = [("oam_data_bank", "BASE_OAM_DATA_BANK")]
# Constants the symbol table does not carry, from constants/common/tileIndices.s:
# the tiles where _drawObjectTerrainEffects draws a puddle and lowers the
# object's sprites by a pixel (TILEINDEX_PUDDLE in Ages; from it to below
# TILEINDEX_WATER in Seasons), and those where it draws grass
# (TILEINDEX_GRASS in Ages; from it to below TILEINDEX_PUDDLE in Seasons).
# Constants of the disassembly's constants/ files (enums and defines the symbol
# table does not carry), resolved per game; a name absent from a game is 0xff,
# as is one given a fourth element naming the only game it belongs to (the
# cutscene indices of both games share one file).
# Item hotkeys.
DISASM_CONSTANTS = [
    ("num_inventory_items", "common/treasure.s", "NUM_INVENTORY_ITEMS"),
    ("item_biggoron_sword", "common/items.s", "ITEM_BIGGORON_SWORD"), ("item_seed_satchel", "common/items.s", "ITEM_SEED_SATCHEL"),
    ("item_shooter", "common/items.s", "ITEM_SHOOTER"), ("item_slingshot", "common/items.s", "ITEM_SLINGSHOT"), ("item_harp", "common/items.s", "ITEM_HARP"),
    ("treasure_ember_seeds", "common/treasure.s", "TREASURE_EMBER_SEEDS"), ("treasure_tune_of_echoes", "common/treasure.s", "TREASURE_TUNE_OF_ECHOES"),
    ("globalflag_intro_done", "common/globalFlags.s", "GLOBALFLAG_INTRO_DONE"),
    ("cutscene_ingame", "common/cutsceneIndices.s", "CUTSCENE_INGAME"), ("cutscene_loading_room", "common/cutsceneIndices.s", "CUTSCENE_LOADING_ROOM"), ("cutscene_onox_final_form", "common/cutsceneIndices.s", "CUTSCENE_S_ONOX_FINAL_FORM"),
    # The cutscenes the Enhanced view frames, which draw their own screens over the room left loaded: the pregame intro
    # (CUTSCENE_PREGAME_INTRO, each game's own name), and in Seasons Din taken to Onox's castle, the temple sinking and
    # Onox's taunt (introCutscenes.s: a scene's graphics header and tile map); 0xff in Ages.
    ("cutscene_pregame_intro", "common/cutsceneIndices.s", "CUTSCENE_{s}PREGAME_INTRO"),
    ("cutscene_din_imprisoned", "common/cutsceneIndices.s", "CUTSCENE_S_DIN_IMPRISONED", "seasons"),
    ("cutscene_temple_sinking", "common/cutsceneIndices.s", "CUTSCENE_S_TEMPLE_SINKING", "seasons"),
    ("cutscene_onox_taunting", "common/cutsceneIndices.s", "CUTSCENE_S_ONOX_TAUNTING", "seasons"),
    ("interac_wild_tokay_controller", "{game}/interactions.s", "INTERAC_WILD_TOKAY_CONTROLLER"),
    ("interac_golden_cave_subrosian", "{game}/interactions.s", "INTERAC_GOLDEN_CAVE_SUBROSIAN"),
    ("tilesetflag_bit_sidescroll", "common/tilesetFlags.s", "TILESETFLAG_BIT_SIDESCROLL"),
    ("tilesetflag_bit_underwater", "common/tilesetFlags.s", "TILESETFLAG_BIT_UNDERWATER"),   # Ages only
    ("menu_inventory", "common/other.s", "MENU_INVENTORY"),
    # The two vehicles Link's update does not take for a mount (link.s): A and B use items in a minecart and on the raft.
    ("specialobject_minecart", "common/specialObjects.s", "SPECIALOBJECT_MINECART"), ("specialobject_raft", "common/specialObjects.s", "SPECIALOBJECT_RAFT"),
    # The ghost's exempt list (ghost_key.c): the first object slot getFreeInteractionSlot scans (include/wram.s, not constants/).
    ("first_dynamic_interaction_index", "../include/wram.s", "FIRST_DYNAMIC_INTERACTION_INDEX"),
    # The coarse list's comparison: the bit the entry into a room sets in its flags, which no load reads.
    ("roomflag_visited", "common/roomFlags.s", "ROOMFLAG_VISITED"),
    # The call transaction: the treasures a mod may give through giveTreasure, and the rupee values of removeRupeeValue.
    ("treasure_rupees", "common/treasure.s", "TREASURE_RUPEES"), ("treasure_heart_refill", "common/treasure.s", "TREASURE_HEART_REFILL"),
    ("treasure_ring", "common/treasure.s", "TREASURE_RING"), ("treasure_ring_box", "common/treasure.s", "TREASURE_RING_BOX"),
    ("treasure_gasha_seed", "common/treasure.s", "TREASURE_GASHA_SEED"), ("treasure_seed_satchel", "common/treasure.s", "TREASURE_SEED_SATCHEL"),
    ("num_rings", "common/rings.s", "NUM_RINGS"),
]
# The global flags the load around the substitutions reads (the coarse list, ghost_key.c), each for the one game
# whose load reads it: 0xff in the other game, as for a flag the game does not have.
COARSE_GLOBAL_FLAGS = [("coarse_flag_finished_game", "ages", "GLOBALFLAG_FINISHEDGAME"),
                       ("coarse_flag_temple_lava", "seasons", "GLOBALFLAG_TEMPLE_REMAINS_FILLED_WITH_LAVA"),
                       ("coarse_flag_moblins_keep", "seasons", "GLOBALFLAG_MOBLINS_KEEP_DESTROYED"),
                       ("coarse_flag_pirate_ship", "seasons", "GLOBALFLAG_PIRATE_SHIP_DOCKED"),
                       ("coarse_flag_intro_done", "seasons", "GLOBALFLAG_INTRO_DONE")]
# The rooms whose after-load tile changes read GLOBALFLAG_INTRO_DONE (the coarse list's one scoped flag, ghost_key.c):
# the routine of applyRoomSpecificTileChangesAfterGfxLoad's jump table that checks the flag, and the rows of the
# group 0 table that name its index.  One room at most; none in a game whose routines do not read it.
def intro_done_room(disasm: Path, game: str) -> int:
    source = (disasm / "code" / game / "roomGfxChanges.s").read_text(encoding="utf-8")
    index = None
    for match in re.finditer(r"^roomTileChangesAfterLoad([0-9a-f]{2}):\n(.*?)(?=^\S)", source, flags=re.M | re.S):
        if re.search(r"ld a, ?GLOBALFLAG_INTRO_DONE\b", match.group(2)):
            if index is not None:
                raise SystemExit(f"{game}: two after-load tile changes read GLOBALFLAG_INTRO_DONE")
            index = int(match.group(1), 16)
    if index is None:
        return 0xff
    group0 = re.search(r"^@group0:\n(.*?)(?=^\S)", source, flags=re.M | re.S)
    if not group0:
        raise SystemExit(f"{game}: no @group0 table in roomGfxChanges.s")
    rooms = [int(room, 16) for room, at in re.findall(r"\.db \$([0-9a-f]{2}) \$([0-9a-f]{2})", group0.group(1)) if int(at, 16) == index]
    if len(rooms) != 1:
        raise SystemExit(f"{game}: {len(rooms)} rooms of group 0 run the after-load tile changes {index:02x}")
    return rooms[0]


# The hotbar of the item hotkeys: a label of four letters and a name for each item a button can
# hold, from the TREASURE_* constants (an inventory item has its treasure's identifier).  Not holdable: the numbered
# placeholders and the helpers of other items.  A label that would collide is written here beside the symbol it names.
ITEM_LABEL_EXCLUDED = {"TREASURE_NONE", "TREASURE_PUNCH", "TREASURE_SWITCH_HOOK_HELPER", "TREASURE_SWITCH_HOOK_CHAIN", "TREASURE_MINECART_COLLISION"}
ITEM_LABEL_OVERRIDES = {"TREASURE_BOMBCHUS": "CHUS"}   # BOMB is TREASURE_BOMBS
# The treasures a mod never names: the item hotkeys' exclusions and the lower half of the bomb flower, a helper too.
TREASURES_NOT_GIVEN = ITEM_LABEL_EXCLUDED | {"TREASURE_BOMB_FLOWER_LOWER_HALF"}


def item_labels(path: Path, game: str) -> list[tuple[str, str]]:
    """(label, name) for the identifiers 0 to NUM_INVENTORY_ITEMS - 1; two empty strings for one no button can hold."""
    constants = load_constants(path, game)
    count = constants["NUM_INVENTORY_ITEMS"]
    if count > 0x20:
        raise SystemExit(f"{game}: {count} inventory items, the tables hold 32 labels (ORACLES_GUEST_ITEM_LABELS)")
    by_value = {value: name for name, value in constants.items() if name.startswith("TREASURE_") and value < count}
    out, seen = [], {}
    for value in range(count):
        name = by_value.get(value, "")
        tail = name[len("TREASURE_"):]
        if not name or name in ITEM_LABEL_EXCLUDED or re.fullmatch(r"[0-9a-fA-F]{2}", tail):
            out.append(("", ""))
            continue
        label = ITEM_LABEL_OVERRIDES.get(name, tail.replace("_", "")[:4]).upper()
        if label in seen:
            raise SystemExit(f"{game}: {name} and {seen[label]} would both be labelled {label}; give one an entry in ITEM_LABEL_OVERRIDES")
        seen[label] = name
        out.append((label, item_name(tail)))
    return out


def item_name(tail: str) -> str:
    """TREASURE_CANE_OF_SOMARIA's tail as the launcher shows it: "Cane of Somaria", the small words in lower case."""
    words = tail.replace("_", " ").title().split(" ")
    return " ".join(word.lower() if i and word.lower() in ("of", "the") else word for i, word in enumerate(words))


# The originals draw grass on the same tiles in every group (grass_tile_last_other_groups = grass_tile_last).  And the
# shutters replaceShutterForLinkEntering (commonTileSubstitutions.s) opens under Link, in both games: it tests for the
# eight tiles from $78, $78 to $7f.
GAME_CONSTANTS = {"ages": [("puddle_tile_first", 0xf9), ("puddle_tile_last", 0xf9), ("grass_tile_first", 0xf8), ("grass_tile_last", 0xf8),
                           ("grass_tile_last_other_groups", 0xf8), ("shutter_tile_first", 0x78), ("shutter_tile_last", 0x7f)],
                  "seasons": [("puddle_tile_first", 0xfa), ("puddle_tile_last", 0xfc), ("grass_tile_first", 0xf8), ("grass_tile_last", 0xf9),
                              ("grass_tile_last_other_groups", 0xf9), ("shutter_tile_first", 0x78), ("shutter_tile_last", 0x7f)]}


def load_labels(path: Path) -> dict[str, tuple[int, int]]:
    seen: dict[str, list[tuple[int, int]]] = {}
    in_labels = False
    in_definitions = False
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("["):
            in_labels = line.strip() == "[labels]"
            in_definitions = line.strip() == "[definitions]"
            continue
        if in_labels:
            m = re.match(r"^([0-9a-f]{2}):([0-9a-f]{4}) (\S+)$", line.strip())
            if m:
                seen.setdefault(m.group(3), []).append((int(m.group(1), 16), int(m.group(2), 16)))
        elif in_definitions:
            # The fields of the special objects (w1Link, the parent items) are
            # linker definitions, not labels; the w1 objects live in WRAM bank 1.
            m = re.match(r"^[0-9a-f]{4}([0-9a-f]{4}) (w1(?:Link|ParentItem[2-5]|WeaponItem)\.\S+)$", line.strip())
            if m:
                seen.setdefault(m.group(2), []).append((1, int(m.group(1), 16)))
    labels: dict[str, tuple[int, int]] = {}
    duplicates: list[tuple[str, list[tuple[int, int]]]] = []
    for name, places in seen.items():
        if len(places) == 1:
            labels[name] = places[0]
        elif name in DUPLICATES:
            chosen = [p for p in places if (p[0] != 0 if DUPLICATES[name] == "banked" else p[0] == DUPLICATES[name])]
            if len(chosen) != 1:
                raise SystemExit(f"{path.name}: {name} is defined at {places}, none or several in bank {DUPLICATES[name]}")
            labels[name] = chosen[0]
        else:
            labels[name] = places[0]
            duplicates.append((name, places))
    _duplicates[path.name] = duplicates
    return labels


_duplicates: dict[str, list] = {}


def load_constants(path: Path, game: str) -> dict[str, int]:
    """The `.define NAME $xx` lines and the `.enum` members of a constants file, for one game (`.ifdef ROM_AGES`)."""
    values: dict[str, int] = {}
    active = [True]          # the .ifdef stack
    counter = None           # inside an enum: the next value
    def number(text: str) -> int:
        text = text.strip()
        return int(text[1:], 16) if text.startswith("$") else int(text, 0)
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.split(";", 1)[0].strip()
        if not line:
            continue
        low = line.lower()
        if low.startswith(".ifdef") or low.startswith(".ifndef"):
            name = line.split()[1]
            if name not in ("ROM_AGES", "ROM_SEASONS", "REGION_JP"):
                raise SystemExit(f"{path.name}: unknown condition {name}")
            defined = name == ("ROM_AGES" if game == "ages" else "ROM_SEASONS")   # REGION_JP: the tables are the US build's, as the symbol files are
            active.append(active[-1] and (defined if low.startswith(".ifdef") else not defined))
            continue
        if low.startswith(".else"):
            active[-1] = active[-2] and not active[-1]
            continue
        if low.startswith(".endif"):
            active.pop()
            continue
        if not active[-1]:
            continue
        m = re.match(r"^\.define\s+(\w+)\s+(\$?[0-9a-fA-Fx]+)$", line, re.I)
        if m:
            values[m.group(1)] = number(m.group(2))
            continue
        m = re.match(r"^\.enum\s+(\$?[0-9a-fA-Fx]+)", line, re.I)
        if m:
            counter = number(m.group(1))
            continue
        if low.startswith(".ende"):
            counter = None
            continue
        if counter is not None:
            m = re.match(r"^(\w+)\s+(db|dw|dsb|dsw)(?:\s+(\$?[0-9a-fA-Fx]+))?$", line, re.I)
            if m:
                values[m.group(1)] = counter
                unit = 2 if m.group(2).lower() in ("dw", "dsw") else 1
                counter += unit * (number(m.group(3)) if m.group(3) else 1)
    return values


def load_sizes(path: Path) -> dict[str, int]:
    sizes: dict[str, int] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = re.match(r"^([0-9a-f]{8}) (\S+)$", line.strip())
        if m:
            sizes[m.group(2)] = int(m.group(1), 16)
    return sizes


def load_defines(wram: Path) -> dict[str, tuple[str, int]]:
    """`.define NAME base + $x` lines of wram.s, as (base symbol, offset)."""
    defines: dict[str, tuple[str, int]] = {}
    for line in wram.read_text(encoding="utf-8", errors="replace").splitlines():
        m = re.match(r"^\.define\s+(\w+)\s+(\w+)\s*\+\s*\$([0-9a-fA-F]+)", line)
        if m:
            defines[m.group(1)] = (m.group(2), int(m.group(3), 16))
    return defines


def struct_offsets(structs: Path, name: str) -> dict[str, int]:
    """Field offsets of a WLA-DX .STRUCT: db 1, dw 2, dsb N, .db an alias of size 0, unions honoured."""
    offsets: dict[str, int] = {}
    inside = False
    offset = 0
    union_start = 0
    union_end = 0
    for raw in structs.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.split(";", 1)[0].strip()
        if not line:
            continue
        if line.startswith(".STRUCT"):
            inside = line.split()[1] == name
            offset = 0
            continue
        if not inside:
            continue
        if line.startswith(".ENDST"):
            break
        if line == ".union":
            union_start = offset
            union_end = offset
            continue
        if line == ".nextu":
            union_end = max(union_end, offset)
            offset = union_start
            continue
        if line == ".endu":
            offset = max(union_end, offset)
            continue
        parts = line.split()
        if len(parts) < 2:
            continue
        field, kind = parts[0], parts[1]
        offsets[field] = offset
        if kind == "db":
            offset += 1
        elif kind == "dw":
            offset += 2
        elif kind == "dsb":
            offset += int(parts[2].replace("$", ""), 16) if parts[2].startswith("$") else int(parts[2])
        elif kind == ".db":
            pass
        else:
            raise SystemExit(f"unknown field kind {kind} for {field}")
    return offsets


def emit_game(game: str, labels: dict[str, tuple[int, int]], defines: dict[str, tuple[str, int]], sizes: dict[str, int], out: Path, disasm: Path) -> None:
    lines = [f"/* Generated by gen_guest_tables.py from {game}.sym; do not edit. */",
             '#include "guest_tables.h"', "",
             f"const OraclesGuestTables oracles_guest_tables_{game} = {{"]
    for field, symbol in FUNCTIONS + VARIABLES:
        if symbol not in labels:
            if symbol in OPTIONAL:
                lines.append(f"    .{field} = {{ ORACLES_GUEST_ABSENT, 0 }},")
                continue
            raise SystemExit(f"{game}: missing symbol {symbol}")
        bank, addr = labels[symbol]
        lines.append(f"    .{field} = {{ 0x{bank:02x}, 0x{addr:04x} }},")
    for field, symbol in DEFINES:
        if symbol not in defines or defines[symbol][0] not in labels:
            raise SystemExit(f"{game}: cannot resolve define {symbol}")
        base, offset = defines[symbol]
        bank, addr = labels[base]
        lines.append(f"    .{field} = {{ 0x{bank:02x}, 0x{addr + offset:04x} }},")
    rooms = load_constants(disasm / "constants" / "common" / "rooms.s", game)
    for field, page, room in ROOM_FLAG_BYTES:
        if room not in rooms:
            lines.append(f"    .{field} = {{ ORACLES_GUEST_ABSENT, 0 }},")
            continue
        bank, addr = labels[page]
        lines.append(f"    .{field} = {{ 0x{bank:02x}, 0x{addr + (rooms[room] & 0xff):04x} }},")
    for field, symbol, factor in SIZES:
        if symbol not in sizes:
            raise SystemExit(f"{game}: missing size {symbol}")
        lines.append(f"    .{field} = 0x{sizes[symbol] * factor:x},")
    for field, symbol in CONSTANTS:
        if symbol not in sizes:
            raise SystemExit(f"{game}: missing constant {symbol}")
        lines.append(f"    .{field} = 0x{sizes[symbol]:02x},")
    for field, value in GAME_CONSTANTS[game]:
        lines.append(f"    .{field} = 0x{value:02x},")
    for field, file, name, *only in DISASM_CONSTANTS:
        path = disasm / "constants" / file.format(game=game)
        value = load_constants(path, game).get(name.format(s="S_" if game == "seasons" else ""), 0xff) if not only or only[0] == game else 0xff
        if value > 0xff:
            raise SystemExit(f"{game}: constant {name} does not fit a byte")
        lines.append(f"    .{field} = 0x{value:02x},")
    global_flags = load_constants(disasm / "constants" / "common" / "globalFlags.s", game)
    for field, reader, name in COARSE_GLOBAL_FLAGS:
        value = global_flags.get(name, 0xff) if reader == game else 0xff
        if reader == game and name not in global_flags:
            raise SystemExit(f"{game}: missing global flag {name}")
        lines.append(f"    .{field} = 0x{value:02x},")
    # The call transaction: a ring a mod gives is named as the disassembly names it, lower case (constants/common/rings.s).
    rings = load_constants(disasm / "constants" / "common" / "rings.s", game)
    ring_by_value = {value: name for name, value in rings.items() if "RING" in name and not name.startswith("RING_") and value < rings["NUM_RINGS"]}
    if len(ring_by_value) != rings["NUM_RINGS"] or rings["NUM_RINGS"] != 0x40:
        raise SystemExit(f"{game}: rings.s names {len(ring_by_value)} of its {rings['NUM_RINGS']} rings, the tables hold 64 (ORACLES_GUEST_RINGS)")
    lines.append("    .ring_names = { " + ", ".join(f'"{ring_by_value[i].lower()}"' for i in range(0x40)) + " },")
    # The treasures, sounds (music and effects) and global flags a mod names, as the disassembly names them, lower
    # case and without their prefix for the treasures and flags ("gasha_seed", "snd_getseed", "finishedgame"); an
    # identifier without a name is "".
    for field, file, prefix, keep_prefix, capacity in (("treasure_names", "treasure.s", "TREASURE_", False, 0x80),
                                                       ("sound_names", "music.s", ("SND_", "MUS_"), True, 0x100),
                                                       ("global_flag_names", "globalFlags.s", "GLOBALFLAG_", False, 0x80)):
        constants = load_constants(disasm / "constants" / "common" / file, game)
        named = {}
        for name, value in sorted(constants.items()):
            if not name.startswith(prefix) or value >= capacity or value in named:
                continue
            if field == "treasure_names" and (name in TREASURES_NOT_GIVEN or re.fullmatch(r"[0-9a-fA-F]{2}", name[len(prefix):])):
                continue   # a placeholder or a helper of another treasure: no name, no call
            named[value] = (name if keep_prefix else name[len(prefix):]).lower()
        lines.append(f"    .{field} = {{ " + ", ".join(f'"{named.get(i, "")}"' for i in range(capacity)) + " },")
    # The treasures' tables the call transaction reads from the ROM at the time of a call (guest_call.c), never as values
    # here: treasureCollectionBehaviourTable (three bytes a treasure), treasureObjectData (four bytes a treasure, a
    # pointer to its subids' list for some) and the end of the last of those lists, and getRupeeValue's BCD words
    # (docs/ROM_DATA_FORMATS.md, section 5.4).
    for field, symbol in (("treasure_collection_behaviours", "treasureCollectionBehaviourTable"), ("treasure_object_data", "treasureObjectData"),
                          ("rupee_values", "getRupeeValue@rupeeValues")):
        if symbol not in labels:
            raise SystemExit(f"{game}: missing symbol {symbol}")
        lines.append(f"    .{field} = {{ 0x{labels[symbol][0]:02x}, 0x{labels[symbol][1]:04x} }},")
    for field, symbol, width in (("treasure_collection_behaviour_count", "treasureCollectionBehaviourTable", 3), ("treasure_object_count", "treasureObjectData", 4)):
        size = sizes.get(f"_sizeof_{symbol}", 0)
        if not size or size % width or size // width > 0xff:
            raise SystemExit(f"{game}: {symbol} is {size} bytes, not a whole number of {width}-byte entries")
        lines.append(f"    .{field} = 0x{size // width:02x},")
    table_bank = labels["treasureObjectData"][0]
    lists = [(labels[name][1] + sizes[f"_sizeof_{name}"], labels[name][0]) for name in labels
             if re.fullmatch(r"treasureObjectData[0-9a-f]{2}", name) and f"_sizeof_{name}" in sizes]
    if len(lists) < 0x10 or any(bank != table_bank for _, bank in lists):
        raise SystemExit(f"{game}: treasureObjectData's subid lists are not {len(lists)} lists of its bank")
    lines.append(f"    .treasure_object_lists_end = {{ 0x{table_bank:02x}, 0x{max(end for end, _ in lists):04x} }},")
    # The number of rupee values (getRupeeValue, bank0.s): RUPEEVAL_COUNT, the enumeration's count.
    values = load_constants(disasm / "constants" / "common" / "rupeeValues.s", game)
    count = values["RUPEEVAL_COUNT"]
    if count > 0x20:
        raise SystemExit(f"{game}: rupeeValues.s has {count} values, more than the getRupeeValue table this reads")
    lines.append(f"    .rupee_value_count = {count},")
    # The font of the text box: character c is 16 bytes at gfx_font_start + 16 * c, in gfx_font's bank (bank0.s, multiplyABy16).
    if "gfx_font" not in labels or "gfx_font_start" not in sizes:
        raise SystemExit(f"{game}: missing gfx_font or gfx_font_start")
    lines.append(f"    .font_start = {{ 0x{labels['gfx_font'][0]:02x}, 0x{sizes['gfx_font_start']:04x} }},")
    lines.append(f"    .coarse_flag_intro_done_room = 0x{intro_done_room(disasm, game):02x},")
    labels_and_names = item_labels(disasm / "constants" / "common" / "treasure.s", game)
    lines.append("    .item_labels = { " + ", ".join(f'"{label}"' for label, _ in labels_and_names) + " },")
    lines.append("    .item_names = { " + ", ".join(f'"{name}"' for _, name in labels_and_names) + " },")
    lines += ["};", ""]
    out.write_text("\n".join(lines), encoding="utf-8")


def emit_offsets(offsets: dict[str, int], out: Path) -> None:
    lines = ["/* Generated by gen_guest_tables.py from include/structs.s (ObjectStruct); do not edit. */",
             "#ifndef ORACLES_GUEST_STRUCT_OFFSETS_H", "#define ORACLES_GUEST_STRUCT_OFFSETS_H", "",
             "/* Objects live in WRAM bank 1, $d000-$dfff: 16 slots of 256 bytes, four objects of 64 bytes each",
             "   (wram.s, RAM_1; docs/GAME_HOOKS.md, section 4.1). */",
             "#define ORACLES_OBJECTS_BANK 1", "#define ORACLES_OBJECTS_BASE 0xd000",
             "#define ORACLES_OBJECT_SIZE 0x40", "#define ORACLES_OBJECTS_PER_SLOT 4", "#define ORACLES_OBJECT_SLOTS 16", ""]
    for field in STRUCT_FIELDS:
        if field not in offsets:
            raise SystemExit(f"structs.s: missing ObjectStruct field {field}")
        macro = re.sub(r"(?<!^)(?=[A-Z])", "_", field).upper()
        lines.append(f"#define ORACLES_OBJ_{macro} 0x{offsets[field]:02x}")
    lines += ["", "#endif", ""]
    out.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--disasm", type=Path, default=find_disasm())
    args = ap.parse_args()
    defines = load_defines(args.disasm / "include" / "wram.s")
    for game in ("ages", "seasons"):
        labels = load_labels(args.disasm / f"{game}.sym")
        used = {symbol for _, symbol in FUNCTIONS + VARIABLES}
        for name, places in _duplicates.get(f"{game}.sym", []):
            if name in used:
                raise SystemExit(f"{game}: {name} is defined at {places}; add it to DUPLICATES with the bank to use")
        sizes = load_sizes(args.disasm / f"{game}.sym")
        emit_game(game, labels, defines, sizes, HERE / f"guest_tables_{game}.c", args.disasm)
        print(f"wrote guest_tables_{game}.c")
    offsets = struct_offsets(args.disasm / "include" / "structs.s", "ObjectStruct")
    emit_offsets(offsets, HERE / "guest_struct_offsets.h")
    print("wrote guest_struct_offsets.h")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
