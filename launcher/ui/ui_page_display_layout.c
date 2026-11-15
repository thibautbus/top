/* Display's layout (ui_page_layout.h), without SDL: its rows, its diagram and its Advanced rows in 16:9 and in 4:3, and
 * the texts they show. */
#include "ui_page_layout_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* A row of Display with choices: its explanation wrapped beside them, `explanation_h` tall at least (its lines from the
 * top), with its italic notes under it if any, both centred on the row. Returns the row's height. */
float oracles_ui_display_row(float x, float y, float w, const char *label, float label_x, const char *explanation, float explanation_h,
                             const char *const *notes, int note_count, const char *const *names, const char *const *sizes, unsigned count,
                             OraclesUiWrapped *label_out, OraclesUiWrapped *explanation_out, OraclesUiLine *const *notes_out,
                             OraclesUiOptionLayout *options, OraclesUiBox *row)
{
    oracles_ui_wrap(&oracles_ui_row_label, label, DISPLAY_LABEL_W, label_x, y + LABEL_PAD_TOP, label_out);
    const float inner_x = x + ROW_PAD_X, inner_y = y + DISPLAY_ROW_PAD_Y, right = x + w - ROW_PAD_X;
    pl_choices(names, sizes, count, right, inner_y, options);
    const float options_w = right - options[0].box.x;
    oracles_ui_wrap(&oracles_ui_row_text, explanation, right - inner_x - ROW_GAP - options_w, inner_x, inner_y, explanation_out);
    const float reserved = explanation_out->h > explanation_h ? explanation_out->h : explanation_h;
    float text_h = reserved;
    for (int i = 0; i < note_count; i++) {
        *notes_out[i] = pl_line_at(&oracles_ui_row_note, notes[i], inner_x, 0.0f, 0.0f);
        text_h += NOTE_GAP + notes_out[i]->h;
    }
    const float inner_h = text_h > options[0].box.h ? text_h : options[0].box.h;
    pl_move_wrapped(explanation_out, inner_y + (inner_h - text_h) * 0.5f);
    float note_y = explanation_out->lines[0].y + reserved;
    for (int i = 0; i < note_count; i++) {
        pl_move_line(notes_out[i], inner_x, note_y + NOTE_GAP);
        note_y = notes_out[i]->y + notes_out[i]->h;
    }
    const float dy = (inner_h - options[0].box.h) * 0.5f;
    for (unsigned i = 0; i < count; i++) {
        options[i].box.y += dy;
        pl_move_line(&options[i].name, options[i].name.x, options[i].name.y + dy);
        if (sizes) pl_move_line(&options[i].size, options[i].size.x, options[i].size.y + dy);
    }
    *row = pl_box(x, y, w, inner_h + 2.0f * DISPLAY_ROW_PAD_Y);
    const float label_h = LABEL_PAD_TOP + label_out->h;
    return label_h > row->h ? label_h : row->h;
}

const OraclesUiTextStyle oracles_ui_panels_choice = { ORACLES_UI_FONT_SERIF, 22.0f, 0.0f, 0 };

/* 16:9's HUD row: its four choices by their short names; with Floating, Panels' line under them, its label and its two
 * choices at the right, its highlight reaching PANELS_PAD_X past them, the row's margins narrower.  Returns the row's
 * height. */
static float hud_row_16_9(const OraclesUiDisplayTexts *t, float x, float y, float w, float label_x, OraclesUiDisplayLayout *out)
{
    const unsigned r = ORACLES_DISPLAY_HUD;
    if (!t->floating)
        return oracles_ui_display_row(x, y, w, oracles_display_labels[r], label_x, t->explanations[6], 0.0f, NULL, 0, oracles_display_hud_short, NULL,
                                      ORACLES_DISPLAY_HUDS, &out->labels[r], &out->explanations[6], NULL, out->hud, &out->rows[r]);
    oracles_ui_wrap(&oracles_ui_row_label, oracles_display_labels[r], DISPLAY_LABEL_W, label_x, y + LABEL_PAD_TOP, &out->labels[r]);
    const float inner_x = x + ROW_PAD_X, inner_y = y + HUD_FLOATING_PAD_Y, right = x + w - ROW_PAD_X;
    pl_choices(oracles_display_hud_short, NULL, ORACLES_DISPLAY_HUDS, right, inner_y, out->hud);
    /* Panels' choices from the right, then its label. */
    float px = right, line_h = 0.0f;
    for (int i = 1; i >= 0; i--) {
        OraclesUiOptionLayout *o = &out->panels[i];
        memset(&o->size, 0, sizeof o->size);
        o->name = pl_line_at(&oracles_ui_panels_choice, oracles_display_panels_choices[i], 0.0f, 0.0f, 0.0f);
        o->box.w = o->name.w + 2.0f * (PANELS_CHOICE_PAD_X + BORDER);
        o->box.h = o->name.h + 2.0f * (PANELS_CHOICE_PAD_Y + BORDER);
        o->box.x = px - o->box.w;
        px = o->box.x - PANELS_CHOICE_GAP;
        line_h = pl_max_f(line_h, o->box.h);
    }
    out->panels_label = pl_line_at(&oracles_ui_diagram_label, oracles_display_labels[ORACLES_DISPLAY_PANELS], 0.0f, 0.0f, 0.0f);
    line_h = pl_max_f(line_h, out->panels_label.h);
    const float line_y = inner_y + out->hud[0].box.h + PANELS_LINE_GAP;
    for (int i = 0; i < 2; i++) {
        OraclesUiOptionLayout *o = &out->panels[i];
        o->box.y = line_y + (line_h - o->box.h) * 0.5f;
        pl_move_line(&o->name, o->box.x + BORDER + PANELS_CHOICE_PAD_X, o->box.y + BORDER + PANELS_CHOICE_PAD_Y);
    }
    const float label_x_at = out->panels[0].box.x - PANELS_LABEL_GAP - out->panels_label.w;
    pl_move_line(&out->panels_label, label_x_at, line_y + (line_h - out->panels_label.h) * 0.5f);
    out->rows[ORACLES_DISPLAY_PANELS] = pl_box(label_x_at - PANELS_PAD_X, line_y, right + PANELS_PAD_X - (label_x_at - PANELS_PAD_X), line_h);
    /* The explanation beside the two lines, wrapped in the room the wider of them leaves. */
    const float column_w = right - fminf(out->hud[0].box.x, label_x_at - PANELS_PAD_X);
    oracles_ui_wrap(&oracles_ui_row_text, t->explanations[6], right - inner_x - ROW_GAP - column_w, inner_x, inner_y, &out->explanations[6]);
    const float choices_h = out->hud[0].box.h + PANELS_LINE_GAP + line_h, inner_h = pl_max_f(choices_h, out->explanations[6].h);
    pl_move_wrapped(&out->explanations[6], inner_y + (inner_h - out->explanations[6].h) * 0.5f);
    const float dy = (inner_h - choices_h) * 0.5f;
    for (int i = 0; i < ORACLES_DISPLAY_HUDS; i++) pl_shift_option(&out->hud[i], 0.0f, dy);
    for (int i = 0; i < 2; i++) pl_shift_option(&out->panels[i], 0.0f, dy);
    pl_move_line(&out->panels_label, out->panels_label.x, out->panels_label.y + dy);
    out->rows[ORACLES_DISPLAY_PANELS].y += dy;
    out->rows[r] = pl_box(x, y, w, inner_h + 2.0f * HUD_FLOATING_PAD_Y);
    return pl_max_f(LABEL_PAD_TOP + out->labels[r].h, out->rows[r].h);
}

/* Display › Picture's Filter: its value framed from (x, y), an arrow's place either side of its name and of the eight
 * marks under it, all centred in it; 16:9's measures or 4:3's. */
typedef struct FilterStep {
    const OraclesUiTextStyle *style;
    float name_line, border, pad_x, pad_y, min_h, gap, arrow_w, arrow_h, triangle_w, triangle_h, middle_w, name_gap, marks_h, mark, mark_gap;
} FilterStep;
static const FilterStep step_16_9 = { &oracles_ui_choice, 0.0f, 1.0f, 4.0f, 3.0f, 0.0f, 2.0f, 32.0f, 50.0f, 9.0f, 12.0f, 132.0f, 2.0f, 14.0f, 8.0f, 5.0f };
static const FilterStep step_4_3 = { &oracles_ui_row_button_4_3, OPTION_NAME_LINE_4_3, 2.0f, 6.0f, 4.0f, OPTION_MIN_H_4_3, 2.0f, 48.0f, 76.0f, 13.0f, 18.0f,
                                     190.0f, 4.0f, 20.0f, 12.0f, 8.0f };

