#include "compositor.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "ppu.h"

#define HUD ORACLES_ENHANCED_HUD_HEIGHT
#define CORE_W ORACLES_ENHANCED_CORE_WIDTH
#define AREA_H ORACLES_ENHANCED_AREA_HEIGHT

/* The game's palette fade applied to an RGB555 colour of a source that does
 * not carry it, as the palette thread does it (updateFadingPalettes): the
 * offset added to each five-bit channel, clamped; then the pixel of that
 * colour in the live pipeline, the one the core's window went through. */
static uint32_t pixel_of(const uint32_t *colours, unsigned colour)
{
    if (colours) return colours[colour & (ORACLES_PPU_COLOURS - 1u)];
    uint32_t out = 0xff000000u;
    for (unsigned shift = 0; shift < 15u; shift += 5u) out |= (uint32_t)(((colour >> shift) & 31u) * 255u / 31u) << (shift / 5u * 8u);
    return out;
}

uint32_t oracles_enhanced_faded_pixel(const uint32_t *colours, uint32_t rgb555, int fade)
{
    unsigned colour = 0;
    for (unsigned shift = 0; shift < 15u; shift += 5u) {
        const int v = (int)((rgb555 >> shift) & 31u) + fade;
        colour |= (unsigned)(v < 0 ? 0 : v > 31 ? 31 : v) << shift;
    }
    return pixel_of(colours, colour);
}

static uint32_t faded(const OraclesEnhancedCompose *in, uint32_t rgb555, int fade) { return oracles_enhanced_faded_pixel(in->colours, rgb555, fade); }

uint16_t oracles_enhanced_bar_tile_pixel(const OraclesEnhancedBar *bar, uint8_t tile_number, uint8_t attr, unsigned x, unsigned y, unsigned *index_out)
{
    const unsigned tile = (bar->lcdc & 0x10u) ? tile_number : 256u + (uint8_t)(tile_number + 128u) - 128u;
    const unsigned line = (attr & 0x40u) ? 7u - y % 8u : y % 8u, bit = (attr & 0x20u) ? x % 8u : 7u - x % 8u;
    const uint8_t *data = ((attr & 0x08u) ? bar->vram1 : bar->vram0) + tile * 16u + line * 2u;
    const unsigned index = ((data[0] >> bit) & 1u) | (((data[1] >> bit) & 1u) << 1);
    const uint8_t *colour = bar->palettes + ((attr & 7u) * 4u + index) * 2u;
    if (index_out) *index_out = index;
    return (uint16_t)((colour[0] | (colour[1] << 8)) & 0x7fffu);
}

uint16_t oracles_enhanced_bar_pixel(const OraclesEnhancedBar *bar, unsigned x, unsigned y, int *hides_sprite)
{
    const unsigned cell = (y / 8u) * 32u + x / 8u;
    const uint8_t attr = bar->attributes[cell];
    unsigned index = 0;
    const uint16_t colour = oracles_enhanced_bar_tile_pixel(bar, bar->tiles[cell], attr, x, y, &index);
    if (hides_sprite) *hides_sprite = (bar->lcdc & 0x01u) && (attr & 0x80u) && index;
    return colour;
}

unsigned oracles_enhanced_hud_fill(const OraclesEnhancedBar *bar, uint32_t (*pixel)(void *opaque, uint16_t rgb555), void *opaque,
                                   const uint32_t *core, uint32_t fill[2u * HUD])
{
    unsigned filled = 0;
    for (unsigned y = 0; y < HUD; y++) {
        const uint32_t *line = core + y * CORE_W;
        unsigned x = 1;
        while (x < CORE_W && line[x] == line[0]) x++;
        const int blank = x == CORE_W;
        for (unsigned side = 0; side < 2u; side++) {
            const unsigned edge = side ? CORE_W - 1u : 0u;
            const uint16_t rgb555 = oracles_enhanced_bar_pixel(bar, edge, y, NULL);
            const uint32_t colour = pixel ? pixel(opaque, rgb555) : pixel_of(NULL, rgb555);
            fill[side * HUD + y] = line[edge] == colour || blank ? line[edge] : 0u;
            filled += fill[side * HUD + y] != 0u;
        }
    }
    return filled;
}

/* The cells of each layout's frame that sign it in the bar's map, row and column, tile and attributes, as the game
 * loads them (docs/GAME_HOOKS.md, section 1): the brackets no icon, count or heart is written over. */
typedef struct bar_cell { uint8_t row, column, tile, attributes; } bar_cell;
#define BAR_SIGNATURE 4u
static const bar_cell bar_signatures[ORACLES_ENHANCED_BAR_BIGGORON + 1u][BAR_SIGNATURE] = {
    [ORACLES_ENHANCED_BAR_NORMAL] = { { 0, 4, 0x06, 0xe0 }, { 1, 4, 0x06, 0xa0 }, { 0, 9, 0x06, 0xe0 }, { 1, 9, 0x06, 0xa0 } },
    [ORACLES_ENHANCED_BAR_TIGHT] = { { 0, 4, 0x07, 0xc0 }, { 1, 4, 0x07, 0x80 }, { 0, 8, 0x08, 0xa0 }, { 1, 8, 0x06, 0xa0 } },
    [ORACLES_ENHANCED_BAR_BIGGORON] = { { 0, 1, 0x05, 0x80 }, { 1, 1, 0x06, 0x80 }, { 0, 7, 0x08, 0xa0 }, { 1, 7, 0x06, 0xa0 } },
};

