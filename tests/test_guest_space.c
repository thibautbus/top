/* The guest's address spaces on a synthetic image of two halves (8 MiB, MBC5, 32 KiB of cartridge RAM), on both cores:
 *   $0150: ld sp,$c200 ; RAM enabled, its bank 1 mapped ; ld a,$c9 ; ld ($a010),a ; ROM bank $13f mapped
 *   $0167: call $4010 ; call $a010 ; jr $0167
 *   bank $13f (the second half's $3f), $4010: ret
 * A hook of the second half's bank $3f fires with the profile's base $100 and not without it; a hook in the cartridge
 * RAM's code fires where the profile names its bank and the RAM holds its own half's copy, and nowhere else.  The ROM readers see the half: a routine scan in
 * the RAM's code reads its copy in the half's bank $41, and the call transaction and the inventory find their write
 * point in a mainThreadStart run from there.  The swim's hold, the cheats and the instant text are armed again in each
 * half the game goes to, and so are the item hotkeys' write point and the call transaction; the text's transactions
 * follow the game to the other half, on its own index alone, or are refused when they could not. */
#include "core.h"
#include "guest.h"
#include "guest_text_box.h"
#include "oracles_rom.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

#define IMAGE_SIZE (8u * 1024u * 1024u)
#define HALF ((size_t)ORACLES_ROM_HALF_SIZE)

static const OraclesCompatProfile *ages_profile(void)
{
    const OraclesRomInfo info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_AGES_US, .known = 1 };
    return oracles_compat_find(&info);
}

static unsigned banked, wrong_bank, in_sram;

static void on_event(void *opaque, const OraclesGuestEvent *e)
{
    (void)opaque;
    if (e->type == ORACLES_EVENT_MENU) banked++;
    else if (e->type == ORACLES_EVENT_SAVE) wrong_bank++;
    else if (e->type == ORACLES_EVENT_TEXT) in_sram++;
}

static uint8_t *make_image(void)
{
    uint8_t *rom = calloc(IMAGE_SIZE, 1);
    if (!rom) return NULL;
    memcpy(rom + 0x134, "TWO HALVES", 10);
    rom[0x143] = 0xc0; rom[0x147] = 0x1b; rom[0x148] = 0x08; rom[0x149] = 0x03;
    rom[0x100] = 0x00; rom[0x101] = 0xc3; rom[0x102] = 0x50; rom[0x103] = 0x01;      /* nop ; jp $0150 */
    const uint8_t program[] = { 0x31, 0x00, 0xc2,                /* $0150: ld sp,$c200 */
                                0x3e, 0x0a, 0xea, 0x00, 0x00,    /* ld a,$0a ; ld ($0000),a : RAM enabled */
                                0x3e, 0x01, 0xea, 0x00, 0x40,    /* ld a,1 ; ld ($4000),a : RAM bank 1 */
                                0x3e, 0xc9, 0xea, 0x10, 0xa0,    /* ld a,$c9 ; ld ($a010),a : a ret in the RAM's code */
                                0x3e, 0x01, 0xea, 0x00, 0x30,    /* ld a,1 ; ld ($3000),a : the ninth bank bit */
                                0x3e, 0x3f, 0xea, 0x00, 0x20,    /* ld a,$3f ; ld ($2000),a : bank $13f */
                                0xcd, 0x10, 0x40,                /* $0167: call $4010 */
                                0xcd, 0x10, 0xa0,                /* call $a010 */
                                0x18, 0xf8 };                    /* jr $0167 */
    memcpy(rom + 0x150, program, sizeof program);
    rom[0x13fu * 0x4000u + 0x10u] = 0xc9;                        /* bank $13f, $4010: ret */
    rom[0x3fu * 0x4000u + 0x10u] = 0xc9;                         /* and the first half's $3f, never mapped */
    rom[0x141u * 0x4000u + 0x10u] = 0xc9;                        /* the second half's bank $41, which the RAM's code copies */
    return rom;
}

/* `frames` frames with the hooks of `profile`; the counts of each. */
static void run_hooks(OraclesCore *core, const OraclesCompatProfile *profile, unsigned frames)
{
    OraclesGuest *guest = oracles_guest_attach(core, profile);
    CHECK(guest != NULL);
    if (!guest) return;
    oracles_guest_set_event_sink(guest, on_event, NULL);
    oracles_guest_clear_hooks(guest);
    CHECK(oracles_guest_add_hook(guest, (OraclesGuestSym){ 0x3f, 0x4010 }, ORACLES_EVENT_MENU, 0) == 0);
    CHECK(oracles_guest_add_hook(guest, (OraclesGuestSym){ 0x3e, 0x4010 }, ORACLES_EVENT_SAVE, 0) == 0);
    CHECK(oracles_guest_add_hook(guest, (OraclesGuestSym){ 0x41, 0xa010 }, ORACLES_EVENT_TEXT, 0) == 0);
    banked = wrong_bank = in_sram = 0;
    for (unsigned i = 0; i < frames; i++) oracles_core_run_frame(core);
    oracles_guest_detach(guest);
}

static void hooks_on(OraclesCoreKind kind, const uint8_t *image)
{
    const OraclesCoreOptions options = { 0, 0, kind, 0 };
    OraclesCore *core = oracles_core_create(image, IMAGE_SIZE, &options);
    CHECK(core != NULL);
    if (!core) return;
    {   /* the RAM's bank 1 holds the second half's copy of bank $41, as fillBootstrappedSram leaves it */
        size_t ram_size = 0;
        uint16_t ram_bank = 0;
        uint8_t *ram = oracles_core_memory(core, ORACLES_CORE_CART_RAM, &ram_size, &ram_bank);
        CHECK(ram && ram_size == 0x8000u);
        if (ram) memcpy(ram + 0x2000u, image + 0x141u * 0x4000u, 0x2000u);
    }
    for (unsigned i = 0; i < 200; i++) oracles_core_run_frame(core);   /* the boot ROM, then the program's loop */
    {
        uint16_t rom_bank = 0, ram_bank = 0;
        oracles_core_memory(core, ORACLES_CORE_ROM, NULL, &rom_bank);
        oracles_core_memory(core, ORACLES_CORE_CART_RAM, NULL, &ram_bank);
        CHECK(rom_bank == 0x13fu && ram_bank == 1u);
    }
    OraclesCompatProfile half = *ages_profile();
    half.rom_bank_base = 0x100;
    half.sram_code_banks = 1u << 1;
    run_hooks(core, &half, 2);
    CHECK(banked > 100 && wrong_bank == 0);
    CHECK(in_sram + 1 >= banked && banked + 1 >= in_sram);   /* a call each, the frame's edge between two */

    /* Without the base, the second half's bank is not the hook's; without the RAM's code banks, nothing runs there. */
    run_hooks(core, ages_profile(), 2);
    CHECK(banked == 0 && wrong_bank == 0 && in_sram == 0);

    /* The first half's: its copy of bank $41 is not what the RAM holds. */
    half.rom_bank_base = 0;
    run_hooks(core, &half, 2);
    CHECK(banked == 0 && in_sram == 0);
    half.rom_bank_base = 0x100;

    /* The RAM's code from another of its banks: not the one mapped. */
    half.sram_code_banks = 1u << 2;
    run_hooks(core, &half, 2);
    CHECK(banked > 100 && in_sram == 0);
    oracles_core_destroy(core);
}

