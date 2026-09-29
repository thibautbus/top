/* The launcher's input, shared by the home screen and the pause menu: SDL's
 * keys and gamepad buttons as the screens' actions, and, while a cell of
 * Controls waits, as the key or the button it takes. */
#ifndef ORACLES_LAUNCHER_HOME_INPUT_H
#define ORACLES_LAUNCHER_HOME_INPUT_H

#include "ui_home_nav.h"

#include <SDL3/SDL.h>

/* Arrows, Enter or Space, Escape or Backspace; the d-pad, A, B (a button as oracles_sdl_pad_button names it). */
int oracles_home_key_action(SDL_Keycode key, OraclesHomeAction *action);
int oracles_home_button_action(const SDL_Event *event, OraclesHomeAction *action);

/* An event while a cell of Controls waits: 1 when it was the capture's (a key down, a button down), with what it
 * did in *command (STORE when the cell took it); 0 when the event is not for the capture. */
int oracles_home_capture(OraclesHomeNav *nav, const SDL_Event *event, OraclesHomeCommand *command);

/* The gamepad the home screen reads: a handle whose device went away is closed (a session consumed its removal),
 * and without one the first gamepad plugged in is opened, one plugged in during the session included. */
void oracles_home_take_controller(SDL_Gamepad **controller);

#endif
