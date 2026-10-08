# Game hooks

What the host hooks in the running game, what it reads there, where it may write, and the facts of the game's engine those rules rely on. The engine is described as oracles-disasm documents it, at the commit pinned in `config/disasm.json`; the symbol names are the disassembly's. Addresses are Ages US; Seasons shares the engine with the differences of section 7, and every address the host uses is resolved per game from the generated tables (`engine/game/guest/guest_tables_*.c`), never written by hand (`tools/check_guest_addresses.py` refuses one).

Conventions: `bb:aaaa` is CPU address `aaaa` in ROM bank `bb`; bank 0 functions are always visible, the others only while their bank is mapped at `$4000-$7fff`. WRAM variables are given by their CPU address; those prefixed `w2`, `w3`, `w4`, `w7` live in that WRAM bank, mapped at `$d000-$dfff` by SVBK; `w1` is bank 1, the objects'.

## 1. Memory and screen

WRAM bank 0 (`$c000-$cfff`) holds the sound, the stacks (`$c0b0-$c2bf`), the threads' states (`$c2e0`), the generic buffer `wBigBuffer` (`$c300`), the vblank function queue (`$c400`), the inputs and graphics registers, the objects to draw (`$c500`), the save file in RAM (`$c5b0-$c700`), the room flags (`$c700-$cb00`), the OAM in RAM (`$cb00`), text and menus, the game variables (`$cc00-$cdff`), the room's collisions (`$ce00`) and layout (`$cf00`). Bank 1 holds the objects (section 4.1); bank 2 palettes, the animation queue and the dungeon layout; bank 3 the current tileset's data (`w3TileMappingData`, 8 bytes per metatile, `w3TileCollisions`); bank 4 menus and the status bar; bank 7 the text box. HRAM holds among others the RNG (`hRng1`/`hRng2`), `hRomBank`, `hActiveThread`, `hCameraY`/`hCameraX` (16 bits each) and the object being updated (`hActiveObjectType`/`hActiveObject`).

The screen:

- two background maps, `$9800` and `$9c00`, used in turn for scrolling transitions; attributes in VRAM bank 1;
- 8x16 sprites, OAM copied by DMA from `wOam` at every vblank;
- the status bar takes the first 16 lines: the LCD interrupt on LYC switches LCDC/SCY/SCX/WY/WX between `wGfxRegs1` (top, status bar) and `wGfxRegs2` (the play area), so the play area is 160x128 pixels, 10x8 metatiles of 16x16;
- `hLcdInterruptBehaviour`, copied from `hNextLcdInterruptBehaviour` at every vblank, has seven behaviours (`lcdInterrupt`, `bank0.s`): 0 and 1 load SCX or SCY from `wBigBuffer` at the start of every line, at index LY, without waiting for a hblank, and rearm LYC for the next line (underwater waves, the distortion of a strange force). The game stores there a table of 128 steps (`w2WaveScrollValues`) read modulo 128 line after line from a phase that moves with `wFrameCounter`, so the 128 lines of the play area are whole periods of the wave. 2, 3 and 4 are normal play, the status bar / play area switch at line 16; 5 (the ring menu) and 6 (the Onox fight) rearm LYC and rewrite registers several times a frame. All are register writes that a journal stamped with LY replays, which is what the native renderer does. Under water, the status bar's switch (behaviour 2, which waits for line 15's hblank, then becomes 0) hands line 16 to the waves: when its handler returns late, line 16's write falls while that line is drawn, and the PPU mixes the two values (the tiles already fetched keep the old scroll, the next ones take the new tile column with the fine scroll latched at the line's start). The journal does not say where in the line a write falls, so the renderer leaves such a line out of its comparison. The window is a layer of its own during menus and the inventory's slide;
- the game runs in CGB double speed.

## 2. Main loop, threads and the order of a frame

`_mainLoop` (`00:0933`): `pollInput`, then the four threads, then the dirty palettes, the copy of `wGfxRegs1` to `wGfxRegsFinal`, and a `halt` until vblank. One iteration is one frame of the game.

