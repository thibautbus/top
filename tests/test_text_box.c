/* The text box drawn by the host (text_box.h) and the host's lines (guest.h): the fixed advance against the game's own
 * layout, the model of the game's box read from its cells, the box drawn from it against the software PPU's image of
 * the same VRAM, at the game's width and wider, and the lines kept from a synthetic text program (no game data) on both
 * cores.  At each routine's address of the game's tables,
 *   drawLineOfText:     ld hl,$c100 ; ld de,w7LineTextBuffer ; ld bc,w7TextGfxBuffer ; loop: ld a,(hli) ; or a ; ret z ;
 *                       call setLineTextBuffers ; call drawCell ; jr loop
 *   setLineTextBuffers: ld (de),a ; the attribute $80 at e + $10 ; inc e ; ret
 *   drawCell ($7000):   the character's 16 rows from a copy of the font at $1000, its ink in the low plane, at bc
 *   dmaTextGfxBuffer:   add $94 ; ld d,a ; an entry of wVBlankFunctionQueue as queueDmaTransfer writes it, from
 *                       w7TextGfxBuffer to d:$00, VRAM bank 0, 512 bytes ; hVBlankFunctionQueueTail at 8 ; ret
 *   vblankDmaFunction:  pop hl ; SVBK, HDMA1, HDMA2, VBK, HDMA3, HDMA4 and HDMA5 from the entry ; the queue emptied
 * called from a loop as $c0f0 says (1 a line from $c100, 2 its copy to the top line, 3 to the bottom, 4 an entry of the
 * queue the test wrote, a character's cell as the fan games' dmaTextCharacter queues it), and from the
 * vblank's handler with the queue's entry pushed, as runVBlankFunctions does. */
#include "core.h"
#include "guest.h"
#include "ppu.h"
#include "text_box.h"
#include "view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

#define FONT_COPY 0x1000u
#define DRAW_CELL 0x7000u
#define HANDLER 0x0200u
#define CELL_BYTES 32u
#define LINE_BYTES 0x200u

static uint32_t seed = 12345u;
static uint8_t next_byte(void) { seed = seed * 1103515245u + 12345u; return (uint8_t)(seed >> 16); }

/* Pixel x, row r of a line's tiles: its colour (0 to 3). */
static unsigned pixel(const uint8_t *gfx, unsigned x, unsigned r)
{
    const unsigned at = x / 8u * 32u + 2u * r, bit = 0x80u >> (x % 8u);
    return ((gfx[at] & bit) ? 1u : 0u) | ((gfx[at + 1u] & bit) ? 2u : 0u);
}

/* The fixed advance lays a line out where the game draws it: character k in tiles 2k and 2k + 1, every column of its
 * cell, symbols and blanks alike; wider, the same, then blank. */
static void test_fixed_advance(void)
{
    OraclesTextFont font;
    oracles_guest_text_box_fixed_font(&font);
    OraclesGuestCompactCharacter c[16];
    uint8_t game[LINE_BYTES];
    memset(game, 0xff, sizeof game);
    for (unsigned k = 0; k < 16u; k++) {
        memset(&c[k], 0, sizeof c[k]);
        c[k].character = k == 3u ? 0x06u : k == 5u ? ' ' : (uint8_t)('A' + k);
        c[k].whole = c[k].character < ORACLES_TEXT_FIRST_CHARACTER;
        c[k].attribute = (uint8_t)(0x80u | (k & 3u));
        for (unsigned i = 0; i < CELL_BYTES; i++) c[k].cell[i] = k == 5u ? 0xffu : next_byte();
        memcpy(game + k * CELL_BYTES, c[k].cell, CELL_BYTES);
    }
    OraclesGuestCompactLine line;
    uint8_t out[ORACLES_GUEST_TEXT_BOX_COLUMNS_MAX * CELL_BYTES];
    oracles_guest_compact_layout_at(c, 16, &font, 128, &line);
    oracles_guest_compact_compose_at(&line, c, 128, out);
    CHECK(line.count == 16 && line.overflows == 0 && memcmp(out, game, LINE_BYTES) == 0);
    for (unsigned k = 0; k < 16u; k++) CHECK(line.start[k] == 8u * k && line.attribute[k] == c[k].attribute);
    memset(out, 0, sizeof out);
    oracles_guest_compact_layout_at(c, 16, &font, 192, &line);
    oracles_guest_compact_compose_at(&line, c, 192, out);
    CHECK(memcmp(out, game, LINE_BYTES) == 0);
    for (unsigned i = LINE_BYTES; i < 24u * CELL_BYTES; i++) CHECK(out[i] == 0xffu);
    oracles_guest_compact_layout_at(c, 16, &font, 64, &line);   /* a narrower line holds eight */
    CHECK(line.count == 8 && line.overflows == 8);
}

/* The fixed advance of the wide box's classic text (oracles_text_font_fixed): from a font of the image, a character a
 * tile, a blank one drawing nothing; at 192 pixels a line of 24 characters, laid out where the game would draw them
 * past its sixteen, grouped in sixteen steps, the blanks joining the character before them first, the step boundaries
 * past 127. */
static void test_fixed_advance_wide(void)
{
    OraclesTextFont font, fixed;
    memset(&font, 0, sizeof font);
    for (unsigned ch = ORACLES_TEXT_FIRST_CHARACTER; ch < 0x100u; ch++) {
        font.left[ch] = 1;
        font.width[ch] = ch == ' ' ? 0 : (uint8_t)(3u + ch % 4u);
        font.advance[ch] = ch == ' ' ? ORACLES_TEXT_SPACE_ADVANCE : (uint8_t)(font.width[ch] + 1u);
    }
    oracles_text_font_fixed(&font, &fixed);
    CHECK(fixed.width[' '] == 0 && fixed.advance[' '] == 8 && fixed.width['A'] == 8 && fixed.left['A'] == 0 && fixed.advance['A'] == 8);
    static const char text[] = "It is a wide line of 24.";
    OraclesGuestCompactCharacter c[24];
    for (unsigned k = 0; k < 24u; k++) {
        memset(&c[k], 0, sizeof c[k]);
        c[k].character = (uint8_t)text[k];
        c[k].attribute = 0x80;
        for (unsigned i = 0; i < CELL_BYTES; i++) c[k].cell[i] = text[k] == ' ' ? 0xffu : next_byte();
    }
    OraclesGuestCompactLine line;
    static uint8_t out[ORACLES_GUEST_TEXT_BOX_COLUMNS_MAX * CELL_BYTES];
    oracles_guest_compact_layout_at(c, 24, &fixed, 192, &line);
    oracles_guest_compact_compose_at(&line, c, 192, out);
    CHECK(line.count == 24 && line.overflows == 0 && line.columns == 24 && line.steps == 16);
    int cells = 1;
    for (unsigned k = 0; k < 24u; k++) {
        CHECK(line.start[k] == 8u * k);
        cells &= !memcmp(out + k * CELL_BYTES, c[k].cell, CELL_BYTES);
    }
    CHECK(cells);
    /* The five blanks join the character before them: "It ", "is ", "a ", "wide ", "line ". */
    CHECK(line.first[0] == 0 && line.first[1] == 1 && line.first[2] == 3);
    CHECK(oracles_guest_compact_boundary(&line, 15) == 192u && oracles_guest_compact_boundary(&line, 14) == 184u);
    oracles_guest_compact_layout_at(c, 24, &fixed, 128, &line);   /* the game's width holds sixteen */
    CHECK(line.count == 16 && line.overflows == 8);
}

/* Text size Large in whole screen pixels: the factor k + ceil(k / 2) of the frame's whole scale, the box centred on
 * itself across, against its edge; where it fits, by view and screen (the far view, 480 x 270 at four times on
 * 1920 x 1080; 480 x 360 once on 640 x 480; Near's 256 at seven times, which holds the game's box, not the wide one);
 * Very large at 2 k, falling to Large where it does not fit. */
