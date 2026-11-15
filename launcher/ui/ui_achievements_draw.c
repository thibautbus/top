/* RetroAchievements' screens drawn (ui_achievements_draw.h): the login and its keyboard, the pause's list with the badges'
 * images, the toasts over a game. */
#include "ui_achievements_draw.h"

#include <stdio.h>
#include <string.h>

#define DIM 0x050408u
#define DIM_OPACITY 0.66f
#define DIALOG 0x0a090du
#define DIALOG_OPACITY 0.97f
#define DIALOG_BORDER_OPACITY 0.12f
#define FIELD_BORDER_OPACITY 0.25f
#define FIELD_FILL_OPACITY 0.04f
#define LABEL 0xa19eaau
#define TEXT 0xefedf2u
#define PLACEHOLDER 0x5d5a66u
#define BUTTON 0xdcd9e1u
#define BUSY_OPACITY 0.45f
#define KEYBOARD 0x15141au
#define KEY_FILL_OPACITY 0.06f
#define KEY_BORDER_OPACITY 0.1f
#define KEY_FOCUSED_TEXT 0x0c0b0fu
#define TOAST 0x1c1b22u
#define TOAST_BORDER_OPACITY 0.1f
#define TOAST_OVER 0xa19eaau
#define PANEL 0x0a090du
#define PANEL_OPACITY 0.92f
#define PANEL_BORDER_OPACITY 0.07f
#define SUMMARY_RULE_OPACITY 0.08f
#define ROW_HIGHLIGHT_OPACITY 0.055f
#define TITLE_UNLOCKED 0xefedf2u
#define TITLE_LOCKED 0xc9c6cfu
#define DESCRIPTION 0xa19eaau
#define POINTS_UNLOCKED 0xd6d3dcu
#define POINTS_LOCKED 0x8f8c98u
#define DATE 0x8f8c98u
#define PROGRESS_TEXT 0xc9c6cfu
#define BAR_TRACK_OPACITY 0.12f
#define PLACEHOLDER_BORDER_OPACITY 0.22f
#define SCROLL_TRACK_OPACITY 0.06f
#define SCROLL_THUMB_OPACITY 0.3f
#define HEAD_OVER 0xd6d3dcu
#define HEAD_TITLE 0xf3f1f5u
#define MODE 0xa19eaau

static const uint32_t tone_dots[4] = { 0x76cf8au, 0xebbd57u, 0xf97770u, 0x6f6c78u };
static const uint32_t tone_texts[4] = { 0xd6d3dcu, 0xecd198u, 0xf9aea7u, 0xa19eaau };

static void text(OraclesUiDraw *draw, const OraclesUiTextStyle *style, const OraclesUiLine *line, const char *s, OraclesUiColor color)
{
    if (s && s[0] && line->h > 0.0f) oracles_ui_draw_text(draw, style, line->x, line->baseline, s, color);
}

static void fill(OraclesUiDraw *draw, const OraclesUiBox *b, float radius, OraclesUiColor color)
{
    if (b->w > 0.0f && b->h > 0.0f) oracles_ui_fill_round_rect(draw, b->x, b->y, b->w, b->h, radius, color);
}

static void stroke(OraclesUiDraw *draw, const OraclesUiBox *b, float radius, float width, OraclesUiColor color)
{
    if (b->w > 0.0f && b->h > 0.0f) oracles_ui_stroke_round_rect(draw, b->x, b->y, b->w, b->h, radius, width, color);
}

/* ---- the badges ------------------------------------------------------------------------------------------- */

/* The badges' images loaded once each from their files, again a while later when a file was not there yet (still being
 * downloaded), for the renderer they were made for. */
#define BADGES 64
#define BADGE_RETRY_MS 2000.0
typedef struct badge_entry {
    char path[4200];
    SDL_Texture *texture;
    double tried_ms;
} badge_entry;
static badge_entry badges[BADGES];
static unsigned badges_next;
static SDL_Renderer *badges_renderer;

