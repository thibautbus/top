/* The transition transaction of the continuous transitions: the only path by which
 * the host writes into the live instance, applied at the return of
 * drawAllSprites or at the vblank of a frame whose logic did not finish. */
#include "guest_internal.h"

void oracles_guest_set_transition_policy(OraclesGuest *guest, OraclesGuestTransitionFn policy, OraclesGuestTransitionResetFn reset, void *opaque)
{
    guest->transition_policy = policy;
    guest->transition_reset = reset;
    guest->transition_opaque = opaque;
}

void oracles_guest_set_transition_swim(OraclesGuest *guest, int on)
{
    guest->transition_swim = on != 0;
}

int oracles_guest_has_transition_policy(const OraclesGuest *guest)
{
    return guest && guest->transition_policy != NULL;
}

/* ---- the transition transaction ---------------------------------------- */

#define TRANSITION_STATE_IDLE 2u
#define TRANSITION_STATE_PREPARE 3u
#define TRANSITION_STATE_SCROLL 5u
#define SCROLL_MODE_NORMAL 1u
#define SCROLL_MODE_TRANSITION_DECIDED 4u
#define SCROLL_MODE_TRANSITION_LOADED 8u
#define LINK_STATE_NORMAL 1u
#define WALK_STEP_MAX 0x100              /* a pixel a frame */
#define SWIM_STEP_MAX 0x160              /* SPEED_160 along an axis: the fastest swim, the mermaid suit with the swimmer's ring */

static uint8_t *bank0_field(OraclesGuest *guest, OraclesGuestSym sym, size_t size)
{
    if (sym.bank != 0 || sym.addr < 0xc000u || (size_t)(sym.addr - 0xc000u) + size > 0x1000u) return NULL;
    uint8_t *wram = oracles_guest_wram_writable(guest, 0);
    return wram ? wram + (sym.addr - 0xc000u) : NULL;
}

static uint8_t *link_field(OraclesGuest *guest, OraclesGuestSym sym, size_t size)
{
    if (sym.bank != ORACLES_OBJECTS_BANK || sym.addr < ORACLES_OBJECTS_BASE || (size_t)(sym.addr - ORACLES_OBJECTS_BASE) + size > 0x100u) return NULL;
    uint8_t *wram = oracles_guest_wram_writable(guest, ORACLES_OBJECTS_BANK);
    return wram ? wram + (sym.addr - ORACLES_OBJECTS_BASE) : NULL;
}

/* A scrolling transition in normal play, Link in his normal state.  A large
 * room scrolls the same 160 or 128 px by the same machine (the columns
 * beyond are drawn afterwards, state 3 of the substate). */
static int transition_state_safe(const OraclesGuestTransitionState *s)
{
    return s->game_state == 2u && s->cutscene_index <= 1u && !s->text_is_active && !(s->tileset_flags & ORACLES_TILESETFLAG_SIDESCROLL)
        && s->transition_state >= TRANSITION_STATE_PREPARE && s->transition_state <= TRANSITION_STATE_SCROLL
        && (s->scroll_mode == SCROLL_MODE_TRANSITION_DECIDED || s->scroll_mode == SCROLL_MODE_TRANSITION_LOADED)
        && s->transition_direction <= 3u && s->link_force_state == 0u
        && s->link_object_index == (ORACLES_OBJECTS_BASE >> 8) && s->link_id == 0u && s->link_state == LINK_STATE_NORMAL;
}

/* Nothing in hand, nothing happening to him. */
static int link_unhindered(const OraclesGuestTransitionState *s)
{
    return (s->link_visible & 0x80u) && !s->link_in_air
        && !s->link_grab_state && !s->magnet_glove_state && !s->link_immobilized
        && !s->link_playing_instrument && !s->link_turning_disabled && !(s->force_link_push_animation & 0x7fu)
        && !s->using_shield && !s->pegasus_seed_counter_nonzero
        && !s->link_knockback_counter && !s->link_stun_counter && !s->active_item;
}

/* Link walking on foot. */
static int transition_walk_safe(const OraclesGuestTransitionState *s)
{
    return transition_state_safe(s) && link_unhindered(s) && s->link_anim_mode == ORACLES_LINK_ANIM_MODE_WALK && !s->link_swimming_state
        && !(s->tileset_flags & ORACLES_TILESETFLAG_UNDERWATER);
}

