/* The home screen's keys of settings.txt: each game's ROM, European ROM, language, profile and
 * gameplay options and the launcher window's size are written and read back,
 * a file without them keeps the defaults, a value the launcher does not know
 * leaves its key alone, names that were never the launcher's (zoom-out as
 * a profile, per-game profile and transitions keys) are not read, a key it does
 * not know is kept as it was, spaces and capitals are read as meant; menu_pad=
 * is taken as the session plays it; the end of a session reports the predictions
 * of the transitions, the wrong ones and those left to the load; a savestate taken with other Display settings finds
 * the settings nearest to the player's that load it. */
#include "compositor.h"
#include "home_choices.h"
#include "session.h"
#include "settings.h"
#include "state_display.h"
#include "core.h"
#include "room_transition.h"
#include "ui_controls_nav.h"
#include "ui_page_nav.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define rmdir _rmdir
#else
#include <unistd.h>
#endif

static int failures;

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); failures++; } } while (0)

static oracles_settings written, read_back;

static void write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs(text, f);
    fclose(f);
}

static int file_has_line(const char *path, const char *wanted)
{
    char line[1200];
    int found = 0;
    FILE *f = fopen(path, "r");
    while (f && fgets(line, sizeof line, f)) found |= !strcmp(line, wanted);
    if (f) fclose(f);
    return found;
}

/* The lines of `path`, without their line ends, at most `capacity`: their count. */
static unsigned read_lines(const char *path, char lines[][256], unsigned capacity)
{
    unsigned count = 0;
    FILE *f = fopen(path, "r");
    while (f && count < capacity && fgets(lines[count], sizeof lines[0], f)) lines[count][strcspn(lines[count], "\r\n")] = 0, count++;
    if (f) fclose(f);
    return count;
}

/* The index of the line `wanted` among `count`, or -1. */
static int line_at(char lines[][256], unsigned count, const char *wanted)
{
    for (unsigned i = 0; i < count; i++) if (!strcmp(lines[i], wanted)) return (int)i;
    return -1;
}

/* The keys this version does not know (a later version's, a test build's): kept as they were, each after the key read
 * before it, at the file's start before any, at its end when that key is not written, and once however often the file is
 * read and written; a comment is not a key. */
static void unknown_keys(const char *path)
{
    static char lines[400][256];
    oracles_settings s;
    write_text(path, "later_first=1\n# a comment of the player's\nprofile=faithful\nlater_lens = Wide View\nview=near\n"
                     "quality_hint=low\nlater_after_hint=C:\\Games\\a=b\n");
    oracles_settings_defaults(&s);
    snprintf(s.path, sizeof s.path, "%s", path);
    oracles_settings_load(&s);
    CHECK(s.profile == ORACLES_PROFILE_FAITHFUL && s.view == 0 && s.quality_hint == ORACLES_QUALITY_LOW);
    s.quality_hint = -1;   /* not written: what followed it goes at the end */
    for (int round = 0; round < 2; round++) {
        CHECK(oracles_settings_store(&s) == 1);
        const unsigned count = read_lines(path, lines, 400);
        const int first = line_at(lines, count, "later_first=1"), lens = line_at(lines, count, "later_lens = Wide View");
        const int hint = line_at(lines, count, "later_after_hint=C:\\Games\\a=b");
        CHECK(first >= 0 && first < line_at(lines, count, "colour_correction=1") && lines[first - 1][0] == '#');
        CHECK(lens == line_at(lines, count, "profile=faithful") + 1);
        CHECK(hint == (int)count - 1);
        CHECK(line_at(lines, count, "# a comment of the player's") < 0);
        int copies = 0;
        for (unsigned i = 0; i < count; i++) copies += !strncmp(lines[i], "later_", 6);
        CHECK(copies == 3);
        oracles_settings_defaults(&s);
        snprintf(s.path, sizeof s.path, "%s", path);
        oracles_settings_load(&s);
    }
    /* A file without any keeps none. */
    write_text(path, "profile=faithful\n");
    oracles_settings_load(&s);
    CHECK(s.unknown[0] == 0);
}

/* Spaces around a key's name and its value, and capitals, as a player writes them by hand: `update_check = OFF` turns
 * the check off, and is written back update_check=off; a path keeps its own spaces and case. */
static void spaces_and_case(const char *path)
{
    oracles_settings s;
    write_text(path, "update_check = OFF\n Menus=Large \t\nVIEW= Medium\nrom_ages =  C:\\My Games\\Ages.gbc  \nkey_a = X\nghosts= 2\n");
    oracles_settings_defaults(&s);
    snprintf(s.path, sizeof s.path, "%s", path);
    oracles_settings_load(&s);
    CHECK(s.update_check == 0 && s.menus_large == 1 && s.view == 1 && s.ghosts == 2);
    CHECK(!strcmp(s.rom[ORACLES_SETTINGS_AGES], "C:\\My Games\\Ages.gbc") && !strcmp(s.key_names[4], "X"));
    CHECK(s.unknown[0] == 0);
    oracles_settings_store(&s);
    CHECK(file_has_line(path, "update_check=off\n") && file_has_line(path, "menus=large\n") && file_has_line(path, "view=medium\n"));
    write_text(path, "Update_Check=On\n");
    oracles_settings_load(&s);
    CHECK(s.update_check == 1);
}

/* menu_pad= `name` with pad_select= `select` and pad_hotkey1= `hotkey`, read as every screen takes it: the menu's
 * button that remains, and its first game's toast. */
static int menu_pad_is(const char *name, const char *select, const char *hotkey, const char *wanted, const char *toast)
{
    oracles_settings s;
    oracles_settings_defaults(&s);
    snprintf(s.menu_pad, sizeof s.menu_pad, "%s", name);
    snprintf(s.pad_names[2], sizeof s.pad_names[2], "%s", select);
    snprintf(s.hotkey_pad_names[0], sizeof s.hotkey_pad_names[0], "%s", hotkey);
    oracles_home_resolve_menu_pad(&s);
    OraclesHomeControls controls;
    oracles_home_inspect_controls(&s, &controls);
    char text[64];
    oracles_controls_menu_tip(ORACLES_INPUT_GAMEPAD, 0, s.menu_pad, text, sizeof text);
    return !strcmp(s.menu_pad, wanted) && !strcmp(controls.menu_pad, wanted) && !strcmp(text, toast);
}

/* The menu's button is the one the session plays with: a 1.1.0 player's left stick's click, which an item hotkey slot
 * or the game's Select may have, leaves the menu to Guide alone, without a reminder promising L3; an unknown name is
 * the default, a trigger stays. */
static void menu_pad_resolved(void)
{
    CHECK(menu_pad_is("leftstick", "back", "x", "leftstick", "L3 opens the menu"));
    CHECK(menu_pad_is("righttrigger", "back", "x", "righttrigger", "RT opens the menu"));
    CHECK(menu_pad_is("leftstick", "back", "leftstick", "", "Guide opens the menu"));
    CHECK(menu_pad_is("leftstick", "leftstick", "x", "", "Guide opens the menu"));
    CHECK(menu_pad_is("nosuchbutton", "back", "x", "leftstick", "L3 opens the menu"));
    CHECK(menu_pad_is("nosuchbutton", "back", "leftstick", "", "Guide opens the menu"));
    CHECK(menu_pad_is("dpup", "back", "x", "", "Guide opens the menu") && menu_pad_is("guide", "back", "x", "", "Guide opens the menu")
          && menu_pad_is("a", "back", "x", "", "Guide opens the menu"));
    CHECK(menu_pad_is("", "back", "x", "", "Guide opens the menu"));
}

/* Display's choices written back: from the pause, the running game's core and workers are not; a margin
 * --screen-margin gave for the run never is, nor a text, a text box or a text size --text, --text-box or --text-size
 * gave, whatever the row holds. */
static void display_stored(void)
{
    oracles_settings s;
    oracles_settings_defaults(&s);
    OraclesHomeDisplay display;
    oracles_home_inspect_display(&s, &display, NULL);
    CHECK(!display.margin_fixed && display.margin == ORACLES_SCREEN_MARGIN_OFF);
    display.margin = ORACLES_SCREEN_MARGIN_LARGE;
    display.margin_fixed = 1;
    display.hud = 0;
    oracles_home_store_display(&display, 0, &s);
    CHECK(s.screen_margin == ORACLES_SCREEN_MARGIN_OFF && s.hud == 0);
    /* Camera: absent until chosen, then stored as chosen, from the pause too; the platform's threshold given to the page. */
    CHECK(display.camera == -1 && s.subpixel_camera == -1 && display.camera_from == (int)oracles_settings_subpixel_default_from());
    display.camera = 0;
    oracles_home_store_display(&display, 1, &s);
    CHECK(s.subpixel_camera == 0);
    oracles_home_inspect_display(&s, &display, NULL);
    CHECK(display.camera == 0);
    display.margin = ORACLES_SCREEN_MARGIN_LARGE;
    display.hud = 0;
    display.margin_fixed = 0;
    display.workers = 2;
    oracles_home_store_display(&display, 1, &s);
    CHECK(s.screen_margin == ORACLES_SCREEN_MARGIN_LARGE && s.ghosts != 2);
    oracles_home_store_display(&display, 0, &s);
    CHECK(s.ghosts == 2);
    /* The text is stored from the pause too, for the next Play, but never --text's. */
    CHECK(display.text == 0 && !display.text_fixed);
    display.text = 1;
    display.text_fixed = 1;
    oracles_home_store_display(&display, 0, &s);
    CHECK(s.compact_text == 0);
    display.text_fixed = 0;
    oracles_home_store_display(&display, 1, &s);
    CHECK(s.compact_text == 1);
    s.compact_text = 0;
    oracles_home_inspect_display(&s, &display, NULL);
    CHECK(display.text == 0);
    /* Display › Text's box and size the same: stored, Very large as xlarge, never --text-box's nor --text-size's. */
    CHECK(display.text_box == 0 && display.text_size == 0 && !display.text_box_fixed && !display.text_size_fixed);
    display.text_box = 1;
    display.text_size = 2;
    display.text_box_fixed = display.text_size_fixed = 1;
    oracles_home_store_display(&display, 0, &s);
    CHECK(s.text_box_wide == 0 && s.text_size == 0);
    display.text_box_fixed = display.text_size_fixed = 0;
    oracles_home_store_display(&display, 0, &s);
    CHECK(s.text_box_wide == 1 && s.text_size == 2);
    oracles_home_inspect_display(&s, &display, NULL);
    CHECK(display.text_box == 1 && display.text_size == 2);
    /* Display › Picture: Menus & scenes the platform's default (View on the desktops), stored from the pause too; Far
     * size Fitted, stored from the home screen alone. */
    CHECK(display.menus == 0 && display.far_fixed == 0);
    display.menus = display.far_fixed = 1;
    oracles_home_store_display(&display, 1, &s);
    CHECK(s.menus_large == 1 && s.far_fixed == 0);
    oracles_home_store_display(&display, 0, &s);
    CHECK(s.far_fixed == 1);
    oracles_home_inspect_display(&s, &display, NULL);
    CHECK(display.menus == 1 && display.far_fixed == 1);
}

/* What the pause's Display changed applies to the running game at once: the HUD where the Enhanced view draws it, the
 * margin unless --screen-margin holds it for the run. */
