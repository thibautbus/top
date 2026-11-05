/* Cartridge's Save row (ui_page_nav.h, ui_page_layout.h): its layout against the layout reference (layout_reference.h),
 * the test's argument, and its navigation.  The reference's frames are turn 33's: Ages' row on a computer, Import save…
 * highlighted, in 16:9 (33a) and 4:3 (33b), and on Android at 640x480 with Export save… (33c) and in 16:9 with its long
 * note (33 android); the import's confirmation over the page (33d, 33d 4:3), and its Import in Replace's place for a
 * game without a save yet, in the same box, and without a folder line for a document whose address names none; the import done with the language set to
 * English (33e, 33e 4:3); a file refused (33f); Moonrise Regalia's row, without a note, on a computer (33g) and at
 * 640x480 on Android (33g 640x480).  The navigation: left and right between the actions, the pointer over them, what OK
 * asks of the host, the actions that do not act, the confirmation's buttons, Back, and the status kept until the page is
 * left. */
#include "layout_reference.h"
#include "ui_page_layout.h"
#include "ui_page_nav.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); layout_fail(); } } while (0)

/* Ages with its original ROM, its save, and its European ROM recognized, on its page, the Save row highlighted. */
static void ages(OraclesHomeNav *nav, int android, OraclesUiLayout layout)
{
    oracles_home_init(nav);
    OraclesHomeGame *g = &nav->games[ORACLES_HOME_GAME_AGES];
    memset(g, 0, sizeof *g);
    g->usable = 1;
    g->rom = g->eu = ORACLES_ROM_ORIGINAL;
    snprintf(g->rom_file, sizeof g->rom_file, "Oracle of Ages (USA).gbc");
    snprintf(g->rom_folder, sizeof g->rom_folder, "C:\\Users\\sam\\Games\\Oracles");
    snprintf(g->save_file, sizeof g->save_file, "Oracle of Ages (USA).sav");
    snprintf(g->save_written, sizeof g->save_written, "21 Sep 2026, 22:14");
    snprintf(g->last_session, sizeof g->last_session, "21 Sep 2026");
    snprintf(g->eu_file, sizeof g->eu_file, "Oracle of Ages (Europe).gbc");
    snprintf(g->eu_folder, sizeof g->eu_folder, "C:\\Users\\sam\\Games\\Oracles");
    nav->screen = ORACLES_SCREEN_GAME;
    nav->row = ORACLES_ROW_SAVE;
    nav->android = android;
    nav->layout = layout;
}

/* Moonrise Regalia's page, its base and patch chosen, its save beside the patch. */
static void moonrise(OraclesHomeNav *nav, int android, OraclesUiLayout layout)
{
    ages(nav, android, layout);
    OraclesHomeGame *g = &nav->games[ORACLES_HOME_GAME_MOONRISE];
    *g = nav->games[ORACLES_HOME_GAME_AGES];
    g->fan = oracles_home_fan_game(ORACLES_HOME_GAME_MOONRISE);
    g->eu = ORACLES_ROM_NONE;
    g->rom_folder[0] = 0;
    g->patch = g->image = ORACLES_ROM_ORIGINAL;
    snprintf(g->patch_file, sizeof g->patch_file, "Moonrise Regalia 1.0.6.bps");
    snprintf(g->patch_folder, sizeof g->patch_folder, "C:\\Users\\sam\\Games\\Oracles");
    snprintf(g->save_file, sizeof g->save_file, "Moonrise Regalia 1.0.6.sav");
    snprintf(g->save_written, sizeof g->save_written, "18 Sep 2026, 20:41");
    snprintf(g->last_session, sizeof g->last_session, "18 Sep 2026");
    nav->entry = ORACLES_HOME_FAN;
    nav->fan_pick = ORACLES_HOME_HERO_MOONRISE;
}