void oracles_ui_achievements_forget_badges(void)
{
    for (unsigned i = 0; i < BADGES; i++) {
        if (badges[i].texture) SDL_DestroyTexture(badges[i].texture);
        memset(&badges[i], 0, sizeof badges[i]);
    }
    badges_next = 0;
    badges_renderer = NULL;
}

SDL_Texture *oracles_ui_achievements_badge(OraclesUiDraw *draw, const char *folder, const char *name, int locked, double now_ms)
{
    if (!folder || !folder[0] || !name || !name[0]) return NULL;
    SDL_Renderer *renderer = oracles_ui_draw_renderer(draw);
    if (renderer != badges_renderer) { oracles_ui_achievements_forget_badges(); badges_renderer = renderer; }
    char path[4200];
    snprintf(path, sizeof path, "%s/%s%s.png", folder, name, locked ? "_lock" : "");
    badge_entry *entry = NULL;
    for (unsigned i = 0; i < BADGES && !entry; i++) if (badges[i].path[0] && !strcmp(badges[i].path, path)) entry = &badges[i];
    if (entry && (entry->texture || now_ms - entry->tried_ms < BADGE_RETRY_MS)) return entry->texture;
    if (!entry) {
        entry = &badges[badges_next];
        badges_next = (badges_next + 1u) % BADGES;
        if (entry->texture) SDL_DestroyTexture(entry->texture);
        memset(entry, 0, sizeof *entry);
        snprintf(entry->path, sizeof entry->path, "%s", path);
    }
    entry->tried_ms = now_ms;
    SDL_Surface *surface = SDL_LoadPNG(path);
    if (!surface) return NULL;
    entry->texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    return entry->texture;
}

/* ---- the login -------------------------------------------------------------------------------------------- */

