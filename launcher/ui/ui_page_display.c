/* Display (ui_page_nav.h): its rows, its keys and pointer, and its texts. */
#include "ui_page_nav.h"

#include "compositor.h"
#include "ui_page_text.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* The profiles: their names, their sizes, their notes. */
const char *const oracles_profile_names[ORACLES_PROFILES] = { "Faithful", "Enhanced" };
void oracles_profile_size(const OraclesHomeNav *nav, int profile, char *out, size_t capacity)
{
    if (profile == ORACLES_PROFILE_ENHANCED) oracles_display_view_size(nav, nav->display.view, out, capacity);
    else snprintf(out, capacity, "160\xc3\x97" "144");
}
static const char *const profile_notes[ORACLES_PROFILES] = {
    "The core alone, as an unrecognized ROM sees it.", "The wide world, drawn back, with the rooms around and continuous transitions."
};

const char *const oracles_display_labels[ORACLES_DISPLAY_ROWS] = { "Profile", "Quality", "Window", "View", "HUD", "Text", "Color correction", "Advanced", "Core",
                                                                   "Continuous transitions", "Vsync", "Neighbor workers", "Scaling", "Camera", "Screen margin",
                                                                   "Text", "Text box", "Text size", "Text speed", "Picture", "Menus & scenes", "Far size",
                                                                   "Unvisited rooms" };
const char *const oracles_display_view_names[3] = { "Near", "Medium", "Far" };
const char *const oracles_display_colour_choices[2] = { "Off", "On" };
const char *const oracles_display_hud_choices[3] = { "Original", "Full width", "Wide layout" };
const char *const oracles_display_margin_choices[ORACLES_DISPLAY_MARGINS] = { "Off", "Auto", "Small", "Large" };
const char *const oracles_display_vsync_choices[3] = { "Auto", "On", "Off" };
const char *const oracles_display_core_choices[2] = { "Accurate (SameBoy)", "Fast (mGBA)" };
const char *const oracles_display_core_names[2] = { "Accurate", "Fast" };
const char *const oracles_display_scaling_choices[2] = { "Sharp", "Fill" };
const char *const oracles_display_camera_choices[2] = { "Pixel", "Smooth" };
const char *const oracles_display_quality_names[ORACLES_DISPLAY_QUALITIES + 1] = { "Low", "Medium", "High", "Max", "Custom" };
const char *const oracles_display_menus_choices[2] = { "View", "Large" };
const char *const oracles_display_far_size_choices[2] = { "Fitted", "Fixed" };
const char *const oracles_display_unvisited_choices[2] = { "Shown", "Hidden" };
int oracles_display_transitions_apply(const OraclesHomeNav *nav) { return nav->display.profile == ORACLES_PROFILE_ENHANCED; }
int oracles_display_hud_applies(const OraclesHomeNav *nav)
{
    return (nav->in_game ? nav->playing : nav->display.profile) == ORACLES_PROFILE_ENHANCED;
}
/* The rows of each page in the order of the keys: Display's own, its links last; the Advanced rows; the Picture rows,
 * Far size among them where it changes something (oracles_display_far_size_shown); the Text rows. */
static const unsigned main_rows[] = { ORACLES_DISPLAY_PROFILE, ORACLES_DISPLAY_QUALITY, ORACLES_DISPLAY_WINDOW, ORACLES_DISPLAY_VIEW,
                                      ORACLES_DISPLAY_HUD, ORACLES_DISPLAY_TEXT, ORACLES_DISPLAY_PICTURE, ORACLES_DISPLAY_ADVANCED };
static const unsigned advanced_rows[] = { ORACLES_DISPLAY_CORE, ORACLES_DISPLAY_TRANSITIONS, ORACLES_DISPLAY_VSYNC, ORACLES_DISPLAY_WORKERS,
                                          ORACLES_DISPLAY_CAMERA };
static const unsigned picture_rows[] = { ORACLES_DISPLAY_SCALING, ORACLES_DISPLAY_MARGIN, ORACLES_DISPLAY_COLOUR, ORACLES_DISPLAY_FAR_SIZE,
                                         ORACLES_DISPLAY_MENUS, ORACLES_DISPLAY_UNVISITED };
static const unsigned text_rows[] = { ORACLES_DISPLAY_LETTERS, ORACLES_DISPLAY_TEXT_BOX, ORACLES_DISPLAY_TEXT_SIZE, ORACLES_DISPLAY_TEXT_SPEED };

static const unsigned *page_rows(const OraclesHomeNav *nav, unsigned *count)
{
#define ROWS_OF(list) (*count = sizeof list / sizeof list[0], list)
    return nav->text_page ? ROWS_OF(text_rows) : nav->picture_page ? ROWS_OF(picture_rows) : nav->advanced ? ROWS_OF(advanced_rows) : ROWS_OF(main_rows);
#undef ROWS_OF
}

/* The row's place in the page's list, `count` when it is not there. */
static unsigned row_index(const unsigned *rows, unsigned count, unsigned row)
{
    unsigned i = 0;
    while (i < count && rows[i] != row) i++;
    return i;
}

int oracles_display_row_shown(const OraclesHomeNav *nav, unsigned row)
{
    unsigned count;
    const unsigned *rows = page_rows(nav, &count);
    return row_index(rows, count, row) < count && (row != ORACLES_DISPLAY_FAR_SIZE || oracles_display_far_size_shown(nav));
}

