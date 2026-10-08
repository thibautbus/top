#include "ui_page.h"

#include "ui_page_layout.h"

#include <stdio.h>
#include <string.h>

/* The pages' colours; the status tones converted once from oklch. */
#define PANEL 0x0a090du
#define PANEL_OPACITY 0.92f
#define PANEL_BORDER 0xffffffu
#define PANEL_BORDER_OPACITY 0.07f
#define ROW_HIGHLIGHT_OPACITY 0.055f
#define OVER 0xd6d3dcu
#define TITLE 0xf3f1f5u
#define STATE 0xa19eaau
#define LABEL 0xa19eaau
#define TEXT 0xefedf2u
#define TEXT_NONE 0x8a8793u
#define PATH 0x8f8c98u
#define NOTE 0xa19eaau
#define NOTE_ITALIC 0x8f8c98u
#define BUTTON 0xdcd9e1u
#define BUTTON_DISABLED 0x58555fu
#define BUTTON_DISABLED_FOCUSED 0x77747fu
#define OPTION_OFF 0xa19eaau
#define CHOICE_OFF 0x8a8793u
#define FRAME_ON_OPACITY 0.4f
#define FRAME_OFF_OPACITY 0.1f
#define OPTION_NOT_ALLOWED 0.38f
#define ROW_NOT_APPLIED 0.4f
#define DIAGRAM 0x0a090du
#define DIAGRAM_OPACITY 0.8f
#define DIAGRAM_BORDER_OPACITY 0.3f
#define DIAGRAM_WINDOW_OPACITY 0.07f
#define DIAGRAM_LABEL 0xa19eaau

#define MODS_PATH 0xc9c6cfu
#define MODS_EMPTY 0xd6d3dcu
#define MODS_SWITCH_OFF 0x26252cu
#define MODS_SWITCH_OFF_BORDER_OPACITY 0.16f
#define MODS_KNOB_ON 0x0c0b0fu
#define MODS_KNOB_OFF 0x8a8793u
#define MODS_REFUSED 0.4f

static const uint32_t tone_dots[4] = { 0x76cf8au, 0xebbd57u, 0xf97770u, 0x6f6c78u };
static const uint32_t tone_texts[4] = { 0xd6d3dcu, 0xecd198u, 0xf9aea7u, 0xa19eaau };

/* The tones of the page's status lines: the ROM's, and a fan game's patch's and image's. */
typedef struct page_tones { OraclesTone rom, patch, image; } page_tones;

static void game_texts(const OraclesHomeNav *nav, OraclesUiGameTexts *t, char *state, page_tones *tones)
{
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    const OraclesHomeHero hero = oracles_home_hero(nav);
    memset(t, 0, sizeof *t);
    oracles_home_state(nav, hero, state);
    t->over = oracles_home_over(hero);
    t->title = oracles_home_title(hero);
    t->state = state;
    snprintf(t->rom_file, sizeof t->rom_file, "%s", oracles_page_rom_file(game));
    snprintf(t->rom_folder, sizeof t->rom_folder, "%s", game->rom_file[0] ? game->rom_folder : "");
    oracles_page_rom_status(game, t->rom_status, sizeof t->rom_status, &tones->rom);
    t->patched = game->fan != NULL;
    if (game->fan) {
        snprintf(t->patch_file, sizeof t->patch_file, "%s", oracles_page_patch_file(game));
        snprintf(t->patch_folder, sizeof t->patch_folder, "%s", game->patch_file[0] ? game->patch_folder : "");
        oracles_page_patch_status(game, t->patch_status, sizeof t->patch_status, &tones->patch);
        oracles_page_image_status(game, t->image_status, sizeof t->image_status, &tones->image);
    }
    snprintf(t->save_file, sizeof t->save_file, "%s", oracles_page_save_file(game));
    oracles_page_save_line(game, t->save_line, sizeof t->save_line);
    t->rom_note = oracles_page_rom_note(nav);
    t->rom_hotkeys_note = oracles_page_rom_hotkeys_note(nav);
    t->play_note = oracles_page_play_note(nav);
}

