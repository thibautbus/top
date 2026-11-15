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
    /* The screen margin and what it was read from (oracles_safe_area_edges), which the backend sets: the frame and the
     * controls keep to its band (oracles_touch_band), drawn straight to the screen from the first frame when the band
     * is not the whole screen. */
    OraclesTouchEdges edges;
    /* Called once the frame is drawn straight to the screen, before the controls, with where it went: what the backend
     * draws over the frame (the text box enlarged).  NULL: nothing. */
    void (*over_frame)(void *opaque, struct SDL_Renderer *renderer, const struct SDL_FRect *to);
    void *over_frame_opaque;
    /* Called in place of drawing the frame's texture there, with the part shown (NULL for the whole frame) and its place:
     * 1 when the backend drew the frame itself (the band the sub-pixel camera moves), 0 to leave it to the texture.
     * NULL: the texture. */
    int (*draw_frame)(void *opaque, struct SDL_Renderer *renderer, const struct SDL_FRect *part, const struct SDL_FRect *to);
    void *draw_frame_opaque;
    /* What the controls drawn with the last frame covered of it, in its pixels (oracles_touch_frame_covered); none while
     * they are hidden. */
    int32_t covered[4][4];
    unsigned covered_count;
} OraclesTouchScreen;

/* 1 when `event` is a finger's, in the renderer's output: *buttons the game's buttons all fingers press now, *pause
 * set when this finger went down on the pause.  0 for any other event. */
int oracles_touch_sdl_event(OraclesTouchScreen *screen, struct SDL_Renderer *renderer, const union SDL_Event *event,
                            unsigned *buttons, int *pause);
/* A short tick of the device's touch feedback, for a finger that pressed something new (OraclesTouch.tick): on Android,
 * a thread of the launcher's woken to ask the activity to give it from its own thread, as Android's own on-screen keys
 * do, following the system's touch feedback setting; nothing elsewhere.  What a tick costs is said once a run on the
 * standard error. */
void oracles_touch_sdl_tick(void);
/* The game's frame, `frame`, drawn to the whole screen with the controls over it when they show: 1, from the first
 * touch of the session on, or with a band less than the whole screen.  0 before it, the caller drawing the frame through the logical presentation. */
int oracles_touch_sdl_frame(OraclesTouchScreen *screen, struct SDL_Renderer *renderer, struct SDL_Texture *frame, const struct SDL_FRect *part);

#endif
