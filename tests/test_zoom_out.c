/* The drawn-back view without a ROM: the geometry of a 480x270
 * surface in the camera's world math, the compositor and the view on a
 * synthetic core, the refusal of a savestate taken at the other size, and,
 * at both sizes, the scenes of both games' intros the band shows or frames, the
 * pictures their cutscenes show between rooms, and the
 * ghost's captures of a fade, never kept. */
#include "camera.h"
#include "compositor.h"
#include "guest_struct_offsets.h"
#include "ppu.h"
#include "state.h"
#include "view.h"
#include "view_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const OraclesCompatProfile *original_profile(OraclesGame game)
{
    const OraclesRomInfo info = { .game = game, .revision = game == ORACLES_GAME_SEASONS ? ORACLES_ROM_REVISION_SEASONS_US : ORACLES_ROM_REVISION_AGES_US, .known = 1 };
    return oracles_compat_find(&info);
}

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

#define W 480u
#define H 270u
#define BAND_H 254u
#define HUD_X 160u

static void world_math(void)
{
    OraclesEnhancedWorld w;
    /* Outdoors the map is the extent at any band: the viewport only says whether it pans. */
    CHECK(oracles_enhanced_world_from_room(0, 0x23, 0, 40, 50, 16, 14, 0, 0, 0, 0, 0, 0x23, W, &w) == 1);
    CHECK(w.bounds_width == 14 * 160 && w.bounds_origin_x == 0 && w.wide && w.viewport == 480);
    CHECK(oracles_enhanced_world_from_room(1, 0x23, 0, 40, 50, 16, 14, 0, 0, 0, 0, 0, 0x23, W, &w) == 1 && w.viewport == 480);
    CHECK(oracles_enhanced_world_from_room(0, 0x23, 0, 40, 50, 16, 14, 0, 0, 0, 0, 0, 0x23, 0, &w) == 1 && w.viewport == 256);
    /* A room shown alone outdoors (the Maku tree's screen) stands centred in the whole band. */
    CHECK(oracles_enhanced_world_from_room(0, 0x38, 0, 80, 40, 16, 14, 0, 0, 0, 0, 1, 0x38, W, &w) == 1);
    CHECK(w.bounds_width == 480 && w.bounds_origin_x == 8 * 160 - 160 && w.wide);
    /* An interior keeps the normal band's 256. */
    CHECK(oracles_enhanced_world_from_room(2, 0x11, 0, 80, 40, 16, 14, 0, 0, 0, 0, 0, 0x11, W, &w) == 1);
    CHECK(w.bounds_width == 256 && w.bounds_origin_x == 160 - 48 && w.wide && w.viewport == 256);
    /* A room of Ages' open sea (group 2, rows 9 to 13, columns 0 to 13): the sea is its extent, the whole band its viewport... */
    const uint8_t sea[4] = { 0, 9, 14, 5 };
    CHECK(oracles_enhanced_world_from_room(2, 0xb6, 0, 80, 64, 16, 14, 0, 0, 0, 0xfu, 0, 0xb6, W, &w) == 1 && w.viewport == 256);
    oracles_enhanced_world_extend_to_sea(&w, sea, W);
    CHECK(w.viewport == 480 && w.wide && w.bounds_origin_x == 0 && w.bounds_width == 14 * 160 && w.bounds_origin_y == 9 * 128 && w.bounds_height == 5 * 128 && !w.alone);
    CHECK(w.link_x == 6 * 160 + 80 && w.link_y == 11 * 128 + 64);
    /* ... but in the normal band, which keeps the room and the sea beside it. */
    CHECK(oracles_enhanced_world_from_room(2, 0xb6, 0, 80, 64, 16, 14, 0, 0, 0, 0xfu, 0, 0xb6, 0, &w) == 1);
    const OraclesEnhancedWorld normal = w;
    oracles_enhanced_world_extend_to_sea(&w, sea, 0);
    CHECK(memcmp(&w, &normal, sizeof w) == 0 && w.bounds_width == 3 * 160 && w.viewport == 256);
}

static void compositor(void)
{
    uint32_t *core = malloc(160u * 144u * sizeof *core);
    for (unsigned y = 0; y < 144u; y++)
        for (unsigned x = 0; x < 160u; x++) core[y * 160u + x] = 0xff000000u | ((uint32_t)x << 8) | y;
    uint32_t *surface = malloc(ORACLES_ENHANCED_ZOOM_WIDTH * ORACLES_ENHANCED_ZOOM_HEIGHT * sizeof *surface);
    CHECK(oracles_enhanced_size(1).width == W && oracles_enhanced_size(1).height == H);
    CHECK(oracles_enhanced_band_height(oracles_enhanced_size(1)) == BAND_H && oracles_enhanced_hud_x(oracles_enhanced_size(1)) == HUD_X);

    /* Framed: the core's image centred both ways, the border around it. */
    OraclesEnhancedCompose in;
    memset(&in, 0, sizeof in);
    in.mode = ORACLES_ENHANCED_FRAMED;
    in.border = 0xff112233u;
    in.size = oracles_enhanced_size(1);
    const unsigned top = (H - 144u) / 2u;
    CHECK(oracles_enhanced_compose(core, &in, surface) == 0);
    CHECK(surface[top * W + HUD_X] == core[0] && surface[(top + 143u) * W + HUD_X + 159u] == core[143u * 160u + 159u]);
    CHECK(surface[(top - 1u) * W + HUD_X] == 0xff112233u && surface[top * W + HUD_X - 1u] == 0xff112233u && surface[0] == 0xff112233u);

    /* World: the status bar centred at 160; the window where the camera puts it; the rest of the band black, counted. */
    in.mode = ORACLES_ENHANCED_WORLD;
    in.window_world_left = 1000; in.window_world_top = 500;
    in.world_left = 1000 - 160; in.world_top = 500 - 63;
    CHECK(oracles_enhanced_compose(core, &in, surface) == W * BAND_H - 160u * 128u);
    CHECK(surface[HUD_X] == core[0] && surface[HUD_X - 1u] == 0xff112233u);
    CHECK(surface[(16u + 63u) * W + HUD_X] == core[16u * 160u] && surface[(16u + 63u) * W + HUD_X - 1u] == 0xff000000u);
    /* The normal band shown alone, centred (an interior): black around it, and not counted. */
    in.shown_width = 256u; in.shown_height = 128u;
    CHECK(oracles_enhanced_compose(core, &in, surface) == 256u * 128u - 160u * 128u);
    CHECK(surface[(16u + 63u) * W + 112u] == 0xff000000u && surface[(16u + 63u) * W + HUD_X] == core[16u * 160u]);
    /* A large room's own rectangle counted, the black around it decided: here the window, 160 by 128, and 40 lines below it uncovered. */
    in.shown_width = 0; in.shown_height = 0;
    in.counted_left = 1000; in.counted_top = 500; in.counted_width = 160u; in.counted_height = 168u;
    CHECK(oracles_enhanced_compose(core, &in, surface) == 160u * 40u);
    in.counted_width = 0;
    /* Two neighbours over the same pixels: the first wins, then the second where the first does not reach. */
    {
        static uint32_t first[80u * 128u], second[160u * 128u];
        for (unsigned i = 0; i < 80u * 128u; i++) first[i] = 0xff0000aau;
        for (unsigned i = 0; i < 160u * 128u; i++) second[i] = 0xff00bb00u;
        const OraclesEnhancedNeighbour two[2] = { { 1160, 500, first, 80u, 128u, 1, 0, 0u }, { 1160, 500, second, 160u, 128u, 1, 0, 0u } };
        in.neighbours = two; in.neighbour_count = 2;
        CHECK(oracles_enhanced_compose(core, &in, surface) == W * BAND_H - 320u * 128u);
        CHECK(surface[(16u + 63u) * W + HUD_X + 170u] == 0xff0000aau && surface[(16u + 63u) * W + HUD_X + 250u] == 0xff00bb00u);
        in.neighbours = NULL; in.neighbour_count = 0;
    }
    free(core); free(surface);
}

/* A room kept dark is the band's own black, faded with the game as the black no source covers, and covered; a room
 * fading in is its pixels that far toward that black. */
static void dark_sources(void)
{
    uint32_t *core = malloc(160u * 144u * sizeof *core);
    for (unsigned i = 0; i < 160u * 144u; i++) core[i] = 0xff405060u;
    uint32_t *surface = malloc(W * H * sizeof *surface);
    static uint32_t colours[ORACLES_PPU_COLOURS], green[160u * 128u], raw[160u * 128u];
    for (unsigned c = 0; c < ORACLES_PPU_COLOURS; c++) colours[c] = 0xff000000u | ((c * 37u + 0x050505u) & 0xffffffu);
    for (unsigned i = 0; i < 160u * 128u; i++) { green[i] = 0xff00bb00u; raw[i] = 0x1234u; }
    OraclesEnhancedNeighbour rooms[2] = { { 1160, 500, green, 0, 0, 1, 256u, 0u }, { 840, 500, raw, 0, 0, 0, 128u, 0u } };
    OraclesEnhancedCompose in;
    memset(&in, 0, sizeof in);
    in.mode = ORACLES_ENHANCED_WORLD;
    in.size = oracles_enhanced_size(1);
    in.window_world_left = 1000; in.window_world_top = 500;
    in.world_left = 1000 - 160; in.world_top = 500 - 63;
    in.neighbours = rooms; in.neighbour_count = 2;
    const unsigned dark = (16u + 63u) * W + HUD_X + 200u, veiled = (16u + 63u) * W + 10u, open = 16u * W;
    const int fades[3] = { 0, -12, 20 };
    for (unsigned f = 0; f < 3u; f++) {
        in.fade = fades[f];
        for (unsigned c = 0; c < 2u; c++) {
            in.colours = c ? colours : NULL;
            CHECK(oracles_enhanced_compose(core, &in, surface) == W * BAND_H - 3u * 160u * 128u);
            const uint32_t black = surface[open];
            CHECK(black == (fades[f] ? oracles_enhanced_faded_pixel(in.colours, 0, fades[f]) : 0xff000000u));
            CHECK(surface[dark] == black && surface[(16u + 190u) * W + HUD_X + 319u] == black);
            const uint32_t pixel = oracles_enhanced_faded_pixel(in.colours, 0x1234u, fades[f]);
            uint32_t half = 0xff000000u;
            for (unsigned shift = 0; shift < 24u; shift += 8u) half |= ((((pixel >> shift) & 0xffu) + ((black >> shift) & 0xffu)) * 128u >> 8) << shift;
            CHECK(surface[veiled] == half);
        }
    }
    /* With no pixels to read, a dark room is drawn all the same; without its veil, it is not. */
    rooms[0].game_area = NULL;
    in.fade = 0; in.colours = NULL;
    CHECK(oracles_enhanced_compose(core, &in, surface) == W * BAND_H - 3u * 160u * 128u && surface[dark] == 0xff000000u);
    rooms[0].veil = 0;
    CHECK(oracles_enhanced_compose(core, &in, surface) == W * BAND_H - 2u * 160u * 128u);
    /* The fade in: from the band's black, the frame a room is first shown 1, to its pixels after sixteen. */
    CHECK(oracles_enhanced_reveal_veil(1) == 240u && oracles_enhanced_reveal_veil(8) == 128u && oracles_enhanced_reveal_veil(15) == 16u);
    CHECK(oracles_enhanced_reveal_veil(ORACLES_ENHANCED_REVEAL_FRAMES) == 0u && oracles_enhanced_reveal_veil(1000) == 0u);
    for (uint32_t age = 1; age < ORACLES_ENHANCED_REVEAL_FRAMES; age++) CHECK(oracles_enhanced_reveal_veil(age + 1u) < oracles_enhanced_reveal_veil(age));
    free(core); free(surface);
}

/* A pixel `veil` 256ths of the way to black, as the compositor blends it. */
static uint32_t veiled_pixel(uint32_t pixel, unsigned veil)
{
    uint32_t out = 0xff000000u;
    for (unsigned shift = 0; shift < 24u; shift += 8u) out |= (((pixel >> shift) & 0xffu) * (256u - veil) >> 8) << shift;
    return out;
}

/* The left edge's pixel `nx` pixels into a room, `full` pixels in full there, its light times `strength` / 256 against
 * the room's own veil. */
static uint32_t edge_pixel(uint32_t pixel, unsigned nx, unsigned full, unsigned strength, unsigned veil)
{
    const unsigned light = (nx < full ? 256u : nx < full + 3u ? 169u : nx < full + 6u ? 102u : 0u) * strength >> 8;
    const unsigned v = (256u - light) * veil >> 8;
    return v >= 256u ? 0xff000000u : veiled_pixel(pixel, v);
}

/* The edge of a room kept dark: along the window drawn in clear beside it, its pixels in full over the wave, then the
 * two steps, then black, every limit on the wave, which the world gives, wherever the camera; round the corner a room
 * shares with the window alone, as the band of the room above it goes on; nothing between two dark rooms, nothing past
 * its depth; the fade in from that look; the hard black with the edges off. */
static void dark_edges(void)
{
    uint32_t *core = malloc(160u * 144u * sizeof *core);
    for (unsigned i = 0; i < 160u * 144u; i++) core[i] = 0xff405060u;
    uint32_t *surface = malloc(W * H * sizeof *surface);
    static uint32_t green[160u * 128u];
    for (unsigned i = 0; i < 160u * 128u; i++) green[i] = 0xff20c040u + (i % 7u);
    /* A right of the window, B below A: the window is B's corner; C left of the window, D above it. */
    OraclesEnhancedNeighbour rooms[4] = { { 1160, 500, green, 0, 0, 1, 256u, 0u }, { 1160, 628, green, 0, 0, 1, 256u, 0u },
                                          { 840, 500, green, 0, 0, 1, 256u, 0u }, { 1000, 372, green, 0, 0, 1, 256u, 0u } };
    OraclesEnhancedCompose in;
    memset(&in, 0, sizeof in);
    in.mode = ORACLES_ENHANCED_WORLD;
    in.size = oracles_enhanced_size(1);
    in.window_world_left = 1000; in.window_world_top = 500;
    in.world_left = 840; in.world_top = 437;
    in.neighbours = rooms; in.neighbour_count = 4;
    in.soft_edges = 1; in.edge_seed = 3;
    #define AT(wx, wy) surface[(16u + (unsigned)((wy) - in.world_top)) * W + (unsigned)((wx) - in.world_left)]
    const uint32_t depth = ORACLES_ENHANCED_EDGE_DEPTH;
    CHECK(depth == 11u);
    /* The wave: whole pixels up to five, smooth, the world's alone. */
    unsigned values = 0, steep = 0;
    for (int32_t along = -300; along < 300; along++) {
        const unsigned f = oracles_enhanced_edge_wave(3, 1160, 1, along), g = oracles_enhanced_edge_wave(3, 1160, 1, along + 1);
        values |= 1u << f;
        steep += (f > g ? f - g : g - f) > 2u;
        CHECK(f <= ORACLES_ENHANCED_EDGE_WAVE && f == oracles_enhanced_edge_wave(3, 1160, 1, along));
    }
    CHECK(steep == 0u && (values & 0x3eu) == 0x3eu);
    unsigned differ = 0;
    for (int32_t along = 0; along < 160; along++) differ += oracles_enhanced_edge_wave(3, 1160, 1, along) != oracles_enhanced_edge_wave(4, 1160, 1, along);
    CHECK(differ > 40u);
    CHECK(oracles_enhanced_compose(core, &in, surface) == W * BAND_H - 3u * 160u * 128u - 160u * (BAND_H - 191u) - 160u * 63u);
    unsigned wrong = 0, shown = 0;
    for (int32_t ny = 0; ny < 128; ny++) {
        const unsigned full = oracles_enhanced_edge_wave(3, 1160, 1, 500 + ny);
        for (int32_t nx = 0; nx < 160; nx++) {
            const uint32_t pixel = AT(1160 + nx, 500 + ny);
            wrong += pixel != edge_pixel(green[ny * 160 + nx], (unsigned)nx, full, 256u, 256u);
            shown += pixel != 0xff000000u;
        }
    }
    CHECK(wrong == 0u && shown > 128u * 6u && shown < 128u * 11u);
    /* C's right side and D's bottom, along the window, the same from the other sides. */
    wrong = 0;
    for (int32_t ny = 0; ny < 128; ny++) {
        const unsigned full = oracles_enhanced_edge_wave(3, 1000, 1, 500 + ny);
        for (int32_t nx = 0; nx < 160; nx++) wrong += AT(840 + nx, 500 + ny) != edge_pixel(green[ny * 160 + nx], 159u - (unsigned)nx, full, 256u, 256u);
    }
    for (int32_t ny = 65; ny < 128; ny++)
        for (int32_t nx = 0; nx < 160; nx++)
            wrong += AT(1000 + nx, 372 + ny) != edge_pixel(green[ny * 160 + nx], 127u - (unsigned)ny, oracles_enhanced_edge_wave(3, 500, 0, 1000 + nx), 256u, 256u);
    CHECK(wrong == 0u);
    /* B: its first row goes on with A's band, its top beyond the corner and its left below it black. */
    wrong = 0;
    const unsigned b_full = oracles_enhanced_edge_wave(3, 1160, 1, 628);
    for (int32_t nx = 0; nx < 160; nx++) wrong += AT(1160 + nx, 628) != edge_pixel(green[nx], (unsigned)nx, b_full, 256u, 256u);
    CHECK(wrong == 0u);
    CHECK(AT(1160 + 80, 628) == 0xff000000u && AT(1160, 628 + depth) == 0xff000000u && AT(1160 + depth, 628 + 1) == 0xff000000u);
    CHECK(AT(1160 + depth - 1u, 628 + depth - 1u) == 0xff000000u && AT(1160, 628 + 1) != 0xff000000u);
    /* Read past the edge's depth, nothing in clear; a pixel there lit is a room drawn in clear, one inside it is not. */
    CHECK(oracles_enhanced_dark_lit(&in, surface) == 0u);
    AT(1160 + depth, 564) = green[0];
    CHECK(oracles_enhanced_dark_lit(&in, surface) == 1u);
    AT(1160 + depth, 564) = 0xff000000u;
    AT(1160 + 80, 628 + depth) = green[0];
    CHECK(oracles_enhanced_dark_lit(&in, surface) == 1u);
    AT(1160 + 80, 628 + depth) = 0xff000000u;
    AT(1160 + 159u - depth, 520) = green[0];
    CHECK(oracles_enhanced_dark_lit(&in, surface) == 1u);
    AT(1160 + 159u - depth, 520) = 0xff000000u;
    AT(1160 + depth - 1u, 564) = green[0];
    CHECK(oracles_enhanced_dark_lit(&in, surface) == 0u);
    /* The camera elsewhere, the same edge on the same pixels of the world. */
    uint32_t *before = malloc(W * H * sizeof *before);
    oracles_enhanced_compose(core, &in, surface);
    memcpy(before, surface, W * H * sizeof *surface);
    in.world_left += 7; in.world_top -= 3;
    oracles_enhanced_compose(core, &in, surface);
    wrong = 0;
    for (int32_t wy = 500; wy < 628; wy++)
        for (int32_t wx = 1160; wx < 1310; wx++)
            wrong += AT(wx, wy) != before[(16u + (unsigned)(wy - 437)) * W + (unsigned)(wx - 840)];
    CHECK(wrong == 0u);
    in.world_left = 840; in.world_top = 437;
    free(before);
    /* Fading in, from that look: the edge's light against its veil; B's side toward A, A's veil less the light. */
    rooms[0].veil = 128u;
    oracles_enhanced_compose(core, &in, surface);
    wrong = 0;
    for (int32_t ny = 0; ny < 128; ny += 9) {
        const unsigned full = oracles_enhanced_edge_wave(3, 1160, 1, 500 + ny);
        for (int32_t nx = 0; nx < 160; nx++) wrong += AT(1160 + nx, 500 + ny) != edge_pixel(green[ny * 160 + nx], (unsigned)nx, full, 256u, 128u);
    }
    CHECK(wrong == 0u);
    const unsigned up_full = oracles_enhanced_edge_wave(3, 628, 0, 1160 + 80);
    CHECK(AT(1160 + 80, 628) == edge_pixel(green[80], 0u, up_full, 128u, 256u));
    CHECK(AT(1160 + 80, 628 + depth) == 0xff000000u);
    /* Lit on a side with nothing in clear beyond it: a stray edge; along the window, the edge itself. */
    rooms[0].veil = 256u;
    oracles_enhanced_compose(core, &in, surface);
    CHECK(oracles_enhanced_dark_stray(&in, surface) == 0u);
    AT(1160 + 80, 500) = green[0];
    CHECK(oracles_enhanced_dark_stray(&in, surface) == 1u);
    AT(1160 + 80, 500) = 0xff000000u;
    AT(840, 560) = green[0];
    CHECK(oracles_enhanced_dark_stray(&in, surface) == 1u);
    AT(840, 560) = 0xff000000u;
    AT(1160, 560) = green[0];
    AT(1160 + 3, 500) = green[0];
    CHECK(oracles_enhanced_dark_stray(&in, surface) == 0u);
    /* An edge coming in: its light that far toward none. */
    rooms[0].edge_veil = 128u;
    oracles_enhanced_compose(core, &in, surface);
    wrong = 0;
    for (int32_t ny = 0; ny < 128; ny += 7) {
        const unsigned full = oracles_enhanced_edge_wave(3, 1160, 1, 500 + ny);
        for (int32_t nx = 0; nx < 160; nx++) wrong += AT(1160 + nx, 500 + ny) != edge_pixel(green[ny * 160 + nx], (unsigned)nx, full, 128u, 256u);
    }
    CHECK(wrong == 0u);
    rooms[0].edge_veil = 256u;
    oracles_enhanced_compose(core, &in, surface);
    CHECK(AT(1160, 560) == 0xff000000u && AT(1160 + 1, 520) == 0xff000000u);
    rooms[0].edge_veil = 0u;
    /* The window across half of A's side (a scroll into a room the cache does not hold): no edge on it, nor on C's. */
    in.window_world_top = 564;
    oracles_enhanced_compose(core, &in, surface);
    CHECK(AT(1160, 600) == 0xff000000u && AT(1160, 520) == 0xff000000u && AT(840 + 159, 600) == 0xff000000u);
    CHECK(oracles_enhanced_dark_stray(&in, surface) == 0u && oracles_enhanced_dark_lit(&in, surface) == 0u);
    in.window_world_top = 500;
    /* Two rooms kept dark side by side: no edge between them. */
    rooms[0].veil = 256u;
    rooms[1].world_left = 1320; rooms[1].world_top = 500;
    oracles_enhanced_compose(core, &in, surface);
    CHECK(AT(1320 + 2, 560) == 0xff000000u && AT(1319, 560) == 0xff000000u);
    /* The edges off: the black cut straight, as without them. */
    in.soft_edges = 0;
    oracles_enhanced_compose(core, &in, surface);
    CHECK(AT(1160, 560) == 0xff000000u && AT(1160, 500) == 0xff000000u && oracles_enhanced_dark_lit(&in, surface) == 0u);
    #undef AT
    free(core); free(surface);
}

/* The status bar drawn full width: the colour beside each of its lines is the
 * bar's own at its edge, its tile's palette and colour index as the game
 * keeps them (bank, flip, palette), wherever the core's image shows it; a
 * line where the image shows something else keeps the border, but a blank
 * line, which extends as it is.  The fill changes the gutters of the HUD
 * band and nothing else. */
static uint32_t expand(uint16_t rgb555)   /* the compositor's plain expansion, without a pipeline */
{
    uint32_t out = 0xff000000u;
    for (unsigned shift = 0; shift < 15u; shift += 5u) out |= (uint32_t)(((rgb555 >> shift) & 31u) * 255u / 31u) << (shift / 5u * 8u);
    return out;
}

