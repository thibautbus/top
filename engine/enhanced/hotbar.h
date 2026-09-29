/* The hotbar of the item hotkeys: four slots of 24 x 16
 * in the two gutters of the HUD band, 1 and 2 on the left, 3 and 4 on the
 * right, never in the 160 columns of the game.  A slot shows the item's icon
 * as the game draws it (or a label of four letters until one is captured),
 * its key at the top right, and under it, on a badge, the button the item
 * sits on when it does.
 *
 * This file is the drawing alone, on a surface of 0xffRRGGBB pixels, from
 * what the caller tells it: no guest here, tested without a ROM. */
#ifndef ORACLES_ENHANCED_HOTBAR_H
#define ORACLES_ENHANCED_HOTBAR_H

#include <stdint.h>

#define ORACLES_HOTBAR_SLOTS 4u
#define ORACLES_HOTBAR_SLOT_WIDTH 24u
#define ORACLES_HOTBAR_ICON 16u                 /* an icon is 16 x 16 */
#define ORACLES_HOTBAR_ICON_OPAQUE 0x8000u      /* an icon's pixel: RGB555, this bit set when it is not transparent */

typedef enum OraclesHotbarFrame { ORACLES_HOTBAR_FRAME_NONE = 0, ORACLES_HOTBAR_FRAME_DOTTED, ORACLES_HOTBAR_FRAME_SOLID, ORACLES_HOTBAR_FRAME_REFUSED } OraclesHotbarFrame;

/* What one slot shows this frame. */
typedef struct OraclesHotbarSlotView {
    const uint16_t *icon;          /* ORACLES_HOTBAR_ICON squared, or NULL: the label instead */
    const char *label;             /* four letters at most; NULL or empty with no icon: nothing */
    const char *key;               /* two characters at most */
    char button;                   /* 'B', 'A', or 0 when the item is on no button */
    int dimmed;                    /* the item is not in the inventory */
    OraclesHotbarFrame frame;
} OraclesHotbarSlotView;

/* The colours of the hotbar, RGB555 turned into pixels by the caller (the game's fade and the player's colour
 * correction are the caller's): the status bar's own background, and the inks. */
typedef struct OraclesHotbarPalette {
    uint32_t background, ink, faint, badge, badge_ink, refused;
    uint32_t (*pixel)(void *opaque, uint16_t rgb555);   /* an icon's colour on screen */
    void *opaque;
} OraclesHotbarPalette;

/* The left edge of slot `n` in a band `width` wide whose game window is `core_width` wide and centred. */
unsigned oracles_hotbar_slot_x(unsigned n, unsigned width, unsigned core_width);
/* Draws the four slots in the top sixteen lines of `surface` (`width` pixels a line). */
void oracles_hotbar_draw(uint32_t *surface, unsigned width, unsigned core_width, const OraclesHotbarSlotView slots[ORACLES_HOTBAR_SLOTS], const OraclesHotbarPalette *palette);

#endif
