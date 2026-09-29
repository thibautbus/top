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
#define NORMAL_SCROLL_MODE 0x01u
#define TRANSITION_IDLE 0x02u
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
 * band there, centred in its own (but at sea, oracles_enhanced_world_extend_to_sea). */
static int wide_domain(OraclesE11Domain domain) { return domain == ORACLES_E11_EXTERIOR || domain == ORACLES_E11_ERA; }
static int drawn_back(const OraclesEnhancedWorld *world) { return world->viewport > NARROW_BAND_W; }

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
    const int viewport = wide_domain(out->domain) && band_width ? (int)band_width : NARROW_BAND_W;
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
        const int gutter = (viewport - LARGE_ROOM_W) / 2;
        int32_t origin = out->origin_x - gutter, right_edge = out->origin_x + LARGE_ROOM_W + gutter;
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
        const int gutter = (viewport - SMALL_ROOM_W) / 2;
        int32_t origin = out->origin_x - gutter, right_edge = out->origin_x + SMALL_ROOM_W + gutter;
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
    out->cutscene = oracles_guest_read8(guest, t->cutscene_index) > 1u;
    out->in_transition = !(scroll_mode == NORMAL_SCROLL_MODE && transition_state == TRANSITION_IDLE);
    out->in_scroll = (transition_state >= 3u && transition_state <= 5u) || scroll_mode == ORACLES_SCROLL_MODE_DECIDED || scroll_mode == ORACLES_SCROLL_MODE_TRANSITION;
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
    const int warping = (link && link[ORACLES_OBJ_STATE] == LINK_STATE_WARPING) || parked || absent;
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
    unsigned band_w, band_h;             /* the surface's world band: 256x128, or 480x254 in the drawn-back view */
    int shown_wide;                      /* the last reduction framed the whole band */
    int restored;                        /* states just restored (a savestate): the next reduction takes their viewport as it finds it */
    OraclesE11State state;               /* the horizontal reducer */
    OraclesE11State state_y;             /* the vertical one: the same reducer, the axes swapped */
    int vertical_tracking;
    OraclesEnhancedObserver observer;
    unsigned overworld_stride;
    char session[ORACLES_E11_ID_CAP];
    char session_y[ORACLES_E11_ID_CAP];
};

/* The vertical reducer sees the world turned on its side: Link's y as its x,
 * the map's height as its width.  Its viewport is pinned at 256 px while the
 * world band is 128 lines tall, so the band is the middle half of that
 * viewport: the band's top is the reducer's position plus 64, and the bounds
 * are widened by 64 px each way so that the band, not the viewport, stops at
 * the map's edge.  Framing at 128 puts Link at the band's middle line.  (The
 * drawn-back view outdoors: a viewport of 480, a band of 254, a margin of 113.) */
static int band_px(const OraclesEnhancedCamera *c, const OraclesEnhancedWorld *world) { return drawn_back(world) ? (int)c->band_h : NARROW_BAND_H; }
static int vertical_margin(const OraclesEnhancedCamera *c, const OraclesEnhancedWorld *world) { return (world->viewport - band_px(c, world)) / 2; }

