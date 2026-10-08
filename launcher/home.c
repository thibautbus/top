#include "home.h"

#include "backends.h"
#include "file_dialog.h"
#include "home_input.h"
#include "ui_controls_nav.h"
#include "ui_home.h"
#include "ui_page_nav.h"

#include <SDL3/SDL.h>

#include <math.h>

#include <stdio.h>
#include <string.h>

/* While something moves, a frame at most this often if the display does not pace the presentation. */
#define FRAME_MS 16.0
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
    int dirty, moving, idle_work, running;
    int redraws;             /* frames still to draw whatever changed (Android: see ANDROID_REDRAWS) */
    int dialog_open;         /* the file dialog has not answered yet */
    int quit_after_dialog;   /* the window was closed meanwhile: zenity's dialog is not modal */
    int inputs_reported;     /* after a game, the inputs said on the standard error so far (the first few) */
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

static void refresh(home_app *app)
{
    if (app->host && app->host->refresh) app->host->refresh(app->host->opaque, &app->nav, app->window);
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
    if (oracles_sdl_fullscreen_only()) { SDL_SetRenderVSync(app->renderer, 1); return; }
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
    SDL_SetRenderVSync(app->renderer, 1);
}

static void start(home_app *app, OraclesHomeCommand game)
{
    if (!app->host || !app->host->start) return;
    char message[256] = "";
    const int closed = app->host->start(app->host->opaque, game, app->window, app->renderer, app->draw, message, sizeof message);
    oracles_ui_draw_freeze(app->draw, 0);   /* the pause held the rasters at the game's scale; the home screen redoes its own */
    if (closed) { app->running = 0; return; }
    restore_window(app);
    /* The session had the controllers' plugs and unplugs: those unplugged are let go, those plugged in are taken. */
    oracles_sdl_pads_open(&app->pads);
    /* Back from the game to the home screen, on Start game, from the page's Play as from the menu. */
    app->nav.screen = ORACLES_SCREEN_HOME;
    app->nav.focus = 0;
    /* What the game left in the queue (the Escape that ended it) is not the home screen's. */
    SDL_FlushEvents(SDL_EVENT_KEY_DOWN, SDL_EVENT_GAMEPAD_REMAPPED);
    report_window(app, "back from the game");
    app->inputs_reported = 0;
    refresh(app);
    changed(app);
    if (message[0]) oracles_ui_home_toast(&app->view, message, now_ms());
}

/* The game's page asks the host for a file, a folder, or to keep a choice. */
static void page_command(home_app *app, OraclesHomeCommand c)
{
    const OraclesHomeHost *host = app->host;
    const int game = oracles_home_game(&app->nav);
    char message[256] = "";
    if (!host || game < 0) return;
    if (c == ORACLES_HOME_STORE && host->store) { host->store(host->opaque, &app->nav); return; }
    if (host->file_chosen && ((c == ORACLES_HOME_CHOOSE_ROM && host->choose_rom) || (c == ORACLES_HOME_CHOOSE_PATCH && host->choose_patch))) {
        /* The dialog answers later (dialog_answered); until then the home screen draws but takes no input. */
        app->dialog_open = 1;
        if (c == ORACLES_HOME_CHOOSE_ROM) host->choose_rom(host->opaque, game, app->window);
        else host->choose_patch(host->opaque, game, app->window);
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

/* The file dialog's answer: the ROM or the patch taken for the game it was opened for, or why not. */
static void dialog_answered(home_app *app, int result, int kind, int game, const char *path, int copied)
{
    char message[256] = "";
    app->dialog_open = 0;
    if (app->quit_after_dialog) app->running = 0;
    app->host->file_chosen(app->host->opaque, game, kind, result, path, copied, message, sizeof message);
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
    if (c == ORACLES_HOME_EXIT) app->running = 0;
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
        if (!click) { if (hit.index != app->nav.row) { app->nav.row = hit.index; changed(app); } return; }
        const OraclesHomeCommand c = app->nav.screen == ORACLES_SCREEN_GAME ? oracles_page_click(&app->nav, hit.index)
                                     : app->nav.screen == ORACLES_SCREEN_MODS ? oracles_mods_click(&app->nav, hit.index)
                                                                              : oracles_display_click(&app->nav, hit.index, hit.option);
        changed(app);
        command(app, c);
        return;
    }
    if (!click) {
        if (hit.kind == ORACLES_UI_HIT_ITEM && hit.index != oracles_home_focus(&app->nav)) { oracles_home_hover(&app->nav, hit.index); changed(app); }
        return;
    }
    if (hit.kind == ORACLES_UI_HIT_ITEM) { const OraclesHomeCommand c = oracles_home_click(&app->nav, hit.index); changed(app); command(app, c); }
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
    /* A cell of Controls that waits takes the next key or button, Escape included (it cancels). */
    if (oracles_home_capture(&app->nav, e, &captured)) { changed(app); command(app, captured); return; }
    /* After a game, the first inputs the home screen gets, and its window's focus and screen changes: what a session
     * report needs to tell input that stopped from input that went elsewhere. */
    if (app->inputs_reported < 3 && (e->type == SDL_EVENT_KEY_DOWN || e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e->type == SDL_EVENT_FINGER_DOWN)) {
        app->inputs_reported++;
        fprintf(stderr, "oracles: home screen input: %s %d\n", e->type == SDL_EVENT_KEY_DOWN ? "key" : e->type == SDL_EVENT_FINGER_DOWN ? "finger" : "controller button",
                e->type == SDL_EVENT_KEY_DOWN ? (int)e->key.key : e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? (int)e->gbutton.button : 0);
    }
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
            if (oracles_home_button_action(e, &action)) act(app, action);
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
            oracles_sdl_pads_event(&app->pads, e);
            break;
        case SDL_EVENT_MOUSE_MOTION: pointer(app, e->motion.x, e->motion.y, 0); break;
        case SDL_EVENT_MOUSE_BUTTON_UP: if (e->button.button == SDL_BUTTON_LEFT) pointer(app, e->button.x, e->button.y, 1); break;
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
    if (!SDL_GetRenderOutputSize(app->renderer, &w, &h) || !oracles_ui_draw_begin(app->draw, w, h, now)) return 0;
    app->moving = oracles_ui_home_draw(app->draw, &app->view, &app->nav, now);
    oracles_ui_draw_end(app->draw);
    SDL_RenderPresent(app->renderer);
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
    app->renderer = oracles_sdl_create_renderer(app->window, 1);
    if (!app->renderer) return 0;
    oracles_sdl_pads_open(&app->pads);
    app->draw = oracles_ui_draw_create(app->renderer);
    return app->draw != NULL;
}

int oracles_home_run(const OraclesHomeHost *host, int *width, int *height)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) { fprintf(stderr, "oracles: %s\n", SDL_GetError()); return 1; }
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
        refresh(&app);
        oracles_ui_home_start(&app.view, &app.nav);
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
    SDL_Quit();
    return status;
}
