/* Enhanced view: the wide surface for a running game.
 *
 * Ties the camera, the compositor and the ghost instance to a core and its
 * guest.  Each frame it observes the guest (Link's world position, the
 * game's window, the seam of a scrolling transition), advances the camera,
 * picks the mode (the wide world band in play, the framed fallback for the
 * ring menu, cutscenes, window frames and everything outside play), and
 * composes the surface from the core's committed image and the terrain of
 * the cached rooms around it.  The view reads the register journal of the
 * frame (to place the game's window where the frame was drawn) and never
 * clears it: the launcher or the harness clears it at the frame's end.
 *
 * Neighbours come from the ghost instance: the rooms around the
 * reference room (the four beside it, the two beyond left and right, the
 * diagonals) are loaded by the game itself in the ghost, settled past their
 * scrolling transition, and rendered without objects; the terrain is cached
 * under the bytes of the cache key (`oracles_ghost_key_ranges`) its
 * substitutions read, and invalidated when one of them changes in the live
 * instance.  On the overworlds and Ages' open sea every room of the grid is
 * a neighbour; in an interior only a room the reference room opens onto; a
 * large room (a dungeon) shows the room a scroll enters and nothing else;
 * the map edges are black, never false terrain (view_map.c).  The ghost runs
 * in a worker thread in play, or synchronously with a fixed budget of guest
 * frames per host frame on validation routes, where the composition must be
 * reproducible.
 *
 * The view never writes to the live guest. */
#ifndef ORACLES_ENHANCED_VIEW_H
#define ORACLES_ENHANCED_VIEW_H

#include "camera.h"
#include "compositor.h"
#include "core.h"
#include "guest.h"
#include "animation.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OraclesEnhancedView OraclesEnhancedView;

/* Pure: whether this frame still holds a dialogue's text box.  The game
 * clears wTextIsActive one frame before it restores the room's cells under
 * the box, and the frame on screen still holds it: the view (and what the
 * harness checks) keeps the box while its cells are still in the displayed
 * map, two frames at most, so that it is not shown where the game drew it
 * instead of where the band puts it.  `active` is the game's flag, `cells`
 * the cells of the displayed map that differ from the room's own, and
 * `trail` the count kept between frames (0 to start). */
#define ORACLES_ENHANCED_TEXT_BOX_TRAIL_FRAMES 2u
int oracles_enhanced_text_box_holds(int active, unsigned cells, unsigned *trail);

/* The text box drawn by the host (text_box.h), with the guest's host lines armed (oracles_guest_arm_text_box): while a
 * text of the standard box or its choice shows (wTextIsActive, the trail of the frames after it, wTextDisplayMode 0 or
 * 1), the game's box where the game's variables put it in the VRAM's map (w7TextboxVramPos, w7d0cc), read into a model
 * from the displayed map and drawn from that model, the host's lines as the frame just scanned showed them and the
 * game's tiles, at `pixels` of text and `lines` lines.  1 drawn into `box`, 0 no box shown, -1 a box the host could not
 * draw (`box->read` says why).  Nothing is drawn in the surface. */
typedef struct OraclesEnhancedHostTextBox {
    const uint32_t *pixels;     /* the box drawn, `width` pixels a row, 0xffRRGGBB */
    unsigned width, height;
    unsigned map;               /* the VRAM's map it stands in, from $8000: $1800 or $1c00 */
    unsigned row, column;       /* its first cell in that map */
    int read;                   /* OraclesTextBoxRead */
} OraclesEnhancedHostTextBox;
int oracles_enhanced_view_host_text_box(OraclesEnhancedView *view, unsigned pixels, unsigned lines, OraclesEnhancedHostTextBox *box);
/* The box the last call drew, drawn again from the same model and lines at `pixels` and `lines` (its place in the map
 * left out): 1, or 0 when that call drew none.  Past the game's 128 pixels, the rows' reveal is read in the text's
 * state (oracles_text_box_reveal), the game's map showing a line's first 128 pixels alone. */
int oracles_enhanced_view_host_text_box_again(OraclesEnhancedView *view, unsigned pixels, unsigned lines, OraclesEnhancedHostTextBox *box);
/* The wide text box: the host draws the box in the surface at `pixels` of text (more than the game's 128, the guest's
 * wide box armed at that width, oracles_guest_arm_wide_text_box) and `lines` lines, in place of the core's: in the
 * world band, centred across it at the height the game drew its box at, the room redrawn under the core's box; over the
 * framed core, at the game's box's place and centred on it, past the frame's edges (the window left over it); the large
 * menus' rectangle (oracles_enhanced_view_framed_crop) the union of the frame and the box.  A text whose box the host
 * cannot draw shows the core's, counted.  0 pixels: the core's box (the default). */
void oracles_enhanced_view_set_text_box(OraclesEnhancedView *view, unsigned pixels, unsigned lines);
/* Whether the last composition drew the host's box, the box and its place in the surface (its top-left pixel). */
int oracles_enhanced_view_text_box_shown(const OraclesEnhancedView *view, OraclesEnhancedHostTextBox *box, int32_t *x, int32_t *y);
/* Compositions that drew it, in the world band and over the framed core, and those of a text whose box it could not
 * draw. */
void oracles_enhanced_view_text_box_counts(const OraclesEnhancedView *view, unsigned *world, unsigned *framed, unsigned *uncovered);
/* Text size, presentation alone, applied at once, each size where the box enlarged fits the view, else the next size
 * down, the view's level changing in play; the fan games keep the game's box at its size (Normal whatever is asked):
 *   - Very large (ORACLES_ENHANCED_TEXT_XLARGE): the text box twice as large in the surface where the view holds it so
 *     across (the game's box, 288 pixels: Medium and Far; the host's wide box, 416: Far); the game's box enlarged from
 *     the core's image, without the host's lines (the classic text in the game's box still hooks nothing), the wide box
 *     drawn from the host's lines twice as large; centred across the band (in the world) or on the game's box (framed),
 *     against the edge of the screen the game drew its box against;
 *   - Large (ORACLES_ENHANCED_TEXT_LARGE): the box at its size in the surface, which the presenter shows again above the
 *     frame at k + ceil(k / 2) screen pixels a pixel, k the frame's whole scale (oracles_text_box_enlarge), where that
 *     fits: the part of the frame it takes, its edge and its size, oracles_enhanced_view_text_box_enlarged;
 *   - with the large menus (oracles_enhanced_view_set_large_menus), over the framed core both sizes are the
 *     presenter's: the box at its size in the surface, Very large at 2 k, and the part shown alone widened for the box
 *     only as far as its own scale holds (oracles_text_box_reserve), never lowered. */
