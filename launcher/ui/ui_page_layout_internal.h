/* The pages' layouts' shared measures and helpers (ui_page_layout.c, ui_page_display_layout.c), in scene pixels. */
#ifndef ORACLES_UI_PAGE_LAYOUT_INTERNAL_H
#define ORACLES_UI_PAGE_LAYOUT_INTERNAL_H

#include "ui_page_layout.h"

/* Vsync's short help in the 4:3 layout (ui_page_display.c has the long one). */
#ifdef __ANDROID__
#define VSYNC_SHORT "Auto: off on Android."
#else
#define VSYNC_SHORT "Auto: follows a display near 60 Hz."
/* 4:3's: the room a file's value has beside its button; a button placed at the rows' right; a row's box; the Play row with
 * its note. */
float pl_file_room_4_3(float value_x, float button_w);
float pl_place_button_4_3(const char *label, OraclesUiLine *text, OraclesUiLine *dot);
OraclesUiBox pl_play_4_3_row(const char *note, float y, OraclesUiLine *note_out, OraclesUiLine *play, OraclesUiLine *dot);
OraclesUiBox pl_row_4_3(float y, float bottom);

/* A button and its dot at a row's right; a text wrapped at `width` in lines of `line_height`; the page's head in 4:3. */
void pl_button(const char *label, float right, float top, float height, OraclesUiLine *text, OraclesUiLine *dot);
void pl_wrap_lines(const OraclesUiTextStyle *style, const char *text, float width, float x, float y, float line_height, OraclesUiWrapped *out);
void pl_page_head_4_3(const char *section, const char *over, const char *title, const char *state, OraclesUiPageHead *out);

#endif

