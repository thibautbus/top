/* The languages and the European, Japanese and Polish ROMs they come from: where their texts and their font lie. */
#include "language_internal.h"

#include "guest_tables.h"
#include "profile.h"
#include "sha1.h"

#include <stdio.h>
#include <string.h>

static const char *const codes[ORACLES_LANGUAGE_COUNT] = { "en", "en-eu", "fr", "de", "it", "es", "ja", "pl" };

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

/* The Japanese ROMs: getTextAddress reads its table of groups in a fixed bank (readByteFromW7TextTableBank), its first
 * base from a table and its second from its own operands (`ld hl,$09f5 ; ld e,$22` in Ages), the second counting from
 * a later group than the US one's; the font is gfx_font, from character $0e, a bank before the US one's place.  The
 * disassembly's dumper's constants (tools/dump/dumpText.py), the font's where its digits and kana are.  The graphics
 * headers' table and the palette headers' table, which its loadGfxHeader and loadPaletteHeader (bank 0, at the US
 * routines' places) map and read, and the title's sprites (titlescreenMakuSeedSprite), which its title screen's code
 * names (bank 3, $4e03); the secrets' cipher (secretXorCipher, where the US one is) and their symbols, which follow it in
 * bank 3 where the US ROM has them in bank 0, and the names of the secrets' list (secretTextTable, bank 2); the object
 * graphics headers' table (objectGfxHeaderTable), which its loadObjectGfx's callers (bank $3f, at the US code's places)
 * name (language_gfx.c). */
static const struct japanese {
    OraclesRomRevision revision;
    OraclesGame game;
    OraclesSym table;
    OraclesSym base1;
    OraclesSym base2;   /* the base itself, as a CPU address in its bank */
    unsigned split;
    OraclesSym font;
    OraclesSym gfx_headers;
    OraclesSym palette_headers;
    OraclesSym title_sprites;
    OraclesSym secret_cipher, secret_symbols, secret_names;
    OraclesSym object_headers;
} japaneses[] = {
    { ORACLES_ROM_REVISION_AGES_JP, ORACLES_GAME_AGES, { 0x1e, 0x4000 }, { 0x3f, 0x4f63 }, { 0x22, 0x49f5 }, 0x34, { 0x1b, 0x7f20 },
      { 0x01, 0x69dc }, { 0x01, 0x6334 }, { 0x3f, 0x58cf }, { 0x03, 0x4ada }, { 0x03, 0x4b0a }, { 0x02, 0x4e7f }, { 0x3f, 0x5a14 } },
    { ORACLES_ROM_REVISION_SEASONS_JP, ORACLES_GAME_SEASONS, { 0x1d, 0x4000 }, { 0x3f, 0x4f92 }, { 0x1f, 0x61df }, 0x34, { 0x1b, 0x7f20 },
      { 0x01, 0x6920 }, { 0x01, 0x6290 }, { 0x03, 0x4e8e }, { 0x03, 0x4ada }, { 0x03, 0x4b0a }, { 0x02, 0x4e3f }, { 0x3f, 0x5a79 } },
};

/* The US ROMs' palette headers' table (paletteHeaderTable, as loadPaletteHeader maps it at $0520 in Ages, $04fc in
 * Seasons), and the bank of the colours its entries point to (paletteDataStart). */
static const struct palettes { OraclesGame game; OraclesSym headers; unsigned colours; } us_palettes[] = {
    { ORACLES_GAME_AGES, { 0x01, 0x632c }, 0x17 },
    { ORACLES_GAME_SEASONS, { 0x01, 0x6290 }, 0x16 },
};

/* The Polish ROMs, as Kaxus's patch (its SHA-1) makes them of the US ROMs: images rebuilt from the disassembly, whose
 * getTextAddress is the US one at another place, reading its table of groups by a pointer (address, bank) and its two
 * bases by their tables (bank, address), the second counting from group `split`, as the US code does; the font is
 * gfx_font where the US one is, character c at font + 16 c; the graphics headers' table, the US one at another
 * place, which its loadGfxHeader (bank 0, $066e in Ages, $064a in Seasons) maps and reads, and the title's sprites of
 * PRESS START (titlescreenPressStartSprites), which its title screen's code names (bank 3, $4295 in Ages, $4293 in
 * Seasons) (language_gfx.c). */
