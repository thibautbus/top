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

An image with expanded tilesets (section 3.5) stores its large rooms raw: the group's table has a word per room, offset from the base plus `$200`, and the 176 bytes follow, without a dictionary (its `@loadLargeRoomLayout`). Its small rooms are as above.

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

### 3.5 Expanded tilesets

A fan game built on the disassembly's expanded tilesets (Gifts of Kinomi and Moonrise Regalia; Temple of Seasons on Seasons' engine) replaces sections 3.2 and 3.3 and the layout group of section 3.1. `expandedTilesetMappingsTable` has 3 bytes per tileset, by the low seven bits of `wTilesetIndex` (the room's byte in `roomTilesetsGroupTable`, unless the code chooses another): the data's address BE, then its bank; a first byte `$ff` makes it seasonal, the next two a little-endian pointer within the table's bank to four such entries chosen by `wRoomStateModifier` (`lookupExpandedTilesetTable`). The data is the 2048 bytes of mappings, already unfolded (8 bytes per metatile), then the 256 collisions; `loadTilesetLayout` copies both, whenever `wTilesetIndex` differs from `wLoadedTilesetLayout`: a seasonal tileset kept from one room to the next keeps the season it was loaded in.

The room's layout group is no longer the tileset's: `wLayoutGroupOverride` (the old `wTilesetLayoutGroup`) is `$ff` unless the code chooses one (the Maku tree's screen, a season); while it is `$ff`, `loadRoomLayout@getLayoutGroup` reads `@layoutGroupTable` at the group `getAdjustedRoomGroup` gives (Ages' engine: groups 0 and 1 as 2 and 3 when bit 0 of the room's flags is set; Seasons' engine: group 0 by the season).

## 4. Graphics and palettes

### 4.1 Graphics headers

`gfxHeaderTable`: `$bb` pointers to lists of 6-byte entries (`m_GfxHeaderHelper`):

| Bytes | Meaning |
| --- | --- |
| 0 | bits 0-5 source bank, bits 6-7 compression mode |
| 1-2 | source address BE |
| 3-4 | destination BE; the low 4 bits are the VRAM or WRAM bank; the address is a multiple of 16 |
| 5 | bits 0-6 size in 16-byte blocks minus 1; bit 7 set when another entry follows |

`loadGfxHeader` requires the screen off. The tilesets' unique headers (`uniqueGfxHeaders.s`, `$15` entries) have the same format, are loaded one entry per frame with the screen on, and may end with `$00, palette index`. Object headers (`objectGfxHeaders.s`, `m_ObjectGfxHeader file [, continue]`) load 32 blocks, 512 bytes, into a sprite slot; the continuation flag chains the next header for objects with more than 32 tiles. Animation headers (`animationGfxHeaders.s`, `m_GfxHeaderAnim file, destination, size, source offset`) are 6-byte entries without a continuation bit, transferred by the DMA queue at vblank. Tree headers (`treeGfxHeaders.s`) and uncompressed ones (`uncmpGfxHeaders.s`) follow the same scheme. Only the first megabyte (banks `00`-`3f`) is reachable from a header's 6-bit bank: `decompressGraphics` maps `bank and $3f`.

The European ROMs (section 8.2) have the same `$bb` headers, their table at `17:451e` in Ages and `16:4000` in Seasons (`loadGfxHeader` at `00:0626` and `00:0602`, as in the US ROMs), with one more kind of entry: a first byte `$00`, then five sources of 3 bytes (bank and mode, address BE), one per language, then the destination (2 bytes) and the size byte; `loadGfxHeader` takes the source of `wActiveLanguage` (`$c62a`). These entries carry the graphics with text: in each game the file select and its screens (copy, erase, new file, secret, link, name entry, message speed), the save menu, the game over and "to be continued" screens, the secrets' screens (their list, the hero's secret, the linked game's secret, the error), the dungeons' names and a label of the inventory. Some are split there: the European `gfx_fileselect` loads its first 16 blocks by language and the rest from one source. The title screen, the icons and the credits have one version (the credits and Seasons' caravan sprite were redrawn for every language alike). `engine/language/language_gfx.c` compares the loads of a header, US and European in each language, byte by byte, to find which US graphic carries what text (section 16).

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

In the US ROMs the font has a fixed advance of 8 pixels (the disassembly compiles the text without `--vwf`): 16 characters per line, accented ones included; the box shows two lines, and past them an arrow waits for a button and the text scrolls. What the disassembly's encoder writes for the US version (`US_available`) is ASCII and `ÀÂÄÆÇÈÉÊËÎÏÑÖŒÙÛÜàâäæçèéêëîïñöœùûü`, at bytes `$80`-`$90` and `$a0`-`$b0`; `ô`, `Ô`, `«»`, `’` and `…` are not in it. In the dialogue box every text byte `≥ $10` is a character (`drawLineOfText`) and every byte below a control code; a character's tile is `gfx_font_start + 16 x byte`, `gfx_font_start` being `1c:4720` in both games, read from the ROM as each character is drawn. The US font's slots `$91`-`$9f` and `$b1`-`$ff` hold filler tiles (three patterns, repeated), which no US text emits.

The control codes take one byte of parameter from `$06` to `$0f`: `$06` a symbol or an item's icon, `$07` `\jump` (the text goes on at another index of its group), `$08` (the shops': the text ends, and the game shows another box after it), `$09` a colour, `$0a` a name (`\Link`, `\Child`, the secrets), `$0b` a character's sound, `$0c` a command by its bits 3-7 (0 the speed, 1 `\num1`, 2 `\opt`, 3 `\stop`, which waits for a button and opens the next box, 4 `\pos`, read only as a text's first command, by `checkInitialTextCommands`, 5 `\heartpiece`, drawn apart from the line, 6 `\num2`, 7 `\slow`), `$0d` a wait, `$0e` a sound, `$0f` `\call` (another text inserted). `$02`-`$05` insert a fragment of dictionary group 0 to 3, `$01` breaks the line, `$00` ends the text.

