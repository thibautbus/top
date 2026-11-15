/* The texts laid out again for the compact text: each page filled to a line's pixels in the font's own widths.
 *
 * The game breaks no line: its texts carry their breaks, made for sixteen characters of eight pixels (ROM_DATA_FORMATS,
 * section 8).  The compact text draws a line in the font's widths (oracles_text_place), so that a line of 128 pixels
 * holds more; this layout makes its pages of two lines hold it.  A text is cut into pages where it opens a new box
 * (\stop, \heartpiece): each page is laid out alone, its line breaks becoming spaces and its words put back on its
 * lines from the first, as many as a line holds.  A word broken at a line's end by a hyphen before a small letter
 * ("Vergan-" / "genheit") is joined again, the hyphen dropped, its break kept as a place a line may end at, hyphen
 * shown, in German and Italian, which break their words so; in English, French and Spanish, whose hyphens at a line's
 * end are their compounds' ("lava-" / "like", "grand-" / "chose"), only a word the language writes whole elsewhere
 * ("some-" / "thing"), or neither of whose parts it writes alone ("appara-" / "tront"), or one of the few closed compounds
 * of English the games break so ("after-" / "life").  A hyphen before a capital, or one the language writes inside a line in the same word
 * (language_wrap_learn), is a word's own, kept.  A line is measured as the transaction draws it: a name (\Link, \Child) as five of the widest
 * letters a name may hold, a number (\num1, \num2) as three of the widest digits, a symbol on its whole tile, the colours
 * as \col sets them, from either attribute a box opens with ($80 or $81, TEXTBOXFLAG_ALTPALETTE1), a tile holding one.
 *
 * A page is left as it is when it lays out a choice (\opt) or a secret, calls another text (\call), shows the piece of
 * heart, or aligns its lines with spaces (a line that starts with a space, two spaces in a row, an empty line); a text
 * is left whole as it is when it shows a single box, and from its first \jump or cmd8 on, what follows being another
 * text's.  A text laid out again must give the same commands as before (oracles_text_same_commands), the same characters
 * but its spaces, breaks and the hyphens of the words it joined, every line within the width and no break before its end nor empty line the
 * layout made (a page kept as it is keeps its own); one that does not keeps its layout. */
#include "language_wrap_internal.h"
#include "sha1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CODE_END 0x00u
#define CODE_DICTIONARY_FIRST 0x02u
#define CODE_SYMBOL 0x06u
#define CODE_JUMP 0x07u
#define CODE_END_HERE 0x08u
#define CODE_COLOUR 0x09u
#define CODE_NAME 0x0au
#define CODE_COMMAND 0x0cu
#define CODE_CALL 0x0fu
#define COMMAND_NUMBER 1u
#define COMMAND_OPTION 2u
#define COMMAND_STOP 3u
#define COMMAND_HEART_PIECE 5u
#define COMMAND_NUMBER_2 6u
#define SPACE 0x20u
#define NAME_LETTERS 5u
#define NUMBER_DIGITS 3u
#define COLOURS 5u
#define WORDS 512u
/* What a layout's choices cost, against a line's empty pixels squared (lay_out). */
#define COST_COLOUR 4096u   /* a break within a colour: a name, an item */
#define COST_HYPHEN 2500u   /* a word broken at a hyphen */
#define COST_PHRASE 900u    /* a break that does not follow a punctuation mark */
#define COST_LINE 1600u     /* a line more than the fewest, which the page's last box holds */

/* The attribute of \col's colours (textbox.s, @textColorData): white and red in palette 0, red behind the sprites, blue
 * and white in palette 1.  A parameter with bit 7 is the attribute itself. */
static const uint8_t colour_attribute[COLOURS] = { 0x80, 0x80, 0x81, 0x81, 0x81 };
/* The attribute a box opens with: $80, or $81 with TEXTBOXFLAG_ALTPALETTE1 (initTextboxStuff). */
static const uint8_t opening_attribute[2] = { 0x80, 0x81 };

/* English compounds the games write closed but break at a line's end with a hyphen, both of whose parts they write
 * alone and none of whose texts writes them whole (Kinomi's "after-life", "under-water", Ages' and Moonrise's
 * "head-stone"): written whole, so that the layout joins them. */
static const char *const closed_compounds[] = { "afterlife", "headstone", "underwater" };