static const OraclesCompatProfile *combined_ages_profile(void)
{
    const OraclesRomInfo info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_COMBINED_1_5_EN, .known = 1 };
    return oracles_compat_find(&info);
}

/* swapGame at $2526 on a synthetic image, a game of the first half calling it in a loop with a routine at $2600:
 *   $0150: ld sp,$c200 ; xor a ; ld ($c4bd),a (the game running: the first half's) ; ld a,2 ; ld ($c4bc),a (asked: the same)
 *   $015b: call $2526 ; call $2600 ; jr $015b
 * The request of the game already running leaves the guest in its half; another game's stops it there: no hook fires
 * after, none is armed again, and a state of the other half's game stops a guest as well. */
static uint8_t *make_switch_image(void)
{
    uint8_t *image = calloc(IMAGE_SIZE, 1);
    if (!image) return NULL;
    memcpy(image + 0x134, "TWO HALVES", 10);
    image[0x143] = 0xc0; image[0x147] = 0x1b; image[0x148] = 0x08; image[0x149] = 0x03;
    image[0x100] = 0x00; image[0x101] = 0xc3; image[0x102] = 0x50; image[0x103] = 0x01;
    const uint8_t program[] = { 0x31, 0x00, 0xc2, 0xaf, 0xea, 0xbd, 0xc4, 0x3e, 0x02, 0xea, 0xbc, 0xc4,
                                0xcd, 0x26, 0x25, 0xcd, 0x00, 0x26, 0x18, 0xf8 };
    memcpy(image + 0x150, program, sizeof program);
    image[0x2526] = 0xc9;
    image[0x2600] = 0xc9;
    return image;
}

static void half_switch_on(OraclesCoreKind kind)
{
    uint8_t *image = make_switch_image();
    CHECK(image != NULL);
    if (!image) return;
    const OraclesCoreOptions options = { 0, 0, kind, 0 };
    OraclesCore *core = oracles_core_create(image, IMAGE_SIZE, &options);
    const OraclesCompatProfile *half = combined_ages_profile();
    CHECK(core != NULL && half != NULL && oracles_compat_half_switch(half));
    if (!core || !half) { oracles_core_destroy(core); free(image); return; }
    for (unsigned i = 0; i < 200; i++) oracles_core_run_frame(core);
    OraclesGuest *guest = oracles_guest_attach(core, half);
    CHECK(guest != NULL);
    if (guest) {
        oracles_guest_set_event_sink(guest, on_event, NULL);
        CHECK(oracles_guest_add_hook(guest, (OraclesGuestSym){ 0x00, 0x2600 }, ORACLES_EVENT_MENU, 0) == 0);
        banked = 0;
        oracles_guest_set_frame(guest, 7);
        oracles_core_run_frame(core);
        CHECK(banked > 100 && !oracles_guest_half_left(guest) && !oracles_guest_check_half(guest));
        size_t size = 0;
        uint16_t bank = 0;
        uint8_t *wram = oracles_core_memory(core, ORACLES_CORE_WRAM, &size, &bank);
        CHECK(wram && size >= 0x1000u);
        if (wram) wram[0x4bc] = 3;   /* the second half's game asked for */
        banked = 0;
        oracles_core_run_frame(core);
        const unsigned before = banked;
        CHECK(oracles_guest_half_left(guest) == 8u && before <= 1u);
        oracles_core_run_frame(core);
        CHECK(banked == before && oracles_guest_hook_count(guest) == 0);
        CHECK(oracles_guest_add_hook(guest, (OraclesGuestSym){ 0x00, 0x2600 }, ORACLES_EVENT_MENU, 0) != 0);
        oracles_guest_detach(guest);
    }
    /* A state of the other half's game: the guest attached to it stops at the first look.  So does one of the frame of
     * the switch, which swapGame has entered and whose byte still names this half; the request of the game running, or
     * none, leaves it in its half. */
    size_t size = 0;
    uint16_t bank = 0;
    uint8_t *wram = oracles_core_memory(core, ORACLES_CORE_WRAM, &size, &bank);
    static const struct { uint8_t running, asked; int stops; } states[] = { { 1, 0, 1 }, { 1, 3, 1 }, { 0, 3, 1 }, { 0, 0, 0 }, { 0, 2, 0 } };
    for (size_t i = 0; wram && i < sizeof states / sizeof states[0]; i++) {
        wram[0x4bd] = states[i].running;
        wram[0x4bc] = states[i].asked;
        guest = oracles_guest_attach(core, half);
        CHECK(guest != NULL && oracles_guest_check_half(guest) == states[i].stops && (oracles_guest_half_left(guest) != 0) == states[i].stops);
        oracles_guest_detach(guest);
    }
    /* An image of one game has no such hook: its guest never stops. */
    guest = oracles_guest_attach(core, ages_profile());
    CHECK(guest != NULL && !oracles_guest_check_half(guest) && oracles_guest_hook_count(guest) == 11u);
    oracles_guest_detach(guest);
    oracles_core_destroy(core);
    free(image);
}

typedef struct half_followed { OraclesGuest *guest; unsigned told, hooks_after; } half_followed;

/* What a consumer does when told: the hook it asked for, in the half's tables. */
static void half_told(void *opaque)
{
    half_followed *f = opaque;
    f->told++;
    oracles_guest_add_hook(f->guest, (OraclesGuestSym){ 0x00, 0x2600 }, ORACLES_EVENT_MENU, 0);
    f->hooks_after = oracles_guest_hook_count(f->guest);
}

