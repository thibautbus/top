/* Item hotkeys, the live policy, without a ROM: driven
 * turn by turn by a stand-in for the game's loop (the joypad read, the two
 * item buttons checked, then the write point), and the buttons the game
 * reads in each state of Link (fact 10). */
#include "item_hotkeys.h"
#include "core.h"
#include "hotkeys_fixture.h"

#include <stdio.h>
#include <stdlib.h>

static int failures;
#define CHECK(c) do { if (!(c)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

#define SWORD 0x05u
#define SHIELD 0x01u
#define HOOK 0x0au
#define SATCHEL 0x19u
#define FEATHER 0x17u
#define MENU_INVENTORY 1u

typedef struct sim {
    OraclesGuestInventoryState s;
    OraclesItemHotkeys *h;
    uint32_t frame;
    unsigned keys_before;                  /* the joypad at the game's last reading */
    unsigned presses[2];                   /* new presses of B and of A the game saw where it uses items */
    uint8_t pressed_item[2];               /* the item that button held at the last of them */
    unsigned exchanges, held_frames;
    int lag;                               /* this frame the game's loop does not come round: no reading, no write point */
} sim;

static void event(sim *m, OraclesGuestEventType type, uint8_t a)
{
    OraclesGuestEvent e;
    memset(&e, 0, sizeof e);
    e.type = type; e.frame = m->frame; e.a = a;
    oracles_item_hotkeys_on_event(m->h, &e, MENU_INVENTORY);
}

/* One frame of the launcher: the mask goes to the core, and unless the game lags, one turn of its loop. */
static void turn(sim *m, unsigned player_mask)
{
    const unsigned keys = oracles_item_hotkeys_filter_keys(m->h, m->frame, player_mask);
    if (keys & (ORACLES_KEY_A | ORACLES_KEY_B) & ~player_mask) m->held_frames++;
    if (!m->lag) {
        event(m, ORACLES_EVENT_INPUT, 0);
        const unsigned just = keys & ~m->keys_before;
        m->keys_before = keys;
        if (oracles_guest_item_buttons(&m->s)) {
            event(m, ORACLES_EVENT_USE_ITEMS, 0);
            if (just & ORACLES_KEY_B) { m->presses[0]++; m->pressed_item[0] = m->s.slots[0]; }
            if (just & ORACLES_KEY_A) { m->presses[1]++; m->pressed_item[1] = m->s.slots[1]; }
        }
        OraclesGuestInventoryOp op;
        memset(&op, 0, sizeof op);
        m->s.frame = m->frame;
        oracles_item_hotkeys_on_write_point(m->h, &m->s, &op);
        if (op.swap || op.variant != ORACLES_VARIANT_NONE) {
            const OraclesGuestInventoryState before = m->s;
            const OraclesGuestInventoryVerdict verdict = oracles_guest_inventory_check(&m->s, &op);
            if (verdict == ORACLES_INVENTORY_APPLIED) {
                if (op.swap) { const uint8_t held = m->s.slots[op.slot_a]; m->s.slots[op.slot_a] = m->s.slots[op.slot_b]; m->s.slots[op.slot_b] = held; }
                if (op.variant == ORACLES_VARIANT_SATCHEL) m->s.satchel_seeds = op.variant_value;
                m->exchanges++;
                CHECK(oracles_guest_inventory_consistent(m->s.slots));
            }
            oracles_item_hotkeys_on_result(m->h, &before, &op, verdict);
        }
    }
    m->frame++;
}

static void turns(sim *m, unsigned count, unsigned player_mask) { for (unsigned i = 0; i < count; i++) turn(m, player_mask); }

static sim start(OraclesGame game, OraclesHotkeysMode mode)
{
    sim m;
    memset(&m, 0, sizeof m);
    m.s = playable(game);
    m.h = oracles_item_hotkeys_start(NULL, mode);
    const OraclesHotkeySlot feather = { 1, FEATHER, ORACLES_HOTKEY_NO_VARIANT, ORACLES_INVENTORY_SLOT_B };
    const OraclesHotkeySlot satchel = { 1, SATCHEL, 1, ORACLES_INVENTORY_SLOT_A };
    const OraclesHotkeySlot hook = { 1, HOOK, ORACLES_HOTKEY_NO_VARIANT, ORACLES_INVENTORY_SLOT_B };
    oracles_item_hotkeys_set_slot(m.h, 0, &feather);
    oracles_item_hotkeys_set_slot(m.h, 1, &satchel);
    oracles_item_hotkeys_set_slot(m.h, 2, &hook);
    return m;
}

