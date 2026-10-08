/* The SDL backend with the launcher's window lent, without a ROM or a display
 * (SDL's dummy video, a software renderer): a start that fails, here because
 * the audio driver named does not exist, gives the window and its renderer
 * back as they were, and SDL keeps running for the home screen; a start that
 * succeeds does the same at its stop.  A file or a text dropped on the window
 * during the game is left to SDL, which owns it, by the backend that polls it.
 * A controller unplugged during a session, and one plugged in, are what the
 * home screen reads afterwards (SDL's virtual controllers); a Nintendo
 * controller's button labelled A is settings.txt's "a", as with SDL 2, and the
 * names settings.txt keeps for buttons are SDL 3's too.  Under Linux, SDL's
 * file dialog through zenity (a fake one on the PATH) answers on the queue: the
 * path chosen, a cancel, and a zenity that cannot open, which is no dialog, not
 * a cancel; it opens in the ROM's folder. */
#include "backends.h"
#include "file_dialog.h"
#include "home_input.h"
#include "touch_controls.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); failures++; } } while (0)

/* SDL's allocations, to see whether a dropped file's name is freed. */
static const void *watched;
static int watched_freed;
static void *test_malloc(size_t size) { return malloc(size); }
static void *test_calloc(size_t count, size_t size) { return calloc(count, size); }
static void *test_realloc(void *memory, size_t size) { return realloc(memory, size); }
static void test_free(void *memory) { if (memory && memory == watched) watched_freed = 1; free(memory); }

/* An event SDL_EVENT_DROP_FILE or SDL_EVENT_DROP_TEXT polled by the session's backend: it gives the game nothing and
 * leaves the text alone, SDL 3 freeing its own. */
static int dropped_and_left(oracles_host_backend *backend, Uint32 type)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = type;
    char *text = SDL_strdup("C:\\Games\\Oracle of Ages.gbc");
    e.drop.data = text;
    watched = text;
    watched_freed = 0;
    if (!SDL_PushEvent(&e)) return 0;
    oracles_host_event event;
    int has_event = 1, events = 0;
    while (has_event) { if (!backend->poll_event(backend->opaque, &event, &has_event)) return 0; events += has_event; }
    const int left = !watched_freed && events == 0;
    watched = NULL;
    SDL_free(text);
    return left;
}

/* A virtual gamepad, `vendor`:`product` (0 for none), whose buttons the test presses. */
static SDL_JoystickID attach_gamepad(Uint16 vendor, Uint16 product)
{
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.vendor_id = vendor;
    desc.product_id = product;
    desc.naxes = 6;
    desc.nbuttons = 15;
    return SDL_AttachVirtualJoystick(&desc);
}

/* The game's buttons the backend's events press, until none is left. */
static unsigned drained(oracles_host_backend *backend)
{
    unsigned mask = 0;
    oracles_host_event event;
    int has_event = 1;
    while (has_event && backend->poll_event(backend->opaque, &event, &has_event))
        if (has_event && event.type == ORACLES_HOST_EVENT_BUTTON && event.pressed) mask |= event.button;
    return mask;
}

/* The game's buttons `button` of the virtual gamepad presses. */
static unsigned pressed_by(oracles_host_backend *backend, SDL_JoystickID pad, SDL_GamepadButton button)
{
    SDL_Joystick *joystick = SDL_GetJoystickFromID(pad);
    SDL_SetJoystickVirtualButton(joystick, (int)button, true);
    SDL_UpdateJoysticks();
    const unsigned mask = drained(backend);
    SDL_SetJoystickVirtualButton(joystick, (int)button, false);
    SDL_UpdateJoysticks();
    drained(backend);
    return mask;
}

/* The events the backend gives the host until none is left: the game's buttons pressed and released, and pauses. */
static void touch_drain(oracles_host_backend *backend, unsigned *pressed, unsigned *released, int *pauses)
{
    *pressed = *released = 0;
    *pauses = 0;
    oracles_host_event event;
    int has_event = 1;
    while (has_event && backend->poll_event(backend->opaque, &event, &has_event)) {
        if (!has_event) break;
        if (event.type == ORACLES_HOST_EVENT_BUTTON) { if (event.pressed) *pressed |= event.button; else *released |= event.button; }
        if (event.type == ORACLES_HOST_EVENT_PAUSE) (*pauses)++;
    }
}

