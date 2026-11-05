/* The cheats (guest.h): each answers in a register, at the one instruction of a routine that reads it, before it runs.
 *
 * linkApplyDamage (specialObjectAnimationsAndDamage.s, bank 6) begins `ld h,d / ld l,damageToApply / ld a,(hl) /
 * ld (hl),$00 / or a / jr z`: it takes the damage the hits of the frame left in w1Link.damageToApply, clears it, and
 * adds it to w1Link.health, from which it takes wLinkHealth down, Link's death decided after.  At the `or a`, a hit's
 * damage (negative) becomes 0: the routine skips the sum, and the knockback, the sound and the invincibility frames
 * the hit set are the game's.  The holes, the water and the lava write their damage there too.
 *
 * decNumBombs (bank0.s) shares its end with decNumActiveSeeds and decNumBombchus: `ld a,(hl) / or a / ret z / call
 * setStatusBarNeedsRefreshBit1 / ld a,(hl) / sub $01 / daa / ld (hl),a / or h / ret`.  At the `ld (hl),a`, a becomes
 * the count (hl) holds: the count is written back as it was, and `or h` answers as for a decrement.  At zero the
 * routine returns before the instruction: no ammunition is made.  Ages' bomb-upgrade fairy, answered with the silver
 * bomb, takes every bomb by writing 1 and calling decNumBombs (bombUpgradeFairy_loseAllBombs, scriptHelper.s): that
 * call, its return address under SP and its bank mapped, is left to the game, the story's own loss.
 *
 * removeRupeeValue (bank0.s) is `ld hl,wNumRupees / call getRupeeValue / jp subDecimalFromHlRef`, Seasons'
 * removeOreChunkValue jumping to its call with hl on wNumOreChunks.  At the call, with hl on wNumRupees, the value
 * becomes 0, whose amount is none (getRupeeValue@rupeeValues' first word): nothing is taken away.  The ore chunks
 * are left as they are, and so is a call the call transaction made (a mod's game.pay), its return address on the
 * stack the transaction's write point. */
#include "guest_internal.h"

#include <stdio.h>

static const char *const cheat_names[ORACLES_CHEATS] = { "no-damage", "ammo", "rupees" };

const char *oracles_cheat_name(unsigned index) { return index < ORACLES_CHEATS ? cheat_names[index] : NULL; }

int oracles_cheats_parse(const char *list, const char *prefix, unsigned *cheats)
{
    const size_t skip = prefix ? strlen(prefix) : 0;
    unsigned found = 0;
    int unknown = 0;
    for (const char *p = list ? list : ""; *p;) {
        const size_t length = strcspn(p, ",");
        if (length >= skip && !strncmp(p, prefix ? prefix : "", skip)) {
            unsigned index = 0;
            while (index < ORACLES_CHEATS && (length - skip != strlen(cheat_names[index]) || strncmp(p + skip, cheat_names[index], length - skip))) index++;
            if (index == ORACLES_CHEATS) unknown = 1;
            else found |= 1u << index;
        }
        p += length + (p[length] == ',');
    }
    *cheats = found;
    return unknown ? -1 : 0;
}

const char *oracles_cheats_format(unsigned cheats, const char *prefix, char *out, size_t capacity)
{
    size_t used = 0;
    if (capacity) out[0] = 0;
    for (unsigned index = 0; index < ORACLES_CHEATS; index++) {
        if (!(cheats & 1u << index) || used >= capacity) continue;
        const int n = snprintf(out + used, capacity - used, "%s%s%s", used ? "," : "", prefix ? prefix : "", cheat_names[index]);
        used = n < 0 ? capacity : used + (size_t)n;
    }
    return out;
}

uint16_t oracles_guest_find_in_routine(const uint8_t *rom, size_t size, OraclesGuestSym routine, const uint8_t *sequence, size_t length)
{
    return oracles_guest_find_in_routine_within(rom, size, routine, sequence, length, 0x40u);
}