static void tap(sim *m, unsigned slot)
{
    CHECK(oracles_item_hotkeys_key(m->h, slot, 1, ORACLES_HOTKEY_PLAIN) == ORACLES_HOTKEY_TRIGGERED);
    turn(m, 0);
    oracles_item_hotkeys_key(m->h, slot, 0, ORACLES_HOTKEY_PLAIN);
}

static void test_use_tap_and_return(void)
{
    sim m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    tap(&m, 0);
    CHECK(m.exchanges == 1 && m.s.slots[0] == FEATHER && m.s.slots[5] == SWORD);   /* exchanged at the write point of the press */
    CHECK(m.presses[0] == 0);                                                      /* and pressed from the next reading on */
    turns(&m, 3, 0);
    CHECK(m.presses[0] == 1 && m.pressed_item[0] == FEATHER && m.presses[1] == 0);
    CHECK(m.held_frames == 1);                                                     /* a tap gives the game a tap */
    turns(&m, ORACLES_HOTKEY_PATIENCE - 3u, 0);
    CHECK(m.s.slots[0] == FEATHER);                                                /* the quiet spell is not over */
    OraclesItemHotkeysLiveStats stats;
    oracles_item_hotkeys_live_stats(m.h, &stats);
    CHECK(stats.returns_pending == 1 && stats.return_longest_delay == 0);
    turns(&m, 8, 0);
    CHECK(m.exchanges == 2 && m.s.slots[0] == SWORD && m.s.slots[5] == FEATHER);
    oracles_item_hotkeys_live_stats(m.h, &stats);
    CHECK(stats.returns_pending == 0 && stats.return_longest_delay <= 1);          /* a due return comes at the next write point */
    CHECK(oracles_item_hotkeys_idle(m.h));
    oracles_item_hotkeys_stop(m.h);
}

static void test_use_held_and_taps(void)
{
    sim m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    oracles_item_hotkeys_key(m.h, 0, 1, ORACLES_HOTKEY_PLAIN);
    turns(&m, 50, 0);
    CHECK(m.held_frames == 49 && m.presses[0] == 1);                               /* a held key is a held button */
    CHECK(m.s.slots[0] == FEATHER);
    oracles_item_hotkeys_key(m.h, 0, 0, ORACLES_HOTKEY_PLAIN);
    turns(&m, 10, 0);
    /* taps during the quiet spell: the item is already there, one exchange for the whole series */
    for (unsigned i = 0; i < 3u; i++) { tap(&m, 0); turns(&m, 6, 0); }
    CHECK(m.exchanges == 1 && m.presses[0] == 4 && m.pressed_item[0] == FEATHER);
    turns(&m, ORACLES_HOTKEY_PATIENCE + 8u, 0);
    CHECK(m.exchanges == 2 && m.s.slots[0] == SWORD);
    oracles_item_hotkeys_stop(m.h);
}

static void test_use_physical_button_down(void)
{
    sim m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    turns(&m, 3, ORACLES_KEY_B);                                                   /* the player holds B: one press of the sword */
    CHECK(m.presses[0] == 1 && m.pressed_item[0] == SWORD);
    oracles_item_hotkeys_key(m.h, 0, 1, ORACLES_HOTKEY_PLAIN);
    turns(&m, 4, ORACLES_KEY_B);
    CHECK(m.presses[0] == 2 && m.pressed_item[0] == FEATHER);                      /* B came up for one reading, then down: a new press */
    oracles_item_hotkeys_key(m.h, 0, 0, ORACLES_HOTKEY_PLAIN);
    oracles_item_hotkeys_stop(m.h);
}

