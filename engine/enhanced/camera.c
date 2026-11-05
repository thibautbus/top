#include "camera.h"

#include "guest_struct_offsets.h"

#include <stdlib.h>
#include <string.h>

#define SMALL_ROOM_W 160   /* SMALL_ROOM_WIDTH * 16 (constants/common/other.s) */
#define SMALL_ROOM_H 128   /* SMALL_ROOM_HEIGHT * 16 */
#define LARGE_ROOM_W 240   /* LARGE_ROOM_WIDTH * 16 */
#define LARGE_ROOM_H 176   /* LARGE_ROOM_HEIGHT * 16 */
#define NARROW_BAND_W 256  /* the reducer's pinned viewport, the normal band's width */
#define NARROW_BAND_H 128  /* the normal band's height, the core's game area's */
#define GAME_WINDOW_W 160  /* the core's game area's width, its window on the room */
#define NORMAL_SCROLL_MODE 0x01u
#define TRANSITION_IDLE 0x02u
#define BTN_RIGHT 0x10u   /* the game's own bits, ORACLES_GAME_DIRECTIONS */
#define BTN_LEFT 0x20u
#define BTN_UP 0x40u
#define BTN_DOWN 0x80u
#define GAME_STATE_LOADING_ROOM 0x01u
#define GAME_STATE_PLAYING 0x02u
#define LINK_STATE_WARPING 0x0au   /* constants/common/linkStates.s */
#define LINK_PARKED_COORDINATE 0xf8u   /* the placeholder the game parks Link at while a save loads or a cutscene takes him over (both coordinates) */
#define DIR_UP 0u
#define DIR_RIGHT 1u
#define DIR_DOWN 2u
#define DIR_LEFT 3u

int oracles_enhanced_signed_coordinate(unsigned raw, int in_scroll, unsigned direction, int axis, int small_room)
{
    const int negative_scroll = in_scroll && ((axis == 0 && direction == DIR_LEFT) || (axis == 1 && direction == DIR_UP));
    const int positive_scroll = in_scroll && ((axis == 0 && direction == DIR_RIGHT) || (axis == 1 && direction == DIR_DOWN));
    /* The game itself takes him at most 10 px past the edge; the continuous
     * transitions up to 25 on foot, and swimming at the mermaid
     * suit's speed past 30 (x 193 measured rightward, 0xc1): anything in the
     * upper half is past the edge he leaves by, and a scroll rightward or
     * downward never takes him below zero.  Otherwise, in a small room the
     * byte never reaches 0xc0 in play (a large room's x runs to 240): past it,
     * it is negative in any state. */
    if (negative_scroll && raw >= 0x80u) return (int)raw - 256;
    if (small_room && positive_scroll) return (int)raw;
    if (small_room && raw >= 0xc0u) return (int)raw - 256;
    /* A large room is 240 by 176: past 0xf8 across or 0xe0 down, negative
     * too, except during a rightward scroll, where the game takes him up to
     * ten pixels past the right edge (240 to 250, 0xf0 to 0xfa). */
    if (!small_room && axis == 0 && in_scroll && direction == DIR_RIGHT) return (int)raw;
    if (!small_room && raw >= (axis == 0 ? 0xf8u : 0xe0u)) return (int)raw - 256;
    return (int)raw;
}

static OraclesE11Domain domain_of_group(uint8_t group)
{
    switch (group & 7u) {
    case 0: return ORACLES_E11_EXTERIOR;    /* present overworld */
    case 1: return ORACLES_E11_ERA;         /* past overworld */
    case 2:
    case 3: return ORACLES_E11_INTERIOR;
    case 4:
    case 5: return ORACLES_E11_DUNGEON;
    default: return ORACLES_E11_UNDERGROUND;/* side-scrolling areas */
    }
}

/* Outdoors the reducer frames the whole band, 480 wide in the drawn-back
 * view; elsewhere its pinned 256, the drawn-back view keeping the normal
 * band there, centred in its own (but at sea, oracles_enhanced_world_extend_to_sea,
 * and in an interior laid out as a room off the map, ORACLES_ENHANCED_INDOORS). */
static int wide_domain(OraclesE11Domain domain) { return domain == ORACLES_E11_EXTERIOR || domain == ORACLES_E11_ERA; }
static int drawn_back(const OraclesEnhancedWorld *world) { return world->viewport > NARROW_BAND_W; }
/* The viewport elsewhere than outdoors: the pinned 256, or the band's width when the band is narrower (near, 4:3). */
static int narrow_viewport(unsigned band_width) { return band_width && band_width < NARROW_BAND_W ? (int)band_width : NARROW_BAND_W; }

int oracles_enhanced_world_from_room(uint8_t group, uint8_t room, int room_is_large,
                                     int link_x, int link_y, unsigned overworld_stride, unsigned map_width, unsigned map_height,
                                     unsigned map_left, unsigned map_top, unsigned open_edges, int isolated, uint8_t cell, unsigned band_width, OraclesEnhancedWorld *out)
{
    if (!out) return 0;
    memset(out, 0, sizeof *out);
    /* A room off its overworld group's map keeps the group's domain: the epoch the reducer follows is made of it, and
     * a scroll between a room of the map and one off it, or the view learning of it a frame late, would lower the
     * epoch and leave the camera behind (the reducer ignores a lower one). */
    out->domain = domain_of_group(group);
    const int viewport = (wide_domain(out->domain) || isolated == ORACLES_ENHANCED_INDOORS) && band_width ? (int)band_width : narrow_viewport(band_width);
    out->viewport = viewport;
    (void)room;
    const unsigned col = cell & 0x0fu;
    const unsigned row = cell >> 4u;
    if (room_is_large && (group & 7u) >= 2u) {
        /* A large room of a dungeon (or of a large interior, or a
         * sidescrolling area, groups 6 and 7, the dungeons' side views): on
         * the 16-wide grid too, 240x176 a cell, its neighbours the rooms it
         * opens onto through its doors (the view establishes them), the
         * extent widened to the viewport, the room's own centred viewport
         * always inside. */
        const int left = (open_edges >> 3) & 1u, right = (open_edges >> 1) & 1u, up = open_edges & 1u, down = (open_edges >> 2) & 1u;
        out->origin_x = (int32_t)(col * LARGE_ROOM_W);
        out->origin_y = (int32_t)(row * LARGE_ROOM_H);
        const int gutter = (viewport - LARGE_ROOM_W) / 2;   /* the left one; the right one takes an odd width's pixel */
        int32_t origin = out->origin_x - gutter, right_edge = out->origin_x + viewport - gutter;
        if (left && out->origin_x - LARGE_ROOM_W < origin) origin = out->origin_x - LARGE_ROOM_W;
        if (right && out->origin_x + 2 * LARGE_ROOM_W > right_edge) right_edge = out->origin_x + 2 * LARGE_ROOM_W;
        out->bounds_origin_x = origin;
        out->bounds_width = right_edge - origin;
        out->bounds_origin_y = out->origin_y - (up ? LARGE_ROOM_H : 0);
        out->bounds_height = (1 + up + down) * LARGE_ROOM_H;
        if (band_width > NARROW_BAND_W) out->viewport = (int32_t)band_width;   /* the whole room in the whole band (camera.c's reduction) */
    } else if (!room_is_large && (group & 7u) <= 3u && (isolated || (group & 7u) >= 2u)) {
        /* An interior: on the 16-wide grid too, but adjacent indices are
         * unrelated rooms unless the room opens onto them.  Its extent is the
         * room and the rooms it opens onto, widened to the viewport with
         * black gutters (a one-room house stands centred, still). */
        const int left = (open_edges >> 3) & 1u, right = (open_edges >> 1) & 1u, up = open_edges & 1u, down = (open_edges >> 2) & 1u;
        out->origin_x = (int32_t)(col * SMALL_ROOM_W);
        out->origin_y = (int32_t)(row * SMALL_ROOM_H);
        /* The room's own centred viewport is always part of the extent: a
         * connected room landing in the cache widens the bounds without ever
         * putting the camera outside them (the reducer would desynchronise
         * and snap), at the price of the gutter it could show on that side. */
        const int gutter = (viewport - SMALL_ROOM_W) / 2;   /* the left one; the right one takes an odd width's pixel */
        int32_t origin = out->origin_x - gutter, right_edge = out->origin_x + viewport - gutter;
        if (left && out->origin_x - SMALL_ROOM_W < origin) origin = out->origin_x - SMALL_ROOM_W;
        if (right && out->origin_x + 2 * SMALL_ROOM_W > right_edge) right_edge = out->origin_x + 2 * SMALL_ROOM_W;
        out->bounds_origin_x = origin;
        out->bounds_width = right_edge - origin;
        out->bounds_origin_y = out->origin_y - (up ? SMALL_ROOM_H : 0);
        out->bounds_height = (1 + up + down) * SMALL_ROOM_H;
        out->alone = 1;
    } else if (!room_is_large && (group & 7u) <= 3u) {
        /* The small-room groups (the two overworlds and their interiors) index
         * their rooms on a 16-wide grid; the camera pans across it, and a
         * scrolling transition between two rooms is a seam, not a jump. */
        const unsigned stride = overworld_stride ? overworld_stride : 16u;
        const unsigned width = map_width ? map_width : stride;   /* the camera stops at the map's edges, not the grid's */
        const unsigned height = map_height ? map_height : stride;
        const unsigned left = map_left < width ? map_left : 0u, top = map_top < height ? map_top : 0u;
        out->bounds_origin_x = (int32_t)(left * SMALL_ROOM_W);
        out->bounds_origin_y = (int32_t)(top * SMALL_ROOM_H);
        out->bounds_width = (int32_t)((width - left) * SMALL_ROOM_W);
        out->bounds_height = (int32_t)((height - top) * SMALL_ROOM_H);
        out->origin_x = (int32_t)(col * SMALL_ROOM_W);
        out->origin_y = (int32_t)(row * SMALL_ROOM_H);
    } else {
        /* A single large room (dungeons, side-scrolling areas): its own
         * extent, widened to the viewport with black gutters so the reducer
         * can still track Link inside it. */
        const int width = 15 * 16, height = 11 * 16;
        const int gutter = width < viewport ? (viewport - width) / 2 : 0;
        out->bounds_origin_x = -gutter;
        out->bounds_origin_y = 0;
        out->bounds_width = width < viewport ? viewport : width;
        out->bounds_height = height;
        out->origin_x = 0;
        out->origin_y = 0;
    }
    out->link_x = out->origin_x + link_x;
    out->link_y = out->origin_y + link_y;
    out->wide = out->bounds_width >= viewport;
    return 1;
}

