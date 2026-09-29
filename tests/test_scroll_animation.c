/* A scroll's tile animation run by the view alone, without a ROM: the reader
 * runs each stream whole loops from the step the game froze, spread over the
 * scroll, one at least, and lands on that very step at the frame the scroll's
 * counter runs out; a stream outside its loop stays still; a plan made again
 * keeps the loops started.  The view's clock (view_scroll.c), fed a scroll
 * as the game counts it, lands at the frame its plan spread the loops over;
 * the band draws the view's images but on the tiles the game writes.
 * The data is built here in the game's format (animationData.s,
 * animationGfxHeaders.s), in a buffer standing for the ROM. */
#include "animation.h"
#include "view_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

#define ROM_SIZE 0x10000u
#define DATA_BANK 1u
#define STREAM_A 0x4100u     /* four images of 15 frames: a loop of 60, the water's */
#define STREAM_B 0x4110u     /* four images of 4 frames: a loop of 16 */
#define STREAM_C 0x4120u     /* an image of 5 frames, then a loop of two images of 4 */
#define HEADERS 0x4200u
#define IMAGES_BANK 2u

static uint8_t rom[ROM_SIZE];
static OraclesGuestTables tables;

static size_t at(unsigned bank, unsigned addr) { return (size_t)bank * 0x4000u + (addr - 0x4000u); }

/* (delay, index) pairs, then the loop marker back to the first. */
static void stream(unsigned addr, const uint8_t pairs[][2], unsigned count)
{
    uint8_t *p = rom + at(DATA_BANK, addr);
    for (unsigned n = 0; n < count; n++) { *p++ = pairs[n][0]; *p++ = pairs[n][1]; }
    const unsigned marker = addr + 2u * count;
    p[0] = 0xffu;
    p[1] = (uint8_t)((addr - (marker + 1u) - 0xff00u) & 0xffu);   /* hl = marker + 1 + $ff00 + c = addr */
}

static void build(void)
{
    memset(rom, 0, sizeof rom);
    memset(&tables, 0, sizeof tables);
    tables.animation_group_table.bank = DATA_BANK;
    tables.animation_group_table.addr = 0x4000u;
    tables.animation_gfx_headers.bank = DATA_BANK;
    tables.animation_gfx_headers.addr = HEADERS;
    static const uint8_t a[4][2] = { { 15, 0 }, { 15, 1 }, { 15, 2 }, { 15, 3 } }, b[4][2] = { { 4, 4 }, { 4, 5 }, { 4, 6 }, { 4, 7 } };
    stream(STREAM_A, a, 4);
    stream(STREAM_B, b, 4);
    /* C: its first entry is outside the loop, which starts at the next one. */
    uint8_t *c = rom + at(DATA_BANK, STREAM_C);
    c[0] = 5; c[1] = 8;
    static const uint8_t loop[2][2] = { { 4, 4 }, { 4, 5 } };
    stream(STREAM_C + 2u, loop, 2);
    /* Index i: 16 bytes of value 0x10 + i, into tile 0 (stream A) or tile 1 (stream B, and index 8). */
    for (unsigned i = 0; i < 9u; i++) {
        uint8_t *h = rom + at(DATA_BANK, HEADERS) + i * 6u;
        const unsigned source = 0x4000u + i * 16u, destination = 0x8000u + (i < 4u ? 0u : 16u);
        h[0] = IMAGES_BANK; h[1] = (uint8_t)(source >> 8); h[2] = (uint8_t)source; h[3] = (uint8_t)(destination >> 8); h[4] = (uint8_t)destination; h[5] = 0;
        memset(rom + at(IMAGES_BANK, source), 0x10 + (int)i, 16u);
    }
}

/* Stream A 7 frames before it loads index 1 (index 0 on screen), stream B 3 before index 6 (5 on screen). */
static OraclesAnimationState frozen_state(void)
{
    OraclesAnimationState s;
    memset(&s, 0, sizeof s);
    s.tileset_animation = 0;
    s.state = 0x03u;
    const unsigned pa = STREAM_A + 3u, pb = STREAM_B + 5u;
    s.counters[0] = 7; s.counters[1] = (uint8_t)pa; s.counters[2] = (uint8_t)(pa >> 8);
    s.counters[3] = 3; s.counters[4] = (uint8_t)pb; s.counters[5] = (uint8_t)(pb >> 8);
    return s;
}

