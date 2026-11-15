/* The languages without a ROM: the text decoder on constructed tables (the dictionary, its fragments nested, the
 * control codes kept, a list and a text across a bank, two bases and three), the comparison of the commands, the
 * tables of exceptions, the rules on constructed texts, the port's own texts and their syntax, the pack's file, the
 * composition into a synthetic US image (the index, the texts kept in English, the glyphs), the graphics' compression
 * both ways, their place in the pack and their headers repointed in a synthetic image, the guest's transaction on a
 * synthetic program that calls getTextAddress (a text laid out again for the compact text served and counted), and
 * English (US) composed laid out for the compact text. */
#include "core.h"
#include "guest.h"
#include "language_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

#define MIB (1024u * 1024u)

extern const OraclesGuestTables oracles_guest_tables_moonrise;

static void put16(uint8_t *at, unsigned v) { at[0] = (uint8_t)v; at[1] = (uint8_t)(v >> 8); }

/* Writes a text table at `table`: seven internal groups (the four of the dictionary, two of texts, one empty), their
 * lists right after it, across the bank at $8000; the first base begins where the last list ends. */
#define TABLE 0x7ff0u
#define BASE0 (TABLE + 30u)
static void write_table(uint8_t *rom)
{
    const unsigned offsets[7] = { 14, 18, 14, 14, 20, 26, 14 };   /* groups 2, 3 and 6 share group 0's list */
    for (unsigned g = 0; g < 7; g++) put16(rom + TABLE + 2u * g, offsets[g]);
    const unsigned g0[2] = { 0x000, 0x010 }, g1[1] = { 0x020 }, g4[3] = { 0x100, 0x110, 0 }, g5[2] = { 0x200, 0x210 };
    for (unsigned i = 0; i < 2; i++) put16(rom + TABLE + 14u + 2u * i, g0[i]);
    put16(rom + TABLE + 18u, g1[0]);
    for (unsigned i = 0; i < 3; i++) put16(rom + TABLE + 20u + 2u * i, g4[i]);
    for (unsigned i = 0; i < 2; i++) put16(rom + TABLE + 26u + 2u * i, g5[i]);
}

/* The dictionary at `base`, group 4's texts at `base4` and group 5's at `base5`; group 4's third text across
 * the bank, its offset from `base4` written in its list. */
static void write_texts(uint8_t *rom, size_t base, size_t base4, size_t base5)
{
    static const uint8_t fragment0[] = { 't', 'h', 'e', ' ', 0 }, fragment1[] = { 'a', 0x03, 0x00, '!', 0 }, fragment2[] = { 'X', 'Y', 0 };
    static const uint8_t text0[] = { 'H', 'i', 0x02, 0x00, 'e', 'n', 'd', 0x01, 0x09, 0x02, 'Z', 0x0a, 0x00, 0 };
    static const uint8_t text1[] = { 0x02, 0x01, 0x0c, 0x18, 0 };
    static const uint8_t text2[] = { 'A', 'B', 'C', 'D', 0 };
    static const uint8_t five0[] = { 'f', 'i', 'v', 'e', 0 }, five1[] = { 0x0f, 0x05, 0 };
    memcpy(rom + base + 0x000, fragment0, sizeof fragment0);
    memcpy(rom + base + 0x010, fragment1, sizeof fragment1);
    memcpy(rom + base + 0x020, fragment2, sizeof fragment2);
    memcpy(rom + base4 + 0x100, text0, sizeof text0);
    memcpy(rom + base4 + 0x110, text1, sizeof text1);
    const size_t across = (base4 / 0x4000u + 2u) * 0x4000u - 2u;
    memcpy(rom + across, text2, sizeof text2);
    put16(rom + TABLE + 24u, (unsigned)(across - base4));
    memcpy(rom + base5 + 0x200, five0, sizeof five0);
    memcpy(rom + base5 + 0x210, five1, sizeof five1);
}

static int decoded(const OraclesRom *rom, const OraclesTextSource *s, unsigned group, unsigned index, const uint8_t *expected, size_t length)
{
    uint8_t out[64];
    size_t n = 0;
    return oracles_text_decode(rom, s, group, index, out, sizeof out, &n) == ORACLES_OK && n == length && !memcmp(out, expected, length);
}

static void test_decoder(void)
{
    uint8_t *data = calloc(MIB, 1);
    write_table(data);
    static const uint8_t text0[] = { 'H', 'i', 't', 'h', 'e', ' ', 'e', 'n', 'd', 0x01, 0x09, 0x02, 'Z', 0x0a, 0x00, 0 };
    static const uint8_t text1[] = { 'a', 'X', 'Y', '!', 0x0c, 0x18, 0 }, text2[] = { 'A', 'B', 'C', 'D', 0 };
    static const uint8_t five0[] = { 'f', 'i', 'v', 'e', 0 }, five1[] = { 0x0f, 0x05, 0 };
    const OraclesRom rom = { data, MIB };

    /* Two bases, as the US ROMs: group 5 counts from the second. */
    write_texts(data, BASE0, BASE0, 0x20000u);
    const OraclesTextSource two = { TABLE, { BASE0, 0x20000u, 0x20000u }, { 5, 0x100 } };
    CHECK(oracles_text_groups(&rom, &two) == 7);
    CHECK(oracles_text_count(&rom, &two, 4) == 3);
    CHECK(oracles_text_count(&rom, &two, 5) == 2);   /* the last list ends where the first base begins */
    CHECK(oracles_text_count(&rom, &two, 6) == 0);   /* group 0's list: empty */
    CHECK(oracles_text_count(&rom, &two, 1) < 0);    /* the dictionary has no count */
    CHECK(decoded(&rom, &two, 4, 0, text0, sizeof text0));
    CHECK(decoded(&rom, &two, 4, 1, text1, sizeof text1));   /* a fragment inside a fragment */
    CHECK(decoded(&rom, &two, 4, 2, text2, sizeof text2));   /* across the bank */
    CHECK(decoded(&rom, &two, 5, 0, five0, sizeof five0));
    CHECK(decoded(&rom, &two, 5, 1, five1, sizeof five1));   /* a call kept as it is */
    CHECK(oracles_text_address(&rom, &two, 5, 1) == 0x20210);
    {
        uint8_t small[4];
        size_t n = 0;
        CHECK(oracles_text_decode(&rom, &two, 4, 0, small, sizeof small, &n) == ORACLES_ERR_CAPACITY);
    }

    /* Three bases, as the European ROMs: group 4 counts from the second, group 5 from the third. */
    memset(data, 0, MIB);
    write_table(data);
    write_texts(data, BASE0, 0x30000u, 0x40000u);
    const OraclesTextSource three = { TABLE, { BASE0, 0x30000u, 0x40000u }, { 4, 5 } };
    CHECK(decoded(&rom, &three, 4, 0, text0, sizeof text0));
    CHECK(decoded(&rom, &three, 4, 2, text2, sizeof text2));
    CHECK(decoded(&rom, &three, 5, 0, five0, sizeof five0));
    CHECK(oracles_text_address(&rom, &three, 4, 1) == 0x30110);

    /* A fragment that inserts itself: refused, never a loop. */
    data[BASE0 + 0x020] = 0x03;
    data[BASE0 + 0x021] = 0x00;
    {
        uint8_t out[64];
        size_t n = 0;
        CHECK(oracles_text_decode(&rom, &three, 4, 1, out, sizeof out, &n) == ORACLES_ERR_FORMAT);
    }
    free(data);
}

static void test_commands(void)
{
    static const uint8_t us[] = { 'H', 0x09, 0x01, 'i', 0x01, 0x0a, 0x00, 0x0c, 0x08, 0x07, 0x03, 0 };
    static const uint8_t laid_out[] = { 'S', 'a', 'l', 'u', 't', 0x0c, 0x18, 0x0a, 0x01, 0x0c, 0x08, 0x01, 0x07, 0x03, 0 };
    static const uint8_t other_jump[] = { 0x0c, 0x08, 0x07, 0x04, 0 };
    static const uint8_t no_number[] = { 'x', 0x07, 0x03, 0 };
    CHECK(oracles_text_same_commands(us, sizeof us, laid_out, sizeof laid_out));   /* colours, breaks, \stop and names aside */
    CHECK(!oracles_text_same_commands(us, sizeof us, other_jump, sizeof other_jump));
    CHECK(!oracles_text_same_commands(us, sizeof us, no_number, sizeof no_number));
    static const uint8_t lines[] = { 'a', 'b', 0x01, 0x0a, 0x00, 'c', 0x0c, 0x08, 'd', 0x09, 0x01, 'e', 0 };
    CHECK(oracles_text_longest_line(lines, sizeof lines) == 10);   /* \Link 5, \num1 2, three letters */
    /* The bytes after a \jump or a cmd8 are another text's: the engine never reads them as this one's. */
    static const uint8_t runs_on[] = { 'a', 0x07, 0x0f, 'W', 0x0c, 0x10, 'Y', 0x0c, 0x10, 'N', 0 }, ends[] = { 'b', 0x07, 0x0f, 0 };
    static const uint8_t shop[] = { 'x', 0x08, 0x0f, 0x0c, 0x08, 0 }, shop_ends[] = { 'y', 0x08, 0x0f, 0 };
    CHECK(oracles_text_same_commands(runs_on, sizeof runs_on, ends, sizeof ends));
    CHECK(oracles_text_same_commands(shop, sizeof shop, shop_ends, sizeof shop_ends));
    CHECK(!oracles_text_same_commands(runs_on, sizeof runs_on, shop_ends, sizeof shop_ends));
}

/* The tables of exceptions: both parse, pin their European ROM, and name what they name; a constructed table's
 * refusals. */
static void test_tables(void)
{
    char error[256];
    language_exception e;
    OraclesLanguage parsed;
    CHECK(oracles_language_parse("en-eu", &parsed) == 0 && parsed == ORACLES_LANGUAGE_ENGLISH_EU && !strcmp(oracles_language_code(parsed), "en-eu"));
    CHECK(oracles_language_parse("en", &parsed) == 0 && parsed == ORACLES_LANGUAGE_ENGLISH && oracles_language_parse("en-us", &parsed) == -1);
    CHECK(language_european_index(ORACLES_LANGUAGE_ENGLISH_EU) == 0 && language_european_index(ORACLES_LANGUAGE_FRENCH) == 1
          && language_european_index(ORACLES_LANGUAGE_SPANISH) == 4);
    language_table *ages = language_table_load(ORACLES_GAME_AGES, error, sizeof error);
    language_table *seasons = language_table_load(ORACLES_GAME_SEASONS, error, sizeof error);
    CHECK(ages && seasons);
    if (ages) {
        CHECK(!strcmp(language_table_rom(ages), "9f2dff3c46406839f839a8d535353dfc1e3e8670"));
        CHECK(language_table_find(ages, ORACLES_LANGUAGE_FRENCH, 0x0a53, &e) && e.source == ORACLES_LANGUAGE_SOURCE_MOVED && e.id == 0x0a51);
        CHECK(language_table_find(ages, ORACLES_LANGUAGE_GERMAN, 0x0e03, &e) && e.source == ORACLES_LANGUAGE_SOURCE_ADAPTED && e.rule == RULE_SHOP_PRICE);
        CHECK(!language_table_find(ages, ORACLES_LANGUAGE_FRENCH, 0x0e03, &e));
        CHECK(!language_table_find(ages, ORACLES_LANGUAGE_FRENCH, 0x0034, &e));
        CHECK(!language_table_find(ages, ORACLES_LANGUAGE_ITALIAN, 0x2f20, &e));   /* the Cukeman: the European hints */
        CHECK(language_table_find(ages, ORACLES_LANGUAGE_FRENCH, 0x2413, &e) && e.source == ORACLES_LANGUAGE_SOURCE_OWN && !strcmp(e.key, "rounds-cleared"));
        CHECK(!language_table_find(ages, ORACLES_LANGUAGE_GERMAN, 0x2413, &e));
        CHECK(language_table_find(ages, ORACLES_LANGUAGE_FRENCH, 0x4d28, &e) && e.source == ORACLES_LANGUAGE_SOURCE_OWN && !strcmp(e.key, "biggoron-forge"));
        CHECK(language_table_find(ages, ORACLES_LANGUAGE_FRENCH, 0x5516, &e) && e.source == ORACLES_LANGUAGE_SOURCE_OWN && !strcmp(e.key, "fairy-note"));
        CHECK(language_table_find(ages, ORACLES_LANGUAGE_ENGLISH_EU, 0x0034, &e) && e.source == ORACLES_LANGUAGE_SOURCE_ADAPTED && e.rule == RULE_NUMBER_AS_MORE);
        CHECK(language_table_find(ages, ORACLES_LANGUAGE_ENGLISH_EU, 0x302e, &e) && e.source == ORACLES_LANGUAGE_SOURCE_MOVED && e.id == 0x302f);   /* all: the five */
        CHECK(!language_table_find(ages, ORACLES_LANGUAGE_ENGLISH, 0x302e, &e));
        unsigned counts[ORACLES_LANGUAGE_SOURCE_COUNT];
        language_table_tally(ages, counts);
        CHECK(counts[ORACLES_LANGUAGE_SOURCE_KEPT] == 0 && counts[ORACLES_LANGUAGE_SOURCE_OWN] == 4);
    }
    if (seasons) {
        CHECK(!strcmp(language_table_rom(seasons), "360b9408e569ba826212eac1ebea1b09573cb48c"));
        CHECK(!language_table_find(seasons, ORACLES_LANGUAGE_SPANISH, 0x2f1a, &e));   /* the Cukeman: the European hints */
        CHECK(language_table_find(seasons, ORACLES_LANGUAGE_GERMAN, 0x0064, &e) && e.source == ORACLES_LANGUAGE_SOURCE_ADAPTED && e.rule == RULE_LEAD);
        CHECK(!language_table_find(seasons, ORACLES_LANGUAGE_FRENCH, 0x0064, &e));
        CHECK(language_table_find(seasons, ORACLES_LANGUAGE_FRENCH, 0x0400, &e) && !language_table_find(seasons, ORACLES_LANGUAGE_ENGLISH_EU, 0x0400, &e));
    }
    /* Every key a table names is translated in the languages it names it for. */
    for (unsigned g = 0; g < 2u; g++) {
        language_table *table = g ? seasons : ages;
        for (unsigned id = 0; table && id < 0x10000u; id++)
            for (OraclesLanguage l = ORACLES_LANGUAGE_ENGLISH_EU; l < ORACLES_LANGUAGE_COUNT; l++) {
                uint8_t text[256];
                size_t length = 0;
                if (language_table_find(table, l, id, &e) && e.source == ORACLES_LANGUAGE_SOURCE_OWN) CHECK(language_own_text(e.key, l, text, sizeof text, &length) == 0);
            }
    }
    language_table_free(ages);
    language_table_free(seasons);

    static const char *const refused[] = {
        "0001\tfr\teu:TX_0002\tmoved\n",                                                            /* no ROM pinned */
        "rom\t0123456789012345678901234567890123456789\n0001 0003-0005\tfr,de\teu:TX_0002\tmoved\n0004\tde\tenglish\tkept\n",   /* named twice */
        "rom\t0123456789012345678901234567890123456789\n0001\tfr\trule:nothing\twhy\n",           /* no such rule */
        "rom\t0123456789012345678901234567890123456789\n0001\ten\tenglish\twhy\n",                /* English (US) is the US ROM's */
        "rom\t0123456789012345678901234567890123456789\n0001\ten-us\tenglish\twhy\n",             /* no such language */
        "rom\t0123456789012345678901234567890123456789\n0001\tfr\teu:TX_02\twhy\n",               /* not an identifier */
        "rom\t0123456789012345678901234567890123456789\n0001\tfr\tenglish\n",                     /* no reason */
        "rom\t0123456789012345678901234567890123456789\n0005-0003\tfr\tenglish\twhy\n",           /* a range backwards */
    };
    for (size_t i = 0; i < sizeof refused / sizeof refused[0]; i++) {
        language_table *table = language_table_parse(refused[i], error, sizeof error);
        CHECK(table == NULL);
        language_table_free(table);
    }
    language_table *table = language_table_parse("# a comment\nrom\t0123456789012345678901234567890123456789\n0001 0003-0005\tfr,de\teu:TX_0002\tmoved\n0004\tit\tenglish\tkept\n0007\tall\town:key\tours\n0008\ten-eu,es\trule:lead\tadapted\n", error, sizeof error);
    CHECK(table != NULL);
    if (table) {
        CHECK(language_table_find(table, ORACLES_LANGUAGE_GERMAN, 0x0004, &e) && e.source == ORACLES_LANGUAGE_SOURCE_MOVED && e.id == 2);
        CHECK(language_table_find(table, ORACLES_LANGUAGE_ITALIAN, 0x0004, &e) && e.source == ORACLES_LANGUAGE_SOURCE_KEPT);
        CHECK(!language_table_find(table, ORACLES_LANGUAGE_SPANISH, 0x0004, &e) && !language_table_find(table, ORACLES_LANGUAGE_FRENCH, 0x0002, &e));
        CHECK(language_table_find(table, ORACLES_LANGUAGE_SPANISH, 0x0007, &e) && e.source == ORACLES_LANGUAGE_SOURCE_OWN && !strcmp(e.key, "key"));
        CHECK(language_table_find(table, ORACLES_LANGUAGE_ENGLISH_EU, 0x0007, &e) && !language_table_find(table, ORACLES_LANGUAGE_ENGLISH, 0x0007, &e));
        CHECK(language_table_find(table, ORACLES_LANGUAGE_ENGLISH_EU, 0x0008, &e) && e.rule == RULE_LEAD && !language_table_find(table, ORACLES_LANGUAGE_FRENCH, 0x0008, &e));
        unsigned counts[ORACLES_LANGUAGE_SOURCE_COUNT];
        language_table_tally(table, counts);
        CHECK(counts[ORACLES_LANGUAGE_SOURCE_MOVED] == 8 && counts[ORACLES_LANGUAGE_SOURCE_KEPT] == 1 && counts[ORACLES_LANGUAGE_SOURCE_OWN] == 5
              && counts[ORACLES_LANGUAGE_SOURCE_ADAPTED] == 2);
    }
    language_table_free(table);
}

