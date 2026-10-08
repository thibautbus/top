#!/usr/bin/env bash
# Assembles the macOS application from a build of the launcher: "The Oracles Project.app", which the Dock and the
# Finder show with its icon, and which runs on a Mac without SDL installed.  A universal build (a release's,
# CMAKE_OSX_ARCHITECTURES="arm64;x86_64") gives a universal application.
#
#   tools/make_macos_app.sh BUILD_DIR [OUT_DIR]
#
#   The Oracles Project.app/Contents/
#     Info.plist                      launcher/macos/Info.plist, with the executable's name, the version
#                                     (config/version.json) and the minimum macOS the executable was built for
#     MacOS/The Oracles Project       the executable, BUILD_DIR/the-oracles-project, its run path
#                                     @executable_path/../Frameworks
#     Frameworks/libSDL3.0.dylib      the SDL 3 it loads: the one built with the engine (-DORACLES_SDL3_FROM_SOURCE=ON,
#                                     as a release builds it) or the system's (Homebrew's sdl3)
#     Resources/the-oracles-project.icns, and the licences of the fonts, SameBoy and SDL 3
#
# The application is signed ad hoc, as a Mac with Apple Silicon requires of any code it runs; it is not signed by a
# developer nor notarised, so the first opening goes through a right-click, Open.  OUT_DIR is the current directory by
# default; an application already there is replaced.
set -euo pipefail

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "usage: tools/make_macos_app.sh BUILD_DIR [OUT_DIR]" >&2
    exit 2
fi
root="$(cd "$(dirname "$0")/.." && pwd)"
build="$(cd "$1" && pwd)"
mkdir -p "${2:-.}"
out="$(cd "${2:-.}" && pwd)"
executable="$build/the-oracles-project"
[ -f "$executable" ] || { echo "$build has no the-oracles-project: build the target oracles first" >&2; exit 1; }

# The run paths of a binary, once each: a universal binary lists its load commands for each architecture.
run_paths() { otool -l "$1" | awk '/LC_RPATH/ { found = 1 } found && $1 == "path" { print $2; found = 0 }' | sort -u; }

app="$out/The Oracles Project.app"
contents="$app/Contents"
binary="$contents/MacOS/The Oracles Project"
rm -rf "$app"
mkdir -p "$contents/MacOS" "$contents/Frameworks" "$contents/Resources"
cp "$executable" "$binary"
chmod u+w "$binary"

# SDL 3, as the executable names it: @rpath/libSDL3.0.dylib when built with the engine, an absolute path for
# Homebrew's.  The file is found through the executable's run paths, copied into Frameworks under the name the
# executable asks for, and named @rpath there.
sdl_ref="$(otool -L "$executable" | awk '/libSDL3/ { print $1; exit }')"
[ -n "$sdl_ref" ] || { echo "the-oracles-project does not load SDL 3" >&2; exit 1; }
sdl_name="$(basename "$sdl_ref")"
sdl_file=""
case "$sdl_ref" in
    @rpath/*)
        while read -r run_path; do
            candidate="${run_path/@loader_path/$build}"
            candidate="${candidate/@executable_path/$build}/$sdl_name"
            if [ -f "$candidate" ]; then sdl_file="$candidate"; break; fi
        done < <(run_paths "$executable")
        ;;
    *) sdl_file="$sdl_ref" ;;
esac
[ -n "$sdl_file" ] && [ -f "$sdl_file" ] || { echo "SDL 3 ($sdl_ref) is not found beside the build" >&2; exit 1; }
cp -L "$sdl_file" "$contents/Frameworks/$sdl_name"
chmod u+w "$contents/Frameworks/$sdl_name"
install_name_tool -id "@rpath/$sdl_name" "$contents/Frameworks/$sdl_name"
[ "$sdl_ref" = "@rpath/$sdl_name" ] || install_name_tool -change "$sdl_ref" "@rpath/$sdl_name" "$binary"
# The build's run paths go, the bundle's own comes.
run_paths "$binary" | while read -r run_path; do
    install_name_tool -delete_rpath "$run_path" "$binary"
done
install_name_tool -add_rpath "@executable_path/../Frameworks" "$binary"

# Info.plist: the executable's own, completed for a bundle.
version="$(python3 -c 'import json, sys; print(json.load(open(sys.argv[1]))["version"])' "$root/config/version.json")"
minimum="$(otool -l "$executable" | awk '$1 == "minos" { print $2; exit }')"
plist="$contents/Info.plist"
cp "$root/launcher/macos/Info.plist" "$plist"
buddy=/usr/libexec/PlistBuddy
"$buddy" -c "Add :CFBundleExecutable string 'The Oracles Project'" "$plist"
"$buddy" -c "Add :CFBundleDisplayName string 'The Oracles Project'" "$plist"
"$buddy" -c "Add :CFBundlePackageType string APPL" "$plist"
"$buddy" -c "Add :CFBundleInfoDictionaryVersion string 6.0" "$plist"
"$buddy" -c "Add :CFBundleShortVersionString string $version" "$plist"
"$buddy" -c "Add :CFBundleVersion string $version" "$plist"
[ -z "$minimum" ] || "$buddy" -c "Add :LSMinimumSystemVersion string $minimum" "$plist"
plutil -lint "$plist" >/dev/null

cp "$root/launcher/icon/the-oracles-project.icns" "$contents/Resources/"
cp "$build"/*-OFL.txt "$build/SameBoy-LICENSE.txt" "$build/mGBA-LICENSE.txt" "$build/mGBA-NOTICE.txt" "$build/mGBA-boot-rom.patch" "$build/inih-LICENSE.txt" \
    "$contents/Resources/"
if [ -f "$build/SDL3-LICENSE.txt" ]; then
    cp "$build/SDL3-LICENSE.txt" "$contents/Resources/"
else
    echo "no SDL3-LICENSE.txt in $build (SDL 3 not built with the engine): the application goes without it" >&2
fi

codesign --force --sign - "$contents/Frameworks/$sdl_name"
codesign --force --sign - "$app"
codesign --verify --strict "$app"
echo "$app ($(lipo -archs "$binary"), macOS $minimum or later)"
