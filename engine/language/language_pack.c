/* A language pack: the texts and the glyphs extracted from a European, a Japanese or a Polish ROM, in memory and in its
 * file.
 *
 * The file, little-endian, version ORACLES_LANGUAGE_PACK_VERSION:
 *   magic        16 bytes   "ORACLES LANGPACK"
 *   version      u16
 *   game         u8         1 Ages, 2 Seasons (OraclesGame)
 *   source       40 bytes   the European, Japanese or Polish ROM's SHA-1, lower-case hexadecimal
 *   image        40 bytes   the SHA-1 of the image whose texts the entries stand for: the US ROM of the game, or a fan
 *                           game made on it (language_fan.c)
 *   fingerprint  40 bytes   the code and the tables it was made with (language.h, ORACLES_LANGUAGE_PACK_REVISION, or
 *                           ORACLES_LANGUAGE_PACK_REVISION_JA for a Japanese ROM's, _PL for a Polish ROM's)
 *   glyphs       u8         n, then n times: the character (u8) and its 16 bytes, 1 bit a pixel, as the font has them
 *   languages    u8         those it holds (a European ROM's five, a Japanese ROM's Japanese, a Polish ROM's
 *                           Polish), then for each: its
 *                           code (its length, u8, then its characters: "fr", "en-eu"), its
 *                           number of entries (u16), for each entry the US text's identifier (u16, TX_GGII), its
 *                           source (u8, OraclesLanguageSource) and its text's length (u16, 0 for the sources that keep
 *                           the US English), by increasing identifier, then the texts' bytes one after the other;
 *                           then its graphics (u16), for each the US graphic it stands for (its bank, u8, and its
 *                           CPU address, u16), its size in blocks minus 1 (u8), its compression mode (u8) and its
 *                           length (u16), by increasing source, then their bytes
 *   checksum     40 bytes   the SHA-1 of every byte before it
 * A text is as oracles_text_decode gives it, the dictionary expanded, the control codes kept, a 0 at its end; or as a
 * rule adapted it, or the port's own.  A graphic is the language's version of a US graphic that carries text, made
 * from the European, Japanese or Polish ROM's (language_gfx.c), compressed as a graphics header reads it, or a table
 * written raw in place of the US one (the Polish title's sprites of PRESS START, the Japanese secrets' symbols, in bank
 * 0, the only graphic whose CPU address is below $4000); read back, it must decompress to its size and end where its
 * length does.  The file holds what the user's ROM holds and the port's own texts, nothing
 * else: it is made on the user's machine and never shipped. */
#include "language_internal.h"

#include "sha1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC "ORACLES LANGPACK"
#define MAGIC_BYTES 16u
#define SHA1_HEX 40u
#define ENTRY_BYTES 5u
#define GRAPHIC_BYTES 7u
#define GRAPHIC_MAX 2048u
#define CODE_MAX 8u   /* a language's code and its terminator */

extern const unsigned char oracles_language_exceptions_ages[];
extern const unsigned char oracles_language_exceptions_seasons[];
extern const unsigned char oracles_language_own_texts[];
extern const unsigned char oracles_language_corrections[];
extern const unsigned char oracles_language_exceptions_ages_ja[];
extern const unsigned char oracles_language_exceptions_seasons_ja[];
extern const unsigned char oracles_language_own_texts_ja[];
extern const unsigned char oracles_language_exceptions_ages_pl[];
extern const unsigned char oracles_language_exceptions_seasons_pl[];
extern const unsigned char oracles_language_corrections_pl[];

static const char *const source_names[ORACLES_LANGUAGE_SOURCE_COUNT] = { "same", "moved", "adapted", "own", "kept", "refused", "missing", "fan" };

const char *oracles_language_source_name(OraclesLanguageSource source)
{
    return source < ORACLES_LANGUAGE_SOURCE_COUNT ? source_names[source] : "?";
}

/* Whether a source has a text. */
static int has_text(OraclesLanguageSource source) { return source <= ORACLES_LANGUAGE_SOURCE_OWN; }

