/* Where Cartridge, Display, Mods and Cheats put their pieces in the scene of a
 * layout (ui_layout.h), without SDL: in 16:9 the title column at the left,
 * the panel of rows at the right; in 4:3 a header with the game's title (and
 * Display's diagram, small, at the top right, its Advanced button beside it)
 * over a single column of rows, Display's header saying its section and the
 * game's title on one line, then the highlighted row's explanation on one line
 * with a note line under it.  16:9's Display has Window's note and the line on
 * its size under the diagram, in the title's column.
 * Display's rows, and Cartridge's European ROM and Language, keep their heights whatever the choices and the highlight;
 * 16:9's Advanced rows keep two lines for their explanations. */
#ifndef ORACLES_UI_PAGE_LAYOUT_H
#define ORACLES_UI_PAGE_LAYOUT_H

#include "ui_home_layout.h"
#include "ui_page_nav.h"
#include "ui_page_text.h"

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
    /* Save's actions, its note (empty: none), its status and the status's second line (NULL: none). */
    const char *save_actions[ORACLES_PAGE_SAVE_ACTIONS];
    const char *save_note, *save_status, *save_status2;
    const char *rom_note, *rom_hotkeys_note, *play_note;
    /* An Oracle with its original ROM (`european`): its European ROM's file, folder (empty without a file) and status,
     * its buttons (Choose…, or Change… and Remove).  Its Language row, or a fan game's (`languages`): its explanation,
     * and its note, empty in English (US). */
    int european, languages;
    char eu_file[ORACLES_HOME_TEXT_LENGTH], eu_folder[ORACLES_HOME_TEXT_LENGTH * 4], eu_status[ORACLES_HOME_TEXT_LENGTH];
    unsigned eu_buttons;
    const char *eu_button_labels[2];
    const char *language_explanation;
    char language_note[ORACLES_HOME_TEXT_LENGTH];
} OraclesUiGameTexts;

