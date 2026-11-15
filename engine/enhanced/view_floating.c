/* The status bar floating over the world (hud=floating): its blocks, the core's own pixels, each in a translucent panel
 * with rounded corners over the world band, which takes the whole surface (floating.h places them).  The bar's
 * background is left out, a pixel of its most common colour where no sprite stands over it, so that the world shows
 * through; the panel takes that colour, its edge a little darker, or a light grey without an edge, at
 * ORACLES_FLOATING_ALPHA.  With the item hotkeys, B, A and the hotkeys' items stand in tiles as a pad has them, each tile
 * the bar's picture of its item, its key's badge in a gutter beside it.
 * While a text box shows, the items fade out, and a panel at the top the box meets goes at once.  The area's blurb
 * (ev_overlay_area_blurb) stands in front of them: their pixels over one of its own are left out, the band's shown.
 * With the sub-pixel camera the panels are a layer apart, which the presenter lays over the band unmoved.  In the near
 * view, where the
 * screen shows it twice or more, they are all there at half the game's scale: laid out on a surface twice as large, in a
 * layer of that size, which the presenter draws at a whole number of the screen's pixels a pixel, each panel kept to
 * its corner. */
#include "view_internal.h"

#include "floating.h"
#include "hotbar_font.h"

#include <stdlib.h>
#include <string.h>

#define FLOATING_FADE 8u   /* frames a panel takes to fade out or in */
_Static_assert(ORACLES_FLOATING_RIGHT == ORACLES_ENHANCED_FLOATING_RIGHT && ORACLES_FLOATING_BOTTOM == ORACLES_ENHANCED_FLOATING_BOTTOM
                   && ORACLES_FLOATING_CENTRE == ORACLES_ENHANCED_FLOATING_CENTRE, "a panel's corner, as the layer hands it");

static uint32_t mix(uint32_t under, uint32_t over, unsigned alpha)
{
    uint32_t out = 0xff000000u;
    for (unsigned shift = 0; shift < 24u; shift += 8u) {
        const unsigned a = (under >> shift) & 0xffu, b = (over >> shift) & 0xffu;
        out |= ((a * (256u - alpha) + b * alpha) >> 8) << shift;
    }
    return out;
}

static uint32_t darker(uint32_t colour)
{
    uint32_t out = 0xff000000u;
    for (unsigned shift = 0; shift < 24u; shift += 8u) out |= ((((colour >> shift) & 0xffu) * 5u) / 8u) << shift;
    return out;
}

static uint32_t core_colour(OraclesEnhancedView *v, uint16_t rgb555) { return oracles_core_convert_rgb555(v->core, rgb555); }

/* The bar's background: the colour most of its pixels have, as its tiles draw them (the game's own beige, whatever its
 * index in the palette). */
static uint16_t bar_background(const OraclesEnhancedBar *bar)
{
    uint16_t seen[16];
    unsigned count[16], kinds = 0;
    for (unsigned y = 0; y < ORACLES_ENHANCED_HUD_HEIGHT; y++)
        for (unsigned x = 0; x < ORACLES_ENHANCED_CORE_WIDTH; x++) {
            const uint16_t colour = oracles_enhanced_bar_pixel(bar, x, y, NULL);
            unsigned k = 0;
            while (k < kinds && seen[k] != colour) k++;
            if (k == kinds && kinds < 16u) { seen[kinds] = colour; count[kinds++] = 0; }
            if (k < kinds) count[k]++;
        }
    unsigned best = 0;
    for (unsigned k = 1; k < kinds; k++) if (count[k] > count[best]) best = k;
    return kinds ? seen[best] : 0u;
}

/* A panel's fade by its block: 0 shown, FLOATING_FADE gone. */
static unsigned fade_index(const OraclesFloatingPanel *p) { return p->kind == ORACLES_FLOATING_TILE ? 4u + (p->slot < 6u ? p->slot : 0u) : (unsigned)p->kind; }

/* The area blurb drawn apart (ev_overlay_area_blurb) under a pixel of the panels at (x, y): the surface's pixel there,
 * or at half the game's scale every pixel of the surface the panel's pixel meets where the screen shows it. */
static int blurb_under(const OraclesEnhancedView *v, int32_t x, int32_t y)
{
    if (!v->blurb_cover_on) return 0;
    int32_t x0 = x, x1 = x + 1, y0 = y, y1 = y + 1;
    if (v->floating_put_half) {
        const int32_t *r = v->floating_put_rect;
        const float *at = v->floating_put_at, ux = at[2] / (float)r[2], uy = at[3] / (float)r[3];
        const float fx = at[0] + (float)(x - r[0]) * ux, fy = at[1] + (float)(y - r[1]) * uy;
        x0 = (int32_t)floorf(fx + 0.001f); x1 = (int32_t)ceilf(fx + ux - 0.001f);
        y0 = (int32_t)floorf(fy + 0.001f); y1 = (int32_t)ceilf(fy + uy - 0.001f);
    }
    for (int32_t sy = y0; sy < y1; sy++) {
        const int32_t by = sy - v->blurb_cover_y;
        if (by < 0 || by >= (int32_t)BLURB_COVER_H || !v->blurb_cover[by]) continue;
        for (int32_t sx = x0; sx < x1; sx++) {
            const int32_t bx = sx - v->blurb_cover_x;
            if (bx >= 0 && bx < (int32_t)BLURB_COVER_W && (v->blurb_cover[by] >> bx) & 1u) return 1;
        }
    }
    return 0;
}

