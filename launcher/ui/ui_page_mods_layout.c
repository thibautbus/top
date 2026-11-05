/* Mods' layout (ui_page_layout.h), without SDL: its list of the mods in the folder, each with its toggle, its count and
 * its note, and Android's Save row, in 16:9 and in 4:3. */
#include "ui_page_layout_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* The first mod the list shows, `shown` at most: all from the first while they fit, else the highlighted one among
 * the last shown. */
static unsigned first_shown(const OraclesHomeNav *nav, unsigned count, unsigned shown)
{
    if (count <= shown) return 0;
    const unsigned row = nav->row;
    const unsigned focused = row == ORACLES_MODS_ROW_FOLDER ? 0u : row > count ? count - 1u : row - 1u;   /* Save and Play: the last */
    return focused < shown ? 0u : focused - (shown - 1u);
}

/* The Save row's texts: the export's status, or the note. */
static void save_texts(const OraclesHomeNav *nav, int four_three, OraclesUiModsLayout *out)
{
    const char *second;
    out->save_status_text = oracles_page_save_status(nav, &out->save_tone, &second);
    out->save_note_text = oracles_mods_save_note(nav, four_three);
}

/* 16:9's Save row from `y`, as the folder's: its label, Export save with mods… at the right, centred on the note or
 * the status at the left.  Returns its height. */
static float save_16_9(const OraclesHomeNav *nav, float left, float value_x, float value_w, float y, OraclesUiModsLayout *out)
{
    const float inner_x = value_x + ROW_PAD_X, inner_w = value_w - 2.0f * ROW_PAD_X, right = inner_x + inner_w, inner_y = y + ROW_PAD_Y;
    save_texts(nav, 0, out);
    out->label_save = pl_line_at(&oracles_ui_row_label, "Save", left, y + LABEL_PAD_TOP, 0.0f);
    pl_button(oracles_mods_save_action, right, 0.0f, 0.0f, &out->save_button, &out->save_button_dot);
    const float room = inner_w - ROW_GAP - (out->save_button_dot.x + out->save_button_dot.w - out->save_button.x);
    OraclesUiWrapped *const lines = out->save_status_text ? &out->save_status : &out->save_note;
    if (out->save_status_text)
        oracles_ui_wrap_pretty(&oracles_ui_row_status, out->save_status_text, room - DOT - DOT_GAP, inner_x + DOT + DOT_GAP, inner_y, &out->save_status);
    else
        pl_wrap_pretty_lines(&oracles_ui_row_note, out->save_note_text, room, inner_x, inner_y, MODS_SAVE_NOTE_LINE, &out->save_note);
    /* A text shorter than the action centred on it, as the action is on a longer one. */
    const float content = pl_max_f(lines->h, out->save_button.h);
    pl_move_wrapped(lines, inner_y + (content - lines->h) * 0.5f);
    if (out->save_status_text) out->save_dot = pl_box(inner_x, lines->lines[0].y + (lines->lines[0].h - DOT) * 0.5f, DOT, DOT);
    pl_button(oracles_mods_save_action, right, inner_y, content, &out->save_button, &out->save_button_dot);
    out->save_row = pl_box(value_x, y, value_w, content + 2.0f * ROW_PAD_Y);
    return out->save_row.h;
}

