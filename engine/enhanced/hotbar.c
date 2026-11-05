#include "hotbar.h"

#include "hotbar_font.h"
#include "hotbar_letters.h"

#define SLOT_H 16u
#define ICON ORACLES_HOTBAR_ICON

unsigned oracles_hotbar_slot_width(OraclesHotbarLook look) { return look == ORACLES_HOTBAR_GAME ? ORACLES_HOTBAR_GAME_SLOT_WIDTH : ORACLES_HOTBAR_SLOT_WIDTH; }

unsigned oracles_hotbar_width(OraclesHotbarLook look)
{
    return 4u * oracles_hotbar_slot_width(look) + (look == ORACLES_HOTBAR_GAME ? ORACLES_HOTBAR_GAME_LEAD : 0u);
}

int oracles_hotbar_fits(unsigned width, unsigned core_width) { return width >= core_width + 4u * ORACLES_HOTBAR_SLOT_WIDTH; }

OraclesHotbarLook oracles_hotbar_gutter_look(unsigned width, unsigned core_width)
{
    return width >= core_width + 2u * (2u * ORACLES_HOTBAR_GAME_SLOT_WIDTH + ORACLES_HOTBAR_GAME_LEAD) ? ORACLES_HOTBAR_GAME : ORACLES_HOTBAR_COMPACT;
}

/* Two slots a side, against the status bar: on the left ending at its first column (blank, before its B), on the right
 * after the game's lead past its last. */
unsigned oracles_hotbar_slot_x(unsigned n, unsigned width, unsigned core_width)
{
    const OraclesHotbarLook look = oracles_hotbar_gutter_look(width, core_width);
    const unsigned w = oracles_hotbar_slot_width(look), lead = look == ORACLES_HOTBAR_GAME ? ORACLES_HOTBAR_GAME_LEAD : 0u;
    const unsigned gutter = (width - core_width) / 2u;
    return n < 2u ? gutter - 2u * w + n * w : gutter + core_width + lead + (n - 2u) * w;
}

typedef struct canvas { uint32_t *pixels; unsigned width, x0, w; } canvas;   /* one slot: every write is clipped to its w x 16 */

