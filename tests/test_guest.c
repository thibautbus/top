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
static int collisions_given;     /* the room entered's collisions, at the last call */
static void transaction_policy(void *opaque, const OraclesGuestTransitionState *s, OraclesGuestTransitionMutation *m)
{
    (void)opaque;
    policy_calls++;
    collisions_given = s->room_collisions != NULL;
    m->set_scroll_delta = 1; m->scroll_delta = s->transition_direction == 1u ? 8u : 0xf8u;
    if (link_moves_left) { m->link_x_delta = 0x100; link_moves_left--; }
    m->update_walk_animation = 1; m->link_anim_counter = 2; m->link_anim_parameter = 7; m->link_anim_pointer = 0x4200; m->link_animation_frame = 0x66;
}

/* The state the policy was last given. */
static OraclesGuestTransitionState last_state;
static void capture_policy(void *opaque, const OraclesGuestTransitionState *s, OraclesGuestTransitionMutation *m)
{
    (void)opaque; (void)m;
    policy_calls++;
    last_state = *s;
}

/* A policy that asks nothing: the frame's step is the game's own. */
static void still_policy(void *opaque, const OraclesGuestTransitionState *s, OraclesGuestTransitionMutation *m)
{
    (void)opaque; (void)s; (void)m;
    policy_calls++;
}

/* The step of the frame that ends a scroll (--continuous-steps): a pixel at each call in normal play, and Link's respawn
 * in the room entered at (9, 80), or at (130, 80), below the room's bottom edge. */
static int respawn_below_edge;
static void scroll_end_policy(void *opaque, const OraclesGuestTransitionState *s, OraclesGuestTransitionMutation *m)
{
    (void)opaque;
    policy_calls++;
    if (s->transition_state == 2u) {
        m->scroll_end = 1; m->link_x_delta = 0x100;
        m->set_respawn = 1; m->respawn_y = respawn_below_edge ? 130u : 9u; m->respawn_x = 80u;
    }
}

/* The transitions at the line: at the frame that ends a scroll, the transition back toward the left asked. */
static void back_policy(void *opaque, const OraclesGuestTransitionState *s, OraclesGuestTransitionMutation *m)
{
    (void)opaque;
    policy_calls++;
    if (s->transition_state == 2u) { m->force_transition = 1; m->forced_direction = 3; }
}

/* The transitions at the line: at the frame that ends a scroll, the scroll up into the room beside started there. */
static void cross_policy(void *opaque, const OraclesGuestTransitionState *s, OraclesGuestTransitionMutation *m)
{
    (void)opaque;
    policy_calls++;
    if (s->transition_state == 2u && s->scroll_mode == 1u) { m->force_transition = 3; m->forced_direction = 0; }
}

/* screenTransitionState2 from its start to @transition's Ages-only checks, as the US image holds it at $01:4105: the
 * four edge tests the line arms on. */