/* A whole scroll of `frames` frames: the tiles written, and whether the view
 * stood apart from the frozen step midway. */
static int run_scroll(OraclesScrollAnimation *s, unsigned frames, uint8_t *tiles)
{
    int moved = 0;
    for (unsigned f = 0; f < frames; f++) {
        CHECK(oracles_scroll_animation_step(s, frames - 1u - f, &tables, rom, ROM_SIZE, tiles, NULL) >= 0);
        if (f + 1u < frames && oracles_animation_gap(&s->now, &s->frozen, &tables, rom, ROM_SIZE) != 0) moved = 1;
    }
    return moved;
}

static void periods(void)
{
    const OraclesAnimationState s = frozen_state();
    CHECK(oracles_animation_stream_period(&s, 0, &tables, rom, ROM_SIZE) == 60u);
    CHECK(oracles_animation_stream_period(&s, 1, &tables, rom, ROM_SIZE) == 16u);
    CHECK(oracles_animation_stream_period(&s, 2, &tables, rom, ROM_SIZE) == 0u);   /* not in use */
}

/* Every stream runs, and lands on the frozen step with its image. */
static void lands(void)
{
    const OraclesAnimationState frozen = frozen_state();
    OraclesScrollAnimation s;
    CHECK(oracles_scroll_animation_begin(&s, &frozen, 30, &tables, rom, ROM_SIZE) == 2u);
    CHECK(s.target[0] == 60u && s.target[1] == 32u);   /* one loop of 60 at twice the pace, two of 16 */
    uint8_t tiles[2u * ORACLES_ANIMATION_TILE_BYTES];
    memset(tiles, 0, sizeof tiles);
    CHECK(run_scroll(&s, 30, tiles) == 1);
    CHECK(memcmp(s.now.counters, frozen.counters, sizeof frozen.counters) == 0);
    CHECK(oracles_animation_gap(&s.now, &frozen, &tables, rom, ROM_SIZE) == 0u);
    CHECK(s.ran[0] == 30u && s.ran[1] == 30u);
    /* Held on the step after the landing (the counter at zero a few frames): not moving, not counted in its pace. */
    for (unsigned f = 0; f < 3u; f++) CHECK(oracles_scroll_animation_step(&s, 0, &tables, rom, ROM_SIZE, tiles, NULL) == 0);
    CHECK(s.ran[0] == 30u && s.ran[1] == 30u && oracles_animation_gap(&s.now, &frozen, &tables, rom, ROM_SIZE) == 0u);
    /* The image last loaded by each loop is the one the game shows at the frozen step. */
    CHECK(tiles[0] == 0x10 && tiles[15] == 0x10);
    CHECK(tiles[16] == 0x15 && tiles[31] == 0x15);
}

/* Round numbers of loops, one at least; a stream outside its loop stays still. */
static void loops_counted(void)
{
    const OraclesAnimationState frozen = frozen_state();
    OraclesScrollAnimation s;
    CHECK(oracles_scroll_animation_begin(&s, &frozen, 45, &tables, rom, ROM_SIZE) == 2u);
    CHECK(s.target[0] == 60u && s.target[1] == 48u);   /* 0.75 and 2.8 loops, rounded */
    CHECK(oracles_scroll_animation_begin(&s, &frozen, 10, &tables, rom, ROM_SIZE) == 2u);
    CHECK(s.target[0] == 60u && s.target[1] == 16u);   /* six times the pace, and one at least */
    OraclesAnimationState prefix = frozen;
    const unsigned pc = STREAM_C + 1u;
    prefix.state = 0x07u;
    prefix.counters[6] = 3; prefix.counters[7] = (uint8_t)pc; prefix.counters[8] = (uint8_t)(pc >> 8);
    CHECK(oracles_animation_stream_period(&prefix, 2, &tables, rom, ROM_SIZE) == 0u);
    CHECK(oracles_scroll_animation_begin(&s, &prefix, 30, &tables, rom, ROM_SIZE) == 2u && s.target[2] == 0u);
    uint8_t tiles[2u * ORACLES_ANIMATION_TILE_BYTES];
    memset(tiles, 0, sizeof tiles);
    run_scroll(&s, 30, tiles);
    CHECK(oracles_animation_gap(&s.now, &prefix, &tables, rom, ROM_SIZE) == 0u);
}

