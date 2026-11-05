/* The hotbar's drawing, without a ROM: where the four
 * slots stand, that nothing is ever written in the game's 160 columns nor
 * under the HUD band, and what each state of a slot puts on screen; from a
 * column, with the status bar laid out; and the game's look where the band
 * holds it, spaced as the status bar spaces B and A. */
#include "hotbar.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

#define W 256u
#define H 144u
#define CORE 160u
#define UNTOUCHED 0x11223344u
#define BACKGROUND 0xff000001u
#define INK 0xff000002u
#define FAINT 0xff000003u
#define BADGE 0xff000004u
#define BADGE_INK 0xff000005u
#define REFUSED 0xff000006u

static uint32_t icon_pixel(void *opaque, uint16_t rgb555) { (void)opaque; return 0xff100000u | rgb555; }

static unsigned count(const uint32_t *surface, unsigned x0, uint32_t colour)
{
    unsigned n = 0;
    for (unsigned y = 0; y < 16u; y++) for (unsigned x = 0; x < ORACLES_HOTBAR_SLOT_WIDTH; x++) n += surface[y * W + x0 + x] == colour;
    return n;
}

#define FAR 480u
static uint32_t far_surface[FAR * 16u];

static unsigned count_in(unsigned x0, unsigned x1, unsigned y0, unsigned y1, uint32_t colour)
{
    unsigned n = 0;
    for (unsigned y = y0; y < y1; y++) for (unsigned x = x0; x < x1; x++) n += far_surface[y * FAR + x] == colour;
    return n;
}

/* The game's look: B's slot as the bar draws B (its letter from column 1, the opening bracket in column 6, the icon from
 * column 8, the closing bracket's upright in 33, its serif in 32); every key's letter a column clear of the bracket; a
 * second character small under the first, the button's badge in its place while the item sits on one; the states on
 * the brackets. */
/* The columns from x0 to x1 (excluded) on lines y0 to y1 that hold `colour`: the first and the last, 0 when none. */
static int span(unsigned x0, unsigned x1, unsigned y0, unsigned y1, uint32_t colour, unsigned *first, unsigned *last)
{
    int found = 0;
    for (unsigned x = x0; x < x1; x++)
        for (unsigned y = y0; y < y1; y++)
            if (far_surface[y * FAR + x] == colour) { if (!found) *first = x; *last = x; found = 1; break; }
    return found;
}

/* Under each key's letter, the badge of the button and the small second character are centred on the letter's middle
 * column, the left one of two for an even width, whatever the letter's width, and the badge keeps a blank column
 * before the opening bracket. */
static void centred_under_letter(const OraclesHotbarPalette *palette, const uint16_t *icon)
{
    static const char *const keys[] = { "A", "S", "Q", "W", "X", "Y", "LB", "RB", "M", "T", "V", "I", "1" };
    const unsigned lead = ORACLES_HOTBAR_GAME_LEAD;
    for (unsigned k = 0; k < sizeof keys / sizeof keys[0]; k++) {
        OraclesHotbarSlotView slots[ORACLES_HOTBAR_SLOTS];
        memset(slots, 0, sizeof slots);
        slots[0].icon = icon; slots[0].key = keys[k]; slots[0].button = 'B';
        slots[1].icon = icon; slots[1].key = keys[k];
        for (unsigned i = 0; i < FAR * 16u; i++) far_surface[i] = UNTOUCHED;
        oracles_hotbar_draw_at(far_surface, FAR, 0, ORACLES_HOTBAR_GAME, slots, palette);
        const unsigned open = oracles_hotbar_game_bracket(keys[k]);
        unsigned l0 = 0, l1 = 0, b0 = 0, b1 = 0;
        CHECK(span(lead, lead + open, 1, 8, INK, &l0, &l1));
        const unsigned middle = (l0 + l1) / 2u;
        CHECK(middle == lead + oracles_hotbar_game_letter_middle(keys[k]));
        CHECK(span(lead, lead + open, 9, 16, BADGE, &b0, &b1) && b1 - b0 == 4u && (b0 + b1) / 2u == middle && b1 + 1u < lead + open);
        const unsigned s1 = lead + ORACLES_HOTBAR_GAME_SLOT_WIDTH;
        if (keys[k][1]) {
            unsigned c0 = 0, c1 = 0;
            CHECK(span(s1, s1 + oracles_hotbar_game_bracket(keys[k]), 9, 16, INK, &c0, &c1) && (c0 + c1) / 2u == middle + ORACLES_HOTBAR_GAME_SLOT_WIDTH);
        }
    }
}

