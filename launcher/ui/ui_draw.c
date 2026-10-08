#include "ui_draw.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* An atlas per font and size drawn at the raster scale, which only a change of scale empties: the home screen, the
 * pages, Controls and the pause menu use 26 of them, and the pause prepares its own on top of the home screen's when
 * both are at one scale (a fullscreen home screen and game).  oracles-launcher-atlases prepares every style at once. */
#define MAX_ATLASES 48
#define ATLAS_MAX_SIDE 4096
#define ATLAS_PADDING 1
#define MAX_TEXT_GLYPHS 128
/* A new scale is rasterised once it has held this long (a window being resized sends many sizes). */
#define RASTER_SETTLE_MS 150.0
/* Points per rounded corner, and per straight side between two corners, the same for every loop of a shape so
 * that loops pair up.  A side is cut in pieces: SDL's software renderer places the two long slivers of a whole
 * side's quad half a pixel apart. */
#define CORNER_POINTS 8
#define SIDE_POINTS 12
#define LOOP_POINTS (4 * (CORNER_POINTS + SIDE_POINTS))
#define LETTERBOX 0x050507u
#define SCENE_BACKGROUND 0x0c0b0fu

typedef struct glyph_slot {
    int16_t x, y, w, h, x_offset, y_offset;
    uint8_t ready;
} glyph_slot;

typedef struct atlas {
    OraclesUiFont font;
    float size;             /* CSS px */
    float pixel_size;       /* rasterised at, px per em */
    unsigned char *coverage;
    int width, height;
    int pen_x, pen_y, row_height;
    glyph_slot *slots;      /* by glyph index */
    SDL_Texture *texture;
    int dirty;
    unsigned rasterised;    /* glyphs rendered into it */
} atlas;

struct OraclesUiDraw {
    SDL_Renderer *renderer;
    float scene_width, motif_x;          /* the layout's (ui_layout.h) */
    float scale, origin_x, origin_y;    /* the scene's placement in the output */
    float raster_scale;                  /* the scale that has held: the atlases' and, once redone, the motifs'; 0: none yet */
    float wanted_scale;
    double wanted_since_ms;
    atlas atlases[MAX_ATLASES];
    int atlas_count;
    SDL_Texture *motifs[ORACLES_UI_MOTIF_COUNT];
    int motif_width[ORACLES_UI_MOTIF_COUNT];
    float motif_scale[ORACLES_UI_MOTIF_COUNT];   /* each motif's own: a motif is redone when it is shown or when the launcher is idle */
    float motif_left[ORACLES_UI_MOTIF_COUNT];    /* the motif's column at the texture's left, the scene's left edge: -motif_x */
    int max_texture_side;
    int frozen;                          /* the rasters stay at their scale (a game's pause): stretched, never redone */
    int out_width, out_height;           /* the output of the frame begun */
    int overflow;                        /* the scene is larger than the output (a minimum scale): its parts are anchored */
    unsigned glyphs_rasterised;          /* since the drawing began, for the pause to say none came during a session */
};

OraclesUiColor oracles_ui_rgb(uint32_t rgb)
{
    const OraclesUiColor color = { (uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb, 255 };
    return color;
}

OraclesUiColor oracles_ui_rgba(uint32_t rgb, float opacity) { return oracles_ui_fade(oracles_ui_rgb(rgb), opacity); }

OraclesUiColor oracles_ui_fade(OraclesUiColor color, float opacity)
{
    if (opacity < 0.0f) opacity = 0.0f;
    if (opacity > 1.0f) opacity = 1.0f;
    color.a = (uint8_t)lroundf((float)color.a * opacity);
    return color;
}

OraclesUiColor oracles_ui_mix(OraclesUiColor from, OraclesUiColor to, float t)
{
    OraclesUiColor out;
    out.r = (uint8_t)lroundf((float)from.r + ((float)to.r - (float)from.r) * t);
    out.g = (uint8_t)lroundf((float)from.g + ((float)to.g - (float)from.g) * t);
    out.b = (uint8_t)lroundf((float)from.b + ((float)to.b - (float)from.b) * t);
    out.a = (uint8_t)lroundf((float)from.a + ((float)to.a - (float)from.a) * t);
    return out;
}

OraclesUiDraw *oracles_ui_draw_create(SDL_Renderer *renderer)
{
    if (!oracles_ui_fonts_load()) return NULL;
    OraclesUiDraw *draw = calloc(1, sizeof *draw);
    if (!draw) return NULL;
    draw->renderer = renderer;
    draw->scene_width = oracles_ui_scene_width(ORACLES_UI_LAYOUT_16_9);
    draw->motif_x = oracles_ui_motif_x(ORACLES_UI_LAYOUT_16_9);
    const Sint64 side = SDL_GetNumberProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 0);
    draw->max_texture_side = side > 0 && side <= INT_MAX ? (int)side : 16384;
    return draw;
}

