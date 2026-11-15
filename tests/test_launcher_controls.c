/* Controls and the pause menu (ui_controls_nav.h, ui_controls_layout.h,
 * ui_home_nav.h): the capture of a key or a button (its states, a key taken
 * from another cell, the cancel), the moves between cells, Reset, the Item
 * hotkeys' line, Swimming; in 4:3 the tabs (keys and pointer), the moves
 * within a tab, the capture from Buttons and Hotkeys, In game and its
 * Swimming, and Controls opened from the pause menu; the pause menu and its
 * reduced form under 960 pixels wide; and Controls' layout against the
 * layout reference, tests/launcher_layout_reference.json
 * (layout_reference.h), the test's argument, in its frames 7g (at rest), 2d
 * (A's key waiting), 17a (Swimming on Hold, highlighted) and 17b (from the
 * pause menu), and in 4:3 10u (Buttons), 10i (A's key waiting), 10j
 * (Hotkeys), 10q (a fan game's Hotkeys from the pause menu), 10k (In game),
 * 17c and 17d (Swimming on Hold, highlighted, at 1440x1080 and 640x480) and
 * 17e (from the pause menu); and a fan game's Controls, from the pause menu
 * in 16:9, Swimming and the Item hotkeys dimmed with their reason (swim
 * fan), and its Swimming in 4:3 (swim fan 4:3, and from the pause, swim fan
 * pause 4:3); Menu's controller button, its moves,
 * its capture and its toast, highlighted in 16:9 (18a) and in 4:3's In game
 * (18b, 18c), and waiting (menu capture, menu capture 4:3); Android's Touch vibration in 16:9 (vibration) and in
 * 4:3's In game tab at 640x480 (vibration 640x480, vibration 640x480 keys); Oracles Combined's item hotkeys, on, off and
 * from the pause (42a to 42c, 42e combined 640x480), the reason of what a fan game lacks (42d, 42e), and Swimming's from the
 * home screen in 16:9, Moonrise Regalia's and Gifts of Kinomi's (47a, 47a kinomi). */
#include "home_fixture.h"
#include "layout_reference.h"
#include "ui_page_layout.h"
#include "ui_controls_layout.h"
#include "ui_controls_nav.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); layout_fail(); } } while (0)

static void controls_nav(OraclesHomeNav *nav)
{
    oracles_home_init(nav);
    fan_profiles(nav);
    nav->games[0].usable = 1;
    nav->games[0].hotkeys = 1;
    oracles_controls_open(nav);
}

static void capture(void)
{
    OraclesHomeNav nav;
    controls_nav(&nav);
    OraclesHomeControls *c = &nav.controls;
    /* OK on Right's key: it waits, and says so. */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && c->capturing);
    CHECK(!strcmp(oracles_controls_cell_text(&nav, 0, 0), "Press a key\xe2\x80\xa6"));
    CHECK(!strcmp(oracles_controls_prompt(&nav), "Press a key \xc2\xb7 Esc cancels"));
    /* A controller's button, a fixed shortcut: not for a keyboard cell, it keeps waiting. */
    CHECK(oracles_controls_capture_button(&nav, "a") == ORACLES_HOME_STAY && c->capturing);
    CHECK(oracles_controls_capture_key(&nav, "F5") == ORACLES_HOME_STAY && c->capturing);
    /* Escape cancels: the key stays. */
    CHECK(oracles_controls_capture_key(&nav, "Escape") == ORACLES_HOME_STAY && !c->capturing && !strcmp(c->keys[0], "Right"));
    /* A key takes the cell and asks to be stored. */
    oracles_home_act(&nav, ORACLES_HOME_OK);
    CHECK(oracles_controls_capture_key(&nav, "D") == ORACLES_HOME_STORE && !c->capturing && !strcmp(c->keys[0], "D"));
    /* A key another cell held moves: that cell is left without one, "—" (the reference's frame 2d). */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    oracles_home_act(&nav, ORACLES_HOME_OK);
    CHECK(oracles_controls_capture_key(&nav, "Z") == ORACLES_HOME_STORE && !strcmp(c->keys[1], "Z") && c->keys[5][0] == 0);
    CHECK(!strcmp(oracles_controls_cell_text(&nav, 0, 5), "\xe2\x80\x94"));
    /* The item hotkeys' keys are the same keyboard: a slot's key taken back by a button of the game. */
    c->column = 0; c->row = 2;
    oracles_home_act(&nav, ORACLES_HOME_OK);
    CHECK(oracles_controls_capture_key(&nav, "left shift") == ORACLES_HOME_STORE && !strcmp(c->keys[2], "left shift") && c->hotkey_keys[4][0] == 0);
    /* A controller cell waits for a button; the d-pad does not bind, a key other than Escape neither. */
    c->column = 1; c->row = 4;
    oracles_home_act(&nav, ORACLES_HOME_OK);
    CHECK(!strcmp(oracles_controls_prompt(&nav), "Press a gamepad button \xc2\xb7 Esc cancels"));
    CHECK(oracles_controls_capture_button(&nav, "dpup") == ORACLES_HOME_STAY && c->capturing);
    CHECK(oracles_controls_capture_key(&nav, "X") == ORACLES_HOME_STAY && c->capturing);
    CHECK(oracles_controls_capture_button(&nav, "x") == ORACLES_HOME_STORE && !strcmp(c->pads[0], "x") && c->hotkey_pads[0][0] == 0);
    /* The d-pad's cells do not bind, nor take the highlight from the pointer. */
    CHECK(oracles_controls_cell_locked(1, 0) && !strcmp(oracles_controls_cell_text(&nav, 1, 0), "D-pad, left stick"));
    CHECK(oracles_controls_click(&nav, 1, 0, -1) == ORACLES_HOME_STAY && !c->capturing);
    /* Back while waiting cancels; Reset brings the defaults back, the slots' items kept. */
    snprintf(c->items[0][0], sizeof c->items[0][0], "Shooter");
    CHECK(oracles_controls_click(&nav, 0, 3, -1) == ORACLES_HOME_STAY && c->capturing);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && !c->capturing && nav.screen == ORACLES_SCREEN_CONTROLS);
    CHECK(oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_RESET, -1) == ORACLES_HOME_STORE);
    CHECK(!strcmp(c->keys[0], "Right") && !strcmp(c->keys[5], "Z") && !strcmp(c->pads[0], "a") && !strcmp(c->hotkey_keys[4], "Left Shift"));
    CHECK(!strcmp(c->items[0][0], "Shooter"));
}

static void moves(void)
{
    OraclesHomeNav nav;
    controls_nav(&nav);
    OraclesHomeControls *c = &nav.controls;
    /* Down the keyboard column to Reset, then up again. */
    for (int i = 0; i < 8; i++) oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->column == 0 && c->row == ORACLES_CONTROLS_ROW_RESET);
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(c->column == 0 && c->row == 7);
    /* Right from Right's key: the controller column's nearest row, A (its first that binds). */
    c->row = 0;
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(c->column == 1 && c->row == 4);
    /* On to the item hotkeys' keys, the same row; up to the Item hotkeys' line. */
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(c->column == 2 && c->row == 4);
    for (int i = 0; i < 6; i++) oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(c->column == 2 && c->row == ORACLES_CONTROLS_ROW_MODE);
    /* There left and right turn the line: Off, then On again, each stored. */
    CHECK(oracles_controls_hotkeys_on(&nav));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.games[0].hotkeys == 0 && !oracles_controls_hotkeys_on(&nav));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.games[0].hotkeys == 1);
    /* On keeps equip when the file has it; Off and On again is use. */
    nav.games[0].hotkeys = 2;
    CHECK(oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_MODE, 1) == ORACLES_HOME_STAY && nav.games[0].hotkeys == 2);
    CHECK(oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_MODE, 0) == ORACLES_HOME_STORE && nav.games[0].hotkeys == 0);
    CHECK(oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_MODE, 1) == ORACLES_HOME_STORE && nav.games[0].hotkeys == 1);
    /* Down from the line to the first slot; the controller's slots have four rows. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->column == 2 && c->row == 0);
    c->row = 5;
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(c->column == 3 && c->row == 3);
    CHECK(!strcmp(oracles_controls_item_heading(&nav, ORACLES_UI_LAYOUT_16_9), "In Ages"));
}

/* Swimming against a frame's: its box, its title, Tap and Hold, its explanation's first line and its count of lines. */
static void swim_probes(const char *frame, const OraclesUiControlsLayout *l)
{
    layout_box(frame, "swim", &l->swim);
    layout_line(frame, "swim label", &l->swim_title);
    layout_box(frame, "swim tap", &l->swim_choices[0].box);
    layout_box(frame, "swim hold", &l->swim_choices[1].box);
    layout_line(frame, "swim tap text", &l->swim_choices[0].name);
    layout_line(frame, "swim hold text", &l->swim_choices[1].name);
    layout_line(frame, "swim explanation", &l->swim_text.lines[0]);
    layout_near(frame, "swim explanation", "lines", (float)l->swim_text.count);
    layout_near(frame, "swim explanation", "width", l->swim_text.w);
}

/* Swimming, under Reset: down from Reset; left, right and OK turn it, each stored; the pointer's Tap and Hold; Reset
 * keeps it; up back to Reset. */