The threads are cooperative: a thread yields with `resumeThreadInAFrames` (`00:0900`) or `resumeThreadNextFrame` (`00:08fe`), which save SP in `wThreadStateBuffer` (`$c2e0`, 8 bytes per thread: state, countdown, SP, function). There is no preemption: a thread an interrupt stops resumes where it was. A suspended thread keeps on its stack, from its saved SP, BC, DE, HL and its resume address: those eight bytes are live state, and only the bytes strictly below the saved SP are dead. This is why every WRAM fingerprint the host compares excludes exactly the dead bytes below each thread's SP.

| Thread | Role | Stack |
| --- | --- | --- |
| 0 | the intro (`introThreadStart`) and the game over screen | `wThread0Stack` |
| 1 | `mainThreadStart`: all of the gameplay, and the file select | `wThread1Stack` |
| 2 | text, through `textThreadStart` | `wThread2Stack` |
| 3 | `paletteFadeThreadStart`: palette fades | `wThread3Stack` |

`mainThreadStart` (`00:33a1`) loops on: `wPlaytimeCounter` and `wFrameCounter` incremented, `bank1.runGameLogic`, `drawAllSprites`, `checkReloadStatusBarGraphics`, `resumeThreadNextFrame`. `runGameLogic` dispatches on `wGameState` (`$c2ee`): 0 `initializeGame`, 1 `loadingRoom` (a room loaded from scratch, as after a warp), 2 `standardGameState`, which dispatches on `wCutsceneIndex` (`$c2ef`: 1 is normal play, 0 a room loading, the others cutscenes), 3 `linkSummonedCutscene`.

In normal play, `cutscene01` runs the menus (`updateMenus`, which holds the frame while a menu is open), then `updateAllObjects`, then, if bit 2 of `wScrollMode` is set (a transition decided in the same iteration), `getNextActiveRoom`, `wScrollMode = 8`, `loadTilesetAndRoomLayout`, `loadRoomCollisions` and `initializeRoom`; then the death checks.

`updateAllObjects` (`00:345b`) runs, in this order: the special objects (the companion, then Link); `updateItems`; `setEnemyTargetToLinkPosition`, then `updateEnemies`; `updateParts`; `updateInteractions`; `bank1.func_4000`, the screen transitions' state machine (section 3); the ridden and carried objects; `loadLinkAndCompanionAnimationFrame`; `updateItemsPost` and the object that follows Link; `updateCamera`; `updateChangedTileQueue`; `updateAnimations` (section 5). Then `drawAllSprites` builds `wOam`. Between `updateAnimations` and `drawAllSprites`, or just after, positions, layout, animations and palettes are consistent: that is where the host reads the state of a frame.

Interrupts: the **vblank** handler (`00:09f8`) writes LCDC/SCY/SCX/WY/WX/LYC from `wGfxRegsFinal` and, if the main loop is waiting, copies `wGfxRegs2` to `wGfxRegs3`, runs the whole `wVBlankFunctionQueue` (graphics DMA, tile rows during transitions), `updateDirtyPalettes` and the OAM DMA; on a frame the main loop did not finish, nothing is committed and the PPU shows the previous frame again. The **LCD** handler switches at the status bar and serves behaviours 0 and 1; the **timer** drives the sound. Neither the vblank nor the LCD handler calls gameplay code: a hook on a gameplay function is never re-entered by an interrupt.

## 3. Screen transitions and scrolling

For a small room, `hCameraY`/`hCameraX` are 0: the screen is the room. For a large room, `updateCameraPosition` aims at the followed object minus half a screen, bounded to the room, and moves one pixel a frame towards it; `updateGfxRegs2Scroll` derives SCX/SCY of `wGfxRegs2` from it, plus `wScreenOffsetX/Y` and the screen shake. The game copies its camera to the display registers at the vblank that opens the next frame, after the logic that moved it: the image on screen was drawn with the camera one frame older.

`wScrollMode` (`$cd00`) governs what updates:

| Value | Meaning |
| --- | --- |
| `$00` | a warp in progress (`initiateWarp`) |
| `$01` | normal play: tile animations, camera and text active |
| `$02` | loading or a non-scrolling entry (`loadingRoom`, `screenTransitionState1`): enemies frozen |
| `$04` | a scrolling transition decided by `screenTransitionState2`; `cutscene01` calls `getNextActiveRoom` in the same iteration and sets `$08` before `initializeRoom` |
| `$08` | the entered room loading, then the scroll (set back to `$01` at the end by `setInstrumentsDisabledCounterAndScrollMode`): enemies, interactions and parts frozen |

