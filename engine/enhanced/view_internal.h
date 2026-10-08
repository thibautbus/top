/* Internal header of the Enhanced view (view.h is the public one): the cache
 * entry, the view's state and the functions its source files share.  The
 * view is split by concern: view.c (mode choice, composition, savestate,
 * accessors), view_cache.c (the terrain cache and its key), view_map.c (which
 * rooms are neighbours), view_ghost.c (the ghost's runs and their results),
 * view_schedule.c (which room the ghost runs next),
 * view_render.c (the neighbours' pixels, the fade, the large rooms),
 * view_overlay.c (the sprites drawn beyond the game's window). */
#ifndef ORACLES_ENHANCED_VIEW_INTERNAL_H
#define ORACLES_ENHANCED_VIEW_INTERNAL_H

#include "view.h"

#include "ghost.h"
#include "objects.h"
#include "sprites.h"
#include "object_sprites.h"
#include "ppu.h"
#include "guest_struct_offsets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IO_LCDC 0x40u
#define IO_SCY 0x42u
#define IO_SCX 0x43u
#define IO_WY 0x4au
#define IO_WX 0x4bu
#define SMALL_ROOM_W 160
#define LARGE_ROOM_W 240u            /* LARGE_ROOM_WIDTH * 16 */
#define LARGE_ROOM_H 176u            /* LARGE_ROOM_HEIGHT * 16 */
#define ENTRY_ANIMATION_IMAGES 192u   /* images of a neighbour's animation kept to match the live tiles */
#define TEXT_BOX_ROWS (LARGE_ROOM_H / 8u)   /* the cells of the largest room, where a text box is looked for */
#define TEXT_BOX_COLS (LARGE_ROOM_W / 8u)
#define LOOP_RECORD_TAG "LOOP"
#define LOOP_RECORD_SIZE 12u         /* the tag, the loops' columns and rows (int32, little-endian) */
#define SIZE_RECORD_TAG "SIZE"
#define SIZE_RECORD_SIZE 8u          /* the tag, the surface's width and height (uint16, little-endian) */
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
    int near;                           /* last wanted near its reference room, not in the second ring: evicted after the others */
    int rerun;                          /* valid and drawn, its settled state not a start for the rooms beyond: run again from the live state */
    uint32_t rerun_at;                  /* when a chained run last asked for that */
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
    int live_valid;
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
    int was_in_transition;
    /* The room a scrolling transition started in, and the direction it
     * scrolled: what the arrival teaches about a room the game may route. */
    int scroll_from_valid, scroll_saw_scroll;
    uint8_t scroll_from_group, scroll_from_room, scroll_dir_seen;
    int pending;                     /* a ghost run in flight */
    unsigned pending_slot;
    uint8_t pending_group, pending_room;
    unsigned pending_generation;
    int pending_chained;             /* from a parent's settled state */
    int pending_beside;              /* from a room beside it, a cutscene holding the game (view_schedule.c, run_from_beside) */
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
    OraclesGhostResult job;          /* the result of the run in flight */
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
    unsigned animation_renders;       /* this frame's renders for a neighbour's own animation alone */
    unsigned image_live_tiles;        /* tiles a neighbour of another animation took live, the live room showing one of its images */
    unsigned streams_matched;         /* neighbour streams matched with a live stream of the same loop */
    unsigned streams_followed;        /* frames a matched stream was moved to its live twin's step (the view's own step not landing there) */
    unsigned own_animation_copies;    /* copies written into a neighbour's tiles by its own animation */
    unsigned other_animation_copies;  /* of those, into a neighbour whose tileset or animation is not the live room's */
    unsigned curtain_frames;          /* frames the curtain of a warp was extended over the band */
    unsigned waiting_frames;          /* frames an existing overworld neighbour in view was still black */
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
    uint32_t curtain_colour;          /* its colour, read in the core's image while the window still shows it */
    int curtain_colour_valid;
    uint8_t animated_tileset_gfx;    /* the tileset the counts above were seen under */
    unsigned tiles_settling;         /* frames of play left after a load, where a change is the room's graphics and not an animation */
    /* The game's palette fade as displayed: the live palettes against the
     * tileset's base palettes, the offset the palette thread has applied
     * so far, to apply to the other rooms. */
    int fade_effective;
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
    /* The Maku tree stands in the reference room (an INTERAC_MAKU_TREE, $87, both games), sampled in normal play. */
    int ref_maku;
    uint8_t ref_maku_group, ref_maku_room;
    int have_view;
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
    int blurb_seen;                  /* the blurb was there at the last composition, at x blurb_x */
    uint8_t blurb_x;
    uint64_t blurb_mask;             /* this frame's OAM entries that are the blurb's (bit per entry) */
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
    const uint8_t *blurb;            /* the blurb object this frame, or NULL */
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
    int source_valid;
    uint8_t source_group, source_room;
    int32_t source_left, source_top;
    uint64_t source_epoch;
    int source_pipeline;
    unsigned uncovered;              /* world-band pixels without a source in the last composed frame */
    unsigned off_camera_frames;      /* frames the game drew its area elsewhere than its camera: framed */
    unsigned wave_frames;            /* frames the game scrolled its area line by line (under water, the strange force): shown in the band, waves and all */
    int16_t line_shift[ORACLES_ENHANCED_AREA_HEIGHT], line_shift_y[ORACLES_ENHANCED_AREA_HEIGHT];   /* this frame's per-line scroll, less the camera's */
    int have_wave;
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
};