static void swimming(void)
{
    OraclesHomeNav nav;
    controls_nav(&nav);
    OraclesHomeControls *c = &nav.controls;
    CHECK(c->swim_hold == 0);
    c->column = 1; c->row = 7;
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->column == 0 && c->row == ORACLES_CONTROLS_ROW_RESET);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->column == 0 && c->row == ORACLES_CONTROLS_ROW_SWIM);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->row == ORACLES_CONTROLS_ROW_SWIM);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && c->swim_hold == 1 && c->row == ORACLES_CONTROLS_ROW_SWIM);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && c->swim_hold == 0 && c->column == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && c->swim_hold == 1 && !c->capturing);
    CHECK(oracles_controls_click(&nav, 1, ORACLES_CONTROLS_ROW_SWIM, 1) == ORACLES_HOME_STAY && c->swim_hold == 1 && c->column == 0);
    CHECK(oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_SWIM, 0) == ORACLES_HOME_STORE && c->swim_hold == 0);
    CHECK(oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_SWIM, -1) == ORACLES_HOME_STAY && c->swim_hold == 0);
    CHECK(oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_SWIM, 1) == ORACLES_HOME_STORE && c->swim_hold == 1);
    CHECK(oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_RESET, -1) == ORACLES_HOME_STORE && c->swim_hold == 1);
    oracles_controls_hover(&nav, 1, ORACLES_CONTROLS_ROW_SWIM);
    CHECK(c->column == 0 && c->row == ORACLES_CONTROLS_ROW_SWIM);
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(c->column == 0 && c->row == ORACLES_CONTROLS_ROW_RESET);
    /* Moonrise Regalia's: from the pause menu in 16:9 (swim fan, turn 29's 27e), Swimming and the Item hotkeys, which its
     * profile has not, dimmed and inert, "Not available for this game." in place of the home screen's lines; in 4:3's In
     * game, Swimming says the same in the warning's tone (swim fan 4:3), from the pause its reason alone (swim fan pause
     * 4:3). */
    OraclesUiControlsLayout l;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(l.swim_fan.h == 0.0f);
    nav.games[ORACLES_HOME_GAME_MOONRISE].usable = 1;
    nav.entry = ORACLES_HOME_FAN;
    nav.fan_pick = ORACLES_HOME_HERO_MOONRISE;
    nav.controls.swim_hold = 0;
    nav.controls.row = 0;
    nav.in_game = 1;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    swim_probes("swim fan", &l);
    CHECK(l.swim_fan.h == 0.0f);
    layout_line("swim fan", "swim later", &l.swim_later);
    layout_box("swim fan", "left panel", &l.left_panel);
    layout_box("swim fan", "right panel", &l.right_panel);
    layout_box("swim fan", "Item hotkeys", &l.mode);
    CHECK(l.mode_fan.h == 0.0f && !l.mode_fan_text[0]);
    layout_line("swim fan", "later", &l.mode_later);
    layout_line("swim fan", "Hotkeys", &l.heading_hotkeys);
    layout_box("swim fan", "slot 0 key", &l.cells[2][0]);
    layout_line("swim fan", "slots note", &l.slots_note);
    CHECK(!strcmp(oracles_display_fan_reason, "Not available for this game."));
    nav.controls.row = ORACLES_CONTROLS_ROW_SWIM;
    nav.controls.column = 0;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.controls.swim_hold == 0);
    CHECK(oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_SWIM, 1) == ORACLES_HOME_STAY && nav.controls.swim_hold == 0);
    nav.controls.row = ORACLES_CONTROLS_ROW_TAB;
    nav.controls.tab = ORACLES_CONTROLS_TAB_IN_GAME;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    swim_probes("swim fan pause 4:3", &l);
    layout_line("swim fan pause 4:3", "swim later", &l.swim_later);
    CHECK(l.swim_fan.h == 0.0f);
    nav.controls.row = 0;
    nav.in_game = 0;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(!strcmp(l.mode_fan_text, "Not available for this game.") && l.mode_later.h == 0.0f);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    CHECK(l.swim.w == 0.0f);
    nav.controls.row = ORACLES_CONTROLS_ROW_TAB;
    nav.controls.tab = ORACLES_CONTROLS_TAB_IN_GAME;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    swim_probes("swim fan 4:3", &l);
    layout_line("swim fan 4:3", "swim fan", &l.swim_fan);
    CHECK(l.swim_later.h == 0.0f);
}

static void pause_menu(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, 0);
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    static const char *const labels[7] = { "Resume", "Save state", "Load state", "Controls", "Display", "Cheats", "Quit to launcher" };
    CHECK(oracles_home_items(&nav, items) == 7);
    /* No key's note; Load state greyed while there is no savestate, its time once there is one. */
    for (int i = 0; i < 7; i++) CHECK(!strcmp(items[i].label, labels[i]) && items[i].disabled == (i == 2));
    CHECK(!items[1].note && !items[2].note && !strcmp(items[2].reason, "No savestate yet") && oracles_home_click(&nav, 2) == ORACLES_HOME_STAY);
    oracles_home_hover(&nav, 0); snprintf(nav.load_note, sizeof nav.load_note, "from 22:14");
    CHECK(oracles_home_items(&nav, items) == 7 && !items[2].disabled && !strcmp(items[2].note, "from 22:14"));
    char state[ORACLES_HOME_STATE_LENGTH];
    oracles_home_state(&nav, ORACLES_HOME_HERO_AGES, state);
    CHECK(!strcmp(state, "Paused \xc2\xb7 Profile: Enhanced"));
    /* The entries' commands; Escape or B resumes; left and right do nothing. */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_RESUME);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_RESUME);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.entry == ORACLES_HOME_AGES);
    CHECK(oracles_home_click(&nav, 1) == ORACLES_HOME_SAVE_STATE && oracles_home_click(&nav, 2) == ORACLES_HOME_LOAD_STATE);
    CHECK(oracles_home_click(&nav, 6) == ORACLES_HOME_QUIT_GAME);
    /* Controls, Display and Cheats from the pause come back to it, on their entry. */
    CHECK(oracles_home_click(&nav, 3) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_CONTROLS);
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(nav.screen == ORACLES_SCREEN_PAUSE && oracles_home_focus(&nav) == 3);
    CHECK(oracles_home_click(&nav, 4) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_DISPLAY);
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(nav.screen == ORACLES_SCREEN_PAUSE && oracles_home_focus(&nav) == 4);
    CHECK(oracles_home_click(&nav, 5) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_CHEATS && nav.row == 0);
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(nav.screen == ORACLES_SCREEN_PAUSE && oracles_home_focus(&nav) == 5);
    /* Under 960 pixels wide: Resume, Save state, Load state, Quit to launcher. */
    oracles_home_pause(&nav, ORACLES_HOME_GAME_SEASONS, ORACLES_PROFILE_FAITHFUL, 1);
    CHECK(oracles_home_items(&nav, items) == 4);
    CHECK(!strcmp(items[2].label, "Load state") && !strcmp(items[3].label, "Quit to launcher"));
    CHECK(oracles_home_click(&nav, 3) == ORACLES_HOME_QUIT_GAME);
    oracles_home_state(&nav, ORACLES_HOME_HERO_SEASONS, state);
    CHECK(!strcmp(state, "Paused \xc2\xb7 Profile: Faithful"));
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    CHECK(oracles_home_hints(&nav, hints) == 3 && !strcmp(hints[2].label, "Resume") && hints[2].back);
    /* A page open while the window turns narrow closes onto Resume, never onto Quit to launcher. */
    static const OraclesScreen pages[3] = { ORACLES_SCREEN_CONTROLS, ORACLES_SCREEN_DISPLAY, ORACLES_SCREEN_CHEATS };
    for (int i = 0; i < 3; i++) {
        oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, 0);
        CHECK(oracles_home_click(&nav, 3u + (unsigned)i) == ORACLES_HOME_STAY && nav.screen == pages[i]);
        nav.narrow = 1;
        oracles_home_act(&nav, ORACLES_HOME_BACK);
        CHECK(nav.screen == ORACLES_SCREEN_PAUSE && oracles_home_focus(&nav) == 0);
        CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_RESUME);
    }
}

/* From the pause of an Oracle whose ROM no profile recognises, Swimming and the Item hotkeys, which it has not, are
 * inert and say why in both layouts; from a recognised one's they change. */
static void refused_rows(void)
{
    for (int layout = 0; layout < 2; layout++) {
        OraclesHomeNav nav;
        oracles_home_init(&nav);
        nav.layout = layout ? ORACLES_UI_LAYOUT_4_3 : ORACLES_UI_LAYOUT_16_9;
        oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_FAITHFUL, 0);
        nav.swim_refused = nav.hotkeys_refused = 1;
        oracles_controls_open(&nav);
        nav.controls.swim_hold = 0;
        nav.games[ORACLES_HOME_GAME_AGES].hotkeys = 0;
        CHECK(oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_SWIM, 1) == ORACLES_HOME_STAY && nav.controls.swim_hold == 0);
        CHECK(oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_MODE, 1) == ORACLES_HOME_STAY && !oracles_controls_hotkeys_on(&nav));
        nav.controls.column = 0;
        nav.controls.row = ORACLES_CONTROLS_ROW_SWIM;
        CHECK(oracles_controls_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.controls.swim_hold == 0);
        nav.controls.column = 2;
        nav.controls.row = ORACLES_CONTROLS_ROW_MODE;
        CHECK(oracles_controls_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && !oracles_controls_hotkeys_on(&nav));
        OraclesUiControlsLayout l;
        nav.controls.tab = ORACLES_CONTROLS_TAB_HOTKEYS;
        oracles_ui_layout_controls(nav.layout, &nav, &l);
        CHECK(l.mode_later.h > 0.0f && l.mode_later_text == oracles_display_original_reason && !l.mode_fan_text[0]);
        nav.controls.tab = ORACLES_CONTROLS_TAB_IN_GAME;
        nav.controls.row = ORACLES_CONTROLS_ROW_SWIM;
        oracles_ui_layout_controls(nav.layout, &nav, &l);
        CHECK(l.swim_later.h > 0.0f && l.swim_later_text == oracles_display_original_reason && l.swim_fan.h == 0.0f);
        nav.swim_refused = nav.hotkeys_refused = 0;
        oracles_ui_layout_controls(nav.layout, &nav, &l);
        CHECK(l.swim_later.h == 0.0f && !l.swim_later_text);
        nav.controls.column = 0;
        CHECK(oracles_controls_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.controls.swim_hold == 1);
        CHECK(oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_MODE, 1) == ORACLES_HOME_STORE && oracles_controls_hotkeys_on(&nav));
    }
}

