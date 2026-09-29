# ---- engine ----------------------------------------------------------------
add_library(oracles_core STATIC
    core/core.c core/state.c core/cgb_boot_rom.c rom/rom.c rom/sha1.c rom/bps.c)
target_include_directories(oracles_core PUBLIC core rom)
target_compile_definitions(oracles_core PRIVATE ORACLES_SAMEBOY_VERSION="${ORACLES_SAMEBOY_VERSION}")
target_link_libraries(oracles_core PUBLIC sameboy)
oracles_set_warnings(oracles_core)

add_library(oracles_host STATIC host/host.c)
target_include_directories(oracles_host PUBLIC host)
target_link_libraries(oracles_host PUBLIC oracles_core)
oracles_set_warnings(oracles_host)

# Guest bus, hooks and register journal; the tables are generated per game.
add_library(oracles_guest STATIC game/guest/guest.c game/guest/guest_transaction.c game/guest/guest_call.c game/guest/guest_inventory.c game/guest/guest_item_buttons.c game/guest/guest_fingerprint.c game/guest/guest_tables_ages.c game/guest/guest_tables_seasons.c game/profiles/profile.c game/profiles/moonrise-regalia/tables.c game/profiles/temple-of-seasons/tables.c game/profiles/gifts-of-kinomi/tables.c)
target_include_directories(oracles_guest PUBLIC game/guest game/profiles game/data)
target_link_libraries(oracles_guest PUBLIC oracles_core)
oracles_set_warnings(oracles_guest)

# Software PPU and the per-frame comparison with the core.
add_library(oracles_render STATIC render/ppu.c render/frame_check.c)
target_include_directories(oracles_render PUBLIC render)
target_link_libraries(oracles_render PUBLIC oracles_guest)
oracles_set_warnings(oracles_render)

# Ghost instance: a second core loaded by savestate in which the game loads a
# neighbouring room; runs synchronously or in a worker thread.
find_package(Threads REQUIRED)
# Native readers of the ROM's data (engine/game/data): the ghost scans the overworld's layouts.
add_library(oracles_data STATIC game/data/oracles_rom.c game/data/layout.c game/data/tileset.c game/data/oracles_tables_ages.c game/data/oracles_tables_seasons.c)
target_include_directories(oracles_data PUBLIC game/data)
oracles_set_warnings(oracles_data)
target_link_libraries(oracles_guest PUBLIC oracles_data)

# The objects of a room and the number each received at its creation:
# read from any instance's bus, the live one or a ghost's.
add_library(oracles_neighbours STATIC neighbours/objects.c neighbours/sprites.c neighbours/animation.c neighbours/object_sprites.c)
target_include_directories(oracles_neighbours PUBLIC neighbours)
target_link_libraries(oracles_neighbours PUBLIC oracles_guest)
oracles_set_warnings(oracles_neighbours)

add_library(oracles_ghost STATIC ghost/ghost.c ghost/ghost_key.c ghost/ghost_data.c)
target_include_directories(oracles_ghost PUBLIC ghost)
target_link_libraries(oracles_ghost PUBLIC oracles_guest oracles_render oracles_data oracles_neighbours Threads::Threads)
oracles_set_warnings(oracles_ghost)

# Enhanced presentation: the smooth camera reducer fed from the guest bus.
add_library(oracles_enhanced STATIC enhanced/camera.c enhanced/compositor.c enhanced/view.c enhanced/view_mode.c enhanced/view_black.c enhanced/view_cache.c enhanced/view_map.c enhanced/view_ghost.c enhanced/view_schedule.c enhanced/view_render.c enhanced/view_overlay.c enhanced/view_objects.c enhanced/view_textbox.c enhanced/view_hotbar.c enhanced/view_scroll.c enhanced/hotbar.c)
target_include_directories(oracles_enhanced PUBLIC enhanced)
target_link_libraries(oracles_enhanced PUBLIC oracles_guest oracles_core oracles_ghost)
oracles_set_warnings(oracles_enhanced)

add_library(oracles_gameplay STATIC gameplay/room_transition.c gameplay/item_hotkeys.c gameplay/item_hotkeys_policy.c)
target_include_directories(oracles_gameplay PUBLIC gameplay)
target_link_libraries(oracles_gameplay PUBLIC oracles_guest)
oracles_set_warnings(oracles_gameplay)