/* ---- shared between the view's files ---- */
int ev_tracked_read(const OraclesEnhancedView *v, uint16_t a);
uint8_t ev_live_byte(OraclesEnhancedView *v, uint16_t a);
size_t ev_read_key(OraclesEnhancedView *v, uint8_t *out);
long ev_key_offset(const OraclesEnhancedView *v, uint16_t a);
void ev_drop_entry(entry *e);
void ev_invalidate_all(OraclesEnhancedView *v);
uint16_t ev_room_flags_address(const OraclesEnhancedView *v, uint8_t group, uint8_t room);
void ev_invalidate_by_reads(OraclesEnhancedView *v);
void ev_refresh_colours(OraclesEnhancedView *v);
entry *ev_find_entry(OraclesEnhancedView *v, uint8_t group, uint8_t room);
entry *ev_take_slot(OraclesEnhancedView *v, uint8_t group, uint8_t room, const uint8_t *wanted, unsigned wanted_count);
entry *ev_find_routed_entry(OraclesEnhancedView *v, uint8_t group, uint8_t from_room, OraclesGhostDirection dir);
entry *ev_take_routed_slot(OraclesEnhancedView *v, uint8_t group, uint8_t from_room, OraclesGhostDirection dir, const uint8_t *wanted, unsigned wanted_count);
void ev_drop_routed_entries(OraclesEnhancedView *v);
void ev_check_routing_keys(OraclesEnhancedView *v);
int ev_room_toward(const OraclesEnhancedView *v, uint8_t room, OraclesGhostDirection dir, uint8_t *out);
int ev_room_toward_in(const OraclesEnhancedView *v, uint8_t group, uint8_t room, OraclesGhostDirection dir, uint8_t *out);
/* Under the profile's tileset map rules, a room of an overworld group (0 or 1) off its map: a house, a dungeon, an
 * area past the map (Moonrise's, Subrosia's), an interior on the grid.  0 without the rules, and for the other groups. */
int ev_off_map(const OraclesEnhancedView *v, uint8_t group, uint8_t room);
/* The columns and rows of an overworld group's map. */
unsigned ev_map_width(const OraclesEnhancedView *v, uint8_t group);
unsigned ev_map_height(const OraclesEnhancedView *v, uint8_t group);
int ev_sea_other_side(const OraclesEnhancedView *v, uint8_t *group);
int ev_room_beside(const OraclesEnhancedView *v, uint8_t room, unsigned side, uint8_t *out);
int ev_isolated_room(const OraclesEnhancedView *v);
int ev_on_grid(const OraclesEnhancedView *v);
int ev_on_map(const OraclesEnhancedView *v);
int ev_open_water(const OraclesEnhancedView *v, uint8_t group, uint8_t room);
int ev_self_routed(const OraclesEnhancedView *v, uint8_t group, uint8_t room);
int ev_routed_candidate(const OraclesEnhancedView *v);
int ev_routes_elsewhere(const OraclesEnhancedView *v, uint8_t group, uint8_t room);
void ev_note_routing(OraclesEnhancedView *v, uint8_t group, uint8_t from_room, OraclesGhostDirection dir, uint8_t got);
int ev_in_routed_zone(const OraclesEnhancedView *v);
int ev_routed_beyond_shown(const OraclesEnhancedView *v, uint8_t room, OraclesGhostDirection *dir_out);
void ev_routed_place(uint8_t ref, OraclesGhostDirection dir, int32_t *left, int32_t *top);
void ev_routed_run_order(const OraclesEnhancedView *v, uint8_t ref, unsigned out[4]);
int ev_on_overworld(const OraclesEnhancedView *v);
int ev_entry_connected(const OraclesEnhancedView *v, const entry *e, OraclesGhostDirection *dir_out);
void ev_update_reference_edges(OraclesEnhancedView *v);
void ev_update_neighbours(OraclesEnhancedView *v);
/* view_ghost.c, for the run order (view_schedule.c): a run started for an
 * entry, the run in flight advanced, the blind pre-runs of a load, and
 * whether an entry holds for the live key. */
