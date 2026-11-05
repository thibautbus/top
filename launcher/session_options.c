/* What a session plays with, from its options and the settings (session.h): its defaults, its screen margin, what the
 * pause changes of the settings, its save, the mods of the Mods page, and its report's line on the transitions. */
#include "session.h"

#include "compositor.h"
#include "room_predict.h"
#include "room_transition.h"
#include "safe_area.h"
#include "text_box.h"
#include "ui_layout.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

void oracles_session_defaults(OraclesSessionOptions *options)
{
    memset(options, 0, sizeof *options);
    options->neighbour_objects = 1;
    options->sdl.scale = 4;
}

int oracles_session_screen_margin(int option, const oracles_settings *settings)
{
    if (option > 0 && option <= ORACLES_SCREEN_MARGINS) return option - 1;
    return settings->screen_margin >= 0 && settings->screen_margin < ORACLES_SCREEN_MARGINS ? settings->screen_margin : ORACLES_SCREEN_MARGIN_OFF;
}

void oracles_session_report_transitions(const OraclesRoomTransitionStats *stats, FILE *out)
{
    fprintf(out, "oracles: continuous transitions: %u taken over (%u swum, %u started by a stroke at an edge), Link moved on %u frames (%u swimming), held back by a wall ahead on %u, left alone on %u, palette_refreshes %u (background palettes refreshed after a palette mix outlived the doubled scroll); "
                 "the room entered's collisions predicted at %u transitions while the game loaded them (%u the same as the game's, %u not), %u left to the load",
            stats->transitions, stats->swim_transitions, stats->forced_transitions, stats->walked_frames, stats->swum_frames, stats->capped_frames, stats->refused_frames,
            stats->palette_refreshes, stats->predicted_transitions, stats->predicted_matched, stats->predicted_mismatched, stats->predict_fallbacks);
    unsigned listed = 0;
    for (unsigned v = 1; v < ORACLES_ROOM_PREDICT_VERDICTS; v++)
        if (stats->predict_fallback_by[v]) fprintf(out, "%s%s %u", listed++ ? ", " : " (", oracles_room_predict_verdict_name((OraclesRoomPredictVerdict)v), stats->predict_fallback_by[v]);
    fprintf(out, "%s\n", listed ? ")" : "");
}

int oracles_session_view_4_3(const OraclesSessionOptions *o, const oracles_settings *settings, struct SDL_Window *window)
{
    const int aspect = o->aspect ? o->aspect - 1 : settings->aspect;
    return aspect == ORACLES_ASPECT_4_3 ? 1 : aspect == ORACLES_ASPECT_16_9 ? 0 : oracles_sdl_screen_4_3(window);
}

OraclesProfile oracles_session_display_options(OraclesSessionOptions *o, const oracles_settings *settings, int enhanced_allowed, struct SDL_Window *window,
                                               OraclesEnhancedSize *surface)
{
    const int command_line_view = o->enhanced || o->zoom_out || o->view || o->continuous_transitions;
    const OraclesProfile chosen = command_line_view ? ORACLES_PROFILE_ENHANCED : settings->profile;
    const OraclesProfile profile = chosen == ORACLES_PROFILE_ENHANCED && !enhanced_allowed ? ORACLES_PROFILE_FAITHFUL : chosen;
    const int enhanced = profile != ORACLES_PROFILE_FAITHFUL;
    /* Display's Continuous transitions take Link swimming too, on across the axis walking diagonally, and a step a frame
     * as the keys give it, a scroll starting at the line between two rooms; --continuous-transitions alone is the command
     * line's, for the routes recorded with it, and so are --continuous-swim, --continuous-diagonal, --continuous-steps and
     * --continuous-line. */
    o->continuous_swim = enhanced && (o->continuous_swim || settings->transitions);
    o->continuous_diagonal = enhanced && (o->continuous_diagonal || settings->transitions);
    o->continuous_steps = enhanced && (o->continuous_steps || settings->transitions);
    o->continuous_line = enhanced && (o->continuous_line || settings->transitions);
    o->continuous_transitions = enhanced && (o->continuous_transitions || settings->transitions);
    /* The compact text, in Enhanced alone: the command line's, else settings.txt's text=, a fan game's too (in its
     * English, the only language the launcher starts it in); an image no profile qualifies plays without (check_profile). */
    if (!o->compact_text) o->compact_text = settings->compact_text ? 2 : 1;
    if (!enhanced) o->compact_text = 1;
    /* The wide text box, the same: the command line's, else settings.txt's text_box=. */
    if (!o->text_box_wide) o->text_box_wide = settings->text_box_wide ? 2 : 1;
    if (!enhanced) o->text_box_wide = 1;
    if (!enhanced) o->enhanced = o->zoom_out = o->view = 0;
    else if (!command_line_view) { o->enhanced = 1; o->view = settings->view + 1; }   /* Display's View */
    else o->enhanced = 1;
    /* The view's surface: its level in the screen's shape, far fitted to the screen (as the session takes it). */
    o->screen_4_3 = oracles_session_view_4_3(o, settings, window);
    oracles_session_far_screen(o, settings, window, &o->screen_w, &o->screen_h, &o->screen_margin_lines);
    const OraclesEnhancedLevel level = o->view ? (OraclesEnhancedLevel)(o->view - 1) : o->zoom_out ? ORACLES_ENHANCED_FAR : ORACLES_ENHANCED_NEAR;
    *surface = !o->enhanced ? (OraclesEnhancedSize){ 160u, 144u } : oracles_session_view_size(o, (int)level);
    return profile;
}

