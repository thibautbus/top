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

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

option(ORACLES_WARNINGS_AS_ERRORS "Treat warnings in the engine's own code as errors" OFF)
option(ORACLES_BUILD_LAUNCHER "Build the SDL 3 launcher" ON)

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


add_subdirectory(third_party)
