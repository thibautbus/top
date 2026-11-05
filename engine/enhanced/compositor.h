/* Enhanced widescreen compositor.
 *
 * Composes one wide surface from the core's committed image and the host
 * camera.  The surface's size is the view's level and the screen's shape
 * (docs/REFERENCE.md): near, 256x144 or 213x160 in 4:3; medium,
 * 384x216 or 320x240; far, the drawn-back view, 480x270 (`--zoom-out`: four
 * times 1920x1080, three rooms across and two down outdoors) or 480x360, or fitted to a screen those fill badly
 * (426x320, 412x360).  It is a HUD band of sixteen lines at the top, the status bar
 * copied from the core's top sixteen lines and centred, and a world band
 * below it (the rest of the surface), the game area placed by the camera in world
 * coordinates.  Columns the active room does not cover are neighbours,
 * black until the ghost instance fills them (a declared fallback).  The
 * gutters beside the status bar are the border, or with the bar drawn full
 * width (`hud=full`) the bar's own background, line by line; with the bar laid
 * out (`hud=wide`) its blocks stand at the band's corners on that background.
 *
 * Frames the software PPU does not reconstruct, the ring menu and cutscenes
 * (LCD behaviours 2 to 6) and frames where the window is scanned, use the
 * framed fallback: the core's 160x144 image centred in the surface with a
 * border, the artefact ratified for the Enhanced profile.  This is a pure
 * function of pixels and a descriptor; it reads no game state. */
#ifndef ORACLES_ENHANCED_COMPOSITOR_H
#define ORACLES_ENHANCED_COMPOSITOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ORACLES_ENHANCED_HUD_HEIGHT 16u          /* the status bar is the top sixteen lines (docs/GAME_HOOKS.md, section 1) */
#define ORACLES_ENHANCED_AREA_HEIGHT 128u        /* the core's game area below it */
#define ORACLES_ENHANCED_CORE_WIDTH 160u
#define ORACLES_ENHANCED_NARROW_WIDTH 256u       /* the normal surface, and the band the drawn-back view keeps where it does not draw back */
#define ORACLES_ENHANCED_NARROW_HEIGHT 144u
#define ORACLES_ENHANCED_ZOOM_WIDTH 480u         /* the drawn-back view's, the far level's */
#define ORACLES_ENHANCED_ZOOM_HEIGHT 270u
#define ORACLES_ENHANCED_MAX_WIDTH 480u          /* the widest surface */

/* The view's level: how much of the world it shows, the screen's shape giving its size. */
typedef enum OraclesEnhancedLevel {
    ORACLES_ENHANCED_NEAR = 0,     /* the band: a room and a bit outdoors */
    ORACLES_ENHANCED_MEDIUM,       /* two rooms or more across outdoors */
    ORACLES_ENHANCED_FAR,          /* the drawn-back view: three rooms across */
    ORACLES_ENHANCED_LEVELS
} OraclesEnhancedLevel;
typedef enum OraclesEnhancedAspect {
    ORACLES_ENHANCED_16_9 = 0,
    ORACLES_ENHANCED_4_3,
    ORACLES_ENHANCED_ASPECTS
} OraclesEnhancedAspect;

/* The surface's size, chosen when the view starts; the world band is what
 * the HUD band leaves, the status bar is centred in the HUD band.  Each size
 * fills the usual screens of its shape at a whole scale: 1920x1080 (near,
 * seven times with a margin; medium, five; far, four) and 640x480 (near,
 * three; medium, two); far in 4:3, 480x360, fills 960x720 twice and
 * 1440x1080 three times, and shows the far view's three rooms across at the
 * 640x480 of a small console, once, with 90 lines of world more. */
