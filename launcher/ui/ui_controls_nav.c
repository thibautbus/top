#include "ui_controls_nav.h"

#include "ui_page_layout.h"

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
const char *const oracles_controls_default_menu_pad = "leftstick";
const char *const oracles_controls_tab_names[ORACLES_CONTROLS_TABS] = { "Buttons", "Hotkeys", "In game" };
const char *const oracles_controls_shortcuts[6][2] = {
    { "F2", "Color" }, { "F3", "Wide world or frame" }, { "F5", "Save state" }, { "F7", "Load state" }, { "F11", "Fullscreen" }, { "Esc", "Menu" },
};

int oracles_controls_shortcut_shown(const OraclesHomeNav *nav, int i)
{
    return oracles_home_keys_shown(nav) && !(nav->android && !strcmp(oracles_controls_shortcuts[i][0], "F11"));
}

const char *oracles_controls_menu_label(const OraclesHomeNav *nav) { return nav->android ? "Menu \xc2\xb7 also Back" : "Menu"; }

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
    copy_name(controls->menu_pad, oracles_controls_default_menu_pad);
    memset(controls->items, 0, sizeof controls->items);
    controls->swim_hold = 0;
    controls->menu_tip = controls->hotkeys_tip = 0;
    controls->column = controls->row = 0;
    controls->capturing = 0;
    controls->tab = ORACLES_CONTROLS_TAB_BUTTONS;
}

void oracles_controls_open(OraclesHomeNav *nav)
{
    nav->screen = ORACLES_SCREEN_CONTROLS;
    nav->controls.column = nav->controls.row = 0;
    nav->controls.capturing = 0;
    nav->controls.tab = ORACLES_CONTROLS_TAB_BUTTONS;
}

int oracles_controls_tab(const OraclesHomeNav *nav)
{
    const OraclesHomeControls *c = &nav->controls;
    if (c->row == ORACLES_CONTROLS_ROW_TAB) return c->tab;
    if (c->row == ORACLES_CONTROLS_ROW_SWIM || c->row == ORACLES_CONTROLS_ROW_MENU) return ORACLES_CONTROLS_TAB_IN_GAME;
    return c->column >= 2 ? ORACLES_CONTROLS_TAB_HOTKEYS : ORACLES_CONTROLS_TAB_BUTTONS;
}

int oracles_controls_cell_exists(int column, int row)
{
    if (row == ORACLES_CONTROLS_ROW_MENU) return column >= 0 && column < ORACLES_CONTROLS_COLUMNS;
    switch (column) {
        case 0:
        case 1: return row >= 0 && row <= ORACLES_CONTROLS_ROW_SWIM;
        case 2: return row >= ORACLES_CONTROLS_ROW_MODE && row < ORACLES_HOME_HOTKEY_ROWS;
        case 3: return row >= ORACLES_CONTROLS_ROW_MODE && row < ORACLES_HOME_SLOTS;
        default: return 0;
    }
}

int oracles_controls_cell_locked(int column, int row) { return column == 1 && row >= 0 && row < ORACLES_HOME_BUTTONS - ORACLES_HOME_PAD_BUTTONS; }

/* The name a cell holds, NULL for Reset, Swimming, the Item hotkeys line and the d-pad's. */
static char *name_of(OraclesHomeControls *c, int column, int row)
{
    if (row == ORACLES_CONTROLS_ROW_MENU) return c->menu_pad;
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
    if (row == ORACLES_CONTROLS_ROW_MENU && c->capturing && c->row == row) return "Press\xe2\x80\xa6";
    if (c->capturing && c->column == column && c->row == row) return column == 0 || column == 2 ? "Press a key\xe2\x80\xa6" : "Press a button\xe2\x80\xa6";
    const char *name = name_of((OraclesHomeControls *)&nav->controls, column, row);
    if (!name) return "";
    if (!name[0]) return "\xe2\x80\x94";   /* — */
    return column == 1 || column == 3 || row == ORACLES_CONTROLS_ROW_MENU ? oracles_controls_button_label(name) : name;
}

const char *oracles_controls_button_label(const char *name)
{
    static const char *const labels[][2] = {
        { "a", "A" }, { "b", "B" }, { "x", "X" }, { "y", "Y" }, { "back", "Back" }, { "start", "Start" }, { "guide", "Guide" },
        { "leftshoulder", "LB" }, { "rightshoulder", "RB" }, { "lefttrigger", "LT" }, { "righttrigger", "RT" },
        { "leftstick", "L3" }, { "rightstick", "R3" },
    };
    for (size_t i = 0; name && i < sizeof labels / sizeof labels[0]; i++) if (same_name(name, labels[i][0])) return labels[i][1];
    return name ? name : "";
}

