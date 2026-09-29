# ROM data formats

The formats of the data in the ROMs of Oracle of Ages and Oracle of Seasons (US), as the native readers of `engine/game/data/`, the ghost's room classification and the mods' composition read them. They are described as the data sits in the ROM and as oracles-disasm and LynnaLab rebuild it. Sources: the `data/ages/*.s`, `objects/`, `rooms/`, `tileset_layouts/` and `tools/build/*.py` files of the disassembly at the commit pinned in `config/disasm.json`, and the classes of LynnaLib (LynnaLab, MIT licence), which implement these formats.

Conventions:

- the addresses of tables come from the disassembly's symbol files (`ages.sym`, `seasons.sym`), through the tables generated from them (`engine/game/data/oracles_tables_*.c`, `engine/game/guest/guest_tables_*.c`); this document describes formats, not places;
- "BE" is a 16-bit word written high byte first, as the disassembly's `dwbe` macro does; other words are little-endian, as on the Game Boy;
- a "3-byte pointer" is `bank, address BE`;
- a room is identified by a 12-bit index, `group << 8 | room`; there are eight groups of 256 rooms;
- the disassembly keeps the original compressed forms under `precompressed/` to rebuild the ROM bit for bit; a reader that starts from the ROM decompresses itself, with the algorithms of section 9.

Seasons shares these formats; its differences are noted inline and summed up in section 12.

## 1. Tables by room

Each group has, for its 256 rooms, a byte or a pointer in the following tables.

| Table | Per room | File |
| --- | --- | --- |
| `roomTilesetsGroupTable` → `groupNTilesets.bin` | tileset index; groups 6 and 7 share the tables of groups 4 and 5 | `tilesetAssignments.s` |
| `roomLayoutGroupTable` | pointer to the compressed layout, section 2 | `roomLayoutGroupTable.s` |
| `objectData` (`objects/ages/pointers.s`) | pointer to the room's object stream, section 5 | `objects/ages/` |
| `warpSourcesTable` | the warps leaving the room, section 6 | `warpSources.s` |
| `chestDataGroupTable` | the group's chests, entries `yx, room, treasure` ended by `$ff` | `chestData.s` |
| `roomPackData` (groups 0 and 1) | bits 0-6 graphics pack, bit 7 forced reload when leaving the pack | `roomPacks.s` |
| `musicAssignmentGroupTable` | music identifier | `musicAssignments.s` |
| `roomsInAltWorldTable` | bit set if the room is in the past | `roomsInAltWorld.s` |
| `dungeonRoomPropertiesGroupTable` (groups 4 and 5) | bits: up `$01`, right `$02`, down `$04`, left `$08`, key `$10`, chest `$20`, boss `$40`, dark `$80` | `dungeonProperties.s` |
| `singleTileChangeGroupTable` | entries `room, flag mask, position, tile`; special masks `$f0` unlinked, `$f1` linked, `$f2` after an unlinked game | `singleTileChanges.s` |

## 2. Room layouts

### 2.1 Group table

`roomLayoutGroupTable` holds, per group: a mode byte (`$01` small rooms, `$00` large rooms), a 3-byte pointer to the group's table of rooms, a 3-byte pointer to the base of the data, a padding byte.

### 2.2 Small rooms (layout groups 0 to 3 in Ages, 0 to 4 in Seasons)

The decompressed layout is 80 bytes: 8 rows of 10 metatile indices. In RAM the game lays it out with a stride of 16 per row.

The group's table has a word per room: bits 0-13 offset from the base, bits 14-15 compression mode.

| Mode | Encoding |
| --- | --- |
| 0 | 80 raw bytes |
| 1 | per block of 8 bytes: a key byte, then the block's most frequent byte if the key is not zero, then the bytes that differ from it in order; bit `i` of the key says byte `i` is the common one; a zero key means 8 raw bytes |
| 2 | the same per block of 16 bytes with a 2-byte key |

This is `compressData_commonByte` of `tools/common.py`. The original game picks mode 0 unless one of the others is strictly smaller.

### 2.3 Large rooms (layout groups 4 and 5 in Ages, 5 and 6 in Seasons)

The decompressed layout is 176 bytes: 11 rows of 16 bytes, 15 of them used. The file `rooms/ages/large/roomGGRR.bin` is 176 bytes.