/* HUD Original, the game's look: the slots continue the bar on both sides, its lead painted with their background, the
 * dark bands beyond the outer slots alone; elsewhere (full width, the band's own colours there) the lead is left. */
static void glued_to_the_bar(const OraclesHotbarPalette *palette, const uint16_t *icon)
{
    OraclesHotbarSlotView slots[ORACLES_HOTBAR_SLOTS];
    memset(slots, 0, sizeof slots);
    for (unsigned n = 0; n < ORACLES_HOTBAR_SLOTS; n++) { slots[n].icon = icon; slots[n].key = "W"; }
    const unsigned gutter = (FAR - CORE) / 2u, w = ORACLES_HOTBAR_GAME_SLOT_WIDTH, lead = ORACLES_HOTBAR_GAME_LEAD;
    for (int glued = 1; glued >= 0; glued--) {
        for (unsigned i = 0; i < FAR * 16u; i++) far_surface[i] = UNTOUCHED;
        oracles_hotbar_draw(far_surface, FAR, CORE, glued, slots, palette);
        CHECK(count_in(gutter - 2u * w, gutter, 0, 16, UNTOUCHED) == 0);                         /* the left pair against the bar */
        CHECK(count_in(gutter + CORE + lead, gutter + CORE + lead + 2u * w, 0, 16, UNTOUCHED) == 0);
        CHECK(count_in(gutter + CORE, gutter + CORE + lead, 0, 16, BACKGROUND) == (glued ? lead * 16u : 0u));
        CHECK(count_in(gutter + CORE, gutter + CORE + lead, 0, 16, UNTOUCHED) == (glued ? 0u : lead * 16u));
        CHECK(count_in(0, gutter - 2u * w, 0, 16, UNTOUCHED) == (gutter - 2u * w) * 16u);       /* the dark bands beyond */
        CHECK(count_in(gutter + CORE, gutter + CORE + 1u, 0, 16, INK) == 0);                    /* no letter against the bar */
    }
    /* The compact look, two slots of 24 a side against the bar with no lead, continues it as it is. */
    static uint32_t near[256u * 16u];
    for (unsigned i = 0; i < 256u * 16u; i++) near[i] = UNTOUCHED;
    oracles_hotbar_draw(near, 256u, CORE, 1, slots, palette);
    unsigned left = 0;
    for (unsigned y = 0; y < 16u; y++) for (unsigned x = 0; x < 256u; x++) left += (x < 48u || x >= 208u) && near[y * 256u + x] == UNTOUCHED;
    CHECK(left == 0);
}