uint16_t oracles_guest_find_in_routine_within(const uint8_t *rom, size_t size, OraclesGuestSym routine, const uint8_t *sequence, size_t length,
                                              size_t window)
{
    if (!rom || routine.bank == ORACLES_GUEST_ABSENT || routine.addr >= 0x8000u) return 0;
    const size_t base = routine.addr < 0x4000u ? routine.addr : (size_t)routine.bank * 0x4000u + (routine.addr - 0x4000u);
    const size_t bank_end = routine.addr < 0x4000u ? 0x4000u : base + (0x8000u - routine.addr);
    uint16_t found = 0;
    unsigned count = 0;
    for (size_t i = 0; i < window && base + i + length <= size && base + i + length <= bank_end; i++) {
        if (memcmp(rom + base + i, sequence, length) != 0) continue;
        found = (uint16_t)(routine.addr + i);
        count++;
    }
    return count == 1u ? found : 0;
}

uint16_t oracles_guest_find_cheat_point(const OraclesGuestTables *t, const uint8_t *rom, size_t size, OraclesGuestCheat cheat)
{
    if (cheat == ORACLES_CHEAT_NO_DAMAGE) {
        if (t->link_damage_to_apply.bank == ORACLES_GUEST_ABSENT) return 0;
        const uint8_t read[] = { 0x62, 0x2e, (uint8_t)t->link_damage_to_apply.addr, 0x7e, 0x36, 0x00, 0xb7 };   /* ld h,d ... or a */
        const uint16_t at = oracles_guest_find_in_routine(rom, size, t->link_apply_damage, read, sizeof read);
        return at ? (uint16_t)(at + 6u) : 0;
    }
    if (cheat == ORACLES_CHEAT_AMMO) {
        const uint8_t end[] = { 0x7e, 0xd6, 0x01, 0x27, 0x77, 0xb4, 0xc9 };   /* ld a,(hl) / sub $01 / daa / ld (hl),a / or h / ret */
        const uint16_t at = oracles_guest_find_in_routine(rom, size, t->dec_num_bombs, end, sizeof end);
        return at ? (uint16_t)(at + 4u) : 0;
    }
    if (cheat == ORACLES_CHEAT_RUPEES) {
        if (t->num_rupees.bank == ORACLES_GUEST_ABSENT || t->get_rupee_value.bank != 0) return 0;
        const uint8_t call[] = { 0x21, (uint8_t)t->num_rupees.addr, (uint8_t)(t->num_rupees.addr >> 8),
                                 0xcd, (uint8_t)t->get_rupee_value.addr, (uint8_t)(t->get_rupee_value.addr >> 8) };   /* ld hl,wNumRupees / call getRupeeValue */
        const uint16_t at = oracles_guest_find_in_routine(rom, size, t->remove_rupee_value, call, sizeof call);
        return at ? (uint16_t)(at + 3u) : 0;
    }
    return 0;
}

static OraclesGuestSym cheat_routine(const OraclesGuestTables *t, unsigned index)
{
    return index == 0 ? t->link_apply_damage : index == 1 ? t->dec_num_bombs : t->remove_rupee_value;
}

/* The return address of the bomb-upgrade fairy's `call decNumBombs`, in the routine's bank; 0 when the game has no such
 * routine, -1 when its call is not found once. */
static int find_scripted_ammo(OraclesGuest *guest, const OraclesGuestRom *rom)
{
    const OraclesGuestTables *t = guest->tables;
    guest->ammo_scripted_return.bank = ORACLES_GUEST_ABSENT;
    if (t->bomb_fairy_lose_all_bombs.bank == ORACLES_GUEST_ABSENT) return 0;
    const uint8_t call[] = { 0xcd, (uint8_t)t->dec_num_bombs.addr, (uint8_t)(t->dec_num_bombs.addr >> 8) };
    const uint16_t at = oracles_guest_find_in_routine(rom->data, rom->size, t->bomb_fairy_lose_all_bombs, call, sizeof call);
    if (!at) return -1;
    guest->ammo_scripted_return.bank = t->bomb_fairy_lose_all_bombs.bank;
    guest->ammo_scripted_return.addr = (uint16_t)(at + 3u);
    return 0;
}

