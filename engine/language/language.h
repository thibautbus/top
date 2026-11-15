/* The official translations of Ages and Seasons, played on the US ROM.
 *
 * The user's European ROM of a game holds its texts in English, French,
 * German, Italian and Spanish, under the identifiers (TX_GGII) of the US
 * texts, in the same format, and a font with the glyphs they need.  A pack is
 * extracted from it once (language_extract.c): per language, its English
 * included (English (EU), the European edition's), a text for each
 * US text, the glyphs of the European font that differ from the US one, and
 * the graphics that carry text (language_gfx.c).  A language is then composed
 * into the image the core runs (language_compose.c): the texts in banks of the
 * extension the game never reads, an index of them, the glyphs written over
 * the US font, and the graphics where the US ones were, their headers
 * repointed; the guest serves the index at the
 * return of getTextAddress (guest.h, oracles_guest_arm_language).  The game
 * lays out, pages and prints a European text as it does its own.
 *
 * A US text takes the European text of its identifier when its commands are
 * the US text's (oracles_text_same_commands); else what the game's table of
 * exceptions (config/language_exceptions_*.tsv, language_table.c) names: the
 * European text of another identifier, where the European ROM moved it; the
 * European text adapted to the US code by a rule (language_rules.c), at
 * extraction, on the user's machine; the port's own, unofficial translation
 * (config/language_own.tsv); or the US English.  Every text so made is checked
 * against the US text's commands and lines; one that fails stays in English.
 *
 * Japanese comes the same way from the user's Japanese ROM of a game, whose text engine is the one the US engine was
 * made from: the same control codes, box and font format.  Its texts have the US identifiers but where the two editions
 * pad a group differently: a US text takes the Japanese text of its representative (the first identifier of its group
 * whose text lies where its own does), else what the game's Japanese table of exceptions names; its pack holds the
 * whole Japanese font, kana where the US font has its lower-case and accented letters, written over the US one.
 *
 * Polish comes from Kaxus's fan translation, the Polish ROM his patch makes of the US ROM of a game: an image rebuilt
 * from the disassembly, whose getTextAddress and text format are the US ones, its texts under the US identifiers.  A US
 * text takes the Polish text of its identifier, corrected first where a line runs past the box
 * (config/language_corrections_pl.tsv); its pack holds the nine glyphs of the Polish letters, written over the US
 * accented capitals the English texts never print.  The player gives the Polish ROM, or the patch, which is applied to
 * the US ROM in memory: the port holds neither. */
#ifndef ORACLES_LANGUAGE_H
#define ORACLES_LANGUAGE_H

#include "rom.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A language, in the order the launcher lists them.  A pack holds those from ORACLES_LANGUAGE_ENGLISH_EU on, each the
 * language of the same name of the European ROM. */
typedef enum OraclesLanguage {
    ORACLES_LANGUAGE_ENGLISH = 0,   /* English (US), the US ROM's own: nothing is composed */
    ORACLES_LANGUAGE_ENGLISH_EU,    /* English (EU), the European ROM's English */
    ORACLES_LANGUAGE_FRENCH,
    ORACLES_LANGUAGE_GERMAN,
    ORACLES_LANGUAGE_ITALIAN,
    ORACLES_LANGUAGE_SPANISH,
    ORACLES_LANGUAGE_JAPANESE,      /* the Japanese ROM's, a pack of its own */
    ORACLES_LANGUAGE_POLISH,        /* the Polish ROM's, Kaxus's fan translation, a pack of its own */
    ORACLES_LANGUAGE_COUNT
} OraclesLanguage;
/* Past the languages of the European ROM. */
#define ORACLES_LANGUAGE_EUROPEAN_END ORACLES_LANGUAGE_JAPANESE

/* "en", "en-eu", "fr", "de", "it", "es", "ja", "pl". */
const char *oracles_language_code(OraclesLanguage language);
/* The language of a code; 0, or -1 for a code that names none. */
int oracles_language_parse(const char *code, OraclesLanguage *language);
/* The game whose European ROM the image is (rom.h's revisions), ORACLES_GAME_UNKNOWN for any other. */
OraclesGame oracles_language_source_game(const OraclesRomInfo *info);
/* The game whose ROM of `language` the image is: its European ROM for English (EU) and the four translations, its
 * Japanese ROM for Japanese, its Polish ROM for Polish; ORACLES_GAME_UNKNOWN for any other image or language. */
