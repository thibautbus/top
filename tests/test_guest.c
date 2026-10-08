/* Guest bus, hooks and journal on a synthetic ROM (no game data):
 *   $0150: ld sp,$c200
 *   $0153: call $0200 ; ld a,$12 ; ldh ($43),a ; jr $0153      (an SCX write per loop)
 *   $0200: ld hl,$c000 ; inc (hl) ; ld a,1 ; ld ($2000),a ; call $4010 ; ret
 *   bank 1, $4010: ret
 * The entry hook of $0200 and the return hook at $0156 must fire once per
 * loop, and so must the entry hook of bank 1's $4010 (matched by bank); the
 * journal must see the SCX writes; the live WRAM hash must ignore the bytes
 * below the stack pointer and include the ones above it. */
#include "core.h"
#include "guest.h"
#include "guest_struct_offsets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

static const OraclesCompatProfile *fixture_profile(void)
{
    const OraclesRomInfo info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_AGES_US, .known = 1 };
    return oracles_compat_find(&info);
}

static unsigned entries, returns, vblanks, banked, saves, created, slot_failures_before;
static uint16_t last_return_pc;

/* The transition transaction: a policy that asks for everything it may.  It
 * moves Link a pixel on the first `link_moves_left` calls only: the synthetic
 * frame returns from the hooked subroutine hundreds of times, and Link
 * walking round the whole byte would meet any sprite entry on his way. */
static unsigned policy_calls, link_moves_left;
static void transaction_policy(void *opaque, const OraclesGuestTransitionState *s, OraclesGuestTransitionMutation *m)
{
    (void)opaque;
    policy_calls++;
    m->set_scroll_delta = 1; m->scroll_delta = s->transition_direction == 1u ? 8u : 0xf8u;
    if (link_moves_left) { m->link_x_delta = 0x100; link_moves_left--; }
    m->update_walk_animation = 1; m->link_anim_counter = 2; m->link_anim_parameter = 7; m->link_anim_pointer = 0x4200; m->link_animation_frame = 0x66;
}

static void put8(OraclesGuest *guest, OraclesGuestSym sym, uint8_t value)
{
    uint8_t *wram = oracles_guest_wram_writable(guest, sym.bank);
    CHECK(wram != NULL);
    if (wram) wram[sym.addr - (sym.bank ? 0xd000u : 0xc000u)] = value;
}

static void on_event(void *opaque, const OraclesGuestEvent *e)
{
    (void)opaque;
    switch (e->type) {
        case ORACLES_EVENT_TEXT: entries++; break;
        case ORACLES_EVENT_PART_CREATED: created++; break;
        case ORACLES_EVENT_WARP: returns++; last_return_pc = e->pc; break;
        case ORACLES_EVENT_VBLANK: vblanks++; break;
        case ORACLES_EVENT_MENU: banked++; break;
        case ORACLES_EVENT_SAVE: saves++; break;
        default: break;
    }
}

