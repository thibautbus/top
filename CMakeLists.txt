cmake_minimum_required(VERSION 3.19)
project(the-oracles-project LANGUAGES C)
# Let CMake drop a static library repeated on a link line where the linker
# needs no repeat (Apple's ld warns about duplicates); CMake 3.29 and later.
if(POLICY CMP0156)
    cmake_policy(SET CMP0156 NEW)
endif()
# A downloaded archive's files keep the time of their extraction (CMake 3.24 and later): SDL 3 built from its sources.
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# The pinned SameBoy version has one source, config/sameboy.json; it names the
# vendored core and every composite savestate.
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/config/sameboy.json ORACLES_SAMEBOY_JSON)
string(JSON ORACLES_SAMEBOY_VERSION GET ${ORACLES_SAMEBOY_JSON} version)
# mGBA's pinned commit has one source too, config/mgba.json.
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/config/mgba.json ORACLES_MGBA_JSON)
string(JSON ORACLES_MGBA_COMMIT GET ${ORACLES_MGBA_JSON} commit)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

option(ORACLES_WARNINGS_AS_ERRORS "Treat warnings in the engine's own code as errors" OFF)
option(ORACLES_BUILD_LAUNCHER "Build the SDL 3 launcher" ON)
# The core the launcher's games run on, until Display lets the player choose it: sameboy (accurate) or mgba (lighter).
set(ORACLES_DEFAULT_CORE sameboy CACHE STRING "The core the launcher's games run on: sameboy or mgba")
set_property(CACHE ORACLES_DEFAULT_CORE PROPERTY STRINGS sameboy mgba)
if(NOT ORACLES_DEFAULT_CORE MATCHES "^(sameboy|mgba)$")
    message(FATAL_ERROR "ORACLES_DEFAULT_CORE is sameboy or mgba, not ${ORACLES_DEFAULT_CORE}")
endif()

