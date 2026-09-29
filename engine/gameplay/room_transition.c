#include "room_transition.h"

#include "guest_tables.h"

#include <stdlib.h>
#include <string.h>

#define STATE_PREPARE 3u
#define STATE_SCROLL 5u
#define SUBSTATE_VERTICAL 1u
#define SUBSTATE_HORIZONTAL 2u
#define PALETTE_MODE_MIX 8u            /* wPaletteThread_mode: the fade between two palettes of a smooth palette transition */
#define PHASE_SCROLLING 2u              /* wScreenTransitionState3: 0 and 1 set up, 2 scrolls, 3 to 5 finish */
#define COUNTER_HORIZONTAL 0x14u        /* columns of a small room */
#define COUNTER_VERTICAL 0x10u          /* rows */
#define NATIVE_STEP_X 0x60              /* Link's move per scroll step, 8.8 (transitionUpdateScrollAndLinkPosition) */
#define NATIVE_STEP_Y 0x80
#define ONE_PIXEL 0x100
#define NATIVE_TOTAL_X (15 * 0x100)     /* what the game moves Link over a native transition */
#define NATIVE_TOTAL_Y (16 * 0x100)
#define SMALL_ROOM_W 160
#define SMALL_ROOM_H 128
#define LARGE_ROOM_W 240
#define LARGE_ROOM_H 176
#define LINK_HALF_WIDTH 4               /* the leading edge of his collision box */
#define SPECIALCOLLISION_HOLE 0x10u     /* specialCollisionValues.s: holes, water and lava alike */
#define SPECIALCOLLISION_VERTICAL_BRIDGE 0x11u   /* $11 to $13: a bridge crossed up and down, and its two sides */
#define SPECIALCOLLISION_VERTICAL_BRIDGE_RIGHT 0x13u
#define SPECIALCOLLISION_STAIRS 0x18u   /* the stairs that slow Link down */
#define SPECIALCOLLISION_HORIZONTAL_BRIDGE 0x19u /* $19 to $1b: a bridge crossed left and right, and its two ends */
#define SPECIALCOLLISION_HORIZONTAL_BRIDGE_RIGHT 0x1bu
#define TILETYPE_WATER 0x07u            /* tileTypes.s */
#define TILETYPE_UPCURRENT 0x12u        /* the four currents, $12 to $15 */
#define TILETYPE_LEFTCURRENT 0x15u
#define TILETYPE_SEAWATER 0x17u         /* Ages: drowns Link without the mermaid suit (checkSwimmingOverSeawater) */
#define VAR2F_MERMAID_SUIT 0x40u

struct OraclesRoomTransition {
    OraclesGuest *guest;
    const uint8_t *rom;
    size_t rom_size;
    uint32_t last_frame;
    int have_last;
    int armed;                          /* the step is doubled for the scroll in progress */
    uint16_t last_camera;               /* hCamera along the axis at the last frame: the machine's own move shows in it */
    int in_transition;
    int start_pos;                      /* Link's position along the axis when the transition began, 8.8 */
    int doubled;                        /* the transition in progress (or the last one) had its step doubled */
    int palette_pending;                /* a palette transition's mix outlived a doubled scroll: refresh the palettes when it stops */
    int swim;                           /* --continuous-swim: Link swimming at the surface too */
    int forced;                         /* the policy raised the forced transition's bit, not yet taken */
    OraclesRoomTransitionStats stats;
};

/* ---- pure helpers ---------------------------------------------------------------- */

int oracles_room_transition_swimming(const OraclesGuestTransitionState *s)
{
    return s && s->link_swimming_state != 0u;
}

int oracles_room_transition_in_water(const OraclesGuestTransitionState *s)
{
    return s && (s->link_swimming_state != 0u || (s->tileset_flags & ORACLES_TILESETFLAG_UNDERWATER));
}

