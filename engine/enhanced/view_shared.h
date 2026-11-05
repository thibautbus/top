/* The functions the Enhanced view's files share, by file (view_internal.h, which includes this file at its end after
 * the view's state). */
#ifndef ORACLES_ENHANCED_VIEW_SHARED_H
#define ORACLES_ENHANCED_VIEW_SHARED_H

#include "view_internal.h"

int ev_tracked_read(const OraclesEnhancedView *v, uint16_t a);
uint8_t ev_live_byte(OraclesEnhancedView *v, uint16_t a);
size_t ev_read_key(OraclesEnhancedView *v, uint8_t *out);
long ev_key_offset(const OraclesEnhancedView *v, uint16_t a);
void ev_drop_entry(entry *e);
void ev_wait_for_start(OraclesEnhancedView *v, entry *e, const entry *parent);
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
int ev_room_whole(const OraclesEnhancedView *v);
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
int ev_entry_key_current(const OraclesEnhancedView *v, const entry *e, const uint8_t *key, size_t key_len, uint8_t group, uint8_t room, int *visits);
int ev_key_byte_exempt(const OraclesEnhancedView *v, long i, const uint8_t *snapshot, const uint8_t *live, uint8_t group, uint8_t room);
int ev_entry_drawable(const OraclesEnhancedView *v, const entry *e);
void ev_capture_source_terrain(OraclesEnhancedView *v);
/* The OAM of the live frame on screen with only the sprites of the room's own
 * objects (kinds 1 to 3, not persistent), by the live tags, which it keeps in
 * `kept`; returns 0 when the tags of that OAM are unknown or leave nothing. */
int ev_live_room_objects_oam(OraclesEnhancedView *v, uint8_t out[160], OraclesSpriteTag kept[ORACLES_SPRITE_TAGS], unsigned *kept_count);
/* Whether OAM entry `index` on screen belongs to an object of a room (kinds 1
 * to 3, not persistent): during a transition those are drawn beyond the
 * window by the captures and the image of the room left, not by the overlay. */
int ev_live_entry_is_room_object(OraclesEnhancedView *v, unsigned index);
/* view_objects.c: the events of the live instance, the fade of what appears on
 * entry, the image of the room left and the fade of what it showed. */
void ev_objects_event(OraclesEnhancedView *v, const OraclesGuestEvent *event);
void ev_fade_appearing(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y);
void ev_keep_left_image(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y);
void ev_fade_vanishing(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y);
int ev_source_is_scroll_source(const OraclesEnhancedView *v);
void ev_track_animated_tiles(OraclesEnhancedView *v);
/* Every frame the game animates: each cached neighbour's own animation, one step. */
void ev_advance_neighbour_animations(OraclesEnhancedView *v);
/* The live room's animation handed to its entry as it becomes a neighbour. */
void ev_hand_live_animation(OraclesEnhancedView *v, entry *e);
void ev_track_fade(OraclesEnhancedView *v);
/* A scene's tint on the live room's base palettes, after ev_track_fade. */
void ev_track_scene_tint(OraclesEnhancedView *v);
/* view_poses.c: a delivery's objects that animate, the live updates that step them, a step per update at each
 * composition, every pose back to its capture at a transition's start; the OAM a neighbour is drawn with, as captured
 * and with its poses (returns 0 when it shows no object); a render that drew it, a frame that showed it; the harness's
 * check of a render of some lines. */
void ev_poses_deliver(OraclesEnhancedView *v, entry *e);
void ev_poses_event(OraclesEnhancedView *v, const OraclesGuestEvent *event);
void ev_poses_advance(OraclesEnhancedView *v);
void ev_poses_reset(OraclesEnhancedView *v, entry *entered);
int ev_neighbour_oam(OraclesEnhancedView *v, entry *e, uint8_t captured[160], uint8_t posed[160]);
void ev_poses_drawn(OraclesEnhancedView *v, entry *e, const uint8_t posed[160], int moved);
void ev_poses_shown(OraclesEnhancedView *v, entry *e, int drawing);
void ev_poses_check_lines(OraclesEnhancedView *v, entry *e, const OraclesPpuInput *in, unsigned width);
/* view_scroll.c: a scroll's animation run by the view, once a composition after the live tiles are tracked; the tiles and
 * the mask of animated tiles the band draws the live tileset with; the game's window and the captures drawn with them. */
void ev_scroll_animation(OraclesEnhancedView *v);
/* A state loaded: no scroll under way, and the clock as when the view started. */
void ev_scroll_reset(OraclesEnhancedView *v);
const uint8_t *ev_shown_vram(OraclesEnhancedView *v, unsigned bank);
const uint8_t *ev_shown_animated(const OraclesEnhancedView *v);
const uint32_t *ev_scroll_window(OraclesEnhancedView *v);
const uint32_t *ev_sway_window(OraclesEnhancedView *v, const uint32_t *image);
/* The scroll across the band shows on the game area's line `line`, the sway taken off. */
static inline int32_t ev_wave_shown(const OraclesEnhancedView *v, unsigned line)
{
    return (v->wave_held && v->shown_shift ? v->shown_shift[line] : v->line_shift[line]) - v->sway;
}
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
void ev_lane_serve(OraclesEnhancedView *v, unsigned lane);
void ev_entry_used_tiles(entry *e);
void ev_render_large_room(OraclesEnhancedView *v);
void ev_overlay_edge_sprites(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y);
void ev_large_objects_event(OraclesEnhancedView *v, const OraclesGuestEvent *event);
void ev_overlay_large_objects(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y);
void ev_blurb_event(OraclesEnhancedView *v, const OraclesGuestEvent *event);
void ev_find_area_blurb(OraclesEnhancedView *v);
void ev_overlay_area_blurb(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y);
/* The columns of its map the game has not redrawn yet after a warp, over the whole band. */
void ev_overlay_warp_curtain(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y);
/* A dialogue's text box, drawn again at the middle of the band (view_textbox.c): the core's, or the host's wide box. */
void ev_overlay_text_box(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y);
void ev_phase_observe(OraclesEnhancedView *v);
void ev_hold_camera(OraclesEnhancedView *v, int32_t *camera_x, int32_t *camera_y);
void ev_hold_note(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y);
void ev_subpixel_layer(OraclesEnhancedView *v, int tracking, int32_t camera_x, int32_t camera_y);
/* A rectangle of the band, in the band's pixels, the sub-pixel camera shows unmoved over the band and Link: what the
 * view draws there stands on the screen, not in the world. */