function(oracles_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(ORACLES_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(ORACLES_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# ---- SDL 3 and the product's version ------------------------------------------
set(ORACLES_SDL_TARGET "")
if(ORACLES_BUILD_LAUNCHER)
    # SDL 3, at least the release pinned in config/sdl3.json: the system's when it has one that recent
    # (Homebrew's sdl3), else that release built from its sources with the engine, as the Android NDK builds it, and as
    # ORACLES_SDL3_FROM_SOURCE asks (build_windows.sh: MSYS2's SDL3.dll would need its libiconv-2.dll beside it).
    # FETCHCONTENT_SOURCE_DIR_SDL3 names a copy of those sources already on disk, for a build without the network.
    option(ORACLES_SDL3_FROM_SOURCE "Build the SDL 3 pinned in config/sdl3.json even when one is installed" OFF)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/config/sdl3.json ORACLES_SDL3_JSON)
    string(JSON ORACLES_SDL3_VERSION GET ${ORACLES_SDL3_JSON} version)
    string(JSON ORACLES_SDL3_URL GET ${ORACLES_SDL3_JSON} archive_url)
    string(JSON ORACLES_SDL3_SHA256 GET ${ORACLES_SDL3_JSON} archive_sha256)
    if(NOT ORACLES_SDL3_FROM_SOURCE)
        find_package(SDL3 ${ORACLES_SDL3_VERSION} QUIET CONFIG)
    endif()
    set(ORACLES_SDL3_BUILT OFF)
    if(NOT TARGET SDL3::SDL3)
        message(STATUS "SDL ${ORACLES_SDL3_VERSION} is built from ${ORACLES_SDL3_URL}")
        include(FetchContent)
        FetchContent_Declare(SDL3 URL ${ORACLES_SDL3_URL} URL_HASH SHA256=${ORACLES_SDL3_SHA256})
        set(SDL_SHARED ON)
        set(SDL_STATIC OFF)
        set(SDL_TEST_LIBRARY OFF)
        FetchContent_MakeAvailable(SDL3)
        # Windows finds a DLL beside the executable: SDL3.dll is built there, for the tests and the release alike.
        set_target_properties(SDL3-shared PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        set(ORACLES_SDL3_BUILT ON)
    endif()
    set(ORACLES_SDL_TARGET SDL3::SDL3)
    if(NOT ORACLES_SDL3_BUILT)
        message(STATUS "SDL ${SDL3_VERSION} found in ${SDL3_DIR}")
    endif()
    # The version: the product's, config/version.json's, raised at each release, which is tagged v<version>
    # (docs/BUILDING.md, releasing).  A build of that tag shows it alone, "v1.0.0"; any other shows the commit's short
    # hash beside it, "v1.0.0 (be8571e)".  The hash comes from `git describe --tags --always --long --match "v[0-9]*"` at
    # configure, or from -DORACLES_GIT_DESCRIBE=..., which build_windows.sh and the Android build pass for a copy without
    # git; outside a repository, the version alone.  A session's report gives the whole description (ORACLES_BUILD).
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/config/version.json ORACLES_VERSION_JSON)
    string(JSON ORACLES_PRODUCT_VERSION GET ${ORACLES_VERSION_JSON} version)
    if(NOT ORACLES_PRODUCT_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
        message(FATAL_ERROR "config/version.json: ${ORACLES_PRODUCT_VERSION} is not MAJOR.MINOR.PATCH")
    endif()
    if(NOT ORACLES_GIT_DESCRIBE)
        find_package(Git QUIET)
        if(GIT_FOUND)
            execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --always --long --match "v[0-9]*" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE ORACLES_GIT_DESCRIBE OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE git_status)
            if(NOT git_status EQUAL 0)
                set(ORACLES_GIT_DESCRIBE "")
            endif()
        endif()
    endif()
    set(ORACLES_VERSION v${ORACLES_PRODUCT_VERSION})
    set(ORACLES_BUILD v${ORACLES_PRODUCT_VERSION})
    if(ORACLES_GIT_DESCRIBE MATCHES "^(v[0-9.]+)-([0-9]+)-g([0-9a-f]+)$")   # a release tag, and the commits past it
        if(NOT CMAKE_MATCH_2 EQUAL 0)
            set(ORACLES_VERSION "v${ORACLES_PRODUCT_VERSION} (${CMAKE_MATCH_3})")
        elseif(NOT CMAKE_MATCH_1 STREQUAL "v${ORACLES_PRODUCT_VERSION}")
            message(WARNING "the commit is tagged ${CMAKE_MATCH_1} but config/version.json says ${ORACLES_PRODUCT_VERSION}")
            set(ORACLES_VERSION "v${ORACLES_PRODUCT_VERSION} (${CMAKE_MATCH_3})")
        endif()
        set(ORACLES_BUILD ${ORACLES_GIT_DESCRIBE})
    elseif(ORACLES_GIT_DESCRIBE MATCHES "^[0-9a-f]+$")   # no release tag yet: the hash
        set(ORACLES_VERSION "v${ORACLES_PRODUCT_VERSION} (${ORACLES_GIT_DESCRIBE})")
        set(ORACLES_BUILD "v${ORACLES_PRODUCT_VERSION}-g${ORACLES_GIT_DESCRIBE}")
    endif()
    message(STATUS "oracles version: ${ORACLES_VERSION}, build ${ORACLES_BUILD}")
endif()

# Every executable at the top of the build directory, whichever directory declares it, as when one file declared them
# all: beside SDL3.dll on Windows, and where build_windows.sh, the CI and the route checks take them.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# The session's sources (engine/session) and the host's backend without a window (engine/host/null_backend.c) are
# compiled into the harness, the launcher and the tests that need them.
set(ORACLES_SESSION_DIR ${CMAKE_CURRENT_SOURCE_DIR}/engine/session)
set(ORACLES_HOST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/engine/host)

add_subdirectory(third_party)
add_subdirectory(engine)
add_subdirectory(harness)
add_subdirectory(launcher)

# ---- tests without ROM --------------------------------------------------------
include(CTest)
add_subdirectory(tests)
