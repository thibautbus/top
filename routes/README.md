# Routes

A route is a recording of a player's inputs, frame by frame, with the header that says which game, which ROM (by SHA-1) and which options it was played with; its starting save sits beside it (`.route.sram`, a player's save, no data of the ROM). Replayed through the harness, a route drives the game exactly as it was played, and the checks of its directory's `checks.tsv` say what must hold on every replay.

The format, the harness and the suite are described in [`docs/ROUTES.md`](../docs/ROUTES.md).

## Replaying the suite

Put your ROMs in one directory and name it in `ORACLES_ROM_DIR`; the routes find their ROM by its SHA-1. A fan game's route names the SHA-1 of the patched image: put the game's BPS patch beside the base ROM it applies to, and the suite applies it, or put the patched image (a `.gbc`) there. Then, from a build of the repository:

```
ORACLES_ROM_DIR=/path/to/roms ctest --test-dir build -R '^oracles-routes$'
ORACLES_ROM_DIR=/path/to/roms python3 tools/check_routes.py --harness build/oracles-harness
```

Without `ORACLES_ROM_DIR` the test is skipped. `tools/check_routes.py` reads every `checks.tsv` under `routes/`, and replays on SameBoy unless given `--core mgba` (`oracles-routes-mgba` in `ctest`), each row saying the core it holds on; `--only TEXT` replays the rows whose route or label contains the text (`ages/intro`, `seasons-dungeon-enhanced-zoom`).

## The routes

The routes recorded before SameBoy's joypad bouncing was cut (`ages/advanced`, `ages/dungeon-sideview`, `ages/overworld`, `ages/underwater`, `seasons/continuous-transitions`, `seasons/hotkeys-equip`, `seasons/lost-woods`, `seasons/neighbour-objects`, `seasons/overworld`) were rewritten by `tools/debounce_route.py`: in the frames the game read the keys, their keys are those it read on SameBoy, bouncing included; in the others, those recorded. Each replays the same game without the bouncing, and keeps its recording as `#recorded` comments. `ages/overworld-continuous.route` is the same session's keys as the game read them with `--continuous-transitions`.