/* The blurb's pixels the band still shows as it drew them: what the composition drew over it after (a warp's curtain)
 * is the world's, under the panels. */
static void blurb_still_shown(OraclesEnhancedView *v)
{
    if (!v->blurb_cover_on) return;
    int any = 0;
    for (uint32_t by = 0; by < BLURB_COVER_H; by++)
        for (uint32_t bx = 0; bx < BLURB_COVER_W; bx++) {
            if (!((v->blurb_cover[by] >> bx) & 1u)) continue;
            const int32_t x = v->blurb_cover_x + (int32_t)bx, y = v->blurb_cover_y + (int32_t)by;
            if (x >= 0 && y >= 0 && x < (int32_t)v->size.width && y < (int32_t)v->size.height
                && v->surface[(size_t)y * v->size.width + (size_t)x] == v->blurb_colour[by][bx]) { any = 1; continue; }
            v->blurb_cover[by] &= ~(1u << bx);
        }
    v->blurb_cover_on = any;
}

/* The panel's ground over the world: its colour at the panel's opacity, its edge darker, `alpha` of it shown. */
/* A pixel of the panels at (x, y), `alpha` of 256 of `colour` over what is there: the surface, or with the sub-pixel camera
 * the panels' own layer (premultiplied, transparent but for them), which the presenter lays over the band unmoved. */
static void put(OraclesEnhancedView *v, int32_t x, int32_t y, uint32_t colour, unsigned alpha)
{
    const uint32_t width = v->floating_layered ? v->floating_layer_w : v->size.width, height = v->floating_layered ? v->floating_layer_h : v->size.height;
    if (x < 0 || y < 0 || x >= (int32_t)width || y >= (int32_t)height || !alpha || blurb_under(v, x, y)) return;
    const size_t at = (size_t)y * width + (size_t)x;
    if (alpha > 256u) alpha = 256u;
    if (!v->floating_layered) { v->surface[at] = alpha == 256u ? colour : mix(v->surface[at], colour, alpha); return; }
    const uint32_t under = v->floating_layer[at];
    uint32_t out = 0;
    for (unsigned shift = 0; shift < 32u; shift += 8u) {
        const unsigned over = shift == 24u ? 255u : (colour >> shift) & 0xffu;
        out |= ((over * alpha + ((under >> shift) & 0xffu) * (256u - alpha)) >> 8) << shift;
    }
    v->floating_layer[at] = out;
}

/* The panel's ground over the world: its colour at the panel's opacity, its edge darker, `alpha` of it shown. */
static void ground(OraclesEnhancedView *v, const OraclesFloatingPanel *p, unsigned radius, uint32_t fill, uint32_t edge, unsigned alpha)
{
    for (int32_t y = 0; y < p->h; y++)
        for (int32_t x = 0; x < p->w; x++) {
            const unsigned kind = oracles_floating_panel_pixel((unsigned)p->w, (unsigned)p->h, radius, (unsigned)x, (unsigned)y);
            if (kind && alpha && blurb_under(v, p->x + x, p->y + y)) v->floating_put_cut++;
            if (kind) put(v, p->x + x, p->y + y, kind == 1u ? edge : fill, alpha);
        }
}

/* The bar's pixels of w x h from (bx, by) at (x, y), its background left out: a pixel of the background's colour that
 * no sprite covers (the core's image shows the bar's own colour there). */
static void bar_block(OraclesEnhancedView *v, const OraclesEnhancedBar *bar, uint16_t background, const uint32_t *core, unsigned bx, unsigned by, unsigned w,
                      unsigned h, int32_t x, int32_t y, unsigned alpha)
{
    for (unsigned row = 0; row < h; row++)
        for (unsigned col = 0; col < w; col++) {
            const unsigned cx = bx + col, cy = by + row;
            if (cx >= ORACLES_ENHANCED_CORE_WIDTH || cy >= ORACLES_ENHANCED_HUD_HEIGHT) continue;
            const uint16_t colour = oracles_enhanced_bar_pixel(bar, cx, cy, NULL);
            const uint32_t shown = core[cy * ORACLES_ENHANCED_CORE_WIDTH + cx];
            if (colour == background && shown == core_colour(v, colour)) continue;
            put(v, x + (int32_t)col, y + (int32_t)row, shown, alpha);
        }
}

