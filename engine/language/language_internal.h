/* What the language's files share: the pack in memory, the texts' sources of a US or a European ROM, the table of
 * exceptions, the rules and the port's own texts.  Not a public interface: the host sees language.h. */
#ifndef ORACLES_LANGUAGE_INTERNAL_H
#define ORACLES_LANGUAGE_INTERNAL_H

#include "guest_tables.h"
#include "language.h"
#include "profile.h"
#include "oracles_text.h"

#define FONT_FIRST 0x10u   /* the first character a text prints; below are the control codes */
#define FONT_GLYPHS (0x100u - FONT_FIRST)
#define GLYPH_BYTES 16u
#define LINE_CHARACTERS 16u   /* a text box's line */

typedef struct pack_text {
    uint16_t id;
    uint8_t source;       /* OraclesLanguageSource */
    uint16_t length;      /* its terminator included; 0 for the sources without a text */
    uint32_t at;          /* in the pack's bytes */
} pack_text;

typedef struct pack_graphic {
    uint8_t bank, blocks, mode;
    uint16_t address, length;
    uint32_t at;          /* in the pack's bytes */
} pack_graphic;

struct OraclesLanguagePack {
    OraclesGame game;
    char source[41];
    char image[41];       /* the image whose texts the entries are, by their identifiers */
    char fingerprint[41]; /* the code and the tables it was made with (ORACLES_LANGUAGE_PACK_REVISION) */
    unsigned count[ORACLES_LANGUAGE_COUNT];
    pack_text *texts[ORACLES_LANGUAGE_COUNT];   /* by increasing identifier; none for English (US) */
    unsigned graphic_count[ORACLES_LANGUAGE_COUNT];
    pack_graphic *graphics[ORACLES_LANGUAGE_COUNT];   /* by increasing source; none for English (US) */
    uint8_t *bytes;
    size_t size, capacity;
    unsigned glyph_count;
    uint8_t characters[FONT_GLYPHS];
    uint8_t bitmaps[FONT_GLYPHS * GLYPH_BYTES];
};

#define LANGUAGE_LAST_BANK 0x7fu   /* a language's banks end with it */

/* The image a pack's texts are for: an original US ROM, or a fan game made on one (its compatibility profile binds its
 * text's tables, its font and the first of the banks it leaves empty for a language); or an image whose texts are its
 * own alone, laid out again in their language, a half of it at a time (Oracles Combined: `halves` 2, each half's
 * profile oracles_compat_half's, its banks counted from its start). */
typedef struct language_target {
    OraclesGame game;                    /* the family, whose European ROM the translations come from */
    OraclesRomRevision revision;         /* the image's: an original's US ROM, or the fan game's */
    const OraclesGuestTables *tables;    /* the first half's */
    unsigned first_bank;                 /* the language's index and texts, from it to LANGUAGE_LAST_BANK */
    int original;                        /* an original US ROM: the image may hold the mods' houses, and grows to 2 MiB */
    char sha1[41];
    const char *name;
    const OraclesCompatProfile *profile; /* the first half's */
    unsigned halves;                     /* 1, or 2 for an image of two games */
    OraclesLanguage own;                 /* the language of its own texts: English, or the one an image of two games holds */
} language_target;

/* language_rom.c: the texts' source of an image whose text's tables are `tables`, of the image of a profile (its third
 * base too), of the US ROM of `game`, and of a language of its European ROM; 0 or -1. */
int language_source_tables(const OraclesRom *rom, const OraclesGuestTables *tables, OraclesTextSource *source);
int language_source_profile(const OraclesRom *rom, const OraclesCompatProfile *profile, OraclesTextSource *source);
int language_source_us(const OraclesRom *rom, OraclesGame game, OraclesTextSource *source);
/* Text TX_`id` of a source of `groups` internal groups, decoded into `out` (ORACLES_TEXT_MAX bytes); 0, or -1 when the
 * source has none. */
int language_text_of(const OraclesRom *rom, const OraclesTextSource *source, unsigned groups, unsigned id, uint8_t *out, size_t *length);
/* A set of texts by their bytes, which lie elsewhere (`bytes` + at): `count` slots, a power of two, an empty one of
 * length 0.  The slot of `text`: the one that holds it, or the empty one where it goes. */