| Route | Covers | Modes |
| --- | --- | --- |
| `ages/overworld.route` | the native renderer on Ages: Lynna, interiors, warps | faithful, faithful-cc, enhanced, enhanced-threaded, enhanced-zoom, enhanced-zoom-threaded |
| `ages/overworld-continuous.route` | the same session as the game plays it with `--continuous-transitions` (its keys as the game read them then: `tools/debounce_route.py`) | enhanced |
| `ages/advanced.route` | the past, the sea and the rooms under water, a dungeon of the past | faithful, faithful-cc, enhanced, enhanced-threaded |
| `ages/dungeon-sideview.route` | a dungeon of the present and its side-view section | faithful, faithful-cc, enhanced, enhanced-threaded |
| `ages/underwater.route` | under water | faithful, faithful-cc, enhanced, enhanced-threaded, enhanced-zoom, enhanced-zoom-threaded |
| `ages/intro.route` | Ages' introduction and cutscenes in the drawn-back view | faithful, enhanced-zoom, enhanced-zoom-threaded |
| `ages/hotkeys-use.route` | item hotkeys in `use` mode, a session equal to its replay | faithful, enhanced, enhanced-threaded, enhanced-zoom |
| `ages/hotkeys-use-set.route` | `ages/hotkeys-use.route` with the item hotkeys turned off and on again by `set item-hotkeys` lines, written by hand: notes, the game and the Enhanced run unchanged | faithful, enhanced, enhanced-threaded |
| `ages/zoom-out.route` | the drawn-back view on Ages: dialogues, time travel, interiors | faithful, enhanced-zoom, enhanced-zoom-threaded |
| `ages/mods-claw-game.route` | Lua mods: a house, its characters, a minigame, `mod.storage`, the save | faithful, enhanced |
| `ages/swim.route` | the continuous transitions through a swim: the sea with the mermaid suit, a dive and the sea floor, an underwater building | enhanced-zoom, enhanced-zoom-threaded |
| `ages/mermaid-hold.route` | the swim's hold with the mermaid suit: `ages/underwater.route`'s start, then directions held on the sea floor, written by hand; with `--continuous-swim --continuous-diagonal`, the diagonal on foot, the route staying on land | faithful, enhanced |
| `ages/mermaid-hold-set.route`, `ages/mermaid-hold-window.route` | `ages/mermaid-hold.route` with `set` lines, as the pause menu writes them, written by hand: the swim's hold off at 3060 and on again at 3140, no stroke started between; the window at 3x, fullscreen and 2x, the game and the Enhanced run unchanged | faithful, enhanced, enhanced-threaded |
| `ages/seafloor-diagonal.route` | the transitions' diagonal on the sea floor with the mermaid suit, tapped and held with the swim's hold, across the edges of 2:b7, 2:b6, 2:a6 and 2:a7, written by hand | faithful, enhanced |
| `ages/diagonal-crossings.route` | the transitions' diagonal on foot: the edges of 0:6c, 0:6b and 0:5c crossed diagonally, walls across included, written by hand | faithful, enhanced, enhanced-threaded, enhanced-zoom |
| `ages/swim-diagonal.route` | the transitions' diagonal swimming with the mermaid suit across the edge between 0:c4 and 0:b4, written by hand | faithful, enhanced, enhanced-threaded, enhanced-zoom |
| `ages/stairs-diagonal.route` | Link's sprite steady on the screen on stairs and walking diagonally, at every size of the view and with both camera profiles, written by hand | faithful, enhanced, enhanced-threaded, enhanced-zoom |
| `ages/save.route`, `ages/load.route` | the game's own save, written by hand after `ages/overworld.route`'s keys: the same cartridge RAM on both cores, and the file opened the same on both | faithful |
| `ages/shop.route` | a purchase: `ages/overworld.route`'s keys into Lynna's shop, then hearts bought for 10 rupees, written by hand; with `--cheats rupees` it costs nothing; with `--text compact`, its price and its choice drawn in the font's own widths, which keep their layout | faithful; enhanced with `--text compact` |
| `ages/compact-lynna.route` | the compact text: `ages/load.route`'s keys open the file before Lynna's shop, then the cook's dialogue (A, each press followed by a tap to the right that turns Link to the fence once the text is closed, whatever its pages), the sign of the Maku Tree, the inventory, and in the shop the bombs' name, price, choice and the shopkeeper's answer, written by hand so that its keys hold whatever the language and the pages (its SRAM `ages/load.route`'s): English (US) laid out again, lines of more than sixteen characters | enhanced with `--text compact` |
| `ages/shop-cheat.route`, `ages/shop-cheat-off.route` | `ages/shop.route`'s keys with `cheat` lines, written by hand: the cheat of the rupees turned on at frame 6228, the frame the purchase is paid at, which costs nothing, then off; on from the start, turned off at that frame, which costs 10 | faithful |
| `seasons/overworld.route` | Enhanced on Seasons: Holodrum, vertical scrolls, interiors, a dungeon | faithful, faithful-cc, enhanced, enhanced-threaded, enhanced-zoom, enhanced-zoom-threaded |
| `seasons/continuous-transitions.route` | continuous transitions, Subrosia, white fades, large rooms | faithful, faithful-cc, enhanced, enhanced-threaded |
| `seasons/neighbour-objects.route` | the neighbouring rooms' objects handed over to the live room | faithful, enhanced |
| `seasons/lost-woods.route` | rooms whose transitions the game routes itself | faithful, enhanced, enhanced-threaded, enhanced-zoom, enhanced-zoom-threaded |
| `seasons/hotkeys-equip.route` | item hotkeys in `equip` mode, the A target and its variants | faithful, enhanced, enhanced-threaded |
| `seasons/scroll-animation.route` | animated tiles during a scroll | faithful, enhanced-zoom, enhanced-zoom-threaded |
| `seasons/intro.route` | Seasons' introduction and cutscenes in the drawn-back view | faithful, enhanced-zoom, enhanced-zoom-threaded |
| `seasons/dungeon.route` | a dungeon in the drawn-back view, large rooms whole | faithful, enhanced-zoom, enhanced-zoom-threaded |
| `seasons/dungeon-diagonal.route` | `seasons/dungeon.route`'s keys into 4:57, then six diagonals across the large room, written by hand: Link drawn at his exact place there with the sub-pixel camera | faithful, enhanced, enhanced-zoom |
| `seasons/swim.route` | the continuous transitions through a swim with the flippers, strokes carrying Link across an edge | faithful, enhanced-zoom, enhanced-zoom-threaded |
| `seasons/swim-hold.route` | the swim's hold with the flippers: `seasons/swim.route`'s start, Link entering the water, then a direction held alone, with A, and nothing, written by hand (SameBoy only) | faithful, enhanced-zoom |
| `seasons/shop.route` | a purchase: `seasons/overworld.route`'s keys to Horon's shop, then a bomb dropped and its blast taken outside, and ten bombs bought for 20 rupees, written by hand; with the cheats the blast costs no heart, the purchase nothing, and the bomb is kept; with `--text compact`, its price and its choice drawn in the font's own widths, which keep their layout | faithful; enhanced with `--text compact` |
| `seasons/diagonal-crossings.route` | the transitions' diagonal on foot: the edges of 0:e7, 0:e8, 0:d7 and 0:d8 crossed diagonally in the eight ways, written by hand | faithful, enhanced, enhanced-threaded |
| `seasons/stairs-diagonal.route` | Link's sprite steady on the screen on stairs and walking diagonally, at every size of the view and with both camera profiles, written by hand | faithful, enhanced, enhanced-threaded, enhanced-zoom |
| `moonrise-regalia/exploration.route` | a fan game from its BPS patch: a new game, continuous transitions | faithful, faithful-cc, enhanced, enhanced-threaded, enhanced-zoom, enhanced-zoom-threaded; with `--continuous-diagonal`, faithful and enhanced |
| `temple-of-seasons/subrosia-and-dungeon.route` | Temple of Seasons: Subrosia, its north and a dungeon, with continuous transitions, the drawn-back view and dialogues | faithful, faithful-cc, enhanced, enhanced-threaded, enhanced-zoom, enhanced-zoom-threaded; with `--continuous-diagonal`, faithful and enhanced |
| `gifts-of-kinomi/castle-and-west.route` | Gifts of Kinomi: the starting castle, its way out and the west, in the drawn-back view with continuous transitions | faithful, faithful-cc, enhanced, enhanced-threaded, enhanced-zoom, enhanced-zoom-threaded; with `--continuous-diagonal`, faithful and enhanced |
| `moonrise-regalia/compact-exploration.route` | the compact text on Moonrise: `moonrise-regalia/exploration.route`'s keys moved for the three dialogues the compact text closes in fewer boxes (each dialogue's keys up to the press that closes it, the directions held then, every later key 108 frames sooner in all), the same 78 rooms; Moonrise's English laid out again, lines of more than sixteen characters | enhanced, enhanced-threaded (recorded with `text-compact`) |
| `temple-of-seasons/compact-subrosia-and-dungeon.route` | the compact text on Temple of Seasons: `temple-of-seasons/subrosia-and-dungeon.route`'s keys moved the same way for the seven dialogues the compact text closes sooner (197 frames in all), the same 27 rooms; the spirits' texts in their colours | enhanced, enhanced-threaded (recorded with `text-compact`) |
| `ages/text-instant-shop.route` | the instant text (`--text-speed instant`): `ages/shop.route`'s keys moved for its two texts, each page advanced by A at the frame it waits, the price and the choice whole at once, the keys after a text those of `ages/shop.route` from the frame it closed, written by a tool over the replay | faithful (recorded with `text-instant`) |
| `ages/text-instant-compact-lynna.route`, `ages/text-instant-lynna-wide.route` | the instant text with the compact text, and with the wide text box: `ages/compact-lynna.route`'s keys moved the same way, the cook's dialogue, the sign, the inventory, the shop's price, choice and answer | enhanced (recorded with `text-compact` or `text-box-wide`, and `text-instant`) |
| `moonrise-regalia/text-instant-compact-exploration.route` | the instant text on Moonrise, its pages compact: `moonrise-regalia/compact-exploration.route`'s keys moved the same way for its first seven dialogues and 44 rooms, cut where a later room parts | enhanced (recorded with `text-compact` and `text-instant`) |
| `temple-of-seasons/text-instant-compact-subrosia-and-dungeon.route` | the instant text on Temple of Seasons, its pages compact: `temple-of-seasons/compact-subrosia-and-dungeon.route`'s keys moved the same way, the same 27 rooms, a character's voice kept (the line stopped a frame on it) and a `\slow` text the game holds letter by letter | enhanced (recorded with `text-compact` and `text-instant`) |
| `gifts-of-kinomi/compact-castle-and-west.route` | the compact text on Gifts of Kinomi: `gifts-of-kinomi/castle-and-west.route`'s keys, Link waiting with no key from the frame the compact text closes the princess's dialogue at the castle to the frame the game's layout did (90 frames), every later key at its frame, the same 28 rooms at the same frames | enhanced, enhanced-threaded (recorded with `text-compact`) |

The modes: `faithful` checks the native renderer against the core and the ghost instance against every scrolling transition; `faithful-cc` the renderer with colour correction; `enhanced` and `enhanced-zoom` the Enhanced view at its two sizes, the ghost synchronous, with a run hash; the `-threaded` modes the same with the ghost in its thread, as played. For every route, the live state is compared with and without the Enhanced view.
