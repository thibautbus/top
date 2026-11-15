/* The mistakes of the official translations the port corrects, at extraction: config/language_corrections.tsv, a word
 * or a phrase replaced by another in the texts and languages a row names.  Both are written in config/language_own.tsv's
 * syntax (the font's letters in UTF-8, \n, \stop, \col(N), \\, \xNN), so that a phrase may run over a line break or a
 * colour, and a correction may break a line again where the corrected words no longer fit it.  The table is read once
 * per extraction, its words encoded then.
 *
 * config/language_corrections_pl.tsv corrects Kaxus's Polish the same way, its rows naming Polish alone, its words in
 * the Polish letters of the Polish ROM's font; a Polish text is corrected before it is checked, so that a correction
 * may mend a line the Polish ROM writes past the box. */
#include "language_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const unsigned char oracles_language_corrections[];
extern const unsigned char oracles_language_corrections_pl[];

#define CODE_DICTIONARY 0x02u        /* codes 2 to 15 take a parameter: 2 to 5 a fragment, which a decoded text has no more */
#define CODE_FIRST_CHARACTER 0x10u
#define RANGES 16u                   /* a row's texts: ranges first-last */
#define WORDS 128u                   /* a word or a phrase, encoded */

typedef struct correction_row {
    unsigned ranges, first[RANGES], last[RANGES];
    unsigned languages;              /* a bit per OraclesLanguage */
    uint8_t from[WORDS], to[WORDS];
    size_t from_length, to_length;   /* without their terminators */
} correction_row;

struct language_corrections {
    correction_row *rows;            /* the game's, in the table's order */
    unsigned count;
    unsigned *applied;               /* per row: the (text, language) it replaced words in */
    unsigned refused;                /* the corrected texts that did not hold, served as they were */
};

static int field(const char **at, const char *end, const char **start, size_t *length)
{
    *start = *at;
    const char *tab = memchr(*at, '\t', (size_t)(end - *at));
    *length = (size_t)((tab ? tab : end) - *at);
    *at = tab ? tab + 1 : end;
    return *length ? 0 : -1;
}

static int parse_texts(const char *texts, size_t length, correction_row *row)
{
    char copy[256];
    if (length >= sizeof copy) return -1;
    memcpy(copy, texts, length);
    copy[length] = 0;
    row->ranges = 0;
    for (const char *at = copy; *at;) {
        unsigned first = 0, last = 0;
        int used = 0;
        if (sscanf(at, "%4x-%4x%n", &first, &last, &used) == 2 && used == 9) {}
        else if (sscanf(at, "%4x%n", &first, &used) == 1 && used == 4) last = first;
        else return -1;
        if (row->ranges == RANGES || last < first) return -1;
        row->first[row->ranges] = first;
        row->last[row->ranges++] = last;
        at += used;
        if (*at && *at != ' ') return -1;
        while (*at == ' ') at++;
    }
    return row->ranges ? 0 : -1;
}

/* The languages a row names: European ones alone, a Japanese pack being made without corrections (a row naming ja
 * would change nothing); Polish alone in the Polish table (`polish`). */
static int parse_languages(const char *languages, size_t length, int polish, unsigned *mask)
{
    *mask = 0;
    if (polish) {
        if (length != 2 || memcmp(languages, "pl", 2)) return -1;
        *mask = 1u << ORACLES_LANGUAGE_POLISH;
        return 0;
    }
    if (length == 3 && !memcmp(languages, "all", 3)) {
        for (unsigned l = ORACLES_LANGUAGE_ENGLISH_EU; l < ORACLES_LANGUAGE_EUROPEAN_END; l++) *mask |= 1u << l;
        return 0;
    }
    for (const char *at = languages, *end = languages + length; at < end;) {
        const char *comma = memchr(at, ',', (size_t)(end - at));
        const size_t n = (size_t)((comma ? comma : end) - at);
        char code[8] = { 0 };
        OraclesLanguage language;
        if (!n || n >= sizeof code) return -1;
        memcpy(code, at, n);
        if (oracles_language_parse(code, &language) != 0 || language == ORACLES_LANGUAGE_ENGLISH || language >= ORACLES_LANGUAGE_EUROPEAN_END) return -1;
        *mask |= 1u << language;
        at += n + 1u;
    }
    return *mask ? 0 : -1;
}

void language_corrections_free(language_corrections *corrections)
{
    if (!corrections) return;
    free(corrections->rows);
    free(corrections->applied);
    free(corrections);
}