/* Link swimming at the surface, with --continuous-swim. */
static int transition_swim_safe(const OraclesGuest *guest, const OraclesGuestTransitionState *s)
{
    return guest->transition_swim && transition_state_safe(s) && link_unhindered(s) && !(s->tileset_flags & ORACLES_TILESETFLAG_UNDERWATER)
        && (s->link_swimming_state & 0x4fu) == ORACLES_SWIMMING_NORMAL
        && (s->link_anim_mode == ORACLES_LINK_ANIM_MODE_SWIM || s->link_anim_mode == ORACLES_LINK_ANIM_MODE_DIVE);
}

/* Link swimming at the surface in normal play, no transition in progress (--continuous-swim), no direction held along the edge's axis,
 * at the edge of `dir` (a pixel short of screenTransitionState2's test, where it puts him back), and nothing of what
 * that test refuses first: the transition his momentum takes him to may be forced there, by the bit
 * screenTransitionState2 reads before any check of its own. */
static int edge_swim_safe(const OraclesGuest *guest, const OraclesGuestTransitionState *s, unsigned dir)
{
    const unsigned xh = s->link_x >> 8, yh = s->link_y >> 8;
    const int at_edge = dir == 0u ? yh <= 6u : dir == 2u ? yh >= s->boundary_y && yh < 0xc0u : dir == 3u ? xh <= 6u : xh >= s->boundary_x && xh < 0xc0u;
    /* The direction held (wLinkAngle, from the pad: a multiple of 4): none, or square across the edge's axis, never
     * toward the edge or away from it, a diagonal included. */
    const unsigned held = s->link_input_angle;
    const int held_across = (held & 0x80u) || ((held - dir * 8u - 8u) & 15u) == 0u;
    return guest->transition_swim && at_edge && s->link_enabled && held_across
        && !s->disable_screen_transitions && !s->screen_transition_delay && !s->hole_or_conveyor
        && s->game_state == 2u && s->cutscene_index <= 1u && !s->text_is_active
        && !(s->tileset_flags & (ORACLES_TILESETFLAG_SIDESCROLL | ORACLES_TILESETFLAG_UNDERWATER))
        && s->transition_state == TRANSITION_STATE_IDLE && s->scroll_mode == SCROLL_MODE_NORMAL
        && s->link_force_state == 0u && s->link_object_index == (ORACLES_OBJECTS_BASE >> 8) && s->link_id == 0u && s->link_state == LINK_STATE_NORMAL
        && link_unhindered(s) && (s->link_swimming_state & 0x4fu) == ORACLES_SWIMMING_NORMAL
        && (s->link_anim_mode == ORACLES_LINK_ANIM_MODE_SWIM || s->link_anim_mode == ORACLES_LINK_ANIM_MODE_DIVE);
}

/* Link on Ages' sea floor, with --continuous-swim: the game has him walk there, in his mermaid suit's gait. */
static int transition_floor_safe(const OraclesGuest *guest, const OraclesGuestTransitionState *s)
{
    return guest->transition_swim && transition_state_safe(s) && link_unhindered(s) && (s->tileset_flags & ORACLES_TILESETFLAG_UNDERWATER)
        && s->link_anim_mode == ORACLES_LINK_ANIM_MODE_WALK && !s->link_swimming_state;
}

