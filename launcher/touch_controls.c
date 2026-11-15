#include "touch_controls.h"

#include "core.h"

#include <math.h>
#include <string.h>

/* Beyond its drawing, a control takes a finger this much further: a thumb lands roughly. */
#define REACH 1.3f
/* Near the d-pad's centre no direction; past it, a direction within 67.5 degrees of an axis holds that axis, which
 * gives the diagonals between (sin 22.5 degrees). */
#define DPAD_DEAD 0.2f
#define DPAD_AXIS 0.3827f
/* What a finger feels pressed, which the vibration ticks for, is steadier than what the game is given: a direction held
 * is felt a little past where the game lets it go, a new one only a little past where the game takes it, the dead
 * centre lets them go only well inside it, and a button is felt a little beyond its reach.  A thumb resting on a border,
 * which the game's directions flicker across, ticks once. */
#define FELT_AXIS 0.06f
#define FELT_DEAD 0.15f
#define FELT_REACH 0.1f

/* The screen's rectangle x0, y0 to x1, y1 in the frame's pixels at `to`, rounded out and cut to the frame; 0 for none. */
static int covered(float x0, float y0, float x1, float y1, OraclesTouchBox to, int frame_w, int frame_h, int32_t out[4])
{
    if (to.w <= 0.0f || to.h <= 0.0f) return 0;
    float fx0 = (x0 - to.x) * (float)frame_w / to.w, fy0 = (y0 - to.y) * (float)frame_h / to.h;
    float fx1 = (x1 - to.x) * (float)frame_w / to.w, fy1 = (y1 - to.y) * (float)frame_h / to.h;
    if (fx0 < 0.0f) fx0 = 0.0f;
    if (fy0 < 0.0f) fy0 = 0.0f;
    if (fx1 > (float)frame_w) fx1 = (float)frame_w;
    if (fy1 > (float)frame_h) fy1 = (float)frame_h;
    if (fx0 >= fx1 || fy0 >= fy1) return 0;
    out[0] = (int32_t)fx0; out[1] = (int32_t)fy0;
    out[2] = (int32_t)(fx1 + 0.999f) - out[0]; out[3] = (int32_t)(fy1 + 0.999f) - out[1];
    return 1;
}

unsigned oracles_touch_frame_covered(const OraclesTouchLayout *l, OraclesTouchBox to, int frame_w, int frame_h, int32_t out[4][4])
{
    unsigned n = 0;
    const OraclesTouchCircle *d = &l->dpad, *a = &l->a, *b = &l->b, *p = &l->pause;
    const float ab_x0 = (a->x - a->r < b->x - b->r ? a->x - a->r : b->x - b->r), ab_y0 = (a->y - a->r < b->y - b->r ? a->y - a->r : b->y - b->r);
    const float ab_x1 = (a->x + a->r > b->x + b->r ? a->x + a->r : b->x + b->r), ab_y1 = (a->y + a->r > b->y + b->r ? a->y + a->r : b->y + b->r);
    n += (unsigned)covered(d->x - d->r, d->y - d->r, d->x + d->r, d->y + d->r, to, frame_w, frame_h, out[n]);
    n += (unsigned)covered(ab_x0, ab_y0, ab_x1, ab_y1, to, frame_w, frame_h, out[n]);
    n += (unsigned)covered(p->x - p->r, p->y - p->r, p->x + p->r, p->y + p->r, to, frame_w, frame_h, out[n]);
    n += (unsigned)covered(l->select.x, l->select.y, l->start.x + l->start.w, l->start.y + l->start.h, to, frame_w, frame_h, out[n]);
    return n;
}

