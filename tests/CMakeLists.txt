# ---- tests without ROM --------------------------------------------------------
if(BUILD_TESTING)
    set(ORACLES_ENHANCED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../engine/enhanced)

    add_executable(oracles-test-rom test_rom.c)
    target_link_libraries(oracles-test-rom PRIVATE oracles_core)
    oracles_set_warnings(oracles-test-rom)
    add_test(NAME oracles-rom COMMAND oracles-test-rom)
    add_executable(oracles-test-bps test_bps.c)
    target_link_libraries(oracles-test-bps PRIVATE oracles_core)
    oracles_set_warnings(oracles-test-bps)
    add_test(NAME oracles-bps COMMAND oracles-test-bps)

    add_executable(oracles-test-compat test_compat.c)
    target_link_libraries(oracles-test-compat PRIVATE oracles_guest)
    oracles_set_warnings(oracles-test-compat)
    add_test(NAME oracles-compat COMMAND oracles-test-compat)

    add_executable(oracles-test-mods test_mods.c ${ORACLES_SESSION_DIR}/mod_folder.c)
    target_include_directories(oracles-test-mods PRIVATE ${ORACLES_SESSION_DIR})
    target_link_libraries(oracles-test-mods PRIVATE oracles_mods)
    oracles_set_warnings(oracles-test-mods)
    add_test(NAME oracles-mods COMMAND oracles-test-mods ${CMAKE_CURRENT_BINARY_DIR}/test_mods.tmp)

    add_executable(oracles-test-state test_state.c)
    target_link_libraries(oracles-test-state PRIVATE oracles_core)
    oracles_set_warnings(oracles-test-state)
    add_test(NAME oracles-state COMMAND oracles-test-state ${CMAKE_CURRENT_BINARY_DIR}/test_state.tmp)

    add_executable(oracles-test-guest test_guest.c)
    target_link_libraries(oracles-test-guest PRIVATE oracles_guest)
    oracles_set_warnings(oracles-test-guest)
    add_test(NAME oracles-guest COMMAND oracles-test-guest)

    add_executable(oracles-test-guest-threads test_guest_threads.c)
    target_link_libraries(oracles-test-guest-threads PRIVATE oracles_guest)
    oracles_set_warnings(oracles-test-guest-threads)
    add_test(NAME oracles-guest-threads COMMAND oracles-test-guest-threads)

    add_executable(oracles-test-render test_render.c)
    target_link_libraries(oracles-test-render PRIVATE oracles_render)
    oracles_set_warnings(oracles-test-render)
    add_test(NAME oracles-render COMMAND oracles-test-render)

    add_executable(oracles-test-ghost test_ghost.c)
    target_link_libraries(oracles-test-ghost PRIVATE oracles_ghost)
    oracles_set_warnings(oracles-test-ghost)
    add_test(NAME oracles-ghost COMMAND oracles-test-ghost)

    add_executable(oracles-test-room-transition test_room_transition.c)
    target_link_libraries(oracles-test-room-transition PRIVATE oracles_gameplay)
    oracles_set_warnings(oracles-test-room-transition)
    add_test(NAME oracles-room-transition COMMAND oracles-test-room-transition)

    add_executable(oracles-test-item-hotkeys test_item_hotkeys.c)
    target_link_libraries(oracles-test-item-hotkeys PRIVATE oracles_gameplay)
    oracles_set_warnings(oracles-test-item-hotkeys)
    add_test(NAME oracles-item-hotkeys COMMAND oracles-test-item-hotkeys)

    add_executable(oracles-test-item-hotkeys-policy test_item_hotkeys_policy.c)
    target_link_libraries(oracles-test-item-hotkeys-policy PRIVATE oracles_gameplay)
    oracles_set_warnings(oracles-test-item-hotkeys-policy)
    add_test(NAME oracles-item-hotkeys-policy COMMAND oracles-test-item-hotkeys-policy)

    add_executable(oracles-test-hotkey-lines test_hotkey_lines.c ${ORACLES_SESSION_DIR}/hotkey_lines.c)
    target_include_directories(oracles-test-hotkey-lines PRIVATE ${ORACLES_SESSION_DIR})
    target_link_libraries(oracles-test-hotkey-lines PRIVATE oracles_gameplay)
    oracles_set_warnings(oracles-test-hotkey-lines)
    add_test(NAME oracles-hotkey-lines COMMAND oracles-test-hotkey-lines)

    add_executable(oracles-test-hotbar test_hotbar.c ${ORACLES_ENHANCED_DIR}/hotbar.c)
    target_include_directories(oracles-test-hotbar PRIVATE ${ORACLES_ENHANCED_DIR})
    oracles_set_warnings(oracles-test-hotbar)
    add_test(NAME oracles-hotbar COMMAND oracles-test-hotbar)

    add_executable(oracles-test-camera test_camera.c)
    target_link_libraries(oracles-test-camera PRIVATE oracles_enhanced)
    oracles_set_warnings(oracles-test-camera)
    add_test(NAME oracles-camera COMMAND oracles-test-camera)

    add_executable(oracles-test-zoom-out test_zoom_out.c)
    target_link_libraries(oracles-test-zoom-out PRIVATE oracles_enhanced)
    oracles_set_warnings(oracles-test-zoom-out)
    add_test(NAME oracles-zoom-out COMMAND oracles-test-zoom-out)
    add_executable(oracles-test-scroll-animation test_scroll_animation.c)
    target_link_libraries(oracles-test-scroll-animation PRIVATE oracles_enhanced)
    oracles_set_warnings(oracles-test-scroll-animation)
    add_test(NAME oracles-scroll-animation COMMAND oracles-test-scroll-animation)
    add_executable(oracles-test-routed-order test_routed_order.c)
    target_link_libraries(oracles-test-routed-order PRIVATE oracles_enhanced)
    oracles_set_warnings(oracles-test-routed-order)
    add_test(NAME oracles-routed-order COMMAND oracles-test-routed-order)
    add_executable(oracles-test-cache-key test_cache_key.c)
    target_link_libraries(oracles-test-cache-key PRIVATE oracles_enhanced)
    target_include_directories(oracles-test-cache-key PRIVATE ${ORACLES_ENHANCED_DIR})
    oracles_set_warnings(oracles-test-cache-key)
    add_test(NAME oracles-cache-key COMMAND oracles-test-cache-key)

    # The routes replayed through the harness (tools/check_routes.py):
    # skipped unless ORACLES_ROM_DIR names a directory with the ROMs.
    find_package(Python3 COMPONENTS Interpreter)
    # A staged copy of the sources alone (build_windows.sh from a WSL path) has no tools/: no route test there.
    if(Python3_Interpreter_FOUND AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_routes.py)
        add_test(NAME oracles-routes COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_routes.py --harness $<TARGET_FILE:oracles-harness>)
        set_tests_properties(oracles-routes PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 3600)
    endif()
    # Each fan game's table against its manifest and the guest table's fields: a field the guest gains fails here,
    # before it takes the zero of C in a checked-in table (engine/game/profiles/gen_tables.py).
    if(Python3_Interpreter_FOUND AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../engine/game/profiles/moonrise-regalia/manifest.json)
        add_test(NAME oracles-fan-game-tables COMMAND ${Python3_EXECUTABLE} -m unittest test_gen_tables
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../engine/game/profiles)
    endif()
    add_executable(oracles-test-state-refusal test_state_refusal.c ${ORACLES_SESSION_DIR}/state_refusal.c)
    target_include_directories(oracles-test-state-refusal PRIVATE ${ORACLES_SESSION_DIR})
    oracles_set_warnings(oracles-test-state-refusal)
    add_test(NAME oracles-state-refusal COMMAND oracles-test-state-refusal)

    add_executable(oracles-test-route test_route.c ${ORACLES_SESSION_DIR}/route.c)
    target_include_directories(oracles-test-route PRIVATE ${ORACLES_SESSION_DIR})
    oracles_set_warnings(oracles-test-route)
    add_test(NAME oracles-route COMMAND oracles-test-route ${CMAKE_CURRENT_BINARY_DIR}/test_route.tmp)

    add_executable(oracles-test-host test_host.c ${ORACLES_HOST_DIR}/null_backend.c)
    target_include_directories(oracles-test-host PRIVATE ${ORACLES_HOST_DIR})
    target_link_libraries(oracles-test-host PRIVATE oracles_host)
    oracles_set_warnings(oracles-test-host)
    add_test(NAME oracles-host COMMAND oracles-test-host)
endif()
