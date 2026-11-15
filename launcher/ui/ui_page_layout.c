#include "ui_page_layout_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The pages' text styles. */
const OraclesUiTextStyle oracles_ui_page_section = { ORACLES_UI_FONT_SERIF, 26.0f, 0.2f, 1 };
const OraclesUiTextStyle oracles_ui_page_over = { ORACLES_UI_FONT_SERIF, 44.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_page_title = { ORACLES_UI_FONT_SERIF, 104.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_page_state = { ORACLES_UI_FONT_SANS, 24.0f, 0.16f, 1 };
const OraclesUiTextStyle oracles_ui_page_credit = { ORACLES_UI_FONT_SANS, 24.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_row_label = { ORACLES_UI_FONT_SERIF, 22.0f, 0.18f, 1 };
const OraclesUiTextStyle oracles_ui_row_title = { ORACLES_UI_FONT_SANS_MEDIUM, 28.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_row_path = { ORACLES_UI_FONT_MONO, 21.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_row_status = { ORACLES_UI_FONT_SANS, 25.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_row_text = { ORACLES_UI_FONT_SANS, 24.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_row_note = { ORACLES_UI_FONT_SANS_ITALIC, 22.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_row_button = { ORACLES_UI_FONT_SERIF, 32.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_option_name = { ORACLES_UI_FONT_SERIF, 27.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_option_size = { ORACLES_UI_FONT_MONO, 20.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_choice = { ORACLES_UI_FONT_SERIF, 25.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_play = { ORACLES_UI_FONT_SERIF, 54.0f, 0.0f, 0 };

const OraclesUiTextStyle oracles_ui_diagram_label = { ORACLES_UI_FONT_SANS, 22.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_mod_games = { ORACLES_UI_FONT_SANS, 22.0f, 0.0f, 0 };

/* 4:3's, for a 640x480 screen: 27 px at least, the running text 30 and 36. */
const OraclesUiTextStyle oracles_ui_page_section_4_3 = { ORACLES_UI_FONT_SERIF, 30.0f, 0.18f, 1 };
const OraclesUiTextStyle oracles_ui_page_title_4_3 = { ORACLES_UI_FONT_SERIF, 60.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_page_state_4_3 = { ORACLES_UI_FONT_SANS, 27.0f, 0.14f, 1 };
const OraclesUiTextStyle oracles_ui_page_credit_4_3 = { ORACLES_UI_FONT_SANS, 27.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_page_note_4_3 = { ORACLES_UI_FONT_SANS_ITALIC, 27.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_row_label_4_3 = { ORACLES_UI_FONT_SERIF, 30.0f, 0.14f, 1 };
const OraclesUiTextStyle oracles_ui_row_title_4_3 = { ORACLES_UI_FONT_SANS_MEDIUM, 36.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_row_path_4_3 = { ORACLES_UI_FONT_MONO, 27.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_row_text_4_3 = { ORACLES_UI_FONT_SANS, 30.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_row_note_4_3 = { ORACLES_UI_FONT_SANS_ITALIC, 30.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_row_button_4_3 = { ORACLES_UI_FONT_SERIF, 36.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_play_4_3 = { ORACLES_UI_FONT_SERIF, 60.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_option_size_4_3 = { ORACLES_UI_FONT_MONO, 27.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_diagram_label_4_3 = { ORACLES_UI_FONT_SANS, 27.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_help_4_3 = { ORACLES_UI_FONT_SANS, 36.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_mod_games_4_3 = { ORACLES_UI_FONT_SANS, 27.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_quality_sets_4_3 = { ORACLES_UI_FONT_SANS, 27.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_language_4_3 = { ORACLES_UI_FONT_SERIF, 30.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_display_title_4_3 = { ORACLES_UI_FONT_SERIF, 30.0f, 0.0f, 0 };

static const OraclesUiPageStyles styles_16_9 = {
    &oracles_ui_page_section, &oracles_ui_page_over, &oracles_ui_page_title, &oracles_ui_page_state, &oracles_ui_row_note,
    &oracles_ui_row_label, &oracles_ui_row_title, &oracles_ui_row_path, &oracles_ui_row_status, &oracles_ui_row_text, &oracles_ui_row_note,
    &oracles_ui_row_button, &oracles_ui_play,
    &oracles_ui_option_name, &oracles_ui_option_size, &oracles_ui_choice, &oracles_ui_diagram_label, &oracles_ui_row_text, &oracles_ui_row_note,
    &oracles_ui_mod_games, &oracles_ui_row_status, &oracles_ui_row_note, &oracles_ui_option_size,
    &oracles_ui_choice,
};
/* A choice is framed as a profile is, its name in the same style, the buttons'. */
static const OraclesUiPageStyles styles_4_3 = {
    &oracles_ui_page_section_4_3, &oracles_ui_page_title_4_3, &oracles_ui_page_title_4_3, &oracles_ui_page_state_4_3,
    &oracles_ui_page_note_4_3,
    &oracles_ui_row_label_4_3, &oracles_ui_row_title_4_3, &oracles_ui_row_path_4_3, &oracles_ui_row_text_4_3, &oracles_ui_row_text_4_3,
    &oracles_ui_row_note_4_3, &oracles_ui_row_button_4_3, &oracles_ui_play_4_3,
    &oracles_ui_row_button_4_3, &oracles_ui_option_size_4_3, &oracles_ui_row_button_4_3, &oracles_ui_diagram_label_4_3, &oracles_ui_help_4_3,
    &oracles_ui_row_note_4_3,
    &oracles_ui_mod_games_4_3, &oracles_ui_row_text_4_3, &oracles_ui_page_note_4_3, &oracles_ui_quality_sets_4_3,
    &oracles_ui_language_4_3,
};

const OraclesUiPageStyles *oracles_ui_page_styles(OraclesUiLayout layout) { return layout == ORACLES_UI_LAYOUT_4_3 ? &styles_4_3 : &styles_16_9; }

const char oracles_ui_transitions_note[] = "A savestate taken with this on is refused with it off.";
const char oracles_ui_core_note[] = "Save states keep their core.";
const char oracles_ui_scaling_note[] = "Fill applies in fullscreen.";
const char oracles_ui_window_fit_note[] = "Window sizes follow the view: a farther view makes a larger window.";
const char oracles_ui_display_later[] = "Grayed rows change from the home screen; the rest applies at once.";
const char *const oracles_ui_transition_choices[2] = { "Off", "On" };

OraclesUiLine pl_line_at(const OraclesUiTextStyle *style, const char *text, float x, float y, float line_height)
{
    OraclesUiLine line;
    float baseline;
    oracles_ui_text_line_box(style, text, line_height, &line.h, &baseline);
    line.w = text && text[0] ? oracles_ui_text_width(style, text) : 0.0f;
    line.x = x;
    line.y = y;
    line.baseline = y + baseline;
    return line;
}

/* An empty block has no height. */
static OraclesUiLine block_at(const OraclesUiTextStyle *style, const char *text, float x, float y)
{
    OraclesUiLine line = pl_line_at(style, text, x, y, 0.0f);
    if (!text || !text[0]) line.h = 0.0f;
    return line;
}

void pl_move_line(OraclesUiLine *line, float x, float y)
{
    line->baseline += y - line->y;
    line->x = x;
    line->y = y;
}

OraclesUiBox pl_box(float x, float y, float w, float h)
{
    const OraclesUiBox b = { x, y, w, h };
    return b;
}

/* `text` without `cut` bytes around its middle, whole UTF-8 sequences, an ellipsis in their place. */
static void cut_middle(const char *text, size_t length, size_t cut, char *out)
{
    size_t head = (length - cut) / 2, tail = head + cut;
    while (head > 0 && ((unsigned char)text[head] & 0xc0u) == 0x80u) head--;
    while (tail < length && ((unsigned char)text[tail] & 0xc0u) == 0x80u) tail++;
    memcpy(out, text, head);
    memcpy(out + head, "\xe2\x80\xa6", 3);
    memcpy(out + head + 3, text + tail, length - tail + 1);
}

void oracles_ui_copy_middle(char *out, size_t capacity, const char *text, size_t length)
{
    if (!capacity) return;
    if (length < capacity) { memcpy(out, text, length); out[length] = 0; return; }
    if (capacity < 4) { out[0] = 0; return; }
    cut_middle(text, length, length + 4 - capacity, out);
}

void oracles_ui_copy_whole(char *out, size_t capacity, const char *text)
{
    if (!capacity) return;
    size_t length = strlen(text);
    if (length >= capacity) {
        length = capacity - 1;
        while (length > 0 && ((unsigned char)text[length] & 0xc0u) == 0x80u) length--;
    }
    memcpy(out, text, length);
    out[length] = 0;
}

void oracles_ui_append_middle(char *out, size_t capacity, const char *text)
{
    size_t used = strnlen(out, capacity);
    if (used && used + 1 < capacity) { out[used++] = ' '; out[used] = 0; }
    if (used < capacity) oracles_ui_copy_middle(out + used, capacity - used, text, strlen(text));
}

/* The elisions done lately: a page drawn at every frame elides each text once, when it changes. */
#define FIT_CACHE 8
#define FIT_CACHE_TEXT 1024
static struct {
    const OraclesUiTextStyle *style;
    float width;
    size_t capacity;
    char text[FIT_CACHE_TEXT], out[FIT_CACHE_TEXT];
} fit_cache[FIT_CACHE];
static unsigned fit_cache_next;

void oracles_ui_fit(const OraclesUiTextStyle *style, const char *text, float width, char *out, size_t capacity)
{
    if (!capacity) return;
    if (!text) text = "";
    const size_t length = strlen(text);
    const int cacheable = length < FIT_CACHE_TEXT && capacity <= FIT_CACHE_TEXT;
    if (cacheable)
        for (unsigned i = 0; i < FIT_CACHE; i++)
            if (fit_cache[i].style == style && fit_cache[i].width == width && fit_cache[i].capacity == capacity && !strcmp(fit_cache[i].text, text)) {
                memcpy(out, fit_cache[i].out, strlen(fit_cache[i].out) + 1);
                return;
            }
    char *candidate = malloc(length + 4);
    if (!candidate) { oracles_ui_copy_middle(out, capacity, text, length); return; }
    if (length < capacity && oracles_ui_text_width(style, text) <= width) {
        memcpy(candidate, text, length + 1);
    } else {
        /* The fewest bytes dropped around the middle for the text with its ellipsis to fit, and to hold in `out`: the
         * width shrinks as bytes go, so a bisection finds them. */
        size_t low = length + 4 > capacity ? length + 4 - capacity : 1, high = length;
        if (low > high) low = high;
        while (low < high) {
            const size_t cut = low + (high - low) / 2;
            cut_middle(text, length, cut, candidate);
            if (oracles_ui_text_width(style, candidate) <= width) high = cut; else low = cut + 1;
        }
        cut_middle(text, length, low, candidate);
    }
    oracles_ui_copy_middle(out, capacity, candidate, strlen(candidate));
    free(candidate);
    if (cacheable) {
        fit_cache[fit_cache_next].style = style;
        fit_cache[fit_cache_next].width = width;
        fit_cache[fit_cache_next].capacity = capacity;
        memcpy(fit_cache[fit_cache_next].text, text, length + 1);
        snprintf(fit_cache[fit_cache_next].out, FIT_CACHE_TEXT, "%s", out);
        fit_cache_next = (fit_cache_next + 1) % FIT_CACHE;
    }
}

void pl_page_head_16_9(const char *section, const char *over, const char *title, const char *state, OraclesUiPageHead *out)
{
    memset(out, 0, sizeof *out);
    out->section = pl_line_at(&oracles_ui_page_section, section, HEAD_X, HEAD_Y, 0.0f);
    out->over = pl_line_at(&oracles_ui_page_over, over, HEAD_X, out->section.y + out->section.h + HEAD_SECTION_GAP, 0.0f);
    /* The title in the column's width, each line as high as the title's font (line-height 1). */
    const float title_y = out->over.y + out->over.h;
    oracles_ui_wrap(&oracles_ui_page_title, title, HEAD_W, HEAD_X, title_y, &out->title);
    for (unsigned i = 0; i < out->title.count; i++)
        out->title.lines[i] = pl_line_at(&oracles_ui_page_title, out->title.text[i], HEAD_X, title_y + (float)i * HEAD_TITLE_LINE, HEAD_TITLE_LINE);
    out->title.h = (float)out->title.count * HEAD_TITLE_LINE;
    out->state = pl_line_at(&oracles_ui_page_state, state, HEAD_X, title_y + out->title.h + HEAD_STATE_MARGIN, 0.0f);
}

/* A button at the right of a row, centred on the row's content: its label and its dot. */
void pl_button(const char *label, float right, float top, float height, OraclesUiLine *text, OraclesUiLine *dot)
{
    *dot = pl_line_at(&oracles_ui_row_button, oracles_ui_item_dot, 0.0f, 0.0f, 0.0f);
    *text = pl_line_at(&oracles_ui_row_button, label, 0.0f, 0.0f, 0.0f);
    pl_move_line(dot, right - dot->w, top + (height - dot->h) * 0.5f);
    pl_move_line(text, dot->x - BUTTON_GAP - text->w, dot->y);
}

/* The width of `length` characters of a monospaced style: a browser's `ch`. */
float pl_characters(const OraclesUiTextStyle *style, int length) { return (float)length * oracles_ui_text_width(style, "0"); }

/* Options framed with a name and a size (profiles, windows), in a line from (x, y), each size line `size_lengths`
 * characters wide at least (NULL: its own width).  Returns their height. */
float pl_framed(const char *const *names, const char *const *sizes, const int *size_lengths, unsigned count, float x, float y,
                    OraclesUiOptionLayout *out)
{
    float height = 0.0f;
    for (unsigned i = 0; i < count; i++) {
        OraclesUiOptionLayout *o = &out[i];
        o->name = pl_line_at(&oracles_ui_option_name, names[i], x + BORDER + OPTION_PAD_X, y + BORDER + OPTION_PAD_Y, 0.0f);
        o->size = pl_line_at(&oracles_ui_option_size, sizes[i], o->name.x, o->name.y + o->name.h + OPTION_LINE_GAP, 0.0f);
        const float size_w = size_lengths && pl_characters(&oracles_ui_option_size, size_lengths[i]) > o->size.w
                             ? pl_characters(&oracles_ui_option_size, size_lengths[i]) : o->size.w;
        const float inner = o->name.w > size_w ? o->name.w : size_w;
        o->box = pl_box(x, y, inner + 2.0f * (OPTION_PAD_X + BORDER), o->size.y + o->size.h + OPTION_PAD_Y + BORDER - y);
        height = o->box.h;
        x += o->box.w + OPTION_GAP;
    }
    return height;
}

/* A row of choices framed, right-aligned at `right` in a line of `height` from `top`; with `sizes` (NULL: none), each
 * choice's size under its name, the two centred in the frame (View). */
void pl_choices(const char *const *names, const char *const *sizes, unsigned count, float right, float top, OraclesUiOptionLayout *out)
{
    float x = right;
    for (unsigned i = count; i-- > 0;) {
        OraclesUiOptionLayout *o = &out[i];
        o->name = pl_line_at(&oracles_ui_choice, names[i], 0.0f, 0.0f, 0.0f);
        memset(&o->size, 0, sizeof o->size);
        if (sizes) o->size = pl_line_at(&oracles_ui_option_size, sizes[i], 0.0f, 0.0f, 0.0f);
        const float inner = sizes && o->size.w > o->name.w ? o->size.w : o->name.w;
        const float inner_h = o->name.h + (sizes ? OPTION_LINE_GAP + o->size.h : 0.0f);
        o->box.w = inner + 2.0f * (CHOICE_PAD_X + BORDER);
        o->box.h = inner_h + 2.0f * (CHOICE_PAD_Y + BORDER);
        o->box.x = x - o->box.w;
        o->box.y = top;
        const float inner_x = o->box.x + BORDER + CHOICE_PAD_X;
        pl_move_line(&o->name, inner_x + (inner - o->name.w) * 0.5f, top + BORDER + CHOICE_PAD_Y);
        if (sizes) pl_move_line(&o->size, inner_x + (inner - o->size.w) * 0.5f, o->name.y + o->name.h + OPTION_LINE_GAP);
        x = o->box.x - CHOICE_GAP;
    }
}

void pl_shift_option(OraclesUiOptionLayout *o, float dx, float dy)
{
    o->box.x += dx;
    o->box.y += dy;
    pl_move_line(&o->name, o->name.x + dx, o->name.y + dy);
    if (o->size.h > 0.0f) pl_move_line(&o->size, o->size.x + dx, o->size.y + dy);
}

/* Buttons side by side, from the right edge `right` leftwards, each its label and its dot `gap` apart, `between` from
 * one to the next, unplaced vertically.  Returns their width. */
float pl_buttons(const OraclesUiTextStyle *style, const char *const *labels, unsigned count, float right, float gap, float between,
                 OraclesUiLine *texts, OraclesUiLine *dots)
{
    float x = right;
    for (unsigned i = count; i-- > 0;) {
        dots[i] = pl_line_at(style, oracles_ui_item_dot, 0.0f, 0.0f, 0.0f);
        texts[i] = pl_line_at(style, labels[i], 0.0f, 0.0f, 0.0f);
        pl_move_line(&dots[i], x - dots[i].w, 0.0f);
        pl_move_line(&texts[i], dots[i].x - gap - texts[i].w, 0.0f);
        x = texts[i].x - between;
    }
    return right - x - between;
}

/* Each button's box, from its label to its dot, where they lie. */
void pl_button_boxes(unsigned count, const OraclesUiLine *texts, const OraclesUiLine *dots, OraclesUiBox *boxes)
{
    for (unsigned i = 0; i < count; i++) boxes[i] = pl_box(texts[i].x, texts[i].y, dots[i].x + dots[i].w - texts[i].x, texts[i].h);
}

/* An edition's buttons, EU_BUTTONS_GAP between them. */
static float edition_buttons(const OraclesUiTextStyle *style, const OraclesUiGameTexts *t, int e, float right, float gap, OraclesUiGameLayout *out)
{
    return pl_buttons(style, t->edition_button_labels[e], t->edition_buttons[e], right, gap, EU_BUTTONS_GAP, out->edition_buttons[e],
                      out->edition_button_dots[e]);
}

/* The languages shown framed from (x, y) in their order, `columns` a line, each `width` wide and as tall as its name and
 * `frame` above and under it, `min_h` at least, a line as tall as its tallest, its name centred; the frames LANGUAGE_GAP
 * apart.  Returns the height of their lines. */
static float languages_at(const OraclesUiTextStyle *style, const OraclesUiGameTexts *t, float x, float y, float width, int columns, float frame,
                          float min_h, int centred, OraclesUiGameLayout *out)
{
    float top = y, bottom = y;
    for (int first = 0; first < t->language_count; first += columns) {
        float height = min_h;
        for (int i = first; i < t->language_count && i < first + columns; i++) {
            OraclesUiOptionLayout *o = &out->languages[t->language_order[i]];
            memset(&o->size, 0, sizeof o->size);
            o->name = pl_line_at(style, oracles_page_language_names[t->language_order[i]], 0.0f, 0.0f, 0.0f);
            height = pl_max_f(height, o->name.h + 2.0f * frame);
        }
        for (int i = first; i < t->language_count && i < first + columns; i++) {
            OraclesUiOptionLayout *o = &out->languages[t->language_order[i]];
            o->box = pl_box(x + (float)(i - first) * (width + LANGUAGE_GAP), top, width, height);
            pl_move_line(&o->name, o->box.x + (width - o->name.w) * 0.5f, centred ? top + (height - o->name.h) * 0.5f : top + frame);
        }
        bottom = top + height;
        top = bottom + LANGUAGE_GAP;
    }
    return bottom - y;
}

/* 16:9's row of edition `e` from `y`: its label (on lines of JOINED_LABEL_LINE where it does not hold on one), the file
 * and the buttons centred on one line, the folder's line and the status under them, each on one line.  Returns the y
 * after it. */
static float edition_16_9(OraclesUiGameTexts *t, int e, float left, float value_x, float value_w, float y, OraclesUiGameLayout *out)
{
    const float inner_x = value_x + ROW_PAD_X, inner_w = value_w - 2.0f * ROW_PAD_X, right = inner_x + inner_w, inner_y = y + ROW_PAD_Y;
    char fitted[ORACLES_HOME_TEXT_LENGTH * 4];
    OraclesUiWrapped *label = &out->edition_labels[e];
    oracles_ui_wrap(&oracles_ui_row_label, oracles_page_edition_labels[e], CARTRIDGE_LABEL_W, left, y + LABEL_PAD_TOP, label);
    if (label->count > 1) pl_wrap_lines(&oracles_ui_row_label, oracles_page_edition_labels[e], CARTRIDGE_LABEL_W, left, y + JOINED_LABEL_PAD_TOP, JOINED_LABEL_LINE, label);
    const float room = inner_w - ROW_GAP - edition_buttons(&oracles_ui_row_button, t, e, right, BUTTON_GAP, out);
    oracles_ui_fit(&oracles_ui_row_title, t->edition_file[e], room, fitted, sizeof t->edition_file[e]); memcpy(t->edition_file[e], fitted, sizeof t->edition_file[e]);
    oracles_ui_fit(&oracles_ui_row_path, t->edition_folder[e], inner_w, fitted, sizeof t->edition_folder[e]); memcpy(t->edition_folder[e], fitted, sizeof t->edition_folder[e]);
    oracles_ui_fit(&oracles_ui_row_status, t->edition_status[e], inner_w - DOT - DOT_GAP, fitted, sizeof t->edition_status[e]);
    memcpy(t->edition_status[e], fitted, sizeof t->edition_status[e]);
    out->edition_files[e] = pl_line_at(&oracles_ui_row_title, t->edition_file[e], inner_x, 0.0f, 0.0f);
    const float first = pl_max_f(out->edition_files[e].h, out->edition_buttons[e][0].h);
    pl_move_line(&out->edition_files[e], inner_x, inner_y + (first - out->edition_files[e].h) * 0.5f);
    for (unsigned i = 0; i < t->edition_buttons[e]; i++) {
        const float top = inner_y + (first - out->edition_buttons[e][i].h) * 0.5f;
        pl_move_line(&out->edition_buttons[e][i], out->edition_buttons[e][i].x, top);
        pl_move_line(&out->edition_button_dots[e][i], out->edition_button_dots[e][i].x, top);
    }
    pl_button_boxes(t->edition_buttons[e], out->edition_buttons[e], out->edition_button_dots[e], out->edition_button_boxes[e]);
    out->edition_folders[e] = pl_line_at(&oracles_ui_row_path, t->edition_folder[e], inner_x, inner_y + first + FILE_GAP, EU_FOLDER_LINE);
    const float status_y = out->edition_folders[e].y + out->edition_folders[e].h + FILE_GAP;
    out->edition_statuses[e] = pl_line_at(&oracles_ui_row_status, t->edition_status[e], inner_x + DOT + DOT_GAP, status_y, EDITION_STATUS_LINE);
    out->edition_dots[e] = pl_box(inner_x, status_y + (out->edition_statuses[e].h - DOT) * 0.5f, DOT, DOT);
    const unsigned row = oracles_page_edition_row(e);
    out->rows[row] = pl_box(value_x, y, value_w, out->edition_statuses[e].y + out->edition_statuses[e].h + ROW_PAD_Y - y);
    return y + out->rows[row].h + GAME_ROW_GAP;
}

/* Oracles Combined's two rows from `y`, each its label on lines of JOINED_LABEL_LINE, the file on the first line with
 * Choose… or where it is set at the right, both centred on it, and the status under them, each on one line.  Returns
 * the y after them. */
static float joined_16_9(OraclesUiGameTexts *t, float left, float value_x, float value_w, float y, OraclesUiGameLayout *out)
{
    const float inner_x = value_x + ROW_PAD_X, inner_w = value_w - 2.0f * ROW_PAD_X, right = inner_x + inner_w;
    char fitted[ORACLES_HOME_TEXT_LENGTH * 4];
    for (int h = 0; h < 2; h++) {
        pl_wrap_lines(&oracles_ui_row_label, t->joined_labels[h], CARTRIDGE_LABEL_W, left, y + JOINED_LABEL_PAD_TOP, JOINED_LABEL_LINE,
                      &out->joined_labels[h]);
        const float inner_y = y + ROW_PAD_Y;
        float side;
        if (t->joined_from[h][0]) {
            out->joined_from[h] = pl_line_at(&oracles_ui_row_note, t->joined_from[h], 0.0f, 0.0f, 0.0f);
            pl_move_line(&out->joined_from[h], right - out->joined_from[h].w, 0.0f);
            side = out->joined_from[h].w;
        } else {
            pl_button("Choose\xe2\x80\xa6", right, 0.0f, 0.0f, &out->joined_buttons[h], &out->joined_button_dots[h]);
            side = out->joined_button_dots[h].x + out->joined_button_dots[h].w - out->joined_buttons[h].x;
        }
        oracles_ui_fit(&oracles_ui_row_title, t->joined_file[h], inner_w - ROW_GAP - side, fitted, sizeof t->joined_file[h]);
        memcpy(t->joined_file[h], fitted, sizeof t->joined_file[h]);
        oracles_ui_fit(&oracles_ui_row_status, t->joined_status[h], inner_w - DOT - DOT_GAP, fitted, sizeof t->joined_status[h]);
        memcpy(t->joined_status[h], fitted, sizeof t->joined_status[h]);
        out->joined_files[h] = pl_line_at(&oracles_ui_row_title, t->joined_file[h], inner_x, 0.0f, 0.0f);
        const float side_h = t->joined_from[h][0] ? out->joined_from[h].h : out->joined_buttons[h].h;
        const float first = pl_max_f(out->joined_files[h].h, side_h);
        pl_move_line(&out->joined_files[h], inner_x, inner_y + (first - out->joined_files[h].h) * 0.5f);
        if (t->joined_from[h][0]) pl_move_line(&out->joined_from[h], out->joined_from[h].x, inner_y + (first - side_h) * 0.5f);
        else pl_button("Choose\xe2\x80\xa6", right, inner_y, first, &out->joined_buttons[h], &out->joined_button_dots[h]);
        const float status_y = inner_y + first + FILE_GAP;
        out->joined_status[h] = pl_line_at(&oracles_ui_row_status, t->joined_status[h], inner_x + DOT + DOT_GAP, status_y, 0.0f);
        out->joined_dots[h] = pl_box(inner_x, status_y + (out->joined_status[h].h - DOT) * 0.5f, DOT, DOT);
        out->rows[h ? ORACLES_ROW_JOINED : ORACLES_ROW_ROM] = pl_box(value_x, y, value_w, status_y + out->joined_status[h].h + ROW_PAD_Y - y);
        y += out->rows[h ? ORACLES_ROW_JOINED : ORACLES_ROW_ROM].h + GAME_ROW_GAP;
    }
    return y;
}

static void game_16_9(OraclesUiGameTexts *t, OraclesUiGameLayout *out)
{
    pl_page_head_16_9(t->section ? t->section : "Cartridge", t->over, t->title, t->state, &out->head);
    if (t->credit) out->head.credit = pl_line_at(&oracles_ui_page_credit, t->credit, HEAD_X, out->head.state.y + out->head.state.h + CREDIT_MARGIN, 0.0f);
    const float left = PANEL_X + PANEL_BORDER + PANEL_PAD_X, width = PANEL_W - 2.0f * (PANEL_BORDER + PANEL_PAD_X);
    const float value_x = left + CARTRIDGE_LABEL_W + GRID_GAP, value_w = width - CARTRIDGE_LABEL_W - GRID_GAP;
    const float inner_x = value_x + ROW_PAD_X, inner_w = value_w - 2.0f * ROW_PAD_X, right = inner_x + inner_w;
    float y = PANEL_Y + PANEL_BORDER + PANEL_PAD_Y;
    /* Cartridge › Editions: its three rows alone. */
    if (t->editions) {
        for (int e = 0; e < ORACLES_PAGE_EDITIONS; e++) y = edition_16_9(t, e, left, value_x, value_w, y, out);
        out->panel = pl_box(PANEL_X, PANEL_Y, PANEL_W, y - GAME_ROW_GAP + PANEL_PAD_Y + PANEL_BORDER - PANEL_Y);
        return;
    }

    float inner_y = y + ROW_PAD_Y, content;
    char fitted[ORACLES_HOME_TEXT_LENGTH * 4];
    if (t->joined) {
        y = joined_16_9(t, left, value_x, value_w, y, out);
    } else {
        /* ROM: the file, its folder, its status; Choose ROM at the right.  A fan game's base ROM: the file and its status. */
        out->label_rom = pl_line_at(&oracles_ui_row_label, t->patched ? "Base ROM" : "ROM", left, y + LABEL_PAD_TOP, 0.0f);
        pl_button("Choose ROM\xe2\x80\xa6", right, 0.0f, 0.0f, &out->rom_button, &out->rom_button_dot);
        const float room = inner_w - ROW_GAP - (out->rom_button_dot.x + out->rom_button_dot.w - out->rom_button.x);
        oracles_ui_fit(&oracles_ui_row_title, t->rom_file, room, fitted, sizeof t->rom_file); memcpy(t->rom_file, fitted, sizeof t->rom_file);
        oracles_ui_fit(&oracles_ui_row_path, t->rom_folder, room, fitted, sizeof t->rom_folder); memcpy(t->rom_folder, fitted, sizeof t->rom_folder);
        out->rom_file = block_at(&oracles_ui_row_title, t->rom_file, inner_x, inner_y);
        float status_y = out->rom_file.y + out->rom_file.h + FILE_GAP;
        if (!t->patched) {
            out->rom_folder = block_at(&oracles_ui_row_path, t->rom_folder, inner_x, status_y);
            status_y = out->rom_folder.y + out->rom_folder.h + FILE_GAP + STATUS_MARGIN;
        }
        /* The status broken into lines beside its dot, which is centred on them. */
        oracles_ui_wrap(&oracles_ui_row_status, t->rom_status, room - DOT - DOT_GAP, inner_x + DOT + DOT_GAP, status_y, &out->rom_status);
        out->rom_dot = pl_box(inner_x, status_y + (out->rom_status.h - DOT) * 0.5f, DOT, DOT);
        /* A ROM that plays in Faithful with Enhanced chosen, or without the item hotkeys Controls has on: a line or two each
         * under the status. */
        float rom_bottom = status_y + out->rom_status.h;
        oracles_ui_wrap(&oracles_ui_row_text, t->rom_note, room, inner_x, rom_bottom + FILE_GAP, &out->rom_note);
        if (out->rom_note.count) rom_bottom = out->rom_note.lines[0].y + out->rom_note.h;
        oracles_ui_wrap(&oracles_ui_row_text, t->rom_hotkeys_note, room, inner_x, rom_bottom + FILE_GAP, &out->rom_hotkeys_note);
        if (out->rom_hotkeys_note.count) rom_bottom = out->rom_hotkeys_note.lines[0].y + out->rom_hotkeys_note.h;
        content = rom_bottom - inner_y;
        if (content < out->rom_button.h) content = out->rom_button.h;
        pl_button("Choose ROM\xe2\x80\xa6", right, inner_y, content, &out->rom_button, &out->rom_button_dot);
        out->rows[ORACLES_ROW_ROM] = pl_box(value_x, y, value_w, content + 2.0f * ROW_PAD_Y);
        y += out->rows[ORACLES_ROW_ROM].h + GAME_ROW_GAP;
    }

    if (t->patched) {
        /* Patch: the file and its status, Choose patch at the right; under the row, the patch's folder (its line kept
         * without a patch) and the image the two files make. */
        out->label_patch = pl_line_at(&oracles_ui_row_label, "Patch", left, y + LABEL_PAD_TOP, 0.0f);
        inner_y = y + ROW_PAD_Y;
        pl_button("Choose patch\xe2\x80\xa6", right, 0.0f, 0.0f, &out->patch_button, &out->patch_button_dot);
        const float patch_room = inner_w - ROW_GAP - (out->patch_button_dot.x + out->patch_button_dot.w - out->patch_button.x);
        oracles_ui_fit(&oracles_ui_row_title, t->patch_file, patch_room, fitted, sizeof t->patch_file); memcpy(t->patch_file, fitted, sizeof t->patch_file);
        out->patch_file = block_at(&oracles_ui_row_title, t->patch_file, inner_x, inner_y);
        /* The status broken into lines beside its dot, which is centred on them. */
        const float patch_status_y = out->patch_file.y + out->patch_file.h + FILE_GAP;
        oracles_ui_wrap_pretty(&oracles_ui_row_status, t->patch_status, patch_room - DOT - DOT_GAP, inner_x + DOT + DOT_GAP, patch_status_y,
                               &out->patch_status);
        out->patch_dot = pl_box(inner_x, patch_status_y + (out->patch_status.h - DOT) * 0.5f, DOT, DOT);
        content = patch_status_y + out->patch_status.h - inner_y;
        if (content < out->patch_button.h) content = out->patch_button.h;
        pl_button("Choose patch\xe2\x80\xa6", right, inner_y, content, &out->patch_button, &out->patch_button_dot);
        out->rows[ORACLES_ROW_PATCH] = pl_box(value_x, y, value_w, content + 2.0f * ROW_PAD_Y);
        const float line_w = value_w - 2.0f * ROW_PAD_X;
        oracles_ui_fit(&oracles_ui_row_path, t->patch_folder, line_w, fitted, sizeof t->patch_folder); memcpy(t->patch_folder, fitted, sizeof t->patch_folder);
        oracles_ui_fit(&oracles_ui_row_status, t->image_status, line_w - DOT - DOT_GAP, fitted, sizeof t->image_status); memcpy(t->image_status, fitted, sizeof t->image_status);
        out->patch_folder = pl_line_at(&oracles_ui_row_path, t->patch_folder, inner_x, y + out->rows[ORACLES_ROW_PATCH].h + PATCH_LINE_GAP, 0.0f);
        const float image_y = out->patch_folder.y + out->patch_folder.h + PATCH_LINE_GAP;
        out->image_status = pl_line_at(&oracles_ui_row_status, t->image_status, inner_x + DOT + DOT_GAP, image_y, 0.0f);
        out->image_dot = pl_box(inner_x, image_y + (out->image_status.h - DOT) * 0.5f, DOT, DOT);
        y = out->image_status.y + out->image_status.h + IMAGE_PAD_BOTTOM;
        /* Its notes under it, each on its line, in line with its text. */
        for (unsigned i = 0; i < t->image_notes; i++) {
            oracles_ui_fit(&oracles_ui_row_text, t->image_note[i], line_w - DOT - DOT_GAP, fitted, sizeof t->image_note[i]);
            memcpy(t->image_note[i], fitted, sizeof t->image_note[i]);
            out->image_notes[i] = pl_line_at(&oracles_ui_row_text, t->image_note[i], inner_x + DOT + DOT_GAP, y + IMAGE_NOTE_GAP, 0.0f);
            y = out->image_notes[i].y + out->image_notes[i].h + IMAGE_NOTE_PAD_BOTTOM;
        }
        y += GAME_ROW_GAP;
    }

    /* Save: the file and its actions, the line about it, then the status or the note. */
    out->label_save = pl_line_at(&oracles_ui_row_label, "Save", left, y + LABEL_PAD_TOP, 0.0f);
    out->rows[ORACLES_ROW_SAVE] = pl_save_row_16_9(t, value_x, value_w, y, out);
    y += out->rows[ORACLES_ROW_SAVE].h + GAME_ROW_GAP;

    if (t->european) {
        /* Editions: its explanation, and at the right its summary and its arrow, both centred on the row. */
        out->label_editions = pl_line_at(&oracles_ui_row_label, "Editions", left, y + LABEL_PAD_TOP, 0.0f);
        inner_y = y + EDITIONS_PAD_Y;
        out->editions_summary = pl_line_at(&oracles_ui_choice, t->editions_summary, 0.0f, 0.0f, 0.0f);
        const float link_w = out->editions_summary.w + EDITIONS_ARROW_GAP + EDITIONS_ARROW_W;
        oracles_ui_wrap(&oracles_ui_row_text, t->editions_explanation, inner_w - ROW_GAP - link_w, inner_x, inner_y, &out->editions_explanation);
        const float inner_h = pl_max_f(pl_max_f(out->editions_explanation.h, out->editions_summary.h), EDITIONS_ARROW_H);
        pl_move_wrapped(&out->editions_explanation, inner_y + (inner_h - out->editions_explanation.h) * 0.5f);
        pl_move_line(&out->editions_summary, right - link_w, inner_y + (inner_h - out->editions_summary.h) * 0.5f);
        out->editions_arrow = pl_box(right - EDITIONS_ARROW_W, inner_y + (inner_h - EDITIONS_ARROW_H) * 0.5f, EDITIONS_ARROW_W, EDITIONS_ARROW_H);
        out->editions_link = pl_box(right - link_w, inner_y, link_w, inner_h);
        out->rows[ORACLES_ROW_EDITIONS] = pl_box(value_x, y, value_w, inner_h + 2.0f * EDITIONS_PAD_Y);
        y += out->rows[ORACLES_ROW_EDITIONS].h + GAME_ROW_GAP;
    }
    if (t->languages) {
        /* Language: the languages shown in lines, then its explanation and its note. */
        out->label_language = pl_line_at(&oracles_ui_row_label, "Language", left, y + LABEL_PAD_TOP, 0.0f);
        inner_y = y + ROW_PAD_Y;
        const float frames = languages_at(&oracles_ui_choice, t, inner_x, inner_y, LANGUAGE_W, t->language_columns, BORDER + CHOICE_PAD_Y, 0.0f, 0, out);
        out->language_explanation = pl_line_at(&oracles_ui_row_text, t->language_explanation, inner_x, inner_y + frames + LANGUAGE_GAP, LANGUAGE_LINE);
        out->language_note = pl_line_at(&oracles_ui_row_note, t->language_note, inner_x, out->language_explanation.y + LANGUAGE_LINE + LANGUAGE_GAP, LANGUAGE_LINE);
        out->rows[ORACLES_ROW_LANGUAGE] = pl_box(value_x, y, value_w, out->language_note.y + LANGUAGE_LINE + ROW_PAD_Y - y);
        y += out->rows[ORACLES_ROW_LANGUAGE].h + GAME_ROW_GAP;
    }

    /* Play, right-aligned across the whole panel, with a note when it cannot. */
    const float play_right = left + width - ROW_PAD_X;
    out->play_dot = pl_line_at(&oracles_ui_play, oracles_ui_item_dot, 0.0f, y + PLAY_PAD_Y, 0.0f);
    pl_move_line(&out->play_dot, play_right - out->play_dot.w, out->play_dot.y);
    out->play = pl_line_at(&oracles_ui_play, "Play", 0.0f, y + PLAY_PAD_Y, 0.0f);
    pl_move_line(&out->play, out->play_dot.x - PLAY_GAP - out->play.w, out->play.y);
    out->play_note = pl_line_at(&oracles_ui_row_text, t->play_note, 0.0f, 0.0f, 0.0f);
    pl_move_line(&out->play_note, out->play.x - PLAY_GAP - out->play_note.w, out->play.y + (out->play.h - out->play_note.h) * 0.5f);
    out->rows[ORACLES_ROW_PLAY] = pl_box(left, y, width, out->play.h + 2.0f * PLAY_PAD_Y);
    y += out->rows[ORACLES_ROW_PLAY].h;

    out->panel = pl_box(PANEL_X, PANEL_Y, PANEL_W, y + PANEL_PAD_Y + PANEL_BORDER - PANEL_Y);
}

/* `text` into `out`, cut short to its capacity. */
static void copy_text(char *out, size_t capacity, const char *text)
{
    size_t length = strlen(text);
    if (length >= capacity) length = capacity - 1;
    memcpy(out, text, length);
    out[length] = 0;
}

/* The length of the next piece of `p` a line may end after: up to a space, or through a dash the browser breaks after,
 * an en dash, or a hyphen not before a digit, with more of the word after it. */
size_t oracles_ui_wrap_piece(const char *p)
{
    size_t n = 0;
    while (p[n] && p[n] != ' ') {
        if (p[n] == '-' && p[n + 1] && p[n + 1] != ' ' && !(p[n + 1] >= '0' && p[n + 1] <= '9')) return n + 1;
        if (!strncmp(p + n, "\xe2\x80\x93", 3) && p[n + 3] && p[n + 3] != ' ') return n + 3;
        n++;
    }
    return n;
}

void oracles_ui_wrap(const OraclesUiTextStyle *style, const char *text, float width, float x, float y, OraclesUiWrapped *out)
{
    memset(out, 0, sizeof *out);
    float height, baseline;
    oracles_ui_line_box(style, 0.0f, &height, &baseline);
    const char *p = text ? text : "";
    char line[ORACLES_UI_WRAP_LENGTH] = "";
    int spaced = 0;   /* a space between the line and the next piece */
    while (*p && out->count < ORACLES_UI_WRAP_LINES) {
        /* The next piece, and the line with it if it fits; a line takes one piece at least. */
        const size_t piece = oracles_ui_wrap_piece(p);
        char candidate[ORACLES_UI_WRAP_LENGTH * 2];
        snprintf(candidate, sizeof candidate, "%s%s%.*s", line, line[0] && spaced ? " " : "", (int)piece, p);
        const int last = out->count == ORACLES_UI_WRAP_LINES - 1;
        if (line[0] && !last && oracles_ui_text_width(style, candidate) > width) {
            snprintf(out->text[out->count++], ORACLES_UI_WRAP_LENGTH, "%s", line);
            snprintf(line, sizeof line, "%.*s", (int)piece, p);
        } else {
            copy_text(line, sizeof line, candidate);
        }
        p += piece;
        spaced = *p == ' ';
        while (*p == ' ') p++;
    }
    if (line[0] && out->count < ORACLES_UI_WRAP_LINES) snprintf(out->text[out->count++], ORACLES_UI_WRAP_LENGTH, "%s", line);
    for (unsigned i = 0; i < out->count; i++) {
        out->lines[i] = pl_line_at(style, out->text[i], x, y + (float)i * height, 0.0f);
        if (out->lines[i].w > out->w) out->w = out->lines[i].w;
    }
    out->h = (float)out->count * height;
}

void oracles_ui_wrap_pretty(const OraclesUiTextStyle *style, const char *text, float width, float x, float y, OraclesUiWrapped *out)
{
    oracles_ui_wrap(style, text, width, x, y, out);
    if (out->count < 2 || strchr(out->text[out->count - 1], ' ')) return;
    OraclesUiWrapped pretty;
    oracles_ui_wrap(style, text, out->lines[out->count - 2].w - 1.0f, x, y, &pretty);
    if (pretty.count == out->count) *out = pretty;
}

/* `text` broken as text-wrap: pretty breaks it, in lines of `line_height`, the first at (x, y). */
void pl_wrap_pretty_lines(const OraclesUiTextStyle *style, const char *text, float width, float x, float y, float line_height,
                              OraclesUiWrapped *out)
{
    oracles_ui_wrap_pretty(style, text, width, x, y, out);
    for (unsigned i = 0; i < out->count; i++) out->lines[i] = pl_line_at(style, out->text[i], x, y + (float)i * line_height, line_height);
    out->h = (float)out->count * line_height;
}

void pl_move_wrapped(OraclesUiWrapped *w, float y)
{
    const float dy = y - (w->count ? w->lines[0].y : y);
    for (unsigned i = 0; i < w->count; i++) pl_move_line(&w->lines[i], w->lines[i].x, w->lines[i].y + dy);
}

/* A text wrapped at `width` in lines of `line_height`, the first at (x, y). */
void pl_wrap_lines(const OraclesUiTextStyle *style, const char *text, float width, float x, float y, float line_height, OraclesUiWrapped *out)
{
    oracles_ui_wrap(style, text, width, x, y, out);
    for (unsigned i = 0; i < out->count; i++) out->lines[i] = pl_line_at(style, out->text[i], x, y + (float)i * line_height, line_height);
    out->h = (float)out->count * line_height;
}

/* ---- 4:3 -------------------------------------------------------------------------------------------------------- */

float pl_max_f(float a, float b) { return a > b ? a : b; }

/* JavaScript's Math.round, as the mockup halves the diagram. */
float pl_round_half_up(float v) { return floorf(v + 0.5f); }

void pl_page_head_4_3(const char *section, const char *over, const char *title, const char *state, OraclesUiPageHead *out)
{
    memset(out, 0, sizeof *out);
    out->section = pl_line_at(&oracles_ui_page_section_4_3, section, HEAD_X_4_3, HEAD_Y_4_3, 0.0f);
    /* The title with its words when they are "Oracle of": "Oracle of Ages"; a fan game's alone. */
    char line[ORACLES_HOME_TEXT_LENGTH];
    if (over && !strcmp(over, "Oracle of")) snprintf(line, sizeof line, "%s %s", over, title ? title : "");
    else snprintf(line, sizeof line, "%s", title ? title : "");
    const float title_y = out->section.y + out->section.h + HEAD_GAP_4_3;
    pl_wrap_lines(&oracles_ui_page_title_4_3, line, HEAD_W_4_3, HEAD_X_4_3, title_y, HEAD_TITLE_LINE_4_3, &out->title);
    if (state) out->state = pl_line_at(&oracles_ui_page_state_4_3, state, HEAD_X_4_3, title_y + out->title.h + HEAD_GAP_4_3, 0.0f);
}

/* The lines of a grid row aligned on one baseline from `top`: their tops moved, the row's bottom returned.  A line's
 * height past its baseline counts whole (a path on two lines). */
float pl_baseline_row(OraclesUiLine *const *lines, unsigned count, float top)
{
    float ascent = 0.0f, descent = 0.0f;
    for (unsigned i = 0; i < count; i++) {
        ascent = pl_max_f(ascent, lines[i]->baseline - lines[i]->y);
        descent = pl_max_f(descent, lines[i]->y + lines[i]->h - lines[i]->baseline);
    }
    for (unsigned i = 0; i < count; i++) pl_move_line(lines[i], lines[i]->x, top + ascent - (lines[i]->baseline - lines[i]->y));
    return top + ascent + descent;
}

/* A button and its dot at the right of the rows, unplaced vertically; returns their width. */
float pl_place_button_4_3(const char *label, OraclesUiLine *text, OraclesUiLine *dot)
{
    *dot = pl_line_at(&oracles_ui_row_button_4_3, oracles_ui_item_dot, 0.0f, 0.0f, 0.0f);
    pl_move_line(dot, RIGHT_4_3 - dot->w, 0.0f);
    *text = pl_line_at(&oracles_ui_row_button_4_3, label, 0.0f, 0.0f, 0.0f);
    pl_move_line(text, dot->x - BUTTON_GAP_4_3 - text->w, 0.0f);
    return dot->x + dot->w - text->x;
}

/* The values' room left of a button `button_w` wide. */
float pl_file_room_4_3(float value_x, float button_w) { return RIGHT_4_3 - button_w - GRID_GAP_4_3 - value_x; }

/* A status: its dot, centred on the line, then its text. Returns the line's bottom. */
static float status_4_3(const char *text, float y, OraclesUiLine *line, OraclesUiBox *dot)
{
    *line = pl_line_at(&oracles_ui_row_text_4_3, text, GAME_VALUE_X_4_3 + DOT_4_3 + DOT_GAP_4_3, y, 0.0f);
    *dot = pl_box(GAME_VALUE_X_4_3, y + (line->h - DOT_4_3) * 0.5f, DOT_4_3, DOT_4_3);
    return y + line->h;
}

/* A row's first line, the label, the file and the button on one baseline, from the row's top `y`; returns its bottom. */
static float file_line_4_3(const char *label, OraclesUiLine *label_out, const char *file, OraclesUiLine *file_out, OraclesUiLine *button,
                           OraclesUiLine *dot, float y)
{
    *label_out = pl_line_at(&oracles_ui_row_label_4_3, label, LEFT_4_3, 0.0f, 0.0f);
    *file_out = pl_line_at(&oracles_ui_row_title_4_3, file, GAME_VALUE_X_4_3, 0.0f, 0.0f);
    OraclesUiLine *const lines[4] = { label_out, file_out, button, dot };
    return pl_baseline_row(lines, 4, y + ROW_PAD_Y_4_3);
}

/* The row's box from `y` to `bottom`, its padding under, 100 tall at least. */
OraclesUiBox pl_row_4_3(float y, float bottom)
{
    return pl_box(ROWS_X_4_3, y, ROWS_W_4_3, pl_max_f(ROW_MIN_H_4_3, bottom + ROW_PAD_Y_4_3 - y));
}

/* Play at the right of its row, centred in it, with its note before it. */
OraclesUiBox pl_play_4_3_row(const char *note, float y, OraclesUiLine *note_out, OraclesUiLine *play, OraclesUiLine *dot)
{
    *dot = pl_line_at(&oracles_ui_play_4_3, oracles_ui_item_dot, 0.0f, 0.0f, 0.0f);
    *play = pl_line_at(&oracles_ui_play_4_3, "Play", 0.0f, 0.0f, 0.0f);
    *note_out = pl_line_at(&oracles_ui_row_text_4_3, note, 0.0f, 0.0f, 0.0f);
    const float h = pl_max_f(ROW_MIN_H_4_3, play->h);
    pl_move_line(dot, RIGHT_4_3 - dot->w, y + (h - dot->h) * 0.5f);
    pl_move_line(play, dot->x - PLAY_GAP_4_3 - play->w, y + (h - play->h) * 0.5f);
    pl_move_line(note_out, play->x - PLAY_GAP_4_3 - note_out->w, y + (h - note_out->h) * 0.5f);
    return pl_box(ROWS_X_4_3, y, ROWS_W_4_3, h);
}

#define FIT(style, field, room) do { oracles_ui_fit(style, field, room, fitted, sizeof field); memcpy(field, fitted, sizeof field); } while (0)

/* Oracles Combined's two rows from `y`: each its label on lines of JOINED_LABEL_LINE_4_3 from the row's top, the file and
 * Choose… on one baseline (Choose… laid out where the ROM is set too, where it is not drawn), the status under them.
 * Returns the y after them. */
static float joined_4_3(OraclesUiGameTexts *t, float y, OraclesUiGameLayout *out)
{
    char fitted[ORACLES_HOME_TEXT_LENGTH * 4];
    const float span = RIGHT_4_3 - GAME_VALUE_X_4_3;
    for (int h = 0; h < 2; h++) {
        const unsigned row = h ? ORACLES_ROW_JOINED : ORACLES_ROW_ROM;
        pl_wrap_lines(&oracles_ui_row_label_4_3, t->joined_labels[h], CARTRIDGE_LABEL_W_4_3, LEFT_4_3, y + JOINED_ROW_PAD_Y_4_3,
                      JOINED_LABEL_LINE_4_3, &out->joined_labels[h]);
        const float room = pl_file_room_4_3(GAME_VALUE_X_4_3, pl_place_button_4_3("Choose\xe2\x80\xa6", &out->joined_buttons[h], &out->joined_button_dots[h]));
        FIT(&oracles_ui_row_title_4_3, t->joined_file[h], room);
        FIT(&oracles_ui_row_text_4_3, t->joined_status[h], span - DOT_4_3 - DOT_GAP_4_3);
        out->joined_files[h] = pl_line_at(&oracles_ui_row_title_4_3, t->joined_file[h], GAME_VALUE_X_4_3, 0.0f, 0.0f);
        OraclesUiLine *const lines[3] = { &out->joined_files[h], &out->joined_buttons[h], &out->joined_button_dots[h] };
        const float bottom = status_4_3(t->joined_status[h], pl_baseline_row(lines, 3, y + JOINED_ROW_PAD_Y_4_3) + GRID_ROW_GAP_4_3,
                                        &out->joined_status[h], &out->joined_dots[h]);
        out->rows[row] = pl_box(ROWS_X_4_3, y, ROWS_W_4_3, pl_max_f(ROW_MIN_H_4_3, bottom + JOINED_ROW_PAD_Y_4_3 - y));
        y += out->rows[row].h + ROW_GAP_4_3;
    }
    return y;
}

/* 4:3's row of edition `e` from `y`: the European ROM's label, file and buttons on one baseline; the others' labels on
 * lines of JOINED_LABEL_LINE_4_3 from the row's top, their file and buttons on one baseline; the folder's line and the
 * status under them, each on one line.  Returns the y after it. */
static float edition_4_3(OraclesUiGameTexts *t, int e, float y, OraclesUiGameLayout *out)
{
    char fitted[ORACLES_HOME_TEXT_LENGTH * 4];
    const float span = RIGHT_4_3 - GAME_VALUE_X_4_3;
    const float room = pl_file_room_4_3(GAME_VALUE_X_4_3, edition_buttons(&oracles_ui_row_button_4_3, t, e, RIGHT_4_3, EU_BUTTON_GAP_4_3, out));
    FIT(&oracles_ui_row_title_4_3, t->edition_file[e], room);
    FIT(&oracles_ui_row_path_4_3, t->edition_folder[e], span);
    FIT(&oracles_ui_row_text_4_3, t->edition_status[e], span - DOT_4_3 - DOT_GAP_4_3);
    out->edition_files[e] = pl_line_at(&oracles_ui_row_title_4_3, t->edition_file[e], GAME_VALUE_X_4_3, 0.0f, 0.0f);
    OraclesUiLine label = pl_line_at(&oracles_ui_row_label_4_3, oracles_page_edition_labels[e], LEFT_4_3, 0.0f, 0.0f);
    OraclesUiLine *const lines[6] = { &out->edition_files[e], &out->edition_buttons[e][0], &out->edition_button_dots[e][0], &out->edition_buttons[e][1],
                                      &out->edition_button_dots[e][1], &label };
    float bottom;
    if (e == 0) {
        OraclesUiLine *const with_label[7] = { &label, lines[0], lines[1], lines[2], lines[3], lines[4] };
        bottom = pl_baseline_row(with_label, 2u + 2u * t->edition_buttons[e], y + ROW_PAD_Y_4_3);
        out->edition_labels[e].count = 1;
        snprintf(out->edition_labels[e].text[0], sizeof out->edition_labels[e].text[0], "%s", oracles_page_edition_labels[e]);
        out->edition_labels[e].lines[0] = label;
        out->edition_labels[e].w = label.w;
        out->edition_labels[e].h = label.h;
    } else {
        pl_wrap_lines(&oracles_ui_row_label_4_3, oracles_page_edition_labels[e], CARTRIDGE_LABEL_W_4_3, LEFT_4_3, y + ROW_PAD_Y_4_3, JOINED_LABEL_LINE_4_3,
                      &out->edition_labels[e]);
        bottom = pl_baseline_row(lines, 1u + 2u * t->edition_buttons[e], y + ROW_PAD_Y_4_3);
    }
    pl_button_boxes(t->edition_buttons[e], out->edition_buttons[e], out->edition_button_dots[e], out->edition_button_boxes[e]);
    out->edition_folders[e] = pl_line_at(&oracles_ui_row_path_4_3, t->edition_folder[e], GAME_VALUE_X_4_3, bottom + GRID_ROW_GAP_4_3, EU_FOLDER_LINE_4_3);
    const float status_y = out->edition_folders[e].y + out->edition_folders[e].h + GRID_ROW_GAP_4_3;
    out->edition_statuses[e] = pl_line_at(&oracles_ui_row_text_4_3, t->edition_status[e], GAME_VALUE_X_4_3 + DOT_4_3 + DOT_GAP_4_3, status_y, EDITION_STATUS_LINE_4_3);
    out->edition_dots[e] = pl_box(GAME_VALUE_X_4_3, status_y + (out->edition_statuses[e].h - DOT_4_3) * 0.5f, DOT_4_3, DOT_4_3);
    bottom = status_y + out->edition_statuses[e].h;
    const unsigned row = oracles_page_edition_row(e);
    out->rows[row] = pl_row_4_3(y, bottom);
    return y + out->rows[row].h + ROW_GAP_4_3;
}

static void game_4_3(OraclesUiGameTexts *t, OraclesUiGameLayout *out)
{
    pl_page_head_4_3(t->section ? t->section : "Cartridge", t->over, t->title, t->state, &out->head);
    if (t->credit)
        out->head.credit = pl_line_at(&oracles_ui_page_credit_4_3, t->credit, HEAD_X_4_3, out->head.state.y + out->head.state.h + HEAD_GAP_4_3,
                                      CREDIT_LINE_4_3);
    char fitted[ORACLES_HOME_TEXT_LENGTH * 4];
    const float span = RIGHT_4_3 - GAME_VALUE_X_4_3;   /* a line under the first, across the file's column and the button's */
    float y = ROWS_Y_4_3;
    /* Cartridge › Editions: its three rows alone. */
    if (t->editions) {
        for (int e = 0; e < ORACLES_PAGE_EDITIONS; e++) y = edition_4_3(t, e, y, out);
        return;
    }

    float room, bottom;
    if (t->joined) {
        y = joined_4_3(t, y, out);
    } else {
        /* ROM: the file, its folder, its status, a note or two; a fan game's base ROM: the file and its status. */
        room = pl_file_room_4_3(GAME_VALUE_X_4_3, pl_place_button_4_3("Choose ROM\xe2\x80\xa6", &out->rom_button, &out->rom_button_dot));
        FIT(&oracles_ui_row_title_4_3, t->rom_file, room);
        FIT(&oracles_ui_row_path_4_3, t->rom_folder, span);
        bottom = file_line_4_3(t->patched ? "Base ROM" : "ROM", &out->label_rom, t->rom_file, &out->rom_file, &out->rom_button,
                                     &out->rom_button_dot, y);
        if (!t->patched) {
            out->rom_folder = block_at(&oracles_ui_row_path_4_3, t->rom_folder, GAME_VALUE_X_4_3, bottom + GRID_ROW_GAP_4_3);
            bottom = out->rom_folder.y + out->rom_folder.h;
        }
        /* The status broken into lines beside its dot, centred on them, and a note or two under it. */
        const float status_y = bottom + GRID_ROW_GAP_4_3;
        oracles_ui_wrap(&oracles_ui_row_text_4_3, t->rom_status, span - DOT_4_3 - DOT_GAP_4_3, GAME_VALUE_X_4_3 + DOT_4_3 + DOT_GAP_4_3, status_y, &out->rom_status);
        out->rom_dot = pl_box(GAME_VALUE_X_4_3, status_y + (out->rom_status.h - DOT_4_3) * 0.5f, DOT_4_3, DOT_4_3);
        bottom = status_y + out->rom_status.h;
        oracles_ui_wrap(&oracles_ui_row_text_4_3, t->rom_note, span, GAME_VALUE_X_4_3, bottom + GRID_ROW_GAP_4_3, &out->rom_note);
        if (out->rom_note.count) bottom = out->rom_note.lines[0].y + out->rom_note.h;
        oracles_ui_wrap(&oracles_ui_row_text_4_3, t->rom_hotkeys_note, span, GAME_VALUE_X_4_3, bottom + GRID_ROW_GAP_4_3, &out->rom_hotkeys_note);
        if (out->rom_hotkeys_note.count) bottom = out->rom_hotkeys_note.lines[0].y + out->rom_hotkeys_note.h;
        out->rows[ORACLES_ROW_ROM] = pl_row_4_3(y, bottom);
        y += out->rows[ORACLES_ROW_ROM].h + ROW_GAP_4_3;
    }

    if (t->patched) {
        /* Patch: the file and its status; under the row, the patch's folder and the image the two files make. */
        room = pl_file_room_4_3(GAME_VALUE_X_4_3, pl_place_button_4_3("Choose patch\xe2\x80\xa6", &out->patch_button, &out->patch_button_dot));
        FIT(&oracles_ui_row_title_4_3, t->patch_file, room);
        bottom = file_line_4_3("Patch", &out->label_patch, t->patch_file, &out->patch_file, &out->patch_button, &out->patch_button_dot, y);
        /* The status broken into lines beside its dot, which is centred on them. */
        const float status_y = bottom + GRID_ROW_GAP_4_3;
        oracles_ui_wrap_pretty(&oracles_ui_row_text_4_3, t->patch_status, span - DOT_4_3 - DOT_GAP_4_3, GAME_VALUE_X_4_3 + DOT_4_3 + DOT_GAP_4_3,
                               status_y, &out->patch_status);
        out->patch_dot = pl_box(GAME_VALUE_X_4_3, status_y + (out->patch_status.h - DOT_4_3) * 0.5f, DOT_4_3, DOT_4_3);
        bottom = status_y + out->patch_status.h;
        out->rows[ORACLES_ROW_PATCH] = pl_row_4_3(y, bottom);
        y += out->rows[ORACLES_ROW_PATCH].h + ROW_GAP_4_3;
        FIT(&oracles_ui_row_path_4_3, t->patch_folder, span);
        FIT(&oracles_ui_row_text_4_3, t->image_status, span - DOT_4_3 - DOT_GAP_4_3);
        out->patch_folder = block_at(&oracles_ui_row_path_4_3, t->patch_folder, GAME_VALUE_X_4_3, y);
        y = status_4_3(t->image_status, out->patch_folder.y + out->patch_folder.h + IMAGE_GAP_4_3, &out->image_status, &out->image_dot);
        /* Its notes under it, each on its line, in line with its text. */
        for (unsigned i = 0; i < t->image_notes; i++) {
            FIT(&oracles_ui_row_text_4_3, t->image_note[i], span - DOT_4_3 - DOT_GAP_4_3);
            out->image_notes[i] = pl_line_at(&oracles_ui_row_text_4_3, t->image_note[i], GAME_VALUE_X_4_3 + DOT_4_3 + DOT_GAP_4_3, y + IMAGE_GAP_4_3, 0.0f);
            y = out->image_notes[i].y + out->image_notes[i].h;
        }
        y += ROW_GAP_4_3;
    }

    /* Save: the label, the file and its actions on one baseline; under them the line about it and the note, or the
     * status. */
    out->rows[ORACLES_ROW_SAVE] = pl_save_row_4_3(t, y, out);
    y += out->rows[ORACLES_ROW_SAVE].h + ROW_GAP_4_3;

    if (t->european) {
        /* Editions: its label, then its summary and its arrow from the values' column, both centred on the row. */
        out->editions_summary = pl_line_at(&oracles_ui_row_button_4_3, t->editions_summary, 0.0f, 0.0f, 0.0f);
        const float content_h = pl_max_f(out->editions_summary.h, EDITIONS_ARROW_H_4_3);
        out->label_editions = pl_line_at(&oracles_ui_row_label_4_3, "Editions", LEFT_4_3, 0.0f, 0.0f);
        const float track = pl_max_f(ROW_MIN_H_4_3 - 2.0f * EDITIONS_PAD_Y_4_3, pl_max_f(out->label_editions.h, content_h));
        out->rows[ORACLES_ROW_EDITIONS] = pl_box(ROWS_X_4_3, y, ROWS_W_4_3, track + 2.0f * EDITIONS_PAD_Y_4_3);
        const float middle = y + EDITIONS_PAD_Y_4_3 + track * 0.5f;
        pl_move_line(&out->label_editions, LEFT_4_3, middle - out->label_editions.h * 0.5f);
        pl_move_line(&out->editions_summary, GAME_VALUE_X_4_3, middle - out->editions_summary.h * 0.5f);
        out->editions_arrow = pl_box(GAME_VALUE_X_4_3 + out->editions_summary.w + EDITIONS_ARROW_GAP_4_3, middle - EDITIONS_ARROW_H_4_3 * 0.5f,
                                     EDITIONS_ARROW_W_4_3, EDITIONS_ARROW_H_4_3);
        out->editions_link = pl_box(GAME_VALUE_X_4_3, middle - content_h * 0.5f, out->editions_summary.w + EDITIONS_ARROW_GAP_4_3 + EDITIONS_ARROW_W_4_3,
                                    content_h);
        y += out->rows[ORACLES_ROW_EDITIONS].h + ROW_GAP_4_3;
    }
    if (t->languages) {
        /* Language: its label at the left of four columns of languages, then one line across the row: the note while
         * there is one, else the explanation. */
        out->label_language = pl_line_at(&oracles_ui_row_label_4_3, "Language", LEFT_4_3, y + LANGUAGE_PAD_Y_4_3 + LANGUAGE_LABEL_PAD_4_3,
                                         LANGUAGE_LINE_4_3);
        const float top = y + LANGUAGE_PAD_Y_4_3, columns = 4.0f;
        const float width = (RIGHT_4_3 - GAME_VALUE_X_4_3 - (columns - 1.0f) * LANGUAGE_GAP) / columns;
        const float frames = languages_at(&oracles_ui_language_4_3, t, GAME_VALUE_X_4_3, top, width, 4, OPTION_BORDER_4_3, LANGUAGE_MIN_H_4_3, 1, out);
        const float line_y = top + frames + LANGUAGE_HELP_GAP_4_3;
        OraclesUiLine *const help = t->language_note[0] ? &out->language_note : &out->language_explanation;
        if (t->language_note[0]) {
            t->language_explanation = "";
            out->language_note = pl_line_at(&oracles_ui_row_note_4_3, t->language_note, LEFT_4_3, line_y, LANGUAGE_LINE_4_3);
        } else out->language_explanation = pl_line_at(&oracles_ui_row_text_4_3, t->language_explanation, LEFT_4_3, line_y, LANGUAGE_LINE_4_3);
        out->rows[ORACLES_ROW_LANGUAGE] = pl_box(ROWS_X_4_3, y, ROWS_W_4_3, help->y + LANGUAGE_LINE_4_3 + LANGUAGE_PAD_Y_4_3 - y);
        y += out->rows[ORACLES_ROW_LANGUAGE].h + ROW_GAP_4_3;
    }

    out->rows[ORACLES_ROW_PLAY] = pl_play_4_3_row(t->play_note, y, &out->play_note, &out->play, &out->play_dot);
}


void oracles_ui_game_texts(const OraclesHomeNav *nav, OraclesUiGameTexts *t, char *state, OraclesUiGameTones *tones)
{
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    const OraclesHomeHero hero = oracles_home_hero(nav);
    memset(t, 0, sizeof *t);
    oracles_home_state(nav, hero, state);
    t->over = oracles_home_over(hero);
    t->title = oracles_home_title(hero);
    t->state = state;
    t->credit = oracles_home_credit(hero);
    t->patched = game->fan != NULL;
    t->joined = game->fan && game->fan->joined;
    /* A ROM row, or Oracles Combined's two. */
    tones->rom = ORACLES_TONE_NONE;
    if (!t->joined) {
        snprintf(t->rom_file, sizeof t->rom_file, "%s", oracles_page_rom_file(game));
        oracles_page_folder(nav->android, game->rom_file[0] ? game->rom_folder : "", t->rom_folder, sizeof t->rom_folder);
        oracles_page_rom_status(game, t->rom_status, sizeof t->rom_status, &tones->rom);
    }
    for (int h = 0; t->joined && h < 2; h++) {
        t->joined_labels[h] = oracles_page_joined_label(h);
        t->joined_from[h] = nav->layout == ORACLES_UI_LAYOUT_4_3 ? "" : oracles_page_joined_from(game, h);
        snprintf(t->joined_file[h], sizeof t->joined_file[h], "%s", oracles_page_joined_file(game, h));
        oracles_page_joined_status(game, h, nav->layout == ORACLES_UI_LAYOUT_4_3, t->joined_status[h], sizeof t->joined_status[h], &tones->joined[h]);
    }
    t->image_notes = oracles_page_image_notes(game, nav->layout == ORACLES_UI_LAYOUT_4_3, t->image_note);
    if (game->fan) {
        snprintf(t->patch_file, sizeof t->patch_file, "%s", oracles_page_patch_file(game));
        oracles_page_folder(nav->android, game->patch_file[0] ? game->patch_folder : "", t->patch_folder, sizeof t->patch_folder);
        oracles_page_patch_status(game, t->patch_status, sizeof t->patch_status, &tones->patch);
        oracles_page_image_status(game, t->image_status, sizeof t->image_status, &tones->image);
    }
    snprintf(t->save_file, sizeof t->save_file, "%s", oracles_page_save_file(game));
    oracles_page_save_line(game, t->save_line, sizeof t->save_line);
    for (int i = 0; i < ORACLES_PAGE_SAVE_ACTIONS; i++) t->save_actions[i] = oracles_page_save_action(nav, i);
    t->save_note = oracles_page_save_note(nav, nav->layout == ORACLES_UI_LAYOUT_4_3);
    t->save_status = oracles_page_save_status(nav, &tones->save, &t->save_status2);
    t->rom_note = oracles_page_rom_note(nav);
    t->rom_hotkeys_note = oracles_page_rom_hotkeys_note(nav);
    t->play_note = oracles_page_play_note(nav);
    t->european = oracles_page_eu_shown(game);
    t->editions = t->european && nav->editions;
    t->section = t->editions ? oracles_page_section(nav) : "Cartridge";
    if (t->european) {
        t->editions_explanation = oracles_page_editions_explanation;
        oracles_page_editions_summary(game, t->editions_summary, sizeof t->editions_summary);
        for (int e = 0; e < ORACLES_PAGE_EDITIONS; e++) {
            snprintf(t->edition_file[e], sizeof t->edition_file[e], "%s", oracles_page_edition_file(game, e));
            oracles_page_folder(nav->android, oracles_page_edition_state(game, e) != ORACLES_ROM_NONE ? oracles_page_edition_folder(game, e) : "",
                                t->edition_folder[e], sizeof t->edition_folder[e]);
            oracles_page_edition_status(nav, e, t->edition_status[e], sizeof t->edition_status[e], &tones->editions[e]);
            t->edition_buttons[e] = oracles_page_edition_buttons(game, e, t->edition_button_labels[e]);
        }
    }
    t->languages = !t->editions && oracles_page_language_shown(nav, game);
    if (t->languages) {
        t->language_count = oracles_page_languages_shown(game, t->language_order);
        t->language_columns = oracles_page_language_count(game) > ORACLES_PAGE_EUROPEAN_LANGUAGES ? 4 : 3;
        t->language_explanation = oracles_page_language_explanation(game);
        oracles_page_language_note(nav, game, t->language_note, sizeof t->language_note);
    }
}

/* ---- the layouts --------------------------------------------------------------------------------------------- */

void oracles_ui_layout_page_head(OraclesUiLayout layout, const char *section, const char *over, const char *title, const char *state,
                                 OraclesUiPageHead *out)
{
    if (layout == ORACLES_UI_LAYOUT_4_3) pl_page_head_4_3(section, over, title, state, out);
    else pl_page_head_16_9(section, over, title, state, out);
}

void oracles_ui_layout_game(OraclesUiLayout layout, OraclesUiGameTexts *t, OraclesUiGameLayout *out)
{
    memset(out, 0, sizeof *out);
    if (layout == ORACLES_UI_LAYOUT_4_3) game_4_3(t, out);
    else game_16_9(t, out);
}
