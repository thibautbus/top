#include "compositor.h"

#include <stddef.h>
#include <string.h>

#include "ppu.h"

#define HUD ORACLES_ENHANCED_HUD_HEIGHT
#define CORE_W ORACLES_ENHANCED_CORE_WIDTH
#define AREA_H ORACLES_ENHANCED_AREA_HEIGHT

/* The game's palette fade applied to an RGB555 colour of a source that does
 * not carry it, as the palette thread does it (updateFadingPalettes): the
 * offset added to each five-bit channel, clamped; then the pixel of that
 * colour in the live pipeline, the one the core's window went through. */
static uint32_t faded(const OraclesEnhancedCompose *in, uint32_t rgb555, int fade)
{
    unsigned colour = 0;
    for (unsigned shift = 0; shift < 15u; shift += 5u) {
        const int v = (int)((rgb555 >> shift) & 31u) + fade;
        colour |= (unsigned)(v < 0 ? 0 : v > 31 ? 31 : v) << shift;
    }
    if (in->colours) return in->colours[colour & (ORACLES_PPU_COLOURS - 1u)];
    uint32_t out = 0xff000000u;
    for (unsigned shift = 0; shift < 15u; shift += 5u) out |= (uint32_t)(((colour >> shift) & 31u) * 255u / 31u) << (shift / 5u * 8u);
    return out;
}

/* The gutters and what no source covers: black, faded with the game (the
 * border colour when nothing fades). */
static uint32_t gutter_pixel(const OraclesEnhancedCompose *in, uint32_t border)
{
    return in->fade ? faded(in, 0, in->fade) : border;
}

/* One row of the world band, the sources painted as rectangles, span by
 * span: the neighbours from the last to the first, then the core's window, so
 * that the window wins, then the first neighbour whose area covers a pixel (a
 * neighbour of another row does not hide the one behind it); what no source
 * painted is black, counted as uncovered inside the counted rectangle.  The
 * line's own scroll (a ripple under water) moves the world under the row:
 * every source is read at the same shifted place, so the whole band ripples
 * with the room, and the window, drawn with that scroll, stays in place.
 * `row` holds the band's columns x0 to x1. */
static unsigned compose_row(const OraclesEnhancedCompose *in, const uint32_t *core, uint32_t *row, unsigned x0, unsigned x1,
                            int32_t world_y, int32_t shift_x, int32_t shift_y, uint32_t black)
{
    uint8_t painted[ORACLES_ENHANCED_MAX_WIDTH];
    memset(painted + x0, 0, x1 - x0);
    /* The band column x shows world column left + x (the shift applied). */
    const int32_t left = in->world_left + shift_x, wy = world_y + shift_y;
    for (unsigned n = in->neighbour_count; n-- > 0;) {
        const OraclesEnhancedNeighbour *nb = &in->neighbours[n];
        const unsigned width = nb->width ? nb->width : CORE_W, height = nb->height ? nb->height : AREA_H;
        const int32_t ny = wy - nb->world_top;
        if (!nb->game_area || ny < 0 || ny >= (int32_t)height) continue;
        int32_t from = nb->world_left - left, to = from + (int32_t)width;
        if (from < (int32_t)x0) from = (int32_t)x0;
        if (to > (int32_t)x1) to = (int32_t)x1;
        const uint32_t *src = nb->game_area + (size_t)ny * width;
        const int32_t offset = left - nb->world_left;   /* the source column of band column x is x + offset, in the span */
        if (from >= to) continue;
        if (nb->faded) memcpy(row + from, src + from + offset, (size_t)(to - from) * sizeof *row);
        else for (int32_t x = from; x < to; x++) row[x] = faded(in, src[x + offset], in->fade);
        memset(painted + from, 1, (size_t)(to - from));
    }
    /* The window: the game area starts after the status bar; the scroll moves it with the world. */
    const int32_t line = world_y - in->window_world_top;
    if (line >= 0 && line < (int32_t)AREA_H) {
        int32_t from = in->window_world_left - in->world_left, to = from + (int32_t)CORE_W;
        const uint32_t *src = core + (HUD + (unsigned)line) * CORE_W;
        const int32_t offset = in->world_left - in->window_world_left;
        if (from < (int32_t)x0) from = (int32_t)x0;
        if (to > (int32_t)x1) to = (int32_t)x1;
        if (from < to) {
            memcpy(row + from, src + from + offset, (size_t)(to - from) * sizeof *row);
            memset(painted + from, 1, (size_t)(to - from));
        }
    }
    unsigned uncovered = 0;
    const int counted_row = !in->counted_width || (wy >= in->counted_top && wy < in->counted_top + (int32_t)in->counted_height);
    for (unsigned x = x0; x < x1; x++) {
        if (painted[x]) continue;
        row[x] = black;
        const int32_t wx = left + (int32_t)x;
        if (counted_row && (!in->counted_width || (wx >= in->counted_left && wx < in->counted_left + (int32_t)in->counted_width))) uncovered++;
    }
    return uncovered;
}