int oracles_room_transition_refusal(const OraclesGuestTransitionState *s, int swim)
{
    /* The large rooms (the dungeons) keep the game's transition,
     * deliberately: the machine is the same (see collision_free) but a
     * dungeon's rooms are meant to be crossed as the game has it. */
    if (!s || (s->game != ORACLES_GAME_AGES && s->game != ORACLES_GAME_SEASONS) || s->active_group > 5u || s->room_is_large || (s->tileset_flags & ORACLES_TILESETFLAG_SIDESCROLL) || s->game_state != 2u || s->cutscene_index > 1u || s->text_is_active) return 0;
    if (s->transition_state < STATE_PREPARE || s->transition_state > STATE_SCROLL || (s->scroll_mode != 4u && s->scroll_mode != 8u) || s->transition_direction > 3u) return 1;
    if (s->link_force_state != 0u || s->link_object_index != 0xd0u || s->link_id != 0u || s->link_state != 1u) return 2;
    if (!(s->link_visible & 0x80u)) return 3;
    if (s->link_in_air) return 4;
    /* The sea floor (Ages, --continuous-swim): the game has Link walk there, in his mermaid suit's gait. */
    if ((s->tileset_flags & ORACLES_TILESETFLAG_UNDERWATER) && !swim) return 9;
    /* Swimming (--continuous-swim): in his normal swimming state only, not
     * while he enters the water (states 1 and 2) or drowns (state 4, lava). */
    if (s->link_swimming_state) {
        if (!swim || (s->link_swimming_state & 0x4fu) != ORACLES_SWIMMING_NORMAL) return 8;
        if (s->link_anim_mode != ORACLES_LINK_ANIM_MODE_SWIM && s->link_anim_mode != ORACLES_LINK_ANIM_MODE_DIVE) return 3;
    } else if (s->link_anim_mode != ORACLES_LINK_ANIM_MODE_WALK) return 3;
    /* Pushing against the edge as he reaches it (wLinkPushingDirection) is walking. */
    if (s->link_grab_state || s->magnet_glove_state || s->link_immobilized
        || s->link_playing_instrument || s->link_turning_disabled || (s->force_link_push_animation & 0x7fu)) return 5;
    /* Invincibility alone does not block: at the start of play the counter runs from a negative value with Link free to walk. */
    if (s->using_shield || s->pegasus_seed_counter_nonzero || s->link_knockback_counter || s->link_stun_counter || s->active_item) return 6;
    if (s->animation_data_bank == 0xffu) return 7;
    return -1;
}

/* screenTransitionState2@transition (bank1.s), the direction held replaced by
 * the one Link swims in: the checks it makes before it starts a transition.
 * It tries up or down, then left or right, and when both start the second
 * overwrites the first: across a corner the horizontal one is taken. */
#define EDGE 5u                         /* yh or xh at most 5: up or left */
#define TILESETFLAG_OUTDOORS 0x01u
#define TILEINDEX_DEEP_WATER 0xfcu      /* Ages: no transition over it without the mermaid suit (@checkCanTransitionOverWater) */

/* Whether Link moves toward the edge of direction `dir`: his motion's angle
 * (32 steps, turning a step at a time) within 5 steps of the edge's own, the
 * 135 degrees convertLinkAngleToDirectionButtons gives a direction held,
 * centred on it on both sides. */
static int moving_toward(uint8_t angle, unsigned dir)
{
    if (angle & 0x80u) return 0;
    unsigned off = (unsigned)(angle - dir * 8u) & 31u;
    if (off > 16u) off = 32u - off;
    return off <= 5u;
}

/* convertLinkAngleToDirectionButtons' table on the direction held (wLinkAngle, a multiple of 4): bit n, direction n. */
static int held_toward(uint8_t angle, unsigned dir)
{
    static const uint8_t sectors[8] = { 1u, 1u | 2u, 2u, 2u | 4u, 4u, 4u | 8u, 8u, 8u | 1u };
    return !(angle & 0x80u) && (sectors[(angle >> 2) & 7u] & (1u << dir));
}

static int edge_allowed(const OraclesGuestTransitionState *s, unsigned dir)
{
    /* A direction held toward the edge is the game's own case; one held
     * away from it, the player's wish, never overridden.  One across it
     * (left along the bottom) leaves the momentum to carry him. */
    if (held_toward(s->link_input_angle, dir) || held_toward(s->link_input_angle, dir ^ 2u)) return 0;
    if (!moving_toward(s->link_move_angle, dir)) return 0;
    if (s->game == ORACLES_GAME_AGES) {
        /* Ages forbids looping around the overworld, except up. */
        if (s->tileset_flags & TILESETFLAG_OUTDOORS) {
            if (dir == 1u && (s->active_room & 0x0fu) == (unsigned)(s->map_width - 1u)) return 0;
            if (dir == 2u && s->active_room >= (unsigned)(s->map_height - 1u) * 16u) return 0;
            if (dir == 3u && (s->active_room & 0x0fu) == 0u) return 0;
        }
        if (!(s->link_var2f & VAR2F_MERMAID_SUIT) && s->active_tile_index == TILEINDEX_DEEP_WATER) return 0;
    }
    return 1;
}