static void layout(void)
{
    OraclesHomeNav nav;
    controls_nav(&nav);
    snprintf(nav.controls.items[0][0], sizeof nav.controls.items[0][0], "Seed Shooter");
    OraclesUiControlsLayout l;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    /* 7g: the title, the panels, the headings. */
    layout_near("7g", "title", "top", l.title.y);
    layout_near("7g", "title", "w", l.title.w);
    layout_box("7g", "left panel", &l.left_panel);
    layout_box("7g", "right panel", &l.right_panel);
    layout_line_top("7g", "Game buttons", &l.heading_buttons);
    layout_line_top("7g", "Keyboard", &l.heading_keyboard);
    layout_line_top("7g", "Gamepad", &l.heading_gamepad);
    /* The rows, 56 px each, 8 apart; the names centred in them. */
    for (int row = 0; row < 8; row++) {
        char what[32];
        snprintf(what, sizeof what, "row %d key", row); layout_box("7g", what, &l.cells[0][row]);
        snprintf(what, sizeof what, "row %d button", row); layout_box("7g", what, &l.cells[1][row]);
        snprintf(what, sizeof what, "row %d name", row); layout_line_top("7g", what, &l.button_names[row]);
    }
    layout_near("7g", "X", "x", l.cell_texts[0][4].x);
    layout_near("7g", "Backspace", "x", l.cell_texts[0][6].x);
    layout_near("7g", "d-pad", "x", l.cell_texts[1][0].x);
    layout_box("7g", "Reset", &l.reset);
    layout_line_top("7g", "Reset to defaults", &l.reset_text);
    layout_line_top("7g", "Reset dot", &l.reset_dot);
    /* The Item hotkeys' line, then the slots and the two modifiers. */
    layout_box("7g", "Item hotkeys", &l.mode);
    layout_line_top("7g", "Item hotkeys title", &l.mode_title);
    layout_box("7g", "Off", &l.mode_choices[0].box);
    layout_box("7g", "On", &l.mode_choices[1].box);
    layout_line_top("7g", "explanation", &l.mode_text);
    layout_line_top("7g", "note", &l.mode_note);
    layout_line_top("7g", "Hotkeys", &l.heading_hotkeys);
    layout_line_top("7g", "In Ages", &l.heading_items);
    for (int row = 0; row < 6; row++) {
        char what[32];
        snprintf(what, sizeof what, "slot %d key", row); layout_box("7g", what, &l.cells[2][row]);
        if (row < 4) { snprintf(what, sizeof what, "slot %d button", row); layout_box("7g", what, &l.cells[3][row]); }
    }
    layout_line_top("7g", "Seed Shooter", &l.items[0]);
    layout_line_top("7g", "Empty", &l.items[3]);
    layout_line_top("7g", "slots note", &l.slots_note);
    /* The fixed shortcuts. */
    /* The fixed keys in game under the right panel, on two lines from F7. */
    layout_line_top("7g", "In game", &l.shortcuts_label);
    layout_box("7g", "F2", &l.shortcut_keys[0]);
    layout_box("7g", "F11", &l.shortcut_keys[4]);
    layout_line_top("7g", "Menu", &l.shortcut_labels[5]);
    for (int i = 0; i < 6; i++) {
        char what[32];
        snprintf(what, sizeof what, "key %d", i); layout_box("7g", what, &l.shortcut_keys[i]);
        snprintf(what, sizeof what, "key %d label", i); layout_line("7g", what, &l.shortcut_labels[i]);
    }
    swim_probes("7g", &l);

    /* 2d: A's key waits, its border 2 px, its row 2 px taller; the rows under it and Reset move down by 2. */
    nav.controls.row = 4;
    oracles_home_act(&nav, ORACLES_HOME_OK);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    layout_line_top("2d", "prompt", &l.prompt);
    layout_box("2d", "A key", &l.cells[0][4]);
    layout_box("2d", "A button", &l.cells[1][4]);
    layout_near("2d", "Press a key", "x", l.cell_texts[0][4].x);
    layout_box("2d", "B key", &l.cells[0][5]);
    layout_line_top("2d", "Reset to defaults", &l.reset_text);
    layout_box("2d", "left panel", &l.left_panel);
    oracles_controls_cancel(&nav);

    /* 17a: Swimming on Hold, highlighted; the panel and the rows unmoved. */
    nav.controls.swim_hold = 1;
    nav.controls.row = ORACLES_CONTROLS_ROW_SWIM;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    swim_probes("17a", &l);
    layout_box("17a", "swim Tap box", &l.swim_choices[0].box);
    layout_box("17a", "swim Hold box", &l.swim_choices[1].box);
    layout_box("17a", "left panel", &l.left_panel);
    layout_box("17a", "Reset", &l.reset);
    CHECK(l.swim_fan.h == 0.0f);

    /* 17b (turn 27's 27a): from an Oracle, the Item hotkeys and Swimming apply to it: laid out as from the home screen,
     * no line saying they wait for the next Play. */
    OraclesUiControlsLayout home;
    nav.controls.swim_hold = 0;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &home);
    nav.in_game = 1;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(!memcmp(&l.mode, &home.mode, sizeof l.mode) && !memcmp(&l.swim, &home.swim, sizeof l.swim)
          && !memcmp(&l.left_panel, &home.left_panel, sizeof l.left_panel) && !memcmp(&l.right_panel, &home.right_panel, sizeof l.right_panel)
          && l.swim_later.h == 0.0f && l.mode_later.h == 0.0f);
    swim_probes("17b", &l);
    layout_box("17b", "left panel", &l.left_panel);
    layout_box("17b", "right panel", &l.right_panel);
    layout_box("17b", "Reset", &l.reset);
    layout_box("17b", "Item hotkeys", &l.mode);
}