static int output_w = 1, output_h = 1;   /* the lent renderer's output, which fingers are fractions of */

/* A finger's event at (x, y) in the output's pixels. */
static void finger(SDL_EventType type, SDL_FingerID id, float x, float y)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = type;
    e.tfinger.touchID = 1;
    e.tfinger.fingerID = id;
    e.tfinger.x = x / (float)output_w;
    e.tfinger.y = y / (float)output_h;
    e.tfinger.pressure = 1.0f;
    SDL_PushEvent(&e);
}

static void key(SDL_Keycode code)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = code;
    e.key.down = true;
    SDL_PushEvent(&e);
}

/* The game's menus enlarged: a frame of the view's 480x270 that names a part to show alone (the framed core, 160x144)
 * takes that part's own logical size, so its own largest whole scale; the next whole frame takes the surface's again. */
static void enlarged_menus(SDL_Window *window, SDL_Renderer *renderer)
{
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    oracles_host_backend backend;
    if (!oracles_sdl_backend_init(&backend, &options)) { CHECK(0); return; }
    static uint32_t pixels[480 * 270];
    if (backend.start(backend.opaque, 480, 270, 48000, 0)) {
        int w = 0, h = 0;
        oracles_host_video_frame frame = { pixels, 480, 270, 480 * sizeof(uint32_t), 160, 63, 160, 144 };
        CHECK(backend.present_frame(backend.opaque, &frame));
        CHECK(SDL_GetRenderLogicalPresentation(renderer, &w, &h, NULL) && w == 160 && h == 144);
        frame.crop_w = frame.crop_h = 0;
        CHECK(backend.present_frame(backend.opaque, &frame));
        CHECK(SDL_GetRenderLogicalPresentation(renderer, &w, &h, NULL) && w == 480 && h == 270);
    } else CHECK(0);
    backend.stop(backend.opaque);
    oracles_sdl_backend_release(&backend);
}

/* The touch controls in a session (asked here; on Android always): a finger presses and releases the game's buttons,
 * a cancelled one too; Back (AC_BACK) leaves them shown, a key of the game hides them and lets the d-pad go; the pause
 * asks for the pause menu; a finger held through a pause presses again as it moves.  And the frame they draw directly
 * goes where SDL's integer-scaled logical presentation puts it. */
static void touches(SDL_Window *window, SDL_Renderer *renderer)
{
    static const int frames[3][2] = { { 160, 144 }, { 256, 144 }, { 480, 270 } };
    CHECK(SDL_GetRenderOutputSize(renderer, &output_w, &output_h) && output_w > 0 && output_h > 0);
    for (int i = 0; i < 3; i++) {
        SDL_FRect sdl_rect;
        SDL_SetRenderLogicalPresentation(renderer, frames[i][0], frames[i][1], SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
        CHECK(SDL_GetRenderLogicalPresentationRect(renderer, &sdl_rect));
        const OraclesTouchBox ours = oracles_touch_frame(output_w, output_h, frames[i][0], frames[i][1]);
        CHECK(ours.x == sdl_rect.x && ours.y == sdl_rect.y && ours.w == sdl_rect.w && ours.h == sdl_rect.h);
    }
    SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);

    OraclesTouchLayout l;
    oracles_touch_layout((float)output_w, (float)output_h, &l);
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    options.pause_menu = 1;
    options.touch_controls = 1;
    oracles_host_backend backend;
    if (!oracles_sdl_backend_init(&backend, &options)) { CHECK(0); return; }
    unsigned pressed, released;
    int pauses;
    if (backend.start(backend.opaque, 160, 144, 48000, 0)) {
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        finger(SDL_EVENT_FINGER_DOWN, 1, l.a.x, l.a.y);
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(pressed == ORACLES_KEY_A && released == 0);
        finger(SDL_EVENT_FINGER_CANCELED, 1, l.a.x, l.a.y);
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(pressed == 0 && released == ORACLES_KEY_A);
        finger(SDL_EVENT_FINGER_DOWN, 2, l.dpad.x + 0.7f * l.dpad.r, l.dpad.y);
        key(SDLK_AC_BACK);
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(pressed == ORACLES_KEY_RIGHT && released == 0);
        key(SDLK_X);   /* the game's A, by default */
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(released == ORACLES_KEY_RIGHT && pressed == ORACLES_KEY_A);
        finger(SDL_EVENT_FINGER_UP, 2, l.dpad.x, l.dpad.y);
        finger(SDL_EVENT_FINGER_DOWN, 3, l.pause.x, l.pause.y);
        finger(SDL_EVENT_FINGER_UP, 3, l.pause.x, l.pause.y);
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(pauses == 1 && pressed == 0);
        finger(SDL_EVENT_FINGER_DOWN, 4, l.b.x, l.b.y);
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(pressed == ORACLES_KEY_B);
        oracles_sdl_backend_hold(&backend, 1);   /* the pause menu takes the fingers; the host lets its keys go */
        oracles_sdl_backend_hold(&backend, 0);
        finger(SDL_EVENT_FINGER_MOTION, 4, l.b.x + 1, l.b.y);
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(pressed == ORACLES_KEY_B && released == 0);
        finger(SDL_EVENT_FINGER_UP, 4, l.b.x, l.b.y);
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(released == ORACLES_KEY_B);
    } else {
        CHECK(0);
    }
    backend.stop(backend.opaque);
    oracles_sdl_backend_release(&backend);
}

