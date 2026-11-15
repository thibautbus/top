/* A language composed into the image the core runs.
 *
 * The image is the one whose texts the pack translates: the US ROM of the pack's game, alone or with the mods' houses
 * (mod_compose.c), which leave the language's banks empty, and grows to 2 MiB; or a fan game made on it, which keeps
 * its size and leaves its own empty banks to the language (its profile names the first, profile.h).  From that bank
 * (ORACLES_IMAGE_LANGUAGE_BANK in an original) to bank $7f: the index (oracles_rom.h), then the language's texts, each within one bank and identical texts stored once, as the game reads them: a text crosses
 * no bank, so nothing depends on how the game steps from one bank to the next.  An entry goes to every text the US
 * game has: the text the pack serves for it, or English when the pack keeps it or has none (language_extract.c says
 * which, and checked every text it serves against the US one).  The glyphs of the European font that differ from the US one are written
 * over it, where the game reads each character as it prints it (ROM_DATA_FORMATS, section 8.1).  The graphics that
 * carry text go in the first megabyte of an original, their headers repointed (language_gfx.c); a fan game keeps its
 * own.  Data only: the game's code is the image's, and so is every byte it reads in English.
 *
 * For the compact text, the texts are laid out again (language_wrap.c) in the font of the image composed, its
 * European glyphs written first: a text laid out again is stored so, its entry marked (ORACLES_LANGUAGE_LAID_OUT); one
 * the game's tables keep (config/vwf_keep_<game>.tsv), or that another text calls or jumps to within a line, keeps its
 * layout.  English (US) has no pack: its index serves the US texts laid out again, the others kept in English; a fan
 * game's English is so laid out, its texts read in its image (the compact text lays it out in its English alone).  An
 * image of two games that holds its own language (Oracles Combined) has no pack either: each half's texts, read from its
 * three bases, are laid out in that language in the half's font, its index and texts in the half's banks from the same
 * first one, the half's game's table of kept texts read, the pages noted where they lie in the whole image. */
#include "language_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BANK 0x4000u
#define ENTRY_BYTES 3u
#define STORED_SLOTS 8192u   /* a power of two, above the texts of a language */
#define GROUP_TEXTS 0x100u
#define INVENTORY_GROUP 0x09u
#define PAGES_A_TEXT 64u   /* TX_09xx: the inventory's names and descriptions, which scroll on a line of their own */
/* Japanese in the wide box: kept as the game breaks its lines (0); or laid out again on its width like the other
 * languages, the lines it joins taking a (full-width) space (1).  Changing it changes the Japanese images:
 * ORACLES_LANGUAGE_PACK_REVISION_JA goes up with it, so that no savestate taken with the other loads. */
#define JAPANESE_LAID_OUT_WIDE 0

typedef struct composition {
    uint8_t *im;                  /* the half composed: the image itself but for an image of two games */
    size_t size;                  /* its bytes */
    size_t base;                  /* where it starts in the image */
    size_t next;                  /* the next free byte of the language's banks */
    size_t end;                   /* past their last */
    language_slot slots[STORED_SLOTS];   /* the texts stored, by the hash of their bytes */
    /* The compact text's layout: NULL without it. */
    language_wrap *wrap;
    language_keep *keep;
    uint8_t inline_target[0x100][GROUP_TEXTS / 8u];   /* by internal group: the texts called or jumped to within a line */
    uint8_t laid[ORACLES_TEXT_MAX];
    size_t page_from[PAGES_A_TEXT], page_to[PAGES_A_TEXT];   /* the pages of `laid` laid out again, */
    unsigned pages;                                          /* how many */
    OraclesLanguagePages *out_pages;                         /* where they go once stored: NULL, not wanted */
} composition;