static void test_enlarge(void)
{
    int32_t out[4];
    unsigned f = 0;
    const unsigned large = ORACLES_TEXT_BOX_ENLARGE_LARGE, xlarge = ORACLES_TEXT_BOX_ENLARGE_XLARGE;
    static const unsigned factors[8][2] = { { 1, 2 }, { 2, 3 }, { 3, 5 }, { 4, 6 }, { 5, 8 }, { 6, 9 }, { 7, 11 }, { 8, 12 } };
    for (unsigned i = 0; i < 8u; i++) {
        oracles_text_box_enlarge(0, 0, 4000, 4000, factors[i][0], 10, 10, 20, 10, large, 0, out, &f);
        CHECK(f == factors[i][1]);
        oracles_text_box_enlarge(0, 0, 4000, 4000, factors[i][0], 10, 10, 20, 10, xlarge, 0, out, &f);
        CHECK(f == 2u * factors[i][0]);
    }
    /* Far 16:9 on 1920 x 1080, k = 4: the wide box (208 x 40 at 136, 16) at six pixels a pixel, 1248 x 240. */
    CHECK(oracles_text_box_enlarge(0, 0, 1920, 1080, 4, 136, 16, 208, 40, large, 0, out, &f) && f == 6);
    CHECK(out[0] == (136 + 104) * 4 - 624 && out[1] == 64 && out[2] == 1248 && out[3] == 240);
    CHECK(oracles_text_box_enlarge(0, 0, 1920, 1080, 4, 136, 200, 208, 40, large, 1, out, &f) && out[1] + out[3] == 240 * 4);   /* against its bottom edge */
    CHECK(oracles_text_box_enlarge(0, 0, 1920, 1080, 4, 136, 16, 208, 40, xlarge, 0, out, &f) && f == 8 && out[2] == 1664);
    /* Far 4:3 on 640 x 480, k = 1 (80, 60 the frame's corner): twice, 416 of the frame's 480. */
    CHECK(oracles_text_box_enlarge(80, 60, 480, 360, 1, 136, 16, 208, 40, large, 0, out, &f) && f == 2 && out[2] == 416);
    /* Near 16:9 on 1920 x 1080 (k = 7, 1792 x 1008): the game's box at eleven, 1584, Very large's 2016 falling to it; the
     * wide one, 2288, does not fit. */
    CHECK(oracles_text_box_enlarge(64, 36, 1792, 1008, 7, 56, 16, 144, 40, large, 0, out, &f) && out[2] == 1584);
    CHECK(oracles_text_box_enlarge(64, 36, 1792, 1008, 7, 56, 16, 144, 40, xlarge, 0, out, &f) && f == 11 && out[2] == 1584);
    CHECK(!oracles_text_box_enlarge(64, 36, 1792, 1008, 7, 24, 16, 208, 40, xlarge, 0, out, &f));
    /* Near 4:3 on 640 x 480 (213 x 160 at three times): neither, 720 and 1040 wide. */
    CHECK(!oracles_text_box_enlarge(0, 0, 639, 480, 3, 34, 16, 144, 40, large, 0, out, &f));
}

/* The large menus: the frame (160 x 144) or its union with the wide box (208), shown alone at its own scale, widened for
 * the box at Large or Very large only where that scale holds it; never a lower scale than Normal's, on any screen, view,
 * screen margin, Scaling, box and size, and where widened the box enlarged fits it at a factor above the scale. */
static void test_reserve(void)
{
    const unsigned large = ORACLES_TEXT_BOX_ENLARGE_LARGE, xlarge = ORACLES_TEXT_BOX_ENLARGE_XLARGE;
    static const uint32_t screens[][2] = { { 640, 480 }, { 800, 480 }, { 1280, 720 }, { 1920, 1080 }, { 1280, 960 }, { 1440, 1080 }, { 1240, 1080 },
                                           { 2560, 1440 }, { 3840, 2160 } };
    static const uint32_t views[][2] = { { 256, 144 }, { 213, 160 }, { 384, 216 }, { 320, 240 }, { 480, 270 }, { 480, 360 }, { 426, 320 }, { 412, 360 } };
    static const uint32_t margins[] = { 0, 48, 96, 192 };   /* the band's height less the screen margin, top and bottom */
    unsigned kept = 0, widened = 0, fits = 0, cases = 0;
    for (unsigned sc = 0; sc < sizeof screens / sizeof screens[0]; sc++)
        for (unsigned m = 0; m < sizeof margins / sizeof margins[0]; m++)
            for (unsigned vw = 0; vw < sizeof views / sizeof views[0]; vw++)
                for (unsigned wide = 0; wide < 2u; wide++)
                    for (unsigned size = large; size <= xlarge; size++)
                        for (int fill = 0; fill < 2; fill++) {
                            const uint32_t area_w = screens[sc][0], area_h = screens[sc][1] - margins[m], frame_w = views[vw][0];
                            const uint32_t box_w = wide ? 208u : 144u, part = wide ? 208u : 160u;
                            if (part > frame_w) continue;
                            const double normal = oracles_text_box_part_scale(part, 144, area_w, area_h, fill);
                            if (normal <= 0.0) continue;
                            uint32_t x = (frame_w - part) / 2u, w = part;
                            oracles_text_box_reserve(frame_w, area_w, area_h, fill, box_w, size, &x, &w, 144);
                            cases++;
                            kept += oracles_text_box_part_scale(w, 144, area_w, area_h, fill) == normal && x + w <= frame_w && w >= part
                                    && 2u * x + w == frame_w - (frame_w - part) % 2u;
                            if (w == part) continue;
                            widened++;
                            /* The part as the presenter shows it, the box centred in the frame, its top row at 16. */
                            const double shown_w = (double)w * normal, shown_h = 144.0 * normal;
                            int32_t out[4];
                            unsigned f = 0;
                            fits += oracles_text_box_enlarge(0, 0, shown_w, shown_h, normal, (frame_w - box_w) / 2u - x, 16, box_w, 40, size, 0, out, &f)
                                    && f > (unsigned)(normal + 1e-6);
                        }
    CHECK(cases > 1000u && kept == cases && widened > 50u && fits == widened);
    /* The handheld of 640 x 480 (the RG DS): the frame at three times with Normal; Large and Very large do not fit at three
     * (720, 864) and leave it so, the box at its size, 432; the wide union (208) at three in Far 4:3, its box 624. */
    uint32_t x = 160, w = 160;
    oracles_text_box_reserve(480, 640, 480, 0, 144, large, &x, &w, 144);
    CHECK(w == 160u && x == 160u);
    oracles_text_box_reserve(480, 640, 480, 0, 144, xlarge, &x, &w, 144);
    CHECK(w == 160u && x == 160u);
    oracles_text_box_reserve(480, 640, 480, 1, 144, xlarge, &x, &w, 144);   /* Fill: 3.33 times, the box at five 720 */
    CHECK(w == 160u && x == 160u);
    x = 136; w = 208;
    oracles_text_box_reserve(480, 640, 480, 0, 208, xlarge, &x, &w, 144);
    CHECK(w == 208u && x == 136u);
    /* 1920 x 1080: the frame at seven times; the game's box at Large, eleven, 1584 screen pixels, the part 228 wide at
     * seven; Very large (2016) falls to it.  The wide union at seven: its box at eleven, 2288, does not fit. */
    x = 160; w = 160;
    oracles_text_box_reserve(480, 1920, 1080, 0, 144, large, &x, &w, 144);
    CHECK(w == 228u && x == 126u && oracles_text_box_part_scale(w, 144, 1920, 1080, 0) == 7.0);
    x = 160; w = 160;
    oracles_text_box_reserve(480, 1920, 1080, 0, 144, xlarge, &x, &w, 144);
    CHECK(w == 228u && x == 126u);
    x = 136; w = 208;
    oracles_text_box_reserve(480, 1920, 1080, 0, 208, large, &x, &w, 144);
    CHECK(w == 208u && x == 136u);
    /* A screen margin of 48 pixels top and bottom on 1920 x 1080: six times, Very large's 1728 fits, the part 288. */
    x = 160; w = 160;
    oracles_text_box_reserve(480, 1920, 984, 0, 144, xlarge, &x, &w, 144);
    CHECK(w == 288u && x == 96u && oracles_text_box_part_scale(w, 144, 1920, 984, 0) == 6.0);
    x = 160; w = 160;
    oracles_text_box_reserve(480, 1920, 1080, 0, 0, xlarge, &x, &w, 144);   /* no box reserved: the part as it is */
    CHECK(w == 160u && x == 160u);
}

/* The pages laid out again, by their physical offsets: the line's start in one of them, or none. */
static void test_in_pages(void)
{
    const uint32_t ranges[6] = { 0x10, 0x20, 0x40, 0x41, 0x1c4000, 0x1c4100 };
    CHECK(!oracles_guest_text_box_in_pages(ranges, 3, 0x0f) && oracles_guest_text_box_in_pages(ranges, 3, 0x10) && oracles_guest_text_box_in_pages(ranges, 3, 0x1f));
    CHECK(!oracles_guest_text_box_in_pages(ranges, 3, 0x20) && oracles_guest_text_box_in_pages(ranges, 3, 0x40) && !oracles_guest_text_box_in_pages(ranges, 3, 0x41));
    CHECK(oracles_guest_text_box_in_pages(ranges, 3, 0x1c40ff) && !oracles_guest_text_box_in_pages(ranges, 3, 0x1c4100) && !oracles_guest_text_box_in_pages(NULL, 0, 0x10));
}

/* The reveal of a line wider than the game's, from the text's state: steps shown whole, the last one shown character by
 * character after the delays of those before it, never past its boundary; a line not revealed shown whole. */
