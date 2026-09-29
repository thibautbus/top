#include "ui_controls.h"

#include "ui_controls_layout.h"
#include "ui_page_layout.h"

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
#define DASH 3.0f                      /* a dashed border's dash and gap, in scene pixels */

static void text(OraclesUiDraw *draw, const OraclesUiTextStyle *style, const OraclesUiLine *line, const char *s, OraclesUiColor color)
{
    if (s && s[0]) oracles_ui_draw_text(draw, style, line->x, line->baseline, s, color);
}

/* A dashed border of one scene pixel, along the box's straight sides. */
static void dashed(OraclesUiDraw *draw, const OraclesUiBox *b, float radius, OraclesUiColor color)
{
    for (float x = b->x + radius; x < b->x + b->w - radius; x += 2.0f * DASH) {
        const float w = x + DASH > b->x + b->w - radius ? b->x + b->w - radius - x : DASH;
        oracles_ui_fill_rect(draw, x, b->y, w, 1.0f, color);
        oracles_ui_fill_rect(draw, x, b->y + b->h - 1.0f, w, 1.0f, color);
    }
    for (float y = b->y + radius; y < b->y + b->h - radius; y += 2.0f * DASH) {
        const float h = y + DASH > b->y + b->h - radius ? b->y + b->h - radius - y : DASH;
        oracles_ui_fill_rect(draw, b->x, y, 1.0f, h, color);
        oracles_ui_fill_rect(draw, b->x + b->w - 1.0f, y, 1.0f, h, color);
    }
}

static void cell(OraclesUiDraw *draw, const OraclesHomeNav *nav, const OraclesUiControlsLayout *l, int column, int row, OraclesUiColor accent, float opacity)
{
    const OraclesUiBox *b = &l->cells[column][row];
    if (b->w <= 0.0f) return;
    const OraclesHomeControls *c = &nav->controls;
    const int focused = c->column == column && c->row == row, waiting = focused && c->capturing;
    const char *s = oracles_controls_cell_text(nav, column, row);
    if (oracles_controls_cell_locked(column, row)) {
        dashed(draw, b, 6.0f, oracles_ui_rgba(0xffffffu, CELL_LOCKED_BORDER_OPACITY * opacity));
        text(draw, &oracles_ui_controls_cell_locked, &l->cell_texts[column][row], s, oracles_ui_rgba(CELL_NONE, opacity));
        return;
    }
    if (waiting || focused) oracles_ui_fill_round_rect(draw, b->x, b->y, b->w, b->h, 6.0f, oracles_ui_rgba(0xffffffu, (waiting ? CELL_WAITING_OPACITY : CELL_FOCUS_OPACITY) * opacity));
    const OraclesUiColor border = focused ? oracles_ui_fade(accent, opacity) : oracles_ui_rgba(0xffffffu, CELL_BORDER_OPACITY * opacity);
    oracles_ui_stroke_round_rect(draw, b->x, b->y, b->w, b->h, 6.0f, waiting ? 2.0f : 1.0f, border);
    const int none = !waiting && !strcmp(s, "\xe2\x80\x94");
    const OraclesUiColor color = waiting || focused ? oracles_ui_fade(accent, opacity) : oracles_ui_rgba(none ? CELL_NONE : CELL_TEXT, opacity);
    text(draw, waiting ? &oracles_ui_controls_cell_waiting : &oracles_ui_controls_cell, &l->cell_texts[column][row], s, color);
}

static void panel(OraclesUiDraw *draw, const OraclesUiBox *b)
{
    oracles_ui_fill_round_rect(draw, b->x, b->y, b->w, b->h, 10.0f, oracles_ui_rgba(PANEL, PANEL_OPACITY));
    oracles_ui_stroke_round_rect(draw, b->x, b->y, b->w, b->h, 10.0f, 1.0f, oracles_ui_rgba(0xffffffu, PANEL_BORDER_OPACITY));
}

static void mode_line(OraclesUiDraw *draw, const OraclesHomeNav *nav, const OraclesUiControlsLayout *l, OraclesUiColor accent)
{
    const int focused = nav->controls.row == ORACLES_CONTROLS_ROW_MODE, on = oracles_controls_hotkeys_on(nav);
    if (focused) oracles_ui_fill_round_rect(draw, l->mode.x, l->mode.y, l->mode.w, l->mode.h, 6.0f, oracles_ui_rgba(0xffffffu, ROW_HIGHLIGHT_OPACITY));
    text(draw, &oracles_ui_controls_mode_title, &l->mode_title, "Item hotkeys", oracles_ui_rgb(NAME));
    for (int i = 0; i < 2; i++) {
        const OraclesUiChipLayout *c = &l->mode_choices[i];
        const int chosen = i == on;
        const OraclesUiColor frame = chosen ? (focused ? accent : oracles_ui_rgba(0xffffffu, 0.4f)) : oracles_ui_rgba(0xffffffu, 0.1f);
        oracles_ui_stroke_round_rect(draw, c->box.x, c->box.y, c->box.w, c->box.h, 5.0f, 1.0f, frame);
        text(draw, &oracles_ui_controls_chip, &c->name, oracles_ui_controls_mode_choices[i], chosen ? (focused ? accent : oracles_ui_rgb(NAME)) : oracles_ui_rgb(CHOICE_OFF));
    }
    text(draw, &oracles_ui_controls_mode_text_style, &l->mode_text, oracles_ui_controls_mode_text, oracles_ui_rgb(TEXT));
    text(draw, &oracles_ui_controls_note, &l->mode_note, oracles_ui_controls_mode_note, oracles_ui_rgb(NOTE));
    if (l->mode_later.h > 0.0f) text(draw, &oracles_ui_controls_note, &l->mode_later, oracles_ui_later, oracles_ui_rgb(NOTE));
}