/* A rule applied to a constructed European text: the text it makes, and whether that holds against the US one. */
static int adapted(language_rule rule, OraclesLanguage language, const uint8_t *us, size_t us_length, const uint8_t *text, size_t length,
                   const uint8_t *next, size_t next_length, const uint8_t *expected, size_t expected_length)
{
    uint8_t out[256];
    size_t n = 0;
    if (language_rule_apply(rule, language, us, us_length, text, length, next, next_length, out, sizeof out, &n) != 0) return 0;
    if (n != expected_length || memcmp(out, expected, n)) {
        fprintf(stderr, "rule %s made:", language_rule_name(rule));
        for (size_t i = 0; i < n; i++) fprintf(stderr, " %02x", out[i]);
        fprintf(stderr, "\n");
        return 0;
    }
    return language_rule_holds(rule, language, us, us_length, out, n);
}

#define ADAPTED(rule, language, us, text, expected) adapted(rule, language, us, sizeof us, text, sizeof text, NULL, 0, expected, sizeof expected)

static void test_rules(void)
{
    for (unsigned r = 0; r < RULE_COUNT; r++) {
        language_rule back;
        const char *name = language_rule_name((language_rule)r);
        CHECK(language_rule_parse(name, strlen(name), &back) == 0 && back == r);
    }
    language_rule none;
    CHECK(language_rule_parse("leads", 5, &none) == -1);

    /* lead: the box placed and the sound played as the US text does, first. */
    static const uint8_t lead_us[] = { 0x0c, 0x22, 0x0e, 0xc6, 'D', 'e', 'e', 0 };
    static const uint8_t lead_eu[] = { 0x0e, 0xc6, 0x0c, 0x22, 'D', 'i', 0 };
    static const uint8_t lead_out[] = { 0x0c, 0x22, 0x0e, 0xc6, 'D', 'i', 0 };
    CHECK(ADAPTED(RULE_LEAD, ORACLES_LANGUAGE_FRENCH, lead_us, lead_eu, lead_out));
    CHECK(!language_rule_check(RULE_LEAD, lead_us, sizeof lead_us, lead_eu, sizeof lead_eu));   /* the \pos second does nothing */
    static const uint8_t grease_us[] = { 'Y', 'o', 'u', 0x01, 0x0c, 0x22, 'f', 'o', 'r', 0 };    /* a \pos past the start does nothing */
    static const uint8_t grease_eu[] = { 0x0c, 0x22, 'D', 'u', 0x01, 'f', 0 }, grease_out[] = { 'D', 'u', 0x01, 'f', 0 };
    CHECK(ADAPTED(RULE_LEAD, ORACLES_LANGUAGE_GERMAN, grease_us, grease_eu, grease_out));

    /* end-at-pos: the next text's words cut away, with the line break before them. */
    static const uint8_t end_us[] = { 0x0c, 0x22, 'I', ' ', 'a', 'm', 0 };
    static const uint8_t end_eu[] = { 0x0c, 0x22, 'S', 'o', 'y', 0x01, 0x0c, 0x22, 'Q', 'u', 0 };
    static const uint8_t end_out[] = { 0x0c, 0x22, 'S', 'o', 'y', 0 };
    CHECK(ADAPTED(RULE_END_AT_POS, ORACLES_LANGUAGE_SPANISH, end_us, end_eu, end_out));
    CHECK(!language_rule_check(RULE_END_AT_POS, end_us, sizeof end_us, end_eu, sizeof end_eu));   /* the words after \pos are another text's */
    {
        uint8_t out[64];
        size_t n = 0;
        CHECK(language_rule_apply(RULE_END_AT_POS, ORACLES_LANGUAGE_SPANISH, end_us, sizeof end_us, end_out, sizeof end_out, NULL, 0, out, sizeof out, &n) == -1);
    }

    /* us-call: the US text's text called. */
    static const uint8_t call_us[] = { 0x0f, 0x18, 0x0c, 0x18, 0x01, 'A', 'h', 0 };
    static const uint8_t call_eu[] = { 0x0f, 0x14, 0x0c, 0x18, 0x01, 'A', 'c', 'h', 0 };
    static const uint8_t call_out[] = { 0x0f, 0x18, 0x0c, 0x18, 0x01, 'A', 'c', 'h', 0 };
    CHECK(ADAPTED(RULE_US_CALL, ORACLES_LANGUAGE_GERMAN, call_us, call_eu, call_out));

    /* shop-price: the printed price becomes \num1 (not the number in the item's name), the printed choice the US
     * text's \jump to it. */
    static const uint8_t shop_us[] = { 0x09, 0x01, '3', ' ', 'H', 0x09, 0x00, 0x0c, 0x18, 0x01, ' ', 0x0c, 0x08, ' ', 'R', 0x01, 0x07, 0x2b, 'Y', 0 };
    static const uint8_t shop_eu[] = { 0x09, 0x01, '3', ' ', 'H', 0x09, 0x00, 0x0c, 0x18, 0x01, 'M', ' ', '1', '0', ' ', 'R', '!', 0x01,
                                       ' ', ' ', 0x0c, 0x10, 'O', ' ', 0x0c, 0x10, 'N', 0x08, 0x0f, 0 };
    static const uint8_t shop_out[] = { 0x09, 0x01, '3', ' ', 'H', 0x09, 0x00, 0x0c, 0x18, 0x01, 'M', ' ', 0x0c, 0x08, ' ', 'R', '!', 0x01, 0x07, 0x2b, 0 };
    CHECK(ADAPTED(RULE_SHOP_PRICE, ORACLES_LANGUAGE_GERMAN, shop_us, shop_eu, shop_out));
    static const uint8_t mirror_us[] = { 'S', 0x0c, 0x18, 0x01, ' ', 0x0c, 0x08, ' ', 'R', 0x01, ' ', 0x0c, 0x10, 'O', ' ', 0x0c, 0x10, 'N', 0x08, 0x0f, 0 };
    static const uint8_t mirror_eu[] = { 'S', 0x0c, 0x18, 0x01, '8', '0', ' ', 'R', 0x01, ' ', 0x0c, 0x10, 'J', ' ', 0x0c, 0x10, 'N', 0x08, 0x0f, 0 };
    static const uint8_t mirror_out[] = { 'S', 0x0c, 0x18, 0x01, 0x0c, 0x08, ' ', 'R', 0x01, ' ', 0x0c, 0x10, 'J', ' ', 0x0c, 0x10, 'N', 0x08, 0x0f, 0 };
    CHECK(ADAPTED(RULE_SHOP_PRICE, ORACLES_LANGUAGE_GERMAN, mirror_us, mirror_eu, mirror_out));
    {
        /* A US text of more commands than are kept (300 \call, a \jump last): the rule does not apply. */
        static uint8_t many[2u * 301u + 1u];
        for (unsigned i = 0; i < 300u; i++) { many[2u * i] = 0x0f; many[2u * i + 1u] = 0x18; }
        many[600] = 0x07;
        many[601] = 0x2b;
        uint8_t made[256];
        size_t n = 0;
        CHECK(language_rule_apply(RULE_SHOP_PRICE, ORACLES_LANGUAGE_GERMAN, many, sizeof many, mirror_eu, sizeof mirror_eu, NULL, 0, made, sizeof made, &n) == -1);
    }

    /* join-next: the next identifier's text after a line break. */
    {
        static const uint8_t us[] = { 0x0e, 0xc7, 'A', 0x01, 0x0e, 0xc7, 'B', 0 }, eu[] = { 0x0e, 0xc7, 'a', 0 }, next[] = { 0x0e, 0xc7, 'b', 0 };
        static const uint8_t out[] = { 0x0e, 0xc7, 'a', 0x01, 0x0e, 0xc7, 'b', 0 };
        CHECK(adapted(RULE_JOIN_NEXT, ORACLES_LANGUAGE_ITALIAN, us, sizeof us, eu, sizeof eu, next, sizeof next, out, sizeof out));
        uint8_t made[64];
        size_t n = 0;
        CHECK(language_rule_apply(RULE_JOIN_NEXT, ORACLES_LANGUAGE_ITALIAN, us, sizeof us, eu, sizeof eu, NULL, 0, made, sizeof made, &n) == -1);
    }

    /* number-as-more: \num1 becomes "mehr", the line over sixteen breaks, and the hyphenated word joins again. */
    static const uint8_t more_us[] = { 'h', 'o', 'l', 'd', ' ', 'm', 'o', 'r', 'e', 0 };
    static const uint8_t more_eu[] = { 'n', 'u', 'n', 0x01, 0x0c, 0x08, ' ', 0x09, 0x01, 'R', 'i', 'n', 'g', 'e', 0x09, 0x00, ' ', 'a', 'u', 'f', 'b', 'e', '-', 0x01,
                                       'w', 'a', 'h', 'r', 'e', 'n', '!', 0 };
    static const uint8_t more_out[] = { 'n', 'u', 'n', 0x01, 'm', 'e', 'h', 'r', ' ', 0x09, 0x01, 'R', 'i', 'n', 'g', 'e', 0x09, 0x00, 0x01,
                                        'a', 'u', 'f', 'b', 'e', 'w', 'a', 'h', 'r', 'e', 'n', '!', 0 };
    CHECK(ADAPTED(RULE_NUMBER_AS_MORE, ORACLES_LANGUAGE_GERMAN, more_us, more_eu, more_out));
    static const uint8_t more_it[] = { 'P', 'e', 'r', ' ', 0x0c, 0x08, ' ', 'a', 0 }, more_it_out[] = { 'P', 'e', 'r', ' ', 'p', 'i', 0xae, ' ', 'a', 0 };
    CHECK(ADAPTED(RULE_NUMBER_AS_MORE, ORACLES_LANGUAGE_ITALIAN, more_us, more_it, more_it_out));

    /* option-numbers: the last choice's sum is the US code's. */
    static const uint8_t sum_us[] = { 'H', 0x01, ' ', 0x0c, 0x10, '1', '0', ' ', ' ', 0x0c, 0x10, '0', 0 };
    static const uint8_t sum_eu[] = { 'W', 0x01, ' ', 0x0c, 0x10, '1', '0', ' ', ' ', 0x0c, 0x10, '1', 0 };
    static const uint8_t sum_out[] = { 'W', 0x01, ' ', 0x0c, 0x10, '1', '0', ' ', ' ', 0x0c, 0x10, '0', 0 };
    CHECK(ADAPTED(RULE_OPTION_NUMBERS, ORACLES_LANGUAGE_FRENCH, sum_us, sum_eu, sum_out));

    /* unshown-number and symbols: the text as it is, the one difference allowed, no other. */
    static const uint8_t rounds_us[] = { 0x0e, 0xc7, 'Y', ' ', 0x0c, 0x08, 0x01, 'o', 'f', ' ', '8', 0 }, rounds_eu[] = { 0x0e, 0xc7, 'T', 'u', ' ', '8', 0 };
    CHECK(ADAPTED(RULE_UNSHOWN_NUMBER, ORACLES_LANGUAGE_FRENCH, rounds_us, rounds_eu, rounds_eu));
    CHECK(!language_rule_check(RULE_SYMBOLS, rounds_us, sizeof rounds_us, rounds_eu, sizeof rounds_eu));
    static const uint8_t song_us[] = { 0x06, 0x1c, 'U', 0x06, 0x1c, 0 }, song_eu[] = { 0x06, 0x1c, 'U', 0x06, 0x1c, 0x01, 'c', 0x06, 0x1c, 0 };
    CHECK(ADAPTED(RULE_SYMBOLS, ORACLES_LANGUAGE_SPANISH, song_us, song_eu, song_eu));
    CHECK(!language_rule_check(RULE_UNSHOWN_NUMBER, song_us, sizeof song_us, song_eu, sizeof song_eu));

    /* The checks of an adapted text: a line of seventeen, a line break before the end, a command the US text has not. */
    static const uint8_t plain[] = { 'a', 0 };
    static const uint8_t seventeen[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 0 };
    static const uint8_t trailing[] = { 'a', 0x01, 0 }, coloured[] = { 'a', 0x09, 0x01, 0 }, number[] = { 'a', 0x0c, 0x08, 0 };
    CHECK(language_rule_check(RULE_LEAD, plain, sizeof plain, plain, sizeof plain));
    CHECK(!language_rule_check(RULE_LEAD, plain, sizeof plain, seventeen, sizeof seventeen));
    CHECK(!language_rule_check(RULE_LEAD, plain, sizeof plain, trailing, sizeof trailing));
    CHECK(language_rule_check(RULE_LEAD, plain, sizeof plain, coloured, sizeof coloured));   /* the colour's parameter is no line break */
    CHECK(!language_rule_check(RULE_LEAD, plain, sizeof plain, number, sizeof number));
}

/* The corrections of the official translations: a word or a phrase replaced, whole words, in the texts and languages
 * listed; every row's words written in the font. */
extern const unsigned char oracles_language_corrections[];

/* language_correct with the table of `game`, read for the call. */
static int correct_in(OraclesGame game, OraclesLanguage language, unsigned id, uint8_t *text, size_t *length, size_t capacity)
{
    char error[128];
    language_corrections *corrections = language_corrections_load(game, error, sizeof error);
    CHECK(corrections != NULL);
    const int corrected = corrections ? language_correct(corrections, language, id, text, length, capacity) : -2;
    language_corrections_free(corrections);
    return corrected;
}

/* Whether text `before` of `id` becomes `after` (string literals, which may hold a zero). */
#define CORRECTED(game, language, id, before, after) corrected(game, language, id, before, sizeof before - 1u, after, sizeof after - 1u)
static int corrected(OraclesGame game, OraclesLanguage language, unsigned id, const char *before, size_t before_length, const char *after, size_t after_length)
{
    uint8_t text[128];
    size_t length = before_length;
    memcpy(text, before, length);
    const int changed = before_length != after_length || memcmp(before, after, after_length);
    return correct_in(game, language, id, text, &length, sizeof text) == changed && length == after_length && !memcmp(text, after, length);
}

