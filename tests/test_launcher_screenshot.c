/* The home screen drawn without a display (oracles --launcher-screenshot):
 * checks the PPM it wrote.  The image has the size asked for, is not empty
 * (many colours, the motif and the text drawn over the scene's background),
 * outside the scene of the layout the image's shape chooses (16:9 or 4:3,
 * ui_layout.h) the letterbox is #050507, and a text is written: where the
 * layout reference puts the hero's title "Ages" (its line box in
 * launcher_layout_reference.json, frame 1a or 10a), or the text of FRAME's
 * PROBE, light text covers part of the box, which the background and the
 * motif's strokes, darker, do not.
 *
 *   oracles-test-launcher-screenshot FILE.ppm WIDTH HEIGHT launcher_layout_reference.json [FRAME PROBE]
 */
#include "layout_reference.h"
#include "ui_layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LETTERBOX 0x050507u
#define MIN_COLOURS 64u
#define TEXT_LUMA 180u          /* the title's #efedf2, antialiased; the motif's strokes stay under it */
#define MIN_TEXT_SHARE 0.03     /* of the title's line box */

static unsigned pixel(const unsigned char *rgb, int width, int x, int y)
{
    const unsigned char *p = rgb + ((size_t)y * (size_t)width + (size_t)x) * 3u;
    return (unsigned)p[0] << 16 | (unsigned)p[1] << 8 | p[2];
}

int main(int argc, char **argv)
{
    if ((argc != 5 && argc != 7) || !layout_reference_load(argv[4])) {
        fprintf(stderr, "usage: oracles-test-launcher-screenshot FILE.ppm WIDTH HEIGHT launcher_layout_reference.json [FRAME PROBE]\n");
        return 2;
    }
    const int want_w = atoi(argv[2]), want_h = atoi(argv[3]);
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "FAIL no image %s\n", argv[1]); return 1; }
    int width = 0, height = 0, maximum = 0;
    if (fscanf(f, "P6 %d %d %d", &width, &height, &maximum) != 3 || fgetc(f) == EOF || maximum != 255) { fprintf(stderr, "FAIL not a binary PPM\n"); fclose(f); return 1; }
    if (width != want_w || height != want_h) { fprintf(stderr, "FAIL %dx%d, asked for %dx%d\n", width, height, want_w, want_h); fclose(f); return 1; }
    const size_t size = (size_t)width * (size_t)height * 3u;
    unsigned char *rgb = malloc(size);
    const int complete = rgb && fread(rgb, 1, size, f) == size;
    fclose(f);
    if (!complete) { fprintf(stderr, "FAIL the image is cut short\n"); free(rgb); return 1; }

    int failures = 0;
    /* Distinct colours, counted in a table of 2^15 buckets: an empty or flat image has a handful. */
    static unsigned char seen[1u << 15];
    unsigned colours = 0;
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            const unsigned c = pixel(rgb, width, x, y), bucket = (c >> 9 & 0x7c00u) | (c >> 6 & 0x3e0u) | (c >> 3 & 0x1fu);
            if (!seen[bucket]) { seen[bucket] = 1; colours++; }
        }
    if (colours < MIN_COLOURS) { fprintf(stderr, "FAIL %u colours: the home screen is not drawn\n", colours); failures++; }
    /* The scene of the layout, min(w/its width, h/1080), centred: the letterbox outside, not inside.  The settings
     * are an empty folder's: aspect= is auto. */
    const OraclesUiLayout layout = oracles_ui_layout_choose(width, height, 0);
    const double scene_width = (double)oracles_ui_scene_width(layout);
    const double scale = (double)width / scene_width < (double)height / 1080.0 ? (double)width / scene_width : (double)height / 1080.0;
    const int scene_w = (int)(scene_width * scale + 0.5), scene_h = (int)(1080.0 * scale + 0.5);
    const int left = (width - scene_w) / 2, top = (height - scene_h) / 2;
    if (left > 1 && pixel(rgb, width, 0, height / 2) != LETTERBOX) { fprintf(stderr, "FAIL the letterbox on the left is #%06x\n", pixel(rgb, width, 0, height / 2)); failures++; }
    if (top > 1 && pixel(rgb, width, width / 2, 0) != LETTERBOX) { fprintf(stderr, "FAIL the letterbox at the top is #%06x\n", pixel(rgb, width, width / 2, 0)); failures++; }
    if (pixel(rgb, width, left + scene_w / 2, top + scene_h / 2) == LETTERBOX) { fprintf(stderr, "FAIL the scene's middle is the letterbox\n"); failures++; }
    /* The title "Ages" (or the probe asked for) in its line box, scaled into the image. */
    const char *frame = argc == 7 ? argv[5] : layout == ORACLES_UI_LAYOUT_4_3 ? "10a" : "1a", *probe = argc == 7 ? argv[6] : "hero title";
    const double box_x = layout_reference(frame, probe, "x"), box_y = layout_reference(frame, probe, "top");
    const double box_w = layout_reference(frame, probe, "w"), box_h = layout_reference(frame, probe, "h");
    const int x0 = left + (int)(box_x * scale), y0 = top + (int)(box_y * scale);
    const int x1 = left + (int)((box_x + box_w) * scale), y1 = top + (int)((box_y + box_h) * scale);
    unsigned lit = 0, area = 0;
    for (int y = y0; y < y1 && y < height; y++)
        for (int x = x0; x < x1 && x < width; x++) {
            const unsigned c = pixel(rgb, width, x, y);
            const unsigned luma = (299u * (c >> 16) + 587u * (c >> 8 & 0xffu) + 114u * (c & 0xffu)) / 1000u;
            lit += luma >= TEXT_LUMA;
            area++;
        }
    const double share = area ? (double)lit / (double)area : 0.0;
    if (share < MIN_TEXT_SHARE) { fprintf(stderr, "FAIL %s's %s holds %.1f%% of light pixels: no text written\n", frame, probe, share * 100.0); failures++; }
    if (layout_failures()) failures++;
    free(rgb);
    if (failures) return 1;
    printf("launcher screenshot: %dx%d, %u colours, the scene at %dx%d, %s's %s %.1f%% light\n", width, height, colours, scene_w, scene_h, frame, probe, share * 100.0);
    return 0;
}
