/* The controller's Escape, without a ROM or a display (SDL's dummy video, a software renderer, SDL's virtual
 * controllers): the left stick's click, Guide, or the button or trigger menu_pad= names ask for the pause menu, never
 * the game, and are Back on the home screen and in the pause, where they give up a capture; the device an event comes
 * from, which the key hints follow, kept from the home screen to the pause and back; the toast over the first game
 * started with a controller, shown once it was drawn. */
#include "backends.h"
#include "home_input.h"
#include "pause.h"
#include "ui_draw.h"
#include "ui_page_layout.h"
#include "ui_controls_nav.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); failures++; } } while (0)

/* A virtual gamepad whose buttons and triggers the test moves. */
static SDL_JoystickID attach_gamepad(void)
{
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = 6;
    desc.nbuttons = 15;
    return SDL_AttachVirtualJoystick(&desc);
}

/* What a session's backend gave the host until none was left: pauses, ends, the game's buttons pressed, hotkeys'
 * commands. */
typedef struct menu_events {
    int pauses, quits, commands;
    unsigned pressed;
} menu_events;

static menu_events polled(oracles_host_backend *backend)
{
    menu_events got = { 0, 0, 0, 0 };
    oracles_host_event event;
    int has_event = 1;
    while (has_event && backend->poll_event(backend->opaque, &event, &has_event) && has_event) {
        if (event.type == ORACLES_HOST_EVENT_PAUSE) got.pauses++;
        if (event.type == ORACLES_HOST_EVENT_QUIT) got.quits++;
        if (event.type == ORACLES_HOST_EVENT_COMMAND) got.commands++;
        if (event.type == ORACLES_HOST_EVENT_BUTTON && event.pressed) got.pressed |= event.button;
    }
    return got;
}

/* A press then a release of `button` of the virtual gamepad, both polled. */
static menu_events press(oracles_host_backend *backend, SDL_JoystickID pad, SDL_GamepadButton button)
{
    SDL_Joystick *joystick = SDL_GetJoystickFromID(pad);
    SDL_SetJoystickVirtualButton(joystick, (int)button, true);
    SDL_UpdateJoysticks();
    menu_events got = polled(backend);
    SDL_SetJoystickVirtualButton(joystick, (int)button, false);
    SDL_UpdateJoysticks();
    const menu_events released = polled(backend);
    got.pauses += released.pauses;
    got.quits += released.quits;
    got.commands += released.commands;
    got.pressed |= released.pressed;
    return got;
}

/* The trigger `axis` of the virtual gamepad moved to `percent` of its travel: a virtual gamepad's trigger reads its
 * joystick axis from SDL_JOYSTICK_AXIS_MIN, released, to the maximum. */
static void set_trigger(SDL_JoystickID pad, SDL_GamepadAxis axis, int percent)
{
    const long value = SDL_JOYSTICK_AXIS_MIN + (long)(SDL_JOYSTICK_AXIS_MAX - SDL_JOYSTICK_AXIS_MIN) * percent / 100;
    SDL_SetJoystickVirtualAxis(SDL_GetJoystickFromID(pad), (int)axis, (Sint16)value);
    SDL_UpdateJoysticks();
}

static menu_events pull(oracles_host_backend *backend, SDL_JoystickID pad, SDL_GamepadAxis axis, int percent)
{
    set_trigger(pad, axis, percent);
    return polled(backend);
}

static int nothing(menu_events got) { return !got.pauses && !got.quits && !got.commands && !got.pressed; }
static int one_pause(menu_events got) { return got.pauses == 1 && !got.quits && !got.commands && !got.pressed; }

/* A session (`pause_menu`: the home screen's) with the menu's input `menu` (NULL: the default) and the first hotkey
 * slot's button `hotkey` (NULL: none). */
