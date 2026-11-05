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

void oracles_guest_set_transition_steps(OraclesGuest *guest, int on)
{
    guest->transition_steps = on != 0;
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
#define SWIM_SPEED_MAX 0x41u           /* SPEED_1a0: past the fastest swim's cap (the mermaid suit with the swimmer's ring, a stroke on top) */
#define WARP_TILES_HELD 0x80u          /* wDisableWarpTiles as the bus sets it for the frame after a scroll it starts (the game's own value is 1) */

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

/* The frame whose logic ended a scroll (screenTransitionState5's last phase, finishScrollingTransition) after
 * linkState01 skipped Link's move, a scroll the policy was last asked in: back in normal play at the entry of its
 * drawAllSprites, before any frame of normal play has moved him (--continuous-steps). */
static int scroll_end_safe(const OraclesGuest *guest, const OraclesGuestTransitionState *s, const OraclesGuestTransitionMutation *m)
{
    return (m->scroll_end || m->set_respawn) && guest->transition_at_entry && guest->transition_last_state == TRANSITION_STATE_SCROLL
        && s->game_state == 2u && s->cutscene_index <= 1u && !s->text_is_active && !(s->tileset_flags & ORACLES_TILESETFLAG_SIDESCROLL)
        && s->transition_state == TRANSITION_STATE_IDLE && s->scroll_mode == SCROLL_MODE_NORMAL && s->transition_direction <= 3u && s->link_force_state == 0u
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

/* Link walking on foot, in a state safe as above (`state_safe`). */
static int transition_walk_safe(const OraclesGuestTransitionState *s, int state_safe)
{
    return state_safe && link_unhindered(s) && s->link_anim_mode == ORACLES_LINK_ANIM_MODE_WALK && !s->link_swimming_state
        && !(s->tileset_flags & ORACLES_TILESETFLAG_UNDERWATER);
}

/* Link swimming at the surface, with --continuous-swim. */
static int transition_swim_safe(const OraclesGuest *guest, const OraclesGuestTransitionState *s, int state_safe)
{
    return guest->transition_swim && state_safe && link_unhindered(s) && !(s->tileset_flags & ORACLES_TILESETFLAG_UNDERWATER)
        && (s->link_swimming_state & 0x4fu) == ORACLES_SWIMMING_NORMAL
        && (s->link_anim_mode == ORACLES_LINK_ANIM_MODE_SWIM || s->link_anim_mode == ORACLES_LINK_ANIM_MODE_DIVE);
}

/* --continuous-line: Link swimming at the surface in his normal swimming state, or on Ages' sea floor, where the game has
 * him walk, with the transitions' swim. */
static int swimming_back(const OraclesGuest *guest, const OraclesGuestTransitionState *s)
{
    if (!guest->transition_swim) return 0;
    if (s->tileset_flags & ORACLES_TILESETFLAG_UNDERWATER) return !s->link_swimming_state && s->link_anim_mode == ORACLES_LINK_ANIM_MODE_WALK;
    return (s->link_swimming_state & 0x4fu) == ORACLES_SWIMMING_NORMAL
        && (s->link_anim_mode == ORACLES_LINK_ANIM_MODE_SWIM || s->link_anim_mode == ORACLES_LINK_ANIM_MODE_DIVE);
}

/* Link swimming at the surface in normal play, no transition in progress (--continuous-swim), no direction held along the edge's axis,
 * at the edge of `dir` (a pixel short of screenTransitionState2's test, where it puts him back), and nothing of what
 * that test refuses first: the transition his momentum takes him to may be forced there, by the bit
 * screenTransitionState2 reads before any check of its own. */
static int edge_swim_safe(const OraclesGuest *guest, const OraclesGuestTransitionState *s, unsigned dir)
{
    const unsigned xh = s->link_x >> 8, yh = s->link_y >> 8;
    /* With the line, his next step takes him across it: within two pixels of it (guest_line.c), at the bottom of the
     * last row his own tile (y+5) is in; on a current the game's bound. */
    const int line = guest->transition_line && !s->hole_or_conveyor && !s->room_is_large;
    const int floor = (s->tileset_flags & ORACLES_TILESETFLAG_UNDERWATER) != 0;
    const unsigned top = line ? 1u : 6u, bottom = s->boundary_y + (line && floor ? 5u : 0u), right = s->boundary_x + (line ? 4u : 0u);
    const int at_edge = dir == 0u ? yh <= top : dir == 2u ? yh >= bottom && yh < 0xc0u : dir == 3u ? xh <= top : xh >= right && xh < 0xc0u;
    /* The direction held (wLinkAngle, from the pad: a multiple of 4): none, or square across the edge's axis, never
     * toward the edge or away from it, a diagonal included; with the line, anything but away from it. */
    const unsigned held = s->link_input_angle;
    const int held_across = (held & 0x80u) || ((held - dir * 8u - 8u) & 15u) == 0u;
    const int held_away = !(held & 0x80u) && ((held - dir * 8u + 16u + 4u) & 31u) <= 8u;
    return guest->transition_swim && at_edge && s->link_enabled && (line ? !held_away : held_across)
        && !s->disable_screen_transitions && !s->screen_transition_delay && !s->hole_or_conveyor
        && s->game_state == 2u && s->cutscene_index <= 1u && !s->text_is_active
        && !(s->tileset_flags & ORACLES_TILESETFLAG_SIDESCROLL) && (line || !floor)
        && s->transition_state == TRANSITION_STATE_IDLE && s->scroll_mode == SCROLL_MODE_NORMAL
        && s->link_force_state == 0u && s->link_object_index == (ORACLES_OBJECTS_BASE >> 8) && s->link_id == 0u && s->link_state == LINK_STATE_NORMAL
        && link_unhindered(s) && swimming_back(guest, s);
}

/* --continuous-line: the bottom line past the boundary, where a swimmer's own tile (y+5) is still the room's last row
 * (guest_line.c). */
static unsigned bottom_line(const OraclesGuestTransitionState *s) { return s->link_swimming_state ? 1u : 6u; }

/* --continuous-line: Link's coordinate along `dir` after a move of (`dx`, `dy`), past the line on that side of a small
 * room, in the room a scroll left: wrapped below zero at the top and the left (the upper half of what lies beyond, as
 * the line's tests take it, guest_line.c), past the room's last pixel at the bottom and the right. */
static int past_line(const OraclesGuestTransitionState *s, unsigned dir, int dx, int dy)
{
    const unsigned xh = ((unsigned)(s->link_x + dx) >> 8) & 0xffu, yh = ((unsigned)(s->link_y + dy) >> 8) & 0xffu;
    return dir == 0u ? yh >= 192u : dir == 3u ? xh >= 208u
         : dir == 2u ? yh > s->boundary_y + bottom_line(s) && yh < 192u : xh > s->boundary_x + 5u && xh < 208u;
}

/* --continuous-line: Link left past the line on the side of `dir` with no transition back to take him there (the bus
 * refused to force it, or the game has disabled the transitions since): put back on the line, his coordinate along it
 * the room's first or last pixel, as the game's own edge test would put him, and never past it. */
static int line_put_back(OraclesGuest *guest, const OraclesGuestTransitionState *s, unsigned dir, int dx, int dy)
{
    if (!guest->transition_line || s->room_is_large || dir > 3u || !past_line(s, dir, dx, dy)) return 0;
    uint8_t *field = link_field(guest, (OraclesGuestSym){ ORACLES_OBJECTS_BANK, ORACLES_OBJECTS_BASE + ((dir & 1u) ? ORACLES_OBJ_X : ORACLES_OBJ_Y) }, 2);
    if (!field) return 0;
    field[0] = 0;
    field[1] = dir == 0u || dir == 3u ? 0u : (uint8_t)(dir == 2u ? s->boundary_y + bottom_line(s) : s->boundary_x + 5u);
    guest->transition_put_back++;
    return 1;
}

/* --continuous-line, at the entry of the drawAllSprites that ends a scroll: Link on foot or swimming in his normal state, his step
 * of that frame (`dx`, `dy`) leaving him past the line on the side of `dir`, back in the room the scroll left (a
 * coordinate wrapped below zero at the top and the left, past the room's last pixel at the bottom and the right), the
 * transitions not disabled: the transition back may be forced from there. */
static int edge_walk_back_safe(const OraclesGuest *guest, const OraclesGuestTransitionState *s, unsigned dir, int dx, int dy)
{
    /* A jump's delay, a hole or a conveyor under him do not hold it back: the game's own forced transitions (a fall off
     * a cliff) pass them too, and putting him back on the line would be a jump of all he walked past it. */
    return guest->transition_line && guest->transition_at_entry && past_line(s, dir, dx, dy) && !s->room_is_large && s->link_enabled
        && !s->disable_screen_transitions
        && s->game_state == 2u && s->cutscene_index <= 1u && !s->text_is_active
        && !(s->tileset_flags & ORACLES_TILESETFLAG_SIDESCROLL)
        && s->transition_state == TRANSITION_STATE_IDLE && s->scroll_mode == SCROLL_MODE_NORMAL
        && s->link_force_state == 0u && s->link_object_index == (ORACLES_OBJECTS_BASE >> 8) && s->link_id == 0u && s->link_state == LINK_STATE_NORMAL
        && link_unhindered(s) && ((s->link_anim_mode == ORACLES_LINK_ANIM_MODE_WALK && !s->link_swimming_state && !(s->tileset_flags & ORACLES_TILESETFLAG_UNDERWATER))
                                  || swimming_back(guest, s));
}

/* Link on Ages' sea floor, with --continuous-swim: the game has him walk there, in his mermaid suit's gait. */
static int transition_floor_safe(const OraclesGuest *guest, const OraclesGuestTransitionState *s, int state_safe)
{
    return guest->transition_swim && state_safe && link_unhindered(s) && (s->tileset_flags & ORACLES_TILESETFLAG_UNDERWATER)
        && s->link_anim_mode == ORACLES_LINK_ANIM_MODE_WALK && !s->link_swimming_state;
}

static void read_transition_state(OraclesGuest *guest, OraclesGuestTransitionState *s, int at_vblank)
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
    /* The room entered's collisions and layout: loaded once wScrollMode is 8, but the transition's state 3 writes 8
     * before it loads them (loadTilesetAndRoomLayout, loadRoomCollisions): at the vblank of an unfinished state-3
     * frame they may still be the room left's, and count as not loaded. */
    const int loaded = s->scroll_mode == SCROLL_MODE_TRANSITION_LOADED && !(at_vblank && s->transition_state == TRANSITION_STATE_PREPARE);
    s->room_collisions = loaded ? oracles_guest_ptr(guest, t->room_collisions, 176) : NULL;
    s->room_layout = loaded ? oracles_guest_ptr(guest, t->room_layout, 176) : NULL;
    const int play = !at_vblank && s->transition_state == TRANSITION_STATE_IDLE && s->scroll_mode == SCROLL_MODE_NORMAL;
    s->play_collisions = play ? oracles_guest_ptr(guest, t->room_collisions, 176) : NULL;
    s->play_layout = play ? oracles_guest_ptr(guest, t->room_layout, 176) : NULL;
    s->active_collisions = oracles_guest_read8(guest, t->active_collisions);
    s->tile_types_bank = t->tile_types_table.bank == ORACLES_GUEST_ABSENT ? 0xffu : (uint8_t)t->tile_types_table.bank;
    s->tile_types_table = t->tile_types_table.addr;
    s->transition_direction_raw = oracles_guest_read8(guest, t->screen_transition_direction);
    s->link_enabled = link ? link[ORACLES_OBJ_ENABLED] : 0;
    s->link_move_angle = link ? link[ORACLES_OBJ_ANGLE] : 0xffu;
    s->link_input_angle = oracles_guest_read8(guest, t->link_angle);
    s->link_direction = link ? link[ORACLES_OBJ_DIRECTION] : 0u;
    s->link_stroke = link ? link[ORACLES_OBJ_VAR35] : 0;
    s->link_speed_tmp = oracles_guest_read8(guest, t->link_speed_cap);
    s->link_speed_counter = oracles_guest_read8(guest, t->link_speed_step_counter);
    s->link_speed_period = oracles_guest_read8(guest, t->link_speed_step_period);
    s->link_counter1 = link ? link[ORACLES_OBJ_COUNTER1] : 0;
    s->link_mermaid_stroke = oracles_guest_read8(guest, t->link_stroke_counter);
    s->game_keys_pressed = oracles_guest_read8(guest, t->game_keys_pressed);
    s->game_keys_just_pressed = oracles_guest_read8(guest, t->game_keys_just_pressed);
    s->swimmers_ring = (uint8_t)oracles_guest_swim_sped(guest);
    s->swim_hold = guest->swim_hold;
    s->ages_water = (uint8_t)oracles_compat_ages_water_rules(oracles_guest_profile(guest));
    s->game_frame = oracles_guest_read8(guest, t->frame_counter);
    s->screen_transition_delay = oracles_guest_read8(guest, t->screen_transition_delay);
    s->disable_screen_transitions = oracles_guest_read8(guest, t->disable_screen_transitions);
    s->use_simulated_input = oracles_guest_simulated_input(guest);
    s->hole_or_conveyor = oracles_guest_read8(guest, t->hole_or_conveyor);
    s->boundary_x = oracles_guest_read8(guest, t->screen_transition_boundary_x);
    s->boundary_y = oracles_guest_read8(guest, t->screen_transition_boundary_y);
    s->active_room = oracles_guest_read8(guest, t->active_room);
    s->active_tile_index = oracles_guest_read8(guest, t->active_tile_index);
    s->map_width = oracles_compat_group_map_width(oracles_guest_profile(guest), s->active_group);
    s->map_height = oracles_compat_group_map_height(oracles_guest_profile(guest), s->active_group);
    s->map_left = oracles_compat_group_map_left(oracles_guest_profile(guest), s->active_group);
    s->map_top = oracles_compat_group_map_top(oracles_guest_profile(guest), s->active_group);
    s->link_speed = link ? link[ORACLES_OBJ_SPEED] : 0;
    s->link_var2f = link ? link[ORACLES_OBJ_VAR2F] : 0;
    s->animation_data_bank = t->special_object_animation_table.bank == ORACLES_GUEST_ABSENT ? 0xffu : (uint8_t)t->special_object_animation_table.bank;
    s->animation_table = t->special_object_animation_table.addr;
    s->map_transition_bank = t->map_transition_group_table.bank == ORACLES_GUEST_ABSENT ? 0xffu : (uint8_t)t->map_transition_group_table.bank;
    s->map_transition_table = t->map_transition_group_table.addr;
    s->room_pack_bank = t->room_pack_data.bank == ORACLES_GUEST_ABSENT ? 0xffu : (uint8_t)t->room_pack_data.bank;
    s->room_pack_table = t->room_pack_data.addr;
    s->moblin_keep_bank = t->moblin_keep_rooms.bank == ORACLES_GUEST_ABSENT ? 0xffu : (uint8_t)t->moblin_keep_rooms.bank;
    s->moblin_keep_table = t->moblin_keep_rooms.addr;
    s->palette_thread_mode = oracles_guest_read8(guest, t->palette_thread_mode);
    s->tileset_palette = oracles_guest_read8(guest, t->tileset_palette);
    s->loaded_tileset_palette = oracles_guest_read8(guest, t->loaded_tileset_palette);
    s->load.tileset_layout = oracles_guest_read8(guest, t->tileset_layout);
    s->load.tileset_layout_group = oracles_guest_read8(guest, t->tileset_layout_group);
    s->load.loaded_tileset_layout = t->loaded_tileset_layout.bank != ORACLES_GUEST_ABSENT ? oracles_guest_read8(guest, t->loaded_tileset_layout) : 0xffu;
    /* getThisRoomFlags: the page of flagLocationGroupTable the group's rooms keep their flags in. */
    const unsigned page = oracles_compat_room_flag_page(oracles_guest_profile(guest), s->active_group);
    if (t->group0_room_flags.bank != ORACLES_GUEST_ABSENT && page * 0x100u + s->active_room < t->room_flags_size)
        s->load.room_flags = oracles_guest_read8(guest, (OraclesGuestSym){ t->group0_room_flags.bank, (uint16_t)(t->group0_room_flags.addr + page * 0x100u + s->active_room) });
    if (t->group0_room_flags.bank != ORACLES_GUEST_ABSENT && page * 0x100u + 0x100u <= t->room_flags_size)
        s->load.room_flags_page = oracles_guest_ptr(guest, (OraclesGuestSym){ t->group0_room_flags.bank, (uint16_t)(t->group0_room_flags.addr + page * 0x100u) }, 0x100u);
    s->load.objects = at_vblank ? NULL : oracles_guest_wram(guest, ORACLES_OBJECTS_BANK);
    s->load.room_state_modifier = oracles_guest_read8(guest, t->room_state_modifier);
    s->load.is_linked_game = oracles_guest_read8(guest, t->is_linked_game);
    if (t->portal_group.bank != ORACLES_GUEST_ABSENT) {
        const uint8_t *portal = oracles_guest_ptr(guest, t->portal_group, 3);   /* wPortalGroup, wPortalRoom, wPortalPos */
        if (portal) { s->load.portal_group = portal[0]; s->load.portal_room = portal[1]; s->load.portal_pos = portal[2]; }
    }
    s->load.warp_transition = oracles_guest_read8(guest, t->warp_transition);
    s->load.warp_dest_pos = oracles_guest_read8(guest, t->warp_dest_pos);
    s->load.link_time_warp_tile = oracles_guest_read8(guest, t->link_time_warp_tile);
    s->load.global_flags = oracles_guest_ptr(guest, t->global_flags, t->global_flags_size);
    s->load.global_flags_size = s->load.global_flags ? t->global_flags_size : 0;
    s->load.gasha_spots_planted = oracles_guest_ptr(guest, t->gasha_spots_planted, t->gasha_spots_planted_size);
    s->load.gasha_spots_planted_size = s->load.gasha_spots_planted ? t->gasha_spots_planted_size : 0;
    if (t->group0_room_flags.bank != ORACLES_GUEST_ABSENT) s->load.room_flags_all = oracles_guest_ptr(guest, t->group0_room_flags, t->room_flags_size);
    s->load.room_flags_all_size = s->load.room_flags_all ? t->room_flags_size : 0;
    if (t->obtained_treasure_flags.bank != ORACLES_GUEST_ABSENT) s->load.obtained_treasures = oracles_guest_ptr(guest, t->obtained_treasure_flags, t->obtained_treasure_flags_size);
    s->load.obtained_treasures_size = s->load.obtained_treasures ? t->obtained_treasure_flags_size : 0;
    {
        const struct { OraclesGuestSym sym; uint8_t *to; uint8_t bit; } bytes[] = {
            { t->gameboy_type, &s->load.gameboy_type, ORACLES_LOAD_GAMEBOY_TYPE }, { t->ricky_state, &s->load.ricky_state, ORACLES_LOAD_RICKY_STATE },
            { t->essences_obtained, &s->load.essences_obtained, ORACLES_LOAD_ESSENCES_OBTAINED },
            { t->animal_companion, &s->load.animal_companion, ORACLES_LOAD_ANIMAL_COMPANION },
            { t->bought_shop_items1, &s->load.bought_shop_items1, ORACLES_LOAD_BOUGHT_SHOP_ITEMS1 },
            { t->seed_tree_refilled_bitset, &s->load.seed_tree_refilled, ORACLES_LOAD_SEED_TREE_REFILLED },
        };
        for (unsigned i = 0; i < sizeof bytes / sizeof *bytes; i++) {
            const uint8_t *at = oracles_guest_ptr(guest, bytes[i].sym, 1);
            if (at) { *bytes[i].to = *at; s->load.given |= bytes[i].bit; }
        }
    }
    s->load.room_code_replay = (uint8_t)oracles_compat_room_code_replay(oracles_guest_profile(guest));
    s->load.tileset_override_rules = (uint8_t)oracles_compat_tileset_override_rules(oracles_guest_profile(guest));
    s->load.chest_opened_tile = t->tileindex_chest_opened;
    s->load.roomflag_item = t->roomflag_item;
    s->load.timewarp_transition = t->transition_dest_timewarp;
    s->load.pollution_fixed_flag = t->globalflag_water_pollution_fixed;
    s->load.finished_game_flag = t->coarse_flag_finished_game;
    s->load.temple_lava_flag = t->coarse_flag_temple_lava;
    s->load.pirate_ship_flag = t->coarse_flag_pirate_ship;
    s->load.intro_done_flag = t->coarse_flag_intro_done;
    s->frame = guest->frame;
}


/* Moves Link's sprite entries with him.  Link's sprite is two 8x16 columns
 * on one line, 8 px apart, placed from his position less the camera's; the
 * game rebuilt them on the last frame it drew, so they stand at most a couple
 * of pixels behind him.  `buffer` is either the game's wOam (built once per
 * drawn frame, copied to the OAM at the next vblank) or the core's OAM (what
 * the LCD reads on a frame the game did not finish: no DMA runs then).  With
 * more than two entries near Link's place (an item, an effect), nothing moves;
 * the status bar's own sprites (its item icons, under its 16 lines) are not
 * counted, which stand at his place when a carry takes him past the room's
 * top.  Swimming, the game draws him lower in the water, up to `lower` px more.
 * `by_tiles`: his own graphics' entries alone, tiles $70-$73 of VRAM bank 1, in
 * the water, where the splashes he leaves stand at his place, and where the game
 * stepped him itself before a vblank it did not finish. */
static void shift_link_sprites(uint8_t *buffer, const OraclesGuestTransitionState *s, int dx, int dy, unsigned lower, int by_tiles)
{
    if (!buffer) return;
    const unsigned ax = (unsigned)((s->link_x >> 8) - s->camera_x) & 0xffu;
    const unsigned ay = (unsigned)((s->link_y >> 8) - s->camera_y + 24) & 0xffu;
    unsigned found[2], count = 0;
    for (unsigned i = 0; i < 40u; i++) {
        const uint8_t *e = buffer + i * 4u;
        if (e[0] == 0 || e[0] >= 160u || e[1] == 0) continue;
        const int own = (e[2] & 0xfcu) == ORACLES_LINK_TILES && (e[3] & ORACLES_OAM_ATTR_VRAM_BANK1);
        if ((e[0] < 32u || by_tiles) && !own) continue;
        const unsigned ddy = (unsigned)(e[0] - ay) & 0xffu, ddx = (unsigned)(e[1] - ax) & 0xffu;
        if (ddy > 2u + lower && ddy < 254u) continue;
        if (!(ddx <= 2u || ddx >= 253u || (ddx >= 5u && ddx <= 10u))) continue;
        if (count == 2u) return;
        found[count++] = i;
    }
    /* None there, the game's camera already the room entered's at a scroll chained to the last one: his own graphics'
     * entries wherever they stand, two at most, on one line. */
    if (!count && by_tiles) {
        for (unsigned i = 0; i < 40u; i++) {
            const uint8_t *e = buffer + i * 4u;
            if (e[0] == 0 || e[0] >= 160u || (e[2] & 0xfcu) != ORACLES_LINK_TILES || !(e[3] & ORACLES_OAM_ATTR_VRAM_BANK1)) continue;
            if (count == 2u || (count && e[0] != buffer[found[0] * 4u])) return;
            found[count++] = i;
        }
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
    int wrote = 0, put_back = 0;
    /* The warp tiles held off for the frame after a scroll the bus started (below): given back once the game has named
     * the room the scroll enters (wScrollMode 8), func_60e9 behind it. */
    uint8_t *warp_tiles = bank0_field(guest, t->disable_warp_tiles, 1);
    if (warp_tiles && *warp_tiles == WARP_TILES_HELD && s->scroll_mode == SCROLL_MODE_TRANSITION_LOADED) { *warp_tiles = 0; wrote = 1; }
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
        if (direction && m->force_transition == 1u && m->forced_direction <= 3u
            && (edge_swim_safe(guest, s, m->forced_direction) || edge_walk_back_safe(guest, s, m->forced_direction, m->link_x_delta, m->link_y_delta))) {
            *direction = (uint8_t)(0x80u | m->forced_direction);
            wrote = 1;
            if (guest->transition_line && guest->transition_at_entry && !m->set_swim) guest->transition_back_forced++;
        }
        /* The transition back refused at the frame that ends the scroll: never left past the line. */
        else if (m->force_transition == 1u && guest->transition_line && guest->transition_at_entry
                 && line_put_back(guest, s, m->forced_direction, m->link_x_delta, m->link_y_delta)) { wrote = put_back = 1; }
        else if (direction && m->force_transition == 2u && (guest->transition_swim || guest->transition_line) && (*direction & 0x80u)) {
            *direction &= 0x7fu;
            wrote = 1;
            if (line_put_back(guest, s, m->forced_direction, 0, 0)) put_back = 1;
        }
        /* The scroll into the room beside, Link carried past the line across at the frame that ends a scroll: started
         * there as @startTransition starts one (wScrollMode 4, wScreenTransitionState 3, the direction), so that Link is
         * not moved, nor his tile and his walls read, by a frame of normal play outside the room entered.  The frame
         * after is still one of the game's logic, whose warps (func_60e9) read the tile under him: past the top line
         * that is the scripts' variables, past wRoomLayout's last row; the warp tiles are held off for it
         * (wDisableWarpTiles), unless the game holds them off already.  Never left past the line otherwise. */
        else if (m->force_transition == 3u && guest->transition_line && guest->transition_at_entry) {
            uint8_t *scroll_mode = bank0_field(guest, t->scroll_mode, 1), *state = bank0_field(guest, t->screen_transition_state, 1);
            if (direction && scroll_mode && state && m->forced_direction <= 3u && edge_walk_back_safe(guest, s, m->forced_direction, m->link_x_delta, m->link_y_delta)) {
                *scroll_mode = SCROLL_MODE_TRANSITION_DECIDED;
                *state = TRANSITION_STATE_PREPARE;
                *direction = m->forced_direction;
                if (warp_tiles && !*warp_tiles) *warp_tiles = WARP_TILES_HELD;
                wrote = 1;
                guest->transition_cross_started++;
            } else if (line_put_back(guest, s, m->forced_direction, m->link_x_delta, m->link_y_delta)) wrote = put_back = 1;
        }
    }
    const int state_safe = transition_state_safe(s) || scroll_end_safe(guest, s, m);
    if (!state_safe) { if (wrote && at_vblank) guest->vblank_writes[s->transition_state & 7u]++; return; }
    const int walk_safe = transition_walk_safe(s, state_safe), swim_safe = transition_swim_safe(guest, s, state_safe), floor_safe = transition_floor_safe(guest, s, state_safe);
    const int step_max = swim_safe || floor_safe ? (guest->transition_line ? ORACLES_TRANSITION_SWIM_LINE_STEP_MAX : ORACLES_TRANSITION_SWIM_STEP_MAX)
                       : guest->transition_line ? ORACLES_TRANSITION_LINE_STEP_MAX : ORACLES_TRANSITION_WALK_STEP_MAX;
    /* The scroll step: the game's own value or twice it, while the scroll runs. */
    if (m->set_scroll_delta && s->transition_state == TRANSITION_STATE_SCROLL && (s->transition_substate == 1u || s->transition_substate == 2u)) {
        const uint8_t v = m->scroll_delta;
        if (v == 4u || v == 8u || v == 0xfcu || v == 0xf8u) {
            uint8_t *delta = bank0_field(guest, t->screen_scroll_delta, 1);
            if (delta) { *delta = v; wrote = 1; }
        }
    }
    /* Link's position: at most a pixel a frame on each axis on foot, the fastest swim swimming. */
    if (!put_back && (walk_safe || swim_safe || floor_safe) && (m->link_x_delta || m->link_y_delta)
        && m->link_x_delta >= -step_max && m->link_x_delta <= step_max && m->link_y_delta >= -step_max && m->link_y_delta <= step_max) {
        uint8_t *x = link_field(guest, (OraclesGuestSym){ ORACLES_OBJECTS_BANK, ORACLES_OBJECTS_BASE + ORACLES_OBJ_X }, 2);
        uint8_t *y = link_field(guest, (OraclesGuestSym){ ORACLES_OBJECTS_BANK, ORACLES_OBJECTS_BASE + ORACLES_OBJ_Y }, 2);
        int dx = 0, dy = 0;
        if (x && m->link_x_delta) { const uint16_t v = (uint16_t)((x[0] | (x[1] << 8)) + m->link_x_delta); dx = (int8_t)((v >> 8) - x[1]); x[0] = (uint8_t)v; x[1] = (uint8_t)(v >> 8); }
        if (y && m->link_y_delta) { const uint16_t v = (uint16_t)((y[0] | (y[1] << 8)) + m->link_y_delta); dy = (int8_t)((v >> 8) - y[1]); y[0] = (uint8_t)v; y[1] = (uint8_t)(v >> 8); }
        if (m->carried && x && y) guest->transition_carried++;
        if (m->scroll_end && !transition_state_safe(s)) guest->transition_end_steps++;
        /* His sprite follows: in wOam, drawn this frame before the move and
         * copied at the coming vblank; in the core's OAM as well when the
         * game did not draw this frame (no copy comes, the LCD shows the OAM).
         * There both still draw him where the last frame the game finished
         * left him, short of its own last step, which no frame draws until the
         * load is over: they go where he now is. */
        wrote = 1;
        if (at_vblank && x && y) {
            const uint16_t nx = (uint16_t)(x[0] | (x[1] << 8)), ny = (uint16_t)(y[0] | (y[1] << 8));
            dx = (int8_t)((nx >> 8) - (guest->link_next_x >> 8));
            dy = (int8_t)((ny >> 8) - (guest->link_next_y >> 8));
            guest->link_next_x = nx;
            guest->link_next_y = ny;
        }
        /* At the entry of drawAllSprites the game draws him where he now is. */
        if ((dx || dy) && !guest->transition_at_entry) {
            const unsigned lower = swim_safe ? ORACLES_LINK_SWIM_LOWER : 0u;
            shift_link_sprites(bank0_field(guest, t->oam, 160), s, dx, dy, lower, swim_safe || floor_safe);
            if (at_vblank) {
                size_t size = 0;
                uint16_t bank = 0;
                uint8_t *oam = oracles_core_memory(guest->core, ORACLES_CORE_OAM, &size, &bank);
                if (oam && size >= 160u) shift_link_sprites(oam, s, dx, dy, lower, swim_safe || floor_safe);
            }
        }
    }
    /* At the vblank of a frame the game did not finish, no move asked, Link walking or in the water: the frame that
     * starts a transition, whose own step the game made before its load ran past the vblank, leaves the OAM where the
     * frame before drew him.  The next scan would show him there once more, still, and the next move's shift take him
     * two steps at once: his sprite goes where the game's step took him, as it follows the policy's, his graphics'
     * entries alone (an item or an effect beside him stays). */
    if (at_vblank && !put_back && (walk_safe || swim_safe || floor_safe) && !m->link_x_delta && !m->link_y_delta && !guest->transition_at_entry) {
        const uint8_t *link = oracles_guest_object(guest, 0, 0);
        const uint16_t nx = link ? (uint16_t)(link[ORACLES_OBJ_X] | (link[ORACLES_OBJ_XH] << 8)) : guest->link_next_x;
        const uint16_t ny = link ? (uint16_t)(link[ORACLES_OBJ_Y] | (link[ORACLES_OBJ_YH] << 8)) : guest->link_next_y;
        const int dx = (int8_t)((nx >> 8) - (guest->link_next_x >> 8)), dy = (int8_t)((ny >> 8) - (guest->link_next_y >> 8));
        const int most = (step_max >> 8) + 1;
        if ((dx || dy) && dx >= -most && dx <= most && dy >= -most && dy <= most) {
            const unsigned lower = swim_safe ? ORACLES_LINK_SWIM_LOWER : 0u;
            OraclesGuestTransitionState drawn = *s;
            drawn.link_x = guest->link_next_x;
            drawn.link_y = guest->link_next_y;
            size_t size = 0;
            uint16_t bank = 0;
            uint8_t *oam = oracles_core_memory(guest->core, ORACLES_CORE_OAM, &size, &bank);
            shift_link_sprites(bank0_field(guest, t->oam, 160), &drawn, dx, dy, lower, 1);
            if (oam && size >= 160u) shift_link_sprites(oam, &drawn, dx, dy, lower, 1);
            guest->link_next_x = nx;
            guest->link_next_y = ny;
            wrote = 1;
        }
    }
    /* His respawn in the room entered (--continuous-steps), which finishScrollingTransition took from where he then
     * stood: within the room, from 1 to the boundary past which the game starts a transition down or right, as the game
     * itself may leave him there; the room small. */
    if (guest->transition_steps && m->set_respawn && scroll_end_safe(guest, s, m) && !s->room_is_large
        && m->respawn_y && m->respawn_y <= s->boundary_y && m->respawn_x && m->respawn_x <= s->boundary_x) {
        uint8_t *respawn = bank0_field(guest, t->link_local_respawn, 2);
        if (respawn) {
            if (respawn[0] != m->respawn_y || respawn[1] != m->respawn_x) guest->transition_respawns++;
            respawn[0] = m->respawn_y;
            respawn[1] = m->respawn_x;
            wrote = 1;
        }
    }
    /* The way he faces, on foot in the scroll itself (--continuous-steps): what updateLinkDirectionFromAngle would make
     * of the keys, which the game takes up after the scroll, and which loadLinkAndCompanionAnimationFrame adds to his
     * frame at the next one. */
    if (guest->transition_steps && ((walk_safe && transition_state_safe(s)) || (guest->transition_line && (swim_safe || floor_safe))) && m->set_facing && m->facing <= 3u) {
        uint8_t *direction = link_field(guest, (OraclesGuestSym){ ORACLES_OBJECTS_BANK, ORACLES_OBJECTS_BASE + ORACLES_OBJ_DIRECTION }, 1);
        if (direction) { *direction = m->facing; wrote = 1; }
    }
    /* His swim's state, as the game's swim routines would have stepped it (--continuous-line): a stroke of the flippers
     * in one of its three states, a speed and its cap within the fastest swim's, an angle or none. */
    if (guest->transition_line && m->set_swim && (swim_safe || floor_safe) && m->swim.speed <= SWIM_SPEED_MAX && m->swim.speed_tmp <= SWIM_SPEED_MAX
        && m->swim.stroke <= 2u && ((m->swim.angle & 0x80u) ? m->swim.angle == 0xffu : m->swim.angle < 32u)) {
        uint8_t *link = link_field(guest, (OraclesGuestSym){ ORACLES_OBJECTS_BANK, ORACLES_OBJECTS_BASE }, 0x40);
        uint8_t *cap = link_field(guest, t->link_speed_cap, 1), *counter = link_field(guest, t->link_speed_step_counter, 1);
        uint8_t *period = link_field(guest, t->link_speed_step_period, 1), *mermaid = link_field(guest, t->link_stroke_counter, 1);
        if (link && cap && counter && period && mermaid) {
            link[ORACLES_OBJ_ANGLE] = m->swim.angle;
            link[ORACLES_OBJ_SPEED] = m->swim.speed;
            link[ORACLES_OBJ_VAR35] = m->swim.stroke;
            link[ORACLES_OBJ_COUNTER1] = m->swim.counter1;
            *cap = m->swim.speed_tmp;
            *counter = m->swim.counter;
            *period = m->swim.period;
            *mermaid = m->swim.mermaid_stroke;
            guest->transition_swim_steps++;
            wrote = 1;
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

int oracles_guest_swim_sped(OraclesGuest *guest)
{
    const OraclesGuestTables *t = guest->tables;
    if (t->swim_speed_treasure == 0xffu) return t->active_ring.bank != ORACLES_GUEST_ABSENT && oracles_guest_read8(guest, t->active_ring) == t->swimmers_ring;
    /* checkTreasureObtained: the treasure's bit in wObtainedTreasureFlags. */
    const uint8_t *treasures = t->obtained_treasure_flags.bank != ORACLES_GUEST_ABSENT ? oracles_guest_ptr(guest, t->obtained_treasure_flags, t->obtained_treasure_flags_size) : NULL;
    return treasures && (unsigned)(t->swim_speed_treasure >> 3) < t->obtained_treasure_flags_size && ((treasures[t->swim_speed_treasure >> 3] >> (t->swim_speed_treasure & 7u)) & 1u);
}

void oracles_guest_apply_transition_entry(OraclesGuest *guest)
{
    if (!guest->transition_policy || !guest->transition_steps || guest->transition_last_state != TRANSITION_STATE_SCROLL) return;
    const OraclesGuestTables *t = guest->tables;
    if (oracles_guest_read8(guest, t->screen_transition_state) != TRANSITION_STATE_IDLE || oracles_guest_read8(guest, t->scroll_mode) != SCROLL_MODE_NORMAL) return;
    guest->transition_at_entry = 1;
    oracles_guest_apply_transition_policy(guest, 0);
}
unsigned oracles_guest_transition_carried(const OraclesGuest *guest) { return guest->transition_carried; }
unsigned oracles_guest_transition_end_steps(const OraclesGuest *guest) { return guest->transition_end_steps; }
unsigned oracles_guest_transition_respawns(const OraclesGuest *guest) { return guest->transition_respawns; }
unsigned oracles_guest_transition_back_forced(const OraclesGuest *guest) { return guest->transition_back_forced; }
unsigned oracles_guest_transition_put_back(const OraclesGuest *guest) { return guest->transition_put_back; }
unsigned oracles_guest_transition_cross_started(const OraclesGuest *guest) { return guest->transition_cross_started; }
unsigned oracles_guest_transition_swim_steps(const OraclesGuest *guest) { return guest->transition_swim_steps; }
void oracles_guest_set_transition_last_state(OraclesGuest *guest, uint8_t state) { guest->transition_last_state = state; }

void oracles_guest_vblank_write_counts(const OraclesGuest *guest, unsigned out[8])
{
    memcpy(out, guest->vblank_writes, sizeof guest->vblank_writes);
}

void oracles_guest_apply_transition_policy(OraclesGuest *guest, int at_vblank)
{
    if (!guest->transition_policy) return;
    OraclesGuestTransitionState state;
    OraclesGuestTransitionMutation mutation;
    read_transition_state(guest, &state, at_vblank);
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
    guest->transition_last_state = state.transition_state;
}
