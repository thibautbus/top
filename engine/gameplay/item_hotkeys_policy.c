/* Item hotkeys, the live policy: four slots, the two modes, the
 * `use` mode's simulated press and the return of the button's own item.
 *
 * Nothing is decided at the key press: a request waits for the write point,
 * where the guest hands the state the game is in.  The `use` mode then picks
 * the carrier button among those the game reads in that state, exchanges if
 * it must, and from the next reading of the joypad holds that button for
 * the player; when the key is released and the game has read the press, the
 * button's own item comes back after a quiet spell. */
#include "item_hotkeys.h"

#include "core.h"

#include <stdlib.h>
#include <string.h>

enum { ASKED_NOTHING = 0, ASKED_USE, ASKED_EQUIP, ASKED_RETURN };

struct OraclesItemHotkeys {
    OraclesGuest *guest;
    OraclesHotkeysMode mode;
    OraclesHotkeySlot slots[ORACLES_HOTKEY_SLOTS];
    OraclesItemHotkeysSink sink;
    int key_down[ORACLES_HOTKEY_SLOTS];
    int turned_down[ORACLES_HOTKEY_SLOTS];                       /* its request was dropped since the key went down: no heir until pressed again */
    unsigned key_order[ORACLES_HOTKEY_SLOTS], presses;          /* which held key was pressed last */
    unsigned slot_dropped[ORACLES_HOTKEY_SLOTS];                 /* requests of each slot refused or given up: the hotbar's red frame */
    struct { int active; unsigned slot, age; } request;          /* one at a time, a new one replacing the old */
    int asked;                                                   /* what the operation handed at this write point is for */
    unsigned asked_button;
    struct {
        int active, released, in_mask, polled, used, announced;
        unsigned slot, button, gap, polls_released;
        uint8_t item;
    } press;                                                     /* the simulated press of the `use` mode */
    struct { int waiting; uint8_t used, original, variant; unsigned quiet, overdue; } returns[2];   /* by button slot; overdue: write points past its quiet spell */
    struct { int valid; uint8_t item; } previous[2];             /* `equip`: what the target held before the last exchange */
    unsigned player_mask, sent_mask;                             /* the player's keys, and what went to the core, at the last frame */
    OraclesItemHotkeysLiveStats stats;
};

static unsigned button_bit(unsigned button_slot) { return button_slot == ORACLES_INVENTORY_SLOT_B ? ORACLES_BUTTON_B : ORACLES_BUTTON_A; }
static unsigned key_bit(unsigned button_slot) { return button_slot == ORACLES_INVENTORY_SLOT_B ? ORACLES_KEY_B : ORACLES_KEY_A; }

static void ask_for(OraclesItemHotkeys *h, unsigned n);

/* One key at a time, the last one pressed.  When its press is over, or its request goes unanswered, the key still
 * held that was pressed last has the button again (the shield's key held while the feather's was tapped).  `but` is
 * the slot that just had its turn. */
static void give_the_button_back(OraclesItemHotkeys *h, unsigned but)
{
    if (h->mode != ORACLES_HOTKEYS_USE || h->request.active) return;
    unsigned heir = ORACLES_HOTKEY_SLOTS;
    for (unsigned k = 0; k < ORACLES_HOTKEY_SLOTS; k++)
        if (k != but && h->key_down[k] && !h->turned_down[k] && h->slots[k].set && (heir == ORACLES_HOTKEY_SLOTS || h->key_order[k] > h->key_order[heir])) heir = k;
    if (heir != ORACLES_HOTKEY_SLOTS) ask_for(h, heir);
}

static void drop_request(OraclesItemHotkeys *h, int refused, OraclesGuestInventoryVerdict why)
{
    const unsigned slot = h->request.slot;
    h->request.active = 0;
    h->slot_dropped[slot]++;
    h->turned_down[slot] = 1;   /* two keys held through a text would otherwise hand the request to each other at every write point */
    if (refused) h->stats.dropped_refused++; else h->stats.dropped_postponed++;
    if ((unsigned)why < ORACLES_INVENTORY_VERDICTS) h->stats.dropped_by[why]++;
    give_the_button_back(h, slot);   /* a key held before this one must not lose its button to a request that came to nothing */
}

