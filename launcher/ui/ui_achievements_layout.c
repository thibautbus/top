/* Where the login, its keyboard, the toasts and the list of achievements put their pieces (ui_achievements.h). */
#include "ui_achievements.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static float max_f(float a, float b) { return a > b ? a : b; }

static OraclesUiBox box(float x, float y, float w, float h)
{
    const OraclesUiBox b = { x, y, w, h };
    return b;
}

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

/* The line moved so that its box is centred on `middle`. */
static void centre_on(OraclesUiLine *line, float middle)
{
    const float y = middle - line->h * 0.5f;
    line->baseline += y - line->y;
    line->y = y;
}

/* ---- the login ------------------------------------------------------------------------------------------- */

/* Its measures in each layout: 16:9's, then 4:3's (the 640x480 screen's). */
typedef struct login_metrics {
    float top, top_system, top_keyboard, width, pad_x, pad_y, gap;
    float label_gap, field_h, field_pad, caret_h, caret_gap, state_h, state_dot, state_gap;
    float button_w, button_h, button_pad, button_gap, border, focus_border;
    float keyboard_top, key_w, key_h, key_gap, keyboard_pad;
    float eye, eye_glyph;
} login_metrics;
static const login_metrics login_16_9 = { 170.0f, 36.0f, 24.0f, 880.0f, 40.0f, 28.0f, 14.0f, 6.0f, 64.0f, 18.0f, 34.0f, 2.0f, 32.0f, 12.0f, 12.0f,
                                          150.0f, 52.0f, 22.0f, 12.0f, 1.0f, 2.0f, 560.0f, 84.0f, 64.0f, 6.0f, 12.0f, 48.0f, 28.0f };
static const login_metrics login_4_3 = { 150.0f, 36.0f, 24.0f, 1040.0f, 44.0f, 30.0f, 14.0f, 6.0f, 80.0f, 18.0f, 44.0f, 2.0f, 40.0f, 12.0f, 12.0f,
                                         200.0f, 80.0f, 22.0f, 12.0f, 2.0f, 3.0f, 492.0f, 112.0f, 84.0f, 8.0f, 14.0f, 60.0f, 36.0f };
#define DIALOG_BORDER 1.0f
#define FIELD_BORDER 1.0f
#define KEYBOARD_BORDER 1.0f
#define MASK_DOT "\xe2\x80\xa2"   /* • */
#define MASK_SPACING 0.12f        /* the dots' letter spacing, em */
#define EYE_GAP 8.0f              /* the eye from the text and from the field's border */

static const OraclesUiTextStyle login_styles[2][ORACLES_UI_LOGIN_STYLES] = {
    { { ORACLES_UI_FONT_SERIF, 26.0f, 0.2f, 1 }, { ORACLES_UI_FONT_SERIF, 20.0f, 0.16f, 1 }, { ORACLES_UI_FONT_SANS, 28.0f, 0.0f, 0 },
      { ORACLES_UI_FONT_SANS, 24.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SANS, 25.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SERIF, 26.0f, 0.0f, 0 },
      { ORACLES_UI_FONT_MONO, 28.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SERIF, 22.0f, 0.0f, 0 } },
    { { ORACLES_UI_FONT_SERIF, 30.0f, 0.2f, 1 }, { ORACLES_UI_FONT_SERIF, 27.0f, 0.16f, 1 }, { ORACLES_UI_FONT_SANS, 36.0f, 0.0f, 0 },
      { ORACLES_UI_FONT_SANS, 30.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SANS, 30.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SERIF, 36.0f, 0.0f, 0 },
      { ORACLES_UI_FONT_MONO, 36.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SERIF, 30.0f, 0.0f, 0 } },
};

const OraclesUiTextStyle *oracles_ui_login_style(OraclesUiLayout layout, int which)
{
    return &login_styles[layout == ORACLES_UI_LAYOUT_4_3][which < ORACLES_UI_LOGIN_STYLES ? which : 0];
}

static const char placeholder[] = "As on retroachievements.org";
/* Each key of the last row: how many keys wide. */
static float key_span(const char *k)
{
    if (!strcmp(k, "Shift") || !strcmp(k, "Done")) return 2.0f;
    if (!strcmp(k, "#+=") || !strcmp(k, "abc")) return 1.6f;
    if (!strcmp(k, "Space")) return 2.0f;
    if (!strcmp(k, "\xe2\x8c\xab")) return 1.4f;
    return 1.0f;
}