/* ---- the fingerprint ------------------------------------------------------------------------ */

unsigned language_fingerprint_tables(OraclesGame game, const unsigned char *tables[LANGUAGE_FINGERPRINT_TABLES])
{
    tables[0] = game == ORACLES_GAME_AGES ? oracles_language_exceptions_ages : oracles_language_exceptions_seasons;
    tables[1] = oracles_language_own_texts;
    tables[2] = oracles_language_corrections;
    return LANGUAGE_FINGERPRINT_TABLES;
}

unsigned language_fingerprint_tables_japanese(OraclesGame game, const unsigned char *tables[LANGUAGE_FINGERPRINT_TABLES])
{
    tables[0] = game == ORACLES_GAME_AGES ? oracles_language_exceptions_ages_ja : oracles_language_exceptions_seasons_ja;
    tables[1] = oracles_language_own_texts_ja;
    return 2u;
}

void language_fingerprint_of(unsigned revision, const unsigned char *const *tables, unsigned count, const language_target *target, char out[41])
{
    size_t size = 160;
    for (unsigned t = 0; t < count; t++) size += strlen((const char *)tables[t]) + 32u;
    char *text = malloc(size);
    if (!text) { snprintf(out, 41, "%040u", 0u); return; }
    const OraclesGuestTables *g = target->tables;
    size_t n = (size_t)snprintf(text, size, "revision %u\ntexts %02x:%04x %02x:%04x %02x:%04x split %02x font %02x:%04x\n", revision,
                                g->text_table_table.bank, g->text_table_table.addr, g->text_offset1_table.bank, g->text_offset1_table.addr,
                                g->text_offset2_table.bank, g->text_offset2_table.addr, g->text_offset_split_index, g->font_start.bank, g->font_start.addr);
    for (unsigned t = 0; t < count; t++) {
        const size_t length = strlen((const char *)tables[t]);
        n += (size_t)snprintf(text + n, size - n, "table %zu\n", length);
        memcpy(text + n, tables[t], length);
        n += length;
    }
    oracles_sha1_hex((const uint8_t *)text, n, out);
    free(text);
}

unsigned language_fingerprint_tables_polish(OraclesGame game, const unsigned char *tables[LANGUAGE_FINGERPRINT_TABLES])
{
    tables[0] = game == ORACLES_GAME_AGES ? oracles_language_exceptions_ages_pl : oracles_language_exceptions_seasons_pl;
    tables[1] = oracles_language_corrections_pl;
    return 2u;
}

language_edition language_edition_of(const OraclesRomInfo *info)
{
    return language_japanese(info) ? EDITION_JAPANESE : language_polish(info) ? EDITION_POLISH : EDITION_EUROPEAN;
}

void language_fingerprint(const language_target *target, language_edition edition, char out[41])
{
    const unsigned char *tables[LANGUAGE_FINGERPRINT_TABLES];
    const unsigned count = edition == EDITION_JAPANESE ? language_fingerprint_tables_japanese(target->game, tables)
                         : edition == EDITION_POLISH ? language_fingerprint_tables_polish(target->game, tables)
                                                     : language_fingerprint_tables(target->game, tables);
    const unsigned revision = edition == EDITION_JAPANESE ? ORACLES_LANGUAGE_PACK_REVISION_JA
                            : edition == EDITION_POLISH ? ORACLES_LANGUAGE_PACK_REVISION_PL : ORACLES_LANGUAGE_PACK_REVISION;
    language_fingerprint_of(revision, tables, count, target, out);
}

/* The edition of the ROM of SHA-1 `source`. */
static language_edition source_edition(const char *source)
{
    OraclesRomInfo info = { 0 };
    return oracles_rom_known(source, &info) == 0 ? language_edition_of(&info) : EDITION_EUROPEAN;
}

/* The language a pack of `edition` holds alone; ORACLES_LANGUAGE_COUNT for the European ROM's five. */
static OraclesLanguage edition_language(language_edition edition)
{
    return edition == EDITION_JAPANESE ? ORACLES_LANGUAGE_JAPANESE : edition == EDITION_POLISH ? ORACLES_LANGUAGE_POLISH : ORACLES_LANGUAGE_COUNT;
}

