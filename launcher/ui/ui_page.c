#include "ui_page.h"

#include "ui_page_layout.h"
#include "ui_version.h"
#ifdef ORACLES_RETROACHIEVEMENTS
#include "ui_achievements_draw.h"
#endif

#include <math.h>
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
#define LANGUAGE_MISSING 0x57545fu          /* a language whose source is missing, and its frame */
#define LANGUAGE_MISSING_OPACITY 0.06f
#define LANGUAGE_SHOWN_OPACITY 0.3f         /* the same with the focus on it */
#define FRAME_ON_OPACITY 0.4f
#define FRAME_OFF_OPACITY 0.1f
#define OPTION_NOT_ALLOWED 0.38f
#define ROW_NOT_APPLIED 0.4f
#define DIAGRAM 0x0a090du
#define DIAGRAM_OPACITY 0.8f
#define DIAGRAM_BORDER_OPACITY 0.3f
#define DIAGRAM_WINDOW_OPACITY 0.07f
#define DIAGRAM_LABEL 0xa19eaau
#define HELP 0xc9c6cfu
#define ADVANCED_BORDER_OPACITY 0.14f   /* 4:3's Advanced button, framed */
#define CUSTOM_OPACITY 0.6f             /* Quality's Custom, shown and never chosen */
#define PREVIEW 0x4a4f58u                /* Display › Text's preview: the band, its status bar, its box's arrow */
#define PREVIEW_HUD 0x2a2d33u
#define SCENE_GROUND 0x0c0b0fu           /* the scene's own background, which a scrolled page's edges fade into */
#define ROWS_FADE 40.0f
/* Floating's panels in the preview: the bar's cream with an edge, or a neutral gray, at 80 %; the hearts, the keys' badges
 * at 90 %. */
#define PANEL_BAR 0xfaf4dcu
#define PANEL_NEUTRAL 0xe2e2e6u
#define PANEL_FILL_OPACITY 0.80f
#define PANEL_BADGE_OPACITY 0.90f
#define PANEL_EDGE_OPACITY 0.45f
#define PANEL_HEART 0xdb4241u
#define PANEL_BADGE 0x3275b4u
#define PANEL_BADGE_NEUTRAL 0x5b5e66u
#define PREVIEW_ARROW 0xe0302au
#define PREVIEW_LETTERBOX 0x050507u     /* around the band where it is smaller than its frame, as around the game */
#define FILTER_REFUSED 0x6f6c78u        /* Filter's name where it cannot run here */
#define FILTER_MARK_REFUSED 0x77747fu   /* its mark */
#define FILTER_MARK_OPACITY 0.5f        /* the marks of the other choices, fainter where they cannot run here */
#define FILTER_MARK_REFUSED_OPACITY 0.16f

#define MODS_PATH 0xc9c6cfu
#define MODS_EMPTY 0xd6d3dcu
#define MODS_SWITCH_OFF 0x26252cu
#define MODS_SWITCH_OFF_BORDER_OPACITY 0.16f
#define MODS_KNOB_ON 0x0c0b0fu
#define MODS_KNOB_OFF 0x8a8793u
#define MODS_REFUSED 0.4f

static const uint32_t tone_dots[4] = { 0x76cf8au, 0xebbd57u, 0xf97770u, 0x6f6c78u };
static const uint32_t tone_texts[4] = { 0xd6d3dcu, 0xecd198u, 0xf9aea7u, 0xa19eaau };

/* The tones of the page's status lines: the ROM's, a fan game's patch's and image's, an Oracle's European ROM's, the
 * save's. */
/* What differs between the layouts in the drawing: the rows' corners, the frames' corners and width. */
typedef struct look {
    float row_radius, frame_radius, choice_radius, frame_width;
} look;
static const look look_16_9 = { 6.0f, 6.0f, 5.0f, 1.0f };
static const look look_4_3 = { 8.0f, 7.0f, 7.0f, 2.0f };

static const look *look_of(OraclesUiLayout layout) { return layout == ORACLES_UI_LAYOUT_4_3 ? &look_4_3 : &look_16_9; }

/* A line the layout has not laid out (zero) is not drawn: the other layout's. */
static void text(OraclesUiDraw *draw, const OraclesUiTextStyle *style, const OraclesUiLine *line, const char *s, OraclesUiColor color)
{
    if (s && s[0] && line->h > 0.0f) oracles_ui_draw_text(draw, style, line->x, line->baseline, s, color);
}

static void wrapped(OraclesUiDraw *draw, const OraclesUiTextStyle *style, const OraclesUiWrapped *w, OraclesUiColor color)
{
    for (unsigned i = 0; i < w->count; i++) text(draw, style, &w->lines[i], w->text[i], color);
}

static void dot(OraclesUiDraw *draw, const OraclesUiBox *b, OraclesTone tone)
{
    oracles_ui_fill_round_rect(draw, b->x, b->y, b->w, b->h, b->w * 0.5f, oracles_ui_rgb(tone_dots[tone]));
}

/* A framed option: its frame, its name, its size when it has one; `profile`, a profile's or a window's frame, which
 * 16:9 rounds more than a choice's. */
static void option(OraclesUiDraw *draw, const OraclesUiPageStyles *st, const look *lk, const OraclesUiOptionLayout *o, int profile, const char *name,
                   const char *size, int on, int focused, OraclesUiColor accent, uint32_t off, float opacity)
{
    const OraclesUiColor frame = on ? (focused ? accent : oracles_ui_rgba(0xffffffu, FRAME_ON_OPACITY)) : oracles_ui_rgba(0xffffffu, FRAME_OFF_OPACITY);
    const OraclesUiColor color = on ? (focused ? accent : oracles_ui_rgb(TEXT)) : oracles_ui_rgb(off);
    oracles_ui_stroke_round_rect(draw, o->box.x, o->box.y, o->box.w, o->box.h, profile ? lk->frame_radius : lk->choice_radius, lk->frame_width,
                                 oracles_ui_fade(frame, opacity));
    text(draw, profile ? st->option_name : st->choice, &o->name, name, oracles_ui_fade(color, opacity));
    if (size) text(draw, st->option_size, &o->size, size, oracles_ui_rgba(PATH, opacity));
}

static void highlight_dimmed(OraclesUiDraw *draw, const look *lk, const OraclesUiBox *b, int on, float opacity)
{
    if (on) oracles_ui_fill_round_rect(draw, b->x, b->y, b->w, b->h, lk->row_radius, oracles_ui_rgba(0xffffffu, ROW_HIGHLIGHT_OPACITY * opacity));
}

static void highlight(OraclesUiDraw *draw, const look *lk, const OraclesUiBox *b, int on) { highlight_dimmed(draw, lk, b, on, 1.0f); }

/* The page's head: its section in the accent, the words above the title (16:9), the title, the state (none: NULL). */
static void head(OraclesUiDraw *draw, const OraclesUiPageStyles *st, const OraclesUiPageHead *h, const char *section, const char *over,
                 const char *state, OraclesUiColor accent)
{
    text(draw, st->section, &h->section, section, accent);
    text(draw, st->over, &h->over, over, oracles_ui_rgb(OVER));
    wrapped(draw, st->title, &h->title, oracles_ui_rgb(TITLE));
    text(draw, st->state, &h->state, state, oracles_ui_rgb(STATE));
}

/* 16:9's panel behind the rows; 4:3 has none. */
static void panel(OraclesUiDraw *draw, const OraclesUiBox *p)
{
    if (p->w <= 0.0f) return;
    oracles_ui_fill_round_rect(draw, p->x, p->y, p->w, p->h, 10.0f, oracles_ui_rgba(PANEL, PANEL_OPACITY));
    oracles_ui_stroke_round_rect(draw, p->x, p->y, p->w, p->h, 10.0f, 1.0f, oracles_ui_rgba(PANEL_BORDER, PANEL_BORDER_OPACITY));
}

