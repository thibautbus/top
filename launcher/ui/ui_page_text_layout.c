/* Display's Text and Picture links and the Display › Text page (ui_page_layout.h), without SDL: a link's summary and
 * arrow at the right of its row; the page's rows as Advanced lays its own out, and its preview, the band at its proportions in a frame
 * of at most 520x293 in 16:9's left column, under the title, its label and note under it, or of at most 314x236 at the
 * top of 4:3's rows, its label and note at its right, at its bottom, so that the four rows fit under it. */
#include "ui_page_layout.h"

#include <math.h>
#include <string.h>

/* As ui_page_layout_internal.h's, in scene pixels. */
#define HEAD_X 96.0f
#define HEAD_W 520.0f
#define LABEL_PAD_TOP 18.0f
#define DISPLAY_LABEL_W 200.0f
#define ROW_PAD_X 20.0f
#define DISPLAY_ROW_PAD_Y 14.0f
#define DISPLAY_ROW_GAP 14.0f
#define ROW_GAP 24.0f
#define EXPLANATION_H 58.0f          /* two lines of 29 kept for each row's explanation, as Advanced's */
#define ROWS_X_4_3 64.0f
#define ROWS_Y_4_3 208.0f
#define ROWS_W_4_3 1312.0f
#define ROW_PAD_X_4_3 20.0f
#define DISPLAY_VALUE_X_4_3 408.0f
#define DISPLAY_ROW_GAP_4_3 6.0f
/* The link: its summary, then its arrow; 58 tall (16:9), 90 (4:3), the arrow centred on it. */
#define LINK_H 58.0f
#define LINK_GAP 14.0f
#define LINK_ARROW_W 9.0f
#define LINK_ARROW_H 12.0f
#define LINK_H_4_3 90.0f
#define LINK_GAP_4_3 20.0f
#define LINK_ARROW_W_4_3 13.0f
#define LINK_ARROW_H_4_3 18.0f
/* The preview: its frame's room, its border, the gaps to its label and note (16:9: under it; 4:3: beside it, their
 * column's gap and padding under it), the note's line kept, and the rows' margin under it (4:3). */
#define PREVIEW_W 520.0f
#define PREVIEW_H 293.0f
#define PREVIEW_W_4_3 314.0f
#define PREVIEW_H_4_3 236.0f
#define PREVIEW_BORDER 1.0f
#define PREVIEW_GAP 12.0f
#define PREVIEW_NOTE_H 29.0f
#define PREVIEW_GAP_4_3 28.0f
#define PREVIEW_LINE_GAP_4_3 4.0f
#define PREVIEW_NOTE_H_4_3 34.0f
#define PREVIEW_MARGIN_4_3 10.0f
/* In the game's pixels: the status bar's band, the box's height, its distance from the band's bottom edge, its text's
 * margins, the arrow's size and its distances from the box's corner. */
#define HUD_H 16.0f
#define BOX_H 40.0f
#define BOX_BOTTOM 8.0f
#define TEXT_LEFT 8.0f
#define TEXT_TOP 4.0f
#define ARROW 3.0f
#define ARROW_RIGHT 3.0f
#define ARROW_BOTTOM 2.0f

static OraclesUiLine line_at(const OraclesUiTextStyle *style, const char *text, float x, float y, float line_height)
{
    OraclesUiLine line;
    float baseline;
    oracles_ui_line_box(style, line_height, &line.h, &baseline);
    line.w = text && text[0] ? oracles_ui_text_width(style, text) : 0.0f;
    line.x = x;
    line.y = y;
    line.baseline = y + baseline;
    return line;
}

static void move_line(OraclesUiLine *line, float x, float y)
{
    line->baseline += y - line->y;
    line->x = x;
    line->y = y;
}

static OraclesUiBox box(float x, float y, float w, float h)
{
    const OraclesUiBox b = { x, y, w, h };
    return b;
}

static void move_wrapped(OraclesUiWrapped *w, float y)
{
    const float dy = y - (w->count ? w->lines[0].y : y);
    for (unsigned i = 0; i < w->count; i++) move_line(&w->lines[i], w->lines[i].x, w->lines[i].y + dy);
}

/* JavaScript's Math.round, as the mockup rounds the preview. */
static float round_d(double v) { return (float)floor(v + 0.5); }