/* The plan made again at the scroll's first step: the loops a stream has
 * started are finished, however short the scroll turns out. */
static void plan_again(void)
{
    const OraclesAnimationState frozen = frozen_state();
    OraclesScrollAnimation s;
    CHECK(oracles_scroll_animation_begin(&s, &frozen, 50, &tables, rom, ROM_SIZE) == 2u);
    CHECK(s.target[0] == 60u && s.target[1] == 48u);
    for (unsigned f = 0; f < 30u; f++) CHECK(oracles_scroll_animation_step(&s, 49u - f, &tables, rom, ROM_SIZE, NULL, NULL) >= 0);
    /* 29 frames of the stream of 16 run, two loops started: shorter, it still finishes them. */
    oracles_scroll_animation_plan(&s, 20);
    CHECK(s.target[0] == 60u && s.target[1] == 32u);
    for (unsigned f = 0; f < 4u; f++) CHECK(oracles_scroll_animation_step(&s, 3u - f, &tables, rom, ROM_SIZE, NULL, NULL) >= 0);
    CHECK(oracles_animation_gap(&s.now, &frozen, &tables, rom, ROM_SIZE) == 0u);
    /* Longer: more loops. */
    CHECK(oracles_scroll_animation_begin(&s, &frozen, 20, &tables, rom, ROM_SIZE) == 2u && s.target[1] == 16u);
    for (unsigned f = 0; f < 5u; f++) CHECK(oracles_scroll_animation_step(&s, 19u - f, &tables, rom, ROM_SIZE, NULL, NULL) >= 0);
    oracles_scroll_animation_plan(&s, 40);
    CHECK(s.target[1] == 48u);
    for (unsigned f = 5; f < 40u; f++) CHECK(oracles_scroll_animation_step(&s, 39u - f, &tables, rom, ROM_SIZE, NULL, NULL) >= 0);
    CHECK(oracles_animation_gap(&s.now, &frozen, &tables, rom, ROM_SIZE) == 0u);
    /* A loop entered by a fraction of a frame counts as started: 17 frames of
     * 50 over three loops of 16 leave the stream 16.32 frames on, so a
     * shorter scroll still gives it two loops, never a target under what it
     * has run. */
    CHECK(oracles_scroll_animation_begin(&s, &frozen, 50, &tables, rom, ROM_SIZE) == 2u && s.target[1] == 48u);
    for (unsigned f = 0; f < 17u; f++) CHECK(oracles_scroll_animation_step(&s, 49u - f, &tables, rom, ROM_SIZE, NULL, NULL) >= 0);
    CHECK(s.phase[1] > (16u << 16) && s.phase[1] < (17u << 16));
    oracles_scroll_animation_plan(&s, 10);
    CHECK(s.target[1] == 32u && ((uint64_t)s.target[1] << 16) >= s.phase[1]);
    if (((uint64_t)s.target[1] << 16) >= s.phase[1] && ((uint64_t)s.target[0] << 16) >= s.phase[0]) {
        for (unsigned f = 0; f < 10u; f++) CHECK(oracles_scroll_animation_step(&s, 9u - f, &tables, rom, ROM_SIZE, NULL, NULL) >= 0);
        CHECK(memcmp(s.now.counters, frozen.counters, sizeof frozen.counters) == 0);
    }
}