#define ORACLES_ENHANCED_TEXT_NORMAL 0u
#define ORACLES_ENHANCED_TEXT_LARGE 1u
#define ORACLES_ENHANCED_TEXT_XLARGE 2u
void oracles_enhanced_view_set_text_size(OraclesEnhancedView *view, unsigned size);
unsigned oracles_enhanced_view_text_size(const OraclesEnhancedView *view);
/* The framed core shown alone at its own scale (menus=large, oracles_enhanced_view_framed_crop as the host's frame_crop). */
void oracles_enhanced_view_set_large_menus(OraclesEnhancedView *view, int large);
/* The host's frame box (oracles_host_run_config's frame_box): 1 when the last composition drew a text box at its size for
 * the presenter to enlarge, its place in the surface (x, y, width, height) and whether it stands against its bottom edge;
 * and whatever it drew, with Large or Very large, the width of the box (the game's 144, the wide 208) the large menus'
 * part takes in and the size (ORACLES_ENHANCED_TEXT_LARGE or XLARGE) it is shown at. */
int oracles_enhanced_view_text_box_enlarged(void *opaque, uint32_t rect[4], int *bottom, uint32_t *reserve_w, unsigned *size);
/* The same, and for a check the core image's pixels it shows (`src`, the game's box; 0 for the host's box). */
int oracles_enhanced_view_text_box_enlarged_source(const OraclesEnhancedView *view, int32_t src[2], int *from_core);
/* The last composition drew the game's box twice as large from the core's image: its top-left in the surface (`dst`),
 * the core image's pixels it enlarged, from `src`, `size` across and down; 0 otherwise. */
int oracles_enhanced_view_text_box_scaled(const OraclesEnhancedView *view, int32_t dst[2], int32_t src[2], unsigned size[2]);
/* The scale the last composition drew the host's box at, 0 for none. */
unsigned oracles_enhanced_view_text_box_shown_scale(const OraclesEnhancedView *view);
/* Compositions that drew a box twice as large, in the world band and over the framed core. */
void oracles_enhanced_view_text_scale_counts(const OraclesEnhancedView *view, unsigned *world, unsigned *framed);
/* The model and the source the host's box was drawn from last (text_box.h), for a check. */
struct OraclesTextBoxModel;
struct OraclesTextBoxSource;
const struct OraclesTextBoxModel *oracles_enhanced_view_text_box_model(const OraclesEnhancedView *view, const struct OraclesTextBoxSource **source);

/* `rom` is copied by the ghost's core; it may be freed afterwards. */
OraclesEnhancedView *oracles_enhanced_view_start(OraclesCore *core, OraclesGuest *guest,
                                                 const uint8_t *rom, size_t rom_size);
void oracles_enhanced_view_stop(OraclesEnhancedView *view);

/* The surface: 256x144, or 480x270 in the drawn-back view (`--zoom-out`, the far 16:9 size),
 * which outdoors shows three rooms across and two down, and keeps
 * the normal band elsewhere, centred; set_size takes the view's other sizes.  Chosen before the first composition. */
void oracles_enhanced_view_set_zoom_out(OraclesEnhancedView *view, int enabled);
/* The surface's size: a level in the screen's shape (compositor.h, oracles_enhanced_view_size); the same moment as
 * set_zoom_out, which is the near or the far 16:9 size.  A size wider than the normal band draws back. */
void oracles_enhanced_view_set_size(OraclesEnhancedView *view, OraclesEnhancedSize size);
uint32_t oracles_enhanced_view_width(const OraclesEnhancedView *view);
uint32_t oracles_enhanced_view_height(const OraclesEnhancedView *view);

/* 0 (default): the ghost runs in a worker thread.  N > 0: the ghost runs
 * synchronously, N guest frames per composed frame (validation routes). */
void oracles_enhanced_view_set_sync_budget(OraclesEnhancedView *view, unsigned frames_per_host_frame);
/* Ghosts working at once, each on its own thread with its own run in flight (1 or 2), before the first composition and
 * after set_sync_budget: a synchronous view keeps one.  Returns the count it has. */
int oracles_enhanced_view_set_ghosts(OraclesEnhancedView *view, unsigned count);
unsigned oracles_enhanced_view_ghosts(const OraclesEnhancedView *view);

/* The camera's pinned configuration: 1 (dead zone of sixteen pixels) or 2
 * (a continuous, gentler follow).  Restarts the camera. */
void oracles_enhanced_view_set_camera_profile(OraclesEnhancedView *view, unsigned profile);

/* Draw the objects of the neighbouring rooms, captured frozen where the game
 * creates them: off by default.  The objects placed at random on
 * entry and those another object created are never drawn, their place at the
 * real entry not being the one the ghost found. */
void oracles_enhanced_view_set_neighbour_objects(OraclesEnhancedView *view, int enabled);
/* Walking diagonally at the same speed both ways, Link's sprite drawn up to a pixel off the game's, on the line his y
 * would take stepping on the frames his x does, and the camera following him there: the band moves along the
 * diagonal without the zigzag of his gait.  Presentation only.  Off by default. */
void oracles_enhanced_view_set_link_phase(OraclesEnhancedView *view, int enabled);
/* This frame's: Link drawn that many pixels below the game's sprite; frames drawn so, frames the phase was in use. */
int oracles_enhanced_view_link_phase_dy(const OraclesEnhancedView *view);
void oracles_enhanced_view_link_phase_counts(const OraclesEnhancedView *view, unsigned *moved_frames, unsigned *engaged_frames);
/* The sub-pixel camera: the camera is the reducer's smooth position, to the 256th of a pixel, and the band is moved
 * by its fraction of a pixel when it is shown, at the screen's resolution; Link and what he holds are drawn apart, at
 * his exact place (his position in 256ths of a pixel), so that he does not shake on the screen.  The surface keeps the camera's whole pixels.  Presentation
 * only.  Off by default; it sets the link phase aside. */
void oracles_enhanced_view_set_subpixel(OraclesEnhancedView *view, int enabled);
/* The same during a game (Display › Advanced's Camera from the pause), without a jolt: switched on, the band and Link
 * take their fractions up from the whole pixels by a sixteenth of a pixel a frame; off, they give them back so,
 * the camera's whole pixels coming back once they have.  On (1) or off (0) as it will be. */
void oracles_enhanced_view_switch_subpixel(OraclesEnhancedView *view, int enabled);
int oracles_enhanced_view_subpixel_on(const OraclesEnhancedView *view);
/* This frame's (the frame just composed): 1 when the band is to be moved, by `x` and `y` 256ths of a pixel left and
 * up (negative: right and down), its lines `band_top` to `band_top + band_height` of the surface, or Link drawn apart;
 * Link's layer (0 alpha where it shows nothing) at `layer_x`, `layer_y` of the surface moved `layer_fx`, `layer_fy`
 * 256ths of a pixel right and down, NULL when it has none.  The surface keeps him: a frame shown as it is shows him at
 * his whole pixels. */