/* The page's texts, as ui_page.c makes them, and its layout. */
static void lay_out(const OraclesHomeNav *nav, OraclesUiGameTexts *t, char *state, OraclesUiGameLayout *l)
{
    const OraclesHomeGame *g = oracles_page_game_const(nav);
    OraclesTone tone;
    memset(t, 0, sizeof *t);
    oracles_home_state(nav, oracles_home_hero(nav), state);
    t->over = oracles_home_over(oracles_home_hero(nav));
    t->title = oracles_home_title(oracles_home_hero(nav));
    t->state = state;
    t->patched = g->fan != NULL;
    if (g->fan) {
        snprintf(t->patch_file, sizeof t->patch_file, "%s", oracles_page_patch_file(g));
        snprintf(t->patch_folder, sizeof t->patch_folder, "%s", g->patch_folder);
        oracles_page_patch_status(g, t->patch_status, sizeof t->patch_status, &tone);
        oracles_page_image_status(g, t->image_status, sizeof t->image_status, &tone);
    }
    snprintf(t->rom_file, sizeof t->rom_file, "%s", oracles_page_rom_file(g));
    snprintf(t->rom_folder, sizeof t->rom_folder, "%s", g->rom_folder);
    oracles_page_rom_status(g, t->rom_status, sizeof t->rom_status, &tone);
    snprintf(t->save_file, sizeof t->save_file, "%s", oracles_page_save_file(g));
    oracles_page_save_line(g, t->save_line, sizeof t->save_line);
    for (int i = 0; i < ORACLES_PAGE_SAVE_ACTIONS; i++) t->save_actions[i] = oracles_page_save_action(nav, i);
    t->save_note = oracles_page_save_note(nav, nav->layout == ORACLES_UI_LAYOUT_4_3);
    t->save_status = oracles_page_save_status(nav, &tone, &t->save_status2);
    t->rom_note = oracles_page_rom_note(nav);
    t->rom_hotkeys_note = oracles_page_rom_hotkeys_note(nav);
    t->play_note = oracles_page_play_note(nav);
    t->european = oracles_page_eu_shown(g);
    if (t->european) {
        snprintf(t->eu_file, sizeof t->eu_file, "%s", oracles_page_eu_file(g));
        snprintf(t->eu_folder, sizeof t->eu_folder, "%s", g->eu_folder);
        oracles_page_eu_status(nav, t->eu_status, sizeof t->eu_status, &tone);
        t->eu_buttons = oracles_page_eu_buttons(g, t->eu_button_labels);
    }
    t->languages = oracles_page_language_shown(nav, g);
    if (t->languages) {
        t->language_explanation = oracles_page_language_explanation(g);
        oracles_page_language_note(g, t->language_note, sizeof t->language_note);
    }
    oracles_ui_layout_game(nav->layout, t, l);
}

/* A text: in 4:3 its line box too. */
static void line(const char *frame, const char *probe, const OraclesUiLine *l, int four_three)
{
    if (four_three) layout_line(frame, probe, l);
    else layout_text(frame, probe, l);
}

/* The row, its file, its two actions with their dots and boxes (`first`: "Open folder" or "Export save"), the rows
 * after it. */
static void save_row(const char *frame, const char *first, const OraclesUiGameLayout *l, int four_three, int european)
{
    char probe[64];
    layout_box(frame, "Save row", &l->rows[ORACLES_ROW_SAVE]);
    line(frame, "Save", &l->label_save, four_three);
    line(frame, "save file", &l->save_file, four_three);
    line(frame, first, &l->save_buttons[0], four_three);
    snprintf(probe, sizeof probe, "%s dot", first);
    line(frame, probe, &l->save_button_dots[0], four_three);
    line(frame, "Import save", &l->save_buttons[1], four_three);
    line(frame, "Import save dot", &l->save_button_dots[1], four_three);
    if (european) {
        layout_box(frame, "Import save box", &l->save_button_boxes[1]);
        layout_box(frame, "EU row", &l->rows[ORACLES_ROW_EU_ROM]);
    }
    layout_box(frame, "Play row", &l->rows[ORACLES_ROW_PLAY]);
}

/* The note: its first line, its lines' height, and the box they make. */
static void note(const char *frame, const OraclesUiGameLayout *l, unsigned lines)
{
    layout_text(frame, "save note", &l->save_note.lines[0]);
    layout_near(frame, "save note", "top", l->save_note.lines[0].y);
    CHECK(l->save_note.count == lines);
    layout_near(frame, "save note", "lines", (float)l->save_note.count);
    layout_near(frame, "save note box", "y", l->save_note.lines[0].y);
    layout_near(frame, "save note box", "h", l->save_note.h);
}

