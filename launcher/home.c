#include "home.h"

#include "backends.h"
#include "file_dialog.h"
#include "home_input.h"
#include "portable.h"
#include "safe_area.h"
#include "filter_preview.h"
#include "sdl_filter.h"
#include "touch_controls.h"
#include "ui_controls_nav.h"
#include "ui_home.h"
#include "ui_page_layout.h"
#include "ui_page_nav.h"
#include "update_check.h"
#ifdef ORACLES_RETROACHIEVEMENTS
#include "home_achievements.h"
#include "ui_achievements_draw.h"
#endif

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

/* Settings' Portable mode turning, on a thread of its own: its copy may take seconds on a slow USB stick, during which the
 * home screen draws "Copying…" and its page takes no input, nor a file dropped on the window.  `done`, set as the thread
 * ends: the loop looks at it too, should the event the thread pushes be lost. */
typedef struct portable_job {
    SDL_Thread *thread;
    SDL_AtomicInt done;
    int on;
    OraclesPortableResult result;
    char next_dir[1024], error[512];
} portable_job;

typedef struct home_app {
    const OraclesHomeHost *host;
    SDL_Window *window;
    SDL_Renderer *renderer;
    int renderer_lost;        /* told the renderer went, not yet the one that follows */
    OraclesSdlPads pads;      /* every gamepad plugged in */
    OraclesUiDraw *draw;
    OraclesFilterPreview preview;   /* Display's preview of the filter */
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
    portable_job portable;
    int achievements_waiting;   /* RetroAchievements' login sent, its answer looked for */
    int secure;                 /* Android: the window kept out of screenshots and the recents while the login is open */
    OraclesUiDisplayDrag display_drag;   /* Display in 4:3, its rows scrolled by a press held */
#ifdef ORACLES_RETROACHIEVEMENTS
    OraclesAchievementsDrag drag;   /* a press held over their page's list */
    double badges_until_ms;         /* their page's last badges looked for until then */
#endif
} home_app;

/* The event the portable mode's thread pushes as it ends, and how often the loop looks for its end meanwhile. */
static Uint32 portable_event;
#define PORTABLE_POLL_MS 100

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
    oracles_filter_preview_reload(&app->preview);   /* a game played meanwhile kept its last picture */
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

/* The game's page asks the host for a file, a folder, or to keep a choice; a choice is kept without a game too
 * (Settings' from the list of fan games). */
