/* Internal header of the Enhanced view (view.h is the public one): the cache
 * entry, the view's state and the functions its source files share.  The
 * view is split by concern: view.c (mode choice, composition, savestate,
 * accessors), view_cache.c (the terrain cache and its key), view_map.c (which
 * rooms are neighbours), view_ghost.c (the ghost's runs and their results),
 * view_schedule.c (which room the ghost runs next),
 * view_render.c (the neighbours' pixels, the fade, the large rooms),
 * view_overlay.c (the sprites drawn beyond the game's window), view_poses.c (a
 * neighbour's objects animated in place), view_hud.c (the HUD band). */
#ifndef ORACLES_ENHANCED_VIEW_INTERNAL_H
#define ORACLES_ENHANCED_VIEW_INTERNAL_H

#include "view.h"

#include "ghost.h"
#include "objects.h"
#include "sprites.h"
#include "object_sprites.h"
#include "object_animation.h"
#include "ppu.h"
#include "guest_struct_offsets.h"
#include "text_box.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUB_LAYER_SIZE ORACLES_ENHANCED_SUBPIXEL_LAYER_PITCH           /* the largest drawing of Link and what he holds the sub-pixel camera draws apart */
#define SUB_MADE_PIXELS 1024u   /* his pixels the edge overlay draws without the OAM in a frame, at most: four sprites of 8x16 and more */

#define IO_LCDC 0x40u
#define IO_SCY 0x42u
#define IO_SCX 0x43u
#define IO_WY 0x4au
#define IO_WX 0x4bu
#define SMALL_ROOM_W 160
#define LARGE_ROOM_W 240u            /* LARGE_ROOM_WIDTH * 16 */
#define LARGE_ROOM_H 176u            /* LARGE_ROOM_HEIGHT * 16 */
#define BORDER_COLOUR 0xff101010u    /* the framed core's border, and the HUD band's gutters beside the bar centred */
#define ENTRY_ANIMATION_IMAGES 192u   /* images of a neighbour's animation kept to match the live tiles */
#define TEXT_BOX_ROWS (LARGE_ROOM_H / 8u)   /* the cells of the largest room, where a text box is looked for */
#define TEXT_BOX_COLS (LARGE_ROOM_W / 8u)
#define LOOP_RECORD_TAG "LOOP"
#define LOOP_RECORD_SIZE 12u         /* the tag, the loops' columns and rows (int32, little-endian) */
#define SIZE_RECORD_TAG "SIZE"
#define SIZE_RECORD_SIZE 8u          /* the tag, the surface's width and height (uint16, little-endian) */
#define OFFSET_RECORD_TAG "SHOW"
#define OFFSET_RECORD_SIZE 12u       /* the tag, the camera's whole offsets across and down (int32, little-endian; OFFSET_NONE: none) */
#define OFFSET_NONE INT32_MIN
#define INTERAC_ERA_OR_SEASON_INFO 0xe0u   /* constants/common/interactions.s: the blurb shown at the top of the screen on entering an area */
#define GHOST_SETTLE_FRAMES 160u     /* a load (4 frames) and a scroll (about 45) with margin */
#define GHOST_BLIND_SETTLE_FRAMES 320u   /* a pre-run primes as soon as play looks normal, while an arrival (the time travel's) still holds the transition back */
#define GHOST_PRERUN_FRAMES 240u     /* a room load and its fade-in, run ahead by the ghost */
#define SIDES 2u                     /* left, right */
/* The room, its four neighbours, the two beyond left and right, the four
 * diagonals and the second ring: 21 rooms, 25 with the drawn-back
 * view's corners; twice that, for Ages' sea is computed on its
 * other side before a dive or a return to the surface, and room to spare
 * for the four directions of a room the game routes itself.  An entry is 268
 * KiB, its settled state aside: the view allocates as many as its size fills
 * (view->slot_count), and SLOTS bounds them, for the arrays that hold a
 * composition's sources. */
#define SLOTS 56u
#define EV_LANES 2u                  /* ghosts at most, each with a run in flight */
#define NORMAL_SLOTS 48u
#define PLAN_ROOMS 25u               /* the rooms a plan wants at most */
#define SEA_LEVEL_GROUPS 2u          /* the sea under a group of the surface is two groups on (checkForUnderwaterTransition) */
#define FAILED_RETRY_FRAMES 300u     /* a run that failed is tried again after five seconds */
#define ENTRY_READS 64u
#define DROP_LOG 16u
#define TILE_DATA_BYTES 0x1800u      /* $8000-$97ff of a VRAM bank: the tiles */
#define MAP_OFFSET 0x1800u           /* $9800-$9fff: the maps */
#define ANIMATION_STEPS_MAX 4u      /* the game's animation steps taken at once by the neighbours: more is a load, not play */
#define LARGE_OBJECT_FRAMES 3u      /* drawings kept: the OAM on screen lags wOam by a frame or two */
#define LARGE_OBJECT_SPRITES 256u   /* sprites of the objects of one drawing: 64 objects queued at most, of a few sprites each */
#define SCROLL_PACE_MAX 1000u       /* the paces a scroll's streams ran at, in percent of the game's, counted up to this */

/* The frames of a scroll whose animation the view runs, from its first
 * frozen frame to the one where the game's counter runs out (view_scroll.c). */
typedef struct EvScrollClock {
    unsigned load_frames;    /* the fewest frames a scroll's load has taken before it moved, SCROLL_LOAD_FRAMES at most */
    int started;             /* a scroll has begun since the view started or loaded a state: load_frames holds */
    unsigned elapsed;        /* frames since the freeze */
    unsigned expected;       /* the frame the plan lands on, counted from the freeze */
    unsigned landed_at;      /* the frame it landed on, the counter run out; 0: not yet */
    int seen_counter;        /* its first step seen: its length is known */
    unsigned first_step;     /* the frames from the freeze to that first step */
    unsigned first_counter;  /* the counter at it, whole until its first row is drawn */
    unsigned per_step;       /* the frames a step takes, 1 with the step doubled, 2 without */
    int continuous;          /* the continuous transitions on: Link walks during the load, and the streams run */
} EvScrollClock;

/* An object of a neighbour animated in place (view_poses.c): its animation as captured and as stepped since, its
 * record, the pixel a puddle lowers it by, its own sprites among its tag's entries in the two frames captured and the
 * pose those entries show (as the capture drew them: a pose or two past the captured one for an object the game
 * animated while the ghost settled), and the pose the entry's last render drew. */
typedef struct ev_pose {
    uint8_t kind, slot, record, lower, own[2];
    OraclesObjectAnimation captured, now;
    uint16_t shown[2];
    uint16_t drawn;
} ev_pose;