static void test_use_carrier(void)
{
    /* the shield held on A: the feather goes on B, and the shield is not touched */
    sim m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    m.s.parents[0].enabled = 1; m.s.parents[0].id = SHIELD; m.s.parents[0].button = ORACLES_BUTTON_A;
    tap(&m, 0);
    turns(&m, 2, ORACLES_KEY_A);
    CHECK(m.s.slots[0] == FEATHER && m.s.slots[1] == SHIELD && m.presses[0] == 1);
    oracles_item_hotkeys_stop(m.h);
    /* the sword charging on B: the feather goes on A */
    m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    m.s.parents[4].enabled = 1; m.s.parents[4].id = SWORD; m.s.parents[4].button = ORACLES_BUTTON_B;
    tap(&m, 0);
    turns(&m, 2, ORACLES_KEY_B);
    CHECK(m.s.slots[1] == FEATHER && m.s.slots[0] == SWORD && m.presses[1] == 1 && m.pressed_item[1] == FEATHER);
    /* and A has its shield back after its own quiet spell, the sword still charging on B: what goes on on the other
     * button does not hold a return back, or A would keep the feather for as long as B is held */
    turns(&m, ORACLES_HOTKEY_PATIENCE + 8u, ORACLES_KEY_B);
    CHECK(m.s.slots[1] == SHIELD && m.s.slots[0] == SWORD && oracles_guest_inventory_consistent(m.s.slots));
    /* while an item in use on A itself does hold it back */
    tap(&m, 0);
    turns(&m, 3, ORACLES_KEY_B);
    m.s.parents[1].enabled = 1; m.s.parents[1].id = FEATHER; m.s.parents[1].button = ORACLES_BUTTON_A;
    turns(&m, 3u * ORACLES_HOTKEY_PATIENCE, ORACLES_KEY_B);
    CHECK(m.s.slots[1] == FEATHER);
    m.s.parents[1].enabled = 0;
    turns(&m, ORACLES_HOTKEY_PATIENCE + 8u, ORACLES_KEY_B);
    CHECK(m.s.slots[1] == SHIELD);
    oracles_item_hotkeys_stop(m.h);
    /* both in use: the request waits, then is given up, and no button was pressed for the player */
    m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    m.s.parents[0].enabled = 1; m.s.parents[0].id = SHIELD; m.s.parents[0].button = ORACLES_BUTTON_A;
    m.s.parents[4].enabled = 1; m.s.parents[4].id = SWORD; m.s.parents[4].button = ORACLES_BUTTON_B;
    tap(&m, 0);
    turns(&m, ORACLES_HOTKEY_PATIENCE + 5u, 0);
    CHECK(m.exchanges == 0 && m.held_frames == 0 && oracles_item_hotkeys_idle(m.h));
    oracles_item_hotkeys_stop(m.h);
    /* under water in Ages the game reads A alone: an item sitting on B is moved to A */
    m = start(ORACLES_GAME_AGES, ORACLES_HOTKEYS_USE);
    m.s.tileset_flags = 1u << 6;
    m.s.slots[0] = FEATHER; m.s.slots[5] = SWORD;
    tap(&m, 0);
    turns(&m, 2, 0);
    CHECK(m.s.slots[1] == FEATHER && m.s.slots[0] == SHIELD && m.presses[1] == 1);
    oracles_item_hotkeys_stop(m.h);
}

static void test_use_states_without_buttons(void)
{
    /* swimming: no item button is read; the request waits thirty write points and goes */
    sim m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    m.s.link_swimming_state = 1;
    tap(&m, 0);
    turns(&m, 10, 0);
    m.s.link_swimming_state = 0;                                                   /* out of the water within the patience: the use goes through */
    turns(&m, 3, 0);
    CHECK(m.exchanges == 1 && m.presses[0] == 1);
    oracles_item_hotkeys_stop(m.h);
    m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    m.s.link_object_index = 0xd1;                                                  /* on a mount B dismounts: never pressed for the player */
    tap(&m, 0);
    turns(&m, ORACLES_HOTKEY_PATIENCE + 5u, 0);
    CHECK(m.exchanges == 0 && m.held_frames == 0 && oracles_item_hotkeys_idle(m.h));
    oracles_item_hotkeys_stop(m.h);
    /* a menu or a text: refused at once, and above all no button pressed into it, even for an item already on B */
    m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    m.s.slots[0] = FEATHER; m.s.slots[5] = SWORD;
    m.s.opened_menu_type = MENU_INVENTORY;
    tap(&m, 0);
    turns(&m, 5, 0);
    m.s.opened_menu_type = 0; m.s.text_is_active = 1;
    tap(&m, 0);
    turns(&m, 5, 0);
    CHECK(m.held_frames == 0 && oracles_item_hotkeys_idle(m.h));
    oracles_item_hotkeys_stop(m.h);
}

