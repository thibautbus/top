/* SameBoy's conversion of a CGB colour (Core/display.c, GB_convert_rgb15, for
 * the model CGB-E, with no light temperature), ported for the cores that do not
 * have it.  SameBoy is under the Expat licence: see third_party/sameboy/LICENSE. */
#include "colour.h"

#include <math.h>

static uint8_t scale_channel(uint8_t x) { return (uint8_t)((x << 3) | (x >> 2)); }

static uint8_t scale_channel_with_curve(uint8_t x)
{
    static const uint8_t curve[32] = { 0, 6, 12, 20, 28, 36, 45, 56, 66, 76, 88, 100, 113, 125, 137, 149,
                                       161, 172, 182, 192, 202, 210, 218, 225, 232, 238, 243, 247, 250, 252, 254, 255 };
    return curve[x];
}

uint32_t oracles_colour_convert(uint16_t rgb555, int correction)
{
    uint8_t r = rgb555 & 0x1fu, g = (rgb555 >> 5) & 0x1fu, b = (rgb555 >> 10) & 0x1fu;
    if (!correction) {
        r = scale_channel(r); g = scale_channel(g); b = scale_channel(b);
    } else {
        r = scale_channel_with_curve(r); g = scale_channel_with_curve(g); b = scale_channel_with_curve(b);
        /* "Modern balanced": green mixed with blue, in a gamma of 1.6 (SameBoy's choice for its high-contrast modes,
         * against too washed-out blues). */
        if (g != b) {
            const double gamma = 1.6;
            g = (uint8_t)round(pow((pow(g / 255.0, gamma) * 3 + pow(b / 255.0, gamma)) / 4, 1 / gamma) * 255);
        }
    }
    return 0xff000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void oracles_colour_fill(uint32_t table[ORACLES_COLOURS], int correction)
{
    for (unsigned c = 0; c < ORACLES_COLOURS; c++) table[c] = oracles_colour_convert((uint16_t)c, correction);
}