void oracles_ui_page_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent)
{
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    const OraclesUiPageStyles *st = oracles_ui_page_styles(nav->layout);
    const look *lk = look_of(nav->layout);
    OraclesUiGameTexts t;
    char state[ORACLES_HOME_STATE_LENGTH];
    OraclesUiGameTones tones;
    oracles_ui_game_texts(nav, &t, state, &tones);
    OraclesUiGameLayout l;
    oracles_ui_layout_game(nav->layout, &t, &l);
    const unsigned row = nav->row;

    head(draw, st, &l.head, t.section, t.over, t.state, accent);
    text(draw, nav->layout == ORACLES_UI_LAYOUT_4_3 ? &oracles_ui_page_credit_4_3 : &oracles_ui_page_credit, &l.head.credit, t.credit, oracles_ui_rgb(STATE));
    panel(draw, &l.panel);
    /* Language is dimmed, label and all, while it lists English (US) alone. */
    const float language_opacity = oracles_page_language_dimmed(game) ? ROW_NOT_APPLIED : 1.0f;
    for (unsigned r = 0; r < ORACLES_GAME_ROWS; r++) highlight_dimmed(draw, lk, &l.rows[r], r == row, r == ORACLES_ROW_LANGUAGE ? language_opacity : 1.0f);
    text(draw, st->row_label, &l.label_rom, t.patched ? "Base ROM" : "ROM", oracles_ui_rgb(LABEL));
    text(draw, st->row_label, &l.label_save, "Save", oracles_ui_rgb(LABEL));
    /* Oracles Combined's European ROMs: each label, file and status; Choose… while it is not set, in the accent while its
     * row is highlighted, else where it is set (16:9). */
    for (int h = 0; t.joined && h < 2; h++) {
        const OraclesHomeEuropean *e = &game->joined[h];
        wrapped(draw, st->row_label, &l.joined_labels[h], oracles_ui_rgb(LABEL));
        text(draw, st->row_title, &l.joined_files[h], t.joined_file[h], oracles_ui_rgb(e->file[0] ? TEXT : TEXT_NONE));
        dot(draw, &l.joined_dots[h], tones.joined[h]);
        text(draw, st->row_status, &l.joined_status[h], t.joined_status[h], oracles_ui_rgb(tone_texts[tones.joined[h]]));
        text(draw, st->row_note, &l.joined_from[h], t.joined_from[h], oracles_ui_rgb(NOTE_ITALIC));
        if (oracles_page_joined_set(game, h)) continue;
        const OraclesUiColor choose = row == (h ? ORACLES_ROW_JOINED : ORACLES_ROW_ROM) ? accent : oracles_ui_rgb(BUTTON);
        text(draw, st->row_button, &l.joined_buttons[h], "Choose\xe2\x80\xa6", choose);
        text(draw, st->row_button, &l.joined_button_dots[h], oracles_ui_item_dot, choose);
    }

    const OraclesUiColor normal = oracles_ui_rgb(BUTTON);
    text(draw, st->row_title, &l.rom_file, t.rom_file, oracles_ui_rgb(game->rom_file[0] ? TEXT : TEXT_NONE));
    text(draw, st->row_path, &l.rom_folder, t.rom_folder, oracles_ui_rgb(PATH));
    dot(draw, &l.rom_dot, tones.rom);
    wrapped(draw, st->row_status, &l.rom_status, oracles_ui_rgb(tone_texts[tones.rom]));
    wrapped(draw, st->row_text, &l.rom_note, oracles_ui_rgb(NOTE));
    wrapped(draw, st->row_text, &l.rom_hotkeys_note, oracles_ui_rgb(NOTE));
    const OraclesUiColor rom_button = row == ORACLES_ROW_ROM ? accent : normal;
    if (!t.joined) {
        text(draw, st->row_button, &l.rom_button, "Choose ROM\xe2\x80\xa6", rom_button);
        text(draw, st->row_button, &l.rom_button_dot, oracles_ui_item_dot, rom_button);
    }

    if (t.patched) {
        text(draw, st->row_label, &l.label_patch, "Patch", oracles_ui_rgb(LABEL));
        text(draw, st->row_title, &l.patch_file, t.patch_file, oracles_ui_rgb(game->patch_file[0] ? TEXT : TEXT_NONE));
        dot(draw, &l.patch_dot, tones.patch);
        wrapped(draw, st->row_status, &l.patch_status, oracles_ui_rgb(tone_texts[tones.patch]));
        const OraclesUiColor patch_button = row == ORACLES_ROW_PATCH ? accent : normal;
        text(draw, st->row_button, &l.patch_button, "Choose patch\xe2\x80\xa6", patch_button);
        text(draw, st->row_button, &l.patch_button_dot, oracles_ui_item_dot, patch_button);
        text(draw, st->row_path, &l.patch_folder, t.patch_folder, oracles_ui_rgb(PATH));
        dot(draw, &l.image_dot, tones.image);
        text(draw, st->row_status, &l.image_status, t.image_status, oracles_ui_rgb(tone_texts[tones.image]));
        for (unsigned i = 0; i < t.image_notes; i++) text(draw, st->row_text, &l.image_notes[i], t.image_note[i], oracles_ui_rgb(NOTE));
    }

    /* Save: its file, its actions (the one left and right chose in the accent while the row is highlighted, one that does
     * not act greyed), the line about it, then its note or its status. */
    text(draw, st->row_title, &l.save_file, t.save_file, oracles_ui_rgb(game->save_file[0] ? TEXT : TEXT_NONE));
    for (int i = 0; i < ORACLES_PAGE_SAVE_ACTIONS; i++) {
        const int chosen = row == ORACLES_ROW_SAVE && nav->save_action == i;
        const OraclesUiColor color = !oracles_page_save_action_live(nav, i) ? oracles_ui_rgb(chosen ? BUTTON_DISABLED_FOCUSED : BUTTON_DISABLED)
                                     : chosen ? accent : normal;
        text(draw, st->row_button, &l.save_buttons[i], t.save_actions[i], color);
        text(draw, st->row_button, &l.save_button_dots[i], oracles_ui_item_dot, color);
    }
    text(draw, st->row_text, &l.save_line, t.save_line, oracles_ui_rgb(NOTE));
    wrapped(draw, nav->layout == ORACLES_UI_LAYOUT_4_3 ? st->page_note : st->row_note, &l.save_note, oracles_ui_rgb(NOTE_ITALIC));
    if (t.save_status) {
        dot(draw, &l.save_dot, tones.save);
        wrapped(draw, st->row_status, &l.save_status, oracles_ui_rgb(tone_texts[tones.save]));
        wrapped(draw, st->row_status, &l.save_status2, oracles_ui_rgb(NOTE));
    }

    if (t.european && !t.editions) {
        /* Editions: its explanation (16:9), its summary and its arrow, in the accent while highlighted. */
        const OraclesUiColor link_color = row == ORACLES_ROW_EDITIONS ? accent : oracles_ui_rgb(LABEL);
        const OraclesUiBox *a = &l.editions_arrow;
        const float arrow[6] = { a->x, a->y, a->x + a->w, a->y + a->h * 0.5f, a->x, a->y + a->h };
        text(draw, st->row_label, &l.label_editions, "Editions", oracles_ui_rgb(LABEL));
        wrapped(draw, st->row_text, &l.editions_explanation, oracles_ui_rgb(NOTE));
        text(draw, nav->layout == ORACLES_UI_LAYOUT_4_3 ? st->row_button : st->choice, &l.editions_summary, t.editions_summary, link_color);
        oracles_ui_fill_polygon(draw, arrow, 3, link_color);
    }
    for (int e = 0; t.editions && e < ORACLES_PAGE_EDITIONS; e++) {
        /* Cartridge › Editions: each edition's file, folder and status; its buttons, the one left and right chose in the
         * accent while the row is highlighted. */
        const unsigned edition_row = oracles_page_edition_row(e);
        wrapped(draw, st->row_label, &l.edition_labels[e], oracles_ui_rgb(LABEL));
        text(draw, st->row_title, &l.edition_files[e], t.edition_file[e], oracles_ui_rgb(oracles_page_edition_state(game, e) != ORACLES_ROM_NONE ? TEXT : TEXT_NONE));
        text(draw, st->row_path, &l.edition_folders[e], t.edition_folder[e], oracles_ui_rgb(PATH));
        dot(draw, &l.edition_dots[e], tones.editions[e]);
        text(draw, st->row_status, &l.edition_statuses[e], t.edition_status[e], oracles_ui_rgb(tone_texts[tones.editions[e]]));
        for (unsigned i = 0; i < t.edition_buttons[e]; i++) {
            const int chosen = t.edition_buttons[e] == 1 || nav->eu_action == (int)i;
            const OraclesUiColor color = row == edition_row && chosen ? accent : normal;
            text(draw, st->row_button, &l.edition_buttons[e][i], t.edition_button_labels[e][i], color);
            text(draw, st->row_button, &l.edition_button_dots[e][i], oracles_ui_item_dot, color);
        }
    }
    if (t.languages) {
        /* Language: each language shown framed, the one chosen marked; one whose source is missing greyed, lighter with
         * the focus on it. */
        text(draw, st->row_label, &l.label_language, "Language", oracles_ui_rgba(LABEL, language_opacity));
        const int chosen = oracles_page_language(nav, game), seen = oracles_page_language_focus(nav, game);
        const int focused = row == ORACLES_ROW_LANGUAGE;
        for (int k = 0; k < t.language_count; k++) {
            const int i = t.language_order[k];
            const OraclesUiOptionLayout *o = &l.languages[i];
            const int on = chosen == i, there = oracles_page_language_available(game, i), shown = !there && focused && seen == i;
            const OraclesUiColor frame = on ? (focused ? accent : oracles_ui_rgba(0xffffffu, FRAME_ON_OPACITY))
                                       : shown ? oracles_ui_rgba(0xffffffu, LANGUAGE_SHOWN_OPACITY)
                                       : oracles_ui_rgba(0xffffffu, there ? FRAME_OFF_OPACITY : LANGUAGE_MISSING_OPACITY);
            const OraclesUiColor color = on ? (focused ? accent : oracles_ui_rgb(TEXT)) : oracles_ui_rgb(shown ? BUTTON_DISABLED_FOCUSED : there ? CHOICE_OFF : LANGUAGE_MISSING);
            oracles_ui_stroke_round_rect(draw, o->box.x, o->box.y, o->box.w, o->box.h, lk->choice_radius, lk->frame_width, oracles_ui_fade(frame, language_opacity));
            text(draw, st->language, &o->name, oracles_page_language_names[i], oracles_ui_fade(color, language_opacity));
        }
        text(draw, st->row_text, &l.language_explanation, t.language_explanation, oracles_ui_rgba(NOTE, language_opacity));
        text(draw, st->row_note, &l.language_note, t.language_note, oracles_ui_rgba(NOTE_ITALIC, language_opacity));
    }
    if (t.editions) return;

    const OraclesUiColor play = !game->usable ? oracles_ui_rgb(row == ORACLES_ROW_PLAY ? BUTTON_DISABLED_FOCUSED : BUTTON_DISABLED)
                                              : row == ORACLES_ROW_PLAY ? accent : normal;
    text(draw, st->row_text, &l.play_note, t.play_note, oracles_ui_rgb(NOTE));
    text(draw, st->play, &l.play, "Play", play);
    text(draw, st->play, &l.play_dot, oracles_ui_item_dot, play);
}

/* The import's confirmation: the scene dimmed, the panel, its texts, and its buttons framed, the one left and right
 * chose in the accent. */
#define IMPORT_BACKDROP 0x050408u
#define IMPORT_BACKDROP_OPACITY 0.62f
#define IMPORT_PANEL_OPACITY 0.97f
#define IMPORT_BORDER_OPACITY 0.12f
#define IMPORT_FILE 0xefedf2u
#define IMPORT_BUTTON_BORDER_OPACITY 0.25f

void oracles_ui_page_dialog_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent)
{
    if (nav->screen != ORACLES_SCREEN_GAME || nav->save_flow.state != ORACLES_SAVE_FLOW_CONFIRM) return;
    const OraclesUiPageStyles *st = oracles_ui_page_styles(nav->layout);
    const look *lk = look_of(nav->layout);
    OraclesUiImportLayout l;
    oracles_ui_layout_import(nav->layout, nav->save_flow.file, nav->save_flow.folder, nav->save_flow.replace, &l);
    oracles_ui_fill_rect(draw, l.backdrop.x, l.backdrop.y, l.backdrop.w, l.backdrop.h, oracles_ui_rgba(IMPORT_BACKDROP, IMPORT_BACKDROP_OPACITY));
    oracles_ui_fill_round_rect(draw, l.panel.x, l.panel.y, l.panel.w, l.panel.h, 10.0f, oracles_ui_rgba(PANEL, IMPORT_PANEL_OPACITY));
    oracles_ui_stroke_round_rect(draw, l.panel.x, l.panel.y, l.panel.w, l.panel.h, 10.0f, 1.0f, oracles_ui_rgba(0xffffffu, IMPORT_BORDER_OPACITY));
    text(draw, oracles_ui_import_style(nav->layout, 0), &l.title, oracles_ui_import_title, accent);
    text(draw, st->row_title, &l.file, l.file_text, oracles_ui_rgb(IMPORT_FILE));
    text(draw, st->row_path, &l.folder, l.folder_text, oracles_ui_rgb(PATH));
    wrapped(draw, oracles_ui_import_style(nav->layout, 1), &l.question, oracles_ui_rgb(HELP));
    for (int i = 0; i < 2; i++) {
        const int chosen = nav->save_flow.choice == i;
        const OraclesUiBox *b = &l.buttons[i];
        oracles_ui_stroke_round_rect(draw, b->x, b->y, b->w, b->h, lk->choice_radius, lk->frame_width,
                                     chosen ? accent : oracles_ui_rgba(0xffffffu, IMPORT_BUTTON_BORDER_OPACITY));
        text(draw, oracles_ui_import_style(nav->layout, 2), &l.labels[i], l.label_texts[i], chosen ? accent : oracles_ui_rgb(BUTTON));
    }
}

