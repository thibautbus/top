#include "home.h"

#include "backends.h"
#include "file_dialog.h"
#include "home_input.h"
#include "safe_area.h"
#include "touch_controls.h"
#include "ui_controls_nav.h"
#include "ui_home.h"
#include "ui_page_nav.h"
#include "update_check.h"

#include <SDL3/SDL.h>

#include <math.h>

#include <stdio.h>
#include <string.h>

/* While something moves, a frame at most this often if the display does not pace the presentation. */
#define FRAME_MS 16.0
/* The home screen paces its own frames (FRAME_MS): vsync only spares a desktop tearing.  On Android the compositor
 * shows whole frames anyway, and a presentation that waits on a display the window has lost may not return, the
 * likely cause of the freeze an Anbernic RG DS (two screens) showed back from a game: none there. */
#define HOME_VSYNC (!oracles_sdl_fullscreen_only())
#define WATCH_MS 15000.0   /* after a game, the seconds the loop says what it did */
#ifdef __ANDROID__
/* Android shows a window's surface some frames after it is ready: a single frame drawn then, at the start or after a
 * window event (the return from the background), can be lost and leave the screen black until an input. */
#define ANDROID_REDRAWS 30
#else
#define ANDROID_REDRAWS 0
#endif

typedef struct home_app {
    const OraclesHomeHost *host;
    SDL_Window *window;
    SDL_Renderer *renderer;
    OraclesSdlPads pads;      /* every gamepad plugged in */
    OraclesUiDraw *draw;
    OraclesHomeNav nav;
    OraclesScreen screen;    /* the one shown when the navigation last changed */
    OraclesUiHome view;
    int width, height;       /* the window's size while windowed, remembered */
    int fullscreen;
    OraclesTouchEdges edges;  /* the screen margin and what it was read from (oracles_safe_area_edges): the page keeps to its band */
    int dirty, moving, idle_work, running;
    int redraws;             /* frames still to draw whatever changed (Android: see ANDROID_REDRAWS) */
    int dialog_open;         /* the file dialog has not answered yet */
    int quit_after_dialog;   /* the window was closed meanwhile: zenity's dialog is not modal */
    int inputs_reported;     /* after a game, the inputs said on the standard error so far (the first few) */
    int trigger_held;        /* the menu's trigger, when menu_pad= names one (oracles_home_button_action) */
    /* After a game, a line a second on the standard error until watch_until_ms: what the loop did, so that a report
     * that stops shows when the loop stopped, and one that goes on shows a loop alive behind a frozen screen. */
    double watch_since_ms, watch_until_ms, watch_next_ms, watch_present_max_ms;
    unsigned watch_events, watch_inputs, watch_window, watch_frames, watch_presented;
    Uint32 watch_type[6];    /* the second's events by type, the first six types seen */
    unsigned watch_type_count[6];
    double presented_ms;
} home_app;

static double now_ms(void)
{
    return (double)SDL_GetPerformanceCounter() * 1000.0 / (double)SDL_GetPerformanceFrequency();
}

static void changed(home_app *app)
{
    /* Mods opened: the folder is read again, for the mods the player put there meanwhile. */
    if (app->nav.screen == ORACLES_SCREEN_MODS && app->screen != ORACLES_SCREEN_MODS && app->host && app->host->read_mods)
        app->host->read_mods(app->host->opaque, &app->nav);
    app->screen = app->nav.screen;
    oracles_ui_home_follow(&app->view, &app->nav, now_ms());
    app->dirty = 1;
}

/* The screen margin's edges read again, for the margin Display has: the page re-laid out in its band. */
static void read_edges(home_app *app)
{
    oracles_safe_area_edges(app->window, app->nav.display.margin, &app->edges);
    app->dirty = 1;
}

static void refresh(home_app *app)
{
    if (app->host && app->host->refresh) app->host->refresh(app->host->opaque, &app->nav, app->window);
    oracles_page_keep_row(&app->nav);
    if (app->nav.display.margin != app->edges.margin) read_edges(app);
}

/* A size of the launcher, in points (settings.txt's), in the window's coordinates: pixels on Windows. */
static int to_window(int points, float scale) { return (int)lroundf((float)points * scale); }