int oracles_room_transition_swim_edge(const OraclesGuestTransitionState *s)
{
    if (!s || s->transition_state != 2u || s->scroll_mode != 1u || !s->link_swimming_state) return -1;
    /* Link as the policy takes him through a transition: the same checks, the transition supposed started. */
    OraclesGuestTransitionState during = *s;
    during.transition_state = STATE_PREPARE;
    during.scroll_mode = 4u;
    if (oracles_room_transition_refusal(&during, 1) >= 0) return -1;
    if (!s->link_enabled || s->disable_screen_transitions || s->screen_transition_delay) return -1;
    /* A hole under him forbids it; a current (the low bits) lets the game take it without the direction. */
    if (s->hole_or_conveyor) return -1;
    /* A stroke of the flippers (A) carries him on without the direction
     * held: the one case where he swims to the edge and the game waits for
     * it.  The mermaid suit is driven by the direction pads, and a glide
     * after they are let go is left to the game. */
    if (!s->link_speed || (s->link_move_angle & 0x80u) || (s->link_var2f & VAR2F_MERMAID_SUIT) || (s->link_stroke != 1u && s->link_stroke != 2u)) return -1;
    /* The game tests the edge after Link has moved, and puts him back on it
     * whether it starts the transition or not (@transitionUp and the others
     * write his yh or xh before @transition): at the end of a frame he stands
     * a pixel short of the test, 6 across the top or the left, the boundary
     * itself across the bottom or the right.  The forced transition skips
     * that write: it starts from where his move took him, up to a pixel
     * further. */
    const unsigned xh = s->link_x >> 8, yh = s->link_y >> 8;
    if (xh <= EDGE + 1u && edge_allowed(s, 3u)) return 3;
    if (xh > EDGE + 1u && xh >= s->boundary_x && edge_allowed(s, 1u)) return 1;
    if (yh <= EDGE + 1u && edge_allowed(s, 0u)) return 0;
    if (yh > EDGE + 1u && yh >= s->boundary_y && edge_allowed(s, 2u)) return 2;
    return -1;
}

int oracles_room_transition_eligible(const OraclesGuestTransitionState *s, int swim)
{
    return oracles_room_transition_refusal(s, swim) < 0;
}

int oracles_room_transition_step(const OraclesGuestTransitionState *s)
{
    if (!s) return 0;
    if (!oracles_room_transition_in_water(s)) return ONE_PIXEL;
    /* objectSpeedTable (bank3.s): row speed/5, $20 (8.8) a row along an axis. */
    return (s->link_speed / 5) * 0x20;
}

static int rom_byte(const uint8_t *rom, size_t rom_size, unsigned bank, uint16_t address, uint8_t *value)
{
    if (!rom || address < 0x4000u || address >= 0x8000u) return 0;
    const size_t offset = (size_t)bank * 0x4000u + (address - 0x4000u);
    if (offset >= rom_size) return 0;
    *value = rom[offset];
    return 1;
}

int oracles_room_transition_next_walk_frame(const uint8_t *rom, size_t rom_size, const OraclesGuestTransitionState *s,
                                            OraclesGuestTransitionMutation *m)
{
    if (!rom || !s || !m || s->animation_data_bank == 0xffu) return 0;
    uint8_t counter = s->link_anim_counter;
    if (counter) counter--;
    if (counter) {
        /* The current frame still has time: only the counter moves. */
        m->update_walk_animation = 1;
        m->link_anim_counter = counter;
        m->link_anim_parameter = s->link_anim_parameter;
        m->link_anim_pointer = s->link_anim_pointer;
        m->link_animation_frame = s->link_animation_frame;
        return 1;
    }
    uint16_t at = s->link_anim_pointer;
    uint8_t duration = 0;
    if (!rom_byte(rom, rom_size, s->animation_data_bank, at, &duration)) return 0;
    if (duration == 0xffu) {
        /* The end marker: the byte after it is a negative offset, taken from
         * that byte's own address (specialObjectNextAnimationFrame builds
         * bc = $ffXX and adds it to hl). */
        const uint16_t offset_at = (uint16_t)(at + 1u);
        uint8_t offset = 0;
        if (!rom_byte(rom, rom_size, s->animation_data_bank, offset_at, &offset)) return 0;
        at = (uint16_t)(offset_at + (uint16_t)(0xff00u | offset));
        if (!rom_byte(rom, rom_size, s->animation_data_bank, at, &duration)) return 0;
    }
    if (duration == 0u || duration == 0xffu || at > 0x7ffcu) return 0;
    uint8_t frame = 0, parameter = 0;
    if (!rom_byte(rom, rom_size, s->animation_data_bank, (uint16_t)(at + 1u), &frame)
        || !rom_byte(rom, rom_size, s->animation_data_bank, (uint16_t)(at + 2u), &parameter)) return 0;
    m->update_walk_animation = 1;
    m->link_anim_counter = duration;
    m->link_anim_parameter = parameter;
    m->link_anim_pointer = (uint16_t)(at + 3u);
    m->link_animation_frame = frame;
    return 1;
}