typedef struct language_slot { uint32_t at; uint16_t length, id; } language_slot;
language_slot *language_slot_of(language_slot *slots, unsigned count, const uint8_t *bytes, const uint8_t *text, size_t length);
/* The image of a SHA-1 as a language's target; 0, or -1 when the port knows no profile for it that binds its texts. */
int language_target_of(const char *sha1, language_target *target);
int language_source_european(const OraclesRom *rom, OraclesGame game, OraclesLanguage language, OraclesTextSource *source);
/* A language's index in the European ROM's tables: 0 English (both Englishes), 1 French, 2 German, 3 Italian, 4 Spanish. */
#define EUROPEAN_LANGUAGES 5u
unsigned language_european_index(OraclesLanguage language);
/* The physical offset of character 0 of the US font, of the European font. */
size_t language_font_us(OraclesGame game);
size_t language_font_european(OraclesGame game);

/* The physical offset of the graphics headers' table of the US ROM, of the European ROM (0 for another game). */
size_t language_gfx_headers_us(OraclesGame game);
size_t language_gfx_headers_european(OraclesGame game);

/* language_pack.c: the tables embedded at configure that a pack of `game` is made from, at most
 * LANGUAGE_FINGERPRINT_TABLES; the fingerprint of the code's `revision`, those `tables` (NUL-terminated) and the text
 * tables and the font `target` binds; and the fingerprint of a pack made now for `target`. */
#define LANGUAGE_FINGERPRINT_TABLES 3u
unsigned language_fingerprint_tables(OraclesGame game, const unsigned char *tables[LANGUAGE_FINGERPRINT_TABLES]);
void language_fingerprint_of(unsigned revision, const unsigned char *const *tables, unsigned count, const language_target *target, char out[41]);
void language_fingerprint(const language_target *target, char out[41]);
/* language_pack.c: an empty pack, from the European ROM `source`, for the texts of the image `image` (a target the port knows, whose
 * fingerprint it takes); an entry appended to a language (in increasing identifiers): a text of `length`
 * bytes, its terminator included, for the sources that have one, none (NULL, 0) for the others; 0 or -1.  A graphic
 * appended to a language (in increasing sources), its bytes copied; 0, or -1 when they do not decompress to its size. */
OraclesLanguagePack *language_pack_new(OraclesGame game, const char *source, const char *image);
int language_pack_add(OraclesLanguagePack *pack, OraclesLanguage language, unsigned id, OraclesLanguageSource source, const uint8_t *text, size_t length);
int language_pack_add_graphic(OraclesLanguagePack *pack, OraclesLanguage language, const OraclesLanguageGraphic *graphic);

/* ---- the graphics that carry text (language_gfx.c) ------------------------------------------------------------- */

#define GFX_HEADERS 0xbbu   /* gfxHeaderTable's headers, in both games and both regions */

/* Decompresses `size` bytes in `mode` (ROM_DATA_FORMATS, section 9.1) from the physical offset `at`, reading on
 * across banks as the game does; the bytes read in *consumed.  0, or -1 when the data leaves the image or refers to a
 * byte before its start. */
int language_gfx_decompress(const uint8_t *rom, size_t rom_size, size_t at, unsigned mode, uint8_t *out, size_t size, size_t *consumed);
/* Compresses `size` bytes (a multiple of 16, 2048 at most) in the mode that makes them shortest; the length written to
 * `out` (`capacity` at least `size`: raw, mode 0, is never longer), its mode in *mode; 0 on failure. */
size_t language_gfx_compress(const uint8_t *data, size_t size, uint8_t *out, size_t capacity, unsigned *mode);
/* The graphics of every language of the European ROM that differ from the US ones, added to the pack; 0 or -1. */
int language_gfx_extract(OraclesLanguagePack *pack, const OraclesRom *eu, const OraclesRom *us, char *error, size_t capacity);
/* A language's graphics written into the 2 MiB image and its US headers repointed to them; 0, or -1 with the reason. */
int language_gfx_compose(const OraclesLanguagePack *pack, OraclesLanguage language, uint8_t *image, size_t size, OraclesLanguageReport *report,
                         char *error, size_t capacity);

/* ---- the rules (language_rules.c) ----------------------------------------------------------------------------- */

typedef enum language_rule {
    RULE_LEAD,              /* the text's leading \pos and \sfx become the US text's */
    RULE_END_AT_POS,        /* the text ends at its first \pos past its start */
    RULE_US_CALL,           /* each \call calls the US text's text */
    RULE_SHOP_PRICE,        /* the price becomes \num1, the choice the US text's \jump */
    RULE_JOIN_NEXT,         /* the text runs on into the next identifier's */
    RULE_NUMBER_AS_MORE,    /* \num1 becomes the language's "more", the lines fitted again */
    RULE_OPTION_NUMBERS,    /* a choice that is a number takes the US text's number */
    RULE_UNSHOWN_NUMBER,    /* as it is: the US code's number is not printed */
    RULE_SYMBOLS,           /* as it is: its \sym, which print a glyph, are not the US text's */
    RULE_COUNT
} language_rule;