static void test_use_lag_and_key_switch(void)
{
    /* a frame the game's loop does not come round to: the press lasts until a reading has seen it */
    sim m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    tap(&m, 0);
    m.lag = 1; turns(&m, 2, 0); m.lag = 0;
    CHECK(m.presses[0] == 0 && m.held_frames == 2);
    turns(&m, 2, 0);
    CHECK(m.presses[0] == 1 && m.pressed_item[0] == FEATHER);
    oracles_item_hotkeys_stop(m.h);
    /* a second key while the first is held: the last one pressed holds the button alone, the chain keeps the sword to give back */
    m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    oracles_item_hotkeys_key(m.h, 0, 1, ORACLES_HOTKEY_PLAIN);
    turns(&m, 5, 0);
    oracles_item_hotkeys_key(m.h, 2, 1, ORACLES_HOTKEY_PLAIN);
    turns(&m, 6, 0);
    CHECK(m.s.slots[0] == HOOK && m.pressed_item[0] == HOOK && m.presses[0] == 2);
    oracles_item_hotkeys_key(m.h, 0, 0, ORACLES_HOTKEY_PLAIN);
    oracles_item_hotkeys_key(m.h, 2, 0, ORACLES_HOTKEY_PLAIN);
    turns(&m, ORACLES_HOTKEY_PATIENCE + 8u, 0);
    CHECK(m.s.slots[0] == SWORD && oracles_guest_inventory_consistent(m.s.slots) && oracles_item_hotkeys_idle(m.h));
    oracles_item_hotkeys_stop(m.h);
}

/* The item already on its button and the key tapped during a scroll: no exchange is needed, and the press still waits
 * for the scroll to end, where the game would find the button already down and start nothing. */
static void test_use_during_a_scroll(void)
{
    sim m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    m.s.slots[5] = m.s.slots[0]; m.s.slots[0] = FEATHER;
    m.s.scroll_mode = 0x04u;
    tap(&m, 0);
    turns(&m, 10, 0);
    CHECK(m.presses[0] == 0 && m.held_frames == 0 && m.exchanges == 0);
    m.s.scroll_mode = 0;
    turns(&m, 3, 0);
    CHECK(m.presses[0] == 1 && m.pressed_item[0] == FEATHER && m.exchanges == 0);
    /* the same while the palette changes: Link's update returns before it uses items */
    turns(&m, ORACLES_HOTKEY_PATIENCE + 8u, 0);
    m.s.palette_thread_mode = 1;
    tap(&m, 0);
    turns(&m, 10, 0);
    CHECK(m.presses[0] == 1);
    m.s.palette_thread_mode = 0;
    turns(&m, 3, 0);
    CHECK(m.presses[0] == 2);
    oracles_item_hotkeys_stop(m.h);
}

