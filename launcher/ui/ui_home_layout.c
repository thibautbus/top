#include "ui_home_layout.h"

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

const char oracles_ui_item_dot[] = "\xc2\xb7";

/* The home screen's geometry, in scene pixels. */
#define SCENE_W 1920.0f
#define SCENE_H 1080.0f
#define TOP 88.0f                   /* top of both title blocks */
#define HERO_RIGHT (SCENE_W - 96.0f)
#define OTHERS_LEFT 96.0f
#define OTHERS_GAP 48.0f
#define HERO_STATE_MARGIN 20.0f
#define OTHER_STATE_MARGIN 10.0f
#define OTHER_TITLE_LINE (72.0f * 1.05f)
#define HERO_TITLE_LINE 120.0f
#define MENU_RIGHT (SCENE_W - 32.0f)
#define MENU_BOTTOM (SCENE_H - 76.0f)
#define MENU_GAP 4.0f
#define ITEM_MIN_WIDTH 720.0f
#define ITEM_PAD_Y 6.0f
#define ITEM_PAD_RIGHT 36.0f
#define ITEM_PAD_LEFT 40.0f
#define ITEM_GAP 28.0f
#define REASON_PAD_X 10.0f
#define REASON_PAD_Y 2.0f
#define HINTS_BOTTOM (SCENE_H - 18.0f)
#define HINTS_GAP 36.0f
#define HINT_PAD_X 10.0f
#define HINT_PAD_Y 4.0f
#define HINT_GAP 10.0f
#define KEY_PAD_X 9.0f
#define KEY_PAD_Y 1.0f
#define KEY_BORDER 1.0f
#define VERSION_LEFT 24.0f
#define VERSION_BOTTOM (SCENE_H - 20.0f)
#define TOAST_BOTTOM (SCENE_H - 78.0f)
#define TOAST_PAD_X 26.0f
#define TOAST_PAD_Y 14.0f
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

/* A line centred on `middle` in a flex row, as an inline element's box; an empty one has no height. */
static OraclesUiLine line_centred(const OraclesUiTextStyle *style, const char *text, float x, float middle)
{
    OraclesUiLine line = line_at(style, text, x, 0.0f, 0.0f);
    const float offset = line.baseline;
    if (!text || !text[0]) line.h = 0.0f;
    line.y = middle - line.h * 0.5f;
    line.baseline = line.y + offset;
    return line;
}

void oracles_ui_layout_hero(const char *over, const char *title, const char *state, OraclesUiTitleLayout *out)
{
    /* text-align: right; the widths carry the trailing letter spacing, as the browser's do. */
    out->over = line_at(&oracles_ui_hero_over, over, 0.0f, TOP, 0.0f);
    out->title = line_at(&oracles_ui_hero_title, title, 0.0f, out->over.y + out->over.h, HERO_TITLE_LINE);
    out->state = line_at(&oracles_ui_hero_state, state, 0.0f, out->title.y + out->title.h + HERO_STATE_MARGIN, 0.0f);
    OraclesUiLine *lines[3] = { &out->over, &out->title, &out->state };
    float width = 0.0f;
    for (int i = 0; i < 3; i++) {
        lines[i]->x = HERO_RIGHT - lines[i]->w;
        if (lines[i]->w > width) width = lines[i]->w;
    }
    out->box.x = HERO_RIGHT - width;
    out->box.y = TOP;
    out->box.w = width;
    out->box.h = out->state.y + out->state.h - TOP;
}