void oracles_touch_layout(float width, float height, OraclesTouchLayout *out)
{
    memset(out, 0, sizeof *out);
    out->width = width;
    out->height = height;
    const float unit = width < height ? width : height;   /* the short side */
    const float margin = 0.05f * unit;
    out->dpad = (OraclesTouchCircle){ margin + 0.17f * unit, height - margin - 0.17f * unit, 0.17f * unit };
    /* A higher and at the right, B lower and at its left, as on the Game Boy. */
    const float button = 0.085f * unit;
    out->a = (OraclesTouchCircle){ width - margin - button, height - margin - 2.2f * button, button };
    out->b = (OraclesTouchCircle){ out->a.x - 2.4f * button, out->a.y + 1.1f * button, button };
    /* Select and Start side by side, out of the d-pad's and B's reach: centred on the screen when the room between
     * them allows, else within it; on a screen too narrow for it (a split screen, a tablet held upright), a row above
     * them all. */
    const float pill_w = 0.13f * unit, pill_h = 0.055f * unit, between = 0.08f * unit, pair = 2.0f * pill_w + between;
    const float clear = 0.03f * unit;   /* beyond a reach, more than a pill's own reach */
    const float left = out->dpad.x + REACH * out->dpad.r + clear, right = out->b.x - REACH * out->b.r - clear;
    float pill_x = 0.5f * (width - pair), pill_y = height - 0.6f * margin - pill_h;
    if (pill_x < left) pill_x = left;
    if (pill_x + pair > right) pill_x = right - pair;
    if (pill_x < left) {
        const float dpad_top = out->dpad.y - REACH * out->dpad.r, a_top = out->a.y - REACH * out->a.r;
        pill_x = 0.5f * (width - pair);
        pill_y = (dpad_top < a_top ? dpad_top : a_top) - clear - REACH * pill_h;
    }
    out->select = (OraclesTouchBox){ pill_x, pill_y, pill_w, pill_h };
    out->start = (OraclesTouchBox){ pill_x + pill_w + between, pill_y, pill_w, pill_h };
    out->pause = (OraclesTouchCircle){ width - margin - 0.05f * unit, margin + 0.05f * unit, 0.05f * unit };
}

static int in_circle(const OraclesTouchCircle *c, float x, float y, float reach)
{
    const float dx = x - c->x, dy = y - c->y, r = c->r * reach;
    return dx * dx + dy * dy <= r * r;
}

static int in_box(const OraclesTouchBox *b, float x, float y, float reach)
{
    const float grow = (reach - 1.0f) * b->h;
    return x >= b->x - grow && x <= b->x + b->w + grow && y >= b->y - grow && y <= b->y + b->h + grow;
}

unsigned oracles_touch_dpad(const OraclesTouchCircle *dpad, float x, float y)
{
    const float dx = x - dpad->x, dy = y - dpad->y, length = sqrtf(dx * dx + dy * dy);
    if (length < DPAD_DEAD * dpad->r) return 0;
    unsigned keys = 0;
    if (dx > DPAD_AXIS * length) keys |= ORACLES_KEY_RIGHT;
    if (dx < -DPAD_AXIS * length) keys |= ORACLES_KEY_LEFT;
    if (dy > DPAD_AXIS * length) keys |= ORACLES_KEY_DOWN;
    if (dy < -DPAD_AXIS * length) keys |= ORACLES_KEY_UP;
    return keys;
}

unsigned oracles_touch_button_at(const OraclesTouchLayout *layout, float x, float y)
{
    /* The nearest of A and B when a finger reaches both, as it can between them. */
    const int on_a = in_circle(&layout->a, x, y, REACH), on_b = in_circle(&layout->b, x, y, REACH);
    if (on_a && on_b) {
        const float da = (x - layout->a.x) * (x - layout->a.x) + (y - layout->a.y) * (y - layout->a.y);
        const float db = (x - layout->b.x) * (x - layout->b.x) + (y - layout->b.y) * (y - layout->b.y);
        return da <= db ? ORACLES_KEY_A : ORACLES_KEY_B;
    }
    if (on_a) return ORACLES_KEY_A;
    if (on_b) return ORACLES_KEY_B;
    if (in_box(&layout->select, x, y, REACH)) return ORACLES_KEY_SELECT;
    if (in_box(&layout->start, x, y, REACH)) return ORACLES_KEY_START;
    return 0;
}