/* 4:3: the tabs' strip, a row of its own; left, right and OK there turn the tab, the arrows move within it. */
static void tabs_4_3(void)
{
    OraclesHomeNav nav;
    controls_nav(&nav);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    OraclesHomeControls *c = &nav.controls;
    CHECK(oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_BUTTONS && c->row == 0 && c->column == 0);
    /* Up from Right's key, and right from it (the strip the nearest row of the controller's column): the strip. */
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(c->row == ORACLES_CONTROLS_ROW_TAB && c->column == 0 && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_BUTTONS);
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(c->row == ORACLES_CONTROLS_ROW_TAB);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->row == 0 && c->column == 0);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(c->row == ORACLES_CONTROLS_ROW_TAB && c->column == 0);
    /* Right, right, right: Hotkeys, In game, Buttons again; left wraps the other way; OK turns as right does. */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_HOTKEYS && c->column == 2);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_IN_GAME);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_BUTTONS);
    oracles_home_act(&nav, ORACLES_HOME_LEFT);
    CHECK(oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_IN_GAME && !c->capturing);
    /* In game: down to Menu's button, then to Swimming, where left and right turn it, the tab kept; down stays on
     * Swimming; up to Menu's button, then the strip; OK on the strip turns to Buttons. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->row == ORACLES_CONTROLS_ROW_MENU && c->column == 0 && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_IN_GAME);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && c->row == ORACLES_CONTROLS_ROW_MENU && !c->capturing);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->row == ORACLES_CONTROLS_ROW_SWIM && c->column == 0 && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_IN_GAME);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && c->swim_hold == 1 && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_IN_GAME);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && c->swim_hold == 0);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->row == ORACLES_CONTROLS_ROW_SWIM);
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(c->row == ORACLES_CONTROLS_ROW_MENU);
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(c->row == ORACLES_CONTROLS_ROW_TAB && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_IN_GAME);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_BUTTONS && !c->capturing);
    /* Buttons: from Up's key right to the controller's A, the nearest row that binds; down past Start to Reset. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(c->column == 1 && c->row == 4);
    for (int i = 0; i < 4; i++) oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->column == 0 && c->row == ORACLES_CONTROLS_ROW_RESET);
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(c->column == 0 && c->row == 7);
    /* Hotkeys: down from the strip to the Item hotkeys line, where left and right turn the hotkeys, then the slots;
     * up from a controller's slot to the line, then the strip, Hotkeys kept. */
    for (int i = 0; i < 8; i++) oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(c->row == ORACLES_CONTROLS_ROW_TAB);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->column == 2 && c->row == ORACLES_CONTROLS_ROW_MODE && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_HOTKEYS);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && !oracles_controls_hotkeys_on(&nav));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && oracles_controls_hotkeys_on(&nav));
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(c->column == 3 && c->row == 0);
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(c->column == 2 && c->row == ORACLES_CONTROLS_ROW_MODE);
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(c->column == 2 && c->row == ORACLES_CONTROLS_ROW_TAB && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_HOTKEYS);
    /* Bind to A has no controller's cell: right goes to the last slot's. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    for (int i = 0; i < 6; i++) oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->column == 2 && c->row == 5);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(c->column == 3 && c->row == 3);

    /* The pointer: a tab clicked shows it, the highlight on the strip; the strip is no target for the hover; a cell
     * of the tab shown takes the highlight, and the tab is the cell's. */
    CHECK(oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_TAB, ORACLES_CONTROLS_TAB_IN_GAME) == ORACLES_HOME_STAY);
    CHECK(c->row == ORACLES_CONTROLS_ROW_TAB && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_IN_GAME && !c->capturing);
    CHECK(oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_TAB, ORACLES_CONTROLS_TAB_HOTKEYS) == ORACLES_HOME_STAY);
    CHECK(c->row == ORACLES_CONTROLS_ROW_TAB && c->column == 2 && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_HOTKEYS);
    oracles_controls_hover(&nav, 0, ORACLES_CONTROLS_ROW_TAB);
    CHECK(c->row == ORACLES_CONTROLS_ROW_TAB && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_HOTKEYS);
    oracles_controls_hover(&nav, 3, 1);
    CHECK(c->column == 3 && c->row == 1 && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_HOTKEYS);
    CHECK(oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_MODE, 0) == ORACLES_HOME_STORE && !oracles_controls_hotkeys_on(&nav));
    CHECK(oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_TAB, 0) == ORACLES_HOME_STAY && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_BUTTONS);
    CHECK(oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_TAB, 3) == ORACLES_HOME_STAY && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_BUTTONS);
    CHECK(oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_RESET, -1) == ORACLES_HOME_STORE && c->row == ORACLES_CONTROLS_ROW_RESET);

    /* The window turned 16:9 with the highlight on the strip: the next key takes it to its tab's first cell. */
    oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_TAB, ORACLES_CONTROLS_TAB_HOTKEYS);
    nav.layout = ORACLES_UI_LAYOUT_16_9;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && c->column == 2 && c->row == ORACLES_CONTROLS_ROW_MODE);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    oracles_home_act(&nav, ORACLES_HOME_UP);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    nav.layout = ORACLES_UI_LAYOUT_16_9;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && c->column == 2 && c->row == ORACLES_CONTROLS_ROW_MENU && !c->capturing);
    CHECK(c->swim_hold == 0);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    oracles_home_act(&nav, ORACLES_HOME_UP);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    nav.layout = ORACLES_UI_LAYOUT_16_9;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && c->column == 0 && c->row == 0 && !c->capturing);
    /* Back from the strip leaves Controls, as from a cell. */
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(c->row == ORACLES_CONTROLS_ROW_TAB);
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(nav.screen == ORACLES_SCREEN_HOME && oracles_home_focus(&nav) == 2);
    /* Controls opens on Buttons, Right's key. */
    oracles_home_act(&nav, ORACLES_HOME_OK);
    CHECK(nav.screen == ORACLES_SCREEN_CONTROLS && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_BUTTONS && c->column == 0 && c->row == 0);
}

/* 4:3: a capture from Buttons and from Hotkeys; the prompt takes the tabs' place. */
static void capture_4_3(void)
{
    OraclesHomeNav nav;
    controls_nav(&nav);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    OraclesHomeControls *c = &nav.controls;
    OraclesUiControlsLayout l;
    /* Buttons: A's key, then the controller's B. */
    for (int i = 0; i < 4; i++) oracles_home_act(&nav, ORACLES_HOME_DOWN);
    oracles_home_act(&nav, ORACLES_HOME_OK);
    CHECK(c->capturing && c->column == 0 && c->row == 4);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    CHECK(l.prompt.w > 0.0f && l.tabs[0].w == 0.0f && l.tabs[2].w == 0.0f);
    /* Left and right do not leave a waiting cell, nor turn a tab. */
    oracles_home_act(&nav, ORACLES_HOME_UP);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(c->capturing && c->column == 0 && c->row == 4 && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_BUTTONS);
    CHECK(oracles_controls_capture_key(&nav, "D") == ORACLES_HOME_STORE && !strcmp(c->keys[4], "D") && !c->capturing);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    CHECK(l.prompt.w == 0.0f && l.tabs[0].w > 0.0f);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(c->column == 1 && c->row == 5);
    oracles_home_act(&nav, ORACLES_HOME_OK);
    CHECK(oracles_controls_capture_button(&nav, "leftstick") == ORACLES_HOME_STORE && !strcmp(c->pads[1], "leftstick"));
    /* Hotkeys: a slot's key, a slot's button, a key a game button held moving to it; Escape cancels. */
    oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_TAB, ORACLES_CONTROLS_TAB_HOTKEYS);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->column == 2 && c->row == 0);
    oracles_home_act(&nav, ORACLES_HOME_OK);
    CHECK(c->capturing && !strcmp(oracles_controls_prompt(&nav), "Press a key \xc2\xb7 Esc cancels"));
    CHECK(oracles_controls_capture_key(&nav, "Z") == ORACLES_HOME_STORE && !strcmp(c->hotkey_keys[0], "Z") && c->keys[5][0] == 0);
    CHECK(oracles_controls_click(&nav, 3, 1, -1) == ORACLES_HOME_STAY && c->capturing);
    CHECK(!strcmp(oracles_controls_prompt(&nav), "Press a gamepad button \xc2\xb7 Esc cancels"));
    CHECK(oracles_controls_capture_button(&nav, "leftstick") == ORACLES_HOME_STORE && !strcmp(c->hotkey_pads[1], "leftstick") && c->pads[1][0] == 0);
    CHECK(oracles_controls_click(&nav, 2, 4, -1) == ORACLES_HOME_STAY && c->capturing);
    CHECK(oracles_controls_capture_key(&nav, "Escape") == ORACLES_HOME_STAY && !c->capturing && !strcmp(c->hotkey_keys[4], "Left Shift"));
    CHECK(oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_HOTKEYS);
    /* While a cell waits, a tab clicked does nothing. */
    oracles_home_act(&nav, ORACLES_HOME_OK);
    CHECK(oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_TAB, ORACLES_CONTROLS_TAB_IN_GAME) == ORACLES_HOME_STAY);
    CHECK(c->capturing && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_HOTKEYS);
}

/* Controls from the pause menu in 4:3: it opens on Buttons, Back returns to its entry; a fan game's Hotkeys say that
 * they stay off and that the Item hotkeys wait for the next Play. */
static void pause_4_3(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    fan_profiles(&nav);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    oracles_home_pause(&nav, ORACLES_HOME_GAME_MOONRISE, ORACLES_PROFILE_ENHANCED, oracles_ui_layout_narrow(nav.layout, 640));
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    CHECK(oracles_home_items(&nav, items) == 7 && !strcmp(items[3].label, "Controls") && items[5].id == ORACLES_HOME_ITEM_CHEATS);
    CHECK(oracles_home_click(&nav, 3) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_CONTROLS);
    CHECK(oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_BUTTONS && nav.controls.row == 0);
    oracles_home_act(&nav, ORACLES_HOME_UP);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.controls.row == ORACLES_CONTROLS_ROW_MODE && oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && !oracles_controls_hotkeys_on(&nav));
    oracles_home_act(&nav, ORACLES_HOME_UP);
    OraclesUiControlsLayout l;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    CHECK(!l.mode_fan_text[0]);
    /* 10q: the Hotkeys tab, from the pause the reason in place of its fan game's line, the grid under it. */
    layout_box("10q", "tab 1", &l.tabs[1]);
    layout_line("10q", "tab 1 label", &l.tab_labels[1]);
    layout_box("10q", "Item hotkeys", &l.mode);
    layout_line("10q", "explanation", &l.mode_text);
    layout_line("10q", "note", &l.mode_note);
    layout_line("10q", "later", &l.mode_later);
    layout_line("10q", "Hotkeys", &l.heading_hotkeys);
    layout_line("10q", "Keyboard", &l.heading_hotkeys_keyboard);
    for (int row = 0; row < 6; row++) {
        char what[32];
        snprintf(what, sizeof what, "slot %d key", row); layout_box("10q", what, &l.cells[2][row]);
        snprintf(what, sizeof what, "slot %d name", row); layout_line("10q", what, &l.hotkey_names[row]);
        if (row < 4) { snprintf(what, sizeof what, "slot %d button", row); layout_box("10q", what, &l.cells[3][row]); }
    }
    layout_line("10q", "Empty", &l.items[0]);
    /* Back to the pause menu, on Controls. */
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(nav.screen == ORACLES_SCREEN_PAUSE && oracles_home_focus(&nav) == 3);
}

/* The hints of Controls in 4:3, against a frame's. */
static void hints_4_3(const char *frame, const OraclesHomeNav *nav, unsigned expected)
{
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    OraclesUiHintLayout h[ORACLES_HOME_MAX_HINTS];
    const unsigned count = oracles_home_hints(nav, hints);
    CHECK(count == expected);
    if (count != expected) return;
    oracles_ui_layout_hints(ORACLES_UI_LAYOUT_4_3, hints, count, h);
    for (unsigned i = 0; i < count; i++) {
        char what[32];
        snprintf(what, sizeof what, "hint %u", i); layout_box(frame, what, &h[i].box);
        snprintf(what, sizeof what, "hint %u key text", i); layout_line(frame, what, &h[i].key);
        snprintf(what, sizeof what, "hint %u label", i); layout_line(frame, what, &h[i].label);
    }
}