void oracles_ui_layout_login(OraclesUiLayout layout, const OraclesHomeNav *nav, OraclesUiLoginLayout *out)
{
    memset(out, 0, sizeof *out);
    const OraclesHomeAchievements *a = &nav->achievements;
    if (!a->login) return;
    const int four_three = layout == ORACLES_UI_LAYOUT_4_3, keyboard = oracles_achievements_keyboard_shown(nav);
    const login_metrics *m = four_three ? &login_4_3 : &login_16_9;
    const OraclesUiTextStyle *st = login_styles[four_three];
    const float scene_w = oracles_ui_scene_width(layout);
    /* Android's keyboard over the lower part, or the launcher's: the dialog at the top, the field typed above them. */
    const float top = a->system_keyboard ? m->top_system : keyboard ? m->top_keyboard : m->top;
    const float left = (scene_w - m->width) * 0.5f, x = left + DIALOG_BORDER + m->pad_x, inner_w = m->width - 2.0f * (DIALOG_BORDER + m->pad_x);
    float y = top + DIALOG_BORDER + m->pad_y;
    out->title = line_at(&st[ORACLES_UI_LOGIN_TITLE], "Log in to RetroAchievements", x, y, 0.0f);
    y += out->title.h + m->gap;
    static const char *const labels[2] = { "User name", "Password" };
    const int busy = a->login == ORACLES_LOGIN_LOGGING;
    out->shown = oracles_achievements_password_shown(nav);
    /* The password's dots, spaced as the browser spaces letters. */
    size_t n = 0;
    for (const char *c = a->password; *c && n + 4 < sizeof out->masked; c++)
        if (((unsigned char)*c & 0xc0u) != 0x80u) { memcpy(out->masked + n, MASK_DOT, 3); n += 3; }
    out->masked[n] = 0;
    for (int f = 0; f < 2; f++) {
        OraclesUiLoginField *field = &out->fields[f];
        const int typed = a->field == f, focused = typed || (f == ORACLES_LOGIN_PASSWORD && a->field == ORACLES_LOGIN_EYE);
        field->label = line_at(&st[ORACLES_UI_LOGIN_LABEL], labels[f], x, y, 0.0f);
        field->frame = box(x, y + field->label.h + m->label_gap, inner_w, m->field_h);
        field->box = box(x, y, inner_w, field->frame.y + field->frame.h - y);
        const float border = focused ? m->focus_border : FIELD_BORDER, middle = field->frame.y + m->field_h * 0.5f;
        const char *value = !f ? a->name : out->shown ? a->password : out->masked;
        OraclesUiTextStyle masked = st[ORACLES_UI_LOGIN_VALUE];
        if (f && !out->shown) masked.tracking = MASK_SPACING;
        field->value = line_at(&masked, value, field->frame.x + border + m->field_pad, 0.0f, 0.0f);
        centre_on(&field->value, middle);
        /* The eye at the password's right end, its room kept while there is nothing to show. */
        float text_right = field->frame.x + field->frame.w - border - m->field_pad;
        if (f) {
            field->eye = box(field->frame.x + field->frame.w - border - EYE_GAP - m->eye, middle - m->eye * 0.5f, m->eye, m->eye);
            field->eye_shown = a->password[0] != 0;
            field->eye_size = m->eye_glyph;
            text_right = field->eye.x - EYE_GAP;
        }
        if (!value[0] && !focused && !f) {
            field->placeholder = line_at(&st[ORACLES_UI_LOGIN_VALUE], placeholder, field->value.x, 0.0f, 0.0f);
            centre_on(&field->placeholder, middle);
        }
        if (typed && !busy) field->caret = box(field->value.x + field->value.w + m->caret_gap, middle - m->caret_h * 0.5f, 2.0f, m->caret_h);
        /* A text longer than the field: drawn within it, before the eye, its end and the caret kept in sight. */
        const float text_x = field->frame.x + border + m->field_pad;
        field->clip = box(text_x, field->frame.y + border, text_right - text_x, field->frame.h - 2.0f * border);
        const float end = field->caret.w > 0.0f ? field->caret.x + field->caret.w : field->value.x + field->value.w;
        const float over = end - text_right;
        if (over > 0.0f) {
            field->value.x -= over;
            field->caret.x -= over;
        }
        y = field->frame.y + field->frame.h + m->gap;
    }
    if (!keyboard) {
        out->note = line_at(&st[ORACLES_UI_LOGIN_NOTE], "Sent once over HTTPS; only the token the server returns is kept.", x, y, 0.0f);
        y += out->note.h + m->gap;
    }
    /* The state line keeps its place, shown or not: nothing moves when it comes. */
    out->state = box(x, y, inner_w, m->state_h);
    OraclesTone tone;
    const char *state = oracles_achievements_login_state_text(nav, &tone);
    out->state_text = line_at(&st[ORACLES_UI_LOGIN_STATE], state, x + m->state_dot + m->state_gap, 0.0f, 0.0f);
    centre_on(&out->state_text, y + m->state_h * 0.5f);
    if (state) out->state_dot = box(x, y + (m->state_h - m->state_dot) * 0.5f, m->state_dot, m->state_dot);
    else out->state_text.h = 0.0f;
    y += m->state_h;
    if (!keyboard) {
        y += m->gap;
        static const char *const names[2] = { "Log in", "Cancel" };
        float bx = x + inner_w;
        for (int b = 1; b >= 0; b--) {
            OraclesUiLine name = line_at(&st[ORACLES_UI_LOGIN_BUTTON], names[b], 0.0f, 0.0f, 0.0f);
            const float w = max_f(m->button_w, name.w + 2.0f * (m->button_pad + m->border));
            bx -= w;
            out->buttons[b] = box(bx, y, w, m->button_h);
            name.x = bx + (w - name.w) * 0.5f;
            centre_on(&name, y + m->button_h * 0.5f);
            out->button_names[b] = name;
            bx -= m->button_gap;
        }
        y += m->button_h;
    }
    out->dialog = box(left, top, m->width, y + m->pad_y + DIALOG_BORDER - top);
    if (!keyboard) return;
    /* The keyboard under the dialog, centred: rows of keys a key's width each, the last row's wider. */
    float width = 0.0f;
    for (unsigned r = 0; r < ORACLES_KEYBOARD_ROWS; r++) {
        out->key_count[r] = oracles_achievements_keyboard_keys(nav, r, out->key_texts[r]);
        float w = 0.0f;
        for (unsigned k = 0; k < out->key_count[r]; k++) {
            const float span = key_span(out->key_texts[r][k]);
            w += roundf(m->key_w * span + m->key_gap * (span - 1.0f)) + (k ? m->key_gap : 0.0f);
        }
        width = max_f(width, w);
    }
    const float kb_w = width + 2.0f * (m->keyboard_pad + KEYBOARD_BORDER);
    out->keyboard = box((scene_w - kb_w) * 0.5f, m->keyboard_top, kb_w,
                        ORACLES_KEYBOARD_ROWS * m->key_h + (ORACLES_KEYBOARD_ROWS - 1) * m->key_gap + 2.0f * (m->keyboard_pad + KEYBOARD_BORDER));
    float ky = out->keyboard.y + KEYBOARD_BORDER + m->keyboard_pad;
    for (unsigned r = 0; r < ORACLES_KEYBOARD_ROWS; r++) {
        float kx = out->keyboard.x + KEYBOARD_BORDER + m->keyboard_pad;
        for (unsigned k = 0; k < out->key_count[r]; k++) {
            const char *text = out->key_texts[r][k];
            const float span = key_span(text), w = roundf(m->key_w * span + m->key_gap * (span - 1.0f));
            out->keys[r][k] = box(kx, ky, w, m->key_h);
            /* One character in the monospace, a word in the serif. */
            const int word = strlen(text) > 1 && strcmp(text, "\xe2\x8c\xab") != 0;
            OraclesUiLine name = line_at(&st[word ? ORACLES_UI_LOGIN_KEY_WORD : ORACLES_UI_LOGIN_KEY], text, 0.0f, 0.0f, 0.0f);
            name.x = kx + (w - name.w) * 0.5f;
            centre_on(&name, ky + m->key_h * 0.5f);
            out->key_names[r][k] = name;
            kx += w + m->key_gap;
        }
        ky += m->key_h + m->key_gap;
    }
}

/* ---- the toasts -------------------------------------------------------------------------------------------- */

static const OraclesUiTextStyle toast_styles[2][2] = {
    { { ORACLES_UI_FONT_SANS, 24.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SANS, 20.0f, 0.0f, 0 } },
    { { ORACLES_UI_FONT_SANS, 30.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SANS, 26.0f, 0.0f, 0 } },
};

const OraclesUiTextStyle *oracles_ui_toast_style(OraclesUiLayout layout, int over) { return &toast_styles[layout == ORACLES_UI_LAYOUT_4_3][over != 0]; }

/* 16:9's, then 4:3's: 24 from the image's left (4:3: 20), 16 under its status bar, under the reminder while it shows
 * (72, 4:3: 102); its padding (top and bottom, right, left), the badge and the gaps; the box's border. */
typedef struct toast_metrics { float left, under, reminder, pad_y, pad_right, pad_left, badge, gap, radius, line_gap; } toast_metrics;
static const toast_metrics toast_16_9 = { 24.0f, 16.0f, 72.0f, 12.0f, 22.0f, 14.0f, 64.0f, 12.0f, 8.0f, 2.0f };
static const toast_metrics toast_4_3 = { 20.0f, 16.0f, 102.0f, 14.0f, 26.0f, 16.0f, 80.0f, 14.0f, 9.0f, 2.0f };
#define TOAST_BORDER 1.0f

void oracles_ui_layout_achievement_toast(OraclesUiLayout layout, const OraclesAchievementsToast *toast, int badge, float left, float bar_bottom, int reminder,
                                         OraclesUiAchievementToastLayout *out)
{
    memset(out, 0, sizeof *out);
    const int four_three = layout == ORACLES_UI_LAYOUT_4_3;
    const toast_metrics *m = four_three ? &toast_4_3 : &toast_16_9;
    const float x = roundf(max_f(0.0f, left) + m->left), y = roundf(max_f(0.0f, bar_bottom) + m->under + (reminder ? m->reminder : 0.0f));
    const float inner_x = x + TOAST_BORDER + m->pad_left, inner_y = y + TOAST_BORDER + m->pad_y;
    float text_x = inner_x;
    if (badge) {
        out->badge = box(inner_x, 0.0f, m->badge, m->badge);
        text_x += m->badge + m->gap;
    }
    out->over = line_at(&toast_styles[four_three][1], toast->over, text_x, 0.0f, 0.0f);
    out->text = line_at(&toast_styles[four_three][0], toast->text, text_x, 0.0f, 0.0f);
    const float column_h = out->over.h + m->line_gap + out->text.h, inner_h = max_f(column_h, badge ? m->badge : 0.0f);
    const float column_y = inner_y + (inner_h - column_h) * 0.5f;
    out->over.baseline += column_y - out->over.y;
    out->over.y = column_y;
    const float text_y = column_y + out->over.h + m->line_gap;
    out->text.baseline += text_y - out->text.y;
    out->text.y = text_y;
    if (badge) out->badge.y = inner_y + (inner_h - m->badge) * 0.5f;
    const float column_w = max_f(out->over.w, out->text.w);
    out->box = box(x, y, text_x + column_w + m->pad_right + TOAST_BORDER - x, inner_h + 2.0f * (m->pad_y + TOAST_BORDER));
    out->radius = m->radius;
}

/* ---- the list ------------------------------------------------------------------------------------------------ */