Each group has a dictionary of 4096 bytes (`dictionary4.bin`, `dictionary5.bin`) at the start of its data. The group's table has a word per room: offset from the base plus `$200`. The stream is a series of blocks: a flag byte, then eight items, a bit per item read from bit 0 to bit 7. Bit 0: a literal byte. Bit 1: a little-endian word, bits 0-11 offset in the dictionary, bits 12-15 length minus 3, so copies of 3 to 18 bytes. Decompression stops after 176 bytes (`compressRoomLayout.py`, dictionary mode).

### 2.4 Substitutions after loading

The layout in RAM is only true after `applyAllTileSubstitutions`: `standardTileSubstitutions` (by room flag bit and collision mode, pairs `new, old` ended by `$00`; indexed by `wActiveCollisions` in Ages, by `wActiveGroup` in Seasons), `singleTileChanges` (section 1), then the room's own code (`code/ages/roomSpecificTileChanges.s`). This is why the host never computes a neighbouring room's terrain itself: the ghost instance lets the game load it (`docs/ARCHITECTURE.md`).

### 2.5 Where a small layout may be placed

`loadRoomLayout` (`code/bank0.s`, `@loadSmallRoomLayout` and `@loadLayoutData`) keeps the bank of the offset table apart from the bank of the streams' base, so both blocks can be pointed elsewhere without moving the loader. The table of 256 words is read directly in its bank and must fit in it whole. Each stream starts between zero and `$3fff` bytes from the base; the compression bits are not extra address bits (`m_RoomLayoutPointer`, `include/macros.s`, encodes the same physical offset).

The start is normalised differently by the two games. Ages takes `$40` off the high byte and moves to the next bank while the address is past `$7fff`, without changing `hFF8C`. Seasons corrects a single overflow: an address from `$8000` to `$bfff` is brought back by `xor $c0` and the bank `hFF8C` is incremented in place; an address of `$c000` or more would land in `$0000-$3fff`, outside the switchable bank. A small layout's start, at most base plus `$3fff`, stays in the case both games correct; a validator of large layouts must apply each game's rule.

The loader then prefetches 176 bytes with `readByteSequential` into a scratch buffer before decompressing: the stream may cross a bank boundary, but the whole prefetch window must stay readable, and no placement may make the one-byte bank selector wrap to zero. The reach of the starts does not mean a recompressed group fits: 256 distinct raw layouts take 20 480 bytes, and a composer checks the offsets of its final placement.

## 3. Tilesets

### 3.1 Definition

`tilesetData`: 8 bytes per tileset.

| Byte | Meaning |
| --- | --- |
| 0 | bits 4-7 collision mode (`wActiveCollisions`), bits 0-3 dungeon index, `$f` none |
| 1 | flags: `$80` past (Subrosia in Seasons), `$40` underwater, `$20` side-scrolling, others in `tilesetFlags.s` |
| 2 | unique graphics header index (`uniqueGfxHeaders.s`) |
| 3 | main graphics header index (`gfxHeaders.s`) |
| 4 | palette header index (`paletteHeaders.s`) |
| 5 | tileset layout index: mappings and collisions (`tilesetHeaders.s`) |
| 6 | room layout group |
| 7 | tile animation group, `$ff` none |

Seasons: an entry may be `$ff, pointer, 5 zero bytes` (`m_SeasonalTileset`); the pointer designates four 8-byte entries, spring, summer, autumn, winter, chosen by `wRoomStateModifier`.

### 3.2 Metatile mappings

Decompressed, a tileset's mappings are 2048 bytes: 256 metatiles x 8 bytes, four 8x8 tile indices in the order top-left, top-right, bottom-left, bottom-right, then four CGB attribute bytes in the same order (bits 0-2 palette, bit 3 VRAM bank, bit 5 flip X, bit 6 flip Y, bit 7 priority). LynnaLab reads `index*8 + y*2 + x` for the indices and `+4` for the attributes.

