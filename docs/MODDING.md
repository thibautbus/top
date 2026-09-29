# Writing a mod in Lua

A mod adds houses, characters who talk and minigames to the game, written in Lua. It does not change the user's ROM: the port composes its houses into a copy in memory at launch, and the mod asks the game to run its own routines (give an item, play a sound) instead of writing its memory. A game with mods replays exactly, like every route of the port. The API may still change.

Two examples come with the repository; a player copies each folder as it is into the launcher's mods folder (see Running):

- [`mods/claw-game/`](../mods/claw-game/): a house in Lynna City (Ages) and in Horon (Seasons), its keeper and the claw game of Link's Awakening (`main.lua`, `claw.lua`, `sprites.lua`);
- [`mods/fortune-teller/`](../mods/fortune-teller/): a second house, on the island east of Lynna City's red house, whose fortune teller remembers how many times Link came (`mod.storage`); loaded with the claw game, it exercises several mods together.

## Running

From the launcher: copy the mod's folder into the **mods folder**, beside the launcher's `settings.txt` (`~/.local/share/the-oracles-project/mods/` on Linux, `~/Library/Application Support/the-oracles-project/mods/` on macOS, `%APPDATA%\the-oracles-project\mods\` on Windows; the Mods page's **Open folder** makes it and opens it). Open the game's **Mods** page (Ages or Seasons), switch the mod on, and **Play**. The page lists every folder of the mods folder, in the order of their names, with the games the mod adds houses to and its description (`mod.description`), or `Refused: ` and the reason the loader gives, the same the command line prints; eight mods at most are active, and [`PLAYING.md`](PLAYING.md#mods) says the rest.

From the command line:

```
the-oracles-project --rom ROM --mods mods/claw-game
the-oracles-project --rom ROM --mods mods/claw-game --mods mods/fortune-teller     # several mods, eight at most
the-oracles-project --rom ROM --mods mods/claw-game --start-at-house claw_game     # every file resumes in front of the door
```

- Mods need **the game's US ROM**: Ages (SHA-1 `880374fb978b18af4aa529e2e32f7ffb4d7dd2f4`) or Seasons (SHA-1 `ba1268290fb2b1b70505d2d7b5825fc8a4816a4b`). On another ROM, a mod that declares a house is refused with the reason.
- A game with mods has **its own save**: `NAME.mods.sav` beside the ROM, copied from `NAME.sav` the first time. A game with mods never writes the save without mods. `--save` chooses another file.
- `--start-at-house NAME` (or `MOD/NAME` when two mods have a house of that name) makes every file of the save resume in front of the house's door, facing it. Only the SRAM loaded changes; the file changes only if the game saves. The save needs a file already.
- The terminal says what each mod does: its loading (`mod claw-game@… loaded`), its houses (`house claw_game in 0/55 (door 68)…`), its conversations, what `print` writes, and why a mod stopped.

## A mod

A mod is a directory named with `a-z`, `0-9`, `-` and `_`. `main.lua` declares what the mod adds; the other `.lua` files are modules it loads with `require("name")` (the file `name.lua`, run once). A whole mod fits in a few lines:

```lua
mod.house("fortune", { ages = { room = "0/46", col = 6, row = 3 } })

mod.npc("teller", { house = "fortune" }, function(talk)
  mod.storage.visits = (mod.storage.visits or 0) + 1
  if talk:ask("Your fortune for 5 Rupees?", { "Yes", "No" }) == "No" then
    return talk:say("The future can wait.")
  end
  if not game.pay(5) then return talk:say("Your purse is empty.") end
  talk:say("Visit " .. mod.storage.visits .. ": Rupees come to those who wait.")
end)
```

`main.lua` runs once, at launch, before the game runs: it declares, it does not play. The conversations run afterwards, during the game.

`mod.description("A fortune teller who remembers each of Link’s visits")` gives the line the launcher's Mods page shows under the mod's name: one line, 100 bytes at most, given once. A mod without one shows an empty line. The launcher loads `main.lua` without a game to list the mod, for Ages and for Seasons: what it declares tells the page which games have its houses, and an error there is the refusal the page shows.

## Houses

```lua
mod.house("claw_game", {
  ages = { room = "0/55", col = 7, row = 4 },
  seasons = { room = "0/e9", col = 4, row = 2 },
})
```

- A house is declared for each game it exists in; a game it does not name has no house (and its keeper waits for the other game).
- The house is a facade of three by three metatiles placed in a room of the world, `0/RR`: Ages' present, Holodrum in Seasons. `RR` is the room's number **in hexadecimal**: the first digit is the map's row, the second its column (`0/45` is just above `0/55`, `0/46` just right of `0/45`). A room is ten metatiles by eight.
- In Seasons the facade is placed in the room's four seasons: look at each (Horon changes season at every visit).
- The facade is drawn with the metatiles of the town rooms (tileset `$00`, Lynna's and Horon's): a room of another tileset is refused, its metatiles drawing something else.
- `col` (0 to 7) and `row` (0 to 4) place the facade's top-left corner. The door is in the middle of its bottom row; the metatile in front of the door becomes path. **The facade and the cell in front of the door must fit in the room.**
- Inside, a counter, and the keeper behind it. Link talks to him standing in front of the counter, facing him, and pressing A. The way out is at the bottom.
- The port refuses a room whose interior index the game already uses, two houses in the same room (even in two mods), and more houses than the game has room for (about seventeen in all in Ages). It checks that every warp of the game still leads where it did. It **does not check** that the facade leaves the paths open: a facade can block a bridge or cut an island. Look at the room with `--start-at-house` before publishing.
- Four houses at most per mod. For now, one facade, one interior and one keeper's appearance per game: a villager in Ages, Mr. Write's look in Seasons. The game's own characters stay where the game puts them: a facade placed on a passer-by's spot shows him in the wall.

## Characters

```lua
mod.npc("keeper", { house = "claw_game" }, function(talk) ... end)            -- the keeper of a house of the mod
mod.npc("plen", { existing = { ages = "3/f8" } }, function(talk) ... end)     -- a character of the game, by its room
```

- A house has one keeper.
- `existing` names a character of the game by the room it stands in (`G/RR`, group and room in hexadecimal): its conversation starts when a text of the game closes in that room. Two mods cannot speak for the same character.
- During a conversation the game waits: Link does not move, and the mod draws over the screen. One conversation at a time, all mods together.

## The conversation

| Call | Effect |
| --- | --- |
| `talk:say(text)` | the text in the game's box, two lines a page; A or B completes the page, then closes it |
| `talk:ask(question, { "Yes", "No" })` | the question, then the options on one line; returns the option chosen; left and right move the cursor, A chooses, B chooses the last |
| `talk:random(n)` | an integer from 1 to n (see the sandbox) |
| `talk:play(scene, parameters)` | runs a scene (a minigame) and returns its result |

- A line of the box holds **16 characters**. The text is cut at spaces; `?`, `!`, `:` and `;` stay with the word before. Letters appear one a frame, as in the game.
- The options of `ask` hold on one line, two spaces between them: 16 characters in all, or the call is refused with the length. A long question is cut into pages, its last line staying with the options.
- The font is the game's: letters, digits, ASCII punctuation and `ÀÂÄÆÇÈÉÊËÎÏÑÖŒÙÛÜàâäæçèéêëîïñöœùûü` (`ROM_DATA_FORMATS.md`, section 8.1). There is no `ô`, no `«»`, no `…`: a missing character stops the mod, naming it.

## Scenes: a minigame

A scene is a table of functions. `talk:play` makes a fresh instance of it, calls `start` once, then `update` and `draw` once a frame (60 a second) until `update` returns a value:

```lua
local scene = {}

function scene:start(ctx)
  self.x = 10                     -- ctx.params: the parameters of talk:play
end                               -- ctx.random(n), ctx.width (160), ctx.height (144)

function scene:update(input)      -- nil: go on; a value: the scene ends and talk:play returns it (false: nothing)
  if input.held.right then self.x = self.x + 1 end
  if input.pressed.a then return self.x end
end

function scene:draw(g)
  g:clear("#000000")
  g:rect(self.x, 60, 16, 16, "#f8f8f8")
  g:sprite("claw_open", self.x, 20)
  g:text("A: stop", 8, 0)
end

return scene
```

- `input.held` (held), `input.pressed` (pressed this frame), `input.released` (released this frame): `a`, `b`, `start`, `select`, `up`, `down`, `left`, `right`.
- The screen is 160x144 pixels; what the scene draws covers the game's screen. With the Enhanced view it is centred, and the rest of the screen darkens when the scene covers everything.
- `g:clear(colour)`, `g:rect(x, y, width, height, colour)`, `g:sprite(name, x, y, mirror)`, `g:text(text, x, y, colour)`; a colour is written `"#rrggbb"`. The characters of `g:text` are 8x16 pixels; the game's text box takes lines 100 to 140.
- `mod.sprite(name, { palette = { k = "#101010", y = "#c8c8d0" }, pixels = [[ ... ]] })`, in `main.lua` or a module: a character per pixel, `.` and space transparent, the others from the palette. It returns the size, `{ width = w, height = h }`.
- A scene may act on the game (`game.play_sound`, `game.give`...) in `update`.

## Acting on the game

The mod asks the game to run its own routines, at the end of the frame, as the game itself would: the counters, the sounds and the status bar follow. The names are the [disassembly](https://github.com/Stewmath/oracles-disasm)'s (`constants/common/`), in lower case and without prefix.

| Call | Effect |
| --- | --- |
| `game.name` | `"ages"` or `"seasons"` |
| `game.rupees()` | Link's rupees, less what `game.pay` has already taken |
| `game.pay(n)` | takes n rupees if he has them (`true`), else nothing (`false`); n among the game's amounts: 1, 2, 5, 10, 20, 25, 30, 40, 50, 60, 70, 80, 100, 150, 200, 300, 400, 500, 900, 999 |
| `game.give("rupees", n)`, `game.give("heart")`, `game.give("seeds", n)`, `game.give("gasha_seed")`, `game.give("ring", "power_ring_l1")` | the usual prizes (a heart: life refilled; seeds: ember seeds); `false` when Link cannot receive them (no satchel, no ring box) |
| `game.has(name)` | whether Link has this treasure (`"seed_satchel"`, `"ring_box"`...) |
| `game.give_treasure(name, parameter)` | `giveTreasure`, for a treasure of `treasure.s` (without `TREASURE_`) |
| `game.lose_treasure(name)` | `loseTreasure` |
| `game.play_sound(name)` | `playSound`: `snd_getseed`, `snd_error`, `mus_minigame`... (`music.s`) |
| `game.flag(name)`, `game.set_flag(name)`, `game.unset_flag(name)` | the game's global flags (`globalFlags.s`, without `GLOBALFLAG_`) |

- `game.give_treasure` accepts only a parameter whose effect the game bounds, by the way it receives that treasure (`treasureCollectionBehaviours.s`). A level or a companion (sword, shield, bracelet, flute...) is given only with a value the game itself gives that treasure (`treasureObjectData.s`: the bracelet 1 or 2, the flute `0x0b` to `0x0d`); life, 20 hearts at most (`0x50`). It refuses, saying why: dungeon items (compass, map, keys), which would write outside their table outside a dungeon; unbounded counters (the satchel's upgrade); values the game would store as they are; maximum life; treasures without a name.
- Names are checked in `main.lua`, so that a typo in a rare branch is not found by playing it: `local JINGLE = game.sound("snd_solvepuzzle")`, `game.treasure("gasha_seed")`, `game.global_flag("…")` return the name, or stop the loading with the file and the line.
- The global flags are the game's: a mod that changes one changes the game. To keep its own data, a mod has `mod.storage`.
- In `main.lua` the game is not running yet: `game.pay` and the other calls to the game are refused there ("the game is not running yet").

## Keeping data: `mod.storage`

`mod.storage` is a table saved with the game's file, one per file (the three of the file select):

```lua
mod.storage.visits = (mod.storage.visits or 0) + 1
```

- It follows the game's operations on its files: empty for a new file, filled with what the file held when the player loads it, written when the game saves, copied with the file, emptied when it is erased. What changes without being saved is lost, as in the game.
- It holds booleans, numbers, strings and tables of those, with integer or string keys: 16 KiB once written, sixteen levels of tables. Anything else (a function, a key `1.5`, `0/0`) stops the mod when the game saves, with the reason; what the file held stays.
- It is read and written in the conversations; in `main.lua` no file is loaded yet. Keep the table itself (`mod.storage`): the port fills it in place.
- The port writes it to `NAME.mods.store`, beside the save; a route carries it in `ROUTE.store`, a savestate with it.

## Several mods together

Mods are taken in the order of their names, whatever the order of `--mods`. Each sees the player's keys; one conversation at a time. Refused, with the names: two mods of the same name, two mods speaking for the same character of the game, two houses in the same room.

## Testing and replaying

- `the-oracles-project --rom ROM --mods MOD --record R.route` records the game: `R.route` (the keys), `R.route.sram` and `R.route.store` (the save and the storage it starts from), `R.route.mod.tsv` (the mods' state frame by frame). `the-oracles-project --rom ROM --mods MOD --play R.route` replays it, then gives the controls back; a replay never writes the save.
- A route names the mods and their version (`mods NAME@SHA1,…`, the hash of all their files, line endings brought back to LF) and the storage it starts from (`store_sha1`): it replays only with the same files and the same `R.route.store`. Changing a mod's files, even a comment, changes its identity: record its routes again, or, when the change cannot change the game (a comment), update the identity in the route's header and prove with `--compare-session` that the session and its replay are the same game.
- Without a window, the harness replays a route and compares: `oracles-harness --rom ROM --route R.route --mods MOD --mod-trace M.tsv --summary S.txt`, then `oracles-harness --compare R.route.mod.tsv M.tsv`. The summary counts, per mod, the conversations, the calls to the game and the errors (`mod.NAME.conversations`, `mod.NAME.errors`...). The route suite resolves a `--mods` directory of a row's options against the repository ([`ROUTES.md`](ROUTES.md)).
- The route format is in [`ROUTES.md`](ROUTES.md); the key mask is `01` right, `02` left, `04` up, `08` down, `10` A, `20` B, `40` Select, `80` Start.
- `print(...)` writes to the terminal.

## When a mod stops

An error (Lua, unknown name, missing character, refused call, budget exceeded) stops the mod, never the game. The message names the mod's file and line and goes to the terminal; a red band "NAME stopped" shows a few seconds in the game. The mod stays stopped until the next launch; the others go on. An error in `main.lua` refuses the mod at launch.

## The sandbox

A mod replays exactly: a route recorded with it gives the same game again. Hence:

- no files, no clock, no network: no `io`, `os`, `load`, `dofile`, `package`, `debug`, `coroutine`, `pcall`, `collectgarbage`, `rawset`;
- no `math.random`: `talk:random(n)` and `ctx.random(n)` draw from a generator seeded by the game's frame and counter at the start of the conversation;
- `pairs` and `next` walk the keys in order: numbers, strings, then `false`, `true`; a table with a table or a function as a key is refused;
- `tostring` and `string.format("%s")` of a table or a function show no address; no `%p`, `__gc` or `__mode`;
- two million Lua instructions a frame at most (fifty million for `main.lua`), the work of the string and table functions counted with them, and what the mod allocates (an instruction per 16 bytes: copying a string of a megabyte counts); 32 MiB of memory; a costly search pattern is refused;
- a table that has its own metatable may serve as a metatable (inheritance), not one whose metatable is hidden (`__metatable`);
- no savestate during a conversation; a savestate loaded ends the one in progress.

## Current limits

- The US ROMs only, and not the fan games.
- One model of house and keeper; no facade, interior or appearance chosen by the mod yet.
- No map of the rooms: a room is chosen by its number and looked at with `--start-at-house`.
- Routes are written by playing (`--record`); there is no tool yet for routes relative to events ("A when the box is full").
