/* The launcher's typefaces and text measurement, without SDL.
 *
 * Sizes are CSS pixels of the 1920x1080 scene.  Measurement
 * follows what a browser does with one line of text: advances and kerning
 * scaled linearly from the font's units, letter spacing added after every
 * character including the last, and a line box whose ascent, descent and
 * line gap are each rounded to whole pixels (the OS/2 typographic metrics
 * when the font sets USE_TYPO_METRICS, else hhea), the leading split with
 * its first half rounded down. */
#ifndef ORACLES_UI_FONT_H
#define ORACLES_UI_FONT_H

#include <stddef.h>
#include <stdint.h>

typedef enum OraclesUiFont {
    ORACLES_UI_FONT_SERIF,          /* Marcellus */
    ORACLES_UI_FONT_SANS,           /* Alegreya Sans */
    ORACLES_UI_FONT_SANS_MEDIUM,
    ORACLES_UI_FONT_SANS_ITALIC,
    ORACLES_UI_FONT_MONO,           /* JetBrains Mono */
    ORACLES_UI_FONT_COUNT
} OraclesUiFont;

typedef struct OraclesUiTextStyle {
    OraclesUiFont font;
    float size;       /* CSS font-size, px */
    float tracking;   /* CSS letter-spacing, em */
    int uppercase;    /* CSS text-transform: uppercase (ASCII letters) */
} OraclesUiTextStyle;

typedef struct OraclesUiGlyph {
    int glyph;        /* glyph index in the font */
    float x;          /* pen position from the start of the text, px */
} OraclesUiGlyph;

/* Parses the embedded fonts once. Returns 0 when one of them is missing or unreadable. */
int oracles_ui_fonts_load(void);

/* One line of text in a block of `line_height` px (0: `normal`): the block's
 * height and the baseline's distance from its top. */
void oracles_ui_line_box(const OraclesUiTextStyle *style, float line_height, float *height, float *baseline);
/* The height of the text's own box (ascent plus descent), as a span of inline text has. */
float oracles_ui_content_height(const OraclesUiTextStyle *style);

/* The text's advance width, letter spacing included. */
float oracles_ui_text_width(const OraclesUiTextStyle *style, const char *utf8);
/* The glyphs of a text and their pen positions; returns the number of glyphs,
 * at most `capacity` stored.  *width receives the advance width. */
size_t oracles_ui_text_glyphs(const OraclesUiTextStyle *style, const char *utf8, OraclesUiGlyph *out, size_t capacity, float *width);

/* Rasterisation at `pixel_size` pixels per em: the glyph's box relative to the
 * pen on the baseline (y down), and its coverage into a buffer of that size. */
void oracles_ui_glyph_box(OraclesUiFont font, int glyph, float pixel_size, int *x0, int *y0, int *x1, int *y1);
void oracles_ui_glyph_render(OraclesUiFont font, int glyph, float pixel_size, unsigned char *out, int width, int height, int stride);
int oracles_ui_font_glyph_count(OraclesUiFont font);

/* The next code point of a UTF-8 string, advancing it; U+FFFD for a malformed sequence, 0 at the end. */
uint32_t oracles_ui_utf8_next(const char **text);

#endif
