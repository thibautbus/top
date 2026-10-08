/* Enhanced view: the wide surface for a running game.
 *
 * Ties the camera, the compositor and the ghost instance to a core and its
 * guest.  Each frame it observes the guest (Link's world position, the
 * game's window, the seam of a scrolling transition), advances the camera,
 * picks the mode (the wide world band in play, the framed fallback for the
 * ring menu, cutscenes, window frames and everything outside play), and
 * composes the surface from the core's committed image and the terrain of
 * the cached rooms around it.  The view reads the register journal of the
 * frame (to place the game's window where the frame was drawn) and never
 * clears it: the launcher or the harness clears it at the frame's end.
 *
 * Neighbours come from the ghost instance: the rooms around the
 * reference room (the four beside it, the two beyond left and right, the
 * diagonals) are loaded by the game itself in the ghost, settled past their
 * scrolling transition, and rendered without objects; the terrain is cached
 * under the bytes of the cache key (`oracles_ghost_key_ranges`) its
 * substitutions read, and invalidated when one of them changes in the live
 * instance.  On the overworlds and Ages' open sea every room of the grid is
 * a neighbour; in an interior only a room the reference room opens onto; a
 * large room (a dungeon) shows the room a scroll enters and nothing else;
 * the map edges are black, never false terrain (view_map.c).  The ghost runs
 * in a worker thread in play, or synchronously with a fixed budget of guest
 * frames per host frame on validation routes, where the composition must be
 * reproducible.
 *
 * The view never writes to the live guest. */
#ifndef ORACLES_ENHANCED_VIEW_H
#define ORACLES_ENHANCED_VIEW_H

#include "camera.h"
#include "compositor.h"
#include "core.h"
#include "guest.h"
#include "animation.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OraclesEnhancedView OraclesEnhancedView;

/* Pure: whether this frame still holds a dialogue's text box.  The game
 * clears wTextIsActive one frame before it restores the room's cells under
 * the box, and the frame on screen still holds it: the view (and what the
 * harness checks) keeps the box while its cells are still in the displayed
 * map, two frames at most, so that it is not shown where the game drew it
 * instead of where the band puts it.  `active` is the game's flag, `cells`
 * the cells of the displayed map that differ from the room's own, and
 * `trail` the count kept between frames (0 to start). */
#define ORACLES_ENHANCED_TEXT_BOX_TRAIL_FRAMES 2u
int oracles_enhanced_text_box_holds(int active, unsigned cells, unsigned *trail);

/* `rom` is copied by the ghost's core; it may be freed afterwards. */
OraclesEnhancedView *oracles_enhanced_view_start(OraclesCore *core, OraclesGuest *guest,
                                                 const uint8_t *rom, size_t rom_size);
void oracles_enhanced_view_stop(OraclesEnhancedView *view);

/* The surface: 256x144, or 480x270 in the drawn-back view (`--zoom-out`, the far 16:9 size),
 * which outdoors shows three rooms across and two down, and keeps
 * the normal band elsewhere, centred; set_size takes the view's other sizes.  Chosen before the first composition. */
void oracles_enhanced_view_set_zoom_out(OraclesEnhancedView *view, int enabled);
/* The surface's size: a level in the screen's shape (compositor.h, oracles_enhanced_view_size); the same moment as
 * set_zoom_out, which is the near or the far 16:9 size.  A size wider than the normal band draws back. */
void oracles_enhanced_view_set_size(OraclesEnhancedView *view, OraclesEnhancedSize size);
uint32_t oracles_enhanced_view_width(const OraclesEnhancedView *view);
uint32_t oracles_enhanced_view_height(const OraclesEnhancedView *view);

/* 0 (default): the ghost runs in a worker thread.  N > 0: the ghost runs
 * synchronously, N guest frames per composed frame (validation routes). */
void oracles_enhanced_view_set_sync_budget(OraclesEnhancedView *view, unsigned frames_per_host_frame);

