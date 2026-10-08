/* Backends of the host facade provided by the launcher. */
#ifndef ORACLES_BACKENDS_H
#define ORACLES_BACKENDS_H

#include "oracles_host.h"

/* Order of the bindable buttons: right, left, up, down, A, B, Select, Start
 * (the bit order of ORACLES_KEY_*). */
#define ORACLES_BINDINGS 8
#define ORACLES_SDL_HOTKEY_SLOTS 4u

/* SDL's types, named without its header: the harness includes this file and has no SDL. */
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Gamepad;
union SDL_Event;

typedef struct oracles_sdl_options {
    unsigned scale;      /* initial integer scale of the window */
    int fullscreen;
    /* The launcher's window and renderer, for a game started from the home
     * screen: the session sizes the window to its surface and gives it back
     * as it found it, without destroying either.  NULL: its own window. */
    struct SDL_Window *window;
    struct SDL_Renderer *renderer;
    int vsync;           /* present in step with the display; the host then does not pace unless the present does not wait, the audio rate control absorbs the difference */
    /* SDL key names (SDL_GetKeyFromName) per button; NULL keeps the default, an empty name leaves the button without
     * a key (Controls gave it to another). */
    const char *key_names[ORACLES_BINDINGS];
    /* SDL game controller button names (SDL_GameControllerGetButtonFromString)
     * for A, B, Select, Start; the d-pad and the left stick always move. */
    const char *pad_names[4];
    /* Item hotkeys: a key and a controller button per slot, and the two keys that, held, make a slot's key
     * take the item of B or of A.  NULL: no key for that slot.  The game never sees these keys. */
    const char *hotkey_key_names[ORACLES_SDL_HOTKEY_SLOTS];
    const char *hotkey_pad_names[ORACLES_SDL_HOTKEY_SLOTS];
    const char *hotkey_bind_b, *hotkey_bind_a;
    /* Escape asks for the pause menu (ORACLES_HOST_EVENT_PAUSE) instead of ending the session: a game the home
     * screen started.  With --rom Escape still ends it. */
    int pause_menu;
    int touch_controls;   /* the game's buttons on a touch screen: always on Android, where the tests ask elsewhere */
} oracles_sdl_options;

/* Default names, in the order above, for the settings file. */
extern const char *const oracles_sdl_default_key_names[ORACLES_BINDINGS];
extern const char *const oracles_sdl_default_pad_names[4];
extern const char *const oracles_sdl_binding_names[ORACLES_BINDINGS];

/* Commands the SDL backend emits as ORACLES_HOST_EVENT_COMMAND. */
enum {
    ORACLES_COMMAND_TOGGLE_COLOUR_CORRECTION = 1,
    ORACLES_COMMAND_SAVE_STATE = 2,
    ORACLES_COMMAND_LOAD_STATE = 3,
    ORACLES_COMMAND_TOGGLE_ENHANCED = 4,  /* F3: the wide world or the framed core, in the Enhanced profile */
    /* A hotkey slot's key or button went down or up: ORACLES_COMMAND_HOTKEY + slot * 8 + modifier * 2 + pressed,
     * the modifier being 0 (none), 1 (the bind-B key is held), 2 (the bind-A key) or 3 (none, from a controller's
     * button: the hotbar shows the keys of the device used last). */
    ORACLES_COMMAND_HOTKEY = 0x100
};
#define ORACLES_COMMAND_HOTKEY_END (ORACLES_COMMAND_HOTKEY + (int)ORACLES_SDL_HOTKEY_SLOTS * 8)

/* SDL 3 window, keyboard, gamepad and audio. */
int oracles_sdl_backend_init(oracles_host_backend *backend, const oracles_sdl_options *options);
/* The refresh rate of the main display, in Hz, or 0 if unknown (initialises SDL's video subsystem). */
unsigned oracles_sdl_display_refresh_hz(void);
/* The SDL_WindowFlags every window of the launcher is created with (SDL_WINDOW_HIGH_PIXEL_DENSITY on macOS). */
uint64_t oracles_sdl_window_flags(void);
/* 1 where a window is always the whole screen (Android): the launcher and the games open fullscreen, and F11 and the
 * window's scale do nothing. */
int oracles_sdl_fullscreen_only(void);
/* A window of the launcher, centred, `width` x `height` in the window's coordinates; and its renderer, SDL's choice or
 * else the software one, presenting in step with the display when `vsync` is set. */
