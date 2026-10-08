/* The hotbar's drawing, without a ROM: where the four
 * slots stand, that nothing is ever written in the game's 160 columns nor
 * under the HUD band, and what each state of a slot puts on screen. */
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

int main(void)
{
    static uint32_t surface[W * H];
    const OraclesHotbarPalette palette = { BACKGROUND, INK, FAINT, BADGE, BADGE_INK, REFUSED, icon_pixel, NULL };
    CHECK(oracles_hotbar_slot_x(0, W, CORE) == 0 && oracles_hotbar_slot_x(1, W, CORE) == 24 && oracles_hotbar_slot_x(2, W, CORE) == 208 && oracles_hotbar_slot_x(3, W, CORE) == 232);
    /* The drawn-back view's 480: two slots against each side of the status bar, not at the surface's edges. */
    CHECK(oracles_hotbar_slot_x(0, 480u, CORE) == 112 && oracles_hotbar_slot_x(1, 480u, CORE) == 136 && oracles_hotbar_slot_x(2, 480u, CORE) == 320 && oracles_hotbar_slot_x(3, 480u, CORE) == 344);
    /* The near view in 4:3, 213 wide, has 26 px a side: no room for two slots, nothing is drawn; the other sizes fit. */
    CHECK(!oracles_hotbar_fits(213u, CORE) && oracles_hotbar_fits(256u, CORE) && oracles_hotbar_fits(320u, CORE) && oracles_hotbar_fits(384u, CORE));
    {
        static uint32_t narrow[213u * 16u];
        for (unsigned i = 0; i < 213u * 16u; i++) narrow[i] = 0x12345678u;
        OraclesHotbarSlotView none[ORACLES_HOTBAR_SLOTS];
        memset(none, 0, sizeof none);
        oracles_hotbar_draw(narrow, 213u, CORE, none, &palette);
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
    oracles_hotbar_draw(surface, W, CORE, slots, &palette);
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
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_hotbar: ok\n");
    return 0;
}
