#include "ui_controls.h"

#include "ui_controls_layout.h"
#include "ui_page_layout.h"

#include <math.h>
#include <string.h>

/* Controls' colours. */
#define PANEL 0x0a090du
#define PANEL_OPACITY 0.92f
#define PANEL_BORDER_OPACITY 0.07f
#define TITLE 0xf3f1f5u
#define HEADING 0xa19eaau
#define NAME 0xefedf2u
#define CELL_TEXT 0xefedf2u
#define CELL_NONE 0x77747fu           /* "—", and the d-pad's cells */
#define CELL_BORDER_OPACITY 0.14f
#define CELL_LOCKED_BORDER_OPACITY 0.1f
#define CELL_FOCUS_OPACITY 0.06f
#define CELL_WAITING_OPACITY 0.1f
#define ROW_HIGHLIGHT_OPACITY 0.055f
#define BUTTON 0xdcd9e1u
#define TEXT 0xa19eaau
#define NOTE 0x8f8c98u
#define CHOICE_OFF 0x8a8793u
#define ITEM 0xd6d3dcu
#define ITEM_EMPTY 0x8f8c98u
#define GRID_OFF_OPACITY 0.4f          /* the grid while the item hotkeys are off */
#define KEY_TEXT 0xd6d3dcu
#define KEY_BORDER 0x4a4752u
#define SHORTCUT 0xc9c6cfu
#define WARN 0xecd198u                 /* a fan game's line, the warning tone's text */
#define TAB_CHOSEN 0xf3f1f5u
#define TAB_OTHER 0x8a8793u
#define TAB_FOCUS_OPACITY 0.07f

/* What differs between the layouts in the drawing: corners, the chips' border. */
typedef struct look {
    float cell_radius, row_radius, chip_radius, chip_border, key_radius, tab_radius;
} look;
static const look look_16_9 = { 6.0f, 6.0f, 5.0f, 1.0f, 4.0f, 0.0f };
static const look look_4_3 = { 7.0f, 8.0f, 7.0f, 2.0f, 6.0f, 7.0f };

static void text(OraclesUiDraw *draw, const OraclesUiTextStyle *style, const OraclesUiLine *line, const char *s, OraclesUiColor color)
{
    if (s && s[0]) oracles_ui_draw_text(draw, style, line->x, line->baseline, s, color);
}

/* A box whose top corners only are rounded (a tab's). */
static void fill_top_round_rect(OraclesUiDraw *draw, const OraclesUiBox *b, float radius, OraclesUiColor color)
{
    enum { ARC = 6 };
    float points[2 * (2 * ARC + 2)];
    int n = 0;
    for (int corner = 0; corner < 2; corner++)
        for (int i = 0; i < ARC; i++) {
            const float a = 3.14159265f * (1.0f + (float)(corner * (ARC - 1) + i) / (float)(2 * ARC - 2));
            const float cx = corner ? b->x + b->w - radius : b->x + radius;
            points[n++] = cx + radius * cosf(a);
            points[n++] = b->y + radius + radius * sinf(a);
        }
    points[n++] = b->x + b->w; points[n++] = b->y + b->h;
    points[n++] = b->x; points[n++] = b->y + b->h;
    oracles_ui_fill_polygon(draw, points, n / 2, color);
}