void oracles_controls_menu_tip(int device, int android, const char *menu_pad, char *out, size_t capacity)
{
    char text[ORACLES_HOME_NAME_LENGTH + 32] = "Esc opens the menu";
    const char *button = menu_pad && menu_pad[0] ? oracles_controls_button_label(menu_pad) : NULL;
    if (device == ORACLES_INPUT_GAMEPAD && button) snprintf(text, sizeof text, "%s%s opens the menu", button, android ? " or Back" : "");
    else if (device == ORACLES_INPUT_GAMEPAD && !android) snprintf(text, sizeof text, "Guide opens the menu");   /* menu_pad= empty: Guide alone */
    else if (device == ORACLES_INPUT_POINTER && android) snprintf(text, sizeof text, "The pause button opens the menu");
    else if (android) snprintf(text, sizeof text, "Back opens the menu");   /* a handheld's buttons that come as keys: no Escape there */
    oracles_ui_copy_whole(out, capacity, text);
}

int oracles_controls_tip_shown(int count, int most) { return count < 0 || count + 1 >= most ? ORACLES_TIP_OFF : count + 1; }

int oracles_controls_menu_tip_due(int count) { return count >= 0 && count < ORACLES_MENU_TIPS; }

int oracles_controls_hotkeys_tip_due(const OraclesHomeNav *nav)
{
    const int game = oracles_home_game(nav);
    return nav->screen == ORACLES_SCREEN_PAUSE && nav->in_game && (game == ORACLES_HOME_GAME_AGES || game == ORACLES_HOME_GAME_SEASONS)
           && !nav->hotkeys_refused && !nav->games[game].hotkeys && !nav->narrow
           && nav->controls.hotkeys_tip >= 0 && nav->controls.hotkeys_tip < ORACLES_HOTKEYS_TIPS;
}

int oracles_controls_tips_opened(OraclesHomeNav *nav, int by_key)
{
    int changed = 0;
    if (by_key && nav->controls.menu_tip != ORACLES_TIP_OFF) {
        nav->controls.menu_tip = ORACLES_TIP_OFF;
        changed = 1;
    }
    nav->hotkeys_line = oracles_controls_hotkeys_tip_due(nav);
    if (nav->hotkeys_line && by_key) {
        nav->controls.hotkeys_tip = oracles_controls_tip_shown(nav->controls.hotkeys_tip, ORACLES_HOTKEYS_TIPS);
        changed = 1;
    }
    return changed;
}

int oracles_controls_hotkeys_line(const OraclesHomeNav *nav)
{
    return nav->screen == ORACLES_SCREEN_PAUSE && nav->hotkeys_line && !oracles_controls_hotkeys_on(nav) && !nav->narrow;
}

const char *oracles_controls_prompt(const OraclesHomeNav *nav)
{
    const OraclesHomeControls *c = &nav->controls;
    if (!c->capturing) return "";
    const int keyboard = (c->column == 0 || c->column == 2) && c->row != ORACLES_CONTROLS_ROW_MENU;
    if (keyboard) return "Press a key \xc2\xb7 Esc cancels";
    if (nav->device != ORACLES_INPUT_GAMEPAD) return "Press a gamepad button \xc2\xb7 Esc cancels";
    /* After a controller's input, the button that gives up (oracles_home_capture): the menu's, else Guide. */
    static char prompt[64];
    snprintf(prompt, sizeof prompt, "Press a gamepad button \xc2\xb7 %s cancels", c->menu_pad[0] ? oracles_controls_button_label(c->menu_pad) : "Guide");
    return prompt;
}

static int hotkey_game(const OraclesHomeNav *nav);

int oracles_controls_hotkeys_on(const OraclesHomeNav *nav)
{
    const int g = hotkey_game(nav);
    return g >= 0 && nav->games[g].hotkeys != 0;
}

int oracles_controls_hotkeys_off_line(const OraclesHomeNav *nav) { return hotkey_game(nav) >= 0 && !nav->hotkeys_refused && !oracles_controls_hotkeys_on(nav); }

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

/* The cells the highlight goes to, in order (a tie for the nearest row goes to the first): 16:9's grid, or 4:3's
 * shown tab, its strip first. */
typedef struct cell { int column, row; } cell;