/* The gap, ahead or behind, in frames. */
static void gaps(void)
{
    const OraclesAnimationState frozen = frozen_state();
    OraclesScrollAnimation s;
    oracles_scroll_animation_begin(&s, &frozen, 30, &tables, rom, ROM_SIZE);
    for (unsigned f = 0; f < 3u; f++) oracles_scroll_animation_step(&s, 100, &tables, rom, ROM_SIZE, NULL, NULL);
    const unsigned ahead = oracles_animation_gap(&s.now, &frozen, &tables, rom, ROM_SIZE);
    CHECK(ahead >= 1u && ahead <= 16u);
    /* Three frames of the game's own stepping: the view three frames ahead,
     * then three behind (the other way round would be 57 and 13 frames). */
    OraclesAnimationState later = frozen;
    uint8_t scratch[2u * ORACLES_ANIMATION_TILE_BYTES];
    for (unsigned f = 0; f < 3u; f++) CHECK(oracles_animation_step(&later, &tables, rom, ROM_SIZE, scratch, NULL) >= 0);
    CHECK(oracles_animation_gap(&later, &frozen, &tables, rom, ROM_SIZE) == 3u);
    CHECK(oracles_animation_gap(&frozen, &later, &tables, rom, ROM_SIZE) == 3u);
    OraclesAnimationState other = frozen;
    other.tileset_animation = 1;
    CHECK(oracles_animation_gap(&frozen, &other, &tables, rom, ROM_SIZE) == ORACLES_ANIMATION_UNREACHED);
    /* A queued copy's tiles. */
    OraclesAnimationState queued = frozen;
    queued.head = 3; queued.tail = 4; queued.queue[4] = 5;
    uint8_t mask[2u * 384u / 8u];
    memset(mask, 0, sizeof mask);
    CHECK(oracles_animation_queued_tiles(&queued, &tables, rom, ROM_SIZE, mask) == 1u && mask[0] == 0x02u);
}

/* A scroll as the game counts it (docs/GAME_HOOKS.md, section 5): `load` frames of
 * load, then wScreenScrollCounter from `steps`, a step a frame with the step
 * doubled (its first value held two frames) or every two frames with the
 * game's own, then held at zero a few frames; the doubled step read as the
 * game's own at the first `late` frames of the scroll (a screen shaken at
 * the freeze: the continuous transitions double it once the hardware's
 * registers, a frame behind the game's, are aligned).  Each frame the clock
 * is fed and the reader stepped by it.  Returns the frames of stream time
 * the landing frame took; `during_load` the frames the stream ran in the
 * load. */
static unsigned game_scroll(EvScrollClock *c, OraclesScrollAnimation *s, unsigned load, unsigned steps, int doubled, unsigned late, unsigned stream, unsigned *during_load)
{
    *during_load = 0;
    const unsigned scroll = doubled ? steps + 2u : 2u * steps + 1u;
    unsigned landing_step = 0;
    for (unsigned f = 0; f < load + scroll + 3u; f++) {
        const int scrolling = f >= load;
        const unsigned k = f - load;
        unsigned counter = 0;   /* what the last scroll left, during the load */
        if (scrolling) counter = doubled ? (k < 2u ? steps : k - 1u < steps ? steps - (k - 1u) : 0u) : (k / 2u < steps ? steps - k / 2u : 0u);
        int replan = 0;
        unsigned plan = 0;
        const unsigned remaining = ev_scroll_clock_frame(c, scrolling, counter, doubled && k >= late ? 0xf8u : 0xfcu, &replan, &plan);
        if (replan) oracles_scroll_animation_plan(s, plan);
        const uint32_t before = s->phase[stream];
        CHECK(oracles_scroll_animation_step(s, remaining, &tables, rom, ROM_SIZE, NULL, NULL) >= 0);
        if (!remaining && c->landed_at == c->elapsed) landing_step = (s->phase[stream] >> 16) - (before >> 16);
        if (f + 1u == load) *during_load = s->phase[stream] >> 16;
    }
    return landing_step;
}

/* The clock lands at the frame the plan spreads the loops over, with the
 * continuous transitions' step and with the game's, whatever the load; the
 * landing is no larger a step than any other.  With the continuous
 * transitions (Link walks through the load) the streams run from the freeze,
 * the scroll's step doubled or, the transition refused, not; without them
 * they stand still through the load, as the game's do, and run over the
 * scroll's own frames.  A step doubled a frame after the first, the counter
 * still whole (Subrosia's shaken screens), is the doubled scroll all the
 * same. */