static void cell(OraclesUiDraw *draw, const OraclesHomeNav *nav, const OraclesUiControlsLayout *l, const OraclesUiControlsStyles *st, const look *lk,
                 int column, int row, OraclesUiColor accent, float opacity)
{
    const OraclesUiBox *b = &l->cells[column][row];
    if (b->w <= 0.0f) return;
    const OraclesHomeControls *c = &nav->controls;
    const int focused = c->column == column && c->row == row, waiting = focused && c->capturing;
    const char *s = oracles_controls_cell_text(nav, column, row);
    if (oracles_controls_cell_locked(column, row)) {
        oracles_ui_stroke_dashed_rect(draw, b->x, b->y, b->w, b->h, lk->cell_radius, 1.0f, oracles_ui_rgba(0xffffffu, CELL_LOCKED_BORDER_OPACITY * opacity));
        text(draw, st->cell_locked, &l->cell_texts[column][row], s, oracles_ui_rgba(CELL_NONE, opacity));
        return;
    }
    if (waiting || focused) oracles_ui_fill_round_rect(draw, b->x, b->y, b->w, b->h, lk->cell_radius, oracles_ui_rgba(0xffffffu, (waiting ? CELL_WAITING_OPACITY : CELL_FOCUS_OPACITY) * opacity));
    const OraclesUiColor border = focused ? oracles_ui_fade(accent, opacity) : oracles_ui_rgba(0xffffffu, CELL_BORDER_OPACITY * opacity);
    oracles_ui_stroke_round_rect(draw, b->x, b->y, b->w, b->h, lk->cell_radius, waiting ? 2.0f : 1.0f, border);
    const int none = !waiting && !strcmp(s, "\xe2\x80\x94");
    const OraclesUiColor color = waiting || focused ? oracles_ui_fade(accent, opacity) : oracles_ui_rgba(none ? CELL_NONE : CELL_TEXT, opacity);
    text(draw, waiting ? st->cell_waiting : st->cell, &l->cell_texts[column][row], s, color);
}

static void panel(OraclesUiDraw *draw, const OraclesUiBox *b)
{
    oracles_ui_fill_round_rect(draw, b->x, b->y, b->w, b->h, 10.0f, oracles_ui_rgba(PANEL, PANEL_OPACITY));
    oracles_ui_stroke_round_rect(draw, b->x, b->y, b->w, b->h, 10.0f, 1.0f, oracles_ui_rgba(0xffffffu, PANEL_BORDER_OPACITY));
}

/* The game buttons' headings, rows and cells, and Reset. */
static void buttons(OraclesUiDraw *draw, const OraclesHomeNav *nav, const OraclesUiControlsLayout *l, const OraclesUiControlsStyles *st, const look *lk,
                    OraclesUiColor accent)
{
    text(draw, st->heading, &l->heading_buttons, "Game buttons", oracles_ui_rgb(HEADING));
    text(draw, st->heading, &l->heading_keyboard, "Keyboard", oracles_ui_rgb(HEADING));
    text(draw, st->heading, &l->heading_gamepad, "Gamepad", oracles_ui_rgb(HEADING));
    for (int row = 0; row < ORACLES_HOME_BUTTONS; row++) {
        text(draw, st->name, &l->button_names[row], oracles_controls_buttons[row], oracles_ui_rgb(NAME));
        cell(draw, nav, l, st, lk, 0, row, accent, 1.0f);
        cell(draw, nav, l, st, lk, 1, row, accent, 1.0f);
    }
    const int reset = nav->controls.row == ORACLES_CONTROLS_ROW_RESET;
    if (reset) oracles_ui_fill_round_rect(draw, l->reset.x, l->reset.y, l->reset.w, l->reset.h, lk->row_radius, oracles_ui_rgba(0xffffffu, ROW_HIGHLIGHT_OPACITY));
    text(draw, st->reset, &l->reset_text, "Reset to defaults", reset ? accent : oracles_ui_rgb(BUTTON));
    text(draw, st->reset, &l->reset_dot, oracles_ui_item_dot, reset ? accent : oracles_ui_rgb(BUTTON));
}