typedef struct OraclesEnhancedSize { unsigned width, height; } OraclesEnhancedSize;
static inline OraclesEnhancedSize oracles_enhanced_view_size(OraclesEnhancedLevel level, OraclesEnhancedAspect aspect)
{
    static const OraclesEnhancedSize sizes[ORACLES_ENHANCED_LEVELS][ORACLES_ENHANCED_ASPECTS] = {
        { { ORACLES_ENHANCED_NARROW_WIDTH, ORACLES_ENHANCED_NARROW_HEIGHT }, { 213u, 160u } },
        { { 384u, 216u }, { 320u, 240u } },
        { { ORACLES_ENHANCED_ZOOM_WIDTH, ORACLES_ENHANCED_ZOOM_HEIGHT }, { ORACLES_ENHANCED_ZOOM_WIDTH, 360u } },
    };
    return sizes[(unsigned)level < ORACLES_ENHANCED_LEVELS ? level : ORACLES_ENHANCED_NEAR][(unsigned)aspect < ORACLES_ENHANCED_ASPECTS ? aspect : ORACLES_ENHANCED_16_9];
}
/* The two sizes before the levels: the band, and the drawn-back view (--zoom-out). */
static inline OraclesEnhancedSize oracles_enhanced_size(int zoom_out)
{
    return oracles_enhanced_view_size(zoom_out ? ORACLES_ENHANCED_FAR : ORACLES_ENHANCED_NEAR, ORACLES_ENHANCED_16_9);
}
static inline unsigned oracles_enhanced_band_height(OraclesEnhancedSize size) { return size.height - ORACLES_ENHANCED_HUD_HEIGHT; }   /* 128, 144, 200, 224, 254, 304 or 344 */
static inline unsigned oracles_enhanced_hud_x(OraclesEnhancedSize size) { return (size.width - ORACLES_ENHANCED_CORE_WIDTH) / 2u; }   /* 48, 26, 112, 80, 133, 126 or 160 */

/* A screen's shape: its long side over its short one (a screen held upright counts as turned) below the middle of
 * 4:3 and 16:9, 1.56, is 4:3; 3:2 and a square screen are 4:3, 16:10 and 5:3 are 16:9. */
static inline OraclesEnhancedAspect oracles_enhanced_screen_aspect(unsigned screen_w, unsigned screen_h)
{
    const unsigned long_side = screen_w > screen_h ? screen_w : screen_h, short_side = screen_w > screen_h ? screen_h : screen_w;
    if (!short_side) return ORACLES_ENHANCED_16_9;
    return (float)long_side / (float)short_side < (4.0f / 3.0f + 16.0f / 9.0f) / 2.0f ? ORACLES_ENHANCED_4_3 : ORACLES_ENHANCED_16_9;
}

/* The far view fitted to the screen.  Where far's size in the screen's shape covers less than four fifths of the
 * screen at its largest whole scale, the smallest whole scale k of 2 or more whose screen/k, each side made even,
 * fits in 480x360 gives the surface, kept if at least 400x224 and as wide as its band is tall (the camera's
 * vertical margin), and if it is one of the sizes the view takes: 426x320 (1280x960 at three times), 412x360
 * (1240x1080).  Else far keeps its size.  The screen counts by its long side across.  None is larger than far in
 * 4:3: the rooms the band can reach, the compositor's bound and the cost stay far's. */
