/* RetroAchievements' screens drawn on SDL's renderer (ui_achievements.h), in a build with ORACLES_RETROACHIEVEMENTS. */
#ifndef ORACLES_UI_ACHIEVEMENTS_DRAW_H
#define ORACLES_UI_ACHIEVEMENTS_DRAW_H

#include "ui_achievements.h"
#include "ui_draw.h"

/* About › RetroAchievements (ui_page.c), and its hits: a row, and its Off (0), On (1) or action (2) in *option. */
void oracles_ui_achievements_page_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent);
int oracles_ui_achievements_page_hit(const OraclesHomeNav *nav, float x, float y, int *option);
/* The login over the page dimmed, its keyboard when it is up; what is under the point: a field, a button or a key as
 * oracles_achievements_login_click takes them, -1 for none. */
void oracles_ui_achievements_login_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent);
int oracles_ui_achievements_login_hit(const OraclesHomeNav *nav, float x, float y);
/* The pause's list; 1 when a badge it drew was not there yet (being downloaded): the pause draws it again a while later. */
void oracles_ui_achievements_list_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent, double now_ms);
int oracles_ui_achievements_list_waiting(void);
/* A toast over the game: its box and texts, its badge from `badges_folder` when its first frame found it there. */
void oracles_ui_achievements_toast_draw(OraclesUiDraw *draw, OraclesUiLayout layout, OraclesAchievementsToast *toast, const char *badges_folder,
                                        float left, float bar_bottom, int reminder, float opacity, double now_ms);
/* The image of badge `name` (its locked one) in `folder` as a texture of the drawing's renderer, NULL while its file is
 * not there; the textures made, freed. */
SDL_Texture *oracles_ui_achievements_badge(OraclesUiDraw *draw, const char *folder, const char *name, int locked, double now_ms);
void oracles_ui_achievements_forget_badges(void);

#endif