/* The report's transitions line, in a file beside `path`: the predictions' mismatches and the fallbacks by reason. */
static void transitions_line(const char *path)
{
    char report[1100], line[1200];
    snprintf(report, sizeof report, "%s.report", path);
    OraclesRoomTransitionStats stats;
    memset(&stats, 0, sizeof stats);
    stats.transitions = 12, stats.swim_transitions = 3, stats.walked_frames = 300, stats.swum_frames = 70, stats.palette_refreshes = 1;
    stats.predicted_transitions = 9, stats.predicted_matched = 8, stats.predicted_mismatched = 1, stats.predict_fallbacks = 3;
    stats.predict_fallback_by[ORACLES_ROOM_PREDICT_LARGE] = 2, stats.predict_fallback_by[ORACLES_ROOM_PREDICT_ROOM_CODE] = 1;
    for (int fallbacks = 1; fallbacks >= 0; fallbacks--) {
        if (!fallbacks) {
            stats.predict_fallbacks = 0;
            memset(stats.predict_fallback_by, 0, sizeof stats.predict_fallback_by);
        }
        FILE *f = fopen(report, "w");
        CHECK(f != NULL);
        if (!f) return;
        oracles_session_report_transitions(&stats, f);
        fclose(f);
        line[0] = 0;
        f = fopen(report, "r");
        if (f && !fgets(line, sizeof line, f)) line[0] = 0;
        if (f) fclose(f);
        CHECK(!strncmp(line, "oracles: continuous transitions: 12 taken over (3 swum, 0 started by a stroke at an edge), Link moved on 300 frames (70 swimming), ", 131));
        CHECK(strstr(line, "palette_refreshes 1 (") != NULL);
        CHECK(strstr(line, "; the room entered's collisions predicted at 9 transitions while the game loaded them (8 the same as the game's, 1 not), ") != NULL);
        if (fallbacks) CHECK(strstr(line, ", 3 left to the load (a large room or a dungeon's group 2, the room's own code 1)\n") != NULL);
        else CHECK(strstr(line, ", 0 left to the load\n") != NULL);
    }
    remove(report);
}

static void pause_changes(void)
{
    oracles_settings s;
    oracles_settings_defaults(&s);
    OraclesSessionApplied applied = { s.colour_correction, s.scaling_fill, s.hud, s.screen_margin, s.text_size, -1, -1, 0, -1, 0, -1, -1 };
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == 0);
    s.hud = !s.hud;
    s.screen_margin = ORACLES_SCREEN_MARGIN_SMALL;
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == (ORACLES_SESSION_CHANGED_HUD | ORACLES_SESSION_CHANGED_MARGIN));
    CHECK(applied.hud == s.hud && applied.margin == ORACLES_SCREEN_MARGIN_SMALL);
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == 0);
    s.hud = 2;   /* the wide layout, at once too */
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == ORACLES_SESSION_CHANGED_HUD && applied.hud == 2);
    /* --screen-margin large for the run: the settings' margin changes nothing, the option's is applied once. */
    s.screen_margin = ORACLES_SCREEN_MARGIN_AUTO;
    CHECK(oracles_session_settings_changed(&s, ORACLES_SCREEN_MARGIN_LARGE + 1, 0, &applied) == ORACLES_SESSION_CHANGED_MARGIN && applied.margin == ORACLES_SCREEN_MARGIN_LARGE);
    s.screen_margin = ORACLES_SCREEN_MARGIN_OFF;
    CHECK(oracles_session_settings_changed(&s, ORACLES_SCREEN_MARGIN_LARGE + 1, 0, &applied) == 0);
    /* Text size, at once. */
    s.text_size = 1;
    CHECK(oracles_session_settings_changed(&s, ORACLES_SCREEN_MARGIN_LARGE + 1, 0, &applied) == ORACLES_SESSION_CHANGED_TEXT_SIZE && applied.text_size == 1);
    s.text_size = 2;
    CHECK(oracles_session_settings_changed(&s, ORACLES_SCREEN_MARGIN_LARGE + 1, 0, &applied) == ORACLES_SESSION_CHANGED_TEXT_SIZE && applied.text_size == 2);
    /* --text-size xlarge for the run: the settings' Normal and another row's change leave it, the game keeps Very large. */
    s.text_size = 0;
    CHECK(oracles_session_settings_changed(&s, ORACLES_SCREEN_MARGIN_LARGE + 1, 3, &applied) == 0 && applied.text_size == 2);
    s.colour_correction = !s.colour_correction;
    CHECK(oracles_session_settings_changed(&s, ORACLES_SCREEN_MARGIN_LARGE + 1, 3, &applied) == ORACLES_SESSION_CHANGED_COLOUR && applied.text_size == 2);
    s.colour_correction = !s.colour_correction;
    CHECK(oracles_session_settings_changed(&s, ORACLES_SCREEN_MARGIN_LARGE + 1, 3, &applied) == ORACLES_SESSION_CHANGED_COLOUR);
    s.text_size = 2;
    /* Without the Enhanced view (Faithful) there is no HUD to change, nor a text size. */
    applied.hud = -1;
    applied.text_size = -1;
    s.text_size = 0;
    s.hud = !s.hud;
    s.colour_correction = !s.colour_correction;
    s.scaling_fill = !s.scaling_fill;
    CHECK(oracles_session_settings_changed(&s, ORACLES_SCREEN_MARGIN_LARGE + 1, 0, &applied) == (ORACLES_SESSION_CHANGED_COLOUR | ORACLES_SESSION_CHANGED_FILL));
    CHECK(applied.hud == -1 && applied.colour == s.colour_correction && applied.fill == s.scaling_fill);
}

/* The texts' layout a session plays: a fan game, with the compact text and no host's box, laid out for the game's box
 * whatever the wide box says, and not at all with the classic text; the originals at 192 pixels in the wide box. */
static void text_layout(void)
{
    unsigned width = 0;
    int fixed = -1;
    CHECK(oracles_session_text_layout(1, 0, 1, 1, 1, &width, &fixed) == 1 && width == 128u && fixed == 0);
    CHECK(oracles_session_text_layout(1, 0, 1, 0, 1, &width, &fixed) == 0);
    CHECK(oracles_session_text_layout(1, 1, 1, 0, 1, &width, &fixed) == 1 && width == 192u && fixed == 1);
    CHECK(oracles_session_text_layout(1, 1, 1, 1, 1, &width, &fixed) == 1 && width == 192u && fixed == 0);
    CHECK(oracles_session_text_layout(1, 1, 1, 1, 0, &width, &fixed) == 1 && width == 128u && fixed == 0);
    CHECK(oracles_session_text_layout(1, 1, 0, 1, 1, &width, &fixed) == 0);
    CHECK(oracles_session_text_layout(0, 0, 1, 1, 1, &width, &fixed) == 0);
}

/* The swim's hold and the item hotkeys from the pause's Controls: each where the game has it (-1 where it has none), the
 * item hotkeys of the settings' game it plays, the others' changing nothing; the window never: the pause applies it
 * when Window is chosen, the settings' window_scale stored only then. */
static void pause_live_changes(void)
{
    oracles_settings s;
    oracles_settings_defaults(&s);
    OraclesSessionApplied applied = { s.colour_correction, s.scaling_fill, s.hud, s.screen_margin, s.text_size, s.swim_hold, ORACLES_SETTINGS_SEASONS,
                                      (int)s.item_hotkeys[ORACLES_SETTINGS_SEASONS], s.text_instant, 0, s.menus_large, s.unvisited_hidden };
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == 0);
    /* Vsync, at once: off, then on, then auto again; it presents in step with a display near 60 Hz alone on auto
     * (never on Android), always on on, never on off. */
    snprintf(s.vsync, sizeof s.vsync, "off");
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == ORACLES_SESSION_CHANGED_VSYNC && applied.vsync == 2);
    snprintf(s.vsync, sizeof s.vsync, "on");
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == ORACLES_SESSION_CHANGED_VSYNC && applied.vsync == 1);
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == 0);
    snprintf(s.vsync, sizeof s.vsync, "auto");
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == ORACLES_SESSION_CHANGED_VSYNC && applied.vsync == 0);
#ifdef __ANDROID__
    CHECK(!oracles_session_vsync_on("auto", 60u));
#else
    CHECK(oracles_session_vsync_on("auto", 60u) && !oracles_session_vsync_on("auto", 144u));
#endif
    CHECK(oracles_session_vsync_on("on", 144u) && !oracles_session_vsync_on("off", 60u));
    /* The large menus, at once, in a game with the Enhanced view; none without it. */
    s.menus_large = !s.menus_large;
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == ORACLES_SESSION_CHANGED_MENUS && applied.menus_large == s.menus_large);
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == 0);
    applied.menus_large = -1;
    s.menus_large = !s.menus_large;
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == 0 && applied.menus_large == -1);
    /* The rooms never visited, when the game resumes, with the Enhanced view; none without it, nor while --unvisited-rooms holds them. */
    s.unvisited_hidden = 1;
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == ORACLES_SESSION_CHANGED_UNVISITED && applied.unvisited_hidden == 1);
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == 0);
    applied.unvisited_hidden = -1;
    s.unvisited_hidden = 0;
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == 0 && applied.unvisited_hidden == -1);
    s.swim_hold = !s.swim_hold;
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == ORACLES_SESSION_CHANGED_SWIM && applied.swim_hold == s.swim_hold);
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == 0);
    s.item_hotkeys[ORACLES_SETTINGS_AGES] = ORACLES_HOTKEYS_USE;   /* the other Oracle's: nothing */
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == 0);
    s.item_hotkeys[ORACLES_SETTINGS_SEASONS] = ORACLES_HOTKEYS_USE;
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == ORACLES_SESSION_CHANGED_HOTKEYS && applied.hotkeys == ORACLES_HOTKEYS_USE);
    s.item_hotkeys[ORACLES_SETTINGS_SEASONS] = ORACLES_HOTKEYS_OFF;
    s.window_scale = 3;
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == ORACLES_SESSION_CHANGED_HOTKEYS && applied.hotkeys == ORACLES_HOTKEYS_OFF);
    /* The text speed, for every game whose profile has the instant text, from the frame the game resumes on. */
    s.text_instant = 1;
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == ORACLES_SESSION_CHANGED_TEXT_SPEED && applied.text_instant == 1);
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == 0);
    s.text_instant = 0;
    applied.text_instant = -1;   /* a game without it (an unrecognised ROM): nothing */
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == 0 && applied.text_instant == -1);
    /* A game whose profile has neither the hold nor the item hotkeys (a fan game), without a window to change: nothing. */
    applied.swim_hold = applied.hotkeys_game = -1;
    s.swim_hold = !s.swim_hold;
    s.item_hotkeys[ORACLES_SETTINGS_SEASONS] = ORACLES_HOTKEYS_USE;
    s.window_scale = 2;
    CHECK(oracles_session_settings_changed(&s, 0, 0, &applied) == 0 && applied.swim_hold == -1 && applied.hotkeys_game == -1);
    /* The window the game takes for Display's choice, as Play opens it: a scale, fullscreen for 0; without a display to
     * measure, the scale asked for. */
    int fullscreen = -1;
    unsigned scale = 0;
    oracles_home_game_window(&s, 3, NULL, (OraclesEnhancedSize){ 256u, 144u }, &fullscreen, &scale);
    CHECK(!fullscreen && scale == 3);
    oracles_home_game_window(&s, 0, NULL, (OraclesEnhancedSize){ 256u, 144u }, &fullscreen, &scale);
    CHECK(fullscreen && scale == 4);
    /* The window the pause's Window shows, F11's toggle included: fullscreen whatever the setting; a window, the
     * setting when the window is the scale it opens at, else the scale the window is exactly, else none (-2): F11 out
     * of a fullscreen start keeps the home screen's 1280x720, no whole scale of Near; -1 where it cannot change. */
    CHECK(oracles_session_window_now(3, 3, 1, 1920, 1080, 256, 144) == 0);
    CHECK(oracles_session_window_now(3, 3, 0, 768, 432, 256, 144) == 3);
    CHECK(oracles_session_window_now(4, 3, 0, 768, 432, 256, 144) == 4);   /* 4x lowered to 3x: the setting */
    CHECK(oracles_session_window_now(0, 0, 0, 1024, 576, 256, 144) == 4 && oracles_session_window_now(0, 0, 0, 640, 576, 160, 144) == 4);
    CHECK(oracles_session_window_now(0, 0, 0, 768, 432, 256, 144) == 3 && oracles_session_window_now(3, 3, 0, 512, 288, 256, 144) == 2);
    CHECK(oracles_session_window_now(0, 0, 0, 1280, 720, 256, 144) == -2 && oracles_session_window_now(0, 0, 0, 2560, 1440, 256, 144) == -2);
    CHECK(oracles_session_window_now(3, 3, 0, 800, 450, 256, 144) == -2);   /* resized by hand */
    CHECK(oracles_session_window_now(0, 0, 0, 1920, 1080, 160, 144) == -2);
    CHECK(oracles_session_window_now(-1, 0, 1, 1920, 1080, 256, 144) == -1);
    /* From the pause, Display stores the window only once Window is chosen: F11's fullscreen shown in the row stays out
     * of the settings through any other change. */
    OraclesHomeDisplay display;
    oracles_home_inspect_display(&s, &display, NULL);
    CHECK(display.window == 0);
    display.window = 3;
    display.hud = !display.hud;
    oracles_home_store_display(&display, 1, &s);
    CHECK(s.window_scale == 2 && s.hud == display.hud);
    display.window_chosen = 1;
    oracles_home_store_display(&display, 1, &s);
    CHECK(s.window_scale == 0);
    display.window = -1;   /* none shown: never stored */
    oracles_home_store_display(&display, 0, &s);
    CHECK(s.window_scale == 0);
    display.window = 1;
    display.window_chosen = 0;
    oracles_home_store_display(&display, 0, &s);   /* the home screen stores it always */
    CHECK(s.window_scale == 3);
    /* The windows whose pause would lose Display: Near's and Faithful's 2x and 3x, Medium's 2x, none of Far's; a 4x
     * lowered to 3x by a small display too. */
    static const unsigned scales[3] = { 2u, 3u, 4u }, lowered[3] = { 2u, 3u, 3u };
    CHECK(oracles_session_windows_refused(scales, 256, 144, ORACLES_ASPECT_AUTO) == 3u);
    CHECK(oracles_session_windows_refused(scales, 160, 144, ORACLES_ASPECT_AUTO) == 3u);
    CHECK(oracles_session_windows_refused(scales, 384, 216, ORACLES_ASPECT_AUTO) == 1u);
    CHECK(oracles_session_windows_refused(scales, 480, 270, ORACLES_ASPECT_AUTO) == 0u);
    CHECK(oracles_session_windows_refused(lowered, 256, 144, ORACLES_ASPECT_AUTO) == 7u);
    CHECK(oracles_session_windows_refused(scales, 160, 144, ORACLES_ASPECT_16_9) == 7u);   /* 640 wide in 16:9 */
}

