add_executable(oracles-harness harness.c harness_options.c hotkeys_check.c hotkeys_live.c ${ORACLES_SESSION_DIR}/hotkey_lines.c animation_check.c object_sprites_check.c coverage.c neighbour_check.c ghost_check.c enhanced_check.c enhanced_check_camera.c enhanced_check_pixels.c enhanced_check_report.c ${ORACLES_HOST_DIR}/null_backend.c ${ORACLES_SESSION_DIR}/route.c ${ORACLES_SESSION_DIR}/route_effects.c ${ORACLES_SESSION_DIR}/hotbar_model.c ${ORACLES_SESSION_DIR}/mod_session.c ${ORACLES_SESSION_DIR}/mod_folder.c)
target_include_directories(oracles-harness PRIVATE ${ORACLES_SESSION_DIR})
target_link_libraries(oracles-harness PRIVATE oracles_host oracles_guest oracles_render oracles_ghost oracles_enhanced oracles_gameplay oracles_neighbours oracles_mods)
oracles_set_warnings(oracles-harness)