/* A session in the lent window with only a virtual gamepad `vendor`:`product` plugged in, known by `mapping` (a line
 * of SDL's database after its GUID) when given: the game's buttons its joystick buttons 0 and 1 press, with the default
 * names ("a" for A, "b" for B). */
static void face_buttons(SDL_Window *window, SDL_Renderer *renderer, Uint16 vendor, Uint16 product, const char *mapping,
                         unsigned *south, unsigned *east)
{
    *south = *east = 0;
    const SDL_JoystickID pad = attach_gamepad(vendor, product);
    if (pad && mapping) {
        char guid[64], line[1024];
        SDL_GUIDToString(SDL_GetJoystickGUIDForID(pad), guid, sizeof guid);
        snprintf(line, sizeof line, "%s,%s", guid, mapping);
        SDL_AddGamepadMapping(line);
    }
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    oracles_host_backend backend;
    if (pad && oracles_sdl_backend_init(&backend, &options)) {
        if (backend.start(backend.opaque, 160, 144, 48000, 0)) {
            *south = pressed_by(&backend, pad, SDL_GAMEPAD_BUTTON_SOUTH);
            *east = pressed_by(&backend, pad, SDL_GAMEPAD_BUTTON_EAST);
        }
        backend.stop(backend.opaque);
        oracles_sdl_backend_release(&backend);
    }
    SDL_DetachVirtualJoystick(pad);
}

/* The game's buttons the south and the east buttons of the second of two gamepads press, south's in the low byte. */
static unsigned second_pad_buttons(SDL_Window *window, SDL_Renderer *renderer)
{
    unsigned result = 0;
    const SDL_JoystickID first = attach_gamepad(0, 0), second = attach_gamepad(0, 0);
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    oracles_host_backend backend;
    if (first && second && oracles_sdl_backend_init(&backend, &options)) {
        if (backend.start(backend.opaque, 160, 144, 48000, 0))
            result = pressed_by(&backend, second, SDL_GAMEPAD_BUTTON_SOUTH) | pressed_by(&backend, second, SDL_GAMEPAD_BUTTON_EAST) << 8;
        backend.stop(backend.opaque);
        oracles_sdl_backend_release(&backend);
    }
    SDL_DetachVirtualJoystick(first);
    SDL_DetachVirtualJoystick(second);
    return result;
}

#if !defined(_WIN32) && !defined(__APPLE__)
#include <sys/stat.h>

/* SDL's file dialog of `kind` through a zenity in `folder`, first on the PATH, that prints `output` and exits with
 * `status` (and writes its arguments one a line into folder/arguments), opened on `start` for Seasons: its answer from
 * the queue, the path chosen into `chosen`; -1 when none came for Seasons and that kind. */
static int fake_dialog(const char *folder, OraclesDialogKind kind, const char *start, const char *output, int status, char *chosen, size_t capacity)
{
    char script[1200];
    snprintf(script, sizeof script, "%s/zenity", folder);
    FILE *f = fopen(script, "w");
    if (!f) return -1;
    fprintf(f, "#!/bin/sh\n[ \"$1\" = --version ] && { echo 3.44.0; exit 0; }\nprintf '%%s\\n' \"$@\" > '%s/arguments'\n"
               "printf '%%s' '%s'\nexit %d\n", folder, output, status);
    fclose(f);
    chmod(script, 0700);
    oracles_choose_file(NULL, kind, start, 1);
    for (int i = 0; i < 1000; i++) {
        SDL_Event e;
        OraclesDialogResult result;
        OraclesDialogKind answered;
        int game, copied;
        if (SDL_WaitEventTimeout(&e, 10) && oracles_dialog_answer(&e, &result, &answered, &game, chosen, capacity, &copied))
            return game == 1 && answered == kind && !copied ? (int)result : -1;
    }
    return -1;
}

