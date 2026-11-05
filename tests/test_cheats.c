/* The cheats (guest.h): their names, the search of their points in a ROM, and their answers on a synthetic ROM (no game
 * data) on both cores.  At each routine's address of the game's tables, in its bank,
 *   linkApplyDamage:  ld a,($c0f0) ; ld d,a ; ld a,($c0f2) ; ld h,d ; ld l,<damageToApply ; ld (hl),a ;
 *                     ld h,d ; ld l,<damageToApply ; ld a,(hl) ; ld (hl),$00 ; or a ; jr z,+ ; ld ($c000),a ; + ret
 *   decNumBombs:      ld hl,$c010 ; ld a,(hl) ; or a ; ret z ; ld a,(hl) ; sub $01 ; daa ; ld (hl),a ; or h ; ret
 *   removeRupeeValue: ld hl,wNumRupees ; call getRupeeValue ; ret,   getRupeeValue: ld ($c001),a ; ret
 *   (Ages) bombUpgradeFairy_loseAllBombs: ld a,($c011) ; ld ($c010),a ; call decNumBombs ; xor a ; ld ($c011),a ; ret
 * called in a loop, decNumBombs alone or through the fairy's routine as $c0f3 says, in the bank $c0f4 names (its own,
 * or another holding the same code at the same address, a call the cheat answers), removeRupeeValue with a = 4 ($10
 * rupees), or through an ore chunks' entry (hl $c020), or returning to the call transaction's write point
 * (mainThreadStart's `call checkReloadStatusBarGraphics`) as a mod's payment does, as $c0f1 says: what each routine saw
 * is in $c000, $c010 and $c001. */
#include "core.h"
#include "guest.h"
#include "guest_struct_offsets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

#define ALL (ORACLES_CHEAT_NO_DAMAGE | ORACLES_CHEAT_AMMO | ORACLES_CHEAT_RUPEES)

static void test_names(void)
{
    unsigned cheats = 99;
    CHECK(oracles_cheats_parse("", NULL, &cheats) == 0 && cheats == 0);
    CHECK(oracles_cheats_parse("no-damage,ammo,rupees", NULL, &cheats) == 0 && cheats == ALL);
    CHECK(oracles_cheats_parse("rupees", NULL, &cheats) == 0 && cheats == ORACLES_CHEAT_RUPEES);
    CHECK(oracles_cheats_parse("no-damage,speed-150", NULL, &cheats) == -1 && cheats == ORACLES_CHEAT_NO_DAMAGE);   /* a cheat this build does not know */
    CHECK(oracles_cheats_parse("no-damag", NULL, &cheats) == -1);
    /* A route's options: the other options skipped, a cheat named after the prefix. */
    CHECK(oracles_cheats_parse("continuous-transitions,cheat-ammo,swim-hold,cheat-no-damage", "cheat-", &cheats) == 0
          && cheats == (ORACLES_CHEAT_AMMO | ORACLES_CHEAT_NO_DAMAGE));
    CHECK(oracles_cheats_parse("continuous-transitions", "cheat-", &cheats) == 0 && cheats == 0);
    CHECK(oracles_cheats_parse("cheat-speed-150", "cheat-", &cheats) == -1);
    char out[64];
    CHECK(!strcmp(oracles_cheats_format(ALL, NULL, out, sizeof out), "no-damage,ammo,rupees"));
    CHECK(!strcmp(oracles_cheats_format(ORACLES_CHEAT_RUPEES | ORACLES_CHEAT_NO_DAMAGE, "cheat-", out, sizeof out), "cheat-no-damage,cheat-rupees"));
    CHECK(!strcmp(oracles_cheats_format(0, "cheat-", out, sizeof out), ""));
    CHECK(oracles_cheats_parse(oracles_cheats_format(ALL, "cheat-", out, sizeof out), "cheat-", &cheats) == 0 && cheats == ALL);
    CHECK(!strcmp(oracles_cheat_name(0), "no-damage") && oracles_cheat_name(ORACLES_CHEATS) == NULL);
}

static void put(uint8_t *rom, size_t at, const uint8_t *bytes, size_t count) { memcpy(rom + at, bytes, count); }

