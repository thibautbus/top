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
const OraclesUiTextStyle oracles_ui_menu_line = { ORACLES_UI_FONT_SANS, 22.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_hero_credit = { ORACLES_UI_FONT_SANS, 24.0f, 0.0f, 0 };
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
const OraclesUiTextStyle oracles_ui_pad_toast_4_3 = { ORACLES_UI_FONT_SANS, 36.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_slow_path_4_3 = { ORACLES_UI_FONT_SERIF, 27.0f, 0.14f, 1 };
const OraclesUiTextStyle oracles_ui_menu_line_4_3 = { ORACLES_UI_FONT_SANS, 30.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_hero_credit_4_3 = { ORACLES_UI_FONT_SANS, 27.0f, 0.0f, 0 };

static const OraclesUiHomeStyles styles_16_9 = {
    &oracles_ui_hero_over, &oracles_ui_hero_title, &oracles_ui_hero_state, &oracles_ui_item_note, &oracles_ui_item_label,
    &oracles_ui_hint_key, &oracles_ui_hint_label, &oracles_ui_version, &oracles_ui_toast, NULL, &oracles_ui_slow_path, &oracles_ui_toast,
    &oracles_ui_menu_line,
};
static const OraclesUiHomeStyles styles_4_3 = {
    &oracles_ui_hero_over_4_3, &oracles_ui_hero_title_4_3, &oracles_ui_hero_state_4_3, &oracles_ui_item_note_4_3, &oracles_ui_item_label_4_3,
    &oracles_ui_hint_key_4_3, &oracles_ui_hint_label_4_3, &oracles_ui_version_4_3, &oracles_ui_toast_4_3, &oracles_ui_tab_4_3, &oracles_ui_slow_path_4_3,
    &oracles_ui_pad_toast_4_3, &oracles_ui_menu_line_4_3,
};

const OraclesUiHomeStyles *oracles_ui_home_styles(OraclesUiLayout layout) { return layout == ORACLES_UI_LAYOUT_4_3 ? &styles_4_3 : &styles_16_9; }

const char oracles_ui_item_dot[] = "\xc2\xb7";
const char oracles_ui_settings_label[] = "Settings";
const char oracles_ui_hotkeys_line_lead[] = "Item hotkeys are off \xc2\xb7 ";
const char oracles_ui_hotkeys_line_path[] = "Controls \xe2\x80\xba Item hotkeys";

/* The home screen's geometry in a layout's scene, in scene pixels. */
typedef struct metrics {
    float scene_w;
    float top, paused_top;               /* top of the hero's block, at home and in the pause */
    float hero_right, hero_title_margin, hero_state_margin, hero_title_line;
    int hero_over;                       /* the words above the title, on their line; 4:3's tab carries them */
    float menu_right, menu_bottom, menu_gap;
    float menu_line_pad;                 /* above the pause menu's line under its list */
    float item_min_width, item_min_height, item_pad_y, item_pad_right, item_pad_left, item_gap;
    float label_line;                    /* the items' line height, 0: normal */
    float hints_bottom, hints_gap, hint_min_height, hint_pad_x, hint_pad_y, hint_gap, key_pad_x, key_pad_y;
    float version_left, version_bottom;
    /* The home screen's Settings button and version: their line's left and bottom, the gap between the two, their line
     * height; the button's padding and border, the version's padding; how far the button's pointer target reaches past
     * its frame (left, top, right, bottom), to the corner in 16:9.  The newer version's notice: above them (4:3),
     * `update_gap` over their line, or after the version on it (16:9), `update_overlap` into the gap; its padding and
     * border. */
    float settings_left, settings_bottom, settings_gap, settings_line, button_pad_x, button_pad_y, button_border, version_pad_x, version_pad_y;
    float settings_hit[4];
    int update_above;
    float update_gap, update_overlap, update_pad_x, update_pad_y, update_border;
    /* The home screen's help bar in 4:3: centred right of `home_hints_left`, its hints `home_hints_gap` apart. */
    float home_hints_left, home_hints_gap;
    float toast_bottom, toast_pad_x, toast_pad_y;
    float slow_left, slow_bottom, slow_max_width, slow_pad_x, slow_pad_y, slow_gap, slow_line;   /* the slow game's toast */
    float pad_toast_gap, pad_toast_pad_x, pad_toast_pad_y, pad_toast_min_h;   /* the reminder over a game, under its status bar */
    float notice_max_width;   /* a notice over a game, as the reminder, wrapped within it */
} metrics;

static const metrics metrics_16_9 = {
    .scene_w = 1920.0f,
    .top = 88.0f, .paused_top = 88.0f,
    .hero_right = 1920.0f - 96.0f, .hero_title_margin = 16.0f, .hero_state_margin = 20.0f, .hero_title_line = 120.0f, .hero_over = 1,
    .menu_right = 1920.0f - 32.0f, .menu_bottom = 1080.0f - 76.0f, .menu_gap = 4.0f, .menu_line_pad = 6.0f,
    .item_min_width = 720.0f, .item_min_height = 0.0f, .item_pad_y = 6.0f, .item_pad_right = 36.0f, .item_pad_left = 40.0f, .item_gap = 28.0f,
    .label_line = 0.0f,
    .hints_bottom = 1080.0f - 18.0f, .hints_gap = 36.0f, .hint_min_height = 0.0f, .hint_pad_x = 10.0f, .hint_pad_y = 4.0f, .hint_gap = 10.0f,
    .key_pad_x = 9.0f, .key_pad_y = 1.0f,
    .version_left = 24.0f, .version_bottom = 1080.0f - 20.0f,
    .settings_left = 12.0f, .settings_bottom = 1080.0f - 12.0f, .settings_gap = 10.0f, .settings_line = 28.0f, .button_pad_x = 12.0f,
    .button_pad_y = 3.0f, .button_border = 1.0f, .version_pad_x = 2.0f, .version_pad_y = 4.0f, .settings_hit = { 12.0f, 16.0f, 3.0f, 13.0f },
    .update_above = 0, .update_gap = 0.0f, .update_overlap = 12.0f, .update_pad_x = 12.0f, .update_pad_y = 4.0f, .update_border = 1.0f,
    .home_hints_left = 0.0f, .home_hints_gap = 36.0f,
    .toast_bottom = 1080.0f - 78.0f, .toast_pad_x = 26.0f, .toast_pad_y = 14.0f,
    .slow_left = 96.0f, .slow_bottom = 1080.0f - 96.0f, .slow_max_width = 760.0f, .slow_pad_x = 26.0f, .slow_pad_y = 16.0f, .slow_gap = 6.0f,
    .slow_line = 24.0f * 1.35f,
    .pad_toast_gap = 16.0f, .pad_toast_pad_x = 26.0f, .pad_toast_pad_y = 14.0f, .pad_toast_min_h = 0.0f,
    .notice_max_width = 1200.0f,
};
/* 4:3: every target at least 90 tall; the hero's title alone under the tabs, and higher in the pause, which has none,
 * so that seven entries fit under it. */
static const metrics metrics_4_3 = {
    .scene_w = 1440.0f,
    .top = 124.0f, .paused_top = 96.0f,
    .hero_right = 1440.0f - 64.0f, .hero_title_margin = 0.0f, .hero_state_margin = 14.0f, .hero_title_line = 120.0f, .hero_over = 0,
    .menu_right = 1440.0f - 36.0f, .menu_bottom = 1080.0f - 110.0f, .menu_gap = 4.0f, .menu_line_pad = 4.0f,
    .item_min_width = 640.0f, .item_min_height = 90.0f, .item_pad_y = 0.0f, .item_pad_right = 28.0f, .item_pad_left = 36.0f, .item_gap = 24.0f,
    .label_line = 58.0f * 1.1f,
    .hints_bottom = 1080.0f - 8.0f, .hints_gap = 16.0f, .hint_min_height = 90.0f, .hint_pad_x = 20.0f, .hint_pad_y = 0.0f, .hint_gap = 12.0f,
    .key_pad_x = 10.0f, .key_pad_y = 0.0f,
    .version_left = 28.0f, .version_bottom = 1080.0f - 38.0f,
    .settings_left = 14.0f, .settings_bottom = 1080.0f - 28.0f, .settings_gap = 12.0f, .settings_line = 34.0f, .button_pad_x = 14.0f,
    .button_pad_y = 3.0f, .button_border = 2.0f, .version_pad_x = 2.0f, .version_pad_y = 5.0f, .settings_hit = { 0.0f, 0.0f, 0.0f, 0.0f },
    .update_above = 1, .update_gap = 4.0f, .update_overlap = 0.0f, .update_pad_x = 14.0f, .update_pad_y = 5.0f, .update_border = 2.0f,
    .home_hints_left = 250.0f, .home_hints_gap = 12.0f,
    .toast_bottom = 1080.0f - 120.0f, .toast_pad_x = 28.0f, .toast_pad_y = 16.0f,
    .slow_left = 64.0f, .slow_bottom = 1080.0f - 120.0f, .slow_max_width = 640.0f, .slow_pad_x = 28.0f, .slow_pad_y = 18.0f, .slow_gap = 6.0f,
    .slow_line = 30.0f * 1.3f,
    .pad_toast_gap = 12.0f, .pad_toast_pad_x = 30.0f, .pad_toast_pad_y = 0.0f, .pad_toast_min_h = 90.0f,
    .notice_max_width = 1240.0f,
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
#define CREDIT_MARGIN 10.0f        /* 16:9: the credit under the state */
#define CREDIT_GAP_4_3 40.0f       /* 4:3: the credit before the state, on its line */
#define CREDIT_UNDER_4_3 6.0f      /* 4:3 under eight entries' title: the credit under the state */
#define CROWDED_ITEM_4_3 86.0f     /* 4:3: an item of the home menu of eight entries */

/* A line of `text` whose box starts at (x, y), in a block of `line_height` (0: normal). */
static OraclesUiLine line_at(const OraclesUiTextStyle *style, const char *text, float x, float y, float line_height)
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
    oracles_ui_layout_hero_credited(layout, paused, over, title, state, NULL, out);
}

void oracles_ui_layout_hero_credited(OraclesUiLayout layout, int paused, const char *over, const char *title, const char *state, const char *credit,
                                     OraclesUiTitleLayout *out)
{
    const metrics *m = metrics_of(layout);
    const OraclesUiHomeStyles *st = oracles_ui_home_styles(layout);
    const float top = paused ? m->paused_top : m->top;
    /* text-align: right; the widths carry the trailing letter spacing, as the browser's do.  Without the words above
     * it, the title starts the block. */
    out->over = line_at(st->hero_over, m->hero_over ? over : NULL, 0.0f, top, 0.0f);
    if (!m->hero_over) out->over.h = 0.0f;
    out->title = line_at(st->hero_title, title, 0.0f, out->over.y + out->over.h + m->hero_title_margin, m->hero_title_line);
    out->state = line_at(st->hero_state, state, 0.0f, out->title.y + out->title.h + m->hero_state_margin, 0.0f);
    memset(&out->credit, 0, sizeof out->credit);
    const int credited = credit && credit[0], four_three = layout == ORACLES_UI_LAYOUT_4_3;
    if (credited && !four_three) out->credit = line_at(&oracles_ui_hero_credit, credit, 0.0f, out->state.y + out->state.h + CREDIT_MARGIN, 0.0f);
    OraclesUiLine *lines[4] = { &out->over, &out->title, &out->state, &out->credit };
    float left = m->hero_right, bottom = out->state.y + out->state.h;
    for (int i = 0; i < 4; i++) {
        if (i == 3 && !credited) continue;
        lines[i]->x = m->hero_right - lines[i]->w;
        if (lines[i]->x < left) left = lines[i]->x;
    }
    if (credited && four_three) {
        /* On the state's line, its baseline the state's: the state's taller line keeps the line's height. */
        out->credit = line_at(&oracles_ui_hero_credit_4_3, credit, 0.0f, 0.0f, 0.0f);
        const float ascent = out->credit.baseline;
        out->credit.x = out->state.x - CREDIT_GAP_4_3 - out->credit.w;
        out->credit.baseline = out->state.baseline;
        out->credit.y = out->credit.baseline - ascent;
        if (out->credit.x < left) left = out->credit.x;
    }
    if (credited && out->credit.y + out->credit.h > bottom) bottom = out->credit.y + out->credit.h;
    out->box.x = left;
    out->box.y = top;
    out->box.w = m->hero_right - left;
    out->box.h = bottom - top;
}

void oracles_ui_layout_hero_left(OraclesUiLayout layout, OraclesUiTitleLayout *out)
{
    const metrics *m = metrics_of(layout);
    const float left = oracles_ui_scene_width(layout) - m->hero_right;   /* as far from the left as from the right */
    OraclesUiLine *lines[4] = { &out->over, &out->title, &out->state, &out->credit };
    for (int i = 0; i < 4; i++) lines[i]->x = left;
    out->box.x = left;
}

void oracles_ui_layout_hero_crowded(OraclesUiLayout layout, const char *over, const char *title, const char *state, const char *credit, OraclesUiTitleLayout *out)
{
    const int credited = credit && credit[0];
    oracles_ui_layout_hero_credited(layout, 0, over, title, state, NULL, out);
    if (layout == ORACLES_UI_LAYOUT_4_3) {
        oracles_ui_layout_hero_left(layout, out);
        if (!credited) return;
        out->credit = line_at(&oracles_ui_hero_credit_4_3, credit, out->state.x, out->state.y + out->state.h + CREDIT_UNDER_4_3, 0.0f);
        out->box.h = out->credit.y + out->credit.h - out->box.y;
        return;
    }
    if (!credited) return;
    /* Before the state, on its baseline. */
    out->credit = line_at(&oracles_ui_hero_credit, credit, 0.0f, 0.0f, 0.0f);
    const float ascent = out->credit.baseline;
    out->credit.x = out->state.x - CREDIT_GAP_4_3 - out->credit.w;
    out->credit.baseline = out->state.baseline;
    out->credit.y = out->credit.baseline - ascent;
    if (out->credit.x < out->box.x) {
        out->box.w += out->box.x - out->credit.x;
        out->box.x = out->credit.x;
    }
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

/* The menu, its items `least` tall at least. */
static void menu_at(OraclesUiLayout layout, float least, const char *const *notes, const int *reasons, const char *const *labels, unsigned count,
                    OraclesUiMenuLineLayout *line, OraclesUiItemLayout *out)
{
    const metrics *m = metrics_of(layout);
    const OraclesUiHomeStyles *st = oracles_ui_home_styles(layout);
    float label_height, baseline;
    oracles_ui_line_box(st->item_label, m->label_line, &label_height, &baseline);
    /* The serif's line is the tallest of the row; 4:3's rows are 90 tall at least. */
    float height = m->item_pad_y * 2.0f + label_height;
    if (height < least) height = least;
    float bottom = m->menu_bottom;
    if (line) {
        /* The last of the column, its text on one line ending where the items' padding begins. */
        const OraclesUiLine lead = line_at(st->menu_line, oracles_ui_hotkeys_line_lead, 0.0f, 0.0f, 0.0f);
        const OraclesUiLine path = line_at(st->menu_line, oracles_ui_hotkeys_line_path, 0.0f, 0.0f, 0.0f);
        line->box.w = lead.w + path.w + m->item_pad_right;
        line->box.h = m->menu_line_pad + lead.h;
        line->box.x = m->menu_right - line->box.w;
        line->box.y = m->menu_bottom - line->box.h;
        line->lead = line_at(st->menu_line, oracles_ui_hotkeys_line_lead, line->box.x, line->box.y + m->menu_line_pad, 0.0f);
        line->path = line_at(st->menu_line, oracles_ui_hotkeys_line_path, line->lead.x + line->lead.w, line->lead.y, 0.0f);
        bottom = line->box.y - m->menu_gap;
    }
    float y = bottom - (float)count * height - (float)(count ? count - 1u : 0u) * m->menu_gap;
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

void oracles_ui_layout_menu(OraclesUiLayout layout, const char *const *notes, const int *reasons, const char *const *labels, unsigned count,
                            OraclesUiMenuLineLayout *line, OraclesUiItemLayout *out)
{
    menu_at(layout, metrics_of(layout)->item_min_height, notes, reasons, labels, count, line, out);
}

void oracles_ui_layout_home_menu(OraclesUiLayout layout, const char *const *notes, const int *reasons, const char *const *labels, unsigned count, OraclesUiItemLayout *out)
{
    const int crowded = layout == ORACLES_UI_LAYOUT_4_3 && count >= ORACLES_UI_MENU_CROWDED;
    menu_at(layout, crowded ? CROWDED_ITEM_4_3 : metrics_of(layout)->item_min_height, notes, reasons, labels, count, NULL, out);
}

/* The hints centred between `left` and the scene's right edge, `gap` apart. */
static void hints_at(OraclesUiLayout layout, const OraclesHomeHint *hints, unsigned count, float left, float gap, OraclesUiHintLayout *out)
{
    const metrics *m = metrics_of(layout);
    const OraclesUiHomeStyles *st = oracles_ui_home_styles(layout);
    float total = 0.0f;
    for (unsigned i = 0; i < count; i++) {
        OraclesUiHintLayout *h = &out[i];
        h->key = line_at(st->hint_key, hints[i].key, 0.0f, 0.0f, 0.0f);
        h->label = line_at(st->hint_label, hints[i].label, 0.0f, 0.0f, 0.0f);
        /* The key's frame as wide as `chars` characters of the font at least (a ch each), its text centred in it. */
        const float least = (float)hints[i].chars * oracles_ui_text_width(st->hint_key, "0");
        const float inner_w = h->key.w > least ? h->key.w : least;
        h->key_box.w = inner_w + 2.0f * (m->key_pad_x + KEY_BORDER);
        h->key_box.h = h->key.h + 2.0f * (m->key_pad_y + KEY_BORDER);
        const float inner = h->key_box.h > h->label.h ? h->key_box.h : h->label.h;
        h->box.w = m->hint_pad_x + h->key_box.w + m->hint_gap + h->label.w + m->hint_pad_x;
        h->box.h = m->hint_pad_y * 2.0f + inner;
        if (h->box.h < m->hint_min_height) h->box.h = m->hint_min_height;
        total += h->box.w + (i ? gap : 0.0f);
    }
    float x = left + (m->scene_w - left - total) * 0.5f;
    for (unsigned i = 0; i < count; i++) {
        OraclesUiHintLayout *h = &out[i];
        h->box.x = x;
        h->box.y = m->hints_bottom - h->box.h;
        const float middle = h->box.y + h->box.h * 0.5f;
        h->key_box.x = x + m->hint_pad_x;
        h->key_box.y = middle - h->key_box.h * 0.5f;
        const float key_baseline = h->key.baseline, label_baseline = h->label.baseline;
        h->key.x = h->key_box.x + KEY_BORDER + m->key_pad_x + (h->key_box.w - 2.0f * (KEY_BORDER + m->key_pad_x) - h->key.w) * 0.5f;
        h->key.y = h->key_box.y + KEY_BORDER + m->key_pad_y;
        h->key.baseline = h->key.y + key_baseline;
        h->label.x = h->key_box.x + h->key_box.w + m->hint_gap;
        h->label.y = middle - h->label.h * 0.5f;
        h->label.baseline = h->label.y + label_baseline;
        x += h->box.w + gap;
    }
}

void oracles_ui_layout_hints(OraclesUiLayout layout, const OraclesHomeHint *hints, unsigned count, OraclesUiHintLayout *out)
{
    hints_at(layout, hints, count, 0.0f, metrics_of(layout)->hints_gap, out);
}

void oracles_ui_layout_screen_hints(OraclesUiLayout layout, const OraclesHomeNav *nav, const OraclesHomeHint *hints, unsigned count, OraclesUiHintLayout *out)
{
    const metrics *m = metrics_of(layout);
    if (layout == ORACLES_UI_LAYOUT_4_3 && nav->screen == ORACLES_SCREEN_HOME && !nav->in_game)
        hints_at(layout, hints, count, m->home_hints_left, m->home_hints_gap, out);
    else oracles_ui_layout_hints(layout, hints, count, out);
}

float oracles_ui_layout_hints_left(OraclesUiLayout layout, const OraclesHomeNav *nav)
{
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    const unsigned count = oracles_home_hints(nav, hints);
    OraclesUiHintLayout l[ORACLES_HOME_MAX_HINTS];
    oracles_ui_layout_screen_hints(layout, nav, hints, count, l);
    return count ? l[0].box.x : metrics_of(layout)->scene_w;
}

void oracles_ui_layout_version(OraclesUiLayout layout, const char *text, OraclesUiLine *out)
{
    const metrics *m = metrics_of(layout);
    *out = line_at(oracles_ui_home_styles(layout)->version, text, m->version_left, 0.0f, 0.0f);
    out->y = m->version_bottom - out->h;
    out->baseline += out->y;
}

/* A box `pad_x` by `pad_y` around a line of `text` in the version's style and the settings' line height, its border
 * `border` wide, from `x`, centred on `middle`; the line into `line`. */
static OraclesUiBox padded(OraclesUiLayout layout, const char *text, float x, float middle, float pad_x, float pad_y, float border, OraclesUiLine *line)
{
    const metrics *m = metrics_of(layout);
    *line = line_at(oracles_ui_home_styles(layout)->version, text, 0.0f, 0.0f, m->settings_line);
    const float offset = line->baseline;
    const OraclesUiBox box = { x, 0.0f, line->w + 2.0f * (pad_x + border), line->h + 2.0f * (pad_y + border) };
    OraclesUiBox b = box;
    b.y = middle - b.h * 0.5f;
    line->x = x + border + pad_x;
    line->y = b.y + border + pad_y;
    line->baseline = line->y + offset;
    return b;
}

void oracles_ui_layout_settings_link(OraclesUiLayout layout, const char *version, const char *release, float hints_x, OraclesUiSettingsLink *out)
{
    const metrics *m = metrics_of(layout);
    memset(out, 0, sizeof *out);
    const int update = release && release[0];
    out->border = m->button_border;
    /* One line, its items centred on it: the button, the version, and in 16:9 the notice, whose frame is the tallest. */
    OraclesUiLine probe = line_at(oracles_ui_home_styles(layout)->version, "", 0.0f, 0.0f, m->settings_line);
    const float button_h = probe.h + 2.0f * (m->button_pad_y + m->button_border), version_h = probe.h + 2.0f * m->version_pad_y;
    const float update_h = probe.h + 2.0f * (m->update_pad_y + m->update_border);
    float line_h = button_h > version_h ? button_h : version_h;
    if (update && !m->update_above && update_h > line_h) line_h = update_h;
    const float middle = m->settings_bottom - line_h * 0.5f;
    out->button = padded(layout, oracles_ui_settings_label, m->settings_left, middle, m->button_pad_x, m->button_pad_y, m->button_border, &out->label);
    out->version_box = padded(layout, version, out->button.x + out->button.w + m->settings_gap, middle, m->version_pad_x, m->version_pad_y, 0.0f, &out->version);
    out->version_shown = version && version[0] && out->version_box.x + out->version_box.w + ORACLES_UI_VERSION_GAP <= hints_x;
    out->hit = out->button;
    out->hit.x -= m->settings_hit[0];
    out->hit.y -= m->settings_hit[1];
    out->hit.w += m->settings_hit[0] + m->settings_hit[2];
    out->hit.h += m->settings_hit[1] + m->settings_hit[3];
    if (!update) return;
    /* The notice: after the version, its frame `update_overlap` into the gap (16:9); above the line, from its left (4:3). */
    OraclesUiUpdateLayout *u = &out->update;
    snprintf(u->label, sizeof u->label, "%s%s available", m->update_above ? "" : "\xc2\xb7 ", release);
    u->border = m->update_border;
    const float after = (out->version_shown ? out->version_box.x + out->version_box.w : out->button.x + out->button.w) + m->settings_gap - m->update_overlap;
    const float top = middle - line_h * 0.5f;
    u->box = padded(layout, u->label, m->update_above ? m->settings_left : after, m->update_above ? top - m->update_gap - update_h * 0.5f : middle,
                    m->update_pad_x, m->update_pad_y, m->update_border, &u->text);
    out->update_shown = u->box.x + u->box.w + ORACLES_UI_VERSION_GAP <= hints_x;
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

void oracles_ui_layout_pad_toast(OraclesUiLayout layout, const char *text, float bar_bottom, OraclesUiToastLayout *out)
{
    const metrics *m = metrics_of(layout);
    out->text = line_at(oracles_ui_home_styles(layout)->pad_toast, text, 0.0f, 0.0f, 0.0f);
    out->box.w = out->text.w + 2.0f * (m->pad_toast_pad_x + TOAST_BORDER);
    out->box.h = out->text.h + 2.0f * (m->pad_toast_pad_y + TOAST_BORDER);
    if (out->box.h < m->pad_toast_min_h) out->box.h = m->pad_toast_min_h;
    out->box.x = m->scene_w * 0.5f - out->box.w * 0.5f;
    out->box.y = bar_bottom + m->pad_toast_gap;
    out->text.x = out->box.x + TOAST_BORDER + m->pad_toast_pad_x;
    out->text.y = out->box.y + (out->box.h - out->text.h) * 0.5f;
    out->text.baseline += out->text.y;
}

void oracles_ui_layout_notice(OraclesUiLayout layout, const char *text, float bar_bottom, OraclesUiNoticeLayout *out)
{
    const metrics *m = metrics_of(layout);
    const OraclesUiTextStyle *style = oracles_ui_home_styles(layout)->pad_toast;
    memset(out, 0, sizeof *out);
    /* As wide as its text, up to its widest, where the text wraps. */
    const float frame = 2.0f * (m->pad_toast_pad_x + TOAST_BORDER);
    float inner = oracles_ui_text_width(style, text);
    if (inner > m->notice_max_width - frame) inner = m->notice_max_width - frame;
    OraclesUiWrapped wrapped;
    oracles_ui_wrap_pretty(style, text, inner, 0.0f, 0.0f, &wrapped);
    out->wrapped_lines = wrapped.count;
    float y = 0.0f, widest = 0.0f;
    for (unsigned i = 0; i < wrapped.count && i < ORACLES_UI_NOTICE_LINES; i++) {
        snprintf(out->text[i], sizeof out->text[i], "%s", wrapped.text[i]);
        out->text_lines[i] = line_at(style, out->text[i], 0.0f, y, 0.0f);
        if (out->text_lines[i].w > widest) widest = out->text_lines[i].w;
        y += out->text_lines[i].h;
        out->lines++;
    }
    out->box.w = widest + frame;
    out->box.h = y + 2.0f * (m->pad_toast_pad_y + TOAST_BORDER);
    if (out->box.h < m->pad_toast_min_h) out->box.h = m->pad_toast_min_h;
    out->box.x = m->scene_w * 0.5f - out->box.w * 0.5f;
    out->box.y = bar_bottom + m->pad_toast_gap;
    /* Each line centred in the box, the lines in the middle of its height. */
    const float top = out->box.y + (out->box.h - y) * 0.5f;
    for (unsigned i = 0; i < out->lines; i++) {
        out->text_lines[i].x = m->scene_w * 0.5f - out->text_lines[i].w * 0.5f;
        out->text_lines[i].y += top;
        out->text_lines[i].baseline += top;
    }
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
    OraclesUiWrapped wrapped;
    oracles_ui_wrap_pretty(st->toast, text, inner, 0.0f, 0.0f, &wrapped);
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