language_corrections *language_corrections_parse(const char *text, int polish, OraclesGame game, char *error, size_t capacity)
{
    const char *name = game == ORACLES_GAME_AGES ? "ages" : game == ORACLES_GAME_SEASONS ? "seasons" : "";
    const OraclesLanguage encoding = polish ? ORACLES_LANGUAGE_POLISH : ORACLES_LANGUAGE_ENGLISH_EU;
    language_corrections *c = calloc(1, sizeof *c);
    unsigned lines = 1, number = 0;
    for (const char *t = text; *t; t++) lines += *t == '\n';
    if (!c || !(c->rows = calloc(lines, sizeof *c->rows)) || !(c->applied = calloc(lines, sizeof *c->applied))) {
        snprintf(error, capacity, "out of memory");
        language_corrections_free(c);
        return NULL;
    }
    for (const char *line = text; *line;) {
        const char *end = strchr(line, '\n');
        if (!end) end = line + strlen(line);
        const char *next = *end ? end + 1 : end;
        number++;
        if (end > line && end[-1] == '\r') end--;
        if (*line != '#' && end > line) {
            const char *at = line, *g, *t, *l, *from, *to;
            size_t gl, tl, ll, fl, ol, from_length = 0, to_length = 0;
            correction_row *row = &c->rows[c->count];
            if (field(&at, end, &g, &gl) != 0 || field(&at, end, &t, &tl) != 0 || field(&at, end, &l, &ll) != 0 || field(&at, end, &from, &fl) != 0
                || field(&at, end, &to, &ol) != 0 || parse_texts(t, tl, row) != 0 || parse_languages(l, ll, polish, &row->languages) != 0
                || language_text_encode_in(encoding, from, fl, row->from, sizeof row->from, &from_length) != 0 || from_length < 2u
                || language_text_encode_in(encoding, to, ol, row->to, sizeof row->to, &to_length) != 0) {
                snprintf(error, capacity, "the %scorrections' row %u is not one", polish ? "Polish " : "", number);
                language_corrections_free(c);
                return NULL;
            }
            if (gl == strlen(name) && !memcmp(g, name, gl)) {
                row->from_length = from_length - 1u;
                row->to_length = to_length - 1u;
                c->count++;
            }
        }
        line = next;
    }
    return c;
}

language_corrections *language_corrections_load(OraclesGame game, char *error, size_t capacity)
{
    return language_corrections_parse((const char *)oracles_language_corrections, 0, game, error, capacity);
}

language_corrections *language_corrections_load_polish(OraclesGame game, char *error, size_t capacity)
{
    return language_corrections_parse((const char *)oracles_language_corrections_pl, 1, game, error, capacity);
}

unsigned language_corrections_count(const language_corrections *corrections) { return corrections->count; }

unsigned language_corrections_named(const language_corrections *corrections, unsigned n)
{
    const correction_row *row = &corrections->rows[n];
    unsigned texts = 0, languages = 0;
    for (unsigned r = 0; r < row->ranges; r++) texts += row->last[r] - row->first[r] + 1u;
    for (unsigned l = 0; l < ORACLES_LANGUAGE_COUNT; l++) languages += row->languages >> l & 1u;
    return texts * languages;
}

unsigned language_corrections_applied(const language_corrections *corrections, unsigned n) { return corrections->applied[n]; }
unsigned language_corrections_refused(const language_corrections *corrections) { return corrections->refused; }
void language_corrections_refuse(language_corrections *corrections) { corrections->refused++; }

static int is_letter(uint8_t c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c >= 0x80u; }

static int is_code(uint8_t c) { return c >= CODE_DICTIONARY && c < CODE_FIRST_CHARACTER; }

/* Whether the last unit of `bytes` (a character, or a control code and its parameter) is a letter. */
static int ends_in_letter(const uint8_t *bytes, size_t length)
{
    size_t last = 0;
    for (size_t at = 0; at < length; at += is_code(bytes[at]) ? 2u : 1u) last = at;
    return length && !is_code(bytes[last]) && is_letter(bytes[last]);
}

/* `from` replaced by `to` in text, wherever it starts on a character: a whole word where it starts or ends with a
 * letter (no letter before it, no letter after it), never inside a control code's parameter. */
static int replace(uint8_t *text, size_t *length, size_t capacity, const uint8_t *from, size_t from_length, const uint8_t *to, size_t to_length)
{
    const int word_start = !is_code(from[0]) && is_letter(from[0]), word_end = ends_in_letter(from, from_length);
    int replaced = 0, after_letter = 0;
    for (size_t at = 0; at < *length;) {
        if (is_code(text[at])) { at += 2; after_letter = 0; continue; }
        const size_t end = at + from_length;
        const int starts = end <= *length && !memcmp(text + at, from, from_length) && !(word_start && after_letter)
                           && !(word_end && end < *length && is_letter(text[end]));
        if (!starts) { after_letter = is_letter(text[at]); at++; continue; }
        if (*length - from_length + to_length > capacity) return -1;
        memmove(text + at + to_length, text + end, *length - end);
        memcpy(text + at, to, to_length);
        *length = *length - from_length + to_length;
        at += to_length;
        after_letter = ends_in_letter(to, to_length);
        replaced = 1;
    }
    return replaced;
}

int language_correct(language_corrections *corrections, OraclesLanguage language, unsigned id, uint8_t *text, size_t *length, size_t capacity)
{
    int corrected = 0;
    for (unsigned n = 0; n < corrections->count; n++) {
        const correction_row *row = &corrections->rows[n];
        int listed = 0;
        for (unsigned r = 0; r < row->ranges && !listed; r++) listed = id >= row->first[r] && id <= row->last[r];
        if (!listed || !(row->languages >> language & 1u)) continue;
        const int replaced = replace(text, length, capacity, row->from, row->from_length, row->to, row->to_length);
        if (replaced < 0) return -1;
        corrections->applied[n] += (unsigned)replaced;
        corrected |= replaced;
    }
    return corrected;
}
