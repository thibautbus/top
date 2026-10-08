/* Drawing of the launcher on SDL's 2D renderer, in C.
 *
 * Everything is placed in the coordinates of the scene of a layout (ui_layout.h), 1920x1080 or 1440x1080, scaled
 * by min(w / its width, h / 1080) and centred in the output on #050507.  Text is drawn from glyph atlases, one per font and size,
 * and the motifs from textures, both rasterised at the output's scale so that
 * text stays sharp at any window size; when the scale changes they are
 * rasterised again once it has held for a moment (a window being resized),
 * the old ones being stretched meanwhile.  Shapes are triangles with a
 * half-pixel feathered edge. */
#ifndef ORACLES_UI_DRAW_H
#define ORACLES_UI_DRAW_H

#include "ui_font.h"
#include "ui_layout.h"
#include "ui_motif.h"

#include <SDL3/SDL.h>
#include <stdint.h>

typedef struct OraclesUiColor {
    uint8_t r, g, b, a;
} OraclesUiColor;

/* An opaque colour from 0xRRGGBB, and the same with an opacity in [0, 1]. */
OraclesUiColor oracles_ui_rgb(uint32_t rgb);
OraclesUiColor oracles_ui_rgba(uint32_t rgb, float opacity);
/* The colour with its alpha multiplied by `opacity`; and the mix of two colours. */
OraclesUiColor oracles_ui_fade(OraclesUiColor color, float opacity);
OraclesUiColor oracles_ui_mix(OraclesUiColor from, OraclesUiColor to, float t);

typedef struct OraclesUiDraw OraclesUiDraw;

OraclesUiDraw *oracles_ui_draw_create(SDL_Renderer *renderer);
void oracles_ui_draw_destroy(OraclesUiDraw *draw);

/* The layout of the frames begun from now on (16:9 until one is set): its scene's width, where the motifs sit in it.
 * A motif rasterised for the other layout is rasterised again when it is next shown. */
void oracles_ui_draw_layout(OraclesUiDraw *draw, OraclesUiLayout layout);
/* Starts a frame on an output of `width` x `height` pixels: the letterbox, the
 * scene's background, the clip to the scene.  Returns 0 on failure. */
int oracles_ui_draw_begin(OraclesUiDraw *draw, int width, int height, double now_ms);
/* The same over what the output already shows (a game's last image): no letterbox, no background, the clip only.  The
 * scale is `min_scale` at least: a scene then larger than the output has its parts anchored (oracles_ui_draw_anchor). */
int oracles_ui_draw_begin_over(OraclesUiDraw *draw, int width, int height, float min_scale, double now_ms);
/* Whether the scene of the frame begun is larger than the output. */
int oracles_ui_draw_overflows(const OraclesUiDraw *draw);
/* For a scene larger than the output, what comes next is placed against the output's sides: `x` 0 left, 0.5 centre,
 * 1 right, `y` 0 top, 1 bottom.  Nothing changes when the scene fits. */
void oracles_ui_draw_anchor(OraclesUiDraw *draw, float x, float y);
/* How far, in the scene's units, a part anchored at `to_x` sits from one anchored at `from_x` (0 when the scene fits). */
float oracles_ui_draw_anchor_shift(const OraclesUiDraw *draw, float from_x, float to_x);
void oracles_ui_draw_end(OraclesUiDraw *draw);
/* Rasterises, for an output of `width` x `height` drawn in the layout set at `min_scale` at least, the glyphs of `text` in each of the `count` styles, and uploads
 * them: what a screen drawn later at that size needs, ready beforehand.  Returns the glyphs rasterised, -1 on failure. */
int oracles_ui_draw_prepare(OraclesUiDraw *draw, int width, int height, float min_scale, const OraclesUiTextStyle *const *styles, int count, const char *text);
/* Frozen, the rasters keep their scale whatever the output's: stretched, never redone (a game's pause). */
void oracles_ui_draw_freeze(OraclesUiDraw *draw, int frozen);
/* The glyphs rasterised since the drawing was created: two readings apart, what was rasterised between them. */
unsigned oracles_ui_draw_glyphs_rasterised(const OraclesUiDraw *draw);
/* The time at which rasters left stretched by a change of scale are due to be
 * redone, for the caller's wait; negative when none are. */
double oracles_ui_draw_due_ms(const OraclesUiDraw *draw);
/* Work for an idle moment: rasterises one motif not yet at the current scale
 * (a motif is otherwise rasterised when first shown).  Returns 1 while some
 * remain, for the caller to come back before it waits. */
int oracles_ui_draw_idle(OraclesUiDraw *draw);
/* A point of the output, in pixels, in the scene's coordinates. */
void oracles_ui_draw_to_scene(const OraclesUiDraw *draw, float x, float y, float *scene_x, float *scene_y);
/* The same for a part anchored as oracles_ui_draw_anchor places it. */
void oracles_ui_draw_to_scene_anchored(const OraclesUiDraw *draw, float x, float y, float anchor_x, float anchor_y, float *scene_x, float *scene_y);

void oracles_ui_fill_rect(OraclesUiDraw *draw, float x, float y, float w, float h, OraclesUiColor color);
void oracles_ui_fill_round_rect(OraclesUiDraw *draw, float x, float y, float w, float h, float radius, OraclesUiColor color);
/* A border of `border` scene pixels inside the box, as CSS draws one. */
void oracles_ui_stroke_round_rect(OraclesUiDraw *draw, float x, float y, float w, float h, float radius, float border, OraclesUiColor color);
/* A dashed border of `border` scene pixels inside the box, along its straight sides, dashes and gaps three times as long. */
void oracles_ui_stroke_dashed_rect(OraclesUiDraw *draw, float x, float y, float w, float h, float radius, float border, OraclesUiColor color);
/* A convex polygon of `count` points, x then y. */
void oracles_ui_fill_polygon(OraclesUiDraw *draw, const float *points, int count, OraclesUiColor color);
/* One line of text starting at `x` with its baseline at `baseline`. */
void oracles_ui_draw_text(OraclesUiDraw *draw, const OraclesUiTextStyle *style, float x, float baseline, const char *utf8, OraclesUiColor color);
/* A motif over the whole scene, slid by `shift` scene pixels (negative: left). */
void oracles_ui_draw_motif(OraclesUiDraw *draw, OraclesUiMotif motif, float shift, float opacity);

#endif
