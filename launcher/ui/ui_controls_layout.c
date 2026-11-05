#include "ui_controls_layout.h"
#include "ui_page_nav.h"

#include "ui_page_layout.h"

#include <stdio.h>
#include <string.h>

/* Controls' text styles. */
const OraclesUiTextStyle oracles_ui_controls_title = { ORACLES_UI_FONT_SERIF, 80.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_prompt = { ORACLES_UI_FONT_SERIF, 26.0f, 0.2f, 1 };
const OraclesUiTextStyle oracles_ui_controls_heading = { ORACLES_UI_FONT_SERIF, 22.0f, 0.18f, 1 };
const OraclesUiTextStyle oracles_ui_controls_name = { ORACLES_UI_FONT_SANS_MEDIUM, 28.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_cell = { ORACLES_UI_FONT_SANS_MEDIUM, 26.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_cell_locked = { ORACLES_UI_FONT_SANS_MEDIUM, 22.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_cell_waiting = { ORACLES_UI_FONT_SANS_ITALIC, 26.0f, 0.0f, 0 };   /* a medium italic */
const OraclesUiTextStyle oracles_ui_controls_item = { ORACLES_UI_FONT_SANS, 25.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_item_empty = { ORACLES_UI_FONT_SANS_ITALIC, 25.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_slots_note = { ORACLES_UI_FONT_SANS, 23.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_shortcut_key = { ORACLES_UI_FONT_MONO, 20.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_mode_title = { ORACLES_UI_FONT_SANS_MEDIUM, 28.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_mode_text_style = { ORACLES_UI_FONT_SANS, 24.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_note = { ORACLES_UI_FONT_SANS_ITALIC, 22.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_chip = { ORACLES_UI_FONT_SERIF, 25.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_reset = { ORACLES_UI_FONT_SERIF, 32.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_shortcut_label = { ORACLES_UI_FONT_SANS, 24.0f, 0.0f, 0 };
/* 4:3's; the others are the pages' (ui_page_layout.h): a name is a row's title, a chip and Reset a row's button. */
const OraclesUiTextStyle oracles_ui_controls_title_4_3 = { ORACLES_UI_FONT_SERIF, 54.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_heading_4_3 = { ORACLES_UI_FONT_SERIF, 30.0f, 0.14f, 1 };
const OraclesUiTextStyle oracles_ui_controls_cell_4_3 = { ORACLES_UI_FONT_SANS_MEDIUM, 32.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_cell_locked_4_3 = { ORACLES_UI_FONT_SANS_MEDIUM, 27.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_cell_waiting_4_3 = { ORACLES_UI_FONT_SANS_ITALIC, 32.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_shortcut_key_4_3 = { ORACLES_UI_FONT_MONO, 30.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_menu_cell = { ORACLES_UI_FONT_SANS_MEDIUM, 20.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_menu_cell_waiting = { ORACLES_UI_FONT_SANS_ITALIC, 20.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_menu_cell_4_3 = { ORACLES_UI_FONT_SANS_MEDIUM, 30.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_controls_menu_cell_waiting_4_3 = { ORACLES_UI_FONT_SANS_ITALIC, 30.0f, 0.0f, 0 };

static const OraclesUiControlsStyles styles_16_9 = {
    &oracles_ui_controls_title, &oracles_ui_controls_prompt, &oracles_ui_controls_heading, &oracles_ui_controls_name, &oracles_ui_controls_cell,
    &oracles_ui_controls_cell_locked, &oracles_ui_controls_cell_waiting, &oracles_ui_controls_item, &oracles_ui_controls_item_empty,
    &oracles_ui_controls_slots_note, &oracles_ui_controls_shortcut_key, &oracles_ui_controls_shortcut_label, &oracles_ui_controls_reset,
    &oracles_ui_controls_mode_title, &oracles_ui_controls_mode_text_style, &oracles_ui_controls_note, &oracles_ui_controls_chip, NULL,
    &oracles_ui_controls_menu_cell, &oracles_ui_controls_menu_cell_waiting,
};
/* The prompt is a heading's style, in the accent. */
static const OraclesUiControlsStyles styles_4_3 = {
    &oracles_ui_controls_title_4_3, &oracles_ui_controls_heading_4_3, &oracles_ui_controls_heading_4_3, &oracles_ui_row_title_4_3,
    &oracles_ui_controls_cell_4_3, &oracles_ui_controls_cell_locked_4_3, &oracles_ui_controls_cell_waiting_4_3, &oracles_ui_row_text_4_3,
    &oracles_ui_row_note_4_3, NULL, &oracles_ui_controls_shortcut_key_4_3, &oracles_ui_help_4_3, &oracles_ui_row_button_4_3,
    &oracles_ui_row_title_4_3, &oracles_ui_row_text_4_3, &oracles_ui_page_note_4_3, &oracles_ui_row_button_4_3, &oracles_ui_tab_4_3,
    &oracles_ui_controls_menu_cell_4_3, &oracles_ui_controls_menu_cell_waiting_4_3,
};

const OraclesUiControlsStyles *oracles_ui_controls_styles(OraclesUiLayout layout) { return layout == ORACLES_UI_LAYOUT_4_3 ? &styles_4_3 : &styles_16_9; }

const char oracles_ui_controls_mode_text[] = "A hotkey uses its item at once, without opening the menu.";
const char oracles_ui_controls_mode_note[] = "Loading a savestate drops an exchange under way; hotkeys keep working.";
const char oracles_ui_controls_slots[] = "Slots fill in game; the item column is read-only.";
const char oracles_ui_controls_hotkeys_off[] = "Off: turn it on to use an item with one key, without opening the inventory.";
const char *const oracles_ui_controls_mode_choices[2] = { "Off", "On" };
const char oracles_ui_controls_swim_text[] =
    "Hold a direction to swim at full speed, instead of tapping A with the flippers or the direction with the mermaid suit.";
const char oracles_ui_controls_swim_fan[] = "Fan games ignore it for now.";
const char oracles_ui_controls_original_only[] = "Needs an original ROM.";
const char *const oracles_ui_controls_swim_choices[2] = { "Tap", "Hold" };

/* Controls' geometry, in scene pixels. */
#define TITLE_X 96.0f
#define TITLE_Y 64.0f
#define TITLE_GAP 32.0f
#define PANEL_Y 190.0f
#define LEFT_X 96.0f
#define LEFT_W 800.0f
#define RIGHT_X 936.0f
#define RIGHT_W 888.0f
#define PANEL_PAD_X 32.0f
#define PANEL_PAD_Y 24.0f
#define BORDER 1.0f
#define PANEL_GAP 8.0f
#define GRID_GAP 16.0f
#define HEADING_PAD 6.0f
#define BUTTON_CELL_W 240.0f
#define HOTKEY_NAME_W 170.0f
#define HOTKEY_CELL_W 180.0f
#define CELL_H 56.0f                /* 54 and the borders */
#define WAITING_BORDER 2.0f         /* a cell waiting for a key: its border is 2 px, its row 2 px taller */
#define BIND_MARGIN 20.0f           /* above Bind to B */
#define RESET_MARGIN 10.0f
#define RESET_PAD_X 20.0f
#define RESET_PAD_Y 6.0f
#define RESET_GAP 14.0f
#define MODE_PAD_X 20.0f
#define MODE_PAD_Y 12.0f
#define MODE_GAP 24.0f
#define MODE_LINE_GAP 2.0f
#define MODE_MARGIN 8.0f            /* under the Item hotkeys line */
#define CHIP_PAD_X 16.0f
#define CHIP_PAD_Y 4.0f
#define CHIP_GAP 8.0f
#define SLOTS_MARGIN 12.0f
#define SWIM_MARGIN 14.0f           /* above Swimming, under Reset */
#define SWIM_MARGIN_IN_GAME 2.0f    /* in a fan game's pause, its reason's line takes back this room and its padding's */
#define SWIM_PAD_Y_IN_GAME 4.0f
#define SHORTCUTS_X (RIGHT_X + PANEL_PAD_X)
#define SHORTCUTS_W (RIGHT_W - 2.0f * PANEL_PAD_X)
#define SHORTCUTS_Y 930.0f
#define SHORTCUTS_GAP 28.0f
#define SHORTCUTS_LINE_GAP 10.0f
#define SHORTCUT_GAP 10.0f
#define MENU_CELL_MIN_W 64.0f       /* Menu's controller cell, its border within */
#define MENU_CELL_H 34.0f
#define MENU_CELL_PAD_X 12.0f
#define KEY_PAD_X 9.0f
#define KEY_PAD_Y 1.0f

/* 4:3's, in its 1440x1080 scene: every target 90 tall. */
#define LEFT_4_3 64.0f
#define WIDTH_4_3 1312.0f
#define HEADER_Y_4_3 24.0f
#define HEADER_H_4_3 90.0f
#define HEADER_GAP_4_3 40.0f       /* between the title and the tabs, or the prompt */
#define TITLE_LINE_4_3 54.0f
#define TAB_GAP_4_3 8.0f
#define TAB_PAD_X_4_3 24.0f
#define TAB_LINE_4_3 3.0f          /* the shown tab's line, the tab's bottom border */
#define GRID_Y_4_3 120.0f
#define GRID_PAD_X_4_3 20.0f
#define GRID_GAP_4_3 16.0f
#define HEADING_PAD_4_3 4.0f
#define CELL_H_4_3 90.0f           /* its border, 2 px while it waits, within */
#define HOTKEYS_OFF_PAD_4_3 10.0f  /* above the sentence under the hotkeys' grid while they are off */
#define BUTTON_CELL_W_4_3 340.0f
#define HOTKEY_NAME_W_4_3 220.0f
#define HOTKEY_CELL_W_4_3 260.0f
#define BIND_MARGIN_4_3 8.0f
#define RESET_MARGIN_4_3 4.0f
#define MODE_H_4_3 90.0f
#define MODE_TEXT_PAD_TOP_4_3 4.0f
#define MODE_TEXT_PAD_BOTTOM_4_3 10.0f
#define CHIP_MIN_W_4_3 110.0f
#define CHIP_PAD_X_4_3 22.0f
#define CHIP_PAD_Y_4_3 4.0f
#define CHIP_BORDER_4_3 2.0f
#define CHIP_GAP_4_3 12.0f
#define CHIP_LINE_4_3 (36.0f * 1.15f)
#define SHORTCUTS_Y_4_3 140.0f
#define SHORTCUTS_GAP_4_3 8.0f
#define SHORTCUTS_TITLE_PAD_4_3 8.0f
#define SHORTCUT_KEY_W_4_3 160.0f
#define SHORTCUT_GAP_4_3 24.0f
#define SHORTCUT_H_4_3 72.0f
#define MENU_CELL_W_4_3 180.0f     /* Menu's controller cell, at least, its column as wide */
#define MENU_ROW_H_4_3 90.0f
#define SWIM_MARGIN_4_3 16.0f      /* above Swimming, under the fixed keys */
#define KEY_PAD_X_4_3 14.0f
#define KEY_PAD_Y_4_3 2.0f

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

static float max_f(float a, float b) { return a > b ? a : b; }

/* A line centred in a box, as a flex item centred both ways. */
static OraclesUiLine centred(const OraclesUiTextStyle *style, const char *text, OraclesUiBox b)
{
    OraclesUiLine l = line_at(style, text, 0.0f, 0.0f, 0.0f);
    move_line(&l, b.x + (b.w - l.w) * 0.5f, b.y + (b.h - l.h) * 0.5f);
    return l;
}

const char *oracles_ui_controls_item_text(const OraclesHomeNav *nav, int slot)
{
    const char *name = oracles_controls_item(nav, slot);
    return name[0] ? name : "Empty";
}

static const OraclesUiTextStyle *cell_style(const OraclesHomeNav *nav, int column, int row)
{
    if (oracles_controls_cell_locked(column, row)) return &oracles_ui_controls_cell_locked;
    if (nav->controls.capturing && nav->controls.column == column && nav->controls.row == row) return &oracles_ui_controls_cell_waiting;
    return &oracles_ui_controls_cell;
}

static int waiting(const OraclesHomeNav *nav, int column, int row)
{
    return nav->controls.capturing && nav->controls.column == column && nav->controls.row == row;
}

/* A row's height: a waiting cell's thicker border makes it taller. */
static float row_height(const OraclesHomeNav *nav, int first_column, int row)
{
    return waiting(nav, first_column, row) || waiting(nav, first_column + 1, row) ? CELL_H + 2.0f * (WAITING_BORDER - 1.0f) : CELL_H;
}

/* A cell centred in its row (align-items: center), the row `height` tall from `y`. */
static void cell_at(const OraclesHomeNav *nav, OraclesUiControlsLayout *out, int column, int row, float x, float y, float height, float w)
{
    const float h = waiting(nav, column, row) ? CELL_H + 2.0f * (WAITING_BORDER - 1.0f) : CELL_H;
    out->cells[column][row] = box(x, y + (height - h) * 0.5f, w, h);
    out->cell_texts[column][row] = centred(cell_style(nav, column, row), oracles_controls_cell_text(nav, column, row), out->cells[column][row]);
}

/* A panel's headings on a grid's columns, aligned on their bottom; returns the rows' top. */
static float headings(float y, const char *const *texts, const float *x, const float *w, const int *centre, int count, OraclesUiLine **out)
{
    float height = 0.0f;
    for (int i = 0; i < count; i++) {
        *out[i] = line_at(&oracles_ui_controls_heading, texts[i], x[i], y, 0.0f);
        if (out[i]->h > height) height = out[i]->h;
    }
    for (int i = 0; i < count; i++) {
        OraclesUiLine *l = out[i];
        move_line(l, centre[i] ? x[i] + (w[i] - l->w) * 0.5f : x[i], y + height - l->h);
    }
    return y + height + HEADING_PAD + PANEL_GAP;
}

static void layout_title(const OraclesHomeNav *nav, OraclesUiControlsLayout *out)
{
    /* The title and the prompt share a baseline (align-items: baseline); the title's line box is its font size. */
    out->title = line_at(&oracles_ui_controls_title, "Controls", TITLE_X, TITLE_Y, 80.0f);
    out->prompt = line_at(&oracles_ui_controls_prompt, oracles_controls_prompt(nav), 0.0f, 0.0f, 0.0f);
    move_line(&out->prompt, TITLE_X + out->title.w + TITLE_GAP, out->title.baseline - (out->prompt.baseline - out->prompt.y));
}

/* Two choices framed at the right of a line from `y`, the second's right at `right`; returns their height. */
static float chips(const char *const *names, float right, float y, OraclesUiChipLayout *out)
{
    float x = right;
    for (int i = 1; i >= 0; i--) {
        OraclesUiChipLayout *c = &out[i];
        c->name = line_at(&oracles_ui_controls_chip, names[i], 0.0f, 0.0f, 0.0f);
        c->box = box(x - c->name.w - 2.0f * (CHIP_PAD_X + BORDER), y, c->name.w + 2.0f * (CHIP_PAD_X + BORDER), c->name.h + 2.0f * (CHIP_PAD_Y + BORDER));
        move_line(&c->name, c->box.x + BORDER + CHIP_PAD_X, y + BORDER + CHIP_PAD_Y);
        x = c->box.x - CHIP_GAP;
    }
    return out[0].box.h;
}

/* 4:3's: 90 tall, as Display's options. */
static void chips_4_3(const char *const *names, float right, float y, OraclesUiChipLayout *out)
{
    float x = right;
    for (int i = 1; i >= 0; i--) {
        OraclesUiChipLayout *c = &out[i];
        c->name = line_at(&oracles_ui_row_button_4_3, names[i], 0.0f, 0.0f, CHIP_LINE_4_3);
        const float w = max_f(CHIP_MIN_W_4_3, c->name.w + 2.0f * (CHIP_PAD_X_4_3 + CHIP_BORDER_4_3));
        c->box = box(x - w, y, w, MODE_H_4_3);
        move_line(&c->name, c->box.x + (w - c->name.w) * 0.5f, y + (MODE_H_4_3 - c->name.h) * 0.5f);
        x = c->box.x - CHIP_GAP_4_3;
    }
}

/* From the pause of a game whose profile has no swim's hold, no item hotkeys (a fan game): Swimming and the Item hotkeys
 * are dimmed and inert there, and say why in place of the fan games' lines; in an Oracle they apply to the game, unless
 * its ROM has neither (an unrecognised one), where they are dimmed and say why.  The line under each, NULL for none. */
static int pause_refuses(const OraclesHomeNav *nav, unsigned feature) { return nav->in_game && oracles_home_refuses(nav, feature); }
static const char *swim_later(const OraclesHomeNav *nav)
{
    return nav->swim_refused ? oracles_ui_controls_original_only : pause_refuses(nav, ORACLES_HOME_REFUSES_SWIM_HOLD) ? oracles_display_fan_reason : NULL;
}
static const char *mode_later(const OraclesHomeNav *nav)
{
    return nav->hotkeys_refused ? oracles_ui_controls_original_only : pause_refuses(nav, ORACLES_HOME_REFUSES_HOTKEYS) ? oracles_display_fan_reason : NULL;
}

/* The fan game shown from the home screen whose profile refuses the item hotkeys, which stay off: the line that says so,
 * else empty. */
static void fan_note(const OraclesHomeNav *nav, char *out, size_t capacity)
{
    const OraclesHomeFanGame *fan = oracles_home_fan_game(oracles_home_game(nav));
    if (fan && !nav->in_game && oracles_home_refuses(nav, ORACLES_HOME_REFUSES_HOTKEYS))
        snprintf(out, capacity, "Fan games refuse item hotkeys: they stay off for %s.", oracles_home_title(fan->hero));
    else if (capacity) out[0] = 0;
}

/* Under Swimming's explanation from `bottom`: in 4:3 the line of a fan game whose profile has no swim's hold, that it
 * does not apply there (16:9's Item hotkeys already say that fan games refuse them), or from its pause why it is dimmed;
 * returns their bottom. */
static float swim_lines(OraclesUiLayout layout, const OraclesHomeNav *nav, OraclesUiControlsLayout *out, float x, float bottom)
{
    const OraclesUiControlsStyles *st = oracles_ui_controls_styles(layout);
    if (layout == ORACLES_UI_LAYOUT_4_3 && !nav->in_game && oracles_home_refuses(nav, ORACLES_HOME_REFUSES_SWIM_HOLD)) {
        out->swim_fan = line_at(st->mode_text, oracles_ui_controls_swim_fan, x, bottom + MODE_LINE_GAP, 0.0f);
        bottom = out->swim_fan.y + out->swim_fan.h;
    }
    if (swim_later(nav)) {
        out->swim_later_text = swim_later(nav);
        out->swim_later = line_at(st->note, out->swim_later_text, x, bottom + MODE_LINE_GAP, 0.0f);
        bottom = out->swim_later.y + out->swim_later.h;
    }
    return bottom;
}

/* Swimming in the game buttons' panel from `y`: its title and Tap and Hold on one line, its explanation, its lines. */
static void swim_16_9(const OraclesHomeNav *nav, OraclesUiControlsLayout *out, float left, float width, float y)
{
    const float pad = swim_later(nav) ? SWIM_PAD_Y_IN_GAME : MODE_PAD_Y, inner_x = left + MODE_PAD_X, inner_y = y + pad;
    const float top_h = chips(oracles_ui_controls_swim_choices, left + width - MODE_PAD_X, inner_y, out->swim_choices);
    out->swim_title = line_at(&oracles_ui_controls_mode_title, "Swimming", inner_x, 0.0f, 0.0f);
    move_line(&out->swim_title, inner_x, inner_y + (top_h - out->swim_title.h) * 0.5f);
    oracles_ui_wrap_pretty(&oracles_ui_controls_mode_text_style, oracles_ui_controls_swim_text, width - 2.0f * MODE_PAD_X, inner_x,
                           inner_y + top_h + MODE_LINE_GAP, &out->swim_text);
    const float bottom = swim_lines(ORACLES_UI_LAYOUT_16_9, nav, out, inner_x, out->swim_text.lines[0].y + out->swim_text.h);
    out->swim = box(left, y, width, bottom + pad - y);
}

static void layout_buttons(const OraclesHomeNav *nav, OraclesUiControlsLayout *out)
{
    const float left = LEFT_X + BORDER + PANEL_PAD_X, width = LEFT_W - 2.0f * (BORDER + PANEL_PAD_X);
    const float name_w = width - 2.0f * BUTTON_CELL_W - 2.0f * GRID_GAP;
    const float x[3] = { left, left + name_w + GRID_GAP, left + name_w + GRID_GAP + BUTTON_CELL_W + GRID_GAP };
    const float w[3] = { name_w, BUTTON_CELL_W, BUTTON_CELL_W };
    const int centre[3] = { 0, 1, 1 };
    static const char *const texts[3] = { "Game buttons", "Keyboard", "Gamepad" };
    OraclesUiLine *lines[3] = { &out->heading_buttons, &out->heading_keyboard, &out->heading_gamepad };
    float y = headings(PANEL_Y + BORDER + PANEL_PAD_Y, texts, x, w, centre, 3, lines);
    for (int row = 0; row < ORACLES_HOME_BUTTONS; row++) {
        const float height = row_height(nav, 0, row);
        out->button_names[row] = line_at(&oracles_ui_controls_name, oracles_controls_buttons[row], x[0], 0.0f, 0.0f);
        move_line(&out->button_names[row], x[0], y + (height - out->button_names[row].h) * 0.5f);
        cell_at(nav, out, 0, row, x[1], y, height, BUTTON_CELL_W);
        cell_at(nav, out, 1, row, x[2], y, height, BUTTON_CELL_W);
        y += height + PANEL_GAP;
    }
    /* Reset to defaults, its dot at the right, 10 px under the rows. */
    OraclesUiLine dot = line_at(&oracles_ui_controls_reset, oracles_ui_item_dot, 0.0f, 0.0f, 0.0f);
    out->reset = box(left, y + RESET_MARGIN, width, dot.h + 2.0f * RESET_PAD_Y);
    move_line(&dot, left + width - RESET_PAD_X - dot.w, out->reset.y + RESET_PAD_Y);
    out->reset_dot = dot;
    out->reset_text = line_at(&oracles_ui_controls_reset, "Reset to defaults", 0.0f, 0.0f, 0.0f);
    move_line(&out->reset_text, dot.x - RESET_GAP - out->reset_text.w, dot.y);
    swim_16_9(nav, out, left, width, out->reset.y + out->reset.h + PANEL_GAP + (swim_later(nav) ? SWIM_MARGIN_IN_GAME : SWIM_MARGIN));
    out->left_panel = box(LEFT_X, PANEL_Y, LEFT_W, out->swim.y + out->swim.h + PANEL_PAD_Y + BORDER - PANEL_Y);
}

static void layout_hotkeys(const OraclesHomeNav *nav, OraclesUiControlsLayout *out)
{
    const float left = RIGHT_X + BORDER + PANEL_PAD_X, width = RIGHT_W - 2.0f * (BORDER + PANEL_PAD_X);
    float y = PANEL_Y + BORDER + PANEL_PAD_Y;
    /* The Item hotkeys line: its title and Off, On on one line; its explanation, its note, and a fan game's line that
     * they stay off, or from its pause why they are dimmed. */
    const float inner_x = left + MODE_PAD_X, inner_y = y + MODE_PAD_Y;
    const float top_h = chips(oracles_ui_controls_mode_choices, left + width - MODE_PAD_X, inner_y, out->mode_choices);
    out->mode_title = line_at(&oracles_ui_controls_mode_title, "Item hotkeys", inner_x, 0.0f, 0.0f);
    move_line(&out->mode_title, inner_x, inner_y + (top_h - out->mode_title.h) * 0.5f);
    out->mode_text = line_at(&oracles_ui_controls_mode_text_style, oracles_ui_controls_mode_text, inner_x, inner_y + top_h + MODE_LINE_GAP, 0.0f);
    out->mode_note = line_at(&oracles_ui_controls_note, oracles_ui_controls_mode_note, inner_x, out->mode_text.y + out->mode_text.h + MODE_LINE_GAP, 0.0f);
    float bottom = out->mode_note.y + out->mode_note.h;
    fan_note(nav, out->mode_fan_text, sizeof out->mode_fan_text);
    if (out->mode_fan_text[0]) {
        out->mode_fan = line_at(&oracles_ui_controls_mode_text_style, out->mode_fan_text, inner_x, bottom + MODE_LINE_GAP, 0.0f);
        bottom = out->mode_fan.y + out->mode_fan.h;
    }
    memset(&out->mode_later, 0, sizeof out->mode_later);
    if (mode_later(nav)) {
        out->mode_later_text = mode_later(nav);
        out->mode_later = line_at(&oracles_ui_controls_note, out->mode_later_text, inner_x, bottom + MODE_LINE_GAP, 0.0f);
        bottom = out->mode_later.y + out->mode_later.h;
    }
    out->mode = box(left, y, width, bottom + MODE_PAD_Y - y);
    y = out->mode.y + out->mode.h + MODE_MARGIN + PANEL_GAP;
    /* The slots and the two modifiers. */
    const float x[4] = { left, left + HOTKEY_NAME_W + GRID_GAP, left + HOTKEY_NAME_W + GRID_GAP + HOTKEY_CELL_W + GRID_GAP,
                         left + HOTKEY_NAME_W + 3.0f * GRID_GAP + 2.0f * HOTKEY_CELL_W };
    const float w[4] = { HOTKEY_NAME_W, HOTKEY_CELL_W, HOTKEY_CELL_W, left + width - x[3] };
    const int centre[4] = { 0, 1, 1, 0 };
    const char *const texts[4] = { "Hotkeys", "Keyboard", "Gamepad", oracles_controls_item_heading(nav) };
    OraclesUiLine *lines[4] = { &out->heading_hotkeys, &out->heading_hotkeys_keyboard, &out->heading_hotkeys_gamepad, &out->heading_items };
    y = headings(y, texts, x, w, centre, 4, lines);
    for (int row = 0; row < ORACLES_HOME_HOTKEY_ROWS; row++) {
        if (row == ORACLES_HOME_SLOTS) y += BIND_MARGIN;
        const float height = row_height(nav, 2, row);
        out->hotkey_names[row] = line_at(&oracles_ui_controls_name, oracles_controls_hotkey_rows[row], x[0], 0.0f, 0.0f);
        move_line(&out->hotkey_names[row], x[0], y + (height - out->hotkey_names[row].h) * 0.5f);
        cell_at(nav, out, 2, row, x[1], y, height, HOTKEY_CELL_W);
        if (row < ORACLES_HOME_SLOTS) {
            cell_at(nav, out, 3, row, x[2], y, height, HOTKEY_CELL_W);
            const int empty = !oracles_controls_item(nav, row)[0];
            const OraclesUiTextStyle *style = empty ? &oracles_ui_controls_item_empty : &oracles_ui_controls_item;
            out->items[row] = line_at(style, oracles_ui_controls_item_text(nav, row), x[3], 0.0f, 0.0f);
            move_line(&out->items[row], x[3], y + (height - out->items[row].h) * 0.5f);
        }
        y += height + PANEL_GAP;
    }
    out->slots_note = line_at(&oracles_ui_controls_slots_note, oracles_ui_controls_slots, left, y + SLOTS_MARGIN, 0.0f);   /* y is under the last row and its gap */
    bottom = out->slots_note.y + out->slots_note.h;
    memset(&out->hotkeys_off, 0, sizeof out->hotkeys_off);
    if (oracles_controls_hotkeys_off_line(nav)) {
        out->hotkeys_off = line_at(&oracles_ui_controls_mode_text_style, oracles_ui_controls_hotkeys_off, left, bottom + PANEL_GAP, 0.0f);
        bottom = out->hotkeys_off.y + out->hotkeys_off.h;
    }
    out->right_panel = box(RIGHT_X, PANEL_Y, RIGHT_W, bottom + PANEL_PAD_Y + BORDER - PANEL_Y);
}

/* Menu's controller cell, `min_w` wide at least and `h` tall, its border 2 px while it waits (within its box): its box
 * at (x, y) and its text centred in it. */
static void menu_cell(const OraclesHomeNav *nav, OraclesUiControlsLayout *out, const OraclesUiTextStyle *style, const OraclesUiTextStyle *waiting_style,
                      float min_w, float h, float x, float y)
{
    const int waits = waiting(nav, nav->controls.column, ORACLES_CONTROLS_ROW_MENU);
    const char *text = oracles_controls_cell_text(nav, 0, ORACLES_CONTROLS_ROW_MENU);
    const float border = waits ? WAITING_BORDER : BORDER;
    const float w = max_f(min_w, oracles_ui_text_width(waits ? waiting_style : style, text) + 2.0f * (MENU_CELL_PAD_X + border));
    out->menu_pad = box(x, y, w, h);
    out->menu_pad_text = centred(waits ? waiting_style : style, text, out->menu_pad);
}

/* A fixed key's frame and its text at (0, 0), in 16:9's style, or `none` (zero) when the screen does not recall it. */
static void key_frame(const char *key, int shown, OraclesUiBox *frame, OraclesUiLine *text)
{
    if (!shown) { memset(frame, 0, sizeof *frame); memset(text, 0, sizeof *text); return; }
    *text = line_at(&oracles_ui_controls_shortcut_key, key, 0.0f, 0.0f, 0.0f);
    *frame = box(0.0f, 0.0f, text->w + 2.0f * (KEY_PAD_X + BORDER), text->h + 2.0f * (KEY_PAD_Y + BORDER));
}

/* A fixed key's frame and its text moved to (x, y). */
static void place_key(OraclesUiBox *frame, OraclesUiLine *text, float x, float y)
{
    frame->x = x;
    frame->y = y;
    move_line(text, x + BORDER + KEY_PAD_X, y + BORDER + KEY_PAD_Y);
}

/* The fixed shortcuts under the right panel: "In game", then Menu (Esc framed, Menu's controller cell, on Android Back
 * framed, its meaning), then each other key framed and its meaning, wrapped in the panel's width as a flex row does,
 * each line centred on its tallest piece; the keys the screen does not recall (oracles_controls_shortcut_shown) take no
 * room, Menu's row then on one line. */
static void layout_shortcuts(const OraclesHomeNav *nav, OraclesUiControlsLayout *out)
{
    enum { PIECES = 7, ESC = 5 };   /* the label, Menu, the five other keys with their meanings */
    out->shortcuts_text = "In game";
    out->shortcuts_label = line_at(&oracles_ui_controls_heading, out->shortcuts_text, 0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 6; i++) {
        const int shown = oracles_controls_shortcut_shown(nav, i);
        key_frame(oracles_controls_shortcuts[i][0], shown, &out->shortcut_keys[i], &out->shortcut_key_texts[i]);
        if (shown || i == ESC) out->shortcut_labels[i] = line_at(&oracles_ui_controls_shortcut_label, oracles_controls_shortcuts[i][1], 0.0f, 0.0f, 0.0f);
    }
    key_frame("Back", nav->android, &out->menu_back, &out->menu_back_text);
    menu_cell(nav, out, &oracles_ui_controls_menu_cell, &oracles_ui_controls_menu_cell_waiting, MENU_CELL_MIN_W, MENU_CELL_H, 0.0f, 0.0f);
    /* The pieces in their order (-1 the label, ESC Menu, else a key; those not recalled left out), their widths and
     * heights. */
    int which[PIECES], pieces = 0;
    float w[PIECES], h[PIECES];
    which[pieces++] = -1;
    which[pieces++] = ESC;
    for (int i = 0; i < ESC; i++) if (out->shortcut_keys[i].w > 0.0f) which[pieces++] = i;
    for (int p = 0; p < pieces; p++) {
        const int i = which[p];
        if (i < 0) { w[p] = out->shortcuts_label.w; h[p] = out->shortcuts_label.h; continue; }
        const OraclesUiLine *label = &out->shortcut_labels[i];
        const float key_w = out->shortcut_keys[i].w;
        w[p] = (key_w > 0.0f ? key_w + SHORTCUT_GAP : 0.0f) + label->w;
        h[p] = max_f(out->shortcut_keys[i].h, label->h);
        if (i == ESC) {
            w[p] += out->menu_pad.w + SHORTCUT_GAP + (out->menu_back.w > 0.0f ? out->menu_back.w + SHORTCUT_GAP : 0.0f);
            h[p] = max_f(max_f(h[p], out->menu_pad.h), out->menu_back.h);
        }
    }
    float y = SHORTCUTS_Y;
    for (int first = 0; first < pieces;) {
        int last = first;
        float line_w = w[first], line_h = h[first];
        while (last + 1 < pieces && line_w + SHORTCUTS_GAP + w[last + 1] <= SHORTCUTS_W) {
            line_w += SHORTCUTS_GAP + w[++last];
            line_h = max_f(line_h, h[last]);
        }
        const float middle = y + line_h * 0.5f;
        float x = SHORTCUTS_X;
        for (int p = first; p <= last; x += w[p] + SHORTCUTS_GAP, p++) {
            const int i = which[p];
            if (i < 0) { move_line(&out->shortcuts_label, x, middle - out->shortcuts_label.h * 0.5f); continue; }
            OraclesUiBox *k = &out->shortcut_keys[i];
            OraclesUiLine *label = &out->shortcut_labels[i];
            float label_x = x;
            if (k->w > 0.0f) {
                place_key(k, &out->shortcut_key_texts[i], x, middle - k->h * 0.5f);
                label_x += k->w + SHORTCUT_GAP;
            }
            if (i == ESC) {
                menu_cell(nav, out, &oracles_ui_controls_menu_cell, &oracles_ui_controls_menu_cell_waiting, MENU_CELL_MIN_W, MENU_CELL_H, label_x,
                          middle - MENU_CELL_H * 0.5f);
                label_x += out->menu_pad.w + SHORTCUT_GAP;
                if (out->menu_back.w > 0.0f) {
                    place_key(&out->menu_back, &out->menu_back_text, label_x, middle - out->menu_back.h * 0.5f);
                    label_x += out->menu_back.w + SHORTCUT_GAP;
                }
            }
            move_line(label, label_x, middle - label->h * 0.5f);
        }
        y += line_h + SHORTCUTS_LINE_GAP;
        first = last + 1;
    }
}

/* 4:3's header, 90 tall: the title, then the tabs or, during a capture, the prompt, all centred on it. */
static void header_4_3(const OraclesHomeNav *nav, OraclesUiControlsLayout *out)
{
    out->title = line_at(&oracles_ui_controls_title_4_3, "Controls", LEFT_4_3, HEADER_Y_4_3 + (HEADER_H_4_3 - TITLE_LINE_4_3) * 0.5f, TITLE_LINE_4_3);
    const float x = LEFT_4_3 + out->title.w + HEADER_GAP_4_3;
    if (nav->controls.capturing) {
        out->prompt = line_at(&oracles_ui_controls_heading_4_3, oracles_controls_prompt(nav), x, 0.0f, 0.0f);
        move_line(&out->prompt, x, HEADER_Y_4_3 + (HEADER_H_4_3 - out->prompt.h) * 0.5f);
        return;
    }
    float tab_x = x;
    for (int i = 0; i < ORACLES_CONTROLS_TABS; i++) {
        OraclesUiLine *label = &out->tab_labels[i];
        *label = line_at(&oracles_ui_tab_4_3, oracles_controls_tab_names[i], tab_x + TAB_PAD_X_4_3, 0.0f, 0.0f);
        move_line(label, label->x, HEADER_Y_4_3 + (HEADER_H_4_3 - TAB_LINE_4_3 - label->h) * 0.5f);
        out->tabs[i] = box(tab_x, HEADER_Y_4_3, label->w + 2.0f * TAB_PAD_X_4_3, HEADER_H_4_3);
        out->tab_lines[i] = box(tab_x, HEADER_Y_4_3 + HEADER_H_4_3 - TAB_LINE_4_3, out->tabs[i].w, TAB_LINE_4_3);
        tab_x += out->tabs[i].w + TAB_GAP_4_3;
    }
}

/* A row of the grid 90 tall from `y`: its name and its cells centred in it. */
static void grid_row_4_3(const OraclesHomeNav *nav, OraclesUiControlsLayout *out, OraclesUiLine *name, const char *text, int first_column, int row,
                         const float *x, float cell_w, float y)
{
    *name = line_at(&oracles_ui_row_title_4_3, text, x[0], 0.0f, 0.0f);
    move_line(name, x[0], y + (CELL_H_4_3 - name->h) * 0.5f);
    for (int k = 0; k < 2; k++) {
        const int column = first_column + k;
        if (!oracles_controls_cell_exists(column, row)) continue;
        out->cells[column][row] = box(x[1 + k], y, cell_w, CELL_H_4_3);
        const OraclesUiTextStyle *style = oracles_controls_cell_locked(column, row) ? &oracles_ui_controls_cell_locked_4_3
                                          : waiting(nav, column, row) ? &oracles_ui_controls_cell_waiting_4_3 : &oracles_ui_controls_cell_4_3;
        out->cell_texts[column][row] = centred(style, oracles_controls_cell_text(nav, column, row), out->cells[column][row]);
    }
}

/* A grid's headings on one line from `y`, Keyboard and Gamepad centred on their columns; returns the rows' top. */
static float headings_4_3(float y, const char *const *texts, const float *x, const float *w, int count, OraclesUiLine **out)
{
    float height = 0.0f;
    for (int i = 0; i < count; i++) {
        *out[i] = line_at(&oracles_ui_controls_heading_4_3, texts[i], x[i], y, 0.0f);
        if (i == 1 || i == 2) move_line(out[i], x[i] + (w[i] - out[i]->w) * 0.5f, y);
        height = max_f(height, out[i]->h);
    }
    return y + height + HEADING_PAD_4_3;
}

static void buttons_4_3(const OraclesHomeNav *nav, OraclesUiControlsLayout *out)
{
    const float left = LEFT_4_3 + GRID_PAD_X_4_3, width = WIDTH_4_3 - 2.0f * GRID_PAD_X_4_3;
    const float name_w = width - 2.0f * (BUTTON_CELL_W_4_3 + GRID_GAP_4_3);
    const float x[3] = { left, left + name_w + GRID_GAP_4_3, left + name_w + 2.0f * GRID_GAP_4_3 + BUTTON_CELL_W_4_3 };
    const float w[3] = { name_w, BUTTON_CELL_W_4_3, BUTTON_CELL_W_4_3 };
    static const char *const texts[3] = { "Game buttons", "Keyboard", "Gamepad" };
    OraclesUiLine *lines[3] = { &out->heading_buttons, &out->heading_keyboard, &out->heading_gamepad };
    float y = headings_4_3(GRID_Y_4_3, texts, x, w, 3, lines);
    for (int row = 0; row < ORACLES_HOME_BUTTONS; row++, y += CELL_H_4_3)
        grid_row_4_3(nav, out, &out->button_names[row], oracles_controls_buttons[row], 0, row, x, BUTTON_CELL_W_4_3, y);
    /* Reset to defaults across the grid, its dot at the right, both centred in its 90. */
    out->reset = box(LEFT_4_3, y + RESET_MARGIN_4_3, WIDTH_4_3, CELL_H_4_3);
    out->reset_dot = line_at(&oracles_ui_row_button_4_3, oracles_ui_item_dot, 0.0f, 0.0f, 0.0f);
    move_line(&out->reset_dot, left + width - out->reset_dot.w, out->reset.y + (out->reset.h - out->reset_dot.h) * 0.5f);
    out->reset_text = line_at(&oracles_ui_row_button_4_3, "Reset to defaults", 0.0f, 0.0f, 0.0f);
    move_line(&out->reset_text, out->reset_dot.x - RESET_GAP - out->reset_text.w, out->reset_dot.y);
}

static void hotkeys_4_3(const OraclesHomeNav *nav, OraclesUiControlsLayout *out)
{
    const float left = LEFT_4_3 + GRID_PAD_X_4_3, right = LEFT_4_3 + WIDTH_4_3 - GRID_PAD_X_4_3;
    /* The Item hotkeys line, 90 tall: its title at the left, Off and On at the right, framed as Display's options. */
    out->mode = box(LEFT_4_3, GRID_Y_4_3, WIDTH_4_3, MODE_H_4_3);
    chips_4_3(oracles_ui_controls_mode_choices, right, out->mode.y, out->mode_choices);
    out->mode_title = line_at(&oracles_ui_row_title_4_3, "Item hotkeys", left, 0.0f, 0.0f);
    move_line(&out->mode_title, left, out->mode.y + (MODE_H_4_3 - out->mode_title.h) * 0.5f);
    /* Under it, its explanation and its note; a fan game's line, or from its pause why it is dimmed. */
    out->mode_text = line_at(&oracles_ui_row_text_4_3, oracles_ui_controls_mode_text, left, out->mode.y + MODE_H_4_3 + MODE_TEXT_PAD_TOP_4_3, 0.0f);
    out->mode_note = line_at(&oracles_ui_page_note_4_3, oracles_ui_controls_mode_note, left, out->mode_text.y + out->mode_text.h + MODE_LINE_GAP, 0.0f);
    float bottom = out->mode_note.y + out->mode_note.h;
    fan_note(nav, out->mode_fan_text, sizeof out->mode_fan_text);
    if (out->mode_fan_text[0]) {
        out->mode_fan = line_at(&oracles_ui_row_text_4_3, out->mode_fan_text, left, bottom + MODE_LINE_GAP, 0.0f);
        bottom = out->mode_fan.y + out->mode_fan.h;
    }
    if (mode_later(nav)) {
        out->mode_later_text = mode_later(nav);
        out->mode_later = line_at(&oracles_ui_page_note_4_3, out->mode_later_text, left, bottom + MODE_LINE_GAP, 0.0f);
        bottom = out->mode_later.y + out->mode_later.h;
    }
    /* The slots and the two modifiers, Bind to B 8 lower. */
    const float x[4] = { left, left + HOTKEY_NAME_W_4_3 + GRID_GAP_4_3, left + HOTKEY_NAME_W_4_3 + 2.0f * GRID_GAP_4_3 + HOTKEY_CELL_W_4_3,
                         left + HOTKEY_NAME_W_4_3 + 3.0f * GRID_GAP_4_3 + 2.0f * HOTKEY_CELL_W_4_3 };
    const float w[4] = { HOTKEY_NAME_W_4_3, HOTKEY_CELL_W_4_3, HOTKEY_CELL_W_4_3, right - x[3] };
    const char *const texts[4] = { "Hotkeys", "Keyboard", "Gamepad", oracles_controls_item_heading(nav) };
    OraclesUiLine *lines[4] = { &out->heading_hotkeys, &out->heading_hotkeys_keyboard, &out->heading_hotkeys_gamepad, &out->heading_items };
    float y = headings_4_3(bottom + MODE_TEXT_PAD_BOTTOM_4_3, texts, x, w, 4, lines);
    for (int row = 0; row < ORACLES_HOME_HOTKEY_ROWS; row++, y += CELL_H_4_3) {
        if (row == ORACLES_HOME_SLOTS) y += BIND_MARGIN_4_3;
        grid_row_4_3(nav, out, &out->hotkey_names[row], oracles_controls_hotkey_rows[row], 2, row, x, HOTKEY_CELL_W_4_3, y);
        if (row >= ORACLES_HOME_SLOTS) continue;
        const int empty = !oracles_controls_item(nav, row)[0];
        out->items[row] = line_at(empty ? &oracles_ui_row_note_4_3 : &oracles_ui_row_text_4_3, oracles_ui_controls_item_text(nav, row), x[3], 0.0f, 0.0f);
        move_line(&out->items[row], x[3], y + (CELL_H_4_3 - out->items[row].h) * 0.5f);
    }
    memset(&out->hotkeys_off, 0, sizeof out->hotkeys_off);
    if (oracles_controls_hotkeys_off_line(nav))   /* y is under the last row */
        out->hotkeys_off = line_at(&oracles_ui_row_text_4_3, oracles_ui_controls_hotkeys_off, left, y + HOTKEYS_OFF_PAD_4_3, 0.0f);
}

/* In game: "Fixed keys in game", then Menu's row of 90, Esc framed, Menu's controller cell and its meaning, then a
 * row of 72 for each other key, framed, and its meaning, all centred in their rows; then Swimming, its title and Tap and
 * Hold on a line 90 tall, its explanation and its lines under them.  Every row has the keys' column, the
 * controller's (Menu's cell) and the meaning's, so that every meaning starts at the same place; the keys the screen
 * does not recall (oracles_controls_shortcut_shown) take no row, Esc's leaving its column empty. */
static void shortcuts_4_3(const OraclesHomeNav *nav, OraclesUiControlsLayout *out)
{
    const float left = LEFT_4_3 + GRID_PAD_X_4_3;
    out->shortcuts_text = "Fixed keys in game";
    out->shortcuts_label = line_at(&oracles_ui_controls_heading_4_3, out->shortcuts_text, left, SHORTCUTS_Y_4_3, 0.0f);
    float y = out->shortcuts_label.y + out->shortcuts_label.h + SHORTCUTS_TITLE_PAD_4_3 + SHORTCUTS_GAP_4_3;
    static const int order[6] = { 5, 0, 1, 2, 3, 4 };   /* Esc first */
    for (int n = 0; n < 6; n++) {
        const int i = order[n], menu = i == 5, shown = oracles_controls_shortcut_shown(nav, i);
        if (!shown && !menu) continue;
        OraclesUiLine *key = &out->shortcut_key_texts[i], *label = &out->shortcut_labels[i];
        float key_w = 0.0f, key_h = 0.0f;
        if (shown) {
            *key = line_at(&oracles_ui_controls_shortcut_key_4_3, oracles_controls_shortcuts[i][0], 0.0f, 0.0f, 0.0f);
            key_w = key->w + 2.0f * (KEY_PAD_X_4_3 + BORDER);
            key_h = key->h + 2.0f * (KEY_PAD_Y_4_3 + BORDER);
        }
        /* Three columns in every row, empty where a row has nothing for them: the keys', the controller's (Menu's
         * cell), the meaning's. */
        const float cell_x = left + SHORTCUT_KEY_W_4_3 + SHORTCUT_GAP_4_3;
        const float column = cell_x + MENU_CELL_W_4_3 + SHORTCUT_GAP_4_3;
        const float label_w = LEFT_4_3 + WIDTH_4_3 - GRID_PAD_X_4_3 - column;
        float label_h;
        if (menu) {
            oracles_ui_wrap(&oracles_ui_help_4_3, oracles_controls_menu_label(nav), label_w, column, 0.0f, &out->menu_label);
            label_h = out->menu_label.h;
        } else {
            *label = line_at(&oracles_ui_help_4_3, oracles_controls_shortcuts[i][1], column, 0.0f, 0.0f);
            label_h = label->h;
        }
        const float height = max_f(menu ? MENU_ROW_H_4_3 : SHORTCUT_H_4_3, max_f(key_h, label_h));
        if (shown) {
            out->shortcut_keys[i] = box(left, y + (height - key_h) * 0.5f, key_w, key_h);
            move_line(key, left + BORDER + KEY_PAD_X_4_3, out->shortcut_keys[i].y + BORDER + KEY_PAD_Y_4_3);
        }
        if (menu) {
            menu_cell(nav, out, &oracles_ui_controls_menu_cell_4_3, &oracles_ui_controls_menu_cell_waiting_4_3, MENU_CELL_W_4_3, MENU_ROW_H_4_3, cell_x,
                      y + (height - MENU_ROW_H_4_3) * 0.5f);
            const float top = y + (height - label_h) * 0.5f;
            for (unsigned l = 0; l < out->menu_label.count; l++) move_line(&out->menu_label.lines[l], column, out->menu_label.lines[l].y + top);
            *label = out->menu_label.lines[0];
        } else {
            move_line(label, column, y + (height - label->h) * 0.5f);
        }
        y += height + SHORTCUTS_GAP_4_3;
    }
    out->swim = box(LEFT_4_3, y + SWIM_MARGIN_4_3, WIDTH_4_3, MODE_H_4_3);
    chips_4_3(oracles_ui_controls_swim_choices, LEFT_4_3 + WIDTH_4_3 - GRID_PAD_X_4_3, out->swim.y, out->swim_choices);
    out->swim_title = line_at(&oracles_ui_row_title_4_3, "Swimming", left, 0.0f, 0.0f);
    move_line(&out->swim_title, left, out->swim.y + (MODE_H_4_3 - out->swim_title.h) * 0.5f);
    oracles_ui_wrap_pretty(&oracles_ui_row_text_4_3, oracles_ui_controls_swim_text, WIDTH_4_3 - 2.0f * GRID_PAD_X_4_3, left,
                           out->swim.y + MODE_H_4_3 + MODE_TEXT_PAD_TOP_4_3, &out->swim_text);
    const float bottom = swim_lines(ORACLES_UI_LAYOUT_4_3, nav, out, left, out->swim_text.lines[0].y + out->swim_text.h);
    out->swim.h = bottom + MODE_TEXT_PAD_BOTTOM_4_3 - out->swim.y;
}

void oracles_ui_layout_controls(OraclesUiLayout layout, const OraclesHomeNav *nav, OraclesUiControlsLayout *out)
{
    memset(out, 0, sizeof *out);
    if (layout == ORACLES_UI_LAYOUT_4_3) {
        header_4_3(nav, out);
        switch (oracles_controls_tab(nav)) {
            case ORACLES_CONTROLS_TAB_BUTTONS: buttons_4_3(nav, out); break;
            case ORACLES_CONTROLS_TAB_HOTKEYS: hotkeys_4_3(nav, out); break;
            default: shortcuts_4_3(nav, out); break;
        }
        return;
    }
    layout_title(nav, out);
    layout_buttons(nav, out);
    layout_hotkeys(nav, out);
    layout_shortcuts(nav, out);
}