static void game_look(const OraclesHotbarPalette *palette, const uint16_t *icon)
{
    CHECK(oracles_hotbar_game_bracket("B") == 6u && oracles_hotbar_game_bracket("A") == 6u && oracles_hotbar_game_bracket("W") == 7u
          && oracles_hotbar_game_bracket("Y") == 7u && oracles_hotbar_game_bracket("1") == 6u && oracles_hotbar_game_bracket("LB") == 6u);
    const unsigned lead = ORACLES_HOTBAR_GAME_LEAD, w = ORACLES_HOTBAR_GAME_SLOT_WIDTH;
    static const char *const keys[] = { "A", "S", "Q", "W", "X", "Y", "LB", "RB", "M", "T", "V", "1", "0", "?" };
    for (unsigned k = 0; k < sizeof keys / sizeof keys[0]; k++) {
        OraclesHotbarSlotView slots[ORACLES_HOTBAR_SLOTS];
        memset(slots, 0, sizeof slots);
        for (unsigned n = 0; n < ORACLES_HOTBAR_SLOTS; n++) { slots[n].icon = icon; slots[n].key = keys[k]; slots[n].button = n == 0 ? 'B' : 0; }
        for (unsigned i = 0; i < FAR * 16u; i++) far_surface[i] = UNTOUCHED;
        oracles_hotbar_draw_at(far_surface, FAR, 0, ORACLES_HOTBAR_GAME, slots, palette);
        const unsigned open = lead + oracles_hotbar_game_bracket(keys[k]);
        CHECK(count_in(0, lead, 0, 16, BACKGROUND) == lead * 16u);                  /* the lead, blank: the badge four columns clear of the block before */
        CHECK(count_in(open - 1u, open, 0, 16, INK) == 0);                          /* the column before the bracket, clear */
        CHECK(count_in(lead, open - 1u, 1, 8, INK) > 5u);                           /* the letter */
        CHECK(count_in(open, open + 1u, 1, 15, INK) == 14u && count_in(open + 27u, open + 28u, 1, 15, INK) == 14u);
        CHECK(count_in(open + 1u, open + 2u, 0, 16, INK) == 2u && count_in(open + 26u, open + 27u, 0, 16, INK) == 2u);   /* the serifs */
        CHECK(count_in(open + 2u, open + 18u, 0, 16, 0xff100000u | 0x1234u) == 40u);   /* the icon from two columns on */
        /* after the closing bracket, three columns blank at least before the next slot, four before the block after the last */
        CHECK(count_in(open + 28u, lead + w, 0, 16, BACKGROUND) == (lead + w - open - 28u) * 16u && lead + w - open - 28u >= 3u);
        CHECK(count_in(0, lead + 4u * w, 0, 16, UNTOUCHED) == 0 && count_in(lead + 4u * w, FAR, 0, 16, UNTOUCHED) == (FAR - lead - 4u * w) * 16u);
        CHECK(oracles_hotbar_width(ORACLES_HOTBAR_GAME) == lead + 4u * w);
    }
    /* RB: R at the bar's size, a small B under it; the item on B: the badge in its place, never both; at rest, the B back */
    OraclesHotbarSlotView slots[ORACLES_HOTBAR_SLOTS];
    memset(slots, 0, sizeof slots);
    slots[0].icon = icon; slots[0].key = "RB";
    slots[1].icon = icon; slots[1].key = "RB"; slots[1].button = 'B';
    slots[2].key = "Q"; slots[2].frame = ORACLES_HOTBAR_FRAME_DOTTED;                                             /* empty */
    slots[3].icon = icon; slots[3].key = "W"; slots[3].frame = ORACLES_HOTBAR_FRAME_REFUSED;
    for (unsigned i = 0; i < FAR * 16u; i++) far_surface[i] = UNTOUCHED;
    oracles_hotbar_draw_at(far_surface, FAR, 0, ORACLES_HOTBAR_GAME, slots, palette);
    const unsigned s0 = lead, s1 = lead + w, s2 = lead + 2u * w, s3 = lead + 3u * w;
    CHECK(count_in(s0, s0 + 5u, 9, 16, INK) > 4u && count_in(s0, s0 + 5u, 9, 16, BADGE) == 0);
    CHECK(count_in(s1, s1 + 5u, 9, 16, INK) == 0 && count_in(s1, s1 + 5u, 9, 16, BADGE) > 20u && count_in(s1, s1 + 5u, 9, 16, BADGE_INK) > 4u);
    CHECK(count_in(s2, s2 + w, 0, 16, FAINT) == 18u && count_in(s2, s2 + w, 0, 16, INK) > 5u);   /* the brackets dotted: seven lines of each upright and the serifs */
    CHECK(count_in(s3, s3 + w, 0, 16, REFUSED) == 32u);
    /* Waiting: a line under the icon, the item on a button or not; none at rest */
    memset(slots, 0, sizeof slots);
    slots[0].icon = icon; slots[0].key = "A"; slots[0].waiting = 1; slots[0].button = 'B'; slots[0].frame = ORACLES_HOTBAR_FRAME_SOLID;
    slots[1].icon = icon; slots[1].key = "S"; slots[1].waiting = 1;
    slots[2].icon = icon; slots[2].key = "Q"; slots[2].button = 'A'; slots[2].frame = ORACLES_HOTBAR_FRAME_SOLID;
    for (unsigned i = 0; i < FAR * 16u; i++) far_surface[i] = UNTOUCHED;
    oracles_hotbar_draw_at(far_surface, FAR, 0, ORACLES_HOTBAR_GAME, slots, palette);
    const unsigned under = oracles_hotbar_game_bracket("A") + 2u;
    CHECK(count_in(s0 + under, s0 + under + 16u, 15, 16, INK) == 16u && count_in(s1 + under, s1 + under + 16u, 15, 16, INK) == 16u);
    CHECK(count_in(s2 + under, s2 + under + 16u, 15, 16, INK) == 0);
    /* The item as the bar draws it on a button (its sprites where the bar puts them, its count beside them): the picture
     * from the icon's column, 24 wide, in place of the icon, between the brackets */
    static uint16_t picture[ORACLES_HOTBAR_GAME_PICTURE_WIDTH * 16u];
    memset(picture, 0, sizeof picture);
    for (unsigned y = 9; y < 15u; y++) for (unsigned x = 16; x < 24u; x++) picture[y * ORACLES_HOTBAR_GAME_PICTURE_WIDTH + x] = (uint16_t)(ORACLES_HOTBAR_ICON_OPAQUE | 0x0042u);
    for (unsigned y = 0; y < 16u; y++) picture[y * ORACLES_HOTBAR_GAME_PICTURE_WIDTH] = (uint16_t)(ORACLES_HOTBAR_ICON_OPAQUE | 0x1234u);
    memset(slots, 0, sizeof slots);
    slots[0].icon = icon; slots[0].picture = picture; slots[0].key = "A";
    for (unsigned i = 0; i < FAR * 16u; i++) far_surface[i] = UNTOUCHED;
    oracles_hotbar_draw_at(far_surface, FAR, 0, ORACLES_HOTBAR_GAME, slots, palette);
    const unsigned open = lead + oracles_hotbar_game_bracket("A");
    CHECK(count_in(open + 2u, open + 3u, 0, 16, 0xff100000u | 0x1234u) == 16u && count_in(0, lead + w, 0, 16, 0xff100000u | 0x1234u) == 16u);
    CHECK(count_in(open + 18u, open + 26u, 9, 15, 0xff100000u | 0x0042u) == 48u);   /* the count's column, inside the closing bracket */
    CHECK(count_in(open + 26u, open + 28u, 1, 15, INK) == 16u);
}

