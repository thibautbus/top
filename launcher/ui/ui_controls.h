/* Controls drawn: ui_controls_nav.h's state at the places of
 * ui_controls_layout.h, in its colours. */
#ifndef ORACLES_UI_CONTROLS_H
#define ORACLES_UI_CONTROLS_H

#include "ui_controls_nav.h"
#include "ui_draw.h"

void oracles_ui_controls_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent);

/* What lies under a point: 1 with a cell's column and row (Reset: row 8; the Item hotkeys line: row -1, and its
 * Off or On in *option, else -1), 0 for nothing. */
int oracles_ui_controls_hit(const OraclesHomeNav *nav, float x, float y, int *column, int *row, int *option);

#endif
