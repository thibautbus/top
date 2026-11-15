/* The texts laid out again for the compact text (language_wrap.c), without a ROM: a font of hand-made widths, texts
 * built here.  The boxes a text shows; pages filled to the width, each on its own, a \stop kept; a single box, a choice,
 * a call, a secret and lines aligned with spaces left as they are; a word too wide for a line; a word broken at a hyphen
 * joined again in German and Italian, a capital after it or a word the language writes with it keeping the hyphen, and
 * broken there again when it must; a compound's hyphen kept in English, French and Spanish, but in a word they write
 * whole; never a hyphen in Japanese; Polish's small letters where the European font has capitals; a line measured with a symbol on its tile, two colours in a tile, a name and a
 * number at their widest; a text after its \jump as it is; the texts called within a line; the table of kept texts. */
#include "language_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

#define WIDTH 128u
#define STOP "\x0c\x18"
#define OPTION "\x0c\x10"
#define CHILD "\x0a\x01"
#define NUMBER "\x0c\x08"

/* Letters five pixels wide (an advance of 6), 'i' and 'l' one (2), 'W' seven (8), a blank space (5), the digits five. */
static OraclesTextFont font_of(void)
{
    OraclesTextFont font;
    memset(&font, 0, sizeof font);
    for (unsigned c = ORACLES_TEXT_FIRST_CHARACTER; c < 0x100u; c++) { font.left[c] = 1; font.width[c] = 5; font.advance[c] = 6; }
    font.left[' '] = 0; font.width[' '] = 0; font.advance[' '] = ORACLES_TEXT_SPACE_ADVANCE;
    font.width['i'] = font.width['l'] = 1; font.advance['i'] = font.advance['l'] = 2;
    font.left['W'] = 0; font.width['W'] = 7; font.advance['W'] = 8;
    return font;
}

/* `text` (a C string, its 0 the terminator) laid out again: the result, and the bytes into `out`. */
static language_wrap_result wrapped(const language_wrap *wrap, const char *text, char *out, size_t *length)
{
    uint8_t bytes[ORACLES_TEXT_MAX];
    const language_wrap_result result = language_wrap_text(wrap, (const uint8_t *)text, strlen(text) + 1u, bytes, sizeof bytes, length);
    memcpy(out, bytes, *length);
    if (result != WRAP_DONE) out[0] = 0;
    return result;
}

static int same(const char *out, size_t length, const char *expected) { return length == strlen(expected) + 1u && !memcmp(out, expected, length); }

static void test_boxes(void)
{
    const char *texts[] = { "a", "a\x01" "b", "a\x01" "b\x01" "c", "a" STOP "\x01" "b\x01" "c\x01" "d", "a\x01" "b\x07\x03" "c\x01" "d\x01" "e" };
    const unsigned boxes[] = { 1, 1, 2, 3, 1 };
    for (unsigned i = 0; i < 5u; i++) CHECK(language_text_boxes((const uint8_t *)texts[i], strlen(texts[i]) + 1u) == boxes[i]);
}