The state machine is `bank1.func_400b`, on `wScreenTransitionState` (`$cd04`):

- state 0: entry into a room from scratch, then `initializeRoomBoundaryAndLoadAnimations`, which sets the room's size (20x16 or 30x22 tiles), the transition boundaries, the camera limits and the tile animation;
- state 1: after a warp, the screen opens in 32 steps, each writing a column of the new room into the other background map through the vblank queue and moving the camera; it starts from the middle column of the window and writes one column a frame, alternately left and right. A column not written yet shows the blank tile: the curtain that opens from the centre;
- state 2: normal play; each frame, if Link's y is at most 5 the transition goes up, past the bottom boundary down, and the same in x; bit 7 of `wScreenTransitionDirection` forces a transition without any test of position or collision;
- states 3 to 5: the transition, Link in `LINK_STATE_WARPING` (`$0a`), the new room from `getNextActiveRoom`, its layout loaded, then the scroll.

The scroll itself (state 5) sets `wScreenScrollCounter` (`$14` columns horizontally, `$10` rows vertically) and the step `wcd14` (±4 px); each step moves `wGfxRegs2` and `hCameraX/Y` by the step and Link by `$0060` in x or `$0080` in y (8.8 fixed point), and a column is drawn when the register is aligned on 8, so every other step. Then the unique graphics load one entry a frame, `wScrollMode` returns to 1, and `finishScrollingTransition` moves Link back by ±160 or ±128, resets `hCameraX/Y` and recomputes the camera. A horizontal crossing is forty steps of 4 px plus the preparation (state 3: layout, collisions and tile animation; state 4: unique graphics), during which `linkState01` skips Link's movement (`wScrollMode & $0e`): he is frozen, and the game moves him 15 px (16 vertically) in all. His walk is the stream of `specialObjectAnimationTable`, advanced by `animateLinkWalking`, which the transition suspends too.

A swimming Link is frozen the same way: `linkState01` returns before `linkApplyTileTypes`, `linkUpdateSwimming` and `linkUpdateDiving`, so his speed (`w1Link.speed`, a row of `objectSpeedTable` times five, `$20` in 8.8 a row along an axis), his angle, his momentum (`var35`, `counter1`), the dive counter (`counter2`) and his animation (`SWIM` `$0b` or `DIVE` `$0c`, advanced outside a transition by `specialObjectAnimate` every frame, standing still included) stay as they are until he arrives. With the flippers he swims at `SPEED_80` (`SPEED_e0` with the swimmer's ring), and a stroke of A raises `speedTmp` by 5 every 4 frames for 13 frames, then lowers it (`linkUpdateFlippersSpeed`); with Ages' mermaid suit (`var2f` bit 6) at `SPEED_120` (`SPEED_160` with the ring). On Ages' sea floor (`TILESETFLAG_UNDERWATER`, groups 2 and 3) the game has him walk: `wLinkSwimmingState` stays zero, the walk's animation takes the mermaid variant, and he moves by `linkUpdateVelocity@mermaidSuit`. A current (`TILETYPE_*CURRENT`) pushes him at `SPEED_c0` and takes him across an edge without the direction held (`wcc92`, `@adjustLinkOnConveyor`). A water tile carries the collision `SPECIALCOLLISION_HOLE` (`$10`, that of holes and lava too); its type comes from `tileTypesTable`, through the list of `wActiveCollisions` (`lookupCollisionTable`). Over water, `@checkCanTransitionOverWater` wants the flippers, and in Ages refuses deep water without the mermaid suit.

`screenTransitionState2@transition` also wants the edge's direction held (`convertLinkAngleToDirectionButtons` on `wLinkAngle`, eight sectors of 45 degrees), except on a current (`wcc92`'s low bits). It tests the position after Link has moved and, past the threshold, puts him back on the edge itself before checking the rest (`@transitionUp` and the others write `yh` or `xh`, then call `@transition`), whether the transition starts or not: at the end of a frame, a Link pushed against the edge without the direction held stands a pixel short of the test (6 at the top and the left, the boundary itself at the bottom and the right). A transition forced by bit 7 skips that write and starts from where Link's move took him, up to a pixel further. When two edges pass at once, the second transition, the horizontal one, overwrites the first. A stroke of the flippers carries Link on in the direction he faces with no direction held.

