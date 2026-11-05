/* Cartridge and Display (ui_page_nav.h, ui_page_layout.h): their layouts
 * against the layout reference, tests/launcher_layout_reference.json
 * (layout_reference.h), the test's argument, and their navigation.  The
 * reference's frames are 1h and 7a (an original ROM), 1i and 7b
 * (unrecognised, Enhanced chosen), 1j (refused), 2a and 7c (Enhanced at 4x,
 * reduced to 3x to fit), 2b and 7d (Faithful, fullscreen) and 2q (an
 * unrecognised ROM, Enhanced chosen), and Moonrise Regalia's pages, 1k (its
 * base ROM, its patch), 1l (no patch) and 2m (its Display, the title on two
 * lines), at 1920x1080 with the vendored fonts; and in the 4:3 layout, at
 * 1440x1080, Cartridge (10d, and Moonrise Regalia's, 10e), Display (10f, 10g
 * with View highlighted, 10o from the pause menu, 10s on a 640x480 screen,
 * where the Window row comes to one size) and its Advanced rows (10h, 10p).
 * Display's Quality on High and on Custom, highlighted, in 16:9 (11a, 11b),
 * in 4:3 (11d, 11e) and on a 640x480 screen (11g).  Advanced's Scaling, on
 * Sharp in 16:9 (13a), on Fill in 4:3 on a 640x480 screen (13b) and from the
 * pause menu (13d), and on Fill in Faithful fullscreen, the diagram filling
 * the screen (13e); Window's one size on a 640x480 screen (13c).  Cartridge's European ROM and Language rows
 * without a European ROM (14a, 14h), recognized with French chosen (14b, and in 4:3, 14d and 14e), with another file
 * (14c), and hidden with an unrecognised ROM (14i).  Display's HUD after View, on Full width and on Original (19a,
 * 19b), in 4:3 (19c) and at 640x480 (19d), from the pause menu (19e, 19f) and in Faithful (19g).  Advanced's Screen
 * margin, a state of the prototype the canvas does not draw: in 16:9 (margin), from the pause menu (margin pause), in
 * Faithful (margin faithful), in 4:3 (margin 4:3, margin pause 4:3) and at 640x480 (margin 640x480).  Far fitted to the
 * screen, Display as 1.2.0 ships it on Android with a controller, View highlighted: at 1280x960 (23a), 1240x1080
 * (23b), 1280x960 with Near chosen (23c), 640x480 (23d) and from the pause menu (23e); and 2a's state measured from that
 * prototype (23f); View's sizes on desktop screens, with far=fixed and a shape named.  Turn 26's Display, turn 22's with
 * its Text a link (test_launcher_text_page.c has Display › Text), which
 * moves Window's notes under the diagram in 16:9 and the help into the header in 4:3, every Display frame measured so,
 * HUD's row with its three choices (Wide layout chosen: 22a to 22e).  A text is
 * compared by its left, its width and its baseline (in 4:3 its line box too). */
#include "layout_reference.h"
#include "ui_page_layout.h"
#include "ui_controls_nav.h"
#include "ui_page_nav.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); layout_fail(); } } while (0)

/* A status the mockup's row cannot hold on its one line, which it lets run past the panel (1l, 1o, 1r): the layout
 * shortens it to its room, the row keeping the panel's width; their left edges, baselines and the row's height hold. */
static void overflowing(const char *frame, const char *status, const OraclesUiLine *line, const char *row, const OraclesUiBox *box)
{
    layout_near(frame, status, "x", line->x);
    layout_near(frame, status, "baseline", line->baseline);
    layout_near(frame, row, "x", box->x);
    layout_near(frame, row, "y", box->y);
    layout_near(frame, row, "h", box->h);
    if (line->x + line->w > box->x + box->w) { fprintf(stderr, "FAIL %s: %s past its row\n", frame, status); layout_fail(); }
}

static void game_1h(OraclesHomeGame *g)
{
    memset(g, 0, sizeof *g);
    g->usable = 1;
    g->rom = ORACLES_ROM_ORIGINAL;
    snprintf(g->rom_file, sizeof g->rom_file, "Oracle of Ages (USA).gbc");
    snprintf(g->rom_folder, sizeof g->rom_folder, "C:\\Users\\sam\\Games\\Oracles");
    snprintf(g->save_file, sizeof g->save_file, "Oracle of Ages (USA).sav");
    snprintf(g->save_written, sizeof g->save_written, "21 Sep 2026, 22:14");
    snprintf(g->last_session, sizeof g->last_session, "21 Sep 2026");
    g->hotkeys = 1;
}

static void texts_of(const OraclesHomeNav *nav, OraclesUiGameTexts *t, char *state)
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
        snprintf(t->eu_folder, sizeof t->eu_folder, "%s", g->eu != ORACLES_ROM_NONE ? g->eu_folder : "");
        oracles_page_eu_status(nav, t->eu_status, sizeof t->eu_status, &tone);
        t->eu_buttons = oracles_page_eu_buttons(g, t->eu_button_labels);
    }
    t->languages = oracles_page_language_shown(nav, g);
    if (t->languages) {
        t->language_explanation = oracles_page_language_explanation(g);
        oracles_page_language_note(g, t->language_note, sizeof t->language_note);
    }
}

static void layout_1h(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    nav.screen = ORACLES_SCREEN_GAME;
    char state[ORACLES_HOME_STATE_LENGTH];
    OraclesUiGameTexts t;
    texts_of(&nav, &t, state);
    OraclesUiGameLayout l;
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_16_9, &t, &l);
    /* The left column. */
    layout_text("1h", "section", &l.head.section);
    layout_text("1h", "over", &l.head.over);
    layout_text("1h", "title", &l.head.title.lines[0]);
    layout_text("1h", "state", &l.head.state);
    /* The rows' labels. */
    layout_text("1h", "ROM", &l.label_rom);
    layout_text("1h", "Save", &l.label_save);
    /* ROM and Save; an original ROM has no line under its status. */
    layout_text("1h", "ROM file", &l.rom_file);
    layout_text("1h", "ROM folder", &l.rom_folder);
    layout_text("1h", "ROM status", &l.rom_status.lines[0]);
    CHECK(t.rom_note[0] == 0 && l.rom_note.count == 0 && t.rom_hotkeys_note[0] == 0 && l.rom_hotkeys_note.count == 0);
    layout_box("1h", "ROM dot", &l.rom_dot);
    layout_text("1h", "Choose ROM", &l.rom_button);
    layout_text("1h", "Choose ROM dot", &l.rom_button_dot);
    layout_box("1h", "ROM row", &l.rows[ORACLES_ROW_ROM]);
    layout_text("1h", "save file", &l.save_file);
    layout_text("1h", "save line", &l.save_line);
    layout_text("1h", "Open folder", &l.save_buttons[0]);
    layout_box("1h", "Save row", &l.rows[ORACLES_ROW_SAVE]);
    /* Play, under Save. */
    layout_text("1h", "Play", &l.play);
    layout_text("1h", "Play dot", &l.play_dot);
    layout_box("1h", "panel", &l.panel);
}

static void layout_1i_1j(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.screen = ORACLES_SCREEN_GAME;
    OraclesHomeGame *g = &nav.games[0];
    game_1h(g);
    g->rom = ORACLES_ROM_UNRECOGNISED;
    snprintf(g->rom_file, sizeof g->rom_file, "Ages randomizer seed 4F1C.gbc");
    /* Before a session, nothing is known of the item hotkeys on this ROM: no line for them. */
    CHECK(oracles_page_rom_hotkeys_note(&nav)[0] == 0);
    g->hotkeys_failed = 1;   /* the frame's state: a session found they could not attach */
    char state[ORACLES_HOME_STATE_LENGTH];
    OraclesUiGameTexts t;
    OraclesUiGameLayout l;
    texts_of(&nav, &t, state);
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_16_9, &t, &l);
    layout_text("1i", "file", &l.rom_file);
    layout_text("1i", "status", &l.rom_status.lines[0]);
    /* Enhanced chosen in Display, and the item hotkeys on in Controls but unable to attach: a line each under the
     * status says the ROM plays in Faithful, without them, and the rows go down. */
    CHECK(!strcmp(t.rom_note, "Enhanced needs an original ROM: this one plays in Faithful"));
    CHECK(!strcmp(t.rom_hotkeys_note, "Item hotkeys need an original ROM: this one plays without them"));
    layout_text("1i", "note", &l.rom_note.lines[0]);
    layout_text("1i", "hotkeys note", &l.rom_hotkeys_note.lines[0]);
    layout_text("1i", "Choose ROM", &l.rom_button);
    layout_box("1i", "ROM row", &l.rows[ORACLES_ROW_ROM]);
    layout_text("1i", "Save", &l.label_save);
    layout_text("1i", "Play", &l.play);
    layout_box("1i", "panel", &l.panel);
    CHECK(oracles_page_profile(g, nav.display.profile) == ORACLES_PROFILE_FAITHFUL);
    /* With Faithful chosen, the profile's line goes; with the hotkeys off, theirs; an original ROM has neither. */
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    CHECK(oracles_page_rom_note(&nav)[0] == 0 && oracles_page_rom_hotkeys_note(&nav)[0] != 0);
    g->hotkeys = 0;
    CHECK(oracles_page_rom_hotkeys_note(&nav)[0] == 0);
    g->hotkeys = 1;
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    g->rom = ORACLES_ROM_ORIGINAL;
    g->hotkeys_failed = 0;
    CHECK(oracles_page_rom_note(&nav)[0] == 0 && oracles_page_rom_hotkeys_note(&nav)[0] == 0);
    g->rom = ORACLES_ROM_UNRECOGNISED;

    g->rom = ORACLES_ROM_REFUSED;
    g->usable = 0;
    g->save_file[0] = 0;
    snprintf(g->rom_file, sizeof g->rom_file, "Oracle of Seasons (USA).gbc");
    snprintf(g->rom_reason, sizeof g->rom_reason, "the header says Oracle of Seasons, not Oracle of Ages");
    texts_of(&nav, &t, state);
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_16_9, &t, &l);
    layout_text("1j", "file", &l.rom_file);
    layout_text("1j", "status", &l.rom_status.lines[0]);
    layout_text("1j", "save", &l.save_file);
    layout_text("1j", "save line", &l.save_line);
    CHECK(t.rom_note[0] == 0);   /* a refused ROM has its reason in its status */
    layout_text("1j", "Play", &l.play);
    CHECK(oracles_page_profile(g, ORACLES_PROFILE_ENHANCED) < 0);

    /* A path wider than its room is shortened in its middle, and fits. */
    char long_folder[600];
    memset(long_folder, 'a', sizeof long_folder - 1);
    long_folder[sizeof long_folder - 1] = 0;
    snprintf(t.rom_folder, sizeof t.rom_folder, "/home/%s/Games", long_folder);
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_16_9, &t, &l);
    CHECK(strstr(t.rom_folder, "\xe2\x80\xa6") != NULL);
    CHECK(!strncmp(t.rom_folder, "/home/", 6) && strstr(t.rom_folder, "/Games") != NULL);
    CHECK(l.rom_folder.x + l.rom_folder.w <= l.rom_button.x - 24.0f + LAYOUT_TOLERANCE);
}

/* Moonrise Regalia as the reference's frame 1k holds it: the Ages ROM for its base, its patch, the image recognised, a
 * save beside the patch. */
static void game_1k(OraclesHomeNav *nav)
{
    OraclesHomeGame *g = &nav->games[ORACLES_HOME_GAME_MOONRISE];
    game_1h(g);
    g->fan = oracles_home_fan_game(ORACLES_HOME_GAME_MOONRISE);
    g->hotkeys = 0;
    g->rom_folder[0] = 0;
    g->patch = g->image = ORACLES_ROM_ORIGINAL;
    snprintf(g->patch_file, sizeof g->patch_file, "Moonrise Regalia 1.0.6.bps");
    snprintf(g->patch_folder, sizeof g->patch_folder, "C:\\Users\\sam\\Games\\Oracles");
    snprintf(g->save_file, sizeof g->save_file, "Moonrise Regalia 1.0.6.sav");
    snprintf(g->save_written, sizeof g->save_written, "18 Sep 2026, 20:41");
    snprintf(g->last_session, sizeof g->last_session, "18 Sep 2026");
    nav->entry = ORACLES_HOME_FAN;
    nav->fan_pick = ORACLES_HOME_HERO_MOONRISE;
    nav->screen = ORACLES_SCREEN_GAME;
}

static void layout_1k_1l(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1k(&nav);
    char state[ORACLES_HOME_STATE_LENGTH];
    OraclesUiGameTexts t;
    OraclesUiGameLayout l;
    texts_of(&nav, &t, state);
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_16_9, &t, &l);
    /* The title on two lines in the column's 520 px, the state under both. */
    layout_text("1k", "section", &l.head.section);
    layout_text("1k", "over", &l.head.over);
    CHECK(l.head.title.count == 2 && !strcmp(l.head.title.text[0], "Moonrise") && !strcmp(l.head.title.text[1], "Regalia"));
    layout_text("1k", "title", &l.head.title.lines[0]);
    layout_near("1k", "title box", "h", l.head.title.h);
    layout_near("1k", "title box", "y", l.head.title.lines[1].y - 104.0f);
    layout_text("1k", "state", &l.head.state);
    /* Base ROM: the Ages ROM, without its folder; Patch, its folder and the image under the row. */
    layout_text("1k", "Base ROM", &l.label_rom);
    layout_text("1k", "Patch", &l.label_patch);
    layout_text("1k", "Save", &l.label_save);
    CHECK(l.rom_folder.h == 0.0f && !strcmp(t.rom_status, "Original ROM, Oracle of Ages (USA)"));
    layout_text("1k", "base file", &l.rom_file);
    layout_text("1k", "base status", &l.rom_status.lines[0]);
    layout_box("1k", "base dot", &l.rom_dot);
    layout_text("1k", "Choose ROM", &l.rom_button);
    layout_text("1k", "Choose ROM dot", &l.rom_button_dot);
    layout_box("1k", "base row", &l.rows[ORACLES_ROW_ROM]);
    layout_text("1k", "patch file", &l.patch_file);
    layout_text("1k", "patch status", &l.patch_status);
    layout_box("1k", "patch dot", &l.patch_dot);
    layout_text("1k", "Choose patch", &l.patch_button);
    layout_text("1k", "Choose patch dot", &l.patch_button_dot);
    layout_box("1k", "patch row", &l.rows[ORACLES_ROW_PATCH]);
    layout_text("1k", "patch folder", &l.patch_folder);
    layout_text("1k", "image status", &l.image_status);
    layout_box("1k", "image dot", &l.image_dot);
    layout_text("1k", "save file", &l.save_file);
    layout_text("1k", "save line", &l.save_line);
    layout_text("1k", "Open folder", &l.save_buttons[0]);
    layout_box("1k", "Save row", &l.rows[ORACLES_ROW_SAVE]);
    layout_text("1k", "Play", &l.play);
    layout_text("1k", "Play dot", &l.play_dot);
    layout_box("1k", "Play row", &l.rows[ORACLES_ROW_PLAY]);
    layout_box("1k", "panel", &l.panel);

    /* Without the patch: the image waits for both files, the folder's line stays, the game waits for its ROM. */
    OraclesHomeGame *g = &nav.games[ORACLES_HOME_GAME_MOONRISE];
    g->patch = g->image = ORACLES_ROM_NONE;
    g->patch_file[0] = g->patch_folder[0] = g->save_file[0] = 0;
    g->usable = 0;
    texts_of(&nav, &t, state);
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_16_9, &t, &l);
    layout_text("1l", "state", &l.head.state);
    layout_text("1l", "patch file", &l.patch_file);
    overflowing("1l", "patch status", &l.patch_status, "patch row", &l.rows[ORACLES_ROW_PATCH]);
    layout_text("1l", "image status", &l.image_status);
    layout_text("1l", "save file", &l.save_file);
    layout_text("1l", "save line", &l.save_line);
    layout_box("1l", "Save row", &l.rows[ORACLES_ROW_SAVE]);
    layout_text("1l", "Play", &l.play);
    layout_box("1l", "panel", &l.panel);
    /* Before a patch is chosen, the page names the one the game expects. */
    char text[ORACLES_HOME_TEXT_LENGTH];
    OraclesTone tone;
    oracles_page_patch_status(g, text, sizeof text, &tone);
    CHECK(!strcmp(text, "No patch: the BPS patch of Moonrise Regalia 1.0.6 is expected"));

    /* The other states, which no frame of the reference holds. */
    g->patch = ORACLES_ROM_REFUSED;
    snprintf(g->patch_reason, sizeof g->patch_reason, "the patch is not a BPS patch");
    oracles_page_patch_status(g, text, sizeof text, &tone);
    CHECK(!strcmp(text, "Refused: the patch is not a BPS patch.") && tone == ORACLES_TONE_ERROR);
    g->patch = ORACLES_ROM_ORIGINAL;
    g->image = ORACLES_ROM_REFUSED;
    snprintf(g->image_reason, sizeof g->image_reason, "this patch is not for this ROM (the base's size or checksum differs)");
    oracles_page_image_status(g, text, sizeof text, &tone);
    CHECK(!strcmp(text, "Refused: this patch is not for this ROM (the base's size or checksum differs).") && tone == ORACLES_TONE_ERROR);
    CHECK(oracles_page_profile(g, ORACLES_PROFILE_ENHANCED) < 0);
    /* Another image than Moonrise Regalia 1.0.6 plays, in Faithful; the page says so on the image's line only. */
    g->image = ORACLES_ROM_UNRECOGNISED;
    g->usable = 1;
    oracles_page_image_status(g, text, sizeof text, &tone);
    CHECK(!strcmp(text, "Patched image not recognized, Faithful only") && tone == ORACLES_TONE_WARN);
    CHECK(oracles_page_profile(g, ORACLES_PROFILE_ENHANCED) == ORACLES_PROFILE_FAITHFUL && oracles_page_rom_note(&nav)[0] == 0);
    g->rom = ORACLES_ROM_UNRECOGNISED;
    oracles_page_rom_status(g, text, sizeof text, &tone);
    CHECK(!strcmp(text, "Unrecognized ROM, not Oracle of Ages (USA)"));
    g->rom = ORACLES_ROM_NONE;
    oracles_page_rom_status(g, text, sizeof text, &tone);
    CHECK(!strcmp(text, "No base ROM chosen"));
    oracles_page_save_line(g, text, sizeof text);
    CHECK(!strcmp(text, "Created next to the patch on first save \xc2\xb7 savestates (.state) too"));
    /* A game that is not a patch has no Patch row: nothing is laid there. */
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    nav.screen = ORACLES_SCREEN_GAME;
    texts_of(&nav, &t, state);
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_16_9, &t, &l);
    CHECK(l.rows[ORACLES_ROW_PATCH].w == 0.0f && l.label_patch.w == 0.0f && l.head.title.count == 1);
}

