/* Cheats and About (ui_page_nav.h): their rows, their keys and pointer, and their layout (ui_page_layout.h). */
#include "ui_page_layout.h"
#include "ui_page_nav.h"

#include <stdio.h>
#include <string.h>

const char *const oracles_cheats_labels[ORACLES_CHEATS_ROWS] = { "No damage", "Ammo never runs out", "Rupees never run out" };
const char *const oracles_cheats_texts[ORACLES_CHEATS_ROWS] = {
    "Enemies, spikes and falls no longer cost hearts.",
    "Seeds, bombs and Bombchus are not used up.",
    "Buying costs nothing.",
};
const char *const oracles_cheats_notes[ORACLES_CHEATS_ROWS] = { "Link is still knocked back.", "", "Mods still charge." };
const char oracles_cheats_note[] = "Cheats play with your usual save: what you keep stays in it.";

const char *oracles_cheats_row_note(const OraclesHomeNav *nav, unsigned row)
{
    if (row >= ORACLES_CHEATS_ROWS) return "";
    return row == 2u && oracles_home_fan_game(oracles_home_game(nav)) ? "" : oracles_cheats_notes[row];
}

void oracles_cheats_explanation(const OraclesHomeNav *nav, unsigned row, char *out, size_t capacity)
{
    if (row >= ORACLES_CHEATS_ROWS) { if (capacity) out[0] = 0; return; }
    const char *note = oracles_cheats_row_note(nav, row);
    snprintf(out, capacity, "%s%s%s", oracles_cheats_texts[row], note[0] ? " " : "", note);
}

/* The page's game: the one the menu or the pause showed, whose profile has cheats (a fan game's too). */
static int cheats_game(const OraclesHomeNav *nav)
{
    const int g = oracles_home_game(nav);
    return g >= 0 ? g : ORACLES_HOME_GAME_AGES;
}

int oracles_cheats_row_live(const OraclesHomeNav *nav, unsigned row) { return row < ORACLES_CHEATS_ROWS && !nav->cheats_fixed; }

int oracles_cheats_on(const OraclesHomeNav *nav, unsigned row)
{
    return row < ORACLES_CHEATS_ROWS && (nav->games[cheats_game(nav)].cheats & 1u << row);
}

static OraclesHomeCommand set_cheat(OraclesHomeNav *nav, unsigned row, int on)
{
    if (!oracles_cheats_row_live(nav, row) || oracles_cheats_on(nav, row) == on) return ORACLES_HOME_STAY;
    const int g = cheats_game(nav);
    nav->games[g].cheats ^= 1u << row;
    nav->cheats_changed |= 1u << g;
    return ORACLES_HOME_STORE;
}

OraclesHomeCommand oracles_cheats_act(OraclesHomeNav *nav, OraclesHomeAction action)
{
    if (nav->row >= ORACLES_CHEATS_ROWS) nav->row = 0;
    const unsigned row = nav->row;
    switch (action) {
        case ORACLES_HOME_UP: nav->row = (row + ORACLES_CHEATS_ROWS - 1u) % ORACLES_CHEATS_ROWS; return ORACLES_HOME_STAY;
        case ORACLES_HOME_DOWN: nav->row = (row + 1u) % ORACLES_CHEATS_ROWS; return ORACLES_HOME_STAY;
        case ORACLES_HOME_LEFT:
        case ORACLES_HOME_RIGHT:
        case ORACLES_HOME_OK: return set_cheat(nav, row, !oracles_cheats_on(nav, row));
        case ORACLES_HOME_BACK:
            /* Back to the menu Cheats was opened from, on Cheats. */
            nav->screen = nav->in_game ? ORACLES_SCREEN_PAUSE : ORACLES_SCREEN_HOME;
            oracles_home_focus_item(nav, ORACLES_HOME_ITEM_CHEATS);
            return ORACLES_HOME_STAY;
    }
    return ORACLES_HOME_STAY;
}

OraclesHomeCommand oracles_cheats_click(OraclesHomeNav *nav, unsigned row, int option)
{
    if (row >= ORACLES_CHEATS_ROWS) return ORACLES_HOME_STAY;
    nav->row = row;
    return option < 0 ? ORACLES_HOME_STAY : set_cheat(nav, row, option == 1);
}