int main(void)
{
    const size_t size = 1024u * 1024u;
    uint8_t *rom = calloc(size, 1);
    memcpy(rom + 0x134, "ZELDA NAYRU", 11);
    rom[0x143] = 0xc0; rom[0x147] = 0x1b; rom[0x148] = 0x05; rom[0x149] = 0x02;
    rom[0x100] = 0x00; rom[0x101] = 0xc3; rom[0x102] = 0x50; rom[0x103] = 0x01;      /* nop ; jp $0150 */
    const uint8_t program[] = { 0x31, 0x00, 0xc2,        /* $0150: ld sp,$c200 */
                                0xcd, 0x00, 0x02,        /* $0153: call $0200 */
                                0x3e, 0x12,              /* $0156: ld a,$12 */
                                0xe0, 0x43,              /* $0158: ldh ($ff43),a : SCX */
                                0x18, 0xf7 };            /* $015a: jr $0153 */
    memcpy(rom + 0x150, program, sizeof program);
    const uint8_t sub[] = { 0x21, 0x00, 0xc0, 0x34,          /* $0200: ld hl,$c000 ; inc (hl) */
                            0x3e, 0x01, 0xea, 0x00, 0x20,    /* ld a,1 ; ld ($2000),a : ROM bank 1 */
                            0xcd, 0x10, 0x40,                /* call $4010 */
                            0xcd, 0x20, 0x02,                /* call $0220 : an allocator that finds a slot */
                            0xcd, 0x30, 0x02,                /* call $0230 : one that does not */
                            0xc9 };                          /* ret */
    memcpy(rom + 0x200, sub, sizeof sub);
    rom[0x4010] = 0xc9;                                      /* bank 1, $4010: ret */
    /* The slot allocators of the game answer Z when they found a slot: a
     * creation event fires on Z only, and a refusal is counted. */
    const uint8_t found[] = { 0xaf, 0xc9 };                  /* $0220: xor a (Z) ; ret */
    const uint8_t full[] = { 0x3e, 0x01, 0xb7, 0xc9 };       /* $0230: ld a,1 ; or a (NZ) ; ret */
    memcpy(rom + 0x220, found, sizeof found);
    memcpy(rom + 0x230, full, sizeof full);

    const OraclesCoreOptions options = { 0, 0, ORACLES_CORE_SAMEBOY, 0 };
    OraclesCore *core = oracles_core_create(rom, size, &options);
    free(rom);
    CHECK(core != NULL);
    if (!core) return 1;
    CHECK(oracles_guest_attach(core, NULL) == NULL);

    /* Ages tables for the bus; the hooks are replaced by the synthetic subroutine. */
    OraclesGuest *guest = oracles_guest_attach(core, fixture_profile());
    CHECK(guest != NULL);
    if (!guest) return 1;
    oracles_guest_set_event_sink(guest, on_event, NULL);
    oracles_guest_clear_hooks(guest);
    const OraclesGuestSym sub_entry = { 0, 0x0200 };
    CHECK(oracles_guest_add_hook(guest, sub_entry, ORACLES_EVENT_TEXT, ORACLES_EVENT_WARP) == 0);
    const OraclesGuestSym banked_entry = { 1, 0x4010 }, other_bank = { 2, 0x4010 };
    CHECK(oracles_guest_add_hook(guest, banked_entry, ORACLES_EVENT_MENU, 0) == 0);
    CHECK(oracles_guest_add_hook(guest, other_bank, ORACLES_EVENT_SAVE, 0) == 0);   /* bank 2 is never mapped: must not fire */
    const OraclesGuestSym slot_found = { 0, 0x0220 }, slot_full = { 0, 0x0230 };
    CHECK(oracles_guest_add_hook(guest, slot_found, 0, ORACLES_EVENT_PART_CREATED) == 0);
    CHECK(oracles_guest_add_hook(guest, slot_full, 0, ORACLES_EVENT_PART_CREATED) == 0);

    /* Let the boot ROM finish (about 130 frames), then observe one frame. */
    for (unsigned i = 0; i < 200; i++) oracles_core_run_frame(core);
    entries = returns = vblanks = banked = created = 0;
    {
        unsigned failures_before[3] = { 0, 0, 0 };
        oracles_guest_slot_failures(guest, failures_before);
        slot_failures_before = failures_before[2];
    }
    oracles_guest_journal_clear(guest);
    const uint8_t before = oracles_guest_wram(guest, 0)[0];
    oracles_guest_set_frame(guest, 200);
    oracles_core_run_frame(core);
    const uint8_t after = oracles_guest_wram(guest, 0)[0];

    CHECK(entries > 100 && returns > 100);
    CHECK(entries + 1 >= returns && returns + 1 >= entries);   /* a call and its return may fall on either side of the frame's edge */
    CHECK(banked == entries || banked == entries - 1 || banked == entries + 1);   /* the banked hook fires per call, by bank */
    CHECK(saves == 0);
    {
        unsigned overflow = 0, purged = 0;
        oracles_guest_dropped_returns(guest, &overflow, &purged);
        CHECK(overflow == 0);
    }
    CHECK(last_return_pc == 0x0156);
    {   /* one creation per call of the allocator that answers Z, none of the other, which is counted */
        unsigned slot_failures[3] = { 0, 0, 0 };
        oracles_guest_slot_failures(guest, slot_failures);
        const unsigned refused = slot_failures[2] - slot_failures_before;
        CHECK(created > 100);
        CHECK(created + 1 >= entries && entries + 1 >= created);
        CHECK(refused + 1 >= created && created + 1 >= refused);
        CHECK(slot_failures[0] == 0 && slot_failures[1] == 0);
    }
    CHECK(vblanks == 1);
    {   /* one increment per call, modulo 256; the frame may end between a call and its increment */
        const uint8_t delta = (uint8_t)(after - before);
        CHECK(delta == (uint8_t)entries || delta == (uint8_t)(entries - 1) || delta == (uint8_t)(entries + 1));
    }
    size_t count = 0;
    const OraclesGuestRegWrite *journal = oracles_guest_journal(guest, &count);
    CHECK(count > 100);
    CHECK(count > 0 && journal[0].reg == 0x43 && journal[0].value == 0x12);
    CHECK(count > 0 && journal[0].ly < 154);

    /* The bus reads by symbol and by object slot; an absent symbol has no pointer. */
    const OraclesGuestSym counter = { 0, 0xc000 };
    CHECK(oracles_guest_read8(guest, counter) == after);
    CHECK(oracles_guest_object(guest, 0, 1) == oracles_guest_wram(guest, 1) + 0x40);
    CHECK(oracles_guest_object(guest, 16, 0) == NULL);
    const OraclesGuestSym absent = { ORACLES_GUEST_ABSENT, 0 };
    CHECK(oracles_guest_ptr(guest, absent, 1) == NULL);

    /* Dead stack bytes: SP is $c200 (thread 1's stack, $c180-$c220): a byte
     * below it is dead, a byte at or above it is live. */
    const uint16_t sp = oracles_guest_sp(guest);
    CHECK(sp == 0xc200 || sp == 0xc1fe); /* $c1fe while inside the subroutine */
    uint8_t *wram = (uint8_t *)oracles_guest_wram(guest, 0);
    const uint64_t reference = oracles_guest_live_wram_hash(guest);
    wram[0xc1f0 - 0xc000] ^= 0xff;
    CHECK(oracles_guest_live_wram_hash(guest) == reference);
    wram[0xc1f0 - 0xc000] ^= 0xff;
    wram[0xc210 - 0xc000] ^= 0xff;
    CHECK(oracles_guest_live_wram_hash(guest) != reference);
    wram[0xc210 - 0xc000] ^= 0xff;
    CHECK(oracles_guest_live_wram_hash(guest) == reference);
    /* A saved thread context counts as live: thread 3's saved SP inside its stack. */
    const OraclesGuestSym thread3_sp = { 0, (uint16_t)(oracles_guest_tables(guest)->thread_state_buffer.addr + 3 * 8 + 2) };
    uint8_t *saved = (uint8_t *)oracles_guest_ptr(guest, thread3_sp, 2);
    saved[0] = 0xb0; saved[1] = 0xc2;                    /* SP = $c2b0 in [$c270, $c2c0) */
    const uint64_t with_thread3 = oracles_guest_live_wram_hash(guest);
    wram[0xc2b4 - 0xc000] ^= 0xff;                       /* above the saved SP: live */
    CHECK(oracles_guest_live_wram_hash(guest) != with_thread3);
    wram[0xc2b4 - 0xc000] ^= 0xff;
    wram[0xc280 - 0xc000] ^= 0xff;                       /* below it: dead */
    CHECK(oracles_guest_live_wram_hash(guest) == with_thread3);
    wram[0xc280 - 0xc000] ^= 0xff;

    /* The transition transaction: at the return of the hooked
     * subroutine, the policy's requests land on the scroll step, Link's x and
     * his animation fields, and nowhere else; in a state that is not a
     * scrolling transition, on nothing. */
    {
        const OraclesGuestTables *t = oracles_guest_tables(guest);
        oracles_guest_clear_hooks(guest);
        CHECK(oracles_guest_add_hook(guest, sub_entry, ORACLES_EVENT_TEXT, ORACLES_EVENT_FRAME_DRAWN) == 0);
        put8(guest, t->active_group, 0); put8(guest, t->room_is_large, 0); put8(guest, t->game_state, 2); put8(guest, t->cutscene_index, 1);
        put8(guest, t->text_is_active, 0); put8(guest, t->tileset_flags, 0); put8(guest, t->scroll_mode, 8);
        put8(guest, t->screen_transition_state, 5); put8(guest, t->screen_transition_substate, 2); put8(guest, t->screen_transition_phase, 2);
        put8(guest, t->screen_transition_direction, 1); put8(guest, t->screen_scroll_delta, 4); put8(guest, t->screen_scroll_counter, 19);
        put8(guest, t->link_force_state, 0); put8(guest, t->link_object_index, 0xd0);
        put8(guest, t->link_id, 0); put8(guest, (OraclesGuestSym){ 1, 0xd004 }, 1); put8(guest, t->link_visible, 0x80); put8(guest, t->link_anim_mode, 0x10);
        put8(guest, t->link_in_air, 0); put8(guest, t->link_swimming_state, 0); put8(guest, t->link_grab_state, 0); put8(guest, t->magnet_glove_state, 0);
        put8(guest, t->link_immobilized, 0); put8(guest, t->link_playing_instrument, 0); put8(guest, t->link_turning_disabled, 0);
        put8(guest, t->force_link_push_animation, 0x80); put8(guest, t->using_shield, 0);
        put8(guest, t->pegasus_seed_counter, 0); put8(guest, (OraclesGuestSym){ 0, (uint16_t)(t->pegasus_seed_counter.addr + 1) }, 0);
        put8(guest, t->link_knockback_counter, 0); put8(guest, t->link_stun_counter, 0);
        put8(guest, t->parent_item2_enabled, 0); put8(guest, t->parent_item3_enabled, 0); put8(guest, t->parent_item4_enabled, 0);
        put8(guest, t->parent_item5_enabled, 0); put8(guest, t->weapon_item_enabled, 0);
        put8(guest, (OraclesGuestSym){ 1, 0xd00c }, 0); put8(guest, (OraclesGuestSym){ 1, 0xd00d }, 227);   /* x = 227.0: a hundred past the camera's (HRAM reads 0x7f here) */
        put8(guest, (OraclesGuestSym){ 1, 0xd010 }, 0x55);                                                 /* an unrelated field: Link's speed */
        put8(guest, t->link_anim_counter, 9);
        /* Link's sprite in wOam: his two columns at his place (line 24 for y 0, x 100 and 108), and an unrelated entry. */
        const uint8_t oam_line = (uint8_t)(0 - oracles_guest_read16(guest, t->camera_y) + 24), oam_x = (uint8_t)(227 - oracles_guest_read16(guest, t->camera_x));
        put8(guest, (OraclesGuestSym){ 1, 0xd00a }, 0); put8(guest, (OraclesGuestSym){ 1, 0xd00b }, 0);
        put8(guest, (OraclesGuestSym){ 0, 0xcb10 }, oam_line); put8(guest, (OraclesGuestSym){ 0, 0xcb11 }, oam_x); put8(guest, (OraclesGuestSym){ 0, 0xcb12 }, 0x10); put8(guest, (OraclesGuestSym){ 0, 0xcb13 }, 0);
        put8(guest, (OraclesGuestSym){ 0, 0xcb14 }, oam_line); put8(guest, (OraclesGuestSym){ 0, 0xcb15 }, (uint8_t)(oam_x + 8)); put8(guest, (OraclesGuestSym){ 0, 0xcb16 }, 0x12); put8(guest, (OraclesGuestSym){ 0, 0xcb17 }, 0);
        put8(guest, (OraclesGuestSym){ 0, 0xcb18 }, oam_line); put8(guest, (OraclesGuestSym){ 0, 0xcb19 }, (uint8_t)(oam_x - 40)); put8(guest, (OraclesGuestSym){ 0, 0xcb1a }, 0x20); put8(guest, (OraclesGuestSym){ 0, 0xcb1b }, 0);
        oracles_guest_set_transition_policy(guest, transaction_policy, NULL, NULL);
        policy_calls = 0; link_moves_left = 20;
        oracles_core_run_frame(core);
        CHECK(policy_calls > 50);
        const uint8_t *w1 = oracles_guest_wram(guest, 1);
        const uint8_t *w0 = oracles_guest_wram(guest, 0);
        CHECK(oracles_guest_read8(guest, t->screen_scroll_delta) == 8);
        CHECK(w1[0x0d] == 247);                                                                 /* a pixel per call, twenty calls */
        CHECK(w0[0xb11] == (uint8_t)(oam_x + 20) && w0[0xb15] == (uint8_t)(oam_x + 28));       /* his sprite followed */
        CHECK(w0[0xb10] == oam_line && w0[0xb19] == (uint8_t)(oam_x - 40));                                       /* the rest did not */
        CHECK(w1[0x10] == 0x55);                                               /* untouched */
        CHECK(oracles_guest_read8(guest, t->link_anim_counter) == 2 && oracles_guest_read8(guest, t->link_anim_parameter) == 7);
        CHECK(oracles_guest_read16(guest, t->link_anim_pointer) == 0x4200 && oracles_guest_read8(guest, t->link_animation_frame) == 0x66);
        /* Not a transition: nothing is written. */
        put8(guest, t->screen_transition_state, 2); put8(guest, t->scroll_mode, 1); put8(guest, t->screen_scroll_delta, 4);
        put8(guest, (OraclesGuestSym){ 1, 0xd00d }, 100); put8(guest, t->link_anim_counter, 9);
        link_moves_left = 20;
        oracles_core_run_frame(core);
        CHECK(oracles_guest_read8(guest, t->screen_scroll_delta) == 4);
        CHECK(oracles_guest_wram(guest, 1)[0x0d] == 100);
        CHECK(oracles_guest_read8(guest, t->link_anim_counter) == 9);
        /* In a transition but with an item out: the step may change, Link may not move. */
        put8(guest, t->screen_transition_state, 5); put8(guest, t->scroll_mode, 8); put8(guest, t->weapon_item_enabled, 1);
        link_moves_left = 20;
        oracles_core_run_frame(core);
        CHECK(oracles_guest_read8(guest, t->screen_scroll_delta) == 8);
        CHECK(oracles_guest_wram(guest, 1)[0x0d] == 100);
        /* A frame the game does not draw (no hooked return) while the
         * transition loads the room (state 3): the transaction runs once at
         * the vblank, Link and his sprite entries move a pixel. */
        oracles_guest_clear_hooks(guest);
        put8(guest, t->weapon_item_enabled, 0);
        put8(guest, t->screen_transition_state, 3);
        put8(guest, (OraclesGuestSym){ 1, 0xd00d }, 227);
        put8(guest, (OraclesGuestSym){ 0, 0xcb11 }, oam_x); put8(guest, (OraclesGuestSym){ 0, 0xcb15 }, (uint8_t)(oam_x + 8));
        link_moves_left = 20; policy_calls = 0;
        oracles_core_run_frame(core);
        CHECK(policy_calls == 1);
        CHECK(oracles_guest_wram(guest, 1)[0x0d] == 228);
        CHECK(w0[0xb11] == (uint8_t)(oam_x + 1) && w0[0xb15] == (uint8_t)(oam_x + 9) && w0[0xb19] == (uint8_t)(oam_x - 40));
        /* The same unfinished frame during the scroll itself (state 5): the
         * vblank is not a write point there; the policy is not even asked,
         * so that it does not take for done what would not be written. */
        put8(guest, t->screen_transition_state, 5);
        put8(guest, (OraclesGuestSym){ 1, 0xd00d }, 227);
        link_moves_left = 20; policy_calls = 0;
        const unsigned skipped_before = oracles_guest_vblank_policy_skipped(guest);
        oracles_core_run_frame(core);
        CHECK(policy_calls == 0);
        CHECK(oracles_guest_vblank_policy_skipped(guest) == skipped_before + 1u);
        CHECK(oracles_guest_wram(guest, 1)[0x0d] == 227);
        unsigned by_state[8];
        oracles_guest_vblank_write_counts(guest, by_state);
        CHECK(by_state[3] == 1 && by_state[5] == 0 && by_state[2] == 0);
        oracles_guest_set_transition_policy(guest, NULL, NULL, NULL);
    }

    oracles_guest_detach(guest);
    oracles_core_destroy(core);
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_guest: ok\n");
    return 0;
}