struct SDL_Window *oracles_sdl_create_window(int width, int height, uint64_t flags);
struct SDL_Renderer *oracles_sdl_create_renderer(struct SDL_Window *window, int vsync);
/* Pixels per unit of the window's coordinates: 2 on a Retina display, else 1 (Windows counts in pixels). */
float oracles_sdl_pixel_ratio(struct SDL_Window *window);
/* 1 when the screen the window is on (NULL: the main one) is nearer 4:3 than 16:9: the shape the Enhanced view takes
 * in fullscreen (docs/PLAYING.md). */
int oracles_sdl_screen_4_3(struct SDL_Window *window);
/* The window's coordinates per point of the launcher's sizes (settings.txt's window, its smallest): the display's
 * scale on Windows (1.25 at 125 %), where SDL 2 counted points and SDL 3 counts pixels; else 1.  NULL: the main
 * display's, for a window about to open. */
float oracles_sdl_point_scale(struct SDL_Window *window);
/* Sizes a window with a renderer to `width` x `height` pixels, whatever the display's scale. */
void oracles_sdl_size_window(struct SDL_Window *window, int width, int height);
/* The first gamepad plugged in, opened, or NULL. */
struct SDL_Gamepad *oracles_sdl_open_gamepad(void);
/* The SDL_GamepadButton of a button event as settings.txt names it: a face button labelled A, B, X or Y is that
 * letter's, as SDL 2 had it, whatever its place (a Nintendo controller's A is on the right). */
int oracles_sdl_pad_button(const union SDL_Event *event);
/* Moves a window so that its centre is at (centre_x, centre_y), kept on its display's usable area. */
void oracles_sdl_place_window(struct SDL_Window *window, int centre_x, int centre_y);
/* 1 when the session ended because the window was closed, not by Escape. */
int oracles_sdl_backend_window_closed(const oracles_host_backend *backend);
/* Frames presented and the seconds between the first and the last (with vsync, the rate the display imposed), and
 * the SDL renderer obtained with whether it reported vsync when the session started ("unknown" before start). */
void oracles_sdl_backend_present_report(const oracles_host_backend *backend, unsigned *presents, double *seconds,
                                        const char **renderer, int *renderer_vsync);
void oracles_sdl_backend_release(oracles_host_backend *backend);

/* For the pause menu, between two frames: the window, its renderer, and the texture that holds the last image of the
 * game with its size. */
void oracles_sdl_backend_pause_view(const oracles_host_backend *backend, struct SDL_Window **window, struct SDL_Renderer **renderer,
                                    struct SDL_Texture **frame, int *width, int *height);
/* The sound stops and its queue empties; it starts again with the next frames, from its prebuffer. */
void oracles_sdl_backend_suspend_audio(const oracles_host_backend *backend);
/* The pause menu holds the session from a call with 1 to a call with 0, at its resume: that time is no
 * presentation's, and the rate of oracles_sdl_backend_present_report leaves it out; a suspension seen during it (Android's
 * background) brings no second pause. */
void oracles_sdl_backend_hold(const oracles_host_backend *backend, int held);
/* The keys and the buttons of `options` from now on (Controls changed them during the pause). */
void oracles_sdl_backend_rebind(const oracles_host_backend *backend, const oracles_sdl_options *options);
/* The window was closed during the pause: the session ends as if closed in play. */
void oracles_sdl_backend_set_window_closed(const oracles_host_backend *backend);

/* Times the audio device found its queue empty (a heard gap) and chunks
 * dropped because the host was ahead of the device, since start. */
void oracles_sdl_backend_audio_report(const oracles_host_backend *backend, unsigned *underruns, unsigned *drops,
                                      unsigned *peak_ms, unsigned *average_ms);

/* An underrun, where the device found its queue empty: the frames presented before it, the time since the previous
 * chunk was queued and what that chunk left queued (a longer wait than the queue drains it), and the duration of the
 * last presentation (a present that stalls the loop). */
typedef struct oracles_sdl_underrun {
    unsigned frame;
    uint64_t since_previous_chunk_ns;
    unsigned previous_chunk_left_ms;
    uint64_t last_present_ns;
} oracles_sdl_underrun;
#define ORACLES_SDL_UNDERRUNS_LOGGED 16u
/* The run's first underruns, at most ORACLES_SDL_UNDERRUNS_LOGGED: sets *log and returns how many. */
unsigned oracles_sdl_backend_underruns(const oracles_host_backend *backend, const oracles_sdl_underrun **log);

/* Per-user settings directory of this platform (ORACLES_SETTINGS_DIR when set), with a trailing separator; 0 if
 * unavailable. */
int oracles_sdl_settings_dir(char *out, size_t capacity);

/* No window, no audio, a deterministic clock: for tests and headless runs. */
int oracles_null_backend_init(oracles_host_backend *backend);
void oracles_null_backend_release(oracles_host_backend *backend);

#endif
