/* The SDL 3 backend's own (sdl_backend.c, sdl_window.c, sdl_input.c, sdl_present.c): its state and what its files share. */
#ifndef ORACLES_SDL_BACKEND_INTERNAL_H
#define ORACLES_SDL_BACKEND_INTERNAL_H

#include "backends.h"
#include "compositor.h"
#include "safe_area.h"
#include "text_box_enlarge.h"
#include "touch_sdl.h"
#include "window_icon.h"

#include <SDL3/SDL.h>

#include <math.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PENDING_EVENTS 16u
/* The device starts once this much audio is queued, so that a device pull
 * never finds the queue empty during the first frames; above the limit the
 * host is ahead of the device and the chunk is dropped. */
#define AUDIO_PREBUFFER_MS 50u
#define AUDIO_QUEUE_LIMIT_MS 1000u /* only reached if the device stalls; rate control holds the queue near its target */
#define AXIS_THRESHOLD 12000

typedef struct sdl_backend {
    SDL_Keycode keys[ORACLES_BINDINGS];
    SDL_GamepadButton pad[4];   /* A, B, Select, Start */
    SDL_Keycode hotkey_keys[ORACLES_SDL_HOTKEY_SLOTS], hotkey_bind_b, hotkey_bind_a;   /* SDLK_UNKNOWN: none */
    SDL_GamepadButton hotkey_pad[ORACLES_SDL_HOTKEY_SLOTS];
    SDL_GamepadButton menu_pad;   /* Escape's on a controller, besides Guide; SDL_GAMEPAD_BUTTON_INVALID: none */
    SDL_GamepadAxis menu_trigger; /* or a trigger's (SDL_GAMEPAD_AXIS_INVALID: none), pressed as oracles_sdl_trigger_pressed says */
    int menu_trigger_held;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_Texture *layer;           /* the sub-pixel camera's layer (Link drawn apart), made at its first use */
    int layer_w, layer_h;
    /* The last frame as it was shown, which the pause shows: moved by the sub-pixel camera (the band, the patch and
     * the layer as `shown` placed them, the layer's texture holding them when `shown_layered`), or the texture as it is. */
    oracles_host_video_subpixel shown;
    int shown_layered, shown_subpixel;
    oracles_host_video_frame shown_box;   /* its text box shown again larger (box_w 0: none), without its pixels */
    oracles_host_video_frame shown_frame; /* the frame itself, without its pixels: its crop and its box */
    SDL_FRect shown_part;                 /* the part shown alone (the game's menus enlarged), when `shown_cropped` */
    int shown_cropped;
    int frame_width, frame_height;   /* the texture's: the session's surface */
    int logical_w, logical_h;        /* the renderer's logical size: the surface's, or the part shown alone */
    int fill;                        /* Scaling's Fill chosen */
    int filled;                      /* and presented: the window fullscreen at the last frame */
    int pause_menu;         /* Escape pauses rather than quits */
    int borrowed;           /* the window and the renderer are the launcher's: known from the start, never destroyed here */
    int subsystems;         /* the audio and controller subsystems a borrowed window started, to quit at the end */
    int window_closed;
    OraclesTouchScreen touch;   /* the game's buttons on a touch screen (Android), and those its fingers press */
    unsigned touch_buttons;
    int touch_enabled;
    int (*overlay)(void *, SDL_Renderer *);   /* over the frames while it returns 1 (oracles_sdl_options) */
    void *overlay_opaque;
    void (*on_suspend)(void *);   /* the host's store at a suspension (watch_suspend), and its argument */
    void *on_suspend_opaque;
    int suspended;   /* the watch saw a suspension, which the next poll hands to the host */
    SDL_AudioStream *audio;          /* bound to the device, which pulls from it */
    OraclesSdlPads pads;               /* every gamepad plugged in (oracles_sdl_pads_open) */
    uint32_t sample_rate_hz;
    int audio_started;
    unsigned audio_underruns;
    unsigned audio_drops;
    uint64_t last_chunk_ns;          /* when the previous chunk was queued, and what the queue held after it */
    Uint32 last_chunk_queued_bytes;
    uint64_t last_present_ns_spent;  /* the duration of the last SDL_RenderPresent */
    /* A game frame's presentation by step, summed and at most: the texture's upload, the drawing, SDL_RenderPresent. */
    uint64_t step_ns_total[3], step_ns_max[3];
    unsigned stepped_presents;
    oracles_sdl_underrun underrun_log[ORACLES_SDL_UNDERRUNS_LOGGED];
    Uint32 audio_peak_bytes;
    uint64_t audio_queued_sum_bytes;
    uint64_t audio_queued_samples_count;
    unsigned scale;
    int fullscreen;
    int vsync;
    unsigned presents;
    uint64_t first_present_ns, last_present_ns;
    uint64_t hold_started_ns, held_ns;   /* the pauses between two presents: no present waited then */
    char renderer_name[32];   /* the SDL renderer the session presents with, and whether it said it had vsync */
    int renderer_vsync;
    unsigned axis_buttons;  /* ORACLES_KEY_* currently held through the left stick */
    oracles_sdl_menu_source menu_source;   /* what asked for the pause menu last */
    oracles_sdl_menu_source last_input;    /* the device of the last input in the game (oracles_sdl_backend_last_input) */
    oracles_host_event pending[PENDING_EVENTS];
    unsigned pending_count;
    const oracles_host_video_frame *presenting;   /* the frame being presented, for the box drawn over it */
    SDL_FRect presenting_part;
    int presenting_cropped;
    unsigned boxes_enlarged;   /* frames whose text box was shown again larger (Text size Large) */
} sdl_backend;

