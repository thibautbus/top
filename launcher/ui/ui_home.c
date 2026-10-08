#include "ui_home.h"

#include "ui_controls.h"
#include "ui_home_layout.h"
#include "ui_page.h"

#include <stdio.h>
#include <string.h>

/* The transitions: the title's opacity (.4s, ease) and slide
 * (.5s, cubic-bezier(.2,.7,.2,1)) from 48 px on the right, the motifs'
 * opacity (.45s), the highlight (.15s) and the toast (.25s, shown long
 * enough to read the loader's reason). */
#define HERO_FADE_MS 400.0f
#define HERO_SLIDE_MS 500.0f
#define HERO_SLIDE 48.0f
#define MOTIF_FADE_MS 450.0f
#define HIGHLIGHT_MS 150.0f
#define TOAST_FADE_MS 250.0f
/* Behind a page the motifs slide left (.55s, the swift curve) and fade to half (.4s). */
#define MOTIF_SLIDE_MS 550.0f
#define MOTIF_LAYER_MS 400.0f
#define PAGE_MOTIF_OPACITY 0.5f
#define CONTROLS_MOTIF_OPACITY 0.35f   /* behind Controls' grids */
#define VERSION_GAP 24.0f               /* the least room between the version and the help bar */
#define TOAST_SHOWN_MS 4000.0

/* The home screen's colours, converted once from oklch where they were given in oklch. */
#define DIAGONAL 0x08080au
#define DIAGONAL_OPACITY 0.94f
#define OTHER_OVER 0x6f6c78u
#define OTHER_TITLE 0x8a8793u
#define OTHER_STATE 0x7d7a86u
#define HERO_OVER 0xd6d3dcu
#define HERO_TITLE 0xf3f1f5u
#define HERO_STATE 0xa19eaau
#define PAUSED_STATE 0xc9c6cfu        /* the pause menu's state and notes, over the game's image */
#define ITEM_NOTE 0xa19eaau
#define ITEM_TEXT 0xdcd9e1u
#define ITEM_DISABLED 0x58555fu
#define ITEM_DISABLED_FOCUSED 0x77747fu
#define ITEM_HIGHLIGHT 0xffffffu
#define ITEM_HIGHLIGHT_OPACITY 0.07f
#define VERSION_TEXT 0x8a8793u
#define HINT_BACKGROUND 0x060609u
#define HINT_BACKGROUND_OPACITY 0.55f
#define HINT_KEY 0xd6d3dcu
#define HINT_KEY_BORDER 0x4a4752u
#define HINT_LABEL 0xa19eaau
#define TOAST_BACKGROUND 0x1c1b22u
#define TOAST_BORDER 0xffffffu
#define TOAST_BORDER_OPACITY 0.1f
#define TOAST_TEXT 0xefedf2u
#define TAB_CHOSEN 0xf3f1f5u
#define TAB_OTHER 0x8a8793u

/* What differs between the layouts in the drawing: corners, the help bar's background, the pause's notes. */
typedef struct look {
    float item_radius, reason_radius, hint_radius, key_radius, toast_radius;
    float hint_opacity;
    uint32_t paused_note;
} look;
static const look look_16_9 = { 6.0f, 5.0f, 6.0f, 4.0f, 8.0f, HINT_BACKGROUND_OPACITY, PAUSED_STATE };
static const look look_4_3 = { 8.0f, 6.0f, 9.0f, 5.0f, 9.0f, 0.6f, ITEM_NOTE };

static const look *look_of(OraclesUiLayout layout) { return layout == ORACLES_UI_LAYOUT_4_3 ? &look_4_3 : &look_16_9; }

/* The accent of each motif: oklch(0.8 0.11 hue) for hues 230, 70 and 320. */
static const uint32_t accents[ORACLES_UI_MOTIF_COUNT] = { 0x6bcbf7u, 0xebb16cu, 0xdda7eau };
static const float diagonal[8] = { 0.0f, 0.0f, 1010.0f, 0.0f, 430.0f, 1080.0f, 0.0f, 1080.0f };