static void page_command(home_app *app, OraclesHomeCommand c)
{
    const OraclesHomeHost *host = app->host;
    const int game = oracles_home_game(&app->nav);
    char message[256] = "";
    if (!host || (game < 0 && oracles_home_command_needs_game(c))) return;
    if (c == ORACLES_HOME_STORE && host->store) {
        host->store(host->opaque, &app->nav);
        if (app->nav.display.margin != app->edges.margin) read_edges(app);
        /* The renderer made again for the filter, if need be; without any renderer left, the launcher cannot go on. */
        if (!oracles_filter_apply(app->renderer, app->nav.display.filter)) {
            fprintf(stderr, "oracles: no renderer after the change of filter: the launcher closes\n");
            app->running = 0;
        } else if (oracles_sdl_filter_index() != app->nav.display.filter) {
            /* No renderer for it: the filter in force kept (else none), the row and settings.txt saying it. */
            char message[96];
            snprintf(message, sizeof message, "%s cannot be drawn here: %s %s", oracles_display_filter_names[app->nav.display.filter],
                     oracles_display_filter_names[oracles_sdl_filter_index()], oracles_sdl_filter_kept() ? "kept" : "instead");
            app->nav.display.filter = oracles_sdl_filter_index();
            host->store(host->opaque, &app->nav);
            oracles_ui_home_toast(&app->view, message, now_ms());
        }
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

static int portable_run(void *data)
{
    portable_job *job = data;
    job->result = oracles_portable_switch(job->on, job->next_dir, sizeof job->next_dir, job->error, sizeof job->error);
    SDL_SetAtomicInt(&job->done, 1);
    SDL_Event e;
    SDL_zero(e);
    e.type = portable_event;
    if (!SDL_PushEvent(&e)) fprintf(stderr, "oracles: portable mode: its end is not signalled (%s): the home screen looks for it\n", SDL_GetError());
    return 0;
}

/* The switch done: the settings and Settings' row follow it. */
static void portable_done(home_app *app)
{
    portable_job *job = &app->portable;
    if (!job->thread) return;
    SDL_WaitThread(job->thread, NULL);
    job->thread = NULL;
    static const char *const results[] = { "copied", "nothing to copy", "the other folder's own settings kept", "unchanged", "the folder cannot be written in",
                                           "the copy failed", "unavailable" };
    const unsigned r = (unsigned)job->result;
    fprintf(stderr, "oracles: portable mode turned %s: %s%s%s\n", job->on ? "on" : "off", r < sizeof results / sizeof results[0] ? results[r] : "?",
            job->error[0] ? ": " : "", job->error);
    if (app->host && app->host->portable_turned) app->host->portable_turned(app->host->opaque, &app->nav, job->on, (int)job->result, job->next_dir);
    changed(app);
}

static void portable_start(home_app *app)
{
    portable_job *job = &app->portable;
    if (job->thread) return;
    memset(job, 0, sizeof *job);
    job->on = app->nav.portable_wanted;
    if (!portable_event) portable_event = SDL_RegisterEvents(1);
    if (portable_event) job->thread = SDL_CreateThread(portable_run, "portable mode", job);
    if (job->thread) return;
    /* Without a thread, at once: the screen waits for it. */
    job->result = oracles_portable_switch(job->on, job->next_dir, sizeof job->next_dir, job->error, sizeof job->error);
    if (app->host && app->host->portable_turned) app->host->portable_turned(app->host->opaque, &app->nav, job->on, (int)job->result, job->next_dir);
    changed(app);
}

#ifdef ORACLES_RETROACHIEVEMENTS
/* RetroAchievements' commands: the login sent, its answer awaited by the loop, or given up; the account forgotten; the
 * player's guide in the browser, the launcher staying (without one, its address in a toast). */
static void achievements_command(home_app *app, OraclesHomeCommand c)
{
    if (c == ORACLES_HOME_ACHIEVEMENTS_PASTE) {
        oracles_home_achievements_paste(&app->nav);
    } else if (c == ORACLES_HOME_ACHIEVEMENTS_CANCEL) {
        if (app->achievements_waiting && app->host->achievements_cancel) app->host->achievements_cancel(app->host->opaque);
        app->achievements_waiting = 0;
    } else if (c == ORACLES_HOME_ACHIEVEMENTS_LOGIN) {
        if (app->host && app->host->achievements_login
            && app->host->achievements_login(app->host->opaque, app->nav.achievements.name, app->nav.achievements.password))
            app->achievements_waiting = 1;
        else oracles_achievements_login_answered(&app->nav, -1, NULL);
    } else if (c == ORACLES_HOME_ACHIEVEMENTS_LOGOUT) {
        if (app->host && app->host->achievements_logout) app->host->achievements_logout(app->host->opaque, &app->nav);
    } else if (c == ORACLES_HOME_ACHIEVEMENTS_GUIDE) {
        char url[256], message[320];
        snprintf(url, sizeof url, "https://github.com/%s/blob/main/docs/PLAYING.md#retroachievements", oracles_update_repository());
        if (SDL_OpenURL(url)) { fprintf(stderr, "oracles: achievements: the player's guide opened\n"); return; }
        fprintf(stderr, "oracles: achievements: the player's guide cannot be opened: %s\n", SDL_GetError());
        snprintf(message, sizeof message, "No browser opened the player's guide: %s", url + 8);
        oracles_ui_home_toast(&app->view, message, now_ms());
    }
    changed(app);
}

/* The login's answer, looked for while it is awaited: in, the settings and the account's row follow. */
static void achievements_answer(home_app *app)
{
    char user[ORACLES_HOME_ACCOUNT_LENGTH];
    const int answer = app->host->achievements_answer(app->host->opaque, user, sizeof user);
    if (answer == -2) return;
    app->achievements_waiting = 0;
    oracles_achievements_login_answered(&app->nav, answer, user);
    if (answer == 1 && app->host->store) app->host->store(app->host->opaque, &app->nav);
    if (answer == 1) refresh(app);
    changed(app);
}
#endif

/* Settings' Licences: docs/THIRD_PARTY.md of the public repository in the browser, the launcher staying (without one,
 * its address in a toast). */
static void open_licences(home_app *app)
{
    char url[256], message[320];
    snprintf(url, sizeof url, "https://github.com/%s/blob/main/docs/THIRD_PARTY.md", oracles_update_repository());
    if (SDL_OpenURL(url)) { fprintf(stderr, "oracles: the licences opened\n"); return; }
    fprintf(stderr, "oracles: the licences cannot be opened: %s\n", SDL_GetError());
    snprintf(message, sizeof message, "No browser opened the licenses: %s", url + 8);
    oracles_ui_home_toast(&app->view, message, now_ms());
    changed(app);
}

static void command(home_app *app, OraclesHomeCommand c)
{
    char message[256];
#ifdef ORACLES_RETROACHIEVEMENTS
    if (c == ORACLES_HOME_ACHIEVEMENTS_LOGIN || c == ORACLES_HOME_ACHIEVEMENTS_LOGOUT || c == ORACLES_HOME_ACHIEVEMENTS_GUIDE || c == ORACLES_HOME_ACHIEVEMENTS_CANCEL
        || c == ORACLES_HOME_ACHIEVEMENTS_PASTE) {
        achievements_command(app, c);
        return;
    }
#endif
    if (c == ORACLES_HOME_EXIT) app->running = 0;
    else if (c == ORACLES_HOME_OPEN_UPDATE) {   /* the browser opens, the launcher staying; without one, the page's address */
        if (!oracles_update_check_open(message, sizeof message) && message[0]) oracles_ui_home_toast(&app->view, message, now_ms());
    }
    else if (c == ORACLES_HOME_PORTABLE) portable_start(app);
    else if (c == ORACLES_HOME_OPEN_LICENCES) open_licences(app);
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

/* Display in 4:3 has rows to scroll: a page taller than its room. */
#define DISPLAY_WHEEL_STEP 34.0f
static int display_scrolls(const home_app *app)
{
    return app->nav.screen == ORACLES_SCREEN_DISPLAY && app->nav.layout == ORACLES_UI_LAYOUT_4_3 && oracles_ui_display_scrolled(&app->nav, 1e6f) > 0.0f;
}

static void pointer(home_app *app, float x, float y, int click)
{
    float sx, sy;
#ifdef ORACLES_RETROACHIEVEMENTS
    if (!click && oracles_achievements_detail_open(&app->nav)) return;   /* a hover changes nothing, and the panel's layout measures its text */
#endif
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
    /* Display's row under a faded edge: nothing hovered; a click brings it into sight. */
    if (hit.kind == ORACLES_UI_HIT_ROW && app->nav.screen == ORACLES_SCREEN_DISPLAY && hit.option == ORACLES_UI_DISPLAY_FADED) {
        if (click) { oracles_display_reveal(&app->nav, hit.index); changed(app); }
        return;
    }
    if (hit.kind == ORACLES_UI_HIT_ROW) {
        if (!click) {
            /* On a game's page the action or the button under the pointer takes the highlight too. */
            const unsigned row = app->nav.row;
            const int save_action = app->nav.save_action, eu_action = app->nav.eu_action, choice = app->nav.save_flow.choice;
            if (app->nav.screen == ORACLES_SCREEN_GAME) oracles_page_hover(&app->nav, hit.index, hit.option);
#ifdef ORACLES_RETROACHIEVEMENTS
            else if (app->nav.screen == ORACLES_SCREEN_ACHIEVEMENTS) oracles_achievements_page_hover(&app->nav, hit.index);
#endif
            else app->nav.row = hit.index;
            if (app->nav.row != row || app->nav.save_action != save_action || app->nav.eu_action != eu_action || app->nav.save_flow.choice != choice)
                changed(app);
            return;
        }
#ifdef ORACLES_RETROACHIEVEMENTS
        if (app->nav.screen == ORACLES_SCREEN_ACHIEVEMENTS) { const OraclesHomeCommand c = oracles_achievements_click(&app->nav, hit.index, hit.option); changed(app); command(app, c); return; }
#endif
        const OraclesHomeCommand c = app->nav.screen == ORACLES_SCREEN_GAME ? oracles_page_click(&app->nav, hit.index, hit.option)
                                     : app->nav.screen == ORACLES_SCREEN_MODS ? oracles_mods_click(&app->nav, hit.index)
                                     : app->nav.screen == ORACLES_SCREEN_CHEATS ? oracles_cheats_click(&app->nav, hit.index, hit.option)
                                     : app->nav.screen == ORACLES_SCREEN_ABOUT ? oracles_about_click(&app->nav, hit.index, hit.option)
                                                                              : oracles_display_click(&app->nav, hit.index, hit.option);
        changed(app);
        command(app, c);
        return;
    }
    if (!click) {
        if (hit.kind == ORACLES_UI_HIT_ITEM && hit.index != oracles_home_focus(&app->nav)) { oracles_home_hover(&app->nav, hit.index); changed(app); }
        return;
    }
#ifdef ORACLES_RETROACHIEVEMENTS
    if (hit.kind == ORACLES_UI_HIT_LOGIN) { const OraclesHomeCommand c = oracles_achievements_login_click(&app->nav, (int)hit.index); changed(app); command(app, c); return; }
#endif
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

/* A press on a key of RetroAchievements' keyboard, a controller's: the keyboard stays up for the click that types it. */
static int on_launcher_keyboard(const home_app *app, const SDL_Event *e)
{
#ifdef ORACLES_RETROACHIEVEMENTS
    if (!oracles_achievements_keyboard_shown(&app->nav) || (e->type != SDL_EVENT_MOUSE_BUTTON_DOWN && e->type != SDL_EVENT_FINGER_DOWN)) return 0;
    float x = e->button.x, y = e->button.y, sx, sy;
    if (e->type == SDL_EVENT_FINGER_DOWN) {
        int w = 0, h = 0;
        SDL_GetWindowSize(app->window, &w, &h);
        x = e->tfinger.x * (float)w;
        y = e->tfinger.y * (float)h;
    }
    window_to_scene(app, x, y, &sx, &sy);
    return oracles_ui_achievements_login_hit(&app->nav, sx, sy) >= ORACLES_LOGIN_FIELDS;
#else
    (void)app;
    (void)e;
    return 0;
#endif
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
    if (oracles_home_input_device(e, &device) && !on_launcher_keyboard(app, e) && oracles_home_input(&app->nav, device)) app->dirty = 1;
    /* A cell of Controls that waits takes the next key or button, Escape included (it cancels). */
    if (oracles_home_capture(&app->nav, e, &captured)) {
        /* A trigger taken for the menu is held still: it counts once let go. */
        if (captured == ORACLES_HOME_STORE) app->trigger_held = oracles_home_trigger_held(&app->nav);
        changed(app);
        command(app, captured);
        return;
    }
#ifdef ORACLES_RETROACHIEVEMENTS
    /* RetroAchievements' login takes the keys, the text typed and the controller's buttons while it is open. */
    OraclesHomeCommand typed;
    if (oracles_home_achievements_event(&app->nav, e, &typed)) {
        changed(app);
        command(app, typed);
        return;
    }
    if (e->type == SDL_EVENT_SCREEN_KEYBOARD_SHOWN || e->type == SDL_EVENT_SCREEN_KEYBOARD_HIDDEN) app->dirty = 1;
#endif
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
    /* The update check's answer (update_check.h), the portable mode's switch done. */
    if (oracles_update_check_event() && e->type == oracles_update_check_event()) read_update(app);
    if (portable_event && e->type == portable_event) portable_done(app);
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
        case SDL_EVENT_MOUSE_MOTION:
            /* Display's rows in 4:3, taller than their room, follow a drag (a finger's too). */
            if ((e->motion.state & SDL_BUTTON_LMASK) && display_scrolls(app)) {
                float x0, y0, x1, y1;
                int window_h = 0;
                SDL_GetWindowSize(app->window, NULL, &window_h);
                window_to_scene(app, 0.0f, 0.0f, &x0, &y0);
                window_to_scene(app, 0.0f, e->motion.yrel, &x1, &y1);
                if (oracles_ui_display_drag_moved(&app->display_drag, e->motion.y, (float)window_h)) {
                    const float before = app->nav.display_scroll;
                    app->nav.display_scroll = oracles_ui_display_scrolled(&app->nav, y0 - y1);
                    if (app->nav.display_scroll != before) changed(app);
                    break;
                }
            }
#ifdef ORACLES_RETROACHIEVEMENTS
            /* The page's list follows a drag (a finger's too), a row at a time. */
            if (oracles_achievements_page_list_on(&app->nav) && (e->motion.state & SDL_BUTTON_LMASK)) {
                int window_h = 0;
                SDL_GetWindowSize(app->window, NULL, &window_h);
                const float row = window_h > 0 ? (float)window_h / 10.0f : 48.0f;
                const int steps = oracles_achievements_drag_steps(&app->nav, &app->drag, e->motion.xrel, e->motion.yrel, row);
                if (steps) { oracles_achievements_page_scroll(&app->nav, steps); changed(app); }
                break;
            }
#endif
            pointer(app, e->motion.x, e->motion.y, 0);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            oracles_ui_display_drag_start(&app->display_drag, e->button.y);
#ifdef ORACLES_RETROACHIEVEMENTS
            oracles_achievements_drag_start(&app->drag);
#endif
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            /* Display's rows in 4:3, a notch a third of a row. */
            if (display_scrolls(app) && e->wheel.y != 0.0f) {
                const float before = app->nav.display_scroll;
                app->nav.display_scroll = oracles_ui_display_scrolled(&app->nav, e->wheel.y > 0.0f ? -DISPLAY_WHEEL_STEP : DISPLAY_WHEEL_STEP);
                if (app->nav.display_scroll != before) changed(app);
                break;
            }
#ifdef ORACLES_RETROACHIEVEMENTS
            if (oracles_achievements_page_list_on(&app->nav) && e->wheel.y != 0.0f) {
                oracles_achievements_page_scroll(&app->nav, e->wheel.y > 0.0f ? -1 : 1);
                changed(app);
            }
#endif
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (app->display_drag.dragged) { app->display_drag.dragged = 0; break; }   /* the end of a drag clicks nothing */
#ifdef ORACLES_RETROACHIEVEMENTS
            if (oracles_achievements_drag_end(&app->drag)) break;   /* the end of a drag clicks nothing */
#endif
            if (e->button.button == SDL_BUTTON_LEFT) pointer(app, e->button.x, e->button.y, 1);
            break;
        case SDL_EVENT_DROP_FILE:   /* SDL frees its copy */
            if (!app->portable.thread) drop(app, e->drop.data);
            break;
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
#ifdef ORACLES_RETROACHIEVEMENTS
    oracles_achievements_detail_follow(&app->nav);
#endif
    oracles_ui_draw_layout(app->draw, oracles_ui_home_scene(&app->nav));
    const OraclesTouchBox safe = oracles_touch_band(w, h, &app->edges);
    oracles_ui_draw_band(app->draw, (int)safe.y, (int)safe.h);
    /* Filter's rules: whether the device runs the shaders, asked once Picture opens; its preview. */
    if (app->nav.picture_page) app->nav.display.filters_basic = !oracles_sdl_filters_runnable();
    app->nav.display.filters_refused = oracles_sdl_filters_refused_codes();
    app->preview.drawn = oracles_display_filter_drawn(&app->nav);
    oracles_ui_draw_set_inset(app->draw, oracles_filter_preview_draw, &app->preview);
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
static int event_wait_ms(const home_app *app, double now)
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

/* The same, but at most PORTABLE_POLL_MS while the portable mode turns. */
static int wait_ms(const home_app *app, double now)
{
    const int wait = event_wait_ms(app, now);
#ifdef ORACLES_RETROACHIEVEMENTS
    const int polled = app->portable.thread || app->achievements_waiting || app->nav.achievements.list_busy || now < app->badges_until_ms;
#else
    const int polled = app->portable.thread || app->achievements_waiting;
#endif
    return polled && (wait < 0 || wait > PORTABLE_POLL_MS) ? PORTABLE_POLL_MS : wait;
}

/* oracles_sdl_renderer_swap's news: the drawing's textures released, then made again on the renderer that follows. */
static void renderer_heard(void *opaque, SDL_Renderer *renderer)
{
    home_app *app = opaque;
    if (!renderer || renderer != app->renderer || app->renderer_lost) {   /* the same address may come back */
        oracles_ui_draw_rebind(app->draw, renderer);
        oracles_filter_preview_forget(&app->preview);
    }
    app->renderer_lost = !renderer;
#ifdef ORACLES_RETROACHIEVEMENTS
    if (!renderer) oracles_ui_achievements_forget_badges();
#endif
    app->renderer = renderer;   /* NULL until the next one, if any */
    app->dirty = 1;
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
    if (app->draw) oracles_sdl_renderer_listen(renderer_heard, app);
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
            if (app.portable.thread && SDL_GetAtomicInt(&app.portable.done)) portable_done(&app);
#ifdef ORACLES_RETROACHIEVEMENTS
            if (app.achievements_waiting) achievements_answer(&app);
            oracles_home_achievements_text_input(app.window, &app.nav);
            if (app.host && app.host->achievements_list && app.host->achievements_list(app.host->opaque, &app.nav)) app.dirty = 1;
            /* The badges of the page's list, come one by one. */
            if (oracles_achievements_badges_redraw(app.nav.achievements.list_busy, oracles_ui_achievements_list_waiting(), now_ms(), &app.badges_until_ms)) app.dirty = 1;
            /* Android's recents take their thumbnail before the window hides: nothing of the login in it. */
            const int secure = app.nav.achievements.login != 0;
            if (secure != app.secure) oracles_android_secure_window(app.secure = secure);
#endif
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
    portable_done(&app);   /* a switch the window was closed during ends, and its settings are written */
#ifdef ORACLES_RETROACHIEVEMENTS
    /* The page's list let go with the window. */
    app.nav.screen = ORACLES_SCREEN_HOME;
    if (app.host && app.host->achievements_list) app.host->achievements_list(app.host->opaque, &app.nav);
#endif
    *width = app.width;
    *height = app.height;
    oracles_filter_preview_forget(&app.preview);
    oracles_ui_draw_destroy(app.draw);
    oracles_sdl_pads_close(&app.pads);
    oracles_sdl_renderer_unlisten(&app);
    oracles_sdl_filter_forget(app.renderer);
    if (app.renderer) SDL_DestroyRenderer(app.renderer);
    oracles_sdl_gpu_device_release();
    if (app.window) SDL_DestroyWindow(app.window);
    oracles_update_check_close();   /* no event pushed past SDL_Quit */
    SDL_Quit();
    return status;
}