static void test_corrections(void)
{
    uint8_t text[64];
    size_t length;
    static const char spanish[] = "parar a Onox.\x01Onoxa";
    memcpy(text, spanish, sizeof spanish - 1u);
    length = sizeof spanish - 1u;
    CHECK(correct_in(ORACLES_GAME_AGES, ORACLES_LANGUAGE_SPANISH, 0x2f11, text, &length, sizeof text) == 1);
    CHECK(length == sizeof spanish && !memcmp(text, "parar a Veran.\x01Onoxa", length));   /* a whole word only */
    memcpy(text, spanish, sizeof spanish - 1u);
    length = sizeof spanish - 1u;
    CHECK(correct_in(ORACLES_GAME_AGES, ORACLES_LANGUAGE_FRENCH, 0x2f11, text, &length, sizeof text) == 0);   /* not French */
    CHECK(correct_in(ORACLES_GAME_AGES, ORACLES_LANGUAGE_SPANISH, 0x2f13, text, &length, sizeof text) == 0);  /* nor that text */
    CHECK(correct_in(ORACLES_GAME_SEASONS, ORACLES_LANGUAGE_SPANISH, 0x2f11, text, &length, sizeof text) == 0); /* nor Seasons */
    CHECK(correct_in(ORACLES_GAME_AGES, ORACLES_LANGUAGE_SPANISH, 0x2f11, text, &length, 16) == -1);           /* no room */
    /* A word right after a control code whose parameter is a letter's byte. */
    CHECK(CORRECTED(ORACLES_GAME_AGES, ORACLES_LANGUAGE_SPANISH, 0x2f10, "\x0e\xc7Onox!", "\x0e\xc7Veran!"));
    /* A phrase over a colour, its correction breaking the line again; the accents in the font's bytes. */
    CHECK(CORRECTED(ORACLES_GAME_AGES, ORACLES_LANGUAGE_FRENCH, 0x1e04, "de\x01\x09\x01Graines Myst\xa5re\x09\x00,", "de\x01\x09\x01Graines\x01Mystiques\x09\x00,"));
    CHECK(CORRECTED(ORACLES_GAME_AGES, ORACLES_LANGUAGE_FRENCH, 0x1e06, "Graines\x01Myst\xa5re!", "Graines\x01Mystiques!"));
    CHECK(CORRECTED(ORACLES_GAME_AGES, ORACLES_LANGUAGE_FRENCH, 0x05b4, "Tu as le\x01\x09\x02" "Cri de l'Echo\x09\x04!", "Tu as\x01l'\x09\x02" "Echo du\x01Hurlement\x09\x04!"));
    /* A phrase that starts with no letter; another that leaves every other lone "o". */
    CHECK(CORRECTED(ORACLES_GAME_AGES, ORACLES_LANGUAGE_FRENCH, 0x541e, "1/2 d\xa6g\xa1ts par", "Aucun d\xa6g\xa1t par"));
    CHECK(CORRECTED(ORACLES_GAME_SEASONS, ORACLES_LANGUAGE_SPANISH, 0x3026, "o tu b\xd5squeda o\x01has o", "o tu b\xd5squeda ni\x01has o"));
    /* A phrase that ends on a colour: a word before it, none needed after it. */
    CHECK(CORRECTED(ORACLES_GAME_AGES, ORACLES_LANGUAGE_FRENCH, 0x280c, "en Labrynna\x09\x04!", "en Labrynna\x09\x00!"));
    CHECK(CORRECTED(ORACLES_GAME_AGES, ORACLES_LANGUAGE_FRENCH, 0x280c, "SLabrynna\x09\x04!", "SLabrynna\x09\x04!"));
    CHECK(CORRECTED(ORACLES_GAME_AGES, ORACLES_LANGUAGE_ENGLISH_EU, 0x541e, "1/2 damage from\x01" "electricity", "No damage from\x01" "electricity"));
    CHECK(CORRECTED(ORACLES_GAME_AGES, ORACLES_LANGUAGE_ENGLISH, 0x541e, "1/2 damage from\x01" "electricity", "1/2 damage from\x01" "electricity"));   /* English (US) untouched */
    /* Every row: six fields, both words in the font. */
    unsigned rows = 0;
    for (const char *line = (const char *)oracles_language_corrections; *line;) {
        const char *end = strchr(line, '\n');
        if (!end) end = line + strlen(line);
        if (*line != '#' && end > line) {
            const char *field[6] = { 0 };
            size_t lengths[6] = { 0 };
            unsigned n = 0;
            for (const char *at = line; n < 6u && at <= end; n++) {
                const char *tab = memchr(at, '\t', (size_t)(end - at));
                field[n] = at;
                lengths[n] = (size_t)((tab ? tab : end) - at);
                at = tab ? tab + 1 : end + 1;
            }
            uint8_t out[128];
            size_t out_length = 0;
            const int good = n == 6u && lengths[3] && lengths[4] && language_text_encode(field[3], lengths[3], out, sizeof out, &out_length) == 0
                             && language_text_encode(field[4], lengths[4], out, sizeof out, &out_length) == 0;
            if (!good) { fprintf(stderr, "correction %.*s is not a row\n", (int)(end - line), line); failures++; }
            rows++;
        }
        line = *end ? end + 1 : end;
    }
    CHECK(rows == 92);
}

/* The port's own texts: every row encodes in the font, fits its lines, holds the commands of the US text it stands for
 * and no other; and the syntax on constructed texts. */
static void test_own(void)
{
    const char *key, *text;
    size_t key_length, text_length;
    OraclesLanguage language;
    unsigned rows = 0;
    /* The US text's commands: the rounds' sound and count, the forge's two choices, the note's title; the Goron fortune
     * teller's openings none, each fortune the call of its opening (a key that starts with its own and goes on). */
    static const uint8_t rounds[] = { 0x0e, 0xc7, 0x0c, 0x08, 0 }, forge[] = { 0x0c, 0x10, 0x0c, 0x10, 0 }, note[] = { 0x0c, 0x38, 0 };
    static const uint8_t opening[] = { 0 }, future[] = { 0x0f, 0x42, 0 }, past[] = { 0x0f, 0x4e, 0 };
    static const struct { const char *key; const uint8_t *commands; size_t length; int prefix; } us[] = {
        { "rounds-cleared", rounds, sizeof rounds, 0 }, { "biggoron-forge", forge, sizeof forge, 0 }, { "fairy-note", note, sizeof note, 0 },
        { "fortune-future", opening, sizeof opening, 0 }, { "fortune-past", opening, sizeof opening, 0 },
        { "fortune-future-", future, sizeof future, 1 }, { "fortune-past-", past, sizeof past, 1 },
    };
    unsigned japanese = 0;
    for (unsigned n = 0; language_own_nth(n, &key, &key_length, &language, &text, &text_length) == 0; n++) {
        uint8_t out[256];
        size_t length = 0;
        const int encoded = language_text_encode_in(language, text, text_length, out, sizeof out, &length) == 0;
        CHECK(encoded && language != ORACLES_LANGUAGE_ENGLISH);
        if (!encoded) { fprintf(stderr, "own text %.*s %s does not encode\n", (int)key_length, key, oracles_language_code(language)); continue; }
        size_t k = 0;
        while (k < sizeof us / sizeof us[0]
               && !(us[k].prefix ? key_length > strlen(us[k].key) && !memcmp(key, us[k].key, strlen(us[k].key)) : key_length == strlen(us[k].key) && !memcmp(key, us[k].key, key_length)))
            k++;
        if (k == sizeof us / sizeof us[0] || !language_text_fits(out, length) || !language_same_commands(language, us[k].commands, us[k].length, out, length)) {
            fprintf(stderr, "own text %.*s %s: lines over sixteen or commands\n", (int)key_length, key, oracles_language_code(language));
            failures++;
        }
        japanese += language == ORACLES_LANGUAGE_JAPANESE;
        rows++;
    }
    CHECK(rows == 27 && japanese == 24);
    uint8_t out[256];
    size_t length = 0;
    static const uint8_t accents[] = { 'G', 'r', 0xb0, 0x91, 'e', 0x01, 0x98, 'S', 0xd3, '!', 0x0c, 0x18, 0x01, 0x09, 0x03, 'a', 0x09, 0x00, '\\', 0xb6, 0 };
    CHECK(language_text_encode("Grüße\\n¡Sí!\\stop\\n\\col(03)a\\col(00)\\\\\\xb6", strlen("Grüße\\n¡Sí!\\stop\\n\\col(03)a\\col(00)\\\\\\xb6"), out, sizeof out, &length) == 0
          && length == sizeof accents && !memcmp(out, accents, length));
    CHECK(language_text_encode("€", strlen("€"), out, sizeof out, &length) == -1);       /* not in the font */
    CHECK(language_text_encode("a\\q", 3, out, sizeof out, &length) == -1);            /* no such escape */
    CHECK(language_text_encode("a\tb", 3, out, sizeof out, &length) == -1);            /* a control character */
    CHECK(language_text_encode("\xc3", 1, out, sizeof out, &length) == -1);            /* a cut UTF-8 sequence */
    CHECK(language_own_text("rounds-cleared", ORACLES_LANGUAGE_FRENCH, out, sizeof out, &length) == 0 && length > 2 && out[0] == 0x0e && out[1] == 0xc7);
    CHECK(language_own_text("rounds-cleared", ORACLES_LANGUAGE_GERMAN, out, sizeof out, &length) == -1);
    CHECK(language_own_text("nothing", ORACLES_LANGUAGE_GERMAN, out, sizeof out, &length) == -1);

    /* Japanese: the kana, the full-width letters and marks (an ASCII one standing for its full-width form), the kanji
     * of the kanji font as a \sym, the escapes as the European syntax has them; no lower-case letter, no accent, no
     * kanji the font has not. */
    static const uint8_t kana[] = { 0x60, 0xb0, 0x93, 0xa4, 0x20, 0x20, 0x21, 0x3f, 0x41, 0x31, 0x2d, 0x2d, 0x5f, 0x1e, 0x1b, 0x1c,
                                    0x06, 0x35, 0x06, 0x4d, 0x01, 0x0c, 0x18, 0x09, 0x01, 0x0f, 0x42, 0 };
    const char *written = "あアっで　 ！?Ａ1ー－・、「」剣時\\n\\stop\\col(01)\\x0f\\x42";
    CHECK(language_text_encode_in(ORACLES_LANGUAGE_JAPANESE, written, strlen(written), out, sizeof out, &length) == 0 && length == sizeof kana
          && !memcmp(out, kana, length));
    CHECK(language_text_encode_in(ORACLES_LANGUAGE_JAPANESE, "a", 1, out, sizeof out, &length) == -1);              /* lower case: kana there */
    CHECK(language_text_encode_in(ORACLES_LANGUAGE_JAPANESE, "\xc3\xbc", 2, out, sizeof out, &length) == -1);   /* no accent */
    CHECK(language_text_encode_in(ORACLES_LANGUAGE_JAPANESE, "漢", strlen("漢"), out, sizeof out, &length) == -1);  /* not a kanji of the font */
    CHECK(language_text_encode_in(ORACLES_LANGUAGE_FRENCH, "あ", strlen("あ"), out, sizeof out, &length) == -1);     /* no kana in the US font */
    CHECK(language_own_text("fortune-future", ORACLES_LANGUAGE_JAPANESE, out, sizeof out, &length) == 0 && out[0] == 0xb6);   /* キ */
    CHECK(language_own_text("fortune-future", ORACLES_LANGUAGE_FRENCH, out, sizeof out, &length) == -1);
}

static OraclesLanguagePack *sample_pack(void)
{
    OraclesLanguagePack *pack = language_pack_new(ORACLES_GAME_AGES, "9f2dff3c46406839f839a8d535353dfc1e3e8670", "880374fb978b18af4aa529e2e32f7ffb4d7dd2f4");
    static const uint8_t same[] = { 'B', 'o', 'n', 'j', 'o', 'u', 'r', 0x01, 0x0c, 0x18, 'x', 0 };
    static const uint8_t moved[] = { 0x0c, 0x08, 0x07, 0x09, 0 };   /* its \jump is not the US text's */
    static const uint8_t ring[] = { 'R', 0x0c, 0x08, 0 };
    static const uint8_t accents[] = { 0xd3, 0xc8, 0 };
    CHECK(language_pack_add(pack, ORACLES_LANGUAGE_FRENCH, 0x0000, ORACLES_LANGUAGE_SOURCE_SAME, same, sizeof same) == 0);
    CHECK(language_pack_add(pack, ORACLES_LANGUAGE_FRENCH, 0x0001, ORACLES_LANGUAGE_SOURCE_REFUSED, NULL, 0) == 0);
    CHECK(language_pack_add(pack, ORACLES_LANGUAGE_FRENCH, 0x0034, ORACLES_LANGUAGE_SOURCE_KEPT, NULL, 0) == 0);
    CHECK(language_pack_add(pack, ORACLES_LANGUAGE_FRENCH, 0x0100, ORACLES_LANGUAGE_SOURCE_ADAPTED, accents, sizeof accents) == 0);
    CHECK(language_pack_add(pack, ORACLES_LANGUAGE_FRENCH, 0x0101, ORACLES_LANGUAGE_SOURCE_OWN, same, sizeof same) == 0);   /* identical: stored once */
    CHECK(language_pack_add(pack, ORACLES_LANGUAGE_FRENCH, 0x0100, ORACLES_LANGUAGE_SOURCE_SAME, same, sizeof same) == -1);  /* identifiers increase */
    CHECK(language_pack_add(pack, ORACLES_LANGUAGE_FRENCH, 0x0102, ORACLES_LANGUAGE_SOURCE_KEPT, moved, sizeof moved) == -1);  /* kept: no text */
    CHECK(language_pack_add(pack, ORACLES_LANGUAGE_FRENCH, 0x0102, ORACLES_LANGUAGE_SOURCE_MOVED, ring, 0) == -1);            /* moved: a text */
    CHECK(language_pack_add(pack, ORACLES_LANGUAGE_GERMAN, 0x0000, ORACLES_LANGUAGE_SOURCE_SAME, same, sizeof same) == 0);
    CHECK(language_pack_add(pack, ORACLES_LANGUAGE_ENGLISH_EU, 0x0000, ORACLES_LANGUAGE_SOURCE_SAME, ring, sizeof ring) == 0);
    CHECK(language_pack_add(pack, ORACLES_LANGUAGE_ENGLISH, 0x0000, ORACLES_LANGUAGE_SOURCE_SAME, ring, sizeof ring) == -1);   /* the US ROM's own */
    pack->glyph_count = 2;
    pack->characters[0] = 0xc8;
    pack->characters[1] = 0x33;
    for (unsigned i = 0; i < 2u * GLYPH_BYTES; i++) pack->bitmaps[i] = (uint8_t)(0xa0 + i);
    return pack;
}