static int cells(const OraclesHomeNav *nav, cell *out)
{
    int n = 0;
    if (nav->layout == ORACLES_UI_LAYOUT_4_3) {
        switch (oracles_controls_tab(nav)) {
            case ORACLES_CONTROLS_TAB_BUTTONS:
                out[n++] = (cell){ 0, ORACLES_CONTROLS_ROW_TAB };
                out[n++] = (cell){ 1, ORACLES_CONTROLS_ROW_TAB };
                for (int r = 0; r < ORACLES_HOME_BUTTONS; r++) out[n++] = (cell){ 0, r };
                for (int r = ORACLES_HOME_BUTTONS - ORACLES_HOME_PAD_BUTTONS; r < ORACLES_HOME_BUTTONS; r++) out[n++] = (cell){ 1, r };
                out[n++] = (cell){ 0, ORACLES_CONTROLS_ROW_RESET };
                out[n++] = (cell){ 1, ORACLES_CONTROLS_ROW_RESET };
                break;
            case ORACLES_CONTROLS_TAB_HOTKEYS:
                out[n++] = (cell){ 2, ORACLES_CONTROLS_ROW_TAB };
                out[n++] = (cell){ 3, ORACLES_CONTROLS_ROW_TAB };
                out[n++] = (cell){ 2, ORACLES_CONTROLS_ROW_MODE };
                out[n++] = (cell){ 3, ORACLES_CONTROLS_ROW_MODE };
                for (int r = 0; r < ORACLES_HOME_HOTKEY_ROWS; r++) out[n++] = (cell){ 2, r };
                for (int r = 0; r < ORACLES_HOME_SLOTS; r++) out[n++] = (cell){ 3, r };
                break;
            default:
                out[n++] = (cell){ 0, ORACLES_CONTROLS_ROW_TAB };
                out[n++] = (cell){ 0, ORACLES_CONTROLS_ROW_MENU };
                out[n++] = (cell){ 0, ORACLES_CONTROLS_ROW_SWIM };
                break;
        }
        return n;
    }
    for (int r = 0; r < ORACLES_HOME_BUTTONS; r++) out[n++] = (cell){ 0, r };
    for (int r = ORACLES_HOME_BUTTONS - ORACLES_HOME_PAD_BUTTONS; r < ORACLES_HOME_BUTTONS; r++) out[n++] = (cell){ 1, r };
    out[n++] = (cell){ 0, ORACLES_CONTROLS_ROW_RESET };
    out[n++] = (cell){ 1, ORACLES_CONTROLS_ROW_RESET };
    out[n++] = (cell){ 0, ORACLES_CONTROLS_ROW_SWIM };
    out[n++] = (cell){ 1, ORACLES_CONTROLS_ROW_SWIM };
    for (int r = 0; r < ORACLES_HOME_HOTKEY_ROWS; r++) out[n++] = (cell){ 2, r };
    for (int r = 0; r < ORACLES_HOME_SLOTS; r++) out[n++] = (cell){ 3, r };
    out[n++] = (cell){ 2, ORACLES_CONTROLS_ROW_MODE };
    out[n++] = (cell){ 3, ORACLES_CONTROLS_ROW_MODE };
    out[n++] = (cell){ 2, ORACLES_CONTROLS_ROW_MENU };
    out[n++] = (cell){ 3, ORACLES_CONTROLS_ROW_MENU };
    return n;
}

/* Where a row stands among its column's for the moves: Menu's button under the hotkeys' rows, beside Start's (16:9),
 * or first in In game's tab (4:3). */
static int place(const OraclesHomeNav *nav, int row)
{
    if (row != ORACLES_CONTROLS_ROW_MENU) return row;
    return nav->layout == ORACLES_UI_LAYOUT_4_3 ? 0 : ORACLES_HOME_BUTTONS - 1;
}

/* Reset, Swimming, the Item hotkeys line, Menu's button and the tabs' strip are one place each, whatever column they
 * were reached from. */
static void settle(const OraclesHomeNav *nav, OraclesHomeControls *c)
{
    if (c->row == ORACLES_CONTROLS_ROW_RESET || c->row == ORACLES_CONTROLS_ROW_SWIM) c->column = 0;
    if (c->row == ORACLES_CONTROLS_ROW_MODE) c->column = 2;
    if (c->row == ORACLES_CONTROLS_ROW_MENU) c->column = nav->layout == ORACLES_UI_LAYOUT_4_3 ? 0 : 2;
    if (c->row == ORACLES_CONTROLS_ROW_TAB) c->column = c->tab == ORACLES_CONTROLS_TAB_HOTKEYS ? 2 : 0;
}

