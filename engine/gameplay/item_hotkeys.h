/* Item hotkeys: the gameplay policy of the inventory transaction.
 *
 * The mechanism: the two operations, computed on
 * a snapshot of the inventory (equip an item on a button; restore a button's
 * original item after a use), and the replay of the exchanges a route noted,
 * each applied at the write point of the frame it names after the guest has
 * checked that the two slots hold the items noted.  The live policy
 * (item_hotkeys_policy.c) adds four slots, the two modes, the `use` mode's
 * simulated press and the return of the button's own item. */
#ifndef ORACLES_GAMEPLAY_ITEM_HOTKEYS_H
#define ORACLES_GAMEPLAY_ITEM_HOTKEYS_H

#include "guest.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ORACLES_HOTKEY_NO_VARIANT 0xffu

/* Pure helpers, testable without a ROM. */
/* `equip` (item, variant or ORACLES_HOTKEY_NO_VARIANT, target slot B or A):
 * the exchange that puts `item` on `target` (from the storage as @equipItem,
 * or from the other button), and its variant when the item has one.  Returns
 * 1 with `op` filled (possibly an operation that changes nothing), 0 when
 * the item is in none of the eighteen slots. */
int oracles_item_hotkeys_equip(const OraclesGuestInventoryState *state, uint8_t item, uint8_t variant, unsigned target, OraclesGuestInventoryOp *op);
/* `restore` (button P, the used item X, the original item Y or 0, X's
 * variant before the use or ORACLES_HOTKEY_NO_VARIANT): gives P its item
 * back.  Returns 1 with `op` filled, 0 when the restore is void (P no longer
 * holds X, Y has left the slots, or no storage slot is empty for an empty Y). */
int oracles_item_hotkeys_restore(const OraclesGuestInventoryState *state, unsigned button, uint8_t used, uint8_t original, uint8_t used_variant, OraclesGuestInventoryOp *op);

/* Adds to `op` the writing of `item`'s variant when it has one, `variant` is given and differs from the current one. */
void oracles_item_hotkeys_ask_variant(const OraclesGuestInventoryState *state, uint8_t item, uint8_t variant, OraclesGuestInventoryOp *op);

/* One exchange a route noted, to apply at the write point of `frame`. */
typedef struct OraclesItemHotkeysEffect {
    uint32_t frame;
    OraclesGuestInventoryOp op;            /* expect set: the slots must hold what the route noted */
} OraclesItemHotkeysEffect;

typedef struct OraclesItemHotkeysStats {
    unsigned applied;                      /* exchanges applied */
    unsigned replay_mismatch;              /* noted exchanges the guest did not apply at their frame, whatever the reason */
    unsigned missed_write_point;           /* noted exchanges whose frame had no write point */
    unsigned verdicts[ORACLES_INVENTORY_VERDICTS];
    uint32_t first_mismatch_frame;
    OraclesGuestInventoryVerdict first_mismatch_verdict;
} OraclesItemHotkeysStats;

typedef struct OraclesItemHotkeysReplay OraclesItemHotkeysReplay;
/* Installs the replay on the guest; `effects` (sorted by frame, at most one a
 * frame) is copied.  NULL when the write point cannot be armed on this ROM. */
OraclesItemHotkeysReplay *oracles_item_hotkeys_replay_start(OraclesGuest *guest, const OraclesItemHotkeysEffect *effects, size_t count);
/* After a savestate load the route has diverged: nothing more is applied. */
void oracles_item_hotkeys_replay_stop_applying(OraclesItemHotkeysReplay *replay);
void oracles_item_hotkeys_replay_stats(const OraclesItemHotkeysReplay *replay, OraclesItemHotkeysStats *out);
void oracles_item_hotkeys_replay_stop(OraclesItemHotkeysReplay *replay);

/* ---- the live policy ----------------------------------------------------------------------- */

#define ORACLES_HOTKEY_SLOTS 4u
#define ORACLES_HOTKEY_PATIENCE 30u        /* write points a postponed request waits, and the quiet frames before a return */

typedef enum OraclesHotkeysMode { ORACLES_HOTKEYS_OFF = 0, ORACLES_HOTKEYS_USE, ORACLES_HOTKEYS_EQUIP } OraclesHotkeysMode;
typedef enum OraclesHotkeyModifier { ORACLES_HOTKEY_PLAIN = 0, ORACLES_HOTKEY_BIND_B, ORACLES_HOTKEY_BIND_A } OraclesHotkeyModifier;
/* What a key did, for the launcher to say and to persist. */
typedef enum OraclesHotkeyOutcome { ORACLES_HOTKEY_IGNORED = 0, ORACLES_HOTKEY_TRIGGERED, ORACLES_HOTKEY_ASSIGNED, ORACLES_HOTKEY_EMPTIED } OraclesHotkeyOutcome;