static const OraclesUiTextStyle list_styles[2][ORACLES_UI_ACH_STYLES] = {
    { { ORACLES_UI_FONT_SANS_MEDIUM, 28.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SANS_MEDIUM, 28.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SANS, 24.0f, 0.0f, 0 },
      { ORACLES_UI_FONT_SERIF, 25.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SANS, 22.0f, 0.0f, 0 }, { ORACLES_UI_FONT_MONO, 20.0f, 0.0f, 0 },
      { ORACLES_UI_FONT_SERIF, 44.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SERIF, 32.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SANS, 24.0f, 0.0f, 0 },
      { ORACLES_UI_FONT_SERIF, 20.0f, 0.14f, 1 }, { ORACLES_UI_FONT_SANS, 24.0f, 0.0f, 0 }, { ORACLES_UI_FONT_MONO, 20.0f, 0.0f, 0 } },
    { { ORACLES_UI_FONT_SANS, 30.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SANS_MEDIUM, 36.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SANS, 30.0f, 0.0f, 0 },
      { ORACLES_UI_FONT_SERIF, 30.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SANS, 27.0f, 0.0f, 0 }, { ORACLES_UI_FONT_MONO, 27.0f, 0.0f, 0 },
      { ORACLES_UI_FONT_SERIF, 54.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SERIF, 36.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SANS, 30.0f, 0.0f, 0 },
      { ORACLES_UI_FONT_SERIF, 26.0f, 0.14f, 1 }, { ORACLES_UI_FONT_SANS, 30.0f, 0.0f, 0 }, { ORACLES_UI_FONT_MONO, 27.0f, 0.0f, 0 } },
};

const OraclesUiTextStyle *oracles_ui_achievements_style(OraclesUiLayout layout, int which)
{
    return &list_styles[layout == ORACLES_UI_LAYOUT_4_3][which < ORACLES_UI_ACH_STYLES ? which : 0];
}

/* 16:9: the head's column as Cheats'; the panel from (656, 56) across 1208 to 1004, padded 24 by 32, the summary over
 * the list (padded 0 20 10, its border under it), the list 10 under it; each achievement 96 tall at least, 6 apart,
 * padded 12 by 20, its badge 64, its texts 24 from it, its points' column 220 wide on the right; the list's scroll bar
 * 6 wide, 4 from the panel's side, the rows 22 from it.  4:3: the head from (64, 28), the summary on its third line; the
 * list from (64, 208) across 1312, 762 high; rows of 120, padded 10 by 20, the badge 80, the bar 8 wide. */
typedef struct list_metrics {
    float panel_x, panel_y, panel_w, panel_h, pad_x, pad_y, border;
    float row_min, row_gap, row_pad_x, row_pad_y, badge, column_gap, right_w, scroll_w, scroll_right, scroll_gap;
    float dot, dot_gap, summary_pad_x, summary_pad_bottom, summary_gap, title_gap, right_gap, bar_w, bar_h, bar_gap;
} list_metrics;
static const list_metrics list_16_9 = { 656.0f, 56.0f, 1208.0f, 948.0f, 32.0f, 24.0f, 1.0f, 96.0f, 6.0f, 20.0f, 12.0f, 64.0f, 24.0f, 220.0f, 6.0f, 4.0f, 22.0f,
                                        12.0f, 12.0f, 20.0f, 10.0f, 10.0f, 2.0f, 6.0f, 110.0f, 6.0f, 10.0f };
static const list_metrics list_4_3 = { 64.0f, 208.0f, 1312.0f, 762.0f, 0.0f, 0.0f, 0.0f, 120.0f, 6.0f, 20.0f, 10.0f, 80.0f, 24.0f, 0.0f, 8.0f, 4.0f, 22.0f,
                                       14.0f, 12.0f, 0.0f, 0.0f, 0.0f, 0.0f, 4.0f, 0.0f, 0.0f, 0.0f };
#define MODE_GAP 28.0f
#define MODE_LINE 32.0f
#define HEAD_X 96.0f
#define HEAD_W 520.0f
#define HEAD_GAP_4_3 4.0f
#define SUMMARY_BORDER 1.0f
#define ROWS_HELP_W_4_3 1272.0f    /* the page's help, across the rows */

/* `text` cut with an ellipsis where it is wider than `width`, as text-overflow: ellipsis cuts it. */
static void ellipsis(const OraclesUiTextStyle *style, const char *text, float width, char *out, size_t capacity)
{
    oracles_ui_copy_whole(out, capacity, text);
    if (oracles_ui_text_width(style, out) <= width) return;
    size_t n = strlen(out);
    while (n > 0) {
        do n--; while (n > 0 && ((unsigned char)out[n] & 0xc0u) == 0x80u);
        char trial[512];
        snprintf(trial, sizeof trial, "%.*s\xe2\x80\xa6", (int)n, text);
        if (oracles_ui_text_width(style, trial) <= width || n == 0) { oracles_ui_copy_whole(out, capacity, trial); return; }
    }
}

/* An achievement's row at (x, y), `w` wide: its badge, its title over its description, each cut to their column, its
 * points and its date, progress or (4:3) "Locked" in the column at the right. */
static void achievement_row(int four_three, const OraclesAchievementsItem *item, unsigned index, float x0, float w, float y, OraclesUiAchievementRow *row)
{
    const list_metrics *m = four_three ? &list_4_3 : &list_16_9;
    const OraclesUiTextStyle *st = list_styles[four_three];
    row->index = index;
    row->row = box(x0, y, w, m->row_min);
    const float in_y = y + m->row_pad_y, in_h = m->row_min - 2.0f * m->row_pad_y, x = x0 + m->row_pad_x;
    row->badge = box(x, in_y + (in_h - m->badge) * 0.5f, m->badge, m->badge);
    /* The points' column: the points, then the date, the progress or (4:3) "Locked", right-aligned. */
    const float right = x0 + w - m->row_pad_x;
    snprintf(row->points_text, sizeof row->points_text, "%u point%s", item->points, item->points == 1u ? "" : "s");
    row->points = line_at(&st[ORACLES_UI_ACH_POINTS], row->points_text, 0.0f, 0.0f, 0.0f);
    row->points.x = right - row->points.w;
    const OraclesUiTextStyle *second = &st[ORACLES_UI_ACH_DATE];
    /* The home page's unlocks have no date: the server gives one to a session alone. */
    if (item->unlocked && !item->date[0]) snprintf(row->date_text, sizeof row->date_text, "Unlocked");
    else if (item->unlocked) snprintf(row->date_text, sizeof row->date_text, four_three ? "%s" : "Unlocked %s", item->date);
    else if (item->progress[0]) { snprintf(row->date_text, sizeof row->date_text, "%s", item->progress); second = &st[ORACLES_UI_ACH_PROGRESS]; }
    else if (four_three) snprintf(row->date_text, sizeof row->date_text, "Locked");
    float column_h = row->points.h;
    if (row->date_text[0]) {
        OraclesUiLine *line = second == &st[ORACLES_UI_ACH_PROGRESS] ? &row->progress : &row->date;
        *line = line_at(second, row->date_text, 0.0f, 0.0f, 0.0f);
        line->x = right - line->w;
        column_h += m->right_gap + line->h;
        if (!four_three && line == &row->progress) row->bar = box(line->x - m->bar_gap - m->bar_w, 0.0f, m->bar_w, m->bar_h);
    }
    const float column_y = in_y + (in_h - column_h) * 0.5f;
    row->points.baseline += column_y - row->points.y;
    row->points.y = column_y;
    OraclesUiLine *second_line = row->progress.h > 0.0f ? &row->progress : &row->date;
    if (second_line->h > 0.0f) {
        const float sy = column_y + row->points.h + m->right_gap;
        second_line->baseline += sy - second_line->y;
        second_line->y = sy;
        if (row->bar.w > 0.0f) row->bar.y = sy + (second_line->h - m->bar_h) * 0.5f;
    }
    /* The title over the description, each on one line cut with an ellipsis; 4:3's column as wide as is left. */
    const float text_x = row->badge.x + m->badge + m->column_gap;
    const float text_w = four_three ? right - m->column_gap - max_f(row->points.w, second_line->w) - text_x : right - m->right_w - m->column_gap - text_x;
    ellipsis(&st[ORACLES_UI_ACH_TITLE], item->title, text_w, row->title_text, sizeof row->title_text);
    ellipsis(&st[ORACLES_UI_ACH_DESCRIPTION], item->description, text_w, row->description_text, sizeof row->description_text);
    row->title = line_at(&st[ORACLES_UI_ACH_TITLE], row->title_text, text_x, 0.0f, 0.0f);
    row->description = line_at(&st[ORACLES_UI_ACH_DESCRIPTION], row->description_text, text_x, 0.0f, 0.0f);
    const float text_h = row->title.h + (four_three ? 0.0f : m->title_gap) + row->description.h, text_y = in_y + (in_h - text_h) * 0.5f;
    row->title.baseline += text_y - row->title.y;
    row->title.y = text_y;
    const float dy = text_y + row->title.h + (four_three ? 0.0f : m->title_gap);
    row->description.baseline += dy - row->description.y;
    row->description.y = dy;
}

