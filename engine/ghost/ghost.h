/* Ghost instance: a second core, loaded from an
 * in-memory savestate of the live instance, in which the game itself loads a
 * neighbouring room by its own scrolling transition; the host reads the
 * committed room layout and throws the copy away.
 *
 * The ghost is primed with the forced-transition bit of
 * wScreenTransitionDirection (screenTransitionState2, bank1.s): the game
 * decides the transition on its next frame and loads the room on the one
 * after (cutscene01, the in-game index: getNextActiveRoom, loadTilesetAndRoomLayout,
 * initializeRoom).  Link is left where he stands, so the tiles that depend on
 * the entry (shutters under Link, replaceShutterForLinkEntering) keep their
 * state from before the entry.
 *
 * The ghost plays no sound, touches no save, and is the only instance the
 * host ever writes to.  It runs synchronously with a frame budget, or in a
 * worker thread with a request/poll pair; never from a hook. */
#ifndef ORACLES_GHOST_H
#define ORACLES_GHOST_H

#include "guest.h"
#include "animation.h"
#include "objects.h"
#include "sprites.h"
#include "rom.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ORACLES_GHOST_LAYOUT_BYTES 176u   /* wRoomLayout: 11 rows with a stride of 16 */
#define ORACLES_GHOST_TILE_BYTES 0x1800u  /* one VRAM bank's tile data, 384 tiles of 16 bytes */

typedef enum OraclesGhostDirection { ORACLES_DIR_UP = 0, ORACLES_DIR_RIGHT = 1, ORACLES_DIR_DOWN = 2, ORACLES_DIR_LEFT = 3 } OraclesGhostDirection;

typedef enum OraclesGhostStatus {
    ORACLES_GHOST_OK = 0,
    ORACLES_GHOST_LOAD_FAILED,      /* the savestate was refused by the core */
    ORACLES_GHOST_NOT_PRIMEABLE,    /* the state is not in normal play (see oracles_ghost_primeable) */
    ORACLES_GHOST_TIMEOUT,          /* the room was not initialized within the frame budget */
    ORACLES_GHOST_INTERRUPTED       /* a text box opened during the run (an interaction Link was made to cross): its map holds the box */
} OraclesGhostStatus;

#define ORACLES_GHOST_RUN_READS 256u
#define ORACLES_GHOST_AREA_WIDTH 160u
#define ORACLES_GHOST_AREA_HEIGHT 128u
#define ORACLES_GHOST_MAP_BYTES 0x1000u   /* $9800-$9fff of bank 0 (tiles) then of bank 1 (attributes) */
#define ORACLES_GHOST_KEY_BYTES 2048u     /* the cache key's bytes, the active group and room left out */