/* A passing condition: the request waits, thirty write points at most once its key is up; a key still held goes on
 * asking, as a held button would (the shield's key held through a swim comes up with Link). */
static void postpone_request(OraclesItemHotkeys *h, OraclesGuestInventoryVerdict why)
{
    if (h->key_down[h->request.slot]) return;
    if (++h->request.age > ORACLES_HOTKEY_PATIENCE) drop_request(h, 0, why);
}

static int item_in_use_on(const OraclesGuestInventoryState *s, unsigned button_slot)
{
    for (unsigned i = 0; i < 5u; i++) {
        if (!s->parents[i].enabled) continue;
        if (s->parents[i].button & button_bit(button_slot)) return 1;
        if (s->slots[button_slot] && s->parents[i].id == s->slots[button_slot]) return 1;
    }
    return 0;
}

/* Whether an item in use stands in the way of giving a button its item back: one that answers to that button, or one
 * of the two items the return exchanges.  An item in use on the other button does not (the shield held there), as it
 * does not postpone an exchange either. */
static int return_held_back(const OraclesGuestInventoryState *s, unsigned button_slot, uint8_t used, uint8_t original)
{
    for (unsigned i = 0; i < 5u; i++) {
        if (!s->parents[i].enabled) continue;
        if (s->parents[i].button & button_bit(button_slot)) return 1;
        if (s->parents[i].id == used || (original && s->parents[i].id == original)) return 1;
    }
    return 0;
}

static int slot_holds(const OraclesGuestInventoryState *s, unsigned button_slot, const OraclesHotkeySlot *slot)
{
    if (s->slots[button_slot] != slot->item) return 0;
    return slot->variant == ORACLES_HOTKEY_NO_VARIANT || oracles_guest_item_variant(s, slot->item) == slot->variant;
}

static void ask_for(OraclesItemHotkeys *h, unsigned n)
{
    h->request.active = 1;
    h->request.slot = n;
    h->request.age = 0;
}

static void begin_press(OraclesItemHotkeys *h, unsigned slot, unsigned button_slot, uint8_t item)
{
    memset(&h->press, 0, sizeof h->press);
    h->press.active = 1;
    h->press.slot = slot;
    h->press.button = button_slot;
    h->press.item = item;
    h->press.released = !h->key_down[slot];
    /* The button already down, by the player's hand or by the press of another slot: the game must see it come up
     * before it sees this press (wGameKeysJustPressed). */
    h->press.gap = ((h->player_mask | h->sent_mask) & key_bit(button_slot)) ? 1u : 0u;
    h->returns[button_slot].quiet = 0;
    h->request.active = 0;
}

/* `use`: the carrier button is chosen here, in the state the game is in, not at the key press. */
static void ask_use(OraclesItemHotkeys *h, const OraclesGuestInventoryState *s, OraclesGuestInventoryOp *op)
{
    const OraclesHotkeySlot *slot = &h->slots[h->request.slot];
    const OraclesGuestInventoryVerdict refusal = slot->set ? oracles_guest_inventory_refusal(s) : ORACLES_INVENTORY_REFUSED_OPERATION;
    if (refusal != ORACLES_INVENTORY_APPLIED) { drop_request(h, 1, refusal); return; }
    const unsigned read = oracles_guest_item_buttons(s);
    if (!read) { postpone_request(h, ORACLES_INVENTORY_POSTPONED_NO_BUTTON); return; }
    static const unsigned order[2] = { ORACLES_INVENTORY_SLOT_B, ORACLES_INVENTORY_SLOT_A };
    for (unsigned i = 0; i < 2u; i++)
        if ((read & button_bit(order[i])) && slot_holds(s, order[i], slot)) { begin_press(h, h->request.slot, order[i], slot->item); return; }
    unsigned carrier = ORACLES_INVENTORY_SLOTS;
    for (unsigned i = 0; i < 2u && carrier == ORACLES_INVENTORY_SLOTS; i++)
        if ((read & button_bit(order[i])) && !item_in_use_on(s, order[i])) carrier = order[i];
    if (carrier == ORACLES_INVENTORY_SLOTS) { postpone_request(h, ORACLES_INVENTORY_POSTPONED_ITEM_IN_USE); return; }
    if (!oracles_item_hotkeys_equip(s, slot->item, slot->variant, carrier, op)) { drop_request(h, 1, ORACLES_INVENTORY_REFUSED_OPERATION); return; }
    /* A chain of uses: the item this one takes the button from gets back the variant it had before its own use, when
     * this exchange writes none of its own (an operation carries one). */
    if (h->returns[carrier].waiting && h->returns[carrier].used != slot->item && op->variant == ORACLES_VARIANT_NONE)
        oracles_item_hotkeys_ask_variant(s, h->returns[carrier].used, h->returns[carrier].variant, op);
    h->asked = ASKED_USE;
    h->asked_button = carrier;
}