static void test_reveal_pixels(void)
{
    static OraclesGuestTextBoxLine line;
    memset(&line, 0, sizeof line);
    line.columns = 24;
    line.count = 20;
    line.steps = 16;
    /* Characters of 9 pixels apart, steps of one character but the 5th and the 12th of three (characters 4-6, 13-15). */
    unsigned k = 0;
    for (unsigned s = 0; s < 16u; s++) {
        line.first[s] = (uint8_t)k;
        k += s == 4u || s == 11u ? 3u : 1u;
    }
    line.first[16] = (uint8_t)k;
    CHECK(k == 20u);
    for (unsigned n = 0; n < 20u; n++) {
        line.character[n].start = (uint16_t)(9u * n);
        line.character[n].drawn = 6;
        line.character[n].delay = (uint8_t)(n == 13u ? 5u : 3u);
    }
    for (unsigned s = 0; s < 16u; s++) line.boundary[s] = (uint16_t)(s + 1u < 16u ? 9u * line.first[s + 1u] : 8u * 23u);
    CHECK(oracles_guest_text_box_reveal_pixels(&line, 0, 0) == 0);
    CHECK(oracles_guest_text_box_reveal_pixels(&line, 1, 0) == 9u);
    CHECK(oracles_guest_text_box_reveal_pixels(&line, 16, 0) == ORACLES_GUEST_TEXT_BOX_WHOLE);
    /* The 12th step, characters 13 to 15, past the game's 128 pixels: 13 alone, then 14 after 13's five frames, then 15
     * after 14's three; its boundary at 16's start. */
    CHECK(oracles_guest_text_box_reveal_pixels(&line, 12, 0) == 9u * 14u && oracles_guest_text_box_reveal_pixels(&line, 12, 4) == 9u * 14u);
    CHECK(oracles_guest_text_box_reveal_pixels(&line, 12, 5) == 9u * 15u && oracles_guest_text_box_reveal_pixels(&line, 12, 7) == 9u * 15u);
    CHECK(oracles_guest_text_box_reveal_pixels(&line, 12, 8) == 9u * 16u && oracles_guest_text_box_reveal_pixels(&line, 12, 200) == 9u * 16u);
    CHECK(9u * 16u > 128u && oracles_guest_text_box_reveal_pixels(&line, 15, 0) == 9u * 19u);
    /* The model's rows: the line revealed to its reveal, the other whole, a row of no line untouched. */
    OraclesTextBoxModel model;
    memset(&model, 0, sizeof model);
    model.row[0] = (OraclesTextBoxRow){ ORACLES_GUEST_TEXT_BOX_TOP, 0, 16, 128 };
    model.row[2] = (OraclesTextBoxRow){ ORACLES_GUEST_TEXT_BOX_BOTTOM, 0, 16, 128 };
    model.row[3] = (OraclesTextBoxRow){ ORACLES_GUEST_TEXT_BOX_BOTTOM, 1, 16, 128 };
    static OraclesGuestTextBoxLine top;
    top = line;
    const OraclesGuestTextBoxLine *const lines[ORACLES_GUEST_TEXT_BOX_LINES] = { NULL, &top, &line };
    const OraclesGuestTextBoxReveal reveal = { ORACLES_GUEST_TEXT_BOX_BOTTOM, 12, 5 };
    oracles_text_box_reveal(&model, &reveal, lines);
    CHECK(model.row[0].pixels == 8u * 24u && model.row[2].pixels == 9u * 15u && model.row[3].pixels == 9u * 15u && model.row[1].pixels == 0);
}

/* ---- the model and the box drawn ----------------------------------------------------------------- */

#define BOX_ROW 10u
#define BOX_COLUMN 1u
#define MAP 0x1800u

typedef struct scene {
    uint8_t vram[0x4000];
    uint8_t palettes[64];
    OraclesGuestTextBoxLine lines[ORACLES_GUEST_TEXT_BOX_LINES];
    uint8_t tiles[ORACLES_TEXT_BOX_GAME_ROWS * ORACLES_TEXT_BOX_GAME_COLUMNS], attributes[ORACLES_TEXT_BOX_GAME_ROWS * ORACLES_TEXT_BOX_GAME_COLUMNS];
} scene;

static void put_cell(scene *s, unsigned r, unsigned c, uint8_t tile, uint8_t attribute)
{
    s->tiles[r * ORACLES_TEXT_BOX_GAME_COLUMNS + c] = tile;
    s->attributes[r * ORACLES_TEXT_BOX_GAME_COLUMNS + c] = attribute;
}

/* The game's tiles in signed addressing (tile n at $9000 + 16 n): the blank all colour 3, the arrow and the cursor;
 * the two lines' tiles at $9400 and $9600, the host's lines the same; random palettes. */
static void make_scene(scene *s)
{
    memset(s, 0, sizeof *s);
    for (unsigned i = 0; i < 16u; i++) {
        s->vram[0x1000u + 16u * ORACLES_TEXT_BOX_BLANK_TILE + i] = 0xff;
        s->vram[0x1000u + 16u * 0x03u + i] = (uint8_t)(i & 1u ? 0x18u : 0x3cu);
        s->vram[0x1000u + 16u * 0x04u + i] = (uint8_t)(i & 1u ? 0x60u : 0xf0u);
    }
    for (unsigned i = 0; i < sizeof s->palettes; i++) s->palettes[i] = next_byte();
    for (unsigned l = ORACLES_GUEST_TEXT_BOX_TOP; l <= ORACLES_GUEST_TEXT_BOX_BOTTOM; l++) {
        OraclesGuestTextBoxLine *line = &s->lines[l];
        line->known = 1;
        line->columns = ORACLES_GUEST_TEXT_BOX_COLUMNS_MAX;
        for (unsigned i = 0; i < sizeof line->gfx; i++) line->gfx[i] = next_byte();
        for (unsigned j = 0; j < ORACLES_GUEST_TEXT_BOX_COLUMNS_MAX; j++) line->attribute[j] = (uint8_t)(0x80u | (l + j % 2u));
        memcpy(s->vram + (l == ORACLES_GUEST_TEXT_BOX_TOP ? 0x1400u : 0x1600u), line->gfx, LINE_BYTES);
    }
    for (unsigned r = 0; r < ORACLES_TEXT_BOX_GAME_ROWS; r++)
        for (unsigned c = 0; c < ORACLES_TEXT_BOX_GAME_COLUMNS; c++) put_cell(s, r, c, ORACLES_TEXT_BOX_BLANK_TILE, ORACLES_TEXT_BOX_BLANK_ATTRIBUTE);
}

/* Row r of the box shows `shown` columns of line `l`'s half `half`. */
static void show_row(scene *s, unsigned r, unsigned l, unsigned half, unsigned shown)
{
    const unsigned base = l == ORACLES_GUEST_TEXT_BOX_TOP ? ORACLES_TEXT_BOX_TOP_TILES : ORACLES_TEXT_BOX_BOTTOM_TILES;
    for (unsigned j = 0; j < shown; j++) put_cell(s, r, j + 1u, (uint8_t)(base + 2u * j + half), s->lines[l].attribute[j]);
}

/* The box's cells into the map, the PPU's image of it at `scx`, and the host's box drawn from the model of the cells
 * at the game's width: the box's 144 x 40 pixels compared.  Returns the pixels that differ, or -1 when not read. */
static int compare_with_ppu(scene *s, uint8_t scx)
{
    for (unsigned r = 0; r < ORACLES_TEXT_BOX_GAME_ROWS; r++)
        for (unsigned c = 0; c < ORACLES_TEXT_BOX_GAME_COLUMNS; c++) {
            const unsigned cell = MAP + (BOX_ROW + r) * 32u + ((BOX_COLUMN + c) & 31u);
            s->vram[cell] = s->tiles[r * ORACLES_TEXT_BOX_GAME_COLUMNS + c];
            s->vram[0x2000u + cell] = s->attributes[r * ORACLES_TEXT_BOX_GAME_COLUMNS + c];
        }
    static uint8_t oam[160];
    static uint32_t image[ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT];
    static uint8_t obj_palettes[64];
    OraclesPpuInput in;
    memset(&in, 0, sizeof in);
    in.vram = s->vram;
    in.oam = oam;
    in.bg_palettes = s->palettes;
    in.obj_palettes = obj_palettes;
    for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) in.lines[ly] = (OraclesPpuRegs){ 0x81, 0, scx, 0, 0 };
    oracles_ppu_render(&in, image);
    const uint8_t known[ORACLES_GUEST_TEXT_BOX_LINES] = { 1, 1, 1 };
    OraclesTextBoxModel model;
    if (oracles_text_box_read(s->tiles, s->attributes, known, &model) != ORACLES_TEXT_BOX_READ) return -1;
    OraclesTextBoxSource source;
    memset(&source, 0, sizeof source);
    source.vram = s->vram;
    source.signed_tiles = 1;
    source.bg_palettes = s->palettes;
    for (unsigned l = 0; l < ORACLES_GUEST_TEXT_BOX_LINES; l++) source.lines[l] = &s->lines[l];
    static uint32_t box[ORACLES_TEXT_BOX_WIDTH_MAX * ORACLES_TEXT_BOX_HEIGHT_MAX];
    oracles_text_box_draw(&model, &source, 128, 2, box, 144);
    int wrong = 0;
    for (unsigned y = 0; y < 40u; y++)
        for (unsigned x = 0; x < 144u; x++) {
            const unsigned sx = (BOX_COLUMN * 8u + x - scx) & 0xffu, sy = BOX_ROW * 8u + y;
            if (sx >= ORACLES_PPU_WIDTH) continue;
            wrong += box[y * 144u + x] != image[sy * ORACLES_PPU_WIDTH + sx];
        }
    return wrong;
}

