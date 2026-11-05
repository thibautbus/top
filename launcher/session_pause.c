/* A game session's pause menu (pause.h): what it reads of the running game, and the settings it changes there,
 * applied at once or from the frame the game resumes on. */
#include "session_internal.h"

#include "home_choices.h"
#include "home_input.h"
#include "hotkeys_session.h"
#include "view.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static int pause_save(void *opaque, char *message, size_t capacity) { return oracles_session_save_state(opaque, message, capacity); }
static int pause_load(void *opaque, char *message, size_t capacity) { return oracles_session_load_state(opaque, message, capacity); }

static void pause_state_time(void *opaque, char *out, size_t capacity)
{
    const session *s = opaque;
    struct stat file;
    const struct tm *local = stat(s->state_path, &file) == 0 ? localtime(&file.st_mtime) : NULL;
    if (!local || !strftime(out, capacity, "%H:%M", local)) { if (capacity) out[0] = 0; }
}

/* The item hotkeys' keys and buttons, and the two keys that bind from a button, handed to the backend while the
 * hotkeys are on; none otherwise, as when they were never on. */
void oracles_session_hotkey_keys(session *s, int on)
{
    const oracles_settings *prefs = s->settings;
    for (unsigned i = 0; i < ORACLES_HOTKEY_SLOTS; i++) {
        s->sdl.hotkey_key_names[i] = on ? prefs->hotkey_key_names[i] : NULL;
        s->sdl.hotkey_pad_names[i] = on ? prefs->hotkey_pad_names[i] : NULL;
    }
    s->sdl.hotkey_bind_b = on ? prefs->hotkey_bind_b : NULL;
    s->sdl.hotkey_bind_a = on ? prefs->hotkey_bind_a : NULL;
}

/* Controls or Display changed the settings: the keys, the colour correction, vsync, the scaling, the status bar's width,
 * the large menus, the text size and the screen margin apply at once, the swim's hold, the text speed, the rooms never visited and the item hotkeys from the frame the game resumes on (the item
 * hotkeys turned off once their policy is at rest), the rest at the next Play (the window: pause_window_chosen).  The
 * colour correction, the swim's hold and the item hotkeys apply when the pause changed them: --colour-correction,
 * --swim-hold, --text-speed and --item-hotkeys hold until then; --screen-margin holds for the run. */