const char oracles_about_label[] = "Update check";
const char *const oracles_about_choices[2] = { "On", "Off" };
const char oracles_about_explanation[] = "On: the launcher asks GitHub once at launch whether a newer version is out. Off: no network request.";
const char oracles_about_help[] = "On: one request to GitHub at launch.";
const char oracles_about_help_note[] = "Off: no network request.";
const char oracles_about_portable_label[] = "Portable mode";
const char *const oracles_about_portable_choices[2] = { "Off", "On" };
const char oracles_about_portable_explanation[] = "Settings and mods stay beside the launcher, for a USB stick.";
const char oracles_about_portable_help[] = "On: settings and mods beside the launcher, for a USB stick.";
const char oracles_about_portable_help_note[] = "Off: in your user folder. ROMs and saves never move.";
const char oracles_about_portable_hint[] = "Your games and their saves stay where they are: move them into the game folder to take them along.";

void oracles_about_note(const OraclesHomeNav *nav, const char *version, char *out, size_t capacity)
{
    if (nav->update[0] && nav->update_check) snprintf(out, capacity, "%s \xc2\xb7 %s available", version, nav->update);
    else snprintf(out, capacity, "%s", version);
}

unsigned oracles_about_rows(const OraclesHomeNav *nav) { return nav->portable_shown ? ORACLES_ABOUT_ROWS : 1u; }

const char *oracles_about_portable_hint_shown(const OraclesHomeNav *nav)
{
    return nav->portable_shown && nav->portable && nav->games_outside ? oracles_about_portable_hint : NULL;
}

const char *oracles_about_portable_status(const OraclesHomeNav *nav, OraclesTone *tone)
{
    const int on = nav->portable != 0;
    *tone = ORACLES_TONE_OK;
    if (!nav->portable_shown) return NULL;
    switch ((OraclesHomePortableStatus)nav->portable_status) {
        case ORACLES_PORTABLE_STATUS_COPYING: *tone = ORACLES_TONE_NONE; return "Copying\xe2\x80\xa6";
        case ORACLES_PORTABLE_STATUS_COPIED:
            return on ? "Settings and mods copied to the game folder." : "Settings and mods copied back to your user folder.";
        case ORACLES_PORTABLE_STATUS_MOVED: return on ? "Settings and mods now stay in the game folder." : "Settings and mods now stay in your user folder.";
        case ORACLES_PORTABLE_STATUS_KEPT:
            *tone = ORACLES_TONE_WARN;
            return on ? "The game folder already has its own settings: they are used from the next launch. Yours stay in your user folder."
                      : "Your user folder still has the settings from before the game folder: they come back from the next launch. Those of the game "
                        "folder stay in it.";
        case ORACLES_PORTABLE_STATUS_READ_ONLY:
            *tone = ORACLES_TONE_ERROR;
            return nav->windows ? "The launcher's folder cannot be written in (such as Program Files): nothing changed. Move the launcher to a folder of yours."
                                : "The launcher's folder cannot be written in (such as /opt): nothing changed. Move the launcher to a folder of yours.";
        case ORACLES_PORTABLE_STATUS_FAILED: *tone = ORACLES_TONE_ERROR; return "The settings could not be copied: nothing changed.";
        case ORACLES_PORTABLE_STATUS_NONE: break;
    }
    return NULL;
}

static OraclesHomeCommand set_update_check(OraclesHomeNav *nav, int on)
{
    if (nav->update_check == on) return ORACLES_HOME_STAY;
    nav->update_check = on;
    return ORACLES_HOME_STORE;
}

/* The switch turns while its data is copied, which the host does (ORACLES_HOME_PORTABLE), and says how it went. */
static OraclesHomeCommand set_portable(OraclesHomeNav *nav, int on)
{
    if (!nav->portable_shown || nav->portable == on) return ORACLES_HOME_STAY;
    nav->portable_wanted = on;
    nav->portable_status = ORACLES_PORTABLE_STATUS_COPYING;
    return ORACLES_HOME_PORTABLE;
}

static int portable_copying(const OraclesHomeNav *nav) { return nav->portable_status == ORACLES_PORTABLE_STATUS_COPYING; }

