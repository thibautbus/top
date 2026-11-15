/* Controls after a key or a controller's input (ui_controls_nav.h,
 * ui_controls_layout.h, ui_home_nav.h): the fixed keys in game show on a
 * desktop before any input and once a key was pressed, not after a
 * controller's input alone, Menu's cell staying; on Android not until a key
 * is pressed, never F11, Back beside Menu's cell; the hints name the device's
 * keys.  Against the layout reference, tests/launcher_layout_reference.json
 * (layout_reference.h), the test's argument, in its frames 21i to 21n (16:9:
 * after a key, after a controller's input, on Android with a controller and
 * with a key, from the pause, Menu's button waiting) and 21o to 21t (4:3's In
 * game tab: after a key, after a controller's input, on Android with a
 * controller and with a key, from the pause with a controller and with a
 * key).  The other keys the launcher names follow the same rule: Display's
 * colour correction says F2 only where a keyboard is at hand (21u, 21v, 21w),
 * and a controller's cell waiting after a controller's input names the menu's
 * button as the one that cancels (21n, 21x). */
#include "layout_reference.h"
#include "ui_controls_layout.h"
#include "ui_controls_nav.h"
#include "ui_page_layout.h"
#include "ui_page_nav.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); layout_fail(); } } while (0)

static void controls_nav(OraclesHomeNav *nav)
{
    oracles_home_init(nav);
    nav->games[0].usable = 1;
    nav->games[0].hotkeys = 1;
    oracles_controls_open(nav);
}

/* A help bar against a frame's: each hint's box, its key's frame and text, its label. */
static void hints_at(const char *frame, const OraclesHomeNav *nav, OraclesUiLayout layout, unsigned expected)
{
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    OraclesUiHintLayout h[ORACLES_HOME_MAX_HINTS];
    const unsigned count = oracles_home_hints(nav, hints);
    CHECK(count == expected);
    if (count != expected) return;
    oracles_ui_layout_hints(layout, hints, count, h);
    for (unsigned i = 0; i < count; i++) {
        char what[32];
        snprintf(what, sizeof what, "hint %u", i); layout_box(frame, what, &h[i].box);
        snprintf(what, sizeof what, "hint %u key", i); layout_box(frame, what, &h[i].key_box);
        snprintf(what, sizeof what, "hint %u key text", i); layout_line(frame, what, &h[i].key);
        snprintf(what, sizeof what, "hint %u label", i); layout_line(frame, what, &h[i].label);
    }
}

/* Esc's frame and text, Menu's meaning; the fixed keys F2 to F7 and, unless `android`, F11. */
static void keys_probes(const char *frame, const OraclesUiControlsLayout *l, int android)
{
    layout_box(frame, "menu key", &l->shortcut_keys[5]);
    layout_line(frame, "menu key text", &l->shortcut_key_texts[5]);
    layout_line(frame, "menu label", &l->shortcut_labels[5]);
    for (int i = 0; i < (android ? 4 : 5); i++) {
        char what[32];
        snprintf(what, sizeof what, "key %d", i); layout_box(frame, what, &l->shortcut_keys[i]);
        snprintf(what, sizeof what, "key %d text", i); layout_line(frame, what, &l->shortcut_key_texts[i]);
        snprintf(what, sizeof what, "key %d label", i); layout_line(frame, what, &l->shortcut_labels[i]);
    }
}

/* 16:9's In game row: its top and height (one line or two), Menu's cell and meaning. */
static void in_game_row(const char *frame, const OraclesUiControlsLayout *l)
{
    float bottom = l->menu_pad.y + l->menu_pad.h;
    for (int i = 0; i < 6; i++)
        if (l->shortcut_keys[i].w > 0.0f && l->shortcut_keys[i].y + l->shortcut_keys[i].h > bottom) bottom = l->shortcut_keys[i].y + l->shortcut_keys[i].h;
    layout_near(frame, "In game row", "y", l->menu_pad.y);
    layout_near(frame, "In game row", "h", bottom - l->menu_pad.y);
    layout_box(frame, "menu pad", &l->menu_pad);
    layout_line(frame, "menu pad text", &l->menu_pad_text);
    layout_line(frame, "menu label", &l->shortcut_labels[5]);
    layout_line(frame, "In game", &l->shortcuts_label);
}

