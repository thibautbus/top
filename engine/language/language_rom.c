/* The languages and the European ROMs they come from: where their texts and their font lie. */
#include "language_internal.h"

#include "guest_tables.h"
#include "profile.h"
#include "sha1.h"

#include <stdio.h>
#include <string.h>

static const char *const codes[ORACLES_LANGUAGE_COUNT] = { "en", "en-eu", "fr", "de", "it", "es" };

/* The European ROMs, recognised by their SHA-1 (rom.c): where getTextAddress finds a language's table (a 3-byte
 * pointer per language: address, bank) and its three bases (4 bytes per language: bank, address in the bank, 0), the
 * groups from which the second and the third base count, and the font, character c at font + 16 c.  The European
 * code has no symbol file: these are the disassembly's dumper's (tools/dump/dumpText.py), and the font's place where
 * the two fonts' glyphs line up; and the graphics headers' table, which its loadGfxHeader (bank 0, $0626 in Ages,
 * $0602 in Seasons, as in the US ROMs) maps and reads (language_gfx.c). */
static const struct european {
    OraclesRomRevision revision;
    OraclesGame game;
    OraclesSym languages;
    OraclesSym bases[3];
    unsigned splits[2];
    OraclesSym font;
    OraclesSym gfx_headers;
} europeans[] = {
    { ORACLES_ROM_REVISION_AGES_EU, ORACLES_GAME_AGES, { 0x3f, 0x5015 }, { { 0x3f, 0x4fd9 }, { 0x3f, 0x4fed }, { 0x3f, 0x5001 } }, { 0x1a, 0x34 }, { 0x1b, 0x4720 }, { 0x17, 0x451e } },
    { ORACLES_ROM_REVISION_SEASONS_EU, ORACLES_GAME_SEASONS, { 0x3f, 0x5044 }, { { 0x3f, 0x5008 }, { 0x3f, 0x501c }, { 0x3f, 0x5030 } }, { 0x1a, 0x34 }, { 0x1b, 0x4720 }, { 0x16, 0x4000 } },
};

static const struct european *european_of(OraclesGame game)
{
    for (size_t i = 0; i < sizeof europeans / sizeof europeans[0]; i++) if (europeans[i].game == game) return &europeans[i];
    return NULL;
}

static const OraclesGuestTables *tables_of(OraclesGame game)
{
    return game == ORACLES_GAME_AGES ? &oracles_guest_tables_ages : game == ORACLES_GAME_SEASONS ? &oracles_guest_tables_seasons : NULL;
}

const char *oracles_language_code(OraclesLanguage language) { return language < ORACLES_LANGUAGE_COUNT ? codes[language] : "?"; }

int oracles_language_parse(const char *code, OraclesLanguage *language)
{
    for (unsigned i = 0; code && i < ORACLES_LANGUAGE_COUNT; i++)
        if (!strcmp(code, codes[i])) { *language = (OraclesLanguage)i; return 0; }
    return -1;
}

OraclesGame oracles_language_source_game(const OraclesRomInfo *info)
{
    for (size_t i = 0; info && info->known && i < sizeof europeans / sizeof europeans[0]; i++)
        if (europeans[i].revision == info->revision) return europeans[i].game;
    return ORACLES_GAME_UNKNOWN;
}

/* A 3-byte pointer as `bank, address in the bank` (the bases) or `address, bank` (Pointer3Byte), physical. */
static int pointer(const OraclesRom *rom, size_t at, int bank_first, size_t *out)
{
    uint8_t b[3];
    for (unsigned i = 0; i < 3u; i++) if (oracles_rom_read8(rom, at + i, &b[i]) != ORACLES_OK) return -1;
    const unsigned bank = bank_first ? b[0] : b[2], addr = bank_first ? (unsigned)(b[1] | b[2] << 8) : (unsigned)(b[0] | b[1] << 8);
    *out = (size_t)bank * 0x4000u + (addr & 0x3fffu);
    return *out < rom->size ? 0 : -1;
}

int language_source_tables(const OraclesRom *rom, const OraclesGuestTables *t, OraclesTextSource *source)
{
    if (!t || t->text_table_table.bank == ORACLES_GUEST_ABSENT || t->text_offset1_table.bank == ORACLES_GUEST_ABSENT
        || t->text_offset2_table.bank == ORACLES_GUEST_ABSENT) return -1;
    memset(source, 0, sizeof *source);
    if (pointer(rom, oracles_rom_offset(t->text_table_table.bank, t->text_table_table.addr), 0, &source->table) != 0
        || pointer(rom, oracles_rom_offset(t->text_offset1_table.bank, t->text_offset1_table.addr), 1, &source->bases[0]) != 0
        || pointer(rom, oracles_rom_offset(t->text_offset2_table.bank, t->text_offset2_table.addr), 1, &source->bases[1]) != 0) return -1;
    source->bases[2] = source->bases[1];
    source->splits[0] = t->text_offset_split_index;
    source->splits[1] = 0x100u;
    return 0;
}

int language_source_profile(const OraclesRom *rom, const OraclesCompatProfile *profile, OraclesTextSource *source)
{
    if (language_source_tables(rom, oracles_compat_guest_tables(profile), source) != 0) return -1;
    unsigned split = 0;
    OraclesGuestSym third;
    if (!oracles_compat_text_third_base(profile, &split, &third)) return 0;
    source->bases[2] = oracles_rom_offset(third.bank, third.addr);
    source->splits[1] = split;
    return source->bases[2] < rom->size ? 0 : -1;
}

