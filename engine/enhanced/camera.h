/* Enhanced camera: the smooth reducer fed from the guest bus.
 *
 * Each frame the host reads Link's world position and the room's world extent
 * from the guest, builds an authenticated observation, and reduces it into a
 * camera position; it never writes to the guest.  The world-coordinate math
 * is a pure function so it can be tested without a ROM.  The exterior
 * overworld (groups 0 and 1) is wide enough to pan; a room narrower than the
 * viewport (an interior) leaves the reducer in its fallback, which the
 * compositor draws framed. */
#ifndef ORACLES_ENHANCED_CAMERA_H
#define ORACLES_ENHANCED_CAMERA_H

#include "guest.h"
#include "smooth_camera.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* wScrollMode during a scrolling transition (docs/GAME_HOOKS.md, section 3): $04
 * once screenTransitionState2 has decided it, $08 from getNextActiveRoom,
 * through the load of the room entered and the scroll, until the game sets
 * it back to $01. */
#define ORACLES_SCROLL_MODE_DECIDED 0x04u
#define ORACLES_SCROLL_MODE_TRANSITION 0x08u

/* Pure: a room-local coordinate byte of Link read with its sign.  Inside a
 * room the byte is unsigned (at most 240); during a scrolling transition the
 * game lets it run past the edge of the room he leaves
 * (transitionUpdateScrollAndLinkPosition): below zero, wrapping, when the
 * scroll goes left or up, past the width when it goes right or down.  `axis`
 * is 0 for x, 1 for y; `direction` the game's (0 up, 1 right, 2 down, 3 left). */
int oracles_enhanced_signed_coordinate(unsigned raw, int in_scroll, unsigned direction, int axis, int small_room);
/* `small_room`: a 160x128 room, whose coordinates never reach 0xc0 in play,
 * so a byte there is negative whatever the state: Link a few pixels past the
 * top before the game's transition starts (Seasons lets him), or parked at
 * a placeholder (0xf8) while a save loads. */

/* Link's position and the room's extent in world pixels (not fixed point). */
typedef struct OraclesEnhancedWorld {
    int32_t link_x, link_y;                 /* world pixels */
    int32_t origin_x, origin_y;             /* world origin of the room Link's coordinates are relative to */
    int32_t bounds_origin_x, bounds_origin_y;
    int32_t bounds_width, bounds_height;    /* world pixels; >= viewport width means the camera can pan */
    OraclesE11Domain domain;
    int32_t viewport;                        /* the reducer's viewport here: 256, or the band's width where the drawn-back view draws back */
    int alone;                               /* a small room shown alone or with the rooms it opens onto (an interior, a room isolated outdoors) */
    int wide;                                /* bounds_width >= the reducer's viewport width */
} OraclesEnhancedWorld;

/* Pure: Link's world position and the room's extent, from the room identity
 * and Link's room-local pixel position, signed: during a scrolling transition
 * the game keeps Link's coordinates relative to the room he leaves and lets
 * them run past its edge (bank1.s, transitionUpdateScrollAndLinkPosition),
 * until finishScrollingTransition rebases them.  Groups 0 and 1 are the
 * exterior and past overworlds, a grid of small rooms; the others are treated
 * as a single room area.  Returns 1 on success, 0 for an unusable room. */
int oracles_enhanced_world_from_room(uint8_t group, uint8_t room, int room_is_large,
                                     int link_x, int link_y, unsigned overworld_stride, unsigned map_width, unsigned map_height,
                                     unsigned map_left, unsigned map_top,
                                     unsigned open_edges, int isolated, uint8_t cell, unsigned band_width, OraclesEnhancedWorld *out);
/* `map_width`, `map_height`: the edges past the last column and row of an overworld group's map, where the camera
 * stops (0: the grid's); `map_left`, `map_top`: its first column and row, where it stops on the other side. */