static void mods_16_9(const OraclesHomeNav *nav, OraclesUiModsLayout *out)
{
    const OraclesHomeHero hero = oracles_home_hero(nav);
    pl_page_head_16_9("Mods", oracles_home_over(hero), oracles_home_title(hero), out->state, &out->head);
    const float left = PANEL_X + PANEL_BORDER + PANEL_PAD_X, width = PANEL_W - 2.0f * (PANEL_BORDER + PANEL_PAD_X);
    const float value_x = left + LABEL_W + GRID_GAP, value_w = width - LABEL_W - GRID_GAP;
    const float inner_x = value_x + ROW_PAD_X, inner_w = value_w - 2.0f * ROW_PAD_X, right = inner_x + inner_w;
    float y = PANEL_Y + PANEL_BORDER + PANEL_PAD_Y;

    /* Folder: its path and where it is; Open folder at the right. */
    out->label_folder = pl_line_at(&oracles_ui_row_label, "Folder", left, y + LABEL_PAD_TOP, 0.0f);
    float inner_y = y + ROW_PAD_Y;
    pl_button("Open folder", right, 0.0f, 0.0f, &out->folder_button, &out->folder_button_dot);
    const float room = inner_w - ROW_GAP - (out->folder_button_dot.x + out->folder_button_dot.w - out->folder_button.x);
    char folder[sizeof out->folder_text];
    oracles_mods_folder(nav, folder, sizeof folder);
    oracles_ui_fit(&oracles_ui_row_path, folder, room, out->folder_text, sizeof out->folder_text);
    out->folder_path = pl_line_at(&oracles_ui_row_path, out->folder_text, inner_x, inner_y, 0.0f);
    out->folder_line = pl_line_at(&oracles_ui_row_text, oracles_mods_folder_line(nav), inner_x, out->folder_path.y + out->folder_path.h + FILE_GAP, 0.0f);
    const float content = out->folder_line.y + out->folder_line.h - inner_y;
    pl_button("Open folder", right, inner_y, content, &out->folder_button, &out->folder_button_dot);
    out->folder_row = pl_box(value_x, y, value_w, content + 2.0f * ROW_PAD_Y);
    y += out->folder_row.h + GAME_ROW_GAP;

    /* Mods: each mod with its switch, or the line that says where to put them; the count and the note on the save. */
    out->label_mods = pl_line_at(&oracles_ui_row_label, "Mods", left, y + LABEL_PAD_TOP, 0.0f);
    const float list_y = y;
    const OraclesHomeMods *mods = oracles_mods_list(nav);
    if (!mods->count) {
        pl_wrap_lines(&oracles_ui_row_status, oracles_mods_empty, inner_w, inner_x, y + MODS_EMPTY_PAD_Y, MODS_EMPTY_LINE, &out->empty);
        y += out->empty.h + 2.0f * MODS_EMPTY_PAD_Y + MODS_ROW_GAP;
    }
    const unsigned most = ORACLES_UI_MODS_SHOWN - (nav->android != 0), first = first_shown(nav, mods->count, most);
    for (unsigned i = first; i < mods->count && out->shown < most; i++) {
        const OraclesHomeMod *mod = &mods->mods[i];
        OraclesUiModLayout *m = &out->mods[out->shown++];
        m->index = i;
        const float text_x = inner_x + MODS_TOGGLE_COLUMN + GRID_GAP, text_w = right - text_x;
        inner_y = y + ROW_PAD_Y;
        oracles_ui_fit(&oracles_ui_row_title, mod->name, text_w, m->name_text, sizeof m->name_text);
        m->name = pl_line_at(&oracles_ui_row_title, m->name_text, text_x, inner_y, 0.0f);
        char games[64];
        oracles_mods_games(mod, games, sizeof games);
        const float games_x = m->name.x + m->name.w + MODS_NAME_GAP;
        oracles_ui_fit(&oracles_ui_mod_games, games, right - games_x, m->games_text, sizeof m->games_text);
        m->games = pl_line_at(&oracles_ui_mod_games, m->games_text, games_x, 0.0f, 0.0f);
        pl_move_line(&m->games, games_x, m->name.baseline - (m->games.baseline - m->games.y));   /* on the name's baseline */
        const float line_y = m->name.y + m->name.h + FILE_GAP;
        oracles_ui_fit(&oracles_ui_row_text, mod->line, text_w - DOT - DOT_GAP, m->line_text, sizeof m->line_text);
        m->line = pl_line_at(&oracles_ui_row_text, m->line_text, text_x + DOT + DOT_GAP, line_y, 0.0f);
        m->dot = pl_box(text_x, line_y + (m->line.h - DOT) * 0.5f, DOT, DOT);
        const float row_h = m->line.y + m->line.h - inner_y + 2.0f * ROW_PAD_Y;
        m->row = pl_box(value_x, y, value_w, row_h);
        m->toggle = pl_box(inner_x, y + (row_h - MODS_TOGGLE_H) * 0.5f, MODS_TOGGLE_W, MODS_TOGGLE_H);
        m->knob = pl_box(m->toggle.x + MODS_KNOB_INSET + (mod->active ? MODS_KNOB_ON : 0.0f), m->toggle.y + MODS_KNOB_INSET, MODS_KNOB, MODS_KNOB);
        y += row_h + MODS_ROW_GAP;
    }
    oracles_mods_count(nav, out->count_text, sizeof out->count_text);
    out->count = pl_line_at(&oracles_ui_row_text, out->count_text, inner_x, y + MODS_COUNT_PAD_TOP, 0.0f);
    y = out->count.y + out->count.h + MODS_ROW_GAP;
    pl_wrap_lines(&oracles_ui_row_note, oracles_mods_note, inner_w, inner_x, y, MODS_NOTE_LINE, &out->note);
    y += out->note.h;
    out->list = pl_box(value_x, list_y, value_w, y - list_y);
    y += GAME_ROW_GAP;
    if (nav->android) y += save_16_9(nav, left, value_x, value_w, y, out) + GAME_ROW_GAP;

    /* Play, right-aligned across the panel, with why it cannot start when it cannot. */
    const float play_right = left + width - ROW_PAD_X;
    out->play_dot = pl_line_at(&oracles_ui_play, oracles_ui_item_dot, 0.0f, y + PLAY_PAD_Y, 0.0f);
    pl_move_line(&out->play_dot, play_right - out->play_dot.w, out->play_dot.y);
    out->play = pl_line_at(&oracles_ui_play, "Play", 0.0f, y + PLAY_PAD_Y, 0.0f);
    pl_move_line(&out->play, out->play_dot.x - PLAY_GAP - out->play.w, out->play.y);
    out->play_note_text = oracles_mods_play_note(nav);
    out->play_note = pl_line_at(&oracles_ui_row_text, out->play_note_text, 0.0f, 0.0f, 0.0f);
    pl_move_line(&out->play_note, out->play.x - PLAY_GAP - out->play_note.w, out->play.y + (out->play.h - out->play_note.h) * 0.5f);
    out->play_row = pl_box(left, y, width, out->play.h + 2.0f * PLAY_PAD_Y);
    y += out->play_row.h;
    out->panel = pl_box(PANEL_X, PANEL_Y, PANEL_W, y + PANEL_PAD_Y + PANEL_BORDER - PANEL_Y);
}

