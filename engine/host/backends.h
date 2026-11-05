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
struct SDL_FRect;
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
    /* The controller button, or trigger (oracles_sdl_menu_input), that opens the pause menu, as Guide does too, and
     * does nothing without one (--rom): NULL keeps the default (oracles_sdl_default_menu_pad_name), an
     * empty name leaves Guide alone.  A button the game's buttons, the d-pad or a hotkey slot already have stays theirs. */
    const char *menu_pad_name;
    /* Escape asks for the pause menu (ORACLES_HOST_EVENT_PAUSE) instead of ending the session: a game the home
     * screen started.  With --rom Escape still ends it. */
    int pause_menu;
    int touch_controls;   /* the game's buttons on a touch screen: always on Android, where the tests ask elsewhere */
    int fill;             /* Scaling's Fill: in fullscreen the image fills the screen in its proportions; a window keeps whole scales */
    int screen_margin;    /* screen_margin=, an OraclesScreenMargin: the band of the screen the image and the touch controls keep to */
    /* Drawn over each game frame before it is presented, while it returns 1 (the toast over the first game started with
     * a controller): NULL, nothing. */
    int (*overlay)(void *opaque, struct SDL_Renderer *renderer);
    void *overlay_opaque;
} oracles_sdl_options;

/* Default names, in the order above, for the settings file. */
extern const char *const oracles_sdl_default_key_names[ORACLES_BINDINGS];
extern const char *const oracles_sdl_default_pad_names[4];
extern const char *const oracles_sdl_default_menu_pad_name;
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
 * in fullscreen (docs/REFERENCE.md). */
int oracles_sdl_screen_4_3(struct SDL_Window *window);
/* The size of that screen's desktop mode in pixels, which the far view fits to; 0 when SDL gives none. */
int oracles_sdl_screen_size(struct SDL_Window *window, unsigned *width, unsigned *height);
/* The window's coordinates per point of the launcher's sizes (settings.txt's window, its smallest): the display's
 * scale on Windows (1.25 at 125 %), where SDL 2 counted points and SDL 3 counts pixels; else 1.  NULL: the main
 * display's, for a window about to open. */
float oracles_sdl_point_scale(struct SDL_Window *window);
/* Sizes a window with a renderer to `width` x `height` pixels, whatever the display's scale. */
void oracles_sdl_size_window(struct SDL_Window *window, int width, int height);
/* The gamepads plugged in, every one open.  A device may count several (a handheld's own controls and another input
 * the system takes for a gamepad, a phone's and a Bluetooth pad), and SDL gives the buttons of a pad that is not open
 * as keys, not as the pad's: on Android its A as Enter (Start in the game) and its B as Escape (the pause). */
#define ORACLES_SDL_PADS 8u
typedef struct OraclesSdlPads {
    struct SDL_Gamepad *pad[ORACLES_SDL_PADS];
    unsigned count;
} OraclesSdlPads;
/* Closes the pads unplugged (a session may have consumed their removal) and opens every one plugged in not yet open. */
void oracles_sdl_pads_open(OraclesSdlPads *pads);
/* A pad plugged in is opened, one unplugged closed; other events are left. */
void oracles_sdl_pads_event(OraclesSdlPads *pads, const union SDL_Event *event);
void oracles_sdl_pads_close(OraclesSdlPads *pads);
/* "N open: name, name" for a report. */
void oracles_sdl_pads_describe(const OraclesSdlPads *pads, char *out, size_t capacity);
/* The menu's controller input as settings.txt names it (menu_pad=): an SDL_GamepadButton in *button, or a trigger,
 * "lefttrigger" or "righttrigger", its SDL_GamepadAxis in *axis, the other invalid; both for an empty name.  0 for a
 * name that is neither. */
int oracles_sdl_menu_input(const char *name, int *button, int *axis);
/* A trigger read as a button, at its axis' `value`: down past half its travel, up below a quarter, *held following;
 * 1 when this value presses it. */
int oracles_sdl_trigger_pressed(int value, int *held);
/* The trigger `axis` (SDL_GAMEPAD_AXIS_INVALID: none) read as one button over every open controller: an axis event's
 * value, or another controller's as far down, as oracles_sdl_trigger_pressed reads it.  1 when this event presses it;
 * a controller letting go while another still holds it lets nothing go. */
int oracles_sdl_trigger_moved(const union SDL_Event *event, int axis, int *held);
/* Whether trigger `axis` (SDL_GAMEPAD_AXIS_INVALID: none) of an open controller is past a quarter of its travel now:
 * where *held starts after a pause or a game, whose moves the next screen did not see. */
int oracles_sdl_trigger_held(int axis);
/* The SDL_GamepadButton of a button event as settings.txt names it: a face button labelled A, B, X or Y is that
 * letter's, as SDL 2 had it, whatever its place (a Nintendo controller's A is on the right). */
int oracles_sdl_pad_button(const union SDL_Event *event);
/* Moves a window so that its centre is at (centre_x, centre_y), kept on its display's usable area. */
void oracles_sdl_place_window(struct SDL_Window *window, int centre_x, int centre_y);
/* The game's presentation, an SDL_RendererLogicalPresentation and the frame's SDL_ScaleMode, for a window fullscreen or
 * not and Scaling on Fill or not: the largest whole scale sampled to the nearest pixel (Sharp, and Fill in a window), or,
 * Fill in fullscreen, the largest size in the image's proportions sampled for pixel art. */
void oracles_sdl_presentation(int fullscreen, int fill, int *presentation, int *scale_mode);
/* Vsync changed during the pause: the presentation in step with the display, or not, from the next frame. */
void oracles_sdl_backend_set_vsync(const oracles_host_backend *backend, int vsync);
/* Scaling changed during the pause: the frame's sampling at once, the presentation from the next frame. */
void oracles_sdl_backend_set_fill(const oracles_host_backend *backend, int fill);
/* The window changed during the pause: fullscreen, or a window of `scale` times the frame around its centre, kept on
 * its display; nothing where the window is always the whole screen. */