static void hud_full_width(void)
{
    static uint8_t tiles[64], attributes[64], vram0[0x2000], vram1[0x2000], palettes[64];
    const OraclesEnhancedBar bar = { tiles, attributes, vram0, vram1, palettes, 0x10u };
    #define RGB555(r, g, b) ((uint16_t)((r) | ((g) << 5) | ((b) << 10)))
    const uint16_t beige = RGB555(31, 26, 17), brown = RGB555(12, 8, 2), blue = RGB555(2, 4, 28);
    palettes[(7u * 4u + 1u) * 2u] = (uint8_t)beige; palettes[(7u * 4u + 1u) * 2u + 1u] = (uint8_t)(beige >> 8);
    palettes[(7u * 4u + 2u) * 2u] = (uint8_t)brown; palettes[(7u * 4u + 2u) * 2u + 1u] = (uint8_t)(brown >> 8);
    palettes[(6u * 4u + 3u) * 2u] = (uint8_t)blue; palettes[(6u * 4u + 3u) * 2u + 1u] = (uint8_t)(blue >> 8);
    /* Left: tile 1, colour 1 in its first column, palette 7, over the top eight lines; below, tile 1 of the second
     * bank, colour 2.  Right: tile 2 flipped across, its first column colour 3, palette 6, on all sixteen lines. */
    tiles[0] = 1; attributes[0] = 7u;
    tiles[32] = 1; attributes[32] = 7u | 0x08u;
    tiles[19] = 2; attributes[19] = 6u | 0x20u;
    tiles[32 + 19] = 2; attributes[32 + 19] = 6u | 0x20u;
    for (unsigned line = 0; line < 8u; line++) {
        vram0[16u + line * 2u] = 0x80u;                                 /* colour 1 */
        vram1[16u + line * 2u + 1u] = 0x80u;                            /* colour 2 */
        vram0[32u + line * 2u] = 0x80u; vram0[32u + line * 2u + 1u] = 0x80u;   /* colour 3 */
    }
    int hides = 1;
    CHECK(oracles_enhanced_bar_pixel(&bar, 0, 3, &hides) == beige && hides == 0);
    CHECK(oracles_enhanced_bar_pixel(&bar, 0, 12, NULL) == brown && oracles_enhanced_bar_pixel(&bar, 159, 12, NULL) == blue);
    CHECK(oracles_enhanced_bar_pixel(&bar, 158, 12, NULL) == 0u);   /* the flipped tile's other columns: colour 0, black here */

    uint32_t *core = malloc(160u * 144u * sizeof *core), fill[32];
    for (unsigned i = 0; i < 160u * 144u; i++) core[i] = 0xff000000u | (i * 2654435761u >> 8);
    uint32_t expect[32];
    for (unsigned y = 0; y < 16u; y++) {
        expect[y] = expand(y < 8u ? beige : brown);
        expect[16u + y] = expand(blue);
        core[y * 160u] = expect[y];
        core[y * 160u + 159u] = expect[16u + y];
    }
    core[5u * 160u] = 0xff00ff00u;   /* something else than the bar there: a sprite on its edge */
    CHECK(oracles_enhanced_hud_fill(&bar, NULL, NULL, core, fill) == 31u);
    unsigned wrong = 0;
    for (unsigned i = 0; i < 32u; i++) wrong += fill[i] != (i == 5u ? 0u : expect[i]);
    CHECK(wrong == 0);
    /* A line the image shows blank, white across with the LCD off: both sides take it, whatever the bar's map says. */
    {
        uint32_t saved[160], blank_fill[32];
        memcpy(saved, core + 9u * 160u, sizeof saved);
        for (unsigned x = 0; x < 160u; x++) core[9u * 160u + x] = 0xffffffffu;
        CHECK(oracles_enhanced_hud_fill(&bar, NULL, NULL, core, blank_fill) == 31u);
        CHECK(blank_fill[9] == 0xffffffffu && blank_fill[16u + 9u] == 0xffffffffu && blank_fill[5] == 0u);
        memcpy(core + 9u * 160u, saved, sizeof saved);
    }

    /* Composed: the gutters of each line take the fill, or the border on line 5; nothing else moves. */
    uint32_t *plain = malloc(W * H * sizeof *plain), *full = malloc(W * H * sizeof *full);
    OraclesEnhancedCompose in;
    memset(&in, 0, sizeof in);
    in.mode = ORACLES_ENHANCED_WORLD;
    in.border = 0xff101010u;
    in.size = oracles_enhanced_size(1);
    in.window_world_left = 1000; in.window_world_top = 500;
    in.world_left = 1000 - 160; in.world_top = 500 - 63;
    const unsigned uncovered = oracles_enhanced_compose(core, &in, plain);
    in.hud_fill = fill;
    CHECK(oracles_enhanced_compose(core, &in, full) == uncovered);
    unsigned changed = 0, outside = 0;
    for (unsigned y = 0; y < H; y++)
        for (unsigned x = 0; x < W; x++) {
            if (plain[y * W + x] == full[y * W + x]) continue;
            changed++;
            if (y >= 16u || (x >= HUD_X && x < HUD_X + 160u)) outside++;
        }
    CHECK(outside == 0 && changed == 2u * HUD_X * 16u - HUD_X);
    CHECK(full[0] == expect[0] && full[HUD_X - 1u] == expect[0] && full[15u * W] == expect[15] && full[5u * W] == 0xff101010u);
    CHECK(full[W - 1u] == expect[16] && full[HUD_X + 160u] == expect[16] && full[5u * W + W - 1u] == expect[21]);
    CHECK(full[HUD_X] == core[0] && full[16u * W] == plain[16u * W]);
    #undef RGB555
    free(plain); free(full); free(core);
}

/* The game's underwater ripple over the whole 254-line band.  The game reads
 * a table of 128 steps modulo 128, one step a line from a phase
 * (checkUpdateUnderwaterWaves): the window's 128 lines are one period, and a
 * row above or below the window ripples with the line a taller screen would
 * have there.  Every source holds the same world function, so the band must
 * be that function shifted by the row's line modulo 128, on every row.  In a
 * room shown whole, the part common to the lines (`sway`) is taken off every
 * source, the core's image drawn again without it: the band is the function
 * shifted by the line's scroll less the sway. */
static void ripple(int32_t sway)
{
    #define WORLD_PIXEL(wx, wy) (0xff000000u | ((uint32_t)((wx) & 0xfff) << 8) | (uint32_t)((wy) & 0xff))
    static const int8_t wave[8] = { 0, 1, 2, 3, 3, 2, 1, 0 };
    int16_t shift_x[128], shift_y[128];
    for (unsigned line = 0; line < 128u; line++) {   /* a steep wave on a slower one: no period shorter than 128 */
        shift_x[line] = (int16_t)(wave[line % 8u] + (int)(line / 32u));
        shift_y[line] = (int16_t)(line / 64u);
    }
    const int32_t window_left = 2000, window_top = 1000;
    const int32_t world_left = window_left - 160, world_top = window_top - 70;   /* 70 rows above the window, 56 below */
    uint32_t *core = malloc(160u * 144u * sizeof *core);
    for (unsigned y = 0; y < 144u; y++)
        for (unsigned x = 0; x < 160u; x++)
            core[y * 160u + x] = y < 16u ? 0xff000000u : WORLD_PIXEL(window_left + (int32_t)x + shift_x[y - 16u] - sway, window_top + (int32_t)(y - 16u) + shift_y[y - 16u]);
    const int32_t nb_left = world_left - 16, nb_top = world_top - 16;
    const unsigned nb_w = W + 32u, nb_h = BAND_H + 32u;
    uint32_t *area = malloc((size_t)nb_w * nb_h * sizeof *area);
    for (unsigned ny = 0; ny < nb_h; ny++)
        for (unsigned nx = 0; nx < nb_w; nx++) area[(size_t)ny * nb_w + nx] = WORLD_PIXEL(nb_left + (int32_t)nx, nb_top + (int32_t)ny);
    const OraclesEnhancedNeighbour around = { nb_left, nb_top, area, nb_w, nb_h, 1, 0, 0u };
    OraclesEnhancedCompose in;
    memset(&in, 0, sizeof in);
    in.mode = ORACLES_ENHANCED_WORLD;
    in.size = oracles_enhanced_size(1);
    in.window_world_left = window_left; in.window_world_top = window_top;
    in.world_left = world_left; in.world_top = world_top;
    in.line_shift = shift_x; in.line_shift_y = shift_y;
    in.sway = sway;
    in.neighbours = &around; in.neighbour_count = 1;
    uint32_t *surface = malloc(W * H * sizeof *surface);
    CHECK(oracles_enhanced_compose(core, &in, surface) == 0);
    unsigned wrong = 0;
    for (unsigned y = 0; y < BAND_H; y++) {
        const int32_t row_line = world_top + (int32_t)y - window_top, line = ((row_line % 128) + 128) % 128;
        for (unsigned x = 0; x < W; x++) {
            const uint32_t expect = WORLD_PIXEL(world_left + (int32_t)x + shift_x[line] - sway, world_top + (int32_t)y + shift_y[line]);
            if (surface[(16u + y) * W + x] != expect) wrong++;
        }
    }
    CHECK(wrong == 0);
    free(surface); free(area); free(core);
    #undef WORLD_PIXEL
}

/* A synthetic core running `jr -2`, whose WRAM the test writes as the game would; or running `code`. */
typedef struct rig { uint8_t *rom; OraclesCore *core; OraclesGuest *guest; } rig;
static int rig_start_code(rig *r, const OraclesCompatProfile *profile, const uint8_t *code, size_t n)
{
    const size_t size = 1024u * 1024u;
    r->rom = calloc(size, 1);
    memcpy(r->rom + 0x134, "ZELDA NAYRU", 11);
    r->rom[0x143] = 0xc0; r->rom[0x147] = 0x1b; r->rom[0x148] = 0x05; r->rom[0x149] = 0x02;
    r->rom[0x100] = 0x00; r->rom[0x101] = 0xc3; r->rom[0x102] = 0x50; r->rom[0x103] = 0x01;
    memcpy(r->rom + 0x150, code, n);
    const OraclesCoreOptions options = { 0, 0, ORACLES_CORE_SAMEBOY, 0 };
    r->core = oracles_core_create(r->rom, size, &options);
    r->guest = r->core && profile ? oracles_guest_attach(r->core, profile) : NULL;
    return r->guest != NULL;
}
static int rig_start_profile(rig *r, const OraclesCompatProfile *profile)
{
    static const uint8_t spin[] = { 0x18, 0xfe };
    return rig_start_code(r, profile, spin, sizeof spin);
}
static int rig_start_game(rig *r, OraclesGame game) { return rig_start_profile(r, original_profile(game)); }
static int rig_start(rig *r) { return rig_start_game(r, ORACLES_GAME_AGES); }
static void rig_stop(rig *r)
{
    if (r->guest) oracles_guest_detach(r->guest);
    if (r->core) oracles_core_destroy(r->core);
    free(r->rom);
}

static void frames(rig *r, OraclesEnhancedView *view, unsigned n, OraclesEnhancedMode *mode, int32_t *cam, int32_t *cam_y)
{
    for (unsigned i = 0; i < n; i++) { oracles_core_run_frame(r->core); oracles_enhanced_view_compose(view, mode, cam, cam_y); }
}

static void view_and_savestate(void)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    OraclesEnhancedView *zoom = r.guest ? oracles_enhanced_view_start(r.core, r.guest, NULL, 0) : NULL;
    OraclesEnhancedView *normal = r.guest ? oracles_enhanced_view_start(r.core, r.guest, NULL, 0) : NULL;
    OraclesEnhancedView *far_4_3 = r.guest ? oracles_enhanced_view_start(r.core, r.guest, NULL, 0) : NULL;
    CHECK(zoom && normal && far_4_3);
    if (!zoom || !normal || !far_4_3) { oracles_enhanced_view_stop(zoom); oracles_enhanced_view_stop(normal); oracles_enhanced_view_stop(far_4_3); rig_stop(&r); return; }
    oracles_enhanced_view_set_zoom_out(zoom, 1);
    oracles_enhanced_view_set_size(far_4_3, oracles_enhanced_view_size(ORACLES_ENHANCED_FAR, ORACLES_ENHANCED_4_3));
    CHECK(oracles_enhanced_view_width(far_4_3) == W && oracles_enhanced_view_height(far_4_3) == 360u);
    CHECK(oracles_enhanced_view_width(zoom) == W && oracles_enhanced_view_height(zoom) == H);
    CHECK(oracles_enhanced_view_width(normal) == 256u && oracles_enhanced_view_height(normal) == 144u);

    /* Outdoors the whole band is shown and Link stands at its middle. */
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    uint8_t *w0 = oracles_guest_wram_writable(r.guest, 0), *w1 = oracles_guest_wram_writable(r.guest, 1);
    uint8_t *link = w1 + (ORACLES_OBJECTS_BASE - 0xd000u);
    #define W0(sym) w0[(sym).addr - 0xc000u]
    W0(t->game_state) = 2; W0(t->cutscene_index) = 1; W0(t->scroll_mode) = 1; W0(t->screen_transition_state) = 2;
    W0(t->active_group) = 0; W0(t->active_room) = 0x45; W0(t->room_is_large) = 0;
    link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_STATE] = 1; link[ORACLES_OBJ_XH] = 80; link[ORACLES_OBJ_YH] = 64;
    OraclesEnhancedMode mode = ORACLES_ENHANCED_FRAMED;
    int32_t cam = 0, cam_y = 0;
    unsigned shown_w = 0, shown_h = 0;
    frames(&r, zoom, 40, &mode, &cam, &cam_y);
    oracles_enhanced_view_shown(zoom, &shown_w, &shown_h);
    CHECK(mode == ORACLES_ENHANCED_WORLD && shown_w == W && shown_h == BAND_H);
    CHECK(cam == 5 * 160 + 80 - 240 && cam_y == 4 * 128 + 64 - 127);
    /* The status bar drawn full width from one composition to the next, and back: the gutters take the bar's edge,
     * then the border again. */
    {
        const uint32_t *shown = oracles_enhanced_view_surface(zoom);
        const uint32_t border = shown[0];
        unsigned hud_frames = 0, kept = 0;
        oracles_enhanced_view_set_hud(zoom, 1);
        frames(&r, zoom, 1, &mode, &cam, &cam_y);
        oracles_enhanced_view_hud_counts(zoom, &hud_frames, &kept);
        CHECK(oracles_enhanced_view_hud(zoom) == 1 && mode == ORACLES_ENHANCED_WORLD && hud_frames == 1u && kept == 0u);
        CHECK(shown[0] != border && shown[0] == shown[HUD_X] && shown[15u * W + W - 1u] == shown[15u * W + HUD_X + 159u]);
        oracles_enhanced_view_set_hud(zoom, 0);
        frames(&r, zoom, 1, &mode, &cam, &cam_y);
        oracles_enhanced_view_hud_counts(zoom, &hud_frames, &kept);
        CHECK(hud_frames == 1u && shown[0] == border && shown[W - 1u] == border);
        /* Changed with the game paused: the last surface's band drawn again at once as the next composition draws it,
         * the world below as it was, nothing counted. */
        static uint32_t centred[W * H], redrawn[W * H];
        const size_t band = 16u * W * sizeof *shown;
        memcpy(centred, shown, sizeof centred);
        oracles_enhanced_view_set_hud(zoom, 1);
        CHECK(oracles_enhanced_view_redraw_hud(zoom) == 1);
        oracles_enhanced_view_hud_counts(zoom, &hud_frames, &kept);
        CHECK(hud_frames == 1u && shown[0] != border && shown[0] == shown[HUD_X] && !memcmp(shown + 16u * W, centred + 16u * W, sizeof centred - band));
        memcpy(redrawn, shown, sizeof redrawn);
        frames(&r, zoom, 1, &mode, &cam, &cam_y);
        CHECK(mode == ORACLES_ENHANCED_WORLD && !memcmp(shown, redrawn, band));
        oracles_enhanced_view_set_hud(zoom, 0);
        CHECK(oracles_enhanced_view_redraw_hud(zoom) == 1 && !memcmp(shown, centred, band));
        frames(&r, zoom, 1, &mode, &cam, &cam_y);
    }

    /* The state names its size; the other size refuses it, and says why. */
    uint8_t wire[1024];
    size_t written = 0;
    char why[256];
    CHECK(oracles_enhanced_view_save_state(zoom, wire, sizeof wire, &written) == 0 && written == 2u * ORACLES_E11_STATE_WIRE_SIZE + 12u + 8u + 12u);
    CHECK(oracles_enhanced_view_check_state(zoom, wire, written, why, sizeof why) == 0);
    CHECK(oracles_enhanced_view_load_state(zoom, wire, written) == 0);
    CHECK(oracles_enhanced_view_check_state(zoom, wire, written - 12u, why, sizeof why) == 0);   /* without the whole offsets: a state saved before them */
    CHECK(oracles_enhanced_view_load_state(zoom, wire, written - 12u) == 0);
    /* It names both surfaces by the view's level, as Display does, and the screen's shape. */
    CHECK(oracles_enhanced_view_check_state(normal, wire, written, why, sizeof why) != 0);
    CHECK(!strcmp(why, "the savestate was taken on the far view, 480x270 in 16:9, "
                       "and this session shows the near view, 256x144 in 16:9: it loads only on its own surface"));
    CHECK(oracles_enhanced_view_load_state(normal, wire, written) != 0);
    /* The far view in the other shape refuses it too, and names the setting that shows the surface it was taken on. */
    CHECK(oracles_enhanced_view_check_state(far_4_3, wire, written, why, sizeof why) != 0);
    CHECK(!strcmp(why, "the savestate was taken on the far view, 480x270 in 16:9, and this session shows the far view, 480x360 in 4:3: "
                       "it loads only on its own surface; aspect=16:9 in the settings shows that surface"));
    CHECK(oracles_enhanced_view_load_state(far_4_3, wire, written) != 0);
    size_t written_4_3 = 0;
    uint8_t wire_4_3[1024];
    CHECK(oracles_enhanced_view_save_state(far_4_3, wire_4_3, sizeof wire_4_3, &written_4_3) == 0);
    CHECK(oracles_enhanced_view_check_state(zoom, wire_4_3, written_4_3, why, sizeof why) != 0 && strstr(why, "aspect=4:3 in the settings"));
    CHECK(oracles_enhanced_view_check_state(far_4_3, wire_4_3, written_4_3, why, sizeof why) == 0);
    CHECK(oracles_enhanced_view_save_state(normal, wire, sizeof wire, &written) == 0 && written == 2u * ORACLES_E11_STATE_WIRE_SIZE + 12u + 12u);
    CHECK(oracles_enhanced_view_check_state(zoom, wire, written, why, sizeof why) != 0 && strstr(why, "the near view, 256x144 in 16:9, and this session shows the far view"));
    CHECK(oracles_enhanced_view_load_state(zoom, wire, written) != 0);
    CHECK(oracles_enhanced_view_load_state(zoom, wire, 2u * ORACLES_E11_STATE_WIRE_SIZE) != 0);   /* a state from before the size record: the normal surface's */
    CHECK(oracles_enhanced_view_load_state(normal, wire, written) == 0);

    /* A state taken in a scroll down from 0x45 into 0x55, the room entered already the game's active one (wScrollMode 8,
     * getNextActiveRoom) while Link's coordinates stay the room left's.  The host state keeps its format, the size's
     * record where it was, and the scroll's record travels in the container's last field: a build before that field
     * reads the fields by index and loads the state whole, its far surface included. */
    {
        W0(t->screen_transition_direction) = 2; W0(t->scroll_mode) = 4; W0(t->screen_transition_state) = 3;
        frames(&r, zoom, 1, &mode, &cam, &cam_y);
        W0(t->scroll_mode) = 8; W0(t->active_room) = 0x55;
        frames(&r, zoom, 3, &mode, &cam, &cam_y);
        CHECK(oracles_enhanced_view_observation(zoom)->world.origin_y == 4 * 128);   /* the room left */
        uint8_t host[1024], scroll[ORACLES_ENHANCED_SCROLL_STATE_SIZE + 4u];
        size_t host_size = 0, scroll_size = 0;
        CHECK(oracles_enhanced_view_save_state(zoom, host, sizeof host, &host_size) == 0 && host_size == 2u * ORACLES_E11_STATE_WIRE_SIZE + 12u + 8u + 12u);
        CHECK(oracles_enhanced_view_save_scroll_state(zoom, scroll, sizeof scroll, &scroll_size) == 0 && scroll_size == ORACLES_ENHANCED_SCROLL_STATE_SIZE);
        CHECK(oracles_enhanced_view_save_scroll_state(zoom, scroll, ORACLES_ENHANCED_SCROLL_STATE_SIZE - 1u, &scroll_size) != 0);
        CHECK(oracles_enhanced_view_save_scroll_state(zoom, scroll, sizeof scroll, &scroll_size) == 0);
        OraclesStateInfo info = { "ages", "0123456789abcdef0123456789abcdef01234567", "", host, host_size, NULL, 0, NULL, 0, NULL, 0, NULL, 0, scroll, scroll_size };
        uint8_t *state = NULL, *loaded = NULL;
        size_t state_size = 0, loaded_size = 0;
        const uint8_t *peek = NULL;
        size_t peek_size = 0;
        CHECK(oracles_state_serialize(r.core, &info, &state, &state_size, why, sizeof why) == 0);
        CHECK(oracles_state_host_state(state, state_size, &peek, &peek_size) == 0 && peek_size == host_size && !memcmp(peek, host, host_size));
        const OraclesEnhancedSize taken = oracles_enhanced_view_state_surface(peek, peek_size);
        CHECK(taken.width == W && taken.height == H && oracles_enhanced_view_check_state(zoom, peek, peek_size, why, sizeof why) == 0);
        CHECK(oracles_enhanced_view_check_state(normal, peek, peek_size, why, sizeof why) != 0 && strstr(why, "the far view, 480x270"));
        CHECK(oracles_state_view_scroll_state(state, state_size, &peek, &peek_size) == 0 && peek_size == scroll_size && !memcmp(peek, scroll, scroll_size));
        CHECK(oracles_state_deserialize(r.core, &info, state, state_size, &loaded, &loaded_size, why, sizeof why) == 0 && loaded_size == host_size);
        free(loaded);
        free(state);
        /* Loaded without the scroll's record (a state taken before it), the room entered stands for the reference; with
         * it, the room left, to the scroll's end. */
        CHECK(oracles_enhanced_view_load_state(zoom, host, host_size) == 0);
        frames(&r, zoom, 1, &mode, &cam, &cam_y);
        CHECK(oracles_enhanced_view_observation(zoom)->world.origin_y == 5 * 128);
        CHECK(oracles_enhanced_view_load_state(zoom, host, host_size) == 0 && oracles_enhanced_view_load_scroll_state(zoom, scroll, scroll_size) == 0);
        frames(&r, zoom, 1, &mode, &cam, &cam_y);
        CHECK(mode == ORACLES_ENHANCED_WORLD && oracles_enhanced_view_observation(zoom)->world.origin_y == 4 * 128);
        /* Refused: cut, of another kind, of a state not taken in a scroll, or loaded where the game is in none or in
         * another group. */
        CHECK(oracles_enhanced_view_load_scroll_state(zoom, scroll, scroll_size - 1u) != 0);
        uint8_t other[ORACLES_ENHANCED_SCROLL_STATE_SIZE];
        memcpy(other, scroll, sizeof other);
        other[0] = 'X';
        CHECK(oracles_enhanced_view_load_scroll_state(zoom, other, sizeof other) != 0);
        memcpy(other, scroll, sizeof other);
        other[4u + 4u * 13u] = 0;   /* was_in_scroll */
        CHECK(oracles_enhanced_view_load_scroll_state(zoom, other, sizeof other) != 0);
        memcpy(other, scroll, sizeof other);
        other[4u + 4u * 1u + 1u] = 1;   /* ref_group past a byte */
        CHECK(oracles_enhanced_view_load_scroll_state(zoom, other, sizeof other) != 0);
        W0(t->active_group) = 1;
        CHECK(oracles_enhanced_view_load_scroll_state(zoom, scroll, scroll_size) != 0);
        W0(t->active_group) = 0; W0(t->scroll_mode) = 1; W0(t->screen_transition_state) = 2;
        CHECK(oracles_enhanced_view_load_scroll_state(zoom, scroll, scroll_size) != 0);
        CHECK(oracles_enhanced_view_load_state(zoom, host, host_size) == 0);
        /* Back in 0x45 for what follows, through a warp. */
        W0(t->scroll_mode) = 2; frames(&r, zoom, 1, &mode, &cam, &cam_y);
        W0(t->active_room) = 0x45; frames(&r, zoom, 1, &mode, &cam, &cam_y);
        W0(t->scroll_mode) = 1;
        frames(&r, zoom, 40, &mode, &cam, &cam_y);
    }

    /* Indoors the normal band, centred in the surface's: the room's left edge at 160 + 48, its top at 63. */
    W0(t->scroll_mode) = 2; frames(&r, zoom, 1, &mode, &cam, &cam_y);
    W0(t->active_group) = 2; W0(t->active_room) = 0x10;
    frames(&r, zoom, 1, &mode, &cam, &cam_y);
    W0(t->scroll_mode) = 1;
    frames(&r, zoom, 40, &mode, &cam, &cam_y);
    oracles_enhanced_view_shown(zoom, &shown_w, &shown_h);
    CHECK(mode == ORACLES_ENHANCED_WORLD && shown_w == 256u && shown_h == 128u);
    CHECK(cam == 0 - 48 - 112 && cam_y == 128 - 63);
    const uint32_t *surface = oracles_enhanced_view_surface(zoom);
    const uint32_t black = surface[16u * W];   /* the band's corner, outside the part shown: black, with the game's fade */
    CHECK(surface[(16u + 10u) * W + 111u] == black && surface[(16u + 62u) * W + 240u] == black && surface[(16u + 191u) * W + 240u] == black);

    /* A large room (a dungeon's, 4:10 at column 0 and row 1 of 240x176 cells):
     * the whole band shown, the room centred both ways in it. */
    W0(t->scroll_mode) = 2; frames(&r, zoom, 1, &mode, &cam, &cam_y);
    W0(t->active_group) = 4; W0(t->active_room) = 0x10; W0(t->dungeon_index) = 0xff;
    frames(&r, zoom, 1, &mode, &cam, &cam_y);
    W0(t->scroll_mode) = 1;
    frames(&r, zoom, 40, &mode, &cam, &cam_y);
    oracles_enhanced_view_shown(zoom, &shown_w, &shown_h);
    CHECK(mode == ORACLES_ENHANCED_WORLD && shown_w == W && shown_h == BAND_H);
    CHECK(cam == 0 - 120 && cam_y == 176 - 39);
    #undef W0
    oracles_enhanced_view_stop(zoom);
    oracles_enhanced_view_stop(normal);
    oracles_enhanced_view_stop(far_4_3);
    rig_stop(&r);
}