OraclesGame oracles_language_source_game_of(const OraclesRomInfo *info, OraclesLanguage language);
/* The game whose ROM of `language` the patch of SHA-1 `sha1` makes of the game's US ROM (Kaxus's Polish patches);
 * ORACLES_GAME_UNKNOWN for any other patch or language. */
OraclesGame oracles_language_patch_game(const char *sha1, OraclesLanguage language);
/* Whether the compact text lays out the language's texts: its words are cut at spaces and hyphens, which Japanese,
 * written without either between its words, has not; 0 for Japanese, which keeps the classic text. */
int oracles_language_compact_text(OraclesLanguage language);
/* Whether the image holds its texts in a language of its own, which no pack translates (Oracles Combined, an image a
 * language): 1 with it, 0 for any other image, whose texts are in English. */
int oracles_language_image_own(const OraclesRomInfo *info, OraclesLanguage *language);

/* ---- the pack ------------------------------------------------------------------------- */

#define ORACLES_LANGUAGE_PACK_VERSION 7u
/* The revision of the code that makes a pack from the same ROMs and tables (the extraction, the rules, the
 * corrections, the graphics' compression, a fan game's derivation): raised by hand with any change of what it makes.
 * A pack's fingerprint is the SHA-1 of it, of the tables embedded at configure that the pack is made from
 * (config/language_exceptions_<game>.tsv, language_own.tsv, language_corrections.tsv) and of the text tables and the
 * font its image's profile binds; a pack of another fingerprint is read as one of an earlier format, made again from
 * the ROM it comes from.  A Japanese pack has a revision of its own, raised with any change of what the Japanese
 * extraction makes, and tables of its own (config/language_exceptions_<game>_ja.tsv, language_own_ja.tsv): it is made
 * again alone, the European packs and their savestates untouched.  So is a Polish pack, by its own revision and tables
 * (config/language_exceptions_<game>_pl.tsv, language_corrections_pl.tsv). */
#define ORACLES_LANGUAGE_PACK_REVISION 1u
#define ORACLES_LANGUAGE_PACK_REVISION_JA 6u
#define ORACLES_LANGUAGE_PACK_REVISION_PL 2u

/* Where a language's text for a US text comes from. */
typedef enum OraclesLanguageSource {
    ORACLES_LANGUAGE_SOURCE_SAME = 0,   /* the European or Polish text of the same identifier; the Japanese text of its representative */
    ORACLES_LANGUAGE_SOURCE_MOVED,      /* the European or Japanese text of another identifier, where that ROM moved it */
    ORACLES_LANGUAGE_SOURCE_ADAPTED,    /* that text adapted to the US code by a rule */
    ORACLES_LANGUAGE_SOURCE_OWN,        /* the port's own translation, unofficial */
    ORACLES_LANGUAGE_SOURCE_KEPT,       /* none: the US English, as the table of exceptions says */
    ORACLES_LANGUAGE_SOURCE_REFUSED,    /* none: the translation found has not the US text's commands or lines */
    ORACLES_LANGUAGE_SOURCE_MISSING,    /* none: the ROM it comes from has no text there and the table names none */
    ORACLES_LANGUAGE_SOURCE_FAN,        /* none: a fan game's own text, which no US text equals, in its English */
    ORACLES_LANGUAGE_SOURCE_COUNT
} OraclesLanguageSource;

/* "same", "moved", "adapted", "own", "kept", "refused", "missing", "fan". */
const char *oracles_language_source_name(OraclesLanguageSource source);

typedef struct OraclesLanguagePack OraclesLanguagePack;

/* The pack of a European ROM, extracted with the US ROM of its game beside it (its texts are what the translations are
 * aligned to and checked against, its font what the European glyphs are compared with), or of a Japanese or Polish ROM,
 * which holds its language alone.  NULL with the reason when either is not the ROM it must be, or the game's table of exceptions
 * was not measured on this ROM. */
