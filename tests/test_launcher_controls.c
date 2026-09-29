/* Controls and the pause menu (ui_controls_nav.h, ui_controls_layout.h,
 * ui_home_nav.h): the capture of a key or a button (its states, a key taken
 * from another cell, the cancel), the moves between cells, Reset, the Item
 * hotkeys' line; the pause menu and its reduced form under 960 pixels wide;
 * and Controls' layout against the layout reference,
 * tests/launcher_layout_reference.json (layout_reference.h), the test's
 * argument, in its frames 7g (at rest) and 2d (A's key waiting). */
#include "layout_reference.h"
#include "ui_controls_layout.h"
#include "ui_controls_nav.h"

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
    CHECK(!strcmp(oracles_controls_item_heading(&nav), "In Ages"));
}

static void pause_menu(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, 0);
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    static const char *const labels[6] = { "Resume", "Save state", "Load state", "Controls", "Display", "Quit to launcher" };
    CHECK(oracles_home_items(&nav, items) == 6);
    for (int i = 0; i < 6; i++) CHECK(!strcmp(items[i].label, labels[i]) && !items[i].disabled);
    CHECK(!strcmp(items[1].note, "F5") && !strcmp(items[2].note, "F7"));
    char state[ORACLES_HOME_STATE_LENGTH];
    oracles_home_state(&nav, ORACLES_HOME_HERO_AGES, state);
    CHECK(!strcmp(state, "Paused \xc2\xb7 Profile: Enhanced"));
    /* The entries' commands; Escape or B resumes; left and right do nothing. */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_RESUME);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_RESUME);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.entry == ORACLES_HOME_AGES);
    CHECK(oracles_home_click(&nav, 1) == ORACLES_HOME_SAVE_STATE && oracles_home_click(&nav, 2) == ORACLES_HOME_LOAD_STATE);
    CHECK(oracles_home_click(&nav, 5) == ORACLES_HOME_QUIT_GAME);
    /* Controls and Display from the pause come back to it, on their entry. */
    CHECK(oracles_home_click(&nav, 3) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_CONTROLS);
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(nav.screen == ORACLES_SCREEN_PAUSE && oracles_home_focus(&nav) == 3);
    CHECK(oracles_home_click(&nav, 4) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_DISPLAY);
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(nav.screen == ORACLES_SCREEN_PAUSE && oracles_home_focus(&nav) == 4);
    /* Under 960 pixels wide: Resume, Save state, Load state, Quit to launcher. */
    oracles_home_pause(&nav, ORACLES_HOME_GAME_SEASONS, ORACLES_PROFILE_FAITHFUL, 1);
    CHECK(oracles_home_items(&nav, items) == 4);
    CHECK(!strcmp(items[2].label, "Load state") && !strcmp(items[3].label, "Quit to launcher"));
    CHECK(oracles_home_click(&nav, 3) == ORACLES_HOME_QUIT_GAME);
    oracles_home_state(&nav, ORACLES_HOME_HERO_SEASONS, state);
    CHECK(!strcmp(state, "Paused \xc2\xb7 Profile: Faithful"));
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    CHECK(oracles_home_hints(&nav, hints) == 3 && !strcmp(hints[2].label, "Resume") && hints[2].back);
}

static void layout(void)
{
    OraclesHomeNav nav;
    controls_nav(&nav);
    snprintf(nav.controls.items[0][0], sizeof nav.controls.items[0][0], "Seed Shooter");
    OraclesUiControlsLayout l;
    oracles_ui_layout_controls(&nav, &l);
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
    layout_line_top("7g", "In game", &l.shortcuts_label);
    layout_box("7g", "F2", &l.shortcut_keys[0]);
    layout_box("7g", "F11", &l.shortcut_keys[4]);
    layout_line_top("7g", "Menu", &l.shortcut_labels[5]);

    /* 2d: A's key waits, its border 2 px, its row 2 px taller; the rows under it and Reset move down by 2. */
    nav.controls.row = 4;
    oracles_home_act(&nav, ORACLES_HOME_OK);
    oracles_ui_layout_controls(&nav, &l);
    layout_line_top("2d", "prompt", &l.prompt);
    layout_box("2d", "A key", &l.cells[0][4]);
    layout_box("2d", "A button", &l.cells[1][4]);
    layout_near("2d", "Press a key", "x", l.cell_texts[0][4].x);
    layout_box("2d", "B key", &l.cells[0][5]);
    layout_line_top("2d", "Reset to defaults", &l.reset_text);
    layout_box("2d", "left panel", &l.left_panel);
    /* In a game, the Item hotkeys' line says it waits for the next Play. */
    nav.in_game = 1;
    oracles_ui_layout_controls(&nav, &l);
    CHECK(l.mode_later.h > 0.0f && l.mode.h > 124.0f);
}

int main(int argc, char **argv)
{
    if (argc < 2 || !layout_reference_load(argv[1])) { fprintf(stderr, "usage: oracles-test-launcher-controls launcher_layout_reference.json\n"); return 2; }
    if (!oracles_ui_fonts_load()) { fprintf(stderr, "FAIL the embedded fonts do not load\n"); return 1; }
    capture();
    moves();
    pause_menu();
    layout();
    if (layout_failures()) { fprintf(stderr, "%d failure(s)\n", layout_failures()); return 1; }
    printf("launcher controls: the capture, the moves and the pause menu behave as expected, and Controls matches the layout reference within %.1f px\n", (double)LAYOUT_TOLERANCE);
    return 0;
}