/* A key's or a button's name on a badge at (x, y), in the tile's gutter: five by seven for a letter, the second of two
 * under the first; its ground at ORACLES_FLOATING_BADGE_ALPHA, `alpha` of it shown. */
static void badge(OraclesEnhancedView *v, int32_t x, int32_t y, const char *name, uint32_t ground_colour, uint32_t ink, unsigned alpha)
{
    const unsigned letters = name && name[0] ? (name[1] ? 2u : 1u) : 0u;
    if (!letters) return;
    const int32_t w = (int32_t)HOTBAR_GLYPH_WIDTH + 2, h = (int32_t)(letters * (HOTBAR_GLYPH_HEIGHT + 1u) + 1u);
    for (int32_t j = 0; j < h; j++) for (int32_t i = 0; i < w; i++) put(v, x + i, y + j, ground_colour, ORACLES_FLOATING_BADGE_ALPHA * alpha / 256u);
    for (unsigned l = 0; l < letters; l++) {
        const uint16_t glyph = hotbar_glyph(name[l]);
        for (unsigned bit = 0; bit < HOTBAR_GLYPH_WIDTH * HOTBAR_GLYPH_HEIGHT; bit++)
            if (glyph & (1u << (HOTBAR_GLYPH_WIDTH * HOTBAR_GLYPH_HEIGHT - 1u - bit)))
                put(v, x + 1 + (int32_t)(bit % HOTBAR_GLYPH_WIDTH), y + 1 + (int32_t)(l * (HOTBAR_GLYPH_HEIGHT + 1u) + bit / HOTBAR_GLYPH_WIDTH), ink, alpha);
    }
}

/* A hotkey's item in its tile, as the hotbar shows it in the game's look: its picture (or icon, or label) at (x, y), the
 * bar's background in it left out as in the bar's blocks, dimmed out of the inventory, a line under it while its request
 * waits. */
static void hotkey_item(OraclesEnhancedView *v, const OraclesHotbarSlotView *s, const OraclesHotbarPalette *p, uint16_t background, int32_t x, int32_t y,
                        uint32_t fill, unsigned alpha)
{
    const unsigned w = s->picture ? ORACLES_HOTBAR_GAME_PICTURE_WIDTH : ORACLES_HOTBAR_ICON;
    const uint16_t *pixels = s->picture ? s->picture : s->icon;
    const int32_t left = x + (s->picture ? 0 : (int32_t)(ORACLES_FLOATING_PICTURE_W - ORACLES_HOTBAR_ICON) / 2);
    if (pixels) {
        for (unsigned row = 0; row < 16u; row++)
            for (unsigned col = 0; col < w; col++) {
                const uint16_t colour = pixels[row * w + col];
                if (!(colour & ORACLES_HOTBAR_ICON_OPAQUE) || (s->picture && (colour & 0x7fffu) == background)) continue;
                const uint32_t pixel = p->pixel(p->opaque, (uint16_t)(colour & 0x7fffu));
                put(v, left + (int32_t)col, y + (int32_t)row, s->dimmed ? mix(pixel, fill, 160u) : pixel, alpha);
            }
    } else if (s->label && s->label[0]) {
        for (unsigned l = 0; l < 4u && s->label[l]; l++) {
            const uint16_t glyph = hotbar_glyph(s->label[l]);
            for (unsigned bit = 0; bit < HOTBAR_GLYPH_WIDTH * HOTBAR_GLYPH_HEIGHT; bit++)
                if (glyph & (1u << (HOTBAR_GLYPH_WIDTH * HOTBAR_GLYPH_HEIGHT - 1u - bit)))
                    put(v, x + 4 + (int32_t)(l * (HOTBAR_GLYPH_WIDTH + 1u) + bit % HOTBAR_GLYPH_WIDTH), y + 6 + (int32_t)(bit / HOTBAR_GLYPH_WIDTH),
                        s->dimmed ? p->faint : p->ink, alpha);
        }
    }
    if (s->waiting) for (int32_t i = 0; i < (int32_t)ORACLES_HOTBAR_ICON; i++) put(v, x + 4 + i, y + 15, p->ink, alpha);
}

/* The scale the panels at half the game's scale are laid out for: the screen's, or twice. */
static float screen_scale(const OraclesEnhancedView *v) { return v->floating_screen_scale > 0.0f ? v->floating_screen_scale : 2.0f; }

/* A panel meets a rectangle of the surface: at half the game's scale, where the screen shows it. */
static int meets(const OraclesEnhancedView *v, const OraclesFloatingPanel *p, const int32_t r[4])
{
    if (v->floating_scale == 1u) return p->x < r[0] + r[2] && r[0] < p->x + p->w && p->y < r[1] + r[3] && r[1] < p->y + p->h;
    const int32_t rect[4] = { p->x, p->y, p->w, p->h };
    float at[4];
    oracles_floating_surface_place(rect, p->anchor, v->floating_layer_w, v->floating_layer_h, screen_scale(v), at);
    return at[0] < (float)(r[0] + r[2]) && (float)r[0] < at[0] + at[2] && at[1] < (float)(r[1] + r[3]) && (float)r[1] < at[1] + at[3];
}

