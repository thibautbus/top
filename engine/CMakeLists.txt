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

# Native readers of the ROM's data (engine/game/data): the ghost scans the overworld's layouts.
add_library(oracles_data STATIC game/data/oracles_rom.c game/data/layout.c game/data/tileset.c game/data/oracles_tables_ages.c game/data/oracles_tables_seasons.c)
target_include_directories(oracles_data PUBLIC game/data)
oracles_set_warnings(oracles_data)
target_link_libraries(oracles_guest PUBLIC oracles_data)