static void release_atlases(OraclesUiDraw *draw)
{
    for (int i = 0; i < draw->atlas_count; i++) {
        atlas *a = &draw->atlases[i];
        draw->glyphs_rasterised += a->rasterised;
        if (a->texture) SDL_DestroyTexture(a->texture);
        free(a->coverage);
        free(a->slots);
    }
    memset(draw->atlases, 0, sizeof draw->atlases);
    draw->atlas_count = 0;
}

void oracles_ui_draw_destroy(OraclesUiDraw *draw)
{
    if (!draw) return;
    release_atlases(draw);
    for (int i = 0; i < ORACLES_UI_MOTIF_COUNT; i++)
        if (draw->motifs[i]) SDL_DestroyTexture(draw->motifs[i]);
    free(draw);
}

/* ---- motifs ---------------------------------------------------------------- */

/* A motif at the scale that has held, its columns from the scene's left edge rasterised up to the slide's reach within
 * the renderer's largest texture (about a tenth of a second at 1080p: NanoSVG's anti-aliasing covers every pixel). */
static int raster_motif(OraclesUiDraw *draw, OraclesUiMotif motif)
{
    const float scale = draw->raster_scale, left = -draw->motif_x;
    float right = left + draw->scene_width + ORACLES_UI_MOTIF_SLIDE;
    if ((right - left) * scale > (float)draw->max_texture_side) right = left + (float)draw->max_texture_side / scale;
    unsigned char *rgba = NULL;
    int width = 0, height = 0;
    if (!oracles_ui_motif_raster(motif, scale, left, right, &rgba, &width, &height)) return 0;
    SDL_Texture *texture = SDL_CreateTexture(draw->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    const int ok = texture && SDL_UpdateTexture(texture, NULL, rgba, width * 4);
    free(rgba);
    if (!ok) { if (texture) SDL_DestroyTexture(texture); return 0; }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    if (draw->motifs[motif]) SDL_DestroyTexture(draw->motifs[motif]);
    draw->motifs[motif] = texture;
    draw->motif_width[motif] = width;
    draw->motif_scale[motif] = scale;
    draw->motif_left[motif] = left;
    return 1;
}

/* Whether the motif's raster is for the scale that has held and the layout's place. */
static int motif_current(const OraclesUiDraw *draw, int motif)
{
    return draw->motif_scale[motif] == draw->raster_scale && draw->motif_left[motif] == -draw->motif_x;
}

int oracles_ui_draw_idle(OraclesUiDraw *draw)
{
    if (draw->raster_scale <= 0.0f) return 0;
    for (int i = 0; i < ORACLES_UI_MOTIF_COUNT; i++) {
        if (motif_current(draw, i)) continue;
        if (!raster_motif(draw, (OraclesUiMotif)i)) {   /* not again: the drawing stays without it */
            draw->motif_scale[i] = draw->raster_scale;
            draw->motif_left[i] = -draw->motif_x;
        }
        break;
    }
    for (int i = 0; i < ORACLES_UI_MOTIF_COUNT; i++)
        if (!motif_current(draw, i)) return 1;
    return 0;
}

void oracles_ui_draw_motif(OraclesUiDraw *draw, OraclesUiMotif motif, float shift, float opacity)
{
    if (opacity <= 0.0f || draw->raster_scale <= 0.0f) return;
    /* Shown before the idle time has redone it: now, unless an older raster can stand in while the scale settles; a
     * raster for the other layout's place, now. */
    if (!draw->motifs[motif] || draw->motif_left[motif] != -draw->motif_x
        || (draw->motif_scale[motif] != draw->raster_scale && draw->raster_scale == draw->wanted_scale))
        raster_motif(draw, motif);
    SDL_Texture *texture = draw->motifs[motif];
    if (!texture) return;
    const float k = draw->scale / draw->motif_scale[motif];
    const float height = ceilf(ORACLES_UI_SCENE_HEIGHT * draw->motif_scale[motif]);
    const SDL_FRect destination = { draw->origin_x + shift * draw->scale, draw->origin_y, (float)draw->motif_width[motif] * k, height * k };
    SDL_SetTextureAlphaMod(texture, (Uint8)lroundf(255.0f * (opacity > 1.0f ? 1.0f : opacity)));
    SDL_RenderTexture(draw->renderer, texture, NULL, &destination);
}

/* ---- frame ------------------------------------------------------------------- */

void oracles_ui_draw_layout(OraclesUiDraw *draw, OraclesUiLayout layout)
{
    draw->scene_width = oracles_ui_scene_width(layout);
    draw->motif_x = oracles_ui_motif_x(layout);
}

static float scale_of(const OraclesUiDraw *draw, int width, int height)
{
    return fminf((float)width / draw->scene_width, (float)height / ORACLES_UI_SCENE_HEIGHT);
}

/* The scale that fits the output, raised to `min_scale` when it is under it. */
static float scale_at_least(const OraclesUiDraw *draw, int width, int height, float min_scale)
{
    const float fit = scale_of(draw, width, height);
    return fit < min_scale ? min_scale : fit;
}

/* The scene placed in the output, the rasters redone once a new scale has held (unless frozen); its whole-pixel box. */
static SDL_Rect place(OraclesUiDraw *draw, float scale, int width, int height, double now_ms)
{
    draw->scale = scale;
    draw->out_width = width;
    draw->out_height = height;
    draw->overflow = 0;
    draw->origin_x = ((float)width - draw->scene_width * scale) * 0.5f;
    draw->origin_y = ((float)height - ORACLES_UI_SCENE_HEIGHT * scale) * 0.5f;
    if (scale != draw->wanted_scale) { draw->wanted_scale = scale; draw->wanted_since_ms = now_ms; }
    if (!draw->frozen && draw->raster_scale != scale && (draw->raster_scale == 0.0f || now_ms - draw->wanted_since_ms >= RASTER_SETTLE_MS)) {
        release_atlases(draw);
        draw->raster_scale = scale;
    }
    SDL_SetRenderClipRect(draw->renderer, NULL);
    SDL_SetRenderDrawBlendMode(draw->renderer, SDL_BLENDMODE_BLEND);
    const SDL_Rect scene = { (int)lroundf(draw->origin_x), (int)lroundf(draw->origin_y),
                             (int)lroundf(draw->scene_width * scale), (int)lroundf(ORACLES_UI_SCENE_HEIGHT * scale) };
    return scene;
}

int oracles_ui_draw_begin(OraclesUiDraw *draw, int width, int height, double now_ms)
{
    const float scale = scale_of(draw, width, height);
    if (scale <= 0.0f) return 1;
    const SDL_Rect scene = place(draw, scale, width, height, now_ms);
    const OraclesUiColor letterbox = oracles_ui_rgb(LETTERBOX);
    SDL_SetRenderDrawColor(draw->renderer, letterbox.r, letterbox.g, letterbox.b, 255);
    SDL_RenderClear(draw->renderer);
    /* The scene's edges on whole pixels: its clip and its background. */
    const OraclesUiColor background = oracles_ui_rgb(SCENE_BACKGROUND);
    SDL_SetRenderDrawColor(draw->renderer, background.r, background.g, background.b, 255);
    SDL_FRect filled;
    SDL_RectToFRect(&scene, &filled);
    SDL_RenderFillRect(draw->renderer, &filled);
    SDL_SetRenderClipRect(draw->renderer, &scene);
    return 1;
}

int oracles_ui_draw_begin_over(OraclesUiDraw *draw, int width, int height, float min_scale, double now_ms)
{
    const float scale = scale_at_least(draw, width, height, min_scale);
    if (scale <= 0.0f) return 1;
    const SDL_Rect scene = place(draw, scale, width, height, now_ms);
    draw->overflow = scale > scale_of(draw, width, height);
    /* A scene larger than the output shows what its anchored parts put inside the output: no clip but the output's. */
    if (!draw->overflow) SDL_SetRenderClipRect(draw->renderer, &scene);
    return 1;
}

int oracles_ui_draw_overflows(const OraclesUiDraw *draw) { return draw->overflow; }

void oracles_ui_draw_anchor(OraclesUiDraw *draw, float x, float y)
{
    if (!draw->overflow) return;
    draw->origin_x = ((float)draw->out_width - draw->scene_width * draw->scale) * x;
    draw->origin_y = ((float)draw->out_height - ORACLES_UI_SCENE_HEIGHT * draw->scale) * y;
}

float oracles_ui_draw_anchor_shift(const OraclesUiDraw *draw, float from_x, float to_x)
{
    if (!draw->overflow || draw->scale <= 0.0f) return 0.0f;
    return ((float)draw->out_width / draw->scale - draw->scene_width) * (to_x - from_x);
}

void oracles_ui_draw_freeze(OraclesUiDraw *draw, int frozen) { draw->frozen = frozen; }

unsigned oracles_ui_draw_glyphs_rasterised(const OraclesUiDraw *draw)
{
    unsigned total = draw->glyphs_rasterised;
    for (int i = 0; i < draw->atlas_count; i++) total += draw->atlases[i].rasterised;
    return total;
}

static const glyph_slot *glyph(atlas *a, int index, int limit);
static int upload(OraclesUiDraw *draw, atlas *a);
static atlas *atlas_for(OraclesUiDraw *draw, OraclesUiFont font, float size);

int oracles_ui_draw_prepare(OraclesUiDraw *draw, int width, int height, float min_scale, const OraclesUiTextStyle *const *styles, int count, const char *text)
{
    const float scale = scale_at_least(draw, width, height, min_scale);
    if (scale <= 0.0f) return 0;
    if (draw->raster_scale != scale) {
        release_atlases(draw);
        draw->raster_scale = scale;
    }
    draw->scale = draw->wanted_scale = scale;
    const int limit = draw->max_texture_side < ATLAS_MAX_SIDE ? draw->max_texture_side : ATLAS_MAX_SIDE;
    const unsigned before = oracles_ui_draw_glyphs_rasterised(draw);
    for (int s = 0; s < count; s++) {
        atlas *a = atlas_for(draw, styles[s]->font, styles[s]->size);
        if (!a) return -1;
        /* The text a piece at a time: the glyphs of a shaped run, ligatures included, are what the screens draw. */
        for (const char *p = text; *p;) {
            OraclesUiGlyph glyphs[MAX_TEXT_GLYPHS];
            char piece[MAX_TEXT_GLYPHS];
            size_t n = 0;
            while (p[n] && n + 1 < sizeof piece - 4) n++;
            while (n > 0 && p[n] && ((unsigned char)p[n] & 0xc0u) == 0x80u) n--;   /* whole UTF-8 sequences */
            memcpy(piece, p, n);
            piece[n] = 0;
            size_t glyph_count = oracles_ui_text_glyphs(styles[s], piece, glyphs, MAX_TEXT_GLYPHS, NULL);
            if (glyph_count > MAX_TEXT_GLYPHS) glyph_count = MAX_TEXT_GLYPHS;
            for (size_t g = 0; g < glyph_count; g++) glyph(a, glyphs[g].glyph, limit);
            p += n;
        }
        if (!upload(draw, a)) return -1;
    }
    return (int)(oracles_ui_draw_glyphs_rasterised(draw) - before);
}

void oracles_ui_draw_end(OraclesUiDraw *draw) { SDL_SetRenderClipRect(draw->renderer, NULL); }

double oracles_ui_draw_due_ms(const OraclesUiDraw *draw)
{
    return draw->raster_scale != draw->wanted_scale ? draw->wanted_since_ms + RASTER_SETTLE_MS : -1.0;
}

void oracles_ui_draw_to_scene(const OraclesUiDraw *draw, float x, float y, float *scene_x, float *scene_y)
{
    *scene_x = draw->scale > 0.0f ? (x - draw->origin_x) / draw->scale : 0.0f;
    *scene_y = draw->scale > 0.0f ? (y - draw->origin_y) / draw->scale : 0.0f;
}

void oracles_ui_draw_to_scene_anchored(const OraclesUiDraw *draw, float x, float y, float anchor_x, float anchor_y, float *scene_x, float *scene_y)
{
    const float ax = draw->overflow ? anchor_x : 0.5f, ay = draw->overflow ? anchor_y : 0.5f;
    const float origin_x = ((float)draw->out_width - draw->scene_width * draw->scale) * ax;
    const float origin_y = ((float)draw->out_height - ORACLES_UI_SCENE_HEIGHT * draw->scale) * ay;
    *scene_x = draw->scale > 0.0f ? (x - origin_x) / draw->scale : 0.0f;
    *scene_y = draw->scale > 0.0f ? (y - origin_y) / draw->scale : 0.0f;
}

/* ---- shapes ------------------------------------------------------------------ */

static SDL_Vertex vertex(float x, float y, OraclesUiColor color)
{
    SDL_Vertex v;
    v.position.x = x;
    v.position.y = y;
    v.color.r = (float)color.r / 255.0f; v.color.g = (float)color.g / 255.0f;   /* the byte back exactly at the renderer */
    v.color.b = (float)color.b / 255.0f; v.color.a = (float)color.a / 255.0f;
    v.tex_coord.x = v.tex_coord.y = 0.0f;
    return v;
}

/* The outline of a rounded rectangle in output pixels, grown by `offset` pixels (shrunk when negative). */
static void round_rect_loop(const OraclesUiDraw *draw, float x, float y, float w, float h, float radius, float offset, SDL_FPoint *out)
{
    const float s = draw->scale;
    float left = draw->origin_x + x * s - offset, top = draw->origin_y + y * s - offset;
    float right = draw->origin_x + (x + w) * s + offset, bottom = draw->origin_y + (y + h) * s + offset;
    float r = radius * s + offset;
    if (r < 0.0f) r = 0.0f;
    if (right < left) left = right = (left + right) * 0.5f;
    if (bottom < top) top = bottom = (top + bottom) * 0.5f;
    if (r > (right - left) * 0.5f) r = (right - left) * 0.5f;
    if (r > (bottom - top) * 0.5f) r = (bottom - top) * 0.5f;
    const float centres[4][2] = { { right - r, top + r }, { right - r, bottom - r }, { left + r, bottom - r }, { left + r, top + r } };
    const float pi = 3.14159265358979f;
    SDL_FPoint *p = out;
    for (int corner = 0; corner < 4; corner++) {
        for (int i = 0; i < CORNER_POINTS; i++) {
            /* Clockwise on screen, from the top-right corner's start at -90 degrees. */
            const float angle = -pi * 0.5f + (float)corner * pi * 0.5f + (float)i / (float)(CORNER_POINTS - 1) * pi * 0.5f;
            p->x = centres[corner][0] + r * cosf(angle);
            p->y = centres[corner][1] + r * sinf(angle);
            p++;
        }
        /* The side to the next corner's start, in pieces. */
        const SDL_FPoint from = p[-1];
        const int next = (corner + 1) % 4;
        const float angle = -pi * 0.5f + (float)next * pi * 0.5f;
        const SDL_FPoint to = { centres[next][0] + r * cosf(angle), centres[next][1] + r * sinf(angle) };
        for (int i = 1; i <= SIDE_POINTS; i++) {
            const float t = (float)i / (float)(SIDE_POINTS + 1);
            p->x = from.x + (to.x - from.x) * t;
            p->y = from.y + (to.y - from.y) * t;
            p++;
        }
    }
}

/* A band between two loops of `count` points, each with its own colour. */
static void band(SDL_Vertex *vertices, int *vertex_count, int *indices, int *index_count,
                 const SDL_FPoint *a, OraclesUiColor color_a, const SDL_FPoint *b, OraclesUiColor color_b, int count)
{
    const int base = *vertex_count;
    for (int i = 0; i < count; i++) {
        vertices[base + 2 * i] = vertex(a[i].x, a[i].y, color_a);
        vertices[base + 2 * i + 1] = vertex(b[i].x, b[i].y, color_b);
    }
    for (int i = 0; i < count; i++) {
        const int j = (i + 1) % count;
        const int quad[6] = { base + 2 * i, base + 2 * i + 1, base + 2 * j, base + 2 * j, base + 2 * i + 1, base + 2 * j + 1 };
        memcpy(indices + *index_count, quad, sizeof quad);
        *index_count += 6;
    }
    *vertex_count += 2 * count;
}

/* The inside of a loop, a fan from its first point. */
static void fan(SDL_Vertex *vertices, int *vertex_count, int *indices, int *index_count, const SDL_FPoint *loop, int count, OraclesUiColor color)
{
    const int base = *vertex_count;
    for (int i = 0; i < count; i++) vertices[base + i] = vertex(loop[i].x, loop[i].y, color);
    for (int i = 1; i + 1 < count; i++) {
        indices[(*index_count)++] = base;
        indices[(*index_count)++] = base + i;
        indices[(*index_count)++] = base + i + 1;
    }
    *vertex_count += count;
}

void oracles_ui_fill_round_rect(OraclesUiDraw *draw, float x, float y, float w, float h, float radius, OraclesUiColor color)
{
    if (color.a == 0) return;
    SDL_FPoint inner[LOOP_POINTS], outer[LOOP_POINTS];
    round_rect_loop(draw, x, y, w, h, radius, -0.5f, inner);
    round_rect_loop(draw, x, y, w, h, radius, 0.5f, outer);
    SDL_Vertex vertices[3 * LOOP_POINTS];
    int indices[9 * LOOP_POINTS], vertex_count = 0, index_count = 0;
    fan(vertices, &vertex_count, indices, &index_count, inner, LOOP_POINTS, color);
    band(vertices, &vertex_count, indices, &index_count, inner, color, outer, oracles_ui_fade(color, 0.0f), LOOP_POINTS);
    SDL_RenderGeometry(draw->renderer, NULL, vertices, vertex_count, indices, index_count);
}

void oracles_ui_fill_rect(OraclesUiDraw *draw, float x, float y, float w, float h, OraclesUiColor color)
{
    oracles_ui_fill_round_rect(draw, x, y, w, h, 0.0f, color);
}

void oracles_ui_stroke_round_rect(OraclesUiDraw *draw, float x, float y, float w, float h, float radius, float border, OraclesUiColor color)
{
    if (color.a == 0) return;
    /* A line thinner than a pixel is drawn one pixel wide and fainter. */
    float width = border * draw->scale;
    if (width < 1.0f) { color = oracles_ui_fade(color, width); width = 1.0f; }
    const float centre = -border * draw->scale * 0.5f;   /* the border's middle, from the box's edge */
    SDL_FPoint loops[4][LOOP_POINTS];
    const float offsets[4] = { centre + width * 0.5f + 0.5f, centre + width * 0.5f - 0.5f, centre - width * 0.5f + 0.5f, centre - width * 0.5f - 0.5f };
    for (int i = 0; i < 4; i++) round_rect_loop(draw, x, y, w, h, radius, offsets[i], loops[i]);
    const OraclesUiColor clear = oracles_ui_fade(color, 0.0f);
    SDL_Vertex vertices[6 * LOOP_POINTS];
    int indices[18 * LOOP_POINTS], vertex_count = 0, index_count = 0;
    band(vertices, &vertex_count, indices, &index_count, loops[0], clear, loops[1], color, LOOP_POINTS);
    band(vertices, &vertex_count, indices, &index_count, loops[1], color, loops[2], color, LOOP_POINTS);
    band(vertices, &vertex_count, indices, &index_count, loops[2], color, loops[3], clear, LOOP_POINTS);
    SDL_RenderGeometry(draw->renderer, NULL, vertices, vertex_count, indices, index_count);
}

#define MAX_POLYGON 16

void oracles_ui_fill_polygon(OraclesUiDraw *draw, const float *points, int count, OraclesUiColor color)
{
    if (count < 3 || count > MAX_POLYGON || color.a == 0) return;
    SDL_FPoint p[MAX_POLYGON], inner[MAX_POLYGON], outer[MAX_POLYGON];
    float area = 0.0f;
    for (int i = 0; i < count; i++) {
        p[i].x = draw->origin_x + points[2 * i] * draw->scale;
        p[i].y = draw->origin_y + points[2 * i + 1] * draw->scale;
    }
    for (int i = 0; i < count; i++) {
        const int j = (i + 1) % count;
        area += p[i].x * p[j].y - p[j].x * p[i].y;
    }
    const float outward = area > 0.0f ? 1.0f : -1.0f;   /* y down: a positive area runs clockwise on screen */
    for (int i = 0; i < count; i++) {
        /* The normals of the edges into and out of the point, and the miter between them. */
        const SDL_FPoint a = p[(i + count - 1) % count], b = p[i], c = p[(i + 1) % count];
        float n1x = (b.y - a.y), n1y = -(b.x - a.x), n2x = (c.y - b.y), n2y = -(c.x - b.x);
        const float l1 = sqrtf(n1x * n1x + n1y * n1y), l2 = sqrtf(n2x * n2x + n2y * n2y);
        if (l1 > 0.0f) { n1x /= l1; n1y /= l1; }
        if (l2 > 0.0f) { n2x /= l2; n2y /= l2; }
        float mx = n1x + n2x, my = n1y + n2y;
        const float d = 1.0f + n1x * n2x + n1y * n2y;
        if (d > 0.25f) { mx /= d; my /= d; } else { mx = n1x * 2.0f; my = n1y * 2.0f; }
        mx *= -outward * 0.5f;
        my *= -outward * 0.5f;
        inner[i].x = b.x - mx; inner[i].y = b.y - my;
        outer[i].x = b.x + mx; outer[i].y = b.y + my;
    }
    SDL_Vertex vertices[3 * MAX_POLYGON];
    int indices[9 * MAX_POLYGON], vertex_count = 0, index_count = 0;
    fan(vertices, &vertex_count, indices, &index_count, inner, count, color);
    band(vertices, &vertex_count, indices, &index_count, inner, color, outer, oracles_ui_fade(color, 0.0f), count);
    SDL_RenderGeometry(draw->renderer, NULL, vertices, vertex_count, indices, index_count);
}

/* ---- text -------------------------------------------------------------------- */

static atlas *atlas_for(OraclesUiDraw *draw, OraclesUiFont font, float size)
{
    for (int i = 0; i < draw->atlas_count; i++)
        if (draw->atlases[i].font == font && draw->atlases[i].size == size) return &draw->atlases[i];
    if (draw->atlas_count == MAX_ATLASES) return NULL;
    atlas *a = &draw->atlases[draw->atlas_count];
    memset(a, 0, sizeof *a);
    a->font = font;
    a->size = size;
    a->pixel_size = size * draw->raster_scale;
    a->width = 512;
    a->height = 256;
    a->coverage = calloc((size_t)a->width * (size_t)a->height, 1);
    a->slots = calloc((size_t)oracles_ui_font_glyph_count(font), sizeof *a->slots);
    if (!a->coverage || !a->slots) { free(a->coverage); free(a->slots); return NULL; }
    draw->atlas_count++;
    return a;
}

/* Doubles the atlas, height first; the texture is made again at the next upload. */
static int grow(atlas *a, int limit)
{
    const int width = a->height < a->width ? a->width : a->width * 2, height = a->height < a->width ? a->height * 2 : a->height;
    if (width > limit || height > limit) return 0;
    unsigned char *coverage = calloc((size_t)width * (size_t)height, 1);
    if (!coverage) return 0;
    for (int row = 0; row < a->height; row++) memcpy(coverage + (size_t)row * (size_t)width, a->coverage + (size_t)row * (size_t)a->width, (size_t)a->width);
    free(a->coverage);
    a->coverage = coverage;
    a->width = width;
    a->height = height;
    if (a->texture) SDL_DestroyTexture(a->texture);
    a->texture = NULL;
    a->dirty = 1;
    return 1;
}

static const glyph_slot *glyph(atlas *a, int index, int limit)
{
    glyph_slot *slot = &a->slots[index];
    if (slot->ready) return slot;
    int x0, y0, x1, y1;
    oracles_ui_glyph_box(a->font, index, a->pixel_size, &x0, &y0, &x1, &y1);
    const int w = x1 - x0, h = y1 - y0;
    slot->x_offset = (int16_t)x0;
    slot->y_offset = (int16_t)y0;
    slot->w = (int16_t)(w > 0 ? w : 0);
    slot->h = (int16_t)(h > 0 ? h : 0);
    slot->ready = 1;
    if (w <= 0 || h <= 0) return slot;   /* a space */
    for (;;) {
        if (a->pen_x + w + ATLAS_PADDING > a->width) { a->pen_x = 0; a->pen_y += a->row_height + ATLAS_PADDING; a->row_height = 0; }
        if (a->pen_y + h + ATLAS_PADDING <= a->height && w + ATLAS_PADDING <= a->width) break;
        if (!grow(a, limit)) { slot->w = slot->h = 0; return slot; }
    }
    slot->x = (int16_t)a->pen_x;
    slot->y = (int16_t)a->pen_y;
    oracles_ui_glyph_render(a->font, index, a->pixel_size, a->coverage + (size_t)a->pen_y * (size_t)a->width + (size_t)a->pen_x, w, h, a->width);
    a->rasterised++;
    a->pen_x += w + ATLAS_PADDING;
    if (h > a->row_height) a->row_height = h;
    a->dirty = 1;
    return slot;
}

static int upload(OraclesUiDraw *draw, atlas *a)
{
    if (!a->dirty && a->texture) return 1;
    if (!a->texture) {
        a->texture = SDL_CreateTexture(draw->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, a->width, a->height);
        if (!a->texture) return 0;
        SDL_SetTextureBlendMode(a->texture, SDL_BLENDMODE_BLEND);
    }
    uint32_t *pixels = malloc((size_t)a->width * (size_t)a->height * sizeof *pixels);
    if (!pixels) return 0;
    for (size_t i = 0; i < (size_t)a->width * (size_t)a->height; i++) pixels[i] = (uint32_t)a->coverage[i] << 24 | 0xffffffu;
    const int ok = SDL_UpdateTexture(a->texture, NULL, pixels, a->width * 4);
    free(pixels);
    a->dirty = !ok;
    return ok;
}

void oracles_ui_draw_text(OraclesUiDraw *draw, const OraclesUiTextStyle *style, float x, float baseline, const char *utf8, OraclesUiColor color)
{
    if (color.a == 0 || draw->raster_scale <= 0.0f) return;
    atlas *a = atlas_for(draw, style->font, style->size);
    if (!a) return;
    OraclesUiGlyph glyphs[MAX_TEXT_GLYPHS];
    size_t count = oracles_ui_text_glyphs(style, utf8, glyphs, MAX_TEXT_GLYPHS, NULL);
    if (count > MAX_TEXT_GLYPHS) count = MAX_TEXT_GLYPHS;
    const int limit = draw->max_texture_side < ATLAS_MAX_SIDE ? draw->max_texture_side : ATLAS_MAX_SIDE;
    for (size_t i = 0; i < count; i++) glyph(a, glyphs[i].glyph, limit);
    if (!upload(draw, a)) return;
    const float s = draw->scale, k = s / draw->raster_scale;
    /* At the rasterised scale each glyph lands on whole pixels, as it was rasterised; stretched otherwise. */
    const int exact = k == 1.0f;
    SDL_SetTextureScaleMode(a->texture, exact ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);
    SDL_SetTextureColorMod(a->texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(a->texture, color.a);
    const float pen_x = draw->origin_x + x * s, pen_y = draw->origin_y + baseline * s;
    for (size_t i = 0; i < count; i++) {
        const glyph_slot *slot = &a->slots[glyphs[i].glyph];
        if (slot->w <= 0 || slot->h <= 0) continue;
        const SDL_FRect source = { (float)slot->x, (float)slot->y, (float)slot->w, (float)slot->h };
        SDL_FRect target;
        if (exact) {
            target.x = roundf(pen_x + glyphs[i].x * s) + (float)slot->x_offset;
            target.y = roundf(pen_y) + (float)slot->y_offset;
            target.w = (float)slot->w;
            target.h = (float)slot->h;
        } else {
            target.x = pen_x + glyphs[i].x * s + (float)slot->x_offset * k;
            target.y = pen_y + (float)slot->y_offset * k;
            target.w = (float)slot->w * k;
            target.h = (float)slot->h * k;
        }
        SDL_RenderTexture(draw->renderer, a->texture, &source, &target);
    }
}
