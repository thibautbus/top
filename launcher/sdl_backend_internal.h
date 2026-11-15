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
    uint32_t *layer_pixels;       /* sb_upload_layer's, the whole texture's */
    size_t layer_pixels_size;
    int layer_margin, layer_bend; /* sb_upload_layer's, for the filter it was written for (oracles_sdl_filter_bend) */
    int layer_written;            /* the texture written whole once since it was made */
    SDL_Texture *panels;          /* the status bar's floating panels apart from the frame (sb_upload_panels), made at their first use */
    int panels_w, panels_h, panels_shown;
    /* The panels in groups, those whose rings meet in one (sb_upload_panels): each group's part drawn (x, y, w, h); their
     * texels' colour straight (for a shader that keeps it to the coverage of the texel it reads), not premultiplied. */
    unsigned panels_groups;
    int panels_draw[ORACLES_HOST_SUBPIXEL_PANELS][4];
    int panels_straight;
    /* The near view's panels at half the game's scale (oracles_host_video_subpixel's panels_scale 2): a group a panel,
     * `panels_draw` its place in their layer twice the frame's size, kept to its corner, drawn after the picture
     * (sb_draw_half_panels); in the texture each alone with as much around it as the filter reads, from `panels_at`,
     * as far as `panels_bend` drawn around it. */
    int panels_half;
    uint8_t panels_anchor[ORACLES_HOST_SUBPIXEL_PANELS];
    int panels_at[ORACLES_HOST_SUBPIXEL_PANELS][2];
    int panels_bend;
    uint32_t *panels_pixels;      /* sb_upload_panels's, a group's part as the texture takes it */
    uint8_t *panels_reach;
    size_t panels_pixels_size;
    uint32_t layer_part_w, layer_part_h;   /* the size of Link's part written last, the rest of `layer_pixels` 0 */
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
    int touch_vibration;   /* a tick as a finger presses a touch control (oracles_touch_sdl_tick) */
    unsigned touch_ticks;  /* the ticks asked, the last at the event time touch_tick_ns */
    uint64_t touch_tick_ns;
    int (*overlay)(void *, SDL_Renderer *);   /* over the frames while it returns 1 (oracles_sdl_options) */
    void *overlay_opaque;
    void (*on_suspend)(void *);   /* the host's store at a suspension (watch_suspend), and its argument */
    void *on_suspend_opaque;
    int suspended;   /* the watch saw a suspension, which the next poll hands to the host */
    SDL_AudioStream *audio;          /* bound to the device, which pulls from it */
    OraclesSdlPads pads;               /* every gamepad plugged in (oracles_sdl_pads_open) */
    unsigned pads_at_start;            /* those open when the game started */
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
#ifdef ORACLES_RETROACHIEVEMENTS
    /* The unlock's sound, rendered at the sound's opening, and how far it played (unlock_frames: it does not); the chunk
     * it is mixed into while it plays, a copy of the core's. */
    float *unlock;
    size_t unlock_frames, unlock_cursor;
    int16_t *unlock_mixed;
    size_t unlock_mixed_capacity;   /* in samples */
#endif
    uint64_t audio_queued_sum_bytes;
    uint64_t audio_queued_samples_count;
    unsigned scale;
    int fullscreen;
    int vsync;
    unsigned presents;
    uint64_t first_present_ns, last_present_ns;
    uint64_t hold_started_ns, held_ns;   /* the pauses between two presents: no present waited then */
    struct OraclesSdlFilter *filter;   /* ORACLES_FILTER over the game's picture, on the GPU renderer */
    /* The last pixels uploaded, the frame's and the layer's (their owners keep them while the game is held), uploaded
     * again into the textures made on a new renderer (oracles_sdl_renderer_swap). */
    unsigned swaps;   /* renderers made again during the session */
    int renderer_lost;   /* told the renderer went, not yet the one that follows */
    const void *last_pixels;
    size_t last_pitch;
    const uint32_t *last_layer, *last_patch, *last_panels;
    char renderer_name[96];   /* the SDL renderer the session presents with, and whether it said it had vsync */
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
/* Link's layer and the patch under him, placed as `at` places them, into the layer's texture, with the frame's pixels
 * around them (`frame`, `frame_pitch` bytes a line; NULL: none), made or made again as their pitch needs: 1 when done. */
int sb_upload_layer(sdl_backend *backend, const uint32_t *layer, const uint32_t *patch, const oracles_host_video_subpixel *at, const uint32_t *frame,
                    size_t frame_pitch);
int sb_put(SDL_Renderer *r, SDL_Texture *texture, const SDL_FRect *src, const SDL_FRect *dst, const SDL_FRect *to, float sx, float sy);
int sb_upload_panels(sdl_backend *backend, const oracles_host_video_subpixel *sp);
/* The layer, the patch and the panels of `frame`'s sub-pixel move uploaded, the move kept for the pause (`shown`). */
void sb_take_subpixel(sdl_backend *backend, const oracles_host_video_frame *frame);
int sb_draw_shown_box(sdl_backend *backend, const SDL_FRect *to);
/* The panels at half the game's scale last uploaded, over the picture drawn at `to` (the frame's place, in the window's
 * pixels), at the unit oracles_floating_screen_place gives, through the filter the picture was drawn through (their
 * texels read at that unit, Scanlines', CRT's and LCD's masks running on from the picture's pixels, xBR, HQ2x and
 * OmniScale on their own texels, CRT's curve over the picture's place), the filter set aside after them; `direct` when the window's
 * pixels are drawn to already, else the logical presentation is set aside for them.  1 when none are to draw. */
int sb_draw_half_panels(sdl_backend *backend, const SDL_FRect *to, int direct);
/* The pause's image of the last frame shown alone at `rect` (x, y, w, h in the frame) from now on, or whole (NULL). */
void sb_show_crop(sdl_backend *backend, const uint32_t rect[4]);
/* The pause's image of the last frame with Text size's box at `rect` (x, y, w, h in the frame) from now on, or none (NULL). */
void sb_show_box(sdl_backend *backend, const uint32_t rect[4], int bottom, uint32_t reserve_w, unsigned size);
int sb_sdl_present_frame(void *opaque, const oracles_host_video_frame *frame);

#endif
