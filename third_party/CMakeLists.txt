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

# ---- mGBA's Game Boy core, vendored with one patch (third_party/mgba/VERSION.md) --
set(MGBA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mgba/core)
# The vendored tree holds only what is built (tools/vendor_mgba.py): every source but the other platform's.
file(GLOB_RECURSE MGBA_SOURCES CONFIGURE_DEPENDS ${MGBA_DIR}/src/*.c)
list(FILTER MGBA_SOURCES EXCLUDE REGEX "/src/platform/")
if(WIN32)
    list(APPEND MGBA_SOURCES ${MGBA_DIR}/src/platform/windows/memory.c)
else()
    list(APPEND MGBA_SOURCES ${MGBA_DIR}/src/platform/posix/memory.c)
endif()
# The version file mGBA's configuration generates, named by the pinned commit; its variables stay in the function.
function(oracles_mgba_version_file)
    set(GIT_COMMIT ${ORACLES_MGBA_COMMIT})
    string(SUBSTRING ${ORACLES_MGBA_COMMIT} 0 8 GIT_COMMIT_SHORT)
    set(GIT_BRANCH "(unknown)")
    set(GIT_REV -1)
    set(BINARY_NAME mgba)
    set(PROJECT_NAME mGBA)
    set(VERSION_STRING ${GIT_COMMIT_SHORT})
    configure_file(${MGBA_DIR}/src/core/version.c.in ${CMAKE_CURRENT_BINARY_DIR}/mgba/version.c)
endfunction()
oracles_mgba_version_file()
add_library(mgba STATIC ${MGBA_SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/mgba/version.c)
target_include_directories(mgba SYSTEM PUBLIC ${MGBA_DIR}/include)
target_include_directories(mgba PRIVATE ${MGBA_DIR}/src)
# The C library's functions mGBA uses when present, found as its own build finds them (find_function), and its locale
# support, which decides how its headers declare locale_t.
include(CheckFunctionExists)
include(CheckIncludeFiles)
set(MGBA_FUNCTION_DEFINES)
foreach(function strdup strlcpy strndup vasprintf freelocale newlocale setlocale snprintf_l uselocale popcount32
        localtime_r realpath)
    string(TOUPPER ${function} flag)
    check_function_exists(${function} MGBA_HAVE_${flag})
    if(MGBA_HAVE_${flag})
        list(APPEND MGBA_FUNCTION_DEFINES HAVE_${flag})
    endif()
endforeach()
if(ANDROID)
    list(APPEND MGBA_FUNCTION_DEFINES HAVE_STRTOF_L)
elseif(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")   # glibc's strtof_l is not the one mGBA means
    check_function_exists(strtof_l MGBA_HAVE_STRTOF_L)
    if(MGBA_HAVE_STRTOF_L)
        list(APPEND MGBA_FUNCTION_DEFINES HAVE_STRTOF_L)
    endif()
endif()
check_include_files(xlocale.h MGBA_HAVE_XLOCALE)
if(MGBA_HAVE_XLOCALE)
    list(APPEND MGBA_FUNCTION_DEFINES HAVE_XLOCALE)
endif()
if(MGBA_HAVE_NEWLOCALE AND MGBA_HAVE_FREELOCALE OR APPLE)
    list(APPEND MGBA_FUNCTION_DEFINES HAVE_LOCALE)
    if(MGBA_HAVE_SNPRINTF_L)
        list(APPEND MGBA_FUNCTION_DEFINES HAVE_SNPRINTF_L)
    endif()
endif()
# The configuration shapes mGBA's structures (mCore, the colour type) and its headers: every reader of those headers
# gets the definitions the library is built with.  Those of mGBA's libretro core less its virtual files (ENABLE_VFS:
# files on disk, which the engine never gives the core) and its 16-bit colour (the engine takes 32 bits): the Game Boy
# core alone, no threads (the engine runs each core on one thread), no video logger, no configuration overrides.
target_compile_definitions(mgba PUBLIC M_CORE_GB MINIMAL_CORE=2 DISABLE_THREADING BUILD_STATIC ${MGBA_FUNCTION_DEFINES})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR (WIN32 AND NOT MSVC))   # as mGBA's own build
    target_compile_definitions(mgba PRIVATE _GNU_SOURCE)
endif()
if(NOT MSVC)
    target_compile_options(mgba PRIVATE -w)
endif()
if(WIN32)
    target_link_libraries(mgba PUBLIC shlwapi)   # PathIsRelativeW, in its file utilities
else()
    target_link_libraries(mgba PUBLIC m)
endif()
