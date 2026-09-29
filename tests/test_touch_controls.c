/* The touch controls' half without SDL (launcher/touch_controls.c): the controls lie inside a phone's, a handheld's, a
 * tablet's and a split or upright screen without overlapping, Select and Start answering wherever they are drawn; the
 * d-pad gives eight directions and none at its centre; A, B, Select, Start and the pause are found where they are drawn
 * and a little beyond; fingers press together, a finger held by the d-pad keeps its direction when it slides off it,
 * one sliding from A to B moves the press, a finger going up releases, one held through a pause presses again as it
 * moves; the frame goes where SDL's integer-scaled logical presentation puts it. */
#include "touch_controls.h"

#include "core.h"

#include <stdio.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

static int circle_inside(const OraclesTouchCircle *c, float w, float h)
{
    return c->x - c->r >= 0 && c->y - c->r >= 0 && c->x + c->r <= w && c->y + c->r <= h;
}

static int apart(const OraclesTouchCircle *p, const OraclesTouchCircle *q)
{
    const float dx = p->x - q->x, dy = p->y - q->y, r = p->r + q->r;
    return dx * dx + dy * dy > r * r;
}

/* Every point of a box, on a grid, gives `key` to a finger going down there. */
static int answers(const OraclesTouchBox *box, float w, float h, unsigned key)
{
    for (int i = 0; i <= 10; i++)
        for (int j = 0; j <= 4; j++) {
            OraclesTouch t = { 0 };
            int pause = 0;
            if (oracles_touch_finger(&t, w, h, 1, ORACLES_TOUCH_DOWN, box->x + box->w * (float)i / 10, box->y + box->h * (float)j / 4, &pause) != key) return 0;
        }
    return 1;
}

static void screens(float w, float h)
{
    OraclesTouchLayout l;
    oracles_touch_layout(w, h, &l);
    CHECK(circle_inside(&l.dpad, w, h) && circle_inside(&l.a, w, h) && circle_inside(&l.b, w, h) && circle_inside(&l.pause, w, h));
    CHECK(apart(&l.a, &l.b) && apart(&l.dpad, &l.b) && apart(&l.pause, &l.a));
    CHECK(l.select.x + l.select.w < l.start.x && l.select.y >= 0 && l.select.y + l.select.h <= h);
    CHECK(l.a.y < l.b.y && l.a.x > l.b.x);   /* A higher and at the right */
    /* Select and Start answer wherever they are drawn, never the d-pad or B. */
    CHECK(answers(&l.select, w, h, ORACLES_KEY_SELECT) && answers(&l.start, w, h, ORACLES_KEY_START));
}

