# Building

The port builds with CMake from the repository's root. It needs a C11 compiler, CMake 3.19 or later, and SDL 3 for the launcher. The core does not build with MSVC: SameBoy uses GNU extensions.

## Linux

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The launcher is `build/the-oracles-project`, the differential harness `build/oracles-harness`.

**SDL 3.** The release pinned in `config/sdl3.json` is the minimum: an installed SDL 3 at least that recent is used (`find_package(SDL3 CONFIG)`); otherwise CMake downloads that release's archive, checks its SHA-256 and builds it in the build directory, as a shared library beside the executables, with its licence (`SDL3-LICENSE.txt`). On Linux that build needs the development packages SDL's `README-linux` lists (X11, Wayland, ALSA, PulseAudio or PipeWire...), those the CI installs; without one of them SDL's configuration fails and names the option that does without it, for instance `-DSDL_X11_XTEST=OFF`. Without network, `-DFETCHCONTENT_SOURCE_DIR_SDL3=/path/to/SDL3-3.x.y` gives already extracted sources.

**Options.**

| Option | Default | Effect |
| --- | --- | --- |
| `ORACLES_BUILD_LAUNCHER` | `ON` | the SDL 3 launcher; `OFF` builds the core, the host, the harness and the tests only, without SDL |
| `ORACLES_WARNINGS_AS_ERRORS` | `OFF` | warnings in the port's own code are errors (the CI sets it) |
| `ORACLES_SDL3_FROM_SOURCE` | `OFF` | build the pinned SDL 3 even when one is installed |
| `ORACLES_GIT_DESCRIBE` | empty | the commit description shown beside the version, for a copy of the sources without `.git` |
| `BUILD_TESTING` | `ON` | the tests |

## macOS

On Intel or Apple Silicon, install SDL 3 with Homebrew, then build as on Linux:

```bash
brew install sdl3
cmake -S . -B build -DORACLES_WARNINGS_AS_ERRORS=ON -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The executable embeds an `Info.plist` (`launcher/macos/`) that asks for a Retina display's pixels. The Dock shows the application's icon for an app bundle only; `tools/make_macos_app.sh` makes one from a build:

```bash
./tools/make_macos_app.sh build      # "The Oracles Project.app" in the current directory
```

The bundle holds the executable (`Contents/MacOS/The Oracles Project`, its run path `@executable_path/../Frameworks`), the SDL 3 it loads (`Contents/Frameworks/libSDL3.0.dylib`, Homebrew's or the one built with `-DORACLES_SDL3_FROM_SOURCE=ON`), `Contents/Info.plist` completed with the executable's name, the version and the minimum macOS, the icon and the licences in `Contents/Resources/`; it runs on a Mac without SDL installed. It is signed ad hoc, as Apple Silicon requires, not by a developer: the first opening goes through a right-click, Open. A build configured with `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DORACLES_SDL3_FROM_SOURCE=ON` gives a universal application, as a release builds it.

## Windows

From an MSYS2 UCRT64 terminal:

```bash
./tools/build_windows.sh
```

The script installs the toolchain packages it misses, builds the pinned SDL 3 from source (MSYS2's own `SDL3.dll` would need `libiconv-2.dll` beside it), runs the tests, and leaves in `build-windows/` the executable with the two libraries it loads (`SDL3.dll`, `libwinpthread-1.dll`) and the licences to ship with them (`*-OFL.txt`, `SameBoy-LICENSE.txt`, `SDL3-LICENSE.txt`, `libwinpthread-COPYING.txt`). A repository opened from WSL (a `//wsl.localhost/...` path) is copied to a local temporary directory for the build; that copy has no `.git`, so pass `ORACLES_GIT_DESCRIBE=$(git describe --tags --always --long --match "v[0-9]*")` to the script to keep the commit shown beside the version. The executable carries a manifest (`launcher/windows/oracles.manifest`) that sets its code page to UTF-8, so paths outside ASCII open as they are.

## Android

```bash
./android/build_apk.sh             # android/build-apk/the-oracles-project-debug.apk
./android/build_apk.sh release     # the release APK, signed when the release key is given
```