/* What the window and the controllers are, said on the standard error: where the home screen stands after a game, so
 * that a session report shows where the input goes (a device of two screens may give the input to the other one). */
static void report_window(home_app *app, const char *when)
{
    const SDL_WindowFlags flags = SDL_GetWindowFlags(app->window);
    int count = 0;
    SDL_JoystickID *pads = SDL_GetGamepads(&count);
    SDL_free(pads);
    char open[256];
    oracles_sdl_pads_describe(&app->pads, open, sizeof open);
    fprintf(stderr, "oracles: home screen %s: display %u, input focus %s, %s, %d controller(s), %s\n", when,
            (unsigned)SDL_GetDisplayForWindow(app->window), (flags & SDL_WINDOW_INPUT_FOCUS) ? "yes" : "no",
            (flags & SDL_WINDOW_FULLSCREEN) ? "fullscreen" : "windowed", count, open);
}

/* The window as the launcher keeps it, after a game had it: its fullscreen state, its smallest and its own size.  SDL
 * 3 may apply them after the calls return: each is waited for before the window is read.  Where the window is always
 * fullscreen (Android), it is left as it is: the game kept it fullscreen, and asking again waits on the system for
 * nothing. */
static void restore_window(home_app *app)
{
    if (oracles_sdl_fullscreen_only()) { SDL_SetRenderVSync(app->renderer, HOME_VSYNC); return; }
    const float scale = oracles_sdl_point_scale(app->window);
    SDL_SetWindowFullscreen(app->window, app->fullscreen != 0);
    SDL_SyncWindow(app->window);
    SDL_SetWindowMinimumSize(app->window, to_window(ORACLES_HOME_MIN_WIDTH, scale), to_window(ORACLES_HOME_MIN_HEIGHT, scale));
    if (!app->fullscreen) {
        int x, y, w, h;
        SDL_GetWindowPosition(app->window, &x, &y);
        SDL_GetWindowSize(app->window, &w, &h);
        SDL_SetWindowSize(app->window, to_window(app->width, scale), to_window(app->height, scale));
        SDL_SyncWindow(app->window);
        oracles_sdl_place_window(app->window, x + w / 2, y + h / 2);
    }
    SDL_SetRenderVSync(app->renderer, HOME_VSYNC);
}

/* The update check's answer: a newer version, shown by the version from then on (it fades in). */
static void read_update(home_app *app)
{
    OraclesUpdateStatus status;
    oracles_update_check_status(&status);
    if (status.state != ORACLES_UPDATE_NEWER || app->nav.update[0]) return;
    snprintf(app->nav.update, sizeof app->nav.update, "%s", status.version);
    changed(app);
}

/* A game played, then the home screen back; the host's answer (home.h). */
static int play(home_app *app, OraclesHomeCommand game)
{
    char message[256] = "";
    int slow = 0;
    const int ended = app->host->start(app->host->opaque, game, &app->nav, app->window, app->renderer, app->draw, message, sizeof message, &slow);
    oracles_ui_draw_freeze(app->draw, 0);   /* the pause held the rasters at the game's scale; the home screen redoes its own */
    if (ended == 1) { app->running = 0; return ended; }
    restore_window(app);
    read_edges(app);   /* the game may have seen them change */
    /* The session had the controllers' plugs and unplugs: every controller is opened again from scratch, so that one the
     * session's handles left in a state of their own is taken as new. */
    oracles_sdl_pads_close(&app->pads);
    oracles_sdl_pads_open(&app->pads);
    app->trigger_held = oracles_home_trigger_held(&app->nav);
    /* Back from the game to the home screen, on Start game, from the page's Play as from the menu; the Save row's
     * status stayed on the page left. */
    app->nav.screen = ORACLES_SCREEN_HOME;
    app->nav.focus = 0;
    app->nav.save_flow.state = ORACLES_SAVE_FLOW_NONE;
    app->nav.slow = slow;
    /* What the game left in the queue (the Escape that ended it) is not the home screen's. */
    SDL_FlushEvents(SDL_EVENT_KEY_DOWN, SDL_EVENT_GAMEPAD_REMAPPED);
    report_window(app, "back from the game");
    app->inputs_reported = 0;
    app->watch_presented = 0;
    app->watch_since_ms = now_ms();
    app->watch_until_ms = app->watch_since_ms + WATCH_MS;
    app->watch_next_ms = app->watch_since_ms + 1000.0;
    app->watch_events = app->watch_inputs = app->watch_window = app->watch_frames = 0;
    memset(app->watch_type_count, 0, sizeof app->watch_type_count);
    app->watch_present_max_ms = 0.0;
    char platform[256];
    oracles_android_activity_text("inputState", platform, sizeof platform);   /* its counts start now */
    refresh(app);
    read_update(app);   /* an answer that came during the game, whose loop took its event */
    changed(app);
    if (message[0]) oracles_ui_home_toast(&app->view, message, now_ms());
    return ended;
}