#define ORACLES_ENHANCED_FITTED_SIZES 2u
static inline OraclesEnhancedSize oracles_enhanced_fitted_size(unsigned i)
{
    static const OraclesEnhancedSize sizes[ORACLES_ENHANCED_FITTED_SIZES] = { { 426u, 320u }, { 412u, 360u } };
    return sizes[i < ORACLES_ENHANCED_FITTED_SIZES ? i : 0u];
}
typedef enum OraclesEnhancedFit {
    ORACLES_ENHANCED_FIT_FITTED = 0,   /* the surface is fitted to the screen */
    ORACLES_ENHANCED_FIT_FILLS,        /* far's size covers four fifths of the screen or more */
    ORACLES_ENHANCED_FIT_SMALL,        /* the scale's size is under 400x224, or its band taller than it is wide */
    ORACLES_ENHANCED_FIT_UNLISTED      /* the scale's size is not one the view takes */
} OraclesEnhancedFit;
typedef struct OraclesEnhancedFitting {
    OraclesEnhancedFit fit;
    OraclesEnhancedSize size;          /* the far view's surface on this screen */
    OraclesEnhancedSize computed;      /* the size the scale gives, kept or not ({0, 0} where far's size fills the screen) */
    unsigned scale;                    /* the scale it fills the screen at */
    OraclesEnhancedAspect aspect;      /* the screen's shape */
    unsigned far_scale, far_percent;   /* far's size in that shape: the whole scale the screen holds it at, the part of the screen it covers */
} OraclesEnhancedFitting;
static inline OraclesEnhancedFitting oracles_enhanced_far_fitting(unsigned screen_w, unsigned screen_h)
{
    OraclesEnhancedFitting f = { ORACLES_ENHANCED_FIT_FILLS, { 0u, 0u }, { 0u, 0u }, 0u, oracles_enhanced_screen_aspect(screen_w, screen_h), 0u, 0u };
    const unsigned long long w = screen_w > screen_h ? screen_w : screen_h, h = screen_w > screen_h ? screen_h : screen_w;
    f.size = oracles_enhanced_view_size(ORACLES_ENHANCED_FAR, f.aspect);
    if (!h) return f;
    const unsigned long long across = w / f.size.width, down = h / f.size.height;
    f.far_scale = (unsigned)(across < down ? across : down);
    f.far_percent = (unsigned)(100ull * f.far_scale * f.size.width * f.far_scale * f.size.height / (w * h));
    if (5ull * f.far_scale * f.size.width * f.far_scale * f.size.height >= 4ull * w * h) return f;
    unsigned k = 2u;
    while ((w / k & ~1ull) > ORACLES_ENHANCED_MAX_WIDTH || (h / k & ~1ull) > 360u) k++;
    f.computed = (OraclesEnhancedSize){ (unsigned)(w / k & ~1ull), (unsigned)(h / k & ~1ull) };
    f.scale = k;
    f.fit = ORACLES_ENHANCED_FIT_SMALL;
    if (f.computed.width < 400u || f.computed.height < 224u || f.computed.width < oracles_enhanced_band_height(f.computed)) return f;
    f.fit = ORACLES_ENHANCED_FIT_UNLISTED;
    for (unsigned i = 0; i < ORACLES_ENHANCED_FITTED_SIZES; i++)
        if (oracles_enhanced_fitted_size(i).width == f.computed.width && oracles_enhanced_fitted_size(i).height == f.computed.height) {
            f.fit = ORACLES_ENHANCED_FIT_FITTED;
            f.size = f.computed;
        }
    return f;
}
/* The view's surface at `level` on a screen of screen_w x screen_h, as a game started there takes it: the level's
 * size in the shape `aspect` names, or for ORACLES_ENHANCED_ASPECTS in the screen's, far then fitted to the screen
 * unless `far_fixed` (aspect= and far= in the settings, docs/REFERENCE.md).  The session, the harness and Display's
 * sizes all take it from here. */
static inline OraclesEnhancedSize oracles_enhanced_screen_view_size(OraclesEnhancedLevel level, OraclesEnhancedAspect aspect, int far_fixed,
                                                                    unsigned screen_w, unsigned screen_h)
{
    if ((unsigned)aspect < ORACLES_ENHANCED_ASPECTS) return oracles_enhanced_view_size(level, aspect);
    if (level == ORACLES_ENHANCED_FAR && !far_fixed) return oracles_enhanced_far_fitting(screen_w, screen_h).size;
    return oracles_enhanced_view_size(level, oracles_enhanced_screen_aspect(screen_w, screen_h));
}
/* What the far view takes on a screen and why, for a log. */
void oracles_enhanced_far_fitting_words(unsigned screen_w, unsigned screen_h, char *out, size_t capacity);
/* A size the view takes: a level's in a shape, or the far view fitted to a screen; `fitted` says which. */
static inline int oracles_enhanced_size_known(OraclesEnhancedSize size, int *fitted)
{
    for (unsigned i = 0; i < ORACLES_ENHANCED_FITTED_SIZES; i++)
        if (oracles_enhanced_fitted_size(i).width == size.width && oracles_enhanced_fitted_size(i).height == size.height) { if (fitted) *fitted = 1; return 1; }
    for (unsigned l = 0; l < ORACLES_ENHANCED_LEVELS; l++)
        for (unsigned a = 0; a < ORACLES_ENHANCED_ASPECTS; a++) {
            const OraclesEnhancedSize s = oracles_enhanced_view_size((OraclesEnhancedLevel)l, (OraclesEnhancedAspect)a);
            if (s.width == size.width && s.height == size.height) { if (fitted) *fitted = 0; return 1; }
        }
    return 0;
}

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
    unsigned veil;                                               /* 0: its pixels; 1 to 255: those blended toward the band's black by veil / 256 (a room fading in);
                                                                  * 256: the band's black, its area unread (a room kept dark), and covered; with
                                                                  * `soft_edges`, both less along the sources drawn in clear beside it */
    unsigned edge_veil;                                          /* with `soft_edges`: 0 its edge whole; 1 to 255 its edge's light that far toward none (an edge
                                                                  * coming in); 256 none */
} OraclesEnhancedNeighbour;