In the ROM this table is stored in two steps. `tilesetLayoutTable` points, per layout index, to a list of 8-byte `m_TilesetLayoutHeader`: dictionary index, source bank, source address BE, destination BE with the WRAM bank in the low 4 bits, size BE with a continuation bit 7 in the high byte. The destinations are `w3TileMappingIndices` and `w3TileCollisions`. The mapping data loaded are **indices** of mapping rows; `loadTilesetLayout` unfolds them into 8 bytes per metatile through `tileMappingTable`, `tileMappingIndexData` and `tileMappingAttributeData`, tables shared by every tileset that deduplicate the 4-byte rows. The index and collision streams are compressed with the dictionary LZ of section 9.3, with the dictionaries `mappingsDictionary.bin` and `collisionsDictionary.bin` of `tilesetHeaders.s`.

A native reader can reproduce this unfolding, as LynnaLab does and as `engine/game/data/tileset.c` does, or rebuild the 2048 bytes per tileset beforehand from the same tables.

### 3.3 Collisions

256 bytes per tileset, one per metatile: `0` free, `1` to `15` shapes by quadrant (a bit per solid subtile), `16` and more special shapes read through the code's `@specialCollisions` table. In RAM, `wRoomCollisions` receives each cell's metatile byte.

### 3.4 Tile properties

