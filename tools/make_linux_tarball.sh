#!/usr/bin/env bash
# Assembles the Linux archive of a release from a build of the launcher, the-oracles-project-X.Y.Z-linux-<machine>.tar.gz,
# which runs without installation: a folder with the-oracles-project and the libSDL3.so.0 it loads, found beside it
# (its run path is $ORIGIN), the licences of what it embeds, a README.txt, and the icon with a .desktop file a player
# may install by hand.  X.Y.Z is config/version.json's.
#
#   tools/make_linux_tarball.sh BUILD_DIR [OUT_DIR]
#
# BUILD_DIR is a build configured with -DORACLES_SDL3_FROM_SOURCE=ON (the SDL 3 pinned in config/sdl3.json, built
# with the engine, rather than the system's) and whose target oracles is built.  The run path is set with patchelf
# (apt install patchelf).  The archive is written to OUT_DIR, the current directory by default.
set -euo pipefail

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "usage: tools/make_linux_tarball.sh BUILD_DIR [OUT_DIR]" >&2
    exit 2
fi
root="$(cd "$(dirname "$0")/.." && pwd)"
build="$(cd "$1" && pwd)"
mkdir -p "${2:-.}"
out="$(cd "${2:-.}" && pwd)"
command -v patchelf >/dev/null || { echo "patchelf is needed (apt install patchelf)" >&2; exit 2; }

version="$(python3 -c 'import json, sys; print(json.load(open(sys.argv[1]))["version"])' "$root/config/version.json")"
name="the-oracles-project-${version}-linux-$(uname -m)"
sdl="$build/_deps/sdl3-build/libSDL3.so.0"
if [ ! -f "$build/the-oracles-project" ] || [ ! -f "$sdl" ]; then
    echo "$build has no the-oracles-project or no SDL 3 of its own: configure it with -DORACLES_SDL3_FROM_SOURCE=ON and build the target oracles" >&2
    exit 1
fi

stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
dir="$stage/$name"
mkdir "$dir"
cp "$build/the-oracles-project" "$dir/"
cp -L "$sdl" "$dir/libSDL3.so.0"
# The library beside the executable, and no path of the build machine.
patchelf --set-rpath '$ORIGIN' "$dir/the-oracles-project"
cp "$build"/*-OFL.txt "$build/SameBoy-LICENSE.txt" "$build/mGBA-LICENSE.txt" "$build/mGBA-NOTICE.txt" "$build/mGBA-boot-rom.patch" "$build/inih-LICENSE.txt" \
    "$build/SDL3-LICENSE.txt" "$dir/"
cp "$root/launcher/icon/the-oracles-project-512.png" "$dir/"
sed -e "s/X\.Y\.Z/${version}/" -e "s/PLATFORM/Linux/" "$root/.github/release-readme.txt" > "$dir/README.txt"
cat > "$dir/the-oracles-project.desktop" <<'DESKTOP'
[Desktop Entry]
# To add the launcher to the applications menu: set Exec and Icon to the full paths of the-oracles-project and
# the-oracles-project-512.png in this folder, then copy this file to ~/.local/share/applications/.
Type=Application
Name=The Oracles Project
Comment=A port of Oracle of Ages and Oracle of Seasons
Exec=the-oracles-project
Icon=the-oracles-project-512
Terminal=false
Categories=Game;
DESKTOP

tar -C "$stage" --owner=0 --group=0 --sort=name -czf "$out/$name.tar.gz" "$name"
echo "$out/$name.tar.gz"
