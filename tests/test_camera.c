/* The Enhanced camera without a ROM: the pure world-coordinate math, and the
 * vendored reducer driven through bootstrap, tracking, recentring, a
 * serialise/restore round-trip and a lost observation. */
#include "camera.h"
#include "compositor.h"
#include "guest_struct_offsets.h"
#include "ppu.h"
#include "view.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)
/* The normal surface; the drawn-back one is test_zoom_out.c's. */
#define NORMAL_W ORACLES_ENHANCED_NARROW_WIDTH
#define NORMAL_PIXELS (ORACLES_ENHANCED_NARROW_WIDTH * ORACLES_ENHANCED_NARROW_HEIGHT)
#define NORMAL_HUD_X 48u
#define NORMAL_BAND_H 128u

static const OraclesCompatProfile *fixture_profile(void)
{
    const OraclesRomInfo info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_AGES_US, .known = 1 };
    return oracles_compat_find(&info);
}

static OraclesE11Observation obs_at(const char *session, uint64_t ordinal, int32_t link_px, int32_t bounds_px)
{
    OraclesE11Observation o;
    oracles_e11_observation_initial(&o);
    snprintf(o.session, sizeof o.session, "%s", session);
    snprintf(o.snapshot_identity, sizeof o.snapshot_identity, "s%llu", (unsigned long long)ordinal);
    o.epoch = 1;
    o.ordinal = ordinal;
    o.tick = ordinal;
    o.link_x = link_px * ORACLES_E11_F256;
    o.link_y = 64 * ORACLES_E11_F256;
    o.bounds_origin_x = 0;
    o.bounds_origin_y = 0;
    o.bounds_width = (int64_t)bounds_px * ORACLES_E11_F256;
    o.bounds_height = 128 * ORACLES_E11_F256;
    o.domain = ORACLES_E11_EXTERIOR;
    o.availability = ORACLES_E11_AVAILABLE;
    o.event = ORACLES_E11_NORMAL_VBLANK;
    return o;
}


/* Link walking at a fraction of a pixel a frame, as the guest reports him: in
 * whole pixels, so a step of one every few frames.  Returns the largest gap,
 * in 1/256 pixel, between the camera and Link's nominal framing once the
 * camera is up to speed, and counts the frames the camera moved backwards
 * while Link was going forwards. */
static int64_t slow_walk(unsigned profile, int num, int den, int sign, unsigned frames, unsigned *backwards)
{
    OraclesE11Config config;
    OraclesE11State state;
    oracles_e11_config_profile(&config, profile);
    oracles_e11_state_initial(&state, &config);
    const int32_t bounds = 40 * 160, start = 3000;
    int64_t worst = 0, last_camera = 0;
    unsigned back = 0;
    for (uint64_t frame = 1; frame <= frames; frame++) {
        const int32_t link = start + sign * (int32_t)(((frame - 1) * (uint64_t)num) / (uint64_t)den);
        OraclesE11Observation o = obs_at("ages", frame, link, bounds);
        OraclesE11Result r = oracles_e11_reduce(&state, &o, &config);
        if (r.error != ORACLES_E11_OK) { failures++; return -1; }
        state = r.state;
        if (frame > 1) {
            const int64_t step = state.camera_pos - last_camera;
            if (sign > 0 ? step < 0 : step > 0) back++;
        }
        last_camera = state.camera_pos;
        if (frame > 120) {   /* past the run-up */
            int64_t gap = (int64_t)link * ORACLES_E11_F256 - config.framing - state.camera_pos;
            if (gap < 0) gap = -gap;
            if (gap > worst) worst = gap;
        }
    }
    if (backwards) *backwards = back;
    return worst;
}

/* Link's place to the 256th of a pixel, as the sub-pixel camera feeds it
 * (`exact`), else in whole pixels: `steps[f]` his move into frame f, from
 * 3000 px, the pad held along the axis on the frames `held` says (none if
 * NULL).  The camera's place after each frame in `cameras`, its look-ahead
 * in `lookaheads` if not NULL. */
static void walk_with(unsigned profile, int exact, const int32_t *steps, const bool *held, unsigned frames, int32_t *cameras, int64_t *lookaheads)
{
    OraclesE11Config config;
    OraclesE11State state;
    oracles_e11_config_profile(&config, profile);
    oracles_e11_state_initial(&state, &config);
    int32_t link = 3000 * ORACLES_E11_F256;
    for (unsigned f = 0; f < frames; f++) {
        link += steps[f];
        OraclesE11Observation o = obs_at("ages", f + 1u, 0, 40 * 160);
        o.link_x = exact ? link : link / ORACLES_E11_F256 * ORACLES_E11_F256;
        o.subpixel = exact != 0;
        o.held = held && held[f];
        OraclesE11Result r = oracles_e11_reduce(&state, &o, &config);
        if (r.error != ORACLES_E11_OK || r.state.status != ORACLES_E11_TRACKING) failures++;
        state = r.state;
        cameras[f] = state.camera_pos;
        if (lookaheads) lookaheads[f] = state.lookahead;
    }
}

/* A frame not seen ends a reducer's move with the follow that settles: a re-centre under way, or a reducer started
 * afresh (held short of a room still being prepared) still moving on to a Link who has not moved since; never the
 * follow's own braking or look-ahead, nor the first profile's moves. */
static void unseen_ends(void)
{
    OraclesE11Config config, first;
    OraclesE11State state;
    oracles_e11_config_profile(&config, 2);
    oracles_e11_config_profile(&first, 1);
    oracles_e11_state_initial(&state, &config);
    state.status = ORACLES_E11_TRACKING;
    state.follow_recenter = ORACLES_E11_FOLLOW;
    state.applied_velocity = 0;
    CHECK(!oracles_enhanced_camera_unseen_ends(&state, &config, 1));   /* fresh, standing: nothing under way */
    state.applied_velocity = 300;
    CHECK(oracles_enhanced_camera_unseen_ends(&state, &config, 1));    /* fresh, gliding over to Link */
    CHECK(!oracles_enhanced_camera_unseen_ends(&state, &config, 0));   /* the follow's own move: kept */
    CHECK(!oracles_enhanced_camera_unseen_ends(&state, &first, 1));    /* the first profile */
    state.follow_recenter = ORACLES_E11_RECENTER;
    CHECK(oracles_enhanced_camera_unseen_ends(&state, &config, 0));    /* a re-centre */
    state.status = ORACLES_E11_DESYNCHRONIZED;
    CHECK(!oracles_enhanced_camera_unseen_ends(&state, &config, 1));   /* not tracking */
}

/* Link walked `walked` frames at `speed` 256ths of a pixel a frame (whole pixels unless `exact`), then put `placed`
 * pixels further: the reducer's state and the observation it last took. */
static void walked_then(unsigned profile, int exact, int32_t speed, unsigned walked, int32_t placed, OraclesE11State *state, OraclesE11Observation *last)
{
    OraclesE11Config config;
    oracles_e11_config_profile(&config, profile);
    oracles_e11_state_initial(state, &config);
    int32_t link = 3000 * ORACLES_E11_F256;
    for (unsigned f = 0; f <= walked; f++) {
        if (f) link += speed;
        if (f == walked) link += placed * ORACLES_E11_F256;
        *last = obs_at("ages", f + 1u, 0, 40 * 160);
        last->link_x = exact ? link : link / ORACLES_E11_F256 * ORACLES_E11_F256;
        last->subpixel = exact != 0;
        const OraclesE11Result r = oracles_e11_reduce(state, last, &config);
        CHECK(r.error == ORACLES_E11_OK && r.state.status == ORACLES_E11_TRACKING);
        *state = r.state;
    }
}

/* A stretch not seen brings the follow that settles to rest at once, where the frames to come would bring it with
 * Link standing still, its look-ahead let go, a re-centre under way to its end: after it the camera moves no more,
 * and the reducer goes on taking the next frame's observation.  Already at rest, after another observation or with
 * the first profile, nothing moves. */
