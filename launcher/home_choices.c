#include "home_choices.h"

#include "backends.h"
#include "guest_tables.h"
#include "safe_area.h"
#include "touch_controls.h"
#include "ui_controls_nav.h"
#include "ui_page_nav.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

/* Room left on the display for the window's frame and title bar, which its usable area does not count. */
#define WINDOW_FRAME 64

/* Display's choices from the settings, and the size of the display the sizes are shown for: the window's, as Play
 * reduces the window on it. */
void oracles_home_inspect_display(const oracles_settings *settings, OraclesHomeDisplay *display, struct SDL_Window *window)
{
    display->profile = settings->profile;
    display->transitions = settings->transitions;
    display->hud = settings->hud;
    display->text = settings->compact_text;
    display->text_box = settings->text_box_wide;
    display->text_size = settings->text_size;
    display->text_speed = settings->text_instant;
    display->text_fixed = display->text_box_fixed = display->text_size_fixed = 0;
    display->view = settings->view;
    display->aspect = settings->aspect;
    display->far_fixed = settings->far_fixed;
    display->menus = settings->menus_large;
    display->unvisited = settings->unvisited_hidden;
    display->unvisited_fixed = 0;
    display->core = oracles_settings_core(settings);
    display->window = settings->window_scale ? settings->window_scale - 2 : 3;
    display->window_w = display->window_h = display->window_chosen = 0;
    display->windows_refused = 0;
    display->colour = settings->colour_correction;
    display->vsync = !strcmp(settings->vsync, "on") ? 1 : !strcmp(settings->vsync, "off") ? 2 : 0;
    display->workers = settings->ghosts;
    display->scaling = settings->scaling_fill;
    display->camera = settings->subpixel_camera;
    display->camera_from = (int)oracles_settings_subpixel_default_from();
    display->margin = settings->screen_margin;
    display->margin_fixed = 0;
    display->cores = SDL_GetNumLogicalCPUCores();   /* as the session counts them for Auto */
    display->fullscreen_only = oracles_sdl_fullscreen_only();
    /* Without a window (a capture drawn offscreen), a 1080p screen. */
    SDL_Rect usable;
    const SDL_DisplayID index = window ? SDL_GetDisplayForWindow(window) : 0;
    const SDL_DisplayMode *mode = index ? SDL_GetDesktopDisplayMode(index) : NULL;
    if (mode && SDL_GetDisplayUsableBounds(index, &usable)) {
        /* In pixels, as the game's window is sized: SDL gives points on a Retina display; the frame is counted in
         * points of the launcher (oracles_sdl_point_scale). */
        const float ratio = oracles_sdl_pixel_ratio(window), frame = WINDOW_FRAME * oracles_sdl_point_scale(window);
        display->screen_w = (int)((float)mode->w * ratio + 0.5f);
        display->screen_h = (int)((float)mode->h * ratio + 0.5f);
        display->room_w = (int)((float)usable.w * ratio + 0.5f);
        display->room_h = (int)(((float)usable.h - frame) * ratio + 0.5f);   /* as fitting_scale reduces the window at Play */
    } else {
        display->screen_w = 1920;
        display->screen_h = 1080;
        display->room_w = 1920;
        display->room_h = 1080 - WINDOW_FRAME;
    }
    /* The band each screen margin leaves the game on this screen, fullscreen (oracles_touch_band). */
    for (int m = 0; m < ORACLES_SCREEN_MARGINS; m++) {
        OraclesTouchEdges edges;
        oracles_safe_area_edges(window, m, &edges);
        const OraclesTouchBox band = oracles_touch_band(display->screen_w, display->screen_h, &edges);
        display->margin_lines[m] = display->screen_h - (int)band.h;
    }
}