static void layout_33abc(void)
{
    OraclesHomeNav nav;
    OraclesUiGameTexts t;
    OraclesUiGameLayout l;
    char state[ORACLES_HOME_STATE_LENGTH];
    ages(&nav, 0, ORACLES_UI_LAYOUT_16_9);
    nav.save_action = 1;
    lay_out(&nav, &t, state, &l);
    save_row("33a", "Open folder", &l, 0, 1);
    layout_box("33a", "Open folder box", &l.save_button_boxes[0]);
    layout_near("33a", "save file box", "y", l.save_file.y);
    layout_near("33a", "save file box", "h", l.save_file.h);
    layout_text("33a", "save line", &l.save_line);
    note("33a", &l, 2);
    layout_box("33a", "panel", &l.panel);

    ages(&nav, 0, ORACLES_UI_LAYOUT_4_3);
    nav.save_action = 1;
    lay_out(&nav, &t, state, &l);
    save_row("33b", "Open folder", &l, 1, 1);
    layout_box("33b", "Open folder box", &l.save_button_boxes[0]);
    layout_line("33b", "save line", &l.save_line);
    note("33b", &l, 1);
    CHECK(!strcmp(l.save_note.text[0], "Also works with the European ROM in other emulators, named like it."));

    ages(&nav, 1, ORACLES_UI_LAYOUT_4_3);
    lay_out(&nav, &t, state, &l);
    save_row("33c", "Export save", &l, 1, 1);
    layout_box("33c", "Export save box", &l.save_button_boxes[0]);
    layout_line("33c", "save line", &l.save_line);
    note("33c", &l, 1);
    CHECK(!strcmp(l.save_note.text[0], "Export a copy to play it elsewhere, also with the European ROM."));

    ages(&nav, 1, ORACLES_UI_LAYOUT_16_9);
    lay_out(&nav, &t, state, &l);
    layout_box("33 android", "Save row", &l.rows[ORACLES_ROW_SAVE]);
    layout_text("33 android", "Export save", &l.save_buttons[0]);
    layout_text("33 android", "Import save", &l.save_buttons[1]);
    layout_box("33 android", "EU row", &l.rows[ORACLES_ROW_EU_ROM]);
    note("33 android", &l, 2);

    /* Import save… highlighted on Android: its own note, whole, the row as tall as with Export save…'s. */
    for (int four_three = 0; four_three < 2; four_three++) {
        OraclesUiGameLayout import;
        ages(&nav, 1, four_three ? ORACLES_UI_LAYOUT_4_3 : ORACLES_UI_LAYOUT_16_9);
        lay_out(&nav, &t, state, &l);
        nav.save_action = 1;
        lay_out(&nav, &t, state, &import);
        CHECK(import.save_note.count == l.save_note.count && import.rows[ORACLES_ROW_SAVE].h == l.rows[ORACLES_ROW_SAVE].h);
        CHECK(import.rows[ORACLES_ROW_PLAY].y == l.rows[ORACLES_ROW_PLAY].y);
        if (four_three) CHECK(!strcmp(import.save_note.text[0], oracles_page_save_note(&nav, 1)));
    }
}

