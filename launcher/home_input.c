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

int oracles_home_button_action(const SDL_Event *event, OraclesHomeAction *action)
{
    switch (oracles_sdl_pad_button(event)) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP: *action = ORACLES_HOME_UP; return 1;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: *action = ORACLES_HOME_DOWN; return 1;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: *action = ORACLES_HOME_LEFT; return 1;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: *action = ORACLES_HOME_RIGHT; return 1;
        case SDL_GAMEPAD_BUTTON_SOUTH: *action = ORACLES_HOME_OK; return 1;   /* "a" */
        case SDL_GAMEPAD_BUTTON_EAST: *action = ORACLES_HOME_BACK; return 1;   /* "b" */
        default: return 0;
    }
}

int oracles_home_capture(OraclesHomeNav *nav, const SDL_Event *event, OraclesHomeCommand *command)
{
    *command = ORACLES_HOME_STAY;
    if (nav->screen != ORACLES_SCREEN_CONTROLS || !nav->controls.capturing) return 0;
    if (event->type == SDL_EVENT_KEY_DOWN) {
        if (!event->key.repeat) *command = oracles_controls_capture_key(nav, SDL_GetKeyName(event->key.key));
        return 1;
    }
    if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        const SDL_GamepadButton button = (SDL_GamepadButton)oracles_sdl_pad_button(event);
        const int keyboard_cell = nav->controls.column == 0 || nav->controls.column == 2;
        /* A keyboard cell waits for a key; a controller's B gives up waiting, as Escape does. */
        if (keyboard_cell && button == SDL_GAMEPAD_BUTTON_EAST) oracles_controls_cancel(nav);
        else *command = oracles_controls_capture_button(nav, SDL_GetGamepadStringForButton(button));
        return 1;
    }
    /* What the capture does not take while it waits: the other keys' releases, the arrows' repeats. */
    return event->type == SDL_EVENT_KEY_UP || event->type == SDL_EVENT_GAMEPAD_BUTTON_UP;
}

void oracles_home_take_controller(SDL_Gamepad **controller)
{
    if (*controller && !SDL_GamepadConnected(*controller)) {
        SDL_CloseGamepad(*controller);
        *controller = NULL;
    }
    if (!*controller) *controller = oracles_sdl_open_gamepad();
}