/* The pages' geometry, in scene pixels. */
#define HEAD_X 96.0f
#define HEAD_Y 88.0f
#define HEAD_W 520.0f
#define HEAD_SECTION_GAP 28.0f
#define HEAD_TITLE_LINE 104.0f
#define HEAD_STATE_MARGIN 20.0f
#define PANEL_X 656.0f
#define PANEL_Y 56.0f
#define PANEL_W 1208.0f
#define PANEL_PAD_X 32.0f
#define PANEL_PAD_Y 24.0f
#define PANEL_BORDER 1.0f
#define GAME_ROW_GAP 10.0f
#define LABEL_W 150.0f              /* Mods' labels */
#define CARTRIDGE_LABEL_W 240.0f    /* Cartridge's, as wide as European ROM */
#define GRID_GAP 20.0f
#define LABEL_PAD_TOP 18.0f
#define ROW_PAD_X 20.0f
#define ROW_PAD_Y 12.0f
#define PLAY_PAD_Y 4.0f
#define ROW_GAP 24.0f          /* between a row's text and its button */
#define BUTTON_GAP 14.0f       /* between a button and its dot */
#define FILE_GAP 4.0f          /* between the lines of a file */
#define STATUS_MARGIN 2.0f
#define PATCH_LINE_GAP 8.0f    /* between the Patch row, its folder and the image's line */
#define IMAGE_PAD_BOTTOM 4.0f
#define EU_BUTTONS_GAP 20.0f        /* between Change… and Remove */
#define EU_FOLDER_LINE 26.0f        /* the European ROM's folder, its line kept without a file */
#define LANGUAGE_W 200.0f           /* each language's frame */
#define LANGUAGE_COLUMNS 3          /* the frames of a line */
#define LANGUAGE_GAP 8.0f           /* between the frames, and under them */
#define LANGUAGE_LINE 29.0f         /* Language's explanation and its note, each line kept */
#define DOT 12.0f
#define DOT_GAP 12.0f
#define OPTION_GAP 12.0f
#define OPTION_PAD_X 18.0f
#define OPTION_PAD_Y 8.0f
#define OPTION_LINE_GAP 2.0f
#define PROFILE_NOTE_GAP 10.0f      /* under the profiles, and under the qualities */
#define PROFILE_NOTE_LINE 32.0f
#define PROFILE_NOTE_H 64.0f         /* two lines kept for the profile's note */
#define QUALITY_TEXT_LINE 32.0f      /* the quality in effect, on one line */
#define QUALITY_NOTE_LINE 28.0f      /* its note */
#define CHOICE_GAP 8.0f
#define CHOICE_PAD_X 16.0f
#define CHOICE_PAD_Y 4.0f
#define BORDER 1.0f
#define NOTE_GAP 2.0f          /* between an explanation and its italic note */
#define ADVANCED_EXPLANATION_H 58.0f   /* two lines of 29 kept for each Advanced row's explanation */
#define MARGIN_ROW_PAD_Y 8.0f          /* Screen margin's row, its choices above its explanation */
#define MARGIN_ROW_GAP 6.0f
#define PLAY_GAP 24.0f
#define DISPLAY_ROW_GAP 14.0f
#define DISPLAY_LABEL_W 200.0f
#define DISPLAY_ROW_PAD_Y 14.0f
#define WINDOW_NOTE_LINE 29.0f       /* Window's note and the line on its size, under the diagram */
#define WINDOW_FIT_H 58.0f           /* two lines kept for the latter */
#define DIAGRAM_MARGIN 56.0f
#define DISPLAY_PAGE_NOTE_GAP 20.0f   /* the note under Display's title, opened from a game */
#define DISPLAY_PAGE_NOTE_LINE 29.0f  /* its lines, in the left column */
#define DIAGRAM_LABEL_GAP 12.0f
#define DIAGRAM_BORDER 1.0f
#define ADVANCED_MARGIN 40.0f        /* Advanced under the diagram's line */
#define ADVANCED_PAD_X 12.0f         /* its highlight past its label, at the left too */
#define ADVANCED_PAD_Y 8.0f
#define ADVANCED_GAP 14.0f           /* between its label and its arrow */
#define ADVANCED_ARROW_W 9.0f
#define ADVANCED_ARROW_H 12.0f
#define MODS_ROW_GAP 6.0f            /* between the list's rows, the count and the note */
#define MODS_EMPTY_PAD_Y 14.0f
#define MODS_EMPTY_LINE 35.0f        /* 25 pixels at a line height of 1.4 */
#define MODS_NOTE_LINE 29.6875f      /* 22 pixels at 1.35, as the layout rounds it to 1/64 of a pixel */
#define MODS_SAVE_NOTE_LINE 29.0f    /* the Save row's note, as Cartridge's */
#define MODS_COUNT_PAD_TOP 4.0f
#define MODS_TOGGLE_COLUMN 72.0f
#define MODS_TOGGLE_W 64.0f
#define MODS_TOGGLE_H 34.0f
#define MODS_KNOB 26.0f
#define MODS_KNOB_INSET 4.0f         /* its border and 3 pixels */
#define MODS_KNOB_ON 30.0f
#define MODS_NAME_GAP 16.0f          /* between a mod's name and its games */

/* 4:3's geometry, in the 1440x1080 scene: the header from (64, 28) in a column 860 wide; the rows from (64, 208)
 * across 1312, each 100 tall at least, a grid of a label column and the values; Display's diagram at the top right. */