static void ask_equip(OraclesItemHotkeys *h, const OraclesGuestInventoryState *s, OraclesGuestInventoryOp *op)
{
    const OraclesHotkeySlot *slot = &h->slots[h->request.slot];
    if (!slot->set) { drop_request(h, 1, ORACLES_INVENTORY_REFUSED_OPERATION); return; }
    const unsigned target = slot->target == ORACLES_INVENTORY_SLOT_A ? ORACLES_INVENTORY_SLOT_A : ORACLES_INVENTORY_SLOT_B;
    uint8_t item = slot->item, variant = slot->variant;
    if (slot_holds(s, target, slot)) {
        /* Already there: the key brings back what the target held before the last exchange, if the host remembers one. */
        if (!h->previous[target].valid) { h->request.active = 0; return; }
        item = h->previous[target].item;
        variant = ORACLES_HOTKEY_NO_VARIANT;
    }
    if (!item || !oracles_item_hotkeys_equip(s, item, variant, target, op)) { drop_request(h, 1, ORACLES_INVENTORY_REFUSED_OPERATION); return; }
    h->asked = ASKED_EQUIP;
    h->asked_button = target;
}

static void ask_return(OraclesItemHotkeys *h, const OraclesGuestInventoryState *s, OraclesGuestInventoryOp *op)
{
    for (unsigned b = 0; b < 2u; b++) {
        if (!h->returns[b].waiting || h->returns[b].quiet < ORACLES_HOTKEY_PATIENCE) continue;
        if (h->press.active && h->press.button == b) continue;
        /* The item to give back sits on the other button, which has a return of its own waiting: that one first. It
         * puts the item back in the storage, and this one is then a plain exchange; the other way round the two
         * buttons are exchanged and the other's own item is left in the storage for good. */
        if (h->returns[1u - b].waiting && h->returns[b].original && s->slots[1u - b] == h->returns[b].original) continue;
        memset(op, 0, sizeof *op);
        if (!oracles_item_hotkeys_restore(s, b, h->returns[b].used, h->returns[b].original, h->returns[b].variant, op)) {
            h->returns[b].waiting = 0;   /* the game, or the player through the menu, has changed the button since */
            h->stats.returns_abandoned++;
            continue;
        }
        if (!op->swap && op->variant == ORACLES_VARIANT_NONE) { h->returns[b].waiting = 0; continue; }
        h->asked = ASKED_RETURN;
        h->asked_button = b;
        return;
    }
}