static void text(OraclesUiDraw *draw, const OraclesUiTextStyle *style, const OraclesUiLine *line, const char *s, OraclesUiColor color)
{
    if (s && s[0]) oracles_ui_draw_text(draw, style, line->x, line->baseline, s, color);
}

static void wrapped(OraclesUiDraw *draw, const OraclesUiTextStyle *style, const OraclesUiWrapped *w, OraclesUiColor color)
{
    for (unsigned i = 0; i < w->count; i++) text(draw, style, &w->lines[i], w->text[i], color);
}

static void dot(OraclesUiDraw *draw, const OraclesUiBox *b, OraclesTone tone)
{
    oracles_ui_fill_round_rect(draw, b->x, b->y, b->w, b->h, 6.0f, oracles_ui_rgb(tone_dots[tone]));
}

/* A framed option: its frame, its name, its size when it has one. */
static void option(OraclesUiDraw *draw, const OraclesUiOptionLayout *o, const OraclesUiTextStyle *style, const char *name, const char *size,
                   int on, int focused, OraclesUiColor accent, uint32_t off, float opacity)
{
    const OraclesUiColor frame = on ? (focused ? accent : oracles_ui_rgba(0xffffffu, FRAME_ON_OPACITY)) : oracles_ui_rgba(0xffffffu, FRAME_OFF_OPACITY);
    const OraclesUiColor color = on ? (focused ? accent : oracles_ui_rgb(TEXT)) : oracles_ui_rgb(off);
    /* A profile's and a window's frame is rounder than a choice's, View's with its size included. */
    oracles_ui_stroke_round_rect(draw, o->box.x, o->box.y, o->box.w, o->box.h, style == &oracles_ui_option_name ? 6.0f : 5.0f, 1.0f,
                                 oracles_ui_fade(frame, opacity));
    text(draw, style, &o->name, name, oracles_ui_fade(color, opacity));
    if (size) text(draw, &oracles_ui_option_size, &o->size, size, oracles_ui_rgba(PATH, opacity));
}

static void highlight_dimmed(OraclesUiDraw *draw, const OraclesUiBox *b, int on, float opacity)
{
    if (on) oracles_ui_fill_round_rect(draw, b->x, b->y, b->w, b->h, 6.0f, oracles_ui_rgba(0xffffffu, ROW_HIGHLIGHT_OPACITY * opacity));
}

static void highlight(OraclesUiDraw *draw, const OraclesUiBox *b, int on) { highlight_dimmed(draw, b, on, 1.0f); }

