/* Cartridge, Display, Mods, Cheats and Settings drawn: ui_page_nav.h's state at the places of
 * ui_page_layout.h, in their colours, in the layout of nav->layout; a point
 * is in that layout's scene. */
#ifndef ORACLES_UI_PAGE_H
#define ORACLES_UI_PAGE_H

#include "ui_draw.h"
#include "ui_page_nav.h"

/* The page over the motifs, in the colours of its game's `accent`. */
void oracles_ui_page_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent);

/* The import's confirmation over the page and the help bar, while it is open (ui_page_nav.h). */
void oracles_ui_page_dialog_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent);

/* What lies under a point of the page: a row, or -1, and an option of it (oracles_page_click's), or -1. */
int oracles_ui_page_hit(const OraclesHomeNav *nav, float x, float y, int *option);

/* Display, and what lies under a point of it. */
void oracles_ui_display_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent);
int oracles_ui_display_hit(const OraclesHomeNav *nav, float x, float y, int *option);

/* Mods, and the row under a point of it: Folder, a mod, Play, or -1. */
void oracles_ui_mods_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent);
int oracles_ui_mods_hit(const OraclesHomeNav *nav, float x, float y);

/* Cheats, and the row under a point of it, or -1, and its Off (0) or On (1) under it, or -1. */
void oracles_ui_cheats_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent);
int oracles_ui_cheats_hit(const OraclesHomeNav *nav, float x, float y, int *option);

/* Settings, and its row under a point of it (0), or -1, and its first (0) or second (1) choice under it, or Licences' line (2), or -1. */
void oracles_ui_about_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent);
int oracles_ui_about_hit(const OraclesHomeNav *nav, float x, float y, int *option);

#endif