int oracles_display_auto_workers(const OraclesHomeNav *nav) { return nav->display.cores >= 4 ? 2 : 1; }

void oracles_display_workers_choice(const OraclesHomeNav *nav, int choice, char *out, size_t capacity)
{
    if (choice == 0) snprintf(out, capacity, "Auto \xc2\xb7 %d", oracles_display_auto_workers(nav));
    else snprintf(out, capacity, "%d", choice);
}

const char *oracles_display_section(const OraclesHomeNav *nav)
{
    return nav->text_page ? "Display \xe2\x80\xba Text" : nav->picture_page ? "Display \xe2\x80\xba Picture"
           : nav->advanced ? "Display \xe2\x80\xba Advanced" : "Display";
}

const char oracles_display_pause_reason[] = "Change it from the home screen.";
const char oracles_display_fan_reason[] = "Ages and Seasons only for now.";

int oracles_display_pause_locked(const OraclesHomeNav *nav, unsigned row)
{
    switch (row) {
        case ORACLES_DISPLAY_PROFILE: case ORACLES_DISPLAY_QUALITY: case ORACLES_DISPLAY_VIEW: case ORACLES_DISPLAY_CORE:
        case ORACLES_DISPLAY_TRANSITIONS: case ORACLES_DISPLAY_WORKERS: case ORACLES_DISPLAY_LETTERS:
        case ORACLES_DISPLAY_TEXT_BOX: case ORACLES_DISPLAY_FAR_SIZE: return nav->in_game;
        default: return 0;
    }
}

int oracles_display_row_fixed(const OraclesHomeNav *nav, unsigned row)
{
    return oracles_display_pause_locked(nav, row)
           || (row == ORACLES_DISPLAY_MARGIN && nav->display.margin_fixed)
           || (row == ORACLES_DISPLAY_LETTERS && nav->display.text_fixed) || (row == ORACLES_DISPLAY_TEXT_BOX && nav->display.text_box_fixed)
           || (row == ORACLES_DISPLAY_TEXT_SIZE && nav->display.text_size_fixed)
           || (row == ORACLES_DISPLAY_UNVISITED && nav->display.unvisited_fixed);
}

/* Each quality's view, core (0 Accurate, 1 Fast) and workers (0 Auto): settings.c's, which tests/test_launcher_settings.c
 * checks these against. */
static const struct { int view, core, workers; } qualities[ORACLES_DISPLAY_QUALITIES] = {
    { 0, 1, 0 },   /* Low: near */
    { 1, 1, 0 },   /* Medium */
    { 2, 1, 0 },   /* High: far */
    { 2, 0, 2 },   /* Max */
};

int oracles_display_quality(const OraclesHomeNav *nav)
{
    for (int q = 0; q < ORACLES_DISPLAY_QUALITIES; q++)
        if (nav->display.view == qualities[q].view && nav->display.core == qualities[q].core && nav->display.workers == qualities[q].workers) return q;
    return ORACLES_DISPLAY_QUALITY_CUSTOM;
}

int oracles_display_quality_applies(const OraclesHomeNav *nav) { return nav->display.profile == ORACLES_PROFILE_ENHANCED && !nav->in_game; }

void oracles_display_quality_sets(int quality, char *out, size_t capacity)
{
    if (quality < 0 || quality >= ORACLES_DISPLAY_QUALITIES) snprintf(out, capacity, "By hand");
    else snprintf(out, capacity, "%s \xc2\xb7 %s", oracles_display_view_names[qualities[quality].view], oracles_display_core_names[qualities[quality].core]);
}

void oracles_display_quality_text(const OraclesHomeNav *nav, int short_form, char *out, size_t capacity)
{
    const int view = nav->display.view >= 0 && nav->display.view < 3 ? nav->display.view : 2, core = nav->display.core ? 1 : 0;
    const int workers = nav->display.workers;
    char on[16];
    if (workers) snprintf(on, sizeof on, "at %d", workers);
    else snprintf(on, sizeof on, "on auto");
    const char *const view_name = oracles_display_view_names[view], *const core_name = oracles_display_core_names[core];
    if (oracles_display_quality(nav) != ORACLES_DISPLAY_QUALITY_CUSTOM)
        snprintf(out, capacity, short_form ? "%s view, %s core, workers %s." : "%s view, %s core, neighbor workers %s.", view_name, core_name, on);
    else if (short_form) {
        char count[8];
        if (workers) snprintf(count, sizeof count, "%d", workers);
        else snprintf(count, sizeof count, "auto");
        snprintf(out, capacity, "Set by hand: %s, %s, workers %s.", view_name, core_name, count);
    } else snprintf(out, capacity, "Set by hand: %s view, %s core, neighbor workers %s.", view_name, core_name, on);
}

const char *oracles_display_quality_note(const OraclesHomeNav *nav, int short_form)
{
    if (oracles_display_quality(nav) == ORACLES_DISPLAY_QUALITY_CUSTOM) return "Pick a quality to set all three again.";
    return short_form ? "Changing one by hand makes it Custom." : "Changing one of them by hand makes it Custom.";
}

