/* The launcher's input, shared by the home screen and the pause menu: SDL's
 * keys and gamepad buttons as the screens' actions, and, while a cell of
 * Controls waits, as the key or the button it takes. */
#ifndef ORACLES_LAUNCHER_HOME_INPUT_H
#define ORACLES_LAUNCHER_HOME_INPUT_H

#include "ui_home_nav.h"

#include <SDL3/SDL.h>

/* Arrows, Enter or Space, Escape or Backspace; the d-pad, A, B (a button as oracles_sdl_pad_button names it), and
 * Back from the controller's Escape: Guide, and the menu's button or trigger (menu_pad=) when the d-pad, A or B are
 * not it.  A trigger goes down once past half its travel on any controller, *trigger_held following it over all of them
 * (oracles_sdl_trigger_moved). */
int oracles_home_key_action(SDL_Keycode key, OraclesHomeAction *action);
int oracles_home_button_action(const OraclesHomeNav *nav, const SDL_Event *event, int *trigger_held, OraclesHomeAction *action);
/* The device that asked for the pause menu (an oracles_sdl_menu_source) as the key hints name it, an
 * OraclesInputDevice; -1 when none did (Android's Back, a suspension): the hints keep the device used last. */
int oracles_home_menu_device(int source);
/* The player opened the pause menu by its key (an oracles_sdl_menu_source): Escape, the controller's menu button or
 * Guide, the touch controls' pause, Android's Back; not a suspension (Android's background), which no device asked. */
int oracles_home_menu_by_key(int source);
/* Where *trigger_held starts on a screen that opens, a game's pause or the home screen back from a game: the menu's
 * trigger held now (a press that opened the pause is not one that closes it). */
int oracles_home_trigger_held(const OraclesHomeNav *nav);

/* An event while a cell of Controls waits: 1 when it was the capture's (a key down, a button down), with what it
 * did in *command (STORE when the cell took it); 0 when the event is not for the capture.  The controller's Escape,
 * Guide or the menu's button, gives up waiting as Escape does, which the key hint says. */
int oracles_home_capture(OraclesHomeNav *nav, const SDL_Event *event, OraclesHomeCommand *command);

/* The device an event comes from, for the key hints (oracles_home_input): a key down, a controller's button down or
 * its stick or trigger past half its travel, a click or a touch.  0 for another event, and on Android for its Back,
 * which a finger or a controller gives as well as a keyboard. */
int oracles_home_input_device(const SDL_Event *event, OraclesInputDevice *device);


#endif
