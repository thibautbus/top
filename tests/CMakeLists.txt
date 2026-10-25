# ---- tests without ROM --------------------------------------------------------
if(BUILD_TESTING)
    set(ORACLES_LAUNCHER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../launcher)
    set(ORACLES_ENHANCED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../engine/enhanced)

    add_executable(oracles-test-rom test_rom.c)
    target_link_libraries(oracles-test-rom PRIVATE oracles_core)
    oracles_set_warnings(oracles-test-rom)
    add_test(NAME oracles-rom COMMAND oracles-test-rom)
    # mGBA's core boots the engine's CGB boot ROM (third_party/mgba/VERSION.md); linked with the engine's core, so
    # that SameBoy and mGBA are seen to link together.
    add_executable(oracles-test-mgba-boot test_mgba_boot.c)
    target_link_libraries(oracles-test-mgba-boot PRIVATE oracles_core mgba)
    oracles_set_warnings(oracles-test-mgba-boot)
    add_test(NAME oracles-mgba-boot COMMAND oracles-test-mgba-boot)
    add_executable(oracles-test-colour test_colour.c)
    target_link_libraries(oracles-test-colour PRIVATE oracles_core)
    oracles_set_warnings(oracles-test-colour)
    add_test(NAME oracles-colour COMMAND oracles-test-colour)
    add_executable(oracles-test-bps test_bps.c)
    target_link_libraries(oracles-test-bps PRIVATE oracles_core)
    oracles_set_warnings(oracles-test-bps)
    add_test(NAME oracles-bps COMMAND oracles-test-bps)
    add_executable(oracles-test-zip test_zip.c)
    target_link_libraries(oracles-test-zip PRIVATE oracles_core)
    oracles_set_warnings(oracles-test-zip)
    add_test(NAME oracles-zip COMMAND oracles-test-zip ${CMAKE_CURRENT_BINARY_DIR}/test_zip.tmp)
    add_executable(oracles-test-save test_save.c)
    target_link_libraries(oracles-test-save PRIVATE oracles_core)
    oracles_set_warnings(oracles-test-save)
    add_test(NAME oracles-save COMMAND oracles-test-save)

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

    add_executable(oracles-test-swim-hold test_swim_hold.c)
    target_link_libraries(oracles-test-swim-hold PRIVATE oracles_guest)
    oracles_set_warnings(oracles-test-swim-hold)
    add_test(NAME oracles-swim-hold COMMAND oracles-test-swim-hold)

    add_executable(oracles-test-text-instant test_text_instant.c)
    target_link_libraries(oracles-test-text-instant PRIVATE oracles_guest)
    oracles_set_warnings(oracles-test-text-instant)
    add_test(NAME oracles-text-instant COMMAND oracles-test-text-instant)

    add_executable(oracles-test-cheats test_cheats.c)
    target_link_libraries(oracles-test-cheats PRIVATE oracles_guest)
    oracles_set_warnings(oracles-test-cheats)
    add_test(NAME oracles-cheats COMMAND oracles-test-cheats)

    add_executable(oracles-test-compact-text test_compact_text.c)
    target_link_libraries(oracles-test-compact-text PRIVATE oracles_guest)
    oracles_set_warnings(oracles-test-compact-text)
    add_test(NAME oracles-compact-text COMMAND oracles-test-compact-text)

    add_executable(oracles-test-language test_language.c)
    target_include_directories(oracles-test-language PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/language)
    target_link_libraries(oracles-test-language PRIVATE oracles_language)
    oracles_set_warnings(oracles-test-language)
    add_test(NAME oracles-language COMMAND oracles-test-language ${CMAKE_CURRENT_BINARY_DIR})
    # The texts laid out again for the compact text, on texts built by the test.
    add_executable(oracles-test-language-wrap test_language_wrap.c)
    target_include_directories(oracles-test-language-wrap PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/language)
    target_link_libraries(oracles-test-language-wrap PRIVATE oracles_language)
    oracles_set_warnings(oracles-test-language-wrap)
    add_test(NAME oracles-language-wrap COMMAND oracles-test-language-wrap)
    # The same on the user's ROMs: skipped unless ORACLES_ROM_DIR names a directory with them.
    add_executable(oracles-test-language-roms test_language_roms.c)
    target_include_directories(oracles-test-language-roms PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/language)
    target_link_libraries(oracles-test-language-roms PRIVATE oracles_language)
    oracles_set_warnings(oracles-test-language-roms)
    add_test(NAME oracles-language-roms COMMAND oracles-test-language-roms ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties(oracles-language-roms PROPERTIES SKIP_RETURN_CODE 77)
    # A session's language: its pack read, extracted again or refused, and written, on the same ROMs.
    add_executable(oracles-test-language-session test_language_session.c ${ORACLES_SESSION_DIR}/language_session.c)
    target_include_directories(oracles-test-language-session PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/language ${ORACLES_SESSION_DIR})
    target_link_libraries(oracles-test-language-session PRIVATE oracles_language)
    oracles_set_warnings(oracles-test-language-session)
    add_test(NAME oracles-language-session COMMAND oracles-test-language-session ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties(oracles-language-session PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(oracles-test-guest-threads test_guest_threads.c)
    target_link_libraries(oracles-test-guest-threads PRIVATE oracles_guest)
    oracles_set_warnings(oracles-test-guest-threads)
    add_test(NAME oracles-guest-threads COMMAND oracles-test-guest-threads)

    add_executable(oracles-test-text-box test_text_box.c)
    target_link_libraries(oracles-test-text-box PRIVATE oracles_enhanced oracles_render)
    oracles_set_warnings(oracles-test-text-box)
    add_test(NAME oracles-text-box COMMAND oracles-test-text-box)

    add_executable(oracles-test-render test_render.c)
    target_link_libraries(oracles-test-render PRIVATE oracles_render)
    oracles_set_warnings(oracles-test-render)
    add_test(NAME oracles-render COMMAND oracles-test-render)
    add_executable(oracles-test-ppu-tiles test_ppu_tiles.c)
    target_link_libraries(oracles-test-ppu-tiles PRIVATE oracles_render)
    oracles_set_warnings(oracles-test-ppu-tiles)
    add_test(NAME oracles-ppu-tiles COMMAND oracles-test-ppu-tiles)

    add_executable(oracles-test-ghost test_ghost.c)
    target_link_libraries(oracles-test-ghost PRIVATE oracles_ghost)
    oracles_set_warnings(oracles-test-ghost)
    add_test(NAME oracles-ghost COMMAND oracles-test-ghost)

    add_executable(oracles-test-room-transition test_room_transition.c)
    target_link_libraries(oracles-test-room-transition PRIVATE oracles_gameplay)
    oracles_set_warnings(oracles-test-room-transition)
    add_test(NAME oracles-room-transition COMMAND oracles-test-room-transition)
    # The policy built alone, the guest's few calls answered by the test.
    set(ORACLES_ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../engine)
    add_executable(oracles-test-room-transition-policy test_room_transition_policy.c ${ORACLES_ENGINE_DIR}/gameplay/room_transition.c
                   ${ORACLES_ENGINE_DIR}/gameplay/room_predict.c)
    target_include_directories(oracles-test-room-transition-policy PRIVATE ${ORACLES_ENGINE_DIR}/gameplay ${ORACLES_ENGINE_DIR}/game/guest
                               ${ORACLES_ENGINE_DIR}/game/profiles ${ORACLES_ENGINE_DIR}/game/data ${ORACLES_ENGINE_DIR}/core ${ORACLES_ENGINE_DIR}/rom)
    target_link_libraries(oracles-test-room-transition-policy PRIVATE oracles_data)
    oracles_set_warnings(oracles-test-room-transition-policy)
    add_test(NAME oracles-room-transition-policy COMMAND oracles-test-room-transition-policy)

    add_executable(oracles-test-room-predict test_room_predict.c)
    target_link_libraries(oracles-test-room-predict PRIVATE oracles_gameplay)
    oracles_set_warnings(oracles-test-room-predict)
    add_test(NAME oracles-room-predict COMMAND oracles-test-room-predict)

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

    add_executable(oracles-test-touch-controls test_touch_controls.c ${ORACLES_LAUNCHER_DIR}/touch_controls.c)
    target_include_directories(oracles-test-touch-controls PRIVATE ${ORACLES_LAUNCHER_DIR})
    target_link_libraries(oracles-test-touch-controls PRIVATE oracles_core)
    oracles_set_warnings(oracles-test-touch-controls)
    add_test(NAME oracles-touch-controls COMMAND oracles-test-touch-controls)

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
    add_executable(oracles-test-hud-wide test_hud_wide.c)
    target_link_libraries(oracles-test-hud-wide PRIVATE oracles_enhanced)
    oracles_set_warnings(oracles-test-hud-wide)
    add_test(NAME oracles-hud-wide COMMAND oracles-test-hud-wide)
    add_executable(oracles-test-far-fitted test_far_fitted.c)
    target_link_libraries(oracles-test-far-fitted PRIVATE oracles_enhanced)
    oracles_set_warnings(oracles-test-far-fitted)
    add_test(NAME oracles-far-fitted COMMAND oracles-test-far-fitted)
    add_executable(oracles-test-scroll-animation test_scroll_animation.c)
    target_link_libraries(oracles-test-scroll-animation PRIVATE oracles_enhanced)
    oracles_set_warnings(oracles-test-scroll-animation)
    add_test(NAME oracles-scroll-animation COMMAND oracles-test-scroll-animation)
    add_executable(oracles-test-object-animation test_object_animation.c)
    target_link_libraries(oracles-test-object-animation PRIVATE oracles_neighbours)
    oracles_set_warnings(oracles-test-object-animation)
    add_test(NAME oracles-object-animation COMMAND oracles-test-object-animation)
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
        # The same routes on mGBA: the rows of both cores and its own.
        add_test(NAME oracles-routes-mgba COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_routes.py --harness $<TARGET_FILE:oracles-harness> --core mgba)
        set_tests_properties(oracles-routes-mgba PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 3600)
        # The rules the script holds a row to, without the ROMs.
        add_test(NAME oracles-check-routes-rules COMMAND ${Python3_EXECUTABLE} -B -m unittest test_check_routes
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../tools)
    endif()
    # Each fan game's table against its manifest and the guest table's fields: a field the guest gains fails here,
    # before it takes the zero of C in a checked-in table (engine/game/profiles/gen_tables.py).
    if(Python3_Interpreter_FOUND AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../engine/game/profiles/moonrise-regalia/manifest.json)
        add_test(NAME oracles-fan-game-tables COMMAND ${Python3_EXECUTABLE} -m unittest test_gen_tables
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../engine/game/profiles)
    endif()

    # The launcher's layout reference, the measures the layout tests compare with.
    set(ORACLES_LAYOUT_REFERENCE ${CMAKE_CURRENT_SOURCE_DIR}/launcher_layout_reference.json)
    add_executable(oracles-test-launcher-text test_launcher_text.c layout_reference.c)
    target_link_libraries(oracles-test-launcher-text PRIVATE oracles_ui)
    oracles_set_warnings(oracles-test-launcher-text)
    add_test(NAME oracles-launcher-text COMMAND oracles-test-launcher-text ${ORACLES_LAYOUT_REFERENCE})

    add_executable(oracles-test-launcher-layout test_launcher_layout.c layout_reference.c)
    target_link_libraries(oracles-test-launcher-layout PRIVATE oracles_ui)
    oracles_set_warnings(oracles-test-launcher-layout)
    add_test(NAME oracles-launcher-layout COMMAND oracles-test-launcher-layout ${ORACLES_LAYOUT_REFERENCE})

    add_executable(oracles-test-launcher-page test_launcher_page.c layout_reference.c)
    target_link_libraries(oracles-test-launcher-page PRIVATE oracles_ui)
    oracles_set_warnings(oracles-test-launcher-page)
    add_test(NAME oracles-launcher-page COMMAND oracles-test-launcher-page ${ORACLES_LAYOUT_REFERENCE})

    add_executable(oracles-test-launcher-save test_launcher_save.c layout_reference.c)
    target_link_libraries(oracles-test-launcher-save PRIVATE oracles_ui)
    oracles_set_warnings(oracles-test-launcher-save)
    add_test(NAME oracles-launcher-save COMMAND oracles-test-launcher-save ${ORACLES_LAYOUT_REFERENCE})

    add_executable(oracles-test-launcher-text-page test_launcher_text_page.c layout_reference.c)
    target_include_directories(oracles-test-launcher-text-page PRIVATE ${ORACLES_ENHANCED_DIR})
    target_link_libraries(oracles-test-launcher-text-page PRIVATE oracles_ui)
    oracles_set_warnings(oracles-test-launcher-text-page)
    add_test(NAME oracles-launcher-text-page COMMAND oracles-test-launcher-text-page ${ORACLES_LAYOUT_REFERENCE})

    add_executable(oracles-test-launcher-mods test_launcher_mods.c layout_reference.c)
    target_link_libraries(oracles-test-launcher-mods PRIVATE oracles_ui)
    oracles_set_warnings(oracles-test-launcher-mods)
    add_test(NAME oracles-launcher-mods COMMAND oracles-test-launcher-mods ${ORACLES_LAYOUT_REFERENCE})

    add_executable(oracles-test-launcher-picture test_launcher_picture.c layout_reference.c)
    target_include_directories(oracles-test-launcher-picture PRIVATE ${ORACLES_ENHANCED_DIR})
    target_link_libraries(oracles-test-launcher-picture PRIVATE oracles_ui)
    oracles_set_warnings(oracles-test-launcher-picture)
    add_test(NAME oracles-launcher-picture COMMAND oracles-test-launcher-picture ${ORACLES_LAYOUT_REFERENCE})
    add_executable(oracles-test-launcher-cheats test_launcher_cheats.c layout_reference.c)
    target_link_libraries(oracles-test-launcher-cheats PRIVATE oracles_ui)
    oracles_set_warnings(oracles-test-launcher-cheats)
    add_test(NAME oracles-launcher-cheats COMMAND oracles-test-launcher-cheats ${ORACLES_LAYOUT_REFERENCE})

    add_executable(oracles-test-launcher-controls test_launcher_controls.c layout_reference.c)
    target_link_libraries(oracles-test-launcher-controls PRIVATE oracles_ui)
    oracles_set_warnings(oracles-test-launcher-controls)
    add_test(NAME oracles-launcher-controls COMMAND oracles-test-launcher-controls ${ORACLES_LAYOUT_REFERENCE})

    add_executable(oracles-test-launcher-devices test_launcher_devices.c layout_reference.c)
    target_link_libraries(oracles-test-launcher-devices PRIVATE oracles_ui)
    oracles_set_warnings(oracles-test-launcher-devices)
    add_test(NAME oracles-launcher-devices COMMAND oracles-test-launcher-devices ${ORACLES_LAYOUT_REFERENCE})

    add_executable(oracles-test-launcher-nav test_launcher_nav.c)
    target_link_libraries(oracles-test-launcher-nav PRIVATE oracles_ui)
    oracles_set_warnings(oracles-test-launcher-nav)
    add_test(NAME oracles-launcher-nav COMMAND oracles-test-launcher-nav)

    add_executable(oracles-test-launcher-icon test_launcher_icon.c)
    target_link_libraries(oracles-test-launcher-icon PRIVATE oracles_ui)
    oracles_set_warnings(oracles-test-launcher-icon)
    add_test(NAME oracles-launcher-icon COMMAND oracles-test-launcher-icon)

    # The launcher's own tests need its SDL backend (the settings' key names) or its binary.
    if(TARGET oracles)
        add_executable(oracles-test-launcher-settings test_launcher_settings.c ${ORACLES_LAUNCHER_DIR}/settings.c ${ORACLES_LAUNCHER_DIR}/home_choices.c ${ORACLES_LAUNCHER_DIR}/session_options.c ${ORACLES_SESSION_DIR}/hotkey_lines.c ${ORACLES_LAUNCHER_DIR}/sdl_backend.c ${ORACLES_LAUNCHER_DIR}/sdl_window.c ${ORACLES_LAUNCHER_DIR}/sdl_input.c ${ORACLES_LAUNCHER_DIR}/sdl_present.c ${ORACLES_LAUNCHER_DIR}/safe_area.c ${ORACLES_LAUNCHER_DIR}/touch_controls.c ${ORACLES_LAUNCHER_DIR}/touch_sdl.c)
        target_include_directories(oracles-test-launcher-settings PRIVATE ${ORACLES_LAUNCHER_DIR} ${ORACLES_SESSION_DIR} ${ORACLES_ENHANCED_DIR})
        target_link_libraries(oracles-test-launcher-settings PRIVATE oracles_host oracles_guest oracles_gameplay oracles_ui ${ORACLES_SDL_TARGET})
        oracles_set_warnings(oracles-test-launcher-settings)
        add_test(NAME oracles-launcher-settings COMMAND oracles-test-launcher-settings ${CMAKE_CURRENT_BINARY_DIR}/test_launcher_settings.tmp)

        add_executable(oracles-test-launcher-home-files test_launcher_home_files.c ${ORACLES_LAUNCHER_DIR}/home_files.c)
        target_include_directories(oracles-test-launcher-home-files PRIVATE ${ORACLES_LAUNCHER_DIR} ${ORACLES_SESSION_DIR} ${ORACLES_ENHANCED_DIR})
        target_link_libraries(oracles-test-launcher-home-files PRIVATE oracles_host oracles_gameplay oracles_ui oracles_core)
        oracles_set_warnings(oracles-test-launcher-home-files)
        add_test(NAME oracles-launcher-home-files COMMAND oracles-test-launcher-home-files ${CMAKE_CURRENT_BINARY_DIR})

        add_executable(oracles-test-launcher-save-transfer test_launcher_save_transfer.c ${ORACLES_LAUNCHER_DIR}/save_transfer.c ${ORACLES_LAUNCHER_DIR}/file_dialog.c)
        target_include_directories(oracles-test-launcher-save-transfer PRIVATE ${ORACLES_LAUNCHER_DIR})
        target_link_libraries(oracles-test-launcher-save-transfer PRIVATE oracles_core ${ORACLES_SDL_TARGET})
        oracles_set_warnings(oracles-test-launcher-save-transfer)
        add_test(NAME oracles-launcher-save-transfer COMMAND oracles-test-launcher-save-transfer ${CMAKE_CURRENT_BINARY_DIR}/test_launcher_save_transfer.tmp)

        # The update check on GitHub's answer for v1.1.0 (update_release.json) and variants of it, and on its thread from a
        # file: no test asks the network.
        set(ORACLES_UPDATE_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/update_release.json)
        add_executable(oracles-test-update-check test_update_check.c)
        target_link_libraries(oracles-test-update-check PRIVATE oracles_update)
        oracles_set_warnings(oracles-test-update-check)
        add_test(NAME oracles-update-check COMMAND oracles-test-update-check ${ORACLES_UPDATE_RELEASE} ${CMAKE_CURRENT_BINARY_DIR}/test_update_check.tmp)
        # The launcher's --update-check-url, for a capture: a release newer than any (v99.0.0) said on the standard error;
        # update_check=off asks nothing, the file given or not; and refused with --rom, whose game asks nothing.
        file(READ ${ORACLES_UPDATE_RELEASE} update_release)
        string(REPLACE "\"tag_name\":\"v1.1.0\"" "\"tag_name\":\"v99.0.0\"" update_release "${update_release}")
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/update_release_newer.json "${update_release}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ORACLES_UPDATE_RELEASE})
        set(update_off_home ${CMAKE_CURRENT_BINARY_DIR}/launcher-test-update-off)
        file(MAKE_DIRECTORY ${update_off_home})
        file(WRITE ${update_off_home}/settings.txt "update_check=off\n")
        add_test(NAME oracles-launcher-update-check
            COMMAND oracles --launcher-screenshot ${CMAKE_CURRENT_BINARY_DIR}/launcher_update.ppm --launcher-size 640x480
                    --update-check-url ${CMAKE_CURRENT_BINARY_DIR}/update_release_newer.json)
        set_tests_properties(oracles-launcher-update-check PROPERTIES PASS_REGULAR_EXPRESSION "update check: version 99\\.0\\.0 available"
            FAIL_REGULAR_EXPRESSION "https://|update_release_newer" TIMEOUT 30 ENVIRONMENT "ORACLES_SETTINGS_DIR=${CMAKE_CURRENT_BINARY_DIR}/launcher-test-home")
        # The home screen shows it above the version at a handheld's 640x480, highlighted by Up from Start game past the
        # version number, after a controller's button (the layout reference's frame 24d).
        add_test(NAME oracles-launcher-screenshot-update-draw
            COMMAND oracles --launcher-screenshot ${CMAKE_CURRENT_BINARY_DIR}/launcher_update_open.ppm --launcher-size 640x480 --launcher-input pad,up,up
                    --update-check-url ${CMAKE_CURRENT_BINARY_DIR}/update_release_newer.json)
        set_tests_properties(oracles-launcher-screenshot-update-draw PROPERTIES FIXTURES_SETUP launcher_update TIMEOUT 30
            ENVIRONMENT "ORACLES_SETTINGS_DIR=${CMAKE_CURRENT_BINARY_DIR}/launcher-test-home")
        add_test(NAME oracles-launcher-screenshot-update
            COMMAND oracles-test-launcher-screenshot ${CMAKE_CURRENT_BINARY_DIR}/launcher_update_open.ppm 640 480 ${ORACLES_LAYOUT_REFERENCE} 24d "update text")
        set_tests_properties(oracles-launcher-screenshot-update PROPERTIES FIXTURES_REQUIRED launcher_update)
        add_test(NAME oracles-launcher-update-check-off
            COMMAND oracles --launcher-screenshot ${CMAKE_CURRENT_BINARY_DIR}/launcher_update_off.ppm --launcher-size 640x480
                    --update-check-url ${CMAKE_CURRENT_BINARY_DIR}/update_release_newer.json)
        set_tests_properties(oracles-launcher-update-check-off PROPERTIES PASS_REGULAR_EXPRESSION "update check: off"
            FAIL_REGULAR_EXPRESSION "asking|available" TIMEOUT 30 ENVIRONMENT "ORACLES_SETTINGS_DIR=${update_off_home}")
        add_test(NAME oracles-launcher-update-check-rom
            COMMAND oracles --rom ${CMAKE_CURRENT_BINARY_DIR}/launcher_without_rom.tmp --update-check-url ${CMAKE_CURRENT_BINARY_DIR}/update_release_newer.json)
        set_tests_properties(oracles-launcher-update-check-rom PROPERTIES PASS_REGULAR_EXPRESSION "usage: the-oracles-project" TIMEOUT 20
            ENVIRONMENT "ORACLES_SETTINGS_DIR=${CMAKE_CURRENT_BINARY_DIR}/launcher-test-home;SDL_VIDEO_DRIVER=dummy")

        # The launcher's window lent to a session whose start fails (no audio driver by that name) comes back intact; what
        # is dropped on it is left to SDL; a controller unplugged and plugged in during a session is taken back; the file
        # dialog answers on the queue, and one that cannot open is no dialog.
        add_executable(oracles-test-launcher-sdl-backend test_launcher_sdl_backend.c ${ORACLES_LAUNCHER_DIR}/sdl_backend.c ${ORACLES_LAUNCHER_DIR}/sdl_window.c ${ORACLES_LAUNCHER_DIR}/sdl_input.c ${ORACLES_LAUNCHER_DIR}/sdl_present.c ${ORACLES_LAUNCHER_DIR}/safe_area.c ${ORACLES_SESSION_DIR}/hotkey_lines.c ${ORACLES_LAUNCHER_DIR}/file_dialog.c ${ORACLES_LAUNCHER_DIR}/touch_controls.c ${ORACLES_LAUNCHER_DIR}/touch_sdl.c)
        target_include_directories(oracles-test-launcher-sdl-backend PRIVATE ${ORACLES_LAUNCHER_DIR} ${ORACLES_SESSION_DIR} ${ORACLES_ENHANCED_DIR})
        target_link_libraries(oracles-test-launcher-sdl-backend PRIVATE oracles_host oracles_guest oracles_gameplay oracles_ui ${ORACLES_SDL_TARGET})
        oracles_set_warnings(oracles-test-launcher-sdl-backend)
        add_test(NAME oracles-launcher-sdl-backend COMMAND oracles-test-launcher-sdl-backend ${CMAKE_CURRENT_BINARY_DIR}/launcher-dialog)

        # The controller's Escape: the menu's button, its trigger and Guide in play, on the home screen and in the pause,
        # the device an event comes from, kept from the home screen to the pause, and the first game's toast.
        add_executable(oracles-test-launcher-menu test_launcher_menu.c ${ORACLES_LAUNCHER_DIR}/sdl_backend.c ${ORACLES_LAUNCHER_DIR}/sdl_window.c ${ORACLES_LAUNCHER_DIR}/sdl_input.c ${ORACLES_LAUNCHER_DIR}/sdl_present.c ${ORACLES_LAUNCHER_DIR}/safe_area.c ${ORACLES_LAUNCHER_DIR}/home_input.c ${ORACLES_LAUNCHER_DIR}/pause.c ${ORACLES_LAUNCHER_DIR}/touch_controls.c ${ORACLES_LAUNCHER_DIR}/touch_sdl.c)
        target_include_directories(oracles-test-launcher-menu PRIVATE ${ORACLES_LAUNCHER_DIR} ${ORACLES_SESSION_DIR} ${ORACLES_ENHANCED_DIR})
        target_link_libraries(oracles-test-launcher-menu PRIVATE oracles_host oracles_guest oracles_gameplay oracles_ui_sdl ${ORACLES_SDL_TARGET})
        oracles_set_warnings(oracles-test-launcher-menu)
        add_test(NAME oracles-launcher-menu COMMAND oracles-test-launcher-menu)

        # Every text style the launcher's sources define fits the atlases of one drawing at one scale.  The list is
        # read from the sources at configure: a style added later is in it.
        file(GLOB ORACLES_UI_SOURCES ${ORACLES_LAUNCHER_DIR}/ui/*.c)
        file(GLOB ORACLES_UI_HEADERS RELATIVE ${ORACLES_LAUNCHER_DIR}/ui ${ORACLES_LAUNCHER_DIR}/ui/*.h)
        set(ORACLES_STYLES_HEADER "/* Generated by CMake from launcher/ui: every text style defined there. */\n")
        foreach(header ${ORACLES_UI_HEADERS})
            string(APPEND ORACLES_STYLES_HEADER "#include \"${header}\"\n")
        endforeach()
        string(APPEND ORACLES_STYLES_HEADER "#define ORACLES_LAUNCHER_STYLES")
        foreach(source ${ORACLES_UI_SOURCES})
            file(STRINGS ${source} definitions REGEX "^const OraclesUiTextStyle oracles_ui_[a-z_0-9]+")
            foreach(definition ${definitions})
                string(REGEX REPLACE "^const OraclesUiTextStyle (oracles_ui_[a-z_0-9]+).*" "\\1" style "${definition}")
                string(APPEND ORACLES_STYLES_HEADER " &${style},")
            endforeach()
        endforeach()
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/generated/launcher_styles.h "${ORACLES_STYLES_HEADER}\n")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ORACLES_UI_SOURCES})
        add_executable(oracles-test-launcher-atlases test_launcher_atlases.c)
        target_include_directories(oracles-test-launcher-atlases PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
        target_link_libraries(oracles-test-launcher-atlases PRIVATE oracles_ui_sdl)
        oracles_set_warnings(oracles-test-launcher-atlases)
        add_test(NAME oracles-launcher-atlases COMMAND oracles-test-launcher-atlases)

        # The options of a game --rom starts, given without it, are refused with the usage.
        foreach(option save play frames screenshot no-window)
            if(option STREQUAL "no-window")
                set(arguments --no-window)
            elseif(option STREQUAL "frames")
                set(arguments --frames 10)
            else()
                set(arguments --${option} ${CMAKE_CURRENT_BINARY_DIR}/launcher_without_rom.tmp)
            endif()
            add_test(NAME oracles-launcher-without-rom-${option} COMMAND oracles ${arguments})
            set_tests_properties(oracles-launcher-without-rom-${option} PROPERTIES PASS_REGULAR_EXPRESSION "usage: the-oracles-project" TIMEOUT 20
                ENVIRONMENT "ORACLES_SETTINGS_DIR=${CMAKE_CURRENT_BINARY_DIR}/launcher-test-home;SDL_VIDEO_DRIVER=dummy")
        endforeach()

        # --launcher-screenshot draws offscreen with SDL's software renderer: a test
        # without a display or a ROM, at 1080p, at the smallest window, letterboxed in the
        # 4:3 layout (1000x700 is nearer 4:3), and at a handheld's 640x480.
        # The settings directory is an empty one of the build (ORACLES_SETTINGS_DIR), the player's never read.
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/launcher-test-home)
        add_executable(oracles-test-launcher-screenshot test_launcher_screenshot.c layout_reference.c)
        target_link_libraries(oracles-test-launcher-screenshot PRIVATE oracles_ui)
        oracles_set_warnings(oracles-test-launcher-screenshot)
        foreach(size 1920x1080 960x540 1000x700 640x480)
            string(REPLACE "x" ";" dimensions ${size})
            list(GET dimensions 0 width)
            list(GET dimensions 1 height)
            add_test(NAME oracles-launcher-screenshot-${size}-draw
                COMMAND oracles --launcher-screenshot ${CMAKE_CURRENT_BINARY_DIR}/launcher_${size}.ppm --launcher-size ${size})
            set_tests_properties(oracles-launcher-screenshot-${size}-draw PROPERTIES FIXTURES_SETUP launcher_${size}
                ENVIRONMENT "ORACLES_SETTINGS_DIR=${CMAKE_CURRENT_BINARY_DIR}/launcher-test-home")
            add_test(NAME oracles-launcher-screenshot-${size}
                COMMAND oracles-test-launcher-screenshot ${CMAKE_CURRENT_BINARY_DIR}/launcher_${size}.ppm ${width} ${height} ${ORACLES_LAYOUT_REFERENCE})
            set_tests_properties(oracles-launcher-screenshot-${size} PROPERTIES FIXTURES_REQUIRED launcher_${size})
        endforeach()
        # Controls in 4:3 at a handheld's 640x480, on its Hotkeys tab (the layout reference's frame 10j).
        add_test(NAME oracles-launcher-screenshot-controls-4-3-draw
            COMMAND oracles --launcher-screenshot ${CMAKE_CURRENT_BINARY_DIR}/launcher_controls_4_3.ppm --launcher-size 640x480 --launcher-input down,down,ok,up,right)
        set_tests_properties(oracles-launcher-screenshot-controls-4-3-draw PROPERTIES FIXTURES_SETUP launcher_controls_4_3
            ENVIRONMENT "ORACLES_SETTINGS_DIR=${CMAKE_CURRENT_BINARY_DIR}/launcher-test-home")
        add_test(NAME oracles-launcher-screenshot-controls-4-3
            COMMAND oracles-test-launcher-screenshot ${CMAKE_CURRENT_BINARY_DIR}/launcher_controls_4_3.ppm 640 480 ${ORACLES_LAYOUT_REFERENCE} 10j "Item hotkeys title")
        set_tests_properties(oracles-launcher-screenshot-controls-4-3 PROPERTIES FIXTURES_REQUIRED launcher_controls_4_3)
        # The home screen at a handheld's 640x480 back from a game that ran slowly at High (core=mgba, far and auto by
        # default): its toast at the bottom left (the layout reference's frame 11h).
        set(slow_home ${CMAKE_CURRENT_BINARY_DIR}/launcher-test-slow)
        file(MAKE_DIRECTORY ${slow_home})
        file(WRITE ${slow_home}/settings.txt "core=mgba\n")
        add_test(NAME oracles-launcher-screenshot-slow-draw
            COMMAND oracles --launcher-screenshot ${CMAKE_CURRENT_BINARY_DIR}/launcher_slow.ppm --launcher-size 640x480 --launcher-input slow)
        set_tests_properties(oracles-launcher-screenshot-slow-draw PROPERTIES FIXTURES_SETUP launcher_slow ENVIRONMENT "ORACLES_SETTINGS_DIR=${slow_home}")
        add_test(NAME oracles-launcher-screenshot-slow
            COMMAND oracles-test-launcher-screenshot ${CMAKE_CURRENT_BINARY_DIR}/launcher_slow.ppm 640 480 ${ORACLES_LAYOUT_REFERENCE} 11h "toast text")
        set_tests_properties(oracles-launcher-screenshot-slow PROPERTIES FIXTURES_REQUIRED launcher_slow)
        # Mods at the smallest window: a settings directory of the build whose mods folder holds the two example mods,
        # both active, and a third refused for its missing main.lua (the layout reference's frame 8c).
        # A copy of the sources without mods/ skips it.
        set(example_mods ${CMAKE_CURRENT_SOURCE_DIR}/../mods)
        if(EXISTS ${example_mods}/claw-game/main.lua AND EXISTS ${example_mods}/fortune-teller/main.lua)
            set(mods_home ${CMAKE_CURRENT_BINARY_DIR}/launcher-test-mods)
            file(REMOVE_RECURSE ${mods_home})
            file(COPY ${example_mods}/claw-game ${example_mods}/fortune-teller DESTINATION ${mods_home}/mods)
            file(WRITE ${mods_home}/mods/night-market/helper.lua "return 1\n")
            file(WRITE ${mods_home}/settings.txt "mods_ages=claw-game,fortune-teller\n")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${example_mods}/claw-game/main.lua ${example_mods}/fortune-teller/main.lua)
            add_test(NAME oracles-launcher-screenshot-mods-draw
                COMMAND oracles --launcher-screenshot ${CMAKE_CURRENT_BINARY_DIR}/launcher_mods.ppm --launcher-size 960x540 --launcher-input down,down,down,down,ok)
            set_tests_properties(oracles-launcher-screenshot-mods-draw PROPERTIES FIXTURES_SETUP launcher_mods ENVIRONMENT "ORACLES_SETTINGS_DIR=${mods_home}")
            add_test(NAME oracles-launcher-screenshot-mods
                COMMAND oracles-test-launcher-screenshot ${CMAKE_CURRENT_BINARY_DIR}/launcher_mods.ppm 960 540 ${ORACLES_LAYOUT_REFERENCE} 8c "claw name")
            set_tests_properties(oracles-launcher-screenshot-mods PROPERTIES FIXTURES_REQUIRED launcher_mods)
        else()
            message(STATUS "No example mods in ${example_mods}: the Mods page's screenshot test is left out")
        endif()
        if(WIN32)
            # Under the UTF-8 code page, a path outside ASCII opens as it is: the capture written into an accented
            # folder, then read by CMake, which takes UTF-8 paths on every system.
            set(accented ${CMAKE_CURRENT_BINARY_DIR}/launcher-dépôt)
            file(MAKE_DIRECTORY ${accented})
            add_test(NAME oracles-launcher-utf8-path-draw
                COMMAND oracles --launcher-screenshot ${accented}/launcher.ppm --launcher-size 960x540)
            set_tests_properties(oracles-launcher-utf8-path-draw PROPERTIES FIXTURES_SETUP launcher_utf8
                ENVIRONMENT "ORACLES_SETTINGS_DIR=${CMAKE_CURRENT_BINARY_DIR}/launcher-test-home")
            add_test(NAME oracles-launcher-utf8-path COMMAND ${CMAKE_COMMAND} -E sha256sum ${accented}/launcher.ppm)
            set_tests_properties(oracles-launcher-utf8-path PROPERTIES FIXTURES_REQUIRED launcher_utf8)
        endif()
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