/* A room of the overworld shown alone (the Maku tree's screen), drawn back:
 * the camera follows it the same whether its edges are open or not, so that a
 * scroll opening one moves the band without a jump, on either axis. */
static void isolated_room_edges(void)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    OraclesEnhancedCamera *camera = r.guest ? oracles_enhanced_camera_start(r.guest) : NULL;
    CHECK(camera != NULL);
    if (!camera) { rig_stop(&r); return; }
    oracles_enhanced_camera_set_band(camera, W, BAND_H);
    OraclesEnhancedObservation ob;
    memset(&ob, 0, sizeof ob);
    ob.playing = 1; ob.grid = 1; ob.epoch = 8;
    int32_t cam = 0, cam_y = 0, last = 0, last_y = 0, worst = 0, worst_y = 0;
    unsigned lost = 0;
    for (uint32_t frame = 0; frame < 180u; frame++) {
        const unsigned edges = frame < 60u ? 0u : frame < 120u ? 1u : 1u | 8u;   /* then up, then up and left */
        CHECK(oracles_enhanced_world_from_room(0, 0x38, 0, 80, 20, 16, 14, 0, 0, 0, edges, 1, 0x38, W, &ob.world) == 1);
        ob.window_left = ob.world.origin_x; ob.window_top = ob.world.origin_y;
        if (!oracles_enhanced_camera_reduce(camera, frame, &ob, &cam, &cam_y)) lost++;
        if (frame > 0) {
            const int32_t d = cam > last ? cam - last : last - cam, dy = cam_y > last_y ? cam_y - last_y : last_y - cam_y;
            if (d > worst) worst = d;
            if (dy > worst_y) worst_y = dy;
        }
        last = cam; last_y = cam_y;
        if (frame == 59u) CHECK(cam_y == 3 * 128 - 63);   /* closed: the room centred in the band */
    }
    CHECK(lost == 0 && worst <= 4 && worst_y <= 4);
    CHECK(oracles_enhanced_camera_vertical_tracking(camera));
    oracles_enhanced_camera_stop(camera);
    rig_stop(&r);
}

/* The Maku tree's screen (Ages 0:38, Link low in it) reached by a loaded game: six frames of the room loaded under
 * the closed curtain (scroll mode 2, transition state 0), the tree's interaction still at its state 0, which its first
 * update, on the sixth, leaves (the tree itself), deletes (Ages' present while it is gone) or turns into a scene's
 * (another subid); then five frames of the curtain still closed (transition state 1, substate 0), the window still
 * the game's white (the rig's image, white until its eleventh frame), the last two of them framed by the closed
 * curtain's white alone, the load's own rule letting them go; then the opening, the screen opening from its middle
 * (substate 2, 35 frames); then normal play.  Until that update the frames are the load's (framed); the five white
 * frames are framed as the load framed the wait, the very same picture; from the update the room stands alone exactly
 * when the tree stays, the first world frame showing the game's window where the last frame of the wait did, and from
 * the first world frame the camera does not move, through the opening and normal play (it glided back to Link when
 * the room was isolated on the tree not yet updated, and jumped when isolated later in the world). */
enum { MAKU_TREE_STAYS, MAKU_TREE_DELETED, MAKU_TREE_SCENE };
typedef struct maku_rig { rig r; OraclesEnhancedView *view; uint8_t *w0, *link, *tree; } maku_rig;
static int maku_start(maku_rig *m, unsigned size)
{
    memset(m, 0, sizeof *m);
    CHECK(rig_start(&m->r));
    m->view = m->r.guest ? oracles_enhanced_view_start(m->r.core, m->r.guest, NULL, 0) : NULL;
    CHECK(m->view != NULL);
    if (!m->view) { rig_stop(&m->r); return 0; }
    if (size) oracles_enhanced_view_set_zoom_out(m->view, 1);
    m->w0 = oracles_guest_wram_writable(m->r.guest, 0);
    m->link = oracles_guest_wram_writable(m->r.guest, 1) + (ORACLES_OBJECTS_BASE - 0xd000u);
    m->tree = m->link + 0x100u + ORACLES_OBJECT_SIZE;
    return 1;
}
static void maku_stop(maku_rig *m)
{
    oracles_enhanced_view_stop(m->view);
    rig_stop(&m->r);
}
#define MW0(m, sym) (m)->w0[(sym).addr - 0xc000u]
/* The game's camera, in the HRAM, at the room's corner: the rig's boot ROM uses those bytes too, from its eleventh
 * frame on. */
static void maku_camera(maku_rig *m)
{
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    size_t size = 0;
    uint16_t bank = 0;
    uint8_t *hram = oracles_core_memory(m->r.core, ORACLES_CORE_HRAM, &size, &bank);
    CHECK(hram != NULL && t->camera_x.addr >= 0xff80u && t->camera_y.addr >= 0xff80u);
    if (!hram) return;
    memset(hram + (t->camera_x.addr - 0xff80u), 0, 2u);
    memset(hram + (t->camera_y.addr - 0xff80u), 0, 2u);
}
static void maku_set(maku_rig *m, uint8_t group, uint8_t room, uint8_t scroll_mode, uint8_t transition_state)
{
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    MW0(m, t->game_state) = 2; MW0(m, t->cutscene_index) = 1; MW0(m, t->scroll_mode) = scroll_mode; MW0(m, t->screen_transition_state) = transition_state;
    MW0(m, t->active_group) = group; MW0(m, t->active_room) = room; MW0(m, t->room_is_large) = 0;
    MW0(m, t->screen_offset_x) = 0; MW0(m, t->screen_offset_y) = 0;
    maku_camera(m);
}
/* Where the frame shows the game's window: in the world, at its place in the band; framed, where the load draws it
 * (its band held at the last world frame's place, or centred under the HUD). */
static void maku_window_place(const maku_rig *m, OraclesEnhancedMode mode, int32_t cam, int32_t cam_y, int32_t *x, int32_t *y)
{
    const OraclesEnhancedView *v = m->view;
    if (mode == ORACLES_ENHANCED_WORLD) {
        *x = v->observation.window_left - cam;
        *y = (int32_t)v->band_top + v->observation.window_top - cam_y;
    } else if (v->load_area_held) {
        *x = v->load_area_left; *y = v->load_area_top;
    } else {
        *x = (int32_t)oracles_enhanced_hud_x(v->size);
        *y = (int32_t)((v->size.height - ORACLES_PPU_HEIGHT) / 2u + ORACLES_ENHANCED_HUD_HEIGHT);
    }
}
static void maku_tree_place(maku_rig *m)
{
    m->tree[ORACLES_OBJ_ENABLED] = 1; m->tree[ORACLES_OBJ_ID] = 0x87; m->tree[ORACLES_OBJ_SUBID] = 0; m->tree[ORACLES_OBJ_STATE] = 0;
}
static int32_t maku_distance(int32_t a, int32_t b) { return a > b ? a - b : b - a; }

static void maku_tree_arrival(void)
{
    for (unsigned size = 0; size < 2u; size++) {
        for (unsigned fate = MAKU_TREE_STAYS; fate <= MAKU_TREE_SCENE; fate++) {
            maku_rig m;
            if (!maku_start(&m, size)) return;
            maku_set(&m, 0, 0x38, 2, 0);
            m.link[ORACLES_OBJ_ENABLED] = 1; m.link[ORACLES_OBJ_STATE] = 1; m.link[ORACLES_OBJ_XH] = 80; m.link[ORACLES_OBJ_YH] = 101;
            maku_tree_place(&m);
            OraclesEnhancedMode mode = ORACLES_ENHANCED_FRAMED;
            int32_t cam = 0, cam_y = 0, first_x = 0, first_y = 0, worst = 0, waited_x = 0, waited_y = 0, shown_x = 0, shown_y = 0;
            unsigned pending_framed = 0, as_decided = 0, world = 0, white_framed = 0, white_same = 0, white_placed = 0;
            const size_t pixels = (size_t)oracles_enhanced_view_width(m.view) * oracles_enhanced_view_height(m.view);
            uint32_t *waited = malloc(pixels * sizeof *waited);
            CHECK(waited != NULL);
            for (unsigned i = 0; waited && i < 86u; i++) {
                if (i == 5u) {
                    if (fate == MAKU_TREE_DELETED) memset(m.tree, 0, ORACLES_OBJECT_SIZE);
                    else { m.tree[ORACLES_OBJ_ENABLED] = 0x81; m.tree[ORACLES_OBJ_STATE] = 1; if (fate == MAKU_TREE_SCENE) m.tree[ORACLES_OBJ_SUBID] = 1; }
                }
                if (i == 6u) { maku_set(&m, 0, 0x38, 2, 1); MW0(&m, oracles_guest_tables_ages.screen_transition_substate) = 0; }
                if (i == 11u) MW0(&m, oracles_guest_tables_ages.screen_transition_substate) = 2;
                if (i == 46u) maku_set(&m, 0, 0x38, 1, 2);
                oracles_core_run_frame(m.r.core);
                maku_camera(&m);
                oracles_enhanced_view_compose(m.view, &mode, &cam, &cam_y);
                if (i < 5u && mode == ORACLES_ENHANCED_FRAMED) pending_framed++;
                if (i == 4u) {
                    maku_window_place(&m, mode, cam, cam_y, &waited_x, &waited_y);
                    memcpy(waited, oracles_enhanced_view_surface(m.view), pixels * sizeof *waited);
                }
                if (i >= 6u && i < 11u) {   /* the closed curtain's white */
                    int32_t x = 0, y = 0;
                    maku_window_place(&m, mode, cam, cam_y, &x, &y);
                    white_framed += mode == ORACLES_ENHANCED_FRAMED;
                    white_placed += x == waited_x && y == waited_y;
                    white_same += memcmp(waited, oracles_enhanced_view_surface(m.view), pixels * sizeof *waited) == 0;
                }
                if (oracles_enhanced_view_isolated(m.view) == (fate == MAKU_TREE_STAYS && i >= 5u)) as_decided++;
                if (mode != ORACLES_ENHANCED_WORLD) continue;
                if (!world++) { first_x = cam; first_y = cam_y; maku_window_place(&m, mode, cam, cam_y, &shown_x, &shown_y); }
                const int32_t d = maku_distance(cam, first_x) + maku_distance(cam_y, first_y);
                if (d > worst) worst = d;
            }
            CHECK(pending_framed == 5u && as_decided == 86u);
            if (white_framed != 5u || white_placed != 5u || white_same != 5u)
                fprintf(stderr, "maku white (size %u, fate %u): framed %u, placed %u, same %u\n", size, fate, white_framed, white_placed, white_same);
            CHECK(white_framed == 5u && white_placed == 5u && white_same == 5u);
            CHECK(world >= 40u + 30u && worst == 0);
            CHECK(fate != MAKU_TREE_STAYS || (shown_x == waited_x && shown_y == waited_y));
            free(waited);
            maku_stop(&m);
        }
    }
}

/* The wait is held to ORACLES_ENHANCED_MAKU_WAIT frames: a scene showing the tree's screen without running its objects
 * (the tree left at its state 0, the screen closed) is shown in the world, the room not isolated until the tree is
 * updated. */
static void maku_tree_wait_held(void)
{
    maku_rig m;
    if (!maku_start(&m, 0)) return;
    maku_set(&m, 0, 0x38, 2, 0);
    m.link[ORACLES_OBJ_ENABLED] = 1; m.link[ORACLES_OBJ_STATE] = 1; m.link[ORACLES_OBJ_XH] = 80; m.link[ORACLES_OBJ_YH] = 101;
    maku_tree_place(&m);
    OraclesEnhancedMode mode = ORACLES_ENHANCED_FRAMED;
    int32_t cam = 0, cam_y = 0;
    unsigned waited = 0, alone = 0;
    for (unsigned i = 0; i < 30u; i++) {
        frames(&m.r, m.view, 1, &mode, &cam, &cam_y);
        if (ev_maku_pending(m.view)) waited++;
        if (oracles_enhanced_view_isolated(m.view)) alone++;
    }
    CHECK(waited == ORACLES_ENHANCED_MAKU_WAIT && alone == 0 && mode == ORACLES_ENHANCED_WORLD);
    m.tree[ORACLES_OBJ_ENABLED] = 0x81; m.tree[ORACLES_OBJ_STATE] = 1;
    frames(&m.r, m.view, 1, &mode, &cam, &cam_y);
    CHECK(!ev_maku_pending(m.view) && oracles_enhanced_view_isolated(m.view));
    maku_stop(&m);
}

/* Arrivals that leave the screen's isolation to normal play: a scroll into the tree's screen (from 0:48 below), and a
 * warp out of it (to 0:48, the objects cleared), after which no room is isolated. */
static void maku_tree_other_arrivals(void)
{
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    for (unsigned size = 0; size < 2u; size++) {
        maku_rig m;
        if (!maku_start(&m, size)) return;
        OraclesEnhancedMode mode = ORACLES_ENHANCED_FRAMED;
        int32_t cam = 0, cam_y = 0;
        unsigned alone = 0;
        maku_set(&m, 0, 0x48, 1, 2);
        m.link[ORACLES_OBJ_ENABLED] = 1; m.link[ORACLES_OBJ_STATE] = 1; m.link[ORACLES_OBJ_XH] = 80; m.link[ORACLES_OBJ_YH] = 20;
        frames(&m.r, m.view, 20, &mode, &cam, &cam_y);
        CHECK(mode == ORACLES_ENHANCED_WORLD && !oracles_enhanced_view_isolated(m.view));
        maku_set(&m, 0, 0x38, ORACLES_SCROLL_MODE_TRANSITION, 3);
        MW0(&m, t->screen_transition_direction) = 0;
        maku_tree_place(&m);
        m.tree[ORACLES_OBJ_ENABLED] = 0x81; m.tree[ORACLES_OBJ_STATE] = 1;
        for (unsigned i = 0; i < 30u; i++) {
            m.link[ORACLES_OBJ_YH] = (uint8_t)(i < 20u ? 20u - i : 120u);
            frames(&m.r, m.view, 1, &mode, &cam, &cam_y);
            if (oracles_enhanced_view_isolated(m.view)) alone++;
        }
        CHECK(alone == 0);
        maku_set(&m, 0, 0x38, 1, 2);
        frames(&m.r, m.view, 20, &mode, &cam, &cam_y);
        CHECK(mode == ORACLES_ENHANCED_WORLD && oracles_enhanced_view_isolated(m.view));
        maku_set(&m, 0, 0x48, 2, 0);
        memset(m.tree, 0, ORACLES_OBJECT_SIZE);
        alone = 0;
        for (unsigned i = 0; i < 46u; i++) {
            if (i == 6u) maku_set(&m, 0, 0x48, 2, 1);
            frames(&m.r, m.view, 1, &mode, &cam, &cam_y);
            if (oracles_enhanced_view_isolated(m.view)) alone++;
        }
        maku_set(&m, 0, 0x48, 1, 2);
        frames(&m.r, m.view, 20, &mode, &cam, &cam_y);
        CHECK(alone == 0 && mode == ORACLES_ENHANCED_WORLD && !oracles_enhanced_view_isolated(m.view));
        maku_stop(&m);
    }
}
/* A room's load framed after its blank, centred where the camera frames an interior of one room (the Maku tree's
 * screen, alone): play takes the room where the load showed it, under the HUD band as with the bar floating, in each
 * level and shape. */
static void load_band_centred(void)
{
    static const OraclesEnhancedHud huds[] = { ORACLES_ENHANCED_HUD_ORIGINAL, ORACLES_ENHANCED_HUD_WIDE, ORACLES_ENHANCED_HUD_FLOATING };
    for (unsigned h = 0; h < sizeof huds / sizeof *huds; h++)
        for (unsigned level = 0; level < ORACLES_ENHANCED_LEVELS; level++)
            for (unsigned aspect = 0; aspect < ORACLES_ENHANCED_ASPECTS; aspect++) {
                maku_rig m;
                if (!maku_start(&m, 0)) return;
                oracles_enhanced_view_set_size(m.view, oracles_enhanced_view_size((OraclesEnhancedLevel)level, (OraclesEnhancedAspect)aspect));
                oracles_enhanced_view_set_hud(m.view, (int)huds[h]);
                maku_set(&m, 0, 0x38, 1, 2);
                m.link[ORACLES_OBJ_ENABLED] = 1; m.link[ORACLES_OBJ_STATE] = 1; m.link[ORACLES_OBJ_XH] = 80; m.link[ORACLES_OBJ_YH] = 101;
                maku_tree_place(&m);
                m.tree[ORACLES_OBJ_ENABLED] = 0x81; m.tree[ORACLES_OBJ_STATE] = 1;
                OraclesEnhancedMode mode = ORACLES_ENHANCED_FRAMED;
                int32_t cam = 0, cam_y = 0, x = 0, y = 0, left = 0, top = 0;
                for (unsigned i = 0; i < 40u; i++) {
                    oracles_core_run_frame(m.r.core);
                    maku_camera(&m);
                    oracles_enhanced_view_compose(m.view, &mode, &cam, &cam_y);
                }
                CHECK(mode == ORACLES_ENHANCED_WORLD && oracles_enhanced_view_isolated(m.view));
                CHECK(m.view->band_top == (huds[h] == ORACLES_ENHANCED_HUD_FLOATING ? 0u : ORACLES_ENHANCED_HUD_HEIGHT));
                maku_window_place(&m, mode, cam, cam_y, &x, &y);
                OraclesEnhancedCompose in;
                memset(&in, 0, sizeof in);
                in.mode = ORACLES_ENHANCED_FRAMED;
                m.view->framed_load = 1; m.view->load_area_held = 0;
                CHECK(ev_load_band(m.view, &in, 0, cam, cam_y) == 1 && in.mode == ORACLES_ENHANCED_WORLD);
                CHECK(oracles_enhanced_view_load_band(m.view, &left, &top) == 1);
                if (left != x || top != y)
                    fprintf(stderr, "load band (hud %u, level %u, aspect %u): at %d,%d, play at %d,%d\n", (unsigned)huds[h], level, aspect, (int)left, (int)top, (int)x, (int)y);
                CHECK(left == x && top == y);
                CHECK(in.window_world_left == x && in.window_world_top == y - (int32_t)m.view->band_top);
                maku_stop(&m);
            }
}
#undef MW0

/* A dungeon's large room in the near band (256x128), the sub-pixel camera on: Link walking down at 0.71 of a pixel a
 * frame, the game's window following him a pixel at a time, the band takes his fraction with the window's whole
 * pixels, to the 256th of a pixel, never back nor by more than a pixel a frame, and stops on the window's last place
 * at the room's bottom without passing it; a scroll starting gives the fraction back by 16 a frame; without the
 * sub-pixel camera, none. */
static void dungeon_diagonal_band(void)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    OraclesEnhancedCamera *camera = r.guest ? oracles_enhanced_camera_start(r.guest) : NULL;
    CHECK(camera != NULL);
    if (!camera) { rig_stop(&r); return; }
    for (int exact = 1; exact >= 0; exact--) {
        oracles_enhanced_camera_set_band(camera, 256, 128);
        oracles_enhanced_camera_set_exact(camera, exact);
        OraclesEnhancedObservation ob;
        memset(&ob, 0, sizeof ob);
        ob.playing = 1; ob.large = 1; ob.large_grid = 1; ob.epoch = 8 + 4;
        int32_t y = 74 * 256, last = INT32_MIN, worst_back = 0, worst_step = 0, cam = 0, cam_y = 0, fx = 0, fy = 0;
        unsigned exact_frames = 0, fractions = 0;
        for (uint32_t frame = 0; frame < 140u; frame++) {
            if (frame < 120u) y += 0xb5;
            int camera_y = y / 256 - 64;
            if (camera_y < 0) camera_y = 0;
            if (camera_y > 48) camera_y = 48;
            ob.in_transition = ob.in_scroll = frame >= 130u;
            CHECK(oracles_enhanced_world_from_room(4, 0x10, 1, 120, y / 256, 16, 14, 0, 0, 0, 0, 0, 0x10, 256, &ob.world) == 1);
            ob.drawn_camera_x = 40; ob.drawn_camera_y = camera_y;
            ob.window_left = ob.world.origin_x + 40; ob.window_top = ob.world.origin_y + camera_y;
            ob.scroll_start_left = ob.window_left; ob.scroll_start_top = ob.window_top;
            ob.drawn_sub_x = 0; ob.drawn_sub_y = y & 0xff;
            CHECK(oracles_enhanced_camera_reduce(camera, frame, &ob, &cam, &cam_y) == 1);
            oracles_enhanced_camera_fraction(camera, &fx, &fy);
            if (fy) fractions++;
            const int32_t shown = cam_y * 256 + fy;
            if (frame < 120u && y / 256 - 64 < 48 && shown == ob.window_top * 256 + (y & 0xff)) exact_frames++;
            if (last != INT32_MIN && frame < 130u) {
                if (last - shown > worst_back) worst_back = last - shown;
                if (shown - last > worst_step) worst_step = shown - last;
            }
            if (frame >= 130u && last != INT32_MIN) CHECK(last - shown <= 16 && shown - last <= 16);
            if (frame == 129u) CHECK(shown == (ob.world.origin_y + 48) * 256);
            last = shown;
        }
        CHECK(fx == 0 && worst_back == 0 && worst_step <= 256);
        if (exact) CHECK(exact_frames >= 50u);
        else CHECK(fractions == 0u && exact_frames == 0u);
    }
    oracles_enhanced_camera_stop(camera);
    rig_stop(&r);
}