int oracles_guest_set_cheats(OraclesGuest *guest, unsigned cheats)
{
    if (!guest || (cheats & ~((1u << ORACLES_CHEATS) - 1u)) || (cheats && !oracles_compat_cheats(guest->profile))) return -1;
    const OraclesGuestRom rom = oracles_guest_rom(guest);
    /* The rupees' value 0 must take none away, as the game's table has it. */
    if ((cheats & ORACLES_CHEAT_RUPEES) && oracles_guest_rupee_amount(guest->tables, &rom, 0) != 0) return -1;
    if ((cheats & ORACLES_CHEAT_AMMO) && !(guest->cheats_armed & ORACLES_CHEAT_AMMO) && find_scripted_ammo(guest, &rom) != 0) return -1;
    static const OraclesGuestEventType events[ORACLES_CHEATS] = { ORACLES_EVENT_CHEAT_DAMAGE, ORACLES_EVENT_CHEAT_AMMO, ORACLES_EVENT_CHEAT_RUPEES };
    for (unsigned index = 0; index < ORACLES_CHEATS; index++) {
        if (!(cheats & 1u << index) || (guest->cheats_armed & 1u << index)) continue;
        const uint16_t point = oracles_guest_find_cheat_point(guest->tables, rom.data, rom.size, (OraclesGuestCheat)(1u << index));
        const OraclesGuestSym at = { cheat_routine(guest->tables, index).bank, point };
        if (!point || oracles_guest_add_hook(guest, at, events[index], 0) != 0) return -1;
        guest->cheats_armed |= 1u << index;
    }
    guest->cheats = cheats;
    return 0;
}

unsigned oracles_guest_cheats(const OraclesGuest *guest) { return guest ? guest->cheats : 0; }

void oracles_guest_cheat_counts(const OraclesGuest *guest, OraclesGuestCheatCounts *counts)
{
    memset(counts, 0, sizeof *counts);
    if (guest) *counts = guest->cheat_counts;
}

static void set_a(OraclesGuest *guest, OraclesCoreRegisters *r, uint8_t a)
{
    r->af = (uint16_t)((r->af & 0xffu) | (uint16_t)a << 8);
    oracles_core_set_registers(guest->core, r);
}

void oracles_guest_cheat_point(OraclesGuest *guest, OraclesGuestEventType type)
{
    OraclesCoreRegisters r = oracles_core_registers(guest->core);
    const uint8_t a = (uint8_t)(r.af >> 8);
    const OraclesGuestTables *t = guest->tables;
    if (type == ORACLES_EVENT_CHEAT_DAMAGE) {
        if (!(guest->cheats & ORACLES_CHEAT_NO_DAMAGE) || (r.de >> 8) != (t->link_damage_to_apply.addr >> 8) || !(a & 0x80u)) return;
        set_a(guest, &r, 0);
        guest->cheat_counts.hits++;
        guest->cheat_counts.health_kept += (uint8_t)-a;
    } else if (type == ORACLES_EVENT_CHEAT_AMMO) {
        const OraclesGuestSym counter = { 0, r.hl }, at_sp = { 0, r.sp };
        const uint8_t *count = oracles_guest_ptr(guest, counter, 1), *word = oracles_guest_ptr(guest, at_sp, 2);
        if (!(guest->cheats & ORACLES_CHEAT_AMMO) || !count || *count == a) return;
        uint16_t bank = 0;
        oracles_core_memory(guest->core, ORACLES_CORE_ROM, NULL, &bank);
        if (guest->ammo_scripted_return.bank != ORACLES_GUEST_ABSENT && word && (uint16_t)(word[0] | word[1] << 8) == guest->ammo_scripted_return.addr
            && bank == guest->ammo_scripted_return.bank)
            return;   /* the story's own loss (Ages' bomb-upgrade fairy); a game without it has nothing to leave */
        set_a(guest, &r, *count);
        guest->cheat_counts.ammo_kept++;
    } else if (type == ORACLES_EVENT_CHEAT_RUPEES) {
        if (!(guest->cheats & ORACLES_CHEAT_RUPEES) || r.hl != t->num_rupees.addr) return;
        const OraclesGuestSym at_sp = { 0, r.sp };
        const uint8_t *word = oracles_guest_ptr(guest, at_sp, 2);
        if (guest->calls_armed && word && (uint16_t)(word[0] | word[1] << 8) == guest->inventory_write_pc) {
            guest->cheat_counts.calls_charged++;   /* a mod pays */
            return;
        }
        const OraclesGuestRom rom = oracles_guest_rom(guest);
        const int amount = oracles_guest_rupee_amount(t, &rom, a < t->rupee_value_count ? a : (uint8_t)(t->rupee_value_count - 1u));
        if (amount <= 0) return;
        set_a(guest, &r, 0);
        guest->cheat_counts.payments++;
        guest->cheat_counts.rupees_kept += (unsigned)amount;
    }
}
