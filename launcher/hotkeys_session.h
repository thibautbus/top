/* The item hotkeys in a session of the launcher: the
 * live policy with the slots of settings.txt, the keys the backend reports,
 * what a recording notes, and the replay of a route that carries exchanges. */
#ifndef ORACLES_LAUNCHER_HOTKEYS_SESSION_H
#define ORACLES_LAUNCHER_HOTKEYS_SESSION_H

#include "route.h"
#include "settings.h"

#include <stdio.h>

typedef struct OraclesHotkeysSession OraclesHotkeysSession;

/* `slots`: the settings' slots the game plays with (oracles_session_hotkey_slots).  `route` is the route being replayed,
 * or NULL: while it lasts its exchanges are applied and the keys do nothing.  NULL when the inventory's write point
 * cannot be armed on this ROM (the option is then refused). */
OraclesHotkeysSession *oracles_hotkeys_session_start(OraclesGuest *guest, unsigned slots, OraclesHotkeysMode mode,
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
/* The mode changed while the game runs (the pause's Item hotkeys): asked for here, the slots' keys let go when the
 * live policy is to stop; taken by oracles_hotkeys_session_settle.  During a replay, the mode the player gets after it. */
void oracles_hotkeys_session_set_mode(OraclesHotkeysSession *session, OraclesHotkeysMode mode);
/* At the start of a frame, before it runs: the mode asked for taken once the live policy is at rest
 * (oracles_item_hotkeys_at_rest), so that no button keeps a hotkey's item; at once when there is none.  1 when the mode
 * changed now, 0 otherwise, -1 when the policy could not start again (the hotkeys then stay off). */
int oracles_hotkeys_session_settle(OraclesHotkeysSession *session, FILE *report);
/* The mode the game plays with, and the one asked for. */
OraclesHotkeysMode oracles_hotkeys_session_mode(const OraclesHotkeysSession *session);
OraclesHotkeysMode oracles_hotkeys_session_next_mode(const OraclesHotkeysSession *session);

#endif
