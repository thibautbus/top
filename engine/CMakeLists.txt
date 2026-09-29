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

# ---- Lua, vendored and unmodified (third_party/lua/VERSION.md) --------------
# The library alone, without the interpreter and the compiler.  The string
# hash seed is fixed: lstate.c otherwise mixes the clock and addresses into it,
# and a mod's tables would be laid out differently from run to run.
set(LUA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/lua/src)
add_library(oracles_lua STATIC
    ${LUA_DIR}/lapi.c ${LUA_DIR}/lcode.c ${LUA_DIR}/lctype.c ${LUA_DIR}/ldebug.c ${LUA_DIR}/ldo.c ${LUA_DIR}/ldump.c
    ${LUA_DIR}/lfunc.c ${LUA_DIR}/lgc.c ${LUA_DIR}/llex.c ${LUA_DIR}/lmem.c ${LUA_DIR}/lobject.c ${LUA_DIR}/lopcodes.c
    ${LUA_DIR}/lparser.c ${LUA_DIR}/lstate.c ${LUA_DIR}/lstring.c ${LUA_DIR}/ltable.c ${LUA_DIR}/ltm.c ${LUA_DIR}/lundump.c
    ${LUA_DIR}/lvm.c ${LUA_DIR}/lzio.c
    ${LUA_DIR}/lauxlib.c ${LUA_DIR}/lbaselib.c ${LUA_DIR}/lcorolib.c ${LUA_DIR}/lmathlib.c ${LUA_DIR}/lstrlib.c
    ${LUA_DIR}/ltablib.c ${LUA_DIR}/lutf8lib.c)
target_include_directories(oracles_lua SYSTEM PUBLIC ${LUA_DIR})
if(MSVC)
    set_source_files_properties(${LUA_DIR}/lstate.c PROPERTIES COMPILE_OPTIONS "/FI${CMAKE_CURRENT_SOURCE_DIR}/mods/lua_seed.h")
else()
    set_source_files_properties(${LUA_DIR}/lstate.c PROPERTIES COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/mods/lua_seed.h")
endif()
if(NOT MSVC)
    target_compile_options(oracles_lua PRIVATE -w)
endif()
if(NOT WIN32)
    target_link_libraries(oracles_lua PUBLIC m)
endif()

# Mods in Lua run during the game.  The prelude, the
# engine's half of a mod's runtime, is Lua embedded as a C string.
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/mods/prelude.lua ORACLES_MOD_PRELUDE_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," ORACLES_MOD_PRELUDE_BYTES "${ORACLES_MOD_PRELUDE_HEX}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/mod_prelude.c
    "/* Generated from engine/mods/prelude.lua at configure; do not edit. */\nconst unsigned char oracles_mod_prelude[] = {${ORACLES_MOD_PRELUDE_BYTES}0};\n")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mods/prelude.lua)
add_library(oracles_mods STATIC mods/mod.c mods/mod_lua.c mods/mod_game.c mods/mod_text.c mods/mod_draw.c mods/mod_compose.c mods/mod_warps.c mods/mod_set.c mods/mod_store.c ${CMAKE_CURRENT_BINARY_DIR}/mod_prelude.c)
target_include_directories(oracles_mods PUBLIC mods)
target_link_libraries(oracles_mods PUBLIC oracles_guest oracles_data oracles_lua)
oracles_set_warnings(oracles_mods)

add_library(oracles_gameplay STATIC gameplay/room_transition.c gameplay/item_hotkeys.c gameplay/item_hotkeys_policy.c)
target_include_directories(oracles_gameplay PUBLIC gameplay)
target_link_libraries(oracles_gameplay PUBLIC oracles_guest)
oracles_set_warnings(oracles_gameplay)