void oracles_enhanced_world_extend_to_sea(OraclesEnhancedWorld *w, const uint8_t sea[4], unsigned band_width)
{
    if (band_width <= NARROW_BAND_W) return;
    w->bounds_origin_x = (int32_t)sea[0] * SMALL_ROOM_W;
    w->bounds_origin_y = (int32_t)sea[1] * SMALL_ROOM_H;
    w->bounds_width = (int32_t)sea[2] * SMALL_ROOM_W;
    w->bounds_height = (int32_t)sea[3] * SMALL_ROOM_H;
    w->viewport = (int32_t)band_width;
    w->wide = w->bounds_width >= w->viewport;
    w->alone = 0;   /* a map of rooms, whose edge the band stops at */
}

/* A large room's place moved by the loops the epoch went through. */
static void shift_by_loops(const OraclesEnhancedObserver *o, OraclesEnhancedWorld *w)
{
    if (!o->ref_large || (o->ref_group & 7u) < 2u || o->loop_epoch != o->epoch) return;
    const int32_t dx = o->loop_cols * LARGE_ROOM_W, dy = o->loop_rows * LARGE_ROOM_H;
    w->origin_x += dx; w->bounds_origin_x += dx; w->link_x += dx;
    w->origin_y += dy; w->bounds_origin_y += dy; w->link_y += dy;
}

/* Normal play, or a room being loaded outside a scroll with Link placed in
 * it (his state no longer the warp's): the reference is the room the game
 * has.  A change that did not come through a scrolling transition is a
 * teleport.  A scroll of a large room that came back into the room it left
 * (the game's loops) moves the room's place on: the last window stands where
 * it was in the world, the game's camera jumps back by the room's size. */
static void update_reference(OraclesGuest *guest, const OraclesGuestTables *t, OraclesEnhancedObserver *o, const OraclesEnhancedObservation *out, uint8_t is_large)
{
    if ((out->group != o->last_group || out->room != o->last_room) && !o->scrolling) o->epoch++;
    if (o->loop_epoch != o->epoch) { o->loop_epoch = o->epoch; o->loop_cols = 0; o->loop_rows = 0; }
    if (o->scrolling && is_large && (out->group & 7u) >= 2u && out->group == o->last_group && out->room == o->last_room) {
        switch (oracles_guest_read8(guest, t->screen_transition_direction) & 3u) {
        case 0: o->loop_rows--; o->last_origin_y -= LARGE_ROOM_H; break;
        case 1: o->loop_cols++; o->last_origin_x += LARGE_ROOM_W; break;
        case 2: o->loop_rows++; o->last_origin_y += LARGE_ROOM_H; break;
        default: o->loop_cols--; o->last_origin_x -= LARGE_ROOM_W; break;
        }
    }
    o->ref_group = out->group; o->ref_room = out->room; o->ref_large = is_large;
    o->last_group = out->group; o->last_room = out->room;
    o->scrolling = 0;
}

/* The reference room's grid cell: its index, or in a dungeon its map
 * position, read while the game's room is the reference (during a scroll
 * the game already holds the destination's).  A dungeon's position is taken
 * as soon as the floor's layout names the room at it (w2DungeonLayout, the
 * game's own lookup): after a warp the room byte is set a few frames before
 * the position, and the room's index stood in for the cell meanwhile, two
 * rows off. */
static void sample_reference_cell(OraclesGuest *guest, const OraclesGuestTables *t, OraclesEnhancedObserver *o, const OraclesEnhancedObservation *out, uint8_t is_large)
{
    if (out->group != o->ref_group || out->room != o->ref_room) return;
    const int in_dungeon = is_large && oracles_guest_read8(guest, t->dungeon_index) != 0xffu;
    if (in_dungeon) {
        const uint8_t pos = oracles_guest_read8(guest, t->dungeon_map_position), floor = oracles_guest_read8(guest, t->dungeon_floor);
        const OraclesGuestSym at = { t->dungeon_layout.bank, (uint16_t)(t->dungeon_layout.addr + (floor & 3u) * 0x40u + (pos & 0x3fu)) };
        if (oracles_guest_read8(guest, at) == out->room) {
            o->ref_cell = (uint8_t)(((pos >> 3) & 7u) << 4 | (pos & 7u)); o->have_ref_cell = 1; o->ref_cell_group = o->ref_group; o->ref_cell_room = o->ref_room;
        }
    } else if (!out->in_transition) {
        o->ref_cell = o->ref_room; o->have_ref_cell = 1; o->ref_cell_group = o->ref_group; o->ref_cell_room = o->ref_room;
    }
}

/* The core's game-area window: the game's own camera inside the reference
 * room, signed 16 bits (it runs negative during a leftward or upward
 * scroll).  The frame on screen was drawn with the camera of the previous
 * frame's logic (copied into the display registers at the vblank that
 * started it), so the window of the previous observation is the one the
 * pixels show; the current one is kept for the next frame.  A new epoch (a
 * teleport) has no previous window that applies.  The same reference room
 * placed elsewhere since (its map position read after a warp): the window
 * moves with it, the frame drawn is the same room. */