/* The camera's pinned configuration: 1 (dead zone of sixteen pixels) or 2
 * (a continuous, gentler follow).  Restarts the camera. */
void oracles_enhanced_view_set_camera_profile(OraclesEnhancedView *view, unsigned profile);

/* Draw the objects of the neighbouring rooms, captured frozen where the game
 * creates them: off by default.  The objects placed at random on
 * entry and those another object created are never drawn, their place at the
 * real entry not being the one the ghost found. */
void oracles_enhanced_view_set_neighbour_objects(OraclesEnhancedView *view, int enabled);

/* The hotbar of the item hotkeys, in the gutters of the HUD band.  The launcher hands the view what
 * only it knows, read-only: the slots, their keys, the slot whose request waits, and a count of refusals a slot, whose
 * rise turns the slot's frame red for twelve frames.  The view reads the rest in the game: the button an item sits on,
 * whether it is in the inventory, the inventory's cursor, and the icons, which are the game's own, captured from its
 * status bar.  NULL hides the hotbar; engine/enhanced does not depend on engine/gameplay. */
#define ORACLES_ENHANCED_HOTBAR_SLOTS 4u
typedef struct OraclesEnhancedHotbarSlot {
    uint8_t set, item, variant;      /* variant 0xff: none */
    char key[3];                     /* two characters at most: A, S, LB */
    uint8_t pending;
    unsigned refusals;
} OraclesEnhancedHotbarSlot;
typedef struct OraclesEnhancedHotbar { OraclesEnhancedHotbarSlot slots[ORACLES_ENHANCED_HOTBAR_SLOTS]; } OraclesEnhancedHotbar;
void oracles_enhanced_view_set_hotbar(OraclesEnhancedView *view, const OraclesEnhancedHotbar *hotbar);
/* Icons captured from the game's status bar so far, and slots drawn with an icon at the last composition. */
void oracles_enhanced_view_hotbar_counts(const OraclesEnhancedView *view, unsigned *icons_captured, unsigned *icons_shown);
/* What the harness checks of the hotbar: at each sound vblank the icon of an item sitting on a button is set against the
 * core's own image where the status bar draws it, opaque pixel for opaque pixel, and the surface outside the four
 * slots is hashed before and after they are drawn.  `checked` icons, of which `wrong` differ; `outside` frames where
 * the drawing touched anything else. */
void oracles_enhanced_view_hotbar_verify(OraclesEnhancedView *view, int enabled);
void oracles_enhanced_view_hotbar_proof(const OraclesEnhancedView *view, unsigned *checked, unsigned *wrong, unsigned *outside);
/* Frames that drew the fade of objects appearing on entry, and objects that started one. */
void oracles_enhanced_view_fade_counts(const OraclesEnhancedView *view, unsigned *frames, unsigned *objects);
/* The hand-off of the objects a capture showed, on the data: objects followed,
 * live twins that appeared elsewhere (jumps), frames an object overlapped the
 * game's window before its twin was visible (holes), objects never seen. */
void oracles_enhanced_view_handoff_counts(const OraclesEnhancedView *view, unsigned *shown, unsigned *jumps, unsigned *hole_frames, unsigned *unseen);

/* The wide world, or the framed core for every frame (F3): a switch between
 * the two looks of the profile without a restart. */
void oracles_enhanced_view_toggle(OraclesEnhancedView *view);
int oracles_enhanced_view_framed_only(const OraclesEnhancedView *view);

/* Advances the camera and the neighbours for this frame and returns the
 * composed surface (width*height 0xffRRGGBB).  `mode`, `camera_x` and
 * `camera_y`, when not NULL, receive the mode chosen and the world position
 * of the world band's top-left pixel. */
const uint32_t *oracles_enhanced_view_compose(OraclesEnhancedView *view, OraclesEnhancedMode *mode, int32_t *camera_x, int32_t *camera_y);