language_wrap *language_wrap_new(const OraclesTextFont *font, unsigned width, OraclesLanguage language)
{
    if (!width || width > ORACLES_TEXT_LINE_TILES * ORACLES_TEXT_CELL_WIDTH) return NULL;
    language_wrap *wrap = calloc(1, sizeof *wrap);
    if (!wrap) return NULL;
    wrap->font = *font;
    wrap->width = width;
    wrap->syllables = language == ORACLES_LANGUAGE_GERMAN || language == ORACLES_LANGUAGE_ITALIAN;
    wrap->japanese = language == ORACLES_LANGUAGE_JAPANESE;
    wrap->polish = language == ORACLES_LANGUAGE_POLISH;
    wrap->name_letter = wrap->digit = '0';
    /* A name is written on the US name screen: ASCII. */
    for (unsigned c = SPACE + 1u; c < 0x7fu; c++)
        if (font->advance[c] > font->advance[wrap->name_letter] || (font->advance[c] == font->advance[wrap->name_letter] && font->width[c] > font->width[wrap->name_letter]))
            wrap->name_letter = (uint8_t)c;
    for (unsigned c = '1'; c <= '9'; c++)
        if (font->advance[c] > font->advance[wrap->digit]) wrap->digit = (uint8_t)c;
    if (language == ORACLES_LANGUAGE_ENGLISH || language == ORACLES_LANGUAGE_ENGLISH_EU)
        for (size_t i = 0; i < sizeof closed_compounds / sizeof closed_compounds[0]; i++)
            language_wrap_write_whole(wrap, (const uint8_t *)closed_compounds[i], strlen(closed_compounds[i]));
    return wrap;
}

void language_wrap_free(language_wrap *wrap)
{
    if (!wrap) return;
    free(wrap->words);
    free(wrap->whole);
    free(wrap);
}

/* ---- measuring a line ------------------------------------------------------------------------------------------- */

/* Whether `bytes` change the colour (`symbols`: or print a symbol). */
static int has_colour(const uint8_t *bytes, size_t length, int symbols)
{
    for (size_t at = 0; at < length; at += code_bytes(bytes[at]))
        if ((bytes[at] == CODE_COLOUR && at + 1u < length) || (symbols && bytes[at] == CODE_SYMBOL)) return 1;
    return 0;
}

/* A line measured as it grows, from each attribute a box may open with: its pens, its attributes, its characters
 * placed (as many from either opening), whether each fits the width. */
typedef struct measure {
    OraclesTextPen pen[2];
    uint8_t attribute[2];
    unsigned openings;       /* 1 when both would give the same */
    unsigned count;
    unsigned width;          /* the line's */
    unsigned start, reach;   /* the furthest a character starts and reaches, from the first opening */
    int fits;
} measure;

static void measure_start(const language_wrap *wrap, measure *m, const uint8_t attributes[2], unsigned openings)
{
    memset(m, 0, sizeof *m);
    m->width = wrap->width;
    m->attribute[0] = attributes[0];
    m->attribute[1] = attributes[1];
    m->openings = openings;
    m->fits = 1;
}

/* `bytes` placed after what the line holds. */
static void measure_bytes(const language_wrap *wrap, measure *m, const uint8_t *bytes, size_t length)
{
    for (unsigned opening = 0; opening < m->openings; opening++) {
        OraclesTextPen *pen = &m->pen[opening];
        uint8_t attribute = m->attribute[opening];
        unsigned count = 0;
        for (size_t at = 0; at < length; at += code_bytes(bytes[at])) {
            const uint8_t c = bytes[at], parameter = at + 1u < length ? bytes[at + 1u] : 0;
            uint8_t glyph = c;
            unsigned repeat = 1, whole = 0;
            if (c < ORACLES_TEXT_FIRST_CHARACTER) {
                if (c == CODE_COLOUR) { attribute = parameter & 0x80u ? parameter : parameter < COLOURS ? colour_attribute[parameter] : attribute; continue; }
                if (c == CODE_NAME && parameter < 2u) { glyph = wrap->name_letter; repeat = NAME_LETTERS; }
                else if (c == CODE_COMMAND && (parameter >> 3 == COMMAND_NUMBER || parameter >> 3 == COMMAND_NUMBER_2)) { glyph = wrap->digit; repeat = NUMBER_DIGITS; }
                else if (c == CODE_SYMBOL) whole = 1;
                else continue;
            }
            for (unsigned r = 0; r < repeat; r++) {
                unsigned start = 0, from = 0, drawn = 0;
                oracles_text_place(pen, &wrap->font, glyph, attribute, (int)whole, &start, &from, &drawn);
                count++;
                if (start >= m->width || start + drawn > m->width) m->fits = 0;
                if (!opening) {
                    if (start > m->start) m->start = start;
                    if (start + drawn > m->reach) m->reach = start + drawn;
                }
            }
        }
        m->attribute[opening] = attribute;
        if (!opening) m->count += count;
    }
}

static int measure_fits(const measure *m) { return m->fits && m->count <= ORACLES_TEXT_LINE_CHARACTERS_MAX; }
/* The pixels its ink takes (a pen unused is zero). */
static unsigned measure_width(const measure *m) { return m->pen[0].end > m->pen[1].end ? m->pen[0].end : m->pen[1].end; }

/* Whether `bytes` fit a line from the attributes `attributes` (both openings, one when they open with the same attribute
 * or no colour follows), which become the line's last, at most ORACLES_TEXT_LINE_CHARACTERS_MAX characters; the pixels
 * its ink takes in *width. */