void oracles_enhanced_view_set_floating_neutral(OraclesEnhancedView *v, int neutral) { v->floating_neutral = neutral != 0; }

/* A light grey, the game's fade on it as on the bar's own colours. */
static uint32_t neutral_colour(const OraclesEnhancedView *v, unsigned level)
{
    const int c = (int)level + v->fade_effective;
    const unsigned channel = (unsigned)(c < 0 ? 0 : c > 31 ? 31 : c);
    return v->colours[(channel | channel << 5 | channel << 10) & (ORACLES_PPU_COLOURS - 1u)];
}

void oracles_enhanced_view_set_floating_avoid(OraclesEnhancedView *v, const int32_t (*rects)[4], unsigned count)
{
    v->floating_avoid_count = count < 4u ? count : 4u;
    if (v->floating_avoid_count) memcpy(v->floating_avoid, rects, v->floating_avoid_count * sizeof *rects);
}

unsigned oracles_enhanced_view_floating_panels(const OraclesEnhancedView *v, uint32_t rects[ORACLES_ENHANCED_FLOATING_PANELS][4])
{
    if (rects && v->floating_count) memcpy(rects, v->floating_rects, v->floating_count * sizeof *rects);
    return v->floating_count;
}

unsigned oracles_enhanced_view_floating_sources(const OraclesEnhancedView *v, int32_t sources[ORACLES_ENHANCED_FLOATING_PANELS][6])
{
    if (v->floating_count) memcpy(sources, v->floating_sources, v->floating_count * sizeof *sources);
    return v->floating_count;
}

unsigned oracles_enhanced_view_floating_shown(const OraclesEnhancedView *v, unsigned shown[ORACLES_ENHANCED_FLOATING_PANELS])
{
    if (v->floating_count) memcpy(shown, v->floating_shown, v->floating_count * sizeof *shown);
    return v->floating_count;
}

int oracles_enhanced_view_text_box_rect(const OraclesEnhancedView *v, int32_t rect[4]) { return ev_text_box_rect(v, rect); }

void oracles_enhanced_view_floating_counts(const OraclesEnhancedView *v, unsigned *frames, unsigned *whole, unsigned *pad, unsigned *faded)
{
    if (frames) *frames = v->floating_frames;
    if (whole) *whole = v->floating_whole_frames;
    if (pad) *pad = v->floating_pad_frames;
    if (faded) *faded = v->floating_faded_frames;
}

unsigned oracles_enhanced_view_floating_label_frames(const OraclesEnhancedView *v) { return v->floating_label_frames; }

void oracles_enhanced_view_floating_blurb_counts(const OraclesEnhancedView *v, unsigned *frames, unsigned *pixels)
{
    if (frames) *frames = v->floating_blurb_frames;
    if (pixels) *pixels = v->floating_blurb_pixels;
}

const uint32_t *oracles_enhanced_view_floating_layer(const OraclesEnhancedView *v, uint32_t rects[ORACLES_ENHANCED_FLOATING_PANELS][4], unsigned *count)
{
    *count = 0;
    if (!v->floating_layered || !v->floating_drawn_count) return NULL;
    memcpy(rects, v->floating_drawn, v->floating_drawn_count * sizeof *rects);
    *count = v->floating_drawn_count;
    return v->floating_layer;
}

unsigned oracles_enhanced_view_floating_scale(const OraclesEnhancedView *v) { return v->floating_layered ? v->floating_scale : 1u; }

unsigned oracles_enhanced_view_floating_anchors(const OraclesEnhancedView *v, uint8_t anchors[ORACLES_ENHANCED_FLOATING_PANELS])
{
    if (!v->floating_layered) return 0;
    memcpy(anchors, v->floating_drawn_anchor, v->floating_drawn_count * sizeof *anchors);
    return v->floating_drawn_count;
}

void oracles_enhanced_view_set_floating_screen(OraclesEnhancedView *v, float scale) { v->floating_screen_scale = scale > 0.0f ? scale : 0.0f; }