After `updateAllObjects`, `cutscene01` calls `func_60e9` before `getNextActiveRoom`: a forced transition does not stop a warp tile under Link from starting `initiateWarp`, which replaces the scroll with a warp. `wDisableWarpTiles` inhibits that check and the edge check of `checkWarpsTopDown`.

Warps (doors, stairs, holes) take another path: `warpSources` per room, `warpDestinations` per group (`docs/ROM_DATA_FORMATS.md`, section 6), `setWarpDestVariables`, then `wGameState = 1` and `loadingRoom`. Ages' dive into the sea and the surfacing do not go through `warpSources`: `checkForUnderwaterTransition` (`link.s`) sets the warp itself, to the room of the same index two groups down (a dive: 0 to 2, 1 to 3) or up (surfacing), at Link's tile, with bit 7 on `wWarpDestGroup`, and `cutscene01` plays it at the next iteration. A dive starts in `linkUpdateDiving` over `TILEINDEX_DEEP_WATER`; a surfacing from a B press under water where `checkLinkCanSurface` allows it, never on a whirlpool.

## 4. Objects

### 4.1 Slots

WRAM bank 1 holds 16 slots of 256 bytes, `$d000` to `$df00`. Each slot holds four 64-byte objects:

| Offset | Type | Structure | Slots |
| --- | --- | --- | --- |
| `$00` | item or special object | `ItemStruct` / `SpecialObjectStruct` | Link `$d000`, companion `$d100`, parent items `$d200`-`$d500`, weapon `$d600`, dynamic items `$d700`-`$db00`, reserved `$dc00`-`$df00` |
| `$40` | interaction | `InteractionStruct` | reserved `$d040` and `$d140`; dynamic `$d240`-`$df40` |
| `$80` | enemy | `ObjectStruct` | `$d080`-`$df80` |
| `$c0` | part | `ObjectStruct` | `$d0c0`-`$dfc0` |

`hActiveObject` holds the slot's high byte (`$d0`-`$df`), `hActiveObjectType` the offset. The `object*` functions of bank 0 work on `de = hActiveObject:hActiveObjectType`. An object's mode is bits 0-1 of `enabled`.

### 4.2 Object graphics and sprite drawing

`interactionData` (3 bytes per id, or a pointer to a list by subid) gives the object graphics header, the OAM tile base, the default animation, the palette and the VRAM bank. `interactionInitGraphics` loads the graphics through `objectGfxHeaderTable` into `wLoadedObjectGfx` (`$cc08` in Ages, `$cc07` in Seasons: eight 2-byte entries, header index and use mark, entry n loaded in VRAM at `$8000 + n x $200`, 32 tiles) and sets `oamTileIndexBase`; enemies and parts work the same way (`enemyData`, `partData`). Animations are lists of pairs duration / OAM block, with a loop; `animPointer` walks them and `oamDataAddress` designates the current block, a count followed by quadruplets Y, X, tile, attributes relative to the object's position.

`drawAllSprites` (`00:0d9a`): `queueDrawEverything` calls `objectQueueDraw` (`00:10a8`) for each visible object, ranked in `wObjectsToDraw` (`$c500`) by priority, sixteen at most per level; then each object becomes 8x16 sprites in `wOam` (`drawAllSprites@drawObject`), placed by `_getObjectPositionOnScreen`, which subtracts the camera and the screen offset (`_getObjectPositionOnScreen_duringScreenTransition` during a scrolling transition, section 4.3). An object is drawn only if bit 7 of `visible` is set (bit 6: terrain effects). `@drawObject` writes into `wOam` (`$a0` bytes, forty entries) and skips each sprite whose OAM y byte, offset included, reaches `$a0` (line 144) or whose x byte reaches `$a8` (column 160): outside a transition, a sprite past the bottom or the right of the screen is not written, sprite by sprite. Outside a transition, a sprite is placed in y at `yh − hCameraY + $10 + zh` plus its offset, in x at `xh − hCameraX` plus its own; its tile is `oamTileIndexBase` plus the block's, its attributes `oamFlags` xor the block's; the block is in bank `BASE_OAM_DATA_BANK` (`$13` in Ages, `$12` in Seasons) plus the two high bits of `oamDataAddress`, at `(oamDataAddress | $4000) & $7fff`. An object on the ground whose tile underfoot is a puddle is drawn one pixel lower. Terrain effects (`_drawObjectTerrainEffects`, for any object with bit 6 of `visible`, none in a side-scrolling room): an object in the air queues a shadow every other frame, written after the loop of objects at its position without `zh`; an object on the ground, outside a scrolling transition, writes the grass or the puddle before its own sprites. `func_0eda` writes these blocks as they are, whole or not at all depending on the room left in the OAM.