static int rom_word(const uint8_t *rom, size_t rom_size, unsigned bank, uint16_t address, uint16_t *value)
{
    uint8_t lo = 0, hi = 0;
    if (!rom_byte(rom, rom_size, bank, address, &lo) || !rom_byte(rom, rom_size, bank, (uint16_t)(address + 1u), &hi)) return 0;
    *value = (uint16_t)(lo | (hi << 8));
    return 1;
}

int oracles_room_transition_tile_type(const uint8_t *rom, size_t rom_size, const OraclesGuestTransitionState *s, uint8_t tile)
{
    /* lookupCollisionTable: the list of wActiveCollisions in tileTypesTable,
     * pairs of tile and type up to a tile $00; a tile not listed is normal. */
    if (!rom || !s || s->tile_types_bank == 0xffu || s->active_collisions > 7u) return -1;
    uint16_t at = 0;
    if (!rom_word(rom, rom_size, s->tile_types_bank, (uint16_t)(s->tile_types_table + 2u * s->active_collisions), &at)) return -1;
    for (unsigned i = 0; i < 128u; i++, at = (uint16_t)(at + 2u)) {
        uint8_t key = 0, type = 0;
        if (!rom_byte(rom, rom_size, s->tile_types_bank, at, &key)) return -1;
        if (key == 0u) return 0;
        if (!rom_byte(rom, rom_size, s->tile_types_bank, (uint16_t)(at + 1u), &type)) return -1;
        if (key == tile) return type;
    }
    return -1;
}

/* Where Link may be moved beyond the game's own distance: on foot, a tile
 * without a collision, and with `ways` stairs and a bridge he crosses along
 * the transition's axis; swimming, water he can swim in (the collision of
 * holes, water and lava, and a water type: a current, the sea with the
 * mermaid suit), never land. */
typedef struct tile_rule { const uint8_t *rom; size_t rom_size; int swimming, ways; } tile_rule;

static int tile_allowed(const OraclesGuestTransitionState *s, const tile_rule *r, unsigned index)
{
    const uint8_t collision = s->room_collisions[index];
    if (!r->swimming) {
        if (collision == 0u) return 1;
        if (!r->ways) return 0;
        if (collision == SPECIALCOLLISION_STAIRS) return 1;
        if (s->transition_direction & 1u) return collision >= SPECIALCOLLISION_HORIZONTAL_BRIDGE && collision <= SPECIALCOLLISION_HORIZONTAL_BRIDGE_RIGHT;
        return collision >= SPECIALCOLLISION_VERTICAL_BRIDGE && collision <= SPECIALCOLLISION_VERTICAL_BRIDGE_RIGHT;
    }
    if (collision != SPECIALCOLLISION_HOLE || !s->room_layout) return 0;
    const int type = oracles_room_transition_tile_type(r->rom, r->rom_size, s, s->room_layout[index]);
    return type == (int)TILETYPE_WATER || (type >= (int)TILETYPE_UPCURRENT && type <= (int)TILETYPE_LEFTCURRENT)
        || (type == (int)TILETYPE_SEAWATER && (s->link_var2f & VAR2F_MERMAID_SUIT));
}

/* Link's coordinates during the transition are those of the room he leaves;
 * finishScrollingTransition rebases them by the room's size along the axis
 * (160x128, or 240x176 for a large room). */
