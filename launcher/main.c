/* The Oracles Project launcher: the home screen, or a game straight from the command line.
 *
 *   the-oracles-project [game options]             the home screen
 *   the-oracles-project --rom PATH [--save PATH] [--scale N] [--fullscreen]
 *                       [--colour-correction on|off] [--vsync auto|on|off] [--camera 1|2]
 *                       [--mute] [--frames N] [--no-window]
 *                       [--screenshot PATH.ppm] [--record ROUTE] [--play ROUTE] [--continuous-transitions] [--continuous-swim] [--item-hotkeys=off|use|equip]
 *                       [--zoom-out]
 *   the-oracles-project --launcher-screenshot PATH.ppm [--launcher-size WxH] [--launcher-input up,down,...]
 *
 * With --rom the game starts at once, as the harness, the routes and the
 * tests run it; --save, --frames, --no-window, --screenshot and --play need
 * it.  Without, the home screen opens (--record then records the first game
 * it starts), and the game options given
 * there (a view, the transitions, the item hotkeys) win over Display's and
 * the settings' choices for the games it starts.
 * The game itself is session.c.
 *
 * Player choices (colour correction, vsync, the Enhanced camera profile,
 * keys, controller buttons, each game's ROM, the profile, the launcher
 * window) live in settings.txt under the per-user settings directory; the
 * file is written with every setting so that it can be edited.  An option on
 * the command line holds for its run alone: the file changes when
 * it is edited, by Display and Controls, and by F2 in game. */
#include "backends.h"
#include "home.h"
#include "home_games.h"
#include "session.h"
#include "settings.h"

/* SDL's entry point where a platform needs one (Windows: the command line in UTF-8; Android: the activity calls it). */
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_cpuinfo.h>
#include <SDL3/SDL_hints.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include <SDL3/SDL_system.h>
#include <android/log.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

/* Android keeps no process's standard error: each line the launcher writes there (a session's report, a refusal) goes
 * to the system's log instead, tagged "the-oracles-project" (adb logcat -s the-oracles-project), and to a file in the
 * application's folder on the shared storage, beside the ROMs and reachable over USB, so that a player can send a
 * session's report without a computer: the-oracles-project.log for this run, the one before kept as
 * the-oracles-project.previous.log. */
static char log_path[1024];

static void *forward_stderr(void *opaque)
{
    FILE *in = fdopen((int)(intptr_t)opaque, "r");
    FILE *file = log_path[0] ? fopen(log_path, "w") : NULL;
    char line[1024];
    while (in && fgets(line, sizeof line, in)) {
        __android_log_write(ANDROID_LOG_INFO, "the-oracles-project", line);
        if (file) { fputs(line, file); fflush(file); }   /* a line at a time: the file holds what was said if the system ends the process */
    }
    if (file) fclose(file);
    return NULL;
}

static void log_stderr(void)
{
    const char *base = SDL_GetAndroidExternalStoragePath();
    if (base) {
        char previous[sizeof log_path];
        snprintf(log_path, sizeof log_path, "%s/the-oracles-project.log", base);
        snprintf(previous, sizeof previous, "%s/the-oracles-project.previous.log", base);
        rename(log_path, previous);
    }
    int pipes[2];
    if (pipe(pipes) != 0) return;
    dup2(pipes[1], STDERR_FILENO);
    close(pipes[1]);
    setvbuf(stderr, NULL, _IOLBF, 0);
    pthread_t thread;
    if (pthread_create(&thread, NULL, forward_stderr, (void *)(intptr_t)pipes[0]) == 0) pthread_detach(thread);
}
#endif

#define SCREENSHOT_WIDTH 1920
#define SCREENSHOT_HEIGHT 1080

