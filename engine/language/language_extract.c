/* The extraction of a pack: for each language, a text for each US text, and the European or Polish glyphs or the
 * Japanese font; and the graphics that carry text of the European, Polish or Japanese ROM (language_gfx.c).
 *
 * A US text TX_GGII takes, in order: what the game's table of exceptions names for it in that language (the European
 * or Japanese text of another identifier, that text adapted by a rule, the port's own text, or the US English); else
 * the European text of its identifier, or the Japanese text of its representative.  The text found is checked against
 * the US text: the same commands (for the texts found and the port's own, language_same_commands; for an adapted text,
 * as the US engine acts on them, language_rule_holds) and no line over sixteen characters.  One that fails, or that the
 * ROM lacks, stays in English, and the pack says why (OraclesLanguageSource).
 *
 * The representative of a US text is the first identifier of its group whose text lies where its own does.  Both
 * editions pad a group's unused identifiers with a copy of a text, but the US one with the text before and the
 * Japanese one with the text after: TX_0066 of Ages is the US text of TX_0065 again, the Japanese of TX_0067.  By its
 * representative, a padding identifier takes the text the US game shows there.
 *
 * A Polish text, the Polish ROM's text of the same identifier, is corrected before it is checked
 * (config/language_corrections_pl.tsv): the Polish ROM writes a few lines past the box, which the correction breaks
 * again.  The identifiers the Polish ROM adds past the US ones (its own items' texts) stand for no US text: none is
 * read. */
#include "language_internal.h"

#include <stdio.h>
#include <string.h>

/* Whether a text found or the port's own may stand for the US text as it is. */
static int holds(OraclesLanguage language, const uint8_t *us, size_t us_length, const uint8_t *text, size_t length)
{
    return language_same_commands(language, us, us_length, text, length) && oracles_text_longest_line(text, length) <= LINE_CHARACTERS;
}

/* The index of the representative of text `index` of internal group `group`. */
static unsigned representative(const OraclesRom *us, const OraclesTextSource *source, unsigned group, unsigned index)
{
    const long at = oracles_text_address(us, source, group, index);
    for (unsigned i = 0; i < index; i++) if (oracles_text_address(us, source, group, i) == at) return i;
    return index;
}