/* The list's place, its rows' and which comes first: nothing of it measures a text, the rows cut to their columns being the
 * cost of a layout. */
typedef struct list_place {
    OraclesUiBox list;
    float row_w, pitch;
    unsigned first, last_first, fit;
} list_place;

static list_place place_of_list(int four_three, const OraclesHomeNav *nav)
{
    const list_metrics *m = four_three ? &list_4_3 : &list_16_9;
    const float inner_x = m->panel_x + m->border + m->pad_x, inner_w = m->panel_w - 2.0f * (m->border + m->pad_x);
    float list_y = m->panel_y;
    if (!four_three) {
        float summary_h, baseline;
        oracles_ui_line_box(&list_styles[0][ORACLES_UI_ACH_SUMMARY], 0.0f, &summary_h, &baseline);
        list_y = m->panel_y + m->border + m->pad_y + summary_h + m->summary_pad_bottom + SUMMARY_BORDER + m->summary_gap;
    }
    list_place p;
    p.list = box(inner_x, list_y, inner_w, m->panel_y + m->panel_h - m->border - m->pad_y - list_y);
    p.row_w = inner_w - m->scroll_gap;
    p.pitch = m->row_min + m->row_gap;
    /* Rows of one height (one line each: title and description cut), scrolled by whole rows from the row kept, never
     * past the last filling the list, and as little as brings the highlight wholly in sight. */
    const OraclesAchievementsList *l = nav->achievements.list;
    const unsigned count = l ? l->count : 0u, focus = nav->achievements.list_focus;
    const unsigned fit = (unsigned)floorf((p.list.h + m->row_gap) / p.pitch);
    p.fit = fit ? fit : 1u;
    p.last_first = count > p.fit ? count - p.fit : 0u;
    unsigned first = nav->achievements.list_first < p.last_first ? nav->achievements.list_first : p.last_first;
    /* Brought to the highlight only once none of it shows: a row cut at the foot still counts. */
    const unsigned shown = (float)p.fit * p.pitch < p.list.h ? p.fit + 1u : p.fit;
    if (focus < count) {
        if (focus < first) first = focus;
        else if (focus >= first + shown) first = focus + 1u - p.fit;
    }
    p.first = first;
    return p;
}

void oracles_ui_layout_achievements_list(OraclesUiLayout layout, const OraclesHomeNav *nav, OraclesUiAchievementsLayout *out)
{
    memset(out, 0, sizeof *out);
    const int four_three = layout == ORACLES_UI_LAYOUT_4_3;
    const list_metrics *m = four_three ? &list_4_3 : &list_16_9;
    const OraclesUiTextStyle *st = list_styles[four_three];
    const OraclesHomeHero hero = oracles_home_hero(nav);
    oracles_ui_layout_page_head(layout, "Achievements", oracles_home_over(hero), oracles_home_title(hero), NULL, &out->head);
    oracles_achievements_list_summary(nav, out->summary_line, sizeof out->summary_line, &out->warning);
    out->panel = box(m->panel_x, m->panel_y, m->panel_w, m->panel_h);
    const list_place place = place_of_list(four_three, nav);
    const float inner_x = place.list.x, inner_w = place.list.w, list_y = place.list.y;
    if (four_three) {
        /* The summary on the head's third line, its dot before it. */
        const float y = out->head.title.lines[0].y + out->head.title.h + HEAD_GAP_4_3;
        out->summary_text = line_at(&st[ORACLES_UI_ACH_SUMMARY], out->summary_line, m->panel_x + m->dot + m->dot_gap, y, 0.0f);
        out->summary = box(m->panel_x, y, m->panel_w, out->summary_text.h);
    } else {
        oracles_ui_wrap_pretty(&oracles_ui_row_text, "RetroAchievements, casual mode: every launcher feature stays, savestates included.", HEAD_W, HEAD_X,
                               out->head.title.lines[0].y + out->head.title.h + MODE_GAP, &out->mode);
        for (unsigned i = 0; i < out->mode.count; i++)
            out->mode.lines[i] = line_at(&oracles_ui_row_text, out->mode.text[i], HEAD_X, out->head.title.lines[0].y + out->head.title.h + MODE_GAP + (float)i * MODE_LINE, MODE_LINE);
        out->mode.h = (float)out->mode.count * MODE_LINE;
        const float y = m->panel_y + m->border + m->pad_y;
        out->summary_text = line_at(&st[ORACLES_UI_ACH_SUMMARY], out->summary_line, inner_x + m->summary_pad_x + m->dot + m->dot_gap, y, 0.0f);
        out->summary = box(inner_x, y, inner_w, out->summary_text.h + m->summary_pad_bottom + SUMMARY_BORDER);
    }
    out->summary_dot = box(out->summary_text.x - m->dot_gap - m->dot, out->summary_text.y + (out->summary_text.h - m->dot) * 0.5f, m->dot, m->dot);
    const float list_bottom = list_y + place.list.h;
    out->list = place.list;
    const OraclesAchievementsList *l = nav->achievements.list;
    const float row_w = place.row_w, pitch = place.pitch;
    const unsigned count = l ? l->count : 0u, last_first = place.last_first, first = place.first;
    out->first = first;
    out->track = box(inner_x + inner_w - m->scroll_right - m->scroll_w, list_y, m->scroll_w, out->list.h);
    if (count) {
        const float visible = out->list.h / pitch, fraction = visible / (float)count < 1.0f ? visible / (float)count : 1.0f;
        const float thumb_h = out->list.h * fraction, along = last_first ? (float)first / (float)last_first : 0.0f;
        out->thumb = box(out->track.x, list_y + (out->list.h - thumb_h) * along, m->scroll_w, thumb_h);
    }
    for (unsigned i = first; i < count && out->shown < ORACLES_UI_ACHIEVEMENTS_SHOWN; i++) {
        const float y = list_y + (float)(i - first) * pitch;
        if (y >= list_bottom) break;
        achievement_row(four_three, &l->items[i], i, inner_x, row_w, y, &out->rows[out->shown++]);
    }
}

/* The rows of the page, moved up by `dy`. */
static void move_up(OraclesUiLine *line, float dy)
{
    line->y -= dy;
    line->baseline -= dy;
}

static void page_scrolled(OraclesUiCheatsLayout *l, float dy)
{
    for (unsigned r = 0; r < ORACLES_UI_LIST_ROWS; r++) {
        move_up(&l->labels[r], dy);
        l->rows[r].y -= dy;
        for (unsigned i = 0; i < l->explanations[r].count; i++) move_up(&l->explanations[r].lines[i], dy);
        for (int c = 0; c < 2; c++) {
            l->choices[r][c].box.y -= dy;
            move_up(&l->choices[r][c].name, dy);
        }
        move_up(&l->actions[r], dy);
        move_up(&l->action_dots[r], dy);
        move_up(&l->values[r], dy);
        l->action_boxes[r].y -= dy;
    }
    for (unsigned i = 0; i < l->status.count; i++) move_up(&l->status.lines[i], dy);
    l->status_dot.y -= dy;
}