int ev_start_job(OraclesEnhancedView *v, entry *e, const uint8_t *state, size_t size, OraclesGhostDirection dir, const entry *parent);
void ev_advance_pending_run(OraclesEnhancedView *v);
void ev_blind_prerun(OraclesEnhancedView *v);
int ev_entry_key_current(const OraclesEnhancedView *v, const entry *e, const uint8_t *key, size_t key_len);
int ev_entry_drawable(const OraclesEnhancedView *v, const entry *e);
void ev_capture_source_terrain(OraclesEnhancedView *v);
/* The OAM of the live frame on screen with only the sprites of the room's own
 * objects (kinds 1 to 3, not persistent), by the live tags; returns 0 when the
 * tags of that OAM are unknown or leave nothing. */
int ev_live_room_objects_oam(OraclesEnhancedView *v, uint8_t out[160]);
/* Whether OAM entry `index` on screen belongs to an object of a room (kinds 1
 * to 3, not persistent): during a transition those are drawn beyond the
 * window by the captures and the image of the room left, not by the overlay. */
int ev_live_entry_is_room_object(OraclesEnhancedView *v, unsigned index);
/* view_objects.c: the events of the live instance, and the fade of what appears on entry. */
void ev_objects_event(OraclesEnhancedView *v, const OraclesGuestEvent *event);
void ev_fade_appearing(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y);
int ev_source_is_scroll_source(const OraclesEnhancedView *v);
void ev_track_animated_tiles(OraclesEnhancedView *v);
/* Every frame the game animates: each cached neighbour's own animation, one step. */
void ev_advance_neighbour_animations(OraclesEnhancedView *v);
/* The live room's animation handed to its entry as it becomes a neighbour. */
void ev_hand_live_animation(OraclesEnhancedView *v, entry *e);
void ev_track_fade(OraclesEnhancedView *v);
/* view_scroll.c: a scroll's animation run by the view, once a composition after the live tiles are tracked; the tiles and
 * the mask of animated tiles the band draws the live tileset with; the game's window and the captures drawn with them. */
void ev_scroll_animation(OraclesEnhancedView *v);
/* A state loaded: no scroll under way, and the clock as when the view started. */
void ev_scroll_reset(OraclesEnhancedView *v);
const uint8_t *ev_shown_vram(OraclesEnhancedView *v, unsigned bank);
const uint8_t *ev_shown_animated(const OraclesEnhancedView *v);
const uint32_t *ev_scroll_window(OraclesEnhancedView *v);
void ev_scroll_keep_capture(OraclesEnhancedView *v, const OraclesPpuRegs *regs, const uint8_t *palettes);
/* The last capture of the room in play, drawn from the inputs it kept the first time it is read. */
const uint32_t *ev_source_area(OraclesEnhancedView *v);
const uint32_t *ev_scroll_capture(OraclesEnhancedView *v, int left);
/* The frames from a scroll's first frozen frame to the one where its counter
 * runs out, the load (states 3 and 4) taking `load` frames and the counter
 * starting at `steps`, a step a frame (`per_step` 1) or every two frames (2). */
unsigned ev_scroll_expected_frames(unsigned load, unsigned steps, unsigned per_step);
/* A scroll frozen: its clock started, and the frames its streams are first
 * planned over returned; none without the continuous transitions, the
 * screen standing still until the scroll moves. */
