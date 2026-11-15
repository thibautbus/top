/* The instant text (guest.h): each line of the text box whole, as a player pressing A at its start gets it.
 *
 * updateCharacterDisplayTimer (textbox.s), which the text's states 2 and 4 call before each character and states 3 and 9
 * between two lines, loads wKeysJustPressed in a and tests A and B: on a press it sets bit 0 of w7d0c1, and
 * displayNextTextCharacter writes the rest of the line in the same frame, looping on itself while the bit is set, then
 * plays the text's sound once.  The bit stays set to the page's wait (state 5), which clears it on the A that turns the
 * page.  While w7TextSlowdownTimer runs (\slow, the essences), the routine does not reach the test.  The transaction adds
 * A at the test, before it runs, where no A or B is pressed.
 *
 * The loop skips @readSubsequentLineBuffers, and with it a character's sound effect (w7LineSoundEffectsBuffer: a Goron's,
 * a Tokay's, Ricky's voice), which the game plays on the character it is on.  At the loop's `bit 0,a`, a loaded from
 * w7d0c1 after the character just written, the transaction takes bit 0 away from a when that character has one: the loop
 * stops there, the game plays its sound and its voice, and the next frame's test starts the loop again.
 *
 * Both answer in a register, before the instruction that reads it; nothing is written in memory. */
#include "guest_internal.h"

uint8_t oracles_guest_text_skip_press(uint8_t keys_just_pressed)
{
    return keys_just_pressed & (ORACLES_GAME_BUTTON_A | ORACLES_GAME_BUTTON_B) ? 0u : ORACLES_GAME_BUTTON_A;
}

uint8_t oracles_guest_text_skip_flags(uint8_t flags, uint8_t sound_effect)
{
    return sound_effect ? (uint8_t)(flags & ~1u) : flags;
}

uint16_t oracles_guest_find_text_skip_test(const OraclesGuestTables *t, const uint8_t *rom, size_t size)
{
    if (t->keys_just_pressed.bank == ORACLES_GUEST_ABSENT) return 0;
    const uint8_t test[] = { 0xfa, (uint8_t)t->keys_just_pressed.addr, (uint8_t)(t->keys_just_pressed.addr >> 8), 0xe6,
                             ORACLES_GAME_BUTTON_A | ORACLES_GAME_BUTTON_B };   /* ld a,(wKeysJustPressed) / and BTN_A|BTN_B */
    const uint16_t at = oracles_guest_find_in_routine(rom, size, t->update_character_display_timer, test, sizeof test);
    return at ? (uint16_t)(at + 3u) : 0;
}

uint16_t oracles_guest_find_text_skip_loop(const OraclesGuestTables *t, const uint8_t *rom, size_t size)
{
    if (t->text_flags.bank == ORACLES_GUEST_ABSENT) return 0;
    const uint8_t loop[] = { 0x1e, (uint8_t)t->text_flags.addr, 0x1a, 0xcb, 0x47 };   /* ld e,<w7d0c1 / ld a,(de) / bit 0,a */
    const uint16_t at = oracles_guest_find_in_routine_within(rom, size, t->display_next_text_character, loop, sizeof loop, 0x60u);
    return at ? (uint16_t)(at + 3u) : 0;
}

static int arm_points(OraclesGuest *guest)
{
    const OraclesGuestTables *t = guest->tables;
    if (!oracles_compat_text_instant(guest->profile) || t->update_character_display_timer.bank == ORACLES_GUEST_ABSENT
        || t->display_next_text_character.bank == ORACLES_GUEST_ABSENT || t->next_text_column.bank == ORACLES_GUEST_ABSENT
        || t->line_sound_effects_buffer.bank == ORACLES_GUEST_ABSENT) return -1;
    const OraclesGuestRom rom = oracles_guest_rom(guest);
    const OraclesGuestSym test = { t->update_character_display_timer.bank, oracles_guest_find_text_skip_test(t, rom.data, rom.size) };
    const OraclesGuestSym loop = { t->display_next_text_character.bank, oracles_guest_find_text_skip_loop(t, rom.data, rom.size) };
    if (!test.addr || !loop.addr) return -1;
    if (oracles_guest_add_hook(guest, test, ORACLES_EVENT_TEXT_SKIP_TEST, 0) != 0 || oracles_guest_add_hook(guest, loop, ORACLES_EVENT_TEXT_SKIP_LOOP, 0) != 0)
        return -1;
    return 0;
}

/* The game went to the other half of its image: the points of that half's code. */
static void follow_half(void *opaque)
{
    OraclesGuest *guest = opaque;
    if (!guest->text_instant_follows) return;
    if (arm_points(guest) != 0) { guest->half_features_refused++; return; }
    guest->text_instant_armed = 1;
}

int oracles_guest_set_text_instant(OraclesGuest *guest, int on)
{
    if (!guest) return -1;
    if (on && !guest->text_instant_armed) {
        if (oracles_guest_listen_half(guest, follow_half) != 0) return -1;
        if (!guest->half_left && arm_points(guest) != 0) return -1;   /* between two halves: armed in the next */
        guest->text_instant_armed = guest->text_instant_follows = 1;
    }
    guest->text_instant = on != 0;
    return 0;
}

int oracles_guest_text_instant(const OraclesGuest *guest) { return guest && guest->text_instant; }

int oracles_guest_text_instant_armed(const OraclesGuest *guest) { return guest && guest->text_instant_armed; }

void oracles_guest_text_instant_counts(const OraclesGuest *guest, OraclesGuestTextInstantCounts *counts)
{
    memset(counts, 0, sizeof *counts);
    if (guest) *counts = guest->text_instant_counts;
}

static void set_a(OraclesGuest *guest, OraclesCoreRegisters *r, uint8_t a)
{
    r->af = (uint16_t)((r->af & 0xffu) | (uint16_t)a << 8);
    oracles_core_set_registers(guest->core, r);
}

void oracles_guest_text_skip_point(OraclesGuest *guest, OraclesGuestEventType type)
{
    if (!guest->text_instant) return;
    OraclesCoreRegisters r = oracles_core_registers(guest->core);
    const uint8_t a = (uint8_t)(r.af >> 8);
    if (type == ORACLES_EVENT_TEXT_SKIP_TEST) {
        const uint8_t add = oracles_guest_text_skip_press(a);
        if (!add) return;
        set_a(guest, &r, (uint8_t)(a | add));
        guest->text_instant_counts.presses++;
        return;
    }
    if (!(a & 1u)) return;
    /* The character just written: the column has moved past it. */
    const OraclesGuestTables *t = guest->tables;
    const uint8_t column = oracles_guest_read8(guest, t->next_text_column);
    OraclesGuestSym at = t->line_sound_effects_buffer;
    at.addr = (uint16_t)(at.addr + ((uint8_t)(column - 1u) & 0x0fu));
    const uint8_t sound_effect = oracles_guest_read8(guest, at);
    const uint8_t flags = oracles_guest_text_skip_flags(a, sound_effect);
    if (flags != a) {
        set_a(guest, &r, flags);
        guest->text_instant_counts.voices_kept++;
    }
    if (sound_effect && (flags & 1u)) guest->text_instant_counts.voices_lost++;
}