/* A screen for state_display: 16:9 at 1920x1080, or 4:3 at 1280x960, where far fitted is 426x320 without a margin. */
typedef struct fake_screen { int four_three; } fake_screen;

static void fake_play(void *opaque, const oracles_settings *settings, OraclesStatePlay *out)
{
    const fake_screen *screen = opaque;
    memset(out, 0, sizeof *out);
    out->core = oracles_settings_core(settings);
    out->enhanced = settings->profile == ORACLES_PROFILE_ENHANCED;
    if (!out->enhanced) return;
    out->transitions = settings->transitions;
    out->compact = settings->compact_text;
    out->wide = settings->text_box_wide;
    const int four_three = settings->aspect == ORACLES_ASPECT_4_3 || (settings->aspect == ORACLES_ASPECT_AUTO && screen->four_three);
    OraclesEnhancedSize size = oracles_enhanced_view_size((OraclesEnhancedLevel)settings->view, four_three ? ORACLES_ENHANCED_4_3 : ORACLES_ENHANCED_16_9);
    if (settings->view == ORACLES_ENHANCED_FAR && screen->four_three && settings->aspect == ORACLES_ASPECT_AUTO && !settings->far_fixed
        && settings->screen_margin == ORACLES_SCREEN_MARGIN_OFF)
        size = oracles_enhanced_fitted_size(0);
    out->width = size.width;
    out->height = size.height;
}

/* The player's settings: Enhanced, Far, the transitions on, the classic text, the Accurate core, no margin. */
static void player(oracles_settings *s)
{
    oracles_settings_defaults(s);
    s->profile = ORACLES_PROFILE_ENHANCED;
    s->view = ORACLES_ENHANCED_FAR;
    s->transitions = 1;
    s->core = ORACLES_CORE_SAMEBOY;
    s->screen_margin = ORACLES_SCREEN_MARGIN_OFF;
    s->aspect = ORACLES_ASPECT_AUTO;
    s->far_fixed = 0;
}

static void state_display(void)
{
    static oracles_settings s, before;
    char notice[192];
    fake_screen wide = { 0 }, square = { 1 };
    /* As the player plays: nothing to change. */
    OraclesStatePlay taken = { ORACLES_CORE_SAMEBOY, 1, 1, 0, 0, 480u, 270u };
    player(&s);
    CHECK(oracles_state_display_settings(&taken, &s, fake_play, &wide, notice, sizeof notice) == 0 && !notice[0]);
    /* Another core, then another view too: those alone change. */
    taken.core = ORACLES_CORE_MGBA;
    CHECK(oracles_state_display_settings(&taken, &s, fake_play, &wide, notice, sizeof notice));
    CHECK(s.core == ORACLES_CORE_MGBA && s.view == ORACLES_ENHANCED_FAR && s.profile == ORACLES_PROFILE_ENHANCED);
    CHECK(!strcmp(notice, "Display set to Fast (mGBA) to load this savestate"));
    player(&s);
    taken.width = 256u;
    taken.height = 144u;
    CHECK(oracles_state_display_settings(&taken, &s, fake_play, &wide, notice, sizeof notice));
    CHECK(s.core == ORACLES_CORE_MGBA && s.view == ORACLES_ENHANCED_NEAR && s.aspect == ORACLES_ASPECT_AUTO);
    CHECK(!strcmp(notice, "Display set to Fast (mGBA) and Near to load this savestate"));
    /* With the transitions too: three changes, Enhanced kept. */
    player(&s);
    taken.transitions = 0;
    CHECK(oracles_state_display_settings(&taken, &s, fake_play, &wide, notice, sizeof notice));
    CHECK(s.profile == ORACLES_PROFILE_ENHANCED && !s.transitions);
    CHECK(!strcmp(notice, "Display set to Fast (mGBA), Continuous transitions Off and Near to load this savestate"));
    /* Taken with the transitions, played in Faithful: Enhanced, which brings them and Far back. */
    player(&s);
    s.profile = ORACLES_PROFILE_FAITHFUL;
    const OraclesStatePlay transitions = { ORACLES_CORE_SAMEBOY, 1, 1, 0, 0, 480u, 270u };
    CHECK(oracles_state_display_settings(&transitions, &s, fake_play, &wide, notice, sizeof notice));
    CHECK(s.profile == ORACLES_PROFILE_ENHANCED && !strcmp(notice, "Display set to Enhanced to load this savestate"));
    /* Taken in Faithful (no surface), played with the transitions: they go, the profile stays. */
    player(&s);
    const OraclesStatePlay faithful = { ORACLES_CORE_SAMEBOY, 0, 0, 0, 0, 0u, 0u };
    CHECK(oracles_state_display_settings(&faithful, &s, fake_play, &wide, notice, sizeof notice));
    CHECK(s.profile == ORACLES_PROFILE_ENHANCED && !s.transitions);
    CHECK(!strcmp(notice, "Display set to Continuous transitions Off to load this savestate"));
    /* Taken with the compact text in the wide box. */
    player(&s);
    const OraclesStatePlay text = { ORACLES_CORE_SAMEBOY, 1, 1, 1, 1, 480u, 270u };
    CHECK(oracles_state_display_settings(&text, &s, fake_play, &wide, notice, sizeof notice));
    CHECK(s.compact_text && s.text_box_wide && !strcmp(notice, "Display set to Compact text and Wide text box to load this savestate"));
    /* On a 4:3 screen where far is fitted: far's fixed 4:3 size is Far size's Fixed, the margin left alone; its 16:9 size
     * only aspect= gives, which is no choice of Display: refused, nothing changed. */
    player(&s);
    const OraclesStatePlay fixed = { ORACLES_CORE_SAMEBOY, 1, 1, 0, 0, 480u, 360u };
    CHECK(oracles_state_display_settings(&fixed, &s, fake_play, &square, notice, sizeof notice));
    CHECK(s.far_fixed && s.aspect == ORACLES_ASPECT_AUTO && s.screen_margin == ORACLES_SCREEN_MARGIN_OFF);
    CHECK(!strcmp(notice, "Display set to Far size Fixed to load this savestate"));
    player(&s);
    before = s;
    const OraclesStatePlay sixteen_nine = { ORACLES_CORE_SAMEBOY, 1, 1, 0, 0, 480u, 270u };
    CHECK(oracles_state_display_settings(&sixteen_nine, &s, fake_play, &square, notice, sizeof notice) == -1 && !notice[0]);
    CHECK(!memcmp(&s, &before, sizeof s));
    /* Taken fitted to this screen, played with a margin: the margin goes. */
    player(&s);
    s.screen_margin = ORACLES_SCREEN_MARGIN_AUTO;
    const OraclesStatePlay fitted = { ORACLES_CORE_SAMEBOY, 1, 1, 0, 0, 426u, 320u };
    CHECK(oracles_state_display_settings(&fitted, &s, fake_play, &square, notice, sizeof notice));
    CHECK(s.screen_margin == ORACLES_SCREEN_MARGIN_OFF && !strcmp(notice, "Display set to Screen margin Off to load this savestate"));
    /* Fitted to another screen: nothing loads it, and nothing changes. */
    player(&s);
    before = s;
    const OraclesStatePlay elsewhere = { ORACLES_CORE_MGBA, 1, 1, 0, 0, 412u, 360u };
    CHECK(oracles_state_display_settings(&elsewhere, &s, fake_play, &square, notice, sizeof notice) == -1 && !notice[0]);
    CHECK(!memcmp(&s, &before, sizeof s));
    /* Past four changes, how many. */
    player(&s);
    s.profile = ORACLES_PROFILE_FAITHFUL;
    s.view = ORACLES_ENHANCED_NEAR;
    s.screen_margin = ORACLES_SCREEN_MARGIN_AUTO;
    const OraclesStatePlay many = { ORACLES_CORE_MGBA, 1, 1, 1, 1, 426u, 320u };
    CHECK(oracles_state_display_settings(&many, &s, fake_play, &square, notice, sizeof notice) == 6);
    CHECK(!strcmp(notice, "Display changed in 6 rows to load this savestate"));
    /* The choices as they were, for the game started again: read and written back whole. */
    OraclesStateDisplay kept;
    player(&before);
    oracles_state_display_read(&before, &kept);
    oracles_state_display_write(&s, &kept);
    CHECK(s.profile == before.profile && s.core == before.core && s.transitions == before.transitions && s.compact_text == before.compact_text
          && s.text_box_wide == before.text_box_wide && s.view == before.view && s.far_fixed == before.far_fixed && s.screen_margin == before.screen_margin);
}

