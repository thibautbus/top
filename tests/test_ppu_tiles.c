/* The software PPU against a reference that draws each pixel on its own, as the renderer did before it drew a tile at
 * a time: random VRAM, OAM, palettes and per-line registers (the window on and off, flips, both banks, both tile data
 * areas, objects of 8 and 16 lines, the BG priority bits), at every width the view renders, with and without a colour
 * table, and over a range of lines.  The two must give the same pixels. */
#include "ppu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

/* The reference: each pixel looked up in the map, its tile and its palette. */
static uint32_t ref_palette_colour(const uint32_t *colours, const uint8_t *palettes, unsigned palette, unsigned index)
{
    const unsigned offset = (palette & 7u) * 8u + (index & 3u) * 2u;
    const uint16_t colour = (uint16_t)((palettes[offset] | (palettes[offset + 1] << 8)) & 0x7fffu);
    return colours ? colours[colour] : oracles_ppu_rgb555(colour);
}

static unsigned ref_map_pixel(const uint8_t *vram, uint8_t lcdc, unsigned map_base, unsigned px, unsigned py, unsigned *palette, int *priority)
{
    const unsigned entry = map_base + (py / 8u) * 32u + (px / 8u);
    const uint8_t index = vram[entry], attr = vram[0x2000u + entry];
    unsigned data = (lcdc & 0x10u) ? (unsigned)index * 16u : 0x1000u + (unsigned)((int8_t)index) * 16u;
    if (attr & 0x08u) data += 0x2000u;
    unsigned row = py & 7u, column = px & 7u;
    if (attr & 0x40u) row = 7u - row;
    if (attr & 0x20u) column = 7u - column;
    *palette = attr & 7u;
    *priority = (attr & 0x80u) != 0;
    const uint8_t low = vram[data + row * 2], high = vram[data + row * 2 + 1];
    const unsigned bit = 7u - column;
    return ((low >> bit) & 1u) | (((high >> bit) & 1u) << 1);
}

typedef struct ref_obj { uint8_t colour, palette, priority; } ref_obj;

static void ref_objects(const OraclesPpuInput *in, unsigned ly, uint8_t lcdc, ref_obj pixels[ORACLES_PPU_WIDTH])
{
    memset(pixels, 0, sizeof(ref_obj) * ORACLES_PPU_WIDTH);
    const unsigned height = (lcdc & 0x04u) ? 16u : 8u;
    unsigned selected = 0;
    for (unsigned i = 0; i < 40u && selected < 10u; i++) {
        const uint8_t *entry = in->oam + i * 4u;
        const int y = (int)entry[0] - 16, x = (int)entry[1] - 8;
        if ((int)ly < y || (int)ly >= y + (int)height) continue;
        selected++;
        const uint8_t attr = entry[3];
        unsigned tile = entry[2];
        if (height == 16) tile &= 0xfeu;
        unsigned row = (unsigned)((int)ly - y);
        if (attr & 0x40u) row = height - 1u - row;
        const unsigned data = tile * 16u + row * 2u + ((attr & 0x08u) ? 0x2000u : 0u);
        const uint8_t low = in->vram[data], high = in->vram[data + 1];
        for (unsigned column = 0; column < 8; column++) {
            const int sx = x + (int)column;
            if (sx < 0 || sx >= ORACLES_PPU_WIDTH) continue;
            const unsigned bit = (attr & 0x20u) ? column : 7u - column;
            const unsigned colour = ((low >> bit) & 1u) | (((high >> bit) & 1u) << 1);
            if (colour == 0 || pixels[sx].colour) continue;
            pixels[sx].colour = (uint8_t)colour;
            pixels[sx].palette = attr & 7u;
            pixels[sx].priority = (attr & 0x80u) != 0;
        }
    }
}

