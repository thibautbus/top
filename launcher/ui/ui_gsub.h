/* The glyph substitutions a browser applies to Latin text by default that
 * change the launcher's texts: the `liga` feature of the font's Latin script
 * (the `fi` and `fl` ligatures of Alegreya Sans and Marcellus, and Alegreya's
 * contextual `f`).  stb_truetype maps characters to glyphs one to one; this
 * reads the font's GSUB table for the lookup types those fonts use: single
 * substitution (1), ligature (4), chained context by coverage (6, format 3)
 * and their extension (7).  Other lookup types are left alone.  As in a
 * browser, the caller does not apply it to letter-spaced text. */
#ifndef ORACLES_UI_GSUB_H
#define ORACLES_UI_GSUB_H

#include <stddef.h>
#include <stdint.h>

#define ORACLES_UI_GSUB_MAX_LOOKUPS 16

typedef struct OraclesUiGsub {
    const unsigned char *table;    /* the GSUB table; NULL: no substitution */
    size_t size;
    uint16_t lookups[ORACLES_UI_GSUB_MAX_LOOKUPS];   /* the feature's lookups, in the order they apply */
    unsigned lookup_count;
} OraclesUiGsub;

/* Finds `feature` (a four-letter tag) for the Latin script, or the default one, in a TrueType font. */
void oracles_ui_gsub_init(OraclesUiGsub *gsub, const unsigned char *font, size_t size, const char *feature);
/* Substitutes in place; returns the new number of glyphs, at most `count`. */
size_t oracles_ui_gsub_apply(const OraclesUiGsub *gsub, int *glyphs, size_t count);

#endif