static void clock_lands(void)
{
    CHECK(ev_scroll_expected_frames(5, 16, 1) == 23u && ev_scroll_expected_frames(5, 16, 2) == 38u && ev_scroll_expected_frames(9, 20, 2) == 50u);
    static const struct { unsigned load; int doubled, horizontal, continuous; unsigned late; } cases[] = {
        { 5, 1, 0, 1, 0 }, { 9, 1, 1, 1, 0 }, { 9, 0, 0, 1, 0 }, { 5, 0, 0, 0, 0 }, { 9, 0, 0, 0, 0 }, { 14, 0, 1, 0, 0 },
        { 9, 1, 0, 1, 1 }, { 11, 1, 1, 1, 1 } };
    for (unsigned n = 0; n < sizeof cases / sizeof cases[0]; n++) {
        const OraclesAnimationState frozen = frozen_state();
        EvScrollClock c;
        memset(&c, 0, sizeof c);
        OraclesScrollAnimation s;
        const unsigned steps = cases[n].horizontal ? 20u : 16u;
        const unsigned first = ev_scroll_clock_begin(&c, cases[n].horizontal, cases[n].continuous);
        CHECK(c.expected == 5u + steps + 2u && first == (cases[n].continuous ? c.expected : 0u));   /* the shortest: the shortest load, the doubled step */
        oracles_scroll_animation_begin(&s, &frozen, first, &tables, rom, ROM_SIZE);
        unsigned during_load;
        const unsigned landing_step = game_scroll(&c, &s, cases[n].load, steps, cases[n].doubled, cases[n].late, 1, &during_load);
        CHECK(c.expected == ev_scroll_expected_frames(cases[n].load, steps, cases[n].doubled ? 1u : 2u));
        CHECK(c.landed_at == c.expected);
        CHECK(memcmp(s.now.counters, frozen.counters, sizeof frozen.counters) == 0);
        /* The stream of 16 frames: its loops spread evenly over the frames it ran, the last step no larger than the others. */
        const unsigned ran = cases[n].continuous ? c.expected : c.expected - cases[n].load;
        CHECK(s.ran[1] == ran && landing_step <= (s.target[1] + ran - 1u) / ran);
        CHECK(cases[n].continuous ? during_load > 0u : during_load == 0u);
        CHECK(s.target[1] == (ran + 8u) / 16u * 16u);   /* round(frames / 16) loops over the frames it runs */
    }
    /* The shortest load is learnt: a load of 3 frames makes the next plan 3 + 16 + 2. */
    EvScrollClock c;
    memset(&c, 0, sizeof c);
    OraclesScrollAnimation s;
    const OraclesAnimationState frozen = frozen_state();
    unsigned during_load;
    oracles_scroll_animation_begin(&s, &frozen, ev_scroll_clock_begin(&c, 0, 1), &tables, rom, ROM_SIZE);
    game_scroll(&c, &s, 3, 16, 1, 0, 1, &during_load);
    CHECK(c.load_frames == 3u && ev_scroll_clock_begin(&c, 0, 1) == 21u);
    /* Down to none: a load of no frame is measured as such, not taken for none measured. */
    game_scroll(&c, &s, 0, 16, 1, 0, 1, &during_load);
    CHECK(c.load_frames == 0u && ev_scroll_clock_begin(&c, 0, 1) == 18u);
    /* A counter the plan did not foresee (held a frame longer): the landing is off the plan's frame, which the view counts. */
    memset(&c, 0, sizeof c);
    ev_scroll_clock_begin(&c, 0, 1);
    int replan = 0;
    unsigned plan = 0;
    for (unsigned f = 0; f < 5u; f++) ev_scroll_clock_frame(&c, 0, 0, 0xf8u, &replan, &plan);
    for (unsigned k = 0; k < 19u; k++) ev_scroll_clock_frame(&c, 1, k < 3u ? 16u : k - 2u < 16u ? 16u - (k - 2u) : 0u, 0xf8u, &replan, &plan);
    CHECK(c.landed_at == c.expected + 1u);
    /* The step doubled at the first step, then given back to the game's own
     * the next frame (a refusal), the counter still whole: the scroll is the
     * game's own, and lands on that plan. */
    memset(&c, 0, sizeof c);
    ev_scroll_clock_begin(&c, 0, 1);
    for (unsigned f = 0; f < 5u; f++) ev_scroll_clock_frame(&c, 0, 0, 0xf8u, &replan, &plan);
    for (unsigned k = 0; k < 36u; k++) ev_scroll_clock_frame(&c, 1, k / 2u < 16u ? 16u - k / 2u : 0u, k == 0u ? 0xf8u : 0xfcu, &replan, &plan);
    CHECK(c.expected == ev_scroll_expected_frames(5, 16, 2) && c.landed_at == c.expected);
    /* A step that changes once the counter has moved, or once the scroll has
     * landed with the counter back at its first value: no plan made again. */
    memset(&c, 0, sizeof c);
    ev_scroll_clock_begin(&c, 0, 1);
    for (unsigned f = 0; f < 5u; f++) ev_scroll_clock_frame(&c, 0, 0, 0xf8u, &replan, &plan);
    int replanned = 0;
    for (unsigned k = 0; k < 21u; k++) {
        ev_scroll_clock_frame(&c, 1, k < 2u ? 16u : k - 1u < 16u ? 16u - (k - 1u) : 0u, k < 4u ? 0xf8u : 0xfcu, &replan, &plan);
        if (k) replanned |= replan;
    }
    CHECK(!replanned && c.expected == ev_scroll_expected_frames(5, 16, 1) && c.landed_at == c.expected);
    ev_scroll_clock_frame(&c, 1, 16, 0xfcu, &replan, &plan);
    CHECK(!replan && c.expected == ev_scroll_expected_frames(5, 16, 1));
}

