/* The cache key of a neighbour's terrain, the tiles that depend on the entry,
 * and the read trace that measured the key. */
#include "ghost_internal.h"

void oracles_ghost_on_read(void *opaque, uint16_t address)
{
    OraclesGhost *g = opaque;
    /* The load's reads outside the substitutions, any thread, counted apart: the trace below stays the substitutions'. */
    if (g->load_counts && (!g->in_substitutions || g->in_object_gfx)) { g->load_counts[address]++; return; }
    /* applyAllTileSubstitutions can wait for the vblank (Seasons: the object
     * graphics of Subrosia load over several frames) and the other threads
     * run meanwhile, on their own stacks: their reads are not the
     * substitutions'.  The thread is told by the stack pointer. */
    const uint16_t sp = oracles_guest_sp(g->guest);
    if (g->trace_stack_top && (sp < g->trace_stack_start || sp > g->trace_stack_top)) { if (g->result) g->result->reads_other_thread++; return; }
    if (g->read_counts) g->read_counts[address]++;
    g->reads_this_run++;
    if (!g->result || address < WRAM_BANK0_BASE) return;
    if (address >= 0xe000u && address < 0xff80u) return;
    if (address >= g->layout_start && address < g->layout_end) return;
    if (address >= g->stacks_start && address < g->stacks_end) return;
    if (g->seen[address >> 3] & (1u << (address & 7u))) return;
    g->seen[address >> 3] |= (uint8_t)(1u << (address & 7u));
    if (g->result->read_count < ORACLES_GHOST_RUN_READS) g->result->reads[g->result->read_count++] = address;
    else g->result->reads_dropped++;
}

/* The stack the substitutions run on (the thread that entered them): reads
 * from another stack are another thread's. */
void oracles_ghost_trace_stack_of_entry(OraclesGhost *g)
{
    const OraclesGuestTables *t = oracles_guest_tables(g->guest);
    const uint16_t sp = oracles_guest_sp(g->guest);
    const OraclesGuestSym stacks[5][2] = { { t->main_stack, t->main_stack_top }, { t->thread0_stack, t->thread0_stack_top },
                                           { t->thread1_stack, t->thread1_stack_top }, { t->thread2_stack, t->thread2_stack_top }, { t->thread3_stack, t->thread3_stack_top } };
    g->trace_stack_start = g->trace_stack_top = 0;
    for (unsigned i = 0; i < 5; i++)
        if (stacks[i][0].bank != ORACLES_GUEST_ABSENT && sp >= stacks[i][0].addr && sp <= stacks[i][1].addr) { g->trace_stack_start = stacks[i][0].addr; g->trace_stack_top = stacks[i][1].addr; }
}

size_t oracles_ghost_key_snapshot(OraclesGuest *guest, uint8_t *out, size_t capacity)
{
    const OraclesGuestTables *t = oracles_guest_tables(guest);
    OraclesGhostKeyRange ranges[ORACLES_GHOST_KEY_RANGES];
    const unsigned count = oracles_ghost_key_ranges(oracles_guest_profile(guest), ranges, ORACLES_GHOST_KEY_RANGES);
    size_t n = 0;
    for (unsigned k = 0; k < count; k++) {
        if (ranges[k].address == t->active_group.addr || ranges[k].address == t->active_room.addr) continue;
        const OraclesGuestSym sym = { 0, ranges[k].address };
        const uint8_t *p = oracles_guest_ptr(guest, sym, ranges[k].length);
        if (!p || n + ranges[k].length > capacity) return 0;
        memcpy(out + n, p, ranges[k].length);
        n += ranges[k].length;
    }
    return n;
}

/* ---- the cache key and the entry-dependent tiles ---------------------------------- */

static unsigned add_range(OraclesGhostKeyRange out[], unsigned n, unsigned max, OraclesGuestSym sym, uint16_t length)
{
    if (sym.bank == ORACLES_GUEST_ABSENT || n >= max) return n;
    out[n].address = sym.addr;
    out[n].length = length;
    return n + 1;
}