static void read_transition_state(OraclesGuest *guest, OraclesGuestTransitionState *s)
{
    const OraclesGuestTables *t = guest->tables;
    const uint8_t *link = oracles_guest_object(guest, 0, 0);
    const uint8_t *io = oracles_guest_io(guest);
    memset(s, 0, sizeof *s);
    s->game = oracles_compat_family(oracles_guest_profile(guest));
    s->active_group = oracles_guest_read8(guest, t->active_group);
    s->room_is_large = oracles_guest_read8(guest, t->room_is_large);
    s->game_state = oracles_guest_read8(guest, t->game_state);
    s->cutscene_index = oracles_guest_read8(guest, t->cutscene_index);
    s->text_is_active = oracles_guest_read8(guest, t->text_is_active);
    s->tileset_flags = oracles_guest_read8(guest, t->tileset_flags);
    s->scroll_mode = oracles_guest_read8(guest, t->scroll_mode);
    s->transition_state = oracles_guest_read8(guest, t->screen_transition_state);
    s->transition_substate = oracles_guest_read8(guest, t->screen_transition_substate);
    s->transition_phase = oracles_guest_read8(guest, t->screen_transition_phase);
    s->transition_direction = oracles_guest_read8(guest, t->screen_transition_direction) & 3u;
    s->screen_scroll_delta = oracles_guest_read8(guest, t->screen_scroll_delta);
    s->screen_scroll_counter = oracles_guest_read8(guest, t->screen_scroll_counter);
    s->scroll_alignment = io ? (uint8_t)(io[(s->transition_direction & 1u) ? 0x43u : 0x42u] & 7u) : 0xffu;
    s->link_force_state = oracles_guest_read8(guest, t->link_force_state);
    s->link_object_index = oracles_guest_read8(guest, t->link_object_index);
    s->link_id = oracles_guest_read8(guest, t->link_id);
    s->link_state = link ? link[ORACLES_OBJ_STATE] : 0xffu;
    s->link_visible = oracles_guest_read8(guest, t->link_visible);
    s->link_anim_mode = oracles_guest_read8(guest, t->link_anim_mode);
    s->link_anim_counter = oracles_guest_read8(guest, t->link_anim_counter);
    s->link_anim_parameter = oracles_guest_read8(guest, t->link_anim_parameter);
    s->link_animation_frame = oracles_guest_read8(guest, t->link_animation_frame);
    s->link_anim_pointer = oracles_guest_read16(guest, t->link_anim_pointer);
    s->link_in_air = oracles_guest_read8(guest, t->link_in_air);
    s->link_swimming_state = oracles_guest_read8(guest, t->link_swimming_state);
    s->link_grab_state = oracles_guest_read8(guest, t->link_grab_state);
    s->magnet_glove_state = oracles_guest_read8(guest, t->magnet_glove_state);
    s->link_immobilized = oracles_guest_read8(guest, t->link_immobilized);
    s->link_pushing_direction = oracles_guest_read8(guest, t->link_pushing_direction);
    s->link_playing_instrument = oracles_guest_read8(guest, t->link_playing_instrument);
    s->link_turning_disabled = oracles_guest_read8(guest, t->link_turning_disabled);
    s->force_link_push_animation = oracles_guest_read8(guest, t->force_link_push_animation);
    s->using_shield = oracles_guest_read8(guest, t->using_shield);
    s->pegasus_seed_counter_nonzero = oracles_guest_read16(guest, t->pegasus_seed_counter) != 0;
    s->active_item = oracles_guest_read8(guest, t->parent_item2_enabled) != 0 || oracles_guest_read8(guest, t->parent_item3_enabled) != 0
                  || oracles_guest_read8(guest, t->parent_item4_enabled) != 0 || oracles_guest_read8(guest, t->parent_item5_enabled) != 0
                  || oracles_guest_read8(guest, t->weapon_item_enabled) != 0;
    s->link_invincibility_counter = oracles_guest_read8(guest, t->link_invincibility_counter);
    s->link_knockback_counter = oracles_guest_read8(guest, t->link_knockback_counter);
    s->link_stun_counter = oracles_guest_read8(guest, t->link_stun_counter);
    s->camera_x = oracles_guest_read16(guest, t->camera_x);
    s->camera_y = oracles_guest_read16(guest, t->camera_y);
    s->link_x = link ? (uint16_t)(link[ORACLES_OBJ_X] | (link[ORACLES_OBJ_XH] << 8)) : 0;
    s->link_y = link ? (uint16_t)(link[ORACLES_OBJ_Y] | (link[ORACLES_OBJ_YH] << 8)) : 0;
    s->room_width = oracles_guest_read8(guest, t->room_width);
    s->room_height = oracles_guest_read8(guest, t->room_height);
    s->room_collisions = s->scroll_mode == SCROLL_MODE_TRANSITION_LOADED ? oracles_guest_ptr(guest, t->room_collisions, 176) : NULL;
    s->room_layout = s->scroll_mode == SCROLL_MODE_TRANSITION_LOADED ? oracles_guest_ptr(guest, t->room_layout, 176) : NULL;
    s->active_collisions = oracles_guest_read8(guest, t->active_collisions);
    s->tile_types_bank = t->tile_types_table.bank == ORACLES_GUEST_ABSENT ? 0xffu : (uint8_t)t->tile_types_table.bank;
    s->tile_types_table = t->tile_types_table.addr;
    s->transition_direction_raw = oracles_guest_read8(guest, t->screen_transition_direction);
    s->link_enabled = link ? link[ORACLES_OBJ_ENABLED] : 0;
    s->link_move_angle = link ? link[ORACLES_OBJ_ANGLE] : 0xffu;
    s->link_input_angle = oracles_guest_read8(guest, t->link_angle);
    s->link_stroke = link ? link[ORACLES_OBJ_VAR35] : 0;
    s->screen_transition_delay = oracles_guest_read8(guest, t->screen_transition_delay);
    s->disable_screen_transitions = oracles_guest_read8(guest, t->disable_screen_transitions);
    s->hole_or_conveyor = oracles_guest_read8(guest, t->hole_or_conveyor);
    s->boundary_x = oracles_guest_read8(guest, t->screen_transition_boundary_x);
    s->boundary_y = oracles_guest_read8(guest, t->screen_transition_boundary_y);
    s->active_room = oracles_guest_read8(guest, t->active_room);
    s->active_tile_index = oracles_guest_read8(guest, t->active_tile_index);
    s->map_width = oracles_compat_map_width(oracles_guest_profile(guest));
    s->map_height = oracles_compat_map_height(oracles_guest_profile(guest));
    s->link_speed = link ? link[ORACLES_OBJ_SPEED] : 0;
    s->link_var2f = link ? link[ORACLES_OBJ_VAR2F] : 0;
    s->animation_data_bank = t->special_object_animation_table.bank == ORACLES_GUEST_ABSENT ? 0xffu : (uint8_t)t->special_object_animation_table.bank;
    s->palette_thread_mode = oracles_guest_read8(guest, t->palette_thread_mode);
    s->tileset_palette = oracles_guest_read8(guest, t->tileset_palette);
    s->loaded_tileset_palette = oracles_guest_read8(guest, t->loaded_tileset_palette);
    s->frame = guest->frame;
}