static int usage(void)
{
    fprintf(stderr, "usage: the-oracles-project [--rom PATH [--patch PATCH.bps]] [--save PATH] [--scale N] [--fullscreen] [--colour-correction on|off]\n"
                    "                           [--vsync auto|on|off] [--camera 1|2] [--mute] [--frames N] [--no-window] [--screenshot PATH.ppm]\n"
                    "                           [--record ROUTE] [--play ROUTE] [--diagnostics] [--native-renderer] [--enhanced] [--neighbours off|static (static)] [--continuous-transitions]\n"
                    "                           [--continuous-swim]   (the continuous transitions with Link swimming at the surface too; implies --continuous-transitions)\n"
                    "                           [--zoom-out]   (Enhanced drawn back, 480x270 or 480x360 in 4:3, three rooms across outdoors; --fullscreen --scale 4 fills 1920x1080; --view far)\n"
                    "                           [--view near|medium|far]   (Enhanced: how much of the world it shows, in the screen's shape)\n"
                    "                           [--core sameboy|mgba]   (the core: Accurate, the reference, or Fast, lighter)\n"
                    "                           [--aspect auto|16:9|4:3]   (the view's shape: the screen's, or the one named)\n"
                    "                           [--ghosts auto|1|2]   (Enhanced: the ghosts preparing the rooms around at once, for this run; auto: two on four processor threads or more)\n"
                    "                           [--mods DIR]...   (a mod in Lua, run during the game, --mods repeated for up to 8; its conversations hold the keys and draw over the screen)\n"
                    "                           [--start-at-house NAME]   (with --mods: every file of the save starts in front of the house NAME, or MOD/NAME)\n"
                    "                           [--item-hotkeys=off|use|equip]   (item hotkeys: four keys that use or equip an item without the menu, for this run; the slots and the keys are remembered)\n"
                    "       --patch: a fan game, the BPS patch applied to --rom in memory at each start; its save beside the patch\n"
                    "       without --rom, the home screen opens; the game options apply to the games it starts, --record to the first\n"
                    "       the-oracles-project --launcher-screenshot PATH.ppm [--launcher-size WxH] [--launcher-input up,down,left,right,ok,back]\n"
                    "                           [--launcher-frame GAME.ppm]\n"
                    "                           (the home screen drawn offscreen, 1920x1080 by default, and written as a PPM; with a game's image\n"
                    "                           from --screenshot, the pause menu over it, as the game's window shows it)\n");
    return 2;
}

typedef struct launcher_options {
    int window_given;                /* --scale or --fullscreen: the window of the games the home screen starts */
    const char *screenshot, *inputs, *frame;
    int width, height;
} launcher_options;