/* Whether the fake dialog's arguments, read back from folder/arguments, hold `line` right after `after`. */
static int argument_given(const char *folder, const char *after, const char *line)
{
    char name[1200], previous[4200] = "", read[4200];
    snprintf(name, sizeof name, "%s/arguments", folder);
    FILE *f = fopen(name, "r");
    int found = 0;
    while (f && !found && fgets(read, sizeof read, f)) {
        read[strcspn(read, "\n")] = 0;
        found = !strcmp(previous, after) && !strcmp(read, line);
        snprintf(previous, sizeof previous, "%s", read);
    }
    if (f) fclose(f);
    return found;
}

/* The dialog chose a file, was cancelled, could not open: the last is no dialog, which the home screen says with its
 * fallback, "drop the ROM on this window".  It opens in the ROM's folder, titled; a patch's is titled as one. */
static void dialogs(const char *folder)
{
    char chosen[1024], start[1200], opened_in[1300];
    mkdir(folder, 0700);
    snprintf(start, sizeof start, "%s/roms", folder);
    snprintf(opened_in, sizeof opened_in, "%s/", start);
    CHECK(fake_dialog(folder, ORACLES_DIALOG_ROM, start, "/games/Oracle of Seasons.gbc", 0, chosen, sizeof chosen) == ORACLES_DIALOG_CHOSEN);
    CHECK(!strcmp(chosen, "/games/Oracle of Seasons.gbc"));
    CHECK(argument_given(folder, "--filename", opened_in));
    CHECK(argument_given(folder, "--title", "Choose ROM"));
    CHECK(fake_dialog(folder, ORACLES_DIALOG_ROM, "", "", 1, chosen, sizeof chosen) == ORACLES_DIALOG_CANCELLED);
    CHECK(fake_dialog(folder, ORACLES_DIALOG_ROM, "", "", 255, chosen, sizeof chosen) == ORACLES_DIALOG_UNAVAILABLE);
    CHECK(fake_dialog(folder, ORACLES_DIALOG_PATCH, start, "/games/Moonrise.bps", 0, chosen, sizeof chosen) == ORACLES_DIALOG_CHOSEN);
    CHECK(!strcmp(chosen, "/games/Moonrise.bps"));
    CHECK(argument_given(folder, "--title", "Choose patch"));
}
#endif

/* A start then a stop, as oracles_host_run calls them (stop even after a failed start), with this audio driver. */
static int lend(SDL_Window *window, SDL_Renderer *renderer, const char *audio_driver)
{
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, audio_driver);
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    oracles_host_backend backend;
    if (!oracles_sdl_backend_init(&backend, &options)) return -1;
    const int started = backend.start(backend.opaque, 160, 144, 48000, 1);
    if (started) {
        CHECK(dropped_and_left(&backend, SDL_EVENT_DROP_FILE));
        CHECK(dropped_and_left(&backend, SDL_EVENT_DROP_TEXT));
    }
    backend.stop(backend.opaque);
    oracles_sdl_backend_release(&backend);
    return started;
}

/* The home screen's window and renderer still work: the window found by its id, SDL's video on, a frame drawn. */
static int intact(SDL_Window *window, SDL_WindowID id, SDL_Renderer *renderer)
{
    if (SDL_GetWindowFromID(id) != window || SDL_GetRenderer(window) != renderer) return 0;
    if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO)) return 0;
    int w = 0, h = 0;
    SDL_RendererLogicalPresentation presentation = SDL_LOGICAL_PRESENTATION_INTEGER_SCALE;
    SDL_GetRenderLogicalPresentation(renderer, &w, &h, &presentation);
    if (w || h || presentation != SDL_LOGICAL_PRESENTATION_DISABLED) return 0;
    return SDL_SetRenderDrawColor(renderer, 5, 5, 7, 255) && SDL_RenderClear(renderer);
}

/* A copy kept in the application's folder (Android's chosen files) never writes over a file: the same bytes under the
 * same name are taken as they are; other bytes, a save that shares the ROM's name or the other game's ROM, get the
 * next free name; no temporary file is left. */