OraclesHomeCommand oracles_about_act(OraclesHomeNav *nav, OraclesHomeAction action)
{
    const unsigned rows = oracles_about_rows(nav);
    if (nav->row >= rows) nav->row = 0;
    if (portable_copying(nav)) return ORACLES_HOME_STAY;   /* until the copy ends, Back included */
    const int portable = nav->row == ORACLES_ABOUT_ROW_PORTABLE;
    switch (action) {
        case ORACLES_HOME_UP: nav->row = (nav->row + rows - 1u) % rows; return ORACLES_HOME_STAY;
        case ORACLES_HOME_DOWN: nav->row = (nav->row + 1u) % rows; return ORACLES_HOME_STAY;
        case ORACLES_HOME_LEFT:
        case ORACLES_HOME_RIGHT:
        case ORACLES_HOME_OK: return portable ? set_portable(nav, !nav->portable) : set_update_check(nav, !nav->update_check);
        case ORACLES_HOME_BACK: {
            /* Back to the home screen, on About's link; the switch's status goes with the page. */
            OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
            nav->screen = ORACLES_SCREEN_HOME;
            nav->focus = oracles_home_items(nav, items) + (oracles_home_update_shown(nav) ? 1u : 0u);
            nav->portable_status = ORACLES_PORTABLE_STATUS_NONE;
            return ORACLES_HOME_STAY;
        }
    }
    return ORACLES_HOME_STAY;
}

OraclesHomeCommand oracles_about_click(OraclesHomeNav *nav, unsigned row, int option)
{
    if (portable_copying(nav) || row >= oracles_about_rows(nav)) return ORACLES_HOME_STAY;
    nav->row = row;
    if (option < 0) return ORACLES_HOME_STAY;
    return row == ORACLES_ABOUT_ROW_PORTABLE ? set_portable(nav, option == 1) : set_update_check(nav, option == 0);
}

/* ---- the layout ------------------------------------------------------------------------------------------------ */

/* 16:9: the head's column from (96, 88), 520 wide, the note 28 under the title in lines of 32; the panel from (656, 56)
 * across 1208, a grid of 360 for the labels and the values, each row's value padded 14 by 20, its explanation beside
 * the choices in lines of 29, two kept. */
#define HEAD_X 96.0f
#define HEAD_W 520.0f
#define NOTE_GAP 28.0f
#define NOTE_LINE 32.0f
#define PANEL_X 656.0f
#define PANEL_Y 56.0f
#define PANEL_W 1208.0f
#define PANEL_PAD_X 32.0f
#define PANEL_PAD_Y 24.0f
#define PANEL_BORDER 1.0f
#define ROW_GAP 14.0f
#define LABEL_W 360.0f
#define GRID_GAP 20.0f
#define LABEL_PAD_TOP 18.0f
#define ROW_PAD_X 20.0f
#define ROW_PAD_Y 14.0f
#define TEXT_GAP 24.0f            /* between the explanation and the choices */
#define TEXT_LINE 29.0f
#define TEXT_H 58.0f
#define CHOICE_GAP 8.0f
#define CHOICE_PAD_X 16.0f
#define CHOICE_PAD_Y 4.0f
#define BORDER 1.0f
/* 4:3: the head from (64, 28), the note under the title; the rows from (64, 208) across 1312, 100 tall at least, padded
 * 5 by 20, a grid of 600 for the labels and the choices, 24 apart; the choices framed, 110x90 at least, 12 apart; the
 * highlighted row's explanation and note under the rows. */
#define HEAD_X_4_3 64.0f
#define HEAD_GAP_4_3 4.0f
#define ROWS_X_4_3 64.0f
#define ROWS_Y_4_3 208.0f
#define ROWS_W_4_3 1312.0f
#define ROW_GAP_4_3 6.0f
#define ROW_MIN_H_4_3 100.0f
#define ROW_PAD_X_4_3 20.0f
#define ROW_PAD_Y_4_3 5.0f
#define LABEL_W_4_3 600.0f
#define GRID_GAP_4_3 24.0f
#define OPTION_MIN_W_4_3 110.0f
#define OPTION_MIN_H_4_3 90.0f
#define OPTION_PAD_X_4_3 22.0f
#define OPTION_PAD_Y_4_3 4.0f
#define OPTION_BORDER_4_3 2.0f
#define OPTION_GAP_4_3 12.0f
#define OPTION_NAME_LINE_4_3 (36.0f * 1.15f)
#define HELP_MARGIN_4_3 14.0f
#define HELP_LINE_4_3 47.0f
#define HELP_NOTE_LINE_4_3 40.0f
#define HELP_NOTE_GAP_4_3 4.0f
/* About's Portable mode: under the row, across it, its hint and its status, the status beside its dot on its first
 * line's baseline; 16:9's 14 under the row (the grid's 20, less 6), in lines of 29 and 31, 4:3's 6 under it in lines of
 * 38. */