OraclesLanguagePack *language_pack_new(OraclesGame game, const char *source, const char *image)
{
    OraclesLanguagePack *pack = calloc(1, sizeof *pack);
    if (!pack) return NULL;
    pack->game = game;
    snprintf(pack->source, sizeof pack->source, "%s", source);
    snprintf(pack->image, sizeof pack->image, "%s", image);
    language_target target;
    if (language_target_of(image, &target) == 0) language_fingerprint(&target, source_edition(source), pack->fingerprint);
    return pack;
}

/* Room for `length` more bytes. */
static int reserve(OraclesLanguagePack *pack, size_t length)
{
    if (pack->size + length <= pack->capacity) return 0;
    const size_t capacity = (pack->capacity ? pack->capacity * 2u : 1u << 18) + length;
    uint8_t *bytes = realloc(pack->bytes, capacity);
    if (!bytes) return -1;
    pack->bytes = bytes;
    pack->capacity = capacity;
    return 0;
}

int language_pack_add(OraclesLanguagePack *pack, OraclesLanguage language, unsigned id, OraclesLanguageSource source, const uint8_t *text, size_t length)
{
    const unsigned n = pack->count[language];
    if (language == ORACLES_LANGUAGE_ENGLISH || language >= ORACLES_LANGUAGE_COUNT || source >= ORACLES_LANGUAGE_SOURCE_COUNT || id > 0xffffu
        || (n && pack->texts[language][n - 1u].id >= id)) return -1;
    if (has_text(source) ? !text || !length || length > ORACLES_TEXT_MAX || text[length - 1u] != 0 : length != 0) return -1;
    if (reserve(pack, length) != 0) return -1;
    if (!(n & (n - 1u))) {   /* 0, 1, 2, 4...: the array doubles */
        pack_text *texts = realloc(pack->texts[language], (n ? 2u * n : 1u) * sizeof *texts);
        if (!texts) return -1;
        pack->texts[language] = texts;
    }
    if (length) memcpy(pack->bytes + pack->size, text, length);
    pack->texts[language][n] = (pack_text){ (uint16_t)id, (uint8_t)source, (uint16_t)length, (uint32_t)pack->size };
    pack->size += length;
    pack->count[language] = n + 1u;
    return 0;
}

int language_pack_add_graphic(OraclesLanguagePack *pack, OraclesLanguage language, const OraclesLanguageGraphic *graphic)
{
    const unsigned n = language < ORACLES_LANGUAGE_COUNT ? pack->graphic_count[language] : 0;
    const unsigned source = (unsigned)graphic->bank << 16 | graphic->address;
    if (language == ORACLES_LANGUAGE_ENGLISH || language >= ORACLES_LANGUAGE_COUNT || graphic->bank > 0x3fu
        || (graphic->bank ? graphic->address < 0x4000u || graphic->address > 0x7fffu : graphic->address > 0x3fffu || graphic->mode)
        || graphic->blocks > 0x7fu || graphic->mode > 3u
        || !graphic->length || graphic->length > GRAPHIC_MAX || !graphic->data
        || (n && ((unsigned)pack->graphics[language][n - 1u].bank << 16 | pack->graphics[language][n - 1u].address) >= source)) return -1;
    uint8_t decompressed[GRAPHIC_MAX];
    size_t consumed = 0;
    if (language_gfx_decompress(graphic->data, graphic->length, 0, graphic->mode, decompressed, 16u * (graphic->blocks + 1u), &consumed) != 0
        || consumed != graphic->length) return -1;
    if (reserve(pack, graphic->length) != 0) return -1;
    if (!(n & (n - 1u))) {   /* 0, 1, 2, 4...: the array doubles */
        pack_graphic *graphics = realloc(pack->graphics[language], (n ? 2u * n : 1u) * sizeof *graphics);
        if (!graphics) return -1;
        pack->graphics[language] = graphics;
    }
    memcpy(pack->bytes + pack->size, graphic->data, graphic->length);
    pack->graphics[language][n] = (pack_graphic){ graphic->bank, graphic->blocks, graphic->mode, graphic->address, graphic->length, (uint32_t)pack->size };
    pack->size += graphic->length;
    pack->graphic_count[language] = n + 1u;
    return 0;
}

