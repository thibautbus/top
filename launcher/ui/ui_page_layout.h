/* Where Cartridge, Display and Mods put their pieces in the scene of a
 * layout (ui_layout.h), without SDL: in 16:9 the title column at the left,
 * the panel of rows at the right; in 4:3 a header with the game's title (and
 * Display's diagram, small, at the top right, its Advanced button beside it)
 * over a single column of rows, Display saying the highlighted row's
 * explanation once, under its rows, on one line with a note line under it.
 * Display's rows keep their heights whatever the choices and the highlight. */
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

/* A text broken into lines at spaces and after a dash, each line as wide as fits: the browser's rule for these texts. */
typedef struct OraclesUiWrapped {
    unsigned count;
    char text[ORACLES_UI_WRAP_LINES][ORACLES_UI_WRAP_LENGTH];
    OraclesUiLine lines[ORACLES_UI_WRAP_LINES];
    float w, h;                 /* the widest line, and the lines' height */
} OraclesUiWrapped;

/* The head of a page: its section's name, the game's title and state.  16:9: the left column, the words above the
 * title on their line, the title on two lines when it is long (Moonrise Regalia).  4:3: the header, the title on one
 * line with its words ("Oracle of Ages"), `over` empty. */
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
/* 4:3's, larger for a 640x480 screen; a choice's name is a profile's, the row button's style. */
extern const OraclesUiTextStyle oracles_ui_page_section_4_3, oracles_ui_page_title_4_3, oracles_ui_page_state_4_3, oracles_ui_page_note_4_3;
extern const OraclesUiTextStyle oracles_ui_row_label_4_3, oracles_ui_row_title_4_3, oracles_ui_row_path_4_3, oracles_ui_row_text_4_3;
extern const OraclesUiTextStyle oracles_ui_row_note_4_3, oracles_ui_row_button_4_3, oracles_ui_play_4_3, oracles_ui_option_size_4_3;
extern const OraclesUiTextStyle oracles_ui_diagram_label_4_3, oracles_ui_help_4_3, oracles_ui_mod_games_4_3, oracles_ui_quality_sets_4_3;

/* The styles a layout draws the pages in.  16:9's row_text is also its explanations', row_note its notes'; 4:3 says
 * the highlighted row's explanation in `help`, with its note in `help_note`, and has no `over` (the title's line holds
 * it: the title's style there).  `quality_sets`: what a quality sets, under its name. */
typedef struct OraclesUiPageStyles {
    const OraclesUiTextStyle *section, *over, *title, *state, *page_note;
    const OraclesUiTextStyle *row_label, *row_title, *row_path, *row_status, *row_text, *row_note, *row_button, *play;
    const OraclesUiTextStyle *option_name, *option_size, *choice, *diagram_label, *help, *help_note;
    const OraclesUiTextStyle *mod_games, *mods_empty, *mods_note, *quality_sets;
} OraclesUiPageStyles;
const OraclesUiPageStyles *oracles_ui_page_styles(OraclesUiLayout layout);

/* The transitions' and the core's notes on Display; under 16:9's windows, when no line says their size is reduced. */
extern const char oracles_ui_transitions_note[], oracles_ui_core_note[], oracles_ui_window_fit_note[];
/* Under Display's title, opened from a game: what applies at once and what waits for the next session. */
extern const char oracles_ui_display_later[];
/* Under a row of Display opened from a game, what only the next session takes. */
extern const char oracles_ui_later[];
extern const char *const oracles_ui_transition_choices[2];

/* The head of a page in `layout`, under the section's name ("Cartridge", "Display"); NULL `state`: none. */
void oracles_ui_layout_page_head(OraclesUiLayout layout, const char *section, const char *over, const char *title, const char *state,
                                 OraclesUiPageHead *out);
/* Cartridge in `layout`; 4:3 has no panel (`panel` zero), its rows the column's width. */
void oracles_ui_layout_game(OraclesUiLayout layout, OraclesUiGameTexts *texts, OraclesUiGameLayout *out);

/* The texts of Display. */
typedef struct OraclesUiDisplayTexts {
    const char *section;                  /* "Display", or "Display › Advanced" */
    int advanced;                         /* the Advanced rows in place of Display's own, and no Advanced under the diagram */
    const char *over, *title;           /* no state line: the profile is Display's first row */
    char window_names[4][32], window_sizes[4][48];
    const char *window_note;
    char window_reduced[ORACLES_HOME_TEXT_LENGTH];   /* empty when the window fits the screen */
    const char *window_fit;               /* 16:9's line under the windows: window_reduced, or oracles_ui_window_fit_note */
    int window_size_length[4];            /* the characters each window's size line keeps room for */
    const char *profile_note;
    char quality_sets[ORACLES_DISPLAY_QUALITIES + 1][32];   /* "Far · Fast", ..., "By hand" */
    char quality_text[ORACLES_HOME_TEXT_LENGTH];             /* the quality in effect, said in full */
    const char *quality_note;
    int later;                            /* opened from a game: the note under the title says what the next Play takes */
    const char *explanations[5];          /* color correction, continuous transitions, vsync, core, the workers */
    const char *view_explanation;
    char workers_names[3][16];            /* "Auto · 2", "1", "2" */
    char profile_sizes[ORACLES_PROFILES][24], view_sizes[3][24];   /* the sizes of the screen's shape, Enhanced's at the view chosen */
    float diagram_box_w;                  /* the diagram's box, the screen's shape at its height (0: 16:9's) */
    float diagram_w, diagram_h;           /* the window drawn in the diagram's box */
    char diagram_label[ORACLES_HOME_TEXT_LENGTH];
    int window_one;                       /* every window comes to one size here: the Window row dimmed and inert */
    const char *help, *help_note;         /* 4:3: the highlighted row's explanation and its note (NULL: none), short */
    char help_text[ORACLES_HOME_TEXT_LENGTH];              /* `help` when made: Quality's */
    char help_note_text[ORACLES_HOME_TEXT_LENGTH + 1];     /* `help_note` when made: the diagram's line and a period */
} OraclesUiDisplayTexts;