static void test_pages(const language_wrap *wrap)
{
    char out[ORACLES_TEXT_MAX];
    size_t n = 0;
    /* Four lines, two boxes: one line. */
    CHECK(wrapped(wrap, "aaaa\x01" "bbbb\x01" "cccc\x01" "dddd", out, &n) == WRAP_DONE && same(out, n, "aaaa bbbb cccc dddd"));
    /* A single box: as it is. */
    CHECK(wrapped(wrap, "aaaa\x01" "bbbb", out, &n) == WRAP_SAME);
    /* Each page alone, its \stop and the break after it kept. */
    CHECK(wrapped(wrap, "aa\x01" "bb\x01" "cc" STOP "\x01" "dd\x01" "ee\x01" "ff", out, &n) == WRAP_DONE && same(out, n, "aa bb cc" STOP "\x01" "dd ee ff"));
    /* As many words as a line holds (21 letters of 6 pixels: 125), the rest on the next. */
    CHECK(wrapped(wrap, "aaaaaaa\x01" "aaaaaaa\x01" "aaaaaaa\x01" "b", out, &n) == WRAP_DONE && same(out, n, "aaaaaaa aaaaaaa\x01" "aaaaaaa b"));
    /* A choice, a call, a secret, a leading space, two spaces: that page as it is, the others laid out. */
    const char *kept[] = { "x\x01" "y\x01" "z " OPTION "Yes", "x\x01" "y\x0f\x05" "z\x01" "w", "x\x01" "y\x0a\x02" "z\x01" "w", "x\x01" " y\x01" "z", "x\x01" "y  z\x01" "w" };
    for (unsigned i = 0; i < 5u; i++) {
        char text[64], expected[64];
        snprintf(text, sizeof text, "aa\x01" "bb\x01" "cc" STOP "\x01%s", kept[i]);
        snprintf(expected, sizeof expected, "aa bb cc" STOP "\x01%s", kept[i]);
        CHECK(wrapped(wrap, text, out, &n) == WRAP_DONE && same(out, n, expected));
        CHECK(wrapped(wrap, kept[i], out, &n) == WRAP_SAME);
    }
    /* A page kept for its empty line, the text's own, after its \stop or within it: kept, the other page laid out. */
    CHECK(wrapped(wrap, "aa\x01" "bb\x01" "cc" STOP "\x01\x01" "dd\x01" "ee", out, &n) == WRAP_DONE && same(out, n, "aa bb cc" STOP "\x01\x01" "dd\x01" "ee"));
    CHECK(wrapped(wrap, "aa\x01" "bb\x01" "cc\x01\x01" "dd" STOP "\x01" "ee\x01" "ff\x01" "gg", out, &n) == WRAP_DONE
          && same(out, n, "aa\x01" "bb\x01" "cc\x01\x01" "dd" STOP "\x01" "ee ff gg"));
    /* From a \jump on, as it is: what follows is another text's. */
    CHECK(wrapped(wrap, "aa\x01" "bb\x01" "cc" STOP "\x01" "dd\x01" "ee\x07\x03" "ff\x01" "gg", out, &n) == WRAP_DONE
          && same(out, n, "aa bb cc" STOP "\x01" "dd\x01" "ee\x07\x03" "ff\x01" "gg"));
    /* A word wider than a line: the text keeps its layout. */
    CHECK(wrapped(wrap, "WWWWWWWWWWWWWWWWW\x01" "a\x01" "b", out, &n) == WRAP_FAILED);
}

static void test_hyphens(language_wrap *wrap)
{
    char out[ORACLES_TEXT_MAX];
    size_t n = 0;
    /* A small letter after the hyphen: one word, the hyphen dropped; a capital: the hyphen kept. */
    CHECK(wrapped(wrap, "a Ver-\x01" "gang\x01" "Ly-\x01" "Dorf", out, &n) == WRAP_DONE && same(out, n, "a Vergang Ly-Dorf"));
    /* A word the language writes with its hyphen inside a line keeps it. */
    const char *learned = "der Nord-ost Wind";
    language_wrap_learn(wrap, (const uint8_t *)learned, strlen(learned) + 1u);
    CHECK(wrapped(wrap, "aa Nord-\x01" "ost\x01" "bb\x01" "cc", out, &n) == WRAP_DONE && same(out, n, "aa Nord-ost bb cc"));
    /* So does a word whose part after the hyphen the language writes after one ("dis-moi": "rends-moi"). */
    const char *pronoun = "dis-moi tout";
    language_wrap_learn(wrap, (const uint8_t *)pronoun, strlen(pronoun) + 1u);
    CHECK(wrapped(wrap, "aa rends-\x01" "moi\x01" "bb\x01" "cc", out, &n) == WRAP_DONE && same(out, n, "aa rends-moi bb cc"));
    /* A dash of two hyphens at a line's end: the line goes on without a space. */
    CHECK(wrapped(wrap, "aa--\x01" "bb\x01" "cc\x01" "dd", out, &n) == WRAP_DONE && same(out, n, "aa--bb cc dd"));
    /* Broken again at its hyphen when the word does not fit a line whole. */
    CHECK(wrapped(wrap, "aaaaaaaaaaaaaaaaaaaa-\x01" "bbbbbbbbbbbbbbbbbbbb\x01" "cc\x01" "dd", out, &n) == WRAP_DONE
          && same(out, n, "aaaaaaaaaaaaaaaaaaaa-\x01" "bbbbbbbbbbbbbbbbbbbb\x01" "cc dd"));
}

