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

    add_executable(oracles-test-state test_state.c)
    target_link_libraries(oracles-test-state PRIVATE oracles_core)
    oracles_set_warnings(oracles-test-state)
    add_test(NAME oracles-state COMMAND oracles-test-state ${CMAKE_CURRENT_BINARY_DIR}/test_state.tmp)

    add_executable(oracles-test-host test_host.c ${ORACLES_HOST_DIR}/null_backend.c)
    target_include_directories(oracles-test-host PRIVATE ${ORACLES_HOST_DIR})
    target_link_libraries(oracles-test-host PRIVATE oracles_host)
    oracles_set_warnings(oracles-test-host)
    add_test(NAME oracles-host COMMAND oracles-test-host)
endif()