static void copies(const char *base)
{
    char folder[1200], path[1400], expected[1400];
    snprintf(folder, sizeof folder, "%s/copies", base);
    static const char *const leftovers[] = { "Oracle.gbc", "Oracle (2).gbc", "Oracle (3).gbc", "Oracle.sav", "noext", "noext (2)" };
    for (size_t i = 0; i < sizeof leftovers / sizeof leftovers[0]; i++) {
        snprintf(path, sizeof path, "%s/%s", folder, leftovers[i]);
        SDL_RemovePath(path);
    }
    int fresh = -1;
    CHECK(oracles_dialog_store_copy(folder, "Oracle.gbc", "ages", 4, path, sizeof path, &fresh) && fresh == 1);
    snprintf(expected, sizeof expected, "%s/Oracle.gbc", folder);
    CHECK(!strcmp(path, expected));
    CHECK(oracles_dialog_store_copy(folder, "Oracle.gbc", "ages", 4, path, sizeof path, &fresh) && fresh == 0 && !strcmp(path, expected));
    CHECK(oracles_dialog_store_copy(folder, "Oracle.gbc", "seasons", 7, path, sizeof path, &fresh) && fresh == 1);
    snprintf(expected, sizeof expected, "%s/Oracle (2).gbc", folder);
    CHECK(!strcmp(path, expected));
    CHECK(oracles_dialog_store_copy(folder, "Oracle.gbc", "other", 5, path, sizeof path, &fresh) && fresh == 1);
    snprintf(expected, sizeof expected, "%s/Oracle (3).gbc", folder);
    CHECK(!strcmp(path, expected));
    size_t size = 0;
    snprintf(expected, sizeof expected, "%s/Oracle.gbc", folder);
    void *kept = SDL_LoadFile(expected, &size);
    CHECK(kept && size == 4 && !memcmp(kept, "ages", 4));   /* the first file untouched */
    SDL_free(kept);
    snprintf(expected, sizeof expected, "%s/Oracle.sav", folder);
    CHECK(SDL_SaveFile(expected, "save", 4));
    CHECK(oracles_dialog_store_copy(folder, "Oracle.sav", "chosen", 6, path, sizeof path, &fresh) && fresh == 1 && strcmp(path, expected));
    kept = SDL_LoadFile(expected, &size);
    CHECK(kept && size == 4 && !memcmp(kept, "save", 4));   /* a save sharing the name is never written over */
    SDL_free(kept);
    SDL_RemovePath(path);
    CHECK(oracles_dialog_store_copy(folder, "noext", "a", 1, path, sizeof path, &fresh) && fresh == 1);
    CHECK(oracles_dialog_store_copy(folder, "noext", "b", 1, path, sizeof path, &fresh) && fresh == 1);
    snprintf(expected, sizeof expected, "%s/noext (2)", folder);
    CHECK(!strcmp(path, expected));
    snprintf(expected, sizeof expected, "%s/Oracle.gbc.partial", folder);
    SDL_PathInfo info;
    CHECK(!SDL_GetPathInfo(expected, &info));
}