/* The frame source signature the host expects (advances one frame). */
const uint32_t *oracles_enhanced_view_frame_source(void *opaque);
/* The host's frame crop (oracles_host_run_config): 1 when the last composition showed the core framed (the game's menus,
 * its map, its cutscenes, or F3), with the core's 160x144 rectangle in the surface. */
int oracles_enhanced_view_framed_crop(void *opaque, uint32_t rect[4]);
/* The last composed surface, without advancing (a screenshot at exit). */
const uint32_t *oracles_enhanced_view_surface(const OraclesEnhancedView *view);
/* The observation of the last composed frame (Link's world position, the window). */
const OraclesEnhancedObservation *oracles_enhanced_view_observation(const OraclesEnhancedView *view);

/* The neighbour of the last composed frame in a direction (the game's: 0 up,
 * 1 right, 2 down, 3 left): whether its terrain was shown, and which room it
 * is: the one drawn (beside a room the game routes itself, the answer for
 * that direction), else the grid's (0xff when there is none on the map). */
void oracles_enhanced_view_neighbour(const OraclesEnhancedView *view, unsigned direction, int *shown, uint8_t *room);
/* Counters over the view's life: ghost runs requested, completed, failed;
 * `reasons`, when not NULL, receives the failures by kind: not primeable,
 * load refused, timeout (never settled), wrong room, other. */
void oracles_enhanced_view_ghost_counts(const OraclesEnhancedView *view, unsigned *requested, unsigned *completed, unsigned *failed);
void oracles_enhanced_view_ghost_failures(const OraclesEnhancedView *view, unsigned reasons[5]);
/* The failed runs, space separated: "from>expected=got" (a wrong room), "from>room?" (never settled), "from>room!reason" (a state not primeable). */
const char *oracles_enhanced_view_failure_log(const OraclesEnhancedView *view);
/* Results dropped because the cache key changed while the ghost ran. */
unsigned oracles_enhanced_view_stale_results(const OraclesEnhancedView *view);
/* Neighbours computed ahead from another neighbour's settled state, and renders of a neighbour with the live tiles. */
unsigned oracles_enhanced_view_chained_results(const OraclesEnhancedView *view);
unsigned oracles_enhanced_view_live_renders(const OraclesEnhancedView *view);
/* The most 8x8 blocks of tiles the live VRAM does not animate that a live render differed from the ghost's render of the same room by: none if the live render is right. */
unsigned oracles_enhanced_view_live_diff_max(const OraclesEnhancedView *view);
/* Results refused as a capture of no terrain (oracles_enhanced_capture_blank): the room run again later. */
unsigned oracles_enhanced_view_plain_renders(const OraclesEnhancedView *view);
/* Results of a room of the live area delivered in another season than the live one (Seasons): not kept. */
unsigned oracles_enhanced_view_season_rejected(const OraclesEnhancedView *view);
const char *oracles_enhanced_view_plain_log(const OraclesEnhancedView *view);   /* their rooms, "group:room" space separated */
/* 1 while the reference room stands alone (the Maku tree's screen and inside). */
int oracles_enhanced_view_isolated(const OraclesEnhancedView *view);
/* World-band pixels of the last composed frame that no source covered (black). */
unsigned oracles_enhanced_view_uncovered(const OraclesEnhancedView *view);
/* The part of the world band the last composition showed, centred in it: all
 * of it, but where the drawn-back view keeps the normal band (256x128). */
