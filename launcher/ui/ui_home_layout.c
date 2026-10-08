#include "ui_home_layout.h"

#include "ui_page_layout.h"

#include <stdio.h>
#include <string.h>

/* The home screen's text styles (font, size, letter spacing, capitals). */
const OraclesUiTextStyle oracles_ui_hero_over = { ORACLES_UI_FONT_SERIF, 48.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_hero_title = { ORACLES_UI_FONT_SERIF, 120.0f, 0.01f, 0 };
const OraclesUiTextStyle oracles_ui_hero_state = { ORACLES_UI_FONT_SANS, 24.0f, 0.16f, 1 };
const OraclesUiTextStyle oracles_ui_other_over = { ORACLES_UI_FONT_SERIF, 36.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_other_title = { ORACLES_UI_FONT_SERIF, 72.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_other_state = { ORACLES_UI_FONT_SANS, 22.0f, 0.16f, 1 };
const OraclesUiTextStyle oracles_ui_item_note = { ORACLES_UI_FONT_SANS, 24.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_item_label = { ORACLES_UI_FONT_SERIF, 54.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_hint_key = { ORACLES_UI_FONT_MONO, 19.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_hint_label = { ORACLES_UI_FONT_SANS, 23.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_version = { ORACLES_UI_FONT_SERIF, 22.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_toast = { ORACLES_UI_FONT_SANS, 24.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_slow_path = { ORACLES_UI_FONT_SERIF, 22.0f, 0.14f, 1 };
/* 4:3's, larger for a 640x480 screen. */
const OraclesUiTextStyle oracles_ui_tab_4_3 = { ORACLES_UI_FONT_SERIF, 36.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_hero_over_4_3 = { ORACLES_UI_FONT_SERIF, 40.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_hero_title_4_3 = { ORACLES_UI_FONT_SERIF, 120.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_hero_state_4_3 = { ORACLES_UI_FONT_SANS, 30.0f, 0.14f, 1 };
const OraclesUiTextStyle oracles_ui_item_note_4_3 = { ORACLES_UI_FONT_SANS, 30.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_item_label_4_3 = { ORACLES_UI_FONT_SERIF, 58.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_hint_key_4_3 = { ORACLES_UI_FONT_MONO, 27.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_hint_label_4_3 = { ORACLES_UI_FONT_SANS, 30.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_version_4_3 = { ORACLES_UI_FONT_SERIF, 27.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_toast_4_3 = { ORACLES_UI_FONT_SANS, 30.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_slow_path_4_3 = { ORACLES_UI_FONT_SERIF, 27.0f, 0.14f, 1 };

static const OraclesUiHomeStyles styles_16_9 = {
    &oracles_ui_hero_over, &oracles_ui_hero_title, &oracles_ui_hero_state, &oracles_ui_item_note, &oracles_ui_item_label,
    &oracles_ui_hint_key, &oracles_ui_hint_label, &oracles_ui_version, &oracles_ui_toast, NULL, &oracles_ui_slow_path,
};
static const OraclesUiHomeStyles styles_4_3 = {
    &oracles_ui_hero_over_4_3, &oracles_ui_hero_title_4_3, &oracles_ui_hero_state_4_3, &oracles_ui_item_note_4_3, &oracles_ui_item_label_4_3,
    &oracles_ui_hint_key_4_3, &oracles_ui_hint_label_4_3, &oracles_ui_version_4_3, &oracles_ui_toast_4_3, &oracles_ui_tab_4_3, &oracles_ui_slow_path_4_3,
};

const OraclesUiHomeStyles *oracles_ui_home_styles(OraclesUiLayout layout) { return layout == ORACLES_UI_LAYOUT_4_3 ? &styles_4_3 : &styles_16_9; }

const char oracles_ui_item_dot[] = "\xc2\xb7";

/* The home screen's geometry in a layout's scene, in scene pixels. */
typedef struct metrics {
    float scene_w;
    float top, paused_top;               /* top of the hero's block, at home and in the pause */
    float hero_right, hero_state_margin, hero_title_line;
    float menu_right, menu_bottom, menu_gap;
    float item_min_width, item_min_height, item_pad_y, item_pad_right, item_pad_left, item_gap;
    float label_line;                    /* the items' line height, 0: normal */
    float hints_bottom, hints_gap, hint_min_height, hint_pad_x, hint_pad_y, hint_gap, key_pad_x, key_pad_y;
    float version_left, version_bottom;
    float toast_bottom, toast_pad_x, toast_pad_y;
    float slow_left, slow_bottom, slow_max_width, slow_pad_x, slow_pad_y, slow_gap, slow_line;   /* the slow game's toast */
} metrics;

static const metrics metrics_16_9 = {
    .scene_w = 1920.0f,
    .top = 88.0f, .paused_top = 88.0f,
    .hero_right = 1920.0f - 96.0f, .hero_state_margin = 20.0f, .hero_title_line = 120.0f,
    .menu_right = 1920.0f - 32.0f, .menu_bottom = 1080.0f - 76.0f, .menu_gap = 4.0f,
    .item_min_width = 720.0f, .item_min_height = 0.0f, .item_pad_y = 6.0f, .item_pad_right = 36.0f, .item_pad_left = 40.0f, .item_gap = 28.0f,
    .label_line = 0.0f,
    .hints_bottom = 1080.0f - 18.0f, .hints_gap = 36.0f, .hint_min_height = 0.0f, .hint_pad_x = 10.0f, .hint_pad_y = 4.0f, .hint_gap = 10.0f,
    .key_pad_x = 9.0f, .key_pad_y = 1.0f,
    .version_left = 24.0f, .version_bottom = 1080.0f - 20.0f,
    .toast_bottom = 1080.0f - 78.0f, .toast_pad_x = 26.0f, .toast_pad_y = 14.0f,
    .slow_left = 96.0f, .slow_bottom = 1080.0f - 96.0f, .slow_max_width = 760.0f, .slow_pad_x = 26.0f, .slow_pad_y = 16.0f, .slow_gap = 6.0f,
    .slow_line = 24.0f * 1.35f,
};
/* 4:3: every target at least 90 tall; the hero under the tabs, and higher in the pause, which has none. */
static const metrics metrics_4_3 = {
    .scene_w = 1440.0f,
    .top = 160.0f, .paused_top = 96.0f,
    .hero_right = 1440.0f - 64.0f, .hero_state_margin = 14.0f, .hero_title_line = 120.0f,
    .menu_right = 1440.0f - 36.0f, .menu_bottom = 1080.0f - 110.0f, .menu_gap = 4.0f,
    .item_min_width = 640.0f, .item_min_height = 90.0f, .item_pad_y = 0.0f, .item_pad_right = 28.0f, .item_pad_left = 36.0f, .item_gap = 24.0f,
    .label_line = 58.0f * 1.1f,
    .hints_bottom = 1080.0f - 8.0f, .hints_gap = 16.0f, .hint_min_height = 90.0f, .hint_pad_x = 20.0f, .hint_pad_y = 0.0f, .hint_gap = 12.0f,
    .key_pad_x = 10.0f, .key_pad_y = 0.0f,
    .version_left = 28.0f, .version_bottom = 1080.0f - 38.0f,
    .toast_bottom = 1080.0f - 120.0f, .toast_pad_x = 28.0f, .toast_pad_y = 16.0f,
    .slow_left = 64.0f, .slow_bottom = 1080.0f - 120.0f, .slow_max_width = 640.0f, .slow_pad_x = 28.0f, .slow_pad_y = 18.0f, .slow_gap = 6.0f,
    .slow_line = 30.0f * 1.3f,
};

static const metrics *metrics_of(OraclesUiLayout layout) { return layout == ORACLES_UI_LAYOUT_4_3 ? &metrics_4_3 : &metrics_16_9; }

/* 16:9's stack of the other entries. */
#define OTHERS_LEFT 96.0f
#define OTHERS_GAP 48.0f
#define OTHER_STATE_MARGIN 10.0f
#define OTHER_TITLE_LINE (72.0f * 1.05f)
/* 4:3's tabs: from the top left, 90 tall with a 3 px line along their bottom. */
#define TABS_LEFT 64.0f
#define TABS_TOP 28.0f
#define TABS_GAP 12.0f
#define TAB_PAD_X 24.0f
#define TAB_HEIGHT 90.0f
#define TAB_LINE 3.0f
#define REASON_PAD_X 10.0f
#define REASON_PAD_Y 2.0f
#define KEY_BORDER 1.0f
#define TOAST_BORDER 1.0f

/* A line of `text` whose box starts at (x, y), in a block of `line_height` (0: normal). */
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

/* A line centred on `middle` in a flex row, as an inline element's box, in a block of `line_height` (0: normal); an
 * empty one has no height. */
static OraclesUiLine line_centred(const OraclesUiTextStyle *style, const char *text, float x, float middle, float line_height)
{
    OraclesUiLine line = line_at(style, text, x, 0.0f, line_height);
    const float offset = line.baseline;
    if (!text || !text[0]) line.h = 0.0f;
    line.y = middle - line.h * 0.5f;
    line.baseline = line.y + offset;
    return line;
}

void oracles_ui_layout_hero(OraclesUiLayout layout, int paused, const char *over, const char *title, const char *state, OraclesUiTitleLayout *out)
{
    const metrics *m = metrics_of(layout);
    const OraclesUiHomeStyles *st = oracles_ui_home_styles(layout);
    const float top = paused ? m->paused_top : m->top;
    /* text-align: right; the widths carry the trailing letter spacing, as the browser's do. */
    out->over = line_at(st->hero_over, over, 0.0f, top, 0.0f);
    out->title = line_at(st->hero_title, title, 0.0f, out->over.y + out->over.h, m->hero_title_line);
    out->state = line_at(st->hero_state, state, 0.0f, out->title.y + out->title.h + m->hero_state_margin, 0.0f);
    OraclesUiLine *lines[3] = { &out->over, &out->title, &out->state };
    float width = 0.0f;
    for (int i = 0; i < 3; i++) {
        lines[i]->x = m->hero_right - lines[i]->w;
        if (lines[i]->w > width) width = lines[i]->w;
    }
    out->box.x = m->hero_right - width;
    out->box.y = top;
    out->box.w = width;
    out->box.h = out->state.y + out->state.h - top;
}

void oracles_ui_layout_others(const char *const over[2], const char *const title[2], const char *const state[2], OraclesUiTitleLayout out[2])
{
    float y = metrics_16_9.top;
    for (int i = 0; i < 2; i++) {
        OraclesUiTitleLayout *o = &out[i];
        o->over = line_at(&oracles_ui_other_over, over[i], OTHERS_LEFT, y, 0.0f);
        o->title = line_at(&oracles_ui_other_title, title[i], OTHERS_LEFT, o->over.y + o->over.h, OTHER_TITLE_LINE);
        o->state = line_at(&oracles_ui_other_state, state[i], OTHERS_LEFT, o->title.y + o->title.h + OTHER_STATE_MARGIN, 0.0f);
        float width = o->over.w;
        if (o->title.w > width) width = o->title.w;
        if (o->state.w > width) width = o->state.w;
        o->box.x = OTHERS_LEFT;
        o->box.y = y;
        o->box.w = width;
        o->box.h = o->state.y + o->state.h - y;
        y += o->box.h + OTHERS_GAP;
    }
}

void oracles_ui_layout_tabs(const char *const labels[ORACLES_HOME_ENTRIES], OraclesUiTabLayout out[ORACLES_HOME_ENTRIES])
{
    float x = TABS_LEFT;
    for (int i = 0; i < ORACLES_HOME_ENTRIES; i++) {
        OraclesUiTabLayout *t = &out[i];
        /* The label centred above the line, in the box's 87 px. */
        t->label = line_at(&oracles_ui_tab_4_3, labels[i], x + TAB_PAD_X, 0.0f, 0.0f);
        const float offset = t->label.baseline;
        t->label.y = TABS_TOP + (TAB_HEIGHT - TAB_LINE - t->label.h) * 0.5f;
        t->label.baseline = t->label.y + offset;
        t->box.x = x;
        t->box.y = TABS_TOP;
        t->box.w = TAB_PAD_X + t->label.w + TAB_PAD_X;
        t->box.h = TAB_HEIGHT;
        t->line.x = x;
        t->line.y = TABS_TOP + TAB_HEIGHT - TAB_LINE;
        t->line.w = t->box.w;
        t->line.h = TAB_LINE;
        x += t->box.w + TABS_GAP;
    }
}

void oracles_ui_layout_menu(OraclesUiLayout layout, const char *const *notes, const int *reasons, const char *const *labels, unsigned count, OraclesUiItemLayout *out)
{
    const metrics *m = metrics_of(layout);
    const OraclesUiHomeStyles *st = oracles_ui_home_styles(layout);
    float label_height, baseline;
    oracles_ui_line_box(st->item_label, m->label_line, &label_height, &baseline);
    /* The serif's line is the tallest of the row; 4:3's rows are 90 tall at least. */
    float height = m->item_pad_y * 2.0f + label_height;
    if (height < m->item_min_height) height = m->item_min_height;
    float y = m->menu_bottom - (float)count * height - (float)(count ? count - 1u : 0u) * m->menu_gap;
    for (unsigned i = 0; i < count; i++, y += height + m->menu_gap) {
        OraclesUiItemLayout *item = &out[i];
        const float middle = y + height * 0.5f;
        item->dot = line_centred(st->item_label, oracles_ui_item_dot, 0.0f, middle, m->label_line);
        item->label = line_centred(st->item_label, labels[i], 0.0f, middle, m->label_line);
        item->note = line_centred(st->item_note, notes[i], 0.0f, middle, 0.0f);
        const float pad = reasons && reasons[i] && item->note.w > 0.0f ? REASON_PAD_X : 0.0f;
        /* justify-content: flex-end, the empty note still a flex item between its two gaps. */
        item->dot.x = m->menu_right - m->item_pad_right - item->dot.w;
        item->label.x = item->dot.x - m->item_gap - item->label.w;
        item->note.x = item->label.x - m->item_gap - pad - item->note.w;
        memset(&item->note_box, 0, sizeof item->note_box);
        if (pad > 0.0f) {
            item->note_box.x = item->note.x - pad;
            item->note_box.w = item->note.w + 2.0f * pad;
            item->note_box.h = item->note.h + 2.0f * REASON_PAD_Y;
            item->note_box.y = middle - item->note_box.h * 0.5f;
        }
        float width = m->item_pad_left + item->note.w + 2.0f * pad + m->item_gap + item->label.w + m->item_gap + item->dot.w + m->item_pad_right;
        if (width < m->item_min_width) width = m->item_min_width;
        item->box.x = m->menu_right - width;
        item->box.y = y;
        item->box.w = width;
        item->box.h = height;
    }
}

void oracles_ui_layout_hints(OraclesUiLayout layout, const OraclesHomeHint *hints, unsigned count, OraclesUiHintLayout *out)
{
    const metrics *m = metrics_of(layout);
    const OraclesUiHomeStyles *st = oracles_ui_home_styles(layout);
    float total = 0.0f;
    for (unsigned i = 0; i < count; i++) {
        OraclesUiHintLayout *h = &out[i];
        h->key = line_at(st->hint_key, hints[i].key, 0.0f, 0.0f, 0.0f);
        h->label = line_at(st->hint_label, hints[i].label, 0.0f, 0.0f, 0.0f);
        h->key_box.w = h->key.w + 2.0f * (m->key_pad_x + KEY_BORDER);
        h->key_box.h = h->key.h + 2.0f * (m->key_pad_y + KEY_BORDER);
        const float inner = h->key_box.h > h->label.h ? h->key_box.h : h->label.h;
        h->box.w = m->hint_pad_x + h->key_box.w + m->hint_gap + h->label.w + m->hint_pad_x;
        h->box.h = m->hint_pad_y * 2.0f + inner;
        if (h->box.h < m->hint_min_height) h->box.h = m->hint_min_height;
        total += h->box.w + (i ? m->hints_gap : 0.0f);
    }
    float x = (m->scene_w - total) * 0.5f;
    for (unsigned i = 0; i < count; i++) {
        OraclesUiHintLayout *h = &out[i];
        h->box.x = x;
        h->box.y = m->hints_bottom - h->box.h;
        const float middle = h->box.y + h->box.h * 0.5f;
        h->key_box.x = x + m->hint_pad_x;
        h->key_box.y = middle - h->key_box.h * 0.5f;
        const float key_baseline = h->key.baseline, label_baseline = h->label.baseline;
        h->key.x = h->key_box.x + KEY_BORDER + m->key_pad_x;
        h->key.y = h->key_box.y + KEY_BORDER + m->key_pad_y;
        h->key.baseline = h->key.y + key_baseline;
        h->label.x = h->key_box.x + h->key_box.w + m->hint_gap;
        h->label.y = middle - h->label.h * 0.5f;
        h->label.baseline = h->label.y + label_baseline;
        x += h->box.w + m->hints_gap;
    }
}

void oracles_ui_layout_version(OraclesUiLayout layout, const char *text, OraclesUiLine *out)
{
    const metrics *m = metrics_of(layout);
    *out = line_at(oracles_ui_home_styles(layout)->version, text, m->version_left, 0.0f, 0.0f);
    out->y = m->version_bottom - out->h;
    out->baseline += out->y;
}

void oracles_ui_layout_toast(OraclesUiLayout layout, const char *text, OraclesUiToastLayout *out)
{
    const metrics *m = metrics_of(layout);
    out->text = line_at(oracles_ui_home_styles(layout)->toast, text, 0.0f, 0.0f, 0.0f);
    out->box.w = out->text.w + 2.0f * (m->toast_pad_x + TOAST_BORDER);
    out->box.h = out->text.h + 2.0f * (m->toast_pad_y + TOAST_BORDER);
    out->box.x = m->scene_w * 0.5f - out->box.w * 0.5f;
    out->box.y = m->toast_bottom - out->box.h;
    out->text.x = out->box.x + TOAST_BORDER + m->toast_pad_x;
    out->text.y = out->box.y + TOAST_BORDER + m->toast_pad_y;
    out->text.baseline += out->text.y;
}

void oracles_ui_layout_slow(OraclesUiLayout layout, const char *text, const char *path, OraclesUiSlowLayout *out)
{
    const metrics *m = metrics_of(layout);
    const OraclesUiHomeStyles *st = oracles_ui_home_styles(layout);
    memset(out, 0, sizeof *out);
    /* As wide as its longer text, up to its widest, where the text wraps. */
    const float frame = 2.0f * (m->slow_pad_x + TOAST_BORDER);
    float inner = oracles_ui_text_width(st->toast, text);
    const float path_w = oracles_ui_text_width(st->slow_path, path);
    if (path_w > inner) inner = path_w;
    if (inner > m->slow_max_width - frame) inner = m->slow_max_width - frame;
    OraclesUiWrapped wrapped, pretty;
    oracles_ui_wrap(st->toast, text, inner, 0.0f, 0.0f, &wrapped);
    /* text-wrap: pretty, as the browser applies it here: a last line of a single word takes the word before it. */
    if (wrapped.count >= 2 && !strchr(wrapped.text[wrapped.count - 1], ' ')) {
        oracles_ui_wrap(st->toast, text, wrapped.lines[wrapped.count - 2].w - 1.0f, 0.0f, 0.0f, &pretty);
        if (pretty.count == wrapped.count) wrapped = pretty;
    }
    const float left = m->slow_left + TOAST_BORDER + m->slow_pad_x;
    float y = 0.0f;
    for (unsigned i = 0; i < wrapped.count && i < ORACLES_UI_SLOW_LINES; i++) {
        snprintf(out->text[i], sizeof out->text[i], "%s", wrapped.text[i]);
        out->text_lines[i] = line_at(st->toast, out->text[i], left, y, m->slow_line);
        y += out->text_lines[i].h;
        out->lines++;
    }
    out->path = line_at(st->slow_path, path, left, y + m->slow_gap, 0.0f);
    out->box.w = inner + frame;
    out->box.h = out->path.y + out->path.h + 2.0f * (m->slow_pad_y + TOAST_BORDER);
    out->box.x = m->slow_left;
    out->box.y = m->slow_bottom - out->box.h;
    /* The lines laid out from 0, moved under the box's top. */
    const float top = out->box.y + TOAST_BORDER + m->slow_pad_y;
    for (unsigned i = 0; i < out->lines; i++) { out->text_lines[i].y += top; out->text_lines[i].baseline += top; }
    out->path.y += top;
    out->path.baseline += top;
}