static int inside(const OraclesUiBox *b, float x, float y) { return x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h; }

int oracles_ui_page_hit(const OraclesHomeNav *nav, float x, float y, int *option)
{
    OraclesUiGameTexts t;
    char state[ORACLES_HOME_STATE_LENGTH];
    OraclesUiGameTones tones;
    oracles_ui_game_texts(nav, &t, state, &tones);
    OraclesUiGameLayout l;
    oracles_ui_layout_game(nav->layout, &t, &l);
    *option = -1;
    /* The import's confirmation open: its buttons alone, Replace (2) and Cancel (3). */
    if (nav->save_flow.state == ORACLES_SAVE_FLOW_CONFIRM) {
        OraclesUiImportLayout dialog;
        oracles_ui_layout_import(nav->layout, nav->save_flow.file, nav->save_flow.folder, nav->save_flow.replace, &dialog);
        for (int i = 0; i < 2; i++) if (inside(&dialog.buttons[i], x, y)) { *option = 2 + i; return ORACLES_ROW_SAVE; }
        return -1;
    }
    /* Save's actions, the editions' buttons, and the languages shown: the others are not drawn. */
    for (int i = 0; !t.editions && i < ORACLES_PAGE_SAVE_ACTIONS; i++) if (inside(&l.save_button_boxes[i], x, y)) { *option = i; return ORACLES_ROW_SAVE; }
    for (int e = 0; t.editions && e < ORACLES_PAGE_EDITIONS; e++)
        for (unsigned i = 0; i < t.edition_buttons[e]; i++)
            if (inside(&l.edition_button_boxes[e][i], x, y)) { *option = (int)i; return (int)oracles_page_edition_row(e); }
    for (int k = 0; t.languages && k < t.language_count; k++)
        if (inside(&l.languages[t.language_order[k]].box, x, y)) { *option = t.language_order[k]; return ORACLES_ROW_LANGUAGE; }
    for (int r = 0; r < ORACLES_GAME_ROWS; r++) if (inside(&l.rows[r], x, y)) return r;   /* a row the page has not is empty */
    return -1;
}

/* ---- Display --------------------------------------------------------------------- */

/* The rectangle (x, y, w, h) of output pixels clipped to `clip`, in the scene's units from the output's corner
 * (zero_x, zero_y), `unit` a pixel, into `out`: whether any of it is left. */
static int clipped(const OraclesUiBox *clip, float x, float y, float w, float h, float zero_x, float zero_y, float unit, float *out)
{
    const float x0 = fmaxf(x, clip->x), y0 = fmaxf(y, clip->y), x1 = fminf(x + w, clip->x + clip->w), y1 = fminf(y + h, clip->y + clip->h);
    if (x1 <= x0 || y1 <= y0) return 0;
    out[0] = zero_x + x0 * unit; out[1] = zero_y + y0 * unit;
    out[2] = (x1 - x0) * unit; out[3] = (y1 - y0) * unit;
    return 1;
}

/* Display › Text's preview: the band's frame, the band, its status bar, on the output's whole pixels, the box, its text
 * and its arrow, a picture at the game's pixels scaled to its place (oracles_ui_text_preview_pixels); the label and the
 * note. */
static void text_preview(OraclesUiDraw *draw, const OraclesUiPageStyles *st, const OraclesUiDisplayTexts *t, const OraclesUiDisplayLayout *l)
{
    const OraclesUiBox *f = &l->preview_frame;
    const OraclesDisplayTextPreview *p = &t->preview;
    /* The output's corner in the scene, and its pixel in the scene's units. */
    float zero_x, zero_y, one_x, one_y;
    oracles_ui_draw_to_scene(draw, 0.0f, 0.0f, &zero_x, &zero_y);
    oracles_ui_draw_to_scene(draw, 1.0f, 1.0f, &one_x, &one_y);
    const float unit = one_x - zero_x;
    if (unit > 0.0f) {
        OraclesUiTextPreviewPixels px;
        const OraclesUiBox frame = { (f->x - zero_x) / unit, (f->y - zero_y) / unit, f->w / unit, f->h / unit };
        oracles_ui_text_preview_pixels(p, frame, l->pixel / unit, &px);
        oracles_ui_fill_round_rect(draw, f->x, f->y, f->w, f->h, 4.0f, oracles_ui_rgb(px.letterbox ? PREVIEW_LETTERBOX : PREVIEW));
        float r[4];
        if (px.letterbox && clipped(&px.clip, px.view.x, px.view.y, px.view.w, px.view.h, zero_x, zero_y, unit, r))
            oracles_ui_fill_rects(draw, r, 1, oracles_ui_rgb(PREVIEW));
        if (clipped(&px.clip, px.hud.x, px.hud.y, px.hud.w, px.hud.h, zero_x, zero_y, unit, r)) oracles_ui_fill_rects(draw, r, 1, oracles_ui_rgb(PREVIEW_HUD));
        static uint32_t picture[ORACLES_UI_PREVIEW_PICTURE_W * ORACLES_UI_PREVIEW_PICTURE_H];
        static const uint32_t colors[3] = { 0xff000000u, 0xffffffffu, 0xff000000u | PREVIEW_ARROW };
        const int w = oracles_ui_text_preview_picture(p, colors, picture);
        const float rect[4] = { zero_x + px.box.x * unit, zero_y + px.box.y * unit, px.box.w * unit, px.box.h * unit };
        const float clip[4] = { zero_x + px.clip.x * unit, zero_y + px.clip.y * unit, px.clip.w * unit, px.clip.h * unit };
        oracles_ui_draw_picture(draw, picture, w, (int)ORACLES_UI_PREVIEW_PICTURE_H, rect, clip);
    }
    /* Floating's panels over the world, in their colors, kept to the frame. */
    if (p->floating) {
        int saved = 0;
        SDL_Rect clip;
        oracles_ui_draw_clip_rect(draw, &saved, &clip);
        oracles_ui_draw_clip(draw, f->x, f->y, f->w, f->h);
        const OraclesUiColor fill = p->neutral ? oracles_ui_rgba(PANEL_NEUTRAL, PANEL_FILL_OPACITY) : oracles_ui_rgba(PANEL_BAR, PANEL_FILL_OPACITY);
        const float e = l->preview_panel_edge;
        for (int i = 0; i < 4; i++) {
            /* Corners of two pixels of the game, whole: two left out of the first line, one of the next. */
            const OraclesUiBox *b = &l->preview_panels[i];
            const float rows[5][4] = { { b->x + 2.0f * e, b->y, b->w - 4.0f * e, e }, { b->x + e, b->y + e, b->w - 2.0f * e, e },
                                       { b->x, b->y + 2.0f * e, b->w, b->h - 4.0f * e }, { b->x + e, b->y + b->h - 2.0f * e, b->w - 2.0f * e, e },
                                       { b->x + 2.0f * e, b->y + b->h - e, b->w - 4.0f * e, e } };
            oracles_ui_fill_rects(draw, &rows[0][0], 5, fill);
            if (p->neutral) continue;
            const float edge[8][4] = { { b->x + 2.0f * e, b->y, b->w - 4.0f * e, e }, { b->x + 2.0f * e, b->y + b->h - e, b->w - 4.0f * e, e },
                                       { b->x, b->y + 2.0f * e, e, b->h - 4.0f * e }, { b->x + b->w - e, b->y + 2.0f * e, e, b->h - 4.0f * e },
                                       { b->x + e, b->y + e, e, e }, { b->x + b->w - 2.0f * e, b->y + e, e, e },
                                       { b->x + e, b->y + b->h - 2.0f * e, e, e }, { b->x + b->w - 2.0f * e, b->y + b->h - 2.0f * e, e, e } };
            oracles_ui_fill_rects(draw, &edge[0][0], 8, oracles_ui_rgba(PREVIEW_HUD, PANEL_EDGE_OPACITY));
        }
        for (int i = 0; i < 6; i++) oracles_ui_fill_rect(draw, l->preview_hearts[i].x, l->preview_hearts[i].y, l->preview_hearts[i].w, l->preview_hearts[i].h, oracles_ui_rgb(PANEL_HEART));
        for (int i = 0; i < 3; i++) oracles_ui_fill_rect(draw, l->preview_inks[i].x, l->preview_inks[i].y, l->preview_inks[i].w, l->preview_inks[i].h, oracles_ui_rgb(PREVIEW_HUD));
        for (int i = 0; i < 2; i++)
            oracles_ui_fill_rect(draw, l->preview_badges[i].x, l->preview_badges[i].y, l->preview_badges[i].w, l->preview_badges[i].h,
                                 oracles_ui_rgba(p->neutral ? PANEL_BADGE_NEUTRAL : PANEL_BADGE, PANEL_BADGE_OPACITY));
        oracles_ui_draw_restore_clip(draw, saved, &clip);
    }
    oracles_ui_stroke_round_rect(draw, f->x, f->y, f->w, f->h, 4.0f, 1.0f, oracles_ui_rgba(0xffffffu, DIAGRAM_BORDER_OPACITY));
    text(draw, st->diagram_label, &l->preview_label, p->label, oracles_ui_rgb(DIAGRAM_LABEL));
    text(draw, st->page_note, &l->preview_note, p->note, oracles_ui_rgb(NOTE_ITALIC));
}

/* Display › Text: its rows, each dimmed where it does not change, and the preview. */
static void text_page(OraclesUiDraw *draw, const OraclesHomeNav *nav, const OraclesUiPageStyles *st, const look *lk, const OraclesUiDisplayTexts *t,
                      const OraclesUiDisplayLayout *l, OraclesUiColor accent)
{
    const int values[ORACLES_DISPLAY_TEXT_ROWS] = { oracles_display_text_shown(nav), nav->display.text_box, nav->display.text_size, nav->display.text_speed };
    const int japanese = oracles_display_japanese(nav);
    for (unsigned i = 0; i < ORACLES_DISPLAY_TEXT_ROWS; i++) {
        const unsigned r = ORACLES_DISPLAY_LETTERS + i;
        const float opacity = oracles_display_text_row_live(nav, r) ? 1.0f : ROW_NOT_APPLIED;
        unsigned count;
        const char *const *names = oracles_display_text_row_choices(r, &count);
        const OraclesUiOptionLayout *options = i == 0u ? l->text : i == 1u ? l->text_box : i == 2u ? l->text_size : l->text_speed;
        highlight_dimmed(draw, lk, &l->rows[r], r == nav->row, opacity);
        wrapped(draw, st->row_label, &l->labels[r], oracles_ui_rgba(LABEL, opacity));
        wrapped(draw, st->row_text, &l->text_explanations[i], oracles_ui_rgba(NOTE, opacity));
        text(draw, st->row_note, &l->text_notes[i], t->text_notes[i], oracles_ui_rgba(NOTE_ITALIC, opacity));
        for (unsigned c = 0; c < count; c++) {
            /* In Japanese, Compact greyed. */
            if (japanese && r == ORACLES_DISPLAY_LETTERS && c == 1u) {
                oracles_ui_stroke_round_rect(draw, options[c].box.x, options[c].box.y, options[c].box.w, options[c].box.h, lk->choice_radius, lk->frame_width,
                                             oracles_ui_rgba(0xffffffu, LANGUAGE_MISSING_OPACITY * opacity));
                text(draw, st->choice, &options[c].name, names[c], oracles_ui_rgba(LANGUAGE_MISSING, opacity));
                continue;
            }
            option(draw, st, lk, &options[c], 0, names[c], NULL, (int)c == values[i], r == nav->row, accent, CHOICE_OFF, opacity);
        }
    }
    text_preview(draw, st, t, l);
}

