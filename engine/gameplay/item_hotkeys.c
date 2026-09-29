#include "item_hotkeys.h"

#include <stdlib.h>
#include <string.h>

/* ---- the two operations ------------------------------------------------------------------- */

static int slot_of(const OraclesGuestInventoryState *s, uint8_t item)
{
    for (unsigned i = 0; item && i < ORACLES_INVENTORY_SLOTS; i++) if (s->slots[i] == item) return (int)i;
    return -1;
}

static unsigned variant_kind(const OraclesGuestInventoryState *s, uint8_t item)
{
    if (item == s->item_seed_satchel) return ORACLES_VARIANT_SATCHEL;
    if (item == s->item_seed_shooter) return ORACLES_VARIANT_SHOOTER;
    if (item == s->item_harp && s->harp_song != 0xffu) return ORACLES_VARIANT_HARP;
    return ORACLES_VARIANT_NONE;
}

void oracles_item_hotkeys_ask_variant(const OraclesGuestInventoryState *s, uint8_t item, uint8_t variant, OraclesGuestInventoryOp *op)
{
    const unsigned kind = variant_kind(s, item);
    if (kind == ORACLES_VARIANT_NONE || variant == ORACLES_HOTKEY_NO_VARIANT) return;
    const uint8_t current = kind == ORACLES_VARIANT_SATCHEL ? s->satchel_seeds : kind == ORACLES_VARIANT_SHOOTER ? s->shooter_seeds : s->harp_song;
    if (variant == current) return;
    op->variant = (uint8_t)kind;
    op->variant_value = variant;
}

int oracles_item_hotkeys_equip(const OraclesGuestInventoryState *s, uint8_t item, uint8_t variant, unsigned target, OraclesGuestInventoryOp *op)
{
    memset(op, 0, sizeof *op);
    const int at = slot_of(s, item);
    if (at < 0 || target > ORACLES_INVENTORY_SLOT_A) return 0;
    if ((unsigned)at != target) { op->swap = 1; op->slot_a = (uint8_t)target; op->slot_b = (uint8_t)at; }   /* from the storage, or from the other button */
    oracles_item_hotkeys_ask_variant(s, item, variant, op);
    return 1;
}

int oracles_item_hotkeys_restore(const OraclesGuestInventoryState *s, unsigned button, uint8_t used, uint8_t original, uint8_t used_variant, OraclesGuestInventoryOp *op)
{
    memset(op, 0, sizeof *op);
    if (button > ORACLES_INVENTORY_SLOT_A || !used || s->slots[button] != used) return 0;   /* the game or the player changed the button: nothing to give back */
    int from = -1;
    if (original) from = slot_of(s, original);
    else for (unsigned i = 2; i < ORACLES_INVENTORY_SLOTS && from < 0; i++) if (!s->slots[i]) from = (int)i;   /* an empty button: the menu unequips into the first empty slot */
    if (from < 0) return 0;
    if ((unsigned)from == button) { oracles_item_hotkeys_ask_variant(s, used, used_variant, op); return 1; }   /* the use only changed the variant of the button's own item */
    op->swap = 1; op->slot_a = (uint8_t)button; op->slot_b = (uint8_t)from;
    oracles_item_hotkeys_ask_variant(s, used, used_variant, op);
    return 1;
}

/* ---- the replay of a route's exchanges (point 9) ------------------------------------------ */

struct OraclesItemHotkeysReplay {
    OraclesGuest *guest;
    OraclesItemHotkeysEffect *effects;
    size_t count, next;
    int stopped;
    OraclesItemHotkeysStats stats;
};

static void mismatch(OraclesItemHotkeysReplay *r, uint32_t frame, OraclesGuestInventoryVerdict verdict)
{
    if (!r->stats.replay_mismatch) { r->stats.first_mismatch_frame = frame; r->stats.first_mismatch_verdict = verdict; }
    r->stats.replay_mismatch++;
}

static void policy(void *opaque, const OraclesGuestInventoryState *state, OraclesGuestInventoryOp *op)
{
    OraclesItemHotkeysReplay *r = opaque;
    if (r->stopped) return;
    /* An exchange whose frame passed without a write point was never applied where the route says. */
    while (r->next < r->count && r->effects[r->next].frame < state->frame) { r->stats.missed_write_point++; mismatch(r, r->effects[r->next].frame, ORACLES_INVENTORY_NOTHING); r->next++; }
    if (r->next >= r->count || r->effects[r->next].frame != state->frame) return;
    *op = r->effects[r->next++].op;
    op->expect = 1;
}

static void result(void *opaque, const OraclesGuestInventoryState *before, const OraclesGuestInventoryOp *op, OraclesGuestInventoryVerdict verdict)
{
    OraclesItemHotkeysReplay *r = opaque;
    (void)op;
    if ((unsigned)verdict < ORACLES_INVENTORY_VERDICTS) r->stats.verdicts[verdict]++;
    if (verdict == ORACLES_INVENTORY_APPLIED) r->stats.applied++;
    else mismatch(r, before->frame, verdict);   /* the route noted an exchange applied at this frame */
}

static void reset(void *opaque)
{
    OraclesItemHotkeysReplay *r = opaque;
    r->stopped = 1;   /* a savestate load: the route has diverged, by construction */
}

OraclesItemHotkeysReplay *oracles_item_hotkeys_replay_start(OraclesGuest *guest, const OraclesItemHotkeysEffect *effects, size_t count)
{
    if (!guest || (count && !effects)) return NULL;
    OraclesItemHotkeysReplay *r = calloc(1, sizeof *r);
    if (!r) return NULL;
    r->guest = guest;
    r->count = count;
    r->effects = count ? malloc(count * sizeof *r->effects) : NULL;
    if (count && !r->effects) { free(r); return NULL; }
    if (count) memcpy(r->effects, effects, count * sizeof *r->effects);
    if (oracles_guest_set_inventory_policy(guest, policy, result, reset, r) != 0) { free(r->effects); free(r); return NULL; }
    return r;
}

void oracles_item_hotkeys_replay_stop_applying(OraclesItemHotkeysReplay *r) { if (r) r->stopped = 1; }

void oracles_item_hotkeys_replay_stats(const OraclesItemHotkeysReplay *r, OraclesItemHotkeysStats *out)
{
    if (!out) return;
    if (r) *out = r->stats; else memset(out, 0, sizeof *out);
    if (r && !r->stopped) out->missed_write_point += (unsigned)(r->count - r->next), out->replay_mismatch += (unsigned)(r->count - r->next);
}

void oracles_item_hotkeys_replay_stop(OraclesItemHotkeysReplay *r)
{
    if (!r) return;
    oracles_guest_set_inventory_policy(r->guest, NULL, NULL, NULL, NULL);
    free(r->effects);
    free(r);
}