static void set_game(uint8_t *wram, unsigned running, unsigned asked, unsigned game_state)
{
    wram[0x4bd] = (uint8_t)running;
    wram[0x4bc] = (uint8_t)asked;
    wram[0x2ee] = (uint8_t)game_state;
}

/* The guest follows the game from half to half, on the same address: not before swapGame names the game it goes to, its
 * hooks then the other half's and its consumers told once, the game between two halves until swapGame ends; a reset out
 * of the second half waits for the game's memory to be cleared; a state, or a late attachment, follows at once; a reader
 * follows without a hook; a ghost's guest only notes swapGame; an image of one game has nothing to follow. */
static void half_follow_on(OraclesCoreKind kind)
{
    uint8_t *image = make_switch_image();
    CHECK(image != NULL);
    if (!image) return;
    const OraclesCoreOptions options = { 0, 0, kind, 0 };
    OraclesCore *core = oracles_core_create(image, IMAGE_SIZE, &options);
    const OraclesCompatProfile *ages = combined_ages_profile(), *seasons = oracles_compat_half(ages, 1);
    CHECK(core != NULL && ages != NULL && seasons != NULL && seasons != ages);
    if (!core || !ages || !seasons) { oracles_core_destroy(core); free(image); return; }
    for (unsigned i = 0; i < 200; i++) oracles_core_run_frame(core);
    size_t size = 0;
    uint16_t bank = 0;
    uint8_t *wram = oracles_core_memory(core, ORACLES_CORE_WRAM, &size, &bank);
    CHECK(wram != NULL);
    OraclesGuest *guest = oracles_guest_attach(core, ages);
    CHECK(guest != NULL);
    if (!guest || !wram) { oracles_guest_detach(guest); oracles_core_destroy(core); free(image); return; }
    /* Attached in the loop's own swapGame (the request of the game running): taken as a state of that copy, at once. */
    CHECK(oracles_guest_follow_half(guest) == 1 && oracles_guest_profile(guest) == ages);
    half_followed followed = { guest, 0, 0 };
    CHECK(oracles_guest_add_half_listener(guest, half_told, &followed) == 0);
    oracles_guest_set_event_sink(guest, on_event, NULL);
    half_told(&followed);
    followed.told = 0;
    const unsigned hooks = oracles_guest_hook_count(guest);
    oracles_core_run_frame(core);
    CHECK(oracles_guest_follow_half(guest) == 0 && oracles_guest_profile(guest) == ages && !oracles_guest_half_left(guest));
    /* The door: asked at the frame swapGame enters; followed once the byte names the other game. */
    set_game(wram, 0, 3, 2);
    oracles_core_run_frame(core);
    CHECK(oracles_guest_half_left(guest) != 0 && oracles_guest_hook_count(guest) == 0 && oracles_guest_half_passage(guest));
    CHECK(oracles_guest_follow_half(guest) == 0 && oracles_guest_profile(guest) == ages && followed.told == 0);
    set_game(wram, 1, 3, 2);
    CHECK(oracles_guest_follow_half(guest) == 1 && oracles_guest_profile(guest) == seasons && oracles_guest_tables(guest) == oracles_compat_guest_tables(seasons));
    CHECK(followed.told == 1 && !oracles_guest_half_left(guest) && oracles_guest_hook_count(guest) == hooks && followed.hooks_after == hooks);
    CHECK(oracles_guest_half_passage(guest));   /* swapGame still copies its code */
    CHECK(oracles_guest_follow_half(guest) == 0 && followed.told == 1);
    set_game(wram, 1, 0, 0);
    CHECK(oracles_guest_half_passage(guest));   /* and its screen is still the one it left */
    set_game(wram, 1, 0, 2);
    CHECK(oracles_guest_follow_half(guest) == 0 && !oracles_guest_half_passage(guest));
    set_game(wram, 1, 3, 2);   /* the loop's own request, of the game running */
    banked = 0;
    oracles_core_run_frame(core);
    CHECK(banked > 100 && oracles_guest_follow_half(guest) == 0);
    /* The way back, through the hook armed in the other half. */
    set_game(wram, 1, 2, 2);
    oracles_core_run_frame(core);
    CHECK(oracles_guest_half_left(guest) != 0);
    set_game(wram, 0, 2, 2);
    CHECK(oracles_guest_follow_half(guest) == 1 && oracles_guest_profile(guest) == ages && followed.told == 2);
    set_game(wram, 0, 0, 0);
    /* The same game asked for answers in its half. */
    set_game(wram, 0, 2, 2);
    oracles_core_run_frame(core);
    CHECK(!oracles_guest_half_left(guest) && oracles_guest_follow_half(guest) == 0);
    /* A reset out of the second half: the byte named by sramBootstrap, the memory cleared later. */
    set_game(wram, 1, 0, 2);
    oracles_guest_reset_execution_state(guest);
    CHECK(oracles_guest_follow_half(guest) == 1 && oracles_guest_profile(guest) == seasons);   /* a state: at once */
    set_game(wram, 0, 0, 2);
    CHECK(oracles_guest_follow_half(guest) == 0 && oracles_guest_half_left(guest) != 0 && oracles_guest_half_passage(guest) && oracles_guest_profile(guest) == seasons);
    set_game(wram, 0, 0, 0);
    CHECK(oracles_guest_follow_half(guest) == 1 && oracles_guest_profile(guest) == ages && oracles_guest_half_passage(guest));
    for (unsigned i = 0; i < 70u; i++) oracles_guest_follow_half(guest);
    CHECK(!oracles_guest_half_passage(guest));   /* the LCD on, the game not playing yet: not for long */
    /* A state of its own half, loaded in a passage, takes the guest back. */
    set_game(wram, 0, 3, 2);
    CHECK(oracles_guest_check_half(guest) && oracles_guest_follow_half(guest) == 0);
    set_game(wram, 0, 0, 2);
    oracles_guest_reset_execution_state(guest);
    CHECK(oracles_guest_follow_half(guest) == 1 && oracles_guest_profile(guest) == ages && !oracles_guest_half_left(guest));
    /* A state taken in swapGame's copy, its game already the guest's half's: the guest attached again (its consumers told),
     * the screen held until the first room's blank; the same state in normal play leaves it alone. */
    set_game(wram, 0, 2, 2);
    oracles_guest_reset_execution_state(guest);
    CHECK(oracles_guest_follow_half(guest) == 1 && oracles_guest_profile(guest) == ages && followed.told == 6 && oracles_guest_half_passage(guest));
    set_game(wram, 0, 0, 2);
    CHECK(oracles_guest_follow_half(guest) == 0 && !oracles_guest_half_passage(guest));
    oracles_guest_reset_execution_state(guest);
    CHECK(oracles_guest_follow_half(guest) == 0 && followed.told == 6);
    /* The reset's wait for init has a ceiling. */
    set_game(wram, 1, 0, 2);
    CHECK(oracles_guest_check_half(guest));
    unsigned waited = 0;
    while (waited < 1000u && oracles_guest_follow_half(guest) == 0) waited++;
    CHECK(waited > 100u && waited < 200u && oracles_guest_profile(guest) == seasons);
    set_game(wram, 0, 0, 2);
    oracles_guest_reset_execution_state(guest);
    CHECK(oracles_guest_follow_half(guest) == 1 && oracles_guest_profile(guest) == ages);
    OraclesGuestHalfCounts counts;
    oracles_guest_half_counts(guest, &counts);
    CHECK(counts.switches == 9u && counts.hooks_armed == 8u * hooks + (hooks - 1u) && counts.returns_dropped == 0u);   /* the first, before the consumer asked */
    CHECK(oracles_guest_hooks_refused(guest) == 0);
    oracles_guest_remove_half_listener(guest, half_told, &followed);
    set_game(wram, 1, 0, 2);
    oracles_guest_reset_execution_state(guest);
    CHECK(oracles_guest_follow_half(guest) == 1 && followed.told == 8);
    oracles_guest_detach(guest);
    /* Attached late to a game in the other half, a reader or a guest: at once. */
    set_game(wram, 1, 0, 2);
    guest = oracles_guest_attach(core, ages);
    CHECK(guest != NULL && oracles_guest_follow_half(guest) == 1 && oracles_guest_profile(guest) == seasons);
    oracles_guest_detach(guest);
    guest = oracles_guest_reader(core, ages);
    CHECK(guest != NULL && oracles_guest_follow_half(guest) == 1 && oracles_guest_profile(guest) == seasons && oracles_guest_hook_count(guest) == 0);
    oracles_guest_detach(guest);
    /* A ghost's guest notes swapGame and stays as it was; the next load forgets it. */
    set_game(wram, 0, 2, 2);
    guest = oracles_guest_attach(core, ages);
    CHECK(guest != NULL);
    if (guest) {
        oracles_guest_clear_hooks(guest);
        oracles_guest_watch_half(guest);
        CHECK(oracles_guest_hook_count(guest) == 1u && !oracles_guest_half_crossed(guest));
        oracles_core_run_frame(core);
        CHECK(!oracles_guest_half_crossed(guest));
        set_game(wram, 0, 3, 2);
        oracles_core_run_frame(core);
        CHECK(oracles_guest_half_crossed(guest) && !oracles_guest_half_left(guest) && oracles_guest_hook_count(guest) == 1u && oracles_guest_half_passage(guest));
        oracles_guest_reset_execution_state(guest);
        CHECK(!oracles_guest_half_crossed(guest));
        oracles_guest_detach(guest);
    }
    /* An image of one game: nothing to follow, no one told. */
    set_game(wram, 1, 3, 2);
    guest = oracles_guest_attach(core, ages_profile());
    CHECK(guest != NULL && oracles_guest_add_half_listener(guest, half_told, &followed) == 0 && oracles_guest_follow_half(guest) == 0 && !oracles_guest_half_passage(guest));
    oracles_guest_watch_half(guest);
    CHECK(oracles_guest_hook_count(guest) == 11u);
    oracles_guest_detach(guest);
    oracles_core_destroy(core);
    free(image);
}