/* Moves Link's sprite entries with him.  Link's sprite is two 8x16 columns
 * on one line, 8 px apart, placed from his position less the camera's; the
 * game rebuilt them on the last frame it drew, so they stand at most a couple
 * of pixels behind him.  `buffer` is either the game's wOam (built once per
 * drawn frame, copied to the OAM at the next vblank) or the core's OAM (what
 * the LCD reads on a frame the game did not finish: no DMA runs then).  With
 * more than two entries near Link's place (an item, an effect), nothing moves.
 * Swimming, the game draws him lower in the water, up to `lower` px more. */
static void shift_link_sprites(uint8_t *buffer, const OraclesGuestTransitionState *s, int dx, int dy, unsigned lower)
{
    if (!buffer) return;
    const unsigned ax = (unsigned)((s->link_x >> 8) - s->camera_x) & 0xffu;
    const unsigned ay = (unsigned)((s->link_y >> 8) - s->camera_y + 24) & 0xffu;
    unsigned found[2], count = 0;
    for (unsigned i = 0; i < 40u; i++) {
        const uint8_t *e = buffer + i * 4u;
        if (e[0] == 0 || e[0] >= 160u || e[1] == 0) continue;
        const unsigned ddy = (unsigned)(e[0] - ay) & 0xffu, ddx = (unsigned)(e[1] - ax) & 0xffu;
        if (ddy > 2u + lower && ddy < 254u) continue;
        if (!(ddx <= 2u || ddx >= 253u || (ddx >= 5u && ddx <= 10u))) continue;
        if (count == 2u) return;
        found[count++] = i;
    }
    for (unsigned k = 0; k < count; k++) {
        uint8_t *e = buffer + found[k] * 4u;
        e[0] = (uint8_t)(e[0] + dy);
        e[1] = (uint8_t)(e[1] + dx);
    }
}

