#include "ui_controls_nav.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Controls' texts: the game buttons, the hotkeys' rows, the shortcuts. */
const char *const oracles_controls_buttons[ORACLES_HOME_BUTTONS] = { "Right", "Left", "Up", "Down", "A", "B", "Select", "Start" };
const char *const oracles_controls_hotkey_rows[ORACLES_HOME_HOTKEY_ROWS] = { "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Bind to B", "Bind to A" };
const char *const oracles_controls_default_keys[ORACLES_HOME_BUTTONS] = { "Right", "Left", "Up", "Down", "X", "Z", "Backspace", "Return" };
const char *const oracles_controls_default_pads[ORACLES_HOME_PAD_BUTTONS] = { "a", "b", "back", "start" };
/* Under the left hand that already holds Z (B) and X (A); on a controller, the buttons the game does not use. */
const char *const oracles_controls_default_hotkey_keys[ORACLES_HOME_HOTKEY_ROWS] = { "A", "S", "Q", "W", "Left Shift", "Left Ctrl" };
const char *const oracles_controls_default_hotkey_pads[ORACLES_HOME_SLOTS] = { "x", "y", "leftshoulder", "rightshoulder" };
const char *const oracles_controls_shortcuts[6][2] = {
    { "F2", "Color" }, { "F3", "Wide world or frame" }, { "F5", "Save state" }, { "F7", "Load state" }, { "F11", "Fullscreen" }, { "Esc", "Menu" },
};

/* Keys the game never gets: the fixed shortcuts, and Escape, which cancels a capture, as Android's Back ("AC Back") does there. */
static const char *const fixed_keys[] = { "F2", "F3", "F5", "F7", "F11" };
/* Buttons that always move. */
static const char *const moving_buttons[] = { "dpup", "dpdown", "dpleft", "dpright" };

static void copy_name(char *out, const char *name) { snprintf(out, ORACLES_HOME_NAME_LENGTH, "%s", name ? name : ""); }

static int same_name(const char *a, const char *b)
{
    if (!a[0] || !b[0]) return 0;
    for (; *a && *b; a++, b++) if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == *b;
}

static int listed(const char *name, const char *const *list, size_t count)
{
    for (size_t i = 0; i < count; i++) if (same_name(name, list[i])) return 1;
    return 0;
}

void oracles_controls_defaults(OraclesHomeControls *controls)
{
    for (int i = 0; i < ORACLES_HOME_BUTTONS; i++) copy_name(controls->keys[i], oracles_controls_default_keys[i]);
    for (int i = 0; i < ORACLES_HOME_PAD_BUTTONS; i++) copy_name(controls->pads[i], oracles_controls_default_pads[i]);
    for (int i = 0; i < ORACLES_HOME_HOTKEY_ROWS; i++) copy_name(controls->hotkey_keys[i], oracles_controls_default_hotkey_keys[i]);
    for (int i = 0; i < ORACLES_HOME_SLOTS; i++) copy_name(controls->hotkey_pads[i], oracles_controls_default_hotkey_pads[i]);
    memset(controls->items, 0, sizeof controls->items);
    controls->column = controls->row = 0;
    controls->capturing = 0;
}

void oracles_controls_open(OraclesHomeNav *nav)
{
    nav->screen = ORACLES_SCREEN_CONTROLS;
    nav->controls.column = nav->controls.row = 0;
    nav->controls.capturing = 0;
}

int oracles_controls_cell_exists(int column, int row)
{
    switch (column) {
        case 0: return row >= 0 && row <= ORACLES_CONTROLS_ROW_RESET;
        case 1: return row >= 0 && row <= ORACLES_CONTROLS_ROW_RESET;
        case 2: return row >= ORACLES_CONTROLS_ROW_MODE && row < ORACLES_HOME_HOTKEY_ROWS;
        case 3: return row >= ORACLES_CONTROLS_ROW_MODE && row < ORACLES_HOME_SLOTS;
        default: return 0;
    }
}

int oracles_controls_cell_locked(int column, int row) { return column == 1 && row >= 0 && row < ORACLES_HOME_BUTTONS - ORACLES_HOME_PAD_BUTTONS; }

/* The name a cell holds, NULL for Reset, the Item hotkeys line and the d-pad's. */
static char *name_of(OraclesHomeControls *c, int column, int row)
{
    if (row < 0 || row >= ORACLES_CONTROLS_ROW_RESET || oracles_controls_cell_locked(column, row)) return NULL;
    switch (column) {
        case 0: return c->keys[row];
        case 1: return c->pads[row - (ORACLES_HOME_BUTTONS - ORACLES_HOME_PAD_BUTTONS)];
        case 2: return row < ORACLES_HOME_HOTKEY_ROWS ? c->hotkey_keys[row] : NULL;
        case 3: return row < ORACLES_HOME_SLOTS ? c->hotkey_pads[row] : NULL;
        default: return NULL;
    }
}