void oracles_enhanced_view_shown(const OraclesEnhancedView *view, unsigned *width, unsigned *height);
/* Frames shown framed because the game drew its area elsewhere than its camera (a cutscene's pan or flash). */
unsigned oracles_enhanced_view_off_camera_frames(const OraclesEnhancedView *view);
/* Frames shown in the band with the game's per-line scroll (under water, the strange force). */
unsigned oracles_enhanced_view_wave_frames(const OraclesEnhancedView *view);
/* World frames where the band shows an overworld room beside the reference room, diagonals included, that no delivered terrain covers yet. */
unsigned oracles_enhanced_view_waiting_frames(const OraclesEnhancedView *view);
/* Rooms delivered for a direction of a room the game routes itself. */
unsigned oracles_enhanced_view_routed_delivered(const OraclesEnhancedView *view);
/* The order in which the four directions of a room the game routes itself are
 * asked of the ghost: the two sides before up and down, since a
 * room of one screen shows its sides in the band at rest and its rows above
 * and below only as a strip when Link stands off its middle; in each pair the
 * side Link is nearer first; and a place off the map, which the camera never
 * shows at rest, after every place on it.  `gap` is Link's distance to each
 * edge of the room and `on_map` whether the place that way is on the map,
 * both indexed by direction (0 up, 1 right, 2 down, 3 left, the game's). */
void oracles_enhanced_routed_order(const int32_t gap[4], const int on_map[4], unsigned out[4]);
/* The rooms beyond an answer of a room the game routes itself, read through
 * that answer (the Lost Woods, finishes of the drawn-back view): when the room
 * the game loads leaving `ref` in direction `dir` is the grid's neighbour
 * there and is not a room the game routes itself (`answer_routed` 0), the
 * game goes on from it by the grid, and the rooms it loads going on are
 * those of the grid: the room beyond it that way (out[0]), the two beside it
 * across that way (out[1], out[2]: up then down for a side, left then right
 * for up or down), the two beside the room beyond (out[3], out[4]).  `have`
 * says which are on the map (`map_width` columns, `map_height` rows); returns
 * how many, 0 when the answer is not such a room. */
/* Whether an answer of a room the game routes itself still holds: its
 * routine's routing key (`key_len` bytes, 0 when the routine has no account)
 * as the live game has it now, against the one it was asked under; during a
 * transition (`taking` the direction taken, -1 none) the answer of that
 * direction holds all the same, the room being entered, asked from the state
 * before the transition moved the key.  An answer without a key holds until
 * the next transition asks it again (ev_drop_routed_entries). */
int oracles_enhanced_routed_answer_holds(unsigned answer_dir, int taking, const uint8_t *asked_key, const uint8_t *live_key, size_t key_len);
unsigned oracles_enhanced_routed_beyond(uint8_t ref, unsigned dir, uint8_t answer, int answer_routed, unsigned map_width, unsigned map_height,
                                        uint8_t out[5], int have[5]);
/* The order in which the drawn-back band outdoors asks the ghost
 * for the rooms around Link: `count` rooms given in the normal band's order,
 * `shown` the pixels of each the band showed at the last composition and
 * `distance` the square of its middle's distance to Link; `out` receives
 * their indices, the most shown first, then the nearest, then the normal
 * order. */
void oracles_enhanced_band_order(const int32_t *shown, const int32_t *distance, unsigned count, unsigned *out);
/* World frames where a neighbour drawn in the band holds a fade's palettes (oracles_enhanced_capture_blank): never, if its guard holds. */
unsigned oracles_enhanced_view_plain_shown_frames(const OraclesEnhancedView *view);
/* Whether a ghost's capture of a room is no terrain: its background palettes
 * the palette thread's fade rather than the room's own (the offset most
 * channels agree on, against the base palettes w2TilesetBgPalettes, more
 * than 2 apart, the tolerance of a smooth palette transition, from the
 * offset the game keeps on the room, `kept_offset`: a dark room's -16,
 * wPaletteThread_parameter; palettes equal to the base are no fade), or,
 * given its area, one colour over nine tenths of it (the LCD off). */
int oracles_enhanced_capture_blank(const uint8_t bg_palettes[64], const uint8_t base_palettes[64], int kept_offset, const uint32_t *area, unsigned pixels);
/* The objects of a large room beyond the game's window: sprites drawn
 * outside the window over the run, and compositions in a large room whose OAM
 * on screen matched none of the game's drawings kept. */