void oracles_sdl_backend_set_window(const oracles_host_backend *backend, int fullscreen, unsigned scale);
/* The screen margin changed during the pause: its band read again, the image and the touch controls in it from the
 * next frame. */
void oracles_sdl_backend_set_screen_margin(const oracles_host_backend *backend, int margin);
/* Scaling's Fill chosen, and in effect at the last frame (the window fullscreen then). */
void oracles_sdl_backend_scaling(const oracles_host_backend *backend, int *chosen, int *in_effect);
/* The whole scale the frame is shown at on the screen now, in the band the screen margin leaves it: the largest whole
 * number of the screen's pixels a pixel of the frame takes there (Fill's 2.67 counts 2), at least 1; 0 before the
 * session started. */
unsigned oracles_sdl_backend_whole_scale(const oracles_host_backend *backend);
/* 1 when the session ended because the window was closed, not by Escape. */
int oracles_sdl_backend_window_closed(const oracles_host_backend *backend);
/* Frames presented and the seconds between the first and the last (with vsync, the rate the display imposed), and
 * the SDL renderer obtained with whether it reported vsync when the session started ("unknown" before start). */
void oracles_sdl_backend_present_report(const oracles_host_backend *backend, unsigned *presents, double *seconds,
                                        const char **renderer, int *renderer_vsync);
/* A game frame's presentation by step, on average and at most, in milliseconds: the texture's upload, the drawing
 * (the frame and the touch controls), and SDL_RenderPresent. */
void oracles_sdl_backend_present_steps(const oracles_host_backend *backend, double average_ms[3], double max_ms[3]);
void oracles_sdl_backend_release(oracles_host_backend *backend);

/* For the pause menu, between two frames: the window, its renderer, and the texture that holds the last image of the
 * game with its size. */
void oracles_sdl_backend_pause_view(const oracles_host_backend *backend, struct SDL_Window **window, struct SDL_Renderer **renderer,
                                    struct SDL_Texture **frame, int *width, int *height);
/* For the pause menu, between two frames: the game's last image drawn into `image`, in the window's pixels (the
 * logical presentation lifted), as its last frame showed it: the sub-pixel camera's band at its fraction of a pixel,
 * the patch and Link's layer at their places, the fixed rectangles unmoved; the frame as it is when the last frame was
 * shown so (no sub-pixel camera, the touch controls' or the screen margin's drawing); then the text box Text size
 * showed again larger over it, at that size over the image.  A part shown alone (the game's menus enlarged) is drawn
 * alone so, and the image's size is that part's (oracles_sdl_backend_pause_size): the frame's otherwise.  The pause
 * changing the large menus changes the part the image shows (oracles_sdl_backend_pause_crop: x, y, w, h in the frame,
 * or NULL for the whole frame). */
int oracles_sdl_backend_pause_image(const oracles_host_backend *backend, const struct SDL_FRect *image);
void oracles_sdl_backend_pause_size(const oracles_host_backend *backend, int *width, int *height);
void oracles_sdl_backend_pause_crop(const oracles_host_backend *backend, const uint32_t rect[4]);
/* The pause changing Text size changes the box it shows again larger (oracles_sdl_backend_pause_box: x, y, w, h in the
 * frame, or NULL for none, with the frame's box_bottom, box_reserve_w and box_size), and the part a reserve widens. */
void oracles_sdl_backend_pause_box(const oracles_host_backend *backend, const uint32_t rect[4], int bottom, uint32_t reserve_w, unsigned size);
/* The last frame's pixels drawn again (the frame's size, its crop, box and sub-pixel move kept): the pause changing a
 * setting that the frame's own pixels show (the status bar's layout) shows it in the image at once. */
void oracles_sdl_backend_pause_pixels(const oracles_host_backend *backend, const uint32_t *pixels);
/* And the rects of them the sub-pixel camera holds still (the text box drawn again at another size), where the last
 * frame was moved by it. */
void oracles_sdl_backend_pause_fixed(const oracles_host_backend *backend, const uint32_t (*rects)[4], unsigned count);
/* What asked for the pause menu last: a key (Escape), a controller (Guide, the menu's button or trigger), a finger
 * (the touch controls' pause) or Android's Back, which a finger or a controller gives too; NONE before any, and for a
 * suspension (Android's background), which no device asked. */
typedef enum oracles_sdl_menu_source {
    ORACLES_SDL_MENU_FROM_NONE,
    ORACLES_SDL_MENU_FROM_KEY,
    ORACLES_SDL_MENU_FROM_PAD,
    ORACLES_SDL_MENU_FROM_TOUCH,
    ORACLES_SDL_MENU_FROM_BACK
} oracles_sdl_menu_source;
oracles_sdl_menu_source oracles_sdl_backend_menu_source(const oracles_host_backend *backend);
/* The device of the last input in the game, a key (Android's Back apart), a controller's button or its stick, or a
 * finger, named as a menu's source; NONE before any. */
oracles_sdl_menu_source oracles_sdl_backend_last_input(const oracles_host_backend *backend);
/* The sound stops and its queue empties; it starts again with the next frames, from its prebuffer. */
void oracles_sdl_backend_suspend_audio(const oracles_host_backend *backend);
/* The pause menu holds the session from a call with 1 to a call with 0, at its resume: that time is no
 * presentation's, and the rate of oracles_sdl_backend_present_report leaves it out; a suspension seen during it (Android's
 * background) brings no second pause; the screen margin's band is read again at the resume, for a safe area or a display
 * that changed meanwhile. */
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