int oracles_session_vsync_on(const char *vsync, unsigned display_hz)
{
    if (!strcmp(vsync, "on")) return 1;
#ifdef __ANDROID__
    return !strcmp(vsync, "auto") && oracles_settings_auto_vsync(display_hz, 1);
#else
    return !strcmp(vsync, "auto") && oracles_settings_auto_vsync(display_hz, 0);
#endif
}

void oracles_session_far_screen(const OraclesSessionOptions *o, const oracles_settings *settings, struct SDL_Window *window, unsigned *width, unsigned *height,
                                unsigned *margin_lines)
{
    *width = *height = *margin_lines = 0;
    const int aspect = o->aspect ? o->aspect - 1 : settings->aspect;
    if (settings->far_fixed || aspect != ORACLES_ASPECT_AUTO) return;
    if (!oracles_sdl_screen_size(window, width, height)) return;
    OraclesTouchEdges edges;
    oracles_safe_area_edges(window, oracles_session_screen_margin(o->screen_margin, settings), &edges);
    const OraclesTouchBox band = oracles_touch_band((int)*width, (int)*height, &edges);
    *margin_lines = *height - (unsigned)band.h;
}

OraclesEnhancedSize oracles_session_view_size(const OraclesSessionOptions *o, int level)
{
    /* A screen given is one far fits to, its shape the view's; else the options' shape. */
    const OraclesEnhancedAspect aspect = o->screen_w && o->screen_h ? ORACLES_ENHANCED_ASPECTS : o->screen_4_3 ? ORACLES_ENHANCED_4_3 : ORACLES_ENHANCED_16_9;
    return oracles_enhanced_screen_view_size((OraclesEnhancedLevel)level, aspect, 0, o->screen_w, o->screen_h, o->screen_margin_lines);
}

unsigned oracles_session_settings_changed(const oracles_settings *settings, int margin_option, int text_size_option, OraclesSessionApplied *applied)
{
    unsigned changed = 0;
    if (settings->colour_correction != applied->colour) { applied->colour = settings->colour_correction; changed |= ORACLES_SESSION_CHANGED_COLOUR; }
    if (settings->scaling_fill != applied->fill) { applied->fill = settings->scaling_fill; changed |= ORACLES_SESSION_CHANGED_FILL; }
    if (applied->hud >= 0 && settings->hud != applied->hud) { applied->hud = settings->hud; changed |= ORACLES_SESSION_CHANGED_HUD; }
    const int text_size = text_size_option > 0 ? text_size_option - 1 : settings->text_size;
    if (applied->text_size >= 0 && text_size != applied->text_size) { applied->text_size = text_size; changed |= ORACLES_SESSION_CHANGED_TEXT_SIZE; }
    const int margin = oracles_session_screen_margin(margin_option, settings);
    if (margin != applied->margin) { applied->margin = margin; changed |= ORACLES_SESSION_CHANGED_MARGIN; }
    if (applied->swim_hold >= 0 && settings->swim_hold != applied->swim_hold) { applied->swim_hold = settings->swim_hold; changed |= ORACLES_SESSION_CHANGED_SWIM; }
    if (applied->text_instant >= 0 && settings->text_instant != applied->text_instant) {
        applied->text_instant = settings->text_instant;
        changed |= ORACLES_SESSION_CHANGED_TEXT_SPEED;
    }
    const int vsync = !strcmp(settings->vsync, "on") ? 1 : !strcmp(settings->vsync, "off") ? 2 : 0;
    if (vsync != applied->vsync) { applied->vsync = vsync; changed |= ORACLES_SESSION_CHANGED_VSYNC; }
    if (applied->menus_large >= 0 && settings->menus_large != applied->menus_large) { applied->menus_large = settings->menus_large; changed |= ORACLES_SESSION_CHANGED_MENUS; }
    if (applied->unvisited_hidden >= 0 && settings->unvisited_hidden != applied->unvisited_hidden) {
        applied->unvisited_hidden = settings->unvisited_hidden;
        changed |= ORACLES_SESSION_CHANGED_UNVISITED;
    }
    if (applied->hotkeys_game >= 0 && applied->hotkeys_game < ORACLES_SETTINGS_GAMES && (int)settings->item_hotkeys[applied->hotkeys_game] != applied->hotkeys) {
        applied->hotkeys = (int)settings->item_hotkeys[applied->hotkeys_game];
        changed |= ORACLES_SESSION_CHANGED_HOTKEYS;
    }
    return changed;
}