static const uint8_t AGES_STATE2[] = {
    0xfa, 0x5c, 0xcc, 0x87, 0x38, 0x07, 0x28, 0x05, 0x3e, 0x04, 0xea, 0x15, 0xcd, 0xfa, 0x02, 0xcd,
    0xcb, 0x7f, 0x28, 0x06, 0xe6, 0x7f, 0x4f, 0xc3, 0xd6, 0x41, 0xfa, 0x2c, 0xcc, 0x67, 0x2e, 0x0b,
    0x3e, 0x05, 0xbe, 0x30, 0x14, 0xfa, 0x0d, 0xcd, 0xbe, 0x38, 0x19, 0x2e, 0x0d, 0x3e, 0x05, 0xbe,
    0x30, 0x1c, 0xfa, 0x0c, 0xcd, 0xbe, 0x38, 0x1e, 0xc9, 0x3c, 0x77, 0x06, 0x40, 0x0e, 0x00, 0xcd,
    0x60, 0x41, 0x18, 0xe7, 0x77, 0x06, 0x80, 0x0e, 0x02, 0xcd, 0x60, 0x41, 0x18, 0xdd, 0x3c, 0x77,
    0x06, 0x20, 0x0e, 0x03, 0x18, 0x05, 0x77, 0x06, 0x10, 0x0e, 0x01, 0xfa, 0x00, 0xd0, 0xb7, 0xc8,
    0xfa, 0x91, 0xcc, 0xb7, 0xc0, 0xfa, 0x15, 0xcd, 0xb7, 0x28, 0x05, 0x3d, 0xea, 0x15, 0xcd, 0xc9,
    0xfa, 0x01, 0xd1, 0xfe, 0x0a, 0x28, 0x5a, 0xfa, 0x92, 0xcc, 0x87, 0xd8, 0xfa, 0x5c, 0xcc, 0x87,
    0x38, 0x4f, 0xfa, 0x2d, 0xd0, 0xb7, 0xc0, 0xfa, 0x92, 0xcc, 0x87, 0x20, 0x05, 0xcd, 0x02, 0x2a,
    0xa0, 0xc8, 0xfa, 0x34, 0xcc, 0xe6, 0x01, 0x28, 0x20, 0xfa, 0x30, 0xcc, 0x5f, 0xe6, 0x0f, 0xfe,
    0x0d, 0x20, 0x04, 0x79, 0xfe, 0x01, 0xc8, 0x7b, 0xfe, 0xd0, 0x38, 0x04, 0x79, 0xfe, 0x02, 0xc8,
};

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
    memcpy(rom + 0x4105, AGES_STATE2, sizeof AGES_STATE2);

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
        /* Link's sprite in wOam: his two columns at his place (line 24 for y 0, x 100 and 108), his graphics' tiles, and an unrelated entry. */
        const uint8_t oam_line = (uint8_t)(0 - oracles_guest_read16(guest, t->camera_y) + 24), oam_x = (uint8_t)(227 - oracles_guest_read16(guest, t->camera_x));
        put8(guest, (OraclesGuestSym){ 1, 0xd00a }, 0); put8(guest, (OraclesGuestSym){ 1, 0xd00b }, 0);
        put8(guest, (OraclesGuestSym){ 0, 0xcb10 }, oam_line); put8(guest, (OraclesGuestSym){ 0, 0xcb11 }, oam_x); put8(guest, (OraclesGuestSym){ 0, 0xcb12 }, 0x70); put8(guest, (OraclesGuestSym){ 0, 0xcb13 }, 0x08);
        put8(guest, (OraclesGuestSym){ 0, 0xcb14 }, oam_line); put8(guest, (OraclesGuestSym){ 0, 0xcb15 }, (uint8_t)(oam_x + 8)); put8(guest, (OraclesGuestSym){ 0, 0xcb16 }, 0x72); put8(guest, (OraclesGuestSym){ 0, 0xcb17 }, 0x08);
        put8(guest, (OraclesGuestSym){ 0, 0xcb18 }, oam_line); put8(guest, (OraclesGuestSym){ 0, 0xcb19 }, (uint8_t)(oam_x - 40)); put8(guest, (OraclesGuestSym){ 0, 0xcb1a }, 0x20); put8(guest, (OraclesGuestSym){ 0, 0xcb1b }, 0);
        /* A status bar's icon under its lines, where a carry past the room's top takes him: not his, and his still follow. */
        put8(guest, (OraclesGuestSym){ 0, 0xcb1c }, oam_line); put8(guest, (OraclesGuestSym){ 0, 0xcb1d }, (uint8_t)(oam_x + 6)); put8(guest, (OraclesGuestSym){ 0, 0xcb1e }, 0x7c); put8(guest, (OraclesGuestSym){ 0, 0xcb1f }, 0x0c);
        oracles_guest_set_transition_policy(guest, transaction_policy, NULL, NULL);
        policy_calls = 0; link_moves_left = 20;
        oracles_core_run_frame(core);
        CHECK(policy_calls > 50);
        CHECK(collisions_given);                                                                /* scroll mode 8, the frame drawn */
        const uint8_t *w1 = oracles_guest_wram(guest, 1);
        const uint8_t *w0 = oracles_guest_wram(guest, 0);
        CHECK(oracles_guest_read8(guest, t->screen_scroll_delta) == 8);
        CHECK(w1[0x0d] == 247);                                                                 /* a pixel per call, twenty calls */
        CHECK(w0[0xb11] == (uint8_t)(oam_x + 20) && w0[0xb15] == (uint8_t)(oam_x + 28));       /* his sprite followed */
        CHECK(w0[0xb10] == oam_line && w0[0xb19] == (uint8_t)(oam_x - 40) && w0[0xb1d] == (uint8_t)(oam_x + 6));   /* the rest did not */
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
        /* Back in normal play right after the scroll (state 5 at the last call), with the steps: the step of the frame
         * that ends it is written once, at the first entry, the policy asked there and not at the return that follows;
         * the calls after it see normal play, where nothing is.  Without the steps, nothing either. */
        put8(guest, t->weapon_item_enabled, 0);
        put8(guest, t->screen_transition_state, 2); put8(guest, t->scroll_mode, 1);
        put8(guest, t->screen_transition_boundary_x, 154); put8(guest, t->screen_transition_boundary_y, 121);
        put8(guest, t->link_local_respawn, 40); put8(guest, (OraclesGuestSym){ 0, (uint16_t)(t->link_local_respawn.addr + 1u) }, 41);
        oracles_guest_set_transition_policy(guest, scroll_end_policy, NULL, NULL);
        oracles_guest_clear_hooks(guest);
        CHECK(oracles_guest_add_hook(guest, sub_entry, ORACLES_EVENT_FRAME_DONE, ORACLES_EVENT_FRAME_DRAWN) == 0);
        policy_calls = 0;
        oracles_core_run_frame(core);
        CHECK(policy_calls > 50 && oracles_guest_wram(guest, 1)[0x0d] == 100);
        CHECK(oracles_guest_read8(guest, t->link_local_respawn) == 40);
        put8(guest, t->screen_transition_state, 5); put8(guest, t->scroll_mode, 8);
        oracles_guest_set_transition_steps(guest, 1);
        oracles_core_run_frame(core);
        oracles_guest_clear_hooks(guest);   /* the returns the frame left pending: the next frame starts at an entry */
        CHECK(oracles_guest_add_hook(guest, sub_entry, ORACLES_EVENT_FRAME_DONE, ORACLES_EVENT_FRAME_DRAWN) == 0);
        put8(guest, t->screen_transition_state, 2); put8(guest, t->scroll_mode, 1);
        put8(guest, (OraclesGuestSym){ 1, 0xd00d }, 100);
        policy_calls = 0;
        oracles_core_run_frame(core);
        CHECK(policy_calls > 50 && oracles_guest_wram(guest, 1)[0x0d] == 101);
        CHECK(oracles_guest_read16(guest, t->link_local_respawn) == (80u << 8 | 9u) && oracles_guest_transition_respawns(guest) == 1);
        oracles_core_run_frame(core);
        CHECK(oracles_guest_wram(guest, 1)[0x0d] == 101);
        /* A respawn past the edges the game starts a transition at is not written. */
        put8(guest, t->screen_transition_state, 5); put8(guest, t->scroll_mode, 8);
        oracles_core_run_frame(core);
        oracles_guest_clear_hooks(guest);
        CHECK(oracles_guest_add_hook(guest, sub_entry, ORACLES_EVENT_FRAME_DONE, ORACLES_EVENT_FRAME_DRAWN) == 0);
        put8(guest, t->screen_transition_state, 2); put8(guest, t->scroll_mode, 1);
        put8(guest, t->link_local_respawn, 40);
        respawn_below_edge = 1;
        oracles_core_run_frame(core);
        CHECK(oracles_guest_wram(guest, 1)[0x0d] == 102 && oracles_guest_read8(guest, t->link_local_respawn) == 40);
        respawn_below_edge = 0;
        /* The transitions at the line: Link walked back past the line (x=230.25, 26 px into the room left, to the
         * left), the transition back asked at the frame that ends the scroll.  A script has disabled the transitions:
         * not raised, and he is put back on the line, x=0, his y untouched.  Enabled: raised from where he stands. */
        CHECK(oracles_guest_set_transition_line(guest, 1) == 0);
        oracles_guest_set_transition_policy(guest, back_policy, NULL, NULL);
        put8(guest, t->room_width, 20); put8(guest, t->room_height, 16); put8(guest, t->room_is_large, 0);
        put8(guest, (OraclesGuestSym){ 1, 0xd000 }, 1);
        for (int enabled = 0; enabled < 2; enabled++) {
            put8(guest, t->screen_transition_state, 5); put8(guest, t->scroll_mode, 8);
            oracles_core_run_frame(core);
            oracles_guest_clear_hooks(guest);
            CHECK(oracles_guest_add_hook(guest, sub_entry, ORACLES_EVENT_FRAME_DONE, ORACLES_EVENT_FRAME_DRAWN) == 0);
            put8(guest, t->screen_transition_state, 2); put8(guest, t->scroll_mode, 1); put8(guest, t->screen_transition_direction, 1);
            put8(guest, (OraclesGuestSym){ 1, 0xd00c }, 0x40); put8(guest, (OraclesGuestSym){ 1, 0xd00d }, 230);
            put8(guest, (OraclesGuestSym){ 1, 0xd00a }, 0x80); put8(guest, (OraclesGuestSym){ 1, 0xd00b }, 60);
            put8(guest, t->disable_screen_transitions, enabled ? 0 : 1);
            const unsigned put_back = oracles_guest_transition_put_back(guest), forced = oracles_guest_transition_back_forced(guest);
            oracles_core_run_frame(core);
            const uint8_t *link = oracles_guest_wram(guest, 1);
            const uint8_t direction = oracles_guest_read8(guest, t->screen_transition_direction);
            if (!enabled) CHECK(link[0x0d] == 0 && link[0x0c] == 0 && direction == 1 && oracles_guest_transition_put_back(guest) == put_back + 1);
            else CHECK(link[0x0d] == 230 && link[0x0c] == 0x40 && direction == 0x83 && oracles_guest_transition_back_forced(guest) == forced + 1
                       && oracles_guest_transition_put_back(guest) == put_back);
            CHECK(link[0x0b] == 60 && link[0x0a] == 0x80);
        }
        /* Carried past the line across into the room beside (y=244.5, 12 px past the top), the scroll up into it
         * asked at the frame that ends the scroll: the transitions disabled, not started, and he is put back on the
         * line, y=0, his x untouched; enabled, started there as @startTransition starts one, from where he stands. */
        oracles_guest_set_transition_policy(guest, cross_policy, NULL, NULL);
        for (int enabled = 0; enabled < 2; enabled++) {
            put8(guest, t->screen_transition_state, 5); put8(guest, t->scroll_mode, 8);
            oracles_core_run_frame(core);
            oracles_guest_clear_hooks(guest);
            CHECK(oracles_guest_add_hook(guest, sub_entry, ORACLES_EVENT_FRAME_DONE, ORACLES_EVENT_FRAME_DRAWN) == 0);
            put8(guest, t->screen_transition_state, 2); put8(guest, t->scroll_mode, 1); put8(guest, t->screen_transition_direction, 1);
            put8(guest, (OraclesGuestSym){ 1, 0xd00a }, 0x80); put8(guest, (OraclesGuestSym){ 1, 0xd00b }, 244);
            put8(guest, (OraclesGuestSym){ 1, 0xd00c }, 0x40); put8(guest, (OraclesGuestSym){ 1, 0xd00d }, 30);
            put8(guest, t->disable_screen_transitions, enabled ? 0 : 1);
            put8(guest, t->disable_warp_tiles, 0);
            const unsigned put_back = oracles_guest_transition_put_back(guest), started = oracles_guest_transition_cross_started(guest);
            oracles_core_run_frame(core);
            const uint8_t *link = oracles_guest_wram(guest, 1);
            const uint8_t direction = oracles_guest_read8(guest, t->screen_transition_direction);
            const uint8_t state = oracles_guest_read8(guest, t->screen_transition_state), mode = oracles_guest_read8(guest, t->scroll_mode);
            if (!enabled) CHECK(link[0x0b] == 0 && link[0x0a] == 0 && direction == 1 && state == 2 && mode == 1 && oracles_guest_transition_put_back(guest) == put_back + 1);
            else CHECK(link[0x0b] == 244 && link[0x0a] == 0x80 && direction == 0 && state == 3 && mode == 4
                       && oracles_guest_transition_cross_started(guest) == started + 1 && oracles_guest_transition_put_back(guest) == put_back);
            CHECK(link[0x0d] == 30 && link[0x0c] == 0x40);
            /* The warp tiles held off for the frame after the scroll started (its warps would read Link's tile in the
             * scripts' variables), given back once the room it enters is named. */
            CHECK(oracles_guest_read8(guest, t->disable_warp_tiles) == (enabled ? 0x80 : 0));
        }
        put8(guest, t->scroll_mode, 4);
        oracles_core_run_frame(core);
        CHECK(oracles_guest_read8(guest, t->disable_warp_tiles) == 0x80);
        put8(guest, t->scroll_mode, 8);
        oracles_core_run_frame(core);
        CHECK(oracles_guest_read8(guest, t->disable_warp_tiles) == 0);
        /* The game's own hold (a companion just left) is left as it is. */
        put8(guest, t->disable_warp_tiles, 1);
        put8(guest, t->screen_transition_state, 5); put8(guest, t->scroll_mode, 8);
        oracles_core_run_frame(core);
        oracles_guest_clear_hooks(guest);
        CHECK(oracles_guest_add_hook(guest, sub_entry, ORACLES_EVENT_FRAME_DONE, ORACLES_EVENT_FRAME_DRAWN) == 0);
        put8(guest, t->screen_transition_state, 2); put8(guest, t->scroll_mode, 1); put8(guest, t->screen_transition_direction, 1);
        put8(guest, (OraclesGuestSym){ 1, 0xd00a }, 0x80); put8(guest, (OraclesGuestSym){ 1, 0xd00b }, 244);
        oracles_core_run_frame(core);
        CHECK(oracles_guest_read8(guest, t->screen_transition_state) == 3 && oracles_guest_read8(guest, t->disable_warp_tiles) == 1);
        put8(guest, t->scroll_mode, 8);
        oracles_core_run_frame(core);
        CHECK(oracles_guest_read8(guest, t->disable_warp_tiles) == 1);
        put8(guest, t->disable_warp_tiles, 0);
        put8(guest, t->disable_screen_transitions, 0);
        put8(guest, (OraclesGuestSym){ 1, 0xd00a }, 0); put8(guest, (OraclesGuestSym){ 1, 0xd00b }, 0); put8(guest, (OraclesGuestSym){ 1, 0xd00c }, 0);
        oracles_guest_set_transition_line(guest, 0);
        oracles_guest_set_transition_policy(guest, scroll_end_policy, NULL, NULL);
        oracles_guest_set_transition_steps(guest, 0);
        oracles_guest_clear_hooks(guest);
        CHECK(oracles_guest_add_hook(guest, sub_entry, ORACLES_EVENT_TEXT, ORACLES_EVENT_FRAME_DRAWN) == 0);
        oracles_guest_set_transition_policy(guest, transaction_policy, NULL, NULL);
        put8(guest, t->screen_transition_state, 5); put8(guest, t->scroll_mode, 8);
        put8(guest, (OraclesGuestSym){ 1, 0xd00d }, 100);
        /* A frame the game does not draw (no hooked return) while the
         * transition loads the room (state 3): the transaction runs once at
         * the vblank, Link moves a pixel, and his sprite entries go where he
         * now is, the game's own last step with it (226 drawn, the OAM copied
         * at the vblank before; 227 when the load began, 228 after). */
        oracles_guest_clear_hooks(guest);
        put8(guest, t->weapon_item_enabled, 0);
        put8(guest, t->screen_transition_state, 2); put8(guest, t->vblank_checker, 0xff);
        put8(guest, (OraclesGuestSym){ 1, 0xd00d }, 226);
        oracles_core_run_frame(core);
        put8(guest, t->screen_transition_state, 3); put8(guest, t->vblank_checker, 0);
        put8(guest, (OraclesGuestSym){ 1, 0xd00d }, 227);
        put8(guest, (OraclesGuestSym){ 0, 0xcb11 }, (uint8_t)(oam_x - 1)); put8(guest, (OraclesGuestSym){ 0, 0xcb15 }, (uint8_t)(oam_x + 7));
        link_moves_left = 20; policy_calls = 0;
        oracles_core_run_frame(core);
        CHECK(policy_calls == 1);
        CHECK(!collisions_given);       /* scroll mode 8 is written before the load: at its vblanks they may be the room left's */
        CHECK(oracles_guest_wram(guest, 1)[0x0d] == 228);
        CHECK(w0[0xb11] == (uint8_t)(oam_x + 1) && w0[0xb15] == (uint8_t)(oam_x + 9) && w0[0xb19] == (uint8_t)(oam_x - 40));
        {
            uint16_t shown_x = 0;
            oracles_core_run_frame(core);   /* the scan of the next frame draws him there */
            CHECK(oracles_guest_link_shown(guest, &shown_x, NULL) && shown_x >> 8 == 228u);
        }
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
        CHECK(by_state[3] == 2 && by_state[5] == 0 && by_state[2] == 0);
        /* Swimming, the frame that starts a transition, unfinished at its vblank, no move asked: the game's own step (226
         * drawn, 228 now) takes his sprite with it, his graphics' entries alone, a splash at his side left where it is. */
        oracles_guest_set_transition_swim(guest, 1);
        put8(guest, t->link_swimming_state, 3); put8(guest, t->link_anim_mode, 0x0b);
        put8(guest, t->screen_transition_state, 2); put8(guest, t->vblank_checker, 0xff);
        put8(guest, (OraclesGuestSym){ 1, 0xd00d }, 226);
        oracles_guest_set_transition_policy(guest, still_policy, NULL, NULL);
        oracles_core_run_frame(core);
        put8(guest, t->screen_transition_state, 3); put8(guest, t->vblank_checker, 0);
        put8(guest, (OraclesGuestSym){ 1, 0xd00d }, 228);
        put8(guest, (OraclesGuestSym){ 0, 0xcb11 }, (uint8_t)(oam_x - 1)); put8(guest, (OraclesGuestSym){ 0, 0xcb15 }, (uint8_t)(oam_x + 7));
        put8(guest, (OraclesGuestSym){ 0, 0xcb18 }, oam_line); put8(guest, (OraclesGuestSym){ 0, 0xcb19 }, (uint8_t)(oam_x + 2));   /* the splash */
        policy_calls = 0;
        oracles_core_run_frame(core);
        CHECK(policy_calls == 1);
        CHECK(oracles_guest_wram(guest, 1)[0x0d] == 228);
        CHECK(w0[0xb11] == (uint8_t)(oam_x + 1) && w0[0xb15] == (uint8_t)(oam_x + 9) && w0[0xb19] == (uint8_t)(oam_x + 2));
        {
            uint16_t shown_x = 0;
            oracles_core_run_frame(core);
            CHECK(oracles_guest_link_shown(guest, &shown_x, NULL) && shown_x >> 8 == 228u);
        }
        oracles_guest_set_transition_swim(guest, 0);
        put8(guest, t->link_swimming_state, 0); put8(guest, t->link_anim_mode, 0x10);
        /* Walking, the same frame: his sprite goes where the game's step took him in wOam and in the core's OAM, which
         * the next scan reads; an item's sprite beside him stays where it is. */
        {
            size_t oam_size = 0;
            uint16_t oam_bank = 0;
            uint8_t *oam = oracles_core_memory(core, ORACLES_CORE_OAM, &oam_size, &oam_bank);
            CHECK(oam != NULL && oam_size >= 160u);
            put8(guest, t->screen_transition_state, 2); put8(guest, t->vblank_checker, 0xff);
            put8(guest, (OraclesGuestSym){ 1, 0xd00d }, 226);
            oracles_core_run_frame(core);
            put8(guest, t->screen_transition_state, 3); put8(guest, t->vblank_checker, 0);
            put8(guest, (OraclesGuestSym){ 1, 0xd00d }, 228);
            put8(guest, (OraclesGuestSym){ 0, 0xcb11 }, (uint8_t)(oam_x - 1)); put8(guest, (OraclesGuestSym){ 0, 0xcb15 }, (uint8_t)(oam_x + 7));
            put8(guest, (OraclesGuestSym){ 0, 0xcb18 }, oam_line); put8(guest, (OraclesGuestSym){ 0, 0xcb19 }, (uint8_t)(oam_x + 2));
            if (oam) {
                memcpy(oam + 0x10, w0 + 0xb10, 16);
                oam[0x11] = (uint8_t)(oam_x - 1); oam[0x15] = (uint8_t)(oam_x + 7); oam[0x18] = oam_line; oam[0x19] = (uint8_t)(oam_x + 2);
            }
            policy_calls = 0;
            oracles_core_run_frame(core);
            CHECK(policy_calls == 1);
            CHECK(oracles_guest_wram(guest, 1)[0x0d] == 228);
            CHECK(w0[0xb11] == (uint8_t)(oam_x + 1) && w0[0xb15] == (uint8_t)(oam_x + 9) && w0[0xb19] == (uint8_t)(oam_x + 2) && w0[0xb1d] == (uint8_t)(oam_x + 6));
            CHECK(oam && oam[0x11] == (uint8_t)(oam_x + 1) && oam[0x15] == (uint8_t)(oam_x + 9) && oam[0x19] == (uint8_t)(oam_x + 2) && oam[0x1d] == (uint8_t)(oam_x + 6));
            uint16_t shown_x = 0;
            oracles_core_run_frame(core);
            CHECK(oracles_guest_link_shown(guest, &shown_x, NULL) && shown_x >> 8 == 228u);
        }
        /* The bytes the rooms' own code reads, as the state gives them to the prediction: hGameboyType in the HRAM, the
         * pages of room flags from $c700 (the vines' positions at +$1f0, wGroup4RoomFlags+$f9 at +$2f9), the treasures
         * obtained, each byte the image has marked given; the original image's room code replayed. */
        {
            size_t hram_size = 0;
            uint16_t hram_bank = 0;
            uint8_t *hram = oracles_core_memory(core, ORACLES_CORE_HRAM, &hram_size, &hram_bank);
            CHECK(hram != NULL && t->gameboy_type.addr >= 0xff80u);
            if (hram) hram[t->gameboy_type.addr - 0xff80u] = 0x81;
            put8(guest, t->ricky_state, 0x21); put8(guest, t->essences_obtained, 0x15); put8(guest, t->animal_companion, 0x0c);
            put8(guest, t->bought_shop_items1, 0x0f); put8(guest, t->seed_tree_refilled_bitset, 0x02);
            put8(guest, (OraclesGuestSym){ 0, (uint16_t)(t->group0_room_flags.addr + 0x1f0u) }, 0x17);
            put8(guest, (OraclesGuestSym){ 0, (uint16_t)(t->group0_room_flags.addr + 0x2f9u) }, 0x04);
            put8(guest, (OraclesGuestSym){ 0, (uint16_t)(t->obtained_treasure_flags.addr + 9u) }, 0x01);   /* treasure $48 */
            oracles_guest_set_transition_policy(guest, capture_policy, NULL, NULL);
            put8(guest, t->screen_transition_state, 3); put8(guest, t->vblank_checker, 0);
            policy_calls = 0;
            oracles_core_run_frame(core);
            CHECK(policy_calls == 1);
            const OraclesGuestTransitionState *g = &last_state;
            CHECK(g->load.gameboy_type == 0x81 && g->load.ricky_state == 0x21 && g->load.essences_obtained == 0x15 && g->load.animal_companion == 0x0c);
            CHECK(g->load.bought_shop_items1 == 0x0f && g->load.seed_tree_refilled == 0x02 && g->load.given == 0x3f && g->load.room_code_replay == 1);
            CHECK(g->load.room_flags_all && g->load.room_flags_all_size == t->room_flags_size && g->load.room_flags_all_size >= 0x300u);
            CHECK(g->load.room_flags_all && g->load.room_flags_all[0x1f0] == 0x17 && g->load.room_flags_all[0x2f9] == 0x04);
            CHECK(g->load.obtained_treasures && g->load.obtained_treasures_size == t->obtained_treasure_flags_size && (g->load.obtained_treasures[9] & 1u));
        }
        oracles_guest_set_transition_policy(guest, NULL, NULL, NULL);
    }

    /* Where the next scan draws Link after a savestate load: where the loaded state has him, whatever the execution it
     * replaced last drew.  The OAM copied at a vblank (wVBlankChecker $ff) follows his position; after the load, two
     * vblanks without a copy show where the reset put him. */
    {
        const OraclesGuestTables *t = oracles_guest_tables(guest);
        uint8_t *objects = oracles_guest_wram_writable(guest, 1);
        CHECK(objects != NULL);
        if (objects) {
            objects[ORACLES_OBJ_X] = 0; objects[ORACLES_OBJ_XH] = 40; objects[ORACLES_OBJ_Y] = 0; objects[ORACLES_OBJ_YH] = 50;
            put8(guest, t->vblank_checker, 0xff);
            oracles_core_run_frame(core);
            oracles_core_run_frame(core);
            uint16_t x = 0, y = 0;
            CHECK(oracles_guest_link_shown(guest, &x, &y) && x == 40u << 8 && y == 50u << 8);
            objects[ORACLES_OBJ_XH] = 90; objects[ORACLES_OBJ_YH] = 70;
            oracles_guest_reset_execution_state(guest);
            put8(guest, t->vblank_checker, 0);
            oracles_core_run_frame(core);
            oracles_core_run_frame(core);
            CHECK(oracles_guest_link_shown(guest, &x, &y) && x == 90u << 8 && y == 70u << 8);
        }
    }

    oracles_guest_detach(guest);

    /* The map the transition state gives for the active group: on Seasons' engine Subrosia's own (group 1), Temple of
     * Seasons' walled inside the grid from its first column and row; Ages' one map for both groups. */
    {
        static const struct { OraclesGame game; OraclesRomRevision revision; uint8_t width[2], height[2], left[2], top[2]; } maps[] = {
            { ORACLES_GAME_SEASONS, ORACLES_ROM_REVISION_SEASONS_US, { 16, 11 }, { 16, 8 }, { 0, 0 }, { 0, 0 } },
            { ORACLES_GAME_SEASONS, ORACLES_ROM_REVISION_TEMPLE_1_073, { 16, 11 }, { 12, 8 }, { 13, 4 }, { 3, 0 } },
            { ORACLES_GAME_AGES, ORACLES_ROM_REVISION_AGES_US, { 14, 14 }, { 14, 14 }, { 0, 0 }, { 0, 0 } },
        };
        for (unsigned i = 0; i < sizeof maps / sizeof maps[0]; i++) {
            const OraclesRomInfo info = { .game = maps[i].game, .revision = maps[i].revision, .known = 1 };
            OraclesGuest *g = oracles_guest_attach(core, oracles_compat_find(&info));
            CHECK(g != NULL);
            if (!g) continue;
            oracles_guest_clear_hooks(g);
            CHECK(oracles_guest_add_hook(g, sub_entry, ORACLES_EVENT_TEXT, ORACLES_EVENT_FRAME_DRAWN) == 0);
            oracles_guest_set_transition_policy(g, capture_policy, NULL, NULL);
            for (unsigned group = 0; group < 2u; group++) {
                put8(g, oracles_guest_tables(g)->active_group, (uint8_t)group);
                policy_calls = 0;
                oracles_core_run_frame(core);
                CHECK(policy_calls > 0);
                CHECK(last_state.active_group == group && last_state.map_width == maps[i].width[group] && last_state.map_height == maps[i].height[group]);
                CHECK(last_state.map_left == maps[i].left[group] && last_state.map_top == maps[i].top[group]);
            }
            oracles_guest_set_transition_policy(g, NULL, NULL, NULL);
            oracles_guest_detach(g);
        }
    }
    oracles_core_destroy(core);
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_guest: ok\n");
    return 0;
}