void oracles_ui_page_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent)
{
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    OraclesUiGameTexts t;
    char state[ORACLES_HOME_STATE_LENGTH];
    page_tones tones;
    game_texts(nav, &t, state, &tones);
    OraclesUiGameLayout l;
    oracles_ui_layout_game(&t, &l);
    const unsigned row = nav->row;

    text(draw, &oracles_ui_page_section, &l.head.section, "Cartridge", accent);
    text(draw, &oracles_ui_page_over, &l.head.over, t.over, oracles_ui_rgb(OVER));
    wrapped(draw, &oracles_ui_page_title, &l.head.title, oracles_ui_rgb(TITLE));
    text(draw, &oracles_ui_page_state, &l.head.state, t.state, oracles_ui_rgb(STATE));

    oracles_ui_fill_round_rect(draw, l.panel.x, l.panel.y, l.panel.w, l.panel.h, 10.0f, oracles_ui_rgba(PANEL, PANEL_OPACITY));
    oracles_ui_stroke_round_rect(draw, l.panel.x, l.panel.y, l.panel.w, l.panel.h, 10.0f, 1.0f, oracles_ui_rgba(PANEL_BORDER, PANEL_BORDER_OPACITY));
    for (unsigned r = 0; r < ORACLES_GAME_ROWS; r++) highlight(draw, &l.rows[r], r == row);
    text(draw, &oracles_ui_row_label, &l.label_rom, t.patched ? "Base ROM" : "ROM", oracles_ui_rgb(LABEL));
    text(draw, &oracles_ui_row_label, &l.label_save, "Save", oracles_ui_rgb(LABEL));

    const OraclesUiColor normal = oracles_ui_rgb(BUTTON);
    const OraclesUiColor off = oracles_ui_rgb(row == ORACLES_ROW_SAVE ? BUTTON_DISABLED_FOCUSED : BUTTON_DISABLED);
    text(draw, &oracles_ui_row_title, &l.rom_file, t.rom_file, oracles_ui_rgb(game->rom_file[0] ? TEXT : TEXT_NONE));
    text(draw, &oracles_ui_row_path, &l.rom_folder, t.rom_folder, oracles_ui_rgb(PATH));
    dot(draw, &l.rom_dot, tones.rom);
    text(draw, &oracles_ui_row_status, &l.rom_status, t.rom_status, oracles_ui_rgb(tone_texts[tones.rom]));
    text(draw, &oracles_ui_row_text, &l.rom_note, t.rom_note, oracles_ui_rgb(NOTE));
    text(draw, &oracles_ui_row_text, &l.rom_hotkeys_note, t.rom_hotkeys_note, oracles_ui_rgb(NOTE));
    const OraclesUiColor rom_button = row == ORACLES_ROW_ROM ? accent : normal;
    text(draw, &oracles_ui_row_button, &l.rom_button, "Choose ROM\xe2\x80\xa6", rom_button);
    text(draw, &oracles_ui_row_button, &l.rom_button_dot, oracles_ui_item_dot, rom_button);

    if (t.patched) {
        text(draw, &oracles_ui_row_label, &l.label_patch, "Patch", oracles_ui_rgb(LABEL));
        text(draw, &oracles_ui_row_title, &l.patch_file, t.patch_file, oracles_ui_rgb(game->patch_file[0] ? TEXT : TEXT_NONE));
        dot(draw, &l.patch_dot, tones.patch);
        text(draw, &oracles_ui_row_status, &l.patch_status, t.patch_status, oracles_ui_rgb(tone_texts[tones.patch]));
        const OraclesUiColor patch_button = row == ORACLES_ROW_PATCH ? accent : normal;
        text(draw, &oracles_ui_row_button, &l.patch_button, "Choose patch\xe2\x80\xa6", patch_button);
        text(draw, &oracles_ui_row_button, &l.patch_button_dot, oracles_ui_item_dot, patch_button);
        text(draw, &oracles_ui_row_path, &l.patch_folder, t.patch_folder, oracles_ui_rgb(PATH));
        dot(draw, &l.image_dot, tones.image);
        text(draw, &oracles_ui_row_status, &l.image_status, t.image_status, oracles_ui_rgb(tone_texts[tones.image]));
    }

    text(draw, &oracles_ui_row_title, &l.save_file, t.save_file, oracles_ui_rgb(game->save_file[0] ? TEXT : TEXT_NONE));
    text(draw, &oracles_ui_row_text, &l.save_line, t.save_line, oracles_ui_rgb(NOTE));
    const OraclesUiColor save_button = !game->usable ? off : row == ORACLES_ROW_SAVE ? accent : normal;
    text(draw, &oracles_ui_row_button, &l.save_button, "Open folder", save_button);
    text(draw, &oracles_ui_row_button, &l.save_button_dot, oracles_ui_item_dot, save_button);

    const OraclesUiColor play = !game->usable ? oracles_ui_rgb(row == ORACLES_ROW_PLAY ? BUTTON_DISABLED_FOCUSED : BUTTON_DISABLED)
                                              : row == ORACLES_ROW_PLAY ? accent : normal;
    text(draw, &oracles_ui_row_text, &l.play_note, t.play_note, oracles_ui_rgb(NOTE));
    text(draw, &oracles_ui_play, &l.play, "Play", play);
    text(draw, &oracles_ui_play, &l.play_dot, oracles_ui_item_dot, play);
}