static void mode_line(OraclesUiDraw *draw, const OraclesHomeNav *nav, const OraclesUiControlsLayout *l, const OraclesUiControlsStyles *st, const look *lk,
                      OraclesUiColor accent)
{
    const int focused = nav->controls.row == ORACLES_CONTROLS_ROW_MODE, on = oracles_controls_hotkeys_on(nav);
    if (focused) oracles_ui_fill_round_rect(draw, l->mode.x, l->mode.y, l->mode.w, l->mode.h, lk->row_radius, oracles_ui_rgba(0xffffffu, ROW_HIGHLIGHT_OPACITY));
    text(draw, st->mode_title, &l->mode_title, "Item hotkeys", oracles_ui_rgb(NAME));
    for (int i = 0; i < 2; i++) {
        const OraclesUiChipLayout *c = &l->mode_choices[i];
        const int chosen = i == on;
        const OraclesUiColor frame = chosen ? (focused ? accent : oracles_ui_rgba(0xffffffu, 0.4f)) : oracles_ui_rgba(0xffffffu, 0.1f);
        oracles_ui_stroke_round_rect(draw, c->box.x, c->box.y, c->box.w, c->box.h, lk->chip_radius, lk->chip_border, frame);
        text(draw, st->chip, &c->name, oracles_ui_controls_mode_choices[i], chosen ? (focused ? accent : oracles_ui_rgb(NAME)) : oracles_ui_rgb(CHOICE_OFF));
    }
    text(draw, st->mode_text, &l->mode_text, oracles_ui_controls_mode_text, oracles_ui_rgb(TEXT));
    text(draw, st->note, &l->mode_note, oracles_ui_controls_mode_note, oracles_ui_rgb(NOTE));
    if (l->mode_fan.h > 0.0f) text(draw, st->mode_text, &l->mode_fan, l->mode_fan_text, oracles_ui_rgb(WARN));
    if (l->mode_later.h > 0.0f) text(draw, st->note, &l->mode_later, oracles_ui_later, oracles_ui_rgb(NOTE));
}

/* The item hotkeys: their line, then the grid, dimmed while they are off. */
static void hotkeys(OraclesUiDraw *draw, const OraclesHomeNav *nav, const OraclesUiControlsLayout *l, const OraclesUiControlsStyles *st, const look *lk,
                    OraclesUiColor accent)
{
    mode_line(draw, nav, l, st, lk, accent);
    const float grid = oracles_controls_hotkeys_on(nav) ? 1.0f : GRID_OFF_OPACITY;
    text(draw, st->heading, &l->heading_hotkeys, "Hotkeys", oracles_ui_rgba(HEADING, grid));
    text(draw, st->heading, &l->heading_hotkeys_keyboard, "Keyboard", oracles_ui_rgba(HEADING, grid));
    text(draw, st->heading, &l->heading_hotkeys_gamepad, "Gamepad", oracles_ui_rgba(HEADING, grid));
    text(draw, st->heading, &l->heading_items, oracles_controls_item_heading(nav), oracles_ui_rgba(HEADING, grid));
    for (int row = 0; row < ORACLES_HOME_HOTKEY_ROWS; row++) {
        text(draw, st->name, &l->hotkey_names[row], oracles_controls_hotkey_rows[row], oracles_ui_rgba(NAME, grid));
        cell(draw, nav, l, st, lk, 2, row, accent, grid);
        if (row >= ORACLES_HOME_SLOTS) continue;
        cell(draw, nav, l, st, lk, 3, row, accent, grid);
        const int empty = !oracles_controls_item(nav, row)[0];
        text(draw, empty ? st->item_empty : st->item, &l->items[row], oracles_ui_controls_item_text(nav, row), oracles_ui_rgba(empty ? ITEM_EMPTY : ITEM, grid));
    }
    if (st->slots_note) text(draw, st->slots_note, &l->slots_note, oracles_ui_controls_slots, oracles_ui_rgba(TEXT, grid));
}

/* The fixed shortcuts. */
static void shortcuts(OraclesUiDraw *draw, const OraclesUiControlsLayout *l, const OraclesUiControlsStyles *st, const look *lk)
{
    text(draw, st->heading, &l->shortcuts_label, l->shortcuts_text, oracles_ui_rgb(HEADING));
    for (int i = 0; i < 6; i++) {
        const OraclesUiBox *k = &l->shortcut_keys[i];
        oracles_ui_stroke_round_rect(draw, k->x, k->y, k->w, k->h, lk->key_radius, 1.0f, oracles_ui_rgb(KEY_BORDER));
        text(draw, st->shortcut_key, &l->shortcut_key_texts[i], oracles_controls_shortcuts[i][0], oracles_ui_rgb(KEY_TEXT));
        text(draw, st->shortcut_label, &l->shortcut_labels[i], oracles_controls_shortcuts[i][1], oracles_ui_rgb(SHORTCUT));
    }
}