/* A link's summary and its arrow pointing right, in `color`. */
static void link(OraclesUiDraw *draw, const OraclesUiPageStyles *st, const OraclesUiLine *summary, const OraclesUiBox *a, const char *text_of,
                 OraclesUiColor color)
{
    const float arrow[6] = { a->x, a->y, a->x + a->w, a->y + a->h * 0.5f, a->x, a->y + a->h };
    text(draw, st->choice, summary, text_of, color);
    if (a->w > 0.0f) oracles_ui_fill_polygon(draw, arrow, 3, color);
}

/* Display › Picture's Filter: its value framed, in the accent while the row has the focus (its frame lighter on a choice
 * that cannot run here), the arrows either side, the name, greyed on such a choice, and the eight marks, the shown one
 * filled. */
static void filter_value(OraclesUiDraw *draw, const look *lk, const OraclesHomeNav *nav, const OraclesUiDisplayTexts *t, const OraclesUiDisplayLayout *l,
                         const OraclesUiPageStyles *st, OraclesUiColor accent)
{
    const int focused = nav->row == ORACLES_DISPLAY_FILTER, ok = t->filter_ok;
    const OraclesUiBox *v = &l->filter_value;
    const OraclesUiColor frame = focused ? (ok ? accent : oracles_ui_rgba(0xffffffu, 0.3f)) : oracles_ui_rgba(0xffffffu, FRAME_ON_OPACITY);
    oracles_ui_stroke_round_rect(draw, v->x, v->y, v->w, v->h, lk->choice_radius, lk->frame_width, frame);
    const OraclesUiColor arrow = focused ? accent : oracles_ui_rgb(CHOICE_OFF);
    const OraclesUiBox *p = &l->filter_prev_arrow, *n = &l->filter_next_arrow;
    const float left[6] = { p->x + p->w, p->y, p->x, p->y + p->h * 0.5f, p->x + p->w, p->y + p->h };
    const float right[6] = { n->x, n->y, n->x + n->w, n->y + n->h * 0.5f, n->x, n->y + n->h };
    oracles_ui_fill_polygon(draw, left, 3, arrow);
    oracles_ui_fill_polygon(draw, right, 3, arrow);
    text(draw, st->choice, &l->filter_name, oracles_display_filter_names[t->filter],
         !ok ? oracles_ui_rgb(FILTER_REFUSED) : focused ? accent : oracles_ui_rgb(TEXT));
    const float border = lk->frame_width, radius = nav->layout == ORACLES_UI_LAYOUT_4_3 ? 2.0f : 1.0f;
    for (int f = 0; f < ORACLES_DISPLAY_FILTERS; f++) {
        const OraclesUiBox *m = &l->filter_marks[f];
        if (f == t->filter)
            oracles_ui_fill_round_rect(draw, m->x, m->y, m->w, m->h, radius, !ok ? oracles_ui_rgb(FILTER_MARK_REFUSED) : focused ? accent : oracles_ui_rgb(TEXT));
        else oracles_ui_stroke_round_rect(draw, m->x, m->y, m->w, m->h, radius, border,
                                          oracles_ui_rgba(0xffffffu, oracles_display_filter_reason(nav, f) ? FILTER_MARK_REFUSED_OPACITY : FILTER_MARK_OPACITY));
    }
}

/* The filter's preview: the host's picture through the filter (oracles_ui_draw_inset), framed, and its caption. */
static void filter_preview(OraclesUiDraw *draw, const OraclesHomeNav *nav, const OraclesUiPageStyles *st, const look *lk, const OraclesUiDisplayLayout *l)
{
    const OraclesUiBox *b = &l->filter_preview;
    const float radius = nav->layout == ORACLES_UI_LAYOUT_4_3 ? 6.0f : 4.0f;
    oracles_ui_fill_round_rect(draw, b->x, b->y, b->w, b->h, radius, oracles_ui_rgb(0x000000u));
    const float inset[4] = { b->x + lk->frame_width, b->y + lk->frame_width, b->w - 2.0f * lk->frame_width, b->h - 2.0f * lk->frame_width };
    oracles_ui_draw_inset(draw, inset);
    oracles_ui_stroke_round_rect(draw, b->x, b->y, b->w, b->h, radius, lk->frame_width, oracles_ui_rgba(0xffffffu, DIAGRAM_BORDER_OPACITY));
    wrapped(draw, st->diagram_label, &l->filter_caption, oracles_ui_rgb(DIAGRAM_LABEL));
}