/* The configuration outdoors: the pinned one, its viewport widened to the band's. */
static void set_wide_config(OraclesEnhancedCamera *c)
{
    c->wide = c->config;
    c->wide.viewport_width = (int32_t)c->band_w * ORACLES_E11_F256;
    c->wide.framing = (int32_t)c->band_w / 2 * ORACLES_E11_F256;
}
static const OraclesE11Config *config_for(const OraclesEnhancedCamera *c, const OraclesEnhancedWorld *world) { return drawn_back(world) ? &c->wide : &c->config; }

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
    if (!vertical) {
        obs->link_x = world->link_x * ORACLES_E11_F256;
        obs->link_y = world->link_y * ORACLES_E11_F256;
        obs->bounds_origin_x = world->bounds_origin_x * ORACLES_E11_F256;
        obs->bounds_origin_y = world->bounds_origin_y * ORACLES_E11_F256;
        obs->bounds_width = (int64_t)world->bounds_width * ORACLES_E11_F256;
        obs->bounds_height = (int64_t)world->bounds_height * ORACLES_E11_F256;
        obs->availability = world->wide && !observation->parked_unknown ? ORACLES_E11_AVAILABLE : ORACLES_E11_UNAVAILABLE;
    } else {
        obs->link_x = world->link_y * ORACLES_E11_F256;
        obs->link_y = world->link_x * ORACLES_E11_F256;
        /* A room shown alone keeps its own centred band inside its extent, as
         * its width keeps its centred viewport (world_from_room): 128 lines
         * in the drawn-back view's 254, the extent is widened to it, and a
         * scroll that opens an edge widens it further without leaving the
         * band where it stood, so that the reducer tracks the room open or
         * closed and the band does not jump.  In the normal band a room fills
         * the band's height already. */
        int32_t top = world->bounds_origin_y, height = world->bounds_height;
        const int32_t band = band_px(c, world);
        if (world->alone && band > SMALL_ROOM_H) {
            const int32_t own_top = world->origin_y - (band - SMALL_ROOM_H) / 2, bottom = top + height;
            if (own_top < top) top = own_top;
            height = (bottom > own_top + band ? bottom : own_top + band) - top;
        }
        obs->bounds_origin_x = (top - vertical_margin(c, world)) * ORACLES_E11_F256;
        obs->bounds_origin_y = world->bounds_origin_x * ORACLES_E11_F256;
        obs->bounds_width = (int64_t)(height + 2 * vertical_margin(c, world)) * ORACLES_E11_F256;
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
    if (c->restored) { c->shown_wide = drawn_back(here); c->restored = 0; }
    if (drawn_back(here) != c->shown_wide) {
        oracles_e11_state_initial(&c->state, config);
        oracles_e11_state_initial(&c->state_y, config);
    }
    c->shown_wide = drawn_back(here);
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
        int32_t cy = observation->window_top - window_inset;
        if (drawn_back(world)) {
            cy = world->origin_y - ((int32_t)c->band_h - LARGE_ROOM_H) / 2;
            if (observation->in_scroll && !(observation->scroll_direction & 1u))
                cy += (observation->window_top - observation->scroll_start_top) * LARGE_ROOM_H / (int32_t)NARROW_BAND_H;
        }
        if (camera_x) *camera_x = cx;
        if (camera_y) *camera_y = cy;
        c->vertical_tracking = 1;
        return 1;
    }
    OraclesE11Observation obs;
    reducer_observation(c, frame, observation, 1, &obs);
    restart_on_domain_change(&c->state_y, &obs, config);
    const OraclesE11Result ry = oracles_e11_reduce(&c->state_y, &obs, config);
    c->state_y = ry.state;
    c->vertical_tracking = c->state_y.status == ORACLES_E11_TRACKING;
    if (camera_y) *camera_y = c->vertical_tracking ? c->state_y.camera_pos / ORACLES_E11_F256 + vertical_margin(c, here) - inset_y : observation->window_top - window_inset;

    reducer_observation(c, frame, observation, 0, &obs);
    restart_on_domain_change(&c->state, &obs, config);
    const OraclesE11Result r = oracles_e11_reduce(&c->state, &obs, config);
    c->state = r.state;
    if (c->state.status != ORACLES_E11_TRACKING) return 0;
    if (camera_x) *camera_x = c->state.camera_pos / ORACLES_E11_F256 - inset_x;
    return 1;
}

int oracles_enhanced_camera_vertical_tracking(const OraclesEnhancedCamera *c) { return c->vertical_tracking; }

void oracles_enhanced_camera_set_band(OraclesEnhancedCamera *c, unsigned width, unsigned height)
{
    c->band_w = width; c->band_h = height;
    c->observer.band_width = width;
    set_wide_config(c);
    oracles_e11_state_initial(&c->state, &c->config);
    oracles_e11_state_initial(&c->state_y, &c->config);
    c->vertical_tracking = 0;
}

void oracles_enhanced_camera_shown(const OraclesEnhancedCamera *c, unsigned *width, unsigned *height)
{
    *width = c->shown_wide ? c->band_w : NARROW_BAND_W;
    *height = c->shown_wide ? c->band_h : NARROW_BAND_H;
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
unsigned oracles_enhanced_camera_profile(const OraclesEnhancedCamera *c) { return c->profile; }

void oracles_enhanced_camera_set_profile(OraclesEnhancedCamera *c, unsigned profile)
{
    c->profile = profile == 2u ? 2u : 1u;
    oracles_e11_config_profile(&c->config, c->profile);
    set_wide_config(c);
    oracles_e11_state_initial(&c->state, &c->config);
    oracles_e11_state_initial(&c->state_y, &c->config);
    c->vertical_tracking = 0;
}

void oracles_enhanced_camera_set_state(OraclesEnhancedCamera *c, const OraclesE11State *state, const OraclesE11State *vertical)
{
    if (!state) return;
    c->state = *state;
    if (vertical) c->state_y = *vertical;
    else oracles_e11_state_initial(&c->state_y, &c->config);
    c->vertical_tracking = 0;
    c->restored = 1;
    /* The reducer ignores observations of an epoch below its own: the
     * observer resumes at the restored epoch, its reference room rebuilt. */
    const unsigned map_width = c->observer.map_width, band_width = c->observer.band_width;
    memset(&c->observer, 0, sizeof c->observer);
    c->observer.map_width = map_width;
    c->observer.band_width = band_width;
    c->observer.epoch = state->epoch / 8u;
}