static void put(uint8_t *image, size_t half, OraclesGuestSym at, const uint8_t *bytes, size_t length)
{
    const size_t offset = at.addr < 0x4000u ? at.addr : oracles_rom_in_sram(at.addr) ? at.bank * 0x4000u + (at.addr - ORACLES_SRAM_CODE_START)
                                                                                       : at.bank * 0x4000u + (at.addr - 0x4000u);
    memcpy(image + half + offset, bytes, length);
}

/* The points of the swim's hold, the cheats and the instant text, as the game's routines hold them, in a half of the
 * switch image at its tables' addresses: removeRupeeValue's hl loaded by a call into the cartridge RAM's code, as in
 * Oracles Combined; the instant text's left out where `text` is 0. */
static void put_features(uint8_t *image, size_t half, const OraclesGuestTables *t, int text)
{
    const uint8_t keys_lo = (uint8_t)t->game_keys_just_pressed.addr, keys_hi = (uint8_t)(t->game_keys_just_pressed.addr >> 8);
    const uint8_t mermaid[] = { 0xfa, keys_lo, keys_hi, 0xe6, 0xf0 }, flippers[] = { 0xfa, keys_lo, keys_hi, 0xe6, 0x01 };
    put(image, half, t->link_update_velocity_mermaid, mermaid, sizeof mermaid);
    put(image, half, t->link_update_flippers_state0, flippers, sizeof flippers);
    const uint8_t damage[] = { 0x62, 0x2e, (uint8_t)t->link_damage_to_apply.addr, 0x7e, 0x36, 0x00, 0xb7 };
    const uint8_t ammo[] = { 0x7e, 0xd6, 0x01, 0x27, 0x77, 0xb4, 0xc9 };
    const uint8_t rupees[] = { 0xcd, 0x4e, 0xa2, 0xcd, (uint8_t)t->get_rupee_value.addr, (uint8_t)(t->get_rupee_value.addr >> 8) };
    const uint8_t loader[] = { 0x21, (uint8_t)t->num_rupees.addr, (uint8_t)(t->num_rupees.addr >> 8), 0xc9 };
    const uint8_t fairy[] = { 0xcd, (uint8_t)t->dec_num_bombs.addr, (uint8_t)(t->dec_num_bombs.addr >> 8) };
    put(image, half, t->link_apply_damage, damage, sizeof damage);
    put(image, half, t->dec_num_bombs, ammo, sizeof ammo);
    put(image, half, t->remove_rupee_value, rupees, sizeof rupees);
    put(image, half, (OraclesGuestSym){ ORACLES_SRAM_CODE_BANK, 0xa24e }, loader, sizeof loader);
    if (t->bomb_fairy_lose_all_bombs.bank != ORACLES_GUEST_ABSENT) put(image, half, t->bomb_fairy_lose_all_bombs, fairy, sizeof fairy);
    if (!text) return;
    const uint8_t test[] = { 0xfa, (uint8_t)t->keys_just_pressed.addr, (uint8_t)(t->keys_just_pressed.addr >> 8), 0xe6, 0x03 };
    const uint8_t loop[] = { 0x1e, (uint8_t)t->text_flags.addr, 0x1a, 0xcb, 0x47 };
    put(image, half, t->update_character_display_timer, test, sizeof test);
    put(image, half, t->display_next_text_character, loop, sizeof loop);
}