/* 4:3's In game tab: Menu's row, its meaning on one line or two, the list down to its last row's end (Swimming's, or on
 * Android Touch vibration's, or the last key's, 72 tall, after it). */
static void in_game_4_3(const char *frame, const OraclesUiControlsLayout *l)
{
    float bottom = l->swim.y + l->swim.h;
    if (l->vibration.y + l->vibration.h > bottom) bottom = l->vibration.y + l->vibration.h;
    for (int i = 0; i < 5; i++)
        if (l->shortcut_keys[i].w > 0.0f && l->shortcut_keys[i].y + l->shortcut_keys[i].h * 0.5f + 36.0f > bottom)
            bottom = l->shortcut_keys[i].y + l->shortcut_keys[i].h * 0.5f + 36.0f;
    layout_box(frame, "menu pad", &l->menu_pad);
    layout_line(frame, "menu pad text", &l->menu_pad_text);
    layout_line(frame, "menu label", &l->shortcut_labels[5]);
    layout_near(frame, "menu label", "lines", (float)l->menu_label.count);
    layout_near(frame, "menu label", "width", l->menu_label.w);
    layout_line(frame, "title", &l->shortcuts_label);
    layout_near(frame, "list", "h", bottom - l->shortcuts_label.y);
    layout_box(frame, "swim", &l->swim);
    layout_line(frame, "swim label", &l->swim_title);
    layout_box(frame, "swim tap", &l->swim_choices[0].box);
    layout_box(frame, "swim hold", &l->swim_choices[1].box);
    layout_line(frame, "swim explanation", &l->swim_text.lines[0]);
}

/* 16:9: every key on a desktop before any input and after a key (21i), none but Menu's cell after a controller's input
 * alone (21j, from the pause 21m), where Menu's cell waiting says its button cancels (21n); on Android none until a key
 * is pressed, Back framed after Menu's cell (21k), then the keys but F11 (21l), the line beside the title there. */
static void layout_16_9(void)
{
    OraclesHomeNav nav;
    controls_nav(&nav);
    OraclesHomeControls *c = &nav.controls;
    OraclesUiControlsLayout l;
    for (int i = 0; i < 6; i++) CHECK(oracles_controls_shortcut_shown(&nav, i));
    CHECK(oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD) && nav.keyboard_used);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    in_game_row("21i", &l);
    keys_probes("21i", &l, 0);
    hints_at("21i", &nav, ORACLES_UI_LAYOUT_16_9, 3);
    /* A controller's input after a key: the keys stay. */
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    CHECK(oracles_controls_shortcut_shown(&nav, 0) && oracles_controls_shortcut_shown(&nav, 5));
    /* No key pressed yet: a controller's input hides them but Menu's cell, the row on one line, its top kept. */
    controls_nav(&nav);
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    for (int i = 0; i < 6; i++) CHECK(!oracles_controls_shortcut_shown(&nav, i));
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(l.shortcut_keys[5].w == 0.0f && l.shortcut_keys[0].w == 0.0f && l.shortcut_labels[0].w == 0.0f && l.menu_back.w == 0.0f);
    in_game_row("21j", &l);
    hints_at("21j", &nav, ORACLES_UI_LAYOUT_16_9, 3);
    /* Menu's cell is still reached beside Start's. */
    c->column = 1; c->row = 7;
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(c->column == 2 && c->row == ORACLES_CONTROLS_ROW_MENU);
    /* From the pause: Swimming applies to the game, with no line under it. */
    nav.in_game = 1;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    in_game_row("21m", &l);
    layout_box("21m", "swim", &l.swim);
    hints_at("21m", &nav, ORACLES_UI_LAYOUT_16_9, 3);
    /* Menu's cell waiting: the hint names its button, which gives up. */
    oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_MENU, -1);
    CHECK(c->capturing);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    layout_box("21n", "menu pad", &l.menu_pad);
    layout_line("21n", "menu pad text", &l.menu_pad_text);
    hints_at("21n", &nav, ORACLES_UI_LAYOUT_16_9, 1);

    controls_nav(&nav);
    nav.android = 1;
    oracles_home_input(&nav, ORACLES_INPUT_POINTER);
    CHECK(!oracles_controls_shortcut_shown(&nav, 0) && !oracles_controls_shortcut_shown(&nav, 5));
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    in_game_row("21k", &l);
    layout_box("21k", "menu back", &l.menu_back);
    layout_line("21k", "menu back text", &l.menu_back_text);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    CHECK(oracles_controls_shortcut_shown(&nav, 0) && !oracles_controls_shortcut_shown(&nav, 4) && oracles_controls_shortcut_shown(&nav, 5));
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(l.shortcut_keys[4].w == 0.0f && l.shortcut_labels[4].w == 0.0f);
    in_game_row("21l", &l);
    keys_probes("21l", &l, 1);
    layout_box("21l", "menu back", &l.menu_back);
    layout_line("21l", "menu back text", &l.menu_back_text);
}