/* One room's terrain in the cache. */
typedef struct entry {
    int used;
    uint8_t group, room;
    int valid;                       /* terrain ready, under the bytes it read */
    int failed;                      /* the ghost could not settle it: black, tried again later */
    uint32_t failed_at;
    int retry_when_link_moves;       /* the ghost warped instead (a door under Link), or its capture was no terrain: try again once he has moved */
    int retry_after_fade;            /* its capture was a fade the live game was showing too: try again as well once that fade is over */
    int link_x, link_y;
    uint32_t last_use;
    /* what the ghost delivered */
    uint32_t game_area[ORACLES_GHOST_AREA_WIDTH * ORACLES_GHOST_AREA_HEIGHT];   /* rendered once by the ghost, frozen */
    int game_area_pipeline;          /* the colour pipeline it was rendered in (ev_refresh_colours) */
    uint8_t layout[ORACLES_GHOST_LAYOUT_BYTES];
    uint8_t bg_map[ORACLES_GHOST_MAP_BYTES];
    uint8_t regs3[6];
    uint8_t used_tiles[2u * 384u / 8u];   /* the tiles its map draws with, a bit each (bank * 384 + tile): set with the map */
    uint8_t tileset_gfx, tileset_palette, tileset_unique_gfx;
    /* Its tile animation, advanced by the view from the game's data while it
     * is cached: `tiles` is animated in place, and `animation_version`
     * counts the copies written into them, for the render's key. */
    OraclesAnimationState animation;
    unsigned animation_version;
    uint64_t live_base_hash;           /* the key of its last render, its animation aside */
    uint8_t own_animated[2u * 384u / 8u];   /* the tiles its own animation has written: no longer the ghost's */
    /* The images its animation can put in each tile it writes (tile, ROM
     * offset of the 16 bytes): where the live room shows one of them on that
     * tile, it is the same element, and the neighbour takes the live tile to
     * stay in step with the room across the edge (the sea of Seasons). */
    uint16_t image_tiles[ENTRY_ANIMATION_IMAGES];
    uint32_t image_sources[ENTRY_ANIMATION_IMAGES];
    unsigned image_count;
    uint32_t align_key;                /* the live room its streams were matched against */
    OraclesAnimationFollow follow;     /* its streams that run the same loop as the live room's, which they follow */
    uint8_t base_bg_palettes[64];       /* the palettes its load gave (w2TilesetBgPalettes), which a fade departs from */
    int8_t palette_offset;              /* the offset the game keeps on it, a dark room's (-16) */
    int season_dependent;               /* a Seasons overworld room: its season is the live one while the game is in its area */
    int refresh;                        /* valid, still drawn, but run again: the live season changed under it, or an enemy of its room died */
    int refresh_season;                 /* that refresh is a season change: its terrain differs from the live one until it is delivered */
    unsigned refresh_failures;          /* runs of the refresh that failed: at the second, the old terrain is no longer drawn */
    uint32_t accepted_at;               /* the view's frame when its last result was accepted */
    uint32_t render_wanted_at;          /* 1 + the frame its own animation first wanted a render the frame's budget put off; 0: none waiting */
    uint32_t render_put_off_at;         /* the last frame that render was put off */
    int near;                           /* last wanted near its reference room, not in the second ring: evicted after the others */
    int rerun;                          /* valid and drawn, its settled state not a start for the rooms beyond: run again from the live state */
    uint32_t rerun_at;                  /* when a chained run last asked for that */
    int not_a_start;                    /* valid, but its settled state refused to prime a run (a scene its entry starts disables the transitions): no parent until delivered again */
    int waits_start;                    /* its parent's state is no start (ev_wait_for_start): it waits for another parent, settled for the band's fill, that parent run again at the pace of a failure */
    uint32_t waits_since;               /* since when */
    unsigned waits_parent, waits_ref;   /* that parent and the reference room then, group << 8 | room: the mark holds for them alone */
    int corner_asked;                   /* a corner beyond it asked for that, and it has not held for the key since (view_schedule.c) */
    int strict_parent;                  /* a run from a parent judged with other rooms' visited bits aside read one of them: its parents judged on the whole key until it is delivered */
    int settled_primeable;              /* its settled state was in normal play when kept: a run from it primes at once, without a pre-run that may go to another room */
    uint8_t season;                     /* the season it was loaded in */
    uint8_t collisions[ORACLES_GHOST_LAYOUT_BYTES];
    uint8_t tiles[2u * ORACLES_GHOST_TILE_BYTES];   /* its own tile data: tileset, unique graphics, one animation frame */
    uint8_t bg_palettes[64];         /* its own background palettes */
    /* Its objects, captured frozen where the game creates them: the
     * OAM of the settled frame, the sprites tagged by object, the object
     * records that say which of them may be shown, and the sprite palettes. */
    uint8_t oam[160];
    OraclesSpriteTag tags[ORACLES_SPRITE_TAGS];
    unsigned tag_count;
    uint8_t oam_before[160];         /* the frame drawn before, shown on odd frames: what the game draws one frame in two flickers as it does */
    OraclesSpriteTag tags_before[ORACLES_SPRITE_TAGS];
    unsigned tag_count_before;
    OraclesObjectRecord objects[ORACLES_OBJECT_RECORDS];
    unsigned object_count;
    /* The live wEnemiesKilledList its run started from (a parent's, for a
     * room chained from it): what its objects, and those of the rooms
     * chained from it, hold for.  Keyed on the live game's list, not on the
     * one the ghost's run went through, which inserts the rooms it crosses:
     * a room's own entry may leave it on the way. */
    uint8_t killed_list[16];
    uint8_t obj_palettes[64];
    /* Its objects animated in place (view_poses.c); back to their captured poses when a transition enters its room,
     * the render that shows them so taking a render of the frame's budget kept for it; the first frame it was shown
     * behind its poses, plus one. */
    ev_pose poses[ORACLES_OBJECT_RECORDS];
    unsigned pose_count;
    int poses_reset;
    uint32_t pose_lag_since;
    int16_t camera_x, camera_y;      /* the game's camera when the ghost settled: the registers' origin in the room */
    int large;                       /* a 240x176 room */
    /* A room whose transitions the game routes itself: the entry
     * holds the room the ghost loaded leaving `routed_from` in `routed_dir`,
     * which is not the grid's neighbour and changes with the live state.
     * Keyed by that pair and not by the room, several directions leading to
     * the same room (the Lost Woods sends Link back to its entrance). */
    int routed;
    uint8_t routed_from;
    uint8_t routed_dir;
    /* The routing key of the routine that routes `routed_from`, as the live game
     * had it when the ghost was asked (oracles_ghost_routing_key_ranges): the
     * answer holds while it keeps these values; length 0, the routine has no
     * account and the answer goes with the transition that asks it again. */
    uint8_t routing_key[ORACLES_GHOST_ROUTING_KEY_BYTES];
    uint8_t routing_key_len;
    uint8_t *settled_state;          /* normal play in this room: the source of its own neighbours */
    size_t settled_size;
    /* invalidation: the bytes of the cache key its substitutions read, with their values then */
    uint16_t read_addr[ENTRY_READS];
    uint8_t read_value[ENTRY_READS];
    unsigned read_count;
    uint8_t key_snapshot[ORACLES_GHOST_KEY_BYTES]; /* the whole key when it was requested: a room beyond is chained from it only while unchanged */
    size_t key_len;
    /* the view's own render of the room from its map, tiles and palettes: the whole room (240x176 for a large one) */
    uint32_t live_area[LARGE_ROOM_W * LARGE_ROOM_H];
    uint64_t live_tiles_hash;
    uint64_t live_inputs;              /* what that render read: rendered again only when it changes */
    /* The tiles and the rest (palettes, objects) that render was drawn with: the
     * next one draws again only the lines whose tiles have changed since. */
    uint8_t rendered_tiles[2u * ORACLES_GHOST_TILE_BYTES];
    uint64_t rendered_rest;
    uint8_t rendered_oam[160];         /* and its objects' OAM: an entry changed since, a pose, draws its lines again */
    int live_valid;
    int in_flight;                     /* a run of its room is under way in one of the ghosts (ev_lane) */
    int queued;                        /* or waits for the thread's ghost, busy when it was asked */
    uint32_t run_serial;               /* that run's: its result is this entry's only while the serials agree */
} entry;