static void filter_step(const FilterStep *k, const char *name, float x, float y, OraclesUiDisplayLayout *out)
{
    out->filter_name = pl_line_at(k->style, name, 0.0f, 0.0f, k->name_line);
    const float middle_w = pl_max_f(k->middle_w, out->filter_name.w), middle_h = out->filter_name.h + k->name_gap + k->marks_h;
    const float w = 2.0f * (k->border + k->pad_x + k->arrow_w + k->gap) + middle_w;
    const float h = pl_max_f(k->min_h, pl_max_f(k->arrow_h, middle_h) + 2.0f * (k->border + k->pad_y));
    out->filter_value = pl_box(x, y, w, h);
    const float centre = y + h * 0.5f, left = x + k->border + k->pad_x, middle_x = left + k->arrow_w + k->gap;
    out->filter_prev = pl_box(left, centre - k->arrow_h * 0.5f, k->arrow_w, k->arrow_h);
    out->filter_next = pl_box(middle_x + middle_w + k->gap, centre - k->arrow_h * 0.5f, k->arrow_w, k->arrow_h);
    out->filter_prev_arrow = pl_box(left + (k->arrow_w - k->triangle_w) * 0.5f, centre - k->triangle_h * 0.5f, k->triangle_w, k->triangle_h);
    out->filter_next_arrow = pl_box(out->filter_next.x + (k->arrow_w - k->triangle_w) * 0.5f, centre - k->triangle_h * 0.5f, k->triangle_w, k->triangle_h);
    const float top = centre - middle_h * 0.5f;
    pl_move_line(&out->filter_name, middle_x + (middle_w - out->filter_name.w) * 0.5f, top);
    const float marks_w = (float)ORACLES_DISPLAY_FILTERS * k->mark + (float)(ORACLES_DISPLAY_FILTERS - 1) * k->mark_gap;
    const float mark_y = top + out->filter_name.h + k->name_gap + (k->marks_h - k->mark) * 0.5f;
    for (int i = 0; i < ORACLES_DISPLAY_FILTERS; i++)
        out->filter_marks[i] = pl_box(middle_x + (middle_w - marks_w) * 0.5f + (float)i * (k->mark + k->mark_gap), mark_y, k->mark, k->mark);
}

/* 16:9's Filter row: its explanation, two lines kept, and its note beside the value at the row's right.  Returns the
 * row's height. */
static float filter_row_16_9(const OraclesUiDisplayTexts *t, float x, float y, float w, float label_x, OraclesUiDisplayLayout *out)
{
    const unsigned r = ORACLES_DISPLAY_FILTER;
    oracles_ui_wrap(&oracles_ui_row_label, oracles_display_labels[r], DISPLAY_LABEL_W, label_x, y + LABEL_PAD_TOP, &out->labels[r]);
    const float inner_x = x + ROW_PAD_X, inner_y = y + DISPLAY_ROW_PAD_Y, right = x + w - ROW_PAD_X;
    const char *name = oracles_display_filter_names[t->filter];
    filter_step(&step_16_9, name, 0.0f, 0.0f, out);
    const float step_w = out->filter_value.w, step_h = out->filter_value.h;
    oracles_ui_wrap(&oracles_ui_row_text, t->filter_explanation, right - inner_x - ROW_GAP - step_w, inner_x, inner_y, &out->filter_explanation);
    const float reserved = pl_max_f(out->filter_explanation.h, ADVANCED_EXPLANATION_H);
    float text_h = reserved;
    if (t->filter_note && t->filter_note[0]) {
        out->filter_note = pl_line_at(&oracles_ui_row_note, t->filter_note, inner_x, 0.0f, 0.0f);
        text_h += NOTE_GAP + out->filter_note.h;
    }
    const float inner_h = pl_max_f(text_h, step_h);
    pl_move_wrapped(&out->filter_explanation, inner_y + (inner_h - text_h) * 0.5f);
    if (out->filter_note.h > 0.0f) pl_move_line(&out->filter_note, inner_x, out->filter_explanation.lines[0].y + reserved + NOTE_GAP);
    filter_step(&step_16_9, name, right - step_w, inner_y + (inner_h - step_h) * 0.5f, out);
    out->rows[r] = pl_box(x, y, w, inner_h + 2.0f * DISPLAY_ROW_PAD_Y);
    return pl_max_f(LABEL_PAD_TOP + out->labels[r].h, out->rows[r].h);
}