void oracles_item_hotkeys_on_write_point(OraclesItemHotkeys *h, const OraclesGuestInventoryState *s, OraclesGuestInventoryOp *op)
{
    h->asked = ASKED_NOTHING;
    /* The inventory open since the use: the player has the buttons in hand, the returns are given up.  Read in the
     * state: Start opens it through openMenu_body, and no event of openMenu's bank-0 entry ever comes. */
    if (s->opened_menu_type && s->opened_menu_type == s->menu_inventory)
        for (unsigned b = 0; b < 2u; b++) if (h->returns[b].waiting) { h->returns[b].waiting = 0; h->stats.returns_abandoned++; }
    /* The quiet spell before a return: frames of normal play where nothing is in use on that button, nor pressed on it
     * for the player.  What goes on on the other button does not count: with the shield held there, the return would
     * never come, and the button would keep the slot's item for as long as the shield is up. */
    const int play = !(s->scroll_mode & 0x0eu) && oracles_guest_inventory_refusal(s) == ORACLES_INVENTORY_APPLIED;
    for (unsigned b = 0; b < 2u; b++) {
        if (!h->returns[b].waiting) continue;
        const int quiet = play && !(h->press.active && h->press.button == b) && !return_held_back(s, b, h->returns[b].used, h->returns[b].original);
        h->returns[b].quiet = quiet ? h->returns[b].quiet + 1u : 0u;
        h->returns[b].overdue = h->returns[b].quiet > ORACLES_HOTKEY_PATIENCE ? h->returns[b].overdue + 1u : 0u;
        if (h->returns[b].overdue > h->stats.return_longest_delay) h->stats.return_longest_delay = h->returns[b].overdue;
    }
    if (h->request.active) {
        /* The press of another slot, given up for this one, first lasts until the game has read it where it uses items. */
        if (h->press.active && h->press.slot != h->request.slot && !h->press.used && h->press.polls_released <= ORACLES_HOTKEY_PATIENCE) return;
        if (h->mode == ORACLES_HOTKEYS_USE) ask_use(h, s, op); else ask_equip(h, s, op);
        /* A request that waits with nothing asked (a key held on a mount) leaves the write point to the returns. */
        if (h->asked != ASKED_NOTHING || h->press.active) return;
    }
    ask_return(h, s, op);
}

static void note_return(OraclesItemHotkeys *h, unsigned button_slot, const OraclesGuestInventoryState *before, uint8_t used)
{
    /* The same item again with another variant (two slots of the satchel): the variant to give back is still the one
     * it had before the first use, not the first slot's. */
    const int same_item_again = h->returns[button_slot].waiting && h->returns[button_slot].used == used;
    /* A chain of uses keeps the item the button held before the first of them. */
    if (!h->returns[button_slot].waiting) { h->returns[button_slot].waiting = 1; h->returns[button_slot].original = before->slots[button_slot]; }
    h->returns[button_slot].used = used;
    if (!same_item_again) h->returns[button_slot].variant = oracles_guest_item_variant(before, used);
    h->returns[button_slot].quiet = 0;
}

void oracles_item_hotkeys_on_result(OraclesItemHotkeys *h, const OraclesGuestInventoryState *before, const OraclesGuestInventoryOp *op, OraclesGuestInventoryVerdict verdict)
{
    const int asked = h->asked;
    h->asked = ASKED_NOTHING;
    h->stats.last_verdict = verdict;
    const int applied = verdict == ORACLES_INVENTORY_APPLIED;
    if (applied) { h->stats.applied++; if (h->sink.applied) h->sink.applied(h->sink.opaque, before, op); }
    if (asked == ASKED_RETURN) {
        if (applied) { h->returns[h->asked_button].waiting = 0; h->stats.returns++; }
        return;   /* otherwise it goes on waiting: a lasting condition postpones a return instead of refusing it */
    }
    if (asked == ASKED_NOTHING || !h->request.active) return;
    if (applied || verdict == ORACLES_INVENTORY_NOTHING) {
        const OraclesHotkeySlot *slot = &h->slots[h->request.slot];
        if (asked == ASKED_USE) {
            if (applied) note_return(h, h->asked_button, before, slot->item);
            begin_press(h, h->request.slot, h->asked_button, slot->item);
        } else {
            if (applied && op->swap) { h->previous[h->asked_button].valid = 1; h->previous[h->asked_button].item = before->slots[h->asked_button]; }
            h->request.active = 0;
        }
    } else if (oracles_guest_inventory_verdict_postpones(verdict)) postpone_request(h, verdict);
    else drop_request(h, 1, verdict);
}

void oracles_item_hotkeys_on_event(OraclesItemHotkeys *h, const OraclesGuestEvent *event, uint8_t menu_inventory)
{
    (void)menu_inventory;
    if (!h->press.active) return;
    /* Durations are counted in readings of the joypad by the game, not in frames of the launcher (fact 11). */
    if (event->type == ORACLES_EVENT_INPUT) {
        if (h->press.gap) h->press.gap--;
        else if (h->press.in_mask) h->press.polled = 1;
        if (h->press.released) h->press.polls_released++;
    } else if (event->type == ORACLES_EVENT_USE_ITEMS && h->press.polled) h->press.used = 1;
}