static void test_model(void)
{
    static scene s;
    make_scene(&s);
    const uint8_t known[ORACLES_GUEST_TEXT_BOX_LINES] = { 1, 1, 1 };
    OraclesTextBoxModel model;
    /* Two lines revealed, the bottom one to its third column, a choice's cursor over its second, the arrow. */
    show_row(&s, 0, ORACLES_GUEST_TEXT_BOX_TOP, 0, 16);
    show_row(&s, 1, ORACLES_GUEST_TEXT_BOX_TOP, 1, 16);
    show_row(&s, 2, ORACLES_GUEST_TEXT_BOX_BOTTOM, 0, 3);
    show_row(&s, 3, ORACLES_GUEST_TEXT_BOX_BOTTOM, 1, 3);
    put_cell(&s, 3, 2, 0x04, 0x82);
    put_cell(&s, 4, 17, 0x03, 0x80);
    CHECK(oracles_text_box_read(s.tiles, s.attributes, known, &model) == ORACLES_TEXT_BOX_READ);
    CHECK(model.row[0].line == ORACLES_GUEST_TEXT_BOX_TOP && model.row[0].half == 0 && model.row[0].shown == 16);
    CHECK(model.row[3].line == ORACLES_GUEST_TEXT_BOX_BOTTOM && model.row[3].half == 1 && model.row[3].shown == 3 && model.row[4].line == 0);
    CHECK(model.marks == 2 && model.mark[0].anchor == ORACLES_TEXT_BOX_TEXT && model.mark[0].column == 1 && model.mark[0].row == 3);
    CHECK(model.mark[1].anchor == ORACLES_TEXT_BOX_RIGHT && model.mark[1].row == 4 && model.mark[1].tile == 0x03);
    CHECK(compare_with_ppu(&s, 0) == 0);
    CHECK(compare_with_ppu(&s, 4) == 0);   /* the map scrolled: the box across the screen's edge */
    /* Scrolled a tile up (shiftTextboxMapUp): the top line's lower half, the bottom line's two halves, a blank row. */
    make_scene(&s);
    show_row(&s, 0, ORACLES_GUEST_TEXT_BOX_TOP, 1, 16);
    show_row(&s, 1, ORACLES_GUEST_TEXT_BOX_BOTTOM, 0, 9);
    show_row(&s, 2, ORACLES_GUEST_TEXT_BOX_BOTTOM, 1, 9);
    CHECK(oracles_text_box_read(s.tiles, s.attributes, known, &model) == ORACLES_TEXT_BOX_READ && model.row[3].line == 0);
    CHECK(compare_with_ppu(&s, 0) == 0);
    /* What the model does not hold, and a line the host does not know. */
    const uint8_t unknown[ORACLES_GUEST_TEXT_BOX_LINES] = { 1, 1, 0 };
    CHECK(oracles_text_box_read(s.tiles, s.attributes, unknown, &model) == ORACLES_TEXT_BOX_LINE_UNKNOWN);
    put_cell(&s, 1, 12, ORACLES_TEXT_BOX_BLANK_TILE, ORACLES_TEXT_BOX_BLANK_ATTRIBUTE);
    put_cell(&s, 1, 13, 0x60 + 2 * 12, 0x82);   /* after a gap */
    CHECK(oracles_text_box_read(s.tiles, s.attributes, known, &model) == ORACLES_TEXT_BOX_UNEXPECTED);
    put_cell(&s, 1, 13, 0x60 + 2 * 11, 0x82);   /* out of its column */
    CHECK(oracles_text_box_read(s.tiles, s.attributes, known, &model) == ORACLES_TEXT_BOX_UNEXPECTED);
    put_cell(&s, 1, 13, ORACLES_TEXT_BOX_BLANK_TILE, ORACLES_TEXT_BOX_BLANK_ATTRIBUTE);
    put_cell(&s, 2, 4, 0x40 + 2 * 3, 0x81);   /* the other line in a row */
    CHECK(oracles_text_box_read(s.tiles, s.attributes, known, &model) == ORACLES_TEXT_BOX_UNEXPECTED);
    put_cell(&s, 2, 4, 0x60 + 2 * 3 + 1, 0x82);
    put_cell(&s, 4, 5, 0x60, 0x82);   /* a line's tile in the arrow's row */
    CHECK(oracles_text_box_read(s.tiles, s.attributes, known, &model) == ORACLES_TEXT_BOX_UNEXPECTED);
    put_cell(&s, 4, 5, ORACLES_TEXT_BOX_BLANK_TILE, ORACLES_TEXT_BOX_BLANK_ATTRIBUTE);
    put_cell(&s, 2, 0, 0x10, 0x00);   /* the room's tile in the margin: no box */
    CHECK(oracles_text_box_read(s.tiles, s.attributes, known, &model) == ORACLES_TEXT_BOX_NOT_SHOWN);
}

/* Wider and taller: the rows keep their lines, the text runs on in the host's lines, the arrow stays at the right. */
static void test_wide(void)
{
    static scene s;
    make_scene(&s);
    show_row(&s, 0, ORACLES_GUEST_TEXT_BOX_TOP, 0, 16);
    put_cell(&s, 4, 17, 0x03, 0x80);
    const uint8_t known[ORACLES_GUEST_TEXT_BOX_LINES] = { 1, 1, 1 };
    OraclesTextBoxModel model;
    CHECK(oracles_text_box_read(s.tiles, s.attributes, known, &model) == ORACLES_TEXT_BOX_READ);
    model.row[0].shown = 24;   /* a line of the wide box, shown whole */
    model.row[0].pixels = 192;
    OraclesTextBoxSource source;
    memset(&source, 0, sizeof source);
    source.vram = s.vram;
    source.signed_tiles = 1;
    source.bg_palettes = s.palettes;
    for (unsigned l = 0; l < ORACLES_GUEST_TEXT_BOX_LINES; l++) source.lines[l] = &s.lines[l];
    static uint32_t box[ORACLES_TEXT_BOX_WIDTH_MAX * ORACLES_TEXT_BOX_HEIGHT_MAX], blank[64], arrow[64], text[64];
    for (unsigned lines = 2; lines <= 3u; lines++) {
        const unsigned width = 208u, height = (2u * lines + 1u) * 8u;
        oracles_text_box_draw(&model, &source, 192, lines, box, width);
        /* The cells as the game's 128 pixels draw them, one of each kind, against the same cells drawn wide. */
        static uint32_t game[144u * 40u];
        oracles_text_box_draw(&model, &source, 128, 2, game, 144);
        for (unsigned y = 0; y < 8u; y++)
            for (unsigned x = 0; x < 8u; x++) {
                blank[y * 8u + x] = game[y * 144u + x];
                arrow[y * 8u + x] = game[(32u + y) * 144u + 136u + x];
                text[y * 8u + x] = game[y * 144u + 8u + x];
            }
        int same_arrow = 1, same_blank = 1, same_text = 1;
        for (unsigned y = 0; y < 8u; y++)
            for (unsigned x = 0; x < 8u; x++) {
                same_arrow &= box[(height - 8u + y) * width + 200u + x] == arrow[y * 8u + x];
                same_blank &= box[(height - 8u + y) * width + 136u + x] == blank[y * 8u + x] && box[y * width + 200u + x] == blank[y * 8u + x];
                same_text &= box[y * width + 8u + x] == text[y * 8u + x];
            }
        CHECK(same_arrow && same_blank && same_text);
        /* Column 20 of the top line, past the game's 16: the host's line, its pixels in its attribute's palette. */
        const OraclesGuestTextBoxLine *top = &s.lines[ORACLES_GUEST_TEXT_BOX_TOP];
        int column = 1;
        for (unsigned y = 0; y < 8u; y++)
            for (unsigned x = 0; x < 8u; x++) {
                const unsigned index = pixel(top->gfx, 20u * 8u + x, y), at = (top->attribute[20] & 7u) * 8u + index * 2u;
                column &= box[y * width + 21u * 8u + x] == oracles_ppu_rgb555((uint16_t)((s.palettes[at] | s.palettes[at + 1u] << 8) & 0x7fffu));
            }
        CHECK(column);
    }
    /* A reveal stopping past the game's 128 pixels, inside tile column 18: its first two pixels drawn, the others the
     * blank's colour 3, the columns after it blank. */
    model.row[0].pixels = 8u * 18u + 2u;
    oracles_text_box_draw(&model, &source, 192, 2, box, 208);
    const OraclesGuestTextBoxLine *top = &s.lines[ORACLES_GUEST_TEXT_BOX_TOP];
    int cut = 1;
    for (unsigned y = 0; y < 8u; y++)
        for (unsigned x = 0; x < 16u; x++) {
            const unsigned at = 18u * 8u + x, index = x < 2u ? pixel(top->gfx, at, y) : 3u;
            const unsigned p = (x < 8u ? top->attribute[at / 8u] & 7u : ORACLES_TEXT_BOX_BLANK_ATTRIBUTE & 7u) * 8u + index * 2u;   /* past it, the box's blank */
            cut &= box[y * 208u + 8u + at] == oracles_ppu_rgb555((uint16_t)((s.palettes[p] | s.palettes[p + 1u] << 8) & 0x7fffu));
        }
    CHECK(cut);
    /* A page the game laid out, centred: its line 32 pixels further, a choice's cursor on its row with it. */
    static OraclesGuestTextBoxLine centred;
    centred = *top;
    centred.offset = 32;
    source.lines[ORACLES_GUEST_TEXT_BOX_TOP] = &centred;
    model.row[0].pixels = 192;
    model.mark[model.marks++] = (OraclesTextBoxMark){ ORACLES_TEXT_BOX_TEXT, 2, 0, 0x04, 0x82 };
    oracles_text_box_draw(&model, &source, 192, 2, box, 208);
    static uint32_t plain[144u * 40u];
    source.lines[ORACLES_GUEST_TEXT_BOX_TOP] = top;
    centred.offset = 0;
    source.lines[ORACLES_GUEST_TEXT_BOX_TOP] = &centred;
    oracles_text_box_draw(&model, &source, 128, 2, plain, 144);
    int moved = 1;
    for (unsigned y = 0; y < 8u; y++)
        for (unsigned x = 0; x < 8u; x++) moved &= box[y * 208u + (1u + 2u + 4u) * 8u + x] == plain[y * 144u + (1u + 2u) * 8u + x];   /* the cursor's tile */
    CHECK(moved);
}

