/* Item hotkeys in the harness: the exchanges a route
 * noted are applied at their frames by the replay policy, and the figures
 * the check asks for are counted: exchanges applied, replay
 * mismatches, violations of the inventory's consistency, and status bar
 * refreshes missed (loadEquippedItemGfx not entered at the turn after an
 * exchange).  With a directory, a table of the inventory frame by frame,
 * from which working routes of the item hotkeys are built. */
#ifndef ORACLES_HARNESS_HOTKEYS_CHECK_H
#define ORACLES_HARNESS_HOTKEYS_CHECK_H

#include "guest.h"
#include "route.h"

#include <stdio.h>

typedef struct OraclesHotkeysCheck OraclesHotkeysCheck;

/* `dir` may be NULL (no table).  NULL when the write point cannot be armed on this ROM. */
OraclesHotkeysCheck *oracles_hotkeys_check_start(OraclesGuest *guest, const OraclesRoute *route, const char *dir);
void oracles_hotkeys_check_event(OraclesHotkeysCheck *check, const OraclesGuestEvent *event);
void oracles_hotkeys_check_frame_end(OraclesHotkeysCheck *check, uint32_t frame, unsigned keys);
void oracles_hotkeys_check_summary(OraclesHotkeysCheck *check, FILE *out);
void oracles_hotkeys_check_report(OraclesHotkeysCheck *check, FILE *out);
/* An exchange not applied as the route noted it, one whose write point never came, an inconsistent inventory or a status bar left stale: the replay failed. */
int oracles_hotkeys_check_failed(OraclesHotkeysCheck *check);
void oracles_hotkeys_check_stop(OraclesHotkeysCheck *check);

#endif