/* Controls' names from the settings, each game's slots by their items' names, and Swimming. */
void oracles_home_inspect_controls(const oracles_settings *settings, OraclesHomeControls *c)
{
    for (int i = 0; i < ORACLES_HOME_BUTTONS; i++) snprintf(c->keys[i], sizeof c->keys[i], "%s", settings->key_names[i]);
    for (int i = 0; i < ORACLES_HOME_PAD_BUTTONS; i++) snprintf(c->pads[i], sizeof c->pads[i], "%s", settings->pad_names[i]);
    for (int i = 0; i < ORACLES_HOME_SLOTS; i++) {
        snprintf(c->hotkey_keys[i], sizeof c->hotkey_keys[i], "%s", settings->hotkey_key_names[i]);
        snprintf(c->hotkey_pads[i], sizeof c->hotkey_pads[i], "%s", settings->hotkey_pad_names[i]);
    }
    snprintf(c->menu_pad, sizeof c->menu_pad, "%s", settings->menu_pad);
    snprintf(c->hotkey_keys[ORACLES_HOME_SLOTS], sizeof c->hotkey_keys[0], "%s", settings->hotkey_bind_b);
    snprintf(c->hotkey_keys[ORACLES_HOME_SLOTS + 1], sizeof c->hotkey_keys[0], "%s", settings->hotkey_bind_a);
    c->swim_hold = settings->swim_hold;
    for (int g = 0; g < 2; g++) {
        const OraclesGuestTables *tables = g == 0 ? &oracles_guest_tables_ages : &oracles_guest_tables_seasons;
        for (int i = 0; i < ORACLES_HOME_SLOTS; i++) {
            const OraclesHotkeySlot *slot = &settings->hotkeys[g][i];
            const char *name = slot->set && slot->item < ORACLES_GUEST_ITEM_LABELS ? tables->item_names[slot->item] : NULL;
            snprintf(c->items[g][i], sizeof c->items[g][i], "%s", name ? name : "");
        }
    }
}

void oracles_home_resolve_menu_pad(oracles_settings *settings)
{
    int button, axis;
    if (!oracles_sdl_menu_input(settings->menu_pad, &button, &axis)) {
        fprintf(stderr, "oracles: unknown controller button '%s' for the menu; using %s\n", settings->menu_pad, oracles_controls_default_menu_pad);
        snprintf(settings->menu_pad, sizeof settings->menu_pad, "%s", oracles_controls_default_menu_pad);
    }
    OraclesHomeControls controls;
    oracles_home_inspect_controls(settings, &controls);
    if (settings->menu_pad[0] && !oracles_controls_menu_free(&controls, settings->menu_pad)) {
        fprintf(stderr, "oracles: menu_pad=%s is Guide, or a button the game, the d-pad or an item hotkey slot has, and stays so: Guide alone opens the menu\n",
                settings->menu_pad);
        settings->menu_pad[0] = 0;
    }
}

void oracles_home_store_controls(const OraclesHomeControls *c, oracles_settings *settings)
{
    for (int i = 0; i < ORACLES_HOME_BUTTONS; i++) snprintf(settings->key_names[i], sizeof settings->key_names[i], "%s", c->keys[i]);
    for (int i = 0; i < ORACLES_HOME_PAD_BUTTONS; i++) snprintf(settings->pad_names[i], sizeof settings->pad_names[i], "%s", c->pads[i]);
    for (int i = 0; i < ORACLES_HOME_SLOTS; i++) {
        snprintf(settings->hotkey_key_names[i], sizeof settings->hotkey_key_names[i], "%s", c->hotkey_keys[i]);
        snprintf(settings->hotkey_pad_names[i], sizeof settings->hotkey_pad_names[i], "%s", c->hotkey_pads[i]);
    }
    snprintf(settings->menu_pad, sizeof settings->menu_pad, "%s", c->menu_pad);
    snprintf(settings->hotkey_bind_b, sizeof settings->hotkey_bind_b, "%s", c->hotkey_keys[ORACLES_HOME_SLOTS]);
    snprintf(settings->hotkey_bind_a, sizeof settings->hotkey_bind_a, "%s", c->hotkey_keys[ORACLES_HOME_SLOTS + 1]);
    settings->swim_hold = c->swim_hold;   /* a game started next takes it; a running one keeps its own */
}

