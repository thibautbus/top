/* The settings a running game takes (session_internal.h): the cheats, the swim's hold, the instant text, the item hotkeys and the
 * window, from the pause or a replayed route's lines, each written while recording. */
#include "session_internal.h"

#include "home_choices.h"
#include "hotkeys_session.h"
#include "view.h"

#include <stdio.h>

/* The guest of a game that had no hooks (they cost an instruction's check each), attached at the first setting that
 * needs it, at `frame`, and kept until the session ends; 1 when there is one. */
int oracles_session_ensure_guest(session *s, uint32_t frame)
{
    if (s->guest) return 1;
    s->guest = oracles_guest_attach(s->core, s->profile);
    if (!s->guest) return 0;
    oracles_guest_set_frame(s->guest, frame);
    return 1;
}

/* Why a feature turned on from the pause found no point to hook: the guest's table of hooks full, when it refused one
 * just now, else the ROM; the launcher's warning written once more. */
static const char *hooks_why(session *s, unsigned refused_before, const char *rom_reason)
{
    if (!s->guest || oracles_guest_hooks_refused(s->guest) == refused_before) return rom_reason;
    fprintf(stderr, "oracles: WARNING: the guest's table of hooks is full: %u hook(s) not installed (%u installed)\n",
            oracles_guest_hooks_refused(s->guest), oracles_guest_hook_count(s->guest));
    return "the guest's table of hooks is full";
}

/* The cheats from `frame` on, before it runs: the guest attached at the first one turned on, its points armed and kept;
 * each change written while recording. */
void oracles_session_change_cheats(session *s, uint32_t frame, unsigned cheats)
{
    char names[64];
    if (cheats && !oracles_session_ensure_guest(s, frame)) { fprintf(stderr, "oracles: the hooks cannot be armed in this ROM: the cheats stay off\n"); return; }
    const unsigned refused = s->guest ? oracles_guest_hooks_refused(s->guest) : 0;
    if (!s->guest || oracles_guest_set_cheats(s->guest, cheats) != 0) {
        if (cheats) fprintf(stderr, "oracles: the cheats cannot be armed (%s): they stay as they were\n", hooks_why(s, refused, "their points are not in this ROM"));
        return;
    }
    for (unsigned index = 0; s->recording && index < ORACLES_CHEATS; index++)
        if ((cheats ^ s->cheats) & 1u << index) oracles_route_writer_cheat(&s->record, frame, oracles_cheat_name(index), (cheats >> index) & 1u);
    fprintf(stderr, "oracles: cheats from frame %u: %s\n", frame, cheats ? oracles_cheats_format(cheats, NULL, names, sizeof names) : "none");
    s->cheats = cheats;
    s->cheats_used |= cheats;
}

/* The swim's hold from `frame` on, before it runs, as the cheats: its tests armed the first time it is on, and kept. */
void oracles_session_change_swim_hold(session *s, uint32_t frame, int on)
{
    if (on && !oracles_session_ensure_guest(s, frame)) { fprintf(stderr, "oracles: the hooks cannot be armed in this ROM: the swim's hold stays off\n"); return; }
    const unsigned refused = s->guest ? oracles_guest_hooks_refused(s->guest) : 0;
    if (!s->guest || oracles_guest_set_swim_hold(s->guest, on) != 0) {
        if (on) fprintf(stderr, "oracles: the swim's hold cannot be armed (%s): it stays off\n", hooks_why(s, refused, "its strokes are not in this ROM"));
        return;
    }
    if (s->recording && on != s->swim_hold) oracles_route_writer_set(&s->record, frame, ORACLES_ROUTE_SET_SWIM_HOLD, on);
    fprintf(stderr, "oracles: swim's hold %s from frame %u\n", on ? "on" : "off", frame);
    s->swim_hold = on;
}

/* The instant text from `frame` on, before it runs, as the swim's hold. */
void oracles_session_change_text_speed(session *s, uint32_t frame, int instant)
{
    if (instant && !oracles_session_ensure_guest(s, frame)) { fprintf(stderr, "oracles: the hooks cannot be armed in this ROM: the text stays classic\n"); return; }
    const unsigned refused = s->guest ? oracles_guest_hooks_refused(s->guest) : 0;
    if (!s->guest || oracles_guest_set_text_instant(s->guest, instant) != 0) {
        if (instant) fprintf(stderr, "oracles: the instant text cannot be armed (%s): the text stays classic\n", hooks_why(s, refused, "its points are not in this ROM"));
        return;
    }
    if (s->recording && instant != s->text_instant) oracles_route_writer_set(&s->record, frame, ORACLES_ROUTE_SET_TEXT_SPEED, instant);
    fprintf(stderr, "oracles: text speed %s from frame %u\n", instant ? "instant" : "classic", frame);
    s->text_instant = instant;
}

