/* The port's own translations, config/language_own.tsv and config/language_own_ja.tsv embedded at configure, and the
 * syntax they are written in.
 *
 * A row: a key, a language and the text, separated by tabs; a line starting with # is a comment.  The text is the
 * font's characters in UTF-8 (ASCII, and the accented letters and marks of the European font; in Japanese, the
 * Japanese font's kana and marks and the kanji font's kanji; in Polish, the Polish ROM's nine letters), \n a line
 * break, \stop the end of a box, \col(N) a colour (N hexadecimal), \\ a backslash, \xNN any other byte.  A table's
 * rows name its own languages alone: the European ROM's in config/language_own.tsv, Japanese in language_own_ja.tsv. */
#include "language_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const unsigned char oracles_language_own_texts[];
extern const unsigned char oracles_language_own_texts_ja[];

#define CODE_SYMBOL 0x06u

/* The Japanese font's characters (ROM_DATA_FORMATS, section 8.5): $60 to $af the hiragana, $b0 to $ff the katakana,
 * each small kana and each kana with its marks a character of its own, in this order; $20 to $5e ASCII's characters
 * at full width, $5f a middle dot. */
static const char kana[] = "あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわをん"
                           "ぁぃぅぇぉっゃゅょがぎぐげござじずぜぞだぢづでどばびぶべぼぱぴぷぺぽ"
                           "アイウエオカキクケコサシスセソタチツテトナニヌネノハヒフヘホマミムメモヤユヨラリルレロワヲン"
                           "ァィゥェォッャュョガギグゲゴザジズゼゾダヂヅデドバビブベボパピプペポ";
/* The marks besides: the code point each prints. */
static const struct { uint16_t code_point; uint8_t byte; } marks[] = {
    { 0x3000, 0x20 }, { 0x201c, 0x1a }, { 0x300c, 0x1b }, { 0x300d, 0x1c }, { 0x3001, 0x1e }, { 0x3002, 0x1f },
    { 0x201d, 0x22 }, { 0x30fc, 0x2d }, { 0x301c, 0x5c }, { 0x30fb, 0x5f }, { 0x2665, 0x14 },
};
/* The kanji font (gfx_font_jp), which \sym N prints from N = 0: '?' where its glyph is a drawing (a note, a keyhole,
 * the Triforce) or none. */
static const char kanji[] = "姫村下木東西南北地図出入口水氷池見門手力知恵勇気火金銀??実上四季春夏秋冬右左大小本王国男女少年"
                            "山人世中々剣花闇将軍真支配者鉄目詩死心節甲邪悪魔聖川結界生時炎?天空暗黒塔海仙?";

/* The font's characters past ASCII: the code point each byte from $80 prints, 0 where it prints none of a language's
 * letters (ROM_DATA_FORMATS, section 16). */
static const uint16_t letters[0x60] = {
    0x00c0, 0x00c2, 0x00c4, 0x00c6, 0x00c7, 0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00ce, 0x00cf, 0x00d1, 0x00d6, 0x0152, 0x00d9, 0x00db,   /* $80 */
    0x00dc, 0x00df, 0, 0, 0, 0, 0, 0, 0x00a1, 0x00bf, 0, 0, 0, 0, 0, 0,                                                               /* $90 */
    0x00e0, 0x00e2, 0x00e4, 0x00e6, 0x00e7, 0x00e8, 0x00e9, 0x00ea, 0x00eb, 0x00ee, 0x00ef, 0x00f1, 0x00f6, 0x0153, 0x00f9, 0x00fb,   /* $a0 */
    0x00fc, 0x00b4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                                                         /* $b0 */
    0x00d4, 0x00b0, 0x00c1, 0x00cd, 0x00d3, 0x00da, 0x00cc, 0x00d2, 0x00c5, 0, 0, 0, 0, 0, 0, 0,                                     /* $c0 */
    0x00f4, 0x00aa, 0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00ec, 0x00f2, 0x00e5, 0, 0, 0, 0, 0, 0, 0,                                     /* $d0 */
};

/* The Polish ROM's letters, from $80 (ROM_DATA_FORMATS, section 8.6): where the US font has its accented capitals. */
static const uint16_t polish_letters[] = { 0x0105, 0x0107, 0x0119, 0x015b, 0x0142, 0x0144, 0x00f3, 0x017c, 0x017a };