/* The key and the exempt list are the closed account of what the substitutions
 * read, established by reading them and their callees in both games
 * (code/<game>/tileSubstitutions.s, commonTileSubstitutions.s,
 * code/<game>/roomSpecificTileChanges.s, and what they call in bank0.s, the
 * room-specific routines one by one, the fall of Ages' group4Map1b into
 * group2Map7e included) and checked by the trace on every route.  The window is
 * the trace's, applyAllTileSubstitutions: the load around it (loadTilesetData,
 * loadRoomLayout, applyRoomSpecificTileChangesAfterGfxLoad) reads live state
 * too, and an entry is not invalidated by a byte only they read, whether in
 * the key or not (the season of Seasons aside, compared on its own): the
 * coarse list below is their account. */
unsigned oracles_ghost_key_ranges(const OraclesCompatProfile *profile, OraclesGhostKeyRange out[], unsigned max)
{
    const OraclesGuestTables *t = oracles_compat_guest_tables(profile);
    if (!t) return 0;
    unsigned n = 0;
    n = add_range(out, n, max, t->active_group, 1);          /* group tests, getThisRoomFlags */
    n = add_range(out, n, max, t->active_room, 1);           /* getThisRoomFlags, room-specific dispatch */
    /* wTilesetFlags and wActiveCollisions are left out
     * (oracles_ghost_key_exempt_ranges): the substitutions test the flags
     * (outdoors, underwater, dungeon, sidescroll), and Ages' take their table
     * by the collision mode (applyStandardTileSubstitutions; Seasons' take it
     * by wActiveGroup and never read the mode), but loadTilesetData
     * has written both from the loaded room's own tileset before they run, so
     * the values are the room's, not the live state's; keyed on them, every
     * room load (each warp) threw the whole cache for nothing, and a change
     * of area, a dive into the sea or a return to its surface threw the rooms
     * of the other side. */
    n = add_range(out, n, max, t->dungeon_index, 1);         /* checkDungeonUsesToggleBlocks, replaceJabuTilesIfUnderwater */
    n = add_range(out, n, max, t->dungeon_floor, 1);         /* replaceJabuTilesIfUnderwater, Jabu-Jabu's platforms (group5Map5c, 5d) */
    n = add_range(out, n, max, t->jabu_water_level, 1);      /* replaceJabuTilesIfUnderwater and the room-specific changes of Jabu-Jabu (Ages; measured on ages/underwater.route) */
    /* Seasons: the season (Holly's house, the swamp's keylock).  Ages' substitutions
     * never read it: calculateRoomStateModifier sets it in initializeRoom, after
     * them, from the room's own tileset and flags (under water, a layout swapped),
     * and keyed on it a room of the sea computed ahead from its surface, where it
     * is 0, never held for the live key. */
    if (oracles_compat_seasons_rules(profile)) n = add_range(out, n, max, t->room_state_modifier, 1);
    n = add_range(out, n, max, t->toggle_blocks_state, 1);   /* replaceToggleBlocks */
    n = add_range(out, n, max, t->switch_state, 1);          /* replaceSwitchTiles, the bridges of the Hero's cave and of dungeon 3 (Ages) */
    /* wWarpTransition and wWarpDestPos are left out too (exempt as well):
     * they only matter to replaceBreakableTileOverLinkTimeWarpingIn, the
     * tile over the spot Link arrives at by a time warp, and a neighbour is
     * reached by a scroll, never by a time warp; keyed on them, every warp of
     * the live game (a house's door) threw the cache of the whole area. */
    n = add_range(out, n, max, t->link_time_warp_tile, 1);   /* return after a refused time travel (Ages) */
    n = add_range(out, n, max, t->portal_group, 3);          /* replaceBreakableTileOverPortal: group, room, position (Ages) */
    n = add_range(out, n, max, t->is_linked_game, 1);        /* applySingleTileChanges, the Black Tower's entrance (Ages) */
    /* checkGlobalFlag: pollution fixed, the finished game, the room-specific
     * ones, and checkPirateShipDocked (Seasons). */
    n = add_range(out, n, max, t->global_flags, t->global_flags_size);
    /* The secret shop emptied of its articles (tileReplacement_group2Map7e in Ages, the member's shop of tileReplacement_group2_3Mapb0 in Seasons).  In Ages the
     * routine before it lacks its `ret` and falls into it, so a room of dungeon 1 reads the byte too (measured on
     * a route through dungeon 1). */
    n = add_range(out, n, max, t->bought_shop_items1, 1);
    /* Read by one or a few rooms each, found by reading, and none met by a
     * route yet.  They change a few times in a game (a treasure, an essence,
     * an event), and an entry is only thrown for the bytes it read, so no
     * other room pays for them. */
    n = add_range(out, n, max, t->twinrova_tile_replacement_mode, 1);   /* the Twinrova and Ganon fight (group5Mapf5 in Ages, group5Map9e in Seasons): set by the fight */
    n = add_range(out, n, max, t->seed_tree_refilled_bitset, 1);   /* Goron mountain's cave (group2Mapf7, bit 0, Ages), the Moblin's house (group0Map6f, bit 1, Seasons): harvest and regrowth of a tree */
    n = add_range(out, n, max, t->ricky_state, 1);           /* Ricky's gloves (group0Map98, Ages), Ricky's sign (group0Map54, Seasons) */
    n = add_range(out, n, max, t->essences_obtained, 1);     /* checkFlag of an essence: the Goron elder's caves (group5Mapb9, c3), the Maku Tree's top (group0Mape0-e2) (Ages) */
    n = add_range(out, n, max, t->obtained_treasure_flags, t->obtained_treasure_flags_size);   /* checkTreasureObtained: Ricky's gloves (group0Map98, Ages); its "related variable" is none */
    n = add_range(out, n, max, t->animal_companion, 1);      /* the bridge into Natzu (group0Map56, Seasons) */
    n = add_range(out, n, max, t->num_placed_slates, 1);     /* Kinomi's slate room (applySingleTileChanges@slateRoom, 4:79); absent from the originals */
    /* All four pages of room flags ($c700-$caff: groups 0, 1, 4, 5; the
     * interiors 2, 3 and the side-scrolling 6, 7 share them): getThisRoomFlags
     * reads the room's own byte, and the room-specific routines read other
     * rooms' bytes by name (tileReplacement_group0Map0b reads
     * wPresentRoomFlags+$0a, the cave right of dungeon 5), so the closed rule
     * is the whole region.  It holds three other facts the substitutions read:
     * getBlackTowerProgress (Ages, two room flags), the vines' positions
     * (getVinePosition, Ages: wVinePositions is wPastRoomFlags+$f0) and the
     * rupees of the hidden rooms of dungeons 2 and 6 (Seasons:
     * wSubrosiaRoomFlags+$f0 and $f8). */
    n = add_range(out, n, max, t->group0_room_flags, t->room_flags_size);
    return n;
}