static int tile_free(const OraclesGuestTransitionState *s, const tile_rule *r, int xh, int yh)
{
    const unsigned dir = s->transition_direction;
    const int w = s->room_is_large ? LARGE_ROOM_W : SMALL_ROOM_W, h = s->room_is_large ? LARGE_ROOM_H : SMALL_ROOM_H;
    int dx = 0, dy = 0;
    if (dir == 1u) dx = -w; else if (dir == 3u) dx = w;
    else if (dir == 2u) dy = -h; else dy = h;
    const int ex = (xh + dx) & 0xff, ey = (yh + dy) & 0xff;
    /* Not yet in the room entered along the axis: the room he leaves, walkable where he walked. */
    if ((dir & 1u) ? (ex < 0 || ex >= w) : (ey < 0 || ey >= h)) return 1;
    if (ex >= w || ey >= h) return 1;
    return tile_allowed(s, r, (unsigned)((ey >> 4) * 16 + (ex >> 4)));
}

static int reachable(const OraclesGuestTransitionState *s, const tile_rule *r, int extra)
{
    const unsigned dir = s->transition_direction;
    int x = s->link_x, y = s->link_y;
    if (dir == 1u) x += extra; else if (dir == 3u) x -= extra; else if (dir == 2u) y += extra; else y -= extra;
    const int xh = (x >> 8) & 0xff, yh = (y >> 8) & 0xff;
    /* The tile under him and the one his leading edge reaches. */
    if (!tile_free(s, r, xh, yh)) return 0;
    if (dir == 1u) return tile_free(s, r, xh + LINK_HALF_WIDTH, yh);
    if (dir == 3u) return tile_free(s, r, xh - LINK_HALF_WIDTH, yh);
    if (dir == 2u) return tile_free(s, r, xh, yh + LINK_HALF_WIDTH);
    return tile_free(s, r, xh, yh - LINK_HALF_WIDTH);
}

int oracles_room_transition_walkable(const OraclesGuestTransitionState *s, int extra)
{
    if (!s || !s->room_collisions) return 1;
    const tile_rule r = { NULL, 0, 0, 0 };
    return reachable(s, &r, extra);
}

int oracles_room_transition_passable(const OraclesGuestTransitionState *s, int extra)
{
    if (!s || !s->room_collisions) return 1;
    const tile_rule r = { NULL, 0, 0, 1 };
    return reachable(s, &r, extra);
}

int oracles_room_transition_swimmable(const uint8_t *rom, size_t rom_size, const OraclesGuestTransitionState *s, int extra)
{
    if (!s || !s->room_collisions) return 1;
    const tile_rule r = { rom, rom_size, 1, 0 };
    return reachable(s, &r, extra);
}

/* ---- the policy ----------------------------------------------------------------------- */

static uint8_t fast_delta(unsigned dir) { return (dir == 1u || dir == 2u) ? 8u : 0xf8u; }
static uint8_t native_delta(unsigned dir) { return (dir == 1u || dir == 2u) ? 4u : 0xfcu; }

static int in_scroll_substate(const OraclesGuestTransitionState *s)
{
    return s->transition_state == STATE_SCROLL
        && s->transition_substate == ((s->transition_direction & 1u) ? SUBSTATE_HORIZONTAL : SUBSTATE_VERTICAL);
}

static void move_link(OraclesRoomTransition *tr, const OraclesGuestTransitionState *s, OraclesGuestTransitionMutation *m, int machine_moved);