unsigned oracles_language_pack_graphic_count(const OraclesLanguagePack *pack, OraclesLanguage language)
{
    return language < ORACLES_LANGUAGE_COUNT ? pack->graphic_count[language] : 0;
}

int oracles_language_pack_graphic(const OraclesLanguagePack *pack, OraclesLanguage language, unsigned n, OraclesLanguageGraphic *graphic)
{
    if (language >= ORACLES_LANGUAGE_COUNT || n >= pack->graphic_count[language]) return -1;
    const pack_graphic *g = &pack->graphics[language][n];
    *graphic = (OraclesLanguageGraphic){ g->bank, g->address, g->blocks, g->mode, g->length, pack->bytes + g->at };
    return 0;
}

void oracles_language_pack_free(OraclesLanguagePack *pack)
{
    if (!pack) return;
    for (unsigned l = 0; l < ORACLES_LANGUAGE_COUNT; l++) { free(pack->texts[l]); free(pack->graphics[l]); }
    free(pack->bytes);
    free(pack);
}

OraclesGame oracles_language_pack_game(const OraclesLanguagePack *pack) { return pack->game; }
const char *oracles_language_pack_source(const OraclesLanguagePack *pack) { return pack->source; }
const char *oracles_language_pack_image(const OraclesLanguagePack *pack) { return pack->image; }
unsigned oracles_language_pack_count(const OraclesLanguagePack *pack, OraclesLanguage language)
{
    return language < ORACLES_LANGUAGE_COUNT ? pack->count[language] : 0;
}

const uint8_t *oracles_language_pack_nth(const OraclesLanguagePack *pack, OraclesLanguage language, unsigned n, unsigned *id,
                                         OraclesLanguageSource *source, size_t *length)
{
    if (language >= ORACLES_LANGUAGE_COUNT || n >= pack->count[language]) return NULL;
    const pack_text *t = &pack->texts[language][n];
    if (id) *id = t->id;
    if (source) *source = (OraclesLanguageSource)t->source;
    if (length) *length = t->length;
    return t->length ? pack->bytes + t->at : NULL;
}

/* The entry of TX_`id` in a language, or NULL. */
static const pack_text *entry_of(const OraclesLanguagePack *pack, OraclesLanguage language, unsigned id)
{
    if (language >= ORACLES_LANGUAGE_COUNT) return NULL;
    unsigned low = 0, high = pack->count[language];
    while (low < high) {
        const unsigned middle = (low + high) / 2u;
        const pack_text *t = &pack->texts[language][middle];
        if (t->id == id) return t;
        if (t->id < id) low = middle + 1u;
        else high = middle;
    }
    return NULL;
}

const uint8_t *oracles_language_pack_text(const OraclesLanguagePack *pack, OraclesLanguage language, unsigned id, size_t *length)
{
    const pack_text *t = entry_of(pack, language, id);
    if (!t || !t->length) return NULL;
    if (length) *length = t->length;
    return pack->bytes + t->at;
}

OraclesLanguageSource oracles_language_pack_source_of(const OraclesLanguagePack *pack, OraclesLanguage language, unsigned id)
{
    const pack_text *t = entry_of(pack, language, id);
    return t ? (OraclesLanguageSource)t->source : ORACLES_LANGUAGE_SOURCE_MISSING;
}

void oracles_language_pack_tally(const OraclesLanguagePack *pack, OraclesLanguage language, unsigned counts[ORACLES_LANGUAGE_SOURCE_COUNT])
{
    memset(counts, 0, ORACLES_LANGUAGE_SOURCE_COUNT * sizeof *counts);
    for (unsigned i = 0; language < ORACLES_LANGUAGE_COUNT && i < pack->count[language]; i++) counts[pack->texts[language][i].source]++;
}