int oracles_enhanced_view_subpixel(const OraclesEnhancedView *view, int32_t *x, int32_t *y, uint32_t *band_top, uint32_t *band_height,
                                   const uint32_t **layer, int32_t *layer_x, int32_t *layer_y, uint32_t *layer_w, uint32_t *layer_h,
                                   int32_t *layer_fx, int32_t *layer_fy);
/* Frames moved by a fraction, frames with Link drawn apart, frames it was refused (Link not wholly in the game's
 * window, or too large). */
/* Its layer's lines are this many pixels apart. */
#define ORACLES_ENHANCED_SUBPIXEL_LAYER_PITCH 64u
void oracles_enhanced_view_subpixel_counts(const OraclesEnhancedView *view, unsigned *frames, unsigned *layer_frames, unsigned *refused_frames);
/* The checks of Link's layer over the run, where it is used: pixels the frame showed in his area, outside his sprites
 * and off the tiles the game animates, that the window rendered again without them does not show (a drawing of his
 * the patch would leave behind; a frame with any is left to the band, oracles_enhanced_view_subpixel_unexplained_frames),
 * and pixels where the layer over the band at the whole pixels is not the frame as the game drew it; and the pixels of
 * his sprites that had the colour of the background under them (which a layer of the pixels that differ would leave
 * behind). */
void oracles_enhanced_view_subpixel_pixel_counts(const OraclesEnhancedView *view, unsigned *left_pixels, unsigned *wrong_pixels, unsigned *alike_pixels);
/* With the layer, the window without him where it shows him (0 alpha elsewhere), the layer's size and place, to draw
 * over the band moved with it before the layer; NULL without a layer. */
const uint32_t *oracles_enhanced_view_subpixel_patch(const OraclesEnhancedView *view);
/* The rectangles of the surface shown unmoved over the band moved and the layer (x, y, w, h): what the view draws
 * standing on the screen, the text box and the area's name; their count. */
#define ORACLES_ENHANCED_SUBPIXEL_FIXED 16u
unsigned oracles_enhanced_view_subpixel_fixed(const OraclesEnhancedView *view, const uint32_t (**rects)[4]);
/* His pixels another drawing covered once the layer was taken (the text box, the area's name), left to the band. */
unsigned oracles_enhanced_view_subpixel_covered_pixels(const OraclesEnhancedView *view);
/* Frames Link was left to the band because the window rendered again without him did not show the frame around him
 * (in a scroll, the next room's lines written into the VRAM since the frame was drawn). */
unsigned oracles_enhanced_view_subpixel_unexplained_frames(const OraclesEnhancedView *view);
/* The OAM entries this frame's layer took (bit k: entry k), none without a layer; and over the run, the special objects
 * at a fraction of their own (a companion waiting, a bomb laid) left to the band, an object a frame. */
uint64_t oracles_enhanced_view_subpixel_taken(const OraclesEnhancedView *view);
unsigned oracles_enhanced_view_subpixel_objects_left(const OraclesEnhancedView *view);

/* The hotbar of the item hotkeys, in the gutters of the HUD band.  The launcher hands the view what
 * only it knows, read-only: the slots, their keys, the slot whose request waits, and a count of refusals a slot, whose
 * rise turns the slot's frame red for twelve frames.  The view reads the rest in the game: the button an item sits on,
 * whether it is in the inventory, the inventory's cursor, and the icons, which are the game's own, captured from its
 * status bar.  NULL hides the hotbar; engine/enhanced does not depend on engine/gameplay. */
#define ORACLES_ENHANCED_HOTBAR_SLOTS 4u
typedef struct OraclesEnhancedHotbarSlot {
    uint8_t set, item, variant;      /* variant 0xff: none */
    char key[3];                     /* two characters at most: A, S, LB */
    uint8_t pending;
    unsigned refusals;
} OraclesEnhancedHotbarSlot;
typedef struct OraclesEnhancedHotbar { OraclesEnhancedHotbarSlot slots[ORACLES_ENHANCED_HOTBAR_SLOTS]; } OraclesEnhancedHotbar;
void oracles_enhanced_view_set_hotbar(OraclesEnhancedView *view, const OraclesEnhancedHotbar *hotbar);
/* Icons captured from the game's status bar so far, and slots drawn with an icon at the last composition. */
void oracles_enhanced_view_hotbar_counts(const OraclesEnhancedView *view, unsigned *icons_captured, unsigned *icons_shown);
/* The distinct icons found in the ROM's tables, as the status bar would draw them on a button, before any was on one,
 * and the times one was found again, its variables possibly changed (the bar reloading its icons, a treasure given). */
unsigned oracles_enhanced_view_hotbar_decoded(const OraclesEnhancedView *view);
unsigned oracles_enhanced_view_hotbar_refound(const OraclesEnhancedView *view);
/* The items in the game's look checked against the bar's own B or A while there, once a stay, and those that differed. */
void oracles_enhanced_view_hotbar_pictures(const OraclesEnhancedView *view, unsigned *checked, unsigned *wrong);
/* What the harness checks of the hotbar: at each sound vblank the icon of an item sitting on a button is set against the
 * core's own image where the status bar draws it, opaque pixel for opaque pixel, and the surface outside the four
 * slots is hashed before and after they are drawn.  `checked` icons, of which `wrong` differ; `outside` frames where
 * the drawing touched anything else. */
void oracles_enhanced_view_hotbar_verify(OraclesEnhancedView *view, int enabled);
void oracles_enhanced_view_hotbar_proof(const OraclesEnhancedView *view, unsigned *checked, unsigned *wrong, unsigned *outside);
/* Of those checks, the ones made in a fade or a flash, each frame, of the icons of B and A against the bar's own. */
unsigned oracles_enhanced_view_hotbar_fade_checked(const OraclesEnhancedView *view);
/* The frame just composed drew the hotbar. */
int oracles_enhanced_view_hotbar_drawn(const OraclesEnhancedView *view);
/* The core's image shows the game's status bar: a layout its map signs, drawn as it signs it, neither it nor the area
 * under it one colour; read through `guest`, an attached one or a reader (oracles_guest_reader), whatever the profile
 * shows. */
int oracles_enhanced_bar_drawn(OraclesGuest *guest, OraclesCore *core);
/* And in play: outside the game's cutscenes (its cutscene index past 1), its menus (the inventory keeps the bar) and its
 * dialogues.  The launcher's reminder over a game waits for it, ORACLES_ENHANCED_BAR_SETTLE frames in a row: a flash of
 * the bar before a cutscene does not start it. */