/* The directions a finger on the d-pad feels at (x, y), having felt `felt`. */
static unsigned felt_dpad(const OraclesTouchCircle *dpad, unsigned felt, float x, float y)
{
    if (!felt) return oracles_touch_dpad(dpad, x, y);
    const float dx = x - dpad->x, dy = y - dpad->y, length = sqrtf(dx * dx + dy * dy);
    if (length < FELT_DEAD * dpad->r) return 0;
    const float keep = (DPAD_AXIS - FELT_AXIS) * length, add = (DPAD_AXIS + FELT_AXIS) * length;
    unsigned keys = 0;
    if (dx > ((felt & ORACLES_KEY_RIGHT) ? keep : add)) keys |= ORACLES_KEY_RIGHT;
    if (dx < -((felt & ORACLES_KEY_LEFT) ? keep : add)) keys |= ORACLES_KEY_LEFT;
    if (dy > ((felt & ORACLES_KEY_DOWN) ? keep : add)) keys |= ORACLES_KEY_DOWN;
    if (dy < -((felt & ORACLES_KEY_UP) ? keep : add)) keys |= ORACLES_KEY_UP;
    return keys;
}

/* The button a finger feels at (x, y), having felt `felt`. */
static unsigned felt_button(const OraclesTouchLayout *layout, unsigned felt, float x, float y)
{
    const float reach = REACH + FELT_REACH;
    const int still = (felt == ORACLES_KEY_A && in_circle(&layout->a, x, y, reach)) || (felt == ORACLES_KEY_B && in_circle(&layout->b, x, y, reach))
                      || (felt == ORACLES_KEY_SELECT && in_box(&layout->select, x, y, reach))
                      || (felt == ORACLES_KEY_START && in_box(&layout->start, x, y, reach));
    return still ? felt : oracles_touch_button_at(layout, x, y);
}

int oracles_touch_on_pause(const OraclesTouchLayout *layout, float x, float y)
{
    return in_circle(&layout->pause, x, y, REACH);
}

unsigned oracles_touch_pressed(const OraclesTouch *touch)
{
    unsigned keys = 0;
    for (int i = 0; i < ORACLES_TOUCH_FINGERS; i++)
        if (touch->fingers[i].used) keys |= touch->fingers[i].buttons;
    return keys;
}

unsigned oracles_touch_finger(OraclesTouch *touch, float width, float height, uint64_t id, OraclesTouchFingerKind kind,
                              float x, float y, int *pause)
{
    *pause = 0;
    touch->tick = 0;
    touch->shown = 1;
    if (touch->layout.width != width || touch->layout.height != height) oracles_touch_layout(width, height, &touch->layout);
    int slot = -1, free_slot = -1;
    for (int i = 0; i < ORACLES_TOUCH_FINGERS; i++) {
        if (touch->fingers[i].used && touch->fingers[i].id == id) slot = i;
        else if (!touch->fingers[i].used && free_slot < 0) free_slot = i;
    }
    if (kind == ORACLES_TOUCH_DOWN) {
        if (slot < 0) slot = free_slot;
        if (slot < 0) return oracles_touch_pressed(touch);   /* an eleventh finger: none of the game's */
        touch->fingers[slot].used = 1;
        touch->fingers[slot].id = id;
        const OraclesTouchLayout *l = &touch->layout;
        touch->fingers[slot].on = oracles_touch_on_pause(l, x, y) ? ORACLES_TOUCH_ON_PAUSE
                                  : in_circle(&l->dpad, x, y, REACH) ? ORACLES_TOUCH_ON_DPAD
                                  : oracles_touch_button_at(l, x, y) ? ORACLES_TOUCH_ON_BUTTONS : ORACLES_TOUCH_ON_NOTHING;
        *pause = touch->tick = touch->fingers[slot].on == ORACLES_TOUCH_ON_PAUSE;
    } else if (slot < 0 && kind == ORACLES_TOUCH_MOTION) {
        /* A finger that went down before the controls took it, one held through a pause: taken as it moves, as if it
         * went down there, the pause's excepted, which only a finger going down on it opens. */
        if (free_slot < 0) return oracles_touch_pressed(touch);
        slot = free_slot;
        touch->fingers[slot].used = 1;
        touch->fingers[slot].id = id;
        const OraclesTouchLayout *l = &touch->layout;
        touch->fingers[slot].on = in_circle(&l->dpad, x, y, REACH) ? ORACLES_TOUCH_ON_DPAD
                                  : oracles_touch_button_at(l, x, y) ? ORACLES_TOUCH_ON_BUTTONS : ORACLES_TOUCH_ON_NOTHING;
    } else if (slot < 0) {
        return oracles_touch_pressed(touch);
    }
    if (kind == ORACLES_TOUCH_UP) {
        touch->fingers[slot].used = 0;
        touch->fingers[slot].buttons = touch->fingers[slot].felt = 0;
        return oracles_touch_pressed(touch);
    }
    /* A tick for a direction the d-pad's finger did not feel, added to those it felt (up to up and right, not on to
     * right), or a button other than the one it felt. */
    const unsigned felt = touch->fingers[slot].felt;
    switch (touch->fingers[slot].on) {
        case ORACLES_TOUCH_ON_DPAD:
            touch->fingers[slot].buttons = oracles_touch_dpad(&touch->layout.dpad, x, y);
            touch->fingers[slot].felt = felt_dpad(&touch->layout.dpad, felt, x, y);
            touch->tick |= (touch->fingers[slot].felt & ~felt) != 0;
            break;
        case ORACLES_TOUCH_ON_BUTTONS:
            touch->fingers[slot].buttons = oracles_touch_button_at(&touch->layout, x, y);
            touch->fingers[slot].felt = felt_button(&touch->layout, felt, x, y);
            touch->tick |= touch->fingers[slot].felt && touch->fingers[slot].felt != felt;
            break;
        default: touch->fingers[slot].buttons = touch->fingers[slot].felt = 0; break;
    }
    return oracles_touch_pressed(touch);
}