/* The same in near 4:3 (213x160), narrower and shorter than the room: Link walking down and right at 0.71 of a pixel
 * a frame each way, the band follows the game's window at its slack's ratio both ways, his fraction with it: never
 * back nor by more than a pixel a frame on either axis, and on the window's last place at the room's far corner. */
static void dungeon_diagonal_band_4_3(void)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    OraclesEnhancedCamera *camera = r.guest ? oracles_enhanced_camera_start(r.guest) : NULL;
    CHECK(camera != NULL);
    if (!camera) { rig_stop(&r); return; }
    oracles_enhanced_camera_set_band(camera, 213, 160);
    oracles_enhanced_camera_set_exact(camera, 1);
    OraclesEnhancedObservation ob;
    memset(&ob, 0, sizeof ob);
    ob.playing = 1; ob.large = 1; ob.large_grid = 1; ob.epoch = 8 + 4;
    int32_t x = 90 * 256, y = 74 * 256, last[2] = { INT32_MIN, INT32_MIN }, worst_back = 0, worst_step = 0, cam = 0, cam_y = 0, fx = 0, fy = 0;
    unsigned fractions = 0;
    for (uint32_t frame = 0; frame < 150u; frame++) {
        if (frame < 140u) { x += 0xb5; y += 0xb5; }
        int cx = x / 256 - 80, cy = y / 256 - 64;
        cx = cx < 0 ? 0 : cx > 80 ? 80 : cx;
        cy = cy < 0 ? 0 : cy > 48 ? 48 : cy;
        CHECK(oracles_enhanced_world_from_room(4, 0x10, 1, x / 256, y / 256, 16, 14, 0, 0, 0, 0, 0, 0x10, 213, &ob.world) == 1);
        ob.drawn_camera_x = cx; ob.drawn_camera_y = cy;
        ob.window_left = ob.world.origin_x + cx; ob.window_top = ob.world.origin_y + cy;
        ob.drawn_sub_x = x & 0xff; ob.drawn_sub_y = y & 0xff;
        CHECK(oracles_enhanced_camera_reduce(camera, frame, &ob, &cam, &cam_y) == 1);
        oracles_enhanced_camera_fraction(camera, &fx, &fy);
        if (fx && fy) fractions++;
        const int32_t shown[2] = { cam * 256 + fx, cam_y * 256 + fy };
        for (unsigned a = 0; a < 2u; a++) {
            if (last[a] != INT32_MIN) {
                if (last[a] - shown[a] > worst_back) worst_back = last[a] - shown[a];
                if (shown[a] - last[a] > worst_step) worst_step = shown[a] - last[a];
            }
            last[a] = shown[a];
        }
    }
    CHECK(worst_back == 0 && worst_step <= 256 && fractions >= 50u);
    CHECK(last[0] == (ob.world.origin_x + 27) * 256 && last[1] == (ob.world.origin_y + 16) * 256);
    oracles_enhanced_camera_stop(camera);
    rig_stop(&r);
}

/* Toward a room the view has no terrain for, the band's edge held where it stood: Link walking right a pixel a frame,
 * the band follows him framed without its look-ahead, never ahead of the band left free nor past him; once the room
 * is there it is the free band again within a second, never more than four pixels a frame. */
static void hold_before_dark(void)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    OraclesEnhancedCamera *held = r.guest ? oracles_enhanced_camera_start(r.guest) : NULL, *free_ = r.guest ? oracles_enhanced_camera_start(r.guest) : NULL;
    CHECK(held != NULL && free_ != NULL);
    if (!held || !free_) { if (held) oracles_enhanced_camera_stop(held); if (free_) oracles_enhanced_camera_stop(free_); rig_stop(&r); return; }
    OraclesEnhancedCamera *cameras[2] = { held, free_ };
    for (unsigned k = 0; k < 2u; k++) {
        oracles_enhanced_camera_set_profile(cameras[k], 2);
        oracles_enhanced_camera_set_band(cameras[k], W, BAND_H);
        oracles_enhanced_camera_set_exact(cameras[k], 1);
    }
    int32_t cam[2] = { 0, 0 }, cam_y = 0, last = 0, largest = 0, most = INT32_MAX, ahead_of_free = 0, past = INT32_MIN, apart = 0;
    int x = 10;
    for (unsigned f = 0; f < 320u; f++) {
        OraclesEnhancedObservation o;
        memset(&o, 0, sizeof o);
        o.playing = 1; o.grid = 1; o.epoch = 8;
        CHECK(oracles_enhanced_world_from_room(0, 0x45, 0, x, 60, 16, 14, 0, 0, 0, 0, 0, 0x45, W, &o.world) == 1);
        o.link_step_x = f >= 20u && f < 240u ? 1 : 0;
        if (f == 20u) most = last;
        const int32_t limits[4] = { INT32_MIN, f >= 20u && f < 120u ? most : INT32_MAX, INT32_MIN, INT32_MAX };
        oracles_enhanced_camera_limit(held, limits);
        for (unsigned k = 0; k < 2u; k++) CHECK(oracles_enhanced_camera_reduce(cameras[k], f, &o, &cam[k], &cam_y) == 1);
        if (f > 0u && (cam[0] - last > largest || last - cam[0] > largest)) largest = cam[0] > last ? cam[0] - last : last - cam[0];
        if (f >= 20u && f < 120u) {
            if (cam[0] > cam[1]) ahead_of_free++;
            if (cam[0] - (o.world.link_x - (int32_t)W / 2) > past) past = cam[0] - (o.world.link_x - (int32_t)W / 2);
        }
        if (f >= 180u && cam[0] != cam[1]) apart++;
        last = cam[0];
        if (f >= 20u && f < 240u) x++;
    }
    CHECK(ahead_of_free == 0);
    CHECK(past <= 1);
    CHECK(apart == 0);
    CHECK(largest <= 4);
    oracles_enhanced_camera_stop(held);
    oracles_enhanced_camera_stop(free_);
    rig_stop(&r);
}

/* Held short of a room still being made, toward either side of either axis (`axis` 0 across, 1 down; `way` 1 toward
 * the far bound, -1 toward the near one): Link standing, the camera held where it stands, then he walks that way and
 * stops, the room coming only once he stands.  In whole pixels the hold leaves the camera pixels short of the one never
 * held (oracles_enhanced_camera_apart says so, the hold keeping it there on that axis); to the 256th, a fraction of a
 * pixel short.  Once the room is there, the camera rejoins it, its moves never a pixel a frame more than the unheld
 * one's, and from then on is that camera to the 256th, Link walking back. */
static void rejoin_after_hold(int exact, int axis, int way)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    OraclesEnhancedCamera *held = r.guest ? oracles_enhanced_camera_start(r.guest) : NULL, *free_ = r.guest ? oracles_enhanced_camera_start(r.guest) : NULL;
    CHECK(held != NULL && free_ != NULL);
    if (!held || !free_) { if (held) oracles_enhanced_camera_stop(held); if (free_) oracles_enhanced_camera_stop(free_); rig_stop(&r); return; }
    OraclesEnhancedCamera *cameras[2] = { held, free_ };
    for (unsigned k = 0; k < 2u; k++) {
        oracles_enhanced_camera_set_profile(cameras[k], 2);
        oracles_enhanced_camera_set_band(cameras[k], W, BAND_H);
        oracles_enhanced_camera_set_exact(cameras[k], exact);
    }
    const unsigned stop = exact ? 30u : 90u, release = exact ? 60u : 150u;
    int64_t at[2] = { 0, 0 }, last[2] = { 0, 0 }, gap_at_release = 0, worst = 0;
    int32_t bound = 0, last_whole = 0;
    unsigned held_reported = 0, apart_after = 0, rejoined_at = 0;
    int place = way > 0 ? 30 : (axis ? 100 : 130), across = 60, down = 60;
    for (unsigned f = 0; f < 320u; f++) {
        OraclesEnhancedObservation o;
        memset(&o, 0, sizeof o);
        o.playing = 1; o.grid = 1; o.epoch = 8;
        if (axis) down = place; else across = place;
        CHECK(oracles_enhanced_world_from_room(0, 0x45, 0, across, down, 16, 14, 0, 0, 0, 0, 0, 0x45, W, &o.world) == 1);
        const int step = f >= 20u && f < stop ? way : f >= 250u && f < 270u ? -way : 0;
        if (axis) o.link_step_y = step; else o.link_step_x = step;
        if (f == 10u) bound = last_whole;
        int32_t limits[4] = { INT32_MIN, INT32_MAX, INT32_MIN, INT32_MAX };
        if (f >= 10u && f < release) limits[axis * 2 + (way > 0)] = bound;
        oracles_enhanced_camera_limit(held, limits);
        int32_t whole[2][2], fraction[2][2];
        for (unsigned k = 0; k < 2u; k++) {
            CHECK(oracles_enhanced_camera_reduce(cameras[k], f, &o, &whole[k][0], &whole[k][1]) == 1);
            oracles_enhanced_camera_fraction(cameras[k], &fraction[k][0], &fraction[k][1]);
            at[k] = (int64_t)whole[k][axis] * 256 + (exact ? fraction[k][axis] : 0);
        }
        int32_t apart[2] = { 0, 0 };
        int kept[2] = { 0, 0 };
        oracles_enhanced_camera_apart(held, &apart[0], &apart[1], &kept[0], &kept[1]);
        const int64_t gap = at[0] - at[1];
        if (!exact && f >= release - 50u && f < release) held_reported += kept[axis] && apart[axis] == whole[0][axis] - whole[1][axis] && !kept[!axis] && !apart[!axis];
        if (f == release - 1u) gap_at_release = gap < 0 ? -gap : gap;
        if (f >= release) {
            const int64_t more = (at[0] - last[0]) - (at[1] - last[1]);
            if ((more < 0 ? -more : more) > worst) worst = more < 0 ? -more : more;
            if (!rejoined_at && !gap) rejoined_at = f;
            if (rejoined_at && (gap || apart[0] || apart[1] || whole[0][!axis] != whole[1][!axis])) apart_after++;
        }
        last[0] = at[0]; last[1] = at[1];
        last_whole = whole[0][axis];
        place += step;
    }
    CHECK(gap_at_release >= (exact ? 1 : 2 * 256));
    CHECK(exact || held_reported == 50u);
    CHECK(rejoined_at >= release && rejoined_at <= release + 8u);
    CHECK(worst <= 256);
    CHECK(apart_after == 0u);
    oracles_enhanced_camera_stop(held);
    oracles_enhanced_camera_stop(free_);
    rig_stop(&r);
}

/* Link walked diagonally from rest into a hold of both axes (`both`) or of the horizontal one alone, in whole pixels,
 * then stands; the room comes, and the view draws him in phase with his diagonal (link_in_phase) from then on, or not
 * (`phase`).  Drawn in phase, a rejoining pixel of one axis alone, on a frame the camera does not move otherwise,
 * waits for the other's up to PAIRED_HOLD_FRAMES (two frames) before it goes; the two axes apart come together.  The
 * camera reaches the unheld one all the same, and is that camera from then on. */
static void rejoin_paired(int both, int phase, unsigned *first_move, unsigned *rejoined)
{
    rig r;
    memset(&r, 0, sizeof r);
    *first_move = *rejoined = 0;
    CHECK(rig_start(&r));
    OraclesEnhancedCamera *held = r.guest ? oracles_enhanced_camera_start(r.guest) : NULL, *free_ = r.guest ? oracles_enhanced_camera_start(r.guest) : NULL;
    CHECK(held != NULL && free_ != NULL);
    if (!held || !free_) { if (held) oracles_enhanced_camera_stop(held); if (free_) oracles_enhanced_camera_stop(free_); rig_stop(&r); return; }
    OraclesEnhancedCamera *cameras[2] = { held, free_ };
    for (unsigned k = 0; k < 2u; k++) {
        oracles_enhanced_camera_set_profile(cameras[k], 2);
        oracles_enhanced_camera_set_band(cameras[k], W, BAND_H);
        oracles_enhanced_camera_set_exact(cameras[k], 0);
    }
    const unsigned release = 150u;
    int32_t cam[2][2] = { { 0, 0 }, { 0, 0 } }, last[2][2] = { { 0, 0 }, { 0, 0 } }, bound[2] = { 0, 0 };
    int across = 30, down = 30;
    for (unsigned f = 0; f < 260u; f++) {
        OraclesEnhancedObservation o;
        memset(&o, 0, sizeof o);
        o.playing = 1; o.grid = 1; o.epoch = 8;
        CHECK(oracles_enhanced_world_from_room(0, 0x45, 0, across, down, 16, 14, 0, 0, 0, 0, 0, 0x45, W, &o.world) == 1);
        const int step = f >= 20u && f < 90u ? 1 : 0;
        o.link_step_x = step; o.link_step_y = step;
        o.link_in_phase = phase && f >= release;
        if (f == 10u) { bound[0] = cam[0][0]; bound[1] = cam[0][1]; }
        int32_t limits[4] = { INT32_MIN, INT32_MAX, INT32_MIN, INT32_MAX };
        if (f >= 10u && f < release) { limits[1] = bound[0]; if (both) limits[3] = bound[1]; }
        oracles_enhanced_camera_limit(held, limits);
        for (unsigned k = 0; k < 2u; k++) CHECK(oracles_enhanced_camera_reduce(cameras[k], f, &o, &cam[k][0], &cam[k][1]) == 1);
        if (f >= release) {
            if (!*first_move && (cam[0][0] != last[0][0] || cam[0][1] != last[0][1])) *first_move = f;
            if (!*rejoined && cam[0][0] == cam[1][0] && cam[0][1] == cam[1][1]) *rejoined = f;
            if (*rejoined) CHECK(cam[0][0] == cam[1][0] && cam[0][1] == cam[1][1]);
        }
        memcpy(last, cam, sizeof last);
        across += step; down += step;
    }
    oracles_enhanced_camera_stop(held);
    oracles_enhanced_camera_stop(free_);
    rig_stop(&r);
}

/* Held short of the room below while Link walks down a pixel a frame toward the map's bottom edge, a pixel or two behind
 * the camera never held, then let go (`release`) about when that camera stops at the edge: the camera rejoins it, a pixel
 * a frame on top of its own follow, and stops at the edge too, its follow never lost (the band framed for a frame, then
 * a snap back) nor its moves more than four pixels a frame. */
static void rejoin_at_edge(unsigned release, unsigned *lost, int32_t *largest)
{
    rig r;
    memset(&r, 0, sizeof r);
    *lost = 0; *largest = 0;
    CHECK(rig_start(&r));
    OraclesEnhancedCamera *held = r.guest ? oracles_enhanced_camera_start(r.guest) : NULL, *free_ = r.guest ? oracles_enhanced_camera_start(r.guest) : NULL;
    CHECK(held != NULL && free_ != NULL);
    if (!held || !free_) { if (held) oracles_enhanced_camera_stop(held); if (free_) oracles_enhanced_camera_stop(free_); rig_stop(&r); return; }
    OraclesEnhancedCamera *cameras[2] = { held, free_ };
    for (unsigned k = 0; k < 2u; k++) {
        oracles_enhanced_camera_set_profile(cameras[k], 2);
        oracles_enhanced_camera_set_band(cameras[k], W, BAND_H);
        oracles_enhanced_camera_set_exact(cameras[k], 0);
    }
    int32_t bound = 0, last = 0, at_edge = 0;
    int y = 640;
    for (unsigned f = 0; f < 320u; f++) {
        OraclesEnhancedObservation o;
        memset(&o, 0, sizeof o);
        o.playing = 1; o.grid = 1; o.epoch = 8;
        const int row = y / 128;
        CHECK(oracles_enhanced_world_from_room(0, (uint8_t)(row * 16 + 5), 0, 60, y - row * 128, 16, 14, 8, 0, 0, 0, 0, (uint8_t)(row * 16 + 5), W, &o.world) == 1);
        const int step = f >= 20u ? 1 : 0;
        o.link_step_y = step;
        if (f == 10u) bound = last;
        const int32_t limits[4] = { INT32_MIN, INT32_MAX, INT32_MIN, f >= 10u && f < release ? bound : INT32_MAX };
        oracles_enhanced_camera_limit(held, limits);
        int32_t cam[2][2];
        for (unsigned k = 0; k < 2u; k++) CHECK(oracles_enhanced_camera_reduce(cameras[k], f, &o, &cam[k][0], &cam[k][1]) == 1);
        if (!oracles_enhanced_camera_vertical_tracking(held)) (*lost)++;
        if (f > 0u && (cam[0][1] - last > *largest || last - cam[0][1] > *largest)) *largest = cam[0][1] > last ? cam[0][1] - last : last - cam[0][1];
        if (f == 319u) at_edge = cam[0][1] == cam[1][1] && cam[0][1] == 8 * 128 - (int32_t)BAND_H;
        last = cam[0][1];
        y += step;
    }
    CHECK(at_edge);
    oracles_enhanced_camera_stop(held);
    oracles_enhanced_camera_stop(free_);
    rig_stop(&r);
}

static void rejoin_at_map_edge(void)
{
    /* The camera never held reaches the edge at frame 278, Link 898 down. */
    for (unsigned release = 266u; release < 292u; release++) {
        unsigned lost = 0;
        int32_t largest = 0;
        rejoin_at_edge(release, &lost, &largest);
        CHECK(lost == 0u);
        CHECK(largest <= 4);
    }
}

static void rejoin_on_diagonal(void)
{
    unsigned first[2][2], joined[2][2];
    for (int both = 0; both < 2; both++)
        for (int phase = 0; phase < 2; phase++) rejoin_paired(both, phase, &first[both][phase], &joined[both][phase]);
    /* The horizontal axis alone apart, Link standing: at once out of phase, two frames later in phase. */
    CHECK(first[0][0] == 150u);
    CHECK(first[0][1] == 150u + 2u);   /* camera.c, PAIRED_HOLD_FRAMES */
    /* Both apart: they go together, as soon in phase as out of it. */
    CHECK(first[1][0] == 150u && first[1][1] == 150u);
    CHECK(joined[1][0] == joined[1][1]);
    for (int both = 0; both < 2; both++)
        for (int phase = 0; phase < 2; phase++) CHECK(joined[both][phase] >= 150u && joined[both][phase] <= 158u);
}

/* A camera state saved outdoors, loaded while the camera frames a house: it
 * resumes where it was, the reducers not restarted for the change of
 * viewport the load brings. */
static void state_across_viewports(void)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    OraclesEnhancedCamera *camera = r.guest ? oracles_enhanced_camera_start(r.guest) : NULL;
    CHECK(camera != NULL);
    if (!camera) { rig_stop(&r); return; }
    oracles_enhanced_camera_set_band(camera, W, BAND_H);
    OraclesEnhancedObservation out, in;
    memset(&out, 0, sizeof out);
    out.playing = 1; out.grid = 1; out.epoch = 8;
    in = out; in.epoch = 16 + 2;   /* a warp into the interiors' domain */
    int32_t cam = 0, cam_y = 0;
    uint32_t frame = 0;
    for (int x = 10; frame < 45u; frame++, x += 3) {   /* Link runs right: the camera trails him */
        CHECK(oracles_enhanced_world_from_room(0, 0x45, 0, x, 64, 16, 14, 0, 0, 0, 0, 0, 0x45, W, &out.world) == 1);
        oracles_enhanced_camera_reduce(camera, frame, &out, &cam, &cam_y);
    }
    const int32_t saved = cam, centred = out.world.link_x - 240;
    const OraclesE11State state = *oracles_enhanced_camera_state(camera), vertical = *oracles_enhanced_camera_state_vertical(camera);
    CHECK(saved - centred > 4 || centred - saved > 4);   /* a restart would bootstrap there, farther than a step of the reducer */
    CHECK(oracles_enhanced_world_from_room(2, 0x10, 0, 80, 64, 16, 14, 0, 0, 0, 0, 0, 0x10, W, &in.world) == 1);
    for (unsigned i = 0; i < 20u; i++, frame++) oracles_enhanced_camera_reduce(camera, frame, &in, &cam, &cam_y);
    oracles_enhanced_camera_set_state(camera, &state, &vertical);
    CHECK(oracles_enhanced_camera_reduce(camera, (uint32_t)state.last_ordinal + 1u, &out, &cam, &cam_y) == 1 && cam - saved <= 4 && saved - cam <= 4);
    oracles_enhanced_camera_stop(camera);
    rig_stop(&r);
}

/* Seasons' Lost Woods: a scroll up out of 0:40 that the game sends back into 0:40, against one into 0:30, the room
 * above.  The room re-entered keeps its place, a room lower than 0:30's: the world moves under Link by the room's
 * height at the arrival (rebase_y), the window the frame on screen shows with it, and the camera follows, a room lower
 * than through 0:30 on every frame from the arrival on and the same before, at both bands and both profiles.  A loop
 * out of the map's first column toward its side off the map moves nothing. */
static void loop_frame(OraclesGuest *guest, uint8_t room, int scrolling, int camera_y, int link_x, int link_y, uint8_t direction)
{
    const OraclesGuestTables *t = oracles_guest_tables(guest);
    uint8_t *w0 = oracles_guest_wram_writable(guest, 0), *link = oracles_guest_wram_writable(guest, 1) + (ORACLES_OBJECTS_BASE - 0xd000u);
    w0[t->game_state.addr - 0xc000u] = 2; w0[t->cutscene_index.addr - 0xc000u] = 0;
    w0[t->scroll_mode.addr - 0xc000u] = scrolling ? ORACLES_SCROLL_MODE_TRANSITION : 1u;
    w0[t->screen_transition_state.addr - 0xc000u] = scrolling ? 4u : 2u;
    w0[t->screen_transition_direction.addr - 0xc000u] = direction;
    w0[t->active_group.addr - 0xc000u] = 0; w0[t->active_room.addr - 0xc000u] = room;
    w0[t->keys_pressed.addr - 0xc000u] = 0x40u;
    uint8_t *cy = (uint8_t *)oracles_guest_ptr(guest, t->camera_y, 2), *cx = (uint8_t *)oracles_guest_ptr(guest, t->camera_x, 2);
    cy[0] = (uint8_t)camera_y; cy[1] = (uint8_t)((unsigned)camera_y >> 8); cx[0] = cx[1] = 0;
    link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_STATE] = 1;
    link[ORACLES_OBJ_XH] = (uint8_t)link_x; link[ORACLES_OBJ_YH] = (uint8_t)link_y;
}