static void test_find(const OraclesGuestTables *t)
{
    const size_t size = 1024u * 1024u;
    uint8_t *rom = calloc(size, 1);
    if (!rom) { failures++; return; }
    const uint8_t damage[] = { 0x62, 0x2e, (uint8_t)t->link_damage_to_apply.addr, 0x7e, 0x36, 0x00, 0xb7 };
    const uint8_t ammo[] = { 0x7e, 0xd6, 0x01, 0x27, 0x77, 0xb4, 0xc9 };
    const uint8_t rupees[] = { 0x21, (uint8_t)t->num_rupees.addr, (uint8_t)(t->num_rupees.addr >> 8),
                               0xcd, (uint8_t)t->get_rupee_value.addr, (uint8_t)(t->get_rupee_value.addr >> 8) };
    const size_t damage_at = oracles_rom_offset(t->link_apply_damage.bank, t->link_apply_damage.addr);
    const size_t ammo_at = t->dec_num_bombs.addr, rupees_at = t->remove_rupee_value.addr;
    CHECK(oracles_guest_find_cheat_point(t, rom, size, ORACLES_CHEAT_NO_DAMAGE) == 0);   /* none */
    CHECK(oracles_guest_find_cheat_point(t, rom, size, ORACLES_CHEAT_AMMO) == 0);
    CHECK(oracles_guest_find_cheat_point(t, rom, size, ORACLES_CHEAT_RUPEES) == 0);
    put(rom, damage_at, damage, sizeof damage);   /* where the games have them */
    put(rom, ammo_at + 6u, ammo, sizeof ammo);
    put(rom, rupees_at, rupees, sizeof rupees);
    CHECK(oracles_guest_find_cheat_point(t, rom, size, ORACLES_CHEAT_NO_DAMAGE) == t->link_apply_damage.addr + 6u);
    CHECK(oracles_guest_find_cheat_point(t, rom, size, ORACLES_CHEAT_AMMO) == t->dec_num_bombs.addr + 10u);
    CHECK(oracles_guest_find_cheat_point(t, rom, size, ORACLES_CHEAT_RUPEES) == t->remove_rupee_value.addr + 3u);
    put(rom, damage_at + 0x20u, damage, sizeof damage);   /* twice: none taken */
    CHECK(oracles_guest_find_cheat_point(t, rom, size, ORACLES_CHEAT_NO_DAMAGE) == 0);
    memset(rom + damage_at + 0x20u, 0, sizeof damage);
    put(rom, damage_at + 0x40u, damage, sizeof damage);   /* past the routine's first $40 bytes: not looked at */
    CHECK(oracles_guest_find_cheat_point(t, rom, size, ORACLES_CHEAT_NO_DAMAGE) == t->link_apply_damage.addr + 6u);
    rom[ammo_at + 6u + 2u] = 0x02;   /* another routine: sub $02 */
    CHECK(oracles_guest_find_cheat_point(t, rom, size, ORACLES_CHEAT_AMMO) == 0);
    rom[rupees_at + 1u] ^= 0x01;   /* another counter: the ore chunks' */
    CHECK(oracles_guest_find_cheat_point(t, rom, size, ORACLES_CHEAT_RUPEES) == 0);
    CHECK(oracles_guest_find_cheat_point(t, rom, damage_at + 4u, ORACLES_CHEAT_NO_DAMAGE) == 0);   /* an image too short */
    free(rom);
}

static uint8_t *emit16(uint8_t *at, uint8_t opcode, unsigned value)
{
    *at++ = opcode;
    *at++ = (uint8_t)value;
    *at++ = (uint8_t)(value >> 8);
    return at;
}

static void jr_to(uint8_t *at, uint8_t opcode, const uint8_t *target)
{
    at[0] = opcode;
    at[1] = (uint8_t)(target - (at + 2));
}

#define OTHER_BANK 0x3fu   /* a bank the game's routines of the test do not use */