/* Cached rooms whose captured objects were dropped because the enemies counted killed in them changed. */
unsigned oracles_enhanced_view_objects_outdated(const OraclesEnhancedView *view);
/* Rooms run again (a season change, a killed enemy) whose old terrain was dropped after two failed runs. */
unsigned oracles_enhanced_view_refresh_given_up(const OraclesEnhancedView *view);
/* Parents run again from the live state before a room was chained from them: an enemy of that room killed since their run. */
unsigned oracles_enhanced_view_parents_run_for_kills(const OraclesEnhancedView *view);
void oracles_enhanced_view_large_object_counts(const OraclesEnhancedView *view, unsigned *sprites_drawn, unsigned *frames_unmatched);
/* Copies a neighbour's own animation wrote into its tiles; `other`: into a neighbour whose tileset or animation is not the live room's;
 * `in_step_tiles`: tiles such a neighbour took live at a render, the live room showing on them an image of its own animation. */
void oracles_enhanced_view_animation_counts(const OraclesEnhancedView *view, unsigned *own, unsigned *other, unsigned *in_step_tiles);
/* The tile animation during a scroll, which the game freezes and resumes
 * where it stopped: the view runs it alone in whole loops, one at least, that
 * land on the step the game resumes from (neighbours/animation.h,
 * view_scroll.c).  What it did, over the run: */
typedef struct OraclesEnhancedScrollCounts {
    unsigned scrolls;          /* scrolls the view animated */
    unsigned scrolls_still;    /* scrolls left frozen: into another animation or tileset, which the game starts again, or off the map */
    unsigned landed;           /* of the scrolls animated, those whose counter ran out, where the view lands on the game's step */
    unsigned landed_off;       /* of those, the ones landed on another frame than the plan's: their streams took a jump at the landing, or stood waiting */
    unsigned gaps, gap_max;    /* scrolls whose last frozen frame had the view's step apart from the game's, and by how many frames at most */
    unsigned tiles_off;        /* tiles the view's streams wrote that differed from the live VRAM at the landing, summed */
    unsigned tiles_given_back; /* tiles the view's streams wrote that the game wrote itself during the scroll (the room entered's graphics): the game's again */
    unsigned streams_run;      /* streams of the scrolls landed that ran (a stream outside its loop stays still and is not counted) */
    unsigned pace_min, pace_median, pace_max;   /* the pace of the streams run, in percent of the game's */
    unsigned window_pixels;    /* pixels of the game's window drawn with the view's tiles */
} OraclesEnhancedScrollCounts;
void oracles_enhanced_view_scroll_animation_counts(const OraclesEnhancedView *view, OraclesEnhancedScrollCounts *out);
/* Neighbour streams matched with a live stream of the same loop, and the frames one was moved to its live twin's step. */
void oracles_enhanced_view_stream_counts(const OraclesEnhancedView *view, unsigned *matched, unsigned *followed);
/* Whether the area blurb was taken out of the window and drawn apart this frame (its pixels are repainted). */
int oracles_enhanced_view_blurb_drawn(const OraclesEnhancedView *view);
/* Pre-runs started while a room loaded: results filed beside the room the ghost primed in, and results dropped. */
void oracles_enhanced_view_blind_runs(const OraclesEnhancedView *view, unsigned *filed, unsigned *dropped);
/* Neighbours run again from the live state because the key had moved since they were computed (the source of a chain). */
unsigned oracles_enhanced_view_refreshed_parents(const OraclesEnhancedView *view);
/* Cached terrains dropped because a key byte they read changed: the count, and "address:drops" pairs into `out`. */
unsigned oracles_enhanced_view_drop_log(const OraclesEnhancedView *view, char *out, size_t capacity);
/* The times a byte of the coarse list (oracles_ghost_key_coarse_ranges) changed and threw the whole cache. */
unsigned oracles_enhanced_view_coarse_drops(const OraclesEnhancedView *view);
/* The black the band shows, from its first frame without any
 * (enhanced.black_pixels_mean in the harness, the same count at the end of
 * a session in the launcher): the band's uncovered pixels in world frames,
 * and the rooms of the overworld's grid (not an interior, a dungeon or the
 * zone the game routes itself) shown by a pixel at least with nothing drawn
 * at their place, for more than ORACLES_ENHANCED_BLACK_ROOM_FRAMES world
 * frames in a row. */