void oracles_ui_controls_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent)
{
    OraclesUiControlsLayout l;
    oracles_ui_layout_controls(nav, &l);
    text(draw, &oracles_ui_controls_title, &l.title, "Controls", oracles_ui_rgb(TITLE));
    text(draw, &oracles_ui_controls_prompt, &l.prompt, oracles_controls_prompt(nav), accent);

    /* The game buttons. */
    panel(draw, &l.left_panel);
    text(draw, &oracles_ui_controls_heading, &l.heading_buttons, "Game buttons", oracles_ui_rgb(HEADING));
    text(draw, &oracles_ui_controls_heading, &l.heading_keyboard, "Keyboard", oracles_ui_rgb(HEADING));
    text(draw, &oracles_ui_controls_heading, &l.heading_gamepad, "Gamepad", oracles_ui_rgb(HEADING));
    for (int row = 0; row < ORACLES_HOME_BUTTONS; row++) {
        text(draw, &oracles_ui_controls_name, &l.button_names[row], oracles_controls_buttons[row], oracles_ui_rgb(NAME));
        cell(draw, nav, &l, 0, row, accent, 1.0f);
        cell(draw, nav, &l, 1, row, accent, 1.0f);
    }
    const int reset = nav->controls.row == ORACLES_CONTROLS_ROW_RESET;
    if (reset) oracles_ui_fill_round_rect(draw, l.reset.x, l.reset.y, l.reset.w, l.reset.h, 6.0f, oracles_ui_rgba(0xffffffu, ROW_HIGHLIGHT_OPACITY));
    text(draw, &oracles_ui_controls_reset, &l.reset_text, "Reset to defaults", reset ? accent : oracles_ui_rgb(BUTTON));
    text(draw, &oracles_ui_controls_reset, &l.reset_dot, oracles_ui_item_dot, reset ? accent : oracles_ui_rgb(BUTTON));

    /* The item hotkeys: their line, then the grid, dimmed while they are off. */
    panel(draw, &l.right_panel);
    mode_line(draw, nav, &l, accent);
    const float grid = oracles_controls_hotkeys_on(nav) ? 1.0f : GRID_OFF_OPACITY;
    text(draw, &oracles_ui_controls_heading, &l.heading_hotkeys, "Hotkeys", oracles_ui_rgba(HEADING, grid));
    text(draw, &oracles_ui_controls_heading, &l.heading_hotkeys_keyboard, "Keyboard", oracles_ui_rgba(HEADING, grid));
    text(draw, &oracles_ui_controls_heading, &l.heading_hotkeys_gamepad, "Gamepad", oracles_ui_rgba(HEADING, grid));
    text(draw, &oracles_ui_controls_heading, &l.heading_items, oracles_controls_item_heading(nav), oracles_ui_rgba(HEADING, grid));
    for (int row = 0; row < ORACLES_HOME_HOTKEY_ROWS; row++) {
        text(draw, &oracles_ui_controls_name, &l.hotkey_names[row], oracles_controls_hotkey_rows[row], oracles_ui_rgba(NAME, grid));
        cell(draw, nav, &l, 2, row, accent, grid);
        if (row >= ORACLES_HOME_SLOTS) continue;
        cell(draw, nav, &l, 3, row, accent, grid);
        const int empty = !oracles_controls_item(nav, row)[0];
        text(draw, empty ? &oracles_ui_controls_item_empty : &oracles_ui_controls_item, &l.items[row], oracles_ui_controls_item_text(nav, row),
             oracles_ui_rgba(empty ? ITEM_EMPTY : ITEM, grid));
    }
    text(draw, &oracles_ui_controls_slots_note, &l.slots_note, oracles_ui_controls_slots, oracles_ui_rgba(TEXT, grid));

    /* The fixed shortcuts. */
    text(draw, &oracles_ui_controls_heading, &l.shortcuts_label, "In game", oracles_ui_rgb(HEADING));
    for (int i = 0; i < 6; i++) {
        const OraclesUiBox *k = &l.shortcut_keys[i];
        oracles_ui_stroke_round_rect(draw, k->x, k->y, k->w, k->h, 4.0f, 1.0f, oracles_ui_rgb(KEY_BORDER));
        text(draw, &oracles_ui_controls_shortcut_key, &l.shortcut_key_texts[i], oracles_controls_shortcuts[i][0], oracles_ui_rgb(KEY_TEXT));
        text(draw, &oracles_ui_controls_shortcut_label, &l.shortcut_labels[i], oracles_controls_shortcuts[i][1], oracles_ui_rgb(SHORTCUT));
    }
}

static int inside(const OraclesUiBox *b, float x, float y) { return x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h; }

int oracles_ui_controls_hit(const OraclesHomeNav *nav, float x, float y, int *column, int *row, int *option)
{
    OraclesUiControlsLayout l;
    oracles_ui_layout_controls(nav, &l);
    *option = -1;
    for (int c = 0; c < ORACLES_CONTROLS_COLUMNS; c++)
        for (int r = 0; r < ORACLES_HOME_BUTTONS; r++)
            if (l.cells[c][r].w > 0.0f && inside(&l.cells[c][r], x, y) && !oracles_controls_cell_locked(c, r)) { *column = c; *row = r; return 1; }
    if (inside(&l.reset, x, y)) { *column = 0; *row = ORACLES_CONTROLS_ROW_RESET; return 1; }
    for (int i = 0; i < 2; i++) if (inside(&l.mode_choices[i].box, x, y)) { *column = 2; *row = ORACLES_CONTROLS_ROW_MODE; *option = i; return 1; }
    if (inside(&l.mode, x, y)) { *column = 2; *row = ORACLES_CONTROLS_ROW_MODE; return 1; }
    return 0;
}
