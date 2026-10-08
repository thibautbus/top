/* The ghost instance without a game ROM: the closed account of the cache key
 * and its coarse list hold in both games, the coarse list's comparison ignores
 * a room's visited bit and the global flags the load does not read, a savestate of one core loads into the ghost, a
 * synthetic state is refused as not primeable with a reason, a run to the
 * other side of the sea primes only once the warp it sets has taken it there, and the read
 * trace records the reads of the program but not those of its interrupt
 * handler. */
#include "core.h"
#include "ghost.h"
#include "guest.h"
#include "guest_struct_offsets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int failures;

#define GHOST_WAIT_SECONDS 10   /* the worker answers a state it cannot prime at once; far more than it needs anywhere */
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

static const OraclesCompatProfile *fixture_profile(void)
{
    const OraclesRomInfo info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_AGES_US, .known = 1 };
    return oracles_compat_find(&info);
}

static const OraclesCompatProfile *original_profile(OraclesGame game)
{
    const OraclesRomInfo info = { .game = game, .revision = game == ORACLES_GAME_SEASONS ? ORACLES_ROM_REVISION_SEASONS_US : ORACLES_ROM_REVISION_AGES_US, .known = 1 };
    return oracles_compat_find(&info);
}

static uint32_t reads[65536];

/* Whether every byte of [address, address + length) lies in one of the ranges. */
static int covered(const OraclesGhostKeyRange *ranges, unsigned count, uint16_t address, unsigned length)
{
    for (unsigned b = 0; b < length; b++) {
        int in = 0;
        for (unsigned k = 0; k < count && !in; k++) in = address + b >= ranges[k].address && address + b < ranges[k].address + ranges[k].length;
        if (!in) return 0;
    }
    return 1;
}

typedef struct expected { const char *name; OraclesGuestSym sym; unsigned length; } expected;

/* The account of what the substitutions read (ghost_key.c), game by game: each
 * variable the game has is in the key, or in the exempt list, whole; a removal
 * fails here before a route has to cross the room that reads it. */