/* In English, French and Spanish a hyphen at a line's end is a compound's: kept, inside a line too, but in a word the
 * language writes whole elsewhere, whatever the case of its capitals, or neither of whose parts it writes alone. */
static void test_compounds(void)
{
    const OraclesTextFont font = font_of();
    const OraclesLanguage languages[] = { ORACLES_LANGUAGE_ENGLISH, ORACLES_LANGUAGE_ENGLISH_EU, ORACLES_LANGUAGE_FRENCH, ORACLES_LANGUAGE_SPANISH };
    const char *written = "There is Something in the Northwest: a nice rock, sized like lava, grand chose.";
    for (unsigned l = 0; l < sizeof languages / sizeof languages[0]; l++) {
        language_wrap *wrap = language_wrap_new(&font, WIDTH, languages[l]);
        CHECK(wrap != NULL);
        if (!wrap) continue;
        language_wrap_learn(wrap, (const uint8_t *)written, strlen(written) + 1u);
        char out[ORACLES_TEXT_MAX];
        size_t n = 0;
        CHECK(wrapped(wrap, "hot lava-\x01" "like\x01" "bb\x01" "cc", out, &n) == WRAP_DONE && same(out, n, "hot lava-like bb cc"));
        CHECK(wrapped(wrap, "aa grand-\x01" "chose\x01" "bb\x01" "cc", out, &n) == WRAP_DONE && same(out, n, "aa grand-chose bb cc"));
        CHECK(wrapped(wrap, "aa nice-\x01" "sized\x01" "bb\x01" "cc", out, &n) == WRAP_DONE && same(out, n, "aa nice-sized bb cc"));
        CHECK(wrapped(wrap, "a some-\x01" "thing\x01" "North-\x01" "west", out, &n) == WRAP_DONE && same(out, n, "a something Northwest"));
        CHECK(wrapped(wrap, "aa cer-\x01" "tainly\x01" "bb\x01" "cc", out, &n) == WRAP_DONE && same(out, n, "aa certainly bb cc"));
        language_wrap_free(wrap);
    }
    /* German and Italian break their words by syllables: joined without the hyphen. */
    const OraclesLanguage syllables[] = { ORACLES_LANGUAGE_GERMAN, ORACLES_LANGUAGE_ITALIAN };
    for (unsigned l = 0; l < 2u; l++) {
        language_wrap *wrap = language_wrap_new(&font, WIDTH, syllables[l]);
        char out[ORACLES_TEXT_MAX];
        size_t n = 0;
        if (wrap) language_wrap_learn(wrap, (const uint8_t *)written, strlen(written) + 1u);
        CHECK(wrap && wrapped(wrap, "hot lava-\x01" "like\x01" "bb\x01" "cc", out, &n) == WRAP_DONE && same(out, n, "hot lavalike bb cc"));
        language_wrap_free(wrap);
    }
    /* In Japanese $2d is the katakana's long mark and the kana take the Latin letters' codes: a line's end after it
     * joins with a space, as any other, and two of them make no dash. */
    language_wrap *japanese = language_wrap_new(&font, WIDTH, ORACLES_LANGUAGE_JAPANESE);
    char out[ORACLES_TEXT_MAX];
    size_t n = 0;
    if (japanese) language_wrap_learn(japanese, (const uint8_t *)written, strlen(written) + 1u);
    CHECK(japanese && wrapped(japanese, "\xa4\x85-\x01" "\xaa\x83\x01" "bb\x01" "cc", out, &n) == WRAP_DONE && same(out, n, "\xa4\x85- \xaa\x83 bb cc"));
    CHECK(japanese && wrapped(japanese, "aa--\x01" "bb\x01" "cc\x01" "dd", out, &n) == WRAP_DONE && same(out, n, "aa-- bb cc dd"));
    CHECK(japanese && wrapped(japanese, "hot lava-\x01" "like\x01" "bb\x01" "cc", out, &n) == WRAP_DONE && same(out, n, "hot lava- like bb cc"));
    language_wrap_free(japanese);
    /* In Polish $80 to $88 are small letters (ą to ź) where the European font has its accented capitals: a word broken
     * before one is joined as before any small letter, its hyphen dropped; a compound it writes with one keeps it.  In
     * French, before a capital, the hyphen stays. */
    static const char *const broken = "a ra\x88ni-\x01" "\x83" "ci\x80!\x01" "bb\x01" "cc";
    language_wrap *polish = language_wrap_new(&font, WIDTH, ORACLES_LANGUAGE_POLISH), *french = language_wrap_new(&font, WIDTH, ORACLES_LANGUAGE_FRENCH);
    const char *goron = "z Goro i tance";
    if (polish) language_wrap_learn(polish, (const uint8_t *)goron, strlen(goron) + 1u);
    CHECK(polish && wrapped(polish, broken, out, &n) == WRAP_DONE && same(out, n, "a ra\x88ni\x83" "ci\x80! bb cc"));
    CHECK(polish && wrapped(polish, "Goro-\x01" "tance!\x01" "bb\x01" "cc", out, &n) == WRAP_DONE && same(out, n, "Goro-tance! bb cc"));
    CHECK(french && wrapped(french, broken, out, &n) == WRAP_DONE && same(out, n, "a ra\x88ni-\x83" "ci\x80! bb cc"));
    language_wrap_free(polish);
    language_wrap_free(french);
}