/* A link's pieces: its summary, its arrow, both in its box. */
typedef struct link_out { OraclesUiLine *summary; OraclesUiBox *arrow, *link; } link_out;

/* The link's summary and arrow from (x, y) in a box `h` tall, the arrow `arrow_w` x `arrow_h` `gap` after the summary,
 * both centred on it: the box. */
static OraclesUiBox link_at(const OraclesUiTextStyle *style, const char *summary, float x, float y, float h, float gap, float arrow_w, float arrow_h,
                            link_out o)
{
    *o.summary = line_at(style, summary, x, 0.0f, 0.0f);
    move_line(o.summary, x, y + (h - o.summary->h) * 0.5f);
    *o.arrow = box(x + o.summary->w + gap, y + (h - arrow_h) * 0.5f, arrow_w, arrow_h);
    *o.link = box(x, y, o.summary->w + gap + arrow_w, h);
    return *o.link;
}

/* A link row of Display, `row`, its explanation beside the link (16:9). */
static float link_16_9(unsigned row, const char *text, const char *summary, float x, float y, float w, float label_x, OraclesUiWrapped *explanation,
                       link_out o, OraclesUiDisplayLayout *out)
{
    OraclesUiWrapped *label = &out->labels[row];
    oracles_ui_wrap(&oracles_ui_row_label, oracles_display_labels[row], DISPLAY_LABEL_W, label_x, y + LABEL_PAD_TOP, label);
    const float inner_x = x + ROW_PAD_X, inner_y = y + DISPLAY_ROW_PAD_Y, right = x + w - ROW_PAD_X;
    const float link_w = oracles_ui_text_width(&oracles_ui_choice, summary) + LINK_GAP + LINK_ARROW_W;
    oracles_ui_wrap(&oracles_ui_row_text, text, right - inner_x - ROW_GAP - link_w, inner_x, inner_y, explanation);
    const float inner_h = fmaxf(explanation->h, LINK_H);
    move_wrapped(explanation, inner_y + (inner_h - explanation->h) * 0.5f);
    link_at(&oracles_ui_choice, summary, right - link_w, inner_y + (inner_h - LINK_H) * 0.5f, LINK_H, LINK_GAP, LINK_ARROW_W, LINK_ARROW_H, o);
    out->rows[row] = box(x, y, w, inner_h + 2.0f * DISPLAY_ROW_PAD_Y);
    return fmaxf(LABEL_PAD_TOP + label->h, out->rows[row].h);
}

static float link_4_3(unsigned row, const char *summary, float y, link_out o, OraclesUiDisplayLayout *out)
{
    const float top = oracles_ui_display_row_4_3(y, oracles_display_labels[row], LINK_H_4_3, &out->labels[row], &out->rows[row]);
    link_at(&oracles_ui_row_button_4_3, summary, DISPLAY_VALUE_X_4_3, top, LINK_H_4_3, LINK_GAP_4_3, LINK_ARROW_W_4_3, LINK_ARROW_H_4_3, o);
    return out->rows[row].y + out->rows[row].h;
}

float oracles_ui_text_link_16_9(const OraclesUiDisplayTexts *t, float x, float y, float w, float label_x, OraclesUiDisplayLayout *out)
{
    const link_out o = { &out->text_summary, &out->text_arrow, &out->text_link };
    return link_16_9(ORACLES_DISPLAY_TEXT, t->explanations[8], t->text_summary, x, y, w, label_x, &out->explanations[8], o, out);
}

float oracles_ui_text_link_4_3(const OraclesUiDisplayTexts *t, float y, OraclesUiDisplayLayout *out)
{
    const link_out o = { &out->text_summary, &out->text_arrow, &out->text_link };
    return link_4_3(ORACLES_DISPLAY_TEXT, t->text_summary, y, o, out);
}

float oracles_ui_picture_link_16_9(const OraclesUiDisplayTexts *t, float x, float y, float w, float label_x, OraclesUiDisplayLayout *out)
{
    const link_out o = { &out->picture_summary, &out->picture_arrow, &out->picture_link };
    return link_16_9(ORACLES_DISPLAY_PICTURE, t->picture_explanation, t->picture_summary, x, y, w, label_x, &out->picture_explanation, o, out);
}