static void test_pack_file(const char *dir)
{
    OraclesLanguagePack *pack = sample_pack();
    char path[512], error[256];
    snprintf(path, sizeof path, "%s/ages.pack", dir);
    CHECK(oracles_language_pack_write(pack, path, error, sizeof error) == 0);
    OraclesLanguagePack *back = oracles_language_pack_read(path, error, sizeof error);
    CHECK(back != NULL);
    if (back) {
        CHECK(oracles_language_pack_game(back) == ORACLES_GAME_AGES);
        CHECK(!strcmp(oracles_language_pack_source(back), oracles_language_pack_source(pack)));
        CHECK(oracles_language_pack_count(back, ORACLES_LANGUAGE_FRENCH) == 5 && oracles_language_pack_count(back, ORACLES_LANGUAGE_GERMAN) == 1);
        CHECK(oracles_language_pack_count(back, ORACLES_LANGUAGE_ITALIAN) == 0 && oracles_language_pack_count(back, ORACLES_LANGUAGE_ENGLISH_EU) == 1);
        size_t a = 0, b = 0;
        const uint8_t *x = oracles_language_pack_text(pack, ORACLES_LANGUAGE_FRENCH, 0x0100, &a), *y = oracles_language_pack_text(back, ORACLES_LANGUAGE_FRENCH, 0x0100, &b);
        CHECK(x && y && a == b && !memcmp(x, y, a));
        CHECK(!oracles_language_pack_text(back, ORACLES_LANGUAGE_FRENCH, 0x0035, &a));
        CHECK(!oracles_language_pack_text(back, ORACLES_LANGUAGE_FRENCH, 0x0034, &a));
        CHECK(oracles_language_pack_source_of(back, ORACLES_LANGUAGE_FRENCH, 0x0034) == ORACLES_LANGUAGE_SOURCE_KEPT);
        CHECK(oracles_language_pack_source_of(back, ORACLES_LANGUAGE_FRENCH, 0x0101) == ORACLES_LANGUAGE_SOURCE_OWN);
        CHECK(oracles_language_pack_source_of(back, ORACLES_LANGUAGE_FRENCH, 0x0035) == ORACLES_LANGUAGE_SOURCE_MISSING);
        unsigned counts[ORACLES_LANGUAGE_SOURCE_COUNT];
        oracles_language_pack_tally(back, ORACLES_LANGUAGE_FRENCH, counts);
        CHECK(counts[ORACLES_LANGUAGE_SOURCE_SAME] == 1 && counts[ORACLES_LANGUAGE_SOURCE_ADAPTED] == 1 && counts[ORACLES_LANGUAGE_SOURCE_OWN] == 1
              && counts[ORACLES_LANGUAGE_SOURCE_KEPT] == 1 && counts[ORACLES_LANGUAGE_SOURCE_REFUSED] == 1 && counts[ORACLES_LANGUAGE_SOURCE_MOVED] == 0);
        const uint8_t *characters, *bitmaps;
        CHECK(oracles_language_pack_glyphs(back, &characters, &bitmaps) == 2 && characters[1] == 0x33 && bitmaps[31] == 0xbf);
        oracles_language_pack_free(back);
    }
    /* Another format version, a damaged file, an unknown source: refused. */
    FILE *f = fopen(path, "r+b");
    if (f) { fseek(f, 16, SEEK_SET); fputc(9, f); fclose(f); }
    CHECK(oracles_language_pack_read(path, error, sizeof error) == NULL && strstr(error, "format 9"));
    snprintf(pack->source, sizeof pack->source, "%s", "0000000000000000000000000000000000000000");
    CHECK(oracles_language_pack_write(pack, path, error, sizeof error) == 0);
    CHECK(oracles_language_pack_read(path, error, sizeof error) == NULL);
    snprintf(pack->source, sizeof pack->source, "%s", "9f2dff3c46406839f839a8d535353dfc1e3e8670");
    /* A byte changed or cut away: its checksum refuses it. */
    CHECK(oracles_language_pack_write(pack, path, error, sizeof error) == 0);
    f = fopen(path, "r+b");
    if (f) { fseek(f, 200, SEEK_SET); fputc(0x55, f); fclose(f); }
    CHECK(oracles_language_pack_read(path, error, sizeof error) == NULL && strstr(error, "checksum"));
    size_t size = 0;
    uint8_t *data = oracles_rom_read_file(path, &size, error, sizeof error);
    CHECK(oracles_language_pack_write(pack, path, error, sizeof error) == 0);
    f = data ? fopen(path, "wb") : NULL;
    if (f) { fwrite(data, 1, size - 41u, f); fclose(f); }
    free(data);
    CHECK(oracles_language_pack_read(path, error, sizeof error) == NULL && strstr(error, "checksum"));
    /* Made with other tables or another revision of the code: its fingerprint refuses it. */
    char fingerprint[41];
    snprintf(fingerprint, sizeof fingerprint, "%s", pack->fingerprint);
    pack->fingerprint[0] = pack->fingerprint[0] == '0' ? '1' : '0';
    CHECK(oracles_language_pack_write(pack, path, error, sizeof error) == 0);
    CHECK(oracles_language_pack_read(path, error, sizeof error) == NULL && strstr(error, "another version"));
    snprintf(pack->fingerprint, sizeof pack->fingerprint, "%s", fingerprint);
    CHECK(oracles_language_pack_write(pack, path, error, sizeof error) == 0);
    OraclesLanguagePack *again = oracles_language_pack_read(path, error, sizeof error);
    CHECK(again != NULL);
    oracles_language_pack_free(again);
    oracles_language_pack_free(pack);
    remove(path);
}

extern const unsigned char oracles_language_exceptions_ages[];
extern const unsigned char oracles_language_exceptions_seasons[];
extern const unsigned char oracles_language_own_texts[];
extern const unsigned char oracles_language_exceptions_ages_ja[];
extern const unsigned char oracles_language_own_texts_ja[];
extern const unsigned char oracles_language_exceptions_ages_pl[];
extern const unsigned char oracles_language_exceptions_seasons_pl[];
extern const unsigned char oracles_language_corrections_pl[];

/* The fingerprint covers every table a pack is made from, the code's revision and the text tables of the image: a
 * byte changed in any of them changes it. */
static void test_fingerprint(void)
{
    language_target ages, seasons, moonrise;
    CHECK(language_target_of("880374fb978b18af4aa529e2e32f7ffb4d7dd2f4", &ages) == 0);
    CHECK(language_target_of("ba1268290fb2b1b70505d2d7b5825fc8a4816a4b", &seasons) == 0);
    CHECK(language_target_of("013cf1fd3149f87990cf341f2973f1d11a28e55f", &moonrise) == 0 && !moonrise.original);
    const unsigned char *tables[LANGUAGE_FINGERPRINT_TABLES];
    CHECK(language_fingerprint_tables(ORACLES_GAME_AGES, tables) == 3u && tables[0] == oracles_language_exceptions_ages
          && tables[1] == oracles_language_own_texts && tables[2] == oracles_language_corrections);
    CHECK(language_fingerprint_tables(ORACLES_GAME_SEASONS, tables) == 3u && tables[0] == oracles_language_exceptions_seasons);
    char base[41], other[41];
    language_fingerprint(&ages, EDITION_EUROPEAN, base);
    language_fingerprint_tables(ORACLES_GAME_AGES, tables);
    language_fingerprint_of(ORACLES_LANGUAGE_PACK_REVISION, tables, 3u, &ages, other);
    CHECK(!strcmp(base, other) && strlen(base) == 40u);
    language_fingerprint_of(ORACLES_LANGUAGE_PACK_REVISION + 1u, tables, 3u, &ages, other);
    CHECK(strcmp(base, other) != 0);
    language_fingerprint(&moonrise, EDITION_EUROPEAN, other);
    CHECK(strcmp(base, other) != 0);   /* a fan game's text tables */
    language_fingerprint(&seasons, EDITION_EUROPEAN, other);
    CHECK(strcmp(base, other) != 0);
    /* A pack of the Japanese ROM: its own tables, which the European packs' fingerprint does not cover. */
    CHECK(language_fingerprint_tables_japanese(ORACLES_GAME_AGES, tables) == 2u && tables[0] == oracles_language_exceptions_ages_ja
          && tables[1] == oracles_language_own_texts_ja);
    language_fingerprint(&ages, EDITION_JAPANESE, other);
    CHECK(strcmp(base, other) != 0);
    /* Its own revision: the Japanese extraction raised alone leaves the European packs' fingerprint as it is. */
    char japanese[41];
    language_fingerprint_tables_japanese(ORACLES_GAME_AGES, tables);
    language_fingerprint_of(ORACLES_LANGUAGE_PACK_REVISION_JA, tables, 2u, &ages, japanese);
    CHECK(!strcmp(japanese, other));
    language_fingerprint_of(ORACLES_LANGUAGE_PACK_REVISION_JA + 1u, tables, 2u, &ages, japanese);
    CHECK(strcmp(japanese, other) != 0);
    /* A pack of the Polish ROM: its own tables and revision too, neither the European packs' nor the Japanese one's. */
    CHECK(language_fingerprint_tables_polish(ORACLES_GAME_AGES, tables) == 2u && tables[0] == oracles_language_exceptions_ages_pl
          && tables[1] == oracles_language_corrections_pl);
    CHECK(language_fingerprint_tables_polish(ORACLES_GAME_SEASONS, tables) == 2u && tables[0] == oracles_language_exceptions_seasons_pl);
    char polish[41], polish_again[41];
    language_fingerprint(&ages, EDITION_POLISH, polish);
    CHECK(strcmp(polish, base) != 0 && strcmp(polish, other) != 0);
    language_fingerprint_tables_polish(ORACLES_GAME_AGES, tables);
    language_fingerprint_of(ORACLES_LANGUAGE_PACK_REVISION_PL, tables, 2u, &ages, polish_again);
    CHECK(!strcmp(polish, polish_again));
    language_fingerprint_of(ORACLES_LANGUAGE_PACK_REVISION_PL + 1u, tables, 2u, &ages, polish_again);
    CHECK(strcmp(polish, polish_again) != 0);
    language_fingerprint_tables(ORACLES_GAME_AGES, tables);
    for (unsigned t = 0; t < 3u; t++) {
        const size_t length = strlen((const char *)tables[t]);
        unsigned char *copy = malloc(length + 1u);
        if (!copy) continue;
        memcpy(copy, tables[t], length + 1u);
        copy[length / 2u] ^= 0x01u;   /* one byte of the middle of the table */
        const unsigned char *changed[3] = { tables[0], tables[1], tables[2] };
        changed[t] = copy;
        language_fingerprint_of(ORACLES_LANGUAGE_PACK_REVISION, changed, 3u, &ages, other);
        CHECK(strcmp(base, other) != 0);
        free(copy);
    }
}

/* A synthetic Ages US image: its text tables where the generated tables say (six internal groups: TX_00xx, 0x40
 * texts, and TX_01xx, two), the first base after the last list, an accent's slot of the font filled; a cartridge
 * header, and at $0100 a jump to $0150. */
static uint8_t *us_image(void)
{
    uint8_t *rom = calloc(MIB, 1);
    memcpy(rom + 0x134, "ZELDA NAYRU", 11);
    rom[0x143] = 0xc0; rom[0x147] = 0x1b; rom[0x148] = 0x05; rom[0x149] = 0x02;
    rom[0x100] = 0x00; rom[0x101] = 0xc3; rom[0x102] = 0x50; rom[0x103] = 0x01;
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    const unsigned groups = 6, g0 = 2u * groups, g4 = g0 + 2u, g5 = g4 + 2u * 0x40u, base = g5 + 4u;   /* offsets in bank $1d; one fragment */
    const size_t table = 0x1du * 0x4000u;
    const size_t pointers = oracles_rom_offset(t->text_table_table.bank, t->text_table_table.addr);
    rom[pointers] = 0x00; rom[pointers + 1] = 0x40; rom[pointers + 2] = 0x1d;   /* Pointer3Byte: $4000, bank $1d */
    const size_t bases[2] = { oracles_rom_offset(t->text_offset1_table.bank, t->text_offset1_table.addr),
                              oracles_rom_offset(t->text_offset2_table.bank, t->text_offset2_table.addr) };
    for (unsigned b = 0; b < 2; b++) { rom[bases[b]] = 0x1d; put16(rom + bases[b] + 1, base); }   /* bank, offset in the bank */
    for (unsigned g = 0; g < groups; g++) put16(rom + table + 2u * g, g == 5 ? g5 : g == 4 ? g4 : g0);
    put16(rom + table + g0, 0x900);
    for (unsigned i = 0; i < 0x40u; i++) put16(rom + table + g4 + 2u * i, 0x10u * i);
    put16(rom + table + g5, 0x800); put16(rom + table + g5 + 2u, 0x810);
    static const uint8_t hello[] = { 'H', 'e', 'l', 'l', 'o', 0x01, 'x', 0 }, jump[] = { 0x0c, 0x08, 0x07, 0x03, 0 };
    for (unsigned i = 0; i < 0x40u; i++) memcpy(rom + table + base + 0x10u * i, i == 1 ? jump : hello, i == 1 ? sizeof jump : sizeof hello);
    memcpy(rom + table + base + 0x800, hello, sizeof hello);
    memcpy(rom + table + base + 0x810, hello, sizeof hello);
    rom[oracles_rom_offset(t->font_start.bank, t->font_start.addr) + 0xc8u * 16u] = 0x55;
    return rom;
}

/* The composition: TX_0000 served, TX_0001 kept (refused), TX_0034 kept (by the table), the others of group 4 absent
 * from the pack, TX_0100 served, TX_0101 the same bytes as TX_0000, stored once. */
static uint8_t *test_compose(void)
{
    uint8_t *rom = us_image();
    OraclesLanguagePack *pack = sample_pack();
    uint8_t *image = NULL, *again = NULL;
    size_t size = 0, other = 0;
    OraclesLanguageReport report;
    char error[256];
    CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_FRENCH, rom, MIB, NULL, &image, &size, &report, error, sizeof error) == 0);
    CHECK(size == 2u * MIB);
    CHECK(report.served == 3 && report.english == 2 && report.missing == 0x40 - 3);
    CHECK(report.bytes == 12u + 3u);
    if (image) {
        const OraclesGuestTables *t = &oracles_guest_tables_ages;
        const uint8_t *index = image + ORACLES_IMAGE_LANGUAGE_BANK * 0x4000u;
        CHECK(!memcmp(index, ORACLES_LANGUAGE_INDEX_MAGIC, 16) && index[16] == 2 && index[17] == 6 && !memcmp(index + 18, "fr\0\0\0\0\0\0", 8));
        const uint8_t *g4 = index + ORACLES_LANGUAGE_INDEX_GROUPS + 16u, *g5 = g4 + 4u;
        CHECK((g4[2] | g4[3] << 8) == 0x40 && (g5[2] | g5[3] << 8) == 2);
        const uint8_t *e0 = index + (g4[0] | g4[1] << 8), *e1 = e0 + 3, *e2 = e0 + 6, *e34 = e0 + 3u * 0x34u;
        CHECK(e1[0] == 0 && (e1[1] | e1[2] << 8) == ORACLES_LANGUAGE_KEPT);
        CHECK(e34[0] == 0 && (e34[1] | e34[2] << 8) == ORACLES_LANGUAGE_KEPT);
        CHECK(e2[0] == 0 && e2[1] == 0 && e2[2] == 0);
        CHECK(e0[0] == ORACLES_IMAGE_LANGUAGE_BANK && e0[2] >= 0x40 && e0[2] < 0x80);
        CHECK(!memcmp(image + (size_t)e0[0] * 0x4000u + ((e0[1] | e0[2] << 8) - 0x4000u), "Bonjour", 7));
        CHECK(!memcmp(index + (g5[0] | g5[1] << 8) + 3, e0, 3));
        const size_t font = oracles_rom_offset(t->font_start.bank, t->font_start.addr);
        CHECK(image[font + 0xc8u * 16u] == 0xa0 && image[font + 0x33u * 16u + 15u] == 0xbf);
        CHECK(image[0x148] == 0x06);
        CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_GERMAN, image, size, NULL, &again, &other, &report, error, sizeof error) == -1);   /* its banks are taken */
    }
    CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_ENGLISH_EU, rom, MIB, NULL, &again, &other, &report, error, sizeof error) == 0);
    CHECK(again && !memcmp(again + ORACLES_IMAGE_LANGUAGE_BANK * 0x4000u + 18u, "en-eu\0\0\0", 8) && report.served == 1);
    free(again);
    again = NULL;
    CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_ENGLISH, rom, MIB, NULL, &again, &other, &report, error, sizeof error) == -1);
    CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_FRENCH, rom, MIB / 2u, NULL, &again, &other, &report, error, sizeof error) == -1);
    free(again);
    free(rom);
    oracles_language_pack_free(pack);
    return image;
}

/* English (US) laid out again for the compact text, on the synthetic image: a font of letters five pixels wide and a
 * blank space, TX_0002 four lines that one holds (two boxes for one), the others a single box each, kept in English. */
