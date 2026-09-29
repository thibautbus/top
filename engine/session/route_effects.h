/* A route's exchanges (format 2) as the effects the replay of the item hotkeys applies: the launcher
 * and the harness replay a route the same way. */
#ifndef ORACLES_LAUNCHER_ROUTE_EFFECTS_H
#define ORACLES_LAUNCHER_ROUTE_EFFECTS_H

#include "item_hotkeys.h"
#include "route.h"

/* The `equip` lines of the route, in its order, each expecting the items the route noted; the caller frees `*out`.
 * Returns their count, 0 with *out NULL when the route has none or memory is short. */
size_t oracles_route_effects(const OraclesRoute *route, OraclesItemHotkeysEffect **out);
/* The `equip` line of an exchange the guest applied, from the state before it. */
void oracles_route_action_of_exchange(const OraclesGuestInventoryState *before, const OraclesGuestInventoryOp *op, OraclesRouteAction *out);

#endif