/* The status in the note's place (4:3: in the line's too), its dot, its second line. */
static void layout_33ef(void)
{
    OraclesHomeNav nav;
    OraclesUiGameTexts t;
    OraclesUiGameLayout l;
    char state[ORACLES_HOME_STATE_LENGTH];
    for (int four_three = 0; four_three < 2; four_three++) {
        const char *frame = four_three ? "33e 4:3" : "33e";
        ages(&nav, 0, four_three ? ORACLES_UI_LAYOUT_4_3 : ORACLES_UI_LAYOUT_16_9);
        nav.save_action = 1;
        nav.save_flow.state = ORACLES_SAVE_FLOW_DONE;
        snprintf(nav.save_flow.text, sizeof nav.save_flow.text, "Save imported. The previous one is in Backups, beside the ROM.");
        snprintf(nav.save_flow.text2, sizeof nav.save_flow.text2, "Its language was set to English for this game.");
        lay_out(&nav, &t, state, &l);
        save_row(frame, "Open folder", &l, four_three, 1);
        CHECK(l.save_note.count == 0 && (four_three ? l.save_line.h == 0.0f : l.save_line.h > 0.0f));
        line(frame, "status", &l.save_status.lines[0], four_three);
        line(frame, "status 2", &l.save_status2.lines[0], four_three);
        layout_box(frame, "status dot", &l.save_dot);
        layout_near(frame, "save status", "y", l.save_status.lines[0].y);
        layout_near(frame, "save status", "h", l.save_status.h + l.save_status2.h);
        if (!four_three) layout_text(frame, "save line", &l.save_line);
    }
    ages(&nav, 0, ORACLES_UI_LAYOUT_16_9);
    nav.save_action = 1;
    nav.save_flow.state = ORACLES_SAVE_FLOW_REFUSED;
    snprintf(nav.save_flow.text, sizeof nav.save_flow.text, "This file is not a save of Oracle of Ages.");
    lay_out(&nav, &t, state, &l);
    save_row("33f", "Open folder", &l, 0, 1);
    layout_text("33f", "status", &l.save_status.lines[0]);
    layout_box("33f", "status dot", &l.save_dot);
    layout_near("33f", "save status", "h", l.save_status.h);
    CHECK(l.save_status2.count == 0);
}

static void layout_33g(void)
{
    OraclesHomeNav nav;
    OraclesUiGameTexts t;
    OraclesUiGameLayout l;
    char state[ORACLES_HOME_STATE_LENGTH];
    moonrise(&nav, 0, ORACLES_UI_LAYOUT_16_9);
    nav.save_action = 1;
    lay_out(&nav, &t, state, &l);
    save_row("33g", "Open folder", &l, 0, 0);
    layout_text("33g", "save line", &l.save_line);
    layout_text("33g", "Play", &l.play);
    layout_box("33g", "panel", &l.panel);
    CHECK(l.save_note.count == 0);   /* a fan game's row says nothing of the European ROM */

    moonrise(&nav, 1, ORACLES_UI_LAYOUT_4_3);
    lay_out(&nav, &t, state, &l);
    save_row("33g 640x480", "Export save", &l, 1, 0);
    layout_line("33g 640x480", "save line", &l.save_line);
    layout_line("33g 640x480", "Play", &l.play);
    CHECK(l.save_note.count == 0);
}