const char *oracles_controls_cell_text(const OraclesHomeNav *nav, int column, int row)
{
    const OraclesHomeControls *c = &nav->controls;
    if (oracles_controls_cell_locked(column, row)) return "D-pad, left stick";
    if (c->capturing && c->column == column && c->row == row) return column == 0 || column == 2 ? "Press a key\xe2\x80\xa6" : "Press a button\xe2\x80\xa6";
    const char *name = name_of((OraclesHomeControls *)&nav->controls, column, row);
    if (!name) return "";
    return name[0] ? name : "\xe2\x80\x94";   /* — */
}

const char *oracles_controls_prompt(const OraclesHomeNav *nav)
{
    const OraclesHomeControls *c = &nav->controls;
    if (!c->capturing) return "";
    return c->column == 0 || c->column == 2 ? "Press a key \xc2\xb7 Esc cancels" : "Press a gamepad button \xc2\xb7 Esc cancels";
}

static int hotkey_game(const OraclesHomeNav *nav);

int oracles_controls_hotkeys_on(const OraclesHomeNav *nav)
{
    const int g = hotkey_game(nav);
    return g >= 0 && nav->games[g].hotkeys != 0;
}

const char *oracles_controls_item_heading(const OraclesHomeNav *nav)
{
    switch (oracles_home_hero(nav)) {
        case ORACLES_HOME_HERO_AGES: return "In Ages";
        case ORACLES_HOME_HERO_SEASONS: return "In Seasons";
        default: {   /* a fan game: no slot of its own yet */
            const OraclesHomeFanGame *fan = oracles_home_fan_game(oracles_home_game(nav));
            return fan ? fan->item_heading : "";
        }
    }
}

/* The game shown, when it has item hotkeys (the two Oracles), else -1. */
static int hotkey_game(const OraclesHomeNav *nav)
{
    const int g = oracles_home_game(nav);
    return g >= 0 && g < ORACLES_HOME_HOTKEY_GAMES ? g : -1;
}

const char *oracles_controls_item(const OraclesHomeNav *nav, int slot)
{
    const int g = hotkey_game(nav);
    return g >= 0 && slot >= 0 && slot < ORACLES_HOME_SLOTS ? nav->controls.items[g][slot] : "";
}

/* The cells the highlight goes to, in order (a tie for the nearest row goes to the first). */
typedef struct cell { int column, row; } cell;

static int cells(cell *out)
{
    int n = 0;
    for (int r = 0; r < ORACLES_HOME_BUTTONS; r++) out[n++] = (cell){ 0, r };
    for (int r = ORACLES_HOME_BUTTONS - ORACLES_HOME_PAD_BUTTONS; r < ORACLES_HOME_BUTTONS; r++) out[n++] = (cell){ 1, r };
    out[n++] = (cell){ 0, ORACLES_CONTROLS_ROW_RESET };
    out[n++] = (cell){ 1, ORACLES_CONTROLS_ROW_RESET };
    for (int r = 0; r < ORACLES_HOME_HOTKEY_ROWS; r++) out[n++] = (cell){ 2, r };
    for (int r = 0; r < ORACLES_HOME_SLOTS; r++) out[n++] = (cell){ 3, r };
    out[n++] = (cell){ 2, ORACLES_CONTROLS_ROW_MODE };
    out[n++] = (cell){ 3, ORACLES_CONTROLS_ROW_MODE };
    return n;
}

/* Reset and the Item hotkeys line are one place each, whatever column they were reached from. */
static void settle(OraclesHomeControls *c)
{
    if (c->row == ORACLES_CONTROLS_ROW_RESET) c->column = 0;
    if (c->row == ORACLES_CONTROLS_ROW_MODE) c->column = 2;
}

static void move(OraclesHomeControls *c, OraclesHomeAction action)
{
    cell list[32];
    const int count = cells(list);
    const cell *best = NULL;
    for (int i = 0; i < count; i++) {
        const cell *x = &list[i];
        int distance;
        if (action == ORACLES_HOME_UP || action == ORACLES_HOME_DOWN) {
            const int d = action == ORACLES_HOME_UP ? -1 : 1;
            if (x->column != c->column || (x->row - c->row) * d <= 0) continue;
            distance = (x->row - c->row) * d;
        } else {
            if (x->column != c->column + (action == ORACLES_HOME_LEFT ? -1 : 1)) continue;
            distance = x->row > c->row ? x->row - c->row : c->row - x->row;
        }
        const int best_distance = best ? (best->row > c->row ? best->row - c->row : c->row - best->row) : 0;
        if (!best || distance < best_distance) best = x;
    }
    if (!best) return;
    c->column = best->column;
    c->row = best->row;
    settle(c);
}

static OraclesHomeCommand toggle_hotkeys(OraclesHomeNav *nav, int on)
{
    /* A fan game's hotkeys stay off: no fan game's profile allows them yet. */
    const int g = hotkey_game(nav);
    if (g < 0) return ORACLES_HOME_STAY;
    const int now = nav->games[g].hotkeys != 0;
    if (now == on) return ORACLES_HOME_STAY;
    nav->games[g].hotkeys = on ? 1 : 0;   /* On is the use mode; equip stays the command line's */
    return ORACLES_HOME_STORE;
}