/* The UTF-8 character at *at, its code point; *at past it.  0 for a sequence that is not one. */
static unsigned utf8(const char **at, const char *end)
{
    const unsigned char *p = (const unsigned char *)*at;
    unsigned c = p[0];
    const unsigned n = c < 0x80u ? 0u : (c & 0xe0u) == 0xc0u ? 1u : (c & 0xf0u) == 0xe0u ? 2u : (c & 0xf8u) == 0xf0u ? 3u : 4u;
    if (n > 3u || (size_t)(end - *at) <= n) return 0;
    c &= n ? 0x3fu >> n : 0x7fu;
    for (unsigned i = 1; i <= n; i++) {
        if ((p[i] & 0xc0u) != 0x80u) return 0;
        c = c << 6 | (p[i] & 0x3fu);
    }
    *at += n + 1u;
    return c;
}

static int hex_byte(const char *at, const char *end, unsigned *value)
{
    *value = 0;
    if (end - at < 2) return -1;
    for (unsigned i = 0; i < 2u; i++) {
        const int c = tolower((unsigned char)at[i]);
        if (!isxdigit(c)) return -1;
        *value = *value << 4 | (unsigned)(isdigit(c) ? c - '0' : c - 'a' + 10);
    }
    return 0;
}

/* The bytes of the escape at *at (\n, \\, \stop, \col(N), \xNN), *at past it: how many, written to out; 0 when it is
 * none. */
static unsigned escape_bytes(const char **at, const char *end, uint8_t out[2])
{
    const char *e = *at;
    const size_t left = (size_t)(end - e);
    unsigned value = 0;
    if (left >= 2u && e[1] == 'n') { out[0] = 0x01; *at += 2; return 1; }
    if (left >= 2u && e[1] == '\\') { out[0] = '\\'; *at += 2; return 1; }
    if (left >= 5u && !memcmp(e, "\\stop", 5)) { out[0] = 0x0c; out[1] = 0x18; *at += 5; return 2; }
    if (left >= 8u && !memcmp(e, "\\col(", 5) && e[7] == ')' && hex_byte(e + 5, end, &value) == 0) { out[0] = 0x09; out[1] = (uint8_t)value; *at += 8; return 2; }
    if (left >= 4u && e[1] == 'x' && hex_byte(e + 2, end, &value) == 0 && value) { out[0] = (uint8_t)value; *at += 4; return 1; }
    return 0;
}

/* The byte the US font with the European glyphs prints a character by; 0 when it has it not. */
static unsigned european_bytes(unsigned c, uint8_t out[2])
{
    if (c >= 0x20u && c < 0x7fu && c != '\\') { out[0] = (uint8_t)c; return 1; }
    for (unsigned i = 0; c && i < sizeof letters / sizeof letters[0]; i++) if (letters[i] == c) { out[0] = (uint8_t)(0x80u + i); return 1; }
    return 0;
}

/* The byte the US font with the Polish glyphs prints a character by; 0 when it has it not. */
static unsigned polish_bytes(unsigned c, uint8_t out[2])
{
    if (c >= 0x20u && c < 0x7fu && c != '\\') { out[0] = (uint8_t)c; return 1; }
    for (unsigned i = 0; c && i < sizeof polish_letters / sizeof polish_letters[0]; i++) if (polish_letters[i] == c) { out[0] = (uint8_t)(0x80u + i); return 1; }
    return 0;
}

/* The n-th code point of a UTF-8 string that holds no other, 0 past its end. */
static unsigned nth_code_point(const char *string, unsigned n)
{
    const char *at = string, *end = string + strlen(string);
    unsigned c = 0, i = 0;
    for (; i <= n && at < end; i++) c = utf8(&at, end);
    return i == n + 1u ? c : 0;
}

/* The bytes the Japanese fonts print a character by: one, or a kanji's \sym and its glyph; 0 when they have it not. */
static unsigned japanese_bytes(unsigned c, uint8_t out[2])
{
    if (c >= 0xff01u && c <= 0xff5eu) c -= 0xfee0u;   /* full width, as ASCII */
    if (c >= 0x20u && c < 0x5fu && c != '\\') { out[0] = (uint8_t)c; return 1; }
    for (size_t i = 0; i < sizeof marks / sizeof marks[0]; i++) if (marks[i].code_point == c) { out[0] = marks[i].byte; return 1; }
    for (unsigned i = 0; i < 0xa0u; i++) if (nth_code_point(kana, i) == c) { out[0] = (uint8_t)(0x60u + i); return 1; }
    for (unsigned i = 0, k; c != '?' && (k = nth_code_point(kanji, i)) != 0; i++)
        if (k == c) { out[0] = CODE_SYMBOL; out[1] = (uint8_t)i; return 2; }
    return 0;
}