static void place_window(OraclesGuest *guest, const OraclesGuestTables *t, OraclesEnhancedObserver *o, OraclesEnhancedObservation *out)
{
    const int camera_x = (int16_t)oracles_guest_read16(guest, t->camera_x);
    const int camera_y = (int16_t)oracles_guest_read16(guest, t->camera_y);
    const int offset_x = oracles_guest_read8(guest, t->screen_offset_x), offset_y = oracles_guest_read8(guest, t->screen_offset_y);
    const int32_t window_left = out->world.origin_x + camera_x, window_top = out->world.origin_y + camera_y;
    if (out->in_scroll && !o->was_in_scroll) { o->scroll_start_left = window_left; o->scroll_start_top = window_top; }
    o->was_in_scroll = out->in_scroll;
    out->scroll_start_left = o->scroll_start_left;
    out->scroll_start_top = o->scroll_start_top;
    if (o->have_last_window && o->last_window_epoch == out->epoch) {
        const int same_room = o->last_window_group == o->ref_group && o->last_window_room == o->ref_room;
        const int32_t shift_x = same_room ? out->world.origin_x - o->last_origin_x : 0, shift_y = same_room ? out->world.origin_y - o->last_origin_y : 0;
        out->window_left = o->last_window_left + shift_x; out->window_top = o->last_window_top + shift_y;
        out->drawn_camera_x = o->last_camera_x; out->drawn_camera_y = o->last_camera_y;
        out->drawn_offset_x = o->last_offset_x; out->drawn_offset_y = o->last_offset_y;
    } else {
        out->window_left = window_left; out->window_top = window_top;
        out->drawn_camera_x = camera_x; out->drawn_camera_y = camera_y;
        out->drawn_offset_x = offset_x; out->drawn_offset_y = offset_y;
    }
    /* The frame on screen drawn with the camera of the observation before (each axis): an OAM left uncopied, built
     * a logic frame earlier, stands where that camera put it. */
    const int before = o->have_last_window && o->last_window_epoch == out->epoch && o->have_before_camera;
    o->drawn_camera_still_x = before && o->before_camera_x == out->drawn_camera_x;
    o->drawn_camera_still_y = before && o->before_camera_y == out->drawn_camera_y;
    o->have_before_camera = o->have_last_window && o->last_window_epoch == out->epoch && o->last_window_group == o->ref_group && o->last_window_room == o->ref_room;
    o->before_camera_x = o->last_camera_x; o->before_camera_y = o->last_camera_y;
    o->have_last_window = 1;
    o->last_window_left = window_left; o->last_window_top = window_top;
    o->last_origin_x = out->world.origin_x; o->last_origin_y = out->world.origin_y;
    o->last_window_group = o->ref_group; o->last_window_room = o->ref_room;
    o->last_camera_x = camera_x; o->last_camera_y = camera_y;
    o->last_offset_x = offset_x; o->last_offset_y = offset_y;
    o->last_window_epoch = out->epoch;
}

void oracles_enhanced_observe(OraclesGuest *guest, OraclesEnhancedObserver *o, OraclesEnhancedObservation *out)
{
    const OraclesGuestTables *t = oracles_guest_tables(guest);
    memset(out, 0, sizeof *out);
    out->group = oracles_guest_read8(guest, t->active_group);
    out->room = oracles_guest_read8(guest, t->active_room);
    /* The room's size is the group's: initializeRoomBoundaryAndLoadAnimations
     * sets wRoomIsLarge from wActiveGroup & 4, at the transition's first
     * state, so the byte lags the room change; the group does not. */
    const uint8_t is_large = (out->group & 4u) != 0;
    /* Bit 7 of wScrollMode is set by updateCameraPosition on every frame the
     * game's camera moves a pixel inside a large room (and cleared the
     * next): a camera step, not a transition; the frame is normal play. */
    const uint8_t scroll_mode = oracles_guest_read8(guest, t->scroll_mode) & 0x7fu;
    const uint8_t transition_state = oracles_guest_read8(guest, t->screen_transition_state);
    out->playing = oracles_guest_read8(guest, t->game_state) == GAME_STATE_PLAYING;
    out->loading_room = oracles_guest_read8(guest, t->game_state) == GAME_STATE_LOADING_ROOM;
    out->cutscene = oracles_guest_read8(guest, t->cutscene_index) > 1u;
    out->in_transition = !(scroll_mode == NORMAL_SCROLL_MODE && transition_state == TRANSITION_IDLE);
    out->keys = oracles_guest_read8(guest, t->keys_pressed);
    out->in_scroll = (transition_state >= 3u && transition_state <= 5u) || scroll_mode == ORACLES_SCROLL_MODE_DECIDED || scroll_mode == ORACLES_SCROLL_MODE_TRANSITION;
    /* A scroll decided at the very frame the last one ended, with no frame of normal play between (the continuous
     * transitions at the line start the scroll into the room beside there): that frame is the room entered's, as the
     * frame of normal play that ends a scroll is, the next one the new scroll's first. */
    if (scroll_mode == ORACLES_SCROLL_MODE_DECIDED && transition_state == 3u && o->scrolling) { out->in_transition = out->in_scroll = 0; out->scroll_chained = 1; }
    out->grid = (out->group & 7u) <= 3u && !is_large;
    out->large = is_large != 0;
    out->large_grid = is_large != 0 && (out->group & 7u) >= 2u;   /* dungeons and their sidescrolling areas (groups 4 to 7) */
    if (scroll_mode == ORACLES_SCROLL_MODE_TRANSITION) o->scrolling = 1;

    if (!o->have_reference) {
        o->have_reference = 1;
        o->ref_group = out->group; o->ref_room = out->room; o->ref_large = is_large;
        o->last_group = out->group; o->last_room = out->room;
        if (o->epoch == 0) o->epoch = 1;   /* a restored observer keeps its epoch */
    }
    const uint8_t *link = oracles_guest_object(guest, 0, 0);   /* w1Link at $d000 */
    /* Warping, parked at the game's placeholder (LINK_PARKED_COORDINATE:
     * a save loading, a room's cutscene taking him over), or absent from
     * play, his object cleared by a cutscene (disableLcdAndLoadRoom clears
     * WRAM bank 1 for the rooms Nayru's song shows, the time portal disables
     * him): his last real position holds, not the corner the cleared object
     * says.  Out of play (a file loading) the object is the game's to set. */
    const int parked = link && link[ORACLES_OBJ_XH] == LINK_PARKED_COORDINATE && link[ORACLES_OBJ_YH] == LINK_PARKED_COORDINATE;
    const int absent = out->playing && link && link[ORACLES_OBJ_ENABLED] == 0;
    out->link_warping = link && link[ORACLES_OBJ_STATE] == LINK_STATE_WARPING;
    const int warping = out->link_warping || parked || absent;
    if (o->have_reference && (!out->in_transition || (!out->in_scroll && !warping))) update_reference(guest, t, o, out, is_large);
    /* Link's coordinates, signed by the scroll in progress (a large room's
     * rightward scroll runs its x past 240 without wrapping). */
    const unsigned direction = oracles_guest_read8(guest, t->screen_transition_direction) & 3u;
    out->scroll_direction = direction;
    const unsigned raw_x = link ? link[ORACLES_OBJ_XH] : 0u, raw_y = link ? link[ORACLES_OBJ_YH] : 0u;
    int link_x = oracles_enhanced_signed_coordinate(raw_x, out->in_scroll, direction, 0, !is_large);
    int link_y = oracles_enhanced_signed_coordinate(raw_y, out->in_scroll, direction, 1, !is_large);
    /* While Link warps (LINK_STATE_WARPING, linkStates.s) the game parks him
     * at a placeholder position: the observation holds his last real one, and
     * the teleport that follows opens a new epoch. */
    if (warping) {
        /* His last real position in this room; in a room he was parked
         * into or is absent from (a cutscene's entry, a save loading), the
         * room's centre for the world, and no observation for the reducers,
         * which bootstrap where he is placed instead of panning there from
         * the centre (the view frames meanwhile). */
        if (o->have_held && o->held_group == o->ref_group && o->held_room == o->ref_room) { link_x = o->held_x; link_y = o->held_y; }
        else { link_x = SMALL_ROOM_W / 2; link_y = SMALL_ROOM_H / 2; out->parked_unknown = 1; }
    } else { o->have_held = 1; o->held_x = link_x; o->held_y = link_y; o->held_group = o->ref_group; o->held_room = o->ref_room; }
    sample_reference_cell(guest, t, o, out, is_large);
    const int have_cell = o->have_ref_cell && o->ref_cell_group == o->ref_group && o->ref_cell_room == o->ref_room;
    const uint8_t cell = have_cell ? o->ref_cell : o->ref_room;
    out->cell_pending = !have_cell && o->ref_large && oracles_guest_read8(guest, t->dungeon_index) != 0xffu;
    oracles_enhanced_world_from_room(o->ref_group, o->ref_room, o->ref_large != 0, link_x, link_y, 16u, o->map_width, o->map_height, o->map_left, o->map_top, o->open_edges, o->isolated, cell, o->band_width, &out->world);
    if (o->sea) oracles_enhanced_world_extend_to_sea(&out->world, o->sea_extent, o->band_width);
    shift_by_loops(o, &out->world);
    out->epoch = o->epoch * 8u + (uint64_t)out->world.domain;
    place_window(guest, t, o, out);
    /* The carries across a transition's axis the transaction applied before this scroll (--continuous-diagonal). */
    const unsigned carried = oracles_guest_transition_carried(guest);
    if (!out->in_scroll) o->carried_before_scroll = carried;
    /* In the same room by his room's coordinates; across the end of a scroll,
     * where the reference changes, by the world's, continuous there. */
    if (o->have_last_link && o->last_link_epoch == out->epoch) {
        const int same_room = o->last_link_group == o->ref_group && o->last_link_room == o->ref_room;
        out->link_step_x = same_room ? link_x - o->last_link_x : out->world.link_x - o->last_world_link_x;
        out->link_step_y = same_room ? link_y - o->last_link_y : out->world.link_y - o->last_world_link_y;
        /* Across the axis of a scroll the transaction carried Link across, at drawAllSprites' return: the frame on
         * screen draws him where the OAM it scanned had him, which a frame whose logic ended past the vblank left
         * uncopied (oracles_guest_link_shown), not always where the previous observation had him: the whole pixels
         * his sprites lag that place by, which the sub-pixel camera's layer of him makes up.  Along the scroll's axis
         * too, where the game's camera has not moved since that OAM was built (the scroll's first frames, the room
         * still loading), where he would step back a pixel against his walk for a frame; with the sub-pixel camera alone,
         * whose layer makes it up: without it the band in whole pixels would follow his sprites back and on, where
         * they stay the game's own. */
        uint16_t shown_x = 0, shown_y = 0;
        if (out->in_scroll && !warping && oracles_guest_link_shown(guest, &shown_x, &shown_y)) {
            const int carries = carried != o->carried_before_scroll;
            const int step_x = (int8_t)(uint8_t)(raw_x - (shown_x >> 8)), step_y = (int8_t)(uint8_t)(raw_y - (shown_y >> 8));
            const int across_x = !(direction & 1u), lag_x = step_x - out->link_step_x, lag_y = step_y - out->link_step_y;
            if (across_x && carries) { out->drawn_lag_x = lag_x; out->link_step_x = step_x; }
            else if (!across_x && o->exact && o->drawn_camera_still_x && lag_x >= -2 && lag_x <= 2) { out->drawn_lag_x = lag_x; out->link_step_x = step_x; }
            if (!across_x && carries) { out->drawn_lag_y = lag_y; out->link_step_y = step_y; }
            else if (across_x && o->exact && o->drawn_camera_still_y && lag_y >= -2 && lag_y <= 2) { out->drawn_lag_y = lag_y; out->link_step_y = step_y; }
        }
    }
    o->have_last_link = 1;
    o->last_link_x = link_x; o->last_link_y = link_y;
    out->drawn_sub_x = o->last_sub_x; out->drawn_sub_y = o->last_sub_y;
    o->last_sub_x = link && !warping ? link[ORACLES_OBJ_X] : 0; o->last_sub_y = link && !warping ? link[ORACLES_OBJ_Y] : 0;
    out->link_sub_x = o->last_sub_x; out->link_sub_y = o->last_sub_y;
    o->last_world_link_x = out->world.link_x; o->last_world_link_y = out->world.link_y;
    o->last_link_group = o->ref_group; o->last_link_room = o->ref_room;
    o->last_link_epoch = out->epoch;
}

