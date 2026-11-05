/* Where the home screen's pieces go in the scene of a layout (ui_layout.h), without SDL: in 16:9 the stack of games
 * on the left, in 4:3 the games as tabs along the top; the hero and its menu, the version and the help bar, which
 * every screen has, the newer version's notice by the version, and the toasts, a message's and the one after a game that
 * ran slowly; computed from the text widths. */
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

/* The pause menu's line under its list: what it says, then where ("Controls › Item hotkeys"), brighter, in its box. */
typedef struct OraclesUiMenuLineLayout {
    OraclesUiBox box;
    OraclesUiLine lead, path;
} OraclesUiMenuLineLayout;

typedef struct OraclesUiHintLayout {
    OraclesUiBox box, key_box;
    OraclesUiLine key, label;
} OraclesUiHintLayout;

typedef struct OraclesUiToastLayout {
    OraclesUiBox box;
    OraclesUiLine text;
} OraclesUiToastLayout;

/* The toast after a game that ran slowly, at the bottom left: its text, on as many lines as it takes within the
 * toast's widest, and under it the row it leads to. */
#define ORACLES_UI_SLOW_LINES 3
typedef struct OraclesUiSlowLayout {
    OraclesUiBox box;
    unsigned lines;
    char text[ORACLES_UI_SLOW_LINES][ORACLES_HOME_TEXT_LENGTH];
    OraclesUiLine text_lines[ORACLES_UI_SLOW_LINES];
    OraclesUiLine path;
} OraclesUiSlowLayout;

/* The newer version's notice by the version: in 16:9 after it on its line, "· 1.3.0 available", in 4:3 on its own line
 * above it, "1.3.0 available"; its frame (the highlight's border and background, `border` wide) and its text. */
typedef struct OraclesUiUpdateLayout {
    OraclesUiBox box;
    float border;
    char label[ORACLES_HOME_NAME_LENGTH + 16];
    OraclesUiLine text;
} OraclesUiUpdateLayout;

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
extern const OraclesUiTextStyle oracles_ui_version_4_3, oracles_ui_toast_4_3, oracles_ui_pad_toast_4_3;
extern const OraclesUiTextStyle oracles_ui_slow_path, oracles_ui_slow_path_4_3;
extern const OraclesUiTextStyle oracles_ui_menu_line, oracles_ui_menu_line_4_3;
/* The pause menu's line on the item hotkeys: "Item hotkeys are off · ", "Controls › Item hotkeys". */
extern const char oracles_ui_hotkeys_line_lead[], oracles_ui_hotkeys_line_path[];

/* The styles a layout draws the home screen's pieces in. */
typedef struct OraclesUiHomeStyles {
    const OraclesUiTextStyle *hero_over, *hero_title, *hero_state, *item_note, *item_label, *hint_key, *hint_label, *version, *toast;
    const OraclesUiTextStyle *tab;   /* the games' tabs: 4:3's only, NULL in 16:9 */
    const OraclesUiTextStyle *slow_path;   /* the slow game's toast: its text in `toast`'s style, the row it leads to in this one */
    const OraclesUiTextStyle *pad_toast;   /* the reminder over a game started, which names the menu's key */
    const OraclesUiTextStyle *menu_line;   /* the pause menu's line under its list */
} OraclesUiHomeStyles;
const OraclesUiHomeStyles *oracles_ui_home_styles(OraclesUiLayout layout);

/* The chosen entry, right-aligned at the top right; `paused`, the pause menu's title, higher in 4:3 without tabs.  4:3
 * has no words above the title (`over` an empty line): the tab names the game. */
void oracles_ui_layout_hero(OraclesUiLayout layout, int paused, const char *over, const char *title, const char *state, OraclesUiTitleLayout *out);
/* 16:9: the two other entries, stacked at the top left. */
void oracles_ui_layout_others(const char *const over[2], const char *const title[2], const char *const state[2], OraclesUiTitleLayout out[2]);
/* 4:3: the three entries as tabs from the top left, in their order (one row: the three fit the 1312 px a row has). */
void oracles_ui_layout_tabs(const char *const labels[ORACLES_HOME_ENTRIES], OraclesUiTabLayout out[ORACLES_HOME_ENTRIES]);
/* The menu at the bottom right; a NULL or empty note takes no width.  A note that is a disabled item's reason
 * (`reasons[i]`, NULL for none) sits on a padded background, readable over the motif.  With `line` (NULL: none), the
 * pause menu's line on the item hotkeys under the items, right-aligned as they are, the items a line higher. */
void oracles_ui_layout_menu(OraclesUiLayout layout, const char *const *notes, const int *reasons, const char *const *labels, unsigned count,
                            OraclesUiMenuLineLayout *line, OraclesUiItemLayout *out);
/* The help bar, centred at the bottom. */
void oracles_ui_layout_hints(OraclesUiLayout layout, const OraclesHomeHint *hints, unsigned count, OraclesUiHintLayout *out);
void oracles_ui_layout_version(OraclesUiLayout layout, const char *text, OraclesUiLine *out);
/* The notice of the newer version `release` ("1.3.0") by the `version` oracles_ui_layout_version laid out, which keeps
 * its place. */
void oracles_ui_layout_update(OraclesUiLayout layout, const OraclesUiLine *version, const char *release, OraclesUiUpdateLayout *out);
/* The version number's place for the highlight, the `version` laid out: padded as the notice is, the line its text, an
 * outline `*outline` wide drawn inside it, so that the number does not move. */
void oracles_ui_layout_version_link(OraclesUiLayout layout, const OraclesUiLine *version, OraclesUiBox *box, float *outline);
void oracles_ui_layout_toast(OraclesUiLayout layout, const char *text, OraclesUiToastLayout *out);
/* The reminder over a game started ("L3 opens the menu"), centred, under the game's status bar, whose bottom is at
 * `bar_bottom` in the scene. */
void oracles_ui_layout_pad_toast(OraclesUiLayout layout, const char *text, float bar_bottom, OraclesUiToastLayout *out);
/* A notice over a game (oracles_pause_notice), as the reminder is: its text on three lines at most within its widest,
 * each centred; `wrapped_lines`, the lines the text took, which a text too long has more of than are laid out. */
#define ORACLES_UI_NOTICE_LINES 3
typedef struct OraclesUiNoticeLayout {
    OraclesUiBox box;
    unsigned lines, wrapped_lines;
    char text[ORACLES_UI_NOTICE_LINES][ORACLES_HOME_TEXT_LENGTH];
    OraclesUiLine text_lines[ORACLES_UI_NOTICE_LINES];
} OraclesUiNoticeLayout;
void oracles_ui_layout_notice(OraclesUiLayout layout, const char *text, float bar_bottom, OraclesUiNoticeLayout *out);
/* The slow game's toast (oracles_home_slow_text): its text and the row it leads to ("Display › Quality"). */
void oracles_ui_layout_slow(OraclesUiLayout layout, const char *text, const char *path, OraclesUiSlowLayout *out);

/* The text "·" that ends each menu item. */
extern const char oracles_ui_item_dot[];

#endif