unsigned oracles_ghost_key_exempt_ranges(const OraclesCompatProfile *profile, OraclesGhostKeyRange out[], unsigned max)
{
    const OraclesGuestTables *t = oracles_compat_guest_tables(profile);
    if (!t) return 0;
    unsigned n = 0;
    n = add_range(out, n, max, t->tileset_flags, 1);     /* loadTilesetData writes the loaded room's own flags first */
    n = add_range(out, n, max, t->active_collisions, 1); /* and its collision mode, from the same tileset (Ages' applyStandardTileSubstitutions; Seasons' never read it) */
    n = add_range(out, n, max, t->warp_transition, 1);   /* replaceBreakableTileOverLinkTimeWarpingIn: a time warp's arrival, never a scroll's */
    n = add_range(out, n, max, t->warp_dest_pos, 1);     /* idem */
    /* replaceShutterForLinkEntering (@temporarilyOpenDoor): the entry itself,
     * whose tiles are the entry-dependent ones excluded from the comparison
     * (oracles_ghost_entry_dependent_tile); the ghost's forced scroll has the
     * real entry's direction. */
    n = add_range(out, n, max, t->scroll_mode, 1);
    n = add_range(out, n, max, t->link_object_index, 1);
    n = add_range(out, n, max, t->screen_transition_direction, 1);
    n = add_range(out, n, max, (OraclesGuestSym){ ORACLES_OBJECTS_BANK, ORACLES_OBJECTS_BASE + ORACLES_OBJ_Y }, 4);   /* w1Link.y to w1Link.xh: where he enters (the same routine) */
    n = add_range(out, n, max, t->minimap_group, 1);     /* Seasons' tileSubstitutions.s: written by the room's own load (bank1.s), like wTilesetFlags */
    n = add_range(out, n, max, t->gameboy_type, 1);      /* the GBA shops (tileReplacement_group1Map58, group0Mapc5): the hardware, constant */
    /* The routines' own scratch and bank: the general-purpose HRAM (hFF8A to
     * hFF93: the room's flags in applyStandardTileSubstitutions, the loop
     * counters of the room-specific routines, decompressGraphics and
     * loadGfxHeader), each byte written by the routine before it reads it;
     * and hRomBank, the bank a far call saves and restores. */
    n = add_range(out, n, max, t->general_purpose_hram, (uint16_t)(t->general_purpose_hram_end.addr - t->general_purpose_hram.addr));
    n = add_range(out, n, max, t->rom_bank, 1);
    /* Written by the loaded room's own load before the substitutions run,
     * like wTilesetFlags: the layout group and the collisions of its tileset
     * (loadRoomLayout, called again by tileReplacement_group4Map52 of Ages
     * for the room above; retrieveTileCollisionValue, from
     * setTileToWitheredVine and setTile, Ages).  w3TileCollisions is in WRAM
     * bank 3 and the trace knows addresses only: its $db00-$dbff are also
     * object slot $db's in bank 1, which no substitution reads besides the
     * interaction byte below. */
    n = add_range(out, n, max, t->tileset_layout_group, 1);
    n = add_range(out, n, max, t->tile_collisions, 0x100);
    n = add_range(out, n, max, t->loading_room, 1);      /* tileReplacement_group4Map52 (Ages) writes it before its loadRoomLayout reads it */
    /* The queue of changed tiles (setTile, tileReplacement_group2Mapf7,
     * Ages): setTile writes the layout unless the queue is full, 31 changes
     * pending, which it never is at a room's load (drained four a frame);
     * the positions only say where the change is queued. */
    n = add_range(out, n, max, t->changed_tile_queue_head, 1);
    n = add_range(out, n, max, t->changed_tile_queue_tail, 1);
    /* The vblank queue a graphics copy is queued in (replaceToggleBlocks,
     * func_02_7a77, queueDmaTransfer, Ages): where, not which tiles. */
    n = add_range(out, n, max, t->vblank_function_queue_tail, 1);
    /* getFreeInteractionSlot (the minecart doors' controller of
     * @temporarilyOpenDoor, the puzzles of the Maku Tree's top, group0Mape0-e2
     * in Ages): the first byte of each dynamic interaction; which slot is
     * free decides where a controller goes, never a tile. */
    for (unsigned slot = t->first_dynamic_interaction_index; slot < (ORACLES_OBJECTS_BASE >> 8) + ORACLES_OBJECT_SLOTS; slot++)
        n = add_range(out, n, max, (OraclesGuestSym){ ORACLES_OBJECTS_BANK, (uint16_t)((slot << 8) + ORACLES_OBJECT_SIZE + ORACLES_OBJ_ENABLED) }, 1);
    return n;
}