static int menu_session(oracles_host_backend *backend, SDL_Window *window, SDL_Renderer *renderer, int pause_menu, const char *menu, const char *hotkey)
{
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    options.pause_menu = pause_menu;
    options.menu_pad_name = menu;
    options.hotkey_pad_names[0] = hotkey;
    if (!oracles_sdl_backend_init(backend, &options)) return 0;
    if (backend->start(backend->opaque, 160, 144, 48000, 0)) { polled(backend); return 1; }
    backend->stop(backend->opaque);
    oracles_sdl_backend_release(backend);
    return 0;
}

static void menu_end(oracles_host_backend *backend)
{
    backend->stop(backend->opaque);
    oracles_sdl_backend_release(backend);
}

/* The controller's Escape in play: the left stick's click by default and Guide always ask for the pause menu (nothing
 * in a --rom session, which has none), and neither, nor their releases, reaches the game; another button free of the game and of the
 * hotkeys, or a trigger past half its travel, can be it instead; one the game or a hotkey has stays theirs, Guide
 * still asking. */
static void menu_inputs(SDL_Window *window, SDL_Renderer *renderer)
{
    const SDL_JoystickID pad = attach_gamepad();
    oracles_host_backend backend;
    CHECK(pad != 0);
    if (menu_session(&backend, window, renderer, 1, NULL, NULL)) {
        /* What asked for the pause, which its key hints follow. */
        CHECK(oracles_sdl_backend_menu_source(&backend) == ORACLES_SDL_MENU_FROM_NONE);
        CHECK(one_pause(press(&backend, pad, SDL_GAMEPAD_BUTTON_LEFT_STICK)));
        CHECK(oracles_sdl_backend_menu_source(&backend) == ORACLES_SDL_MENU_FROM_PAD);
        CHECK(one_pause(press(&backend, pad, SDL_GAMEPAD_BUTTON_GUIDE)));
        CHECK(nothing(press(&backend, pad, SDL_GAMEPAD_BUTTON_RIGHT_STICK)));
        CHECK(nothing(pull(&backend, pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 100)));
        CHECK(nothing(pull(&backend, pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 0)));
        CHECK(press(&backend, pad, SDL_GAMEPAD_BUTTON_SOUTH).pressed == ORACLES_KEY_A);
        /* The device of the last input in play, which the reminder over the game names the key of. */
        CHECK(oracles_sdl_backend_last_input(&backend) == ORACLES_SDL_MENU_FROM_PAD);
        SDL_Event key;
        SDL_zero(key);
        key.type = SDL_EVENT_KEY_DOWN;
        key.key.key = SDLK_M;
        SDL_PushEvent(&key);
        CHECK(nothing(polled(&backend)) && oracles_sdl_backend_last_input(&backend) == ORACLES_SDL_MENU_FROM_KEY);
        menu_end(&backend);
    } else CHECK(0);
    /* Where there is no pause menu (--rom): neither the button, nor Guide, nor a trigger end the session, as before
     * the controller had an Escape; Escape still does. */
    if (menu_session(&backend, window, renderer, 0, NULL, NULL)) {
        CHECK(nothing(press(&backend, pad, SDL_GAMEPAD_BUTTON_LEFT_STICK)));
        CHECK(nothing(press(&backend, pad, SDL_GAMEPAD_BUTTON_GUIDE)));
        SDL_Event e;
        SDL_zero(e);
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.key = SDLK_ESCAPE;
        SDL_PushEvent(&e);
        const menu_events got = polled(&backend);
        CHECK(got.quits == 1 && !got.pauses);
        menu_end(&backend);
    } else CHECK(0);
    if (menu_session(&backend, window, renderer, 0, "righttrigger", NULL)) {
        CHECK(nothing(pull(&backend, pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 100)));
        CHECK(nothing(pull(&backend, pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0)));
        menu_end(&backend);
    } else CHECK(0);
    /* Moved to the right stick's click: the left one's is nobody's. */
    if (menu_session(&backend, window, renderer, 1, "rightstick", NULL)) {
        CHECK(one_pause(press(&backend, pad, SDL_GAMEPAD_BUTTON_RIGHT_STICK)));
        CHECK(nothing(press(&backend, pad, SDL_GAMEPAD_BUTTON_LEFT_STICK)));
        menu_end(&backend);
    } else CHECK(0);
    /* None: Guide alone. */
    if (menu_session(&backend, window, renderer, 1, "", NULL)) {
        CHECK(nothing(press(&backend, pad, SDL_GAMEPAD_BUTTON_LEFT_STICK)));
        CHECK(one_pause(press(&backend, pad, SDL_GAMEPAD_BUTTON_GUIDE)));
        menu_end(&backend);
    } else CHECK(0);
    /* The game's A, and the d-pad, stay the game's. */
    if (menu_session(&backend, window, renderer, 1, "a", NULL)) {
        const menu_events got = press(&backend, pad, SDL_GAMEPAD_BUTTON_SOUTH);
        CHECK(got.pressed == ORACLES_KEY_A && !got.pauses);
        menu_end(&backend);
    } else CHECK(0);
    if (menu_session(&backend, window, renderer, 1, "dpup", NULL)) {
        const menu_events got = press(&backend, pad, SDL_GAMEPAD_BUTTON_DPAD_UP);
        CHECK(got.pressed == ORACLES_KEY_UP && !got.pauses);
        menu_end(&backend);
    } else CHECK(0);
    /* A hotkey slot's button stays the slot's; Guide still asks. */
    if (menu_session(&backend, window, renderer, 1, NULL, "leftstick")) {
        const menu_events got = press(&backend, pad, SDL_GAMEPAD_BUTTON_LEFT_STICK);
        CHECK(got.commands == 2 && !got.pauses && !got.pressed);   /* the slot's down and up */
        CHECK(one_pause(press(&backend, pad, SDL_GAMEPAD_BUTTON_GUIDE)));
        menu_end(&backend);
    } else CHECK(0);
    /* An unknown name keeps the default. */
    if (menu_session(&backend, window, renderer, 1, "nosuchbutton", NULL)) {
        CHECK(one_pause(press(&backend, pad, SDL_GAMEPAD_BUTTON_LEFT_STICK)));
        menu_end(&backend);
    } else CHECK(0);
    /* The right trigger: down past half its travel, once; up below a quarter.  Eased a little, then pressed again
     * without being let go, it asks nothing more; the left one, and the click it replaced, are nobody's. */
    if (menu_session(&backend, window, renderer, 1, "righttrigger", NULL)) {
        CHECK(nothing(pull(&backend, pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 40)));
        CHECK(one_pause(pull(&backend, pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 60)));
        CHECK(nothing(pull(&backend, pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 100)));
        CHECK(nothing(pull(&backend, pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 35)));
        CHECK(nothing(pull(&backend, pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 100)));
        CHECK(nothing(pull(&backend, pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 10)));
        CHECK(one_pause(pull(&backend, pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 100)));
        /* Held through the pause, which took the press that closed it: the next press is a new one. */
        oracles_sdl_backend_hold(&backend, 1);
        oracles_sdl_backend_hold(&backend, 0);
        CHECK(nothing(pull(&backend, pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 90)));
        CHECK(nothing(pull(&backend, pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0)));
        CHECK(one_pause(pull(&backend, pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 100)));
        /* Let go during a pause, whose events the backend never reads: a press after it asks again. */
        oracles_sdl_backend_hold(&backend, 1);
        set_trigger(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0);
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        oracles_sdl_backend_hold(&backend, 0);
        CHECK(one_pause(pull(&backend, pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 100)));
        CHECK(nothing(pull(&backend, pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0)));
        CHECK(nothing(pull(&backend, pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 100)));
        CHECK(nothing(pull(&backend, pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 0)));
        CHECK(nothing(press(&backend, pad, SDL_GAMEPAD_BUTTON_LEFT_STICK)));
        CHECK(one_pause(press(&backend, pad, SDL_GAMEPAD_BUTTON_GUIDE)));
        menu_end(&backend);
    } else CHECK(0);
    SDL_DetachVirtualJoystick(pad);
}