#define ORACLES_ENHANCED_BAR_SETTLE 120u
int oracles_enhanced_bar_in_play(OraclesGuest *guest, OraclesCore *core);
/* The status bar in the HUD band (`hud=`).  Drawn full width, the gutters beside it, on its sixteen lines, take the
 * bar's own background, its palette's colour at its edge as the game shows it (fades, its own changes and the colour
 * correction follow), in the world band and a room's load drawn under it; the framed core keeps the border.  Laid out,
 * the bar is cut into its blocks on that background (OraclesEnhancedHudPlace): the hearts and the money at the left
 * corner, the hotbar then B and A at the right, wherever the game's map signs one of its layouts and the core's image
 * shows it; else, and in the framed core, the bar stays centred.  Original by default (the border).  It may change
 * between two compositions: it is presentation alone.  The counts: world frames that filled a line at least, and the
 * gutters' lines that kept the border in every world frame with the bar, filled or not, the left's and the right's
 * sixteen each (32 a frame at most), the core's image showing there something else than the bar; laid out, the world
 * frames laid out, those left centred, and those with the hearts on one row. */
typedef enum OraclesEnhancedHud { ORACLES_ENHANCED_HUD_ORIGINAL = 0, ORACLES_ENHANCED_HUD_FULL, ORACLES_ENHANCED_HUD_WIDE } OraclesEnhancedHud;
void oracles_enhanced_view_set_hud(OraclesEnhancedView *view, int hud);
int oracles_enhanced_view_hud(const OraclesEnhancedView *view);
/* The last composed surface's HUD band drawn again for the status bar's setting now, the game not having run since (a
 * change from the pause): 1 when it was, a world frame or a room's load under the band; 0 for a framed frame, whose bar
 * stays centred whatever the setting. */
int oracles_enhanced_view_redraw_hud(OraclesEnhancedView *view);
void oracles_enhanced_view_hud_counts(const OraclesEnhancedView *view, unsigned *frames, unsigned *lines_kept);
void oracles_enhanced_view_hud_wide_counts(const OraclesEnhancedView *view, unsigned *laid, unsigned *centred, unsigned *one_row);
/* A room's load (a door, stairs) drawn under the HUD band the world frames draw, the game area alone where the world
 * shows it: whether the last composition drew one, and the surface's place of the game area's top-left pixel there or in
 * the last world frame.  The counts: the load's frames drawn so, and those left framed whole for showing one colour
 * (the LCD off, a full fade). */
int oracles_enhanced_view_load_band(const OraclesEnhancedView *view, int32_t *area_left, int32_t *area_top);
void oracles_enhanced_view_load_counts(const OraclesEnhancedView *view, unsigned *band_frames, unsigned *blank_frames);
/* Frames composed under a scene's tint of the game's screen, which the other rooms took, and the first and last of them. */
void oracles_enhanced_view_scene_tint_counts(const OraclesEnhancedView *view, unsigned *frames, uint32_t *first, uint32_t *last);
/* Frames that drew the fade of objects appearing on entry, and objects that started one. */
void oracles_enhanced_view_fade_counts(const OraclesEnhancedView *view, unsigned *frames, unsigned *objects);
/* Frames that drew the fade of objects an image of a room left showed, once replaced, and objects that started one. */
void oracles_enhanced_view_fade_out_counts(const OraclesEnhancedView *view, unsigned *frames, unsigned *objects);
/* The hand-off of the objects a capture showed, on the data: objects followed,
 * live twins that appeared elsewhere (jumps), frames an object overlapped the
 * game's window before its twin was visible (holes), objects never seen. */
void oracles_enhanced_view_handoff_counts(const OraclesEnhancedView *view, unsigned *shown, unsigned *jumps, unsigned *hole_frames, unsigned *unseen);

/* The wide world, or the framed core for every frame (F3): a switch between
 * the two looks of the profile without a restart. */
void oracles_enhanced_view_toggle(OraclesEnhancedView *view);
int oracles_enhanced_view_framed_only(const OraclesEnhancedView *view);

/* Advances the camera and the neighbours for this frame and returns the
 * composed surface (width*height 0xffRRGGBB).  `mode`, `camera_x` and
 * `camera_y`, when not NULL, receive the mode chosen and the world position
 * of the world band's top-left pixel. */
const uint32_t *oracles_enhanced_view_compose(OraclesEnhancedView *view, OraclesEnhancedMode *mode, int32_t *camera_x, int32_t *camera_y);

/* The frame source signature the host expects (advances one frame). */
const uint32_t *oracles_enhanced_view_frame_source(void *opaque);
/* The host's frame crop (oracles_host_run_config): 1 when the last composition showed the core framed (the game's menus,
 * its map, its cutscenes, or F3), with the core's 160x144 rectangle in the surface; 0 for a room's load framed, which
 * stays at the view's scale. */
int oracles_enhanced_view_framed_crop(void *opaque, uint32_t rect[4]);
/* The last composed surface, without advancing (a screenshot at exit). */
const uint32_t *oracles_enhanced_view_surface(const OraclesEnhancedView *view);
/* The observation of the last composed frame (Link's world position, the window). */
const OraclesEnhancedObservation *oracles_enhanced_view_observation(const OraclesEnhancedView *view);

/* The neighbour of the last composed frame in a direction (the game's: 0 up,
 * 1 right, 2 down, 3 left): whether its terrain was shown, and which room it
 * is: the one drawn (beside a room the game routes itself, the answer for
 * that direction), else the grid's (0xff when there is none on the map). */
void oracles_enhanced_view_neighbour(const OraclesEnhancedView *view, unsigned direction, int *shown, uint8_t *room);
/* Counters over the view's life: ghost runs requested, completed, failed;
 * `reasons`, when not NULL, receives the failures by kind: not primeable,
 * load refused, timeout (never settled), wrong room, other. */
void oracles_enhanced_view_ghost_counts(const OraclesEnhancedView *view, unsigned *requested, unsigned *completed, unsigned *failed);
void oracles_enhanced_view_ghost_failures(const OraclesEnhancedView *view, unsigned reasons[5]);
/* The failed runs, space separated: "from>expected=got" (a wrong room), "from>room?" (never settled), "from>room!reason" (a state not primeable). */
const char *oracles_enhanced_view_failure_log(const OraclesEnhancedView *view);
/* Results dropped because the cache key changed while the ghost ran. */
unsigned oracles_enhanced_view_stale_results(const OraclesEnhancedView *view);
/* Neighbours computed ahead from another neighbour's settled state, and renders of a neighbour with the live tiles. */
unsigned oracles_enhanced_view_chained_results(const OraclesEnhancedView *view);
unsigned oracles_enhanced_view_live_renders(const OraclesEnhancedView *view);
/* The renders the live tiles asked for whose inputs had not changed (ev_neighbour_pixels): the last one kept, not made again. */
unsigned oracles_enhanced_view_live_renders_kept(const OraclesEnhancedView *view);
/* The most frames in a row a neighbour waited for the render its own animation wanted (the frame's budget spent), the frame it was
 * made counted: 0 if none waited.  A diagnostic: the wait is bounded by construction (RENDER_WAIT_LIMIT, view_render.c). */
