/* The launcher's home screen in its window: `the-oracles-project` without --rom opens
 * it.
 *
 * The window opens 16:9, resizable down to 960x540; the scene follows its
 * size, and its shape the layout (ui_layout.h), 16:9 or 4:3.  Cartridge opens the game's page (ui_page.h).  Keyboard: arrows, Enter or Space, Escape or Backspace; controller:
 * d-pad, A, B; mouse: hover and click.  F11 toggles fullscreen.  The loop
 * waits for events, and draws only while something changes or moves.
 *
 * What the screen does with the games goes through the host: the ROMs'
 * state, a file dropped on the window, a game started in the same window. */
#ifndef ORACLES_LAUNCHER_HOME_H
#define ORACLES_LAUNCHER_HOME_H

#include "ui_draw.h"
#include "ui_home_nav.h"

#include <stddef.h>

struct SDL_Window;
struct SDL_Renderer;

#define ORACLES_HOME_DEFAULT_WIDTH 1280
#define ORACLES_HOME_DEFAULT_HEIGHT 720
#define ORACLES_HOME_MIN_WIDTH 960
#define ORACLES_HOME_MIN_HEIGHT 540

typedef struct OraclesHomeHost {
    void *opaque;
    /* The games' ROMs as they are now, into nav->games, and Display's screen, the one `window` is on (NULL: a 1080p
     * screen, for a capture drawn offscreen): at the start, after a drop, after a session and when the window moves to
     * another display. */
    void (*refresh)(void *opaque, OraclesHomeNav *nav, struct SDL_Window *window);
    /* A file dropped on the window, with the game whose page is open (ORACLES_HOME_GAME_*), for which it is a ROM (a fan
     * game's base ROM, or its patch), or -1: the entry it was given to, or -1; *message is what to show. NULL: drops are
     * ignored. */
    int (*drop)(void *opaque, const char *path, int page_game, char *message, size_t capacity);
    /* The choices of the game's page changed: the host writes them. */
    void (*store)(void *opaque, const OraclesHomeNav *nav);
    /* The game's page: the system's file dialog for game `game` (ORACLES_HOME_GAME_*: a fan game's base ROM is the Ages
     * ROM, the same setting) or for a fan game's patch, opened, and its answer (an OraclesDialogKind, an
     * OraclesDialogResult, the path chosen and whether it is a copy made for it, to remove if refused), which comes
     * later; and the folder of its save. */
    void (*choose_rom)(void *opaque, int game, struct SDL_Window *window);
    void (*choose_patch)(void *opaque, int game, struct SDL_Window *window);
    void (*file_chosen)(void *opaque, int game, int kind, int result, const char *path, int copied, char *message, size_t capacity);
    void (*open_folder)(void *opaque, int game, char *message, size_t capacity);
    /* Mods: the mods folder read into nav->mods and nav->mods_folder, with each Oracle's active mods (the refresh reads
     * it too), when the page opens; the folder in the system's file manager, made when it is not there. */
    void (*read_mods)(void *opaque, OraclesHomeNav *nav);
    void (*open_mods)(void *opaque, char *message, size_t capacity);
    /* Plays the game in the launcher's window until the player leaves it, the pause menu drawn with `draw`.  Returns
     * 1 when the window was closed meanwhile (the launcher then ends too); *message is what to show, empty when all
     * went well. */
    int (*start)(void *opaque, OraclesHomeCommand game, struct SDL_Window *window, struct SDL_Renderer *renderer,
                 OraclesUiDraw *draw, char *message, size_t capacity);
} OraclesHomeHost;

/* Runs the home screen until Exit or the window is closed.  *width and *height
 * are the window's size in, and the size the player left it at out. Returns
 * the process's exit status. */
int oracles_home_run(const OraclesHomeHost *host, int *width, int *height);

/* Draws the home screen at rest (with `frame`, a binary PPM of a game's image, the pause menu over it, Ages paused in
 * Display's profile), in `width` x `height` pixels, after the
 * navigation `inputs` (a comma-separated list of up, down, left, right, ok,
 * back; NULL or empty: the default state), and writes it as a binary PPM.
 * It draws offscreen with SDL's software renderer: no display is needed.
 * Returns 0, or 1 with the reason in *error. */
int oracles_home_screenshot(const OraclesHomeHost *host, const char *path, int width, int height, const char *inputs,
                            const char *frame, char *error, size_t capacity);

#endif
