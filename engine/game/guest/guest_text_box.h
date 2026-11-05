/* The guest's hooks for the host's text box (guest.h, where the compact text's are). */
#ifndef ORACLES_GUEST_TEXT_BOX_H
#define ORACLES_GUEST_TEXT_BOX_H

#include "guest.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- the host's text box -------------------------------------------------------------------
 *
 * The lines of the text box as the host keeps them, so that it can draw the box itself, from what it knows, at another
 * width than the game's.  The game composes a line whole in w7TextGfxBuffer (drawLineOfText), and dmaTextGfxBuffer
 * queues its copy to the top line's tiles ($9400, a = 0) or the bottom line's ($9600, a = 2), which vblankDmaFunction
 * runs at a vblank the main loop waits for, or queueDmaTransfer at once with the LCD off.  The host keeps three lines, as
 * the VRAM does:
 *   - the line composed, at drawLineOfText's return: with the compact text, the transaction's own composition in the
 *     font's widths; without it, the characters the game kept in the line's buffers and drew in its cells, laid out
 *     again at a fixed advance, a whole cell each, which is the game's own layout;
 *   - the top and bottom lines: the line composed copied into one where the game's DMA copies w7TextGfxBuffer, at the
 *     entry of vblankDmaFunction for a queued entry from w7TextGfxBuffer to $9400 or $9600 in bank 0, and at
 *     dmaTextGfxBuffer's with the LCD off;
 *   - in the game's box (128 pixels), the compact text's reveal writes at the vblank (oracles_guest_compact_vblank) made
 *     in the host's line as in the VRAM; in a wider box, the host's lines stay whole, and the reveal is read from the
 *     text's state (oracles_guest_text_box_reveal_state), which the game's map cannot say past its sixteen columns.
 * At each vblank, before its writes, the lines are kept as the frame just scanned showed them, for the view that
 * composes that frame after the vblank, and the text's reveal as the same frame showed it (read at the vblank before).
 * Reads only: nothing is written into the game.  The lines and the reveal outlive a frame: a savestate carries them
 * (oracles_guest_text_box_save_state), and a load without them leaves them unknown until the game copies a line
 * again. */
#define ORACLES_GUEST_TEXT_BOX_COLUMNS_MAX ORACLES_TEXT_LINE_TILES   /* a line's tile columns at most: 256 pixels */
#define ORACLES_GUEST_TEXT_BOX_COMPOSED 0u   /* the line drawLineOfText composed last */
#define ORACLES_GUEST_TEXT_BOX_TOP 1u        /* the top line's tiles, $9400 */
#define ORACLES_GUEST_TEXT_BOX_BOTTOM 2u     /* the bottom line's, $9600 */
#define ORACLES_GUEST_TEXT_BOX_LINES 3u
typedef struct OraclesGuestTextBoxCharacter {
    uint8_t character;      /* as setLineTextBuffers received it ($06 a symbol) */
    uint8_t whole;          /* a symbol or a cursor's place: a whole tile */
    uint8_t attribute;      /* the attribute it was drawn in */
    uint8_t delay;          /* the frames the game waits after it */
    uint8_t from, drawn;    /* its cell's first column drawn and how many (oracles_text_place) */
    uint16_t start;         /* the line's pixel its first column drawn goes to */
} OraclesGuestTextBoxCharacter;
typedef struct OraclesGuestTextBoxLine {
    uint8_t known;                                            /* filled since the arming, a reset or a load */
    uint8_t columns;                                          /* its tile columns: the box's text width in tiles */
    uint8_t attribute[ORACLES_GUEST_TEXT_BOX_COLUMNS_MAX];    /* each tile column's attribute */
    uint8_t gfx[ORACLES_GUEST_TEXT_BOX_COLUMNS_MAX * 32u];    /* tile column t: bytes 32 t to 32 t + 31, its two tiles' 16 rows, as w7TextGfxBuffer */
    /* Its layout, as composed: its characters and its reveal's steps, step s from character first[s] to first[s + 1] - 1,
     * its characters ending before pixel boundary[s] (where the next step starts, the line's drawn tiles' end after the
     * last), on 16 bits where the step's byte in w7LineTextBuffer holds 127 at most. */
    uint8_t count, steps;
    uint8_t offset;                                           /* the pixels it starts at: a page the game laid out, centred in a wide box */
    uint8_t first[ORACLES_GUEST_LINE_COLUMNS + 1u];
    uint16_t boundary[ORACLES_GUEST_LINE_COLUMNS];
    OraclesGuestTextBoxCharacter character[ORACLES_GUEST_LINE_CHARACTERS];
} OraclesGuestTextBoxLine;
/* Arms the host's text box at `pixels` of text a line (a multiple of 8 from 128, the game's, to 256): the points of
 * drawLineOfText, dmaTextGfxBuffer and vblankDmaFunction, read-only.  -1 when the box is wider than the game's and the
 * profile does not qualify the host's box (Ages', Seasons' and the fan games' do), a routine is missing, the width is
 * out of range or the hook table is full; at the game's width, which the harness's check alone arms, the routines
 * suffice. */
int oracles_guest_arm_text_box(OraclesGuest *guest, unsigned pixels);
int oracles_guest_text_box_armed(const OraclesGuest *guest);
/* The host's width of text, 0 unarmed. */
unsigned oracles_guest_text_box_pixels(const OraclesGuest *guest);
/* The wide text box (the host draws the box, the game's VRAM holding only a line's first 128 pixels): the host's text
 * box armed at `pixels`, more than the game's 128, and the compact text's transaction armed to compose each line at that
 * width, in the font's own widths, or at the game's fixed advance with `fixed` (the classic text,
 * oracles_text_font_fixed): w7TextGfxBuffer receives the line's first 128 pixels and the steps' bytes their boundaries
 * within them, the host's line the whole line, its steps' boundaries on 16 bits and its characters.  The game's
 * pagination, pace, sounds, choices and commands are its own.  -1 when either cannot arm, or the transaction is armed
 * already in another font. */