/* The quality `quality` applied: the view, the core and the workers it sets. */
static OraclesHomeCommand quality_set(OraclesHomeNav *nav, int quality)
{
    if (!oracles_display_quality_applies(nav) || quality < 0 || quality >= ORACLES_DISPLAY_QUALITIES || quality == oracles_display_quality(nav))
        return ORACLES_HOME_STAY;
    nav->display.view = qualities[quality].view;
    nav->display.core = qualities[quality].core;
    nav->display.workers = qualities[quality].workers;
    return ORACLES_HOME_STORE;
}

/* One quality lighter (`by` -1) or heavier (1), stopping at Low and at Max; from Custom, heavier is Low, lighter Max. */
static OraclesHomeCommand quality_step(OraclesHomeNav *nav, int by)
{
    const int quality = oracles_display_quality(nav);
    if (quality == ORACLES_DISPLAY_QUALITY_CUSTOM) return quality_set(nav, by > 0 ? 0 : ORACLES_DISPLAY_QUALITIES - 1);
    const int next = quality + by;
    return quality_set(nav, next < 0 ? 0 : next >= ORACLES_DISPLAY_QUALITIES ? ORACLES_DISPLAY_QUALITIES - 1 : next);
}

/* The shape the settings' aspect= names (an OraclesAspect: 1 16:9, 2 4:3), ORACLES_ENHANCED_ASPECTS for auto. */
static OraclesEnhancedAspect named_aspect(const OraclesHomeNav *nav)
{
    return nav->display.aspect == 2 ? ORACLES_ENHANCED_4_3 : nav->display.aspect == 1 ? ORACLES_ENHANCED_16_9 : ORACLES_ENHANCED_ASPECTS;
}

/* The display's size, 0x0 when unknown. */
static void screen_size(const OraclesHomeNav *nav, unsigned *w, unsigned *h)
{
    const int known = nav->display.screen_w > 0 && nav->display.screen_h > 0;
    *w = known ? (unsigned)nav->display.screen_w : 0u;
    *h = known ? (unsigned)nav->display.screen_h : 0u;
}

int oracles_display_screen_4_3(const OraclesHomeNav *nav)
{
    /* As the session decides it (oracles_session_view_4_3): the settings' shape, else the screen's. */
    const OraclesEnhancedAspect named = named_aspect(nav);
    if (named != ORACLES_ENHANCED_ASPECTS) return named == ORACLES_ENHANCED_4_3;
    unsigned w, h;
    screen_size(nav, &w, &h);
    return oracles_enhanced_screen_aspect(w, h) == ORACLES_ENHANCED_4_3;
}

/* The view's surface as a game started from here takes it (oracles_session_view_size): far fitted to the screen unless
 * far=fixed or aspect= names a shape. */
static OraclesEnhancedSize view_surface(const OraclesHomeNav *nav, int view)
{
    const int level = view >= 0 && view < ORACLES_ENHANCED_LEVELS ? view : ORACLES_ENHANCED_FAR;
    unsigned w, h;
    screen_size(nav, &w, &h);
    return oracles_enhanced_screen_view_size((OraclesEnhancedLevel)level, named_aspect(nav), nav->display.far_fixed, w, h);
}

void oracles_display_view_size(const OraclesHomeNav *nav, int view, char *out, size_t capacity)
{
    const OraclesEnhancedSize s = view_surface(nav, view);
    snprintf(out, capacity, "%u\xc3\x97%u", s.width, s.height);
}

int oracles_display_far_size_shown(const OraclesHomeNav *nav)
{
    unsigned w, h;
    screen_size(nav, &w, &h);
    return named_aspect(nav) == ORACLES_ENHANCED_ASPECTS && oracles_enhanced_far_fitting(w, h).fit == ORACLES_ENHANCED_FIT_FITTED;
}

void oracles_display_picture_summary(const OraclesHomeNav *nav, char *out, size_t capacity)
{
    static const char *const margins[ORACLES_DISPLAY_MARGINS] = { "No margin", "Margin auto", "Small margin", "Large margin" };
    const int margin = nav->display.margin >= 0 && nav->display.margin < ORACLES_DISPLAY_MARGINS ? nav->display.margin : 0;
    const int enhanced = oracles_display_hud_applies(nav);
    const char *extras[3];
    unsigned count = 0, length = 0;
    if (enhanced && nav->display.far_fixed && oracles_display_far_size_shown(nav)) extras[count++] = "Far fixed";
    if (enhanced && !nav->display.menus != !nav->android) extras[count++] = nav->display.menus ? "Large menus" : "View menus";
    if (enhanced && nav->display.unvisited) extras[count++] = "Rooms hidden";
    for (unsigned i = 0; i < count; i++) length += (unsigned)strlen(extras[i]) + (i ? 3u : 0u);   /* " · " is three characters */
    int used = snprintf(out, capacity, "%s \xc2\xb7 %s \xc2\xb7 Color %s", nav->display.scaling ? "Fill" : "Sharp", margins[margin], nav->display.colour ? "on" : "off");
    if (count && length > (nav->layout == ORACLES_UI_LAYOUT_4_3 ? 23u : 12u)) {
        if (used >= 0 && (size_t)used < capacity) snprintf(out + used, capacity - (size_t)used, " \xc2\xb7 %u more", count);
        return;
    }
    for (unsigned i = 0; i < count && used >= 0 && (size_t)used < capacity; i++) {
        const int n = snprintf(out + used, capacity - (size_t)used, " \xc2\xb7 %s", extras[i]);
        if (n < 0) break;
        used += n;
    }
}