/* ---- the synthetic text program ---------------------------------------------------------------- */

static uint8_t *emit16(uint8_t *at, uint8_t opcode, unsigned value)
{
    *at++ = opcode;
    *at++ = (uint8_t)value;
    *at++ = (uint8_t)(value >> 8);
    return at;
}

/* The glyph of character c: rows of ink from its number, so that no two cells are alike. */
static void write_font(uint8_t *at)
{
    for (unsigned c = 0; c < 0x100u; c++)
        for (unsigned r = 0; r < 16u; r++) at[16u * c + r] = c == ' ' ? 0xffu : (uint8_t)~((c * 7u + r * 13u) | 0x81u);
}

static uint8_t *synthetic_rom(const OraclesGuestTables *t, OraclesGame game, size_t size)
{
    uint8_t *rom = calloc(size, 1);
    if (!rom) return NULL;
    memcpy(rom + 0x134, game == ORACLES_GAME_AGES ? "ZELDA NAYRU" : "ZELDA DIN", game == ORACLES_GAME_AGES ? 11 : 9);
    rom[0x143] = 0xc0; rom[0x147] = 0x1b; rom[0x148] = 0x05; rom[0x149] = 0x02;
    rom[0x100] = 0x00; rom[0x101] = 0xc3; rom[0x102] = 0x50; rom[0x103] = 0x01;   /* nop ; jp $0150 */
    emit16(rom + 0x40, 0xc3, HANDLER);                                             /* the vblank's vector */
    const uint8_t tail = (uint8_t)t->vblank_function_queue_tail.addr;
    const unsigned queue = t->vblank_function_queue.addr;
    uint8_t *at = rom + 0x150;
    at = emit16(at, 0x31, 0xc200);                                                 /* ld sp,$c200 */
    *at++ = 0x3e; *at++ = 0x07; *at++ = 0xe0; *at++ = 0x70;                        /* ld a,7 ; ldh (<SVBK),a */
    *at++ = 0x3e; *at++ = t->draw_line_of_text.bank; at = emit16(at, 0xea, 0x2000); /* ld a,bank ; ld ($2000),a */
    *at++ = 0xaf; *at++ = 0xe0; *at++ = tail;                                       /* xor a ; ldh (<tail),a */
    *at++ = 0x3e; *at++ = 0x01; at = emit16(at, 0xea, 0xffff); *at++ = 0xfb;       /* ld a,1 ; ld (IE),a ; ei */
    const unsigned loop = (unsigned)(at - rom);
    at = emit16(at, 0xfa, 0xc0f0);                                                 /* loop: ld a,($c0f0) */
    *at++ = 0xb7; *at++ = 0x28; *at++ = 0xfa;                                      /* or a ; jr z,loop */
    *at++ = 0x47; *at++ = 0xaf; at = emit16(at, 0xea, 0xc0f0); *at++ = 0x78;       /* ld b,a ; xor a ; ld ($c0f0),a ; ld a,b */
    *at++ = 0xfe; *at++ = 0x04; *at++ = 0x20; *at++ = 0x07;                        /* cp 4 ; jr nz,+7 */
    *at++ = 0x3e; *at++ = 0x08; *at++ = 0xe0; *at++ = tail;                        /* ld a,8 ; ldh (<tail),a: the entry the test wrote */
    at = emit16(at, 0xc3, loop);                                                   /* jp loop */
    *at++ = 0xfe; *at++ = 0x01; *at++ = 0x20; *at++ = 0x06;                        /* cp 1 ; jr nz,+6 */
    at = emit16(at, 0xcd, t->draw_line_of_text.addr);                             /* call drawLineOfText */
    at = emit16(at, 0xc3, loop);                                                   /* jp loop */
    *at++ = 0xd6; *at++ = 0x02; *at++ = 0x87;                                      /* + sub 2 ; add a */
    at = emit16(at, 0xcd, t->dma_text_gfx_buffer.addr);                           /* call dmaTextGfxBuffer */
    emit16(at, 0xc3, loop);                                                        /* jp loop */
    const unsigned done = HANDLER + 14u;
    at = rom + HANDLER;
    *at++ = 0xf5; *at++ = 0xe5; *at++ = 0xf0; *at++ = tail; *at++ = 0xb7;          /* push af ; push hl ; ldh a,(<tail) ; or a */
    *at++ = 0x28; *at++ = 0x07;                                                    /* jr z,done */
    at = emit16(at, 0x21, queue + 1u); *at++ = 0xe5;                               /* ld hl,queue + 1 ; push hl */
    at = emit16(at, 0xc3, t->vblank_dma_function.addr);                           /* jp vblankDmaFunction */
    *at++ = 0xe1; *at++ = 0xf1; *at++ = 0xd9;                                      /* done: pop hl ; pop af ; reti */
    at = rom + t->vblank_dma_function.addr;
    *at++ = 0xe1;                                                                  /* pop hl */
    const uint8_t registers[] = { 0x70, 0x51, 0x52, 0x4f, 0x53, 0x54, 0x55 };      /* SVBK, HDMA1, HDMA2, VBK, HDMA3, HDMA4, HDMA5 */
    for (unsigned i = 0; i < sizeof registers; i++) { *at++ = 0x2a; *at++ = 0xe0; *at++ = registers[i]; }   /* ldi a,(hl) ; ldh (<reg),a */
    *at++ = 0xaf; *at++ = 0xe0; *at++ = 0x4f; *at++ = 0xe0; *at++ = tail;           /* xor a ; ldh (<VBK),a ; ldh (<tail),a */
    *at++ = 0x3e; *at++ = 0x07; *at++ = 0xe0; *at++ = 0x70;                        /* ld a,7 ; ldh (<SVBK),a */
    emit16(at, 0xc3, done);                                                        /* jp done */
    write_font(rom + FONT_COPY);
    at = rom + oracles_rom_offset(t->draw_line_of_text.bank, t->draw_line_of_text.addr);
    at = emit16(at, 0x21, 0xc100);
    at = emit16(at, 0x11, t->line_text_buffer.addr);
    at = emit16(at, 0x01, t->text_gfx_buffer.addr);
    uint8_t *const top = at;
    *at++ = 0x2a; *at++ = 0xb7; *at++ = 0xc8;                                      /* loop: ld a,(hli) ; or a ; ret z */
    at = emit16(at, 0xcd, t->set_line_text_buffers.addr);
    at = emit16(at, 0xcd, DRAW_CELL);
    *at = 0x18; at[1] = (uint8_t)(top - (at + 2));                                 /* jr loop */
    const uint8_t keep[] = { 0x12, 0xd5, 0x7b, 0xc6, 0x10, 0x5f, 0x3e, 0x80, 0x12,   /* ld (de),a ; the attribute $80 at e + $10 */
                             0xd1, 0x1a, 0x1c, 0xc9 };                               /* pop de ; ld a,(de) ; inc e ; ret */
    memcpy(rom + oracles_rom_offset(t->set_line_text_buffers.bank, t->set_line_text_buffers.addr), keep, sizeof keep);
    const uint8_t draw[] = { 0xe5, 0xd5, 0x6f, 0x26, 0x00, 0x29, 0x29, 0x29, 0x29,   /* push hl ; push de ; hl = 16 a */
                             0x11, (uint8_t)FONT_COPY, (uint8_t)(FONT_COPY >> 8), 0x19, 0x1e, 0x10,   /* + the font's copy ; ld e,16 */
                             0x2a, 0x02, 0x03, 0x3e, 0xff, 0x02, 0x03, 0x1d, 0x20, 0xf6,   /* a row: its ink low, $ff high */
                             0xd1, 0xe1, 0xc9 };
    memcpy(rom + oracles_rom_offset(t->draw_line_of_text.bank, DRAW_CELL), draw, sizeof draw);
    at = rom + oracles_rom_offset(t->dma_text_gfx_buffer.bank, t->dma_text_gfx_buffer.addr);
    *at++ = 0xc6; *at++ = 0x94; *at++ = 0x57;                                      /* add $94 ; ld d,a */
    at = emit16(at, 0x21, queue);                                                  /* ld hl,queue */
    const uint8_t entry[8] = { 0x00, 0x07, (uint8_t)(t->text_gfx_buffer.addr >> 8), (uint8_t)t->text_gfx_buffer.addr, 0x00, 0x00, 0x00, 0x1f };
    for (unsigned i = 0; i < sizeof entry; i++) {
        if (i == 5u) *at++ = 0x7a;                                                 /* ld a,d */
        else { *at++ = 0x3e; *at++ = entry[i]; }                                   /* ld a,byte */
        *at++ = 0x22;                                                              /* ldi (hl),a */
    }
    *at++ = 0x3e; *at++ = 0x08; *at++ = 0xe0; *at++ = tail; *at++ = 0xc9;          /* ld a,8 ; ldh (<tail),a ; ret */
    /* For the compact text to arm: the font where the game has it, and its two points where the games have them. */
    write_font(rom + oracles_rom_offset(t->font_start.bank, t->font_start.addr));
    const uint8_t written[] = { 0x7b, 0xd6, 0x20, 0x5f, 0x7e, 0x12, 0x16, (uint8_t)(t->next_text_column.addr >> 8), 0x1e, (uint8_t)t->next_text_column.addr };
    const uint8_t kept[] = { 0x7b, 0x87, 0xf6, 0x60, 0x47, 0x04, 0x70 };
    memcpy(rom + oracles_rom_offset(t->display_next_text_character.bank, t->display_next_text_character.addr) + 0x37u, written, sizeof written);
    memcpy(rom + oracles_rom_offset(t->text_control_code_option.bank, t->text_control_code_option.addr) + 0x0cu, kept, sizeof kept);
    return rom;
}