static int pause_settings_changed(void *opaque)
{
    session *s = opaque;
    OraclesSessionApplied applied = { s->colour_setting, s->sdl.fill, s->view ? oracles_enhanced_view_hud(s->view) : -1, s->sdl.screen_margin,
                                      s->view && oracles_compat_host_text_box(s->profile) ? (int)oracles_enhanced_view_text_size(s->view) : -1,
                                      s->swim_setting, s->hotkeys_game, s->hotkeys_setting, s->text_speed_setting, s->vsync_setting,
                                      s->view ? s->menus_large : -1, s->view && !s->unvisited_option ? oracles_enhanced_view_unvisited_hidden(s->view) : -1 };
    const unsigned changed = oracles_session_settings_changed(s->settings, s->screen_margin_option, s->text_size_option, &applied);
    if (changed & ORACLES_SESSION_CHANGED_COLOUR) {
        s->colour_applied = s->colour_setting = applied.colour;
        oracles_core_set_colour_correction(s->core, s->colour_applied);
        fprintf(stderr, "oracles: colour correction %s\n", s->colour_applied ? "on" : "off");
    }
    if (changed & ORACLES_SESSION_CHANGED_FILL) {
        s->sdl.fill = applied.fill;
        oracles_sdl_backend_set_fill(&s->backend, s->sdl.fill);
        fprintf(stderr, "oracles: scaling %s\n", s->sdl.fill ? "fill" : "sharp");
    }
    if (changed & ORACLES_SESSION_CHANGED_HUD) {
        oracles_enhanced_view_set_hud(s->view, applied.hud);
        if (oracles_enhanced_view_redraw_hud(s->view)) {   /* the pause's image of a world frame, its bar laid out again at once */
            const uint32_t *pixels = oracles_enhanced_view_surface(s->view);
            if (s->mod) pixels = oracles_mod_session_screen(s->mod, pixels, oracles_enhanced_view_width(s->view), oracles_enhanced_view_height(s->view));
            oracles_sdl_backend_pause_pixels(&s->backend, pixels);
        }
        fprintf(stderr, "oracles: status bar %s\n", oracles_settings_hud_names[applied.hud >= 0 && applied.hud < 3 ? applied.hud : 0]);
    }
    if (changed & ORACLES_SESSION_CHANGED_MENUS) {
        s->menus_large = applied.menus_large;
        oracles_enhanced_view_set_large_menus(s->view, s->menus_large);
        uint32_t crop[4];   /* the pause's image of a framed screen shown alone, or at the view's scale, at once */
        oracles_sdl_backend_pause_crop(&s->backend, s->menus_large && oracles_enhanced_view_framed_crop(s->view, crop) ? crop : NULL);
        fprintf(stderr, "oracles: menus %s\n", s->menus_large ? "large" : "at the view's scale");
    }
    if (changed & ORACLES_SESSION_CHANGED_UNVISITED) {   /* the image behind the pause stays as it was composed: the game resuming shows it */
        oracles_enhanced_view_set_unvisited_hidden(s->view, applied.unvisited_hidden);
        fprintf(stderr, "oracles: unvisited rooms %s\n", applied.unvisited_hidden ? "hidden" : "shown");
    }
    if (changed & ORACLES_SESSION_CHANGED_TEXT_SIZE) {
        oracles_enhanced_view_set_text_size(s->view, (unsigned)applied.text_size);
        uint32_t box[4], reserve = 0;   /* the pause's image of the box the presenter enlarges, at once */
        int bottom = 0;
        unsigned size = 0;
        const int enlarged = oracles_enhanced_view_text_box_enlarged(s->view, box, &bottom, &reserve, &size);
        oracles_sdl_backend_pause_box(&s->backend, enlarged ? box : NULL, bottom, reserve, size);
        fprintf(stderr, "oracles: text size %s\n", applied.text_size == 2 ? "very large" : applied.text_size ? "large" : "normal");
    }
    if (changed & ORACLES_SESSION_CHANGED_MARGIN) {
        s->sdl.screen_margin = applied.margin;
        fprintf(stderr, "oracles: screen margin %s\n", oracles_screen_margin_names[applied.margin]);
        oracles_sdl_backend_set_screen_margin(&s->backend, applied.margin);
    }
    if (changed & ORACLES_SESSION_CHANGED_VSYNC) {
        s->vsync_setting = applied.vsync;
        s->vsync = s->settings->vsync;   /* --vsync's gives way */
        const int on = oracles_session_vsync_on(s->vsync, s->display_hz);
        fprintf(stderr, "oracles: vsync %s (setting %s, display %u Hz)\n", on ? "on" : "off", s->vsync, s->display_hz);
        if (on != s->sdl.vsync) {
            s->sdl.vsync = on;
            oracles_sdl_backend_set_vsync(&s->backend, on);
            s->paces_changed = 1;
        }
    }
    if (changed & ORACLES_SESSION_CHANGED_SWIM) {
        s->swim_setting = s->swim_next = applied.swim_hold;
        s->swim_pending = 1;
    }
    if (changed & ORACLES_SESSION_CHANGED_TEXT_SPEED) {
        s->text_speed_setting = s->text_speed_next = applied.text_instant;
        s->text_speed_pending = 1;
    }
    if (changed & ORACLES_SESSION_CHANGED_HOTKEYS) {
        s->hotkeys_setting = applied.hotkeys;
        s->hotkeys_next = (OraclesHotkeysMode)applied.hotkeys;
        s->hotkeys_pending = 1;
        oracles_session_hotkey_keys(s, s->hotkeys_next != ORACLES_HOTKEYS_OFF);   /* off: the slots' keys do nothing from now on */
    }
    oracles_sdl_backend_rebind(&s->backend, &s->sdl);   /* s->sdl's names are the settings' own */
    return s->sdl.screen_margin;
}

/* The game's cheats as the pause's Cheats shows them: those the next frame begins with. */
static unsigned pause_cheats(const session *s) { return s->cheats_pending ? s->cheats_next : s->cheats; }

/* Cheats changed them, from the running game's (--cheats' perhaps): from the frame the game resumes on, the first
 * after the pause. */
static void pause_cheats_changed(void *opaque, unsigned cheats)
{
    session *s = opaque;
    if (s->cheats_game < 0) return;
    s->cheats_next = cheats;
    s->cheats_pending = cheats != s->cheats;
}

/* Window chosen from the pause (window_scale=): applied at once, whatever the settings held before, since F11 and
 * --fullscreen change the window without them. */