typedef struct OraclesGhostResult {
    OraclesGhostStatus status;
    uint8_t group, room;                          /* the room the ghost loaded */
    uint8_t room_is_large;
    uint8_t layout[ORACLES_GHOST_LAYOUT_BYTES];   /* wRoomLayout when initializeRoom returned */
    unsigned guest_frames;                        /* frames run from priming to the initialized room */
    uint32_t traced_reads;                        /* reads recorded by applyAllTileSubstitutions, if tracing */
    /* When tracing: the distinct WRAM and HRAM addresses applyAllTileSubstitutions
     * read in this run, the layout it rewrites and the thread stacks excluded. */
    uint16_t reads[ORACLES_GHOST_RUN_READS];
    unsigned read_count;
    unsigned reads_dropped;                       /* distinct addresses beyond the capacity */
    uint32_t reads_other_thread;                  /* reads by another thread while the substitutions waited (Seasons' Subrosia loads object graphics over several frames): not the substitutions' */
    /* When the run settles (the scrolling transition into the room is over):
     * the game area of the room as the game itself displays it, rendered by
     * the software PPU from the ghost's committed display state without
     * objects, through the colour table given at the request (copied: the
     * caller may change its table afterwards). */
    int settled;
    uint32_t game_area[ORACLES_GHOST_AREA_WIDTH * ORACLES_GHOST_AREA_HEIGHT];
    /* With the render: the room's background map and attributes (VRAM $9800-$9fff of
     * both banks), its game-area registers (wGfxRegs3) and its tileset identity, so
     * that the host can render the map again with the live tiles when the live
     * room has loaded the same tileset (animated tiles in step). */
    uint8_t bg_map[ORACLES_GHOST_MAP_BYTES];
    uint8_t regs3[6];
    uint8_t tileset_gfx, tileset_palette, tileset_unique_gfx;
    uint8_t room_state_modifier;                  /* wRoomStateModifier once settled: in Seasons, the season the room was loaded in */
    uint8_t room_pack;                            /* wRoomPack once settled: the area of an overworld room */
    OraclesAnimationState animation;              /* its tile animation once settled, for the view to advance */
    uint8_t collisions[ORACLES_GHOST_LAYOUT_BYTES];   /* wRoomCollisions of the room, same stride as the layout (0: free) */
    uint8_t tiles[2u * ORACLES_GHOST_TILE_BYTES];     /* the tile data of both VRAM banks ($8000-$97ff) when the run settled: the room's own tileset, unique graphics and animation frame */
    uint8_t bg_palettes[64];                          /* the room's own background palettes when the run settled */
    uint8_t base_bg_palettes[64];                     /* w2TilesetBgPalettes then: the palettes the room's load gave, which a fade departs from */
    int8_t palette_offset;                            /* wPaletteThread_parameter then: the offset the game keeps on a room it darkens (checkDarkenRoom, darkenRoom: -16), 0 elsewhere */
    int16_t camera_x, camera_y;                       /* hCameraX/Y when the run settled: the registers' origin inside the room (a large room's window sits inside it) */
    /* The cache key's bytes as the ghost saw them when it primed (the values
     * the room's substitutions read), and the frames run before priming. */
    uint8_t key[ORACLES_GHOST_KEY_BYTES];
    size_t key_len;
    unsigned prerun_frames;
    uint8_t from_group, from_room;                /* the room the ghost primed in (after a pre-run, the room the load reached) */
    /* The objects of the room entered, as they stand when the run settles, with
     * the number each received at the room's initialisation: what a
     * neighbour shows, and what the entry's own objects are paired with. */
    OraclesObjectRecord objects[ORACLES_OBJECT_RECORDS];
    unsigned object_count;
    uint8_t killed_enemies;                       /* the room's enemies counted killed in the run's own list once it entered the room: what its objects hold for */
    /* With the capture (oracles_ghost_set_capture): the room's objects frozen
     * where the game creates them, their sprites tagged by object, and what is
     * needed to draw them: the OAM of the settled frame, the sprite palettes
     * (the tiles are already in `tiles`). */
    uint8_t oam[160];
    OraclesSpriteTag tags[ORACLES_SPRITE_TAGS];
    unsigned tag_count;
    /* The frame drawn before it, for the objects the game draws one frame in
     * two; tag_count_before is 0 when there is none. */
    uint8_t oam_before[160];
    OraclesSpriteTag tags_before[ORACLES_SPRITE_TAGS];
    unsigned tag_count_before;
    uint8_t obj_palettes[64];
} OraclesGhostResult;


/* The cache key: the WRAM ranges applyAllTileSubstitutions
 * reads besides the layout and the stacks, each justified by the routine that
 * reads it (ghost_key.c).  A neighbour's terrain is valid while none of these
 * bytes it read changed in the live instance.  Returns the number of ranges
 * written, at most `max`: callers give ORACLES_GHOST_KEY_RANGES. */