unsigned oracles_enhanced_view_render_wait_max(const OraclesEnhancedView *view);
/* Those renders made past the frame's budget once they had waited the most it allows. */
unsigned oracles_enhanced_view_renders_forced(const OraclesEnhancedView *view);
/* A neighbour's objects animated in place: the interactions and parts whose animation loops over several poses go
 * through it at the live game's updates of its objects, back to their captured poses at each transition's start; on
 * by default, off in the Low quality (`enabled` 0), from the next delivery of each neighbour. */
void oracles_enhanced_view_set_neighbour_animation(OraclesEnhancedView *view, int enabled);
/* The harness's check: each render of a pose checked against the step and against a render of the whole area. */
void oracles_enhanced_view_pose_verify(OraclesEnhancedView *view, int enabled);
typedef struct OraclesEnhancedPoseCounts {
    unsigned animated;            /* objects of the captures delivered that their neighbour animates */
    unsigned frozen_code;         /* of several poses, frozen: their code did not advance their animation by one step an update at the capture's probe */
    unsigned frozen_enemies;      /* of several poses, frozen: enemies */
    unsigned frozen_low;          /* that would animate, frozen: the Low quality */
    unsigned frozen_unplaced;     /* that would animate, frozen: the captured pose, built at their place, is not what the capture drew */
    unsigned frozen_unreadable;   /* their animation's data cannot be read */
    unsigned steps;               /* updates of the live objects the neighbours stepped */
    unsigned changes;             /* pose changes stepped */
    unsigned changes_drawn;       /* pose changes a render drew */
    unsigned renders, lines;      /* renders made for poses alone (under the frame's budget), and their lines */
    unsigned resets;              /* objects put back in their captured pose as a transition enters their room */
    unsigned drawn_wrong;         /* with the check: objects a render drew otherwise than their step's pose builds */
    unsigned lines_wrong;         /* with the check: lines of a render of some lines that differ from a render of the whole area */
    unsigned lag_max;             /* the most frames in a row a neighbour shown drew a pose behind its step */
    unsigned probes, probe_frames;   /* captures delivered after their probe, and the guest frames those probes ran */
} OraclesEnhancedPoseCounts;
void oracles_enhanced_view_pose_counts(const OraclesEnhancedView *view, OraclesEnhancedPoseCounts *out);
/* The lines of the renders made that were not drawn again, their tiles unchanged since the last render (ev_neighbour_pixels). */
unsigned oracles_enhanced_view_live_lines_kept(const OraclesEnhancedView *view);
/* The most 8x8 blocks of tiles the live VRAM does not animate that a live render differed from the ghost's render of the same room by: none if the live render is right. */
unsigned oracles_enhanced_view_live_diff_max(const OraclesEnhancedView *view);
/* Results refused as a capture of no terrain (oracles_enhanced_capture_blank): the room run again later. */
unsigned oracles_enhanced_view_plain_renders(const OraclesEnhancedView *view);
/* Results of a room of the live area delivered in another season than the live one (Seasons): not kept. */
unsigned oracles_enhanced_view_season_rejected(const OraclesEnhancedView *view);
const char *oracles_enhanced_view_plain_log(const OraclesEnhancedView *view);   /* their rooms, "group:room" space separated */
/* 1 while the reference room stands alone (the Maku tree's screen and inside). */
int oracles_enhanced_view_isolated(const OraclesEnhancedView *view);
/* World-band pixels of the last composed frame that no source covered (black). */
unsigned oracles_enhanced_view_uncovered(const OraclesEnhancedView *view);
/* The part of the world band the last composition showed, centred in it: all
 * of it, but where the drawn-back view keeps the normal band (256x128). */
void oracles_enhanced_view_shown(const OraclesEnhancedView *view, unsigned *width, unsigned *height);
/* Frames shown framed because the game drew its area elsewhere than its camera (a cutscene's pan or flash). */
unsigned oracles_enhanced_view_off_camera_frames(const OraclesEnhancedView *view);
/* World frames shown while a cutscene held the game with its area drawn elsewhere than its camera, a room's load
 * included, waves and shakes apart: a picture the band placed at the camera's spot.  None is meant. */
unsigned oracles_enhanced_view_world_off_camera_frames(const OraclesEnhancedView *view);
/* Frames shown in the band with the game's per-line scroll (under water, the strange force). */
unsigned oracles_enhanced_view_wave_frames(const OraclesEnhancedView *view);
/* Of those, the frames of a room the band shows whole (a large room drawn back, a room shown alone that it holds): its
 * waves held to its rows, from the game's own sine where the profile binds it, else their part common to every line
 * taken off, so that the room stands still while its lines ripple. */
unsigned oracles_enhanced_view_sway_frames(const OraclesEnhancedView *view);
/* Frames of a scroll under Ages' sea, where the game stops its waves, drawn with them all the same (of the wave frames). */
unsigned oracles_enhanced_view_continued_wave_frames(const OraclesEnhancedView *view);
/* The band shows the room whole and still (oracles_enhanced_view_sway_frames). */
int oracles_enhanced_view_room_whole(const OraclesEnhancedView *view);
/* Frames shown in the band shaken whole with the game's screen shake (a gate, a bomb, a boss). */
unsigned oracles_enhanced_view_shake_frames(const OraclesEnhancedView *view);
/* World frames where the band shows an overworld room beside the reference room, diagonals included, that no delivered terrain covers yet. */
unsigned oracles_enhanced_view_waiting_frames(const OraclesEnhancedView *view);
/* The rooms never visited kept dark (`hidden` 1): a room of the band that the game's own record says Link has never
 * entered (the visited bit of its flags, from which the game's map draws its squares) is drawn in the band's black, its
 * objects with it, the reference room and the room a scroll enters always shown.  Off (0) by default.  The ghost
 * prepares every room as before, so that a room shows as soon as it is visited: drawn in the band after a composition
 * that kept it dark, it fades in from the black over ORACLES_ENHANCED_REVEAL_FRAMES compositions (the veil of its
 * age, the frame it is first shown 1), never on the first composition of an epoch (a warp, a time travel, a load) nor
 * at the switch.  Presentation alone: it may change between two compositions.  A dark room counts as decided, never as
 * a room waiting for the ghost, in the black the view measures.  The counts: world frames with a room kept dark, the
 * most rooms so in a frame, the rooms that faded in, and, whatever the setting, the frames of normal play (the first
 * after a transition aside) whose reference room the game had not marked visited, with the first of them. */
#define ORACLES_ENHANCED_REVEAL_FRAMES 16u
void oracles_enhanced_view_set_unvisited_hidden(OraclesEnhancedView *view, int hidden);
int oracles_enhanced_view_unvisited_hidden(const OraclesEnhancedView *view);
/* A room kept dark beside a room drawn in clear shows its edge (ORACLES_ENHANCED_EDGE_DEPTH, compositor.h): a band of
 * its own terrain along their frontier, in full over a wave, then darker, then black, the fade in going from that look
 * to the room whole (`soft` 1, the default); or the black cut straight (0, the Low quality).  A room laid dark before
 * its pixels came has no edge yet: it comes in with them, over ORACLES_ENHANCED_REVEAL_FRAMES compositions. */
