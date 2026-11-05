/* Controls, without SDL: its cells, what the keys and the pointer do there,
 * and the capture of a key or a button.
 *
 * Two panels.  Game buttons: Right, Left, Up, Down, A, B, Select, Start, a
 * keyboard column (column 0) and a controller column (column 1), whose four
 * first rows are the d-pad and the left stick and do not bind; Reset to
 * defaults under them (row 8), then Swimming, Tap or Hold for every game
 * (row 9).  Item hotkeys: their line, Off or On for the game shown (row -1),
 * then the four slots and Bind to B, Bind to A, a keyboard column (column 2,
 * six rows) and a controller column (column 3, the four slots), and each
 * slot's item, read-only.  Under it, the fixed keys in game, Menu first: Esc,
 * fixed, and the controller's button that opens the menu too (row 10, one
 * place for columns 2 and 3, under their last rows).
 *
 * The arrows move between cells: up and down in a column, left and right to
 * the nearest row of the next column; on the Item hotkeys line and on
 * Swimming they change it.  OK on a cell waits for a key (keyboard columns)
 * or a button (controller columns, and Menu's); Escape cancels the wait.  A key or a
 * button already held by another cell of the same device moves: that cell is
 * left without one ("—"), Menu's too.  Menu takes only a button the game and
 * the item hotkeys leave free, or a trigger (LT, RT, L3 and R3 with the
 * defaults); B gives up waiting there, as Escape does.  The fixed shortcuts
 * (F2, F3, F5, F7, F11, Escape) and the d-pad do not bind.  A controller's
 * button shows its short name (oracles_controls_button_label).
 *
 * In the 4:3 layout (nav->layout) the page is split in three tabs, Buttons
 * (columns 0 and 1, Reset), Hotkeys (the Item hotkeys line, columns 2 and 3)
 * and In game (Menu's button, then the other fixed shortcuts, read-only, then
 * Swimming, all on column 0), whose strip
 * is a row of its own above the cells (row -2): left, right and OK there
 * change the tab, down enters it.  The arrows move among the shown tab's
 * cells as above, the strip being the nearest row of both columns.  The tab
 * shown is the highlighted cell's, or nav->controls.tab while the highlight
 * is on the strip.  A click on a tab shows it, the highlight on the strip.
 * When the window turns 16:9 with the highlight on the strip, the next key
 * but Back takes it to its tab's first cell (Buttons': Right's key;
 * Hotkeys': the Item hotkeys line; In game's: Menu's button). */
#ifndef ORACLES_UI_CONTROLS_NAV_H
#define ORACLES_UI_CONTROLS_NAV_H

#include "ui_home_nav.h"

#define ORACLES_CONTROLS_COLUMNS 4
#define ORACLES_CONTROLS_ROW_MODE (-1)     /* the Item hotkeys line, on columns 2 and 3 */
#define ORACLES_CONTROLS_ROW_RESET 8       /* Reset to defaults, on columns 0 and 1 */
#define ORACLES_CONTROLS_ROW_SWIM 9        /* Swimming, on columns 0 and 1: under Reset, in 4:3 In game's */
#define ORACLES_CONTROLS_ROW_TAB (-2)      /* 4:3: the tabs' strip, on the shown tab's columns */
#define ORACLES_CONTROLS_ROW_MENU 10       /* Menu's controller button: on columns 2 and 3, in 4:3 In game's on column 0 */

/* 4:3's tabs, in their order. */
enum { ORACLES_CONTROLS_TAB_BUTTONS, ORACLES_CONTROLS_TAB_HOTKEYS, ORACLES_CONTROLS_TAB_IN_GAME, ORACLES_CONTROLS_TABS };
extern const char *const oracles_controls_tab_names[ORACLES_CONTROLS_TABS];   /* "Buttons", "Hotkeys", "In game" */

extern const char *const oracles_controls_buttons[ORACLES_HOME_BUTTONS];         /* "Right" ... "Start" */
extern const char *const oracles_controls_hotkey_rows[ORACLES_HOME_HOTKEY_ROWS];  /* "Slot 1" ... "Bind to A" */
/* The defaults, as settings.txt writes them. */
extern const char *const oracles_controls_default_keys[ORACLES_HOME_BUTTONS];
extern const char *const oracles_controls_default_pads[ORACLES_HOME_PAD_BUTTONS];
extern const char *const oracles_controls_default_hotkey_keys[ORACLES_HOME_HOTKEY_ROWS];
extern const char *const oracles_controls_default_hotkey_pads[ORACLES_HOME_SLOTS];
extern const char *const oracles_controls_default_menu_pad;   /* the left stick's click, which the game does not use */
/* The fixed shortcuts in game, key and meaning, as the screen recalls them. */
extern const char *const oracles_controls_shortcuts[6][2];
/* Whether the screen recalls fixed shortcut `i` (Esc's is 5): the keys once one was pressed, or on a desktop until a
 * controller's input; on Android none until a key is pressed, and never F11 (always fullscreen there).  Menu's
 * controller cell always shows. */
int oracles_controls_shortcut_shown(const OraclesHomeNav *nav, int i);
/* Menu's meaning in 4:3's In game tab: "Menu", on Android "Menu · also Back". */
const char *oracles_controls_menu_label(const OraclesHomeNav *nav);

/* The defaults' names, no item in any slot, Swimming on Tap, the first cell highlighted. */
void oracles_controls_defaults(OraclesHomeControls *controls);
/* Opens Controls on its first cell, Buttons in 4:3. */
void oracles_controls_open(OraclesHomeNav *nav);
/* The tab shown in 4:3 (ORACLES_CONTROLS_TAB_*). */
int oracles_controls_tab(const OraclesHomeNav *nav);

