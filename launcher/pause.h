/* The pause menu of a game the home screen started: Escape in play holds
 * the session between two frames
 * and lays the menu over the game's last image, darkened, at the size of the
 * game's window, in the layout the window's shape chooses (ui_layout.h).
 * Resume, Save state, Load state, Controls, Display, Cheats (for a game whose
 * profile has them), Quit to launcher; in the 16:9 layout under 960 pixels
 * wide, Controls, Display and Cheats stay in the launcher.
 *
 * Nothing is rasterised during the session: the glyphs of every screen the
 * pause shows are rasterised for the window's size before the first frame
 * (oracles_pause_prepare), again at a pause only if the window's size or its
 * band has changed since, and the rasters stay frozen while the session lasts. */
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
    /* The settings changed: the keys, the colour correction, the HUD, the scaling and the screen margin apply at once,
     * the swim's hold and the item hotkeys from the frame the game resumes on.  Returns the screen margin in effect (an
     * OraclesScreenMargin), which the menu's band follows, as it follows the window's size. */
    int (*settings_changed)(void *opaque);
    /* Cheats changed the running game's cheats (a mask): they apply from the frame the game resumes on. */
    void (*cheats_changed)(void *opaque, unsigned cheats);
    /* Display's Window was chosen (window_scale=: 2 to 4, 0 fullscreen): it applies at once. */
    void (*window_chosen)(void *opaque, int window);
    int core;   /* the running game's core, an OraclesCoreKind: Display shows it, dimmed */
    int ghosts; /* the running game's neighbour workers, 0 auto, 1 or 2: Display shows them, dimmed */
    int device; /* the device that opened the pause (an OraclesInputDevice), which its key hints follow; -1 when not known */
    unsigned cheats;   /* the running game's cheats, --cheats' perhaps, which its Cheats page starts from */
    int has_cheats;    /* its profile has them: else the menu has no Cheats */
    /* Draws the game's last image into `image`, in the window's pixels, as its last frame showed it (the sub-pixel
     * camera's band at its fraction of a pixel, Link at his place, Text size's box enlarged); NULL: the frame drawn there
     * as it is. */
    int (*game_image)(void *opaque, SDL_Renderer *renderer, const SDL_FRect *image);
    int has_swim, has_hotkeys;   /* its profile has the swim's hold, the item hotkeys: else an Oracle's Controls dims them */
    /* Its window as window_scale= counts it (2 to 4, 0 fullscreen), F11's toggle included, which Display shows, -2 for
     * a window no choice is (its size window_w x window_h); -1 when Display cannot change it.  The windows (1 <<
     * Display's Window value) whose pause would keep only its own entries, which Display refuses. */
    int window, window_w, window_h;
    unsigned windows_refused;
    int has_text_instant;   /* its profile has the instant text: else Display › Text dims Text speed */
    /* The size of that image, in the game's pixels: its part shown alone (the game's menus enlarged, which Display may
     * change here), or the whole frame; NULL: the frame's. */
    void (*game_size)(void *opaque, int *width, int *height);
    int by_key;   /* the player opened it by its key (Escape, the controller's, the touch controls' pause, Android's Back) */
} OraclesPauseSession;

typedef enum OraclesPauseResult {
    ORACLES_PAUSE_RESUME,
    ORACLES_PAUSE_QUIT,              /* Quit to launcher */
    ORACLES_PAUSE_WINDOW_CLOSED
} OraclesPauseResult;

typedef struct OraclesPause OraclesPause;

/* A pause for `game`'s session (ORACLES_HOME_GAME_*) running in `playing`, drawn with the home screen's `draw`, its settings read and written
 * through `host` (refresh, store); `aspect`, the settings' aspect=, which the layout follows; `margin`, the session's
 * screen margin (an OraclesScreenMargin), whose band the menu keeps to, which Display changes at once; `input`, the
 * home screen's device used last and whether a key was pressed there, which the pauses follow and keep. */
OraclesPause *oracles_pause_create(OraclesUiDraw *draw, const OraclesHomeHost *host, int game, OraclesProfile playing, int aspect, int margin,
                                   const OraclesHomeNav *input);
/* The device used last in the pauses (the home screen's when none opened) and whether a key was pressed, into `nav`. */
void oracles_pause_input(const OraclesPause *pause, OraclesHomeNav *nav);
void oracles_pause_destroy(OraclesPause *pause);

/* Before the session's first frame: the glyphs for the renderer's output, and the rasters frozen.  Says on stderr
 * what it rasterised and how long it took. */
void oracles_pause_prepare(OraclesPause *pause, SDL_Renderer *renderer);
/* The pause, until the player resumes or leaves; `frame` holds the game's last image, `width` x `height`. */
OraclesPauseResult oracles_pause_run(OraclesPause *pause, SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *frame,
                                     int width, int height, const OraclesPauseSession *session);
/* The reminder over this game, as settings.txt's menu_tip= has it due: from the first frame that shows the status bar in
 * play, centred under it, the key that opens the menu on the device used last ("L3 opens the menu",
 * oracles_controls_menu_tip, `menu_pad` settings.txt's menu_pad=) for about 4 s whatever the player presses, then
 * faded out over 0.6 s; it goes as the pause opens over it, and never takes an input.  Its glyphs are the pause's,
 * prepared beforehand. */
void oracles_pause_reminder(OraclesPause *pause, const char *menu_pad);
/* A line over the game from its first frame, as the reminder shows (before it, when both are due): what the game
 * started again for its savestate changed. */
void oracles_pause_notice(OraclesPause *pause, const char *text);
/* A notice or the reminder waits or shows. */
int oracles_pause_overlay_pending(const OraclesPause *pause);
/* It waits for its first frame, or shows. */
int oracles_pause_reminder_pending(const OraclesPause *pause);
/* It was drawn over a frame of the game: settings.txt counts it. */
int oracles_pause_reminder_drawn(const OraclesPause *pause);
/* The frame about to be presented, for the reminder: whether it shows the status bar in play (the session says so once
 * the bar has been in play for two seconds of frames in a row, not for a flash of it before a cutscene), the device used
 * last in the game (an OraclesInputDevice; -1 none yet: the one the pause knows), and the game's image as the session
 * presents it, its size in the game's pixels and Fill. */
typedef struct OraclesPauseFrame {
    int in_play;
    int device;
    int width, height;
    int fill;
} OraclesPauseFrame;
/* Draws the reminder over that frame (the SDL backend's overlay, through the session): 1 while it waits or shows, 0 once
 * it is gone. */
int oracles_pause_overlay(OraclesPause *pause, SDL_Renderer *renderer, const OraclesPauseFrame *frame);
/* Glyphs rasterised during the session's pauses (none when prepared beforehand), for the session's report. */
unsigned oracles_pause_late_glyphs(const OraclesPause *pause);

/* The pause's image on an output of `out_width` x `out_height`: the game's `frame` at its whole scale (with `fill`, as
 * Scaling's Fill shows it in fullscreen: the whole output in its proportions), darkened, and the screen of `nav` over
 * it, in its layout (nav->layout and nav->narrow set for that output); the capture draws it offscreen too.  With
 * `session` and its game_image, the game's image is drawn by it, `frame` otherwise.  Returns 1 while a transition runs. */
int oracles_pause_paint(OraclesUiDraw *draw, SDL_Renderer *renderer, SDL_Texture *frame, int width, int height, int fill,
                        int out_width, int out_height, OraclesUiHome *view, const OraclesHomeNav *nav, double now_ms,
                        const OraclesPauseSession *session);

#endif