static OraclesHomeCommand reset(OraclesHomeNav *nav)
{
    OraclesHomeControls *c = &nav->controls;
    char items[2][ORACLES_HOME_SLOTS][ORACLES_HOME_NAME_LENGTH];
    const int column = c->column, row = c->row;
    memcpy(items, c->items, sizeof items);
    oracles_controls_defaults(c);
    memcpy(c->items, items, sizeof items);
    c->column = column;
    c->row = row;
    return ORACLES_HOME_STORE;
}

/* OK on the highlighted place: a cell waits, Reset resets, the Item hotkeys line turns. */
static OraclesHomeCommand activate(OraclesHomeNav *nav)
{
    OraclesHomeControls *c = &nav->controls;
    if (c->row == ORACLES_CONTROLS_ROW_MODE) return toggle_hotkeys(nav, !oracles_controls_hotkeys_on(nav));
    if (c->row == ORACLES_CONTROLS_ROW_RESET) return reset(nav);
    if (name_of(c, c->column, c->row)) c->capturing = 1;
    return ORACLES_HOME_STAY;
}

OraclesHomeCommand oracles_controls_act(OraclesHomeNav *nav, OraclesHomeAction action)
{
    OraclesHomeControls *c = &nav->controls;
    if (c->capturing) {
        if (action == ORACLES_HOME_BACK) c->capturing = 0;
        return ORACLES_HOME_STAY;
    }
    switch (action) {
        case ORACLES_HOME_LEFT:
        case ORACLES_HOME_RIGHT:
            if (c->row == ORACLES_CONTROLS_ROW_MODE) return toggle_hotkeys(nav, !oracles_controls_hotkeys_on(nav));
            move(c, action);
            return ORACLES_HOME_STAY;
        case ORACLES_HOME_UP:
        case ORACLES_HOME_DOWN: move(c, action); return ORACLES_HOME_STAY;
        case ORACLES_HOME_OK: return activate(nav);
        case ORACLES_HOME_BACK:
            /* Back to the menu Controls was opened from, on Controls. */
            nav->screen = nav->in_game ? ORACLES_SCREEN_PAUSE : ORACLES_SCREEN_HOME;
            nav->focus = nav->in_game ? 3 : 2;
            return ORACLES_HOME_STAY;
    }
    return ORACLES_HOME_STAY;
}

void oracles_controls_hover(OraclesHomeNav *nav, int column, int row)
{
    OraclesHomeControls *c = &nav->controls;
    if (c->capturing || !oracles_controls_cell_exists(column, row) || oracles_controls_cell_locked(column, row)) return;
    c->column = column;
    c->row = row;
    settle(c);
}

OraclesHomeCommand oracles_controls_click(OraclesHomeNav *nav, int column, int row, int option)
{
    OraclesHomeControls *c = &nav->controls;
    if (c->capturing || !oracles_controls_cell_exists(column, row) || oracles_controls_cell_locked(column, row)) return ORACLES_HOME_STAY;
    c->column = column;
    c->row = row;
    settle(c);
    if (row == ORACLES_CONTROLS_ROW_MODE) return option >= 0 ? toggle_hotkeys(nav, option == 1) : ORACLES_HOME_STAY;
    return activate(nav);
}

/* The waiting cell takes `name`; a cell of the same device that held it is left without one. */
static OraclesHomeCommand take(OraclesHomeControls *c, const char *name, int keyboard)
{
    char *target = name_of(c, c->column, c->row);
    if (!target) { c->capturing = 0; return ORACLES_HOME_STAY; }
    for (int column = keyboard ? 0 : 1; column < ORACLES_CONTROLS_COLUMNS; column += 2)
        for (int row = 0; row < ORACLES_CONTROLS_ROW_RESET; row++) {
            char *other = name_of(c, column, row);
            if (other && other != target && same_name(other, name)) other[0] = 0;
        }
    copy_name(target, name);
    c->capturing = 0;
    return ORACLES_HOME_STORE;
}

OraclesHomeCommand oracles_controls_capture_key(OraclesHomeNav *nav, const char *name)
{
    OraclesHomeControls *c = &nav->controls;
    if (!c->capturing || !name || !name[0]) return ORACLES_HOME_STAY;
#ifdef __ANDROID__
    if (same_name(name, "AC Back")) { c->capturing = 0; return ORACLES_HOME_STAY; }
#endif
    if (same_name(name, "Escape")) { c->capturing = 0; return ORACLES_HOME_STAY; }
    if ((c->column != 0 && c->column != 2) || listed(name, fixed_keys, sizeof fixed_keys / sizeof fixed_keys[0])) return ORACLES_HOME_STAY;
    return take(c, name, 1);
}

OraclesHomeCommand oracles_controls_capture_button(OraclesHomeNav *nav, const char *name)
{
    OraclesHomeControls *c = &nav->controls;
    if (!c->capturing || !name || !name[0]) return ORACLES_HOME_STAY;
    if ((c->column != 1 && c->column != 3) || listed(name, moving_buttons, sizeof moving_buttons / sizeof moving_buttons[0])) return ORACLES_HOME_STAY;
    return take(c, name, 0);
}

void oracles_controls_cancel(OraclesHomeNav *nav) { nav->controls.capturing = 0; }