void oracles_home_store_display(const OraclesHomeDisplay *display, int in_game, oracles_settings *prefs)
{
    const int view = prefs->view, core = oracles_settings_core(prefs), ghosts = prefs->ghosts;
    prefs->profile = display->profile;
    prefs->transitions = display->transitions;
    prefs->hud = display->hud;   /* a running game takes it at once (the pause's settings_changed) */
    /* The text a game lays its texts out for at its start: the next Play's, from the pause too; never --text's. */
    if (!display->text_fixed) prefs->compact_text = display->text;
    if (!display->text_box_fixed) prefs->text_box_wide = display->text_box;
    /* Text size: a running game takes it at once (the pause's settings_changed); never --text-size's. */
    if (!display->text_size_fixed) prefs->text_size = display->text_size;
    prefs->text_instant = display->text_speed;   /* a running game takes it when it resumes (the pause's settings_changed) */
    prefs->view = display->view;
    prefs->menus_large = display->menus;   /* a running game takes it at once (the pause's settings_changed) */
    if (!in_game) prefs->far_fixed = display->far_fixed;   /* the view's size: the next Play's, never the pause's */
    if (!display->unvisited_fixed) prefs->unvisited_hidden = display->unvisited;   /* a running game takes it when it resumes; never --unvisited-rooms' */
    /* The core is stored once the player chooses one other than the playing default; a game's core is its own, perhaps
     * --core's, never stored. */
    if (!in_game && (prefs->core >= 0 || display->core != oracles_settings_core(prefs))) prefs->core = display->core;
    /* So are the workers: a game's are perhaps --ghosts', for its run alone. */
    if (!in_game) prefs->ghosts = display->workers;
    oracles_settings_quality_changed(prefs, view, core, ghosts);
    static const char *const vsync_names[3] = { "auto", "on", "off" };
    /* From the pause, only once Window is chosen: the row shows the window F11 or --fullscreen left, which hold till then. */
    if ((!in_game || display->window_chosen) && display->window >= 0) prefs->window_scale = display->window < 3 ? display->window + 2 : 0;
    prefs->colour_correction = display->colour;
    prefs->scaling_fill = display->scaling;
    prefs->subpixel_camera = display->camera;   /* absent until chosen; a running game takes it at once (oracles_session_follow_subpixel) */
    if (!display->margin_fixed) prefs->screen_margin = display->margin;   /* never --screen-margin's */
    snprintf(prefs->vsync, sizeof prefs->vsync, "%s", vsync_names[display->vsync]);
}

/* The largest whole scale, up to the one asked for, at which the surface's window fits the display. */
unsigned oracles_home_fitting_scale(struct SDL_Window *window, unsigned wanted, OraclesEnhancedSize surface)
{
    SDL_Rect usable;
    const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (!display || !SDL_GetDisplayUsableBounds(display, &usable)) return wanted;
    unsigned scale = wanted;
    /* The usable area in pixels, as the window is sized (points on a Retina display), less the window's frame. */
    const float ratio = oracles_sdl_pixel_ratio(window), frame = WINDOW_FRAME * oracles_sdl_point_scale(window);
    const int room_w = (int)((float)usable.w * ratio + 0.5f), room_h = (int)(((float)usable.h - frame) * ratio + 0.5f);
    while (scale > 1u && ((int)(scale * surface.width) > room_w || (int)(scale * surface.height) > room_h)) scale--;
    return scale;
}

void oracles_home_game_window(const oracles_settings *settings, int window_scale, struct SDL_Window *window, OraclesEnhancedSize surface,
                              int *fullscreen, unsigned *scale)
{
    *fullscreen = window_scale == 0 || oracles_sdl_fullscreen_only();
    *scale = oracles_home_fitting_scale(window, window_scale > 0 ? (unsigned)window_scale : 4u, surface);
    if (!*fullscreen && settings->scaling_fill) {
        OraclesHomeDisplay display;
        oracles_home_inspect_display(settings, &display, window);
        *fullscreen = oracles_display_one_size_for(&display, (int)surface.width, (int)surface.height);
    }
}