static uint8_t *synthetic_rom(const OraclesGuestTables *t, OraclesGame game, size_t size)
{
    uint8_t *rom = calloc(size, 1);
    if (!rom) return NULL;
    memcpy(rom + 0x134, game == ORACLES_GAME_AGES ? "ZELDA NAYRU" : "ZELDA DIN", game == ORACLES_GAME_AGES ? 11 : 9);
    rom[0x143] = 0xc0; rom[0x147] = 0x1b; rom[0x148] = 0x05; rom[0x149] = 0x02;
    rom[0x100] = 0x00; rom[0x101] = 0xc3; rom[0x102] = 0x50; rom[0x103] = 0x01;   /* nop ; jp $0150 */
    const unsigned ore = 0x0300u, written = t->main_thread_start.addr + 3u;
    uint8_t *at = rom + 0x150;
    at = emit16(at, 0x31, 0xc200);                                                 /* ld sp,$c200 */
    *at++ = 0x3e; *at++ = t->link_apply_damage.bank; at = emit16(at, 0xea, 0x2000);   /* ld a,bank ; ld ($2000),a */
    uint8_t *loop = at;
    at = emit16(at, 0xcd, t->link_apply_damage.addr);                             /* call linkApplyDamage */
    at = emit16(at, 0xfa, 0xc0f3);                                                 /* ld a,($c0f3) */
    *at++ = 0xb7;                                                                  /* or a */
    uint8_t *to_fairy = at; at += 2;                                               /* jr nz,fairy */
    at = emit16(at, 0xcd, t->dec_num_bombs.addr);                                 /* call decNumBombs */
    uint8_t *to_after = at; at += 2;                                               /* jr after */
    jr_to(to_fairy, 0x20, at);
    const int fairy = t->bomb_fairy_lose_all_bombs.bank != ORACLES_GUEST_ABSENT;
    at = emit16(at, 0xfa, 0xc0f4); at = emit16(at, 0xea, 0x2000);                  /* fairy: ld a,($c0f4) ; ld ($2000),a */
    at = emit16(at, 0xcd, fairy ? t->bomb_fairy_lose_all_bombs.addr : t->dec_num_bombs.addr);
    *at++ = 0x3e; *at++ = t->link_apply_damage.bank; at = emit16(at, 0xea, 0x2000);
    jr_to(to_after, 0x18, at);
    at = emit16(at, 0xfa, 0xc0f1);                                                 /* ld a,($c0f1) */
    *at++ = 0xfe; *at++ = 0x01;                                                    /* cp 1 */
    uint8_t *to_ore = at; at += 2;                                                 /* jr z,ore */
    *at++ = 0xfe; *at++ = 0x02;                                                    /* cp 2 */
    uint8_t *to_mod = at; at += 2;                                                 /* jr z,mod */
    *at++ = 0x3e; *at++ = 0x04;                                                    /* ld a,4 */
    at = emit16(at, 0xcd, t->remove_rupee_value.addr);                            /* call removeRupeeValue */
    jr_to(at, 0x18, loop); at += 2;
    jr_to(to_ore, 0x28, at);
    *at++ = 0x3e; *at++ = 0x04;                                                    /* ore: ld a,4 */
    at = emit16(at, 0xcd, ore);                                                    /* call the ore chunks' entry */
    jr_to(at, 0x18, loop); at += 2;
    jr_to(to_mod, 0x28, at);
    at = emit16(at, 0x21, written);                                                /* mod: ld hl,the write point */
    *at++ = 0xe5; *at++ = 0x3e; *at++ = 0x04;                                      /* push hl ; ld a,4 */
    emit16(at, 0xc3, t->remove_rupee_value.addr);                                 /* jp removeRupeeValue */
    /* mainThreadStart's call, the transaction's write point after it, which goes back to the loop. */
    emit16(emit16(rom + t->main_thread_start.addr, 0xcd, t->check_reload_status_bar_graphics.addr), 0xc3, (unsigned)(loop - rom));
    emit16(emit16(rom + ore, 0x21, 0xc020), 0xc3, t->remove_rupee_value.addr + 3u);   /* ld hl,$c020 ; jp removeRupeeValue's call */
    at = rom + oracles_rom_offset(t->link_apply_damage.bank, t->link_apply_damage.addr);
    const uint8_t damage[] = { 0xfa, 0xf0, 0xc0, 0x57, 0xfa, 0xf2, 0xc0, 0x62, 0x2e, (uint8_t)t->link_damage_to_apply.addr, 0x77,
                               0x62, 0x2e, (uint8_t)t->link_damage_to_apply.addr, 0x7e, 0x36, 0x00, 0xb7, 0x28, 0x03, 0xea, 0x00, 0xc0, 0xc9 };
    memcpy(at, damage, sizeof damage);
    const uint8_t ammo[] = { 0x21, 0x10, 0xc0, 0x7e, 0xb7, 0xc8, 0x7e, 0xd6, 0x01, 0x27, 0x77, 0xb4, 0xc9 };
    memcpy(rom + t->dec_num_bombs.addr, ammo, sizeof ammo);
    if (fairy) {
        const uint8_t lose_all[] = { 0xfa, 0x11, 0xc0, 0xea, 0x10, 0xc0, 0xcd, (uint8_t)t->dec_num_bombs.addr, (uint8_t)(t->dec_num_bombs.addr >> 8),
                                     0xaf, 0xea, 0x11, 0xc0, 0xc9 };
        memcpy(rom + oracles_rom_offset(t->bomb_fairy_lose_all_bombs.bank, t->bomb_fairy_lose_all_bombs.addr), lose_all, sizeof lose_all);
        memcpy(rom + oracles_rom_offset(OTHER_BANK, t->bomb_fairy_lose_all_bombs.addr), lose_all, sizeof lose_all);
    }
    at = emit16(emit16(rom + t->remove_rupee_value.addr, 0x21, t->num_rupees.addr), 0xcd, t->get_rupee_value.addr);
    *at = 0xc9;
    at = emit16(rom + t->get_rupee_value.addr, 0xea, 0xc001);                      /* ld ($c001),a ; ret */
    *at = 0xc9;
    rom[t->rupee_values.addr + 8u] = 0x10;                                         /* the value 4: $0010 rupees in BCD */
    return rom;
}