/* Whether a cell exists, and whether it is one that does not bind (the d-pad's in the controller column). */
int oracles_controls_cell_exists(int column, int row);
int oracles_controls_cell_locked(int column, int row);
/* A cell's text: its key or button (a button's short name), "—" for none, "D-pad, left stick", or the capture's
 * "Press a key…" ("Press…" in Menu's small cell). */
const char *oracles_controls_cell_text(const OraclesHomeNav *nav, int column, int row);
/* A controller's button or trigger as settings.txt names it, by the name its controls show: "A", "B", "X", "Y",
 * "Back", "Start", "LB", "RB", "LT", "RT", "L3", "R3"; another name as it is. */
const char *oracles_controls_button_label(const char *name);
/* The reminder over a game started, naming the key that opens the menu on the device used last (`device`, an
 * OraclesInputDevice): "L3 opens the menu" on a controller, `menu_pad` (settings.txt's menu_pad=) by its short name,
 * "L3 or Back opens the menu" on Android (`android`), "Back opens the menu" there without a button, and "Guide opens
 * the menu" elsewhere without one; on Android, "The pause button opens the menu" by touch, "Back opens the menu" after a
 * key (a handheld's buttons that come as keys); "Esc opens the menu" otherwise. */
void oracles_controls_menu_tip(int device, int android, const char *menu_pad, char *out, size_t capacity);
/* The reminder and the pause menu's line on the item hotkeys, as settings.txt counts them (menu_tip=, hotkeys_tip=): the
 * games the reminder was shown over, the pause menus the line was shown in, or ORACLES_TIP_OFF once dismissed (the menu
 * opened by its key; the item hotkeys turned on) or shown as often as it is at most. */
#define ORACLES_TIP_OFF (-1)
#define ORACLES_MENU_TIPS 10
#define ORACLES_HOTKEYS_TIPS 3
/* The count after one more showing: ORACLES_TIP_OFF at the last. */
int oracles_controls_tip_shown(int count, int most);
/* The reminder is due over the next game started: shown fewer than ORACLES_MENU_TIPS times, not dismissed. */
int oracles_controls_menu_tip_due(int count);
/* The pause menu's line "Item hotkeys are off · Controls › Item hotkeys", due as the pause opens: Ages or Seasons (not a
 * fan game, nor a ROM whose profile has no item hotkeys), their hotkeys off, Controls in the menu (not narrow), shown in
 * fewer than ORACLES_HOTKEYS_TIPS pause menus. */
int oracles_controls_hotkeys_tip_due(const OraclesHomeNav *nav);
/* As the pause menu opens, `by_key` when the player opened it by its key: the reminder done for good then, and the line
 * on the item hotkeys shown (nav->hotkeys_line) when due, counted only then (the pause Android's background brings is
 * not).  1 when a count changed, for settings.txt. */
int oracles_controls_tips_opened(OraclesHomeNav *nav, int by_key);
/* That line shows under the pause menu's list: due as it opened (nav->hotkeys_line), the game's hotkeys still off and
 * Controls still in the menu. */
int oracles_controls_hotkeys_line(const OraclesHomeNav *nav);
/* The line over the panels while a cell waits: "Press a key · Esc cancels", a controller's cell after a controller's
 * input naming the button that gives up ("Press a gamepad button · L3 cancels"); empty otherwise. */
const char *oracles_controls_prompt(const OraclesHomeNav *nav);
/* The game shown's hotkeys: on (use, or equip from the file), and whether its grid is dimmed. */
int oracles_controls_hotkeys_on(const OraclesHomeNav *nav);
/* The item hotkeys of the Oracle shown are off, where they can be turned on (not a fan game's, nor refused): the
 * sentence under their grid says what On does. */
int oracles_controls_hotkeys_off_line(const OraclesHomeNav *nav);
/* "In Ages", "In Seasons", a fan game's ("In Moonrise Regalia"): the item column's heading. */
const char *oracles_controls_item_heading(const OraclesHomeNav *nav);
/* The item in a slot of the game shown, empty when none (a fan game's are all empty). */
const char *oracles_controls_item(const OraclesHomeNav *nav, int slot);

OraclesHomeCommand oracles_controls_act(OraclesHomeNav *nav, OraclesHomeAction action);
/* The pointer: a cell clicked (it waits), Reset, the Item hotkeys line's Off (0) or On (1), Swimming's Tap (0) or Hold
 * (1), a tab (row -2, its index in `option`). */
void oracles_controls_hover(OraclesHomeNav *nav, int column, int row);
OraclesHomeCommand oracles_controls_click(OraclesHomeNav *nav, int column, int row, int option);

/* While a cell waits: a key by its SDL name, a controller button (or, for Menu, a trigger: "lefttrigger",
 * "righttrigger") by its SDL name.  STORE when the cell took it; STAY when it does not bind there (a button for a
 * keyboard cell, a fixed shortcut, the d-pad, a button the game or the hotkeys have for Menu).  Escape cancels. */
OraclesHomeCommand oracles_controls_capture_key(OraclesHomeNav *nav, const char *name);
OraclesHomeCommand oracles_controls_capture_button(OraclesHomeNav *nav, const char *name);
void oracles_controls_cancel(OraclesHomeNav *nav);
/* A button Menu may take: one the game's buttons and the hotkeys' leave free, not the d-pad, not Guide (which opens the
 * menu anyway); a trigger too. */
int oracles_controls_menu_free(const OraclesHomeControls *controls, const char *name);

#endif