void oracles_enhanced_view_fold_floating_at(const OraclesEnhancedView *v, const uint32_t *surface, uint32_t *out, unsigned scale)
{
    const size_t width = v->size.width, height = v->size.height;
    const unsigned layer = oracles_enhanced_view_floating_scale(v);
    if (scale != 2u) scale = 1u;
    if (scale == 1u && out != surface) memcpy(out, surface, width * height * sizeof *out);
    if (scale == 2u)
        for (size_t y = 2u * height; y-- > 0;)
            for (size_t x = 2u * width; x-- > 0;) out[y * 2u * width + x] = surface[(y / 2u) * width + x / 2u];
    if (!v->floating_layered) return;
    const size_t out_w = width * scale, layer_w = v->floating_layer_w;
    for (unsigned i = 0; i < v->floating_drawn_count; i++) {
        const uint32_t *r = v->floating_drawn[i];
        /* The layer's pixels each one of `out`'s takes: one, four of them at half the layer's scale (their mean), or a
         * quarter of one at twice it. */
        const uint32_t x0 = r[0] * scale / layer, y0 = r[1] * scale / layer;
        const uint32_t x1 = ((r[0] + r[2]) * scale + layer - 1u) / layer, y1 = ((r[1] + r[3]) * scale + layer - 1u) / layer;
        for (uint32_t y = y0; y < y1; y++)
            for (uint32_t x = x0; x < x1; x++) {
                uint32_t p = 0;
                if (layer > scale) {
                    unsigned sum[4] = { 0, 0, 0, 0 };
                    for (unsigned k = 0; k < 4u; k++) {
                        const uint32_t q = v->floating_layer[(2u * y + k / 2u) * layer_w + 2u * x + k % 2u];
                        for (unsigned c = 0; c < 4u; c++) sum[c] += (q >> (8u * c)) & 0xffu;
                    }
                    for (unsigned c = 0; c < 4u; c++) p |= ((sum[c] + 2u) / 4u) << (8u * c);
                } else p = v->floating_layer[(size_t)(y * layer / scale) * layer_w + x * layer / scale];
                const uint32_t under = out[y * out_w + x], a = p >> 24;
                if (!a) continue;
                uint32_t mixed = 0xff000000u;
                for (unsigned shift = 0; shift < 24u; shift += 8u)
                    mixed |= ((((p >> shift) & 0xffu) * 255u + ((under >> shift) & 0xffu) * (255u - a) + 127u) / 255u) << shift;
                out[y * out_w + x] = mixed;
            }
    }
}

void oracles_enhanced_view_fold_floating(const OraclesEnhancedView *v, const uint32_t *surface, uint32_t *out) { oracles_enhanced_view_fold_floating_at(v, surface, out, 1u); }

/* The panels' layer cleared where the last frame drew them, panel by panel: the rest of it is transparent still. */
static void clear_layer(OraclesEnhancedView *v)
{
    for (unsigned i = 0; v->floating_layer && i < v->floating_drawn_count; i++) {
        const uint32_t *r = v->floating_drawn[i];
        for (uint32_t y = r[1]; y < r[1] + r[3]; y++) memset(v->floating_layer + (size_t)y * v->floating_layer_w + r[0], 0, r[2] * sizeof *v->floating_layer);
    }
    v->floating_drawn_count = 0;
}

/* A panel drawn in the layer, kept to it: what the next frame clears, the presenter uploads and draws. */
static void note_drawn(OraclesEnhancedView *v, const OraclesFloatingPanel *p)
{
    if (!v->floating_layered || v->floating_drawn_count >= ORACLES_ENHANCED_FLOATING_PANELS) return;
    const int32_t x0 = p->x < 0 ? 0 : p->x, y0 = p->y < 0 ? 0 : p->y;
    const int32_t x1 = p->x + p->w > (int32_t)v->floating_layer_w ? (int32_t)v->floating_layer_w : p->x + p->w;
    const int32_t y1 = p->y + p->h > (int32_t)v->floating_layer_h ? (int32_t)v->floating_layer_h : p->y + p->h;
    if (x1 <= x0 || y1 <= y0) return;
    v->floating_drawn_anchor[v->floating_drawn_count] = (uint8_t)p->anchor;
    uint32_t *r = v->floating_drawn[v->floating_drawn_count++];
    r[0] = (uint32_t)x0; r[1] = (uint32_t)y0; r[2] = (uint32_t)(x1 - x0); r[3] = (uint32_t)(y1 - y0);
}

/* The panels' layer cleared, and used this frame when the sub-pixel camera moves the band of a world frame: the panels
 * stand on the screen while the world goes on under them by its fraction of a pixel.  In the near view at half the
 * game's scale, in every frame they are drawn in, twice the surface's size. */
static void begin_layer(OraclesEnhancedView *v)
{
    clear_layer(v);
    v->floating_layered = 0;
    v->floating_scale = 1u;
    if (v->band_top) return;
    const unsigned scale = v->size.width <= ORACLES_ENHANCED_NARROW_WIDTH && screen_scale(v) >= 2.0f ? 2u : 1u;
    if (scale == 1u && (v->load_band || !oracles_enhanced_view_subpixel_on(v))) return;
    const uint32_t w = scale * v->size.width, h = scale * v->size.height;
    if (v->floating_layer && (v->floating_layer_w != w || v->floating_layer_h != h)) { free(v->floating_layer); v->floating_layer = NULL; }
    if (!v->floating_layer) { v->floating_layer = calloc((size_t)w * h, sizeof *v->floating_layer); v->floating_layer_w = w; v->floating_layer_h = h; }
    v->floating_layered = v->floating_layer != NULL;
    v->floating_scale = v->floating_layered ? scale : 1u;
}

