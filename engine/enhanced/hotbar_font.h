/* The hotbar's 3x5 font: digits, capitals, a dash and a question mark for anything else.
 * A glyph is fifteen bits, rows from the top, the leftmost pixel first. */
#ifndef ORACLES_ENHANCED_HOTBAR_FONT_H
#define ORACLES_ENHANCED_HOTBAR_FONT_H

#include <stdint.h>

#define HOTBAR_GLYPH_WIDTH 3u
#define HOTBAR_GLYPH_HEIGHT 5u

static const uint16_t hotbar_glyphs[38] = {
    0x7b6f, 0x2c97, 0x73e7, 0x72cf, 0x5bc9, 0x79cf, 0x79ef, 0x7292,   /* 01234567 */
    0x7bef, 0x7bcf, 0x2bed, 0x6bae, 0x3923, 0x6b6e, 0x79a7, 0x79a4,   /* 89ABCDEF */
    0x396b, 0x5bed, 0x7497, 0x126a, 0x5bad, 0x4927, 0x5fed, 0x6b6d,   /* GHIJKLMN */
    0x2b6a, 0x6ba4, 0x2b7b, 0x6bad, 0x388e, 0x7492, 0x5b6f, 0x5b6a,   /* OPQRSTUV */
    0x5bfd, 0x5aad, 0x5a92, 0x72a7, 0x01c0, 0x7282,   /* WXYZ-? */
};

static inline uint16_t hotbar_glyph(char c)
{
    if (c >= '0' && c <= '9') return hotbar_glyphs[c - '0'];
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return hotbar_glyphs[10 + (c - 'A')];
    if (c == '-') return hotbar_glyphs[36];
    if (c == ' ') return 0;
    return hotbar_glyphs[37];
}

#endif
