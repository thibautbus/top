/* Item hotkeys, the mechanism, without a ROM: the guest's conditions
 * on synthetic snapshots of both games, the two operations, the inventory's
 * consistency, and the transaction itself on a synthetic program that calls
 * the write point from mainThreadStart and from elsewhere. */
#include "item_hotkeys.h"
#include "guest_tables.h"
#include "core.h"
#include "hotkeys_fixture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const OraclesCompatProfile *original_profile(OraclesGame game)
{
    const OraclesRomInfo info = { .game = game, .revision = game == ORACLES_GAME_SEASONS ? ORACLES_ROM_REVISION_SEASONS_US : ORACLES_ROM_REVISION_AGES_US, .known = 1 };
    return oracles_compat_find(&info);
}

static int failures;
#define CHECK(c) do { if (!(c)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static OraclesGuestInventoryOp swap(unsigned a, unsigned b)
{
    OraclesGuestInventoryOp op;
    memset(&op, 0, sizeof op);
    op.swap = 1; op.slot_a = (uint8_t)a; op.slot_b = (uint8_t)b;
    return op;
}

static void test_conditions(OraclesGame game)
{
    const OraclesGuestInventoryState base = playable(game);
    OraclesGuestInventoryOp op = swap(0, 2);
    CHECK(oracles_guest_inventory_check(&base, &op) == ORACLES_INVENTORY_APPLIED);
    OraclesGuestInventoryOp none;
    memset(&none, 0, sizeof none);
    CHECK(oracles_guest_inventory_check(&base, &none) == ORACLES_INVENTORY_NOTHING);
    OraclesGuestInventoryState s;
#define VERDICT(change, expected) do { s = base; change; CHECK(oracles_guest_inventory_check(&s, &op) == (expected)); } while (0)
    /* the refusals: the lasting conditions, in their order */
    VERDICT(s.game_state = 1, ORACLES_INVENTORY_REFUSED_NOT_IN_PLAY);
    VERDICT(s.cutscene_index = 5, ORACLES_INVENTORY_REFUSED_NOT_IN_PLAY);
    VERDICT(s.cutscene_index = 0, ORACLES_INVENTORY_REFUSED_NOT_IN_PLAY);                        /* a room loading without a scroll: a warp */
    VERDICT((s.cutscene_index = 0, s.scroll_mode = 2), ORACLES_INVENTORY_REFUSED_NOT_IN_PLAY);   /* as the game has it: wScrollMode $02, the transition that does not scroll */
    VERDICT((s.cutscene_index = 0, s.scroll_mode = 4), ORACLES_INVENTORY_POSTPONED_SCROLL);
    VERDICT((s.cutscene_index = 0, s.scroll_mode = 8), ORACLES_INVENTORY_POSTPONED_SCROLL);      /* a scroll between two rooms runs under that cutscene: the request waits */
    VERDICT(s.cutscene_index = 0x13, game == ORACLES_GAME_SEASONS ? ORACLES_INVENTORY_APPLIED : ORACLES_INVENTORY_REFUSED_NOT_IN_PLAY);   /* Onox's final form calls updateMenus too */
    VERDICT(s.opened_menu_type = 1, ORACLES_INVENTORY_REFUSED_MENU_OPEN);
    VERDICT(s.text_is_active = 1, ORACLES_INVENTORY_REFUSED_TEXT);
    VERDICT(s.link_death_trigger = 1, ORACLES_INVENTORY_REFUSED_DEATH);
    VERDICT(s.intro_done = 0, ORACLES_INVENTORY_REFUSED_INTRO);
    VERDICT(s.use_simulated_input = 1, ORACLES_INVENTORY_REFUSED_SIMULATED_INPUT);
    VERDICT(s.use_simulated_input = 2, ORACLES_INVENTORY_APPLIED);           /* the inverted directions against Ganon are play */
    VERDICT(s.minigame_controller = 1, ORACLES_INVENTORY_REFUSED_MINIGAME);
    VERDICT(s.in_boxing_match = 1, ORACLES_INVENTORY_REFUSED_BOXING);
    VERDICT(s.slots[1] = 0x0c, ORACLES_INVENTORY_REFUSED_BIGGORON);
    VERDICT(s.slots[2] = 0x0c, ORACLES_INVENTORY_REFUSED_BIGGORON);          /* the item moved is the Biggoron sword */
    VERDICT(s.slots[7] = 0x0a, ORACLES_INVENTORY_REFUSED_INCONSISTENT);      /* a duplicate, as some minigames leave */
    /* the postponements */
    VERDICT(s.scroll_mode = 0x08, ORACLES_INVENTORY_POSTPONED_SCROLL);
    VERDICT(s.scroll_mode = 0x81, ORACLES_INVENTORY_APPLIED);                /* bit 7: the camera stepped in a large room */
    VERDICT(s.menu_disabled = 1, ORACLES_INVENTORY_POSTPONED_MENU_DISABLED);
    VERDICT(s.disable_link_collisions_and_menu = 1, ORACLES_INVENTORY_POSTPONED_COLLISIONS_DISABLED);
    VERDICT(s.link_playing_instrument = 1, ORACLES_INVENTORY_POSTPONED_INSTRUMENT);
    VERDICT(s.dont_update_status_bar = 1, ORACLES_INVENTORY_POSTPONED_STATUS_BAR_HIDDEN);
    /* a refusal comes before a postponement: a text during a scroll is refused */
    VERDICT((s.scroll_mode = 0x08, s.text_is_active = 1), ORACLES_INVENTORY_REFUSED_TEXT);
    /* an item in use (fact 6): postponed only when it answers to an exchanged button or is an exchanged item */
    VERDICT((s.parents[0].enabled = 1, s.parents[0].id = 0x01, s.parents[0].button = ORACLES_BUTTON_A), ORACLES_INVENTORY_APPLIED);             /* the shield held on A, B exchanged */
    VERDICT((s.parents[0].enabled = 1, s.parents[0].id = 0x05, s.parents[0].button = ORACLES_BUTTON_B), ORACLES_INVENTORY_POSTPONED_ITEM_IN_USE); /* the sword swinging on B */
    VERDICT((s.parents[4].enabled = 1, s.parents[4].id = 0x0a, s.parents[4].button = 0), ORACLES_INVENTORY_POSTPONED_ITEM_IN_USE);               /* the exchanged item itself in use */
    CHECK(oracles_guest_inventory_verdict_postpones(ORACLES_INVENTORY_POSTPONED_ITEM_IN_USE) && !oracles_guest_inventory_verdict_postpones(ORACLES_INVENTORY_REFUSED_TEXT)
          && !oracles_guest_inventory_verdict_postpones(ORACLES_INVENTORY_APPLIED));
    {   /* a variant alone waits for its own item */
        OraclesGuestInventoryOp seeds;
        memset(&seeds, 0, sizeof seeds); seeds.variant = ORACLES_VARIANT_SATCHEL; seeds.variant_value = 1;
        s = base; s.parents[1].enabled = 1; s.parents[1].id = 0x19;
        CHECK(oracles_guest_inventory_check(&s, &seeds) == ORACLES_INVENTORY_POSTPONED_ITEM_IN_USE);
        s.parents[1].id = 0x05;
        CHECK(oracles_guest_inventory_check(&s, &seeds) == ORACLES_INVENTORY_APPLIED);
    }
    /* operations the menu could not make */
    op = swap(2, 3); CHECK(oracles_guest_inventory_check(&base, &op) == ORACLES_INVENTORY_REFUSED_OPERATION);     /* two storage slots */
    op = swap(0, 0); CHECK(oracles_guest_inventory_check(&base, &op) == ORACLES_INVENTORY_REFUSED_OPERATION);
    op = swap(0, 18); CHECK(oracles_guest_inventory_check(&base, &op) == ORACLES_INVENTORY_REFUSED_OPERATION);
    op = swap(0, 1); CHECK(oracles_guest_inventory_check(&base, &op) == ORACLES_INVENTORY_APPLIED);              /* A and B: the menu's three equips */
    op = swap(0, 9); CHECK(oracles_guest_inventory_check(&base, &op) == ORACLES_INVENTORY_APPLIED);              /* an empty slot: how the menu unequips */
    op = swap(0, 2); op.expect = 1; op.expect_a = 0x05; op.expect_b = 0x0a; CHECK(oracles_guest_inventory_check(&base, &op) == ORACLES_INVENTORY_APPLIED);
    op.expect_b = 0x0b; CHECK(oracles_guest_inventory_check(&base, &op) == ORACLES_INVENTORY_REFUSED_EXPECTATION);
    /* variants */
    memset(&op, 0, sizeof op); op.variant = ORACLES_VARIANT_SATCHEL; op.variant_value = 1;
    CHECK(oracles_guest_inventory_check(&base, &op) == ORACLES_INVENTORY_APPLIED);
    op.variant_value = 0; CHECK(oracles_guest_inventory_check(&base, &op) == ORACLES_INVENTORY_NOTHING);         /* already selected */
    op.variant_value = 3; CHECK(oracles_guest_inventory_check(&base, &op) == ORACLES_INVENTORY_REFUSED_VARIANT); /* gale seeds not obtained */
    op.variant_value = 5; CHECK(oracles_guest_inventory_check(&base, &op) == ORACLES_INVENTORY_REFUSED_VARIANT);
    op.variant = ORACLES_VARIANT_HARP; op.variant_value = 2;
    CHECK(oracles_guest_inventory_check(&base, &op) == (game == ORACLES_GAME_AGES ? ORACLES_INVENTORY_APPLIED : ORACLES_INVENTORY_REFUSED_VARIANT));
    op.variant_value = 3; CHECK(oracles_guest_inventory_check(&base, &op) == ORACLES_INVENTORY_REFUSED_VARIANT); /* the tune of ages not obtained */
#undef VERDICT
}

