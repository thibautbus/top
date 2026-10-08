#include "ui_page_nav.h"

#include "compositor.h"

#include <stdio.h>
#include <string.h>

/* The pages' texts: the profiles, the ROM's states, the renderer's values. */
const char *const oracles_profile_names[ORACLES_PROFILES] = { "Faithful", "Enhanced" };
void oracles_profile_size(const OraclesHomeNav *nav, int profile, char *out, size_t capacity)
{
    if (profile == ORACLES_PROFILE_ENHANCED) oracles_display_view_size(nav, nav->display.view, out, capacity);
    else snprintf(out, capacity, "160\xc3\x97" "144");
}
static const char *const profile_notes[ORACLES_PROFILES] = {
    "The core alone, as an unrecognised ROM sees it.", "The wide world, drawn back, with continuous transitions and the smooth camera."
};

static int page_game(const OraclesHomeNav *nav)
{
    const int game = oracles_home_game(nav);
    return game >= 0 ? game : ORACLES_HOME_GAME_AGES;
}

OraclesHomeGame *oracles_page_game(OraclesHomeNav *nav) { return &nav->games[page_game(nav)]; }
const OraclesHomeGame *oracles_page_game_const(const OraclesHomeNav *nav) { return &nav->games[page_game(nav)]; }

int oracles_page_profile_allowed(const OraclesHomeGame *game, OraclesProfile profile)
{
    /* A fan game's image decides, as a ROM does: the one its patch is made to give plays in every profile. */
    const OraclesRomState state = game->fan ? game->image : game->rom;
    if (state == ORACLES_ROM_ORIGINAL) return 1;
    return state == ORACLES_ROM_UNRECOGNISED && profile == ORACLES_PROFILE_FAITHFUL;
}

int oracles_page_profile(const OraclesHomeGame *game, OraclesProfile chosen)
{
    if (oracles_page_profile_allowed(game, chosen)) return (int)chosen;
    for (int p = 0; p < ORACLES_PROFILES; p++) if (oracles_page_profile_allowed(game, (OraclesProfile)p)) return p;
    return -1;
}

const char *oracles_page_rom_file(const OraclesHomeGame *game) { return game->rom_file[0] ? game->rom_file : "None"; }

void oracles_page_rom_status(const OraclesHomeGame *game, char *out, size_t capacity, OraclesTone *tone)
{
    switch (game->rom) {
        case ORACLES_ROM_ORIGINAL:
            if (game->fan) snprintf(out, capacity, "Original ROM, %s", game->fan->base_name);
            else snprintf(out, capacity, "Original ROM, all profiles");
            *tone = ORACLES_TONE_OK;
            break;
        case ORACLES_ROM_UNRECOGNISED:
            /* A base that is not the original: the patch, made for it, is refused on it. */
            if (game->fan) snprintf(out, capacity, "Unrecognised ROM, not %s", game->fan->base_name);
            else snprintf(out, capacity, "Unrecognised ROM, Faithful only");
            *tone = ORACLES_TONE_WARN;
            break;
        case ORACLES_ROM_REFUSED: snprintf(out, capacity, "Refused: %s.", game->rom_reason); *tone = ORACLES_TONE_ERROR; break;
        default: snprintf(out, capacity, game->fan ? "No base ROM chosen" : "No ROM chosen"); *tone = ORACLES_TONE_NONE; break;
    }
}

const char *oracles_page_patch_file(const OraclesHomeGame *game) { return game->patch_file[0] ? game->patch_file : "None"; }

void oracles_page_patch_status(const OraclesHomeGame *game, char *out, size_t capacity, OraclesTone *tone)
{
    switch (game->patch) {
        case ORACLES_ROM_NONE:   /* the player sees which patch the game expects before choosing one */
            snprintf(out, capacity, "No patch: the BPS patch of %s is expected", game->fan ? game->fan->image_name : "the game");
            *tone = ORACLES_TONE_NONE;
            break;
        case ORACLES_ROM_REFUSED: snprintf(out, capacity, "Refused: %s.", game->patch_reason); *tone = ORACLES_TONE_ERROR; break;
        default: snprintf(out, capacity, "Patch file read"); *tone = ORACLES_TONE_OK; break;
    }
}