OraclesLanguagePack *oracles_language_pack_extract(const uint8_t *european, size_t european_size, const uint8_t *us, size_t us_size,
                                                   char *error, size_t capacity);
/* The pack of a fan game made on the US ROM `us` of the game of `base` (that game's pack), for its image `image`: its
 * texts that are US texts take their translations from `base`, its own stay in its English (language_fan.c).  NULL with
 * the reason when the US ROM is not the one `base` translates, or the image is not a fan game the port binds the texts
 * of. */
OraclesLanguagePack *oracles_language_pack_derive(const OraclesLanguagePack *base, const uint8_t *us, size_t us_size, const uint8_t *image,
                                                  size_t size, char *error, size_t capacity);
/* Writes the pack to `path` (language_pack.c says the format), through a temporary file renamed over it; 0, or -1 with
 * the reason. */
int oracles_language_pack_write(const OraclesLanguagePack *pack, const char *path, char *error, size_t capacity);
/* The SHA-1 of the pack's content, as its file holds it (its checksum): what a savestate taken with it is tied to. */
void oracles_language_pack_digest(const OraclesLanguagePack *pack, char out[41]);
/* Reads a pack, its format version, its checksum, its fingerprint and its source checked; NULL with the reason. */
OraclesLanguagePack *oracles_language_pack_read(const char *path, char *error, size_t capacity);
void oracles_language_pack_free(OraclesLanguagePack *pack);

OraclesGame oracles_language_pack_game(const OraclesLanguagePack *pack);
/* The SHA-1 of the European, Japanese or Polish ROM the pack was extracted from. */
const char *oracles_language_pack_source(const OraclesLanguagePack *pack);
/* The SHA-1 of the image whose texts it translates: the US ROM of its game, or a fan game made on it. */
const char *oracles_language_pack_image(const OraclesLanguagePack *pack);
/* The number of entries of a language, one per US text (0 for English (US), which a pack does not hold). */
unsigned oracles_language_pack_count(const OraclesLanguagePack *pack, OraclesLanguage language);
/* The n-th entry of a language, by increasing identifier: its identifier, its source, and the text's bytes with their
 * terminator, or NULL for the sources that keep the US English. */
const uint8_t *oracles_language_pack_nth(const OraclesLanguagePack *pack, OraclesLanguage language, unsigned n, unsigned *id,
                                         OraclesLanguageSource *source, size_t *length);
/* Text TX_`id` of a language, or NULL when the pack serves none. */
const uint8_t *oracles_language_pack_text(const OraclesLanguagePack *pack, OraclesLanguage language, unsigned id, size_t *length);
/* The source of TX_`id` in a language; ORACLES_LANGUAGE_SOURCE_MISSING when the pack has no entry for it. */
OraclesLanguageSource oracles_language_pack_source_of(const OraclesLanguagePack *pack, OraclesLanguage language, unsigned id);
/* A language's entries by source: counts[ORACLES_LANGUAGE_SOURCE_COUNT]. */
void oracles_language_pack_tally(const OraclesLanguagePack *pack, OraclesLanguage language, unsigned counts[ORACLES_LANGUAGE_SOURCE_COUNT]);
/* The glyphs of the European or Polish font that differ from the US one, or the whole Japanese font: their characters,
 * and 16 bytes (1 bit a pixel) each. */
unsigned oracles_language_pack_glyphs(const OraclesLanguagePack *pack, const uint8_t **characters, const uint8_t **bitmaps);

/* A graphic that carries text, in a language: the US graphic it stands for, as the US graphics headers name it (bank and
 * CPU address), its size as they load it, and the language's version, compressed as a header reads it. */
typedef struct OraclesLanguageGraphic {
    uint8_t bank;
    uint16_t address;
    uint8_t blocks;           /* its size in 16-byte blocks, minus 1 */
    uint8_t mode;             /* how `data` is compressed (docs/ROM_DATA_FORMATS.md, section 9.1) */
    uint16_t length;          /* the bytes of `data` */
    const uint8_t *data;
} OraclesLanguageGraphic;

/* The graphics of a language that differ from the US ones (0 for English (US)). */
unsigned oracles_language_pack_graphic_count(const OraclesLanguagePack *pack, OraclesLanguage language);
/* The n-th of them, by increasing source; 0, or -1 past the last. */
int oracles_language_pack_graphic(const OraclesLanguagePack *pack, OraclesLanguage language, unsigned n, OraclesLanguageGraphic *graphic);

