/* State and internals shared by the ghost's three files: the run itself
 * (ghost.c), the cache key and the read trace (ghost_key.c), and the tables
 * read from the ROM at creation (ghost_data.c).  Not a public interface: the
 * host sees ghost.h alone. */
#ifndef ORACLES_GHOST_INTERNAL_H
#define ORACLES_GHOST_INTERNAL_H

#include "ghost.h"

#include "guest_struct_offsets.h"
#include "oracles_rom.h"
#include "ppu.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define WRAM_BANK0_BASE 0xc000u
#define WRAM_BANKED_BASE 0xd000u
#define FORCED_TRANSITION 0x80u   /* bit 7 of wScreenTransitionDirection (screenTransitionState2) */
#define NORMAL_PLAY_SCROLL_MODE 0x01u
#define TRANSITION_STATE_IDLE 0x02u
#define GAME_STATE_PLAYING 0x02u
#define SPECIALOBJECT_MINECART 0x0au  /* constants/common/specialObjects.s */
#define CUTSCENE_INGAME 0x01u     /* cutsceneIndices.s: 0 loading a room, 1 in game; higher values are cutscenes */
#define WARP_DEST_SET 0x80u       /* bit 7 of wWarpDestGroup: a warp is set (checkForUnderwaterTransition, link.s) */
#define LEVEL_CHANGE_WARP_TRANSITION2 0x03u   /* the wWarpTransition2 it sets, wWarpTransition 0 */

struct OraclesGhost {
    OraclesCore *core;
    OraclesGuest *guest;
    OraclesObjects *objects;            /* the room's objects and their creation numbers */
    OraclesSprites *sprites;            /* their sprites, tagged by object */
    int capture;                        /* freeze the objects of the room entered and keep their sprites */
    int frozen;                         /* wDisabledObjects raised for the capture */
    uint8_t prior_disabled_objects;
    uint8_t prior_visible[2];           /* Link's and his companion's, cleared while the capture runs */
    size_t state_size;
    /* the run in progress */
    int active;
    int initialized;
    int settle, settled;
    const char *last_reason;            /* why the last begin found its state not primeable */
    int settle_pending;                 /* the transition is over; the render waits for the next vblank's DMA */
    int warp_tiles_guarded;             /* wDisableWarpTiles raised for the run, restored before the settled state is kept */
    int primed;                         /* the forced transition is set; before that, a pre-run */
    int hold_request;                   /* Seasons: the season a run starts in and keeps across areas, -1 none (oracles_ghost_set_held_season) */
    int job_hold;                       /* the request's, copied under the mutex */
    int run_hold;                       /* the run's: a season, -1 none, ORACLES_GHOST_HOLD_OWN the ghost's at priming */
    int primed_hold;                    /* the season the primed run holds, -1 none */
    int level_request;                  /* Ages: the group a run crosses the sea's surface to before priming, -1 none (oracles_ghost_set_level_change) */
    int job_level;                      /* the request's, copied under the mutex */
    int run_level;                      /* the run's */
    int await_group;                    /* the group the run primes in once its warp has taken it there, -1 any */
    uint8_t fixed_season[256];          /* Seasons: 1 for a room pack whose season never changes (no stump, or $f0 and up) */
    uint8_t room_pack_of[256];          /* Seasons: the pack of each overworld room (roomPackData) */
    uint8_t open_water_room[8][256];    /* Ages: 1 for a room of the open sea (under water and outdoors, not one of its houses) */
    uint8_t map_room[2][256];           /* under the profile's tileset map rules, 1 for a room of an overworld group on its map */
    uint8_t self_routed_room[8][256];   /* for a room whose transitions the game routes itself (mapTransitionGroupTable), its routine's case plus one; 0 otherwise */
    unsigned primed_at;                 /* frames run when it was */
    unsigned prerun_max;                /* frames a pre-run may take, 0: a state that cannot be primed is refused */
    OraclesGhostDirection direction;
    uint8_t prior_warp_tiles;
    unsigned settled_frame;
    uint8_t *settled_state;
    size_t settled_size;
    uint32_t *colours;                  /* the run's own copy of the caller's table, or NULL for raw */
    uint32_t *colours_copy;             /* the buffer behind it */
    uint32_t *job_colours_copy;         /* the table of the request in flight, copied under the mutex */
    unsigned frames_run;
    OraclesGhostResult *result;
    uint32_t *full_frame;               /* 160x144 scratch for the terrain render */
    /* read trace */
    int tracing;
    uint32_t *read_counts;
    uint32_t *load_counts;              /* every read of the run, from its priming on (oracles_ghost_set_trace_load) */
    uint32_t reads_this_run;
    uint8_t seen[65536u / 8u];   /* addresses already listed in the run */
    uint16_t layout_start, layout_end, stacks_start, stacks_end;
    uint16_t trace_stack_start, trace_stack_top;   /* the stack of the thread that entered the substitutions (0: any) */
    int in_substitutions;                          /* between the entry and the return of applyAllTileSubstitutions */
    int in_object_gfx;                             /* inside its load of object graphics (Subrosia), which reads no tile state */
    /* worker */
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int thread_started, quit;
    int job_pending, job_done;
    uint8_t *job_state;
    size_t job_size;
    OraclesGhostDirection job_direction;
    int job_settle;
    int job_has_colours;
    unsigned job_max_frames;
    OraclesGhostResult job_result;
    int job_return;
};

/* ghost_key.c */
void oracles_ghost_on_read(void *opaque, uint16_t address, uint16_t sp);
void oracles_ghost_trace_stack_of_entry(OraclesGhost *g);

/* ghost_data.c */
void oracles_ghost_find_fixed_seasons(OraclesGhost *g, const uint8_t *rom, size_t rom_size);
void oracles_ghost_find_open_water_rooms(OraclesGhost *g, const uint8_t *rom, size_t rom_size);
void oracles_ghost_find_map_rooms(OraclesGhost *g, const uint8_t *rom, size_t rom_size);
void oracles_ghost_find_self_routed_rooms(OraclesGhost *g, const uint8_t *rom, size_t rom_size);

#endif