#define EXTRA_GAP 14.0f
#define HINT_LINE 29.0f
#define STATUS_LINE 31.0f
#define STATUS_DOT 12.0f
#define STATUS_DOT_GAP 12.0f
#define EXTRA_LINE_4_3 38.0f
#define STATUS_DOT_4_3 14.0f

const OraclesUiTextStyle oracles_ui_about_status = { ORACLES_UI_FONT_SANS, 25.0f, 0.0f, 0 };
const OraclesUiTextStyle oracles_ui_about_status_4_3 = { ORACLES_UI_FONT_SANS, 30.0f, 0.0f, 0 };

static const char *const choice_names[2] = { "Off", "On" };

static float max_f(float a, float b) { return a > b ? a : b; }

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

/* `text` broken as text-wrap: pretty breaks it, in lines of `line_height` from (x, y). */
static void wrap_lines(const OraclesUiTextStyle *style, const char *text, float width, float x, float y, float line_height, OraclesUiWrapped *out)
{
    oracles_ui_wrap_pretty(style, text, width, x, y, out);
    for (unsigned i = 0; i < out->count; i++) out->lines[i] = line_at(style, out->text[i], x, y + (float)i * line_height, line_height);
    out->h = (float)out->count * line_height;
}

/* The two choices `names`, right-aligned at `right` from `top`. */
static void choices_16_9(const char *const *names, float right, float top, OraclesUiOptionLayout out[2])
{
    float x = right;
    for (int i = 1; i >= 0; i--) {
        OraclesUiOptionLayout *o = &out[i];
        memset(&o->size, 0, sizeof o->size);
        o->name = line_at(&oracles_ui_choice, names[i], 0.0f, 0.0f, 0.0f);
        o->box = box(x - o->name.w - 2.0f * (CHOICE_PAD_X + BORDER), top, o->name.w + 2.0f * (CHOICE_PAD_X + BORDER), o->name.h + 2.0f * (CHOICE_PAD_Y + BORDER));
        move_line(&o->name, o->box.x + BORDER + CHOICE_PAD_X, top + BORDER + CHOICE_PAD_Y);
        x = o->box.x - CHOICE_GAP;
    }
}

/* A page of rows laid out as Cheats: its head, the note under it, and each row's label, explanation (16:9's) and two
 * choices; 4:3's help, the highlighted row's explanation and note; under the last row its hint and its status, NULL for
 * none. */
typedef struct list_page {
    const char *section, *over, *title, *note;
    unsigned rows;
    const char *labels[ORACLES_CHEATS_ROWS];
    char explanations[ORACLES_CHEATS_ROWS][ORACLES_HOME_TEXT_LENGTH];
    const char *const *choices[ORACLES_CHEATS_ROWS];
    const char *help, *help_note;
    const char *hint, *status;
    int centred;   /* an explanation shorter than its two lines centred in them (About's), else at their top */
} list_page;

/* The last row's hint and status from `y`, across `width` from `x`, each `gap` under what comes before; returns their
 * bottom. */
static float row_extras(const list_page *page, OraclesUiCheatsLayout *out, const OraclesUiTextStyle *hint_style, const OraclesUiTextStyle *status_style,
                        float hint_line, float status_line, float dot, float x, float width, float y, float gap)
{
    if (page->hint) {
        wrap_lines(hint_style, page->hint, width, x, y + gap, hint_line, &out->hint);
        y = out->hint.lines[0].y + out->hint.h;
    }
    if (page->status) {
        const float text_x = x + dot + STATUS_DOT_GAP;
        wrap_lines(status_style, page->status, width - dot - STATUS_DOT_GAP, text_x, y + gap, status_line, &out->status);
        out->status_dot = box(x, out->status.lines[0].baseline - dot, dot, dot);
        y = out->status.lines[0].y + out->status.h;
    }
    return y;
}

