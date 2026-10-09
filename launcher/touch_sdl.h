/* The touch controls (touch_controls.h) on SDL: the fingers' events read, the controls drawn over the game. */
#ifndef ORACLES_LAUNCHER_TOUCH_SDL_H
#define ORACLES_LAUNCHER_TOUCH_SDL_H

#include "touch_controls.h"

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_FRect;
union SDL_Event;

/* The controls and how the game's frame is drawn once they showed: SDL's logical presentation draws the frame through
 * a texture of the frame's own size and leaves the screen beside it as it was, where the controls go.  From the first
 * touch of a session the frame is drawn straight to the screen, where the logical presentation would put it (its
 * integer scale, or with `fill` the whole screen in the frame's proportions), black beside it: given back to SDL, the
 * logical presentation would leave the controls' last drawing, or Android's undefined buffer, beside the frame. */
typedef struct OraclesTouchScreen {
    OraclesTouch touch;
    int direct;
    int logical_w, logical_h;
    int fill;   /* Scaling's Fill in fullscreen, which the backend sets */
} OraclesTouchScreen;

/* 1 when `event` is a finger's, in the renderer's output: *buttons the game's buttons all fingers press now, *pause
 * set when this finger went down on the pause.  0 for any other event. */
int oracles_touch_sdl_event(OraclesTouchScreen *screen, struct SDL_Renderer *renderer, const union SDL_Event *event,
                            unsigned *buttons, int *pause);
/* The game's frame, `frame`, drawn to the whole screen with the controls over it when they show: 1, from the first
 * touch of the session on.  0 before it, the caller drawing the frame through the logical presentation. */
int oracles_touch_sdl_frame(OraclesTouchScreen *screen, struct SDL_Renderer *renderer, struct SDL_Texture *frame, const struct SDL_FRect *part);

#endif
