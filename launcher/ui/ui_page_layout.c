#include "ui_page_layout.h"

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

const char oracles_ui_transitions_note[] = "A savestate taken with this on is refused with it off.";
const char oracles_ui_core_note[] = "Save states keep their core.";
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
#define PROFILE_NOTE_GAP 10.0f
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
#define DIAGRAM_MARGIN 56.0f
#define DISPLAY_PAGE_NOTE_GAP 20.0f   /* the note under Display's title, opened from a game */
#define DIAGRAM_LABEL_GAP 12.0f
#define DIAGRAM_BORDER 1.0f
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

void oracles_ui_layout_page_head(const char *section, const char *over, const char *title, const char *state, OraclesUiPageHead *out)
{
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

/* Options framed with a name and a size (profiles, windows), in a line from (x, y). Returns their height. */
static float framed(const char *const *names, const char *const *sizes, unsigned count, float x, float y, OraclesUiOptionLayout *out)
{
    float height = 0.0f;
    for (unsigned i = 0; i < count; i++) {
        OraclesUiOptionLayout *o = &out[i];
        o->name = line_at(&oracles_ui_option_name, names[i], x + BORDER + OPTION_PAD_X, y + BORDER + OPTION_PAD_Y, 0.0f);
        o->size = line_at(&oracles_ui_option_size, sizes[i], o->name.x, o->name.y + o->name.h + OPTION_LINE_GAP, 0.0f);
        const float inner = o->name.w > o->size.w ? o->name.w : o->size.w;
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

void oracles_ui_layout_game(OraclesUiGameTexts *t, OraclesUiGameLayout *out)
{
    memset(out, 0, sizeof *out);
    oracles_ui_layout_page_head("Cartridge", t->over, t->title, t->state, &out->head);
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

void oracles_ui_wrap(const OraclesUiTextStyle *style, const char *text, float width, float x, float y, OraclesUiWrapped *out)
{
    memset(out, 0, sizeof *out);
    float height, baseline;
    oracles_ui_line_box(style, 0.0f, &height, &baseline);
    const char *p = text ? text : "";
    char line[ORACLES_UI_WRAP_LENGTH] = "";
    while (*p && out->count < ORACLES_UI_WRAP_LINES) {
        /* The next word, and the line with it if it fits; a line takes one word at least. */
        const size_t word = strcspn(p, " ");
        char candidate[ORACLES_UI_WRAP_LENGTH * 2];
        snprintf(candidate, sizeof candidate, "%s%s%.*s", line, line[0] ? " " : "", (int)word, p);
        const int last = out->count == ORACLES_UI_WRAP_LINES - 1;
        if (line[0] && !last && oracles_ui_text_width(style, candidate) > width) {
            snprintf(out->text[out->count++], ORACLES_UI_WRAP_LENGTH, "%s", line);
            snprintf(line, sizeof line, "%.*s", (int)word, p);
        } else {
            copy_text(line, sizeof line, candidate);
        }
        p += word;
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

void oracles_ui_layout_display(const OraclesUiDisplayTexts *t, OraclesUiDisplayLayout *out)
{
    memset(out, 0, sizeof *out);
    oracles_ui_layout_page_head("Display", t->over, t->title, NULL, &out->head);
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

    const float left = PANEL_X + PANEL_BORDER + PANEL_PAD_X, width = PANEL_W - 2.0f * (PANEL_BORDER + PANEL_PAD_X);
    const float value_x = left + DISPLAY_LABEL_W + GRID_GAP, value_w = width - DISPLAY_LABEL_W - GRID_GAP;
    const float inner_x = value_x + ROW_PAD_X;
    float y = PANEL_Y + PANEL_BORDER + PANEL_PAD_Y;

    /* Profile: the two profiles framed, each with its surface, and the chosen one's note. */
    oracles_ui_wrap(&oracles_ui_row_label, oracles_display_labels[ORACLES_DISPLAY_PROFILE], DISPLAY_LABEL_W, left, y + LABEL_PAD_TOP,
                    &out->labels[ORACLES_DISPLAY_PROFILE]);
    float inner_y = y + DISPLAY_ROW_PAD_Y;
    const char *profile_sizes[ORACLES_PROFILES];
    for (int p = 0; p < ORACLES_PROFILES; p++) profile_sizes[p] = t->profile_sizes[p];
    float options_h = framed(oracles_profile_names, profile_sizes, ORACLES_PROFILES, inner_x, inner_y, out->profiles);
    out->profile_note = line_at(&oracles_ui_row_text, t->profile_note, inner_x, inner_y + options_h + PROFILE_NOTE_GAP, 0.0f);
    float bottom = out->profile_note.y + out->profile_note.h;
    out->rows[ORACLES_DISPLAY_PROFILE] = box(value_x, y, value_w, bottom + DISPLAY_ROW_PAD_Y - y);
    y += out->rows[ORACLES_DISPLAY_PROFILE].h + DISPLAY_ROW_GAP;

    /* Window: the four choices framed, each with its size, the note, and the line that says a window is reduced. */
    oracles_ui_wrap(&oracles_ui_row_label, oracles_display_labels[ORACLES_DISPLAY_WINDOW], DISPLAY_LABEL_W, left, y + LABEL_PAD_TOP,
                    &out->labels[ORACLES_DISPLAY_WINDOW]);
    inner_y = y + DISPLAY_ROW_PAD_Y;
    const char *names[4], *sizes[4];
    for (int i = 0; i < 4; i++) { names[i] = t->window_names[i]; sizes[i] = t->window_sizes[i]; }
    options_h = framed(names, sizes, 4, inner_x, inner_y, out->windows);
    out->window_note = line_at(&oracles_ui_row_text, t->window_note, inner_x, inner_y + options_h + DISPLAY_WINDOW_GAP, 0.0f);
    bottom = out->window_note.y + out->window_note.h;
    if (t->window_reduced[0]) {
        out->window_reduced = line_at(&oracles_ui_row_text, t->window_reduced, inner_x, bottom + DISPLAY_WINDOW_GAP, 0.0f);
        bottom = out->window_reduced.y + out->window_reduced.h;
    }
    out->rows[ORACLES_DISPLAY_WINDOW] = box(value_x, y, value_w, bottom + DISPLAY_ROW_PAD_Y - y);
    y += out->rows[ORACLES_DISPLAY_WINDOW].h + DISPLAY_ROW_GAP;

    /* View (each level with its size), color correction, continuous transitions (with the note on savestates), vsync,
     * core (with its note on savestates): an explanation and choices. */
    const char *const transitions_notes[1] = { oracles_ui_transitions_note }, *const core_notes[1] = { oracles_ui_core_note };
    OraclesUiLine *const transitions_out[1] = { &out->transitions_note }, *const core_out[1] = { &out->core_note };
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
                     out->transitions, &out->rows[ORACLES_DISPLAY_TRANSITIONS]) + DISPLAY_ROW_GAP;
    y += display_row(value_x, y, value_w, oracles_display_labels[ORACLES_DISPLAY_VSYNC], left, t->explanations[2], NULL, 0,
                     oracles_display_vsync_choices, NULL, 3, &out->labels[ORACLES_DISPLAY_VSYNC], &out->explanations[2], NULL, out->vsync,
                     &out->rows[ORACLES_DISPLAY_VSYNC]) + DISPLAY_ROW_GAP;
    y += display_row(value_x, y, value_w, oracles_display_labels[ORACLES_DISPLAY_CORE], left, t->explanations[3], core_notes, 1,
                     oracles_display_core_choices, NULL, 2, &out->labels[ORACLES_DISPLAY_CORE], &out->explanations[3], core_out, out->core,
                     &out->rows[ORACLES_DISPLAY_CORE]);
    out->panel = box(PANEL_X, PANEL_Y, PANEL_W, y + PANEL_PAD_Y + PANEL_BORDER - PANEL_Y);
}

/* A text wrapped at `width` in lines of `line_height`, the first at (x, y). */
static void wrap_lines(const OraclesUiTextStyle *style, const char *text, float width, float x, float y, float line_height, OraclesUiWrapped *out)
{
    oracles_ui_wrap(style, text, width, x, y, out);
    for (unsigned i = 0; i < out->count; i++) out->lines[i] = line_at(style, out->text[i], x, y + (float)i * line_height, line_height);
    out->h = (float)out->count * line_height;
}

/* The first mod the list shows: all from the first while they fit, else the highlighted one among the last shown. */
static unsigned first_shown(const OraclesHomeNav *nav, unsigned count)
{
    if (count <= ORACLES_UI_MODS_SHOWN) return 0;
    const unsigned row = nav->row, play = oracles_mods_row_play(nav);
    const unsigned focused = row == ORACLES_MODS_ROW_FOLDER ? 0u : row >= play ? count - 1u : row - 1u;
    return focused < ORACLES_UI_MODS_SHOWN ? 0u : focused - (ORACLES_UI_MODS_SHOWN - 1u);
}

void oracles_ui_layout_mods(const OraclesHomeNav *nav, OraclesUiModsLayout *out)
{
    memset(out, 0, sizeof *out);
    const OraclesHomeHero hero = oracles_home_hero(nav);
    oracles_home_state(nav, hero, out->state);
    oracles_ui_layout_page_head("Mods", oracles_home_over(hero), oracles_home_title(hero), out->state, &out->head);
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
    const unsigned first = first_shown(nav, mods->count);
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