static void display_16_9(const OraclesUiDisplayTexts *t, OraclesUiDisplayLayout *out)
{
    pl_page_head_16_9(t->section ? t->section : "Display", t->over, t->title, NULL, &out->head);
    const float diagram_w = t->diagram_box_w > 0.0f ? t->diagram_box_w : ORACLES_UI_DIAGRAM_W;
    float head_bottom = out->head.title.lines[0].y + out->head.title.h;
    /* Opened from a game, under the title: what only the next session takes, said once for the page. */
    if (t->later) {
        pl_wrap_pretty_lines(&oracles_ui_row_note, oracles_ui_display_later, HEAD_W, HEAD_X, head_bottom + DISPLAY_PAGE_NOTE_GAP, DISPLAY_PAGE_NOTE_LINE,
                          &out->page_note);
        head_bottom += DISPLAY_PAGE_NOTE_GAP + out->page_note.h;
    }
    const float left = PANEL_X + PANEL_BORDER + PANEL_PAD_X, width = PANEL_W - 2.0f * (PANEL_BORDER + PANEL_PAD_X);
    const float value_x = left + DISPLAY_LABEL_W + GRID_GAP, value_w = width - DISPLAY_LABEL_W - GRID_GAP;
    const float inner_x = value_x + ROW_PAD_X;
    float y = PANEL_Y + PANEL_BORDER + PANEL_PAD_Y;
    /* Display › Text: its preview in the diagram's place, its rows in the panel. */
    if (t->text_page) {
        out->link = ORACLES_DISPLAY_ROWS;
        y = oracles_ui_text_page_16_9(t, head_bottom + DIAGRAM_MARGIN, value_x, y, value_w, left, out);
        out->panel = pl_box(PANEL_X, PANEL_Y, PANEL_W, y + PANEL_PAD_Y + PANEL_BORDER - PANEL_Y);
        return;
    }
    out->link = t->advanced || t->picture ? ORACLES_DISPLAY_ROWS : ORACLES_DISPLAY_ADVANCED;
    /* While Filter has the focus, the preview and its caption in the diagram's place, which goes with its lines. */
    if (t->filter_preview) {
        out->filter_preview = pl_box(HEAD_X, head_bottom + DIAGRAM_MARGIN, ORACLES_UI_FILTER_PREVIEW_W, ORACLES_UI_FILTER_PREVIEW_H);
        oracles_ui_wrap(&oracles_ui_diagram_label, t->filter_caption, HEAD_W, HEAD_X, out->filter_preview.y + out->filter_preview.h + DIAGRAM_LABEL_GAP,
                        &out->filter_caption);
    }
    out->diagram = pl_box(HEAD_X, head_bottom + DIAGRAM_MARGIN, diagram_w, ORACLES_UI_DIAGRAM_H);
    /* A window as wide as the screen (Fill) stays inside the box's border across, over it top and bottom. */
    const float window_w = t->diagram_w < diagram_w - 2.0f * DIAGRAM_BORDER ? t->diagram_w : diagram_w - 2.0f * DIAGRAM_BORDER;
    out->diagram_window = pl_box(out->diagram.x + (diagram_w - window_w) * 0.5f, out->diagram.y + (ORACLES_UI_DIAGRAM_H - t->diagram_h) * 0.5f,
                              window_w, t->diagram_h);
    out->diagram_label = pl_line_at(&oracles_ui_diagram_label, t->diagram_label, HEAD_X, out->diagram.y + out->diagram.h + DIAGRAM_LABEL_GAP, 0.0f);
    /* Window's note under it, and the line on the window's size, italic, two lines kept: that it is reduced, or else
     * that the sizes follow the view; the Advanced rows keep them. */
    const float note_y = out->diagram_label.y + out->diagram_label.h + DIAGRAM_LABEL_GAP;
    pl_wrap_pretty_lines(&oracles_ui_diagram_label, t->window_note, HEAD_W, HEAD_X, note_y, WINDOW_NOTE_LINE, &out->window_note);
    const float fit_y = note_y + out->window_note.h + DIAGRAM_LABEL_GAP;
    pl_wrap_pretty_lines(&oracles_ui_row_note, t->window_fit, HEAD_W, HEAD_X, fit_y, WINDOW_NOTE_LINE, &out->window_reduced);
    const float fit_bottom = fit_y + fmaxf(out->window_reduced.h, WINDOW_FIT_H);
    /* Advanced under them, its highlight reaching past its label at the left as at the right, the arrow after the label,
     * both centred on it; not on the Advanced rows nor on Picture's. */
    if (out->link < ORACLES_DISPLAY_ROWS) {
        const float y = fit_bottom + ADVANCED_MARGIN;
        out->advanced_label = pl_line_at(&oracles_ui_row_label, oracles_display_labels[out->link], HEAD_X, y + ADVANCED_PAD_Y, 0.0f);
        const float inner_h = out->advanced_label.h > ADVANCED_ARROW_H ? out->advanced_label.h : ADVANCED_ARROW_H;
        pl_move_line(&out->advanced_label, HEAD_X, y + ADVANCED_PAD_Y + (inner_h - out->advanced_label.h) * 0.5f);
        out->advanced_arrow = pl_box(HEAD_X + out->advanced_label.w + ADVANCED_GAP, y + ADVANCED_PAD_Y + (inner_h - ADVANCED_ARROW_H) * 0.5f,
                                  ADVANCED_ARROW_W, ADVANCED_ARROW_H);
        out->rows[out->link] = pl_box(HEAD_X - ADVANCED_PAD_X, y, out->advanced_arrow.x + ADVANCED_ARROW_W + ADVANCED_PAD_X - (HEAD_X - ADVANCED_PAD_X),
                                      inner_h + 2.0f * ADVANCED_PAD_Y);
    }

    if (t->picture) {
        /* Display › Picture, two lines kept for each explanation: the scaling (each with the image's size in fullscreen,
         * and its note), the screen margin, color correction, Far size where it changes something, Menus & scenes and
         * Unvisited rooms, the last three with their notes. */
        const char *scaling_sizes[2];
        for (int i = 0; i < 2; i++) scaling_sizes[i] = t->scaling_sizes[i];
        const char *const scaling_notes[1] = { oracles_ui_scaling_note };
        OraclesUiLine *const scaling_out[1] = { &out->scaling_note };
        y += oracles_ui_display_row(value_x, y, value_w, oracles_display_labels[ORACLES_DISPLAY_SCALING], left, t->explanations[5], ADVANCED_EXPLANATION_H,
                         scaling_notes, 1, oracles_display_scaling_choices, scaling_sizes, 2, &out->labels[ORACLES_DISPLAY_SCALING], &out->explanations[5],
                         scaling_out, out->scaling, &out->rows[ORACLES_DISPLAY_SCALING]) + DISPLAY_ROW_GAP;
        y += filter_row_16_9(t, value_x, y, value_w, left, out) + DISPLAY_ROW_GAP;
        /* Screen margin: its explanation too long beside four choices, they stand above it from the row's left, as
         * Quality's do, the explanation under them on the row's width, two lines kept. */
        oracles_ui_wrap(&oracles_ui_row_label, oracles_display_labels[ORACLES_DISPLAY_MARGIN], DISPLAY_LABEL_W, left, y + LABEL_PAD_TOP,
                        &out->labels[ORACLES_DISPLAY_MARGIN]);
        const float right = value_x + value_w - ROW_PAD_X, inner_y = y + MARGIN_ROW_PAD_Y;
        pl_choices(oracles_display_margin_choices, NULL, ORACLES_DISPLAY_MARGINS, right, inner_y, out->margin);
        const float from_left = inner_x - out->margin[0].box.x;
        for (int m = 0; m < ORACLES_DISPLAY_MARGINS; m++) pl_shift_option(&out->margin[m], from_left, 0.0f);
        oracles_ui_wrap(&oracles_ui_row_text, t->explanations[7], right - inner_x, inner_x, inner_y + out->margin[0].box.h + MARGIN_ROW_GAP,
                        &out->explanations[7]);
        const float explanation_h = out->explanations[7].h > ADVANCED_EXPLANATION_H ? out->explanations[7].h : ADVANCED_EXPLANATION_H;
        out->rows[ORACLES_DISPLAY_MARGIN] = pl_box(value_x, y, value_w, out->margin[0].box.h + MARGIN_ROW_GAP + explanation_h + 2.0f * MARGIN_ROW_PAD_Y);
        const float label_h = LABEL_PAD_TOP + out->labels[ORACLES_DISPLAY_MARGIN].h;
        y += (label_h > out->rows[ORACLES_DISPLAY_MARGIN].h ? label_h : out->rows[ORACLES_DISPLAY_MARGIN].h) + DISPLAY_ROW_GAP;
        y += oracles_ui_display_row(value_x, y, value_w, oracles_display_labels[ORACLES_DISPLAY_COLOUR], left, t->explanations[0], ADVANCED_EXPLANATION_H,
                         NULL, 0, oracles_display_colour_choices, NULL, 2, &out->labels[ORACLES_DISPLAY_COLOUR], &out->explanations[0], NULL, out->colour,
                         &out->rows[ORACLES_DISPLAY_COLOUR]);
        static const unsigned size_rows[3] = { ORACLES_DISPLAY_FAR_SIZE, ORACLES_DISPLAY_MENUS, ORACLES_DISPLAY_UNVISITED };
        const char *const *const size_choices[3] = { oracles_display_far_size_choices, oracles_display_menus_choices, oracles_display_unvisited_choices };
        OraclesUiOptionLayout *const size_options[3] = { out->far_size, out->menus, out->unvisited };
        for (unsigned i = t->far_size ? 0u : 1u; i < 3u; i++) {
            const unsigned row = size_rows[i], n = row - ORACLES_DISPLAY_MENUS;
            const char *const notes[1] = { t->size_notes[n] };
            OraclesUiLine *const notes_out[1] = { &out->size_notes[n] };
            y += DISPLAY_ROW_GAP;
            y += oracles_ui_display_row(value_x, y, value_w, oracles_display_labels[row], left, t->size_explanations[n], ADVANCED_EXPLANATION_H, notes,
                                        t->size_notes[n][0] ? 1 : 0, size_choices[i], NULL, row == ORACLES_DISPLAY_UNVISITED ? 3 : 2, &out->labels[row],
                                        &out->size_explanations[n], notes_out,
                                        size_options[i], &out->rows[row]);
        }
        out->panel = pl_box(PANEL_X, PANEL_Y, PANEL_W, y + PANEL_PAD_Y + PANEL_BORDER - PANEL_Y);
        return;
    }
    if (t->advanced) {
        /* The Advanced rows, two lines kept for each explanation so that none changes height: Core (with its note on
         * savestates), continuous transitions (with theirs), Vsync, the workers (Auto naming its count), the camera; from
         * a game the core, the transitions and the workers say why they are dimmed in place of their notes. */
        const char *workers[3];
        for (int i = 0; i < 3; i++) workers[i] = t->workers_names[i];
        const char *const transitions_notes[1] = { t->transitions_note };
        const char *const core_notes[1] = { t->core_note }, *const vsync_notes[1] = { t->vsync_note }, *const workers_notes[1] = { t->workers_note };
        OraclesUiLine *const transitions_out[1] = { &out->transitions_note };
        OraclesUiLine *const core_out[1] = { &out->core_note }, *const vsync_out[1] = { &out->vsync_note }, *const workers_out[1] = { &out->workers_note };
        y += oracles_ui_display_row(value_x, y, value_w, oracles_display_labels[ORACLES_DISPLAY_CORE], left, t->explanations[3], ADVANCED_EXPLANATION_H,
                         core_notes, 1, oracles_display_core_choices, NULL, 2, &out->labels[ORACLES_DISPLAY_CORE], &out->explanations[3], core_out,
                         out->core, &out->rows[ORACLES_DISPLAY_CORE]) + DISPLAY_ROW_GAP;
        y += oracles_ui_display_row(value_x, y, value_w, oracles_display_labels[ORACLES_DISPLAY_TRANSITIONS], left, t->explanations[1], ADVANCED_EXPLANATION_H,
                         transitions_notes, 1, oracles_ui_transition_choices, NULL, 2, &out->labels[ORACLES_DISPLAY_TRANSITIONS], &out->explanations[1],
                         transitions_out, out->transitions, &out->rows[ORACLES_DISPLAY_TRANSITIONS]) + DISPLAY_ROW_GAP;
        y += oracles_ui_display_row(value_x, y, value_w, oracles_display_labels[ORACLES_DISPLAY_VSYNC], left, t->explanations[2], ADVANCED_EXPLANATION_H,
                         vsync_notes, t->vsync_note ? 1 : 0, oracles_display_vsync_choices, NULL, 3, &out->labels[ORACLES_DISPLAY_VSYNC], &out->explanations[2],
                         vsync_out, out->vsync, &out->rows[ORACLES_DISPLAY_VSYNC]) + DISPLAY_ROW_GAP;
        y += oracles_ui_display_row(value_x, y, value_w, oracles_display_labels[ORACLES_DISPLAY_WORKERS], left, t->explanations[4], ADVANCED_EXPLANATION_H,
                         workers_notes, t->workers_note ? 1 : 0, workers, NULL, 3, &out->labels[ORACLES_DISPLAY_WORKERS], &out->explanations[4],
                         workers_out, out->workers, &out->rows[ORACLES_DISPLAY_WORKERS]) + DISPLAY_ROW_GAP;
        /* Camera: its explanation on three lines, its note under it where it has no effect. */
        const char *const camera_notes[1] = { t->camera_note };
        OraclesUiLine *const camera_out[1] = { &out->camera_note };
        y += oracles_ui_display_row(value_x, y, value_w, oracles_display_labels[ORACLES_DISPLAY_CAMERA], left, t->camera_explanation, ADVANCED_EXPLANATION_H,
                         camera_notes, t->camera_note ? 1 : 0, oracles_display_camera_choices, NULL, 2, &out->labels[ORACLES_DISPLAY_CAMERA],
                         &out->camera_explanation, camera_out, out->camera, &out->rows[ORACLES_DISPLAY_CAMERA]);
        out->panel = pl_box(PANEL_X, PANEL_Y, PANEL_W, y + PANEL_PAD_Y + PANEL_BORDER - PANEL_Y);
        return;
    }

    /* Profile: the two profiles framed, each with its surface, and the chosen one's note, two lines kept for it. */
    oracles_ui_wrap(&oracles_ui_row_label, oracles_display_labels[ORACLES_DISPLAY_PROFILE], DISPLAY_LABEL_W, left, y + LABEL_PAD_TOP,
                    &out->labels[ORACLES_DISPLAY_PROFILE]);
    float inner_y = y + DISPLAY_ROW_PAD_Y;
    const char *profile_sizes[ORACLES_PROFILES];
    for (int p = 0; p < ORACLES_PROFILES; p++) profile_sizes[p] = t->profile_sizes[p];
    float options_h = pl_framed(oracles_profile_names, profile_sizes, NULL, ORACLES_PROFILES, inner_x, inner_y, out->profiles);
    out->profile_note = pl_line_at(&oracles_ui_row_text, t->profile_note, inner_x, inner_y + options_h + PROFILE_NOTE_GAP, PROFILE_NOTE_LINE);
    float bottom = out->profile_note.y + PROFILE_NOTE_H;
    out->rows[ORACLES_DISPLAY_PROFILE] = pl_box(value_x, y, value_w, bottom + DISPLAY_ROW_PAD_Y - y);
    y += out->rows[ORACLES_DISPLAY_PROFILE].h + DISPLAY_ROW_GAP;

    /* Quality: its choices from the left, each with what it sets, Custom's place kept; under them the quality in effect
     * and its note, a line each. */
    oracles_ui_wrap(&oracles_ui_row_label, oracles_display_labels[ORACLES_DISPLAY_QUALITY], DISPLAY_LABEL_W, left, y + LABEL_PAD_TOP,
                    &out->labels[ORACLES_DISPLAY_QUALITY]);
    inner_y = y + DISPLAY_ROW_PAD_Y;
    const char *quality_sets[ORACLES_DISPLAY_QUALITIES + 1];
    for (int q = 0; q <= ORACLES_DISPLAY_QUALITIES; q++) quality_sets[q] = t->quality_sets[q];
    pl_choices(oracles_display_quality_names, quality_sets, ORACLES_DISPLAY_QUALITIES + 1, value_x + value_w - ROW_PAD_X, inner_y, out->qualities);
    const float from_left = inner_x - out->qualities[0].box.x;
    for (int q = 0; q <= ORACLES_DISPLAY_QUALITIES; q++) pl_shift_option(&out->qualities[q], from_left, 0.0f);
    out->quality_text = pl_line_at(&oracles_ui_row_text, t->quality_text, inner_x, inner_y + out->qualities[0].box.h + PROFILE_NOTE_GAP, QUALITY_TEXT_LINE);
    out->quality_note = pl_line_at(&oracles_ui_row_note, t->quality_note, inner_x, out->quality_text.y + out->quality_text.h + NOTE_GAP, QUALITY_NOTE_LINE);
    bottom = out->quality_note.y + out->quality_note.h;
    out->rows[ORACLES_DISPLAY_QUALITY] = pl_box(value_x, y, value_w, bottom + DISPLAY_ROW_PAD_Y - y);
    y += out->rows[ORACLES_DISPLAY_QUALITY].h + DISPLAY_ROW_GAP;

    /* Window: the four choices framed, each with its size, each as wide whatever the profile and the view; its notes are
     * under the diagram. */
    oracles_ui_wrap(&oracles_ui_row_label, oracles_display_labels[ORACLES_DISPLAY_WINDOW], DISPLAY_LABEL_W, left, y + LABEL_PAD_TOP,
                    &out->labels[ORACLES_DISPLAY_WINDOW]);
    inner_y = y + DISPLAY_ROW_PAD_Y;
    const char *names[4], *sizes[4];
    for (int i = 0; i < 4; i++) { names[i] = t->window_names[i]; sizes[i] = t->window_sizes[i]; }
    options_h = pl_framed(names, sizes, t->window_size_length, 4, inner_x, inner_y, out->windows);
    out->rows[ORACLES_DISPLAY_WINDOW] = pl_box(value_x, y, value_w, options_h + 2.0f * DISPLAY_ROW_PAD_Y);
    y += out->rows[ORACLES_DISPLAY_WINDOW].h + DISPLAY_ROW_GAP;

    /* View (each level with its size), the HUD, the Text and Picture links: an explanation and choices, or the link. */
    const char *view_sizes[3];
    for (int v = 0; v < 3; v++) view_sizes[v] = t->view_sizes[v];
    const char *const view_notes[1] = { t->view_note };
    OraclesUiLine *const view_out[1] = { &out->view_note };
    y += oracles_ui_display_row(value_x, y, value_w, oracles_display_labels[ORACLES_DISPLAY_VIEW], left, t->view_explanation, 0.0f, view_notes,
                     t->view_note ? 1 : 0, oracles_display_view_names, view_sizes, 3, &out->labels[ORACLES_DISPLAY_VIEW], &out->view_explanation, view_out,
                     out->views, &out->rows[ORACLES_DISPLAY_VIEW]) + DISPLAY_ROW_GAP;
    y += hud_row_16_9(t, value_x, y, value_w, left, out) + DISPLAY_ROW_GAP;
    y += oracles_ui_text_link_16_9(t, value_x, y, value_w, left, out) + DISPLAY_ROW_GAP;
    y += oracles_ui_picture_link_16_9(t, value_x, y, value_w, left, out);
    out->panel = pl_box(PANEL_X, PANEL_Y, PANEL_W, y + PANEL_PAD_Y + PANEL_BORDER - PANEL_Y);
}