/* A rule's name in the table ("lead", "end-at-pos"...); and the rule of a name, 0 or -1. */
const char *language_rule_name(language_rule rule);
int language_rule_parse(const char *name, size_t length, language_rule *rule);
/* The European text `text` of a US text `us` adapted by `rule` into `out`, its terminator included; `next` is the
 * European text of the next identifier (join-next), NULL when there is none.  0, or -1 when the rule does not apply. */
int language_rule_apply(language_rule rule, OraclesLanguage language, const uint8_t *us, size_t us_length, const uint8_t *text, size_t length,
                        const uint8_t *next, size_t next_length, uint8_t *out, size_t capacity, size_t *out_length);
/* Whether an adapted text holds: the US text's commands as the US engine acts on them (the \pos past a text's start
 * does nothing there, and the rule may leave out a \num or a \sym), no line over LINE_CHARACTERS, no line break right
 * before its end. */
int language_rule_check(language_rule rule, const uint8_t *us, size_t us_length, const uint8_t *text, size_t length);
/* Whether a text's lines fit: none over LINE_CHARACTERS, none empty at its end. */
int language_text_fits(const uint8_t *text, size_t length);
/* The corrections of config/language_corrections.tsv for `game`, embedded at configure, their words encoded; NULL with
 * the reason when a row is not one. */
typedef struct language_corrections language_corrections;
language_corrections *language_corrections_load(OraclesGame game, char *error, size_t capacity);
void language_corrections_free(language_corrections *corrections);
/* The corrections for text `id` in `language`, made in `text` (`*length` bytes of `capacity`): 1 when one applied, 0
 * when none, -1 when the text would not fit `capacity`. */
int language_correct(language_corrections *corrections, OraclesLanguage language, unsigned id, uint8_t *text, size_t *length, size_t capacity);
/* What the corrections did, for the tests: their rows (the game's), the (text, language) row n names, those it replaced
 * words in, and the corrected texts that did not hold (language_corrections_refuse), served uncorrected. */
unsigned language_corrections_count(const language_corrections *corrections);
unsigned language_corrections_named(const language_corrections *corrections, unsigned n);
unsigned language_corrections_applied(const language_corrections *corrections, unsigned n);
unsigned language_corrections_refused(const language_corrections *corrections);
void language_corrections_refuse(language_corrections *corrections);
/* oracles_language_pack_extract with the corrections given, which keep the count of what they did. */
OraclesLanguagePack *language_pack_extract_with(const uint8_t *european, size_t european_size, const uint8_t *us, size_t us_size,
                                                language_corrections *corrections, char *error, size_t capacity);

/* ---- the table of exceptions (language_table.c) --------------------------------------------------------------- */

typedef struct language_exception {
    OraclesLanguageSource source;   /* MOVED, ADAPTED, OWN or KEPT */
    unsigned id;                    /* MOVED: the European text's identifier */
    language_rule rule;             /* ADAPTED */
    char key[32];                   /* OWN: the key of config/language_own.tsv */
} language_exception;

typedef struct language_table language_table;

/* The table of `game`, config/language_exceptions_<game>.tsv embedded at configure; NULL with the reason when a row
 * is not one, or names a text and a language twice. */
language_table *language_table_load(OraclesGame game, char *error, size_t capacity);
/* The same from a table's text (the tests'). */
language_table *language_table_parse(const char *text, char *error, size_t capacity);
void language_table_free(language_table *table);
/* The SHA-1 of the European ROM the table was measured on. */
const char *language_table_rom(const language_table *table);
/* The exception for TX_`id` in `language`: 1 with it, 0 when the table names none. */
int language_table_find(const language_table *table, OraclesLanguage language, unsigned id, language_exception *exception);
/* The number of (text, language) the table names, by source: counts[ORACLES_LANGUAGE_SOURCE_COUNT]. */
void language_table_tally(const language_table *table, unsigned counts[ORACLES_LANGUAGE_SOURCE_COUNT]);

/* ---- the layout of the compact text (language_wrap.c) -------------------------------------------------------- */

