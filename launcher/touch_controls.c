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

static int in_box(const OraclesTouchBox *b, float x, float y)
{
    const float grow = (REACH - 1.0f) * b->h;
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
    if (in_box(&layout->select, x, y)) return ORACLES_KEY_SELECT;
    if (in_box(&layout->start, x, y)) return ORACLES_KEY_START;
    return 0;
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
        *pause = touch->fingers[slot].on == ORACLES_TOUCH_ON_PAUSE;
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
        touch->fingers[slot].buttons = 0;
        return oracles_touch_pressed(touch);
    }
    switch (touch->fingers[slot].on) {
        case ORACLES_TOUCH_ON_DPAD: touch->fingers[slot].buttons = oracles_touch_dpad(&touch->layout.dpad, x, y); break;
        case ORACLES_TOUCH_ON_BUTTONS: touch->fingers[slot].buttons = oracles_touch_button_at(&touch->layout, x, y); break;
        default: touch->fingers[slot].buttons = 0; break;
    }
    return oracles_touch_pressed(touch);
}

OraclesTouchBox oracles_touch_frame(int width, int height, int frame_w, int frame_h)
{
    int scale = width / frame_w < height / frame_h ? width / frame_w : height / frame_h;
    if (scale < 1) scale = 1;
    return (OraclesTouchBox){ (float)((width - frame_w * scale) / 2), (float)((height - frame_h * scale) / 2),
                              (float)(frame_w * scale), (float)(frame_h * scale) };
}

void oracles_touch_release(OraclesTouch *touch)
{
    for (int i = 0; i < ORACLES_TOUCH_FINGERS; i++) { touch->fingers[i].used = 0; touch->fingers[i].buttons = 0; }
}

void oracles_touch_hide(OraclesTouch *touch)
{
    oracles_touch_release(touch);
    touch->shown = 0;
}