/* Every US text's entry in a language, by increasing identifier. */
static int extract_language(OraclesLanguagePack *pack, const language_table *table, language_corrections *corrections, const OraclesRom *eu,
                            const OraclesRom *us, OraclesLanguage language, char *error, size_t capacity)
{
    const char *code = oracles_language_code(language);
    const int japanese = language == ORACLES_LANGUAGE_JAPANESE, polish = language == ORACLES_LANGUAGE_POLISH;
    OraclesTextSource eu_source, us_source;
    if ((japanese ? language_source_japanese(eu, pack->game, &eu_source)
         : polish ? language_source_polish(eu, pack->game, &eu_source) : language_source_european(eu, pack->game, language, &eu_source)) != 0) {
        snprintf(error, capacity, "the %s texts' tables cannot be read", code);
        return -1;
    }
    if (language_source_us(us, pack->game, &us_source) != 0) { snprintf(error, capacity, "the US texts' tables cannot be read"); return -1; }
    const unsigned eu_groups = oracles_text_groups(eu, &eu_source), us_groups = oracles_text_groups(us, &us_source);
    uint8_t original[ORACLES_TEXT_MAX], found[ORACLES_TEXT_MAX], next[ORACLES_TEXT_MAX], made[ORACLES_TEXT_MAX];
    for (unsigned group = ORACLES_TEXT_DICTIONARY_GROUPS; group < us_groups; group++) {
        const int count = oracles_text_count(us, &us_source, group);
        if (count < 0) { snprintf(error, capacity, "the US texts' group %02x cannot be read", group); return -1; }
        for (int index = 0; index < count; index++) {
            const unsigned id = (group - ORACLES_TEXT_DICTIONARY_GROUPS) << 8 | (unsigned)index;
            const unsigned from = japanese ? (id & ~0xffu) | representative(us, &us_source, group, (unsigned)index) : id;
            size_t us_length = 0, length = 0, next_length = 0;
            if (oracles_text_decode(us, &us_source, group, (unsigned)index, original, sizeof original, &us_length) != ORACLES_OK) {
                snprintf(error, capacity, "the US text TX_%04x cannot be decoded", id);
                return -1;
            }
            language_exception e = { ORACLES_LANGUAGE_SOURCE_SAME, id, RULE_COUNT, "" };
            const int listed = language_table_find(table, language, id, &e);
            OraclesLanguageSource source = e.source;
            const uint8_t *text = made;
            switch (source) {
            case ORACLES_LANGUAGE_SOURCE_SAME:
            case ORACLES_LANGUAGE_SOURCE_MOVED:
                text = found;
                if (language_text_of(eu, &eu_source, eu_groups, listed ? e.id : from, found, &length) != 0) source = listed ? ORACLES_LANGUAGE_SOURCE_REFUSED : ORACLES_LANGUAGE_SOURCE_MISSING;
                else if (polish && language_correct(corrections, language, id, found, &length, sizeof found) < 0) source = ORACLES_LANGUAGE_SOURCE_REFUSED;
                else if (!holds(language, original, us_length, found, length)) source = ORACLES_LANGUAGE_SOURCE_REFUSED;
                break;
            case ORACLES_LANGUAGE_SOURCE_ADAPTED: {
                const int has_next = language_text_of(eu, &eu_source, eu_groups, from + 1u, next, &next_length) == 0;
                if (language_text_of(eu, &eu_source, eu_groups, from, found, &length) != 0
                    || language_rule_apply(e.rule, language, original, us_length, found, length, has_next ? next : NULL, next_length, made, sizeof made, &length) != 0
                    || !language_rule_holds(e.rule, language, original, us_length, made, length)) source = ORACLES_LANGUAGE_SOURCE_REFUSED;
                break;
            }
            case ORACLES_LANGUAGE_SOURCE_OWN:
                if (language_own_text(e.key, language, made, sizeof made, &length) != 0 || !holds(language, original, us_length, made, length) || !language_text_fits(made, length))
                    source = ORACLES_LANGUAGE_SOURCE_REFUSED;
                break;
            default:
                source = ORACLES_LANGUAGE_SOURCE_KEPT;
                break;
            }
            /* A mistake of the official translation, corrected when the corrected text still stands for the US one. */
            if (!polish && source <= ORACLES_LANGUAGE_SOURCE_ADAPTED) {
                uint8_t corrected[ORACLES_TEXT_MAX];
                size_t corrected_length = length;
                memcpy(corrected, text, length);
                const int corrected_now = language_correct(corrections, language, id, corrected, &corrected_length, sizeof corrected);
                if (corrected_now == 1 && language_same_commands(language, original, us_length, corrected, corrected_length) && language_text_fits(corrected, corrected_length)) {
                    memcpy(made, corrected, corrected_length);
                    text = made;
                    length = corrected_length;
                } else if (corrected_now != 0) language_corrections_refuse(corrections);
            }
            const int served = source <= ORACLES_LANGUAGE_SOURCE_OWN;
            if (language_pack_add(pack, language, id, source, served ? text : NULL, served ? length : 0) != 0) { snprintf(error, capacity, "out of memory"); return -1; }
        }
    }
    if (!pack->count[language]) { snprintf(error, capacity, "no US text to translate in %s", code); return -1; }
    return 0;
}

OraclesLanguagePack *oracles_language_pack_extract(const uint8_t *european, size_t european_size, const uint8_t *us, size_t us_size,
                                                   char *error, size_t capacity)
{
    OraclesRomInfo eu_info;
    if (oracles_rom_identify(european, european_size, &eu_info, error, capacity) != 0) return NULL;
    language_corrections *corrections = language_polish(&eu_info) ? language_corrections_load_polish(eu_info.game, error, capacity)
                                                                   : language_corrections_load(oracles_language_source_game(&eu_info), error, capacity);
    if (!corrections) return NULL;
    OraclesLanguagePack *pack = language_pack_extract_with(european, european_size, us, us_size, corrections, error, capacity);
    language_corrections_free(corrections);
    return pack;
}