int main(int argc, char **argv)
{
#if !defined(_WIN32) && !defined(__APPLE__)
    /* SDL's zenity is the first on the PATH, which the fake one heads, as SDL's own environment has it. */
    char path[4096];
    if (argc > 1) {
        snprintf(path, sizeof path, "%s:%s", argv[1], getenv("PATH") ? getenv("PATH") : "/usr/bin:/bin");
        SDL_setenv_unsafe("PATH", path, 1);
        SDL_SetHint(SDL_HINT_FILE_DIALOG_DRIVER, "zenity");
    }
#else
    (void)argc; (void)argv;
#endif
    SDL_SetMemoryFunctions(test_malloc, test_calloc, test_realloc, test_free);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    SDL_Window *window = SDL_CreateWindow("home", 1280, 720, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, SDL_SOFTWARE_RENDERER) : NULL;
    if (!renderer) { fprintf(stderr, "window: %s\n", SDL_GetError()); return 1; }
    const SDL_WindowID id = SDL_GetWindowID(window);
#if !defined(_WIN32) && !defined(__APPLE__)
    if (argc > 1) dialogs(argv[1]);
#endif
    if (argc > 1) copies(argv[1]);

    /* The names settings.txt has always kept for buttons (the defaults, the hotkeys', Controls' captures) are SDL 3's. */
    static const char *const names[] = { "a", "b", "x", "y", "back", "start", "leftshoulder", "rightshoulder", "leftstick", "rightstick", "guide" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        const SDL_GamepadButton button = SDL_GetGamepadButtonFromString(names[i]);
        CHECK(button != SDL_GAMEPAD_BUTTON_INVALID && !strcmp(SDL_GetGamepadStringForButton(button), names[i]));
    }

    /* No audio driver by that name: the loan fails before touching the window. */
    CHECK(lend(window, renderer, "none") == 0);
    CHECK(intact(window, id, renderer));
    CHECK(!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO));

    /* The dummy driver: the session starts, and its stop quits only the audio it started. */
    CHECK(lend(window, renderer, "dummy") == 1);
    CHECK(intact(window, id, renderer));
    CHECK(!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO));
    CHECK(SDL_WasInit(SDL_INIT_GAMEPAD) & SDL_INIT_GAMEPAD);

    /* A failure again after a session that ran. */
    CHECK(lend(window, renderer, "none") == 0);
    CHECK(intact(window, id, renderer));

    /* A controller at the start; unplugged during a session, which consumes the removal; another plugged in there. */
    const SDL_JoystickID first = attach_gamepad(0, 0);
    OraclesSdlPads pads = { { NULL }, 0 };
    oracles_sdl_pads_open(&pads);
    CHECK(first && pads.count == 1 && SDL_GamepadConnected(pads.pad[0]));
    SDL_DetachVirtualJoystick(first);
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    CHECK(pads.count == 1 && !SDL_GamepadConnected(pads.pad[0]));
    const SDL_JoystickID second = attach_gamepad(0, 0);
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    oracles_sdl_pads_open(&pads);
    CHECK(second && pads.count == 1 && SDL_GamepadConnected(pads.pad[0]) && SDL_GetGamepadID(pads.pad[0]) == second);
    /* Two plugged in: both open, opened once each however often asked. */
    const SDL_JoystickID third = attach_gamepad(0, 0);
    oracles_sdl_pads_open(&pads);
    oracles_sdl_pads_open(&pads);
    CHECK(third && pads.count == 2);
    /* Unplugged with none left: the handles go. */
    SDL_DetachVirtualJoystick(second);
    SDL_DetachVirtualJoystick(third);
    oracles_sdl_pads_open(&pads);
    CHECK(pads.count == 0);
    oracles_sdl_pads_close(&pads);


    /* In play, "a" is the south button of a pad like Xbox's, and the button labelled A, on the right, of a Nintendo
     * Switch Pro Controller (057e:2009), as SDL 2 had it; "b" the other. */
    unsigned south = 0, east = 0;
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    touches(window, renderer);
    enlarged_menus(window, renderer);
    face_buttons(window, renderer, 0, 0, NULL, &south, &east);
    CHECK(south == ORACLES_KEY_A && east == ORACLES_KEY_B);
    /* In play, with two gamepads plugged in, the buttons of the second are the game's too, as the first's: SDL gives a
     * pad that is not open as keys (on Android, its A as Enter, the game's Start). */
    CHECK(second_pad_buttons(window, renderer) == (ORACLES_KEY_A | (ORACLES_KEY_B << 8)));
    face_buttons(window, renderer, 0x057e, 0x2009, NULL, &south, &east);
    CHECK(south == ORACLES_KEY_B && east == ORACLES_KEY_A);
    /* An 8BitDo SN30 Pro known by SDL 3's database line, of a standard type: its joystick button 0, labelled A, is "a". */
    face_buttons(window, renderer, 0x2dc8, 0x6101, "8BitDo SN30 Pro,a:b1,b:b0,back:b10,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,"
                 "dpup:h0.1,leftshoulder:b6,leftstick:b13,lefttrigger:b8,leftx:a0,lefty:a1,rightshoulder:b7,rightstick:b14,"
                 "righttrigger:b9,rightx:a3,righty:a4,start:b11,x:b4,y:b3,hint:!SDL_GAMECONTROLLER_USE_BUTTON_LABELS:=1,", &south, &east);
    CHECK(south == ORACLES_KEY_A && east == ORACLES_KEY_B);
    CHECK(intact(window, id, renderer));

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("oracles-launcher-sdl-backend: ok\n");
    return 0;
}