static OraclesUiMotif motif_of(OraclesHomeHero hero)
{
    return hero == ORACLES_HOME_HERO_AGES ? ORACLES_UI_MOTIF_AGES : hero == ORACLES_HOME_HERO_SEASONS ? ORACLES_UI_MOTIF_SEASONS : ORACLES_UI_MOTIF_FAN;
}

OraclesUiLayout oracles_ui_home_scene(const OraclesHomeNav *nav) { return nav->layout; }

void oracles_ui_home_start(OraclesUiHome *home, const OraclesHomeNav *nav)
{
    memset(home, 0, sizeof *home);
    const OraclesHomeHero hero = oracles_home_hero(nav);
    for (int h = 0; h < ORACLES_HOME_HEROES; h++) {
        oracles_ui_tween_jump(&home->hero_opacity[h], h == (int)hero ? 1.0f : 0.0f);
        oracles_ui_tween_jump(&home->hero_shift[h], h == (int)hero ? 0.0f : HERO_SLIDE);
    }
    for (int m = 0; m < ORACLES_UI_MOTIF_COUNT; m++) oracles_ui_tween_jump(&home->motif[m], m == (int)motif_of(hero) ? 1.0f : 0.0f);
    for (unsigned i = 0; i < ORACLES_HOME_MAX_ITEMS; i++) oracles_ui_tween_jump(&home->highlight[i], i == oracles_home_focus(nav) ? 1.0f : 0.0f);
    oracles_ui_tween_jump(&home->motif_shift, nav->screen == ORACLES_SCREEN_HOME ? 0.0f : -ORACLES_UI_MOTIF_SLIDE);
    oracles_ui_tween_jump(&home->motif_layer, nav->screen == ORACLES_SCREEN_HOME ? 1.0f : nav->screen == ORACLES_SCREEN_CONTROLS ? CONTROLS_MOTIF_OPACITY : PAGE_MOTIF_OPACITY);
    oracles_ui_tween_jump(&home->toast, 0.0f);
}

void oracles_ui_home_follow(OraclesUiHome *home, const OraclesHomeNav *nav, double now_ms)
{
    const OraclesHomeHero hero = oracles_home_hero(nav);
    for (int h = 0; h < ORACLES_HOME_HEROES; h++) {
        const int shown = h == (int)hero;
        oracles_ui_tween_to(&home->hero_opacity[h], shown ? 1.0f : 0.0f, now_ms, HERO_FADE_MS, &oracles_ui_ease);
        oracles_ui_tween_to(&home->hero_shift[h], shown ? 0.0f : HERO_SLIDE, now_ms, HERO_SLIDE_MS, &oracles_ui_ease_swift);
    }
    for (int m = 0; m < ORACLES_UI_MOTIF_COUNT; m++)
        oracles_ui_tween_to(&home->motif[m], m == (int)motif_of(hero) ? 1.0f : 0.0f, now_ms, MOTIF_FADE_MS, &oracles_ui_ease);
    for (unsigned i = 0; i < ORACLES_HOME_MAX_ITEMS; i++)
        oracles_ui_tween_to(&home->highlight[i], i == oracles_home_focus(nav) ? 1.0f : 0.0f, now_ms, HIGHLIGHT_MS, &oracles_ui_ease);
    const int page = nav->screen != ORACLES_SCREEN_HOME;
    oracles_ui_tween_to(&home->motif_shift, page ? -ORACLES_UI_MOTIF_SLIDE : 0.0f, now_ms, MOTIF_SLIDE_MS, &oracles_ui_ease_swift);
    const float layer = !page ? 1.0f : nav->screen == ORACLES_SCREEN_CONTROLS ? CONTROLS_MOTIF_OPACITY : PAGE_MOTIF_OPACITY;
    oracles_ui_tween_to(&home->motif_layer, layer, now_ms, MOTIF_LAYER_MS, &oracles_ui_ease);
}

void oracles_ui_home_toast(OraclesUiHome *home, const char *text, double now_ms)
{
    snprintf(home->toast_text, sizeof home->toast_text, "%s", text);
    home->toast_until_ms = now_ms + TOAST_SHOWN_MS;
    oracles_ui_tween_to(&home->toast, 1.0f, now_ms, TOAST_FADE_MS, &oracles_ui_ease);
}