### 8.2 The European text

The European ROMs (`AZ8P`, `AZ7P`, 2 MiB) hold five languages under the US texts' identifiers, in the same format: index 0 English, 1 French, 2 German, 3 Italian, 4 Spanish. Their code is not the US code: the places come from the disassembly's dumper (`tools/dump/dumpText.py`), pinned by the ROM's SHA-1 (`engine/language/language_rom.c`):

| | Ages (`9f2dff3c…`) | Seasons (`360b9408…`) |
| --- | --- | --- |
| language table (a `Pointer3Byte` per language) | `3f:5015` | `3f:5044` |
| three bases (4 bytes per language: bank, address in the bank, 0) | `3f:4fd9`, `3f:4fed`, `3f:5001` | `3f:5008`, `3f:501c`, `3f:5030` |
| first internal group of the second, the third base | `$1a`, `$34` | `$1a`, `$34` |
| font (`gfx_font_start`) | `1b:4720` | `1b:4720` |

A group's list ends where the next text group's begins, the last where the first base begins; a group that shares group 0's list is empty. Some languages leave dictionary groups 1 to 3 pointing into texts they never insert from. Ages has 2,824 texts in each language; Seasons 1,884 in each non-English language, the US 1,885: a group has one text more in Europe, and two US texts have none. The European font differs from the US one on 35 glyphs: 22 in the US filler slots (`$91`, `$98`, `$99`, `$b1`, `$c0`-`$c8`, `$d0`-`$d8`: the sharp s, the inverted marks, an acute accent alone, a degree sign, a feminine ordinal, and accented capitals and small letters) and 13 redrawn (`$17`, `$18`, `$26`, `$32`, `$33`, `$35`, `$36`, `$43`, `$47`, `$7f`, `$a4`, `$a9`, `$aa`). The European English equals the US text but on 70 texts in Ages and 42 in Seasons; it is served as a language of its own, English (EU), as the four translations are (section 8.3).

### 8.3 The European text on the US code

The European version is a revision: most of its English differs from the US by a fix or a colour, and the four other languages translate it; a few texts moved or were rewritten. Measured on the two European ROMs above (`config/language_exceptions_ages.tsv`, `config/language_exceptions_seasons.tsv`, pinned by their SHA-1):