static void test_measure(const language_wrap *wrap)
{
    char text[128];
    /* A symbol takes a whole tile from the next tile's edge: after 20 letters (120 pixels) it fits, after 21 not. */
    snprintf(text, sizeof text, "%s\x06\x10", "aaaaaaaaaaaaaaaaaaaa");
    CHECK(language_wrap_fits(wrap, (const uint8_t *)text, strlen(text) + 1u));
    snprintf(text, sizeof text, "%s\x06\x10", "aaaaaaaaaaaaaaaaaaaaa");
    CHECK(!language_wrap_fits(wrap, (const uint8_t *)text, strlen(text) + 1u));
    /* Two attributes in a tile: the next tile's edge.  21 letters fit, not with a blue one among them. */
    CHECK(language_wrap_fits(wrap, (const uint8_t *)"aaaaaaaaaaaaaaaaaaaaa", 22));
    snprintf(text, sizeof text, "%s\x09\x03" "a\x09\x01" "a", "aaaaaaaaaaaaaaaaaaa");
    CHECK(!language_wrap_fits(wrap, (const uint8_t *)text, strlen(text) + 1u));
    /* A name: five of the widest letters a name holds ('W', 8 pixels). */
    snprintf(text, sizeof text, "%s " CHILD, "aaaaaaaaaaaaaa");
    CHECK(language_wrap_fits(wrap, (const uint8_t *)text, strlen(text) + 1u));
    snprintf(text, sizeof text, "%s " CHILD, "aaaaaaaaaaaaaaa");
    CHECK(!language_wrap_fits(wrap, (const uint8_t *)text, strlen(text) + 1u));
    /* A number: three digits (6 pixels each). */
    snprintf(text, sizeof text, "%s " NUMBER, "aaaaaaaaaaaaaaaaa");
    CHECK(language_wrap_fits(wrap, (const uint8_t *)text, strlen(text) + 1u));
    snprintf(text, sizeof text, "%s " NUMBER, "aaaaaaaaaaaaaaaaaa");
    CHECK(!language_wrap_fits(wrap, (const uint8_t *)text, strlen(text) + 1u));
    /* Laid out with a blue letter, which takes its tile: the next word does not fit after it. */
    char out[ORACLES_TEXT_MAX];
    size_t n = 0;
    CHECK(wrapped(wrap, "aaaaaaaaaa\x01" "aaaaaaaaa\x09\x03" "a\x09\x01\x01" "a\x01" "b", out, &n) == WRAP_DONE
          && same(out, n, "aaaaaaaaaa aaaaaaaaa\x09\x03" "a\x09\x01\x01" "a b"));
}