double oracles_ui_home_due_ms(const OraclesUiHome *home)
{
    return home->toast_until_ms > 0.0 ? home->toast_until_ms : -1.0;
}

static void draw_line(OraclesUiDraw *draw, const OraclesUiTextStyle *style, const OraclesUiLine *line, const char *text, float shift, OraclesUiColor color)
{
    if (text && text[0]) oracles_ui_draw_text(draw, style, line->x + shift, line->baseline, text, color);
}

/* The left stack: the two entries not chosen, their texts and places. */
typedef struct others_view {
    OraclesHomeEntry entries[2];
    char states[2][ORACLES_HOME_STATE_LENGTH];
    const char *over[2], *title[2], *state[2];
    OraclesUiTitleLayout layout[2];
} others_view;

static void layout_others(const OraclesHomeNav *nav, others_view *v)
{
    oracles_home_others(nav, v->entries);
    for (int i = 0; i < 2; i++) {
        const OraclesHomeHero hero = oracles_home_entry_hero(v->entries[i]);
        oracles_home_state(nav, hero, v->states[i]);
        v->over[i] = oracles_home_over(hero);
        v->title[i] = oracles_home_title(hero);
        v->state[i] = v->states[i];
    }
    oracles_ui_layout_others(v->over, v->title, v->state, v->layout);
}

/* 4:3: the three entries as tabs, the chosen one underlined in the accent. */
static const char *const tab_labels[ORACLES_HOME_ENTRIES] = { "Oracle of Ages", "Oracle of Seasons", "Fan games" };

static void draw_tabs(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent)
{
    OraclesUiTabLayout tabs[ORACLES_HOME_ENTRIES];
    oracles_ui_layout_tabs(tab_labels, tabs);
    for (int i = 0; i < ORACLES_HOME_ENTRIES; i++) {
        const int chosen = (OraclesHomeEntry)i == nav->entry;
        if (chosen) oracles_ui_fill_rect(draw, tabs[i].line.x, tabs[i].line.y, tabs[i].line.w, tabs[i].line.h, accent);
        draw_line(draw, &oracles_ui_tab_4_3, &tabs[i].label, tab_labels[i], 0.0f, oracles_ui_rgb(chosen ? TAB_CHOSEN : TAB_OTHER));
    }
}

static void draw_others(OraclesUiDraw *draw, const OraclesHomeNav *nav)
{
    others_view v;
    layout_others(nav, &v);
    for (int i = 0; i < 2; i++) {
        draw_line(draw, &oracles_ui_other_over, &v.layout[i].over, v.over[i], 0.0f, oracles_ui_rgb(OTHER_OVER));
        draw_line(draw, &oracles_ui_other_title, &v.layout[i].title, v.title[i], 0.0f, oracles_ui_rgb(OTHER_TITLE));
        draw_line(draw, &oracles_ui_other_state, &v.layout[i].state, v.state[i], 0.0f, oracles_ui_rgb(OTHER_STATE));
    }
}

static int draw_heroes(OraclesUiDraw *draw, const OraclesUiHome *home, const OraclesHomeNav *nav, OraclesUiLayout scene, double now_ms)
{
    const OraclesUiHomeStyles *st = oracles_ui_home_styles(scene);
    int moving = 0;
    for (int h = 0; h < ORACLES_HOME_HEROES; h++) {
        moving |= oracles_ui_tween_running(&home->hero_opacity[h], now_ms) || oracles_ui_tween_running(&home->hero_shift[h], now_ms);
        const float opacity = oracles_ui_tween_value(&home->hero_opacity[h], now_ms);
        if (opacity <= 0.0f) continue;
        const float shift = oracles_ui_tween_value(&home->hero_shift[h], now_ms);
        char state[ORACLES_HOME_STATE_LENGTH];
        oracles_home_state(nav, (OraclesHomeHero)h, state);
        OraclesUiTitleLayout l;
        oracles_ui_layout_hero(scene, nav->screen == ORACLES_SCREEN_PAUSE, oracles_home_over((OraclesHomeHero)h), oracles_home_title((OraclesHomeHero)h), state, &l);
        draw_line(draw, st->hero_over, &l.over, oracles_home_over((OraclesHomeHero)h), shift, oracles_ui_rgba(HERO_OVER, opacity));
        draw_line(draw, st->hero_title, &l.title, oracles_home_title((OraclesHomeHero)h), shift, oracles_ui_rgba(HERO_TITLE, opacity));
        draw_line(draw, st->hero_state, &l.state, state, shift, oracles_ui_rgba(nav->in_game ? PAUSED_STATE : HERO_STATE, opacity));
    }
    return moving;
}

