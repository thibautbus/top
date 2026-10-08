/* Where Controls puts its pieces in the scene of a layout (ui_layout.h),
 * without SDL.  16:9: the title and the capture's prompt, the game buttons'
 * panel on the left, the item hotkeys' on the right, the fixed shortcuts
 * under them.  4:3: the title and the tabs, Buttons, Hotkeys and In game
 * (the prompt in their place during a capture), over the shown tab alone:
 * the game buttons' grid and Reset, the Item hotkeys line and its grid, or
 * the fixed shortcuts as a list; no panels, no slots' note. */
#ifndef ORACLES_UI_CONTROLS_LAYOUT_H
#define ORACLES_UI_CONTROLS_LAYOUT_H

#include "ui_controls_nav.h"
#include "ui_home_layout.h"

typedef struct OraclesUiChipLayout {
    OraclesUiBox box;
    OraclesUiLine name;
} OraclesUiChipLayout;

typedef struct OraclesUiControlsLayout {
    OraclesUiLine title, prompt;
    OraclesUiBox left_panel, right_panel;
    OraclesUiLine heading_buttons, heading_keyboard, heading_gamepad;
    OraclesUiLine button_names[ORACLES_HOME_BUTTONS];
    OraclesUiBox cells[ORACLES_CONTROLS_COLUMNS][ORACLES_HOME_BUTTONS];   /* by column and row; zero where none */
    OraclesUiLine cell_texts[ORACLES_CONTROLS_COLUMNS][ORACLES_HOME_BUTTONS];
    OraclesUiBox reset;
    OraclesUiLine reset_text, reset_dot;
    OraclesUiBox mode;                          /* the Item hotkeys line's box */
    OraclesUiLine mode_title, mode_text, mode_note, mode_later;
    OraclesUiChipLayout mode_choices[2];
    OraclesUiLine heading_hotkeys, heading_hotkeys_keyboard, heading_hotkeys_gamepad, heading_items;
    OraclesUiLine hotkey_names[ORACLES_HOME_HOTKEY_ROWS];
    OraclesUiLine items[ORACLES_HOME_SLOTS];
    OraclesUiLine slots_note;
    OraclesUiLine shortcuts_label;
    const char *shortcuts_text;                 /* the label's: "In game", 4:3's "Fixed keys in game" */
    OraclesUiBox shortcut_keys[6];
    OraclesUiLine shortcut_key_texts[6], shortcut_labels[6];
    /* 4:3: each tab's box (the pointer's target), its label, and the line along its bottom, zero during a capture;
     * under the Item hotkeys' text, a fan game's line that its hotkeys stay off.  Zero in 16:9, and the pieces of the
     * tabs not shown. */
    OraclesUiBox tabs[ORACLES_CONTROLS_TABS], tab_lines[ORACLES_CONTROLS_TABS];
    OraclesUiLine tab_labels[ORACLES_CONTROLS_TABS];
    OraclesUiLine mode_fan;
    char mode_fan_text[ORACLES_HOME_TEXT_LENGTH];
} OraclesUiControlsLayout;

extern const OraclesUiTextStyle oracles_ui_controls_title, oracles_ui_controls_prompt, oracles_ui_controls_heading;
extern const OraclesUiTextStyle oracles_ui_controls_name, oracles_ui_controls_cell, oracles_ui_controls_cell_locked, oracles_ui_controls_cell_waiting;
extern const OraclesUiTextStyle oracles_ui_controls_item, oracles_ui_controls_item_empty, oracles_ui_controls_slots_note;
extern const OraclesUiTextStyle oracles_ui_controls_shortcut_key, oracles_ui_controls_shortcut_label, oracles_ui_controls_reset;
extern const OraclesUiTextStyle oracles_ui_controls_mode_title, oracles_ui_controls_mode_text_style, oracles_ui_controls_note, oracles_ui_controls_chip;
/* 4:3's, larger for a 640x480 screen: the cells' 32 px, the d-pad's 27. */
extern const OraclesUiTextStyle oracles_ui_controls_title_4_3, oracles_ui_controls_heading_4_3, oracles_ui_controls_cell_4_3;
extern const OraclesUiTextStyle oracles_ui_controls_cell_locked_4_3, oracles_ui_controls_cell_waiting_4_3, oracles_ui_controls_shortcut_key_4_3;
extern const char oracles_ui_controls_mode_text[], oracles_ui_controls_mode_note[], oracles_ui_controls_slots[];
extern const char *const oracles_ui_controls_mode_choices[2];

/* The styles a layout draws Controls in; 4:3 has no slots' note (NULL), 16:9 no tabs (NULL). */
typedef struct OraclesUiControlsStyles {
    const OraclesUiTextStyle *title, *prompt, *heading, *name, *cell, *cell_locked, *cell_waiting, *item, *item_empty, *slots_note;
    const OraclesUiTextStyle *shortcut_key, *shortcut_label, *reset, *mode_title, *mode_text, *note, *chip, *tab;
} OraclesUiControlsStyles;
const OraclesUiControlsStyles *oracles_ui_controls_styles(OraclesUiLayout layout);

/* The screen for `nav` in `layout` (its cells' texts, the prompt, the game shown's items; in a game, the line that
 * says the Item hotkeys wait for the next Play; in 4:3, nav's tab). */
void oracles_ui_layout_controls(OraclesUiLayout layout, const OraclesHomeNav *nav, OraclesUiControlsLayout *out);
/* The text of an item's cell: the item's name, or "Empty". */
const char *oracles_ui_controls_item_text(const OraclesHomeNav *nav, int slot);

#endif