static void test_targets(void)
{
    uint8_t targets[4];
    const struct { const char *text; unsigned count, first; } cases[] = {
        { "abc \x0f\x05" "def", 1, 5 },            /* a call within a line */
        { "\x0f\x06\x01" "abc", 0, 0 },            /* a call on a line of its own */
        { "abc\x01\x07\x07", 0, 0 },               /* a jump at a line's start */
        { "abc\x07\x08", 1, 8 },                   /* a jump within a line */
        { "abc \x0f\xfd" "def", 0, 0 },            /* a substitution: no text known */
    };
    for (unsigned i = 0; i < 5u; i++) {
        const unsigned count = language_text_inline_targets((const uint8_t *)cases[i].text, strlen(cases[i].text) + 1u, targets, 4);
        CHECK(count == cases[i].count && (!count || targets[0] == cases[i].first));
    }
}

static void test_keep(void)
{
    char error[128];
    language_keep *keep = language_keep_parse("# a comment\n0001 0003-0005\tfr,de\twhy\n0200\tall\twhy\n", error, sizeof error);
    CHECK(keep != NULL);
    CHECK(language_keep_has(keep, ORACLES_LANGUAGE_FRENCH, 0x0004) && language_keep_has(keep, ORACLES_LANGUAGE_GERMAN, 0x0001));
    CHECK(!language_keep_has(keep, ORACLES_LANGUAGE_ITALIAN, 0x0004) && !language_keep_has(keep, ORACLES_LANGUAGE_FRENCH, 0x0002));
    CHECK(language_keep_has(keep, ORACLES_LANGUAGE_ENGLISH, 0x0200) && language_keep_has(keep, ORACLES_LANGUAGE_SPANISH, 0x0200));
    language_keep_free(keep);
    CHECK(!language_keep_parse("0001\txx\twhy\n", error, sizeof error) && !language_keep_parse("01\tfr\twhy\n", error, sizeof error));
    CHECK(!language_keep_parse("0001\tfr\n", error, sizeof error));
    keep = language_keep_load(ORACLES_ROM_REVISION_AGES_US, error, sizeof error);
    CHECK(keep != NULL);
    language_keep_free(keep);
    keep = language_keep_load(ORACLES_ROM_REVISION_SEASONS_US, error, sizeof error);
    CHECK(keep != NULL);
    language_keep_free(keep);
    const OraclesRomRevision fans[] = { ORACLES_ROM_REVISION_MOONRISE_1_0_6, ORACLES_ROM_REVISION_KINOMI_1_1_2, ORACLES_ROM_REVISION_TEMPLE_1_073 };
    for (unsigned f = 0; f < sizeof fans / sizeof fans[0]; f++) {
        keep = language_keep_load(fans[f], error, sizeof error);
        CHECK(keep != NULL && language_keep_has(keep, ORACLES_LANGUAGE_ENGLISH, f < 2u ? 0x570fu : 0x2702u));
        CHECK(!language_keep_has(keep, ORACLES_LANGUAGE_FRENCH, f < 2u ? 0x570fu : 0x2702u));
        language_keep_free(keep);
    }
    /* Each image its table, and its layout its own digest. */
    const OraclesLanguageLayout layout = { WIDTH, 0, 0, NULL };
    char ages[41], moonrise[41];
    oracles_language_layout_digest(&layout, ORACLES_ROM_REVISION_AGES_US, ages);
    oracles_language_layout_digest(&layout, ORACLES_ROM_REVISION_MOONRISE_1_0_6, moonrise);
    CHECK(strcmp(ages, moonrise) != 0);
}

int main(void)
{
    const OraclesTextFont font = font_of();
    language_wrap *wrap = language_wrap_new(&font, WIDTH, ORACLES_LANGUAGE_GERMAN);
    CHECK(wrap != NULL && !language_wrap_new(&font, 0, ORACLES_LANGUAGE_GERMAN) && !language_wrap_new(&font, 257, ORACLES_LANGUAGE_GERMAN));
    if (!wrap) return 1;
    test_boxes();
    test_pages(wrap);
    test_hyphens(wrap);
    test_compounds();
    test_measure(wrap);
    test_targets();
    test_keep();
    language_wrap_free(wrap);
    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("language wrap: all tests passed\n");
    return 0;
}
