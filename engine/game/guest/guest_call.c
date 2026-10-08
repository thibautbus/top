/* The call transaction (guest.h): a mod's host scene asks the game to run one
 * of its own routines, from a closed catalogue, and never writes the game's
 * variables itself.
 *
 * mainThreadStart runs, once a frame, `call drawAllSprites`, `call
 * checkReloadStatusBarGraphics`, `call resumeThreadNextFrame` (bank0.s).  At
 * the entry of checkReloadStatusBarGraphics from that call, with a call
 * queued and the state safe, the guest notes the stack pointer; at the
 * instruction that returns from it (its own `ret z`, or the `ret` of the
 * function it tail-jumps to), the stack pointer and the return address still
 * the ones noted, the guest pushes the routine's address under the return
 * address and sets its registers: the return pops the routine, which runs and
 * returns to `call resumeThreadNextFrame` with the stack as the game left it.
 * That call reads no register, nor does the loop after it. */
#include "guest_internal.h"

/* checkReloadStatusBarGraphics and loadUncompressedGfxHeader, where it tail-jumps, return by `ret` or `ret z`; the
 * other returns are listed so that any function it may reach in another build is covered.  1 when the instruction
 * about to run returns (a conditional return whose condition holds, from the flags in F). */
static int returns_now(uint8_t opcode, uint8_t flags)
{
    const int z = (flags & 0x80u) != 0, c = (flags & 0x10u) != 0;
    switch (opcode) {
        case 0xc9u: case 0xd9u: return 1;   /* ret, reti */
        case 0xc0u: return !z;              /* ret nz */
        case 0xc8u: return z;               /* ret z */
        case 0xd0u: return !c;              /* ret nc */
        case 0xd8u: return c;               /* ret c */
        default: return 0;
    }
}

static int named(const char *const *names, unsigned count, uint8_t value)
{
    return value < count && names[value][0] != 0;
}

static int bcd(uint8_t value) { return (value >> 4) <= 9u && (value & 0x0fu) <= 9u; }

/* A symbol's `length` bytes in the ROM image, or NULL outside it: bank 0 from 0, bank n from n * $4000. */
static const uint8_t *rom_at(const OraclesGuestRom *rom, OraclesGuestSym sym, size_t length)
{
    if (!rom || !rom->data || sym.bank == ORACLES_GUEST_ABSENT) return NULL;
    size_t at;
    if (sym.bank == 0u) {
        if (sym.addr >= 0x4000u) return NULL;
        at = sym.addr;
    } else {
        if (sym.addr < 0x4000u || sym.addr >= 0x8000u) return NULL;
        at = (size_t)sym.bank * 0x4000u + (sym.addr - 0x4000u);
    }
    return at + length <= rom->size ? rom->data + at : NULL;
}

int oracles_guest_treasure_mode(const OraclesGuestTables *t, const OraclesGuestRom *rom, uint8_t treasure)
{
    if (treasure >= t->treasure_collection_behaviour_count) return -1;
    const OraclesGuestSym entry = { t->treasure_collection_behaviours.bank, (uint16_t)(t->treasure_collection_behaviours.addr + 3u * treasure) };
    const uint8_t *bytes = rom_at(rom, entry, 3u);
    return bytes ? bytes[1] & 0x0f : -1;
}

/* Whether the game itself gives `treasure` with `parameter` somewhere: the parameter byte of its entry in
 * treasureObjectData, or of each subid of the list the entry points to.  A list has no count: it ends where the next
 * list begins, or at treasure_object_lists_end for the last. */
static int game_gives(const OraclesGuestTables *t, const OraclesGuestRom *rom, uint8_t treasure, uint8_t parameter)
{
    const OraclesGuestSym table = t->treasure_object_data;
    const uint8_t *entries = rom_at(rom, table, 4u * t->treasure_object_count);
    if (!entries || treasure >= t->treasure_object_count) return 0;
    const uint8_t *entry = entries + 4u * treasure;
    if (!(entry[0] & 0x80u)) return entry[1] == parameter;
    const uint16_t start = (uint16_t)(entry[1] | entry[2] << 8);
    uint16_t end = t->treasure_object_lists_end.addr;
    if (!start || t->treasure_object_lists_end.bank != table.bank || start >= end) return 0;
    for (unsigned i = 0; i < t->treasure_object_count; i++) {
        const uint8_t *other = entries + 4u * i;
        const uint16_t next = (uint16_t)(other[1] | other[2] << 8);
        if ((other[0] & 0x80u) && next > start && next < end) end = next;
    }
    const uint8_t *list = rom_at(rom, (OraclesGuestSym){ table.bank, start }, (size_t)(end - start));
    for (uint16_t at = 0; list && at + 4u <= (uint16_t)(end - start); at = (uint16_t)(at + 4u))
        if (list[at + 1u] == parameter) return 1;
    return 0;
}