static void policy(void *opaque, const OraclesGuestTransitionState *s, OraclesGuestTransitionMutation *m)
{
    OraclesRoomTransition *tr = opaque;
    const int horizontal = s->transition_direction & 1u;
    const uint16_t camera = horizontal ? s->camera_x : s->camera_y;
    int camera_moved = 0;
    /* The frames the bus did not ask it about (a vblank it may not write at) are no break. */
    if (!tr->have_last || s->frame != tr->last_frame + 1u + s->frames_not_asked) {
        /* A savestate or a break in the frames: the step byte says whether a doubled scroll is in progress. */
        tr->armed = in_scroll_substate(s) && s->screen_scroll_delta == fast_delta(s->transition_direction);
        if (tr->armed) tr->doubled = 1;
    } else {
        const int moved = (int16_t)(uint16_t)(camera - tr->last_camera);
        camera_moved = moved == 4 || moved == -4 || moved == 8 || moved == -8;
    }
    tr->have_last = 1;
    tr->last_frame = s->frame;
    tr->last_camera = camera;

    const int in_transition = s->transition_state >= STATE_PREPARE && s->transition_state <= STATE_SCROLL;
    if (in_transition && !tr->in_transition) { tr->start_pos = horizontal ? s->link_x : s->link_y; tr->doubled = 0; }
    /* A smooth palette transition (paletteTransitions.s) mixes BG2-7 from the
     * palette left to the one entered over about thirty frames, the loaded
     * palette header forgotten ($ff) meanwhile.  The game loads the room's
     * palettes at the end of the scroll and the refresh shows them exactly;
     * a doubled scroll ends before the mix does, the load comes first (the
     * header loaded again while the thread still mixes, a state the native
     * order never shows) and the mix's last step (1/16 of the palette left,
     * truncated) stays on screen for the whole room.  The refresh is asked
     * for again once the thread stops, in normal play (the guest's own
     * conditions, so that it is not lost); a warp or a cutscene reloads the
     * palettes itself.  A pending refresh outlives the start of another
     * transition: its scroll waits for the thread and may load nothing. */
    const int normal_play = s->game_state == 2u && s->cutscene_index <= 1u;
    if (!in_transition && tr->doubled && s->palette_thread_mode == PALETTE_MODE_MIX && s->loaded_tileset_palette == s->tileset_palette) tr->palette_pending = 1;
    if (!in_transition && s->palette_thread_mode != PALETTE_MODE_MIX) tr->doubled = 0;   /* only the mix that runs on from the scroll: not a later one of a script */
    if (!normal_play) tr->palette_pending = 0;
    if (tr->palette_pending && !in_transition && s->palette_thread_mode == 0u && s->loaded_tileset_palette == s->tileset_palette) {
        m->refresh_bg_palettes = 0xfcu;
        tr->palette_pending = 0;
        tr->stats.palette_refreshes++;
    }
    tr->in_transition = in_transition;

    /* --continuous-swim: the transition a swim's momentum takes him to, the game asking the direction held. */
    if (tr->swim && !in_transition) {
        const int edge = oracles_room_transition_swim_edge(s);
        if (edge >= 0) {
            m->force_transition = 1;
            m->forced_direction = (uint8_t)edge;
            if (!(s->transition_direction_raw & 0x80u)) tr->stats.forced_transitions++;
            tr->forced = 1;
        } else if (tr->forced && (s->transition_direction_raw & 0x80u) && s->transition_state == 2u) {
            m->force_transition = 2;
            tr->forced = 0;
        }
    }
    if (in_transition) tr->forced = 0;
    const int refusal = oracles_room_transition_refusal(s, tr->swim);
    if (refusal >= 0) {
        /* Hands off; a doubled step is given back to the game before the scroll goes on. */
        if (tr->armed && in_scroll_substate(s) && s->transition_phase <= PHASE_SCROLLING) {
            m->set_scroll_delta = 1;
            m->scroll_delta = native_delta(s->transition_direction);
        }
        tr->armed = 0;
        if (s->transition_state >= STATE_PREPARE && s->transition_state <= STATE_SCROLL) { tr->stats.refused_frames++; tr->stats.refused_by[refusal]++; }
        return;
    }

    /* The scroll step, doubled at the first frame of the scroll (the counter
     * still whole, the registers aligned), so that one column or row is drawn
     * per step. */
    if (in_scroll_substate(s) && s->transition_phase <= PHASE_SCROLLING && !tr->armed
        && s->screen_scroll_counter == (horizontal ? COUNTER_HORIZONTAL : COUNTER_VERTICAL) && s->scroll_alignment == 0u) {
        m->set_scroll_delta = 1;
        m->scroll_delta = fast_delta(s->transition_direction);
        tr->armed = 1;
        tr->doubled = 1;
        tr->stats.transitions++;
        if (oracles_room_transition_in_water(s)) tr->stats.swim_transitions++;
    }
    move_link(tr, s, m, camera_moved && in_scroll_substate(s));
}

/* Link walks a pixel a frame, or swims at his own speed: what the machine
 * moved him this frame (it moves the camera and him together, so the
 * camera's move says), topped up; his animation goes on with it. */
