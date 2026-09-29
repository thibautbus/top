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

