/* The status bar floating over the world (hud=floating), without a ROM: the world band the whole surface, the same
 * world rows as under the HUD band a band's height higher, no HUD band composed, and the checks of rooms kept dark
 * reading the band where it stands. */
#include "compositor.h"
#include "floating.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

/* The band's top and height for each choice, and a surface's own lines. */
static void geometry(void)
{
    CHECK(oracles_enhanced_band_top(0) == ORACLES_ENHANCED_HUD_HEIGHT && oracles_enhanced_band_top(1) == 0u);
    for (unsigned l = 0; l < ORACLES_ENHANCED_LEVELS; l++)
        for (unsigned a = 0; a < ORACLES_ENHANCED_ASPECTS; a++) {
            const OraclesEnhancedSize s = oracles_enhanced_view_size((OraclesEnhancedLevel)l, (OraclesEnhancedAspect)a);
            CHECK(oracles_enhanced_world_height(s, 0) == oracles_enhanced_band_height(s));
            CHECK(oracles_enhanced_world_height(s, 1) == s.height);
        }
}

/* A world composed floating at a camera sixteen lines higher than under the band: every line from the band's top on the
 * same, the sixteen lines above it the world rows over it, the core's bar nowhere; the uncovered pixels the same less
 * those of the sixteen rows. */
static void composed(unsigned width, unsigned height)
{
    static uint32_t core[160u * 144u], neighbour[160u * 128u];
    for (unsigned i = 0; i < 160u * 144u; i++) core[i] = 0xff000000u | (i * 2654435761u >> 8);
    for (unsigned i = 0; i < 160u * 128u; i++) neighbour[i] = 0xff000000u | (0x51u + i * 40503u);
    uint32_t *under = malloc((size_t)width * height * sizeof *under), *floating = malloc((size_t)width * height * sizeof *floating);
    OraclesEnhancedNeighbour above = { 1000, 500 - 128, neighbour, 0, 0, 1, 0, 0 };
    OraclesEnhancedCompose in;
    memset(&in, 0, sizeof in);
    in.mode = ORACLES_ENHANCED_WORLD;
    in.border = 0xff101010u;
    in.size = (OraclesEnhancedSize){ width, height };
    in.window_world_left = 1000; in.window_world_top = 500;
    in.world_left = 1000 - (int32_t)(width - 160u) / 2; in.world_top = 500 - 40;
    in.neighbours = &above; in.neighbour_count = 1;
    const unsigned uncovered_under = oracles_enhanced_compose(core, &in, under);
    in.floating = 1;
    in.world_top -= (int32_t)ORACLES_ENHANCED_HUD_HEIGHT;
    const unsigned uncovered_floating = oracles_enhanced_compose(core, &in, floating);
    CHECK(memcmp(under + 16u * width, floating + 16u * width, (size_t)width * (height - 16u) * sizeof *under) == 0);
    unsigned uncovered_top = 0, bar = 0;
    for (unsigned y = 0; y < 16u; y++)
        for (unsigned x = 0; x < width; x++) {
            const int32_t wx = in.world_left + (int32_t)x, wy = in.world_top + (int32_t)y;
            const int lit = wx >= above.world_left && wx < above.world_left + 160 && wy >= above.world_top && wy < above.world_top + 128;
            if (!lit) uncovered_top++;
            else CHECK(floating[y * width + x] == neighbour[(unsigned)(wy - above.world_top) * 160u + (unsigned)(wx - above.world_left)]);
            bar += floating[y * width + x] == core[y * 160u + (x + 160u - width % 160u) % 160u];
        }
    CHECK(uncovered_floating == uncovered_under + uncovered_top);
    CHECK(bar < width);   /* the world, not the bar: a coincidence here and there at most */
    /* The HUD band alone draws nothing floating. */
    memcpy(under, floating, (size_t)width * height * sizeof *under);
    oracles_enhanced_compose_hud(core, &in, floating);
    CHECK(memcmp(under, floating, (size_t)width * height * sizeof *under) == 0);
    free(under); free(floating);
}