static void start(home_app *app, OraclesHomeCommand game)
{
    if (!app->host || !app->host->start) return;
    /* The game again, at once (its savestate's Display chosen in its pause), from the window and the inputs as a game
     * started here finds them, before the home screen draws. */
    while (play(app, game) == 2) {}
}

/* The game's page asks the host for a file, a folder, or to keep a choice. */
static void page_command(home_app *app, OraclesHomeCommand c)
{
    const OraclesHomeHost *host = app->host;
    const int game = oracles_home_game(&app->nav);
    char message[256] = "";
    if (!host || game < 0) return;
    if (c == ORACLES_HOME_STORE && host->store) {
        host->store(host->opaque, &app->nav);
        if (app->nav.display.margin != app->edges.margin) read_edges(app);
        return;
    }
    if (host->file_chosen && ((c == ORACLES_HOME_CHOOSE_ROM && host->choose_rom) || (c == ORACLES_HOME_CHOOSE_PATCH && host->choose_patch)
                              || (c == ORACLES_HOME_CHOOSE_EU_ROM && host->choose_eu_rom))) {
        /* The dialog answers later (dialog_answered); until then the home screen draws but takes no input. */
        app->dialog_open = 1;
        if (c == ORACLES_HOME_CHOOSE_ROM) host->choose_rom(host->opaque, game, app->window);
        else if (c == ORACLES_HOME_CHOOSE_PATCH) host->choose_patch(host->opaque, game, app->window);
        else host->choose_eu_rom(host->opaque, game, app->window);
        return;
    }
    if ((c == ORACLES_HOME_IMPORT_SAVE || c == ORACLES_HOME_EXPORT_SAVE || c == ORACLES_HOME_EXPORT_MODS_SAVE) && host->choose_save && host->save_chosen) {
        const OraclesDialogKind kind = c == ORACLES_HOME_EXPORT_MODS_SAVE ? ORACLES_DIALOG_EXPORT_MODS
                                     : c == ORACLES_HOME_EXPORT_SAVE    ? ORACLES_DIALOG_EXPORT
                                                                        : ORACLES_DIALOG_SAVE;
        app->dialog_open = host->choose_save(host->opaque, game, kind, app->window);
        /* The file to copy gone: the page shows it, its action greyed. */
        if (!app->dialog_open) {
            refresh(app);
            changed(app);
        }
        return;
    }
    if (c == ORACLES_HOME_REPLACE_SAVE && host->replace_save) {
        host->replace_save(host->opaque, game, &app->nav);
        refresh(app);
        changed(app);
        return;
    }
    if (c == ORACLES_HOME_MODS_LIMIT) { oracles_ui_home_toast(&app->view, oracles_mods_limit, now_ms()); return; }
    if (c == ORACLES_HOME_OPEN_MODS && host->open_mods) host->open_mods(host->opaque, message, sizeof message);
    else if (c == ORACLES_HOME_OPEN_FOLDER && host->open_folder) host->open_folder(host->opaque, game, message, sizeof message);
    else return;
    /* The file manager may have held the loop: what came meanwhile is stale. */
    SDL_FlushEvents(SDL_EVENT_KEY_DOWN, SDL_EVENT_GAMEPAD_REMAPPED);
    refresh(app);
    changed(app);
    if (message[0]) oracles_ui_home_toast(&app->view, message, now_ms());
}