static void suspended(void *opaque) { (*(int *)opaque)++; }

/* What asked for the pause is the device its hints start from: Escape the keyboard, the controller's Escape the
 * controller, the touch controls' pause a pointer; Android's Back and a suspension (the background) none, the hints
 * keeping the device used last, even after the controller asked for the pause before. */
static void menu_device(SDL_Window *window, SDL_Renderer *renderer)
{
    CHECK(oracles_home_menu_device(ORACLES_SDL_MENU_FROM_KEY) == ORACLES_INPUT_KEYBOARD);
    CHECK(oracles_home_menu_device(ORACLES_SDL_MENU_FROM_PAD) == ORACLES_INPUT_GAMEPAD);
    CHECK(oracles_home_menu_device(ORACLES_SDL_MENU_FROM_TOUCH) == ORACLES_INPUT_POINTER);
    CHECK(oracles_home_menu_device(ORACLES_SDL_MENU_FROM_NONE) == -1);
    CHECK(oracles_home_menu_device(ORACLES_SDL_MENU_FROM_BACK) == -1);   /* Android's Back: a finger's or a controller's */
    /* Opened by its key, Android's Back included; not by a suspension, which no device asked. */
    CHECK(oracles_home_menu_by_key(ORACLES_SDL_MENU_FROM_KEY) && oracles_home_menu_by_key(ORACLES_SDL_MENU_FROM_PAD)
          && oracles_home_menu_by_key(ORACLES_SDL_MENU_FROM_TOUCH) && oracles_home_menu_by_key(ORACLES_SDL_MENU_FROM_BACK));
    CHECK(!oracles_home_menu_by_key(ORACLES_SDL_MENU_FROM_NONE));
    const SDL_JoystickID pad = attach_gamepad();
    oracles_host_backend backend;
    if (pad && menu_session(&backend, window, renderer, 1, NULL, NULL)) {
        int stores = 0;
        backend.watch_suspend(backend.opaque, suspended, &stores);
        CHECK(one_pause(press(&backend, pad, SDL_GAMEPAD_BUTTON_LEFT_STICK)) && oracles_sdl_backend_menu_source(&backend) == ORACLES_SDL_MENU_FROM_PAD
              && oracles_home_menu_by_key((int)oracles_sdl_backend_menu_source(&backend)));
        SDL_Event e;
        SDL_zero(e);
        e.type = SDL_EVENT_WILL_ENTER_BACKGROUND;
        SDL_PushEvent(&e);
        oracles_host_event event;
        int has_event = 0;
        CHECK(backend.poll_event(backend.opaque, &event, &has_event) && has_event && event.type == ORACLES_HOST_EVENT_SUSPEND && stores == 1);
        CHECK(oracles_sdl_backend_menu_source(&backend) == ORACLES_SDL_MENU_FROM_NONE && !oracles_home_menu_by_key((int)oracles_sdl_backend_menu_source(&backend)));
        polled(&backend);
        backend.watch_suspend(backend.opaque, NULL, NULL);
        menu_end(&backend);
    } else CHECK(0);
    SDL_DetachVirtualJoystick(pad);
}