int main(void)
{
    static uint32_t surface[W * H];
    const OraclesHotbarPalette palette = { BACKGROUND, INK, FAINT, BADGE, BADGE_INK, REFUSED, icon_pixel, NULL };
    CHECK(oracles_hotbar_slot_x(0, W, CORE) == 0 && oracles_hotbar_slot_x(1, W, CORE) == 24 && oracles_hotbar_slot_x(2, W, CORE) == 208 && oracles_hotbar_slot_x(3, W, CORE) == 232);
    /* The drawn-back view's 480, in the game's look: two slots of 38 against each side of the status bar, after four
     * blank columns on its right, not at the surface's edges.  The compact look in near 16:9 alone, the game's from medium 4:3 (80 a side) on. */
    CHECK(oracles_hotbar_slot_x(0, 480u, CORE) == 84 && oracles_hotbar_slot_x(1, 480u, CORE) == 122 && oracles_hotbar_slot_x(2, 480u, CORE) == 324 && oracles_hotbar_slot_x(3, 480u, CORE) == 362);
    CHECK(oracles_hotbar_gutter_look(256u, CORE) == ORACLES_HOTBAR_COMPACT && oracles_hotbar_gutter_look(320u, CORE) == ORACLES_HOTBAR_GAME
          && oracles_hotbar_gutter_look(384u, CORE) == ORACLES_HOTBAR_GAME && oracles_hotbar_gutter_look(412u, CORE) == ORACLES_HOTBAR_GAME);
    /* The near view in 4:3, 213 wide, has 26 px a side: no room for two slots, nothing is drawn; the other sizes fit. */
    CHECK(!oracles_hotbar_fits(213u, CORE) && oracles_hotbar_fits(256u, CORE) && oracles_hotbar_fits(320u, CORE) && oracles_hotbar_fits(384u, CORE));
    {
        static uint32_t narrow[213u * 16u];
        for (unsigned i = 0; i < 213u * 16u; i++) narrow[i] = 0x12345678u;
        OraclesHotbarSlotView none[ORACLES_HOTBAR_SLOTS];
        memset(none, 0, sizeof none);
        oracles_hotbar_draw(narrow, 213u, CORE, 1, none, &palette);
        int untouched = 1;
        for (unsigned i = 0; i < 213u * 16u; i++) untouched &= narrow[i] == 0x12345678u;
        CHECK(untouched);
    }

    uint16_t icon[ORACLES_HOTBAR_ICON * ORACLES_HOTBAR_ICON];
    memset(icon, 0, sizeof icon);
    for (unsigned i = 0; i < 40u; i++) icon[5u * ORACLES_HOTBAR_ICON + i % 8u + (i / 8u) * ORACLES_HOTBAR_ICON] = (uint16_t)(ORACLES_HOTBAR_ICON_OPAQUE | 0x1234u);
    OraclesHotbarSlotView slots[ORACLES_HOTBAR_SLOTS];
    memset(slots, 0, sizeof slots);
    slots[0].icon = icon; slots[0].key = "A"; slots[0].button = 'B'; slots[0].frame = ORACLES_HOTBAR_FRAME_SOLID;   /* on a button */
    slots[1].label = "FEAT"; slots[1].key = "S";                                                                    /* no icon yet */
    slots[2].icon = icon; slots[2].key = "LB"; slots[2].dimmed = 1; slots[2].frame = ORACLES_HOTBAR_FRAME_REFUSED;  /* not owned, and refused */
    slots[3].key = "RB"; slots[3].frame = ORACLES_HOTBAR_FRAME_DOTTED;                                              /* empty */

    for (unsigned i = 0; i < W * H; i++) surface[i] = UNTOUCHED;
    oracles_hotbar_draw(surface, W, CORE, 1, slots, &palette);
    /* never in the game's columns, never under the HUD band; the gutters' sixteen lines wholly painted */
    unsigned wrong = 0, unpainted = 0;
    for (unsigned y = 0; y < H; y++) for (unsigned x = 0; x < W; x++) {
        const int in_gutter = y < 16u && (x < 48u || x >= 208u);
        if (!in_gutter && surface[y * W + x] != UNTOUCHED) wrong++;
        if (in_gutter && surface[y * W + x] == UNTOUCHED) unpainted++;
    }
    CHECK(wrong == 0 && unpainted == 0);
    /* slot 1: the icon's forty pixels, a whole frame, the badge and its letter */
    CHECK(count(surface, 0, 0xff100000u | 0x1234u) >= 30u);                       /* the frame takes a few of them */
    CHECK(count(surface, 0, BADGE) > 20u && count(surface, 0, BADGE_INK) > 5u);
    /* slot 2: a label and no badge, no frame */
    CHECK(count(surface, 24, INK) > 30u && count(surface, 24, BADGE) == 0 && count(surface, 24, REFUSED) == 0);
    /* slot 3: the icon dimmed (none of its own colour left), a red frame, a key of two letters */
    CHECK(count(surface, 208, 0xff100000u | 0x1234u) == 0 && count(surface, 208, REFUSED) == 60u);
    /* slot 4: a dotted frame, half the pixels of a whole one */
    CHECK(count(surface, 232, FAINT) == 31u && count(surface, 232, BADGE) == 0);   /* every other pixel of the sixty, one corner shared */
    /* From a column, the status bar laid out: the four slots side by side there, the rest untouched. */
    for (unsigned i = 0; i < W * H; i++) surface[i] = UNTOUCHED;
    oracles_hotbar_draw_at(surface, W, 80u, ORACLES_HOTBAR_COMPACT, slots, &palette);
    wrong = unpainted = 0;
    for (unsigned y = 0; y < H; y++) for (unsigned x = 0; x < W; x++) {
        const int in_slots = y < 16u && x >= 80u && x < 80u + 4u * ORACLES_HOTBAR_SLOT_WIDTH;
        if (!in_slots && surface[y * W + x] != UNTOUCHED) wrong++;
        if (in_slots && surface[y * W + x] == UNTOUCHED) unpainted++;
    }
    CHECK(wrong == 0 && unpainted == 0);
    CHECK(count(surface, 80u, BADGE) > 20u && count(surface, 104u, INK) > 30u && count(surface, 128u, REFUSED) == 60u && count(surface, 152u, FAINT) == 31u);
    game_look(&palette, icon);
    centred_under_letter(&palette, icon);
    glued_to_the_bar(&palette, icon);
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_hotbar: ok\n");
    return 0;
}