/* The menu's texts: a disabled item shows its reason while highlighted. */
static unsigned menu_texts(const OraclesHomeNav *nav, OraclesHomeItem *items, const char **notes, int *reasons, const char **labels)
{
    const unsigned count = oracles_home_items(nav, items), focus = oracles_home_focus(nav);
    for (unsigned i = 0; i < count; i++) {
        reasons[i] = !items[i].note && i == focus && items[i].disabled;
        notes[i] = items[i].note ? items[i].note : reasons[i] ? items[i].reason : NULL;
        labels[i] = items[i].label;
    }
    return count;
}

static int draw_menu(OraclesUiDraw *draw, const OraclesUiHome *home, const OraclesHomeNav *nav, OraclesUiLayout scene, double now_ms)
{
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    const char *notes[ORACLES_HOME_MAX_ITEMS], *labels[ORACLES_HOME_MAX_ITEMS];
    int reasons[ORACLES_HOME_MAX_ITEMS];
    const unsigned count = menu_texts(nav, items, notes, reasons, labels);
    OraclesUiItemLayout layout[ORACLES_HOME_MAX_ITEMS];
    oracles_ui_layout_menu(scene, notes, reasons, labels, count, layout);
    const OraclesUiHomeStyles *st = oracles_ui_home_styles(scene);
    const look *lk = look_of(scene);
    const OraclesUiColor accent = oracles_ui_rgb(accents[motif_of(oracles_home_hero(nav))]);
    int moving = 0;
    for (unsigned i = 0; i < count; i++) {
        moving |= oracles_ui_tween_running(&home->highlight[i], now_ms);
        const float t = oracles_ui_tween_value(&home->highlight[i], now_ms);
        const OraclesUiItemLayout *l = &layout[i];
        oracles_ui_fill_round_rect(draw, l->box.x, l->box.y, l->box.w, l->box.h, lk->item_radius, oracles_ui_rgba(ITEM_HIGHLIGHT, ITEM_HIGHLIGHT_OPACITY * t));
        const OraclesUiColor color = items[i].disabled ? oracles_ui_mix(oracles_ui_rgb(ITEM_DISABLED), oracles_ui_rgb(ITEM_DISABLED_FOCUSED), t)
                                                       : oracles_ui_mix(oracles_ui_rgb(ITEM_TEXT), accent, t);
        /* A reason on the highlight's background, readable over the motif. */
        if (l->note_box.w > 0.0f)
            oracles_ui_fill_round_rect(draw, l->note_box.x, l->note_box.y, l->note_box.w, l->note_box.h, lk->reason_radius, oracles_ui_rgba(ITEM_HIGHLIGHT, ITEM_HIGHLIGHT_OPACITY));
        draw_line(draw, st->item_note, &l->note, notes[i], 0.0f, oracles_ui_rgb(nav->in_game ? lk->paused_note : ITEM_NOTE));
        draw_line(draw, st->item_label, &l->label, labels[i], 0.0f, color);
        draw_line(draw, st->item_label, &l->dot, oracles_ui_item_dot, 0.0f, color);
    }
    return moving;
}

/* The help bar's left edge in the scene. */
static float hints_left(const OraclesHomeNav *nav, OraclesUiLayout scene)
{
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    const unsigned count = oracles_home_hints(nav, hints);
    OraclesUiHintLayout layout[ORACLES_HOME_MAX_HINTS];
    oracles_ui_layout_hints(scene, hints, count, layout);
    return count ? layout[0].box.x : oracles_ui_scene_width(scene);
}