static void settle_at_rest(void)
{
    OraclesE11Config config, first;
    oracles_e11_config_profile(&config, 2);
    oracles_e11_config_profile(&first, 1);
    const struct { int exact; int32_t speed; unsigned walked; int32_t placed; } cases[] = {
        { 1, 384, 90, 0 },    /* Smooth: a walk at a pixel and a half a frame, then a stop */
        { 0, 256, 90, 0 },    /* Pixel: a walk at a pixel a frame */
        { 1, 0, 120, -8 },    /* standing, then put eight pixels back (a scene's end, under its white) */
        { 0, 170, 60, 0 },    /* Pixel: under a pixel a frame, still frames between the steps */
        { 0, 0, 120, 23 },    /* standing, then put 23 pixels on: a re-centre */
    };
    for (size_t k = 0; k < sizeof cases / sizeof cases[0]; k++) {
        OraclesE11State state, settled;
        OraclesE11Observation last;
        walked_then(2, cases[k].exact, cases[k].speed, cases[k].walked, cases[k].placed, &state, &last);
        CHECK(state.applied_velocity != 0);
        CHECK((state.follow_recenter == ORACLES_E11_RECENTER) == (cases[k].placed > 8));
        /* The camera left to the frames to come, Link standing. */
        OraclesE11State glide = state;
        OraclesE11Observation o = last;
        for (unsigned f = 0; f < 600; f++) {
            o.ordinal = o.tick = glide.last_ordinal + 1u;
            snprintf(o.snapshot_identity, sizeof o.snapshot_identity, "g%u", f);
            glide = oracles_e11_reduce(&glide, &o, &config).state;
        }
        settled = state;
        CHECK(oracles_enhanced_camera_settle(&settled, &last, &config) == 1);
        CHECK(settled.status == ORACLES_E11_TRACKING && settled.applied_velocity == 0);
        CHECK(settled.last_ordinal == state.last_ordinal && strcmp(settled.last_observation_digest, state.last_observation_digest) == 0);
        const int64_t gap = (int64_t)settled.camera_pos - glide.camera_pos;
        CHECK(gap >= -1 && gap <= 1);   /* where the glide ends */
        /* The frames after: accepted, the camera standing. */
        o = last;
        for (unsigned f = 0; f < 120; f++) {
            o.ordinal = o.tick = last.ordinal + 1u + f;
            snprintf(o.snapshot_identity, sizeof o.snapshot_identity, "n%u", f);
            const OraclesE11Result r = oracles_e11_reduce(&settled, &o, &config);
            CHECK(r.error == ORACLES_E11_OK && r.state.status == ORACLES_E11_TRACKING && r.state.camera_pos == settled.camera_pos);
            settled = r.state;
        }
        /* At rest: nothing to do. */
        CHECK(oracles_enhanced_camera_settle(&settled, &o, &config) == 0);
    }
    OraclesE11State state, kept;
    OraclesE11Observation last;
    walked_then(2, 1, 384, 90, 0, &state, &last);
    kept = state;
    last.ordinal++;   /* not the observation it took last */
    CHECK(oracles_enhanced_camera_settle(&kept, &last, &config) == 0 && kept.camera_pos == state.camera_pos);
    walked_then(1, 0, 256, 90, 0, &state, &last);
    kept = state;
    CHECK(oracles_enhanced_camera_settle(&kept, &last, &first) == 0 && kept.camera_pos == state.camera_pos);
}

/* The whole pixel the camera at rest is shown on: Link's sprite, drawn a frame late, stepping by the pixels still
 * pending at the next frame, the whole pixels the camera then gains (toward zero, as shown_correction takes them) are
 * his step, the whole camera standing; with no step pending, the nearest.  Then the frames washed out by a fade that
 * bring the follow to rest: the eighth running, not the seventh, never while a menu is open, counted afresh after a
 * frame that is not. */
static void rest_whole_and_fade(void)
{
    const int64_t places[] = { 0, 1, 33, 128, 129, 255, 256, 290202, -1, -128, -129, -223, -256, -26496 };
    const int32_t steps[] = { 1, -1, 2, -2, 9, -9, 6 };
    for (size_t i = 0; i < sizeof places / sizeof places[0]; i++) {
        const int64_t pos = places[i];
        for (size_t k = 0; k < sizeof steps / sizeof steps[0]; k++) {
            const int32_t whole = oracles_enhanced_rest_whole(pos, steps[k]);
            const int64_t left = (int64_t)whole * ORACLES_E11_F256 - pos;   /* the gap the frame at rest shows */
            CHECK(left > -ORACLES_E11_F256 && left < ORACLES_E11_F256);
            CHECK((int64_t)(left / ORACLES_E11_F256) == 0);               /* that frame keeps its whole pixel */
            const int64_t next = (int64_t)steps[k] * ORACLES_E11_F256 + left;
            CHECK(next / ORACLES_E11_F256 == steps[k]);                   /* the step's: the whole camera stands */
        }
        const int32_t nearest = oracles_enhanced_rest_whole(pos, 0);
        const int64_t gap = (int64_t)nearest * ORACLES_E11_F256 - pos;
        CHECK(gap >= -ORACLES_E11_F256 / 2 && gap <= ORACLES_E11_F256 / 2);
    }
    unsigned run = 0;
    for (unsigned f = 1; f < ORACLES_ENHANCED_SETTLE_RUN; f++) run = oracles_enhanced_faded_run(run, 1, 0);
    CHECK(run == ORACLES_ENHANCED_SETTLE_RUN - 1u);                       /* seven frames: no rest yet */
    run = oracles_enhanced_faded_run(run, 1, 0);
    CHECK(run == ORACLES_ENHANCED_SETTLE_RUN);                            /* the eighth */
    CHECK(oracles_enhanced_faded_run(run, 1, 0) == ORACLES_ENHANCED_SETTLE_RUN);
    CHECK(oracles_enhanced_faded_run(run, 0, 0) == 0);                    /* a frame seen */
    run = 0;
    for (unsigned f = 0; f < 20u; f++) run = oracles_enhanced_faded_run(run, 1, 1);
    CHECK(run == 0);                                                      /* a menu's fade, however long */
    run = oracles_enhanced_faded_run(ORACLES_ENHANCED_SETTLE_RUN, 1, 1);
    CHECK(run == 0);                                                      /* a menu opening in a fade */
}

/* The band's fraction taken up or given back by a sixteenth of a pixel a frame, only while the camera moves:
 * standing, it holds where it stood. */
static void band_fraction_still(void)
{
    int32_t ease = 0;
    /* The fraction comes back (a scene's end), the camera standing: the band holds at none. */
    CHECK(oracles_enhanced_band_fraction(1, 1, 1, 0, 192, 192, 0, &ease) == 0 && ease == -192);
    CHECK(oracles_enhanced_band_fraction(1, 1, 1, 1, 192, 192, 0, &ease) == 0 && ease == -192);
    /* The camera moves: the band goes by SUB_EASE toward it. */
    CHECK(oracles_enhanced_band_fraction(1, 0, 1, 1, 192, 192, 0, &ease) == 16 && ease == -176);
    /* The same comeback with the camera moving: eased from the first frame. */
    ease = 0;
    CHECK(oracles_enhanced_band_fraction(1, 0, 1, 0, 192, 192, 0, &ease) == 16);
    /* Given back, standing: held at the fraction it showed. */
    ease = 0;
    CHECK(oracles_enhanced_band_fraction(1, 1, 0, 1, 192, 192, 192, &ease) == 192 && ease == 192);
    /* No frame before: the target at once. */
    ease = 50;
    CHECK(oracles_enhanced_band_fraction(0, 1, 1, 0, 100, 0, 0, &ease) == 100 && ease == 0);
}

static void exact_walk(unsigned profile, const int32_t *steps, unsigned frames, int32_t *cameras)
{
    walk_with(profile, 1, steps, NULL, frames, cameras, NULL);
}