static void check_key_account(const OraclesCompatProfile *profile, const char *game)
{
    const OraclesGuestTables *t = oracles_compat_guest_tables(profile);
    OraclesGhostKeyRange key[ORACLES_GHOST_KEY_RANGES], exempt[ORACLES_GHOST_KEY_RANGES];
    const unsigned nk = oracles_ghost_key_ranges(profile, key, ORACLES_GHOST_KEY_RANGES);
    const unsigned ne = oracles_ghost_key_exempt_ranges(profile, exempt, ORACLES_GHOST_KEY_RANGES);
    /* Short of the capacity: a list that reached it may have lost its last ranges. */
    CHECK(nk < ORACLES_GHOST_KEY_RANGES && ne < ORACLES_GHOST_KEY_RANGES);
    const expected in_key[] = {
        { "wActiveGroup", t->active_group, 1 }, { "wActiveRoom", t->active_room, 1 },
        { "wDungeonIndex", t->dungeon_index, 1 }, { "wDungeonFloor", t->dungeon_floor, 1 }, { "wJabuWaterLevel", t->jabu_water_level, 1 },
        { "wToggleBlocksState", t->toggle_blocks_state, 1 },
        { "wSwitchState", t->switch_state, 1 }, { "wLinkTimeWarpTile", t->link_time_warp_tile, 1 }, { "wPortalGroup", t->portal_group, 3 },
        { "wIsLinkedGame", t->is_linked_game, 1 }, { "wGlobalFlags", t->global_flags, t->global_flags_size },
        { "wBoughtShopItems1", t->bought_shop_items1, 1 }, { "wTwinrovaTileReplacementMode", t->twinrova_tile_replacement_mode, 1 },
        { "wSeedTreeRefilledBitset", t->seed_tree_refilled_bitset, 1 }, { "wRickyState", t->ricky_state, 1 },
        { "wEssencesObtained", t->essences_obtained, 1 }, { "wObtainedTreasureFlags", t->obtained_treasure_flags, t->obtained_treasure_flags_size },
        { "wAnimalCompanion", t->animal_companion, 1 }, { "room flags", t->group0_room_flags, t->room_flags_size },
    };
    const expected in_exempt[] = {
        { "wTilesetFlags", t->tileset_flags, 1 }, { "wActiveCollisions", t->active_collisions, 1 }, { "wWarpTransition", t->warp_transition, 1 }, { "wWarpDestPos", t->warp_dest_pos, 1 },
        { "wScrollMode", t->scroll_mode, 1 }, { "wLinkObjectIndex", t->link_object_index, 1 },
        { "wScreenTransitionDirection", t->screen_transition_direction, 1 },
        { "w1Link.y to xh", { ORACLES_OBJECTS_BANK, ORACLES_OBJECTS_BASE + ORACLES_OBJ_Y }, 4 },
        { "wMinimapGroup", t->minimap_group, 1 }, { "hGameboyType", t->gameboy_type, 1 },
        { "hFF8A to hFF93", t->general_purpose_hram, (unsigned)(t->general_purpose_hram_end.addr - t->general_purpose_hram.addr) },
        { "hRomBank", t->rom_bank, 1 }, { "wTilesetLayoutGroup", t->tileset_layout_group, 1 }, { "w3TileCollisions", t->tile_collisions, 0x100 },
        { "wLoadingRoom", t->loading_room, 1 }, { "wChangedTileQueueHead", t->changed_tile_queue_head, 1 },
        { "wChangedTileQueueTail", t->changed_tile_queue_tail, 1 }, { "hVBlankFunctionQueueTail", t->vblank_function_queue_tail, 1 },
    };
    /* The season is read by Seasons' substitutions; Ages sets the byte after its own (calculateRoomStateModifier). */
    if (t == &oracles_guest_tables_seasons) CHECK(covered(key, nk, t->room_state_modifier.addr, 1));
    else CHECK(!covered(key, nk, t->room_state_modifier.addr, 1) && !covered(exempt, ne, t->room_state_modifier.addr, 1));
    for (unsigned i = 0; i < sizeof in_key / sizeof in_key[0]; i++)
        if (in_key[i].sym.bank != ORACLES_GUEST_ABSENT && !covered(key, nk, in_key[i].sym.addr, in_key[i].length)) { failures++; fprintf(stderr, "FAIL %s: %s not in the key\n", game, in_key[i].name); }
    for (unsigned i = 0; i < sizeof in_exempt / sizeof in_exempt[0]; i++)
        if (in_exempt[i].sym.bank != ORACLES_GUEST_ABSENT && !covered(exempt, ne, in_exempt[i].sym.addr, in_exempt[i].length)) { failures++; fprintf(stderr, "FAIL %s: %s not exempt\n", game, in_exempt[i].name); }
    /* getFreeInteractionSlot: the first byte of each dynamic interaction. */
    CHECK(t->first_dynamic_interaction_index == (ORACLES_OBJECTS_BASE >> 8) + 2u);
    for (unsigned slot = t->first_dynamic_interaction_index; slot < (ORACLES_OBJECTS_BASE >> 8) + ORACLES_OBJECT_SLOTS; slot++)
        CHECK(covered(exempt, ne, (uint16_t)((slot << 8) + ORACLES_OBJECT_SIZE), 1));
    /* A byte is keyed or exempt, not both; and the key's bytes fit its snapshot. */
    size_t bytes = 0;
    for (unsigned k = 0; k < nk; k++) {
        CHECK(!covered(exempt, ne, key[k].address, 1) && !covered(exempt, ne, (uint16_t)(key[k].address + key[k].length - 1u), 1));
        if (key[k].address != t->active_group.addr && key[k].address != t->active_room.addr) bytes += key[k].length;
    }
    CHECK(bytes <= ORACLES_GHOST_KEY_BYTES);
    /* The coarse list (the load around the substitutions): each variable the game has, whole; none exempt; it fits its snapshot. */
    OraclesGhostKeyRange coarse[ORACLES_GHOST_KEY_RANGES];
    const unsigned nc = oracles_ghost_key_coarse_ranges(t, coarse, ORACLES_GHOST_KEY_RANGES);
    CHECK(nc < ORACLES_GHOST_KEY_RANGES);
    const expected in_coarse[] = {
        { "wAnimalCompanion", t->animal_companion, 1 },
        { "wIsLinkedGame", t->is_linked_game, 1 }, { "wGashaSpotsPlantedBitset", t->gasha_spots_planted, t->gasha_spots_planted_size },
        { "the Maku Tree's tileset room flags", t->maku_tree_tileset_room_flags, 1 }, { "Veran's room flags", t->veran_room_flags, 1 },
    };
    for (unsigned i = 0; i < sizeof in_coarse / sizeof in_coarse[0]; i++)
        if (in_coarse[i].sym.bank != ORACLES_GUEST_ABSENT && !covered(coarse, nc, in_coarse[i].sym.addr, in_coarse[i].length)) { failures++; fprintf(stderr, "FAIL %s: %s not in the coarse list\n", game, in_coarse[i].name); }
    /* The global flags the load reads, game by game (the others 0xff), each flag's byte in the list. */
    const int ages = t == &oracles_guest_tables_ages;
    CHECK((t->coarse_flag_finished_game != 0xffu) == ages);
    CHECK((t->coarse_flag_temple_lava != 0xffu) == !ages && (t->coarse_flag_moblins_keep != 0xffu) == !ages);
    CHECK((t->coarse_flag_pirate_ship != 0xffu) == !ages && (t->coarse_flag_intro_done != 0xffu) == !ages);
    /* The one room the end of Seasons' intro changes: the wagon west of Din's troupe (roomTileChangesAfterLoad0e). */
    CHECK(t->coarse_flag_intro_done_room == (ages ? 0xffu : 0x97u));
    const uint8_t read_flags[5] = { t->coarse_flag_finished_game, t->coarse_flag_temple_lava, t->coarse_flag_moblins_keep, t->coarse_flag_pirate_ship, t->coarse_flag_intro_done };
    for (unsigned i = 0; i < 5u; i++)
        if (read_flags[i] != 0xffu) CHECK(covered(coarse, nc, (uint16_t)(t->global_flags.addr + (read_flags[i] >> 3)), 1));
    bytes = 0;
    for (unsigned k = 0; k < nc; k++) {
        for (unsigned b = 0; b < coarse[k].length; b++) CHECK(!covered(exempt, ne, (uint16_t)(coarse[k].address + b), 1));
        bytes += coarse[k].length;
    }
    CHECK(bytes <= ORACLES_GHOST_COARSE_BYTES);
}