static void small_room_loop(void)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start_game(&r, ORACLES_GAME_SEASONS));
    if (!r.guest) { rig_stop(&r); return; }
    for (unsigned band = 0; band < 2u; band++) {
        for (unsigned profile = 1; profile <= 2u; profile++) {
            OraclesEnhancedCamera *cameras[2] = { oracles_enhanced_camera_start(r.guest), oracles_enhanced_camera_start(r.guest) };
            OraclesEnhancedObserver observers[2];
            memset(observers, 0, sizeof observers);
            for (unsigned k = 0; k < 2u; k++) {
                observers[k].map_width = 16;
                if (band) oracles_enhanced_camera_set_band(cameras[k], W, BAND_H);
                oracles_enhanced_camera_set_profile(cameras[k], profile);
            }
            const uint8_t entered[2] = { 0x30, 0x40 };
            unsigned same_before = 0, lower_after = 0, windows = 0, rebased = 0;
            for (uint32_t f = 0; f < 140u; f++) {
                /* 40 frames walking up in 0:40, 32 of the scroll, the game's window up a room and Link 40 px on, then
                 * the walk up in the room entered. */
                const int scrolling = f >= 40u && f < 72u;
                const int camera_y = scrolling ? -4 * (int)(f - 39u) : 0;
                const int link_y = f < 40u ? 60 - (int)f : scrolling ? 20 - (int)(f - 39u) * 40 / 32 : 108 - (int)(f - 72u);
                OraclesEnhancedObservation ob[2];
                int32_t cam[2] = { 0, 0 }, cam_y[2] = { 0, 0 };
                for (unsigned k = 0; k < 2u; k++) {
                    loop_frame(r.guest, f < 40u ? 0x40 : entered[k], scrolling, camera_y, 80, link_y, 0);
                    oracles_enhanced_observe(r.guest, &observers[k], &ob[k]);
                    CHECK(oracles_enhanced_camera_reduce(cameras[k], f, &ob[k], &cam[k], &cam_y[k]) == 1);
                }
                if (ob[1].rebase_y) rebased++;
                CHECK(ob[0].rebase_x == 0 && ob[0].rebase_y == 0 && ob[1].rebase_x == 0);
                CHECK(ob[1].rebase_y == (f == 72u ? 128 : 0));
                if (f < 72u) { if (cam[1] == cam[0] && cam_y[1] == cam_y[0]) same_before++; }
                else if (cam[1] == cam[0] && cam_y[1] == cam_y[0] + 128) lower_after++;
                if (f >= 72u && ob[1].window_top == ob[0].window_top + 128 && ob[1].link_step_y == ob[0].link_step_y) windows++;
            }
            CHECK(rebased == 1u && same_before == 72u && lower_after == 68u && windows == 68u);
            for (unsigned k = 0; k < 2u; k++) oracles_enhanced_camera_stop(cameras[k]);
        }
    }
    /* Left out of 0:40, the map's first column, and back: off the map, nothing moves; out of 0:41, a room across. */
    for (unsigned k = 0; k < 2u; k++) {
        const uint8_t room = k ? 0x41 : 0x40;
        OraclesEnhancedObserver observer;
        memset(&observer, 0, sizeof observer);
        observer.map_width = 16;
        OraclesEnhancedObservation ob;
        int moved = 0;
        for (uint32_t f = 0; f < 50u; f++) {
            const int scrolling = f >= 10u && f < 42u;
            loop_frame(r.guest, room, scrolling, 0, f < 10u ? 20 - (int)f : scrolling ? 10 - (int)(f - 9u) * 20 / 32 : 150, 64, 3);
            oracles_enhanced_observe(r.guest, &observer, &ob);
            if (ob.rebase_x || ob.rebase_y) moved = ob.rebase_x;
        }
        CHECK(moved == (k ? 160 : 0));
    }
    rig_stop(&r);
}

/* A walk at the game's sub-pixel speeds: whole pixels on the frames the
 * sub-pixel carries over.  The frame on screen draws Link where the previous
 * observation had him (link_step), and the camera steps with him there: once
 * it follows him, his sprite on the screen never goes a pixel one way and
 * back the next frame on either axis, and the camera stays within a pixel of
 * the reducer's position, three where it turns back or he stands.  A state saved on the way, its whole offsets with
 * it, shows the next frame to the pixel the walk showed; without them (a
 * state saved before them), within a pixel. */
static void gait_walk(unsigned speed_x, unsigned speed_y)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    OraclesEnhancedCamera *camera = r.guest ? oracles_enhanced_camera_start(r.guest) : NULL;
    CHECK(camera != NULL);
    if (!camera) { rig_stop(&r); return; }
    oracles_enhanced_camera_set_profile(camera, 2);
    oracles_enhanced_camera_set_band(camera, W, BAND_H);
    OraclesEnhancedObservation walk[120];
    memset(walk, 0, sizeof walk);
    unsigned sub_x = 0, sub_y = 0;
    int x = 40, y = 30;
    for (unsigned f = 0; f < 120u; f++) {
        walk[f].playing = 1; walk[f].grid = 1; walk[f].epoch = 8;
        CHECK(oracles_enhanced_world_from_room(0, 0x45, 0, x, y, 16, 14, 0, 0, 0, 0, 0, 0x45, W, &walk[f].world) == 1);
        if (f > 0u) { walk[f].link_step_x = walk[f].world.link_x - walk[f - 1].world.link_x; walk[f].link_step_y = walk[f].world.link_y - walk[f - 1].world.link_y; }
        if (f >= 10u) { sub_x += speed_x; sub_y += speed_y; x += (int)(sub_x >> 8); y += (int)(sub_y >> 8); sub_x &= 0xffu; sub_y &= 0xffu; }
    }
    int32_t cam[120][2], last_step[2] = { 0, 0 };
    unsigned back_and_forth = 0, off_smooth = 0;
    OraclesE11State state, vertical;
    OraclesEnhancedShownOffset across, down;
    for (unsigned f = 0; f < 120u; f++) {
        CHECK(oracles_enhanced_camera_reduce(camera, f, &walk[f], &cam[f][0], &cam[f][1]) == 1);
        const int64_t gap = (int64_t)cam[f][0] * 256 - oracles_enhanced_camera_state(camera)->camera_pos;
        if (gap >= 768 || gap <= -768) off_smooth++;
        if (f > 0u)
            for (unsigned a = 0; a < 2u; a++) {
                const int32_t drawn = a ? walk[f].world.link_y - walk[f].link_step_y : walk[f].world.link_x - walk[f].link_step_x;
                const int32_t before = a ? walk[f - 1].world.link_y - walk[f - 1].link_step_y : walk[f - 1].world.link_x - walk[f - 1].link_step_x;
                const int32_t step = (drawn - cam[f][a]) - (before - cam[f - 1][a]);
                if (f >= 50u && step && step == -last_step[a]) back_and_forth++;
                last_step[a] = step;
            }
        if (f == 80u) {
            state = *oracles_enhanced_camera_state(camera); vertical = *oracles_enhanced_camera_state_vertical(camera);
            oracles_enhanced_camera_shown_offsets(camera, &across, &down);
        }
    }
    CHECK(walk[119].world.link_x - walk[50].world.link_x + walk[119].world.link_y - walk[50].world.link_y > 25);   /* he walked */
    CHECK(back_and_forth == 0);
    CHECK(off_smooth == 0);
    int32_t again = 0, again_y = 0;
    oracles_enhanced_camera_set_state(camera, &state, &vertical);
    oracles_enhanced_camera_set_shown_offsets(camera, &across, &down);
    CHECK(oracles_enhanced_camera_reduce(camera, 81u, &walk[81], &again, &again_y) == 1 && again == cam[81][0] && again_y == cam[81][1]);
    oracles_enhanced_camera_set_state(camera, &state, &vertical);
    CHECK(oracles_enhanced_camera_reduce(camera, 81u, &walk[81], &again, &again_y) == 1);
    CHECK(again - cam[81][0] <= 1 && cam[81][0] - again <= 1 && again_y - cam[81][1] <= 1 && cam[81][1] - again_y <= 1);
    /* Loaded from a savestate, its whole offsets with it but not Link's last place, then switched to the sub-pixel
     * camera before the next frame: the camera takes the reducer's place, within a pixel of the walk's. */
    OraclesEnhancedShownOffset loaded_across = across, loaded_down = down;
    loaded_across.last_link = INT32_MIN; loaded_down.last_link = INT32_MIN;
    oracles_enhanced_camera_set_state(camera, &state, &vertical);
    oracles_enhanced_camera_set_shown_offsets(camera, &loaded_across, &loaded_down);
    oracles_enhanced_camera_set_exact(camera, 1);
    CHECK(oracles_enhanced_camera_reduce(camera, 81u, &walk[81], &again, &again_y) == 1);
    CHECK(again - cam[81][0] <= 1 && cam[81][0] - again <= 1 && again_y - cam[81][1] <= 1 && cam[81][1] - again_y <= 1);
    oracles_enhanced_camera_set_exact(camera, 0);
    oracles_enhanced_camera_stop(camera);
    rig_stop(&r);
}

/* The camera in whole pixels with the follow that settles: Link walks right diagonally (0xb5/256 a frame, steps of
 * 1, 1, 0), stands, then walks left a pixel a frame.  His sprite on the screen never goes a pixel one way and back
 * the next frame, as he stops, stands or turns, and the camera never moves more than two pixels a frame. */
static void pixel_stop_turn(void)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    OraclesEnhancedCamera *camera = r.guest ? oracles_enhanced_camera_start(r.guest) : NULL;
    CHECK(camera != NULL);
    if (!camera) { rig_stop(&r); return; }
    oracles_enhanced_camera_set_profile(camera, 2);
    oracles_enhanced_camera_set_band(camera, W, BAND_H);
    enum { FRAMES = 200 };
    OraclesEnhancedObservation walk[FRAMES];
    memset(walk, 0, sizeof walk);
    unsigned sub = 0;
    int x = 60;
    for (unsigned f = 0; f < FRAMES; f++) {
        walk[f].playing = 1; walk[f].grid = 1; walk[f].epoch = 8;
        CHECK(oracles_enhanced_world_from_room(0, 0x45, 0, x, 60, 16, 14, 0, 0, 0, 0, 0, 0x45, W, &walk[f].world) == 1);
        if (f > 0u) walk[f].link_step_x = walk[f].world.link_x - walk[f - 1].world.link_x;
        if (f >= 10u && f < 110u) { sub += 0xb5u; x += (int)(sub >> 8); sub &= 0xffu; }
        else if (f >= 140u) x--;
    }
    int32_t cam = 0, cam_y = 0, last_cam = 0, last_screen = 0, last_move = 0, largest = 0;
    unsigned back = 0;
    for (unsigned f = 0; f < FRAMES; f++) {
        CHECK(oracles_enhanced_camera_reduce(camera, f, &walk[f], &cam, &cam_y) == 1);
        const int32_t drawn = walk[f].world.link_x - walk[f].link_step_x, screen = drawn - cam;
        if (f > 0u) {
            const int32_t move = screen - last_screen;
            if (f >= 30u && move && last_move && (move > 0) != (last_move > 0)) back++;
            last_move = move;
            if (f >= 30u && (cam - last_cam > largest || last_cam - cam > largest)) largest = cam > last_cam ? cam - last_cam : last_cam - cam;
        }
        last_cam = cam; last_screen = screen;
    }
    CHECK(back == 0);
    CHECK(largest <= 2);
    if (back || largest > 2) fprintf(stderr, "  whole pixels: %u steps back on the screen, the camera by %d at most\n", back, largest);
    oracles_enhanced_camera_stop(camera);
    rig_stop(&r);
}

/* The first profile with the sub-pixel camera switched on and off during a diagonal walk, as Display's Camera from
 * the pause: it stays the first profile, its state bound to its own configuration, and the camera moves no more than
 * two pixels a frame. */
static void first_profile_switch(void)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    OraclesEnhancedCamera *camera = r.guest ? oracles_enhanced_camera_start(r.guest) : NULL;
    CHECK(camera != NULL);
    if (!camera) { rig_stop(&r); return; }
    oracles_enhanced_camera_set_profile(camera, 1);
    oracles_enhanced_camera_set_band(camera, W, BAND_H);
    OraclesE11Config first;
    oracles_e11_config_profile(&first, 1);
    unsigned sub = 0, last_sub = 0;
    int x = 40;
    int32_t cam = 0, cam_y = 0, last_cam = 0, largest = 0;
    OraclesEnhancedObservation o, before;
    memset(&before, 0, sizeof before);
    for (unsigned f = 0; f < 160u; f++) {
        memset(&o, 0, sizeof o);
        o.playing = 1; o.grid = 1; o.epoch = 8;
        CHECK(oracles_enhanced_world_from_room(0, 0x45, 0, x, 60, 16, 14, 0, 0, 0, 0, 0, 0x45, W, &o.world) == 1);
        if (f > 0u) o.link_step_x = o.world.link_x - before.world.link_x;
        o.link_sub_x = (int)sub; o.drawn_sub_x = (int)last_sub;
        if (f == 50u || f == 110u) oracles_enhanced_camera_set_exact(camera, 1);
        if (f == 80u) oracles_enhanced_camera_set_exact(camera, 0);
        CHECK(oracles_enhanced_camera_reduce(camera, f, &o, &cam, &cam_y) == 1);
        if (f > 20u && (cam - last_cam > largest || last_cam - cam > largest)) largest = cam > last_cam ? cam - last_cam : last_cam - cam;
        last_cam = cam; before = o; last_sub = sub;
        if (f >= 10u) { sub += 0xb5u; x += (int)(sub >> 8); sub &= 0xffu; }
    }
    CHECK(oracles_enhanced_camera_profile(camera) == 1);
    CHECK(strcmp(oracles_enhanced_camera_state(camera)->config_digest, first.digest) == 0);
    CHECK(largest <= 2);
    oracles_enhanced_camera_stop(camera);
    rig_stop(&r);
}

/* Diagonally, 0xb5/256 of a pixel a frame each way (SPEED_100 at 45
 * degrees); on stairs, half a pixel a frame (SPEED_080), the 1, 0 cadence. */
static void diagonal_walk(void) { gait_walk(0xb5u, 0xb5u); }
static void stairs_walk(void) { gait_walk(0u, 0x80u); }

/* The scenes the game plays in its own rooms (Ages' intro and cutscenes),
 * at both sizes: a scene in the world stays in the band, the pregame intro,
 * drawn on a screen the game cleared, is framed; Link taken away by a
 * scene holds the camera on his last place, or frames the rooms he has none in. */
static void cutscenes(void)
{
    for (int drawn_back = 0; drawn_back < 2; drawn_back++) {
        rig r;
        memset(&r, 0, sizeof r);
        CHECK(rig_start(&r));
        OraclesEnhancedView *view = r.guest ? oracles_enhanced_view_start(r.core, r.guest, NULL, 0) : NULL;
        CHECK(view != NULL);
        if (!view) { rig_stop(&r); return; }
        oracles_enhanced_view_set_zoom_out(view, drawn_back);
        const OraclesGuestTables *t = &oracles_guest_tables_ages;
        uint8_t *w0 = oracles_guest_wram_writable(r.guest, 0), *w1 = oracles_guest_wram_writable(r.guest, 1);
        uint8_t *link = w1 + (ORACLES_OBJECTS_BASE - 0xd000u);
        #define W0(sym) w0[(sym).addr - 0xc000u]
        W0(t->game_state) = 2; W0(t->cutscene_index) = 1; W0(t->scroll_mode) = 1; W0(t->screen_transition_state) = 2;
        W0(t->active_group) = 0; W0(t->active_room) = 0x45; W0(t->room_is_large) = 0;
        link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_STATE] = 1; link[ORACLES_OBJ_XH] = 80; link[ORACLES_OBJ_YH] = 64;
        OraclesEnhancedMode mode = ORACLES_ENHANCED_FRAMED;
        int32_t cam = 0, cam_y = 0;
        frames(&r, view, 40, &mode, &cam, &cam_y);
        CHECK(mode == ORACLES_ENHANCED_WORLD);
        const int32_t cam0 = cam, cam_y0 = cam_y;

        /* The pregame intro (CUTSCENE_PREGAME_INTRO): its orb over a cleared
         * screen, the game's scroll stopped (wScrollMode 0), in the room loaded. */
        W0(t->cutscene_index) = t->cutscene_pregame_intro; W0(t->scroll_mode) = 0;
        unsigned world = 0;
        for (unsigned i = 0; i < 10u; i++) { frames(&r, view, 1, &mode, &cam, &cam_y); world += mode == ORACLES_ENHANCED_WORLD; }
        CHECK(world == 0);
        W0(t->cutscene_index) = 1; W0(t->scroll_mode) = 1;
        frames(&r, view, 5, &mode, &cam, &cam_y);
        CHECK(mode == ORACLES_ENHANCED_WORLD && cam == cam0 && cam_y == cam_y0);

        /* A scene played in the room itself (Nayru's song after its picture, CUTSCENE_NAYRU_SINGING): the band. */
        W0(t->cutscene_index) = 6;
        for (unsigned i = 0; i < 10u; i++) { frames(&r, view, 1, &mode, &cam, &cam_y); world += mode == ORACLES_ENHANCED_WORLD; }
        CHECK(world == 10u && cam == cam0 && cam_y == cam_y0);

        /* The song takes Link away: his object cleared (enabled 0, at the
         * room's corner), first in his room (the time portal does the same),
         * where the camera holds on his last place... */
        link[ORACLES_OBJ_ENABLED] = 0; link[ORACLES_OBJ_XH] = 0; link[ORACLES_OBJ_YH] = 0;
        unsigned moved = 0;
        for (unsigned i = 0; i < 60u; i++) { frames(&r, view, 1, &mode, &cam, &cam_y); moved += mode != ORACLES_ENHANCED_WORLD || cam != cam0 || cam_y != cam_y0; }
        CHECK(moved == 0);
        /* ... then in the rooms the song shows (0:98), where he has no place:
         * framed, never the band centred on the corner ... */
        W0(t->active_room) = 0x98;
        world = 0;
        for (unsigned i = 0; i < 30u; i++) { frames(&r, view, 1, &mode, &cam, &cam_y); world += mode == ORACLES_ENHANCED_WORLD; }
        CHECK(world == 0);
        /* ... and back in his room, the band on his last place, where he appears. */
        W0(t->active_room) = 0x45;
        for (unsigned i = 0; i < 30u; i++) { frames(&r, view, 1, &mode, &cam, &cam_y); moved += mode != ORACLES_ENHANCED_WORLD || cam != cam0 || cam_y != cam_y0; }
        link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_XH] = 80; link[ORACLES_OBJ_YH] = 64;
        for (unsigned i = 0; i < 30u; i++) { frames(&r, view, 1, &mode, &cam, &cam_y); moved += mode != ORACLES_ENHANCED_WORLD || cam != cam0 || cam_y != cam_y0; }
        CHECK(moved == 0);

        /* Out of play (a file loading, wGameState 3) the game sets Link's
         * place while his object is still disabled: that place is read, and
         * back in play, Link warping in, it is the one that holds, so that
         * the observed Link does not jump when he steps out of the warp. */
        W0(t->game_state) = 3;
        link[ORACLES_OBJ_ENABLED] = 0; link[ORACLES_OBJ_XH] = 24; link[ORACLES_OBJ_YH] = 24;
        frames(&r, view, 10, &mode, &cam, &cam_y);
        W0(t->game_state) = 2; link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_STATE] = 0x0a;   /* LINK_STATE_WARPING */
        const OraclesEnhancedObservation *ob = oracles_enhanced_view_observation(view);
        int32_t last_x = 0, last_y = 0, jump = 0;
        for (unsigned i = 0; i < 20u; i++) {
            if (i == 5u) link[ORACLES_OBJ_STATE] = 1;
            frames(&r, view, 1, &mode, &cam, &cam_y);
            const int32_t dx = ob->world.link_x - last_x, dy = ob->world.link_y - last_y;
            if (i > 0u && (dx > 8 || dx < -8 || dy > 8 || dy < -8)) jump++;
            last_x = ob->world.link_x; last_y = ob->world.link_y;
        }
        CHECK(jump == 0 && last_x == 5 * 160 + 24 && last_y == 4 * 128 + 24);
        #undef W0
        oracles_enhanced_view_stop(view);
        rig_stop(&r);
    }
}

/* Seasons' intro, at both sizes: Din's dance is played in the room and stays
 * in the band; Din taken to Onox's castle (the tower rising under a dark sky),
 * the temple sinking and the pregame intro draw their own screens and are
 * framed.  The indices are Seasons' alone: Ages has none of them. */
static void seasons_scenes(void)
{
    CHECK(oracles_guest_tables_ages.cutscene_din_imprisoned == 0xffu && oracles_guest_tables_ages.cutscene_temple_sinking == 0xffu && oracles_guest_tables_ages.cutscene_onox_taunting == 0xffu);
    for (int drawn_back = 0; drawn_back < 2; drawn_back++) {
        rig r;
        memset(&r, 0, sizeof r);
        CHECK(rig_start_game(&r, ORACLES_GAME_SEASONS));
        OraclesEnhancedView *view = r.guest ? oracles_enhanced_view_start(r.core, r.guest, NULL, 0) : NULL;
        CHECK(view != NULL);
        if (!view) { rig_stop(&r); return; }
        oracles_enhanced_view_set_zoom_out(view, drawn_back);
        const OraclesGuestTables *t = &oracles_guest_tables_seasons;
        uint8_t *w0 = oracles_guest_wram_writable(r.guest, 0), *w1 = oracles_guest_wram_writable(r.guest, 1);
        uint8_t *link = w1 + (ORACLES_OBJECTS_BASE - 0xd000u);
        #define W0(sym) w0[(sym).addr - 0xc000u]
        W0(t->game_state) = 2; W0(t->cutscene_index) = 1; W0(t->scroll_mode) = 1; W0(t->screen_transition_state) = 2;
        W0(t->active_group) = 0; W0(t->active_room) = 0x98; W0(t->room_is_large) = 0;
        link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_STATE] = 1; link[ORACLES_OBJ_XH] = 80; link[ORACLES_OBJ_YH] = 64;
        OraclesEnhancedMode mode = ORACLES_ENHANCED_FRAMED;
        int32_t cam = 0, cam_y = 0;
        frames(&r, view, 40, &mode, &cam, &cam_y);
        CHECK(mode == ORACLES_ENHANCED_WORLD);
        const uint8_t scenes[4] = { t->cutscene_din_imprisoned, t->cutscene_temple_sinking, t->cutscene_pregame_intro, t->cutscene_onox_taunting };
        W0(t->cutscene_index) = 6;   /* CUTSCENE_S_DIN_DANCING */
        unsigned world = 0;
        for (unsigned i = 0; i < 10u; i++) { frames(&r, view, 1, &mode, &cam, &cam_y); world += mode == ORACLES_ENHANCED_WORLD; }
        CHECK(world == 10u);
        for (unsigned k = 0; k < 4u; k++) {
            W0(t->cutscene_index) = scenes[k];   /* the game keeps wScrollMode 1 through most of them */
            world = 0;
            for (unsigned i = 0; i < 10u; i++) { frames(&r, view, 1, &mode, &cam, &cam_y); world += mode == ORACLES_ENHANCED_WORLD; }
            CHECK(world == 0);
        }
        W0(t->cutscene_index) = 1;
        frames(&r, view, 5, &mode, &cam, &cam_y);
        CHECK(mode == ORACLES_ENHANCED_WORLD);
        #undef W0
        oracles_enhanced_view_stop(view);
        rig_stop(&r);
    }
}