static void test_compose_layout(void)
{
    uint8_t *rom = us_image();
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    const size_t font = oracles_rom_offset(t->font_start.bank, t->font_start.addr), table = 0x1du * 0x4000u;
    for (unsigned c = 0x10; c < 0x100u; c++) memset(rom + font + 16u * c, c == ' ' ? 0xff : 0x83, 16);
    static const uint8_t lines[] = { 'H', 'i', 0x01, 'y', 'o', 'u', 0x01, 'a', 'l', 'l', 0x01, 't', 'h', 'r', 'e', 'e', 0 };
    const unsigned g4 = 2u * 6u + 2u, base = g4 + 2u * 0x40u + 4u;
    memcpy(rom + table + base + 0xa00u, lines, sizeof lines);
    put16(rom + table + g4 + 2u * 2u, 0xa00u);
    const OraclesLanguageLayout layout = { 128, 0, 0, NULL };
    uint8_t *image = NULL;
    size_t size = 0;
    OraclesLanguageReport report;
    char error[256];
    const char *ages = "880374fb978b18af4aa529e2e32f7ffb4d7dd2f4";
    CHECK(oracles_language_compose_english(ages, rom, MIB, &layout, &image, &size, &report, error, sizeof error) == 0 && size == 2u * MIB);
    CHECK(report.laid_out == 1 && report.served == 1 && report.layout_failed == 0 && report.layout_kept == 1);   /* TX_0003, which TX_0001 jumps to within a line */
    CHECK(report.english == 0x40u + 2u - 1u && report.boxes == 0x40u + 2u + 1u && report.boxes_laid_out == 0x40u + 2u);
    if (image) {
        const uint8_t *index = image + ORACLES_IMAGE_LANGUAGE_BANK * 0x4000u;
        CHECK(!memcmp(index + 18, "en\0\0\0\0\0\0", 8));
        const uint8_t *e2 = index + (index[ORACLES_LANGUAGE_INDEX_GROUPS + 16u] | index[ORACLES_LANGUAGE_INDEX_GROUPS + 17u] << 8) + 6u;
        const unsigned addr = e2[1] | e2[2] << 8;
        CHECK(e2[0] == ORACLES_IMAGE_LANGUAGE_BANK && (addr & ORACLES_LANGUAGE_LAID_OUT));
        CHECK(!memcmp(image + (size_t)e2[0] * 0x4000u + ((addr & ~ORACLES_LANGUAGE_LAID_OUT) - 0x4000u), "Hi you all three", 17));
        const uint8_t *e0 = e2 - 6u;
        CHECK(e0[0] == 0 && (e0[1] | e0[2] << 8) == ORACLES_LANGUAGE_KEPT);
    }
    free(image);
    image = NULL;
    /* Without a layout, or for a fan game's image: refused. */
    CHECK(oracles_language_compose_english(ages, rom, MIB, NULL, &image, &size, &report, error, sizeof error) == -1 && !image);
    CHECK(oracles_language_compose_english("013cf1fd3149f87990cf341f2973f1d11a28e55f", rom, MIB, &layout, &image, &size, &report, error, sizeof error) == -1);
    /* The digest of the layout follows its width, the fixed advance and the lines; the game's two lines named or not. */
    char a[41], b[41], c[41], d[41];
    oracles_language_layout_digest(&layout, ORACLES_ROM_REVISION_AGES_US, a);
    const OraclesLanguageLayout wider = { 208, 0, 0, NULL }, two = { 128, 0, 2, NULL }, fixed = { 128, 1, 0, NULL };
    oracles_language_layout_digest(&wider, ORACLES_ROM_REVISION_AGES_US, b);
    CHECK(strlen(a) == 40u && strcmp(a, b) != 0);
    oracles_language_layout_digest(&two, ORACLES_ROM_REVISION_AGES_US, c);
    oracles_language_layout_digest(&fixed, ORACLES_ROM_REVISION_AGES_US, d);
    CHECK(!strcmp(a, c) && strcmp(a, d) != 0);
    /* The wide box's layouts, 192 pixels: classic at the fixed advance, a character a tile, 24 a line; compact in the
     * font's widths, more.  A text of four lines of the game's, two boxes, then holds in one. */
    static const uint8_t long_lines[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', ' ', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 0x01,
                                          'q', 'r', 's', 't', 0x01, 'u', 'v', 'w', 0x01, 'x', 'y', 'z', 0 };
    memcpy(rom + table + base + 0xa40u, long_lines, sizeof long_lines);
    put16(rom + table + g4 + 2u * 4u, 0xa40u);
    static const struct { OraclesLanguageLayout layout; const char *text; } wide[] = {
        { { 192, 1, 0, NULL }, "abcdefgh ijklmnop qrst\001uvw xyz" },
        { { 192, 0, 0, NULL }, "abcdefgh ijklmnop qrst uvw xyz" },
    };
    for (unsigned w = 0; w < 2u; w++) {
        image = NULL;
        uint32_t ranges[2 * 8];
        OraclesLanguagePages pages = { ranges, 8, 0, 0 };
        OraclesLanguageLayout with_pages = wide[w].layout;
        with_pages.pages = &pages;
        CHECK(oracles_language_compose_english(ages, rom, MIB, &with_pages, &image, &size, &report, error, sizeof error) == 0);
        CHECK(pages.count == 2 && pages.overflowed == 0 && ranges[0] < ranges[1] && ranges[1] <= ranges[2] && ranges[2] < ranges[3]);   /* TX_0002's, TX_0004's */
        CHECK(report.laid_out == 2 && report.layout_failed == 0 && report.boxes == 0x40u + 2u + 2u && report.boxes_laid_out == 0x40u + 2u);
        if (image) {
            const uint8_t *index = image + ORACLES_IMAGE_LANGUAGE_BANK * 0x4000u;
            const uint8_t *e4 = index + (index[ORACLES_LANGUAGE_INDEX_GROUPS + 16u] | index[ORACLES_LANGUAGE_INDEX_GROUPS + 17u] << 8) + 12u;
            const unsigned addr = e4[1] | e4[2] << 8;
            CHECK(e4[0] == ORACLES_IMAGE_LANGUAGE_BANK && (addr & ORACLES_LANGUAGE_LAID_OUT));
            CHECK(!memcmp(image + (size_t)e4[0] * 0x4000u + ((addr & ~ORACLES_LANGUAGE_LAID_OUT) - 0x4000u), wide[w].text, strlen(wide[w].text) + 1u));
            /* Its one page, where the text lies, its terminator out of it. */
            const uint32_t at = (uint32_t)e4[0] * 0x4000u + ((addr & ~ORACLES_LANGUAGE_LAID_OUT) - 0x4000u);
            CHECK(ranges[2] == at && ranges[3] == at + strlen(wide[w].text));
        }
        free(image);
    }
    free(rom);
}

static void emit16(uint8_t **at, uint8_t opcode, unsigned address) { *(*at)++ = opcode; *(*at)++ = (uint8_t)address; *(*at)++ = (uint8_t)(address >> 8); }

/* The transaction on a core, in the composed image with a program: at $0150, WRAM bank 7 and ROM bank $3f mapped,
 * then a loop calling getTextAddress and keeping hl, a and w7ActiveBank at $c010-$c013; getTextAddress answers the
 * "US text" $5555 in bank $22.  The text looked up is the one the test writes in wTextIndexH and wTextIndexL. */
static void test_transaction(uint8_t *image, OraclesCoreKind kind)
{
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    uint8_t *at = image + 0x150;
    *at++ = 0x31; *at++ = 0x00; *at++ = 0xc2;                          /* ld sp,$c200 */
    *at++ = 0x3e; *at++ = 0x07; *at++ = 0xe0; *at++ = 0x70;            /* ld a,7 ; ldh (SVBK),a */
    *at++ = 0x3e; *at++ = t->get_text_address.bank; emit16(&at, 0xea, 0x2000);   /* ld a,bank ; ld ($2000),a */
    uint8_t *loop = at;
    emit16(&at, 0xcd, t->get_text_address.addr);                       /* call getTextAddress */
    emit16(&at, 0xea, 0xc012);                                         /* ld ($c012),a */
    *at++ = 0x7d; emit16(&at, 0xea, 0xc010);                           /* ld a,l ; ld ($c010),a */
    *at++ = 0x7c; emit16(&at, 0xea, 0xc011);                           /* ld a,h ; ld ($c011),a */
    emit16(&at, 0xfa, t->active_text_bank.addr); emit16(&at, 0xea, 0xc013);
    *at = 0x18; at[1] = (uint8_t)(loop - (at + 2)); at += 2;           /* jr loop */
    uint8_t *routine = image + oracles_rom_offset(t->get_text_address.bank, t->get_text_address.addr);
    emit16(&routine, 0x21, 0x5555);                                    /* ld hl,$5555 */
    *routine++ = 0x3e; *routine++ = 0x22;                              /* ld a,$22 */
    emit16(&routine, 0xea, t->active_text_bank.addr);                  /* ld (w7ActiveBank),a */
    *routine = 0xc9;                                                   /* ret */

    /* TX_0000 marked laid out again for the compact text: served the same, and counted. */
    uint8_t *flagged = image + ORACLES_IMAGE_LANGUAGE_BANK * 0x4000u;
    flagged += (flagged[ORACLES_LANGUAGE_INDEX_GROUPS + 16u] | flagged[ORACLES_LANGUAGE_INDEX_GROUPS + 17u] << 8) + 2u;
    *flagged |= ORACLES_LANGUAGE_LAID_OUT >> 8;
    const OraclesCoreOptions options = { 0, 0, kind, 0 };
    OraclesCore *core = oracles_core_create(image, 2u * MIB, &options);
    CHECK(core != NULL);
    if (!core) return;
    const OraclesRomInfo info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_AGES_US, .known = 1 };
    OraclesGuest *guest = oracles_guest_attach(core, oracles_compat_find(&info));
    CHECK(guest && oracles_guest_arm_language(guest) == 0 && !strcmp(oracles_guest_language(guest), "fr"));
    if (!guest) { oracles_core_destroy(core); return; }
    const struct { unsigned group, index, hl, a; } cases[] = {
        { 4, 0, 0, ORACLES_IMAGE_LANGUAGE_BANK },   /* served: its address from the index */
        { 4, 1, 0x5555, 0x22 },                     /* kept in English */
        { 4, 2, 0x5555, 0x22 },                     /* absent from the pack */
        { 2, 0, 0x5555, 0x22 },                     /* a fragment of the dictionary */
    };
    const uint8_t *index = image + ORACLES_IMAGE_LANGUAGE_BANK * 0x4000u;
    const uint8_t *entry = index + (index[ORACLES_LANGUAGE_INDEX_GROUPS + 16u] | index[ORACLES_LANGUAGE_INDEX_GROUPS + 17u] << 8);
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        uint8_t *wram = oracles_guest_wram_writable(guest, 0);
        wram[t->text_index_h.addr - 0xc000u] = (uint8_t)cases[i].group;
        wram[t->text_index_l.addr - 0xc000u] = (uint8_t)cases[i].index;
        for (unsigned f = 0; f < (i ? 2u : 200u); f++) oracles_core_run_frame(core);   /* the boot ROM first */
        const unsigned hl = wram[0x10] | wram[0x11] << 8, expected = cases[i].hl ? cases[i].hl : (unsigned)(entry[1] | entry[2] << 8) & ~ORACLES_LANGUAGE_LAID_OUT;
        CHECK(hl == expected && wram[0x12] == cases[i].a && wram[0x13] == cases[i].a);
    }
    unsigned served = 0, english = 0, missing = 0;
    oracles_guest_language_counts(guest, &served, &english, &missing);
    CHECK(served > 100 && english > 100 && missing > 100);
    CHECK(oracles_guest_language_laid_out(guest) > 100 && oracles_guest_language_laid_out(guest) <= served);
    oracles_guest_detach(guest);
    oracles_core_destroy(core);
}

/* A Japanese name in capitals: the rules; the two points found where the tables name the routines; and the transaction
 * on a core, in the composed image with a program at $0150 that calls, in a loop, Ages' text command that prints a name
 * (handleTextControlCode@controlCodeA, 3f:57df: `ldi a,(hl) / or a / ret z / call setLineTextBuffers / jr`, the call
 * answering `ld (de),a / inc e`) on Link's name, on the child's and on a secret, and the file select's copy
 * (copyTextCharactersFromHl, 02:4110, its kanji's `inc c / ldi a,(hl)` before the call of a routine that keeps a in
 * (de)) on the slots of the files' names in WRAM bank 4, on the secret being typed and on WRAM bank 7; the characters
 * kept from $c020, every $10.  In Japanese, the letters of the names that hold a Latin capital only, a kanji's index
 * aside: "Linke", "Li" with a kanji and "k", "abcdZ" and "zzA" (a capital at either end of the range, Z at its slot's
 * last letter); the names without one left as kana, each slot judged within w4NameBuffer: the child's "link" (all in
 * lower case), "\x61\x65" (いか, codes of lower-case letters alone, after the slot that ends in Z), "\xb5\x61" (カい),
 * and the last slot's two bytes "ab", the secret after them starting with a capital; in French, nothing. */