void oracles_ui_achievements_login_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent)
{
    const OraclesHomeAchievements *a = &nav->achievements;
    if (!a->login) return;
    const OraclesUiLayout layout = nav->layout;
    const int four_three = layout == ORACLES_UI_LAYOUT_4_3;
    OraclesUiLoginLayout l;
    oracles_ui_layout_login(layout, nav, &l);
    /* The page dimmed, the whole scene. */
    oracles_ui_fill_rect(draw, -4000.0f, -4000.0f, 10000.0f, 10000.0f, oracles_ui_rgba(DIM, DIM_OPACITY));
    fill(draw, &l.dialog, 10.0f, oracles_ui_rgba(DIALOG, DIALOG_OPACITY));
    stroke(draw, &l.dialog, 10.0f, 1.0f, oracles_ui_rgba(0xffffffu, DIALOG_BORDER_OPACITY));
    text(draw, oracles_ui_login_style(layout, ORACLES_UI_LOGIN_TITLE), &l.title, "Log in to RetroAchievements", accent);
    const int busy = a->login == ORACLES_LOGIN_LOGGING;
    static const char *const labels[2] = { "User name", "Password" };
    for (int f = 0; f < 2; f++) {
        const OraclesUiLoginField *field = &l.fields[f];
        const int focused = a->field == f;
        text(draw, oracles_ui_login_style(layout, ORACLES_UI_LOGIN_LABEL), &field->label, labels[f], oracles_ui_rgb(LABEL));
        fill(draw, &field->frame, 6.0f, oracles_ui_rgba(0xffffffu, FIELD_FILL_OPACITY));
        stroke(draw, &field->frame, 6.0f, focused ? (four_three ? 3.0f : 2.0f) : 1.0f, focused ? accent : oracles_ui_rgba(0xffffffu, FIELD_BORDER_OPACITY));
        OraclesUiTextStyle value = *oracles_ui_login_style(layout, ORACLES_UI_LOGIN_VALUE);
        if (f) value.tracking = 0.12f;
        int saved;
        SDL_Rect clip;
        oracles_ui_draw_clip_rect(draw, &saved, &clip);
        oracles_ui_draw_clip(draw, field->clip.x, field->clip.y, field->clip.w, field->clip.h);
        text(draw, &value, &field->value, f ? l.masked : a->name, oracles_ui_rgb(TEXT));
        text(draw, oracles_ui_login_style(layout, ORACLES_UI_LOGIN_VALUE), &field->placeholder, "As on retroachievements.org", oracles_ui_rgb(PLACEHOLDER));
        fill(draw, &field->caret, 0.0f, accent);
        oracles_ui_draw_restore_clip(draw, saved, &clip);
    }
    text(draw, oracles_ui_login_style(layout, ORACLES_UI_LOGIN_NOTE), &l.note, "Sent once over HTTPS; only the token the server returns is kept.",
         oracles_ui_rgb(LABEL));
    OraclesTone tone;
    const char *state = oracles_achievements_login_state_text(nav, &tone);
    if (state) {
        fill(draw, &l.state_dot, l.state_dot.w * 0.5f, oracles_ui_rgb(tone_dots[tone]));
        text(draw, oracles_ui_login_style(layout, ORACLES_UI_LOGIN_STATE), &l.state_text, state, oracles_ui_rgb(tone_texts[tone]));
    }
    static const char *const names[2] = { "Log in", "Cancel" };
    for (int b = 0; b < 2; b++) {
        if (l.buttons[b].w <= 0.0f) continue;
        const int focused = a->field == ORACLES_LOGIN_OK + b;
        const float opacity = busy && b == 0 ? BUSY_OPACITY : 1.0f;   /* Cancel gives the login up */
        stroke(draw, &l.buttons[b], 5.0f, four_three ? 2.0f : 1.0f, oracles_ui_fade(focused ? accent : oracles_ui_rgba(0xffffffu, FIELD_BORDER_OPACITY), opacity));
        text(draw, oracles_ui_login_style(layout, ORACLES_UI_LOGIN_BUTTON), &l.button_names[b], names[b], oracles_ui_fade(focused ? accent : oracles_ui_rgb(BUTTON), opacity));
    }
    if (l.keyboard.w <= 0.0f) return;
    /* The launcher's keyboard: the key highlighted filled with the accent, Shift on framed in it. */
    fill(draw, &l.keyboard, 10.0f, oracles_ui_rgb(KEYBOARD));
    stroke(draw, &l.keyboard, 10.0f, 1.0f, oracles_ui_rgba(0xffffffu, DIALOG_BORDER_OPACITY));
    for (unsigned r = 0; r < ORACLES_KEYBOARD_ROWS; r++)
        for (unsigned k = 0; k < l.key_count[r]; k++) {
            const char *key = l.key_texts[r][k];
            const int focused = (int)r == a->key_row && (int)k == a->key_column, shift = !strcmp(key, "Shift") && a->keyboard_shift;
            fill(draw, &l.keys[r][k], 6.0f, focused ? accent : oracles_ui_rgba(0xffffffu, KEY_FILL_OPACITY));
            if (!focused) stroke(draw, &l.keys[r][k], 6.0f, shift ? 2.0f : 1.0f, shift ? accent : oracles_ui_rgba(0xffffffu, KEY_BORDER_OPACITY));
            const int word = strlen(key) > 1 && strcmp(key, "\xe2\x8c\xab") != 0;
            text(draw, oracles_ui_login_style(layout, word ? ORACLES_UI_LOGIN_KEY_WORD : ORACLES_UI_LOGIN_KEY), &l.key_names[r][k], key,
                 focused ? oracles_ui_rgb(KEY_FOCUSED_TEXT) : shift ? accent : oracles_ui_rgb(TEXT));
        }
}

static int inside(const OraclesUiBox *b, float x, float y) { return x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h; }

int oracles_ui_achievements_login_hit(const OraclesHomeNav *nav, float x, float y)
{
    OraclesUiLoginLayout l;
    oracles_ui_layout_login(nav->layout, nav, &l);
    for (int f = 0; f < 2; f++) if (inside(&l.fields[f].box, x, y)) return f;
    for (int b = 0; b < 2; b++) if (inside(&l.buttons[b], x, y)) return ORACLES_LOGIN_OK + b;
    for (unsigned r = 0; r < ORACLES_KEYBOARD_ROWS; r++)
        for (unsigned k = 0; k < l.key_count[r]; k++) if (inside(&l.keys[r][k], x, y)) return ORACLES_LOGIN_FIELDS + (int)(r * ORACLES_KEYBOARD_COLUMNS + k);
    return -1;
}