/* Returns 0 when the arguments are understood. */
static int parse(int argc, char **argv, OraclesSessionOptions *o, launcher_options *l)
{
    for (int i = 1; i < argc; i++) {
        const int more = i + 1 < argc;
        if (!strcmp(argv[i], "--rom") && more) o->rom_path = argv[++i];
        else if (!strcmp(argv[i], "--patch") && more) o->patch_path = argv[++i];
        else if (!strcmp(argv[i], "--save") && more) o->save_path = argv[++i];
        else if (!strcmp(argv[i], "--scale") && more) { o->sdl.scale = (unsigned)strtoul(argv[++i], NULL, 10); l->window_given = 1; }
        else if (!strcmp(argv[i], "--fullscreen")) { o->sdl.fullscreen = 1; l->window_given = 1; }
        else if (!strcmp(argv[i], "--colour-correction") && more) o->colour_option = argv[++i];
        else if (!strcmp(argv[i], "--vsync") && more) o->vsync_option = argv[++i];
        else if (!strcmp(argv[i], "--camera") && more) {
            const char *camera = argv[++i];
            if (strcmp(camera, "1") != 0 && strcmp(camera, "2") != 0) return 1;
            o->camera_profile = atoi(camera);
        }
        else if (!strcmp(argv[i], "--mute")) o->mute = 1;
        else if (!strcmp(argv[i], "--frames") && more) o->frames = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--no-window")) o->no_window = 1;
        else if (!strcmp(argv[i], "--screenshot") && more) o->screenshot = argv[++i];
        else if (!strcmp(argv[i], "--record") && more) o->record_path = argv[++i];
        else if (!strcmp(argv[i], "--play") && more) o->play_path = argv[++i];
        else if (!strcmp(argv[i], "--diagnostics")) o->diagnostics = 1;
        else if (!strcmp(argv[i], "--native-renderer")) o->native_renderer = 1;
        else if (!strcmp(argv[i], "--enhanced")) o->enhanced = 1;
        else if (!strcmp(argv[i], "--neighbours") && more) {
            /* The objects of the neighbouring rooms: off, or their state of apparition. */
            const char *level = argv[++i];
            if (!strcmp(level, "static")) { o->neighbour_objects = 1; o->enhanced = 1; }
            else if (!strcmp(level, "off")) o->neighbour_objects = 0;
            else return 1;
        }
        else if (!strcmp(argv[i], "--continuous-transitions")) { o->continuous_transitions = 1; o->enhanced = 1; }
        else if (!strcmp(argv[i], "--continuous-swim")) { o->continuous_swim = o->continuous_transitions = 1; o->enhanced = 1; }
        else if (!strcmp(argv[i], "--zoom-out")) { o->zoom_out = 1; o->enhanced = 1; o->view = 3; }
        else if (!strcmp(argv[i], "--view") && more) {
            const char *name = argv[++i];
            o->view = !strcmp(name, "near") ? 1 : !strcmp(name, "medium") ? 2 : !strcmp(name, "far") ? 3 : 0;
            if (!o->view) return 1;
            o->enhanced = 1;
        }
        else if (!strcmp(argv[i], "--core") && more) {
            const char *name = argv[++i];
            o->core = 0;
            for (int c = 0; c < 2; c++) if (!strcmp(name, oracles_settings_core_names[c])) o->core = c + 1;
            if (!o->core) return 1;
        }
        else if (!strcmp(argv[i], "--ghosts") && more) {
            const char *name = argv[++i];
            o->ghosts = !strcmp(name, "auto") ? 1 : !strcmp(name, "1") ? 2 : !strcmp(name, "2") ? 3 : 0;
            if (!o->ghosts) return 1;
        }
        else if (!strcmp(argv[i], "--aspect") && more) {
            const char *name = argv[++i];
            o->aspect = 0;
            for (int a = 0; a < 3; a++) if (!strcmp(name, oracles_settings_aspect_names[a])) o->aspect = a + 1;
            if (!o->aspect) return 1;
        }
        else if (!strcmp(argv[i], "--mods") && i + 1 < argc) { if (o->mods_count == 8u) return 1; o->mods_dirs[o->mods_count++] = argv[++i]; }
        else if (!strcmp(argv[i], "--start-at-house") && more) o->start_house = argv[++i];
        else if (!strncmp(argv[i], "--item-hotkeys=", 15)) o->hotkeys_option = argv[i] + 15;
        else if (!strcmp(argv[i], "--launcher-screenshot") && more) l->screenshot = argv[++i];
        else if (!strcmp(argv[i], "--launcher-size") && more) { if (sscanf(argv[++i], "%dx%d", &l->width, &l->height) != 2) return 1; }
        else if (!strcmp(argv[i], "--launcher-input") && more) l->inputs = argv[++i];
        else if (!strcmp(argv[i], "--launcher-frame") && more) l->frame = argv[++i];
        else return 1;
    }
    if (o->sdl.scale == 0 || o->sdl.scale > 16) return 1;
    if (o->colour_option && strcmp(o->colour_option, "on") != 0 && strcmp(o->colour_option, "off") != 0) return 1;
    if (o->vsync_option && strcmp(o->vsync_option, "on") != 0 && strcmp(o->vsync_option, "off") != 0 && strcmp(o->vsync_option, "auto") != 0) return 1;
    if (o->start_house && (!o->mods_count || o->play_path)) return 1;
    if (o->hotkeys_option && strcmp(o->hotkeys_option, "off") != 0 && strcmp(o->hotkeys_option, "use") != 0 && strcmp(o->hotkeys_option, "equip") != 0) return 1;
    if (l->width <= 0 || l->height <= 0 || l->width > 16384 || l->height > 16384) return 1;
    /* Without a window there is no home screen, and a save, a route to play, a frame count or a screenshot belong to
     * the game --rom starts: the home screen's games have their own save and no end.  The view and gameplay options
     * stay, and --record records the first game the home screen starts. */
    if (!o->rom_path && (o->no_window || o->save_path || o->play_path || o->frames || o->screenshot || o->patch_path)) return 1;
    return 0;
}