/* The file dialog's answer: the ROM or the patch taken for the game it was opened for, or why not; a save to import
 * or where a copy of it goes, which the page then shows. */
static void dialog_answered(home_app *app, int result, int kind, int game, const char *path, int copied)
{
    char message[256] = "";
    app->dialog_open = 0;
    if (app->quit_after_dialog) app->running = 0;
    if (kind == ORACLES_DIALOG_SAVE || kind == ORACLES_DIALOG_EXPORT || kind == ORACLES_DIALOG_EXPORT_MODS) app->host->save_chosen(app->host->opaque, game, kind, result, path, &app->nav);
    else app->host->file_chosen(app->host->opaque, game, kind, result, path, copied, message, sizeof message);
    refresh(app);
    changed(app);
    if (message[0]) oracles_ui_home_toast(&app->view, message, now_ms());
}

/* What the player does while the file dialog is open is not the home screen's. */
static int input(const SDL_Event *e)
{
    return (e->type >= SDL_EVENT_KEY_DOWN && e->type <= SDL_EVENT_TEXT_INPUT) ||
           (e->type >= SDL_EVENT_MOUSE_MOTION && e->type <= SDL_EVENT_MOUSE_WHEEL) ||
           (e->type >= SDL_EVENT_GAMEPAD_AXIS_MOTION && e->type <= SDL_EVENT_GAMEPAD_BUTTON_UP) || e->type == SDL_EVENT_DROP_FILE;
}

static void command(home_app *app, OraclesHomeCommand c)
{
    char message[256];
    if (c == ORACLES_HOME_EXIT) app->running = 0;
    else if (c == ORACLES_HOME_OPEN_UPDATE) {   /* the browser opens, the launcher staying; without one, the page's address */
        if (!oracles_update_check_open(message, sizeof message) && message[0]) oracles_ui_home_toast(&app->view, message, now_ms());
    }
    else if (c == ORACLES_HOME_START_AGES || c == ORACLES_HOME_START_SEASONS || c == ORACLES_HOME_START_AGES_MODS || c == ORACLES_HOME_START_SEASONS_MODS
             || oracles_home_fan_game_started(c)) start(app, c);
    else if (c != ORACLES_HOME_STAY) page_command(app, c);
}

static void act(home_app *app, OraclesHomeAction action)
{
    const OraclesHomeCommand c = oracles_home_act(&app->nav, action);
    changed(app);
    command(app, c);
}

/* A point of the window, in its own coordinates, in the scene's. */
static void window_to_scene(const home_app *app, float x, float y, float *scene_x, float *scene_y)
{
    int window_w, window_h, output_w, output_h;
    SDL_GetWindowSize(app->window, &window_w, &window_h);
    SDL_GetRenderOutputSize(app->renderer, &output_w, &output_h);
    const float kx = window_w > 0 ? (float)output_w / (float)window_w : 1.0f, ky = window_h > 0 ? (float)output_h / (float)window_h : 1.0f;
    oracles_ui_draw_to_scene(app->draw, x * kx, y * ky, scene_x, scene_y);
}

