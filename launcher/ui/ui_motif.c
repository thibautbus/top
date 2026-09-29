#include "ui_motif.h"

#include "ui_assets.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nanosvg.h"
#include "nanosvgrast.h"

/* The scene's origin in the files' image space (their viewBox starts at x = -1000). */
#define SCENE_ORIGIN_X 1000.0f

static const char *const motif_assets[ORACLES_UI_MOTIF_COUNT] = { "ages.svg", "seasons.svg", "fan.svg" };

int oracles_ui_motif_raster(OraclesUiMotif motif, float scale, float x0, float x1,
                            unsigned char **rgba, int *width, int *height)
{
    *rgba = NULL;
    *width = *height = 0;
    const OraclesUiAsset *asset = oracles_ui_asset(motif_assets[motif]);
    if (!asset || scale <= 0.0f || x1 <= x0) return 0;
    /* NanoSVG parses in place: a copy, NUL-terminated like the embedded array. */
    char *text = malloc(asset->size + 1);
    if (!text) return 0;
    memcpy(text, asset->data, asset->size + 1);
    NSVGimage *image = nsvgParse(text, "px", 96.0f);
    free(text);
    if (!image) return 0;
    const int w = (int)ceilf((x1 - x0) * scale), h = (int)ceilf(1080.0f * scale);
    unsigned char *pixels = w > 0 && h > 0 ? malloc((size_t)w * (size_t)h * 4u) : NULL;
    NSVGrasterizer *rasterizer = nsvgCreateRasterizer();
    const int ok = pixels && rasterizer;
    if (ok) {
        memset(pixels, 0, (size_t)w * (size_t)h * 4u);
        nsvgRasterize(rasterizer, image, -(SCENE_ORIGIN_X + x0) * scale, 0.0f, scale, pixels, w, h, w * 4);
        *rgba = pixels;
        *width = w;
        *height = h;
    } else {
        free(pixels);
    }
    if (rasterizer) nsvgDeleteRasterizer(rasterizer);
    nsvgDelete(image);
    return ok;
}
