/* The status bar laid out across the HUD band (hud=wide), without a ROM: where
 * its blocks go in each size of the view, the layout its map signs and the
 * image confirms, and the composition, every block the core's own pixels and
 * nothing of the world band touched. */
#include "compositor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

#define HOTBAR 96u
#define FILL 0xff00f0e0u
#define FILL_RIGHT 0xff00f0e1u

/* Every size the view takes: the hearts on one row from medium 4:3's 320 up, the hotbar against B but in near 4:3, the
 * margin at each edge eight pixels or what the blocks leave. */
static void places(void)
{
    OraclesEnhancedHudPlace p = oracles_enhanced_hud_place(256u, ORACLES_ENHANCED_BAR_BUTTONS, HOTBAR);
    CHECK(p.hearts == 56u && !p.one_row && p.margin == 0u && p.hearts_at == 0u && p.money_at == 56u && p.buttons_at == 176u && p.hotbar && p.hotbar_at == 80u);   /* 80 + 96 + 80 */
    p = oracles_enhanced_hud_place(256u, ORACLES_ENHANCED_BAR_BUTTONS_TIGHT, HOTBAR);
    CHECK(p.hearts == 64u && !p.one_row && p.margin == 0u && p.money_at == 64u && p.buttons_at == 184u && p.hotbar && p.hotbar_at == 88u);   /* 88 + 96 + 72 */
    p = oracles_enhanced_hud_place(213u, ORACLES_ENHANCED_BAR_BUTTONS, HOTBAR);
    CHECK(!p.one_row && p.margin == 8u && p.hearts_at == 8u && p.money_at == 64u && p.buttons_at == 125u && !p.hotbar);   /* 37 between the two blocks */
    p = oracles_enhanced_hud_place(213u, ORACLES_ENHANCED_BAR_BUTTONS_TIGHT, HOTBAR);
    CHECK(!p.one_row && p.margin == 8u && p.money_at == 72u && p.buttons_at == 133u && !p.hotbar);
    p = oracles_enhanced_hud_place(320u, ORACLES_ENHANCED_BAR_BUTTONS, HOTBAR);
    CHECK(p.one_row && p.margin == 4u && p.hearts_at == 4u && p.money_at == 116u && p.buttons_at == 236u && p.hotbar && p.hotbar_at == 140u);   /* 4 + 136 + 96 + 80 + 4 */
    p = oracles_enhanced_hud_place(320u, ORACLES_ENHANCED_BAR_BUTTONS_TIGHT, HOTBAR);
    CHECK(p.one_row && p.margin == 0u && p.money_at == 128u && p.buttons_at == 248u && p.hotbar && p.hotbar_at == 152u);   /* 152 + 96 + 72: exactly */
    const unsigned wide[] = { 384u, 412u, 426u, 480u };
    for (unsigned i = 0; i < sizeof wide / sizeof wide[0]; i++)
        for (unsigned tight = 0; tight < 2u; tight++) {
            p = oracles_enhanced_hud_place(wide[i], tight ? ORACLES_ENHANCED_BAR_BUTTONS_TIGHT : ORACLES_ENHANCED_BAR_BUTTONS, HOTBAR);
            CHECK(p.one_row && p.margin == 8u && p.hearts_at == 8u && p.buttons_at + p.buttons + 8u == wide[i] && p.hotbar && p.hotbar_at + HOTBAR == p.buttons_at
                  && p.money_at == 8u + 2u * p.hearts);
        }
    /* The hotbar's width counts whether it is drawn or not: without it, near would hold the hearts on one row. */
    CHECK(!oracles_enhanced_hud_place(256u, ORACLES_ENHANCED_BAR_BUTTONS, HOTBAR).one_row && oracles_enhanced_hud_place(256u, ORACLES_ENHANCED_BAR_BUTTONS, 0).one_row);
}

/* A bar whose frame tiles 5 to 8 are each one colour index (5: 1, 6: 2, 7: 3, 8: 1 on its left half), palette 0. */
static uint8_t tiles[64], attributes[64], vram0[0x2000], vram1[0x2000], palettes[64];
static const OraclesEnhancedBar bar = { tiles, attributes, vram0, vram1, palettes, 0x10u };

