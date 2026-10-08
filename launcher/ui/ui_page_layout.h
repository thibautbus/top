/* Where Cartridge and Display put their pieces in the 1920x1080 scene,
 * without SDL: the title column at the left, the
 * panel of rows at the right. */
#ifndef ORACLES_UI_PAGE_LAYOUT_H
#define ORACLES_UI_PAGE_LAYOUT_H

#include "ui_home_layout.h"
#include "ui_page_nav.h"

/* An option in a frame: a profile or a window (a name and its size), or a choice of a row (a name). */
typedef struct OraclesUiOptionLayout {
    OraclesUiBox box;
    OraclesUiLine name, size;
} OraclesUiOptionLayout;

#define ORACLES_UI_WRAP_LINES 4
#define ORACLES_UI_WRAP_LENGTH 160

/* A text broken into lines at spaces, each line as wide as fits: the browser's rule for these texts. */
typedef struct OraclesUiWrapped {
    unsigned count;
    char text[ORACLES_UI_WRAP_LINES][ORACLES_UI_WRAP_LENGTH];
    OraclesUiLine lines[ORACLES_UI_WRAP_LINES];
    float w, h;                 /* the widest line, and the lines' height */
} OraclesUiWrapped;

/* The left column of a page: its section's name, the game's title (on two lines when it is long: Moonrise Regalia)
 * and state. */
typedef struct OraclesUiPageHead {
    OraclesUiLine section, over, state;
    OraclesUiWrapped title;
} OraclesUiPageHead;

/* The texts of Cartridge, some shortened to their room by the layout.  A fan game made as a patch (`patched`) has
 * its base ROM in the rom_ texts, without a folder, then its patch, the patch's folder and the image's line. */
typedef struct OraclesUiGameTexts {
    const char *over, *title, *state;
    int patched;
    char rom_file[ORACLES_HOME_TEXT_LENGTH], rom_folder[ORACLES_HOME_TEXT_LENGTH * 4], rom_status[ORACLES_HOME_TEXT_LENGTH];
    char patch_file[ORACLES_HOME_TEXT_LENGTH], patch_folder[ORACLES_HOME_TEXT_LENGTH * 4], patch_status[ORACLES_HOME_TEXT_LENGTH];
    char image_status[ORACLES_HOME_TEXT_LENGTH];
    char save_file[ORACLES_HOME_TEXT_LENGTH], save_line[ORACLES_HOME_TEXT_LENGTH];
    const char *rom_note, *rom_hotkeys_note, *play_note;
} OraclesUiGameTexts;

typedef struct OraclesUiGameLayout {
    OraclesUiPageHead head;
    OraclesUiBox panel;
    OraclesUiLine label_rom, label_patch, label_save;   /* "ROM", or "Base ROM" and "Patch" */
    OraclesUiBox rows[ORACLES_GAME_ROWS];            /* each row's highlight, the pointer's target; no Patch row but a fan game's */
    OraclesUiLine rom_file, rom_folder, rom_status, rom_note, rom_hotkeys_note, rom_button, rom_button_dot;
    OraclesUiBox rom_dot;
    OraclesUiLine patch_file, patch_status, patch_button, patch_button_dot, patch_folder, image_status;
    OraclesUiBox patch_dot, image_dot;
    OraclesUiLine save_file, save_line, save_button, save_button_dot;
    OraclesUiLine play_note, play, play_dot;
} OraclesUiGameLayout;

extern const OraclesUiTextStyle oracles_ui_page_section, oracles_ui_page_over, oracles_ui_page_title, oracles_ui_page_state;
extern const OraclesUiTextStyle oracles_ui_row_label, oracles_ui_row_title, oracles_ui_row_path;
extern const OraclesUiTextStyle oracles_ui_row_status, oracles_ui_row_text, oracles_ui_row_note, oracles_ui_row_button;
extern const OraclesUiTextStyle oracles_ui_option_name, oracles_ui_option_size, oracles_ui_choice, oracles_ui_play;

/* The transitions' and the core's notes on Display. */
extern const char oracles_ui_transitions_note[], oracles_ui_core_note[];
/* Under Display's title, opened from a game: what applies at once and what waits for the next session. */
extern const char oracles_ui_display_later[];
/* Under a row of Display opened from a game, what only the next session takes. */
extern const char oracles_ui_later[];
extern const char *const oracles_ui_transition_choices[2];

/* The left column of a page, under the section's name ("Cartridge", "Display"). */
void oracles_ui_layout_page_head(const char *section, const char *over, const char *title, const char *state, OraclesUiPageHead *out);
void oracles_ui_layout_game(OraclesUiGameTexts *texts, OraclesUiGameLayout *out);