static void navigation(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    /* Cartridge opens the page on its first row; Back returns to Cartridge. */
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    oracles_home_items(&nav, items);
    CHECK(!strcmp(items[1].label, "Cartridge"));
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_GAME && nav.row == ORACLES_ROW_ROM);
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    CHECK(oracles_home_hints(&nav, hints) == 4 && !strcmp(hints[1].label, "Change") && hints[3].back);
    /* OK on ROM asks for the file dialog; on Save, the folder; left and right change nothing. */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_CHOOSE_ROM);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_OPEN_FOLDER);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY);
    /* An original ROM: European ROM, then Language, after Save. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_ROW_EU_ROM && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_CHOOSE_EU_ROM);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_ROW_LANGUAGE);
    /* Play starts the game; down from Play wraps to ROM. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_ROW_PLAY && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_START_AGES);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_ROW_ROM);
    /* Without a usable ROM, Play is greyed with its note and does nothing; the save's folder neither without a save. */
    nav.games[0].usable = 0;
    nav.games[0].rom = ORACLES_ROM_NONE;
    nav.games[0].save_file[0] = 0;
    nav.row = ORACLES_ROW_PLAY;
    CHECK(!strcmp(oracles_page_play_note(&nav), "Choose a ROM first"));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY);
    nav.row = ORACLES_ROW_SAVE;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY);
    CHECK(oracles_page_click(&nav, ORACLES_ROW_ROM, -1) == ORACLES_HOME_CHOOSE_ROM && nav.row == ORACLES_ROW_ROM);
    /* An unrecognised ROM plays in Faithful whatever Display's profile; the home screen says so beside Display. */
    nav.games[0].usable = 1;
    nav.games[0].rom = ORACLES_ROM_UNRECOGNISED;
    CHECK(oracles_page_profile(&nav.games[0], ORACLES_PROFILE_ENHANCED) == ORACLES_PROFILE_FAITHFUL);
    oracles_home_items(&nav, items);
    CHECK(!strcmp(items[3].note, "Profile: Faithful"));
    nav.games[0].rom = ORACLES_ROM_ORIGINAL;
    oracles_home_items(&nav, items);
    CHECK(!strcmp(items[3].note, "Profile: Enhanced"));
    /* Back to the home screen, on Cartridge. */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_HOME && oracles_home_focus(&nav) == 1);
    /* Moonrise Regalia's Cartridge: Base ROM, Patch, Save, Language, Play, in that order; its Base ROM is the Ages ROM's
     * dialog. */
    game_1k(&nav);
    nav.screen = ORACLES_SCREEN_HOME;
    nav.focus = 0;
    oracles_home_items(&nav, items);
    CHECK(!items[0].disabled && !items[1].disabled && !strcmp(items[3].note, "Profile: Enhanced"));
    nav.focus = 1;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_GAME && nav.row == ORACLES_ROW_ROM);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_CHOOSE_ROM);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_ROW_PATCH && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_CHOOSE_PATCH);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_ROW_SAVE && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_OPEN_FOLDER);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_ROW_PLAY && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_START_MOONRISE);
    oracles_home_act(&nav, ORACLES_HOME_UP);
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(nav.row == ORACLES_ROW_PATCH && oracles_page_click(&nav, ORACLES_ROW_PATCH, -1) == ORACLES_HOME_CHOOSE_PATCH);
    /* Its image, not the Ages ROM, gives its profile. */
    nav.games[ORACLES_HOME_GAME_MOONRISE].image = ORACLES_ROM_UNRECOGNISED;
    CHECK(oracles_display_played_profile(&nav) == ORACLES_PROFILE_FAITHFUL);
    nav.games[ORACLES_HOME_GAME_MOONRISE].image = ORACLES_ROM_ORIGINAL;
    CHECK(oracles_display_played_profile(&nav) == ORACLES_PROFILE_ENHANCED);
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(nav.screen == ORACLES_SCREEN_HOME && oracles_home_focus(&nav) == 1);
    /* Ages' page has no Patch row: down from ROM goes to Save, and the row is not hovered or clicked. */
    oracles_home_select(&nav, ORACLES_HOME_AGES);
    nav.screen = ORACLES_SCREEN_GAME;
    nav.row = ORACLES_ROW_ROM;
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_ROW_SAVE);
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(nav.row == ORACLES_ROW_ROM);
    oracles_page_hover(&nav, ORACLES_ROW_PATCH, -1);
    CHECK(nav.row == ORACLES_ROW_ROM && oracles_page_click(&nav, ORACLES_ROW_PATCH, -1) == ORACLES_HOME_STAY);
    nav.screen = ORACLES_SCREEN_HOME;
    oracles_home_select(&nav, ORACLES_HOME_FAN);
    oracles_home_act(&nav, ORACLES_HOME_OK);
    /* Its Controls: the item column named after it, empty, and the item hotkeys off, which do not turn on. */
    nav.focus = 2;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_CONTROLS);
    snprintf(nav.controls.items[0][0], sizeof nav.controls.items[0][0], "Shooter");
    CHECK(!strcmp(oracles_controls_item_heading(&nav), "In Gifts of Kinomi") && oracles_controls_item(&nav, 0)[0] == 0);
    CHECK(!oracles_controls_hotkeys_on(&nav) && oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_MODE, 1) == ORACLES_HOME_STAY);
    CHECK(!oracles_controls_hotkeys_on(&nav));
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    nav.focus = 3;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_DISPLAY);
}

/* A fan game's frames after Moonrise's: its page (Cartridge) with its base ROM, its patch and the image recognised, the
 * page without the patch, and Display (layout_fan_page). */
typedef struct fan_frames {
    const char *page, *bare, *display; int game; OraclesHomeHero hero;
    const char *base_file, *base_name, *file_stem, *written, *title[2];
} fan_frames;
static const fan_frames temple_frames = { "1n", "1o", "2n", ORACLES_HOME_GAME_TEMPLE, ORACLES_HOME_HERO_TEMPLE,
    "Oracle of Seasons (USA).gbc", "Oracle of Seasons (USA)", "Temple of Seasons 1.073",
    "27 Sep 2026, 10:12", { "Temple of", "Seasons" } };
static const fan_frames kinomi_frames = { "1q", "1r", "2o", ORACLES_HOME_GAME_KINOMI, ORACLES_HOME_HERO_KINOMI,
    "Oracle of Ages (USA).gbc", "Oracle of Ages (USA)", "Gifts of Kinomi 1.1.2",
    "27 Sep 2026, 18:30", { "Gifts of", "Kinomi" } };

/* The fan game as the reference's frame holds it: its Oracle's ROM for its base, its patch, the image recognised. */
static void game_fan(OraclesHomeNav *nav, const fan_frames *f)
{
    OraclesHomeGame *g = &nav->games[f->game];
    game_1h(g);
    g->fan = oracles_home_fan_game(f->game);
    g->hotkeys = 0;
    g->rom_folder[0] = 0;
    snprintf(g->rom_file, sizeof g->rom_file, "%s", f->base_file);
    g->patch = g->image = ORACLES_ROM_ORIGINAL;
    snprintf(g->patch_file, sizeof g->patch_file, "%s.bps", f->file_stem);
    snprintf(g->patch_folder, sizeof g->patch_folder, "C:\\Users\\sam\\Games\\Oracles");
    snprintf(g->save_file, sizeof g->save_file, "%s.sav", f->file_stem);
    snprintf(g->save_written, sizeof g->save_written, "%s", f->written);
    snprintf(g->last_session, sizeof g->last_session, "27 Sep 2026");
    nav->entry = ORACLES_HOME_FAN;
    nav->fan_pick = f->hero;
    nav->screen = ORACLES_SCREEN_GAME;
}

/* ---- European ROM and Language (turns 14, 15 and 16) --------------------------------------------------------------- */

/* The European ROM of frames 14b, 14d, 14e and 15a to 15c: recognized, `language` chosen. */
static void eu_recognized(OraclesHomeGame *g, int language)
{
    g->eu = ORACLES_ROM_ORIGINAL;
    snprintf(g->eu_file, sizeof g->eu_file, "Oracle of Ages (Europe).gbc");
    snprintf(g->eu_folder, sizeof g->eu_folder, "C:\\Users\\sam\\Games\\Oracles");
    g->language = language;
}

/* The Language row of a frame, in either layout (4:3 comparing the texts' line boxes too): its label, its row, each
 * language's frame and name, and its lines, in 16:9 the explanation and the note's line, in 4:3 the one line, the note
 * while there is one, the other line not laid out; then Play. */
static void language_row(const char *frame, const OraclesUiGameTexts *t, const OraclesUiGameLayout *l, int four_three)
{
    static const char *const ids[ORACLES_PAGE_LANGUAGES] = { "en", "en-eu", "fr", "de", "it", "es" };
    void (*line)(const char *, const char *, const OraclesUiLine *) = four_three ? layout_line : layout_text;
    line(frame, "Language label", &l->label_language);
    layout_box(frame, "language row", &l->rows[ORACLES_ROW_LANGUAGE]);
    for (int i = 0; i < ORACLES_PAGE_LANGUAGES; i++) {
        char name[32];
        snprintf(name, sizeof name, "language %s", ids[i]); layout_box(frame, name, &l->languages[i].box);
        snprintf(name, sizeof name, "language %s name", ids[i]); line(frame, name, &l->languages[i].name);
    }
    if (!four_three) {
        line(frame, "language expl", &l->language_explanation);
        layout_near(frame, "language note box", "y", l->language_note.y);
        layout_near(frame, "language note box", "h", l->language_note.h);
        if (t->language_note[0]) line(frame, "language note", &l->language_note);
    } else if (t->language_note[0]) {
        CHECK(l->language_explanation.h == 0.0f && !t->language_explanation[0]);
        line(frame, "language note", &l->language_note);
        layout_near(frame, "language note box", "y", l->language_note.y);
        layout_near(frame, "language note box", "h", l->language_note.h);
    } else {
        CHECK(l->language_note.h == 0.0f);
        line(frame, "language expl", &l->language_explanation);
        layout_near(frame, "language expl box", "y", l->language_explanation.y);
        layout_near(frame, "language expl box", "h", l->language_explanation.h);
    }
    layout_box(frame, "Play row", &l->rows[ORACLES_ROW_PLAY]);
    line(frame, "Play", &l->play);
}

/* The two rows of an Oracle's frame of turns 14 and 15, in either layout. */
static void eu_rows(const char *frame, const OraclesUiGameTexts *t, const OraclesUiGameLayout *l, int four_three)
{
    void (*line)(const char *, const char *, const OraclesUiLine *) = four_three ? layout_line : layout_text;
    line(frame, "EU label", &l->label_eu);
    layout_box(frame, "EU row", &l->rows[ORACLES_ROW_EU_ROM]);
    line(frame, "EU file", &l->eu_file);
    layout_near(frame, "EU folder box", "y", l->eu_folder.y);
    layout_near(frame, "EU folder box", "h", l->eu_folder.h);
    line(frame, "EU status", &l->eu_status);
    layout_box(frame, "EU dot", &l->eu_dot);
    if (t->eu_buttons == 1) {
        line(frame, "EU choose", &l->eu_buttons[0]);
        line(frame, "EU choose dot", &l->eu_button_dots[0]);
        layout_box(frame, "EU choose box", &l->eu_button_boxes[0]);
    } else {
        layout_near(frame, "EU folder", "x", l->eu_folder.x);
        layout_near(frame, "EU folder", "w", l->eu_folder.w);
        layout_near(frame, "EU folder", "baseline", l->eu_folder.baseline);
        line(frame, "EU change", &l->eu_buttons[0]);
        line(frame, "EU change dot", &l->eu_button_dots[0]);
        layout_box(frame, "EU change box", &l->eu_button_boxes[0]);
        line(frame, "EU remove", &l->eu_buttons[1]);
        line(frame, "EU remove dot", &l->eu_button_dots[1]);
        layout_box(frame, "EU remove box", &l->eu_button_boxes[1]);
    }
    language_row(frame, t, l, four_three);
    line(frame, "Play dot", &l->play_dot);
}

/* The rows' boxes are the same in both layouts: `a`'s and `b`'s. */
static int same_rows(const OraclesUiGameLayout *a, const OraclesUiGameLayout *b)
{
    for (int r = 0; r < ORACLES_GAME_ROWS; r++)
        if (memcmp(&a->rows[r], &b->rows[r], sizeof a->rows[r])) return 0;
    return memcmp(a->languages, b->languages, sizeof a->languages) == 0 && a->language_note.y == b->language_note.y;
}

#define EUROPEAN_LANGUAGES "English (EU), Fran\xc3\xa7" "ais, Deutsch, Italiano, Espa\xc3\xb1" "ol"

static void layout_turn_14(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    nav.screen = ORACLES_SCREEN_GAME;
    char state[ORACLES_HOME_STATE_LENGTH];
    OraclesUiGameTexts t;
    OraclesUiGameLayout l, other;
    /* 14a: no European ROM, Language greyed with English (US) alone; ROM highlighted. */
    texts_of(&nav, &t, state);
    CHECK(t.european && t.languages && t.eu_buttons == 1 && !strcmp(t.eu_file, "None") && t.language_note[0] == 0);
    CHECK(!strcmp(t.eu_status, "Optional: " EUROPEAN_LANGUAGES));
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_16_9, &t, &l);
    eu_rows("14a", &t, &l, 0);
    layout_box("14a", "Save row", &l.rows[ORACLES_ROW_SAVE]);
    layout_box("14a", "panel", &l.panel);
    CHECK(oracles_page_language_available(&nav.games[0], 0) && !oracles_page_language_available(&nav.games[0], 1));
    /* 14h: European ROM highlighted, the rows where they were. */
    nav.row = ORACLES_ROW_EU_ROM;
    texts_of(&nav, &t, state);
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_16_9, &t, &other);
    eu_rows("14h", &t, &other, 0);
    CHECK(same_rows(&l, &other));
    /* 14b: recognized, French chosen, Language highlighted: the rows where they were, the note in its kept line. */
    eu_recognized(&nav.games[0], 2);
    nav.row = ORACLES_ROW_LANGUAGE;
    texts_of(&nav, &t, state);
    CHECK(t.eu_buttons == 2 && !strcmp(t.eu_button_labels[0], "Change\xe2\x80\xa6") && !strcmp(t.eu_button_labels[1], "Remove"));
    CHECK(!strcmp(t.eu_status, "Recognized: " EUROPEAN_LANGUAGES));
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_16_9, &t, &other);
    eu_rows("14b", &t, &other, 0);
    layout_box("14b", "panel", &other.panel);
    CHECK(same_rows(&l, &other));
    /* 14d and 14e: the same in 4:3, whatever the screen: the note alone under the languages. */
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_4_3, &t, &l);
    eu_rows("14d", &t, &l, 1);
    layout_box("14d", "Save row", &l.rows[ORACLES_ROW_SAVE]);
    texts_of(&nav, &t, state);
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_4_3, &t, &l);
    eu_rows("14e", &t, &l, 1);
    /* 14c: a file that is not the game's European ROM; Language back in English (US). */
    OraclesHomeGame *g = &nav.games[0];
    g->eu = ORACLES_ROM_UNRECOGNISED;
    snprintf(g->eu_file, sizeof g->eu_file, "Oracle of Seasons (Europe).gbc");
    g->language = 0;
    nav.row = ORACLES_ROW_EU_ROM;
    texts_of(&nav, &t, state);
    OraclesTone tone;
    oracles_page_eu_status(&nav, t.eu_status, sizeof t.eu_status, &tone);
    CHECK(!strcmp(t.eu_status, "Not the European ROM of Oracle of Ages") && tone == ORACLES_TONE_ERROR);
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_16_9, &t, &l);
    eu_rows("14c", &t, &l, 0);
    /* 14i: an unrecognised US ROM, the two rows hidden, the page as it was before them. */
    eu_recognized(g, 2);
    g->rom = ORACLES_ROM_UNRECOGNISED;
    g->hotkeys_failed = 1;
    g->save_file[0] = 0;
    snprintf(g->rom_file, sizeof g->rom_file, "Ages randomizer seed 4F1C.gbc");
    nav.row = ORACLES_ROW_ROM;
    texts_of(&nav, &t, state);
    CHECK(!t.european && !t.languages && oracles_page_language(&nav, g) == 0);
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_4_3, &t, &l);
    layout_box("14i", "ROM row", &l.rows[ORACLES_ROW_ROM]);
    layout_line("14i", "status", &l.rom_status.lines[0]);
    layout_line("14i", "note", &l.rom_note.lines[0]);
    layout_line("14i", "hotkeys note", &l.rom_hotkeys_note.lines[0]);
    layout_line("14i", "Save", &l.label_save);
    layout_box("14i", "Save row", &l.rows[ORACLES_ROW_SAVE]);
    layout_box("14i", "Play row", &l.rows[ORACLES_ROW_PLAY]);
    layout_line("14i", "Play", &l.play);
    CHECK(l.rows[ORACLES_ROW_EU_ROM].w == 0.0f && l.rows[ORACLES_ROW_LANGUAGE].w == 0.0f && l.label_eu.w == 0.0f);
}

/* Turn 15: English (EU) chosen, Language highlighted, in 16:9 (15a), in 4:3 (15b) and at 640x480 (15c). */
static void layout_turn_15(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    nav.screen = ORACLES_SCREEN_GAME;
    eu_recognized(&nav.games[0], 1);
    nav.row = ORACLES_ROW_LANGUAGE;
    char state[ORACLES_HOME_STATE_LENGTH];
    OraclesUiGameTexts t;
    OraclesUiGameLayout l;
    texts_of(&nav, &t, state);
    CHECK(oracles_page_language(&nav, &nav.games[0]) == 1 && !strcmp(oracles_page_language_names[1], "English (EU)"));
    CHECK(!strcmp(t.language_explanation, "The European edition's text, from your European ROM."));
    CHECK(!strcmp(t.language_note, "Where the official translation was wrong, the port corrects it."));
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_16_9, &t, &l);
    eu_rows("15a", &t, &l, 0);
    layout_box("15a", "panel", &l.panel);
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_4_3, &t, &l);
    eu_rows("15b", &t, &l, 1);
    texts_of(&nav, &t, state);
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_4_3, &t, &l);
    eu_rows("15c", &t, &l, 1);
}

/* A fan game's rows of a frame of turn 16, its Language row after Save. */
static void fan_language_rows(const char *frame, const OraclesUiGameTexts *t, const OraclesUiGameLayout *l, int four_three)
{
    void (*line)(const char *, const char *, const OraclesUiLine *) = four_three ? layout_line : layout_text;
    line(frame, "Base ROM", &l->label_rom);
    line(frame, "Patch", &l->label_patch);
    line(frame, "Save", &l->label_save);
    layout_box(frame, "base row", &l->rows[ORACLES_ROW_ROM]);
    layout_box(frame, "patch row", &l->rows[ORACLES_ROW_PATCH]);
    layout_box(frame, "Save row", &l->rows[ORACLES_ROW_SAVE]);
    language_row(frame, t, l, four_three);
    line(frame, "Play dot", &l->play_dot);
    if (!four_three) layout_box(frame, "panel", &l->panel);
}

/* Turn 16, with the fan games' languages on (the mockup's fanLang): a fan game's Language row, the European ROM its
 * Oracle's page names.  Moonrise Regalia in French (16a), in 4:3 (16b) and at 640x480 (16c); Gifts of Kinomi without the
 * European ROM of Ages, greyed with English (US) alone (16d); Temple of Seasons in English (EU) (16e). */
static void layout_turn_16(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.fan_languages = 1;
    game_1k(&nav);
    OraclesHomeGame *g = &nav.games[ORACLES_HOME_GAME_MOONRISE];
    eu_recognized(g, 2);
    nav.row = ORACLES_ROW_LANGUAGE;
    char state[ORACLES_HOME_STATE_LENGTH];
    OraclesUiGameTexts t;
    OraclesUiGameLayout l;
    texts_of(&nav, &t, state);
    CHECK(!t.european && t.languages && oracles_page_language(&nav, g) == 2);
    CHECK(!strcmp(t.language_explanation, "Texts the fan game shares with Oracle of Ages, from its European edition."));
    CHECK(!strcmp(t.language_note, "Only the texts it shares with Oracle of Ages are translated; its own dialogues stay in English."));
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_16_9, &t, &l);
    fan_language_rows("16a", &t, &l, 0);
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_4_3, &t, &l);
    fan_language_rows("16b", &t, &l, 1);
    texts_of(&nav, &t, state);
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_4_3, &t, &l);
    fan_language_rows("16c", &t, &l, 1);
    /* English (US) on Moonrise: its own text, no note. */
    g->language = 0;
    texts_of(&nav, &t, state);
    CHECK(!strcmp(t.language_explanation, "The original text of your fan game.") && !t.language_note[0]);

    oracles_home_init(&nav);
    nav.fan_languages = 1;
    game_fan(&nav, &kinomi_frames);
    g = &nav.games[ORACLES_HOME_GAME_KINOMI];
    g->language = 2;   /* settings.txt's, kept while the European ROM of Ages is missing */
    nav.row = ORACLES_ROW_LANGUAGE;
    texts_of(&nav, &t, state);
    CHECK(!oracles_page_language_available(g, 1) && oracles_page_language(&nav, g) == 0 && !t.language_note[0]);
    CHECK(!strcmp(t.language_explanation, "Choose the European ROM of Oracle of Ages in its Cartridge to play it in another language."));
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_16_9, &t, &l);
    fan_language_rows("16d", &t, &l, 0);
    /* Recognized, but its Oracle's ROM not the original: still English (US) alone. */
    g->eu = ORACLES_ROM_ORIGINAL;
    CHECK(oracles_page_language(&nav, g) == 2);
    g->rom = ORACLES_ROM_UNRECOGNISED;
    CHECK(!oracles_page_language_available(g, 1) && oracles_page_language(&nav, g) == 0);

    oracles_home_init(&nav);
    nav.fan_languages = 1;
    game_fan(&nav, &temple_frames);
    g = &nav.games[ORACLES_HOME_GAME_TEMPLE];
    eu_recognized(g, 1);
    nav.row = ORACLES_ROW_LANGUAGE;
    texts_of(&nav, &t, state);
    CHECK(!strcmp(t.language_explanation, "Texts the fan game shares with Oracle of Seasons, from its European edition."));
    CHECK(!strcmp(t.language_note, "Only the texts it shares with Oracle of Seasons are translated; its own dialogues stay in English."));
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_16_9, &t, &l);
    fan_language_rows("16e", &t, &l, 0);
}