/* Options framed (a name, and a size under it in `size_style` when `sizes`, `size_lengths` characters wide at least when
 * given), from (x, y) in lines `width` wide, `gap` apart, each 110x90 at least, `pad_x` past its texts, their texts
 * centred.  Returns their height. */
static float framed_4_3_as(const char *const *names, const char *const *sizes, const int *size_lengths, const OraclesUiTextStyle *size_style,
                           float pad_x, float gap, unsigned count, float x, float y, float width, OraclesUiOptionLayout *out)
{
    float line_x = x, line_y = y, line_h = 0.0f;
    for (unsigned i = 0; i < count; i++) {
        OraclesUiOptionLayout *o = &out[i];
        o->name = pl_line_at(&oracles_ui_row_button_4_3, names[i], 0.0f, 0.0f, OPTION_NAME_LINE_4_3);
        memset(&o->size, 0, sizeof o->size);
        if (sizes) o->size = pl_line_at(size_style, sizes[i], 0.0f, 0.0f, OPTION_SIZE_LINE_4_3);
        const float size_w = size_lengths ? pl_max_f(o->size.w, pl_characters(size_style, size_lengths[i])) : o->size.w;
        const float inner_w = pl_max_f(o->name.w, size_w), inner_h = o->name.h + o->size.h;
        o->box.w = pl_max_f(OPTION_MIN_W_4_3, inner_w + 2.0f * (pad_x + OPTION_BORDER_4_3));
        o->box.h = pl_max_f(OPTION_MIN_H_4_3, inner_h + 2.0f * (OPTION_PAD_Y_4_3 + OPTION_BORDER_4_3));
        if (line_x > x && line_x + o->box.w > x + width) {   /* wrapped to the next line */
            line_x = x;
            line_y += line_h + gap;
            line_h = 0.0f;
        }
        o->box.x = line_x;
        o->box.y = line_y;
        const float top = o->box.y + (o->box.h - inner_h) * 0.5f;
        pl_move_line(&o->name, o->box.x + (o->box.w - o->name.w) * 0.5f, top);
        if (sizes) pl_move_line(&o->size, o->box.x + (o->box.w - o->size.w) * 0.5f, top + o->name.h);
        line_x += o->box.w + gap;
        line_h = pl_max_f(line_h, o->box.h);
    }
    return line_y + line_h - y;
}