/* The texts of Display. */
typedef struct OraclesUiDisplayTexts {
    const char *section;                  /* "Display", or "Display › Advanced" */
    int advanced;                         /* the Advanced rows in place of Display's own, and no Advanced under the diagram */
    const char *over, *title;           /* no state line: the profile is Display's first row */
    char window_names[4][32], window_sizes[4][48];
    const char *window_note;
    char window_reduced[ORACLES_HOME_TEXT_LENGTH];   /* empty when the window fits the screen */
    const char *profile_note;
    int later;                            /* opened from a game: the note under the title says what the next Play takes */
    const char *explanations[5];          /* color correction, continuous transitions, vsync, core, the workers */
    const char *view_explanation;
    char workers_names[3][16];            /* "Auto · 2", "1", "2" */
    char profile_sizes[ORACLES_PROFILES][24], view_sizes[3][24];   /* the sizes of the screen's shape, Enhanced's at the view chosen */
    float diagram_box_w;                  /* the diagram's box, the screen's shape at its height (0: 16:9's) */
    float diagram_w, diagram_h;           /* the window drawn in the diagram's box */
    char diagram_label[ORACLES_HOME_TEXT_LENGTH];
} OraclesUiDisplayTexts;

typedef struct OraclesUiDisplayLayout {
    OraclesUiPageHead head;
    OraclesUiBox diagram, diagram_window;
    OraclesUiLine diagram_label;
    OraclesUiBox panel;
    /* Profile, Window, View, Color correction, Continuous transitions, or Core, Vsync, Neighbour workers; zero for the
     * rows not shown.  Advanced is laid out under the diagram: its row is its own highlight, its label and arrow in it. */
    OraclesUiWrapped labels[ORACLES_DISPLAY_ROWS];
    OraclesUiBox rows[ORACLES_DISPLAY_ROWS];         /* each row's highlight, the pointer's target */
    OraclesUiLine advanced_label;
    OraclesUiBox advanced_arrow;                     /* a triangle pointing right, in this box */
    OraclesUiOptionLayout profiles[ORACLES_PROFILES];
    OraclesUiLine profile_note;
    OraclesUiOptionLayout windows[4];
    OraclesUiLine window_note, window_reduced;
    OraclesUiLine transitions_note, core_note;
    OraclesUiLine page_note;                         /* under the title, zero unless `later` */
    OraclesUiOptionLayout views[3];
    OraclesUiWrapped view_explanation;
    OraclesUiOptionLayout transitions[2];
    OraclesUiWrapped explanations[5];
    OraclesUiOptionLayout colour[2], vsync[3], core[2], workers[3];
} OraclesUiDisplayLayout;

#define ORACLES_UI_DIAGRAM_W 432.0f
#define ORACLES_UI_DIAGRAM_H 243.0f

extern const OraclesUiTextStyle oracles_ui_diagram_label;

void oracles_ui_layout_display(const OraclesUiDisplayTexts *texts, OraclesUiDisplayLayout *out);

/* Mods: the folder's row, each mod's row (its switch and its knob, its name, its games, its line and that line's dot),
 * the count and the note under the list, Play.  The texts are the layout's, fitted to their room.  Six mods show at
 * once: past six, the list shows those around the highlighted row. */
#define ORACLES_UI_MODS_SHOWN 6
typedef struct OraclesUiModLayout {
    unsigned index;                        /* the mod's in the list */
    OraclesUiBox row, toggle, knob, dot;
    OraclesUiLine name, games, line;
    char name_text[ORACLES_HOME_MOD_NAME], games_text[64], line_text[ORACLES_HOME_TEXT_LENGTH];
} OraclesUiModLayout;

typedef struct OraclesUiModsLayout {
    OraclesUiPageHead head;
    char state[ORACLES_HOME_STATE_LENGTH];
    OraclesUiBox panel;
    OraclesUiLine label_folder, label_mods;
    OraclesUiBox folder_row;
    OraclesUiLine folder_path, folder_line, folder_button, folder_button_dot;
    char folder_text[ORACLES_HOME_TEXT_LENGTH * 4];
    OraclesUiWrapped empty;                /* when no mod is found */
    unsigned shown;
    OraclesUiModLayout mods[ORACLES_UI_MODS_SHOWN];
    OraclesUiLine count;
    char count_text[ORACLES_HOME_TEXT_LENGTH];
    OraclesUiWrapped note;
    OraclesUiBox list, play_row;
    OraclesUiLine play_note, play, play_dot;
    const char *play_note_text;
} OraclesUiModsLayout;

extern const OraclesUiTextStyle oracles_ui_mod_games;

void oracles_ui_layout_mods(const OraclesHomeNav *nav, OraclesUiModsLayout *out);

/* `text` broken into lines of at most `width`, the first at (x, y). */
void oracles_ui_wrap(const OraclesUiTextStyle *style, const char *text, float width, float x, float y, OraclesUiWrapped *out);

/* `text` into `out`, shortened in its middle with an ellipsis when wider than `width` or longer than `out`, so that its
 * end, a path's last folder, stays.  Each text is elided once: the last few are kept. */
void oracles_ui_fit(const OraclesUiTextStyle *style, const char *text, float width, char *out, size_t capacity);
/* `length` bytes of `text` into `out`, shortened in their middle with an ellipsis when they do not hold in it. */
void oracles_ui_copy_middle(char *out, size_t capacity, const char *text, size_t length);

#endif