static void pause_window_chosen(void *opaque, int window)
{
    session *s = opaque;
    if (s->window_setting < 0) return;
    s->window_setting = s->window_line = window;
    oracles_session_apply_window(s, window);
}

void oracles_session_started(void *opaque)
{
    session *s = opaque;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *frame;
    int width, height;
    oracles_sdl_backend_pause_view(&s->backend, &window, &renderer, &frame, &width, &height);
    oracles_pause_prepare(s->pause, renderer);
}

/* The game's image under the pause: its last frame as it was shown. */
static int pause_game_image(void *opaque, SDL_Renderer *renderer, const SDL_FRect *image)
{
    session *s = opaque;
    (void)renderer;
    return oracles_sdl_backend_pause_image(&s->backend, image);
}

static void pause_game_size(void *opaque, int *width, int *height)
{
    session *s = opaque;
    oracles_sdl_backend_pause_size(&s->backend, width, height);
}

/* The window as window_scale= counts it, F11's toggle included; -2 for none; its size in *w x *h. */
static int window_now(const session *s, SDL_Window *window, int *w, int *h)
{
    *w = *h = 0;
    if (!window || s->window_setting < 0) return s->window_setting;
    const OraclesEnhancedSize surface = oracles_session_window_surface(s);
    SDL_GetWindowSizeInPixels(window, w, h);
    const unsigned scale = s->window_setting > 0 ? oracles_home_fitting_scale(window, (unsigned)s->window_setting, surface) : 0u;
    return oracles_session_window_now(s->window_setting, scale, (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0, *w, *h,
                                      (int)surface.width, (int)surface.height);
}

/* Display's windows that would take Display from the pause, as oracles_session_apply_window opens them. */
static unsigned windows_refused(const session *s, SDL_Window *window)
{
    if (s->window_setting < 0 || !window) return 0;
    const OraclesEnhancedSize surface = oracles_session_window_surface(s);
    unsigned scales[3];
    for (unsigned v = 0; v < 3; v++) scales[v] = oracles_home_fitting_scale(window, v + 2u, surface);
    return oracles_session_windows_refused(scales, (int)surface.width, (int)surface.height, s->settings->aspect);
}

/* After the pause: whether the display paces the frames from now on, vsync turned on or off there; -1 when it stays. */
int oracles_session_display_paces_now(void *opaque)
{
    session *s = opaque;
    if (!s->paces_changed) return -1;
    s->paces_changed = 0;
    fprintf(stderr, "oracles: %s\n", s->sdl.vsync ? "the display paces the frames from here" : "the host paces the frames at 59.7275 Hz from here");
    return s->sdl.vsync;
}

oracles_host_pause_result oracles_session_pause(void *opaque)
{
    session *s = opaque;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *frame;
    int width, height;
    oracles_sdl_backend_pause_view(&s->backend, &window, &renderer, &frame, &width, &height);
    oracles_sdl_backend_suspend_audio(&s->backend);
    oracles_sdl_backend_hold(&s->backend, 1);
    const oracles_sdl_menu_source source = oracles_sdl_backend_menu_source(&s->backend);
    const int device = oracles_home_menu_device((int)source);
    /* F11 and --fullscreen change the window without the settings: the pause's Window shows the window the game has. */
    int window_w, window_h;
    const int window_shown = window_now(s, window, &window_w, &window_h);
    const OraclesPauseSession services = { s, pause_save, pause_load, pause_state_time, pause_settings_changed, pause_cheats_changed,
                                           pause_window_chosen, (int)oracles_core_kind(s->core), s->ghosts, device, pause_cheats(s),
                                           s->cheats_game >= 0, pause_game_image, s->swim_setting >= 0, s->hotkeys_game >= 0, window_shown, window_w,
                                           window_h, windows_refused(s, window), s->text_speed_setting >= 0, pause_game_size,
                                           oracles_home_menu_by_key((int)source) };
    const OraclesPauseResult result = oracles_pause_run(s->pause, window, renderer, frame, width, height, &services);
    if (result == ORACLES_PAUSE_RESUME) oracles_sdl_backend_hold(&s->backend, 0);   /* a pause that ends the session is past the last present */
    if (result == ORACLES_PAUSE_WINDOW_CLOSED) oracles_sdl_backend_set_window_closed(&s->backend);
    return result == ORACLES_PAUSE_RESUME ? ORACLES_HOST_PAUSE_RESUME : ORACLES_HOST_PAUSE_QUIT;
}