/* `text` broken as a browser breaks a path with overflow-wrap: anywhere, after a hyphen or a space when the line has
 * one, its lines as wide as fit in `width`, `max_lines` at most: shortened in its middle first when it would take more
 * (a path's end stays). */
static void wrap_anywhere(const OraclesUiTextStyle *style, const char *text, float width, unsigned max_lines, float x, float y, OraclesUiWrapped *out)
{
    char fitted[ORACLES_HOME_TEXT_LENGTH * 4];
    float room = width * (float)max_lines;
    for (int attempt = 0; attempt < 8; attempt++, room -= width * 0.1f) {
        oracles_ui_fit(style, text, room, fitted, sizeof fitted);
        memset(out, 0, sizeof *out);
        const char *p = fitted;
        while (*p && out->count < ORACLES_UI_WRAP_LINES) {
            /* The most whole characters that fit, one at least; then the line up to its last hyphen or space, if any,
             * when the text goes on. */
            size_t length = 0, opportunity = 0;
            for (;;) {
                const char *next = p + length;
                oracles_ui_utf8_next(&next);
                const size_t candidate = (size_t)(next - p);
                if (candidate >= ORACLES_UI_WRAP_LENGTH) break;
                char piece[ORACLES_UI_WRAP_LENGTH];
                memcpy(piece, p, candidate);
                piece[candidate] = 0;
                if (length && oracles_ui_text_width(style, piece) > width) break;
                length = candidate;
                if (p[length - 1] == '-' || p[length - 1] == ' ') opportunity = length;
                if (!p[length]) break;
            }
            if (!length) break;
            if (p[length] && opportunity) length = opportunity;
            memcpy(out->text[out->count], p, length);
            out->text[out->count][length] = 0;
            out->count++;
            p += length;
        }
        if (out->count <= max_lines && !*p) break;
    }
    float height, baseline;
    oracles_ui_line_box(style, 0.0f, &height, &baseline);
    for (unsigned i = 0; i < out->count; i++) {
        out->lines[i] = pl_line_at(style, out->text[i], x, y + (float)i * height, 0.0f);
        out->w = pl_max_f(out->w, out->lines[i].w);
    }
    out->h = (float)out->count * height;
}

/* 4:3's Save row from `y`: its label and Export save with mods… on one baseline, the note on one line under them, or
 * the status beside its dot.  Returns its height. */