unsigned oracles_language_pack_glyphs(const OraclesLanguagePack *pack, const uint8_t **characters, const uint8_t **bitmaps)
{
    if (characters) *characters = pack->characters;
    if (bitmaps) *bitmaps = pack->bitmaps;
    return pack->glyph_count;
}

/* ---- the file ------------------------------------------------------------------------------ */

/* A pack's file being built in memory. */
typedef struct writer { uint8_t *data; size_t size, capacity; int bad; } writer;

static void put(writer *w, const void *bytes, size_t n)
{
    if (w->bad) return;
    if (w->size + n > w->capacity) {
        const size_t capacity = (w->capacity ? 2u * w->capacity : 1u << 18) + n;
        uint8_t *data = realloc(w->data, capacity);
        if (!data) { w->bad = 1; return; }
        w->data = data;
        w->capacity = capacity;
    }
    memcpy(w->data + w->size, bytes, n);
    w->size += n;
}

static void put8(writer *w, unsigned v) { const uint8_t b = (uint8_t)v; put(w, &b, 1); }
static void put16(writer *w, unsigned v) { put8(w, v & 0xffu); put8(w, v >> 8 & 0xffu); }

/* The file's bytes, its checksum included, in a buffer the caller frees; NULL when out of memory. */
static uint8_t *serialise(const OraclesLanguagePack *pack, size_t *size)
{
    writer w = { NULL, 0, 0, 0 };
    put(&w, MAGIC, MAGIC_BYTES);
    put16(&w, ORACLES_LANGUAGE_PACK_VERSION);
    put8(&w, (unsigned)pack->game);
    put(&w, pack->source, SHA1_HEX);
    put(&w, pack->image, SHA1_HEX);
    put(&w, pack->fingerprint, SHA1_HEX);
    put8(&w, pack->glyph_count);
    for (unsigned g = 0; g < pack->glyph_count; g++) {
        put8(&w, pack->characters[g]);
        put(&w, pack->bitmaps + g * GLYPH_BYTES, GLYPH_BYTES);
    }
    unsigned languages = 0;
    for (unsigned l = ORACLES_LANGUAGE_ENGLISH_EU; l < ORACLES_LANGUAGE_COUNT; l++) languages += pack->count[l] || pack->graphic_count[l];
    put8(&w, languages);
    for (unsigned l = ORACLES_LANGUAGE_ENGLISH_EU; l < ORACLES_LANGUAGE_COUNT; l++) {
        if (!pack->count[l] && !pack->graphic_count[l]) continue;
        const char *code = oracles_language_code((OraclesLanguage)l);
        put8(&w, (unsigned)strlen(code));
        put(&w, code, strlen(code));
        put16(&w, pack->count[l]);
        for (unsigned i = 0; i < pack->count[l]; i++) {
            const pack_text *t = &pack->texts[l][i];
            put16(&w, t->id);
            put8(&w, t->source);
            put16(&w, t->length);
        }
        for (unsigned i = 0; i < pack->count[l]; i++) put(&w, pack->bytes + pack->texts[l][i].at, pack->texts[l][i].length);
        put16(&w, pack->graphic_count[l]);
        for (unsigned i = 0; i < pack->graphic_count[l]; i++) {
            const pack_graphic *g = &pack->graphics[l][i];
            put8(&w, g->bank);
            put16(&w, g->address);
            put8(&w, g->blocks);
            put8(&w, g->mode);
            put16(&w, g->length);
        }
        for (unsigned i = 0; i < pack->graphic_count[l]; i++) put(&w, pack->bytes + pack->graphics[l][i].at, pack->graphics[l][i].length);
    }
    char checksum[SHA1_HEX + 1u] = { 0 };
    if (!w.bad) oracles_sha1_hex(w.data, w.size, checksum);
    put(&w, checksum, SHA1_HEX);
    if (w.bad) { free(w.data); return NULL; }
    *size = w.size;
    return w.data;
}