/* The confirmation, centred over the page in each layout. */
static void layout_33d(void)
{
    for (int four_three = 0; four_three < 2; four_three++) {
        const char *frame = four_three ? "33d 4:3" : "33d";
        OraclesUiImportLayout l;
        const OraclesUiLayout layout = four_three ? ORACLES_UI_LAYOUT_4_3 : ORACLES_UI_LAYOUT_16_9;
        oracles_ui_layout_import(layout, "Oracle of Ages (Europe).sav", "C:\\Users\\sam\\Downloads", 1, &l);
        layout_box(frame, "backdrop", &l.backdrop);
        layout_box(frame, "dialog", &l.panel);
        layout_line(frame, "dialog title", &l.title);
        layout_line(frame, "dialog file", &l.file);
        layout_line(frame, "dialog folder", &l.folder);
        layout_line(frame, "dialog text", &l.question.lines[0]);
        CHECK(l.question.count == 1);
        layout_box(frame, "Replace", &l.buttons[0]);
        layout_box(frame, "Cancel", &l.buttons[1]);
        layout_line(frame, "Replace text", &l.labels[0]);
        layout_line(frame, "Cancel text", &l.labels[1]);
        CHECK(!strcmp(l.label_texts[0], "Replace") && !strncmp(l.question_text, "Replace this game's save?", 25));
        /* Without a save yet: whether to take the file, Import and Cancel, in the same box at the same places. */
        OraclesUiImportLayout take;
        oracles_ui_layout_import(layout, "Oracle of Ages (Europe).sav", "C:\\Users\\sam\\Downloads", 0, &take);
        CHECK(!strcmp(take.question_text, "Use this file as this game's save?") && take.question.count == 1);
        CHECK(!strcmp(take.label_texts[0], "Import") && !strcmp(take.label_texts[1], "Cancel"));
        CHECK(!memcmp(&take.panel, &l.panel, sizeof l.panel) && !memcmp(take.buttons, l.buttons, sizeof l.buttons));
        CHECK(take.question.lines[0].y == l.question.lines[0].y && take.labels[0].baseline == l.labels[0].baseline);
        /* The game with mods' save too: the question says so, Replace and Cancel. */
        OraclesUiImportLayout mods;
        oracles_ui_layout_import(layout, "Oracle of Ages (Europe).sav", "C:\\Users\\sam\\Downloads", 2, &mods);
        CHECK(!strcmp(mods.question_text, "Replace this game's save, with mods too? What it replaces is kept as a backup."));
        CHECK(!strcmp(mods.label_texts[0], "Replace") && mods.panel.y + mods.panel.h <= 1080.0f);
        CHECK(take.labels[0].x + take.labels[0].w * 0.5f == take.buttons[0].x + take.buttons[0].w * 0.5f);
        /* A document whose address names no folder (Android): no folder line, the question under the file. */
        OraclesUiImportLayout bare;
        oracles_ui_layout_import(layout, "Oracle of Ages (Europe).sav", "", 1, &bare);
        CHECK(bare.folder.h == 0.0f && bare.panel.h < l.panel.h);
        CHECK((l.question.lines[0].y - l.file.y) - (bare.question.lines[0].y - bare.file.y) == l.panel.h - bare.panel.h);
    }
}

/* Left and right between the actions; OK asks for what the chosen one does, nothing when it does not act. */
static void actions(void)
{
    OraclesHomeNav nav;
    ages(&nav, 0, ORACLES_UI_LAYOUT_16_9);
    CHECK(!strcmp(oracles_page_save_action(&nav, 0), "Open folder") && !strcmp(oracles_page_save_action(&nav, 1), "Import save\xe2\x80\xa6"));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_OPEN_FOLDER);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(nav.save_action == 1 && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_IMPORT_SAVE);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(nav.save_action == 1);
    oracles_home_act(&nav, ORACLES_HOME_LEFT);
    CHECK(nav.save_action == 0);
    /* A click on an action takes it and acts; on the row, the one chosen acts. */
    CHECK(oracles_page_click(&nav, ORACLES_ROW_SAVE, 1) == ORACLES_HOME_IMPORT_SAVE && nav.save_action == 1);
    CHECK(oracles_page_click(&nav, ORACLES_ROW_SAVE, -1) == ORACLES_HOME_IMPORT_SAVE);
    CHECK(oracles_page_click(&nav, ORACLES_ROW_SAVE, 0) == ORACLES_HOME_OPEN_FOLDER && nav.save_action == 0);

    /* Android: Export save…, greyed while there is no save; Import save… greyed without a usable ROM. */
    ages(&nav, 1, ORACLES_UI_LAYOUT_4_3);
    CHECK(!strcmp(oracles_page_save_action(&nav, 0), "Export save\xe2\x80\xa6"));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_EXPORT_SAVE);
    nav.games[ORACLES_HOME_GAME_AGES].save_file[0] = 0;
    CHECK(!oracles_page_save_action_live(&nav, 0) && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY);
    nav.games[ORACLES_HOME_GAME_AGES].usable = 0;
    nav.save_action = 1;
    CHECK(!oracles_page_save_action_live(&nav, 1) && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY);
    /* On a computer, Open folder needs a ROM or a save. */
    ages(&nav, 0, ORACLES_UI_LAYOUT_16_9);
    nav.games[ORACLES_HOME_GAME_AGES].usable = 0;
    CHECK(oracles_page_save_action_live(&nav, 0));
    nav.games[ORACLES_HOME_GAME_AGES].save_file[0] = 0;
    CHECK(!oracles_page_save_action_live(&nav, 0) && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY);
    /* From a game, Import is greyed and its note says why: the running game holds the save. */
    ages(&nav, 0, ORACLES_UI_LAYOUT_16_9);
    nav.in_game = 1;
    nav.save_action = 1;
    CHECK(!oracles_page_save_action_live(&nav, 1) && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY);
    CHECK(!strcmp(oracles_page_save_note(&nav, 0), oracles_display_pause_reason));
}

