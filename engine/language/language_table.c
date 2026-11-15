/* The tables of exceptions, config/language_exceptions_ages.tsv and config/language_exceptions_seasons.tsv, embedded
 * at configure: the US texts whose translation is not the European text of the same identifier;
 * config/language_exceptions_ages_ja.tsv and config/language_exceptions_seasons_ja.tsv: those whose Japanese is not the
 * Japanese text of their representative; and config/language_exceptions_ages_pl.tsv and
 * config/language_exceptions_seasons_pl.tsv: those whose Polish is not the Polish text of the same identifier.
 *
 * A line starting with # is a comment.  The first row is `rom`, a tab and the SHA-1 of the European, Japanese or Polish
 * ROM the table was measured on.  A row then: the texts (four hexadecimal digits each, TX_GGII, or a range first-last,
 * space-separated), the languages (en-eu, fr, de, it, es, comma-separated, or all; ja in a Japanese table, pl in a Polish
 * one), the source (eu:TX_GGII, jp:TX_GGII in a Japanese table, pl:TX_GGII in a Polish one, rule:NAME, own:KEY or
 * english) and why, all separated by tabs.  A row names its table's languages alone.  Identifiers and names only: the
 * table holds no text. */
#include "language_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const unsigned char oracles_language_exceptions_ages[];
extern const unsigned char oracles_language_exceptions_seasons[];
extern const unsigned char oracles_language_exceptions_ages_ja[];
extern const unsigned char oracles_language_exceptions_seasons_ja[];
extern const unsigned char oracles_language_exceptions_ages_pl[];
extern const unsigned char oracles_language_exceptions_seasons_pl[];

/* `all`: every language of the European ROM, its English included. */
#define ALL_LANGUAGES ((1u << ORACLES_LANGUAGE_EUROPEAN_END) - (1u << ORACLES_LANGUAGE_ENGLISH_EU))
#define JAPANESE (1u << ORACLES_LANGUAGE_JAPANESE)
#define POLISH (1u << ORACLES_LANGUAGE_POLISH)

typedef struct row {
    unsigned first, last;   /* the texts, a range */
    unsigned languages;     /* a bit per OraclesLanguage */
    language_exception exception;
} row;

struct language_table {
    char rom[41];
    language_edition edition;   /* of the ROM it pins: a Japanese or Polish table's rows name its language alone */
    row *rows;
    size_t count, capacity;
};

void language_table_free(language_table *table)
{
    if (!table) return;
    free(table->rows);
    free(table);
}

const char *language_table_rom(const language_table *table) { return table->rom; }

/* The field from *at to the next tab or the line's end, `end`; *at past its tab. */
static const char *field(const char **at, const char *end, size_t *length)
{
    const char *start = *at, *tab = memchr(start, '\t', (size_t)(end - start));
    *length = (size_t)((tab ? tab : end) - start);
    *at = tab ? tab + 1 : end;
    return start;
}

static int hex4(const char *at, const char *end, unsigned *value)
{
    if (end - at < 4) return -1;
    *value = 0;
    for (unsigned i = 0; i < 4u; i++) {
        const int c = tolower((unsigned char)at[i]);
        if (!isxdigit(c)) return -1;
        *value = *value << 4 | (unsigned)(isdigit(c) ? c - '0' : c - 'a' + 10);
    }
    return 0;
}

static int has(const char *text, size_t length, const char *word) { return length == strlen(word) && !memcmp(text, word, length); }

static int parse_languages(const char *text, size_t length, unsigned *languages)
{
    *languages = 0;
    if (has(text, length, "all")) { *languages = ALL_LANGUAGES; return 0; }
    for (const char *at = text, *end = text + length; at < end;) {
        const char *comma = memchr(at, ',', (size_t)(end - at));
        const size_t n = (size_t)((comma ? comma : end) - at);
        char code[8] = { 0 };
        OraclesLanguage language;
        if (!n || n >= sizeof code) return -1;
        memcpy(code, at, n);
        if (oracles_language_parse(code, &language) != 0 || language == ORACLES_LANGUAGE_ENGLISH) return -1;
        *languages |= 1u << language;
        at += n + 1u;
    }
    return *languages ? 0 : -1;
}