void oracles_ui_layout_others(const char *const over[2], const char *const title[2], const char *const state[2], OraclesUiTitleLayout out[2])
{
    float y = TOP;
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

void oracles_ui_layout_menu(const char *const *notes, const int *reasons, const char *const *labels, unsigned count, OraclesUiItemLayout *out)
{
    float label_height, baseline;
    oracles_ui_line_box(&oracles_ui_item_label, 0.0f, &label_height, &baseline);
    const float height = ITEM_PAD_Y * 2.0f + label_height;   /* the serif's line is the tallest of the row */
    float y = MENU_BOTTOM - (float)count * height - (float)(count ? count - 1u : 0u) * MENU_GAP;
    for (unsigned i = 0; i < count; i++, y += height + MENU_GAP) {
        OraclesUiItemLayout *item = &out[i];
        const float middle = y + height * 0.5f;
        item->dot = line_centred(&oracles_ui_item_label, oracles_ui_item_dot, 0.0f, middle);
        item->label = line_centred(&oracles_ui_item_label, labels[i], 0.0f, middle);
        item->note = line_centred(&oracles_ui_item_note, notes[i], 0.0f, middle);
        const float pad = reasons && reasons[i] && item->note.w > 0.0f ? REASON_PAD_X : 0.0f;
        /* justify-content: flex-end, the empty note still a flex item between its two gaps. */
        item->dot.x = MENU_RIGHT - ITEM_PAD_RIGHT - item->dot.w;
        item->label.x = item->dot.x - ITEM_GAP - item->label.w;
        item->note.x = item->label.x - ITEM_GAP - pad - item->note.w;
        memset(&item->note_box, 0, sizeof item->note_box);
        if (pad > 0.0f) {
            item->note_box.x = item->note.x - pad;
            item->note_box.w = item->note.w + 2.0f * pad;
            item->note_box.h = item->note.h + 2.0f * REASON_PAD_Y;
            item->note_box.y = middle - item->note_box.h * 0.5f;
        }
        float width = ITEM_PAD_LEFT + item->note.w + 2.0f * pad + ITEM_GAP + item->label.w + ITEM_GAP + item->dot.w + ITEM_PAD_RIGHT;
        if (width < ITEM_MIN_WIDTH) width = ITEM_MIN_WIDTH;
        item->box.x = MENU_RIGHT - width;
        item->box.y = y;
        item->box.w = width;
        item->box.h = height;
    }
}

void oracles_ui_layout_hints(const OraclesHomeHint *hints, unsigned count, OraclesUiHintLayout *out)
{
    float total = 0.0f;
    for (unsigned i = 0; i < count; i++) {
        OraclesUiHintLayout *h = &out[i];
        h->key = line_at(&oracles_ui_hint_key, hints[i].key, 0.0f, 0.0f, 0.0f);
        h->label = line_at(&oracles_ui_hint_label, hints[i].label, 0.0f, 0.0f, 0.0f);
        h->key_box.w = h->key.w + 2.0f * (KEY_PAD_X + KEY_BORDER);
        h->key_box.h = h->key.h + 2.0f * (KEY_PAD_Y + KEY_BORDER);
        const float inner = h->key_box.h > h->label.h ? h->key_box.h : h->label.h;
        h->box.w = HINT_PAD_X + h->key_box.w + HINT_GAP + h->label.w + HINT_PAD_X;
        h->box.h = HINT_PAD_Y * 2.0f + inner;
        total += h->box.w + (i ? HINTS_GAP : 0.0f);
    }
    float x = (SCENE_W - total) * 0.5f;
    for (unsigned i = 0; i < count; i++) {
        OraclesUiHintLayout *h = &out[i];
        h->box.x = x;
        h->box.y = HINTS_BOTTOM - h->box.h;
        const float middle = h->box.y + h->box.h * 0.5f;
        h->key_box.x = x + HINT_PAD_X;
        h->key_box.y = middle - h->key_box.h * 0.5f;
        const float key_baseline = h->key.baseline, label_baseline = h->label.baseline;
        h->key.x = h->key_box.x + KEY_BORDER + KEY_PAD_X;
        h->key.y = h->key_box.y + KEY_BORDER + KEY_PAD_Y;
        h->key.baseline = h->key.y + key_baseline;
        h->label.x = h->key_box.x + h->key_box.w + HINT_GAP;
        h->label.y = middle - h->label.h * 0.5f;
        h->label.baseline = h->label.y + label_baseline;
        x += h->box.w + HINTS_GAP;
    }
}

void oracles_ui_layout_version(const char *text, OraclesUiLine *out)
{
    *out = line_at(&oracles_ui_version, text, VERSION_LEFT, 0.0f, 0.0f);
    out->y = VERSION_BOTTOM - out->h;
    out->baseline += out->y;
}

void oracles_ui_layout_toast(const char *text, OraclesUiToastLayout *out)
{
    out->text = line_at(&oracles_ui_toast, text, 0.0f, 0.0f, 0.0f);
    out->box.w = out->text.w + 2.0f * (TOAST_PAD_X + TOAST_BORDER);
    out->box.h = out->text.h + 2.0f * (TOAST_PAD_Y + TOAST_BORDER);
    out->box.x = SCENE_W * 0.5f - out->box.w * 0.5f;
    out->box.y = TOAST_BOTTOM - out->box.h;
    out->text.x = out->box.x + TOAST_BORDER + TOAST_PAD_X;
    out->text.y = out->box.y + TOAST_BORDER + TOAST_PAD_Y;
    out->text.baseline += out->text.y;
}