A renderer that draws the objects from `wObjectsToDraw` or from the object table, with their OAM blocks, reproduces the image without the hardware limit of ten sprites per line; to equal the core's framebuffer it reads the OAM committed by DMA and applies the CGB PPU's selection of ten objects per line by OAM index.

### 4.3 Objects across a scrolling transition

1. Decision: in `updateAllObjects`, after the active room's enemies, parts and interactions are updated, `screenTransitionState2` writes `wScrollMode = $04` and `wScreenTransitionState = 3` (from an edge, which requires `w1Link.enabled`, or from bit 7 of `wScreenTransitionDirection`).
2. The same iteration, `cutscene01` calls `getNextActiveRoom`, which looks up `mapTransitionGroupTable` before the standard transition: the entered room is not always the grid's neighbour. The table, indexed by group, is a dictionary of room and case ended by a null room; it routes Seasons' Lost Woods (`0:40`) and the sword upgrade room (`0:c9`), Ages' nine rooms of the scrambled forest (`0:70` to `0:92`), and in group 5 Onox's dungeon and the eye puzzles. The Lost Woods send Link back to their entrance unless he follows the right sequence of directions, counted in `wLostWoodsTransitionCounter1` and `2`: the room reached from a room of the woods depends on the sequence and changes at every transition. Then `setObjectsEnabledTo2` moves every object of mode 1 to mode 2 (mode 3 persists), the tileset and layout load, `wScrollMode = $08`, and `initializeRoom` creates the entered room's objects: first those of the room's own code, Maple, a remembered companion and Ages' time portal, then `parseObjectData` in mode 1 in the free slots (sixteen per type, taken in order; an opcode without a slot is dropped), then `parseStaticObjects`. During the whole scroll the objects of both rooms share bank 1: what the room left occupies changes the slots, and possibly the number, of the objects created.
3. While `wScrollMode` is `$08` the objects are frozen, except as the table below says: the initialisation of a newly created object runs during the scroll, then it stays frozen, and a frozen object's animation does not advance. `drawAllSprites` draws them with a 16-bit offset (`hFF90`-`hFF93`) by mode (an object of mode 3 is placed by the camera) and drops a whole object whose position leaves a window.
4. End of the scroll: `wScrollMode` returns to 1; `finishScrollingTransition` moves Link back, resets the camera and calls `clearObjectsWithEnabled2`, which erases every object of mode 2: the room left has no objects any more.

No object of a room exists outside the active room and the transition in progress, except those of mode 3 and those `wStaticObjects` remembers: an entered room receives its objects in their initial state, computed at that moment.

| Type | Routine | Frozen when | Updated all the same |
| --- | --- | --- | --- |
| enemies | `updateEnemies` | `wScrollMode & $0e`, `wTextIsActive`, `wDisabledObjects & $84`, `wPaletteThread_mode` not zero | `state` and `substate` at 0 |
| interactions | `updateInteractions` | `wScrollMode = $08`, `wDisabledObjects & $02`, `wTextIsActive` | bit 7 of `enabled` set, or `state` at 0 |
| parts | `updateParts` | `wScrollMode = $08`, `wTextIsActive`, `wDisabledObjects & $88` | bit 7 of `enabled` set, or `state` at 0 |

Bit 7 of `wDisabledObjects` also freezes Link, the companion and the items.