static void list_16_9(const list_page *page, OraclesUiCheatsLayout *out)
{
    oracles_ui_layout_page_head(ORACLES_UI_LAYOUT_16_9, page->section, page->over, page->title, NULL, &out->head);
    const float note_y = out->head.title.lines[0].y + out->head.title.h + NOTE_GAP;
    wrap_lines(&oracles_ui_row_text, page->note, HEAD_W, HEAD_X, note_y, NOTE_LINE, &out->note);

    const float left = PANEL_X + PANEL_BORDER + PANEL_PAD_X, width = PANEL_W - 2.0f * (PANEL_BORDER + PANEL_PAD_X);
    const float value_x = left + LABEL_W + GRID_GAP, value_w = width - LABEL_W - GRID_GAP;
    const float inner_x = value_x + ROW_PAD_X, right = value_x + value_w - ROW_PAD_X;
    float y = PANEL_Y + PANEL_BORDER + PANEL_PAD_Y;
    for (unsigned r = 0; r < page->rows; r++) {
        out->labels[r] = line_at(&oracles_ui_row_label, page->labels[r], left, y + LABEL_PAD_TOP, 0.0f);
        /* The explanation beside the two choices, two lines kept, the two centred on the row. */
        OraclesUiOptionLayout *o = out->choices[r];
        choices_16_9(page->choices[r], right, 0.0f, o);
        wrap_lines(&oracles_ui_row_text, page->explanations[r], o[0].box.x - TEXT_GAP - inner_x, inner_x, 0.0f, TEXT_LINE, &out->explanations[r]);
        const float text_h = max_f(out->explanations[r].h, TEXT_H), inner_h = max_f(text_h, o[0].box.h);
        const float inner_y = y + ROW_PAD_Y;
        const float top = inner_y + (inner_h - text_h) * 0.5f + (page->centred ? (text_h - out->explanations[r].h) * 0.5f : 0.0f);
        for (unsigned i = 0; i < out->explanations[r].count; i++) move_line(&out->explanations[r].lines[i], inner_x, top + (float)i * TEXT_LINE);
        for (int c = 0; c < 2; c++) {
            const float dy = inner_y + (inner_h - o[c].box.h) * 0.5f;
            o[c].box.y += dy;
            move_line(&o[c].name, o[c].name.x, o[c].name.y + dy);
        }
        out->rows[r] = box(value_x, y, value_w, inner_h + 2.0f * ROW_PAD_Y);
        y += max_f(LABEL_PAD_TOP + out->labels[r].h, out->rows[r].h);
        if (r + 1u == page->rows)
            y = row_extras(page, out, &oracles_ui_row_text, &oracles_ui_about_status, HINT_LINE, STATUS_LINE, STATUS_DOT, left + ROW_PAD_X,
                           width - 2.0f * ROW_PAD_X, y, EXTRA_GAP);
        else y += ROW_GAP;
    }
    out->panel = box(PANEL_X, PANEL_Y, PANEL_W, y + PANEL_PAD_Y + PANEL_BORDER - PANEL_Y);
}

static void list_4_3(const list_page *page, OraclesUiCheatsLayout *out)
{
    oracles_ui_layout_page_head(ORACLES_UI_LAYOUT_4_3, page->section, page->over, page->title, NULL, &out->head);
    out->note.count = 1;
    snprintf(out->note.text[0], sizeof out->note.text[0], "%s", page->note);
    out->note.lines[0] = line_at(&oracles_ui_page_note_4_3, page->note, HEAD_X_4_3, out->head.title.lines[0].y + out->head.title.h + HEAD_GAP_4_3, 0.0f);
    out->note.w = out->note.lines[0].w;
    out->note.h = out->note.lines[0].h;

    const float left = ROWS_X_4_3 + ROW_PAD_X_4_3, value_x = left + LABEL_W_4_3 + GRID_GAP_4_3;
    float y = ROWS_Y_4_3;
    for (unsigned r = 0; r < page->rows; r++) {
        /* The two choices framed from the values' column, the label and they centred on the row. */
        OraclesUiOptionLayout *o = out->choices[r];
        float x = value_x, options_h = 0.0f;
        for (int c = 0; c < 2; c++) {
            memset(&o[c].size, 0, sizeof o[c].size);
            o[c].name = line_at(&oracles_ui_row_button_4_3, page->choices[r][c], 0.0f, 0.0f, OPTION_NAME_LINE_4_3);
            o[c].box = box(x, 0.0f, max_f(OPTION_MIN_W_4_3, o[c].name.w + 2.0f * (OPTION_PAD_X_4_3 + OPTION_BORDER_4_3)),
                           max_f(OPTION_MIN_H_4_3, o[c].name.h + 2.0f * (OPTION_PAD_Y_4_3 + OPTION_BORDER_4_3)));
            x += o[c].box.w + OPTION_GAP_4_3;
            options_h = max_f(options_h, o[c].box.h);
        }
        out->labels[r] = line_at(&oracles_ui_row_label_4_3, page->labels[r], left, 0.0f, 0.0f);
        const float track = max_f(ROW_MIN_H_4_3 - 2.0f * ROW_PAD_Y_4_3, max_f(out->labels[r].h, options_h));
        out->rows[r] = box(ROWS_X_4_3, y, ROWS_W_4_3, track + 2.0f * ROW_PAD_Y_4_3);
        move_line(&out->labels[r], left, y + ROW_PAD_Y_4_3 + (track - out->labels[r].h) * 0.5f);
        for (int c = 0; c < 2; c++) {
            o[c].box.y = y + ROW_PAD_Y_4_3 + (track - o[c].box.h) * 0.5f;
            move_line(&o[c].name, o[c].box.x + (o[c].box.w - o[c].name.w) * 0.5f, o[c].box.y + (o[c].box.h - o[c].name.h) * 0.5f);
        }
        y += out->rows[r].h + ROW_GAP_4_3;
    }
    if (page->hint || page->status)
        y = row_extras(page, out, &oracles_ui_row_text_4_3, &oracles_ui_about_status_4_3, EXTRA_LINE_4_3, EXTRA_LINE_4_3, STATUS_DOT_4_3, left,
                       ROWS_W_4_3 - 2.0f * ROW_PAD_X_4_3, y - ROW_GAP_4_3, ROW_GAP_4_3) + ROW_GAP_4_3;
    /* Under the rows, the highlighted row's explanation on one line, and its note on the next, kept when empty. */
    out->help = line_at(&oracles_ui_help_4_3, page->help, left, y + HELP_MARGIN_4_3, HELP_LINE_4_3);
    out->help_note = line_at(&oracles_ui_row_note_4_3, page->help_note, left, out->help.y + out->help.h + HELP_NOTE_GAP_4_3, HELP_NOTE_LINE_4_3);
}