OraclesEnhancedBarLayout oracles_enhanced_bar_layout(const OraclesEnhancedBar *bar)
{
    for (unsigned l = ORACLES_ENHANCED_BAR_NORMAL; l <= ORACLES_ENHANCED_BAR_BIGGORON; l++) {
        unsigned i = 0;
        for (const bar_cell *c = bar_signatures[l]; i < BAR_SIGNATURE; i++, c++)
            if (bar->tiles[c->row * 32u + c->column] != c->tile || bar->attributes[c->row * 32u + c->column] != c->attributes) break;
        if (i == BAR_SIGNATURE) return (OraclesEnhancedBarLayout)l;
    }
    return ORACLES_ENHANCED_BAR_NONE;
}

int oracles_enhanced_bar_layout_shown(const OraclesEnhancedBar *bar, OraclesEnhancedBarLayout layout, uint32_t (*pixel)(void *opaque, uint16_t rgb555),
                                      void *opaque, const uint32_t *core)
{
    if (layout <= ORACLES_ENHANCED_BAR_NONE || layout > ORACLES_ENHANCED_BAR_BIGGORON) return 0;
    uint8_t tiles[64] = { 0 }, attributes[64] = { 0 };
    OraclesEnhancedBar frame = *bar;
    frame.tiles = tiles;
    frame.attributes = attributes;
    for (unsigned i = 0; i < BAR_SIGNATURE; i++) {
        const bar_cell *c = &bar_signatures[layout][i];
        tiles[c->row * 32u + c->column] = c->tile;
        attributes[c->row * 32u + c->column] = c->attributes;
        for (unsigned y = c->row * 8u; y < c->row * 8u + 8u; y++)
            for (unsigned x = c->column * 8u; x < c->column * 8u + 8u; x++) {
                const uint16_t rgb555 = oracles_enhanced_bar_pixel(&frame, x, y, NULL);
                if (core[y * CORE_W + x] != (pixel ? pixel(opaque, rgb555) : pixel_of(NULL, rgb555))) return 0;
            }
    }
    return 1;
}

/* One line of the HUD band with the bar laid out: the background (the left gutter's to the band's middle, the right's
 * beyond), then each block from the bar's line, the hearts' second row beside their first on the middle eight lines. */
static void hud_wide_line(const OraclesEnhancedHudPlace *p, const uint32_t *core, unsigned y, unsigned W, uint32_t left, uint32_t right, uint32_t *line)
{
    const uint32_t *bar = core + y * CORE_W;
    const unsigned hearts_x = CORE_W - p->hearts, top = (HUD - 8u) / 2u;
    for (unsigned x = 0; x < W; x++) line[x] = x < W / 2u ? left : right;
    memcpy(line + p->buttons_at, bar, p->buttons * sizeof *line);
    memcpy(line + p->money_at, bar + p->buttons, ORACLES_ENHANCED_BAR_MONEY * sizeof *line);
    if (!p->one_row) memcpy(line + p->hearts_at, bar + hearts_x, p->hearts * sizeof *line);
    else if (y >= top && y < top + 8u)
        for (unsigned row = 0; row < 2u; row++) memcpy(line + p->hearts_at + row * p->hearts, core + (y - top + row * 8u) * CORE_W + hearts_x, p->hearts * sizeof *line);
}

/* The gutters and what no source covers: black, faded with the game (the
 * border colour when nothing fades). */
static uint32_t gutter_pixel(const OraclesEnhancedCompose *in, uint32_t border)
{
    return in->fade ? faded(in, 0, in->fade) : border;
}

/* A pixel `veil` 256ths of the way to `black`, channel by channel: red and blue together, then green, none of the
 * sums reaching the next channel. */
static uint32_t toward(uint32_t pixel, uint32_t black, unsigned veil)
{
    const uint32_t keep = 256u - veil;
    const uint32_t rb = (((pixel & 0xff00ffu) * keep + (black & 0xff00ffu) * veil) >> 8) & 0xff00ffu;
    const uint32_t g = (((pixel & 0x00ff00u) * keep + (black & 0x00ff00u) * veil) >> 8) & 0x00ff00u;
    return (pixel & 0xff000000u) | rb | g;
}

/* ---- the edge of a room kept dark ---------------------------------------------------------- */

#define EDGE_DEPTH ((int)ORACLES_ENHANCED_EDGE_DEPTH)
#define EDGE_STEP ((int)ORACLES_ENHANCED_EDGE_STEP)
#define EDGE_SOURCES 64u     /* the sources that may take an edge: the view's cached rooms, its captures */
#define EDGE_POOL (40u * 2u * (160u + 128u))   /* the waves of forty rooms of the overworld, a room taking its sides' */
#define EDGE_NONE 0xffu