static int line_fits(const language_wrap *wrap, const uint8_t *bytes, size_t length, uint8_t attributes[2], unsigned *width)
{
    measure m;
    const int same = attributes[0] == attributes[1];
    measure_start(wrap, &m, attributes, !same && has_colour(bytes, length, 0) ? 2u : 1u);
    measure_bytes(wrap, &m, bytes, length);
    attributes[0] = m.attribute[0];
    attributes[1] = m.attribute[same ? 0 : 1];
    if (width) *width = measure_width(&m);
    return measure_fits(&m);
}

/* The attributes after `bytes`, whatever they print. */
static void follow_colours(const uint8_t *bytes, size_t length, uint8_t attributes[2])
{
    for (size_t at = 0; at + 1u < length; at += code_bytes(bytes[at]))
        if (bytes[at] == CODE_COLOUR)
            for (unsigned opening = 0; opening < 2u; opening++) {
                const uint8_t parameter = bytes[at + 1u];
                attributes[opening] = parameter & 0x80u ? parameter : parameter < COLOURS ? colour_attribute[parameter] : attributes[opening];
            }
}

/* A word placed alone from a pen at 0 after a glyph joined to its cell's edge or not, in one attribute: where it leaves
 * the pen, the end of its last ink (0 for none), the furthest a character of it starts and reaches, its characters and
 * whether its last is joined.  Without a colour or a symbol, oracles_text_place moves no character to a tile's edge:
 * the word placed at pen p is its shape moved by p. */
typedef struct shape {
    unsigned advance, end, start, reach, count;
    int joined;
} shape;

static void shape_of(const language_wrap *wrap, const uint8_t *bytes, size_t length, int joined, shape *out)
{
    measure m;
    const uint8_t attributes[2] = { opening_attribute[0], opening_attribute[0] };
    measure_start(wrap, &m, attributes, 1u);
    m.pen[0].joined = joined;
    measure_bytes(wrap, &m, bytes, length);
    *out = (shape){ m.pen[0].pen, m.pen[0].end, m.start, m.reach, m.count, m.pen[0].joined };
}

/* The same line while it places words by their shapes: its pen, the end of its ink, its characters, whether its last
 * glyph is joined and the tile its pen is in holds ink (of the attribute it prints in), whether all fit.  Of the tiles
 * before the pen, oracles_text_place reads none again. */
typedef struct run {
    unsigned pen, end, count;
    int joined, inked, fits;
} run;

/* The word of shape `s` placed after the line. */
static void run_place(run *r, const shape *s, unsigned width)
{
    const unsigned tile = r->pen / ORACLES_TEXT_CELL_WIDTH;
    if (s->count && (r->pen + s->start >= width || r->pen + s->reach > width)) r->fits = 0;
    if (s->count) r->joined = s->joined;
    r->pen += s->advance;
    r->count += s->count;
    if (s->end) {
        r->end = r->pen - s->advance + s->end;
        r->inked = r->end > r->pen / ORACLES_TEXT_CELL_WIDTH * ORACLES_TEXT_CELL_WIDTH;
    } else if (r->pen / ORACLES_TEXT_CELL_WIDTH != tile) r->inked = 0;
}

/* The line placing by shapes again: when every opening's pen is at the same place, and the tile it is in holds no ink
 * of another attribute than the one it prints in.  0 otherwise. */
static int run_from(const measure *m, run *r)
{
    const OraclesTextPen *pen = &m->pen[0];
    const unsigned tile = pen->pen / ORACLES_TEXT_CELL_WIDTH;
    const int inked = tile < ORACLES_TEXT_LINE_TILES && pen->inked[tile];
    for (unsigned o = 0; o < m->openings; o++) {
        const OraclesTextPen *other = &m->pen[o];
        if (other->pen != pen->pen || other->end != pen->end || other->joined != pen->joined || (tile < ORACLES_TEXT_LINE_TILES && other->inked[tile] != inked)
            || (inked && other->attribute[tile] != m->attribute[o])) return 0;
    }
    *r = (run){ pen->pen, pen->end, m->count, pen->joined, inked, m->fits };
    return 1;
}

/* The line measured character by character again, from where its words' shapes left it. */
static void run_to(const run *r, measure *m)
{
    const unsigned tile = r->pen / ORACLES_TEXT_CELL_WIDTH;
    for (unsigned o = 0; o < m->openings; o++) {
        OraclesTextPen *pen = &m->pen[o];
        memset(pen, 0, sizeof *pen);
        pen->pen = r->pen;
        pen->end = r->end;
        pen->joined = r->joined;
        if (r->inked && tile < ORACLES_TEXT_LINE_TILES) {
            pen->inked[tile] = pen->marked[tile] = 1;
            pen->attribute[tile] = m->attribute[o];
        }
    }
    m->count = r->count;
    m->fits = r->fits;
}