#define ORACLES_GHOST_KEY_RANGES 48u      /* the capacity of the key's list and of the exempt list */
typedef struct OraclesGhostKeyRange { uint16_t address, length; } OraclesGhostKeyRange;
unsigned oracles_ghost_key_ranges(const OraclesCompatProfile *profile, OraclesGhostKeyRange out[], unsigned max);
/* The bytes applyAllTileSubstitutions reads that are not in the key, each
 * with the reason its live value cannot make a cached terrain stale
 * (ghost_key.c).  The ghost check counts a read outside the key and outside
 * this list as an anomaly, so the two lists together stay the closed account
 * of what the substitutions read. */
unsigned oracles_ghost_key_exempt_ranges(const OraclesCompatProfile *profile, OraclesGhostKeyRange out[], unsigned max);
/* The coarse list: the bytes a run reads outside the substitutions that its
 * answer depends on (ghost_key.c).  No entry records them: when one changes
 * in the live instance, the whole cache goes, or the one room it is scoped to.  The snapshot copies them in
 * the order of the ranges (0 when `capacity` is too small); the comparison
 * says whether two snapshots differ in a bit the load reads (a global flag's
 * byte holds other flags, a room's flags its visited bit), and at which
 * address; `room` is the overworld room (group 0) the change is scoped to
 * when it changes that room's terrain alone (GLOBALFLAG_INTRO_DONE in
 * Seasons), -1 when the whole cache goes. */
#define ORACLES_GHOST_COARSE_BYTES 64u
unsigned oracles_ghost_key_coarse_ranges(const OraclesGuestTables *t, OraclesGhostKeyRange out[], unsigned max);
size_t oracles_ghost_coarse_snapshot(OraclesGuest *guest, uint8_t *out, size_t capacity);
int oracles_ghost_coarse_changed(const OraclesGuestTables *t, const uint8_t *before, const uint8_t *after, size_t len, uint16_t *address, int *room);

/* Tiles whose state depends on the entry itself, shown as they stand before
 * it: the shutters replaceShutterForLinkEntering opens under Link. */
int oracles_ghost_entry_dependent_tile(const OraclesGuestTables *t, uint8_t tile);

typedef struct OraclesGhost OraclesGhost;

/* Capture of a neighbour's objects: from the return of initializeRoom
 * the ghost freezes its objects (wDisabledObjects) and hides Link and his
 * companion, so that the room settles with its objects in the state the game
 * creates them in; the result then carries them, their tagged sprites and the
 * palettes to draw them.  Both are restored before the settled state is kept,
 * so a chained run starts from a normal state.  Off by default. */
void oracles_ghost_set_capture(OraclesGhost *ghost, int enabled);
/* The check of the tagging inside the ghost, over the frames it has drawn. */
void oracles_ghost_sprite_stats(const OraclesGhost *ghost, unsigned *frames, unsigned *frames_uncovered, unsigned *worst_gap, unsigned *tags_dropped);
unsigned oracles_ghost_frames_oam_full(const OraclesGhost *ghost);

/* Loads the ROM once (copied by the core) and attaches its own guest. */
OraclesGhost *oracles_ghost_create(const uint8_t *rom, size_t rom_size, const OraclesCompatProfile *profile);
void oracles_ghost_destroy(OraclesGhost *ghost);
size_t oracles_ghost_state_size(OraclesGhost *ghost);

/* Whether a state allows priming: game state 2, no cutscene, scroll mode 1,
 * transition state 2 (no transition in progress), no text, no menu,
 * transitions not disabled, no forced Link state.  Works on any guest, the live one included,
 * so the host knows when a snapshot is worth taking.  `reason` receives a
 * static string when the answer is no. */
int oracles_ghost_primeable(OraclesGuest *guest, const char **reason);

/* Whether a state that cannot be primed will become primeable by itself:
 * the game is loading a room (a warp, the start of play, the fade-in that
 * follows), no text is open.  With `oracles_ghost_set_prerun`, the ghost
 * accepts such a state, runs it until it can be primed (the live instance
 * does the same in the meantime), then primes: the neighbours are ready
 * when the fade-in ends.  A run that never becomes primeable within the
 * pre-run budget times out. */