Tables by collision mode (overworld, indoors, dungeon, side-scrolling, underwater, a fifth), lists of pairs `tile, value` ended by `$00`: `tileTypeMappings.s` (the tile's type for Link: hole, vine, grass, stairs, whirlpool, ice, puddle, water, sea...), `hazards.s` (water `$01`, hole `$02`, lava `$04` for objects), `breakableTiles.s`, `cliffTiles.s`, `pushableTiles.s`, `keydoorTiles.s`, `warpTiles.s`, `interactableTiles.s`, `itemPassableTiles.s`, `enemyUnspawnableTiles.s`, `conveyor*.s`, `seaEffectTiles*.s`, `timewarp*.s`, `landableTilesFromCliffs.s`, `facingDirAfterWarp.s`, all in `data/ages/tile_properties/`.

## 4. Graphics and palettes

### 4.1 Graphics headers

`gfxHeaderTable`: `$bb` pointers to lists of 6-byte entries (`m_GfxHeaderHelper`):

| Bytes | Meaning |
| --- | --- |
| 0 | bits 0-5 source bank, bits 6-7 compression mode |
| 1-2 | source address BE |
| 3-4 | destination BE; the low 4 bits are the VRAM or WRAM bank; the address is a multiple of 16 |
| 5 | bits 0-6 size in 16-byte blocks minus 1; bit 7 set when another entry follows |

`loadGfxHeader` requires the screen off. The tilesets' unique headers (`uniqueGfxHeaders.s`, `$15` entries) have the same format, are loaded one entry per frame with the screen on, and may end with `$00, palette index`. Object headers (`objectGfxHeaders.s`, `m_ObjectGfxHeader file [, continue]`) load 32 blocks, 512 bytes, into a sprite slot; the continuation flag chains the next header for objects with more than 32 tiles. Animation headers (`animationGfxHeaders.s`, `m_GfxHeaderAnim file, destination, size, source offset`) are 6-byte entries without a continuation bit, transferred by the DMA queue at vblank. Tree headers (`treeGfxHeaders.s`) and uncompressed ones (`uncmpGfxHeaders.s`) follow the same scheme. Only the first megabyte (banks `00`-`3f`) is reachable from a header's 6-bit bank.

### 4.2 Palettes

`paletteHeaderTable`: `$cb` pointers to lists of `m_PaletteHeaderBg` or `Spr` entries: a control byte (bits 0-2 number of palettes minus 1, bits 4-6 first palette, bit 3 background or sprites, bit 7 another entry follows), then a 16-bit pointer to the colours. Colours are little-endian RGB555 words (`m_RGB16 r, g, b`, 5-bit components), four per palette. Some headers point to RAM instead of ROM.

## 5. A room's objects

### 5.1 Object stream

For each group, a table of 256 pointers (`objects/ages/pointers.s`) gives the room's object stream, read by `parseObjectData`. An opcode stays active for the following entries until the next opcode (`objects/macros.s`, `M_LASTOPCODE`), which packs the lists.

| Opcode | Entry | Bytes | Fields |
| --- | --- | --- | --- |
| `$f0` | condition | 1 | bits 0-3 the seasons the following entries apply in (Seasons); in Ages, a room property |
| `$f1` | interaction without position | 2 | id, subid |
| `$f2` | interaction | 4 | id, subid, y, x |
| `$f3` | pointer | 2 | includes a list ended by `$fe` |
| `$f4` | "before event" pointer | 2 | included if bit 7 of the room flags is 0 |
| `$f5` | "after event" pointer | 2 | included if bit 7 is 1 |
| `$f6` | random enemies | 3 | flags: bit 0 always respawns, bit 1 not counted in `wNumEnemies`, bit 2 anywhere, bits 5-7 quantity; id, subid |
| `$f7` | placed enemy A | 5 | flags: bit 0 respawns, bit 1 not counted; id, subid, y, x; the following entries omit the flags |
| `$f8` | part | 3 | id, subid, yx in two nibbles; blocks random placement at that position |
| `$f9` | object with parameter | 6 | type (0 interaction, 1 enemy B, 2 part), id, subid, var03, y, x |
| `$fa` | item drop | 3 | flags: bit 0 respawns; item; yx in nibbles; the following entries omit the flags |
| `$fe` | end of an included list | 1 | |
| `$ff` | end | 1 | |

Y and X are in pixels, at the metatile's centre; LynnaLab packs a position `(y−8, x−8)` that is a multiple of 16 into the short form.

### 5.2 Data by identifier

- `interactionData`: 3 bytes per id, object graphics header index, OAM tile base (bits 0-6), then default animation (bits 0-3), palette (bits 4-6), VRAM bank (bit 7); or `$80` as second byte and a pointer to a list by subid (`m_InteractionSubidData`, with a continuation bit).
- `enemyData`: 4 bytes per id, graphics header, `enemyCollisionMode` (bit 7 collisions on), index into `extraEnemyData`, palette and tile base; or a pointer to a list by subid.
- `partData`: 8 bytes per id, graphics header, collision mode, Y and X radii, damage, health, tile base, OAM flags, reserved.
- `objectGfxHeaderTable`: section 4.1.
- Animations: `interactionAnimationTable`, `enemyAnimationTable`, `partAnimationTable`, `specialObjectAnimation*`; per id, a list of animation pointers; each animation is a series of pairs `duration, OAM index` with an `m_AnimationLoop` loop (16-bit relative offset).
- OAM blocks: `interactionOamData`, `enemyOamData`, `partOamData`, `specialObjectOamData`; a count, then per sprite `y, x, tile, attributes`; the game adds the object's tile base and flags; the origin is the object's corner (LynnaLab subtracts 16 in Y and 8 in X to display it).

### 5.3 Static dungeon objects

`staticDungeonObjects`: per dungeon, 6-byte entries `type (3 interaction, 4 enemy, 5 part), room, id high, id low with bit 7 → bit 1 of enabled, y, x`, loaded by `parseStaticObjects`.

### 5.4 Treasures

`treasureObjectData`: per treasure identifier, either 4 bytes (`m_TreasureSubid`) or an `m_TreasurePointer` to a list of subids:

| Byte | Meaning |
| --- | --- |
| 0 | bits 4-6 spawn mode (`treasureSpawnModes.s`: instant, cloud, falls, from a chest, underwater, buried...), bits 0-2 grab mode, bit 7 pointer |
| 1 | parameter passed to `giveTreasure` (level, quantity) |
| 2 | text shown (`TX_00XX`, `$ff` none) |
| 3 | graphics: subid of interaction `$60` in `interactionData` |

`treasureObjectData` holds one 4-byte entry per treasure identifier, from 0, as many as its symbol's size divided by 4 (`$63` in both games). An entry whose byte 0 has bit 7 set is a pointer: bytes 1-2 are the address, in the same bank, of the list of that treasure's subids, 4-byte entries in the format above, and byte 3 is `$00`; a null address is no list. A list has no count and no terminator (the game indexes it by subid): it ends where the next list begins, the lists following one another after the table, and the last one ends with the section, at the end of the last `treasureObjectDataXX` label. The call transaction reads there whether the game itself gives a treasure with a parameter (byte 1 of the entry, or of one of its subids).

`treasureCollectionBehaviourTable`: 3 bytes per treasure identifier, from 0, as many as its symbol's size divided by 3 (`$68` in both games), read by `giveTreasure`:

| Byte | Meaning |
| --- | --- |
| 0 | low address, in the `$c6xx` block, of the variable the treasure acts on (`$00`: none) |
| 1 | bit 7: no sound; bits 0-3: the action with the parameter: 0 nothing more, 1 set bit `param`, 2 increment, 3 increment in BCD, 4 add `param` in BCD, 5 set to `param`, 6 set the dungeon's bit, 7 increment the dungeon's key count, 8 set to `param` if greater (a level, a companion), 9 add ring `param` unappraised, `$a` add `param`, `$b` set upgrade bit `param`, `$c` add `param` capped by the next byte (health), `$d` add `param` in BCD capped by the next byte, `$e` add rupee value `param`, `$f` add `param` in BCD capped by the satchel's level |
| 2 | sound played |

`getRupeeValue@rupeeValues` (bank 0): one 16-bit little-endian word in BCD per rupee value, the amount it stands for (`$0010`: 10 rupees), `RUPEEVAL_COUNT` of them.

Chests (`chestData.s`) name a treasure by its 16-bit index `id << 8 | subid`.

## 6. Warps

### 6.1 Sources

`warpSourcesTable`: per group, a list of 4-byte entries:

| Form | Byte 0 | Bytes 1-3 |
| --- | --- | --- |
| standard | bits 0-3 active corners (0 the whole room; bit 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right), bit 7 continuation | room, destination index, `group << 4 \| source transition` |
| pointer | `$40` | room, 16-bit pointer to a list of position warps |
| position | position `yx` in nibbles | destination index, `group << 4 \| source transition` |

End of list: a last entry marked as default (`m_WarpListEndWithDefault`), or `$ff $00 $00 $00` without a default (Ages only, automatic dungeon stairs). Screen-edge warps are found by `findScreenEdgeWarpSource`; `treeWarps.s` (Ages) handles the trees.

### 6.2 Destinations

`warpDestTable`: per group, 3-byte entries: room, position `yx` in nibbles, `parameter << 4 | destination transition`. The transition types are in `constants/common/transitions.s`: basic, with a respawn point, entering from the edge (`$3`, with `yx = $ff` to centre), without respawn, falling, portal (Ages).

## 7. Dungeons

`dungeonDataTable`: 8 bytes per dungeon: high byte of the flags' address, the room Wallmasters send back to, layout index, number of floors, base floor plus 3, floors the compass reveals, two unused bytes. `dungeonLayoutDataStart`: per floor, 64 bytes, an 8x8 grid of room indices; a dungeon's floors follow one another. The per-room properties are in section 1.

## 8. Text

A text index is `group << 8 | index` (`TX_GGII`). The text is encoded with a dictionary of fragments (`text/ages/dict.yaml`) referenced by codes, and commands prefixed by a control byte. The disassembly exposes it as YAML (`text/ages/text.yaml`) with the commands `\call`, `\jump`, `\col`, `\pos`, `\opt`, `\stop`, `\wait`, `\speed`, `\sfx`, `\num1`, `\num2`, `\Link`, `\Child`, `\item`, `\sym`, `\heartpiece`, `\secret1/2`, and symbols.

### 8.1 The US text

`textTableTable` picks, per language, a table such as `textTableENG` through a `Pointer3Byte`; `initTextboxStuff` keeps its address and bank. `getTextAddress` then reads two levels of words: the group's table offset from the language table, then the text's offset from one of two bases chosen by `textOffset1Table` or `textOffset2Table`, by language and by comparing the internal group with `TEXT_OFFSET_SPLIT_INDEX` (`code/textbox.s`). The internal groups include the dictionary's; `tools/build/parseText.py` subtracts four from the group number when it names the `TX_…` identifiers, so an internal group is not the high byte of that identifier. The two bases are data (`textOffset1Table`, `textOffset2Table`, a 4-byte entry per language: bank, address in the bank, zero); each covers `$ffff` bytes from its base, carry into the next banks included; only the split index is compiled into an instruction.

In the US ROMs the font has a fixed advance of 8 pixels (the disassembly compiles the text without `--vwf`): 16 characters per line, accented ones included; the box shows two lines, and past them an arrow waits for a button and the text scrolls. What the disassembly's encoder writes for the US version (`US_available`) is ASCII and `ÀÂÄÆÇÈÉÊËÎÏÑÖŒÙÛÜàâäæçèéêëîïñöœùûü`, at bytes `$80`-`$90` and `$a0`-`$b0`; `ô`, `Ô`, `«»`, `’` and `…` are not in it. In the dialogue box every text byte `≥ $10` is a character (`drawLineOfText`) and every byte below a control code; a character's tile is `gfx_font_start + 16 x byte`, `gfx_font_start` being `1c:4720` in both games. The US font's slots `$91`-`$9f` and `$b1`-`$ff` hold the same filler tile, which no US text emits.

## 9. Compression

### 9.1 Graphics

The mode is in bits 6-7 of byte 0 of the header; the disassembly's `.cmp` file starts with the mode byte and the 16-bit decompressed size.

| Mode | Encoding |
| --- | --- |
| 0 | raw |
| 1 | short LZ: a flag byte then eight items, bits read from bit 7 to bit 0; 0 a literal byte; 1 a reference byte: bits 0-4 distance minus 1 (32 bytes at most), bits 5-7 length minus 1; if those bits are zero, a length byte follows |
| 2 | per 16-byte tile: a 16-bit key, a common byte, then the bytes that differ; a zero key means 16 raw bytes |
| 3 | long LZ: flags as in mode 1; a 16-bit reference, bits 0-10 distance minus 1 (2048 at most), bits 11-15 length minus 2; if zero, a length byte follows |

`decompressGraphics` writes straight to VRAM or WRAM at the header's destination.

### 9.2 Small room layouts

Section 2.2, `compressData_commonByte`.

### 9.3 Dictionary LZ

Used by the large rooms (section 2.3) and by the tilesets' mappings and collisions: a flag byte then eight items; a set bit is a little-endian word, bits 0-11 offset in a 4096-byte dictionary, bits 12-15 length minus 3. For tilesets the longest copy is 18 in mode 0 and 32 in mode 1 (`compressTilesetLayoutData.py`); the mode byte of the dictionary header (`m_TilesetLayoutDictionaryHeader`, `$00` or `$80`) selects the variant.

## 10. Tile animations

`animationGroupTable`: per group, a byte whose bits 0-3 say which of the four slots are active (`$1`, `$3`, `$7`, `$f`), then as many pointers to animations. An animation is a series of pairs `duration in frames, animation header index` ended by `$ff` and a 16-bit loop offset (`m_AnimationLoop`). `animationGfxHeaders`: 6 bytes per index, the format of section 4.1, transferred at vblank. How the game steps them is in `docs/GAME_HOOKS.md`, section 5.

## 11. Other tables

| Table | File | Meaning |
| --- | --- | --- |
| Rooms where a companion can be called | `companionCallableRooms.s` | lists by companion |
| Seed tree refills | `seedTreeRefillData.s` | `group/room, data position` |
| Maple's places | `mapleLocations.s` | |
| Item data and attributes | `itemData.s`, `itemAttributes.s`, `itemUsageTables.s` | |
| Collisions by object | `objectCollisionTable.s`, `enemyActiveCollisions.s`, `partActiveCollisions.s` | |
| Terrain effects | `terrainEffects.s` | grass, puddle and shadow animations |
| Underwater surface | `underwaterSurfaceData.s` | Ages |
| Moving platforms | `movingPlatformScriptTable.s`, `movingSidescrollPlatform.s` | movement scripts |
| Dungeon properties | `dungeonData.s`, `dungeonLayouts.s`, `dungeonsUsingToggleBlocks.s` | section 7 |
| Vine positions | `defaultVinePositions.s` | Ages |
| Maps and popups | `mapTextAndPopups.s` | the menu's map |
| Signs | `signText.s` | text per sign |
| Music | `audio/ages/groupNIDs.bin` | section 1; the track format is out of scope |

## 12. Seasons' differences

Seasonal tilesets (section 3.1); tile substitutions indexed by group; Subrosia in group 1 (11x8 rooms); an overworld of 16x16 rooms instead of 14x14; eight seed trees; no `treeWarps`; text and scripts in other banks. LynnaLab handles both games with the same code and four layouts per room in Seasons.

## 13. Data overlays

Dropped: the table of what a data overlay would replace per room, tileset, dialogue, dungeon and character. The port's mods run in the game instead (`docs/MODDING.md`).

## 14. Save file

The SRAM of both games is 8 KiB (the raw `.sav`). It holds three game files and their copies, each `$550` bytes, an image of WRAM `$c5b0`-`$caff` (`code/fileManagement.s`, `@saveFileAddresses`):

| File | Copy 1 (SRAM address, offset in the `.sav`) | Copy 2 |
| --- | --- | --- |
| 1 | `$a010`, `$0010` | `$b000`, `$1000` |
| 2 | `$a560`, `$0560` | `$b550`, `$1550` |
| 3 | `$aab0`, `$0ab0` | `$baa0`, `$1aa0` |

`saveFile` writes both copies; `loadFile` reads copy 1 if it is valid, else copy 2, and `verifyFileCopies` rewrites the invalid copy from the valid one. A copy is valid when its first two bytes equal the sum, modulo `$10000`, of the `$2a7` little-endian words that follow (`calculateFileChecksum`, bytes `+2` to `+$54f`), and the next eight bytes are the game's string (`saveVerificationString`: `Z11216-0` in Seasons, `Z21216-0` in Ages).

Fields that name a room, as offsets from the start of the file (WRAM address minus `$c5b0`, the same in both games unless noted):

| Offset | Field | Content |
| --- | --- | --- |
| `+$7b` | `wDeathRespawnBuffer` (`DeathRespawnStruct`, `include/structs.s`) | group, room, `stateModifier`, direction, y, x of the respawn point |
| `+$81` to `+$83` | same structure | identifier, group and room of the remembered companion |
| `+$8a`, `+$8b` | `wMinimapGroup`, `wMinimapRoom` | the minimap's position |
| `+$11a` (Seasons), `+$120` (Ages) | `wGlobalFlags` | 128 global flags, 16 bytes |
| `+$150` to `+$54f` | room flag pages `$c700`-`$caff` | a byte per room index, pages shared by `flagLocationGroupTable` |

Changing a field of a file means recomputing its sum and writing both copies.

## 15. Room left in the US ROMs

Places nothing that reads the ROM was found to read, measured on the US ROMs and on the `[sections]` map of the pinned symbol tables; none is a reserve of the original game. After the last section of a bank, the bytes are filling: the bank's number in Seasons, `$00` in Ages. The mods' composition writes its warps and object streams there (`engine/game/data/gen_tables.py`, `FREE_TAILS`):

| Bank | Seasons | Ages |
| --- | --- | --- |
| warps (`warpSourcesTable`'s bank) | `04:7e03`-`7fff`, 509 bytes after `Warp_Data` | `04:7ede`-`7fff`, 290 bytes |
| object streams (`parseObjectData`'s bank) | `11:7eb0`-`7fff`, 336 bytes after `Objects_2` | `12:7e8f`-`7fff`, 369 bytes |
| main scripts | `0b:7f6d`-`7fff`, 147 bytes after `Scripts` | `0c:7f93`-`7fff`, 109 bytes |

An extended image also writes the header's size byte `$148` and its sums `$14d`-`$14f`.

## Sources

- disassembly: `data/ages/`, `objects/`, `rooms/`, `tileset_layouts/`, `text/`, `include/macros.s`, `include/gfxDataMacros.s`, `objects/macros.s`, `code/fileManagement.s`, `code/textbox.s`, `tools/common.py`, `tools/build/compressGfx.py`, `tools/build/compressRoomLayout.py`, `tools/build/compressTilesetLayoutData.py`, `tools/build/parseTilesetLayouts.py`, `tools/build/parseText.py`, `files.md`;
- LynnaLab: `LynnaLib/RoomLayout.cs`, `Room.cs`, `Tileset.cs`, `RealTileset.cs`, `TilesetLayoutHeaderData.cs`, `TilesetHeaderGroup.cs`, `ObjectData.cs`, `ObjectGroup.cs`, `WarpSourceData.cs`, `WarpDestData.cs`, `Chest.cs`, `TreasureGroup.cs`, `TreasureObject.cs`, `Dungeon.cs`, `AnimationGroup.cs`, `Animation.cs`, `GfxHeaderData.cs`, `PaletteHeaderData.cs`, `ObjectGfxHeaderData.cs`, `ObjectAnimation.cs`, `ObjectAnimationFrame.cs`.