static void move_link(OraclesRoomTransition *tr, const OraclesGuestTransitionState *s, OraclesGuestTransitionMutation *m, int machine_moved)
{
    const int horizontal = s->transition_direction & 1u, swimming = oracles_room_transition_swimming(s), in_water = oracles_room_transition_in_water(s);
    int extra = oracles_room_transition_step(s) - (machine_moved ? (horizontal ? NATIVE_STEP_X : NATIVE_STEP_Y) : 0);
    /* Up to the distance the game itself moves him (15 px across, 16 px up
     * or down) the tiles are the game's own choice; beyond it, only onto
     * tiles without a collision on foot, onto water swimming. */
    const int pos = horizontal ? s->link_x : s->link_y;
    const int ahead = (s->transition_direction == 1u || s->transition_direction == 2u) ? pos - tr->start_pos : tr->start_pos - pos;
    /* Leftward or upward his coordinate runs below zero ($00c2 to $ffc2) before the rebase: the difference is signed
     * over 16 bits.  Without --continuous-swim it is taken as it always was, so that the routes recorded with
     * --continuous-transitions alone replay as they were played: there the distance runs negative and the cap never
     * holds Link back, leftward or upward. */
    const int travelled = tr->swim ? (int16_t)(uint16_t)ahead : ahead;
    const int native_total = horizontal ? NATIVE_TOTAL_X : NATIVE_TOTAL_Y;
    if (extra > 0 && travelled + extra > native_total
        && !(swimming ? oracles_room_transition_swimmable(tr->rom, tr->rom_size, s, extra)
             : tr->swim ? oracles_room_transition_passable(s, extra) : oracles_room_transition_walkable(s, extra))) {
        extra = 0;
        tr->stats.capped_frames++;
    }
    /* On foot he stops when he may not go on; swimming, his animation runs on
     * whether he moves or not, as linkUpdateDiving animates it every frame. */
    if (extra < 0) extra = 0;
    if (!extra && !swimming) return;
    OraclesGuestTransitionMutation walk;
    memset(&walk, 0, sizeof walk);
    if (!oracles_room_transition_next_walk_frame(tr->rom, tr->rom_size, s, &walk)) { tr->stats.unanimated_frames++; return; }   /* no animation, no move */
    m->update_walk_animation = 1;
    m->link_anim_counter = walk.link_anim_counter;
    m->link_anim_parameter = walk.link_anim_parameter;
    m->link_anim_pointer = walk.link_anim_pointer;
    m->link_animation_frame = walk.link_animation_frame;
    switch (s->transition_direction) {
    case 1u: m->link_x_delta = (int16_t)extra; break;
    case 3u: m->link_x_delta = (int16_t)-extra; break;
    case 2u: m->link_y_delta = (int16_t)extra; break;
    default: m->link_y_delta = (int16_t)-extra; break;
    }
    if (!extra) return;
    tr->stats.walked_frames++;
    if (in_water) tr->stats.swum_frames++;
}

static void reset(void *opaque)
{
    OraclesRoomTransition *tr = opaque;
    tr->have_last = 0;
    tr->armed = 0;
    tr->in_transition = 0;
    tr->doubled = 0;
    tr->palette_pending = 0;
    tr->forced = 0;
}

OraclesRoomTransition *oracles_room_transition_start(OraclesGuest *guest, const uint8_t *rom, size_t rom_size, int swim)
{
    if (!guest || !rom || !rom_size || !oracles_compat_continuous_transitions(oracles_guest_profile(guest))) return NULL;
    if (swim && !oracles_compat_continuous_swim(oracles_guest_profile(guest))) return NULL;
    OraclesRoomTransition *tr = calloc(1, sizeof *tr);
    if (!tr) return NULL;
    tr->guest = guest;
    tr->rom = rom;
    tr->rom_size = rom_size;
    tr->swim = swim != 0;
    oracles_guest_set_transition_swim(guest, tr->swim);
    oracles_guest_set_transition_policy(guest, policy, reset, tr);
    return tr;
}

void oracles_room_transition_stop(OraclesRoomTransition *tr)
{
    if (!tr) return;
    oracles_guest_set_transition_policy(tr->guest, NULL, NULL, NULL);
    oracles_guest_set_transition_swim(tr->guest, 0);
    free(tr);
}

void oracles_room_transition_stats(const OraclesRoomTransition *tr, OraclesRoomTransitionStats *out)
{
    if (!out) return;
    if (tr) *out = tr->stats; else memset(out, 0, sizeof *out);
}
