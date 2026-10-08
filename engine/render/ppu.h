/* Software CGB PPU.
 *
 * Renders one 160x144 frame from the display state the core committed at
 * vblank: VRAM, OAM, palette RAM, and the LCDC/SCY/SCX/WY/WX registers in
 * effect on each line, reconstructed from the register journal.  Applies the
 * CGB rules: ten objects per line in OAM order, OAM-index priority between
 * objects, BG-to-OBJ priority from the tile attribute, the object attribute
 * and LCDC bit 0, the window with its own line counter, and the colour
 * conversion of the core's active pipeline (raw RGB555 to RGB888, or the
 * player's colour correction) through a table the caller supplies. */
#ifndef ORACLES_PPU_H
#define ORACLES_PPU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ORACLES_PPU_WIDTH 160
#define ORACLES_PPU_HEIGHT 144
#define ORACLES_PPU_WHITE 0xffffffffu

typedef struct OraclesPpuRegs {
    uint8_t lcdc, scy, scx, wy, wx;
} OraclesPpuRegs;

typedef struct OraclesPpuInput {
    const uint8_t *vram;          /* 16 KiB: bank 0 then bank 1 */
    const uint8_t *oam;           /* 160 bytes */
    const uint8_t *bg_palettes;   /* 64 bytes, RGB555 little-endian */
    const uint8_t *obj_palettes;  /* 64 bytes */
    const uint32_t *colours;      /* 32768 pixels, one per RGB555 colour; NULL: the raw conversion */
    OraclesPpuRegs lines[ORACLES_PPU_HEIGHT];   /* registers in effect when each line was scanned */
} OraclesPpuInput;

#define ORACLES_PPU_COLOURS 32768u

/* Numeric 0xffRRGGBB pixels, as the core's encoder produces them. */
void oracles_ppu_render(const OraclesPpuInput *input, uint32_t *out);
/* The same over `width` columns per line (up to 256, the map's width), the
 * background and window only past the 160th: for the terrain of a large room
 * beyond the LCD window (`out` has `width` pixels per line, 144 lines). */
void oracles_ppu_render_wide(const OraclesPpuInput *input, uint32_t *out, unsigned width);
/* The same, lines `first` to `end` (excluded) only: the others are left as they are in `out`, the lines before
 * `first` still counted for the window as the LCD counts them. */
void oracles_ppu_render_wide_lines(const OraclesPpuInput *input, uint32_t *out, unsigned width, unsigned first, unsigned end);

/* The core's raw conversion of an RGB555 colour (colour correction disabled). */
uint32_t oracles_ppu_rgb555(uint16_t colour);

#ifdef __cplusplus
}
#endif

#endif