/* ---- a page ----------------------------------------------------------------------------------------------------- */

typedef enum gap { GAP_SPACE, GAP_SOFT, GAP_HARD } gap;

typedef struct page {
    size_t from[WORDS], to[WORDS];   /* a word's bytes in the text */
    gap after[WORDS];                /* what lies between it and the next */
    size_t space_from[WORDS], space_to[WORDS];   /* the spaces between them, within a line (GAP_SPACE without a break) */
    unsigned words;
} page;

/* The first character `text` prints from `at`, past its commands that print nothing; 0 when none. */
static uint8_t first_printed(const uint8_t *text, size_t at, size_t end)
{
    while (at < end && text[at] < ORACLES_TEXT_FIRST_CHARACTER && text[at] > CODE_NEWLINE && text[at] != CODE_NAME && text[at] != CODE_SYMBOL
           && !(text[at] == CODE_COMMAND && at + 1u < end && (text[at + 1u] >> 3 == COMMAND_NUMBER || text[at + 1u] >> 3 == COMMAND_NUMBER_2)))
        at += 2u;
    return at < end && text[at] >= ORACLES_TEXT_FIRST_CHARACTER ? text[at] : 0;
}

/* The page `text`[from, to) as words and what lies between them; 0, or -1 when its lines are aligned with spaces or it
 * has more words than a page holds. */
static int words_of(const language_wrap *wrap, const uint8_t *text, size_t from, size_t to, page *p)
{
    p->words = 0;
    size_t at = from;
    int line_start = 1;
    while (at < to) {
        if (text[at] == SPACE) {
            if (line_start || (at + 1u < to && text[at + 1u] == SPACE)) return -1;
            at++;
            continue;
        }
        if (text[at] == CODE_NEWLINE) {
            if (line_start) return -1;   /* an empty line */
            line_start = 1;
            at++;
            continue;
        }
        if (p->words == WORDS) return -1;
        const unsigned w = p->words++;
        p->from[w] = at;
        /* The word's characters: the run of letters it ends with, and whether its last is a hyphen after one. */
        size_t run = at, letters = at;
        int in_run = 0, hyphen = 0, dash = 0;
        uint8_t previous = 0;
        while (at < to && text[at] != SPACE && text[at] != CODE_NEWLINE) {
            const uint8_t c = text[at];
            hyphen = 0;
            dash = !wrap->japanese && c == HYPHEN && previous == HYPHEN;
            if (is_letter(c)) {
                if (!in_run) run = at;
                in_run = 1;
            } else {
                hyphen = !wrap->japanese && c == HYPHEN && in_run;
                if (hyphen) letters = run;
                in_run = 0;
            }
            if (c >= ORACLES_TEXT_FIRST_CHARACTER) previous = c;
            at += code_bytes(c);
        }
        if (at > to) return -1;
        p->to[w] = at;
        p->after[w] = GAP_SPACE;
        p->space_from[w] = p->space_to[w] = at;
        line_start = 0;
        /* What follows: spaces kept within the line, or the line's end. */
        size_t gap_end = at;
        while (gap_end < to && text[gap_end] == SPACE) gap_end++;
        if (gap_end - at > 1u) return -1;   /* two spaces in a row: an alignment */
        if (gap_end < to && text[gap_end] == CODE_NEWLINE) {
            const uint8_t next = first_printed(text, gap_end + 1u, to);
            if (gap_end == at && hyphen && next && (is_letter(next) || (next >= '0' && next <= '9'))) {
                /* A hyphen at the line's end, inside a word: the word goes on at the next line, joined as one the
                 * language writes with its hyphen (the word, or its part after the hyphen).  Else, when a small letter
                 * follows, without it in a language that breaks its words by syllables; in the others, whose hyphens at
                 * a line's end are their compounds' ("lava-like", "grand-chose"), when it writes the word whole
                 * elsewhere ("some-thing"), or neither of its parts alone ("appara-tront"). */
                uint8_t joined[LEARNED_BYTES];
                size_t n = 0, right = gap_end + 1u;
                for (size_t i = letters; i < p->to[w] && n < LEARNED_BYTES; i++) joined[n++] = text[i];
                while (right < to && text[right] < ORACLES_TEXT_FIRST_CHARACTER && text[right] > CODE_NEWLINE) right += 2u;
                const size_t tail = n;
                for (size_t i = right; i < to && is_letter(text[i]) && n < LEARNED_BYTES; i++) joined[n++] = text[i];
                const int own = language_wrap_learned(wrap, joined, n) || language_wrap_learned_tail(wrap, joined + tail, n - tail);
                const int broken = wrap->syllables || (tail && language_wrap_written_whole(wrap, joined, tail - 1u, joined + tail, n - tail))
                    || (tail && n > tail && !language_wrap_written_whole(wrap, joined, tail - 1u, NULL, 0) && !language_wrap_written_whole(wrap, joined + tail, n - tail, NULL, 0));
                p->after[w] = is_small_in(wrap, next) && !own && broken ? GAP_SOFT : GAP_HARD;
            } else if (gap_end == at && dash) p->after[w] = GAP_HARD;   /* a dash of two hyphens: the line goes on without a space */
            line_start = 1;
            at = gap_end + 1u;
        } else {
            p->space_to[w] = gap_end;
            at = gap_end;
        }
    }
    return 0;
}

