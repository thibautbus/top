/* The pack of a fan game, made from the pack of the game it is built on.
 *
 * A fan game made on the US ROM of Ages or Seasons keeps most of its texts: those whose bytes, decoded, are the US
 * text's of the same identifier take that identifier's translation in the base game's pack (the European text, moved,
 * adapted, corrected or the port's own, as language_extract.c chose it); those that are the US text of another
 * identifier take that one's (the first, by identifier); the fan game's own texts stay in its English
 * (ORACLES_LANGUAGE_SOURCE_FAN).  The glyphs of the European font are kept where the fan game left the US font's; its
 * graphics are its own and stay as they are.  Nothing of the fan game is stored: its texts are read in its image, made
 * on the user's machine from his ROM and the fan game's patch, and the pack is pinned to that image's SHA-1. */
#include "language_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SLOTS 8192u   /* a power of two, above the US texts of a game */

typedef struct us_texts {
    uint8_t *bytes;
    size_t size, capacity;
    language_slot slots[SLOTS];    /* by the hash of their bytes, the first identifier of each text */
} us_texts;

/* Every US text, by its bytes; 0, or -1 when one cannot be read. */
static int read_us(us_texts *t, const OraclesRom *rom, const OraclesTextSource *source)
{
    const unsigned groups = oracles_text_groups(rom, source);
    uint8_t text[ORACLES_TEXT_MAX];
    for (unsigned group = ORACLES_TEXT_DICTIONARY_GROUPS; group < groups; group++) {
        const int count = oracles_text_count(rom, source, group);
        if (count < 0) return -1;
        for (int index = 0; index < count; index++) {
            size_t length = 0;
            if (oracles_text_decode(rom, source, group, (unsigned)index, text, sizeof text, &length) != ORACLES_OK) return -1;
            language_slot *slot = language_slot_of(t->slots, SLOTS, t->bytes, text, length);
            if (slot->length) continue;   /* an earlier identifier has it */
            if (t->size + length > t->capacity) {
                const size_t capacity = (t->capacity ? 2u * t->capacity : 1u << 18) + length;
                uint8_t *bytes = realloc(t->bytes, capacity);
                if (!bytes) return -1;
                t->bytes = bytes;
                t->capacity = capacity;
            }
            memcpy(t->bytes + t->size, text, length);
            *slot = (language_slot){ (uint32_t)t->size, (uint16_t)length, (uint16_t)((group - ORACLES_TEXT_DICTIONARY_GROUPS) << 8 | (unsigned)index) };
            t->size += length;
        }
    }
    return 0;
}

/* A fan game's texts beside the US ones: the two images' text sources, and every US text by its bytes. */
typedef struct fan_texts {
    OraclesRom us_rom, fan_rom;
    OraclesTextSource us_source, fan_source;
    unsigned us_groups, groups;
    us_texts *us;
} fan_texts;

static void close_texts(fan_texts *f)
{
    if (f->us) free(f->us->bytes);
    free(f->us);
    f->us = NULL;
}

/* The texts of the fan game `image`, whose profile is `target`, and of the US ROM `us` of its Oracle; 0, or -1 with the
 * reason. */
static int open_texts(fan_texts *f, const language_target *target, const uint8_t *us, size_t us_size, const uint8_t *image, size_t size,
                      char *error, size_t capacity)
{
    memset(f, 0, sizeof *f);
    f->us_rom = (OraclesRom){ us, us_size };
    f->fan_rom = (OraclesRom){ image, size };
    if (language_source_us(&f->us_rom, target->game, &f->us_source) != 0 || language_source_tables(&f->fan_rom, target->tables, &f->fan_source) != 0) {
        snprintf(error, capacity, "the texts' tables of %s cannot be read", target->name);
        return -1;
    }
    if (!(f->us = calloc(1, sizeof *f->us))) { snprintf(error, capacity, "out of memory"); return -1; }
    if (read_us(f->us, &f->us_rom, &f->us_source) != 0) { snprintf(error, capacity, "the US texts cannot be read"); close_texts(f); return -1; }
    f->us_groups = oracles_text_groups(&f->us_rom, &f->us_source);
    f->groups = oracles_text_groups(&f->fan_rom, &f->fan_source);
    if (!f->groups) { snprintf(error, capacity, "the fan game's texts cannot be read"); close_texts(f); return -1; }
    return 0;
}

/* The identifier whose translation the fan game's text TX_`id` (internal group, index) takes: its own when its text is
 * the US one, else the first whose is; -1 for the fan game's own. */