/* Two controllers' triggers are one menu input: one let go while the other is held lets nothing go, in play as on the
 * home screen and in the pause, so that the one still held does not ask again when it moves. */
static void two_triggers(SDL_Window *window, SDL_Renderer *renderer)
{
    const SDL_JoystickID one = attach_gamepad(), two = attach_gamepad();
    const SDL_GamepadAxis right = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
    oracles_host_backend backend;
    CHECK(one && two);
    if (menu_session(&backend, window, renderer, 1, "righttrigger", NULL)) {
        CHECK(one_pause(pull(&backend, one, right, 100)));
        CHECK(nothing(pull(&backend, two, right, 100)));
        CHECK(nothing(pull(&backend, two, right, 0)));
        CHECK(nothing(pull(&backend, one, right, 90)));
        CHECK(nothing(pull(&backend, one, right, 0)));
        CHECK(one_pause(pull(&backend, two, right, 100)));
        CHECK(nothing(pull(&backend, two, right, 0)));
        menu_end(&backend);
    } else CHECK(0);
    OraclesSdlPads pads = { { NULL }, 0 };
    oracles_sdl_pads_open(&pads);
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    snprintf(nav.controls.menu_pad, sizeof nav.controls.menu_pad, "righttrigger");
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    int held = oracles_home_trigger_held(&nav), backs[5] = { 0 };
    static const struct { int first, percent; } moves[5] = { { 1, 100 }, { 0, 100 }, { 0, 0 }, { 1, 90 }, { 1, 0 } };
    for (int m = 0; m < 5; m++) {
        set_trigger(moves[m].first ? one : two, right, moves[m].percent);
        SDL_Event e;
        OraclesHomeAction action;
        while (SDL_PollEvent(&e)) backs[m] += oracles_home_button_action(&nav, &e, &held, &action) && action == ORACLES_HOME_BACK;
    }
    CHECK(backs[0] == 1 && !backs[1] && !backs[2] && !backs[3] && !backs[4] && !held);
    oracles_sdl_pads_close(&pads);
    SDL_DetachVirtualJoystick(one);
    SDL_DetachVirtualJoystick(two);
}