int oracles_display_size_row_live(const OraclesHomeNav *nav, unsigned row)
{
    return (row == ORACLES_DISPLAY_MENUS || row == ORACLES_DISPLAY_FAR_SIZE || row == ORACLES_DISPLAY_UNVISITED) && oracles_display_hud_applies(nav)
           && !oracles_display_row_fixed(nav, row);
}

void oracles_display_size_note(const OraclesHomeNav *nav, unsigned row, char *out, size_t capacity)
{
    unsigned w, h;
    screen_size(nav, &w, &h);
    const OraclesEnhancedFitting f = oracles_enhanced_far_fitting(w, h);
    if (capacity) out[0] = 0;
    if (row != ORACLES_DISPLAY_MENUS && row != ORACLES_DISPLAY_FAR_SIZE && row != ORACLES_DISPLAY_UNVISITED) return;
    if (!oracles_display_hud_applies(nav)) snprintf(out, capacity, "Enhanced only.");
    else if (row == ORACLES_DISPLAY_UNVISITED) { if (nav->in_game && !nav->display.unvisited_fixed) snprintf(out, capacity, "Applies when the game resumes."); }
    else if (oracles_display_row_fixed(nav, row)) snprintf(out, capacity, "%s", oracles_display_pause_reason);
    else if (row == ORACLES_DISPLAY_MENUS) return;
    else if (named_aspect(nav) == ORACLES_ENHANCED_ASPECTS && f.fit == ORACLES_ENHANCED_FIT_FITTED) {
        const OraclesEnhancedSize fixed = oracles_enhanced_view_size(ORACLES_ENHANCED_FAR, f.aspect);
        snprintf(out, capacity, "Here Fitted is %u\xc3\x97%u at %u\xc3\x97; Fixed is %u\xc3\x97%u at %u\xc3\x97.", f.size.width, f.size.height, f.scale,
                 fixed.width, fixed.height, f.far_scale);
    } else snprintf(out, capacity, "Same on this screen.");
}

void oracles_display_far_note(const OraclesHomeNav *nav, char *out, size_t capacity)
{
    unsigned w, h;
    screen_size(nav, &w, &h);
    const OraclesEnhancedFitting f = oracles_enhanced_far_fitting(w, h);
    if (named_aspect(nav) == ORACLES_ENHANCED_ASPECTS && !nav->display.far_fixed && f.fit == ORACLES_ENHANCED_FIT_FITTED)
        snprintf(out, capacity, "Far fits this screen: %u\xc3\x97%u at %u\xc3\x97.", f.size.width, f.size.height, f.scale);
    else if (capacity) out[0] = 0;
}

OraclesProfile oracles_display_played_profile(const OraclesHomeNav *nav)
{
    const int game = oracles_home_game(nav);
    if (game >= 0 && nav->games[game].usable) {
        const int played = oracles_page_profile(&nav->games[game], nav->display.profile);
        if (played >= 0) return (OraclesProfile)played;
    }
    return nav->display.profile;
}

void oracles_display_surface(const OraclesHomeNav *nav, int *width, int *height)
{
    if (oracles_display_played_profile(nav) == ORACLES_PROFILE_ENHANCED) {
        const OraclesEnhancedSize s = view_surface(nav, nav->display.view);
        *width = (int)s.width;
        *height = (int)s.height;
    } else {
        *width = 160;
        *height = 144;
    }
}

/* The largest whole scale of the surface w x h in room_w x room_h, at least 1. */
static int whole_scale(int room_w, int room_h, int w, int h)
{
    const int kx = room_w / w, ky = room_h / h;
    const int k = kx < ky ? kx : ky;
    return k > 1 ? k : 1;
}

int oracles_display_scale(const OraclesHomeNav *nav, int window)
{
    if (window < 3) return window + 2;
    int w, h;
    oracles_display_surface(nav, &w, &h);
    return whole_scale(nav->display.screen_w, nav->display.screen_h, w, h);
}

int oracles_display_fit(const OraclesHomeNav *nav)
{
    int w, h;
    oracles_display_surface(nav, &w, &h);
    return whole_scale(nav->display.room_w, nav->display.room_h, w, h);
}

int oracles_display_one_size_for(const OraclesHomeDisplay *display, int w, int h)
{
    const int fit = whole_scale(display->room_w, display->room_h, w, h);
    return display->fullscreen_only || (fit <= 2 && fit == whole_scale(display->screen_w, display->screen_h, w, h));
}

int oracles_display_one_size(const OraclesHomeNav *nav)
{
    int w, h;
    oracles_display_surface(nav, &w, &h);
    return oracles_display_one_size_for(&nav->display, w, h);
}

int oracles_display_fullscreen(const OraclesHomeNav *nav)
{
    return nav->display.window == 3 || nav->display.fullscreen_only || (nav->display.scaling && oracles_display_one_size(nav));
}

const char oracles_display_one_size_note[] = "One size only here; Scaling in Picture can fill the screen.";
const char oracles_display_windows_refused_note[] = "Smaller windows change from the home screen.";
const char oracles_display_windows_refused_tail[] = "; smaller ones from the home screen.";

