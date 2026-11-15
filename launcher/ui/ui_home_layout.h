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

/* A title with the words above it and its state below; a fan game's credit line (zero when none). */
typedef struct OraclesUiTitleLayout {
    OraclesUiLine over, title, state, credit;
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

/* The newer version's notice by the version: in 16:9 after it on its line, "· 1.5.1 available", in 4:3 on its own line
 * above it, "1.5.1 available"; its frame (the highlight's border and background, `border` wide) and its text. */
typedef struct OraclesUiUpdateLayout {
    OraclesUiBox box;
    float border;
    char label[ORACLES_HOME_NAME_LENGTH + 16];
    OraclesUiLine text;
} OraclesUiUpdateLayout;

/* The home screen's Settings button and the version after it, in plain text, at the bottom left (the pages and the
 * pause keep the version alone, oracles_ui_layout_version), the newer version's notice `release` ("1.5.1"; NULL or empty:
 * none) after the two on their line in 16:9, on its own line above them in 4:3: the button's frame (its border `border`
 * wide) and label, the version's padded box and text, and `hit`, the button's pointer target, which in 16:9 reaches the
 * scene's bottom left corner.  The version, then the notice, stay out where they would come nearer the help bar (its
 * left edge `hints_x`, oracles_ui_layout_hints_left) than ORACLES_UI_VERSION_GAP (`version_shown`, `update_shown`). */
typedef struct OraclesUiSettingsLink {
    OraclesUiBox button, hit, version_box;
    OraclesUiLine label, version;
    float border;
    int version_shown, update_shown;
    OraclesUiUpdateLayout update;
} OraclesUiSettingsLink;
#define ORACLES_UI_VERSION_GAP 24.0f   /* the least room between the version and the help bar */
extern const char oracles_ui_settings_label[];   /* "Settings" */

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
extern const OraclesUiTextStyle oracles_ui_version, oracles_ui_toast, oracles_ui_hero_credit, oracles_ui_hero_credit_4_3;
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
/* The pause's title where its menu would reach it (4:3's of eight entries, RetroAchievements' with Cheats): at the top
 * left, each line from the left, as far from it as from the right. */
void oracles_ui_layout_hero_left(OraclesUiLayout layout, OraclesUiTitleLayout *out);
/* The same with a fan game's `credit` (NULL: none): 16:9, under the state, so that the state stays where it is; 4:3, on
 * the state's line at its left, on its baseline, the menu starting right under that line. */
void oracles_ui_layout_hero_credited(OraclesUiLayout layout, int paused, const char *over, const char *title, const char *state, const char *credit,
                                     OraclesUiTitleLayout *out);
/* The home screen's title over a menu of eight entries: 16:9, where it stays, a fan game's `credit` on the state's line at
 * its left, on its baseline, clear of Start game; 4:3, at the top left, each line from the left, the credit under the
 * state, clear of the menu. */
void oracles_ui_layout_hero_crowded(OraclesUiLayout layout, const char *over, const char *title, const char *state, const char *credit, OraclesUiTitleLayout *out);
#define ORACLES_UI_MENU_CROWDED 8u   /* the home menu's entries from which its title gives way */
/* 16:9: the two other entries, stacked at the top left. */
void oracles_ui_layout_others(const char *const over[2], const char *const title[2], const char *const state[2], OraclesUiTitleLayout out[2]);
/* 4:3: the three entries as tabs from the top left, in their order (one row: the three fit the 1312 px a row has). */
void oracles_ui_layout_tabs(const char *const labels[ORACLES_HOME_ENTRIES], OraclesUiTabLayout out[ORACLES_HOME_ENTRIES]);
/* The menu at the bottom right; a NULL or empty note takes no width.  A note that is a disabled item's reason
 * (`reasons[i]`, NULL for none) sits on a padded background, readable over the motif.  With `line` (NULL: none), the
 * pause menu's line on the item hotkeys under the items, right-aligned as they are, the items a line higher. */
void oracles_ui_layout_menu(OraclesUiLayout layout, const char *const *notes, const int *reasons, const char *const *labels, unsigned count,
                            OraclesUiMenuLineLayout *line, OraclesUiItemLayout *out);
/* The home screen's menu: as oracles_ui_layout_menu lays it out, but for 4:3's items, 86 tall when there are
 * ORACLES_UI_MENU_CROWDED, so that it starts under the longest title at the top left. */
void oracles_ui_layout_home_menu(OraclesUiLayout layout, const char *const *notes, const int *reasons, const char *const *labels, unsigned count, OraclesUiItemLayout *out);
/* The help bar, centred at the bottom. */
void oracles_ui_layout_hints(OraclesUiLayout layout, const OraclesHomeHint *hints, unsigned count, OraclesUiHintLayout *out);
/* The help bar of the screen `nav` shows in `layout`: oracles_ui_layout_hints', except on the home screen in 4:3, where it
 * is centred right of the Settings button and the version, its hints 12 apart instead of 16. */
void oracles_ui_layout_screen_hints(OraclesUiLayout layout, const OraclesHomeNav *nav, const OraclesHomeHint *hints, unsigned count, OraclesUiHintLayout *out);
/* The left edge of the help bar of the screen `nav` shows, or the scene's right edge without a hint. */
float oracles_ui_layout_hints_left(OraclesUiLayout layout, const OraclesHomeNav *nav);
void oracles_ui_layout_version(OraclesUiLayout layout, const char *text, OraclesUiLine *out);
void oracles_ui_layout_settings_link(OraclesUiLayout layout, const char *version, const char *release, float hints_x, OraclesUiSettingsLink *out);
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