/* A drawing of the game in a large room: every object's sprites, built, and the wOam it wrote. */
typedef struct large_object_frame {
    uint8_t oam[160];
    OraclesObjectSprite sprites[LARGE_OBJECT_SPRITES];
    unsigned count;
    uint64_t entries;                /* the OAM entries its sprites account for (bit per entry) */
    uint8_t group, room;             /* the room it was drawn in */
    int valid;
} large_object_frame;

/* A drawing of the game (the return of drawAllSprites, objects drawn): the
 * wOam it wrote, and the area blurb among its objects, its place and state
 * with the camera the objects were placed by. */
#define BLURB_DRAWINGS 4u           /* drawings kept: the OAM on screen lags wOam by a frame, more through a room's load */
typedef struct blurb_drawing {
    uint8_t oam[160];
    int valid, there;
    uint8_t x, y, state;
    int camera_x, camera_y;
} blurb_drawing;

#define HOTBAR_ICON_VARIANTS 8u   /* no variant, then the seeds 0 to 4 or the songs 1 to 3 */

struct OraclesEnhancedView {
    OraclesCore *core;
    OraclesGuest *guest;
    OraclesEnhancedCamera *camera;
    OraclesEnhancedObserver observer;
    OraclesEnhancedObservation observation;   /* of the last composed frame */
    uint32_t frame;
    int framed_only;
    OraclesEnhancedBlack black;      /* view_black.c */
    struct { uint8_t group, room; uint32_t since; } black_runs[32];   /* the places black now, and since when */
    uint32_t drawn_places[8];        /* the rooms of the reference's group the last composition drew at their place on the grid, a bit each */
    unsigned black_run_count;
    /* The black by kind (view_black.c): the rooms' episodes under way, the edge's, the ripple's, the lone room's and the rest's, since
     * when (0 none, else the frame plus one), and the frame the band's last fill began. */
    struct { uint8_t group, room, kind, in_transition; uint32_t since; } black_episodes[32];
    unsigned black_episode_count;
    uint32_t black_place_since[4];
    uint8_t black_place_transition[4];
    uint32_t black_cold_from;
    int black_have_ref, black_loaded;
    uint8_t black_ref_group, black_ref_room;
    char black_log[512];
    OraclesEnhancedSize size;        /* the surface's, a level's in a shape (oracles_enhanced_view_size), chosen before the first composition */
    unsigned band_height;            /* its world band's, oracles_enhanced_band_height of the size */
    unsigned shown_width, shown_height;   /* the part of the band the last composition showed, centred (oracles_enhanced_camera_shown) */
    int chain_routed_idle;           /* the run order's last pass, with nothing else to run: a room the game routes itself may be a parent (drawn-back view) */
    uint32_t *surface;               /* size.width by size.height, allocated for the view's size */
    /* the ghost and its jobs */
    OraclesGhost *ghost;
    uint8_t *snapshot;
    size_t state_size;
    unsigned sync_budget;
    int neighbour_objects;           /* draw the objects a neighbour captured */
    OraclesSprites *live_sprites;    /* the live instance's sprites, tagged by object, while the objects are drawn */
    OraclesObjects *live_objects;    /* the live instance's objects and their creation numbers */
    /* The objects that fade in on entry (view_objects.c) */
    uint8_t appear_seen[4][16];
    uint32_t appear_first[4][16];    /* the view's frame each became visible on */
    int appear_active, appear_ready, appear_ended;
    uint32_t appear_end_frame;
    uint8_t appear_group, appear_room;
    unsigned fade_frames, faded_objects;   /* frames that drew a fade, objects that started one */
    unsigned objects_outdated;       /* cached rooms whose objects were dropped: an enemy of theirs killed, or back, since the capture */
    unsigned refresh_given_up;       /* rooms run again whose old terrain was dropped after two failed runs */
    unsigned parents_run_for_kills;  /* parents run again from the live state before chaining: an enemy killed since their run */
    /* The hand-off, measured on the data rather than on the composed pixels:
     * each object a capture showed, followed from the entry to its live twin's
     * first visible frame (view_objects.c). */
    struct { uint8_t kind, slot, checked; OraclesObjectRecord shown; int32_t box_left, box_top, box_right, box_bottom; } handoff[48];
    unsigned handoff_count;
    int handoff_armed;
    unsigned handoff_shown, handoff_jumps, handoff_hole_frames, handoff_unseen;
    /* The image of the room a scroll left (design, 6.2.2): its terrain and its
     * objects as the game last drew them in normal play, shown frozen while the
     * room stays in the band, instead of its capture of apparition. */
    uint32_t left_area[ORACLES_GHOST_AREA_WIDTH * ORACLES_GHOST_AREA_HEIGHT];
    int left_valid;
    uint8_t left_group, left_room;
    int32_t left_left, left_top;
    uint64_t left_epoch;
    int left_pipeline;
    /* What its objects were drawn from (the capture's source_* fields). */
    OraclesPpuRegs left_render_regs;
    uint8_t left_oam[160], left_obj_palettes[64];
    OraclesSpriteTag left_tags[ORACLES_SPRITE_TAGS];
    unsigned left_tag_count;
    OraclesObjectRecord left_objects[ORACLES_OBJECT_RECORDS];
    unsigned left_object_count;
    /* The objects of the room a scroll left that its capture does not show
     * where the game last drew them, which fade out (view_objects.c): their
     * pixels in the image of that room, RGB555, VANISH_NONE elsewhere, at the
     * room's place. */
    uint32_t vanish_area[ORACLES_GHOST_AREA_WIDTH * ORACLES_GHOST_AREA_HEIGHT];
    int vanish_active;
    uint32_t vanish_first;           /* the view's frame the fade started on, the first out of the scroll; 0: the scroll goes on */
    int32_t vanish_left, vanish_top;
    uint64_t vanish_epoch;
    unsigned fade_out_frames, faded_out_objects;   /* frames that drew a fade out, objects that started one */
    int was_in_transition;
    /* The room a scrolling transition started in, and the direction it
     * scrolled: what the arrival teaches about a room the game may route. */
    int scroll_from_valid, scroll_saw_scroll;
    uint8_t scroll_from_group, scroll_from_room, scroll_dir_seen;
    int pending;                     /* a ghost run in flight */
    unsigned pending_slot;
    uint8_t pending_group, pending_room;
    unsigned pending_generation;
    int pending_pipeline;            /* the colour pipeline the run renders in */
    int pending_chained;             /* from a parent's settled state */
    int pending_beside;              /* from a room beside it, a cutscene holding the game (view_schedule.c, run_from_beside), or from a parent judged with other rooms' visited bits aside: its reads checked on delivery */
    int pending_visits;              /* that parent judged so (view_schedule.c, run_from_parent) */
    uint8_t pending_killed_list[16]; /* the live wEnemiesKilledList the pending run stands for */
    unsigned pending_parent;
    uint8_t pending_parent_room;
    int pending_blind;               /* a pre-run while a room loads: the result lands beside the room the ghost primed in */
    int pending_routed;              /* a direction asked from a room the game routes itself: whatever room comes back is the answer */
    uint8_t pending_routed_from;
    OraclesGhostDirection pending_dir;
    /* the blind pre-runs of the current load: one per direction */
    uint64_t blind_epoch;
    uint8_t blind_room, blind_mask;
    unsigned blind_results, blind_dropped;
    uint32_t pending_parent_accepted; /* the parent's accepted_at when its settled state was taken: another ghost may deliver it again meanwhile */
    uint32_t pending_serial;         /* the run's serial, given to its entry (entry.run_serial) */
    uint32_t run_serials;            /* the last serial given */
    OraclesGhostResult *job;         /* the result of the run in flight */
    /* The ghosts, each with its run in flight: the fields above from `ghost`
     * to `job` are those of the lane being served (ev_lane_serve), lane 0's
     * between two services.  One lane when the ghost is synchronous. */
    struct ev_lane {
        OraclesGhost *ghost;
        OraclesGhostResult *job;
        int pending;
        unsigned pending_slot;
        uint8_t pending_group, pending_room;
        unsigned pending_generation;
        int pending_pipeline;
        int pending_chained, pending_beside, pending_visits;
        uint8_t pending_killed_list[16];
        unsigned pending_parent;
        uint8_t pending_parent_room;
        uint32_t pending_parent_accepted, pending_serial;
        int pending_blind, pending_routed;
        uint8_t pending_routed_from;
        OraclesGhostDirection pending_dir;
    } lanes[EV_LANES];
    unsigned lane_count, lane_now;
    const OraclesCompatProfile *rom_profile;   /* for a ghost created later (oracles_enhanced_view_set_ghosts) */
    OraclesCoreKind ghost_core;
    unsigned generation;             /* bumped when every entry must go: the colour pipeline, a savestate load */
    /* the cache */
    entry *slots;                    /* slot_count of them, allocated for the view's size */
    unsigned slot_count;             /* NORMAL_SLOTS, or SLOTS in the drawn-back view */
    int shown[4];                    /* a neighbour's terrain shown this frame, by the game's direction (0 up, 1 right, 2 down, 3 left) */
    unsigned shown_slot[4];
    unsigned requested, completed, failed, stale, chained, refreshed, live_renders;
    unsigned live_lines_kept;        /* lines of those renders not drawn again, their tiles unchanged */
    unsigned live_renders_kept;      /* renders the key asked for whose inputs had not changed: not made */
    unsigned season_rejected;        /* results of a room of the live area in another season than the live one (Seasons): not kept */
    unsigned beside_refused;         /* results run from a room beside, a byte they read no longer the live one: not kept */
    unsigned plain_renders;          /* results refused as no terrain (oracles_enhanced_capture_blank): a fade caught, the LCD off */
    unsigned live_diff_max;          /* 8x8 blocks of non-animated tiles a live render differs from the ghost's render by, at most: none if right */
    char plain_log[256];             /* "group:room" of each, space separated, as far as it fits */
    /* The reference room's collisions, sampled in normal play (during a
     * transition the game's buffer already holds the destination's): an
     * interior opens onto a room beside it when both share free edge cells. */
    uint8_t ref_collisions[ORACLES_GHOST_LAYOUT_BYTES];
    int have_ref_collisions;
    uint8_t ref_collisions_group, ref_collisions_room;
    /* The room the last scrolling transition came from: connected to the
     * reference room by that very fact, before the ghost has run it. */
    int32_t view_left, view_top;     /* the world band of the last composition, for the run order */
    /* The tiles the live VRAM animates: those seen changing from one frame
     * of normal play to the next since the tileset was loaded (the water,
     * the flowers, the torches), one bit per tile and bank. */
    uint8_t live_tiles_prev[2u * ORACLES_GHOST_TILE_BYTES];
    int have_live_tiles_prev;
    uint8_t animated[2u * 384u / 8u];
    uint64_t live_tiles_hash;          /* this frame's hash of the live tile data of both banks, hashed once for every neighbour's render */
    int have_live_tiles_hash;
    uint8_t tile_changes[2u * 384u];   /* times each tile was seen changing: an animation cycles, a load (a font, a room's unique graphics) changes once */
    /* The ROM, kept for the neighbours' animation data (the launcher frees its own copy early). */
    uint8_t *rom;
    size_t rom_size;
    unsigned animation_renders;       /* this frame's renders for a neighbour's own animation alone, its objects' poses included */
    /* A neighbour's objects animated in place (view_poses.c): off in the Low quality, the harness's check of the
     * renders, the live game's updates of its objects since the last composition, where the last OAM composed put
     * each pose's sprites, and the counts. */
    int poses_off, pose_verify;
    unsigned object_updates;
    uint8_t pose_at[ORACLES_OBJECT_RECORDS], pose_count_at[ORACLES_OBJECT_RECORDS];
    OraclesEnhancedPoseCounts poses;
    unsigned render_wait_max;         /* the most frames in a row a neighbour's own-animation render waited, the frame it was made past the budget counted */
    unsigned renders_forced;          /* such renders made past the frame's budget, having waited RENDER_WAIT_LIMIT frames */
    unsigned image_live_tiles;        /* tiles a neighbour of another animation took live, the live room showing one of its images */
    unsigned streams_matched;         /* neighbour streams matched with a live stream of the same loop */
    unsigned streams_followed;        /* frames a matched stream was moved to its live twin's step (the view's own step not landing there) */
    unsigned own_animation_copies;    /* copies written into a neighbour's tiles by its own animation */
    unsigned other_animation_copies;  /* of those, into a neighbour whose tileset or animation is not the live room's */
    unsigned curtain_frames;          /* frames the curtain of a warp was extended over the band */
    unsigned waiting_frames;          /* frames an existing overworld neighbour in view was still black */
    /* The rooms never visited kept dark (view_black.c): the rooms of the reference's group the last composition drew so,
     * in that epoch, and those drawn so now; the rooms fading in since, from the frame each was shown. */
    int unvisited_hidden;
    int unvisited_hard;               /* their edge toward the rooms drawn in clear cut hard (the Low quality) */
    int dark_check;                   /* the harness's check of the rooms kept dark past their edge */
    unsigned dark_lit;                /* the rooms the last composition kept dark and showed in clear past their edge */
    unsigned dark_stray;              /* and those it lit on a side where nothing in clear stands beyond */
    int have_hidden_places;
    uint8_t hidden_group;
    uint64_t hidden_epoch;
    uint32_t hidden_places[8], hidden_now[8];
    uint32_t shown_places[8], shown_now[8];   /* the rooms of that group the last composition, and the one under way, drew in clear or fading in */
    uint32_t referenced[8];           /* the rooms of that group that were the reference in that epoch */
    unsigned hidden_count;            /* the rooms drawn dark by the composition under way */
    struct { uint8_t room; uint32_t since; } reveals[8];
    unsigned reveal_count;
    uint32_t bare_places[8], bare_now[8];   /* the rooms of that group the last composition, and the one under way, laid dark with no pixels yet */
    struct { uint8_t room; uint32_t since; } edge_fades[8];   /* the rooms kept dark whose edge comes in since their pixels came, from that frame */
    unsigned edge_fade_count;
    unsigned edge_fades_counted;      /* edges that came in so */
    unsigned hidden_frames;           /* world frames with a room kept dark */
    unsigned hidden_rooms_max;        /* the most rooms kept dark in one frame */
    unsigned reveal_fades;            /* rooms that faded in, once visited */
    unsigned unvisited_reference;     /* frames of normal play, the first after a transition aside, whose reference room was not visited */
    uint32_t unvisited_reference_first;   /* the first of them, plus one; 0 none */
    uint8_t unvisited_reference_group, unvisited_reference_room;
    unsigned reference_settled;       /* compositions of normal play in a row, the last one's included */
    unsigned routed_delivered;        /* answers that were not the grid's neighbour: what the game routes itself */
    /* Learned from the ghost's own answers: a candidate room whose direction
     * came back as another room than the grid's neighbour.  A room of
     * mapTransitionGroupTable routes only under the game's own condition
     * (Ages' forest scrambler stops once the forest is unscrambled), which
     * the host does not read: it asks, and believes the answer. */
    uint8_t routes_elsewhere[8][256];   /* by room: a bit by direction */
    unsigned plain_shown_frames;      /* frames a neighbour holding a fade's palettes was drawn in the band */
    unsigned text_box_frames;         /* frames a text box was moved to the middle of the band */
    unsigned text_box_trail;          /* compositions left where the box is still the box after the game's text flag fell */
    unsigned host_text_box_trail;     /* the same for the box the host draws */
    uint32_t host_text_box[ORACLES_TEXT_BOX_WIDTH_MAX * ORACLES_TEXT_BOX_HEIGHT_MAX];   /* the box the host drew last */
    OraclesTextBoxModel host_text_box_model;   /* and what it drew it from */
    OraclesTextBoxSource host_text_box_source;
    int host_text_box_drawn;
    unsigned wide_pixels, wide_lines;    /* the box the host draws in the surface (oracles_enhanced_view_set_text_box), 0 for the core's */
    int wide_shown;                      /* the last composition drew it, at (wide_x, wide_y) in the surface */
    int32_t wide_x, wide_y;
    OraclesEnhancedHostTextBox wide_box;
    unsigned wide_frames_world, wide_frames_framed;   /* compositions that drew it, by mode */
    unsigned wide_uncovered;             /* compositions of a text whose box the host could not draw: the core's shown */
    unsigned wide_scale;                 /* the scale the last composition drew the host's box at */
    unsigned text_size;                  /* Text size: ORACLES_ENHANCED_TEXT_* */
    int large_menus;                     /* the framed core shown alone (menus=large): its box enlarged by the presenter, at every size */
    int enlarged_noted, enlarged_bottom; /* the box at its size at enlarged_rect in the surface, which the presenter enlarges with Large */
    int enlarged_framed;                 /* over the framed core: XLARGE the presenter's too with the large menus */
    unsigned enlarged_width;             /* the width its scale is chosen for (box_scale, framed_scale) */
    int32_t enlarged_rect[4], enlarged_src[2];   /* (x, y, w, h); the core image's pixels it shows, the game's box */
    int enlarged_from_core;
    unsigned scaled_trail;               /* the frames after a text, the game's box read without the host's lines */
    int scaled_shown;                    /* the last composition drew the game's box from the core's image at twice its size, */
    int32_t scaled_x, scaled_y, scaled_src_x, scaled_src_y;   /* at (x, y) in the surface, from (src_x, src_y) in the core's image, */
    unsigned scaled_w, scaled_h;         /* that many pixels of it */
    unsigned scaled_frames_world, scaled_frames_framed;   /* compositions that drew a box twice as large, by mode */
    int above;                           /* the box to draw above all in the world band: 1 the core's enlarged, 2 the host's */
    OraclesEnhancedHostTextBox above_box;
    unsigned above_scale, above_size[2];
    int above_lower;                     /* the game drew its box in the lower half of its screen */
    int32_t above_source[2], above_at[2];   /* the core image's pixels it enlarges; its top-left in the surface */
    uint32_t curtain_colour;          /* its colour, read in the core's image while the window still shows it */
    int curtain_colour_valid;
    uint8_t animated_tileset_gfx;    /* the tileset the counts above were seen under */
    unsigned tiles_settling;         /* frames of play left after a load, where a change is the room's graphics and not an animation */
    /* The game's palette fade as displayed: the live palettes against the
     * tileset's base palettes, the offset the palette thread has applied
     * so far, to apply to the other rooms. */
    int fade_effective;
    /* A scene's tint (ev_track_scene_tint): the palettes a cutscene shows
     * instead of the room's own (the night of Nayru's possession), as a
     * colour transform from the palettes the room's load gives to those, to
     * apply to the other rooms.  `tint_changed`: the palettes it changed;
     * `tint_slots`: those the transform explains. */
    int scene_tint;
    int transition_mix;                /* the palette thread mixes the palettes of a scroll's two rooms, or left their mix on screen */
    unsigned transition_mix_room;      /* the room (group << 8 | room) it mixes in */
    uint32_t tint_fade_at;             /* the last frame with a fade, a scroll or that mix */
    OraclesTintMemory tint_memories[ORACLES_TINT_MEMORIES];   /* by room, its own palettes a tint was taken from, kept while the scene lasts */
    uint32_t tint_epoch;               /* the observation's epoch they were kept in */
    uint8_t tint_slots, tint_changed;
    uint8_t tint_reference[64], tint_scene[64];
    int32_t tint_coefficients[12];     /* by output channel: red, green, blue and a constant, in 1/1024 */
    uint64_t tint_key;
    unsigned scene_tint_frames;        /* frames composed under a scene's tint */
    uint32_t scene_tint_first, scene_tint_last;
    /* The hotbar of the item hotkeys (view_hotbar.c). */
    int hotbar_shown;
    unsigned hud_top;                /* the surface's line where the status bar stands this frame: 0, or the framed drawn-back surface's centred image's */
    OraclesEnhancedHotbar hotbar;
    unsigned hotbar_refusals_seen[ORACLES_ENHANCED_HOTBAR_SLOTS];
    uint32_t hotbar_refused_until[ORACLES_ENHANCED_HOTBAR_SLOTS];
    struct { uint8_t item, variant; unsigned frames; int verified; } hotbar_on_button[2];   /* how long B and A have held the same item */
    uint16_t (*hotbar_icons)[HOTBAR_ICON_VARIANTS][256];                      /* by item and variant (0: none); NULL until the first capture */
    uint8_t hotbar_have_icon[ORACLES_GUEST_ITEM_LABELS][HOTBAR_ICON_VARIANTS];
    int hotbar_icons_stale;                                                   /* giveTreasure ran: an item may have changed level, and icon */
    unsigned hotbar_icons_captured, hotbar_icons_shown;
    /* What the harness checks: each icon against the core's own image, and the surface
     * outside the four slots before and after they are drawn. */
    int hotbar_verify;
    int8_t hotbar_icon_origin[ORACLES_GUEST_ITEM_LABELS][HOTBAR_ICON_VARIANTS][2];   /* where the icon's corner stands from the sprites' left edge, and from the top line */
    unsigned hotbar_icons_checked, hotbar_icon_wrong, hotbar_outside_gutters;
    int hotbar_drawn;                /* the frame just composed drew it */
    int hotbar_framed;               /* this frame shows the core's image framed (a menu, a cutscene), a room's load apart */
    unsigned hotbar_fade_checked;     /* frames of a fade or a flash its icons were checked against the bar's own */
    OraclesEnhancedBarLayout bar_shown_layout;   /* the status bar's layout last shown (ev_status_bar_shown) */
    /* The status bar drawn full width (hud=full, and laid out): the gutters' colours of the last composition, line by
     * line, the world frames that filled a line at least, and the lines of those frames left to the border. */
    int hud;                         /* OraclesEnhancedHud */
    uint32_t hud_fill[2u * ORACLES_ENHANCED_HUD_HEIGHT];
    unsigned hud_frames, hud_lines_kept;
    /* The status bar laid out (hud=wide, view_hud.c): the last composition's blocks, the layout last laid out, whether
     * the tightened B and A are known (Biggoron's sword frames them either way), and the world frames counted. */
    int hud_laid;
    OraclesEnhancedHudPlace hud_place;
    OraclesEnhancedBarLayout hud_layout;
    int hud_tight_known, hud_tight;
    unsigned hud_wide_frames, hud_wide_centred, hud_wide_one_row;
    /* The Maku tree stands in the reference room (an INTERAC_MAKU_TREE, $87, both games), sampled in normal play. */
    int ref_maku;
    uint8_t ref_maku_group, ref_maku_room;
    int have_view;
    int framed_load;                 /* the last composition framed a room's load: not enlarged with the large menus */
    int enlarged_before;             /* the composition before it was framed and not a load's: enlarged with the large menus */
    /* A room's load under the world's HUD band (view_mode.c): whether the last composition drew one, the place of the
     * game area in the surface (the last world frame's, or the load's), whether a place is held, and the frames counted. */
    int load_band, load_area_held;
    int32_t load_area_left, load_area_top;
    unsigned load_band_frames, load_blank_frames;
    int have_last_ref, have_came_from;
    uint8_t last_ref_group, last_ref_room, came_from_room;
    uint64_t last_ref_epoch;
    OraclesGhostDirection came_from_dir;    /* from the reference room toward it */
    unsigned last_scroll_dir;               /* the direction of the last scroll seen, the game's */
    int have_last_scroll_dir;
    /* which key bytes dropped entries, and how often: the invalidation in figures */
    uint16_t drop_addr[DROP_LOG];
    unsigned drop_count[DROP_LOG];
    unsigned drops;
    /* the coarse list (oracles_ghost_key_coarse_ranges) as last seen in normal play, and the caches it threw */
    uint8_t coarse[ORACLES_GHOST_COARSE_BYTES];
    size_t coarse_len;
    unsigned coarse_drops;
    unsigned failure_reasons[5];
    char failure_log[512];           /* the wrong rooms: "from>expected=got" */
    /* the cache key of the live instance */
    OraclesGhostKeyRange key[ORACLES_GHOST_KEY_RANGES];
    unsigned key_count;
    /* the colour table of the live pipeline, for the renders */
    uint32_t colours[ORACLES_PPU_COLOURS];
    int colours_pipeline;
    /* the identity table: a render through it holds RGB555 colours, for the
     * captures the compose fades and converts itself (compositor.h) */
    uint32_t raw_colours[ORACLES_PPU_COLOURS];
    /* a large room: its whole terrain from the live BG map, rendered each frame */
    uint32_t strip[LARGE_ROOM_W * ORACLES_PPU_HEIGHT];
    uint32_t strip_raw[LARGE_ROOM_W * LARGE_ROOM_H];      /* the whole room, 240x176, RGB555 */
    uint32_t strip_area[LARGE_ROOM_W * LARGE_ROOM_H];     /* the same in the live pipeline */
    int strip_valid;
    /* The last whole render of a large room in normal play: the room being
     * left during a scroll (the live map is then rewritten row by row). */
    uint32_t large_capture[LARGE_ROOM_W * LARGE_ROOM_H];
    int large_capture_valid;
    int32_t large_capture_left, large_capture_top;
    uint64_t large_capture_epoch;
    int large_capture_pipeline;
    uint8_t large_capture_group, large_capture_room;
    /* The room left by the last scroll, kept drawn at its place while the
     * camera still shows part of it (the pan to Link after the rebase). */
    uint32_t large_prev[LARGE_ROOM_W * LARGE_ROOM_H];
    int large_prev_valid;
    uint32_t popup_frame[ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT];   /* the LCD frame rendered again by the software PPU: without the blurb's sprites, those that appear, or a text box */
    blurb_drawing blurb_drawings[BLURB_DRAWINGS];
    unsigned blurb_drawings_head;
    uint64_t blurb_mask;             /* this frame's OAM entries that are the blurb's (bit per entry) */
    unsigned link_unshown_frames;    /* scroll frames with Link to draw, none of him on the screen nor drawn past its edge */
    unsigned link_built_frames;      /* scroll frames he was drawn from his fields, the game writing none of his sprites */
    int edge_link_made;              /* this frame's edge overlay drew sprites of his the OAM does not hold (a column, or all of him) */
    int edge_link_built;             /* all of him, from his fields */
    /* The objects of a large room beyond the game's window: at each drawing
     * of the game in a large room, every object's
     * sprites built from its fields and the game's data, kept with the wOam
     * the game wrote, to be found again by the OAM on screen. */
    large_object_frame large_objects[LARGE_OBJECT_FRAMES];
    unsigned large_objects_head;
    unsigned animation_steps;        /* the game's animation steps since the last composition (updateAnimations' entries) */
    int animation_steps_counted;     /* its hook is armed: the count stands for the steps */
    unsigned large_objects_first;    /* hOamTail / 4 at the entry of the drawing under way */
    unsigned large_objects_drawn;    /* objects the game drew since the entry of drawAllSprites */
    uint64_t large_objects_entries;  /* this composition's: the OAM entries the large room's overlay draws, which the edge overlay leaves */
    int large_objects_listening;
    void *run_plan;                  /* the plan of the ghost's runs, rebuilt each frame (view_ghost.c) */
    void *sea_plan;                  /* Ages: the plan of the same place on the other side of the sea, run when the first has nothing to run */
    int level_change;                /* the group a run from the live state crosses the sea's surface to first, -1 none */
    uint8_t keep_group, keep[PLAN_ROOMS];   /* the other plan's rooms, which a slot taken for this one does not take */
    unsigned keep_count;
    unsigned sea_runs;               /* runs started across the sea's surface, ahead of a dive or a return to it */
    unsigned large_object_sprites_drawn;   /* sprites drawn beyond the window, over the run */
    unsigned large_object_frames_unmatched;  /* compositions in a large room whose OAM on screen matched no drawing kept */
    int blurb_built_x;               /* its x when the OAM on screen was built */
    uint8_t blurb_built_y;           /* and its y */
    int blurb_built_camera_x;        /* the camera that OAM placed it by */
    int blurb_shown;                 /* the OAM on screen holds the blurb: built while it was there */
    int large_was_in_scroll;         /* the last composition, a world one, was inside a large room's scroll */
    uint64_t large_scroll_epoch;     /* its epoch */
    int32_t large_prev_left, large_prev_top;
    uint64_t large_prev_epoch;
    int large_prev_pipeline;
    /* scratch for the live renders of the neighbours */
    uint8_t hybrid_vram[0x4000];
    uint32_t hybrid_frame[ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT];
    /* the room Link is in, rendered from the live VRAM before a scroll: what
     * the game's window leaves behind during the scroll, when the cache has
     * no entry for that room yet */
    uint32_t source_area[ORACLES_GHOST_AREA_WIDTH * ORACLES_GHOST_AREA_HEIGHT];   /* read through ev_source_area */
    /* drawn when it is read: the capture keeps the inputs of its render (the VRAM and the BG palettes in
     * source_vram and source_palettes) */
    int source_area_pending;
    OraclesPpuRegs source_render_regs;
    uint8_t source_oam[160], source_obj_palettes[64];
    OraclesSpriteTag source_tags[ORACLES_SPRITE_TAGS];   /* the tags of its objects' sprites in source_oam */
    unsigned source_tag_count;
    OraclesObjectRecord source_objects[ORACLES_OBJECT_RECORDS];
    unsigned source_object_count;
    int source_valid;
    uint8_t source_group, source_room;
    int32_t source_left, source_top;
    uint64_t source_epoch;
    int source_pipeline;
    unsigned uncovered;              /* world-band pixels without a source in the last composed frame */
    unsigned uncovered_edges;        /* and the sides of the part shown whose edge had some (OraclesEnhancedCompose) */
    unsigned off_camera_frames;      /* frames the game drew its area elsewhere than its camera: framed */
    unsigned world_off_camera_frames;   /* world frames a cutscene drew elsewhere than its camera (oracles_enhanced_view_world_off_camera_frames) */
    unsigned wave_frames;            /* frames the game scrolled its area line by line (under water, the strange force): shown in the band, waves and all */
    unsigned sway_frames;            /* of those, frames of a room shown whole, its waves held (oracles_enhanced_view_sway_frames) */
    int32_t sway;                    /* this frame's part common to the lines taken off, without the game's sine */
    int wave_held;                   /* this frame's waves held to the room shown whole: the core's image drawn again (ev_sway_window) */
    int16_t held_shift[ORACLES_ENHANCED_AREA_HEIGHT];   /* their scroll across, line by line, from the game's sine */
    const int16_t *shown_shift;      /* the shifts the band shows this frame: line_shift or held_shift */
    int waves_continued;             /* this frame's waves drawn through a scroll the game stopped them for */
    unsigned continued_wave_frames;
    uint32_t sway_window[ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT];   /* the core's image, its background drawn again without it */
    unsigned shake_frames;           /* frames the game shook its screen (updateScreenShake): shown in the band, shaken whole */
    unsigned shake_history;          /* bit n: the shake under way n + 1 observations ago (the image on screen shows the registers of a logic run one or more observations before) */
    int16_t line_shift[ORACLES_ENHANCED_AREA_HEIGHT], line_shift_y[ORACLES_ENHANCED_AREA_HEIGHT];   /* this frame's per-line scroll, less the camera's */
    int have_wave;
    int wave_lines;                  /* the lines' scrolls differ (the waves, the strange force), where a shake moves them all alike */
    /* The tile animation of a scroll, run by the view alone (view_scroll.c). */
    int scroll_decided, scroll_running;
    OraclesScrollAnimation scroll;
    EvScrollClock scroll_clock;
    unsigned scroll_gap_last, scroll_tiles_off_last, scroll_version;
    int scroll_wrote;                /* the view's streams have written an image this scroll */
    uint8_t play_tileset_gfx, play_tileset_animation;   /* the live room's in normal play, before the scroll loads the next */
    int have_play_tileset;
    int shown_active;                /* this frame draws the live tileset with the view's tiles */
    uint8_t scroll_images[2u * TILE_DATA_BYTES];   /* the images the view's streams wrote, in their tiles */
    uint8_t scroll_begin_tiles[2u * TILE_DATA_BYTES];   /* the live tile data when the scroll froze the animation */
    uint8_t shown_tiles[2u * TILE_DATA_BYTES];     /* the live tile data, the tiles the streams wrote taken from those images */
    uint8_t scroll_touched[2u * 384u / 8u], shown_animated[2u * 384u / 8u];
    uint8_t scroll_game_wrote[2u * 384u / 8u];   /* tiles the game has written since the freeze: its own again */
    uint32_t scroll_frame[ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT];
    uint32_t scroll_window[ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT];
    uint8_t source_vram[0x4000], left_vram[0x4000];            /* the VRAM of both banks, tiles and maps, the captures were drawn from */
    OraclesPpuRegs source_regs, left_regs;
    uint8_t source_palettes[64], left_palettes[64];
    uint32_t source_shown[ORACLES_GHOST_AREA_WIDTH * ORACLES_GHOST_AREA_HEIGHT], left_shown[ORACLES_GHOST_AREA_WIDTH * ORACLES_GHOST_AREA_HEIGHT];
    unsigned source_shown_version, left_shown_version;
    unsigned scrolls_animated, scrolls_still, scroll_landed, scroll_landed_off, scroll_gaps, scroll_gap_max, scroll_tiles_off;
    unsigned scroll_streams_run, scroll_window_pixels, scroll_tiles_given_back;
    unsigned scroll_pace[SCROLL_PACE_MAX + 1u];
    /* Link drawn in phase on a diagonal walk (view_phase.c). */
    int link_phase;                  /* enabled */
    unsigned phase_count;            /* positions kept, up to 3 */
    int32_t phase_x[3], phase_y[3];  /* Link's room-local position in 256ths, this observation's and the two before */
    uint64_t phase_epoch[3];
    uint8_t phase_room[3];
    int phase_latched;               /* the offset below is in use */
    int32_t phase_c;                 /* added to his y, in 256ths: his y steps on the frames his x does */
    int phase_dy, phase_last_dy;     /* this frame's and the last: Link drawn that many pixels below the game's sprite */
    uint64_t phase_mask;             /* his OAM entries on screen, to draw moved */
    unsigned phase_frames, phase_engaged_frames;
    /* The sub-pixel camera (view_phase.c): the band moved by the camera's fraction of a pixel at the screen's
     * resolution, Link and what he holds drawn apart, unmoved. */
    int subpixel;                    /* enabled */
    int sub_leaving;                 /* switched off during the game: on until the band and Link have given their fractions back */
    int32_t sub_x, sub_y;            /* this frame's, in 256ths of a pixel: the band drawn that much left and up */
    /* The fraction shown, kept from frame to frame: where the camera's fraction comes or goes (the reducer tracking or
     * not, Link drawn apart or not for a stretch), the band takes it up or gives it back by SUB_EASE a frame from where
     * it stood, never at once. */
    uint32_t sub_last_frame;
    uint64_t sub_last_epoch;
    int sub_have_last, sub_live;
    int32_t sub_last_x, sub_last_y, sub_raw_x, sub_raw_y;   /* the last frame's fraction shown, and the camera's */
    unsigned sub_shake_hold;         /* frames left before the band takes a fraction again after a shake of the screen */
    /* The camera held where it stood at a scroll's start that opens an interior's room (ev_hold_camera), and what the
     * last frame composed in the world left for it (ev_hold_note). */
    int camera_held;
    int hold_have, hold_was_whole;   /* the last frame showed a room whole, in play */
    int hold_was_still;              /* the last frame stood in an interior's room, in play, the camera as on the frame before */
    uint8_t hold_group, hold_room;   /* the room of the last frame in play */
    uint32_t hold_frame;
    uint64_t hold_epoch;
    int32_t hold_camera_x, hold_camera_y;   /* the last frame's camera, whole pixels */
    int32_t sub_ease_x, sub_ease_y;  /* the fraction shown less the camera's, given back by SUB_EASE a frame */
    int32_t sub_link_x, sub_link_y;  /* Link's own fraction shown, his layer's place less his whole one, eased the same way */
    int sub_link_live;
    int32_t sub_link_last_x, sub_link_last_y, sub_link_raw_x, sub_link_raw_y, sub_link_ease_x, sub_link_ease_y;
    uint32_t sub_layer[SUB_LAYER_SIZE * SUB_LAYER_SIZE];   /* Link's pixels, 0 where none */
    uint32_t sub_patch[SUB_LAYER_SIZE * SUB_LAYER_SIZE];   /* the window without him under them, 0 elsewhere */
    uint8_t sub_pending[SUB_LAYER_SIZE * SUB_LAYER_SIZE];  /* his pixels past the window, which the edge overlay draws: taken once it has */
    uint32_t sub_fixed[ORACLES_ENHANCED_SUBPIXEL_FIXED][4];  /* the surface's rectangles shown unmoved (the text box, the area's name) */
    unsigned sub_fixed_count;
    int32_t sub_layer_x, sub_layer_y;   /* its top left in the surface */
    int32_t sub_layer_fx, sub_layer_fy;   /* and its move, in 256ths of a pixel right and down: Link's place less the band's */
    uint32_t sub_layer_w, sub_layer_h;
    unsigned sub_frames, sub_layer_frames, sub_refused_frames;
    unsigned sub_left_pixels, sub_wrong_pixels, sub_alike_pixels;   /* the checks of the layer (view_phase.c) */
    unsigned sub_covered_pixels;     /* his pixels another drawing covered after the layer was taken, left to the band */
    unsigned sub_unexplained_frames; /* frames the window rendered again did not explain around him: left to the band */
    uint64_t sub_taken;              /* the OAM entries the layer took, this frame's */
    int sub_ready;                   /* this frame may draw him apart: his layer taken, or none of him in the OAM to take it from */
    /* His sprites the edge overlay made without the OAM this frame (a column, or all of him), each pixel as it drew it:
     * its place in the band, the band's colour there before and its own; the settling takes them into the layer. */
    struct { int32_t x, y; uint32_t under, colour; } sub_made[SUB_MADE_PIXELS];
    unsigned sub_made_count;
    int sub_made_lost;               /* more than sub_made holds */
    unsigned sub_objects_left;       /* special objects near him at a fraction of their own, left to the band (an object a frame) */
};

#include "view_shared.h"   /* the functions the view's files share */

#endif
