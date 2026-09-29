#include "route_effects.h"

#include <stdlib.h>
#include <string.h>

size_t oracles_route_effects(const OraclesRoute *route, OraclesItemHotkeysEffect **out)
{
    *out = NULL;
    size_t count = 0;
    for (size_t i = 0; i < route->action_count; i++) if (route->actions[i].verb == ORACLES_ROUTE_EQUIP) count++;
    if (!count) return 0;
    OraclesItemHotkeysEffect *effects = calloc(count, sizeof *effects);
    if (!effects) return 0;
    size_t n = 0;
    for (size_t i = 0; i < route->action_count; i++) {
        const OraclesRouteAction *a = &route->actions[i];
        if (a->verb != ORACLES_ROUTE_EQUIP) continue;
        OraclesItemHotkeysEffect *e = &effects[n++];
        e->frame = a->frame;
        e->op.swap = a->swap; e->op.slot_a = a->slot_a; e->op.slot_b = a->slot_b;
        e->op.expect = 1; e->op.expect_a = a->item_a; e->op.expect_b = a->item_b;
        e->op.variant = a->variant; e->op.variant_value = a->variant_value;
    }
    *out = effects;
    return count;
}

void oracles_route_action_of_exchange(const OraclesGuestInventoryState *before, const OraclesGuestInventoryOp *op, OraclesRouteAction *out)
{
    memset(out, 0, sizeof *out);
    out->frame = before->frame;
    out->verb = ORACLES_ROUTE_EQUIP;
    out->swap = op->swap;
    if (op->swap) { out->slot_a = op->slot_a; out->item_a = before->slots[op->slot_a]; out->slot_b = op->slot_b; out->item_b = before->slots[op->slot_b]; }
    out->variant = op->variant;
    out->variant_value = op->variant_value;
}