/* The cutscenes that show full-screen pictures between rooms (the black
 * tower's explanations, the endings, the credits, the flames lit in the room
 * of rites), on each game that has them, at both sizes: framed while the
 * status bar is off the screen (wGfxRegs1 splitting it nowhere, or on its
 * first line), in the band while the bar shows (Ambi's room after the
 * tower), the screen variables cleared as clearScreenVariablesAndWramBank1
 * leaves them, which the observation takes for a load in the room it has.
 * A warp's fade-out (cutscene03 to cutscene05) is framed the same way while
 * the LCD is on: from a picture, not from play; the LCD off, its blank is a
 * load's, at the view's scale, but after a frame the large menus enlarge.
 * The scenes played in the room itself stay in the band, the bar shown or
 * not.  The core writes LCDC from RIG_LCDC (xor $91: 0 leaves it on). */
#define RIG_LCDC 0xc0feu
static const uint8_t rig_lcdc_code[] = {
    0xfa, 0xfe, 0xc0,   /* ld a,(RIG_LCDC) */
    0xee, 0x91,         /* xor $91 */
    0x47,               /* ld b,a */
    0xf0, 0x40,         /* ldh a,(LCDC) */
    0xb8,               /* cp b */
    0x28, 0xf5,         /* jr z,$0150 */
    0x78,               /* ld a,b */
    0xe0, 0x40,         /* ldh (LCDC),a */
    0x18, 0xf0,         /* jr $0150 */
};

static unsigned scene_world_frames(rig *r, OraclesEnhancedView *view, const OraclesGuestTables *t, uint8_t cutscene, uint8_t lyc, int cleared, int lcd_on, unsigned *enlarged)
{
    uint8_t *w0 = oracles_guest_wram_writable(r->guest, 0);
    #define W0(sym) w0[(sym).addr - 0xc000u]
    W0(t->cutscene_index) = cutscene; W0(t->scroll_mode) = cleared ? 0 : 1; W0(t->screen_transition_state) = cleared ? 0 : 2;
    w0[t->gfx_regs1.addr + 5u - 0xc000u] = lyc;
    w0[RIG_LCDC - 0xc000u] = lcd_on ? 0x00 : 0x80;   /* LCDC $91, or $11 */
    OraclesEnhancedMode mode = ORACLES_ENHANCED_FRAMED;
    int32_t cam = 0, cam_y = 0;
    uint32_t rect[4];
    unsigned world = 0;
    for (unsigned i = 0; i < 10u; i++) {
        frames(r, view, 1, &mode, &cam, &cam_y);
        world += mode == ORACLES_ENHANCED_WORLD;
        if (enlarged) *enlarged += (unsigned)oracles_enhanced_view_framed_crop(view, rect);
    }
    W0(t->cutscene_index) = 1; W0(t->scroll_mode) = 1; W0(t->screen_transition_state) = 2;
    w0[t->gfx_regs1.addr + 5u - 0xc000u] = 0x0f;
    w0[RIG_LCDC - 0xc000u] = 0x00;
    frames(r, view, 5, &mode, &cam, &cam_y);
    CHECK(mode == ORACLES_ENHANCED_WORLD && !oracles_enhanced_view_framed_crop(view, rect));
    #undef W0
    return world;
}

static void picture_scenes_of(const OraclesCompatProfile *profile, const uint8_t *pictures, unsigned n_pictures, const uint8_t *in_room, unsigned n_in_room)
{
    for (int drawn_back = 0; drawn_back < 2; drawn_back++) {
        rig r;
        memset(&r, 0, sizeof r);
        CHECK(rig_start_code(&r, profile, rig_lcdc_code, sizeof rig_lcdc_code));
        OraclesEnhancedView *view = r.guest ? oracles_enhanced_view_start(r.core, r.guest, NULL, 0) : NULL;
        CHECK(view != NULL);
        if (!view) { rig_stop(&r); return; }
        oracles_enhanced_view_set_zoom_out(view, drawn_back);
        const OraclesGuestTables *t = oracles_compat_guest_tables(profile);
        uint8_t *w0 = oracles_guest_wram_writable(r.guest, 0), *w1 = oracles_guest_wram_writable(r.guest, 1);
        uint8_t *link = w1 + (ORACLES_OBJECTS_BASE - 0xd000u);
        #define W0(sym) w0[(sym).addr - 0xc000u]
        /* The boot ROM's logo first: the code runs once LCDC holds its $11. */
        w0[RIG_LCDC - 0xc000u] = 0x80;
        unsigned boot = 0;
        while (boot < 600u && oracles_guest_io(r.guest)[0x40] != 0x11u) { oracles_core_run_frame(r.core); boot++; }
        CHECK(boot < 600u);
        w0[RIG_LCDC - 0xc000u] = 0x00;
        W0(t->game_state) = 2; W0(t->cutscene_index) = 1; W0(t->scroll_mode) = 1; W0(t->screen_transition_state) = 2;
        W0(t->active_group) = 0; W0(t->active_room) = 0x86; W0(t->room_is_large) = 0;
        w0[t->gfx_regs1.addr + 5u - 0xc000u] = 0x0f;   /* gfxRegisterStates 2: the bar above the game area */
        link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_STATE] = 1; link[ORACLES_OBJ_XH] = 80; link[ORACLES_OBJ_YH] = 64;
        OraclesEnhancedMode mode = ORACLES_ENHANCED_FRAMED;
        int32_t cam = 0, cam_y = 0;
        frames(&r, view, 40, &mode, &cam, &cam_y);
        CHECK(mode == ORACLES_ENHANCED_WORLD);
        CHECK((oracles_guest_io(r.guest)[0x40] & 0x80u) != 0);
        uint8_t still = 0xffu;
        for (unsigned k = 0; k < n_pictures; k++) {
            CHECK(pictures[k] != 0xffu);
            const int fade = pictures[k] == t->cutscene_03 || pictures[k] == t->cutscene_04 || pictures[k] == t->cutscene_05;
            if (!fade && still == 0xffu) still = pictures[k];
            unsigned enlarged = 0;
            CHECK(scene_world_frames(&r, view, t, pictures[k], 0x0f, 0, 1, NULL) == 10u);
            CHECK(scene_world_frames(&r, view, t, pictures[k], 0x0f, 1, 1, NULL) == 10u);
            CHECK(scene_world_frames(&r, view, t, pictures[k], 0xc7, 1, 1, &enlarged) == 0 && enlarged == 10u);
            CHECK(scene_world_frames(&r, view, t, pictures[k], 0xc7, 0, 1, NULL) == 0);
            CHECK(scene_world_frames(&r, view, t, pictures[k], 0x00, 1, 1, NULL) == 0);
            /* The LCD off: a fade's blank is not the picture's, in the band while the screen variables say play, a
             * load's at the view's scale once cleared; a picture's own blank is framed as the picture. */
            enlarged = 0;
            CHECK(scene_world_frames(&r, view, t, pictures[k], 0xc7, 0, 0, NULL) == (fade ? 10u : 0u));
            CHECK(scene_world_frames(&r, view, t, pictures[k], 0xc7, 1, 0, &enlarged) == 0 && enlarged == (fade ? 0u : 10u));
            CHECK(scene_world_frames(&r, view, t, pictures[k], 0x00, 1, 0, NULL) == 0);
        }
        /* A picture, its fade to white under cutscene03, then the LCD off for the warp's reload: one scale throughout,
         * the large menus', until the room shows in the band. */
        CHECK(still != 0xffu);
        W0(t->cutscene_index) = still;
        w0[t->gfx_regs1.addr + 5u - 0xc000u] = 0xc7;
        unsigned enlarged = 0, world = 0;
        uint32_t rect[4];
        for (unsigned i = 0; i < 24u; i++) {
            if (i == 8u) W0(t->cutscene_index) = t->cutscene_03;
            if (i == 16u) { W0(t->scroll_mode) = 0; W0(t->screen_transition_state) = 0; w0[RIG_LCDC - 0xc000u] = 0x80; }
            frames(&r, view, 1, &mode, &cam, &cam_y);
            world += mode == ORACLES_ENHANCED_WORLD;
            enlarged += (unsigned)oracles_enhanced_view_framed_crop(view, rect);
        }
        CHECK(world == 0 && enlarged == 24u);
        W0(t->cutscene_index) = 1; W0(t->scroll_mode) = 1; W0(t->screen_transition_state) = 2;
        w0[t->gfx_regs1.addr + 5u - 0xc000u] = 0x0f;
        w0[RIG_LCDC - 0xc000u] = 0x00;
        frames(&r, view, 5, &mode, &cam, &cam_y);
        CHECK(mode == ORACLES_ENHANCED_WORLD && !oracles_enhanced_view_framed_crop(view, rect));
        #undef W0
        for (unsigned k = 0; k < n_in_room; k++) {
            CHECK(scene_world_frames(&r, view, t, in_room[k], 0x0f, 0, 1, NULL) == 10u);
            CHECK(scene_world_frames(&r, view, t, in_room[k], 0xc7, 1, 1, NULL) == 10u);
            CHECK(scene_world_frames(&r, view, t, in_room[k], 0x00, 0, 1, NULL) == 10u);
        }
        oracles_enhanced_view_stop(view);
        rig_stop(&r);
    }
}

static void picture_scenes(void)
{
    /* Ages and the fan games on its engine: the explanations of the black tower, Veran beaten, the credits, the room of
     * rites collapsing, the flames of sorrow and despair, the tower complete; in the room: a wall retracting, the Wing
     * Dungeon collapsing, Jabu-Jabu opening, Ambi's passage, the seas cleaned, the fairies hiding, the palace's guard,
     * the people turned to stone, the princess taken, the pirates' ship (Seasons) in its window.  Nayru's song is
     * neither: its picture (GFXH_NAYRU_SINGING_CUTSCENE under registers 4, SCY 0 and SCX panning, where the camera
     * says SCY -16) is drawn off the camera, which frames it in the game and not on a core that writes no scroll
     * register (ages/intro.route holds it), then the room with its bar. */
    const OraclesGuestTables *a = &oracles_guest_tables_ages, *s = &oracles_guest_tables_seasons;
    CHECK(a->cutscene_black_tower_explanation == 0x08u && a->cutscene_black_tower_escape == 0x09u && a->cutscene_credits == 0x0au
          && a->cutscene_room_of_rites_collapse == 0x0fu && a->cutscene_flame_of_sorrow == 0x11u && a->cutscene_flame_of_despair == 0x20u
          && a->cutscene_black_tower_complete == 0x21u && a->cutscene_din_crystal_descending == 0xffu && a->cutscene_flame_of_destruction == 0xffu
          && a->cutscene_princess_kidnapped == 0xffu && a->cutscene_the_end_screen == 0xffu);
    CHECK(s->cutscene_din_crystal_descending == 0x09u && s->cutscene_credits == 0x0au && s->cutscene_room_of_rites_collapse == 0x0fu
          && s->cutscene_flame_of_destruction == 0x10u && s->cutscene_princess_kidnapped == 0x12u && s->cutscene_black_tower_explanation == 0xffu
          && s->cutscene_black_tower_escape == 0xffu && s->cutscene_flame_of_sorrow == 0xffu && s->cutscene_flame_of_despair == 0xffu
          && s->cutscene_black_tower_complete == 0xffu && s->cutscene_the_end_screen == 0xffu);
    const uint8_t ages_pictures[] = { 0x03, 0x04, 0x05, 0x08, 0x09, 0x0a, 0x0f, 0x11, 0x20, 0x21 };
    const uint8_t ages_in_room[] = { 0x0b, 0x1a, 0x1d, 0x1c, 0x1e, 0x13, 0x14, 0x10, 0x12 };
    const uint8_t seasons_pictures[] = { 0x03, 0x04, 0x05, 0x09, 0x0a, 0x0f, 0x10, 0x12 };
    const uint8_t seasons_in_room[] = { 0x06, 0x0b, 0x0c };
    /* Gifts of Kinomi's credits are a stub that never runs (0xff), and its own end screen (0x22) is a picture. */
    const uint8_t kinomi_pictures[] = { 0x03, 0x04, 0x05, 0x08, 0x09, 0x0f, 0x11, 0x20, 0x21, 0x22 };
    picture_scenes_of(original_profile(ORACLES_GAME_AGES), ages_pictures, sizeof ages_pictures, ages_in_room, sizeof ages_in_room);
    picture_scenes_of(original_profile(ORACLES_GAME_SEASONS), seasons_pictures, sizeof seasons_pictures, seasons_in_room, sizeof seasons_in_room);
    const OraclesRomInfo moonrise = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_MOONRISE_1_0_6, .known = 1 };
    const OraclesRomInfo temple = { .game = ORACLES_GAME_SEASONS, .revision = ORACLES_ROM_REVISION_TEMPLE_1_073, .known = 1 };
    const OraclesRomInfo kinomi = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_KINOMI_1_1_2, .known = 1 };
    CHECK(oracles_compat_find(&moonrise) && oracles_compat_find(&temple) && oracles_compat_find(&kinomi));
    if (!oracles_compat_find(&moonrise) || !oracles_compat_find(&temple) || !oracles_compat_find(&kinomi)) return;
    CHECK(oracles_compat_guest_tables(oracles_compat_find(&kinomi))->cutscene_credits == 0xffu);
    picture_scenes_of(oracles_compat_find(&moonrise), ages_pictures, sizeof ages_pictures, ages_in_room, sizeof ages_in_room);
    picture_scenes_of(oracles_compat_find(&kinomi), kinomi_pictures, sizeof kinomi_pictures, ages_in_room, sizeof ages_in_room);
    picture_scenes_of(oracles_compat_find(&temple), seasons_pictures, sizeof seasons_pictures, seasons_in_room, sizeof seasons_in_room);
}

/* The black the band shows, counted by the view for the harness and the
 * launcher: without a ghost every room around the one in play stays black,
 * and each is a room black whole once past the threshold, counted from the
 * first frame it was shown so; a framed frame ends the runs. */
static void black_count(void)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    OraclesEnhancedView *view = r.guest ? oracles_enhanced_view_start(r.core, r.guest, NULL, 0) : NULL;
    CHECK(view != NULL);
    if (!view) { rig_stop(&r); return; }
    oracles_enhanced_view_set_zoom_out(view, 1);
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    uint8_t *w0 = oracles_guest_wram_writable(r.guest, 0), *w1 = oracles_guest_wram_writable(r.guest, 1);
    uint8_t *link = w1 + (ORACLES_OBJECTS_BASE - 0xd000u);
    #define W0(sym) w0[(sym).addr - 0xc000u]
    W0(t->game_state) = 2; W0(t->cutscene_index) = 1; W0(t->scroll_mode) = 1; W0(t->screen_transition_state) = 2;
    W0(t->active_group) = 0; W0(t->active_room) = 0x45; W0(t->room_is_large) = 0;
    link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_STATE] = 1; link[ORACLES_OBJ_XH] = 80; link[ORACLES_OBJ_YH] = 64;
    OraclesEnhancedMode mode = ORACLES_ENHANCED_FRAMED;
    int32_t cam = 0, cam_y = 0;
    OraclesEnhancedBlack black;
    unsigned world = 0;
    for (unsigned i = 0; i < 100u; i++) { frames(&r, view, 1, &mode, &cam, &cam_y); world += mode == ORACLES_ENHANCED_WORLD; }
    oracles_enhanced_view_black(view, &black);
    CHECK(world > 60u && black.have_world && !black.have_full && black.rooms_over == 0);   /* never a full frame: no mean yet */
    frames(&r, view, 90, &mode, &cam, &cam_y);
    oracles_enhanced_view_black(view, &black);
    CHECK(mode == ORACLES_ENHANCED_WORLD && black.rooms_over >= 8u);   /* the eight rooms around it, and more drawn back */
    CHECK(black.longest[0].group == 0 && black.longest[0].room != 0x45 && black.longest[0].frames > ORACLES_ENHANCED_BLACK_ROOM_FRAMES);
    CHECK(black.longest[0].from == black.first_world && black.longest[0].frames == 190u - black.first_world);
    const unsigned over = black.rooms_over;
    W0(t->opened_menu_type) = 1;   /* framed: the runs end where they stood */
    frames(&r, view, 5, &mode, &cam, &cam_y);
    oracles_enhanced_view_black(view, &black);
    CHECK(mode == ORACLES_ENHANCED_FRAMED && black.rooms_over == over);
    #undef W0
    oracles_enhanced_view_stop(view);
    rig_stop(&r);
}

/* The rooms never visited kept dark, drawn back, no ghost to fill the band: the eight rooms around Link's are the game's
 * own unvisited ones, dark as decided, so that nothing of the band counts as black waiting for the ghost; a room
 * visited since is black again, waiting.  The reference room the game has not marked visited is counted, whatever the
 * setting. */
static void unvisited_rooms(void)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    OraclesEnhancedView *view = r.guest ? oracles_enhanced_view_start(r.core, r.guest, NULL, 0) : NULL;
    CHECK(view != NULL);
    if (!view) { rig_stop(&r); return; }
    oracles_enhanced_view_set_zoom_out(view, 1);
    CHECK(!oracles_enhanced_view_unvisited_hidden(view));
    oracles_enhanced_view_set_unvisited_hidden(view, 1);
    CHECK(oracles_enhanced_view_unvisited_hidden(view));
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    uint8_t *w0 = oracles_guest_wram_writable(r.guest, 0), *w1 = oracles_guest_wram_writable(r.guest, 1);
    uint8_t *link = w1 + (ORACLES_OBJECTS_BASE - 0xd000u), *flags = w0 + (t->group0_room_flags.addr - 0xc000u);
    #define W0(sym) w0[(sym).addr - 0xc000u]
    W0(t->game_state) = 2; W0(t->cutscene_index) = 1; W0(t->scroll_mode) = 1; W0(t->screen_transition_state) = 2;
    W0(t->active_group) = 0; W0(t->active_room) = 0x45; W0(t->room_is_large) = 0;
    link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_STATE] = 1; link[ORACLES_OBJ_XH] = 80; link[ORACLES_OBJ_YH] = 64;
    OraclesEnhancedMode mode = ORACLES_ENHANCED_FRAMED;
    int32_t cam = 0, cam_y = 0;
    OraclesEnhancedBlack black;
    OraclesEnhancedUnvisitedCounts u;
    frames(&r, view, 190, &mode, &cam, &cam_y);
    oracles_enhanced_view_black(view, &black);
    oracles_enhanced_view_unvisited_counts(view, &u);
    /* What the game's window leaves of the room in play (the rig's camera is no room's) is all that is uncovered. */
    const OraclesEnhancedObservation *ob = oracles_enhanced_view_observation(view);
    const int32_t across = (ob->window_left + 160 < 960 ? ob->window_left + 160 : 960) - (ob->window_left > 800 ? ob->window_left : 800);
    const int32_t down = (ob->window_top + 128 < 640 ? ob->window_top + 128 : 640) - (ob->window_top > 512 ? ob->window_top : 512);
    const unsigned in_play = 160u * 128u - (unsigned)(across * down);
    CHECK(mode == ORACLES_ENHANCED_WORLD && oracles_enhanced_view_uncovered(view) == in_play && black.rooms_over == 0u && black.have_full);
    CHECK(oracles_enhanced_view_waiting_frames(view) == 0u && u.hidden_frames > 60u && u.hidden_rooms_max == 8u && u.reveal_fades == 0u);
    const uint32_t *surface = oracles_enhanced_view_surface(view);
    const uint32_t dark = surface[16u * W];
    CHECK(surface[(16u + 127u) * W + W - 1u] == dark && oracles_enhanced_view_neighbour_dark(view, 1, NULL) && oracles_enhanced_view_neighbour_dark(view, 0, NULL));
    /* The reference room, unmarked, counted once the game has had its first frames of play to mark it; marked, no more. */
    CHECK(u.unvisited_reference > 60u && u.first_group == 0u && u.first_room == 0x45u);
    flags[0x45] = t->roomflag_visited;
    const unsigned counted = u.unvisited_reference;
    frames(&r, view, 5, &mode, &cam, &cam_y);
    oracles_enhanced_view_unvisited_counts(view, &u);
    CHECK(u.unvisited_reference == counted);
    /* The room to the right visited (another flag of it set before changes nothing): no ghost has drawn it, black that waits. */
    flags[0x46] = 0x20u;
    frames(&r, view, 5, &mode, &cam, &cam_y);
    CHECK(oracles_enhanced_view_uncovered(view) == in_play && oracles_enhanced_view_waiting_frames(view) == 0u);
    flags[0x46] |= t->roomflag_visited;
    frames(&r, view, 5, &mode, &cam, &cam_y);
    oracles_enhanced_view_unvisited_counts(view, &u);
    CHECK(oracles_enhanced_view_uncovered(view) == in_play + 160u * 128u && oracles_enhanced_view_waiting_frames(view) == 5u && u.hidden_rooms_max == 8u);
    CHECK(!oracles_enhanced_view_neighbour_dark(view, 1, NULL) && oracles_enhanced_view_neighbour_dark(view, 3, NULL));
    /* Shown: the band as before, every room around black, waiting. */
    const unsigned hidden_frames = u.hidden_frames;
    oracles_enhanced_view_set_unvisited_hidden(view, 0);
    frames(&r, view, 5, &mode, &cam, &cam_y);
    oracles_enhanced_view_unvisited_counts(view, &u);
    CHECK(u.hidden_frames == hidden_frames && oracles_enhanced_view_uncovered(view) == W * BAND_H - 160u * 128u && surface[16u * W] == dark);
    CHECK(!oracles_enhanced_view_neighbour_dark(view, 3, NULL));
    /* Kept dark in a dungeon alone: outdoors, the band as Shown. */
    oracles_enhanced_view_set_unvisited_hidden(view, ORACLES_ENHANCED_UNVISITED_DUNGEONS);
    CHECK(oracles_enhanced_view_unvisited_hidden(view) == ORACLES_ENHANCED_UNVISITED_DUNGEONS);
    frames(&r, view, 5, &mode, &cam, &cam_y);
    oracles_enhanced_view_unvisited_counts(view, &u);
    CHECK(u.hidden_frames == hidden_frames && oracles_enhanced_view_uncovered(view) == W * BAND_H - 160u * 128u && !oracles_enhanced_view_neighbour_dark(view, 3, NULL));
    oracles_enhanced_view_set_unvisited_hidden(view, 0);
    #undef W0
    oracles_enhanced_view_stop(view);
    rig_stop(&r);
}

/* A room's veil from one composition to the next, as add_grid_sources asks it of a room drawn. */
static unsigned veil_step(OraclesEnhancedView *v, uint8_t room)
{
    ev_hidden_begin(v);
    const unsigned veil = ev_room_veil(v, room);
    ev_hidden_end(v);
    v->frame++;
    return veil;
}

/* The rooms never visited from one composition to the next: a room visited between two of them fades in through
 * every veil, once; across an epoch (a warp, a time travel), another group, a savestate's load or the setting
 * switched, it shows at once; the room a scroll enters shows, fading in where the band shows it beside the window, and
 * so does a room that was the reference in the epoch, unmarked as the game's own cutscenes leave theirs. */