void oracles_page_image_status(const OraclesHomeGame *game, char *out, size_t capacity, OraclesTone *tone)
{
    switch (game->image) {
        case ORACLES_ROM_ORIGINAL: snprintf(out, capacity, "Patched image recognised: %s", game->fan ? game->fan->image_name : "?"); *tone = ORACLES_TONE_OK; break;
        case ORACLES_ROM_UNRECOGNISED: snprintf(out, capacity, "Patched image not recognised, Faithful only"); *tone = ORACLES_TONE_WARN; break;
        case ORACLES_ROM_REFUSED: snprintf(out, capacity, "Refused: %s.", game->image_reason); *tone = ORACLES_TONE_ERROR; break;
        default: snprintf(out, capacity, "The game needs both files."); *tone = ORACLES_TONE_NONE; break;
    }
}

const char *oracles_page_rom_note(const OraclesHomeNav *nav)
{
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    /* A fan game's image line says Faithful only already. */
    if (!game->usable || game->fan || nav->display.profile != ORACLES_PROFILE_ENHANCED || oracles_page_profile_allowed(game, ORACLES_PROFILE_ENHANCED)) return "";
    return "Enhanced needs an original ROM: this one plays in Faithful";
}

const char *oracles_page_rom_hotkeys_note(const OraclesHomeNav *nav)
{
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    if (!game->usable || !game->hotkeys || !game->hotkeys_failed) return "";
    return "Item hotkeys need an original ROM: this one plays without them";
}

const char *oracles_page_save_file(const OraclesHomeGame *game) { return game->save_file[0] ? game->save_file : "No save yet"; }

void oracles_page_save_line(const OraclesHomeGame *game, char *out, size_t capacity)
{
    if (game->save_file[0]) snprintf(out, capacity, "Last written %s \xc2\xb7 savestates (.state) sit next to it", game->save_written);
    else if (game->usable) snprintf(out, capacity, "Created next to the %s on first save \xc2\xb7 savestates (.state) too", game->fan ? "patch" : "ROM");
    else snprintf(out, capacity, "%s", game->fan ? "Choose both files first" : "Choose a ROM first");
}

const char *oracles_page_play_note(const OraclesHomeNav *nav)
{
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    if (nav->row != ORACLES_ROW_PLAY || game->usable) return "";
    return oracles_home_fan_game(page_game(nav)) ? "Choose both files first" : "Choose a ROM first";
}

/* The page has the row: the Patch row is a fan game's only. */
static int has_row(const OraclesHomeNav *nav, unsigned row)
{
    return row < ORACLES_GAME_ROWS && (row != ORACLES_ROW_PATCH || oracles_page_game_const(nav)->fan);
}

static OraclesHomeCommand activate(OraclesHomeNav *nav, unsigned row)
{
    const OraclesHomeGame *game = oracles_page_game(nav);
    switch (row) {
        case ORACLES_ROW_ROM: return ORACLES_HOME_CHOOSE_ROM;
        case ORACLES_ROW_PATCH: return ORACLES_HOME_CHOOSE_PATCH;
        case ORACLES_ROW_SAVE: return game->usable || game->save_file[0] ? ORACLES_HOME_OPEN_FOLDER : ORACLES_HOME_STAY;
        case ORACLES_ROW_PLAY: return game->usable ? oracles_home_start_command(page_game(nav)) : ORACLES_HOME_STAY;
        default: return ORACLES_HOME_STAY;
    }
}

/* The next row of the page from `row`, one step up or down, wrapping. */
static unsigned step(const OraclesHomeNav *nav, unsigned row, unsigned by)
{
    do row = (row + by) % ORACLES_GAME_ROWS; while (!has_row(nav, row));
    return row;
}

OraclesHomeCommand oracles_page_act(OraclesHomeNav *nav, OraclesHomeAction action)
{
    const unsigned row = has_row(nav, nav->row) ? nav->row : 0;
    switch (action) {
        case ORACLES_HOME_UP: nav->row = step(nav, row, ORACLES_GAME_ROWS - 1u); return ORACLES_HOME_STAY;
        case ORACLES_HOME_DOWN: nav->row = step(nav, row, 1u); return ORACLES_HOME_STAY;
        case ORACLES_HOME_LEFT:
        case ORACLES_HOME_RIGHT: return ORACLES_HOME_STAY;
        case ORACLES_HOME_OK: return activate(nav, row);
        case ORACLES_HOME_BACK:
            /* Back to the home screen, on the item that opened the page. */
            nav->screen = ORACLES_SCREEN_HOME;
            nav->focus = 1;
            return ORACLES_HOME_STAY;
    }
    return ORACLES_HOME_STAY;
}

