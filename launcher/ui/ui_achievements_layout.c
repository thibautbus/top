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
} login_metrics;
static const login_metrics login_16_9 = { 170.0f, 36.0f, 24.0f, 880.0f, 40.0f, 28.0f, 14.0f, 6.0f, 64.0f, 18.0f, 34.0f, 2.0f, 32.0f, 12.0f, 12.0f,
                                          150.0f, 52.0f, 22.0f, 12.0f, 1.0f, 2.0f, 560.0f, 84.0f, 64.0f, 6.0f, 12.0f };
static const login_metrics login_4_3 = { 150.0f, 36.0f, 24.0f, 1040.0f, 44.0f, 30.0f, 14.0f, 6.0f, 80.0f, 18.0f, 44.0f, 2.0f, 40.0f, 12.0f, 12.0f,
                                         200.0f, 80.0f, 22.0f, 12.0f, 2.0f, 3.0f, 492.0f, 112.0f, 84.0f, 8.0f, 14.0f };
#define DIALOG_BORDER 1.0f
#define FIELD_BORDER 1.0f
#define KEYBOARD_BORDER 1.0f
#define MASK_DOT "\xe2\x80\xa2"   /* • */
#define MASK_SPACING 0.12f        /* the dots' letter spacing, em */

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
    if (!strcmp(k, "Space")) return 3.0f;
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
    /* The password's dots, spaced as the browser spaces letters. */
    size_t n = 0;
    for (const char *c = a->password; *c && n + 4 < sizeof out->masked; c++)
        if (((unsigned char)*c & 0xc0u) != 0x80u) { memcpy(out->masked + n, MASK_DOT, 3); n += 3; }
    out->masked[n] = 0;
    for (int f = 0; f < 2; f++) {
        OraclesUiLoginField *field = &out->fields[f];
        const int focused = a->field == f;
        field->label = line_at(&st[ORACLES_UI_LOGIN_LABEL], labels[f], x, y, 0.0f);
        field->frame = box(x, y + field->label.h + m->label_gap, inner_w, m->field_h);
        field->box = box(x, y, inner_w, field->frame.y + field->frame.h - y);
        const float border = focused ? m->focus_border : FIELD_BORDER, middle = field->frame.y + m->field_h * 0.5f;
        const char *value = f ? out->masked : a->name;
        OraclesUiTextStyle masked = st[ORACLES_UI_LOGIN_VALUE];
        if (f) masked.tracking = MASK_SPACING;
        field->value = line_at(&masked, value, field->frame.x + border + m->field_pad, 0.0f, 0.0f);
        centre_on(&field->value, middle);
        if (!value[0] && !focused && !f) {
            field->placeholder = line_at(&st[ORACLES_UI_LOGIN_VALUE], placeholder, field->value.x, 0.0f, 0.0f);
            centre_on(&field->placeholder, middle);
        }
        if (focused && !busy) field->caret = box(field->value.x + field->value.w + m->caret_gap, middle - m->caret_h * 0.5f, 2.0f, m->caret_h);
        /* A text longer than the field: drawn within it, its end and the caret kept in sight. */
        field->clip = box(field->frame.x + border, field->frame.y + border, field->frame.w - 2.0f * border, field->frame.h - 2.0f * border);
        const float end = field->caret.w > 0.0f ? field->caret.x + field->caret.w : field->value.x + field->value.w;
        const float over = end - (field->frame.x + field->frame.w - border - m->field_pad);
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
      { ORACLES_UI_FONT_SERIF, 25.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SANS, 22.0f, 0.0f, 0 }, { ORACLES_UI_FONT_MONO, 20.0f, 0.0f, 0 } },
    { { ORACLES_UI_FONT_SANS, 30.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SANS_MEDIUM, 36.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SANS, 30.0f, 0.0f, 0 },
      { ORACLES_UI_FONT_SERIF, 30.0f, 0.0f, 0 }, { ORACLES_UI_FONT_SANS, 27.0f, 0.0f, 0 }, { ORACLES_UI_FONT_MONO, 27.0f, 0.0f, 0 } },
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