static void unvisited_states(void)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    OraclesEnhancedView *v = r.guest ? oracles_enhanced_view_start(r.core, r.guest, NULL, 0) : NULL;
    CHECK(v != NULL);
    if (!v) { rig_stop(&r); return; }
    oracles_enhanced_view_set_zoom_out(v, 1);
    oracles_enhanced_view_set_unvisited_hidden(v, 1);
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    uint8_t *w0 = oracles_guest_wram_writable(r.guest, 0), *w1 = oracles_guest_wram_writable(r.guest, 1);
    uint8_t *link = w1 + (ORACLES_OBJECTS_BASE - 0xd000u), *present = w0 + (t->group0_room_flags.addr - 0xc000u), *past = present + 0x100u;
    const uint8_t visited = t->roomflag_visited;
    #define W0(sym) w0[(sym).addr - 0xc000u]
    W0(t->game_state) = 2; W0(t->cutscene_index) = 1; W0(t->scroll_mode) = 1; W0(t->screen_transition_state) = 2;
    W0(t->active_group) = 0; W0(t->active_room) = 0x45; W0(t->room_is_large) = 0;
    link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_STATE] = 1; link[ORACLES_OBJ_XH] = 80; link[ORACLES_OBJ_YH] = 64;
    present[0x45] = visited;
    OraclesEnhancedMode mode = ORACLES_ENHANCED_FRAMED;
    int32_t cam = 0, cam_y = 0;
    frames(&r, v, 40, &mode, &cam, &cam_y);
    CHECK(mode == ORACLES_ENHANCED_WORLD && v->observer.ref_group == 0u && v->observer.ref_room == 0x45u && v->observation.room == 0x45u);
    OraclesEnhancedUnvisitedCounts u;
    oracles_enhanced_view_unvisited_counts(v, &u);
    const unsigned fades = u.reveal_fades;
    /* Kept dark, then visited: every veil from 240 down, then the room whole, one fade. */
    CHECK(veil_step(v, 0x46) == 256u && veil_step(v, 0x46) == 256u);
    present[0x46] |= visited;
    for (unsigned age = 1; age < ORACLES_ENHANCED_REVEAL_FRAMES; age++) CHECK(veil_step(v, 0x46) == 256u - age * 16u);
    CHECK(veil_step(v, 0x46) == 0u && veil_step(v, 0x46) == 0u);
    oracles_enhanced_view_unvisited_counts(v, &u);
    CHECK(u.reveal_fades == fades + 1u);
    /* Laid dark without pixels, then with them: its edge comes in over the same frames, once; one never laid bare, whole. */
    ev_hidden_begin(v); v->bare_now[0x56 >> 5u] |= 1u << (0x56 & 31u); ev_hidden_end(v); v->frame++;
    for (unsigned age = 1; age < ORACLES_ENHANCED_REVEAL_FRAMES; age++) {
        ev_hidden_begin(v); CHECK(ev_room_edge_veil(v, 0x56) == 256u - age * 16u); ev_hidden_end(v); v->frame++;
    }
    ev_hidden_begin(v); CHECK(ev_room_edge_veil(v, 0x56) == 0u && ev_room_edge_veil(v, 0x57) == 0u); ev_hidden_end(v); v->frame++;
    oracles_enhanced_view_unvisited_counts(v, &u);
    CHECK(u.edge_fades == 1u);
    /* Visited across a new epoch: shown at once. */
    CHECK(veil_step(v, 0x47) == 256u);
    present[0x47] |= visited;
    v->observation.epoch++;
    CHECK(veil_step(v, 0x47) == 0u);
    /* Across another group (the other era): its own page, shown at once once marked there. */
    CHECK(veil_step(v, 0x48) == 256u);
    present[0x48] |= visited;
    v->observer.ref_group = 1;
    CHECK(veil_step(v, 0x48) == 256u);   /* the past's 0x48, never entered */
    past[0x48] |= visited;
    v->observer.ref_group = 0;
    CHECK(veil_step(v, 0x48) == 0u);
    /* Across a savestate's load. */
    CHECK(veil_step(v, 0x49) == 256u);
    present[0x49] |= visited;
    uint8_t wire[1024];
    size_t written = 0;
    CHECK(oracles_enhanced_view_save_state(v, wire, sizeof wire, &written) == 0 && oracles_enhanced_view_load_state(v, wire, written) == 0);
    v->observer.ref_group = 0; v->observer.ref_room = 0x45;   /* as the next observation rebuilds them */
    CHECK(veil_step(v, 0x49) == 0u);
    /* Across the setting switched off and on. */
    CHECK(veil_step(v, 0x4a) == 256u);
    present[0x4a] |= visited;
    oracles_enhanced_view_set_unvisited_hidden(v, 0);
    CHECK(veil_step(v, 0x4a) == 0u);
    oracles_enhanced_view_set_unvisited_hidden(v, 1);
    CHECK(veil_step(v, 0x4a) == 0u);
    oracles_enhanced_view_unvisited_counts(v, &u);
    CHECK(u.reveal_fades == fades + 1u);
    /* The room a scroll enters, the game's active room before it marks it: shown, fading in beside the window. */
    CHECK(veil_step(v, 0x55) == 256u);
    v->observation.room = 0x55;
    CHECK(!ev_room_hidden(v, 0, 0x55) && veil_step(v, 0x55) == 240u);
    v->observation.room = 0x45;
    /* A room that was the reference in this epoch, unmarked: shown, until the epoch ends. */
    v->observer.ref_room = 0x35;
    CHECK(veil_step(v, 0x35) == 0u);
    v->observer.ref_room = 0x45;
    CHECK(!ev_room_hidden(v, 0, 0x35) && veil_step(v, 0x35) == 0u);
    v->observation.epoch++;
    CHECK(veil_step(v, 0x35) == 256u);
    /* Through the composition: a fade under way, then a real time travel (a room change outside a scroll, to the past's
     * 0x45): the observer opens a new epoch and the composition after it drops the fade, the rooms of the past dark or
     * shown as they are. */
    CHECK(veil_step(v, 0x56) == 256u);
    present[0x56] |= visited;
    CHECK(veil_step(v, 0x56) == 240u && v->reveal_count == 1u);
    frames(&r, v, 1, &mode, &cam, &cam_y);   /* the composition puts its own observation back */
    const uint64_t before = v->observation.epoch;
    W0(t->scroll_mode) = 2; frames(&r, v, 1, &mode, &cam, &cam_y);
    W0(t->active_group) = 1; frames(&r, v, 1, &mode, &cam, &cam_y);
    W0(t->scroll_mode) = 1; frames(&r, v, 2, &mode, &cam, &cam_y);
    CHECK(v->observation.epoch != before && v->observer.ref_group == 1u && mode == ORACLES_ENHANCED_WORLD);
    CHECK(v->reveal_count == 0u && v->hidden_group == 1u && v->hidden_epoch == v->observation.epoch);
    #undef W0
    oracles_enhanced_view_stop(v);
    rig_stop(&r);
}

/* The same black in episodes by cause, in far 4:3, no ghost to fill the
 * band: the rooms around the first room are the band's fill; Link in the
 * room beside, near its top right corner, shows the corner two columns and
 * two rows away and other rooms, black from then on, until a menu ends them. */
static void black_kinds(void)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    OraclesEnhancedView *view = r.guest ? oracles_enhanced_view_start(r.core, r.guest, NULL, 0) : NULL;
    CHECK(view != NULL);
    if (!view) { rig_stop(&r); return; }
    oracles_enhanced_view_set_size(view, oracles_enhanced_view_size(ORACLES_ENHANCED_FAR, ORACLES_ENHANCED_4_3));
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    uint8_t *w0 = oracles_guest_wram_writable(r.guest, 0), *w1 = oracles_guest_wram_writable(r.guest, 1);
    uint8_t *link = w1 + (ORACLES_OBJECTS_BASE - 0xd000u);
    #define W0(sym) w0[(sym).addr - 0xc000u]
    W0(t->game_state) = 2; W0(t->cutscene_index) = 1; W0(t->scroll_mode) = 1; W0(t->screen_transition_state) = 2;
    W0(t->active_group) = 0; W0(t->active_room) = 0x45; W0(t->room_is_large) = 0;
    link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_STATE] = 1; link[ORACLES_OBJ_XH] = 80; link[ORACLES_OBJ_YH] = 64;
    OraclesEnhancedMode mode = ORACLES_ENHANCED_FRAMED;
    int32_t cam = 0, cam_y = 0;
    OraclesEnhancedBlack black;
    frames(&r, view, ORACLES_ENHANCED_BLACK_COLD_FRAMES + 30u, &mode, &cam, &cam_y);
    oracles_enhanced_view_black(view, &black);
    CHECK(mode == ORACLES_ENHANCED_WORLD && black.kinds[ORACLES_ENHANCED_BLACK_COLD].episodes >= 8u);
    CHECK(black.kinds[ORACLES_ENHANCED_BLACK_CORNER].episodes == 0 && black.kinds[ORACLES_ENHANCED_BLACK_NEIGHBOUR].episodes == 0);
    CHECK(black.kinds[ORACLES_ENHANCED_BLACK_COLD].frames == black.kinds[ORACLES_ENHANCED_BLACK_COLD].longest);
    CHECK(black.kinds[ORACLES_ENHANCED_BLACK_OTHER].episodes == 0 && black.kinds[ORACLES_ENHANCED_BLACK_RIPPLE].episodes == 0);
    W0(t->active_room) = 0x46; link[ORACLES_OBJ_XH] = 150; link[ORACLES_OBJ_YH] = 10;
    frames(&r, view, 60, &mode, &cam, &cam_y);
    oracles_enhanced_view_black(view, &black);
    CHECK(mode == ORACLES_ENHANCED_WORLD && black.kinds[ORACLES_ENHANCED_BLACK_CORNER].episodes == 1u && black.kinds[ORACLES_ENHANCED_BLACK_NEIGHBOUR].episodes >= 3u);
    CHECK(black.kinds[ORACLES_ENHANCED_BLACK_CORNER].in_transition == 0 && oracles_enhanced_view_black_log(view)[0] == 0);
    W0(t->opened_menu_type) = 1;
    frames(&r, view, 5, &mode, &cam, &cam_y);
    oracles_enhanced_view_black(view, &black);
    CHECK(mode == ORACLES_ENHANCED_FRAMED && strstr(oracles_enhanced_view_black_log(view), "c 0:28 ") != NULL);
    CHECK(black.kinds[ORACLES_ENHANCED_BLACK_CORNER].episodes == 1u && black.kinds[ORACLES_ENHANCED_BLACK_CORNER].longest <= 60u);
    #undef W0
    oracles_enhanced_view_stop(view);
    rig_stop(&r);
}

/* A neighbour's settled state that may start a run while a cutscene holds
 * the game: its key the live one but for other rooms' flags (0:98 visited
 * since, as Din dances there), never the room's own nor anything else. */
static void parent_keys(void)
{
    uint8_t parent[64], live[64];
    for (unsigned i = 0; i < 64u; i++) parent[i] = live[i] = (uint8_t)(i * 7u);
    const long flags_at = 16, own = 16 + 0x17;   /* four pages of room flags would be longer: a short run of them serves */
    CHECK(oracles_enhanced_parent_key_holds(parent, live, 64u, flags_at, 32u, own));
    live[16 + 0x18] ^= 0x50u;                    /* another room visited since: holds */
    CHECK(oracles_enhanced_parent_key_holds(parent, live, 64u, flags_at, 32u, own));
    live[own] ^= 0x10u;                          /* the room's own flags: not */
    CHECK(!oracles_enhanced_parent_key_holds(parent, live, 64u, flags_at, 32u, own));
    live[own] ^= 0x10u; live[3] ^= 1u;           /* a byte outside the flags: not */
    CHECK(!oracles_enhanced_parent_key_holds(parent, live, 64u, flags_at, 32u, own));
    live[3] ^= 1u; live[16 + 32] ^= 1u;          /* the byte just past them: not */
    CHECK(!oracles_enhanced_parent_key_holds(parent, live, 64u, flags_at, 32u, own));
    live[16 + 32] ^= 1u;
    CHECK(!oracles_enhanced_parent_key_holds(parent, live, 64u, -1, 0u, own));   /* the flags not in the key: only an equal key */
    live[16 + 0x18] ^= 0x50u;
    CHECK(oracles_enhanced_parent_key_holds(parent, live, 64u, -1, 0u, -1));
}

/* A parent judged for a room chained from it: its key the live one but for
 * its own visited bit, and, in the drawn-back band, the other rooms' visited
 * bits (Link's first entry into a room, the rooms the ghost's chain crossed),
 * said so that the run's reads are checked on delivery. */
static void parent_visits(void)
{
    uint8_t parent[64], live[64];
    for (unsigned i = 0; i < 64u; i++) parent[i] = live[i] = (uint8_t)(i * 7u);
    const long flags_at = 16, own = 16 + 0x17;
    int visits = 0;
    CHECK(oracles_enhanced_parent_key_current(parent, live, 64u, flags_at, 32u, own, 0x10u, &visits) && !visits);
    live[own] ^= 0x10u;                          /* its own visited bit: holds, nothing to check */
    CHECK(oracles_enhanced_parent_key_current(parent, live, 64u, flags_at, 32u, own, 0x10u, &visits) && !visits);
    CHECK(oracles_enhanced_parent_key_current(parent, live, 64u, -1, 0u, own, 0x10u, NULL));
    live[16 + 0x18] ^= 0x10u;                    /* another room's visited bit: holds, its reads to check */
    CHECK(!oracles_enhanced_parent_key_current(parent, live, 64u, flags_at, 32u, own, 0x10u, NULL));   /* not judged so: not */
    CHECK(oracles_enhanced_parent_key_current(parent, live, 64u, flags_at, 32u, own, 0x10u, &visits) && visits);
    visits = 0;
    live[16 + 0x19] ^= 0x20u;                    /* another flag of another room: not */
    CHECK(!oracles_enhanced_parent_key_current(parent, live, 64u, flags_at, 32u, own, 0x10u, &visits));
    live[16 + 0x19] ^= 0x20u; live[own] ^= 0x01u;   /* another bit of its own flags: not */
    CHECK(!oracles_enhanced_parent_key_current(parent, live, 64u, flags_at, 32u, own, 0x10u, &visits));
    live[own] ^= 0x01u; live[3] ^= 0x10u;        /* bit 4 of a byte outside the flags: not */
    CHECK(!oracles_enhanced_parent_key_current(parent, live, 64u, flags_at, 32u, own, 0x10u, &visits));
    live[3] ^= 0x10u; live[16 + 32] ^= 0x10u;    /* the byte just past them: not */
    CHECK(!oracles_enhanced_parent_key_current(parent, live, 64u, flags_at, 32u, own, 0x10u, &visits));
}

/* A room run from beside during a cutscene, kept only if the bytes its run
 * read are the live ones, the visited bit of its own flags aside. */
static void beside_reads(void)
{
    const uint16_t addresses[3] = { 0xc797, 0xc798, 0xcc4e };   /* its own flags, a neighbour's, the season */
    uint8_t seen[3] = { 0x00, 0x00, 0x02 }, live[3] = { 0x10, 0x00, 0x02 };
    CHECK(oracles_enhanced_reads_hold(3, addresses, seen, live, 0xc797, 0x10));    /* only its visited bit: holds */
    live[1] = 0x50;
    CHECK(!oracles_enhanced_reads_hold(3, addresses, seen, live, 0xc797, 0x10));   /* a flag it read changed since: refused */
    live[1] = 0x10;
    CHECK(!oracles_enhanced_reads_hold(3, addresses, seen, live, 0xc797, 0x10));   /* another room's visited bit: refused, the bit is its own room's alone */
    live[1] = 0x00; live[0] = 0x30;
    CHECK(!oracles_enhanced_reads_hold(3, addresses, seen, live, 0xc797, 0x10));   /* another bit of its own flags: refused */
    CHECK(oracles_enhanced_reads_hold(0, addresses, seen, live, 0xc797, 0x10));    /* nothing read */
}

/* A neighbour's objects, run again when a byte they read moved in the live
 * game since its run: the visited bit of any room's flags aside, which an
 * entry sets (Ages' intro: GLOBALFLAG_INTRO_DONE, bit 2 of $c6d1, clears 0:39
 * of Nayru and the animals). */
static void object_reads(void)
{
    const uint16_t addresses[3] = { 0xc6d1, 0xc739, 0xc749 };   /* a global flags' byte, the room's own flags, a neighbour's */
    const uint8_t values[3] = { 0x00, 0xc8, 0x00 };
    uint8_t live[3] = { 0x00, 0xc8, 0x00 };
    CHECK(oracles_enhanced_object_reads_hold(3, addresses, values, live, 0xc700, 0x400, 0x10));
    live[0] = 0x04;
    CHECK(!oracles_enhanced_object_reads_hold(3, addresses, values, live, 0xc700, 0x400, 0x10));   /* the flag set: run again */
    live[0] = 0x00; live[1] = 0xd8; live[2] = 0x10;
    CHECK(oracles_enhanced_object_reads_hold(3, addresses, values, live, 0xc700, 0x400, 0x10));    /* visited bits, its own and another room's: hold */
    live[2] = 0x30;
    CHECK(!oracles_enhanced_object_reads_hold(3, addresses, values, live, 0xc700, 0x400, 0x10));   /* another flag of a room: run again */
    live[2] = 0x10; live[0] = 0x10;
    CHECK(!oracles_enhanced_object_reads_hold(3, addresses, values, live, 0xc700, 0x400, 0x10));   /* bit 4 outside the rooms' flags: run again */
    CHECK(oracles_enhanced_object_reads_hold(0, addresses, values, live, 0xc700, 0x400, 0x10));    /* nothing read */
}

/* A neighbour whose objects read the side they were entered by holds beside
 * the room in play only when its run scrolled the way the game would (Ages'
 * 0:39: its Ralph is created only from the Maku Tree's room, on its left). */
static void entry_side(void)
{
    CHECK(oracles_enhanced_entry_side_holds(0, ORACLES_DIR_RIGHT, ORACLES_DIR_UP));    /* the side not read */
    CHECK(oracles_enhanced_entry_side_holds(1, ORACLES_DIR_RIGHT, -1));                /* not beside: entered from more than one side */
    CHECK(oracles_enhanced_entry_side_holds(1, ORACLES_DIR_RIGHT, ORACLES_DIR_RIGHT)); /* run from the room in play's side */
    CHECK(!oracles_enhanced_entry_side_holds(1, ORACLES_DIR_RIGHT, ORACLES_DIR_UP));   /* run from the left, drawn above: run again */
}

/* A neighbour out of date for its objects is met only where it draws one of
 * them: Seasons' 0:97 holds an object without sprites, its carnival music. */
static void tags_draw_objects(void)
{
    OraclesSpriteTag tags[3];
    memset(tags, 0, sizeof tags);
    CHECK(!oracles_enhanced_tags_draw_objects(tags, 0));
    tags[0].count = 4;                           /* Link (kind 0) */
    tags[1].kind = ORACLES_OBJECT_KIND_INTERACTION;   /* an interaction that wrote no entry */
    CHECK(!oracles_enhanced_tags_draw_objects(tags, 2));
    tags[2].kind = ORACLES_OBJECT_KIND_ENEMY;
    tags[2].count = 2;
    CHECK(oracles_enhanced_tags_draw_objects(tags, 3));
}

/* The room a scroll left is drawn from its entry only when its objects did not
 * read the side, or its run entered it the way the game did; else from the
 * image of it the game last drew (Ages' 0:39 run from the Maku Tree's room,
 * left downward by Link who came in from below: no Ralph). */
static void left_entry_side(void)
{
    CHECK(oracles_enhanced_left_entry_side_holds(0, ORACLES_DIR_RIGHT, ORACLES_DIR_UP));    /* the side not read */
    CHECK(oracles_enhanced_left_entry_side_holds(1, ORACLES_DIR_UP, ORACLES_DIR_UP));       /* run as the game entered it */
    CHECK(!oracles_enhanced_left_entry_side_holds(1, ORACLES_DIR_RIGHT, ORACLES_DIR_UP));   /* run from the left, entered from below */
    CHECK(!oracles_enhanced_left_entry_side_holds(1, ORACLES_DIR_UP, -1));                  /* entered by a warp or a load: no side to match */
    CHECK(oracles_enhanced_left_entry_yields(1, ORACLES_DIR_RIGHT, ORACLES_DIR_UP, 1, 0));   /* through the scroll */
    CHECK(oracles_enhanced_left_entry_yields(1, ORACLES_DIR_RIGHT, ORACLES_DIR_UP, 0, 1));   /* after it, beside Link and out of date */
    CHECK(!oracles_enhanced_left_entry_yields(1, ORACLES_DIR_UP, -1, 0, 0));                 /* run again from below: drawn, animated */
    CHECK(!oracles_enhanced_left_entry_yields(1, ORACLES_DIR_UP, ORACLES_DIR_UP, 1, 0));     /* run as the game entered it */
    CHECK(!oracles_enhanced_left_entry_yields(0, ORACLES_DIR_RIGHT, ORACLES_DIR_UP, 1, 1));  /* no objects that read the side */
}

/* A room run again for its objects starts only with the live game in play:
 * at once for the side it is entered by, a second after for its story (a
 * treasure's text follows the flag it sets); a run that comes back with the
 * game held is put off, not failed, one interrupted or blank fails. */
static void objects_refresh_pace(void)
{
    CHECK(!oracles_enhanced_objects_refresh_may_start(0, 0));     /* a text box, a menu or a scene holds the game */
    CHECK(oracles_enhanced_objects_refresh_may_start(0, 1));      /* the side: as soon as the game is in play */
    CHECK(!oracles_enhanced_objects_refresh_may_start(1, 1));     /* the story: not at the frame of the change */
    CHECK(!oracles_enhanced_objects_refresh_may_start(1, 59));
    CHECK(oracles_enhanced_objects_refresh_may_start(1, 60));
    CHECK(!oracles_enhanced_objects_refresh_put_off(1));          /* in play: taken, or failed if interrupted or blank */
    CHECK(oracles_enhanced_objects_refresh_put_off(0));           /* the game held as it came back */
    CHECK(oracles_enhanced_load_window_holds(0, 0));                /* a load: the window opens */
    CHECK(!oracles_enhanced_load_window_holds(1, 1));               /* the band filled in play */
    CHECK(oracles_enhanced_load_window_holds(1827, 0));             /* the longest close measured, the Lost Woods with the thread */
    CHECK(!oracles_enhanced_load_window_holds(ORACLES_ENHANCED_LOAD_WINDOW_FRAMES, 0));   /* a ghost always busy: closed all the same */
}

/* A parent found no start from a run beyond it that came back elsewhere,
 * while it was not beside the room in play, is run again once it is; one
 * that refused to start (a scene) stays marked. */
static void parent_marks(void)
{
    CHECK(oracles_enhanced_parent_mark_lifts(1, 1));
    CHECK(!oracles_enhanced_parent_mark_lifts(1, 0));   /* still apart: its own parent would give the same state */
    CHECK(!oracles_enhanced_parent_mark_lifts(0, 1));   /* a refused start */
}

/* A room run again for its terrain more than three times in a row, each
 * within five seconds of the one before, waits for the end of the five
 * seconds (a byte that keeps moving in a parent's state). */
static void terrain_refresh_pace(void)
{
    CHECK(!oracles_enhanced_terrain_refresh_waits(1, 0));
    CHECK(!oracles_enhanced_terrain_refresh_waits(3, 10));
    CHECK(oracles_enhanced_terrain_refresh_waits(4, 10));
    CHECK(oracles_enhanced_terrain_refresh_waits(4, 299));
    CHECK(!oracles_enhanced_terrain_refresh_waits(4, 300));
}

/* A room whose run was refused for its parent's state, no start (Ages' 1:37
 * beyond the Maku sapling's grove at the first arrival in the past): settled
 * for the band's fill as a failed room is, though it waits on no clock, and
 * its parent run again no sooner than a failure would allow. */
