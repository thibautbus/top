/* The item hotkeys in a session of the launcher: the
 * live policy with the slots of settings.txt, the keys the backend reports,
 * what a recording notes, and the replay of a route that carries exchanges. */
#ifndef ORACLES_LAUNCHER_HOTKEYS_SESSION_H
#define ORACLES_LAUNCHER_HOTKEYS_SESSION_H

#include "route.h"
#include "settings.h"

#include <stdio.h>

typedef struct OraclesHotkeysSession OraclesHotkeysSession;

/* `route` is the route being replayed, or NULL: while it lasts its exchanges are applied and the keys do nothing.
 * NULL when the inventory's write point cannot be armed on this ROM (the option is then refused). */
OraclesHotkeysSession *oracles_hotkeys_session_start(OraclesGuest *guest, OraclesGame game, OraclesHotkeysMode mode,
                                                     oracles_settings *settings, const OraclesRoute *route);
void oracles_hotkeys_session_stop(OraclesHotkeysSession *session, FILE *report);
/* The route writer of a recording, or NULL when it stops. */
void oracles_hotkeys_session_record(OraclesHotkeysSession *session, OraclesRouteWriter *writer);
/* One of the backend's ORACLES_COMMAND_HOTKEY commands. */
void oracles_hotkeys_session_command(OraclesHotkeysSession *session, int command);
/* Once a frame: hands the Enhanced view what the hotbar shows; `view` may be NULL (Faithful has no hotbar). */
struct OraclesEnhancedView;
void oracles_hotkeys_session_frame(OraclesHotkeysSession *session, struct OraclesEnhancedView *view);
/* The host's input filter. */
unsigned oracles_hotkeys_session_filter(void *session, uint32_t frame, unsigned mask);
/* The replayed route is over, or a savestate was loaded during it: the player has the keys. */
void oracles_hotkeys_session_route_over(OraclesHotkeysSession *session);

#endif