/* The notes: an Oracle's, by platform, layout and, on Android, action; none for a fan game, nor under a status. */
static void notes(void)
{
    OraclesHomeNav nav;
    ages(&nav, 0, ORACLES_UI_LAYOUT_16_9);
    CHECK(!strncmp(oracles_page_save_note(&nav, 0), "This save also works with the European ROM", 42));
    CHECK(!strcmp(oracles_page_save_note(&nav, 1), "Also works with the European ROM in other emulators, named like it."));
    nav.android = 1;
    CHECK(!strncmp(oracles_page_save_note(&nav, 0), "Export a copy to play it elsewhere: this save also works", 56));
    CHECK(!strcmp(oracles_page_save_note(&nav, 1), "Export a copy to play it elsewhere, also with the European ROM."));
    /* Import save… highlighted: its own note on Android, the same on a computer. */
    nav.save_action = 1;
    CHECK(!strcmp(oracles_page_save_note(&nav, 0), "Import a save from another emulator or another device, also one made with the European ROM: its language is then set to English for this game."));
    CHECK(!strcmp(oracles_page_save_note(&nav, 1), "Import a save from elsewhere, the European ROM's too."));
    nav.row = ORACLES_ROW_EU_ROM;
    CHECK(!strncmp(oracles_page_save_note(&nav, 1), "Export a copy", 13));
    nav.row = ORACLES_ROW_SAVE;
    nav.android = 0;
    CHECK(!strncmp(oracles_page_save_note(&nav, 0), "This save also works with the European ROM", 42));
    nav.android = 1;
    nav.save_action = 0;
    nav.save_flow.state = ORACLES_SAVE_FLOW_REFUSED;
    CHECK(!oracles_page_save_note(&nav, 0)[0]);
    moonrise(&nav, 0, ORACLES_UI_LAYOUT_16_9);
    CHECK(!oracles_page_save_note(&nav, 0)[0] && !oracles_page_save_note(&nav, 1)[0]);
}

/* The status of an import done: that it is, the language set to English, then what went to Backups, file by file,
 * beside the ROM or the patch; each line on one line of the row in 16:9 and 4:3, whatever went. */