void oracles_enhanced_observation_rebound(const OraclesEnhancedObserver *o, OraclesEnhancedObservation *out)
{
    const int lx = out->world.link_x - out->world.origin_x, ly = out->world.link_y - out->world.origin_y;
    const uint8_t cell = o->have_ref_cell && o->ref_cell_group == o->ref_group && o->ref_cell_room == o->ref_room ? o->ref_cell : o->ref_room;
    oracles_enhanced_world_from_room(o->ref_group, o->ref_room, o->ref_large != 0, lx, ly, 16u, o->map_width, o->map_height, o->map_left, o->map_top, o->open_edges, o->isolated, cell, o->band_width, &out->world);
    if (o->sea) oracles_enhanced_world_extend_to_sea(&out->world, o->sea_extent, o->band_width);
    shift_by_loops(o, &out->world);
}

struct OraclesEnhancedCamera {
    OraclesGuest *guest;
    unsigned profile;
    OraclesE11Config config;             /* the pinned configuration of the profile, the reducers' but outdoors */
    OraclesE11Config wide;               /* outdoors: the same, its viewport the band's width and framed at its middle */
    unsigned band_w, band_h;             /* the surface's world band, its width and oracles_enhanced_band_height */
    int shown_wide;                      /* the last reduction framed the band's whole width */
    int shown_height;                    /* and that many of its lines */
    int restored;                        /* states just restored (a savestate): the next reduction takes their viewport as it finds it */
    OraclesE11State state;               /* the horizontal reducer */
    OraclesE11State state_y;             /* the vertical one: the same reducer, the axes swapped */
    int vertical_tracking;
    OraclesEnhancedObserver observer;
    unsigned overworld_stride;
    char session[ORACLES_E11_ID_CAP];
    char session_y[ORACLES_E11_ID_CAP];
    OraclesEnhancedShownOffset shown_x, shown_y;   /* Link's place from the camera in whole pixels, each axis (shown_correction) */
    unsigned held_x, held_y;                       /* frames a correction of one axis alone has waited (PAIRED_HOLD_FRAMES) */
    int exact;                                     /* the reducers take Link's place to the 256th of a pixel (the sub-pixel camera) */
    int fed_exact;                                 /* and took it so at the last reduction */
    int32_t limits[4];                             /* the band's reach toward black (oracles_enhanced_camera_limit), for one reduction */
    int32_t fraction_x, fraction_y;                /* the last reduction's: the camera that many 256ths of a pixel past its whole one */
    int large_have, large_in_transition;           /* a large room's last reduction, and whether it was in a transition */
    int32_t large_ease_x, large_ease_y;            /* its fractions' part still easing toward Link's (LARGE_EASE a frame) */
};

/* The vertical reducer sees the world turned on its side: Link's y as its x,
 * the map's height as its width.  Its viewport is pinned at 256 px while the
 * world band is 128 lines tall, so the band is the middle half of that
 * viewport: the band's top is the reducer's position plus 64, and the bounds
 * are widened by 64 px each way so that the band, not the viewport, stops at
 * the map's edge.  Framing at 128 puts Link at the band's middle line.  (The
 * drawn-back view outdoors: a viewport of 480, a band of 254, a margin of 113;
 * in its 4:3 shape, a band of 344, a margin of 68.) */
/* The reducers frame the band's whole width where the viewport is as wide as it (outdoors, a large room drawn back,
 * and every place in a band no wider than 256), else the normal 256 centred in it (the interiors of a wider band);
 * its whole height outdoors and where a large room is drawn back, else the 128 lines of a room (an interior, a band
 * of the normal height). */
static int whole_band(const OraclesEnhancedCamera *c, const OraclesEnhancedWorld *world) { return world->viewport >= (int32_t)c->band_w; }
static int band_px(const OraclesEnhancedCamera *c, const OraclesEnhancedWorld *world)
{
    return whole_band(c, world) && (wide_domain(world->domain) || drawn_back(world)) ? (int)c->band_h : NARROW_BAND_H;
}
static int vertical_margin(const OraclesEnhancedCamera *c, const OraclesEnhancedWorld *world) { return (world->viewport - band_px(c, world)) / 2; }