static void move(OraclesHomeNav *nav, OraclesHomeAction action)
{
    OraclesHomeControls *c = &nav->controls;
    cell list[32];
    const int count = cells(nav, list);
    settle(nav, c);   /* the layout's own place, for a window that turned since */
    c->tab = oracles_controls_tab(nav);   /* the strip, when the move reaches it, holds the tab moved from */
    const int here = place(nav, c->row);
    const cell *best = NULL;
    int best_distance = 0;
    for (int i = 0; i < count; i++) {
        const cell *x = &list[i];
        const int there = place(nav, x->row);
        int distance;
        if (action == ORACLES_HOME_UP || action == ORACLES_HOME_DOWN) {
            const int d = action == ORACLES_HOME_UP ? -1 : 1;
            if (x->column != c->column || (there - here) * d <= 0) continue;
            distance = (there - here) * d;
        } else {
            if (x->column != c->column + (action == ORACLES_HOME_LEFT ? -1 : 1)) continue;
            distance = there > here ? there - here : here - there;
        }
        if (!best || distance < best_distance) { best = x; best_distance = distance; }
    }
    if (!best) return;
    c->column = best->column;
    c->row = best->row;
    settle(nav, c);
}

static OraclesHomeCommand toggle_hotkeys(OraclesHomeNav *nav, int on)
{
    /* A fan game's hotkeys stay off: no fan game's profile allows them yet; nor from the pause of a game whose profile has none. */
    const int g = hotkey_game(nav);
    if (g < 0 || nav->hotkeys_refused) return ORACLES_HOME_STAY;
    const int now = nav->games[g].hotkeys != 0;
    if (now == on) return ORACLES_HOME_STAY;
    nav->games[g].hotkeys = on ? 1 : 0;   /* On is the use mode; equip stays the command line's */
    if (on) {   /* found: the pause menu's line on them is done, Off again or not */
        nav->controls.hotkeys_tip = ORACLES_TIP_OFF;
        nav->hotkeys_line = 0;
    }
    return ORACLES_HOME_STORE;
}

/* Swimming: Tap (0) or Hold (1), for every game; not from a fan game's pause, the fan games having no swim's hold, nor
 * from the pause of an Oracle whose profile has none. */
static OraclesHomeCommand set_swim(OraclesHomeNav *nav, int hold)
{
    if (nav->controls.swim_hold == hold || nav->swim_refused || (nav->in_game && oracles_home_fan_game(oracles_home_game(nav)))) return ORACLES_HOME_STAY;
    nav->controls.swim_hold = hold;
    return ORACLES_HOME_STORE;
}

/* The keys and buttons, and the reminder and the pause menu's line due again; the slots' items and Swimming kept. */
static OraclesHomeCommand reset(OraclesHomeNav *nav)
{
    OraclesHomeControls *c = &nav->controls;
    char items[2][ORACLES_HOME_SLOTS][ORACLES_HOME_NAME_LENGTH];
    const int column = c->column, row = c->row, tab = c->tab, swim_hold = c->swim_hold;
    memcpy(items, c->items, sizeof items);
    oracles_controls_defaults(c);
    memcpy(c->items, items, sizeof items);
    c->swim_hold = swim_hold;
    c->column = column;
    c->row = row;
    c->tab = tab;
    return ORACLES_HOME_STORE;
}

/* OK on the highlighted place: a cell waits, Reset resets, the Item hotkeys line and Swimming turn. */
static OraclesHomeCommand activate(OraclesHomeNav *nav)
{
    OraclesHomeControls *c = &nav->controls;
    if (c->row == ORACLES_CONTROLS_ROW_MODE) return toggle_hotkeys(nav, !oracles_controls_hotkeys_on(nav));
    if (c->row == ORACLES_CONTROLS_ROW_SWIM) return set_swim(nav, !c->swim_hold);
    if (c->row == ORACLES_CONTROLS_ROW_RESET) return reset(nav);
    if (name_of(c, c->column, c->row)) c->capturing = 1;
    return ORACLES_HOME_STAY;
}

/* The tabs' strip: left, right and OK show the tab before or after, wrapping. */
static OraclesHomeCommand turn_tab(const OraclesHomeNav *nav, OraclesHomeControls *c, int step)
{
    c->tab = (c->tab + step + ORACLES_CONTROLS_TABS) % ORACLES_CONTROLS_TABS;
    settle(nav, c);
    return ORACLES_HOME_STAY;
}