void oracles_enhanced_view_set_unvisited_edge(OraclesEnhancedView *view, int soft);
/* The harness's check: after each composition, the rooms it kept dark read past their edge (oracles_enhanced_dark_lit)
 * and along their sides where nothing in clear stands beyond (oracles_enhanced_dark_stray), and how many of them the
 * last one showed lit there. */
void oracles_enhanced_view_check_dark(OraclesEnhancedView *view, int enabled);
unsigned oracles_enhanced_view_dark_lit(const OraclesEnhancedView *view);
unsigned oracles_enhanced_view_dark_stray(const OraclesEnhancedView *view);
/* The rooms of the reference's group the last composition drew, a bit each: kept dark (`dark`), and in clear or fading in
 * (`shown`, the rooms the setting judged, the reference room among them as a cached entry).  0 with the setting off or
 * before a composition that applied it. */
int oracles_enhanced_view_drawn_rooms(const OraclesEnhancedView *view, uint8_t *group, uint32_t dark[8], uint32_t shown[8]);
/* The neighbour of the last composed frame in a direction (oracles_enhanced_view_neighbour) kept dark, and which room
 * it is in the reference's group (`room`, when not NULL, set whenever there is one). */
int oracles_enhanced_view_neighbour_dark(const OraclesEnhancedView *view, unsigned direction, uint8_t *room);
unsigned oracles_enhanced_reveal_veil(uint32_t age);
typedef struct OraclesEnhancedUnvisitedCounts {
    unsigned hidden_frames, hidden_rooms_max, reveal_fades;
    unsigned edge_fades;   /* rooms kept dark whose edge came in as their pixels came */
    unsigned unvisited_reference;
    uint32_t first_frame;
    uint8_t first_group, first_room;
} OraclesEnhancedUnvisitedCounts;
void oracles_enhanced_view_unvisited_counts(const OraclesEnhancedView *view, OraclesEnhancedUnvisitedCounts *out);
/* Rooms delivered for a direction of a room the game routes itself. */
unsigned oracles_enhanced_view_routed_delivered(const OraclesEnhancedView *view);
/* The order in which the four directions of a room the game routes itself are
 * asked of the ghost: the two sides before up and down, since a
 * room of one screen shows its sides in the band at rest and its rows above
 * and below only as a strip when Link stands off its middle; in each pair the
 * side Link is nearer first; and a place off the map, which the camera never
 * shows at rest, after every place on it.  `gap` is Link's distance to each
 * edge of the room and `on_map` whether the place that way is on the map,
 * both indexed by direction (0 up, 1 right, 2 down, 3 left, the game's). */
void oracles_enhanced_routed_order(const int32_t gap[4], const int on_map[4], unsigned out[4]);
/* The rooms beyond an answer of a room the game routes itself, read through
 * that answer (the Lost Woods, finishes of the drawn-back view): when the room
 * the game loads leaving `ref` in direction `dir` is the grid's neighbour
 * there and is not a room the game routes itself (`answer_routed` 0), the
 * game goes on from it by the grid, and the rooms it loads going on are
 * those of the grid: the room beyond it that way (out[0]), the two beside it
 * across that way (out[1], out[2]: up then down for a side, left then right
 * for up or down), the two beside the room beyond (out[3], out[4]).  `have`
 * says which are on the map (`map_width` columns, `map_height` rows); returns
 * how many, 0 when the answer is not such a room. */
/* Whether an answer of a room the game routes itself still holds: its
 * routine's routing key (`key_len` bytes, 0 when the routine has no account)
 * as the live game has it now, against the one it was asked under; during a
 * transition (`taking` the direction taken, -1 none) the answer of that
 * direction holds all the same, the room being entered, asked from the state
 * before the transition moved the key.  An answer without a key holds until
 * the next transition asks it again (ev_drop_routed_entries). */
int oracles_enhanced_routed_answer_holds(unsigned answer_dir, int taking, const uint8_t *asked_key, const uint8_t *live_key, size_t key_len);
unsigned oracles_enhanced_routed_beyond(uint8_t ref, unsigned dir, uint8_t answer, int answer_routed, unsigned map_width, unsigned map_height,
                                        uint8_t out[5], int have[5]);
/* The order in which the drawn-back band outdoors asks the ghost
 * for the rooms around Link: `count` rooms given in the normal band's order,
 * `shown` the pixels of each the band showed at the last composition and
 * `distance` the square of its middle's distance to Link; `out` receives
 * their indices, the most shown first, then the nearest, then the normal
 * order. */
void oracles_enhanced_band_order(const int32_t *shown, const int32_t *distance, unsigned count, unsigned *out);
/* World frames where a neighbour drawn in the band holds a fade's palettes (oracles_enhanced_capture_blank): never, if its guard holds. */
unsigned oracles_enhanced_view_plain_shown_frames(const OraclesEnhancedView *view);
/* A scene's tint of the game's screen: `scene`, the palettes a cutscene
 * shows over `reference`, those the room's own load gives.  1 when it
 * changed palettes (`changed`, a bit each), one of them by more than 2 on a
 * channel, and one colour transform (`coefficients`, by output channel red,
 * green, blue: the weights of red, green and blue and a constant, in 1/1024)
 * takes every colour of at least four of them (`slots`) within 4 of the
 * scene's, the others left out the farthest first.  `oracles_enhanced_scene_tinted`
 * applies it to a room's own palettes: of those changed, a palette that is the
 * scene's already stays, the room's that is the reference's takes the scene's,
 * any other goes through the transform. */
int oracles_enhanced_scene_tint(const uint8_t reference[64], const uint8_t scene[64], uint8_t *slots, uint8_t *changed, int32_t coefficients[12]);
void oracles_enhanced_scene_tinted(const uint8_t own[64], const uint8_t reference[64], const uint8_t scene[64], uint8_t changed,
                                   const int32_t coefficients[12], uint8_t out[64]);
/* Whether a tint found in the few frames after a fade, a scroll or a palette
 * transition is a scene's: at least four of the palettes it explains
 * (`slots`) changed by more than 2 on a channel, the palettes a fade ends on
 * (the night a fade from white opens on), not the last step of the fade or
 * of the transition still on screen beside a palette the room shows apart. */
int oracles_enhanced_scene_tint_after_fade(const uint8_t reference[64], const uint8_t scene[64], uint8_t slots);
/* The room's own palettes a scene's tint is taken from, kept by room while
 * the scene lasts: its entry may be taken for another room, or run again
 * from the live game, which carries the scene's palettes (the ghost's load
 * keeps the base of the same palette header: loadTilesetData loads the
 * palettes of another one only).  `oracles_enhanced_tint_reference` returns
 * the palettes to fit the scene against: those kept for the room, the first
 * a tint held from there, else `entry` (its last delivery, NULL without
 * one); it drops what was kept once the room's entry, the scene and the kept
 * palettes are the same (the scene over), and what no tint held from for
 * ORACLES_TINT_MEMORY_FRAMES.  `oracles_enhanced_tint_kept` notes a tint
 * held from `reference`; `oracles_enhanced_tint_forget` drops everything (a
 * warp, a load). */
