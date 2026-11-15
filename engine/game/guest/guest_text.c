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
    return 0;
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