/* Appends `count` bytes to out. */
static int put(uint8_t *out, size_t capacity, size_t *used, const uint8_t *bytes, size_t count)
{
    if (*used + count > capacity) return -1;
    memcpy(out + *used, bytes, count);
    *used += count;
    return 0;
}

/* Words first to last - 1 of a page as one line: the spaces between them, a joined hyphen dropped, the last one's kept
 * (marked in `soft`, beside out). */
static int line_bytes(const uint8_t *text, const page *p, unsigned first, unsigned last, uint8_t *out, uint8_t *soft, size_t capacity, size_t *used)
{
    for (unsigned w = first; w < last; w++) {
        const int inside = w + 1u < last;
        const size_t end = inside && p->after[w] == GAP_SOFT ? p->to[w] - 1u : p->to[w];
        if (put(out, capacity, used, text + p->from[w], end - p->from[w]) != 0) return -1;
        if (!inside && p->after[w] == GAP_SOFT) soft[*used - 1u] = 1;
        if (!inside || p->after[w] != GAP_SPACE) continue;
        if (p->space_to[w] > p->space_from[w]) { if (put(out, capacity, used, text + p->space_from[w], p->space_to[w] - p->space_from[w]) != 0) return -1; }
        else {
            const uint8_t space = SPACE;
            if (put(out, capacity, used, &space, 1) != 0) return -1;
        }
    }
    return 0;
}

/* Whether a word ends a phrase: its last character a punctuation mark. */
static int ends_phrase(const uint8_t *text, size_t from, size_t to)
{
    uint8_t last = 0;
    for (size_t at = from; at < to; at += code_bytes(text[at]))
        if (text[at] >= ORACLES_TEXT_FIRST_CHARACTER) last = text[at];
    return last && strchr(".!?,;:", last) != NULL;
}

/* The page laid out, appended to out; its attributes followed.  Its lines are as few as the width allows, or one more
 * when that fills its last box, and among those the breaks cost least: a line's empty pixels squared, a break within a
 * colour (a name, an item), at a hyphen joined or not after a punctuation mark costing more.  0, or -1 when a word does
 * not fit a line alone or out of memory. */