/* 16:9: the list 20 under the rows (the panel's gap and 6), its rule, the summary 16 under it, padded 20, the first
 * achievement 14 under it (the summary's padding and the gap); the panel 948 high, an achievement in sight from the rows'
 * top to 970.  4:3: the list 6 under the rows, its rule, the summary centred in 40, 6 under its rule, the first
 * achievement 6 under it; 662 of rows and list, the help 20 under them; the page scrolled by whole achievements, five in
 * sight. */
#define PAGE_ROWS_GAP 14.0f
#define PAGE_LIST_GAP 6.0f
#define PAGE_SUMMARY_PAD 16.0f
#define PAGE_SUMMARY_FOOT 8.0f
#define PAGE_FOOT 970.0f
#define PAGE_SUMMARY_GAP_4_3 6.0f
#define PAGE_SUMMARY_H_4_3 40.0f
#define PAGE_VIEW_4_3 662.0f
#define PAGE_HELP_4_3 20.0f
#define PAGE_SIGHT_4_3 656.0f

/* The page unscrolled: its places (its rows, then its achievements), the band of the view the keys keep the highlight in
 * (`top` to `foot`), the view's clip, how far it scrolls; 4:3's scrolls are 0, then `above` (the list's first achievement
 * at the view's top) and a pitch more each. */
typedef struct page_frame {
    int whole;
    unsigned places;
    float first_y, pitch, row_min, top, foot, clip_top, clip_foot, above, max;
    float row_top[ORACLES_ACHIEVEMENTS_ROWS], row_foot[ORACLES_ACHIEVEMENTS_ROWS];
} page_frame;

static void place_span(const page_frame *f, unsigned place, float *top, float *foot)
{
    if (place < ORACLES_ACHIEVEMENTS_ROWS) {
        *top = f->row_top[place];
        *foot = f->row_foot[place];
        return;
    }
    *top = f->first_y + (float)(place - ORACLES_ACHIEVEMENTS_ROWS) * f->pitch;
    *foot = *top + f->row_min;
}

static float snap_down(const page_frame *f, float scroll)
{
    if (!f->whole) return scroll;
    if (scroll < f->above) return 0.0f;
    return f->above + floorf((scroll - f->above) / f->pitch + 0.001f) * f->pitch;
}

static float snap_up(const page_frame *f, float scroll)
{
    if (!f->whole || scroll <= 0.0f) return scroll;
    if (scroll <= f->above) return f->above;
    return f->above + ceilf((scroll - f->above) / f->pitch - 0.001f) * f->pitch;
}

static float page_clamp(const page_frame *f, float scroll)
{
    scroll = snap_down(f, scroll);
    if (scroll > f->max) scroll = f->max;
    return scroll > 0.0f ? scroll : 0.0f;
}

static int page_sees(const page_frame *f, unsigned place, float scroll)
{
    float top, foot;
    place_span(f, place, &top, &foot);
    return top - scroll >= f->top - 0.5f && foot - scroll <= f->foot + 0.5f;
}

/* `place` wholly within the view's clip, or partly. */
static int page_in_clip(const page_frame *f, unsigned place, float scroll, int partly)
{
    float top, foot;
    place_span(f, place, &top, &foot);
    top -= scroll;
    foot -= scroll;
    return partly ? foot > f->clip_top && top < f->clip_foot : top >= f->clip_top - 0.5f && foot <= f->clip_foot + 0.5f;
}

/* The scroll kept, within the page's ends, moved as little as brings `place` wholly in the keys' band. */
static float page_follow(const page_frame *f, unsigned place, float scroll)
{
    scroll = page_clamp(f, scroll);
    if (place >= f->places || page_sees(f, place, scroll)) return scroll;
    float top, foot;
    place_span(f, place, &top, &foot);
    return page_clamp(f, top - scroll < f->top - 0.5f ? snap_down(f, top - f->top) : snap_up(f, foot - f->foot));
}

/* The page laid out unscrolled, and its frame; 0 while it has no list. */
static int page_base(OraclesUiLayout layout, const OraclesHomeNav *nav, OraclesUiAchievementsPageLayout *out, page_frame *f)
{
    memset(out, 0, sizeof *out);
    memset(f, 0, sizeof *f);
    oracles_ui_layout_achievements(layout, nav, &out->page);
    const int four_three = layout == ORACLES_UI_LAYOUT_4_3;
    const list_metrics *m = four_three ? &list_4_3 : &list_16_9;
    const OraclesUiTextStyle *st = list_styles[four_three];
    OraclesUiCheatsLayout *p = &out->page;
    out->list = oracles_achievements_page_list(nav);
    if (!out->list) {
        out->clip = four_three ? box(m->panel_x, m->panel_y, m->panel_w, 1080.0f - m->panel_y) : p->panel;
        return 0;
    }
    const OraclesAchievementsList *l = nav->achievements.list;
    oracles_achievements_list_summary(nav, out->summary_line, sizeof out->summary_line, &out->warning);
    const OraclesUiBox *sound = &p->rows[ORACLES_ACHIEVEMENTS_ROW_SOUND];
    const float inner_x = m->panel_x + m->border + m->pad_x, inner_w = m->panel_w - 2.0f * (m->border + m->pad_x);
    const float rows_bottom = sound->y + sound->h, pitch = m->row_min + m->row_gap;
    const float rule_y = rows_bottom + (four_three ? m->row_gap : PAGE_ROWS_GAP + PAGE_LIST_GAP);
    out->rule = box(inner_x, rule_y, inner_w, 1.0f);
    float first_y;
    if (four_three) {
        const float top = rule_y + 1.0f + PAGE_SUMMARY_GAP_4_3, middle = top + PAGE_SUMMARY_H_4_3 * 0.5f;
        out->summary_text = line_at(&st[ORACLES_UI_ACH_SUMMARY], out->summary_line, inner_x + m->row_pad_x + m->dot + m->dot_gap, 0.0f, 0.0f);
        centre_on(&out->summary_text, middle);
        first_y = top + PAGE_SUMMARY_H_4_3 + m->row_gap;
    } else {
        out->summary_text = line_at(&st[ORACLES_UI_ACH_SUMMARY], out->summary_line, inner_x + m->row_pad_x + m->dot + m->dot_gap, rule_y + 1.0f + PAGE_SUMMARY_PAD, 0.0f);
        first_y = out->summary_text.y + out->summary_text.h + PAGE_SUMMARY_FOOT + m->row_gap;
    }
    out->summary_dot = box(out->summary_text.x - m->dot_gap - m->dot, out->summary_text.y + (out->summary_text.h - m->dot) * 0.5f, m->dot, m->dot);
    f->whole = four_three;
    f->places = ORACLES_ACHIEVEMENTS_ROWS + l->count;
    f->first_y = first_y;
    f->pitch = pitch;
    f->row_min = m->row_min;
    for (unsigned r = 0; r < ORACLES_ACHIEVEMENTS_ROWS; r++) {
        f->row_top[r] = p->rows[r].y;
        f->row_foot[r] = p->rows[r].y + p->rows[r].h;
    }
    f->top = four_three ? fminf(m->panel_y + 1.0f, f->row_top[0]) : f->row_top[0];
    f->foot = four_three ? m->panel_y + 1.0f + PAGE_SIGHT_4_3 : PAGE_FOOT;
    f->clip_top = four_three ? m->panel_y : m->panel_y + m->border;
    f->clip_foot = four_three ? m->panel_y + PAGE_VIEW_4_3 : m->panel_y + m->panel_h - m->border;
    f->above = first_y - 1.0f - m->panel_y;
    const float over = l->count ? first_y + (float)(l->count - 1u) * pitch + m->row_min - f->foot : 0.0f;
    f->max = over > 0.0f ? snap_up(f, over) : 0.0f;
    return 1;
}

/* The scroll shown: the one kept, within the page's ends, brought to the highlight only once none of it shows. */
static float page_view(const page_frame *f, const OraclesHomeNav *nav)
{
    const float scroll = page_clamp(f, nav->achievements.page_scroll);
    return nav->row < f->places && !page_in_clip(f, nav->row, scroll, 1) ? page_follow(f, nav->row, scroll) : scroll;
}

