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

## What this repository does not contain

No ROM, no asset of the games, no data decompressed from a ROM, no code of the games. You provide your own ROM of Oracle of Ages or Oracle of Seasons (US); a fan game needs that ROM and the game's own BPS patch. The addresses and symbol names the host uses come from the [oracles-disasm](https://github.com/Stewmath/oracles-disasm) disassembly, at the commit pinned in `config/disasm.json`, through tables generated from its symbol files; the CI audits that no ROM byte enters the repository. Tests that need a ROM are skipped without one, never simulated.

## Platforms

- Linux and macOS (Intel and Apple Silicon), with SDL 3;
- Windows, built with MSYS2 UCRT64;
- Android 10 and later (`arm64-v8a`, `x86_64`), with touch controls, the same launcher and the same options.

The CI builds and tests all four on every pull request.

## Documentation

| Document | For |
| --- | --- |
| [`docs/BUILDING.md`](docs/BUILDING.md) | building on each platform, the options, the tests, releasing |
| [`docs/PLAYING.md`](docs/PLAYING.md) | the launcher, the fan games, the command line, the profiles and options, the settings |
| [`docs/MODDING.md`](docs/MODDING.md) | writing a mod in Lua |
| [`docs/ROUTES.md`](docs/ROUTES.md) | recorded routes, the differential harness, the route suite |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | the layers, the rules between them, the SameBoy strategy |
| [`docs/GAME_HOOKS.md`](docs/GAME_HOOKS.md) | what the host hooks in the game and where it may write |
| [`docs/ROM_DATA_FORMATS.md`](docs/ROM_DATA_FORMATS.md) | the formats of the ROM's data |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | the rules of the code and of a change |

## AI-assisted development

This repository was developed with AI assistance. Every change, whoever writes it, is held by the same checks: the tests the CI runs without a ROM, the route suite replayed on the games with its run hashes kept, the text and boundary checks, and a review before it is merged.

## Credits

- [oracles-disasm](https://github.com/Stewmath/oracles-disasm), the disassembly of both games by Stewmath and its contributors: the addresses and symbol names the host reads and hooks are generated from its symbol files.
- [SameBoy](https://sameboy.github.io/) by Lior Halphon: the emulator core the game runs in, unmodified.
- The fan games the port recognises, and their authors:
  - Gifts of Kinomi, by ZerotoKoops, Stewmath, Gamma and Ralfaro;
  - Moonrise Regalia, by PontiusStone;
  - Temple of Seasons, by Jay (like the bird, not the letter) and Ralfaro.

## Licence

The port is under the MIT licence ([`LICENSE`](LICENSE)). It vendors or embeds, each under its own licence, shipped with the binaries:

- [SameBoy](third_party/sameboy/) (Expat/MIT), the emulator core;
- [Lua](third_party/lua/) (MIT), the mods' runtime;
- [SDL 3](https://libsdl.org/) (zlib), built from the release pinned in `config/sdl3.json`;
- [stb_truetype](third_party/stb/) (MIT or public domain) and [NanoSVG](third_party/nanosvg/) (zlib), the launcher's text and motifs;
- the fonts [Marcellus](third_party/fonts/marcellus/), [Alegreya Sans](third_party/fonts/alegreya-sans/) and [JetBrains Mono](third_party/fonts/jetbrains-mono/) (SIL Open Font Licence);
- the Gradle wrapper of the Android build (Apache 2.0).

Oracle of Ages and Oracle of Seasons are Nintendo's; this project is not affiliated with Nintendo. The fan games are their authors'.