/* Display's texts for the state of `nav`. */
void oracles_ui_display_texts(const OraclesHomeNav *nav, OraclesUiDisplayTexts *t);

typedef struct OraclesUiDisplayLayout {
    OraclesUiPageHead head;
    OraclesUiBox diagram, diagram_window;
    OraclesUiLine diagram_label;
    OraclesUiBox panel;
    /* Profile, Quality, Window, View, Color correction, Continuous transitions, or Core, Vsync, Neighbour workers; zero
     * for the rows not shown.  Advanced is its own highlight, its label and arrow in it: under the diagram in 16:9, a
     * framed button beside it in 4:3. */
    OraclesUiWrapped labels[ORACLES_DISPLAY_ROWS];
    OraclesUiBox rows[ORACLES_DISPLAY_ROWS];         /* each row's highlight, the pointer's target */
    OraclesUiLine advanced_label;
    OraclesUiBox advanced_arrow;                     /* a triangle pointing right, in this box */
    OraclesUiOptionLayout profiles[ORACLES_PROFILES];
    OraclesUiLine profile_note;
    /* Quality's choices, what each sets under its name, Custom's last, laid out whether it shows or not; 16:9 says the
     * quality in effect and its note under them. */
    OraclesUiOptionLayout qualities[ORACLES_DISPLAY_QUALITIES + 1];
    OraclesUiLine quality_text, quality_note;
    OraclesUiOptionLayout windows[4];
    OraclesUiLine window_note, window_reduced;
    OraclesUiLine window_one;                        /* 4:3: in the windows' place, that they come to one size; else zero */
    OraclesUiLine transitions_note, core_note;
    OraclesUiLine page_note;                         /* under the title, zero unless `later` */
    OraclesUiOptionLayout views[3];
    OraclesUiWrapped view_explanation;
    OraclesUiOptionLayout transitions[2];
    OraclesUiWrapped explanations[5];
    OraclesUiOptionLayout colour[2], vsync[3], core[2], workers[3];
    OraclesUiLine help, help_note;                   /* 4:3: under the rows, the highlighted row's explanation and its note */
} OraclesUiDisplayLayout;

/* The diagram's box in 16:9, the screen's shape at its height (texts->diagram_box_w its width); 4:3 draws it and its
 * window at half these, rounded. */
#define ORACLES_UI_DIAGRAM_W 432.0f
#define ORACLES_UI_DIAGRAM_H 243.0f

extern const OraclesUiTextStyle oracles_ui_diagram_label;

void oracles_ui_layout_display(OraclesUiLayout layout, const OraclesUiDisplayTexts *texts, OraclesUiDisplayLayout *out);

/* Mods: the folder's row, each mod's row (its switch and its knob, its name, its games, its line and that line's dot),
 * the count and the note under the list, Play.  The texts are the layout's, fitted to their room.  Six mods show at
 * once in 16:9, three in 4:3: past those, the list shows those around the highlighted row. */
#define ORACLES_UI_MODS_SHOWN 6
#define ORACLES_UI_MODS_SHOWN_4_3 3
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
    OraclesUiWrapped folder_lines;         /* 4:3: the path broken anywhere, on two lines at most, folder_path its first; 16:9: none */
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

void oracles_ui_layout_mods(OraclesUiLayout layout, const OraclesHomeNav *nav, OraclesUiModsLayout *out);

/* `text` broken into lines of at most `width`, the first at (x, y). */
void oracles_ui_wrap(const OraclesUiTextStyle *style, const char *text, float width, float x, float y, OraclesUiWrapped *out);

/* `text` into `out`, shortened in its middle with an ellipsis when wider than `width` or longer than `out`, so that its
 * end, a path's last folder, stays.  Each text is elided once: the last few are kept. */
void oracles_ui_fit(const OraclesUiTextStyle *style, const char *text, float width, char *out, size_t capacity);
/* `length` bytes of `text` into `out`, shortened in their middle with an ellipsis when they do not hold in it. */
void oracles_ui_copy_middle(char *out, size_t capacity, const char *text, size_t length);

#endif