- moved: Ages `TX_0a53` (the Wild Tokay's question) is a secret's text in Europe, the question also being at `TX_0a51` and `TX_0a54`-`TX_0a5f`; Ages `TX_302e` is another text in four languages, `TX_302f` being the same as the US `TX_302e` in all five; the two Seasons texts the four translations lack (`TX_0400`, `TX_2d00`) are empty, as `TX_0900` is in every language, the European English included;
- rewritten: the lines of group `$2f` that the Buzz Blob turned Cukeman says at random (`TX_2f1e` + 0 to 7 in both games; Seasons also has `TX_2f19`-`TX_2f1d` with the first line) are hints in Europe, where the US has jokes: no translation of the US lines exists, and a language serves the European hints, official; Seasons `TX_2e0d`, a sign, is rewritten as `TX_2c00` is, which every language translates as rewritten;
- laid out for the European code: the European engine ends a text at a `\pos` past its start, where the US engine does nothing and would print the next text's words that follow it (German Ages `TX_2815`, Italian and Spanish `TX_2f0c`, Spanish `TX_2803`, `TX_2f10`-`TX_2f12`); a `\pos` placed after a sound, where the US engine reads it only first (Seasons `TX_0b4d`, `TX_0b4e`, `TX_0b50` in all four languages); the German shops print their prices and their choice themselves where the US texts print `\num1` and go on at the shop's choice with a `\jump`; the Ring Box shows the box's capacity with a `\num1` (`TX_0034`, German, Italian, Spanish) the US code does not set; a contribution's last choice is 1 Rupee where the US code takes 0 (Ages `TX_4b02`); German calls another text than the US one (`\call`, Ages `TX_0116`, `TX_3150`, `TX_3152`); a few texts place their box or play their sound otherwise (`\pos`, `\sfx`).

What the US engine reads of a text ends at its first `\jump` or `$08`: the bytes the decoder gives after them belong to the texts that follow it in the ROM. Compared that way, a European text serves a US one when it gives the US engine the same commands, the layout's aside (line breaks, `\stop`, `\Link`, `\Child`, colours): 2,789 to 2,812 of Ages' 2,824 texts per translation, 1,864 to 1,865 of Seasons' 1,885; in English (EU), 2,820 and 1,883, the others being the moved texts, the Ring Box's count (`number-as-more`, "more": the US text) and the contribution's sums (Seasons' three boxes placed after a sound, adapted in the translations, are the US text there). The others are served by the table: a moved text (`eu:`), a rule that adapts the European text to the US code at extraction (`rule:`, `engine/language/language_rules.c`: `lead`, `end-at-pos`, `us-call`, `shop-price`, `join-next`, `number-as-more`, `option-numbers`, `unshown-number`, `symbols`), or the port's own translation (`own:`, `config/language_own.tsv`). An adapted text must give the US engine the US text's commands (a `\pos` counting only first), fit sixteen characters a line and not end on a line break.

The port's own texts are written in UTF-8 with these characters past ASCII (the European font draws all of them; `?` where a byte is none of a language's letters):

| | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | a | b | c | d | e | f |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `$8x` | À | Â | Ä | Æ | Ç | È | É | Ê | Ë | Î | Ï | Ñ | Ö | Œ | Ù | Û |
| `$9x` | Ü | ß | ? | ? | ? | ? | ? | ? | ¡ | ¿ | ? | ? | ? | ? | ? | ? |
| `$ax` | à | â | ä | æ | ç | è | é | ê | ë | î | ï | ñ | ö | œ | ù | û |
| `$bx` | ü | ´ | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? | ? |
| `$cx` | Ô | ° | Á | Í | Ó | Ú | Ì | Ò | Å | ? | ? | ? | ? | ? | ? | ? |
| `$dx` | ô | ª | á | í | ó | ú | ì | ò | å | ? | ? | ? | ? | ? | ? | ? |

### 8.4 The texts laid out for the compact text

The game breaks no line: a text's line breaks are in its bytes, made for sixteen cells of eight pixels, and a box shows two lines. The compact text draws a line in the font's own widths (`docs/GAME_HOOKS.md`, section 6), so that 128 pixels hold about 18 characters; the texts are laid out again for it when a language is composed (`engine/language/language_wrap.c`), English (US) included, whose index then serves the US texts laid out again and keeps the others in English (section 16); a fan game's English the same way, its texts read in its image (its own and those it shares with the US ROM), its index in the banks it leaves to a language; its translations are not laid out.