static void test_names(const uint8_t *composed, const char *code, OraclesCoreKind kind)
{
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    CHECK(oracles_guest_name_capital('a') == 'A' && oracles_guest_name_capital('z') == 'Z' && oracles_guest_name_capital('A') == 'A');
    CHECK(oracles_guest_name_capital('`') == '`' && oracles_guest_name_capital('{') == '{' && oracles_guest_name_capital(0xb1) == 0xb1);
    CHECK(oracles_guest_name_latin((const uint8_t *)"Link", 4) && oracles_guest_name_latin((const uint8_t *)"LINK", 4) && oracles_guest_name_latin((const uint8_t *)"liNk", 4));
    CHECK(oracles_guest_name_latin((const uint8_t *)"zzA", 3) && oracles_guest_name_latin((const uint8_t *)"aaZ", 3));
    CHECK(!oracles_guest_name_latin((const uint8_t *)"link", 4) && !oracles_guest_name_latin((const uint8_t *)"\x61\x65\x7a", 3)
          && !oracles_guest_name_latin((const uint8_t *)"\xb5\xbd\x2d\x20", 4) && !oracles_guest_name_latin((const uint8_t *)"@[", 2));
    uint8_t *image = malloc(2u * MIB);
    CHECK(image != NULL);
    if (!image) return;
    memcpy(image, composed, 2u * MIB);
    memset(image + ORACLES_IMAGE_LANGUAGE_BANK * 0x4000u + ORACLES_LANGUAGE_INDEX_CODE, 0, 8);
    memcpy(image + ORACLES_IMAGE_LANGUAGE_BANK * 0x4000u + ORACLES_LANGUAGE_INDEX_CODE, code, strlen(code));
    const unsigned keep = 0x3f00u;   /* bank 0: ld (de),a ; inc de ; ret */
    image[keep] = 0x12; image[keep + 1u] = 0x13; image[keep + 2u] = 0xc9;
    uint8_t *at = image + oracles_rom_offset(t->set_line_text_buffers.bank, t->set_line_text_buffers.addr);
    at[0] = 0x12; at[1] = 0x1c; at[2] = 0xc9;                          /* ld (de),a ; inc e ; ret */
    at = image + oracles_rom_offset(t->text_control_code_name.bank, t->text_control_code_name.addr);
    *at++ = 0x2a; *at++ = 0xb7; *at++ = 0xc8;                           /* ldi a,(hl) ; or a ; ret z */
    emit16(&at, 0xcd, t->set_line_text_buffers.addr); *at++ = 0x18; *at++ = 0xf8;   /* call setLineTextBuffers ; jr */
    at = image + oracles_rom_offset(t->copy_text_characters_from_hl.bank, t->copy_text_characters_from_hl.addr);
    static const uint8_t copy[] = { 0x2a, 0xb7, 0xc8, 0x0e, 0x00, 0xfe, 0x06, 0x20, 0x02, 0x0c, 0x2a };   /* ... ; inc c ; ldi a,(hl) */
    memcpy(at, copy, sizeof copy);
    at += sizeof copy;
    emit16(&at, 0xcd, keep); *at++ = 0x18; *at++ = 0xf0;
    uint16_t text = 0, file = 0;
    CHECK(oracles_guest_find_name_points(t, image, 2u * MIB, &text, &file) == 1 && text == t->text_control_code_name.addr + 3u
          && file == t->copy_text_characters_from_hl.addr + sizeof copy);
    CHECK(oracles_guest_find_name_points(&oracles_guest_tables_moonrise, image, 2u * MIB, &text, &file) == 0);
    at = image + 0x150;
    *at++ = 0x31; *at++ = 0x00; *at++ = 0xc2;                           /* ld sp,$c200 */
    uint8_t *loop = at;
    static const struct { uint8_t rom, ram; uint16_t from, to, routine; } calls[] = {
        { 0x3f, 0, 0xc602, 0xc020, 0 }, { 0x3f, 7, 0xd460, 0xc030, 0 },
        { 0x02, 4, 0xd7a0, 0xc040, 1 }, { 0x02, 4, 0xd7c0, 0xc050, 1 }, { 0x02, 7, 0xd7a0, 0xc060, 1 },
        { 0x3f, 0, 0xc609, 0xc070, 0 }, { 0x02, 4, 0xd7a6, 0xc080, 1 }, { 0x02, 4, 0xd7ac, 0xc090, 1 }, { 0x02, 4, 0xd7b2, 0xc0a0, 1 },
        { 0x02, 4, 0xd7be, 0xc0b0, 1 }, { 0x02, 4, 0xd7b8, 0xc0c0, 1 },
    };
    for (size_t i = 0; i < sizeof calls / sizeof calls[0]; i++) {
        *at++ = 0x3e; *at++ = calls[i].rom; emit16(&at, 0xea, 0x2000);  /* ld a,bank ; ld ($2000),a */
        if (calls[i].ram) { *at++ = 0x3e; *at++ = calls[i].ram; *at++ = 0xe0; *at++ = 0x70; }   /* ld a,bank ; ldh (SVBK),a */
        emit16(&at, 0x21, calls[i].from); emit16(&at, 0x11, calls[i].to);
        emit16(&at, 0xcd, calls[i].routine ? t->copy_text_characters_from_hl.addr : t->text_control_code_name.addr);
    }
    *at = 0x18; at[1] = (uint8_t)(loop - (at + 2)); at += 2;            /* jr loop */
    const OraclesCoreOptions options = { 0, 0, kind, 0 };
    OraclesCore *core = oracles_core_create(image, 2u * MIB, &options);
    CHECK(core != NULL);
    const OraclesRomInfo info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_AGES_US, .known = 1 };
    OraclesGuest *guest = core ? oracles_guest_attach(core, oracles_compat_find(&info)) : NULL;
    CHECK(guest && oracles_guest_arm_language(guest) == 0 && !strcmp(oracles_guest_language(guest), code));
    if (guest) {
        for (unsigned f = 0; f < 200u; f++) oracles_core_run_frame(core);   /* the boot ROM first */
        uint8_t *bank0 = oracles_guest_wram_writable(guest, 0), *bank4 = oracles_guest_wram_writable(guest, 4), *bank7 = oracles_guest_wram_writable(guest, 7);
        memcpy(bank0 + 0x602, "Linke", 6);                                /* wLinkName */
        memcpy(bank0 + 0x609, "link", 5);                                 /* wKidName */
        memcpy(bank7 + 0x460, "\x65\x6a\x30", 4);                         /* w7SecretText1: kana where the US font has lower-case letters */
        memcpy(bank4 + 0x7a0, "Li\x06\x61k", 6);                          /* w4NameBuffer, a kanji's index among its letters */
        memcpy(bank4 + 0x7a6, "abcdZ", 6);                                /* its other slots */
        memcpy(bank4 + 0x7ac, "\x61\x65", 3);
        memcpy(bank4 + 0x7b2, "\xb5\x61", 3);
        memcpy(bank4 + 0x7b8, "zzA", 4);
        memcpy(bank4 + 0x7be, "ab", 2);                                   /* the last one, two bytes */
        memcpy(bank4 + 0x7c0, "Abc", 4);                                  /* w4SecretBuffer */
        memcpy(bank7 + 0x7a0, "xyz", 4);
        memset(bank0 + 0x20, 0, 0xb0);
        for (unsigned f = 0; f < 4u; f++) oracles_core_run_frame(core);
        const int ja = !strcmp(code, "ja");
        CHECK(!memcmp(bank0 + 0x20, ja ? "LINKE" : "Linke", 5));
        CHECK(!memcmp(bank0 + 0x30, "\x65\x6a\x30", 3));
        CHECK(!memcmp(bank0 + 0x40, ja ? "LI\x61K" : "Li\x61k", 4));
        CHECK(!memcmp(bank0 + 0x50, "Abc", 3) && !memcmp(bank0 + 0x60, "xyz", 3));
        CHECK(!memcmp(bank0 + 0x70, "link", 4) && !memcmp(bank0 + 0x80, ja ? "ABCDZ" : "abcdZ", 5) && !memcmp(bank0 + 0x90, "\x61\x65", 2));
        CHECK(!memcmp(bank0 + 0xa0, "\xb5\x61", 2) && !memcmp(bank0 + 0xb0, "abAbc", 5)
              && !memcmp(bank0 + 0xc0, ja ? "ZZA" : "zzA", 3));
        CHECK(ja ? oracles_guest_name_capitals(guest) >= 8u : oracles_guest_name_capitals(guest) == 0);
        CHECK(ja ? oracles_guest_name_kana(guest) >= 9u : oracles_guest_name_kana(guest) == 0);
        oracles_guest_detach(guest);
    }
    if (core) oracles_core_destroy(core);
    free(image);
}

/* The graphics' decompressor on hand-made streams of each mode, and the compressor's round trip. */
static void test_gfx_compression(void)
{
    uint8_t out[2048], packed[2048];
    size_t consumed = 0;
    /* Mode 1: a literal, a reference of 3 at distance 1 (length bits 2), one of 7 bytes with a length byte. */
    static const uint8_t mode1[] = { 0x60, 0xaa, 0x40, 0x01, 0x07 };
    CHECK(language_gfx_decompress(mode1, sizeof mode1, 0, 1, out, 11, &consumed) == 0 && consumed == sizeof mode1);
    CHECK(out[0] == 0xaa && out[3] == 0xaa && out[4] == 0xaa && out[10] == 0xaa);
    /* Mode 3: two literals, a reference of 5 at distance 2 (16 bits: distance - 1, length - 2 in bits 11-15). */
    static const uint8_t mode3[] = { 0x20, 0x11, 0x22, 0x01, 0x18 };
    CHECK(language_gfx_decompress(mode3, sizeof mode3, 0, 3, out, 7, &consumed) == 0 && consumed == sizeof mode3);
    CHECK(out[0] == 0x11 && out[1] == 0x22 && out[2] == 0x11 && out[5] == 0x22 && out[6] == 0x11);
    /* Mode 2: a tile whose bytes 0 and 15 are the common byte, then a raw tile. */
    uint8_t mode2[3 + 14 + 18];
    mode2[0] = 0x80; mode2[1] = 0x01; mode2[2] = 0x5a;
    for (unsigned i = 0; i < 14u; i++) mode2[3 + i] = (uint8_t)(i + 1u);
    mode2[17] = 0; mode2[18] = 0;
    for (unsigned i = 0; i < 16u; i++) mode2[19 + i] = (uint8_t)(0xf0u + i);
    CHECK(language_gfx_decompress(mode2, sizeof mode2, 0, 2, out, 32, &consumed) == 0 && consumed == sizeof mode2);
    CHECK(out[0] == 0x5a && out[1] == 1 && out[14] == 14 && out[15] == 0x5a && out[16] == 0xf0 && out[31] == 0xff);
    /* A reference before the start, a stream cut short: refused. */
    static const uint8_t early[] = { 0x80, 0x00 };
    CHECK(language_gfx_decompress(early, sizeof early, 0, 1, out, 4, &consumed) == -1);
    CHECK(language_gfx_decompress(mode1, 3, 0, 1, out, 11, &consumed) == -1);
    /* The round trip, on tiles of every kind: blank, repeated, text-like, noise; whichever mode wins. */
    uint8_t data[2048];
    uint32_t seed = 12345u;
    unsigned modes = 0;
    for (unsigned kind = 0; kind < 5u; kind++) {
        const size_t size = kind == 4u ? 2048u : 16u * (16u + 24u * kind);
        for (size_t i = 0; i < size; i++) {
            seed = seed * 1103515245u + 12345u;
            data[i] = kind == 0u ? 0 : kind == 1u ? (uint8_t)(i % 48u < 16u ? 0xff : i) : kind == 2u ? (uint8_t)((seed >> 16) % 3u ? 0 : seed >> 24)
                    : kind == 3u ? (uint8_t)(i / 16u % 5u * 17u + (i % 4u ? 0 : 0x80)) : (uint8_t)(seed >> 16);
        }
        unsigned mode = 9;
        const size_t length = language_gfx_compress(data, size, packed, sizeof packed, &mode);
        CHECK(length > 0 && length <= size && mode <= 3u);
        modes |= 1u << mode;
        CHECK(language_gfx_decompress(packed, length, 0, mode, out, size, &consumed) == 0 && consumed == length && !memcmp(out, data, size));
    }
    CHECK(modes & 1u && modes & ~1u);   /* the noise stays raw, the others shrink */
    unsigned mode = 0;
    CHECK(language_gfx_compress(data, 17, packed, sizeof packed, &mode) == 0);   /* not whole tiles */
}

/* A graphic in the pack: kept by the file, refused when its bytes do not decompress to its size. */
static void test_gfx_pack(const char *dir)
{
    OraclesLanguagePack *pack = language_pack_new(ORACLES_GAME_AGES, "9f2dff3c46406839f839a8d535353dfc1e3e8670", "880374fb978b18af4aa529e2e32f7ffb4d7dd2f4");
    static const uint8_t blank[] = { 0x80, 0x00, 0x00, 0x00 };   /* mode 2: a tile of zeros, key all ones */
    static const uint8_t blank2[] = { 0xff, 0xff, 0x00 };
    OraclesLanguageGraphic g = { 0x38, 0x5000, 0, 2, sizeof blank2, blank2 };
    CHECK(language_pack_add_graphic(pack, ORACLES_LANGUAGE_FRENCH, &g) == 0);
    g.address = 0x4fff;
    CHECK(language_pack_add_graphic(pack, ORACLES_LANGUAGE_FRENCH, &g) == -1);   /* sources increase */
    g.address = 0x5100;
    g.data = blank;
    g.length = sizeof blank;
    CHECK(language_pack_add_graphic(pack, ORACLES_LANGUAGE_FRENCH, &g) == -1);   /* not that tile */
    g.bank = 0x40;
    g.data = blank2;
    g.length = sizeof blank2;
    CHECK(language_pack_add_graphic(pack, ORACLES_LANGUAGE_FRENCH, &g) == -1);   /* past the first megabyte */
    CHECK(language_pack_add_graphic(pack, ORACLES_LANGUAGE_ENGLISH, &g) == -1);
    char path[512], error[256];
    snprintf(path, sizeof path, "%s/graphics.pack", dir);
    CHECK(oracles_language_pack_write(pack, path, error, sizeof error) == 0);
    OraclesLanguagePack *back = oracles_language_pack_read(path, error, sizeof error);
    CHECK(back && oracles_language_pack_graphic_count(back, ORACLES_LANGUAGE_FRENCH) == 1 && oracles_language_pack_graphic_count(back, ORACLES_LANGUAGE_GERMAN) == 0);
    OraclesLanguageGraphic read;
    CHECK(back && oracles_language_pack_graphic(back, ORACLES_LANGUAGE_FRENCH, 0, &read) == 0 && read.bank == 0x38 && read.address == 0x5000
          && read.mode == 2 && read.blocks == 0 && read.length == sizeof blank2 && !memcmp(read.data, blank2, sizeof blank2));
    CHECK(back && oracles_language_pack_graphic(back, ORACLES_LANGUAGE_FRENCH, 1, &read) == -1);
    oracles_language_pack_free(back);
    oracles_language_pack_free(pack);
    remove(path);
}

/* The graphics headers of a synthetic image, at the generated table's place: header 0 loads A then B, header 1 loads
 * A, header 2 loads C, every other header B; the objects' and the trees' graphics headers (3f:5a8a, $e0 then $0b, as
 * the code names them at 3f:41d4, 3f:42f5 and 3f:42ff), each loading 32 blocks raw from B's place. */
#define GFX_A 0x38u
#define OBJECTS 0x5a8au
#define OBJECT_HEADERS 0xebu
static void put_entry6(uint8_t *at, unsigned bank, unsigned mode, unsigned address, unsigned dest, unsigned blocks, int more)
{
    at[0] = (uint8_t)(bank | mode << 6); at[1] = (uint8_t)(address >> 8); at[2] = (uint8_t)address;
    at[3] = (uint8_t)(dest >> 8); at[4] = (uint8_t)dest; at[5] = (uint8_t)(blocks | (more ? 0x80u : 0u));
}

static void put_object(uint8_t *rom, unsigned i, unsigned bank, unsigned mode, unsigned address)
{
    uint8_t *at = rom + oracles_rom_offset(0x3f, OBJECTS + 3u * i);
    at[0] = (uint8_t)(bank | mode << 6); at[1] = (uint8_t)(0x80u | address >> 8); at[2] = (uint8_t)address;
}

static void write_gfx_headers(uint8_t *rom)
{
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    const size_t table = oracles_rom_offset(t->gfx_header_table.bank, t->gfx_header_table.addr);
    const unsigned lists = t->gfx_header_table.addr + 2u * GFX_HEADERS;
    uint8_t *list = rom + table + 2u * GFX_HEADERS;
    put_entry6(list, GFX_A, 0, 0x5000, 0x8801, 1, 1);        /* A, raw, 32 bytes */
    put_entry6(list + 6, 0x37, 0, 0x7000, 0x9000, 0, 0);     /* B */
    put_entry6(list + 12, GFX_A, 0, 0x5000, 0x8801, 1, 0);   /* A again */
    put_entry6(list + 18, 0x37, 0, 0x7100, 0x9001, 0, 0);    /* C */
    for (unsigned h = 0; h < GFX_HEADERS; h++) put16(rom + table + 2u * h, h == 0 ? lists : h == 1 ? lists + 12u : h == 2 ? lists + 18u : lists + 6u);
    for (unsigned i = 0; i < 32u; i++) rom[oracles_rom_offset(GFX_A, 0x5000) + i] = (uint8_t)(0x40u + i);
    for (unsigned i = 0; i < 16u; i++) rom[oracles_rom_offset(0x37, 0x7000) + i] = 0x77;
    const unsigned operands[3][2] = { { 0x41d4, OBJECTS + 1u }, { 0x42f5, OBJECTS + 3u * 0xe0u }, { 0x42ff, OBJECTS } };
    for (unsigned i = 0; i < 3u; i++) {
        rom[oracles_rom_offset(0x3f, operands[i][0])] = 0x21;
        put16(rom + oracles_rom_offset(0x3f, operands[i][0]) + 1u, operands[i][1]);
    }
    for (unsigned i = 0; i < OBJECT_HEADERS; i++) put_object(rom, i, 0x37, 0, 0x7000);
}

/* The composition of a graphic: the bytes of A free, the language's version written (in A's place or the bank's free
 * tail), the three entries that named A repointed, B and C untouched; a graphic no header names refused. */
