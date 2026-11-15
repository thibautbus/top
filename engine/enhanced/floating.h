/* The status bar floating over the world (hud=floating): where its blocks stand, each in a panel of its own over the
 * world band, which takes the whole surface.  The hearts at the top-left corner, as many cells as Link has hearts; the
 * money at the top-right; B and A at the bottom-left, or, with the item hotkeys, the six buttons as a Nintendo pad has
 * them (the shoulders LB and RB with X between them on top, Y and A in the middle, B below), each a tile of its own, its
 * key's badge beside its item; the keys taken from the keyboard, the same tiles as a block of three rows of two (LB and
 * RB, X and Y, B and A).  In the near view B and A alone.  A bar whose map signs none of the game's layouts stands whole in one panel at the top's
 * middle.  With the touch controls shown, B and A alone under the hearts, the money left of the controls' top-right
 * rectangle (the pause), the other rectangles avoided.  This file is the arithmetic alone, tested without a ROM. */
#ifndef ORACLES_ENHANCED_FLOATING_H
#define ORACLES_ENHANCED_FLOATING_H

#include <math.h>
#include <stdint.h>

#define ORACLES_FLOATING_MARGIN 6u          /* from the surface's edges; 4 in the near view */
#define ORACLES_FLOATING_MARGIN_NEAR 4u
#define ORACLES_FLOATING_PAD 2u             /* between a panel's edge and the bar's pixels in it */
#define ORACLES_FLOATING_RADIUS 2u          /* a panel's corners, in whole pixels: two left out of its first line, one of the next */
/* An item's tile in the pad's layout: its key's badge in a gutter at its left (from BADGE_X, BADGE_Y), the bar's
 * picture of 24 x 16 right of it (from PICTURE_X), the pad around them. */
#define ORACLES_FLOATING_BADGE_X 1u
#define ORACLES_FLOATING_BADGE_Y 2u
#define ORACLES_FLOATING_PICTURE_X 7u
#define ORACLES_FLOATING_PICTURE_W 24u
#define ORACLES_FLOATING_TILE_W (ORACLES_FLOATING_PICTURE_X + ORACLES_FLOATING_PICTURE_W + ORACLES_FLOATING_PAD)
#define ORACLES_FLOATING_TILE_H 20u
#define ORACLES_FLOATING_TILE_GAP 4u
#define ORACLES_FLOATING_ALPHA 205u         /* the panels' opacity, of 256: 80 % */
#define ORACLES_FLOATING_BADGE_ALPHA 230u   /* a key's badge's ground: 90 % */
#define ORACLES_FLOATING_PANELS 8u
#define ORACLES_FLOATING_AVOID 4u
/* The corner or edge a panel keeps to (OraclesFloatingPanel's anchor): the left and the top, or */
#define ORACLES_FLOATING_RIGHT 1u
#define ORACLES_FLOATING_BOTTOM 2u
#define ORACLES_FLOATING_CENTRE 4u          /* the middle across */

typedef enum OraclesFloatingKind {
    ORACLES_FLOATING_HEARTS = 0,
    ORACLES_FLOATING_MONEY,
    ORACLES_FLOATING_BUTTONS,          /* B and A as the bar draws them */
    ORACLES_FLOATING_BAR,              /* the whole bar, its map signing no layout */
    ORACLES_FLOATING_TILE              /* an item's tile in the pad's layout: `slot` 0 to 3 the hotkeys' slots, 4 B, 5 A */
} OraclesFloatingKind;
#define ORACLES_FLOATING_SLOT_B 4u
#define ORACLES_FLOATING_SLOT_A 5u

typedef struct OraclesFloatingPanel {
    OraclesFloatingKind kind;
    unsigned slot;
    int32_t x, y, w, h;                /* the panel on the surface */
    int top;                           /* at the surface's top: a text box it meets hides it; the others hide under any text */
    unsigned anchor;                   /* ORACLES_FLOATING_RIGHT, _BOTTOM, _CENTRE: the corner it keeps to on a screen of another size */
} OraclesFloatingPanel;

typedef struct OraclesFloatingInput {
    unsigned width, height;            /* the surface */
    int near_view;                     /* the near view: the narrower margin, B and A alone */
    int known;                         /* the bar's map signs one of the game's layouts */
    unsigned buttons;                  /* B and A's width in the bar, 80 or 72 tightened */
    unsigned hearts, per_row;          /* Link's hearts (0: the bar's two rows whole) and the cells a row, 7 or 8 tightened */
    int pad;                           /* the item hotkeys shown: the pad's six tiles */
    int keyboard;                      /* in the keyboard's block (oracles_floating_keys_cell) */
    unsigned avoid_count;              /* the touch controls' rectangles, x, y, w, h: B and A under the hearts, the money left of */
    int32_t avoid[ORACLES_FLOATING_AVOID][4];   /* the one at the top-right (the pause) */
} OraclesFloatingInput;