static int inside(const OraclesUiBox *b, float x, float y) { return x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h; }

int oracles_ui_page_hit(const OraclesHomeNav *nav, float x, float y)
{
    OraclesUiGameTexts t;
    char state[ORACLES_HOME_STATE_LENGTH];
    page_tones tones;
    game_texts(nav, &t, state, &tones);
    OraclesUiGameLayout l;
    oracles_ui_layout_game(&t, &l);
    for (int r = 0; r < ORACLES_GAME_ROWS; r++) if (inside(&l.rows[r], x, y)) return r;   /* a row the page has not is empty */
    return -1;
}

/* ---- Display --------------------------------------------------------------------- */

static void display_texts(const OraclesHomeNav *nav, OraclesUiDisplayTexts *t)
{
    const OraclesHomeHero hero = oracles_home_hero(nav);
    memset(t, 0, sizeof *t);
    t->over = oracles_home_over(hero);
    t->title = oracles_home_title(hero);
    for (int w = 0; w < 4; w++) oracles_display_window_texts(nav, w, t->window_names[w], t->window_sizes[w], sizeof t->window_sizes[w]);
    t->window_note = oracles_display_window_note;
    oracles_display_reduced(nav, t->window_reduced, sizeof t->window_reduced);
    t->profile_note = oracles_display_profile_note(nav);
    t->later = nav->in_game;
    t->explanations[0] = oracles_display_explanation(ORACLES_DISPLAY_COLOUR);
    t->explanations[1] = oracles_display_explanation(ORACLES_DISPLAY_TRANSITIONS);
    t->explanations[2] = oracles_display_explanation(ORACLES_DISPLAY_VSYNC);
    t->view_explanation = oracles_display_explanation(ORACLES_DISPLAY_VIEW);
    for (int p = 0; p < ORACLES_PROFILES; p++) oracles_profile_size(nav, p, t->profile_sizes[p], sizeof t->profile_sizes[p]);
    for (int v = 0; v < 3; v++) oracles_display_view_size(nav, v, t->view_sizes[v], sizeof t->view_sizes[v]);
    /* The diagram's box has the screen's shape at its height, as the sizes take it: 432 wide for 16:9 (and any wider
     * screen, which the view takes as 16:9), 324 for 4:3. */
    t->diagram_box_w = oracles_display_screen_4_3(nav) ? 324.0f : ORACLES_UI_DIAGRAM_W;
    oracles_display_diagram(nav, t->diagram_box_w, ORACLES_UI_DIAGRAM_H, &t->diagram_w, &t->diagram_h, t->diagram_label, sizeof t->diagram_label);
}

