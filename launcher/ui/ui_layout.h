/* The launcher's two layouts, without SDL (the mockup's turn 10): the 16:9 one, drawn in a 1920x1080
 * scene, and the 4:3 one, drawn in a 1440x1080 scene sized for a 640x480 handheld (its smallest text 27 px, its
 * targets 90 px tall).  An output takes the one its shape is nearer to, by the ratio of the two shapes (the 4:3
 * layout for a portrait window too), unless the settings' aspect= names one; it is chosen again at each frame, so
 * that a window resized across the middle changes layout.  Each screen is drawn in its layout's scene, scaled by
 * min(w / scene width, h / 1080) and centred (ui_draw.h). */
#ifndef ORACLES_UI_LAYOUT_H
#define ORACLES_UI_LAYOUT_H

typedef enum OraclesUiLayout {
    ORACLES_UI_LAYOUT_16_9,
    ORACLES_UI_LAYOUT_4_3
} OraclesUiLayout;

#define ORACLES_UI_SCENE_HEIGHT 1080.0f

/* The layout of an output of `width` x `height` pixels under `aspect`, the settings' aspect= (an OraclesAspect: 0
 * auto, 1 16:9, 2 4:3). */
OraclesUiLayout oracles_ui_layout_choose(int width, int height, int aspect);
/* The width of the layout's scene: 1920 or 1440. */
float oracles_ui_scene_width(OraclesUiLayout layout);
/* Where the motifs' 1920x1080 scene sits in the layout's, x: 0 in 16:9; in 4:3, 800 to the left, the ring left of
 * the menu instead of behind it. */
float oracles_ui_motif_x(OraclesUiLayout layout);
/* Whether the pause menu in a window `width` pixels wide keeps only the session's own entries: under 960 in 16:9,
 * never in 4:3, whose scene is sized for 640x480 already. */
int oracles_ui_layout_narrow(OraclesUiLayout layout, int width);

#endif