/* The 4:3 layout against the mockup's frames: Buttons (10u) and A's key waiting (10i), Hotkeys (10j), In game (10k). */
static void layout_4_3(void)
{
    OraclesHomeNav nav;
    controls_nav(&nav);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    snprintf(nav.controls.items[0][0], sizeof nav.controls.items[0][0], "Seed Shooter");
    OraclesUiControlsLayout l;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    /* 10u: the title and the tabs; the grid, each row 90 tall; Reset 7 pixels above the help bar. */
    layout_line("10u", "title", &l.title);
    for (int i = 0; i < ORACLES_CONTROLS_TABS; i++) {
        char what[32];
        snprintf(what, sizeof what, "tab %d", i); layout_box("10u", what, &l.tabs[i]);
        snprintf(what, sizeof what, "tab %d label", i); layout_line("10u", what, &l.tab_labels[i]);
    }
    layout_line("10u", "Game buttons", &l.heading_buttons);
    layout_line("10u", "Keyboard", &l.heading_keyboard);
    layout_line("10u", "Gamepad", &l.heading_gamepad);
    for (int row = 0; row < 8; row++) {
        char what[32];
        snprintf(what, sizeof what, "row %d key", row); layout_box("10u", what, &l.cells[0][row]);
        snprintf(what, sizeof what, "row %d button", row); layout_box("10u", what, &l.cells[1][row]);
        snprintf(what, sizeof what, "row %d name", row); layout_line("10u", what, &l.button_names[row]);
    }
    layout_line("10u", "A key text", &l.cell_texts[0][4]);
    layout_line("10u", "Z", &l.cell_texts[0][5]);
    layout_line("10u", "Backspace", &l.cell_texts[0][6]);
    layout_line("10u", "d-pad", &l.cell_texts[1][0]);
    layout_box("10u", "Reset", &l.reset);
    layout_line("10u", "Reset to defaults", &l.reset_text);
    layout_line("10u", "Reset dot", &l.reset_dot);
    hints_4_3("10u", &nav, 3);
    OraclesUiHintLayout bar[ORACLES_HOME_MAX_HINTS];
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    oracles_ui_layout_hints(ORACLES_UI_LAYOUT_4_3, hints, oracles_home_hints(&nav, hints), bar);
    CHECK(bar[0].box.y - (l.reset.y + l.reset.h) == 7.0f);

    /* 10i: A's key waits, its border within its 90, the rows unmoved; the prompt in the tabs' place. */
    nav.controls.row = 4;
    oracles_home_act(&nav, ORACLES_HOME_OK);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    layout_line("10i", "prompt", &l.prompt);
    layout_box("10i", "row 4 key", &l.cells[0][4]);
    layout_line("10i", "Press a key", &l.cell_texts[0][4]);
    layout_box("10i", "row 5 key", &l.cells[0][5]);
    layout_box("10i", "Reset", &l.reset);
    hints_4_3("10i", &nav, 1);
    oracles_controls_cancel(&nav);

    /* 10j: Hotkeys, the Item hotkeys line, Off and On at its right, its texts; the slots, Bind to B 8 lower. */
    oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_TAB, ORACLES_CONTROLS_TAB_HOTKEYS);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    CHECK(l.cells[0][0].w == 0.0f && l.reset.w == 0.0f && l.slots_note.w == 0.0f && l.mode_fan.h == 0.0f && l.mode_later.h == 0.0f);
    for (int i = 0; i < ORACLES_CONTROLS_TABS; i++) {
        char what[32];
        snprintf(what, sizeof what, "tab %d", i); layout_box("10j", what, &l.tabs[i]);
    }
    layout_box("10j", "Item hotkeys", &l.mode);
    layout_line("10j", "Item hotkeys title", &l.mode_title);
    layout_box("10j", "Off", &l.mode_choices[0].box);
    layout_box("10j", "On", &l.mode_choices[1].box);
    layout_line("10j", "Off text", &l.mode_choices[0].name);
    layout_line("10j", "On text", &l.mode_choices[1].name);
    layout_line("10j", "explanation", &l.mode_text);
    layout_line("10j", "note", &l.mode_note);
    layout_line("10j", "Hotkeys", &l.heading_hotkeys);
    layout_line("10j", "Keyboard", &l.heading_hotkeys_keyboard);
    layout_line("10j", "Gamepad", &l.heading_hotkeys_gamepad);
    layout_line("10j", "In Ages", &l.heading_items);
    for (int row = 0; row < 6; row++) {
        char what[32];
        snprintf(what, sizeof what, "slot %d key", row); layout_box("10j", what, &l.cells[2][row]);
        snprintf(what, sizeof what, "slot %d name", row); layout_line("10j", what, &l.hotkey_names[row]);
        if (row < 4) { snprintf(what, sizeof what, "slot %d button", row); layout_box("10j", what, &l.cells[3][row]); }
    }
    CHECK(l.cells[3][4].w == 0.0f && l.cells[3][5].w == 0.0f);
    layout_line("10j", "Seed Shooter", &l.items[0]);
    layout_line("10j", "Empty", &l.items[3]);
    hints_4_3("10j", &nav, 3);

    /* 10k: In game, the fixed keys framed and their meanings. */
    oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_TAB, ORACLES_CONTROLS_TAB_IN_GAME);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    CHECK(l.mode.w == 0.0f && l.cells[2][0].w == 0.0f && l.cells[0][0].w == 0.0f);
    layout_line("10k", "title", &l.shortcuts_label);
    for (int i = 0; i < 6; i++) {
        char what[32];
        snprintf(what, sizeof what, "key %d", i); layout_box("10k", what, &l.shortcut_keys[i]);
        snprintf(what, sizeof what, "key %d text", i); layout_line("10k", what, &l.shortcut_key_texts[i]);
        snprintf(what, sizeof what, "key %d label", i); layout_line("10k", what, &l.shortcut_labels[i]);
    }
    swim_probes("10k", &l);

    /* 17c: Swimming on Hold, highlighted, under the fixed keys; 17d the same at 640x480, its hints too. */
    nav.controls.swim_hold = 1;
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.controls.row == ORACLES_CONTROLS_ROW_SWIM);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    static const char *const swim_frames[2] = { "17c", "17d" };
    for (int f = 0; f < 2; f++) {
        swim_probes(swim_frames[f], &l);
        layout_box(swim_frames[f], "key 5", &l.shortcut_keys[5]);
        layout_line(swim_frames[f], "key 5 label", &l.shortcut_labels[5]);
        for (int i = 0; i < ORACLES_CONTROLS_TABS; i++) {
            char what[32];
            snprintf(what, sizeof what, "tab %d", i); layout_box(swim_frames[f], what, &l.tabs[i]);
        }
        hints_4_3(swim_frames[f], &nav, 3);
    }
    CHECK(l.swim_fan.h == 0.0f);

    /* 17e (turn 27's 27b): from an Oracle's pause menu, Swimming applies to the game: no line under it. */
    nav.in_game = 1;
    nav.controls.swim_hold = 0;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    swim_probes("17e", &l);
    CHECK(l.swim_later.h == 0.0f);
    layout_box("17e", "tab 2", &l.tabs[2]);
}

/* Menu's controller button against a frame's: Esc's frame and text, the cell and its text, Menu, and the other fixed
 * keys, F2 to F11. */
static void menu_probes(const char *frame, const OraclesUiControlsLayout *l)
{
    layout_box(frame, "menu key", &l->shortcut_keys[5]);
    layout_line(frame, "menu key text", &l->shortcut_key_texts[5]);
    layout_box(frame, "menu pad", &l->menu_pad);
    layout_line(frame, "menu pad text", &l->menu_pad_text);
    layout_line(frame, "menu label", &l->shortcut_labels[5]);
}

static void fixed_keys(const char *frame, const OraclesUiControlsLayout *l)
{
    for (int i = 0; i < 5; i++) {
        char what[32];
        snprintf(what, sizeof what, "key %d", i); layout_box(frame, what, &l->shortcut_keys[i]);
        snprintf(what, sizeof what, "key %d text", i); layout_line(frame, what, &l->shortcut_key_texts[i]);
        snprintf(what, sizeof what, "key %d label", i); layout_line(frame, what, &l->shortcut_labels[i]);
    }
}

/* Menu's controller button (turn 18): reached under the hotkeys' rows, beside Start's; it waits for a button the game
 * and the hotkeys leave free, or a trigger, and a cell that takes its button leaves it without one; Reset gives L3
 * back; its toast names it.  In 16:9 (18a, its capture) and in 4:3's In game tab (18b, 18c, its capture). */