static int lay_out(const language_wrap *wrap, const uint8_t *text, const page *p, uint8_t attributes[2], uint8_t *out, uint8_t *soft, size_t capacity, size_t *used)
{
    const unsigned n = p->words;
    if (!n) return 0;
    uint8_t (*before)[2] = malloc((n + 1u) * sizeof *before), *coloured = malloc(n), *plain = malloc(n);
    shape (*shapes)[4] = malloc(n * sizeof *shapes);   /* a word's whole and inside a line, after a glyph not joined and joined */
    unsigned *extent = malloc(n * sizeof *extent), *width = malloc((size_t)n * ORACLES_TEXT_LINE_CHARACTERS_MAX * sizeof *width), *breaks = malloc((n + 1u) * sizeof *breaks);
    unsigned long *best = NULL;
    unsigned *choice = NULL;
    int status = -1;
    if (!before || !coloured || !plain || !shapes || !extent || !width || !breaks) goto done;
    /* The attributes each word starts with, the colour after it, and what a break after it costs. */
    uint8_t colour = 0;
    before[0][0] = attributes[0];
    before[0][1] = attributes[1];
    for (unsigned w = 0; w < n; w++) {
        before[w + 1u][0] = before[w][0];
        before[w + 1u][1] = before[w][1];
        follow_colours(text + p->from[w], p->to[w] - p->from[w], before[w + 1u]);
        for (size_t at = p->from[w]; at + 1u < p->to[w]; at += code_bytes(text[at]))
            if (text[at] == CODE_COLOUR) colour = text[at + 1u];
        breaks[w + 1u] = (colour ? COST_COLOUR : 0u) + (p->after[w] != GAP_SPACE ? COST_HYPHEN : 0u) + (ends_phrase(text, p->from[w], p->to[w]) ? 0u : COST_PHRASE);
    }
    /* The lines that fit: from word i, extent[i] words at most, width[i][k] the pixels of k + 1 of them, measured as
     * line_bytes writes them, a word at a time: the line before the last word as it stands inside a line (its joined
     * hyphen dropped, the space after it), the last one whole.  A word of no colour nor symbol is placed by its shape
     * (after a glyph not joined, and joined when no space comes before it), the others character by character, from
     * both openings when they open with other attributes and a colour follows. */
    for (unsigned i = n; i-- > 0;) {
        const uint8_t *word = text + p->from[i];
        const size_t length = p->to[i] - p->from[i];
        coloured[i] = (uint8_t)((i + 1u < n && coloured[i + 1u]) || has_colour(word, length, 0));
        plain[i] = (uint8_t)!has_colour(word, length, 1);
        for (unsigned joined = 0; plain[i] && joined < (i && p->after[i - 1u] != GAP_SPACE ? 2u : 1u); joined++) {
            shape_of(wrap, word, length, (int)joined, &shapes[i][joined]);
            if (p->after[i] == GAP_SOFT) shape_of(wrap, word, length - 1u, (int)joined, &shapes[i][2u + joined]);
        }
    }
    const uint8_t blank = SPACE;
    shape space[2];
    shape_of(wrap, &blank, 1, 0, &space[0]);
    shape_of(wrap, &blank, 1, 1, &space[1]);
    for (unsigned i = 0; i < n; i++) {
        extent[i] = 0;
        measure line;
        measure_start(wrap, &line, before[i], before[i][0] != before[i][1] && coloured[i] ? 2u : 1u);
        run shaped = { 0, 0, 0, 0, 0, 1 };
        int by_shapes = 1;
        for (unsigned j = i + 1u; j <= n && j - i <= ORACLES_TEXT_LINE_CHARACTERS_MAX; j++) {
            const unsigned w = j - 1u;
            unsigned pixels = 0;
            if (by_shapes && !plain[w]) { run_to(&shaped, &line); by_shapes = 0; }
            else if (!by_shapes && plain[w]) by_shapes = run_from(&line, &shaped);
            if (by_shapes) {
                run trial = shaped;
                run_place(&trial, &shapes[w][trial.joined != 0], wrap->width);
                if (!trial.fits || trial.count > ORACLES_TEXT_LINE_CHARACTERS_MAX) break;
                pixels = trial.end;
                if (p->after[w] == GAP_SOFT) run_place(&shaped, &shapes[w][2u + (shaped.joined != 0)], wrap->width);
                else shaped = trial;
                if (p->after[w] == GAP_SPACE) run_place(&shaped, &space[shaped.joined != 0], wrap->width);
            } else {
                measure whole = line;
                measure_bytes(wrap, &whole, text + p->from[w], p->to[w] - p->from[w]);
                if (!measure_fits(&whole)) break;
                pixels = measure_width(&whole);
                if (p->after[w] == GAP_SOFT) measure_bytes(wrap, &line, text + p->from[w], p->to[w] - p->from[w] - 1u);
                else line = whole;
                if (p->after[w] == GAP_SPACE) measure_bytes(wrap, &line, &blank, 1);
            }
            width[i * ORACLES_TEXT_LINE_CHARACTERS_MAX + (j - i - 1u)] = pixels;
            extent[i] = j - i;
        }
        if (!extent[i]) goto done;
    }
    /* The fewest lines, as the longest lines give them. */
    unsigned fewest = 0;
    for (unsigned i = 0; i < n; i += extent[i]) fewest++;
    const unsigned most = (fewest + 1u) / 2u * 2u;
    best = malloc((size_t)(most + 1u) * (n + 1u) * sizeof *best);
    choice = malloc((size_t)(most + 1u) * (n + 1u) * sizeof *choice);
    if (!best || !choice) goto done;
    for (size_t i = 0; i < (size_t)(most + 1u) * (n + 1u); i++) best[i] = ~0ul;
    best[0] = 0;
    for (unsigned k = 1; k <= most; k++)
        for (unsigned i = 0; i < n; i++) {
            const unsigned long from = best[(k - 1u) * (n + 1u) + i];
            if (from == ~0ul) continue;
            for (unsigned j = i + 1u; j <= i + extent[i]; j++) {
                const unsigned pixels = width[i * ORACLES_TEXT_LINE_CHARACTERS_MAX + (j - i - 1u)];
                const unsigned long slack = wrap->width > pixels ? wrap->width - pixels : 0u;
                const unsigned long cost = from + (j == n ? 0u : slack * slack + breaks[j]);
                if (cost < best[k * (n + 1u) + j]) { best[k * (n + 1u) + j] = cost; choice[k * (n + 1u) + j] = i; }
            }
        }
    unsigned lines = fewest;
    for (unsigned k = fewest; k <= most; k++)
        if (best[k * (n + 1u) + n] != ~0ul && best[k * (n + 1u) + n] + (unsigned long)(k - fewest) * COST_LINE < best[lines * (n + 1u) + n] + (unsigned long)(lines - fewest) * COST_LINE) lines = k;
    /* The breaks, from the last line back. */
    for (unsigned k = lines, j = n; k > 0; k--) {
        breaks[k] = j;
        j = choice[k * (n + 1u) + j];
    }
    breaks[0] = 0;
    for (unsigned k = 0; k < lines; k++) {
        const uint8_t newline = CODE_NEWLINE;
        if ((k && put(out, capacity, used, &newline, 1) != 0) || line_bytes(text, p, breaks[k], breaks[k + 1u], out, soft, capacity, used) != 0) goto done;
    }
    attributes[0] = before[n][0];
    attributes[1] = before[n][1];
    status = 0;
done:
    free(before);
    free(coloured);
    free(plain);
    free(shapes);
    free(extent);
    free(width);
    free(breaks);
    free(best);
    free(choice);
    return status;
}