/* ---- the composition ------------------------------------------------------------------- */

typedef struct OraclesLanguageReport {
    unsigned served;     /* the US texts the language serves */
    unsigned english;    /* those it keeps in English: kept by the table, or refused */
    unsigned missing;    /* those the pack has no text for */
    unsigned bytes;      /* the bytes of the texts stored, identical texts once */
    unsigned graphics;         /* the graphics that carry text written in the language */
    unsigned graphics_bytes;   /* their bytes, compressed */
    /* The compact text's layout (OraclesLanguageLayout), over every text the language has: */
    unsigned laid_out;         /* the texts laid out again */
    unsigned layout_kept;      /* those that keep the game's layout by config/vwf_keep_<game>.tsv, or another text's call within a line */
    unsigned layout_failed;    /* those laid out again that did not hold, which keep the game's layout */
    unsigned boxes;            /* the boxes of the texts as the game lays them out, */
    unsigned boxes_laid_out;   /* and as they are served */
} OraclesLanguageReport;

/* The texts laid out again for the compact text (language_wrap.c): each page's lines filled to `width` pixels, in the
 * font of the image composed; a width of 0, or no layout, keeps the game's layout.  `fixed`: in that font at the game's
 * fixed advance (oracles_text_font_fixed), the classic text of a box wider than the game's.  `lines`: the lines of a
 * box, 0 for the game's two (the only count laid out yet).  `pages`, when not NULL, receives where the pages laid out
 * again lie in the image composed, each the physical offsets from its first byte to past its last, in increasing order:
 * every other page a text box shows is laid out as the game lays it out (a page kept, a text kept, English kept), which
 * the host's wide box centres.  Ages and Seasons in every language, a fan game in its English (which has no
 * wide box yet). */
typedef struct OraclesLanguagePages {
    uint32_t *ranges;        /* pairs: a page's first offset, past its last; the caller's */
    unsigned capacity;       /* the pairs `ranges` holds */
    unsigned count;          /* the pairs written */
    unsigned overflowed;     /* the pages laid out that did not fit, not written */
} OraclesLanguagePages;
typedef struct OraclesLanguageLayout {
    unsigned width;
    int fixed;
    unsigned lines;
    OraclesLanguagePages *pages;
} OraclesLanguageLayout;
/* The revision of the code that lays the texts out (language_wrap.c), raised by hand with any change of what it makes. */
#define ORACLES_LANGUAGE_LAYOUT_REVISION 3u
/* The SHA-1 of what an image's texts laid out by `layout` depend on besides the texts: the revision, the width, the
 * fixed advance and the lines when they are not the game's, and the kept texts of the image of `revision`
 * (config/vwf_keep_<game>.tsv); a savestate taken within a text laid out is tied to it. */
void oracles_language_layout_digest(const OraclesLanguageLayout *layout, OraclesRomRevision revision, char out[41]);

/* The image with `language` composed into it, from the US ROM of the pack's game, possibly with the mods' houses
 * (mod.h), laid out by `layout` (NULL: the game's layout): a buffer the caller frees.  -1 with the reason when the image
 * is not one, or the language does not fit. */
int oracles_language_compose(const OraclesLanguagePack *pack, OraclesLanguage language, const uint8_t *image, size_t size, const OraclesLanguageLayout *layout,
                             uint8_t **out, size_t *out_size, OraclesLanguageReport *report, char *error, size_t capacity);
/* The same for English (US), which has no pack, laid out by `layout`: an index whose texts are the US texts laid out
 * again, the others kept in English; on the US ROM of SHA-1 `sha1` (the image, possibly with the mods' houses), or on
 * the image of a fan game made on it, whose own English is so laid out, in the banks it leaves to a language; or on an
 * image that holds its own language (oracles_language_image_own), its texts so laid out in it, an index in each half. */
int oracles_language_compose_english(const char *sha1, const uint8_t *image, size_t size, const OraclesLanguageLayout *layout, uint8_t **out,
                                     size_t *out_size, OraclesLanguageReport *report, char *error, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