/* European ROM's buttons and the languages, by the keys and the pointer. */
static void languages_navigation(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    OraclesHomeGame *g = &nav.games[0];
    nav.focus = 1;
    oracles_home_act(&nav, ORACLES_HOME_OK);
    CHECK(nav.screen == ORACLES_SCREEN_GAME && nav.row == ORACLES_ROW_ROM && nav.eu_action == 0);
    char note[ORACLES_HOME_TEXT_LENGTH];
    /* No European ROM: Choose… alone, left and right change nothing; Language greyed: nothing changes it. */
    nav.row = ORACLES_ROW_EU_ROM;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.eu_action == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_CHOOSE_EU_ROM);
    CHECK(oracles_page_click(&nav, ORACLES_ROW_EU_ROM, 0) == ORACLES_HOME_CHOOSE_EU_ROM);
    nav.row = ORACLES_ROW_LANGUAGE;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && g->language == 0);
    oracles_page_language_note(g, note, sizeof note);
    CHECK(!strcmp(oracles_page_language_explanation(g), "The original text of your US ROM.") && !note[0]);
    CHECK(oracles_page_click(&nav, ORACLES_ROW_LANGUAGE, 1) == ORACLES_HOME_STAY && g->language == 0);
    /* Recognized: the six languages go round, each choice kept (ORACLES_HOME_STORE); the home screen names it beside
     * Cartridge. */
    eu_recognized(g, 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && g->language == 1);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && g->language == 2);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && g->language == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && g->language == 5);
    CHECK(oracles_page_click(&nav, ORACLES_ROW_LANGUAGE, 2) == ORACLES_HOME_STORE && g->language == 2);
    CHECK(oracles_page_click(&nav, ORACLES_ROW_LANGUAGE, 2) == ORACLES_HOME_STAY && oracles_page_click(&nav, ORACLES_ROW_LANGUAGE, -1) == ORACLES_HOME_STAY);
    CHECK(!strcmp(oracles_page_language_explanation(g), "The European edition's text, from your European ROM."));
    oracles_page_language_note(g, note, sizeof note);
    CHECK(!strcmp(note, "Where the official translation was wrong, the port corrects it."));
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    oracles_home_items(&nav, items);
    CHECK(items[1].note && !strcmp(items[1].note, "Fran\xc3\xa7" "ais") && !strcmp(items[3].note, "Profile: Enhanced"));
    CHECK(oracles_page_click(&nav, ORACLES_ROW_LANGUAGE, 1) == ORACLES_HOME_STORE && g->language == 1);
    oracles_home_items(&nav, items);
    CHECK(items[1].note && !strcmp(items[1].note, "English (EU)"));
    /* European ROM: right moves to Remove, left back to Change…; Remove forgets the file and the language. */
    nav.row = ORACLES_ROW_EU_ROM;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.eu_action == 1);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && nav.eu_action == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_CHOOSE_EU_ROM && g->eu == ORACLES_ROM_ORIGINAL);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && g->eu == ORACLES_ROM_NONE && g->language == 0 && nav.eu_action == 0);
    oracles_home_items(&nav, items);
    CHECK(items[1].note == NULL);
    /* By the pointer: Remove removes, the rest of the row opens the dialog. */
    eu_recognized(g, 3);
    CHECK(oracles_page_click(&nav, ORACLES_ROW_EU_ROM, -1) == ORACLES_HOME_CHOOSE_EU_ROM && nav.eu_action == 0);
    CHECK(oracles_page_click(&nav, ORACLES_ROW_EU_ROM, 1) == ORACLES_HOME_STORE && g->eu == ORACLES_ROM_NONE && g->language == 0);
    /* A file that is not the game's European ROM, or one gone: English (US) alone, Change… and Remove. */
    eu_recognized(g, 2);
    g->eu = ORACLES_ROM_REFUSED;
    snprintf(g->eu_reason, sizeof g->eu_reason, "cannot open the file");
    char status[ORACLES_HOME_TEXT_LENGTH];
    OraclesTone tone;
    oracles_page_eu_status(&nav, status, sizeof status, &tone);
    CHECK(!strcmp(status, "Refused: cannot open the file.") && tone == ORACLES_TONE_ERROR && oracles_page_language(&nav, g) == 0);
    const char *labels[2];
    CHECK(oracles_page_eu_buttons(g, labels) == 2);
    /* An unrecognised US ROM: no European ROM or Language row; down from Save goes to Play. */
    g->rom = ORACLES_ROM_UNRECOGNISED;
    nav.row = ORACLES_ROW_SAVE;
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_ROW_PLAY);
    oracles_page_hover(&nav, ORACLES_ROW_LANGUAGE, -1);
    CHECK(nav.row == ORACLES_ROW_PLAY && oracles_page_click(&nav, ORACLES_ROW_EU_ROM, 0) == ORACLES_HOME_STAY);
    /* A fan game, its languages off as in the product: no European ROM or Language row, down from Save to Play, no
     * language beside Cartridge whatever settings.txt chose and its Oracle's European ROM recognized. */
    oracles_home_init(&nav);
    game_1h(&nav.games[ORACLES_HOME_GAME_AGES]);
    game_1k(&nav);
    OraclesHomeGame *fan = &nav.games[ORACLES_HOME_GAME_MOONRISE];
    fan->eu = ORACLES_ROM_ORIGINAL;
    fan->language = 2;
    CHECK(!oracles_page_eu_shown(fan) && !oracles_page_language_shown(&nav, fan) && oracles_page_language(&nav, fan) == 0);
    nav.row = ORACLES_ROW_SAVE;
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_ROW_PLAY);
    oracles_page_hover(&nav, ORACLES_ROW_LANGUAGE, -1);
    CHECK(nav.row == ORACLES_ROW_PLAY && oracles_page_click(&nav, ORACLES_ROW_LANGUAGE, 2) == ORACLES_HOME_STAY);
    nav.screen = ORACLES_SCREEN_HOME;
    oracles_home_items(&nav, items);
    CHECK(items[1].note == NULL);
    /* On (the mockup's fanLang): the Language row and no European ROM row, down from Save to Language, then Play; its
     * languages its Oracle's European ROM's, beside Cartridge too, and gone with it. */
    oracles_home_init(&nav);
    nav.fan_languages = 1;
    game_1h(&nav.games[ORACLES_HOME_GAME_AGES]);
    game_1k(&nav);
    CHECK(!oracles_page_eu_shown(fan) && oracles_page_language_shown(&nav, fan) && oracles_page_language(&nav, fan) == 0);
    nav.row = ORACLES_ROW_SAVE;
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_ROW_LANGUAGE);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && fan->language == 0);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_ROW_PLAY);
    eu_recognized(&nav.games[ORACLES_HOME_GAME_AGES], 0);
    fan->eu = ORACLES_ROM_ORIGINAL;
    nav.row = ORACLES_ROW_LANGUAGE;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && fan->language == 5);
    nav.screen = ORACLES_SCREEN_HOME;
    oracles_home_items(&nav, items);
    CHECK(items[1].note && !strcmp(items[1].note, "Espa\xc3\xb1" "ol"));
    /* The European ROM of Ages removed on its page: Moonrise back to English (US), its choice kept for later. */
    nav.entry = ORACLES_HOME_AGES;
    nav.screen = ORACLES_SCREEN_GAME;
    nav.row = ORACLES_ROW_EU_ROM;
    nav.eu_action = 1;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.games[ORACLES_HOME_GAME_AGES].eu == ORACLES_ROM_NONE);
    CHECK(fan->eu == ORACLES_ROM_NONE && fan->language == 5 && oracles_page_language(&nav, fan) == 0);
}

/* A refresh or a drop that hides the highlighted row: the highlight goes to the nearest row the page has, so that it
 * shows, and Down and OK act from there. */
static void kept_row(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    nav.screen = ORACLES_SCREEN_GAME;
    OraclesHomeGame *g = &nav.games[0];
    nav.row = ORACLES_ROW_LANGUAGE;
    oracles_page_keep_row(&nav);
    CHECK(nav.row == ORACLES_ROW_LANGUAGE);
    g->rom = ORACLES_ROM_UNRECOGNISED;   /* another ROM dropped on the page */
    oracles_page_keep_row(&nav);
    CHECK(nav.row == ORACLES_ROW_PLAY && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_START_AGES);
    nav.row = ORACLES_ROW_EU_ROM;
    oracles_page_keep_row(&nav);
    CHECK(nav.row == ORACLES_ROW_SAVE);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_ROW_PLAY);
    g->rom = ORACLES_ROM_REFUSED;
    g->usable = 0;
    g->save_file[0] = 0;
    nav.row = ORACLES_ROW_EU_ROM;
    oracles_page_keep_row(&nav);
    CHECK(nav.row == ORACLES_ROW_SAVE && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY);
    /* A fan game, its languages off: no Language row to keep. */
    oracles_home_init(&nav);
    game_1k(&nav);
    nav.row = ORACLES_ROW_LANGUAGE;
    oracles_page_keep_row(&nav);
    CHECK(nav.row == ORACLES_ROW_PLAY);
    /* Elsewhere than a game's page, the row is Display's: left as it is. */
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.row = ORACLES_DISPLAY_VIEW;
    oracles_page_keep_row(&nav);
    CHECK(nav.row == ORACLES_DISPLAY_VIEW);
}

/* Frame 2q: an unrecognised ROM with Enhanced chosen plays in Faithful, and Display counts its sizes and draws its
 * window at 160x144, the profile row saying why. */
static void display_2q(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    nav.games[0].rom = ORACLES_ROM_UNRECOGNISED;
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.display.window = 2;
    CHECK(oracles_display_played_profile(&nav) == ORACLES_PROFILE_FAITHFUL);
    OraclesUiDisplayTexts t;
    OraclesUiDisplayLayout l;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    CHECK(!strcmp(t.profile_note, "Enhanced needs an original ROM: this game plays in Faithful."));
    layout_text("2q", "profile note", &l.profile_note);
    static const char *const sizes[4] = { "320\xc3\x97" "288", "480\xc3\x97" "432", "640\xc3\x97" "576", "7\xc3\x97 \xc2\xb7 1120\xc3\x97" "1008" };
    for (int i = 0; i < 4; i++) {
        char what[32];
        CHECK(!strcmp(t.window_sizes[i], sizes[i]));
        snprintf(what, sizeof what, "window %d size", i); layout_text("2q", what, &l.windows[i].size);
    }
    CHECK(t.window_reduced[0] == 0);
    layout_box("2q", "diagram window", &l.diagram_window);
    layout_text("2q", "diagram label", &l.diagram_label);
    /* A game without a ROM, and a fan game, show the profile chosen. */
    nav.games[0].usable = 0;
    CHECK(oracles_display_played_profile(&nav) == ORACLES_PROFILE_ENHANCED);
    nav.games[0].usable = 1;
    nav.entry = ORACLES_HOME_FAN;
    CHECK(oracles_display_played_profile(&nav) == ORACLES_PROFILE_ENHANCED);
}

/* Display from Moonrise Regalia: its title on two lines puts the diagram under both. */
static void display_2m(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1k(&nav);
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.display.window = 1;
    OraclesUiDisplayTexts t;
    OraclesUiDisplayLayout l;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    layout_text("2m", "title", &l.head.title.lines[0]);
    layout_near("2m", "title box", "h", l.head.title.h);
    layout_box("2m", "diagram", &l.diagram);
    layout_text("2m", "diagram label", &l.diagram_label);
    /* A fan game has the compact text too: its page has the Text link where an Oracle's has it (26e, text link 4:3). */
    CHECK(oracles_display_row_shown(&nav, ORACLES_DISPLAY_TEXT));
    layout_box("26e", "text row", &l.rows[ORACLES_DISPLAY_TEXT]);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    layout_box("text link 4:3", "text row", &l.rows[ORACLES_DISPLAY_TEXT]);
    nav.row = ORACLES_DISPLAY_HUD;
    CHECK(oracles_display_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_TEXT);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_HUD);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_TEXT, -1) == ORACLES_HOME_STAY && nav.text_page && nav.row == ORACLES_DISPLAY_LETTERS);
}

static void layout_fan_page(const fan_frames *f)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_fan(&nav, f);
    char state[ORACLES_HOME_STATE_LENGTH], expected[ORACLES_HOME_TEXT_LENGTH];
    OraclesUiGameTexts t;
    OraclesUiGameLayout l;
    texts_of(&nav, &t, state);
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_16_9, &t, &l);
    const char *p = f->page;
    layout_text(p, "over", &l.head.over);
    CHECK(l.head.title.count == 2 && !strcmp(l.head.title.text[0], f->title[0]) && !strcmp(l.head.title.text[1], f->title[1]));
    layout_text(p, "title", &l.head.title.lines[0]);
    layout_near(p, "title box", "h", l.head.title.h);
    layout_text(p, "state", &l.head.state);
    snprintf(expected, sizeof expected, "Original ROM, %s", f->base_name);
    CHECK(!strcmp(t.rom_status, expected));
    layout_text(p, "base file", &l.rom_file);
    layout_text(p, "base status", &l.rom_status.lines[0]);
    layout_box(p, "base row", &l.rows[ORACLES_ROW_ROM]);
    layout_text(p, "patch file", &l.patch_file);
    layout_text(p, "patch status", &l.patch_status);
    layout_box(p, "patch row", &l.rows[ORACLES_ROW_PATCH]);
    layout_text(p, "patch folder", &l.patch_folder);
    snprintf(expected, sizeof expected, "Patched image recognized: %s", f->file_stem);
    CHECK(!strcmp(t.image_status, expected));
    layout_text(p, "image status", &l.image_status);
    layout_text(p, "save file", &l.save_file);
    layout_text(p, "save line", &l.save_line);
    layout_box(p, "Save row", &l.rows[ORACLES_ROW_SAVE]);
    layout_box(p, "Play row", &l.rows[ORACLES_ROW_PLAY]);
    layout_box(p, "panel", &l.panel);

    /* Without the patch, as Moonrise's page. */
    OraclesHomeGame *g = &nav.games[f->game];
    g->patch = g->image = ORACLES_ROM_NONE;
    g->patch_file[0] = g->patch_folder[0] = g->save_file[0] = 0;
    g->usable = 0;
    texts_of(&nav, &t, state);
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_16_9, &t, &l);
    p = f->bare;
    layout_text(p, "state", &l.head.state);
    layout_text(p, "patch file", &l.patch_file);
    overflowing(p, "patch status", &l.patch_status, "patch row", &l.rows[ORACLES_ROW_PATCH]);
    layout_text(p, "image status", &l.image_status);
    layout_text(p, "save file", &l.save_file);
    layout_text(p, "save line", &l.save_line);
    layout_box(p, "Save row", &l.rows[ORACLES_ROW_SAVE]);
    layout_box(p, "panel", &l.panel);
    /* A base that is not the ROM its patch is made for. */
    char text[ORACLES_HOME_TEXT_LENGTH];
    OraclesTone tone;
    g->rom = ORACLES_ROM_UNRECOGNISED;
    oracles_page_rom_status(g, text, sizeof text, &tone);
    snprintf(expected, sizeof expected, "Unrecognized ROM, not %s", f->base_name);
    CHECK(!strcmp(text, expected) && tone == ORACLES_TONE_WARN);

    /* Display from the fan game. */
    oracles_home_init(&nav);
    game_fan(&nav, f);
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.display.window = 1;
    OraclesUiDisplayTexts d; OraclesUiDisplayLayout dl;
    oracles_ui_display_texts(&nav, &d);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &d, &dl);
    layout_text(f->display, "title", &dl.head.title.lines[0]);
    layout_near(f->display, "title box", "h", dl.head.title.h);
    layout_box(f->display, "diagram", &dl.diagram);
    layout_text(f->display, "diagram label", &dl.diagram_label);
}

/* Quality's row in 16:9 or 4:3: its label, its row, each choice's frame, name and second line, Custom's place laid out
 * whether it shows or not. */
static void display_quality_row(const char *frame, const OraclesUiDisplayTexts *t, const OraclesUiDisplayLayout *l)
{
    static const char *const ids[ORACLES_DISPLAY_QUALITIES + 1] = { "low", "medium", "high", "max", "custom" };
    static const char *const sets[ORACLES_DISPLAY_QUALITIES + 1] = { "Near \xc2\xb7 Fast", "Medium \xc2\xb7 Fast", "Far \xc2\xb7 Fast",
                                                                     "Far \xc2\xb7 Accurate", "By hand" };
    const int wide = layout_reference(frame, "Quality", "h") > 30.0f;   /* 4:3's label is 37 tall */
    if (wide) layout_line(frame, "Quality", &l->labels[ORACLES_DISPLAY_QUALITY].lines[0]);
    else layout_text(frame, "Quality", &l->labels[ORACLES_DISPLAY_QUALITY].lines[0]);
    for (int q = 0; q <= ORACLES_DISPLAY_QUALITIES; q++) {
        char what[48];
        CHECK(!strcmp(t->quality_sets[q], sets[q]));
        snprintf(what, sizeof what, "quality %s", ids[q]); layout_box(frame, what, &l->qualities[q].box);
        snprintf(what, sizeof what, "quality %s name", ids[q]);
        if (wide) layout_line(frame, what, &l->qualities[q].name); else layout_text(frame, what, &l->qualities[q].name);
        snprintf(what, sizeof what, "quality %s sets", ids[q]);
        if (wide) layout_line(frame, what, &l->qualities[q].size); else layout_text(frame, what, &l->qualities[q].size);
    }
    layout_box(frame, wide ? "quality grid" : "quality row", &l->rows[ORACLES_DISPLAY_QUALITY]);
}

/* Window's note and the line on its size under the diagram (16:9, turn 22), against a frame's: the note on two lines in
 * the title's column, the line (`line`, the probe of its text) in a block of two lines kept, 58 tall. */
static void window_notes(const char *frame, const char *line, const OraclesUiDisplayLayout *l)
{
    CHECK(l->window_note.count == 2 && l->window_reduced.count >= 1);
    layout_text(frame, "window note", &l->window_note.lines[0]);
    layout_near(frame, "window note", "top", l->window_note.lines[0].y);
    layout_near(frame, "window note", "h", l->window_note.lines[0].h);
    layout_near(frame, "window note", "width", l->window_note.w);
    layout_text(frame, line, &l->window_reduced.lines[0]);
    layout_near(frame, line, "top", l->window_reduced.lines[0].y);
    layout_near(frame, line, "h", l->window_reduced.lines[0].h);
    layout_near(frame, "fit", "y", l->window_reduced.lines[0].y);
    layout_near(frame, "fit", "h", l->window_reduced.h > 58.0f ? l->window_reduced.h : 58.0f);
}

/* HUD's row in 16:9 against a frame's: its label, its explanation on two lines, Original, Full width and Wide layout. */
static void hud_probes(const char *frame, const OraclesUiDisplayLayout *l)
{
    layout_text(frame, "HUD", &l->labels[ORACLES_DISPLAY_HUD].lines[0]);
    CHECK(l->explanations[6].count == 2);
    layout_near(frame, "hud explanation", "top", l->explanations[6].lines[0].y);
    layout_near(frame, "hud explanation", "width", l->explanations[6].w);
    layout_box(frame, "hud Original", &l->hud[0].box);
    layout_box(frame, "hud Full", &l->hud[1].box);
    layout_box(frame, "hud Wide", &l->hud[2].box);
    layout_text(frame, "hud Original text", &l->hud[0].name);
    layout_text(frame, "hud Full text", &l->hud[1].name);
    layout_text(frame, "hud Wide text", &l->hud[2].name);
    layout_box(frame, "hud row", &l->rows[ORACLES_DISPLAY_HUD]);
    layout_near(frame, "hud grid", "y", l->rows[ORACLES_DISPLAY_HUD].y);
    layout_near(frame, "hud grid", "h", l->rows[ORACLES_DISPLAY_HUD].h);
}

/* The same in 4:3, with the help under the rows: HUD's short line and its empty note's. */
static void hud_probes_4_3(const char *frame, const OraclesUiDisplayLayout *l)
{
    layout_line(frame, "HUD", &l->labels[ORACLES_DISPLAY_HUD].lines[0]);
    layout_box(frame, "hud Original", &l->hud[0].box);
    layout_box(frame, "hud Full", &l->hud[1].box);
    layout_box(frame, "hud Wide", &l->hud[2].box);
    layout_line(frame, "hud Original text", &l->hud[0].name);
    layout_line(frame, "hud Full text", &l->hud[1].name);
    layout_line(frame, "hud Wide text", &l->hud[2].name);
    layout_box(frame, "hud row", &l->rows[ORACLES_DISPLAY_HUD]);
    layout_box(frame, "picture row", &l->rows[ORACLES_DISPLAY_PICTURE]);
    layout_line(frame, "help text", &l->help);
    layout_near(frame, "help", "y", l->help.y);
    layout_near(frame, "help", "h", l->help.h);
    layout_near(frame, "help note", "y", l->help_note.y);
    layout_near(frame, "help note", "h", l->help_note.h);
}

