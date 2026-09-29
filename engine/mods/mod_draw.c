/* What a conversation draws: a 160x144 surface over the game's screen, filled
 * with rectangles, the mod's sprites and text in the game's own font, read
 * from the user's ROM. */
#include "mod_internal.h"

#include <string.h>

void mod_draw_clear(OraclesMod *mod)
{
    memset(mod->surface, 0, sizeof mod->surface);
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* "#rrggbb": opaque. */
int mod_parse_colour(const char *text, uint32_t *argb)
{
    if (!text || text[0] != '#' || strlen(text) != 7) return -1;
    uint32_t value = 0;
    for (unsigned i = 1; i < 7; i++) {
        const int digit = hex_digit(text[i]);
        if (digit < 0) return -1;
        value = (value << 4) | (uint32_t)digit;
    }
    *argb = 0xff000000u | value;
    return 0;
}

static void put(OraclesMod *mod, int x, int y, uint32_t argb)
{
    if (x < 0 || y < 0 || x >= (int)ORACLES_MOD_WIDTH || y >= (int)ORACLES_MOD_HEIGHT || !(argb >> 24)) return;
    mod->surface[(size_t)y * ORACLES_MOD_WIDTH + (size_t)x] = argb;
    mod->drawn = 1;
}

void mod_draw_rect(OraclesMod *mod, int x, int y, int width, int height, uint32_t argb)
{
    /* Clipped to the surface first: a rectangle of any size costs at most the surface. */
    const int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
    const int right = x + width > (int)ORACLES_MOD_WIDTH ? (int)ORACLES_MOD_WIDTH : x + width;
    const int bottom = y + height > (int)ORACLES_MOD_HEIGHT ? (int)ORACLES_MOD_HEIGHT : y + height;
    for (int row = top; row < bottom; row++)
        for (int column = left; column < right; column++) put(mod, column, row, argb);
}

void mod_draw_sprite(OraclesMod *mod, const mod_sprite *sprite, int x, int y, int flip_x)
{
    for (uint32_t row = 0; row < sprite->height; row++)
        for (uint32_t column = 0; column < sprite->width; column++) {
            const uint32_t source = flip_x ? sprite->width - 1u - column : column;
            put(mod, x + (int)column, y + (int)row, sprite->pixels[row * sprite->width + source]);
        }
}

/* The US font's accented letters (tools/build/parseText.py, US_available and US_values; ROM_DATA_FORMATS, section 8.1). */
static const struct { const char *utf8; uint8_t byte; } accents[] = {
    { "À", 0x80 }, { "Â", 0x81 }, { "Ä", 0x82 }, { "Æ", 0x83 }, { "Ç", 0x84 }, { "È", 0x85 }, { "É", 0x86 }, { "Ê", 0x87 },
    { "Ë", 0x88 }, { "Î", 0x89 }, { "Ï", 0x8a }, { "Ñ", 0x8b }, { "Ö", 0x8c }, { "Œ", 0x8d }, { "Ù", 0x8e }, { "Û", 0x8f },
    { "Ü", 0x90 }, { "à", 0xa0 }, { "â", 0xa1 }, { "ä", 0xa2 }, { "æ", 0xa3 }, { "ç", 0xa4 }, { "è", 0xa5 }, { "é", 0xa6 },
    { "ê", 0xa7 }, { "ë", 0xa8 }, { "î", 0xa9 }, { "ï", 0xaa }, { "ñ", 0xab }, { "ö", 0xac }, { "œ", 0xad }, { "ù", 0xae },
    { "û", 0xaf }, { "ü", 0xb0 },
};

int mod_font_char(const char **text)
{
    const unsigned char c = (unsigned char)**text;
    if (c >= 0x20u && c < 0x7fu) { (*text)++; return c; }
    for (size_t i = 0; i < sizeof accents / sizeof accents[0]; i++) {
        const size_t length = strlen(accents[i].utf8);
        if (strncmp(*text, accents[i].utf8, length) == 0) { *text += length; return accents[i].byte; }
    }
    return -1;
}

int mod_draw_text(OraclesMod *mod, const char *utf8, int x, int y, uint32_t argb, char *bad, size_t bad_capacity)
{
    const char *at = utf8;
    while (*at) {
        const char *start = at;
        const int byte = mod_font_char(&at);
        if (byte < 0) {
            size_t length = 1;
            while (start[length] && ((unsigned char)start[length] & 0xc0u) == 0x80u) length++;
            if (bad && bad_capacity) snprintf(bad, bad_capacity, "%.*s", (int)length, start);
            return -1;
        }
        if (x >= (int)ORACLES_MOD_WIDTH) { x += 8; continue; }   /* past the surface: checked, not drawn */
        for (unsigned row = 0; row < 16u; row++) {
            const uint8_t bits = mod->font[byte][row];
            for (unsigned column = 0; column < 8u; column++)
                if (!(bits & (0x80u >> column))) put(mod, x + (int)column, y + (int)row, argb);
        }
        x += 8;
    }
    return 0;
}

void mod_font_load(OraclesMod *mod, const uint8_t *rom, size_t rom_size, uint8_t bank, uint16_t address)
{
    const size_t offset = (size_t)bank * 0x4000u + (size_t)(address - 0x4000u);
    memset(mod->font, 0xff, sizeof mod->font);
    if (address < 0x4000u || offset + sizeof mod->font > rom_size) return;
    memcpy(mod->font, rom + offset, sizeof mod->font);
}