static void draw_hints(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiLayout scene)
{
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    const unsigned count = oracles_home_hints(nav, hints);
    OraclesUiHintLayout layout[ORACLES_HOME_MAX_HINTS];
    oracles_ui_layout_hints(scene, hints, count, layout);
    const OraclesUiHomeStyles *st = oracles_ui_home_styles(scene);
    const look *lk = look_of(scene);
    for (unsigned i = 0; i < count; i++) {
        const OraclesUiHintLayout *l = &layout[i];
        oracles_ui_fill_round_rect(draw, l->box.x, l->box.y, l->box.w, l->box.h, lk->hint_radius, oracles_ui_rgba(HINT_BACKGROUND, lk->hint_opacity));
        oracles_ui_stroke_round_rect(draw, l->key_box.x, l->key_box.y, l->key_box.w, l->key_box.h, lk->key_radius, 1.0f, oracles_ui_rgb(HINT_KEY_BORDER));
        draw_line(draw, st->hint_key, &l->key, hints[i].key, 0.0f, oracles_ui_rgb(HINT_KEY));
        draw_line(draw, st->hint_label, &l->label, hints[i].label, 0.0f, oracles_ui_rgb(HINT_LABEL));
    }
}

static int draw_toast(OraclesUiDraw *draw, OraclesUiHome *home, OraclesUiLayout scene, double now_ms)
{
    if (home->toast_until_ms > 0.0 && now_ms >= home->toast_until_ms) {
        home->toast_until_ms = 0.0;
        oracles_ui_tween_to(&home->toast, 0.0f, now_ms, TOAST_FADE_MS, &oracles_ui_ease);
    }
    const float opacity = oracles_ui_tween_value(&home->toast, now_ms);
    if (opacity > 0.0f && home->toast_text[0]) {
        OraclesUiToastLayout l;
        oracles_ui_layout_toast(scene, home->toast_text, &l);
        const float radius = look_of(scene)->toast_radius;
        oracles_ui_fill_round_rect(draw, l.box.x, l.box.y, l.box.w, l.box.h, radius, oracles_ui_rgba(TOAST_BACKGROUND, opacity));
        oracles_ui_stroke_round_rect(draw, l.box.x, l.box.y, l.box.w, l.box.h, radius, 1.0f, oracles_ui_rgba(TOAST_BORDER, TOAST_BORDER_OPACITY * opacity));
        draw_line(draw, oracles_ui_home_styles(scene)->toast, &l.text, home->toast_text, 0.0f, oracles_ui_rgba(TOAST_TEXT, opacity));
    }
    return oracles_ui_tween_running(&home->toast, now_ms);
}

/* The slow game's toast, on the home screen only, as long as nav->slow holds, and the row it leads to: 0 when there is
 * none to show. */
static int layout_slow(const OraclesHomeNav *nav, OraclesUiSlowLayout *out, const char **path)
{
    char text[ORACLES_HOME_TEXT_LENGTH];
    if (!nav->slow || nav->screen != ORACLES_SCREEN_HOME || !oracles_home_slow_text(nav, text, sizeof text, path)) return 0;
    oracles_ui_layout_slow(oracles_ui_home_scene(nav), text, *path, out);
    return 1;
}

/* No fade: it shows on the return from the game, and goes with the first input. */
static void draw_slow(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiLayout scene, OraclesUiColor accent)
{
    OraclesUiSlowLayout l;
    const char *path;
    if (!layout_slow(nav, &l, &path)) return;
    const OraclesUiHomeStyles *st = oracles_ui_home_styles(scene);
    const float radius = look_of(scene)->toast_radius;
    oracles_ui_fill_round_rect(draw, l.box.x, l.box.y, l.box.w, l.box.h, radius, oracles_ui_rgb(TOAST_BACKGROUND));
    oracles_ui_stroke_round_rect(draw, l.box.x, l.box.y, l.box.w, l.box.h, radius, 1.0f, oracles_ui_rgba(TOAST_BORDER, TOAST_BORDER_OPACITY));
    for (unsigned i = 0; i < l.lines; i++) draw_line(draw, st->toast, &l.text_lines[i], l.text[i], 0.0f, oracles_ui_rgb(TOAST_TEXT));
    draw_line(draw, st->slow_path, &l.path, path, 0.0f, accent);
}