/* The comparison of the coarse list on a guest's WRAM: a byte of the list
 * changed throws the cache, a room's visited bit in it does not, nor does a
 * byte outside it. */
static void check_coarse_comparison(OraclesCore *core)
{
    OraclesGuest *guest = oracles_guest_attach(core, original_profile(ORACLES_GAME_AGES));
    CHECK(guest != NULL);
    if (!guest) return;
    const OraclesGuestTables *t = oracles_guest_tables(guest);
    uint8_t *wram = oracles_guest_wram_writable(guest, 0);
    CHECK(wram != NULL);
    if (!wram) { oracles_guest_detach(guest); return; }
    uint8_t before[ORACLES_GHOST_COARSE_BYTES], after[ORACLES_GHOST_COARSE_BYTES];
    const size_t len = oracles_ghost_coarse_snapshot(guest, before, sizeof before);
    CHECK(len > 0);
    uint16_t at = 0;
    int room = 0;
    wram[t->ricky_state.addr - 0xc000u] ^= 0x01u;                          /* in the key, not in the coarse list */
    wram[t->maku_tree_tileset_room_flags.addr - 0xc000u] ^= t->roomflag_visited;   /* the room entered */
    CHECK(oracles_ghost_coarse_snapshot(guest, after, sizeof after) == len);
    CHECK(!oracles_ghost_coarse_changed(t, before, after, len, &at, &room));
    wram[t->maku_tree_tileset_room_flags.addr - 0xc000u] ^= 0x01u;          /* the Maku Tree saved */
    CHECK(oracles_ghost_coarse_snapshot(guest, after, sizeof after) == len);
    CHECK(oracles_ghost_coarse_changed(t, before, after, len, &at, &room) && at == t->maku_tree_tileset_room_flags.addr && room == -1);
    memcpy(before, after, len);
    wram[t->animal_companion.addr - 0xc000u] ^= 0x01u;
    CHECK(oracles_ghost_coarse_snapshot(guest, after, sizeof after) == len);
    CHECK(oracles_ghost_coarse_changed(t, before, after, len, &at, &room) && at == t->animal_companion.addr);
    oracles_guest_detach(guest);
}

