/* Enhanced widescreen compositor.
 *
 * Composes one wide surface from the core's committed image and the host
 * camera.  The surface is 256x144, or 480x270 in the drawn-back view
 * (`--zoom-out`: four times 1920x1080, three rooms across and two
 * down outdoors): a HUD band of sixteen lines at the top, the status bar
 * copied from the core's top sixteen lines and centred, and a world band
 * below it (128 lines, or 254), the game area placed by the camera in world
 * coordinates.  Columns the active room does not cover are neighbours,
 * black until the ghost instance fills them (a declared fallback).
 *
 * Frames the software PPU does not reconstruct, the ring menu and cutscenes
 * (LCD behaviours 2 to 6) and frames where the window is scanned, use the
 * framed fallback: the core's 160x144 image centred in the surface with a
 * border, the artefact ratified for the Enhanced profile.  This is a pure
 * function of pixels and a descriptor; it reads no game state. */
#ifndef ORACLES_ENHANCED_COMPOSITOR_H
#define ORACLES_ENHANCED_COMPOSITOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ORACLES_ENHANCED_HUD_HEIGHT 16u          /* the status bar is the top sixteen lines (docs/GAME_HOOKS.md, section 1) */
#define ORACLES_ENHANCED_AREA_HEIGHT 128u        /* the core's game area below it */
#define ORACLES_ENHANCED_CORE_WIDTH 160u
#define ORACLES_ENHANCED_NARROW_WIDTH 256u       /* the normal surface, and the band the drawn-back view keeps where it does not draw back */
#define ORACLES_ENHANCED_NARROW_HEIGHT 144u
#define ORACLES_ENHANCED_ZOOM_WIDTH 480u         /* the drawn-back view's */
#define ORACLES_ENHANCED_ZOOM_HEIGHT 270u

/* The surface's size, chosen when the view starts; the world band is what
 * the HUD band leaves, the status bar is centred in the HUD band. */
typedef struct OraclesEnhancedSize { unsigned width, height; } OraclesEnhancedSize;
static inline OraclesEnhancedSize oracles_enhanced_size(int zoom_out)
{
    const OraclesEnhancedSize narrow = { ORACLES_ENHANCED_NARROW_WIDTH, ORACLES_ENHANCED_NARROW_HEIGHT }, zoom = { ORACLES_ENHANCED_ZOOM_WIDTH, ORACLES_ENHANCED_ZOOM_HEIGHT };
    return zoom_out ? zoom : narrow;
}
static inline unsigned oracles_enhanced_band_height(OraclesEnhancedSize size) { return size.height - ORACLES_ENHANCED_HUD_HEIGHT; }   /* 128, or 254 */
static inline unsigned oracles_enhanced_hud_x(OraclesEnhancedSize size) { return (size.width - ORACLES_ENHANCED_CORE_WIDTH) / 2u; }   /* 48, or 160 */

typedef enum OraclesEnhancedMode {
    ORACLES_ENHANCED_WORLD = 0,   /* the wide world band, HUD centred, neighbours black */
    ORACLES_ENHANCED_FRAMED       /* the core's 160x144 image centred with a border */
} OraclesEnhancedMode;

/* One neighbour's committed game area, its top-left corner in world pixels. */
typedef struct OraclesEnhancedNeighbour {
    int32_t world_left, world_top;                               /* world position of its pixel (0,0) */
    const uint32_t *game_area;                                   /* width x height, or NULL for black */
    unsigned width, height;                                      /* 0: 160, 0: 128 */
    int faded;                                                   /* 1: 0xffRRGGBB pixels rendered with the game's fade already; 0: RGB555 colours, faded by the compose and converted through `colours` */
} OraclesEnhancedNeighbour;

typedef struct OraclesEnhancedCompose {
    OraclesEnhancedMode mode;
    int32_t world_left, world_top;                               /* world position of the world band's top-left pixel (the camera) */
    int32_t window_world_left, window_world_top;                 /* world position of the core's game-area window (the room's origin plus the game's camera) */
    uint32_t border;                                             /* the gutter and framed-mode border colour */
    int fade;                                                    /* the game's palette fade, -32 (black) to 31 (white), 0 none: applied to every source but the core's window, which carries it already */
    const uint32_t *colours;                                     /* ORACLES_PPU_COLOURS pixels by RGB555 colour, the live pipeline; NULL for a plain expansion */
    const int16_t *line_shift, *line_shift_y;                    /* world mode: the scroll each of the game area's 128 lines was drawn with, less the camera's (the per-line ripples, SCX and SCY), repeated above and below the window; NULL: none */
    const OraclesEnhancedNeighbour *neighbours;                  /* world mode; may be NULL */
    unsigned neighbour_count;
    OraclesEnhancedSize size;                                    /* the surface's; {0, 0}: the normal 256x144 */
    unsigned shown_width, shown_height;                          /* world mode: the part of the band shown, centred, the rest black; 0: all of it */
    int32_t counted_left, counted_top;                           /* world mode: the world rectangle whose uncovered pixels are counted (a large room and the one a scroll enters, */
    unsigned counted_width, counted_height;                      /* the black around them decided); width 0: all of the part shown */
} OraclesEnhancedCompose;

/* Writes size.width * size.height pixels of 0xffRRGGBB. `core` is the 160x144 image
 * of the core (or the software PPU): its top sixteen lines are the status
 * bar, its bottom 128 the game area of the active room.  Returns the number
 * of world-band pixels no source covered (black), 0 in framed mode. */
unsigned oracles_enhanced_compose(const uint32_t *core, const OraclesEnhancedCompose *in, uint32_t *out);

#ifdef __cplusplus
}
#endif

#endif