/* The coarse list: what a ghost's run reads outside the trace's window that
 * its answer depends on, established by reading, in both games, the run of a
 * scroll's load (cutscene01, bank1.s): loadTilesetData and what it calls,
 * loadTilesetAndRoomLayout around the substitutions (loadTilesetLayout,
 * loadRoomLayout, Seasons' @adjustLoadingRoomForTempleRemains),
 * loadRoomCollisions, generateVramTilesWithRoomChanges with
 * applyRoomSpecificTileChangesAfterGfxLoad, and the scroll's frames after it
 * (the tileset's unique graphics and animation, checkDarkenRoom, the palette
 * fades).  No entry records these reads, so a change of one of these bytes
 * in the live instance throws the whole cache; they change a few times in a
 * game.  A byte may be in the key as well, when the substitutions read it
 * too; never in the exempt list.
 *
 * Not in the list, each for its reason:
 * - written by the room's own load before being read, like wTilesetFlags:
 *   wTilesetLayout, wTilesetLayoutGroup, wLoadingRoom, wLoadingRoomPack, the
 *   tileset's data (wTilesetGfx, wTilesetPalette, wTilesetUniqueGfx,
 *   wTilesetAnimation, w3TileMappingData, w3TileCollisions), wDungeonIndex,
 *   wActiveCollisions, wDungeonRoomProperties;
 * - the room's own flag byte (getAdjustedRoomGroup's layout swap, the
 *   entrances of Seasons' Gnarled Root and Snake's Remains, Ages' dungeon 2,
 *   Crown Dungeon and Tokay's tree, Din's troupe): read by
 *   applyStandardTileSubstitutions of the same entry, traced;
 * - Jabu-Jabu's water level and floor (checkTilesetOverride, Ages): read by
 *   replaceJabuTilesIfUnderwater of the same entry under the same condition;
 * - wToggleBlocksState (checkUpdateToggleBlocks): read by replaceToggleBlocks
 *   of the same entry;
 * - the season (Seasons: the tileset, the temple's lava rooms, the palette
 *   fades): compared on its own by the view;
 * - GLOBALFLAG_TUNI_NUT_PLACED (Ages) and the fades of Seasons: they choose
 *   whether the palette fades during the scroll, not the palette the room
 *   settles in;
 * - wInShop, wDiggingUpEnemiesForbidden, wLoadedTreeGfxActive: written by
 *   the load, or a copy it skips when already made;
 * - wGashaSpotKillCounters, the enemies killed at each Gasha spot (a sprout
 *   or a tree): it changes with every enemy killed, so it is not in the
 *   list, and a Gasha spot shown as a neighbour may keep the stage it had
 *   when the ghost ran it until Link enters it (an accepted limit);
 * - $cfd0, the state of Seasons' crystal trap in dungeon 6, whose walls
 *   roomTileChangesAfterLoad02 draws in rooms 4:c5 and 4:c6: a byte of the
 *   scratch wTmpcfc0 that other rooms use for their own ends, so it is not in
 *   the list, and a trap room shown as a neighbour may keep the walls it had
 *   when the ghost ran it until Link enters it (the same accepted limits);
 * - GLOBALFLAG_TUNI_NUT_PLACED (Ages), read by checkGlobalFlag outside the
 *   window too: see the palette fades above.
 * Outside this account, and still open: the objects
 * captured with an answer, whose creation (initializeRoom) and first update
 * read far more live state (the pirate ship's position, Maple's counter, a
 * companion's place) and keep their own rule, the enemies killed. */

