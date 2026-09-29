#!/usr/bin/env bash
# Builds the launcher's Android application (arm64-v8a and x86_64) and puts it
# in android/build-apk/: the-oracles-project-debug.apk, or with `release`
# the-oracles-project-release.apk, signed when the release's key is given
# (ORACLES_KEYSTORE and its passwords, docs/BUILDING.md), else
# the-oracles-project-release-unsigned.apk.
#
#   Linux or macOS, with an Android SDK (ANDROID_HOME) and a JDK 17 or later:
#       ./android/build_apk.sh [release]
#   WSL, with Android Studio installed on Windows and ANDROID_HOME unset:
#       ./android/build_apk.sh [release]
#     The build then runs on Windows, with Android Studio's JDK, its SDK and
#     the Windows network, from a copy of the sources and config/ kept under
#     %LOCALAPPDATA%\oracles-android (updated in place, so that Gradle builds
#     incrementally); the APK is copied back.
#
# The Android Gradle plugin installs the NDK and CMake the build names when the
# SDK lacks them.
set -euo pipefail

# Written for macOS's bash 3.2 too.
case "${1:-}" in
    "") variant=debug; task=assembleDebug ;;
    release) variant=release; task=assembleRelease ;;
    *) echo "usage: $0 [release]" >&2; exit 2 ;;
esac

here="$(cd "$(dirname "$0")" && pwd)"
repository="$(cd "$here/.." && pwd)"
describe="$(git -C "$repository" describe --tags --always --long --match "v[0-9]*" 2>/dev/null || true)"
output="$here/build-apk"
mkdir -p "$output"

if [ -z "${ANDROID_HOME:-}" ] && grep -qi microsoft /proc/version 2>/dev/null; then
    local_app_data="$(cd /mnt/c && cmd.exe /c "echo %LOCALAPPDATA%" 2>/dev/null | tr -d '\r')"
    stage_windows="$local_app_data\\oracles-android"
    stage="$(wslpath "$stage_windows")"
    studio="C:\\Program Files\\Android\\Android Studio\\jbr"
    [ -d "$(wslpath "$studio")" ] || { echo "no Android Studio in $studio: install it, or set ANDROID_HOME for a Linux SDK" >&2; exit 2; }
    mkdir -p "$stage"
    # CMakeLists.txt reads config/sameboy.json and config/sdl3.json and its subdirectories: all under one root, the build
    # outputs left out.
    rsync -a --delete --exclude=/engine/game/data/build --exclude=/android/.gradle \
        --exclude=/android/build --exclude=/android/app/build --exclude=/android/app/.cxx --exclude=/android/build-apk \
        --include=/CMakeLists.txt --include='/third_party/***' --include='/engine/***' --include='/harness/***' \
        --include='/launcher/***' --include='/tests/***' --include='/android/***' --include='/config/***' --exclude='*' \
        "$repository/" "$stage/"
    # The commands go through a batch file: quotes passed from WSL to cmd.exe would reach it escaped.  The release's key
    # goes through the environment (WSLENV, the keystore's path translated), never into the file.
    printf '@echo off\r\nset "JAVA_HOME=%s"\r\nset "ANDROID_HOME=%s\\Android\\Sdk"\r\ncd /d "%s\\android"\r\ncall gradlew.bat --no-daemon %s "-PoraclesGitDescribe=%s"\r\n' \
        "$studio" "$local_app_data" "$stage_windows" "$task" "$describe" > "$stage/build.bat"
    export WSLENV="${WSLENV:+$WSLENV:}ORACLES_KEYSTORE/p:ORACLES_KEYSTORE_PASSWORD:ORACLES_KEY_ALIAS:ORACLES_KEY_PASSWORD"
    built="$stage/android/app/build/outputs/apk/$variant"
    rm -f "$built"/*.apk   # a signed APK of an earlier build must not pass for this one
    (cd /mnt/c && cmd.exe /c "$stage_windows\\build.bat")
else
    built="$here/app/build/outputs/apk/$variant"
    rm -f "$built"/*.apk
    (cd "$here" && ./gradlew "$task" "-PoraclesGitDescribe=$describe")
fi
if [ "$variant" = debug ]; then
    apk="$output/the-oracles-project-debug.apk"
    cp "$built/app-debug.apk" "$apk"
elif [ -f "$built/app-release.apk" ]; then
    apk="$output/the-oracles-project-release.apk"
    cp "$built/app-release.apk" "$apk"
else
    apk="$output/the-oracles-project-release-unsigned.apk"
    cp "$built/app-release-unsigned.apk" "$apk"
fi
echo "the APK is $apk"