static void command(OraclesCore *core, OraclesGuest *guest, uint8_t what)
{
    uint8_t *wram0 = oracles_guest_wram_writable(guest, 0);
    wram0[0xf0] = what;
    oracles_core_run_frame(core);
    CHECK(wram0[0xf0] == 0);
}

static void draw_line(OraclesCore *core, OraclesGuest *guest, const OraclesGuestTables *t, const char *text)
{
    uint8_t *wram0 = oracles_guest_wram_writable(guest, 0), *wram7 = oracles_guest_wram_writable(guest, 7);
    memset(wram7 + (t->line_text_buffer.addr - 0xd000u), 0, 0x50u);   /* clearLineTextBuffer's and clearTextGfxBuffer's work */
    memset(wram7 + (t->text_gfx_buffer.addr - 0xd000u), 0xff, LINE_BYTES);
    memcpy(wram0 + 0x100u, text, strlen(text) + 1u);
    command(core, guest, 1);
}

/* Where the text thread reads the text: w7ActiveBank and w7TextAddress. */
static void text_at(OraclesGuest *guest, const OraclesGuestTables *t, uint8_t bank, uint16_t addr)
{
    uint8_t *wram7 = oracles_guest_wram_writable(guest, 7);
    wram7[t->active_text_bank.addr - 0xd000u] = bank;
    wram7[t->text_address.addr - 0xd000u] = (uint8_t)addr;
    wram7[t->text_address.addr - 0xd000u + 1u] = (uint8_t)(addr >> 8);
}

/* The VRAM's line `which` against the host's, next and shown. */
static int line_in_vram(OraclesCore *core, OraclesGuest *guest, unsigned which, int shown)
{
    size_t size = 0;
    uint16_t bank = 0;
    const uint8_t *vram = oracles_core_memory(core, ORACLES_CORE_VRAM, &size, &bank);
    const OraclesGuestTextBoxLine *line = shown ? oracles_guest_text_box_line(guest, which) : oracles_guest_text_box_next(guest, which);
    return vram && line && line->known && memcmp(vram + (which == ORACLES_GUEST_TEXT_BOX_TOP ? 0x1400u : 0x1600u), line->gfx, LINE_BYTES) == 0;
}

/* An entry of the vblank's queue as the fan games' dmaTextCharacter writes one, from w7d800 (`cell`'s 32 bytes) to
 * `destination` in VRAM bank `vram_bank`, `blocks` of 16 bytes less one, run at the next vblank: the cells it kept. */
static unsigned queue_cell(OraclesCore *core, OraclesGuest *guest, const OraclesGuestTables *t, const uint8_t cell[CELL_BYTES], unsigned vram_bank,
                           uint16_t destination, uint8_t blocks)
{
    uint8_t *wram0 = oracles_guest_wram_writable(guest, 0), *wram7 = oracles_guest_wram_writable(guest, 7);
    memcpy(wram7 + 0x800u, cell, CELL_BYTES);
    const uint8_t entry[8] = { 0x00, 0x07, 0xd8, 0x00, (uint8_t)vram_bank, (uint8_t)(destination >> 8), (uint8_t)destination, blocks };
    memcpy(wram0 + (t->vblank_function_queue.addr - 0xc000u), entry, sizeof entry);
    command(core, guest, 4);
    oracles_core_run_frame(core);
    OraclesGuestTextBoxCounts counts;
    oracles_guest_text_box_counts(guest, &counts);
    return counts.cells;
}

static void test_program(OraclesCoreKind kind, OraclesGame game, int compact)
{
    const OraclesGuestTables *t = game == ORACLES_GAME_AGES ? &oracles_guest_tables_ages : &oracles_guest_tables_seasons;
    const size_t size = 1024u * 1024u;
    uint8_t *rom = synthetic_rom(t, game, size);
    if (!rom) { failures++; return; }
    const OraclesCoreOptions options = { 0, 0, kind, 0 };
    OraclesCore *core = oracles_core_create(rom, size, &options);
    free(rom);
    CHECK(core != NULL);
    if (!core) return;
    const OraclesRomInfo info = { .game = game, .revision = game == ORACLES_GAME_AGES ? ORACLES_ROM_REVISION_AGES_US : ORACLES_ROM_REVISION_SEASONS_US,
                                  .known = 1 };
    OraclesGuest *guest = oracles_guest_attach(core, oracles_compat_find(&info));
    CHECK(guest != NULL);
    if (!guest) { oracles_core_destroy(core); return; }
    CHECK(oracles_guest_arm_text_box(guest, 100) == -1 && oracles_guest_arm_text_box(guest, 128) == 0 && oracles_guest_arm_text_box(guest, 128) == 0);
    CHECK(oracles_guest_arm_text_box(guest, 192) == -1);   /* armed at one width */
    if (compact) CHECK(oracles_guest_arm_compact_text(guest) == 0);
    for (unsigned f = 0; f < 200u; f++) oracles_core_run_frame(core);   /* the boot ROM */
    CHECK(!oracles_guest_text_box_line(guest, ORACLES_GUEST_TEXT_BOX_TOP)->known);
    {
        static const uint8_t unknown[CELL_BYTES];
        CHECK(queue_cell(core, guest, t, unknown, 0, 0x9600u, 0x01) == 0u);   /* a line the host does not know yet */
    }
    /* A line composed, then copied to the top line: by the vblank's DMA, at the next vblank the main loop waits for. */
    draw_line(core, guest, t, "Hello, world!");
    OraclesGuestTextBoxCounts counts;
    oracles_guest_text_box_counts(guest, &counts);
    CHECK(counts.composed == 1 && counts.copies == 0 && oracles_guest_text_box_line(guest, ORACLES_GUEST_TEXT_BOX_COMPOSED)->known);
    const uint8_t *gfx = oracles_guest_wram(guest, 7) + (t->text_gfx_buffer.addr - 0xd000u);
    CHECK(memcmp(oracles_guest_text_box_line(guest, ORACLES_GUEST_TEXT_BOX_COMPOSED)->gfx, gfx, LINE_BYTES) == 0);
    command(core, guest, 2);
    oracles_guest_text_box_counts(guest, &counts);
    CHECK(counts.copies == 0);   /* queued */
    oracles_core_run_frame(core);
    oracles_guest_text_box_counts(guest, &counts);
    CHECK(counts.copies == 1 && counts.copies_unlike == 0 && line_in_vram(core, guest, ORACLES_GUEST_TEXT_BOX_TOP, 0) && line_in_vram(core, guest, ORACLES_GUEST_TEXT_BOX_TOP, 1));
    /* Another to the bottom; the top line stays. */
    draw_line(core, guest, t, "A second line.");
    command(core, guest, 3);
    oracles_core_run_frame(core);
    CHECK(line_in_vram(core, guest, ORACLES_GUEST_TEXT_BOX_BOTTOM, 1) && line_in_vram(core, guest, ORACLES_GUEST_TEXT_BOX_TOP, 1));
    /* The line composed copied again to the top (the scroll's copy of the bottom line up). */
    command(core, guest, 2);
    oracles_core_run_frame(core);
    CHECK(line_in_vram(core, guest, ORACLES_GUEST_TEXT_BOX_TOP, 1) && memcmp(oracles_guest_text_box_line(guest, ORACLES_GUEST_TEXT_BOX_TOP)->gfx,
                                                                              oracles_guest_text_box_line(guest, ORACLES_GUEST_TEXT_BOX_BOTTOM)->gfx, LINE_BYTES) == 0);
    /* A character's cell DMAed into the top line's tiles by the game itself (a choice's cursor in the fan games): kept in
     * the host's line at its column, as in the VRAM.  Not an entry to the other bank, to the map, off a cell's edge, of
     * 16 bytes, or to a line the host does not know yet; those whose bytes the line's tiles take hold the host's own
     * there, the VRAM and the host's line kept equal. */
    {
        uint8_t cell[CELL_BYTES];
        for (unsigned i = 0; i < CELL_BYTES; i++) cell[i] = (uint8_t)(0x5au ^ i);
        CHECK(queue_cell(core, guest, t, cell, 0, 0x9440u, 0x01) == 1u);   /* the top line's third cell */
        const OraclesGuestTextBoxLine *line = oracles_guest_text_box_line(guest, ORACLES_GUEST_TEXT_BOX_TOP);
        CHECK(line_in_vram(core, guest, ORACLES_GUEST_TEXT_BOX_TOP, 1) && line->gfx[2u * CELL_BYTES] == 0x5au);
        CHECK(queue_cell(core, guest, t, cell, 1, 0x9440u, 0x01) == 1u);   /* VRAM bank 1 */
        CHECK(queue_cell(core, guest, t, cell, 0, 0x9840u, 0x01) == 1u);   /* the map */
        memcpy(cell, line->gfx + 0x50u, CELL_BYTES);
        CHECK(queue_cell(core, guest, t, cell, 0, 0x9450u, 0x01) == 1u);   /* off a cell's edge */
        memcpy(cell, line->gfx + 0x60u, CELL_BYTES);
        CHECK(queue_cell(core, guest, t, cell, 0, 0x9460u, 0x00) == 1u);   /* 16 bytes */
        CHECK(line_in_vram(core, guest, ORACLES_GUEST_TEXT_BOX_TOP, 1));
    }
    /* A savestate: the lines saved with it come back with it; a state without them leaves them unknown. */
    static uint8_t field[ORACLES_GUEST_TEXT_BOX_STATE_SIZE], core_state[1u << 20];
    const size_t field_size = oracles_guest_text_box_save_state(guest, field, sizeof field);
    const size_t state_size = oracles_core_state_size(core);
    CHECK(field_size > 0 && state_size <= sizeof core_state && oracles_core_save_state(core, core_state, state_size) == 0);
    draw_line(core, guest, t, "Not saved.");
    command(core, guest, 3);
    oracles_core_run_frame(core);
    CHECK(oracles_core_load_state(core, core_state, state_size) == 0);
    oracles_guest_reset_execution_state(guest);
    CHECK(!oracles_guest_text_box_line(guest, ORACLES_GUEST_TEXT_BOX_BOTTOM)->known);
    CHECK(oracles_guest_text_box_load_state(guest, field, field_size) == 0);
    CHECK(line_in_vram(core, guest, ORACLES_GUEST_TEXT_BOX_BOTTOM, 0) && line_in_vram(core, guest, ORACLES_GUEST_TEXT_BOX_TOP, 1));
    oracles_core_run_frame(core);
    CHECK(line_in_vram(core, guest, ORACLES_GUEST_TEXT_BOX_BOTTOM, 1));
    CHECK(oracles_guest_text_box_load_state(guest, field, field_size - 1u) == -1 && !oracles_guest_text_box_line(guest, ORACLES_GUEST_TEXT_BOX_TOP)->known);
    oracles_guest_detach(guest);
    oracles_core_destroy(core);
}