/* Where the text lies in the image, stored there now if it was not; 0 when the banks are full. */
static size_t store(composition *c, const uint8_t *text, size_t length, unsigned *bytes)
{
    language_slot *slot = language_slot_of(c->slots, STORED_SLOTS, c->im, text, length);
    if (slot->length) return slot->at;
    if (c->next % BANK + length > BANK) c->next = (c->next / BANK + 1u) * BANK;
    if (c->next + length > c->end) return 0;
    const size_t at = c->next;
    memcpy(c->im + at, text, length);
    c->next += length;
    *slot = (language_slot){ (uint32_t)at, (uint16_t)length, 0 };
    *bytes += (unsigned)length;
    return at;
}

static void put_entry(uint8_t *entry, unsigned bank, unsigned addr)
{
    entry[0] = (uint8_t)bank;
    entry[1] = (uint8_t)addr;
    entry[2] = (uint8_t)(addr >> 8);
}

/* The text of TX_`id` the language has: the pack's, or without one the image's own (`us`, decoded into `buffer`);
 * NULL when it has none. */
static const uint8_t *text_of(const composition *c, const OraclesLanguagePack *pack, OraclesLanguage language, const OraclesTextSource *us,
                              unsigned groups, unsigned id, uint8_t *buffer, size_t *length)
{
    if (pack) return oracles_language_pack_text(pack, language, id, length);
    const OraclesRom rom = { c->im, c->size };
    return language_text_of(&rom, us, groups, id, buffer, length) == 0 ? buffer : NULL;
}

/* The text served for TX_`id` laid out again: `text` itself, or c->laid; *laid_out set when it is. */
static const uint8_t *laid_out(composition *c, OraclesLanguage language, unsigned group, unsigned id, const uint8_t *text, size_t *length,
                               int *laid, OraclesLanguageReport *report)
{
    *laid = 0;
    if (group == ORACLES_TEXT_DICTIONARY_GROUPS + INVENTORY_GROUP) return text;
    const unsigned before = language_text_boxes(text, *length);
    report->boxes += before;
    const uint8_t *served = text;
    if (language_keep_has(c->keep, language, id) || c->inline_target[group][(id & 0xffu) / 8u] & 1u << (id & 7u)) report->layout_kept++;
    else {
        size_t n = 0;
        const language_wrap_result result = language_wrap_text_pages(c->wrap, text, *length, c->laid, sizeof c->laid, &n, c->page_from, c->page_to,
                                                                     PAGES_A_TEXT, &c->pages);
        if (result == WRAP_DONE) {
            served = c->laid;
            *length = n;
            *laid = 1;
            report->laid_out++;
        } else if (result == WRAP_FAILED) report->layout_failed++;
    }
    report->boxes_laid_out += served == text ? before : language_text_boxes(served, *length);
    return served;
}

/* The pages of the text laid out again just stored at `at`, where they lie in the image. */
static void note_pages(composition *c, size_t at)
{
    OraclesLanguagePages *out = c->out_pages;
    for (unsigned i = 0; i < c->pages; i++) {
        if (i >= PAGES_A_TEXT || out->count >= out->capacity) { out->overflowed++; continue; }
        out->ranges[2u * out->count] = (uint32_t)(c->base + at + c->page_from[i]);
        out->ranges[2u * out->count + 1u] = (uint32_t)(c->base + at + c->page_to[i]);
        out->count++;
    }
}