static void import_done(void)
{
    static const struct { unsigned kept; int fan; const char *second; } cases[] = {
        { 0, 0, "" },
        { ORACLES_PAGE_KEPT_SAVE, 0, "In Backups, beside the ROM: the old save." },
        { ORACLES_PAGE_KEPT_SAVE | ORACLES_PAGE_KEPT_STATE, 0, "In Backups, beside the ROM: the old save and its savestate." },
        { ORACLES_PAGE_KEPT_STATE, 1, "In Backups, beside the patch: the old savestate." },
        { ORACLES_PAGE_KEPT_SAVE | ORACLES_PAGE_KEPT_MODS_SAVE, 0, "In Backups: the old save, with mods too." },
        { ORACLES_PAGE_KEPT_SAVE | ORACLES_PAGE_KEPT_STATE | ORACLES_PAGE_KEPT_MODS_SAVE | ORACLES_PAGE_KEPT_MODS_STATE, 0,
          "In Backups: the old save and its savestate, with mods too." },
        { 31u, 0, "In Backups: the old save and its savestate, with mods too, and the mods' data." },
        { ORACLES_PAGE_KEPT_SAVE | ORACLES_PAGE_KEPT_MODS_SAVE | ORACLES_PAGE_KEPT_MODS_STATE, 0,
          "In Backups: the old save, and the save with mods and its savestate." },
        { ORACLES_PAGE_KEPT_STATE | ORACLES_PAGE_KEPT_MODS_SAVE, 0, "In Backups: the old savestate and the save with mods." },
        { ORACLES_PAGE_KEPT_SAVE | ORACLES_PAGE_KEPT_STATE | ORACLES_PAGE_KEPT_MODS_SAVE, 0,
          "In Backups: the old save and its savestate, and the save with mods." },
        { ORACLES_PAGE_KEPT_MODS_SAVE | ORACLES_PAGE_KEPT_MODS_STATE, 0, "In Backups: the old save with mods and its savestate." },
        { ORACLES_PAGE_KEPT_MODS_STATE | ORACLES_PAGE_KEPT_MODS_STORE, 0, "In Backups: the old mods' savestate and data." },
        { ORACLES_PAGE_KEPT_SAVE | ORACLES_PAGE_KEPT_MODS_STORE, 0, "In Backups: the old save and the mods' data." },
    };
    char first[ORACLES_HOME_TEXT_LENGTH], second[ORACLES_HOME_TEXT_LENGTH];
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        oracles_page_import_done(cases[i].kept, cases[i].fan, 0, first, second);
        if (strcmp(second, cases[i].second)) fprintf(stderr, "  %u gave \"%s\"\n", cases[i].kept, second);
        CHECK(!strcmp(first, "Save imported.") && !strcmp(second, cases[i].second));
    }
    oracles_page_import_done(0, 0, 1, first, second);
    CHECK(!strcmp(first, "Save imported. Its language was set to English for this game.") && !second[0]);
    for (unsigned kept = 0; kept < 64u; kept++) {
        for (int four_three = 0; four_three < 2; four_three++) {
            OraclesHomeNav nav;
            OraclesUiGameTexts t;
            OraclesUiGameLayout l;
            char state[ORACLES_HOME_STATE_LENGTH];
            ages(&nav, 1, four_three ? ORACLES_UI_LAYOUT_4_3 : ORACLES_UI_LAYOUT_16_9);
            nav.save_action = 1;
            nav.save_flow.state = ORACLES_SAVE_FLOW_DONE;
            oracles_page_import_done(kept & 31u, 1, kept >= 32u, nav.save_flow.text, nav.save_flow.text2);
            lay_out(&nav, &t, state, &l);
            CHECK(l.save_status.count == 1u && l.save_status2.count == ((kept & 31u) ? 1u : 0u));
        }
    }
}

/* The confirmation: left and right between Replace and Cancel, OK, Back; the rest does nothing while it is open; the
 * status stays until the page is left. */
static void confirmation(void)
{
    OraclesHomeNav nav;
    ages(&nav, 0, ORACLES_UI_LAYOUT_16_9);
    nav.save_action = 1;
    nav.save_flow.state = ORACLES_SAVE_FLOW_CONFIRM;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_ROW_SAVE);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(nav.save_flow.choice == 1 && nav.save_action == 1);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.save_flow.state == ORACLES_SAVE_FLOW_NONE);
    nav.save_flow.state = ORACLES_SAVE_FLOW_CONFIRM;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.save_flow.state == ORACLES_SAVE_FLOW_NONE);
    CHECK(nav.screen == ORACLES_SCREEN_GAME);   /* Back closes the confirmation, not the page */
    nav.save_flow.state = ORACLES_SAVE_FLOW_CONFIRM;
    oracles_home_act(&nav, ORACLES_HOME_LEFT);
    CHECK(nav.save_flow.choice == 0 && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_REPLACE_SAVE);
    /* Without a save yet, Import takes the place of Replace and asks the same of the host. */
    nav.save_flow.replace = 0;
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    oracles_home_act(&nav, ORACLES_HOME_LEFT);
    CHECK(nav.save_flow.choice == 0 && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_REPLACE_SAVE);
    CHECK(oracles_page_click(&nav, ORACLES_ROW_SAVE, 2) == ORACLES_HOME_REPLACE_SAVE);
    /* Clicks: its buttons alone. */
    CHECK(oracles_page_click(&nav, ORACLES_ROW_PLAY, -1) == ORACLES_HOME_STAY && nav.save_flow.state == ORACLES_SAVE_FLOW_CONFIRM);
    CHECK(oracles_page_click(&nav, ORACLES_ROW_SAVE, 2) == ORACLES_HOME_REPLACE_SAVE);
    CHECK(oracles_page_click(&nav, ORACLES_ROW_SAVE, 3) == ORACLES_HOME_STAY && nav.save_flow.state == ORACLES_SAVE_FLOW_NONE);
    /* The host's outcome stays as the row's status; Back leaves the page and it goes. */
    nav.save_flow.state = ORACLES_SAVE_FLOW_DONE;
    snprintf(nav.save_flow.text, sizeof nav.save_flow.text, "Save imported.");
    OraclesTone tone = ORACLES_TONE_NONE;
    const char *second = "";
    CHECK(!strcmp(oracles_page_save_status(&nav, &tone, &second), "Save imported.") && tone == ORACLES_TONE_OK && !second);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.save_flow.state == ORACLES_SAVE_FLOW_DONE);
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(nav.screen == ORACLES_SCREEN_HOME && nav.save_flow.state == ORACLES_SAVE_FLOW_NONE && !oracles_page_save_status(&nav, &tone, &second));
}