/* The swim's hold, the cheats and the instant text follow the game from half to half: their points armed again in the
 * code of the half it goes to, each feature's own listener told; turned on between two halves, armed in the next and
 * not in the half left; a half without a feature's points counts it refused, the others armed. */
static void half_features_on(OraclesCoreKind kind)
{
    uint8_t *image = make_switch_image();
    const OraclesCompatProfile *ages = combined_ages_profile(), *seasons = oracles_compat_half(ages, 1);
    CHECK(image != NULL && ages != NULL && seasons != NULL);
    if (!image || !ages || !seasons) { free(image); return; }
    put_features(image, 0, oracles_compat_guest_tables(ages), 1);
    put_features(image, HALF, oracles_compat_guest_tables(seasons), 1);
    const OraclesCoreOptions options = { 0, 0, kind, 0 };
    OraclesCore *core = oracles_core_create(image, IMAGE_SIZE, &options);
    CHECK(core != NULL);
    if (!core) { free(image); return; }
    for (unsigned i = 0; i < 200; i++) oracles_core_run_frame(core);
    size_t size = 0;
    uint16_t bank = 0;
    uint8_t *wram = oracles_core_memory(core, ORACLES_CORE_WRAM, &size, &bank);
    OraclesGuest *guest = oracles_guest_attach(core, ages);
    CHECK(guest != NULL && wram != NULL);
    if (!guest || !wram) { oracles_guest_detach(guest); oracles_core_destroy(core); free(image); return; }
    const unsigned base = oracles_guest_hook_count(guest);
    const unsigned all = ORACLES_CHEAT_NO_DAMAGE | ORACLES_CHEAT_AMMO | ORACLES_CHEAT_RUPEES;
    CHECK(oracles_guest_set_swim_hold(guest, 1) == 0 && oracles_guest_set_text_instant(guest, 1) == 0 && oracles_guest_set_cheats(guest, all) == 0);
    CHECK(oracles_guest_hook_count(guest) == base + 7u);
    /* The pause changing them again and again: each feature told of a switch once, its points armed once. */
    for (unsigned i = 0; i < 20u; i++)
        CHECK(oracles_guest_set_cheats(guest, i & 1u ? all : ORACLES_CHEAT_RUPEES) == 0 && oracles_guest_set_swim_hold(guest, (int)(i & 1u)) == 0
              && oracles_guest_set_text_instant(guest, (int)(i & 1u)) == 0);
    CHECK(oracles_guest_hook_count(guest) == base + 7u);
    CHECK(oracles_guest_set_cheats(guest, ORACLES_CHEAT_AMMO) == 0 && oracles_guest_set_swim_hold(guest, 0) == 0);   /* off: still armed */
    set_game(wram, 0, 3, 2);
    oracles_core_run_frame(core);
    CHECK(oracles_guest_half_left(guest) != 0 && oracles_guest_hook_count(guest) == 0);
    set_game(wram, 1, 3, 2);
    CHECK(oracles_guest_follow_half(guest) == 1 && oracles_guest_profile(guest) == seasons && oracles_guest_hook_count(guest) == base + 7u);
    CHECK(oracles_guest_cheats(guest) == ORACLES_CHEAT_AMMO && !oracles_guest_swim_hold(guest) && oracles_guest_text_instant(guest));
    CHECK(oracles_guest_set_swim_hold(guest, 1) == 0 && oracles_guest_set_cheats(guest, all) == 0 && oracles_guest_hook_count(guest) == base + 7u);
    set_game(wram, 1, 0, 2);
    oracles_guest_follow_half(guest);
    set_game(wram, 1, 2, 2);
    oracles_core_run_frame(core);
    set_game(wram, 0, 2, 2);
    CHECK(oracles_guest_follow_half(guest) == 1 && oracles_guest_profile(guest) == ages && oracles_guest_hook_count(guest) == base + 7u);
    OraclesGuestHalfCounts counts;
    oracles_guest_half_counts(guest, &counts);
    CHECK(counts.switches == 2u && counts.hooks_armed == 2u * (base + 7u) && counts.features_refused == 0u && oracles_guest_hooks_refused(guest) == 0);
    oracles_guest_detach(guest);

    /* Turned on between two halves: nothing armed in the half left, everything in the next. */
    set_game(wram, 0, 0, 2);
    guest = oracles_guest_attach(core, ages);
    CHECK(guest != NULL);
    if (guest) {
        set_game(wram, 0, 3, 2);
        oracles_core_run_frame(core);
        CHECK(oracles_guest_half_left(guest) != 0);
        CHECK(oracles_guest_set_swim_hold(guest, 1) == 0 && oracles_guest_set_text_instant(guest, 1) == 0 && oracles_guest_set_cheats(guest, all) == 0);
        CHECK(oracles_guest_hook_count(guest) == 0 && oracles_guest_swim_hold(guest) && oracles_guest_cheats(guest) == all);
        set_game(wram, 1, 3, 2);
        CHECK(oracles_guest_follow_half(guest) == 1 && oracles_guest_hook_count(guest) == base + 7u);
        oracles_guest_half_counts(guest, &counts);
        CHECK(counts.features_refused == 0u);
        oracles_guest_detach(guest);
    }
    /* Turned on first in the second half: armed in its code, and in the first half's on the way back. */
    set_game(wram, 0, 0, 2);
    guest = oracles_guest_attach(core, ages);
    CHECK(guest != NULL);
    if (guest) {
        set_game(wram, 0, 3, 2);
        oracles_core_run_frame(core);
        set_game(wram, 1, 3, 2);
        CHECK(oracles_guest_follow_half(guest) == 1 && oracles_guest_hook_count(guest) == base);
        CHECK(oracles_guest_set_swim_hold(guest, 1) == 0 && oracles_guest_set_text_instant(guest, 1) == 0 && oracles_guest_set_cheats(guest, all) == 0);
        CHECK(oracles_guest_hook_count(guest) == base + 7u);
        set_game(wram, 1, 0, 2);
        oracles_guest_follow_half(guest);
        set_game(wram, 1, 2, 2);
        oracles_core_run_frame(core);
        set_game(wram, 0, 2, 2);
        CHECK(oracles_guest_follow_half(guest) == 1 && oracles_guest_profile(guest) == ages && oracles_guest_hook_count(guest) == base + 7u);
        oracles_guest_half_counts(guest, &counts);
        CHECK(counts.features_refused == 0u);
        oracles_guest_detach(guest);
    }
    oracles_core_destroy(core);

    /* A half without the instant text's points nor the damage's: the other features armed there, the ammunition and the
     * rupees too, the instant text and the damage refused, each counted. */
    memset(image + HALF + 0x3fu * 0x4000u, 0, 0x4000u);
    {
        const OraclesGuestSym damage = oracles_compat_guest_tables(seasons)->link_apply_damage;
        memset(image + HALF + damage.bank * 0x4000u + (damage.addr - 0x4000u), 0, 8u);
    }
    core = oracles_core_create(image, IMAGE_SIZE, &options);
    CHECK(core != NULL);
    wram = core ? oracles_core_memory(core, ORACLES_CORE_WRAM, &size, &bank) : NULL;
    for (unsigned i = 0; core && i < 200; i++) oracles_core_run_frame(core);
    set_game(wram, 0, 0, 2);
    guest = core && wram ? oracles_guest_attach(core, ages) : NULL;
    CHECK(guest != NULL);
    if (guest) {
        CHECK(oracles_guest_set_swim_hold(guest, 1) == 0 && oracles_guest_set_text_instant(guest, 1) == 0 && oracles_guest_set_cheats(guest, all) == 0);
        set_game(wram, 0, 3, 2);
        oracles_core_run_frame(core);
        set_game(wram, 1, 3, 2);
        CHECK(oracles_guest_follow_half(guest) == 1 && oracles_guest_hook_count(guest) == base + 4u);
        oracles_guest_half_counts(guest, &counts);
        CHECK(counts.features_refused == 2u);
        oracles_guest_detach(guest);
    }
    oracles_core_destroy(core);
    free(image);
}