/* The configuration outdoors: the pinned one, its viewport widened to the band's. */
static void set_wide_config(OraclesEnhancedCamera *c)
{
    c->wide = c->config;
    c->wide.viewport_width = (int32_t)c->band_w * ORACLES_E11_F256;
    c->wide.framing = (int32_t)c->band_w * ORACLES_E11_F256 / 2;   /* the middle, an odd width's included (213) */
}
static const OraclesE11Config *config_for(const OraclesEnhancedCamera *c, const OraclesEnhancedWorld *world) { return whole_band(c, world) ? &c->wide : &c->config; }

OraclesEnhancedCamera *oracles_enhanced_camera_start(OraclesGuest *guest)
{
    OraclesEnhancedCamera *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->guest = guest;
    c->profile = 1;
    oracles_e11_config_default(&c->config);
    c->band_w = NARROW_BAND_W; c->band_h = NARROW_BAND_H;
    set_wide_config(c);
    oracles_e11_state_initial(&c->state, &c->config);
    oracles_e11_state_initial(&c->state_y, &c->config);
    c->overworld_stride = 16u;   /* the addressing stride of the room byte; the real map is narrower */
    c->limits[0] = c->limits[2] = INT32_MIN; c->limits[1] = c->limits[3] = INT32_MAX;
    const OraclesCompatProfile *profile = oracles_guest_profile(guest);
    const int seasons = oracles_compat_family(profile) == ORACLES_GAME_SEASONS;
    c->observer.map_width = oracles_compat_map_width(profile);
    snprintf(c->session, sizeof c->session, "%s", seasons ? "seasons" : "ages");
    snprintf(c->session_y, sizeof c->session_y, "%s-y", seasons ? "seasons" : "ages");
    return c;
}

void oracles_enhanced_camera_stop(OraclesEnhancedCamera *c) { free(c); }

static void reducer_observation(const OraclesEnhancedCamera *c, uint32_t frame, const OraclesEnhancedObservation *observation, int vertical, OraclesE11Observation *obs)
{
    const OraclesEnhancedWorld *world = &observation->world;
    oracles_e11_observation_initial(obs);
    snprintf(obs->session, sizeof obs->session, "%s", vertical ? c->session_y : c->session);
    snprintf(obs->snapshot_identity, sizeof obs->snapshot_identity, "f%u", frame);
    /* A teleport or a domain change opens a new epoch: the camera snaps, it does not pan. */
    obs->epoch = observation->epoch;
    obs->ordinal = frame;
    obs->tick = frame;
    obs->domain = world->domain;
    obs->event = ORACLES_E11_NORMAL_VBLANK;
    /* With the sub-pixel camera, his place to the 256th of a pixel: the reducers' speed is then his own, not the
     * whole pixels' 1, 1, 0, and a frame without a step a stop, unless the player holds the pad along the axis or
     * a transition goes on: the game then holds him a frame (a scroll's start or end, a swim's stroke), or a wall
     * does. */
    if (oracles_e11_config_settles(&c->config)) {
        obs->subpixel = c->exact != 0;
        obs->held = observation->in_transition || (observation->keys & (vertical ? BTN_UP | BTN_DOWN : BTN_RIGHT | BTN_LEFT)) != 0;
    }
    const int32_t sub_x = c->exact ? observation->link_sub_x : 0, sub_y = c->exact ? observation->link_sub_y : 0;
    if (!vertical) {
        obs->link_x = world->link_x * ORACLES_E11_F256 + sub_x;
        obs->link_y = world->link_y * ORACLES_E11_F256 + sub_y;
        obs->bounds_origin_x = world->bounds_origin_x * ORACLES_E11_F256;
        obs->bounds_origin_y = world->bounds_origin_y * ORACLES_E11_F256;
        obs->bounds_width = (int64_t)world->bounds_width * ORACLES_E11_F256;
        obs->bounds_height = (int64_t)world->bounds_height * ORACLES_E11_F256;
        obs->availability = world->wide && !observation->parked_unknown ? ORACLES_E11_AVAILABLE : ORACLES_E11_UNAVAILABLE;
    } else {
        obs->link_x = world->link_y * ORACLES_E11_F256 + sub_y;
        obs->link_y = world->link_x * ORACLES_E11_F256 + sub_x;
        /* A room shown alone keeps its own centred band inside its extent, as
         * its width keeps its centred viewport (world_from_room): 128 lines
         * in the drawn-back view's 254, the extent is widened to it, and a
         * scroll that opens an edge widens it further without leaving the
         * band where it stood, so that the reducer tracks the room open or
         * closed and the band does not jump.  A band taller than two rooms
         * (344 lines, the far view in 4:3) would leave the room a scroll
         * enters a range of places that does not meet the one of the room it
         * leaves, and the band would jump at its end: the rooms the room opens
         * onto keep their own centred bands in the extent too.  In the normal
         * band a room fills the band's height already. */
        int32_t top = world->bounds_origin_y, height = world->bounds_height;
        const int32_t band = band_px(c, world);
        if (world->alone && band > SMALL_ROOM_H) {
            const int32_t margin = (band - SMALL_ROOM_H) / 2, bottom = top + height;
            int32_t first = world->origin_y - margin, last = first + band;
            if (band > 2 * SMALL_ROOM_H) { first = top - margin; last = bottom - SMALL_ROOM_H - margin + band; }
            if (first < top) top = first;
            height = (bottom > last ? bottom : last) - top;
        }
        obs->bounds_origin_x = (top - vertical_margin(c, world)) * ORACLES_E11_F256;
        obs->bounds_origin_y = world->bounds_origin_x * ORACLES_E11_F256;
        /* The margins add up to the viewport less the band, an odd width's included (213 near in 4:3), where twice
         * the halved margin would leave the bounds a pixel short of the viewport. */
        obs->bounds_width = (int64_t)(height + world->viewport - band) * ORACLES_E11_F256;
        obs->bounds_height = (int64_t)world->bounds_width * ORACLES_E11_F256;
        /* The rooms above and below come from the ghost, which runs the
         * grid rooms (the overworlds, the interiors): elsewhere the vertical
         * stays the game's window. */
        const int on_map = (observation->grid && (world->domain == ORACLES_E11_EXTERIOR || world->domain == ORACLES_E11_ERA || world->domain == ORACLES_E11_INTERIOR))
                        || observation->large_grid;
        obs->availability = world->wide && on_map && !observation->parked_unknown ? ORACLES_E11_AVAILABLE : ORACLES_E11_UNAVAILABLE;
    }
}

/* The pinned reducer, once desynchronised by a change of domain (a house, a
 * dungeon, the other era), takes only observations of the domain it was
 * tracking: it would stay in its fallback for the whole visit, the view
 * framed.  A new epoch in another domain is a teleport, at which the camera
 * snaps anyway: the reducer restarts there and bootstraps in the new domain. */
static void restart_on_domain_change(OraclesE11State *state, const OraclesE11Observation *obs, const OraclesE11Config *config)
{
    if (state->status != ORACLES_E11_UNINITIALIZED && obs->epoch > state->epoch && obs->domain != state->domain)
        oracles_e11_state_initial(state, config);
}

/* The follow that settles kept from showing black: the observation's bounds narrowed to the band's reach
 * (oracles_enhanced_camera_limit), in the reducer's place (`offset` whole pixels from the band's edge), never short of
 * where the camera stands or of where its braking ends, nor of Link 32 px inside the band. */