static void map(OraclesEnhancedBarLayout layout)
{
    memset(tiles, 0, sizeof tiles);
    memset(attributes, 0x80, sizeof attributes);
    static const uint8_t cells[4][4][4] = {
        [ORACLES_ENHANCED_BAR_NORMAL] = { { 0, 4, 0x06, 0xe0 }, { 1, 4, 0x06, 0xa0 }, { 0, 9, 0x06, 0xe0 }, { 1, 9, 0x06, 0xa0 } },
        [ORACLES_ENHANCED_BAR_TIGHT] = { { 0, 4, 0x07, 0xc0 }, { 1, 4, 0x07, 0x80 }, { 0, 8, 0x08, 0xa0 }, { 1, 8, 0x06, 0xa0 } },
        [ORACLES_ENHANCED_BAR_BIGGORON] = { { 0, 1, 0x05, 0x80 }, { 1, 1, 0x06, 0x80 }, { 0, 7, 0x08, 0xa0 }, { 1, 7, 0x06, 0xa0 } },
    };
    if (layout == ORACLES_ENHANCED_BAR_NONE) return;
    for (unsigned i = 0; i < 4u; i++) {
        tiles[cells[layout][i][0] * 32u + cells[layout][i][1]] = cells[layout][i][2];
        attributes[cells[layout][i][0] * 32u + cells[layout][i][1]] = cells[layout][i][3];
    }
}

/* The core's top sixteen lines as the bar's map draws them, plain RGB. */
static void draw(uint32_t *core)
{
    for (unsigned y = 0; y < 16u; y++)
        for (unsigned x = 0; x < 160u; x++) {
            const uint16_t c = oracles_enhanced_bar_pixel(&bar, x, y, NULL);
            core[y * 160u + x] = oracles_enhanced_faded_pixel(NULL, c, 0);
        }
}

static void layouts(uint32_t *core)
{
    for (unsigned line = 0; line < 8u; line++) {
        vram0[5u * 16u + line * 2u] = 0xffu;                                   /* colour 1 */
        vram0[6u * 16u + line * 2u + 1u] = 0xffu;                              /* colour 2 */
        vram0[7u * 16u + line * 2u] = vram0[7u * 16u + line * 2u + 1u] = 0xffu;   /* colour 3 */
        vram0[8u * 16u + line * 2u] = 0xf0u;                                   /* colour 1 on the left half: a flip shows */
    }
    for (unsigned i = 0; i < 32u; i++) palettes[i] = (uint8_t)(i * 37u + 11u);
    for (unsigned l = ORACLES_ENHANCED_BAR_NONE; l <= ORACLES_ENHANCED_BAR_BIGGORON; l++) {
        map((OraclesEnhancedBarLayout)l);
        CHECK(oracles_enhanced_bar_layout(&bar) == (OraclesEnhancedBarLayout)l);
        draw(core);
        for (unsigned k = ORACLES_ENHANCED_BAR_NORMAL; k <= ORACLES_ENHANCED_BAR_BIGGORON; k++)
            CHECK(oracles_enhanced_bar_layout_shown(&bar, (OraclesEnhancedBarLayout)k, NULL, NULL, core) == (k == l));
    }
    /* One cell's attributes changed (a flip): another frame; one pixel of the image changed (a sprite over the frame): not shown. */
    map(ORACLES_ENHANCED_BAR_TIGHT);
    attributes[8] = 0x80u;
    CHECK(oracles_enhanced_bar_layout(&bar) == ORACLES_ENHANCED_BAR_NONE);
    map(ORACLES_ENHANCED_BAR_TIGHT);
    draw(core);
    core[9u * 160u + 66u] ^= 0x00ffffffu;
    CHECK(!oracles_enhanced_bar_layout_shown(&bar, ORACLES_ENHANCED_BAR_TIGHT, NULL, NULL, core));
    CHECK(!oracles_enhanced_bar_layout_shown(&bar, ORACLES_ENHANCED_BAR_NONE, NULL, NULL, core));
}

/* Oracles Combined writes the current game's icon, tile $3f, in the map's first row at column 11 whatever the layout: not
 * a cell of any frame, and always in the money's block, which carries it. */
static void game_icon(uint32_t *core)
{
    for (unsigned l = ORACLES_ENHANCED_BAR_NORMAL; l <= ORACLES_ENHANCED_BAR_BIGGORON; l++) {
        map((OraclesEnhancedBarLayout)l);
        tiles[11] = 0x3fu;
        for (unsigned line = 0; line < 8u; line++) vram0[0x3fu * 16u + line * 2u] = 0xffu;
        CHECK(oracles_enhanced_bar_layout(&bar) == (OraclesEnhancedBarLayout)l);
        draw(core);
        for (unsigned k = ORACLES_ENHANCED_BAR_NORMAL; k <= ORACLES_ENHANCED_BAR_BIGGORON; k++)
            CHECK(oracles_enhanced_bar_layout_shown(&bar, (OraclesEnhancedBarLayout)k, NULL, NULL, core) == (k == l));
        unsigned drawn = 0;
        for (unsigned y = 0; y < 8u; y++) for (unsigned x = 0; x < 8u; x++) drawn += core[y * 160u + 11u * 8u + x] != core[y * 160u + 12u * 8u + x];
        CHECK(drawn == 64u);
        const unsigned buttons[] = { ORACLES_ENHANCED_BAR_BUTTONS, ORACLES_ENHANCED_BAR_BUTTONS_TIGHT };
        for (unsigned b = l == ORACLES_ENHANCED_BAR_TIGHT; b < (l == ORACLES_ENHANCED_BAR_NORMAL ? 1u : 2u); b++) {
            CHECK(buttons[b] <= 11u * 8u && 12u * 8u <= buttons[b] + ORACLES_ENHANCED_BAR_MONEY);
            const OraclesEnhancedHudPlace p = oracles_enhanced_hud_place(480u, buttons[b], HOTBAR);
            static uint32_t band[16u * 480u];
            OraclesEnhancedCompose in;
            memset(&in, 0, sizeof in);
            in.mode = ORACLES_ENHANCED_WORLD;
            in.size = (OraclesEnhancedSize){ 480u, 270u };
            in.hud_place = &p;
            oracles_enhanced_compose_hud(core, &in, band);
            for (unsigned y = 0; y < 8u; y++)
                for (unsigned x = 0; x < 8u; x++)
                    CHECK(band[y * 480u + p.money_at + 11u * 8u - buttons[b] + x] == core[y * 160u + 11u * 8u + x]);
        }
    }
}