/* A game started again for its savestate that does not load it: Display's choices put back and stored, the quality
 * suggested forgotten as View and Core change back. */
static void display_restored(const char *path)
{
    static oracles_settings s, read;
    player(&s);
    snprintf(s.path, sizeof s.path, "%s", path);
    OraclesStateDisplay previous;
    oracles_state_display_read(&s, &previous);
    s.core = ORACLES_CORE_MGBA;
    s.view = ORACLES_ENHANCED_NEAR;
    s.transitions = 0;
    s.quality_hint = 2;
    oracles_state_display_restore(&s, &previous);
    CHECK(s.core == ORACLES_CORE_SAMEBOY && s.view == ORACLES_ENHANCED_FAR && s.transitions && s.quality_hint == -1);
    oracles_settings_defaults(&read);
    snprintf(read.path, sizeof read.path, "%s", path);
    oracles_settings_load(&read);
    CHECK(read.core == ORACLES_CORE_SAMEBOY && read.view == ORACLES_ENHANCED_FAR && read.transitions);
    remove(path);
}

/* Display's part of a game the home screen starts, which a savestate's game started again is predicted with: the
 * settings, the command line's options winning, Enhanced only where the ROM plays it. */
static void display_options(void)
{
    static oracles_settings s;
    player(&s);
    s.aspect = ORACLES_ASPECT_16_9;   /* named: no screen asked */
    s.transitions = 0;
    s.compact_text = 1;
    OraclesSessionOptions o;
    OraclesEnhancedSize surface;
    oracles_session_defaults(&o);
    CHECK(oracles_session_display_options(&o, &s, 1, NULL, &surface) == ORACLES_PROFILE_ENHANCED);
    CHECK(o.enhanced && o.view == ORACLES_ENHANCED_FAR + 1 && !o.continuous_transitions && !o.continuous_swim && o.compact_text == 2 && o.text_box_wide == 1);
    CHECK(surface.width == 480u && surface.height == 270u);
    /* An unrecognized ROM: Faithful, nothing of Enhanced. */
    oracles_session_defaults(&o);
    CHECK(oracles_session_display_options(&o, &s, 0, NULL, &surface) == ORACLES_PROFILE_FAITHFUL);
    CHECK(!o.enhanced && !o.view && o.compact_text == 1 && surface.width == 160u && surface.height == 144u);
    /* --view near and --continuous-swim: the view and the swim the command line's, whatever the settings say. */
    oracles_session_defaults(&o);
    o.view = ORACLES_ENHANCED_NEAR + 1;
    o.continuous_swim = 1;
    CHECK(oracles_session_display_options(&o, &s, 1, NULL, &surface) == ORACLES_PROFILE_ENHANCED);
    CHECK(o.view == ORACLES_ENHANCED_NEAR + 1 && o.continuous_swim && !o.continuous_transitions && surface.width == 256u && surface.height == 144u);
    /* Faithful chosen, the command line's view asks for Enhanced. */
    s.profile = ORACLES_PROFILE_FAITHFUL;
    oracles_session_defaults(&o);
    CHECK(oracles_session_display_options(&o, &s, 1, NULL, &surface) == ORACLES_PROFILE_FAITHFUL && !o.enhanced);
    oracles_session_defaults(&o);
    o.view = ORACLES_ENHANCED_FAR + 1;
    CHECK(oracles_session_display_options(&o, &s, 1, NULL, &surface) == ORACLES_PROFILE_ENHANCED && o.enhanced && surface.width == 480u);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: oracles-test-launcher-settings TEMPORARY_FILE\n"); return 2; }
    const char *path = argv[1];

    oracles_settings_defaults(&written);
    CHECK(written.rom[ORACLES_SETTINGS_AGES][0] == 0 && written.rom[ORACLES_SETTINGS_SEASONS][0] == 0);
    /* A first opening: Enhanced, the view drawn back, fullscreen, colour correction on; the transitions come with
     * Enhanced, vsync follows the display, the item hotkeys are off. */
    CHECK(written.profile == ORACLES_PROFILE_ENHANCED && written.window_scale == 0 && written.colour_correction == 1);
    CHECK(written.launcher_width == 1280 && written.launcher_height == 720);
    CHECK(written.transitions && written.item_hotkeys[0] == ORACLES_HOTKEYS_OFF && written.item_hotkeys[1] == ORACLES_HOTKEYS_OFF);
    CHECK(!written.swim_hold);   /* the mermaid suit as the game plays it */
    CHECK(!written.compact_text);   /* the game's fixed width */
    CHECK(!written.text_box_wide);   /* the game's box */
    CHECK(written.text_size == 0);   /* the game's size */
    CHECK(!written.text_instant);   /* the game's pace */
    CHECK(!written.cheats[ORACLES_SETTINGS_AGES] && !written.cheats[ORACLES_SETTINGS_SEASONS]);   /* the games as they play */
    CHECK(!strcmp(written.vsync, "auto"));
    CHECK(!strcmp(written.menu_pad, "leftstick"));   /* the left stick's click opens the pause menu */
    CHECK(written.menu_tip == 0 && written.hotkeys_tip == 0);   /* the reminder that names it and the pause menu's line on the hotkeys, due */
    CHECK(written.update_check);   /* the home screen asks GitHub for the latest release */

    /* Paths as players have them: spaces, an equals sign, backslashes, and as long as the field holds. */
    snprintf(written.path, sizeof written.path, "%s", path);
    snprintf(written.rom[ORACLES_SETTINGS_AGES], sizeof written.rom[0], "C:\\Users\\sam\\Games\\Oracle of Ages (USA).gbc");
    memset(written.rom[ORACLES_SETTINGS_SEASONS], 'x', sizeof written.rom[1] - 1);
    memcpy(written.rom[ORACLES_SETTINGS_SEASONS], "/home/sam/roms/a=b ", 19);
    written.rom[ORACLES_SETTINGS_SEASONS][sizeof written.rom[1] - 1] = 0;
    snprintf(written.patch[ORACLES_HOME_FAN_MOONRISE], sizeof written.patch[0], "C:\\Users\\sam\\Games\\Moonrise Regalia 1.0.6.bps");
    snprintf(written.patch[ORACLES_HOME_FAN_TEMPLE], sizeof written.patch[0], "/home/sam/roms/Temple_of_Seasons_1.073.bps");
    snprintf(written.patch[ORACLES_HOME_FAN_KINOMI], sizeof written.patch[0], "/home/sam/roms/Gifts_of_Kinomi_1.1.2.bps");
    /* Each Oracle's European ROM and language: English, the US ROM's own, until one is chosen. */
    CHECK(written.eu_rom[ORACLES_SETTINGS_AGES][0] == 0 && written.eu_rom[ORACLES_SETTINGS_SEASONS][0] == 0);
    CHECK(written.language[ORACLES_SETTINGS_AGES] == 0 && written.language[ORACLES_SETTINGS_SEASONS] == 0);
    snprintf(written.eu_rom[ORACLES_SETTINGS_AGES], sizeof written.eu_rom[0], "C:\\Users\\sam\\Games\\Oracle of Ages (Europe) (En,Fr,De,Es,It).gbc");
    written.language[ORACLES_SETTINGS_AGES] = 2;   /* French */
    snprintf(written.eu_rom[ORACLES_SETTINGS_SEASONS], sizeof written.eu_rom[0], "/home/sam/roms/a=b Seasons (Europe).gbc");
    written.language[ORACLES_SETTINGS_SEASONS] = 5;   /* Spanish */
    /* Each fan game's language, its Oracle's European ROM being the one used: English until one is set. */
    CHECK(written.fan_language[ORACLES_HOME_FAN_MOONRISE] == 0 && written.fan_language[ORACLES_HOME_FAN_TEMPLE] == 0 && written.fan_language[ORACLES_HOME_FAN_KINOMI] == 0);
    written.fan_language[ORACLES_HOME_FAN_MOONRISE] = 3;   /* German */
    written.fan_language[ORACLES_HOME_FAN_TEMPLE] = 2;     /* French */
    written.fan_language[ORACLES_HOME_FAN_KINOMI] = 1;     /* English (EU) */
    written.profile = ORACLES_PROFILE_ENHANCED;
    written.launcher_width = 1600;
    written.launcher_height = 900;
    written.camera = 1;
    /* Controls' keys and buttons as it writes them: renamed, and one left without a key after another cell took it. */
    snprintf(written.key_names[0], sizeof written.key_names[0], "D");
    written.key_names[5][0] = 0;
    snprintf(written.pad_names[0], sizeof written.pad_names[0], "x");
    written.hotkey_pad_names[0][0] = 0;
    snprintf(written.menu_pad, sizeof written.menu_pad, "righttrigger");
    written.menu_tip = 3;
    written.hotkeys_tip = ORACLES_TIP_OFF;
    snprintf(written.hotkey_bind_b, sizeof written.hotkey_bind_b, "Right Shift");
    written.transitions = 0;
    written.swim_hold = 1;
    written.update_check = 0;
    written.compact_text = 1;
    written.text_box_wide = 1;
    written.text_size = 2;
    written.text_instant = 1;
    written.cheats[ORACLES_SETTINGS_SEASONS] = ORACLES_CHEAT_NO_DAMAGE | ORACLES_CHEAT_RUPEES;
    written.item_hotkeys[ORACLES_SETTINGS_AGES] = ORACLES_HOTKEYS_EQUIP;
    written.item_hotkeys[ORACLES_SETTINGS_SEASONS] = ORACLES_HOTKEYS_USE;
    written.window_scale = 0;   /* fullscreen */
    /* The Mods page's choices: kept in the order of their names, whatever the order they were made in. */
    CHECK(written.mods[ORACLES_SETTINGS_AGES][0] == 0 && written.mods[ORACLES_SETTINGS_SEASONS][0] == 0);
    CHECK(oracles_settings_mod_set(written.mods[ORACLES_SETTINGS_AGES], "fortune-teller", 1));
    CHECK(oracles_settings_mod_set(written.mods[ORACLES_SETTINGS_AGES], "claw-game", 1));
    CHECK(oracles_settings_mod_set(written.mods[ORACLES_SETTINGS_AGES], "claw-game", 1));   /* once */
    CHECK(!strcmp(written.mods[ORACLES_SETTINGS_AGES], "claw-game,fortune-teller"));
    CHECK(oracles_settings_mod_active(written.mods[ORACLES_SETTINGS_AGES], "claw-game") && !oracles_settings_mod_active(written.mods[ORACLES_SETTINGS_AGES], "claw"));
    CHECK(oracles_settings_mod_set(written.mods[ORACLES_SETTINGS_SEASONS], "claw-game", 1));
    oracles_settings_store(&written);

    oracles_settings_defaults(&read_back);
    snprintf(read_back.path, sizeof read_back.path, "%s", path);
    oracles_settings_load(&read_back);
    CHECK(!strcmp(read_back.rom[ORACLES_SETTINGS_AGES], written.rom[ORACLES_SETTINGS_AGES]));
    CHECK(!strcmp(read_back.rom[ORACLES_SETTINGS_SEASONS], written.rom[ORACLES_SETTINGS_SEASONS]));
    CHECK(!strcmp(read_back.patch[ORACLES_HOME_FAN_MOONRISE], written.patch[ORACLES_HOME_FAN_MOONRISE]));
    CHECK(!strcmp(read_back.patch[ORACLES_HOME_FAN_TEMPLE], written.patch[ORACLES_HOME_FAN_TEMPLE]));
    CHECK(!strcmp(read_back.patch[ORACLES_HOME_FAN_KINOMI], written.patch[ORACLES_HOME_FAN_KINOMI]));
    CHECK(!strcmp(read_back.eu_rom[ORACLES_SETTINGS_AGES], written.eu_rom[ORACLES_SETTINGS_AGES]));
    CHECK(!strcmp(read_back.eu_rom[ORACLES_SETTINGS_SEASONS], written.eu_rom[ORACLES_SETTINGS_SEASONS]));
    CHECK(read_back.language[ORACLES_SETTINGS_AGES] == 2 && read_back.language[ORACLES_SETTINGS_SEASONS] == 5);
    CHECK(read_back.fan_language[ORACLES_HOME_FAN_MOONRISE] == 3 && read_back.fan_language[ORACLES_HOME_FAN_TEMPLE] == 2 && read_back.fan_language[ORACLES_HOME_FAN_KINOMI] == 1);
    CHECK(read_back.profile == ORACLES_PROFILE_ENHANCED);
    CHECK(read_back.launcher_width == 1600 && read_back.launcher_height == 900);
    CHECK(read_back.camera == 1);   /* the keys around them still read */
    CHECK(!strcmp(read_back.key_names[0], "D") && read_back.key_names[5][0] == 0 && !strcmp(read_back.key_names[1], "Left"));
    CHECK(!strcmp(read_back.pad_names[0], "x") && read_back.hotkey_pad_names[0][0] == 0 && !strcmp(read_back.hotkey_pad_names[1], "y"));
    CHECK(!strcmp(read_back.menu_pad, "righttrigger") && read_back.menu_tip == 3 && read_back.hotkeys_tip == ORACLES_TIP_OFF);
    CHECK(!strcmp(read_back.hotkey_bind_b, "Right Shift") && !strcmp(read_back.hotkey_bind_a, "Left Ctrl"));
    CHECK(read_back.transitions == 0 && read_back.swim_hold == 1 && read_back.update_check == 0 && read_back.compact_text == 1 && read_back.text_box_wide == 1 && read_back.text_size == 2);
    CHECK(read_back.text_instant == 1);
    CHECK(read_back.cheats[ORACLES_SETTINGS_AGES] == 0 && read_back.cheats[ORACLES_SETTINGS_SEASONS] == (ORACLES_CHEAT_NO_DAMAGE | ORACLES_CHEAT_RUPEES));
    CHECK(read_back.item_hotkeys[ORACLES_SETTINGS_AGES] == ORACLES_HOTKEYS_EQUIP && read_back.item_hotkeys[ORACLES_SETTINGS_SEASONS] == ORACLES_HOTKEYS_USE);
    CHECK(read_back.window_scale == 0);
    CHECK(!strcmp(read_back.mods[ORACLES_SETTINGS_AGES], "claw-game,fortune-teller") && !strcmp(read_back.mods[ORACLES_SETTINGS_SEASONS], "claw-game"));
    char names[ORACLES_SETTINGS_MODS_MAX][ORACLES_SETTINGS_MOD_NAME];
    CHECK(oracles_settings_mod_names(read_back.mods[ORACLES_SETTINGS_AGES], names, ORACLES_SETTINGS_MODS_MAX) == 2
          && !strcmp(names[0], "claw-game") && !strcmp(names[1], "fortune-teller"));
    CHECK(oracles_settings_mod_set(read_back.mods[ORACLES_SETTINGS_AGES], "claw-game", 0) && !strcmp(read_back.mods[ORACLES_SETTINGS_AGES], "fortune-teller"));
    /* Eight at most: a ninth is refused and the list stays. */
    for (int i = 0; i < 7; i++) {
        char name[8];
        snprintf(name, sizeof name, "m%d", i);
        CHECK(oracles_settings_mod_set(read_back.mods[ORACLES_SETTINGS_AGES], name, 1));
    }
    CHECK(!oracles_settings_mod_set(read_back.mods[ORACLES_SETTINGS_AGES], "zz", 1));
    CHECK(!strcmp(read_back.mods[ORACLES_SETTINGS_AGES], "fortune-teller,m0,m1,m2,m3,m4,m5,m6"));
    CHECK(oracles_settings_mod_set(read_back.mods[ORACLES_SETTINGS_AGES], "m3", 0) && oracles_settings_mod_set(read_back.mods[ORACLES_SETTINGS_AGES], "zz", 1));
    CHECK(!strcmp(read_back.mods[ORACLES_SETTINGS_AGES], "fortune-teller,m0,m1,m2,m4,m5,m6,zz"));

    /* The names of the file. */
    FILE *f = fopen(path, "r");
    char line[8192];
    int seen = 0;
    while (f && fgets(line, sizeof line, f)) {
        if (!strcmp(line, "profile=enhanced\n")) seen |= 1;
        if (!strncmp(line, "profile_", 8) || !strncmp(line, "transitions_", 12)) seen |= 128;   /* no per-game keys */
        if (!strcmp(line, "camera=1\n")) seen |= 2;
        if (!strcmp(line, "launcher_window=1600x900\n")) seen |= 4;
        if (!strncmp(line, "rom_ages=C:\\Users\\sam\\Games\\", 28)) seen |= 8;
        if (!strcmp(line, "transitions=off\n")) seen |= 16;
        if (!strcmp(line, "item_hotkeys_ages=equip\n")) seen |= 32;
        if (!strcmp(line, "window_scale=full\n")) seen |= 64;
        if (!strcmp(line, "patch_moonrise=C:\\Users\\sam\\Games\\Moonrise Regalia 1.0.6.bps\n")) seen |= 256;
        if (!strcmp(line, "patch_temple=/home/sam/roms/Temple_of_Seasons_1.073.bps\n")) seen |= 512;
        if (!strcmp(line, "patch_kinomi=/home/sam/roms/Gifts_of_Kinomi_1.1.2.bps\n")) seen |= 1024;
        if (!strcmp(line, "eu_rom_seasons=/home/sam/roms/a=b Seasons (Europe).gbc\n")) seen |= 2048;
        if (!strcmp(line, "language_ages=fr\n")) seen |= 4096;
        if (!strcmp(line, "language_seasons=es\n")) seen |= 8192;
        if (!strcmp(line, "language_moonrise=de\n")) seen |= 16384;
        if (!strcmp(line, "language_temple=fr\n")) seen |= 32768;
        if (!strcmp(line, "language_kinomi=en-eu\n")) seen |= 65536;
        if (!strcmp(line, "swim_hold=on\n")) seen |= 131072;
        if (!strcmp(line, "menu_pad=righttrigger\n")) seen |= 262144;
        if (!strcmp(line, "menu_tip=3\n")) seen |= 524288;
        if (!strcmp(line, "hotkeys_tip=off\n")) seen |= 33554432;
        if (!strncmp(line, "menu_toast=", 11)) seen |= 128;   /* 1.3's, no longer written */
        if (!strcmp(line, "cheats_ages=\n")) seen |= 1048576;
        if (!strcmp(line, "cheats_seasons=no-damage,rupees\n")) seen |= 2097152;
        if (!strcmp(line, "update_check=off\n")) seen |= 4194304;
        if (!strcmp(line, "text=compact\n")) seen |= 8388608;
        if (!strcmp(line, "text_speed=instant\n")) seen |= 16777216;
    }
    if (f) fclose(f);
    CHECK(seen == (127 | 256 | 512 | 1024 | 2048 | 4096 | 8192 | 16384 | 32768 | 65536 | 131072 | 262144 | 524288 | 1048576 | 2097152 | 4194304 | 8388608
                   | 16777216 | 33554432));   /* and not 128: no per-game profile or transitions, nor 1.3's menu_toast= */

    /* A file of an earlier version, without the keys: the defaults; the keys it has hold, colour correction off. */
    write_text(path, "colour_correction=0\nvsync=off\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.colour_correction == 0 && !strcmp(read_back.vsync, "off") && read_back.rom[0][0] == 0 && read_back.patch[ORACLES_HOME_FAN_MOONRISE][0] == 0
          && read_back.patch[ORACLES_HOME_FAN_TEMPLE][0] == 0 && read_back.patch[ORACLES_HOME_FAN_KINOMI][0] == 0 && read_back.profile == ORACLES_PROFILE_ENHANCED);
    CHECK(read_back.eu_rom[ORACLES_SETTINGS_AGES][0] == 0 && read_back.language[ORACLES_SETTINGS_AGES] == 0 && read_back.language[ORACLES_SETTINGS_SEASONS] == 0);
    CHECK(read_back.fan_language[ORACLES_HOME_FAN_MOONRISE] == 0 && read_back.fan_language[ORACLES_HOME_FAN_TEMPLE] == 0);
    CHECK(read_back.transitions == 1 && read_back.window_scale == 0 && read_back.swim_hold == 0 && read_back.compact_text == 0 && read_back.text_instant == 0);
    CHECK(read_back.cheats[ORACLES_SETTINGS_AGES] == 0 && read_back.cheats[ORACLES_SETTINGS_SEASONS] == 0);
    CHECK(read_back.launcher_width == 1280 && read_back.launcher_height == 720);
    CHECK(!strcmp(read_back.menu_pad, "leftstick") && read_back.menu_tip == 0 && read_back.hotkeys_tip == 0 && read_back.update_check == 1);
    /* 1.3's toast, shown once with a controller: dropped, and not kept as a key unknown; the reminder due again. */
    write_text(path, "menu_pad=leftstick\nmenu_toast=shown\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.menu_tip == 0 && !strstr(read_back.unknown, "menu_toast"));
    /* The counts: on is 0, a count as far as its most is off, a word the launcher does not know leaves the default. */
    write_text(path, "menu_tip=on\nhotkeys_tip=2\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.menu_tip == 0 && read_back.hotkeys_tip == 2);
    write_text(path, "menu_tip=10\nhotkeys_tip=3\n");
    oracles_settings_load(&read_back);
    CHECK(read_back.menu_tip == ORACLES_TIP_OFF && read_back.hotkeys_tip == ORACLES_TIP_OFF);
    write_text(path, "menu_tip=9\nhotkeys_tip=often\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.menu_tip == 9 && read_back.hotkeys_tip == 0);
    /* An empty menu_pad= is none: Guide alone. */
    write_text(path, "menu_pad=\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.menu_pad[0] == 0);

    /* The mods folder beside settings.txt, whichever separator its path uses, or where mods_path puts it (Android's
     * shared storage), the language packs staying beside settings.txt; none without a settings directory. */
    oracles_settings mods_at;
    char folder[ORACLES_SETTINGS_PATH_LENGTH];
    oracles_settings_defaults(&mods_at);
    snprintf(mods_at.path, sizeof mods_at.path, "/home/sam/.local/share/the-oracles-project/settings.txt");
    oracles_settings_mods_folder(&mods_at, folder, sizeof folder);
    CHECK(!strcmp(folder, "/home/sam/.local/share/the-oracles-project/mods"));
    snprintf(mods_at.path, sizeof mods_at.path, "C:\\Users\\sam\\AppData\\Roaming\\the-oracles-project\\settings.txt");
    oracles_settings_mods_folder(&mods_at, folder, sizeof folder);
    CHECK(!strcmp(folder, "C:\\Users\\sam\\AppData\\Roaming\\the-oracles-project\\mods"));
    oracles_settings_language_pack(&mods_at, ORACLES_SETTINGS_SEASONS, folder, sizeof folder);
    CHECK(!strcmp(folder, "C:\\Users\\sam\\AppData\\Roaming\\the-oracles-project\\language-seasons.pack"));
    oracles_settings_fan_language_pack(&mods_at, ORACLES_HOME_FAN_KINOMI, folder, sizeof folder);
    CHECK(!strcmp(folder, "C:\\Users\\sam\\AppData\\Roaming\\the-oracles-project\\language-kinomi.pack"));
    snprintf(mods_at.mods_path, sizeof mods_at.mods_path, "/storage/emulated/0/Android/data/org.theoraclesproject/files/mods");
    oracles_settings_mods_folder(&mods_at, folder, sizeof folder);
    CHECK(!strcmp(folder, "/storage/emulated/0/Android/data/org.theoraclesproject/files/mods"));
    oracles_settings_language_pack(&mods_at, ORACLES_SETTINGS_SEASONS, folder, sizeof folder);
    CHECK(!strcmp(folder, "C:\\Users\\sam\\AppData\\Roaming\\the-oracles-project\\language-seasons.pack"));
    mods_at.mods_path[0] = 0;
    mods_at.path[0] = 0;
    oracles_settings_mods_folder(&mods_at, folder, sizeof folder);
    CHECK(folder[0] == 0);
    oracles_settings_language_pack(&mods_at, ORACLES_SETTINGS_AGES, folder, sizeof folder);
    CHECK(folder[0] == 0);

    /* Mods' Play gives a session the mods --mods would, in the same order, and the same save of its own; a mod no
     * longer in the folder is left out.  Start game, without them, keeps the usual save. */
    {
        char dir[ORACLES_SETTINGS_PATH_LENGTH + 32], dirs[ORACLES_SETTINGS_MODS_MAX][ORACLES_SETTINGS_PATH_LENGTH], home_folder[ORACLES_SETTINGS_PATH_LENGTH];
        oracles_settings home;
        oracles_settings_defaults(&home);
        snprintf(home.path, sizeof home.path, "%s", path);   /* the mods folder beside the test's settings file */
        oracles_settings_mods_folder(&home, home_folder, sizeof home_folder);
        mkdir(home_folder, 0700);
        const char *const names[3] = { "claw-game", "fortune-teller", "gone" };
        for (int i = 0; i < 2; i++) { snprintf(dir, sizeof dir, "%s/%s", home_folder, names[i]); mkdir(dir, 0700); }
        for (int i = 2; i >= 0; i--) CHECK(oracles_settings_mod_set(home.mods[ORACLES_SETTINGS_AGES], names[i], 1));
        OraclesSessionOptions from_page, from_command_line;
        oracles_session_defaults(&from_page);
        oracles_session_defaults(&from_command_line);
        from_page.rom_path = from_command_line.rom_path = "/home/sam/roms/Oracle of Ages (USA).gbc";
        char claw[ORACLES_SETTINGS_PATH_LENGTH + 32], fortune[ORACLES_SETTINGS_PATH_LENGTH + 32];
        snprintf(claw, sizeof claw, "%s/claw-game", home_folder);
        snprintf(fortune, sizeof fortune, "%s/fortune-teller", home_folder);
        from_command_line.mods_dirs[from_command_line.mods_count++] = claw;       /* --mods .../claw-game */
        from_command_line.mods_dirs[from_command_line.mods_count++] = fortune;    /* --mods .../fortune-teller */
        CHECK(oracles_session_home_mods(&from_page, &home, ORACLES_SETTINGS_AGES, dirs) == 2 && from_page.mods_count == from_command_line.mods_count);
        for (unsigned i = 0; i < from_page.mods_count && i < from_command_line.mods_count; i++) CHECK(!strcmp(from_page.mods_dirs[i], from_command_line.mods_dirs[i]));
        char save_page[ORACLES_SETTINGS_PATH_LENGTH], save_command_line[ORACLES_SETTINGS_PATH_LENGTH];
        oracles_session_save_path(&from_page, save_page, sizeof save_page);
        oracles_session_save_path(&from_command_line, save_command_line, sizeof save_command_line);
        CHECK(!strcmp(save_page, save_command_line) && !strcmp(save_page, "/home/sam/roms/Oracle of Ages (USA).mods.sav"));
        /* Seasons has none active; Start game plays without mods, on the usual save; --save is kept as given. */
        CHECK(oracles_session_home_mods(&from_page, &home, ORACLES_SETTINGS_SEASONS, dirs) == 0);
        oracles_session_save_path(&from_page, save_page, sizeof save_page);
        CHECK(!strcmp(save_page, "/home/sam/roms/Oracle of Ages (USA).sav"));
        from_command_line.save_path = "/tmp/other.sav";
        oracles_session_save_path(&from_command_line, save_command_line, sizeof save_command_line);
        CHECK(!strcmp(save_command_line, "/tmp/other.sav"));
        /* A fan game's save is beside its patch. */
        from_page.patch_path = "/home/sam/roms/Moonrise Regalia 1.0.6.bps";
        oracles_session_save_path(&from_page, save_page, sizeof save_page);
        CHECK(!strcmp(save_page, "/home/sam/roms/Moonrise Regalia 1.0.6.sav"));
        for (int i = 0; i < 2; i++) { snprintf(dir, sizeof dir, "%s/%s", home_folder, names[i]); rmdir(dir); }
        rmdir(home_folder);
    }

    /* A mods line written by hand: its names sorted, once each, those that name no folder a mod can have dropped. */
    write_text(path, "mods_ages=zeta,Bad Name,alpha,,zeta,../up,beta\nmods_seasons=\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(!strcmp(read_back.mods[ORACLES_SETTINGS_AGES], "alpha,beta,zeta") && read_back.mods[ORACLES_SETTINGS_SEASONS][0] == 0);

    /* Values it does not know leave the default; an empty ROM clears it. */
    write_text(path, "profile=widescreen\nlauncher_window=big\nrom_seasons=\ntransitions=yes\nswim_hold=1\nupdate_check=0\ntext=narrow\nitem_hotkeys_seasons=always\nwindow_scale=5\n"
                     "eu_rom_ages=\nlanguage_ages=ja\nlanguage_seasons=de\nlanguage_temple=xx\nlanguage_kinomi=it\nlanguage_moonrise=en-us\n"
                     "cheats_ages=ammo,speed-150\ncheats_seasons=rupees,ammo\ntext_speed=fast\n");
    oracles_settings_defaults(&read_back);
    snprintf(read_back.rom[1], sizeof read_back.rom[1], "old.gbc");
    snprintf(read_back.eu_rom[0], sizeof read_back.eu_rom[0], "old.gbc");
    oracles_settings_load(&read_back);
    CHECK(read_back.eu_rom[ORACLES_SETTINGS_AGES][0] == 0 && read_back.language[ORACLES_SETTINGS_AGES] == 0 && read_back.language[ORACLES_SETTINGS_SEASONS] == 3);
    CHECK(read_back.fan_language[ORACLES_HOME_FAN_TEMPLE] == 0 && read_back.fan_language[ORACLES_HOME_FAN_KINOMI] == 4 && read_back.fan_language[ORACLES_HOME_FAN_MOONRISE] == 0);
    CHECK(read_back.profile == ORACLES_PROFILE_ENHANCED);
    CHECK(read_back.launcher_width == 1280 && read_back.launcher_height == 720);
    CHECK(read_back.rom[ORACLES_SETTINGS_SEASONS][0] == 0);
    CHECK(read_back.transitions == 1 && read_back.swim_hold == 0 && read_back.update_check == 1 && read_back.compact_text == 0 && read_back.item_hotkeys[ORACLES_SETTINGS_SEASONS] == ORACLES_HOTKEYS_OFF);
    CHECK(read_back.cheats[ORACLES_SETTINGS_AGES] == ORACLES_CHEAT_AMMO && read_back.cheats[ORACLES_SETTINGS_SEASONS] == (ORACLES_CHEAT_AMMO | ORACLES_CHEAT_RUPEES));   /* a cheat unknown: dropped */
    CHECK(read_back.window_scale == 0);
    CHECK(read_back.text_instant == 0);   /* a later version's speed: the default */
    /* Display's View: far at the first opening, a level written and read by its name, an unknown name the default. */
    oracles_settings_defaults(&read_back);
    CHECK(read_back.view == 2);
    write_text(path, "view=near\n");
    oracles_settings_load(&read_back);
    CHECK(read_back.view == 0);
    read_back.view = 1;
    oracles_settings_store(&read_back);
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.view == 1);
    write_text(path, "view=wide\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.view == 2);
    /* The view's shape: the screen's at the first opening, a shape written and read by its name, an unknown one auto. */
    oracles_settings_defaults(&read_back);
    CHECK(read_back.aspect == ORACLES_ASPECT_AUTO);
    write_text(path, "aspect=4:3\n");
    oracles_settings_load(&read_back);
    CHECK(read_back.aspect == ORACLES_ASPECT_4_3);
    read_back.aspect = ORACLES_ASPECT_16_9;
    oracles_settings_store(&read_back);
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.aspect == ORACLES_ASPECT_16_9);
    write_text(path, "aspect=21:9\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.aspect == ORACLES_ASPECT_AUTO);
    /* The far view: fitted to the screen at the first opening, fixed written and read back, an unknown value screen. */
    oracles_settings_defaults(&read_back);
    CHECK(read_back.far_fixed == 0);
    write_text(path, "far=fixed\n");
    oracles_settings_load(&read_back);
    CHECK(read_back.far_fixed == 1);
    oracles_settings_store(&read_back);
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.far_fixed == 1);
    write_text(path, "far=wide\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.far_fixed == 0);
    /* The rooms never entered: shown at the first opening, hidden written and read back, an unknown value shown. */
    oracles_settings_defaults(&read_back);
    CHECK(read_back.unvisited_hidden == 0);
    write_text(path, "unvisited_rooms=hidden\n");
    oracles_settings_load(&read_back);
    CHECK(read_back.unvisited_hidden == 1);
    oracles_settings_store(&read_back);
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.unvisited_hidden == 1);
    write_text(path, "unvisited_rooms = SHOWN\n");
    oracles_settings_load(&read_back);
    CHECK(read_back.unvisited_hidden == 0);
    write_text(path, "unvisited_rooms=dark\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.unvisited_hidden == 0);
    /* Fitted only where the view takes the screen's shape and far= is screen. */
    {
        OraclesSessionOptions options;
        oracles_session_defaults(&options);
        unsigned w = 1, h = 1, lines = 1;
        read_back.far_fixed = 1;
        oracles_session_far_screen(&options, &read_back, NULL, &w, &h, &lines);
        CHECK(w == 0 && h == 0 && lines == 0);
        read_back.far_fixed = 0;
        options.aspect = 1 + ORACLES_ASPECT_4_3;
        w = h = lines = 1;
        oracles_session_far_screen(&options, &read_back, NULL, &w, &h, &lines);
        CHECK(w == 0 && h == 0 && lines == 0);
        options.aspect = 0;
        options.screen_w = 1280; options.screen_h = 960;   /* its shape the view's, whatever screen_4_3 says */
        const OraclesEnhancedSize far = oracles_session_view_size(&options, ORACLES_ENHANCED_FAR), medium = oracles_session_view_size(&options, ORACLES_ENHANCED_MEDIUM);
        CHECK(far.width == 426 && far.height == 320 && medium.width == 320 && medium.height == 240);
        options.screen_margin_lines = 120;   /* the band a margin leaves: 1280x840, where far keeps its size */
        CHECK(oracles_session_view_size(&options, ORACLES_ENHANCED_FAR).width == 480 && oracles_session_view_size(&options, ORACLES_ENHANCED_FAR).height == 360);
        CHECK(oracles_session_view_size(&options, ORACLES_ENHANCED_MEDIUM).width == 320);
        options.screen_margin_lines = 0;
        options.screen_w = 1920; options.screen_h = 1080;
        CHECK(oracles_session_view_size(&options, ORACLES_ENHANCED_FAR).height == 270);
        options.screen_w = options.screen_h = 0;   /* far=fixed or a shape named: the options' shape */
        options.screen_4_3 = 1;
        CHECK(oracles_session_view_size(&options, ORACLES_ENHANCED_FAR).width == 480 && oracles_session_view_size(&options, ORACLES_ENHANCED_FAR).height == 360);
    }
    /* The ghosts: auto at the first opening, a count written and read back, an unknown value auto. */
    oracles_settings_defaults(&read_back);
    CHECK(read_back.ghosts == 0);
    write_text(path, "ghosts=1\n");
    oracles_settings_load(&read_back);
    CHECK(read_back.ghosts == 1);
    read_back.ghosts = 2;
    oracles_settings_store(&read_back);
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.ghosts == 2);
    read_back.ghosts = 0;
    oracles_settings_store(&read_back);
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.ghosts == 0);
    write_text(path, "ghosts=3\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.ghosts == 0);
    /* The scaling: sharp at the first opening, written under its comment; fill read and written back; an unknown value
     * sharp. */
    oracles_settings_defaults(&read_back);
    CHECK(read_back.scaling_fill == 0);
    oracles_settings_store(&read_back);
    {
        FILE *f = fopen(path, "r");
        char line[512], previous[512] = "";
        int found = 0;
        while (f && fgets(line, sizeof line, f)) {
            if (!strcmp(line, "scaling=sharp\n")) found = !strncmp(previous, "# proportions, sampled for pixel art", 36);
            snprintf(previous, sizeof previous, "%s", line);
        }
        if (f) fclose(f);
        CHECK(found);
    }
    write_text(path, "scaling=fill\n");
    oracles_settings_load(&read_back);
    CHECK(read_back.scaling_fill == 1);
    oracles_settings_store(&read_back);
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.scaling_fill == 1);
    write_text(path, "scaling=smooth\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.scaling_fill == 0);
    /* The screen margin: off at the first opening, written under its comment; each value written and read back by its
     * name; an unknown value off, even over another read before; --screen-margin wins for the run, never written. */
    oracles_settings_defaults(&read_back);
    CHECK(read_back.screen_margin == ORACLES_SCREEN_MARGIN_OFF);
    oracles_settings_store(&read_back);
    {
        FILE *f = fopen(path, "r");
        char line[512], previous[512] = "";
        int found = 0;
        while (f && fgets(line, sizeof line, f)) {
            if (!strcmp(line, "screen_margin=off\n")) found = !strncmp(previous, "# for a screen whose edges hide part of the picture", 51);
            snprintf(previous, sizeof previous, "%s", line);
        }
        if (f) fclose(f);
        CHECK(found);
    }
    for (int m = 0; m < ORACLES_SCREEN_MARGINS; m++) {
        char text[64];
        snprintf(text, sizeof text, "screen_margin=%s\n", oracles_screen_margin_names[m]);
        write_text(path, text);
        oracles_settings_defaults(&read_back);
        oracles_settings_load(&read_back);
        CHECK(read_back.screen_margin == m);
        read_back.screen_margin = (m + 1) % ORACLES_SCREEN_MARGINS;
        oracles_settings_store(&read_back);
        oracles_settings_defaults(&read_back);
        oracles_settings_load(&read_back);
        CHECK(read_back.screen_margin == (m + 1) % ORACLES_SCREEN_MARGINS);
    }
    CHECK(!strcmp(oracles_screen_margin_names[ORACLES_SCREEN_MARGIN_AUTO], "auto") && !strcmp(oracles_screen_margin_names[ORACLES_SCREEN_MARGIN_SMALL], "small")
          && !strcmp(oracles_screen_margin_names[ORACLES_SCREEN_MARGIN_LARGE], "large"));
    write_text(path, "screen_margin=large\nscreen_margin=huge\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.screen_margin == ORACLES_SCREEN_MARGIN_OFF);
    read_back.screen_margin = ORACLES_SCREEN_MARGIN_SMALL;
    CHECK(oracles_session_screen_margin(0, &read_back) == ORACLES_SCREEN_MARGIN_SMALL);
    CHECK(oracles_session_screen_margin(ORACLES_SCREEN_MARGIN_LARGE + 1, &read_back) == ORACLES_SCREEN_MARGIN_LARGE);
    CHECK(oracles_session_screen_margin(ORACLES_SCREEN_MARGIN_OFF + 1, &read_back) == ORACLES_SCREEN_MARGIN_OFF);
    /* The game's menus: framed at the view's scale on a desktop (the tests' build), large written and read back. */
    oracles_settings_defaults(&read_back);
    CHECK(read_back.menus_large == 0);
    write_text(path, "menus=large\n");
    oracles_settings_load(&read_back);
    CHECK(read_back.menus_large == 1);
    oracles_settings_store(&read_back);
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.menus_large == 1);
    write_text(path, "menus=huge\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.menus_large == 0);
    /* The status bar: full width at the first opening, written; original and the wide layout written and read back; an
     * unknown value leaves the default. */
    oracles_settings_defaults(&read_back);
    CHECK(read_back.hud == 1);
    oracles_settings_store(&read_back);
    CHECK(file_has_line(path, "hud=full\n"));
    write_text(path, "hud=original\n");
    oracles_settings_load(&read_back);
    CHECK(read_back.hud == 0);
    oracles_settings_store(&read_back);
    CHECK(file_has_line(path, "hud=original\n"));
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.hud == 0);
    write_text(path, "hud=wide\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.hud == 2);
    oracles_settings_store(&read_back);
    CHECK(file_has_line(path, "hud=wide\n"));
    write_text(path, "hud=split\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.hud == 1);
    /* The core: none chosen at the first opening, the build's default playing (SameBoy in the tests' build) and no core=
     * line written, so that another build's default plays its own; a core chosen is written and read by its name; an
     * unknown name leaves none chosen. */
    oracles_settings_defaults(&read_back);
    CHECK(read_back.core == -1 && oracles_settings_core(&read_back) == ORACLES_CORE_SAMEBOY);
    oracles_settings_store(&read_back);
    {
        FILE *f = fopen(path, "r");
        char line[512];
        int core_line = 0;
        while (f && fgets(line, sizeof line, f)) if (!strncmp(line, "core=", 5)) core_line = 1;
        if (f) fclose(f);
        CHECK(!core_line);
    }
    write_text(path, "core=mgba\n");
    oracles_settings_load(&read_back);
    CHECK(read_back.core == ORACLES_CORE_MGBA && oracles_settings_core(&read_back) == ORACLES_CORE_MGBA);
    read_back.core = ORACLES_CORE_SAMEBOY;
    oracles_settings_store(&read_back);
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.core == ORACLES_CORE_SAMEBOY);
    write_text(path, "core=gambatte\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.core == -1 && oracles_settings_core(&read_back) == ORACLES_CORE_SAMEBOY);
    /* zoom-out is not a profile, and the per-game keys are not the launcher's: the defaults stay, the profile is written
     * back as it plays, and the per-game keys, which a later version may have, are kept as they were, unread. */
    write_text(path, "profile=zoom-out\nprofile_ages=enhanced\ntransitions_ages=off\nprofile_seasons=enhanced\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.profile == ORACLES_PROFILE_ENHANCED && read_back.transitions == 1);
    oracles_settings_store(&read_back);
    f = fopen(path, "r");
    seen = 0;
    while (f && fgets(line, sizeof line, f)) {
        if (!strcmp(line, "profile=enhanced\n")) seen |= 1;
        if (!strcmp(line, "transitions=on\n")) seen |= 2;
        if (strstr(line, "zoom-out")) seen |= 4;
        if (!strcmp(line, "profile_ages=enhanced\n") || !strcmp(line, "transitions_ages=off\n") || !strcmp(line, "profile_seasons=enhanced\n")) seen += 8;
    }
    if (f) fclose(f);
    CHECK(seen == 3 + 3 * 8);

    /* The file is written through a temporary one: none is left, and a write that cannot start leaves the old file. */
    char temporary[1100];
    snprintf(temporary, sizeof temporary, "%s.tmp", path);
    f = fopen(temporary, "r");
    CHECK(f == NULL);
    if (f) fclose(f);
    write_text(path, "profile=faithful\n");   /* not the default: the file kept is told from a new one */
    CHECK(mkdir(temporary, 0700) == 0);   /* a directory where the temporary file would go */
    oracles_settings_defaults(&written);
    snprintf(written.path, sizeof written.path, "%s", path);
    CHECK(oracles_settings_store(&written) == 0);
    oracles_settings_defaults(&read_back);
    snprintf(read_back.path, sizeof read_back.path, "%s", path);
    oracles_settings_load(&read_back);
    CHECK(read_back.profile == ORACLES_PROFILE_FAITHFUL);
    rmdir(temporary);
    CHECK(oracles_settings_store(&written) == 1);

    /* Quality: the profile the view, the core and the neighbour workers make, Custom when none. */
    oracles_settings_defaults(&written);
    for (int q = ORACLES_QUALITY_LOW; q < ORACLES_QUALITY_CUSTOM; q++) {
        oracles_settings_apply_quality(&written, (OraclesQuality)q);
        CHECK(oracles_settings_quality(&written) == (OraclesQuality)q);
    }
    CHECK(written.view == 2 && oracles_settings_core(&written) == ORACLES_CORE_SAMEBOY && written.ghosts == 2);   /* Max */
    CHECK(written.core == ORACLES_CORE_SAMEBOY);   /* chosen once (Low's Fast): written from then on */
    oracles_settings_defaults(&written);
    oracles_settings_apply_quality(&written, ORACLES_QUALITY_MAX);
    CHECK(written.core == -1);   /* none chosen, and the tests' build plays SameBoy by default: Max does not write it */
    oracles_settings_apply_quality(&written, ORACLES_QUALITY_MEDIUM);
    CHECK(written.view == 1 && written.core == ORACLES_CORE_MGBA && written.ghosts == 0);
    written.view = 2;   /* View changed by hand: Far, Fast, auto is High */
    CHECK(oracles_settings_quality(&written) == ORACLES_QUALITY_HIGH);
    written.ghosts = 1;   /* Neighbour workers changed by hand: no profile */
    CHECK(oracles_settings_quality(&written) == ORACLES_QUALITY_CUSTOM);
    oracles_settings_apply_quality(&written, ORACLES_QUALITY_CUSTOM);   /* Custom changes nothing */
    CHECK(written.ghosts == 1 && written.view == 2);
    oracles_settings_defaults(&written);   /* a player of before: far, the default core, auto: Custom on a desktop build */
    CHECK(oracles_settings_quality(&written) == ORACLES_QUALITY_CUSTOM);
    /* A session's own: Low (near, Fast, auto) leaves a neighbour's objects frozen; near on the other core is no profile. */
    CHECK(oracles_settings_quality_of(0, ORACLES_CORE_MGBA, 0) == ORACLES_QUALITY_LOW);
    CHECK(oracles_settings_quality_of(0, ORACLES_CORE_SAMEBOY, 0) == ORACLES_QUALITY_CUSTOM);
    CHECK(oracles_settings_quality_of(2, ORACLES_CORE_SAMEBOY, 2) == ORACLES_QUALITY_MAX);
    CHECK(oracles_settings_default_quality(0, 4, 8192) == ORACLES_QUALITY_MAX);
    CHECK(oracles_settings_default_quality(1, 4, 4096) == ORACLES_QUALITY_MEDIUM);    /* the RG DS */
    CHECK(oracles_settings_default_quality(1, 8, 5800) == ORACLES_QUALITY_HIGH);      /* a "6 GB" device */
    CHECK(oracles_settings_auto_vsync(60u, 0) && oracles_settings_auto_vsync(59u, 0) && !oracles_settings_auto_vsync(120u, 0));
    CHECK(!oracles_settings_auto_vsync(60u, 1) && !oracles_settings_auto_vsync(120u, 1));   /* Android: off */
    CHECK(oracles_settings_default_quality(1, 8, 4096) == ORACLES_QUALITY_MEDIUM);
    CHECK(oracles_settings_lighter_quality(ORACLES_QUALITY_MAX) == ORACLES_QUALITY_HIGH);
    CHECK(oracles_settings_lighter_quality(ORACLES_QUALITY_MEDIUM) == ORACLES_QUALITY_LOW);
    CHECK(oracles_settings_lighter_quality(ORACLES_QUALITY_LOW) == ORACLES_QUALITY_CUSTOM);
    CHECK(oracles_settings_lighter_quality(ORACLES_QUALITY_CUSTOM) == ORACLES_QUALITY_CUSTOM);
    /* Display's Quality (ui_page_nav.c) reads the same profile off every view, core and workers, and picking one there
     * sets what oracles_settings_apply_quality sets. */
    for (int view = 0; view < 3; view++)
        for (int core = 0; core < 2; core++)
            for (int ghosts = 0; ghosts < 3; ghosts++) {
                OraclesHomeNav nav;
                oracles_home_init(&nav);
                oracles_settings_defaults(&written);
                written.view = nav.display.view = view;
                written.core = nav.display.core = core;
                written.ghosts = nav.display.workers = ghosts;
                CHECK((int)oracles_settings_quality(&written) == oracles_display_quality(&nav));
            }
    for (int q = ORACLES_QUALITY_LOW; q < ORACLES_QUALITY_CUSTOM; q++) {
        OraclesHomeNav nav;
        oracles_home_init(&nav);
        nav.screen = ORACLES_SCREEN_DISPLAY;
        nav.display.workers = 1;
        oracles_settings_defaults(&written);
        oracles_settings_apply_quality(&written, (OraclesQuality)q);
        CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_QUALITY, q) == ORACLES_HOME_STORE);
        CHECK(nav.display.view == written.view && nav.display.core == oracles_settings_core(&written) && nav.display.workers == written.ghosts);
    }
    /* quality_hint= round-trips, none by default and not written then; a first opening is no file. */
    oracles_settings_defaults(&written);
    snprintf(written.path, sizeof written.path, "%s", path);
    CHECK(written.quality_hint == -1);
    written.quality_hint = ORACLES_QUALITY_HIGH;
    CHECK(oracles_settings_store(&written) == 1);
    oracles_settings_defaults(&read_back);
    snprintf(read_back.path, sizeof read_back.path, "%s", path);
    oracles_settings_load(&read_back);
    CHECK(read_back.quality_hint == ORACLES_QUALITY_HIGH && !read_back.first_run);
    remove(path);
    oracles_settings_defaults(&read_back);
    snprintf(read_back.path, sizeof read_back.path, "%s", path);
    oracles_settings_load(&read_back);
    CHECK(read_back.first_run && read_back.quality_hint == -1);

    /* A game ran slowly: in Enhanced, a minute at least (3584 frames at 59.7275 Hz), over 2 % of its frames late. */
    CHECK(oracles_settings_ran_slowly(1, 6000, 121) && !oracles_settings_ran_slowly(1, 6000, 120));   /* 2 % is not over */
    CHECK(!oracles_settings_ran_slowly(1, 3583, 3000) && oracles_settings_ran_slowly(1, ORACLES_SLOW_FRAMES, 72));
    CHECK(!oracles_settings_ran_slowly(0, 6000, 3000));   /* Faithful */
    /* The suggestion: the quality just lighter, or under Custom the view just nearer; none under Low, nor in the near view. */
    OraclesQuality lighter;
    int lighter_view;
    oracles_settings_defaults(&written);
    snprintf(written.path, sizeof written.path, "%s", path);
    oracles_settings_apply_quality(&written, ORACLES_QUALITY_HIGH);
    CHECK(oracles_settings_lighter(&written, &lighter, &lighter_view) && lighter == ORACLES_QUALITY_MEDIUM);
    CHECK(!oracles_settings_slow_hint(&written, 1, 6000, 100) && written.quality_hint == -1);   /* under 2 % */
    CHECK(!oracles_settings_slow_hint(&written, 1, 3000, 3000) && written.quality_hint == -1);   /* under a minute */
    CHECK(!oracles_settings_slow_hint(&written, 0, 6000, 3000) && written.quality_hint == -1);   /* Faithful */
    CHECK(oracles_settings_slow_hint(&written, 1, 6000, 300) && written.quality_hint == ORACLES_QUALITY_HIGH);
    /* Once a quality: the same one flagged suggests nothing again, nor does the same quality left as it was. */
    CHECK(!oracles_settings_slow_hint(&written, 1, 6000, 300));
    oracles_settings_quality_changed(&written, 2, ORACLES_CORE_MGBA, 0);
    CHECK(written.quality_hint == ORACLES_QUALITY_HIGH);
    /* quality_hint written when the toast shows, and read back. */
    CHECK(oracles_settings_store(&written) == 1);
    oracles_settings_defaults(&read_back);
    snprintf(read_back.path, sizeof read_back.path, "%s", path);
    oracles_settings_load(&read_back);
    CHECK(read_back.quality_hint == ORACLES_QUALITY_HIGH && !oracles_settings_slow_hint(&read_back, 1, 6000, 300));
    /* Another quality picked: Medium suggests Low; back at High, the hint no longer holds it back. */
    oracles_settings_apply_quality(&written, ORACLES_QUALITY_MEDIUM);
    oracles_settings_quality_changed(&written, 2, ORACLES_CORE_MGBA, 0);
    CHECK(written.quality_hint == -1);
    CHECK(oracles_settings_slow_hint(&written, 1, 6000, 300) && written.quality_hint == ORACLES_QUALITY_MEDIUM);
    oracles_settings_apply_quality(&written, ORACLES_QUALITY_HIGH);
    oracles_settings_quality_changed(&written, 1, ORACLES_CORE_MGBA, 0);
    CHECK(written.quality_hint == -1 && oracles_settings_slow_hint(&written, 1, 6000, 300) && written.quality_hint == ORACLES_QUALITY_HIGH);
    /* Low: nothing lighter, no suggestion. */
    oracles_settings_apply_quality(&written, ORACLES_QUALITY_LOW);
    written.quality_hint = -1;
    CHECK(!oracles_settings_lighter(&written, &lighter, &lighter_view) && !oracles_settings_slow_hint(&written, 1, 6000, 3000) && written.quality_hint == -1);
    /* Custom: the far view on the Accurate core with the workers on auto suggests the medium view, once; the view
     * changed by hand, the near view has nothing nearer. */
    oracles_settings_defaults(&written);
    snprintf(written.path, sizeof written.path, "%s", path);
    CHECK(oracles_settings_quality(&written) == ORACLES_QUALITY_CUSTOM);
    CHECK(oracles_settings_lighter(&written, &lighter, &lighter_view) && lighter == ORACLES_QUALITY_CUSTOM && lighter_view == 1);
    CHECK(oracles_settings_slow_hint(&written, 1, 6000, 300) && written.quality_hint == ORACLES_QUALITY_CUSTOM);
    CHECK(!oracles_settings_slow_hint(&written, 1, 6000, 300));
    CHECK(oracles_settings_store(&written) == 1);
    oracles_settings_defaults(&read_back);
    snprintf(read_back.path, sizeof read_back.path, "%s", path);
    oracles_settings_load(&read_back);
    CHECK(read_back.quality_hint == ORACLES_QUALITY_CUSTOM);
    written.view = 1;
    oracles_settings_quality_changed(&written, 2, oracles_settings_core(&written), 0);
    CHECK(written.quality_hint == -1);
    CHECK(oracles_settings_lighter(&written, &lighter, &lighter_view) && lighter_view == 0);
    written.view = 0;
    CHECK(!oracles_settings_lighter(&written, &lighter, &lighter_view) && !oracles_settings_slow_hint(&written, 1, 6000, 300));

    /* The sub-pixel camera: absent, the platform's default whatever the Quality, on on the desktops at every scale, on
     * Android from three times on, and not written; set either way, written as it is, at every scale. */
#ifdef __ANDROID__
    CHECK(oracles_settings_subpixel_default_from() == 3u);
#else
    CHECK(oracles_settings_subpixel_default_from() == 0u);
#endif
    for (unsigned scale = 1; scale <= 6; scale++)
        CHECK(oracles_settings_default_subpixel_camera(0, scale) == 1 && oracles_settings_default_subpixel_camera(1, scale) == (scale >= 3));
    oracles_settings defaults_only;
    oracles_settings_defaults(&defaults_only);
    snprintf(defaults_only.path, sizeof defaults_only.path, "%s", path);
    CHECK(defaults_only.subpixel_camera == -1);
    for (unsigned scale = 1; scale <= 6; scale++) {
#ifdef __ANDROID__
        const int platform_subpixel = scale >= 3;
#else
        const int platform_subpixel = 1;
#endif
        CHECK(oracles_settings_subpixel_camera(&defaults_only, scale) == platform_subpixel);
        for (int quality = 0; quality < ORACLES_QUALITIES; quality++) {
            oracles_settings_apply_quality(&defaults_only, (OraclesQuality)quality);
            CHECK(oracles_settings_subpixel_camera(&defaults_only, scale) == platform_subpixel);
        }
    }
    oracles_settings_store(&defaults_only);
    CHECK(!file_has_line(path, "subpixel_camera=0\n") && !file_has_line(path, "subpixel_camera=1\n"));
    for (int chosen = 0; chosen < 2; chosen++) {
        write_text(path, chosen ? "subpixel_camera=1\n" : "subpixel_camera=0\n");
        oracles_settings_defaults(&read_back);
        snprintf(read_back.path, sizeof read_back.path, "%s", path);
        oracles_settings_load(&read_back);
        CHECK(read_back.subpixel_camera == chosen);
        for (unsigned scale = 1; scale <= 6; scale++) CHECK(oracles_settings_subpixel_camera(&read_back, scale) == chosen);
        oracles_settings_store(&read_back);
        CHECK(file_has_line(path, chosen ? "subpixel_camera=1\n" : "subpixel_camera=0\n"));
    }

    unknown_keys(path);
    spaces_and_case(path);
    menu_pad_resolved();
    display_stored();
    pause_changes();
    text_layout();
    pause_live_changes();
    state_display();
    display_options();
    char restored[ORACLES_SETTINGS_PATH_LENGTH];
    snprintf(restored, sizeof restored, "%s.restored", path);
    display_restored(restored);
    transitions_line(path);

    remove(path);
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("launcher settings: ROMs, the profile, the active mods and the window's size round-trip through settings.txt\n");
    return 0;
}