/* 4:3's In game tab, after a key (21o) and a controller's input alone (21p: the keys' column left empty, Menu's cell and
 * meaning in place, Swimming under them, which the moves reach in turn), from the pause (21s, 21t), on Android ("Menu ·
 * also Back" on one line, Swimming 8 closer to it, Touch vibration under it: 21q; with a key, the keys but F11, after
 * Touch vibration: 21r).  Every meaning starts in the same column. */
static void layout_4_3(void)
{
    OraclesHomeNav nav;
    controls_nav(&nav);
    OraclesHomeControls *c = &nav.controls;
    OraclesUiControlsLayout l;
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_TAB, ORACLES_CONTROLS_TAB_IN_GAME);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    in_game_4_3("21o", &l);
    keys_probes("21o", &l, 0);
    hints_at("21o", &nav, ORACLES_UI_LAYOUT_4_3, 3);
    nav.keyboard_used = 0;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    CHECK(l.shortcut_keys[5].w == 0.0f && l.shortcut_keys[0].w == 0.0f);
    in_game_4_3("21p", &l);
    hints_at("21p", &nav, ORACLES_UI_LAYOUT_4_3, 3);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->column == 0 && c->row == ORACLES_CONTROLS_ROW_MENU);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->row == ORACLES_CONTROLS_ROW_SWIM);
    c->row = ORACLES_CONTROLS_ROW_TAB;
    nav.in_game = 1;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    in_game_4_3("21s", &l);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    in_game_4_3("21t", &l);
    keys_probes("21t", &l, 0);

    nav.in_game = 0;
    nav.android = 1;
    nav.keyboard_used = 0;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    CHECK(!strcmp(oracles_controls_menu_label(&nav), "Menu \xc2\xb7 also Back"));
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    CHECK(l.menu_label.count == 1 && l.shortcut_labels[5].x == l.menu_pad.x + l.menu_pad.w + 24.0f);
    in_game_4_3("21q", &l);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    CHECK(l.shortcut_keys[4].w == 0.0f && l.shortcut_labels[4].w == 0.0f && l.menu_label.count == 1 && l.shortcut_labels[0].x == l.shortcut_labels[5].x);
    in_game_4_3("21r", &l);
    keys_probes("21r", &l, 1);
}

/* Display › Picture's colour correction after a controller's input: its explanation without F2, on one line beside its
 * choices (21u, the row two lines tall all the same); in 4:3's help, without F2 (21v) and, after a key, with it (21w). */