void oracles_ui_display_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent)
{
    OraclesUiDisplayTexts t;
    display_texts(nav, &t);
    OraclesUiDisplayLayout l;
    oracles_ui_layout_display(&t, &l);
    const unsigned row = nav->row;

    text(draw, &oracles_ui_page_section, &l.head.section, "Display", accent);
    text(draw, &oracles_ui_page_over, &l.head.over, t.over, oracles_ui_rgb(OVER));
    wrapped(draw, &oracles_ui_page_title, &l.head.title, oracles_ui_rgb(TITLE));
    /* The window on the screen. */
    oracles_ui_fill_round_rect(draw, l.diagram.x, l.diagram.y, l.diagram.w, l.diagram.h, 4.0f, oracles_ui_rgba(DIAGRAM, DIAGRAM_OPACITY));
    oracles_ui_stroke_round_rect(draw, l.diagram.x, l.diagram.y, l.diagram.w, l.diagram.h, 4.0f, 1.0f, oracles_ui_rgba(0xffffffu, DIAGRAM_BORDER_OPACITY));
    const OraclesUiBox *w = &l.diagram_window;
    oracles_ui_fill_rect(draw, w->x, w->y, w->w, w->h, oracles_ui_rgba(0xffffffu, DIAGRAM_WINDOW_OPACITY));
    oracles_ui_stroke_round_rect(draw, w->x, w->y, w->w, w->h, 0.0f, 2.0f, accent);
    text(draw, &oracles_ui_diagram_label, &l.diagram_label, t.diagram_label, oracles_ui_rgb(DIAGRAM_LABEL));

    oracles_ui_fill_round_rect(draw, l.panel.x, l.panel.y, l.panel.w, l.panel.h, 10.0f, oracles_ui_rgba(PANEL, PANEL_OPACITY));
    oracles_ui_stroke_round_rect(draw, l.panel.x, l.panel.y, l.panel.w, l.panel.h, 10.0f, 1.0f, oracles_ui_rgba(PANEL_BORDER, PANEL_BORDER_OPACITY));
    /* View and the transitions carry the Enhanced view: in Faithful their rows are dimmed, label and all. */
    const float applied = oracles_display_transitions_apply(nav) ? 1.0f : ROW_NOT_APPLIED;
    for (unsigned r = 0; r < ORACLES_DISPLAY_ROWS; r++) {
        const float opacity = r == ORACLES_DISPLAY_TRANSITIONS || r == ORACLES_DISPLAY_VIEW ? applied : 1.0f;
        highlight_dimmed(draw, &l.rows[r], r == row, opacity);
        wrapped(draw, &oracles_ui_row_label, &l.labels[r], oracles_ui_rgba(LABEL, opacity));
    }
    for (int p = 0; p < ORACLES_PROFILES; p++)
        option(draw, &l.profiles[p], &oracles_ui_option_name, oracles_profile_names[p], t.profile_sizes[p], p == (int)nav->display.profile,
               row == ORACLES_DISPLAY_PROFILE, accent, OPTION_OFF, 1.0f);
    text(draw, &oracles_ui_row_text, &l.profile_note, t.profile_note, oracles_ui_rgb(NOTE));
    for (int i = 0; i < 4; i++)
        option(draw, &l.windows[i], &oracles_ui_option_name, t.window_names[i], t.window_sizes[i], i == nav->display.window,
               row == ORACLES_DISPLAY_WINDOW, accent, OPTION_OFF, 1.0f);
    text(draw, &oracles_ui_row_text, &l.window_note, t.window_note, oracles_ui_rgb(NOTE));
    text(draw, &oracles_ui_row_text, &l.window_reduced, t.window_reduced, oracles_ui_rgb(NOTE));
    wrapped(draw, &oracles_ui_row_text, &l.view_explanation, oracles_ui_rgba(NOTE, applied));
    for (int v = 0; v < 3; v++)
        option(draw, &l.views[v], &oracles_ui_choice, oracles_display_view_names[v], t.view_sizes[v], v == nav->display.view,
               row == ORACLES_DISPLAY_VIEW, accent, CHOICE_OFF, applied);
    wrapped(draw, &oracles_ui_row_text, &l.explanations[0], oracles_ui_rgb(NOTE));
    wrapped(draw, &oracles_ui_row_text, &l.explanations[1], oracles_ui_rgba(NOTE, applied));
    text(draw, &oracles_ui_row_note, &l.transitions_note, oracles_ui_transitions_note, oracles_ui_rgba(NOTE_ITALIC, applied));
    if (t.later) {
        text(draw, &oracles_ui_row_note, &l.profile_later, oracles_ui_later, oracles_ui_rgb(NOTE_ITALIC));
        text(draw, &oracles_ui_row_note, &l.window_later, oracles_ui_later, oracles_ui_rgb(NOTE_ITALIC));
        text(draw, &oracles_ui_row_note, &l.view_later, oracles_ui_later, oracles_ui_rgba(NOTE_ITALIC, applied));
        text(draw, &oracles_ui_row_note, &l.transitions_later, oracles_ui_later, oracles_ui_rgba(NOTE_ITALIC, applied));
        text(draw, &oracles_ui_row_note, &l.vsync_later, oracles_ui_later, oracles_ui_rgb(NOTE_ITALIC));
    }
    wrapped(draw, &oracles_ui_row_text, &l.explanations[2], oracles_ui_rgb(NOTE));
    for (int c = 0; c < 2; c++)
        option(draw, &l.colour[c], &oracles_ui_choice, oracles_display_colour_choices[c], NULL, c == nav->display.colour, row == ORACLES_DISPLAY_COLOUR, accent, CHOICE_OFF, 1.0f);
    for (int c = 0; c < 2; c++)
        option(draw, &l.transitions[c], &oracles_ui_choice, oracles_ui_transition_choices[c], NULL, c == nav->display.transitions,
               row == ORACLES_DISPLAY_TRANSITIONS, accent, CHOICE_OFF, applied);
    for (int c = 0; c < 3; c++)
        option(draw, &l.vsync[c], &oracles_ui_choice, oracles_display_vsync_choices[c], NULL, c == nav->display.vsync, row == ORACLES_DISPLAY_VSYNC, accent, CHOICE_OFF, 1.0f);
}