static void menu(void)
{
    OraclesHomeNav nav;
    controls_nav(&nav);
    OraclesHomeControls *c = &nav.controls;
    CHECK(!strcmp(c->menu_pad, "leftstick") && !strcmp(oracles_controls_cell_text(&nav, 2, ORACLES_CONTROLS_ROW_MENU), "L3"));
    CHECK(!strcmp(oracles_controls_cell_text(&nav, 1, 4), "A") && !strcmp(oracles_controls_cell_text(&nav, 3, 2), "LB"));
    CHECK(!strcmp(oracles_controls_button_label("righttrigger"), "RT") && !strcmp(oracles_controls_button_label("misc1"), "misc1"));
    /* Right from Start's button, down from the last slot's: Menu's; up to Bind to A's key, left back to Start's. */
    c->column = 1; c->row = 7;
    for (int i = 0; i < 2; i++) { oracles_home_act(&nav, ORACLES_HOME_RIGHT); CHECK(c->column == 2 && c->row == ORACLES_CONTROLS_ROW_MENU); }
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(c->column == 2 && c->row == 5);
    c->column = 3; c->row = 3; oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->column == 2 && c->row == ORACLES_CONTROLS_ROW_MENU);
    oracles_home_act(&nav, ORACLES_HOME_LEFT);
    CHECK(c->column == 1 && c->row == 7);
    /* OK waits; the game's buttons, the hotkeys', the d-pad, Guide and a key do not bind there. */
    oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_MENU, -1);
    CHECK(c->capturing && !strcmp(oracles_controls_prompt(&nav), "Press a gamepad button \xc2\xb7 Esc cancels"));
    CHECK(!strcmp(oracles_controls_cell_text(&nav, 2, ORACLES_CONTROLS_ROW_MENU), "Press\xe2\x80\xa6"));
    static const char *const taken[] = { "a", "back", "x", "rightshoulder", "dpdown", "guide" };
    for (size_t i = 0; i < sizeof taken / sizeof taken[0]; i++) CHECK(oracles_controls_capture_button(&nav, taken[i]) == ORACLES_HOME_STAY && c->capturing);
    CHECK(oracles_controls_capture_key(&nav, "M") == ORACLES_HOME_STAY && c->capturing);
    CHECK(!oracles_controls_menu_free(c, "b") && oracles_controls_menu_free(c, "lefttrigger") && oracles_controls_menu_free(c, "rightstick"));
    CHECK(oracles_controls_capture_button(&nav, "righttrigger") == ORACLES_HOME_STORE && !c->capturing && !strcmp(c->menu_pad, "righttrigger"));
    CHECK(!strcmp(oracles_controls_cell_text(&nav, 2, ORACLES_CONTROLS_ROW_MENU), "RT"));
    char tip[64];
    oracles_controls_menu_tip(ORACLES_INPUT_GAMEPAD, 0, c->menu_pad, tip, sizeof tip);
    CHECK(!strcmp(tip, "RT opens the menu"));
    /* None: Guide alone, which the reminder names (Back on Android). */
    c->menu_pad[0] = 0;
    CHECK(!strcmp(oracles_controls_cell_text(&nav, 2, ORACLES_CONTROLS_ROW_MENU), "\xe2\x80\x94"));
    oracles_controls_menu_tip(ORACLES_INPUT_GAMEPAD, 0, c->menu_pad, tip, sizeof tip);
    CHECK(!strcmp(tip, "Guide opens the menu"));
    oracles_controls_menu_tip(ORACLES_INPUT_GAMEPAD, 1, c->menu_pad, tip, sizeof tip);
    CHECK(!strcmp(tip, "Back opens the menu"));
    /* Reset to defaults: L3 again, and the reminder and the pause menu's line due again. */
    c->menu_tip = c->hotkeys_tip = ORACLES_TIP_OFF;
    CHECK(oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_RESET, -1) == ORACLES_HOME_STORE && !strcmp(c->menu_pad, "leftstick"));
    CHECK(c->menu_tip == 0 && c->hotkeys_tip == 0);
    oracles_controls_menu_tip(ORACLES_INPUT_GAMEPAD, 0, c->menu_pad, tip, sizeof tip);
    CHECK(!strcmp(tip, "L3 opens the menu"));

    /* 18a: Menu's button highlighted, first of the fixed keys under the item hotkeys' panel. */
    OraclesUiControlsLayout l;
    c->column = 2; c->row = ORACLES_CONTROLS_ROW_MENU;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    menu_probes("18a", &l);
    fixed_keys("18a", &l);
    layout_line("18a", "In game", &l.shortcuts_label);
    layout_box("18a", "right panel", &l.right_panel);
    /* Its button waiting: the cell wider, the keys after it moved. */
    oracles_home_act(&nav, ORACLES_HOME_OK);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    menu_probes("menu capture", &l);
    fixed_keys("menu capture", &l);
    layout_line("menu capture", "prompt", &l.prompt);
    layout_line("menu capture", "In game", &l.shortcuts_label);
    oracles_controls_cancel(&nav);

    /* 18b and 18c: 4:3's In game tab, Menu's row first, before the other keys and Swimming. */
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_TAB, ORACLES_CONTROLS_TAB_IN_GAME);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->column == 0 && c->row == ORACLES_CONTROLS_ROW_MENU && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_IN_GAME);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    static const char *const frames[2] = { "18b", "18c" };
    for (int f = 0; f < 2; f++) {
        menu_probes(frames[f], &l);
        fixed_keys(frames[f], &l);
        swim_probes(frames[f], &l);
        layout_line(frames[f], "title", &l.shortcuts_label);
        for (int i = 0; i < ORACLES_CONTROLS_TABS; i++) {
            char what[32];
            snprintf(what, sizeof what, "tab %d", i); layout_box(frames[f], what, &l.tabs[i]);
            snprintf(what, sizeof what, "tab %d label", i); layout_line(frames[f], what, &l.tab_labels[i]);
        }
        hints_4_3(frames[f], &nav, 3);
    }
    oracles_home_act(&nav, ORACLES_HOME_OK);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    menu_probes("menu capture 4:3", &l);
    layout_line("menu capture 4:3", "prompt", &l.prompt);
    layout_box("menu capture 4:3", "key 0", &l.shortcut_keys[0]);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && !c->capturing);
}

/* Touch vibration against a frame's: its box, its title, On and Off, its explanation, in 4:3 its note. */
static void vibration_probes(const char *frame, const OraclesUiControlsLayout *l, int four_three)
{
    layout_box(frame, "vibration", &l->vibration);
    layout_line(frame, "vibration label", &l->vibration_title);
    layout_box(frame, "vibration on", &l->vibration_choices[0].box);
    layout_box(frame, "vibration off", &l->vibration_choices[1].box);
    layout_line(frame, "vibration explanation", &l->vibration_text);
    if (four_three) layout_line(frame, "vibration note", &l->vibration_note);
}

/* Touch vibration, on Android only.  16:9 (vibration, after a touch): under the item hotkeys' panel, the In game line
 * beside the title, Menu's button then above the Item hotkeys line, Touch vibration under Bind to A.  4:3's In game tab
 * (vibration 640x480, with a controller): under Swimming, 8 closer to Menu's row; after a key, the fixed keys under it
 * (vibration 640x480 keys).  Left, right and OK turn it, a click on On or Off sets it, each stored; Reset to defaults
 * turns it on again; from the pause too.  None on a desktop. */
static void vibration(void)
{
    OraclesHomeNav nav;
    controls_nav(&nav);
    OraclesHomeControls *c = &nav.controls;
    OraclesUiControlsLayout l;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(c->touch_vibration == 1 && l.vibration.w == 0.0f && l.vibration_choices[0].box.w == 0.0f);
    c->column = 2; c->row = 5;
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->row == ORACLES_CONTROLS_ROW_MENU);
    CHECK(oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_VIBRATION, 1) == ORACLES_HOME_STAY && c->touch_vibration == 1);

    controls_nav(&nav);
    nav.android = 1;
    oracles_home_input(&nav, ORACLES_INPUT_POINTER);
    c->column = 2; c->row = ORACLES_CONTROLS_ROW_MODE;
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(c->column == 2 && c->row == ORACLES_CONTROLS_ROW_MENU);
    c->column = 2; c->row = 5;
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->column == 2 && c->row == ORACLES_CONTROLS_ROW_VIBRATION);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    vibration_probes("vibration", &l, 0);
    layout_near("vibration", "In game row", "y", l.menu_pad.y);
    layout_box("vibration", "menu pad", &l.menu_pad);
    layout_line("vibration", "In game", &l.shortcuts_label);
    layout_box("vibration", "right panel", &l.right_panel);
    layout_line("vibration", "title", &l.title);
    CHECK(l.vibration.y >= l.right_panel.y + l.right_panel.h && l.shortcuts_label.y + l.shortcuts_label.h <= l.right_panel.y);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && c->touch_vibration == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && c->touch_vibration == 1);
    CHECK(oracles_controls_click(&nav, 3, ORACLES_CONTROLS_ROW_VIBRATION, 1) == ORACLES_HOME_STORE && c->touch_vibration == 0 && c->column == 2);
    CHECK(oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_VIBRATION, 1) == ORACLES_HOME_STAY);
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(c->column == 2 && c->row == 5);
    CHECK(oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_RESET, -1) == ORACLES_HOME_STORE && c->touch_vibration == 1);
    /* From the pause, at once. */
    nav.in_game = 1;
    CHECK(oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_VIBRATION, 1) == ORACLES_HOME_STORE && c->touch_vibration == 0);
    nav.in_game = 0;
    /* Under the panel whatever lines it has: the item hotkeys off, a fan game's line, the reason from its pause. */
    nav.games[0].hotkeys = 0;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(l.hotkeys_off.h > 0.0f && l.vibration.y >= l.right_panel.y + l.right_panel.h);
    nav.entry = ORACLES_HOME_FAN;
    nav.fan_pick = ORACLES_HOME_HERO_MOONRISE;
    for (int paused = 0; paused < 2; paused++) {
        nav.in_game = paused;
        oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
        CHECK((paused ? l.mode_later.h : l.mode_fan.h) > 0.0f && l.vibration.y >= l.right_panel.y + l.right_panel.h);
    }
    nav.in_game = 0;

    /* 4:3's In game tab: Menu's button, Swimming, Touch vibration. */
    controls_nav(&nav);
    nav.android = 1;
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_TAB, ORACLES_CONTROLS_TAB_IN_GAME);
    for (int i = 0; i < 3; i++) oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->column == 0 && c->row == ORACLES_CONTROLS_ROW_VIBRATION && oracles_controls_tab(&nav) == ORACLES_CONTROLS_TAB_IN_GAME);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(c->row == ORACLES_CONTROLS_ROW_VIBRATION);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    vibration_probes("vibration 640x480", &l, 1);
    swim_probes("vibration 640x480", &l);
    layout_near("vibration 640x480", "list", "h", l.vibration.y + l.vibration.h - l.shortcuts_label.y);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    vibration_probes("vibration 640x480 keys", &l, 1);
    for (int i = 0; i < 4; i++) {
        char what[32];
        snprintf(what, sizeof what, "fixed %d", i);
        layout_near("vibration 640x480 keys", what, "y", l.shortcut_keys[i].y + l.shortcut_keys[i].h * 0.5f - 36.0f);
    }
    CHECK(l.shortcut_keys[0].y > l.vibration.y + l.vibration.h && l.shortcut_keys[4].w == 0.0f);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && c->touch_vibration == 0);
    CHECK(oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_VIBRATION, 0) == ORACLES_HOME_STORE && c->touch_vibration == 1);
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(c->row == ORACLES_CONTROLS_ROW_SWIM);
    /* A desktop's In game tab has none. */
    nav.android = 0;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    CHECK(l.vibration.w == 0.0f && l.shortcut_keys[0].y < l.swim.y);
}