float oracles_ui_picture_link_4_3(const OraclesUiDisplayTexts *t, float y, OraclesUiDisplayLayout *out)
{
    const link_out o = { &out->picture_summary, &out->picture_arrow, &out->picture_link };
    return link_4_3(ORACLES_DISPLAY_PICTURE, t->picture_summary, y, o, out);
}

/* A piece of the floating panels, (x, y, w, h) in the game's pixels at `k` scene pixels each from (ox, oy), each measure
 * rounded as the mockup rounds it. */
static OraclesUiBox panel_piece(double x, double y, double w, double h, double k, float ox, float oy)
{
    return box(ox + round_d(x * k), oy + round_d(y * k), round_d(w * k), round_d(h * k));
}

/* Floating's panels over the band (vw x vh game pixels at `k`): the hearts' at the top left with six hearts, the rupees'
 * at the top right with their ink, B's and A's at the bottom left four apart, each with its key's badge in a gutter at
 * its left and its item's ink right of it; their corners in whole pixels of the game, as the game draws them. */
static void floating_panels(double vw, double vh, double k, float ox, float oy, OraclesUiDisplayLayout *out)
{
    out->preview_panels[0] = panel_piece(6.0, 6.0, 51.0, 11.0, k, ox, oy);
    for (int i = 0; i < 6; i++) out->preview_hearts[i] = panel_piece(8.0 + 8.0 * i, 8.0, 7.0, 7.0, k, ox, oy);
    out->preview_panels[1] = panel_piece(vw - 34.0, 6.0, 28.0, 11.0, k, ox, oy);
    out->preview_inks[0] = panel_piece(vw - 30.0, 8.0, 20.0, 7.0, k, ox, oy);
    const double y0 = vh - 6.0 - 18.0;
    for (int c = 0; c < 2; c++) {
        const double cx = c ? 37.0 : 6.0;
        out->preview_panels[2 + c] = panel_piece(cx, y0, 27.0, 18.0, k, ox, oy);
        out->preview_inks[1 + c] = panel_piece(cx + 9.0, y0 + 2.0, 14.0, 14.0, k, ox, oy);
        out->preview_badges[c] = panel_piece(cx + 1.0, y0 + 2.0, 6.0, 9.0, k, ox, oy);
    }
    out->preview_panel_edge = fmaxf(1.0f, round_d(k));
}

/* The band's frame from (x, y), at most `room_w` x `room_h`, and what it holds: the status bar's band, the box at the
 * size it takes, its text's place and its arrow. */
static void preview_frame(const OraclesDisplayTextPreview *p, float x, float y, float room_w, float room_h, OraclesUiDisplayLayout *out)
{
    /* In double precision, as the mockup's script computes it: 314 / 480 * 360 is 235.5 there, 235.49998 in single. */
    const double vw = (double)p->view_w, vh = (double)p->view_h;
    const double scale = vw > 0.0 && vh > 0.0 ? fmin((double)room_w / vw, (double)room_h / vh) : 1.0;
    const double unit = scale * (double)p->scale_num / (double)(p->scale_den ? p->scale_den : 1u);
    out->preview_frame = box(x, y, round_d(vw * scale), round_d(vh * scale));
    const float in_x = x + PREVIEW_BORDER, in_y = y + PREVIEW_BORDER;
    out->preview_hud = box(in_x, in_y, out->preview_frame.w - 2.0f * PREVIEW_BORDER, p->floating ? 0.0f : round_d(HUD_H * scale));
    if (p->floating) floating_panels(vw, vh, scale, in_x, in_y, out);
    const double bw = (double)p->box_w * unit;
    out->preview_box = box(in_x + round_d((vw * scale - bw) * 0.5), in_y + round_d(vh * scale - BOX_BOTTOM * scale - BOX_H * unit),
                           round_d(bw), round_d(BOX_H * unit));
    out->pixel = (float)unit;
    out->text_x = out->preview_box.x + round_d(TEXT_LEFT * unit);
    out->text_y = out->preview_box.y + round_d(TEXT_TOP * unit);
    const float arrow = fmaxf(ARROW, round_d(ARROW * unit));
    out->preview_arrow = box(out->preview_box.x + out->preview_box.w - round_d(ARROW_RIGHT * unit) - 2.0f * arrow,
                             out->preview_box.y + out->preview_box.h - round_d(ARROW_BOTTOM * unit) - arrow, 2.0f * arrow, arrow);
}

