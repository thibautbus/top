# Sourced by tools/make_linux_tarball.sh and tools/make_linux_appimage.sh: what the Linux archive and the AppImage of
# a release both hold, from a build of the launcher.
#
#   linux_release_check BUILD_DIR       checks the build and patchelf, and sets version (config/version.json's) and
#                                       arch, the machine in the files' names: x86_64 or arm64 (uname -m's aarch64)
#   linux_release_files BUILD_DIR DIR   puts in DIR the-oracles-project with the libSDL3.so.0 it loads beside it (its
#                                       run path is $ORIGIN), the licences of what it embeds and a README.txt
#
# BUILD_DIR is a build configured with -DORACLES_SDL3_FROM_SOURCE=ON (the SDL 3 pinned in config/sdl3.json, built
# with the engine, rather than the system's) and whose target oracles is built.

linux_release_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

linux_release_check() {
    command -v patchelf >/dev/null || { echo "patchelf is needed (apt install patchelf)" >&2; exit 2; }
    if [ ! -f "$1/the-oracles-project" ] || [ ! -f "$1/_deps/sdl3-build/libSDL3.so.0" ]; then
        echo "$1 has no the-oracles-project or no SDL 3 of its own: configure it with -DORACLES_SDL3_FROM_SOURCE=ON and build the target oracles" >&2
        exit 1
    fi
    version="$(python3 -c 'import json, sys; print(json.load(open(sys.argv[1]))["version"])' "$linux_release_root/config/version.json")"
    arch="$(uname -m)"
    if [ "$arch" = aarch64 ]; then arch=arm64; fi
}

linux_release_files() {
    local build="$1" dir="$2"
    cp "$build/the-oracles-project" "$dir/"
    cp -L "$build/_deps/sdl3-build/libSDL3.so.0" "$dir/libSDL3.so.0"
    # The library beside the executable, and no path of the build machine.
    patchelf --set-rpath '$ORIGIN' "$dir/the-oracles-project"
    cp "$build"/*-OFL.txt "$build/SameBoy-LICENSE.txt" "$build/xBR-LICENSE.txt" "$build/mGBA-LICENSE.txt" "$build/mGBA-NOTICE.txt" "$build/mGBA-boot-rom.patch" "$build/inih-LICENSE.txt" \
        "$build/Lua-LICENSE.txt" "$build/SDL3-LICENSE.txt" "$dir/"
    # RetroAchievements' licence where the build has them (ORACLES_RETROACHIEVEMENTS).
    if grep -q '^ORACLES_RETROACHIEVEMENTS:BOOL=ON' "$build/CMakeCache.txt" 2>/dev/null; then cp "$build/rcheevos-LICENSE.txt" "$dir/"; fi
    sed -e "s/X\.Y\.Z/${version}/" -e "s/PLATFORM/Linux/" "$linux_release_root/.github/release-readme.txt" > "$dir/README.txt"
}