/* `isolated` values: a room of an overworld group off its map (by the profile's tileset map rules: a house, a
 * dungeon, an area past the map) is laid out as an interior there, alone or with the rooms it opens onto, in its
 * group's domain and band, as the Maku tree's screen (ORACLES_ENHANCED_ISOLATED) is. */
#define ORACLES_ENHANCED_ISOLATED 1
#define ORACLES_ENHANCED_INDOORS 2
/* `band_width`: the world band's width outdoors (groups 0 and 1), which is
 * the reducer's viewport there: the band of the view's size, 213 to 480
 * (256 the normal one, 480 the drawn-back view); 0 stands for 256.  Elsewhere
 * the viewport is 256, or the band where it is narrower, but for a large
 * room of the grid (groups 2 to 7), which a band wider than 256 shows whole
 * in the whole band. */
/* Pure: in the drawn-back view (a band wider than 256), a room of Ages' open
 * sea takes the sea for its extent, as the map is outdoors, and the whole
 * band for its viewport: `sea` is the sea's rectangle on the grid (first
 * column and row, columns, rows).  The normal band keeps the room and the
 * rooms of the sea beside it. */
void oracles_enhanced_world_extend_to_sea(OraclesEnhancedWorld *world, const uint8_t sea[4], unsigned band_width);
/* `cell`: the room's place on its grid (row << 4 | column): the room byte
 * itself on the overworlds and in the interiors; in a dungeon, whose rooms
 * are laid out by the floor's 8x8 map (wDungeonMapPosition) and not by
 * their index, the map position. */
/* `isolated`: a room shown alone whatever its group (Ages' Maku tree
 * screen, every large room): its extent is the room widened to the
 * viewport, plus the rooms `open_edges` names (the one a scroll goes to or
 * came from, so that the camera stays continuous through it). */
/* `open_edges`: for an interior (groups 2 and 3), the directions (bit d for
 * the game's direction d: 0 up, 1 right, 2 down, 3 left) in which the room
 * opens onto the room beside it on the grid, as the view has established
 * them; its extent is the room plus those, widened to the viewport. */

/* The observer keeps the room Link's coordinates refer to: wActiveRoom
 * changes when the next room is loaded, two frames into a transition, while
 * Link's coordinates are rebased only when the scroll ends; between the two,
 * the reference stays the room he leaves, and his world position is
 * continuous across the seam.  A room change outside a scrolling transition
 * (a warp) is a teleport: it opens a new epoch for the camera. */
typedef struct OraclesEnhancedObserver {
    int have_reference;
    uint8_t ref_group, ref_room, ref_large;
    uint8_t last_group, last_room;
    int scrolling;                           /* a scrolling transition has loaded its room since normal play */
    uint64_t epoch;
    int have_held;                           /* Link's last position while he is not warping: held during a warp */
    int held_x, held_y;
    uint8_t held_group, held_room;           /* the reference room it is relative to: in another room it means nothing */
    /* The window of the previous observation: the frame on screen was drawn
     * with it (the game copies its camera into the display registers at the
     * vblank that starts the frame, after the logic that moved it). */
    int was_in_scroll;                       /* the previous observation was inside a scroll */
    int32_t scroll_start_left, scroll_start_top;   /* the game's window at the scroll's first frame: a large room's camera slides from it */
    int have_last_window;
    int32_t last_window_left, last_window_top;
    int32_t last_origin_x, last_origin_y;       /* the reference room's origin the last window was placed with */
    uint8_t last_window_group, last_window_room;  /* the reference room it was placed in */
    int last_camera_x, last_camera_y;
    int last_offset_x, last_offset_y;
    uint64_t last_window_epoch;
    unsigned map_width;                      /* rooms per overworld row: 14 in Ages, 16 in Seasons (0: 16) */
    unsigned map_height;                     /* rows of the reference group's map under the tileset map rules (set by the view), 0: the grid's 16 */
    unsigned map_left, map_top;              /* its first column and row, 0 but for a map walled inside the grid (set by the view) */
    unsigned band_width;                     /* the world band's width outdoors (0: 256; 213 to 480 by the view's size) */
    int sea;                                 /* the reference room is of Ages' open sea, whose rectangle is `sea_extent` (set by the view) */
    uint8_t sea_extent[4];
    unsigned open_edges;                     /* an interior's open edges, set by the view from its cache */
    int isolated;                            /* the reference room stands alone (set by the view) */
    uint8_t ref_cell;                        /* the reference room's grid cell, sampled in normal play (a dungeon's map position) */
    int have_ref_cell;
    uint8_t ref_cell_group, ref_cell_room;
    /* Scrolls of a large room that came back into the room they left (the
     * game's loops: screenTransitionEyePuzzle, screenTransitionOnoxDungeon's
     * up), in rooms along each axis: the room re-entered is placed beside the
     * room left, as a neighbour would be, for the rest of the epoch. */
    int loop_cols, loop_rows;
    uint64_t loop_epoch;
} OraclesEnhancedObserver;