int oracles_ghost_prerunnable(OraclesGuest *guest, const char **reason);
void oracles_ghost_set_prerun(OraclesGhost *ghost, unsigned max_frames);
/* Seasons: the season (0-3) the next runs start in on the overworld, kept by a
 * forced scroll into another area that follows the season instead of that
 * area's own (the Enhanced band shown in one season, the live one); -1 none,
 * the default: the ghost check compares the game.  ORACLES_GHOST_HOLD_OWN holds the
 * season the ghost has when it primes (a pre-run through a load, whose season the
 * game sets during it).  Taken with each request. */
#define ORACLES_GHOST_HOLD_OWN (-2)
void oracles_ghost_set_held_season(OraclesGhost *ghost, int season);
/* Ages: the next runs start by taking Link to the other side of the open sea
 * where he stands, the room of that index in `group` (a dive from the
 * surface, or a return to it from the sea below): the ghost sets the warp the
 * game sets for it (checkForUnderwaterTransition, docs/GAME_HOOKS.md, section 3), plays the
 * warp and its fade-in, and primes in the room reached, as a pre-run does;
 * the result is the neighbour of that room in the direction asked, or the
 * run times out.  The state given must be primeable.  -1 none, the default.
 * Taken with each request. */
void oracles_ghost_set_level_change(OraclesGhost *ghost, int group);
/* Seasons: whether an overworld room pack takes the season the game holds (0 for an
 * area whose season never changes: no stump for the rod of seasons, or $f0 and up). */
int oracles_ghost_area_holds_season(const OraclesGhost *ghost, uint8_t room_pack);
/* Ages: whether a room is part of the open sea (its tileset under water and
 * outdoors), which is a map of rooms side by side; a house under water is not. */
int oracles_ghost_room_open_water(const OraclesGhost *ghost, uint8_t group, uint8_t room);
/* Whether a room of an overworld group (0 or 1) is on its map: without the
 * profile's tileset map rules, every one; with them, a room whose tileset is
 * outdoors (TILESETFLAG_OUTDOORS) within the map's columns and rows.  The
 * others are interiors on the grid: Moonrise's houses and dungeon filed
 * under its overworld groups and its areas past the 8 x 8 map, Subrosia's
 * rooms past its 11 x 8. */
int oracles_ghost_room_on_map(const OraclesGhost *ghost, uint8_t group, uint8_t room);
/* The rectangle of the grid a group's open sea covers: its first column and
 * row, its columns and rows; 0 when the group has no sea. */
int oracles_ghost_open_water_extent(const OraclesGhost *ghost, uint8_t group, uint8_t extent[4]);
/* Whether the game routes this room's transitions itself (mapTransitionGroupTable,
 * docs/GAME_HOOKS.md, section 4.3): the room it loads when Link leaves is not the grid's neighbour
 * and depends on live state, so each direction is asked of the ghost from the room
 * in play (Seasons' Lost Woods, Ages' forest scrambler, both games' eye puzzle). */
int oracles_ghost_room_self_routed(const OraclesGhost *ghost, uint8_t group, uint8_t room);
/* The case of mapTransitionGroupTable that routes such a room (the routine
 * getNextActiveRoom jumps to), -1 for a room the game does not route. */
int oracles_ghost_room_routine(const OraclesGhost *ghost, uint8_t group, uint8_t room);
/* The routing key of a routine: the bytes it reads to choose the room, besides
 * the room it routes and the direction, each with the routine that reads it
 * (ghost_key.c), so that an answer the ghost gave holds while they keep their
 * values.  Returns the ranges written, 0 when the routine's account is not
 * written: its answers hold for no longer than the transition that asks them
 * again.  The snapshot copies the bytes in order; 0 when there are none. */
#define ORACLES_GHOST_ROUTING_KEY_BYTES 4u
unsigned oracles_ghost_routing_key_ranges(const OraclesCompatProfile *profile, int routine, OraclesGhostKeyRange out[], unsigned max);
size_t oracles_ghost_routing_key_snapshot(OraclesGuest *guest, int routine, uint8_t *out, size_t capacity);