#define ORACLES_TINT_MEMORIES 4u
#define ORACLES_TINT_MEMORY_FRAMES 600u
typedef struct OraclesTintMemory {
    int valid;
    uint8_t group, room, header;   /* the room and its palette header (wTilesetPalette) */
    uint32_t used_at;              /* the last frame a tint held from it */
    uint8_t reference[64];
} OraclesTintMemory;
const uint8_t *oracles_enhanced_tint_reference(OraclesTintMemory memories[ORACLES_TINT_MEMORIES], uint8_t group, uint8_t room, uint8_t header,
                                               uint32_t frame, const uint8_t *entry, const uint8_t scene[64]);
void oracles_enhanced_tint_kept(OraclesTintMemory memories[ORACLES_TINT_MEMORIES], uint8_t group, uint8_t room, uint8_t header, uint32_t frame,
                                const uint8_t reference[64]);
void oracles_enhanced_tint_forget(OraclesTintMemory memories[ORACLES_TINT_MEMORIES]);
/* Whether a ghost's capture of a room is no terrain: its background palettes
 * the palette thread's fade rather than the room's own (the offset most
 * channels agree on, against the base palettes w2TilesetBgPalettes, more
 * than 2 apart, the tolerance of a smooth palette transition, from the
 * offset the game keeps on the room, `kept_offset`: a dark room's -16,
 * wPaletteThread_parameter; palettes equal to the base are no fade), or,
 * given its area, one colour over nine tenths of it (the LCD off). */
int oracles_enhanced_capture_blank(const uint8_t bg_palettes[64], const uint8_t base_palettes[64], int kept_offset, const uint32_t *area, unsigned pixels);
/* The objects of a large room beyond the game's window: sprites drawn
 * outside the window over the run, and compositions in a large room whose OAM
 * on screen matched none of the game's drawings kept. */
/* Cached rooms whose captured objects were dropped because the enemies counted killed in them changed. */
unsigned oracles_enhanced_view_objects_outdated(const OraclesEnhancedView *view);
/* Rooms run again (a season change, a killed enemy) whose old terrain was dropped after two failed runs. */
unsigned oracles_enhanced_view_refresh_given_up(const OraclesEnhancedView *view);
/* Parents run again from the live state before a room was chained from them: an enemy of that room killed since their run. */
unsigned oracles_enhanced_view_parents_run_for_kills(const OraclesEnhancedView *view);
void oracles_enhanced_view_large_object_counts(const OraclesEnhancedView *view, unsigned *sprites_drawn, unsigned *frames_unmatched);
/* Copies a neighbour's own animation wrote into its tiles; `other`: into a neighbour whose tileset or animation is not the live room's;
 * `in_step_tiles`: tiles such a neighbour took live at a render, the live room showing on them an image of its own animation. */
void oracles_enhanced_view_animation_counts(const OraclesEnhancedView *view, unsigned *own, unsigned *other, unsigned *in_step_tiles);
/* The tile animation during a scroll, which the game freezes and resumes
 * where it stopped: the view runs it alone in whole loops, one at least, that
 * land on the step the game resumes from (neighbours/animation.h,
 * view_scroll.c).  What it did, over the run: */
typedef struct OraclesEnhancedScrollCounts {
    unsigned scrolls;          /* scrolls the view animated */
    unsigned scrolls_still;    /* scrolls left frozen: into another animation or tileset, which the game starts again, or off the map */
    unsigned landed;           /* of the scrolls animated, those whose counter ran out, where the view lands on the game's step */
    unsigned landed_off;       /* of those, the ones landed on another frame than the plan's: their streams took a jump at the landing, or stood waiting */
    unsigned gaps, gap_max;    /* scrolls whose last frozen frame had the view's step apart from the game's, and by how many frames at most */
    unsigned tiles_off;        /* tiles the view's streams wrote that differed from the live VRAM at the landing, summed */
    unsigned tiles_given_back; /* tiles the view's streams wrote that the game wrote itself during the scroll (the room entered's graphics): the game's again */
    unsigned streams_run;      /* streams of the scrolls landed that ran (a stream outside its loop stays still and is not counted) */
    unsigned pace_min, pace_median, pace_max;   /* the pace of the streams run, in percent of the game's */
    unsigned window_pixels;    /* pixels of the game's window drawn with the view's tiles */
} OraclesEnhancedScrollCounts;
void oracles_enhanced_view_scroll_animation_counts(const OraclesEnhancedView *view, OraclesEnhancedScrollCounts *out);
/* Neighbour streams matched with a live stream of the same loop, and the frames one was moved to its live twin's step. */
void oracles_enhanced_view_stream_counts(const OraclesEnhancedView *view, unsigned *matched, unsigned *followed);
/* Whether the area blurb was taken out of the window and drawn apart this frame (its pixels are repainted). */
int oracles_enhanced_view_blurb_drawn(const OraclesEnhancedView *view);
/* Scroll frames where Link is to be drawn (his object visible) and nothing of him is on the screen nor drawn past its
 * edge: Link missing from the view; and those he was drawn from his fields, past the edge where the game writes none
 * of his sprites. */
unsigned oracles_enhanced_view_link_unshown_frames(const OraclesEnhancedView *view);
unsigned oracles_enhanced_view_link_built_frames(const OraclesEnhancedView *view);
/* Pre-runs started while a room loaded: results filed beside the room the ghost primed in, and results dropped. */
void oracles_enhanced_view_blind_runs(const OraclesEnhancedView *view, unsigned *filed, unsigned *dropped);
/* Neighbours run again from the live state because the key had moved since they were computed (the source of a chain). */
unsigned oracles_enhanced_view_refreshed_parents(const OraclesEnhancedView *view);
/* Cached terrains dropped because a key byte they read changed: the count, and "address:drops" pairs into `out`. */
unsigned oracles_enhanced_view_drop_log(const OraclesEnhancedView *view, char *out, size_t capacity);
/* The times a byte of the coarse list (oracles_ghost_key_coarse_ranges) changed and threw the whole cache. */
unsigned oracles_enhanced_view_coarse_drops(const OraclesEnhancedView *view);
/* The black the band shows, from its first frame without any
 * (enhanced.black_pixels_mean in the harness, the same count at the end of
 * a session in the launcher): the band's uncovered pixels in world frames,
 * and the rooms of the overworld's grid (not an interior, a dungeon or the
 * zone the game routes itself) shown by a pixel at least with nothing drawn
 * at their place, for more than ORACLES_ENHANCED_BLACK_ROOM_FRAMES world
 * frames in a row. */