static void test_transaction(OraclesCoreKind kind, OraclesGame game)
{
    const OraclesGuestTables *t = game == ORACLES_GAME_AGES ? &oracles_guest_tables_ages : &oracles_guest_tables_seasons;
    const size_t size = 1024u * 1024u;
    uint8_t *rom = synthetic_rom(t, game, size);
    if (!rom) { failures++; return; }
    const OraclesCoreOptions options = { 0, 0, kind, 0 };
    OraclesCore *core = oracles_core_create(rom, size, &options);
    free(rom);
    CHECK(core != NULL);
    if (!core) return;
    const OraclesRomInfo info = { .game = game, .revision = game == ORACLES_GAME_AGES ? ORACLES_ROM_REVISION_AGES_US : ORACLES_ROM_REVISION_SEASONS_US,
                                  .known = 1 };
    OraclesGuest *guest = oracles_guest_attach(core, oracles_compat_find(&info));
    CHECK(guest && oracles_guest_arm_calls(guest) == 0);
    if (!guest) { oracles_core_destroy(core); return; }
    uint8_t *wram0 = oracles_guest_wram_writable(guest, 0);
    const struct { unsigned cheats; uint8_t object, payment, fairy; } cases[] = {
        { 0, 0xd0, 0, 0 }, { ALL, 0xd0, 0, 0 }, { ORACLES_CHEAT_NO_DAMAGE, 0xd0, 0, 0 }, { ORACLES_CHEAT_AMMO, 0xd0, 0, 0 },
        { ORACLES_CHEAT_RUPEES, 0xd0, 0, 0 },
        { ALL, 0xd1, 1, 0 },   /* another object than Link's; the ore chunks */
        { ALL, 0xd0, 2, 1 },   /* a payment of the call transaction; the fairy taking every bomb (Ages) */
        { ALL, 0xd0, 0, 2 },   /* the fairy's call at its address, in another bank: answered (Ages) */
        { 0, 0xd0, 2, 0 },     /* every cheat off again: the points armed answer nothing */
    };
    const int fairy = t->bomb_fairy_lose_all_bombs.bank != ORACLES_GUEST_ABSENT;
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        CHECK(oracles_guest_set_cheats(guest, cases[i].cheats) == 0 && oracles_guest_cheats(guest) == cases[i].cheats);
        wram0[0xf0] = cases[i].object;
        wram0[0xf2] = 0xfc;   /* a hit of a heart, at every call */
        wram0[0xf1] = cases[i].payment;
        wram0[0xf3] = (uint8_t)(cases[i].fairy && fairy);
        wram0[0xf4] = (uint8_t)(!fairy ? 0u : cases[i].fairy == 2 ? OTHER_BANK : t->bomb_fairy_lose_all_bombs.bank);
        /* A frame for the loop to take the case (a frame may end within a routine), the boot ROM first. */
        for (unsigned f = 0; f < (i ? 1u : 200u); f++) oracles_core_run_frame(core);
        wram0[0x00] = wram0[0x01] = 0x55;
        wram0[0x10] = 0x10;
        wram0[0x11] = 0x01;   /* the fairy's one bomb, once */
        OraclesGuestCheatCounts before, after;
        oracles_guest_cheat_counts(guest, &before);
        for (unsigned f = 0; f < 2u; f++) {
            if (cases[i].fairy == 2) wram0[0x11] = 0x01;
            oracles_core_run_frame(core);
        }
        oracles_guest_cheat_counts(guest, &after);
        const int no_damage = (cases[i].cheats & ORACLES_CHEAT_NO_DAMAGE) && cases[i].object == 0xd0;
        CHECK(wram0[0x00] == (no_damage ? 0x55 : 0xfc));
        CHECK(no_damage ? after.hits - before.hits > 100 : after.hits == before.hits);
        CHECK(after.health_kept - before.health_kept == 4u * (after.hits - before.hits));
        const int ammo = (cases[i].cheats & ORACLES_CHEAT_AMMO) && !wram0[0xf3];
        if (wram0[0xf3] && cases[i].fairy == 2) {
            /* Its one bomb kept at each frame's call, whatever the routine was doing when the frame began. */
            CHECK(after.ammo_kept - before.ammo_kept >= 1u);
        } else {
            CHECK(wram0[0x10] == (ammo ? 0x10 : 0x00));
            CHECK(ammo ? after.ammo_kept - before.ammo_kept > 100 : after.ammo_kept == before.ammo_kept);
        }
        const int rupees = (cases[i].cheats & ORACLES_CHEAT_RUPEES) && cases[i].payment == 0;
        CHECK(wram0[0x01] == (rupees ? 0x00 : 0x04));
        CHECK(rupees ? after.payments - before.payments > 100 : after.payments == before.payments);
        CHECK(after.rupees_kept - before.rupees_kept == 10u * (after.payments - before.payments));
        const int charged = (cases[i].cheats & ORACLES_CHEAT_RUPEES) && cases[i].payment == 2;
        CHECK(charged ? after.calls_charged - before.calls_charged > 100 : after.calls_charged == before.calls_charged);
    }
    oracles_guest_detach(guest);
    oracles_core_destroy(core);
}