void oracles_ui_display_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent)
{
    const OraclesUiPageStyles *st = oracles_ui_page_styles(nav->layout);
    const look *lk = look_of(nav->layout);
    OraclesUiDisplayTexts t;
    oracles_ui_display_texts(nav, &t);
    OraclesUiDisplayLayout l;
    oracles_ui_layout_display(nav->layout, &t, &l);
    const unsigned row = nav->row;

    head(draw, st, &l.head, t.section, t.over, NULL, accent);
    text(draw, &oracles_ui_display_title_4_3, &l.title, l.title_text, oracles_ui_rgb(OVER));   /* 4:3 */
    if (t.later) wrapped(draw, st->page_note, &l.page_note, oracles_ui_rgb(NOTE_ITALIC));
    if (t.text_page) {
        panel(draw, &l.panel);
        text(draw, st->help, &l.help, t.help, oracles_ui_rgb(HELP));
        text(draw, st->help_note, &l.help_note, t.help_note, oracles_ui_rgb(NOTE_ITALIC));
        text_page(draw, nav, st, lk, &t, &l, accent);
        return;
    }
    /* The window on the screen; in 16:9 the filter's preview in its place while Filter has the focus. */
    const int diagram = !(t.filter_preview && nav->layout != ORACLES_UI_LAYOUT_4_3);
    if (diagram) {
        oracles_ui_fill_round_rect(draw, l.diagram.x, l.diagram.y, l.diagram.w, l.diagram.h, 4.0f, oracles_ui_rgba(DIAGRAM, DIAGRAM_OPACITY));
        oracles_ui_stroke_round_rect(draw, l.diagram.x, l.diagram.y, l.diagram.w, l.diagram.h, 4.0f, 1.0f, oracles_ui_rgba(0xffffffu, DIAGRAM_BORDER_OPACITY));
        const OraclesUiBox *w = &l.diagram_window;
        oracles_ui_fill_rect(draw, w->x, w->y, w->w, w->h, oracles_ui_rgba(0xffffffu, DIAGRAM_WINDOW_OPACITY));
        oracles_ui_stroke_round_rect(draw, w->x, w->y, w->w, w->h, 0.0f, 2.0f, accent);
        text(draw, st->diagram_label, &l.diagram_label, t.diagram_label, oracles_ui_rgb(DIAGRAM_LABEL));
        /* 16:9: Window's note under it, and the line on the window's size. */
        wrapped(draw, st->diagram_label, &l.window_note, oracles_ui_rgb(NOTE));
        wrapped(draw, st->row_note, &l.window_reduced, oracles_ui_rgb(NOTE_ITALIC));
    }
    /* Advanced, in the accent while highlighted, its arrow with it: under the diagram (16:9), or beside it (4:3), framed,
     * the frame in the accent too; on Display's own rows alone. */
    if (l.link < ORACLES_DISPLAY_ROWS) {
        const int on = row == l.link;
        const OraclesUiColor color = on ? accent : oracles_ui_rgb(LABEL);
        const OraclesUiBox *a = &l.advanced_arrow, *b = &l.rows[l.link];
        const float arrow[6] = { a->x, a->y, a->x + a->w, a->y + a->h * 0.5f, a->x, a->y + a->h };
        highlight(draw, lk, b, on);
        if (nav->layout == ORACLES_UI_LAYOUT_4_3)
            oracles_ui_stroke_round_rect(draw, b->x, b->y, b->w, b->h, lk->row_radius, lk->frame_width, on ? accent : oracles_ui_rgba(0xffffffu, ADVANCED_BORDER_OPACITY));
        text(draw, st->row_label, &l.advanced_label, oracles_display_labels[l.link], color);
        oracles_ui_fill_polygon(draw, arrow, 3, color);
    }

    panel(draw, &l.panel);
    /* 4:3: the highlighted row's explanation and its note, in the header. */
    text(draw, st->help, &l.help, t.help, oracles_ui_rgb(HELP));
    text(draw, st->help_note, &l.help_note, t.help_note, oracles_ui_rgb(NOTE_ITALIC));
    if (t.picture) {
        /* The scaling, the margin and the colors in either profile, the margin dimmed while --screen-margin holds it; Far
         * size, Menus & scenes and Unvisited rooms where they change (oracles_display_size_row_live), dimmed with their
         * reason else. */
        const float margin_applied = oracles_display_row_fixed(nav, ORACLES_DISPLAY_MARGIN) ? ROW_NOT_APPLIED : 1.0f;
        const unsigned rows[7] = { ORACLES_DISPLAY_SCALING, ORACLES_DISPLAY_FILTER, ORACLES_DISPLAY_MARGIN, ORACLES_DISPLAY_COLOUR, ORACLES_DISPLAY_FAR_SIZE,
                                   ORACLES_DISPLAY_MENUS, ORACLES_DISPLAY_UNVISITED };
        for (unsigned i = 0; i < 7u; i++) {
            const unsigned r = rows[i];
            if (r == ORACLES_DISPLAY_FAR_SIZE && !t.far_size) continue;
            const float opacity = r == ORACLES_DISPLAY_MARGIN ? margin_applied
                                : r >= ORACLES_DISPLAY_MENUS && r <= ORACLES_DISPLAY_UNVISITED ? (oracles_display_size_row_live(nav, r) ? 1.0f : ROW_NOT_APPLIED)
                                : 1.0f;
            highlight_dimmed(draw, lk, &l.rows[r], r == row, opacity);
            wrapped(draw, st->row_label, &l.labels[r], oracles_ui_rgba(LABEL, opacity));
        }
        wrapped(draw, st->row_text, &l.explanations[5], oracles_ui_rgb(NOTE));
        text(draw, st->row_note, &l.scaling_note, oracles_ui_scaling_note, oracles_ui_rgb(NOTE_ITALIC));
        for (int c = 0; c < 2; c++)
            option(draw, st, lk, &l.scaling[c], 0, oracles_display_scaling_choices[c], t.scaling_sizes[c], c == nav->display.scaling,
                   row == ORACLES_DISPLAY_SCALING, accent, CHOICE_OFF, 1.0f);
        wrapped(draw, st->row_text, &l.filter_explanation, oracles_ui_rgb(NOTE));
        text(draw, st->row_note, &l.filter_note, t.filter_note, oracles_ui_rgb(NOTE_ITALIC));
        filter_value(draw, lk, nav, &t, &l, st, accent);
        if (t.filter_preview) filter_preview(draw, nav, st, lk, &l);
        wrapped(draw, st->row_text, &l.explanations[7], oracles_ui_rgba(NOTE, margin_applied));
        for (int c = 0; c < ORACLES_DISPLAY_MARGINS; c++)
            option(draw, st, lk, &l.margin[c], 0, oracles_display_margin_choices[c], NULL, c == nav->display.margin, row == ORACLES_DISPLAY_MARGIN, accent,
                   CHOICE_OFF, margin_applied);
        wrapped(draw, st->row_text, &l.explanations[0], oracles_ui_rgb(NOTE));
        for (int c = 0; c < 2; c++)
            option(draw, st, lk, &l.colour[c], 0, oracles_display_colour_choices[c], NULL, c == nav->display.colour, row == ORACLES_DISPLAY_COLOUR, accent,
                   CHOICE_OFF, 1.0f);
        const int values[3] = { nav->display.menus, nav->display.far_fixed, oracles_display_unvisited_choice(nav->display.unvisited) };
        static const unsigned size_rows[3] = { ORACLES_DISPLAY_FAR_SIZE, ORACLES_DISPLAY_MENUS, ORACLES_DISPLAY_UNVISITED };
        const char *const *const size_names[3] = { oracles_display_far_size_choices, oracles_display_menus_choices, oracles_display_unvisited_choices };
        const OraclesUiOptionLayout *const size_options[3] = { l.far_size, l.menus, l.unvisited };
        for (unsigned i = t.far_size ? 0u : 1u; i < 3u; i++) {
            const unsigned r = size_rows[i], n = r - ORACLES_DISPLAY_MENUS;
            const float opacity = oracles_display_size_row_live(nav, r) ? 1.0f : ROW_NOT_APPLIED;
            const char *const *names = size_names[i];
            const OraclesUiOptionLayout *options = size_options[i];
            wrapped(draw, st->row_text, &l.size_explanations[n], oracles_ui_rgba(NOTE, opacity));
            text(draw, st->row_note, &l.size_notes[n], t.size_notes[n], oracles_ui_rgba(NOTE_ITALIC, opacity));
            for (int c = 0; c < (r == ORACLES_DISPLAY_UNVISITED ? 3 : 2); c++)
                option(draw, st, lk, &options[c], 0, names[c], NULL, c == values[n], r == row, accent, CHOICE_OFF, opacity);
        }
        return;
    }
    if (t.advanced) {
        /* From a game the core, the transitions and the workers are the running game's: their rows are dimmed, label and
         * all, Vsync and the camera staying live.  The transitions and the camera carry the Enhanced view: dimmed in
         * Faithful. */
        const float core_applied = oracles_display_row_fixed(nav, ORACLES_DISPLAY_CORE) ? ROW_NOT_APPLIED : 1.0f;
        const float workers_applied = oracles_display_row_fixed(nav, ORACLES_DISPLAY_WORKERS) ? ROW_NOT_APPLIED : 1.0f;
        const float vsync_applied = oracles_display_row_fixed(nav, ORACLES_DISPLAY_VSYNC) ? ROW_NOT_APPLIED : 1.0f;
        const float transitions_applied = oracles_display_transitions_apply(nav) && !oracles_display_row_fixed(nav, ORACLES_DISPLAY_TRANSITIONS)
                                          ? 1.0f : ROW_NOT_APPLIED;
        const float camera_applied = oracles_display_camera_applies(nav) ? 1.0f : ROW_NOT_APPLIED;
        const unsigned rows[5] = { ORACLES_DISPLAY_CORE, ORACLES_DISPLAY_TRANSITIONS, ORACLES_DISPLAY_VSYNC, ORACLES_DISPLAY_WORKERS, ORACLES_DISPLAY_CAMERA };
        const float opacities[5] = { core_applied, transitions_applied, vsync_applied, workers_applied, camera_applied };
        for (unsigned i = 0; i < 5u; i++) {
            highlight_dimmed(draw, lk, &l.rows[rows[i]], rows[i] == row, opacities[i]);
            wrapped(draw, st->row_label, &l.labels[rows[i]], oracles_ui_rgba(LABEL, opacities[i]));
        }
        wrapped(draw, st->row_text, &l.explanations[3], oracles_ui_rgba(NOTE, core_applied));
        text(draw, st->row_note, &l.core_note, t.core_note, oracles_ui_rgba(NOTE_ITALIC, core_applied));
        wrapped(draw, st->row_text, &l.explanations[1], oracles_ui_rgba(NOTE, transitions_applied));
        text(draw, st->row_note, &l.transitions_note, t.transitions_note, oracles_ui_rgba(NOTE_ITALIC, transitions_applied));
        wrapped(draw, st->row_text, &l.explanations[2], oracles_ui_rgba(NOTE, vsync_applied));
        text(draw, st->row_note, &l.vsync_note, t.vsync_note, oracles_ui_rgba(NOTE_ITALIC, vsync_applied));
        wrapped(draw, st->row_text, &l.explanations[4], oracles_ui_rgba(NOTE, workers_applied));
        text(draw, st->row_note, &l.workers_note, t.workers_note, oracles_ui_rgba(NOTE_ITALIC, workers_applied));
        for (int c = 0; c < 2; c++)
            option(draw, st, lk, &l.core[c], 0, oracles_display_core_choices[c], NULL, c == nav->display.core, row == ORACLES_DISPLAY_CORE, accent,
                   CHOICE_OFF, core_applied);
        for (int c = 0; c < 2; c++)
            option(draw, st, lk, &l.transitions[c], 0, oracles_ui_transition_choices[c], NULL, c == nav->display.transitions,
                   row == ORACLES_DISPLAY_TRANSITIONS, accent, CHOICE_OFF, transitions_applied);
        for (int c = 0; c < 3; c++)
            option(draw, st, lk, &l.vsync[c], 0, oracles_display_vsync_choices[c], NULL, c == nav->display.vsync, row == ORACLES_DISPLAY_VSYNC, accent,
                   CHOICE_OFF, vsync_applied);
        for (int c = 0; c < 3; c++)
            option(draw, st, lk, &l.workers[c], 0, t.workers_names[c], NULL, c == nav->display.workers, row == ORACLES_DISPLAY_WORKERS, accent,
                   CHOICE_OFF, workers_applied);
        wrapped(draw, st->row_text, &l.camera_explanation, oracles_ui_rgba(NOTE, camera_applied));
        text(draw, st->row_note, &l.camera_note, t.camera_note, oracles_ui_rgba(NOTE_ITALIC, camera_applied));
        for (int c = 0; c < 2; c++)
            option(draw, st, lk, &l.camera[c], 0, oracles_display_camera_choices[c], NULL, c == oracles_display_camera(nav), row == ORACLES_DISPLAY_CAMERA,
                   accent, CHOICE_OFF, camera_applied);
        return;
    }
    /* View and the HUD carry the Enhanced view: in Faithful their rows are dimmed, label and all, the HUD's from the pause
     * by the running game's profile, which takes it at once; the Text link where none of its rows applies; Window at one
     * size, Quality in Faithful, and Profile, Quality and View from a game, which keeps them. */
    const float applied = oracles_display_transitions_apply(nav) && !oracles_display_row_fixed(nav, ORACLES_DISPLAY_VIEW) ? 1.0f : ROW_NOT_APPLIED;
    const float profile = oracles_display_row_fixed(nav, ORACLES_DISPLAY_PROFILE) ? ROW_NOT_APPLIED : 1.0f;
    const float hud = oracles_display_hud_applies(nav) ? 1.0f : ROW_NOT_APPLIED;
    const float text_applied = oracles_display_text_link_applies(nav) ? 1.0f : ROW_NOT_APPLIED;
    const float window = t.window_one ? ROW_NOT_APPLIED : 1.0f;
    const float quality = oracles_display_quality_applies(nav) ? 1.0f : ROW_NOT_APPLIED;
    /* 4:3: a page taller than its room scrolled in it, the highlight kept in sight. */
    const int scrolled = l.rows_room.h > 0.0f && l.scroll_max > 0.0f;
    int saved_clip = 0;
    SDL_Rect clip;
    if (scrolled) {
        oracles_ui_draw_clip_rect(draw, &saved_clip, &clip);
        oracles_ui_draw_clip(draw, l.rows_room.x - 16.0f, l.rows_room.y, l.rows_room.w + 32.0f, l.rows_room.h);
        oracles_ui_draw_shift(draw, -l.scroll);
    }
    const unsigned own[8] = { ORACLES_DISPLAY_PROFILE, ORACLES_DISPLAY_QUALITY, ORACLES_DISPLAY_WINDOW, ORACLES_DISPLAY_VIEW, ORACLES_DISPLAY_HUD,
                              ORACLES_DISPLAY_PANELS, ORACLES_DISPLAY_TEXT, ORACLES_DISPLAY_PICTURE };
    for (unsigned i = 0; i < 8u; i++) {
        const unsigned r = own[i];
        if (r == ORACLES_DISPLAY_PANELS && !t.floating) continue;
        const float opacity = r == ORACLES_DISPLAY_HUD || r == ORACLES_DISPLAY_PANELS ? hud : r == ORACLES_DISPLAY_VIEW ? applied : r == ORACLES_DISPLAY_WINDOW ? window
                            : r == ORACLES_DISPLAY_QUALITY ? quality : r == ORACLES_DISPLAY_TEXT ? text_applied
                            : r == ORACLES_DISPLAY_PROFILE ? profile : 1.0f;
        highlight_dimmed(draw, lk, &l.rows[r], r == row, opacity);
        wrapped(draw, st->row_label, &l.labels[r], oracles_ui_rgba(LABEL, opacity));
    }
    for (int p = 0; p < ORACLES_PROFILES; p++)
        option(draw, st, lk, &l.profiles[p], 1, oracles_profile_names[p], t.profile_sizes[p], p == (int)nav->display.profile,
               row == ORACLES_DISPLAY_PROFILE, accent, OPTION_OFF, profile);
    text(draw, st->row_text, &l.profile_note, t.profile_note, oracles_ui_rgba(NOTE, profile));
    /* The qualities, Custom only while it is in effect, dashed and fainter; the one in effect and its note (16:9). */
    const int in_effect = oracles_display_quality(nav);
    for (int q = 0; q <= ORACLES_DISPLAY_QUALITIES; q++) {
        const OraclesUiOptionLayout *o = &l.qualities[q];
        const int on = q == in_effect, focused = row == ORACLES_DISPLAY_QUALITY;
        if (q == ORACLES_DISPLAY_QUALITY_CUSTOM && !on) continue;
        const float opacity = quality * (q == ORACLES_DISPLAY_QUALITY_CUSTOM ? CUSTOM_OPACITY : 1.0f);
        const OraclesUiColor frame = on ? (focused ? accent : oracles_ui_rgba(0xffffffu, FRAME_ON_OPACITY)) : oracles_ui_rgba(0xffffffu, FRAME_OFF_OPACITY);
        if (q == ORACLES_DISPLAY_QUALITY_CUSTOM)
            oracles_ui_stroke_dashed_rect(draw, o->box.x, o->box.y, o->box.w, o->box.h, lk->choice_radius, lk->frame_width, oracles_ui_fade(frame, opacity));
        else oracles_ui_stroke_round_rect(draw, o->box.x, o->box.y, o->box.w, o->box.h, lk->choice_radius, lk->frame_width, oracles_ui_fade(frame, opacity));
        const OraclesUiColor color = on ? (focused ? accent : oracles_ui_rgb(TEXT)) : oracles_ui_rgb(CHOICE_OFF);
        text(draw, st->choice, &o->name, oracles_display_quality_names[q], oracles_ui_fade(color, opacity));
        text(draw, st->quality_sets, &o->size, t.quality_sets[q], oracles_ui_rgba(PATH, opacity));
    }
    text(draw, st->row_text, &l.quality_text, t.quality_text, oracles_ui_rgba(NOTE, quality));
    text(draw, st->row_note, &l.quality_note, t.quality_note, oracles_ui_rgba(NOTE_ITALIC, quality));
    /* The windows the pause refuses, dimmed. */
    for (int i = 0; i < 4; i++)
        option(draw, st, lk, &l.windows[i], 1, t.window_names[i], t.window_sizes[i], i == nav->display.window,
               row == ORACLES_DISPLAY_WINDOW, accent, OPTION_OFF, nav->display.windows_refused >> i & 1u ? ROW_NOT_APPLIED : window);
    text(draw, st->row_note, &l.window_one, oracles_display_one_size_note, oracles_ui_rgba(NOTE_ITALIC, window));
    wrapped(draw, st->row_text, &l.view_explanation, oracles_ui_rgba(NOTE, applied));
    text(draw, st->row_note, &l.view_note, t.view_note, oracles_ui_rgba(NOTE_ITALIC, applied));
    for (int v = 0; v < 3; v++)
        option(draw, st, lk, &l.views[v], 0, oracles_display_view_names[v], t.view_sizes[v], v == nav->display.view,
               row == ORACLES_DISPLAY_VIEW, accent, CHOICE_OFF, applied);
    wrapped(draw, st->row_text, &l.explanations[6], oracles_ui_rgba(NOTE, hud));
    const int four_three = nav->layout == ORACLES_UI_LAYOUT_4_3;
    for (int c = 0; c < ORACLES_DISPLAY_HUDS; c++)
        option(draw, st, lk, &l.hud[c], 0, four_three ? oracles_display_hud_choices[c] : oracles_display_hud_short[c], NULL, c == nav->display.hud,
               row == ORACLES_DISPLAY_HUD, accent, CHOICE_OFF, hud);
    /* Floating's Panels: in 16:9 its line under HUD's choices, its label and its choices smaller. */
    if (t.floating) {
        OraclesUiPageStyles panels = *st;
        if (!four_three) panels.choice = &oracles_ui_panels_choice;
        text(draw, &oracles_ui_diagram_label, &l.panels_label, oracles_display_labels[ORACLES_DISPLAY_PANELS], oracles_ui_rgba(LABEL, hud));
        for (int c = 0; c < 2; c++)
            option(draw, &panels, lk, &l.panels[c], 0, oracles_display_panels_choices[c], NULL, c == nav->display.hud_panel, row == ORACLES_DISPLAY_PANELS,
                   accent, CHOICE_OFF, hud);
    }
    /* The Text and Picture links: each one's summary and its arrow, in the accent while highlighted. */
    wrapped(draw, st->row_text, &l.explanations[8], oracles_ui_rgba(NOTE, text_applied));
    link(draw, st, &l.text_summary, &l.text_arrow, t.text_summary, oracles_ui_fade(row == ORACLES_DISPLAY_TEXT ? accent : oracles_ui_rgb(LABEL), text_applied));
    wrapped(draw, st->row_text, &l.picture_explanation, oracles_ui_rgb(NOTE));
    link(draw, st, &l.picture_summary, &l.picture_arrow, t.picture_summary, row == ORACLES_DISPLAY_PICTURE ? accent : oracles_ui_rgb(LABEL));
    if (scrolled) {
        oracles_ui_draw_shift(draw, l.scroll);
        /* The edges more rows lie past fade into the page, as the mockup's mask does: the scene's ground over a band
         * of ROWS_FADE, opaque at the edge. */
        const OraclesUiBox *room = &l.rows_room;
        for (int f = 0; f < (int)ROWS_FADE; f++) {
            const float opacity = 1.0f - ((float)f + 0.5f) / ROWS_FADE;
            if (l.scroll > 0.0f) oracles_ui_fill_rect(draw, room->x - 16.0f, room->y + (float)f, room->w + 32.0f, 1.0f, oracles_ui_rgba(SCENE_GROUND, opacity));
            if (l.scroll < l.scroll_max)
                oracles_ui_fill_rect(draw, room->x - 16.0f, room->y + room->h - 1.0f - (float)f, room->w + 32.0f, 1.0f, oracles_ui_rgba(SCENE_GROUND, opacity));
        }
        oracles_ui_draw_restore_clip(draw, saved_clip, &clip);
    }
}