What else a created object depends on:

- `obj_RandomEnemy` (`$f6`) draws positions from a buffer that `getRandomNumber` fills, and the RNG advances whenever any object draws a number: the RNG at the entry into a room depends on everything that ran before. On a scrolling transition, `checkPositionValidForEnemySpawn` refuses the three rows or columns of the edge Link enters by; on a warp, the cells within three tiles of the destination. So a randomly placed enemy's position depends on the edge of entry and on the RNG; near half the enemies of the object lists are placed this way.
- `checkEnemyKilled`: up to seven killable enemies per room receive an index; `wEnemiesKilledList` keeps, for the last eight rooms visited, the room number and the bitset of the enemies killed, which do not reappear; a warp's `loadingRoom` empties the list.
- `setEnemyTargetToLinkPosition` copies Link's position to `hEnemyTargetY/X` before `updateEnemies`: the enemies aim at Link through those bytes.
- The sprite tiles of an object depend on the order in which the room loaded its graphics (`wLoadedObjectGfx`, section 4.2).

## 5. Tile animation

`initializeAnimations` loads, for `wTilesetAnimation`, a group (`animationGroups.s`) that activates up to four animation streams (`wAnimationState` bits 0-3), each a list of pairs duration / graphics index with a loop (`animationData.s`, `docs/ROM_DATA_FORMATS.md`, section 10). Flowers, water and torches move by replacing tiles in VRAM, not by changing the layout.

Each frame, `updateAnimations` (`04:5906`) clears bit 6 of `wAnimationState`, takes a single index out of the queue (setting bit 6 if it takes one), then counts the active streams: a counter (`wAnimationCounterN`) followed by its pointer (`wAnimationPointerN`, three bytes per stream), the pointer designating the current entry's graphics index, the next byte the next entry's delay. At zero, the index is queued (`w2AnimationQueue`, 32 entries, `wAnimationQueueHead`/`Tail`; an index is lost when the queue is full) and the next delay read; a delay of `$ff` is the loop marker, followed by the low byte of an offset that, added to that byte's address with `$ff` as high byte, goes back to the start of the list. A copy's header (`animationGfxHeaders`, six bytes): ROM bank, source (high then low byte), destination (high then low, VRAM bank in bit 0), size in 16-byte blocks minus one. The copy reaches VRAM at the vblank after the frame that dequeues it, or the one after when the game's frame overruns. The view's reader (`engine/neighbours/animation.c`) starts from this state, read in the ghost for a neighbour, and advances it step for step with the game.

During a scroll between two rooms the animation is frozen and resumes where it stopped: `updateAnimations` returns before the queue and the counters while bit 0 of `wScrollMode` is clear (`$04` when the edge is touched, `$08` during the whole transition), and only `loadAnimationData` rewrites the counters, called at the transition when the entered room's `wTilesetAnimation` differs from `wLoadedTilesetAnimation`: the animation then restarts from its initial state. The frames of the scroll are never counted: across a scroll under the same animation, the game takes a single step between the last frame before the freeze and the first after. The entered room has loaded its tileset (`wTilesetAnimation`, `wTilesetGfx`) by the first frozen frame. Indices queued when the freeze starts stay queued and reach VRAM after it.

## 6. Hook points and safe write points

The events the host needs, and where it takes them. The addresses come from the generated tables.