static unsigned filtered(OraclesItemHotkeys *h, uint32_t frame, unsigned mask)
{
    if (!h->press.active) return mask;
    const unsigned bit = key_bit(h->press.button);
    if (h->press.gap) { h->press.in_mask = 0; return mask & ~bit; }
    /* Released, and the game has read the press where it uses items (or never will in this state): the press is over. */
    if (h->press.released && (h->press.used || h->press.polls_released > ORACLES_HOTKEY_PATIENCE)) { h->press.active = 0; give_the_button_back(h, h->press.slot); return mask; }
    if (!h->press.announced) {
        h->press.announced = 1;
        h->stats.uses++;
        if (h->sink.use_started) h->sink.use_started(h->sink.opaque, frame, h->press.button, h->press.item);
    }
    h->press.in_mask = 1;
    return mask | bit;
}

unsigned oracles_item_hotkeys_filter_keys(OraclesItemHotkeys *h, uint32_t frame, unsigned mask)
{
    h->player_mask = mask;
    h->sent_mask = filtered(h, frame, mask);
    return h->sent_mask;
}

static void assign(OraclesItemHotkeys *h, unsigned n, const OraclesGuestInventoryState *s, uint8_t item, unsigned target)
{
    memset(&h->slots[n], 0, sizeof h->slots[n]);
    if (!item) return;
    h->slots[n].set = 1;
    h->slots[n].item = item;
    h->slots[n].variant = oracles_guest_item_variant(s, item);
    h->slots[n].target = (uint8_t)target;
}

OraclesHotkeyOutcome oracles_item_hotkeys_key(OraclesItemHotkeys *h, unsigned n, int pressed, OraclesHotkeyModifier modifier)
{
    if (!h || n >= ORACLES_HOTKEY_SLOTS) return ORACLES_HOTKEY_IGNORED;
    if (!pressed) {
        /* A request still waiting goes on to its press, even with its key up (point 4.4): a tap made during a scroll
         * is not lost to a key held before it.  The button goes back to such a key when that press is over. */
        h->key_down[n] = 0;
        if (h->press.active && h->press.slot == n) h->press.released = 1;
        return ORACLES_HOTKEY_IGNORED;
    }
    if (h->guest) {
        OraclesGuestInventoryState s;
        unsigned cursor = 0;
        const int in_menu = oracles_guest_inventory_cursor(h->guest, &cursor);
        if (in_menu || modifier != ORACLES_HOTKEY_PLAIN) {
            oracles_guest_inventory_state(h->guest, &s);
            if (!in_menu && oracles_guest_inventory_refusal(&s) != ORACLES_INVENTORY_APPLIED) return ORACLES_HOTKEY_IGNORED;
            const unsigned from = in_menu ? cursor : modifier == ORACLES_HOTKEY_BIND_A ? ORACLES_INVENTORY_SLOT_A : ORACLES_INVENTORY_SLOT_B;
            const uint8_t item = s.slots[from] < s.num_inventory_items && s.slots[from] != s.item_biggoron_sword ? s.slots[from] : 0u;
            assign(h, n, &s, item, modifier == ORACLES_HOTKEY_BIND_A ? ORACLES_INVENTORY_SLOT_A : ORACLES_INVENTORY_SLOT_B);
            return item ? ORACLES_HOTKEY_ASSIGNED : ORACLES_HOTKEY_EMPTIED;
        }
    } else if (modifier != ORACLES_HOTKEY_PLAIN) return ORACLES_HOTKEY_IGNORED;
    if (h->mode == ORACLES_HOTKEYS_OFF) return ORACLES_HOTKEY_IGNORED;
    h->key_down[n] = 1;
    h->turned_down[n] = 0;
    h->key_order[n] = ++h->presses;
    if (!h->slots[n].set) return ORACLES_HOTKEY_IGNORED;
    /* One key at a time: the last one pressed holds the carrier button alone. */
    if (h->press.active && h->press.slot != n) h->press.released = 1;
    h->stats.requests++;   /* a key press, not the turn a held key gets back */
    ask_for(h, n);
    return ORACLES_HOTKEY_TRIGGERED;
}

