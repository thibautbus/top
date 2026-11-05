/* Cartridge's Save row in each layout, and the import's confirmation over the page (ui_page_layout.h). */
#include "ui_page_layout_internal.h"

#include <string.h>

#define SAVE_ACTIONS_GAP 20.0f          /* between Open folder and Import save…, as European ROM's buttons */
#define SAVE_NOTE_LINE 29.0f            /* the note's lines */
#define SAVE_ACTIONS_GAP_4_3 24.0f
#define SAVE_ACTION_DOT_GAP_4_3 12.0f   /* between an action and its dot */

/* The confirmation: its panel's width, padding and border, the gap between its parts and above its buttons, the
 * question's line, the buttons' least width and height, padding, border and gap; 16:9's, then 4:3's. */
typedef struct import_metrics {
    float scene_w, panel_w, pad_x, pad_y, gap, buttons_margin, question_line, button_min_w, button_min_h, button_pad_x, button_pad_y,
          button_border, button_gap;
} import_metrics;
static const import_metrics import_16_9 = { 1920.0f, 880.0f, 40.0f, 32.0f, 16.0f, 6.0f, 26.0f * 1.35f, 150.0f, 0.0f, 22.0f, 6.0f, 1.0f, 12.0f };
static const import_metrics import_4_3 = { 1440.0f, 1040.0f, 44.0f, 36.0f, 18.0f, 8.0f, 33.0f * 1.35f, 200.0f, 90.0f, 28.0f, 0.0f, 2.0f, 16.0f };
#define IMPORT_PANEL_BORDER 1.0f
#define SCENE_H 1080.0f

const char oracles_ui_import_title[] = "Import save";
static const char replace_question[] = "Replace this game's save? The current one is kept as a backup.";
static const char take_question[] = "Use this file as this game's save?";
static const char mods_question[] = "Replace this game's save, with mods too? What it replaces is kept as a backup.";

static const OraclesUiTextStyle import_title_4_3 = { ORACLES_UI_FONT_SERIF, 30.0f, 0.2f, 1 };
static const OraclesUiTextStyle import_question = { ORACLES_UI_FONT_SANS, 26.0f, 0.0f, 0 };
static const OraclesUiTextStyle import_question_4_3 = { ORACLES_UI_FONT_SANS, 33.0f, 0.0f, 0 };
static const OraclesUiTextStyle import_button = { ORACLES_UI_FONT_SERIF, 26.0f, 0.0f, 0 };

const OraclesUiTextStyle *oracles_ui_import_style(OraclesUiLayout layout, int part)
{
    const int four_three = layout == ORACLES_UI_LAYOUT_4_3;
    if (part == 0) return four_three ? &import_title_4_3 : &oracles_ui_page_section;
    if (part == 1) return four_three ? &import_question_4_3 : &import_question;
    return four_three ? &oracles_ui_row_button_4_3 : &import_button;
}

/* The status beside its dot from `y`, wrapped in `width` from `x`, its second line under it; the dot centred on the
 * first line (16:9) or on its baseline (4:3).  Returns its bottom. */
static float status_at(const OraclesUiTextStyle *style, const OraclesUiGameTexts *t, float x, float y, float width, float dot, float dot_gap,
                       int on_baseline, OraclesUiGameLayout *out)
{
    oracles_ui_wrap_pretty(style, t->save_status, width - dot - dot_gap, x + dot + dot_gap, y, &out->save_status);
    const OraclesUiLine *first = &out->save_status.lines[0];
    out->save_dot = pl_box(x, on_baseline ? first->baseline - dot : y + (first->h - dot) * 0.5f, dot, dot);
    y += out->save_status.h;
    if (t->save_status2) {
        oracles_ui_wrap_pretty(style, t->save_status2, width - dot - dot_gap, x + dot + dot_gap, y, &out->save_status2);
        y += out->save_status2.h;
    }
    return y;
}