int oracles_guest_arm_wide_text_box(OraclesGuest *guest, unsigned pixels, int fixed);
/* The reveal of the line the game is revealing, as the frame just scanned showed it (read at the vblank before, with
 * the lines): `line` ORACLES_GUEST_TEXT_BOX_TOP or _BOTTOM, 0 when no line is being revealed (every line shown is then
 * shown whole); the steps the game has shown (w7NextTextColumnToDisplay) and the frames since its last one
 * (w7LineDelaysBuffer's delay of that step less w7CharacterDisplayTimer). */
typedef struct OraclesGuestTextBoxReveal {
    uint8_t line;
    uint8_t shown;
    uint8_t elapsed;
} OraclesGuestTextBoxReveal;
void oracles_guest_text_box_reveal_state(const OraclesGuest *guest, OraclesGuestTextBoxReveal *reveal);
/* The pages laid out again at the wide box's width (oracles_language_session_laid_pages: `count` pairs of physical
 * offsets of the image, a page's first byte and past its last, increasing), copied.  At drawLineOfText's entry the line
 * starts at w7TextAddress in bank w7ActiveBank: a line of none of them is of a page laid out as the game lays it out
 * for its 128 pixels (a choice, a centred line, a song, a text kept whole, English kept), and the host's line starts
 * (pixels - 128) / 2 further, centred in the wide box, its tiles, characters, steps, cursor and icons with it; the
 * game's buffers and VRAM keep it where the game draws it.  0, or -1 out of memory (the pages then none). */
int oracles_guest_set_text_box_pages(OraclesGuest *guest, const uint32_t *ranges, unsigned count);
/* Pure: whether physical offset `at` lies in one of `count` increasing pairs of `ranges`. */
int oracles_guest_text_box_in_pages(const uint32_t *ranges, unsigned count, uint32_t at);
#define ORACLES_GUEST_TEXT_BOX_WHOLE 0xffffu
/* Pure: the pixels of `line` shown from its start once `shown` steps are and `elapsed` frames have passed since the last
 * one: that step's characters one by one, each after the delay of the one before it, never past the step's boundary;
 * ORACLES_GUEST_TEXT_BOX_WHOLE past the last step. */
unsigned oracles_guest_text_box_reveal_pixels(const OraclesGuestTextBoxLine *line, unsigned shown, unsigned elapsed);
/* The host's line `which` (ORACLES_GUEST_TEXT_BOX_*): the top and bottom lines as the frame just scanned showed them,
 * the line composed as it stands; NULL when the box is not armed. */
const OraclesGuestTextBoxLine *oracles_guest_text_box_line(const OraclesGuest *guest, unsigned which);
/* The same as the next frame will show them, what a savestate carries. */
const OraclesGuestTextBoxLine *oracles_guest_text_box_next(const OraclesGuest *guest, unsigned which);
/* The lines and the reveal as a savestate's field: their bytes into `out`, the size written; 0 when not armed or
 * `capacity` is short. */
#define ORACLES_GUEST_TEXT_BOX_LINE_LAYOUT_SIZE (3u + (ORACLES_GUEST_LINE_COLUMNS + 1u) + 2u * ORACLES_GUEST_LINE_COLUMNS + 8u * ORACLES_GUEST_LINE_CHARACTERS)
#define ORACLES_GUEST_TEXT_BOX_STATE_SIZE \
    (4u + 3u + ORACLES_GUEST_TEXT_BOX_LINES * (2u + ORACLES_GUEST_TEXT_BOX_COLUMNS_MAX * 33u + ORACLES_GUEST_TEXT_BOX_LINE_LAYOUT_SIZE))
size_t oracles_guest_text_box_save_state(const OraclesGuest *guest, uint8_t *out, size_t capacity);
/* Loads them back: 0, or -1 for bytes of another format or width, the lines then unknown. */
int oracles_guest_text_box_load_state(OraclesGuest *guest, const uint8_t *data, size_t size);
typedef struct OraclesGuestTextBoxCounts {
    unsigned composed;        /* lines composed at drawLineOfText's return */
    unsigned copies;          /* lines copied to the top or bottom line at the game's DMA */
    unsigned reveal_writes;   /* the compact text's reveal writes made in the host's line */
    unsigned copies_unlike;   /* copies whose line composed differed from w7TextGfxBuffer in its first 128 pixels */
    unsigned symbols;         /* characters of the lines composed that are symbols ($06: an icon or a kanji) */
    unsigned wide_lines;      /* lines composed whose drawn pixels go past the game's 128 */
    unsigned overflows;       /* characters past the host's width, not drawn */
    unsigned centred;         /* lines composed of a page the game laid out, centred in the wide box */
    unsigned cells;           /* a character the game DMAed into a line's tiles itself, kept in the host's line (the fan games' cursor) */
} OraclesGuestTextBoxCounts;
void oracles_guest_text_box_counts(const OraclesGuest *guest, OraclesGuestTextBoxCounts *counts);
/* Pure: the font of the fixed advance, each character a whole cell: laid out in it, a line is the game's own. */
void oracles_guest_text_box_fixed_font(OraclesTextFont *font);

#ifdef __cplusplus
}
#endif

#endif