void oracles_page_hover(OraclesHomeNav *nav, unsigned row)
{
    if (has_row(nav, row)) nav->row = row;
}

OraclesHomeCommand oracles_page_click(OraclesHomeNav *nav, unsigned row)
{
    if (!has_row(nav, row)) return ORACLES_HOME_STAY;
    nav->row = row;
    return activate(nav, row);
}

/* ---- Display --------------------------------------------------------------------- */

const char *const oracles_display_labels[ORACLES_DISPLAY_ROWS] = { "Profile", "Window", "View", "Color correction", "Continuous transitions", "Vsync", "Core" };
const char *const oracles_display_view_names[3] = { "Near", "Medium", "Far" };
const char *const oracles_display_colour_choices[2] = { "Off", "On" };
const char *const oracles_display_vsync_choices[3] = { "Auto", "On", "Off" };
const char *const oracles_display_core_choices[2] = { "Accurate (SameBoy)", "Fast (mGBA)" };
int oracles_display_transitions_apply(const OraclesHomeNav *nav) { return nav->display.profile == ORACLES_PROFILE_ENHANCED; }

int oracles_display_screen_4_3(const OraclesHomeNav *nav)
{
    /* As the session decides it (oracles_session_view_4_3): the settings' shape, else the screen's, the long side over
     * the short one below the middle of 4:3 and 16:9 (oracles_sdl_screen_4_3). */
    if (nav->display.aspect == 2) return 1;   /* ORACLES_ASPECT_4_3 */
    if (nav->display.aspect == 1) return 0;   /* ORACLES_ASPECT_16_9 */
    const int w = nav->display.screen_w, h = nav->display.screen_h;
    if (w <= 0 || h <= 0) return 0;
    const int long_side = w > h ? w : h, short_side = w > h ? h : w;
    return (float)long_side / (float)short_side < (4.0f / 3.0f + 16.0f / 9.0f) / 2.0f;
}

static OraclesEnhancedSize view_surface(const OraclesHomeNav *nav, int view)
{
    const int level = view >= 0 && view < ORACLES_ENHANCED_LEVELS ? view : ORACLES_ENHANCED_FAR;
    return oracles_enhanced_view_size((OraclesEnhancedLevel)level, oracles_display_screen_4_3(nav) ? ORACLES_ENHANCED_4_3 : ORACLES_ENHANCED_16_9);
}