void oracles_display_reduced(const OraclesHomeNav *nav, char *out, size_t capacity)
{
    const int fit = oracles_display_fit(nav);
    if (oracles_display_one_size(nav)) snprintf(out, capacity, "%s", oracles_display_one_size_note);
    else if (nav->display.window >= 0 && nav->display.window < 3 && oracles_display_scale(nav, nav->display.window) > fit)
        snprintf(out, capacity, "Reduced to %d\xc3\x97 to fit this screen%s", fit, nav->display.windows_refused ? oracles_display_windows_refused_tail : ".");
    else if (nav->display.windows_refused) snprintf(out, capacity, "%s", oracles_display_windows_refused_note);
    else if (capacity) out[0] = 0;
}

void oracles_display_window_texts(const OraclesHomeNav *nav, int window, char *name, char *size, size_t capacity)
{
    int w, h;
    oracles_display_surface(nav, &w, &h);
    const int k = oracles_display_scale(nav, window);
    if (window < 3) {
        snprintf(name, capacity, "%d\xc3\x97", k);
        snprintf(size, capacity, "%d\xc3\x97%d", k * w, k * h);
    } else {
        snprintf(name, capacity, "Fullscreen");
        snprintf(size, capacity, "%d\xc3\x97 \xc2\xb7 %d\xc3\x97%d", k, k * w, k * h);
    }
}

const char oracles_display_window_note[] = "Each step is a whole multiple of the game's picture, so pixels stay sharp.";

/* The characters of a window's size on the surface w x h, as oracles_display_window_texts writes it. */
static int window_size_length(const OraclesHomeNav *nav, int window, int w, int h)
{
    char text[48];
    int k = window + 2;
    if (window >= 3) {
        const int kx = nav->display.screen_w / w, ky = nav->display.screen_h / h;
        k = kx < ky ? kx : ky;
        if (k < 1) k = 1;
        snprintf(text, sizeof text, "%d\xc3\x97 \xc2\xb7 %d\xc3\x97%d", k, k * w, k * h);
    } else snprintf(text, sizeof text, "%d\xc3\x97%d", k * w, k * h);
    int length = 0;
    for (const char *p = text; *p; p++) length += ((unsigned char)*p & 0xc0u) != 0x80u;
    return length;
}

int oracles_display_window_size_length(const OraclesHomeNav *nav, int window)
{
    int longest = window_size_length(nav, window, 160, 144);
    for (int v = 0; v < ORACLES_ENHANCED_LEVELS; v++) {
        const OraclesEnhancedSize s = view_surface(nav, v);
        const int length = window_size_length(nav, window, (int)s.width, (int)s.height);
        if (length > longest) longest = length;
    }
    return longest;
}

/* The scale Fill gives the surface w x h on the screen: the largest that keeps its proportions. */
static float fill_scale(const OraclesHomeNav *nav, int w, int h)
{
    const float kx = (float)nav->display.screen_w / (float)w, ky = (float)nav->display.screen_h / (float)h;
    return kx < ky ? kx : ky;
}

void oracles_display_scaling_size(const OraclesHomeNav *nav, int scaling, char *out, size_t capacity)
{
    int w, h;
    oracles_display_surface(nav, &w, &h);
    if (scaling) {
        const float k = fill_scale(nav, w, h);
        snprintf(out, capacity, "%d\xc3\x97%d", (int)floorf(k * (float)w + 0.5f), (int)floorf(k * (float)h + 0.5f));
    } else {
        const int k = oracles_display_scale(nav, 3);
        snprintf(out, capacity, "%d\xc3\x97%d", k * w, k * h);
    }
}

float oracles_display_shown_scale(const OraclesHomeNav *nav)
{
    int sw, sh;
    oracles_display_surface(nav, &sw, &sh);
    /* Fill takes the screen in fullscreen only. */
    if (nav->display.scaling && oracles_display_fullscreen(nav)) return fill_scale(nav, sw, sh);
    if (nav->display.window < 0 && nav->display.window_w > 0 && nav->display.window_h > 0)   /* a window no choice is: the frame's whole scale in it */
        return (float)whole_scale(nav->display.window_w, nav->display.window_h, sw, sh);
    const int k = oracles_display_scale(nav, nav->display.window), fit = oracles_display_fit(nav);
    return (float)(nav->display.window < 3 && k > fit ? fit : k);   /* as Play reduces it */
}

int oracles_display_camera_applies(const OraclesHomeNav *nav) { return oracles_display_hud_applies(nav); }

unsigned oracles_display_camera_scale(const OraclesHomeNav *nav)
{
    const float k = floorf(oracles_display_band_scale(nav));
    return k >= 1.0f ? (unsigned)k : 1u;
}

int oracles_display_camera(const OraclesHomeNav *nav)
{
    if (nav->display.camera >= 0) return nav->display.camera != 0;
    return oracles_display_camera_scale(nav) >= (unsigned)nav->display.camera_from;
}

const char *oracles_display_camera_note(const OraclesHomeNav *nav)
{
    return oracles_display_camera_scale(nav) < 2u ? "No effect at this size: the view fills the screen at 1\xc3\x97." : "";
}

