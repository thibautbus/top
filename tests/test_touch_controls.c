/* The touch controls' half without SDL (launcher/touch_controls.c): the controls lie inside a phone's, a handheld's, a
 * tablet's and a split or upright screen without overlapping, Select and Start answering wherever they are drawn; the
 * d-pad gives eight directions and none at its centre; A, B, Select, Start and the pause are found where they are drawn
 * and a little beyond; fingers press together, a finger held by the d-pad keeps its direction when it slides off it,
 * one sliding from A to B moves the press, a finger going up releases, one held through a pause presses again as it
 * moves; the frame goes where SDL's integer-scaled logical presentation puts it; the screen margin's band is the whole
 * screen off, whatever the insets, keeps clear of the insets at the screen's top and bottom with auto, and of 24 or 48
 * dp with small and large, an inset there where it is larger; the frame and the controls lie in it; the vibration ticks as a
 * finger presses something new, never as it holds, slides off, rests on a border or goes up, the game given the same. */
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
    OraclesTouchBox f = oracles_touch_frame(2400, 1080, 160, 144, 0);
    CHECK(f.x == 640 && f.y == 36 && f.w == 1120 && f.h == 1008);
    f = oracles_touch_frame(2400, 1080, 480, 270, 0);
    CHECK(f.x == 240 && f.y == 0 && f.w == 1920 && f.h == 1080);
    f = oracles_touch_frame(100, 80, 160, 144, 0);   /* smaller than the frame: scale 1 */
    CHECK(f.w == 160 && f.h == 144);
    /* Scaling's Fill, where SDL's letterboxed presentation puts it: the whole height of a wide screen, the whole width of
     * an upright one, the whole screen in its own shape. */
    f = oracles_touch_frame(2400, 1080, 160, 144, 1);
    CHECK(f.x == 600 && f.y == 0 && f.w == 1200 && f.h == 1080);
    f = oracles_touch_frame(1080, 2400, 480, 270, 1);
    CHECK(f.x == 0 && f.y == 896.5f && f.w == 1080 && f.h == 607);
    f = oracles_touch_frame(640, 480, 480, 360, 1);
    CHECK(f.x == 0 && f.y == 0 && f.w == 640 && f.h == 480);

    /* The band: off, the whole screen whatever the insets, as before the margin; auto, below a top inset and above a
     * bottom one, the whole screen without them; the whole screen where they are negative or would take half its
     * height. */
    const OraclesTouchEdges off = { ORACLES_SCREEN_MARGIN_OFF, 2.625f, 53, 53 };
    OraclesTouchBox s = oracles_touch_band(2400, 1080, &off);
    CHECK(s.x == 0 && s.y == 0 && s.w == 2400 && s.h == 1080);
    OraclesTouchEdges e = { ORACLES_SCREEN_MARGIN_AUTO, 2.625f, 0, 0 };
    s = oracles_touch_band(2400, 1080, &e);
    CHECK(s.x == 0 && s.y == 0 && s.w == 2400 && s.h == 1080);
    e.inset_top = 80;
    s = oracles_touch_band(1920, 1080, &e);
    CHECK(s.x == 0 && s.y == 80 && s.w == 1920 && s.h == 1000);
    e.inset_top = 300; e.inset_bottom = 260;
    CHECK(oracles_touch_band(1920, 1080, &e).h == 1080);
    e.inset_top = -4; e.inset_bottom = 0;
    CHECK(oracles_touch_band(1920, 1080, &e).y == 0);
    /* A curved edge above and below (53 lines each): Medium's 384x216 drops from 5 to 4 times, whole in the band, and
     * Fill takes the band's height; the controls lie in the band. */
    e.inset_top = e.inset_bottom = 53;
    s = oracles_touch_band(2400, 1080, &e);
    CHECK(s.y == 53 && s.h == 974);
    f = oracles_touch_frame((int)s.w, (int)s.h, 384, 216, 0);
    CHECK(f.w == 1536 && f.h == 864 && s.y + f.y >= 53 && s.y + f.y + f.h <= 1080 - 53);
    f = oracles_touch_frame((int)s.w, (int)s.h, 384, 216, 1);
    CHECK(f.y == 0 && f.h == 974);
    screens(s.w, s.h);
    /* Small and large: 24 and 48 dp at the top and at the bottom in the display's pixels, rounded; a dp a pixel on a
     * desktop at 100 %, 2.625 pixels on a phone of 420 dpi, 2.25 at 360 dpi. */
    e = (OraclesTouchEdges){ ORACLES_SCREEN_MARGIN_SMALL, 1.0f, 0, 0 };
    s = oracles_touch_band(1920, 1080, &e);
    CHECK(s.x == 0 && s.y == 24 && s.w == 1920 && s.h == 1032);
    e.margin = ORACLES_SCREEN_MARGIN_LARGE;
    CHECK(oracles_touch_band(1920, 1080, &e).y == 48 && oracles_touch_band(1920, 1080, &e).h == 984);
    e = (OraclesTouchEdges){ ORACLES_SCREEN_MARGIN_SMALL, 2.625f, 0, 0 };
    s = oracles_touch_band(1920, 1080, &e);
    CHECK(s.y == 63 && s.h == 954);
    e.density = 2.25f;
    CHECK(oracles_touch_band(1920, 1080, &e).y == 54);
    e = (OraclesTouchEdges){ ORACLES_SCREEN_MARGIN_LARGE, 2.625f, 0, 0 };
    s = oracles_touch_band(1920, 1080, &e);
    CHECK(s.y == 126 && s.h == 828);
    e.density = 0.0f;   /* unknown: a dp a pixel */
    CHECK(oracles_touch_band(1920, 1080, &e).y == 48);
    /* The larger of the fixed margin and the inset, side by side: a 100-line inset at the top wins over small's 63, small
     * wins at the bottom over a 10-line one; with large (126) the margin wins at both. */
    e = (OraclesTouchEdges){ ORACLES_SCREEN_MARGIN_SMALL, 2.625f, 100, 10 };
    s = oracles_touch_band(1920, 1080, &e);
    CHECK(s.y == 100 && s.h == 1080 - 100 - 63);
    e.margin = ORACLES_SCREEN_MARGIN_LARGE;
    s = oracles_touch_band(1920, 1080, &e);
    CHECK(s.y == 126 && s.h == 1080 - 2 * 126);
    /* Large where it would take half the screen (a small window at 4 pixels a dp): the whole screen. */
    e = (OraclesTouchEdges){ ORACLES_SCREEN_MARGIN_LARGE, 4.0f, 0, 0 };
    CHECK(oracles_touch_band(1280, 720, &e).h == 720);
    /* The game in small's band on a 1920x1080 screen at 2.625 pixels a dp: Medium's 384x216 from 5 to 4 times, Far's
     * 480x270 from 4 to 3, Faithful's 160x144 from 7 to 6, each whole in the band; Fill the band's height; the controls
     * in the band. */
    e = (OraclesTouchEdges){ ORACLES_SCREEN_MARGIN_SMALL, 2.625f, 0, 0 };
    s = oracles_touch_band(1920, 1080, &e);
    static const int sizes[3][3] = { { 384, 216, 4 }, { 480, 270, 3 }, { 160, 144, 6 } };
    for (int i = 0; i < 3; i++) {
        CHECK(oracles_touch_frame(1920, 1080, sizes[i][0], sizes[i][1], 0).w == (float)(sizes[i][0] * (sizes[i][2] + 1)));
        f = oracles_touch_frame((int)s.w, (int)s.h, sizes[i][0], sizes[i][1], 0);
        CHECK(f.w == (float)(sizes[i][0] * sizes[i][2]) && s.y + f.y >= 63 && s.y + f.y + f.h <= 1080 - 63);
    }
    f = oracles_touch_frame((int)s.w, (int)s.h, 480, 270, 1);
    CHECK(f.y == 0 && f.h == 954);
    screens(s.w, s.h);

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

    /* The vibration's ticks: a finger going down on a button or the pause; onto a direction of the d-pad added to those
     * it held, or onto the other button; never as it holds still, slides off a button or from a diagonal to one of its
     * directions, goes up, or touches the game. */
    t = (OraclesTouch){ 0 };
    oracles_touch_finger(&t, 2400, 1080, 1, ORACLES_TOUCH_DOWN, l.a.x, l.a.y, &pause);
    CHECK(t.tick);
    oracles_touch_finger(&t, 2400, 1080, 1, ORACLES_TOUCH_MOTION, l.a.x + 1, l.a.y, &pause);
    CHECK(!t.tick);
    oracles_touch_finger(&t, 2400, 1080, 1, ORACLES_TOUCH_MOTION, l.b.x, l.b.y, &pause);
    CHECK(t.tick);
    oracles_touch_finger(&t, 2400, 1080, 1, ORACLES_TOUCH_MOTION, 1200, 300, &pause);
    CHECK(!t.tick && oracles_touch_pressed(&t) == 0);
    oracles_touch_finger(&t, 2400, 1080, 1, ORACLES_TOUCH_MOTION, l.b.x, l.b.y, &pause);
    CHECK(t.tick);   /* back onto B */
    oracles_touch_finger(&t, 2400, 1080, 1, ORACLES_TOUCH_UP, l.b.x, l.b.y, &pause);
    CHECK(!t.tick);
    oracles_touch_finger(&t, 2400, 1080, 2, ORACLES_TOUCH_DOWN, d->x + 0.1f * d->r, d->y, &pause);
    CHECK(!t.tick);   /* the d-pad's dead centre */
    oracles_touch_finger(&t, 2400, 1080, 2, ORACLES_TOUCH_MOTION, d->x, d->y - 0.7f * d->r, &pause);
    CHECK(t.tick);
    oracles_touch_finger(&t, 2400, 1080, 2, ORACLES_TOUCH_MOTION, d->x + 0.05f * d->r, d->y - 0.8f * d->r, &pause);
    CHECK(!t.tick && oracles_touch_pressed(&t) == ORACLES_KEY_UP);   /* up still */
    oracles_touch_finger(&t, 2400, 1080, 2, ORACLES_TOUCH_MOTION, d->x + 0.5f * d->r, d->y - 0.5f * d->r, &pause);
    CHECK(t.tick);   /* up and right */
    oracles_touch_finger(&t, 2400, 1080, 2, ORACLES_TOUCH_MOTION, d->x + 0.7f * d->r, d->y, &pause);
    CHECK(!t.tick && oracles_touch_pressed(&t) == ORACLES_KEY_RIGHT);   /* right, held as part of the diagonal */
    oracles_touch_finger(&t, 2400, 1080, 2, ORACLES_TOUCH_MOTION, d->x + 0.5f * d->r, d->y + 0.5f * d->r, &pause);
    CHECK(t.tick);   /* down and right: down added */
    oracles_touch_finger(&t, 2400, 1080, 3, ORACLES_TOUCH_DOWN, l.start.x + 1, l.start.y + 1, &pause);
    CHECK(t.tick);   /* a second finger, the d-pad's still held */
    oracles_touch_finger(&t, 2400, 1080, 2, ORACLES_TOUCH_MOTION, d->x + 0.7f * d->r, d->y + 1, &pause);
    CHECK(!t.tick);
    oracles_touch_finger(&t, 2400, 1080, 2, ORACLES_TOUCH_MOTION, d->x + 0.1f * d->r, d->y, &pause);
    CHECK(!t.tick && oracles_touch_pressed(&t) == ORACLES_KEY_START);   /* back to the centre: released */
    oracles_touch_finger(&t, 2400, 1080, 4, ORACLES_TOUCH_DOWN, 1200, 300, &pause);
    CHECK(!t.tick);
    oracles_touch_finger(&t, 2400, 1080, 5, ORACLES_TOUCH_DOWN, l.pause.x, l.pause.y, &pause);
    CHECK(t.tick && pause);
    /* After a pause, a finger held through it ticks once as it presses again. */
    oracles_touch_release(&t);
    oracles_touch_finger(&t, 2400, 1080, 3, ORACLES_TOUCH_MOTION, l.start.x + 1, l.start.y + 1, &pause);
    CHECK(t.tick);
    oracles_touch_finger(&t, 2400, 1080, 3, ORACLES_TOUCH_MOTION, l.start.x + 2, l.start.y + 1, &pause);
    CHECK(!t.tick);
    oracles_touch_hide(&t);

    /* A thumb resting on a border, the touch screen's noise moving it a pixel or two at each event: the game's buttons
     * flicker as they did, given as oracles_touch_dpad and oracles_touch_button_at give them, and the vibration ticks
     * once at most. */
    static const float up_right = 0.38268343f, up = 0.92387953f;   /* 22.5 degrees from up: up meets up and right */
    /* Where the finger goes down, then the border it rests on. */
    const float borders[4][4] = { { d->x, d->y - 0.6f * d->r, d->x + 0.7f * d->r * up_right, d->y - 0.7f * d->r * up },
                                  { d->x, d->y, d->x, d->y - 0.2f * d->r },   /* the dead centre's edge */
                                  { l.a.x, l.a.y, l.a.x, l.a.y - 1.3f * l.a.r },   /* A's reach */
                                  { l.select.x + 0.5f * l.select.w, l.select.y + 0.5f * l.select.h,
                                    l.select.x + 0.5f * l.select.w, l.select.y - 0.3f * l.select.h } };   /* Select's */
    for (int b = 0; b < 4; b++) {
        const float bx = borders[b][2], by = borders[b][3];
        unsigned noise = 12345u, first = 0;
        int ticks = 0, same = 1, flickers = 0;
        t = (OraclesTouch){ 0 };
        oracles_touch_finger(&t, 2400, 1080, 1, ORACLES_TOUCH_DOWN, borders[b][0], borders[b][1], &pause);
        for (int i = 0; i < 240; i++) {
            noise = noise * 1103515245u + 12345u;
            const float x = bx + (float)((noise >> 8) % 301u) / 100.0f - 1.5f;
            noise = noise * 1103515245u + 12345u;
            const float y = by + (float)((noise >> 8) % 301u) / 100.0f - 1.5f;
            const unsigned game = oracles_touch_finger(&t, 2400, 1080, 1, ORACLES_TOUCH_MOTION, x, y, &pause);
            same &= game == (b < 2 ? oracles_touch_dpad(d, x, y) : oracles_touch_button_at(&l, x, y));
            if (i == 0) first = game;
            flickers += game != first;
            ticks += t.tick;
        }
        CHECK(same && ticks <= 1);
        CHECK(flickers > 0);   /* the game's buttons did flicker there */
    }

    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_touch_controls: ok\n");
    return 0;
}