#define ORACLES_ENHANCED_BLACK_ROOM_FRAMES 120u
#define ORACLES_ENHANCED_BLACK_ROOMS_KEPT 8u
typedef struct OraclesEnhancedBlackRoom { uint8_t group, room; uint32_t from, frames; } OraclesEnhancedBlackRoom;
/* The black told apart by its cause, in episodes: a run of world frames, a
 * scroll's included, in which a room of the overworld's grid or of Ages'
 * open sea shows nothing drawn (a corner, two rows and two columns from the
 * room in play; another room; either while the band fills, less than
 * ORACLES_ENHANCED_BLACK_COLD_FRAMES frames after the first world frame, a
 * load or a warp), or the band shows a place of no room of its grid (past
 * the map's edge, the sea's shore), or what is left black is the fringe of a
 * ripple, or around a room shown alone on purpose (the Maku tree's), or
 * none of these. */
#define ORACLES_ENHANCED_BLACK_COLD_FRAMES 300u
enum { ORACLES_ENHANCED_BLACK_CORNER, ORACLES_ENHANCED_BLACK_NEIGHBOUR, ORACLES_ENHANCED_BLACK_COLD, ORACLES_ENHANCED_BLACK_EDGE,
       ORACLES_ENHANCED_BLACK_RIPPLE, ORACLES_ENHANCED_BLACK_ALONE, ORACLES_ENHANCED_BLACK_OTHER, ORACLES_ENHANCED_BLACK_KINDS };
typedef struct OraclesEnhancedBlackKind {
    unsigned episodes, frames, longest;   /* frames: world frames with one episode of the kind at least */
    unsigned in_transition;               /* episodes whose first frame was in a transition */
} OraclesEnhancedBlackKind;
typedef struct OraclesEnhancedBlack {
    int have_world, have_full;
    uint32_t first_world, first_full;     /* the frames (composition ordinals, the route's frame numbers) */
    unsigned full_world_frames, max;      /* world frames from the first full one, and the most black in one */
    uint64_t pixels;                      /* black pixels over them */
    unsigned rooms_over;                  /* rooms black for more than the threshold, a run each */
    OraclesEnhancedBlackRoom longest[ORACLES_ENHANCED_BLACK_ROOMS_KEPT];   /* the longest of them, longest first */
    OraclesEnhancedBlackKind kinds[ORACLES_ENHANCED_BLACK_KINDS];
} OraclesEnhancedBlack;
void oracles_enhanced_view_black(const OraclesEnhancedView *view, OraclesEnhancedBlack *out);
/* The black kinds' names (corner, neighbour, cold, edge, ripple, alone, other), and the
 * corners' and other rooms' episodes ended so far outside the band's fill,
 * "c|n group:room from+frames" with a `t` when begun in a transition, the
 * first that fit. */
const char *oracles_enhanced_black_kind_name(unsigned kind);
const char *oracles_enhanced_view_black_log(const OraclesEnhancedView *view);
/* Whether a neighbour's settled state may start a run of the room whose
 * own flags sit at `own` in the key, the live state unable to prime the
 * ghost (a cutscene): its key the live one but for the flags of other rooms
 * (the room flags' pages at `flags_at` in the key, `flags_length` long). */
int oracles_enhanced_parent_key_holds(const uint8_t *parent_key, const uint8_t *live_key, size_t length, long flags_at, size_t flags_length, long own);
/* Whether a parent's key is the live one for a room chained from it: but for
 * the visited bit (`visited`) of the parent's own flags (at `own` in the key),
 * and, `visits` not NULL, of the other rooms' flags (the pages at `flags_at`,
 * `flags_length` long), `*visits` then set when one of those differs. */
int oracles_enhanced_parent_key_current(const uint8_t *parent_key, const uint8_t *live_key, size_t length, long flags_at, size_t flags_length, long own,
                                        uint8_t visited, int *visits);
/* Whether the `count` bytes a run read, at `addresses`, as it saw them
 * (`seen`) are the live ones: the visited bit (`visited`) of the room's own
 * flags (`own_flags`) aside, which the entry into it sets and no load reads.
 * A room run from beside is kept only if they are. */
int oracles_enhanced_reads_hold(unsigned count, const uint16_t *addresses, const uint8_t *seen, const uint8_t *live, uint16_t own_flags, uint8_t visited);
/* Whether a room around the reference room `ref` is settled for the band's
 * fill: delivered, under way, failed, or waiting for another parent than one
 * whose state is no start (`waits_start`, marked around `waits_ref`), which
 * the band would otherwise wait on for as long as that parent holds.  Rooms
 * as group << 8 | room. */
int oracles_enhanced_fill_settled(int valid, int in_flight, int failed, int waits_start, unsigned waits_ref, unsigned ref);
/* Whether `parent`, no longer holding, may be run again for a room around
 * `ref` at `frame`: at once, unless it is the parent the room waits on as no
 * start (`waits_parent`, found so around `waits_ref` since `waits_since`),
 * then at the pace of a failure, as when the room itself was failed. */
int oracles_enhanced_parent_rerun_due(int waits_start, unsigned waits_parent, unsigned waits_ref, uint32_t waits_since,
                                      unsigned parent, unsigned ref, uint32_t frame);
/* Results run from a room beside while a cutscene held the game, refused because a byte they read had changed. */
unsigned oracles_enhanced_view_beside_refused(const OraclesEnhancedView *view);
/* Runs started from the live state across the sea's surface (Ages), ahead of a dive or a return to the surface. */
unsigned oracles_enhanced_view_sea_runs(const OraclesEnhancedView *view);
/* The committed layout (176 bytes) behind a shown neighbour in a direction; 0 when none is shown, 2 when the
 * neighbour is shown in its old season while the ghost runs it again after a season change (deliberately: less abrupt than black). */
int oracles_enhanced_view_neighbour_layout(const OraclesEnhancedView *view, unsigned direction, uint8_t *out);

/* Host state for the composite savestate: the camera reducer's wire state.
 * After a load the observer and the neighbours rebuild from the guest. */
int oracles_enhanced_view_save_state(const OraclesEnhancedView *view, uint8_t *out, size_t capacity, size_t *written);
int oracles_enhanced_view_load_state(OraclesEnhancedView *view, const uint8_t *data, size_t size);
/* 0 when a host state was taken at this view's surface size; -1 and the
 * reason in `why` when at the other (a state of the drawn-back view in the
 * normal one, or the reverse), which the launcher refuses whole. */
int oracles_enhanced_view_check_state(const OraclesEnhancedView *view, const uint8_t *data, size_t size, char *why, size_t capacity);

OraclesEnhancedCamera *oracles_enhanced_view_camera(OraclesEnhancedView *view);

#ifdef __cplusplus
}
#endif

#endif