/* A global flag the load reads is seen when it changes; another flag of the
 * same byte is not, in either game (Ages' finished game; Seasons' temple,
 * keep and pirate ship, which share a byte).  Seasons' intro flag alone is
 * scoped to its room; with another flag of the list it throws the cache. */
static void check_coarse_global_flags(OraclesCore *core, OraclesGame game, uint8_t flag)
{
    OraclesGuest *guest = oracles_guest_attach(core, original_profile(game));
    CHECK(guest != NULL);
    if (!guest) return;
    const OraclesGuestTables *t = oracles_guest_tables(guest);
    uint8_t *wram = oracles_guest_wram_writable(guest, 0);
    CHECK(wram != NULL && flag != 0xffu);
    if (!wram || flag == 0xffu) { oracles_guest_detach(guest); return; }
    uint8_t *byte = &wram[t->global_flags.addr + (flag >> 3) - 0xc000u];
    const uint8_t read_bits = (uint8_t)(1u << (flag & 7u));
    uint8_t unread = 0;   /* a flag of the same byte the load does not read */
    const uint8_t flags[5] = { t->coarse_flag_finished_game, t->coarse_flag_temple_lava, t->coarse_flag_moblins_keep, t->coarse_flag_pirate_ship, t->coarse_flag_intro_done };
    for (unsigned bit = 0; bit < 8u && !unread; bit++) {
        int read = 0;
        for (unsigned i = 0; i < 5u; i++) read |= flags[i] != 0xffu && (flags[i] >> 3) == (flag >> 3) && (flags[i] & 7u) == bit;
        if (!read) unread = (uint8_t)(1u << bit);
    }
    CHECK(unread != 0);
    uint8_t before[ORACLES_GHOST_COARSE_BYTES], after[ORACLES_GHOST_COARSE_BYTES];
    const size_t len = oracles_ghost_coarse_snapshot(guest, before, sizeof before);
    uint16_t at = 0;
    int room = 0;
    const int scoped = flag == t->coarse_flag_intro_done && t->coarse_flag_intro_done_room != 0xffu;
    *byte ^= unread;
    CHECK(oracles_ghost_coarse_snapshot(guest, after, sizeof after) == len && len > 0);
    CHECK(!oracles_ghost_coarse_changed(t, before, after, len, &at, &room));
    *byte ^= read_bits;
    CHECK(oracles_ghost_coarse_snapshot(guest, after, sizeof after) == len);
    CHECK(oracles_ghost_coarse_changed(t, before, after, len, &at, &room) && at == t->global_flags.addr + (flag >> 3));
    CHECK(room == (scoped ? (int)t->coarse_flag_intro_done_room : -1));
    if (scoped) {
        wram[t->animal_companion.addr - 0xc000u] ^= 0x01u;
        CHECK(oracles_ghost_coarse_snapshot(guest, after, sizeof after) == len);
        CHECK(oracles_ghost_coarse_changed(t, before, after, len, &at, &room) && at == t->animal_companion.addr && room == -1);
        wram[t->animal_companion.addr - 0xc000u] ^= 0x01u;
    }
    *byte ^= (uint8_t)(unread | read_bits);
    oracles_guest_detach(guest);
}