static void display_2a_2b(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.display.window = 2;
    OraclesUiDisplayTexts t;
    OraclesUiDisplayLayout l;
    /* 2a, and 23f: Display as 1.2.0 ships it, measured from the prototype of turn 23 (release 1.2), the same. */
    static const char *const view_probes[3] = { "Near", "Medium", "Far" };
    for (int f = 0; f < 2; f++) {
        const char *frame = f ? "23f" : "2a";
        oracles_ui_display_texts(&nav, &t);
        oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
        /* No state line under the title: the diagram comes 56 px under it. */
        layout_text(frame, "section", &l.head.section);
        layout_box(frame, "diagram", &l.diagram);
        /* The diagram shows the window Play will open: 4x reduced to 3x, 1440x810, centred on the screen. */
        layout_box(frame, "diagram window", &l.diagram_window);
        layout_text(frame, "diagram label", &l.diagram_label);
        CHECK(!strcmp(t.diagram_label, "1440\xc3\x97" "810 on a 1920\xc3\x97" "1080 screen"));
        /* Profile first: the two profiles framed with their surfaces, the chosen one's note. */
        layout_text(frame, "Profile", &l.labels[ORACLES_DISPLAY_PROFILE].lines[0]);
        for (int p = 0; p < 2; p++) {
            char what[32];
            snprintf(what, sizeof what, "profile %d", p); layout_text(frame, what, &l.profiles[p].name);
            snprintf(what, sizeof what, "profile %d size", p); layout_text(frame, what, &l.profiles[p].size);
        }
        layout_text(frame, "profile note", &l.profile_note);
        layout_box(frame, "Profile row", &l.rows[ORACLES_DISPLAY_PROFILE]);
        /* Quality under it: its four qualities and Custom's place, whichever is in effect. */
        display_quality_row(frame, &t, &l);
        /* Window next. */
        layout_text(frame, "Window", &l.labels[ORACLES_DISPLAY_WINDOW].lines[0]);
        static const char *const sizes[4] = { "960\xc3\x97" "540", "1440\xc3\x97" "810", "1920\xc3\x97" "1080", "4\xc3\x97 \xc2\xb7 1920\xc3\x97" "1080" };
        for (int i = 0; i < 4; i++) {
            char what[32];
            snprintf(what, sizeof what, "window %d", i); layout_text(frame, what, &l.windows[i].name);
            snprintf(what, sizeof what, "window %d size", i); layout_text(frame, what, &l.windows[i].size);
            CHECK(!strcmp(t.window_sizes[i], sizes[i]));
        }
        /* 4x makes 1920x1080: with its title bar the window is reduced to 3x on a 1080p screen, and a line says so, under
         * the diagram with Window's note; the row holds the windows alone. */
        CHECK(!strcmp(t.window_reduced, "Reduced to 3\xc3\x97 to fit this screen.") && oracles_display_fit(&nav) == 3);
        CHECK(t.window_fit == t.window_reduced);
        window_notes(frame, "reduced", &l);
        layout_box(frame, "Window row", &l.rows[ORACLES_DISPLAY_WINDOW]);
        /* View under it: each level framed with its size, far chosen; its explanation on two lines beside them. */
        CHECK(nav.display.view == 2 && l.view_explanation.count == 2);
        layout_text(frame, "View", &l.labels[ORACLES_DISPLAY_VIEW].lines[0]);
        layout_near(frame, "view explanation", "top", l.view_explanation.lines[0].y);
        layout_near(frame, "view explanation", "width", l.view_explanation.w);
        for (int v = 0; v < 3; v++) {
            char what[32];
            snprintf(what, sizeof what, "view %d", v); layout_text(frame, what, &l.views[v].name);
            snprintf(what, sizeof what, "view %d size", v); layout_text(frame, what, &l.views[v].size);
            snprintf(what, sizeof what, "view %s", view_probes[v]); layout_box(frame, what, &l.views[v].box);
        }
        CHECK(!strcmp(t.view_sizes[1], "384\xc3\x97" "216") && !strcmp(t.profile_sizes[ORACLES_PROFILE_ENHANCED], "480\xc3\x97" "270"));
        layout_box(frame, "View row", &l.rows[ORACLES_DISPLAY_VIEW]);
        /* HUD under it, Full width chosen: its explanation on two lines beside Original and Full width. */
        CHECK(nav.display.hud == 1);
        hud_probes(frame, &l);
        /* Picture, in Color correction's former place (turn 34): the panel's last row, a link with its summary; Color
         * correction is on its page. */
        CHECK(l.rows[ORACLES_DISPLAY_COLOUR].w == 0.0f && l.colour[0].box.w == 0.0f);
        layout_text(frame, "Picture", &l.labels[ORACLES_DISPLAY_PICTURE].lines[0]);
        layout_box(frame, "picture link", &l.picture_link);
        layout_box(frame, "picture row", &l.rows[ORACLES_DISPLAY_PICTURE]);
        /* Advanced sits under the diagram, its highlight past its label on both sides; the Advanced rows, the transitions
         * among them, are not laid out. */
        CHECK(l.rows[ORACLES_DISPLAY_TRANSITIONS].w == 0.0f && l.transitions_note.h == 0.0f);
        layout_box(frame, "panel", &l.panel);
        layout_text(frame, "Advanced", &l.advanced_label);
        layout_near(frame, "Advanced", "top", l.advanced_label.y);
        layout_box(frame, "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);
        CHECK(l.rows[ORACLES_DISPLAY_CORE].w == 0.0f && l.rows[ORACLES_DISPLAY_VSYNC].w == 0.0f && l.rows[ORACLES_DISPLAY_WORKERS].w == 0.0f);
        CHECK(!strcmp(t.section, "Display"));
    }

    /* 2g: opened from the game, a note under the title says that the dimmed rows change from the home screen, once for
     * the page, on a line in the left column, which moves down under it (turn 29); color correction applies at once.
     * Profile, Quality and View, which the game keeps, are dimmed and inert, "Change it from the home screen." in place
     * of Profile's and Quality's notes and under View's explanation. */
    nav.in_game = 1;
    nav.playing = ORACLES_PROFILE_ENHANCED;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    CHECK(!strcmp(t.profile_note, "Change it from the home screen.") && t.quality_note == t.profile_note && t.view_note == t.profile_note);
    layout_text("2g", "profile note", &l.profile_note);
    layout_text("2g", "quality note", &l.quality_note);
    layout_text("2g", "view reason", &l.view_note);
    layout_near("2g", "view explanation", "top", l.view_explanation.lines[0].y);
    layout_box("2g", "quality row", &l.rows[ORACLES_DISPLAY_QUALITY]);
    layout_box("2g", "Text row", &l.rows[ORACLES_DISPLAY_TEXT]);
    for (unsigned r = ORACLES_DISPLAY_PROFILE; r <= ORACLES_DISPLAY_ADVANCED; r++)
        CHECK(oracles_display_row_fixed(&nav, r) == (r == ORACLES_DISPLAY_PROFILE || r == ORACLES_DISPLAY_QUALITY || r == ORACLES_DISPLAY_VIEW));
    {
        const unsigned profile = nav.display.profile, view = nav.display.view;
        nav.row = ORACLES_DISPLAY_PROFILE;
        CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && oracles_display_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY);
        CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_PROFILE, 0) == ORACLES_HOME_STAY && nav.display.profile == profile);
        nav.row = ORACLES_DISPLAY_VIEW;
        CHECK(oracles_display_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && oracles_display_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY);
        CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_VIEW, 0) == ORACLES_HOME_STAY && nav.display.view == (int)view);
        nav.row = ORACLES_DISPLAY_QUALITY;
        CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && oracles_display_click(&nav, ORACLES_DISPLAY_QUALITY, 0) == ORACLES_HOME_STAY);
        /* The Text and Picture links open all the same, and Color correction changes there. */
        CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_TEXT, -1) == ORACLES_HOME_STAY && nav.text_page);
        oracles_display_act(&nav, ORACLES_HOME_BACK);
        CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_PICTURE, -1) == ORACLES_HOME_STAY && nav.picture_page);
        CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_COLOUR, 1) == ORACLES_HOME_STORE && nav.display.colour == 1);
        oracles_display_act(&nav, ORACLES_HOME_BACK);
        nav.display.colour = 0;
        nav.row = ORACLES_DISPLAY_PROFILE;
        oracles_ui_display_texts(&nav, &t);
        oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    }
    layout_line("2g", "page note", &l.page_note.lines[0]);
    layout_near("2g", "page note", "lines", (float)l.page_note.count);
    layout_near("2g", "page note", "width", l.page_note.w);
    CHECK(l.page_note.lines[1].x + l.page_note.lines[1].w <= 96.0f + 520.0f && l.page_note.lines[0].x + l.page_note.lines[0].w <= 96.0f + 520.0f);
    layout_near("2g", "Window row", "y", l.rows[ORACLES_DISPLAY_WINDOW].y);
    layout_near("2g", "View row", "y", l.rows[ORACLES_DISPLAY_VIEW].y);
    layout_near("2g", "View row", "h", l.rows[ORACLES_DISPLAY_VIEW].h);
    layout_near("2g", "picture row", "y", l.rows[ORACLES_DISPLAY_PICTURE].y);
    layout_box("2g", "hud row", &l.rows[ORACLES_DISPLAY_HUD]);
    layout_box("2g", "hud Original", &l.hud[0].box);
    layout_box("2g", "panel", &l.panel);
    /* Advanced under the diagram, which the note moved down. */
    layout_text("2g", "Advanced", &l.advanced_label);
    layout_box("2g", "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);
    /* Its rows end above the pause's key hints, which start at 1025 in 1080p. */
    CHECK(l.rows[ORACLES_DISPLAY_PICTURE].y + l.rows[ORACLES_DISPLAY_PICTURE].h <= 1025.0f);
    nav.in_game = 0;

    /* 2b: Faithful; the window fullscreen, seven times on a 1080p screen, never reduced. */
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    nav.display.window = 3;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    CHECK(!oracles_display_transitions_apply(&nav));
    CHECK(!strcmp(t.window_sizes[3], "7\xc3\x97 \xc2\xb7 1120\xc3\x97" "1008") && oracles_display_scale(&nav, 3) == 7);
    layout_box("2b", "diagram window", &l.diagram_window);
    layout_text("2b", "diagram label", &l.diagram_label);
    layout_text("2b", "profile note", &l.profile_note);
    layout_text("2b", "window 2", &l.windows[2].name);
    layout_text("2b", "window 3", &l.windows[3].name);
    layout_box("2b", "hud Original", &l.hud[0].box);
    layout_box("2b", "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);
    layout_box("2b", "panel", &l.panel);

    /* 2r: the medium view; Enhanced's size under its profile, the windows and the diagram follow it. */
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.display.view = 1;
    nav.display.window = 2;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    layout_text("2r", "profile 1 size", &l.profiles[ORACLES_PROFILE_ENHANCED].size);
    layout_text("2r", "view 1 size", &l.views[1].size);
    layout_box("2r", "view Medium", &l.views[1].box);
    for (int i = 0; i < 4; i++) {
        char what[32];
        snprintf(what, sizeof what, "window %d size", i);
        layout_text("2r", what, &l.windows[i].size);
    }
    layout_box("2r", "diagram window", &l.diagram_window);
    layout_text("2r", "diagram label", &l.diagram_label);
    /* 2s: the same on a 4:3 screen, which the layout reference takes as 1440x1080: the view's 4:3 sizes, the windows counted
     * in them, the diagram's box narrower. */
    nav.display.screen_w = 1440;
    nav.display.room_w = 1440;
    nav.display.window = 3;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    CHECK(oracles_display_screen_4_3(&nav) && !strcmp(t.view_sizes[0], "213\xc3\x97" "160") && !strcmp(t.view_sizes[2], "480\xc3\x97" "360"));
    /* The settings' aspect= names the shape whatever the screen: 16:9 on this 4:3 one, 4:3 on a 16:9 one. */
    nav.display.aspect = 1;
    CHECK(!oracles_display_screen_4_3(&nav));
    nav.display.screen_w = 1920;
    nav.display.aspect = 2;
    CHECK(oracles_display_screen_4_3(&nav));
    nav.display.aspect = 0;
    CHECK(!oracles_display_screen_4_3(&nav));
    nav.display.screen_w = 1440;
    layout_text("2s", "profile 1 size", &l.profiles[ORACLES_PROFILE_ENHANCED].size);
    for (int v = 0; v < 3; v++) {
        char what[32];
        snprintf(what, sizeof what, "view %d size", v); layout_text("2s", what, &l.views[v].size);
        snprintf(what, sizeof what, "view %s", view_probes[v]); layout_box("2s", what, &l.views[v].box);
    }
    for (int i = 0; i < 4; i++) {
        char what[32];
        snprintf(what, sizeof what, "window %d size", i);
        layout_text("2s", what, &l.windows[i].size);
    }
    layout_box("2s", "diagram", &l.diagram);
    layout_box("2s", "diagram window", &l.diagram_window);
    layout_text("2s", "diagram label", &l.diagram_label);
}

/* The Advanced rows of a frame: Core with its two cores, its explanation on two lines and its note on savestates;
 * the continuous transitions, their label and explanation on two lines and their note; Vsync; the neighbour workers,
 * their label and explanation on two lines, Auto naming the two of an eight-core device. */
static void display_advanced_rows(const char *frame, const OraclesUiDisplayTexts *t, const OraclesUiDisplayLayout *l)
{
    layout_text(frame, "section", &l->head.section);
    CHECK(!strcmp(t->section, "Display \xe2\x80\xba Advanced"));
    CHECK(l->rows[ORACLES_DISPLAY_PROFILE].w == 0.0f && l->rows[ORACLES_DISPLAY_HUD].w == 0.0f && l->rows[ORACLES_DISPLAY_ADVANCED].w == 0.0f
          && l->link == ORACLES_DISPLAY_ROWS && l->rows[ORACLES_DISPLAY_PICTURE].w == 0.0f && l->rows[ORACLES_DISPLAY_SCALING].w == 0.0f);
    CHECK(l->labels[ORACLES_DISPLAY_TRANSITIONS].count == 2 && l->explanations[1].count == 2);
    layout_near(frame, "Continuous transitions", "top", l->labels[ORACLES_DISPLAY_TRANSITIONS].lines[0].y);
    layout_near(frame, "Continuous transitions", "width", l->labels[ORACLES_DISPLAY_TRANSITIONS].w);
    layout_near(frame, "transitions explanation", "top", l->explanations[1].lines[0].y);
    layout_near(frame, "transitions explanation", "width", l->explanations[1].w);
    layout_text(frame, "transitions note", &l->transitions_note);
    layout_box(frame, "transitions Off", &l->transitions[0].box);
    layout_box(frame, "transitions On", &l->transitions[1].box);
    layout_box(frame, "transitions row", &l->rows[ORACLES_DISPLAY_TRANSITIONS]);
    CHECK(l->explanations[3].count == 2);
    layout_text(frame, "Core", &l->labels[ORACLES_DISPLAY_CORE].lines[0]);
    layout_text(frame, "core explanation", &l->explanations[3].lines[0]);
    layout_near(frame, "core explanation", "width", l->explanations[3].w);
    layout_text(frame, "core note", &l->core_note);
    layout_text(frame, "core 0", &l->core[0].name);
    layout_text(frame, "core 1", &l->core[1].name);
    layout_box(frame, "core Accurate", &l->core[0].box);
    layout_box(frame, "core Fast", &l->core[1].box);
    layout_box(frame, "core row", &l->rows[ORACLES_DISPLAY_CORE]);
    layout_text(frame, "Vsync", &l->labels[ORACLES_DISPLAY_VSYNC].lines[0]);
    layout_text(frame, "vsync explanation", &l->explanations[2].lines[0]);
    layout_box(frame, "vsync Auto", &l->vsync[0].box);
    layout_box(frame, "vsync Off", &l->vsync[2].box);
    layout_box(frame, "vsync row", &l->rows[ORACLES_DISPLAY_VSYNC]);
    CHECK(l->labels[ORACLES_DISPLAY_WORKERS].count == 2 && l->explanations[4].count == 2);
    layout_text(frame, "Neighbour workers", &l->labels[ORACLES_DISPLAY_WORKERS].lines[0]);
    layout_near(frame, "Neighbour workers", "width", l->labels[ORACLES_DISPLAY_WORKERS].w);
    layout_text(frame, "workers explanation", &l->explanations[4].lines[0]);
    layout_near(frame, "workers explanation", "width", l->explanations[4].w);
    CHECK(!strcmp(t->workers_names[0], "Auto \xc2\xb7 2") && !strcmp(t->workers_names[1], "1") && !strcmp(t->workers_names[2], "2"));
    static const char *const workers[3] = { "Auto", "One", "Two" };
    for (int i = 0; i < 3; i++) {
        char what[32];
        snprintf(what, sizeof what, "workers %d", i); layout_text(frame, what, &l->workers[i].name);
        snprintf(what, sizeof what, "workers %s", workers[i]); layout_box(frame, what, &l->workers[i].box);
    }
    layout_box(frame, "workers row", &l->rows[ORACLES_DISPLAY_WORKERS]);
    layout_box(frame, "panel", &l->panel);
}

/* 9a: Display with Advanced highlighted; 9b: its Advanced rows from the home screen; 9c: from the game, the note under
 * the title. */
static void display_9a_9b_9c(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.display.window = 2;
    nav.display.cores = 8;
    nav.row = ORACLES_DISPLAY_ADVANCED;
    OraclesUiDisplayTexts t;
    OraclesUiDisplayLayout l;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    layout_text("9a", "Advanced", &l.advanced_label);
    layout_box("9a", "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);
    layout_text("9a", "diagram label", &l.diagram_label);
    /* Its hints: Enter or A opens it; left and right change nothing there. */
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    OraclesUiHintLayout h[ORACLES_HOME_MAX_HINTS];
    CHECK(oracles_home_hints(&nav, hints) == 3 && !strcmp(hints[0].label, "Move") && !strcmp(hints[1].label, "Open") && hints[2].back);
    oracles_ui_layout_hints(ORACLES_UI_LAYOUT_16_9, hints, 3, h);
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof name, "hint %d", i); layout_box("9a", name, &h[i].box);
        snprintf(name, sizeof name, "hint %d key", i); layout_box("9a", name, &h[i].key_box);
        snprintf(name, sizeof name, "hint %d key text", i); layout_line("9a", name, &h[i].key);
        snprintf(name, sizeof name, "hint %d label", i); layout_line("9a", name, &h[i].label);
    }

    nav.advanced = 1;
    nav.row = ORACLES_DISPLAY_CORE;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    display_advanced_rows("9b", &t, &l);
    layout_box("9b", "diagram", &l.diagram);
    layout_text("9b", "diagram label", &l.diagram_label);

    nav.in_game = 1;
    nav.playing = ORACLES_PROFILE_ENHANCED;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    display_advanced_rows("9c", &t, &l);
    layout_text("9c", "page note", &l.page_note.lines[0]);
    layout_box("9c", "diagram", &l.diagram);
    /* From the game (turn 29) Core, the transitions and the workers, which it keeps, are dimmed and inert, their reason
     * in place of their notes, the workers' row growing by its line; Vsync (turn 32), Scaling, the camera and the margin
     * change. */
    layout_text("9c", "workers note", &l.workers_note);
    CHECK(!strcmp(t.core_note, "Change it from the home screen.") && t.transitions_note == t.core_note && !t.vsync_note
          && t.workers_note == t.core_note && l.vsync_note.w == 0.0f);
    static const unsigned advanced[5] = { ORACLES_DISPLAY_CORE, ORACLES_DISPLAY_TRANSITIONS, ORACLES_DISPLAY_VSYNC, ORACLES_DISPLAY_WORKERS, ORACLES_DISPLAY_CAMERA };
    for (unsigned i = 0; i < 5u; i++) {
        const unsigned r = advanced[i];
        const int locked = r == ORACLES_DISPLAY_CORE || r == ORACLES_DISPLAY_TRANSITIONS || r == ORACLES_DISPLAY_WORKERS;
        nav.row = r;
        CHECK(oracles_display_row_fixed(&nav, r) == locked);
        CHECK((oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY) == locked);
        if (locked) CHECK(oracles_display_click(&nav, r, 1) == ORACLES_HOME_STAY);
    }
    nav.display.scaling = nav.display.margin = 0;
    nav.display.camera = -1;
    nav.row = ORACLES_DISPLAY_CORE;
    /* Auto names the one worker of a device under four cores. */
    nav.display.cores = 2;
    oracles_ui_display_texts(&nav, &t);
    CHECK(!strcmp(t.workers_names[0], "Auto \xc2\xb7 1"));
}

