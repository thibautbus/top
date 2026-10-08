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

/* A tile row's byte spread over eight bytes, its pixel k in byte k (little-endian): `order` picks the bit each byte
 * keeps, bit 7 first (0x0102040810204080) or, flipped, bit 0 first (0x8040201008040201); each byte is then 1 if its
 * bit was set (adding 0x7f to a byte of one bit sets its bit 7, without a carry out of it). */
static uint64_t spread(uint8_t byte, uint64_t order)
{
    const uint64_t kept = (byte * 0x0101010101010101ull) & order;
    return (((kept + 0x7f7f7f7f7f7f7f7full) | kept) & 0x8080808080808080ull) >> 7;
}

/* A span of a background or window line: `count` pixels from map pixel (px, py) of the map at `map_base`, a tile at
 * a time, each as its colour index (bits 0-1), its palette (bits 2-4) and its BG priority (bit 7).  The background
 * wraps at 256; the window does not (its last tile column may read the next map row, as it always has).  `out` has
 * room for seven more bytes, which a tile written whole may fill past `count`. */
static void map_span(const uint8_t *vram, uint8_t lcdc, unsigned map_base, unsigned px, unsigned py, int wrap,
                     uint8_t *out, unsigned count)
{
    const unsigned row_base = map_base + (py / 8u) * 32u, fine_y = py & 7u;
    unsigned n = 0;
    while (n < count) {
        const unsigned entry = row_base + px / 8u;
        const uint8_t index = vram[entry];
        const uint8_t attr = vram[0x2000u + entry];
        unsigned data;
        if (lcdc & LCDC_TILE_DATA) data = (unsigned)index * 16u;
        else data = 0x1000u + (unsigned)((int8_t)index) * 16u;
        if (attr & ATTR_BANK) data += 0x2000u;
        const unsigned row = (attr & ATTR_FLIP_Y) ? 7u - fine_y : fine_y;
        const uint64_t order = (attr & ATTR_FLIP_X) ? 0x8040201008040201ull : 0x0102040810204080ull;
        const uint8_t base = (uint8_t)(((attr & ATTR_PALETTE) << 2) | ((attr & ATTR_PRIORITY) ? 0x80u : 0u));
        const uint64_t pixels = spread(vram[data + row * 2], order) | (spread(vram[data + row * 2 + 1], order) << 1) | (base * 0x0101010101010101ull);
        uint8_t bytes[8];
        for (unsigned k = 0; k < 8u; k++) bytes[k] = (uint8_t)(pixels >> (8u * k));
        const unsigned column = px & 7u, take = 8u - column;
        memcpy(out + n, bytes + column, take);
        n += take;
        px += take;
        if (wrap) px &= 0xffu;
    }
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

static void render_lines(const OraclesPpuInput *in, uint32_t *out, unsigned width, unsigned first, unsigned end)
{
    /* The 32 colours of each kind of palette, looked up once. */
    uint32_t bg_colours[32], obj_colours[32];
    for (unsigned i = 0; i < 32u; i++) {
        bg_colours[i] = palette_colour(in->colours, in->bg_palettes, i >> 2, i & 3u);
        obj_colours[i] = palette_colour(in->colours, in->obj_palettes, i >> 2, i & 3u);
    }
    obj_pixel objects[ORACLES_PPU_WIDTH];
    uint8_t line[256 + 8];   /* a span's last tile may be written whole */
    int wy_triggered = 0;
    int window_line = -1;
    for (unsigned ly = 0; ly < end; ly++) {
        const OraclesPpuRegs *r = &in->lines[ly];
        uint32_t *row = out + ly * width;
        if (!(r->lcdc & LCDC_ENABLE)) {
            if (ly < first) continue;
            for (unsigned x = 0; x < width; x++) row[x] = ORACLES_PPU_WHITE;
            continue;
        }
        if (ly == r->wy && (r->lcdc & LCDC_WIN_ENABLE)) wy_triggered = 1;   /* as SameBoy: WY is checked while the window is enabled */
        const int window_on = (r->lcdc & LCDC_WIN_ENABLE) && wy_triggered && r->wx <= 166u;
        if (window_on) window_line++;
        if (ly < first) continue;   /* a line before the range only moves the window's line on */
        const unsigned bg_map = (r->lcdc & LCDC_BG_MAP) ? 0x1c00u : 0x1800u;
        const unsigned win_map = (r->lcdc & LCDC_WIN_MAP) ? 0x1c00u : 0x1800u;
        /* The background up to the window's first pixel, the window from it. */
        const int wx = (int)r->wx - 7;
        const unsigned split = !window_on ? width : wx <= 0 ? 0u : (unsigned)wx < width ? (unsigned)wx : width;
        map_span(in->vram, r->lcdc, bg_map, r->scx, (ly + r->scy) & 0xffu, 1, line, split);
        if (split < width) map_span(in->vram, r->lcdc, win_map, (unsigned)((int)split - wx), (unsigned)window_line & 0xffu, 0, line + split, width - split);
        unsigned x = 0;
        if (r->lcdc & LCDC_OBJ_ENABLE) {
            render_objects(in, ly, r->lcdc, objects);
            const int bg_over = (r->lcdc & LCDC_BG_PRIORITY) != 0;
            for (; x < width && x < ORACLES_PPU_WIDTH; x++) {
                const unsigned bg = line[x];
                const obj_pixel *o = &objects[x];
                /* the object, but under a BG colour 1-3 that has priority (its own or the object's bit) */
                if (o->colour && !(bg_over && (bg & 3u) && ((bg & 0x80u) || o->priority))) row[x] = obj_colours[(o->palette & 7u) * 4u + o->colour];
                else row[x] = bg_colours[bg & 31u];
            }
        }
        for (; x < width; x++) row[x] = bg_colours[line[x] & 31u];
    }
}

void oracles_ppu_render(const OraclesPpuInput *in, uint32_t *out) { render_lines(in, out, ORACLES_PPU_WIDTH, 0, ORACLES_PPU_HEIGHT); }

void oracles_ppu_render_wide(const OraclesPpuInput *in, uint32_t *out, unsigned width)
{
    render_lines(in, out, width > 256u ? 256u : width, 0, ORACLES_PPU_HEIGHT);
}

void oracles_ppu_render_wide_lines(const OraclesPpuInput *in, uint32_t *out, unsigned width, unsigned first, unsigned end)
{
    if (end > ORACLES_PPU_HEIGHT) end = ORACLES_PPU_HEIGHT;
    render_lines(in, out, width > 256u ? 256u : width, first, end);
}