void oracles_ui_layout_achievements_page(OraclesUiLayout layout, const OraclesHomeNav *nav, OraclesUiAchievementsPageLayout *out)
{
    page_frame f;
    if (!page_base(layout, nav, out, &f)) return;
    const int four_three = layout == ORACLES_UI_LAYOUT_4_3;
    const list_metrics *m = four_three ? &list_4_3 : &list_16_9;
    OraclesUiCheatsLayout *p = &out->page;
    const OraclesAchievementsList *l = nav->achievements.list;
    const float inner_x = m->panel_x + m->border + m->pad_x, inner_w = m->panel_w - 2.0f * (m->border + m->pad_x);
    const float scroll = page_view(&f, nav);
    out->scroll = scroll;
    if (four_three) {
        out->clip = box(m->panel_x, m->panel_y, m->panel_w, PAGE_VIEW_4_3);
        /* The help at the foot of the view, whatever scrolls. */
        const float help_y = m->panel_y + PAGE_VIEW_4_3 + PAGE_HELP_4_3, moved = help_y - p->help.y;
        p->help.y += moved; p->help.baseline += moved;
        p->help_note.y += moved; p->help_note.baseline += moved;
    } else {
        p->panel.h = m->panel_h;
        out->clip = box(m->panel_x + m->border, m->panel_y + m->border, m->panel_w - 2.0f * m->border, m->panel_h - 2.0f * m->border);
    }
    page_scrolled(p, scroll);
    move_up(&out->summary_text, scroll);
    out->summary_dot.y -= scroll;
    out->rule.y -= scroll;
    const float top = out->clip.y, bottom = out->clip.y + out->clip.h;
    for (unsigned i = 0; i < l->count && out->shown < ORACLES_UI_ACHIEVEMENTS_SHOWN; i++) {
        const float y = f.first_y + (float)i * f.pitch - scroll;
        if (y + m->row_min <= top) continue;
        if (y >= bottom) break;
        achievement_row(four_three, &l->items[i], i, inner_x, inner_w, y, &out->rows[out->shown++]);
    }
    const OraclesAchievementsItem *focused = oracles_achievements_page_item(nav);
    if (focused && four_three) {
        ellipsis(&oracles_ui_help_4_3, focused->description, ROWS_HELP_W_4_3, out->help, sizeof out->help);
        if (focused->unlocked) snprintf(out->help_note, sizeof out->help_note, "Unlocked%s%s", focused->date[0] ? " " : "", focused->date);
        else if (focused->progress[0]) snprintf(out->help_note, sizeof out->help_note, "Progress %s", focused->progress);
        else snprintf(out->help_note, sizeof out->help_note, "Locked");
    }
}

float oracles_ui_achievements_page_view(const OraclesHomeNav *nav)
{
    static OraclesUiAchievementsPageLayout page;
    page_frame f;
    return page_base(nav->layout, nav, &page, &f) ? page_view(&f, nav) : 0.0f;
}

float oracles_ui_achievements_page_follow(const OraclesHomeNav *nav)
{
    static OraclesUiAchievementsPageLayout page;
    page_frame f;
    return page_base(nav->layout, nav, &page, &f) ? page_follow(&f, nav->row, nav->achievements.page_scroll) : 0.0f;
}

float oracles_ui_achievements_page_moved(const OraclesHomeNav *nav, int steps, unsigned *place)
{
    static OraclesUiAchievementsPageLayout page;
    page_frame f;
    *place = nav->row;
    if (!page_base(nav->layout, nav, &page, &f)) return 0.0f;
    const float shown = page_view(&f, nav);
    float scroll = shown;
    if (f.whole) {
        for (; steps > 0; steps--) scroll = page_clamp(&f, snap_up(&f, scroll + 1.0f));
        for (; steps < 0; steps++) scroll = page_clamp(&f, snap_down(&f, scroll - 1.0f));
    } else {
        scroll = page_clamp(&f, scroll + (float)steps * f.pitch);
    }
    /* The highlight left out of sight by the scroll: the place nearest it that is in. */
    if (scroll != shown && nav->row < f.places && !page_in_clip(&f, nav->row, scroll, 0)) {
        float top, foot;
        place_span(&f, nav->row, &top, &foot);
        if (top - scroll < f.clip_top) {
            for (unsigned p = nav->row + 1u; p < f.places; p++) if (page_in_clip(&f, p, scroll, 0)) { *place = p; break; }
        } else {
            for (unsigned p = nav->row; p-- > 0u;) if (page_in_clip(&f, p, scroll, 0)) { *place = p; break; }
        }
    }
    return scroll;
}

/* ---- an achievement's detail --------------------------------------------------------------------------------- */

/* 16:9: the column under the page's name, 32 under it, down to the panel's foot: the badge beside its points, rarity and tag,
 * then the title, the description and the state, 16 apart.  4:3: the panel over the list, padded 28, 36, 32, its border 2, down
 * to the list's foot at most: the badge beside the title over a row of points, rarity and tag, 24 apart, then the
 * description and the state. */
typedef struct detail_metrics {
    float badge, badge_gap, text_gap, tag_pad_x, tag_pad_y, tag_border, title_line, text_line, dot, dot_gap, bar_gap, bar_w, bar_h, bottom;   /* `bottom` the column's */
} detail_metrics;
static const detail_metrics detail_16_9 = { 128.0f, 24.0f, 6.0f, 12.0f, 2.0f, 1.0f, 49.28f, 32.0f, 12.0f, 12.0f, 8.0f, 110.0f, 6.0f, 1004.0f };
static const detail_metrics detail_4_3 = { 160.0f, 32.0f, 8.0f, 14.0f, 2.0f, 2.0f, 59.4f, 40.0f, 14.0f, 14.0f, 10.0f, 140.0f, 8.0f, 0.0f };
#define DETAIL_GAP 16.0f
#define DETAIL_TOP_GAP 32.0f
#define DETAIL_PANEL_X 64.0f
#define DETAIL_PANEL_Y 208.0f
#define DETAIL_PANEL_W 1312.0f
#define DETAIL_PANEL_PAD_X 36.0f
#define DETAIL_PANEL_PAD_TOP 28.0f
#define DETAIL_PANEL_PAD_BOTTOM 32.0f
#define DETAIL_PANEL_BORDER 2.0f
#define DETAIL_ROW_GAP 24.0f
#define DETAIL_PANEL_FOOT 970.0f
/* A pause too small for the column shows part of the scene, centred: the panel 24 inside it, above the hints, which keep to
 * its foot (their top 55 from the scene's, and 8 more). */
#define DETAIL_VIEW_MARGIN 24.0f
#define DETAIL_VIEW_HINTS 63.0f
#define DETAIL_SHORT_TITLE_W 600.0f

static const char *after_spaces(const char *p)
{
    while (*p == ' ') p++;
    return p;
}

static float part_width(const OraclesUiTextStyle *style, const char *text, size_t length)
{
    char part[ORACLES_UI_DETAIL_LINE_LENGTH];
    snprintf(part, sizeof part, "%.*s", (int)length, text);
    return oracles_ui_text_width(style, part);
}

/* The characters of the first `length` bytes. */
static size_t characters_of(const char *text, size_t length)
{
    size_t n = 0;
    for (size_t i = 0; i < length; i++) n += ((unsigned char)text[i] & 0xc0u) != 0x80u;
    return n;
}

/* Whether `length` bytes from `start` are a line: within its buffer, the glyphs a text draws, and `width`. */
static int line_fits(const OraclesUiTextStyle *style, const char *start, size_t length, float width)
{
    return length < ORACLES_UI_DETAIL_LINE_LENGTH && characters_of(start, length) <= ORACLES_UI_DETAIL_LINE_GLYPHS && part_width(style, start, length) <= width;
}

/* The end of the line from `start`: the last piece that fits, else (a word wider than the line) the last whole character. */
static const char *line_end(const OraclesUiTextStyle *style, const char *start, float width)
{
    const char *end = start, *next = start;
    while (*next) {
        const char *piece = next + oracles_ui_wrap_piece(next);
        if (!line_fits(style, start, (size_t)(piece - start), width)) break;
        end = piece;
        next = after_spaces(piece);
    }
    if (end > start) return end;
    for (const char *p = start; *p;) {
        const char *after = p + 1;
        while (((unsigned char)*after & 0xc0u) == 0x80u) after++;
        if (p > start && !line_fits(style, start, (size_t)(after - start), width)) break;
        end = p = after;
    }
    return end;
}