static void display_navigation(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    /* Display opens from the menu on Profile; Back returns to Display. */
    nav.focus = 3;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_DISPLAY && nav.row == ORACLES_DISPLAY_PROFILE);
    /* The profile goes between Faithful and Enhanced for every game, and asks to be stored. */
    CHECK(nav.display.profile == ORACLES_PROFILE_ENHANCED);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.profile == ORACLES_PROFILE_FAITHFUL);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.display.profile == ORACLES_PROFILE_ENHANCED);
    /* Quality under it (display_quality_navigation); the window goes round 2x, 3x, 4x, fullscreen. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_DISPLAY_QUALITY);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_DISPLAY_WINDOW && nav.display.window == 2);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.window == 3);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.display.window == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.display.window == 3);
    /* View goes round near, medium, far, far first. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_DISPLAY_VIEW && nav.display.view == 2);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.view == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.display.view == 2);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.display.view == 1);
    /* The HUD, Full width first, goes round Original, Full width and Wide layout in Enhanced, from a game too (it applies
     * at once), and not in Faithful. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_DISPLAY_HUD && nav.display.hud == 1);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.hud == 2);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.hud == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.display.hud == 2);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.hud == 0);
    nav.in_game = 1;
    nav.playing = ORACLES_PROFILE_ENHANCED;
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_HUD, 1) == ORACLES_HOME_STORE && nav.display.hud == 1 && !oracles_display_row_fixed(&nav, ORACLES_DISPLAY_HUD));
    nav.in_game = 0;
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.display.hud == 1);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_HUD, 0) == ORACLES_HOME_STAY && nav.display.hud == 1);
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    /* Text, a link: left does nothing, right and OK open Display › Text on its Text row, Back returns on the link
     * (test_launcher_text_page.c has the page). */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_DISPLAY_TEXT && oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && !nav.text_page);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.text_page && nav.row == ORACLES_DISPLAY_LETTERS);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && !nav.text_page && nav.row == ORACLES_DISPLAY_TEXT);
    /* Picture, a link too (turn 34), in Color correction's former place: right opens Display › Picture on Scaling, Back
     * returns on the link (test_launcher_picture.c has the page). */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_DISPLAY_PICTURE && oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && !nav.picture_page);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.picture_page && nav.row == ORACLES_DISPLAY_SCALING);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && !nav.picture_page && nav.row == ORACLES_DISPLAY_PICTURE);
    /* Advanced after Picture, last: left does nothing there, right and OK open its rows on Core.  Down from it wraps to
     * Profile, up from Profile to it. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_DISPLAY_ADVANCED && !nav.advanced);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && !nav.advanced);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_PROFILE);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_ADVANCED);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.advanced && nav.row == ORACLES_DISPLAY_CORE);
    CHECK(!strcmp(oracles_display_section(&nav), "Display \xe2\x80\xba Advanced"));
    /* Its five rows (turn 34): Core, the transitions, Vsync, the workers, the camera, going round; Display's own, the
     * Picture rows and the Text rows are not there. */
    CHECK(oracles_display_row_shown(&nav, ORACLES_DISPLAY_WORKERS) && !oracles_display_row_shown(&nav, ORACLES_DISPLAY_PROFILE)
          && !oracles_display_row_shown(&nav, ORACLES_DISPLAY_ADVANCED) && !oracles_display_row_shown(&nav, ORACLES_DISPLAY_PICTURE)
          && !oracles_display_row_shown(&nav, ORACLES_DISPLAY_SCALING) && !oracles_display_row_shown(&nav, ORACLES_DISPLAY_MARGIN)
          && !oracles_display_row_shown(&nav, ORACLES_DISPLAY_LETTERS) && !oracles_display_row_shown(&nav, ORACLES_DISPLAY_MENUS));
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_PROFILE, 0) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_CORE);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_CAMERA);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_WORKERS);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_CAMERA);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_CORE);
    /* The two cores by a key and by a click; from a game the core is the game's and does not change. */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.core == 1);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_CORE, 0) == ORACLES_HOME_STORE && nav.display.core == 0);
    nav.in_game = 1;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.display.core == 0);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_CORE, 1) == ORACLES_HOME_STAY && nav.display.core == 0);
    nav.in_game = 0;
    /* The transitions under Core toggle in Enhanced, not from a game, which keeps them, nor in Faithful. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_DISPLAY_TRANSITIONS && nav.display.transitions == 1);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.display.transitions == 0);
    nav.in_game = 1;
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_TRANSITIONS, 1) == ORACLES_HOME_STAY && nav.display.transitions == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && nav.display.transitions == 0);
    nav.in_game = 0;
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.display.transitions == 0);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_TRANSITIONS, 1) == ORACLES_HOME_STAY);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_DISPLAY_VSYNC && oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.display.vsync == 2);
    /* Vsync changes from a game too, which takes it at once (turn 32). */
    nav.in_game = 1;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.vsync == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.display.vsync == 2);
    nav.in_game = 0;
    /* The workers go round Auto, 1, 2, by a key and by a click; from a game they are the game's and do not change. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_DISPLAY_WORKERS && nav.display.workers == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.display.workers == 2);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.display.workers == 0);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_WORKERS, 1) == ORACLES_HOME_STORE && nav.display.workers == 1);
    nav.in_game = 1;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.display.workers == 1);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_WORKERS, 2) == ORACLES_HOME_STAY && nav.display.workers == 1);
    nav.in_game = 0;
    /* The camera, Pixel then Smooth, the platform's default shown until one is chosen (Smooth on the desktops, on
     * Android from three times on): the shown value changes nothing, the other is stored; from a game too, where it
     * applies at once, and not in Faithful, nor from a game running in Faithful. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_DISPLAY_CAMERA && nav.display.camera == -1 && oracles_display_camera(&nav) == 1 && !oracles_display_row_fixed(&nav, ORACLES_DISPLAY_CAMERA));
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    CHECK(!oracles_display_camera_applies(&nav) && oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && nav.display.camera == -1);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_CAMERA, 0) == ORACLES_HOME_STAY && nav.display.camera == -1);
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_CAMERA, 1) == ORACLES_HOME_STAY && nav.display.camera == -1);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.camera == 0 && oracles_display_camera(&nav) == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.display.camera == 1);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_CAMERA, 0) == ORACLES_HOME_STORE && nav.display.camera == 0);
    nav.in_game = 1;
    nav.playing = ORACLES_PROFILE_ENHANCED;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.display.camera == 1 && !oracles_display_row_fixed(&nav, ORACLES_DISPLAY_CAMERA));
    nav.playing = ORACLES_PROFILE_FAITHFUL;
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_CAMERA, 0) == ORACLES_HOME_STAY && nav.display.camera == 1);
    nav.in_game = 0;
    nav.display.camera = -1;
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_CORE);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_CAMERA);
    /* Back closes them on Advanced; a click on Advanced opens them again, Back closes them. */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_DISPLAY && !nav.advanced
          && nav.row == ORACLES_DISPLAY_ADVANCED && !strcmp(oracles_display_section(&nav), "Display"));
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_ADVANCED, -1) == ORACLES_HOME_STAY && nav.advanced && nav.row == ORACLES_DISPLAY_CORE);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_VSYNC, -1) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_VSYNC);
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(!nav.advanced && nav.row == ORACLES_DISPLAY_ADVANCED);
    /* Display › Picture's scaling and screen margin, which turn 34 took from Advanced. */
    nav.row = ORACLES_DISPLAY_PICTURE;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.picture_page && nav.row == ORACLES_DISPLAY_SCALING);
    oracles_home_act(&nav, ORACLES_HOME_UP);
    /* The scaling first, Sharp then Fill, by a key and by a click; from a game too, where it applies at once. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_DISPLAY_SCALING && nav.display.scaling == 0 && !oracles_display_row_fixed(&nav, ORACLES_DISPLAY_SCALING));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.scaling == 1);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.display.scaling == 0);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_SCALING, 1) == ORACLES_HOME_STORE && nav.display.scaling == 1);
    nav.in_game = 1;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.display.scaling == 0);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_SCALING, 1) == ORACLES_HOME_STORE && nav.display.scaling == 1);
    CHECK(!oracles_display_row_fixed(&nav, ORACLES_DISPLAY_SCALING));
    nav.in_game = 0;
    nav.display.scaling = 0;
    /* The screen margin under it, Off, Auto, Small, Large, going round, by a key and by a click; in Faithful and from a game
     * too, where it applies at once. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_DISPLAY_MARGIN && nav.display.margin == 0 && !oracles_display_row_fixed(&nav, ORACLES_DISPLAY_MARGIN));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.display.margin == 3);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.display.margin == 0);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_MARGIN, 2) == ORACLES_HOME_STORE && nav.display.margin == 2);
    nav.in_game = 1;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.margin == 3);
    nav.in_game = 0;
    CHECK(nav.display.profile == ORACLES_PROFILE_FAITHFUL && oracles_display_click(&nav, ORACLES_DISPLAY_MARGIN, 1) == ORACLES_HOME_STORE
          && nav.display.margin == 1);
    nav.display.margin = 0;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_COLOUR);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && !nav.picture_page && nav.row == ORACLES_DISPLAY_PICTURE);
    nav.row = ORACLES_DISPLAY_ADVANCED;

    /* From a game, Advanced opens on Vsync, the first of its rows that changes there, by a key as by a click. */
    nav.in_game = 1;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.advanced && nav.row == ORACLES_DISPLAY_VSYNC);
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_ADVANCED, -1) == ORACLES_HOME_STAY && nav.advanced && nav.row == ORACLES_DISPLAY_VSYNC);
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    nav.in_game = 0;
    CHECK(!nav.advanced && nav.row == ORACLES_DISPLAY_ADVANCED);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_DISPLAY_PROFILE);
    /* In Faithful, View does not change either, by a click as by a key. */
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_VIEW, 0) == ORACLES_HOME_STAY && nav.display.view == 1);
    /* A click on a choice sets it. */
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_PROFILE, ORACLES_PROFILE_ENHANCED) == ORACLES_HOME_STORE && nav.display.profile == ORACLES_PROFILE_ENHANCED);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_WINDOW, 1) == ORACLES_HOME_STORE && nav.display.window == 1 && nav.row == ORACLES_DISPLAY_WINDOW);
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(nav.row == ORACLES_DISPLAY_QUALITY);
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(nav.row == ORACLES_DISPLAY_PROFILE);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_HOME && oracles_home_focus(&nav) == 3);
    /* Display opens on its own rows, whatever it was left on. */
    nav.advanced = 1;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_DISPLAY && !nav.advanced
          && nav.row == ORACLES_DISPLAY_PROFILE);
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    /* Display's sizes follow the display: fullscreen on a 2560x1440 screen is 10x Faithful. */
    nav.display.screen_w = 2560;
    nav.display.screen_h = 1440;
    CHECK(oracles_display_scale(&nav, 3) == 10);
    /* A window that fits says nothing; one reduced says to what. */
    char reduced[64];
    nav.display.room_w = 2560;
    nav.display.room_h = 1440 - 64;
    nav.display.window = 2;
    oracles_display_reduced(&nav, reduced, sizeof reduced);
    CHECK(reduced[0] == 0);
    nav.display.room_h = 500;
    oracles_display_reduced(&nav, reduced, sizeof reduced);
    CHECK(!strcmp(reduced, "Reduced to 3\xc3\x97 to fit this screen."));
    nav.display.window = 3;
    oracles_display_reduced(&nav, reduced, sizeof reduced);
    CHECK(reduced[0] == 0);   /* fullscreen is never reduced */
}

/* Whole UTF-8 sequences only: no continuation byte without its lead, no lead without its continuations. */
static int valid_utf8(const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text; *p;) {
        const int n = *p < 0x80u ? 1 : (*p & 0xe0u) == 0xc0u ? 2 : (*p & 0xf0u) == 0xe0u ? 3 : (*p & 0xf8u) == 0xf0u ? 4 : 0;
        if (!n) return 0;
        for (int k = 1; k < n; k++) if ((p[k] & 0xc0u) != 0x80u) return 0;
        p += n;
    }
    return 1;
}

static int ellipses(const char *text)
{
    int n = 0;
    for (const char *p = text; (p = strstr(p, "\xe2\x80\xa6")) != NULL; p += 3) n++;
    return n;
}

/* A folder longer than its field keeps its end, the part a player recognises; the page elides it once, in its middle,
 * whole characters, and quickly even when long. */
/* On Android, a folder on the shared storage shows from its Android/data, one of the application's private files is
 * named. */
static void android_folders(void)
{
    static const char *const cases[][2] = {
        { "/storage/emulated/0/Android/data/org.theoraclesproject/files/roms", "Android/data/org.theoraclesproject/files/roms" },
        { "/storage/emulated/0/Android/data/org.theoraclesproject/files/patches", "Android/data/org.theoraclesproject/files/patches" },
        { "/data/data/org.theoraclesproject/files/mods", "App storage \xe2\x80\xba mods" },
        { "/data/user/0/org.theoraclesproject/files/mods/claw-game", "App storage \xe2\x80\xba mods \xe2\x80\xba claw-game" },
        { "/data/data/org.theoraclesproject/files", "App storage" },
        { "/storage/emulated/0/Download", "/storage/emulated/0/Download" },
        { "/data/data/org.theoraclesproject/cache", "/data/data/org.theoraclesproject/cache" },
        { "/data/data/org.theoraclesproject/filesystem", "/data/data/org.theoraclesproject/filesystem" },
        { "", "" },
    };
    char out[ORACLES_HOME_TEXT_LENGTH * 4];
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        oracles_page_folder(1, cases[i][0], out, sizeof out);
        CHECK(!strcmp(out, cases[i][1]));
        oracles_page_folder(0, cases[i][0], out, sizeof out);
        CHECK(!strcmp(out, cases[i][0]));
    }
    char small[20];
    oracles_page_folder(1, cases[1][0], small, sizeof small);
    CHECK(strlen(small) < sizeof small && !strncmp(small, "Android/data", 12));
}

static void long_paths(void)
{
    static char path[3200], field[ORACLES_HOME_TEXT_LENGTH * 4], fitted[ORACLES_HOME_TEXT_LENGTH * 4];
    size_t n = (size_t)snprintf(path, sizeof path, "C:\\Users\\sam\\Games");
    while (n < 3000) n += (size_t)snprintf(path + n, sizeof path - n, "\\D\xc3\xa9p\xc3\xb4t %zu", n);
    n += (size_t)snprintf(path + n, sizeof path - n, "\\Oracles, the real end");
    oracles_ui_copy_middle(field, sizeof field, path, n);
    CHECK(strlen(field) < sizeof field && strlen(field) + 4 >= sizeof field && valid_utf8(field) && ellipses(field) == 1);
    CHECK(!strncmp(field, "C:\\Users\\sam\\Games", 18) && strstr(field, "\\Oracles, the real end") == field + strlen(field) - 22);
    oracles_ui_copy_middle(fitted, sizeof fitted, "short", 5);
    CHECK(!strcmp(fitted, "short"));
    /* A toast's note after its message, a file's name in it: cut in its middle on whole characters, its end kept. */
    char toast[256] = "Route written to C:\\routes\\ages.route";
    char note[400];
    snprintf(note, sizeof note, "%.300s is not the European ROM of Oracle of Ages: the game played in English.", strchr(path + 2850, '\\'));
    oracles_ui_append_middle(toast, sizeof toast, note);
    CHECK(strlen(toast) < sizeof toast && valid_utf8(toast) && ellipses(toast) == 1 && !strncmp(toast, "Route written to C:\\routes\\ages.route ", 38));
    CHECK(strstr(toast, "the game played in English.") == toast + strlen(toast) - 27);
    snprintf(toast, sizeof toast, "%s", "");
    oracles_ui_append_middle(toast, sizeof toast, "short");
    CHECK(!strcmp(toast, "short"));

    const float room = 700.0f;
    clock_t start = clock();
    oracles_ui_fit(&oracles_ui_row_path, field, room, fitted, sizeof fitted);
    const double first_ms = (double)(clock() - start) * 1000.0 / CLOCKS_PER_SEC;
    CHECK(oracles_ui_text_width(&oracles_ui_row_path, fitted) <= room && valid_utf8(fitted) && ellipses(fitted) == 1);
    CHECK(!strncmp(fitted, "C:\\Users", 8) && strstr(fitted, "the real end") == fitted + strlen(fitted) - 12);
    /* Drawn again at the next frame: the same text is not elided again. */
    char again[sizeof fitted];
    start = clock();
    for (int frame = 0; frame < 100; frame++) oracles_ui_fit(&oracles_ui_row_path, field, room, again, sizeof again);
    const double hundred_ms = (double)(clock() - start) * 1000.0 / CLOCKS_PER_SEC;
    CHECK(!strcmp(again, fitted));
    printf("long path: a %zu-byte folder elided in %.2f ms, then 100 frames in %.2f ms\n", strlen(field), first_ms, hundred_ms);

    /* A text that fits, and one of accented letters elided between whole characters. */
    oracles_ui_fit(&oracles_ui_row_title, "Oracle of Ages.gbc", room, fitted, sizeof fitted);
    CHECK(!strcmp(fitted, "Oracle of Ages.gbc"));
    char accents[600] = "";
    for (int i = 0; i < 200; i++) strcat(accents, "\xc3\xa9");
    oracles_ui_fit(&oracles_ui_row_title, accents, 300.0f, fitted, 64);
    CHECK(valid_utf8(fitted) && ellipses(fitted) == 1 && strlen(fitted) < 64 && oracles_ui_text_width(&oracles_ui_row_title, fitted) <= 300.0f);
}

/* ---- 4:3 ---------------------------------------------------------------------------------------------------- */

/* 10d: Ages' Cartridge in 4:3, a header with "Oracle of Ages" on one line, then the rows across the column, each line
 * of a row's first on one baseline; no panel. */
static void layout_10d(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    nav.screen = ORACLES_SCREEN_GAME;
    char state[ORACLES_HOME_STATE_LENGTH];
    OraclesUiGameTexts t;
    OraclesUiGameLayout l;
    texts_of(&nav, &t, state);
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_4_3, &t, &l);
    layout_line("10d", "section", &l.head.section);
    CHECK(l.head.title.count == 1 && !strcmp(l.head.title.text[0], "Oracle of Ages") && l.head.over.w == 0.0f && l.panel.w == 0.0f);
    layout_line("10d", "title", &l.head.title.lines[0]);
    layout_line("10d", "state", &l.head.state);
    layout_line("10d", "ROM", &l.label_rom);
    layout_line("10d", "Save", &l.label_save);
    layout_line("10d", "ROM file", &l.rom_file);
    layout_line("10d", "ROM folder", &l.rom_folder);
    layout_line("10d", "ROM status", &l.rom_status.lines[0]);
    layout_box("10d", "ROM dot", &l.rom_dot);
    layout_line("10d", "Choose ROM", &l.rom_button);
    layout_line("10d", "Choose ROM dot", &l.rom_button_dot);
    layout_box("10d", "ROM row", &l.rows[ORACLES_ROW_ROM]);
    layout_line("10d", "save file", &l.save_file);
    layout_line("10d", "save line", &l.save_line);
    layout_line("10d", "Open folder", &l.save_buttons[0]);
    layout_line("10d", "Open folder dot", &l.save_button_dots[0]);
    layout_box("10d", "Save row", &l.rows[ORACLES_ROW_SAVE]);
    layout_line("10d", "Play", &l.play);
    layout_line("10d", "Play dot", &l.play_dot);
    layout_box("10d", "Play row", &l.rows[ORACLES_ROW_PLAY]);
    /* Every target 90 tall at least, every text 27 px at least. */
    for (int r = 0; r < ORACLES_GAME_ROWS; r++) CHECK(r == ORACLES_ROW_PATCH || l.rows[r].h >= 90.0f);
    const OraclesUiPageStyles *st = oracles_ui_page_styles(ORACLES_UI_LAYOUT_4_3);
    const OraclesUiTextStyle *const all[] = { st->section, st->title, st->state, st->page_note, st->row_label, st->row_title, st->row_path,
                                              st->row_status, st->row_text, st->row_note, st->row_button, st->play, st->option_name,
                                              st->option_size, st->choice, st->diagram_label, st->help, st->help_note, st->mod_games,
                                              st->mods_empty, st->mods_note, st->quality_sets };
    for (size_t i = 0; i < sizeof all / sizeof all[0]; i++) CHECK(all[i]->size >= 27.0f);
}

/* 10e: Moonrise Regalia's Cartridge in 4:3: its base ROM and its patch, then the patch's folder and the image's line. */
static void layout_10e(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1k(&nav);
    char state[ORACLES_HOME_STATE_LENGTH];
    OraclesUiGameTexts t;
    OraclesUiGameLayout l;
    texts_of(&nav, &t, state);
    oracles_ui_layout_game(ORACLES_UI_LAYOUT_4_3, &t, &l);
    layout_line("10e", "section", &l.head.section);
    CHECK(!strcmp(l.head.title.text[0], "Moonrise Regalia"));
    layout_line("10e", "title", &l.head.title.lines[0]);
    layout_line("10e", "state", &l.head.state);
    layout_line("10e", "Base ROM", &l.label_rom);
    layout_line("10e", "Patch", &l.label_patch);
    layout_line("10e", "Save", &l.label_save);
    layout_line("10e", "base file", &l.rom_file);
    layout_line("10e", "base status", &l.rom_status.lines[0]);
    layout_box("10e", "base dot", &l.rom_dot);
    layout_line("10e", "Choose ROM", &l.rom_button);
    layout_box("10e", "base row", &l.rows[ORACLES_ROW_ROM]);
    layout_line("10e", "patch file", &l.patch_file);
    layout_line("10e", "patch status", &l.patch_status);
    layout_box("10e", "patch dot", &l.patch_dot);
    layout_line("10e", "Choose patch", &l.patch_button);
    layout_line("10e", "Choose patch dot", &l.patch_button_dot);
    layout_box("10e", "patch row", &l.rows[ORACLES_ROW_PATCH]);
    layout_line("10e", "patch folder", &l.patch_folder);
    layout_line("10e", "image status", &l.image_status);
    layout_box("10e", "image dot", &l.image_dot);
    layout_line("10e", "save file", &l.save_file);
    layout_line("10e", "save line", &l.save_line);
    layout_box("10e", "Save row", &l.rows[ORACLES_ROW_SAVE]);
    layout_line("10e", "Play", &l.play);
    layout_box("10e", "Play row", &l.rows[ORACLES_ROW_PLAY]);
}

/* Display in 4:3 on the mockup's screen of `width` x `height`, the view's sizes in that screen's shape. */
static void display_4_3_nav(OraclesHomeNav *nav, int width, int height)
{
    oracles_home_init(nav);
    game_1h(&nav->games[0]);
    nav->layout = ORACLES_UI_LAYOUT_4_3;
    nav->screen = ORACLES_SCREEN_DISPLAY;
    nav->display.profile = ORACLES_PROFILE_ENHANCED;
    nav->display.window = 2;
    nav->display.cores = 8;
    nav->display.screen_w = nav->display.room_w = width;
    nav->display.screen_h = height;
    nav->display.room_h = height - 64;
}

