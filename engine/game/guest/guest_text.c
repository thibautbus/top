/* The language transaction (guest.h): at the return of getTextAddress, the language's text in place of the US one.
 *
 * getTextAddress (textbox.s) ends with the text's CPU address in hl and its bank in a and w7ActiveBank; its callers
 * read the text through readByteFromW7ActiveBank, which maps that bank at $4000.  The index the composition wrote at
 * the start of the profile's first language bank (ORACLES_IMAGE_LANGUAGE_BANK in an original, oracles_rom.h; a bank a
 * fan game leaves empty, profile.h) gives, per internal group and index, the bank and address of the language's text
 * in the image: the transaction writes them in hl, a and w7ActiveBank, at the instruction the
 * routine returns to, before the caller reads them.  The index is read in the core's own copy of the ROM, in the half
 * the game runs in (its banks relative to it, as w7ActiveBank's). */
#include "guest_internal.h"

#include "oracles_rom.h"
#include "oracles_text.h"

/* A Japanese name in capitals (guest.h).  The text's command that prints a name reads it a character at a time
 * (handleTextControlCode@controlCodeA, textbox.s: `ldi a,(hl) / or a / jr z / call setLineTextBuffers / call
 * retrieveTextCharacter`), from the address nameAddressTable gives: Link's name, the child's, or a secret, which the
 * point leaves alone; the file select copies a string a character at a time (copyTextCharactersFromHl, bank2.s: `ldi
 * a,(hl)`, then `inc c / ldi a,(hl)` for a kanji, then `call copyTextCharacterGfx`), from its files' names in WRAM bank 4
 * (w4NameBuffer, a name in each slot of 6 bytes: the files' three, the name being typed in the first), its other
 * strings, the secrets' or the ROM's.  At each call, a letter read from a name, c 0 at the file select (not a kanji's
 * index), is taken into capitals in a, before the call that keeps it or draws it, when its name is Latin: when the name
 * holds a Latin capital ($41-$5a), which the Japanese name entry never types (kana, ー and the space alone); a name
 * without one is in kana, left as it is.  The name is read whole where the game keeps it (wLinkName, wKidName; the
 * slot of w4NameBuffer, within it), nothing written in memory but what the game writes of it. */
#define NAME_LETTERS 5u   /* a name's letters, its terminator after them (fileManagement.s) */
#define NAME_SLOT 6u      /* a name and its terminator in w4NameBuffer (loadFileDisplayVariables) */

uint8_t oracles_guest_name_capital(uint8_t character)
{
    return character >= 'a' && character <= 'z' ? (uint8_t)(character - 'a' + 'A') : character;
}

int oracles_guest_name_latin(const uint8_t *name, size_t length)
{
    for (size_t i = 0; i < length; i++) if (name[i] >= 'A' && name[i] <= 'Z') return 1;
    return 0;
}

unsigned oracles_guest_name_capitals(const OraclesGuest *guest) { return guest ? guest->name_capitals : 0; }
unsigned oracles_guest_name_kana(const OraclesGuest *guest) { return guest ? guest->name_kana : 0; }

static int within(OraclesGuestSym from, uint16_t at, unsigned bytes) { return at >= from.addr && at < from.addr + bytes; }

void oracles_guest_name_point(OraclesGuest *guest, OraclesGuestEventType type)
{
    const OraclesGuestTables *t = guest->tables;
    OraclesCoreRegisters r = oracles_core_registers(guest->core);
    const uint8_t a = (uint8_t)(r.af >> 8), capital = oracles_guest_name_capital(a);
    const uint16_t from = (uint16_t)(r.hl - 1u);
    if (capital == a) return;
    const uint8_t *name = NULL;
    size_t length = 0;
    if (type == ORACLES_EVENT_NAME_TEXT_CHARACTER) {
        const OraclesGuestSym *of = within(t->link_name, from, NAME_LETTERS) ? &t->link_name : within(t->kid_name, from, NAME_LETTERS) ? &t->kid_name : NULL;
        const uint8_t *wram = of ? oracles_guest_wram(guest, 0) : NULL;
        if (!wram || of->addr < 0xc000u || of->addr + NAME_LETTERS > 0xd000u) return;
        name = wram + (of->addr - 0xc000u);
        length = NAME_LETTERS;
    } else {
        const uint8_t *io = oracles_guest_io(guest);
        const unsigned bank = io ? (io[0x70] & 7u ? io[0x70] & 7u : 1u) : 0u;
        const unsigned buffer = t->name_buffer.addr, end = t->name_buffer_end.addr;
        if ((r.bc & 0xffu) || bank != t->name_buffer.bank || !within(t->name_buffer, from, end - buffer) || buffer < 0xd000u || end > 0xe000u) return;
        const unsigned slot = buffer + (from - buffer) / NAME_SLOT * NAME_SLOT;
        const uint8_t *wram = oracles_guest_wram(guest, bank);
        if (!wram) return;
        name = wram + (slot - 0xd000u);
        length = end - slot < NAME_SLOT ? end - slot : NAME_SLOT;
    }
    if (!oracles_guest_name_latin(name, length)) { guest->name_kana++; return; }
    r.af = (uint16_t)(capital << 8 | (r.af & 0xffu));
    oracles_core_set_registers(guest->core, &r);
    guest->name_capitals++;
}

