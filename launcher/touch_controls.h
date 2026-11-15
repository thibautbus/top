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
    /* felt: the buttons the finger feels pressed, which the vibration ticks for: the game's, held a little past their
     * borders. */
    struct { int used; uint64_t id; OraclesTouchOn on; unsigned buttons, felt; } fingers[ORACLES_TOUCH_FINGERS];
    /* The last finger given to oracles_touch_finger pressed something new: it went down on the pause, onto a button
     * other than the one it felt, or onto a direction of the d-pad added to those it felt (up to up and right, not on
     * to right); never as it goes up, slides off, holds still or rests on a border.  What the vibration ticks for
     * (touch_vibration=). */
    int tick;
} OraclesTouch;

void oracles_touch_layout(float width, float height, OraclesTouchLayout *out);
/* The directions (ORACLES_KEY_* bits) of a point seen from the d-pad's centre: eight ways, none near the centre. */
unsigned oracles_touch_dpad(const OraclesTouchCircle *dpad, float x, float y);
/* The button (one ORACLES_KEY_* bit) at a point, a little beyond its drawing; 0 over none. */
unsigned oracles_touch_button_at(const OraclesTouchLayout *layout, float x, float y);
int oracles_touch_on_pause(const OraclesTouchLayout *layout, float x, float y);

/* A finger went down, moved or went up at (x, y) on a screen of width x height: the controls show, and are laid out
 * again when the screen's size changed; a finger unknown as it moves (held through a pause) is taken as going down
 * there.  Returns the game's buttons all the fingers press now; *pause is set when this finger went down on the pause, and
 * touch->tick when it pressed something new. */
unsigned oracles_touch_finger(OraclesTouch *touch, float width, float height, uint64_t id, OraclesTouchFingerKind kind,
                              float x, float y, int *pause);
/* Where the game's frame of frame_w x frame_h goes on a screen of width x height, as SDL's logical presentation puts
 * it: integer-scaled, the largest whole scale, centred; with `fill` (letterboxed), the largest size in the frame's
 * proportions, centred. */
OraclesTouchBox oracles_touch_frame(int width, int height, int frame_w, int frame_h, int fill);
/* settings.txt's screen_margin=: what the game's frame, the controls and the launcher's pages keep clear of at the
 * screen's top and bottom.  Off, the whole screen; auto, the display cutout's insets Android reports there; small and
 * large, a margin of 24 or 48 dp at the top and at the bottom, or the cutout's inset where it is larger (a screen whose
 * edges hide part of the picture without Android saying so). */
typedef enum OraclesScreenMargin {
    ORACLES_SCREEN_MARGIN_OFF,
    ORACLES_SCREEN_MARGIN_AUTO,
    ORACLES_SCREEN_MARGIN_SMALL,
    ORACLES_SCREEN_MARGIN_LARGE
} OraclesScreenMargin;
#define ORACLES_SCREEN_MARGINS 4
/* Their names in settings.txt and on the command line: "off", "auto", "small", "large". */
extern const char *const oracles_screen_margin_names[ORACLES_SCREEN_MARGINS];
/* What the band keeps clear of: the margin, an OraclesScreenMargin; the display's pixels per dp; and the display
 * cutout's insets at the screen's top and bottom, in pixels (a cutout at a side leaves the frame, centred, clear of
 * it). */
typedef struct OraclesTouchEdges {
    int margin;
    float density;
    int inset_top, inset_bottom;
} OraclesTouchEdges;
/* The part of a screen of width x height that the game's frame, the controls and the launcher's pages keep to, at the
 * screen's whole width: below the top margin and above the bottom one, each the larger of the margin's dp and the
 * cutout's inset (the inset alone with auto).  The whole screen with off whatever the insets, and where the margins
 * would leave less than half its height. */
OraclesTouchBox oracles_touch_band(int width, int height, const OraclesTouchEdges *edges);
/* The game's buttons the fingers press. */
unsigned oracles_touch_pressed(const OraclesTouch *touch);
/* The fingers forgotten (a pause, whose menu takes them); hide: a key or a controller was used, the controls go too. */
void oracles_touch_release(OraclesTouch *touch);
void oracles_touch_hide(OraclesTouch *touch);

#endif