void oracles_display_view_size(const OraclesHomeNav *nav, int view, char *out, size_t capacity)
{
    const OraclesEnhancedSize s = view_surface(nav, view);
    snprintf(out, capacity, "%u\xc3\x97%u", s.width, s.height);
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

int oracles_display_scale(const OraclesHomeNav *nav, int window)
{
    if (window < 3) return window + 2;
    int w, h;
    oracles_display_surface(nav, &w, &h);
    const int kx = nav->display.screen_w / w, ky = nav->display.screen_h / h;
    const int k = kx < ky ? kx : ky;
    return k > 1 ? k : 1;
}

int oracles_display_fit(const OraclesHomeNav *nav)
{
    int w, h;
    oracles_display_surface(nav, &w, &h);
    const int kx = nav->display.room_w / w, ky = nav->display.room_h / h;
    const int k = kx < ky ? kx : ky;
    return k > 1 ? k : 1;
}

void oracles_display_reduced(const OraclesHomeNav *nav, char *out, size_t capacity)
{
    const int fit = oracles_display_fit(nav);
    if (nav->display.window < 3 && oracles_display_scale(nav, nav->display.window) > fit) snprintf(out, capacity, "Reduced to %d\xc3\x97 to fit this screen", fit);
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

void oracles_display_diagram(const OraclesHomeNav *nav, float box_w, float box_h, float *w, float *h, char *label, size_t capacity)
{
    int sw, sh;
    oracles_display_surface(nav, &sw, &sh);
    int k = oracles_display_scale(nav, nav->display.window);
    if (nav->display.window < 3 && k > oracles_display_fit(nav)) k = oracles_display_fit(nav);   /* as Play reduces it */
    /* The window on the screen drawn to the box's scale, rounded to the pixel, and kept inside. */
    *w = (float)(int)((float)(k * sw) / (float)nav->display.screen_w * box_w + 0.5f);
    *h = (float)(int)((float)(k * sh) / (float)nav->display.screen_h * box_h + 0.5f);
    if (*w > box_w) *w = box_w;
    if (*h > box_h) *h = box_h;
    snprintf(label, capacity, "%d\xc3\x97%d on a %d\xc3\x97%d screen", k * sw, k * sh, nav->display.screen_w, nav->display.screen_h);
}

const char *oracles_display_profile_note(const OraclesHomeNav *nav)
{
    if (oracles_display_played_profile(nav) != nav->display.profile) return "Enhanced needs an original ROM: this game plays in Faithful.";
    return profile_notes[nav->display.profile == ORACLES_PROFILE_ENHANCED];
}

const char *oracles_display_explanation(unsigned row)
{
    switch (row) {
        case ORACLES_DISPLAY_VIEW: return "How much of the world the Enhanced view shows; farther asks more of the device.";
        case ORACLES_DISPLAY_COLOUR: return "On: colors as the Game Boy Color screen showed them. Off: the raw palette. Also F2 in game.";
        case ORACLES_DISPLAY_TRANSITIONS: return "Rooms scroll into one another instead of stopping at each edge, Link swimming too.";
        case ORACLES_DISPLAY_VSYNC: return "Auto: follows your display when it is close to 60 Hz.";
        case ORACLES_DISPLAY_CORE: return "Accurate: the reference. Fast: lighter, for small devices.";
        default: return "";
    }
}

/* The value of a row, set to `value` (wrapped to the row's choices); the view and the transitions do not change in Faithful,
 * nor the core in a game. */
static OraclesHomeCommand display_set(OraclesHomeNav *nav, unsigned row, int value)
{
    static const int counts[ORACLES_DISPLAY_ROWS] = { ORACLES_PROFILES, 4, 3, 2, 2, 3, 2 };
    int profile = (int)nav->display.profile;
    int *fields[ORACLES_DISPLAY_ROWS] = { &profile, &nav->display.window, &nav->display.view, &nav->display.colour, &nav->display.transitions,
                                          &nav->display.vsync, &nav->display.core };
    if (row >= ORACLES_DISPLAY_ROWS || ((row == ORACLES_DISPLAY_TRANSITIONS || row == ORACLES_DISPLAY_VIEW) && !oracles_display_transitions_apply(nav))
        || (row == ORACLES_DISPLAY_CORE && nav->in_game))
        return ORACLES_HOME_STAY;
    value = (value % counts[row] + counts[row]) % counts[row];
    if (*fields[row] == value) return ORACLES_HOME_STAY;
    *fields[row] = value;
    nav->display.profile = (OraclesProfile)profile;
    return ORACLES_HOME_STORE;
}

static int display_value(const OraclesHomeNav *nav, unsigned row)
{
    const int values[ORACLES_DISPLAY_ROWS] = { (int)nav->display.profile, nav->display.window, nav->display.view, nav->display.colour, nav->display.transitions,
                                               nav->display.vsync, nav->display.core };
    return row < ORACLES_DISPLAY_ROWS ? values[row] : 0;
}

OraclesHomeCommand oracles_display_act(OraclesHomeNav *nav, OraclesHomeAction action)
{
    const unsigned row = nav->row < ORACLES_DISPLAY_ROWS ? nav->row : 0;
    switch (action) {
        case ORACLES_HOME_UP: nav->row = (row + ORACLES_DISPLAY_ROWS - 1u) % ORACLES_DISPLAY_ROWS; return ORACLES_HOME_STAY;
        case ORACLES_HOME_DOWN: nav->row = (row + 1u) % ORACLES_DISPLAY_ROWS; return ORACLES_HOME_STAY;
        case ORACLES_HOME_LEFT: return display_set(nav, row, display_value(nav, row) - 1);
        case ORACLES_HOME_RIGHT:
        case ORACLES_HOME_OK: return display_set(nav, row, display_value(nav, row) + 1);
        case ORACLES_HOME_BACK:
            /* Back to the menu Display was opened from, on Display. */
            nav->screen = nav->in_game ? ORACLES_SCREEN_PAUSE : ORACLES_SCREEN_HOME;
            nav->focus = nav->in_game ? 4 : 3;
            return ORACLES_HOME_STAY;
    }
    return ORACLES_HOME_STAY;
}

OraclesHomeCommand oracles_display_click(OraclesHomeNav *nav, unsigned row, int option)
{
    if (row >= ORACLES_DISPLAY_ROWS) return ORACLES_HOME_STAY;
    nav->row = row;
    return option < 0 ? ORACLES_HOME_STAY : display_set(nav, row, option);
}