OraclesUiBox pl_save_row_16_9(OraclesUiGameTexts *t, float value_x, float value_w, float y, OraclesUiGameLayout *out)
{
    const float inner_x = value_x + ROW_PAD_X, inner_w = value_w - 2.0f * ROW_PAD_X, inner_y = y + ROW_PAD_Y;
    char fitted[ORACLES_HOME_TEXT_LENGTH];
    /* The file and the actions centred on the first line, the file in the room the actions leave. */
    const float actions = pl_buttons(&oracles_ui_row_button, t->save_actions, ORACLES_PAGE_SAVE_ACTIONS, inner_x + inner_w, BUTTON_GAP,
                                     SAVE_ACTIONS_GAP, out->save_buttons, out->save_button_dots);
    oracles_ui_fit(&oracles_ui_row_title, t->save_file, inner_w - ROW_GAP - actions, fitted, sizeof t->save_file);
    memcpy(t->save_file, fitted, sizeof t->save_file);
    oracles_ui_fit(&oracles_ui_row_text, t->save_line, inner_w, fitted, sizeof t->save_line);
    memcpy(t->save_line, fitted, sizeof t->save_line);
    out->save_file = pl_line_at(&oracles_ui_row_title, t->save_file, inner_x, 0.0f, 0.0f);
    const float first = pl_max_f(out->save_file.h, out->save_buttons[0].h);
    pl_move_line(&out->save_file, inner_x, inner_y + (first - out->save_file.h) * 0.5f);
    for (unsigned i = 0; i < ORACLES_PAGE_SAVE_ACTIONS; i++) {
        const float top = inner_y + (first - out->save_buttons[i].h) * 0.5f;
        pl_move_line(&out->save_buttons[i], out->save_buttons[i].x, top);
        pl_move_line(&out->save_button_dots[i], out->save_button_dots[i].x, top);
    }
    pl_button_boxes(ORACLES_PAGE_SAVE_ACTIONS, out->save_buttons, out->save_button_dots, out->save_button_boxes);
    /* The line across the row, then the status or the note. */
    out->save_line = pl_line_at(&oracles_ui_row_text, t->save_line, inner_x, inner_y + first + FILE_GAP, 0.0f);
    float bottom = out->save_line.y + out->save_line.h;
    if (t->save_status) {
        bottom = status_at(&oracles_ui_row_status, t, inner_x, bottom + FILE_GAP, inner_w, DOT, DOT_GAP, 0, out);
    } else if (t->save_note && t->save_note[0]) {
        pl_wrap_pretty_lines(&oracles_ui_row_note, t->save_note, inner_w, inner_x, bottom + FILE_GAP, SAVE_NOTE_LINE, &out->save_note);
        bottom += FILE_GAP + out->save_note.h;
    }
    return pl_box(value_x, y, value_w, bottom + ROW_PAD_Y - y);
}

OraclesUiBox pl_save_row_4_3(OraclesUiGameTexts *t, float y, OraclesUiGameLayout *out)
{
    const float span = RIGHT_4_3 - GAME_VALUE_X_4_3;
    char fitted[ORACLES_HOME_TEXT_LENGTH];
    const float actions = pl_buttons(&oracles_ui_row_button_4_3, t->save_actions, ORACLES_PAGE_SAVE_ACTIONS, RIGHT_4_3, SAVE_ACTION_DOT_GAP_4_3,
                                     SAVE_ACTIONS_GAP_4_3, out->save_buttons, out->save_button_dots);
    oracles_ui_fit(&oracles_ui_row_title_4_3, t->save_file, pl_file_room_4_3(GAME_VALUE_X_4_3, actions), fitted, sizeof t->save_file);
    memcpy(t->save_file, fitted, sizeof t->save_file);
    out->label_save = pl_line_at(&oracles_ui_row_label_4_3, "Save", LEFT_4_3, 0.0f, 0.0f);
    out->save_file = pl_line_at(&oracles_ui_row_title_4_3, t->save_file, GAME_VALUE_X_4_3, 0.0f, 0.0f);
    OraclesUiLine *const lines[6] = { &out->label_save, &out->save_file, &out->save_buttons[0], &out->save_button_dots[0],
                                      &out->save_buttons[1], &out->save_button_dots[1] };
    float bottom = pl_baseline_row(lines, 6, y + ROW_PAD_Y_4_3);
    pl_button_boxes(ORACLES_PAGE_SAVE_ACTIONS, out->save_buttons, out->save_button_dots, out->save_button_boxes);
    if (t->save_status) {
        bottom = status_at(&oracles_ui_row_text_4_3, t, GAME_VALUE_X_4_3, bottom + GRID_ROW_GAP_4_3, span, DOT_4_3, DOT_GAP_4_3, 1, out);
    } else {
        oracles_ui_fit(&oracles_ui_row_text_4_3, t->save_line, span, fitted, sizeof t->save_line);
        memcpy(t->save_line, fitted, sizeof t->save_line);
        out->save_line = pl_line_at(&oracles_ui_row_text_4_3, t->save_line, GAME_VALUE_X_4_3, bottom + GRID_ROW_GAP_4_3, 0.0f);
        bottom = out->save_line.y + out->save_line.h;
        if (t->save_note && t->save_note[0]) {
            /* The note on one line, shortened where it does not hold. */
            memset(&out->save_note, 0, sizeof out->save_note);
            oracles_ui_fit(&oracles_ui_page_note_4_3, t->save_note, span, out->save_note.text[0], sizeof out->save_note.text[0]);
            out->save_note.count = 1;
            out->save_note.lines[0] = pl_line_at(&oracles_ui_page_note_4_3, out->save_note.text[0], GAME_VALUE_X_4_3, bottom + GRID_ROW_GAP_4_3, 0.0f);
            out->save_note.w = out->save_note.lines[0].w;
            out->save_note.h = out->save_note.lines[0].h;
            bottom += GRID_ROW_GAP_4_3 + out->save_note.h;
        }
    }
    return pl_row_4_3(y, bottom);
}