int oracles_ui_display_hit(const OraclesHomeNav *nav, float x, float y, int *option)
{
    OraclesUiDisplayTexts t;
    display_texts(nav, &t);
    OraclesUiDisplayLayout l;
    oracles_ui_layout_display(&t, &l);
    *option = -1;
    for (int p = 0; p < ORACLES_PROFILES; p++) if (inside(&l.profiles[p].box, x, y)) { *option = p; return ORACLES_DISPLAY_PROFILE; }
    for (int i = 0; i < 4; i++) if (inside(&l.windows[i].box, x, y)) { *option = i; return ORACLES_DISPLAY_WINDOW; }
    for (int v = 0; v < 3; v++) if (inside(&l.views[v].box, x, y)) { *option = v; return ORACLES_DISPLAY_VIEW; }
    for (int c = 0; c < 2; c++) if (inside(&l.transitions[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_TRANSITIONS; }
    for (int c = 0; c < 2; c++) if (inside(&l.colour[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_COLOUR; }
    for (int c = 0; c < 3; c++) if (inside(&l.vsync[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_VSYNC; }
    for (int r = 0; r < ORACLES_DISPLAY_ROWS; r++) if (inside(&l.rows[r], x, y)) return r;
    return -1;
}

/* ---- Mods ------------------------------------------------------------------------ */

void oracles_ui_mods_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent)
{
    OraclesUiModsLayout l;
    oracles_ui_layout_mods(nav, &l);
    const OraclesHomeMods *mods = oracles_mods_list(nav);
    const unsigned row = nav->row, play_row = oracles_mods_row_play(nav);
    const OraclesHomeHero hero = oracles_home_hero(nav);

    text(draw, &oracles_ui_page_section, &l.head.section, "Mods", accent);
    text(draw, &oracles_ui_page_over, &l.head.over, oracles_home_over(hero), oracles_ui_rgb(OVER));
    wrapped(draw, &oracles_ui_page_title, &l.head.title, oracles_ui_rgb(TITLE));
    text(draw, &oracles_ui_page_state, &l.head.state, l.state, oracles_ui_rgb(STATE));

    oracles_ui_fill_round_rect(draw, l.panel.x, l.panel.y, l.panel.w, l.panel.h, 10.0f, oracles_ui_rgba(PANEL, PANEL_OPACITY));
    oracles_ui_stroke_round_rect(draw, l.panel.x, l.panel.y, l.panel.w, l.panel.h, 10.0f, 1.0f, oracles_ui_rgba(PANEL_BORDER, PANEL_BORDER_OPACITY));
    text(draw, &oracles_ui_row_label, &l.label_folder, "Folder", oracles_ui_rgb(LABEL));
    text(draw, &oracles_ui_row_label, &l.label_mods, "Mods", oracles_ui_rgb(LABEL));

    highlight(draw, &l.folder_row, row == ORACLES_MODS_ROW_FOLDER);
    text(draw, &oracles_ui_row_path, &l.folder_path, l.folder_text, oracles_ui_rgb(MODS_PATH));
    text(draw, &oracles_ui_row_text, &l.folder_line, oracles_mods_folder_line, oracles_ui_rgb(NOTE));
    const OraclesUiColor folder_button = row == ORACLES_MODS_ROW_FOLDER ? accent : oracles_ui_rgb(BUTTON);
    text(draw, &oracles_ui_row_button, &l.folder_button, "Open folder", folder_button);
    text(draw, &oracles_ui_row_button, &l.folder_button_dot, oracles_ui_item_dot, folder_button);

    wrapped(draw, &oracles_ui_row_status, &l.empty, oracles_ui_rgb(MODS_EMPTY));
    for (unsigned i = 0; i < l.shown; i++) {
        const OraclesUiModLayout *m = &l.mods[i];
        const OraclesHomeMod *mod = &mods->mods[m->index];
        const int on = mod->active && !mod->refused;
        const float opacity = mod->refused ? MODS_REFUSED : 1.0f;
        highlight(draw, &m->row, row == m->index + 1u);
        /* The switch: a pill, its knob at the right when on. */
        const OraclesUiBox *t = &m->toggle;
        oracles_ui_fill_round_rect(draw, t->x, t->y, t->w, t->h, t->h * 0.5f, oracles_ui_fade(on ? accent : oracles_ui_rgb(MODS_SWITCH_OFF), opacity));
        oracles_ui_stroke_round_rect(draw, t->x, t->y, t->w, t->h, t->h * 0.5f, 1.0f,
                                     oracles_ui_fade(on ? accent : oracles_ui_rgba(0xffffffu, MODS_SWITCH_OFF_BORDER_OPACITY), opacity));
        oracles_ui_fill_round_rect(draw, m->knob.x, m->knob.y, m->knob.w, m->knob.h, m->knob.w * 0.5f, oracles_ui_rgba(on ? MODS_KNOB_ON : MODS_KNOB_OFF, opacity));
        text(draw, &oracles_ui_row_title, &m->name, m->name_text, oracles_ui_rgb(mod->refused ? LABEL : TEXT));
        text(draw, &oracles_ui_mod_games, &m->games, m->games_text, oracles_ui_rgb(PATH));
        dot(draw, &m->dot, mod->refused ? ORACLES_TONE_ERROR : on ? ORACLES_TONE_OK : ORACLES_TONE_NONE);
        text(draw, &oracles_ui_row_text, &m->line, m->line_text, oracles_ui_rgb(mod->refused ? tone_texts[ORACLES_TONE_ERROR] : NOTE));
    }
    text(draw, &oracles_ui_row_text, &l.count, l.count_text, oracles_ui_rgb(NOTE));
    wrapped(draw, &oracles_ui_row_note, &l.note, oracles_ui_rgb(NOTE_ITALIC));

    const int playable = !l.play_note_text[0];
    highlight(draw, &l.play_row, row == play_row);
    const OraclesUiColor play = !playable ? oracles_ui_rgb(row == play_row ? BUTTON_DISABLED_FOCUSED : BUTTON_DISABLED)
                                          : row == play_row ? accent : oracles_ui_rgb(BUTTON);
    text(draw, &oracles_ui_row_text, &l.play_note, l.play_note_text, oracles_ui_rgb(NOTE));
    text(draw, &oracles_ui_play, &l.play, "Play", play);
    text(draw, &oracles_ui_play, &l.play_dot, oracles_ui_item_dot, play);
}

int oracles_ui_mods_hit(const OraclesHomeNav *nav, float x, float y)
{
    OraclesUiModsLayout l;
    oracles_ui_layout_mods(nav, &l);
    if (inside(&l.folder_row, x, y)) return (int)ORACLES_MODS_ROW_FOLDER;
    for (unsigned i = 0; i < l.shown; i++) if (inside(&l.mods[i].row, x, y)) return (int)l.mods[i].index + 1;
    if (inside(&l.play_row, x, y)) return (int)oracles_mods_row_play(nav);
    return -1;
}