- **Pages.** A text is cut where the game opens a box: after a `\stop` (`$0c $18`), always followed by a line break or the text's end, or a `\heartpiece`. Each page is laid out alone: its line breaks become spaces and its words go back on the fewest lines the width allows, or one more when the page's last box holds it; among those layouts, the one whose breaks cost least: a line's empty pixels squared, a break within a colour (a name, an item) 4096, at a hyphen 2500, not after a punctuation mark 900, a line more than the fewest 1600.
- **Hyphens.** A word a line break cut after a hyphen is joined again, with the hyphen when a capital follows, when the language writes the whole word with its hyphen inside a line elsewhere, or writes its second part after a hyphen (the French `-vous`, `-ce`). Before a small letter, German and Italian, which break their words by syllables at a line's end, join it without the hyphen; in English, French and Spanish a hyphen at a line's end is a compound's (`lava-`/`like`, `grand-`/`chose`, `pensent-`/`ils`) and stays, inside a line too, but for a word the language writes whole elsewhere (`some-`/`thing`, `don-`/`neront`) or neither of whose parts it writes alone (`appara-`/`tront`), joined without it. A word joined without its hyphen keeps its place as a break the layout may take, hyphen shown; a dash of two hyphens joins without a space. Measured: Ages' German breaks a word 955 times at a line's end, Italian 135, French and English 17, Spanish never; the layout joins 1,125 hyphens in Ages' German, 129 in Italian, 18 in English, 10 in French.
- **The measure** is the transaction's, its rule shared (`oracles_text_place`): a character's ink and a column, a space 5 pixels, a character whose ink reaches its cell's right edge its ink alone (the halves of the A and B buttons, `$b8 $b9` and `$ba $bb`, stay joined), a symbol or an item's icon on its whole tile from the next tile's edge, a character with ink starting the next tile when the tile it would share holds ink of another attribute; a name (`\Link`, `\Child`, 5 letters at most) as five of the widest letters of the US name screen, a number (`\num1`, `\num2`) as three of the widest digits; the attributes as `\col` sets them (`$80` white and red, `$81` red behind the sprites, blue and white of palette 1, the parameter itself with bit 7), from either attribute a box opens with (`$80`, `$81` with `TEXTBOXFLAG_ALTPALETTE1`). A line holds 64 characters at most, two pixels each.
- **Kept as the game lays them out.** A page that lays out a choice (`\opt`, whose cursor the game draws on the box's bottom row), shows a secret (`$0a` 2 or 3), calls another text (`\call`), shows the piece of heart, holds a dictionary code or aligns its lines with spaces (a line starting with one, two in a row, an empty line); a whole text that shows a single box, and a text from its first `\jump` or `$08` on, what follows being another text's. A text another text calls or jumps to within a line (the call not alone on its line), whose first line goes on a line the caller started; the inventory's texts (`TX_09xx`), which scroll on a line of their own; the texts `config/vwf_keep_ages.tsv` and `config/vwf_keep_seasons.tsv` name by identifier and language (the songs, a verse a line; a word broken at a line's end the hyphen rules would get wrong: a syllable break one of whose parts the language writes alone and never the word whole, a compound neither of whose parts it writes alone), and for a fan game its own table (`config/vwf_keep_moonrise.tsv`, `vwf_keep_kinomi.tsv`, `vwf_keep_temple.tsv`: the songs of its Oracle it keeps, Kinomi's credits, which put the game's title on a line of its own, a sign of Temple's whose arrows each start a line, the same broken words).
- **Checks.** A text laid out again gives the same commands as before (`oracles_text_same_commands`), the same characters, its hyphens included, but its spaces, line breaks and the hyphens of the words it joined, every line within the width from either opening attribute, no empty line the layout made (a page kept as it is keeps its own) and no more boxes; one that does not keeps the game's layout, and is counted (none does in the user's ROMs and fan games).