static void pointer(home_app *app, float x, float y, int click)
{
    float sx, sy;
    window_to_scene(app, x, y, &sx, &sy);
    const OraclesUiHomeHit hit = oracles_ui_home_hit(&app->nav, sx, sy);
    if (hit.kind == ORACLES_UI_HIT_CELL) {
        const OraclesHomeControls before = app->nav.controls;
        if (!click) { oracles_controls_hover(&app->nav, hit.column, hit.row); if (memcmp(&before, &app->nav.controls, sizeof before)) changed(app); return; }
        const OraclesHomeCommand c = oracles_controls_click(&app->nav, hit.column, hit.row, hit.option);
        changed(app);
        command(app, c);
        return;
    }
    if (hit.kind == ORACLES_UI_HIT_ROW) {
        if (!click) {
            /* On a game's page the action or the button under the pointer takes the highlight too. */
            const unsigned row = app->nav.row;
            const int save_action = app->nav.save_action, eu_action = app->nav.eu_action, choice = app->nav.save_flow.choice;
            if (app->nav.screen == ORACLES_SCREEN_GAME) oracles_page_hover(&app->nav, hit.index, hit.option);
            else app->nav.row = hit.index;
            if (app->nav.row != row || app->nav.save_action != save_action || app->nav.eu_action != eu_action || app->nav.save_flow.choice != choice)
                changed(app);
            return;
        }
        const OraclesHomeCommand c = app->nav.screen == ORACLES_SCREEN_GAME ? oracles_page_click(&app->nav, hit.index, hit.option)
                                     : app->nav.screen == ORACLES_SCREEN_MODS ? oracles_mods_click(&app->nav, hit.index)
                                     : app->nav.screen == ORACLES_SCREEN_CHEATS ? oracles_cheats_click(&app->nav, hit.index, hit.option)
                                     : app->nav.screen == ORACLES_SCREEN_ABOUT ? oracles_about_click(&app->nav, hit.option)
                                                                              : oracles_display_click(&app->nav, hit.index, hit.option);
        changed(app);
        command(app, c);
        return;
    }
    if (!click) {
        if (hit.kind == ORACLES_UI_HIT_ITEM && hit.index != oracles_home_focus(&app->nav)) { oracles_home_hover(&app->nav, hit.index); changed(app); }
        return;
    }
    if (hit.kind == ORACLES_UI_HIT_SLOW) { oracles_home_slow_open(&app->nav); changed(app); }
    else if (hit.kind == ORACLES_UI_HIT_ITEM) { const OraclesHomeCommand c = oracles_home_click(&app->nav, hit.index); changed(app); command(app, c); }
    else if (hit.kind == ORACLES_UI_HIT_ENTRY) { oracles_home_select(&app->nav, hit.entry); changed(app); }
    else if (hit.kind == ORACLES_UI_HIT_BACK) act(app, ORACLES_HOME_BACK);
}

static void drop(home_app *app, const char *path)
{
    if (!app->host || !app->host->drop) return;
    char message[256] = "";
    const int page_game = app->nav.screen == ORACLES_SCREEN_GAME ? oracles_home_game(&app->nav) : -1;
    const int entry = app->host->drop(app->host->opaque, path, page_game, message, sizeof message);
    refresh(app);
    if (entry >= 0) oracles_home_select(&app->nav, (OraclesHomeEntry)entry);
    changed(app);
    if (message[0]) oracles_ui_home_toast(&app->view, message, now_ms());
}

static void toggle_fullscreen(home_app *app)
{
    if (oracles_sdl_fullscreen_only()) return;
    app->fullscreen = !app->fullscreen;
    SDL_SetWindowFullscreen(app->window, app->fullscreen != 0);
}