/* The entries of one internal group of the US text, written at `entries`. */
static int compose_group(composition *c, const OraclesLanguagePack *pack, OraclesLanguage language, const OraclesTextSource *us, unsigned groups,
                         unsigned group, unsigned count, uint8_t *entries, OraclesLanguageReport *report, char *error, size_t capacity)
{
    uint8_t buffer[ORACLES_TEXT_MAX];
    for (unsigned index = 0; index < count; index++) {
        const unsigned id = (group - ORACLES_TEXT_DICTIONARY_GROUPS) << 8 | index;
        uint8_t *entry = entries + ENTRY_BYTES * index;
        size_t length = 0;
        const uint8_t *text = text_of(c, pack, language, us, groups, id, buffer, &length);
        int laid = 0;
        if (text && c->wrap) text = laid_out(c, language, group, id, text, &length, &laid, report);
        if (!text || (!pack && !laid)) {
            const int kept = !pack || oracles_language_pack_source_of(pack, language, id) != ORACLES_LANGUAGE_SOURCE_MISSING;
            put_entry(entry, 0, kept ? ORACLES_LANGUAGE_KEPT : 0u);
            if (kept) report->english++;
            else report->missing++;
            continue;
        }
        const size_t before = c->next;
        const size_t at = store(c, text, length, &report->bytes);
        if (!at) { snprintf(error, capacity, "the %s texts do not fit the image's language banks", oracles_language_code(language)); return -1; }
        if (laid && c->out_pages && c->next != before) note_pages(c, at);   /* stored now: an identical text stored before has its pages */
        put_entry(entry, (unsigned)(at / BANK), (BANK + (unsigned)(at % BANK)) | (laid ? ORACLES_LANGUAGE_LAID_OUT : 0u));
        report->served++;
    }
    return 0;
}

/* The compact text's layout of a language, in the font of the image as composed: its words written with a hyphen
 * learned and the texts other texts call or jump to within a line found, over all its texts. */
static int prepare_layout(composition *c, const OraclesLanguagePack *pack, OraclesLanguage language, const OraclesTextSource *us, unsigned groups,
                          const language_target *target, const OraclesCompatProfile *half, const OraclesLanguageLayout *layout, char *error,
                          size_t capacity)
{
    const OraclesRom rom = { c->im, c->size };
    const OraclesGuestTables *t = oracles_compat_guest_tables(half);
    OraclesTextFont font;
    const int read = oracles_text_font(&rom, oracles_rom_offset(t->font_start.bank, t->font_start.addr), &font);
    if (read == ORACLES_OK && layout->fixed) oracles_text_font_fixed(&font, &font);
    if (read != ORACLES_OK || (layout->lines && layout->lines != 2u) || !(c->wrap = language_wrap_new(&font, layout->width, language))) {
        snprintf(error, capacity, "the compact text's layout of %u pixels a line cannot be made", layout->width);
        return -1;
    }
    if (!(c->keep = language_keep_parse(language_keep_table_half(target->revision, oracles_compat_family(half)), error, capacity))) return -1;
    uint8_t buffer[ORACLES_TEXT_MAX], targets[64];
    for (unsigned group = ORACLES_TEXT_DICTIONARY_GROUPS; group < groups; group++) {
        const int count = oracles_text_count(&rom, us, group);
        for (int index = 0; index < count; index++) {
            size_t length = 0;
            const uint8_t *text = text_of(c, pack, language, us, groups, (group - ORACLES_TEXT_DICTIONARY_GROUPS) << 8 | (unsigned)index, buffer, &length);
            if (!text) continue;
            language_wrap_learn(c->wrap, text, length);
            const unsigned n = language_text_inline_targets(text, length, targets, sizeof targets);
            for (unsigned i = 0; i < n; i++) c->inline_target[group][targets[i] / 8u] |= (uint8_t)(1u << (targets[i] & 7u));
        }
    }
    return 0;
}

static void free_composition(composition *c)
{
    language_wrap_free(c->wrap);
    language_keep_free(c->keep);
    free(c);
}

/* The language composed into one half of the image `im` (the image itself but for an image of two games): its index and
 * its texts in the half's banks from the target's first. */