#define HEAD_X_4_3 64.0f
#define HEAD_Y_4_3 28.0f
#define HEAD_W_4_3 860.0f
#define HEAD_GAP_4_3 4.0f
#define HEAD_TITLE_LINE_4_3 63.0f         /* 60 px at 1.05 */
#define ROWS_X_4_3 64.0f
#define ROWS_Y_4_3 208.0f
#define ROWS_W_4_3 1312.0f
#define ROW_MIN_H_4_3 100.0f
#define ROW_GAP_4_3 8.0f                  /* between Cartridge's rows, and Mods' */
#define ROW_PAD_X_4_3 20.0f
#define ROW_PAD_Y_4_3 14.0f
#define LABEL_W_4_3 200.0f                /* Mods' labels */
#define CARTRIDGE_LABEL_W_4_3 310.0f      /* Cartridge's, as wide as European ROM */
#define EU_FOLDER_LINE_4_3 34.0f
#define EU_BUTTON_GAP_4_3 12.0f           /* between European ROM's buttons and their dots */
#define LANGUAGE_W_4_3 205.0f
#define LANGUAGE_MIN_H_4_3 90.0f
#define LANGUAGE_PAD_TOP_4_3 8.0f          /* Language's row, around its label, its frames and its line */
#define LANGUAGE_PAD_BOTTOM_4_3 10.0f
#define LANGUAGE_LINE_GAP_4_3 8.0f
#define LANGUAGE_LINE_4_3 38.0f
#define GRID_GAP_4_3 24.0f
#define GRID_ROW_GAP_4_3 2.0f
#define BUTTON_GAP_4_3 14.0f
#define DOT_4_3 14.0f
#define DOT_GAP_4_3 12.0f
#define IMAGE_GAP_4_3 4.0f                /* between a patch's folder and the image's line */
#define PLAY_GAP_4_3 24.0f
#define DISPLAY_ROW_GAP_4_3 6.0f
#define DISPLAY_LABEL_W_4_3 300.0f
#define DISPLAY_PAD_Y_4_3 5.0f
#define OPTION_MIN_W_4_3 110.0f
#define OPTION_MIN_H_4_3 90.0f
#define OPTION_PAD_X_4_3 22.0f
#define OPTION_PAD_Y_4_3 4.0f
#define OPTION_BORDER_4_3 2.0f
#define OPTION_GAP_4_3 12.0f
#define OPTION_NAME_LINE_4_3 (36.0f * 1.15f)
#define OPTION_SIZE_LINE_4_3 (27.0f * 1.2f)
#define QUALITY_PAD_X_4_3 12.0f           /* a quality's frame past its texts */
#define QUALITY_GAP_4_3 10.0f
#define ADVANCED_GAP_4_3 16.0f
#define ADVANCED_ARROW_W_4_3 13.0f
#define ADVANCED_ARROW_H_4_3 18.0f
#define ADVANCED_H_4_3 90.0f              /* the header's Advanced button, framed, beside the diagram */
#define ADVANCED_PAD_X_4_3 24.0f
#define ADVANCED_BORDER_4_3 2.0f
#define ADVANCED_MARGIN_4_3 24.0f         /* between it and the diagram */
#define DISPLAY_TITLE_GAP_4_3 24.0f       /* Display's title after its section's name */
#define HELP_LINE_4_3 47.0f               /* the explanation, on one line */
#define HELP_NOTE_LINE_4_3 40.0f          /* its note, on one line */
#define HELP_NOTE_GAP_4_3 4.0f
#define DISPLAY_PAGE_NOTE_LINE_4_3 33.0f       /* Display's note from a game, under the title */
#define DIAGRAM_RIGHT_4_3 (1440.0f - 64.0f)
#define DIAGRAM_H_4_3 122.0f
#define MODS_LABEL_PAD_4_3 34.0f
#define MODS_ROW_PAD_Y_4_3 10.0f
#define MODS_ROW_GAP_4_3 6.0f
#define MODS_TOGGLE_W_4_3 96.0f
#define MODS_TOGGLE_H_4_3 52.0f
#define MODS_KNOB_4_3 42.0f
#define MODS_KNOB_INSET_4_3 4.0f
#define MODS_KNOB_ON_4_3 42.0f
#define MODS_NAME_GAP_4_3 16.0f
#define MODS_EMPTY_PAD_4_3 14.0f
#define MODS_EMPTY_LINE_4_3 (30.0f * 1.35f)
#define MODS_NOTE_LINE_4_3 (27.0f * 1.35f)
#define MODS_COUNT_PAD_4_3 4.0f
#define MODS_FOLDER_LINES_4_3 2u