/* The sides in the order of the ghost's directions, and the corners between them: corner s between side s - 1 and side s. */
enum { EDGE_UP, EDGE_RIGHT, EDGE_DOWN, EDGE_LEFT };

typedef struct edge_room {
    unsigned side[4], corner[4];        /* the light of what is drawn beside it that way, 256 less its veil; 0: nothing in clear */
    uint8_t *wave[4];                   /* each side's pixels in full along it: up and down by column, left and right by row */
} edge_room;

typedef struct edge_set {
    edge_room rooms[EDGE_SOURCES];
    uint8_t of[EDGE_SOURCES];           /* each source's room, EDGE_NONE without an edge */
    uint8_t pool[EDGE_POOL];            /* the waves, room after room */
} edge_set;

static uint32_t edge_hash(uint32_t seed, uint32_t line, uint32_t knot)
{
    uint32_t h = seed * 0x9e3779b9u + line * 0x85ebca6bu + knot * 0xc2b2ae35u;
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return h >> 16;
}

/* The smooth step from a knot to the next at each of the places between them, in 4096ths. */
static const uint32_t edge_smooth[ORACLES_ENHANCED_EDGE_PERIOD] = { 0u, 303u, 1062u, 2048u, 3034u, 3793u };

static int32_t edge_knot(int32_t along)
{
    const int32_t period = (int32_t)ORACLES_ENHANCED_EDGE_PERIOD;
    return along >= 0 ? along / period : -((period - 1 - along) / period);
}

/* The wave between two knots' values, at the place `place` after the first. */
static uint8_t edge_between(uint32_t h0, uint32_t h1, int32_t place)
{
    const uint32_t s = edge_smooth[place], v = h0 * (4096u - s) + h1 * s;   /* under 2^28 */
    return (uint8_t)((ORACLES_ENHANCED_EDGE_WAVE * v + (1u << 28) - 1u) >> 28);
}

unsigned oracles_enhanced_edge_wave(uint32_t seed, int32_t line, int vertical, int32_t along)
{
    const int32_t knot = edge_knot(along);
    const uint32_t id = (uint32_t)line * 2u + (vertical != 0);
    return edge_between(edge_hash(seed, id, (uint32_t)knot), edge_hash(seed, id, (uint32_t)knot + 1u), along - knot * (int32_t)ORACLES_ENHANCED_EDGE_PERIOD);
}

/* The wave of `count` places from `first` on a line, each knot hashed once. */
static void edge_wave_line(uint32_t seed, int32_t line, int vertical, int32_t first, unsigned count, uint8_t *out)
{
    const uint32_t id = (uint32_t)line * 2u + (vertical != 0);
    int32_t knot = edge_knot(first), place = first - knot * (int32_t)ORACLES_ENHANCED_EDGE_PERIOD;
    uint32_t h0 = edge_hash(seed, id, (uint32_t)knot), h1 = edge_hash(seed, id, (uint32_t)knot + 1u);
    for (unsigned i = 0; i < count; i++) {
        out[i] = edge_between(h0, h1, place);
        if (++place == (int32_t)ORACLES_ENHANCED_EDGE_PERIOD) {
            place = 0;
            knot++;
            h0 = h1;
            h1 = edge_hash(seed, id, (uint32_t)knot + 1u);
        }
    }
}

/* The light in full, on the first step, on the second (256ths), and `d` pixels past the wave: in full before it, then
 * the two steps, then none. */
static const unsigned edge_steps[3] = { 256u, 169u, 102u };
static unsigned edge_step(int d) { return d < 0 ? edge_steps[0] : d < EDGE_STEP ? edge_steps[1] : d < 2 * EDGE_STEP ? edge_steps[2] : 0u; }

/* Round a corner: `a` and `b` the pixels past the waves of its two sides' lines, the distance from the corner of the
 * room in clear grown by them. */
static unsigned edge_turn(int a, int b)
{
    if (a < 0 || b < 0) return edge_step(a < 0 ? b : a);
    const int d2 = a * a + b * b;
    return d2 < EDGE_STEP * EDGE_STEP ? edge_steps[1] : d2 < 4 * EDGE_STEP * EDGE_STEP ? edge_steps[2] : 0u;
}

/* `light` and the edge's beside a side, `across` pixels past its wave, and round its corner, `along` past the wave of
 * the other side's line, the more of them. */
static unsigned edge_end(unsigned light, unsigned side, unsigned corner, int across, int along)
{
    if (side) { const unsigned l = edge_step(across) * side >> 8; if (l > light) light = l; }
    if (corner) { const unsigned l = edge_turn(across, along) * corner >> 8; if (l > light) light = l; }
    return light;
}

static unsigned source_width(const OraclesEnhancedNeighbour *nb) { return nb->width ? nb->width : CORE_W; }
static unsigned source_height(const OraclesEnhancedNeighbour *nb) { return nb->height ? nb->height : AREA_H; }

/* The light of what the compose draws at the world pixel (x, y): the core's window, else the first source there, 256
 * less its veil; 0 where nothing is drawn, or something kept dark. */
