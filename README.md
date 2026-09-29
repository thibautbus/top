# The Oracles Project

A faithful, moddable port of Oracle of Ages and Oracle of Seasons for PC, macOS and Android. Your own ROM runs in an unmodified [SameBoy](https://sameboy.github.io/) core; around it, a native host observes the game and presents it:

- **Faithful**: the original image, 160x144, exactly what the game draws;
- **Enhanced**: a widescreen view with the neighbouring rooms, which the game itself computes in a second instance of the core, a smooth camera, and the status bar repositioned in a band of its own;
- **gameplay options**: continuous transitions (Link keeps walking through the scrolling transitions), item hotkeys (four keys that use or equip an item without the menu), and a drawn-back view that shows three rooms across and two down outdoors;
- **Lua mods**: houses, characters who talk and minigames, which act on the game through the game's own routines and replay exactly;
- **fan games** from their BPS patch, applied in memory to your ROM:
  - Gifts of Kinomi 1.1.2, built on Oracle of Ages;
  - Moonrise Regalia 1.0.6, built on Oracle of Ages;
  - Temple of Seasons 1.073, built on Oracle of Seasons.

Nothing of the game's code is rewritten: the game runs as it is, and the host reads its state. [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) says why.

## Quick start

On Linux, with a C11 compiler, CMake 3.19 or later and the development packages SDL 3 builds against (or an installed SDL 3):

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Then run the launcher, and choose your ROM on its Cartridge page, or start a game directly:

```bash
build/the-oracles-project
build/the-oracles-project --rom "/path/to/Oracle of Ages.gbc" --enhanced
```

macOS, Windows (MSYS2) and Android are in [`docs/BUILDING.md`](docs/BUILDING.md); the launcher, the options, the keys and the settings in [`docs/PLAYING.md`](docs/PLAYING.md).

## Licence

The port is under the MIT licence ([`LICENSE`](LICENSE)). It vendors or embeds, each under its own licence, shipped with the binaries:

- [SameBoy](third_party/sameboy/) (Expat/MIT), the emulator core;
- [Lua](third_party/lua/) (MIT), the mods' runtime;
- [SDL 3](https://libsdl.org/) (zlib), built from the release pinned in `config/sdl3.json`;
- [stb_truetype](third_party/stb/) (MIT or public domain) and [NanoSVG](third_party/nanosvg/) (zlib), the launcher's text and motifs;
- the fonts [Marcellus](third_party/fonts/marcellus/), [Alegreya Sans](third_party/fonts/alegreya-sans/) and [JetBrains Mono](third_party/fonts/jetbrains-mono/) (SIL Open Font Licence);
- the Gradle wrapper of the Android build (Apache 2.0).

Oracle of Ages and Oracle of Seasons are Nintendo's; this project is not affiliated with Nintendo. The fan games are their authors'.