/* The edge of a room kept dark beside a room drawn in clear (`soft_edges`): along their frontier, inside the dark room,
 * its own pixels in full over a wave of up to ORACLES_ENHANCED_EDGE_WAVE pixels, smooth from one knot to the next every
 * ORACLES_ENHANCED_EDGE_PERIOD pixels, then ORACLES_ENHANCED_EDGE_STEP pixels at 66 % of their light and as many at
 * 40 %, then the black, each limit following the wave.  At a corner it shares with a room in clear alone, the edge
 * turns round it; where two frontiers meet, the nearer holds.  A side is a frontier where what is drawn beside it is in
 * clear at its middle and at the edge's depth from either end.  Nothing shows ORACLES_ENHANCED_EDGE_DEPTH pixels or
 * more from the dark room's sides.  The wave belongs to the world: a frontier's line and the place along it, with the
 * seed, give it, whatever the camera. */
#define ORACLES_ENHANCED_EDGE_WAVE 5u
#define ORACLES_ENHANCED_EDGE_PERIOD 6u
#define ORACLES_ENHANCED_EDGE_STEP 3u
#define ORACLES_ENHANCED_EDGE_DEPTH (ORACLES_ENHANCED_EDGE_WAVE + 2u * ORACLES_ENHANCED_EDGE_STEP)
/* The pixels in full at `along` (world pixels) on the frontier at `line`, a world row (`vertical` 0, the place a column)
 * or a world column (`vertical` 1, the place a row): 0 to ORACLES_ENHANCED_EDGE_WAVE. */
unsigned oracles_enhanced_edge_wave(uint32_t seed, int32_t line, int vertical, int32_t along);

typedef struct OraclesEnhancedCompose {
    OraclesEnhancedMode mode;
    int32_t world_left, world_top;                               /* world position of the world band's top-left pixel (the camera) */
    int32_t window_world_left, window_world_top;                 /* world position of the core's game-area window (the room's origin plus the game's camera) */
    uint32_t border;                                             /* the gutter and framed-mode border colour */
    int fade;                                                    /* the game's palette fade, -32 (black) to 31 (white), 0 none: applied to every source but the core's window, which carries it already */
    const uint32_t *colours;                                     /* ORACLES_PPU_COLOURS pixels by RGB555 colour, the live pipeline; NULL for a plain expansion */
    const int16_t *line_shift, *line_shift_y;                    /* world mode: the scroll each of the game area's 128 lines was drawn with, less the camera's (the per-line ripples, SCX and SCY), repeated above and below the window; NULL: none */
    int32_t sway;                                                /* world mode: the part of line_shift common to its lines, taken off them (the core's image drawn again without it) */
    const OraclesEnhancedNeighbour *neighbours;                  /* world mode; may be NULL */
    unsigned neighbour_count;
    OraclesEnhancedSize size;                                    /* the surface's; {0, 0}: the normal 256x144 */
    unsigned shown_width, shown_height;                          /* world mode: the part of the band shown, centred, the rest black; 0: all of it */
    int32_t counted_left, counted_top;                           /* world mode: the world rectangle whose uncovered pixels are counted (a large room and the one a scroll enters, */
    unsigned counted_width, counted_height;                      /* the black around them decided); width 0: all of the part shown */
    const uint32_t *hud_fill;                                    /* world mode: the HUD band's gutters line by line (oracles_enhanced_hud_fill), 0 where a line keeps the border; NULL: the border */
    const struct OraclesEnhancedHudPlace *hud_place;             /* world mode: the status bar cut into its blocks and laid out (oracles_enhanced_hud_place); NULL: centred */
    int soft_edges;                                              /* world mode: a source kept dark or fading in takes the edge toward the sources drawn in clear beside it; 0: its veil alone */
    uint32_t edge_seed;                                          /* the world its waves are drawn in (the rooms' group) */
    unsigned *uncovered_edges;                                   /* world mode, not NULL: set to the sides of the part shown whose edge column or line has uncovered pixels counted, bits 0 to 3 left, right, top, bottom */
} OraclesEnhancedCompose;