int oracles_ui_display_hit(const OraclesHomeNav *nav, float x, float y, int *option)
{
    OraclesUiDisplayTexts t;
    oracles_ui_display_texts(nav, &t);
    OraclesUiDisplayLayout l;
    oracles_ui_layout_display(nav->layout, &t, &l);
    *option = -1;
    if (t.text_page) {
        for (int c = 0; c < 2; c++) if (inside(&l.text[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_LETTERS; }
        for (int c = 0; c < 2; c++) if (inside(&l.text_box[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_TEXT_BOX; }
        for (int c = 0; c < ORACLES_DISPLAY_TEXT_SIZES; c++) if (inside(&l.text_size[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_TEXT_SIZE; }
        for (int c = 0; c < 2; c++) if (inside(&l.text_speed[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_TEXT_SPEED; }
        for (int r = ORACLES_DISPLAY_LETTERS; r <= ORACLES_DISPLAY_TEXT_SPEED; r++) if (inside(&l.rows[r], x, y)) return r;
        return -1;
    }
    if (t.picture) {
        for (int c = 0; c < 2; c++) if (inside(&l.scaling[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_SCALING; }
        for (int c = 0; c < ORACLES_DISPLAY_MARGINS; c++) if (inside(&l.margin[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_MARGIN; }
        for (int c = 0; c < 2; c++) if (inside(&l.colour[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_COLOUR; }
        for (int c = 0; t.far_size && c < 2; c++) if (inside(&l.far_size[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_FAR_SIZE; }
        for (int c = 0; c < 2; c++) if (inside(&l.menus[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_MENUS; }
        for (int c = 0; c < 3; c++) if (inside(&l.unvisited[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_UNVISITED; }
        if (inside(&l.filter_prev, x, y)) { *option = 0; return ORACLES_DISPLAY_FILTER; }
        if (inside(&l.filter_next, x, y)) { *option = 1; return ORACLES_DISPLAY_FILTER; }
        const unsigned rows[7] = { ORACLES_DISPLAY_SCALING, ORACLES_DISPLAY_FILTER, ORACLES_DISPLAY_MARGIN, ORACLES_DISPLAY_COLOUR, ORACLES_DISPLAY_FAR_SIZE,
                                   ORACLES_DISPLAY_MENUS, ORACLES_DISPLAY_UNVISITED };
        for (int i = 0; i < 7; i++) if (inside(&l.rows[rows[i]], x, y)) return (int)rows[i];
        return -1;
    }
    if (t.advanced) {
        for (int c = 0; c < 2; c++) if (inside(&l.core[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_CORE; }
        for (int c = 0; c < 2; c++) if (inside(&l.transitions[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_TRANSITIONS; }
        for (int c = 0; c < 3; c++) if (inside(&l.vsync[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_VSYNC; }
        for (int c = 0; c < 3; c++) if (inside(&l.workers[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_WORKERS; }
        for (int c = 0; c < 2; c++) if (inside(&l.camera[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_CAMERA; }
        const unsigned rows[5] = { ORACLES_DISPLAY_CORE, ORACLES_DISPLAY_TRANSITIONS, ORACLES_DISPLAY_VSYNC, ORACLES_DISPLAY_WORKERS, ORACLES_DISPLAY_CAMERA };
        for (int i = 0; i < 5; i++) if (inside(&l.rows[rows[i]], x, y)) return (int)rows[i];
        return -1;
    }
    /* 4:3: the rows of a page taller than its room, scrolled in it, reached there alone; Advanced above it. */
    int faded = 0;
    if (l.scroll_max > 0.0f) {
        if (inside(&l.rows[l.link < ORACLES_DISPLAY_ROWS ? l.link : ORACLES_DISPLAY_ADVANCED], x, y)) return ORACLES_DISPLAY_ADVANCED;
        if (y < l.rows_room.y || y >= l.rows_room.y + l.rows_room.h) return -1;
        /* Under a faded edge, a row is found but marked so (ORACLES_UI_DISPLAY_FADED). */
        faded = (l.scroll > 0.0f && y < l.rows_room.y + ROWS_FADE) || (l.scroll < l.scroll_max && y >= l.rows_room.y + l.rows_room.h - ROWS_FADE);
        y += l.scroll;
    }
    if (faded) {
        for (int r = 0; r < ORACLES_DISPLAY_ROWS; r++)
            if (r != (int)l.link && oracles_display_row_shown(nav, (unsigned)r) && inside(&l.rows[r], x, y)) { *option = ORACLES_UI_DISPLAY_FADED; return r; }
        return -1;
    }
    for (int p = 0; p < ORACLES_PROFILES; p++) if (inside(&l.profiles[p].box, x, y)) { *option = p; return ORACLES_DISPLAY_PROFILE; }
    /* Custom's place is the row's: it is never chosen. */
    for (int q = 0; q < ORACLES_DISPLAY_QUALITIES; q++) if (inside(&l.qualities[q].box, x, y)) { *option = q; return ORACLES_DISPLAY_QUALITY; }
    for (int i = 0; i < 4; i++) if (inside(&l.windows[i].box, x, y)) { *option = i; return ORACLES_DISPLAY_WINDOW; }
    for (int v = 0; v < 3; v++) if (inside(&l.views[v].box, x, y)) { *option = v; return ORACLES_DISPLAY_VIEW; }
    for (int c = 0; c < ORACLES_DISPLAY_HUDS; c++) if (inside(&l.hud[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_HUD; }
    for (int c = 0; t.floating && c < 2; c++) if (inside(&l.panels[c].box, x, y)) { *option = c; return ORACLES_DISPLAY_PANELS; }
    if (t.floating && inside(&l.rows[ORACLES_DISPLAY_PANELS], x, y)) return ORACLES_DISPLAY_PANELS;
    for (int r = 0; r <= ORACLES_DISPLAY_ADVANCED; r++) if (inside(&l.rows[r], x, y)) return r;
    if (inside(&l.rows[ORACLES_DISPLAY_PICTURE], x, y)) return ORACLES_DISPLAY_PICTURE;
    return -1;
}

/* ---- Mods ------------------------------------------------------------------------ */

void oracles_ui_mods_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent)
{
    const OraclesUiPageStyles *st = oracles_ui_page_styles(nav->layout);
    const look *lk = look_of(nav->layout);
    OraclesUiModsLayout l;
    oracles_ui_layout_mods(nav->layout, nav, &l);
    const OraclesHomeMods *mods = oracles_mods_list(nav);
    const unsigned row = nav->row, play_row = oracles_mods_row_play(nav);

    head(draw, st, &l.head, "Mods", oracles_home_over(oracles_home_hero(nav)), l.state, accent);
    panel(draw, &l.panel);
    text(draw, st->row_label, &l.label_folder, "Folder", oracles_ui_rgb(LABEL));
    text(draw, st->row_label, &l.label_mods, "Mods", oracles_ui_rgb(LABEL));

    highlight(draw, lk, &l.folder_row, row == ORACLES_MODS_ROW_FOLDER);
    if (l.folder_lines.count) wrapped(draw, st->row_path, &l.folder_lines, oracles_ui_rgb(MODS_PATH));
    else text(draw, st->row_path, &l.folder_path, l.folder_text, oracles_ui_rgb(MODS_PATH));
    text(draw, st->row_text, &l.folder_line, oracles_mods_folder_line(nav), oracles_ui_rgb(NOTE));
    const OraclesUiColor folder_button = row == ORACLES_MODS_ROW_FOLDER ? accent : oracles_ui_rgb(BUTTON);
    text(draw, st->row_button, &l.folder_button, "Open folder", folder_button);
    text(draw, st->row_button, &l.folder_button_dot, oracles_ui_item_dot, folder_button);

    wrapped(draw, st->mods_empty, &l.empty, oracles_ui_rgb(MODS_EMPTY));
    for (unsigned i = 0; i < l.shown; i++) {
        const OraclesUiModLayout *m = &l.mods[i];
        const OraclesHomeMod *mod = &mods->mods[m->index];
        const int on = mod->active && !mod->refused;
        const float opacity = mod->refused ? MODS_REFUSED : 1.0f;
        highlight(draw, lk, &m->row, row == m->index + 1u);
        /* The switch: a pill, its knob at the right when on. */
        const OraclesUiBox *t = &m->toggle;
        oracles_ui_fill_round_rect(draw, t->x, t->y, t->w, t->h, t->h * 0.5f, oracles_ui_fade(on ? accent : oracles_ui_rgb(MODS_SWITCH_OFF), opacity));
        oracles_ui_stroke_round_rect(draw, t->x, t->y, t->w, t->h, t->h * 0.5f, 1.0f,
                                     oracles_ui_fade(on ? accent : oracles_ui_rgba(0xffffffu, MODS_SWITCH_OFF_BORDER_OPACITY), opacity));
        oracles_ui_fill_round_rect(draw, m->knob.x, m->knob.y, m->knob.w, m->knob.h, m->knob.w * 0.5f, oracles_ui_rgba(on ? MODS_KNOB_ON : MODS_KNOB_OFF, opacity));
        text(draw, st->row_title, &m->name, m->name_text, oracles_ui_rgb(mod->refused ? LABEL : TEXT));
        text(draw, st->mod_games, &m->games, m->games_text, oracles_ui_rgb(PATH));
        dot(draw, &m->dot, mod->refused ? ORACLES_TONE_ERROR : on ? ORACLES_TONE_OK : ORACLES_TONE_NONE);
        text(draw, st->row_text, &m->line, m->line_text, oracles_ui_rgb(mod->refused ? tone_texts[ORACLES_TONE_ERROR] : NOTE));
    }
    text(draw, st->row_text, &l.count, l.count_text, oracles_ui_rgb(NOTE));
    wrapped(draw, st->mods_note, &l.note, oracles_ui_rgb(NOTE_ITALIC));

    /* Android's Save row: Export save with mods… in the accent while highlighted, greyed while it does not act; its note
     * or its status. */
    if (l.save_row.h > 0.0f) {
        const unsigned save_row = oracles_mods_row_save(nav);
        text(draw, st->row_label, &l.label_save, "Save", oracles_ui_rgb(LABEL));
        highlight(draw, lk, &l.save_row, row == save_row);
        const OraclesUiColor save = !oracles_mods_save_live(nav) ? oracles_ui_rgb(row == save_row ? BUTTON_DISABLED_FOCUSED : BUTTON_DISABLED)
                                    : row == save_row ? accent : oracles_ui_rgb(BUTTON);
        text(draw, st->row_button, &l.save_button, oracles_mods_save_action, save);
        text(draw, st->row_button, &l.save_button_dot, oracles_ui_item_dot, save);
        wrapped(draw, nav->layout == ORACLES_UI_LAYOUT_4_3 ? st->page_note : st->row_note, &l.save_note, oracles_ui_rgb(NOTE_ITALIC));
        if (l.save_status_text) {
            dot(draw, &l.save_dot, l.save_tone);
            wrapped(draw, st->row_status, &l.save_status, oracles_ui_rgb(tone_texts[l.save_tone]));
        }
    }

    const int playable = !l.play_note_text[0];
    highlight(draw, lk, &l.play_row, row == play_row);
    const OraclesUiColor play = !playable ? oracles_ui_rgb(row == play_row ? BUTTON_DISABLED_FOCUSED : BUTTON_DISABLED)
                                          : row == play_row ? accent : oracles_ui_rgb(BUTTON);
    text(draw, st->row_text, &l.play_note, l.play_note_text, oracles_ui_rgb(NOTE));
    text(draw, st->play, &l.play, "Play", play);
    text(draw, st->play, &l.play_dot, oracles_ui_item_dot, play);
}

int oracles_ui_mods_hit(const OraclesHomeNav *nav, float x, float y)
{
    OraclesUiModsLayout l;
    oracles_ui_layout_mods(nav->layout, nav, &l);
    if (inside(&l.folder_row, x, y)) return (int)ORACLES_MODS_ROW_FOLDER;
    for (unsigned i = 0; i < l.shown; i++) if (inside(&l.mods[i].row, x, y)) return (int)l.mods[i].index + 1;
    if (l.save_row.h > 0.0f && inside(&l.save_row, x, y)) return (int)oracles_mods_row_save(nav);
    if (inside(&l.play_row, x, y)) return (int)oracles_mods_row_play(nav);
    return -1;
}

/* A list page's second note under the first, in the warning's tone. */
static void second_note(OraclesUiDraw *draw, OraclesUiLayout layout, const OraclesUiWrapped *note)
{
    wrapped(draw, layout == ORACLES_UI_LAYOUT_4_3 ? &oracles_ui_page_credit_4_3 : &oracles_ui_row_text, note, oracles_ui_rgb(tone_texts[ORACLES_TONE_WARN]));
}

#ifdef ORACLES_RETROACHIEVEMENTS
/* An action and its dot, in the accent while its row is highlighted. */
static void action(OraclesUiDraw *draw, const OraclesUiPageStyles *st, const OraclesUiLine *label, const OraclesUiLine *dot_line, const char *text_,
                   int focused, OraclesUiColor accent)
{
    const OraclesUiColor color = focused ? accent : oracles_ui_rgb(BUTTON);
    text(draw, st->row_button, label, text_, color);
    text(draw, st->row_button, dot_line, oracles_ui_item_dot, color);
}
#endif

/* ---- Cheats ---------------------------------------------------------------------- */

void oracles_ui_cheats_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent)
{
    const OraclesUiPageStyles *st = oracles_ui_page_styles(nav->layout);
    const look *lk = look_of(nav->layout);
    OraclesUiCheatsLayout l;
    oracles_ui_layout_cheats(nav->layout, nav, &l);
    const unsigned row = nav->row;
    head(draw, st, &l.head, "Cheats", oracles_home_over(oracles_home_hero(nav)), NULL, accent);
    wrapped(draw, nav->layout == ORACLES_UI_LAYOUT_4_3 ? st->page_note : st->row_text, &l.note,
            oracles_ui_rgb(nav->layout == ORACLES_UI_LAYOUT_4_3 ? NOTE_ITALIC : NOTE));
    second_note(draw, nav->layout, &l.note2);
    panel(draw, &l.panel);
    /* Every row dimmed, label and all, while --cheats holds them. */
    for (unsigned r = 0; r < ORACLES_CHEATS_ROWS; r++) {
        const float opacity = oracles_cheats_row_live(nav, r) ? 1.0f : ROW_NOT_APPLIED;
        highlight_dimmed(draw, lk, &l.rows[r], r == row, opacity);
        text(draw, st->row_label, &l.labels[r], oracles_cheats_labels[r], oracles_ui_rgba(LABEL, opacity));
        wrapped(draw, st->row_text, &l.explanations[r], oracles_ui_rgba(NOTE, opacity));
        const int on = oracles_cheats_on(nav, r);
        for (int c = 0; c < 2; c++)
            option(draw, st, lk, &l.choices[r][c], 0, c ? "On" : "Off", NULL, c == on, r == row, accent, CHOICE_OFF, opacity);
    }
    /* 4:3: the highlighted row's explanation and its note, under the rows. */
    const unsigned shown = row < ORACLES_CHEATS_ROWS ? row : 0u;
    text(draw, st->help, &l.help, oracles_cheats_texts[shown], oracles_ui_rgb(HELP));
    text(draw, st->help_note, &l.help_note, oracles_cheats_row_note(nav, shown), oracles_ui_rgb(NOTE_ITALIC));
}

int oracles_ui_cheats_hit(const OraclesHomeNav *nav, float x, float y, int *option)
{
    OraclesUiCheatsLayout l;
    oracles_ui_layout_cheats(nav->layout, nav, &l);
    *option = -1;
    for (int r = 0; r < ORACLES_CHEATS_ROWS; r++)
        for (int c = 0; c < 2; c++) if (inside(&l.choices[r][c].box, x, y)) { *option = c; return r; }
    for (int r = 0; r < ORACLES_CHEATS_ROWS; r++) if (inside(&l.rows[r], x, y)) return r;
    return -1;
}

/* ---- Settings (About) ------------------------------------------------------------ */

#define ABOUT_RULE 0xffffffu
#define ABOUT_RULE_OPACITY 0.08f

/* Settings' About block: its rule, its highlight, its label, the version, the source and the cores' credits, and
 * Licences, its dot and its line, Licences in the accent while highlighted. */
static void about_block(OraclesUiDraw *draw, const OraclesUiPageStyles *st, const look *lk, OraclesUiLayout layout, const struct OraclesUiAboutBlock *b,
                        int focused, OraclesUiColor accent)
{
    const int four_three = layout == ORACLES_UI_LAYOUT_4_3;
    oracles_ui_fill_rect(draw, b->rule.x, b->rule.y, b->rule.w, b->rule.h, oracles_ui_rgba(ABOUT_RULE, ABOUT_RULE_OPACITY));
    highlight(draw, lk, &b->row, focused);
    text(draw, st->row_label, &b->label, oracles_about_block_label, oracles_ui_rgb(LABEL));
    text(draw, st->row_title, &b->title, b->title_text, oracles_ui_rgb(TEXT));
    const OraclesUiTextStyle *lines = four_three ? &oracles_ui_row_text_4_3 : st->row_text;
    text(draw, lines, &b->source, b->source_text, oracles_ui_rgb(NOTE));
    text(draw, lines, &b->credits, oracles_about_credits, oracles_ui_rgb(NOTE));
    const OraclesUiColor link = focused ? accent : oracles_ui_rgb(BUTTON);
    text(draw, st->row_button, &b->licences, oracles_about_licences_label, link);
    text(draw, st->row_button, &b->dot, oracles_ui_item_dot, link);
    text(draw, lines, &b->list, oracles_about_licences_line, oracles_ui_rgb(NOTE));
}

void oracles_ui_about_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent)
{
    const OraclesUiPageStyles *st = oracles_ui_page_styles(nav->layout);
    const look *lk = look_of(nav->layout);
    OraclesUiCheatsLayout l;
    oracles_ui_layout_about(nav->layout, nav, ORACLES_VERSION, ORACLES_REPOSITORY, &l);
    head(draw, st, &l.head, "Settings", nav->layout == ORACLES_UI_LAYOUT_4_3 ? NULL : "The Oracles", NULL, accent);
    panel(draw, &l.panel);
    /* Update check, Portable mode where it can be, its choices faded while it copies; the highlighted row's help under
     * them in 4:3; the About block. */
    const unsigned places = oracles_about_rows(nav), row = nav->row < places ? nav->row : 0u;
    OraclesTone tone;
    const char *status = oracles_about_portable_status(nav, &tone);
    const int copying = nav->portable_status == ORACLES_PORTABLE_STATUS_COPYING;
    for (unsigned r = 0; r + 1u < places; r++) {
        highlight(draw, lk, &l.rows[r], r == row);
        wrapped(draw, st->row_text, &l.explanations[r], oracles_ui_rgb(NOTE));
        const int portable = oracles_about_row_kind(nav, r) == ORACLES_ABOUT_ROW_PORTABLE;
        const int chosen = portable ? nav->portable != 0 : nav->update_check == 0;   /* the index of the choice on */
        text(draw, st->row_label, &l.labels[r], portable ? oracles_about_portable_label : oracles_about_label, oracles_ui_rgb(LABEL));
        for (int c = 0; c < 2; c++)
            option(draw, st, lk, &l.choices[r][c], 0, (portable ? oracles_about_portable_choices : oracles_about_choices)[c], NULL, c == chosen, r == row,
                   accent, CHOICE_OFF, portable && copying ? ROW_NOT_APPLIED : 1.0f);
    }
    const int four_three = nav->layout == ORACLES_UI_LAYOUT_4_3;
    wrapped(draw, four_three ? &oracles_ui_row_text_4_3 : st->row_text, &l.hint, oracles_ui_rgb(NOTE));
    if (status) {
        dot(draw, &l.status_dot, tone);
        wrapped(draw, four_three ? &oracles_ui_about_status_4_3 : &oracles_ui_about_status, &l.status, oracles_ui_rgb(tone_texts[tone]));
    }
    text(draw, st->help, &l.help, l.help_text, oracles_ui_rgb(HELP));
    text(draw, st->help_note, &l.help_note, l.help_note_text, oracles_ui_rgb(NOTE_ITALIC));
    about_block(draw, st, lk, nav->layout, &l.about, row + 1u == places, accent);
}

int oracles_ui_about_hit(const OraclesHomeNav *nav, float x, float y, int *option)
{
    OraclesUiCheatsLayout l;
    oracles_ui_layout_about(nav->layout, nav, ORACLES_VERSION, ORACLES_REPOSITORY, &l);
    *option = -1;
    if (nav->portable_status == ORACLES_PORTABLE_STATUS_COPYING) return -1;   /* the page takes no input meanwhile */
    const int places = (int)oracles_about_rows(nav);
    for (int r = 0; r + 1 < places; r++)
        for (int c = 0; c < 2; c++) if (inside(&l.choices[r][c].box, x, y)) { *option = c; return r; }
    for (int r = 0; r + 1 < places; r++) if (inside(&l.rows[r], x, y)) return r;
    if (inside(&l.about.link, x, y)) { *option = 2; return places - 1; }
    if (inside(&l.about.row, x, y)) return places - 1;
    return -1;
}

#ifdef ORACLES_RETROACHIEVEMENTS
/* ---- Achievements' page ------------------------------------------------------------------------------------ */

void oracles_ui_achievements_page_draw(OraclesUiDraw *draw, const OraclesHomeNav *nav, OraclesUiColor accent, double now_ms)
{
    const OraclesUiPageStyles *st = oracles_ui_page_styles(nav->layout);
    const look *lk = look_of(nav->layout);
    const int four_three = nav->layout == ORACLES_UI_LAYOUT_4_3;
    const OraclesHomeAchievements *a = &nav->achievements;
    static OraclesUiAchievementsPageLayout page;
    oracles_ui_layout_achievements_page(nav->layout, nav, &page);
    const OraclesUiCheatsLayout *l = &page.page;
    /* 16:9: the column shows the highlighted achievement's detail under the page's name. */
    if (oracles_achievements_detail_column(nav)) text(draw, st->section, &l->head.section, "Achievements", accent);
    else {
        head(draw, st, &l->head, "Achievements", oracles_home_over(oracles_home_hero(nav)), NULL, accent);
        wrapped(draw, four_three ? st->page_note : st->row_text, &l->note, oracles_ui_rgb(four_three ? NOTE_ITALIC : NOTE));
    }
    panel(draw, &l->panel);
    int saved;
    SDL_Rect clip;
    oracles_ui_draw_clip_rect(draw, &saved, &clip);
    if (page.list) oracles_ui_draw_clip(draw, page.clip.x, page.clip.y, page.clip.w, page.clip.h);
    const unsigned row = nav->row;
    for (unsigned r = 0; r < ORACLES_ACHIEVEMENTS_ROWS; r++) {
        highlight(draw, lk, &l->rows[r], r == row);
        text(draw, st->row_label, &l->labels[r], oracles_achievements_row_label(r), oracles_ui_rgb(LABEL));
    }
    const int focused_on = row == ORACLES_ACHIEVEMENTS_ROW_ON;
    wrapped(draw, st->row_text, &l->explanations[ORACLES_ACHIEVEMENTS_ROW_ON], oracles_ui_rgb(NOTE));
    for (int c = 0; c < 2; c++)
        option(draw, st, lk, &l->choices[ORACLES_ACHIEVEMENTS_ROW_ON][c], 0, c ? "On" : "Off", NULL, c == (a->on != 0), focused_on, accent, CHOICE_OFF, 1.0f);
    /* The account: its name as the server spells it, brighter, or "Not logged in"; its action. */
    const OraclesUiColor account = oracles_ui_rgb(a->user[0] ? TEXT : NOTE);
    wrapped(draw, a->user[0] ? st->row_title : st->row_text, &l->explanations[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT], account);
    text(draw, &oracles_ui_row_title_4_3, &l->values[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT], a->user[0] ? a->user : "Not logged in", account);
    action(draw, st, &l->actions[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT], &l->action_dots[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT], a->user[0] ? "Log out" : "Log in\xe2\x80\xa6",
           row == ORACLES_ACHIEVEMENTS_ROW_ACCOUNT, accent);
    wrapped(draw, st->row_text, &l->explanations[ORACLES_ACHIEVEMENTS_ROW_PRIVACY], oracles_ui_rgb(NOTE));
    action(draw, st, &l->actions[ORACLES_ACHIEVEMENTS_ROW_PRIVACY], &l->action_dots[ORACLES_ACHIEVEMENTS_ROW_PRIVACY], "Player's guide",
           row == ORACLES_ACHIEVEMENTS_ROW_PRIVACY, accent);
    wrapped(draw, st->row_text, &l->explanations[ORACLES_ACHIEVEMENTS_ROW_SOUND], oracles_ui_rgb(NOTE));
    for (int c = 0; c < 2; c++)
        option(draw, st, lk, &l->choices[ORACLES_ACHIEVEMENTS_ROW_SOUND][c], 0, c ? "Off" : "On", NULL, c == (a->sound == 0), row == ORACLES_ACHIEVEMENTS_ROW_SOUND,
               accent, CHOICE_OFF, 1.0f);
    if (l->below.h > 0.0f) {
        oracles_ui_fill_rect(draw, l->below_rule.x, l->below_rule.y, l->below_rule.w, l->below_rule.h, oracles_ui_rgba(ABOUT_RULE, ABOUT_RULE_OPACITY));
        text(draw, four_three ? &oracles_ui_row_text_4_3 : st->row_text, &l->below, oracles_achievements_page_line(nav), oracles_ui_rgb(NOTE));
    }
    char status[96];
    OraclesTone dot_tone, tone;
    if (oracles_achievements_account_status(nav, status, sizeof status, &dot_tone, &tone)) {
        dot(draw, &l->status_dot, dot_tone);
        wrapped(draw, four_three ? &oracles_ui_about_status_4_3 : &oracles_ui_about_status, &l->status, oracles_ui_rgb(tone_texts[tone]));
    }
    if (page.list) oracles_ui_achievements_page_list_draw(draw, nav, &page, accent, now_ms);
    oracles_ui_draw_restore_clip(draw, saved, &clip);
    const char *help, *note;
    oracles_achievements_help(nav, &help, &note);
    text(draw, st->help, &l->help, page.help[0] ? page.help : help, oracles_ui_rgb(HELP));
    text(draw, st->help_note, &l->help_note, page.help[0] ? page.help_note : note, oracles_ui_rgb(NOTE_ITALIC));
    oracles_ui_achievements_detail_draw(draw, nav, accent, now_ms);
}
#endif