/* Hotkeys with Ages' item hotkeys off (37i, 37i 4:3): under the grid, dimmed, the sentence that says what On does,
 * the right panel growing by it in 16:9; on, neither (37j, 37j 4:3).  Never for a fan game, nor where they are refused. */
static void hotkeys_off(void)
{
    OraclesHomeNav nav;
    controls_nav(&nav);
    nav.games[0].hotkeys = 0;
    nav.controls.column = 2;
    nav.controls.row = ORACLES_CONTROLS_ROW_MODE;
    OraclesUiControlsLayout l;
    for (int on = 0; on < 2; on++) {
        const char *frame = on ? "37j" : "37i", *frame_4_3 = on ? "37j 4:3" : "37i 4:3";
        nav.games[0].hotkeys = on;
        CHECK(oracles_controls_hotkeys_off_line(&nav) == !on);
        oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
        layout_box(frame, "right panel", &l.right_panel);
        layout_box(frame, "Item hotkeys", &l.mode);
        layout_line_top(frame, "note", &l.mode_note);
        for (int row = 0; row < 6; row++) {
            char what[32];
            snprintf(what, sizeof what, "slot %d key", row); layout_box(frame, what, &l.cells[2][row]);
        }
        layout_line_top(frame, "slots note", &l.slots_note);
        if (!on) {
            layout_line(frame, "off text", &l.hotkeys_off);
            layout_near(frame, "off line", "y", l.hotkeys_off.y);
        } else CHECK(l.hotkeys_off.h == 0.0f);
        nav.controls.tab = ORACLES_CONTROLS_TAB_HOTKEYS;
        oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
        layout_box(frame_4_3, "Item hotkeys", &l.mode);
        layout_line(frame_4_3, "note", &l.mode_note);
        for (int row = 0; row < 6; row++) {
            char what[32];
            snprintf(what, sizeof what, "slot %d key", row); layout_box(frame_4_3, what, &l.cells[2][row]);
            snprintf(what, sizeof what, "slot %d name", row); layout_line(frame_4_3, what, &l.hotkey_names[row]);
        }
        if (!on) {
            layout_line(frame_4_3, "off text", &l.hotkeys_off);
            layout_near(frame_4_3, "off line", "y", l.hotkeys_off.y - 10.0f);
        } else CHECK(l.hotkeys_off.h == 0.0f);
        nav.controls.tab = ORACLES_CONTROLS_TAB_BUTTONS;
    }
    nav.games[0].hotkeys = 0;
    nav.hotkeys_refused = 1;   /* an unrecognised ROM's pause: they cannot be turned on */
    CHECK(!oracles_controls_hotkeys_off_line(&nav));
    nav.hotkeys_refused = 0;
    oracles_home_select(&nav, ORACLES_HOME_FAN);
    nav.fan_pick = ORACLES_HOME_HERO_MOONRISE;
    CHECK(!oracles_controls_hotkeys_off_line(&nav));
}

/* Oracles Combined's hotkeys against a frame's, 16:9's: their line, the grid, the item column and its three items. */
static void combined_probes(const char *frame, const OraclesUiControlsLayout *l, int layout_4_3)
{
    layout_box(frame, "Item hotkeys", &l->mode);
    layout_box(frame, "Off", &l->mode_choices[0].box);
    layout_box(frame, "On", &l->mode_choices[1].box);
    layout_line(frame, "Item hotkeys title", &l->mode_title);
    layout_line(frame, "explanation", &l->mode_text);
    layout_line(frame, "note", &l->mode_note);
    layout_line(frame, "Hotkeys", &l->heading_hotkeys);
    layout_line(frame, layout_4_3 ? "In Oracles Combined" : "In Combined", &l->heading_items);
    for (int row = 0; row < 6; row++) {
        char what[32];
        snprintf(what, sizeof what, "slot %d key", row); layout_box(frame, what, &l->cells[2][row]);
        if (row < 4) { snprintf(what, sizeof what, "slot %d button", row); layout_box(frame, what, &l->cells[3][row]); }
        if (layout_4_3) { snprintf(what, sizeof what, "slot %d name", row); layout_line(frame, what, &l->hotkey_names[row]); }
    }
    for (int i = 0; i < 3; i++) {
        char what[32];
        snprintf(what, sizeof what, "item %d", i); layout_line(frame, what, &l->items[i]);
    }
    layout_line(frame, "Empty", &l->items[3]);
    if (!layout_4_3) layout_line(frame, "slots note", &l->slots_note);
    CHECK(l->mode_fan.h == 0.0f && !l->mode_fan_text[0] && l->mode_later.h == 0.0f);
}

/* Oracles Combined's Controls (42a to 42c): its item hotkeys as Ages' and Seasons', one mode and one set of slots for
 * both halves, its items from both, the grid live while on (42a, 42a 640x480) and dimmed with the line that says what
 * On does while off (42b, 42b 640x480); the item column "In Combined" in 16:9, where "In Oracles Combined" would take two
 * lines, the whole name in 4:3; no line that they stay off.  From the pause, Item hotkeys and Swimming live, without a
 * reason (42c, 42c 640x480). */
static void combined_hotkeys(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    fan_profiles(&nav);
    nav.games[ORACLES_HOME_GAME_COMBINED].refuses = 0;   /* its profile's, as the host reads it for its release */
    nav.games[ORACLES_HOME_GAME_COMBINED].usable = 1;
    nav.entry = ORACLES_HOME_FAN;
    nav.fan_pick = ORACLES_HOME_HERO_COMBINED;
    oracles_controls_open(&nav);
    static const char *const items[3] = { "Seed Shooter", "Rod of Seasons", "Switch Hook" };
    for (int i = 0; i < 3; i++) snprintf(nav.controls.items[2][i], sizeof nav.controls.items[2][i], "%s", items[i]);
    CHECK(!strcmp(oracles_controls_item(&nav, 1), "Rod of Seasons"));
    CHECK(!strcmp(oracles_controls_item_heading(&nav, ORACLES_UI_LAYOUT_16_9), "In Combined"));
    CHECK(!strcmp(oracles_controls_item_heading(&nav, ORACLES_UI_LAYOUT_4_3), "In Oracles Combined"));
    nav.controls.column = 2;
    nav.controls.row = ORACLES_CONTROLS_ROW_MODE;
    OraclesUiControlsLayout l;
    for (int on = 1; on >= 0; on--) {
        const char *frame = on ? "42a" : "42b", *frame_4_3 = on ? "42a 640x480" : "42b 640x480";
        nav.games[ORACLES_HOME_GAME_COMBINED].hotkeys = on;
        CHECK(oracles_controls_hotkeys_on(&nav) == on && oracles_controls_hotkeys_off_line(&nav) == !on);
        nav.android = 0;
        oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
        oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
        combined_probes(frame, &l, 0);
        layout_box(frame, "right panel", &l.right_panel);
        if (!on) layout_line(frame, "off text", &l.hotkeys_off);
        else CHECK(l.hotkeys_off.h == 0.0f);
        nav.android = 1;
        oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
        nav.controls.tab = ORACLES_CONTROLS_TAB_HOTKEYS;
        oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
        combined_probes(frame_4_3, &l, 1);
        layout_box(frame_4_3, "tab 1", &l.tabs[1]);
        if (!on) layout_line(frame_4_3, "off text", &l.hotkeys_off);
        nav.controls.tab = ORACLES_CONTROLS_TAB_BUTTONS;
    }
    /* Left and right turn them on and off, each stored, as Ages'. */
    nav.android = 0;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.games[ORACLES_HOME_GAME_COMBINED].hotkeys == 1);
    /* From the pause: live, no reason, Swimming too. */
    nav.in_game = 1;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    combined_probes("42c", &l, 0);
    swim_probes("42c", &l);
    CHECK(l.swim_later.h == 0.0f && !l.swim_later_text);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.games[ORACLES_HOME_GAME_COMBINED].hotkeys == 0);
    nav.android = 1;
    nav.controls.tab = ORACLES_CONTROLS_TAB_HOTKEYS;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    combined_probes("42c 640x480", &l, 1);
    layout_line("42c 640x480", "off text", &l.hotkeys_off);
    /* Its In game tab from the pause: Swimming live, without a line (42e combined 640x480). */
    nav.controls.tab = ORACLES_CONTROLS_TAB_IN_GAME;
    nav.controls.row = ORACLES_CONTROLS_ROW_SWIM;
    nav.controls.column = 0;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    swim_probes("42e combined 640x480", &l);
    CHECK(l.swim_fan.h == 0.0f && l.swim_later.h == 0.0f);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.controls.swim_hold == 1);
}