OraclesLanguagePack *language_pack_extract_with(const uint8_t *european, size_t european_size, const uint8_t *us, size_t us_size,
                                                language_corrections *corrections, char *error, size_t capacity)
{
    OraclesRomInfo eu_info, us_info;
    if (oracles_rom_identify(european, european_size, &eu_info, error, capacity) != 0) return NULL;
    if (language_own_check(error, capacity) != 0) return NULL;
    const language_edition kind = language_edition_of(&eu_info);
    const int japanese = kind == EDITION_JAPANESE, polish = kind == EDITION_POLISH;
    const OraclesGame game = japanese || polish ? eu_info.game : oracles_language_source_game(&eu_info);
    const char *edition = japanese ? "Japanese" : polish ? "Polish" : "European";
    if (game == ORACLES_GAME_UNKNOWN) { snprintf(error, capacity, "not a European, Japanese or Polish ROM the port knows (SHA-1 %s)", eu_info.sha1); return NULL; }
    if (oracles_rom_identify(us, us_size, &us_info, error, capacity) != 0) return NULL;
    if (!oracles_rom_is_original(&us_info) || us_info.game != game) {
        snprintf(error, capacity, "the %s ROM of %s needs the US ROM of the same game beside it", edition, oracles_game_name(game));
        return NULL;
    }
    language_table *table = japanese ? language_table_load_japanese(game, error, capacity)
                          : polish ? language_table_load_polish(game, error, capacity) : language_table_load(game, error, capacity);
    if (!table) return NULL;
    if (strcmp(language_table_rom(table), eu_info.sha1) != 0) {
        snprintf(error, capacity, "the table of exceptions of %s was measured on the %s ROM %s, not %s", oracles_game_name(game), edition, language_table_rom(table), eu_info.sha1);
        language_table_free(table);
        return NULL;
    }
    OraclesLanguagePack *pack = language_pack_new(game, eu_info.sha1, us_info.sha1);
    if (!pack) snprintf(error, capacity, "out of memory");
    const OraclesRom eu_rom = { european, european_size }, us_rom = { us, us_size };
    const OraclesLanguage first = japanese ? ORACLES_LANGUAGE_JAPANESE : polish ? ORACLES_LANGUAGE_POLISH : ORACLES_LANGUAGE_ENGLISH_EU;
    const OraclesLanguage end = japanese ? ORACLES_LANGUAGE_JAPANESE + 1 : polish ? ORACLES_LANGUAGE_POLISH + 1 : ORACLES_LANGUAGE_EUROPEAN_END;
    for (OraclesLanguage language = first; pack && language < end; language++)
        if (extract_language(pack, table, corrections, &eu_rom, &us_rom, language, error, capacity) != 0) { oracles_language_pack_free(pack); pack = NULL; }
    language_table_free(table);
    if (!pack) return NULL;
    if (kind == EDITION_EUROPEAN && language_gfx_extract(pack, &eu_rom, &us_rom, error, capacity) != 0) { oracles_language_pack_free(pack); return NULL; }
    if (polish && language_gfx_extract_polish(pack, &eu_rom, &us_rom, error, capacity) != 0) { oracles_language_pack_free(pack); return NULL; }
    if (japanese && language_gfx_extract_japanese(pack, &eu_rom, &us_rom, error, capacity) != 0) { oracles_language_pack_free(pack); return NULL; }
    /* The glyphs that differ: those the US font lacks (its slots past the accents hold one filler tile) and those the
     * European or Polish font redraws (the Polish letters where the US font has its accented capitals); or the whole
     * Japanese font, its kana where the US font has its lower-case and accented letters. */
    const size_t eu_font = japanese ? language_font_japanese(game) : polish ? language_font_polish(game) : language_font_european(game), us_font = language_font_us(game);
    const int fonts = eu_font + 0x100u * GLYPH_BYTES <= european_size && us_font + 0x100u * GLYPH_BYTES <= us_size;
    for (unsigned c = japanese ? GLYPH_FIRST : FONT_FIRST; fonts && c < 0x100u; c++) {
        const uint8_t *mine = european + eu_font + c * GLYPH_BYTES, *theirs = us + us_font + c * GLYPH_BYTES;
        if (!japanese && !memcmp(mine, theirs, GLYPH_BYTES)) continue;
        pack->characters[pack->glyph_count] = (uint8_t)c;
        memcpy(pack->bitmaps + pack->glyph_count * GLYPH_BYTES, mine, GLYPH_BYTES);
        pack->glyph_count++;
    }
    if (japanese && pack->glyph_count != GLYPHS_MAX) {
        snprintf(error, capacity, "the Japanese font of %s cannot be read", oracles_game_name(game));
        oracles_language_pack_free(pack);
        return NULL;
    }
    return pack;
}