typedef struct OraclesUiGameLayout {
    OraclesUiPageHead head;
    OraclesUiBox panel;
    OraclesUiLine label_rom, label_patch, label_save;   /* "ROM", or "Base ROM" and "Patch" */
    OraclesUiLine label_eu, label_language;            /* "European ROM", "Language" */
    /* Each row's highlight, the pointer's target; no Patch row but a fan game's, no European ROM row but an Oracle's
     * with its original ROM, no Language row but that Oracle's and a fan game's.  They keep their heights whatever the
     * choices and the highlight. */
    OraclesUiBox rows[ORACLES_GAME_ROWS];
    OraclesUiLine rom_file, rom_folder, rom_button, rom_button_dot;
    OraclesUiWrapped rom_status, rom_note, rom_hotkeys_note;   /* broken into lines in the file's column */
    OraclesUiBox rom_dot;
    /* European ROM: its file, folder (its line kept without a file), status, and its buttons with their dots, each
     * button's box the pointer's target; Language: the six languages framed, each as wide, laid out whether they show
     * or not, in 16:9 on two lines of three, its explanation and its note (its line kept in English (US)); in 4:3 its
     * label over them, on one line, and one line under them, the note while there is one (the explanation's line then
     * zero), else the explanation (the note's zero). */
    OraclesUiLine eu_file, eu_folder, eu_status, eu_buttons[2], eu_button_dots[2];
    OraclesUiBox eu_dot, eu_button_boxes[2];
    OraclesUiOptionLayout languages[ORACLES_PAGE_LANGUAGES];
    OraclesUiLine language_explanation, language_note;
    OraclesUiLine patch_file, patch_status, patch_button, patch_button_dot, patch_folder, image_status;
    OraclesUiBox patch_dot, image_dot;
    /* Save: its file and its actions with their dots on the first line, each action's box the pointer's target; the line
     * about it; the note, wrapped in 16:9 and on one line in 4:3, or the status beside its dot, its second line under
     * it, in the note's place (4:3: in the line's too). */
    OraclesUiLine save_file, save_line, save_buttons[ORACLES_PAGE_SAVE_ACTIONS], save_button_dots[ORACLES_PAGE_SAVE_ACTIONS];
    OraclesUiBox save_button_boxes[ORACLES_PAGE_SAVE_ACTIONS], save_dot;
    OraclesUiWrapped save_note, save_status, save_status2;
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
extern const OraclesUiTextStyle oracles_ui_language_4_3;
/* Display's 4:3 title, on its section's line. */
extern const OraclesUiTextStyle oracles_ui_display_title_4_3;

/* The styles a layout draws the pages in.  16:9's row_text is also its explanations', row_note its notes'; 4:3 says
 * the highlighted row's explanation in `help`, with its note in `help_note`, and has no `over` (the title's line holds
 * it: the title's style there).  `quality_sets`: what a quality sets, under its name. */
typedef struct OraclesUiPageStyles {
    const OraclesUiTextStyle *section, *over, *title, *state, *page_note;
    const OraclesUiTextStyle *row_label, *row_title, *row_path, *row_status, *row_text, *row_note, *row_button, *play;
    const OraclesUiTextStyle *option_name, *option_size, *choice, *diagram_label, *help, *help_note;
    const OraclesUiTextStyle *mod_games, *mods_empty, *mods_note, *quality_sets;
    const OraclesUiTextStyle *language;   /* Cartridge's languages, in their frames */
} OraclesUiPageStyles;
const OraclesUiPageStyles *oracles_ui_page_styles(OraclesUiLayout layout);

/* The transitions', the core's and the scaling's notes on Display; under 16:9's windows, when no line says their size is
 * reduced. */
extern const char oracles_ui_transitions_note[], oracles_ui_core_note[], oracles_ui_scaling_note[], oracles_ui_window_fit_note[];
/* Under Display's title, opened from a game: that the dimmed rows change from the home screen, the rest at once. */
extern const char oracles_ui_display_later[];
extern const char *const oracles_ui_transition_choices[2];

/* The head of a page in `layout`, under the section's name ("Cartridge", "Display"); NULL `state`: none. */
void oracles_ui_layout_page_head(OraclesUiLayout layout, const char *section, const char *over, const char *title, const char *state,
                                 OraclesUiPageHead *out);
/* Cartridge in `layout`; 4:3 has no panel (`panel` zero), its rows the column's width. */
void oracles_ui_layout_game(OraclesUiLayout layout, OraclesUiGameTexts *texts, OraclesUiGameLayout *out);

/* The import's confirmation over Cartridge (ui_page_save_layout.c): the scene dimmed, a panel at its centre with its
 * title, the file chosen, its folder (no line when it is empty), the question and its buttons, each box the pointer's target: over a save, whether
 * to replace it (Replace, Cancel), the game with mods' own save too when it has one; without one, whether to take the
 * file (Import, Cancel), in the same box. */
typedef struct OraclesUiImportLayout {
    OraclesUiBox backdrop, panel;
    OraclesUiLine title, file, folder;
    char file_text[ORACLES_HOME_TEXT_LENGTH], folder_text[ORACLES_HOME_TEXT_LENGTH * 4];   /* fitted to the panel */
    OraclesUiWrapped question;
    const char *question_text, *label_texts[2];
    OraclesUiBox buttons[2];
    OraclesUiLine labels[2];
} OraclesUiImportLayout;

extern const char oracles_ui_import_title[];
/* Its styles in `layout`: the title's, the question's and the buttons'. */
const OraclesUiTextStyle *oracles_ui_import_style(OraclesUiLayout layout, int part);
/* `replace`: 1 when the game has a save, which the file replaces; 2 when the game with mods has a save, a savestate or
 * mods' data (NAME.mods.*), which go to Backups too. */
void oracles_ui_layout_import(OraclesUiLayout layout, const char *file, const char *folder, int replace, OraclesUiImportLayout *out);

/* The texts of Display. */
typedef struct OraclesUiDisplayTexts {
    const char *section;                  /* "Display", "Display › Advanced", "Display › Picture" or "Display › Text" */
    int advanced;                         /* the Advanced rows in place of Display's own, and no Advanced under the diagram */
    int picture;                          /* the Picture rows in their place, the same */
    int far_size;                         /* Far size among them, where far is fitted to the screen */
    int text_page;                        /* the Text rows in place of Display's own, the preview in the diagram's place */
    const char *over, *title;           /* no state line: the profile is Display's first row */
    char window_names[4][32], window_sizes[4][48];
    const char *window_note;
    char window_reduced[ORACLES_HOME_TEXT_LENGTH];   /* empty when the window fits the screen */
    const char *window_fit;               /* 16:9's line under the windows: window_reduced, or oracles_ui_window_fit_note */
    int window_size_length[4];            /* the characters each window's size line keeps room for */
    const char *profile_note;             /* from a game, oracles_display_pause_reason, as the other notes below it names */
    char quality_sets[ORACLES_DISPLAY_QUALITIES + 1][32];   /* "Far · Fast", ..., "By hand" */
    char quality_text[ORACLES_HOME_TEXT_LENGTH];             /* the quality in effect, said in full */
    const char *quality_note;
    int later;                            /* opened from a game: the note under the title says the dimmed rows change from the home
                                           * screen */
    const char *explanations[9];          /* color correction, continuous transitions, vsync, core, the workers, the scaling, the HUD,
                                           * the screen margin, the Text link */
    char text_summary[64];                /* beside the Text link: "Compact · Wide · Normal" */
    char picture_summary[96];             /* beside the Picture link: "Sharp · No margin · Color off" */
    const char *picture_explanation;      /* the Picture link's */
    /* Display › Text: each row's explanation and note (empty: none), Text, Text box, Text size; the preview. */
    const char *text_explanations[ORACLES_DISPLAY_TEXT_ROWS], *text_notes[ORACLES_DISPLAY_TEXT_ROWS];
    OraclesDisplayTextPreview preview;
    const char *view_explanation;
    /* Under View's explanation, Core's, the transitions', Vsync's and the workers': their notes, from a game why they are
     * dimmed (NULL: none; Vsync's always, live from a game). */
    const char *view_note, *core_note, *transitions_note, *vsync_note, *workers_note;
    char workers_names[3][16];            /* "Auto · 2", "1", "2" */
    char scaling_sizes[2][24];            /* the image's size in fullscreen with Sharp, with Fill */
    const char *camera_explanation, *camera_note;   /* Camera's, and its note (NULL: none) where it has no effect */
    /* Display › Picture's Menus & scenes, Far size and Unvisited rooms: their explanations and notes (empty: none). */
    const char *size_explanations[3];
    char size_notes[3][ORACLES_HOME_TEXT_LENGTH];
    char profile_sizes[ORACLES_PROFILES][24], view_sizes[3][24];   /* the sizes of the screen's shape, Enhanced's at the view chosen */
    float diagram_box_w;                  /* the diagram's box, the screen's shape at its height (0: 16:9's) */
    float diagram_w, diagram_h;           /* the window drawn in the diagram's box */
    char diagram_label[ORACLES_HOME_TEXT_LENGTH];
    int window_one;                       /* every window comes to one size here: the Window row dimmed and inert */
    const char *help, *help_note;         /* 4:3: the highlighted row's explanation and its note (NULL: none), short, in the header */
    char help_text[ORACLES_HOME_TEXT_LENGTH];              /* `help` when made: Quality's */
    char help_note_text[ORACLES_HOME_TEXT_LENGTH + 1];     /* `help_note` when made: the diagram's line and a period, View's far note */
} OraclesUiDisplayTexts;

/* Display's texts for the state of `nav`. */
void oracles_ui_display_texts(const OraclesHomeNav *nav, OraclesUiDisplayTexts *t);

typedef struct OraclesUiDisplayLayout {
    OraclesUiPageHead head;                          /* 4:3: its section alone, the title on its line in `title` */
    OraclesUiLine title;
    char title_text[ORACLES_HOME_TEXT_LENGTH];
    OraclesUiBox diagram, diagram_window;
    OraclesUiLine diagram_label;
    OraclesUiBox panel;
    /* Profile, Quality, Window, View, HUD, Text, Picture, or Core, Continuous transitions, Vsync, Neighbour workers, Camera,
     * or Scaling, Screen margin, Color correction, Far size, Menus & scenes, or Text, Text box, Text size, Text speed; zero
     * for the rows not shown.  Advanced is its own highlight, its label and arrow in it: under the diagram in 16:9, a
     * framed button beside it in 4:3; `link` is ORACLES_DISPLAY_ADVANCED on Display's own rows, ORACLES_DISPLAY_ROWS on
     * the other pages, which have none. */
    OraclesUiWrapped labels[ORACLES_DISPLAY_ROWS];
    OraclesUiBox rows[ORACLES_DISPLAY_ROWS];         /* each row's highlight, the pointer's target */
    unsigned link;
    OraclesUiLine advanced_label;
    OraclesUiBox advanced_arrow;                     /* a triangle pointing right, in this box */
    OraclesUiOptionLayout profiles[ORACLES_PROFILES];
    OraclesUiLine profile_note;
    /* Quality's choices, what each sets under its name, Custom's last, laid out whether it shows or not; 16:9 says the
     * quality in effect and its note under them. */
    OraclesUiOptionLayout qualities[ORACLES_DISPLAY_QUALITIES + 1];
    OraclesUiLine quality_text, quality_note;
    OraclesUiOptionLayout windows[4];
    /* 16:9: under the diagram's line, Window's note and the line on its size, two lines kept for the latter. */
    OraclesUiWrapped window_note, window_reduced;
    OraclesUiLine window_one;                        /* 4:3: in the windows' place, that they come to one size; else zero */
    OraclesUiLine transitions_note, core_note, scaling_note, vsync_note, workers_note, view_note;
    OraclesUiWrapped page_note;                      /* under the title, none unless `later`: two lines in 16:9's column, one in 4:3 */
    OraclesUiOptionLayout views[3];
    OraclesUiWrapped view_explanation;
    OraclesUiOptionLayout transitions[2];
    OraclesUiWrapped explanations[9];
    OraclesUiOptionLayout colour[2], vsync[3], core[2], workers[3], scaling[2];   /* each scaling with its size */
    OraclesUiOptionLayout hud[3];
    /* The Text and Picture links: each one's summary and its arrow at the right of its row, both in `text_link` or
     * `picture_link`, the summary's box; Picture's explanation (16:9). */
    OraclesUiLine text_summary, picture_summary;
    OraclesUiBox text_arrow, text_link, picture_arrow, picture_link;
    OraclesUiWrapped picture_explanation;
    /* Display › Text: Text's, Text box's, Text size's and Text speed's choices, their explanations (16:9, two lines kept)
     * and notes (16:9, under them; 4:3 says them on the help's note line). */
    OraclesUiOptionLayout text[2], text_box[2], text_size[ORACLES_DISPLAY_TEXT_SIZES], text_speed[2];
    OraclesUiWrapped text_explanations[ORACLES_DISPLAY_TEXT_ROWS];
    OraclesUiLine text_notes[ORACLES_DISPLAY_TEXT_ROWS];
    /* Its preview: all of it (16:9: the frame, its label and its note under it; 4:3: the frame, its label and note at its
     * right, at its bottom), the band's frame, the status bar's band in it, the box and its arrow (a triangle pointing
     * down in this box), the box's text from (text_x, text_y) at `pixel` scene pixels a game pixel: the band fitted to
     * the frame, which the drawing puts on the output's pixels (oracles_ui_text_preview_pixels). */
    OraclesUiBox preview, preview_frame, preview_hud, preview_box, preview_arrow;
    float text_x, text_y, pixel;
    OraclesUiLine preview_label, preview_note;
    OraclesUiOptionLayout margin[ORACLES_DISPLAY_MARGINS];   /* 16:9: above its explanation, from the row's left */
    OraclesUiOptionLayout camera[2];
    /* Display › Picture's Menus & scenes, Far size and Unvisited rooms: their choices, explanations (16:9, two lines kept)
     * and notes (16:9, under them; 4:3 says them on the help's note line). */
    OraclesUiOptionLayout menus[2], far_size[2], unvisited[2];
    OraclesUiWrapped size_explanations[3];
    OraclesUiLine size_notes[3];
    OraclesUiWrapped camera_explanation;              /* 16:9 */
    OraclesUiLine camera_note;                        /* 16:9: under its explanation */
    OraclesUiLine help, help_note;                   /* 4:3: in the header, the highlighted row's explanation and its note */
} OraclesUiDisplayLayout;

/* The diagram's box in 16:9, the screen's shape at its height (texts->diagram_box_w its width); 4:3 draws it and its
 * window at half these, rounded. */
#define ORACLES_UI_DIAGRAM_W 432.0f
#define ORACLES_UI_DIAGRAM_H 243.0f

extern const OraclesUiTextStyle oracles_ui_diagram_label;

void oracles_ui_layout_display(OraclesUiLayout layout, const OraclesUiDisplayTexts *texts, OraclesUiDisplayLayout *out);

/* Display's rows the Text page shares (ui_page_text_layout.c).  16:9: a row of choices, its explanation wrapped beside
 * them, `explanation_h` tall at least, its notes under it, centred on the row; returns the row's height.  4:3: a row,
 * its label wrapped in its column and `content_h` of values, both centred in it, 100 tall at least, returning the values'
 * top; a row of options framed, returning its bottom. */
float oracles_ui_display_row(float x, float y, float w, const char *label, float label_x, const char *explanation, float explanation_h,
                             const char *const *notes, int note_count, const char *const *names, const char *const *sizes, unsigned count,
                             OraclesUiWrapped *label_out, OraclesUiWrapped *explanation_out, OraclesUiLine *const *notes_out,
                             OraclesUiOptionLayout *options, OraclesUiBox *row);
float oracles_ui_display_row_4_3(float y, const char *label, float content_h, OraclesUiWrapped *label_out, OraclesUiBox *row);
float oracles_ui_display_options_row_4_3(float y, unsigned row, const char *const *names, const char *const *sizes, const int *size_lengths,
                                         unsigned count, OraclesUiOptionLayout *options, OraclesUiDisplayLayout *out);
/* The Text link among Display's rows, from `y`: 16:9 in the panel's values from `x`, `w` wide, its label at `label_x`;
 * 4:3 in the rows' column.  Returns the row's height (16:9) or its bottom (4:3). */
float oracles_ui_text_link_16_9(const OraclesUiDisplayTexts *t, float x, float y, float w, float label_x, OraclesUiDisplayLayout *out);
float oracles_ui_text_link_4_3(const OraclesUiDisplayTexts *t, float y, OraclesUiDisplayLayout *out);
/* The Picture link the same, under the Text link. */
float oracles_ui_picture_link_16_9(const OraclesUiDisplayTexts *t, float x, float y, float w, float label_x, OraclesUiDisplayLayout *out);
float oracles_ui_picture_link_4_3(const OraclesUiDisplayTexts *t, float y, OraclesUiDisplayLayout *out);
/* Display › Text: 16:9 its preview in the left column from `preview_y`, its rows in the panel's values from (x, y), `w`
 * wide, their labels at `label_x`, returning the rows' bottom; 4:3 its preview from the rows' top, its rows under it. */
float oracles_ui_text_page_16_9(const OraclesUiDisplayTexts *t, float preview_y, float x, float y, float w, float label_x, OraclesUiDisplayLayout *out);
void oracles_ui_text_page_4_3(const OraclesUiDisplayTexts *t, OraclesUiDisplayLayout *out);
/* The preview as drawn, on the output's whole pixels, in a frame of `frame` output pixels where the band fits it at
 * `unit` output pixels a box's pixel.  At one output pixel or more, the band fills the frame and the box takes its place
 * in it, `pixel` = `unit` output pixels a pixel; under one, the box's pixel is one output pixel, the band's
 * scale_den / scale_num, and the frame shows the band's bottom around the box, the box centred, or from the frame's left
 * where its text is wider than the frame, the band whole and centred where it is smaller (`letterbox`).  `clip`, the
 * frame on whole pixels, what is drawn; the band, its status bar and the box unclipped, in output pixels. */
typedef struct OraclesUiTextPreviewPixels {
    OraclesUiBox clip, view, hud, box;
    float pixel;
    int letterbox;
} OraclesUiTextPreviewPixels;
void oracles_ui_text_preview_pixels(const OraclesDisplayTextPreview *p, OraclesUiBox frame, float unit, OraclesUiTextPreviewPixels *out);
/* The box, its text and its arrow at one of the game's pixels a pixel, box_w x box_h pixels from `out`, a row box_w,
 * `colors` its ground, its text's ink and its arrow (0xAARRGGBB): its width. */
#define ORACLES_UI_PREVIEW_PICTURE_W (ORACLES_DISPLAY_SAMPLE_W + 16u)
#define ORACLES_UI_PREVIEW_PICTURE_H 40u
int oracles_ui_text_preview_picture(const OraclesDisplayTextPreview *p, const uint32_t colors[3], uint32_t *out);

/* Mods: the folder's row, each mod's row (its switch and its knob, its name, its games, its line and that line's dot),
 * the count and the note under the list, on Android the Save row, Play.  The texts are the layout's, fitted to their
 * room.  Six mods show at once in 16:9, three in 4:3 (on Android, whose Save row takes the room of one, five and two):
 * past those, the list shows those around the highlighted row. */
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
    /* Android's Save row (save_row's height 0 elsewhere): its label, Export save with mods… and its dot at the right; in
     * 16:9 beside them, in 4:3 under them, its note (save_note_text, wrapped in 16:9, on one line in 4:3), or the
     * export's status (save_status_text, its tone save_tone) beside its dot. */
    OraclesUiBox save_row, save_dot;
    OraclesUiLine label_save, save_button, save_button_dot;
    OraclesUiWrapped save_note, save_status;
    const char *save_note_text, *save_status_text;
    OraclesTone save_tone;
    OraclesUiBox list, play_row;
    OraclesUiLine play_note, play, play_dot;
    const char *play_note_text;
} OraclesUiModsLayout;

