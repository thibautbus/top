/* The continuous transition policy itself, frame by frame, through synthetic
 * scrolling transitions: room_transition.c built alone, with the few guest
 * calls it makes answered here, so that the policy the guest would install is
 * the one driven.  A transition is a number of frames of the room's load
 * (state 3, the collisions of the room entered given or not), the frame the
 * scroll is set up on (phase 1, its step the game's), the frame drawing its
 * first row, its steps a frame each (Link moved $80 or $60 along the axis,
 * the camera by the step the policy leaves in wcd14, a row drawn and the
 * counter decremented on each landing on a multiple of 8, the scroll over on
 * the one after it reaches zero), frames finishing it, and the frame whose
 * logic ends it, back in normal play, Link's coordinates rebased into the room
 * entered's. */
#include "room_transition.h"
#include "room_predict_fixture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static OraclesGuestTransitionFn installed;
static OraclesGuestTransitionResetFn installed_reset;
static void *installed_opaque;
const OraclesCompatProfile *oracles_guest_profile(const OraclesGuest *guest) { (void)guest; return NULL; }
int oracles_compat_continuous_transitions(const OraclesCompatProfile *profile) { (void)profile; return 1; }
int oracles_compat_continuous_swim(const OraclesCompatProfile *profile) { (void)profile; return 1; }
int oracles_compat_continuous_diagonal(const OraclesCompatProfile *profile) { (void)profile; return 1; }
int oracles_compat_continuous_steps(const OraclesCompatProfile *profile) { (void)profile; return 1; }
static const OraclesTables *predict_tables;   /* the tables the prediction reads, NULL: the profile does not qualify it */
int oracles_compat_predict_collisions(const OraclesCompatProfile *profile) { (void)profile; return predict_tables != NULL; }
const OraclesTables *oracles_compat_data_tables(const OraclesCompatProfile *profile) { (void)profile; return predict_tables; }
void oracles_guest_set_transition_swim(OraclesGuest *guest, int on) { (void)guest; (void)on; }
void oracles_guest_set_transition_steps(OraclesGuest *guest, int on) { (void)guest; (void)on; }
int oracles_compat_continuous_line(const OraclesCompatProfile *profile) { (void)profile; return 1; }
int oracles_compat_ages_water_rules(const OraclesCompatProfile *profile) { (void)profile; return 1; }
int oracles_guest_set_transition_line(OraclesGuest *guest, int on) { (void)guest; (void)on; return 0; }
static uint8_t bus_last_state;   /* what the policy's savestate data gives back to the bus */
void oracles_guest_set_transition_last_state(OraclesGuest *guest, uint8_t state) { (void)guest; bus_last_state = state; }
void oracles_guest_set_transition_policy(OraclesGuest *guest, OraclesGuestTransitionFn policy, OraclesGuestTransitionResetFn reset, void *opaque)
{
    (void)guest;
    installed = policy; installed_reset = reset; installed_opaque = opaque;
}

typedef struct run {
    unsigned dir;
    uint8_t angle;              /* wLinkAngle held */
    int x, y;                   /* 8.8, in the room left */
    int loading, post;          /* frames of the room's load, frames after the scroll */
    const uint8_t *collisions;  /* the room entered's */
    int unknown;                /* the collisions given NULL at every frame */
    int reset_at;               /* a savestate's load before that frame (-1 none) */
    int sea_floor;              /* Ages' sea floor, moving at `move_angle` and `speed` ($28, a pixel a frame, when 0) */
    uint8_t move_angle;
    int blind;                  /* the first frames of the load given no collisions (0: the first alone) */
    uint8_t speed;
    int restore;                /* the savestate carries the policy's progress, saved before the frame `reset_at` */
    const uint8_t *layout;      /* the room entered's tiles, given with its collisions */
    uint8_t room;               /* the room entered (wActiveRoom), group 0 */
    int unnamed;                /* the load's first frames before the game names the room (wScrollMode 4) */
    int end;                    /* the frame that ends the scroll played too */
    int game_x, game_y;         /* the game's own step in the frame that starts the transition, its put back on the edge included (8.8) */
    int release_at;             /* the direction let go from that frame of the transition on (0: held to the end) */
    int chained;                /* the transition starts at the frame after the last play's end, with no frame of normal play between (a corner) */
} run;
static int back_at;
static const uint8_t *left_room;   /* the room left's collisions, as the frame of normal play before the transition gives them (NULL: none) */
static int end_forced = -1;        /* the direction the frame that ends the scroll forced the next transition to, -1 none */
static int end_started = -1;       /* the direction of the transition it started there (--continuous-line, the room beside), -1 none */
static const uint8_t *flags_page;  /* the rooms' flags of group 0, with the overworld's map and room packs of the fixture (NULL: none) */
static int disable_at;             /* the frame of the transition from which wDisableScreenTransitions is set (0: never) */
static uint16_t animation_table;   /* specialObjectAnimationTable in bank 6, 0: none */
static uint8_t raised_floor;       /* wLinkRaisedFloorOffset (Ages), 0: none */
static int last_pose = -1;         /* the base frame the last animation written gave Link, -1 none */             /* the direction turned the other way from that frame of the transition on (0: never) */

static OraclesRoomTransition *policy_under_test;
static uint32_t frame_counter = 100;
static int largest_across;      /* the largest move across the axis asked in a frame of the last play */
static int animated;            /* its frames that advanced Link's animation */
static int scroll_steps;        /* the machine's steps in it */
static int end_moved;           /* its frame that ends the scroll moved Link */
static int still_after_release; /* its frames from the one the direction was let go, to the scroll's last step, that left Link still along the axis */
static int facing_last = -1;    /* the way the last play turned Link (w1Link.direction written), -1 none */
static int fastest_released;    /* his largest move along the axis in a frame from the one the direction was let go, the machine's included */
static int still_across;        /* its frames after the first, the frame that ends the scroll included, a direction held across, that left Link still across */
static int largest_along;       /* the largest move along the axis asked in a frame */
static int end_respawn;         /* the frame that ends the scroll asked for Link's respawn: end_respawn_y, end_respawn_x */
static int end_respawn_y, end_respawn_x;

/* Link's position at the end, 8.8, unwrapped, in the room left's coordinates. */
/* The frame after the one that ends a scroll: normal play (state 2) or a transition the game took (state 3), with
 * wScreenTransitionDirection as `raw`; what the policy asks. */
static OraclesGuestTransitionMutation next_frame_at(uint8_t raw, uint8_t state, uint16_t x, int disabled)
{
    OraclesGuestTransitionState s;
    memset(&s, 0, sizeof s);
    s.game = ORACLES_GAME_AGES; s.ages_water = 1; s.game_state = 2; s.cutscene_index = 1; s.scroll_mode = state == 2u ? 1 : 4;
    s.transition_state = state; s.transition_direction = (uint8_t)(raw & 3u); s.transition_direction_raw = raw;
    s.link_object_index = 0xd0; s.link_state = 1; s.link_visible = 0x80; s.link_anim_mode = 0x10; s.animation_data_bank = 6;
    s.link_anim_pointer = 0x4000; s.link_input_angle = 0x80; s.tile_types_bank = 0xff; s.boundary_x = 154; s.boundary_y = 121;
    s.link_x = x; s.link_y = 60 << 8; s.disable_screen_transitions = (uint8_t)disabled;
    s.frame = frame_counter++;
    OraclesGuestTransitionMutation m;
    memset(&m, 0, sizeof m);
    installed(installed_opaque, &s, &m);
    return m;
}

static OraclesGuestTransitionMutation next_frame(uint8_t raw, uint8_t state) { return next_frame_at(raw, state, 80 << 8, 0); }