The Gradle project in `android/` builds, through the NDK, `libmain.so` (the launcher, whose `main` SDL calls) and `libSDL3.so` from the root `CMakeLists.txt`, and SDL 3's own Java activity loads them. The activity and the engine come from the same SDL release, `config/sdl3.json`'s: the Gradle task `fetchSdl3` downloads its archive once, checks its SHA-256, and gives its Java sources to the application and its C sources to CMake. No SDL source is in the repository. Android 10 (API 29) at least, target API 36; ABIs `arm64-v8a` (phones and handhelds) and `x86_64` (a PC's emulator). The Android Gradle plugin installs the NDK and CMake the build names when the SDK lacks them.

On Linux or macOS the script runs Gradle with an Android SDK (`ANDROID_HOME`) and a JDK 17 or later. Under WSL without `ANDROID_HOME`, it builds on the Windows side with Android Studio (its JDK, its SDK, Windows' network) from a copy of the sources kept up to date under `%LOCALAPPDATA%\oracles-android`, and brings the APK back. The project also opens in Android Studio (the `android` folder).

The debug APK is signed with the repository's debug key (`android/debug.keystore`, Android's public debug credentials): debug APKs from the CI and from any machine install over one another. Install and start one with:

```bash
adb install -r android/build-apk/the-oracles-project-debug.apk
adb shell am start -n io.github.thibautbus.theoraclesproject/.OraclesActivity
```

The launcher's standard error, the session report included, goes to Android's log, `adb logcat -s the-oracles-project`, and to `the-oracles-project.log` in the application's folder on the shared storage ([PLAYING.md](PLAYING.md#android)).

## Tests

`ctest` runs the tests without a ROM: the core, the ROM loader and BPS patches, the guest bus and its transactions, the renderer, the ghost's key, the camera, the item hotkeys, the mods' sandbox, the routes' format, the launcher's layout and settings, and the fan games' tables against their manifests. Tests that need a ROM are skipped without one, never simulated; the route suite is one of them (`oracles-routes`, see [`ROUTES.md`](ROUTES.md)).

**The layout reference.** The launcher's layout tests compare what the launcher lays out with `tests/launcher_layout_reference.json`, the measures of every text and element of its screens in the scene's pixels at 1920x1080. It is a reference file: a failing probe names itself, its frame and its expected and actual values. When a change of layout is intended, update the values of the probes it moves in that file, and the review sees what moved.

## The icon

The application's icon lives in `launcher/icon/`: its sources, the SVGs of `launcher/icon/source/` (the mark at three levels of detail, in the rounded square and in macOS' shape, and the layers of Android's adaptive icon), and the files the platforms take, generated from them and committed, so that a build needs nothing more: the Windows `.ico`, the macOS `.icns`, PNGs for Linux desktops, the window's icon (`window_icon.c`) and Android's `mipmap-*` resources. After a change of the sources, `tools/make_icons.py` writes them all again (Linux, with librsvg, cairo and ImageMagick).

## The ROM data readers' oracle

The readers of `engine/game/data/` decode room layouts (modes 0, 1, 2 and the large rooms' dictionary), tileset definitions and layouts, and graphics (modes 0 to 3 of `decompressGraphics`), in the formats of [`ROM_DATA_FORMATS.md`](ROM_DATA_FORMATS.md). No table address is written in their code: `oracles_tables_ages.c` and `oracles_tables_seasons.c` are generated by `gen_tables.py` from the symbol files of the pinned disassembly.

```bash
cd engine/game/data
make tables DISASM=/path/to/oracles-disasm
make check DISASM=/path/to/oracles-disasm ROM_AGES=/path/to/ages.gbc ROM_SEASONS=/path/to/seasons.gbc
```

`make check` decodes everything from the user's ROMs and compares byte for byte with every room of the disassembly's `rooms/<game>/small` and `large`, every `tilesetMappingsXX.bin` and `tilesetCollisionsXX.bin` of `tileset_layouts/<game>`, and every entry of every graphics header against the disassembly's own Python decompressor applied to the same ROM bytes. The empty entries of the tables (Ages' groups 6 and 7, tileset layouts `$33` to `$3f` without a header) are refused by the decoder rather than read past. The ROMs and the disassembly stay outside the repository; the disassembly's commit is `config/disasm.json`'s.

## Releasing

The product's version is in `config/version.json`, the same on every platform, and changes only at a release. To release: change that file (`1.0.0` to `1.1.0`), commit, tag the commit `vX.Y.Z` and push the tag. A build of that commit shows `v1.1.0`; any other shows its commit's short hash beside it, `v1.1.0 (be8571e)`, and CMake warns when a release tag does not match the file. On Android the `versionCode` that decides updates follows it, `MAJOR x 10000 + MINOR x 100 + PATCH` (10000 for 1.0.0): a release installs over the one before, and builds between two releases over one another.

Pushing the tag starts the `Release` workflow (`.github/workflows/release.yml`). It checks the tag against `config/version.json`, builds every platform in Release with warnings as errors (the tests are the CI's, run on the same commit), and publishes a GitHub Release of the tag with five files, which players download with their own ROM:

| Asset | Content |
| --- | --- |
| `the-oracles-project-X.Y.Z-windows.zip` | a folder with `the-oracles-project.exe`, `SDL3.dll`, `libwinpthread-1.dll`, the licences and a `README.txt`, built by `tools/build_windows.sh` (MSYS2 UCRT64) |
| `the-oracles-project-X.Y.Z-macos.zip` | `The Oracles Project.app`, universal (Apple Silicon and Intel, macOS 11 or later), assembled by `tools/make_macos_app.sh`, and a `README.txt` |
| `the-oracles-project-X.Y.Z-linux-x86_64.tar.gz` | a folder with `the-oracles-project` and the `libSDL3.so.0` it loads beside it (run path `$ORIGIN`), the licences, a `README.txt`, the icon and a `.desktop` file to install by hand; built on Ubuntu 22.04, the oldest runner, for the widest glibc, by `tools/make_linux_tarball.sh` |
| `the-oracles-project-X.Y.Z-android.apk` | the Android application, signed with the release key |
| `SHA256SUMS.txt` | the checksums of the four others |

The notes come from `.github/release-notes.md`, the version put in: what the port is, and how to install each file. The desktop files are not signed by a developer and the macOS application is not notarised: the notes say how Windows' SmartScreen and macOS' first opening let them through. What changed goes into the Release by hand afterwards, if at all. Every `README.txt` comes from `.github/release-readme.txt`.

Run by hand (Actions, Release, Run workflow), the workflow is a rehearsal: it builds the same five files from the chosen branch and keeps them seven days as an artifact, `the-oracles-project-X.Y.Z-rehearsal`, without releasing; download it to try each file before tagging. The Android key lives in the repository's secrets, never in the code: `ORACLES_KEYSTORE_BASE64`, the keystore file in base64, and `ORACLES_KEYSTORE_PASSWORD`; `ORACLES_KEY_ALIAS` and `ORACLES_KEY_PASSWORD` only when they differ from `oracles` and the keystore's password. The Linux archive can be made locally from a build with the pinned SDL 3 (`patchelf` installed):

```bash
cmake -S . -B build -DORACLES_SDL3_FROM_SOURCE=ON && cmake --build build --parallel --target oracles
./tools/make_linux_tarball.sh build dist      # dist/the-oracles-project-X.Y.Z-linux-x86_64.tar.gz
```

To build a signed APK locally:

```bash
ORACLES_KEYSTORE=/path/to/release.jks ORACLES_KEYSTORE_PASSWORD=... ./android/build_apk.sh release
```

writes `android/build-apk/the-oracles-project-release.apk`; without `ORACLES_KEYSTORE` the APK is built unsigned (`the-oracles-project-release-unsigned.apk`). Under WSL the variables reach Windows through `WSLENV`, the keystore's path translated; none is written to disk. Android installs an update only when it is signed by the same key: create it once and keep it with a backup, for instance with Android Studio's `keytool`:

```bash
keytool -genkeypair -keystore release.jks -alias oracles -keyalg RSA -keysize 4096 -validity 10000
```

The licences of the embedded fonts, of SameBoy and of SDL 3 are in the APK under `assets/licences/`, and beside the desktop executable.
