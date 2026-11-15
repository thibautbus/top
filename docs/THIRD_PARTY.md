# Licences

The Oracles Project is under the MIT licence. The launcher builds in, or ships beside its executable, the work of other projects, each under its own licence, listed here with the file that holds it. The launcher's **Settings > About > Licences** opens this page.

## The launcher

| What | Licence | File |
| --- | --- | --- |
| The Oracles Project: the launcher, the engine, the mods' interface | MIT | [`LICENSE`](../LICENSE) |

## The emulator cores

| What | Licence | File |
| --- | --- | --- |
| [SameBoy](https://sameboy.github.io/), by Lior Halphon: the Accurate core, and the free boot ROMs both cores start from | Expat (MIT) | [`third_party/sameboy/LICENSE`](../third_party/sameboy/LICENSE) |
| [mGBA](https://mgba.io/)'s Game Boy core, by Jeffrey Pfau and the mGBA contributors: the Fast core, with one change, [`boot-rom.patch`](../third_party/mgba/boot-rom.patch) | Mozilla Public License 2.0 | [`third_party/mgba/LICENSE`](../third_party/mgba/LICENSE), and the notice that says where its source is, [`third_party/mgba/NOTICE.txt`](../third_party/mgba/NOTICE.txt) |
| [inih](https://github.com/benhoyt/inih), by Ben Hoyt, which mGBA uses | BSD 3-Clause | [`third_party/mgba/core/src/third-party/inih/LICENSE.txt`](../third_party/mgba/core/src/third-party/inih/LICENSE.txt) |

## The libraries

| What | Licence | File |
| --- | --- | --- |
| [SDL 3](https://libsdl.org/): the window, the sound, the controllers, built from the release `config/sdl3.json` pins | zlib | [`LICENSE.txt`](https://github.com/libsdl-org/SDL/blob/main/LICENSE.txt) of SDL's repository, shipped as `SDL3-LICENSE.txt` |
| [Lua](https://www.lua.org/): the mods' runtime | MIT | [`third_party/lua/LICENSE`](../third_party/lua/LICENSE) |
| [stb_truetype](https://github.com/nothings/stb): the launcher's text | MIT or public domain, as you choose | [`third_party/stb/LICENSE`](../third_party/stb/LICENSE) |
| [NanoSVG](https://github.com/memononen/nanosvg): the launcher's motifs | zlib | [`third_party/nanosvg/LICENSE.txt`](../third_party/nanosvg/LICENSE.txt) |
| [rcheevos](https://github.com/RetroAchievements/rcheevos): RetroAchievements, in the builds that have them | MIT | [`third_party/rcheevos/LICENSE`](../third_party/rcheevos/LICENSE), shipped as `rcheevos-LICENSE.txt` |

## The fonts

| What | Licence | File |
| --- | --- | --- |
| [Alegreya Sans](https://github.com/huertatipografica/Alegreya-Sans) | SIL Open Font License 1.1 | [`third_party/fonts/alegreya-sans/OFL.txt`](../third_party/fonts/alegreya-sans/OFL.txt) |
| [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono) | SIL Open Font License 1.1 | [`third_party/fonts/jetbrains-mono/OFL.txt`](../third_party/fonts/jetbrains-mono/OFL.txt) |
| [Marcellus](https://fonts.google.com/specimen/Marcellus) | SIL Open Font License 1.1 | [`third_party/fonts/marcellus/OFL.txt`](../third_party/fonts/marcellus/OFL.txt) |

## Each platform's own

| What | Where | Licence | File |
| --- | --- | --- | --- |
| libwinpthread, of the [MinGW-w64](https://www.mingw-w64.org/) project: the threads | Windows | MIT (the mingw-w64 project), with parts derived from Lockless Inc.'s Posix Threads library under the BSD 3-Clause licence | the `COPYING` of MSYS2's package, shipped as `libwinpthread-COPYING.txt` |
| The AppImage's runtime | Linux's AppImage | MIT | [`launcher/linux/licences/AppImage-runtime-LICENSE.txt`](../launcher/linux/licences/AppImage-runtime-LICENSE.txt) |
| libfuse, which the runtime is built with | Linux's AppImage | LGPL 2.1 | [`launcher/linux/licences/libfuse-LICENSE.txt`](../launcher/linux/licences/libfuse-LICENSE.txt) |
| squashfuse, zstd, zlib, musl and mimalloc, which the runtime is built with | Linux's AppImage | each its own | in `launcher/linux/licences/`: [`squashfuse-LICENSE.txt`](../launcher/linux/licences/squashfuse-LICENSE.txt), [`zstd-LICENSE.txt`](../launcher/linux/licences/zstd-LICENSE.txt), [`zlib-LICENSE.txt`](../launcher/linux/licences/zlib-LICENSE.txt), [`musl-LICENSE.txt`](../launcher/linux/licences/musl-LICENSE.txt), [`mimalloc-LICENSE.txt`](../launcher/linux/licences/mimalloc-LICENSE.txt) |
| The Gradle wrapper, which builds the Android application | the Android source | Apache 2.0 | `android/gradle/wrapper/` |

Each download carries, beside the executable (the Android application in its assets, under `licences/`), the licences that ask for it: those of the cores, inih, Lua, SDL 3, rcheevos, the fonts and each platform's own. The versions are in each folder's `VERSION.md` and in `config/`.

Oracle of Ages and Oracle of Seasons are Nintendo's; this project is not affiliated with Nintendo. The fan games are their authors'.