/* A controller's button or trigger event of a gamepad that is not plugged in, as the home screen and the pause read it. */
static SDL_Event pad_event(Uint32 type, int input, Sint16 value)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = type;
    if (type == SDL_EVENT_GAMEPAD_AXIS_MOTION) { e.gaxis.axis = (Uint8)input; e.gaxis.value = value; }
    else { e.gbutton.button = (Uint8)input; e.gbutton.down = true; }
    return e;
}

/* On the home screen and in the pause, the controller's Escape is Back, as B and Escape are: Guide, the menu's button
 * unless A, B or the d-pad are it, or its trigger once past half its travel. */
static void menu_in_launcher(void)
{
    /* menu_pad= names a button or one of the two triggers; a stick's axis is neither. */
    int button = 0, axis = 0;
    CHECK(oracles_sdl_menu_input("leftstick", &button, &axis) && button == SDL_GAMEPAD_BUTTON_LEFT_STICK && axis == SDL_GAMEPAD_AXIS_INVALID);
    CHECK(oracles_sdl_menu_input("righttrigger", &button, &axis) && button == SDL_GAMEPAD_BUTTON_INVALID && axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    CHECK(oracles_sdl_menu_input("lefttrigger", &button, &axis) && axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    CHECK(oracles_sdl_menu_input("", &button, &axis) && button == SDL_GAMEPAD_BUTTON_INVALID && axis == SDL_GAMEPAD_AXIS_INVALID);
    CHECK(!oracles_sdl_menu_input("leftx", &button, &axis) && !oracles_sdl_menu_input("nosuchbutton", &button, &axis));

    OraclesHomeNav nav;
    oracles_home_init(&nav);
    OraclesHomeAction action = ORACLES_HOME_UP;
    int held = 0;
    SDL_Event e = pad_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_LEFT_STICK, 0);
    CHECK(!strcmp(nav.controls.menu_pad, "leftstick") && oracles_home_button_action(&nav, &e, &held, &action) && action == ORACLES_HOME_BACK);
    e = pad_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_GUIDE, 0);
    action = ORACLES_HOME_UP;
    CHECK(oracles_home_button_action(&nav, &e, &held, &action) && action == ORACLES_HOME_BACK);
    e = pad_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_RIGHT_STICK, 0);
    CHECK(!oracles_home_button_action(&nav, &e, &held, &action));
    snprintf(nav.controls.menu_pad, sizeof nav.controls.menu_pad, "a");
    e = pad_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_SOUTH, 0);
    CHECK(oracles_home_button_action(&nav, &e, &held, &action) && action == ORACLES_HOME_OK);
    snprintf(nav.controls.menu_pad, sizeof nav.controls.menu_pad, "lefttrigger");
    CHECK(oracles_home_trigger_held(&nav) == 0);
    e = pad_event(SDL_EVENT_GAMEPAD_AXIS_MOTION, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 30000);
    action = ORACLES_HOME_UP;
    CHECK(oracles_home_button_action(&nav, &e, &held, &action) && action == ORACLES_HOME_BACK && held);
    CHECK(!oracles_home_button_action(&nav, &e, &held, &action));
    e = pad_event(SDL_EVENT_GAMEPAD_AXIS_MOTION, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 0);
    CHECK(!oracles_home_button_action(&nav, &e, &held, &action) && !held);
    e = pad_event(SDL_EVENT_GAMEPAD_AXIS_MOTION, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 30000);
    CHECK(!oracles_home_button_action(&nav, &e, &held, &action));
    e = pad_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_LEFT_STICK, 0);
    CHECK(!oracles_home_button_action(&nav, &e, &held, &action));

    /* Controls' Menu cell waiting: a trigger past half its travel binds, a trigger barely pressed is swallowed, a
     * button the game has is not taken, and B gives up waiting while the game has it. */
    OraclesHomeCommand command;
    oracles_controls_open(&nav);
    snprintf(nav.controls.menu_pad, sizeof nav.controls.menu_pad, "leftstick");
    oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_MENU, -1);
    e = pad_event(SDL_EVENT_GAMEPAD_AXIS_MOTION, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 5000);
    CHECK(oracles_home_capture(&nav, &e, &command) && command == ORACLES_HOME_STAY && nav.controls.capturing);
    e = pad_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_SOUTH, 0);
    CHECK(oracles_home_capture(&nav, &e, &command) && command == ORACLES_HOME_STAY && nav.controls.capturing);
    e = pad_event(SDL_EVENT_GAMEPAD_AXIS_MOTION, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 30000);
    CHECK(oracles_home_capture(&nav, &e, &command) && command == ORACLES_HOME_STORE && !nav.controls.capturing);
    CHECK(!strcmp(nav.controls.menu_pad, "righttrigger"));
    oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_MENU, -1);
    e = pad_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_EAST, 0);
    CHECK(oracles_home_capture(&nav, &e, &command) && command == ORACLES_HOME_STAY && !nav.controls.capturing);
    CHECK(!strcmp(nav.controls.menu_pad, "righttrigger"));
    oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_MENU, -1);
    e = pad_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_RIGHT_STICK, 0);
    CHECK(oracles_home_capture(&nav, &e, &command) && command == ORACLES_HOME_STORE && !strcmp(nav.controls.menu_pad, "rightstick"));

    /* A slot's button waiting: the controller's Escape, the menu's button or Guide, gives up as the hint says ("R3
     * Cancel"), the slot's button kept; another button binds. */
    oracles_controls_click(&nav, 3, 0, -1);
    e = pad_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_RIGHT_STICK, 0);
    CHECK(oracles_home_capture(&nav, &e, &command) && command == ORACLES_HOME_STAY && !nav.controls.capturing);
    CHECK(!strcmp(nav.controls.hotkey_pads[0], "x") && !strcmp(nav.controls.menu_pad, "rightstick"));
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    oracles_controls_click(&nav, 3, 0, -1);
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    CHECK(oracles_home_hints(&nav, hints) == 1 && !strcmp(hints[0].key, "R3") && !strcmp(hints[0].label, "Cancel"));
    e = pad_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_GUIDE, 0);
    CHECK(oracles_home_capture(&nav, &e, &command) && command == ORACLES_HOME_STAY && !nav.controls.capturing);
    oracles_controls_click(&nav, 3, 0, -1);
    e = pad_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_LEFT_STICK, 0);
    CHECK(oracles_home_capture(&nav, &e, &command) && command == ORACLES_HOME_STORE && !strcmp(nav.controls.hotkey_pads[0], "leftstick"));
}