static const struct polish {
    OraclesRomRevision revision;
    OraclesGame game;
    const char *patch;
    OraclesSym table;
    OraclesSym bases[2];
    unsigned split;
    OraclesSym font;
    OraclesSym gfx_headers;
    OraclesSym press_start;
} polishes[] = {
    { ORACLES_ROM_REVISION_AGES_PL, ORACLES_GAME_AGES, "5173a7aa608597ae7d5ac054d610914d0a292c0d", { 0x3f, 0x4ff0 }, { { 0x3f, 0x4fc0 }, { 0x3f, 0x4fd8 } }, 0x28,
      { 0x1c, 0x4720 }, { 0x01, 0x6dde }, { 0x3f, 0x59ce } },
    { ORACLES_ROM_REVISION_SEASONS_PL, ORACLES_GAME_SEASONS, "b8cac23dd9e7129eafb66cffebd06e718f81213d", { 0x3f, 0x5a52 }, { { 0x3f, 0x5a22 }, { 0x3f, 0x5a3a } },
      0x34, { 0x1c, 0x4720 }, { 0x16, 0x6f15 }, { 0x03, 0x435b } },
};

static const struct polish *polish_of(OraclesGame game)
{
    for (size_t i = 0; i < sizeof polishes / sizeof polishes[0]; i++) if (polishes[i].game == game) return &polishes[i];
    return NULL;
}

static const struct japanese *japanese_of(OraclesGame game)
{
    for (size_t i = 0; i < sizeof japaneses / sizeof japaneses[0]; i++) if (japaneses[i].game == game) return &japaneses[i];
    return NULL;
}

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

int language_japanese(const OraclesRomInfo *info)
{
    for (size_t i = 0; info && info->known && i < sizeof japaneses / sizeof japaneses[0]; i++) if (japaneses[i].revision == info->revision) return 1;
    return 0;
}

int language_polish(const OraclesRomInfo *info)
{
    for (size_t i = 0; info && info->known && i < sizeof polishes / sizeof polishes[0]; i++) if (polishes[i].revision == info->revision) return 1;
    return 0;
}

OraclesGame oracles_language_patch_game(const char *sha1, OraclesLanguage language)
{
    for (size_t i = 0; sha1 && language == ORACLES_LANGUAGE_POLISH && i < sizeof polishes / sizeof polishes[0]; i++)
        if (!strcmp(polishes[i].patch, sha1)) return polishes[i].game;
    return ORACLES_GAME_UNKNOWN;
}

OraclesGame oracles_language_source_game_of(const OraclesRomInfo *info, OraclesLanguage language)
{
    if (language == ORACLES_LANGUAGE_JAPANESE) return language_japanese(info) ? info->game : ORACLES_GAME_UNKNOWN;
    if (language == ORACLES_LANGUAGE_POLISH) return language_polish(info) ? info->game : ORACLES_GAME_UNKNOWN;
    return language > ORACLES_LANGUAGE_ENGLISH && language < ORACLES_LANGUAGE_EUROPEAN_END ? oracles_language_source_game(info) : ORACLES_GAME_UNKNOWN;
}

int oracles_language_compact_text(OraclesLanguage language) { return language != ORACLES_LANGUAGE_JAPANESE; }

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
    if (!e || language >= ORACLES_LANGUAGE_EUROPEAN_END) return -1;
    const unsigned index = language_european_index(language);
    memset(source, 0, sizeof *source);
    if (pointer(rom, oracles_rom_offset(e->languages.bank, e->languages.addr) + 3u * index, 0, &source->table) != 0) return -1;
    for (unsigned k = 0; k < 3u; k++)
        if (pointer(rom, oracles_rom_offset(e->bases[k].bank, e->bases[k].addr) + 4u * index, 1, &source->bases[k]) != 0) return -1;
    source->splits[0] = e->splits[0];
    source->splits[1] = e->splits[1];
    return 0;
}

