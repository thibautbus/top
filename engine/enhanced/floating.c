#include "floating.h"

#include <string.h>

static const uint8_t pad_cells[6][2] = { { 1, 0 }, { 0, 1 }, { 0, 0 }, { 2, 0 }, { 1, 2 }, { 2, 1 } };   /* X Y LB RB, then B and A */

void oracles_floating_pad_cell(unsigned slot, unsigned *column, unsigned *row)
{
    const unsigned s = slot < 6u ? slot : 0u;
    *column = pad_cells[s][0];
    *row = pad_cells[s][1];
}

static const uint8_t keys_cells[6][2] = { { 0, 1 }, { 1, 1 }, { 0, 0 }, { 1, 0 }, { 0, 2 }, { 1, 2 } };   /* X Y LB RB, then B and A */

void oracles_floating_keys_cell(unsigned slot, unsigned *column, unsigned *row)
{
    const unsigned s = slot < 6u ? slot : 0u;
    *column = keys_cells[s][0];
    *row = keys_cells[s][1];
}

/* Inside the panel's rounded corners: the pixel's centre within the radius of the corner's own centre. */
static int inside(unsigned w, unsigned h, unsigned r, int x, int y)
{
    if (x < 0 || y < 0 || x >= (int)w || y >= (int)h) return 0;
    const int cx = x < (int)r ? (int)r : x >= (int)(w - r) ? (int)(w - r) - 1 : x;
    const int cy = y < (int)r ? (int)r : y >= (int)(h - r) ? (int)(h - r) - 1 : y;
    if (cx == x || cy == y) return 1;
    /* doubled: the centre of pixel x is 2x + 1; within half a pixel less than the radius of the corner's centre */
    const int dx = 2 * x + 1 - (x < cx ? 2 * cx : 2 * cx + 2), dy = 2 * y + 1 - (y < cy ? 2 * cy : 2 * cy + 2);
    return dx * dx + dy * dy <= (2 * (int)r - 1) * (2 * (int)r - 1);
}

unsigned oracles_floating_panel_pixel(unsigned w, unsigned h, unsigned radius, unsigned x, unsigned y)
{
    const int px = (int)x, py = (int)y;
    if (!inside(w, h, radius, px, py)) return 0u;
    return inside(w, h, radius, px - 1, py) && inside(w, h, radius, px + 1, py) && inside(w, h, radius, px, py - 1) && inside(w, h, radius, px, py + 1) ? 2u : 1u;
}

static void add(OraclesFloatingLayout *out, OraclesFloatingKind kind, unsigned slot, int32_t x, int32_t y, int32_t w, int32_t h, int top)
{
    if (out->count >= ORACLES_FLOATING_PANELS) return;
    OraclesFloatingPanel *p = &out->panels[out->count++];
    p->kind = kind; p->slot = slot; p->x = x; p->y = y; p->w = w; p->h = h; p->top = top;
}

/* Each panel's corner (oracles_floating_corner). */
static void anchor(const OraclesFloatingInput *in, OraclesFloatingLayout *out)
{
    for (unsigned i = 0; i < out->count; i++) {
        OraclesFloatingPanel *p = &out->panels[i];
        const int32_t rect[4] = { p->x, p->y, p->w, p->h };
        p->anchor = oracles_floating_corner(rect, in->width, in->height);
    }
}

static void lay_out(const OraclesFloatingInput *in, OraclesFloatingLayout *out);

void oracles_floating_layout(const OraclesFloatingInput *in, OraclesFloatingLayout *out)
{
    lay_out(in, out);
    anchor(in, out);
}

static void lay_out(const OraclesFloatingInput *in, OraclesFloatingLayout *out)
{
    memset(out, 0, sizeof *out);
    const int32_t m = (int32_t)(in->near_view ? ORACLES_FLOATING_MARGIN_NEAR : ORACLES_FLOATING_MARGIN), pad = (int32_t)ORACLES_FLOATING_PAD;
    const int32_t W = (int32_t)in->width, H = (int32_t)in->height, bar_h = 16 + 2 * pad;
    out->radius = ORACLES_FLOATING_RADIUS;
    out->pad = ORACLES_FLOATING_PAD;
    if (!in->known) {   /* the whole bar, at the top's middle */
        add(out, ORACLES_FLOATING_BAR, 0, (W - 160 - 2 * pad) / 2, m, 160 + 2 * pad, bar_h, 1);
        return;
    }
    const unsigned per_row = in->per_row ? in->per_row : 7u;
    const unsigned hearts = in->hearts && in->hearts <= 2u * per_row ? in->hearts : 2u * per_row;
    out->hearts_cols = hearts < per_row ? hearts : per_row;
    out->hearts_rows = hearts > per_row ? 2u : 1u;
    const int32_t hearts_w = (int32_t)(8u * out->hearts_cols) + 2 * pad, hearts_h = (int32_t)(8u * out->hearts_rows) + 2 * pad;
    add(out, ORACLES_FLOATING_HEARTS, 0, m, m, hearts_w, hearts_h, 1);
    /* The touch controls: the pause at the top-right, the rectangle whose right edge is the furthest right among those in
     * the top half; the money left of it. */
    int32_t money_right = W - m;
    for (unsigned i = 0; i < in->avoid_count && i < ORACLES_FLOATING_AVOID; i++) {
        const int32_t *a = in->avoid[i];
        if (a[1] < H / 2 && a[0] + a[2] > W / 2 && a[0] - m < money_right) money_right = a[0] - m;
    }
    const int32_t money_w = 24 + 2 * pad;
    add(out, ORACLES_FLOATING_MONEY, 0, money_right - money_w, m, money_w, bar_h, 1);
    const int32_t buttons_w = (int32_t)in->buttons + 2 * pad;
    if (in->avoid_count) {   /* under the hearts, clear of the pad the controls draw at the bottom-left */
        add(out, ORACLES_FLOATING_BUTTONS, 0, m, m + hearts_h + 2 * pad, buttons_w, bar_h, 0);
        return;
    }
    if (in->near_view || !in->pad) {
        add(out, ORACLES_FLOATING_BUTTONS, 0, m, H - m - bar_h, buttons_w, bar_h, 0);
        return;
    }
    const int32_t tw = (int32_t)ORACLES_FLOATING_TILE_W, th = (int32_t)ORACLES_FLOATING_TILE_H, gap = (int32_t)ORACLES_FLOATING_TILE_GAP;
    const int32_t top = H - m - 3 * th - 2 * gap;
    for (unsigned slot = 0; slot < 6u; slot++) {
        unsigned column, row;
        if (in->keyboard) oracles_floating_keys_cell(slot, &column, &row);
        else oracles_floating_pad_cell(slot, &column, &row);
        add(out, ORACLES_FLOATING_TILE, slot, m + (int32_t)column * (tw + gap), top + (int32_t)row * (th + gap), tw, th, 0);
    }
}