int oracles_guest_rupee_amount(const OraclesGuestTables *t, const OraclesGuestRom *rom, uint8_t value)
{
    if (value >= t->rupee_value_count) return -1;
    const uint8_t *word = rom_at(rom, (OraclesGuestSym){ t->rupee_values.bank, (uint16_t)(t->rupee_values.addr + 2u * value) }, 2u);
    if (!word) return -1;
    const unsigned bcd_word = word[0] | word[1] << 8;
    unsigned amount = 0;
    for (int shift = 12; shift >= 0; shift -= 4) {
        const unsigned digit = bcd_word >> shift & 0x0fu;
        if (digit > 9u) return -1;
        amount = amount * 10u + digit;
    }
    return (int)amount;
}

/* Whether giveTreasure applies `parameter` to `treasure` within what the game bounds itself, by the treasure's mode
 * (the low nibble of its treasureCollectionBehaviourTable entry's second byte, read in the ROM; giveTreasure_body@
 * applyParameter in treasureAndDrops.s).  Refused: the dungeon's treasures (modes 6 and 7, which index by
 * wDungeonIndex, $ff outside a dungeon), the increments without a bound (mode 2: shovel, heart pieces; mode 3: the
 * satchel's upgrade, which climbs past its table of capacities), the values set as given (mode 5: harp song, trade
 * item) and the added maximum health (mode $a).  A level or a companion (mode 8) is given only with a parameter the
 * game itself gives that treasure with somewhere (treasureObjectData: the bracelet's levels, the flute's companions);
 * health (mode $c) adds before it caps, on 8 bits: at most 20 hearts, which a life of at most 20 hearts cannot carry
 * past 255. */
static int parameter_allowed(const OraclesGuestTables *t, const OraclesGuestRom *rom, uint8_t treasure, uint8_t parameter)
{
    switch (oracles_guest_treasure_mode(t, rom, treasure)) {
        case 0x0: return parameter == 0;                             /* nothing more */
        case 0x1: return parameter < 8u;                             /* an essence's bit */
        case 0x4: case 0xd: case 0xf: return parameter && bcd(parameter);   /* ammunition in BCD, capped by the game */
        case 0x8: return parameter < 16u && game_gives(t, rom, treasure, parameter);   /* a level, or the flute's companion */
        case 0x9: return parameter < t->num_rings;                   /* a ring, unappraised */
        case 0xb: return parameter < 8u;                             /* an upgrade's bit */
        case 0xc: return parameter <= 0x50u;                         /* health, capped by the maximum after an 8-bit sum */
        case 0xe: return parameter < t->rupee_value_count;           /* a rupee value */
        default: return 0;
    }
}

int oracles_guest_call_allowed(const OraclesGuestTables *t, const OraclesGuestRom *rom, const OraclesGuestCall *call)
{
    int mode;
    switch (call->routine) {
        case ORACLES_ROUTINE_GIVE_TREASURE:
            return named(t->treasure_names, ORACLES_GUEST_TREASURES, call->a) && parameter_allowed(t, rom, call->a, call->c);
        case ORACLES_ROUTINE_LOSE_TREASURE:
            mode = oracles_guest_treasure_mode(t, rom, call->a);
            return named(t->treasure_names, ORACLES_GUEST_TREASURES, call->a) && mode != 0x6 && mode != 0x7;
        case ORACLES_ROUTINE_REMOVE_RUPEES:
            return call->a < t->rupee_value_count;
        case ORACLES_ROUTINE_PLAY_SOUND:
            return call->a && named(t->sound_names, ORACLES_GUEST_SOUNDS, call->a);
        case ORACLES_ROUTINE_SET_GLOBAL_FLAG:
        case ORACLES_ROUTINE_UNSET_GLOBAL_FLAG:
            return named(t->global_flag_names, ORACLES_GUEST_GLOBAL_FLAGS, call->a);
        default:
            return 0;
    }
}

OraclesGuestRom oracles_guest_rom(OraclesGuest *guest)
{
    OraclesGuestRom rom = { NULL, 0 };
    uint16_t bank = 0;
    if (guest && guest->core) rom.data = oracles_core_memory(guest->core, ORACLES_CORE_ROM, &rom.size, &bank);
    return rom;
}

static OraclesGuestSym routine_entry(const OraclesGuestTables *t, OraclesGuestRoutine routine)
{
    switch (routine) {
        case ORACLES_ROUTINE_GIVE_TREASURE: return t->give_treasure;
        case ORACLES_ROUTINE_LOSE_TREASURE: return t->lose_treasure;
        case ORACLES_ROUTINE_REMOVE_RUPEES: return t->remove_rupee_value;
        case ORACLES_ROUTINE_PLAY_SOUND: return t->play_sound;
        case ORACLES_ROUTINE_SET_GLOBAL_FLAG: return t->set_global_flag;
        default: return t->unset_global_flag;
    }
}

/* The return address of the one `call checkReloadStatusBarGraphics` of mainThreadStart, read in the user's ROM (as
 * the inventory transaction finds its write point, guest_inventory.c). */