static void test_gfx_compose(void)
{
    uint8_t *rom = us_image();
    write_gfx_headers(rom);
    OraclesLanguagePack *pack = sample_pack();
    uint8_t data[32], packed[32];
    for (unsigned i = 0; i < 32u; i++) data[i] = i < 16u ? 0x00 : (uint8_t)(0xc0u + i % 3u);
    unsigned mode = 0;
    const size_t length = language_gfx_compress(data, sizeof data, packed, sizeof packed, &mode);
    CHECK(length > 0 && length < 32u);
    const OraclesLanguageGraphic g = { GFX_A, 0x5000, 1, (uint8_t)mode, (uint16_t)length, packed };
    CHECK(language_pack_add_graphic(pack, ORACLES_LANGUAGE_FRENCH, &g) == 0);
    uint8_t *image = NULL;
    size_t size = 0;
    OraclesLanguageReport report;
    char error[256];
    CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_FRENCH, rom, MIB, NULL, &image, &size, &report, error, sizeof error) == 0);
    CHECK(report.graphics == 1 && report.graphics_bytes == length);
    if (image) {
        const OraclesGuestTables *t = &oracles_guest_tables_ages;
        const size_t table = oracles_rom_offset(t->gfx_header_table.bank, t->gfx_header_table.addr);
        const uint8_t *list = image + table + 2u * GFX_HEADERS;
        CHECK((list[0] & 0x3fu) == GFX_A && list[0] >> 6 == mode && !memcmp(list, list + 12, 3) && !memcmp(list + 3, "\x88\x01\x81", 3));
        const size_t at = oracles_rom_offset(list[0] & 0x3fu, (unsigned)list[1] << 8 | list[2]);
        CHECK(at < MIB);
        uint8_t out[32];
        size_t consumed = 0;
        CHECK(language_gfx_decompress(image, size, at, mode, out, sizeof out, &consumed) == 0 && !memcmp(out, data, sizeof out));
        CHECK(!memcmp(list + 6, rom + table + 2u * GFX_HEADERS + 6, 6) && !memcmp(list + 18, rom + table + 2u * GFX_HEADERS + 18, 6));
        free(image);
    }
    /* German has no graphic: the headers stay the US ones. */
    CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_GERMAN, rom, MIB, NULL, &image, &size, &report, error, sizeof error) == 0);
    if (image) {
        const OraclesGuestTables *t = &oracles_guest_tables_ages;
        const size_t table = oracles_rom_offset(t->gfx_header_table.bank, t->gfx_header_table.addr);
        CHECK(report.graphics == 0 && !memcmp(image + table, rom + table, 2u * GFX_HEADERS + 24u));
        CHECK(!memcmp(image + oracles_rom_offset(GFX_A, 0x5000), rom + oracles_rom_offset(GFX_A, 0x5000), 32));
    }
    free(image);
    /* A graphic no header loads: refused. */
    const OraclesLanguageGraphic stray = { GFX_A, 0x6000, 1, (uint8_t)mode, (uint16_t)length, packed };
    CHECK(language_pack_add_graphic(pack, ORACLES_LANGUAGE_ITALIAN, &stray) == 0);
    pack->count[ORACLES_LANGUAGE_ITALIAN] = pack->count[ORACLES_LANGUAGE_FRENCH];
    pack->texts[ORACLES_LANGUAGE_ITALIAN] = pack->texts[ORACLES_LANGUAGE_FRENCH];
    CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_ITALIAN, rom, MIB, NULL, &image, &size, &report, error, sizeof error) == -1 && strstr(error, "do not load"));
    pack->count[ORACLES_LANGUAGE_ITALIAN] = 0;
    pack->texts[ORACLES_LANGUAGE_ITALIAN] = NULL;
    oracles_language_pack_free(pack);
    free(rom);
}

/* The objects' graphics headers: one that reads on past its graphic into A's place keeps reading the US bytes, the
 * language's A going elsewhere; one that names A, 32 blocks in the language, is repointed with A's entries, the bit that
 * chains the next header kept; an image whose code does not name the US objects' tables is refused. */
static void test_gfx_compose_objects(void)
{
    uint8_t *rom = us_image();
    write_gfx_headers(rom);
    uint8_t data[512], packed[512];
    for (unsigned i = 0; i < 512u; i++) data[i] = (uint8_t)(i < 32u ? 0x10u + i % 5u : i % 7u);
    unsigned mode = 0;
    size_t length = language_gfx_compress(data, 32, packed, sizeof packed, &mode);
    OraclesLanguagePack *pack = sample_pack();
    const OraclesLanguageGraphic small = { GFX_A, 0x5000, 1, (uint8_t)mode, (uint16_t)length, packed };
    CHECK(length > 0 && language_pack_add_graphic(pack, ORACLES_LANGUAGE_FRENCH, &small) == 0);
    const size_t a = oracles_rom_offset(GFX_A, 0x5000), object = oracles_rom_offset(0x3f, OBJECTS + 3u * 5u);
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    const size_t list = oracles_rom_offset(t->gfx_header_table.bank, t->gfx_header_table.addr) + 2u * GFX_HEADERS;
    uint8_t *image = NULL;
    size_t size = 0;
    OraclesLanguageReport report;
    char error[256];
    /* Object 5 reads 512 bytes from 38:4ff0: A's place kept. */
    put_object(rom, 5, GFX_A, 0, 0x4ff0);
    CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_FRENCH, rom, MIB, NULL, &image, &size, &report, error, sizeof error) == 0);
    if (image) {
        const size_t at = oracles_rom_offset(image[list] & 0x3fu, (unsigned)image[list + 1u] << 8 | image[list + 2u]);
        CHECK(report.graphics == 1 && at != a && !memcmp(image + a, rom + a, 32) && !memcmp(image + object, rom + object, 3));
        free(image);
    }
    oracles_language_pack_free(pack);
    /* Object 5 names A, which the language has in 32 blocks: repointed with A's entries. */
    put_object(rom, 5, GFX_A, 0, 0x5000);
    length = language_gfx_compress(data, sizeof data, packed, sizeof packed, &mode);
    pack = sample_pack();
    const OraclesLanguageGraphic large = { GFX_A, 0x5000, 31, (uint8_t)mode, (uint16_t)length, packed };
    CHECK(length > 0 && language_pack_add_graphic(pack, ORACLES_LANGUAGE_FRENCH, &large) == 0);
    CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_FRENCH, rom, MIB, NULL, &image, &size, &report, error, sizeof error) == 0);
    if (image) {
        const uint8_t *o = image + object;
        uint8_t out[512];
        CHECK(report.graphics == 1 && o[0] == image[list] && o[1] == (0x80u | image[list + 1u]) && o[2] == image[list + 2u]);
        CHECK(language_gfx_decompress(image, size, oracles_rom_offset(o[0] & 0x3fu, (unsigned)(o[1] & 0x7fu) << 8 | o[2]), o[0] >> 6, out, sizeof out, NULL) == 0
              && !memcmp(out, data, sizeof out));
        CHECK(!memcmp(image + object + 3u, rom + object + 3u, 3));
        free(image);
    }
    /* The code no longer naming the objects' table: refused. */
    rom[oracles_rom_offset(0x3f, 0x41d5)] ^= 0x01u;
    image = NULL;
    CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_FRENCH, rom, MIB, NULL, &image, &size, &report, error, sizeof error) == -1 && !image
          && strstr(error, "does not name the US object graphics headers"));
    oracles_language_pack_free(pack);
    free(rom);
}

/* Japanese on constructed data: its texts read where the Japanese getTextAddress reads them, the comparison without
 * what only draws or paces the text, its rules, its tables. */
static void test_japanese(void)
{
    /* Ages': the table of groups at 1e:4000, the first base by its table at 3f:4f63, the second at 22:09f5 from group $34
     * on.  Group 4 (TX_00xx), two texts from the first base, the first with a fragment of the dictionary and a kanji;
     * group $34 (TX_30xx), one from the second. */
    uint8_t *data = calloc(MIB, 1);
    if (!data) return;
    const size_t table = 0x78000u, base1 = table + 0x74u, base2 = 0x889f5u;
    for (unsigned g = 0; g < 0x36u; g++) {
        const unsigned offset = g == 4u ? 0x6eu : g == 0x34u ? 0x72u : 0x6cu;
        data[table + 2u * g] = (uint8_t)offset;
        data[table + 2u * g + 1u] = (uint8_t)(offset >> 8);
    }
    static const uint8_t lists[] = { 0x00, 0x00, 0x10, 0x00, 0x20, 0x00, 0x00, 0x00 };
    memcpy(data + table + 0x6cu, lists, sizeof lists);
    static const uint8_t pointer[] = { 0x1e, 0x74, 0x40 };
    memcpy(data + 0xfcf63u, pointer, sizeof pointer);
    static const uint8_t fragment[] = { 0x60, 0x61, 0 }, first[] = { 0x02, 0x00, 0xb0, 0x06, 0x35, 0x01, 0x21, 0 }, second[] = { 0x0c, 0x08, 0xb9, 0x7e, 0xa4, 0 };
    static const uint8_t third[] = { 0x7e, 0x0f, 0x42, 0 };
    memcpy(data + base1, fragment, sizeof fragment);
    memcpy(data + base1 + 0x10u, first, sizeof first);
    memcpy(data + base1 + 0x20u, second, sizeof second);
    memcpy(data + base2, third, sizeof third);
    const OraclesRom rom = { data, MIB };
    OraclesTextSource source;
    CHECK(language_source_japanese(&rom, ORACLES_GAME_AGES, &source) == 0);
    CHECK(source.table == table && source.bases[0] == base1 && source.bases[1] == base2 && source.splits[0] == 0x34u);
    CHECK(oracles_text_groups(&rom, &source) == 0x36u && oracles_text_count(&rom, &source, 4) == 2 && oracles_text_count(&rom, &source, 0x34) == 1);
    CHECK(oracles_text_count(&rom, &source, 5) == 0);
    static const uint8_t expanded[] = { 0x60, 0x61, 0xb0, 0x06, 0x35, 0x01, 0x21, 0 };
    CHECK(decoded(&rom, &source, 4, 0, expanded, sizeof expanded));
    CHECK(decoded(&rom, &source, 4, 1, second, sizeof second));
    CHECK(decoded(&rom, &source, 0x34, 0, third, sizeof third));
    CHECK(language_source_japanese(&rom, ORACLES_GAME_SEASONS, &source) == 0 && source.table == 0x74000u && source.bases[1] == 0x7e1dfu);
    CHECK(language_source_japanese(&rom, ORACLES_GAME_UNKNOWN, &source) == -1);
    CHECK(language_font_japanese(ORACLES_GAME_AGES) + GLYPH_FIRST * GLYPH_BYTES == 0x70000u);
    free(data);

    /* The codes, and the ROMs a language comes from. */
    OraclesLanguage parsed;
    CHECK(oracles_language_parse("ja", &parsed) == 0 && parsed == ORACLES_LANGUAGE_JAPANESE && !strcmp(oracles_language_code(parsed), "ja"));
    CHECK(oracles_language_compact_text(ORACLES_LANGUAGE_FRENCH) && !oracles_language_compact_text(ORACLES_LANGUAGE_JAPANESE));
    OraclesRomInfo jp, eu;
    CHECK(oracles_rom_known("596aa066ccee9fae71643576f7526baa22d26d6d", &jp) == 0 && jp.game == ORACLES_GAME_AGES && language_japanese(&jp));
    CHECK(oracles_rom_known("9f2dff3c46406839f839a8d535353dfc1e3e8670", &eu) == 0 && !language_japanese(&eu));
    CHECK(oracles_language_source_game_of(&jp, ORACLES_LANGUAGE_JAPANESE) == ORACLES_GAME_AGES && oracles_language_source_game_of(&jp, ORACLES_LANGUAGE_FRENCH) == ORACLES_GAME_UNKNOWN);
    CHECK(oracles_language_source_game_of(&eu, ORACLES_LANGUAGE_FRENCH) == ORACLES_GAME_AGES && oracles_language_source_game_of(&eu, ORACLES_LANGUAGE_JAPANESE) == ORACLES_GAME_UNKNOWN);
    CHECK(oracles_language_source_game(&jp) == ORACLES_GAME_UNKNOWN);   /* not a European ROM */

    /* The comparison: a kanji, an icon in another place or the text's speed change nothing the US engine does; a \pos,
     * a call or a number do. */
    static const uint8_t us[] = { 'Y', 'o', 'u', 0x06, 0x86, ' ', 0x06, 0x85, 0x0c, 0x22, 'x', 0 };
    static const uint8_t ja[] = { 0x06, 0x85, 0x60, 0x06, 0x35, 0x06, 0x86, 0x0c, 0x02, 0x0c, 0x22, 0x0c, 0x3a, 0x61, 0 };
    static const uint8_t ja_moved[] = { 0x0c, 0x22, 0x60, 0 }, ja_call[] = { 0x0c, 0x22, 0x60, 0x0f, 0x01, 0 };
    CHECK(language_same_commands(ORACLES_LANGUAGE_JAPANESE, us, sizeof us, ja, sizeof ja));
    CHECK(!language_same_commands(ORACLES_LANGUAGE_FRENCH, us, sizeof us, ja, sizeof ja));
    CHECK(language_same_commands(ORACLES_LANGUAGE_JAPANESE, us, sizeof us, ja_moved, sizeof ja_moved));   /* every \pos, wherever it is */
    CHECK(!language_same_commands(ORACLES_LANGUAGE_JAPANESE, us, sizeof us, ja_call, sizeof ja_call));

    /* The rules: the Ring Box's count becomes "more" with its counter; a lead with kanji in the text. */
    static const uint8_t ring_us[] = { 'm', 'o', 'r', 'e', 0 };
    static const uint8_t ring[] = { 0x84, 0x97, 0x0c, 0x08, 0xb9, 0x7e, 0xa4, 0x06, 0x0b, 0x89, 0x21, 0 };
    static const uint8_t ring_more[] = { 0x84, 0x97, 0x20, 0x82, 0x93, 0x73, 0x06, 0x0b, 0x89, 0x21, 0 };
    static const uint8_t ring_bare[] = { 0x84, 0x0c, 0x08, 0x21, 0 };
    CHECK(ADAPTED(RULE_NUMBER_AS_MORE, ORACLES_LANGUAGE_JAPANESE, ring_us, ring, ring_more));
    CHECK(!ADAPTED(RULE_NUMBER_AS_MORE, ORACLES_LANGUAGE_JAPANESE, ring_us, ring_bare, ring_more));   /* a number without its counter */
    static const uint8_t lead_us[] = { 0x0c, 0x22, 'Y', 'o', 'u', 0 }, lead_ja[] = { 0x06, 0x35, 0x60, 0 }, lead_made[] = { 0x0c, 0x22, 0x06, 0x35, 0x60, 0 };
    CHECK(ADAPTED(RULE_LEAD, ORACLES_LANGUAGE_JAPANESE, lead_us, lead_ja, lead_made));
    CHECK(!language_rule_holds(RULE_LEAD, ORACLES_LANGUAGE_FRENCH, lead_us, sizeof lead_us, lead_made, sizeof lead_made));   /* the kanji, a \sym */
    /* one-digit: a line of sixteen with its \num1 in one digit, seventeen as the US measure counts a number, served as it
     * is; one more character, or the measure without the rule, refuses it. */
    static const uint8_t count_us[] = { 'S', 'o', ' ', 'f', 'a', 'r', 0x0c, 0x08, 0 };
    static const uint8_t count_ja[] = { 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x20, 0x0c, 0x08, 0x6a, 0x6b, 0x6c, 0x6d, 0 };
    static const uint8_t count_over[] = { 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x20, 0x0c, 0x08, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0 };
    CHECK(ADAPTED(RULE_ONE_DIGIT, ORACLES_LANGUAGE_JAPANESE, count_us, count_ja, count_ja));
    CHECK(!language_text_fits(count_ja, sizeof count_ja) && !language_rule_holds(RULE_LEAD, ORACLES_LANGUAGE_JAPANESE, count_us, sizeof count_us, count_ja, sizeof count_ja));
    CHECK(!ADAPTED(RULE_ONE_DIGIT, ORACLES_LANGUAGE_JAPANESE, count_us, count_over, count_over));
    /* The lines fitted again after number-as-more: a line put on its own that ends with the katakana's long mark ($2d)
     * before a kana is not a word broken at a hyphen. */
    static const uint8_t mark_ja[] = { 0x0c, 0x08, 0xb9, 0x7e, 0xa4, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x20, 0xb5, 0x2d, 0x01,
                                       0xa4, 0xa5, 0 };
    static const uint8_t mark_fit[] = { 0x20, 0x82, 0x93, 0x73, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x01, 0xb5, 0x2d, 0x01, 0xa4,
                                        0xa5, 0 };
    CHECK(ADAPTED(RULE_NUMBER_AS_MORE, ORACLES_LANGUAGE_JAPANESE, ring_us, mark_ja, mark_fit));

    /* The Japanese tables: they pin the Japanese ROMs and name Japanese alone; a row of another language, or a moved
     * text by eu:, refused there; a Japanese row refused in a European table. */
    char error[256];
    language_exception e;
    language_table *ages = language_table_load_japanese(ORACLES_GAME_AGES, error, sizeof error);
    language_table *seasons = language_table_load_japanese(ORACLES_GAME_SEASONS, error, sizeof error);
    CHECK(ages && seasons);
    if (ages) {
        CHECK(!strcmp(language_table_rom(ages), "596aa066ccee9fae71643576f7526baa22d26d6d"));
        CHECK(language_table_find(ages, ORACLES_LANGUAGE_JAPANESE, 0x302d, &e) && e.source == ORACLES_LANGUAGE_SOURCE_MOVED && e.id == 0x302d);
        CHECK(language_table_find(ages, ORACLES_LANGUAGE_JAPANESE, 0x0034, &e) && e.source == ORACLES_LANGUAGE_SOURCE_ADAPTED && e.rule == RULE_NUMBER_AS_MORE);
        CHECK(language_table_find(ages, ORACLES_LANGUAGE_JAPANESE, 0x3159, &e) && e.source == ORACLES_LANGUAGE_SOURCE_OWN && !strcmp(e.key, "fortune-past-bombs"));
        CHECK(!language_table_find(ages, ORACLES_LANGUAGE_FRENCH, 0x302d, &e) && !language_table_find(ages, ORACLES_LANGUAGE_JAPANESE, 0x0066, &e));
        unsigned counts[ORACLES_LANGUAGE_SOURCE_COUNT];
        language_table_tally(ages, counts);
        CHECK(counts[ORACLES_LANGUAGE_SOURCE_OWN] == 24 && counts[ORACLES_LANGUAGE_SOURCE_MOVED] == 8 && counts[ORACLES_LANGUAGE_SOURCE_ADAPTED] == 6
              && counts[ORACLES_LANGUAGE_SOURCE_KEPT] == 0);
        for (unsigned id = 0; id < 0x10000u; id++) {
            uint8_t text[256];
            size_t length = 0;
            if (language_table_find(ages, ORACLES_LANGUAGE_JAPANESE, id, &e) && e.source == ORACLES_LANGUAGE_SOURCE_OWN)
                CHECK(language_own_text(e.key, ORACLES_LANGUAGE_JAPANESE, text, sizeof text, &length) == 0);
        }
    }
    if (seasons) {
        CHECK(!strcmp(language_table_rom(seasons), "08bf49d7e225c695f64d38131cb720decb84b663"));
        CHECK(language_table_find(seasons, ORACLES_LANGUAGE_JAPANESE, 0x2f1b, &e) && e.source == ORACLES_LANGUAGE_SOURCE_MOVED && e.id == 0x2f1e);
        CHECK(language_table_find(seasons, ORACLES_LANGUAGE_JAPANESE, 0x0b4e, &e) && e.source == ORACLES_LANGUAGE_SOURCE_ADAPTED && e.rule == RULE_LEAD);
    }
    language_table_free(ages);
    language_table_free(seasons);
    static const char *const refused[] = {
        "rom\t596aa066ccee9fae71643576f7526baa22d26d6d\n0001\tfr\tjp:TX_0002\tmoved\n",      /* a European language */
        "rom\t596aa066ccee9fae71643576f7526baa22d26d6d\n0001\tja,fr\tjp:TX_0002\tmoved\n",   /* with Japanese */
        "rom\t596aa066ccee9fae71643576f7526baa22d26d6d\n0001\tja\teu:TX_0002\tmoved\n",      /* not the Japanese ROM's */
        "rom\t9f2dff3c46406839f839a8d535353dfc1e3e8670\n0001\tja\tenglish\twhy\n",          /* Japanese in a European table */
        "rom\t9f2dff3c46406839f839a8d535353dfc1e3e8670\n0001\tfr\tjp:TX_0002\twhy\n",        /* the Japanese ROM's there */
    };
    for (size_t i = 0; i < sizeof refused / sizeof refused[0]; i++) {
        language_table *refusal = language_table_parse(refused[i], error, sizeof error);
        CHECK(refusal == NULL);
        language_table_free(refusal);
    }
    language_table *moved = language_table_parse("rom\t596aa066ccee9fae71643576f7526baa22d26d6d\n0001 0003\tja\tjp:TX_0002\tmoved\n", error, sizeof error);
    CHECK(moved && language_table_find(moved, ORACLES_LANGUAGE_JAPANESE, 0x0003, &e) && e.source == ORACLES_LANGUAGE_SOURCE_MOVED && e.id == 2);
    language_table_free(moved);
}