/* ---- the list ------------------------------------------------------------------------------------------- */

/* The list laid out again only when what it shows changes: its texts cut to their rows once. */
typedef struct list_key {
    OraclesUiLayout layout;
    const OraclesAchievementsList *list;
    unsigned generation, focus;
    int suspended, hero;
} list_key;
static list_key laid_key;
static OraclesUiAchievementsLayout laid;
static int laid_once, badges_waiting;

int oracles_ui_achievements_list_waiting(void) { return badges_waiting; }

void oracles_ui_achievements_list_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent, double now_ms)
{
    const OraclesUiLayout layout = nav->layout;
    const int four_three = layout == ORACLES_UI_LAYOUT_4_3;
    const OraclesUiPageStyles *st = oracles_ui_page_styles(layout);
    list_key key;
    memset(&key, 0, sizeof key);
    key.layout = layout;
    key.list = nav->achievements.list;
    key.generation = key.list ? key.list->generation : 0u;
    key.focus = nav->achievements.list_focus;
    key.suspended = nav->achievements.suspended;
    key.hero = (int)oracles_home_hero(nav);
    if (!laid_once || memcmp(&key, &laid_key, sizeof key) != 0) {
        oracles_ui_layout_achievements_list(layout, nav, &laid);
        laid_key = key;
        laid_once = 1;
    }
    const OraclesUiAchievementsLayout *l = &laid;
    const OraclesHomeHero hero = oracles_home_hero(nav);
    text(draw, st->section, &l->head.section, "Achievements", accent);
    if (!four_three) text(draw, st->over, &l->head.over, oracles_home_over(hero), oracles_ui_rgb(HEAD_OVER));
    for (unsigned i = 0; i < l->head.title.count; i++) text(draw, st->title, &l->head.title.lines[i], l->head.title.text[i], oracles_ui_rgb(HEAD_TITLE));
    for (unsigned i = 0; i < l->mode.count; i++) text(draw, &oracles_ui_row_text, &l->mode.lines[i], l->mode.text[i], oracles_ui_rgb(MODE));
    if (!four_three) {
        fill(draw, &l->panel, 10.0f, oracles_ui_rgba(PANEL, PANEL_OPACITY));
        stroke(draw, &l->panel, 10.0f, 1.0f, oracles_ui_rgba(0xffffffu, PANEL_BORDER_OPACITY));
        oracles_ui_fill_rect(draw, l->summary.x, l->summary.y + l->summary.h - 1.0f, l->summary.w, 1.0f, oracles_ui_rgba(0xffffffu, SUMMARY_RULE_OPACITY));
    }
    fill(draw, &l->summary_dot, l->summary_dot.w * 0.5f, oracles_ui_rgb(tone_dots[l->warning ? ORACLES_TONE_WARN : ORACLES_TONE_OK]));
    text(draw, oracles_ui_achievements_style(layout, ORACLES_UI_ACH_SUMMARY), &l->summary_text, l->summary_line,
         oracles_ui_rgb(l->warning ? tone_texts[ORACLES_TONE_WARN] : TEXT));
    const OraclesAchievementsList *list = nav->achievements.list;
    int saved;
    SDL_Rect clip;
    oracles_ui_draw_clip_rect(draw, &saved, &clip);
    oracles_ui_draw_clip(draw, l->list.x, l->list.y, l->list.w, l->list.h);
    const float radius = four_three ? 8.0f : 6.0f, badge_radius = four_three ? 7.0f : 6.0f;
    badges_waiting = 0;
    for (unsigned i = 0; i < l->shown && list; i++) {
        const OraclesUiAchievementRow *row = &l->rows[i];
        const OraclesAchievementsItem *item = &list->items[row->index];
        if (row->index == nav->achievements.list_focus) fill(draw, &row->row, radius, oracles_ui_rgba(0xffffffu, ROW_HIGHLIGHT_OPACITY));
        /* The badge: in colour once unlocked, its locked image otherwise, a dashed frame until it is downloaded. */
        SDL_Texture *badge = oracles_ui_achievements_badge(draw, list->badges, item->badge, !item->unlocked, now_ms);
        if (!badge && item->badge[0]) badges_waiting = 1;
        if (badge) oracles_ui_draw_texture(draw, badge, row->badge.x, row->badge.y, row->badge.w, row->badge.h, 1.0f);
        else oracles_ui_stroke_dashed_rect(draw, row->badge.x, row->badge.y, row->badge.w, row->badge.h, badge_radius, 2.0f,
                                           oracles_ui_rgba(0xffffffu, PLACEHOLDER_BORDER_OPACITY));
        text(draw, oracles_ui_achievements_style(layout, ORACLES_UI_ACH_TITLE), &row->title, row->title_text, oracles_ui_rgb(item->unlocked ? TITLE_UNLOCKED : TITLE_LOCKED));
        text(draw, oracles_ui_achievements_style(layout, ORACLES_UI_ACH_DESCRIPTION), &row->description, row->description_text, oracles_ui_rgb(DESCRIPTION));
        text(draw, oracles_ui_achievements_style(layout, ORACLES_UI_ACH_POINTS), &row->points, row->points_text, oracles_ui_rgb(item->unlocked ? POINTS_UNLOCKED : POINTS_LOCKED));
        text(draw, oracles_ui_achievements_style(layout, ORACLES_UI_ACH_DATE), &row->date, row->date_text, oracles_ui_rgb(DATE));
        text(draw, oracles_ui_achievements_style(layout, ORACLES_UI_ACH_PROGRESS), &row->progress, row->date_text,
             oracles_ui_rgb(four_three ? DATE : PROGRESS_TEXT));
        if (row->bar.w > 0.0f) {
            fill(draw, &row->bar, row->bar.h * 0.5f, oracles_ui_rgba(0xffffffu, BAR_TRACK_OPACITY));
            const OraclesUiBox done = { row->bar.x, row->bar.y, row->bar.w * (item->percent < 1.0f ? item->percent : 1.0f), row->bar.h };
            oracles_ui_fill_rect(draw, done.x, done.y, done.w, done.h, accent);
        }
    }
    oracles_ui_draw_restore_clip(draw, saved, &clip);
    fill(draw, &l->track, l->track.w * 0.5f, oracles_ui_rgba(0xffffffu, SCROLL_TRACK_OPACITY));
    fill(draw, &l->thumb, l->thumb.w * 0.5f, oracles_ui_rgba(0xffffffu, SCROLL_THUMB_OPACITY));
}