/* A copy the game's queue holds back at the freeze: stream B queued index 5
 * a frame before it (its counter at 3 of 4) and the game has not loaded it,
 * the tile showing index 4.  The view's loops show index 5 on their way, but
 * not their last load of it: the tile shows index 4 up to the landing, as
 * the game's does until it loads the copy, never 5 then 4 again.  The same
 * with two copies of the stream held back. */
static void queued_copy(void)
{
    OraclesAnimationState frozen = frozen_state();
    frozen.head = 3; frozen.tail = 4; frozen.queue[4] = 5;
    OraclesScrollAnimation s;
    CHECK(oracles_scroll_animation_begin(&s, &frozen, 30, &tables, rom, ROM_SIZE) == 2u);
    uint8_t tiles[2u * ORACLES_ANIMATION_TILE_BYTES];
    memset(tiles, 0, sizeof tiles);
    tiles[16] = 0x14;
    int five_on_the_way = 0;
    uint8_t before_landing = 0;
    for (unsigned f = 0; f < 30u; f++) {
        CHECK(oracles_scroll_animation_step(&s, 29u - f, &tables, rom, ROM_SIZE, tiles, NULL) >= 0);
        if (tiles[16] == 0x15) five_on_the_way = 1;
        if (f == 28u) before_landing = tiles[16];
    }
    CHECK(five_on_the_way && before_landing == 0x14);
    CHECK(tiles[16] == 0x14 && tiles[0] == 0x10);   /* index 4 held to the landing; stream A lands on its own image */
    CHECK(memcmp(s.now.counters, frozen.counters, sizeof frozen.counters) == 0);
    /* Two copies of B held back (the queue behind): 4 queued five frames
     * before the freeze, 5 one frame before; the tile shows 7.  Neither is
     * shown at the end of the loops. */
    frozen.head = 2; frozen.tail = 4; frozen.queue[3] = 4; frozen.queue[4] = 5;
    CHECK(oracles_scroll_animation_begin(&s, &frozen, 30, &tables, rom, ROM_SIZE) == 2u);
    memset(tiles, 0, sizeof tiles);
    tiles[16] = 0x17;
    five_on_the_way = 0;
    for (unsigned f = 0; f < 30u; f++) {
        CHECK(oracles_scroll_animation_step(&s, 29u - f, &tables, rom, ROM_SIZE, tiles, NULL) >= 0);
        if (tiles[16] == 0x15) five_on_the_way = 1;
        if (f == 28u) before_landing = tiles[16];
    }
    CHECK(five_on_the_way && before_landing == 0x17 && tiles[16] == 0x17);
    /* Queued the other way round, 4 is not a copy B made before 5: only 5 is left to the game. */
    frozen.queue[3] = 5; frozen.queue[4] = 4;
    CHECK(oracles_scroll_animation_begin(&s, &frozen, 30, &tables, rom, ROM_SIZE) == 2u);
    memset(tiles, 0, sizeof tiles);
    for (unsigned f = 0; f < 30u; f++) CHECK(oracles_scroll_animation_step(&s, 29u - f, &tables, rom, ROM_SIZE, tiles, NULL) >= 0);
    CHECK(tiles[16] == 0x14);
}