static void test_operations(void)
{
    const OraclesGuestInventoryState s = playable(ORACLES_GAME_AGES);
    OraclesGuestInventoryOp op;
    /* equip: from the storage, from the other button, already there, absent */
    CHECK(oracles_item_hotkeys_equip(&s, 0x0a, ORACLES_HOTKEY_NO_VARIANT, 0, &op) && op.swap && op.slot_a == 0 && op.slot_b == 2 && !op.variant);
    CHECK(oracles_item_hotkeys_equip(&s, 0x01, ORACLES_HOTKEY_NO_VARIANT, 0, &op) && op.swap && op.slot_a == 0 && op.slot_b == 1);
    CHECK(oracles_item_hotkeys_equip(&s, 0x05, ORACLES_HOTKEY_NO_VARIANT, 0, &op) && !op.swap && !op.variant);
    CHECK(!oracles_item_hotkeys_equip(&s, 0x0d, ORACLES_HOTKEY_NO_VARIANT, 0, &op));
    /* a variant is asked only when the item has one and it differs */
    CHECK(oracles_item_hotkeys_equip(&s, 0x19, 1, 1, &op) && op.swap && op.slot_a == 1 && op.slot_b == 3 && op.variant == ORACLES_VARIANT_SATCHEL && op.variant_value == 1);
    CHECK(oracles_item_hotkeys_equip(&s, 0x19, 0, 1, &op) && op.swap && !op.variant);
    CHECK(oracles_item_hotkeys_equip(&s, 0x11, 2, 0, &op) && op.variant == ORACLES_VARIANT_HARP && op.variant_value == 2);
    CHECK(oracles_item_hotkeys_equip(&s, 0x0a, 2, 0, &op) && !op.variant);
    /* restore: after the switch hook was used from B, the sword comes back from where the exchange put it */
    OraclesGuestInventoryState used = s;
    used.slots[0] = 0x0a; used.slots[2] = 0x05;
    CHECK(oracles_item_hotkeys_restore(&used, 0, 0x0a, 0x05, ORACLES_HOTKEY_NO_VARIANT, &op) && op.swap && op.slot_a == 0 && op.slot_b == 2);
    CHECK(!oracles_item_hotkeys_restore(&s, 0, 0x0a, 0x05, ORACLES_HOTKEY_NO_VARIANT, &op));          /* B no longer holds the used item */
    used.slots[2] = 0;
    CHECK(!oracles_item_hotkeys_restore(&used, 0, 0x0a, 0x05, ORACLES_HOTKEY_NO_VARIANT, &op));       /* the original item has left the slots */
    CHECK(oracles_item_hotkeys_restore(&used, 0, 0x0a, 0, ORACLES_HOTKEY_NO_VARIANT, &op) && op.slot_b == 2);   /* B was empty: into the first empty storage slot */
    used.slots[0] = 0x19; used.slots[3] = 0x05; used.satchel_seeds = 1;
    CHECK(oracles_item_hotkeys_restore(&used, 0, 0x19, 0x05, 0, &op) && op.slot_b == 3 && op.variant == ORACLES_VARIANT_SATCHEL && op.variant_value == 0);
    /* consistency */
    CHECK(oracles_guest_inventory_consistent(s.slots));
    used = s; used.slots[9] = 0x05;
    CHECK(!oracles_guest_inventory_consistent(used.slots));
}