int oracles_session_text_layout(int profile_compact, int profile_host_box, int enhanced, int compact, int wide, unsigned *width, int *fixed)
{
    compact = compact && profile_compact;
    wide = wide && profile_host_box;
    if ((!compact && !wide) || !enhanced) return 0;
    /* The compact text at the game's 128 pixels; in the wide box at its 192, classic at the game's fixed advance. */
    *width = wide ? ORACLES_TEXT_BOX_WIDE_PIXELS : 128u;
    *fixed = !compact;
    return 1;
}

int oracles_session_window_now(int setting, unsigned setting_scale, int fullscreen, int width, int height, int surface_w, int surface_h)
{
    if (setting < 0) return setting;
    if (fullscreen) return 0;
    if (setting > 0 && width == (int)setting_scale * surface_w && height == (int)setting_scale * surface_h) return setting;
    for (int k = 2; k <= 4; k++) if (width == k * surface_w && height == k * surface_h) return k;
    return -2;
}

unsigned oracles_session_windows_refused(const unsigned scales[3], int surface_w, int surface_h, int aspect)
{
    unsigned refused = 0;
    for (unsigned v = 0; v < 3; v++) {
        const int w = (int)scales[v] * surface_w, h = (int)scales[v] * surface_h;
        if (oracles_ui_layout_narrow(oracles_ui_layout_pause(w, h, aspect), w)) refused |= 1u << v;
    }
    return refused;
}

void oracles_session_default_save_path(const char *rom_path, char *out, size_t capacity)
{
    snprintf(out, capacity, "%s", rom_path);
    char *dot = strrchr(out, '.');
    const char *slash = strrchr(out, '/');
    const char *backslash = strrchr(out, '\\');
    if (backslash && (!slash || backslash > slash)) slash = backslash;
    if (dot && (!slash || dot > slash)) *dot = 0;
    const size_t length = strlen(out);
    snprintf(out + length, capacity - length, ".sav");
}

void oracles_session_save_path(const OraclesSessionOptions *o, char *out, size_t capacity)
{
    if (o->save_path) { snprintf(out, capacity, "%s", o->save_path); return; }
    oracles_session_default_save_path(o->patch_path ? o->patch_path : o->rom_path, out, capacity);
    const size_t length = strlen(out);
    if (o->mods_count && length >= 4u) snprintf(out + length - 4u, capacity - (length - 4u), ".mods.sav");   /* for ".sav" */
}

unsigned oracles_session_home_mods(OraclesSessionOptions *o, const oracles_settings *settings, OraclesSettingsGame g,
                                   char dirs[ORACLES_SETTINGS_MODS_MAX][ORACLES_SETTINGS_PATH_LENGTH])
{
    char folder[ORACLES_SETTINGS_PATH_LENGTH], names[ORACLES_SETTINGS_MODS_MAX][ORACLES_SETTINGS_MOD_NAME];
    oracles_settings_mods_folder(settings, folder, sizeof folder);
    const size_t count = folder[0] ? oracles_settings_mod_names(settings->mods[g], names, ORACLES_SETTINGS_MODS_MAX) : 0;
    o->mods_count = 0;
    for (size_t i = 0; i < count; i++) {
        struct stat info;
        char *dir = dirs[o->mods_count];
        if (snprintf(dir, ORACLES_SETTINGS_PATH_LENGTH, "%s/%s", folder, names[i]) >= ORACLES_SETTINGS_PATH_LENGTH) continue;
        if (stat(dir, &info) != 0 || !S_ISDIR(info.st_mode)) { fprintf(stderr, "oracles: mod %s is no longer in %s: it plays without it\n", names[i], folder); continue; }
        o->mods_dirs[o->mods_count++] = dir;
    }
    return o->mods_count;
}