/* The pointer over an action takes the highlight to it, as left and right do: Save's, European ROM's with a file (not
 * its lone Choose…), the confirmation's; over the row elsewhere, the action chosen stays. */
static void hover(void)
{
    OraclesHomeNav nav;
    ages(&nav, 0, ORACLES_UI_LAYOUT_16_9);
    nav.row = ORACLES_ROW_ROM;
    oracles_page_hover(&nav, ORACLES_ROW_SAVE, 1);
    CHECK(nav.row == ORACLES_ROW_SAVE && nav.save_action == 1 && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_IMPORT_SAVE);
    oracles_page_hover(&nav, ORACLES_ROW_SAVE, -1);
    CHECK(nav.save_action == 1);
    oracles_page_hover(&nav, ORACLES_ROW_SAVE, 0);
    CHECK(nav.save_action == 0 && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_OPEN_FOLDER);
    oracles_page_hover(&nav, ORACLES_ROW_EU_ROM, 1);
    CHECK(nav.row == ORACLES_ROW_EU_ROM && nav.eu_action == 1 && nav.save_action == 0);
    oracles_page_hover(&nav, ORACLES_ROW_EU_ROM, 0);
    CHECK(nav.eu_action == 0 && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_CHOOSE_EU_ROM);
    nav.games[ORACLES_HOME_GAME_AGES].eu = ORACLES_ROM_NONE;
    nav.eu_action = 0;
    oracles_page_hover(&nav, ORACLES_ROW_EU_ROM, 1);
    CHECK(nav.eu_action == 0);
    /* The confirmation open: its buttons alone, the rows under it left as they are. */
    nav.row = ORACLES_ROW_SAVE;
    nav.save_flow.state = ORACLES_SAVE_FLOW_CONFIRM;
    oracles_page_hover(&nav, ORACLES_ROW_SAVE, 3);
    CHECK(nav.save_flow.choice == 1 && nav.save_action == 0);
    oracles_page_hover(&nav, ORACLES_ROW_PLAY, -1);
    CHECK(nav.row == ORACLES_ROW_SAVE);
    oracles_page_hover(&nav, ORACLES_ROW_SAVE, 2);
    CHECK(nav.save_flow.choice == 0 && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_REPLACE_SAVE);
}

int main(int argc, char **argv)
{
    if (argc < 2 || !layout_reference_load(argv[1])) { fprintf(stderr, "usage: oracles-test-launcher-save launcher_layout_reference.json\n"); return 2; }
    if (!oracles_ui_fonts_load()) { fprintf(stderr, "FAIL the embedded fonts do not load\n"); return 1; }
    layout_33abc();
    layout_33d();
    layout_33ef();
    layout_33g();
    actions();
    notes();
    import_done();
    confirmation();
    hover();
    if (layout_failures()) { fprintf(stderr, "%d failure(s)\n", layout_failures()); return 1; }
    printf("launcher save row: Cartridge's Save row and the import's confirmation match the layout reference in both layouts, and behave as expected\n");
    return 0;
}