/* ---- the transaction on a synthetic program ---------------------------------------------- */

static unsigned asked, results, gfx_events;
static OraclesGuestInventoryVerdict last_verdict;

static void once_policy(void *opaque, const OraclesGuestInventoryState *state, OraclesGuestInventoryOp *op)
{
    (void)opaque; (void)state;
    if (asked++) return;
    *op = swap(0, 2);
    op->variant = ORACLES_VARIANT_SATCHEL; op->variant_value = 1;
}

static void on_result(void *opaque, const OraclesGuestInventoryState *before, const OraclesGuestInventoryOp *op, OraclesGuestInventoryVerdict verdict)
{
    (void)opaque; (void)before; (void)op;
    results++; last_verdict = verdict;
}

static void put8(OraclesGuest *guest, OraclesGuestSym sym, uint8_t value)
{
    uint8_t *wram = oracles_guest_wram_writable(guest, sym.bank);
    CHECK(wram != NULL);
    if (wram) wram[sym.addr - (sym.bank ? 0xd000u : 0xc000u)] = value;
}

static void test_transaction(void)
{
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    const uint16_t main_start = t->main_thread_start.addr, check = t->check_reload_status_bar_graphics.addr;
    const size_t size = 1024u * 1024u;
    uint8_t *rom = calloc(size, 1);
    CHECK(rom != NULL);
    if (!rom) return;
    memcpy(rom + 0x134, "ZELDA NAYRU", 11);
    rom[0x143] = 0xc0; rom[0x147] = 0x1b; rom[0x148] = 0x05; rom[0x149] = 0x02;
    rom[0x100] = 0x00; rom[0x101] = 0xc3; rom[0x102] = 0x50; rom[0x103] = 0x01;                        /* nop ; jp $0150 */
    const uint8_t program[] = { 0x31, 0x00, 0xc2,                                                       /* $0150: ld sp,$c200 */
                                0xcd, (uint8_t)check, (uint8_t)(check >> 8),                            /* a call from elsewhere: not the write point */
                                0xc3, (uint8_t)main_start, (uint8_t)(main_start >> 8) };                /* jp mainThreadStart */
    memcpy(rom + 0x150, program, sizeof program);
    const uint8_t loop[] = { 0xcd, (uint8_t)check, (uint8_t)(check >> 8), 0x18, 0xfb };                 /* mainThreadStart: call check ; jr mainThreadStart */
    memcpy(rom + main_start, loop, sizeof loop);
    rom[check] = 0xc9;                                                                                  /* checkReloadStatusBarGraphics: ret */
    const OraclesCoreOptions options = { 0, 0, ORACLES_CORE_SAMEBOY, 0 };
    OraclesCore *core = oracles_core_create(rom, size, &options);
    CHECK(core != NULL);
    if (!core) { free(rom); return; }
    OraclesGuest *guest = oracles_guest_attach(core, original_profile(ORACLES_GAME_AGES));
    CHECK(guest != NULL);
    if (!guest) { free(rom); return; }
    oracles_guest_clear_hooks(guest);
    for (unsigned i = 0; i < 200; i++) oracles_core_run_frame(core);   /* past the boot ROM (about 130 frames) */
    /* A state where the player could open the inventory. */
    put8(guest, t->game_state, 2); put8(guest, t->cutscene_index, 1);
    put8(guest, (OraclesGuestSym){ 0, (uint16_t)(t->global_flags.addr + t->globalflag_intro_done / 8u) }, (uint8_t)(1u << (t->globalflag_intro_done % 8u)));
    put8(guest, t->inventory_b, 0x05); put8(guest, t->inventory_a, 0x01);
    put8(guest, t->inventory_storage, 0x0a); put8(guest, (OraclesGuestSym){ 0, (uint16_t)(t->inventory_storage.addr + 1u) }, 0x19);
    put8(guest, (OraclesGuestSym){ 0, (uint16_t)(t->obtained_treasure_flags.addr + t->treasure_ember_seeds / 8u) }, 0x03);   /* ember and scent seeds */
    put8(guest, t->status_bar_needs_refresh, 0x04);
    OraclesGuestInventoryState s;
    oracles_guest_inventory_state(guest, &s);
    CHECK(s.slots[0] == 0x05 && s.slots[2] == 0x0a && s.slots[3] == 0x19 && s.intro_done && s.obtained_seeds == 0x03 && s.harp_song != 0xff);

    CHECK(oracles_guest_set_inventory_policy(guest, once_policy, on_result, NULL, NULL) == 0);
    oracles_core_run_frame(core);
    CHECK(asked > 1 && results == 1 && last_verdict == ORACLES_INVENTORY_APPLIED);
    oracles_guest_inventory_state(guest, &s);
    CHECK(s.slots[0] == 0x0a && s.slots[2] == 0x05 && s.slots[1] == 0x01 && s.slots[3] == 0x19);       /* the exchange, nothing else */
    CHECK(s.satchel_seeds == 1 && s.status_bar_needs_refresh == 0x05);                                  /* the variant; bit 0 raised by an OR */
    unsigned applied = 0, violations = 0;
    oracles_guest_inventory_counts(guest, &applied, &violations);
    CHECK(applied == 1 && violations == 0);

    /* The replay of a route's exchange: applied at its frame when the slots hold what it noted, a mismatch otherwise. */
    oracles_guest_set_inventory_policy(guest, NULL, NULL, NULL, NULL);
    OraclesItemHotkeysEffect effects[2];
    memset(effects, 0, sizeof effects);
    effects[0].frame = 10; effects[0].op = swap(0, 2); effects[0].op.expect_a = 0x0a; effects[0].op.expect_b = 0x05;
    effects[1].frame = 11; effects[1].op = swap(0, 2); effects[1].op.expect_a = 0x0a; effects[1].op.expect_b = 0x05;   /* stale: B holds the sword again */
    OraclesItemHotkeysReplay *replay = oracles_item_hotkeys_replay_start(guest, effects, 2);
    CHECK(replay != NULL);
    oracles_guest_set_frame(guest, 9); oracles_core_run_frame(core);
    oracles_guest_inventory_state(guest, &s);
    CHECK(s.slots[0] == 0x0a);                                                                          /* not its frame yet */
    oracles_guest_set_frame(guest, 10); oracles_core_run_frame(core);
    oracles_guest_inventory_state(guest, &s);
    CHECK(s.slots[0] == 0x05 && s.slots[2] == 0x0a);
    oracles_guest_set_frame(guest, 11); oracles_core_run_frame(core);
    OraclesItemHotkeysStats stats;
    oracles_item_hotkeys_replay_stats(replay, &stats);
    CHECK(stats.applied == 1 && stats.replay_mismatch == 1 && stats.first_mismatch_frame == 11 && stats.first_mismatch_verdict == ORACLES_INVENTORY_REFUSED_EXPECTATION);
    oracles_guest_inventory_state(guest, &s);
    CHECK(s.slots[0] == 0x05 && s.slots[2] == 0x0a && oracles_guest_inventory_consistent(s.slots));
    oracles_item_hotkeys_replay_stop(replay);

    /* A ROM whose mainThreadStart calls the function twice has no single write point: the option is refused. */
    (void)gfx_events;
    oracles_guest_detach(guest);
    oracles_core_destroy(core);
    memcpy(rom + main_start + 5u, loop, 3u);
    core = oracles_core_create(rom, size, &options);
    guest = core ? oracles_guest_attach(core, original_profile(ORACLES_GAME_AGES)) : NULL;
    CHECK(guest != NULL);
    if (guest) { CHECK(oracles_guest_set_inventory_policy(guest, once_policy, on_result, NULL, NULL) == -1); oracles_guest_detach(guest); }
    if (core) oracles_core_destroy(core);
    free(rom);
}

int main(void)
{
    test_conditions(ORACLES_GAME_AGES);
    test_conditions(ORACLES_GAME_SEASONS);
    test_operations();
    test_transaction();
    if (failures) return 1;
    puts("test_item_hotkeys: ok");
    return 0;
}