static void limit_bounds(const OraclesE11State *state, const OraclesE11Config *config, int32_t least, int32_t most, int32_t offset, OraclesE11Observation *obs)
{
    if (!oracles_e11_config_settles(config) || (least == INT32_MIN && most == INT32_MAX)) return;
    const int64_t low = obs->bounds_origin_x, high = low + obs->bounds_width - config->viewport_width;
    /* No further than that reach, or than Link framed without the look-ahead when he leads it there. */
    const int64_t framed = (int64_t)obs->link_x - config->framing;
    int64_t lo = least == INT32_MIN ? low : ((int64_t)least + offset) * ORACLES_E11_F256;
    int64_t hi = most == INT32_MAX ? high : ((int64_t)most + offset) * ORACLES_E11_F256;
    if (least != INT32_MIN && lo > framed) lo = framed;
    if (most != INT32_MAX && hi < framed) hi = framed;
    if (state->status == ORACLES_E11_TRACKING && state->epoch == obs->epoch) {
        int64_t braking = 0;
        if (!oracles_e11_braking_offset(state->applied_velocity, config, &braking)) braking = 0;   /* at its own rate, the slowest it sheds */
        const int64_t pos = state->camera_pos, stop = pos + braking;
        if (lo > pos) lo = pos;
        if (lo > stop) lo = stop;
        if (hi < pos) hi = pos;
        if (hi < stop) hi = stop;
    }
    const int64_t margin = 32 * ORACLES_E11_F256;
    if (lo > obs->link_x - margin) lo = obs->link_x - margin;
    if (hi < obs->link_x - config->viewport_width + margin) hi = obs->link_x - config->viewport_width + margin;
    if (lo < low) lo = low;
    if (hi > high) hi = high;
    if (hi < lo || lo < INT32_MIN || lo > INT32_MAX) return;
    obs->bounds_origin_x = (int32_t)lo;
    obs->bounds_width = hi - lo + config->viewport_width;
}

/* x / 256 rounded to the nearest, a half up, negative values included. */
static int32_t nearest_pixel(int64_t x)
{
    const int64_t shifted = x + ORACLES_E11_F256 / 2;
    return (int32_t)(shifted >= 0 ? shifted / ORACLES_E11_F256 : -((-shifted + ORACLES_E11_F256 - 1) / ORACLES_E11_F256));
}

/* The whole camera `shown` (in 256ths of a pixel) moved back by `pixels` stays within the bounds. */
static int within(int64_t shown, int32_t pixels, int64_t low, int64_t high)
{
    const int64_t after = shown - (int64_t)pixels * ORACLES_E11_F256;
    return after >= low && after <= high;
}

/* The camera in whole pixels on one axis: the whole pixels its offset moves by
 * this frame (the reducers apply them).  The reducer's position is smooth,
 * to the 256th of a pixel, and Link's comes in whole pixels.  Cut to the
 * pixel on its own, the camera would step on frames of its own, which at a
 * speed under a pixel a frame (a diagonal walk, about 0.7 a frame each way)
 * are not Link's: he would shake a pixel back and forth on the screen, on one
 * axis then the other.  The camera is Link's place less a whole offset, kept
 * while the smooth one stays within a pixel of it, then moved by the whole
 * pixels the smooth one has gained or lost: the band steps with Link, both
 * axes on his frames, and he moves on the screen only as the camera gains or
 * loses a pixel on him.  Within a pixel of the reducer's position, it stays
 * inside the bounds that position does (both are whole pixels).  A new epoch
 * or segment (a snap) takes the offset afresh, the smooth one rounded.
 * `link` is Link's place as the frame on screen draws him, the previous
 * observation's (link_step): the game builds its sprites in the logic and the
 * vblank copies them, so the frame shows him a step behind his coordinates.
 * Stepped with his coordinates, the camera would move a frame before his
 * sprite and he would go a pixel back and forth on the screen at every
 * irregular step (on stairs, at half a pixel a frame, every frame). */
static int32_t shown_correction(OraclesEnhancedShownOffset *s, const OraclesE11State *state, const OraclesE11Observation *obs,
                                const OraclesE11Config *config, int32_t link, int whole)
{
    const int64_t smooth = (int64_t)link * ORACLES_E11_F256 - state->camera_pos;
    if (!s->valid || s->epoch != state->epoch || s->segment != state->segment) {
        s->valid = 1; s->epoch = state->epoch; s->segment = state->segment;
        s->offset = nearest_pixel(smooth);
        s->turn = 0;
        s->last_link = INT32_MIN;
    }
    const int64_t gap = smooth - (int64_t)s->offset * ORACLES_E11_F256;
    const int stepped = link != s->last_link && s->last_link != INT32_MIN;   /* not known: a state just loaded */
    const int leap = stepped && (link - s->last_link >= 3 || s->last_link - link >= 3);   /* his sprite put 3 px or more away (a pose) */
    if (stepped) s->turn = link > s->last_link ? 1 : -1;
    s->last_link = link;
    /* With the camera in whole pixels, Link's whole steps and the camera's turn over on frames of their own, and
     * his sprite lags his place now and then: the offset moving against the way he last stepped would set him going
     * back a pixel on the screen.  Such a move waits for the smooth offset to be two pixels away on a frame he
     * steps, three on one he does not, and goes a pixel short of it; a move the way he goes does not wait.  Three
     * or four pixels, the most a wait leaves with his step, come as two, so that he never jolts across the screen
     * (more is the world put elsewhere under him, the woods' loop, followed at once, as is his sprite put three
     * pixels or more away, a pose, which a camera brought short would leave past its own), and no wait leaves the
     * camera past the reducer's bounds, the edge of the room or of the black a neighbour still being made would
     * show (limit_bounds).  A re-centre, the camera alone going to him, it follows as it goes. */
    int32_t whole_pixels = (int32_t)(gap / ORACLES_E11_F256);
    if (whole && oracles_e11_config_settles(config) && state->follow_recenter != ORACLES_E11_RECENTER) {
        const int64_t low = obs->bounds_origin_x, high = low + obs->bounds_width - config->viewport_width;
        const int back = s->turn ? (gap > 0) != (s->turn > 0) : !stepped;
        if (back) {
            const int64_t wait = stepped ? 2 * ORACLES_E11_F256 : 3 * ORACLES_E11_F256;
            const int32_t waited = (gap < 0 ? -gap : gap) < wait ? 0
                : (int32_t)((gap < 0 ? gap + (wait - ORACLES_E11_F256) : gap - (wait - ORACLES_E11_F256)) / ORACLES_E11_F256);
            if (within(state->camera_pos + gap, waited, low, high)) whole_pixels = waited;
        }
        if (state->status == ORACLES_E11_TRACKING && !leap && (whole_pixels == 3 || whole_pixels == 4 || whole_pixels == -3 || whole_pixels == -4) &&
            within(state->camera_pos + gap, whole_pixels > 0 ? 2 : -2, low, high))
            whole_pixels = whole_pixels > 0 ? 2 : -2;
    }
    return whole_pixels;   /* toward zero: under a pixel left */
}

/* The camera to the 256th of a pixel on one axis: the reducer's own smooth position, which the whole camera keeps
 * within a pixel of (shown_correction), less that whole camera, in 256ths of a pixel (-255 to 255).  The view moves
 * the band by it at the screen's resolution. */
static int32_t shown_fraction(const OraclesE11State *state, int32_t shown)
{
    const int64_t fraction = (int64_t)state->camera_pos - (int64_t)shown * ORACLES_E11_F256;
    return fraction > -ORACLES_E11_F256 && fraction < ORACLES_E11_F256 ? (int32_t)fraction : 0;
}

#define LARGE_EASE 16   /* 256ths of a pixel a frame: a large room's fraction taken up or given back (the view's SUB_EASE) */

/* Link drawn in phase on a diagonal (the view's drawn_link_dy): a pixel the camera gains or loses on one axis alone
 * waits up to two frames for the other's, so that the band does not step across the walk on its own. */
#define PAIRED_HOLD_FRAMES 2