/* `text` cut with an ellipsis where it is wider than `width`, as text-overflow: ellipsis cuts it. */
static void ellipsis(const OraclesUiTextStyle *style, const char *text, float width, char *out, size_t capacity)
{
    snprintf(out, capacity, "%s", text);
    if (oracles_ui_text_width(style, out) <= width) return;
    size_t n = strlen(out);
    while (n > 0) {
        do n--; while (n > 0 && ((unsigned char)out[n] & 0xc0u) == 0x80u);
        char trial[512];
        snprintf(trial, sizeof trial, "%.*s\xe2\x80\xa6", (int)n, text);
        if (oracles_ui_text_width(style, trial) <= width || n == 0) { oracles_ui_copy_whole(out, capacity, trial); return; }
    }
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
    float list_y;
    const float inner_x = m->panel_x + m->border + m->pad_x, inner_w = m->panel_w - 2.0f * (m->border + m->pad_x);
    if (four_three) {
        /* The summary on the head's third line, its dot before it. */
        const float y = out->head.title.lines[0].y + out->head.title.h + HEAD_GAP_4_3;
        out->summary_text = line_at(&st[ORACLES_UI_ACH_SUMMARY], out->summary_line, m->panel_x + m->dot + m->dot_gap, y, 0.0f);
        out->summary = box(m->panel_x, y, m->panel_w, out->summary_text.h);
        list_y = m->panel_y;
    } else {
        oracles_ui_wrap_pretty(&oracles_ui_row_text, "RetroAchievements, casual mode: every launcher feature stays, savestates included.", HEAD_W, HEAD_X,
                               out->head.title.lines[0].y + out->head.title.h + MODE_GAP, &out->mode);
        for (unsigned i = 0; i < out->mode.count; i++)
            out->mode.lines[i] = line_at(&oracles_ui_row_text, out->mode.text[i], HEAD_X, out->head.title.lines[0].y + out->head.title.h + MODE_GAP + (float)i * MODE_LINE, MODE_LINE);
        out->mode.h = (float)out->mode.count * MODE_LINE;
        const float y = m->panel_y + m->border + m->pad_y;
        out->summary_text = line_at(&st[ORACLES_UI_ACH_SUMMARY], out->summary_line, inner_x + m->summary_pad_x + m->dot + m->dot_gap, y, 0.0f);
        out->summary = box(inner_x, y, inner_w, out->summary_text.h + m->summary_pad_bottom + SUMMARY_BORDER);
        list_y = out->summary.y + out->summary.h + m->summary_gap;
    }
    out->summary_dot = box(out->summary_text.x - m->dot_gap - m->dot, out->summary_text.y + (out->summary_text.h - m->dot) * 0.5f, m->dot, m->dot);
    const float list_bottom = m->panel_y + m->panel_h - m->border - m->pad_y;
    out->list = box(inner_x, list_y, inner_w, list_bottom - list_y);
    /* Rows of one height (one line each: title and description cut), scrolled by whole rows: the highlight past the
     * sixth (4:3: the fourth) moves them, until the last fills the list. */
    const OraclesAchievementsList *l = nav->achievements.list;
    const float row_w = inner_w - m->scroll_gap, pitch = m->row_min + m->row_gap;
    const unsigned count = l ? l->count : 0u, keep = four_three ? 4u : 6u, focus = nav->achievements.list_focus;
    const unsigned fit = (unsigned)floorf((out->list.h + m->row_gap) / pitch), last_first = count > fit ? count - fit : 0u;
    const unsigned first = focus > keep ? (focus - keep < last_first ? focus - keep : last_first) : 0u;
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
        const OraclesAchievementsItem *item = &l->items[i];
        OraclesUiAchievementRow *row = &out->rows[out->shown++];
        row->index = i;
        row->row = box(inner_x, y, row_w, m->row_min);
        const float in_y = y + m->row_pad_y, in_h = m->row_min - 2.0f * m->row_pad_y, x = inner_x + m->row_pad_x;
        row->badge = box(x, in_y + (in_h - m->badge) * 0.5f, m->badge, m->badge);
        /* The points' column: the points, then the date, the progress or (4:3) "Locked", right-aligned. */
        const float right = inner_x + row_w - m->row_pad_x;
        snprintf(row->points_text, sizeof row->points_text, "%u point%s", item->points, item->points == 1u ? "" : "s");
        row->points = line_at(&st[ORACLES_UI_ACH_POINTS], row->points_text, 0.0f, 0.0f, 0.0f);
        row->points.x = right - row->points.w;
        const OraclesUiTextStyle *second = &st[ORACLES_UI_ACH_DATE];
        if (item->unlocked) snprintf(row->date_text, sizeof row->date_text, four_three ? "%s" : "Unlocked %s", item->date);
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
}

unsigned oracles_achievements_list_first(const OraclesHomeNav *nav)
{
    static OraclesUiAchievementsLayout l;
    oracles_ui_layout_achievements_list(nav->layout, nav, &l);
    return l.first;
}