/* The game's status bar as it keeps it: its map (two rows of 32 cells, tiles and attributes), the LCDC of its lines
 * (the tile data's addressing, the background's priority), the VRAM's two banks, and the background palettes on
 * screen, the game's fade in them. */
typedef struct OraclesEnhancedBar {
    const uint8_t *tiles, *attributes, *vram0, *vram1, *palettes;
    uint8_t lcdc;
} OraclesEnhancedBar;
/* The bar's pixel at (x, y) of its 160x16, as the hardware draws its background: the RGB555 colour of its tile's
 * palette and colour index there, and, when `hides_sprite` is not NULL, whether it hides a sprite (the tile's priority
 * bit and the LCDC's set, its colour index not 0). */
uint16_t oracles_enhanced_bar_pixel(const OraclesEnhancedBar *bar, unsigned x, unsigned y, int *hides_sprite);
/* What the gutters beside the status bar take with the bar drawn full width (`fill`: its sixteen lines on the left,
 * then on the right): on each line the bar's own pixel at its edge, column 0 or 159, its palette's colour on screen
 * (`pixel`, the core's conversion of an RGB555 colour, colour correction included; NULL: a plain expansion), where
 * the core's image (`core`, 160x144) shows that colour there, and 0 where it does not (a sprite on the edge, the top
 * lines not the bar as the game keeps it): that line keeps the border.  A line the image shows in one colour across
 * (the LCD off, a blank bar) extends as it is.  Returns the lines filled, 32 at most. */
unsigned oracles_enhanced_hud_fill(const OraclesEnhancedBar *bar, uint32_t (*pixel)(void *opaque, uint16_t rgb555), void *opaque,
                                   const uint32_t *core, uint32_t fill[2u * ORACLES_ENHANCED_HUD_HEIGHT]);

/* The status bar's layouts, which the game chooses and loads as its map (loadStatusBarMap, docs/GAME_HOOKS.md, section 1):
 * the normal one; the one tightened for a fifteenth and a sixteenth heart, B and A 36 wide each where they are 40, the
 * money and the hearts a column further left, eight hearts a row where they are seven; Biggoron's sword, B and A one
 * frame, the money and the hearts where the hearts put them.  Each is four blocks side by side: B and A, the money (the
 * rupees, a dungeon's keys, Seasons' ore in Subrosia), 24 wide, and the hearts to the bar's right edge. */
typedef enum OraclesEnhancedBarLayout {
    ORACLES_ENHANCED_BAR_NONE = 0,     /* a map that signs none: the bar hidden by a cutscene, or a game's own */
    ORACLES_ENHANCED_BAR_NORMAL,
    ORACLES_ENHANCED_BAR_TIGHT,
    ORACLES_ENHANCED_BAR_BIGGORON
} OraclesEnhancedBarLayout;
#define ORACLES_ENHANCED_BAR_BUTTONS 80u         /* B and A's width: 40 each */
#define ORACLES_ENHANCED_BAR_BUTTONS_TIGHT 72u   /* 36 each, the hearts tightened */
#define ORACLES_ENHANCED_BAR_MONEY 24u
/* The layout the bar's map signs: the cells of its frame as the game loads them, tile and attributes. */
OraclesEnhancedBarLayout oracles_enhanced_bar_layout(const OraclesEnhancedBar *bar);
/* Whether the core's image (160x144) shows `layout`'s frame, its cells drawn from the bar's VRAM and palettes and
 * converted by `pixel` (as for oracles_enhanced_hud_fill), pixel for pixel. */
int oracles_enhanced_bar_layout_shown(const OraclesEnhancedBar *bar, OraclesEnhancedBarLayout layout, uint32_t (*pixel)(void *opaque, uint16_t rgb555),
                                      void *opaque, const uint32_t *core);