/* One reason for what a game's profile lacks, "Not available for this game.": Moonrise Regalia's line under Item
 * hotkeys from the home screen (42d), Swimming's and Item hotkeys' from its pause (42d pause), Swimming's in its 4:3 In
 * game tab (42d 640x480); from the pause of Gifts of Kinomi and Temple of Seasons, Item hotkeys alone, Swimming live (42e
 * kinomi, 42e temple). */
static void not_available(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    fan_profiles(&nav);
    nav.entry = ORACLES_HOME_FAN;
    nav.fan_pick = ORACLES_HOME_HERO_MOONRISE;
    nav.games[ORACLES_HOME_GAME_MOONRISE].usable = 1;
    oracles_controls_open(&nav);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    OraclesUiControlsLayout l;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(!strcmp(l.mode_fan_text, oracles_display_fan_reason));
    layout_box("42d", "Item hotkeys", &l.mode);
    layout_line("42d", "note", &l.mode_note);
    layout_line("42d", "fan", &l.mode_fan);
    layout_line("42d", "Hotkeys", &l.heading_hotkeys);
    layout_box("42d", "slot 0 key", &l.cells[2][0]);
    layout_box("42d", "right panel", &l.right_panel);
    nav.android = 1;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    nav.controls.tab = ORACLES_CONTROLS_TAB_IN_GAME;
    nav.controls.row = ORACLES_CONTROLS_ROW_SWIM;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    swim_probes("42d 640x480", &l);
    layout_line("42d 640x480", "swim fan", &l.swim_fan);
    nav.android = 0;
    nav.in_game = 1;
    nav.controls.row = 0;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    swim_probes("42d pause", &l);
    CHECK(l.swim_later_text == oracles_display_fan_reason && l.mode_later_text == oracles_display_fan_reason && !l.mode_fan_text[0]);
    layout_line("42d pause", "swim later", &l.swim_later);
    layout_box("42d pause", "Item hotkeys", &l.mode);
    layout_line("42d pause", "later", &l.mode_later);
    layout_line("42d pause", "Hotkeys", &l.heading_hotkeys);
    layout_box("42d pause", "slot 0 key", &l.cells[2][0]);
    static const OraclesHomeHero heroes[2] = { ORACLES_HOME_HERO_KINOMI, ORACLES_HOME_HERO_TEMPLE };
    static const char *const frames[2] = { "42e kinomi", "42e temple" };
    for (int f = 0; f < 2; f++) {
        nav.fan_pick = heroes[f];
        oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
        swim_probes(frames[f], &l);
        CHECK(l.swim_later.h == 0.0f && l.mode_later_text == oracles_display_fan_reason);
        layout_box(frames[f], "Item hotkeys", &l.mode);
        layout_line(frames[f], "later", &l.mode_later);
        layout_line(frames[f], "Hotkeys", &l.heading_hotkeys);
        layout_box(frames[f], "slot 0 key", &l.cells[2][0]);
    }
}

/* Swimming from the home screen in 16:9 (47a): Moonrise Regalia's, whose profile has no swim's hold, says "Not available
 * for this game." under its explanation in the warning's tone, in the room of the block's own spacing, and stays live,
 * one setting for every game, and a click on the line lands on Swimming; the left panel still ends above the hints.
 * Gifts of Kinomi's, which keeps the hold, without the line (47a kinomi), nor Temple of Seasons' and Oracles Combined's;
 * from Moonrise Regalia's pause, its reason alone, as before. */
static int inside(const OraclesUiBox *b, float x, float y) { return b->w > 0.0f && x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h; }

/* Whether the pointer at (x, y) lands on Swimming's row itself (oracles_ui_controls_hit's order): in its box, on none
 * of the cells, Reset, Menu's cell, Tap or Hold, Touch vibration or the Item hotkeys line before it. */
static int swim_row_under(const OraclesUiControlsLayout *l, float x, float y)
{
    for (int c = 0; c < ORACLES_CONTROLS_COLUMNS; c++)
        for (int r = 0; r < ORACLES_HOME_BUTTONS; r++) if (inside(&l->cells[c][r], x, y)) return 0;
    if (inside(&l->reset, x, y) || inside(&l->menu_pad, x, y) || inside(&l->vibration, x, y) || inside(&l->mode, x, y)) return 0;
    for (int i = 0; i < 2; i++) if (inside(&l->swim_choices[i].box, x, y)) return 0;
    return inside(&l->swim, x, y);
}

static void swim_from_home(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    fan_profiles(&nav);
    nav.entry = ORACLES_HOME_FAN;
    nav.fan_pick = ORACLES_HOME_HERO_MOONRISE;
    nav.games[ORACLES_HOME_GAME_MOONRISE].usable = 1;
    nav.games[ORACLES_HOME_GAME_KINOMI].usable = 1;
    oracles_controls_open(&nav);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    OraclesUiControlsLayout l;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    swim_probes("42d", &l);
    layout_line("42d", "swim fan", &l.swim_fan);
    layout_box("42d", "left panel", &l.left_panel);
    nav.controls.row = ORACLES_CONTROLS_ROW_SWIM;
    nav.controls.column = 0;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    swim_probes("47a", &l);
    layout_line("47a", "swim fan", &l.swim_fan);
    layout_box("47a", "left panel", &l.left_panel);
    CHECK(l.swim_later.h == 0.0f && !l.swim_later_text);
    CHECK(l.left_panel.y + l.left_panel.h < layout_reference("47a", "hint 0", "y"));
    CHECK(!strcmp(l.mode_fan_text, oracles_display_fan_reason));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.controls.swim_hold == 1);
    CHECK(oracles_controls_click(&nav, 0, ORACLES_CONTROLS_ROW_SWIM, 0) == ORACLES_HOME_STORE && nav.controls.swim_hold == 0);
    CHECK(swim_row_under(&l, l.swim_fan.x + l.swim_fan.w * 0.5f, l.swim_fan.y + l.swim_fan.h * 0.5f));
    nav.fan_pick = ORACLES_HOME_HERO_KINOMI;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    swim_probes("47a kinomi", &l);
    layout_box("47a kinomi", "left panel", &l.left_panel);
    CHECK(l.swim_fan.h == 0.0f && l.swim_later.h == 0.0f);
    const float kinomi_bottom = l.left_panel.y + l.left_panel.h;
    nav.games[ORACLES_HOME_GAME_TEMPLE].usable = 1;
    nav.games[ORACLES_HOME_GAME_COMBINED].refuses = 0;
    nav.games[ORACLES_HOME_GAME_COMBINED].usable = 1;
    static const OraclesHomeHero others[2] = { ORACLES_HOME_HERO_TEMPLE, ORACLES_HOME_HERO_COMBINED };
    for (int f = 0; f < 2; f++) {
        nav.fan_pick = others[f];
        oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
        CHECK(l.swim_fan.h == 0.0f && l.swim_later.h == 0.0f && l.left_panel.y + l.left_panel.h == kinomi_bottom);
    }
    nav.fan_pick = ORACLES_HOME_HERO_MOONRISE;
    nav.in_game = 1;
    oracles_ui_layout_controls(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(l.swim_fan.h == 0.0f && l.swim_later_text == oracles_display_fan_reason);
    swim_probes("42d pause", &l);
    layout_line("42d pause", "swim later", &l.swim_later);
}

int main(int argc, char **argv)
{
    if (argc < 2 || !layout_reference_load(argv[1])) { fprintf(stderr, "usage: oracles-test-launcher-controls launcher_layout_reference.json\n"); return 2; }
    if (!oracles_ui_fonts_load()) { fprintf(stderr, "FAIL the embedded fonts do not load\n"); return 1; }
    capture();
    moves();
    swimming();
    menu();
    pause_menu();
    refused_rows();
    layout();
    tabs_4_3();
    capture_4_3();
    pause_4_3();
    layout_4_3();
    hotkeys_off();
    vibration();
    combined_hotkeys();
    not_available();
    swim_from_home();
    if (layout_failures()) { fprintf(stderr, "%d failure(s)\n", layout_failures()); return 1; }
    printf("launcher controls: the capture, the moves, Swimming, the 4:3 tabs and the pause menu behave as expected, and Controls matches the layout reference in both layouts within %.1f px\n", (double)LAYOUT_TOLERANCE);
    return 0;
}