/* The global flags the load reads, 0xff where the game's load reads none. */
static unsigned coarse_global_flags(const OraclesGuestTables *t, uint8_t out[5])
{
    const uint8_t all[5] = { t->coarse_flag_finished_game, t->coarse_flag_temple_lava, t->coarse_flag_moblins_keep,
                             t->coarse_flag_pirate_ship, t->coarse_flag_intro_done };
    unsigned n = 0;
    for (unsigned i = 0; i < 5u; i++) if (all[i] != 0xffu) out[n++] = all[i];
    return n;
}

/* The bits of a coarse byte the comparison looks at: a global flag's byte,
 * only the flags the load reads; a room's flags, all but its visited bit,
 * which the entry into that room sets and no load reads; any other, all. */
static uint8_t coarse_mask(const OraclesGuestTables *t, uint16_t a)
{
    if (a >= t->group0_room_flags.addr && a < t->group0_room_flags.addr + t->room_flags_size) return (uint8_t)~t->roomflag_visited;
    if (a < t->global_flags.addr || a >= t->global_flags.addr + t->global_flags_size) return 0xffu;
    uint8_t flags[5], mask = 0;
    const unsigned n = coarse_global_flags(t, flags);
    for (unsigned i = 0; i < n; i++) if (t->global_flags.addr + (flags[i] >> 3) == a) mask = (uint8_t)(mask | (1u << (flags[i] & 7u)));   /* checkFlag, _flagHlpr: byte a/8, bitTable[a%8] */
    return mask;
}
unsigned oracles_ghost_key_coarse_ranges(const OraclesGuestTables *t, OraclesGhostKeyRange out[], unsigned max)
{
    unsigned n = 0;
    /* checkGlobalFlag, each flag's byte, the comparison masked to the flags
     * read (coarse_mask): in Ages GLOBALFLAG_FINISHEDGAME, the mermaid statue
     * (roomTileChangesAfterLoad09); in Seasons
     * GLOBALFLAG_TEMPLE_REMAINS_FILLED_WITH_LAVA, the temple's tileset and
     * layout (getTempleRemainsSeasonsTilesetData,
     * @adjustLoadingRoomForTempleRemains), GLOBALFLAG_MOBLINS_KEEP_DESTROYED,
     * the keep's tileset and King Moblin's house
     * (getMoblinKeepSeasonsTilesetData, roomTileChangesAfterLoad05),
     * GLOBALFLAG_PIRATE_SHIP_DOCKED, the beach (roomTileChangesAfterLoad00,
     * 01), GLOBALFLAG_INTRO_DONE, the wagon west of Din's troupe
     * (roomTileChangesAfterLoad0e: that room alone, scoped by
     * oracles_ghost_coarse_changed).  Any other flag may be set as often as
     * the story goes without throwing the cache; the key keeps the whole
     * page, where an entry goes only for the bytes it read. */
    uint8_t flags[5];
    const unsigned nflags = coarse_global_flags(t, flags);
    for (unsigned i = 0; i < nflags; i++) {
        const uint16_t a = (uint16_t)(t->global_flags.addr + (flags[i] >> 3));
        int listed = 0;
        for (unsigned k = 0; k < n; k++) listed |= out[k].address == a;
        if (!listed) n = add_range(out, n, max, (OraclesGuestSym){ t->global_flags.bank, a }, 1);
    }
    /* The companion: Nuun's tileset and layout group (checkTilesetOverride)
     * and wRoomStateModifier there (calculateRoomStateModifier), Ages; the
     * Moblins' keep's tileset (getMoblinKeepSeasonsTilesetData), Seasons. */
    n = add_range(out, n, max, t->animal_companion, 1);
    n = add_range(out, n, max, t->is_linked_game, 1);    /* the staircase of the Maku Tree's present room (roomTileChangesAfterLoad06, Ages): a save file's own */
    /* The Gasha seeds planted: a sprout or a tree drawn into the layout and
     * the map (roomTileChangesAfterLoad08). */
    n = add_range(out, n, max, t->gasha_spots_planted, t->gasha_spots_planted_size);
    /* Other rooms' flags read by name, Ages: the Maku Tree saved in the past
     * switches the present room 0:38 to its other tileset (checkTilesetOverride),
     * and Veran beaten opens its staircase (roomTileChangesAfterLoad06). */
    n = add_range(out, n, max, t->maku_tree_tileset_room_flags, 1);
    n = add_range(out, n, max, t->veran_room_flags, 1);
    return n;
}