static void apply_transition_mutation(OraclesGuest *guest, const OraclesGuestTransitionState *s, const OraclesGuestTransitionMutation *m, int at_vblank)
{
    const OraclesGuestTables *t = guest->tables;
    int wrote = 0;
    /* The BG palettes refreshed from the room's own (refreshDirtyPalettes, the
     * palette sources cleared when the thread stopped), as the palette load
     * at the end of a scroll does: out of a transition, in normal play, the
     * thread stopped and the room's palette header the one loaded. */
    if (m->refresh_bg_palettes && s->game_state == 2u && s->cutscene_index <= 1u
        && (s->transition_state < TRANSITION_STATE_PREPARE || s->transition_state > TRANSITION_STATE_SCROLL)
        && s->palette_thread_mode == 0u && s->loaded_tileset_palette == s->tileset_palette
        && t->dirty_bg_palettes.addr >= 0xff80u && t->dirty_bg_palettes.addr <= 0xfffeu) {
        size_t size = 0;
        uint16_t bank = 0;
        uint8_t *hram = oracles_core_memory(guest->core, ORACLES_CORE_HRAM, &size, &bank);
        if (hram && size > (size_t)(t->dirty_bg_palettes.addr - 0xff80u)) { hram[t->dirty_bg_palettes.addr - 0xff80u] |= (uint8_t)(m->refresh_bg_palettes & 0xfcu); wrote = 1; }
    }
    /* The transition a swim's momentum takes Link to (--continuous-swim): wScreenTransitionDirection's bit 7, which
     * screenTransitionState2 reads first in normal play, raised to one of the four directions, or lowered when the
     * policy raised it and it was not taken; never at a vblank. */
    if (m->force_transition && !at_vblank && s->game_state == 2u && s->transition_state == TRANSITION_STATE_IDLE) {
        uint8_t *direction = bank0_field(guest, t->screen_transition_direction, 1);
        if (direction && m->force_transition == 1u && m->forced_direction <= 3u && edge_swim_safe(guest, s, m->forced_direction)) { *direction = (uint8_t)(0x80u | m->forced_direction); wrote = 1; }
        else if (direction && m->force_transition == 2u && guest->transition_swim && (*direction & 0x80u)) { *direction &= 0x7fu; wrote = 1; }
    }
    if (!transition_state_safe(s)) { if (wrote && at_vblank) guest->vblank_writes[s->transition_state & 7u]++; return; }
    const int walk_safe = transition_walk_safe(s), swim_safe = transition_swim_safe(guest, s), floor_safe = transition_floor_safe(guest, s);
    const int step_max = swim_safe || floor_safe ? SWIM_STEP_MAX : WALK_STEP_MAX;
    /* The scroll step: the game's own value or twice it, while the scroll runs. */
    if (m->set_scroll_delta && s->transition_state == TRANSITION_STATE_SCROLL && (s->transition_substate == 1u || s->transition_substate == 2u)) {
        const uint8_t v = m->scroll_delta;
        if (v == 4u || v == 8u || v == 0xfcu || v == 0xf8u) {
            uint8_t *delta = bank0_field(guest, t->screen_scroll_delta, 1);
            if (delta) { *delta = v; wrote = 1; }
        }
    }
    /* Link's position: at most a pixel a frame on each axis on foot, the fastest swim swimming. */
    if ((walk_safe || swim_safe || floor_safe) && (m->link_x_delta || m->link_y_delta)
        && m->link_x_delta >= -step_max && m->link_x_delta <= step_max && m->link_y_delta >= -step_max && m->link_y_delta <= step_max) {
        uint8_t *x = link_field(guest, (OraclesGuestSym){ ORACLES_OBJECTS_BANK, ORACLES_OBJECTS_BASE + ORACLES_OBJ_X }, 2);
        uint8_t *y = link_field(guest, (OraclesGuestSym){ ORACLES_OBJECTS_BANK, ORACLES_OBJECTS_BASE + ORACLES_OBJ_Y }, 2);
        int dx = 0, dy = 0;
        if (x && m->link_x_delta) { const uint16_t v = (uint16_t)((x[0] | (x[1] << 8)) + m->link_x_delta); dx = (int8_t)((v >> 8) - x[1]); x[0] = (uint8_t)v; x[1] = (uint8_t)(v >> 8); }
        if (y && m->link_y_delta) { const uint16_t v = (uint16_t)((y[0] | (y[1] << 8)) + m->link_y_delta); dy = (int8_t)((v >> 8) - y[1]); y[0] = (uint8_t)v; y[1] = (uint8_t)(v >> 8); }
        /* His sprite follows: in wOam, drawn this frame before the move and
         * copied at the coming vblank; in the core's OAM as well when the
         * game did not draw this frame (no copy comes, the LCD shows the OAM). */
        wrote = 1;
        if (dx || dy) {
            const unsigned lower = swim_safe ? ORACLES_LINK_SWIM_LOWER : 0u;
            shift_link_sprites(bank0_field(guest, t->oam, 160), s, dx, dy, lower);
            if (at_vblank) {
                size_t size = 0;
                uint16_t bank = 0;
                uint8_t *oam = oracles_core_memory(guest->core, ORACLES_CORE_OAM, &size, &bank);
                if (oam && size >= 160u) shift_link_sprites(oam, s, dx, dy, lower);
            }
        }
    }
    /* Link's walking, swimming or diving animation: counter, parameter, pointer into the animation bank, base frame. */
    if ((walk_safe || swim_safe || floor_safe) && m->update_walk_animation && s->animation_data_bank != 0xffu && m->link_anim_pointer >= 0x4000u && m->link_anim_pointer < 0x8000u) {
        uint8_t *counter = link_field(guest, t->link_anim_counter, 1), *parameter = link_field(guest, t->link_anim_parameter, 1);
        uint8_t *pointer = link_field(guest, t->link_anim_pointer, 2), *frame = link_field(guest, t->link_animation_frame, 1);
        if (counter && parameter && pointer && frame) {
            *counter = m->link_anim_counter; *parameter = m->link_anim_parameter; *frame = m->link_animation_frame;
            pointer[0] = (uint8_t)m->link_anim_pointer; pointer[1] = (uint8_t)(m->link_anim_pointer >> 8);
            wrote = 1;
        }
    }
    /* The writes made at a vblank, by transition state: the loads (3 and 4)
     * only, by construction; the harness reports the counts (docs/GAME_HOOKS.md,
     * section 6). */
    if (wrote && at_vblank) guest->vblank_writes[s->transition_state & 7u]++;
}