float oracles_display_band_scale(const OraclesHomeNav *nav)
{
    const float k = oracles_display_shown_scale(nav);
    const int margin = nav->display.margin, lines = margin >= 0 && margin < ORACLES_DISPLAY_MARGINS ? nav->display.margin_lines[margin] : 0;
    if (lines <= 0) return k;
    int sw, sh;
    oracles_display_surface(nav, &sw, &sh);
    /* The screen fullscreen, else the window, less the margin's lines: the frame at its largest whole scale there, or
     * filling it with Fill in fullscreen. */
    const int full = oracles_display_fullscreen(nav), sized = nav->display.window < 0 && nav->display.window_w > 0 && nav->display.window_h > 0;
    const float space_w = full ? (float)nav->display.screen_w : sized ? (float)nav->display.window_w : k * (float)sw;
    const float whole_h = full ? (float)nav->display.screen_h : sized ? (float)nav->display.window_h : k * (float)sh;
    if (2.0f * (float)lines > whole_h) return k;   /* a margin that would take half of it is not applied */
    const float space_h = whole_h - (float)lines;
    const float kx = space_w / (float)sw, ky = space_h / (float)sh, fit = kx < ky ? kx : ky;
    const float band = nav->display.scaling && full ? fit : (fit >= 1.0f ? floorf(fit) : 1.0f);
    return band < k ? band : k;
}

void oracles_display_diagram(const OraclesHomeNav *nav, float box_w, float box_h, float *w, float *h, char *label, size_t capacity)
{
    int sw, sh;
    oracles_display_surface(nav, &sw, &sh);
    const float k = oracles_display_shown_scale(nav);
    float image_w = k * (float)sw, image_h = k * (float)sh;
    if (nav->display.window < 0) { image_w = (float)nav->display.window_w; image_h = (float)nav->display.window_h; }   /* a window no choice is */
    /* The window on the screen drawn to the box's scale, rounded to the pixel, and kept inside. */
    *w = floorf(image_w / (float)nav->display.screen_w * box_w + 0.5f);
    *h = floorf(image_h / (float)nav->display.screen_h * box_h + 0.5f);
    if (*w > box_w) *w = box_w;
    if (*h > box_h) *h = box_h;
    snprintf(label, capacity, "%d\xc3\x97%d on a %d\xc3\x97%d screen", (int)floorf(image_w + 0.5f), (int)floorf(image_h + 0.5f),
             nav->display.screen_w, nav->display.screen_h);
}

const char *oracles_display_profile_note(const OraclesHomeNav *nav)
{
    if (oracles_display_played_profile(nav) != nav->display.profile) return "Enhanced needs an original ROM: this game plays in Faithful.";
    return profile_notes[nav->display.profile == ORACLES_PROFILE_ENHANCED];
}

const char *oracles_display_colour_explanation(const OraclesHomeNav *nav, int four_three)
{
    static const char *const texts[2][2] = {
        { "On: colors as the Game Boy Color screen showed them. Off: the raw palette.",
          "On: colors as the Game Boy Color screen showed them. Off: the raw palette. Also F2 in game." },
        { "On: as the Game Boy Color showed it.", "On: as the Game Boy Color showed it. Also F2." },
    };
    return texts[four_three != 0][oracles_home_keys_shown(nav)];
}

const char *oracles_display_explanation(unsigned row)
{
    switch (row) {
        case ORACLES_DISPLAY_VIEW: return "How much of the world the Enhanced view shows; farther asks more of the device.";
        case ORACLES_DISPLAY_HUD: return "Wide layout: hearts left, items right. Full width: the bar's color across.";
        case ORACLES_DISPLAY_TEXT:
        case ORACLES_DISPLAY_LETTERS:
        case ORACLES_DISPLAY_TEXT_BOX:
        case ORACLES_DISPLAY_TEXT_SIZE:
        case ORACLES_DISPLAY_TEXT_SPEED: return oracles_display_text_explanation(row, 0);
        case ORACLES_DISPLAY_TRANSITIONS: return "Rooms scroll into one another instead of stopping at each edge, Link swimming too.";
#ifdef __ANDROID__
        case ORACLES_DISPLAY_VSYNC: return "Auto: off on Android, which composes the frames itself.";
#else
        case ORACLES_DISPLAY_VSYNC: return "Auto: follows your display when it is close to 60 Hz.";
#endif
        case ORACLES_DISPLAY_CORE: return "Accurate: the reference. Fast: lighter, for small devices.";
        case ORACLES_DISPLAY_WORKERS:
            return "Rooms around you are prepared by background workers. Two fill the view faster after a warp or a load, using one more processor core.";
        case ORACLES_DISPLAY_SCALING: return "Sharp: whole steps, every pixel exact. Fill: the whole screen, pixels slightly softer.";
        case ORACLES_DISPLAY_CAMERA:
            return "Smooth: the camera moves by fractions of a pixel at your screen's resolution, so diagonal walks and stairs glide. Pixel: "
                   "whole pixels, as the game does.";
        case ORACLES_DISPLAY_MARGIN:
            return "Keeps the picture clear of screen edges a device hides. Auto follows what the system reports; Small and Large keep a fixed margin at "
                   "the top and bottom.";
        case ORACLES_DISPLAY_ADVANCED: return "Core, transitions, Vsync, workers, camera.";
        case ORACLES_DISPLAY_PICTURE: return "Scaling, screen margin, colors, and the size of menus and scenes.";
        case ORACLES_DISPLAY_MENUS:
            return "Large: menus, map, cutscenes and the file screen alone, at their own largest scale, easier to read on a small screen. View: "
                   "at the view's scale.";
        case ORACLES_DISPLAY_FAR_SIZE: return "Fitted: Far takes a size made for this screen. Fixed: Far's usual size.";
        case ORACLES_DISPLAY_UNVISITED: return "Hidden keeps the rooms you have never entered dark around the game, as on the map.";
        default: return "";   /* Quality's says what is in effect: oracles_display_quality_text */
    }
}

