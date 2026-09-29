/* The three background motifs (the SVG files of motifs/, static, colours in sRGB),
 * rasterised with NanoSVG, without SDL.
 *
 * The files' viewBox is -1000 0 3920 1080: the 1920x1080 scene sits at x = 0
 * of their user space, with room on both sides for the motif layer's slide. */
#ifndef ORACLES_UI_MOTIF_H
#define ORACLES_UI_MOTIF_H

typedef enum OraclesUiMotif {
    ORACLES_UI_MOTIF_AGES,
    ORACLES_UI_MOTIF_SEASONS,
    ORACLES_UI_MOTIF_FAN,
    ORACLES_UI_MOTIF_COUNT
} OraclesUiMotif;

/* How far the motif layer slides left on the screens after the
 * home screen: a motif is rasterised that far past the scene's right edge. */
#define ORACLES_UI_MOTIF_SLIDE 860.0f

/* Rasterises the scene's columns [x0, x1) and rows [0, 1080) at `scale` pixels
 * per scene pixel into a buffer the caller frees: straight RGBA, 8 bits per
 * channel, rows of *width pixels.  Returns 0 on failure. */
int oracles_ui_motif_raster(OraclesUiMotif motif, float scale, float x0, float x1,
                            unsigned char **rgba, int *width, int *height);

#endif