void oracles_ui_layout_import(OraclesUiLayout layout, const char *file, const char *folder, int replace, OraclesUiImportLayout *out)
{
    memset(out, 0, sizeof *out);
    const int four_three = layout == ORACLES_UI_LAYOUT_4_3;
    const import_metrics *m = four_three ? &import_4_3 : &import_16_9;
    const OraclesUiTextStyle *title = oracles_ui_import_style(layout, 0), *question = oracles_ui_import_style(layout, 1),
                             *button = oracles_ui_import_style(layout, 2);
    const OraclesUiTextStyle *file_style = four_three ? &oracles_ui_row_title_4_3 : &oracles_ui_row_title;
    const OraclesUiTextStyle *path_style = four_three ? &oracles_ui_row_path_4_3 : &oracles_ui_row_path;
    out->backdrop = pl_box(0.0f, 0.0f, m->scene_w, SCENE_H);
    /* The parts in a column from the panel's top, then the panel centred on them. */
    const float x = (m->scene_w - m->panel_w) * 0.5f + IMPORT_PANEL_BORDER + m->pad_x;
    const float width = m->panel_w - 2.0f * (IMPORT_PANEL_BORDER + m->pad_x);
    oracles_ui_fit(file_style, file, width, out->file_text, sizeof out->file_text);
    oracles_ui_fit(path_style, folder, width, out->folder_text, sizeof out->folder_text);
    float y = IMPORT_PANEL_BORDER + m->pad_y;
    out->title = pl_line_at(title, oracles_ui_import_title, x, y, 0.0f);
    y += out->title.h + m->gap;
    out->file = pl_line_at(file_style, out->file_text, x, y, 0.0f);
    y += out->file.h + m->gap;
    if (out->folder_text[0]) {   /* a document whose address names no folder (Android): no line */
        out->folder = pl_line_at(path_style, out->folder_text, x, y, 0.0f);
        y += out->folder.h + m->gap;
    }
    out->question_text = replace == 2 ? mods_question : replace ? replace_question : take_question;
    out->label_texts[0] = replace ? "Replace" : "Import";
    out->label_texts[1] = "Cancel";
    pl_wrap_pretty_lines(question, out->question_text, width, x, y, m->question_line, &out->question);
    y += out->question.h + m->gap + m->buttons_margin;
    /* The buttons at the right, each its label centred in its frame. */
    float right = x + width;
    float buttons_h = 0.0f;
    for (unsigned i = 2; i-- > 0;) {
        out->labels[i] = pl_line_at(button, out->label_texts[i], 0.0f, 0.0f, 0.0f);
        const float w = pl_max_f(m->button_min_w, out->labels[i].w + 2.0f * (m->button_pad_x + m->button_border));
        const float h = pl_max_f(m->button_min_h, out->labels[i].h + 2.0f * (m->button_pad_y + m->button_border));
        out->buttons[i] = pl_box(right - w, y, w, h);
        pl_move_line(&out->labels[i], right - w + (w - out->labels[i].w) * 0.5f, y + (h - out->labels[i].h) * 0.5f);
        buttons_h = pl_max_f(buttons_h, h);
        right -= w + m->button_gap;
    }
    const float h = y + buttons_h + m->pad_y + IMPORT_PANEL_BORDER, top = (SCENE_H - h) * 0.5f;
    out->panel = pl_box((m->scene_w - m->panel_w) * 0.5f, top, m->panel_w, h);
    pl_move_line(&out->title, x, out->title.y + top);
    pl_move_line(&out->file, x, out->file.y + top);
    if (out->folder_text[0]) pl_move_line(&out->folder, x, out->folder.y + top);
    pl_move_wrapped(&out->question, out->question.lines[0].y + top);
    for (unsigned i = 0; i < 2u; i++) {
        out->buttons[i].y += top;
        pl_move_line(&out->labels[i], out->labels[i].x, out->labels[i].y + top);
    }
}