/* Whether the page `text`[from, to) must keep its layout: a choice, a secret, a call, the piece of heart, a code the
 * layout does not know. */
static int page_kept(const uint8_t *text, size_t from, size_t to)
{
    for (size_t at = from; at < to; at += code_bytes(text[at])) {
        const uint8_t c = text[at], parameter = at + 1u < to ? text[at + 1u] : 0;
        if (c >= ORACLES_TEXT_FIRST_CHARACTER || c == CODE_NEWLINE) continue;
        if ((c >= CODE_DICTIONARY_FIRST && c < CODE_SYMBOL) || c == CODE_CALL || (c == CODE_NAME && parameter >= 2u)) return 1;
        if (c == CODE_COLOUR && !(parameter & 0x80u) && parameter >= COLOURS) return 1;
        if (c == CODE_COMMAND && (parameter >> 3 == COMMAND_OPTION || parameter >> 3 == COMMAND_HEART_PIECE)) return 1;
    }
    return 0;
}

/* The end of the page that starts at `at`: past its \stop or \heartpiece, or at the text's end (its terminator, a \jump,
 * a cmd8: *last set). */
static size_t page_end(const uint8_t *text, size_t length, size_t at, int *last)
{
    *last = 0;
    while (at < length) {
        const uint8_t c = text[at];
        if (c == CODE_END || c == CODE_JUMP || c == CODE_END_HERE) { *last = 1; return at; }
        if (c == CODE_COMMAND && at + 1u < length && (text[at + 1u] >> 3 == COMMAND_STOP || text[at + 1u] >> 3 == COMMAND_HEART_PIECE)) return at + 2u;
        at += code_bytes(c);
    }
    *last = 1;
    return length;
}

/* ---- checks ----------------------------------------------------------------------------------------------------- */

/* The text's characters and commands but its spaces, breaks and the hyphens `soft` marks: those of the words the
 * layout joins. */
static size_t skeleton(const uint8_t *text, size_t length, const uint8_t *soft, uint8_t *out)
{
    size_t n = 0;
    for (size_t at = 0; at < length; at += code_bytes(text[at])) {
        if (code_bytes(text[at]) == 2u) {
            out[n++] = text[at];
            if (at + 1u < length) out[n++] = text[at + 1u];
        } else if (text[at] != SPACE && text[at] != CODE_NEWLINE && !(text[at] == HYPHEN && soft[at])) out[n++] = text[at];
    }
    return n;
}

int language_wrap_fits(const language_wrap *wrap, const uint8_t *text, size_t length)
{
    uint8_t attributes[2] = { opening_attribute[0], opening_attribute[1] };
    size_t start = 0;
    for (size_t at = 0; at <= length; ) {
        const int end = at == length || text[at] == CODE_END || text[at] == CODE_JUMP || text[at] == CODE_END_HERE;
        if (end || text[at] == CODE_NEWLINE) {
            if (!line_fits(wrap, text + start, at - start, attributes, NULL)) return 0;
            if (end) return 1;
            start = at + 1u;
            at++;
            continue;
        }
        at += code_bytes(text[at]);
    }
    return 1;
}

unsigned language_text_boxes(const uint8_t *text, size_t length)
{
    unsigned boxes = 0;
    for (size_t at = 0; at < length;) {
        int last = 0;
        const size_t end = page_end(text, length, at, &last);
        unsigned lines = 1;
        for (size_t i = at; i < end; i += code_bytes(text[i]))
            if (text[i] == CODE_NEWLINE) lines++;
        if (end > at) boxes += (lines + 1u) / 2u;
        if (last) break;
        at = end < length && text[end] == CODE_NEWLINE ? end + 1u : end;
    }
    return boxes;
}

/* ---- a text ----------------------------------------------------------------------------------------------------- */

language_wrap_result language_wrap_text(const language_wrap *wrap, const uint8_t *text, size_t length, uint8_t *out, size_t capacity, size_t *out_length)
{
    unsigned pages = 0;
    return language_wrap_text_pages(wrap, text, length, out, capacity, out_length, NULL, NULL, 0, &pages);
}