/* The value of a row, set to `value` (wrapped to the row's choices); the view and the transitions do not change in
 * Faithful, nor the HUD and the camera in Faithful or, from the pause, in a game running in Faithful, nor a Text row where it is not
 * live (oracles_display_text_row_live), nor Far size and Menus & scenes where they are not (oracles_display_size_row_live), nor
 * the core and the workers in a game, nor the window at one size, nor to a window the pause refuses.  Advanced, Picture and Text, the
 * links, have no value, nor Quality, which the view, the core and the workers make (quality_set). */
static OraclesHomeCommand display_set(OraclesHomeNav *nav, unsigned row, int value)
{
    static const int counts[ORACLES_DISPLAY_ROWS] = { ORACLES_PROFILES, 0, 4, 3, 3, 0, 2, 0, 2, 2, 3, 3, 2, 2, ORACLES_DISPLAY_MARGINS,
                                                      2, 2, ORACLES_DISPLAY_TEXT_SIZES, 2, 0, 2, 2, 2 };
    int profile = (int)nav->display.profile;
    int *fields[ORACLES_DISPLAY_ROWS] = { &profile, NULL, &nav->display.window, &nav->display.view, &nav->display.hud, NULL,
                                          &nav->display.colour, NULL, &nav->display.core, &nav->display.transitions, &nav->display.vsync,
                                          &nav->display.workers, &nav->display.scaling, NULL, &nav->display.margin,
                                          &nav->display.text, &nav->display.text_box, &nav->display.text_size, &nav->display.text_speed, NULL,
                                          &nav->display.menus, &nav->display.far_fixed, &nav->display.unvisited };
    const int enhanced_only = row == ORACLES_DISPLAY_TRANSITIONS || row == ORACLES_DISPLAY_VIEW;
    if (row == ORACLES_DISPLAY_CAMERA) {   /* the shown value the platform's default until one is chosen, then written */
        value = (value % 2 + 2) % 2;
        if (!oracles_display_camera_applies(nav) || value == oracles_display_camera(nav)) return ORACLES_HOME_STAY;
        nav->display.camera = value;
        return ORACLES_HOME_STORE;
    }
    const int text_row = row >= ORACLES_DISPLAY_LETTERS && row <= ORACLES_DISPLAY_TEXT_SPEED;
    const int size_row = row == ORACLES_DISPLAY_MENUS || row == ORACLES_DISPLAY_FAR_SIZE || row == ORACLES_DISPLAY_UNVISITED;
    if (row >= ORACLES_DISPLAY_ROWS || !fields[row] || (enhanced_only && !oracles_display_transitions_apply(nav))
        || (row == ORACLES_DISPLAY_HUD && !oracles_display_hud_applies(nav)) || (text_row && !oracles_display_text_row_live(nav, row))
        || (size_row && !oracles_display_size_row_live(nav, row))
        || oracles_display_row_fixed(nav, row) || (row == ORACLES_DISPLAY_WINDOW && oracles_display_one_size(nav)))
        return ORACLES_HOME_STAY;
    value = (value % counts[row] + counts[row]) % counts[row];
    if (*fields[row] == value || (row == ORACLES_DISPLAY_WINDOW && (nav->display.windows_refused >> value & 1u))) return ORACLES_HOME_STAY;
    *fields[row] = value;
    if (row == ORACLES_DISPLAY_WINDOW) nav->display.window_chosen = 1;
    nav->display.profile = (OraclesProfile)profile;
    return ORACLES_HOME_STORE;
}

static int display_value(const OraclesHomeNav *nav, unsigned row)
{
    const int values[ORACLES_DISPLAY_ROWS] = { (int)nav->display.profile, 0, nav->display.window, nav->display.view, nav->display.hud, 0,
                                               nav->display.colour, 0, nav->display.core, nav->display.transitions, nav->display.vsync, nav->display.workers,
                                               nav->display.scaling, oracles_display_camera(nav), nav->display.margin, nav->display.text, nav->display.text_box,
                                               nav->display.text_size, nav->display.text_speed, 0, nav->display.menus, nav->display.far_fixed,
                                               nav->display.unvisited };
    return row < ORACLES_DISPLAY_ROWS ? values[row] : 0;
}

/* The value `step` from a row's: Window's steps over the windows the pause refuses. */
static int display_step(const OraclesHomeNav *nav, unsigned row, int step)
{
    int value = display_value(nav, row) + step;
    for (int i = 0; row == ORACLES_DISPLAY_WINDOW && i < 3 && (nav->display.windows_refused >> ((value % 4 + 4) % 4) & 1u); i++) value += step;
    return value;
}

