#include "ui_page_layout.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The pages' text styles. */
const OraclesUiTextStyle oracles_ui_page_section = { ORACLES_UI_FONT_SERIF, 26.0f, 0.2f, 1 };
const OraclesUiTextStyle oracles_ui_page_over = { ORACLES_UI_FONT_SERIF, 44.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_page_title = { ORACLES_UI_FONT_SERIF, 104.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_page_state = { ORACLES_UI_FONT_SANS, 24.0f, 0.16f, 1 };
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

static const OraclesUiPageStyles styles_16_9 = {
    &oracles_ui_page_section, &oracles_ui_page_over, &oracles_ui_page_title, &oracles_ui_page_state, &oracles_ui_row_note,
    &oracles_ui_row_label, &oracles_ui_row_title, &oracles_ui_row_path, &oracles_ui_row_status, &oracles_ui_row_text, &oracles_ui_row_note,
    &oracles_ui_row_button, &oracles_ui_play,
    &oracles_ui_option_name, &oracles_ui_option_size, &oracles_ui_choice, &oracles_ui_diagram_label, &oracles_ui_row_text, &oracles_ui_row_note,
    &oracles_ui_mod_games, &oracles_ui_row_status, &oracles_ui_row_note, &oracles_ui_option_size,
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
};

const OraclesUiPageStyles *oracles_ui_page_styles(OraclesUiLayout layout) { return layout == ORACLES_UI_LAYOUT_4_3 ? &styles_4_3 : &styles_16_9; }

const char oracles_ui_transitions_note[] = "A savestate taken with this on is refused with it off.";
const char oracles_ui_core_note[] = "Save states keep their core.";
const char oracles_ui_window_fit_note[] = "Window sizes follow the view: a farther view makes a larger window.";
const char oracles_ui_display_later[] = "Only color correction applies at once; the rest at the next Play.";
const char oracles_ui_later[] = "Applies at the next Play.";
const char *const oracles_ui_transition_choices[2] = { "Off", "On" };

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
#define LABEL_W 150.0f
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
#define PLAY_GAP 24.0f
#define DISPLAY_ROW_GAP 14.0f
#define DISPLAY_LABEL_W 200.0f
#define DISPLAY_ROW_PAD_Y 14.0f
#define DISPLAY_WINDOW_GAP 12.0f     /* between the windows and their note */
#define DISPLAY_FIT_LINE 32.0f       /* the line on the window's size, kept when empty */
#define DIAGRAM_MARGIN 56.0f
#define DISPLAY_PAGE_NOTE_GAP 20.0f   /* the note under Display's title, opened from a game */
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
#define MODS_COUNT_PAD_TOP 4.0f
#define MODS_TOGGLE_COLUMN 72.0f
#define MODS_TOGGLE_W 64.0f
#define MODS_TOGGLE_H 34.0f
#define MODS_KNOB 26.0f
#define MODS_KNOB_INSET 4.0f         /* its border and 3 pixels */
#define MODS_KNOB_ON 30.0f
#define MODS_NAME_GAP 16.0f          /* between a mod's name and its games */

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

/* An empty block has no height. */
static OraclesUiLine block_at(const OraclesUiTextStyle *style, const char *text, float x, float y)
{
    OraclesUiLine line = line_at(style, text, x, y, 0.0f);
    if (!text || !text[0]) line.h = 0.0f;
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

static void page_head_16_9(const char *section, const char *over, const char *title, const char *state, OraclesUiPageHead *out)
{
    memset(out, 0, sizeof *out);
    out->section = line_at(&oracles_ui_page_section, section, HEAD_X, HEAD_Y, 0.0f);
    out->over = line_at(&oracles_ui_page_over, over, HEAD_X, out->section.y + out->section.h + HEAD_SECTION_GAP, 0.0f);
    /* The title in the column's width, each line as high as the title's font (line-height 1). */
    const float title_y = out->over.y + out->over.h;
    oracles_ui_wrap(&oracles_ui_page_title, title, HEAD_W, HEAD_X, title_y, &out->title);
    for (unsigned i = 0; i < out->title.count; i++)
        out->title.lines[i] = line_at(&oracles_ui_page_title, out->title.text[i], HEAD_X, title_y + (float)i * HEAD_TITLE_LINE, HEAD_TITLE_LINE);
    out->title.h = (float)out->title.count * HEAD_TITLE_LINE;
    out->state = line_at(&oracles_ui_page_state, state, HEAD_X, title_y + out->title.h + HEAD_STATE_MARGIN, 0.0f);
}

/* A button at the right of a row, centred on the row's content: its label and its dot. */
static void button(const char *label, float right, float top, float height, OraclesUiLine *text, OraclesUiLine *dot)
{
    *dot = line_at(&oracles_ui_row_button, oracles_ui_item_dot, 0.0f, 0.0f, 0.0f);
    *text = line_at(&oracles_ui_row_button, label, 0.0f, 0.0f, 0.0f);
    move_line(dot, right - dot->w, top + (height - dot->h) * 0.5f);
    move_line(text, dot->x - BUTTON_GAP - text->w, dot->y);
}

/* The width of `length` characters of a monospaced style: a browser's `ch`. */
static float characters(const OraclesUiTextStyle *style, int length) { return (float)length * oracles_ui_text_width(style, "0"); }

/* Options framed with a name and a size (profiles, windows), in a line from (x, y), each size line `size_lengths`
 * characters wide at least (NULL: its own width).  Returns their height. */
static float framed(const char *const *names, const char *const *sizes, const int *size_lengths, unsigned count, float x, float y,
                    OraclesUiOptionLayout *out)
{
    float height = 0.0f;
    for (unsigned i = 0; i < count; i++) {
        OraclesUiOptionLayout *o = &out[i];
        o->name = line_at(&oracles_ui_option_name, names[i], x + BORDER + OPTION_PAD_X, y + BORDER + OPTION_PAD_Y, 0.0f);
        o->size = line_at(&oracles_ui_option_size, sizes[i], o->name.x, o->name.y + o->name.h + OPTION_LINE_GAP, 0.0f);
        const float size_w = size_lengths && characters(&oracles_ui_option_size, size_lengths[i]) > o->size.w
                             ? characters(&oracles_ui_option_size, size_lengths[i]) : o->size.w;
        const float inner = o->name.w > size_w ? o->name.w : size_w;
        o->box = box(x, y, inner + 2.0f * (OPTION_PAD_X + BORDER), o->size.y + o->size.h + OPTION_PAD_Y + BORDER - y);
        height = o->box.h;
        x += o->box.w + OPTION_GAP;
    }
    return height;
}

/* A row of choices framed, right-aligned at `right` in a line of `height` from `top`; with `sizes` (NULL: none), each
 * choice's size under its name, the two centred in the frame (View). */
static void choices(const char *const *names, const char *const *sizes, unsigned count, float right, float top, OraclesUiOptionLayout *out)
{
    float x = right;
    for (unsigned i = count; i-- > 0;) {
        OraclesUiOptionLayout *o = &out[i];
        o->name = line_at(&oracles_ui_choice, names[i], 0.0f, 0.0f, 0.0f);
        memset(&o->size, 0, sizeof o->size);
        if (sizes) o->size = line_at(&oracles_ui_option_size, sizes[i], 0.0f, 0.0f, 0.0f);
        const float inner = sizes && o->size.w > o->name.w ? o->size.w : o->name.w;
        const float inner_h = o->name.h + (sizes ? OPTION_LINE_GAP + o->size.h : 0.0f);
        o->box.w = inner + 2.0f * (CHOICE_PAD_X + BORDER);
        o->box.h = inner_h + 2.0f * (CHOICE_PAD_Y + BORDER);
        o->box.x = x - o->box.w;
        o->box.y = top;
        const float inner_x = o->box.x + BORDER + CHOICE_PAD_X;
        move_line(&o->name, inner_x + (inner - o->name.w) * 0.5f, top + BORDER + CHOICE_PAD_Y);
        if (sizes) move_line(&o->size, inner_x + (inner - o->size.w) * 0.5f, o->name.y + o->name.h + OPTION_LINE_GAP);
        x = o->box.x - CHOICE_GAP;
    }
}

static void shift_option(OraclesUiOptionLayout *o, float dx, float dy)
{
    o->box.x += dx;
    o->box.y += dy;
    move_line(&o->name, o->name.x + dx, o->name.y + dy);
    if (o->size.h > 0.0f) move_line(&o->size, o->size.x + dx, o->size.y + dy);
}

static void game_16_9(OraclesUiGameTexts *t, OraclesUiGameLayout *out)
{
    page_head_16_9("Cartridge", t->over, t->title, t->state, &out->head);
    const float left = PANEL_X + PANEL_BORDER + PANEL_PAD_X, width = PANEL_W - 2.0f * (PANEL_BORDER + PANEL_PAD_X);
    const float value_x = left + LABEL_W + GRID_GAP, value_w = width - LABEL_W - GRID_GAP;
    const float inner_x = value_x + ROW_PAD_X, inner_w = value_w - 2.0f * ROW_PAD_X, right = inner_x + inner_w;
    float y = PANEL_Y + PANEL_BORDER + PANEL_PAD_Y;

    /* ROM: the file, its folder, its status; Choose ROM at the right.  A fan game's base ROM: the file and its status. */
    out->label_rom = line_at(&oracles_ui_row_label, t->patched ? "Base ROM" : "ROM", left, y + LABEL_PAD_TOP, 0.0f);
    float inner_y = y + ROW_PAD_Y;
    button("Choose ROM\xe2\x80\xa6", right, 0.0f, 0.0f, &out->rom_button, &out->rom_button_dot);
    const float room = inner_w - ROW_GAP - (out->rom_button_dot.x + out->rom_button_dot.w - out->rom_button.x);
    char fitted[ORACLES_HOME_TEXT_LENGTH * 4];
    oracles_ui_fit(&oracles_ui_row_title, t->rom_file, room, fitted, sizeof t->rom_file); memcpy(t->rom_file, fitted, sizeof t->rom_file);
    oracles_ui_fit(&oracles_ui_row_path, t->rom_folder, room, fitted, sizeof t->rom_folder); memcpy(t->rom_folder, fitted, sizeof t->rom_folder);
    oracles_ui_fit(&oracles_ui_row_status, t->rom_status, room - DOT - DOT_GAP, fitted, sizeof t->rom_status); memcpy(t->rom_status, fitted, sizeof t->rom_status);
    out->rom_file = block_at(&oracles_ui_row_title, t->rom_file, inner_x, inner_y);
    float status_y = out->rom_file.y + out->rom_file.h + FILE_GAP;
    if (!t->patched) {
        out->rom_folder = block_at(&oracles_ui_row_path, t->rom_folder, inner_x, status_y);
        status_y = out->rom_folder.y + out->rom_folder.h + FILE_GAP + STATUS_MARGIN;
    }
    out->rom_status = line_at(&oracles_ui_row_status, t->rom_status, inner_x + DOT + DOT_GAP, status_y, 0.0f);
    out->rom_dot = box(inner_x, status_y + (out->rom_status.h - DOT) * 0.5f, DOT, DOT);
    /* A ROM that plays in Faithful with Enhanced chosen, or without the item hotkeys Controls has on: a line each under
     * the status. */
    float rom_bottom = out->rom_status.y + out->rom_status.h;
    out->rom_note = block_at(&oracles_ui_row_text, t->rom_note, inner_x, rom_bottom + FILE_GAP);
    if (out->rom_note.h > 0.0f) rom_bottom = out->rom_note.y + out->rom_note.h;
    out->rom_hotkeys_note = block_at(&oracles_ui_row_text, t->rom_hotkeys_note, inner_x, rom_bottom + FILE_GAP);
    if (out->rom_hotkeys_note.h > 0.0f) rom_bottom = out->rom_hotkeys_note.y + out->rom_hotkeys_note.h;
    float content = rom_bottom - inner_y;
    if (content < out->rom_button.h) content = out->rom_button.h;
    button("Choose ROM\xe2\x80\xa6", right, inner_y, content, &out->rom_button, &out->rom_button_dot);
    out->rows[ORACLES_ROW_ROM] = box(value_x, y, value_w, content + 2.0f * ROW_PAD_Y);
    y += out->rows[ORACLES_ROW_ROM].h + GAME_ROW_GAP;

    if (t->patched) {
        /* Patch: the file and its status, Choose patch at the right; under the row, the patch's folder (its line kept
         * without a patch) and the image the two files make. */
        out->label_patch = line_at(&oracles_ui_row_label, "Patch", left, y + LABEL_PAD_TOP, 0.0f);
        inner_y = y + ROW_PAD_Y;
        button("Choose patch\xe2\x80\xa6", right, 0.0f, 0.0f, &out->patch_button, &out->patch_button_dot);
        const float patch_room = inner_w - ROW_GAP - (out->patch_button_dot.x + out->patch_button_dot.w - out->patch_button.x);
        oracles_ui_fit(&oracles_ui_row_title, t->patch_file, patch_room, fitted, sizeof t->patch_file); memcpy(t->patch_file, fitted, sizeof t->patch_file);
        oracles_ui_fit(&oracles_ui_row_status, t->patch_status, patch_room - DOT - DOT_GAP, fitted, sizeof t->patch_status); memcpy(t->patch_status, fitted, sizeof t->patch_status);
        out->patch_file = block_at(&oracles_ui_row_title, t->patch_file, inner_x, inner_y);
        const float patch_status_y = out->patch_file.y + out->patch_file.h + FILE_GAP;
        out->patch_status = line_at(&oracles_ui_row_status, t->patch_status, inner_x + DOT + DOT_GAP, patch_status_y, 0.0f);
        out->patch_dot = box(inner_x, patch_status_y + (out->patch_status.h - DOT) * 0.5f, DOT, DOT);
        content = out->patch_status.y + out->patch_status.h - inner_y;
        if (content < out->patch_button.h) content = out->patch_button.h;
        button("Choose patch\xe2\x80\xa6", right, inner_y, content, &out->patch_button, &out->patch_button_dot);
        out->rows[ORACLES_ROW_PATCH] = box(value_x, y, value_w, content + 2.0f * ROW_PAD_Y);
        const float line_w = value_w - 2.0f * ROW_PAD_X;
        oracles_ui_fit(&oracles_ui_row_path, t->patch_folder, line_w, fitted, sizeof t->patch_folder); memcpy(t->patch_folder, fitted, sizeof t->patch_folder);
        oracles_ui_fit(&oracles_ui_row_status, t->image_status, line_w - DOT - DOT_GAP, fitted, sizeof t->image_status); memcpy(t->image_status, fitted, sizeof t->image_status);
        out->patch_folder = line_at(&oracles_ui_row_path, t->patch_folder, inner_x, y + out->rows[ORACLES_ROW_PATCH].h + PATCH_LINE_GAP, 0.0f);
        const float image_y = out->patch_folder.y + out->patch_folder.h + PATCH_LINE_GAP;
        out->image_status = line_at(&oracles_ui_row_status, t->image_status, inner_x + DOT + DOT_GAP, image_y, 0.0f);
        out->image_dot = box(inner_x, image_y + (out->image_status.h - DOT) * 0.5f, DOT, DOT);
        y = out->image_status.y + out->image_status.h + IMAGE_PAD_BOTTOM + GAME_ROW_GAP;
    }

    /* Save: the file and a line about it; Open folder at the right. */
    out->label_save = line_at(&oracles_ui_row_label, "Save", left, y + LABEL_PAD_TOP, 0.0f);
    inner_y = y + ROW_PAD_Y;
    button("Open folder", right, 0.0f, 0.0f, &out->save_button, &out->save_button_dot);
    const float save_room = inner_w - ROW_GAP - (out->save_button_dot.x + out->save_button_dot.w - out->save_button.x);
    oracles_ui_fit(&oracles_ui_row_title, t->save_file, save_room, fitted, sizeof t->save_file); memcpy(t->save_file, fitted, sizeof t->save_file);
    oracles_ui_fit(&oracles_ui_row_text, t->save_line, save_room, fitted, sizeof t->save_line); memcpy(t->save_line, fitted, sizeof t->save_line);
    out->save_file = block_at(&oracles_ui_row_title, t->save_file, inner_x, inner_y);
    out->save_line = block_at(&oracles_ui_row_text, t->save_line, inner_x, out->save_file.y + out->save_file.h + FILE_GAP);
    content = out->save_line.y + out->save_line.h - inner_y;
    if (content < out->save_button.h) content = out->save_button.h;
    button("Open folder", right, inner_y, content, &out->save_button, &out->save_button_dot);
    out->rows[ORACLES_ROW_SAVE] = box(value_x, y, value_w, content + 2.0f * ROW_PAD_Y);
    y += out->rows[ORACLES_ROW_SAVE].h + GAME_ROW_GAP;

    /* Play, right-aligned across the whole panel, with a note when it cannot. */
    const float play_right = left + width - ROW_PAD_X;
    out->play_dot = line_at(&oracles_ui_play, oracles_ui_item_dot, 0.0f, y + PLAY_PAD_Y, 0.0f);
    move_line(&out->play_dot, play_right - out->play_dot.w, out->play_dot.y);
    out->play = line_at(&oracles_ui_play, "Play", 0.0f, y + PLAY_PAD_Y, 0.0f);
    move_line(&out->play, out->play_dot.x - PLAY_GAP - out->play.w, out->play.y);
    out->play_note = line_at(&oracles_ui_row_text, t->play_note, 0.0f, 0.0f, 0.0f);
    move_line(&out->play_note, out->play.x - PLAY_GAP - out->play_note.w, out->play.y + (out->play.h - out->play_note.h) * 0.5f);
    out->rows[ORACLES_ROW_PLAY] = box(left, y, width, out->play.h + 2.0f * PLAY_PAD_Y);
    y += out->rows[ORACLES_ROW_PLAY].h;

    out->panel = box(PANEL_X, PANEL_Y, PANEL_W, y + PANEL_PAD_Y + PANEL_BORDER - PANEL_Y);
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
static size_t piece_length(const char *p)
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
        const size_t piece = piece_length(p);
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
        out->lines[i] = line_at(style, out->text[i], x, y + (float)i * height, 0.0f);
        if (out->lines[i].w > out->w) out->w = out->lines[i].w;
    }
    out->h = (float)out->count * height;
}

static void move_wrapped(OraclesUiWrapped *w, float y)
{
    const float dy = y - (w->count ? w->lines[0].y : y);
    for (unsigned i = 0; i < w->count; i++) move_line(&w->lines[i], w->lines[i].x, w->lines[i].y + dy);
}

/* A row of Display with choices: its explanation wrapped beside them, with its italic note under it if any (NULL: none),
 * both centred on the row. Returns the row's height. */
static float display_row(float x, float y, float w, const char *label, float label_x, const char *explanation, const char *const *notes,
                         int note_count, const char *const *names, const char *const *sizes, unsigned count, OraclesUiWrapped *label_out,
                         OraclesUiWrapped *explanation_out, OraclesUiLine *const *notes_out, OraclesUiOptionLayout *options, OraclesUiBox *row)
{
    oracles_ui_wrap(&oracles_ui_row_label, label, DISPLAY_LABEL_W, label_x, y + LABEL_PAD_TOP, label_out);
    const float inner_x = x + ROW_PAD_X, inner_y = y + DISPLAY_ROW_PAD_Y, right = x + w - ROW_PAD_X;
    choices(names, sizes, count, right, inner_y, options);
    const float options_w = right - options[0].box.x;
    oracles_ui_wrap(&oracles_ui_row_text, explanation, right - inner_x - ROW_GAP - options_w, inner_x, inner_y, explanation_out);
    float text_h = explanation_out->h;
    for (int i = 0; i < note_count; i++) {
        *notes_out[i] = line_at(&oracles_ui_row_note, notes[i], inner_x, 0.0f, 0.0f);
        text_h += NOTE_GAP + notes_out[i]->h;
    }
    const float inner_h = text_h > options[0].box.h ? text_h : options[0].box.h;
    move_wrapped(explanation_out, inner_y + (inner_h - text_h) * 0.5f);
    float note_y = explanation_out->lines[0].y + explanation_out->h;
    for (int i = 0; i < note_count; i++) {
        move_line(notes_out[i], inner_x, note_y + NOTE_GAP);
        note_y = notes_out[i]->y + notes_out[i]->h;
    }
    const float dy = (inner_h - options[0].box.h) * 0.5f;
    for (unsigned i = 0; i < count; i++) {
        options[i].box.y += dy;
        move_line(&options[i].name, options[i].name.x, options[i].name.y + dy);
        if (sizes) move_line(&options[i].size, options[i].size.x, options[i].size.y + dy);
    }
    *row = box(x, y, w, inner_h + 2.0f * DISPLAY_ROW_PAD_Y);
    const float label_h = LABEL_PAD_TOP + label_out->h;
    return label_h > row->h ? label_h : row->h;
}

static void display_16_9(const OraclesUiDisplayTexts *t, OraclesUiDisplayLayout *out)
{
    page_head_16_9(t->section ? t->section : "Display", t->over, t->title, NULL, &out->head);
    const float diagram_w = t->diagram_box_w > 0.0f ? t->diagram_box_w : ORACLES_UI_DIAGRAM_W;
    float head_bottom = out->head.title.lines[0].y + out->head.title.h;
    /* Opened from a game, under the title: what only the next session takes, said once for the page. */
    if (t->later) {
        out->page_note = line_at(&oracles_ui_row_note, oracles_ui_display_later, HEAD_X, head_bottom + DISPLAY_PAGE_NOTE_GAP, 0.0f);
        head_bottom = out->page_note.y + out->page_note.h;
    }
    out->diagram = box(HEAD_X, head_bottom + DIAGRAM_MARGIN, diagram_w, ORACLES_UI_DIAGRAM_H);
    out->diagram_window = box(out->diagram.x + (diagram_w - t->diagram_w) * 0.5f, out->diagram.y + (ORACLES_UI_DIAGRAM_H - t->diagram_h) * 0.5f,
                              t->diagram_w, t->diagram_h);
    out->diagram_label = line_at(&oracles_ui_diagram_label, t->diagram_label, HEAD_X, out->diagram.y + out->diagram.h + DIAGRAM_LABEL_GAP, 0.0f);
    /* Advanced under the diagram, its highlight reaching past its label at the left as at the right, the arrow after the
     * label, both centred on it; not on the Advanced rows. */
    if (!t->advanced) {
        const float y = out->diagram_label.y + out->diagram_label.h + ADVANCED_MARGIN;
        out->advanced_label = line_at(&oracles_ui_row_label, oracles_display_labels[ORACLES_DISPLAY_ADVANCED], HEAD_X, y + ADVANCED_PAD_Y, 0.0f);
        const float inner_h = out->advanced_label.h > ADVANCED_ARROW_H ? out->advanced_label.h : ADVANCED_ARROW_H;
        move_line(&out->advanced_label, HEAD_X, y + ADVANCED_PAD_Y + (inner_h - out->advanced_label.h) * 0.5f);
        out->advanced_arrow = box(HEAD_X + out->advanced_label.w + ADVANCED_GAP, y + ADVANCED_PAD_Y + (inner_h - ADVANCED_ARROW_H) * 0.5f,
                                  ADVANCED_ARROW_W, ADVANCED_ARROW_H);
        out->rows[ORACLES_DISPLAY_ADVANCED] = box(HEAD_X - ADVANCED_PAD_X, y, out->advanced_arrow.x + ADVANCED_ARROW_W + ADVANCED_PAD_X - (HEAD_X - ADVANCED_PAD_X),
                                                  inner_h + 2.0f * ADVANCED_PAD_Y);
    }

    const float left = PANEL_X + PANEL_BORDER + PANEL_PAD_X, width = PANEL_W - 2.0f * (PANEL_BORDER + PANEL_PAD_X);
    const float value_x = left + DISPLAY_LABEL_W + GRID_GAP, value_w = width - DISPLAY_LABEL_W - GRID_GAP;
    const float inner_x = value_x + ROW_PAD_X;
    float y = PANEL_Y + PANEL_BORDER + PANEL_PAD_Y;
    const char *const core_notes[1] = { oracles_ui_core_note };
    OraclesUiLine *const core_out[1] = { &out->core_note };

    if (t->advanced) {
        /* The Advanced rows: Core (with its note on savestates), Vsync, the workers (Auto naming its count). */
        const char *workers[3];
        for (int i = 0; i < 3; i++) workers[i] = t->workers_names[i];
        y += display_row(value_x, y, value_w, oracles_display_labels[ORACLES_DISPLAY_CORE], left, t->explanations[3], core_notes, 1,
                         oracles_display_core_choices, NULL, 2, &out->labels[ORACLES_DISPLAY_CORE], &out->explanations[3], core_out, out->core,
                         &out->rows[ORACLES_DISPLAY_CORE]) + DISPLAY_ROW_GAP;
        y += display_row(value_x, y, value_w, oracles_display_labels[ORACLES_DISPLAY_VSYNC], left, t->explanations[2], NULL, 0,
                         oracles_display_vsync_choices, NULL, 3, &out->labels[ORACLES_DISPLAY_VSYNC], &out->explanations[2], NULL, out->vsync,
                         &out->rows[ORACLES_DISPLAY_VSYNC]) + DISPLAY_ROW_GAP;
        y += display_row(value_x, y, value_w, oracles_display_labels[ORACLES_DISPLAY_WORKERS], left, t->explanations[4], NULL, 0, workers, NULL, 3,
                         &out->labels[ORACLES_DISPLAY_WORKERS], &out->explanations[4], NULL, out->workers, &out->rows[ORACLES_DISPLAY_WORKERS]);
        out->panel = box(PANEL_X, PANEL_Y, PANEL_W, y + PANEL_PAD_Y + PANEL_BORDER - PANEL_Y);
        return;
    }

    /* Profile: the two profiles framed, each with its surface, and the chosen one's note, two lines kept for it. */
    oracles_ui_wrap(&oracles_ui_row_label, oracles_display_labels[ORACLES_DISPLAY_PROFILE], DISPLAY_LABEL_W, left, y + LABEL_PAD_TOP,
                    &out->labels[ORACLES_DISPLAY_PROFILE]);
    float inner_y = y + DISPLAY_ROW_PAD_Y;
    const char *profile_sizes[ORACLES_PROFILES];
    for (int p = 0; p < ORACLES_PROFILES; p++) profile_sizes[p] = t->profile_sizes[p];
    float options_h = framed(oracles_profile_names, profile_sizes, NULL, ORACLES_PROFILES, inner_x, inner_y, out->profiles);
    out->profile_note = line_at(&oracles_ui_row_text, t->profile_note, inner_x, inner_y + options_h + PROFILE_NOTE_GAP, PROFILE_NOTE_LINE);
    float bottom = out->profile_note.y + PROFILE_NOTE_H;
    out->rows[ORACLES_DISPLAY_PROFILE] = box(value_x, y, value_w, bottom + DISPLAY_ROW_PAD_Y - y);
    y += out->rows[ORACLES_DISPLAY_PROFILE].h + DISPLAY_ROW_GAP;

    /* Quality: its choices from the left, each with what it sets, Custom's place kept; under them the quality in effect
     * and its note, a line each. */
    oracles_ui_wrap(&oracles_ui_row_label, oracles_display_labels[ORACLES_DISPLAY_QUALITY], DISPLAY_LABEL_W, left, y + LABEL_PAD_TOP,
                    &out->labels[ORACLES_DISPLAY_QUALITY]);
    inner_y = y + DISPLAY_ROW_PAD_Y;
    const char *quality_sets[ORACLES_DISPLAY_QUALITIES + 1];
    for (int q = 0; q <= ORACLES_DISPLAY_QUALITIES; q++) quality_sets[q] = t->quality_sets[q];
    choices(oracles_display_quality_names, quality_sets, ORACLES_DISPLAY_QUALITIES + 1, value_x + value_w - ROW_PAD_X, inner_y, out->qualities);
    const float from_left = inner_x - out->qualities[0].box.x;
    for (int q = 0; q <= ORACLES_DISPLAY_QUALITIES; q++) shift_option(&out->qualities[q], from_left, 0.0f);
    out->quality_text = line_at(&oracles_ui_row_text, t->quality_text, inner_x, inner_y + out->qualities[0].box.h + PROFILE_NOTE_GAP, QUALITY_TEXT_LINE);
    out->quality_note = line_at(&oracles_ui_row_note, t->quality_note, inner_x, out->quality_text.y + out->quality_text.h + NOTE_GAP, QUALITY_NOTE_LINE);
    bottom = out->quality_note.y + out->quality_note.h;
    out->rows[ORACLES_DISPLAY_QUALITY] = box(value_x, y, value_w, bottom + DISPLAY_ROW_PAD_Y - y);
    y += out->rows[ORACLES_DISPLAY_QUALITY].h + DISPLAY_ROW_GAP;

    /* Window: the four choices framed, each with its size, each as wide whatever the profile and the view; the note,
     * and the line on the window's size, italic: that it is reduced, or else that the sizes follow the view. */
    oracles_ui_wrap(&oracles_ui_row_label, oracles_display_labels[ORACLES_DISPLAY_WINDOW], DISPLAY_LABEL_W, left, y + LABEL_PAD_TOP,
                    &out->labels[ORACLES_DISPLAY_WINDOW]);
    inner_y = y + DISPLAY_ROW_PAD_Y;
    const char *names[4], *sizes[4];
    for (int i = 0; i < 4; i++) { names[i] = t->window_names[i]; sizes[i] = t->window_sizes[i]; }
    options_h = framed(names, sizes, t->window_size_length, 4, inner_x, inner_y, out->windows);
    out->window_note = line_at(&oracles_ui_row_text, t->window_note, inner_x, inner_y + options_h + DISPLAY_WINDOW_GAP, 0.0f);
    out->window_reduced = line_at(&oracles_ui_row_note, t->window_fit, inner_x, out->window_note.y + out->window_note.h + DISPLAY_WINDOW_GAP,
                                  DISPLAY_FIT_LINE);
    bottom = out->window_reduced.y + out->window_reduced.h;
    out->rows[ORACLES_DISPLAY_WINDOW] = box(value_x, y, value_w, bottom + DISPLAY_ROW_PAD_Y - y);
    y += out->rows[ORACLES_DISPLAY_WINDOW].h + DISPLAY_ROW_GAP;

    /* View (each level with its size), color correction, continuous transitions (with the note on savestates): an
     * explanation and choices. */
    const char *const transitions_notes[1] = { oracles_ui_transitions_note };
    OraclesUiLine *const transitions_out[1] = { &out->transitions_note };
    const char *view_sizes[3];
    for (int v = 0; v < 3; v++) view_sizes[v] = t->view_sizes[v];
    y += display_row(value_x, y, value_w, oracles_display_labels[ORACLES_DISPLAY_VIEW], left, t->view_explanation, NULL, 0,
                     oracles_display_view_names, view_sizes, 3, &out->labels[ORACLES_DISPLAY_VIEW], &out->view_explanation, NULL, out->views,
                     &out->rows[ORACLES_DISPLAY_VIEW]) + DISPLAY_ROW_GAP;
    y += display_row(value_x, y, value_w, oracles_display_labels[ORACLES_DISPLAY_COLOUR], left, t->explanations[0], NULL, 0, oracles_display_colour_choices,
                     NULL, 2, &out->labels[ORACLES_DISPLAY_COLOUR], &out->explanations[0], NULL, out->colour, &out->rows[ORACLES_DISPLAY_COLOUR])
         + DISPLAY_ROW_GAP;
    y += display_row(value_x, y, value_w, oracles_display_labels[ORACLES_DISPLAY_TRANSITIONS], left, t->explanations[1], transitions_notes, 1,
                     oracles_ui_transition_choices, NULL, 2, &out->labels[ORACLES_DISPLAY_TRANSITIONS], &out->explanations[1], transitions_out,
                     out->transitions, &out->rows[ORACLES_DISPLAY_TRANSITIONS]);
    out->panel = box(PANEL_X, PANEL_Y, PANEL_W, y + PANEL_PAD_Y + PANEL_BORDER - PANEL_Y);
}

/* A text wrapped at `width` in lines of `line_height`, the first at (x, y). */
static void wrap_lines(const OraclesUiTextStyle *style, const char *text, float width, float x, float y, float line_height, OraclesUiWrapped *out)
{
    oracles_ui_wrap(style, text, width, x, y, out);
    for (unsigned i = 0; i < out->count; i++) out->lines[i] = line_at(style, out->text[i], x, y + (float)i * line_height, line_height);
    out->h = (float)out->count * line_height;
}

/* The first mod the list shows, `shown` at most: all from the first while they fit, else the highlighted one among
 * the last shown. */
static unsigned first_shown(const OraclesHomeNav *nav, unsigned count, unsigned shown)
{
    if (count <= shown) return 0;
    const unsigned row = nav->row, play = oracles_mods_row_play(nav);
    const unsigned focused = row == ORACLES_MODS_ROW_FOLDER ? 0u : row >= play ? count - 1u : row - 1u;
    return focused < shown ? 0u : focused - (shown - 1u);
}

static void mods_16_9(const OraclesHomeNav *nav, OraclesUiModsLayout *out)
{
    const OraclesHomeHero hero = oracles_home_hero(nav);
    page_head_16_9("Mods", oracles_home_over(hero), oracles_home_title(hero), out->state, &out->head);
    const float left = PANEL_X + PANEL_BORDER + PANEL_PAD_X, width = PANEL_W - 2.0f * (PANEL_BORDER + PANEL_PAD_X);
    const float value_x = left + LABEL_W + GRID_GAP, value_w = width - LABEL_W - GRID_GAP;
    const float inner_x = value_x + ROW_PAD_X, inner_w = value_w - 2.0f * ROW_PAD_X, right = inner_x + inner_w;
    float y = PANEL_Y + PANEL_BORDER + PANEL_PAD_Y;

    /* Folder: its path and where it is; Open folder at the right. */
    out->label_folder = line_at(&oracles_ui_row_label, "Folder", left, y + LABEL_PAD_TOP, 0.0f);
    float inner_y = y + ROW_PAD_Y;
    button("Open folder", right, 0.0f, 0.0f, &out->folder_button, &out->folder_button_dot);
    const float room = inner_w - ROW_GAP - (out->folder_button_dot.x + out->folder_button_dot.w - out->folder_button.x);
    oracles_ui_fit(&oracles_ui_row_path, nav->mods_folder, room, out->folder_text, sizeof out->folder_text);
    out->folder_path = line_at(&oracles_ui_row_path, out->folder_text, inner_x, inner_y, 0.0f);
    out->folder_line = line_at(&oracles_ui_row_text, oracles_mods_folder_line, inner_x, out->folder_path.y + out->folder_path.h + FILE_GAP, 0.0f);
    const float content = out->folder_line.y + out->folder_line.h - inner_y;
    button("Open folder", right, inner_y, content, &out->folder_button, &out->folder_button_dot);
    out->folder_row = box(value_x, y, value_w, content + 2.0f * ROW_PAD_Y);
    y += out->folder_row.h + GAME_ROW_GAP;

    /* Mods: each mod with its switch, or the line that says where to put them; the count and the note on the save. */
    out->label_mods = line_at(&oracles_ui_row_label, "Mods", left, y + LABEL_PAD_TOP, 0.0f);
    const float list_y = y;
    const OraclesHomeMods *mods = oracles_mods_list(nav);
    if (!mods->count) {
        wrap_lines(&oracles_ui_row_status, oracles_mods_empty, inner_w, inner_x, y + MODS_EMPTY_PAD_Y, MODS_EMPTY_LINE, &out->empty);
        y += out->empty.h + 2.0f * MODS_EMPTY_PAD_Y + MODS_ROW_GAP;
    }
    const unsigned first = first_shown(nav, mods->count, ORACLES_UI_MODS_SHOWN);
    for (unsigned i = first; i < mods->count && out->shown < ORACLES_UI_MODS_SHOWN; i++) {
        const OraclesHomeMod *mod = &mods->mods[i];
        OraclesUiModLayout *m = &out->mods[out->shown++];
        m->index = i;
        const float text_x = inner_x + MODS_TOGGLE_COLUMN + GRID_GAP, text_w = right - text_x;
        inner_y = y + ROW_PAD_Y;
        oracles_ui_fit(&oracles_ui_row_title, mod->name, text_w, m->name_text, sizeof m->name_text);
        m->name = line_at(&oracles_ui_row_title, m->name_text, text_x, inner_y, 0.0f);
        char games[64];
        oracles_mods_games(mod, games, sizeof games);
        const float games_x = m->name.x + m->name.w + MODS_NAME_GAP;
        oracles_ui_fit(&oracles_ui_mod_games, games, right - games_x, m->games_text, sizeof m->games_text);
        m->games = line_at(&oracles_ui_mod_games, m->games_text, games_x, 0.0f, 0.0f);
        move_line(&m->games, games_x, m->name.baseline - (m->games.baseline - m->games.y));   /* on the name's baseline */
        const float line_y = m->name.y + m->name.h + FILE_GAP;
        oracles_ui_fit(&oracles_ui_row_text, mod->line, text_w - DOT - DOT_GAP, m->line_text, sizeof m->line_text);
        m->line = line_at(&oracles_ui_row_text, m->line_text, text_x + DOT + DOT_GAP, line_y, 0.0f);
        m->dot = box(text_x, line_y + (m->line.h - DOT) * 0.5f, DOT, DOT);
        const float row_h = m->line.y + m->line.h - inner_y + 2.0f * ROW_PAD_Y;
        m->row = box(value_x, y, value_w, row_h);
        m->toggle = box(inner_x, y + (row_h - MODS_TOGGLE_H) * 0.5f, MODS_TOGGLE_W, MODS_TOGGLE_H);
        m->knob = box(m->toggle.x + MODS_KNOB_INSET + (mod->active ? MODS_KNOB_ON : 0.0f), m->toggle.y + MODS_KNOB_INSET, MODS_KNOB, MODS_KNOB);
        y += row_h + MODS_ROW_GAP;
    }
    oracles_mods_count(nav, out->count_text, sizeof out->count_text);
    out->count = line_at(&oracles_ui_row_text, out->count_text, inner_x, y + MODS_COUNT_PAD_TOP, 0.0f);
    y = out->count.y + out->count.h + MODS_ROW_GAP;
    wrap_lines(&oracles_ui_row_note, oracles_mods_note, inner_w, inner_x, y, MODS_NOTE_LINE, &out->note);
    y += out->note.h;
    out->list = box(value_x, list_y, value_w, y - list_y);
    y += GAME_ROW_GAP;

    /* Play, right-aligned across the panel, with why it cannot start when it cannot. */
    const float play_right = left + width - ROW_PAD_X;
    out->play_dot = line_at(&oracles_ui_play, oracles_ui_item_dot, 0.0f, y + PLAY_PAD_Y, 0.0f);
    move_line(&out->play_dot, play_right - out->play_dot.w, out->play_dot.y);
    out->play = line_at(&oracles_ui_play, "Play", 0.0f, y + PLAY_PAD_Y, 0.0f);
    move_line(&out->play, out->play_dot.x - PLAY_GAP - out->play.w, out->play.y);
    out->play_note_text = oracles_mods_play_note(nav);
    out->play_note = line_at(&oracles_ui_row_text, out->play_note_text, 0.0f, 0.0f, 0.0f);
    move_line(&out->play_note, out->play.x - PLAY_GAP - out->play_note.w, out->play.y + (out->play.h - out->play_note.h) * 0.5f);
    out->play_row = box(left, y, width, out->play.h + 2.0f * PLAY_PAD_Y);
    y += out->play_row.h;
    out->panel = box(PANEL_X, PANEL_Y, PANEL_W, y + PANEL_PAD_Y + PANEL_BORDER - PANEL_Y);
}

/* ---- 4:3 -------------------------------------------------------------------------------------------------------- */

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
#define LABEL_W_4_3 200.0f
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
#define HELP_MARGIN_4_3 14.0f             /* past the rows' gap */
#define HELP_LINE_4_3 47.0f               /* the explanation, on one line */
#define HELP_NOTE_LINE_4_3 40.0f          /* its note, on one line */
#define HELP_NOTE_GAP_4_3 4.0f
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
#define RIGHT_4_3 (ROWS_X_4_3 + ROWS_W_4_3 - ROW_PAD_X_4_3)
#define DISPLAY_VALUE_X_4_3 (LEFT_4_3 + DISPLAY_LABEL_W_4_3 + GRID_GAP_4_3)

static float max_f(float a, float b) { return a > b ? a : b; }

/* JavaScript's Math.round, as the mockup halves the diagram. */
static float round_half_up(float v) { return floorf(v + 0.5f); }

static void page_head_4_3(const char *section, const char *over, const char *title, const char *state, OraclesUiPageHead *out)
{
    memset(out, 0, sizeof *out);
    out->section = line_at(&oracles_ui_page_section_4_3, section, HEAD_X_4_3, HEAD_Y_4_3, 0.0f);
    /* The title with its words when they are "Oracle of": "Oracle of Ages"; a fan game's alone. */
    char line[ORACLES_HOME_TEXT_LENGTH];
    if (over && !strcmp(over, "Oracle of")) snprintf(line, sizeof line, "%s %s", over, title ? title : "");
    else snprintf(line, sizeof line, "%s", title ? title : "");
    const float title_y = out->section.y + out->section.h + HEAD_GAP_4_3;
    wrap_lines(&oracles_ui_page_title_4_3, line, HEAD_W_4_3, HEAD_X_4_3, title_y, HEAD_TITLE_LINE_4_3, &out->title);
    if (state) out->state = line_at(&oracles_ui_page_state_4_3, state, HEAD_X_4_3, title_y + out->title.h + HEAD_GAP_4_3, 0.0f);
}

/* The lines of a grid row aligned on one baseline from `top`: their tops moved, the row's bottom returned.  A line's
 * height past its baseline counts whole (a path on two lines). */
static float baseline_row(OraclesUiLine *const *lines, unsigned count, float top)
{
    float ascent = 0.0f, descent = 0.0f;
    for (unsigned i = 0; i < count; i++) {
        ascent = max_f(ascent, lines[i]->baseline - lines[i]->y);
        descent = max_f(descent, lines[i]->y + lines[i]->h - lines[i]->baseline);
    }
    for (unsigned i = 0; i < count; i++) move_line(lines[i], lines[i]->x, top + ascent - (lines[i]->baseline - lines[i]->y));
    return top + ascent + descent;
}

/* A button and its dot at the right of the rows, unplaced vertically; returns their width. */
static float place_button_4_3(const char *label, OraclesUiLine *text, OraclesUiLine *dot)
{
    *dot = line_at(&oracles_ui_row_button_4_3, oracles_ui_item_dot, 0.0f, 0.0f, 0.0f);
    move_line(dot, RIGHT_4_3 - dot->w, 0.0f);
    *text = line_at(&oracles_ui_row_button_4_3, label, 0.0f, 0.0f, 0.0f);
    move_line(text, dot->x - BUTTON_GAP_4_3 - text->w, 0.0f);
    return dot->x + dot->w - text->x;
}

/* The values' room left of a button `button_w` wide. */
static float file_room_4_3(float button_w) { return RIGHT_4_3 - button_w - GRID_GAP_4_3 - VALUE_X_4_3; }

/* A status: its dot, centred on the line, then its text. Returns the line's bottom. */
static float status_4_3(const char *text, float y, OraclesUiLine *line, OraclesUiBox *dot)
{
    *line = line_at(&oracles_ui_row_text_4_3, text, VALUE_X_4_3 + DOT_4_3 + DOT_GAP_4_3, y, 0.0f);
    *dot = box(VALUE_X_4_3, y + (line->h - DOT_4_3) * 0.5f, DOT_4_3, DOT_4_3);
    return y + line->h;
}

/* A row's first line, the label, the file and the button on one baseline, from the row's top `y`; returns its bottom. */
static float file_line_4_3(const char *label, OraclesUiLine *label_out, const char *file, OraclesUiLine *file_out, OraclesUiLine *button,
                           OraclesUiLine *dot, float y)
{
    *label_out = line_at(&oracles_ui_row_label_4_3, label, LEFT_4_3, 0.0f, 0.0f);
    *file_out = line_at(&oracles_ui_row_title_4_3, file, VALUE_X_4_3, 0.0f, 0.0f);
    OraclesUiLine *const lines[4] = { label_out, file_out, button, dot };
    return baseline_row(lines, 4, y + ROW_PAD_Y_4_3);
}

/* The row's box from `y` to `bottom`, its padding under, 100 tall at least. */
static OraclesUiBox row_4_3(float y, float bottom)
{
    return box(ROWS_X_4_3, y, ROWS_W_4_3, max_f(ROW_MIN_H_4_3, bottom + ROW_PAD_Y_4_3 - y));
}

/* Play at the right of its row, centred in it, with its note before it. */
static OraclesUiBox play_4_3_row(const char *note, float y, OraclesUiLine *note_out, OraclesUiLine *play, OraclesUiLine *dot)
{
    *dot = line_at(&oracles_ui_play_4_3, oracles_ui_item_dot, 0.0f, 0.0f, 0.0f);
    *play = line_at(&oracles_ui_play_4_3, "Play", 0.0f, 0.0f, 0.0f);
    *note_out = line_at(&oracles_ui_row_text_4_3, note, 0.0f, 0.0f, 0.0f);
    const float h = max_f(ROW_MIN_H_4_3, play->h);
    move_line(dot, RIGHT_4_3 - dot->w, y + (h - dot->h) * 0.5f);
    move_line(play, dot->x - PLAY_GAP_4_3 - play->w, y + (h - play->h) * 0.5f);
    move_line(note_out, play->x - PLAY_GAP_4_3 - note_out->w, y + (h - note_out->h) * 0.5f);
    return box(ROWS_X_4_3, y, ROWS_W_4_3, h);
}

#define FIT(style, field, room) do { oracles_ui_fit(style, field, room, fitted, sizeof field); memcpy(field, fitted, sizeof field); } while (0)

static void game_4_3(OraclesUiGameTexts *t, OraclesUiGameLayout *out)
{
    page_head_4_3("Cartridge", t->over, t->title, t->state, &out->head);
    char fitted[ORACLES_HOME_TEXT_LENGTH * 4];
    const float span = RIGHT_4_3 - VALUE_X_4_3;   /* a line under the first, across the file's column and the button's */
    float y = ROWS_Y_4_3;

    /* ROM: the file, its folder, its status, a note or two; a fan game's base ROM: the file and its status. */
    float room = file_room_4_3(place_button_4_3("Choose ROM\xe2\x80\xa6", &out->rom_button, &out->rom_button_dot));
    FIT(&oracles_ui_row_title_4_3, t->rom_file, room);
    FIT(&oracles_ui_row_path_4_3, t->rom_folder, span);
    FIT(&oracles_ui_row_text_4_3, t->rom_status, span - DOT_4_3 - DOT_GAP_4_3);
    float bottom = file_line_4_3(t->patched ? "Base ROM" : "ROM", &out->label_rom, t->rom_file, &out->rom_file, &out->rom_button,
                                 &out->rom_button_dot, y);
    if (!t->patched) {
        out->rom_folder = block_at(&oracles_ui_row_path_4_3, t->rom_folder, VALUE_X_4_3, bottom + GRID_ROW_GAP_4_3);
        bottom = out->rom_folder.y + out->rom_folder.h;
    }
    bottom = status_4_3(t->rom_status, bottom + GRID_ROW_GAP_4_3, &out->rom_status, &out->rom_dot);
    if (t->rom_note && t->rom_note[0]) {
        out->rom_note = line_at(&oracles_ui_row_text_4_3, t->rom_note, VALUE_X_4_3, bottom + GRID_ROW_GAP_4_3, 0.0f);
        bottom = out->rom_note.y + out->rom_note.h;
    }
    if (t->rom_hotkeys_note && t->rom_hotkeys_note[0]) {
        out->rom_hotkeys_note = line_at(&oracles_ui_row_text_4_3, t->rom_hotkeys_note, VALUE_X_4_3, bottom + GRID_ROW_GAP_4_3, 0.0f);
        bottom = out->rom_hotkeys_note.y + out->rom_hotkeys_note.h;
    }
    out->rows[ORACLES_ROW_ROM] = row_4_3(y, bottom);
    y += out->rows[ORACLES_ROW_ROM].h + ROW_GAP_4_3;

    if (t->patched) {
        /* Patch: the file and its status; under the row, the patch's folder and the image the two files make. */
        room = file_room_4_3(place_button_4_3("Choose patch\xe2\x80\xa6", &out->patch_button, &out->patch_button_dot));
        FIT(&oracles_ui_row_title_4_3, t->patch_file, room);
        FIT(&oracles_ui_row_text_4_3, t->patch_status, span - DOT_4_3 - DOT_GAP_4_3);
        bottom = file_line_4_3("Patch", &out->label_patch, t->patch_file, &out->patch_file, &out->patch_button, &out->patch_button_dot, y);
        bottom = status_4_3(t->patch_status, bottom + GRID_ROW_GAP_4_3, &out->patch_status, &out->patch_dot);
        out->rows[ORACLES_ROW_PATCH] = row_4_3(y, bottom);
        y += out->rows[ORACLES_ROW_PATCH].h + ROW_GAP_4_3;
        FIT(&oracles_ui_row_path_4_3, t->patch_folder, span);
        FIT(&oracles_ui_row_text_4_3, t->image_status, span - DOT_4_3 - DOT_GAP_4_3);
        out->patch_folder = block_at(&oracles_ui_row_path_4_3, t->patch_folder, VALUE_X_4_3, y);
        y = status_4_3(t->image_status, out->patch_folder.y + out->patch_folder.h + IMAGE_GAP_4_3, &out->image_status, &out->image_dot) + ROW_GAP_4_3;
    }

    /* Save: the file and a line about it; Open folder at the right. */
    room = file_room_4_3(place_button_4_3("Open folder", &out->save_button, &out->save_button_dot));
    FIT(&oracles_ui_row_title_4_3, t->save_file, room);
    FIT(&oracles_ui_row_text_4_3, t->save_line, span);
    bottom = file_line_4_3("Save", &out->label_save, t->save_file, &out->save_file, &out->save_button, &out->save_button_dot, y);
    out->save_line = block_at(&oracles_ui_row_text_4_3, t->save_line, VALUE_X_4_3, bottom + GRID_ROW_GAP_4_3);
    out->rows[ORACLES_ROW_SAVE] = row_4_3(y, out->save_line.y + out->save_line.h);
    y += out->rows[ORACLES_ROW_SAVE].h + ROW_GAP_4_3;

    out->rows[ORACLES_ROW_PLAY] = play_4_3_row(t->play_note, y, &out->play_note, &out->play, &out->play_dot);
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
        o->name = line_at(&oracles_ui_row_button_4_3, names[i], 0.0f, 0.0f, OPTION_NAME_LINE_4_3);
        memset(&o->size, 0, sizeof o->size);
        if (sizes) o->size = line_at(size_style, sizes[i], 0.0f, 0.0f, OPTION_SIZE_LINE_4_3);
        const float size_w = size_lengths ? max_f(o->size.w, characters(size_style, size_lengths[i])) : o->size.w;
        const float inner_w = max_f(o->name.w, size_w), inner_h = o->name.h + o->size.h;
        o->box.w = max_f(OPTION_MIN_W_4_3, inner_w + 2.0f * (pad_x + OPTION_BORDER_4_3));
        o->box.h = max_f(OPTION_MIN_H_4_3, inner_h + 2.0f * (OPTION_PAD_Y_4_3 + OPTION_BORDER_4_3));
        if (line_x > x && line_x + o->box.w > x + width) {   /* wrapped to the next line */
            line_x = x;
            line_y += line_h + gap;
            line_h = 0.0f;
        }
        o->box.x = line_x;
        o->box.y = line_y;
        const float top = o->box.y + (o->box.h - inner_h) * 0.5f;
        move_line(&o->name, o->box.x + (o->box.w - o->name.w) * 0.5f, top);
        if (sizes) move_line(&o->size, o->box.x + (o->box.w - o->size.w) * 0.5f, top + o->name.h);
        line_x += o->box.w + gap;
        line_h = max_f(line_h, o->box.h);
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
    for (unsigned i = 0; i < count; i++) shift_option(&o[i], 0.0f, dy);
}

/* A row of Display: its label wrapped in its column and `content_h` of values, both centred in the row, 100 tall at
 * least.  Returns the values' top. */
static float display_row_4_3(float y, const char *label, float content_h, OraclesUiWrapped *label_out, OraclesUiBox *row)
{
    oracles_ui_wrap(&oracles_ui_row_label_4_3, label, DISPLAY_LABEL_W_4_3, LEFT_4_3, 0.0f, label_out);
    const float track = max_f(ROW_MIN_H_4_3 - 2.0f * DISPLAY_PAD_Y_4_3, max_f(label_out->h, content_h));
    *row = box(ROWS_X_4_3, y, ROWS_W_4_3, track + 2.0f * DISPLAY_PAD_Y_4_3);
    move_wrapped(label_out, y + DISPLAY_PAD_Y_4_3 + (track - label_out->h) * 0.5f);
    return y + DISPLAY_PAD_Y_4_3 + (track - content_h) * 0.5f;
}

/* A row of Display whose values are options (their size lines `size_lengths` characters wide at least, NULL: their
 * own): returns the row's bottom. */
static float options_row_4_3(float y, unsigned row, const char *const *names, const char *const *sizes, const int *size_lengths, unsigned count,
                             OraclesUiOptionLayout *options, OraclesUiDisplayLayout *out)
{
    const float width = RIGHT_4_3 - DISPLAY_VALUE_X_4_3;
    const float h = framed_4_3(names, sizes, size_lengths, count, DISPLAY_VALUE_X_4_3, 0.0f, width, options);
    shift_options(options, count, display_row_4_3(y, oracles_display_labels[row], h, &out->labels[row], &out->rows[row]));
    return out->rows[row].y + out->rows[row].h;
}

static void display_4_3(const OraclesUiDisplayTexts *t, OraclesUiDisplayLayout *out)
{
    page_head_4_3(t->section ? t->section : "Display", t->over, t->title, NULL, &out->head);
    /* Opened from a game, under the title: what only the next session takes, said once for the page. */
    if (t->later)
        out->page_note = line_at(&oracles_ui_page_note_4_3, oracles_ui_display_later, HEAD_X_4_3, out->head.title.lines[0].y + out->head.title.h + HEAD_GAP_4_3,
                                 0.0f);
    /* The diagram at the top right, half 16:9's, without its line: Window's help says the size. */
    const float box_w = round_half_up((t->diagram_box_w > 0.0f ? t->diagram_box_w : ORACLES_UI_DIAGRAM_W) * 0.5f);
    const float w = round_half_up(t->diagram_w * 0.5f), h = round_half_up(t->diagram_h * 0.5f);
    out->diagram = box(DIAGRAM_RIGHT_4_3 - box_w, HEAD_Y_4_3, box_w, DIAGRAM_H_4_3);
    out->diagram_window = box(out->diagram.x + (box_w - w) * 0.5f, out->diagram.y + (DIAGRAM_H_4_3 - h) * 0.5f, w, h);
    /* Advanced beside it, framed: its label, then the arrow, both centred in it; not on the Advanced rows. */
    if (!t->advanced) {
        out->advanced_label = line_at(&oracles_ui_row_label_4_3, oracles_display_labels[ORACLES_DISPLAY_ADVANCED], 0.0f, 0.0f, 0.0f);
        const float button_w = 2.0f * (ADVANCED_BORDER_4_3 + ADVANCED_PAD_X_4_3) + out->advanced_label.w + ADVANCED_GAP_4_3 + ADVANCED_ARROW_W_4_3;
        const float right = out->diagram.x - ADVANCED_MARGIN_4_3;
        out->rows[ORACLES_DISPLAY_ADVANCED] = box(right - button_w, HEAD_Y_4_3, button_w, ADVANCED_H_4_3);
        const float label_x = right - button_w + ADVANCED_BORDER_4_3 + ADVANCED_PAD_X_4_3;
        move_line(&out->advanced_label, label_x, HEAD_Y_4_3 + (ADVANCED_H_4_3 - out->advanced_label.h) * 0.5f);
        out->advanced_arrow = box(label_x + out->advanced_label.w + ADVANCED_GAP_4_3, HEAD_Y_4_3 + (ADVANCED_H_4_3 - ADVANCED_ARROW_H_4_3) * 0.5f,
                                  ADVANCED_ARROW_W_4_3, ADVANCED_ARROW_H_4_3);
    }

    float y = ROWS_Y_4_3;
    const float width = RIGHT_4_3 - DISPLAY_VALUE_X_4_3;
    if (t->advanced) {
        const char *workers[3];
        for (int i = 0; i < 3; i++) workers[i] = t->workers_names[i];
        y = options_row_4_3(y, ORACLES_DISPLAY_CORE, oracles_display_core_choices, NULL, NULL, 2, out->core, out) + DISPLAY_ROW_GAP_4_3;
        y = options_row_4_3(y, ORACLES_DISPLAY_VSYNC, oracles_display_vsync_choices, NULL, NULL, 3, out->vsync, out) + DISPLAY_ROW_GAP_4_3;
        y = options_row_4_3(y, ORACLES_DISPLAY_WORKERS, workers, NULL, NULL, 3, out->workers, out);
    } else {
        const char *profile_sizes[ORACLES_PROFILES], *quality_sets[ORACLES_DISPLAY_QUALITIES + 1], *names[4], *sizes[4], *view_sizes[3];
        for (int p = 0; p < ORACLES_PROFILES; p++) profile_sizes[p] = t->profile_sizes[p];
        for (int q = 0; q <= ORACLES_DISPLAY_QUALITIES; q++) quality_sets[q] = t->quality_sets[q];
        for (int i = 0; i < 4; i++) { names[i] = t->window_names[i]; sizes[i] = t->window_sizes[i]; }
        for (int v = 0; v < 3; v++) view_sizes[v] = t->view_sizes[v];
        y = options_row_4_3(y, ORACLES_DISPLAY_PROFILE, oracles_profile_names, profile_sizes, NULL, ORACLES_PROFILES, out->profiles, out)
            + DISPLAY_ROW_GAP_4_3;
        /* Quality: its choices narrower, what each sets under its name in the running text's face, Custom's place kept. */
        const float quality_h = framed_4_3_as(oracles_display_quality_names, quality_sets, NULL, &oracles_ui_quality_sets_4_3, QUALITY_PAD_X_4_3,
                                              QUALITY_GAP_4_3, ORACLES_DISPLAY_QUALITIES + 1, DISPLAY_VALUE_X_4_3, 0.0f, width, out->qualities);
        shift_options(out->qualities, ORACLES_DISPLAY_QUALITIES + 1,
                      display_row_4_3(y, oracles_display_labels[ORACLES_DISPLAY_QUALITY], quality_h, &out->labels[ORACLES_DISPLAY_QUALITY],
                                      &out->rows[ORACLES_DISPLAY_QUALITY]));
        y = out->rows[ORACLES_DISPLAY_QUALITY].y + out->rows[ORACLES_DISPLAY_QUALITY].h + DISPLAY_ROW_GAP_4_3;
        /* Window: its choices, or at one size the line that says so in their place, as tall. */
        if (t->window_one) {
            out->window_one = line_at(&oracles_ui_row_text_4_3, oracles_display_one_size_note, DISPLAY_VALUE_X_4_3, 0.0f, 0.0f);
            const float top = display_row_4_3(y, oracles_display_labels[ORACLES_DISPLAY_WINDOW], OPTION_MIN_H_4_3, &out->labels[ORACLES_DISPLAY_WINDOW],
                                              &out->rows[ORACLES_DISPLAY_WINDOW]);
            move_line(&out->window_one, DISPLAY_VALUE_X_4_3, top + (OPTION_MIN_H_4_3 - out->window_one.h) * 0.5f);
            y = out->rows[ORACLES_DISPLAY_WINDOW].y + out->rows[ORACLES_DISPLAY_WINDOW].h + DISPLAY_ROW_GAP_4_3;
        } else {
            y = options_row_4_3(y, ORACLES_DISPLAY_WINDOW, names, sizes, t->window_size_length, 4, out->windows, out) + DISPLAY_ROW_GAP_4_3;
        }
        y = options_row_4_3(y, ORACLES_DISPLAY_VIEW, oracles_display_view_names, view_sizes, NULL, 3, out->views, out) + DISPLAY_ROW_GAP_4_3;
        y = options_row_4_3(y, ORACLES_DISPLAY_COLOUR, oracles_display_colour_choices, NULL, NULL, 2, out->colour, out) + DISPLAY_ROW_GAP_4_3;
        y = options_row_4_3(y, ORACLES_DISPLAY_TRANSITIONS, oracles_ui_transition_choices, NULL, NULL, 2, out->transitions, out);
    }
    /* Under the rows, the highlighted row's explanation on one line, and its note on the next, kept when empty. */
    out->help = line_at(&oracles_ui_help_4_3, t->help, LEFT_4_3, y + DISPLAY_ROW_GAP_4_3 + HELP_MARGIN_4_3, HELP_LINE_4_3);
    out->help_note = line_at(&oracles_ui_row_note_4_3, t->help_note, LEFT_4_3, out->help.y + out->help.h + HELP_NOTE_GAP_4_3, HELP_NOTE_LINE_4_3);
}

/* `text` broken as a browser breaks a path with overflow-wrap: anywhere, after a hyphen or a space when the line has
 * one, its lines as wide as fit in `width`, `max_lines` at most: shortened in its middle first when it would take more
 * (a path's end stays). */
static void wrap_anywhere(const OraclesUiTextStyle *style, const char *text, float width, unsigned max_lines, float x, float y, OraclesUiWrapped *out)
{
    char fitted[ORACLES_HOME_TEXT_LENGTH * 4];
    float room = width * (float)max_lines;
    for (int attempt = 0; attempt < 8; attempt++, room -= width * 0.1f) {
        oracles_ui_fit(style, text, room, fitted, sizeof fitted);
        memset(out, 0, sizeof *out);
        const char *p = fitted;
        while (*p && out->count < ORACLES_UI_WRAP_LINES) {
            /* The most whole characters that fit, one at least; then the line up to its last hyphen or space, if any,
             * when the text goes on. */
            size_t length = 0, opportunity = 0;
            for (;;) {
                const char *next = p + length;
                oracles_ui_utf8_next(&next);
                const size_t candidate = (size_t)(next - p);
                if (candidate >= ORACLES_UI_WRAP_LENGTH) break;
                char piece[ORACLES_UI_WRAP_LENGTH];
                memcpy(piece, p, candidate);
                piece[candidate] = 0;
                if (length && oracles_ui_text_width(style, piece) > width) break;
                length = candidate;
                if (p[length - 1] == '-' || p[length - 1] == ' ') opportunity = length;
                if (!p[length]) break;
            }
            if (!length) break;
            if (p[length] && opportunity) length = opportunity;
            memcpy(out->text[out->count], p, length);
            out->text[out->count][length] = 0;
            out->count++;
            p += length;
        }
        if (out->count <= max_lines && !*p) break;
    }
    float height, baseline;
    oracles_ui_line_box(style, 0.0f, &height, &baseline);
    for (unsigned i = 0; i < out->count; i++) {
        out->lines[i] = line_at(style, out->text[i], x, y + (float)i * height, 0.0f);
        out->w = max_f(out->w, out->lines[i].w);
    }
    out->h = (float)out->count * height;
}

static void mods_4_3(const OraclesHomeNav *nav, OraclesUiModsLayout *out)
{
    const OraclesHomeHero hero = oracles_home_hero(nav);
    page_head_4_3("Mods", oracles_home_over(hero), oracles_home_title(hero), out->state, &out->head);
    float y = ROWS_Y_4_3;

    /* Folder: its path, broken anywhere on two lines at most, and where it is; Open folder at the right. */
    const float room = file_room_4_3(place_button_4_3("Open folder", &out->folder_button, &out->folder_button_dot));
    out->label_folder = line_at(&oracles_ui_row_label_4_3, "Folder", LEFT_4_3, 0.0f, 0.0f);
    wrap_anywhere(&oracles_ui_row_path_4_3, nav->mods_folder, room, MODS_FOLDER_LINES_4_3, VALUE_X_4_3, 0.0f, &out->folder_lines);
    snprintf(out->folder_text, sizeof out->folder_text, "%s", out->folder_lines.count ? out->folder_lines.text[0] : "");
    OraclesUiLine path = line_at(&oracles_ui_row_path_4_3, out->folder_text, VALUE_X_4_3, 0.0f, 0.0f);
    const float path_line = path.h;
    if (out->folder_lines.count > 1) path.h = out->folder_lines.h;   /* the whole path under the first line's baseline */
    OraclesUiLine *const first[4] = { &out->label_folder, &path, &out->folder_button, &out->folder_button_dot };
    const float first_bottom = baseline_row(first, 4, y + ROW_PAD_Y_4_3);
    for (unsigned i = 0; i < out->folder_lines.count; i++) move_line(&out->folder_lines.lines[i], VALUE_X_4_3, path.y + (float)i * path_line);
    out->folder_path = out->folder_lines.count ? out->folder_lines.lines[0] : path;
    out->folder_line = line_at(&oracles_ui_row_text_4_3, oracles_mods_folder_line, VALUE_X_4_3, first_bottom + GRID_ROW_GAP_4_3, 0.0f);
    out->folder_row = row_4_3(y, out->folder_line.y + out->folder_line.h);
    y += out->folder_row.h + ROW_GAP_4_3;

    /* Mods: each mod with its switch, its row reaching left under the label's gap, or the line that says where to put
     * them; the count and the note on the save. */
    const float list_y = y, list_w = RIGHT_4_3 + ROW_PAD_X_4_3 - VALUE_X_4_3;
    out->label_mods = line_at(&oracles_ui_row_label_4_3, "Mods", LEFT_4_3, y + MODS_LABEL_PAD_4_3, 0.0f);
    const OraclesHomeMods *mods = oracles_mods_list(nav);
    if (!mods->count) {
        wrap_lines(&oracles_ui_row_text_4_3, oracles_mods_empty, list_w - ROW_PAD_X_4_3, VALUE_X_4_3, y + MODS_EMPTY_PAD_4_3, MODS_EMPTY_LINE_4_3,
                   &out->empty);
        y += out->empty.h + 2.0f * MODS_EMPTY_PAD_4_3 + MODS_ROW_GAP_4_3;
    }
    const float row_x = VALUE_X_4_3 - ROW_PAD_X_4_3, row_w = list_w + ROW_PAD_X_4_3;
    const float text_x = VALUE_X_4_3 + MODS_TOGGLE_W_4_3 + GRID_GAP_4_3, text_w = RIGHT_4_3 - text_x;
    const unsigned first_mod = first_shown(nav, mods->count, ORACLES_UI_MODS_SHOWN_4_3);
    for (unsigned i = first_mod; i < mods->count && out->shown < ORACLES_UI_MODS_SHOWN_4_3; i++) {
        const OraclesHomeMod *mod = &mods->mods[i];
        OraclesUiModLayout *m = &out->mods[out->shown++];
        m->index = i;
        oracles_ui_fit(&oracles_ui_row_title_4_3, mod->name, text_w, m->name_text, sizeof m->name_text);
        m->name = line_at(&oracles_ui_row_title_4_3, m->name_text, text_x, 0.0f, 0.0f);
        char games[64];
        oracles_mods_games(mod, games, sizeof games);
        const float games_x = m->name.x + m->name.w + MODS_NAME_GAP_4_3;
        oracles_ui_fit(&oracles_ui_mod_games_4_3, games, RIGHT_4_3 - games_x, m->games_text, sizeof m->games_text);
        m->games = line_at(&oracles_ui_mod_games_4_3, m->games_text, games_x, 0.0f, 0.0f);
        OraclesUiLine *const name_line[2] = { &m->name, &m->games };
        const float line_y = baseline_row(name_line, 2, y + MODS_ROW_PAD_Y_4_3) + GRID_ROW_GAP_4_3;
        oracles_ui_fit(&oracles_ui_row_text_4_3, mod->line, text_w - DOT_4_3 - DOT_GAP_4_3, m->line_text, sizeof m->line_text);
        m->line = line_at(&oracles_ui_row_text_4_3, m->line_text, text_x + DOT_4_3 + DOT_GAP_4_3, line_y, 0.0f);
        m->dot = box(text_x, line_y + (m->line.h - DOT_4_3) * 0.5f, DOT_4_3, DOT_4_3);
        const float row_h = max_f(ROW_MIN_H_4_3, m->line.y + m->line.h + MODS_ROW_PAD_Y_4_3 - y);
        m->row = box(row_x, y, row_w, row_h);
        m->toggle = box(VALUE_X_4_3, y + (row_h - MODS_TOGGLE_H_4_3) * 0.5f, MODS_TOGGLE_W_4_3, MODS_TOGGLE_H_4_3);
        m->knob = box(m->toggle.x + MODS_KNOB_INSET_4_3 + (mod->active && !mod->refused ? MODS_KNOB_ON_4_3 : 0.0f), m->toggle.y + MODS_KNOB_INSET_4_3,
                      MODS_KNOB_4_3, MODS_KNOB_4_3);
        y += row_h + MODS_ROW_GAP_4_3;
    }
    oracles_mods_count(nav, out->count_text, sizeof out->count_text);
    out->count = line_at(&oracles_ui_row_text_4_3, out->count_text, VALUE_X_4_3, y + MODS_COUNT_PAD_4_3, 0.0f);
    y = out->count.y + out->count.h + MODS_ROW_GAP_4_3;
    wrap_lines(&oracles_ui_page_note_4_3, oracles_mods_note, list_w, VALUE_X_4_3, y, MODS_NOTE_LINE_4_3, &out->note);
    y += out->note.h;
    out->list = box(VALUE_X_4_3, list_y, list_w, y - list_y);
    y += ROW_GAP_4_3;

    out->play_note_text = oracles_mods_play_note(nav);
    out->play_row = play_4_3_row(out->play_note_text, y, &out->play_note, &out->play, &out->play_dot);
}

/* ---- the layouts --------------------------------------------------------------------------------------------- */

void oracles_ui_layout_page_head(OraclesUiLayout layout, const char *section, const char *over, const char *title, const char *state,
                                 OraclesUiPageHead *out)
{
    if (layout == ORACLES_UI_LAYOUT_4_3) page_head_4_3(section, over, title, state, out);
    else page_head_16_9(section, over, title, state, out);
}

void oracles_ui_layout_game(OraclesUiLayout layout, OraclesUiGameTexts *t, OraclesUiGameLayout *out)
{
    memset(out, 0, sizeof *out);
    if (layout == ORACLES_UI_LAYOUT_4_3) game_4_3(t, out);
    else game_16_9(t, out);
}

void oracles_ui_layout_display(OraclesUiLayout layout, const OraclesUiDisplayTexts *t, OraclesUiDisplayLayout *out)
{
    memset(out, 0, sizeof *out);
    if (layout == ORACLES_UI_LAYOUT_4_3) display_4_3(t, out);
    else display_16_9(t, out);
}

void oracles_ui_layout_mods(OraclesUiLayout layout, const OraclesHomeNav *nav, OraclesUiModsLayout *out)
{
    memset(out, 0, sizeof *out);
    oracles_home_state(nav, oracles_home_hero(nav), out->state);
    if (layout == ORACLES_UI_LAYOUT_4_3) mods_4_3(nav, out);
    else mods_16_9(nav, out);
}

void oracles_ui_display_texts(const OraclesHomeNav *nav, OraclesUiDisplayTexts *t)
{
    const OraclesHomeHero hero = oracles_home_hero(nav);
    memset(t, 0, sizeof *t);
    t->section = oracles_display_section(nav);
    t->advanced = nav->advanced;
    t->over = oracles_home_over(hero);
    t->title = oracles_home_title(hero);
    for (int w = 0; w < 4; w++) oracles_display_window_texts(nav, w, t->window_names[w], t->window_sizes[w], sizeof t->window_sizes[w]);
    t->window_note = oracles_display_window_note;
    oracles_display_reduced(nav, t->window_reduced, sizeof t->window_reduced);
    t->window_one = oracles_display_one_size(nav);
    t->profile_note = oracles_display_profile_note(nav);
    for (int q = 0; q <= ORACLES_DISPLAY_QUALITIES; q++) oracles_display_quality_sets(q, t->quality_sets[q], sizeof t->quality_sets[q]);
    for (int w = 0; w < 4; w++) t->window_size_length[w] = oracles_display_window_size_length(nav, w);
    t->window_fit = t->window_reduced[0] ? t->window_reduced : oracles_ui_window_fit_note;
    oracles_display_quality_text(nav, 0, t->quality_text, sizeof t->quality_text);
    t->quality_note = oracles_display_quality_note(nav, 0);
    t->later = nav->in_game;
    t->explanations[0] = oracles_display_explanation(ORACLES_DISPLAY_COLOUR);
    t->explanations[1] = oracles_display_explanation(ORACLES_DISPLAY_TRANSITIONS);
    t->explanations[2] = oracles_display_explanation(ORACLES_DISPLAY_VSYNC);
    t->explanations[3] = oracles_display_explanation(ORACLES_DISPLAY_CORE);
    t->explanations[4] = oracles_display_explanation(ORACLES_DISPLAY_WORKERS);
    t->view_explanation = oracles_display_explanation(ORACLES_DISPLAY_VIEW);
    for (int i = 0; i < 3; i++) oracles_display_workers_choice(nav, i, t->workers_names[i], sizeof t->workers_names[i]);
    for (int p = 0; p < ORACLES_PROFILES; p++) oracles_profile_size(nav, p, t->profile_sizes[p], sizeof t->profile_sizes[p]);
    for (int v = 0; v < 3; v++) oracles_display_view_size(nav, v, t->view_sizes[v], sizeof t->view_sizes[v]);
    /* The diagram's box has the screen's shape at its height, as the sizes take it: 432 wide for 16:9 (and any wider
     * screen, which the view takes as 16:9), 324 for 4:3. */
    t->diagram_box_w = oracles_display_screen_4_3(nav) ? 324.0f : ORACLES_UI_DIAGRAM_W;
    oracles_display_diagram(nav, t->diagram_box_w, ORACLES_UI_DIAGRAM_H, &t->diagram_w, &t->diagram_h, t->diagram_label, sizeof t->diagram_label);
    /* 4:3's help, one line and a note line, its own short texts so that neither wraps: the profile chosen (or that the
     * game plays in Faithful), Window's with the line on its size or the diagram's, the quality in effect. */
    static const char *const help_4_3[ORACLES_DISPLAY_ROWS] = {
        NULL, NULL, "Whole multiples of the picture: pixels stay sharp.", "How much of the world shows; farther asks more.",
        "On: as the Game Boy Color showed it. Also F2.", "Rooms scroll into one another, swimming too.", "Core, Vsync and neighbor workers.",
        "Accurate: the reference. Fast: lighter.", "Auto: follows a display near 60 Hz.", "Two fill the view faster, on one more core.",
    };
    const unsigned row = nav->row < ORACLES_DISPLAY_ROWS ? nav->row : ORACLES_DISPLAY_PROFILE;
    t->help = help_4_3[row];
    switch (row) {
        case ORACLES_DISPLAY_PROFILE:
            t->help = oracles_display_played_profile(nav) != nav->display.profile ? t->profile_note
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
        case ORACLES_DISPLAY_TRANSITIONS: t->help_note = "Savestates with it on need it on."; break;
        case ORACLES_DISPLAY_CORE: t->help_note = oracles_ui_core_note; break;
        default: break;
    }
}