static void handle(home_app *app, const SDL_Event *e)
{
    OraclesHomeAction action;
    OraclesHomeCommand captured;
    OraclesDialogResult result;
    OraclesDialogKind kind;
    int game, copied;
    char path[4096];
    if (oracles_dialog_answer(e, &result, &kind, &game, path, sizeof path, &copied)) {
        dialog_answered(app, (int)result, (int)kind, game, path, copied);
        return;
    }
    if (app->dialog_open && e->type == SDL_EVENT_QUIT) { app->quit_after_dialog = 1; return; }   /* after its answer */
    if (app->dialog_open && input(e)) return;
    OraclesInputDevice device;
    if (oracles_home_input_device(e, &device) && oracles_home_input(&app->nav, device)) app->dirty = 1;
    /* A cell of Controls that waits takes the next key or button, Escape included (it cancels). */
    if (oracles_home_capture(&app->nav, e, &captured)) {
        /* A trigger taken for the menu is held still: it counts once let go. */
        if (captured == ORACLES_HOME_STORE) app->trigger_held = oracles_home_trigger_held(&app->nav);
        changed(app);
        command(app, captured);
        return;
    }
    /* After a game, the first inputs the home screen gets, and its window's focus and screen changes: what a session
     * report needs to tell input that stopped from input that went elsewhere. */
    if (app->inputs_reported < 3 && (e->type == SDL_EVENT_KEY_DOWN || e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e->type == SDL_EVENT_FINGER_DOWN)) {
        app->inputs_reported++;
        fprintf(stderr, "oracles: home screen input: %s %d\n", e->type == SDL_EVENT_KEY_DOWN ? "key" : e->type == SDL_EVENT_FINGER_DOWN ? "finger" : "controller button",
                e->type == SDL_EVENT_KEY_DOWN ? (int)e->key.key : e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? (int)e->gbutton.button : 0);
    }
    if (app->watch_until_ms > 0.0) {
        app->watch_events++;
        for (unsigned i = 0; i < 6u; i++) {
            if (app->watch_type_count[i] && app->watch_type[i] != e->type) continue;
            app->watch_type[i] = e->type;
            app->watch_type_count[i]++;
            break;
        }
        if (e->type == SDL_EVENT_KEY_DOWN || e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e->type == SDL_EVENT_FINGER_DOWN) app->watch_inputs++;
        if (e->type >= SDL_EVENT_WINDOW_FIRST && e->type <= SDL_EVENT_WINDOW_LAST) app->watch_window++;
        /* The application's life (background, foreground, low memory) and any window event, by its number. */
        if ((e->type >= SDL_EVENT_TERMINATING && e->type <= SDL_EVENT_DID_ENTER_FOREGROUND) || (e->type >= SDL_EVENT_WINDOW_FIRST && e->type <= SDL_EVENT_WINDOW_LAST))
            fprintf(stderr, "oracles: home screen event 0x%x, %.0f ms after the game\n", (unsigned)e->type, now_ms() - app->watch_since_ms);
    }
    if (oracles_safe_area_event(app->window, "home screen", e, &app->edges)) app->dirty = 1;
    /* The update check's answer (update_check.h). */
    if (oracles_update_check_event() && e->type == oracles_update_check_event()) read_update(app);
    if (e->type == SDL_EVENT_WINDOW_FOCUS_GAINED || e->type == SDL_EVENT_WINDOW_FOCUS_LOST || e->type == SDL_EVENT_WINDOW_DISPLAY_CHANGED
        || e->type == SDL_EVENT_WINDOW_HIDDEN || e->type == SDL_EVENT_WINDOW_SHOWN)
        fprintf(stderr, "oracles: home screen window: %s\n", e->type == SDL_EVENT_WINDOW_FOCUS_GAINED ? "focus gained" : e->type == SDL_EVENT_WINDOW_FOCUS_LOST ? "focus lost"
                : e->type == SDL_EVENT_WINDOW_DISPLAY_CHANGED ? "moved to another display" : e->type == SDL_EVENT_WINDOW_HIDDEN ? "hidden" : "shown");
    switch (e->type) {
        case SDL_EVENT_QUIT: app->running = 0; break;
        case SDL_EVENT_KEY_DOWN:
            if (e->key.key == SDLK_F11 && !e->key.repeat) { toggle_fullscreen(app); break; }
            /* A held arrow repeats; a held Enter or Escape does not. */
            if (oracles_home_key_action(e->key.key, &action) && (!e->key.repeat || action <= ORACLES_HOME_RIGHT)) act(app, action);
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (oracles_home_button_action(&app->nav, e, &app->trigger_held, &action)) act(app, action);
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
            oracles_sdl_pads_event(&app->pads, e);
            break;
        case SDL_EVENT_MOUSE_MOTION: pointer(app, e->motion.x, e->motion.y, 0); break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (e->button.button == SDL_BUTTON_LEFT) pointer(app, e->button.x, e->button.y, 1);
            break;
        case SDL_EVENT_DROP_FILE: drop(app, e->drop.data); break;   /* SDL frees its copy */
        case SDL_EVENT_WINDOW_RESIZED:
            if (!(SDL_GetWindowFlags(app->window) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED))) {
                const float scale = oracles_sdl_point_scale(app->window);
                app->width = (int)lroundf((float)e->window.data1 / scale);
                app->height = (int)lroundf((float)e->window.data2 / scale);
            }
            app->dirty = 1;
            break;
        /* Display's sizes and its diagram are those of the screen the window is on. */
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED: refresh(app); changed(app); break;
        default:
            if (e->type >= SDL_EVENT_WINDOW_FIRST && e->type <= SDL_EVENT_WINDOW_LAST) { app->dirty = 1; app->redraws = ANDROID_REDRAWS; }
            break;
    }
}