extern const OraclesUiTextStyle oracles_ui_mod_games;

void oracles_ui_layout_mods(OraclesUiLayout layout, const OraclesHomeNav *nav, OraclesUiModsLayout *out);

/* Cheats (ui_page_cheats.c), and About as it: the head and the note on the save under the title (16:9: in the head's column, wrapped;
 * 4:3: one line), each row's label and Off and On; 16:9 has the panel, each row's explanation beside its choices and
 * each row's highlight the value's cell, 4:3 the whole row and the highlighted row's explanation and note under the
 * rows (`help`, `help_note`). */
typedef struct OraclesUiCheatsLayout {
    OraclesUiPageHead head;
    OraclesUiWrapped note;
    OraclesUiBox panel;
    OraclesUiLine labels[ORACLES_CHEATS_ROWS];
    OraclesUiBox rows[ORACLES_CHEATS_ROWS];
    OraclesUiWrapped explanations[ORACLES_CHEATS_ROWS];
    OraclesUiOptionLayout choices[ORACLES_CHEATS_ROWS][2];
    OraclesUiLine help, help_note;
} OraclesUiCheatsLayout;

void oracles_ui_layout_cheats(OraclesUiLayout layout, const OraclesHomeNav *nav, OraclesUiCheatsLayout *out);
/* About (ui_page_nav.h), laid out as Cheats with its one row, its note (oracles_about_note, made in `note` for `version`)
 * under the title. */