unsigned oracles_guest_vblank_policy_skipped(const OraclesGuest *guest) { return guest->vblank_policy_skipped; }

void oracles_guest_vblank_write_counts(const OraclesGuest *guest, unsigned out[8])
{
    memcpy(out, guest->vblank_writes, sizeof guest->vblank_writes);
}

void oracles_guest_apply_transition_policy(OraclesGuest *guest, int at_vblank)
{
    if (!guest->transition_policy) return;
    OraclesGuestTransitionState state;
    OraclesGuestTransitionMutation mutation;
    read_transition_state(guest, &state);
    /* At the vblank of a frame the main loop did not finish, the only writes
     * are those of a room load in progress (the transition's states 3 and 4:
     * the layout, the collisions, the unique graphics), whose code does not
     * touch the fields the transaction writes (docs/GAME_HOOKS.md, section 6).  A
     * scroll frame the game did not finish is left alone: its logic could
     * stand in the middle of Link's own move, and a write there could double
     * a carry.  The policy is not asked at all there: what it asks for, it
     * takes as done (a palette refresh, the arming of a scroll). */
    if (at_vblank && (state.transition_state < TRANSITION_STATE_PREPARE || state.transition_state >= TRANSITION_STATE_SCROLL)) {
        guest->vblank_policy_skipped++;
        guest->policy_not_asked++;
        return;
    }
    state.frames_not_asked = guest->policy_not_asked;
    guest->policy_not_asked = 0;
    memset(&mutation, 0, sizeof mutation);
    guest->transition_policy(guest->transition_opaque, &state, &mutation);
    apply_transition_mutation(guest, &state, &mutation, at_vblank);
}
