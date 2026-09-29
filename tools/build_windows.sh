#!/usr/bin/env bash
# Builds and tests the engine on Windows from an MSYS2 UCRT64 shell, and puts
# a runnable the-oracles-project.exe with SDL3.dll in build-windows/.
#
#   MSYS2 UCRT64 terminal:   ./tools/build_windows.sh
#   CI (windows-latest):     C:\msys64\usr\bin\bash.exe -lc "cd <repo> && ./tools/build_windows.sh"
#
# A repository opened from WSL (a //wsl.localhost/... path) is first copied to
# a local temporary directory, because the toolchain does not build reliably
# on a network path; the outputs are copied back to build-windows/.
set -euo pipefail

if [ "${MSYSTEM:-}" != "UCRT64" ]; then
    echo "run this from an MSYS2 UCRT64 shell (MSYSTEM=UCRT64)" >&2
    exit 2
fi

here="$(cd "$(dirname "$0")/.." && pwd)"
source_dir="$here"
work_dir="$here/build-windows"
copied=0
case "$here" in
    //*)
        # CMakeLists.txt reads config/ (sameboy.json, sdl3.json, version.json), and the tests the example mods of mods/:
        # copy them under one local root.
        stage="/tmp/oracles-src"
        source_dir="$stage"
        work_dir="/tmp/oracles-engine-build"
        rm -rf "$stage" "$work_dir"
        mkdir -p "$stage/config"
        # Everything the build reads except build outputs, in one pass: the share is slow per file.
        echo "copying the sources to $source_dir (a network path is slow: about a minute)"
        tar -C "$here" --exclude=engine/game/data/build -cf - CMakeLists.txt third_party engine harness launcher mods tests | tar -C "$source_dir" -xf -
        echo "copied"
        cp "$here"/config/*.json "$stage/config/"
        copied=1
        ;;
esac

# The toolchain is installed only when a piece of it is missing (a fresh
# machine, the CI runner): a build on a machine that has it must not depend on
# the mirrors.  -y refreshes the package database first, the mirrors keeping
# only the latest version of each package; on a system that is not up to date
# that is a partial upgrade, which fails on a conflict as soon as MSYS2 renames
# or splits a package (gcc-libs into libgcc, 2026-09): bring the whole system
# up to date first, pacman -Syu.
missing=0
for tool in gcc cmake ninja; do command -v "$tool" >/dev/null 2>&1 || missing=1; done
if [ "$missing" = 1 ]; then
    pacman -Sy --needed --noconfirm mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja \
        || { echo "the toolchain could not be installed; update MSYS2 first (pacman -Syu, twice if it asks to restart), then run this again" >&2; exit 1; }
fi

# The version is config/version.json's; the commit shown beside it comes from git: ORACLES_GIT_DESCRIBE when given
# (from WSL the copy has no .git: pass ORACLES_GIT_DESCRIBE=$(git describe --tags --always --long --match "v[0-9]*")),
# else CMake asks git, else the version alone.
# SDL 3 is the release pinned in config/sdl3.json, built here into SDL3.dll beside the-oracles-project.exe with its licence
# (SDL3-LICENSE.txt): MSYS2's own SDL3.dll would bring libiconv-2.dll and its licence with it.
cmake -G Ninja -S "$source_dir" -B "$work_dir" -DBUILD_TESTING=ON -DORACLES_WARNINGS_AS_ERRORS=ON -DORACLES_SDL3_FROM_SOURCE=ON \
    ${ORACLES_GIT_DESCRIBE:+"-DORACLES_GIT_DESCRIBE=$ORACLES_GIT_DESCRIBE"}
cmake --build "$work_dir" --parallel
ctest --test-dir "$work_dir" --no-tests=error --output-on-failure
# The other library the binaries load: winpthread for the threads of the ghost instance, whose licence asks that its
# notice go with copies.
cp /ucrt64/bin/libwinpthread-1.dll "$work_dir"/
cp /ucrt64/share/licenses/libwinpthread/COPYING "$work_dir"/libwinpthread-COPYING.txt

if [ "$copied" = 1 ]; then
    # No mkdir -p here: on a //wsl.localhost path it fails on the share root.
    [ -d "$here/build-windows" ] || mkdir "$here/build-windows"
    cp "$work_dir"/the-oracles-project.exe "$work_dir"/oracles-harness.exe "$work_dir"/SDL3.dll "$work_dir"/libwinpthread-1.dll \
        "$work_dir"/SDL3-LICENSE.txt "$work_dir"/libwinpthread-COPYING.txt "$work_dir"/*-OFL.txt "$work_dir"/SameBoy-LICENSE.txt "$here/build-windows/"
fi
echo "the-oracles-project.exe, SDL3.dll, libwinpthread-1.dll and the licences (*-OFL.txt, SameBoy-LICENSE.txt, SDL3-LICENSE.txt, libwinpthread-COPYING.txt) are in $here/build-windows/"
echo "(built in $work_dir)"