static void test_use_held_key_takes_the_button_back(void)
{
    /* the hook's key held, the feather's tapped: the feather has the button for its tap, then the hook has it again */
    sim m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    oracles_item_hotkeys_key(m.h, 2, 1, ORACLES_HOTKEY_PLAIN);
    turns(&m, 10, 0);
    CHECK(m.pressed_item[0] == HOOK && m.presses[0] == 1);
    tap(&m, 0);
    turns(&m, 8, 0);
    CHECK(m.presses[0] == 3 && m.pressed_item[0] == HOOK && m.s.slots[0] == HOOK);   /* feather, then the hook again */
    CHECK(oracles_item_hotkeys_filter_keys(m.h, m.frame, 0) == ORACLES_KEY_B);       /* and held, as its key is */
    oracles_item_hotkeys_key(m.h, 2, 0, ORACLES_HOTKEY_PLAIN);
    turns(&m, ORACLES_HOTKEY_PATIENCE + 8u, 0);
    CHECK(m.s.slots[0] == SWORD && oracles_guest_inventory_consistent(m.s.slots) && oracles_item_hotkeys_idle(m.h));
    oracles_item_hotkeys_stop(m.h);
    /* a key held through a state where no button is read goes on asking, as a held button would; a tap does not */
    m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    m.s.link_swimming_state = 1;
    oracles_item_hotkeys_key(m.h, 0, 1, ORACLES_HOTKEY_PLAIN);
    turns(&m, 4u * ORACLES_HOTKEY_PATIENCE, 0);
    CHECK(m.exchanges == 0 && m.held_frames == 0);
    m.s.link_swimming_state = 0;
    turns(&m, 3, 0);
    CHECK(m.exchanges == 1 && m.presses[0] == 1 && m.pressed_item[0] == FEATHER);
    oracles_item_hotkeys_key(m.h, 0, 0, ORACLES_HOTKEY_PLAIN);
    oracles_item_hotkeys_stop(m.h);
    /* a lasting condition drops the request even with the key held: nothing fires when the text closes */
    m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    m.s.text_is_active = 1;
    oracles_item_hotkeys_key(m.h, 0, 1, ORACLES_HOTKEY_PLAIN);
    turns(&m, 5, 0);
    m.s.text_is_active = 0;
    turns(&m, 10, 0);
    CHECK(m.exchanges == 0 && m.held_frames == 0);
    oracles_item_hotkeys_stop(m.h);
}

/* Two returns, one a button, must not undo each other. */
static void test_use_two_returns(void)
{
    sim m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    const OraclesHotkeySlot sword = { 1, SWORD, ORACLES_HOTKEY_NO_VARIANT, 0 };
    oracles_item_hotkeys_set_slot(m.h, 1, &sword);
    tap(&m, 2); turns(&m, 3, 0);                                                   /* the hook on B, in use */
    m.s.parents[1].enabled = 1; m.s.parents[1].id = HOOK; m.s.parents[1].button = ORACLES_BUTTON_B;
    oracles_item_hotkeys_key(m.h, 1, 1, ORACLES_HOTKEY_PLAIN); turns(&m, 3, 0);    /* the sword, B's own item, goes to A */
    m.s.parents[4].enabled = 1; m.s.parents[4].id = SWORD; m.s.parents[4].button = ORACLES_BUTTON_A;
    turns(&m, 20, 0); m.s.parents[1].enabled = 0; turns(&m, 40, 0);
    oracles_item_hotkeys_key(m.h, 1, 0, ORACLES_HOTKEY_PLAIN); turns(&m, 3, 0);
    m.s.parents[4].enabled = 0; turns(&m, 3u * ORACLES_HOTKEY_PATIENCE, 0);
    CHECK(m.s.slots[0] == SWORD && m.s.slots[1] == SHIELD && oracles_item_hotkeys_idle(m.h));   /* A's return first, then B's */
    oracles_item_hotkeys_stop(m.h);
}