/* The framed core: its 160x144 image centred in the surface (vertically too
 * in the drawn-back view's taller one), the rest the border. */
static void compose_framed(const uint32_t *core, const OraclesEnhancedCompose *in, OraclesEnhancedSize size, uint32_t *out)
{
    const uint32_t gutter = gutter_pixel(in, in->border);   /* a flash covers the frame's gutters as well */
    const unsigned W = size.width, H = size.height, HUD_X = oracles_enhanced_hud_x(size), top = (H - ORACLES_PPU_HEIGHT) / 2u;
    for (unsigned y = 0; y < H; y++)
        for (unsigned x = 0; x < W; x++)
            out[y * W + x] = (x >= HUD_X && x < HUD_X + CORE_W && y >= top && y < top + ORACLES_PPU_HEIGHT) ? core[(y - top) * CORE_W + (x - HUD_X)] : gutter;
}

static unsigned compose_world(const uint32_t *core, const OraclesEnhancedCompose *in, OraclesEnhancedSize size, uint32_t *out)
{
    unsigned uncovered = 0;
    const unsigned W = size.width, HUD_X = oracles_enhanced_hud_x(size), band_h = oracles_enhanced_band_height(size);
    /* HUD band: the status bar, centred, gutters in the border colour (faded with the game). */
    const uint32_t gutter = gutter_pixel(in, in->border);
    for (unsigned y = 0; y < HUD; y++)
        for (unsigned x = 0; x < W; x++)
            out[y * W + x] = (x >= HUD_X && x < HUD_X + CORE_W) ? core[y * CORE_W + (x - HUD_X)] : gutter;
    /* World band: the game area placed by the camera; uncovered pixels black.
     * A band row shows the LCD line of the window that stands there, and that
     * line's own scroll: the shifts are read by line, not by band row, or the
     * neighbours would ripple out of step with the window beside them (the
     * band row and the line differ by the vertical camera).  The rows above and
     * below the window take the line a taller screen would have there, modulo
     * the area's 128: the game's waves repeat every 128 lines (docs/GAME_HOOKS.md,
     * section 1), so the area's lines are whole periods and the
     * phase goes on past them.  Outside the part of the band shown, black,
     * not counted. */
    const unsigned shown_w = in->shown_width ? in->shown_width : W, shown_h = in->shown_height ? in->shown_height : band_h;
    const unsigned x0 = (W - shown_w) / 2u, y0 = (band_h - shown_h) / 2u;
    const uint32_t black = gutter_pixel(in, 0xff000000u);
    for (unsigned y = 0; y < band_h; y++) {
        uint32_t *row = out + (HUD + y) * W;
        if (y < y0 || y >= y0 + shown_h) { for (unsigned x = 0; x < W; x++) row[x] = black; continue; }
        const int32_t world_y = in->world_top + (int32_t)y;
        const int32_t line = (int32_t)((uint32_t)(world_y - in->window_world_top) % AREA_H);   /* AREA_H a power of two: a floor modulo */
        const int32_t shift_x = in->line_shift ? in->line_shift[line] : 0, shift_y = in->line_shift_y ? in->line_shift_y[line] : 0;
        for (unsigned x = 0; x < x0; x++) row[x] = black;
        for (unsigned x = x0 + shown_w; x < W; x++) row[x] = black;
        uncovered += compose_row(in, core, row, x0, x0 + shown_w, world_y, shift_x, shift_y, black);
    }
    return uncovered;
}

unsigned oracles_enhanced_compose(const uint32_t *core, const OraclesEnhancedCompose *in, uint32_t *out)
{
    const OraclesEnhancedSize size = in->size.width ? in->size : oracles_enhanced_size(0);
    if (in->mode == ORACLES_ENHANCED_FRAMED) { compose_framed(core, in, size, out); return 0; }
    return compose_world(core, in, size, out);
}