/* Advanced opens on the first of its rows that changes: Core on the home screen, Vsync from a game.  Closed, Display
 * shows Advanced highlighted. */
static void display_advanced(OraclesHomeNav *nav, int open)
{
    nav->advanced = open;
    nav->text_page = nav->picture_page = 0;
    nav->row = ORACLES_DISPLAY_ADVANCED;
    for (unsigned i = 0; open && i < sizeof advanced_rows / sizeof advanced_rows[0]; i++) {
        const unsigned r = advanced_rows[i];
        if (oracles_display_row_fixed(nav, r) || (r == ORACLES_DISPLAY_TRANSITIONS && !oracles_display_transitions_apply(nav))) continue;
        nav->row = r;
        break;
    }
}

/* Display › Picture opens on Scaling, which changes in either profile and from the pause.  Closed, Display shows Picture
 * highlighted. */
static void display_picture(OraclesHomeNav *nav, int open)
{
    nav->picture_page = open;
    nav->advanced = nav->text_page = 0;
    nav->row = open ? ORACLES_DISPLAY_SCALING : ORACLES_DISPLAY_PICTURE;
}

/* Display › Text opens on the first of its rows that changes: Text on the home screen, Text size from a game, Text
 * speed in Faithful; on Text when none does.  Closed, Display shows Text highlighted. */
static void display_text_page(OraclesHomeNav *nav, int open)
{
    nav->text_page = open;
    nav->advanced = nav->picture_page = 0;
    nav->row = open ? ORACLES_DISPLAY_LETTERS : ORACLES_DISPLAY_TEXT;
    for (unsigned r = ORACLES_DISPLAY_LETTERS; open && r <= ORACLES_DISPLAY_TEXT_SPEED; r++) {
        if (!oracles_display_text_row_live(nav, r)) continue;
        nav->row = r;
        break;
    }
}

OraclesHomeCommand oracles_display_act(OraclesHomeNav *nav, OraclesHomeAction action)
{
    /* The page's rows: up and down go round those shown. */
    unsigned count;
    const unsigned *rows = page_rows(nav, &count);
    const unsigned row = oracles_display_row_shown(nav, nav->row) ? nav->row : rows[0];
    switch (action) {
        case ORACLES_HOME_UP:
        case ORACLES_HOME_DOWN: {
            const unsigned step = action == ORACLES_HOME_UP ? count - 1u : 1u;
            unsigned i = row_index(rows, count, row);
            do i = (i + step) % count; while (!oracles_display_row_shown(nav, rows[i]));
            nav->row = rows[i];
            return ORACLES_HOME_STAY;
        }
        case ORACLES_HOME_LEFT:
            if (row == ORACLES_DISPLAY_QUALITY) return quality_step(nav, -1);
            return row == ORACLES_DISPLAY_ADVANCED || row == ORACLES_DISPLAY_TEXT || row == ORACLES_DISPLAY_PICTURE ? ORACLES_HOME_STAY
                                                                                                                     : display_set(nav, row, display_step(nav, row, -1));
        case ORACLES_HOME_RIGHT:
        case ORACLES_HOME_OK:
            if (row == ORACLES_DISPLAY_ADVANCED) { display_advanced(nav, 1); return ORACLES_HOME_STAY; }
            if (row == ORACLES_DISPLAY_PICTURE) { display_picture(nav, 1); return ORACLES_HOME_STAY; }
            if (row == ORACLES_DISPLAY_TEXT) { display_text_page(nav, 1); return ORACLES_HOME_STAY; }
            if (row == ORACLES_DISPLAY_QUALITY) return quality_step(nav, 1);
            return display_set(nav, row, display_step(nav, row, 1));
        case ORACLES_HOME_BACK:
            if (nav->picture_page) { display_picture(nav, 0); return ORACLES_HOME_STAY; }
            if (nav->advanced) { display_advanced(nav, 0); return ORACLES_HOME_STAY; }
            if (nav->text_page) { display_text_page(nav, 0); return ORACLES_HOME_STAY; }
            /* Back to the menu Display was opened from, on Display. */
            nav->screen = nav->in_game ? ORACLES_SCREEN_PAUSE : ORACLES_SCREEN_HOME;
            oracles_home_focus_item(nav, ORACLES_HOME_ITEM_DISPLAY);
            return ORACLES_HOME_STAY;
    }
    return ORACLES_HOME_STAY;
}

OraclesHomeCommand oracles_display_click(OraclesHomeNav *nav, unsigned row, int option)
{
    if (!oracles_display_row_shown(nav, row)) return ORACLES_HOME_STAY;
    if (row == ORACLES_DISPLAY_ADVANCED) { display_advanced(nav, 1); return ORACLES_HOME_STAY; }
    if (row == ORACLES_DISPLAY_PICTURE) { display_picture(nav, 1); return ORACLES_HOME_STAY; }
    if (row == ORACLES_DISPLAY_TEXT) { display_text_page(nav, 1); return ORACLES_HOME_STAY; }
    nav->row = row;
    if (option < 0) return ORACLES_HOME_STAY;
    return row == ORACLES_DISPLAY_QUALITY ? quality_set(nav, option) : display_set(nav, row, option);
}