| Event | Hook | What is read |
| --- | --- | --- |
| game frame finished | entry of `drawAllSprites` | the whole gameplay state of the frame. The return of `updateAllObjects` is not enough: `getNextActiveRoom`, `loadTilesetAndRoomLayout` and `initializeRoom` run after it and before `drawAllSprites`; a frame that changes room is a discontinuity (`wActiveRoom` changed). The display state is not committed yet: palettes, animated tiles and OAM are, at the next vblank |
| image committed | the core's vblank hook (`on_vblank` in `engine/core/core.h`, SameBoy's vblank callback), of type `ORACLES_VBLANK_NORMAL` only; it runs before the guest's vblank handler | the display state the scan just finished used: VRAM, OAM, `w2BgPalettesBuffer`, `wGfxRegs3`, plus the journal of the register writes of the LCD interrupt by line. `LCD_OFF`, `ARTIFICIAL` and `REPEAT` are frames without a scan |
| room entered | entry of `initializeRoom`, after `loadTilesetAndRoomLayout` | group, room, layout, tileset, flags |
| room initialised | return of `initializeRoom` | the object table. `parseObjectData` runs only if bit 0 of `wcc05` is set, and in Ages `initializeRoom` leaves by a tail jump when `wSentBackByStrangeForce`: a return hook is set on the return address captured at the entry, never on the function's `ret` |
| interaction created | return of `getFreeInteractionSlot` with Z set | slot |
| enemy created | return of `getFreeEnemySlot` with Z set | slot |
| enemy killed | a write of 0 to the `health` of a live enemy, through the write callback | slot |
| screen transition decided | the write of `wScreenTransitionDirection` by `@startTransition`, when `wScreenTransitionState` is 3 and `wScrollMode` 4; `screenTransitionState2` runs every frame of normal play, its entry says nothing | direction |
| warp applied | return of `applyWarpDest` | destination |
| text shown | entry of `showText` | index |
| text choice | write of `wSelectedTextOption` | value |
| global or room flag changed | a write to `$c6d0-$c6df` or `$c700-$caff` | address, bit |
| treasure obtained | `giveTreasure` | id |
| menu opened | entry of `openMenu` | type |
| save | entry of `saveFile` | slot |
| file operation (the mods' `mod.storage`) | entry of `fileManagementFunction` (`07:4000`), the one path of `initializeFile`, `saveFile`, `loadFile` and `eraseFile` | `c` (0 creation, 1 save, 2 load, 3 erase), `hActiveFileSlot` |
| player input | after `pollInput` | `wKeysPressed` |

**Reading.** A hook reads freely, by physical address (bank and offset: `w3TileMappingData`, `w2BgPalettesBuffer`, `w4StatusBarTileMap`, `w7TextboxMap` and the object table share the same window `$d000-$dfff`), through direct access to the core's memory, never through the guest CPU's bus, which returns `$ff` on VRAM and OAM while the PPU draws and drops VRAM writes in mode 3. Reading or writing `SVBK`/`VBK` to reach a bank is a guest write, and forbidden.

**Writing.** Only between `drawAllSprites` and the `resumeThreadNextFrame` that follows, or in the events marked safe (after `initializeRoom`); never during `updateAllObjects`. A hook never changes SP or the stacks, the threads' saved contexts included, nor the bank registers. The presentation writes nothing; the only writes to the live game are three closed transactions of the guest bus (`engine/game/guest/`):

- **The continuous transitions** (`--continuous-transitions`) write at two points: at the return of `drawAllSprites`, and at the vblank of a frame the main loop did not finish while the transition loads the room (`wScreenTransitionState` 3 or 4: layout, collisions, unique graphics, whose code does not touch these fields); an unfinished frame of the scroll itself (state 5) is not written, its logic possibly standing in the middle of Link's own movement, and the harness counts the vblank writes by state (`transitions.vblank_scroll_writes` is zero by construction). The fields: `wcd14` (the step), `w1Link.x` and `w1Link.y`, `w1Link.animCounter`, `animParameter`, `animPointer` and `var31` (the walk), the two `wOam` entries of Link's sprite and, at the vblank, the same in the core's OAM, and `hDirtyBgPalettes` outside a transition. The bus checks the state itself (a scrolling transition of a small room, Link on foot, nothing carried). With `--continuous-swim` the bus also takes Link swimming at the surface in his normal state (bits 0-3 of `wLinkSwimmingState` at 3, bit 6 clear, diving or not; the `SWIM` or `DIVE` animation), and on Ages' sea floor, where the game has him walk (`LINK_ANIM_MODE_WALK`, `wLinkSwimmingState` zero, `var2f` bit 7): the same fields, the move bounded by a pixel an axis on foot and by `$160` (8.8, `SPEED_160` along an axis, the fastest swim) in the water; a side view (`TILESETFLAG_SIDESCROLL`) is never written. The option adds one field, at the return of `drawAllSprites` in normal play (`wScreenTransitionState` 2, `wScrollMode` 1), never at a vblank: bit 7 of `wScreenTransitionDirection`, raised to one of the four directions for the transition a stroke of the flippers carries Link to, or lowered when it was raised and not taken. `screenTransitionState2` reads it first and starts the transition without its other checks, which the bus makes itself: Link swimming at the surface in his normal state, never in a side view, at the edge of that direction, enabled, transitions neither disabled nor delayed, nothing in `wcc92`, the direction held none or square across the edge's axis.
- **The item hotkeys** (`--item-hotkeys`) write at one point: the return of `checkReloadStatusBarGraphics` from its only `call` in `mainThreadStart`, found in the user's ROM when the option arms (exactly one, else the option is refused; the tail jumps of `bank2.s` to the same function return elsewhere and do not count). It is the one moment when bit 0 of `wStatusBarNeedsRefresh` set by the host survives until the next turn's `updateStatusBar`, `checkReloadStatusBarGraphics` clearing the byte every turn. The fields: an exchange of two of the eighteen contiguous slots `wInventoryB`, `wInventoryA`, `wInventoryStorage[0..15]`, one of them a button, never a new value; `wSatchelSelectedSeeds`, `wShooterSelectedSeeds` (0 to 4, a seed obtained) and `wSelectedHarpSong` (Ages, 1 to 3, a song obtained); bit 0 of `wStatusBarNeedsRefresh` by an OR. The bus applies it only in the states where the player could open the inventory (`b2_updateMenus`), on an inventory where each item appears once.
- **The mods' calls** share that point and write no variable: at the instruction that returns from that same call (SP and return address equal to those of the entry, a conditional return taken), in a state without a lasting refusal of the inventory, the transaction pushes under the return address that of a routine from a closed list (`giveTreasure`, `loseTreasure`, `removeRupeeValue`, `playSound`, `setGlobalFlag`, `unsetGlobalFlag`) and sets `a` and `c`; the routine returns to the `call resumeThreadNextFrame`, which reads no register. It is the one exception to the rule on SP and the stacks, one call a frame.

The ghost instance, a disposable copy of the core, is the only other one to receive writes: bit 7 of `wScreenTransitionDirection` and `wDisableWarpTiles` to prime it, Link's position at the edge, `wRoomStateModifier` (Seasons, the season held) and, at the entry of `checkRoomPack`, `wRoomPack`.

What the renderer reads each frame: `wActiveGroup`, `wActiveRoom`, `wRoomIsLarge`, `hCameraX/Y`, `wScreenOffsetX/Y`, `wScreenShakeCounterX/Y`, `wRoomLayout`, `w3TileMappingData` and the attributes, `w2BgPalettesBuffer`/`w2SprPalettesBuffer`, `wAnimationState` and the pointers, `wObjectsToDraw` or the object table with their OAM blocks, `w4StatusBarTileMap`, `wTextIsActive` and the text box (`w7TextboxMap`). A neighbouring room's terrain never comes from the ROM with reimplemented substitutions: it comes from a ghost instance in which the game loads the room itself.

## 7. Seasons

The same engine, structures and threads, not the same addresses: `hram.s` and `wram.s` have conditional blocks by game (`hCameraX` is `$ffac` in Ages and `$ffaa` in Seasons; `wActiveGroup` `$cc2d` against `$cc49`; `wGlobalFlags` `$c6d0` against `$c6ca`), which is why every address is resolved per game from the generated tables. Other differences: the code banks of interactions and enemies, the scripts in bank `$14`, group 1 is Subrosia (11x8 rooms), the tilesets change with the season through `wRoomStateModifier` and the room packs, eight seed trees instead of sixteen, a few special transitions (the Lost Woods, Onox). The disassembly marks the differences with `ROM_AGES`/`ROM_SEASONS` and `AGES_ENGINE`.

## Sources

`include/wram.s`, `include/hram.s`, `include/structs.s`, `include/constants.s`; `code/bank0.s` (loop, interrupts, object primitives, loading), `code/bank1.s` (game logic, transitions, camera), `code/objectLoading.s`, `code/roomInitialization.s`, `code/animations.s`, `code/fileManagement.s`, `code/specialObjects.s`, `code/textbox.s`; `constants/common/*.s`, `objects/macros.s`, `data/ages/*.s`.
