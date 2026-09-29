/* Where Controls puts its pieces in the 1920x1080 scene, without SDL: the
 * title and the capture's prompt, the game buttons' panel on
 * the left, the item hotkeys' on the right, the fixed shortcuts under them. */
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
    OraclesUiBox shortcut_keys[6];
    OraclesUiLine shortcut_key_texts[6], shortcut_labels[6];
} OraclesUiControlsLayout;

extern const OraclesUiTextStyle oracles_ui_controls_title, oracles_ui_controls_prompt, oracles_ui_controls_heading;
extern const OraclesUiTextStyle oracles_ui_controls_name, oracles_ui_controls_cell, oracles_ui_controls_cell_locked, oracles_ui_controls_cell_waiting;
extern const OraclesUiTextStyle oracles_ui_controls_item, oracles_ui_controls_item_empty, oracles_ui_controls_slots_note;
extern const OraclesUiTextStyle oracles_ui_controls_shortcut_key, oracles_ui_controls_shortcut_label, oracles_ui_controls_reset;
extern const OraclesUiTextStyle oracles_ui_controls_mode_title, oracles_ui_controls_mode_text_style, oracles_ui_controls_note, oracles_ui_controls_chip;
extern const char oracles_ui_controls_mode_text[], oracles_ui_controls_mode_note[], oracles_ui_controls_slots[];
extern const char *const oracles_ui_controls_mode_choices[2];

/* The screen for `nav` (its cells' texts, the prompt, the game shown's items; in a game, the line that says the
 * Item hotkeys wait for the next Play). */
void oracles_ui_layout_controls(const OraclesHomeNav *nav, OraclesUiControlsLayout *out);
/* The text of an item's cell: the item's name, or "Empty". */
const char *oracles_ui_controls_item_text(const OraclesHomeNav *nav, int slot);

#endif