static void put(const canvas *c, unsigned x, unsigned y, uint32_t colour)
{
    if (x < c->w && y < SLOT_H) c->pixels[y * c->width + c->x0 + x] = colour;
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

/* The icon from column `x0`, or the label in its place. */
static void draw_icon(const canvas *c, unsigned x0, const OraclesHotbarSlotView *s, const OraclesHotbarPalette *p)
{
    if (s->icon) {
        for (unsigned y = 0; y < ICON; y++)
            for (unsigned x = 0; x < ICON; x++) {
                const uint16_t colour = s->icon[y * ICON + x];
                if (!(colour & ORACLES_HOTBAR_ICON_OPAQUE)) continue;
                const uint32_t pixel = p->pixel(p->opaque, (uint16_t)(colour & 0x7fffu));
                put(c, x0 + x, y, s->dimmed ? halfway(pixel, p->background) : pixel);
            }
    } else if (s->label && s->label[0]) text(c, x0 + 1u, 5, s->label, 4, s->dimmed ? p->faint : p->ink);
}

static void fill(const canvas *c, uint32_t colour) { for (unsigned y = 0; y < SLOT_H; y++) for (unsigned x = 0; x < c->w; x++) put(c, x, y, colour); }

/* The button the item sits on, as a badge: its corner and size, the letter in it. */
static void badge(const canvas *c, unsigned x0, unsigned y0, unsigned w, unsigned h, char button, const OraclesHotbarPalette *p)
{
    for (unsigned y = 0; y < h; y++) for (unsigned x = 0; x < w; x++) put(c, x0 + x, y0 + y, p->badge);
    const char letter[2] = { button, 0 };
    text(c, x0 + (w - HOTBAR_GLYPH_WIDTH) / 2u, y0 + (h - HOTBAR_GLYPH_HEIGHT) / 2u, letter, 1, p->badge_ink);
}

static void draw_slot(const canvas *c, const OraclesHotbarSlotView *s, const OraclesHotbarPalette *p)
{
    fill(c, p->background);
    draw_icon(c, 0, s, p);
    if (s->frame == ORACLES_HOTBAR_FRAME_DOTTED) frame(c, p->faint, 1);
    else if (s->frame == ORACLES_HOTBAR_FRAME_SOLID) frame(c, p->ink, 0);
    else if (s->frame == ORACLES_HOTBAR_FRAME_REFUSED) frame(c, p->refused, 0);
    /* The column of eight at the right: the key at the top, two characters at most (LB), and the game's button on a
     * badge under it, which reads as a state and not as a second key. */
    text(c, ICON + 1u, 1, s->key, 2, p->ink);
    if (s->button) badge(c, ICON + 1u, 8, 7, 7, s->button, p);
}

/* ---- the game's look ---------------------------------------------------------------------- */

#define LETTER_X 1u   /* the bar's B stands one column in */

unsigned oracles_hotbar_game_bracket(const char *key)
{
    const HotbarLetter *letter = key && key[0] ? hotbar_letter(key[0]) : NULL;
    const unsigned after = LETTER_X + (letter ? letter->width : 4u) + 1u;
    return after > 6u ? after : 6u;   /* the column of B's: a narrow letter leaves the badge under it its own gap too */
}

unsigned oracles_hotbar_game_letter_middle(const char *key)
{
    const HotbarLetter *first = key && key[0] ? hotbar_letter(key[0]) : NULL;
    return LETTER_X + ((first ? first->width : 4u) - 1u) / 2u;
}

static void letter(const canvas *c, char ch, unsigned x0, unsigned y0, uint32_t colour)
{
    const HotbarLetter *g = hotbar_letter(ch);
    if (!g) return;
    for (unsigned y = 0; y < HOTBAR_LETTER_HEIGHT; y++)
        for (unsigned x = 0; x < g->width; x++) if (g->rows[y] & (0x10u >> x)) put(c, x0 + x, y0 + y, colour);
}

/* A bracket of the bar: two columns, its upright on lines 1 to 14, the serifs on the first and last toward the icon. */
static void bracket(const canvas *c, unsigned upright, int closing, uint32_t colour, int dotted)
{
    for (unsigned y = 1; y <= 14u; y++) if (!dotted || (y & 1u)) put(c, upright, y, colour);
    const unsigned serif = closing ? upright - 1u : upright + 1u;
    put(c, serif, 1, colour);
    put(c, serif, 14, colour);
}

/* The key's letter at the bar's size on the top seven lines, and under it on the free lines either a second character,
 * small (LB, RB), or the button the item sits on, which takes its place while it does. */
static void draw_slot_game(const canvas *c, const OraclesHotbarSlotView *s, const OraclesHotbarPalette *p)
{
    fill(c, p->background);
    const char *key = s->key ? s->key : "";
    if (key[0]) letter(c, key[0], LETTER_X, 1, p->ink);
    /* Under the letter, centred on its middle column (the left one of two, for a letter of even width): the badge, five
     * wide, or the small second character, three. */
    const unsigned middle = oracles_hotbar_game_letter_middle(key);
    if (s->button) badge(c, middle - 2u, 9, 5, 7, s->button, p);
    else if (key[0] && key[1]) text(c, middle - 1u, 10, key + 1, 1, p->ink);
    const unsigned open = oracles_hotbar_game_bracket(key), icon = open + 2u;
    uint32_t ink = p->ink;
    int dotted = 0;
    if (s->frame == ORACLES_HOTBAR_FRAME_DOTTED) { ink = p->faint; dotted = 1; }
    else if (s->frame == ORACLES_HOTBAR_FRAME_REFUSED) ink = p->refused;
    bracket(c, open, 0, ink, dotted);
    bracket(c, open + 27u, 1, ink, dotted);
    if (s->picture) {
        for (unsigned y = 0; y < SLOT_H; y++)
            for (unsigned x = 0; x < ORACLES_HOTBAR_GAME_PICTURE_WIDTH; x++) {
                const uint16_t colour = s->picture[y * ORACLES_HOTBAR_GAME_PICTURE_WIDTH + x];
                if (!(colour & ORACLES_HOTBAR_ICON_OPAQUE)) continue;
                const uint32_t pixel = p->pixel(p->opaque, (uint16_t)(colour & 0x7fffu));
                put(c, icon + x, y, s->dimmed ? halfway(pixel, p->background) : pixel);
            }
    } else draw_icon(c, icon, s, p);
    /* waiting: a line under the icon, as the compact look's frame blinks, the item on a button or not */
    if (s->waiting) for (unsigned x = 0; x < ICON; x++) put(c, icon + x, 15, p->ink);
}

static void draw_look(const canvas *c, OraclesHotbarLook look, const OraclesHotbarSlotView *s, const OraclesHotbarPalette *p)
{
    if (look == ORACLES_HOTBAR_GAME) draw_slot_game(c, s, p); else draw_slot(c, s, p);
}

void oracles_hotbar_draw(uint32_t *surface, unsigned width, unsigned core_width, int glued, const OraclesHotbarSlotView slots[ORACLES_HOTBAR_SLOTS], const OraclesHotbarPalette *palette)
{
    if (!oracles_hotbar_fits(width, core_width)) return;
    const OraclesHotbarLook look = oracles_hotbar_gutter_look(width, core_width);
    if (glued && look == ORACLES_HOTBAR_GAME) {
        const unsigned right = (width - core_width) / 2u + core_width;   /* the left pair ends against the bar: nothing between */
        for (unsigned y = 0; y < SLOT_H; y++) for (unsigned k = 0; k < ORACLES_HOTBAR_GAME_LEAD; k++) surface[y * width + right + k] = palette->background;
    }
    for (unsigned n = 0; n < ORACLES_HOTBAR_SLOTS; n++) {
        const canvas c = { surface, width, oracles_hotbar_slot_x(n, width, core_width), oracles_hotbar_slot_width(look) };
        draw_look(&c, look, &slots[n], palette);
    }
}

void oracles_hotbar_draw_at(uint32_t *surface, unsigned width, unsigned x, OraclesHotbarLook look, const OraclesHotbarSlotView slots[ORACLES_HOTBAR_SLOTS], const OraclesHotbarPalette *palette)
{
    const unsigned w = oracles_hotbar_slot_width(look), lead = look == ORACLES_HOTBAR_GAME ? ORACLES_HOTBAR_GAME_LEAD : 0u;
    for (unsigned y = 0; y < SLOT_H; y++) for (unsigned k = 0; k < lead; k++) surface[y * width + x + k] = palette->background;
    for (unsigned n = 0; n < ORACLES_HOTBAR_SLOTS; n++) {
        const canvas c = { surface, width, x + lead + n * w, w };
        draw_look(&c, look, &slots[n], palette);
    }
}