OraclesTouchBox oracles_touch_frame(int width, int height, int frame_w, int frame_h, int fill)
{
    if (fill) {
        /* SDL's letterbox: the side that limits takes the screen's, the other its scale floored. */
        const float want = (float)frame_w / (float)frame_h, real = (float)width / (float)height;
        if (fabsf(want - real) < 0.0001f) return (OraclesTouchBox){ 0.0f, 0.0f, (float)width, (float)height };
        if (want > real) {
            const float h = floorf((float)frame_h * ((float)width / (float)frame_w));
            return (OraclesTouchBox){ 0.0f, ((float)height - h) / 2.0f, (float)width, h };
        }
        const float w = floorf((float)frame_w * ((float)height / (float)frame_h));
        return (OraclesTouchBox){ ((float)width - w) / 2.0f, 0.0f, w, (float)height };
    }
    int scale = width / frame_w < height / frame_h ? width / frame_w : height / frame_h;
    if (scale < 1) scale = 1;
    return (OraclesTouchBox){ (float)((width - frame_w * scale) / 2), (float)((height - frame_h * scale) / 2),
                              (float)(frame_w * scale), (float)(frame_h * scale) };
}

const char *const oracles_screen_margin_names[ORACLES_SCREEN_MARGINS] = { "off", "auto", "small", "large" };

OraclesTouchBox oracles_touch_band(int width, int height, const OraclesTouchEdges *edges)
{
    static const float dp[ORACLES_SCREEN_MARGINS] = { 0.0f, 0.0f, 24.0f, 48.0f };
    int top = 0, bottom = 0;
    if (edges->margin > ORACLES_SCREEN_MARGIN_OFF && edges->margin < ORACLES_SCREEN_MARGINS) {
        const int fixed = (int)(dp[edges->margin] * (edges->density > 0.0f ? edges->density : 1.0f) + 0.5f);
        top = edges->inset_top > fixed ? edges->inset_top : fixed;
        bottom = edges->inset_bottom > fixed ? edges->inset_bottom : fixed;
    }
    if (top < 0 || bottom < 0 || (top + bottom) * 2 > height) top = bottom = 0;
    return (OraclesTouchBox){ 0.0f, (float)top, (float)width, (float)(height - top - bottom) };
}

void oracles_touch_release(OraclesTouch *touch)
{
    for (int i = 0; i < ORACLES_TOUCH_FINGERS; i++) { touch->fingers[i].used = 0; touch->fingers[i].buttons = touch->fingers[i].felt = 0; }
}

void oracles_touch_hide(OraclesTouch *touch)
{
    oracles_touch_release(touch);
    touch->shown = 0;
}