/* The wide box on the synthetic program: a line of twenty characters composed whole in the host's line at 192 pixels
 * (classic at the fixed advance, or compact), its first 128 pixels in w7TextGfxBuffer and in the VRAM, its sixteen
 * steps' bytes within them and their boundaries past them in the host's line; the reveal read from the text's state as
 * the frame just scanned showed it; the field of the savestate giving all of it back. */
static void test_program_wide(OraclesCoreKind kind, OraclesGame game, int fixed)
{
    const OraclesGuestTables *t = game == ORACLES_GAME_AGES ? &oracles_guest_tables_ages : &oracles_guest_tables_seasons;
    const size_t size = 1024u * 1024u;
    uint8_t *rom = synthetic_rom(t, game, size);
    if (!rom) { failures++; return; }
    const OraclesCoreOptions options = { 0, 0, kind, 0 };
    OraclesCore *core = oracles_core_create(rom, size, &options);
    free(rom);
    CHECK(core != NULL);
    if (!core) return;
    const OraclesRomInfo info = { .game = game, .revision = game == ORACLES_GAME_AGES ? ORACLES_ROM_REVISION_AGES_US : ORACLES_ROM_REVISION_SEASONS_US,
                                  .known = 1 };
    OraclesGuest *guest = oracles_guest_attach(core, oracles_compat_find(&info));
    CHECK(guest != NULL);
    if (!guest) { oracles_core_destroy(core); return; }
    CHECK(oracles_guest_arm_wide_text_box(guest, 128, fixed) == -1);   /* the game's width is not wide */
    CHECK(oracles_guest_arm_wide_text_box(guest, 192, fixed) == 0 && oracles_guest_arm_wide_text_box(guest, 192, fixed) == 0);
    CHECK(oracles_guest_arm_wide_text_box(guest, 192, !fixed) == -1 && oracles_guest_text_box_pixels(guest) == 192u);
    for (unsigned f = 0; f < 200u; f++) oracles_core_run_frame(core);
    /* The text read at $70:$4105, physical $1c0105: a page laid out at the wide width, drawn from the box's left. */
    const uint32_t laid_page[2] = { 0x1c0100, 0x1c0110 };
    CHECK(oracles_guest_set_text_box_pages(guest, laid_page, 1) == 0);
    text_at(guest, t, 0x70, 0x4105);
    draw_line(core, guest, t, "Hello, wide world!!!");
    const OraclesGuestTextBoxLine *composed = oracles_guest_text_box_line(guest, ORACLES_GUEST_TEXT_BOX_COMPOSED);
    const uint8_t *wram7 = oracles_guest_wram(guest, 7);
    const uint8_t *gfx = wram7 + (t->text_gfx_buffer.addr - 0xd000u), *steps = wram7 + (t->line_text_buffer.addr - 0xd000u);
    CHECK(composed && composed->known && composed->columns == 24 && composed->count == 20 && composed->steps == 16);
    if (!composed) { oracles_guest_detach(guest); oracles_core_destroy(core); return; }
    CHECK(!memcmp(composed->gfx, gfx, LINE_BYTES));
    OraclesGuestTextBoxCounts counts;
    oracles_guest_text_box_counts(guest, &counts);
    CHECK(counts.overflows == 0 && counts.wide_lines == 1u);
    if (fixed) {   /* the fixed advance: the game's own cells, a character a tile, past the sixteenth too */
        uint8_t font[16 * 0x100];
        write_font(font);
        int same = 1;
        for (unsigned k = 0; k < 20u; k++) {
            const uint8_t c = (uint8_t)"Hello, wide world!!!"[k];
            CHECK(composed->character[k].start == 8u * k && composed->character[k].character == c);
            for (unsigned r = 0; r < 16u; r++) same &= composed->gfx[k * CELL_BYTES + 2u * r] == font[16u * c + r] && composed->gfx[k * CELL_BYTES + 2u * r + 1u] == 0xffu;
        }
        CHECK(same && composed->boundary[15] == 160u && composed->boundary[14] == 152u && steps[15] == (0x80u | 127u) && steps[0] == (0x80u | composed->boundary[0]));
    }
    /* Copied to the top line by the vblank's DMA: the VRAM holds its first 128 pixels, the host the whole line. */
    command(core, guest, 2);
    oracles_core_run_frame(core);
    CHECK(line_in_vram(core, guest, ORACLES_GUEST_TEXT_BOX_TOP, 1) && !memcmp(oracles_guest_text_box_line(guest, ORACLES_GUEST_TEXT_BOX_TOP)->gfx, composed->gfx, sizeof composed->gfx));
    /* The text's state of a reveal: the top line, three steps shown, the third's delay of 10 frames counted down to 4. */
    uint8_t *w0 = oracles_guest_wram_writable(guest, 0), *w7 = oracles_guest_wram_writable(guest, 7);
    w0[t->text_is_active.addr - 0xc000u] = 1;
    w0[t->text_display_mode.addr - 0xc000u] = 0;
    w7[t->text_display_state.addr - 0xd000u] = 2;
    w7[t->text_line_tiles.addr - 0xd000u] = 0x40;
    w7[t->next_text_column.addr - 0xd000u] = 3;
    w7[t->line_delays_buffer.addr - 0xd000u + 2u] = 10;
    w7[t->character_display_timer.addr - 0xd000u] = 4;
    OraclesGuestTextBoxReveal reveal;
    oracles_core_run_frame(core);
    oracles_guest_text_box_reveal_state(guest, &reveal);
    CHECK(reveal.line == 0);   /* read at this vblank: what the next frame shows */
    oracles_core_run_frame(core);
    oracles_guest_text_box_reveal_state(guest, &reveal);
    CHECK(reveal.line == ORACLES_GUEST_TEXT_BOX_TOP && reveal.shown == 3 && reveal.elapsed == 6);
    /* The field: the lines, their layout and the reveal, back after a load. */
    static uint8_t field[ORACLES_GUEST_TEXT_BOX_STATE_SIZE];
    static OraclesGuestTextBoxLine saved;
    saved = *oracles_guest_text_box_next(guest, ORACLES_GUEST_TEXT_BOX_TOP);
    const size_t field_size = oracles_guest_text_box_save_state(guest, field, sizeof field);
    CHECK(field_size > 0 && field_size <= sizeof field);
    w0[t->text_is_active.addr - 0xc000u] = 0;
    draw_line(core, guest, t, "Another.");
    command(core, guest, 2);
    oracles_core_run_frame(core);
    oracles_core_run_frame(core);
    oracles_guest_text_box_reveal_state(guest, &reveal);
    CHECK(reveal.line == 0 && oracles_guest_text_box_next(guest, ORACLES_GUEST_TEXT_BOX_TOP)->count == 8);
    oracles_guest_reset_execution_state(guest);
    CHECK(oracles_guest_text_box_load_state(guest, field, field_size) == 0);
    const OraclesGuestTextBoxLine *back = oracles_guest_text_box_line(guest, ORACLES_GUEST_TEXT_BOX_TOP);
    CHECK(back->count == saved.count && back->steps == saved.steps && !memcmp(back->first, saved.first, sizeof saved.first)
          && !memcmp(back->boundary, saved.boundary, sizeof saved.boundary) && !memcmp(back->gfx, saved.gfx, sizeof saved.gfx));
    int characters = 1;
    for (unsigned k = 0; k < ORACLES_GUEST_LINE_CHARACTERS; k++)
        characters &= back->character[k].start == saved.character[k].start && back->character[k].character == saved.character[k].character
                    && back->character[k].delay == saved.character[k].delay && back->character[k].drawn == saved.character[k].drawn;
    oracles_guest_text_box_reveal_state(guest, &reveal);
    CHECK(characters && reveal.line == ORACLES_GUEST_TEXT_BOX_TOP && reveal.shown == 3 && reveal.elapsed == 6);
    /* A field damaged within its size: steps naming characters past the line's or out of order, an offset past the box's
     * spare width or off a tile, refused, the lines unknown; the field whole loads again. */
    {
        static uint8_t damaged[ORACLES_GUEST_TEXT_BOX_STATE_SIZE];
        const size_t top = 4u + 3u + ORACLES_GUEST_TEXT_BOX_TOP * (2u + 24u * 33u + ORACLES_GUEST_TEXT_BOX_LINE_LAYOUT_SIZE) + 2u + 24u * 33u;
        CHECK(field[top] == 20u && field[top + 1u] == 16u && field[top + 2u] == 0u && field[top + 3u + 16u] == 20u);   /* count, steps, offset, first[16] */
        static const struct { unsigned at; uint8_t value; } damage[] = { { 3u + 16u, 21u }, { 3u + 16u, 255u }, { 3u + 2u, 200u }, { 2u, 72u }, { 2u, 3u } };
        for (unsigned d = 0; d < sizeof damage / sizeof damage[0]; d++) {
            memcpy(damaged, field, field_size);
            damaged[top + damage[d].at] = damage[d].value;
            CHECK(oracles_guest_text_box_load_state(guest, damaged, field_size) == -1 && !oracles_guest_text_box_line(guest, ORACLES_GUEST_TEXT_BOX_TOP)->known);
        }
        memcpy(damaged, field, field_size);
        damaged[top + 2u] = 64u;   /* the whole spare width: the game's line at the box's right */
        CHECK(oracles_guest_text_box_load_state(guest, damaged, field_size) == 0);
        CHECK(oracles_guest_text_box_load_state(guest, field, field_size) == 0 && oracles_guest_text_box_line(guest, ORACLES_GUEST_TEXT_BOX_TOP)->known);
    }
    /* A line of a page none of those holds, the game's layout: centred, 32 pixels further, its tiles, characters and
     * steps with it; the game's buffer as the game draws it. */
    text_at(guest, t, 0x70, 0x4110);   /* past the page: the game's layout */
    draw_line(core, guest, t, "Kept line");
    const OraclesGuestTextBoxLine *kept = oracles_guest_text_box_line(guest, ORACLES_GUEST_TEXT_BOX_COMPOSED);
    const uint8_t *game_gfx = oracles_guest_wram(guest, 7) + (t->text_gfx_buffer.addr - 0xd000u);
    int blank = 1;
    for (unsigned i = 0; i < 4u * CELL_BYTES; i++) blank &= kept->gfx[i] == 0xffu;
    CHECK(kept->offset == 32u && kept->character[0].start == 32u && blank && !memcmp(kept->gfx + 4u * CELL_BYTES, game_gfx, 9u * CELL_BYTES));
    CHECK(kept->boundary[kept->steps - 1u] == 32u + 8u * 9u);
    oracles_guest_text_box_counts(guest, &counts);
    CHECK(counts.centred == 1);
    /* Copied to the top line, a cell the game DMAs to its third goes to the host's column moved with the line. */
    command(core, guest, 2);
    oracles_core_run_frame(core);
    {
        uint8_t cell[CELL_BYTES];
        for (unsigned i = 0; i < CELL_BYTES; i++) cell[i] = (uint8_t)(0xa5u ^ i);
        const unsigned before = counts.cells;
        CHECK(queue_cell(core, guest, t, cell, 0, 0x9440u, 0x01) == before + 1u);
        const OraclesGuestTextBoxLine *top = oracles_guest_text_box_line(guest, ORACLES_GUEST_TEXT_BOX_TOP);
        CHECK(top->offset == 32u && !memcmp(top->gfx + 6u * CELL_BYTES, cell, CELL_BYTES) && memcmp(top->gfx + 2u * CELL_BYTES, cell, CELL_BYTES) != 0);
    }
    field[1] = 16;   /* a state of another width */
    CHECK(oracles_guest_text_box_load_state(guest, field, field_size) == -1 && !oracles_guest_text_box_line(guest, ORACLES_GUEST_TEXT_BOX_TOP)->known);
    oracles_guest_detach(guest);
    oracles_core_destroy(core);
}