/* A profile's, a window's, a choice's: the size in the mono style. */
static float framed_4_3(const char *const *names, const char *const *sizes, const int *size_lengths, unsigned count, float x, float y, float width,
                        OraclesUiOptionLayout *out)
{
    return framed_4_3_as(names, sizes, size_lengths, &oracles_ui_option_size_4_3, OPTION_PAD_X_4_3, OPTION_GAP_4_3, count, x, y, width, out);
}

static void shift_options(OraclesUiOptionLayout *o, unsigned count, float dy)
{
    for (unsigned i = 0; i < count; i++) pl_shift_option(&o[i], 0.0f, dy);
}

/* A row of Display: its label wrapped in its column and `content_h` of values, both centred in the row, 100 tall at
 * least.  Returns the values' top. */
float oracles_ui_display_row_4_3(float y, const char *label, float content_h, OraclesUiWrapped *label_out, OraclesUiBox *row)
{
    oracles_ui_wrap(&oracles_ui_row_label_4_3, label, DISPLAY_LABEL_W_4_3, LEFT_4_3, 0.0f, label_out);
    const float track = pl_max_f(ROW_MIN_H_4_3 - 2.0f * DISPLAY_PAD_Y_4_3, pl_max_f(label_out->h, content_h));
    *row = pl_box(ROWS_X_4_3, y, ROWS_W_4_3, track + 2.0f * DISPLAY_PAD_Y_4_3);
    pl_move_wrapped(label_out, y + DISPLAY_PAD_Y_4_3 + (track - label_out->h) * 0.5f);
    return y + DISPLAY_PAD_Y_4_3 + (track - content_h) * 0.5f;
}

/* A row of Display whose values are options (their size lines `size_lengths` characters wide at least, NULL: their
 * own): returns the row's bottom. */
float oracles_ui_display_options_row_4_3(float y, unsigned row, const char *const *names, const char *const *sizes, const int *size_lengths,
                                         unsigned count, OraclesUiOptionLayout *options, OraclesUiDisplayLayout *out)
{
    const float width = RIGHT_4_3 - DISPLAY_VALUE_X_4_3;
    const float h = framed_4_3(names, sizes, size_lengths, count, DISPLAY_VALUE_X_4_3, 0.0f, width, options);
    shift_options(options, count, oracles_ui_display_row_4_3(y, oracles_display_labels[row], h, &out->labels[row], &out->rows[row]));
    return out->rows[row].y + out->rows[row].h;
}

/* The bottom of the rows laid out: the lowest row's. */
static float rows_bottom(const OraclesUiDisplayLayout *out)
{
    float bottom = ROWS_Y_4_3;
    for (unsigned r = 0; r < ORACLES_DISPLAY_ROWS; r++)
        if (r != out->link && out->rows[r].h > 0.0f && out->rows[r].y >= ROWS_Y_4_3) bottom = pl_max_f(bottom, out->rows[r].y + out->rows[r].h);
    return bottom;
}

/* 4:3's rows' room, and the scroll kept within the page's ends: none while the rows fit. */
static void rows_room_4_3(const OraclesUiDisplayTexts *t, OraclesUiDisplayLayout *out)
{
    out->rows_room = pl_box(ROWS_X_4_3, ROWS_ROOM_TOP_4_3, ROWS_W_4_3, ROWS_ROOM_H_4_3);
    const float full = rows_bottom(out) - ROWS_Y_4_3, room = ROWS_ROOM_H_4_3 - (ROWS_Y_4_3 - ROWS_ROOM_TOP_4_3);
    out->scroll_max = full > room + 1.0f ? full - room : 0.0f;
    out->scroll = fminf(pl_max_f(t->scroll, 0.0f), out->scroll_max);
}

/* The scroll that keeps row `row` in sight from `scroll`, as little moved as can be. */
static float follow_4_3(const OraclesUiDisplayLayout *out, unsigned row, float scroll)
{
    if (out->scroll_max <= 0.0f) return 0.0f;
    const float room = ROWS_ROOM_H_4_3 - (ROWS_Y_4_3 - ROWS_ROOM_TOP_4_3), full = out->scroll_max + room;
    float o = fminf(pl_max_f(scroll, 0.0f), out->scroll_max);
    if (row < ORACLES_DISPLAY_ROWS && row != out->link && out->rows[row].h > 0.0f) {
        const float top = out->rows[row].y - ROWS_Y_4_3, bottom = top + out->rows[row].h;
        const float clear_below = bottom < full - 1.0f ? ROWS_FADE_4_3 : 0.0f, clear_above = top > 1.0f ? ROWS_FADE_4_3 : 0.0f;
        if (bottom - o > room - clear_below) o = bottom - room + clear_below;
        if (top - o < clear_above) o = top - clear_above;
    }
    return roundf(fminf(pl_max_f(o, 0.0f), out->scroll_max));
}