static void colour_help(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.games[0].usable = 1;
    nav.games[0].rom = ORACLES_ROM_ORIGINAL;
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.picture_page = 1;   /* Display › Picture, since turn 34 */
    nav.row = ORACLES_DISPLAY_COLOUR;
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.display.window = 2;
    OraclesUiDisplayTexts t;
    OraclesUiDisplayLayout l;
    CHECK(strstr(oracles_display_colour_explanation(&nav, 0), "Also F2 in game."));
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    CHECK(!strcmp(t.explanations[0], "On: colors as the Game Boy Color screen showed them. Off: the raw palette.") && l.explanations[0].count == 1);
    layout_near("21u", "color explanation", "top", l.explanations[0].lines[0].y);
    layout_near("21u", "color explanation", "width", l.explanations[0].w);
    layout_box("21u", "Color row", &l.rows[ORACLES_DISPLAY_COLOUR]);
    layout_box("21u", "color Off", &l.colour[0].box);
    layout_box("21u", "color On", &l.colour[1].box);
    layout_box("21u", "panel", &l.panel);

    nav.layout = ORACLES_UI_LAYOUT_4_3;
    nav.row = ORACLES_DISPLAY_COLOUR;
    nav.display.screen_w = nav.display.room_w = 1440;
    nav.display.room_h = 1080 - 64;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    CHECK(!strcmp(t.help, "On: as the Game Boy Color showed it."));
    layout_line("21v", "help text", &l.help);
    layout_near("21v", "help", "y", l.help.y);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    CHECK(!strcmp(t.help, "On: as the Game Boy Color showed it. Also F2."));
    layout_line("21w", "help text", &l.help);
    /* On Android, F2 only once a key is pressed. */
    oracles_home_init(&nav);
    nav.android = 1;
    CHECK(!strstr(oracles_display_colour_explanation(&nav, 1), "F2"));
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    CHECK(strstr(oracles_display_colour_explanation(&nav, 1), "F2"));
}

/* A cell waiting: a key's says Esc cancels whatever the device; a controller's names the menu's button after a
 * controller's input (Guide without one), Esc otherwise; in 16:9 (21n) and 4:3 (21x). */
static void capture_prompt(void)
{
    OraclesHomeNav nav;
    controls_nav(&nav);
    OraclesUiControlsLayout l;
    oracles_controls_click(&nav, 0, 4, -1);
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    CHECK(!strcmp(oracles_controls_prompt(&nav), "Press a key \xc2\xb7 Esc cancels"));
    oracles_controls_cancel(&nav);
    oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_MENU, -1);
    CHECK(!strcmp(oracles_controls_prompt(&nav), "Press a gamepad button \xc2\xb7 L3 cancels"));
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    layout_line("21n", "prompt", &l.prompt);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    layout_line("21x", "prompt", &l.prompt);
    layout_box("21x", "menu pad", &l.menu_pad);
    oracles_home_input(&nav, ORACLES_INPUT_POINTER);
    CHECK(!strcmp(oracles_controls_prompt(&nav), "Press a gamepad button \xc2\xb7 Esc cancels"));
    nav.controls.menu_pad[0] = 0;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    CHECK(!strcmp(oracles_controls_prompt(&nav), "Press a gamepad button \xc2\xb7 Guide cancels"));
}

int main(int argc, char **argv)
{
    if (argc < 2 || !layout_reference_load(argv[1])) { fprintf(stderr, "usage: oracles-test-launcher-devices launcher_layout_reference.json\n"); return 2; }
    if (!oracles_ui_fonts_load()) { fprintf(stderr, "FAIL the embedded fonts do not load\n"); return 1; }
    layout_16_9();
    layout_4_3();
    colour_help();
    capture_prompt();
    if (layout_failures()) { fprintf(stderr, "%d failure(s)\n", layout_failures()); return 1; }
    printf("launcher devices: Controls' keys in game follow the device used last, as do the hints, and match the layout reference in both layouts within %.1f px\n",
           (double)LAYOUT_TOLERANCE);
    return 0;
}
