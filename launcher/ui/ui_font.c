#include "ui_font.h"

#include "ui_assets.h"
#include "ui_gsub.h"
#include "stb_truetype.h"

#include <math.h>
#include <string.h>

#define MAX_GLYPHS 256

typedef struct face {
    stbtt_fontinfo info;
    OraclesUiGsub ligatures;
    float units_per_em;
    int ascent, descent, line_gap;   /* font units, descent positive */
    int loaded;
} face;

static face faces[ORACLES_UI_FONT_COUNT];
static const char *const face_assets[ORACLES_UI_FONT_COUNT] = {
    "Marcellus-Regular.ttf", "AlegreyaSans-Regular.ttf", "AlegreyaSans-Medium.ttf",
    "AlegreyaSans-Italic.ttf", "JetBrainsMono-Regular.ttf"
};

static unsigned read_u16(const unsigned char *p) { return (unsigned)p[0] << 8 | p[1]; }
static int read_s16(const unsigned char *p) { return (int)(int16_t)read_u16(p); }

/* A table of the font file, or NULL (stb_truetype keeps its own lookup private). */
static const unsigned char *find_table(const OraclesUiAsset *asset, const char *tag, size_t minimum)
{
    const unsigned char *data = asset->data;
    if (asset->size < 12) return NULL;
    const unsigned tables = read_u16(data + 4);
    for (unsigned i = 0; i < tables && 12 + 16 * (i + 1) <= asset->size; i++) {
        const unsigned char *entry = data + 12 + 16 * i;
        if (memcmp(entry, tag, 4) != 0) continue;
        const unsigned long offset = (unsigned long)entry[8] << 24 | (unsigned long)entry[9] << 16 | (unsigned long)entry[10] << 8 | entry[11];
        return offset + minimum <= asset->size ? data + offset : NULL;
    }
    return NULL;
}

static int load_face(face *f, const char *name)
{
    const OraclesUiAsset *asset = oracles_ui_asset(name);
    if (!asset || !stbtt_InitFont(&f->info, asset->data, 0)) return 0;
    const unsigned char *head = find_table(asset, "head", 20);
    if (!head) return 0;
    f->units_per_em = (float)read_u16(head + 18);
    /* The line metrics a browser takes: OS/2 typographic ones when the font asks for them (fsSelection bit 7), else hhea. */
    const unsigned char *os2 = find_table(asset, "OS/2", 74);
    if (os2 && (read_u16(os2 + 62) & 0x80u)) {
        f->ascent = read_s16(os2 + 68);
        f->descent = -read_s16(os2 + 70);
        f->line_gap = read_s16(os2 + 72);
    } else {
        int ascent, descent, gap;
        stbtt_GetFontVMetrics(&f->info, &ascent, &descent, &gap);
        f->ascent = ascent;
        f->descent = -descent;
        f->line_gap = gap;
    }
    oracles_ui_gsub_init(&f->ligatures, asset->data, asset->size, "liga");
    f->loaded = 1;
    return 1;
}

int oracles_ui_fonts_load(void)
{
    for (int i = 0; i < ORACLES_UI_FONT_COUNT; i++)
        if (!faces[i].loaded && !load_face(&faces[i], face_assets[i])) return 0;
    return 1;
}

static float px_per_unit(const face *f, float size) { return size / f->units_per_em; }

void oracles_ui_line_box(const OraclesUiTextStyle *style, float line_height, float *height, float *baseline)
{
    const face *f = &faces[style->font];
    const float k = px_per_unit(f, style->size);
    const float ascent = roundf((float)f->ascent * k), descent = roundf((float)f->descent * k), gap = roundf((float)f->line_gap * k);
    /* Layout units are 1/64 px: 1.05 x 72 px is 75.59375. */
    const float box = line_height > 0.0f ? floorf(line_height * 64.0f) / 64.0f : ascent + descent + gap;
    *height = box;
    *baseline = floorf((box - (ascent + descent)) * 0.5f) + ascent;
}

float oracles_ui_content_height(const OraclesUiTextStyle *style)
{
    const face *f = &faces[style->font];
    const float k = px_per_unit(f, style->size);
    return roundf((float)f->ascent * k) + roundf((float)f->descent * k);
}

uint32_t oracles_ui_utf8_next(const char **text)
{
    const unsigned char *s = (const unsigned char *)*text;
    if (!s[0]) return 0;
    uint32_t c = s[0];
    unsigned extra = c < 0x80u ? 0u : (c & 0xe0u) == 0xc0u ? 1u : (c & 0xf0u) == 0xe0u ? 2u : (c & 0xf8u) == 0xf0u ? 3u : 4u;
    if (extra == 4u) { *text += 1; return 0xfffdu; }
    c &= extra == 0u ? 0x7fu : extra == 1u ? 0x1fu : extra == 2u ? 0x0fu : 0x07u;
    for (unsigned i = 1; i <= extra; i++) {
        if ((s[i] & 0xc0u) != 0x80u) { *text += i; return 0xfffdu; }
        c = c << 6 | (s[i] & 0x3fu);
    }
    *text += 1 + extra;
    return c;
}

size_t oracles_ui_text_glyphs(const OraclesUiTextStyle *style, const char *utf8, OraclesUiGlyph *out, size_t capacity, float *width)
{
    const face *f = &faces[style->font];
    const float k = px_per_unit(f, style->size), spacing = style->tracking * style->size;
    int glyphs[MAX_GLYPHS];
    size_t count = 0;
    for (uint32_t c; count < MAX_GLYPHS && (c = oracles_ui_utf8_next(&utf8)) != 0;) {
        if (style->uppercase && c >= 'a' && c <= 'z') c -= 'a' - 'A';
        glyphs[count++] = stbtt_FindGlyphIndex(&f->info, (int)c);
    }
    /* A browser leaves out the optional ligatures of letter-spaced text. */
    if (spacing == 0.0f) count = oracles_ui_gsub_apply(&f->ligatures, glyphs, count);
    float pen = 0.0f;
    for (size_t i = 0; i < count; i++) {
        if (i) pen += (float)stbtt_GetGlyphKernAdvance(&f->info, glyphs[i - 1], glyphs[i]) * k;
        if (i < capacity) { out[i].glyph = glyphs[i]; out[i].x = pen; }
        int advance, bearing;
        stbtt_GetGlyphHMetrics(&f->info, glyphs[i], &advance, &bearing);
        pen += (float)advance * k + spacing;
    }
    if (width) *width = pen;
    return count;
}

float oracles_ui_text_width(const OraclesUiTextStyle *style, const char *utf8)
{
    float width = 0.0f;
    oracles_ui_text_glyphs(style, utf8, NULL, 0, &width);
    return width;
}

static float raster_scale(const face *f, float pixel_size) { return stbtt_ScaleForMappingEmToPixels(&f->info, pixel_size); }

void oracles_ui_glyph_box(OraclesUiFont font, int glyph, float pixel_size, int *x0, int *y0, int *x1, int *y1)
{
    const face *f = &faces[font];
    const float scale = raster_scale(f, pixel_size);
    stbtt_GetGlyphBitmapBox(&f->info, glyph, scale, scale, x0, y0, x1, y1);
}

void oracles_ui_glyph_render(OraclesUiFont font, int glyph, float pixel_size, unsigned char *out, int width, int height, int stride)
{
    const face *f = &faces[font];
    const float scale = raster_scale(f, pixel_size);
    stbtt_MakeGlyphBitmap(&f->info, out, width, height, stride, scale, scale, glyph);
}

int oracles_ui_font_glyph_count(OraclesUiFont font) { return faces[font].info.numGlyphs; }