int language_text_encode_in(OraclesLanguage language, const char *text, size_t length, uint8_t *out, size_t capacity, size_t *out_length)
{
    size_t n = 0;
    const char *end = text + length;
    for (const char *at = text; at < end;) {
        uint8_t bytes[2];
        const unsigned count = *at == '\\' ? escape_bytes(&at, end, bytes)
                             : language == ORACLES_LANGUAGE_JAPANESE ? japanese_bytes(utf8(&at, end), bytes)
                             : language == ORACLES_LANGUAGE_POLISH ? polish_bytes(utf8(&at, end), bytes) : european_bytes(utf8(&at, end), bytes);
        if (!count || n + count >= capacity) return -1;
        memcpy(out + n, bytes, count);
        n += count;
    }
    out[n++] = 0;
    *out_length = n;
    return 0;
}

int language_text_encode(const char *text, size_t length, uint8_t *out, size_t capacity, size_t *out_length)
{
    return language_text_encode_in(ORACLES_LANGUAGE_ENGLISH_EU, text, length, out, capacity, out_length);
}

/* The tables of the port's own texts, the European languages' then Japanese's. */
static const unsigned char *const own_tables[] = { oracles_language_own_texts, oracles_language_own_texts_ja };

int language_own_table_check(const char *table, int japanese, char *error, size_t capacity)
{
    unsigned number = 0;
    for (const char *line = table; *line;) {
        const char *end = strchr(line, '\n');
        if (!end) end = line + strlen(line);
        const char *next = *end ? end + 1 : end;
        number++;
        if (end > line && end[-1] == '\r') end--;
        if (*line != '#' && end > line) {
            const char *tab1 = memchr(line, '\t', (size_t)(end - line));
            const char *tab2 = tab1 ? memchr(tab1 + 1, '\t', (size_t)(end - tab1 - 1)) : NULL;
            char code[8] = { 0 };
            OraclesLanguage language = ORACLES_LANGUAGE_ENGLISH;
            if (tab2 && tab2 - tab1 >= 2 && (size_t)(tab2 - tab1 - 1) < sizeof code) memcpy(code, tab1 + 1, (size_t)(tab2 - tab1 - 1));
            const int parsed = tab2 && oracles_language_parse(code, &language) == 0;
            if (!parsed || (japanese ? language != ORACLES_LANGUAGE_JAPANESE : language == ORACLES_LANGUAGE_ENGLISH || language >= ORACLES_LANGUAGE_EUROPEAN_END)) {
                snprintf(error, capacity, "the port's own %stexts' row %u names a language their table does not hold", japanese ? "Japanese " : "", number);
                return -1;
            }
        }
        line = next;
    }
    return 0;
}

int language_own_check(char *error, size_t capacity)
{
    for (size_t table = 0; table < sizeof own_tables / sizeof own_tables[0]; table++)
        if (language_own_table_check((const char *)own_tables[table], table == 1u, error, capacity) != 0) return -1;
    return 0;
}

int language_own_nth(unsigned n, const char **key, size_t *key_length, OraclesLanguage *language, const char **text, size_t *text_length)
{
    unsigned row = 0;
    for (size_t table = 0; table < sizeof own_tables / sizeof own_tables[0]; table++) {
        for (const char *line = (const char *)own_tables[table]; *line;) {
            const char *end = strchr(line, '\n');
            if (!end) end = line + strlen(line);
            const char *next = *end ? end + 1 : end;
            if (end > line && end[-1] == '\r') end--;
            if (*line != '#' && end > line && row++ == n) {
                const char *tab1 = memchr(line, '\t', (size_t)(end - line));
                const char *tab2 = tab1 ? memchr(tab1 + 1, '\t', (size_t)(end - tab1 - 1)) : NULL;
                char code[8] = { 0 };
                if (!tab2 || tab2 - tab1 < 2 || (size_t)(tab2 - tab1 - 1) >= sizeof code) return -1;
                memcpy(code, tab1 + 1, (size_t)(tab2 - tab1 - 1));
                if (oracles_language_parse(code, language) != 0) return -1;
                *key = line;
                *key_length = (size_t)(tab1 - line);
                *text = tab2 + 1;
                *text_length = (size_t)(end - tab2 - 1);
                return 0;
            }
            line = next;
        }
    }
    return -1;
}

int language_own_text(const char *key, OraclesLanguage language, uint8_t *out, size_t capacity, size_t *out_length)
{
    const char *k, *text;
    size_t k_length, text_length;
    OraclesLanguage l;
    for (unsigned n = 0; language_own_nth(n, &k, &k_length, &l, &text, &text_length) == 0; n++)
        if (l == language && k_length == strlen(key) && !memcmp(k, key, k_length)) return language_text_encode_in(language, text, text_length, out, capacity, out_length);
    return -1;
}
