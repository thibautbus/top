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
static void play(const run *r, int *out_x, int *out_y)
{
    OraclesGuestTransitionState s;
    memset(&s, 0, sizeof s);
    s.game = ORACLES_GAME_AGES; s.game_state = 2; s.cutscene_index = 1; s.scroll_mode = 8;
    s.transition_direction = (uint8_t)r->dir; s.link_object_index = 0xd0; s.link_state = 1; s.link_visible = 0x80;
    s.link_anim_mode = 0x10; s.animation_data_bank = 6; s.link_anim_pointer = 0x4000;
    s.active_room = r->room;
    s.boundary_x = 154; s.boundary_y = 121;   /* a small room's (wScreenTransitionBoundaryX/Y) */
    s.link_input_angle = r->angle; s.link_move_angle = r->sea_floor ? r->move_angle : r->angle; s.tile_types_bank = 0xff;
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
        end_respawn_y = m.respawn_y;
        end_respawn_x = m.respawn_x;
    }
    *out_x = x; *out_y = y;
    frame_counter += 10;
}

int main(void)
{
    const size_t rom_size = 8u * 0x4000u;
    uint8_t *rom = calloc(rom_size, 1);
    CHECK(rom != NULL);
    if (!rom) return 1;
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
    }
    CHECK(!oracles_room_transition_set_steps(tr, 0));

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

    if (failures) return 1;
    puts("test_room_transition_policy: ok");
    oracles_room_transition_stop(tr);
    free(rom);
    return 0;
}