/* 4:3's tabs: the shown one underlined in the accent, its label in the accent on a lighter ground while the highlight
 * is on the strip. */
static void tabs(OraclesUiDraw *draw, const OraclesHomeNav *nav, const OraclesUiControlsLayout *l, const OraclesUiControlsStyles *st, const look *lk,
                 OraclesUiColor accent)
{
    const int shown = oracles_controls_tab(nav), focused = nav->controls.row == ORACLES_CONTROLS_ROW_TAB;
    for (int i = 0; i < ORACLES_CONTROLS_TABS; i++) {
        if (l->tabs[i].w <= 0.0f) continue;
        const int on = i == shown;
        if (on && focused) fill_top_round_rect(draw, &l->tabs[i], lk->tab_radius, oracles_ui_rgba(0xffffffu, TAB_FOCUS_OPACITY));
        if (on) oracles_ui_fill_rect(draw, l->tab_lines[i].x, l->tab_lines[i].y, l->tab_lines[i].w, l->tab_lines[i].h, accent);
        text(draw, st->tab, &l->tab_labels[i], oracles_controls_tab_names[i], on ? (focused ? accent : oracles_ui_rgb(TAB_CHOSEN)) : oracles_ui_rgb(TAB_OTHER));
    }
}

void oracles_ui_controls_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent)
{
    const OraclesUiControlsStyles *st = oracles_ui_controls_styles(nav->layout);
    OraclesUiControlsLayout l;
    oracles_ui_layout_controls(nav->layout, nav, &l);
    text(draw, st->title, &l.title, "Controls", oracles_ui_rgb(TITLE));
    text(draw, st->prompt, &l.prompt, oracles_controls_prompt(nav), accent);
    if (nav->layout == ORACLES_UI_LAYOUT_4_3) {
        tabs(draw, nav, &l, st, &look_4_3, accent);
        switch (oracles_controls_tab(nav)) {
            case ORACLES_CONTROLS_TAB_BUTTONS: buttons(draw, nav, &l, st, &look_4_3, accent); break;
            case ORACLES_CONTROLS_TAB_HOTKEYS: hotkeys(draw, nav, &l, st, &look_4_3, accent); break;
            default: shortcuts(draw, &l, st, &look_4_3); break;
        }
        return;
    }
    panel(draw, &l.left_panel);
    buttons(draw, nav, &l, st, &look_16_9, accent);
    panel(draw, &l.right_panel);
    hotkeys(draw, nav, &l, st, &look_16_9, accent);
    shortcuts(draw, &l, st, &look_16_9);
}

static int inside(const OraclesUiBox *b, float x, float y) { return x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h; }

int oracles_ui_controls_hit(const OraclesHomeNav *nav, float x, float y, int *column, int *row, int *option)
{
    OraclesUiControlsLayout l;
    oracles_ui_layout_controls(nav->layout, nav, &l);
    *option = -1;
    for (int i = 0; i < ORACLES_CONTROLS_TABS; i++)
        if (inside(&l.tabs[i], x, y)) { *column = i == ORACLES_CONTROLS_TAB_HOTKEYS ? 2 : 0; *row = ORACLES_CONTROLS_ROW_TAB; *option = i; return 1; }
    for (int c = 0; c < ORACLES_CONTROLS_COLUMNS; c++)
        for (int r = 0; r < ORACLES_HOME_BUTTONS; r++)
            if (l.cells[c][r].w > 0.0f && inside(&l.cells[c][r], x, y) && !oracles_controls_cell_locked(c, r)) { *column = c; *row = r; return 1; }
    if (inside(&l.reset, x, y)) { *column = 0; *row = ORACLES_CONTROLS_ROW_RESET; return 1; }
    for (int i = 0; i < 2; i++) if (inside(&l.mode_choices[i].box, x, y)) { *column = 2; *row = ORACLES_CONTROLS_ROW_MODE; *option = i; return 1; }
    if (inside(&l.mode, x, y)) { *column = 2; *row = ORACLES_CONTROLS_ROW_MODE; return 1; }
    return 0;
}