/* ---- the toasts ------------------------------------------------------------------------------------------ */

void oracles_ui_achievements_toast_draw(OraclesUiDraw *draw, OraclesUiLayout layout, OraclesAchievementsToast *toast, const char *badges_folder,
                                        float left, float bar_bottom, int reminder, float opacity, double now_ms)
{
    /* With its badge or without, as its first frame found the image: its text never moves while it shows. */
    SDL_Texture *badge = toast->badge[0] && toast->badge_shown >= 0 ? oracles_ui_achievements_badge(draw, badges_folder, toast->badge, 0, now_ms) : NULL;
    const int with_badge = oracles_achievements_toast_badge(toast, badge != NULL);
    OraclesUiAchievementToastLayout l;
    oracles_ui_layout_achievement_toast(layout, toast, with_badge, left, bar_bottom, reminder, &l);
    fill(draw, &l.box, l.radius, oracles_ui_rgba(TOAST, opacity));
    stroke(draw, &l.box, l.radius, 1.0f, oracles_ui_rgba(0xffffffu, TOAST_BORDER_OPACITY * opacity));
    if (badge) oracles_ui_draw_texture(draw, badge, l.badge.x, l.badge.y, l.badge.w, l.badge.h, opacity);
    text(draw, oracles_ui_toast_style(layout, 1), &l.over, toast->over, oracles_ui_rgba(TOAST_OVER, opacity));
    text(draw, oracles_ui_toast_style(layout, 0), &l.text, toast->text, oracles_ui_rgba(TEXT, opacity));
}
