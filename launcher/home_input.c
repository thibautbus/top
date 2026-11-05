#include "home_input.h"

#include "backends.h"
#include "ui_controls_nav.h"

int oracles_home_key_action(SDL_Keycode key, OraclesHomeAction *action)
{
    switch (key) {
        case SDLK_UP: *action = ORACLES_HOME_UP; return 1;
        case SDLK_DOWN: *action = ORACLES_HOME_DOWN; return 1;
        case SDLK_LEFT: *action = ORACLES_HOME_LEFT; return 1;
        case SDLK_RIGHT: *action = ORACLES_HOME_RIGHT; return 1;
        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: *action = ORACLES_HOME_OK; return 1;
        case SDLK_ESCAPE: case SDLK_BACKSPACE: *action = ORACLES_HOME_BACK; return 1;
#ifdef __ANDROID__
        case SDLK_AC_BACK: *action = ORACLES_HOME_BACK; return 1;   /* Android's Back; elsewhere a key like any other */
#endif
        default: return 0;
    }
}

int oracles_home_button_action(const OraclesHomeNav *nav, const SDL_Event *event, int *trigger_held, OraclesHomeAction *action)
{
    int menu_button, menu_trigger;
    oracles_sdl_menu_input(nav->controls.menu_pad, &menu_button, &menu_trigger);
    if (event->type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        if (!oracles_sdl_trigger_moved(event, menu_trigger, trigger_held)) return 0;
        *action = ORACLES_HOME_BACK;
        return 1;
    }
    if (event->type != SDL_EVENT_GAMEPAD_BUTTON_DOWN) return 0;
    const int button = oracles_sdl_pad_button(event);
    switch (button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP: *action = ORACLES_HOME_UP; return 1;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: *action = ORACLES_HOME_DOWN; return 1;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: *action = ORACLES_HOME_LEFT; return 1;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: *action = ORACLES_HOME_RIGHT; return 1;
        case SDL_GAMEPAD_BUTTON_SOUTH: *action = ORACLES_HOME_OK; return 1;   /* "a" */
        case SDL_GAMEPAD_BUTTON_EAST: *action = ORACLES_HOME_BACK; return 1;   /* "b" */
        case SDL_GAMEPAD_BUTTON_GUIDE: *action = ORACLES_HOME_BACK; return 1;   /* the menu's in a game, as Escape */
        default:
            if (button != menu_button) return 0;
            *action = ORACLES_HOME_BACK;
            return 1;
    }
}

int oracles_home_menu_device(int source)
{
    switch (source) {
        case ORACLES_SDL_MENU_FROM_KEY: return ORACLES_INPUT_KEYBOARD;
        case ORACLES_SDL_MENU_FROM_PAD: return ORACLES_INPUT_GAMEPAD;
        case ORACLES_SDL_MENU_FROM_TOUCH: return ORACLES_INPUT_POINTER;
        default: return -1;
    }
}

int oracles_home_menu_by_key(int source) { return source != ORACLES_SDL_MENU_FROM_NONE; }

int oracles_home_trigger_held(const OraclesHomeNav *nav)
{
    int menu_button, menu_trigger;
    oracles_sdl_menu_input(nav->controls.menu_pad, &menu_button, &menu_trigger);
    return oracles_sdl_trigger_held(menu_trigger);
}

int oracles_home_capture(OraclesHomeNav *nav, const SDL_Event *event, OraclesHomeCommand *command)
{
    *command = ORACLES_HOME_STAY;
    if (nav->screen != ORACLES_SCREEN_CONTROLS || !nav->controls.capturing) return 0;
    if (event->type == SDL_EVENT_KEY_DOWN) {
        if (!event->key.repeat) *command = oracles_controls_capture_key(nav, SDL_GetKeyName(event->key.key));
        return 1;
    }
    const int menu = nav->controls.row == ORACLES_CONTROLS_ROW_MENU;
    if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        const SDL_GamepadButton button = (SDL_GamepadButton)oracles_sdl_pad_button(event);
        int menu_button, menu_trigger;
        oracles_sdl_menu_input(nav->controls.menu_pad, &menu_button, &menu_trigger);
        if (button == SDL_GAMEPAD_BUTTON_GUIDE || (int)button == menu_button) { oracles_controls_cancel(nav); return 1; }
        const int keyboard_cell = !menu && (nav->controls.column == 0 || nav->controls.column == 2);
        /* A keyboard cell waits for a key; a controller's B gives up waiting, as Escape does, and so in Menu's cell
         * while the game has B. */
        if ((keyboard_cell || (menu && !oracles_controls_menu_free(&nav->controls, "b"))) && button == SDL_GAMEPAD_BUTTON_EAST)
            oracles_controls_cancel(nav);
        else *command = oracles_controls_capture_button(nav, SDL_GetGamepadStringForButton(button));
        return 1;
    }
    /* Menu's cell takes a trigger pressed past half its travel too. */
    if (event->type == SDL_EVENT_GAMEPAD_AXIS_MOTION && menu
        && (event->gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || event->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) {
        int held = 0;
        if (oracles_sdl_trigger_pressed(event->gaxis.value, &held))
            *command = oracles_controls_capture_button(nav, SDL_GetGamepadStringForAxis((SDL_GamepadAxis)event->gaxis.axis));
        return 1;
    }
    /* What the capture does not take while it waits: the other keys' releases, the arrows' repeats. */
    return event->type == SDL_EVENT_KEY_UP || event->type == SDL_EVENT_GAMEPAD_BUTTON_UP;
}

int oracles_home_input_device(const SDL_Event *event, OraclesInputDevice *device)
{
    const int half = SDL_JOYSTICK_AXIS_MAX / 2;
    switch (event->type) {
        case SDL_EVENT_KEY_DOWN:
#ifdef __ANDROID__
            if (event->key.key == SDLK_AC_BACK) return 0;
#endif
            *device = ORACLES_INPUT_KEYBOARD;
            return 1;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN: *device = ORACLES_INPUT_GAMEPAD; return 1;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (event->gaxis.value <= half && event->gaxis.value >= -half) return 0;
            *device = ORACLES_INPUT_GAMEPAD;
            return 1;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_FINGER_DOWN: *device = ORACLES_INPUT_POINTER; return 1;
        default: return 0;
    }
}