static void play(const run *r, int *out_x, int *out_y)
{
    OraclesGuestTransitionState s;
    memset(&s, 0, sizeof s);
    s.game = ORACLES_GAME_AGES; s.ages_water = 1; s.game_state = 2; s.cutscene_index = 1; s.scroll_mode = 8;
    s.transition_direction = (uint8_t)r->dir; s.link_object_index = 0xd0; s.link_state = 1; s.link_visible = 0x80;
    s.link_anim_mode = 0x10; s.animation_data_bank = 6; s.link_anim_pointer = 0x4000; s.animation_table = animation_table;
    s.map_transition_bank = 5; s.map_transition_table = 0x7000;   /* every group's list empty (main) */
    last_pose = -1;
    s.active_room = r->room;
    s.boundary_x = 154; s.boundary_y = 121;   /* a small room's (wScreenTransitionBoundaryX/Y) */
    if (flags_page) {
        static const uint8_t no_objects[0x1000];
        s.load.room_flags_page = flags_page; s.load.objects = no_objects; s.map_width = 14; s.map_height = 14;
        s.room_pack_bank = FIXTURE_BANK; s.room_pack_table = FIXTURE_ROOM_PACKS; s.tileset_flags = 0x01;
    }
    s.link_input_angle = r->angle; s.link_move_angle = r->sea_floor ? r->move_angle : r->angle; s.tile_types_bank = 0xff;
    s.link_raised_floor = raised_floor;
    if (r->sea_floor) { s.tileset_flags = ORACLES_TILESETFLAG_UNDERWATER; s.link_var2f = 0xc0; s.link_speed = r->speed ? r->speed : 0x28; }
    int x = r->x, y = r->y;
    const int horizontal = r->dir & 1u, rows = horizontal ? 20 : 16, step = horizontal ? 0x60 : 0x80;
    const int sign = (r->dir == 1u || r->dir == 2u) ? 1 : -1;
    uint16_t camera = 0;
    int phase = 0, counter = rows, delta = sign * 4, post = 0;
    if (!r->chained) installed_reset(installed_opaque);
    else frame_counter -= 10;
    largest_across = animated = scroll_steps = end_moved = still_after_release = fastest_released = 0;
    still_across = largest_along = end_respawn = 0;
    facing_last = -1;
    s.link_direction = (uint8_t)r->dir;
    /* A frame of normal play first, as the policy sees one before any transition. */
    s.transition_state = 2; s.scroll_mode = 1; s.frame = r->chained ? 0 : frame_counter++;
    s.play_collisions = left_room;
    end_forced = end_started = -1;
    s.link_x = (uint16_t)x; s.link_y = (uint16_t)y;
    if (!r->chained) {
        OraclesGuestTransitionMutation m;
        memset(&m, 0, sizeof m);
        installed(installed_opaque, &s, &m);
    }
    s.scroll_mode = 8;
    for (int f = 0; post < r->post || phase < 3; f++) {
        if (f == r->reset_at) {
            uint8_t saved[ORACLES_ROOM_TRANSITION_STATE_SIZE];
            const size_t size = oracles_room_transition_save_state(policy_under_test, saved, sizeof saved);
            CHECK(size == sizeof saved);
            installed_reset(installed_opaque);
            if (r->restore) CHECK(oracles_room_transition_load_state(policy_under_test, saved, size) == 0);
            frame_counter += 7;   /* the frames the load came after, in the host's count */
        }
        int machine = 0;
        s.frame = frame_counter++;
        if (f == 0) { x += r->game_x; y += r->game_y; }
        if (r->release_at && f == r->release_at) { s.link_input_angle = 0x80; if (!r->sea_floor) s.link_move_angle = 0x80; }
        if (back_at && f == back_at) s.link_input_angle = s.link_move_angle = (uint8_t)((r->angle + 16u) & 31u);
        s.disable_screen_transitions = (uint8_t)(disable_at && f >= disable_at);
        s.link_anim_counter = 3;
        s.room_collisions = r->unknown ? NULL : r->collisions;
        s.room_layout = s.room_collisions ? r->layout : NULL;
        s.scroll_mode = f < r->unnamed ? 4 : 8;
        if (f < r->loading) { s.transition_state = 3; if (f < (r->blind ? r->blind : 1)) s.room_collisions = s.room_layout = NULL; }
        else if (f == r->loading) { s.transition_state = 5; s.transition_substate = horizontal ? 2 : 1; phase = 1; }
        else if (phase == 1) { phase = 2; counter--; }
        else if (phase == 2) {
            camera = (uint16_t)(camera + delta);
            machine = step;
            scroll_steps++;
            if (!(camera & 7u)) { if (counter) counter--; else phase = 3; }
        } else post++;
        s.transition_phase = (uint8_t)phase; s.screen_scroll_counter = (uint8_t)counter; s.screen_scroll_delta = (uint8_t)delta;
        s.scroll_alignment = (uint8_t)(camera & 7u);
        if (horizontal) s.camera_x = camera; else s.camera_y = camera;
        if (horizontal) x += sign * machine; else y += sign * machine;
        s.link_x = (uint16_t)x; s.link_y = (uint16_t)y;
        OraclesGuestTransitionMutation m;
        memset(&m, 0, sizeof m);
        installed(installed_opaque, &s, &m);
        if (m.set_scroll_delta) delta = (int8_t)m.scroll_delta;
        x += m.link_x_delta; y += m.link_y_delta;
        animated += m.update_walk_animation;
        if (m.update_walk_animation) last_pose = m.link_animation_frame;
        if (m.set_facing) { s.link_direction = m.facing; facing_last = m.facing; }
        if (r->release_at && f >= r->release_at && !machine && !(horizontal ? m.link_x_delta : m.link_y_delta)) still_after_release++;
        if (f > 0 && !(s.link_input_angle & 0x80u) && (s.link_input_angle & 7u) && !(horizontal ? m.link_y_delta : m.link_x_delta)) still_across++;
        if (abs(horizontal ? m.link_x_delta : m.link_y_delta) > largest_along) largest_along = abs(horizontal ? m.link_x_delta : m.link_y_delta);
        if (r->release_at && f >= r->release_at) {
            const int moved = machine + abs(horizontal ? m.link_x_delta : m.link_y_delta);
            if (moved > fastest_released) fastest_released = moved;
        }
        const int across = abs(horizontal ? m.link_y_delta : m.link_x_delta);
        if (across > largest_across) largest_across = across;
    }
    if (r->end) {
        /* finishScrollingTransition: the coordinates rebased by the room's size along the axis, the room entered's
         * collisions those of normal play. */
        const int rebase_x = r->dir == 1u ? -(160 << 8) : r->dir == 3u ? 160 << 8 : 0, rebase_y = r->dir == 2u ? -(128 << 8) : r->dir == 0u ? 128 << 8 : 0;
        s.frame = frame_counter++;
        s.transition_state = 2; s.transition_substate = 0; s.transition_phase = 0; s.scroll_mode = 1;
        s.camera_x = s.camera_y = 0;
        s.room_collisions = s.room_layout = NULL;
        s.play_collisions = r->collisions; s.play_layout = r->layout;
        s.link_x = (uint16_t)(x + rebase_x); s.link_y = (uint16_t)(y + rebase_y);
        OraclesGuestTransitionMutation m;
        memset(&m, 0, sizeof m);
        installed(installed_opaque, &s, &m);
        CHECK(!(m.link_x_delta || m.link_y_delta) || m.scroll_end);
        x += m.link_x_delta; y += m.link_y_delta;
        end_moved = m.link_x_delta || m.link_y_delta;
        animated += m.update_walk_animation;
        if (r->release_at && !(horizontal ? m.link_x_delta : m.link_y_delta)) still_after_release++;
        if (!(s.link_input_angle & 0x80u) && (s.link_input_angle & 7u) && !(horizontal ? m.link_y_delta : m.link_x_delta)) still_across++;
        end_respawn = m.set_respawn;
        if (m.force_transition == 1) end_forced = m.forced_direction;
        if (m.force_transition == 3) end_started = m.forced_direction;
        end_respawn_y = m.respawn_y;
        end_respawn_x = m.respawn_x;
    }
    *out_x = x; *out_y = y;
    frame_counter += 10;
}

/* The game's swim a frame (oracles_room_transition_swim_step) against the game itself: pairs of frames of normal play
 * read on Ages' and Seasons' routes, the swim's hold on, the motion before, the keys of the frame (wLinkAngle,
 * wGameKeysPressed, wGameKeysJustPressed), the motion after. */
typedef struct swim_vector { OraclesSwimMotion before; uint8_t angle, pressed, just; OraclesSwimMotion after; } swim_vector;

static void check_swim_steps(const swim_vector *v, size_t count, int mermaid)
{
    for (size_t i = 0; i < count; i++) {
        OraclesSwimMotion m = v[i].before;
        const OraclesSwimKeys keys = { v[i].angle, v[i].pressed, v[i].just, (uint8_t)mermaid, 0, 1, 0 };
        oracles_room_transition_swim_step(&m, &keys);
        const OraclesSwimMotion *w = &v[i].after;
        const int same = m.angle == w->angle && m.speed == w->speed && m.speed_tmp == w->speed_tmp && m.counter == w->counter && m.period == w->period
                         && m.stroke == w->stroke && m.direction == w->direction && (mermaid ? m.mermaid_stroke == w->mermaid_stroke : m.counter1 == w->counter1);
        if (!same) fprintf(stderr, "swim step %s %zu: angle %02x speed %02x cap %02x counter %02x stroke %u/%02x/%02x direction %u\n", mermaid ? "mermaid" : "flippers", i,
                           m.angle, m.speed, m.speed_tmp, m.counter, m.stroke, m.counter1, m.mermaid_stroke, m.direction);
        CHECK(same);
    }
}