void oracles_item_hotkeys_reset(OraclesItemHotkeys *h)
{
    if (!h) return;
    memset(&h->request, 0, sizeof h->request);
    memset(&h->press, 0, sizeof h->press);
    memset(h->returns, 0, sizeof h->returns);
    memset(h->previous, 0, sizeof h->previous);
    memset(h->key_down, 0, sizeof h->key_down);   /* a key still held triggers nothing until it is pressed again */
    memset(h->key_order, 0, sizeof h->key_order);
    h->asked = ASKED_NOTHING;
}

int oracles_item_hotkeys_idle(const OraclesItemHotkeys *h)
{
    return !h->request.active && !h->press.active && !h->returns[0].waiting && !h->returns[1].waiting && !h->previous[0].valid && !h->previous[1].valid;
}

/* ---- on a guest ----------------------------------------------------------------------------- */

static void guest_policy(void *opaque, const OraclesGuestInventoryState *state, OraclesGuestInventoryOp *op) { oracles_item_hotkeys_on_write_point(opaque, state, op); }
static void guest_result(void *opaque, const OraclesGuestInventoryState *before, const OraclesGuestInventoryOp *op, OraclesGuestInventoryVerdict verdict) { oracles_item_hotkeys_on_result(opaque, before, op, verdict); }
static void guest_reset(void *opaque) { oracles_item_hotkeys_reset(opaque); }
static void guest_event(void *opaque, const OraclesGuestEvent *event)
{
    OraclesItemHotkeys *h = opaque;
    oracles_item_hotkeys_on_event(h, event, oracles_guest_tables(h->guest)->menu_inventory);
}

OraclesItemHotkeys *oracles_item_hotkeys_start(OraclesGuest *guest, OraclesHotkeysMode mode)
{
    OraclesItemHotkeys *h = calloc(1, sizeof *h);
    if (!h) return NULL;
    h->guest = guest;
    h->mode = mode;
    if (!guest) return h;
    if (oracles_guest_set_inventory_policy(guest, guest_policy, guest_result, guest_reset, h) != 0) { free(h); return NULL; }
    if (oracles_guest_add_event_listener(guest, guest_event, h) != 0) { oracles_guest_set_inventory_policy(guest, NULL, NULL, NULL, NULL); free(h); return NULL; }
    return h;
}

void oracles_item_hotkeys_stop(OraclesItemHotkeys *h)
{
    if (!h) return;
    if (h->guest) {
        oracles_guest_remove_event_listener(h->guest, guest_event, h);
        oracles_guest_set_inventory_policy(h->guest, NULL, NULL, NULL, NULL);
    }
    free(h);
}

void oracles_item_hotkeys_set_sink(OraclesItemHotkeys *h, const OraclesItemHotkeysSink *sink)
{
    if (sink) h->sink = *sink; else memset(&h->sink, 0, sizeof h->sink);
}

void oracles_item_hotkeys_set_slot(OraclesItemHotkeys *h, unsigned n, const OraclesHotkeySlot *slot)
{
    if (n < ORACLES_HOTKEY_SLOTS && slot) h->slots[n] = *slot;
}

void oracles_item_hotkeys_slot(const OraclesItemHotkeys *h, unsigned n, OraclesHotkeySlot *out)
{
    if (n < ORACLES_HOTKEY_SLOTS && out) *out = h->slots[n];
}

void oracles_item_hotkeys_slot_status(const OraclesItemHotkeys *h, unsigned n, int *pending, unsigned *dropped)
{
    if (n >= ORACLES_HOTKEY_SLOTS) return;
    if (pending) *pending = h->request.active && h->request.slot == n;
    if (dropped) *dropped = h->slot_dropped[n];
}

void oracles_item_hotkeys_live_stats(const OraclesItemHotkeys *h, OraclesItemHotkeysLiveStats *out)
{
    if (!out) return;
    *out = h->stats;
    out->returns_pending = (unsigned)(h->returns[0].waiting != 0) + (unsigned)(h->returns[1].waiting != 0);
}