/* The item hotkeys' mode taken at `frame` (oracles_hotkeys_session_settle's result): said, and written while
 * recording. */
void oracles_session_hotkeys_settled(session *s, uint32_t frame, int settled)
{
    static const char *const names[3] = { "off", "use", "equip" };
    if (!settled) return;
    if (settled < 0) fprintf(stderr, "oracles: item hotkeys: the policy could not start again; they stay off\n");
    const OraclesHotkeysMode mode = oracles_hotkeys_session_mode(s->hotkeys);
    if (s->recording && mode != s->hotkeys_mode) oracles_route_writer_set(&s->record, frame, ORACLES_ROUTE_SET_ITEM_HOTKEYS, (int)mode);
    fprintf(stderr, "oracles: item hotkeys %s from frame %u\n", names[mode], frame);
    s->hotkeys_mode = mode;
}

/* The pause's Item hotkeys at the frame the game resumes on: on, the policy started (the guest and the session's
 * hotkeys first, for a game that began without them); off, asked of the policy, which stops once at rest. */
void oracles_session_change_hotkeys(session *s, uint32_t frame, OraclesHotkeysMode mode)
{
    if (s->hotkeys) { oracles_hotkeys_session_set_mode(s->hotkeys, mode); return; }
    if (mode == ORACLES_HOTKEYS_OFF) return;
    if (!oracles_session_ensure_guest(s, frame)) { fprintf(stderr, "oracles: the hooks cannot be armed in this ROM: the item hotkeys stay off\n"); return; }
    const unsigned refused = oracles_guest_hooks_refused(s->guest);
    s->hotkeys = oracles_hotkeys_session_start(s->guest, s->info.game, mode, s->settings, NULL);
    if (!s->hotkeys) {
        fprintf(stderr, "oracles: the item hotkeys cannot be armed (%s): they stay off\n", hooks_why(s, refused, "the inventory's write point is not in this ROM"));
        return;
    }
    if (s->recording) oracles_hotkeys_session_record(s->hotkeys, &s->record);
    oracles_session_hotkeys_settled(s, frame, 1);
}

/* The window from the pause, or a replayed route's `set window` line: fullscreen, or a scale lowered to what the
 * display holds, as Play opens it.  Presentation alone: the game stays the same. */
OraclesEnhancedSize oracles_session_window_surface(const session *s)
{
    const OraclesEnhancedSize surface = { s->view ? oracles_enhanced_view_width(s->view) : ORACLES_SCREEN_WIDTH,
                                          s->view ? oracles_enhanced_view_height(s->view) : ORACLES_SCREEN_HEIGHT };
    return surface;
}

void oracles_session_apply_window(session *s, int window)
{
    if (s->window_setting < 0) return;
    const OraclesEnhancedSize surface = oracles_session_window_surface(s);
    SDL_Window *sdl_window;
    SDL_Renderer *renderer;
    SDL_Texture *frame;
    int width, height, fullscreen;
    unsigned scale;
    oracles_sdl_backend_pause_view(&s->backend, &sdl_window, &renderer, &frame, &width, &height);
    oracles_home_game_window(s->settings, window, sdl_window, surface, &fullscreen, &scale);
    oracles_sdl_backend_set_window(&s->backend, fullscreen, scale);
    if (fullscreen) fprintf(stderr, "oracles: window: fullscreen\n");
    else fprintf(stderr, "oracles: window: scale %u%s\n", scale, window > 0 && scale < (unsigned)window ? ", lowered to what the display holds" : "");
}

void oracles_session_follow_subpixel(session *s)
{
    if (!s->view || !s->subpixel_followed) return;
    const unsigned scale = oracles_sdl_backend_whole_scale(&s->backend);
    if (!scale) return;
    const int on = oracles_settings_subpixel_camera(s->settings, scale);
    const int said = s->subpixel_said;
    s->subpixel_said = 1;
    if (on != oracles_enhanced_view_subpixel_on(s->view)) oracles_enhanced_view_switch_subpixel(s->view, on);
    else if (said) return;
    /* The first frame says the camera and why; a change after it, the scale it came with. */
    if (!said)
        fprintf(stderr, "oracles: camera %s (%s), the view at %ux on the screen\n", on ? "smooth" : "pixel",
                s->settings->subpixel_camera >= 0 ? "subpixel_camera= in the settings" : "the platform's default", scale);
    else fprintf(stderr, "oracles: camera %s, the view at %ux on the screen\n", on ? "smooth" : "pixel", scale);
}