/* sdl_backend.c: an event for the host, a button, a command, the quit; a suspension the watch saw; the logical
 * presentation set for a size; whether the window is fullscreen; the monotonic clock. */
void sb_push_button(sdl_backend *backend, unsigned button, int pressed);
void sb_push_command(sdl_backend *backend, int command);
void sb_push_event(sdl_backend *backend, oracles_host_event_type type);
void sb_push_quit(sdl_backend *backend);
bool SDLCALL sb_suspension(void *opaque, SDL_Event *e);
void sb_present_as(sdl_backend *backend, int logical_w, int logical_h);
int sb_window_fullscreen(const sdl_backend *backend);
uint64_t sb_sdl_monotonic_ns(void *opaque);
/* sdl_input.c: the keys, the controller's buttons, the item hotkeys and the menu's input taken from the options; an
 * event polled. */
void sb_resolve_bindings(sdl_backend *backend, const oracles_sdl_options *options);
void sb_resolve_hotkeys(sdl_backend *backend, const oracles_sdl_options *options);
void sb_resolve_menu(sdl_backend *backend, const oracles_sdl_options *options);
int sb_sdl_poll_event(void *opaque, oracles_host_event *event, int *has_event);
/* sdl_present.c: a texture's part drawn at a place of the frame; the band moved by the sub-pixel camera; the last
 * frame's text box shown again larger over the whole frame drawn at `to`, in the window's pixels; a frame presented. */
int sb_draw_subpixel(sdl_backend *backend, const oracles_host_video_subpixel *sp, int layered, const SDL_FRect *to);
/* The last frame's move (`shown`) drawn into `to`, in the window's pixels, placed at its whole pixels and cut to it. */
int sb_draw_subpixel_cut(sdl_backend *backend, const SDL_FRect *to);
int sb_put(SDL_Renderer *r, SDL_Texture *texture, const SDL_FRect *src, const SDL_FRect *dst, const SDL_FRect *to, float sx, float sy);
int sb_draw_shown_box(sdl_backend *backend, const SDL_FRect *to);
/* The pause's image of the last frame shown alone at `rect` (x, y, w, h in the frame) from now on, or whole (NULL). */
void sb_show_crop(sdl_backend *backend, const uint32_t rect[4]);
/* The pause's image of the last frame with Text size's box at `rect` (x, y, w, h in the frame) from now on, or none (NULL). */
void sb_show_box(sdl_backend *backend, const uint32_t rect[4], int bottom, uint32_t reserve_w, unsigned size);
int sb_sdl_present_frame(void *opaque, const oracles_host_video_frame *frame);

#endif
