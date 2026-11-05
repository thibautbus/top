/* The hotbar of the item hotkeys: four slots in the two gutters of the HUD
 * band, 1 and 2 on the left, 3 and 4 on the right, never in the 160 columns
 * of the game; or, the status bar laid out across the band, side by side
 * against B.  Two looks, the widest the band holds: the game's, each slot
 * drawn as the status bar draws B and A (the key's letter at their size, then
 * the item's icon between the bar's two brackets, spaced as the bar spaces
 * them, 40 wide), and the compact one (24 wide: the icon, its key at the top
 * right, and under it the button the item sits on).  A slot shows the item's
 * icon as the game draws it (or a label of four letters where the game's
 * tables cannot say), and on a badge the button the item sits on when it does.
 *
 * This file is the drawing alone, on a surface of 0xffRRGGBB pixels, from
 * what the caller tells it: no guest here, tested without a ROM. */
#ifndef ORACLES_ENHANCED_HOTBAR_H
#define ORACLES_ENHANCED_HOTBAR_H

#include <stdint.h>

#define ORACLES_HOTBAR_SLOTS 4u
#define ORACLES_HOTBAR_SLOT_WIDTH 24u           /* the compact look */
#define ORACLES_HOTBAR_GAME_SLOT_WIDTH 38u      /* the game's: the bar's B and its count, and blank columns to the next */
#define ORACLES_HOTBAR_GAME_LEAD 4u             /* and four blank before the first, after the block it follows, as the bar leaves between B and A */
#define ORACLES_HOTBAR_GAME_PICTURE_WIDTH 24u   /* the bar's three columns from its left sprite's: the sprites and their count */

typedef enum OraclesHotbarLook { ORACLES_HOTBAR_COMPACT = 0, ORACLES_HOTBAR_GAME } OraclesHotbarLook;
unsigned oracles_hotbar_slot_width(OraclesHotbarLook look);
/* The four slots' width in `look`, the game's lead included. */
unsigned oracles_hotbar_width(OraclesHotbarLook look);
#define ORACLES_HOTBAR_ICON 16u                 /* an icon is 16 x 16 */
#define ORACLES_HOTBAR_ICON_OPAQUE 0x8000u      /* an icon's pixel: RGB555, this bit set when it is not transparent */

typedef enum OraclesHotbarFrame { ORACLES_HOTBAR_FRAME_NONE = 0, ORACLES_HOTBAR_FRAME_DOTTED, ORACLES_HOTBAR_FRAME_SOLID, ORACLES_HOTBAR_FRAME_REFUSED } OraclesHotbarFrame;

/* What one slot shows this frame. */
typedef struct OraclesHotbarSlotView {
    const uint16_t *icon;          /* ORACLES_HOTBAR_ICON squared, or NULL: the label instead */
    const uint16_t *picture;       /* the game's look: ORACLES_HOTBAR_GAME_PICTURE_WIDTH x 16, the item as the bar draws it on
                                    * a button from its left sprite's column (sprites, count, level), or NULL: the icon */
    const char *label;             /* four letters at most; NULL or empty with no icon: nothing */
    const char *key;               /* two characters at most */
    char button;                   /* 'B', 'A', or 0 when the item is on no button */
    int dimmed;                    /* the item is not in the inventory */
    int waiting;                   /* its request waits, this frame of its blink: the game's look marks it under the icon */
    OraclesHotbarFrame frame;
} OraclesHotbarSlotView;

/* The colours of the hotbar, RGB555 turned into pixels by the caller (the game's fade and the player's colour
 * correction are the caller's): the status bar's own background, and the inks. */
typedef struct OraclesHotbarPalette {
    uint32_t background, ink, faint, badge, badge_ink, refused;
    uint32_t (*pixel)(void *opaque, uint16_t rgb555);   /* an icon's colour on screen */
    void *opaque;
} OraclesHotbarPalette;

/* 1 when the gutters beside the status bar hold two slots a side in either look; a narrower surface (the near view in
 * 4:3, 213 wide) draws no hotbar, the hotkeys working all the same. */
int oracles_hotbar_fits(unsigned width, unsigned core_width);
/* The look the gutters hold: the game's where two of its slots fit a side with its lead (the medium and far views, 80 a
 * side and more), else the compact one (the near view in 16:9). */
OraclesHotbarLook oracles_hotbar_gutter_look(unsigned width, unsigned core_width);
/* The left edge of slot `n` in a band `width` wide whose game window is `core_width` wide and centred, in the gutters' look. */
unsigned oracles_hotbar_slot_x(unsigned n, unsigned width, unsigned core_width);
/* Draws the four slots in the gutters, in their look, in the top sixteen lines of `surface` (`width` pixels a line).
 * `glued`: the bar stands alone between dark bands (HUD Original), and the game's lead after its right edge takes the
 * slots' background, so that the slots continue the bar on both sides; otherwise the lead is left as it is. */
void oracles_hotbar_draw(uint32_t *surface, unsigned width, unsigned core_width, int glued, const OraclesHotbarSlotView slots[ORACLES_HOTBAR_SLOTS], const OraclesHotbarPalette *palette);
/* The same, the four slots side by side from column `x`, in `look`, after the game's lead in its look. */
void oracles_hotbar_draw_at(uint32_t *surface, unsigned width, unsigned x, OraclesHotbarLook look, const OraclesHotbarSlotView slots[ORACLES_HOTBAR_SLOTS], const OraclesHotbarPalette *palette);
/* The game's look of a slot whose key is `key`: the column of the opening bracket (one column after the key's letter, as
 * the bar leaves one after its B), the icon's two further right, the closing bracket's 27 from the opening one, as the
 * bar spaces B's and A's around their icons (the item's count between). */
unsigned oracles_hotbar_game_bracket(const char *key);
/* The column of the key's letter's middle in the game's look (the left one of two for an even width), which the badge
 * of the button and the small second character under it are centred on. */
unsigned oracles_hotbar_game_letter_middle(const char *key);

#endif