int main(void)
{
    const size_t rom_size = 8u * 0x4000u;
    uint8_t *rom = calloc(rom_size, 1);
    CHECK(rom != NULL);
    if (!rom) return 1;
    /* mapTransitionGroupTable at $7000 of bank 5: every group's list at $7080, empty. */
    uint8_t *const groups = rom + 5u * 0x4000u + 0x3000u, *const list = groups + 0x80u;
    for (unsigned g = 0; g < 8u; g++) { groups[2u * g] = 0x80; groups[2u * g + 1u] = 0x70; }
    OraclesRoomTransition *tr = oracles_room_transition_start((OraclesGuest *)rom, rom, rom_size, 1);
    CHECK(tr != NULL && installed != NULL);
    if (!tr || !installed) return 1;
    CHECK(oracles_room_transition_set_diagonal(tr, 1));
    policy_under_test = tr;
    uint8_t collisions[176];
    int x = 0, y = 0;

    /* Down from y=121 at x=40, straight, twelve frames of load: a pixel a frame up to the game's 16 px, the machine's
     * 8 px of the doubled scroll counted, and beyond them only where nothing is in his way. */
    memset(collisions, 0, sizeof collisions);
    run r = { 2u, 16u, 40 << 8, 121 << 8, 12, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0 };
    play(&r, &x, &y);
    CHECK(y - (121 << 8) == (12 + 2 + 16 + 2) << 8 && scroll_steps == 16);   /* a pixel a frame, the room clear */
    /* While the room entered's collisions are not known nothing says what lies beyond: the pixel the game's distance
     * ends on at most. */
    r.unknown = 1;
    play(&r, &x, &y);
    CHECK((y >> 8) == 121 + 16);

    /* A savestate loaded in the scroll with the policy's progress: it goes on as if no load had come between. */
    r.unknown = 0; r.reset_at = 20; r.restore = 1;
    play(&r, &x, &y);
    CHECK(y - (121 << 8) == (12 + 2 + 16 + 2) << 8 && scroll_steps == 16);
    /* One without it (a state written before the progress was kept): the machine takes Link to the scroll's end alone,
     * its step still doubled, so that he goes no further than its own steps from where the load found him (20 px by
     * then, then its last ten steps of $80); the next transition is the policy's again. */
    r.restore = 0;
    play(&r, &x, &y);
    {
        OraclesRoomTransitionStats stats;
        oracles_room_transition_stats(tr, &stats);
        CHECK(stats.refused_by[10] == 12);                       /* frames 20 to 31 */
    }
    CHECK(y - (121 << 8) == (20 << 8) + 10 * 0x80 && scroll_steps == 16);
    /* One without it loaded in the room's load, before the scroll: the scroll keeps the game's own step, and the machine
     * takes Link the game's distance from where the load found him (5 px by then, then 32 steps of $80), not half of it. */
    r.reset_at = 5;
    play(&r, &x, &y);
    CHECK(y - (121 << 8) == (5 << 8) + 32 * 0x80 && scroll_steps == 32);
    r.reset_at = -1;
    play(&r, &x, &y);
    CHECK(y - (121 << 8) == (12 + 2 + 16 + 2) << 8);

    /* On the sea floor Link glides on with no direction held, at his speed and angle (here down and right, 45 degrees
     * off the axis): the carry goes on with him. */
    r.sea_floor = 1; r.angle = 0x80; r.move_angle = 12; r.loading = 4; r.post = 2;
    play(&r, &x, &y);
    CHECK(x - (40 << 8) == 24 * 0xb5);                                  /* the first frame's carry, withheld while the room loads, caught up */
    CHECK(y - (121 << 8) == (16 << 8) + 2 * 0xb5);                       /* the game's 16 px by the scroll's end, then his own two steps */
    CHECK(largest_across <= 0x160);
    CHECK(animated == 0);                                               /* standing, as the game moves him without a direction */
    r.angle = 12u;
    play(&r, &x, &y);
    CHECK(animated == 4 + 2 + 16 + 2);                                  /* walking, a direction held */

    /* On foot, down and right, the room's collisions unknown for three frames: the carry they withheld is caught up
     * once they are known, at most a pixel a frame across, the bus's bound; and in the water at most $160. */
    r = (run){ 2u, 12u, 40 << 8, 121 << 8, 12, 2, collisions, 0, -1, 0, 0, 3, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0 };
    memset(collisions, 0, sizeof collisions);
    play(&r, &x, &y);
    CHECK(x - (40 << 8) == 32 * 0xb5 && y - (121 << 8) == 32 * 0xb5 && largest_across == 0x100);
    /* A savestate loaded in the load, with what is owed across: the same carry. */
    r.reset_at = 2; r.restore = 1;
    play(&r, &x, &y);
    CHECK(x - (40 << 8) == 32 * 0xb5 && y - (121 << 8) == 32 * 0xb5);
    r.reset_at = -1; r.restore = 0;
    r.sea_floor = 1; r.angle = 0x80; r.move_angle = 12; r.loading = 4; r.blind = 3;
    play(&r, &x, &y);
    CHECK(x - (40 << 8) == 24 * 0xb5 && largest_across == 0x160);
    /* A wall across, along the room entered's first row, refuses the carry when the collisions are known: what it
     * refused, the load's frames' carry with it, is never caught up, every carry after it a step of his own. */
    {
        OraclesRoomTransitionStats before, after;
        for (unsigned c = 3; c < 10u; c++) collisions[0 * 16 + c] = 0x0f;
        r = (run){ 2u, 12u, 43 << 8, 121 << 8, 12, 2, collisions, 0, -1, 0, 0, 3, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0 };   /* his right side (x+4) at 47, column 2 */
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK(after.carry_asked_frames > before.carry_asked_frames && after.carry_withheld_frames > before.carry_withheld_frames + 3);
        CHECK(x - (43 << 8) == (int)(after.carry_asked_frames - before.carry_asked_frames) * 0xb5 && largest_across == 0xb5);
        memset(collisions, 0, sizeof collisions);
    }

    /* Slower than the machine's $80 a step ($0a, $40 a frame), straight down on the sea floor: the doubled scroll's
     * steps, his own short of them, would leave him 9.75 px on; what they leave of the game's 16 px is spread over its
     * steps, as far as those 16 px, and his own step goes on after the scroll. */
    r = (run){ 2u, 16u, 40 << 8, 121 << 8, 4, 2, collisions, 0, -1, 1, 16u, 0, 0x0a, 0, NULL, 0, 0, 0, 0, 0, 0, 0 };
    play(&r, &x, &y);
    CHECK(y - (121 << 8) == (16 << 8) + 2 * 0x40 && x == 40 << 8);

    /* Beyond the game's distance in whole pixels: from y=121, the room entered's row 1 a wall, which his feet (y+7)
     * reach at y=9 already, the game's own place; past the pixel it ends on, nothing; within it, a fraction further
     * (137.80, his steps of $80 in the scroll). */
    memset(collisions, 0, sizeof collisions);
    for (unsigned c = 0; c < 10u; c++) collisions[1 * 16 + c] = 0x0f;
    r = (run){ 2u, 16u, 40 << 8, 121 << 8, 12, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0 };
    play(&r, &x, &y);
    CHECK(y == (137 << 8) + 0x80);
    /* Carried down and right, a hole in row 1 under the carried path's tile (y+5) beyond the game's distance and not
     * under the straight path's: the cap holds him where his tile stays clear of it. */
    memset(collisions, 0, sizeof collisions);
    collisions[1 * 16 + 3] = 0x10;
    r = (run){ 2u, 12u, 40 << 8, 121 << 8, 12, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0 };
    play(&r, &x, &y);
    CHECK(x == 0x3ea0 && y == 0x89e6);                             /* x=62.63 in column 3, y=9.90: his tile (14) still in row 0 */
    r.angle = 16u;
    play(&r, &x, &y);
    CHECK(x == 40 << 8 && (y >> 8) - 128 == 121 + 32 - 128);      /* straight, column 2: nothing in his way */

    /* The room entered's collisions predicted while the game loads them (room_predict.h): on foot down and right, the
     * load's first three frames without them, the carry goes on from the first frame at Link's own step, nothing withheld
     * nor caught up, the prediction what the game then loads; the room 0:05 of the fixture, its tiles and collisions 0. */
    {
        fixture f;
        fixture_init(&f, rom);
        predict_tables = &f.tables;
        uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES], table[ORACLES_TILESET_COLLISIONS_BYTES], expected[ORACLES_ROOM_LAYOUT_BYTES];
        memset(layout, 0, sizeof layout);
        memset(table, 0, sizeof table);
        oracles_room_predict_collisions(table, layout, 1, expected);
        oracles_room_transition_stop(tr);
        tr = oracles_room_transition_start((OraclesGuest *)rom, rom, rom_size, 1);
        CHECK(tr != NULL && oracles_room_transition_set_diagonal(tr, 1));
        if (!tr) return 1;
        policy_under_test = tr;
        OraclesRoomTransitionStats before, after;
        r = (run){ 2u, 12u, 40 << 8, 121 << 8, 12, 2, expected, 0, -1, 0, 0, 3, 0, 0, layout, 0x05, 0, 0, 0, 0, 0, 0 };
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK(x - (40 << 8) == 32 * 0xb5 && y - (121 << 8) == 32 * 0xb5 && largest_across == 0xb5);
        CHECK(after.carry_predicted_frames - before.carry_predicted_frames == 3 && after.carry_unknown_frames == before.carry_unknown_frames);
        CHECK(after.carry_withheld_frames == before.carry_withheld_frames);
        CHECK(after.predicted_transitions - before.predicted_transitions == 1 && after.predicted_matched - before.predicted_matched == 1);
        /* Straight down through a load of twelve frames whose collisions the game gives at none of them (--continuous-steps):
         * the cap along the axis tests the prediction, and he walks on a pixel a frame through the load, as in a room
         * whose collisions are known; without it he would stand from the seventh frame. */
        CHECK(oracles_room_transition_set_steps(tr, 1));
        r = (run){ 2u, 16u, 40 << 8, 121 << 8, 12, 2, expected, 0, -1, 0, 0, 12, 0, 0, layout, 0x05, 0, 0, 0, 0, 0, 0 };
        play(&r, &x, &y);
        CHECK(y - (121 << 8) == (12 + 2 + 16 + 2) << 8);
        CHECK(!oracles_room_transition_set_steps(tr, 0));
        r = (run){ 2u, 12u, 40 << 8, 121 << 8, 12, 2, expected, 0, -1, 0, 0, 3, 0, 0, layout, 0x05, 0, 0, 0, 0, 0, 0 };
        /* A vblank before the game names the room: that frame's carry withheld, the next ones on the prediction, what was
         * withheld caught up. */
        r.unnamed = 1;
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK(x - (40 << 8) == 32 * 0xb5 && largest_across == 0x100);
        CHECK(after.carry_unnamed_frames - before.carry_unnamed_frames == 1 && after.carry_predicted_frames - before.carry_predicted_frames == 2);
        CHECK(after.predicted_matched - before.predicted_matched == 1);
        r.unnamed = 0;
        /* A savestate loaded in the load keeps the prediction: the same carry. */
        r.reset_at = 2; r.restore = 1;
        play(&r, &x, &y);
        CHECK(x - (40 << 8) == 32 * 0xb5 && largest_across == 0xb5);
        r.reset_at = -1; r.restore = 0;
        /* A state of the first version, saved before the prediction, still loads. */
        {
            uint8_t first_version[53];
            memset(first_version, 0, sizeof first_version);
            first_version[0] = 1;
            CHECK(oracles_room_transition_load_state(tr, first_version, sizeof first_version) == 0);
            first_version[0] = 2;
            CHECK(oracles_room_transition_load_state(tr, first_version, sizeof first_version) == -1);
            installed_reset(installed_opaque);
        }
        /* What the game loads otherwise than predicted is counted (a wall in its row 6, off Link's path). */
        uint8_t walled[ORACLES_ROOM_LAYOUT_BYTES];
        memcpy(walled, expected, sizeof walled);
        walled[6 * 16 + 8] = 0x0f;
        r.collisions = walled;
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK(after.predicted_mismatched - before.predicted_mismatched == 1 && after.predicted_matched == before.predicted_matched);
        /* A room whose own code changes its tiles is left to the game's load: the carry withheld, then caught up. */
        const uint8_t code[] = { 0x05, 0x08, 0x00 };
        fixture_group_lists(&f, f.tables.room_tile_changer_code_group_table.addr, (const uint8_t[]){ 0 }, 1, 0, code, sizeof code);
        r.collisions = expected;
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK(x - (40 << 8) == 32 * 0xb5 && largest_across == 0x100);
        CHECK(after.carry_unknown_frames - before.carry_unknown_frames == 3 && after.carry_predicted_frames == before.carry_predicted_frames);
        CHECK(after.predict_fallbacks - before.predict_fallbacks == 1 && after.predict_fallback_by[ORACLES_ROOM_PREDICT_ROOM_CODE] - before.predict_fallback_by[ORACLES_ROOM_PREDICT_ROOM_CODE] == 1);
        predict_tables = NULL;
        oracles_room_transition_stop(tr);
        tr = oracles_room_transition_start((OraclesGuest *)rom, rom, rom_size, 1);
        CHECK(tr != NULL);
        if (!tr) return 1;
        policy_under_test = tr;
    }

    /* Without the steps, the frame that ends the scroll leaves Link still, and the frame that starts a diagonal one moves
     * him across by the game's step and the policy's both. */
    CHECK(oracles_room_transition_set_diagonal(tr, 1));
    {
        OraclesRoomTransitionStats before, after;
        memset(collisions, 0, sizeof collisions);
        r = (run){ 2u, 12u, 40 << 8, 121 << 8, 12, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 1, 0xb5, -0x4b, 0, 0 };
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK(!end_moved && after.end_stalls - before.end_stalls == 1 && after.doubled_starts == before.doubled_starts);
        CHECK(x - (40 << 8) == 33 * 0xb5);
    }

    /* With the steps (--continuous-steps): a step a frame, the game's own in the frame that starts the transition
     * completed, never doubled (across nothing more, along up to a step from where the frame before left him), and the
     * frame that ends the scroll a step too, onto the room entered's collisions of normal play. */
    CHECK(oracles_room_transition_set_steps(tr, 1));
    {
        OraclesRoomTransitionStats before, after;
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK(end_moved && after.end_stalls == before.end_stalls && after.doubled_starts == before.doubled_starts);
        CHECK(x - (40 << 8) == 33 * 0xb5 && y - (121 << 8) == 33 * 0xb5 && largest_across == 0xb5);
        CHECK(animated == 32);                                      /* his walk at the frame that ends it the game's */
        /* Straight down: a pixel a frame and the frame that ends it. */
        r = (run){ 2u, 16u, 40 << 8, 121 << 8, 12, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 1, 0, 0, 0, 0 };
        play(&r, &x, &y);
        CHECK(y - (121 << 8) == (12 + 2 + 16 + 2 + 1) << 8 && x == 40 << 8 && end_moved);
        /* A wall in the room entered's row 2, which his feet (y+7) reach past y=24: held short of it, at the frame that
         * ends the scroll too. */
        for (unsigned c = 0; c < 10u; c++) collisions[2 * 16 + c] = 0x0f;
        play(&r, &x, &y);
        CHECK(!end_moved && y - (121 << 8) == 31 << 8);
        memset(collisions, 0, sizeof collisions);
        /* A tap: the direction let go at the third frame of the load.  Nothing more there, then the game's own move of
         * the transition, its 16 px from where he started, the machine's steps topped up and spread over the scroll and
         * the frames after it, up to the frame that ends it, not animated; no frame leaves him still. */
        r.release_at = 3;
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK(y - (121 << 8) == 16 << 8 && end_moved && animated == 3 && still_after_release == 0);
        CHECK(after.released_frames - before.released_frames == 12 - 3 + 2 + 16 + 2 + 1 && after.end_stalls == before.end_stalls);
        /* Held through the load, let go in the scroll: what he walked is his, the machine's steps after it too. */
        r.release_at = 16;
        play(&r, &x, &y);
        CHECK(y - (121 << 8) == (16 << 8) + 0x80 * 14 && animated == 16);
        /* A diagonal let go at the fourth frame: the carry across to then (the first frame's the game's step), and the game's
         * own move along the axis, 16 px from where it put him back on the edge. */
        r = (run){ 2u, 12u, 40 << 8, 121 << 8, 12, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 1, 0xb5, -0x4b, 4, 0 };
        play(&r, &x, &y);
        CHECK(y - (121 << 8) == (16 << 8) - 0x4b && x - (40 << 8) == 4 * 0xb5);
        /* Right alone held from there, square across the axis: the carry across goes on at a pixel a frame, the game's own
         * move along it. */
        r.angle = 8u; r.release_at = 0;
        play(&r, &x, &y);
        CHECK(y - (121 << 8) == (16 << 8) - 0x4b && x - (40 << 8) == 0xb5 + 32 * 0x100 && largest_across == 0x100);
        CHECK(facing_last == 1);                                    /* turned right, as the game turns him walking right */
        /* Down and right held: a diagonal whose two ways include the one he faces, down: he keeps it. */
        r.angle = 12u;
        play(&r, &x, &y);
        CHECK(facing_last == -1);
        /* The tap again, in a load of four frames, let go at its third: the game's own move from that frame on, evenly,
         * no frame leaving him still before the scroll's last step. */
        r = (run){ 2u, 16u, 40 << 8, 121 << 8, 4, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 1, 0, 0, 2, 0 };
        play(&r, &x, &y);
        CHECK(y - (121 << 8) == 16 << 8 && still_after_release == 0 && fastest_released <= 0x100);
        /* In a load of seven frames let go at its seventh: he has walked as far as the doubled scroll's own steps take
         * him to the game's place, nothing is left to spread, and he stands until the scroll's first step. */
        r.loading = 7; r.release_at = 7;
        play(&r, &x, &y);
        CHECK((y - (121 << 8)) >> 8 == 16 && still_after_release <= 2);
        /* A diagonal held down and right in a load of two frames, let go late in the scroll, short of the game's place:
         * the game's own move never faster than he walked, 0xb5 along the axis. */
        r = (run){ 2u, 12u, 40 << 8, 121 << 8, 2, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 1, 0xb5, -0x4b, 14, 0 };
        play(&r, &x, &y);
        CHECK(fastest_released <= 0xb5 && y - (121 << 8) <= 16 << 8);
        /* At a corner: down and right held from x=150, near the room's right edge (the boundary 154): the carry slows
         * so as to reach it at the frame that ends the scroll, never past it, so that the transition right after this one
         * starts without the game putting him back; it never stops across before. */
        r = (run){ 2u, 12u, 150 << 8, 121 << 8, 12, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 1, 0xb5, -0x4b, 0, 0 };
        {
            OraclesRoomTransitionStats before_edge, after_edge;
            oracles_room_transition_stats(tr, &before_edge);
            play(&r, &x, &y);
            oracles_room_transition_stats(tr, &after_edge);
            CHECK((x >> 8) == 154 && after_edge.carry_edge_frames > before_edge.carry_edge_frames && still_across == 0);
            /* The transition right that starts at the next frame, the game's step toward the edge in it: its scroll is
             * doubled as the first was, the first's doubled step forgotten as it ended. */
            run next = { 1u, 12u, 154 << 8, (y - (128 << 8)), 2, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 1, 0xb5, 0xb5, 0, 1 };
            play(&next, &x, &y);
            CHECK(scroll_steps == 20);
        }
        /* The respawn finishScrollingTransition took from where the steps left him goes where the game's own scroll would
         * have left him, from where it put him back on the edge: down, 16 px from y=121.21 (yh 121 kept), 9 in the room
         * entered, across on the straight path (40.70); up, 16 px from yh 6, 118; left, 15 px from xh 6, 151; right, 15 px
         * from xh 154, 9. */
        r = (run){ 2u, 12u, 40 << 8, (121 << 8) + 0x80, 12, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 1, 0xb5, -0x4b, 0, 0 };
        play(&r, &x, &y);
        CHECK(end_respawn && end_respawn_y == 9 && end_respawn_x == 40 && (y >> 8) - 128 == 16 && (x >> 8) == 63);   /* he stands at 16, 63 */
        r = (run){ 0u, 0u, 60 << 8, (6 << 8) + 0x40, 4, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 1, 0, 0, 0, 0 };
        play(&r, &x, &y);
        CHECK(end_respawn && end_respawn_y == 118 && end_respawn_x == 60);
        r = (run){ 3u, 24u, 6 << 8, 60 << 8, 4, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 1, 0, 0, 0, 0 };
        play(&r, &x, &y);
        CHECK(end_respawn && end_respawn_y == 60 && end_respawn_x == 151);
        r = (run){ 1u, 8u, (154 << 8) + 0x20, 60 << 8, 4, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 1, 0, 0, 0, 0 };
        play(&r, &x, &y);
        CHECK(end_respawn && end_respawn_y == 60 && end_respawn_x == 9);
        /* On the sea floor, the game having put him back on the edge 0.875 px further than the bus's bound in the water
         * leaves room to make up at the frame that starts the transition: the rest follows within it over the next
         * frames, so that he swims on from where his own step would have taken him. */
        {
            int y_straight = 0;
            r = (run){ 2u, 16u, 40 << 8, 121 << 8, 4, 2, collisions, 0, -1, 1, 16u, 0, 0, 0, NULL, 0, 0, 1, 0, 0, 0, 0 };
            play(&r, &x, &y_straight);
            r.game_y = -0xe0;
            play(&r, &x, &y);
            CHECK(y == y_straight && largest_along <= 0x160);
        }
        /* A savestate loaded at the frame that starts the transition, the policy's progress with it: the step of the
         * frame before it known, the same steps. */
        r = (run){ 2u, 12u, 40 << 8, 121 << 8, 12, 2, collisions, 0, 0, 0, 0, 0, 0, 1, NULL, 0, 0, 1, 0xb5, -0x4b, 0, 0 };
        play(&r, &x, &y);
        CHECK(x - (40 << 8) == 33 * 0xb5 && y - (121 << 8) == 33 * 0xb5 && largest_across == 0xb5);
        /* The state the policy was last asked in goes back to the bus with the load (here 5, a scroll's). */
        {
            uint8_t saved[ORACLES_ROOM_TRANSITION_STATE_SIZE];
            r = (run){ 2u, 16u, 40 << 8, 121 << 8, 12, 2, collisions, 0, 20, 0, 0, 0, 0, 1, NULL, 0, 0, 1, 0, 0, 0, 0 };
            bus_last_state = 0;
            play(&r, &x, &y);
            CHECK(bus_last_state == 5 && oracles_room_transition_save_state(tr, saved, sizeof saved) == sizeof saved);
        }
        /* A state of the second version, saved before the steps, loads. */
        uint8_t second_version[57 + 2 * ORACLES_ROOM_LAYOUT_BYTES];
        memset(second_version, 0, sizeof second_version);
        second_version[0] = 2;
        CHECK(oracles_room_transition_load_state(tr, second_version, sizeof second_version) == 0);
        installed_reset(installed_opaque);
        /* The third (before the room left's collisions) and the fourth (before its index) load; one of the fourth's
         * size named the fifth, or the reverse, does not. */
        uint8_t third_version[89 + 2 * ORACLES_ROOM_LAYOUT_BYTES], fourth_version[90 + 3 * ORACLES_ROOM_LAYOUT_BYTES];
        memset(third_version, 0, sizeof third_version);
        memset(fourth_version, 0, sizeof fourth_version);
        third_version[0] = 3;
        fourth_version[0] = 4;
        CHECK(oracles_room_transition_load_state(tr, third_version, sizeof third_version) == 0);
        CHECK(oracles_room_transition_load_state(tr, fourth_version, sizeof fourth_version) == 0);
        fourth_version[0] = 5;
        CHECK(oracles_room_transition_load_state(tr, fourth_version, sizeof fourth_version) == -1);
        /* The fifth (before the room beside) loads too; one of the sixth's size named the fifth does not. */
        uint8_t fifth_version[91 + 3 * ORACLES_ROOM_LAYOUT_BYTES];
        memset(fifth_version, 0, sizeof fifth_version);
        fifth_version[0] = 5;
        CHECK(oracles_room_transition_load_state(tr, fifth_version, sizeof fifth_version) == 0);
        /* The sixth (before the swim at the line) loads too; one of the seventh's size named the sixth does not. */
        uint8_t sixth_version[95 + 7 * ORACLES_ROOM_LAYOUT_BYTES];
        memset(sixth_version, 0, sizeof sixth_version);
        sixth_version[0] = 6;
        CHECK(oracles_room_transition_load_state(tr, sixth_version, sizeof sixth_version) == 0);
        uint8_t seventh[ORACLES_ROOM_TRANSITION_STATE_SIZE];
        CHECK(oracles_room_transition_save_state(tr, seventh, sizeof seventh) == sizeof seventh && seventh[0] == 7);
        seventh[0] = 6;
        CHECK(oracles_room_transition_load_state(tr, seventh, sizeof seventh) == -1);
        installed_reset(installed_opaque);
    }
    /* At the line (--continuous-line): let go, he stops where he is, the machine's steps undone, at the frame that ends
     * the scroll too; turned back, he walks back as far as where the transition began, the line, and no further. */
    CHECK(oracles_room_transition_set_steps(tr, 1) && oracles_room_transition_set_line(tr, 1));
    {
        memset(collisions, 0, sizeof collisions);
        OraclesRoomTransitionStats before, after;
        oracles_room_transition_stats(tr, &before);
        r = (run){ 2u, 16u, 40 << 8, 127 << 8, 12, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 1, 0, 0, 3, 0 };
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK(y - (127 << 8) == 3 << 8 && !end_moved && animated == 3 && largest_along <= 0x100);
        CHECK(after.standing_frames == before.standing_frames && last_pose == 0);   /* no animation data: the step as frozen */
        /* With the ROM's tables (specialObjectAnimationTable, Link's list, the walk's entry: 2 frames of pose $54), he
         * stands in that pose from the frame it is let go, as the game stands him with nothing held. */
        uint8_t *bank6 = rom + 6u * 0x4000u;
        bank6[0x3000] = 0x00; bank6[0x3001] = 0x71;                         /* $7000: Link's list at $7100 */
        bank6[0x3100 + 2 * 0x10] = 0x00; bank6[0x3100 + 2 * 0x10 + 1] = 0x72; /* the walk's data at $7200 */
        bank6[0x3200] = 0x02; bank6[0x3201] = 0x54; bank6[0x3202] = 0x00;
        animation_table = 0x7000;
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK(y - (127 << 8) == 3 << 8 && last_pose == 0x54);
        CHECK(after.standing_frames - before.standing_frames == after.stopped_frames - before.stopped_frames - 1);   /* all but the frame that ends it, the game's */
        OraclesGuestTransitionState st;
        memset(&st, 0, sizeof st);
        st.animation_data_bank = 6; st.animation_table = 0x7000;
        OraclesGuestTransitionMutation pose;
        memset(&pose, 0, sizeof pose);
        CHECK(oracles_room_transition_standing_frame(rom, rom_size, &st, &pose) && pose.link_anim_counter == 2 && pose.link_animation_frame == 0x54
              && pose.link_anim_pointer == 0x7203 && pose.link_anim_parameter == 0);
        st.animation_table = 0;
        CHECK(!oracles_room_transition_standing_frame(rom, rom_size, &st, &pose));
        CHECK(after.stopped_frames - before.stopped_frames == 12 - 3 + 2 + 16 + 2 + 1 && after.back_frames == before.back_frames);
        /* Let go in the scroll: what he walked, nothing after. */
        r.release_at = 20;
        play(&r, &x, &y);
        CHECK(y - (127 << 8) == 20 << 8);
        /* Right, at a pixel a frame for eight frames, then left: back to the line, the room entered's first pixel (160 in
         * the room left's coordinates, the start at 159), turned left, and still there. */
        r = (run){ 1u, 8u, 159 << 8, 60 << 8, 12, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 1, 0, 0, 0, 0 };
        back_at = 8;
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK(x == 160 << 8 && facing_last == 3 && largest_along <= ORACLES_TRANSITION_LINE_STEP_MAX && last_pose == 0);   /* walking back, walking */
        CHECK(after.back_frames > before.back_frames && after.back_line_frames > before.back_line_frames);
        CHECK(end_forced == -1);
        /* With the room left's collisions known, as normal play gives them: back past the line into it, a pixel a frame
         * to the end of the scroll, and the transition back forced from there, leftward. */
        uint8_t left[176];
        memset(left, 0, sizeof left);
        left_room = left;
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK(x < 159 << 8 && end_forced == 3 && after.back_past_frames > before.back_past_frames && after.back_reversals == before.back_reversals + 1);
        CHECK(after.back_line_frames == before.back_line_frames && largest_along <= ORACLES_TRANSITION_LINE_STEP_MAX);
        /* Downward, turned back up into the room above: 5 px past the line at most, his tile (y+5) still in the room entered's
         * rows once the scroll is over, and the transition back forced upward from there. */
        {
            run up = (run){ 2u, 16u, 40 << 8, 127 << 8, 12, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 1, 0, 0, 0, 0 };
            const int saved_back = back_at;
            back_at = 8;
            play(&up, &x, &y);
            CHECK(y == (128 - 5) << 8 && end_forced == 0);
            back_at = saved_back;
        }
        /* A room with a transition of its own in the group's list (getNextActiveRoom), the room entered here (0, the
         * runs' wActiveRoom): the scroll back may lead elsewhere; back to the line, and no further. */
        {
            OraclesGuestTransitionState st;
            memset(&st, 0, sizeof st);
            st.map_transition_bank = 5; st.map_transition_table = 0x7000;
            CHECK(!oracles_room_transition_special_room(rom, rom_size, &st, 0, 0x40));
            list[0] = 0x40; list[1] = 0x00; list[2] = 0x00;
            CHECK(oracles_room_transition_special_room(rom, rom_size, &st, 0, 0x40) && !oracles_room_transition_special_room(rom, rom_size, &st, 0, 0x41));
            st.map_transition_bank = 0xff;
            CHECK(oracles_room_transition_special_room(rom, rom_size, &st, 0, 0x41));   /* no table known: as if it had one */
            st.map_transition_bank = 5;
            list[0] = 0x00;
            run special = r;
            special.room = 0x40;
            list[0] = 0x40;
            play(&special, &x, &y);
            CHECK(x == 160 << 8 && end_forced == -1);
            list[0] = 0x00;
            play(&r, &x, &y);
            CHECK(x < 159 << 8 && end_forced == 3);
        }
        /* The next frame does not take it (a menu opened there), Link still past the line (x=232, 24 px back into the room
         * left): kept, frame after frame, for the game to take after the menu, from where he stands. */
        const OraclesGuestTransitionMutation kept1 = next_frame_at(0x83, 2, 232 << 8, 0), kept2 = next_frame_at(0x83, 2, 232 << 8, 0);
        CHECK(kept1.force_transition == 0 && kept2.force_transition == 0);
        oracles_room_transition_stats(tr, &after);
        CHECK(after.back_lowered == before.back_lowered && after.back_kept_frames == before.back_kept_frames + 2);
        /* A script disables the transitions meanwhile: lowered, once, toward the room left (the bus puts him back on the
         * line). */
        const OraclesGuestTransitionMutation lowered = next_frame_at(0x83, 2, 232 << 8, 1);
        CHECK(lowered.force_transition == 2 && lowered.forced_direction == 3 && next_frame_at(0x83, 2, 232 << 8, 1).force_transition == 0);
        oracles_room_transition_stats(tr, &after);
        CHECK(after.back_lowered == before.back_lowered + 1);
        /* No longer past the line (inside the room entered): lowered at once. */
        play(&r, &x, &y);
        CHECK(end_forced == 3 && next_frame(0x83, 2).force_transition == 2);
        /* Taken, the game in the transition back: nothing to lower. */
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        CHECK(end_forced == 3 && next_frame(0x03, 3).force_transition != 2);
        oracles_room_transition_stats(tr, &after);
        CHECK(after.back_lowered == before.back_lowered);
        /* A savestate loaded in the scroll, the policy's progress with it (the room left's collisions included): the same. */
        {
            const int expected_x = x;
            run reloaded = r;
            reloaded.reset_at = 20;
            reloaded.restore = 1;
            play(&reloaded, &x, &y);
            CHECK(x == expected_x && end_forced == 3);
            /* Without the progress, the room left unknown: back to the line, and no further. */
            reloaded.restore = 0;
            left_room = NULL;
            play(&reloaded, &x, &y);
            CHECK(x >= 160 << 8 && end_forced == -1);
            left_room = left;
        }
        /* A wall in the room left's last column, where his left points reach past the line: as far as the line, and no
         * transition forced. */
        for (unsigned row = 0; row < 8u; row++) left[row * 16 + 9] = 0x0f;
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK(x == 160 << 8 && end_forced == -1 && after.back_wall_frames > before.back_wall_frames);
        left_room = NULL;
        /* Turned back in the scroll: a pixel a frame back to the frame that ends it, 24 px on and 13 back, the machine's
         * step and his own back together within the bus's bound. */
        back_at = 24;
        play(&r, &x, &y);
        CHECK(x - (159 << 8) == 11 << 8 && largest_along == 0x100 + 0x60);
        r = (run){ 2u, 16u, 40 << 8, 127 << 8, 12, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 1, 0, 0, 0, 0 };
        back_at = 26;
        play(&r, &x, &y);
        CHECK(y - (127 << 8) == 19 << 8 && largest_along == ORACLES_TRANSITION_LINE_STEP_MAX);
        back_at = 0;
        animation_table = 0;
        /* Right from the line along the room entered's top row, its column 1 a wall: past the game's own place (its edge,
         * 154, and its 15 px, not the line's) his step stops where his right points (x+4) reach the wall, x=12, as the
         * game's walk stops him, never in it; the machine's later steps undone. */
        memset(collisions, 0, sizeof collisions);
        for (unsigned row = 0; row < 8u; row++) collisions[row * 16 + 1] = 0x0f;
        r = (run){ 1u, 8u, 159 << 8, 9 << 8, 12, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 1, 0, 0, 0, 0 };
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK((x >> 8) == 172 && oracles_room_transition_wall_excess(collisions, 160, 128, (x >> 8) - 160, y >> 8) == 0);
        CHECK(after.line_wall_frames > before.line_wall_frames && after.line_unknown_frames == before.line_unknown_frames);
        CHECK(oracles_room_transition_wall_excess(collisions, 160, 128, 14, 9) > 0);   /* where the steps from the line took him */
        /* Its collisions unknown through the scroll: held at the game's place (169), a step on at the frame that ends it,
         * which knows them. */
        r.unknown = 1;
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK((x >> 8) == 170 && after.line_unknown_frames > before.line_unknown_frames);
        r.unknown = 0;
        /* Column 1 the raised floor's edge ($1e): a wall to his walk, but not raised on it (Ages' wLinkRaisedFloorOffset,
         * @checkTileCollisionAt_allowRaisedFl), where he walks on, as far as the cap past the line's 15 px. */
        for (unsigned row = 0; row < 8u; row++) collisions[row * 16 + 1] = 0x1e;
        play(&r, &x, &y);
        CHECK((x >> 8) == 172);
        raised_floor = 1;
        play(&r, &x, &y);
        CHECK((x >> 8) > 172);
        raised_floor = 0;
        /* Up through a corner's second scroll, started past the line already (y=-8, 120 in the room entered), up-left held:
         * row 6 a wall but for its last column, row 7 clear.  Up only as far as his top points (y-3) reach the wall,
         * y=114, where the steps from the line took him 10 px further, into it. */
        memset(collisions, 0, sizeof collisions);
        for (unsigned col = 0; col < 9u; col++) collisions[6 * 16 + col] = 0x0f;
        r = (run){ 0u, 28u, 140 << 8, -(8 << 8), 12, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 1, 0, 0, 0, 0 };
        play(&r, &x, &y);
        CHECK(((y + (128 << 8)) >> 8) == 114);
        CHECK(oracles_room_transition_wall_excess(collisions, 160, 128, x >> 8, (y + (128 << 8)) >> 8) == 0);
        CHECK(oracles_room_transition_wall_excess(collisions, 160, 128, 140, 104) > 0);
    }
    CHECK(!oracles_room_transition_set_line(tr, 0));
    CHECK(!oracles_room_transition_set_steps(tr, 0));

    /* At the line, the room beside the room entered predicted (room_predict.h): up-right from 0:12's line into 0:13, the
     * diagonal held, he goes on across at his pace past the line into 0:03 rather than slowing down to reach it as the
     * scroll ends, and the frame that ends it starts the scroll up from where he stands; the prediction is what the
     * game then loads. */
    {
        fixture f;
        fixture_init(&f, rom);
        predict_tables = &f.tables;
        uint8_t flags[256], layout[ORACLES_ROOM_LAYOUT_BYTES], table[ORACLES_TILESET_COLLISIONS_BYTES], expected[ORACLES_ROOM_LAYOUT_BYTES];
        memset(flags, 0, sizeof flags);
        memset(layout, 0, sizeof layout);
        memset(table, 0, sizeof table);
        oracles_room_predict_collisions(table, layout, 1, expected);
        flags_page = flags;
        oracles_room_transition_stop(tr);
        tr = oracles_room_transition_start((OraclesGuest *)rom, rom, rom_size, 1);
        CHECK(tr != NULL && oracles_room_transition_set_diagonal(tr, 1) && oracles_room_transition_set_steps(tr, 1) && oracles_room_transition_set_line(tr, 1));
        if (!tr) return 1;
        policy_under_test = tr;
        OraclesRoomTransitionStats before, after;
        r = (run){ 1u, 4u, 159 << 8, 10 << 8, 12, 2, expected, 0, -1, 0, 0, 0, 0, 0, layout, 0x13, 0, 1, 0, 0, 0, 0 };
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        const int past_y = y;
        CHECK(y < -(8 << 8) && end_started == 0 && largest_across == 0xb5);
        CHECK(after.carry_edge_frames == before.carry_edge_frames && after.beside_predicted - before.beside_predicted == 1);
        CHECK(after.beside_crossings - before.beside_crossings == 1 && after.beside_past_frames > before.beside_past_frames);
        /* The next scroll, up into 0:03, loads what was predicted. */
        {
            OraclesGuestTransitionState n;
            memset(&n, 0, sizeof n);
            n.game = ORACLES_GAME_AGES; n.ages_water = 1; n.game_state = 2; n.cutscene_index = 1; n.scroll_mode = 8; n.transition_state = 3;
            n.link_object_index = 0xd0; n.link_state = 1; n.link_visible = 0x80; n.link_anim_mode = 0x10; n.animation_data_bank = 6;
            n.link_input_angle = 4; n.tile_types_bank = 0xff; n.active_room = 0x03; n.room_collisions = expected; n.room_layout = layout;
            n.frame = frame_counter++;
            OraclesGuestTransitionMutation m;
            memset(&m, 0, sizeof m);
            oracles_room_transition_stats(tr, &before);
            installed(installed_opaque, &n, &m);
            oracles_room_transition_stats(tr, &after);
            CHECK(after.beside_matched - before.beside_matched == 1 && after.beside_mismatched == before.beside_mismatched);
        }
        /* A savestate loaded in the scroll, the policy's progress with it: the same place, the same scroll started. */
        r.reset_at = 20; r.restore = 1;
        play(&r, &x, &y);
        CHECK(y == past_y && end_started == 0);
        r.reset_at = -1; r.restore = 0;
        /* The transitions disabled from the 26th frame, past the line already (an object of the room entered): he
         * walks back across to the line at his pace, a pixel a frame at most, and nothing is started. */
        disable_at = 26;
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK(y >= 0 && end_started == -1 && largest_across <= 0x100 && after.beside_returned_frames > before.beside_returned_frames);
        disable_at = 0;
        /* Square up alone from the line through a long load, in the room entered along the axis: straight across at a
         * pixel a frame, held 32 px past the line, the scroll up started from there. */
        r = (run){ 1u, 0u, 160 << 8, 1 << 8, 24, 2, expected, 0, -1, 0, 0, 0, 0, 0, layout, 0x13, 0, 1, 0, 0, 0, 0 };
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK(y == -(32 << 8) && end_started == 0 && after.beside_held_frames > before.beside_held_frames);
        r = (run){ 1u, 4u, 159 << 8, 10 << 8, 12, 2, expected, 0, -1, 0, 0, 0, 0, 0, layout, 0x13, 0, 1, 0, 0, 0, 0 };
        /* A hole along 0:03's bottom row (collision $10, which water and lava share: the carry reads the collision
         * alone): his tile (y+5) never carried onto it, nothing started. */
        for (unsigned col = 0; col < 3u; col++) fixture_room_tile(&f, 0x03, col, 7, 0xc3);
        fixture_collision(&f, 0xc3, 0x10);
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK(y + (5 << 8) >= 0 && end_started == -1 && after.beside_wall_frames > before.beside_wall_frames);
        for (unsigned col = 0; col < 3u; col++) fixture_room_tile(&f, 0x03, col, 7, 0);
        /* A wall along 0:03's bottom row: the carry held where his points would reach it, short of the line, nothing
         * started; he goes on along the axis. */
        fixture_room_tile(&f, 0x03, 0, 7, 0x3a);
        fixture_room_tile(&f, 0x03, 1, 7, 0x3a);
        fixture_room_tile(&f, 0x03, 2, 7, 0x3a);
        fixture_collision(&f, 0x3a, 0x0f);
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK(y >= 3 << 8 && end_started == -1 && x - (159 << 8) > 20 << 8);
        CHECK(after.carry_edge_frames == before.carry_edge_frames && after.beside_crossings == before.beside_crossings);
        CHECK(after.beside_wall_frames > before.beside_wall_frames);
        /* Off the map's top, the room beside not predicted: slowed to reach the line as the scroll ends, as without it. */
        r.room = 0x03;
        oracles_room_transition_stats(tr, &before);
        play(&r, &x, &y);
        oracles_room_transition_stats(tr, &after);
        CHECK(y >= 0 && y < 1 << 8 && end_started == -1 && after.carry_edge_frames > before.carry_edge_frames);
        CHECK(after.beside_fallback_by[ORACLES_ROOM_PREDICT_ROUTED] - before.beside_fallback_by[ORACLES_ROOM_PREDICT_ROUTED] == 1);
        flags_page = NULL;
        predict_tables = NULL;
        CHECK(!oracles_room_transition_set_line(tr, 0));
        CHECK(!oracles_room_transition_set_steps(tr, 0));
    }

    /* With the transitions alone, the cap the routes recorded with them were played with: past the game's distance in
     * 8.8, his middle and 4 px ahead, the machine's steps not counted (the wall of row 1 under his feet from y=9, under
     * y+4 from 12). */
    oracles_room_transition_stop(tr);
    tr = oracles_room_transition_start((OraclesGuest *)rom, rom, rom_size, 0);
    CHECK(tr != NULL);
    if (!tr) return 1;
    policy_under_test = tr;
    memset(collisions, 0, sizeof collisions);
    for (unsigned c = 0; c < 10u; c++) collisions[1 * 16 + c] = 0x0f;
    r = (run){ 2u, 16u, 40 << 8, 121 << 8, 12, 2, collisions, 0, -1, 0, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0 };
    play(&r, &x, &y);
    CHECK(y == 145 << 8);                                          /* 17 in the room entered, his feet in the wall */

    {
        /* The mermaid suit (Ages, ages/swim-line.route): a stroke speeding him up, its frames spent, a step lost a period,
         * a press turning him, a glide to a stop and his motion then none. */
        static const swim_vector mermaid[] = {
            { { 0x10, 0x19, 0x2d, 0x00, 0x05, 0x00, 0x00, 0x04, 2 }, 0x10, 0x80, 0x00, { 0x10, 0x1e, 0x2d, 0x00, 0x05, 0x00, 0x00, 0x03, 2 } },
            { { 0x10, 0x2d, 0x2d, 0x00, 0x05, 0x00, 0x00, 0x00, 2 }, 0x10, 0x80, 0x00, { 0x10, 0x2d, 0x2d, 0x01, 0x05, 0x00, 0x00, 0xff, 2 } },
            { { 0x10, 0x2d, 0x2d, 0x03, 0x05, 0x00, 0x00, 0xff, 2 }, 0x14, 0xa0, 0x20, { 0x11, 0x2d, 0x2d, 0x00, 0x05, 0x00, 0x00, 0x04, 2 } },
            { { 0x10, 0x2d, 0x2d, 0x00, 0x05, 0x00, 0x00, 0x04, 2 }, 0x0c, 0x90, 0x10, { 0x0f, 0x2d, 0x2d, 0x00, 0x05, 0x00, 0x00, 0x04, 2 } },
            { { 0x0c, 0x2d, 0x2d, 0x02, 0x05, 0x00, 0x00, 0xff, 2 }, 0x10, 0x80, 0x00, { 0x0d, 0x2d, 0x2d, 0x00, 0x05, 0x00, 0x00, 0x04, 2 } },
            { { 0x18, 0x2d, 0x2d, 0x04, 0x05, 0x00, 0x00, 0xff, 3 }, 0xff, 0x00, 0x00, { 0x18, 0x28, 0x2d, 0x00, 0x05, 0x00, 0x00, 0xff, 3 } },
            { { 0x18, 0x05, 0x2d, 0x04, 0x05, 0x00, 0x00, 0xff, 3 }, 0x18, 0x20, 0x20, { 0x18, 0x0a, 0x2d, 0x00, 0x05, 0x00, 0x00, 0x04, 3 } },
            { { 0x18, 0x05, 0x2d, 0x04, 0x05, 0x00, 0x00, 0xff, 3 }, 0xff, 0x00, 0x00, { 0xff, 0x00, 0x2d, 0x12, 0x05, 0x00, 0x00, 0xff, 3 } },
            { { 0xff, 0x00, 0x2d, 0x12, 0x05, 0x00, 0x00, 0xff, 3 }, 0xff, 0x00, 0x00, { 0xff, 0x00, 0x2d, 0x12, 0x05, 0x00, 0x00, 0xff, 3 } },
            { { 0xff, 0x00, 0x2d, 0x12, 0x05, 0x00, 0x00, 0xff, 3 }, 0x18, 0x20, 0x20, { 0x18, 0x00, 0x2d, 0x14, 0x05, 0x00, 0x00, 0x04, 3 } },
            { { 0x08, 0x05, 0x2d, 0x00, 0x05, 0x00, 0x00, 0x01, 1 }, 0xff, 0x00, 0x00, { 0xff, 0x00, 0x2d, 0x12, 0x05, 0x00, 0x00, 0x00, 1 } },
        };
        /* The flippers (Seasons, seasons/swim-line.route): a stroke's two states, its cap raised then lowered, the
         * stroke over at the flippers' own speed, the next one started by the hold, turning toward the keys. */
        static const swim_vector flippers[] = {
            { { 0x18, 0x19, 0x19, 0x00, 0x03, 0x01, 0x0c, 0x00, 3 }, 0x18, 0x20, 0x00, { 0x18, 0x19, 0x19, 0x01, 0x03, 0x01, 0x0b, 0x00, 3 } },
            { { 0x18, 0x19, 0x1e, 0x02, 0x03, 0x01, 0x07, 0x00, 3 }, 0x18, 0x20, 0x00, { 0x18, 0x1e, 0x1e, 0x00, 0x03, 0x01, 0x06, 0x00, 3 } },
            { { 0x18, 0x14, 0x19, 0x02, 0x03, 0x01, 0x0b, 0x00, 3 }, 0x1c, 0x60, 0x00, { 0x19, 0x19, 0x19, 0x00, 0x03, 0x01, 0x0a, 0x00, 3 } },
            { { 0x1c, 0x23, 0x23, 0x00, 0x03, 0x01, 0x01, 0x00, 3 }, 0x1c, 0x60, 0x00, { 0x1c, 0x23, 0x28, 0x01, 0x03, 0x02, 0x0c, 0x00, 3 } },
            { { 0x1c, 0x28, 0x23, 0x02, 0x03, 0x02, 0x08, 0x00, 3 }, 0x1c, 0x60, 0x00, { 0x1c, 0x23, 0x23, 0x00, 0x03, 0x02, 0x07, 0x00, 3 } },
            { { 0x1c, 0x1e, 0x1e, 0x00, 0x03, 0x02, 0x01, 0x00, 3 }, 0x00, 0x40, 0x00, { 0x1c, 0x14, 0x14, 0x01, 0x03, 0x00, 0x00, 0x00, 0 } },
            { { 0x1c, 0x14, 0x14, 0x01, 0x03, 0x00, 0x00, 0x00, 0 }, 0x00, 0x40, 0x00, { 0x00, 0x14, 0x19, 0x01, 0x03, 0x01, 0x0c, 0x00, 0 } },
            { { 0x00, 0x19, 0x1e, 0x02, 0x03, 0x01, 0x08, 0x00, 0 }, 0x1c, 0x60, 0x20, { 0x1f, 0x1e, 0x1e, 0x00, 0x03, 0x01, 0x07, 0x00, 0 } },
            { { 0x0b, 0x14, 0x19, 0x02, 0x03, 0x01, 0x0b, 0x00, 1 }, 0x04, 0x50, 0x40, { 0x0a, 0x14, 0x19, 0x00, 0x03, 0x01, 0x0a, 0x00, 1 } },
            { { 0x1c, 0x14, 0x14, 0x01, 0x03, 0x00, 0x00, 0x00, 2 }, 0x14, 0xa0, 0x00, { 0x19, 0x0f, 0x19, 0x01, 0x03, 0x01, 0x0c, 0x00, 2 } },
            { { 0x15, 0x1e, 0x23, 0x00, 0x03, 0x01, 0x01, 0x00, 2 }, 0x14, 0xa0, 0x00, { 0x14, 0x1e, 0x28, 0x01, 0x03, 0x02, 0x0c, 0x00, 2 } },
        };
        check_swim_steps(mermaid, sizeof mermaid / sizeof mermaid[0], 1);
        check_swim_steps(flippers, sizeof flippers / sizeof flippers[0], 0);
        /* objectSpeedTable as getPositionOffsetForVelocity reads it: SPEED_120 at 45 degrees down-left, $cc a frame on
         * each axis; none without a motion. */
        OraclesTables speeds;
        memset(&speeds, 0, sizeof speeds);
        speeds.object_speed_table = (OraclesSym){ 3, 0x409b };
        uint8_t *table = rom + 3u * 0x4000u + (0x409bu - 0x4000u) - 0x50u + 16u * 0x2du + 2u * 0x14u;
        table[0] = 0xcc; table[1] = 0x00; table[16] = 0x34; table[17] = 0xff;
        int vy = 0, vx = 0;
        CHECK(oracles_room_transition_swim_velocity(rom, rom_size, &speeds, 0x2d, 0x14, &vy, &vx) && vy == 0xcc && vx == -0xcc);
        CHECK(oracles_room_transition_swim_velocity(rom, rom_size, &speeds, 0x2d, 0xff, &vy, &vx) && vy == 0 && vx == 0);
    }

    if (failures) return 1;
    puts("test_room_transition_policy: ok");
    oracles_room_transition_stop(tr);
    free(rom);
    return 0;
}
