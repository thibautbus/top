/* Where the home screen's pieces go in the 1920x1080 scene, without SDL: the
 * stack of games on the left, the hero and its menu, the version and the help bar,
 * computed from the text widths. */
#ifndef ORACLES_UI_HOME_LAYOUT_H
#define ORACLES_UI_HOME_LAYOUT_H

#include "ui_font.h"
#include "ui_home_nav.h"

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

/* The text styles of the home screen. */
extern const OraclesUiTextStyle oracles_ui_hero_over, oracles_ui_hero_title, oracles_ui_hero_state;
extern const OraclesUiTextStyle oracles_ui_other_over, oracles_ui_other_title, oracles_ui_other_state;
extern const OraclesUiTextStyle oracles_ui_item_note, oracles_ui_item_label;
extern const OraclesUiTextStyle oracles_ui_hint_key, oracles_ui_hint_label;
extern const OraclesUiTextStyle oracles_ui_version, oracles_ui_toast;

/* The chosen entry, right-aligned at the top right. */
void oracles_ui_layout_hero(const char *over, const char *title, const char *state, OraclesUiTitleLayout *out);
/* The two other entries, stacked at the top left. */
void oracles_ui_layout_others(const char *const over[2], const char *const title[2], const char *const state[2], OraclesUiTitleLayout out[2]);
/* The menu at the bottom right; a NULL or empty note takes no width.  A note that is a disabled item's reason
 * (`reasons[i]`, NULL for none) sits on a padded background, readable over the motif. */
void oracles_ui_layout_menu(const char *const *notes, const int *reasons, const char *const *labels, unsigned count, OraclesUiItemLayout *out);
/* The help bar, centred at the bottom. */
void oracles_ui_layout_hints(const OraclesHomeHint *hints, unsigned count, OraclesUiHintLayout *out);
void oracles_ui_layout_version(const char *text, OraclesUiLine *out);
void oracles_ui_layout_toast(const char *text, OraclesUiToastLayout *out);

/* The text "·" that ends each menu item. */
extern const char oracles_ui_item_dot[];

#endif