language_wrap_result language_wrap_text_pages(const language_wrap *wrap, const uint8_t *text, size_t length, uint8_t *out, size_t capacity, size_t *out_length,
                                              size_t *page_from, size_t *page_to, unsigned page_capacity, unsigned *pages)
{
    *out_length = 0;
    *pages = 0;
    if (!length || length > ORACLES_TEXT_MAX) return WRAP_SAME;
    {   /* A single box: nothing to gain. */
        int last = 0;
        const size_t end = page_end(text, length, 0, &last);
        unsigned lines = 1;
        for (size_t i = 0; i < end; i += code_bytes(text[i]))
            if (text[i] == CODE_NEWLINE) lines++;
        if (last && lines <= 2u) return WRAP_SAME;
    }
    page *p = malloc(sizeof *p);
    uint8_t *written = calloc(1, capacity ? capacity : 1u);   /* the bytes of out the layout wrote, not copied from the text */
    /* The hyphens of the words joined: in the text, all; in out, those kept at a line's end. */
    uint8_t *soft_in = calloc(1, length), *soft_out = calloc(1, capacity ? capacity : 1u);
    if (!p || !written || !soft_in || !soft_out) { free(p); free(written); free(soft_in); free(soft_out); return WRAP_SAME; }
    uint8_t attributes[2] = { opening_attribute[0], opening_attribute[1] };
    size_t used = 0, at = 0;
    int failed = 0, laid = 0;
    for (;;) {
        int last = 0;
        const size_t end = page_end(text, length, at, &last);
        if (last && end < length && text[end] != CODE_END) {   /* a \jump or a cmd8: what follows as it is */
            failed |= put(out, capacity, &used, text + at, length - at);
            break;
        }
        if (page_kept(text, at, end) || words_of(wrap, text, at, end, p) != 0) {
            failed |= put(out, capacity, &used, text + at, end - at);
            follow_colours(text + at, end - at, attributes);
        } else {
            const size_t from = used;
            failed |= lay_out(wrap, text, p, attributes, out, soft_out, capacity, &used);
            for (size_t i = from; i < used; i++) written[i] = 1;
            for (unsigned w = 0; w < p->words; w++)
                if (p->after[w] == GAP_SOFT) soft_in[p->to[w] - 1u] = 1;
            if (*pages < page_capacity) { page_from[*pages] = from; page_to[*pages] = used; }
            (*pages)++;
            laid = 1;
        }
        if (failed || last) {
            failed |= put(out, capacity, &used, text + end, length - end);
            break;
        }
        /* A page's \stop ends its line: a break follows it, or the text ends. */
        at = end;
        if (at < length && text[at] == CODE_NEWLINE) failed |= put(out, capacity, &used, text + at++, 1);
        else if (at >= length || text[at] != CODE_END) { laid = 0; break; }
    }
    free(p);
    if (!laid || (used == length && !memcmp(out, text, length))) { free(written); free(soft_in); free(soft_out); return WRAP_SAME; }
    *out_length = used;
    /* An empty line the layout made fails; one of a page kept as it is, the text's own, stays. */
    int empty_line = 0;
    for (size_t i = 0; i + 1u < used; i += code_bytes(out[i]))
        if (written[i] && out[i] == CODE_NEWLINE && (out[i + 1u] == CODE_END || out[i + 1u] == CODE_NEWLINE)) empty_line = 1;
    free(written);
    uint8_t *a = failed ? NULL : malloc(2u * ORACLES_TEXT_MAX);
    if (!a) { free(soft_in); free(soft_out); return WRAP_FAILED; }
    const size_t na = skeleton(text, length, soft_in, a), nb = skeleton(out, used, soft_out, a + ORACLES_TEXT_MAX);
    const int same = na == nb && !memcmp(a, a + ORACLES_TEXT_MAX, na);
    free(a);
    free(soft_in);
    free(soft_out);
    if (!same || empty_line || !oracles_text_same_commands(text, length, out, used) || !language_wrap_fits(wrap, out, used)
        || language_text_boxes(out, used) > language_text_boxes(text, length)) return WRAP_FAILED;
    return WRAP_DONE;
}

unsigned language_text_inline_targets(const uint8_t *text, size_t length, uint8_t *targets, unsigned capacity)
{
    unsigned count = 0;
    int line_start = 1;
    for (size_t at = 0; at < length; at += code_bytes(text[at])) {
        const uint8_t c = text[at];
        if (c == CODE_END) break;
        if ((c == CODE_CALL || c == CODE_JUMP) && at + 1u < length) {
            const uint8_t next = at + 2u < length ? text[at + 2u] : CODE_END;
            const int alone = line_start && (c == CODE_JUMP || next == CODE_NEWLINE || next == CODE_END);
            if (!alone && text[at + 1u] < 0xfcu && count < capacity) targets[count++] = text[at + 1u];
            if (c == CODE_JUMP) break;
        }
        if (c == CODE_END_HERE) break;
        line_start = c == CODE_NEWLINE;
    }
    return count;
}