/* A room kept dark beside the window: lit past its edge or not, read on the band where it stands, floating or not. */
static void dark(void)
{
    static uint32_t core[160u * 144u], surface[480u * 270u];
    for (unsigned i = 0; i < 160u * 144u; i++) core[i] = 0xff808080u;
    for (int floating = 0; floating < 2; floating++) {
        OraclesEnhancedNeighbour kept = { 1160, 500, NULL, 0, 0, 0, 256, 0 };
        OraclesEnhancedCompose in;
        memset(&in, 0, sizeof in);
        in.mode = ORACLES_ENHANCED_WORLD;
        in.size = (OraclesEnhancedSize){ 480u, 270u };
        in.window_world_left = 1000; in.window_world_top = 500;
        in.world_left = 1000 - 160; in.world_top = 500 - 63 - (floating ? 16 : 0);
        in.neighbours = &kept; in.neighbour_count = 1;
        in.floating = floating;
        oracles_enhanced_compose(core, &in, surface);
        CHECK(oracles_enhanced_dark_lit(&in, surface) == 0u && oracles_enhanced_dark_stray(&in, surface) == 0u);
        const unsigned top = oracles_enhanced_band_top(floating);
        /* the room's middle, lit by hand */
        surface[(top + (unsigned)(500 + 64 - in.world_top)) * 480u + (unsigned)(1160 + 80 - in.world_left)] = 0xffffffffu;
        CHECK(oracles_enhanced_dark_lit(&in, surface) == 1u);
        oracles_enhanced_compose(core, &in, surface);
        /* its first line inside its top side, lit by hand */
        surface[(top + (unsigned)(500 - in.world_top)) * 480u + (unsigned)(1160 + 40 - in.world_left)] = 0xffffffffu;
        CHECK(oracles_enhanced_dark_stray(&in, surface) == 1u);
    }
}

static int panel_is(const OraclesFloatingPanel *p, OraclesFloatingKind kind, int32_t x, int32_t y, int32_t w, int32_t h)
{
    return p->kind == kind && p->x == x && p->y == y && p->w == w && p->h == h;
}

/* The panels in each case: the hearts sized from Link's, the money at the top-right, B and A at the bottom-left or the
 * pad's six tiles, the whole bar alone for a map of no layout, the touch controls' rectangles kept clear. */