static int parse_source(const char *text, size_t length, language_edition edition, language_exception *e)
{
    memset(e, 0, sizeof *e);
    if (has(text, length, "english")) { e->source = ORACLES_LANGUAGE_SOURCE_KEPT; return 0; }
    if (length == 10u && !memcmp(text, edition == EDITION_JAPANESE ? "jp:TX_" : edition == EDITION_POLISH ? "pl:TX_" : "eu:TX_", 6)) { e->source = ORACLES_LANGUAGE_SOURCE_MOVED; return hex4(text + 6, text + length, &e->id); }
    if (length > 5u && !memcmp(text, "rule:", 5)) { e->source = ORACLES_LANGUAGE_SOURCE_ADAPTED; return language_rule_parse(text + 5, length - 5u, &e->rule); }
    if (length > 4u && length - 4u < sizeof e->key && !memcmp(text, "own:", 4)) {
        e->source = ORACLES_LANGUAGE_SOURCE_OWN;
        memcpy(e->key, text + 4, length - 4u);
        return 0;
    }
    return -1;
}

static int add(language_table *table, const row *r)
{
    for (size_t i = 0; i < table->count; i++) {
        const row *o = &table->rows[i];
        if ((o->languages & r->languages) && o->first <= r->last && r->first <= o->last) return -1;
    }
    if (table->count == table->capacity) {
        const size_t capacity = table->capacity ? 2u * table->capacity : 32u;
        row *rows = realloc(table->rows, capacity * sizeof *rows);
        if (!rows) return -1;
        table->rows = rows;
        table->capacity = capacity;
    }
    table->rows[table->count++] = *r;
    return 0;
}

/* One row's texts, each a row of the table; 0, or -1 with the reason. */
static int parse_row(language_table *table, const char *line, const char *end, unsigned number, char *error, size_t capacity)
{
    const char *at = line;
    size_t texts_length, languages_length, source_length, why_length;
    const char *texts = field(&at, end, &texts_length), *languages = field(&at, end, &languages_length);
    const char *source = field(&at, end, &source_length);
    field(&at, end, &why_length);
    row r;
    const unsigned own = table->edition == EDITION_JAPANESE ? JAPANESE : table->edition == EDITION_POLISH ? POLISH : 0u;
    if (parse_languages(languages, languages_length, &r.languages) != 0 || (own ? r.languages != own : (r.languages & ~ALL_LANGUAGES) != 0)
        || parse_source(source, source_length, table->edition, &r.exception) != 0 || !why_length) {
        snprintf(error, capacity, "the table's row %u is not one", number);
        return -1;
    }
    unsigned ids = 0;
    for (const char *t = texts, *t_end = texts + texts_length; t < t_end;) {
        if (*t == ' ') { t++; continue; }
        if (hex4(t, t_end, &r.first) != 0) { snprintf(error, capacity, "the table's row %u names a text that is not one", number); return -1; }
        r.last = r.first;
        t += 4;
        if (t < t_end && *t == '-') {
            if (hex4(t + 1, t_end, &r.last) != 0 || r.last < r.first) { snprintf(error, capacity, "the table's row %u has a range that is not one", number); return -1; }
            t += 5;
        }
        if (t < t_end && *t != ' ') { snprintf(error, capacity, "the table's row %u names a text that is not one", number); return -1; }
        if (add(table, &r) != 0) { snprintf(error, capacity, "the table's row %u names a text and a language a row before it names", number); return -1; }
        ids++;
    }
    if (!ids) { snprintf(error, capacity, "the table's row %u names no text", number); return -1; }
    return 0;
}