void oracles_ui_text_preview_pixels(const OraclesDisplayTextPreview *p, OraclesUiBox frame, float unit, OraclesUiTextPreviewPixels *out)
{
    const float x0 = roundf(frame.x), y0 = roundf(frame.y), x1 = roundf(frame.x + frame.w), y1 = roundf(frame.y + frame.h);
    out->clip = box(x0, y0, x1 - x0, y1 - y0);
    const float to_band = (float)(p->scale_den ? p->scale_den : 1u) / (float)(p->scale_num ? p->scale_num : 1u);
    if (unit >= 1.0f) {
        /* The band filling the frame, the box at its place and its proportion in it, its edges on whole pixels. */
        const float band = unit * to_band, bw = (float)p->box_w * unit, bh = BOX_H * unit;
        const float bx = x0 + (out->clip.w - bw) * 0.5f, by = y1 - BOX_BOTTOM * band - bh;
        out->pixel = unit;
        out->letterbox = 0;
        out->view = out->clip;
        out->hud = box(x0, y0, out->clip.w, p->floating ? 0.0f : roundf(HUD_H * band));
        out->box = box(roundf(bx), roundf(by), roundf(bx + bw) - roundf(bx), roundf(by + bh) - roundf(by));
        return;
    }
    out->pixel = 1.0f;
    const float vw = roundf((float)p->view_w * to_band), vh = roundf((float)p->view_h * to_band);
    const float bw = (float)p->box_w, bx = roundf((vw - bw) * 0.5f), by = vh - roundf(BOX_BOTTOM * to_band) - BOX_H;
    /* Across, the band centred, its box with it, unless the text is wider than the frame: then the box from its left. */
    const float vx = (float)p->text_w > out->clip.w ? x0 - bx : x0 + floorf((out->clip.w - vw) * 0.5f);
    const float vy = vh <= out->clip.h ? y0 + floorf((out->clip.h - vh) * 0.5f) : y1 - vh;
    out->letterbox = vx > x0 || vy > y0 || vx + vw < x1 || vy + vh < y1;
    out->view = box(vx, vy, vw, vh);
    out->hud = box(vx, vy, vw, p->floating ? 0.0f : roundf(HUD_H * to_band));
    out->box = box(vx + bx, vy + by, bw, BOX_H);
}

int oracles_ui_text_preview_picture(const OraclesDisplayTextPreview *p, const uint32_t colors[3], uint32_t *out)
{
    const unsigned w = p->box_w > 0 && (unsigned)p->box_w < ORACLES_UI_PREVIEW_PICTURE_W ? (unsigned)p->box_w : ORACLES_UI_PREVIEW_PICTURE_W;
    const unsigned h = ORACLES_UI_PREVIEW_PICTURE_H, left = (unsigned)TEXT_LEFT, top = (unsigned)TEXT_TOP;
    unsigned text_w = p->text_w > 0 ? (unsigned)p->text_w : 0u;
    if (text_w > ORACLES_DISPLAY_SAMPLE_W) text_w = ORACLES_DISPLAY_SAMPLE_W;
    if (left + text_w > w) text_w = w - left;
    for (unsigned i = 0; i < w * h; i++) out[i] = colors[0];
    for (unsigned row = 0; row < ORACLES_DISPLAY_SAMPLE_H && top + row < h; row++)
        for (unsigned col = 0; col < text_w; col++)
            if (p->sample[row][col]) out[(top + row) * w + left + col] = colors[1];
    /* The arrow, a triangle pointing down, each row a pixel shorter at either end. */
    const unsigned arrow = (unsigned)ARROW, ax = w - (unsigned)ARROW_RIGHT - 2u * arrow, ay = h - (unsigned)ARROW_BOTTOM - arrow;
    for (unsigned row = 0; row < arrow; row++)
        for (unsigned col = row; col < 2u * arrow - row; col++) out[(ay + row) * w + ax + col] = colors[2];
    return (int)w;
}