typedef struct OraclesEnhancedObservation {
    OraclesEnhancedWorld world;              /* Link and the bounds, relative to the reference room */
    int32_t window_left, window_top;         /* world position of the core's game-area window as drawn in the frame on screen */
    int drawn_camera_x, drawn_camera_y;      /* the game's camera (hCameraX/Y) that frame was drawn with, room-local, signed */
    int drawn_offset_x, drawn_offset_y;      /* and its screen offsets (wScreenOffsetX/Y): SCX = camera + offset, SCY = camera + offset - 16 */
    uint8_t group, room;                     /* wActiveGroup/wActiveRoom as the game has them now */
    int in_transition;                       /* not in normal play: scroll mode != 1 or transition state != 2 */
    int in_scroll;                           /* a scrolling transition between two rooms (states 3 to 5): a seam */
    unsigned scroll_direction;               /* its direction, the game's: 0 up, 1 right, 2 down, 3 left */
    int32_t scroll_start_left, scroll_start_top;   /* the game's window at the scroll's first frame */
    int grid;                                /* a small room of groups 0 to 3: on the 16-wide grid */
    int large;                               /* wRoomIsLarge: a 15x11 room (dungeons, wide interiors) */
    int large_grid;                          /* a large room of groups 2 to 5: on the 16-wide grid of 240x176 cells, its neighbours through its doors */
    int playing;                             /* game state 2: the title, the file select and the game over are not */
    int cutscene;                            /* cutscene index above 1: the game may move Link itself */
    int parked_unknown;                      /* Link parked at the game's placeholder, or absent, in a room he has had no real position in yet: the reducers wait for him */
    int cell_pending;                        /* a dungeon room whose map position the game has not set yet: its place on the grid is not known (the view frames) */
    uint64_t epoch;                          /* teleports and domain changes: the camera snaps across epochs, never pans */
} OraclesEnhancedObservation;

void oracles_enhanced_observe(OraclesGuest *guest, OraclesEnhancedObserver *observer, OraclesEnhancedObservation *out);
/* Recomputes the observation's world extent from the observer's current
 * open edges: the view establishes them from its cache once it knows the
 * reference room the observation settled on (at the end of a scroll the
 * reference changes in the same frame), so that the bounds the reducer sees
 * never lag the room they belong to. */
void oracles_enhanced_observation_rebound(const OraclesEnhancedObserver *observer, OraclesEnhancedObservation *out);

typedef struct OraclesEnhancedCamera OraclesEnhancedCamera;

/* An axis's camera in whole pixels: Link's place less `offset`, kept while the
 * reducer's smooth position stays within a pixel of it (camera.c,
 * shown_position), in the reducer's epoch and segment it was taken in. */
typedef struct OraclesEnhancedShownOffset {
    int valid;
    int32_t offset;
    uint64_t epoch, segment;
} OraclesEnhancedShownOffset;