/* `text` broken into `capacity` lines at most, the last cut with an ellipsis when more is left; how many. */
static unsigned break_lines(const OraclesUiTextStyle *style, const char *text, float width, char lines[][ORACLES_UI_DETAIL_LINE_LENGTH], unsigned capacity)
{
    unsigned count = 0;
    for (const char *start = after_spaces(text); *start && count < capacity; count++) {
        if (count + 1u == capacity) { ellipsis(style, start, width, lines[count], ORACLES_UI_DETAIL_LINE_LENGTH); continue; }
        const char *end = line_end(style, start, width);
        snprintf(lines[count], ORACLES_UI_DETAIL_LINE_LENGTH, "%.*s", (int)(end - start), start);
        start = after_spaces(end);
    }
    return count;
}

/* `text` in lines of `line_height` from (x, y), as text-wrap: pretty breaks it: no last line of a single word when the lines
 * before it can give it another; returns the lines' height. */
static float detail_lines(const OraclesUiTextStyle *style, const char *text, float width, float x, float y, float line_height, char lines[][ORACLES_UI_DETAIL_LINE_LENGTH],
                          OraclesUiLine *out, unsigned capacity, unsigned *count)
{
    *count = break_lines(style, text, width, lines, capacity);
    if (*count >= 2u && *count < capacity && !strchr(lines[*count - 1u], ' ')) {
        char pretty[ORACLES_UI_DETAIL_LINES][ORACLES_UI_DETAIL_LINE_LENGTH];
        if (break_lines(style, text, oracles_ui_text_width(style, lines[*count - 2u]) - 1.0f, pretty, capacity) == *count) memcpy(lines, pretty, sizeof pretty[0] * *count);
    }
    const float pitch = floorf(line_height * 64.0f) / 64.0f;   /* layout units, 1/64 px */
    for (unsigned i = 0; i < *count; i++) out[i] = line_at(style, lines[i], x, y + (float)i * pitch, line_height);
    return (float)*count * pitch;
}

/* The tag's frame and name, with its left at `x` and its top at `y`. */
static void detail_tag(const detail_metrics *m, const OraclesUiTextStyle *style, const char *name, float x, float y, OraclesUiAchievementDetail *out)
{
    out->tag_name = line_at(style, name, x + m->tag_border + m->tag_pad_x, y + m->tag_border + m->tag_pad_y, 0.0f);
    out->tag = box(x, y, out->tag_name.w + 2.0f * (m->tag_border + m->tag_pad_x), out->tag_name.h + 2.0f * (m->tag_border + m->tag_pad_y));
}

static void line_to(OraclesUiLine *line, float x, float y)
{
    line->baseline += y - line->y;
    line->x = x;
    line->y = y;
}

/* The state, its dot before it, and for an achievement in progress its bar and its count. */
static void detail_state(const detail_metrics *m, const OraclesUiTextStyle *st, const OraclesAchievementsItem *item, float x, float y, OraclesUiAchievementDetail *out)
{
    out->state = line_at(&st[ORACLES_UI_ACH_DETAIL_TEXT], out->state_text, x + m->dot + m->dot_gap, y, 0.0f);
    const float middle = y + out->state.h * 0.5f;
    out->state_dot = box(x, middle - m->dot * 0.5f, m->dot, m->dot);
    if (item->unlocked || !item->progress[0]) return;
    out->bar = box(out->state.x + out->state.w + m->dot_gap + m->bar_gap, middle - m->bar_h * 0.5f, m->bar_w, m->bar_h);
    out->progress = line_at(&st[ORACLES_UI_ACH_DETAIL_PROGRESS], item->progress, out->bar.x + m->bar_w + m->dot_gap, 0.0f, 0.0f);
    centre_on(&out->progress, middle);
}

/* Where the panel goes: its left, its top, its width, the foot it stays above. */
typedef struct detail_place { float x, y, w, foot; } detail_place;

/* The points, the rarity and the tag on one row under the title, as many rows as a narrow panel needs. */
#define DETAIL_PIECES 3
static float detail_row_heights(const float *widths, const float *heights, float room, float gap, unsigned *row_of, float *row_h)
{
    float used = 0.0f, total = 0.0f;
    unsigned row = 0;
    for (unsigned i = 0; i < DETAIL_PIECES; i++) {
        if (widths[i] <= 0.0f) continue;
        if (used > 0.0f && used + DETAIL_ROW_GAP + widths[i] > room) { row++; used = 0.0f; }
        used += (used > 0.0f ? DETAIL_ROW_GAP : 0.0f) + widths[i];
        row_of[i] = row;
        row_h[row] = max_f(row_h[row], heights[i]);
    }
    for (unsigned r = 0; r <= row; r++) total += gap + row_h[r];
    return total;
}