static void waiting_on_a_start(void)
{
    const unsigned grove = 0x138u, beside = 0x147u, arrival = 0x139u, below = 0x148u;
    CHECK(!oracles_enhanced_fill_settled(0, 0, 0, 0, 0u, arrival));   /* nothing yet: the fill goes on */
    CHECK(oracles_enhanced_fill_settled(1, 0, 0, 0, 0u, arrival) && oracles_enhanced_fill_settled(0, 1, 0, 0, 0u, arrival)
          && oracles_enhanced_fill_settled(0, 0, 1, 0, 0u, arrival));
    CHECK(oracles_enhanced_fill_settled(0, 0, 0, 1, arrival, arrival));   /* waiting for another parent: settled, the look-ahead and the corners go on */
    CHECK(!oracles_enhanced_fill_settled(0, 0, 0, 1, arrival, below));    /* marked around another reference room: not, until found so again */
    CHECK(oracles_enhanced_parent_rerun_due(0, 0u, 0u, 0u, grove, arrival, 5u));   /* not waiting: the parent is run again at once */
    CHECK(!oracles_enhanced_parent_rerun_due(1, grove, arrival, 1000u, grove, arrival, 1000u));
    CHECK(!oracles_enhanced_parent_rerun_due(1, grove, arrival, 1000u, grove, arrival, 1299u));
    CHECK(oracles_enhanced_parent_rerun_due(1, grove, arrival, 1000u, grove, arrival, 1300u));   /* the pace of a failure, 300 frames */
    CHECK(oracles_enhanced_parent_rerun_due(1, grove, arrival, 0xffffff00u, grove, arrival, 0x0000002cu));   /* across the frame counter's wrap */
    CHECK(oracles_enhanced_parent_rerun_due(1, grove, arrival, 1000u, beside, arrival, 1001u));   /* another parent: at once */
    CHECK(oracles_enhanced_parent_rerun_due(1, grove, arrival, 1000u, grove, below, 1001u));      /* Link in another room: at once */
}

/* A ghost's capture of no terrain, never kept: Seasons' intro fades to white
 * as Din starts dancing, and a run settled in it captured every colour white
 * but for the outlines (71 % of its area one colour, under the nine tenths
 * that caught a blank screen).  A room the game darkens, or whose own
 * palettes are white, is its own look and kept. */
static void blank_captures(void)
{
    uint8_t base[64], live[64];
    for (unsigned i = 0; i < 32u; i++) {   /* a room's own palettes: every channel between 4 and 26 */
        const unsigned c = (4u + i % 20u) | (6u + i % 17u) << 5 | (8u + i % 13u) << 10;
        base[i * 2u] = (uint8_t)c; base[i * 2u + 1u] = (uint8_t)(c >> 8);
    }
    #define SET_OFFSET(o) for (unsigned i = 0; i < 32u; i++) { \
        const unsigned c = (unsigned)(base[i * 2u] | (base[i * 2u + 1u] << 8)); unsigned f = 0; \
        for (unsigned sh = 0; sh < 15u; sh += 5u) { const int ch = (int)((c >> sh) & 31u) + (o); f |= (unsigned)(ch < 0 ? 0 : ch > 31 ? 31 : ch) << sh; } \
        live[i * 2u] = (uint8_t)f; live[i * 2u + 1u] = (uint8_t)(f >> 8); }
    SET_OFFSET(0);
    CHECK(!oracles_enhanced_capture_blank(live, base, 0, NULL, 0));          /* the room's own */
    SET_OFFSET(2);
    CHECK(!oracles_enhanced_capture_blank(live, base, 0, NULL, 0));          /* a smooth palette transition's last mix, 2 apart */
    SET_OFFSET(28);
    CHECK(oracles_enhanced_capture_blank(live, base, 0, NULL, 0));           /* a fade to white nearly done */
    SET_OFFSET(-12);
    CHECK(oracles_enhanced_capture_blank(live, base, 0, NULL, 0));           /* a fade to black */
    SET_OFFSET(0);
    live[0] = 0xff; live[1] = 0x7f; live[2] = 0xff; live[3] = 0x7f;       /* a palette set white by other code (Seasons' outdoors): outvoted */
    CHECK(!oracles_enhanced_capture_blank(live, base, 0, NULL, 0));
    for (unsigned c = 0; c < 4u; c++) { live[c * 2u] = 0xff; live[c * 2u + 1u] = 0x7f; }   /* the whole palette white, the others the room's: still outvoted */
    CHECK(!oracles_enhanced_capture_blank(live, base, 0, NULL, 0));
    memset(live, 0, sizeof live);   /* a fade to black that puts one palette back, in the base too (the Maku Tree's remote voice): a full fade */
    memcpy(live + 8u, base + 8u, 8u);
    CHECK(oracles_enhanced_capture_blank(live, base, 0, NULL, 0));
    SET_OFFSET(-16);   /* a room the game darkens (checkDarkenRoom): -16 kept on it, its own look */
    CHECK(!oracles_enhanced_capture_blank(live, base, -16, NULL, 0));
    CHECK(oracles_enhanced_capture_blank(live, base, 0, NULL, 0));        /* the same palettes on a room kept at 0: a fade caught */
    SET_OFFSET(28);
    CHECK(oracles_enhanced_capture_blank(live, base, -16, NULL, 0));      /* a fade to white over a dark room */
    #undef SET_OFFSET
    uint8_t white[64];   /* a room whose own palettes are all white (3:f0 and 3:f2 of Ages): no fade */
    memset(white, 0xff, sizeof white);
    for (unsigned i = 1; i < 64u; i += 2u) white[i] = 0x7f;
    CHECK(!oracles_enhanced_capture_blank(white, white, 0, NULL, 0));
    static uint32_t area[160u * 128u];
    for (unsigned i = 0; i < 160u * 128u; i++) area[i] = i % 10u == 0u ? 0xff101010u : 0xffe0e0e0u;
    CHECK(!oracles_enhanced_capture_blank(base, base, 0, area, 160u * 128u));   /* nine tenths exactly: a terrain */
    area[0] = 0xffe0e0e0u; area[10] = 0xffe0e0e0u;
    CHECK(oracles_enhanced_capture_blank(base, base, 0, area, 160u * 128u));    /* more: the LCD off */
}

/* The night Nayru's possession puts on Lynna (Ages' CUTSCENE_NAYRU_SINGING):
 * PALH_99 over palettes 2 to 7 of PALH_TILESET_LYNNA_CITY, the base itself,
 * with no fade.  The other rooms take it as a colour transform; a palette or
 * two of an object, a palette transition's last mix and palettes of another
 * room are no scene's tint. */
static void set_colours(uint8_t *palettes, unsigned first, const uint8_t (*rgb)[3], unsigned count)
{
    for (unsigned i = 0; i < count; i++) {
        const unsigned c = rgb[i][0] | rgb[i][1] << 5 | rgb[i][2] << 10;
        palettes[(first + i) * 2u] = (uint8_t)c; palettes[(first + i) * 2u + 1u] = (uint8_t)(c >> 8);
    }
}

static unsigned channel(const uint8_t *palettes, unsigned i, unsigned shift) { return ((unsigned)(palettes[i * 2u] | palettes[i * 2u + 1u] << 8) >> shift) & 31u; }

static void scene_tints(void)
{
    static const uint8_t common[8][3] = { { 31, 31, 31 }, { 20, 20, 20 }, { 10, 10, 10 }, { 0, 0, 0 }, { 31, 31, 24 }, { 31, 12, 4 }, { 16, 4, 0 }, { 0, 0, 0 } };
    static const uint8_t lynna[24][3] = {   /* paletteData4a30 */
        { 31, 28, 2 }, { 16, 28, 2 }, { 6, 17, 0 }, { 0, 0, 0 }, { 25, 17, 1 }, { 16, 28, 2 }, { 6, 17, 0 }, { 0, 0, 0 },
        { 31, 28, 2 }, { 25, 17, 1 }, { 16, 10, 0 }, { 0, 0, 0 }, { 26, 23, 20 }, { 15, 18, 20 }, { 8, 9, 12 }, { 4, 2, 0 },
        { 31, 31, 24 }, { 4, 22, 31 }, { 0, 10, 22 }, { 0, 0, 0 }, { 20, 24, 15 }, { 7, 18, 16 }, { 9, 4, 16 }, { 0, 5, 5 } };
    static const uint8_t night[24][3] = {   /* paletteData44a8 */
        { 7, 16, 19 }, { 6, 10, 13 }, { 2, 3, 11 }, { 0, 0, 5 }, { 9, 11, 11 }, { 6, 11, 13 }, { 2, 3, 11 }, { 0, 0, 5 },
        { 7, 16, 19 }, { 9, 11, 11 }, { 5, 6, 8 }, { 0, 0, 5 }, { 7, 16, 19 }, { 3, 12, 14 }, { 2, 4, 12 }, { 0, 0, 5 },
        { 7, 18, 25 }, { 3, 14, 21 }, { 2, 4, 14 }, { 0, 0, 5 }, { 7, 16, 19 }, { 3, 12, 14 }, { 2, 4, 12 }, { 0, 0, 5 } };
    uint8_t reference[64], scene[64], out[64], other[64];
    int32_t k[12];
    uint8_t slots = 0, changed = 0;
    set_colours(reference, 0, common, 8); set_colours(reference, 8, lynna, 24);
    set_colours(scene, 0, common, 8); set_colours(scene, 8, night, 24);
    CHECK(oracles_enhanced_scene_tint(reference, scene, &slots, &changed, k) && slots == 0xfcu && changed == 0xfcu);
    oracles_enhanced_scene_tinted(reference, reference, scene, changed, k, out);
    CHECK(memcmp(out, scene, 64u) == 0);   /* a room of the live room's palettes: the scene's own */
    oracles_enhanced_scene_tinted(scene, reference, scene, changed, k, out);
    CHECK(memcmp(out, scene, 64u) == 0);   /* a room the ghost ran during the scene, its palettes the scene's: kept */
    /* Another room's palettes: through the transform, darker and bluer, palettes 0 and 1 untouched. */
    for (unsigned i = 0; i < 32u; i++) {
        const uint8_t rgb[1][3] = { { (uint8_t)(10u + i % 20u), (uint8_t)(12u + i % 19u), (uint8_t)(i % 9u) } };
        set_colours(other, i, rgb, 1);
    }
    oracles_enhanced_scene_tinted(other, reference, scene, changed, k, out);
    CHECK(memcmp(out, other, 16u) == 0);
    unsigned red_in = 0, red_out = 0, blue_in = 0, blue_out = 0;
    for (unsigned i = 8; i < 32u; i++) { red_in += channel(other, i, 0); red_out += channel(out, i, 0); blue_in += channel(other, i, 10); blue_out += channel(out, i, 10); }
    CHECK(red_out * 2u < red_in && blue_out > blue_in);
    /* Half way through the palette thread's fade between the two: the tint so far. */
    uint8_t half[64];
    memcpy(half, reference, 64u);
    for (unsigned i = 4; i < 32u; i++) {
        unsigned c = 0;
        for (unsigned shift = 0; shift < 15u; shift += 5u) c |= ((channel(reference, i, shift) + channel(scene, i, shift)) / 2u) << shift;
        half[i * 2u] = (uint8_t)c; half[i * 2u + 1u] = (uint8_t)(c >> 8);
    }
    CHECK(oracles_enhanced_scene_tint(reference, half, &slots, &changed, k) && slots == 0xfcu);
    /* Two palettes of an object: no tint of the screen. */
    memcpy(half, reference, 64u);
    memcpy(half + 16u, scene + 16u, 16u);
    CHECK(!oracles_enhanced_scene_tint(reference, half, &slots, &changed, k));
    /* A palette transition's last mix, every channel within 2: no scene's tint. */
    memcpy(half, reference, 64u);
    for (unsigned i = 4; i < 32u; i++) {
        unsigned c = 0;
        for (unsigned shift = 0; shift < 15u; shift += 5u) { const unsigned ch = channel(reference, i, shift); c |= (ch >= 2u ? ch - 2u : ch + 1u) << shift; }
        half[i * 2u] = (uint8_t)c; half[i * 2u + 1u] = (uint8_t)(c >> 8);
    }
    CHECK(!oracles_enhanced_scene_tint(reference, half, &slots, &changed, k));
    /* That last mix with two palettes far apart, which no transform of the
     * others explains: what is left is no tint either. */
    for (unsigned i = 0; i < 8u; i++) { half[i * 2u] = (uint8_t)(0x1fu * (i & 1u)); half[i * 2u + 1u] = (uint8_t)(0x7cu * ((i >> 1) & 1u)); }
    CHECK(!oracles_enhanced_scene_tint(reference, half, &slots, &changed, k));
    /* The same palettes in another order: no transform takes the one to the other. */
    memcpy(half, reference, 64u);
    for (unsigned p = 2; p < 8u; p++) memcpy(half + p * 8u, reference + (9u - p) * 8u, 8u);
    CHECK(!oracles_enhanced_scene_tint(reference, half, &slots, &changed, k));
    CHECK(!oracles_enhanced_scene_tint(reference, reference, &slots, &changed, k));
    /* Onox's arrival in Seasons: four palettes to the night, two others red,
     * which no transform of the night explains.  The night is the tint: a
     * room's palettes that were the live room's take the scene's, red ones
     * included, any other goes to the night. */
    static const uint8_t red[8][3] = { { 18, 3, 1 }, { 25, 16, 4 }, { 19, 10, 1 }, { 0, 0, 0 }, { 24, 1, 11 }, { 29, 3, 0 }, { 18, 2, 1 }, { 0, 0, 0 } };
    memcpy(half, scene, 64u);
    set_colours(half, 24, red, 8);
    CHECK(oracles_enhanced_scene_tint(reference, half, &slots, &changed, k) && slots == 0x3cu && changed == 0xfcu);
    oracles_enhanced_scene_tinted(reference, reference, half, changed, k, out);
    CHECK(memcmp(out, half, 64u) == 0);
    uint8_t dark_other[64];
    oracles_enhanced_scene_tinted(other, reference, scene, changed, k, dark_other);
    oracles_enhanced_scene_tinted(other, reference, half, changed, k, out);
    CHECK(memcmp(out, dark_other, 64u) == 0);   /* another room's palettes all go to the night */
}

static void set_rgb555(uint8_t *palettes, const uint16_t colours[32])
{
    for (unsigned i = 0; i < 32u; i++) { palettes[i * 2u] = (uint8_t)colours[i]; palettes[i * 2u + 1u] = (uint8_t)(colours[i] >> 8); }
}

/* In the few frames after a fade, a scroll or a palette transition, a tint
 * is a scene's only when most of its palettes changed beyond the
 * transition's last mix.  The palettes on screen two frames after a scroll's
 * fade into Lynna's 0:6b, within 2 of the room's but for
 * palette 1, which the room shows white, fit a tint of four palettes and are
 * none; Nayru's night as its palettes start to mix (intro, 7877) and the
 * night a fade from white opens on (14718) are. */
static void tint_after_fade(void)
{
    static const uint16_t room_6b[32] = { 0x142d, 0x0c3d, 0x475f, 0x0000, 0x433f, 0x269f, 0x0e1f, 0x01be, 0x5212, 0x3dac, 0x2d05, 0x0000, 0x3d52, 0x3dac, 0x2d05, 0x0000,
                                          0x5212, 0x3d51, 0x2469, 0x0000, 0x498e, 0x3d0c, 0x2ca7, 0x0000, 0x6193, 0x48ed, 0x3849, 0x0000, 0x5212, 0x50ef, 0x2c07, 0x0000 };
    static const uint16_t fade_end[32] = { 0x142d, 0x0c3d, 0x475f, 0x0000, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x4a12, 0x39ac, 0x2905, 0x0000, 0x3952, 0x39ac, 0x2905, 0x0000,
                                           0x4a12, 0x3951, 0x2069, 0x0000, 0x498e, 0x3d0c, 0x2ca7, 0x0000, 0x61b3, 0x48ec, 0x3848, 0x0000, 0x4e12, 0x4cee, 0x2c07, 0x0000 };
    static const uint16_t lynna[32] = { 0x142d, 0x0c3d, 0x475f, 0x0000, 0x11df, 0x7ea4, 0x03e8, 0x0000, 0x0b9f, 0x0b90, 0x0226, 0x0000, 0x0639, 0x0b90, 0x0226, 0x0000,
                                        0x0b9f, 0x0639, 0x0150, 0x0000, 0x52fa, 0x524f, 0x3128, 0x0044, 0x63ff, 0x7ec4, 0x5940, 0x0000, 0x3f14, 0x4247, 0x4089, 0x14a0 };
    static const uint16_t dusk[32] = { 0x142d, 0x0c3d, 0x475f, 0x0000, 0x11df, 0x7ea4, 0x03e8, 0x0000, 0x173a, 0x130e, 0x09c5, 0x0000, 0x09f6, 0x130e, 0x09c5, 0x0000,
                                       0x173a, 0x09f6, 0x052d, 0x0000, 0x4eb6, 0x4a0c, 0x3106, 0x0023, 0x639a, 0x7683, 0x5100, 0x0000, 0x3ed1, 0x3e06, 0x3c87, 0x1480 };
    static const uint16_t night[32] = { 0x142d, 0x0c3d, 0x475f, 0x0000, 0x11df, 0x7ea4, 0x03e8, 0x0000, 0x4e07, 0x3546, 0x2c62, 0x1400, 0x2d69, 0x3566, 0x2c62, 0x1400,
                                        0x4e07, 0x2d69, 0x20c5, 0x1400, 0x4e07, 0x3983, 0x3082, 0x1400, 0x6647, 0x55c3, 0x3882, 0x1400, 0x4e07, 0x3983, 0x3082, 0x1400 };
    uint8_t reference[64], scene[64];
    int32_t k[12];
    uint8_t slots = 0, changed = 0;
    set_rgb555(reference, room_6b); set_rgb555(scene, fade_end);
    CHECK(oracles_enhanced_scene_tint(reference, scene, &slots, &changed, k) && slots == 0xc6u);   /* a tint of four palettes, the white one among them */
    CHECK(!oracles_enhanced_scene_tint_after_fade(reference, scene, slots));                      /* one beyond the mix: none just after the fade */
    set_rgb555(reference, lynna); set_rgb555(scene, dusk);
    CHECK(oracles_enhanced_scene_tint(reference, scene, &slots, &changed, k) && oracles_enhanced_scene_tint_after_fade(reference, scene, slots));
    set_rgb555(scene, night);
    CHECK(oracles_enhanced_scene_tint(reference, scene, &slots, &changed, k) && slots == 0xfcu && oracles_enhanced_scene_tint_after_fade(reference, scene, slots));
}

/* The room's own palettes a scene's tint is taken from, kept by room while
 * the scene lasts and no longer: its entry run again during the scene (the
 * night in it, or the fade part way) or taken for another room does not
 * change them; the scene over, a warp or a load, or ten seconds without a
 * tint drop them, so that palettes kept by mistake cannot tint a room's
 * neighbours for the rest of the session. */
static void tint_memories(void)
{
    OraclesTintMemory m[ORACLES_TINT_MEMORIES];
    uint8_t day[64], night[64], dusk[64], other[64];
    for (unsigned i = 0; i < 64u; i++) { day[i] = (uint8_t)(i * 3u); night[i] = (uint8_t)(i * 3u + 1u); dusk[i] = (uint8_t)(i * 3u + 2u); other[i] = (uint8_t)(200u - i); }
    oracles_enhanced_tint_forget(m);
    CHECK(oracles_enhanced_tint_reference(m, 0, 0x39, 0x10, 100, day, night) == day);   /* nothing kept: the entry */
    CHECK(oracles_enhanced_tint_reference(m, 0, 0x39, 0x10, 100, NULL, night) == NULL);
    oracles_enhanced_tint_kept(m, 0, 0x39, 0x10, 100, day);
    const uint8_t *r = oracles_enhanced_tint_reference(m, 0, 0x39, 0x10, 101, night, night);   /* the entry run again in the night */
    CHECK(r && memcmp(r, day, 64u) == 0);
    r = oracles_enhanced_tint_reference(m, 0, 0x39, 0x10, 102, dusk, night);   /* run again part way through the fade */
    CHECK(r && memcmp(r, day, 64u) == 0);
    oracles_enhanced_tint_kept(m, 0, 0x39, 0x10, 102, dusk);   /* what a tint held from: never the later entry */
    r = oracles_enhanced_tint_reference(m, 0, 0x39, 0x10, 103, NULL, night);   /* the entry taken for another room */
    CHECK(r && memcmp(r, day, 64u) == 0);
    CHECK(oracles_enhanced_tint_reference(m, 0, 0x38, 0x10, 103, other, night) == other);   /* another room: its own entry */
    CHECK(oracles_enhanced_tint_reference(m, 0, 0x39, 0x11, 103, NULL, night) == NULL);    /* another palette header */
    /* The scene over: entry, screen and kept palettes the same. */
    CHECK(oracles_enhanced_tint_reference(m, 0, 0x39, 0x10, 104, day, day) == day);
    CHECK(oracles_enhanced_tint_reference(m, 0, 0x39, 0x10, 105, NULL, night) == NULL);
    /* Ten seconds without a tint held from them. */
    oracles_enhanced_tint_kept(m, 0, 0x39, 0x10, 200, day);
    CHECK(oracles_enhanced_tint_reference(m, 0, 0x39, 0x10, 200 + ORACLES_TINT_MEMORY_FRAMES, NULL, night) != NULL);
    CHECK(oracles_enhanced_tint_reference(m, 0, 0x39, 0x10, 201 + ORACLES_TINT_MEMORY_FRAMES, NULL, night) == NULL);
    /* A warp or a load. */
    oracles_enhanced_tint_kept(m, 0, 0x39, 0x10, 300, day);
    oracles_enhanced_tint_forget(m);
    CHECK(oracles_enhanced_tint_reference(m, 0, 0x39, 0x10, 301, NULL, night) == NULL);
    /* Four rooms kept at most: the longest without a tint goes first. */
    for (unsigned i = 0; i < ORACLES_TINT_MEMORIES + 1u; i++) oracles_enhanced_tint_kept(m, 0, (uint8_t)(0x40u + i), 0x10, 400u + i, day);
    CHECK(oracles_enhanced_tint_reference(m, 0, 0x40, 0x10, 410, NULL, night) == NULL);
    CHECK(oracles_enhanced_tint_reference(m, 0, 0x44, 0x10, 410, NULL, night) != NULL);
}

int main(void)
{
    world_math();
    compositor();
    dark_sources();
    dark_edges();
    hud_full_width();
    ripple(0);
    ripple(2);
    view_and_savestate();
    isolated_room_edges();
    maku_tree_arrival();
    maku_tree_wait_held();
    maku_tree_other_arrivals();
    load_band_centred();
    dungeon_diagonal_band();
    dungeon_diagonal_band_4_3();
    state_across_viewports();
    small_room_loop();
    diagonal_walk();
    stairs_walk();
    pixel_stop_turn();
    first_profile_switch();
    hold_before_dark();
    for (int exact = 0; exact < 2; exact++)
        for (int axis = 0; axis < 2; axis++) {
            rejoin_after_hold(exact, axis, 1);
            rejoin_after_hold(exact, axis, -1);
        }
    rejoin_on_diagonal();
    rejoin_at_map_edge();
    cutscenes();
    seasons_scenes();
    picture_scenes();
    blank_captures();
    scene_tints();
    tint_after_fade();
    tint_memories();
    black_count();
    black_kinds();
    unvisited_rooms();
    unvisited_states();
    parent_keys();
    parent_visits();
    beside_reads();
    object_reads();
    entry_side();
    tags_draw_objects();
    left_entry_side();
    objects_refresh_pace();
    parent_marks();
    terrain_refresh_pace();
    waiting_on_a_start();
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_zoom_out: ok\n");
    return 0;
}