static void display_4_3(const OraclesUiDisplayTexts *t, OraclesUiDisplayLayout *out)
{
    /* The header: the section's name and the game's title on one line, their baselines aligned, then the highlighted
     * row's explanation on one line and its note on the next, kept when empty. */
    memset(&out->head, 0, sizeof out->head);
    out->head.section = pl_line_at(&oracles_ui_page_section_4_3, t->section ? t->section : "Display", HEAD_X_4_3, HEAD_Y_4_3, 0.0f);
    if (t->over && !strcmp(t->over, "Oracle of")) snprintf(out->title_text, sizeof out->title_text, "%s %s", t->over, t->title ? t->title : "");
    else snprintf(out->title_text, sizeof out->title_text, "%s", t->title ? t->title : "");
    out->title = pl_line_at(&oracles_ui_display_title_4_3, out->title_text, out->head.section.x + out->head.section.w + DISPLAY_TITLE_GAP_4_3, 0.0f, 0.0f);
    OraclesUiLine *const head_line[2] = { &out->head.section, &out->title };
    const float head_bottom = pl_baseline_row(head_line, 2, HEAD_Y_4_3);
    out->help = pl_line_at(&oracles_ui_help_4_3, t->help, HEAD_X_4_3, head_bottom + HEAD_GAP_4_3, HELP_LINE_4_3);
    out->help_note = pl_line_at(&oracles_ui_row_note_4_3, t->help_note, HEAD_X_4_3, out->help.y + out->help.h + HELP_NOTE_GAP_4_3, HELP_NOTE_LINE_4_3);
    /* Opened from a game, under them: what only the next session takes, said once for the page. */
    if (t->later) {
        out->page_note.count = 1;
        snprintf(out->page_note.text[0], sizeof out->page_note.text[0], "%s", oracles_ui_display_later);
        out->page_note.lines[0] = pl_line_at(&oracles_ui_page_note_4_3, oracles_ui_display_later, HEAD_X_4_3, out->help_note.y + out->help_note.h + HEAD_GAP_4_3,
                                          DISPLAY_PAGE_NOTE_LINE_4_3);
        out->page_note.w = out->page_note.lines[0].w;
        out->page_note.h = out->page_note.lines[0].h;
    }
    /* Display › Text: its preview from the rows' top, its rows under it; no diagram, no Advanced. */
    if (t->text_page) {
        out->link = ORACLES_DISPLAY_ROWS;
        oracles_ui_text_page_4_3(t, out);
        return;
    }
    /* The diagram at the top right, half 16:9's, without its line: Window's help says the size. */
    const float box_w = pl_round_half_up((t->diagram_box_w > 0.0f ? t->diagram_box_w : ORACLES_UI_DIAGRAM_W) * 0.5f);
    const float w = fminf(pl_round_half_up(t->diagram_w * 0.5f), box_w - 2.0f * DIAGRAM_BORDER), h = pl_round_half_up(t->diagram_h * 0.5f);
    out->diagram = pl_box(DIAGRAM_RIGHT_4_3 - box_w, HEAD_Y_4_3, box_w, DIAGRAM_H_4_3);
    out->diagram_window = pl_box(out->diagram.x + (box_w - w) * 0.5f, out->diagram.y + (DIAGRAM_H_4_3 - h) * 0.5f, w, h);
    /* Advanced beside it, framed: its label, then the arrow, both centred in it; not on the Advanced rows nor on Picture's. */
    out->link = t->advanced || t->picture ? ORACLES_DISPLAY_ROWS : ORACLES_DISPLAY_ADVANCED;
    if (out->link < ORACLES_DISPLAY_ROWS) {
        out->advanced_label = pl_line_at(&oracles_ui_row_label_4_3, oracles_display_labels[out->link], 0.0f, 0.0f, 0.0f);
        const float button_w = 2.0f * (ADVANCED_BORDER_4_3 + ADVANCED_PAD_X_4_3) + out->advanced_label.w + ADVANCED_GAP_4_3 + ADVANCED_ARROW_W_4_3;
        const float right = out->diagram.x - ADVANCED_MARGIN_4_3;
        out->rows[out->link] = pl_box(right - button_w, HEAD_Y_4_3, button_w, ADVANCED_H_4_3);
        const float label_x = right - button_w + ADVANCED_BORDER_4_3 + ADVANCED_PAD_X_4_3;
        pl_move_line(&out->advanced_label, label_x, HEAD_Y_4_3 + (ADVANCED_H_4_3 - out->advanced_label.h) * 0.5f);
        out->advanced_arrow = pl_box(label_x + out->advanced_label.w + ADVANCED_GAP_4_3, HEAD_Y_4_3 + (ADVANCED_H_4_3 - ADVANCED_ARROW_H_4_3) * 0.5f,
                                  ADVANCED_ARROW_W_4_3, ADVANCED_ARROW_H_4_3);
    }

    float y = ROWS_Y_4_3;
    const float width = RIGHT_4_3 - DISPLAY_VALUE_X_4_3;
    if (t->picture) {
        const char *scaling_sizes[2];
        for (int i = 0; i < 2; i++) scaling_sizes[i] = t->scaling_sizes[i];
        y = oracles_ui_display_options_row_4_3(y, ORACLES_DISPLAY_SCALING, oracles_display_scaling_choices, scaling_sizes, NULL, 2, out->scaling, out) + DISPLAY_ROW_GAP_4_3;
        filter_step(&step_4_3, oracles_display_filter_names[t->filter], 0.0f, 0.0f, out);
        const float filter_top = oracles_ui_display_row_4_3(y, oracles_display_labels[ORACLES_DISPLAY_FILTER], out->filter_value.h,
                                                            &out->labels[ORACLES_DISPLAY_FILTER], &out->rows[ORACLES_DISPLAY_FILTER]);
        filter_step(&step_4_3, oracles_display_filter_names[t->filter], DISPLAY_VALUE_X_4_3, filter_top, out);
        y = out->rows[ORACLES_DISPLAY_FILTER].y + out->rows[ORACLES_DISPLAY_FILTER].h + DISPLAY_ROW_GAP_4_3;
        /* While Filter has the focus, the preview right of the rows, under Filter, where they have no choices. */
        if (t->filter_preview) {
            out->filter_preview = pl_box(FILTER_PREVIEW_X_4_3, FILTER_PREVIEW_Y_4_3, ORACLES_UI_FILTER_PREVIEW_W_4_3, ORACLES_UI_FILTER_PREVIEW_H_4_3);
            pl_wrap_pretty_lines(&oracles_ui_diagram_label_4_3, t->filter_caption, ORACLES_UI_FILTER_PREVIEW_W_4_3, FILTER_PREVIEW_X_4_3,
                                 out->filter_preview.y + out->filter_preview.h + FILTER_CAPTION_GAP_4_3, FILTER_CAPTION_LINE_4_3, &out->filter_caption);
        }
        y = oracles_ui_display_options_row_4_3(y, ORACLES_DISPLAY_MARGIN, oracles_display_margin_choices, NULL, NULL, ORACLES_DISPLAY_MARGINS, out->margin, out)
            + DISPLAY_ROW_GAP_4_3;
        y = oracles_ui_display_options_row_4_3(y, ORACLES_DISPLAY_COLOUR, oracles_display_colour_choices, NULL, NULL, 2, out->colour, out) + DISPLAY_ROW_GAP_4_3;
        if (t->far_size)
            y = oracles_ui_display_options_row_4_3(y, ORACLES_DISPLAY_FAR_SIZE, oracles_display_far_size_choices, NULL, NULL, 2, out->far_size, out) + DISPLAY_ROW_GAP_4_3;
        y = oracles_ui_display_options_row_4_3(y, ORACLES_DISPLAY_MENUS, oracles_display_menus_choices, NULL, NULL, 2, out->menus, out) + DISPLAY_ROW_GAP_4_3;
        oracles_ui_display_options_row_4_3(y, ORACLES_DISPLAY_UNVISITED, oracles_display_unvisited_choices, NULL, NULL, 3, out->unvisited, out);
    } else if (t->advanced) {
        const char *workers[3];
        for (int i = 0; i < 3; i++) workers[i] = t->workers_names[i];
        y = oracles_ui_display_options_row_4_3(y, ORACLES_DISPLAY_CORE, oracles_display_core_choices, NULL, NULL, 2, out->core, out) + DISPLAY_ROW_GAP_4_3;
        y = oracles_ui_display_options_row_4_3(y, ORACLES_DISPLAY_TRANSITIONS, oracles_ui_transition_choices, NULL, NULL, 2, out->transitions, out) + DISPLAY_ROW_GAP_4_3;
        y = oracles_ui_display_options_row_4_3(y, ORACLES_DISPLAY_VSYNC, oracles_display_vsync_choices, NULL, NULL, 3, out->vsync, out) + DISPLAY_ROW_GAP_4_3;
        y = oracles_ui_display_options_row_4_3(y, ORACLES_DISPLAY_WORKERS, workers, NULL, NULL, 3, out->workers, out) + DISPLAY_ROW_GAP_4_3;
        oracles_ui_display_options_row_4_3(y, ORACLES_DISPLAY_CAMERA, oracles_display_camera_choices, NULL, NULL, 2, out->camera, out);
    } else {
        const char *profile_sizes[ORACLES_PROFILES], *quality_sets[ORACLES_DISPLAY_QUALITIES + 1], *names[4], *sizes[4], *view_sizes[3];
        for (int p = 0; p < ORACLES_PROFILES; p++) profile_sizes[p] = t->profile_sizes[p];
        for (int q = 0; q <= ORACLES_DISPLAY_QUALITIES; q++) quality_sets[q] = t->quality_sets[q];
        for (int i = 0; i < 4; i++) { names[i] = t->window_names[i]; sizes[i] = t->window_sizes[i]; }
        for (int v = 0; v < 3; v++) view_sizes[v] = t->view_sizes[v];
        y = oracles_ui_display_options_row_4_3(y, ORACLES_DISPLAY_PROFILE, oracles_profile_names, profile_sizes, NULL, ORACLES_PROFILES, out->profiles, out)
            + DISPLAY_ROW_GAP_4_3;
        /* Quality: its choices narrower, what each sets under its name in the running text's face, Custom's place kept. */
        const float quality_h = framed_4_3_as(oracles_display_quality_names, quality_sets, NULL, &oracles_ui_quality_sets_4_3, QUALITY_PAD_X_4_3,
                                              QUALITY_GAP_4_3, ORACLES_DISPLAY_QUALITIES + 1, DISPLAY_VALUE_X_4_3, 0.0f, width, out->qualities);
        shift_options(out->qualities, ORACLES_DISPLAY_QUALITIES + 1,
                      oracles_ui_display_row_4_3(y, oracles_display_labels[ORACLES_DISPLAY_QUALITY], quality_h, &out->labels[ORACLES_DISPLAY_QUALITY],
                                      &out->rows[ORACLES_DISPLAY_QUALITY]));
        y = out->rows[ORACLES_DISPLAY_QUALITY].y + out->rows[ORACLES_DISPLAY_QUALITY].h + DISPLAY_ROW_GAP_4_3;
        /* Window: its choices, or at one size the italic line that says so in their place, as tall. */
        if (t->window_one) {
            out->window_one = pl_line_at(&oracles_ui_row_note_4_3, oracles_display_one_size_note, DISPLAY_VALUE_X_4_3, 0.0f, 0.0f);
            const float top = oracles_ui_display_row_4_3(y, oracles_display_labels[ORACLES_DISPLAY_WINDOW], OPTION_MIN_H_4_3, &out->labels[ORACLES_DISPLAY_WINDOW],
                                              &out->rows[ORACLES_DISPLAY_WINDOW]);
            pl_move_line(&out->window_one, DISPLAY_VALUE_X_4_3, top + (OPTION_MIN_H_4_3 - out->window_one.h) * 0.5f);
            y = out->rows[ORACLES_DISPLAY_WINDOW].y + out->rows[ORACLES_DISPLAY_WINDOW].h + DISPLAY_ROW_GAP_4_3;
        } else {
            y = oracles_ui_display_options_row_4_3(y, ORACLES_DISPLAY_WINDOW, names, sizes, t->window_size_length, 4, out->windows, out) + DISPLAY_ROW_GAP_4_3;
        }
        y = oracles_ui_display_options_row_4_3(y, ORACLES_DISPLAY_VIEW, oracles_display_view_names, view_sizes, NULL, 3, out->views, out) + DISPLAY_ROW_GAP_4_3;
        y = oracles_ui_display_options_row_4_3(y, ORACLES_DISPLAY_HUD, oracles_display_hud_choices, NULL, NULL, ORACLES_DISPLAY_HUDS, out->hud, out)
            + DISPLAY_ROW_GAP_4_3;
        if (t->floating)
            y = oracles_ui_display_options_row_4_3(y, ORACLES_DISPLAY_PANELS, oracles_display_panels_choices, NULL, NULL, 2, out->panels, out) + DISPLAY_ROW_GAP_4_3;
        y = oracles_ui_text_link_4_3(t, y, out) + DISPLAY_ROW_GAP_4_3;
        oracles_ui_picture_link_4_3(t, y, out);
    }
    rows_room_4_3(t, out);
}