typedef struct OraclesHotkeySlot {
    uint8_t set;
    uint8_t item;
    uint8_t variant;                       /* ORACLES_HOTKEY_NO_VARIANT when the item has none */
    uint8_t target;                        /* ORACLES_INVENTORY_SLOT_B or _A: the `equip` mode's button */
} OraclesHotkeySlot;

/* What a session records of the policy (the route's `equip` and `use` lines). */
typedef struct OraclesItemHotkeysSink {
    void (*applied)(void *opaque, const OraclesGuestInventoryState *before, const OraclesGuestInventoryOp *op);
    void (*use_started)(void *opaque, uint32_t frame, unsigned button_slot, uint8_t item);
    void *opaque;
} OraclesItemHotkeysSink;

typedef struct OraclesItemHotkeysLiveStats {
    unsigned requests, applied, uses, returns, returns_abandoned, dropped_postponed, dropped_refused;
    unsigned returns_pending;          /* still waiting when the figures are read */
    unsigned return_longest_delay;     /* write points the latest return came after its quiet spell was over */
    unsigned dropped_by[ORACLES_INVENTORY_VERDICTS];   /* the condition that refused a request, or the last one that postponed it */
    OraclesGuestInventoryVerdict last_verdict;
} OraclesItemHotkeysLiveStats;

typedef struct OraclesItemHotkeys OraclesItemHotkeys;
/* `guest` may be NULL (tests drive the entries below by hand).  With a guest, installs the policy and listens to its
 * events; NULL when the write point cannot be armed on this ROM. */
OraclesItemHotkeys *oracles_item_hotkeys_start(OraclesGuest *guest, OraclesHotkeysMode mode);
void oracles_item_hotkeys_stop(OraclesItemHotkeys *hotkeys);
void oracles_item_hotkeys_set_sink(OraclesItemHotkeys *hotkeys, const OraclesItemHotkeysSink *sink);
void oracles_item_hotkeys_set_slot(OraclesItemHotkeys *hotkeys, unsigned n, const OraclesHotkeySlot *slot);
void oracles_item_hotkeys_slot(const OraclesItemHotkeys *hotkeys, unsigned n, OraclesHotkeySlot *out);
/* A slot's key went down or up.  Inventory open on its items and waiting: the key assigns the item under the cursor
 * (target A with BIND_A, B otherwise).  In play with a modifier: it assigns the item of that button.  Otherwise it
 * triggers the slot.  Needs the guest for the two assignments. */
OraclesHotkeyOutcome oracles_item_hotkeys_key(OraclesItemHotkeys *hotkeys, unsigned n, int pressed, OraclesHotkeyModifier modifier);
/* The joypad mask handed to the core for this frame: the player's, with the simulated press of the `use` mode. */
unsigned oracles_item_hotkeys_filter_keys(OraclesItemHotkeys *hotkeys, uint32_t frame, unsigned mask);
/* A savestate was loaded: the pending request, the simulated press, the returns and the memory of the previous item go. */
void oracles_item_hotkeys_reset(OraclesItemHotkeys *hotkeys);
/* Nothing pending and no simulated press held: what a reset must leave. */
int oracles_item_hotkeys_idle(const OraclesItemHotkeys *hotkeys);
void oracles_item_hotkeys_live_stats(const OraclesItemHotkeys *hotkeys, OraclesItemHotkeysLiveStats *out);
/* For the hotbar: whether slot `n`'s request is waiting, and how many of its requests were refused or given up so far. */
void oracles_item_hotkeys_slot_status(const OraclesItemHotkeys *hotkeys, unsigned n, int *pending, unsigned *dropped);
/* The policy's entries, which the guest calls; public for the ROM-free tests. */
void oracles_item_hotkeys_on_write_point(OraclesItemHotkeys *hotkeys, const OraclesGuestInventoryState *state, OraclesGuestInventoryOp *op);
void oracles_item_hotkeys_on_result(OraclesItemHotkeys *hotkeys, const OraclesGuestInventoryState *before, const OraclesGuestInventoryOp *op, OraclesGuestInventoryVerdict verdict);
void oracles_item_hotkeys_on_event(OraclesItemHotkeys *hotkeys, const OraclesGuestEvent *event, uint8_t menu_inventory);

#ifdef __cplusplus
}
#endif

#endif