/* The two points, where the image's code is the one the tables name: the call of setLineTextBuffers in the text's
 * command, the call that follows `inc c / ldi a,(hl)` in the file select's copy.  Absent from a game without them (a
 * fan game, Oracles Combined: neither plays in Japanese). */
int oracles_guest_find_name_points(const OraclesGuestTables *t, const uint8_t *rom, size_t size, uint16_t *text, uint16_t *file)
{
    *text = *file = 0;
    if (t->text_control_code_name.bank == ORACLES_GUEST_ABSENT || t->copy_text_characters_from_hl.bank == ORACLES_GUEST_ABSENT
        || t->set_line_text_buffers.bank == ORACLES_GUEST_ABSENT || t->link_name.bank == ORACLES_GUEST_ABSENT
        || t->kid_name.bank == ORACLES_GUEST_ABSENT || t->name_buffer.bank == ORACLES_GUEST_ABSENT || t->name_buffer_end.bank == ORACLES_GUEST_ABSENT) return 0;
    const uint8_t call[] = { 0xcd, (uint8_t)t->set_line_text_buffers.addr, (uint8_t)(t->set_line_text_buffers.addr >> 8) };   /* call setLineTextBuffers */
    const uint8_t copy[] = { 0x0c, 0x2a, 0xcd };   /* inc c / ldi a,(hl) / call copyTextCharacterGfx */
    const uint16_t at_copy = oracles_guest_find_in_routine(rom, size, t->copy_text_characters_from_hl, copy, sizeof copy);
    *text = oracles_guest_find_in_routine(rom, size, t->text_control_code_name, call, sizeof call);
    *file = at_copy ? (uint16_t)(at_copy + 2u) : 0;
    return *text && *file ? 1 : -1;
}

static int arm_names(OraclesGuest *guest, const uint8_t *rom, size_t size)
{
    const OraclesGuestTables *t = guest->tables;
    uint16_t text = 0, file = 0;
    const int found = oracles_guest_find_name_points(t, rom, size, &text, &file);
    if (found <= 0) return found;
    const OraclesGuestSym text_point = { t->text_control_code_name.bank, text }, file_point = { t->copy_text_characters_from_hl.bank, file };
    if (oracles_guest_add_hook(guest, text_point, ORACLES_EVENT_NAME_TEXT_CHARACTER, 0) != 0
        || oracles_guest_add_hook(guest, file_point, ORACLES_EVENT_NAME_FILE_CHARACTER, 0) != 0) return -1;
    return 0;
}

int oracles_guest_arm_language(OraclesGuest *guest)
{
    const OraclesGuestRom image = oracles_guest_rom(guest);
    const uint8_t *rom = image.data;
    const size_t size = image.size;
    const size_t at = (size_t)oracles_compat_language_bank(guest->profile) * 0x4000u;
    if (!rom || !at || size < at + 0x4000u || memcmp(rom + at, ORACLES_LANGUAGE_INDEX_MAGIC, ORACLES_LANGUAGE_INDEX_MAGIC_BYTES)) return 0;
    const uint8_t *index = rom + at;
    const OraclesGuestTables *t = guest->tables;
    if (index[16] != ORACLES_LANGUAGE_INDEX_VERSION || t->get_text_address.bank == ORACLES_GUEST_ABSENT || t->active_text_bank.bank != 7u) return -1;
    if (oracles_guest_add_hook(guest, t->get_text_address, 0, ORACLES_EVENT_TEXT_ADDRESS) != 0
        || oracles_guest_text_follow_half(guest, ORACLES_GUEST_TEXT_LANGUAGE) != 0) return -1;
    guest->language_index = index;
    memcpy(guest->language, index + ORACLES_LANGUAGE_INDEX_CODE, ORACLES_LANGUAGE_INDEX_CODE_BYTES);
    guest->language[ORACLES_LANGUAGE_INDEX_CODE_BYTES - 1u] = 0;
    return strcmp(guest->language, "ja") ? 0 : arm_names(guest, rom, size);
}