static float save_4_3(const OraclesHomeNav *nav, float y, OraclesUiModsLayout *out)
{
    const float span = RIGHT_4_3 - VALUE_X_4_3;
    save_texts(nav, 1, out);
    pl_place_button_4_3(oracles_mods_save_action, &out->save_button, &out->save_button_dot);
    out->label_save = pl_line_at(&oracles_ui_row_label_4_3, "Save", LEFT_4_3, 0.0f, 0.0f);
    OraclesUiLine *const first[3] = { &out->label_save, &out->save_button, &out->save_button_dot };
    float bottom = pl_baseline_row(first, 3, y + ROW_PAD_Y_4_3) + GRID_ROW_GAP_4_3;
    if (out->save_status_text) {
        oracles_ui_wrap_pretty(&oracles_ui_row_text_4_3, out->save_status_text, span - DOT_4_3 - DOT_GAP_4_3, VALUE_X_4_3 + DOT_4_3 + DOT_GAP_4_3, bottom,
                               &out->save_status);
        out->save_dot = pl_box(VALUE_X_4_3, out->save_status.lines[0].baseline - DOT_4_3, DOT_4_3, DOT_4_3);
        bottom += out->save_status.h;
    } else {
        /* The note on one line, shortened where it does not hold, as Cartridge's. */
        out->save_note.count = 1;
        oracles_ui_fit(&oracles_ui_page_note_4_3, out->save_note_text, span, out->save_note.text[0], sizeof out->save_note.text[0]);
        out->save_note.lines[0] = pl_line_at(&oracles_ui_page_note_4_3, out->save_note.text[0], VALUE_X_4_3, bottom, 0.0f);
        out->save_note.w = out->save_note.lines[0].w;
        out->save_note.h = out->save_note.lines[0].h;
        bottom += out->save_note.h;
    }
    out->save_row = pl_row_4_3(y, bottom);
    return out->save_row.h;
}