void oracles_language_pack_digest(const OraclesLanguagePack *pack, char out[41])
{
    size_t size = 0;
    uint8_t *data = serialise(pack, &size);
    snprintf(out, 41, "%.40s", data ? (const char *)data + size - SHA1_HEX : "");
    free(data);
}

int oracles_language_pack_write(const OraclesLanguagePack *pack, const char *path, char *error, size_t capacity)
{
    size_t size = 0;
    uint8_t *data = serialise(pack, &size);
    if (!data) { snprintf(error, capacity, "out of memory"); return -1; }
    /* Written beside it, then renamed over it: an interrupted write never leaves a pack cut short. */
    const size_t length = strlen(path) + 5u;
    char *temporary = malloc(length);
    FILE *f = NULL;
    if (temporary) {
        snprintf(temporary, length, "%s.tmp", path);
        f = fopen(temporary, "wb");
    }
    int written = f && fwrite(data, 1, size, f) == size;
    free(data);
    if (f && fclose(f) != 0) written = 0;
    if (written && rename(temporary, path) != 0) {
        remove(path);   /* a rename that does not replace a file (Windows) */
        written = rename(temporary, path) == 0;
    }
    if (!written) {
        snprintf(error, capacity, "cannot write %s", path);
        if (f) remove(temporary);
    }
    free(temporary);
    return written ? 0 : -1;
}

typedef struct reader { const uint8_t *data; size_t size, at; int bad; } reader;

static const uint8_t *take(reader *r, size_t n)
{
    if (r->bad || n > r->size - r->at) { r->bad = 1; return NULL; }
    const uint8_t *p = r->data + r->at;
    r->at += n;
    return p;
}

static unsigned take8(reader *r) { const uint8_t *p = take(r, 1); return p ? p[0] : 0; }
static unsigned take16(reader *r) { const uint8_t *p = take(r, 2); return p ? (unsigned)(p[0] | p[1] << 8) : 0; }