Measured on the user's ROMs, the boxes of all the texts and, between brackets, of the box's texts without a choice, a call, a secret or a jump (the study's measure): Ages 7,911 to 6,318 in English (US), -20.1 % (-23.3 %); 7,922 to 6,326 in English (EU), -20.1 % (-23.3 %); French 8,481 to 6,431, -24.2 % (-27.9 %); German 8,875 to 7,244, -18.4 % (-21.3 %); Italian 7,806 to 6,106, -21.8 % (-25.2 %); Spanish 8,595 to 6,733, -21.7 % (-25.0 %). Seasons 4,833 to 4,038 in English (US), -16.4 % (-19.6 %); 4,853 to 4,053 in English (EU), -16.5 % (-19.6 %); French 5,391 to 4,212, -21.9 % (-26.3 %); German 5,487 to 4,532, -17.4 % (-20.9 %); Italian 4,946 to 3,985, -19.4 % (-23.1 %); Spanish 5,241 to 4,269, -18.5 % (-22.2 %). From 929 (Seasons, English) to 1,764 texts (Ages, German) are laid out again; 16 to 24 are kept by the tables or a call within a line. The fan games, in their English: Moonrise Regalia 8,075 to 6,474, -19.8 % (-22.8 %), 1,637 texts laid out again, 18 kept; Gifts of Kinomi 8,918 to 7,074, -20.7 % (-23.7 %), 1,726 laid out again, 24 kept; Temple of Seasons 4,927 to 4,120, -16.4 % (-19.4 %), 919 laid out again, 24 kept.

## 9. Compression

### 9.1 Graphics

The mode is in bits 6-7 of byte 0 of the header; the disassembly's `.cmp` file starts with the mode byte and the 16-bit decompressed size.

| Mode | Encoding |
| --- | --- |
| 0 | raw |
| 1 | short LZ: a flag byte then eight items, bits read from bit 7 to bit 0; 0 a literal byte; 1 a reference byte: bits 0-4 distance minus 1 (32 bytes at most), bits 5-7 length minus 1; if those bits are zero, a length byte follows |
| 2 | per 16-byte tile: a 16-bit key, a common byte, then the bytes that differ; a zero key means 16 raw bytes |
| 3 | long LZ: flags as in mode 1; a 16-bit reference, bits 0-10 distance minus 1 (2048 at most), bits 11-15 length minus 2; if zero, a length byte follows |

`decompressGraphics` writes straight to VRAM or WRAM at the header's destination. It reads its source sequentially across banks (`readByteSequential`, `_adjustHLSequential`: past `$7fff`, `$4000` of the next bank), and a reference copies from the bytes already written, byte by byte, so that it may overlap them; it stops when the size is written, in the middle of a reference if need be.

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

The language and the fields that name a room, as offsets from the start of the file (WRAM address minus `$c5b0`, the same in both games unless noted):

| Offset | Field | Content |
| --- | --- | --- |
| `+$7a` | `wActiveLanguage` | the language, 0 in the US games: English, French, German, Italian, Spanish (0 to 4) in the European ones |
| `+$7b` | `wDeathRespawnBuffer` (`DeathRespawnStruct`, `include/structs.s`) | group, room, `stateModifier`, direction, y, x of the respawn point |
| `+$81` to `+$83` | same structure | identifier, group and room of the remembered companion |
| `+$8a`, `+$8b` | `wMinimapGroup`, `wMinimapRoom` | the minimap's position |
| `+$11a` (Seasons), `+$120` (Ages) | `wGlobalFlags` | 128 global flags, 16 bytes |
| `+$150` to `+$54f` | room flag pages `$c700`-`$caff` | a byte per room index, pages shared by `flagLocationGroupTable` |

Changing a field of a file means recomputing its sum and writing both copies.

The European ROMs keep the same files, the same sum and the same strings, and add a header: `L_O_C_A_L_I_Z_E` at `$a000`-`$a00e` (offset `$0000`) and the language chosen at `$a00f`, which their `loadFile` copies into `wActiveLanguage` over the file's own, and `saveFile` then writes to `+$7a`. A US save loads in a European game, which asks for the language first, the header being absent. A European save loads in the US game, which ignores the header but reads `+$7a`: its `textOffset2Table` holds the right offsets for 0 and 1 only, and with 2, 3 or 4 every text of a high index `$2c` or more is read at the wrong place (signs, rings' names, many dialogues). Setting `+$7a` to 0 in each valid copy, its sum recomputed, is all such a save needs (`engine/rom/save.c`).

## 15. Room left in the US ROMs

Places nothing that reads the ROM was found to read, measured on the US ROMs and on the `[sections]` map of the pinned symbol tables; none is a reserve of the original game. After the last section of a bank, the bytes are filling: the bank's number in Seasons, `$00` in Ages. The mods' composition writes its warps and object streams there (`engine/game/data/gen_tables.py`, `FREE_TAILS`):