static int compose_half(const OraclesLanguagePack *pack, OraclesLanguage language, const language_target *target, unsigned h, uint8_t *im,
                        size_t composed, const OraclesLanguageLayout *layout, OraclesLanguageReport *report, char *error, size_t capacity)
{
    const OraclesCompatProfile *half = h ? oracles_compat_half(target->profile, h) : target->profile;
    const size_t first = (size_t)target->first_bank * BANK, end = (LANGUAGE_LAST_BANK + 1u) * BANK;
    composition *c = calloc(1, sizeof *c);
    if (!c) { snprintf(error, capacity, "out of memory"); return -1; }
    c->base = (size_t)oracles_compat_rom_bank_base(half) * BANK;
    c->im = (uint8_t *)oracles_rom_half(im, composed, oracles_compat_rom_bank_base(half), &c->size);
    c->end = end;
    c->out_pages = layout ? layout->pages : NULL;
    const OraclesRom rom = { c->im, c->size };
    OraclesTextSource us;
    const unsigned groups = c->im && language_source_profile(&rom, half, &us) == 0 ? oracles_text_groups(&rom, &us) : 0;
    unsigned entries = 0;
    for (unsigned group = ORACLES_TEXT_DICTIONARY_GROUPS; group < groups; group++) {
        const int count = oracles_text_count(&rom, &us, group);
        entries += count > 0 ? (unsigned)count : 0u;
    }
    c->next = first + ORACLES_LANGUAGE_INDEX_GROUPS + 4u * groups + ENTRY_BYTES * entries;
    if (!groups || groups > 0xffu || c->next > first + BANK) {
        snprintf(error, capacity, "the image's text tables are not those of %s", target->name);
        free_composition(c);
        return -1;
    }
    /* The glyphs first: the layout measures the texts in the font the game will print them in. */
    const uint8_t *characters = NULL, *bitmaps = NULL;
    const unsigned glyphs = pack ? oracles_language_pack_glyphs(pack, &characters, &bitmaps) : 0;
    const OraclesGuestTables *t = oracles_compat_guest_tables(half);
    const size_t font = oracles_rom_offset(t->font_start.bank, t->font_start.addr);
    for (unsigned g = 0; g < glyphs; g++) memcpy(c->im + font + characters[g] * GLYPH_BYTES, bitmaps + g * GLYPH_BYTES, GLYPH_BYTES);
    int status = layout && layout->width ? prepare_layout(c, pack, language, &us, groups, target, half, layout, error, capacity) : 0;
    uint8_t *index = c->im + first;
    memcpy(index, ORACLES_LANGUAGE_INDEX_MAGIC, ORACLES_LANGUAGE_INDEX_MAGIC_BYTES);
    index[16] = ORACLES_LANGUAGE_INDEX_VERSION;
    index[17] = (uint8_t)groups;
    memcpy(index + ORACLES_LANGUAGE_INDEX_CODE, oracles_language_code(language), strlen(oracles_language_code(language)));
    size_t at = ORACLES_LANGUAGE_INDEX_GROUPS + 4u * groups;
    for (unsigned group = ORACLES_TEXT_DICTIONARY_GROUPS; group < groups && status == 0; group++) {
        const int count = oracles_text_count(&rom, &us, group);
        uint8_t *row = index + ORACLES_LANGUAGE_INDEX_GROUPS + 4u * group;
        if (count <= 0) continue;
        row[0] = (uint8_t)at; row[1] = (uint8_t)(at >> 8); row[2] = (uint8_t)count; row[3] = (uint8_t)(count >> 8);
        status = compose_group(c, pack, language, &us, groups, group, (unsigned)count, index + at, report, error, capacity);
        at += ENTRY_BYTES * (unsigned)count;
    }
    free_composition(c);
    return status;
}

/* The composition of `language` (without a pack: the image's own texts laid out again, English (US)'s or the language an
 * image of two games holds), a half at a time. */
