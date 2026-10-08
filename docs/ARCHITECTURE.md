# Architecture

How the port is built: the layers, the rules between them, and why the game runs in an emulator core, SameBoy unmodified or mGBA's with one patch. The facts of the game's engine that the host relies on are in [`GAME_HOOKS.md`](GAME_HOOKS.md), the formats of its data in [`ROM_DATA_FORMATS.md`](ROM_DATA_FORMATS.md).

## The SameBoy strategy

The game runs unmodified in SameBoy, an emulator that aims at accuracy first. The Faithful profile on it is therefore exact by construction: it is the game, frame for frame, on the player's own ROM. The player may choose mGBA's Game Boy core instead (Display's Core, Fast), several times lighter, for small devices: every layer below runs on both cores through the same interface, SameBoy staying the reference, and the routes hold on both. Everything the port adds, the widescreen view, the camera, the options, the mods, is a host around that core that **observes** the game's state and presents it, instead of reimplementing the game's logic.

The project's first chain took the other road: a static recompilation of the ROM into C. Such a chain needs a generation step per ROM, and to be correct it must reproduce the machine's timing anyway. The game's vblank handler works to a budget of cycles and its LCD interrupts fire per line: moving an interrupt by a few cycles changes what the handler finishes, and an interrupt that lands inside a replaced routine pushes different bytes on the stack. The recompiled code ends up as an unrolled interpreter around a complete emulator, with the same observables as an emulator and more moving parts: generation, patches, private build identities. With an accuracy-first core those observables come for free, and replacing a routine by native code is neither needed nor, for whole subsystems, even possible cycle for cycle: what the port keeps equal is the observable state of each frame.

The consequence that matters most for a contributor: nothing is generated per ROM. A fan game, a BPS patch of an original, runs as soon as the port knows where its engine keeps the variables the host reads, which is a compatibility profile of addresses (see "Fan games" below). That is why the Enhanced view costs almost nothing on a fan game: the game still draws, loads and routes its rooms itself; the host only needs to know where to look.

The ghost instance follows from the same choice. The Enhanced view shows the neighbouring rooms, and a room's terrain depends on the game's substitutions (flags, seasons, the room's own code; `ROM_DATA_FORMATS.md`, section 2.4). Those are never reimplemented: a second instance of the core, loaded from a savestate of the live one, lets the game itself load the neighbour by its own scrolling transition, and the host reads the result.

## Layers

```text
the player's ROM (+ a fan game's BPS patch, + the mods' houses composed in memory)
        |
        v
SameBoy core, vendored, unmodified          <- runs the game; the only authority on gameplay
  (or mGBA's Game Boy core, one patch)       <- the same, lighter, when the player chooses Fast
        |            |               |
   framebuffer   direct access    callbacks (vblank, memory write, execution)
        |            |               |
        |     guest bus (bank + offset), entry and return hooks, register journal
        |            |
        |     differential harness: recorded routes, live-state fingerprints
        |            |
        v            v
native host
   Faithful: shows the core's framebuffer
   Enhanced: software PPU from the state committed at vblank, neighbours from a ghost instance,
             smooth camera, status bar placed in a wide band
   gameplay options: continuous transitions, item hotkeys (closed transactions of the bus)
   mods: Lua run during the game, acting through the game's own routines
        |
        v
host facade (window, input, audio, files) -> SDL 3; launcher

engine/game/data: readers of the ROM's data (rooms, tilesets, graphics), for the ghost's classification
                  of rooms and the mods' composition
```

The tree:

| Directory | What it holds |
| --- | --- |
| `engine/core`, `engine/rom` | the core's interface (`core.h`: memory, registers, hooks), its SameBoy and mGBA implementations (`core_sameboy.c`, `core_mgba.c`, the only files that see each core), the free boot ROM and composite savestates; the ROM loader, SHA-1 identification and BPS patches |
| `engine/game/guest` | the guest bus, the hooks, the register journal, the transactions, the fingerprints, the tables generated per game |
| `engine/game/profiles` | the profiles of the recognised images, and each fan game's manifest and generated table |
| `engine/game/data` | the ROM data readers and their oracle against the disassembly |
| `engine/render` | the software PPU and its per-class comparison with the core |
| `engine/ghost`, `engine/neighbours` | the ghost instance and its cache key; the objects, sprites and tile animation of a neighbour |
| `engine/enhanced` | the camera, the compositor and the Enhanced view |
| `engine/gameplay` | the continuous transitions and the item hotkeys, the only code that asks the bus to write |
| `engine/mods` | the Lua runtime, the houses' composition, the mods' storage |
| `engine/session`, `engine/host` | what a session shares between the launcher and the harness (routes, route effects, the hotbar's model, mods of a session); the host facade and its backend without a window |
| `launcher`, `android` | the SDL 3 launcher and its drawn interface; the Android application |
| `harness`, `tests`, `tools` | the differential harness; the tests without a ROM; the route suite and the checks |

## Principles

- **The core is the only authority.** The game runs in SameBoy, or in mGBA when the player chooses it; the host reads the game's state and never reimplements its logic. What needs a computation of the game (a neighbour's terrain with its substitutions) is asked of the game itself, in a ghost instance.
- **Faithful depends on the core alone.** The renderer, the hooks and the mods are layers above; the Faithful profile shows the core's framebuffer and stays playable if everything else is absent or off.
- **Reads by physical address, never through the guest CPU's bus.** The host reads the core's memory by bank and offset, through direct access; the CPU bus returns `$ff` on VRAM and OAM while the PPU draws and drops writes.
- **The presentation never writes into the game.** Enhanced never changes the live instance, and the route suite compares the live-state fingerprints with and without the view on every route. The only guest writes are the three closed transactions of the bus, at the safe points of [`GAME_HOOKS.md`](GAME_HOOKS.md), section 6: the continuous transitions (with `--continuous-swim`, through a swim too), the item hotkeys, and the mods' calls to the game's own routines.
- **One fact in one place.** An address or a format lives in the reference documents or in code generated from the pinned disassembly (`config/disasm.json`), resolved per game: Ages and Seasons do not share addresses.
- **Every comparison is intra-core.** Same core, same inputs, with and without hooks, with and without Enhanced; the fingerprints exclude the dead stack bytes below each thread's SP and nothing else.
- **Portability by construction.** No SDL, POSIX or system call outside the host facade and the launcher; no Python at run time.

## Dependency rules

```text
Faithful ------> core
Enhanced ------> core (read), bus, hooks, journal, engine/game/data     ; never the other way
mods ----------> the game's routines through the call transaction      ; never a byte of code
harness -------> core, bus, recorded routes                            ; depends on no presentation
ghost ---------> core, bus, engine/game/data (classification of rooms) ; never the live instance
engine/game/data -> ROM                                                ; depends on nothing else
```

## Addresses and hooks

Every address the host reads or hooks comes from tables generated from the symbol files of the pinned disassembly (`engine/game/guest/gen_guest_tables.py`, `engine/game/data/gen_tables.py`), never written by hand: `tools/check_guest_addresses.py` refuses a hexadecimal literal in the guest's address space outside the hardware map. The disassembly is the provenance of those names and addresses; no byte of the ROM is in the repository. The hooks are entry and return points of the game's functions and memory write callbacks on the core; which ones, what they read, and where a write is safe, is [`GAME_HOOKS.md`](GAME_HOOKS.md).

## The native renderer

`engine/render` is a software CGB PPU: it composes each frame from the state committed at vblank (VRAM, OAM, the PPU's palettes, and LCDC, SCY, SCX, WY, WX per line rebuilt from the journal of register writes), with the selection of ten objects per line, the priority by OAM index, the background/object priority and the window. Each vblank is classified (normal, LCD off, first frame after the LCD comes back, LCD behaviours 0 and 1, 5 and 6, a commit during the scan, no scan) and the native image is compared pixel for pixel with the core's framebuffer; frames committed during the scan are not compared, nor a line whose scroll was written late, while it was drawn (the core mixes both values there; the rest of the frame is compared). The Enhanced view draws the neighbours and the parts of the band outside the core's window with it.

## The ghost instance

`engine/ghost` is a second instance of the core, loaded from an in-memory savestate of the live one, in which the game loads a neighbouring room: the ghost forces the game's scrolling transition (bit 7 of `wScreenTransitionDirection`), runs until `initializeRoom` returns and a frame more, waits for the palette thread to rest, and delivers the committed map, tiles and palettes; Link is not moved. It runs synchronously with a budget of guest frames per call, or in a host thread; it never writes into the live instance. During its run it raises `wDisableWarpTiles` (a warp tile under Link would otherwise replace the forced scroll by a warp) and restores it before keeping its settled state.

Each rendered room is a cache entry that stays valid while the bytes of the cache key its substitutions actually read keep their value (`engine/ghost/ghost_key.c` lists the ranges and the reason for each, and the trace the harness runs refuses any read outside them): a chest opened three rooms away does not drop it; its own flag, or a global flag its tiles depend on, does. The cache is filled ahead, one run at a time, first the rooms the band shows, then those beyond; a room the game routes itself (the Lost Woods, Ages' scrambled forest) is asked of the ghost from the live state, direction by direction. Neighbours come from the map of groups 0 and 1, from the rooms that open onto each other in an interior, and from the floor's layout in a dungeon; the Maku tree's screen stands alone. A profile's named rules decide the exceptions (the seasons, Ages' sea, the tileset that places a room on its map).

## The Enhanced compositor

`engine/enhanced` composes the wide surface, the size of the view's level in the screen's shape (`compositor.h`): near 256x144, medium 384x216, far 480x270 (the drawn-back view, `--zoom-out`), or on a 4:3 screen near 213x160, medium 320x240 and far 480x360. A band of sixteen lines at the top holds the status bar copied from the core and centred; the world band below it holds the play area placed by a host camera in world coordinates, the neighbours from the ghost's cache around it. The camera is a smooth reducer (dead zone, bounded look-ahead, damping) fed from the guest bus with Link's world position; a second reducer drives the vertical axis. Large rooms (dungeons) are shown whole, centred; frames outside the world (menus, cutscenes, the file select) show the core's 160x144 image framed. Columns no terrain covers yet are black, never a wrong terrain.

## The neighbours' objects

What the band shows of a neighbour's objects, at one of two levels (`--enhanced-neighbours off|static`, `static` by default): `off`, the terrain alone; `static`, the objects in the state the game creates them in when Link enters the neighbour by the edge he would cross, captured during the ghost's terrain run and shown frozen. Objects whose state at creation would not be that of the real entry are not shown: those the room's list places at random (their position depends on the RNG and the edge of entry; `GAME_HOOKS.md`, section 4.3) and those another object creates.

### Who draws an object during an entry

From the frame the game decides a transition (T1) to the end of the scroll (Tfin), each object drawn has one source per frame:

- inside the core's window, the core draws the live objects; an object of the entered room that is still invisible (before its initialisation) is not there, and the view adds nothing inside the window;
- outside the window, the objects of the room left: an image of that room's sprites taken at the frame before the decision, until Tfin;
- outside the window, the entered room's objects: the capture, for the objects that are paired with a live one, at the live object's position once it has initialised, at the captured position before; nothing for the others;
- the only live sprites the view draws past the window's edges are Link and the objects of mode 3; the live OAM entries of objects of modes 1 and 2 that fall outside the left and top edges are ignored, recognised by the tags of the live instance's sprites.

After Tfin the entered room is the active one and entirely in the window: the core draws its objects.

### Pairing

Between the room's entry and the return of `initializeRoom`, each instance, live or ghost, numbers the objects created in order, from the creation events of interactions, enemies and parts; each event carries the caller's return address, and allocation failures are counted by type on the same returns. The number is kept per slot with the object's key (type, `id`, `subid`, `var03`) and cleared when the slot is freed. An object created after the initialisation has no number and is never paired.

For each key, the numbered objects of the two sides are paired in the order of their numbers. If the count of numbered objects of a key differs between the two sides, that key alone is unpaired; if the sequence of keys created before the room's object list (the room's own code, Maple, a remembered companion, the time portal) differs, the whole room is unpaired. The position plays no part. Two creations give the same sequence of keys when they have the same object list, the same flags, the same entry of `wEnemiesKilledList`, the same `wStaticObjects` and the same objects created before the list, and no type ran out of slots.

## Fan games

A fan game is a BPS patch of an original ROM; the player keeps the base ROM and the patch, and the port applies the patch in memory at each launch (`engine/rom/bps.c`) and never writes the image. A recognised fan game is a **compatibility profile** (`engine/game/profiles/profile.c`): its image's SHA-1, its family (Ages' or Seasons' engine), the options it qualifies (continuous transitions, the swim through them, the drawn-back view; the item hotkeys only where its profile qualifies them), and its named rules (the seasons, the Lost Woods' routing, the map by tileset...): the engine asks for a rule, never for a game. Its addresses are a manifest beside its table (`engine/game/profiles/<game>/manifest.json`), reduced to what the generator reads (`engine/game/profiles/gen_tables.py`), from which the checked-in `tables.c` and `identity.h` are generated and checked by the `oracles-fan-game-tables` test. A profile's addresses come from the game's published source or from the hack base it is built on, never from a byte of the patched image in the repository. An image with an Oracle's header but no profile plays in Faithful without hooks; options that need a profile are refused with the reason.

## Mods

A mod is a directory of Lua files run during the game by `engine/mods`: its conversations hold the game's keys and draw over the screen, and it acts on the game through the bus's call transaction, which makes the game run one of its own routines from a closed list. A mod that declares a house has it composed into the image in memory before the core exists, so the core, the ghost and the view all read the same image. [`MODDING.md`](MODDING.md) says how to write one.