static void on_read(void *opaque, uint16_t address, uint16_t sp)
{
    (void)opaque;
    (void)sp;
    reads[address]++;
}

int main(void)
{
    const size_t size = 1024u * 1024u;
    uint8_t *rom = calloc(size, 1);
    memcpy(rom + 0x134, "ZELDA NAYRU", 11);
    rom[0x143] = 0xc0; rom[0x147] = 0x1b; rom[0x148] = 0x05; rom[0x149] = 0x02;
    /* $0040 (vblank vector): ld a,($c456) ; reti.
     * $0100: nop ; jp $0150.  $0150: ld a,1 ; ldh ($ff),a ; ei ; loop: ld a,($c123) ; then the warp of
     * applyWarpTransition2 reduced to its effect, with Ages' addresses: when wWarpTransition2 is set, the
     * room and group of the warp become the active ones and it is cleared ; jr loop. */
    rom[0x40] = 0xfa; rom[0x41] = 0x56; rom[0x42] = 0xc4; rom[0x43] = 0xd9;
    rom[0x100] = 0x00; rom[0x101] = 0xc3; rom[0x102] = 0x50; rom[0x103] = 0x01;
    {
        const OraclesGuestTables *t = &oracles_guest_tables_ages;
        #define LO(sym) (uint8_t)((sym).addr & 0xffu)
        #define HI(sym) (uint8_t)((sym).addr >> 8)
        const uint8_t program[] = { 0x3e, 0x01, 0xe0, 0xff, 0xfb,
            0xfa, 0x23, 0xc1,                                            /* loop: ld a,($c123) */
            0xfa, LO(t->warp_transition2), HI(t->warp_transition2),      /* ld a,(wWarpTransition2) */
            0xb7, 0x28, 0xf7,                                            /* or a ; jr z,loop */
            0xfa, LO(t->warp_dest_room), HI(t->warp_dest_room), 0xea, LO(t->active_room), HI(t->active_room),
            0xfa, LO(t->warp_dest_group), HI(t->warp_dest_group), 0xe6, 0x07, 0xea, LO(t->active_group), HI(t->active_group),
            0xaf, 0xea, LO(t->warp_transition2), HI(t->warp_transition2),
            0x18, 0xe1 };                                                /* jr loop */
        #undef LO
        #undef HI
        memcpy(rom + 0x150, program, sizeof program);
    }

    const OraclesCoreOptions options = { 0, 0, ORACLES_CORE_SAMEBOY, 0 };
    OraclesCore *core = oracles_core_create(rom, size, &options);
    CHECK(core != NULL);
    if (!core) return 1;
    /* Past the free boot ROM's logo animation, so the program at $0150 runs. */
    for (unsigned frame = 0; frame < 600; frame++) oracles_core_run_frame(core);

    OraclesGhost *ghost = oracles_ghost_create(rom, size, fixture_profile(), oracles_core_kind(core));
    CHECK(ghost != NULL);
    if (!ghost) return 1;
    CHECK(oracles_ghost_state_size(ghost) == oracles_core_state_size(core));

    /* The cache key's account in both games; the shutters are the entry-dependent tiles. */
    check_key_account(original_profile(ORACLES_GAME_AGES), "ages");
    check_key_account(original_profile(ORACLES_GAME_SEASONS), "seasons");
    {
        OraclesGhostKeyRange key[ORACLES_GHOST_KEY_RANGES];
        const unsigned n = oracles_ghost_key_ranges(original_profile(ORACLES_GAME_AGES), key, ORACLES_GHOST_KEY_RANGES);
        CHECK(oracles_ghost_key_ranges(original_profile(ORACLES_GAME_SEASONS), key, ORACLES_GHOST_KEY_RANGES) < n);   /* no portal, no time warp tile, no Jabu-Jabu */
        const OraclesGuestTables *ages_t = &oracles_guest_tables_ages, *seasons_t = &oracles_guest_tables_seasons;
        CHECK(oracles_ghost_entry_dependent_tile(ages_t, 0x78) && oracles_ghost_entry_dependent_tile(seasons_t, 0x7f));
        CHECK(!oracles_ghost_entry_dependent_tile(ages_t, 0x77) && !oracles_ghost_entry_dependent_tile(seasons_t, 0x80));
    }

    /* The routing key: Seasons' Lost Woods (case 0 of its table) reads its two
     * sequence counters and the season; no other routine has an account. */
    {
        OraclesGhostKeyRange r[ORACLES_GHOST_ROUTING_KEY_BYTES];
        const OraclesGuestTables *s = &oracles_guest_tables_seasons;
        CHECK(oracles_ghost_routing_key_ranges(original_profile(ORACLES_GAME_SEASONS), 0, r, ORACLES_GHOST_ROUTING_KEY_BYTES) == 3);
        CHECK(covered(r, 3, s->lost_woods_transition_counter1.addr, 1) && covered(r, 3, s->lost_woods_transition_counter2.addr, 1));
        CHECK(covered(r, 3, s->room_state_modifier.addr, 1));
        CHECK(oracles_ghost_routing_key_ranges(original_profile(ORACLES_GAME_SEASONS), 1, r, ORACLES_GHOST_ROUTING_KEY_BYTES) == 0);
        CHECK(oracles_ghost_routing_key_ranges(original_profile(ORACLES_GAME_AGES), 0, r, ORACLES_GHOST_ROUTING_KEY_BYTES) == 0);
    }

    check_coarse_comparison(core);
    check_coarse_global_flags(core, ORACLES_GAME_AGES, oracles_guest_tables_ages.coarse_flag_finished_game);
    check_coarse_global_flags(core, ORACLES_GAME_SEASONS, oracles_guest_tables_seasons.coarse_flag_pirate_ship);
    check_coarse_global_flags(core, ORACLES_GAME_SEASONS, oracles_guest_tables_seasons.coarse_flag_intro_done);

    /* A state of the same program loads; it is not primeable, with a reason. */
    const size_t state_size = oracles_core_state_size(core);
    uint8_t *state = malloc(state_size);
    CHECK(oracles_core_save_state(core, state, state_size) == 0);
    OraclesGhostResult result;
    CHECK(oracles_ghost_run(ghost, state, state_size, ORACLES_DIR_UP, 4, &result) == -1);
    CHECK(result.status == ORACLES_GHOST_NOT_PRIMEABLE);
    uint8_t *sea = NULL;   /* a state in normal play, for the thread's budget below */
    /* A run to the other side of the sea (Ages): from a state in normal play,
     * the ghost sets the warp the game sets for a dive, the room's own index in
     * the group asked for, and primes only once the warp has taken it there. */
    {
        OraclesGuest *live = oracles_guest_attach(core, original_profile(ORACLES_GAME_AGES));
        uint8_t *wram = live ? oracles_guest_wram_writable(live, 0) : NULL;
        CHECK(wram != NULL);
        if (wram) {
            const OraclesGuestTables *t = &oracles_guest_tables_ages;
            #define W0(sym) wram[(sym).addr - 0xc000u]
            W0(t->game_state) = 2; W0(t->cutscene_index) = 1; W0(t->scroll_mode) = 1; W0(t->screen_transition_state) = 2;
            W0(t->active_group) = 0; W0(t->active_room) = 0xa7; W0(t->active_tile_pos) = 0x77;
            #undef W0
            CHECK(oracles_ghost_primeable(live, NULL));
            sea = malloc(state_size);
            CHECK(oracles_core_save_state(core, sea, state_size) == 0);
            oracles_ghost_set_prerun(ghost, 60);
            /* Without it, the run primes where it starts (and times out: nothing loads a room here). */
            CHECK(oracles_ghost_begin(ghost, sea, state_size, ORACLES_DIR_LEFT, &result) == 0 && oracles_ghost_step(ghost, 100, 4, &result) == -1);
            CHECK(result.status == ORACLES_GHOST_TIMEOUT && result.from_group == 0 && result.from_room == 0xa7 && result.prerun_frames == 0);
            /* With it, in the group below, the same room, after the warp. */
            oracles_ghost_set_level_change(ghost, 2);
            CHECK(oracles_ghost_begin(ghost, sea, state_size, ORACLES_DIR_LEFT, &result) == 0 && oracles_ghost_step(ghost, 100, 4, &result) == -1);
            CHECK(result.status == ORACLES_GHOST_TIMEOUT && result.from_group == 2 && result.from_room == 0xa7 && result.prerun_frames > 0);
            /* A state that is not in normal play is refused. */
            CHECK(oracles_ghost_run(ghost, state, state_size, ORACLES_DIR_LEFT, 4, &result) == -1 && result.status == ORACLES_GHOST_NOT_PRIMEABLE);
            oracles_ghost_set_level_change(ghost, -1);
            oracles_ghost_set_prerun(ghost, 0);
        }
        if (live) oracles_guest_detach(live);
    }

    /* The threaded path answers the same. */
    CHECK(oracles_ghost_request(ghost, state, state_size, ORACLES_DIR_DOWN, 4) == 0);
    /* Waited for on the clock, not in turns of the loop: how many turns the
     * worker needs depends on the machine and its scheduler (a hundred
     * thousand were too few on Windows). */
    int polled;
    struct timespec start, now;
    timespec_get(&start, TIME_UTC);
    do {
        polled = oracles_ghost_poll(ghost, &result);
        timespec_get(&now, TIME_UTC);
    } while (polled == 0 && now.tv_sec - start.tv_sec < GHOST_WAIT_SECONDS);
    CHECK(polled == 1);
    CHECK(result.status == ORACLES_GHOST_NOT_PRIMEABLE);
    CHECK(oracles_ghost_poll(ghost, &result) == -1);
    /* A job whose budget is spent before its run finishes (here a room that never
     * loads) answers a timeout, not a result whose fields are still the start's. */
    if (sea) {
        CHECK(oracles_ghost_request(ghost, sea, state_size, ORACLES_DIR_LEFT, 4) == 0);
        timespec_get(&start, TIME_UTC);
        do {
            polled = oracles_ghost_poll(ghost, &result);
            timespec_get(&now, TIME_UTC);
        } while (polled == 0 && now.tv_sec - start.tv_sec < GHOST_WAIT_SECONDS);
        CHECK(polled == 1 && result.status == ORACLES_GHOST_TIMEOUT);
    }
    free(sea);

    /* The read trace on a guest: the program's reads are seen, the handler's are not. */
    OraclesGuest *guest = oracles_guest_attach(core, fixture_profile());
    CHECK(guest != NULL);
    oracles_guest_clear_hooks(guest);
    oracles_guest_set_read_trace(guest, on_read, NULL);
    oracles_guest_enable_read_trace(guest, 1);
    for (unsigned frame = 0; frame < 30; frame++) oracles_core_run_frame(core);
    CHECK(reads[0xc123] > 1000);       /* the loop reads this every iteration */
    CHECK(reads[0xc456] == 0);         /* the handler's read, inside the interrupt, excluded */
    CHECK(reads[0x0155] > 1000);       /* the loop's own instruction fetches are reads too */
    /* The handler ran (its vblank fires each frame), but its vector fetch and
     * its reads are inside the interrupt and never traced. */
    CHECK(reads[0x0040] == 0);
    oracles_guest_enable_read_trace(guest, 0);
    const uint32_t before = reads[0xc123];
    oracles_core_run_frame(core);
    CHECK(reads[0xc123] == before);
    oracles_guest_set_read_trace(guest, NULL, NULL);
    oracles_guest_detach(guest);

    free(state);
    oracles_ghost_destroy(ghost);
    oracles_core_destroy(core);
    free(rom);
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_ghost: ok\n");
    return 0;
}