unsigned ev_scroll_clock_begin(EvScrollClock *c, int horizontal, int continuous);
/* One frame of the scroll: `scrolling` whether its counter runs (state 5
 * under way), `counter` and `delta` the game's (wScreenScrollCounter, wcd14).
 * At its first step the scroll's length is known, and again should the step
 * change before the counter moves: *replan is set and *plan holds the frames
 * to plan the streams over again.  Returns the frames still
 * to come before the one that lands (0: this one). */
unsigned ev_scroll_clock_frame(EvScrollClock *c, int scrolling, unsigned counter, uint8_t delta, int *replan, unsigned *plan);
/* The tiles of a scroll, each marked as bank * 384 + tile in masks of
 * 2 * 384 bits, the tile data as two banks of TILE_DATA_BYTES (`live0`,
 * `live1`; `begin` and `images` one after the other).  The tiles the game
 * has written since the freeze (the live data against `begin`) marked in
 * `game_wrote`: returns how many of them the view's streams had written,
 * the game's own again. */
unsigned ev_scroll_note_game_writes(const uint8_t *live0, const uint8_t *live1, const uint8_t *begin, const uint8_t *touched, uint8_t *game_wrote);
/* The tiles the view's streams wrote, taken from their images into `tiles`
 * (two banks `stride` bytes apart), but those the game has written. */
void ev_scroll_overlay(uint8_t *tiles, size_t stride, const uint8_t *images, const uint8_t *touched, const uint8_t *game_wrote);
/* The tiles the view's streams wrote that differ from the live data, but
 * those `held` (the copies the game's queue holds back) and those the game
 * has written. */
unsigned ev_scroll_tiles_off(const uint8_t *live0, const uint8_t *live1, const uint8_t *images, const uint8_t *touched, const uint8_t *held, const uint8_t *game_wrote);
int ev_displayed_fade(const uint8_t live[64], const uint8_t base[64]);
const uint32_t *ev_neighbour_pixels(OraclesEnhancedView *v, entry *e);
void ev_entry_used_tiles(entry *e);
void ev_render_large_room(OraclesEnhancedView *v);
void ev_overlay_edge_sprites(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y);
void ev_large_objects_event(OraclesEnhancedView *v, const OraclesGuestEvent *event);
void ev_overlay_large_objects(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y);
void ev_find_area_blurb(OraclesEnhancedView *v);
void ev_overlay_area_blurb(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y);
/* The columns of its map the game has not redrawn yet after a warp, over the whole band. */
void ev_overlay_warp_curtain(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y);
/* A dialogue's text box, drawn again at the middle of the band (view_textbox.c). */
void ev_overlay_text_box(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y);
/* view_hotbar.c: the hotbar of the item hotkeys, drawn last in the HUD band's gutters, in every mode of the surface. */
void ev_draw_hotbar(OraclesEnhancedView *v);
void ev_hotbar_event(OraclesEnhancedView *v, const OraclesGuestEvent *event);
/* view_mode.c: the frame's mode, and the ripple the band follows (v->have_wave, v->line_shift). */
OraclesEnhancedMode ev_choose_mode(OraclesEnhancedView *v, int tracking);
/* view_black.c: the black of the composed frame counted, once a frame. */
void ev_measure_black(OraclesEnhancedView *v, OraclesEnhancedMode mode, int32_t camera_x, int32_t camera_y);
/* Whether the part of the band shown, the band's top-left corner at (band_left,
 * band_top), meets a rectangle of the world grown by `reach` each way. */
int ev_shown_meets(const OraclesEnhancedView *v, int32_t band_left, int32_t band_top, int32_t left, int32_t top, int32_t width, int32_t height, int32_t reach);
/* The last composition showed more than the normal band: the drawn-back view outdoors, at sea, in a large room. */
static inline int ev_drawn_back(const OraclesEnhancedView *v) { return v->shown_width > ORACLES_ENHANCED_NARROW_WIDTH; }
/* The surface's pixel at band row `y`, column `x`. */
static inline uint32_t *ev_band_pixel(OraclesEnhancedView *v, unsigned x, unsigned y) { return &v->surface[(ORACLES_ENHANCED_HUD_HEIGHT + y) * v->size.width + x]; }

#endif