int oracles_enhanced_camera_reduce(OraclesEnhancedCamera *c, uint32_t frame, const OraclesEnhancedObservation *observation, int32_t *camera_x, int32_t *camera_y)
{
    /* The band a reducer frames stands centred in the surface's world band:
     * the whole of it outdoors, else the normal band's 256x128 (the same in
     * the normal surface).  The camera is the world place of the surface's. */
    const OraclesEnhancedWorld *here = &observation->world;
    const OraclesE11Config *config = config_for(c, here);
    const int32_t inset_x = ((int32_t)c->band_w - here->viewport) / 2;
    const int32_t inset_y = ((int32_t)c->band_h - band_px(c, here)) / 2, window_inset = ((int32_t)c->band_h - NARROW_BAND_H) / 2;
    /* The sea and a house under water share a domain but not a viewport (a
     * warp between them, a new epoch): the reducers restart for the other one.
     * Not across a savestate's load: the states restored are the place's. */
    if (c->restored) { c->shown_wide = whole_band(c, here); c->restored = 0; }
    if (whole_band(c, here) != c->shown_wide) {
        oracles_e11_state_initial(&c->state, config);
        oracles_e11_state_initial(&c->state_y, config);
    }
    c->shown_wide = whole_band(c, here);
    c->shown_height = band_px(c, here);
    if (observation->large_grid) {
        /* A large room (a dungeon) keeps the game's own framing, the room
         * extended: the band's lines are the game's window's (its camera
         * follows Link inside the room), the room is centred across, and a
         * scroll into the next room slides the band with the game's window,
         * the 160 px it scrolls stretched to the 240 px between the two
         * rooms' centres; the reducers are not consulted.  The drawn-back
         * view shows the whole room, centred both ways in the band, still
         * while the game's window follows Link inside it; a vertical scroll
         * slides it too, the 128 lines stretched to the 176 between the two
         * rooms' middles. */
        const OraclesEnhancedWorld *world = &observation->world;
        int32_t cx = world->origin_x - ((int32_t)c->band_w - LARGE_ROOM_W) / 2;
        if (observation->in_scroll && (observation->scroll_direction & 1u))
            cx += (observation->window_left - observation->scroll_start_left) * LARGE_ROOM_W / 160;
        /* A band narrower than the room (near, 4:3) follows the game's window across, inside the room: the window's
         * 80 px of travel take the band's 27, its left edge on the room's at one end, its right edge on the room's at
         * the other, so that the band never shows past the room while the window has room to show.  A scroll slides
         * it from the place it had in the room left to the one it takes in the room entered, the band's width over
         * the window's 160 px. */
        if (c->band_w < LARGE_ROOM_W) {
            const int32_t slack = LARGE_ROOM_W - (int32_t)c->band_w, travel = LARGE_ROOM_W - GAME_WINDOW_W;
            const int across = observation->in_scroll && (observation->scroll_direction & 1u);
            const int32_t left = across ? observation->scroll_start_left : observation->window_left;
            cx = world->origin_x + (left - world->origin_x) * slack / travel;
            if (across) cx += (observation->window_left - observation->scroll_start_left) * (int32_t)c->band_w / GAME_WINDOW_W;
        }
        int32_t cy = observation->window_top - window_inset;
        if (drawn_back(world)) {
            cy = world->origin_y - ((int32_t)c->band_h - LARGE_ROOM_H) / 2;
            if (observation->in_scroll && !(observation->scroll_direction & 1u))
                cy += (observation->window_top - observation->scroll_start_top) * LARGE_ROOM_H / (int32_t)NARROW_BAND_H;
        } else if (c->band_h > NARROW_BAND_H) {
            /* A band taller than the game's window but not the room's (near, 4:3: 144 lines) shows all its lines,
             * following the window up and down as it does across: the window's 48 lines of travel take the band's
             * 32, and a scroll slides it the band's height over the window's 128 lines. */
            const int32_t slack = LARGE_ROOM_H - (int32_t)c->band_h, travel = LARGE_ROOM_H - (int32_t)NARROW_BAND_H;
            const int down = observation->in_scroll && !(observation->scroll_direction & 1u);
            const int32_t top = down ? observation->scroll_start_top : observation->window_top;
            cy = world->origin_y + (top - world->origin_y) * slack / travel;
            if (down) cy += (observation->window_top - observation->scroll_start_top) * (int32_t)c->band_h / (int32_t)NARROW_BAND_H;
            c->shown_height = (int)c->band_h;
        }
        /* With the sub-pixel camera, the band takes Link's fraction of a pixel on an axis where it follows the game's
         * window and the window follows him, his place less half its own size (updateCameraPosition, a pixel a frame
         * toward it, which a walk never outruns), short of the room's far edge, where the fraction would take the band
         * past the window's last place: it moves as the window would to the 256th of a pixel, in the same part of it
         * as its whole pixels.  A room shown whole and still takes none, Link alone drawn at his; nor does a side
         * view, where the game's window lags his jumps. */
        const int32_t last_x = c->fraction_x, last_y = c->fraction_y;
        c->fraction_x = c->fraction_y = 0;
        if (c->exact && !observation->in_transition && world->domain == ORACLES_E11_DUNGEON) {
            const int lx = world->link_x - observation->link_step_x - world->origin_x, ly = world->link_y - observation->link_step_y - world->origin_y;
            const int32_t fx = observation->drawn_camera_x == lx - GAME_WINDOW_W / 2 && observation->drawn_camera_x < LARGE_ROOM_W - GAME_WINDOW_W ? observation->drawn_sub_x : 0;
            const int32_t fy = observation->drawn_camera_y == ly - NARROW_BAND_H / 2 && observation->drawn_camera_y < LARGE_ROOM_H - NARROW_BAND_H ? observation->drawn_sub_y : 0;
            if (c->band_w < LARGE_ROOM_W) {
                const int32_t slack = LARGE_ROOM_W - (int32_t)c->band_w, travel = LARGE_ROOM_W - GAME_WINDOW_W, inside = observation->window_left - world->origin_x;
                c->fraction_x = (inside * ORACLES_E11_F256 + fx) * slack / travel - inside * slack / travel * ORACLES_E11_F256;
            }
            if (drawn_back(world)) c->fraction_y = 0;
            else if (c->band_h > NARROW_BAND_H) {
                const int32_t slack = LARGE_ROOM_H - (int32_t)c->band_h, travel = LARGE_ROOM_H - (int32_t)NARROW_BAND_H, inside = observation->window_top - world->origin_y;
                c->fraction_y = (inside * ORACLES_E11_F256 + fy) * slack / travel - inside * slack / travel * ORACLES_E11_F256;
            } else c->fraction_y = fy;
        }
        /* A transition starting or ending (a scroll, a load) takes the fraction from Link or gives it back: the band
         * goes on from where it stood and reaches the new one by LARGE_EASE a frame, rather than by a whole pixel's
         * part at once. */
        if (c->large_have && observation->in_transition != c->large_in_transition) {
            c->large_ease_x = last_x - c->fraction_x;
            c->large_ease_y = last_y - c->fraction_y;
        }
        c->large_ease_x = c->large_ease_x > LARGE_EASE ? c->large_ease_x - LARGE_EASE : c->large_ease_x < -LARGE_EASE ? c->large_ease_x + LARGE_EASE : 0;
        c->large_ease_y = c->large_ease_y > LARGE_EASE ? c->large_ease_y - LARGE_EASE : c->large_ease_y < -LARGE_EASE ? c->large_ease_y + LARGE_EASE : 0;
        c->fraction_x += c->large_ease_x; c->fraction_y += c->large_ease_y;
        c->large_have = c->exact; c->large_in_transition = observation->in_transition;
        if (camera_x) *camera_x = cx;
        if (camera_y) *camera_y = cy;
        c->vertical_tracking = 1;
        c->shown_x.valid = 0; c->shown_y.valid = 0;
        return 1;
    }
    c->large_have = 0; c->large_ease_x = c->large_ease_y = 0;
    /* The camera switched to the 256th of a pixel or back since the last reduction, with the follow that settles,
     * which takes Link's place in the camera's precision: the reducers' last place of Link is that observation's in
     * the new precision, so that the switch is not a step of his (a stop or a jump of his speed). */
    if (c->exact != c->fed_exact) {
        if (oracles_e11_config_settles(config)) {
            const int32_t sign = c->exact ? 1 : -1;
            if (c->state.status == ORACLES_E11_TRACKING) c->state.last_link_x += sign * observation->drawn_sub_x;
            if (c->state_y.status == ORACLES_E11_TRACKING) c->state_y.last_link_x += sign * observation->drawn_sub_y;
            /* To the 256th of a pixel the band shows the reducer's place: it takes the whole camera's, which in
             * whole pixels may stand up to three pixels from it (shown_correction), so that the switch moves
             * nothing; not known (a state just loaded, no reduction since), the reducer's own. */
            if (c->exact && c->state.status == ORACLES_E11_TRACKING && c->shown_x.valid && c->shown_x.last_link != INT32_MIN)
                c->state.camera_pos = (int32_t)(((int64_t)c->shown_x.last_link - c->shown_x.offset) * ORACLES_E11_F256);
            if (c->exact && c->state_y.status == ORACLES_E11_TRACKING && c->shown_y.valid && c->shown_y.last_link != INT32_MIN)
                c->state_y.camera_pos = (int32_t)(((int64_t)c->shown_y.last_link - c->shown_y.offset) * ORACLES_E11_F256);
        }
        c->fed_exact = c->exact;
    }
    OraclesE11Observation obs;
    reducer_observation(c, frame, observation, 1, &obs);
    restart_on_domain_change(&c->state_y, &obs, config);
    limit_bounds(&c->state_y, config, c->limits[2], c->limits[3], inset_y - vertical_margin(c, here), &obs);
    const OraclesE11Result ry = oracles_e11_reduce(&c->state_y, &obs, config);
    c->state_y = ry.state;
    c->vertical_tracking = c->state_y.status == ORACLES_E11_TRACKING;
    if (!c->vertical_tracking) c->shown_y.valid = 0;
    if (observation->drawn_link_dy_held && c->shown_y.valid) c->shown_y.offset -= observation->drawn_link_dy_held;
    const int32_t link_y = here->link_y - observation->link_step_y + observation->drawn_link_dy;
    int32_t correction_y = c->vertical_tracking ? shown_correction(&c->shown_y, &c->state_y, &obs, config, link_y, !c->exact) : 0;

    reducer_observation(c, frame, observation, 0, &obs);
    restart_on_domain_change(&c->state, &obs, config);
    limit_bounds(&c->state, config, c->limits[0], c->limits[1], inset_x, &obs);
    c->limits[0] = c->limits[2] = INT32_MIN; c->limits[1] = c->limits[3] = INT32_MAX;
    const OraclesE11Result r = oracles_e11_reduce(&c->state, &obs, config);
    c->state = r.state;
    const int tracking = c->state.status == ORACLES_E11_TRACKING;
    const int32_t link_x = here->link_x - observation->link_step_x;
    int32_t correction_x = tracking ? shown_correction(&c->shown_x, &c->state, &obs, config, link_x, !c->exact) : 0;
    if (tracking && c->vertical_tracking && observation->link_in_phase && !correction_x != !correction_y) {
        int32_t *alone = correction_x ? &correction_x : &correction_y;
        unsigned *held = correction_x ? &c->held_x : &c->held_y;
        if ((*alone == 1 || *alone == -1) && *held < PAIRED_HOLD_FRAMES) { *alone = 0; (*held)++; }
        else *held = 0;
    } else { c->held_x = 0; c->held_y = 0; }
    if (c->vertical_tracking) c->shown_y.offset += correction_y;
    if (camera_y) *camera_y = c->vertical_tracking ? link_y - c->shown_y.offset + vertical_margin(c, here) - inset_y : observation->window_top - window_inset;
    c->fraction_x = 0;
    c->fraction_y = c->vertical_tracking ? shown_fraction(&c->state_y, link_y - c->shown_y.offset) : 0;
    if (!tracking) { c->shown_x.valid = 0; return 0; }
    c->shown_x.offset += correction_x;
    if (camera_x) *camera_x = link_x - c->shown_x.offset - inset_x;
    c->fraction_x = shown_fraction(&c->state, link_x - c->shown_x.offset);
    return 1;
}