/* What the band draws the live tileset with during a scroll: the view's
 * images on the tiles its streams wrote, but a tile the game has written
 * since the freeze (the graphics of the room entered), which is the game's;
 * and the tiles counted off at the landing. */
static void view_tiles(void)
{
    static uint8_t live0[TILE_DATA_BYTES], live1[TILE_DATA_BYTES], begin[2u * TILE_DATA_BYTES], images[2u * TILE_DATA_BYTES], shown[0x4000];
    uint8_t touched[96], game_wrote[96], held[96];
    memset(touched, 0, sizeof touched); memset(game_wrote, 0, sizeof game_wrote); memset(held, 0, sizeof held);
    const unsigned a = 5u, b = 384u + 300u, c = 6u, d = 7u, e = 384u + 10u;   /* bank * 384 + tile */
    memset(live0, 0x11, sizeof live0); memset(live1, 0x22, sizeof live1);
    memset(live0 + d * 16u, 0x77, 16u);      /* d: frozen on the image the view lands on */
    memset(live1 + 10u * 16u, 0x77, 16u);    /* e: the same, in bank 1 */
    memcpy(begin, live0, TILE_DATA_BYTES); memcpy(begin + TILE_DATA_BYTES, live1, TILE_DATA_BYTES);
    memset(images, 0x77, sizeof images);
    touched[a >> 3] |= (uint8_t)(1u << (a & 7u)); touched[b >> 3] |= (uint8_t)(1u << (b & 7u)); touched[d >> 3] |= (uint8_t)(1u << (d & 7u));
    touched[e >> 3] |= (uint8_t)(1u << (e & 7u));
    memset(live1 + 300u * 16u, 0x33, 16u);   /* b: the game writes it, over a tile the view animates */
    memset(live0 + c * 16u, 0x44, 16u);      /* c: the game writes it, a tile the view leaves alone */
    CHECK(ev_scroll_note_game_writes(live0, live1, begin, touched, game_wrote) == 1u);   /* b given back, c not the view's */
    CHECK((game_wrote[b >> 3] >> (b & 7u)) & 1u && (game_wrote[c >> 3] >> (c & 7u)) & 1u && !((game_wrote[a >> 3] >> (a & 7u)) & 1u));
    CHECK(ev_scroll_note_game_writes(live0, live1, begin, touched, game_wrote) == 0u);   /* given back once */
    memset(live1 + 300u * 16u, 0x22, 16u);   /* back to what it was: still the game's */
    CHECK(ev_scroll_note_game_writes(live0, live1, begin, touched, game_wrote) == 0u);
    /* Drawn with the banks 0x2000 apart, as a render's VRAM. */
    memset(shown, 0, sizeof shown);
    memcpy(shown, live0, TILE_DATA_BYTES); memcpy(shown + 0x2000u, live1, TILE_DATA_BYTES);
    ev_scroll_overlay(shown, 0x2000u, images, touched, game_wrote);
    CHECK(shown[a * 16u] == 0x77 && shown[a * 16u + 15u] == 0x77);                 /* the view's image */
    CHECK(shown[0x2000u + 300u * 16u] == 0x22);                                     /* the game's own */
    memset(shown + 0x2000u + 10u * 16u, 0, 16u);
    ev_scroll_overlay(shown, 0x2000u, images, touched, game_wrote);
    CHECK(shown[0x2000u + 10u * 16u] == 0x77 && shown[TILE_DATA_BYTES + 10u * 16u] == 0);   /* bank 1 at its own place */
    CHECK(shown[c * 16u] == 0x44 && shown[8u * 16u] == 0x11 && shown[0x2000u + 299u * 16u] == 0x22);
    /* At the landing: a differs from the live VRAM, d does not, b is the game's. */
    CHECK(ev_scroll_tiles_off(live0, live1, images, touched, held, game_wrote) == 1u);
    held[a >> 3] |= (uint8_t)(1u << (a & 7u));   /* a copy the queue holds back: loaded by the game after */
    CHECK(ev_scroll_tiles_off(live0, live1, images, touched, held, game_wrote) == 0u);
}

int main(void)
{
    build();
    periods();
    lands();
    loops_counted();
    plan_again();
    gaps();
    clock_lands();
    queued_copy();
    view_tiles();
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("scroll animation: ok\n");
    return 0;
}