/* What a key held before another one keeps, and what a tap made during a wait keeps. */
static void test_use_keys_in_turn(void)
{
    /* the hook's key held, a scroll, the feather tapped and released during it: the feather still goes, then the hook again */
    sim m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    oracles_item_hotkeys_key(m.h, 2, 1, ORACLES_HOTKEY_PLAIN);
    turns(&m, 10, 0);
    m.s.scroll_mode = 4; m.s.cutscene_index = 0;                                   /* as the game runs a scroll: under CUTSCENE_LOADING_ROOM */
    tap(&m, 0);
    turns(&m, 12, 0);
    CHECK(m.pressed_item[0] == HOOK && m.presses[0] == 1);
    m.s.scroll_mode = 1; m.s.cutscene_index = 1;
    turns(&m, 12, 0);
    CHECK(m.presses[0] == 3 && m.pressed_item[0] == HOOK && m.s.slots[0] == HOOK);   /* the feather had its press in between */
    oracles_item_hotkeys_key(m.h, 2, 0, ORACLES_HOTKEY_PLAIN);
    oracles_item_hotkeys_stop(m.h);
    /* the hook's key held, a key whose item is not in the inventory tapped: refused, and the hook has the button again */
    m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    const OraclesHotkeySlot missing = { 1, 0x08, ORACLES_HOTKEY_NO_VARIANT, 0 };
    oracles_item_hotkeys_set_slot(m.h, 3, &missing);
    oracles_item_hotkeys_key(m.h, 2, 1, ORACLES_HOTKEY_PLAIN);
    turns(&m, 10, 0);
    tap(&m, 3);
    turns(&m, 8, 0);
    CHECK(oracles_item_hotkeys_filter_keys(m.h, m.frame, 0) == ORACLES_KEY_B && m.s.slots[0] == HOOK);
    oracles_item_hotkeys_key(m.h, 2, 0, ORACLES_HOTKEY_PLAIN);
    oracles_item_hotkeys_stop(m.h);
    /* a key held on an animal waits, and does not hold back the return of an earlier use */
    m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    tap(&m, 0);
    turns(&m, 5, 0);
    m.s.link_object_index = 0xd1;
    oracles_item_hotkeys_key(m.h, 2, 1, ORACLES_HOTKEY_PLAIN);
    turns(&m, 2u * ORACLES_HOTKEY_PATIENCE, 0);
    CHECK(m.s.slots[0] == SWORD);
    oracles_item_hotkeys_key(m.h, 2, 0, ORACLES_HOTKEY_PLAIN);
    oracles_item_hotkeys_stop(m.h);
    /* a chain of uses: the satchel's seeds come back though the feather took its button */
    m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    m.s.satchel_seeds = 0;
    tap(&m, 1);
    turns(&m, 6, 0);
    CHECK(m.s.satchel_seeds == 1);
    tap(&m, 0);
    turns(&m, 6, 0);
    CHECK(m.s.slots[0] == FEATHER && m.s.satchel_seeds == 0);
    turns(&m, ORACLES_HOTKEY_PATIENCE + 8u, 0);
    CHECK(m.s.slots[0] == SWORD && oracles_guest_inventory_consistent(m.s.slots));
    oracles_item_hotkeys_stop(m.h);
}

/* Two keys held through a text: each is refused once, and neither hands the request back to the other for ever. */
static void test_use_two_keys_held_in_a_text(void)
{
    sim m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    m.s.text_is_active = 1;
    oracles_item_hotkeys_key(m.h, 0, 1, ORACLES_HOTKEY_PLAIN);
    turn(&m, 0);
    oracles_item_hotkeys_key(m.h, 2, 1, ORACLES_HOTKEY_PLAIN);
    turns(&m, 100, 0);
    OraclesItemHotkeysLiveStats stats;
    oracles_item_hotkeys_live_stats(m.h, &stats);
    CHECK(stats.requests == 2 && stats.dropped_refused == 2 && m.exchanges == 0);
    /* pressed again once the text is over, a key works as ever */
    m.s.text_is_active = 0;
    oracles_item_hotkeys_key(m.h, 0, 0, ORACLES_HOTKEY_PLAIN);
    oracles_item_hotkeys_key(m.h, 2, 0, ORACLES_HOTKEY_PLAIN);
    tap(&m, 0);
    turns(&m, 3, 0);
    CHECK(m.s.slots[0] == FEATHER && m.presses[0] == 1);
    oracles_item_hotkeys_stop(m.h);
}

/* Two slots of the same item with two variants, one after the other within the quiet spell: the satchel gets back the
 * seeds the player had chosen, not those of the first slot. */
static void test_use_two_slots_of_one_item(void)
{
    sim m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    const OraclesHotkeySlot satchel0 = { 1, SATCHEL, 0, ORACLES_INVENTORY_SLOT_B };
    oracles_item_hotkeys_set_slot(m.h, 0, &satchel0);
    m.s.obtained_seeds = 0x1fu;
    m.s.satchel_seeds = 3;
    tap(&m, 0);
    turns(&m, 6, 0);
    CHECK(m.s.slots[0] == SATCHEL && m.s.satchel_seeds == 0);
    tap(&m, 1);
    turns(&m, 6, 0);
    CHECK(m.s.slots[0] == SATCHEL && m.s.satchel_seeds == 1);
    turns(&m, ORACLES_HOTKEY_PATIENCE + 8u, 0);
    CHECK(m.s.slots[0] == SWORD && m.s.satchel_seeds == 3 && oracles_item_hotkeys_idle(m.h));
    oracles_item_hotkeys_stop(m.h);
}

