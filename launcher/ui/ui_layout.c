#include "ui_layout.h"

#define ASPECT_16_9 1   /* ORACLES_ASPECT_16_9 */
#define ASPECT_4_3 2    /* ORACLES_ASPECT_4_3 */
#define NARROW_WIDTH 960

OraclesUiLayout oracles_ui_layout_choose(int width, int height, int aspect)
{
    if (aspect == ASPECT_4_3) return ORACLES_UI_LAYOUT_4_3;
    if (aspect == ASPECT_16_9 || width <= 0 || height <= 0) return ORACLES_UI_LAYOUT_16_9;
    /* Nearer 4:3 than 16:9 by their ratio: w / h under sqrt(4/3 x 16/9), 27 w^2 < 64 h^2, in whole numbers. */
    const long long w = width, h = height;
    return 27 * w * w < 64 * h * h ? ORACLES_UI_LAYOUT_4_3 : ORACLES_UI_LAYOUT_16_9;
}

float oracles_ui_scene_width(OraclesUiLayout layout) { return layout == ORACLES_UI_LAYOUT_4_3 ? 1440.0f : 1920.0f; }

float oracles_ui_motif_x(OraclesUiLayout layout) { return layout == ORACLES_UI_LAYOUT_4_3 ? -800.0f : 0.0f; }

int oracles_ui_layout_narrow(OraclesUiLayout layout, int width) { return layout == ORACLES_UI_LAYOUT_16_9 && width < NARROW_WIDTH; }