static void no_operation(void *opaque, const OraclesGuestInventoryState *state, OraclesGuestInventoryOp *op) { (void)opaque; (void)state; (void)op; }

/* The readers' view of the half, the RAM's code read in its ROM bank; mainThreadStart run from there. */
static void readers_on(const uint8_t *image_in)
{
    uint8_t *image = malloc(IMAGE_SIZE);
    CHECK(image != NULL);
    if (!image) return;
    memcpy(image, image_in, IMAGE_SIZE);
    size_t half_size = 0;
    CHECK(oracles_rom_half(image, IMAGE_SIZE, 0x100, &half_size) == image + HALF && half_size == HALF);
    CHECK(oracles_rom_half(image, IMAGE_SIZE, 0, &half_size) == image && half_size == HALF);
    CHECK(oracles_rom_half(image, 2u * 1024u * 1024u, 0, &half_size) == image && half_size == 2u * 1024u * 1024u);   /* an original: the whole image */
    CHECK(oracles_rom_half(image, 2u * 1024u * 1024u, 0x100, &half_size) == NULL && half_size == 0);

    /* A routine of the RAM's code at $a020, its copy in the half's bank $41: the scan finds the sequence there alone. */
    const uint8_t sequence[] = { 0xfa, 0x29, 0xcc, 0xe6, 0x0f };
    const size_t copy = HALF + 0x41u * 0x4000u;
    memcpy(image + copy + 0x20u + 5u, sequence, sizeof sequence);
    memcpy(image + 0x41u * 0x4000u + 0x20u + 9u, sequence, sizeof sequence);   /* the other half's copy: not this half's */
    const uint8_t *view = image + HALF;
    const OraclesGuestSym routine = { 0x41, 0xa020 };
    CHECK(oracles_guest_find_in_routine(view, HALF, routine, sequence, sizeof sequence) == 0xa025u);
    CHECK(oracles_guest_find_in_routine(image, HALF, routine, sequence, sizeof sequence) == 0xa029u);
    {   /* the copy ends at $c000: a sequence across its end is not the routine's */
        memcpy(image + copy + 0x1ffeu, sequence, sizeof sequence);
        CHECK(oracles_guest_find_in_routine(view, HALF, (OraclesGuestSym){ 0x41, 0xbff0 }, sequence, sizeof sequence) == 0);
        memset(image + copy + 0x1ffeu, 0, sizeof sequence);
    }
    const OraclesGuestRom rom = { view, HALF };
    CHECK(oracles_guest_rom_at(&rom, routine, 2) == view + 0x41u * 0x4000u + 0x20u);
    CHECK(oracles_guest_rom_at(&rom, (OraclesGuestSym){ 0x41, 0xbff0 }, 0x10) != NULL);   /* the copy's 8 KiB, and no further */
    CHECK(oracles_guest_rom_at(&rom, (OraclesGuestSym){ 0x41, 0xbff0 }, 0x11) == NULL);
    CHECK(oracles_guest_rom_at(&rom, (OraclesGuestSym){ 0x41, 0x4020 }, 2) == view + 0x41u * 0x4000u + 0x20u);

    /* mainThreadStart in the RAM's code at $a333, its `call checkReloadStatusBarGraphics` in the second half's copy; the
     * catalogue's flag routines there too. */
    static OraclesGuestTables tables;
    tables = *oracles_compat_guest_tables(ages_profile());
    tables.main_thread_start = (OraclesGuestSym){ 0x41, 0xa333 };
    tables.set_global_flag = (OraclesGuestSym){ 0x41, 0xb331 };
    tables.unset_global_flag = (OraclesGuestSym){ 0x41, 0xb247 };
    const uint16_t target = tables.check_reload_status_bar_graphics.addr;
    const uint8_t call[] = { 0xcd, (uint8_t)target, (uint8_t)(target >> 8) };
    memcpy(image + copy + 0x333u + 6u, call, sizeof call);
    OraclesCompatProfile half = *ages_profile();
    half.guest_tables = &tables;
    half.rom_bank_base = 0x100;
    half.sram_code_banks = 1u << 1;
    const OraclesCoreOptions options = { 0, 0, ORACLES_CORE_SAMEBOY, 1 };
    OraclesCore *core = oracles_core_create(image, IMAGE_SIZE, &options);
    CHECK(core != NULL);
    if (core) {
        OraclesGuest *guest = oracles_guest_attach(core, &half);
        CHECK(guest != NULL);
        if (guest) {
            const OraclesGuestRom seen = oracles_guest_rom(guest);
            CHECK(seen.data && seen.size == HALF && memcmp(seen.data, image + HALF, 0x4000u) == 0);
            CHECK(oracles_guest_arm_calls(guest) == 0);
            CHECK(oracles_guest_set_inventory_policy(guest, no_operation, NULL, NULL, NULL) == 0);
            oracles_guest_detach(guest);
        }
        /* The first half's copy has no such call; a profile without the RAM's code refuses its routines. */
        half.rom_bank_base = 0;
        guest = oracles_guest_attach(core, &half);
        CHECK(guest != NULL && oracles_guest_arm_calls(guest) != 0);
        CHECK(guest != NULL && oracles_guest_set_inventory_policy(guest, no_operation, NULL, NULL, NULL) != 0);
        oracles_guest_detach(guest);
        half.rom_bank_base = 0x100;
        half.sram_code_banks = 0;
        guest = oracles_guest_attach(core, &half);
        CHECK(guest != NULL && oracles_guest_arm_calls(guest) != 0);
        oracles_guest_detach(guest);
        oracles_core_destroy(core);
    }
    free(image);
}