typedef struct OraclesFloatingLayout {
    unsigned count;
    OraclesFloatingPanel panels[ORACLES_FLOATING_PANELS];
    unsigned radius, pad;              /* the corners' radius, the pixels between a panel's edge and its content */
    unsigned hearts_cols, hearts_rows; /* the hearts' cells shown */
} OraclesFloatingLayout;

void oracles_floating_layout(const OraclesFloatingInput *in, OraclesFloatingLayout *out);
/* The pad's place of an item's tile, column and row of three by three: LB, X, RB on top, Y and A in the middle, B below;
 * the hotkeys' slots where a controller's buttons take them at first, X, Y, LB and RB. */
void oracles_floating_pad_cell(unsigned slot, unsigned *column, unsigned *row);
/* The keyboard's place of an item's tile, column and row of two by three: LB and RB on top, X and Y in the middle, B and
 * A below. */
void oracles_floating_keys_cell(unsigned slot, unsigned *column, unsigned *row);
/* The corner a panel (x, y, w, h) of a layout `width` x `height` keeps to on a screen of another size: the side of the
 * middle its own middle stands on, across and down; the middle across when it stands on it. */
static inline unsigned oracles_floating_corner(const int32_t rect[4], uint32_t width, uint32_t height)
{
    const int32_t across = 2 * rect[0] + rect[2] - (int32_t)width, down = 2 * rect[1] + rect[3] - (int32_t)height;
    return (across > 0 ? ORACLES_FLOATING_RIGHT : 0u) | (across == 0 ? ORACLES_FLOATING_CENTRE : 0u) | (down > 0 ? ORACLES_FLOATING_BOTTOM : 0u);
}
/* The near view's panels at half the game's scale, laid out on a surface twice the view's (`layer_w` x `layer_h`), on a
 * picture of the view `picture_w` x `picture_h` screen pixels: the screen pixels a pixel of theirs takes across, the
 * picture's scale halved, to the nearest from twice on, a half up (a whole number: each pixel the same square; 4 at
 * seven times, 2 at three times), unrounded under it. */
static inline float oracles_floating_screen_unit(float picture_w, uint32_t layer_w)
{
    const float scale = layer_w ? picture_w * 2.0f / (float)layer_w : 0.0f;
    return scale >= 2.0f ? floorf(scale / 2.0f + 0.5f) : scale / 2.0f;
}
/* A panel (x, y, w, h of that layout, its corner `anchor`) on that picture: its place from the picture's corner, x, y,
 * w, h in screen pixels, as far from the picture's edges at that unit as from the layout's; whole pixels from twice on. */
static inline void oracles_floating_screen_place(const int32_t rect[4], unsigned anchor, uint32_t layer_w, uint32_t layer_h, float picture_w, float picture_h,
                                                 float out[4])
{
    const float unit = oracles_floating_screen_unit(picture_w, layer_w);
    const int whole = unit >= 1.0f;
    float x = (float)rect[0] * unit, y = (float)rect[1] * unit;
    if (anchor & ORACLES_FLOATING_RIGHT) x = picture_w - (float)((int32_t)layer_w - rect[0]) * unit;
    else if (anchor & ORACLES_FLOATING_CENTRE) x = picture_w / 2.0f - (float)((int32_t)layer_w / 2 - rect[0]) * unit;
    if (anchor & ORACLES_FLOATING_BOTTOM) y = picture_h - (float)((int32_t)layer_h - rect[1]) * unit;
    out[0] = whole ? floorf(x) : x;
    out[1] = whole ? floorf(y) : y;
    out[2] = (float)rect[2] * unit;
    out[3] = (float)rect[3] * unit;
}
/* The same panel on the view's own surface (`layer_w` / 2 x `layer_h` / 2) shown at `scale`: x, y, w, h in its pixels. */
static inline void oracles_floating_surface_place(const int32_t rect[4], unsigned anchor, uint32_t layer_w, uint32_t layer_h, float scale, float out[4])
{
    oracles_floating_screen_place(rect, anchor, layer_w, layer_h, (float)(layer_w / 2u) * scale, (float)(layer_h / 2u) * scale, out);
    for (unsigned i = 0; i < 4u; i++) out[i] /= scale;
}
/* A pixel (x, y) of a panel w x h with corners of `radius`: 0 outside its rounded corners, 1 on its edge, 2 inside. */
unsigned oracles_floating_panel_pixel(unsigned w, unsigned h, unsigned radius, unsigned x, unsigned y);

#endif