/* The fan games' profiles qualify the host's box and Text size, as Ages'.  A profile without the host's box has the wide
 * one refused, the game's box armed for the harness's check alone, and Text size taken from the core's image as Ages
 * takes it; a profile without Text size keeps Normal whatever is asked. */
static void test_fan_game(OraclesCoreKind kind)
{
    static const struct { OraclesGame game; OraclesRomRevision revision; } fans[3] = {
        { ORACLES_GAME_AGES, ORACLES_ROM_REVISION_MOONRISE_1_0_6 }, { ORACLES_GAME_SEASONS, ORACLES_ROM_REVISION_TEMPLE_1_073 },
        { ORACLES_GAME_AGES, ORACLES_ROM_REVISION_KINOMI_1_1_2 } };
    for (unsigned i = 0; i < 3u; i++) {
        const OraclesRomInfo info = { .game = fans[i].game, .revision = fans[i].revision, .known = 1 };
        const OraclesCompatProfile *profile = oracles_compat_find(&info);
        CHECK(profile && oracles_compat_host_text_box(profile) && oracles_compat_text_size(profile));
    }
    const OraclesRomInfo moonrise_info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_MOONRISE_1_0_6, .known = 1 };
    const OraclesRomInfo ages_info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_AGES_US, .known = 1 };
    OraclesCompatProfile unboxed = *oracles_compat_find(&moonrise_info), unsized = unboxed;
    unboxed.host_text_box = 0;
    unsized.host_text_box = unsized.text_size = 0;
    const OraclesCompatProfile *const profiles[3] = { &unboxed, &unsized, oracles_compat_find(&ages_info) };
    const size_t size = 1024u * 1024u;
    for (unsigned r = 0; r < 3u; r++) {
        uint8_t *rom = synthetic_rom(&oracles_guest_tables_ages, ORACLES_GAME_AGES, size);
        if (!rom) { failures++; return; }
        const OraclesCoreOptions options = { 0, 0, kind, 0 };
        OraclesCore *core = oracles_core_create(rom, size, &options);
        free(rom);
        CHECK(core != NULL);
        if (!core) return;
        OraclesGuest *guest = oracles_guest_attach(core, profiles[r]);
        CHECK(guest != NULL);
        OraclesEnhancedView *view = guest ? oracles_enhanced_view_start(core, guest, NULL, 0) : NULL;
        CHECK(view != NULL);
        if (view) {
            const int fan = r < 2u, sized = r != 1u;
            if (fan) CHECK(oracles_guest_arm_wide_text_box(guest, 192, 0) == -1);
            CHECK(oracles_guest_arm_text_box(guest, 128) == 0);
            oracles_enhanced_view_set_text_size(view, ORACLES_ENHANCED_TEXT_XLARGE);
            CHECK(oracles_enhanced_view_text_size(view) == (sized ? ORACLES_ENHANCED_TEXT_XLARGE : ORACLES_ENHANCED_TEXT_NORMAL));
            oracles_enhanced_view_set_text_size(view, ORACLES_ENHANCED_TEXT_LARGE);
            CHECK(oracles_enhanced_view_text_size(view) == (sized ? ORACLES_ENHANCED_TEXT_LARGE : ORACLES_ENHANCED_TEXT_NORMAL));
            oracles_enhanced_view_stop(view);
        }
        if (guest) oracles_guest_detach(guest);
        oracles_core_destroy(core);
    }
}

int main(void)
{
    test_fixed_advance();
    test_fixed_advance_wide();
    test_reveal_pixels();
    test_in_pages();
    test_enlarge();
    test_reserve();
    test_model();
    test_wide();
    const OraclesCoreKind kinds[] = { ORACLES_CORE_SAMEBOY, ORACLES_CORE_MGBA };
    for (unsigned k = 0; k < 2u; k++)
        for (unsigned compact = 0; compact < 2u; compact++) {
            test_program(kinds[k], ORACLES_GAME_AGES, (int)compact);
            test_program(kinds[k], ORACLES_GAME_SEASONS, (int)compact);
            test_program_wide(kinds[k], compact ? ORACLES_GAME_SEASONS : ORACLES_GAME_AGES, (int)!compact);
        }
    test_fan_game(ORACLES_CORE_SAMEBOY);
    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("text box: all tests passed\n");
    return 0;
}