static unsigned clear_at(const OraclesEnhancedCompose *in, int32_t x, int32_t y)
{
    if (x >= in->window_world_left && x < in->window_world_left + (int32_t)CORE_W && y >= in->window_world_top && y < in->window_world_top + (int32_t)AREA_H)
        return 256u;
    for (unsigned n = 0; n < in->neighbour_count; n++) {
        const OraclesEnhancedNeighbour *nb = &in->neighbours[n];
        if (!nb->game_area && nb->veil < 256u) continue;
        if (x < nb->world_left || x >= nb->world_left + (int32_t)source_width(nb) || y < nb->world_top || y >= nb->world_top + (int32_t)source_height(nb)) continue;
        return nb->veil >= 256u ? 0u : 256u - nb->veil;
    }
    return 0u;
}

/* The light of what is drawn beside a side, the least of it at the side's middle and at the edge's depth from either
 * end: a side along something in clear in part only (the game's window half across it in a scroll) takes none.  `x`,
 * `y` the point beside its first pixel, (dx, dy) the way along it, `length` its pixels. */
static unsigned clear_along(const OraclesEnhancedCompose *in, int32_t x, int32_t y, int32_t dx, int32_t dy, int32_t length)
{
    const int32_t at[3] = { EDGE_DEPTH, length / 2, length - 1 - EDGE_DEPTH };
    unsigned light = 256u;
    for (unsigned i = 0; i < 3u && light; i++) {
        const unsigned l = clear_at(in, x + dx * at[i], y + dy * at[i]);
        if (l < light) light = l;
    }
    return light;
}

/* How far a line's own scroll moves the sources from where they stand (a ripple under water), the most. */
static int32_t shift_reach(const OraclesEnhancedCompose *in)
{
    int32_t reach = 0;
    for (unsigned i = 0; i < AREA_H; i++) {
        const int32_t ax = in->line_shift ? in->line_shift[i] - in->sway : 0, ay = in->line_shift_y ? in->line_shift_y[i] : 0;
        if (ax > reach) reach = ax;
        if (-ax > reach) reach = -ax;
        if (ay > reach) reach = ay;
        if (-ay > reach) reach = -ay;
    }
    return reach;
}

/* The sources kept dark or fading in, in the part of the band shown, that something drawn in clear meets, a side or a
 * corner: their edges, each side's wave drawn where a side or a corner needs it, the waves taken from the set's pool
 * (a room the pool no longer holds keeps its veil alone). */
static void edge_rooms(const OraclesEnhancedCompose *in, OraclesEnhancedSize size, edge_set *set)
{
    const unsigned W = size.width, band_h = oracles_enhanced_band_height(size);
    const unsigned shown_w = in->shown_width ? in->shown_width : W, shown_h = in->shown_height ? in->shown_height : band_h;
    const int32_t reach = shift_reach(in);
    const int32_t view_left = in->world_left + (int32_t)(W - shown_w) / 2 - reach, view_top = in->world_top + (int32_t)(band_h - shown_h) / 2 - reach;
    const int32_t view_right = view_left + (int32_t)shown_w + 2 * reach, view_bottom = view_top + (int32_t)shown_h + 2 * reach;
    unsigned count = 0, used = 0;
    memset(set->of, EDGE_NONE, sizeof set->of);
    for (unsigned n = 0; n < in->neighbour_count && n < EDGE_SOURCES; n++) {
        const OraclesEnhancedNeighbour *nb = &in->neighbours[n];
        const unsigned w = source_width(nb), h = source_height(nb);
        const int32_t l = nb->world_left, t = nb->world_top, r = l + (int32_t)w, b = t + (int32_t)h;
        if (!nb->veil || !nb->game_area || nb->edge_veil >= 256u || w < 2u * ORACLES_ENHANCED_EDGE_DEPTH || h < 2u * ORACLES_ENHANCED_EDGE_DEPTH
            || r <= view_left || l >= view_right || b <= view_top || t >= view_bottom || used + 2u * (w + h) > EDGE_POOL) continue;
        edge_room *e = &set->rooms[count];
        e->side[EDGE_UP] = clear_along(in, l, t - 1, 1, 0, (int32_t)w); e->side[EDGE_RIGHT] = clear_along(in, r, t, 0, 1, (int32_t)h);
        e->side[EDGE_DOWN] = clear_along(in, l, b, 1, 0, (int32_t)w); e->side[EDGE_LEFT] = clear_along(in, l - 1, t, 0, 1, (int32_t)h);
        e->corner[0] = clear_at(in, l - 1, t - 1); e->corner[1] = clear_at(in, r, t - 1);
        e->corner[2] = clear_at(in, r, b); e->corner[3] = clear_at(in, l - 1, b);
        unsigned any = 0;
        for (unsigned s = 0; s < 4u; s++) {   /* an edge coming in (pixels just delivered) weaker */
            e->side[s] = e->side[s] * (256u - nb->edge_veil) >> 8;
            e->corner[s] = e->corner[s] * (256u - nb->edge_veil) >> 8;
            any |= e->side[s] | e->corner[s];
        }
        if (!any) continue;
        for (unsigned s = 0; s < 4u; s++) {
            const unsigned along = s == EDGE_UP || s == EDGE_DOWN ? w : h;
            e->wave[s] = set->pool + used;
            used += along;
            if (!e->side[s] && !e->corner[s] && !e->corner[(s + 1u) & 3u]) { memset(e->wave[s], 0, along); continue; }
            const int vertical = s == EDGE_RIGHT || s == EDGE_LEFT;
            const int32_t line = s == EDGE_UP ? t : s == EDGE_DOWN ? b : s == EDGE_LEFT ? l : r, first = vertical ? t : l;
            edge_wave_line(in->edge_seed, line, vertical, first, along, e->wave[s]);
        }
        set->of[n] = (uint8_t)count++;
    }
}

