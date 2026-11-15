/* RetroAchievements' login on the home screen's window (ui_achievements.h), in a build with ORACLES_RETROACHIEVEMENTS: the
 * keys, the controller's buttons and the text typed while it is open, and the system's text input (Android's keyboard)
 * while a field takes typing. */
#ifndef ORACLES_LAUNCHER_HOME_ACHIEVEMENTS_H
#define ORACLES_LAUNCHER_HOME_ACHIEVEMENTS_H

#include "ui_achievements.h"

#include <SDL3/SDL.h>

/* The login's input: 1 when it took `event`, *command what it asks of the host.  The clipboard's text pasted with
 * Ctrl+V (Cmd+V on macOS), Shift+Insert or a Paste key, a system keyboard's text of several characters by the same rules,
 * the Enter or Tab Android makes up for that text's line breaks dropped; the password hidden again whenever the
 * application goes to the background or its window loses the focus (those events left to the host). */
int oracles_home_achievements_event(OraclesHomeNav *nav, const SDL_Event *event, OraclesHomeCommand *command);
/* The clipboard's text into the field highlighted (a chord, the launcher's keyboard's Paste key). */
void oracles_home_achievements_paste(OraclesHomeNav *nav);
/* The system's text input on while a field takes typing and the launcher's keyboard is not up (Android's keyboard comes
 * up with it), of the field's kind, off otherwise (a key's character then typed as it is) and once the system put it away
 * (Android's Back), until a field is chosen again; nav->achievements.system_keyboard follows Android's keyboard. */
void oracles_home_achievements_text_input(SDL_Window *window, OraclesHomeNav *nav);

#endif