int main(void)
{
    /* ---- world-coordinate math ---- */
    OraclesEnhancedWorld w;
    /* Small overworld room, column 3 row 2, Link at (40,50): world grid. */
    CHECK(oracles_enhanced_world_from_room(0, 0x23, 0, 40, 50, 16, 14, 0, 0, 0, 0, 0, 0x23, 0, &w) == 1);
    CHECK(w.domain == ORACLES_E11_EXTERIOR);
    CHECK(w.link_x == 3 * 160 + 40);
    CHECK(w.link_y == 2 * 128 + 50);
    CHECK(w.origin_x == 3 * 160 && w.origin_y == 2 * 128);
    CHECK(w.bounds_width == 14 * 160 && w.wide);   /* Ages' map is 14 rooms wide: the camera stops there */
    /* During a leftward scroll Link's coordinate runs negative, still relative to the room he leaves. */
    CHECK(oracles_enhanced_world_from_room(0, 0x23, 0, -9, 50, 16, 14, 0, 0, 0, 0, 0, 0x23, 0, &w) == 1 && w.link_x == 3 * 160 - 9);
    /* The past overworld is its own epoch-worthy domain. */
    CHECK(oracles_enhanced_world_from_room(1, 0x23, 0, 40, 50, 16, 14, 0, 0, 0, 0, 0, 0x23, 0, &w) == 1 && w.domain == ORACLES_E11_ERA);
    /* An interior is on the same 16-wide grid, in its own domain; alone, it
     * stands centred in the viewport; open to the right and above, its
     * extent takes those rooms in. */
    CHECK(oracles_enhanced_world_from_room(2, 0x11, 0, 80, 40, 16, 14, 0, 0, 0, 0, 0, 0x11, 0, &w) == 1);
    CHECK(w.domain == ORACLES_E11_INTERIOR && w.wide && w.link_x == 160 + 80 && w.origin_x == 160);
    CHECK(w.bounds_width == 256 && w.bounds_origin_x == 160 - 48 && w.bounds_height == 128 && w.bounds_origin_y == 128);
    CHECK(oracles_enhanced_world_from_room(2, 0x11, 0, 80, 40, 16, 14, 0, 0, 0, (1u << 1) | 1u, 0, 0x11, 0, &w) == 1);
    CHECK(w.bounds_width == 320 + 48 && w.bounds_origin_x == 160 - 48 && w.bounds_height == 256 && w.bounds_origin_y == 0);
    /* An overworld room shown alone (the Maku tree's screen): its own extent, centred, whatever the map. */
    CHECK(oracles_enhanced_world_from_room(0, 0x38, 0, 80, 40, 16, 14, 0, 0, 0, 0, 1, 0x38, 0, &w) == 1);
    CHECK(w.domain == ORACLES_E11_EXTERIOR && w.bounds_width == 256 && w.bounds_origin_x == 8 * 160 - 48 && w.bounds_height == 128 && w.bounds_origin_y == 3 * 128);
    /* A map of its own rows (Subrosia's 11 x 8, Moonrise's 8 x 8): the camera stops at its last row, not the grid's. */
    CHECK(oracles_enhanced_world_from_room(1, 0x72, 0, 80, 100, 16, 11, 8, 0, 0, 0, 0, 0x72, 0, &w) == 1);
    CHECK(w.domain == ORACLES_E11_ERA && w.bounds_width == 11 * 160 && w.bounds_height == 8 * 128 && w.link_y == 7 * 128 + 100);
    /* A map walled inside the grid (Temple of Seasons' Subrosia, columns 4 to 10): the camera stops at its first
     * column as at its last, the room at its place on the grid. */
    CHECK(oracles_enhanced_world_from_room(1, 0x47, 0, 80, 100, 16, 11, 8, 4, 0, 0, 0, 0x47, 0, &w) == 1);
    CHECK(w.bounds_origin_x == 4 * 160 && w.bounds_width == 7 * 160 && w.bounds_origin_y == 0 && w.bounds_height == 8 * 128 && w.origin_x == 7 * 160);
    CHECK(oracles_enhanced_world_from_room(0, 0x5d, 0, 80, 100, 16, 16, 12, 13, 3, 0, 0, 0x5d, 0, &w) == 1);   /* its Holodrum, 13-15 x 3-11 */
    CHECK(w.bounds_origin_x == 13 * 160 && w.bounds_width == 3 * 160 && w.bounds_origin_y == 3 * 128 && w.bounds_height == 9 * 128);
    /* A room of an overworld group off its map (a house the profile's tileset map rules find there): laid out as an
     * interior, alone or with the rooms it opens onto, but in its group's domain, as the Maku tree's screen, so that
     * a scroll between it and a room of the map never lowers the epoch the reducer follows (it ignores a lower one:
     * the camera would stay behind). */
    CHECK(oracles_enhanced_world_from_room(0, 0xe0, 0, 80, 40, 16, 8, 8, 0, 0, 0, ORACLES_ENHANCED_INDOORS, 0xe0, 0, &w) == 1);
    CHECK(w.domain == ORACLES_E11_EXTERIOR && w.alone && w.bounds_width == 256 && w.bounds_origin_x == -48
          && w.bounds_height == 128 && w.bounds_origin_y == 14 * 128);
    CHECK(oracles_enhanced_world_from_room(1, 0xa1, 0, 80, 40, 16, 8, 8, 0, 0, 0, ORACLES_ENHANCED_INDOORS, 0xa1, 0, &w) == 1);
    CHECK(w.domain == ORACLES_E11_ERA && w.alone);   /* Moonrise's group 1 dungeon: the past's domain, as its map's */
    /* An interior laid out as a room off the map (a game whose interiors are areas, its caves): its own domain, the
     * drawn-back band's width, the room centred in it; the normal band keeps the pinned 256. */
    CHECK(oracles_enhanced_world_from_room(3, 0x38, 0, 80, 40, 16, 8, 8, 0, 0, 1u, ORACLES_ENHANCED_INDOORS, 0x38, 480, &w) == 1);
    CHECK(w.domain == ORACLES_E11_INTERIOR && w.alone && w.viewport == 480 && w.bounds_width == 480 && w.bounds_origin_x == 8 * 160 - 160
          && w.bounds_origin_y == 2 * 128 && w.bounds_height == 256);
    CHECK(oracles_enhanced_world_from_room(3, 0x38, 0, 80, 40, 16, 8, 8, 0, 0, 1u, ORACLES_ENHANCED_INDOORS, 0x38, 256, &w) == 1 && w.viewport == 256);
    CHECK(oracles_enhanced_world_from_room(3, 0x38, 0, 80, 40, 16, 8, 8, 0, 0, 1u, 0, 0x38, 480, &w) == 1 && w.viewport == 256);
    /* Link's coordinate byte, signed by the scroll: leftward past zero it wraps, rightward it runs past 240 (a large room). */
    CHECK(oracles_enhanced_signed_coordinate(247, 1, 3, 0, 1) == -9);    /* scrolling left, x */
    CHECK(oracles_enhanced_signed_coordinate(247, 1, 1, 0, 0) == 247);   /* scrolling right, x: past the edge of a large room */
    CHECK(oracles_enhanced_signed_coordinate(250, 1, 0, 1, 1) == -6);    /* scrolling up, y */
    CHECK(oracles_enhanced_signed_coordinate(193, 1, 1, 0, 1) == 193);   /* scrolling right, x: a swim past 32 px, a small room */
    CHECK(oracles_enhanced_signed_coordinate(200, 1, 2, 1, 1) == 200);   /* scrolling down, y */
    CHECK(oracles_enhanced_signed_coordinate(193, 0, 1, 0, 1) == -63);   /* out of a scroll: past 0xc0, negative as before */
    CHECK(oracles_enhanced_signed_coordinate(200, 1, 2, 1, 0) == 200);   /* scrolling down, y: past the edge of a large room */
    CHECK(oracles_enhanced_signed_coordinate(247, 0, 3, 0, 0) == 247);   /* not scrolling, a large room: unsigned */
    CHECK(oracles_enhanced_signed_coordinate(248, 0, 3, 1, 1) == -8);    /* a small room: past 0xc0 it is negative in any state */
    CHECK(oracles_enhanced_signed_coordinate(180, 0, 3, 1, 1) == 180);
    CHECK(oracles_enhanced_signed_coordinate(250, 0, 2, 1, 0) == -6);    /* a large room, y past 0xe0: negative */
    CHECK(oracles_enhanced_signed_coordinate(240, 0, 1, 0, 0) == 240);   /* a large room's x reaches 240 */
    CHECK(oracles_enhanced_signed_coordinate(231, 1, 3, 0, 1) == -25);   /* a continuous transition takes him further */
    CHECK(oracles_enhanced_signed_coordinate(100, 1, 3, 0, 1) == 100);
    /* A large dungeon room: on its own 16-wide grid of 240x176 cells, alone widened to the viewport with gutters; open below, its extent takes the room below in. */
    CHECK(oracles_enhanced_world_from_room(4, 0x35, 1, 100, 60, 16, 14, 0, 0, 0, 0, 0, 0x35, 0, &w) == 1);
    CHECK(w.domain == ORACLES_E11_DUNGEON && w.wide && w.bounds_width == 256 && w.bounds_origin_x == 5 * 240 - 8 && w.link_x == 5 * 240 + 100);
    CHECK(w.origin_y == 3 * 176 && w.link_y == 3 * 176 + 60 && w.bounds_height == 176 && w.bounds_origin_y == 3 * 176);
    CHECK(oracles_enhanced_world_from_room(4, 0x35, 1, 100, 60, 16, 14, 0, 0, 0, 1u << 2, 0, 0x35, 0, &w) == 1 && w.bounds_height == 352 && w.bounds_origin_y == 3 * 176);
    /* A dungeon room sits where its floor's map puts it, not where its index would. */
    CHECK(oracles_enhanced_world_from_room(4, 0x2c, 1, 100, 60, 16, 14, 0, 0, 0, 0, 0, 0x21, 0, &w) == 1 && w.origin_x == 1 * 240 && w.origin_y == 2 * 176);

    /* ---- reducer: bootstrap, then follow Link walking right ---- */
    OraclesE11Config config;
    OraclesE11State state;
    oracles_e11_config_default(&config);
    oracles_e11_state_initial(&state, &config);
    const int32_t bounds = 16 * 160;   /* a wide overworld row */

    OraclesE11Result r = oracles_e11_reduce(&state, &(OraclesE11Observation){0}, &config);
    /* An empty observation is invalid, not a crash. */
    CHECK(r.error == ORACLES_E11_INVALID_OBSERVATION || r.error == ORACLES_E11_OK);

    OraclesE11Observation o = obs_at("ages", 1, 400, bounds);
    r = oracles_e11_reduce(&state, &o, &config);
    state = r.state;
    CHECK(state.status == ORACLES_E11_TRACKING);
    const int32_t first = state.camera_pos;

    int32_t last = first;
    int moved_right = 0;
    for (uint64_t frame = 2; frame < 80; frame++) {
        o = obs_at("ages", frame, 400 + (int32_t)frame * 3, bounds);   /* Link walks right */
        r = oracles_e11_reduce(&state, &o, &config);
        CHECK(r.error == ORACLES_E11_OK);
        state = r.state;
        CHECK(state.status == ORACLES_E11_TRACKING);
        if (state.camera_pos > last) moved_right = 1;
        last = state.camera_pos;
    }
    CHECK(moved_right && last > first);   /* the camera followed Link to the right */

    /* Determinism: the same walk from the same start gives the same camera. */
    OraclesE11State replay;
    oracles_e11_state_initial(&replay, &config);
    for (uint64_t frame = 1; frame < 80; frame++) {
        o = obs_at("ages", frame, 400 + (frame >= 2 ? (int32_t)frame * 3 : 0), bounds);
        replay = oracles_e11_reduce(&replay, &o, &config).state;
    }
    CHECK(replay.camera_pos == last);

    /* Serialise and restore round-trips the tracking state exactly. */
    uint8_t wire[ORACLES_E11_STATE_WIRE_MAX];
    size_t written = 0;
    CHECK(oracles_e11_state_serialize(&state, wire, sizeof wire, &written) == ORACLES_E11_OK);
    OraclesE11State restored;
    CHECK(oracles_e11_state_restore(wire, written, &config, &restored) == ORACLES_E11_OK);
    CHECK(memcmp(&restored, &state, sizeof state) == 0);

    /* ---- the second pinned configuration: valid, its own digest, its states bound to it ---- */
    {
        OraclesE11Config v2, v4;
        oracles_e11_config_profile(&v2, 2);
        CHECK(oracles_e11_config_valid(&v2));
        CHECK(strcmp(v2.digest, ORACLES_E11_CONFIG_DIGEST_V5) == 0 && strcmp(v2.schema, ORACLES_E11_CONFIG_SCHEMA_V5) == 0);
        CHECK(v2.dead_half_width == 2 * ORACLES_E11_F256 && v2.max_speed == 2 * ORACLES_E11_F256);
        CHECK(oracles_e11_state_restore(wire, written, &v2, &restored) != ORACLES_E11_OK);   /* a v1 state is not a v2 state */
        /* The second as it followed before it settled, for comparison: its own digest, the same values. */
        oracles_e11_config_profile(&v4, 3);
        CHECK(oracles_e11_config_valid(&v4));
        CHECK(strcmp(v4.digest, ORACLES_E11_CONFIG_DIGEST_V4) == 0 && strcmp(v4.schema, ORACLES_E11_CONFIG_SCHEMA_V4) == 0);
        CHECK(v4.dead_half_width == v2.dead_half_width && v4.lookahead_max == v2.lookahead_max && v4.max_speed == v2.max_speed && v4.max_accel == v2.max_accel);
        OraclesE11Config v1;
        oracles_e11_config_profile(&v1, 1);
        CHECK(memcmp(&v1, &config, sizeof v1) == 0);
        /* Link walks right at one pixel per frame from the centre of the view.
         * Under the first configuration the camera stands still while he
         * crosses the dead zone, then catches up; under the second it moves
         * with him, never more than two pixels a frame. */
        unsigned still_v1 = 0, still_v2 = 0, largest_v2 = 0;
        int32_t last_v1 = 0, last_v2 = 0;
        OraclesE11State s1, s2;
        oracles_e11_state_initial(&s1, &v1);
        oracles_e11_state_initial(&s2, &v2);
        for (uint64_t frame = 1; frame < 200; frame++) {
            const int32_t link = 400 + (frame > 30 ? (int32_t)(frame - 30) : 0);
            o = obs_at("ages", frame, link, bounds);
            s1 = oracles_e11_reduce(&s1, &o, &v1).state;
            s2 = oracles_e11_reduce(&s2, &o, &v2).state;
            if (frame > 40 && frame < 200) {
                const int32_t d1 = s1.camera_pos - last_v1, d2 = s2.camera_pos - last_v2;
                const unsigned m2 = (unsigned)(d2 < 0 ? -d2 : d2);
                if (d1 == 0) still_v1++;
                if (d2 == 0) still_v2++;
                if (m2 > largest_v2) largest_v2 = m2;
            }
            last_v1 = s1.camera_pos; last_v2 = s2.camera_pos;
        }
        CHECK(s1.status == ORACLES_E11_TRACKING && s2.status == ORACLES_E11_TRACKING);
        CHECK(still_v1 > 10);                         /* the dead zone of sixteen pixels: the camera waits */
        CHECK(still_v2 == 0);                         /* the camera never stops while Link walks */
        CHECK(largest_v2 <= 2u * ORACLES_E11_F256);   /* and never hurries */
        /* Link stops: under the second configuration the camera brakes in
         * one direction and stays where it stopped, no backward twitch, no
         * recentring later. */
        int32_t stop_link = 400 + 169, last_cam = s2.camera_pos;
        int reversed = 0, braking = 0;
        int32_t rest_pos = 0;
        for (uint64_t frame = 200; frame < 400; frame++) {
            o = obs_at("ages", frame, stop_link, bounds);
            s2 = oracles_e11_reduce(&s2, &o, &v2).state;
            const int32_t d = s2.camera_pos - last_cam;
            if (d < 0) reversed = 1;
            if (d > 0) braking++;
            if (frame == 260) rest_pos = s2.camera_pos;
            last_cam = s2.camera_pos;
        }
        CHECK(s2.status == ORACLES_E11_TRACKING);
        CHECK(!reversed);                             /* never a step back */
        CHECK(braking > 0 && braking < 30);           /* a short braking, then still */
        CHECK(s2.camera_pos == rest_pos);             /* and no recentring 140 frames later */
        CHECK(s2.follow_recenter == ORACLES_E11_FOLLOW);

        /* Under one pixel a frame -- Link swimming -- the guest reports a step
         * of one pixel every few frames.  The camera must follow him all the
         * same: at these speeds the look-ahead is under three pixels (the
         * filtered velocity times the eight-tick horizon at half a pixel a
         * tick), far below the twenty-four the configuration allows, so eight
         * pixels of gap leaves room for the dead zone and the rounding and
         * still fails on a camera that stalls -- it drifts to the edge of the
         * band, sixty-five pixels and more. */
        static const int rates[][2] = { { 1, 4 }, { 2, 5 }, { 1, 2 }, { 3, 4 } };
        for (unsigned rate = 0; rate < sizeof rates / sizeof rates[0]; rate++) {
            for (int sign = 1; sign >= -1; sign -= 2) {
                unsigned backwards = 0;
                const int64_t gap = slow_walk(2u, rates[rate][0], rates[rate][1], sign, 400, &backwards);
                CHECK(gap >= 0 && gap <= 8 * ORACLES_E11_F256);
                CHECK(backwards == 0);
                if (gap > 8 * ORACLES_E11_F256)
                    fprintf(stderr, "  %d/%d px a frame, sign %+d: gap %.2f px\n",
                            rates[rate][0], rates[rate][1], sign, (double)gap / ORACLES_E11_F256);
            }
        }
        /* Link to the 256th of a pixel, as the sub-pixel camera feeds him.
         * On stairs walked diagonally, 90/256 of a pixel a frame each way, the
         * camera settles on his speed: past its run-up it steps by his own
         * step to a few 256ths.  The configuration before it braked inside
         * its dead zone and overtook him again, a cycle from 15 to 190. */
        {
            enum { STAIRS = 240 };
            int32_t steps[STAIRS], settled[STAIRS], cycling[STAIRS];
            for (unsigned f = 0; f < STAIRS; f++) steps[f] = 90;
            exact_walk(2u, steps, STAIRS, settled);
            exact_walk(3u, steps, STAIRS, cycling);
            int32_t low = INT32_MAX, high = INT32_MIN, low_v4 = INT32_MAX, high_v4 = INT32_MIN;
            for (unsigned f = 80; f < STAIRS; f++) {
                const int32_t step = settled[f] - settled[f - 1], step_v4 = cycling[f] - cycling[f - 1];
                if (step < low) low = step;
                if (step > high) high = step;
                if (step_v4 < low_v4) low_v4 = step_v4;
                if (step_v4 > high_v4) high_v4 = step_v4;
            }
            CHECK(low >= 86 && high <= 94);
            CHECK(high_v4 - low_v4 > 100);
            if (low < 86 || high > 94) fprintf(stderr, "  stairs: camera steps %d to %d\n", low, high);
        }
        /* Link walks at a pixel a frame and stops: the first still frame is
         * the stop, the look-ahead let go at once.  The camera sheds its speed
         * at twice the rate it gains it, in four frames, never steps back, and
         * stays; before, it went on for eight, four of them before it saw the
         * stop. */
        {
            enum { WALK = 120, STILL = 60 };
            int32_t steps[WALK + STILL], cameras[WALK + STILL], cameras_v4[WALK + STILL];
            for (unsigned f = 0; f < WALK + STILL; f++) steps[f] = f < WALK ? 256 : 0;
            exact_walk(2u, steps, WALK + STILL, cameras);
            exact_walk(3u, steps, WALK + STILL, cameras_v4);
            unsigned moving = 0, moving_v4 = 0, back = 0;
            for (unsigned f = WALK; f < WALK + STILL; f++) {
                const int32_t step = cameras[f] - cameras[f - 1];
                if (step) moving++;
                if (step < 0) back++;
                if (cameras_v4[f] != cameras_v4[f - 1]) moving_v4++;
            }
            CHECK(moving <= 4 && back == 0);
            CHECK(moving_v4 >= 8);
            CHECK(cameras[WALK + STILL - 1] - cameras[WALK - 1] <= 2 * ORACLES_E11_F256);   /* under two pixels past his stop */
        }
        /* Link turns back: the camera stops going his old way within four
         * frames, then follows him the other way without coming back. */
        {
            enum { OUT = 120, BACK = 120 };
            int32_t steps[OUT + BACK], cameras[OUT + BACK];
            for (unsigned f = 0; f < OUT + BACK; f++) steps[f] = f < OUT ? 256 : -256;
            exact_walk(2u, steps, OUT + BACK, cameras);
            unsigned old_way = 0, turns = 0;
            int32_t last = 0;
            for (unsigned f = OUT; f < OUT + BACK; f++) {
                const int32_t step = cameras[f] - cameras[f - 1];
                if (step > 0) old_way++;
                if (step && last && (step > 0) != (last > 0)) turns++;
                if (step) last = step;
            }
            CHECK(old_way <= 4 && turns == 1);
            CHECK(cameras[OUT + BACK - 1] - cameras[OUT + BACK - 2] == -256);   /* and goes at his speed */
        }
        /* The game puts Link elsewhere (a respawn, 63 pixels back): no walk
         * to smooth, the camera re-centres on him within half a second, never
         * more than four pixels a frame, and stays. */
        {
            enum { BEFORE = 60, AFTER = 60 };
            int32_t steps[BEFORE + AFTER], cameras[BEFORE + AFTER];
            for (unsigned f = 0; f < BEFORE + AFTER; f++) steps[f] = f == BEFORE ? -63 * ORACLES_E11_F256 : 0;
            exact_walk(2u, steps, BEFORE + AFTER, cameras);
            int32_t largest = 0;
            for (unsigned f = BEFORE; f < BEFORE + AFTER; f++) {
                const int32_t step = cameras[f] > cameras[f - 1] ? cameras[f] - cameras[f - 1] : cameras[f - 1] - cameras[f];
                if (step > largest) largest = step;
            }
            const int32_t framed = (3000 - 63) * ORACLES_E11_F256 - 128 * ORACLES_E11_F256;
            const int32_t off = cameras[BEFORE + 29] - framed;
            CHECK(largest <= 4 * ORACLES_E11_F256);
            CHECK(off >= -2 * ORACLES_E11_F256 && off <= 2 * ORACLES_E11_F256);
            CHECK(cameras[BEFORE + AFTER - 1] == cameras[BEFORE + 29]);
        }
        /* The game holds Link a frame, the pad held (a scroll's end, a
         * swim's stroke): no stop, the look-ahead kept and the camera going
         * on at its speed.  The pad let go, the same frame is the stop. */
        {
            enum { WALK = 120, AFTER = 30 };
            int32_t steps[WALK + AFTER], cameras[WALK + AFTER], stopped[WALK + AFTER];
            int64_t ahead[WALK + AFTER], let_go[WALK + AFTER];
            bool held[WALK + AFTER];
            for (unsigned f = 0; f < WALK + AFTER; f++) { steps[f] = f == WALK ? 0 : 256; held[f] = true; }
            walk_with(2u, 1, steps, held, WALK + AFTER, cameras, ahead);
            walk_with(2u, 1, steps, NULL, WALK + AFTER, stopped, let_go);
            CHECK(ahead[WALK] == ahead[WALK - 1]);
            CHECK(cameras[WALK] - cameras[WALK - 1] == cameras[WALK - 1] - cameras[WALK - 2]);
            CHECK(let_go[WALK] != let_go[WALK - 1]);
            CHECK(stopped[WALK] - stopped[WALK - 1] < cameras[WALK] - cameras[WALK - 1]);
        }
        /* In whole pixels, diagonally (0xb5/256 a frame, steps of 1, 1, 0):
         * a still frame between two steps is no stop, the camera's speed
         * keeps within a quarter of a pixel; Link stopping, the camera stops
         * within ten frames and never steps back; turning back, it stops going
         * his old way within eight. */
        {
            enum { WALK = 150, STILL = 40, BACK = 60 };
            int32_t steps[WALK + STILL + BACK], cameras[WALK + STILL + BACK];
            for (unsigned f = WALK; f < WALK + STILL + BACK; f++) steps[f] = f < WALK + STILL ? 0 : -256;
            int32_t carried = 0;
            for (unsigned f = 0; f < WALK; f++) { carried += 0xb5; steps[f] = (carried >> 8) * ORACLES_E11_F256; carried &= 0xff; }
            walk_with(2u, 0, steps, NULL, WALK + STILL + BACK, cameras, NULL);
            int32_t low = INT32_MAX, high = INT32_MIN;
            for (unsigned f = 80; f < WALK; f++) {
                const int32_t step = cameras[f] - cameras[f - 1];
                if (step < low) low = step;
                if (step > high) high = step;
            }
            CHECK(high - low <= ORACLES_E11_F256 / 4);
            unsigned moving = 0, back = 0, old_way = 0;
            for (unsigned f = WALK; f < WALK + STILL; f++) {
                if (cameras[f] != cameras[f - 1]) moving++;
                if (cameras[f] < cameras[f - 1]) back++;
            }
            for (unsigned f = WALK + STILL; f < WALK + STILL + BACK; f++) if (cameras[f] > cameras[f - 1]) old_way++;
            CHECK(moving <= 10 && back == 0);
            CHECK(old_way == 0);
            if (high - low > ORACLES_E11_F256 / 4 || moving > 10 || back || old_way)
                fprintf(stderr, "  whole pixels: speed %d to %d, %u frames moving after the stop, %u back, %u his old way\n", low, high, moving, back, old_way);
        }
        /* The first configuration crosses its sixteen-pixel dead zone and
         * catches up, at a quarter of a pixel a frame as at one: its gap is
         * that dead zone, and it does not grow. */
        unsigned back_v1 = 0;
        const int64_t slow_v1 = slow_walk(1u, 1, 4, 1, 400, &back_v1), fast_v1 = slow_walk(1u, 1, 1, 1, 400, NULL);
        CHECK(slow_v1 <= 16 * ORACLES_E11_F256 && fast_v1 <= 16 * ORACLES_E11_F256 && back_v1 == 0);

        /* A state saved under the previous schema names a configuration this
         * build no longer has: it is refused, not read as a v4 state. */
        uint8_t old_wire[ORACLES_E11_STATE_WIRE_MAX];
        size_t old_written = 0;
        OraclesE11State s2_saved = s2, junk;
        CHECK(oracles_e11_state_serialize(&s2_saved, old_wire, sizeof old_wire, &old_written) == ORACLES_E11_OK);
        memcpy(old_wire + 6, "03125465f8a69be43b2bba18087835478d1239d85cdd5bff29b97b7081579474", 64);
        CHECK(oracles_e11_state_restore(old_wire, old_written, &v2, &junk) != ORACLES_E11_OK);
        CHECK(oracles_e11_state_restore(old_wire, old_written, &v1, &junk) != ORACLES_E11_OK);
    }

    /* Idle for long enough: the reducer switches to recentring. */
    int recentred = 0;
    for (uint64_t frame = 80; frame < 200; frame++) {
        o = obs_at("ages", frame, last / ORACLES_E11_F256 + 128, bounds);   /* Link stands still at rest */
        state = oracles_e11_reduce(&state, &o, &config).state;
        if (state.follow_recenter == ORACLES_E11_RECENTER) recentred = 1;
    }
    CHECK(recentred);

    /* A gap in the ordinals loses the observation: the reducer desynchronises. */
    o = obs_at("ages", 500, 800, bounds);   /* far ahead of the last ordinal */
    state = oracles_e11_reduce(&state, &o, &config).state;
    CHECK(state.status == ORACLES_E11_DESYNCHRONIZED);
    /* The next consecutive observation re-bootstraps into tracking. */
    o = obs_at("ages", 501, 800, bounds);
    state = oracles_e11_reduce(&state, &o, &config).state;
    CHECK(state.status == ORACLES_E11_TRACKING);

    /* ---- compositor ---- */
    {
        /* A core image whose pixel value encodes its (x,y): top 16 lines are the
         * status bar, bottom 128 the game area. */
        uint32_t *core = malloc(160u * 144u * sizeof *core);
        for (unsigned y = 0; y < 144u; y++)
            for (unsigned x = 0; x < 160u; x++)
                core[y * 160u + x] = 0xff000000u | ((uint32_t)x << 8) | y;
        uint32_t *surface = malloc(NORMAL_PIXELS * sizeof *surface);

        /* Framed mode: the core centred at x=48, border elsewhere. */
        OraclesEnhancedCompose fc;
        memset(&fc, 0, sizeof fc);
        fc.mode = ORACLES_ENHANCED_FRAMED;
        fc.border = 0xff112233u;
        oracles_enhanced_compose(core, &fc, surface);
        CHECK(surface[0] == 0xff112233u);                                  /* left gutter is the border */
        CHECK(surface[0 * NORMAL_W + NORMAL_HUD_X] == core[0]);   /* core's (0,0) */
        CHECK(surface[10 * NORMAL_W + NORMAL_HUD_X + 20] == core[10 * 160u + 20]);
        CHECK(surface[0 * NORMAL_W + NORMAL_W - 1] == 0xff112233u); /* right gutter */

        /* World mode: HUD band centred, world band placed by the camera. */
        OraclesEnhancedCompose wc;
        memset(&wc, 0, sizeof wc);
        wc.mode = ORACLES_ENHANCED_WORLD;
        wc.border = 0xff445566u;
        wc.window_world_left = 480;     /* column 3 of a 160px grid, the game's camera at 0 */
        wc.window_world_top = 0;
        wc.world_left = 480 - NORMAL_HUD_X;   /* so the room sits centred in the surface */
        oracles_enhanced_compose(core, &wc, surface);
        /* HUD: the status bar centred, gutters the border colour. */
        CHECK(surface[0] == 0xff445566u);
        CHECK(surface[5 * NORMAL_W + NORMAL_HUD_X + 7] == core[5 * 160u + 7]);
        /* World band: at surface x = HUD_X the world_x is room_world_left, so game-area column 0. */
        {
            const unsigned sy = ORACLES_ENHANCED_HUD_HEIGHT + 30u;   /* game-area row 30 */
            const uint32_t expect = core[(ORACLES_ENHANCED_HUD_HEIGHT + 30u) * 160u + 0u];
            CHECK(surface[sy * NORMAL_W + NORMAL_HUD_X] == expect);
            const uint32_t expect2 = core[(ORACLES_ENHANCED_HUD_HEIGHT + 30u) * 160u + 50u];
            CHECK(surface[sy * NORMAL_W + NORMAL_HUD_X + 50u] == expect2);
            /* Left of the room, no neighbour: black. */
            CHECK(surface[sy * NORMAL_W + 0u] == 0xff000000u);
        }
        /* A neighbour to the left fills what was black. */
        uint32_t *left = malloc(160u * 128u * sizeof *left);
        for (unsigned i = 0; i < 160u * 128u; i++) left[i] = 0xffabcdefu;
        OraclesEnhancedNeighbour nb = { 480 - 160, 0, left, 0, 0, 1, 0, 0u };
        wc.neighbours = &nb;
        wc.neighbour_count = 1;
        const unsigned uncovered_with = oracles_enhanced_compose(core, &wc, surface);
        CHECK(surface[(ORACLES_ENHANCED_HUD_HEIGHT + 30u) * NORMAL_W + 0u] == 0xffabcdefu);
        /* The uncovered count: the right gutter's 48 columns over 128 rows are still black. */
        CHECK(uncovered_with == 48u * NORMAL_BAND_H);
        /* The vertical camera: the band starts 64 lines below the window's top;
         * its lower half is below the room, black until a neighbour covers it. */
        {
            wc.world_top = 64;
            oracles_enhanced_compose(core, &wc, surface);
            CHECK(surface[ORACLES_ENHANCED_HUD_HEIGHT * NORMAL_W + NORMAL_HUD_X] == core[(ORACLES_ENHANCED_HUD_HEIGHT + 64u) * 160u + 0u]);
            CHECK(surface[(ORACLES_ENHANCED_HUD_HEIGHT + 64u) * NORMAL_W + NORMAL_HUD_X] == 0xff000000u);
            OraclesEnhancedNeighbour below = { 480, 128, left, 0, 0, 1, 0, 0u };
            wc.neighbours = &below;
            oracles_enhanced_compose(core, &wc, surface);
            CHECK(surface[(ORACLES_ENHANCED_HUD_HEIGHT + 64u) * NORMAL_W + NORMAL_HUD_X] == 0xffabcdefu);
            CHECK(surface[(ORACLES_ENHANCED_HUD_HEIGHT + 63u) * NORMAL_W + NORMAL_HUD_X] == core[(ORACLES_ENHANCED_HUD_HEIGHT + 127u) * 160u + 0u]);
            wc.world_top = 0;
            wc.neighbours = &nb;
        }
        /* A neighbour of another row, listed first, does not hide the one that covers the pixel. */
        {
            OraclesEnhancedNeighbour rows[2] = { { 480 - 160, -128, left, 0, 0, 1, 0, 0u }, { 480 - 160, 0, left, 0, 0, 1, 0, 0u } };
            wc.neighbours = rows;
            wc.neighbour_count = 2;
            oracles_enhanced_compose(core, &wc, surface);
            CHECK(surface[(ORACLES_ENHANCED_HUD_HEIGHT + 30u) * NORMAL_W + 0u] == 0xffabcdefu);
            wc.neighbours = &nb;
            wc.neighbour_count = 1;
        }

        /* Determinism: same input, same surface. */
        uint32_t *again = malloc(NORMAL_PIXELS * sizeof *again);
        oracles_enhanced_compose(core, &wc, again);
        CHECK(memcmp(surface, again, NORMAL_PIXELS * sizeof *surface) == 0);

        /* A source without the fade holds RGB555 colours: the compose adds
         * the game's fade to each channel, clamped, and converts the colour
         * through the pipeline's table; the gutters and what no source
         * covers are black faded the same way, in both modes. */
        {
            uint32_t *table = malloc(ORACLES_PPU_COLOURS * sizeof *table);
            for (unsigned i = 0; i < ORACLES_PPU_COLOURS; i++) table[i] = 0xff000000u | i;   /* the colour itself, recognisable */
            uint32_t *raw = malloc(160u * 128u * sizeof *raw);
            for (unsigned i = 0; i < 160u * 128u; i++) raw[i] = (20u << 10) | (4u << 5) | 30u;   /* b 20, g 4, r 30 */
            OraclesEnhancedNeighbour rn = { 480 - 160, 0, raw, 0, 0, 0, 0, 0u };
            wc.neighbours = &rn;
            wc.colours = table;
            wc.fade = 0;
            oracles_enhanced_compose(core, &wc, surface);
            CHECK(surface[(ORACLES_ENHANCED_HUD_HEIGHT + 30u) * NORMAL_W + 0u] == (0xff000000u | (20u << 10) | (4u << 5) | 30u));
            CHECK(surface[0] == 0xff445566u);   /* no fade: the border */
            wc.fade = 5;
            oracles_enhanced_compose(core, &wc, surface);
            CHECK(surface[(ORACLES_ENHANCED_HUD_HEIGHT + 30u) * NORMAL_W + 0u] == (0xff000000u | (25u << 10) | (9u << 5) | 31u));
            CHECK(surface[0] == (0xff000000u | (5u << 10) | (5u << 5) | 5u));   /* the HUD gutter: black faded */
            CHECK(surface[(ORACLES_ENHANCED_HUD_HEIGHT + 30u) * NORMAL_W + NORMAL_W - 1u] == (0xff000000u | (5u << 10) | (5u << 5) | 5u));   /* uncovered: the same */
            wc.fade = -32;
            oracles_enhanced_compose(core, &wc, surface);
            CHECK(surface[(ORACLES_ENHANCED_HUD_HEIGHT + 30u) * NORMAL_W + 0u] == 0xff000000u);
            fc.fade = 31; fc.colours = table;
            oracles_enhanced_compose(core, &fc, surface);
            CHECK(surface[0] == (0xff000000u | (31u << 10) | (31u << 5) | 31u));   /* the frame's gutters go white with the flash */
            CHECK(surface[0 * NORMAL_W + NORMAL_HUD_X] == core[0]);
            fc.fade = 0; fc.colours = NULL;
            wc.fade = 0; wc.colours = NULL; wc.neighbours = &nb;
            free(table); free(raw);
        }

        free(core); free(surface); free(left); free(again);
    }

    /* ---- the wide render agrees with the normal one on the first 160 columns ---- */
    {
        uint8_t vram[0x4000], oam[160], bg[64], obj[64];
        memset(oam, 0, sizeof oam);
        for (unsigned i = 0; i < sizeof vram; i++) vram[i] = (uint8_t)(i * 7u + (i >> 5));
        for (unsigned i = 0; i < 64; i++) { bg[i] = (uint8_t)(i * 3u); obj[i] = (uint8_t)(i * 5u); }
        OraclesPpuInput in;
        in.vram = vram; in.oam = oam; in.bg_palettes = bg; in.obj_palettes = obj; in.colours = NULL;
        const OraclesPpuRegs regs = { 0x91, 3, 5, 200, 200 };
        for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) in.lines[ly] = regs;
        static uint32_t narrow[ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT], wide[240u * ORACLES_PPU_HEIGHT];
        oracles_ppu_render(&in, narrow);
        oracles_ppu_render_wide(&in, wide, 240u);
        int same = 1, beyond_varies = 0;
        for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT && same; ly++)
            if (memcmp(narrow + ly * ORACLES_PPU_WIDTH, wide + ly * 240u, ORACLES_PPU_WIDTH * sizeof *narrow) != 0) same = 0;
        for (unsigned i = 160; i < 240 && !beyond_varies; i++) if (wide[i] != wide[160]) beyond_varies = 1;
        CHECK(same);
        CHECK(beyond_varies);   /* the columns past 160 are a real render of the map, not a fill */
    }

    /* ---- the view on a synthetic core: composes, and its camera state round-trips ---- */
    {
        const size_t size = 1024u * 1024u;
        uint8_t *rom = calloc(size, 1);
        memcpy(rom + 0x134, "ZELDA NAYRU", 11);
        rom[0x143] = 0xc0; rom[0x147] = 0x1b; rom[0x148] = 0x05; rom[0x149] = 0x02;
        rom[0x100] = 0x00; rom[0x101] = 0xc3; rom[0x102] = 0x50; rom[0x103] = 0x01;
        rom[0x150] = 0x18; rom[0x151] = 0xfe;   /* jr -2 */
        const OraclesCoreOptions options = { 0, 0, ORACLES_CORE_SAMEBOY, 0 };
        OraclesCore *core = oracles_core_create(rom, size, &options);
        OraclesGuest *guest = core ? oracles_guest_attach(core, fixture_profile()) : NULL;
        OraclesEnhancedView *view = guest ? oracles_enhanced_view_start(core, guest, NULL, 0) : NULL;
        CHECK(view != NULL);
        if (view) {
            OraclesEnhancedMode mode = ORACLES_ENHANCED_WORLD;
            for (unsigned i = 0; i < 5; i++) { oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, NULL, NULL); }
            CHECK(mode == ORACLES_ENHANCED_FRAMED);   /* not playing: the framed core */
            CHECK(oracles_enhanced_view_width(view) == NORMAL_W && oracles_enhanced_view_height(view) == 144u);
            uint8_t wire[1024];
            size_t written = 0;
            CHECK(oracles_enhanced_view_save_state(view, wire, sizeof wire, &written) == 0 && written == 2u * ORACLES_E11_STATE_WIRE_SIZE + 12u + 12u);   /* both axes, the loops' record, the whole offsets' */
            CHECK(oracles_enhanced_view_load_state(view, wire, written) == 0);
            CHECK(oracles_enhanced_view_load_state(view, wire, written - 1) != 0);   /* a truncated state is refused */
            CHECK(oracles_enhanced_view_load_state(view, wire, written - 12u) == 0);   /* without the whole offsets: a state saved before them */
            CHECK(oracles_enhanced_view_load_state(view, wire, 2u * ORACLES_E11_STATE_WIRE_SIZE) == 0);   /* both axes without the loops: a state saved before them */
            CHECK(oracles_enhanced_view_load_state(view, wire, ORACLES_E11_STATE_WIRE_SIZE) == 0);   /* the horizontal record alone: a state saved before the vertical reducer */
            /* A state saved under one camera profile loads under the other: the savestate's profile wins. */
            CHECK(oracles_enhanced_camera_profile(oracles_enhanced_view_camera(view)) == 1);
            oracles_enhanced_view_set_camera_profile(view, 2);
            CHECK(oracles_enhanced_camera_profile(oracles_enhanced_view_camera(view)) == 2);
            CHECK(oracles_enhanced_view_load_state(view, wire, written) == 0);
            CHECK(oracles_enhanced_camera_profile(oracles_enhanced_view_camera(view)) == 1);
            oracles_enhanced_view_set_camera_profile(view, 2);
            CHECK(oracles_enhanced_view_save_state(view, wire, sizeof wire, &written) == 0);
            oracles_enhanced_view_set_camera_profile(view, 1);
            CHECK(oracles_enhanced_view_load_state(view, wire, written) == 0);
            CHECK(oracles_enhanced_camera_profile(oracles_enhanced_view_camera(view)) == 2);
            /* A state of the second profile before it settled (v4) resumes under it, renamed. */
            oracles_enhanced_view_set_camera_profile(view, 3);
            CHECK(oracles_enhanced_view_save_state(view, wire, sizeof wire, &written) == 0);
            oracles_enhanced_view_set_camera_profile(view, 2);
            CHECK(oracles_enhanced_view_load_state(view, wire, written) == 0);
            CHECK(oracles_enhanced_camera_profile(oracles_enhanced_view_camera(view)) == 2);
            CHECK(strcmp(oracles_enhanced_camera_state(oracles_enhanced_view_camera(view))->config_digest, ORACLES_E11_CONFIG_DIGEST_V5) == 0);
            CHECK(strcmp(oracles_enhanced_camera_state_vertical(oracles_enhanced_view_camera(view))->config_digest, ORACLES_E11_CONFIG_DIGEST_V5) == 0);
            /* And under the first profile, as it did when the second was that one. */
            oracles_enhanced_view_set_camera_profile(view, 3);
            CHECK(oracles_enhanced_view_save_state(view, wire, sizeof wire, &written) == 0);
            oracles_enhanced_view_set_camera_profile(view, 1);
            CHECK(oracles_enhanced_view_load_state(view, wire, written) == 0);
            CHECK(oracles_enhanced_camera_profile(oracles_enhanced_view_camera(view)) == 2);
            CHECK(strcmp(oracles_enhanced_camera_state(oracles_enhanced_view_camera(view))->config_digest, ORACLES_E11_CONFIG_DIGEST_V5) == 0);
            oracles_enhanced_view_toggle(view);
            CHECK(oracles_enhanced_view_framed_only(view) == 1);
            oracles_enhanced_view_toggle(view);

            /* A state of play written into the guest's WRAM through the tables:
             * the view enters world mode and the camera tracks Link. */
            const OraclesGuestTables *t = &oracles_guest_tables_ages;
            uint8_t *w0 = oracles_guest_wram_writable(guest, 0);
            uint8_t *w1 = oracles_guest_wram_writable(guest, 1);
            uint8_t *link = w1 + (ORACLES_OBJECTS_BASE - 0xd000u);
            #define W0(sym) w0[(sym).addr - 0xc000u]
            W0(t->game_state) = 2; W0(t->cutscene_index) = 1; W0(t->cutscene_trigger) = 0;
            W0(t->scroll_mode) = 1; W0(t->screen_transition_state) = 2; W0(t->text_is_active) = 0;
            W0(t->disable_screen_transitions) = 0; W0(t->link_force_state) = 0;
            W0(t->active_group) = 0; W0(t->active_room) = 0x23; W0(t->room_is_large) = 0;
            link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_STATE] = 1; link[ORACLES_OBJ_XH] = 80; link[ORACLES_OBJ_YH] = 64;
            int32_t cam = 0, cam_before = 0, cam_y = 0;
            for (unsigned i = 0; i < 40; i++) { oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y); }
            CHECK(mode == ORACLES_ENHANCED_WORLD);
            /* A warp: the room changes outside a scrolling transition, a new epoch. */
            W0(t->scroll_mode) = 2; oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
            W0(t->active_room) = 0x45; oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
            W0(t->scroll_mode) = 1;
            for (unsigned i = 0; i < 40; i++) { oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y); }
            CHECK(mode == ORACLES_ENHANCED_WORLD);
            const uint64_t epoch_after_warp = oracles_enhanced_view_observation(view)->epoch;
            CHECK(epoch_after_warp > 8u);   /* at least one teleport since the start */
            /* Save, walk, load: the camera is back at the saved point... */
            CHECK(oracles_enhanced_view_save_state(view, wire, sizeof wire, &written) == 0);
            const int32_t saved_cam = cam;
            for (unsigned i = 0; i < 60; i++) { link[ORACLES_OBJ_XH] = (uint8_t)(80 + i); oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y); }
            CHECK(cam != saved_cam);
            link[ORACLES_OBJ_XH] = 80;
            CHECK(oracles_enhanced_view_load_state(view, wire, written) == 0);
            oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
            CHECK(mode == ORACLES_ENHANCED_WORLD);
            CHECK(cam == saved_cam);
            CHECK(oracles_enhanced_view_observation(view)->epoch == epoch_after_warp);
            /* ... and it still follows Link afterwards (the reducer's epoch and the observer's agree). */
            cam_before = cam;
            for (unsigned i = 0; i < 60; i++) { link[ORACLES_OBJ_XH] = (uint8_t)(80 + i); oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y); }
            CHECK(mode == ORACLES_ENHANCED_WORLD);
            CHECK(cam > cam_before);
            /* The vertical camera: on the map it keeps Link in the world band
             * and follows him down; off the grid (a large room) it is the game's window. */
            {
                const int32_t link_world_y = 4 * 128 + 64;   /* room 0x45: row 4 */
                for (unsigned i = 0; i < 60; i++) { oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y); }
                CHECK(oracles_enhanced_camera_vertical_tracking(oracles_enhanced_view_camera(view)));
                CHECK(cam_y <= link_world_y && link_world_y < cam_y + (int32_t)NORMAL_BAND_H);
                const int32_t cam_y_before = cam_y;
                for (unsigned i = 0; i < 60; i++) { link[ORACLES_OBJ_YH] = (uint8_t)(64 + i); oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y); }
                CHECK(cam_y > cam_y_before);
                CHECK(cam_y <= link_world_y + 59 && link_world_y + 59 < cam_y + (int32_t)NORMAL_BAND_H);
                W0(t->scroll_mode) = 2; oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                /* A dungeon room (group 4: large by the group): the game's own framing, the room extended,
                 * the band's lines the window's, the room centred with its 8 px gutters. */
                W0(t->active_group) = 4; W0(t->active_room) = 0x10; W0(t->dungeon_index) = 0xff; link[ORACLES_OBJ_YH] = 64; link[ORACLES_OBJ_XH] = 80;
                oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                W0(t->scroll_mode) = 1;
                for (unsigned i = 0; i < 40; i++) { oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y); }
                CHECK(mode == ORACLES_ENHANCED_WORLD);
                CHECK(oracles_enhanced_view_observation(view)->large_grid);
                CHECK(cam_y == oracles_enhanced_view_observation(view)->window_top);
                CHECK(cam == oracles_enhanced_view_observation(view)->world.origin_x - 8);
                W0(t->scroll_mode) = 2; oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                W0(t->active_group) = 0; W0(t->active_room) = 0x45;
                oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                W0(t->scroll_mode) = 1;
                for (unsigned i = 0; i < 40; i++) { oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y); }
                CHECK(mode == ORACLES_ENHANCED_WORLD);
            }
            /* A warp into another domain (an interior): the reducers restart
             * there instead of staying desynchronised, and the view keeps the
             * world; back on the overworld, the same. */
            {
                W0(t->scroll_mode) = 2; oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                W0(t->active_group) = 2; W0(t->active_room) = 0x10; W0(t->room_is_large) = 0; link[ORACLES_OBJ_YH] = 64; link[ORACLES_OBJ_XH] = 80;
                oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                W0(t->scroll_mode) = 1;
                for (unsigned i = 0; i < 40; i++) { oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y); }
                CHECK(mode == ORACLES_ENHANCED_WORLD);
                CHECK(oracles_enhanced_view_observation(view)->world.domain == ORACLES_E11_INTERIOR);
                W0(t->scroll_mode) = 2; oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                W0(t->active_group) = 0; W0(t->active_room) = 0x45;
                oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                W0(t->scroll_mode) = 1;
                for (unsigned i = 0; i < 40; i++) { oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y); }
                CHECK(mode == ORACLES_ENHANCED_WORLD);
                CHECK(oracles_enhanced_camera_vertical_tracking(oracles_enhanced_view_camera(view)));
                /* Parked at the game's placeholder: his last position in this room holds;
                 * parked in a room he has no position in (a cutscene's entry), its centre. */
                {
                    const int32_t before_x = oracles_enhanced_view_observation(view)->world.link_x;
                    link[ORACLES_OBJ_XH] = 0xf8; link[ORACLES_OBJ_YH] = 0xf8;
                    oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                    CHECK(oracles_enhanced_view_observation(view)->world.link_x == before_x);
                    W0(t->scroll_mode) = 2; oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                    W0(t->active_room) = 0x46; oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                    W0(t->scroll_mode) = 1; oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                    CHECK(oracles_enhanced_view_observation(view)->world.link_x == 6 * 160 + 80 && oracles_enhanced_view_observation(view)->world.link_y == 4 * 128 + 64);
                    link[ORACLES_OBJ_XH] = 80; link[ORACLES_OBJ_YH] = 64;
                    oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                }
                /* An open menu: the framed core. */
                W0(t->opened_menu_type) = 1;
                oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                CHECK(mode == ORACLES_ENHANCED_FRAMED);
                W0(t->opened_menu_type) = 0;
                oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                CHECK(mode == ORACLES_ENHANCED_WORLD);
                /* Link put elsewhere while the core is framed (a menu, a cutscene's picture): the first frame of the
                 * world shows the camera where it settles, not on its way there. */
                {
                    for (unsigned i = 0; i < 60; i++) { oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y); }
                    const int32_t before = cam;
                    W0(t->opened_menu_type) = 1; link[ORACLES_OBJ_XH] = 140;
                    oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                    CHECK(mode == ORACLES_ENHANCED_FRAMED);
                    W0(t->opened_menu_type) = 0;
                    oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                    CHECK(mode == ORACLES_ENHANCED_WORLD);
                    const int32_t first = cam;
                    for (unsigned i = 0; i < 60; i++) { oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y); }
                    CHECK(cam - before == 60 && first == cam);
                    /* His object made anew by a cutscene, at its first state in the room's corner: his place holds. */
                    const int32_t held_x = oracles_enhanced_view_observation(view)->world.link_x;
                    link[ORACLES_OBJ_STATE] = 0; link[ORACLES_OBJ_XH] = 0; link[ORACLES_OBJ_YH] = 0;
                    oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                    CHECK(oracles_enhanced_view_observation(view)->world.link_x == held_x && cam == first);
                    link[ORACLES_OBJ_STATE] = 1; link[ORACLES_OBJ_XH] = 80; link[ORACLES_OBJ_YH] = 64;
                }
                /* The same under a fade at white, the background and the sprites washed out; with a sprite's colour
                 * still lit over it (the time portal's black), the frame is seen and the camera pans as ever. */
                {
                    uint8_t *bg = (uint8_t *)(uintptr_t)oracles_guest_bg_palettes(guest), *obj = (uint8_t *)(uintptr_t)oracles_guest_obj_palettes(guest);
                    uint8_t *base = oracles_guest_wram_writable(guest, 2) + (t->tileset_bg_palettes.addr - 0xd000u);
                    CHECK(bg != NULL && obj != NULL);
                    if (bg && obj) {
                        uint8_t bg_kept[64], obj_kept[64];
                        memcpy(bg_kept, bg, sizeof bg_kept); memcpy(obj_kept, obj, sizeof obj_kept);
                        for (unsigned i = 0; i < 64u; i += 2u) { base[i] = 0x08; base[i + 1u] = 0x21; }
                        for (unsigned lit = 0; lit < 2u; lit++) {
                            for (unsigned i = 0; i < 60; i++) { oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y); }
                            const int32_t before = cam;
                            for (unsigned i = 0; i < 64u; i += 2u) { bg[i] = 0xff; bg[i + 1u] = 0x7f; obj[i] = 0xff; obj[i + 1u] = 0x7f; }
                            if (lit) { obj[2] = 0x1f; obj[3] = 0x00; }
                            link[ORACLES_OBJ_XH] = lit ? 80 : 140;
                            oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                            CHECK(mode == ORACLES_ENHANCED_WORLD && oracles_enhanced_view_washed_out(view) == !lit);
                            memcpy(bg, base, 64u); memcpy(obj, obj_kept, sizeof obj_kept);
                            oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                            const int32_t first = cam;
                            for (unsigned i = 0; i < 60; i++) { oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y); }
                            CHECK(cam - before == (lit ? -60 : 60));
                            CHECK(lit ? first != cam : first == cam);
                        }
                        memcpy(bg, bg_kept, sizeof bg_kept);
                    }
                }
                /* The follow, Link walking, keeps its speed and its look-ahead through a menu: no step back. */
                {
                    for (unsigned i = 0; i < 60; i++) { oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y); }
                    int32_t last = cam, step = 0;
                    for (unsigned i = 0; i < 40; i++) {
                        link[ORACLES_OBJ_XH] = (uint8_t)(80 + i);
                        W0(t->opened_menu_type) = i == 30u;
                        oracles_core_run_frame(core); oracles_enhanced_view_compose(view, &mode, &cam, &cam_y);
                        if (i == 29u) step = cam - last;
                        if (i == 31u) CHECK(step > 0 && cam - last >= 2 * step - 1 && cam - last <= 2 * step + 1);
                        if (i != 30u) last = cam;
                    }
                    link[ORACLES_OBJ_XH] = 80;
                }
            }
            #undef W0
            oracles_enhanced_view_stop(view);
        }
        if (guest) oracles_guest_detach(guest);
        if (core) oracles_core_destroy(core);
        free(rom);
    }

    /* ---- compositor: the game's per-line ripple, with the camera off the window ----
     * Every source holds the same world function, so the band must hold it too:
     * the line's shift is the one of the LCD line that stands on that band row,
     * not of the band row itself, or the neighbours ripple out of step with the
     * window (the two differ by the vertical camera). */
    {
        #define WORLD_PIXEL(wx, wy) (0xff000000u | ((uint32_t)((wx) & 0xff) << 8) | (uint32_t)((wy) & 0xff))
        int16_t shift_x[NORMAL_BAND_H], shift_y[NORMAL_BAND_H];
        for (unsigned r = 0; r < NORMAL_BAND_H; r++) {
            shift_x[r] = (int16_t)((r % 8u) < 4u ? 2 : -2);
            shift_y[r] = (int16_t)((r % 6u) < 3u ? 1 : -1);
        }
        const int32_t window_left = 480, window_top = 256;
        const int32_t world_left = window_left - 40, world_top = window_top + 24;   /* the camera off the window on both axes */
        uint32_t *img = malloc(160u * 144u * sizeof *img);
        for (unsigned r = 0; r < NORMAL_BAND_H; r++)
            for (unsigned c = 0; c < 160u; c++)
                img[(ORACLES_ENHANCED_HUD_HEIGHT + r) * 160u + c] =
                    WORLD_PIXEL(window_left + (int32_t)c + shift_x[r], window_top + (int32_t)r + shift_y[r]);
        for (unsigned y = 0; y < ORACLES_ENHANCED_HUD_HEIGHT; y++)
            for (unsigned c = 0; c < 160u; c++) img[y * 160u + c] = 0xff000000u;
        /* One neighbour wide enough to cover the whole band, the same world function. */
        const int32_t nb_left = window_left - 480, nb_top = window_top - 256;
        const unsigned nb_w = 1280u, nb_h = 768u;
        uint32_t *area = malloc((size_t)nb_w * nb_h * sizeof *area);
        for (unsigned ny = 0; ny < nb_h; ny++)
            for (unsigned nx = 0; nx < nb_w; nx++)
                area[(size_t)ny * nb_w + nx] = WORLD_PIXEL(nb_left + (int32_t)nx, nb_top + (int32_t)ny);
        OraclesEnhancedNeighbour around = { nb_left, nb_top, area, nb_w, nb_h, 1, 0, 0u };
        OraclesEnhancedCompose rc;
        memset(&rc, 0, sizeof rc);
        rc.mode = ORACLES_ENHANCED_WORLD;
        rc.window_world_left = window_left;
        rc.window_world_top = window_top;
        rc.world_left = world_left;
        rc.world_top = world_top;
        rc.line_shift = shift_x;
        rc.line_shift_y = shift_y;
        rc.neighbours = &around;
        rc.neighbour_count = 1;
        uint32_t *band = malloc(NORMAL_PIXELS * sizeof *band);
        CHECK(oracles_enhanced_compose(img, &rc, band) == 0);   /* the neighbour covers everything */
        unsigned wrong = 0;
        for (unsigned y = 0; y < NORMAL_BAND_H; y++) {
            const int32_t line = (world_top + (int32_t)y - window_top) % (int32_t)NORMAL_BAND_H;   /* below the window: the lines of a taller screen, the wave's period 128 */
            for (unsigned x = 0; x < NORMAL_W; x++) {
                const uint32_t expect = WORLD_PIXEL(world_left + (int32_t)x + shift_x[line], world_top + (int32_t)y + shift_y[line]);
                if (band[(ORACLES_ENHANCED_HUD_HEIGHT + y) * NORMAL_W + x] != expect) wrong++;
            }
        }
        CHECK(wrong == 0);
        free(band); free(area); free(img);
        #undef WORLD_PIXEL
    }

    unseen_ends();
    settle_at_rest();
    rest_whole_and_fade();
    band_fraction_still();

    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_camera: ok\n");
    return 0;
}
