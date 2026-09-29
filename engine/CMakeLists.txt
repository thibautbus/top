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