void oracles_ui_layout_about(OraclesUiLayout layout, const OraclesHomeNav *nav, const char *version, char *note, size_t capacity, OraclesUiCheatsLayout *out);

/* `text` broken into lines of at most `width`, the first at (x, y). */
void oracles_ui_wrap(const OraclesUiTextStyle *style, const char *text, float width, float x, float y, OraclesUiWrapped *out);
/* The same, as text-wrap: pretty breaks it in the browser: a last line of a single word takes the word before it. */
void oracles_ui_wrap_pretty(const OraclesUiTextStyle *style, const char *text, float width, float x, float y, OraclesUiWrapped *out);

/* `text` into `out`, shortened in its middle with an ellipsis when wider than `width` or longer than `out`, so that its
 * end, a path's last folder, stays.  Each text is elided once: the last few are kept. */
void oracles_ui_fit(const OraclesUiTextStyle *style, const char *text, float width, char *out, size_t capacity);
/* `length` bytes of `text` into `out`, shortened in their middle with an ellipsis when they do not hold in it. */
void oracles_ui_copy_middle(char *out, size_t capacity, const char *text, size_t length);
/* `text` into `out`, its end cut where it does not hold, on a character's boundary. */
void oracles_ui_copy_whole(char *out, size_t capacity, const char *text);
/* `text` after the text `out` holds, a space between them when it holds one, shortened in its middle with an ellipsis
 * when it does not hold in the rest of `out`: whole characters, its end kept. */
void oracles_ui_append_middle(char *out, size_t capacity, const char *text);

#endif