void oracles_ui_layout_display(OraclesUiLayout layout, const OraclesUiDisplayTexts *t, OraclesUiDisplayLayout *out)
{
    memset(out, 0, sizeof *out);
    if (layout == ORACLES_UI_LAYOUT_4_3) display_4_3(t, out);
    else display_16_9(t, out);
}

void oracles_ui_display_texts(const OraclesHomeNav *nav, OraclesUiDisplayTexts *t)
{
    const OraclesHomeHero hero = oracles_home_hero(nav);
    memset(t, 0, sizeof *t);
    t->section = oracles_display_section(nav);
    t->advanced = nav->advanced;
    t->picture = nav->picture_page;
    t->far_size = oracles_display_far_size_shown(nav);
    oracles_display_picture_summary(nav, t->picture_summary, sizeof t->picture_summary);
    t->picture_explanation = oracles_display_explanation(ORACLES_DISPLAY_PICTURE);
    t->text_page = nav->text_page;
    t->over = oracles_home_over(hero);
    t->title = oracles_home_title(hero);
    for (int w = 0; w < 4; w++) oracles_display_window_texts(nav, w, t->window_names[w], t->window_sizes[w], sizeof t->window_sizes[w]);
    t->window_note = oracles_display_window_note;
    oracles_display_reduced(nav, t->window_reduced, sizeof t->window_reduced);
    t->window_one = oracles_display_one_size(nav);
    /* From a game, the rows it keeps say why they are dimmed in place of their notes. */
    const char *const locked = nav->in_game ? oracles_display_pause_reason : NULL;
    t->profile_note = locked ? locked : oracles_display_profile_note(nav);
    t->view_note = t->workers_note = locked;
    t->core_note = locked ? locked : oracles_ui_core_note;
    t->transitions_note = locked ? locked : oracles_ui_transitions_note;
    for (int q = 0; q <= ORACLES_DISPLAY_QUALITIES; q++) oracles_display_quality_sets(q, t->quality_sets[q], sizeof t->quality_sets[q]);
    for (int w = 0; w < 4; w++) t->window_size_length[w] = oracles_display_window_size_length(nav, w);
    t->window_fit = t->window_reduced[0] ? t->window_reduced : oracles_ui_window_fit_note;
    oracles_display_quality_text(nav, 0, t->quality_text, sizeof t->quality_text);
    t->quality_note = locked ? locked : oracles_display_quality_note(nav, 0);
    t->later = nav->in_game;
    t->explanations[0] = oracles_display_colour_explanation(nav, 0);
    t->explanations[1] = oracles_display_explanation(ORACLES_DISPLAY_TRANSITIONS);
    t->explanations[2] = oracles_display_explanation(ORACLES_DISPLAY_VSYNC);
    t->explanations[3] = oracles_display_explanation(ORACLES_DISPLAY_CORE);
    t->explanations[4] = oracles_display_explanation(ORACLES_DISPLAY_WORKERS);
    t->explanations[5] = oracles_display_explanation(ORACLES_DISPLAY_SCALING);
    t->explanations[6] = oracles_display_hud_explanation(nav, 0);
    t->floating = nav->display.hud == ORACLES_DISPLAY_HUD_FLOATING;
    t->row = nav->row;
    t->scroll = nav->display_scroll;
    t->explanations[7] = oracles_display_explanation(ORACLES_DISPLAY_MARGIN);
    t->explanations[8] = oracles_display_explanation(ORACLES_DISPLAY_TEXT);
    t->camera_explanation = oracles_display_explanation(ORACLES_DISPLAY_CAMERA);
    t->camera_note = oracles_display_camera_note(nav)[0] ? oracles_display_camera_note(nav) : NULL;
    for (unsigned i = 0; i < 3u; i++) {
        t->size_explanations[i] = oracles_display_explanation(ORACLES_DISPLAY_MENUS + i);
        oracles_display_size_note(nav, ORACLES_DISPLAY_MENUS + i, t->size_notes[i], sizeof t->size_notes[i]);
    }
    oracles_display_text_summary(nav, t->text_summary, sizeof t->text_summary);
    for (unsigned i = 0; i < ORACLES_DISPLAY_TEXT_ROWS; i++) {
        t->text_explanations[i] = oracles_display_text_explanation(ORACLES_DISPLAY_LETTERS + i, 0);
        t->text_notes[i] = oracles_display_text_note(nav, ORACLES_DISPLAY_LETTERS + i);
    }
    if (nav->text_page) oracles_display_text_preview(nav, &t->preview);
    for (int s = 0; s < 2; s++) oracles_display_scaling_size(nav, s, t->scaling_sizes[s], sizeof t->scaling_sizes[s]);
    t->filter = oracles_display_filter_shown(nav);
    t->filter_ok = !oracles_display_filter_reason(nav, t->filter);
    t->filter_explanation = oracles_display_filter_explanation(t->filter);
    t->filter_note = oracles_display_filter_note(nav);
    t->filter_preview = nav->picture_page && nav->row == ORACLES_DISPLAY_FILTER;
    if (nav->in_game) snprintf(t->filter_caption, sizeof t->filter_caption, "Your game, as it resumes");
    else if (nav->display.last_picture[0]) snprintf(t->filter_caption, sizeof t->filter_caption, "%s, where you left it", nav->display.last_picture);
    else snprintf(t->filter_caption, sizeof t->filter_caption, "Sample picture: play to see your own.");
    t->view_explanation = oracles_display_explanation(ORACLES_DISPLAY_VIEW);
    for (int i = 0; i < 3; i++) oracles_display_workers_choice(nav, i, t->workers_names[i], sizeof t->workers_names[i]);
    for (int p = 0; p < ORACLES_PROFILES; p++) oracles_profile_size(nav, p, t->profile_sizes[p], sizeof t->profile_sizes[p]);
    for (int v = 0; v < 3; v++) oracles_display_view_size(nav, v, t->view_sizes[v], sizeof t->view_sizes[v]);
    /* The diagram's box has the screen's shape at its height, as the sizes take it: 432 wide for 16:9 (and any wider
     * screen, which the view takes as 16:9), 324 for 4:3, less for a screen narrower than 4:3 (279 for 1240x1080), a
     * screen held upright counting as turned. */
    t->diagram_box_w = ORACLES_UI_DIAGRAM_W;
    if (oracles_display_screen_4_3(nav)) {
        const int w = nav->display.screen_w, h = nav->display.screen_h, across = w > h ? w : h, down = w > h ? h : w;
        const float narrower = down > 0 ? floorf(ORACLES_UI_DIAGRAM_H * (float)across / (float)down + 0.5f) : 324.0f;
        t->diagram_box_w = narrower < 324.0f ? narrower : 324.0f;
    }
    oracles_display_diagram(nav, t->diagram_box_w, ORACLES_UI_DIAGRAM_H, &t->diagram_w, &t->diagram_h, t->diagram_label, sizeof t->diagram_label);
    /* 4:3's help, one line and a note line, its own short texts so that neither wraps: the profile chosen (or that the
     * game plays in Faithful), Window's with the line on its size or the diagram's, the quality in effect. */
    static const char *const help_4_3[ORACLES_DISPLAY_ROWS] = {
        NULL, NULL, "Whole multiples of the picture: pixels stay sharp.", "How much of the world shows; farther asks more.",
        "Wide layout: hearts left, items right.", NULL, NULL, "Core, transitions, Vsync, workers, camera.",
        "Accurate: the reference. Fast: lighter.", "Rooms scroll into one another, swimming too.", VSYNC_SHORT, "Two fill the view faster, on one more core.",
        "Sharp: exact pixels. Fill: the whole screen.", "Smooth: diagonal walks glide.", "Keeps the picture clear of hidden screen edges.", NULL, NULL,
        NULL, NULL, "Scaling, filter, margin, colors, menus and scenes.", "Large: menus and scenes at their largest scale.",
        "Fitted: a Far made for this screen.", "Dungeons: Link's room alone. Hidden: unentered dark too.", NULL,
        "Bar color: the bar's own color. Neutral: light gray.",
    };
    const unsigned row = nav->row < ORACLES_DISPLAY_ROWS ? nav->row : ORACLES_DISPLAY_PROFILE;
    t->help = row == ORACLES_DISPLAY_COLOUR ? oracles_display_colour_explanation(nav, 1)
              : row == ORACLES_DISPLAY_HUD ? oracles_display_hud_explanation(nav, 1) : help_4_3[row];
    const int text_row = row >= ORACLES_DISPLAY_LETTERS && row <= ORACLES_DISPLAY_TEXT_SPEED;
    if (row == ORACLES_DISPLAY_TEXT || text_row) t->help = oracles_display_text_explanation(row, 1);
    if (text_row && oracles_display_text_note(nav, row)[0]) t->help_note = oracles_display_text_note(nav, row);
    switch (row) {
        case ORACLES_DISPLAY_PROFILE:
            t->help = oracles_display_played_profile(nav) != nav->display.profile ? oracles_display_profile_note(nav)
                      : nav->display.profile == ORACLES_PROFILE_ENHANCED ? "The wide world, drawn back." : "The game as it was, 160\xc3\x97" "144.";
            break;
        case ORACLES_DISPLAY_QUALITY:
            oracles_display_quality_text(nav, 1, t->help_text, sizeof t->help_text);
            t->help = t->help_text;
            t->help_note = oracles_display_quality_note(nav, 1);
            break;
        case ORACLES_DISPLAY_WINDOW:
            /* That the window is reduced, else its size, the diagram's, which 4:3 does not write under it. */
            if (t->window_reduced[0] && !t->window_one) t->help_note = t->window_reduced;
            else {
                snprintf(t->help_note_text, sizeof t->help_note_text, "%s.", t->diagram_label);
                t->help_note = t->help_note_text;
            }
            break;
        case ORACLES_DISPLAY_VIEW:
            /* On a screen far is fitted to, whichever view is chosen. */
            oracles_display_far_note(nav, t->help_note_text, sizeof t->help_note_text);
            if (t->help_note_text[0]) t->help_note = t->help_note_text;
            break;
        case ORACLES_DISPLAY_TEXT:
            /* The link: its settings summed up. */
            snprintf(t->help_note_text, sizeof t->help_note_text, "%s.", t->text_summary);
            t->help_note = t->help_note_text;
            break;
        case ORACLES_DISPLAY_TRANSITIONS: t->help_note = "Savestates with it on need it on."; break;
        case ORACLES_DISPLAY_CORE: t->help_note = oracles_ui_core_note; break;
        case ORACLES_DISPLAY_SCALING: t->help_note = oracles_ui_scaling_note; break;
        case ORACLES_DISPLAY_CAMERA: t->help_note = t->camera_note; break;
        case ORACLES_DISPLAY_FILTER:
            t->help = t->filter_explanation;
            if (t->filter_note[0]) t->help_note = t->filter_note;
            break;
        case ORACLES_DISPLAY_MENUS:
        case ORACLES_DISPLAY_FAR_SIZE:
        case ORACLES_DISPLAY_UNVISITED: {
            /* The row's note: why it is dimmed, or Far size's sizes on this screen. */
            const char *note = t->size_notes[row - ORACLES_DISPLAY_MENUS];
            if (note[0]) t->help_note = note;
            break;
        }
        default: break;
    }
    if (oracles_display_pause_locked(nav, row) && row != ORACLES_DISPLAY_FAR_SIZE) t->help_note = oracles_display_pause_reason;
}

float oracles_ui_display_follow(const OraclesHomeNav *nav)
{
    if (nav->layout != ORACLES_UI_LAYOUT_4_3) return 0.0f;
    static OraclesUiDisplayTexts t;
    static OraclesUiDisplayLayout l;
    oracles_ui_display_texts(nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    return follow_4_3(&l, t.row, nav->display_scroll);
}

float oracles_ui_display_scrolled(const OraclesHomeNav *nav, float dy)
{
    if (nav->layout != ORACLES_UI_LAYOUT_4_3 || !oracles_ui_fonts_load()) return 0.0f;
    static OraclesUiDisplayTexts t;
    static OraclesUiDisplayLayout l;
    oracles_ui_display_texts(nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    return roundf(fminf(fmaxf(nav->display_scroll + dy, 0.0f), l.scroll_max));
}

void oracles_ui_display_drag_start(OraclesUiDisplayDrag *drag, float y)
{
    drag->press_y = y;
    drag->dragged = 0;
}

int oracles_ui_display_drag_moved(OraclesUiDisplayDrag *drag, float y, float window_h)
{
    if (!drag->dragged && fabsf(y - drag->press_y) >= fmaxf(window_h, 1.0f) / 40.0f) drag->dragged = 1;
    return drag->dragged;
}