/* The coarse list's bytes in the order of its ranges; 0 when `capacity` is too small. */
size_t oracles_ghost_coarse_snapshot(OraclesGuest *guest, uint8_t *out, size_t capacity)
{
    const OraclesGuestTables *t = oracles_guest_tables(guest);
    OraclesGhostKeyRange ranges[ORACLES_GHOST_KEY_RANGES];
    const unsigned count = oracles_ghost_key_coarse_ranges(t, ranges, ORACLES_GHOST_KEY_RANGES);
    size_t n = 0;
    for (unsigned k = 0; k < count; k++) {
        const OraclesGuestSym sym = { 0, ranges[k].address };
        const uint8_t *p = oracles_guest_ptr(guest, sym, ranges[k].length);
        if (!p || n + ranges[k].length > capacity) return 0;
        memcpy(out + n, p, ranges[k].length);
        n += ranges[k].length;
    }
    return n;
}

int oracles_ghost_coarse_changed(const OraclesGuestTables *t, const uint8_t *before, const uint8_t *after, size_t len, uint16_t *address, int *room)
{
    OraclesGhostKeyRange ranges[ORACLES_GHOST_KEY_RANGES];
    const unsigned count = oracles_ghost_key_coarse_ranges(t, ranges, ORACLES_GHOST_KEY_RANGES);
    /* GLOBALFLAG_INTRO_DONE changes one room's terrain (coarse_flag_intro_done_room): alone, it is scoped to that room. */
    const int scoped = t->coarse_flag_intro_done != 0xffu && t->coarse_flag_intro_done_room != 0xffu;
    const uint16_t intro_byte = (uint16_t)(t->global_flags.addr + (t->coarse_flag_intro_done >> 3));
    const uint8_t intro_bit = (uint8_t)(1u << (t->coarse_flag_intro_done & 7u));
    int changed = 0, whole = 0;
    size_t i = 0;
    for (unsigned k = 0; k < count; k++)
        for (unsigned b = 0; b < ranges[k].length && i < len; b++, i++) {
            const uint16_t a = (uint16_t)(ranges[k].address + b);
            const uint8_t differs = (uint8_t)((before[i] ^ after[i]) & coarse_mask(t, a));
            if (!differs) continue;
            if (!changed && address) *address = a;
            changed = 1;
            if (!whole && !(scoped && a == intro_byte && differs == intro_bit)) { if (address) *address = a; whole = 1; }
        }
    if (room) *room = changed && !whole ? t->coarse_flag_intro_done_room : -1;
    return changed;
}