OraclesHomeCommand oracles_controls_act(OraclesHomeNav *nav, OraclesHomeAction action)
{
    OraclesHomeControls *c = &nav->controls;
    if (c->capturing) {
        if (action == ORACLES_HOME_BACK) c->capturing = 0;
        return ORACLES_HOME_STAY;
    }
    if (c->row == ORACLES_CONTROLS_ROW_TAB && action != ORACLES_HOME_BACK) {
        if (nav->layout == ORACLES_UI_LAYOUT_4_3) {
            if (action == ORACLES_HOME_LEFT || action == ORACLES_HOME_RIGHT || action == ORACLES_HOME_OK)
                return turn_tab(nav, c, action == ORACLES_HOME_LEFT ? -1 : 1);
        } else {
            /* 16:9 has no strip: its tab's first cell. */
            const int hotkeys = c->tab == ORACLES_CONTROLS_TAB_HOTKEYS, in_game = c->tab == ORACLES_CONTROLS_TAB_IN_GAME;
            c->column = hotkeys || in_game ? 2 : 0;
            c->row = hotkeys ? ORACLES_CONTROLS_ROW_MODE : in_game ? ORACLES_CONTROLS_ROW_MENU : 0;
            return ORACLES_HOME_STAY;
        }
    }
    switch (action) {
        case ORACLES_HOME_LEFT:
        case ORACLES_HOME_RIGHT:
            if (c->row == ORACLES_CONTROLS_ROW_MODE) return toggle_hotkeys(nav, !oracles_controls_hotkeys_on(nav));
            if (c->row == ORACLES_CONTROLS_ROW_SWIM) return set_swim(nav, !c->swim_hold);
            move(nav, action);
            return ORACLES_HOME_STAY;
        case ORACLES_HOME_UP:
        case ORACLES_HOME_DOWN: move(nav, action); return ORACLES_HOME_STAY;
        case ORACLES_HOME_OK: return activate(nav);
        case ORACLES_HOME_BACK:
            /* Back to the menu Controls was opened from, on Controls. */
            nav->screen = nav->in_game ? ORACLES_SCREEN_PAUSE : ORACLES_SCREEN_HOME;
            oracles_home_focus_item(nav, ORACLES_HOME_ITEM_CONTROLS);
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
    settle(nav, c);
}

OraclesHomeCommand oracles_controls_click(OraclesHomeNav *nav, int column, int row, int option)
{
    OraclesHomeControls *c = &nav->controls;
    if (!c->capturing && row == ORACLES_CONTROLS_ROW_TAB && option >= 0 && option < ORACLES_CONTROLS_TABS) {
        c->tab = option;
        c->row = ORACLES_CONTROLS_ROW_TAB;
        settle(nav, c);
        return ORACLES_HOME_STAY;
    }
    if (c->capturing || !oracles_controls_cell_exists(column, row) || oracles_controls_cell_locked(column, row)) return ORACLES_HOME_STAY;
    c->column = column;
    c->row = row;
    settle(nav, c);
    if (row == ORACLES_CONTROLS_ROW_MODE) return option >= 0 ? toggle_hotkeys(nav, option == 1) : ORACLES_HOME_STAY;
    if (row == ORACLES_CONTROLS_ROW_SWIM) return option >= 0 ? set_swim(nav, option == 1) : ORACLES_HOME_STAY;
    return activate(nav);
}

/* The waiting cell takes `name`; a cell of the same device that held it is left without one.  Menu's button never
 * comes here for another cell: it gives up the capture (oracles_home_capture). */
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
    if ((c->column != 0 && c->column != 2) || c->row == ORACLES_CONTROLS_ROW_MENU || listed(name, fixed_keys, sizeof fixed_keys / sizeof fixed_keys[0]))
        return ORACLES_HOME_STAY;
    return take(c, name, 1);
}

int oracles_controls_menu_free(const OraclesHomeControls *c, const char *name)
{
    if (listed(name, moving_buttons, sizeof moving_buttons / sizeof moving_buttons[0]) || same_name(name, "guide")) return 0;
    for (int i = 0; i < ORACLES_HOME_PAD_BUTTONS; i++) if (same_name(c->pads[i], name)) return 0;
    for (int i = 0; i < ORACLES_HOME_SLOTS; i++) if (same_name(c->hotkey_pads[i], name)) return 0;
    return 1;
}

OraclesHomeCommand oracles_controls_capture_button(OraclesHomeNav *nav, const char *name)
{
    OraclesHomeControls *c = &nav->controls;
    if (!c->capturing || !name || !name[0]) return ORACLES_HOME_STAY;
    if (c->row == ORACLES_CONTROLS_ROW_MENU) return oracles_controls_menu_free(c, name) ? take(c, name, 0) : ORACLES_HOME_STAY;
    if ((c->column != 1 && c->column != 3) || listed(name, moving_buttons, sizeof moving_buttons / sizeof moving_buttons[0])) return ORACLES_HOME_STAY;
    return take(c, name, 0);
}

void oracles_controls_cancel(OraclesHomeNav *nav) { nav->controls.capturing = 0; }