/* The device an event comes from, which the key hints follow: a key, a controller's button, its stick or trigger past
 * half its travel, a click or a touch; not a release, a slight move, the pointer's motion. */
static void input_devices(void)
{
    OraclesInputDevice device = ORACLES_INPUT_POINTER;
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = SDLK_F5;
    CHECK(oracles_home_input_device(&e, &device) && device == ORACLES_INPUT_KEYBOARD);
    e.type = SDL_EVENT_KEY_UP;
    CHECK(!oracles_home_input_device(&e, &device));
    e = pad_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_NORTH, 0);
    CHECK(oracles_home_input_device(&e, &device) && device == ORACLES_INPUT_GAMEPAD);
    device = ORACLES_INPUT_POINTER;
    e = pad_event(SDL_EVENT_GAMEPAD_AXIS_MOTION, SDL_GAMEPAD_AXIS_LEFTX, 8000);
    CHECK(!oracles_home_input_device(&e, &device));
    e = pad_event(SDL_EVENT_GAMEPAD_AXIS_MOTION, SDL_GAMEPAD_AXIS_LEFTY, -30000);
    CHECK(oracles_home_input_device(&e, &device) && device == ORACLES_INPUT_GAMEPAD);
    SDL_zero(e);
    e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    CHECK(oracles_home_input_device(&e, &device) && device == ORACLES_INPUT_POINTER);
    e.type = SDL_EVENT_MOUSE_MOTION;
    CHECK(!oracles_home_input_device(&e, &device));
    e.type = SDL_EVENT_FINGER_DOWN;
    device = ORACLES_INPUT_KEYBOARD;
    CHECK(oracles_home_input_device(&e, &device) && device == ORACLES_INPUT_POINTER);
}