typedef struct inventory_told { OraclesGuest *guest; unsigned resets, resets_in_switch, resets_by_swap; } inventory_told;

static void inventory_reset(void *opaque)
{
    inventory_told *t = opaque;
    t->resets++;
    if (oracles_guest_switching_half(t->guest)) t->resets_in_switch++;
    if (oracles_guest_switching_half(t->guest) == 2) t->resets_by_swap++;
}

/* The item hotkeys follow the game from half to half: the inventory's write point found again in the mainThreadStart
 * of the half it goes to (each half's copy of bank $41), its hooks and the call transaction's armed there; the policy
 * told by a reset that says it is the switch's, and swapGame's apart from a reset's, where a load's says neither; given
 * between two halves, armed in the next. */
static void hotkeys_follow_on(OraclesCoreKind kind)
{
    uint8_t *image = make_switch_image();
    const OraclesCompatProfile *ages = combined_ages_profile(), *seasons = oracles_compat_half(ages, 1);
    CHECK(image != NULL && ages != NULL && seasons != NULL);
    if (!image || !ages || !seasons) { free(image); return; }
    for (unsigned h = 0; h < 2u; h++) {
        const OraclesGuestTables *t = oracles_compat_guest_tables(h ? seasons : ages);
        const uint16_t target = t->check_reload_status_bar_graphics.addr;
        const uint8_t call[] = { 0xcd, (uint8_t)target, (uint8_t)(target >> 8) };
        put(image, h * HALF, (OraclesGuestSym){ t->main_thread_start.bank, (uint16_t)(t->main_thread_start.addr + 6u) }, call, sizeof call);
    }
    const OraclesCoreOptions options = { 0, 0, kind, 0 };
    OraclesCore *core = oracles_core_create(image, IMAGE_SIZE, &options);
    CHECK(core != NULL);
    if (!core) { free(image); return; }
    for (unsigned i = 0; i < 200; i++) oracles_core_run_frame(core);
    size_t size = 0;
    uint16_t bank = 0;
    uint8_t *wram = oracles_core_memory(core, ORACLES_CORE_WRAM, &size, &bank);
    set_game(wram, 0, 0, 2);
    OraclesGuest *guest = wram ? oracles_guest_attach(core, ages) : NULL;
    CHECK(guest != NULL);
    if (!guest) { oracles_core_destroy(core); free(image); return; }
    inventory_told told = { guest, 0, 0, 0 };
    CHECK(oracles_guest_follow_half(guest) == 0);   /* the frame of the attachment: no switch */
    const unsigned base = oracles_guest_hook_count(guest);
    CHECK(oracles_guest_set_inventory_policy(guest, no_operation, NULL, inventory_reset, &told) == 0 && oracles_guest_arm_calls(guest) == 0);
    CHECK(oracles_guest_hook_count(guest) == base + 5u);
    set_game(wram, 0, 3, 2);
    oracles_core_run_frame(core);
    CHECK(oracles_guest_half_left(guest) != 0 && oracles_guest_hook_count(guest) == 0);
    set_game(wram, 1, 3, 2);
    CHECK(oracles_guest_follow_half(guest) == 1 && oracles_guest_profile(guest) == seasons && oracles_guest_hook_count(guest) == base + 5u);
    CHECK(told.resets == 1u && told.resets_in_switch == 1u && told.resets_by_swap == 1u && !oracles_guest_switching_half(guest));
    const OraclesGuestCall sound = { ORACLES_ROUTINE_PLAY_SOUND, 0x56, 0, 1 };
    CHECK(oracles_guest_queue_call(guest, &sound, NULL) == 0);
    set_game(wram, 1, 0, 2);   /* swapGame over */
    oracles_guest_reset_execution_state(guest);   /* a load: not the switch's */
    CHECK(told.resets == 2u && told.resets_in_switch == 1u);
    CHECK(oracles_guest_follow_half(guest) == 0);
    /* The reset out of the second half (no swapGame, the game's memory cleared): a switch, not swapGame's. */
    set_game(wram, 0, 0, 0);
    CHECK(oracles_guest_follow_half(guest) == 1 && oracles_guest_profile(guest) == ages);
    CHECK(told.resets == 3u && told.resets_in_switch == 2u && told.resets_by_swap == 1u && oracles_guest_hook_count(guest) == base + 5u);
    OraclesGuestHalfCounts counts;
    oracles_guest_half_counts(guest, &counts);
    CHECK(counts.features_refused == 0u && oracles_guest_hooks_refused(guest) == 0);
    oracles_guest_detach(guest);

    /* Given between two halves: nothing armed in the half left, the write point and the calls in the next. */
    set_game(wram, 0, 0, 2);
    guest = oracles_guest_attach(core, ages);
    CHECK(guest != NULL);
    if (guest) {
        set_game(wram, 0, 3, 2);
        oracles_core_run_frame(core);
        CHECK(oracles_guest_half_left(guest) != 0);
        CHECK(oracles_guest_set_inventory_policy(guest, no_operation, NULL, NULL, NULL) == 0 && oracles_guest_arm_calls(guest) == 0);
        CHECK(oracles_guest_hook_count(guest) == 0);
        set_game(wram, 1, 3, 2);
        CHECK(oracles_guest_follow_half(guest) == 1 && oracles_guest_hook_count(guest) == base + 5u && oracles_guest_queue_call(guest, &sound, NULL) == 0);
        oracles_guest_detach(guest);
    }
    /* A half whose mainThreadStart has no such call: the policy refused there, counted. */
    memset(image + HALF + 0x41u * 0x4000u + 0x333u, 0, 0x80u);
    oracles_core_destroy(core);
    core = oracles_core_create(image, IMAGE_SIZE, &options);
    wram = core ? oracles_core_memory(core, ORACLES_CORE_WRAM, &size, &bank) : NULL;
    for (unsigned i = 0; core && i < 200; i++) oracles_core_run_frame(core);
    set_game(wram, 0, 0, 2);
    guest = core && wram ? oracles_guest_attach(core, ages) : NULL;
    CHECK(guest != NULL);
    if (guest) {
        CHECK(oracles_guest_set_inventory_policy(guest, no_operation, NULL, NULL, NULL) == 0 && oracles_guest_arm_calls(guest) == 0);
        set_game(wram, 0, 3, 2);
        oracles_core_run_frame(core);
        set_game(wram, 1, 3, 2);
        CHECK(oracles_guest_follow_half(guest) == 1);
        oracles_guest_half_counts(guest, &counts);
        CHECK(counts.features_refused == 2u && oracles_guest_queue_call(guest, &sound, NULL) != 0);
        oracles_guest_detach(guest);
    }
    oracles_core_destroy(core);
    free(image);
}

