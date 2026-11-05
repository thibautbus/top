/* Display's and Controls' choices for the home screen's host (home_games.c): read from settings.txt into the pages,
 * with the size of the screen Display shows them for, and Controls' written back; and the whole scale a game's window
 * is lowered to on the display. */
#ifndef ORACLES_LAUNCHER_HOME_CHOICES_H
#define ORACLES_LAUNCHER_HOME_CHOICES_H

#include "compositor.h"
#include "rom.h"
#include "settings.h"
#include "ui_home_nav.h"

struct SDL_Window;

void oracles_home_inspect_display(const oracles_settings *settings, OraclesHomeDisplay *display, struct SDL_Window *window);
void oracles_home_inspect_controls(const oracles_settings *settings, OraclesHomeControls *c);
/* menu_pad= as every screen and the session take it, once the settings are read: an unknown name the default, and
 * none, Guide alone, when the game's buttons, the d-pad or an item hotkey slot have it (oracles_controls_menu_free);
 * the standard error says which.  Controls then never gives the menu a button another cell has, nor that cell its. */
void oracles_home_resolve_menu_pad(oracles_settings *settings);
/* Display's choices written back into the settings (`in_game`: from the pause, whose core and workers are the running
 * game's and never stored); a margin --screen-margin gave (display->margin_fixed), and a text, a text box or a text size
 * --text, --text-box or --text-size gave (display->text_fixed, text_box_fixed, text_size_fixed), and the rooms never
 * visited --unvisited-rooms gave (display->unvisited_fixed) are never stored either, nor from the pause a window not
 * chosen (display->window_chosen). */
void oracles_home_store_display(const OraclesHomeDisplay *display, int in_game, oracles_settings *prefs);
void oracles_home_store_controls(const OraclesHomeControls *c, oracles_settings *settings);
/* What the profile of a game's own image does not qualify (OraclesHomeGame's refuses), by its revision: the image a fan
 * game's patch is made to give, built or not. */
unsigned oracles_home_refusals(OraclesGame family, OraclesRomRevision revision);
/* The cheats of game `game` (ORACLES_HOME_GAME_*) in the settings: an Oracle's cheats_<game>=, a fan game's own. */
unsigned oracles_home_cheats(const oracles_settings *settings, int game);
/* The cheats the Cheats page changed (nav->cheats_changed), each game's into its own key; never --cheats'. */
void oracles_home_store_cheats(const OraclesHomeNav *nav, oracles_settings *prefs);
unsigned oracles_home_fitting_scale(struct SDL_Window *window, unsigned wanted, OraclesEnhancedSize surface);
/* Display's window (window_scale=: 2 to 4, 0 fullscreen) for a game whose surface is `surface`, on the screen of
 * `window`, as Play opens it: a scale lowered to what the display holds (4 for fullscreen, F11's window), and
 * fullscreen where Fill finds every window the same size (oracles_display_one_size_for). */
void oracles_home_game_window(const oracles_settings *settings, int window_scale, struct SDL_Window *window, OraclesEnhancedSize surface,
                              int *fullscreen, unsigned *scale);

#endif
