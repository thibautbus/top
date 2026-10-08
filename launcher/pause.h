/* The pause menu of a game the home screen started: Escape in play holds
 * the session between two frames
 * and lays the menu over the game's last image, darkened, at the size of the
 * game's window.  Resume, Save state, Load state, Controls, Display, Quit to
 * launcher; under 960 pixels wide, Controls and Display stay in the launcher.
 *
 * Nothing is rasterised during the session: the glyphs of every screen the
 * pause shows are rasterised for the window's size before the first frame
 * (oracles_pause_prepare), again at a pause only if the window's size has
 * changed since, and the rasters stay frozen while the session lasts. */
#ifndef ORACLES_LAUNCHER_PAUSE_H
#define ORACLES_LAUNCHER_PAUSE_H

#include "home.h"
#include "ui_home.h"

#include <SDL3/SDL.h>

/* What the pause asks of the running session. */
typedef struct OraclesPauseSession {
    void *opaque;
    /* F5 and F7's work; 1 when done, *message saying what went wrong otherwise. */
    int (*save_state)(void *opaque, char *message, size_t capacity);
    int (*load_state)(void *opaque, char *message, size_t capacity);
    /* The time of the savestate, "22:14", or empty when there is none. */
    void (*state_time)(void *opaque, char *out, size_t capacity);
    /* The settings changed: the keys and the colour correction apply at once. */
    void (*settings_changed)(void *opaque);
    int core;   /* the running game's core, an OraclesCoreKind: Display shows it, dimmed */
} OraclesPauseSession;

typedef enum OraclesPauseResult {
    ORACLES_PAUSE_RESUME,
    ORACLES_PAUSE_QUIT,              /* Quit to launcher */
    ORACLES_PAUSE_WINDOW_CLOSED
} OraclesPauseResult;

typedef struct OraclesPause OraclesPause;

/* A pause for `game`'s session (ORACLES_HOME_GAME_*) running in `playing`, drawn with the home screen's `draw`, its settings read and written
 * through `host` (refresh, store). */
OraclesPause *oracles_pause_create(OraclesUiDraw *draw, const OraclesHomeHost *host, int game, OraclesProfile playing);
void oracles_pause_destroy(OraclesPause *pause);

/* Before the session's first frame: the glyphs for the renderer's output, and the rasters frozen.  Says on stderr
 * what it rasterised and how long it took. */
void oracles_pause_prepare(OraclesPause *pause, SDL_Renderer *renderer);
/* The pause, until the player resumes or leaves; `frame` holds the game's last image, `width` x `height`. */
OraclesPauseResult oracles_pause_run(OraclesPause *pause, SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *frame,
                                     int width, int height, const OraclesPauseSession *session);
/* Glyphs rasterised during the session's pauses (none when prepared beforehand), for the session's report. */
unsigned oracles_pause_late_glyphs(const OraclesPause *pause);

/* The pause's image on an output of `out_width` x `out_height`: the game's `frame` at its whole scale, darkened, and
 * the screen of `nav` over it (the capture draws it offscreen too).  Returns 1 while a transition runs. */
int oracles_pause_paint(OraclesUiDraw *draw, SDL_Renderer *renderer, SDL_Texture *frame, int width, int height,
                        int out_width, int out_height, OraclesUiHome *view, const OraclesHomeNav *nav, double now_ms);
/* Whether a window this wide gets the reduced menu. */
int oracles_pause_narrow(int out_width);

#endif