/* What a composition read for its panels, kept for its text box drawn again. */
struct ev_floating_frame {
    OraclesFloatingLayout layout;
    OraclesEnhancedBar bar;
    OraclesHotbarSlotView views[ORACLES_HOTBAR_SLOTS], buttons[2];
    OraclesHotbarPalette palette;
    uint16_t background;
    uint32_t fill, edge;
    unsigned hearts_x, buttons_w;
    int pad, known;
};

/* The surface under the panels kept before they are drawn there (not in the layer), to be lifted again. */
static void keep_under(OraclesEnhancedView *v, const OraclesFloatingLayout *l)
{
    v->floating_under_kept = 0;
    if (v->floating_layered) return;
    size_t need = 0;
    for (unsigned i = 0; i < l->count; i++) need += (size_t)l->panels[i].w * (size_t)l->panels[i].h;
    if (need > v->floating_under_size) {
        uint32_t *under = realloc(v->floating_under, need * sizeof *under);
        if (!under) return;
        v->floating_under = under;
        v->floating_under_size = need;
    }
    uint32_t *out = v->floating_under;
    for (unsigned i = 0; i < l->count; i++) {
        const OraclesFloatingPanel *p = &l->panels[i];
        for (int32_t y = 0; y < p->h; y++)
            for (int32_t x = 0; x < p->w; x++, out++) {
                const int32_t sx = p->x + x, sy = p->y + y;
                *out = sx >= 0 && sy >= 0 && sx < (int32_t)v->size.width && sy < (int32_t)v->size.height ? v->surface[(size_t)sy * v->size.width + (size_t)sx] : 0u;
            }
    }
    v->floating_under_kept = 1;
}

void ev_floating_lift(OraclesEnhancedView *v)
{
    if (!v->floating_under_kept || !v->floating_frame) return;
    const OraclesFloatingLayout *l = &v->floating_frame->layout;
    const uint32_t *in = v->floating_under;
    for (unsigned i = 0; i < l->count; i++) {
        const OraclesFloatingPanel *p = &l->panels[i];
        for (int32_t y = 0; y < p->h; y++)
            for (int32_t x = 0; x < p->w; x++, in++) {
                const int32_t sx = p->x + x, sy = p->y + y;
                if (sx >= 0 && sy >= 0 && sx < (int32_t)v->size.width && sy < (int32_t)v->size.height) v->surface[(size_t)sy * v->size.width + (size_t)sx] = *in;
            }
    }
    v->floating_under_kept = 0;
}

/* The panels of `f` over the band: each block's fade a step on (`again` 0: the composition's), or as the composition left
 * it (`again`: a text box drawn again), a panel the box meets gone at once either way. */