/* Band columns `from` to `to` of a source, each pixel `veil[x]` 256ths of the way to the band's black (256 the black
 * itself, 0 the pixel), in one pass without a branch. */
static void edge_blend(const OraclesEnhancedCompose *in, const OraclesEnhancedNeighbour *nb, const uint32_t *src, uint32_t *row,
                       int32_t from, int32_t to, int32_t offset, const uint16_t *veil, uint32_t black)
{
    if (nb->faded) for (int32_t x = from; x < to; x++) row[x] = toward(src[x + offset], black, veil[x]);
    else for (int32_t x = from; x < to; x++) row[x] = toward(faded(in, src[x + offset], in->fade), black, veil[x]);
}

/* A span of a source with its edge: each pixel's veil, the source's, less the edge's light there.  On the rows the
 * edge's top or bottom reaches, pixel by pixel; on the others, the left and right bands' steps, and the source's veil
 * alone between them.  Kept out of compose_row, whose plain spans it would otherwise weigh on. */
__attribute__((noinline)) static void edge_span(const OraclesEnhancedCompose *in, const OraclesEnhancedNeighbour *nb, const edge_room *e, uint32_t *row,
                      int32_t from, int32_t to, int32_t offset, unsigned ny, uint32_t black)
{
    const unsigned w = source_width(nb), h = source_height(nb), depth = ORACLES_ENHANCED_EDGE_DEPTH;
    const uint32_t *src = nb->game_area + (size_t)ny * w;
    uint16_t veil[ORACLES_ENHANCED_MAX_WIDTH];
    const int near = (ny < depth && (e->side[EDGE_UP] || e->corner[0] || e->corner[1]))
        || (h - 1u - ny < depth && (e->side[EDGE_DOWN] || e->corner[2] || e->corner[3]));
    if (near) {
        /* The top or the bottom band, by the wave's pixels in full at each column, and within the depth of either end,
         * the side's band and the corner's there. */
        const int up = ny < depth;
        const uint8_t *wave = e->wave[up ? EDGE_UP : EDGE_DOWN];
        const unsigned strength = e->side[up ? EDGE_UP : EDGE_DOWN], d = up ? ny : h - 1u - ny;
        const unsigned left_corner = e->corner[up ? 0 : 3], right_corner = e->corner[up ? 1 : 2];
        const int left_wave = e->wave[EDGE_LEFT][ny], right_wave = e->wave[EDGE_RIGHT][ny];
        uint16_t by_wave[ORACLES_ENHANCED_EDGE_WAVE + 1u];
        for (unsigned f = 0; f <= ORACLES_ENHANCED_EDGE_WAVE; f++) by_wave[f] = (uint16_t)((256u - (edge_step((int)d - (int)f) * strength >> 8)) * nb->veil >> 8);
        for (int32_t x = from; x < to; x++) {
            const unsigned nx = (unsigned)(x + offset);
            if (nx < depth)
                veil[x] = (uint16_t)((256u - edge_end(edge_step((int)d - wave[nx]) * strength >> 8, e->side[EDGE_LEFT], left_corner, (int)nx - left_wave, (int)d - wave[nx])) * nb->veil >> 8);
            else if (nx >= w - depth)
                veil[x] = (uint16_t)((256u - edge_end(edge_step((int)d - wave[nx]) * strength >> 8, e->side[EDGE_RIGHT], right_corner, (int)(w - 1u - nx) - right_wave, (int)d - wave[nx])) * nb->veil >> 8);
            else veil[x] = by_wave[wave[nx]];
        }
        edge_blend(in, nb, src, row, from, to, offset, veil, black);
        return;
    }
    int32_t x = from;
    if (e->side[EDGE_LEFT]) {
        const int32_t f = e->wave[EDGE_LEFT][ny], end = f + 2 * EDGE_STEP - offset < to ? f + 2 * EDGE_STEP - offset : to;
        uint16_t steps[3];
        for (unsigned i = 0; i < 3u; i++) steps[i] = (uint16_t)((256u - (edge_steps[i] * e->side[EDGE_LEFT] >> 8)) * nb->veil >> 8);
        for (int32_t c = x; c < end; c++) veil[c] = steps[c + offset < f ? 0 : c + offset < f + EDGE_STEP ? 1 : 2];
        if (x < end) { edge_blend(in, nb, src, row, x, end, offset, veil, black); x = end; }
    }
    const int32_t f = e->side[EDGE_RIGHT] ? e->wave[EDGE_RIGHT][ny] : 0, right = e->side[EDGE_RIGHT] ? (int32_t)w - f - 2 * EDGE_STEP - offset : to;
    const int32_t middle = right < to ? right : to;
    if (nb->veil >= 256u) for (; x < middle; x++) row[x] = black;
    else if (x < middle) {
        for (int32_t c = x; c < middle; c++) veil[c] = (uint16_t)nb->veil;
        edge_blend(in, nb, src, row, x, middle, offset, veil, black);
        x = middle;
    }
    if (x >= to) return;
    uint16_t steps[3];
    for (unsigned i = 0; i < 3u; i++) steps[i] = (uint16_t)((256u - (edge_steps[i] * e->side[EDGE_RIGHT] >> 8)) * nb->veil >> 8);
    for (int32_t c = x; c < to; c++) {
        const int32_t far = (int32_t)w - 1 - (c + offset);
        veil[c] = steps[far < f ? 0 : far < f + EDGE_STEP ? 1 : 2];
    }
    edge_blend(in, nb, src, row, x, to, offset, veil, black);
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
static unsigned compose_row(const OraclesEnhancedCompose *in, const edge_set *edges, const uint32_t *core, uint32_t *row, unsigned x0, unsigned x1,
                            int32_t world_y, int32_t shift_x, int32_t shift_y, uint32_t black, unsigned *sides)
{
    uint8_t painted[ORACLES_ENHANCED_MAX_WIDTH];
    memset(painted + x0, 0, x1 - x0);
    /* The band column x shows world column left + x (the shift applied). */
    const int32_t left = in->world_left + shift_x, wy = world_y + shift_y;
    for (unsigned n = in->neighbour_count; n-- > 0;) {
        const OraclesEnhancedNeighbour *nb = &in->neighbours[n];
        const unsigned width = nb->width ? nb->width : CORE_W, height = nb->height ? nb->height : AREA_H;
        const int32_t ny = wy - nb->world_top;
        if ((!nb->game_area && nb->veil < 256u) || ny < 0 || ny >= (int32_t)height) continue;
        int32_t from = nb->world_left - left, to = from + (int32_t)width;
        if (from < (int32_t)x0) from = (int32_t)x0;
        if (to > (int32_t)x1) to = (int32_t)x1;
        const int32_t offset = left - nb->world_left;   /* the source column of band column x is x + offset, in the span */
        if (from >= to) continue;
        const unsigned edge = edges && n < EDGE_SOURCES ? edges->of[n] : EDGE_NONE;
        if (edge != EDGE_NONE) edge_span(in, nb, &edges->rooms[edge], row, from, to, offset, (unsigned)ny, black);
        else if (nb->veil >= 256u) for (int32_t x = from; x < to; x++) row[x] = black;
        else {
            const uint32_t *src = nb->game_area + (size_t)ny * width;
            if (nb->faded) memcpy(row + from, src + from + offset, (size_t)(to - from) * sizeof *row);
            else for (int32_t x = from; x < to; x++) row[x] = faded(in, src[x + offset], in->fade);
            if (nb->veil) for (int32_t x = from; x < to; x++) row[x] = toward(row[x], black, nb->veil);
        }
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
        if (counted_row && (!in->counted_width || (wx >= in->counted_left && wx < in->counted_left + (int32_t)in->counted_width))) {
            uncovered++;
            if (x == x0) *sides |= 1u;
            if (x == x1 - 1u) *sides |= 2u;
        }
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

/* HUD band: the status bar, centred, gutters in the border colour (faded with the game), or the bar's own background
 * on the lines it is given; or the bar laid out across it. */
static void compose_hud(const uint32_t *core, const OraclesEnhancedCompose *in, OraclesEnhancedSize size, uint32_t *out)
{
    const unsigned W = size.width, HUD_X = oracles_enhanced_hud_x(size);
    const uint32_t gutter = gutter_pixel(in, in->border);
    for (unsigned y = 0; y < HUD; y++) {
        const uint32_t left = in->hud_fill && in->hud_fill[y] ? in->hud_fill[y] : gutter;
        const uint32_t right = in->hud_fill && in->hud_fill[HUD + y] ? in->hud_fill[HUD + y] : gutter;
        if (in->hud_place) { hud_wide_line(in->hud_place, core, y, W, left, right, out + y * W); continue; }
        for (unsigned x = 0; x < W; x++)
            out[y * W + x] = x < HUD_X ? left : x < HUD_X + CORE_W ? core[y * CORE_W + (x - HUD_X)] : right;
    }
}

/* The world row the band's row `y` shows, and the scroll its line was drawn with, by line: see compose_world. */
static int32_t row_shift(const OraclesEnhancedCompose *in, unsigned y, int32_t *shift_x, int32_t *shift_y)
{
    const int32_t world_y = in->world_top + (int32_t)y;
    const int32_t line = (int32_t)((uint32_t)(world_y - in->window_world_top) % AREA_H);   /* AREA_H a power of two: a floor modulo */
    *shift_x = in->line_shift ? in->line_shift[line] - in->sway : 0;
    *shift_y = in->line_shift_y ? in->line_shift_y[line] : 0;
    return world_y;
}

static unsigned compose_world(const uint32_t *core, const OraclesEnhancedCompose *in, OraclesEnhancedSize size, uint32_t *out)
{
    unsigned uncovered = 0;
    const unsigned W = size.width, band_h = oracles_enhanced_band_height(size);
    compose_hud(core, in, size, out);
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
    unsigned sides = 0;
    edge_set edges;
    int edged = 0;
    for (unsigned n = 0; n < in->neighbour_count && in->soft_edges && !edged; n++) edged = in->neighbours[n].veil != 0u;
    if (edged) edge_rooms(in, size, &edges);
    for (unsigned y = 0; y < band_h; y++) {
        uint32_t *row = out + (HUD + y) * W;
        if (y < y0 || y >= y0 + shown_h) { for (unsigned x = 0; x < W; x++) row[x] = black; continue; }
        int32_t shift_x, shift_y;
        const int32_t world_y = row_shift(in, y, &shift_x, &shift_y);
        for (unsigned x = 0; x < x0; x++) row[x] = black;
        for (unsigned x = x0 + shown_w; x < W; x++) row[x] = black;
        unsigned row_sides = 0;
        const unsigned row_uncovered = compose_row(in, edged ? &edges : NULL, core, row, x0, x0 + shown_w, world_y, shift_x, shift_y, black, &row_sides);
        uncovered += row_uncovered;
        sides |= row_sides;
        if (row_uncovered && y == y0) sides |= 4u;
        if (row_uncovered && y == y0 + shown_h - 1u) sides |= 8u;
    }
    if (in->uncovered_edges) *in->uncovered_edges = sides;
    return uncovered;
}

unsigned oracles_enhanced_dark_lit(const OraclesEnhancedCompose *in, const uint32_t *out)
{
    if (in->mode != ORACLES_ENHANCED_WORLD) return 0;
    const OraclesEnhancedSize size = in->size.width ? in->size : oracles_enhanced_size(0);
    const unsigned W = size.width, band_h = oracles_enhanced_band_height(size);
    const unsigned shown_w = in->shown_width ? in->shown_width : W, shown_h = in->shown_height ? in->shown_height : band_h;
    const unsigned x0 = (W - shown_w) / 2u, y0 = (band_h - shown_h) / 2u;
    const uint32_t black = gutter_pixel(in, 0xff000000u);
    const int32_t depth = EDGE_DEPTH;
    unsigned lit = 0;
    for (unsigned n = 0; n < in->neighbour_count; n++) {
        const OraclesEnhancedNeighbour *nb = &in->neighbours[n];
        if (nb->veil < 256u) continue;
        const int32_t w = (int32_t)source_width(nb), h = (int32_t)source_height(nb);
        /* The sources before it that reach inside its edge: they win there. */
        unsigned over[EDGE_SOURCES], overs = 0;
        for (unsigned k = 0; k < n && overs < EDGE_SOURCES; k++) {
            const OraclesEnhancedNeighbour *o = &in->neighbours[k];
            if ((!o->game_area && o->veil < 256u) || o->world_left >= nb->world_left + w - depth || o->world_left + (int32_t)source_width(o) <= nb->world_left + depth
                || o->world_top >= nb->world_top + h - depth || o->world_top + (int32_t)source_height(o) <= nb->world_top + depth) continue;
            over[overs++] = k;
        }
        int found = 0;
        for (unsigned y = y0; y < y0 + shown_h && !found; y++) {
            int32_t shift_x, shift_y;
            const int32_t world_y = row_shift(in, y, &shift_x, &shift_y), left = in->world_left + shift_x, wy = world_y + shift_y;
            const int32_t ny = wy - nb->world_top;
            if (ny < depth || ny >= h - depth) continue;
            /* The whole row at the depth from the top and the bottom and in the middle, else three columns: at the
             * depth from the left and the right, and in the middle. */
            const int whole = ny == depth || ny == h - 1 - depth || ny == h / 2;
            const int32_t line = world_y - in->window_world_top, columns[3] = { depth, w / 2, w - 1 - depth }, count = whole ? w - 2 * depth : 3;
            for (int32_t i = 0; i < count && !found; i++) {
                const int32_t nx = whole ? depth + i : columns[i];
                const int32_t x = nb->world_left + nx - left;
                if (x < (int32_t)x0 || x >= (int32_t)(x0 + shown_w)) continue;
                if (line >= 0 && line < (int32_t)AREA_H && x >= in->window_world_left - in->world_left && x < in->window_world_left - in->world_left + (int32_t)CORE_W) continue;
                int covered = 0;
                for (unsigned i = 0; i < overs && !covered; i++) {
                    const OraclesEnhancedNeighbour *o = &in->neighbours[over[i]];
                    covered = left + x >= o->world_left && left + x < o->world_left + (int32_t)source_width(o) && wy >= o->world_top && wy < o->world_top + (int32_t)source_height(o);
                }
                found = !covered && out[(HUD + y) * W + (unsigned)x] != black;
            }
        }
        lit += (unsigned)found;
    }
    return lit;
}

unsigned oracles_enhanced_compose(const uint32_t *core, const OraclesEnhancedCompose *in, uint32_t *out)
{
    const OraclesEnhancedSize size = in->size.width ? in->size : oracles_enhanced_size(0);
    if (in->mode == ORACLES_ENHANCED_FRAMED) { compose_framed(core, in, size, out); return 0; }
    return compose_world(core, in, size, out);
}

unsigned oracles_enhanced_dark_stray(const OraclesEnhancedCompose *in, const uint32_t *out)
{
    if (in->mode != ORACLES_ENHANCED_WORLD) return 0;
    const OraclesEnhancedSize size = in->size.width ? in->size : oracles_enhanced_size(0);
    const unsigned W = size.width, band_h = oracles_enhanced_band_height(size);
    const unsigned shown_w = in->shown_width ? in->shown_width : W, shown_h = in->shown_height ? in->shown_height : band_h;
    const unsigned x0 = (W - shown_w) / 2u, y0 = (band_h - shown_h) / 2u;
    const uint32_t black = gutter_pixel(in, 0xff000000u);
    const int32_t depth = EDGE_DEPTH;
    unsigned stray = 0;
    for (unsigned n = 0; n < in->neighbour_count; n++) {
        const OraclesEnhancedNeighbour *nb = &in->neighbours[n];
        if (nb->veil < 256u) continue;
        const int32_t w = (int32_t)source_width(nb), h = (int32_t)source_height(nb), l = nb->world_left, t = nb->world_top;
        int found = 0;
        for (unsigned y = y0; y < y0 + shown_h && !found; y++) {
            int32_t shift_x, shift_y;
            const int32_t world_y = row_shift(in, y, &shift_x, &shift_y), left = in->world_left + shift_x, wy = world_y + shift_y;
            const int32_t ny = wy - t, line = world_y - in->window_world_top;
            if (ny < 0 || ny >= h) continue;
            const int across = ny == 0 || ny == h - 1;   /* the top or the bottom side, else the left and the right */
            if (!across && (ny < depth || ny >= h - depth)) continue;
            const int32_t count = across ? w - 2 * depth : 2;
            for (int32_t i = 0; i < count && !found; i++) {
                const int32_t nx = across ? depth + i : i ? w - 1 : 0;
                const int32_t bx = across ? l + nx : i ? l + w : l - 1, by = across ? (ny ? t + h : t - 1) : wy;
                if (clear_at(in, bx, by)) continue;
                const int32_t x = l + nx - left;
                if (x < (int32_t)x0 || x >= (int32_t)(x0 + shown_w)) continue;
                if (line >= 0 && line < (int32_t)AREA_H && x >= in->window_world_left - in->world_left && x < in->window_world_left - in->world_left + (int32_t)CORE_W) continue;
                int covered = 0;
                for (unsigned k = 0; k < n && !covered; k++) {
                    const OraclesEnhancedNeighbour *o = &in->neighbours[k];
                    covered = (o->game_area || o->veil >= 256u) && left + x >= o->world_left && left + x < o->world_left + (int32_t)source_width(o)
                        && wy >= o->world_top && wy < o->world_top + (int32_t)source_height(o);
                }
                found = !covered && out[(HUD + y) * W + (unsigned)x] != black;
            }
        }
        stray += (unsigned)found;
    }
    return stray;
}

void oracles_enhanced_compose_hud(const uint32_t *core, const OraclesEnhancedCompose *in, uint32_t *out)
{
    compose_hud(core, in, in->size.width ? in->size : oracles_enhanced_size(0), out);
}

void oracles_enhanced_far_fitting_words(unsigned screen_w, unsigned screen_h, unsigned margin_lines, char *out, size_t capacity)
{
    const OraclesEnhancedFitting f = oracles_enhanced_far_fitting_band(screen_w, screen_h, margin_lines);
    const OraclesEnhancedSize far = oracles_enhanced_view_size(ORACLES_ENHANCED_FAR, f.aspect);
    char place[64];
    if (margin_lines && margin_lines < screen_h) snprintf(place, sizeof place, "%ux%u band the margin leaves on the %ux%u screen", screen_w, screen_h - margin_lines, screen_w, screen_h);
    else snprintf(place, sizeof place, "%ux%u screen", screen_w, screen_h);
    if (f.fit == ORACLES_ENHANCED_FIT_FITTED)
        snprintf(out, capacity, "the far view fitted to the %s: %ux%u at %ux, where %ux%u covers %u%% of it at %ux",
                 place, f.size.width, f.size.height, f.scale, far.width, far.height, f.far_percent, f.far_scale);
    else if (f.fit == ORACLES_ENHANCED_FIT_FILLS)
        snprintf(out, capacity, "the far view at %ux%u: it covers %u%% of the %s at %ux", f.size.width, f.size.height, f.far_percent, place, f.far_scale);
    else
        snprintf(out, capacity, "the far view at %ux%u: the %s at %ux gives %ux%u, %s", f.size.width, f.size.height, place, f.scale,
                 f.computed.width, f.computed.height, f.fit == ORACLES_ENHANCED_FIT_SMALL ? "under 400x224 or taller than wide" : "not a size the view takes");
}