/* The rows' columns: the labels', the values' from VALUE_X_4_3 to RIGHT_4_3. */
#define LEFT_4_3 (ROWS_X_4_3 + ROW_PAD_X_4_3)
#define VALUE_X_4_3 (LEFT_4_3 + LABEL_W_4_3 + GRID_GAP_4_3)
#define GAME_VALUE_X_4_3 (LEFT_4_3 + CARTRIDGE_LABEL_W_4_3 + GRID_GAP_4_3)
#define RIGHT_4_3 (ROWS_X_4_3 + ROWS_W_4_3 - ROW_PAD_X_4_3)
#define DISPLAY_VALUE_X_4_3 (LEFT_4_3 + DISPLAY_LABEL_W_4_3 + GRID_GAP_4_3)

/* The helpers the layouts share (ui_page_layout.c): a line of `style` at (x, y), moved; a box; the page's head in
 * 16:9; the width of `length` digits; options framed, and choices at a row's right, moved; a text broken as text-wrap:
 * pretty breaks it, moved; the larger of two; JavaScript's Math.round; lines set on one baseline under `top`. */
OraclesUiLine pl_line_at(const OraclesUiTextStyle *style, const char *text, float x, float y, float line_height);
void pl_move_line(OraclesUiLine *line, float x, float y);
OraclesUiBox pl_box(float x, float y, float w, float h);
void pl_page_head_16_9(const char *section, const char *over, const char *title, const char *state, OraclesUiPageHead *out);
float pl_characters(const OraclesUiTextStyle *style, int length);
float pl_framed(const char *const *names, const char *const *sizes, const int *size_lengths, unsigned count, float x, float y,
                    OraclesUiOptionLayout *out);
void pl_choices(const char *const *names, const char *const *sizes, unsigned count, float right, float top, OraclesUiOptionLayout *out);
void pl_shift_option(OraclesUiOptionLayout *o, float dx, float dy);
void pl_wrap_pretty_lines(const OraclesUiTextStyle *style, const char *text, float width, float x, float y, float line_height,
                              OraclesUiWrapped *out);
void pl_move_wrapped(OraclesUiWrapped *w, float y);
float pl_max_f(float a, float b);
float pl_round_half_up(float v);
float pl_baseline_row(OraclesUiLine *const *lines, unsigned count, float top);
/* Buttons side by side from a row's right edge, unplaced vertically, returning their width; their boxes, from each label
 * to its dot.  Cartridge's Save row (ui_page_save_layout.c) from the top of its row `y`, returning its box: in 16:9 in the
 * panel's values from `value_x`, `value_w` wide. */
float pl_buttons(const OraclesUiTextStyle *style, const char *const *labels, unsigned count, float right, float gap, float between,
                 OraclesUiLine *texts, OraclesUiLine *dots);
void pl_button_boxes(unsigned count, const OraclesUiLine *texts, const OraclesUiLine *dots, OraclesUiBox *boxes);
OraclesUiBox pl_save_row_16_9(OraclesUiGameTexts *t, float value_x, float value_w, float y, OraclesUiGameLayout *out);
OraclesUiBox pl_save_row_4_3(OraclesUiGameTexts *t, float y, OraclesUiGameLayout *out);

/* 4:3's: the room a file's value has beside its button; a button placed at the rows' right; a row's box; the Play row with
 * its note. */
float pl_file_room_4_3(float value_x, float button_w);
float pl_place_button_4_3(const char *label, OraclesUiLine *text, OraclesUiLine *dot);
OraclesUiBox pl_play_4_3_row(const char *note, float y, OraclesUiLine *note_out, OraclesUiLine *play, OraclesUiLine *dot);
OraclesUiBox pl_row_4_3(float y, float bottom);

/* A button and its dot at a row's right; a text wrapped at `width` in lines of `line_height`; the page's head in 4:3. */
void pl_button(const char *label, float right, float top, float height, OraclesUiLine *text, OraclesUiLine *dot);
void pl_wrap_lines(const OraclesUiTextStyle *style, const char *text, float width, float x, float y, float line_height, OraclesUiWrapped *out);
void pl_page_head_4_3(const char *section, const char *over, const char *title, const char *state, OraclesUiPageHead *out);

#endif