int main(void)
{
    screens(2400, 1080);   /* a phone */
    screens(1920, 1080);   /* a handheld */
    screens(1280, 1024);   /* a squarer screen */
    screens(2048, 1536);   /* a tablet */
    screens(1200, 1080);   /* half a phone, split screen */
    screens(960, 1080);
    screens(1080, 2400);   /* a phone upright, a tablet Android keeps upright */
    screens(1600, 2560);
    screens(1080, 1200);
    OraclesTouchLayout wide;
    oracles_touch_layout(2400, 1080, &wide);
    CHECK(wide.select.y + wide.select.h > wide.b.y);   /* where there is room, at the bottom as on the Game Boy */

    /* The frame where SDL's integer-scaled logical presentation puts it (the SDL backend's test compares it with SDL). */
    OraclesTouchBox f = oracles_touch_frame(2400, 1080, 160, 144);
    CHECK(f.x == 640 && f.y == 36 && f.w == 1120 && f.h == 1008);
    f = oracles_touch_frame(2400, 1080, 480, 270);
    CHECK(f.x == 240 && f.y == 0 && f.w == 1920 && f.h == 1080);
    f = oracles_touch_frame(100, 80, 160, 144);   /* smaller than the frame: scale 1 */
    CHECK(f.w == 160 && f.h == 144);

    OraclesTouchLayout l;
    oracles_touch_layout(2400, 1080, &l);
    const OraclesTouchCircle *d = &l.dpad;
    CHECK(oracles_touch_dpad(d, d->x, d->y) == 0);
    CHECK(oracles_touch_dpad(d, d->x + 0.1f * d->r, d->y) == 0);   /* the dead centre */
    CHECK(oracles_touch_dpad(d, d->x + 0.7f * d->r, d->y) == ORACLES_KEY_RIGHT);
    CHECK(oracles_touch_dpad(d, d->x - 0.7f * d->r, d->y) == ORACLES_KEY_LEFT);
    CHECK(oracles_touch_dpad(d, d->x, d->y - 0.7f * d->r) == ORACLES_KEY_UP);
    CHECK(oracles_touch_dpad(d, d->x, d->y + 0.7f * d->r) == ORACLES_KEY_DOWN);
    CHECK(oracles_touch_dpad(d, d->x + 0.5f * d->r, d->y - 0.5f * d->r) == (ORACLES_KEY_RIGHT | ORACLES_KEY_UP));
    CHECK(oracles_touch_dpad(d, d->x - 0.5f * d->r, d->y + 0.5f * d->r) == (ORACLES_KEY_LEFT | ORACLES_KEY_DOWN));
    CHECK(oracles_touch_dpad(d, d->x + 0.7f * d->r, d->y - 0.2f * d->r) == ORACLES_KEY_RIGHT);   /* near the axis: straight */

    CHECK(oracles_touch_button_at(&l, l.a.x, l.a.y) == ORACLES_KEY_A);
    CHECK(oracles_touch_button_at(&l, l.b.x, l.b.y) == ORACLES_KEY_B);
    CHECK(oracles_touch_button_at(&l, l.a.x + 1.2f * l.a.r, l.a.y) == ORACLES_KEY_A);   /* a little beyond */
    CHECK(oracles_touch_button_at(&l, l.select.x + 1, l.select.y + 1) == ORACLES_KEY_SELECT);
    CHECK(oracles_touch_button_at(&l, l.start.x + l.start.w - 1, l.start.y + 1) == ORACLES_KEY_START);
    CHECK(oracles_touch_button_at(&l, 1200, 300) == 0);   /* the middle of the screen: the game */
    CHECK(oracles_touch_on_pause(&l, l.pause.x, l.pause.y) && !oracles_touch_on_pause(&l, 1200, 300));

    OraclesTouch t = { 0 };
    int pause = 0;
    CHECK(!t.shown);
    CHECK(oracles_touch_finger(&t, 2400, 1080, 1, ORACLES_TOUCH_DOWN, d->x + 0.7f * d->r, d->y, &pause) == ORACLES_KEY_RIGHT && t.shown && !pause);
    CHECK(oracles_touch_finger(&t, 2400, 1080, 2, ORACLES_TOUCH_DOWN, l.a.x, l.a.y, &pause) == (ORACLES_KEY_RIGHT | ORACLES_KEY_A));
    /* The d-pad's finger slides far up and off the d-pad: still the d-pad's, now up. */
    CHECK(oracles_touch_finger(&t, 2400, 1080, 1, ORACLES_TOUCH_MOTION, d->x, d->y - 3.0f * d->r, &pause) == (ORACLES_KEY_UP | ORACLES_KEY_A));
    /* A's finger rolls onto B. */
    CHECK(oracles_touch_finger(&t, 2400, 1080, 2, ORACLES_TOUCH_MOTION, l.b.x, l.b.y, &pause) == (ORACLES_KEY_UP | ORACLES_KEY_B));
    CHECK(oracles_touch_finger(&t, 2400, 1080, 1, ORACLES_TOUCH_UP, d->x, d->y, &pause) == ORACLES_KEY_B);
    CHECK(oracles_touch_finger(&t, 2400, 1080, 2, ORACLES_TOUCH_UP, l.b.x, l.b.y, &pause) == 0);
    /* A finger on the game presses nothing, and does not become the d-pad's as it slides there. */
    CHECK(oracles_touch_finger(&t, 2400, 1080, 3, ORACLES_TOUCH_DOWN, 1200, 300, &pause) == 0);
    CHECK(oracles_touch_finger(&t, 2400, 1080, 3, ORACLES_TOUCH_MOTION, d->x + 0.7f * d->r, d->y, &pause) == 0);
    oracles_touch_finger(&t, 2400, 1080, 3, ORACLES_TOUCH_UP, 0, 0, &pause);
    CHECK(oracles_touch_finger(&t, 2400, 1080, 4, ORACLES_TOUCH_DOWN, l.pause.x, l.pause.y, &pause) == 0 && pause);
    oracles_touch_finger(&t, 2400, 1080, 4, ORACLES_TOUCH_UP, l.pause.x, l.pause.y, &pause);
    /* A pause forgets the fingers; one held through it presses again as it moves, taken where it is, never the pause. */
    oracles_touch_finger(&t, 2400, 1080, 5, ORACLES_TOUCH_DOWN, l.a.x, l.a.y, &pause);
    oracles_touch_release(&t);
    CHECK(oracles_touch_pressed(&t) == 0 && t.shown);
    CHECK(oracles_touch_finger(&t, 2400, 1080, 5, ORACLES_TOUCH_MOTION, l.a.x, l.a.y, &pause) == ORACLES_KEY_A && !pause);
    CHECK(oracles_touch_finger(&t, 2400, 1080, 6, ORACLES_TOUCH_MOTION, l.pause.x, l.pause.y, &pause) == ORACLES_KEY_A && !pause);
    oracles_touch_finger(&t, 2400, 1080, 5, ORACLES_TOUCH_UP, 0, 0, &pause);
    oracles_touch_finger(&t, 2400, 1080, 6, ORACLES_TOUCH_UP, 0, 0, &pause);
    oracles_touch_hide(&t);
    CHECK(!t.shown && oracles_touch_pressed(&t) == 0);

    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_touch_controls: ok\n");
    return 0;
}