static void list_layout(OraclesUiLayout layout, const list_page *page, OraclesUiCheatsLayout *out)
{
    memset(out, 0, sizeof *out);
    if (layout == ORACLES_UI_LAYOUT_4_3) list_4_3(page, out);
    else list_16_9(page, out);
}

void oracles_ui_layout_cheats(OraclesUiLayout layout, const OraclesHomeNav *nav, OraclesUiCheatsLayout *out)
{
    const OraclesHomeHero hero = oracles_home_hero(nav);
    const unsigned row = nav->row < ORACLES_CHEATS_ROWS ? nav->row : 0u;
    list_page page = { "Cheats", oracles_home_over(hero), oracles_home_title(hero), oracles_cheats_note, ORACLES_CHEATS_ROWS, { NULL }, { "" },
                       { NULL }, oracles_cheats_texts[row], oracles_cheats_row_note(nav, row), NULL, NULL, 0 };
    for (unsigned r = 0; r < ORACLES_CHEATS_ROWS; r++) {
        page.labels[r] = oracles_cheats_labels[r];
        page.choices[r] = choice_names;
        oracles_cheats_explanation(nav, r, page.explanations[r], sizeof page.explanations[r]);
    }
    list_layout(layout, &page, out);
}

void oracles_ui_layout_about(OraclesUiLayout layout, const OraclesHomeNav *nav, const char *version, char *note, size_t capacity, OraclesUiCheatsLayout *out)
{
    /* 16:9: "The Oracles" over the title "Project"; 4:3: the whole name on the title's line. */
    oracles_about_note(nav, version, note, capacity);
    const int four_three = layout == ORACLES_UI_LAYOUT_4_3, portable = nav->row == ORACLES_ABOUT_ROW_PORTABLE && nav->portable_shown;
    OraclesTone tone;
    list_page page = { "About", four_three ? NULL : "The Oracles", four_three ? "The Oracles Project" : "Project", note, oracles_about_rows(nav),
                       { oracles_about_label, oracles_about_portable_label }, { "" }, { oracles_about_choices, oracles_about_portable_choices },
                       portable ? oracles_about_portable_help : oracles_about_help, portable ? oracles_about_portable_help_note : oracles_about_help_note,
                       oracles_about_portable_hint_shown(nav), oracles_about_portable_status(nav, &tone), 1 };
    snprintf(page.explanations[0], sizeof page.explanations[0], "%s", oracles_about_explanation);
    snprintf(page.explanations[1], sizeof page.explanations[1], "%s", oracles_about_portable_explanation);
    list_layout(layout, &page, out);
}
