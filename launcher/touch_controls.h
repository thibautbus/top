/* The game's buttons on a touch screen (Android): a d-pad at the left, A and B at the right, Select and Start at the
 * bottom's centre, a pause at the top right, drawn over the game (touch_sdl.c) from the first touch until a key or a
 * controller is used.  This half knows no SDL: where the controls lie on a screen of a given size, and which of the
 * game's buttons the fingers on it press. */
#ifndef ORACLES_LAUNCHER_TOUCH_CONTROLS_H
#define ORACLES_LAUNCHER_TOUCH_CONTROLS_H

#include <stdint.h>

#define ORACLES_TOUCH_FINGERS 10

typedef struct OraclesTouchCircle { float x, y, r; } OraclesTouchCircle;
typedef struct OraclesTouchBox { float x, y, w, h; } OraclesTouchBox;

/* In the screen's pixels; the sizes follow its short side, so that a control is as large on a phone as on a tablet. */
typedef struct OraclesTouchLayout {
    float width, height;
    OraclesTouchCircle dpad, a, b, pause;
    OraclesTouchBox select, start;
} OraclesTouchLayout;

typedef enum OraclesTouchOn {
    ORACLES_TOUCH_ON_NOTHING,
    ORACLES_TOUCH_ON_DPAD,      /* held by the d-pad wherever it slides, its direction taken from the d-pad's centre */
    ORACLES_TOUCH_ON_BUTTONS,   /* A, B, Select or Start, whichever it is over as it slides */
    ORACLES_TOUCH_ON_PAUSE
} OraclesTouchOn;

typedef enum OraclesTouchFingerKind { ORACLES_TOUCH_DOWN, ORACLES_TOUCH_MOTION, ORACLES_TOUCH_UP } OraclesTouchFingerKind;

typedef struct OraclesTouch {
    int shown;
    OraclesTouchLayout layout;
    struct { int used; uint64_t id; OraclesTouchOn on; unsigned buttons; } fingers[ORACLES_TOUCH_FINGERS];
} OraclesTouch;

void oracles_touch_layout(float width, float height, OraclesTouchLayout *out);
/* The directions (ORACLES_KEY_* bits) of a point seen from the d-pad's centre: eight ways, none near the centre. */
unsigned oracles_touch_dpad(const OraclesTouchCircle *dpad, float x, float y);
/* The button (one ORACLES_KEY_* bit) at a point, a little beyond its drawing; 0 over none. */
unsigned oracles_touch_button_at(const OraclesTouchLayout *layout, float x, float y);
int oracles_touch_on_pause(const OraclesTouchLayout *layout, float x, float y);

/* A finger went down, moved or went up at (x, y) on a screen of width x height: the controls show, and are laid out
 * again when the screen's size changed; a finger unknown as it moves (held through a pause) is taken as going down
 * there.  Returns the game's buttons all the fingers press now; *pause is set when this finger went down on the pause. */
unsigned oracles_touch_finger(OraclesTouch *touch, float width, float height, uint64_t id, OraclesTouchFingerKind kind,
                              float x, float y, int *pause);
/* Where the game's frame of frame_w x frame_h goes on a screen of width x height, as SDL's logical presentation puts
 * it: integer-scaled, the largest whole scale, centred; with `fill` (letterboxed), the largest size in the frame's
 * proportions, centred. */
OraclesTouchBox oracles_touch_frame(int width, int height, int frame_w, int frame_h, int fill);
/* The game's buttons the fingers press. */
unsigned oracles_touch_pressed(const OraclesTouch *touch);
/* The fingers forgotten (a pause, whose menu takes them); hide: a key or a controller was used, the controls go too. */
void oracles_touch_release(OraclesTouch *touch);
void oracles_touch_hide(OraclesTouch *touch);

#endif