/* The game went to the other half of its image: the switch disarmed the text's transactions, each armed again on the
 * half's tables and its index, its font, its points, as the game's host armed them in the half it left. */
static void text_half_switched(void *opaque)
{
    OraclesGuest *guest = opaque;
    const unsigned wanted = guest->text_wanted;
    unsigned lost = 0;
    if (wanted & ORACLES_GUEST_TEXT_LANGUAGE) lost += oracles_guest_arm_language(guest) != 0 || !guest->language_index;
    if (wanted & ORACLES_GUEST_TEXT_BOX) lost += oracles_guest_arm_text_box(guest, guest->text_box_pixels) != 0;
    if (wanted & ORACLES_GUEST_TEXT_COMPACT) {
        const int fixed = guest->compact_fixed;
        lost += oracles_guest_arm_compact_text(guest) != 0;
        if (fixed) oracles_text_font_fixed(&guest->compact_font, &guest->compact_font);
    }
    guest->text_lost += lost;
    if (!lost) guest->text_armed++;
}

int oracles_guest_text_follow_half(OraclesGuest *guest, unsigned armed)
{
    guest->text_wanted |= armed;
    return guest->half_watched ? 0 : oracles_guest_listen_half(guest, text_half_switched);
}

const char *oracles_guest_language(const OraclesGuest *guest) { return guest && guest->language_index ? guest->language : ""; }

void oracles_guest_language_counts(const OraclesGuest *guest, unsigned *served, unsigned *english, unsigned *missing)
{
    if (served) *served = guest ? guest->language_served : 0;
    if (english) *english = guest ? guest->language_english : 0;
    if (missing) *missing = guest ? guest->language_missing : 0;
}

unsigned oracles_guest_language_laid_out(const OraclesGuest *guest) { return guest ? guest->language_laid_out : 0; }

void oracles_guest_serve_text(OraclesGuest *guest)
{
    const uint8_t *index = guest->language_index;
    if (!index) return;
    const OraclesGuestTables *t = guest->tables;
    const unsigned group = oracles_guest_read8(guest, t->text_index_h), text = oracles_guest_read8(guest, t->text_index_l);
    if (group < ORACLES_TEXT_DICTIONARY_GROUPS) return;   /* a fragment of the dictionary: the US text's own */
    if (group >= index[17]) { guest->language_missing++; return; }
    const uint8_t *row = index + ORACLES_LANGUAGE_INDEX_GROUPS + 4u * group;
    const unsigned entries = row[0] | row[1] << 8, count = row[2] | row[3] << 8;
    if (text >= count || entries + 3u * count > 0x4000u) { guest->language_missing++; return; }
    const uint8_t *entry = index + entries + 3u * text;
    const unsigned bank = entry[0], addr = (entry[1] | entry[2] << 8) & ~ORACLES_LANGUAGE_LAID_OUT;
    if (!bank) {
        if (addr == ORACLES_LANGUAGE_KEPT) guest->language_english++;
        else guest->language_missing++;
        return;
    }
    uint8_t *wram = oracles_guest_wram_writable(guest, 7);
    if (!wram || addr < 0x4000u || addr >= 0x8000u) return;
    wram[t->active_text_bank.addr - 0xd000u] = (uint8_t)bank;
    OraclesCoreRegisters r = oracles_core_registers(guest->core);
    r.hl = (uint16_t)addr;
    r.af = (uint16_t)(bank << 8 | (r.af & 0xffu));
    oracles_core_set_registers(guest->core, &r);
    guest->language_served++;
    if (entry[2] << 8 & ORACLES_LANGUAGE_LAID_OUT) guest->language_laid_out++;
}