/* The reducers advanced with an observation already made: the horizontal
 * one gives the world x of the surface's left edge and the return value (1
 * tracking, 0 fallback); the vertical one, a second instance of the same
 * reducer fed with the axes swapped, gives the world y of the world band's
 * top line when it tracks, on the ghost's map (the overworlds, groups 0 and
 * 1, where the rooms above and below can be shown), else the game's own
 * window, so that a room of another group keeps the game's vertical framing. */
int oracles_enhanced_camera_reduce(OraclesEnhancedCamera *camera, uint32_t frame,
                                   const OraclesEnhancedObservation *observation, int32_t *camera_x, int32_t *camera_y);
/* 1 when the vertical reducer tracked at the last reduction. */
int oracles_enhanced_camera_vertical_tracking(const OraclesEnhancedCamera *camera);
/* The surface's world band, which the reducers frame outdoors: 256x128 (the
 * default), or that of another of the view's sizes, 213x144 to 480x254 (the
 * drawn-back view); elsewhere they keep 128 lines and the band's width up to
 * 256, centred in the surface's, but for a large room a band wider than 256
 * draws back whole, and a narrower one taller than 128 shows all its lines.
 * Restarts them. */
void oracles_enhanced_camera_set_band(OraclesEnhancedCamera *camera, unsigned width, unsigned height);
/* The part of the band the last reduction framed, centred in it: all of it,
 * but where a band wider than 256 does not draw back (256x128), and its
 * width by 128 lines where a narrower band frames a room off the overworlds,
 * but for a large room. */
void oracles_enhanced_camera_shown(const OraclesEnhancedCamera *camera, unsigned *width, unsigned *height);

/* Attaches to a guest (its tables give the addresses; the game decides the
 * overworld width).  The reducer uses its pinned default configuration. */
OraclesEnhancedCamera *oracles_enhanced_camera_start(OraclesGuest *guest);
void oracles_enhanced_camera_stop(OraclesEnhancedCamera *camera);

/* Reads the guest, advances the reducer once for this frame's ordinal, and
 * returns 1 when the camera is tracking (position written, in pixels), 0 when
 * it is in fallback (a narrow room, a transition, or a lost observation). */
int oracles_enhanced_camera_frame(OraclesEnhancedCamera *camera, uint32_t frame, int32_t *camera_x, int32_t *camera_y);

/* The reducers' serialisable states, for the composite savestate (host
 * state): the horizontal one, and the vertical one.  Restoring takes the
 * horizontal state, and the vertical one or NULL (a state saved before the
 * vertical reducer existed: it restarts). */
const OraclesE11State *oracles_enhanced_camera_state(const OraclesEnhancedCamera *camera);
const OraclesE11State *oracles_enhanced_camera_state_vertical(const OraclesEnhancedCamera *camera);
void oracles_enhanced_camera_set_state(OraclesEnhancedCamera *camera, const OraclesE11State *state, const OraclesE11State *vertical);
/* The whole offsets of the two axes, for the savestate too: set after the
 * states (oracles_enhanced_camera_set_state forgets them), a loaded state
 * shows the camera to the pixel it showed. */
void oracles_enhanced_camera_shown_offsets(const OraclesEnhancedCamera *camera, OraclesEnhancedShownOffset *x, OraclesEnhancedShownOffset *y);
void oracles_enhanced_camera_set_shown_offsets(OraclesEnhancedCamera *camera, const OraclesEnhancedShownOffset *x, const OraclesEnhancedShownOffset *y);
const OraclesE11Config *oracles_enhanced_camera_config(const OraclesEnhancedCamera *camera);
/* 1: the first pinned configuration (dead zone of sixteen pixels); 2: the
 * second (a continuous, gentler follow).  Restarts the reducer. */
void oracles_enhanced_camera_set_profile(OraclesEnhancedCamera *camera, unsigned profile);
unsigned oracles_enhanced_camera_profile(const OraclesEnhancedCamera *camera);

#ifdef __cplusplus
}
#endif

#endif