static int us_identifier(fan_texts *f, unsigned group, unsigned index, unsigned id)
{
    uint8_t text[ORACLES_TEXT_MAX], original[ORACLES_TEXT_MAX];
    size_t length = 0, us_length = 0;
    if (oracles_text_decode(&f->fan_rom, &f->fan_source, group, index, text, sizeof text, &length) != ORACLES_OK) return -1;
    if (language_text_of(&f->us_rom, &f->us_source, f->us_groups, id, original, &us_length) == 0 && us_length == length && !memcmp(original, text, length))
        return (int)id;
    const language_slot *slot = language_slot_of(f->us->slots, SLOTS, f->us->bytes, text, length);
    return slot->length ? slot->id : -1;
}

static OraclesLanguagePack *fail(OraclesLanguagePack *pack, fan_texts *f, char *error, size_t capacity, const char *message)
{
    if (message) snprintf(error, capacity, "%s", message);
    oracles_language_pack_free(pack);
    close_texts(f);
    return NULL;
}

OraclesLanguagePack *oracles_language_pack_derive(const OraclesLanguagePack *base, const uint8_t *us, size_t us_size, const uint8_t *image,
                                                  size_t size, char *error, size_t capacity)
{
    OraclesRomInfo us_info, info;
    language_target target;
    if (oracles_rom_identify(us, us_size, &us_info, error, capacity) != 0 || strcmp(us_info.sha1, base->image) != 0) {
        snprintf(error, capacity, "the US ROM is not the one the language pack of %s translates", oracles_game_name(base->game));
        return NULL;
    }
    if (oracles_rom_identify(image, size, &info, error, capacity) != 0) return NULL;
    if (language_target_of(info.sha1, &target) != 0 || target.original || target.game != base->game || target.halves > 1u) {
        snprintf(error, capacity, "not a fan game made on %s whose texts the port knows (SHA-1 %s)", oracles_game_name(base->game), info.sha1);
        return NULL;
    }
    fan_texts f;
    if (open_texts(&f, &target, us, us_size, image, size, error, capacity) != 0) return NULL;
    OraclesLanguagePack *pack = language_pack_new(base->game, base->source, info.sha1);
    if (!pack) return fail(pack, &f, error, capacity, "out of memory");
    for (unsigned group = ORACLES_TEXT_DICTIONARY_GROUPS; group < f.groups; group++) {
        const int count = oracles_text_count(&f.fan_rom, &f.fan_source, group);
        for (int index = 0; index < count; index++) {
            const unsigned id = (group - ORACLES_TEXT_DICTIONARY_GROUPS) << 8 | (unsigned)index;
            const int from = us_identifier(&f, group, (unsigned)index, id);
            for (OraclesLanguage language = ORACLES_LANGUAGE_ENGLISH_EU; language < ORACLES_LANGUAGE_COUNT; language++) {
                OraclesLanguageSource source = ORACLES_LANGUAGE_SOURCE_FAN;
                size_t translated_length = 0;
                const uint8_t *translated = from < 0 ? NULL : oracles_language_pack_text(base, language, (unsigned)from, &translated_length);
                if (from >= 0) {
                    source = oracles_language_pack_source_of(base, language, (unsigned)from);
                    if (translated && (unsigned)from != id) source = ORACLES_LANGUAGE_SOURCE_MOVED;
                    if (!translated) source = ORACLES_LANGUAGE_SOURCE_KEPT;
                }
                if (language_pack_add(pack, language, id, source, translated, translated ? translated_length : 0) != 0)
                    return fail(pack, &f, error, capacity, "out of memory");
            }
        }
    }
    close_texts(&f);
    /* The European glyphs, but where the fan game drew its own over the US font. */
    const size_t us_font = language_font_us(base->game), font = oracles_rom_offset(target.tables->font_start.bank, target.tables->font_start.addr);
    for (unsigned g = 0; g < base->glyph_count; g++) {
        const size_t at = base->characters[g] * GLYPH_BYTES;
        if (us_font + at + GLYPH_BYTES > us_size || font + at + GLYPH_BYTES > size || memcmp(us + us_font + at, image + font + at, GLYPH_BYTES)) continue;
        pack->characters[pack->glyph_count] = base->characters[g];
        memcpy(pack->bitmaps + pack->glyph_count * GLYPH_BYTES, base->bitmaps + g * GLYPH_BYTES, GLYPH_BYTES);
        pack->glyph_count++;
    }
    return pack;
}