static void draw_panels(OraclesEnhancedView *v, const struct ev_floating_frame *f, int again)
{
    const OraclesFloatingLayout *l = &f->layout;
    v->floating_count = 0;
    keep_under(v, l);
    int32_t box[4];
    const int text = ev_text_box_rect(v, box);
    int faded = 0, labelled = 0;
    v->floating_put_cut = 0;
    for (unsigned i = 0; i < l->count; i++) {
        const OraclesFloatingPanel *p = &l->panels[i];
        v->floating_put_half = v->floating_scale > 1u;
        if (v->floating_put_half) {
            const int32_t rect[4] = { p->x, p->y, p->w, p->h };
            memcpy(v->floating_put_rect, rect, sizeof rect);
            oracles_floating_surface_place(rect, p->anchor, v->floating_layer_w, v->floating_layer_h, screen_scale(v), v->floating_put_at);
        }
        /* Under a text box the items fade out, and back in after it; a panel the box meets goes at once, never over it. */
        const int met = text && meets(v, p, box), hidden = met || (text && !p->top);
        const unsigned index = fade_index(p);
        uint8_t *fade = &v->floating_fade[index];
        if (again) *fade = met ? FLOATING_FADE : v->floating_fade_kept[index];
        else if (met) *fade = FLOATING_FADE;
        else if (hidden && *fade < FLOATING_FADE) (*fade)++;
        else if (!hidden && *fade) (*fade)--;
        faded |= *fade != 0u;
        const unsigned shown = (FLOATING_FADE - *fade) * 256u / FLOATING_FADE;
        v->floating_rects[v->floating_count][0] = (uint32_t)p->x; v->floating_rects[v->floating_count][1] = (uint32_t)p->y;
        v->floating_rects[v->floating_count][2] = (uint32_t)p->w; v->floating_rects[v->floating_count][3] = (uint32_t)p->h;
        int32_t *source = v->floating_sources[v->floating_count];
        memset(source, 0, 6u * sizeof *source);
        v->floating_shown[v->floating_count] = shown;
        v->floating_count++;
        if (!shown) continue;
        note_drawn(v, p);
        ground(v, p, l->radius, f->fill, f->edge, ORACLES_FLOATING_ALPHA * shown / 256u);
        const int32_t cx = p->x + (int32_t)l->pad, cy = p->y + (int32_t)l->pad;
        switch (p->kind) {
        case ORACLES_FLOATING_HEARTS:
            source[0] = (int32_t)f->hearts_x; source[2] = (int32_t)(8u * l->hearts_cols); source[3] = (int32_t)(8u * l->hearts_rows);
            for (unsigned k = 0; k < l->hearts_cols * l->hearts_rows; k++) {
                const unsigned col = k % l->hearts_cols, row = k / l->hearts_cols;
                bar_block(v, &f->bar, f->background, oracles_core_pixels(v->core), f->hearts_x + col * 8u, row * 8u, 8u, 8u, cx + (int32_t)(col * 8u), cy + (int32_t)(row * 8u), shown);
            }
            break;
        case ORACLES_FLOATING_MONEY:
            source[0] = (int32_t)f->buttons_w; source[2] = (int32_t)ORACLES_ENHANCED_BAR_MONEY; source[3] = 16;
            bar_block(v, &f->bar, f->background, oracles_core_pixels(v->core), f->buttons_w, 0, ORACLES_ENHANCED_BAR_MONEY, 16u, cx, cy, shown);
            break;
        case ORACLES_FLOATING_BUTTONS:
            source[2] = (int32_t)f->buttons_w; source[3] = 16;
            bar_block(v, &f->bar, f->background, oracles_core_pixels(v->core), 0, 0, f->buttons_w, 16u, cx, cy, shown);
            break;
        case ORACLES_FLOATING_BAR:
            source[2] = (int32_t)ORACLES_ENHANCED_CORE_WIDTH; source[3] = 16;
            bar_block(v, &f->bar, f->background, oracles_core_pixels(v->core), 0, 0, ORACLES_ENHANCED_CORE_WIDTH, 16u, cx, cy, shown);
            break;
        case ORACLES_FLOATING_TILE: {
            const int32_t px = p->x + (int32_t)ORACLES_FLOATING_PICTURE_X, py = p->y + (p->h - 16) / 2;
            const int32_t kx = p->x + (int32_t)ORACLES_FLOATING_BADGE_X, ky = p->y + (int32_t)ORACLES_FLOATING_BADGE_Y;
            const OraclesHotbarPalette *palette = &f->palette;
            if (p->slot >= ORACLES_FLOATING_SLOT_B) {   /* B's item or A's in the game's look, as the bar draws it */
                const OraclesHotbarSlotView *button = &f->buttons[p->slot - ORACLES_FLOATING_SLOT_B];
                hotkey_item(v, button, palette, f->background, px, py, f->fill, shown);
                labelled |= !button->picture && !button->icon && button->label && button->label[0];
                badge(v, kx, ky, p->slot == ORACLES_FLOATING_SLOT_A ? "A" : "B", palette->badge, palette->badge_ink, shown);
                break;
            }
            const OraclesHotbarSlotView *s = &f->views[p->slot];
            const uint32_t frame = s->frame == ORACLES_HOTBAR_FRAME_REFUSED ? palette->refused : s->frame == ORACLES_HOTBAR_FRAME_SOLID ? palette->ink : 0u;
            if (frame)   /* the slot's state on the panel's edge, as the hotbar frames it */
                for (int32_t y = 0; y < p->h; y++)
                    for (int32_t x = 0; x < p->w; x++)
                        if (oracles_floating_panel_pixel((unsigned)p->w, (unsigned)p->h, l->radius, (unsigned)x, (unsigned)y) == 1u) put(v, p->x + x, p->y + y, frame, shown);
            hotkey_item(v, s, palette, f->background, px, py, f->fill, shown);
            labelled |= !s->picture && !s->icon && s->label && s->label[0];
            badge(v, kx, ky, s->key, s->frame == ORACLES_HOTBAR_FRAME_DOTTED ? palette->faint : palette->ink, palette->badge_ink, shown);
            break;
        }
        }
        if (p->kind != ORACLES_FLOATING_TILE) { source[4] = cx; source[5] = cy; }
        if (shown < 256u) source[2] = 0;   /* fading: not the bar's own pixels */
    }
    v->floating_put_half = 0;
    if (f->pad) v->hotbar_drawn = 1;
    if (again) return;
    v->floating_blurb_frames += v->floating_put_cut != 0u;
    v->floating_blurb_pixels += v->floating_put_cut;
    memcpy(v->floating_fade_kept, v->floating_fade, sizeof v->floating_fade_kept);
    v->floating_frames++;
    if (!f->known) v->floating_whole_frames++;
    if (f->pad) v->floating_pad_frames++;
    if (faded) v->floating_faded_frames++;
    if (labelled) v->floating_label_frames++;
}

void ev_floating_again(OraclesEnhancedView *v)
{
    if (!v->floating_count || !v->floating_frame) return;
    if (v->floating_layered) clear_layer(v);
    draw_panels(v, v->floating_frame, 1);
}