static void test_use_variant_and_returns_given_up(void)
{
    /* the satchel with the seeds of the slot, and the seeds it had given back with the button */
    sim m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    m.s.satchel_seeds = 0;
    tap(&m, 1);
    turns(&m, 3, 0);
    CHECK(m.s.slots[0] == SATCHEL && m.s.satchel_seeds == 1 && m.pressed_item[0] == SATCHEL);
    turns(&m, ORACLES_HOTKEY_PATIENCE + 8u, 0);
    CHECK(m.s.slots[0] == SWORD && m.s.satchel_seeds == 0);
    oracles_item_hotkeys_stop(m.h);
    /* the inventory opened since the use: the player has the buttons in hand */
    m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    tap(&m, 0);
    turns(&m, 5, 0);
    m.s.opened_menu_type = MENU_INVENTORY;                                        /* by Start: no event of openMenu comes, the state says it */
    turns(&m, 20, 0);
    m.s.opened_menu_type = 0;
    turns(&m, ORACLES_HOTKEY_PATIENCE + 8u, 0);
    CHECK(m.s.slots[0] == FEATHER && m.exchanges == 1 && oracles_item_hotkeys_idle(m.h));
    oracles_item_hotkeys_stop(m.h);
    /* the game took the item off the button: nothing to give back */
    m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    tap(&m, 0);
    turns(&m, 5, 0);
    m.s.slots[0] = 0;
    turns(&m, ORACLES_HOTKEY_PATIENCE + 8u, 0);
    CHECK(m.exchanges == 1 && oracles_item_hotkeys_idle(m.h));
    oracles_item_hotkeys_stop(m.h);
    /* an item in use holds the return back */
    m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    tap(&m, 2);
    turns(&m, 3, 0);
    m.s.parents[1].enabled = 1; m.s.parents[1].id = HOOK; m.s.parents[1].button = ORACLES_BUTTON_B;
    turns(&m, 3u * ORACLES_HOTKEY_PATIENCE, 0);
    CHECK(m.s.slots[0] == HOOK);
    m.s.parents[1].enabled = 0;
    turns(&m, ORACLES_HOTKEY_PATIENCE + 8u, 0);
    CHECK(m.s.slots[0] == SWORD);
    oracles_item_hotkeys_stop(m.h);
}

static void test_reset(void)
{
    sim m = start(ORACLES_GAME_SEASONS, ORACLES_HOTKEYS_USE);
    oracles_item_hotkeys_key(m.h, 0, 1, ORACLES_HOTKEY_PLAIN);
    turns(&m, 5, 0);
    CHECK(!oracles_item_hotkeys_idle(m.h) && oracles_item_hotkeys_filter_keys(m.h, m.frame, 0) == ORACLES_KEY_B);
    oracles_item_hotkeys_reset(m.h);                                               /* a savestate load */
    CHECK(oracles_item_hotkeys_idle(m.h) && oracles_item_hotkeys_filter_keys(m.h, m.frame, 0) == 0);
    const unsigned exchanges = m.exchanges;
    turns(&m, 3u * ORACLES_HOTKEY_PATIENCE, 0);
    CHECK(m.exchanges == exchanges && m.held_frames == 4);                         /* no return of a use the loaded state never had */
    oracles_item_hotkeys_stop(m.h);
}