void oracles_enhanced_camera_fraction(const OraclesEnhancedCamera *c, int32_t *x, int32_t *y) { *x = c->fraction_x; *y = c->fraction_y; }
void oracles_enhanced_camera_set_exact(OraclesEnhancedCamera *c, int exact) { c->exact = exact != 0; }
void oracles_enhanced_camera_limit(OraclesEnhancedCamera *c, const int32_t limits[4]) { memcpy(c->limits, limits, sizeof c->limits); }

int oracles_enhanced_camera_vertical_tracking(const OraclesEnhancedCamera *c) { return c->vertical_tracking; }

void oracles_enhanced_camera_set_band(OraclesEnhancedCamera *c, unsigned width, unsigned height)
{
    c->band_w = width; c->band_h = height;
    c->observer.band_width = width;
    set_wide_config(c);
    oracles_e11_state_initial(&c->state, &c->config);
    oracles_e11_state_initial(&c->state_y, &c->config);
    c->vertical_tracking = 0;
    c->shown_x.valid = 0; c->shown_y.valid = 0;
    c->large_have = 0; c->large_ease_x = c->large_ease_y = 0;
}

void oracles_enhanced_camera_shown(const OraclesEnhancedCamera *c, unsigned *width, unsigned *height)
{
    *width = c->shown_wide ? c->band_w : NARROW_BAND_W;
    *height = c->shown_height ? (unsigned)c->shown_height : NARROW_BAND_H;
}

int oracles_enhanced_camera_frame(OraclesEnhancedCamera *c, uint32_t frame, int32_t *camera_x, int32_t *camera_y)
{
    OraclesEnhancedObservation observation;
    oracles_enhanced_observe(c->guest, &c->observer, &observation);
    return oracles_enhanced_camera_reduce(c, frame, &observation, camera_x, camera_y);
}

const OraclesE11State *oracles_enhanced_camera_state(const OraclesEnhancedCamera *c) { return &c->state; }
const OraclesE11State *oracles_enhanced_camera_state_vertical(const OraclesEnhancedCamera *c) { return &c->state_y; }
const OraclesE11Config *oracles_enhanced_camera_config(const OraclesEnhancedCamera *c) { return &c->config; }
void oracles_enhanced_camera_shown_offsets(const OraclesEnhancedCamera *c, OraclesEnhancedShownOffset *x, OraclesEnhancedShownOffset *y) { *x = c->shown_x; *y = c->shown_y; }
void oracles_enhanced_camera_set_shown_offsets(OraclesEnhancedCamera *c, const OraclesEnhancedShownOffset *x, const OraclesEnhancedShownOffset *y)
{
    c->shown_x = *x; c->shown_y = *y;
}
unsigned oracles_enhanced_camera_profile(const OraclesEnhancedCamera *c) { return c->profile; }

void oracles_enhanced_camera_set_profile(OraclesEnhancedCamera *c, unsigned profile)
{
    c->profile = profile == 2u || profile == 3u ? profile : 1u;
    oracles_e11_config_profile(&c->config, c->profile);
    set_wide_config(c);
    oracles_e11_state_initial(&c->state, &c->config);
    oracles_e11_state_initial(&c->state_y, &c->config);
    c->vertical_tracking = 0;
    c->shown_x.valid = 0; c->shown_y.valid = 0;
    c->large_have = 0; c->large_ease_x = c->large_ease_y = 0;
}

void oracles_enhanced_camera_set_state(OraclesEnhancedCamera *c, const OraclesE11State *state, const OraclesE11State *vertical)
{
    if (!state) return;
    c->state = *state;
    if (vertical) c->state_y = *vertical;
    else oracles_e11_state_initial(&c->state_y, &c->config);
    c->vertical_tracking = 0;
    c->restored = 1;
    c->fed_exact = c->exact;
    /* The whole offsets: the savestate's, set after the states, else taken afresh at the next frame (shown_correction). */
    c->shown_x.valid = 0; c->shown_y.valid = 0;
    c->large_have = 0; c->large_in_transition = 0; c->large_ease_x = c->large_ease_y = 0;   /* a large room's fraction taken afresh */
    /* The reducer ignores observations of an epoch below its own: the
     * observer resumes at the restored epoch, its reference room rebuilt. */
    const unsigned map_width = c->observer.map_width, band_width = c->observer.band_width;
    memset(&c->observer, 0, sizeof c->observer);
    c->observer.map_width = map_width;
    c->observer.band_width = band_width;
    c->observer.epoch = state->epoch / 8u;
}