void ev_draw_floating(OraclesEnhancedView *v)
{
    v->floating_count = 0;
    v->floating_under_kept = 0;
    begin_layer(v);
    if (v->band_top || v->framed_only) return;
    if (!v->floating_frame && !(v->floating_frame = calloc(1, sizeof *v->floating_frame))) return;
    struct ev_floating_frame *f = v->floating_frame;
    const uint32_t *core = oracles_core_pixels(v->core);
    if (!core || !ev_status_bar(v, &f->bar)) return;
    /* The bar the game cleared (a cutscene, tiles 0 throughout), or one colour (the blank between two rooms): nothing. */
    int cleared = 1, blank = 1;
    for (unsigned i = 0; i < 64u && cleared; i++) cleared = (i % 32u >= 20u) || f->bar.tiles[i] == 0u;
    for (unsigned i = 1; i < ORACLES_ENHANCED_HUD_HEIGHT * ORACLES_ENHANCED_CORE_WIDTH && blank; i++) blank = core[i] == core[0];
    if (cleared || blank) return;
    /* The layout its map signs and the image shows, or the last while either still holds it (ev_hud_place). */
    const OraclesEnhancedBarLayout map = oracles_enhanced_bar_layout(&f->bar), last = v->hud_layout;
    OraclesEnhancedBarLayout layout = ORACLES_ENHANCED_BAR_NONE;
    if (map != ORACLES_ENHANCED_BAR_NONE && ev_status_bar_shown(v)) layout = map;
    else if (last != ORACLES_ENHANCED_BAR_NONE && last == map) layout = last;
    v->hud_layout = layout;
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const uint8_t *max_health = oracles_guest_ptr(v->guest, t->link_max_health, 1u);
    if (layout == ORACLES_ENHANCED_BAR_NORMAL || layout == ORACLES_ENHANCED_BAR_TIGHT) { v->hud_tight = layout == ORACLES_ENHANCED_BAR_TIGHT; v->hud_tight_known = 1; }
    else if (layout == ORACLES_ENHANCED_BAR_BIGGORON && max_health) { v->hud_tight = *max_health >= 14u * 4u + 1u; v->hud_tight_known = 1; }
    OraclesFloatingInput in;
    memset(&in, 0, sizeof in);
    const unsigned scale = v->floating_scale;   /* at half the game's scale, laid out on a surface twice as large */
    in.width = scale * v->size.width; in.height = scale * v->size.height;
    in.near_view = v->size.width <= ORACLES_ENHANCED_NARROW_WIDTH && scale == 1u;
    in.known = layout != ORACLES_ENHANCED_BAR_NONE && v->hud_tight_known;
    in.buttons = v->hud_tight ? ORACLES_ENHANCED_BAR_BUTTONS_TIGHT : ORACLES_ENHANCED_BAR_BUTTONS;
    in.per_row = v->hud_tight ? 8u : 7u;
    in.hearts = max_health ? (*max_health + 3u) / 4u : 0u;
    in.pad = v->hotbar_shown;
    in.keyboard = v->hotbar.keyboard;
    in.avoid_count = v->floating_avoid_count;
    /* At half the game's scale, the touch controls' rectangles as far from the screen's right edge in the panels' pixels
     * as they stand in the screen's: the money kept left of the pause there. */
    const float across = scale == 1u ? 1.0f : screen_scale(v) / oracles_floating_screen_unit((float)v->size.width * screen_scale(v), in.width);
    for (unsigned i = 0; i < ORACLES_FLOATING_AVOID; i++) {
        const int32_t *a = v->floating_avoid[i];
        int32_t *out = in.avoid[i];
        out[0] = (int32_t)in.width - (int32_t)ceilf((float)((int32_t)v->size.width - a[0]) * across);
        out[2] = (int32_t)ceilf((float)a[2] * across);
        out[1] = a[1] * (int32_t)scale;
        out[3] = a[3] * (int32_t)scale;
    }
    oracles_floating_layout(&in, &f->layout);
    /* The hotkeys' tiles: their items as the hotbar has them; none to show, B and A alone. */
    f->pad = 0;
    for (unsigned i = 0; i < f->layout.count && !f->pad; i++) f->pad = f->layout.panels[i].kind == ORACLES_FLOATING_TILE;
    if (f->pad && !ev_hotbar_views(v, ORACLES_HOTBAR_GAME, f->views, f->buttons, &f->palette)) {
        in.pad = 0;
        oracles_floating_layout(&in, &f->layout);
        f->pad = 0;
    }
    f->known = in.known;
    f->buttons_w = in.buttons;
    f->background = bar_background(&f->bar);
    f->fill = v->floating_neutral ? neutral_colour(v, 26u) : core_colour(v, f->background);
    f->edge = v->floating_neutral ? f->fill : darker(f->fill);
    f->hearts_x = ORACLES_ENHANCED_CORE_WIDTH - (v->hud_tight ? 64u : 56u);
    blurb_still_shown(v);
    draw_panels(v, f, 0);
}