| Bank | Seasons | Ages |
| --- | --- | --- |
| warps (`warpSourcesTable`'s bank) | `04:7e03`-`7fff`, 509 bytes after `Warp_Data` | `04:7ede`-`7fff`, 290 bytes |
| object streams (`parseObjectData`'s bank) | `11:7eb0`-`7fff`, 336 bytes after `Objects_2` | `12:7e8f`-`7fff`, 369 bytes |
| main scripts | `0b:7f6d`-`7fff`, 147 bytes after `Scripts` | `0c:7f93`-`7fff`, 109 bytes |

The graphics are a stream across banks `$1a` to `$38` (`gfxDataMain.s`); after the last file of a bank comes filling. A language's graphics use the tails of the last bank and of Seasons' bank `$1a` (section 16): `38:6aec`-`7fff` in Ages, 5,396 bytes after `oth_twinrova_normal_layout`; `38:7e58`-`7fff` in Seasons, 424 bytes after `spr_caravan`, and `1a:70e0`-`7fff`, 3,872 bytes after `spr_magnet_gloves`.

An extended image also writes the header's size byte `$148` and its sums `$14d`-`$14f`.

## 16. The extended image

An image the port composes is 2 MiB, header byte `$148` = `$06` and both sums rewritten. The extension's banks `$40`-`$7f`, which the US code never maps, are shared: the mods' houses take them from `$40` up to `$6f` (`engine/mods/mod_compose.c`), a language from `$70` to the end (`engine/language/language_compose.c`, `ORACLES_IMAGE_LANGUAGE_BANK`); the houses' composition runs first and leaves the language's banks empty.

A language's index starts bank `$70`: the 16 bytes `ORACLES LANGUAGE`, a version byte (2), the number of internal text groups, the language's code (8 bytes: its characters, `fr` or `en-eu`, then zeros); from offset `$20`, 4 bytes per internal group, the offset of its entries from the bank's start and their number (16 bits each, 0 for the dictionary's groups); an entry, 3 bytes per text: the bank and the CPU address (`$4000`-`$7fff`) of the language's text, bit 15 of the address set for a text laid out again for the compact text (section 8.4), or bank 0 for a text the game reads in English, address 1 when the language keeps it, 0 when it has none. With the compact text, English (US) has an index too (code `en`): its entries name the US texts laid out again, the others the game's own; and so does a fan game's English, in the banks its profile leaves to a language. The texts follow, each within one bank, identical texts once, decoded: the dictionary expanded, the control codes as they are. They are the language pack's (`engine/language/language_pack.c`, format 7: its European ROM's SHA-1, then the SHA-1 of the image whose texts it translates, then its fingerprint; each language named by its code, preceded by its length; last, the SHA-1 of every byte before it), which holds an entry per US text, with its source: the European text of its identifier, a moved one, one adapted by a rule, the port's own, or none, the US English then being kept (section 8.3), a mistake of the official translation corrected where `config/language_corrections.tsv` names one (a word or a phrase replaced, whole words, both written in `config/language_own.tsv`'s syntax so that a phrase may run over a line break or a colour and its correction break the line again; the text still fitting its lines and keeping the US commands); the pack is extracted on the user's machine and checked there, every text it serves against the US one. The glyphs that differ are written over the US font at `gfx_font_start`.

The graphics that carry text are not in the language's banks: a header's 6-bit bank does not reach them (section 4.1). The pack holds, per language, each US graphic that differs in it (34 in Ages, 33 in Seasons, in every translation; 22 and 21 in English (EU), whose dungeons' names and "to be continued" are the US ones, and in Seasons the Hero's Cave's name too), named by the US source its headers point to (bank, address), with its size in blocks and its bytes compressed in the mode that makes them shortest (raw, common byte, or one of the two LZ, section 9.1; an optimal parse, from 14.4 to 14.9 KB a translation, 9.3 to 9.5 KB in English (EU)): made at extraction from the European loads (section 4.1), every byte a US entry is the last to write taken from what the European game writes there in the language. The composition writes them in place of the US graphics they replace, whose bytes no other header reads, and in the graphics banks' tails (section 15), the longest first, each where it first fits; then every US entry that named a replaced graphic gets its new bank, mode and address, its destination and size unchanged. In Ages the slots so freed hold 14,643 bytes, in Seasons 14,548, the tails 5,396 and 4,296. A pack's fingerprint is the SHA-1 of the revision of the code that makes it (`ORACLES_LANGUAGE_PACK_REVISION`, raised by hand when that code makes something else from the same ROMs), of the tables it is made from (`config/language_exceptions_<game>.tsv`, `config/language_own.tsv`, `config/language_corrections.tsv`, embedded at build) and of the text tables and the font its image's profile binds. A pack of an earlier format, of another fingerprint or whose checksum is not its content's is extracted again from the European ROM.

A fan game made on a US ROM keeps its size (2 MiB for Moonrise Regalia 1.0.6 and Gifts of Kinomi 1.1.2, 4 MiB for Temple of Seasons 1.073) and its code; its profile binds its `getTextAddress` and text tables, found as the US routine with other operands (Moonrise `3f:4f63`, tables `3f:4fbd` and `3f:4fd5`, `textTableTable` `3f:4fed`, split `$2e`; Kinomi `3f:4f7b`, `3f:4fd5`, `3f:4fed`, `3f:5005`, `$28`; Temple `3f:4fa0`, `3f:4ffa`, `3f:5012`, `3f:502a`, `$4e`; `wTextIndexH`/`L` `$cba3`/`$cba2` and `w7ActiveBank` `7:$d0d4` in all three, the font at `1c:4720`), and the first of the banks up to `$7f` it leaves empty, every byte 0: `$71` in Moonrise (its bank `$70` holds data), `$72` in Kinomi (`$70` and `$71`), `$70` in Temple (empty from `$5f`). The index starts that bank, the texts follow it up to `$7f`; the header's size byte stays, its sums are rewritten. A fan game's pack is made from its Oracle's (`engine/language/language_fan.c`): a fan text whose decoded bytes are the US text of the same identifier takes that identifier's entry, one that is the US text of another identifier the first such identifier's (source `moved`), the others stay in the fan game's English (source `fan`); the European glyphs are written where the fan game left the US font's glyph (all 35: the three changed only `$9f`, which none of them is). Its graphics are its own: no header is repointed. Measured: Moonrise 2,838 texts, 2,534 the US text of their identifier, 33 of another, 2,567 translated in each language and 271 in English, from 141,674 (Italian) to 175,111 bytes (German) in 10 to 12 banks; Kinomi 2,953, 2,453 and 4, 2,457 translated and 496 in English, 136,498 to 168,243 bytes in 9 to 11 banks; Temple 1,906, 1,521 and 37, 1,558 translated and 348 in English, 101,815 to 123,018 bytes in 7 or 8 banks.

## 17. A room's tile substitutions

`loadTilesetAndRoomLayout` decodes the room's layout (section 2) into `wRoomLayout`, its 192 bytes cleared first, then `applyAllTileSubstitutions` changes its tiles from the tables below and the RAM; `loadRoomCollisions` maps each of the 176 bytes through `w3TileCollisions`, the tileset layout's collisions (section 3), and writes `$ff` over the sixteenth column and, in groups 0 to 3, the ninth row and the eleventh column. The continuous transitions predict a scroll's room from them while the game loads it (`engine/gameplay/room_predict.c`). In the order of Ages (`code/ages/tileSubstitutions.s`), Seasons doing the same less the pollution and the time warps (`code/seasons/tileSubstitutions.s`):

| Table | Format | Applied |
| --- | --- | --- |
| `replacePollutionWithWaterIfPollutionFixed@aboveWaterReplacement`, `@belowWaterReplacement` | pairs of the tile to put and the tile it replaces everywhere, ended by `$00`: in Ages `$fc` for `$eb`, `$3b` for `$eb` under water | Ages, `GLOBALFLAG_WATER_POLLUTION_FIXED` set, the tileset outdoors |
| `singleTileChangeGroupTable` | a pointer by group to entries of room, mask, position, tile; ended by a mask `$00` | the mask against the room's flags; in Ages' engine `$f0` unlinked, `$f1` linked, `$f2` the game finished (its absence ends the list) |
| `standardTileSubstitutions` | for bits 0, 1, 2, 3 and 7 of the room's flags, a pointer per collision mode (Ages, 6) or per group (Seasons, 8) to pairs of the tile to put and the tile it replaces everywhere, ended by `$00` | the bit set |
| `chestDataGroupTable` | a pointer by group to entries of position, room and contents (2 bytes), ended by a position `$ff` | `ROOMFLAG_ITEM`: `TILEINDEX_CHEST_OPENED` at the chest's position (past the layout for a room without one); where an image has `replaceOpenedChest@exceptionTable` (the rooms' low bytes, ended by `$00`), its rooms of group 0 keep their chest closed while bit 0 of the past's room `$20` is clear |
| `roomTileChangerCodeGroupTable` | a pointer by group to pairs of room and routine, ended by a room `$00` (`findRoomSpecificData`) | the room's own code; Seasons runs it in groups 0, 1 and 3. Replayed in groups 0 to 3, each routine for its own room alone (`room_predict.c`): a few writes under a flag of the room, of another room of its group (`getARoomFlags`), of another page by name (Ages' `wPresentRoomFlags` and `wPastRoomFlags`, the vines' positions at `wPastRoomFlags+$f0`; Seasons' `wSubrosiaRoomFlags`, `wGroup4RoomFlags`), a global flag, the season, or a variable of the game's progress (`hGameboyType`, `wRickyState`, `wEssencesObtained`, `wAnimalCompanion`, `wBoughtShopItems1`, `wSeedTreeRefilledBitset`, `wObtainedTreasureFlags`); a withered vine reads `w3TileCollisions`, loaded before the layout. On the US images alone (their profiles), a byte the image does not have refusing the room; beside a corner, a routine that reads `wSeedTreeRefilledBitset` is refused, which the scroll into its room may change before the load (`updateSeedTreeRefillData`: Seasons' 0:6f) |
| `timewarpEntryTileReplacementDict`, `timewarpReturnTileReplacementDict` | pairs of a tile and its replacement, ended by `$00` (`lookupKey`) | Ages' groups 0 and 1: the tile at `wPortalPos` in the portal's room and at `wWarpDestPos` after a time warp (the first), at `wLinkTimeWarpTile` - 1 for Link sent back (the second) |
| `applyRoomSpecificTileChangesAfterGfxLoad@tileChangesGroupTable` | as `roomTileChangerCodeGroupTable` | after the graphics (`generateVramTilesWithRoomChanges`): routines that draw alone, others that change the layout and the collisions; the Gasha spot's (`$08`) only where something is planted (`gashaSpotRooms`, the rooms' low bytes, the spot's bit in `wGashaSpotsPlantedBitset`) |

Groups 4 and 5 also run the shutters, the switches, the toggle blocks and Jabu-Jabu's water (`replaceShutterForLinkEntering` to `replaceJabuTilesIfUnderwater`), and Seasons' Temple Remains filled with lava take the layout of another room (`@adjustLoadingRoomForTempleRemains`): none of them is predicted.

With the transitions at the line, the room beside the room a scroll enters, across its axis, is predicted the same way before any scroll loads it: its tileset is `loadTilesetData`'s choice made again, the room's byte in `roomTilesetsGroupTable` (in Ages, groups 0 and 1, the group of the layout swapped when bit 0 of the room's flags is set: `getAdjustedRoomGroup`), its 8 bytes in `tilesetData` (Seasons: a seasonal one's entry of the season in `wRoomStateModifier`), from which come its collision mode, its flags, its tileset layout and its layout group. Only on Ages' two overworlds and Seasons' Holodrum and Subrosia, within the group's map (Subrosia's own 11 by 8, `SUBROSIA_WIDTH` and `SUBROSIA_HEIGHT`; from its first column and row where a fan game walls it inside the grid, as Temple of Seasons does), between two rooms of one pack in `roomPackData` (a scroll between two packs fades, or sets the season: `checkRoomPack`; Ages compares their low seven bits, Seasons compares them in Holodrum only, its `checkRoomPack` and `loadScreenMusic` leaving Subrosia's scrolls alone), outdoors, and not where the game's own code chooses the tileset (Ages' Maku Tree, `checkTilesetOverride`, and Nuun Highlands, pack `$7f`; Seasons' Moblin's Keep in Holodrum, `getMoblinKeepSeasonsTilesetData`, its five rooms read from the image's `moblinKeepRooms`, which Temple of Seasons changed). Nor where one of the room entered's objects, live since its load, is one `engine/gameplay/gen_room_scenes.py` marks from the sources (a subid of an interaction where its code chooses by subid).

On an image with expanded tilesets (section 3.5), the tileset's collisions and the room's layout group are read as that section says (a seasonal tileset whose seasons' collisions differ, only where it is loaded again), and so is the room beside, where the image's `checkTilesetOverride` is the one followed: on Ages' engine, Moonrise Regalia's is the base game's (the Maku Tree's screen, Nuun Highlands' pack `$7f`), Gifts of Kinomi's gives every pack from `$7f` a tileset of the season, left to its load, as is a seasonal expanded tileset beside; Temple of Seasons follows Seasons' rules.

## Sources

- disassembly: `data/ages/`, `objects/`, `rooms/`, `tileset_layouts/`, `text/`, `include/macros.s`, `include/gfxDataMacros.s`, `objects/macros.s`, `code/fileManagement.s`, `code/textbox.s`, `tools/common.py`, `tools/build/compressGfx.py`, `tools/build/compressRoomLayout.py`, `tools/build/compressTilesetLayoutData.py`, `tools/build/parseTilesetLayouts.py`, `tools/build/parseText.py`, `files.md`;
- LynnaLab: `LynnaLib/RoomLayout.cs`, `Room.cs`, `Tileset.cs`, `RealTileset.cs`, `TilesetLayoutHeaderData.cs`, `TilesetHeaderGroup.cs`, `ObjectData.cs`, `ObjectGroup.cs`, `WarpSourceData.cs`, `WarpDestData.cs`, `Chest.cs`, `TreasureGroup.cs`, `TreasureObject.cs`, `Dungeon.cs`, `AnimationGroup.cs`, `Animation.cs`, `GfxHeaderData.cs`, `PaletteHeaderData.cs`, `ObjectGfxHeaderData.cs`, `ObjectAnimation.cs`, `ObjectAnimationFrame.cs`.