static void lay_out_detail(int panel, const detail_place *place, const OraclesAchievementsItem *item, OraclesUiAchievementDetail *out)
{
    memset(out, 0, sizeof *out);
    const detail_metrics *m = panel ? &detail_4_3 : &detail_16_9;
    const OraclesUiTextStyle *st = list_styles[panel];
    const char *tag = oracles_achievements_tag_text(item->tag);
    snprintf(out->points_text, sizeof out->points_text, "%u point%s", item->points, item->points == 1u ? "" : "s");
    oracles_achievements_rarity_text(item->rarity, out->rarity_text, sizeof out->rarity_text);
    oracles_achievements_state_text(item, out->state_text, sizeof out->state_text);
    out->points = line_at(&st[ORACLES_UI_ACH_DETAIL_POINTS], out->points_text, 0.0f, 0.0f, 0.0f);
    if (out->rarity_text[0]) out->rarity = line_at(&st[ORACLES_UI_ACH_DETAIL_RARITY], out->rarity_text, 0.0f, 0.0f, 0.0f);
    const float foot = panel ? place->foot - DETAIL_PANEL_PAD_BOTTOM - DETAIL_PANEL_BORDER : m->bottom;
    float x, width, y;
    if (panel) {
        x = place->x + DETAIL_PANEL_BORDER + DETAIL_PANEL_PAD_X;
        width = place->w - 2.0f * (DETAIL_PANEL_BORDER + DETAIL_PANEL_PAD_X);
        y = place->y + DETAIL_PANEL_BORDER + DETAIL_PANEL_PAD_TOP;
        /* The title and the rows of points, centred on the badge. */
        const float text_x = x + m->badge + m->badge_gap, text_w = x + width - text_x;
        const unsigned title_capacity = text_w < DETAIL_SHORT_TITLE_W ? 2u : ORACLES_UI_DETAIL_TITLE_LINES;   /* the smallest windows keep room for the description */
        const float title_h = detail_lines(&st[ORACLES_UI_ACH_DETAIL_TITLE], item->title, text_w, text_x, 0.0f, m->title_line, out->title_text, out->title, title_capacity, &out->title_lines);
        if (tag) detail_tag(m, &st[ORACLES_UI_ACH_DETAIL_TAG], tag, 0.0f, 0.0f, out);
        const float widths[DETAIL_PIECES] = { out->points.w, out->rarity.w, out->tag.w }, heights[DETAIL_PIECES] = { out->points.h, out->rarity.h, out->tag.h };
        unsigned row_of[DETAIL_PIECES] = { 0u, 0u, 0u };
        float row_h[DETAIL_PIECES] = { 0.0f, 0.0f, 0.0f };
        const float column_h = title_h + detail_row_heights(widths, heights, text_w, m->text_gap, row_of, row_h), top_h = max_f(m->badge, column_h);
        out->badge = box(x, y + (top_h - m->badge) * 0.5f, m->badge, m->badge);
        const float column_y = y + (top_h - column_h) * 0.5f;
        for (unsigned i = 0; i < out->title_lines; i++) line_to(&out->title[i], text_x, out->title[i].y + column_y);
        float row_y = column_y + title_h + m->text_gap, right = text_x;
        unsigned row = 0;
        for (unsigned i = 0; i < DETAIL_PIECES; i++) {
            if (widths[i] <= 0.0f) continue;
            if (row_of[i] != row) { row_y += row_h[row] + m->text_gap; row = row_of[i]; right = text_x; }
            const float piece_y = row_y + (row_h[row] - heights[i]) * 0.5f;
            if (i == 0) line_to(&out->points, right, piece_y);
            else if (i == 1) line_to(&out->rarity, right, piece_y);
            else detail_tag(m, &st[ORACLES_UI_ACH_DETAIL_TAG], tag, right, piece_y, out);
            right += widths[i] + DETAIL_ROW_GAP;
        }
        y += top_h;
    } else {
        OraclesUiPageHead head;
        oracles_ui_layout_page_head(ORACLES_UI_LAYOUT_16_9, "Achievements", "", "", NULL, &head);
        x = HEAD_X;
        width = HEAD_W;
        y = head.section.y + head.section.h + DETAIL_TOP_GAP;
        /* The points, the rarity and the tag beside the badge, centred on it. */
        const float text_x = x + m->badge + m->badge_gap;
        if (tag) detail_tag(m, &st[ORACLES_UI_ACH_DETAIL_TAG], tag, text_x, 0.0f, out);
        const float column_h = out->points.h + (out->rarity_text[0] ? m->text_gap + out->rarity.h : 0.0f) + (tag ? m->text_gap + out->tag.h : 0.0f);
        out->badge = box(x, y, m->badge, m->badge);
        float column_y = y + (m->badge - column_h) * 0.5f;
        line_to(&out->points, text_x, column_y);
        column_y += out->points.h + m->text_gap;
        if (out->rarity_text[0]) {
            line_to(&out->rarity, text_x, column_y);
            column_y += out->rarity.h + m->text_gap;
        }
        if (tag) detail_tag(m, &st[ORACLES_UI_ACH_DETAIL_TAG], tag, text_x, column_y, out);
        y += m->badge + DETAIL_GAP;
        y += detail_lines(&st[ORACLES_UI_ACH_DETAIL_TITLE], item->title, width, x, y, m->title_line, out->title_text, out->title, ORACLES_UI_DETAIL_TITLE_LINES, &out->title_lines);
    }
    y += DETAIL_GAP;
    /* The description in the room the state leaves down to the foot, whatever its length. */
    float state_h, baseline;
    oracles_ui_line_box(&st[ORACLES_UI_ACH_DETAIL_TEXT], 0.0f, &state_h, &baseline);
    const float room = foot - state_h - DETAIL_GAP - y;
    const unsigned fit = room >= m->text_line ? (unsigned)floorf(room / m->text_line) : 1u;
    const float text_h = detail_lines(&st[ORACLES_UI_ACH_DETAIL_TEXT], item->description, width, x, y, m->text_line, out->description_text, out->description,
                                      fit < ORACLES_UI_DETAIL_LINES ? fit : ORACLES_UI_DETAIL_LINES, &out->description_lines);
    if (out->description_lines) y += text_h + DETAIL_GAP;
    detail_state(m, st, item, x, y, out);
    if (panel) out->panel = box(place->x, place->y, place->w, y + out->state.h + DETAIL_PANEL_PAD_BOTTOM + DETAIL_PANEL_BORDER - place->y);
}

void oracles_ui_layout_achievement_detail(OraclesUiLayout layout, const OraclesAchievementsItem *item, OraclesUiAchievementDetail *out)
{
    static const detail_place place = { DETAIL_PANEL_X, DETAIL_PANEL_Y, DETAIL_PANEL_W, DETAIL_PANEL_FOOT };
    lay_out_detail(layout == ORACLES_UI_LAYOUT_4_3, &place, item, out);
}

void oracles_ui_layout_achievement_panel(float view_w, float view_h, const OraclesAchievementsItem *item, OraclesUiAchievementDetail *out)
{
    const float left = (1920.0f - view_w) * 0.5f, top = (ORACLES_UI_SCENE_HEIGHT - view_h) * 0.5f;
    const detail_place place = { left + DETAIL_VIEW_MARGIN, top + DETAIL_VIEW_MARGIN, view_w - 2.0f * DETAIL_VIEW_MARGIN, top + view_h - DETAIL_VIEW_HINTS };
    lay_out_detail(1, &place, item, out);
}

void oracles_ui_achievement_detail_of(const OraclesHomeNav *nav, OraclesUiAchievementDetail *out)
{
    const OraclesHomeAchievements *a = &nav->achievements;
    const OraclesAchievementsItem *item = oracles_achievements_detail_item(nav);
    if (a->view_w > 0.0f && nav->layout != ORACLES_UI_LAYOUT_4_3) oracles_ui_layout_achievement_panel(a->view_w, a->view_h, item, out);
    else oracles_ui_layout_achievement_detail(nav->layout, item, out);
}

unsigned oracles_achievements_list_first(const OraclesHomeNav *nav)
{
    return place_of_list(nav->layout == ORACLES_UI_LAYOUT_4_3, nav).first;
}

unsigned oracles_achievements_list_fit(const OraclesHomeNav *nav)
{
    return place_of_list(nav->layout == ORACLES_UI_LAYOUT_4_3, nav).fit;
}

static int inside(const OraclesUiBox *b, float x, float y) { return x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h; }

int oracles_ui_achievements_detail_hit(const OraclesHomeNav *nav, float x, float y)
{
    const OraclesAchievementsItem *item = oracles_achievements_detail_item(nav);
    if (!item) return 0;
    static OraclesUiAchievementDetail detail;
    oracles_ui_achievement_detail_of(nav, &detail);
    return inside(&detail.panel, x, y);
}

int oracles_ui_achievements_list_hit(const OraclesHomeNav *nav, float x, float y)
{
    const int four_three = nav->layout == ORACLES_UI_LAYOUT_4_3;
    const list_place p = place_of_list(four_three, nav);
    if (!inside(&p.list, x, y) || x >= p.list.x + p.row_w) return -1;
    const unsigned row = (unsigned)((y - p.list.y) / p.pitch), index = p.first + row;
    const OraclesAchievementsList *l = nav->achievements.list;
    const float row_h = (four_three ? &list_4_3 : &list_16_9)->row_min;
    if (!l || index >= l->count || y - p.list.y - (float)row * p.pitch >= row_h) return -1;   /* past the last, or in the gap under a row */
    return (int)index;
}

int oracles_ui_achievements_page_hit(const OraclesHomeNav *nav, float x, float y, int *option)
{
    static OraclesUiAchievementsPageLayout page;
    oracles_ui_layout_achievements_page(nav->layout, nav, &page);
    const OraclesUiCheatsLayout *l = &page.page;
    *option = -1;
    if (page.list && !inside(&page.clip, x, y)) return -1;
    for (int c = 0; c < 2; c++) if (inside(&l->choices[ORACLES_ACHIEVEMENTS_ROW_ON][c].box, x, y)) { *option = c; return ORACLES_ACHIEVEMENTS_ROW_ON; }
    for (int c = 0; c < 2; c++) if (inside(&l->choices[ORACLES_ACHIEVEMENTS_ROW_SOUND][c].box, x, y)) { *option = c; return ORACLES_ACHIEVEMENTS_ROW_SOUND; }
    if (inside(&l->action_boxes[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT], x, y)) { *option = 2; return ORACLES_ACHIEVEMENTS_ROW_ACCOUNT; }
    if (inside(&l->action_boxes[ORACLES_ACHIEVEMENTS_ROW_PRIVACY], x, y)) { *option = 2; return ORACLES_ACHIEVEMENTS_ROW_PRIVACY; }
    for (int r = 0; r < ORACLES_ACHIEVEMENTS_ROWS; r++) if (inside(&l->rows[r], x, y)) return r;
    for (unsigned i = 0; i < page.shown; i++) if (inside(&page.rows[i].row, x, y)) return ORACLES_ACHIEVEMENTS_ROWS + (int)page.rows[i].index;
    return -1;
}