int language_source_japanese(const OraclesRom *rom, OraclesGame game, OraclesTextSource *source)
{
    const struct japanese *j = japanese_of(game);
    if (!j) return -1;
    memset(source, 0, sizeof *source);
    if (pointer(rom, oracles_rom_offset(j->base1.bank, j->base1.addr), 1, &source->bases[0]) != 0) return -1;
    source->table = oracles_rom_offset(j->table.bank, j->table.addr);
    source->bases[1] = source->bases[2] = oracles_rom_offset(j->base2.bank, j->base2.addr);
    source->splits[0] = j->split;
    source->splits[1] = 0x100u;
    return source->table < rom->size && source->bases[1] < rom->size ? 0 : -1;
}

int language_source_polish(const OraclesRom *rom, OraclesGame game, OraclesTextSource *source)
{
    const struct polish *p = polish_of(game);
    if (!p) return -1;
    memset(source, 0, sizeof *source);
    if (pointer(rom, oracles_rom_offset(p->table.bank, p->table.addr), 0, &source->table) != 0) return -1;
    for (unsigned k = 0; k < 2u; k++)
        if (pointer(rom, oracles_rom_offset(p->bases[k].bank, p->bases[k].addr), 1, &source->bases[k]) != 0) return -1;
    source->bases[2] = source->bases[1];
    source->splits[0] = p->split;
    source->splits[1] = 0x100u;
    return 0;
}

size_t language_font_polish(OraclesGame game)
{
    const struct polish *p = polish_of(game);
    return p ? oracles_rom_offset(p->font.bank, p->font.addr) : 0;
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

size_t language_font_japanese(OraclesGame game)
{
    const struct japanese *j = japanese_of(game);
    return j ? oracles_rom_offset(j->font.bank, j->font.addr) : 0;
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

size_t language_gfx_headers_polish(OraclesGame game)
{
    const struct polish *p = polish_of(game);
    return p ? oracles_rom_offset(p->gfx_headers.bank, p->gfx_headers.addr) : 0;
}

size_t language_press_start_polish(OraclesGame game)
{
    const struct polish *p = polish_of(game);
    return p ? oracles_rom_offset(p->press_start.bank, p->press_start.addr) : 0;
}

size_t language_gfx_headers_japanese(OraclesGame game)
{
    const struct japanese *j = japanese_of(game);
    return j ? oracles_rom_offset(j->gfx_headers.bank, j->gfx_headers.addr) : 0;
}

size_t language_palette_headers_japanese(OraclesGame game)
{
    const struct japanese *j = japanese_of(game);
    return j ? oracles_rom_offset(j->palette_headers.bank, j->palette_headers.addr) : 0;
}

size_t language_title_sprites_japanese(OraclesGame game)
{
    const struct japanese *j = japanese_of(game);
    return j ? oracles_rom_offset(j->title_sprites.bank, j->title_sprites.addr) : 0;
}

size_t language_secret_cipher_japanese(OraclesGame game)
{
    const struct japanese *j = japanese_of(game);
    return j ? oracles_rom_offset(j->secret_cipher.bank, j->secret_cipher.addr) : 0;
}

size_t language_secret_symbols_japanese(OraclesGame game)
{
    const struct japanese *j = japanese_of(game);
    return j ? oracles_rom_offset(j->secret_symbols.bank, j->secret_symbols.addr) : 0;
}

size_t language_secret_names_japanese(OraclesGame game)
{
    const struct japanese *j = japanese_of(game);
    return j ? oracles_rom_offset(j->secret_names.bank, j->secret_names.addr) : 0;
}

size_t language_object_headers_japanese(OraclesGame game)
{
    const struct japanese *j = japanese_of(game);
    return j ? oracles_rom_offset(j->object_headers.bank, j->object_headers.addr) : 0;
}

size_t language_palette_headers_us(OraclesGame game, unsigned *colours)
{
    for (size_t i = 0; i < sizeof us_palettes / sizeof us_palettes[0]; i++)
        if (us_palettes[i].game == game) {
            *colours = us_palettes[i].colours;
            return oracles_rom_offset(us_palettes[i].headers.bank, us_palettes[i].headers.addr);
        }
    return 0;
}