static void layouts(void)
{
    OraclesFloatingInput in;
    OraclesFloatingLayout l;
    memset(&in, 0, sizeof in);
    in.width = 480u; in.height = 270u; in.known = 1; in.buttons = 80u; in.hearts = 13u; in.per_row = 7u; in.pad = 1;
    oracles_floating_layout(&in, &l);
    CHECK(l.count == 8u && l.radius == 2u && l.hearts_cols == 7u && l.hearts_rows == 2u);
    CHECK(panel_is(&l.panels[0], ORACLES_FLOATING_HEARTS, 6, 6, 60, 20) && l.panels[0].top);
    CHECK(panel_is(&l.panels[1], ORACLES_FLOATING_MONEY, 480 - 6 - 28, 6, 28, 20) && l.panels[1].top);
    /* LB X RB on top, Y and A in the middle, B below, the slots X, Y, LB, RB: 107 x 68 at the bottom-left, tiles 33 x 20
     * four apart */
    static const int32_t at[6][2] = { { 43, 196 }, { 6, 220 }, { 6, 196 }, { 80, 196 }, { 43, 244 }, { 80, 220 } };
    for (unsigned s = 0; s < 6u; s++) CHECK(panel_is(&l.panels[2u + s], ORACLES_FLOATING_TILE, at[s][0], at[s][1], 33, 20) && l.panels[2u + s].slot == s && !l.panels[2u + s].top);
    /* The keys taken from the keyboard: LB RB on top, X Y in the middle, B A below, 70 x 68 at the bottom-left */
    in.keyboard = 1;
    oracles_floating_layout(&in, &l);
    static const int32_t keys_at[6][2] = { { 6, 220 }, { 43, 220 }, { 6, 196 }, { 43, 196 }, { 6, 244 }, { 43, 244 } };
    CHECK(l.count == 8u);
    for (unsigned s = 0; s < 6u; s++) CHECK(panel_is(&l.panels[2u + s], ORACLES_FLOATING_TILE, keys_at[s][0], keys_at[s][1], 33, 20) && l.panels[2u + s].slot == s && l.panels[2u + s].anchor == ORACLES_FLOATING_BOTTOM);
    in.keyboard = 0;
    in.hearts = 3u;   /* three hearts: one row of three */
    oracles_floating_layout(&in, &l);
    CHECK(l.hearts_cols == 3u && l.hearts_rows == 1u && panel_is(&l.panels[0], ORACLES_FLOATING_HEARTS, 6, 6, 28, 12));
    in.hearts = 16u; in.per_row = 8u; in.buttons = 72u; in.pad = 0;   /* tightened: eight a row, B and A 72 wide */
    oracles_floating_layout(&in, &l);
    CHECK(l.count == 3u && l.hearts_cols == 8u && l.hearts_rows == 2u && panel_is(&l.panels[0], ORACLES_FLOATING_HEARTS, 6, 6, 68, 20));
    CHECK(panel_is(&l.panels[2], ORACLES_FLOATING_BUTTONS, 6, 270 - 6 - 20, 76, 20));
    in.hearts = 40u;   /* past the bar's two rows: its two rows */
    oracles_floating_layout(&in, &l);
    CHECK(l.hearts_cols == 8u && l.hearts_rows == 2u);
    /* Near: four from the edges, B and A alone with the pad asked */
    memset(&in, 0, sizeof in);
    in.width = 256u; in.height = 144u; in.near_view = 1; in.known = 1; in.buttons = 80u; in.hearts = 7u; in.per_row = 7u; in.pad = 1;
    oracles_floating_layout(&in, &l);
    CHECK(l.count == 3u && l.radius == 2u && panel_is(&l.panels[0], ORACLES_FLOATING_HEARTS, 4, 4, 60, 12));
    CHECK(panel_is(&l.panels[1], ORACLES_FLOATING_MONEY, 256 - 4 - 28, 4, 28, 20) && panel_is(&l.panels[2], ORACLES_FLOATING_BUTTONS, 4, 144 - 4 - 20, 84, 20));
    CHECK(l.panels[0].anchor == 0u && l.panels[1].anchor == ORACLES_FLOATING_RIGHT && l.panels[2].anchor == ORACLES_FLOATING_BOTTOM);
    /* A map of no layout: the whole bar at the top's middle */
    in.known = 0;
    oracles_floating_layout(&in, &l);
    CHECK(l.count == 1u && panel_is(&l.panels[0], ORACLES_FLOATING_BAR, (256 - 164) / 2, 4, 164, 20) && l.panels[0].top && l.panels[0].anchor == ORACLES_FLOATING_CENTRE);
    /* Near at half the game's scale: the layout of a surface twice as large, the pad's six tiles at the bottom-left */
    memset(&in, 0, sizeof in);
    in.width = 2u * 213u; in.height = 2u * 160u; in.known = 1; in.buttons = 80u; in.hearts = 7u; in.per_row = 7u; in.pad = 1;
    oracles_floating_layout(&in, &l);
    CHECK(l.count == 8u && panel_is(&l.panels[0], ORACLES_FLOATING_HEARTS, 6, 6, 60, 12) && l.panels[0].anchor == 0u);
    CHECK(panel_is(&l.panels[1], ORACLES_FLOATING_MONEY, 426 - 6 - 28, 6, 28, 20) && l.panels[1].anchor == ORACLES_FLOATING_RIGHT);
    for (unsigned i = 2; i < 8u; i++) CHECK(l.panels[i].kind == ORACLES_FLOATING_TILE && l.panels[i].anchor == ORACLES_FLOATING_BOTTOM);
    /* On the screen: a whole number of pixels a pixel from twice on, the near view's scale halved to the nearest, a half
     * up (7 at 1920x1080: 4; 3 at 640x480: 2), each panel as far from its corner's edges in them as in the layout */
    CHECK(oracles_floating_screen_unit(1792.0f, 512u) == 4.0f && oracles_floating_screen_unit(639.0f, 426u) == 2.0f && oracles_floating_screen_unit(1536.0f, 512u) == 3.0f);
    CHECK(oracles_floating_screen_unit(512.0f, 512u) == 1.0f && oracles_floating_screen_unit(256.0f, 512u) == 0.5f);
    CHECK(oracles_floating_screen_unit(640.0f, 512u) == 1.0f && oracles_floating_screen_unit(1920.0f, 512u) == 4.0f);   /* Fill's 2.5 and 7.5 */
    float place[4];
    const int32_t money[4] = { l.panels[1].x, l.panels[1].y, l.panels[1].w, l.panels[1].h }, tile[4] = { l.panels[7].x, l.panels[7].y, l.panels[7].w, l.panels[7].h };
    oracles_floating_screen_place(money, l.panels[1].anchor, 426u, 320u, 639.0f, 480.0f, place);
    CHECK(place[0] == 639.0f - 6.0f * 2.0f - 28.0f * 2.0f && place[1] == 12.0f && place[2] == 56.0f && place[3] == 40.0f);
    oracles_floating_screen_place(tile, l.panels[7].anchor, 426u, 320u, 639.0f, 480.0f, place);
    CHECK(place[0] == (float)tile[0] * 2.0f && place[1] + place[3] == 480.0f - (float)(320 - tile[1] - tile[3]) * 2.0f);
    in.known = 0;
    oracles_floating_layout(&in, &l);
    CHECK(l.count == 1u && l.panels[0].anchor == ORACLES_FLOATING_CENTRE);
    const int32_t bar[4] = { l.panels[0].x, l.panels[0].y, l.panels[0].w, l.panels[0].h };
    oracles_floating_screen_place(bar, l.panels[0].anchor, 426u, 320u, 639.0f, 480.0f, place);
    CHECK(place[0] == 319.0f - 82.0f * 2.0f && place[2] == 164.0f * 2.0f);
    /* The touch controls: the pause at the top-right, the pad at the bottom-left; B and A under the hearts */
    memset(&in, 0, sizeof in);
    in.width = 480u; in.height = 270u; in.known = 1; in.buttons = 80u; in.hearts = 10u; in.per_row = 7u; in.pad = 1;
    in.avoid_count = 2u;
    const int32_t pause[4] = { 440, 4, 36, 30 }, cross[4] = { 0, 180, 90, 90 };
    memcpy(in.avoid[0], cross, sizeof cross);
    memcpy(in.avoid[1], pause, sizeof pause);
    for (int keyboard = 1; keyboard >= 0; keyboard--) {   /* the keyboard's block or the pad: B and A alone either way */
        in.keyboard = keyboard;
        oracles_floating_layout(&in, &l);
        CHECK(l.count == 3u && panel_is(&l.panels[1], ORACLES_FLOATING_MONEY, 440 - 6 - 28, 6, 28, 20));
        CHECK(panel_is(&l.panels[2], ORACLES_FLOATING_BUTTONS, 6, 6 + 20 + 4, 84, 20));
    }
    for (unsigned i = 0; i < l.count; i++)
        for (unsigned a = 0; a < 2u; a++) {
            const int32_t *r = in.avoid[a];
            CHECK(!(l.panels[i].x < r[0] + r[2] && r[0] < l.panels[i].x + l.panels[i].w && l.panels[i].y < r[1] + r[3] && r[1] < l.panels[i].y + l.panels[i].h));
        }
    /* The pad's cells, and a panel's rounded corners: of radius 2, two pixels of its first line and of its first column
     * left out, the next and the one diagonal to the corner on the edge; the middle inside. */
    unsigned column, row;
    oracles_floating_pad_cell(ORACLES_FLOATING_SLOT_B, &column, &row);
    CHECK(column == 1u && row == 2u);
    oracles_floating_pad_cell(ORACLES_FLOATING_SLOT_A, &column, &row);
    CHECK(column == 2u && row == 1u);
    oracles_floating_keys_cell(ORACLES_FLOATING_SLOT_B, &column, &row);
    CHECK(column == 0u && row == 2u);
    oracles_floating_keys_cell(ORACLES_FLOATING_SLOT_A, &column, &row);
    CHECK(column == 1u && row == 2u);
    CHECK(oracles_floating_panel_pixel(28, 20, 2, 1, 0) == 0u && oracles_floating_panel_pixel(28, 20, 2, 0, 1) == 0u && oracles_floating_panel_pixel(28, 20, 2, 0, 0) == 0u);
    CHECK(oracles_floating_panel_pixel(28, 20, 2, 2, 0) == 1u && oracles_floating_panel_pixel(28, 20, 2, 0, 2) == 1u && oracles_floating_panel_pixel(28, 20, 2, 1, 1) == 1u);
    CHECK(oracles_floating_panel_pixel(28, 20, 2, 27, 19) == 0u && oracles_floating_panel_pixel(28, 20, 2, 14, 10) == 2u && oracles_floating_panel_pixel(28, 20, 2, 14, 19) == 1u);
    unsigned inside = 0, edge = 0;
    for (unsigned y = 0; y < 20u; y++) for (unsigned x = 0; x < 28u; x++) { const unsigned k = oracles_floating_panel_pixel(28, 20, 2, x, y); inside += k == 2u; edge += k == 1u; }
    CHECK(edge == 84u && inside == 26u * 18u - 4u);   /* the inside's corners: the pixel diagonal to each is the edge's */
}

int main(void)
{
    geometry();
    layouts();
    composed(256u, 144u);
    composed(213u, 160u);
    composed(384u, 216u);
    composed(480u, 360u);
    dark();
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_hud_floating: ok\n");
    return 0;
}