static void display_4_3(void)
{
    OraclesHomeNav nav;
    display_4_3_nav(&nav, 1440, 1080);
    OraclesUiDisplayTexts t;
    OraclesUiDisplayLayout l;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    /* 10f: the rows in one column, the explanation of the highlighted one, Profile, in the header with its section and title on one line. */
    layout_line("10f", "section", &l.head.section);
    CHECK(!strcmp(l.title_text, "Oracle of Ages") && l.head.title.count == 0 && l.head.state.h == 0.0f && l.page_note.count == 0);
    layout_line("10f", "title", &l.title);
    /* The diagram's box in the screen's shape, 162 wide; Advanced beside it, framed. */
    layout_box("10f", "diagram", &l.diagram);
    layout_box("10f", "diagram window", &l.diagram_window);
    layout_line("10f", "Advanced", &l.advanced_label);
    layout_box("10f", "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);
    CHECK(l.diagram_label.h == 0.0f);   /* Window's help says the size */
    layout_line("10f", "Profile", &l.labels[ORACLES_DISPLAY_PROFILE].lines[0]);
    static const char *const profiles[2] = { "Faithful", "Enhanced" };
    for (int p = 0; p < ORACLES_PROFILES; p++) {
        char what[32];
        snprintf(what, sizeof what, "profile %d", p); layout_line("10f", what, &l.profiles[p].name);
        snprintf(what, sizeof what, "profile %d size", p); layout_line("10f", what, &l.profiles[p].size);
        snprintf(what, sizeof what, "profile %s", profiles[p]); layout_box("10f", what, &l.profiles[p].box);
    }
    layout_box("10f", "Profile row", &l.rows[ORACLES_DISPLAY_PROFILE]);
    display_quality_row("10f", &t, &l);
    layout_line("10f", "Window", &l.labels[ORACLES_DISPLAY_WINDOW].lines[0]);
    for (int i = 0; i < 4; i++) {
        char what[32];
        snprintf(what, sizeof what, "window %d", i); layout_line("10f", what, &l.windows[i].name);
        snprintf(what, sizeof what, "window %d size", i); layout_line("10f", what, &l.windows[i].size);
        snprintf(what, sizeof what, "window %d box", i); layout_box("10f", what, &l.windows[i].box);
    }
    layout_box("10f", "Window row", &l.rows[ORACLES_DISPLAY_WINDOW]);
    CHECK(l.window_note.h == 0.0f && l.window_reduced.h == 0.0f && l.window_one.h == 0.0f && l.profile_note.h == 0.0f);
    layout_line("10f", "View", &l.labels[ORACLES_DISPLAY_VIEW].lines[0]);
    static const char *const views[3] = { "Near", "Medium", "Far" };
    for (int v = 0; v < 3; v++) {
        char what[32];
        snprintf(what, sizeof what, "view %d", v); layout_line("10f", what, &l.views[v].name);
        snprintf(what, sizeof what, "view %d size", v); layout_line("10f", what, &l.views[v].size);
        snprintf(what, sizeof what, "view %s", views[v]); layout_box("10f", what, &l.views[v].box);
    }
    layout_box("10f", "View row", &l.rows[ORACLES_DISPLAY_VIEW]);
    layout_line("10f", "HUD", &l.labels[ORACLES_DISPLAY_HUD].lines[0]);
    layout_box("10f", "hud Original", &l.hud[0].box);
    layout_box("10f", "hud Full", &l.hud[1].box);
    layout_line("10f", "hud Original text", &l.hud[0].name);
    layout_line("10f", "hud Full text", &l.hud[1].name);
    layout_box("10f", "hud row", &l.rows[ORACLES_DISPLAY_HUD]);
    CHECK(l.rows[ORACLES_DISPLAY_COLOUR].w == 0.0f && l.rows[ORACLES_DISPLAY_TRANSITIONS].w == 0.0f);
    layout_line("10f", "Picture", &l.labels[ORACLES_DISPLAY_PICTURE].lines[0]);
    layout_box("10f", "picture link", &l.picture_link);
    layout_box("10f", "picture row", &l.rows[ORACLES_DISPLAY_PICTURE]);
    CHECK(l.explanations[0].count == 0 && l.view_explanation.count == 0 && l.transitions_note.h == 0.0f);
    CHECK(l.quality_text.h == 0.0f && l.quality_note.h == 0.0f);
    CHECK(l.advanced_arrow.x == l.advanced_label.x + l.advanced_label.w + 16.0f && l.advanced_arrow.w == 13.0f && l.advanced_arrow.h == 18.0f);
    CHECK(!strcmp(t.help, "The wide world, drawn back.") && !t.help_note);
    layout_line("10f", "help text", &l.help);
    layout_near("10f", "help", "y", l.help.y);
    layout_near("10f", "help", "h", l.help.h);
    layout_near("10f", "help note", "y", l.help_note.y);
    layout_near("10f", "help note", "h", l.help_note.h);
    for (unsigned r = 0; r <= ORACLES_DISPLAY_ADVANCED; r++) CHECK(r == ORACLES_DISPLAY_COLOUR || l.rows[r].h >= 90.0f);
    for (unsigned r = 0; r < ORACLES_DISPLAY_ADVANCED; r++) CHECK(r == ORACLES_DISPLAY_COLOUR || l.rows[r].h == 100.0f);
    CHECK(l.rows[ORACLES_DISPLAY_PICTURE].h == 100.0f);

    /* 10g: View highlighted, its explanation under the rows; the window's note and the reduced line under Window's. */
    nav.row = ORACLES_DISPLAY_VIEW;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    layout_line("10g", "help text", &l.help);
    layout_near("10g", "help", "y", l.help.y);
    /* Window's help: its note, and that the window is reduced, or else its size, the diagram's, which 4:3 does not write
     * under it; never 16:9's line on the view. */
    nav.row = ORACLES_DISPLAY_WINDOW;
    oracles_ui_display_texts(&nav, &t);
    CHECK(!strcmp(t.help, "Whole multiples of the picture: pixels stay sharp.") && !strcmp(t.help_note, "Reduced to 2\xc3\x97 to fit this screen."));
    nav.display.window = 0;
    oracles_ui_display_texts(&nav, &t);
    CHECK(!strcmp(t.help_note, "960\xc3\x97" "720 on a 1440\xc3\x97" "1080 screen."));
    nav.display.window = 2;
    nav.row = ORACLES_DISPLAY_ADVANCED;
    oracles_ui_display_texts(&nav, &t);
    CHECK(!strcmp(t.help, "Core, transitions, Vsync, workers, camera.") && !t.help_note);
    nav.row = ORACLES_DISPLAY_TRANSITIONS;
    oracles_ui_display_texts(&nav, &t);
    CHECK(!strcmp(t.help, "Rooms scroll into one another, swimming too.") && !strcmp(t.help_note, "Savestates with it on need it on."));
    nav.row = ORACLES_DISPLAY_HUD;
    oracles_ui_display_texts(&nav, &t);
    CHECK(!strcmp(t.help, "Wide layout: hearts left, items right.") && !t.help_note);
    /* The profile's: Faithful's, and that a game whose ROM allows only Faithful plays in it. */
    nav.row = ORACLES_DISPLAY_PROFILE;
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    oracles_ui_display_texts(&nav, &t);
    CHECK(!strcmp(t.help, "The game as it was, 160\xc3\x97" "144."));
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.games[0].rom = ORACLES_ROM_UNRECOGNISED;
    oracles_ui_display_texts(&nav, &t);
    CHECK(!strcmp(t.help, "Enhanced needs an original ROM: this game plays in Faithful."));
    nav.games[0].rom = ORACLES_ROM_ORIGINAL;

    /* 10o: from the pause menu, the note at the state's place; the rows where they were. */
    nav.row = ORACLES_DISPLAY_PROFILE;
    nav.in_game = 1;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    layout_line("10o", "page note", &l.page_note.lines[0]);
    layout_box("10o", "Window row", &l.rows[ORACLES_DISPLAY_WINDOW]);
    layout_near("10o", "help", "y", l.help.y);
    layout_box("10o", "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);

    /* 10h, 10p: the Advanced rows, Core highlighted, its explanation and its note under them; from the pause menu too. */
    for (int in_game = 0; in_game < 2; in_game++) {
        const char *frame = in_game ? "10p" : "10h";
        nav.in_game = in_game;
        nav.advanced = 1;
        nav.row = ORACLES_DISPLAY_CORE;
        oracles_ui_display_texts(&nav, &t);
        oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
        layout_box(frame, "core row", &l.rows[ORACLES_DISPLAY_CORE]);
        layout_box(frame, "workers row", &l.rows[ORACLES_DISPLAY_WORKERS]);
        layout_near(frame, "help", "y", l.help.y);
        layout_line(frame, "help note", &l.help_note);
        CHECK(l.rows[ORACLES_DISPLAY_PROFILE].w == 0.0f && l.rows[ORACLES_DISPLAY_QUALITY].w == 0.0f && l.rows[ORACLES_DISPLAY_ADVANCED].w == 0.0f
              && l.link == ORACLES_DISPLAY_ROWS);
        if (in_game) { layout_line(frame, "page note", &l.page_note.lines[0]); continue; }
        layout_line(frame, "section", &l.head.section);
        layout_line(frame, "Core", &l.labels[ORACLES_DISPLAY_CORE].lines[0]);
        layout_line(frame, "core 0", &l.core[0].name);
        layout_line(frame, "core 1", &l.core[1].name);
        layout_box(frame, "core Accurate", &l.core[0].box);
        layout_box(frame, "core Fast", &l.core[1].box);
        layout_line(frame, "Vsync", &l.labels[ORACLES_DISPLAY_VSYNC].lines[0]);
        layout_box(frame, "vsync Auto", &l.vsync[0].box);
        layout_box(frame, "vsync Off", &l.vsync[2].box);
        layout_box(frame, "vsync row", &l.rows[ORACLES_DISPLAY_VSYNC]);
        layout_line(frame, "Continuous transitions", &l.labels[ORACLES_DISPLAY_TRANSITIONS].lines[0]);
        layout_box(frame, "transitions Off", &l.transitions[0].box);
        layout_box(frame, "transitions On", &l.transitions[1].box);
        layout_box(frame, "transitions row", &l.rows[ORACLES_DISPLAY_TRANSITIONS]);
        CHECK(l.labels[ORACLES_DISPLAY_WORKERS].count == 2);
        layout_line(frame, "Neighbour workers", &l.labels[ORACLES_DISPLAY_WORKERS].lines[0]);
        static const char *const workers[3] = { "Auto", "One", "Two" };
        for (int i = 0; i < 3; i++) {
            char what[32];
            snprintf(what, sizeof what, "workers %d", i); layout_line(frame, what, &l.workers[i].name);
            snprintf(what, sizeof what, "workers %s", workers[i]); layout_box(frame, what, &l.workers[i].box);
        }
        layout_line(frame, "help text", &l.help);
    }

    /* 10s: a 640x480 screen, where every window comes to one size: the Window row inert, its note in the windows' place,
     * as tall; the help says the size there. */
    display_4_3_nav(&nav, 640, 480);
    CHECK(oracles_display_one_size(&nav));
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    CHECK(t.window_one && !strcmp(t.window_reduced, oracles_display_one_size_note));
    layout_line("10s", "Window", &l.labels[ORACLES_DISPLAY_WINDOW].lines[0]);
    layout_box("10s", "Window row", &l.rows[ORACLES_DISPLAY_WINDOW]);
    for (int i = 0; i < 4; i++) CHECK(l.windows[i].box.w == 0.0f);
    layout_line("10s", "one size", &l.window_one);
    layout_box("10s", "View row", &l.rows[ORACLES_DISPLAY_VIEW]);
    layout_near("10s", "help", "y", l.help.y);
    layout_box("10s", "diagram", &l.diagram);
    layout_box("10s", "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);
    nav.row = ORACLES_DISPLAY_WINDOW;
    oracles_ui_display_texts(&nav, &t);
    CHECK(!strcmp(t.help_note, "480\xc3\x97" "360 on a 640\xc3\x97" "480 screen."));
}

/* Where every window comes to one size, the Window row keeps the highlight but changes no more, by a key or a click,
 * and says so under the windows in 16:9 too; a platform always fullscreen likewise. */
static void one_size(void)
{
    OraclesHomeNav nav;
    display_4_3_nav(&nav, 640, 480);
    nav.layout = ORACLES_UI_LAYOUT_16_9;
    nav.row = ORACLES_DISPLAY_QUALITY;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_WINDOW);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY
          && nav.display.window == 2);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_WINDOW, 0) == ORACLES_HOME_STAY && nav.display.window == 2 && nav.row == ORACLES_DISPLAY_WINDOW);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_VIEW);
    OraclesUiDisplayTexts t;
    OraclesUiDisplayLayout l;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    CHECK(l.window_reduced.w > 0.0f && !strcmp(t.window_reduced, oracles_display_one_size_note) && l.window_one.h == 0.0f);
    /* Faithful at 640x480 fits 2x windowed and 3x fullscreen: the choices differ, the row changes. */
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    CHECK(!oracles_display_one_size(&nav) && oracles_display_click(&nav, ORACLES_DISPLAY_WINDOW, 0) == ORACLES_HOME_STORE);
    /* A 1080p screen in Enhanced: 2x and 3x fit; a platform always fullscreen, one size. */
    display_4_3_nav(&nav, 1920, 1080);
    CHECK(!oracles_display_one_size(&nav));
    nav.display.fullscreen_only = 1;
    CHECK(oracles_display_one_size(&nav) && oracles_display_click(&nav, ORACLES_DISPLAY_WINDOW, 0) == ORACLES_HOME_STAY);
}

/* Fill at one size: on a 1366x768 screen the Far view's windows all come to 2x, whatever Window keeps (3x here, the row
 * not changing), and Fill fills the screen, the game then fullscreen; Sharp keeps the window, 960x540. */
static void fill_one_size(void)
{
    OraclesHomeNav nav;
    display_4_3_nav(&nav, 1366, 768);
    nav.layout = ORACLES_UI_LAYOUT_16_9;
    nav.display.view = 2;
    nav.display.window = 1;
    nav.display.room_h = 768 - 40 - 64;   /* a taskbar, the window's frame */
    nav.display.scaling = 1;
    float w, h;
    char label[64];
    CHECK(oracles_display_one_size(&nav) && oracles_display_one_size_for(&nav.display, 480, 270) && oracles_display_fullscreen(&nav));
    oracles_display_diagram(&nav, ORACLES_UI_DIAGRAM_W, ORACLES_UI_DIAGRAM_H, &w, &h, label, sizeof label);
    CHECK(!strcmp(label, "1365\xc3\x97" "768 on a 1366\xc3\x97" "768 screen") && w == ORACLES_UI_DIAGRAM_W);
    nav.display.scaling = 0;
    CHECK(!oracles_display_fullscreen(&nav));
    oracles_display_diagram(&nav, ORACLES_UI_DIAGRAM_W, ORACLES_UI_DIAGRAM_H, &w, &h, label, sizeof label);
    CHECK(!strcmp(label, "960\xc3\x97" "540 on a 1366\xc3\x97" "768 screen"));
    /* Faithful there: 4x windowed, 5x fullscreen, not one size: Fill applies with Fullscreen chosen only. */
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    nav.display.scaling = 1;
    CHECK(!oracles_display_one_size(&nav) && !oracles_display_fullscreen(&nav) && !oracles_display_one_size_for(&nav.display, 160, 144));
    nav.display.window = 3;
    CHECK(oracles_display_fullscreen(&nav));
}

/* Display's rows and Quality's choices where another quality or another highlighted row leaves them: no row's height
 * changes with the choice or the focus. */
static int same_places(const OraclesUiDisplayLayout *a, const OraclesUiDisplayLayout *b)
{
    for (unsigned r = 0; r < ORACLES_DISPLAY_ROWS; r++) if (memcmp(&a->rows[r], &b->rows[r], sizeof a->rows[r])) return 0;
    for (int q = 0; q <= ORACLES_DISPLAY_QUALITIES; q++) if (memcmp(&a->qualities[q].box, &b->qualities[q].box, sizeof a->qualities[q].box)) return 0;
    return a->panel.h == b->panel.h && a->quality_text.y == b->quality_text.y && a->quality_note.y == b->quality_note.y && a->help.y == b->help.y
           && a->help_note.y == b->help_note.y;
}

/* Window's four boxes against a frame's. */
static void display_window_boxes(const char *frame, const OraclesUiDisplayLayout *l)
{
    for (int i = 0; i < 4; i++) {
        char what[32];
        snprintf(what, sizeof what, "window %d box", i);
        layout_box(frame, what, &l->windows[i].box);
    }
}

/* Window's boxes where another view leaves them: each as wide as its longest size over the views and Faithful. */
static int same_windows(const OraclesUiDisplayLayout *a, const OraclesUiDisplayLayout *b)
{
    for (int i = 0; i < 4; i++) if (memcmp(&a->windows[i].box, &b->windows[i].box, sizeof a->windows[i].box)) return 0;
    return 1;
}

/* Quality highlighted on High (the far view on the Fast core, the workers on Auto) in 16:9: 11a. */
static void quality_16_9(const char *frame, const OraclesHomeNav *nav, const char *text, const char *note, OraclesUiDisplayLayout *l)
{
    OraclesUiDisplayTexts t;
    oracles_ui_display_texts(nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, l);
    display_quality_row(frame, &t, l);
    CHECK(!strcmp(t.quality_text, text) && !strcmp(t.quality_note, note));
    layout_text(frame, "quality text", &l->quality_text);
    layout_near(frame, "quality expl", "y", l->quality_text.y);
    layout_near(frame, "quality expl", "h", l->quality_text.h);
    layout_text(frame, "quality note", &l->quality_note);
    layout_near(frame, "quality expl note", "y", l->quality_note.y);
    layout_near(frame, "quality expl note", "h", l->quality_note.h);
    layout_box(frame, "Profile row", &l->rows[ORACLES_DISPLAY_PROFILE]);
    layout_text(frame, "profile note", &l->profile_note);
    layout_box(frame, "Window row", &l->rows[ORACLES_DISPLAY_WINDOW]);
    CHECK(!strcmp(t.window_fit, oracles_ui_window_fit_note));
    window_notes(frame, "fit text", l);
    display_window_boxes(frame, l);
    layout_box(frame, "View row", &l->rows[ORACLES_DISPLAY_VIEW]);
    layout_box(frame, "hud row", &l->rows[ORACLES_DISPLAY_HUD]);
    layout_box(frame, "picture row", &l->rows[ORACLES_DISPLAY_PICTURE]);
    layout_box(frame, "panel", &l->panel);
    layout_text(frame, "Advanced", &l->advanced_label);
    layout_box(frame, "advanced", &l->rows[ORACLES_DISPLAY_ADVANCED]);
}

/* The same in 4:3: the description in the help lines under the rows, Advanced beside the diagram. */
static void quality_4_3(const char *frame, OraclesHomeNav *nav, const char *text, const char *note, OraclesUiDisplayLayout *l)
{
    OraclesUiDisplayTexts t;
    oracles_ui_display_texts(nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, l);
    display_quality_row(frame, &t, l);
    CHECK(!strcmp(t.help, text) && !strcmp(t.help_note, note));
    layout_line(frame, "help text", &l->help);
    layout_near(frame, "help", "y", l->help.y);
    layout_near(frame, "help", "h", l->help.h);
    layout_near(frame, "help note box", "y", l->help_note.y);
    layout_near(frame, "help note box", "h", l->help_note.h);
    layout_line(frame, "help note", &l->help_note);
    layout_box(frame, "Profile row", &l->rows[ORACLES_DISPLAY_PROFILE]);
    layout_box(frame, "Window row", &l->rows[ORACLES_DISPLAY_WINDOW]);
    layout_box(frame, "View row", &l->rows[ORACLES_DISPLAY_VIEW]);
    layout_box(frame, "diagram", &l->diagram);
    layout_line(frame, "Advanced", &l->advanced_label);
    layout_box(frame, "advanced", &l->rows[ORACLES_DISPLAY_ADVANCED]);
}

/* 11a, 11b, 11d, 11e, 11g: Quality highlighted on High, then on Custom (the near view on the Accurate core, which no
 * quality makes), in both layouts and on a 640x480 screen; the rows do not move with the quality nor with the focus. */