/* A language's index at bank $70 of a half of the synthetic image of swapGame, `code` its language. */
static void put_index(uint8_t *image, unsigned half, const char *code)
{
    uint8_t *index = image + (half * 0x100u + 0x70u) * 0x4000u;
    memcpy(index, ORACLES_LANGUAGE_INDEX_MAGIC, ORACLES_LANGUAGE_INDEX_MAGIC_BYTES);
    index[16] = ORACLES_LANGUAGE_INDEX_VERSION;
    memcpy(index + ORACLES_LANGUAGE_INDEX_CODE, code, strlen(code));
}

static void no_listener(void *opaque) { (void)opaque; }

/* The text's transactions across the switch of halves: the language armed again on the index of the half gone to, its
 * own; none there, the guest serves none (not the index of the half it left) and counts it lost.  A guest whose image
 * of two games has no room for the listener that would arm them again refuses them; a ghost's guest, which never
 * switches, needs none. */
static void text_follows_on(OraclesCoreKind kind)
{
    const OraclesCompatProfile *ages = combined_ages_profile(), *seasons = oracles_compat_half(ages, 1);
    CHECK(ages && seasons);
    for (unsigned both = 0; both < 2u && ages && seasons; both++) {
        uint8_t *image = make_switch_image();
        CHECK(image != NULL);
        if (!image) return;
        put_index(image, 0, "en");
        if (both) put_index(image, 1, "fr");
        const OraclesCoreOptions options = { 0, 0, kind, 0 };
        OraclesCore *core = oracles_core_create(image, IMAGE_SIZE, &options);
        OraclesGuest *guest = core ? oracles_guest_attach(core, ages) : NULL;
        CHECK(guest != NULL);
        if (guest) {
            CHECK(oracles_guest_arm_language(guest) == 0 && !strcmp(oracles_guest_language(guest), "en"));
            CHECK(oracles_guest_switch_to(guest, seasons) == 0);
            OraclesGuestHalfCounts half;
            oracles_guest_half_counts(guest, &half);
            CHECK(half.switches == 1u && half.text_armed == both && half.text_lost == !both);
            CHECK(!strcmp(oracles_guest_language(guest), both ? "fr" : ""));
            oracles_guest_detach(guest);
        }
        /* No room for the listener: refused, nothing served. */
        guest = core ? oracles_guest_attach(core, ages) : NULL;
        int fill = 0;
        for (unsigned i = 0; guest && i < 64u && oracles_guest_add_half_listener(guest, no_listener, (void *)(uintptr_t)(i + 1u)) == 0; i++) fill++;
        CHECK(guest && fill > 0 && oracles_guest_arm_language(guest) == -1 && !strcmp(oracles_guest_language(guest), ""));
        CHECK(guest && oracles_guest_arm_text_box(guest, 128u) == -1 && !oracles_guest_text_box_armed(guest));
        /* A ghost's guest: armed all the same. */
        oracles_guest_watch_half(guest);
        CHECK(guest && oracles_guest_arm_language(guest) == 0 && !strcmp(oracles_guest_language(guest), "en"));
        oracles_guest_detach(guest);
        oracles_core_destroy(core);
        free(image);
    }
}

int main(void)
{
    uint8_t *image = make_image();
    CHECK(image != NULL);
    if (!image) return 1;
    hooks_on(ORACLES_CORE_SAMEBOY, image);
    hooks_on(ORACLES_CORE_MGBA, image);
    readers_on(image);
    half_switch_on(ORACLES_CORE_SAMEBOY);
    half_switch_on(ORACLES_CORE_MGBA);
    half_follow_on(ORACLES_CORE_SAMEBOY);
    half_follow_on(ORACLES_CORE_MGBA);
    half_features_on(ORACLES_CORE_SAMEBOY);
    half_features_on(ORACLES_CORE_MGBA);
    text_follows_on(ORACLES_CORE_SAMEBOY);
    text_follows_on(ORACLES_CORE_MGBA);
    hotkeys_follow_on(ORACLES_CORE_SAMEBOY);
    hotkeys_follow_on(ORACLES_CORE_MGBA);
    free(image);
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_guest_space: ok\n");
    return 0;
}
