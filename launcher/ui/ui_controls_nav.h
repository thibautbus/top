/* Controls, without SDL: its cells, what
 * the keys and the pointer do there, and the capture of a key or a button.
 *
 * Two panels.  Game buttons: Right, Left, Up, Down, A, B,
 * Select, Start, a keyboard column (column 0) and a controller column
 * (column 1), whose four first rows are the d-pad and the left stick and do
 * not bind; Reset to defaults under them (row 8).  Item hotkeys: their line,
 * Off or On for the game shown (row -1), then the four slots and Bind to B,
 * Bind to A, a keyboard column (column 2, six rows) and a controller column
 * (column 3, the four slots), and each slot's item, read-only.
 *
 * The arrows move between cells: up and down in a
 * column, left and right to the nearest row of the next column; on the Item
 * hotkeys line they change it.  OK on a cell waits for a key (keyboard
 * columns) or a button (controller columns); Escape cancels the wait.  A key
 * or a button already held by another cell of the same device moves: that
 * cell is left without one ("—").  The fixed shortcuts (F2, F3, F5, F7, F11,
 * Escape) and the d-pad do not bind.
 *
 * In the 4:3 layout (nav->layout) the page is split in three tabs, Buttons
 * (columns 0 and 1, Reset), Hotkeys (the Item hotkeys line, columns 2 and 3)
 * and In game (the fixed shortcuts, read-only), whose strip is a row of its
 * own above the cells (row -2): left, right and OK there change the tab,
 * down enters it.  The arrows move among the shown tab's cells as above,
 * the strip being the nearest row of both columns.  The tab shown is the
 * highlighted cell's, or nav->controls.tab while the highlight is on the
 * strip.  A click on a tab shows it, the highlight on the strip.  When the
 * window turns 16:9 with the highlight on the strip, the next key but Back
 * takes it to its tab's first cell (Buttons' and In game's: Right's key;
 * Hotkeys': the Item hotkeys line). */
#ifndef ORACLES_UI_CONTROLS_NAV_H
#define ORACLES_UI_CONTROLS_NAV_H

#include "ui_home_nav.h"

#define ORACLES_CONTROLS_COLUMNS 4
#define ORACLES_CONTROLS_ROW_MODE (-1)     /* the Item hotkeys line, on columns 2 and 3 */
#define ORACLES_CONTROLS_ROW_RESET 8       /* Reset to defaults, on columns 0 and 1 */
#define ORACLES_CONTROLS_ROW_TAB (-2)      /* 4:3: the tabs' strip, on the shown tab's columns */

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
/* The fixed shortcuts in game, key and meaning, as the screen recalls them. */
extern const char *const oracles_controls_shortcuts[6][2];

/* The defaults' names, no item in any slot, the first cell highlighted. */
void oracles_controls_defaults(OraclesHomeControls *controls);
/* Opens Controls on its first cell, Buttons in 4:3. */
void oracles_controls_open(OraclesHomeNav *nav);
/* The tab shown in 4:3 (ORACLES_CONTROLS_TAB_*). */
int oracles_controls_tab(const OraclesHomeNav *nav);

/* Whether a cell exists, and whether it is one that does not bind (the d-pad's in the controller column). */
int oracles_controls_cell_exists(int column, int row);
int oracles_controls_cell_locked(int column, int row);
/* A cell's text: its key or button, "—" for none, "D-pad, left stick", or the capture's "Press a key…". */
const char *oracles_controls_cell_text(const OraclesHomeNav *nav, int column, int row);
/* The line over the panels while a cell waits: "Press a key · Esc cancels", empty otherwise. */
const char *oracles_controls_prompt(const OraclesHomeNav *nav);
/* The game shown's hotkeys: on (use, or equip from the file), and whether its grid is dimmed. */
int oracles_controls_hotkeys_on(const OraclesHomeNav *nav);
/* "In Ages", "In Seasons", a fan game's ("In Moonrise Regalia"): the item column's heading. */
const char *oracles_controls_item_heading(const OraclesHomeNav *nav);
/* The item in a slot of the game shown, empty when none (a fan game's are all empty). */
const char *oracles_controls_item(const OraclesHomeNav *nav, int slot);

OraclesHomeCommand oracles_controls_act(OraclesHomeNav *nav, OraclesHomeAction action);
/* The pointer: a cell clicked (it waits), Reset, the Item hotkeys line's Off (0) or On (1), a tab (row -2, its index
 * in `option`). */
void oracles_controls_hover(OraclesHomeNav *nav, int column, int row);
OraclesHomeCommand oracles_controls_click(OraclesHomeNav *nav, int column, int row, int option);

/* While a cell waits: a key by its SDL name, a controller button by its SDL name.  STORE when the cell took it;
 * STAY when it does not bind there (a button for a keyboard cell, a fixed shortcut, the d-pad).  Escape cancels. */
OraclesHomeCommand oracles_controls_capture_key(OraclesHomeNav *nav, const char *name);
OraclesHomeCommand oracles_controls_capture_button(OraclesHomeNav *nav, const char *name);
void oracles_controls_cancel(OraclesHomeNav *nav);

#endif