/* The cache key's bytes in the order of the ranges, the active group and
 * room left out; returns the length, 0 when `capacity` is too small. */
size_t oracles_ghost_key_snapshot(OraclesGuest *guest, uint8_t *out, size_t capacity);

/* Synchronous use: load, prime, then step with a budget of guest frames per
 * call.  step returns 1 when the room is initialized (result filled), 0 while
 * more frames are needed, -1 on failure (result.status says why). */
int oracles_ghost_begin(OraclesGhost *ghost, const uint8_t *state, size_t size, OraclesGhostDirection direction, OraclesGhostResult *result);
int oracles_ghost_step(OraclesGhost *ghost, unsigned frame_budget, unsigned max_frames, OraclesGhostResult *result);
/* begin + step in one call. */
int oracles_ghost_run(OraclesGhost *ghost, const uint8_t *state, size_t size, OraclesGhostDirection direction, unsigned max_frames, OraclesGhostResult *result);

/* The same, settling: after the room is initialized the ghost keeps running
 * until its scrolling transition is over (normal play again), then renders
 * the room's game area into the result; `colours` is the 32768-entry table
 * of the live pipeline (NULL: raw), and must outlive the run. */
int oracles_ghost_begin_ex(OraclesGhost *ghost, const uint8_t *state, size_t size, OraclesGhostDirection direction,
                           int settle, const uint32_t *colours, OraclesGhostResult *result);
int oracles_ghost_run_ex(OraclesGhost *ghost, const uint8_t *state, size_t size, OraclesGhostDirection direction,
                         int settle, const uint32_t *colours, unsigned max_frames, OraclesGhostResult *result);
int oracles_ghost_request_ex(OraclesGhost *ghost, const uint8_t *state, size_t size, OraclesGhostDirection direction,
                             int settle, const uint32_t *colours, unsigned max_frames);

/* Threaded use: the request copies the state and wakes the worker; poll
 * returns 1 once with the result, 0 while pending, -1 when nothing was
 * requested.  One request at a time: a request while one is pending fails. */
int oracles_ghost_request(OraclesGhost *ghost, const uint8_t *state, size_t size, OraclesGhostDirection direction, unsigned max_frames);
int oracles_ghost_poll(OraclesGhost *ghost, OraclesGhostResult *result);

/* After a settled run: the ghost's savestate at the render, a state of normal
 * play in the room loaded, from which the room's own neighbours can be asked
 * for.  Valid until the next begin or request; copies at most `capacity`
 * bytes and returns the state's size, 0 when there is none. */
size_t oracles_ghost_settled_state(const OraclesGhost *ghost, uint8_t *out, size_t capacity);
/* Why the last synchronous begin found its state not primeable ("" otherwise). */
const char *oracles_ghost_last_reason(const OraclesGhost *ghost);

/* Read trace of applyAllTileSubstitutions: when enabled,
 * every CPU read between its entry and its return, outside interrupts, is
 * counted per address over all the runs.  Not synchronised with the worker:
 * enable before the first request, read the counts after the last poll. */
void oracles_ghost_set_trace(OraclesGhost *ghost, int enabled);
const uint32_t *oracles_ghost_read_counts(const OraclesGhost *ghost);   /* 65536 counters, NULL when never traced */
/* With the trace on: every read of a run from its priming to its end outside
 * the substitutions, any thread, counted per address over all the runs; what
 * the coarse list has to account for, never a cache key. */
void oracles_ghost_set_trace_load(OraclesGhost *ghost, int enabled);
const uint32_t *oracles_ghost_load_read_counts(const OraclesGhost *ghost);   /* NULL unless enabled */
/* Return hooks the ghost's own guest lost over all its runs (oracles_guest_dropped_returns). */
void oracles_ghost_dropped_returns(const OraclesGhost *ghost, unsigned *overflow, unsigned *purged);

#ifdef __cplusplus
}
#endif

#endif