/* The rows Text, Text box, Text size and Text speed. */
static const char *const *row_choices(unsigned i, unsigned *count) { return oracles_display_text_row_choices(ORACLES_DISPLAY_LETTERS + i, count); }

static OraclesUiOptionLayout *row_options(OraclesUiDisplayLayout *out, unsigned i)
{
    return i == 0u ? out->text : i == 1u ? out->text_box : i == 2u ? out->text_size : out->text_speed;
}

float oracles_ui_text_page_16_9(const OraclesUiDisplayTexts *t, float preview_y, float x, float y, float w, float label_x, OraclesUiDisplayLayout *out)
{
    preview_frame(&t->preview, HEAD_X, preview_y, PREVIEW_W, PREVIEW_H, out);
    const float label_y = out->preview_frame.y + out->preview_frame.h + PREVIEW_GAP;
    out->preview_label = line_at(&oracles_ui_diagram_label, t->preview.label, HEAD_X, label_y, 0.0f);
    out->preview_note = line_at(&oracles_ui_row_note, t->preview.note, HEAD_X, label_y + out->preview_label.h + PREVIEW_GAP, 0.0f);
    out->preview = box(HEAD_X, preview_y, HEAD_W, out->preview_note.y + PREVIEW_NOTE_H - preview_y);
    for (unsigned i = 0; i < ORACLES_DISPLAY_TEXT_ROWS; i++) {
        const unsigned row = ORACLES_DISPLAY_LETTERS + i;
        unsigned count;
        const char *const *names = row_choices(i, &count);
        const char *const notes[1] = { t->text_notes[i] };
        OraclesUiLine *const notes_out[1] = { &out->text_notes[i] };
        if (i) y += DISPLAY_ROW_GAP;
        y += oracles_ui_display_row(x, y, w, oracles_display_labels[row], label_x, t->text_explanations[i], EXPLANATION_H, notes,
                                    t->text_notes[i] && t->text_notes[i][0] ? 1 : 0, names, NULL, count, &out->labels[row],
                                    &out->text_explanations[i], notes_out, row_options(out, i), &out->rows[row]);
    }
    return y;
}

void oracles_ui_text_page_4_3(const OraclesUiDisplayTexts *t, OraclesUiDisplayLayout *out)
{
    const float x = ROWS_X_4_3 + ROW_PAD_X_4_3;
    preview_frame(&t->preview, x, ROWS_Y_4_3, PREVIEW_W_4_3, PREVIEW_H_4_3, out);
    const float text_x = x + out->preview_frame.w + PREVIEW_GAP_4_3;
    out->preview_label = line_at(&oracles_ui_diagram_label_4_3, t->preview.label, text_x, 0.0f, 0.0f);
    out->preview_note = line_at(&oracles_ui_page_note_4_3, t->preview.note, text_x, 0.0f, 0.0f);
    const float column_h = out->preview_label.h + PREVIEW_LINE_GAP_4_3 + PREVIEW_NOTE_H_4_3 + PREVIEW_LINE_GAP_4_3;
    const float h = fmaxf(out->preview_frame.h, column_h), bottom = ROWS_Y_4_3 + h;
    /* The frame and the label's column stand on the preview's bottom. */
    out->preview_frame.y = bottom - out->preview_frame.h;
    preview_frame(&t->preview, x, out->preview_frame.y, PREVIEW_W_4_3, PREVIEW_H_4_3, out);
    move_line(&out->preview_note, text_x, bottom - PREVIEW_LINE_GAP_4_3 - PREVIEW_NOTE_H_4_3);
    move_line(&out->preview_label, text_x, out->preview_note.y - PREVIEW_LINE_GAP_4_3 - out->preview_label.h);
    out->preview = box(ROWS_X_4_3, ROWS_Y_4_3, ROWS_W_4_3, h);
    float y = bottom + PREVIEW_MARGIN_4_3 + DISPLAY_ROW_GAP_4_3;
    for (unsigned i = 0; i < ORACLES_DISPLAY_TEXT_ROWS; i++) {
        unsigned count;
        const char *const *names = row_choices(i, &count);
        y = oracles_ui_display_options_row_4_3(y, ORACLES_DISPLAY_LETTERS + i, names, NULL, NULL, count, row_options(out, i), out) + DISPLAY_ROW_GAP_4_3;
    }
}