static void mods_4_3(const OraclesHomeNav *nav, OraclesUiModsLayout *out)
{
    const OraclesHomeHero hero = oracles_home_hero(nav);
    pl_page_head_4_3("Mods", oracles_home_over(hero), oracles_home_title(hero), out->state, &out->head);
    float y = ROWS_Y_4_3;

    /* Folder: its path, broken anywhere on two lines at most, and where it is; Open folder at the right. */
    const float room = pl_file_room_4_3(VALUE_X_4_3, pl_place_button_4_3("Open folder", &out->folder_button, &out->folder_button_dot));
    out->label_folder = pl_line_at(&oracles_ui_row_label_4_3, "Folder", LEFT_4_3, 0.0f, 0.0f);
    char folder[sizeof out->folder_text];
    oracles_mods_folder(nav, folder, sizeof folder);
    wrap_anywhere(&oracles_ui_row_path_4_3, folder, room, MODS_FOLDER_LINES_4_3, VALUE_X_4_3, 0.0f, &out->folder_lines);
    snprintf(out->folder_text, sizeof out->folder_text, "%s", out->folder_lines.count ? out->folder_lines.text[0] : "");
    OraclesUiLine path = pl_line_at(&oracles_ui_row_path_4_3, out->folder_text, VALUE_X_4_3, 0.0f, 0.0f);
    const float path_line = path.h;
    if (out->folder_lines.count > 1) path.h = out->folder_lines.h;   /* the whole path under the first line's baseline */
    OraclesUiLine *const first[4] = { &out->label_folder, &path, &out->folder_button, &out->folder_button_dot };
    const float first_bottom = pl_baseline_row(first, 4, y + ROW_PAD_Y_4_3);
    for (unsigned i = 0; i < out->folder_lines.count; i++) pl_move_line(&out->folder_lines.lines[i], VALUE_X_4_3, path.y + (float)i * path_line);
    out->folder_path = out->folder_lines.count ? out->folder_lines.lines[0] : path;
    out->folder_line = pl_line_at(&oracles_ui_row_text_4_3, oracles_mods_folder_line(nav), VALUE_X_4_3, first_bottom + GRID_ROW_GAP_4_3, 0.0f);
    out->folder_row = pl_row_4_3(y, out->folder_line.y + out->folder_line.h);
    y += out->folder_row.h + ROW_GAP_4_3;

    /* Mods: each mod with its switch, its row reaching left under the label's gap, or the line that says where to put
     * them; the count and the note on the save. */
    const float list_y = y, list_w = RIGHT_4_3 + ROW_PAD_X_4_3 - VALUE_X_4_3;
    out->label_mods = pl_line_at(&oracles_ui_row_label_4_3, "Mods", LEFT_4_3, y + MODS_LABEL_PAD_4_3, 0.0f);
    const OraclesHomeMods *mods = oracles_mods_list(nav);
    if (!mods->count) {
        pl_wrap_lines(&oracles_ui_row_text_4_3, oracles_mods_empty, list_w - ROW_PAD_X_4_3, VALUE_X_4_3, y + MODS_EMPTY_PAD_4_3, MODS_EMPTY_LINE_4_3,
                   &out->empty);
        y += out->empty.h + 2.0f * MODS_EMPTY_PAD_4_3 + MODS_ROW_GAP_4_3;
    }
    const float row_x = VALUE_X_4_3 - ROW_PAD_X_4_3, row_w = list_w + ROW_PAD_X_4_3;
    const float text_x = VALUE_X_4_3 + MODS_TOGGLE_W_4_3 + GRID_GAP_4_3, text_w = RIGHT_4_3 - text_x;
    const unsigned most = ORACLES_UI_MODS_SHOWN_4_3 - (nav->android != 0), first_mod = first_shown(nav, mods->count, most);
    for (unsigned i = first_mod; i < mods->count && out->shown < most; i++) {
        const OraclesHomeMod *mod = &mods->mods[i];
        OraclesUiModLayout *m = &out->mods[out->shown++];
        m->index = i;
        oracles_ui_fit(&oracles_ui_row_title_4_3, mod->name, text_w, m->name_text, sizeof m->name_text);
        m->name = pl_line_at(&oracles_ui_row_title_4_3, m->name_text, text_x, 0.0f, 0.0f);
        char games[64];
        oracles_mods_games(mod, games, sizeof games);
        const float games_x = m->name.x + m->name.w + MODS_NAME_GAP_4_3;
        oracles_ui_fit(&oracles_ui_mod_games_4_3, games, RIGHT_4_3 - games_x, m->games_text, sizeof m->games_text);
        m->games = pl_line_at(&oracles_ui_mod_games_4_3, m->games_text, games_x, 0.0f, 0.0f);
        OraclesUiLine *const name_line[2] = { &m->name, &m->games };
        const float line_y = pl_baseline_row(name_line, 2, y + MODS_ROW_PAD_Y_4_3) + GRID_ROW_GAP_4_3;
        oracles_ui_fit(&oracles_ui_row_text_4_3, mod->line, text_w - DOT_4_3 - DOT_GAP_4_3, m->line_text, sizeof m->line_text);
        m->line = pl_line_at(&oracles_ui_row_text_4_3, m->line_text, text_x + DOT_4_3 + DOT_GAP_4_3, line_y, 0.0f);
        m->dot = pl_box(text_x, line_y + (m->line.h - DOT_4_3) * 0.5f, DOT_4_3, DOT_4_3);
        const float row_h = pl_max_f(ROW_MIN_H_4_3, m->line.y + m->line.h + MODS_ROW_PAD_Y_4_3 - y);
        m->row = pl_box(row_x, y, row_w, row_h);
        m->toggle = pl_box(VALUE_X_4_3, y + (row_h - MODS_TOGGLE_H_4_3) * 0.5f, MODS_TOGGLE_W_4_3, MODS_TOGGLE_H_4_3);
        m->knob = pl_box(m->toggle.x + MODS_KNOB_INSET_4_3 + (mod->active && !mod->refused ? MODS_KNOB_ON_4_3 : 0.0f), m->toggle.y + MODS_KNOB_INSET_4_3,
                      MODS_KNOB_4_3, MODS_KNOB_4_3);
        y += row_h + MODS_ROW_GAP_4_3;
    }
    oracles_mods_count(nav, out->count_text, sizeof out->count_text);
    out->count = pl_line_at(&oracles_ui_row_text_4_3, out->count_text, VALUE_X_4_3, y + MODS_COUNT_PAD_4_3, 0.0f);
    y = out->count.y + out->count.h + MODS_ROW_GAP_4_3;
    pl_wrap_lines(&oracles_ui_page_note_4_3, oracles_mods_note, list_w, VALUE_X_4_3, y, MODS_NOTE_LINE_4_3, &out->note);
    y += out->note.h;
    out->list = pl_box(VALUE_X_4_3, list_y, list_w, y - list_y);
    y += ROW_GAP_4_3;
    if (nav->android) y += save_4_3(nav, y, out) + ROW_GAP_4_3;

    out->play_note_text = oracles_mods_play_note(nav);
    out->play_row = pl_play_4_3_row(out->play_note_text, y, &out->play_note, &out->play, &out->play_dot);
}

void oracles_ui_layout_mods(OraclesUiLayout layout, const OraclesHomeNav *nav, OraclesUiModsLayout *out)
{
    memset(out, 0, sizeof *out);
    oracles_home_state(nav, oracles_home_hero(nav), out->state);
    if (layout == ORACLES_UI_LAYOUT_4_3) mods_4_3(nav, out);
    else mods_16_9(nav, out);
}