static int render(home_app *app, double now)
{
    int w, h;
    if (!SDL_GetRenderOutputSize(app->renderer, &w, &h)) return 0;
    /* The layout of the window's shape, chosen again at each frame: a resize across the middle of 4:3 and 16:9
     * changes it, and the pointer is then looked for in the new one. */
    app->nav.layout = oracles_ui_layout_choose(w, h, app->nav.display.aspect);
    oracles_ui_draw_layout(app->draw, oracles_ui_home_scene(&app->nav));
    const OraclesTouchBox safe = oracles_touch_band(w, h, &app->edges);
    oracles_ui_draw_band(app->draw, (int)safe.y, (int)safe.h);
    if (!oracles_ui_draw_begin(app->draw, w, h, now)) return 0;
    app->moving = oracles_ui_home_draw(app->draw, &app->view, &app->nav, now);
    oracles_ui_draw_end(app->draw);
    /* After a game, each presentation of the watched seconds said before and after: a session report that stops
     * between the two is a presentation that never returned. */
    const int watched = app->watch_until_ms > 0.0;
    if (watched) fprintf(stderr, "oracles: home screen: presenting frame %u after the game\n", ++app->watch_presented);
    const double present_started = now_ms();
    SDL_RenderPresent(app->renderer);
    if (watched) {
        const double took = now_ms() - present_started;
        app->watch_frames++;
        if (took > app->watch_present_max_ms) app->watch_present_max_ms = took;
        fprintf(stderr, "oracles: home screen: frame %u presented in %.1f ms\n", app->watch_presented, took);
    }
    app->presented_ms = now;
    app->dirty = 0;
    if (app->redraws > 0) app->redraws--;
    app->idle_work = 1;
    return 1;
}

/* How long to wait for an event: not at all with a frame due, until the next timed change otherwise, forever when none. */
static int wait_ms(const home_app *app, double now)
{
    if (app->dirty) return 0;
    if (app->redraws > 0) { const double left = FRAME_MS - (now - app->presented_ms); return left > 0.0 ? (int)left : 0; }
    if (app->moving) { const double left = FRAME_MS - (now - app->presented_ms); return left > 0.0 ? (int)left : 0; }
    if (app->idle_work) return 0;
    double due = oracles_ui_home_due_ms(&app->view);
    if (app->watch_until_ms > 0.0 && (due < 0.0 || app->watch_next_ms < due)) due = app->watch_next_ms;
    const double settle = oracles_ui_draw_due_ms(app->draw);
    if (settle >= 0.0 && (due < 0.0 || settle < due)) due = settle;
    if (due < 0.0) return -1;
    return due > now ? (int)(due - now) + 1 : 0;
}