int oracles_ghost_entry_dependent_tile(const OraclesGuestTables *t, uint8_t tile)
{
    /* replaceShutterForLinkEntering (commonTileSubstitutions.s): the eight tiles from $78 in the originals; a fan game's range
     * is its own (Kinomi's takes its red shutters too). */
    return tile >= t->shutter_tile_first && tile <= t->shutter_tile_last;
}

/* ---- read trace ---------------------------------------------------------------- */

void oracles_ghost_set_trace(OraclesGhost *g, int enabled)
{
    g->tracing = enabled != 0;
    if (enabled && !g->read_counts) g->read_counts = calloc(65536u, sizeof *g->read_counts);
    oracles_guest_set_read_trace(g->guest, enabled ? oracles_ghost_on_read : NULL, g);
    oracles_guest_enable_read_trace(g->guest, 0);
}
const uint32_t *oracles_ghost_read_counts(const OraclesGhost *g) { return g->read_counts; }

void oracles_ghost_set_trace_load(OraclesGhost *g, int enabled)
{
    if (enabled && !g->load_counts) g->load_counts = calloc(65536u, sizeof *g->load_counts);
    if (!enabled) { free(g->load_counts); g->load_counts = NULL; }
}
const uint32_t *oracles_ghost_load_read_counts(const OraclesGhost *g) { return g->load_counts; }

/* ---- the routing key -------------------------------------------------------------- */

/* What a routine of mapTransitionGroupTable reads to choose the room, read in
 * the routine and its callees; written for Seasons' Lost Woods alone.
 * screenTransitionLostWoods (case 0 of Seasons' table) compares the
 * direction and the season (wRoomStateModifier) with the step of two
 * sequences it counts in wLostWoodsTransitionCounter1 (north) and
 * wLostWoodsTransitionCounter2 (the sword upgrade), and loads 0:40, 0:41
 * (the standard transition right), 0:30 or 0:c9 from them.  The season
 * decides the room, not only its terrain: at the third step of the sequence
 * north, right in spring loads 0:40 and right in another season 0:41. */
unsigned oracles_ghost_routing_key_ranges(const OraclesCompatProfile *profile, int routine, OraclesGhostKeyRange out[], unsigned max)
{
    unsigned n = 0;
    const OraclesGuestTables *t = oracles_compat_guest_tables(profile);
    if (!t || !oracles_compat_lost_woods_rules(profile) || routine != 0) return 0;
    n = add_range(out, n, max, t->lost_woods_transition_counter1, 1);
    n = add_range(out, n, max, t->lost_woods_transition_counter2, 1);
    n = add_range(out, n, max, t->room_state_modifier, 1);
    return n;
}

size_t oracles_ghost_routing_key_snapshot(OraclesGuest *guest, int routine, uint8_t *out, size_t capacity)
{
    OraclesGhostKeyRange ranges[ORACLES_GHOST_ROUTING_KEY_BYTES];
    const unsigned count = oracles_ghost_routing_key_ranges(oracles_guest_profile(guest), routine, ranges, ORACLES_GHOST_ROUTING_KEY_BYTES);
    size_t n = 0;
    for (unsigned k = 0; k < count; k++) {
        const OraclesGuestSym sym = { 0, ranges[k].address };
        const uint8_t *p = oracles_guest_ptr(guest, sym, ranges[k].length);
        if (!p || n + ranges[k].length > capacity) return 0;
        memcpy(out + n, p, ranges[k].length);
        n += ranges[k].length;
    }
    return n;
}