/* Remembered choices: defaults, then the settings file.  The command line's --colour-correction, --vsync and --camera
 * are in the session's options: they hold for the games of this run and are never written. */
static void load_settings(oracles_settings *prefs, const OraclesSessionOptions *o)
{
    memset(prefs, 0, sizeof *prefs);
    oracles_settings_defaults(prefs);
    if (!o->no_window) {
        char dir[ORACLES_SETTINGS_PATH_LENGTH - 16];
        if (oracles_sdl_settings_dir(dir, sizeof dir)) snprintf(prefs->path, sizeof prefs->path, "%ssettings.txt", dir);
    }
    oracles_settings_load(prefs);
    /* A first opening takes the device's quality: what weighs on it set for it, Display's Quality to change. */
    if (prefs->first_run) {
        const OraclesQuality quality = oracles_settings_default_quality(oracles_sdl_fullscreen_only(), SDL_GetNumLogicalCPUCores(), SDL_GetSystemRAM());
        oracles_settings_apply_quality(prefs, quality);
        fprintf(stderr, "oracles: a first opening: the %s quality, for %d processor threads and %d MB\n", oracles_settings_quality_names[quality],
                SDL_GetNumLogicalCPUCores(), SDL_GetSystemRAM());
    }
}

int main(int argc, char **argv)
{
#if defined(__linux__) && !defined(__ANDROID__)
    /* X11 first on a Wayland desktop that has XWayland, as SDL 2 had it: SDL 3 would take Wayland where the compositor
     * has fifo-v1, and there a window that does not ask for the display's pixel density is drawn at one pixel a point
     * and stretched by the compositor, text and pixels blurred at 125 % or 150 %.  Elsewhere SDL chooses, a console
     * without X or Wayland (KMSDRM) included; SDL_VIDEO_DRIVER in the environment still chooses. */
    if (getenv("WAYLAND_DISPLAY") && getenv("DISPLAY")) SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11,wayland");
#endif
#ifdef __ANDROID__
    log_stderr();
    /* Landscape either way up: SDL would otherwise let a resizable window follow the device, portrait on a phone. */
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    /* Back is the launcher's Escape (SDLK_AC_BACK), not the end of the activity. */
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
#endif
    OraclesSessionOptions options;
    oracles_session_defaults(&options);
    launcher_options launcher = { 0, NULL, NULL, NULL, SCREENSHOT_WIDTH, SCREENSHOT_HEIGHT };
    if (parse(argc, argv, &options, &launcher) != 0) return usage();

    /* The settings are allocated: they hold a path per game. */
    oracles_settings *prefs = malloc(sizeof *prefs);
    if (!prefs) { fprintf(stderr, "oracles: out of memory\n"); return 1; }
    load_settings(prefs, &options);
    static OraclesHomeGames games;   /* with a refused path per game: kept off the stack */
    games.settings = prefs;
    games.options = &options;
    games.window_from_command_line = launcher.window_given;
    OraclesHomeHost host;
    oracles_home_games_host(&games, &host);
    int status;
    if (launcher.screenshot) {
        /* A capture reads the settings (the games' ROMs) and never writes them. */
        char error[256];
        status = oracles_home_screenshot(&host, launcher.screenshot, launcher.width, launcher.height, launcher.inputs, launcher.frame, error, sizeof error);
        if (status) fprintf(stderr, "oracles: %s\n", error);
    } else if (options.rom_path) {
        oracles_settings_store(prefs); /* also writes the defaults the first time, so they can be edited */
        if (options.enhanced && !options.no_window) options.screen_4_3 = oracles_session_view_4_3(&options, prefs, NULL);   /* the view's shape */
        OraclesSessionResult result;
        status = oracles_session_run(&options, prefs, &result);
    } else {
        oracles_settings_store(prefs);
        status = oracles_home_run(&host, &prefs->launcher_width, &prefs->launcher_height);
        oracles_settings_store(prefs);
    }
    free(prefs);
    return status;
}