void ev_subpixel_fixed(OraclesEnhancedView *v, int32_t x, int32_t y, int32_t w, int32_t h);
/* Whether an OAM entry stands at Link's place, drawn from his own graphics: his left column (0) or his right one (1),
 * else -1 (view_overlay.c). */
int ev_link_column(const OraclesEnhancedView *v, const uint8_t entry[4]);
/* A pixel of his the edge overlay draws without the OAM at (x, y) of the band, over `under`: kept for the layer. */
void ev_subpixel_made(OraclesEnhancedView *v, int32_t x, int32_t y, uint32_t under, uint32_t colour);
/* Once every drawing is in the band: his pixels the edge overlay made join the layer, those another drawing covered go
 * back to the band. */
void ev_subpixel_settle(OraclesEnhancedView *v);
void ev_overlay_link_phase(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y);
/* The host's wide box over the framed core, at the game's box's place, centred on it (view_textbox.c). */
void ev_overlay_text_box_framed(OraclesEnhancedView *v);
/* The box that stays above all in the world band, the black outside the part shown included: the host's, or the game's
 * twice as large (view_textbox.c). */
void ev_overlay_text_box_above(OraclesEnhancedView *v);
/* view_hotbar.c: the hotbar of the item hotkeys, drawn last in the HUD band's gutters, in every mode of the surface, or
 * against B where the bar is laid out. */
void ev_draw_hotbar(OraclesEnhancedView *v);
void ev_hotbar_event(OraclesEnhancedView *v, const OraclesGuestEvent *event);
/* view_hud.c: the status bar as the game keeps it (0 where the profile binds no map of it), and the gutters beside it
 * this frame (OraclesEnhancedCompose.hud_fill), counted when `counted`; NULL: the border. */
int ev_status_bar(OraclesEnhancedView *v, OraclesEnhancedBar *bar);
int ev_status_bar_shown(OraclesEnhancedView *v);
const uint32_t *ev_hud_fill(OraclesEnhancedView *v, OraclesEnhancedMode mode, int counted);
/* The status bar's blocks laid out this frame (OraclesEnhancedCompose.hud_place, v->hud_laid), counted when `counted`;
 * NULL: centred. */
const OraclesEnhancedHudPlace *ev_hud_place(OraclesEnhancedView *v, OraclesEnhancedMode mode, int counted);
/* view_mode.c: the frame's mode, and the ripple the band follows (v->have_wave, v->line_shift). */
OraclesEnhancedMode ev_choose_mode(OraclesEnhancedView *v, int tracking);
/* A room's load composed under the world's HUD band, the game area alone at the place the camera gives it or the world
 * last showed it: sets the compose's descriptor for it and returns 1; 0 for another frame, which it leaves as it is. */
int ev_load_band(OraclesEnhancedView *v, OraclesEnhancedCompose *in, int tracking, int32_t camera_x, int32_t camera_y);
/* view_black.c: the black of the composed frame counted, once a frame. */
void ev_measure_black(OraclesEnhancedView *v, OraclesEnhancedMode mode, int32_t camera_x, int32_t camera_y);
/* The rooms never visited: whether a room of the reference's group is kept dark, the reference room and the active one
 * never; a composition's sources begun, the veil of a room drawn (256 kept dark, less while it fades in, 0 shown), the
 * places of the grid in view that no source covers and that are kept dark added, and the composition's count closed;
 * the reference room's visit, once a composition; what a savestate's load forgets. */
int ev_room_hidden(OraclesEnhancedView *v, uint8_t group, uint8_t room);
void ev_hidden_begin(OraclesEnhancedView *v);
unsigned ev_room_veil(OraclesEnhancedView *v, uint8_t room);
unsigned ev_room_edge_veil(OraclesEnhancedView *v, uint8_t room);
unsigned ev_add_hidden_places(OraclesEnhancedView *v, OraclesEnhancedNeighbour *nbs, unsigned count, unsigned capacity,
                              int32_t camera_x, int32_t camera_y, int32_t reach);
void ev_hidden_end(OraclesEnhancedView *v);
void ev_check_reference_visited(OraclesEnhancedView *v);
void ev_hidden_forget(OraclesEnhancedView *v);
/* Whether the part of the band shown, the band's top-left corner at (band_left,
 * band_top), meets a rectangle of the world grown by `reach` each way. */
int ev_shown_meets(const OraclesEnhancedView *v, int32_t band_left, int32_t band_top, int32_t left, int32_t top, int32_t width, int32_t height, int32_t reach);
/* The last composition showed more than the normal band: the drawn-back view outdoors, at sea, in a large room. */
static inline int ev_drawn_back(const OraclesEnhancedView *v) { return v->shown_width > ORACLES_ENHANCED_NARROW_WIDTH; }
/* The surface's pixel at band row `y`, column `x`. */
static inline uint32_t *ev_band_pixel(OraclesEnhancedView *v, unsigned x, unsigned y) { return &v->surface[(ORACLES_ENHANCED_HUD_HEIGHT + y) * v->size.width + x]; }

#endif