static int compose(const OraclesLanguagePack *pack, OraclesLanguage language, const language_target *target, const uint8_t *image, size_t size,
                   const OraclesLanguageLayout *layout, uint8_t **out, size_t *out_size, OraclesLanguageReport *report, char *error, size_t capacity)
{
    const size_t first = (size_t)target->first_bank * BANK, end = (LANGUAGE_LAST_BANK + 1u) * BANK;
    size_t composed = ORACLES_IMAGE_SIZE;
    if (target->original) {
        if (size != ORACLES_IMAGE_SIZE / 2u && size != ORACLES_IMAGE_SIZE) { snprintf(error, capacity, "a language plays on the US ROM only"); return -1; }
    } else {
        OraclesRomInfo info;
        char reason[128];
        if (oracles_rom_identify(image, size, &info, reason, sizeof reason) != 0 || strcmp(info.sha1, target->sha1) != 0 || size < end) {
            snprintf(error, capacity, "the pack's language plays on %s only", target->name);
            return -1;
        }
        composed = size;
    }
    for (unsigned h = 0; h < target->halves; h++) {
        size_t half_size = 0;
        const uint8_t *half = oracles_rom_half(image, size, oracles_compat_rom_bank_base(h ? oracles_compat_half(target->profile, h) : target->profile), &half_size);
        for (size_t i = first; half && i < half_size && i < end; i++)
            if (half[i]) { snprintf(error, capacity, "the image's language banks are taken"); return -1; }
    }
    uint8_t *im = calloc(1, composed);
    if (!im) { snprintf(error, capacity, "out of memory"); return -1; }
    memcpy(im, image, size);
    if (layout && layout->pages) layout->pages->count = layout->pages->overflowed = 0;
    int status = 0;
    for (unsigned h = 0; h < target->halves && status == 0; h++) status = compose_half(pack, language, target, h, im, composed, layout, report, error, capacity);
    if (status == 0 && pack && target->original) status = language_gfx_compose(pack, language, im, composed, report, error, capacity);
    if (status == 0) {
        oracles_image_finish(im, composed);
        *out = im;
        *out_size = composed;
    } else free(im);
    return status;
}

int oracles_language_compose(const OraclesLanguagePack *pack, OraclesLanguage language, const uint8_t *image, size_t size, const OraclesLanguageLayout *layout,
                             uint8_t **out, size_t *out_size, OraclesLanguageReport *report, char *error, size_t capacity)
{
    *out = NULL;
    memset(report, 0, sizeof *report);
    language_target target;
    if (language_target_of(pack->image, &target) != 0 || target.game != pack->game) {
        snprintf(error, capacity, "the pack translates an image the port does not know (SHA-1 %s)", pack->image);
        return -1;
    }
    if (target.halves > 1u) {
        snprintf(error, capacity, "%s holds its own language: no pack's is composed into it", target.name);
        return -1;
    }
    if (language == ORACLES_LANGUAGE_ENGLISH || !oracles_language_pack_count(pack, language)) {
        snprintf(error, capacity, "the pack has no %s text", oracles_language_code(language));
        return -1;
    }
    if (layout && layout->width && !target.original) { snprintf(error, capacity, "the compact text lays out a fan game's texts in its English alone"); return -1; }
    if (language == ORACLES_LANGUAGE_JAPANESE && !JAPANESE_LAID_OUT_WIDE) layout = NULL;
    return compose(pack, language, &target, image, size, layout, out, out_size, report, error, capacity);
}

int oracles_language_compose_english(const char *sha1, const uint8_t *image, size_t size, const OraclesLanguageLayout *layout, uint8_t **out,
                                     size_t *out_size, OraclesLanguageReport *report, char *error, size_t capacity)
{
    *out = NULL;
    memset(report, 0, sizeof *report);
    language_target target;
    if (language_target_of(sha1, &target) != 0) {
        snprintf(error, capacity, "the compact text lays out the texts of the US ROM of Ages or Seasons, or of a fan game the port binds the texts of");
        return -1;
    }
    if (!layout || !layout->width) { snprintf(error, capacity, "English (US) is composed only laid out again"); return -1; }
    return compose(NULL, target.own, &target, image, size, layout, out, out_size, report, error, capacity);
}