static void display_quality(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.display.window = 1;
    nav.display.core = 1;
    nav.row = ORACLES_DISPLAY_QUALITY;
    CHECK(oracles_display_quality(&nav) == 2 && !strcmp(oracles_display_quality_names[2], "High"));
    OraclesUiDisplayLayout high, custom, other;
    OraclesUiDisplayTexts t;
    quality_16_9("11a", &nav, "Far view, Fast core, neighbor workers on auto.", "Changing one of them by hand makes it Custom.", &high);
    nav.display.view = 0;
    nav.display.core = 0;
    CHECK(oracles_display_quality(&nav) == ORACLES_DISPLAY_QUALITY_CUSTOM);
    quality_16_9("11b", &nav, "Set by hand: Near view, Accurate core, neighbor workers on auto.", "Pick a quality to set all three again.", &custom);
    CHECK(same_places(&high, &custom) && same_windows(&high, &custom));
    /* 11i: Low, the near view on the Fast core: Window's boxes as at High. */
    nav.display.core = 1;
    quality_16_9("11i", &nav, "Near view, Fast core, neighbor workers on auto.", "Changing one of them by hand makes it Custom.", &other);
    CHECK(oracles_display_quality(&nav) == 0 && same_places(&high, &other) && same_windows(&high, &other));
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &other);
    CHECK(same_windows(&high, &other));
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.display.core = 0;
    for (unsigned row = 0; row < ORACLES_DISPLAY_ADVANCED; row++) {
        nav.row = row;
        oracles_ui_display_texts(&nav, &t);
        oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &other);
        CHECK(same_places(&custom, &other));
    }
    /* Max, its workers two; in Faithful the row keeps its place. */
    nav.display.view = 2;
    nav.display.workers = 2;
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    CHECK(oracles_display_quality(&nav) == 3 && !oracles_display_quality_applies(&nav));
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &other);
    CHECK(!strcmp(t.quality_text, "Far view, Accurate core, neighbor workers at 2.") && same_places(&high, &other));

    display_4_3_nav(&nav, 1440, 1080);
    nav.display.window = 1;
    nav.display.core = 1;
    nav.row = ORACLES_DISPLAY_QUALITY;
    quality_4_3("11d", &nav, "Far view, Fast core, workers on auto.", "Changing one by hand makes it Custom.", &high);
    layout_box("11d", "hud row", &high.rows[ORACLES_DISPLAY_HUD]);
    layout_box("11d", "picture row", &high.rows[ORACLES_DISPLAY_PICTURE]);
    nav.display.view = 0;
    nav.display.core = 0;
    quality_4_3("11e", &nav, "Set by hand: Near, Accurate, workers auto.", "Pick a quality to set all three again.", &custom);
    display_window_boxes("11d", &high);
    display_window_boxes("11e", &custom);
    CHECK(same_places(&high, &custom) && same_windows(&high, &custom));
    for (unsigned row = 0; row < ORACLES_DISPLAY_ROWS; row++) {
        if (!oracles_display_row_shown(&nav, row)) continue;
        nav.row = row;
        oracles_ui_display_texts(&nav, &t);
        oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &other);
        CHECK(same_places(&custom, &other));
    }

    /* 11g: a 640x480 screen, the Window row's note in place of its windows, the rows where they were. */
    display_4_3_nav(&nav, 640, 480);
    nav.display.core = 1;
    nav.row = ORACLES_DISPLAY_QUALITY;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &other);
    display_quality_row("11g", &t, &other);
    layout_line("11g", "one size", &other.window_one);
    layout_line("11g", "help text", &other.help);
    layout_box("11g", "Window row", &other.rows[ORACLES_DISPLAY_WINDOW]);
    layout_box("11g", "View row", &other.rows[ORACLES_DISPLAY_VIEW]);
    CHECK(same_places(&high, &other));
    layout_box("11g", "advanced", &other.rows[ORACLES_DISPLAY_ADVANCED]);
}

/* Quality by the keys and the pointer: each sets the view, the core and the workers; a hand change makes it Custom. */
static void display_quality_navigation(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.row = ORACLES_DISPLAY_QUALITY;
    /* The far view on the Accurate core, the workers on Auto: no quality, Max having two. */
    CHECK(oracles_display_quality(&nav) == ORACLES_DISPLAY_QUALITY_CUSTOM && oracles_display_quality_applies(&nav));
    /* Right from Custom takes Low, then each heavier one, stopping at Max; left the lighter. */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && oracles_display_quality(&nav) == 0);
    CHECK(nav.display.view == 0 && nav.display.core == 1 && nav.display.workers == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.view == 1 && oracles_display_quality(&nav) == 1);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.display.view == 2 && oracles_display_quality(&nav) == 2);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && oracles_display_quality(&nav) == 3);
    CHECK(nav.display.view == 2 && nav.display.core == 0 && nav.display.workers == 2);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && oracles_display_quality(&nav) == 3);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && oracles_display_quality(&nav) == 2);
    /* View changed by hand makes Medium; the workers then, Custom; left from Custom takes Max. */
    nav.row = ORACLES_DISPLAY_VIEW;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && oracles_display_quality(&nav) == 1);
    nav.display.workers = 1;
    CHECK(oracles_display_quality(&nav) == ORACLES_DISPLAY_QUALITY_CUSTOM);
    char text[ORACLES_HOME_TEXT_LENGTH];
    oracles_display_quality_text(&nav, 1, text, sizeof text);
    CHECK(!strcmp(text, "Set by hand: Medium, Fast, workers 1."));
    nav.row = ORACLES_DISPLAY_QUALITY;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && oracles_display_quality(&nav) == 3);
    oracles_display_quality_text(&nav, 1, text, sizeof text);
    CHECK(!strcmp(text, "Far view, Accurate core, workers at 2."));
    /* A click on a quality applies it; on Custom's place, or on the quality in effect, nothing changes. */
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_QUALITY, 0) == ORACLES_HOME_STORE && oracles_display_quality(&nav) == 0);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_QUALITY, 0) == ORACLES_HOME_STAY);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_QUALITY, ORACLES_DISPLAY_QUALITY_CUSTOM) == ORACLES_HOME_STAY && oracles_display_quality(&nav) == 0);
    /* In Faithful, and from a game (whose core is fixed), the row is inert. */
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && oracles_display_click(&nav, ORACLES_DISPLAY_QUALITY, 2) == ORACLES_HOME_STAY);
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.in_game = 1;
    CHECK(!oracles_display_quality_applies(&nav) && oracles_display_row_fixed(&nav, ORACLES_DISPLAY_QUALITY));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && oracles_display_quality(&nav) == 0);
}

/* The Scaling row of a frame in 16:9: its explanation on two lines and its note, Sharp and Fill each with the image's
 * size in fullscreen. */
static void display_scaling_16_9(const char *frame, const char *sharp, const char *fill, const OraclesUiDisplayTexts *t,
                                 const OraclesUiDisplayLayout *l)
{
    CHECK(!strcmp(t->scaling_sizes[0], sharp) && !strcmp(t->scaling_sizes[1], fill));
    layout_text(frame, "Scaling", &l->labels[ORACLES_DISPLAY_SCALING].lines[0]);
    CHECK(l->explanations[5].count == 2);
    layout_text(frame, "scaling explanation", &l->explanations[5].lines[0]);
    layout_near(frame, "scaling explanation", "width", l->explanations[5].w);
    layout_text(frame, "scaling note", &l->scaling_note);
    static const char *const names[2] = { "Sharp", "Fill" };
    for (int i = 0; i < 2; i++) {
        char what[48];
        snprintf(what, sizeof what, "scaling %s", names[i]); layout_box(frame, what, &l->scaling[i].box);
        snprintf(what, sizeof what, "scaling %s box", names[i]); layout_box(frame, what, &l->scaling[i].box);
        snprintf(what, sizeof what, "scaling %s name", names[i]); layout_text(frame, what, &l->scaling[i].name);
        snprintf(what, sizeof what, "scaling %s size text", names[i]); layout_text(frame, what, &l->scaling[i].size);
    }
    layout_box(frame, "scaling row", &l->rows[ORACLES_DISPLAY_SCALING]);
    layout_near(frame, "scaling grid", "y", l->rows[ORACLES_DISPLAY_SCALING].y);
    layout_near(frame, "scaling grid", "h", l->rows[ORACLES_DISPLAY_SCALING].h);
}

/* The same in 4:3: the help under the rows says the row's short explanation and its note. */
static void display_scaling_4_3(const char *frame, const char *sharp, const char *fill, const OraclesUiDisplayTexts *t,
                                const OraclesUiDisplayLayout *l)
{
    CHECK(!strcmp(t->scaling_sizes[0], sharp) && !strcmp(t->scaling_sizes[1], fill));
    layout_line(frame, "Scaling", &l->labels[ORACLES_DISPLAY_SCALING].lines[0]);
    static const char *const names[2] = { "Sharp", "Fill" };
    for (int i = 0; i < 2; i++) {
        char what[48];
        snprintf(what, sizeof what, "scaling %s", names[i]); layout_box(frame, what, &l->scaling[i].box);
        snprintf(what, sizeof what, "scaling %s name", names[i]); layout_line(frame, what, &l->scaling[i].name);
        snprintf(what, sizeof what, "scaling %s size text", names[i]); layout_line(frame, what, &l->scaling[i].size);
    }
    layout_box(frame, "scaling row", &l->rows[ORACLES_DISPLAY_SCALING]);
    CHECK(!strcmp(t->help, "Sharp: exact pixels. Fill: the whole screen.") && !strcmp(t->help_note, "Fill applies in fullscreen."));
    layout_near(frame, "help", "y", l->help.y);
    layout_line(frame, "help text", &l->help);
    layout_line(frame, "help note", &l->help_note);
    layout_box(frame, "diagram", &l->diagram);
    layout_box(frame, "diagram window", &l->diagram_window);
}

/* 13a, 13e, 13b, 13c, 13d: Scaling, on Display › Picture since turn 34. */
static void display_scaling(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.display.window = 1;
    nav.display.cores = 8;
    nav.picture_page = 1;
    nav.row = ORACLES_DISPLAY_SCALING;
    OraclesUiDisplayTexts t;
    OraclesUiDisplayLayout l;
    /* 13a: the far view on a 1080p screen, where Sharp's 4x and Fill both make 1920x1080; the page's first row. */
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    display_scaling_16_9("13a", "1920\xc3\x97" "1080", "1920\xc3\x97" "1080", &t, &l);
    layout_text("13a", "section", &l.head.section);
    layout_box("13a", "panel", &l.panel);
    layout_box("13a", "diagram window", &l.diagram_window);
    layout_text("13a", "diagram label", &l.diagram_label);
    /* Fill chosen in a window: the diagram keeps the window, Fill applying in fullscreen only. */
    nav.display.scaling = 1;
    oracles_ui_display_texts(&nav, &t);
    CHECK(!strcmp(t.diagram_label, "1440\xc3\x97" "810 on a 1920\xc3\x97" "1080 screen"));

    /* 13e: Faithful fullscreen on Fill: 7x makes 1120x1008, Fill 1200x1080, which the diagram draws. */
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    nav.display.window = 3;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    display_scaling_16_9("13e", "1120\xc3\x97" "1008", "1200\xc3\x97" "1080", &t, &l);
    layout_box("13e", "diagram", &l.diagram);
    layout_box("13e", "diagram window", &l.diagram_window);
    layout_text("13e", "diagram label", &l.diagram_label);
    CHECK(!strcmp(t.diagram_label, "1200\xc3\x97" "1080 on a 1920\xc3\x97" "1080 screen"));

    /* 13b: 4:3 on a 640x480 screen, Fill chosen: the far view's 480x360 at 1x, or the whole screen. */
    display_4_3_nav(&nav, 640, 480);
    nav.display.window = 1;
    nav.display.scaling = 1;
    nav.picture_page = 1;
    nav.row = ORACLES_DISPLAY_SCALING;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    display_scaling_4_3("13b", "480\xc3\x97" "360", "640\xc3\x97" "480", &t, &l);
    layout_line("13b", "section", &l.head.section);
    layout_near("13b", "rows", "y", l.rows[ORACLES_DISPLAY_SCALING].y);
    CHECK(l.rows[ORACLES_DISPLAY_SCALING].h == 100.0f && l.rows[ORACLES_DISPLAY_MARGIN].h == 100.0f && l.rows[ORACLES_DISPLAY_MENUS].h == 100.0f);

    /* 13c: Display there, Window's one size saying that Scaling can fill the screen, italic, in the windows' place. */
    nav.picture_page = 0;
    nav.row = ORACLES_DISPLAY_PROFILE;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    CHECK(t.window_one && !strcmp(oracles_display_one_size_note, "One size only here; Scaling in Picture can fill the screen."));
    layout_line("13c", "Window", &l.labels[ORACLES_DISPLAY_WINDOW].lines[0]);
    layout_box("13c", "Window row", &l.rows[ORACLES_DISPLAY_WINDOW]);
    layout_line("13c", "one size", &l.window_one);
    layout_box("13c", "View row", &l.rows[ORACLES_DISPLAY_VIEW]);
    layout_box("13c", "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);
    layout_near("13c", "help", "y", l.help.y);

    /* 13d: from the pause menu on a 4:3 screen: the note says what applies at once; Scaling at 3x and Fill alike. */
    display_4_3_nav(&nav, 1440, 1080);
    nav.display.window = 1;
    nav.in_game = 1;
    nav.picture_page = 1;
    nav.row = ORACLES_DISPLAY_SCALING;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    display_scaling_4_3("13d", "1440\xc3\x97" "1080", "1440\xc3\x97" "1080", &t, &l);
    layout_line("13d", "page note", &l.page_note.lines[0]);
    layout_box("13d", "margin row", &l.rows[ORACLES_DISPLAY_MARGIN]);
    layout_box("13d", "Color row", &l.rows[ORACLES_DISPLAY_COLOUR]);
    layout_box("13d", "menus row", &l.rows[ORACLES_DISPLAY_MENUS]);
    CHECK(oracles_display_row_fixed(&nav, ORACLES_DISPLAY_CORE) && oracles_display_row_fixed(&nav, ORACLES_DISPLAY_WORKERS)
          && !oracles_display_row_fixed(&nav, ORACLES_DISPLAY_VSYNC) && oracles_display_row_fixed(&nav, ORACLES_DISPLAY_TRANSITIONS)
          && !oracles_display_row_fixed(&nav, ORACLES_DISPLAY_SCALING) && !oracles_display_row_fixed(&nav, ORACLES_DISPLAY_MARGIN));
}

/* Advanced's rows and choices stay where they are whatever row is highlighted and whatever is chosen, in both layouts,
 * from the home screen as from a game. */
/* The far view fitted to the screen, as a game started from Display takes it (oracles_session_view_size): View's
 * sizes, Enhanced's, the windows, Scaling's and the diagram's line on a desktop screen of `w` x `h`, Far chosen, 3x. */
static void far_sizes(int w, int h, const char *near, const char *far, const char *note, const char *windows[4], const char *fill,
                      const char *diagram)
{
    OraclesHomeNav nav;
    display_4_3_nav(&nav, w, h);
    nav.display.window = 1;
    nav.row = ORACLES_DISPLAY_VIEW;
    OraclesUiDisplayTexts t;
    oracles_ui_display_texts(&nav, &t);
    const int ok = !strcmp(t.view_sizes[0], near) && !strcmp(t.view_sizes[2], far) && !strcmp(t.profile_sizes[ORACLES_PROFILE_ENHANCED], far)
                   && !strcmp(t.help_note ? t.help_note : "", note) && !strcmp(t.window_sizes[0], windows[0]) && !strcmp(t.window_sizes[1], windows[1])
                   && !strcmp(t.window_sizes[2], windows[2]) && !strcmp(t.window_sizes[3], windows[3]) && !strcmp(t.scaling_sizes[1], fill)
                   && !strcmp(t.diagram_label, diagram);
    if (!ok) fprintf(stderr, "FAIL %dx%d: %s, %s, \"%s\"; %s, %s, %s, %s; %s; %s\n", w, h, t.view_sizes[0], t.view_sizes[2], t.help_note ? t.help_note : "",
                     t.window_sizes[0], t.window_sizes[1], t.window_sizes[2], t.window_sizes[3], t.scaling_sizes[1], t.diagram_label);
    CHECK(ok);
}

static void far_fitted_sizes(void)
{
    const char *nova[4] = { "852\xc3\x97" "640", "1278\xc3\x97" "960", "1704\xc3\x97" "1280", "3\xc3\x97 \xc2\xb7 1278\xc3\x97" "960" };
    far_sizes(1280, 960, "213\xc3\x97" "160", "426\xc3\x97" "320", "Far fits this screen: 426\xc3\x97" "320 at 3\xc3\x97.", nova, "1278\xc3\x97" "960",
              "852\xc3\x97" "640 on a 1280\xc3\x97" "960 screen");
    const char *mini[4] = { "824\xc3\x97" "720", "1236\xc3\x97" "1080", "1648\xc3\x97" "1440", "3\xc3\x97 \xc2\xb7 1236\xc3\x97" "1080" };
    far_sizes(1240, 1080, "213\xc3\x97" "160", "412\xc3\x97" "360", "Far fits this screen: 412\xc3\x97" "360 at 3\xc3\x97.", mini, "1236\xc3\x97" "1080",
              "824\xc3\x97" "720 on a 1240\xc3\x97" "1080 screen");
    const char *small[4] = { "960\xc3\x97" "720", "1440\xc3\x97" "1080", "1920\xc3\x97" "1440", "1\xc3\x97 \xc2\xb7 480\xc3\x97" "360" };
    far_sizes(640, 480, "213\xc3\x97" "160", "480\xc3\x97" "360", "", small, "640\xc3\x97" "480", "480\xc3\x97" "360 on a 640\xc3\x97" "480 screen");
    const char *hd[4] = { "960\xc3\x97" "540", "1440\xc3\x97" "810", "1920\xc3\x97" "1080", "4\xc3\x97 \xc2\xb7 1920\xc3\x97" "1080" };
    far_sizes(1920, 1080, "256\xc3\x97" "144", "480\xc3\x97" "270", "", hd, "1920\xc3\x97" "1080", "1440\xc3\x97" "810 on a 1920\xc3\x97" "1080 screen");

    /* far=fixed keeps 480x360 on the screens far is fitted to, as does a shape aspect= names; 16:9 named, 480x270. */
    OraclesHomeNav nav;
    char size[24], note[64];
    for (int s = 0; s < 2; s++) {
        display_4_3_nav(&nav, s ? 1240 : 1280, s ? 1080 : 960);
        nav.display.far_fixed = 1;
        oracles_display_view_size(&nav, 2, size, sizeof size);
        oracles_display_far_note(&nav, note, sizeof note);
        CHECK(!strcmp(size, "480\xc3\x97" "360") && !note[0]);
        nav.display.far_fixed = 0;
        nav.display.aspect = 2;
        oracles_display_view_size(&nav, 2, size, sizeof size);
        oracles_display_far_note(&nav, note, sizeof note);
        CHECK(!strcmp(size, "480\xc3\x97" "360") && !note[0]);
        nav.display.aspect = 1;
        oracles_display_view_size(&nav, 2, size, sizeof size);
        CHECK(!strcmp(size, "480\xc3\x97" "270") && !oracles_display_screen_4_3(&nav));
        nav.display.aspect = 0;
        oracles_display_view_size(&nav, 2, size, sizeof size);
        CHECK(!strcmp(size, s ? "412\xc3\x97" "360" : "426\xc3\x97" "320"));
    }
    /* The note is View's alone, whatever the view chosen; in the pause, where View is dimmed, the note says why. */
    display_4_3_nav(&nav, 1280, 960);
    OraclesUiDisplayTexts t;
    for (unsigned row = 0; row < ORACLES_DISPLAY_ROWS; row++) {
        nav.row = row;
        nav.advanced = row >= ORACLES_DISPLAY_CORE;
        oracles_ui_display_texts(&nav, &t);
        CHECK((row == ORACLES_DISPLAY_VIEW) == (t.help_note && !strncmp(t.help_note, "Far fits", 8)));
    }
    nav.advanced = 0;
    nav.row = ORACLES_DISPLAY_VIEW;
    nav.display.view = 0;
    nav.in_game = 1;
    oracles_ui_display_texts(&nav, &t);
    CHECK(t.help_note && !strcmp(t.help_note, "Change it from the home screen.") && !strcmp(t.profile_sizes[1], "213\xc3\x97" "160"));
    /* A screen held upright counts as turned; a screen far is not fitted to (5:4) keeps 480x360. */
    display_4_3_nav(&nav, 1080, 1240);
    oracles_display_view_size(&nav, 2, size, sizeof size);
    CHECK(!strcmp(size, "412\xc3\x97" "360"));
    display_4_3_nav(&nav, 1280, 1024);
    oracles_display_view_size(&nav, 2, size, sizeof size);
    oracles_display_far_note(&nav, note, sizeof note);
    CHECK(!strcmp(size, "480\xc3\x97" "360") && !note[0]);
}

/* 23a to 23e, Display as 1.2.0 ships it on Android with a controller, View highlighted, against turn 23 of the
 * prototype: on a 1280x960 screen (23a) and a 1240x1080 one (23b) Far's and Enhanced's sizes are the fitted one's, the
 * note under View's help names it, the diagram's box has the screen's shape (narrower at 1240x1080, Advanced beside
 * it); Near chosen, the note stays (23c); on 640x480 far keeps 480x360 and there is no note (23d); from the pause
 * menu, under the title the note on what applies at once (23e). */
static void far_fitted_probes(const char *frame, const OraclesUiDisplayLayout *l, int sizes)
{
    static const char *const view_probes[3] = { "Near", "Medium", "Far" };
    layout_line(frame, "section", &l->head.section);
    layout_line(frame, "title", &l->title);
    layout_box(frame, "Profile row", &l->rows[ORACLES_DISPLAY_PROFILE]);
    layout_box(frame, "profile Enhanced", &l->profiles[ORACLES_PROFILE_ENHANCED].box);
    layout_line(frame, "profile 1 size", &l->profiles[ORACLES_PROFILE_ENHANCED].size);
    layout_box(frame, "Window row", &l->rows[ORACLES_DISPLAY_WINDOW]);
    layout_line(frame, "one size", &l->window_one);
    layout_line(frame, "View", &l->labels[ORACLES_DISPLAY_VIEW].lines[0]);
    layout_box(frame, "View row", &l->rows[ORACLES_DISPLAY_VIEW]);
    for (int v = 0; v < 3; v++) {
        char what[32];
        snprintf(what, sizeof what, "view %d", v); layout_line(frame, what, &l->views[v].name);
        snprintf(what, sizeof what, "view %s", view_probes[v]); layout_box(frame, what, &l->views[v].box);
        if (sizes || v == 2) { snprintf(what, sizeof what, "view %d size", v); layout_line(frame, what, &l->views[v].size); }
    }
    layout_box(frame, "hud row", &l->rows[ORACLES_DISPLAY_HUD]);
    layout_box(frame, "picture row", &l->rows[ORACLES_DISPLAY_PICTURE]);
    layout_line(frame, "Advanced", &l->advanced_label);
    layout_box(frame, "advanced", &l->rows[ORACLES_DISPLAY_ADVANCED]);
    layout_line(frame, "help text", &l->help);
    layout_near(frame, "help", "y", l->help.y);
    layout_near(frame, "help", "h", l->help.h);
    layout_near(frame, "help note box", "y", l->help_note.y);
    layout_near(frame, "help note box", "h", l->help_note.h);
}

static void display_far_fitted(void)
{
    static const struct { const char *frame; int w, h, view, in_game; const char *size, *note; } frames[] = {
        { "23a", 1280, 960, 2, 0, "426\xc3\x97" "320", "Far fits this screen: 426\xc3\x97" "320 at 3\xc3\x97." },
        { "23b", 1240, 1080, 2, 0, "412\xc3\x97" "360", "Far fits this screen: 412\xc3\x97" "360 at 3\xc3\x97." },
        { "23c", 1280, 960, 0, 0, "213\xc3\x97" "160", "Far fits this screen: 426\xc3\x97" "320 at 3\xc3\x97." },
        { "23d", 640, 480, 2, 0, "480\xc3\x97" "360", "" },
        { "23e", 1280, 960, 2, 1, "426\xc3\x97" "320", "Change it from the home screen." },
    };
    for (size_t f = 0; f < sizeof frames / sizeof frames[0]; f++) {
        const char *frame = frames[f].frame;
        OraclesHomeNav nav;
        display_4_3_nav(&nav, frames[f].w, frames[f].h);
        nav.android = 1;
        nav.display.fullscreen_only = 1;
        nav.display.window = 1;
        nav.display.view = frames[f].view;
        nav.row = ORACLES_DISPLAY_VIEW;
        nav.in_game = frames[f].in_game;
        if (nav.in_game) nav.playing = ORACLES_PROFILE_ENHANCED;
        oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
        OraclesUiDisplayTexts t;
        OraclesUiDisplayLayout l;
        oracles_ui_display_texts(&nav, &t);
        oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
        CHECK(t.window_one && !strcmp(t.profile_sizes[ORACLES_PROFILE_ENHANCED], frames[f].size));
        CHECK(!strcmp(t.help, "How much of the world shows; farther asks more.") && !strcmp(t.help_note ? t.help_note : "", frames[f].note));
        far_fitted_probes(frame, &l, frames[f].note[0] != 0);
        if (frames[f].note[0]) layout_line(frame, "help note", &l.help_note);
        else CHECK(l.help_note.w == 0.0f);
        if (frames[f].in_game) layout_line(frame, "page note", &l.page_note.lines[0]);
        else if (frames[f].w != 640) {
            layout_box(frame, "diagram", &l.diagram);
            layout_box(frame, "diagram window", &l.diagram_window);
        }
    }
}

static void advanced_rows_stay(void)
{
    for (int wide = 0; wide < 2; wide++) {
        const OraclesUiLayout layout = wide ? ORACLES_UI_LAYOUT_4_3 : ORACLES_UI_LAYOUT_16_9;
        OraclesHomeNav nav;
        display_4_3_nav(&nav, 1920, 1080);
        nav.layout = layout;
        nav.advanced = 1;
        OraclesUiDisplayTexts t;
        OraclesUiDisplayLayout first, other;
        for (int in_game = 0; in_game < 2; in_game++)
            for (unsigned row = ORACLES_DISPLAY_CORE; row < ORACLES_DISPLAY_LETTERS; row++)
                for (int choice = 0; choice < 3; choice++) {
                    nav.in_game = in_game;
                    if (row == ORACLES_DISPLAY_CORE && !choice) {   /* each place's own: from a game the rows it keeps say why */
                        nav.row = row;
                        nav.display.core = nav.display.vsync = nav.display.workers = nav.display.scaling = nav.display.transitions = 0;
                        nav.display.margin = 0;
                        nav.display.profile = ORACLES_PROFILE_ENHANCED;
                        oracles_ui_display_texts(&nav, &t);
                        oracles_ui_layout_display(layout, &t, &first);
                    }
                    nav.row = row;
                    nav.display.core = choice % 2;
                    nav.display.vsync = choice;
                    nav.display.workers = 2 - choice;
                    nav.display.scaling = choice % 2;
                    nav.display.transitions = choice % 2;
                    nav.display.margin = choice + 1;
                    nav.display.profile = choice == 2 ? ORACLES_PROFILE_FAITHFUL : ORACLES_PROFILE_ENHANCED;
                    oracles_ui_display_texts(&nav, &t);
                    oracles_ui_layout_display(layout, &t, &other);
                    for (unsigned r = ORACLES_DISPLAY_CORE; r < ORACLES_DISPLAY_LETTERS; r++)
                        CHECK(!memcmp(&first.rows[r], &other.rows[r], sizeof first.rows[r]));
                    for (int c = 0; c < 2; c++) CHECK(!memcmp(&first.scaling[c].box, &other.scaling[c].box, sizeof first.scaling[c].box));
                    for (int c = 0; c < 3; c++) CHECK(!memcmp(&first.vsync[c].box, &other.vsync[c].box, sizeof first.vsync[c].box));
                    for (int c = 0; c < 2; c++) CHECK(!memcmp(&first.transitions[c].box, &other.transitions[c].box, sizeof first.transitions[c].box));
                    for (int c = 0; c < ORACLES_DISPLAY_MARGINS; c++) CHECK(!memcmp(&first.margin[c].box, &other.margin[c].box, sizeof first.margin[c].box));
                    CHECK(!memcmp(&first.panel, &other.panel, sizeof first.panel));
                }
    }
}

/* HUD (turn 19), after View: highlighted on Full width (19a) and on Original (19b), in 4:3 (19c) and at 640x480 (19d),
 * from the pause menu, live there (19e, 19f), and in Faithful, dimmed and inert (19g); on Wide layout (turn 22) in 16:9
 * (22a), in 4:3 (22b), at 640x480 on Android (22c), from the pause menu (22d) and in Faithful (22e). */
static void display_hud(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.display.window = 1;
    nav.row = ORACLES_DISPLAY_HUD;
    OraclesUiDisplayTexts t;
    OraclesUiDisplayLayout l;
    for (int hud = 1; hud >= 0; hud--) {
        const char *frame = hud ? "19a" : "19b";
        nav.display.hud = hud;
        oracles_ui_display_texts(&nav, &t);
        oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
        hud_probes(frame, &l);
        layout_box(frame, "picture row", &l.rows[ORACLES_DISPLAY_PICTURE]);
    }
    layout_box("19a", "View row", &l.rows[ORACLES_DISPLAY_VIEW]);
    layout_box("19a", "panel", &l.panel);
    nav.in_game = 1;
    nav.playing = ORACLES_PROFILE_ENHANCED;
    nav.display.hud = 1;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    hud_probes("19e", &l);
    layout_text("19e", "page note", &l.page_note.lines[0]);
    CHECK(!strcmp(oracles_ui_display_later, "Greyed rows change from the home screen; the rest applies at once."));
    layout_box("19e", "picture row", &l.rows[ORACLES_DISPLAY_PICTURE]);
    layout_box("19e", "panel", &l.panel);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_HUD, 0) == ORACLES_HOME_STORE && nav.display.hud == 0);
    nav.in_game = 0;
    nav.display.hud = 1;
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    hud_probes("19g", &l);
    layout_box("19g", "picture row", &l.rows[ORACLES_DISPLAY_PICTURE]);
    layout_box("19g", "panel", &l.panel);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.display.hud == 1);

    display_4_3_nav(&nav, 1440, 1080);
    nav.row = ORACLES_DISPLAY_HUD;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    hud_probes_4_3("19c", &l);
    layout_box("19c", "View row", &l.rows[ORACLES_DISPLAY_VIEW]);
    layout_box("19c", "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);
    nav.in_game = 1;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    hud_probes_4_3("19f", &l);
    layout_line("19f", "page note", &l.page_note.lines[0]);
    nav.in_game = 0;
    display_4_3_nav(&nav, 640, 480);
    nav.row = ORACLES_DISPLAY_HUD;
    nav.display.hud = 0;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    hud_probes_4_3("19d", &l);
    nav.display.hud = 2;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    hud_probes_4_3("22c", &l);
    display_4_3_nav(&nav, 1440, 1080);
    nav.row = ORACLES_DISPLAY_HUD;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    hud_probes_4_3("22b", &l);
    layout_box("22b", "View row", &l.rows[ORACLES_DISPLAY_VIEW]);
    layout_box("22b", "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);

    /* The Wide layout chosen, in 16:9: highlighted, from the pause menu, where it applies at once, and in Faithful. */
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.display.window = 1;
    nav.row = ORACLES_DISPLAY_HUD;
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_HUD, 2) == ORACLES_HOME_STORE && nav.display.hud == 2);
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    hud_probes("22a", &l);
    layout_box("22a", "View row", &l.rows[ORACLES_DISPLAY_VIEW]);
    layout_box("22a", "picture row", &l.rows[ORACLES_DISPLAY_PICTURE]);
    layout_box("22a", "panel", &l.panel);
    nav.in_game = 1;
    nav.playing = ORACLES_PROFILE_ENHANCED;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    hud_probes("22d", &l);
    layout_text("22d", "page note", &l.page_note.lines[0]);
    layout_box("22d", "picture row", &l.rows[ORACLES_DISPLAY_PICTURE]);
    layout_box("22d", "panel", &l.panel);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_HUD, 1) == ORACLES_HOME_STORE && nav.display.hud == 1);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_HUD, 2) == ORACLES_HOME_STORE && nav.display.hud == 2);
    nav.in_game = 0;
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    hud_probes("22e", &l);
    layout_box("22e", "picture row", &l.rows[ORACLES_DISPLAY_PICTURE]);
    layout_box("22e", "panel", &l.panel);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_HUD, 0) == ORACLES_HOME_STAY && nav.display.hud == 2);
}

