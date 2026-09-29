# ---- tests without ROM --------------------------------------------------------
if(BUILD_TESTING)

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

    find_package(Python3 COMPONENTS Interpreter)
    # Each fan game's table against its manifest and the guest table's fields: a field the guest gains fails here,
    # before it takes the zero of C in a checked-in table (engine/game/profiles/gen_tables.py).
    if(Python3_Interpreter_FOUND AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../engine/game/profiles/moonrise-regalia/manifest.json)
        add_test(NAME oracles-fan-game-tables COMMAND ${Python3_EXECUTABLE} -m unittest test_gen_tables
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../engine/game/profiles)
    endif()
    add_executable(oracles-test-host test_host.c ${ORACLES_HOST_DIR}/null_backend.c)
    target_include_directories(oracles-test-host PRIVATE ${ORACLES_HOST_DIR})
    target_link_libraries(oracles-test-host PRIVATE oracles_host)
    oracles_set_warnings(oracles-test-host)
    add_test(NAME oracles-host COMMAND oracles-test-host)
endif()
