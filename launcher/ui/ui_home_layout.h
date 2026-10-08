/* Where the home screen's pieces go in the scene of a layout (ui_layout.h), without SDL: in 16:9 the stack of games
 * on the left, in 4:3 the games as tabs along the top; the hero and its menu, the version and the help bar, which
 * every screen has, and the toast; computed from the text widths. */
#ifndef ORACLES_UI_HOME_LAYOUT_H
#define ORACLES_UI_HOME_LAYOUT_H

#include "ui_font.h"
#include "ui_home_nav.h"
#include "ui_layout.h"

/* One line of text: its line box (x, y, w, h) and its baseline, all in scene pixels. */
typedef struct OraclesUiLine {
    float x, y, w, h;
    float baseline;
} OraclesUiLine;

typedef struct OraclesUiBox {
    float x, y, w, h;
} OraclesUiBox;

/* A title with the words above it and its state below. */
typedef struct OraclesUiTitleLayout {
    OraclesUiLine over, title, state;
    OraclesUiBox box;
} OraclesUiTitleLayout;

typedef struct OraclesUiItemLayout {
    OraclesUiBox box;
    OraclesUiLine note, label, dot;
    OraclesUiBox note_box;      /* a reason's background (zero for another note) */
} OraclesUiItemLayout;

typedef struct OraclesUiHintLayout {
    OraclesUiBox box, key_box;
    OraclesUiLine key, label;
} OraclesUiHintLayout;

typedef struct OraclesUiToastLayout {
    OraclesUiBox box;
    OraclesUiLine text;
} OraclesUiToastLayout;

/* A game's tab (4:3): its box, its label, and the line along its bottom, the chosen tab's drawn in the accent. */
typedef struct OraclesUiTabLayout {
    OraclesUiBox box, line;
    OraclesUiLine label;
} OraclesUiTabLayout;

/* The text styles of the home screen, 16:9's and 4:3's. */
extern const OraclesUiTextStyle oracles_ui_hero_over, oracles_ui_hero_title, oracles_ui_hero_state;
extern const OraclesUiTextStyle oracles_ui_other_over, oracles_ui_other_title, oracles_ui_other_state;
extern const OraclesUiTextStyle oracles_ui_item_note, oracles_ui_item_label;
extern const OraclesUiTextStyle oracles_ui_hint_key, oracles_ui_hint_label;
extern const OraclesUiTextStyle oracles_ui_version, oracles_ui_toast;
extern const OraclesUiTextStyle oracles_ui_tab_4_3, oracles_ui_hero_over_4_3, oracles_ui_hero_title_4_3, oracles_ui_hero_state_4_3;
extern const OraclesUiTextStyle oracles_ui_item_note_4_3, oracles_ui_item_label_4_3, oracles_ui_hint_key_4_3, oracles_ui_hint_label_4_3;
extern const OraclesUiTextStyle oracles_ui_version_4_3, oracles_ui_toast_4_3;

/* The styles a layout draws the home screen's pieces in. */
typedef struct OraclesUiHomeStyles {
    const OraclesUiTextStyle *hero_over, *hero_title, *hero_state, *item_note, *item_label, *hint_key, *hint_label, *version, *toast;
    const OraclesUiTextStyle *tab;   /* the games' tabs: 4:3's only, NULL in 16:9 */
} OraclesUiHomeStyles;
const OraclesUiHomeStyles *oracles_ui_home_styles(OraclesUiLayout layout);

/* The chosen entry, right-aligned at the top right; `paused`, the pause menu's title, higher in 4:3 without tabs. */
void oracles_ui_layout_hero(OraclesUiLayout layout, int paused, const char *over, const char *title, const char *state, OraclesUiTitleLayout *out);
/* 16:9: the two other entries, stacked at the top left. */
void oracles_ui_layout_others(const char *const over[2], const char *const title[2], const char *const state[2], OraclesUiTitleLayout out[2]);
/* 4:3: the three entries as tabs from the top left, in their order (one row: the three fit the 1312 px a row has). */
void oracles_ui_layout_tabs(const char *const labels[ORACLES_HOME_ENTRIES], OraclesUiTabLayout out[ORACLES_HOME_ENTRIES]);
/* The menu at the bottom right; a NULL or empty note takes no width.  A note that is a disabled item's reason
 * (`reasons[i]`, NULL for none) sits on a padded background, readable over the motif. */
void oracles_ui_layout_menu(OraclesUiLayout layout, const char *const *notes, const int *reasons, const char *const *labels, unsigned count, OraclesUiItemLayout *out);
/* The help bar, centred at the bottom. */
void oracles_ui_layout_hints(OraclesUiLayout layout, const OraclesHomeHint *hints, unsigned count, OraclesUiHintLayout *out);
void oracles_ui_layout_version(OraclesUiLayout layout, const char *text, OraclesUiLine *out);
void oracles_ui_layout_toast(OraclesUiLayout layout, const char *text, OraclesUiToastLayout *out);

/* The text "·" that ends each menu item. */
extern const char oracles_ui_item_dot[];

#endif