/* The status bar cut into its blocks and laid out across the HUD band (`hud=wide`): the hearts at the left edge, the
 * money after them, B and A against the right edge, the hotbar's slots against B where they fit; the rest of the band
 * the bar's background, as full width paints it.  The hearts' two rows stand side by side on the band's middle eight
 * lines, the game's cells as they are, where the band holds them with the hotbar's width (`hotbar`) beside, whether the
 * hotbar is drawn or not.  The blocks keep a margin from the band's edges, eight pixels each side, or what the band
 * leaves beside them and the hotbar's width where it fits: none in near 16:9, four in medium 4:3, none there with the
 * hearts tightened.  Each block is the core's own pixels: icons, counts, fades and a sprite over them follow. */
#define ORACLES_ENHANCED_HUD_MARGIN 8u
typedef struct OraclesEnhancedHudPlace {
    unsigned buttons;                  /* B and A's width, the bar's columns from 0: 80, or 72 tightened */
    unsigned hearts;                   /* the hearts' width, the bar's last columns: 56 or 64, two rows of seven or eight cells */
    int one_row;                       /* the hearts' rows side by side */
    unsigned margin;                   /* the band's columns left at each edge */
    unsigned hearts_at, money_at, buttons_at;   /* the band's columns of the hearts, of the money and of B and A */
    int hotbar;                        /* the hotbar's slots fit between the money and B, */
    unsigned hotbar_at;                /* from this column */
} OraclesEnhancedHudPlace;
static inline OraclesEnhancedHudPlace oracles_enhanced_hud_place(unsigned width, unsigned buttons, unsigned hotbar)
{
    OraclesEnhancedHudPlace p;
    p.buttons = buttons;
    p.hearts = ORACLES_ENHANCED_CORE_WIDTH - buttons - ORACLES_ENHANCED_BAR_MONEY;
    p.one_row = width >= 2u * p.hearts + ORACLES_ENHANCED_BAR_MONEY + hotbar + buttons;   /* 312 wide, 320 tightened */
    const unsigned left = (p.one_row ? 2u : 1u) * p.hearts + ORACLES_ENHANCED_BAR_MONEY;
    p.hotbar = width >= left + hotbar + buttons;
    const unsigned blocks = left + (p.hotbar ? hotbar : 0u) + buttons, room = width > blocks ? (width - blocks) / 2u : 0u;
    p.margin = room < ORACLES_ENHANCED_HUD_MARGIN ? room : ORACLES_ENHANCED_HUD_MARGIN;
    p.hearts_at = p.margin;
    p.money_at = p.margin + left - ORACLES_ENHANCED_BAR_MONEY;
    p.buttons_at = width - p.margin - buttons;
    p.hotbar_at = p.buttons_at - hotbar;
    return p;
}

/* Writes size.width * size.height pixels of 0xffRRGGBB. `core` is the 160x144 image
 * of the core (or the software PPU): its top sixteen lines are the status
 * bar, its bottom 128 the game area of the active room.  Returns the number
 * of world-band pixels no source covered (black), 0 in framed mode. */
unsigned oracles_enhanced_compose(const uint32_t *core, const OraclesEnhancedCompose *in, uint32_t *out);
/* The sources `in` keeps dark (veil 256) that `out`, the surface it composed, shows in anything but the band's black at
 * ORACLES_ENHANCED_EDGE_DEPTH pixels or more from each of their sides, where neither a source before them nor the core's
 * window covers them: a room drawn in clear past its edge.  Read on the line at that depth inside each side and on the
 * room's middle row and column. */
unsigned oracles_enhanced_dark_lit(const OraclesEnhancedCompose *in, const uint32_t *out);
/* The sources `in` keeps dark that `out` shows in anything but the band's black on their first line inside a side, the
 * edge's depth from either end aside, where nothing drawn in clear stands just beyond it (another room kept dark, nothing,
 * the world's end): an edge where there is no frontier. */
unsigned oracles_enhanced_dark_stray(const OraclesEnhancedCompose *in, const uint32_t *out);
/* The HUD band alone, the world mode's sixteen lines at the top of `out`, the rest left as it is. */
void oracles_enhanced_compose_hud(const uint32_t *core, const OraclesEnhancedCompose *in, uint32_t *out);
/* An RGB555 colour of a source that does not carry the game's palette fade, as the compose draws it: the fade
 * (-32 to 31) applied as the game applies it, then the pixel of `colours` (NULL: plain RGB). */
uint32_t oracles_enhanced_faded_pixel(const uint32_t *colours, uint32_t rgb555, int fade);

#ifdef __cplusplus
}
#endif

#endif
