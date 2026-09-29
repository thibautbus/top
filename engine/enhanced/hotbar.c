#include "hotbar.h"

#include "hotbar_font.h"

#define SLOT_W ORACLES_HOTBAR_SLOT_WIDTH
#define SLOT_H 16u
#define ICON ORACLES_HOTBAR_ICON

unsigned oracles_hotbar_slot_x(unsigned n, unsigned width, unsigned core_width)
{
    const unsigned gutter = (width - core_width) / 2u;   /* 48: two slots a side, against the status bar (160 in the drawn-back view) */
    return n < 2u ? gutter - 2u * SLOT_W + n * SLOT_W : gutter + core_width + (n - 2u) * SLOT_W;
}

typedef struct canvas { uint32_t *pixels; unsigned width, x0; } canvas;   /* one slot: every write is clipped to its 24 x 16 */

static void put(const canvas *c, unsigned x, unsigned y, uint32_t colour)
{
    if (x < SLOT_W && y < SLOT_H) c->pixels[y * c->width + c->x0 + x] = colour;
}

static void text(const canvas *c, unsigned x, unsigned y, const char *s, unsigned max, uint32_t colour)
{
    for (unsigned i = 0; s && s[i] && i < max; i++) {
        const uint16_t glyph = hotbar_glyph(s[i]);
        for (unsigned bit = 0; bit < HOTBAR_GLYPH_WIDTH * HOTBAR_GLYPH_HEIGHT; bit++)
            if (glyph & (1u << (HOTBAR_GLYPH_WIDTH * HOTBAR_GLYPH_HEIGHT - 1u - bit)))
                put(c, x + i * (HOTBAR_GLYPH_WIDTH + 1u) + bit % HOTBAR_GLYPH_WIDTH, y + bit / HOTBAR_GLYPH_WIDTH, colour);
    }
}

static void frame(const canvas *c, uint32_t colour, int dotted)
{
    for (unsigned i = 0; i < ICON; i++) {
        if (dotted && (i & 1u)) continue;
        put(c, i, 0, colour); put(c, i, ICON - 1u, colour); put(c, 0, i, colour); put(c, ICON - 1u, i, colour);
    }
}

static uint32_t halfway(uint32_t a, uint32_t b)
{
    uint32_t out = 0xff000000u;
    for (unsigned shift = 0; shift < 24u; shift += 8u) out |= ((((a >> shift) & 0xffu) + 2u * ((b >> shift) & 0xffu)) / 3u) << shift;
    return out;
}

static void draw_slot(const canvas *c, const OraclesHotbarSlotView *s, const OraclesHotbarPalette *p)
{
    for (unsigned y = 0; y < SLOT_H; y++) for (unsigned x = 0; x < SLOT_W; x++) put(c, x, y, p->background);
    if (s->icon) {
        for (unsigned y = 0; y < ICON; y++)
            for (unsigned x = 0; x < ICON; x++) {
                const uint16_t colour = s->icon[y * ICON + x];
                if (!(colour & ORACLES_HOTBAR_ICON_OPAQUE)) continue;
                const uint32_t pixel = p->pixel(p->opaque, (uint16_t)(colour & 0x7fffu));
                put(c, x, y, s->dimmed ? halfway(pixel, p->background) : pixel);
            }
    } else if (s->label && s->label[0]) text(c, 1, 5, s->label, 4, s->dimmed ? p->faint : p->ink);
    if (s->frame == ORACLES_HOTBAR_FRAME_DOTTED) frame(c, p->faint, 1);
    else if (s->frame == ORACLES_HOTBAR_FRAME_SOLID) frame(c, p->ink, 0);
    else if (s->frame == ORACLES_HOTBAR_FRAME_REFUSED) frame(c, p->refused, 0);
    /* The column of eight at the right: the key at the top, two characters at most (LB), and the game's button on a
     * badge under it, which reads as a state and not as a second key. */
    text(c, ICON + 1u, 1, s->key, 2, p->ink);
    if (s->button) {
        for (unsigned y = 0; y < 7u; y++) for (unsigned x = 0; x < 7u; x++) put(c, ICON + 1u + x, 8u + y, p->badge);
        const char letter[2] = { s->button, 0 };
        text(c, ICON + 3u, 9, letter, 1, p->badge_ink);
    }
}

void oracles_hotbar_draw(uint32_t *surface, unsigned width, unsigned core_width, const OraclesHotbarSlotView slots[ORACLES_HOTBAR_SLOTS], const OraclesHotbarPalette *palette)
{
    for (unsigned n = 0; n < ORACLES_HOTBAR_SLOTS; n++) {
        const canvas c = { surface, width, oracles_hotbar_slot_x(n, width, core_width) };
        draw_slot(&c, &slots[n], palette);
    }
}