/* Screen margin's probes: its label, its row and its grid, each choice's box, Small's and Large's names, and the rows
 * from Core's to its own. */
static void margin_probes(const char *frame, const OraclesUiDisplayLayout *l, int wide)
{
    static const char *const ids[ORACLES_DISPLAY_MARGINS] = { "off", "auto", "small", "large" };
    if (wide) layout_line(frame, "Screen margin", &l->labels[ORACLES_DISPLAY_MARGIN].lines[0]);
    else layout_text(frame, "Screen margin", &l->labels[ORACLES_DISPLAY_MARGIN].lines[0]);
    layout_box(frame, "margin row", &l->rows[ORACLES_DISPLAY_MARGIN]);
    layout_near(frame, "margin grid", "y", l->rows[ORACLES_DISPLAY_MARGIN].y);
    layout_near(frame, "margin grid", "h", l->rows[ORACLES_DISPLAY_MARGIN].h);
    for (int m = 0; m < ORACLES_DISPLAY_MARGINS; m++) {
        char what[32];
        snprintf(what, sizeof what, "margin %s", ids[m]); layout_box(frame, what, &l->margin[m].box);
    }
    if (wide) { layout_line(frame, "margin Small text", &l->margin[2].name); layout_line(frame, "margin Large text", &l->margin[3].name); }
    else { layout_text(frame, "margin Small text", &l->margin[2].name); layout_text(frame, "margin Large text", &l->margin[3].name); }
    layout_box(frame, "scaling row", &l->rows[ORACLES_DISPLAY_SCALING]);
    layout_near(frame, "rows", "y", l->rows[ORACLES_DISPLAY_SCALING].y);
    layout_near(frame, "rows", "h", l->rows[ORACLES_DISPLAY_UNVISITED].y + l->rows[ORACLES_DISPLAY_UNVISITED].h - l->rows[ORACLES_DISPLAY_SCALING].y);
}

/* Screen margin (turn 20), Display › Picture's second row since turn 34: in 16:9 its choices above its explanation, on two lines (margin), from
 * the pause menu (margin pause) and in Faithful, live in both (margin faithful); in 4:3 (margin 4:3, margin pause 4:3)
 * and at 640x480 (margin 640x480), its short help under the rows. */
static void display_margin(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.display.window = 1;
    nav.display.cores = 8;
    nav.picture_page = 1;
    nav.row = ORACLES_DISPLAY_MARGIN;
    nav.display.margin = 1;
    OraclesUiDisplayTexts t;
    OraclesUiDisplayLayout l;
    static const char *const frames[3] = { "margin", "margin pause", "margin faithful" };
    nav.playing = ORACLES_PROFILE_ENHANCED;
    for (int f = 0; f < 3; f++) {
        nav.in_game = f == 1;
        nav.display.profile = f == 2 ? ORACLES_PROFILE_FAITHFUL : ORACLES_PROFILE_ENHANCED;
        nav.display.window = f == 2 ? 3 : 1;
        nav.display.margin = f + 1;
        oracles_ui_display_texts(&nav, &t);
        oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
        margin_probes(frames[f], &l, 0);
        CHECK(l.explanations[7].count == 2);
        layout_text(frames[f], "margin explanation", &l.explanations[7].lines[0]);
        layout_near(frames[f], "margin explanation", "width", l.explanations[7].w);
        layout_box(frames[f], "panel", &l.panel);
        if (nav.in_game) layout_text(frames[f], "page note", &l.page_note.lines[0]);
        CHECK(!oracles_display_row_fixed(&nav, ORACLES_DISPLAY_MARGIN));
        CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_MARGIN, 0) == ORACLES_HOME_STORE && nav.display.margin == 0);
    }

    for (int f = 0; f < 3; f++) {
        const char *frame = f == 0 ? "margin 4:3" : f == 1 ? "margin 640x480" : "margin pause 4:3";
        display_4_3_nav(&nav, f == 1 ? 640 : 1440, f == 1 ? 480 : 1080);
        nav.picture_page = 1;
        nav.row = ORACLES_DISPLAY_MARGIN;
        nav.in_game = f == 2;
        nav.display.margin = f == 2 ? 0 : f + 1;
        oracles_ui_display_texts(&nav, &t);
        oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
        CHECK(!strcmp(t.help, "Keeps the picture clear of hidden screen edges.") && !t.help_note);
        margin_probes(frame, &l, 1);
        layout_line(frame, "help text", &l.help);
        layout_near(frame, "help", "y", l.help.y);
        layout_near(frame, "help note", "y", l.help_note.y);
        if (nav.in_game) layout_line(frame, "page note", &l.page_note.lines[0]);
    }
}

/* Camera's probes: its label, its row and its grid, Pixel's and Smooth's boxes and names, the rows around it. */
static void camera_probes(const char *frame, const OraclesUiDisplayLayout *l, int wide)
{
    if (wide) layout_line(frame, "Camera", &l->labels[ORACLES_DISPLAY_CAMERA].lines[0]);
    else layout_text(frame, "Camera", &l->labels[ORACLES_DISPLAY_CAMERA].lines[0]);
    layout_box(frame, "cam row", &l->rows[ORACLES_DISPLAY_CAMERA]);
    layout_near(frame, "cam grid", "y", l->rows[ORACLES_DISPLAY_CAMERA].y);
    layout_near(frame, "cam grid", "h", l->rows[ORACLES_DISPLAY_CAMERA].h);
    layout_box(frame, "cam Pixel", &l->camera[0].box);
    layout_box(frame, "cam Smooth", &l->camera[1].box);
    layout_box(frame, "cam Pixel box", &l->camera[0].box);
    layout_box(frame, "cam Smooth box", &l->camera[1].box);
    if (wide) { layout_line(frame, "cam Pixel text", &l->camera[0].name); layout_line(frame, "cam Smooth text", &l->camera[1].name); }
    else { layout_text(frame, "cam Pixel text", &l->camera[0].name); layout_text(frame, "cam Smooth text", &l->camera[1].name); }
    layout_box(frame, "workers row", &l->rows[ORACLES_DISPLAY_WORKERS]);
    layout_near(frame, "rows", "y", l->rows[ORACLES_DISPLAY_CORE].y);
    layout_near(frame, "rows", "h", l->rows[ORACLES_DISPLAY_CAMERA].y + l->rows[ORACLES_DISPLAY_CAMERA].h - l->rows[ORACLES_DISPLAY_CORE].y);
}

/* Camera (turn 30), the last Advanced row since turn 34: in 16:9 Smooth chosen and highlighted, its explanation on
 * three lines (30h), from the pause, live (30k), and in Faithful, dimmed and inert (30l); in 4:3 its short help (30i);
 * at 640x480 on Android, the far view at once, the note that it does nothing there (30j).  The value shown is the
 * player's, else the platform's default at the scale the game is shown at: Smooth at every scale on the desktops, on
 * Android from three times on. */
static void display_camera(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    game_1h(&nav.games[0]);
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.display.window = 2;
    nav.display.cores = 8;
    nav.advanced = 1;
    nav.row = ORACLES_DISPLAY_CAMERA;
    nav.display.camera = 1;
    OraclesUiDisplayTexts t;
    OraclesUiDisplayLayout l;
    static const char *const frames[3] = { "30h", "30k", "30l" };
    for (int f = 0; f < 3; f++) {
        nav.in_game = f == 1;
        nav.playing = ORACLES_PROFILE_ENHANCED;
        nav.display.profile = f == 2 ? ORACLES_PROFILE_FAITHFUL : ORACLES_PROFILE_ENHANCED;
        oracles_ui_display_texts(&nav, &t);
        oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
        camera_probes(frames[f], &l, 0);
        CHECK(l.camera_explanation.count == 3 && !t.camera_note);
        layout_text(frames[f], "cam explanation", &l.camera_explanation.lines[0]);
        layout_near(frames[f], "cam explanation", "width", l.camera_explanation.w);
        layout_box(frames[f], "panel", &l.panel);
        if (nav.in_game) layout_text(frames[f], "page note", &l.page_note.lines[0]);
        CHECK(!oracles_display_row_fixed(&nav, ORACLES_DISPLAY_CAMERA) && oracles_display_camera_applies(&nav) == (f != 2));
    }
    display_advanced_rows("30h", &t, &l);
    for (int f = 0; f < 2; f++) {
        const char *frame = f ? "30j" : "30i";
        display_4_3_nav(&nav, f ? 640 : 1440, f ? 480 : 1080);
        nav.advanced = 1;
        nav.row = ORACLES_DISPLAY_CAMERA;
        nav.display.camera = 1;
        nav.display.view = 2;
        oracles_ui_display_texts(&nav, &t);
        oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
        CHECK(!strcmp(t.help, "Smooth: diagonal walks glide."));
        camera_probes(frame, &l, 1);
        layout_box(frame, "core row", &l.rows[ORACLES_DISPLAY_CORE]);
        layout_line(frame, "help text", &l.help);
        layout_near(frame, "help", "y", l.help.y);
        layout_near(frame, "help note", "y", l.help_note.y);
        if (f) {
            CHECK(oracles_display_camera_scale(&nav) == 1u && t.help_note && !strcmp(t.help_note, "No effect at this size: the view fills the screen at 1\xc3\x97."));
            layout_line(frame, "help note text", &l.help_note);
        } else CHECK(!t.help_note && oracles_display_camera_scale(&nav) == 2u);   /* 4x reduced to the 2x that fits */
    }
    /* The default shown: on the desktops Smooth whatever the scale; on Android from three times on (Near 4:3's 213x160
     * at 3x on 640x480, not Medium's 320x240 at 2x nor Far's at 1x). */
    display_4_3_nav(&nav, 640, 480);
    nav.display.window = 3;
    nav.display.fullscreen_only = 1;
    for (int view = 0; view < 3; view++) {
        nav.display.view = view;
        nav.display.camera_from = 0;
        CHECK(oracles_display_camera(&nav) == 1);
        nav.display.camera_from = 3;
        CHECK(oracles_display_camera_scale(&nav) == (unsigned)(3 - view) && oracles_display_camera(&nav) == (view == 0));
        nav.display.camera = 0;
        CHECK(oracles_display_camera(&nav) == 0);
        nav.display.camera = 1;
        CHECK(oracles_display_camera(&nav) == 1);
        nav.display.camera = -1;
    }
}

int main(int argc, char **argv)
{
    if (argc < 2 || !layout_reference_load(argv[1])) { fprintf(stderr, "usage: oracles-test-launcher-page launcher_layout_reference.json\n"); return 2; }
    if (!oracles_ui_fonts_load()) { fprintf(stderr, "FAIL the embedded fonts do not load\n"); return 1; }
    layout_1h();
    layout_1i_1j();
    layout_1k_1l();
    layout_fan_page(&temple_frames);
    layout_fan_page(&kinomi_frames);
    navigation();
    layout_turn_14();
    layout_turn_15();
    layout_turn_16();
    languages_navigation();
    kept_row();
    display_2a_2b();
    display_9a_9b_9c();
    display_2q();
    display_2m();
    long_paths();
    android_folders();
    display_navigation();
    layout_10d();
    layout_10e();
    display_4_3();
    one_size();
    fill_one_size();
    display_quality();
    display_quality_navigation();
    display_scaling();
    display_hud();
    display_margin();
    display_camera();
    far_fitted_sizes();
    display_far_fitted();
    advanced_rows_stay();
    if (layout_failures()) { fprintf(stderr, "%d failure(s)\n", layout_failures()); return 1; }
    printf("launcher pages: Cartridge and Display match the layout reference within %.1f px in both layouts, and their rows and options behave as expected\n",
           (double)LAYOUT_TOLERANCE);
    return 0;
}