int language_source_us(const OraclesRom *rom, OraclesGame game, OraclesTextSource *source)
{
    return language_source_tables(rom, tables_of(game), source);
}

int language_target_of(const char *sha1, language_target *target)
{
    memset(target, 0, sizeof *target);
    OraclesRomInfo info;
    if (oracles_rom_known(sha1, &info) != 0) return -1;
    const OraclesCompatProfile *profile = oracles_compat_find(&info);
    const OraclesGuestTables *t = oracles_compat_guest_tables(profile);
    const unsigned bank = oracles_compat_language_bank(profile);
    if (!t || !bank || bank > LANGUAGE_LAST_BANK || t->get_text_address.bank == ORACLES_GUEST_ABSENT || t->font_start.bank == ORACLES_GUEST_ABSENT)
        return -1;
    target->game = info.game;
    target->revision = info.revision;
    target->tables = t;
    target->first_bank = bank;
    target->original = oracles_rom_is_original(&info);
    snprintf(target->sha1, sizeof target->sha1, "%s", sha1);
    target->name = oracles_compat_name(profile);
    target->profile = profile;
    target->halves = oracles_compat_half_switch(profile) ? 2u : 1u;
    target->own = ORACLES_LANGUAGE_ENGLISH;
    oracles_language_image_own(&info, &target->own);
    for (unsigned h = 1; h < target->halves; h++) {
        const OraclesCompatProfile *half = oracles_compat_half(profile, h);
        const OraclesGuestTables *ht = oracles_compat_guest_tables(half);
        if (!ht || oracles_compat_language_bank(half) != bank || ht->get_text_address.bank == ORACLES_GUEST_ABSENT || ht->font_start.bank == ORACLES_GUEST_ABSENT)
            return -1;
    }
    return 0;
}

int oracles_language_image_own(const OraclesRomInfo *info, OraclesLanguage *language)
{
    static const OraclesLanguage combined[] = { ORACLES_LANGUAGE_ENGLISH, ORACLES_LANGUAGE_FRENCH, ORACLES_LANGUAGE_GERMAN, ORACLES_LANGUAGE_SPANISH,
                                                ORACLES_LANGUAGE_ITALIAN };
    if (!info || !info->known || info->revision < ORACLES_ROM_REVISION_COMBINED_1_5_EN || info->revision > ORACLES_ROM_REVISION_COMBINED_1_5_IT) return 0;
    *language = combined[info->revision - ORACLES_ROM_REVISION_COMBINED_1_5_EN];
    return 1;
}

int language_text_of(const OraclesRom *rom, const OraclesTextSource *source, unsigned groups, unsigned id, uint8_t *out, size_t *length)
{
    const unsigned group = (id >> 8) + ORACLES_TEXT_DICTIONARY_GROUPS;
    if (group >= groups || (int)(id & 0xffu) >= oracles_text_count(rom, source, group)) return -1;
    return oracles_text_decode(rom, source, group, id & 0xffu, out, ORACLES_TEXT_MAX, length) == ORACLES_OK ? 0 : -1;
}

language_slot *language_slot_of(language_slot *slots, unsigned count, const uint8_t *bytes, const uint8_t *text, size_t length)
{
    uint32_t h = 2166136261u;   /* FNV-1a */
    for (size_t i = 0; i < length; i++) h = (h ^ text[i]) * 16777619u;
    uint32_t slot = h & (count - 1u);
    for (; slots[slot].length; slot = (slot + 1u) & (count - 1u))
        if (slots[slot].length == length && !memcmp(bytes + slots[slot].at, text, length)) break;
    return &slots[slot];
}

unsigned language_european_index(OraclesLanguage language)
{
    return language == ORACLES_LANGUAGE_ENGLISH || language == ORACLES_LANGUAGE_ENGLISH_EU ? 0u : (unsigned)language - ORACLES_LANGUAGE_ENGLISH_EU;
}

int language_source_european(const OraclesRom *rom, OraclesGame game, OraclesLanguage language, OraclesTextSource *source)
{
    const struct european *e = european_of(game);
    if (!e || language >= ORACLES_LANGUAGE_COUNT) return -1;
    const unsigned index = language_european_index(language);
    memset(source, 0, sizeof *source);
    if (pointer(rom, oracles_rom_offset(e->languages.bank, e->languages.addr) + 3u * index, 0, &source->table) != 0) return -1;
    for (unsigned k = 0; k < 3u; k++)
        if (pointer(rom, oracles_rom_offset(e->bases[k].bank, e->bases[k].addr) + 4u * index, 1, &source->bases[k]) != 0) return -1;
    source->splits[0] = e->splits[0];
    source->splits[1] = e->splits[1];
    return 0;
}

size_t language_font_us(OraclesGame game)
{
    const OraclesGuestTables *t = tables_of(game);
    return t ? oracles_rom_offset(t->font_start.bank, t->font_start.addr) : 0;
}

size_t language_font_european(OraclesGame game)
{
    const struct european *e = european_of(game);
    return e ? oracles_rom_offset(e->font.bank, e->font.addr) : 0;
}

size_t language_gfx_headers_us(OraclesGame game)
{
    const OraclesGuestTables *t = tables_of(game);
    return t ? oracles_rom_offset(t->gfx_header_table.bank, t->gfx_header_table.addr) : 0;
}

size_t language_gfx_headers_european(OraclesGame game)
{
    const struct european *e = european_of(game);
    return e ? oracles_rom_offset(e->gfx_headers.bank, e->gfx_headers.addr) : 0;
}