#define ORACLES_ENHANCED_BLACK_ROOM_FRAMES 120u
#define ORACLES_ENHANCED_BLACK_ROOMS_KEPT 8u
typedef struct OraclesEnhancedBlackRoom { uint8_t group, room; uint32_t from, frames; } OraclesEnhancedBlackRoom;
typedef struct OraclesEnhancedBlack {
    int have_world, have_full;
    uint32_t first_world, first_full;     /* the frames (composition ordinals, the route's frame numbers) */
    unsigned full_world_frames, max;      /* world frames from the first full one, and the most black in one */
    uint64_t pixels;                      /* black pixels over them */
    unsigned rooms_over;                  /* rooms black for more than the threshold, a run each */
    OraclesEnhancedBlackRoom longest[ORACLES_ENHANCED_BLACK_ROOMS_KEPT];   /* the longest of them, longest first */
} OraclesEnhancedBlack;
void oracles_enhanced_view_black(const OraclesEnhancedView *view, OraclesEnhancedBlack *out);
/* Whether a neighbour's settled state may start a run of the room whose
 * own flags sit at `own` in the key, the live state unable to prime the
 * ghost (a cutscene): its key the live one but for the flags of other rooms
 * (the room flags' pages at `flags_at` in the key, `flags_length` long). */
int oracles_enhanced_parent_key_holds(const uint8_t *parent_key, const uint8_t *live_key, size_t length, long flags_at, size_t flags_length, long own);
/* Whether the `count` bytes a run read, at `addresses`, as it saw them
 * (`seen`) are the live ones: the visited bit (`visited`) of the room's own
 * flags (`own_flags`) aside, which the entry into it sets and no load reads.
 * A room run from beside is kept only if they are. */
int oracles_enhanced_reads_hold(unsigned count, const uint16_t *addresses, const uint8_t *seen, const uint8_t *live, uint16_t own_flags, uint8_t visited);
/* Results run from a room beside while a cutscene held the game, refused because a byte they read had changed. */
unsigned oracles_enhanced_view_beside_refused(const OraclesEnhancedView *view);
/* Runs started from the live state across the sea's surface (Ages), ahead of a dive or a return to the surface. */
unsigned oracles_enhanced_view_sea_runs(const OraclesEnhancedView *view);
/* The committed layout (176 bytes) behind a shown neighbour in a direction; 0 when none is shown, 2 when the
 * neighbour is shown in its old season while the ghost runs it again after a season change (deliberately: less abrupt than black). */
int oracles_enhanced_view_neighbour_layout(const OraclesEnhancedView *view, unsigned direction, uint8_t *out);

/* Host state for the composite savestate: the camera reducer's wire state.
 * After a load the observer and the neighbours rebuild from the guest. */
int oracles_enhanced_view_save_state(const OraclesEnhancedView *view, uint8_t *out, size_t capacity, size_t *written);
int oracles_enhanced_view_load_state(OraclesEnhancedView *view, const uint8_t *data, size_t size);
/* 0 when a host state was taken at this view's surface size; -1 and the
 * reason in `why` when at the other (a state of the drawn-back view in the
 * normal one, or the reverse), which the launcher refuses whole. */
int oracles_enhanced_view_check_state(const OraclesEnhancedView *view, const uint8_t *data, size_t size, char *why, size_t capacity);

OraclesEnhancedCamera *oracles_enhanced_view_camera(OraclesEnhancedView *view);

#ifdef __cplusplus
}
#endif

#endif