/* A profile that does not qualify the cheats (Ages' without them): refused, nothing armed. */
static void test_refused(void)
{
    const size_t size = 1024u * 1024u;
    uint8_t *rom = synthetic_rom(&oracles_guest_tables_ages, ORACLES_GAME_AGES, size);
    if (!rom) { failures++; return; }
    const OraclesCoreOptions options = { 0, 0, ORACLES_CORE_SAMEBOY, 0 };
    OraclesCore *core = oracles_core_create(rom, size, &options);
    free(rom);
    if (!core) { failures++; return; }
    const OraclesRomInfo info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_AGES_US, .known = 1 };
    OraclesCompatProfile without = *oracles_compat_find(&info);
    without.cheats = 0;
    OraclesGuest *guest = oracles_guest_attach(core, &without);
    CHECK(guest != NULL && !oracles_compat_cheats(&without));
    if (guest) {
        CHECK(oracles_guest_set_cheats(guest, ORACLES_CHEAT_NO_DAMAGE) == -1 && oracles_guest_cheats(guest) == 0);
        CHECK(oracles_guest_set_cheats(guest, 0) == 0);
        oracles_guest_detach(guest);
    }
    oracles_core_destroy(core);
}

int main(void)
{
    test_names();
    test_find(&oracles_guest_tables_ages);
    test_find(&oracles_guest_tables_seasons);
    test_transaction(ORACLES_CORE_SAMEBOY, ORACLES_GAME_AGES);
    test_transaction(ORACLES_CORE_MGBA, ORACLES_GAME_AGES);
    test_transaction(ORACLES_CORE_SAMEBOY, ORACLES_GAME_SEASONS);
    test_transaction(ORACLES_CORE_MGBA, ORACLES_GAME_SEASONS);
    test_refused();
    if (failures) fprintf(stderr, "%d failure(s)\n", failures);
    else printf("cheats: ok\n");
    return failures ? 1 : 0;
}