static int open_window(home_app *app)
{
    const float scale = oracles_sdl_point_scale(NULL);
    const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    /* A remembered size larger than the screen would open off it. */
    if (mode) {
        if (to_window(app->width, scale) > mode->w) app->width = (int)((float)mode->w / scale);
        if (to_window(app->height, scale) > mode->h) app->height = (int)((float)mode->h / scale);
    }
    if (app->width < ORACLES_HOME_MIN_WIDTH || app->height < ORACLES_HOME_MIN_HEIGHT) {
        app->width = ORACLES_HOME_DEFAULT_WIDTH;
        app->height = ORACLES_HOME_DEFAULT_HEIGHT;
    }
    app->window = oracles_sdl_create_window(to_window(app->width, scale), to_window(app->height, scale),
                                            SDL_WINDOW_RESIZABLE | oracles_sdl_window_flags() | (app->fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
    if (!app->window) return 0;
    SDL_SetWindowMinimumSize(app->window, to_window(ORACLES_HOME_MIN_WIDTH, scale), to_window(ORACLES_HOME_MIN_HEIGHT, scale));
    app->renderer = oracles_sdl_create_renderer(app->window, HOME_VSYNC);
    if (!app->renderer) return 0;
    oracles_sdl_pads_open(&app->pads);
    app->draw = oracles_ui_draw_create(app->renderer);
    return app->draw != NULL;
}

int oracles_home_run(const OraclesHomeHost *host, int *width, int *height)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) { fprintf(stderr, "oracles: %s\n", SDL_GetError()); return 1; }
    oracles_update_check_events(1);
    home_app app;
    memset(&app, 0, sizeof app);
    app.fullscreen = oracles_sdl_fullscreen_only();
    app.host = host;
    app.width = *width;
    app.height = *height;
    int status = 0;
    if (!open_window(&app)) {
        fprintf(stderr, "oracles: the launcher's window cannot open: %s\n", SDL_GetError());
        status = 1;
    } else {
        SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true);
        oracles_home_init(&app.nav);
        refresh(&app);   /* the screen margin's edges read with Display's */
        oracles_safe_area_report(app.window, "home screen", "opened", &app.edges);
        oracles_ui_home_start(&app.view, &app.nav);
        read_update(&app);   /* an answer that came before the events were on */
        app.dirty = 1;
        app.redraws = ANDROID_REDRAWS;
        app.running = 1;
        while (app.running) {
            SDL_Event e;
            const int wait = wait_ms(&app, now_ms());
            if (wait < 0 ? SDL_WaitEvent(&e) : SDL_WaitEventTimeout(&e, wait)) {
                handle(&app, &e);
                while (app.running && SDL_PollEvent(&e)) handle(&app, &e);
            }
            if (!app.running) break;
            const double now = now_ms();
            if (app.watch_until_ms > 0.0 && now >= app.watch_next_ms) {
                const SDL_WindowFlags flags = SDL_GetWindowFlags(app.window);
                char types[128] = "", platform[256] = "";
                for (unsigned i = 0, used = 0; i < 6u && app.watch_type_count[i] && used < sizeof types; i++)
                    used += (unsigned)snprintf(types + used, sizeof types - used, "%s0x%x x%u", i ? ", " : "; by type ", (unsigned)app.watch_type[i], app.watch_type_count[i]);
                oracles_android_activity_text("inputState", platform, sizeof platform);
                fprintf(stderr, "oracles: home screen, %.0f s after the game: %u events (%u inputs, %u of the window%s), %u frames presented (at most %.1f ms), input focus %s%s%s%s\n",
                        (now - app.watch_since_ms) / 1000.0, app.watch_events, app.watch_inputs, app.watch_window, types, app.watch_frames, app.watch_present_max_ms,
                        (flags & SDL_WINDOW_INPUT_FOCUS) ? "yes" : "no", (flags & SDL_WINDOW_HIDDEN) ? ", hidden" : "", platform[0] ? "; " : "", platform);
                memset(app.watch_type_count, 0, sizeof app.watch_type_count);
                app.watch_events = app.watch_inputs = app.watch_window = app.watch_frames = 0;
                app.watch_present_max_ms = 0.0;
                app.watch_next_ms += 1000.0;
                if (app.watch_next_ms > app.watch_until_ms) app.watch_until_ms = 0.0;
            }
            const double toast = oracles_ui_home_due_ms(&app.view), settle = oracles_ui_draw_due_ms(app.draw);
            if (app.dirty || ((app.moving || app.redraws > 0) && now - app.presented_ms >= FRAME_MS) || (toast >= 0.0 && now >= toast) || (settle >= 0.0 && now >= settle)) {
                if (!render(&app, now)) { fprintf(stderr, "oracles: the launcher cannot draw: %s\n", SDL_GetError()); status = 1; break; }
            } else if (app.idle_work && !app.moving) {
                app.idle_work = oracles_ui_draw_idle(app.draw);
            }
        }
    }
    *width = app.width;
    *height = app.height;
    oracles_ui_draw_destroy(app.draw);
    oracles_sdl_pads_close(&app.pads);
    if (app.renderer) SDL_DestroyRenderer(app.renderer);
    if (app.window) SDL_DestroyWindow(app.window);
    oracles_update_check_close();   /* no event pushed past SDL_Quit */
    SDL_Quit();
    return status;
}