/* A layout of `width` pixels a line in `font` (from 1 to ORACLES_TEXT_LINE_TILES cells) for the texts of `language`;
 * NULL out of that range or of memory. */
typedef struct language_wrap language_wrap;
language_wrap *language_wrap_new(const OraclesTextFont *font, unsigned width, OraclesLanguage language);
void language_wrap_free(language_wrap *wrap);
/* The words a language's text writes with a hyphen inside a line, learned from it: a word broken by a hyphen at a line's
 * end is joined with it when it is one of them.  In a language whose hyphens at a line's end are its compounds' (English,
 * French, Spanish), the words it writes whole too: a word broken by a hyphen at a line's end is joined without it when
 * the language writes it whole, or neither of its parts alone, the others keeping theirs; German and Italian break their
 * words by syllables, and join them without it. */
void language_wrap_learn(language_wrap *wrap, const uint8_t *text, size_t length);
typedef enum language_wrap_result {
    WRAP_SAME,     /* nothing to lay out again: a single box, or every page kept as it is */
    WRAP_DONE,     /* laid out again, into `out` */
    WRAP_FAILED    /* laid out again, it did not hold: the text keeps its layout */
} language_wrap_result;
/* A decoded text laid out again into `out` (`capacity` bytes, ORACLES_TEXT_MAX at least), its terminator included. */
language_wrap_result language_wrap_text(const language_wrap *wrap, const uint8_t *text, size_t length, uint8_t *out, size_t capacity, size_t *out_length);
/* The same, the pages it laid out again in `out` from page_from[i] to page_to[i] - 1, *pages of them (`page_capacity` at
 * most kept, all counted): the pages of a text WRAP_DONE gives that are not kept as they are. */
language_wrap_result language_wrap_text_pages(const language_wrap *wrap, const uint8_t *text, size_t length, uint8_t *out, size_t capacity, size_t *out_length,
                                              size_t *page_from, size_t *page_to, unsigned page_capacity, unsigned *pages);
/* Whether every line of a text fits the layout's width, measured as the compact text draws it, a name and a number at
 * their widest, from either attribute a box opens with. */
int language_wrap_fits(const language_wrap *wrap, const uint8_t *text, size_t length);
/* The boxes a text shows, two lines a box, a \stop or \heartpiece opening the next; up to its first \jump or cmd8. */
unsigned language_text_boxes(const uint8_t *text, size_t length);
/* The texts of its group a text calls or jumps to within a line, by their index, at most `capacity`: those go on a
 * line the text started, and keep their layout. */
unsigned language_text_inline_targets(const uint8_t *text, size_t length, uint8_t *targets, unsigned capacity);
/* The texts the compact text leaves as the game lays them out, config/vwf_keep_<game>.tsv embedded at configure, of
 * the image of `revision`: an original's (ages, seasons) or a fan game's (moonrise, kinomi, temple); the table's text,
 * empty for another, and the table read; NULL with the reason when a row is not one. */
typedef struct language_keep language_keep;
const char *language_keep_table(OraclesRomRevision revision);
language_keep *language_keep_load(OraclesRomRevision revision, char *error, size_t capacity);
/* The table of the half of `family` of an image of two games, its game's: Oracles Combined's halves are Ages' and
 * Seasons' texts, under their identifiers; the image's own for any other. */
const char *language_keep_table_half(OraclesRomRevision revision, OraclesGame family);
language_keep *language_keep_parse(const char *table, char *error, size_t capacity);
int language_keep_has(const language_keep *keep, OraclesLanguage language, unsigned id);
void language_keep_free(language_keep *keep);

/* ---- the port's own texts (language_own.c) -------------------------------------------------------------------- */

/* A text written in config/language_own.tsv's syntax (UTF-8 characters of the font, \n, \stop, \col(N), \\, \xNN)
 * encoded as the game reads it, its terminator added; 0, or -1 for a character or an escape the font has not. */
int language_text_encode(const char *text, size_t length, uint8_t *out, size_t capacity, size_t *out_length);
/* Our text `key` in `language`, encoded; 0, or -1 when there is none or it cannot be encoded. */
int language_own_text(const char *key, OraclesLanguage language, uint8_t *out, size_t capacity, size_t *out_length);
/* The n-th row of config/language_own.tsv: its key, language and text (not terminated); 0, or -1 past the last. */
int language_own_nth(unsigned n, const char **key, size_t *key_length, OraclesLanguage *language, const char **text, size_t *text_length);

#endif
