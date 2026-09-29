#include "ppu.h"

#include <string.h>

#define LCDC_ENABLE 0x80u
#define LCDC_WIN_MAP 0x40u
#define LCDC_WIN_ENABLE 0x20u
#define LCDC_TILE_DATA 0x10u
#define LCDC_BG_MAP 0x08u
#define LCDC_OBJ_SIZE 0x04u
#define LCDC_OBJ_ENABLE 0x02u
#define LCDC_BG_PRIORITY 0x01u   /* CGB: master BG-to-OBJ priority */

#define ATTR_PRIORITY 0x80u
#define ATTR_FLIP_Y 0x40u
#define ATTR_FLIP_X 0x20u
#define ATTR_BANK 0x08u
#define ATTR_PALETTE 0x07u

#define OBJECTS 40
#define OBJECTS_PER_LINE 10

static uint8_t scale_channel(uint8_t x)
{
    return (uint8_t)((x << 3) | (x >> 2));
}

uint32_t oracles_ppu_rgb555(uint16_t colour)
{
    const uint8_t r = scale_channel(colour & 0x1fu);
    const uint8_t g = scale_channel((colour >> 5) & 0x1fu);
    const uint8_t b = scale_channel((colour >> 10) & 0x1fu);
    return 0xff000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static uint32_t palette_colour(const uint32_t *colours, const uint8_t *palettes, unsigned palette, unsigned index)
{
    const unsigned offset = (palette & 7u) * 8u + (index & 3u) * 2u;
    const uint16_t colour = (uint16_t)((palettes[offset] | (palettes[offset + 1] << 8)) & 0x7fffu);
    return colours ? colours[colour] : oracles_ppu_rgb555(colour);
}

/* Colour index (0-3) of pixel `column` of row `row` of the 16-byte tile at `data`. */
static unsigned tile_pixel(const uint8_t *data, unsigned row, unsigned column)
{
    const uint8_t low = data[row * 2], high = data[row * 2 + 1];
    const unsigned bit = 7u - column;
    return ((low >> bit) & 1u) | (((high >> bit) & 1u) << 1);
}

/* A background or window pixel: the tile map entry at (map_x, map_y) in tile
 * units, with its attribute from bank 1. Returns the colour index and fills
 * *palette and *priority. */
static unsigned map_pixel(const uint8_t *vram, uint8_t lcdc, unsigned map_base, unsigned px, unsigned py,
                          unsigned *palette, int *priority)
{
    const unsigned entry = map_base + (py / 8u) * 32u + (px / 8u);
    const uint8_t index = vram[entry];
    const uint8_t attr = vram[0x2000u + entry];
    unsigned data;
    if (lcdc & LCDC_TILE_DATA) data = (unsigned)index * 16u;
    else data = 0x1000u + (unsigned)((int8_t)index) * 16u;
    if (attr & ATTR_BANK) data += 0x2000u;
    unsigned row = py & 7u, column = px & 7u;
    if (attr & ATTR_FLIP_Y) row = 7u - row;
    if (attr & ATTR_FLIP_X) column = 7u - column;
    *palette = attr & ATTR_PALETTE;
    *priority = (attr & ATTR_PRIORITY) != 0;
    return tile_pixel(vram + data, row, column);
}

typedef struct obj_pixel {
    uint8_t colour;      /* 0: none */
    uint8_t palette;
    uint8_t priority;    /* attribute bit 7: behind BG colours 1-3 */
} obj_pixel;

/* The objects of one line, in OAM order, at most ten, drawn into `pixels`
 * with OAM-index priority (an earlier object keeps its pixels). */
static void render_objects(const OraclesPpuInput *in, unsigned ly, uint8_t lcdc, obj_pixel pixels[ORACLES_PPU_WIDTH])
{
    memset(pixels, 0, sizeof(obj_pixel) * ORACLES_PPU_WIDTH);
    const unsigned height = (lcdc & LCDC_OBJ_SIZE) ? 16u : 8u;
    unsigned selected = 0;
    for (unsigned i = 0; i < OBJECTS && selected < OBJECTS_PER_LINE; i++) {
        const uint8_t *entry = in->oam + i * 4u;
        const int y = (int)entry[0] - 16, x = (int)entry[1] - 8;
        if ((int)ly < y || (int)ly >= y + (int)height) continue;
        selected++;
        const uint8_t attr = entry[3];
        unsigned tile = entry[2];
        if (height == 16) tile &= 0xfeu;
        unsigned row = (unsigned)((int)ly - y);
        if (attr & ATTR_FLIP_Y) row = height - 1u - row;
        const unsigned data = tile * 16u + row * 2u + ((attr & ATTR_BANK) ? 0x2000u : 0u);
        const uint8_t low = in->vram[data], high = in->vram[data + 1];
        for (unsigned column = 0; column < 8; column++) {
            const int sx = x + (int)column;
            if (sx < 0 || sx >= ORACLES_PPU_WIDTH) continue;
            const unsigned bit = (attr & ATTR_FLIP_X) ? column : 7u - column;
            const unsigned colour = ((low >> bit) & 1u) | (((high >> bit) & 1u) << 1);
            if (colour == 0 || pixels[sx].colour) continue;
            pixels[sx].colour = (uint8_t)colour;
            pixels[sx].palette = attr & ATTR_PALETTE;
            pixels[sx].priority = (attr & ATTR_PRIORITY) != 0;
        }
    }
}

static void render_lines(const OraclesPpuInput *in, uint32_t *out, unsigned width)
{
    obj_pixel objects[ORACLES_PPU_WIDTH];
    int wy_triggered = 0;
    int window_line = -1;
    for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) {
        const OraclesPpuRegs *r = &in->lines[ly];
        uint32_t *row = out + ly * width;
        if (!(r->lcdc & LCDC_ENABLE)) {
            for (unsigned x = 0; x < width; x++) row[x] = ORACLES_PPU_WHITE;
            continue;
        }
        if (ly == r->wy && (r->lcdc & LCDC_WIN_ENABLE)) wy_triggered = 1;   /* as SameBoy: WY is checked while the window is enabled */
        const int window_on = (r->lcdc & LCDC_WIN_ENABLE) && wy_triggered && r->wx <= 166u;
        if (window_on) window_line++;
        render_objects(in, ly, r->lcdc, objects);
        const unsigned bg_map = (r->lcdc & LCDC_BG_MAP) ? 0x1c00u : 0x1800u;
        const unsigned win_map = (r->lcdc & LCDC_WIN_MAP) ? 0x1c00u : 0x1800u;
        for (unsigned x = 0; x < width; x++) {
            unsigned palette = 0;
            int priority = 0;
            unsigned colour;
            const int wx = (int)r->wx - 7;
            if (window_on && (int)x >= wx) {
                colour = map_pixel(in->vram, r->lcdc, win_map, (unsigned)((int)x - wx), (unsigned)window_line & 0xffu, &palette, &priority);
            } else {
                colour = map_pixel(in->vram, r->lcdc, bg_map, (x + r->scx) & 0xffu, (ly + r->scy) & 0xffu, &palette, &priority);
            }
            const obj_pixel *o = x < ORACLES_PPU_WIDTH ? &objects[x] : &objects[0];
            int draw_object = x < ORACLES_PPU_WIDTH && o->colour != 0 && (r->lcdc & LCDC_OBJ_ENABLE);
            if (draw_object) {
                int bg_priority = priority || o->priority;
                if (!(r->lcdc & LCDC_BG_PRIORITY)) bg_priority = 0;
                if (colour != 0 && bg_priority) draw_object = 0;
            }
            row[x] = draw_object ? palette_colour(in->colours, in->obj_palettes, o->palette, o->colour)
                                 : palette_colour(in->colours, in->bg_palettes, palette, colour);
        }
    }
}

void oracles_ppu_render(const OraclesPpuInput *in, uint32_t *out) { render_lines(in, out, ORACLES_PPU_WIDTH); }

void oracles_ppu_render_wide(const OraclesPpuInput *in, uint32_t *out, unsigned width)
{
    render_lines(in, out, width > 256u ? 256u : width);
}