static void test_equip_mode(void)
{
    sim m = start(ORACLES_GAME_AGES, ORACLES_HOTKEYS_EQUIP);
    tap(&m, 0);
    turns(&m, 3, 0);
    CHECK(m.s.slots[0] == FEATHER && m.held_frames == 0);                          /* equips, presses nothing */
    turns(&m, 3u * ORACLES_HOTKEY_PATIENCE, 0);
    CHECK(m.s.slots[0] == FEATHER && m.exchanges == 1);                            /* and nothing comes back by itself */
    tap(&m, 0);                                                                    /* already there: the item the button held before */
    turns(&m, 3, 0);
    CHECK(m.s.slots[0] == SWORD && m.exchanges == 2);
    tap(&m, 1);                                                                    /* the satchel's slot targets A, with its seeds */
    turns(&m, 3, 0);
    CHECK(m.s.slots[1] == SATCHEL && m.s.satchel_seeds == 1);
    oracles_item_hotkeys_reset(m.h);
    tap(&m, 1);                                                                    /* the memory of the previous item goes with a load */
    turns(&m, 3, 0);
    CHECK(m.s.slots[1] == SATCHEL && m.exchanges == 3);
    oracles_item_hotkeys_stop(m.h);
    /* off: a key does nothing */
    m = start(ORACLES_GAME_AGES, ORACLES_HOTKEYS_OFF);
    CHECK(oracles_item_hotkeys_key(m.h, 0, 1, ORACLES_HOTKEY_PLAIN) == ORACLES_HOTKEY_IGNORED);
    turns(&m, 5, 0);
    CHECK(m.exchanges == 0 && m.held_frames == 0);
    oracles_item_hotkeys_stop(m.h);
}

/* checkUseItems, branch for branch (fact 10). */
static void test_item_buttons(void)
{
    const unsigned both = ORACLES_BUTTON_A | ORACLES_BUTTON_B;
    for (int g = 0; g < 2; g++) {
        const OraclesGame game = g ? ORACLES_GAME_AGES : ORACLES_GAME_SEASONS;
        const OraclesGuestInventoryState base = playable(game);
        OraclesGuestInventoryState s;
#define BUTTONS(change, expected) do { s = base; change; CHECK(oracles_guest_item_buttons(&s) == (expected)); } while (0)
        BUTTONS((void)0, both);
        BUTTONS(s.items_disabled = 0x80, 0u);
        BUTTONS(s.items_disabled = 0x7f, both);
        BUTTONS(s.in_shop = 1, 0u);
        BUTTONS(s.link_in_air = 0x80, 0u);
        BUTTONS(s.link_in_air = 0x01, both);                                       /* a jump: bit 7 alone stops the buttons */
        BUTTONS(s.link_in_spinner = 0x80, 0u);
        BUTTONS(s.link_grab_state = 1, 0u);
        BUTTONS(s.link_grabbed = 1, 0u);
        BUTTONS(s.link_climbing_vine = 0xff, 0u);
        BUTTONS(s.link_swimming_state = 1, 0u);
        BUTTONS(s.disabled_objects = 0x80, 0u);
        BUTTONS(s.disabled_objects = 0x01, 0u);
        BUTTONS(s.disabled_objects = 0x02, both);
        BUTTONS(s.link_object_index = 0xd1, 0u);                                   /* on an animal */
        BUTTONS((s.link_object_index = 0xd1, s.companion_id = 0x0a), both);        /* in a minecart Link's update goes on to checkUseItems */
        BUTTONS((s.link_object_index = 0xd1, s.companion_id = 0x13), game == ORACLES_GAME_AGES ? both : 0u);   /* the raft, Ages alone */
        BUTTONS(s.tileset_flags = 1u << 5, both);                                  /* a side-scrolling room, on foot */
        BUTTONS((s.tileset_flags = 1u << 5, s.link_swimming_state = 1), ORACLES_BUTTON_B);
        BUTTONS((s.tileset_flags = 1u << 5, s.link_swimming_state = 1, s.link_var2f = 0x80), game == ORACLES_GAME_AGES ? both : ORACLES_BUTTON_B);
        BUTTONS(s.tileset_flags = 1u << 6, game == ORACLES_GAME_AGES ? ORACLES_BUTTON_A : both);   /* under water, Ages alone */
#undef BUTTONS
    }
}

int main(void)
{
    test_use_tap_and_return();
    test_use_held_and_taps();
    test_use_physical_button_down();
    test_use_carrier();
    test_use_states_without_buttons();
    test_use_lag_and_key_switch();
    test_use_during_a_scroll();
    test_use_held_key_takes_the_button_back();
    test_use_two_returns();
    test_use_keys_in_turn();
    test_use_two_keys_held_in_a_text();
    test_use_two_slots_of_one_item();
    test_use_variant_and_returns_given_up();
    test_reset();
    test_equip_mode();
    test_item_buttons();
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_item_hotkeys_policy: ok\n");
    return 0;
}