/* The device used last on the home screen is the pause's until one is used there, and the home screen's again after
 * the game; a key pressed is remembered with it. */
static void device_kept(SDL_Renderer *renderer)
{
    OraclesUiDraw *draw = oracles_ui_draw_create(renderer);
    OraclesHomeNav home, back;
    oracles_home_init(&home);
    oracles_home_init(&back);
    oracles_home_input(&home, ORACLES_INPUT_KEYBOARD);
    oracles_home_input(&home, ORACLES_INPUT_GAMEPAD);
    CHECK(home.device == ORACLES_INPUT_GAMEPAD && home.keyboard_used && back.device != ORACLES_INPUT_GAMEPAD && !back.keyboard_used);
    OraclesPause *pause = draw ? oracles_pause_create(draw, NULL, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, 0, 0, &home) : NULL;
    CHECK(pause != NULL);
    if (pause) {
        oracles_pause_input(pause, &back);
        CHECK(back.device == ORACLES_INPUT_GAMEPAD && back.keyboard_used);
    }
    oracles_pause_destroy(pause);
    oracles_ui_draw_destroy(draw);
}

/* Each game started names the key that opens the menu on the device used last, until the player opened it by its key,
 * ten games at most: from the first frame that shows the status bar in play, counted once drawn. */
static void reminder(SDL_Renderer *renderer)
{
    char text[64];
    oracles_controls_menu_tip(ORACLES_INPUT_GAMEPAD, 0, "leftstick", text, sizeof text);
    CHECK(!strcmp(text, "L3 opens the menu"));
    oracles_controls_menu_tip(ORACLES_INPUT_GAMEPAD, 1, "leftstick", text, sizeof text);
    CHECK(!strcmp(text, "L3 or Back opens the menu"));
    oracles_controls_menu_tip(ORACLES_INPUT_GAMEPAD, 0, "righttrigger", text, sizeof text);
    CHECK(!strcmp(text, "RT opens the menu"));
    oracles_controls_menu_tip(ORACLES_INPUT_KEYBOARD, 0, "leftstick", text, sizeof text);
    CHECK(!strcmp(text, "Esc opens the menu"));
    oracles_controls_menu_tip(ORACLES_INPUT_KEYBOARD, 1, "leftstick", text, sizeof text);   /* a handheld's buttons as keys */
    CHECK(!strcmp(text, "Back opens the menu"));
    oracles_controls_menu_tip(ORACLES_INPUT_GAMEPAD, 0, "", text, sizeof text);   /* no button: Guide alone */
    CHECK(!strcmp(text, "Guide opens the menu"));
    oracles_controls_menu_tip(ORACLES_INPUT_GAMEPAD, 1, "", text, sizeof text);
    CHECK(!strcmp(text, "Back opens the menu"));
    oracles_controls_menu_tip(ORACLES_INPUT_POINTER, 1, "leftstick", text, sizeof text);
    CHECK(!strcmp(text, "The pause button opens the menu"));
    oracles_controls_menu_tip(ORACLES_INPUT_POINTER, 0, "leftstick", text, sizeof text);   /* a mouse: the keyboard's */
    CHECK(!strcmp(text, "Esc opens the menu"));
    /* Due at each game until shown ten times or dismissed. */
    int count = 0, games = 0;
    while (oracles_controls_menu_tip_due(count) && games < 20) { count = oracles_controls_tip_shown(count, ORACLES_MENU_TIPS); games++; }
    CHECK(games == ORACLES_MENU_TIPS && count == ORACLES_TIP_OFF);
    CHECK(!oracles_controls_menu_tip_due(ORACLES_TIP_OFF) && oracles_controls_tip_shown(ORACLES_TIP_OFF, ORACLES_MENU_TIPS) == ORACLES_TIP_OFF);
    CHECK(oracles_controls_tip_shown(0, ORACLES_HOTKEYS_TIPS) == 1 && oracles_controls_tip_shown(2, ORACLES_HOTKEYS_TIPS) == ORACLES_TIP_OFF);
    /* A text longer than its field loses its end on a character's boundary, never half a character. */
    oracles_ui_copy_whole(text, 8, "Menu \xc2\xb7 L3");
    CHECK(!strcmp(text, "Menu \xc2\xb7"));
    oracles_ui_copy_whole(text, 7, "Menu \xc2\xb7 L3");
    CHECK(!strcmp(text, "Menu "));
    oracles_controls_menu_tip(ORACLES_INPUT_GAMEPAD, 0, "\xc3\xa9\xc3\xa9", text, 4);
    CHECK(!strcmp(text, "\xc3\xa9"));
    OraclesUiDraw *draw = oracles_ui_draw_create(renderer);
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    OraclesPause *pause = draw ? oracles_pause_create(draw, NULL, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, 0, 0, &nav) : NULL;
    CHECK(pause != NULL);
    if (pause) {
        const OraclesPauseFrame title = { 0, ORACLES_INPUT_GAMEPAD, 480, 270, 0 }, play = { 1, ORACLES_INPUT_GAMEPAD, 480, 270, 0 };
        CHECK(!oracles_pause_overlay(pause, renderer, &play) && !oracles_pause_reminder_drawn(pause) && !oracles_pause_reminder_pending(pause));
        oracles_pause_reminder(pause, "leftstick");
        CHECK(oracles_pause_reminder_pending(pause) && !oracles_pause_reminder_drawn(pause));
        /* A title, a menu or a cutscene: it waits, drawing nothing. */
        CHECK(oracles_pause_overlay(pause, renderer, &title) && !oracles_pause_reminder_drawn(pause));
        CHECK(oracles_pause_overlay(pause, renderer, &play) && oracles_pause_reminder_drawn(pause));
        /* Shown, it stays its time whatever follows. */
        CHECK(oracles_pause_overlay(pause, renderer, &title) && oracles_pause_reminder_pending(pause));
    }
    oracles_pause_destroy(pause);
    oracles_ui_draw_destroy(draw);
}

int main(void)
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    SDL_Window *window = SDL_CreateWindow("home", 1280, 720, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, SDL_SOFTWARE_RENDERER) : NULL;
    if (!renderer) { fprintf(stderr, "window: %s\n", SDL_GetError()); return 1; }
    menu_inputs(window, renderer);
    two_triggers(window, renderer);
    menu_device(window, renderer);
    menu_in_launcher();
    input_devices();
    device_kept(renderer);
    reminder(renderer);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("oracles-launcher-menu: ok\n");
    return 0;
}
