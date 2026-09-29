# ---- SameBoy core, vendored and unmodified (third_party/sameboy/VERSION.md) --
set(SAMEBOY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sameboy/Core)
# Same file list as upstream's `make lib` with DISABLE_DEBUGGER, DISABLE_CHEATS,
# DISABLE_REWIND and DISABLE_TIMEKEEPING: debugger.c, sm83_disassembler.c,
# symbol_hash.c, cheats.c, cheat_search.c and rewind.c are left out.
add_library(sameboy STATIC
    ${SAMEBOY_DIR}/apu.c ${SAMEBOY_DIR}/camera.c ${SAMEBOY_DIR}/display.c
    ${SAMEBOY_DIR}/gb.c ${SAMEBOY_DIR}/joypad.c ${SAMEBOY_DIR}/mbc.c
    ${SAMEBOY_DIR}/memory.c ${SAMEBOY_DIR}/printer.c ${SAMEBOY_DIR}/random.c
    ${SAMEBOY_DIR}/rumble.c ${SAMEBOY_DIR}/save_state.c ${SAMEBOY_DIR}/sgb.c
    ${SAMEBOY_DIR}/sm83_cpu.c ${SAMEBOY_DIR}/timing.c ${SAMEBOY_DIR}/workboy.c)
target_include_directories(sameboy SYSTEM PUBLIC ${SAMEBOY_DIR})
target_compile_definitions(sameboy PRIVATE GB_INTERNAL GB_VERSION="${ORACLES_SAMEBOY_VERSION}" _GNU_SOURCE _USE_MATH_DEFINES)
# The debugger, cheats and rewind of SameBoy are not part of the product; the
# same definitions reach every consumer of the headers.
target_compile_definitions(sameboy PUBLIC GB_DISABLE_DEBUGGER GB_DISABLE_CHEATS GB_DISABLE_CHEAT_SEARCH GB_DISABLE_REWIND GB_DISABLE_TIMEKEEPING)
if(NOT MSVC)
    # Third-party code is compiled as upstream ships it; its warnings are not ours.
    target_compile_options(sameboy PRIVATE -w)
endif()
if(NOT WIN32)
    target_link_libraries(sameboy PUBLIC m)
endif()