int oracles_ui_home_draw(OraclesUiDraw *draw, OraclesUiHome *home, const OraclesHomeNav *nav, double now_ms)
{
    int moving = 0;
    const OraclesUiLayout scene = oracles_ui_home_scene(nav);
    /* The drawing order: motifs (ages under seasons under fan, one layer that slides and fades behind a page), then
     * the diagonal and the stack (16:9) or the tabs (4:3), the title and the menu of the home screen, or a page; the
     * version, the help, the toast.  In a game the game's image stands behind instead of the motifs, drawn by the
     * pause beforehand. */
    if (!nav->in_game) {
        moving = oracles_ui_tween_running(&home->motif_shift, now_ms) || oracles_ui_tween_running(&home->motif_layer, now_ms);
        const float shift = oracles_ui_tween_value(&home->motif_shift, now_ms), layer = oracles_ui_tween_value(&home->motif_layer, now_ms);
        for (int m = 0; m < ORACLES_UI_MOTIF_COUNT; m++) {
            moving |= oracles_ui_tween_running(&home->motif[m], now_ms);
            oracles_ui_draw_motif(draw, (OraclesUiMotif)m, shift, layer * oracles_ui_tween_value(&home->motif[m], now_ms));
        }
    }
    const OraclesUiColor accent = oracles_ui_rgb(accents[motif_of(oracles_home_hero(nav))]);
    if (nav->screen == ORACLES_SCREEN_HOME) {
        if (scene == ORACLES_UI_LAYOUT_4_3) {
            draw_tabs(draw, nav, accent);
        } else {
            oracles_ui_fill_polygon(draw, diagonal, 4, oracles_ui_rgba(DIAGONAL, DIAGONAL_OPACITY));
            draw_others(draw, nav);
        }
        moving |= draw_heroes(draw, home, nav, scene, now_ms);
        moving |= draw_menu(draw, home, nav, scene, now_ms);
    } else if (nav->screen == ORACLES_SCREEN_PAUSE) {
        /* At the pause's minimum scale in a small window, the title keeps to the top right, the menu to the bottom
         * right (the anchors do nothing when the scene fits). */
        oracles_ui_draw_anchor(draw, 1.0f, 0.0f);
        moving |= draw_heroes(draw, home, nav, scene, now_ms);
        oracles_ui_draw_anchor(draw, 1.0f, 1.0f);
        moving |= draw_menu(draw, home, nav, scene, now_ms);
    } else if (nav->screen == ORACLES_SCREEN_GAME) {
        oracles_ui_page_draw(draw, nav, accent);
    } else if (nav->screen == ORACLES_SCREEN_CONTROLS) {
        oracles_ui_controls_draw(draw, nav, accent);
    } else if (nav->screen == ORACLES_SCREEN_MODS) {
        oracles_ui_mods_draw(draw, nav, accent);
    } else {
        oracles_ui_display_draw(draw, nav, accent);
    }
    OraclesUiLine version;
    oracles_ui_layout_version(scene, ORACLES_VERSION, &version);
    oracles_ui_draw_anchor(draw, 0.0f, 1.0f);
    /* The version gives way to the help bar where the two would meet: a long `git describe` in the pause's small
     * window. */
    if (version.x + version.w + VERSION_GAP <= hints_left(nav, scene) + oracles_ui_draw_anchor_shift(draw, 0.0f, 0.5f))
        draw_line(draw, oracles_ui_home_styles(scene)->version, &version, ORACLES_VERSION, 0.0f, oracles_ui_rgb(VERSION_TEXT));
    oracles_ui_draw_anchor(draw, 0.5f, 1.0f);
    draw_hints(draw, nav, scene);
    oracles_ui_draw_anchor(draw, 0.0f, 1.0f);
    draw_slow(draw, nav, scene, accent);
    oracles_ui_draw_anchor(draw, 0.5f, 1.0f);
    moving |= draw_toast(draw, home, scene, now_ms);
    return moving;
}