/* The bar laid out in `width`: the hearts, the money and B and A where the place says, each the core's own pixels, the
 * hearts' second row beside the first on the middle eight lines; the rest of the band the fill, the left's to the
 * middle; nothing under the band changes from the bar centred. */
static void composed(uint32_t *core, unsigned width, unsigned height, unsigned buttons)
{
    for (unsigned i = 0; i < 160u * 144u; i++) core[i] = 0xff000000u | (i * 2654435761u >> 8);
    uint32_t fill[32];
    for (unsigned i = 0; i < 32u; i++) fill[i] = i < 16u ? FILL : FILL_RIGHT;
    const OraclesEnhancedHudPlace p = oracles_enhanced_hud_place(width, buttons, HOTBAR);
    uint32_t *centred = malloc((size_t)width * height * sizeof *centred), *laid = malloc((size_t)width * height * sizeof *laid);
    OraclesEnhancedCompose in;
    memset(&in, 0, sizeof in);
    in.mode = ORACLES_ENHANCED_WORLD;
    in.border = 0xff101010u;
    in.size = (OraclesEnhancedSize){ width, height };
    in.window_world_left = 1000; in.window_world_top = 500;
    in.world_left = 1000 - (int32_t)(width - 160u) / 2; in.world_top = 500 - 40;
    in.hud_fill = fill;
    const unsigned uncovered = oracles_enhanced_compose(core, &in, centred);
    in.hud_place = &p;
    CHECK(oracles_enhanced_compose(core, &in, laid) == uncovered);
    CHECK(memcmp(centred + 16u * width, laid + 16u * width, (size_t)width * (height - 16u) * sizeof *laid) == 0);
    unsigned wrong = 0;
    const unsigned hearts_x = 160u - p.hearts;
    for (unsigned y = 0; y < 16u; y++)
        for (unsigned x = 0; x < width; x++) {
            uint32_t expect = x < width / 2u ? FILL : FILL_RIGHT;
            if (x >= p.buttons_at + p.buttons) {}
            else if (x >= p.buttons_at) expect = core[y * 160u + x - p.buttons_at];
            else if (x >= p.money_at && x < p.money_at + ORACLES_ENHANCED_BAR_MONEY) expect = core[y * 160u + p.buttons + x - p.money_at];
            else if (x < p.hearts_at) {}
            else if (!p.one_row && x < p.hearts_at + p.hearts) expect = core[y * 160u + hearts_x + x - p.hearts_at];
            else if (p.one_row && x < p.hearts_at + 2u * p.hearts && y >= 4u && y < 12u)
                expect = core[(y - 4u + (x - p.hearts_at >= p.hearts ? 8u : 0u)) * 160u + hearts_x + (x - p.hearts_at) % p.hearts];
            wrong += laid[y * width + x] != expect;
        }
    CHECK(wrong == 0);
    free(centred); free(laid);
}

int main(void)
{
    uint32_t *core = calloc(160u * 144u, sizeof *core);
    places();
    layouts(core);
    game_icon(core);
    composed(core, 256u, 144u, ORACLES_ENHANCED_BAR_BUTTONS);
    composed(core, 256u, 144u, ORACLES_ENHANCED_BAR_BUTTONS_TIGHT);
    composed(core, 213u, 160u, ORACLES_ENHANCED_BAR_BUTTONS);
    composed(core, 320u, 240u, ORACLES_ENHANCED_BAR_BUTTONS_TIGHT);
    composed(core, 480u, 270u, ORACLES_ENHANCED_BAR_BUTTONS);
    free(core);
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_hud_wide: ok\n");
    return 0;
}