static uint16_t find_call_point(OraclesGuest *guest)
{
    const OraclesGuestTables *t = guest->tables;
    size_t size = 0;
    uint16_t bank = 0;
    const uint8_t *rom = oracles_core_memory(guest->core, ORACLES_CORE_ROM, &size, &bank);
    const uint16_t start = t->main_thread_start.addr, target = t->check_reload_status_bar_graphics.addr;
    if (!rom || t->main_thread_start.bank != 0 || t->check_reload_status_bar_graphics.bank != 0 || (size_t)start + 0x80u > size || start + 0x80u > 0x4000u) return 0;
    uint16_t found = 0;
    unsigned count = 0;
    for (uint16_t at = start; at < start + 0x80u - 2u; at++)
        if (rom[at] == 0xcdu && rom[at + 1u] == (uint8_t)target && rom[at + 2u] == (uint8_t)(target >> 8)) { found = (uint16_t)(at + 3u); count++; }
    return count == 1u ? found : 0;
}

int oracles_guest_arm_calls(OraclesGuest *guest)
{
    if (!guest || !oracles_compat_item_hotkeys(guest->profile)) return -1;
    if (guest->calls_armed) return 0;
    const OraclesGuestTables *t = guest->tables;
    /* Every routine of the catalogue is a bank-0 entry, which switches banks itself. */
    for (OraclesGuestRoutine routine = ORACLES_ROUTINE_GIVE_TREASURE; routine <= ORACLES_ROUTINE_UNSET_GLOBAL_FLAG; routine++)
        if (routine_entry(t, routine).bank != 0) return -1;
    const uint16_t point = find_call_point(guest);
    if (!point || (guest->inventory_write_pc && guest->inventory_write_pc != point)) return -1;
    guest->inventory_write_pc = point;
    if (oracles_guest_add_hook(guest, t->check_reload_status_bar_graphics, ORACLES_EVENT_STATUS_BAR_CHECK, 0) != 0) return -1;
    guest->calls_armed = 1;
    return 0;
}

int oracles_guest_queue_call(OraclesGuest *guest, const OraclesGuestCall *call, unsigned *serial)
{
    if (!guest || !call || !guest->calls_armed || guest->call_count >= ORACLES_GUEST_CALLS) return -1;
    const OraclesGuestRom rom = oracles_guest_rom(guest);
    if (!oracles_guest_call_allowed(guest->tables, &rom, call)) return -1;
    if (serial) *serial = guest->calls_done + guest->call_count;
    guest->calls[guest->call_count++] = *call;
    return 0;
}

unsigned oracles_guest_calls_pending(const OraclesGuest *guest) { return guest ? guest->call_count : 0; }
unsigned oracles_guest_calls_done(const OraclesGuest *guest) { return guest ? guest->calls_done : 0; }

void oracles_guest_call_point(OraclesGuest *guest, uint16_t sp)
{
    if (!guest->calls_armed || !guest->call_count || guest->call_watch_sp) return;
    const OraclesGuestSym at_sp = { 0, sp };
    const uint8_t *word = oracles_guest_ptr(guest, at_sp, 2);
    if (!word || (uint16_t)(word[0] | (word[1] << 8)) != guest->inventory_write_pc) return;   /* not the call of mainThreadStart */
    OraclesGuestInventoryState state;
    oracles_guest_inventory_state(guest, &state);
    if (oracles_guest_inventory_refusal(&state) != ORACLES_INVENTORY_APPLIED) return;   /* the call waits for a safe state */
    guest->call_watch_sp = sp;
}

void oracles_guest_call_step(OraclesGuest *guest, uint16_t sp, uint8_t opcode)
{
    if (sp > guest->call_watch_sp) { guest->call_watch_sp = 0; return; }   /* returned some other way: try again next frame */
    OraclesCoreRegisters r = oracles_core_registers(guest->core);
    if (sp != guest->call_watch_sp || !returns_now(opcode, (uint8_t)r.af)) return;
    const OraclesGuestSym at_sp = { 0, sp };
    const uint8_t *word = oracles_guest_ptr(guest, at_sp, 2);
    guest->call_watch_sp = 0;
    if (!word || (uint16_t)(word[0] | (word[1] << 8)) != guest->inventory_write_pc) return;
    const OraclesGuestCall call = guest->calls[0];
    const OraclesGuestSym routine = routine_entry(guest->tables, call.routine);
    uint8_t *wram = oracles_guest_wram_writable(guest, 0);
    const uint16_t below = (uint16_t)(sp - 2u);
    if (!wram || below < 0xc000u || below > 0xcffeu) return;
    wram[below - 0xc000u] = (uint8_t)routine.addr;
    wram[below - 0xc000u + 1u] = (uint8_t)(routine.addr >> 8);
    r.sp = below;
    r.af = (uint16_t)((call.a << 8) | (r.af & 0xffu));
    r.bc = (uint16_t)((r.bc & 0xff00u) | call.c);
    oracles_core_set_registers(guest->core, &r);
    memmove(&guest->calls[0], &guest->calls[1], (guest->call_count - 1u) * sizeof guest->calls[0]);
    guest->call_count--;
    guest->calls_done++;
}