language_table *language_table_parse(const char *text, char *error, size_t capacity)
{
    language_table *table = calloc(1, sizeof *table);
    if (!table) { snprintf(error, capacity, "out of memory"); return NULL; }
    unsigned number = 0;
    for (const char *line = text; *line;) {
        const char *end = strchr(line, '\n');
        if (!end) end = line + strlen(line);
        const char *next = *end ? end + 1 : end;
        number++;
        if (end > line && end[-1] == '\r') end--;
        if (*line == '#' || end == line) { line = next; continue; }
        if (!table->rom[0]) {
            if (end - line != 44 || memcmp(line, "rom\t", 4)) { snprintf(error, capacity, "the table's first row does not pin its ROM"); language_table_free(table); return NULL; }
            memcpy(table->rom, line + 4, 40);
            OraclesRomInfo info = { 0 };
            table->edition = oracles_rom_known(table->rom, &info) == 0 ? language_edition_of(&info) : EDITION_EUROPEAN;
        } else if (parse_row(table, line, end, number, error, capacity) != 0) {
            language_table_free(table);
            return NULL;
        }
        line = next;
    }
    if (!table->rom[0]) { snprintf(error, capacity, "the table does not pin its ROM"); language_table_free(table); return NULL; }
    return table;
}

language_table *language_table_load(OraclesGame game, char *error, size_t capacity)
{
    if (game != ORACLES_GAME_AGES && game != ORACLES_GAME_SEASONS) { snprintf(error, capacity, "no table of exceptions for this game"); return NULL; }
    return language_table_parse((const char *)(game == ORACLES_GAME_AGES ? oracles_language_exceptions_ages : oracles_language_exceptions_seasons), error, capacity);
}

language_table *language_table_load_japanese(OraclesGame game, char *error, size_t capacity)
{
    if (game != ORACLES_GAME_AGES && game != ORACLES_GAME_SEASONS) { snprintf(error, capacity, "no table of exceptions for this game"); return NULL; }
    language_table *table = language_table_parse((const char *)(game == ORACLES_GAME_AGES ? oracles_language_exceptions_ages_ja : oracles_language_exceptions_seasons_ja),
                                                 error, capacity);
    if (table && table->edition != EDITION_JAPANESE) {
        snprintf(error, capacity, "the Japanese table of exceptions does not pin a Japanese ROM");
        language_table_free(table);
        return NULL;
    }
    return table;
}

language_table *language_table_load_polish(OraclesGame game, char *error, size_t capacity)
{
    if (game != ORACLES_GAME_AGES && game != ORACLES_GAME_SEASONS) { snprintf(error, capacity, "no table of exceptions for this game"); return NULL; }
    language_table *table = language_table_parse((const char *)(game == ORACLES_GAME_AGES ? oracles_language_exceptions_ages_pl : oracles_language_exceptions_seasons_pl),
                                                 error, capacity);
    if (table && table->edition != EDITION_POLISH) {
        snprintf(error, capacity, "the Polish table of exceptions does not pin a Polish ROM");
        language_table_free(table);
        return NULL;
    }
    return table;
}

int language_table_find(const language_table *table, OraclesLanguage language, unsigned id, language_exception *exception)
{
    for (size_t i = 0; i < table->count; i++) {
        const row *r = &table->rows[i];
        if ((r->languages >> language & 1u) && id >= r->first && id <= r->last) { *exception = r->exception; return 1; }
    }
    return 0;
}

void language_table_tally(const language_table *table, unsigned counts[ORACLES_LANGUAGE_SOURCE_COUNT])
{
    memset(counts, 0, ORACLES_LANGUAGE_SOURCE_COUNT * sizeof *counts);
    for (size_t i = 0; i < table->count; i++) {
        const row *r = &table->rows[i];
        unsigned languages = 0;
        for (unsigned l = ORACLES_LANGUAGE_ENGLISH_EU; l < ORACLES_LANGUAGE_COUNT; l++) languages += r->languages >> l & 1u;
        counts[r->exception.source] += languages * (r->last - r->first + 1u);
    }
}