/* Polish, Kaxus's fan translation: its code, its ROMs and patches, its letters; its tables, which pin the Polish ROMs and
 * name Polish alone, and its corrections, which mend the lines past the box; the port's own texts' tables, each naming
 * its own languages alone. */
static void test_polish(void)
{
    OraclesLanguage parsed;
    CHECK(oracles_language_parse("pl", &parsed) == 0 && parsed == ORACLES_LANGUAGE_POLISH && !strcmp(oracles_language_code(parsed), "pl"));
    CHECK(oracles_language_compact_text(ORACLES_LANGUAGE_POLISH));
    OraclesRomInfo ages, seasons, us;
    CHECK(oracles_rom_known("58da91d033bec3a11670dcfb07ac2d7c1b357aa6", &ages) == 0 && ages.game == ORACLES_GAME_AGES && language_polish(&ages) && !language_japanese(&ages));
    CHECK(oracles_rom_known("0255e2c5d4b8eb2ec192d6dbe4d0cd5bc10b5e56", &seasons) == 0 && seasons.game == ORACLES_GAME_SEASONS && language_polish(&seasons));
    CHECK(oracles_rom_known("880374fb978b18af4aa529e2e32f7ffb4d7dd2f4", &us) == 0 && !language_polish(&us) && !oracles_rom_is_original(&ages));
    CHECK(language_edition_of(&ages) == EDITION_POLISH && language_edition_of(&us) == EDITION_EUROPEAN);
    CHECK(oracles_language_source_game_of(&ages, ORACLES_LANGUAGE_POLISH) == ORACLES_GAME_AGES && oracles_language_source_game_of(&ages, ORACLES_LANGUAGE_FRENCH) == ORACLES_GAME_UNKNOWN);
    CHECK(oracles_language_source_game_of(&seasons, ORACLES_LANGUAGE_JAPANESE) == ORACLES_GAME_UNKNOWN && oracles_language_source_game(&seasons) == ORACLES_GAME_UNKNOWN);
    CHECK(oracles_language_patch_game("5173a7aa608597ae7d5ac054d610914d0a292c0d", ORACLES_LANGUAGE_POLISH) == ORACLES_GAME_AGES);
    CHECK(oracles_language_patch_game("b8cac23dd9e7129eafb66cffebd06e718f81213d", ORACLES_LANGUAGE_POLISH) == ORACLES_GAME_SEASONS);
    CHECK(oracles_language_patch_game("5173a7aa608597ae7d5ac054d610914d0a292c0d", ORACLES_LANGUAGE_FRENCH) == ORACLES_GAME_UNKNOWN);
    CHECK(oracles_language_patch_game("58da91d033bec3a11670dcfb07ac2d7c1b357aa6", ORACLES_LANGUAGE_POLISH) == ORACLES_GAME_UNKNOWN);
    CHECK(language_font_polish(ORACLES_GAME_AGES) == language_font_us(ORACLES_GAME_AGES) && language_font_polish(ORACLES_GAME_UNKNOWN) == 0);

    /* Its letters, from $80 where the European font has its accented capitals; an accent of the European font, none. */
    uint8_t out[64];
    size_t length = 0;
    static const uint8_t letters[] = { 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, ' ', 'Z', 0x01, 0 };
    CHECK(language_text_encode_in(ORACLES_LANGUAGE_POLISH, "ąćęśłńóżź Z\\n", strlen("ąćęśłńóżź Z\\n"), out, sizeof out, &length) == 0
          && length == sizeof letters && !memcmp(out, letters, length));
    CHECK(language_text_encode_in(ORACLES_LANGUAGE_POLISH, "é", strlen("é"), out, sizeof out, &length) == -1);
    CHECK(language_text_encode("ą", strlen("ą"), out, sizeof out, &length) == -1);

    /* The tables: they pin the Polish ROMs, with no row; a row of another language, or a moved text by eu: or jp:, refused
     * there; a Polish row refused in a European or a Japanese table. */
    char error[256];
    language_exception e;
    language_table *table = language_table_load_polish(ORACLES_GAME_AGES, error, sizeof error);
    CHECK(table && !strcmp(language_table_rom(table), "58da91d033bec3a11670dcfb07ac2d7c1b357aa6") && !language_table_find(table, ORACLES_LANGUAGE_POLISH, 0x000b, &e));
    language_table_free(table);
    table = language_table_load_polish(ORACLES_GAME_SEASONS, error, sizeof error);
    CHECK(table && !strcmp(language_table_rom(table), "0255e2c5d4b8eb2ec192d6dbe4d0cd5bc10b5e56"));
    language_table_free(table);
    static const char *const refused[] = {
        "rom\t58da91d033bec3a11670dcfb07ac2d7c1b357aa6\n0001\tfr\tpl:TX_0002\tmoved\n",      /* a European language */
        "rom\t58da91d033bec3a11670dcfb07ac2d7c1b357aa6\n0001\tpl,ja\tpl:TX_0002\tmoved\n",   /* with Japanese */
        "rom\t58da91d033bec3a11670dcfb07ac2d7c1b357aa6\n0001\tpl\teu:TX_0002\tmoved\n",      /* not the Polish ROM's */
        "rom\t58da91d033bec3a11670dcfb07ac2d7c1b357aa6\n0001\tpl\tjp:TX_0002\tmoved\n",
        "rom\t9f2dff3c46406839f839a8d535353dfc1e3e8670\n0001\tpl\tenglish\twhy\n",          /* Polish in a European table */
        "rom\t9f2dff3c46406839f839a8d535353dfc1e3e8670\n0001\tfr,pl\tenglish\twhy\n",
        "rom\t596aa066ccee9fae71643576f7526baa22d26d6d\n0001\tpl\tenglish\twhy\n",          /* in a Japanese one */
    };
    for (size_t i = 0; i < sizeof refused / sizeof refused[0]; i++) {
        language_table *refusal = language_table_parse(refused[i], error, sizeof error);
        CHECK(refusal == NULL);
        language_table_free(refusal);
    }
    language_table *moved = language_table_parse("rom\t58da91d033bec3a11670dcfb07ac2d7c1b357aa6\n0003\tpl\tpl:TX_0002\tmoved\n", error, sizeof error);
    CHECK(moved && language_table_find(moved, ORACLES_LANGUAGE_POLISH, 0x0003, &e) && e.source == ORACLES_LANGUAGE_SOURCE_MOVED && e.id == 2);
    language_table_free(moved);

    /* The corrections: Ages' four rows, Seasons none; each line past the box broken again, in the Polish letters. */
    language_corrections *corrections = language_corrections_load_polish(ORACLES_GAME_AGES, error, sizeof error);
    CHECK(corrections && language_corrections_count(corrections) == 4u);
    if (corrections) {
        static const struct { unsigned id; const char *before, *after; } fixed[] = {
            { 0x000b, "a\x01tera\x88niejszo\x83" "ci\x80!\x01W", "a tera\x88niejszo-\x01\x83" "ci\x80!\x01W" },
            { 0x242a, "Goronem-tancerzem!\x01M", "Goronem-\x01tancerzem!\x01M" },
            { 0x3306, "si\x82gnie \x01\x09\x01", "si\x82gnie\x01\x09\x01" },
            { 0x5805, "odrestaurowany...", "naprawiony..." },
        };
        for (size_t i = 0; i < sizeof fixed / sizeof fixed[0]; i++) {
            uint8_t text[64];
            size_t n = strlen(fixed[i].before);
            memcpy(text, fixed[i].before, n);
            CHECK(language_correct(corrections, ORACLES_LANGUAGE_POLISH, fixed[i].id, text, &n, sizeof text) == 1 && n == strlen(fixed[i].after)
                  && !memcmp(text, fixed[i].after, n));
            n = strlen(fixed[i].before);
            memcpy(text, fixed[i].before, n);
            CHECK(language_correct(corrections, ORACLES_LANGUAGE_FRENCH, fixed[i].id, text, &n, sizeof text) == 0);
        }
    }
    language_corrections_free(corrections);
    corrections = language_corrections_load_polish(ORACLES_GAME_SEASONS, error, sizeof error);
    CHECK(corrections && language_corrections_count(corrections) == 0u);
    language_corrections_free(corrections);
    /* A Polish row in the European table, a European one in the Polish table, an "all" there, a European accent in a
     * Polish row: refused. */
    static const char *const refused_rows[][2] = {
        { "ages\t0001\tpl\tab\tcd\twhy\n", "" }, { "", "ages\t0001\tfr\tab\tcd\twhy\n" }, { "", "ages\t0001\tall\tab\tcd\twhy\n" },
        { "", "ages\t0001\tpl\tab\tcé\twhy\n" }, { "ages\t0001\tfr\tab\tcą\twhy\n", "" },
    };
    for (size_t i = 0; i < sizeof refused_rows / sizeof refused_rows[0]; i++) {
        const int polish = !refused_rows[i][0][0];
        language_corrections *refusal = language_corrections_parse(refused_rows[i][polish], polish, ORACLES_GAME_AGES, error, sizeof error);
        CHECK(refusal == NULL);
        language_corrections_free(refusal);
    }
    corrections = language_corrections_parse("ages\t0001\tpl\tab\tcą\twhy\n", 1, ORACLES_GAME_AGES, error, sizeof error);
    CHECK(corrections && language_corrections_count(corrections) == 1u);
    language_corrections_free(corrections);

    /* The port's own texts: each table names its own languages alone, a stray row failing the extraction. */
    CHECK(language_own_check(error, sizeof error) == 0);
    CHECK(language_own_table_check((const char *)oracles_language_own_texts, 0, error, sizeof error) == 0);
    CHECK(language_own_table_check((const char *)oracles_language_own_texts_ja, 1, error, sizeof error) == 0);
    static const struct { const char *table; int japanese; } strays[] = {
        { "# key\tlanguage\ttext\nk\tfr\tabc\nk\tja\tabc\n", 0 }, { "k\tpl\tabc\n", 0 }, { "k\ten\tabc\n", 0 }, { "k\tfr\tabc\n", 1 },
        { "k\tpl\tabc\n", 1 }, { "k\txx\tabc\n", 0 }, { "k fr abc\n", 0 },
    };
    for (size_t i = 0; i < sizeof strays / sizeof strays[0]; i++) CHECK(language_own_table_check(strays[i].table, strays[i].japanese, error, sizeof error) == -1);
    CHECK(language_own_table_check("# a comment\n\nk\tde\tabc\nl\ten-eu\tdef\n", 0, error, sizeof error) == 0);
}

int main(int argc, char **argv)
{
    test_decoder();
    test_japanese();
    test_polish();
    test_gfx_compression();
    test_gfx_pack(argc > 1 ? argv[1] : ".");
    test_gfx_compose();
    test_gfx_compose_objects();
    test_commands();
    test_tables();
    test_rules();
    test_corrections();
    test_own();
    test_pack_file(argc > 1 ? argv[1] : ".");
    test_fingerprint();
    uint8_t *image = test_compose();
    if (image) {
        for (unsigned k = 0; k < 4u; k++) test_names(image, k < 2u ? "ja" : "fr", k % 2u ? ORACLES_CORE_MGBA : ORACLES_CORE_SAMEBOY);
        test_transaction(image, ORACLES_CORE_SAMEBOY);
        test_transaction(image, ORACLES_CORE_MGBA);
    }
    free(image);
    test_compose_layout();
    if (failures) fprintf(stderr, "%d failure(s)\n", failures);
    else printf("language: ok\n");
    return failures ? 1 : 0;
}