static void ref_render(const OraclesPpuInput *in, uint32_t *out, unsigned width)
{
    ref_obj objects[ORACLES_PPU_WIDTH];
    int wy_triggered = 0, window_line = -1;
    for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) {
        const OraclesPpuRegs *r = &in->lines[ly];
        uint32_t *row = out + ly * width;
        if (!(r->lcdc & 0x80u)) { for (unsigned x = 0; x < width; x++) row[x] = ORACLES_PPU_WHITE; continue; }
        if (ly == r->wy && (r->lcdc & 0x20u)) wy_triggered = 1;
        const int window_on = (r->lcdc & 0x20u) && wy_triggered && r->wx <= 166u;
        if (window_on) window_line++;
        ref_objects(in, ly, r->lcdc, objects);
        const unsigned bg_map = (r->lcdc & 0x08u) ? 0x1c00u : 0x1800u, win_map = (r->lcdc & 0x40u) ? 0x1c00u : 0x1800u;
        for (unsigned x = 0; x < width; x++) {
            unsigned palette = 0, colour;
            int priority = 0;
            const int wx = (int)r->wx - 7;
            if (window_on && (int)x >= wx) colour = ref_map_pixel(in->vram, r->lcdc, win_map, (unsigned)((int)x - wx), (unsigned)window_line & 0xffu, &palette, &priority);
            else colour = ref_map_pixel(in->vram, r->lcdc, bg_map, (x + r->scx) & 0xffu, (ly + r->scy) & 0xffu, &palette, &priority);
            const ref_obj *o = x < ORACLES_PPU_WIDTH ? &objects[x] : &objects[0];
            int draw_object = x < ORACLES_PPU_WIDTH && o->colour != 0 && (r->lcdc & 0x02u);
            if (draw_object) {
                int bg_priority = priority || o->priority;
                if (!(r->lcdc & 0x01u)) bg_priority = 0;
                if (colour != 0 && bg_priority) draw_object = 0;
            }
            row[x] = draw_object ? ref_palette_colour(in->colours, in->obj_palettes, o->palette, o->colour)
                                 : ref_palette_colour(in->colours, in->bg_palettes, palette, colour);
        }
    }
}

static uint32_t seed = 12345u;
static uint32_t next(void) { seed = seed * 1664525u + 1013904223u; return seed >> 8; }

int main(void)
{
    static uint8_t vram[0x4000], oam[160], bg[64], obj[64];
    static uint32_t colours[ORACLES_PPU_COLOURS], a[ORACLES_PPU_HEIGHT * 256u], b[ORACLES_PPU_HEIGHT * 256u];
    for (unsigned i = 0; i < ORACLES_PPU_COLOURS; i++) colours[i] = 0xff000000u | (i * 2654435761u >> 8);
    static const unsigned widths[] = { ORACLES_PPU_WIDTH, 240u, 256u, 213u };
    unsigned compared = 0;
    for (unsigned trial = 0; trial < 2000u; trial++) {
        for (unsigned i = 0; i < sizeof vram; i++) vram[i] = (uint8_t)next();
        for (unsigned i = 0; i < sizeof oam; i++) oam[i] = (uint8_t)next();
        if (trial & 1u) for (unsigned i = 0; i < 40u; i++) { oam[i * 4] = (uint8_t)(next() % 176u); oam[i * 4 + 1] = (uint8_t)(next() % 176u); }   /* on screen, most of them */
        for (unsigned i = 0; i < 64u; i++) { bg[i] = (uint8_t)next(); obj[i] = (uint8_t)next(); }
        OraclesPpuInput in = { vram, oam, bg, obj, (trial & 2u) ? colours : NULL, { { 0 } } };
        /* Registers that hold for a stretch of lines and change, as a scroll split or a status bar does. */
        OraclesPpuRegs r = { 0 };
        for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) {
            if (ly == 0 || next() % 24u == 0) {
                r.lcdc = (uint8_t)(next() | ((trial % 7u) ? 0x80u : 0u));
                r.scy = (uint8_t)next(); r.scx = (uint8_t)next();
                r.wy = (uint8_t)(next() % 160u); r.wx = (uint8_t)(next() % 180u);
            }
            in.lines[ly] = r;
        }
        const unsigned width = widths[trial % 4u];
        ref_render(&in, a, width);
        if (width == ORACLES_PPU_WIDTH && !(trial & 4u)) oracles_ppu_render(&in, b);
        else oracles_ppu_render_wide(&in, b, width);
        const int same = memcmp(a, b, (size_t)width * ORACLES_PPU_HEIGHT * sizeof a[0]) == 0;
        CHECK(same);
        if (!same) { fprintf(stderr, "trial %u, width %u\n", trial, width); break; }
        /* A range of lines: those drawn equal the reference's, the others untouched. */
        const unsigned first = next() % ORACLES_PPU_HEIGHT, end = first + 1u + next() % (ORACLES_PPU_HEIGHT - first);
        memset(b, 0x5a, sizeof b);
        oracles_ppu_render_wide_lines(&in, b, width, first, end);
        int range_same = memcmp(a + first * width, b + first * width, (size_t)(end - first) * width * sizeof a[0]) == 0;
        for (unsigned i = 0; i < first * width && range_same; i++) range_same = b[i] == 0x5a5a5a5au;
        for (unsigned i = end * width; i < ORACLES_PPU_HEIGHT * width && range_same; i++) range_same = b[i] == 0x5a5a5a5au;
        CHECK(range_same);
        if (!range_same) { fprintf(stderr, "trial %u, width %u, lines %u to %u\n", trial, width, first, end); break; }
        compared++;
    }
    CHECK(compared == 2000u);
    if (failures) return 1;
    printf("ppu tiles: %u renders equal to the per-pixel reference\n", compared);
    return 0;
}