static int inside(const OraclesUiBox *box, float x, float y)
{
    return x >= box->x && x < box->x + box->w && y >= box->y && y < box->y + box->h;
}

/* The help bar's hint that goes back, under the point. */
static int back_hint(const OraclesHomeNav *nav, float x, float y)
{
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    const unsigned count = oracles_home_hints(nav, hints);
    OraclesUiHintLayout layout[ORACLES_HOME_MAX_HINTS];
    oracles_ui_layout_hints(oracles_ui_home_scene(nav), hints, count, layout);
    for (unsigned i = 0; i < count; i++) if (hints[i].back && inside(&layout[i].box, x, y)) return 1;
    return 0;
}

OraclesUiHomeHit oracles_ui_home_hit(const OraclesHomeNav *nav, float x, float y)
{
    OraclesUiHomeHit hit = { ORACLES_UI_HIT_NONE, 0, -1, ORACLES_HOME_AGES, 0, 0 };
    if (back_hint(nav, x, y)) { hit.kind = ORACLES_UI_HIT_BACK; return hit; }
    OraclesUiSlowLayout slow;
    const char *path;
    if (layout_slow(nav, &slow, &path) && inside(&slow.box, x, y)) { hit.kind = ORACLES_UI_HIT_SLOW; return hit; }
    if (nav->screen == ORACLES_SCREEN_CONTROLS) {
        if (oracles_ui_controls_hit(nav, x, y, &hit.column, &hit.row, &hit.option)) hit.kind = ORACLES_UI_HIT_CELL;
        return hit;
    }
    if (nav->screen != ORACLES_SCREEN_HOME && nav->screen != ORACLES_SCREEN_PAUSE) {
        const int row = nav->screen == ORACLES_SCREEN_GAME ? oracles_ui_page_hit(nav, x, y)
                        : nav->screen == ORACLES_SCREEN_MODS ? oracles_ui_mods_hit(nav, x, y) : oracles_ui_display_hit(nav, x, y, &hit.option);
        if (row >= 0) { hit.kind = ORACLES_UI_HIT_ROW; hit.index = (unsigned)row; }
        return hit;
    }
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    const char *notes[ORACLES_HOME_MAX_ITEMS], *labels[ORACLES_HOME_MAX_ITEMS];
    int reasons[ORACLES_HOME_MAX_ITEMS];
    const unsigned count = menu_texts(nav, items, notes, reasons, labels);
    OraclesUiItemLayout menu[ORACLES_HOME_MAX_ITEMS];
    oracles_ui_layout_menu(oracles_ui_home_scene(nav), notes, reasons, labels, count, menu);
    for (unsigned i = 0; i < count; i++)
        if (inside(&menu[i].box, x, y)) { hit.kind = ORACLES_UI_HIT_ITEM; hit.index = i; return hit; }
    if (nav->screen == ORACLES_SCREEN_PAUSE) return hit;   /* the pause has no left stack, no tabs */
    if (oracles_ui_home_scene(nav) == ORACLES_UI_LAYOUT_4_3) {
        OraclesUiTabLayout tabs[ORACLES_HOME_ENTRIES];
        oracles_ui_layout_tabs(tab_labels, tabs);
        for (int i = 0; i < ORACLES_HOME_ENTRIES; i++)
            if (inside(&tabs[i].box, x, y)) { hit.kind = ORACLES_UI_HIT_ENTRY; hit.entry = (OraclesHomeEntry)i; return hit; }
        return hit;
    }
    /* The left stack's entries are as wide as the widest of them, a column's items being stretched. */
    others_view v;
    layout_others(nav, &v);
    const float width = v.layout[0].box.w > v.layout[1].box.w ? v.layout[0].box.w : v.layout[1].box.w;
    for (int i = 0; i < 2; i++) {
        OraclesUiBox box = v.layout[i].box;
        box.w = width;
        if (inside(&box, x, y)) { hit.kind = ORACLES_UI_HIT_ENTRY; hit.entry = v.entries[i]; return hit; }
    }
    return hit;
}