/* The pack a file's bytes hold; NULL with the reason. */
static OraclesLanguagePack *parse(const uint8_t *data, size_t size, const char *path, char *error, size_t capacity)
{
    reader r = { data, size, 0, 0 };
    const uint8_t *magic = take(&r, MAGIC_BYTES);
    if (!magic || memcmp(magic, MAGIC, MAGIC_BYTES)) { snprintf(error, capacity, "%s is not a language pack", path); return NULL; }
    const unsigned version = take16(&r);
    if (version != ORACLES_LANGUAGE_PACK_VERSION) {
        snprintf(error, capacity, "%s is a language pack of format %u, this version reads %u: extract it again from its ROM", path, version, ORACLES_LANGUAGE_PACK_VERSION);
        return NULL;
    }
    char checksum[SHA1_HEX + 1u];
    if (size >= r.at + SHA1_HEX) oracles_sha1_hex(data, size - SHA1_HEX, checksum);
    if (size < r.at + SHA1_HEX || memcmp(checksum, data + size - SHA1_HEX, SHA1_HEX)) {
        snprintf(error, capacity, "%s is a damaged language pack (its checksum is not its content's): extract it again from its ROM", path);
        return NULL;
    }
    r.size = size -= SHA1_HEX;
    const unsigned game = take8(&r);
    const uint8_t *sha1 = take(&r, SHA1_HEX);
    char source[SHA1_HEX + 1u] = { 0 }, image[SHA1_HEX + 1u] = { 0 };
    if (sha1) memcpy(source, sha1, SHA1_HEX);
    const uint8_t *target_sha1 = take(&r, SHA1_HEX);
    if (target_sha1) memcpy(image, target_sha1, SHA1_HEX);
    const uint8_t *fingerprint = take(&r, SHA1_HEX);
    OraclesRomInfo info = { 0 };
    language_target target;
    const language_edition edition = r.bad ? EDITION_EUROPEAN : source_edition(source);
    if (r.bad || oracles_rom_known(source, &info) != 0 || game == ORACLES_GAME_UNKNOWN
        || (edition != EDITION_EUROPEAN ? info.game : oracles_language_source_game(&info)) != (OraclesGame)game) {
        snprintf(error, capacity, "%s was not extracted from a European, Japanese or Polish ROM the port knows", path);
        return NULL;
    }
    if (language_target_of(image, &target) != 0 || target.game != (OraclesGame)game) {
        snprintf(error, capacity, "%s is not the language pack of a game the port knows", path);
        return NULL;
    }
    char expected[SHA1_HEX + 1u];
    language_fingerprint(&target, edition, expected);
    if (!fingerprint || memcmp(fingerprint, expected, SHA1_HEX)) {
        snprintf(error, capacity, "%s was made by another version of the port or from other tables: extract it again from its ROM", path);
        return NULL;
    }
    OraclesLanguagePack *pack = language_pack_new((OraclesGame)game, source, image);
    if (!pack) { snprintf(error, capacity, "out of memory"); return NULL; }
    pack->glyph_count = take8(&r);
    for (unsigned g = 0; g < pack->glyph_count && !r.bad && g < GLYPHS_MAX; g++) {
        pack->characters[g] = (uint8_t)take8(&r);
        const uint8_t *bitmap = take(&r, GLYPH_BYTES);
        if (bitmap) memcpy(pack->bitmaps + g * GLYPH_BYTES, bitmap, GLYPH_BYTES);
        if (pack->characters[g] < GLYPH_FIRST) r.bad = 1;
    }
    const unsigned languages = take8(&r);
    for (unsigned l = 0; l < languages && !r.bad; l++) {
        const unsigned code_length = take8(&r);
        const uint8_t *code = code_length < CODE_MAX ? take(&r, code_length) : NULL;
        char name[CODE_MAX] = { 0 };
        if (code) memcpy(name, code, code_length);
        OraclesLanguage language;
        if (oracles_language_parse(name, &language) != 0 || language == ORACLES_LANGUAGE_ENGLISH || pack->count[language]
            || (edition == EDITION_EUROPEAN ? language >= ORACLES_LANGUAGE_EUROPEAN_END : language != edition_language(edition))) { r.bad = 1; break; }
        const unsigned count = take16(&r);
        const uint8_t *entries = take(&r, ENTRY_BYTES * count);
        for (unsigned i = 0; entries && i < count && !r.bad; i++) {
            const uint8_t *e = entries + ENTRY_BYTES * i;
            const unsigned id = e[0] | e[1] << 8, source_of = e[2], length = e[3] | e[4] << 8;
            const uint8_t *text = length ? take(&r, length) : NULL;
            if ((length && !text) || source_of >= ORACLES_LANGUAGE_SOURCE_COUNT
                || language_pack_add(pack, language, id, (OraclesLanguageSource)source_of, text, length) != 0) r.bad = 1;
        }
        const unsigned graphics = take16(&r);
        const uint8_t *headers = take(&r, GRAPHIC_BYTES * graphics);
        for (unsigned i = 0; headers && i < graphics && !r.bad; i++) {
            const uint8_t *g = headers + GRAPHIC_BYTES * i;
            OraclesLanguageGraphic graphic = { g[0], (uint16_t)(g[1] | g[2] << 8), g[3], g[4], (uint16_t)(g[5] | g[6] << 8), NULL };
            graphic.data = take(&r, graphic.length);
            if (!graphic.data || language_pack_add_graphic(pack, language, &graphic) != 0) r.bad = 1;
        }
    }
    if (r.bad || r.at != size || pack->glyph_count > GLYPHS_MAX) {
        snprintf(error, capacity, "%s is a damaged language pack: extract it again from its ROM", path);
        oracles_language_pack_free(pack);
        return NULL;
    }
    return pack;
}

OraclesLanguagePack *oracles_language_pack_read(const char *path, char *error, size_t capacity)
{
    size_t size = 0;
    uint8_t *data = oracles_rom_read_file(path, &size, error, capacity);
    if (!data) { snprintf(error, capacity, "cannot read the language pack %s", path); return NULL; }
    OraclesLanguagePack *pack = parse(data, size, path, error, capacity);
    free(data);
    return pack;
}
