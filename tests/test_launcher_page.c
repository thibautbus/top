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
 * where the Window row comes to one size) and its Advanced rows (10h, 10p); a
 * text is compared by its left, its width and its baseline (in 4:3 its line
 * box too). */
#include "layout_reference.h"
#include "ui_page_layout.h"
#include "ui_controls_nav.h"
#include "ui_page_nav.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); layout_fail(); } } while (0)

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
    t->rom_note = oracles_page_rom_note(nav);
    t->rom_hotkeys_note = oracles_page_rom_hotkeys_note(nav);
    t->play_note = oracles_page_play_note(nav);
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
    layout_text("1h", "ROM status", &l.rom_status);
    CHECK(t.rom_note[0] == 0 && l.rom_note.h == 0.0f && t.rom_hotkeys_note[0] == 0 && l.rom_hotkeys_note.h == 0.0f);
    layout_box("1h", "ROM dot", &l.rom_dot);
    layout_text("1h", "Choose ROM", &l.rom_button);
    layout_text("1h", "Choose ROM dot", &l.rom_button_dot);
    layout_box("1h", "ROM row", &l.rows[ORACLES_ROW_ROM]);
    layout_text("1h", "save file", &l.save_file);
    layout_text("1h", "save line", &l.save_line);
    layout_text("1h", "Open folder", &l.save_button);
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
    layout_text("1i", "status", &l.rom_status);
    /* Enhanced chosen in Display, and the item hotkeys on in Controls but unable to attach: a line each under the
     * status says the ROM plays in Faithful, without them, and the rows go down. */
    CHECK(!strcmp(t.rom_note, "Enhanced needs an original ROM: this one plays in Faithful"));
    CHECK(!strcmp(t.rom_hotkeys_note, "Item hotkeys need an original ROM: this one plays without them"));
    layout_text("1i", "note", &l.rom_note);
    layout_text("1i", "hotkeys note", &l.rom_hotkeys_note);
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
    layout_text("1j", "status", &l.rom_status);
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
    layout_text("1k", "base status", &l.rom_status);
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
    layout_text("1k", "Open folder", &l.save_button);
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
    layout_text("1l", "patch status", &l.patch_status);
    layout_box("1l", "patch row", &l.rows[ORACLES_ROW_PATCH]);
    layout_text("1l", "image status", &l.image_status);
    layout_text("1l", "save file", &l.save_file);
    layout_text("1l", "save line", &l.save_line);
    layout_box("1l", "Save row", &l.rows[ORACLES_ROW_SAVE]);
    layout_text("1l", "Play", &l.play);
    layout_box("1l", "panel", &l.panel);
    /* Before a patch is chosen, the page names the one the game expects. */
    CHECK(!strcmp(t.patch_status, "No patch: the BPS patch of Moonrise Regalia 1.0.6 is expected"));

    /* The other states, which no frame of the reference holds. */
    char text[ORACLES_HOME_TEXT_LENGTH];
    OraclesTone tone;
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
    CHECK(!strcmp(text, "Patched image not recognised, Faithful only") && tone == ORACLES_TONE_WARN);
    CHECK(oracles_page_profile(g, ORACLES_PROFILE_ENHANCED) == ORACLES_PROFILE_FAITHFUL && oracles_page_rom_note(&nav)[0] == 0);
    g->rom = ORACLES_ROM_UNRECOGNISED;
    oracles_page_rom_status(g, text, sizeof text, &tone);
    CHECK(!strcmp(text, "Unrecognised ROM, not Oracle of Ages (USA)"));
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
    CHECK(oracles_page_click(&nav, ORACLES_ROW_ROM) == ORACLES_HOME_CHOOSE_ROM && nav.row == ORACLES_ROW_ROM);
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
    /* Moonrise Regalia's Cartridge: Base ROM, Patch, Save, Play, in that order; its Base ROM is the Ages ROM's dialog. */
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
    CHECK(nav.row == ORACLES_ROW_PATCH && oracles_page_click(&nav, ORACLES_ROW_PATCH) == ORACLES_HOME_CHOOSE_PATCH);
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
    oracles_page_hover(&nav, ORACLES_ROW_PATCH);
    CHECK(nav.row == ORACLES_ROW_ROM && oracles_page_click(&nav, ORACLES_ROW_PATCH) == ORACLES_HOME_STAY);
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
    layout_text(p, "base status", &l.rom_status);
    layout_box(p, "base row", &l.rows[ORACLES_ROW_ROM]);
    layout_text(p, "patch file", &l.patch_file);
    layout_text(p, "patch status", &l.patch_status);
    layout_box(p, "patch row", &l.rows[ORACLES_ROW_PATCH]);
    layout_text(p, "patch folder", &l.patch_folder);
    snprintf(expected, sizeof expected, "Patched image recognised: %s", f->file_stem);
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
    layout_text(p, "patch status", &l.patch_status);
    layout_box(p, "patch row", &l.rows[ORACLES_ROW_PATCH]);
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
    snprintf(expected, sizeof expected, "Unrecognised ROM, not %s", f->base_name);
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
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    /* No state line under the title: the diagram comes 56 px under it. */
    layout_text("2a", "section", &l.head.section);
    layout_box("2a", "diagram", &l.diagram);
    /* The diagram shows the window Play will open: 4x reduced to 3x, 1440x810, centred on the screen. */
    layout_box("2a", "diagram window", &l.diagram_window);
    layout_text("2a", "diagram label", &l.diagram_label);
    CHECK(!strcmp(t.diagram_label, "1440\xc3\x97" "810 on a 1920\xc3\x97" "1080 screen"));
    /* Profile first: the two profiles framed with their surfaces, the chosen one's note. */
    layout_text("2a", "Profile", &l.labels[ORACLES_DISPLAY_PROFILE].lines[0]);
    for (int p = 0; p < 2; p++) {
        char what[32];
        snprintf(what, sizeof what, "profile %d", p); layout_text("2a", what, &l.profiles[p].name);
        snprintf(what, sizeof what, "profile %d size", p); layout_text("2a", what, &l.profiles[p].size);
    }
    layout_text("2a", "profile note", &l.profile_note);
    layout_box("2a", "Profile row", &l.rows[ORACLES_DISPLAY_PROFILE]);
    /* Window next. */
    layout_text("2a", "Window", &l.labels[ORACLES_DISPLAY_WINDOW].lines[0]);
    static const char *const sizes[4] = { "960\xc3\x97" "540", "1440\xc3\x97" "810", "1920\xc3\x97" "1080", "4\xc3\x97 \xc2\xb7 1920\xc3\x97" "1080" };
    for (int i = 0; i < 4; i++) {
        char what[32];
        snprintf(what, sizeof what, "window %d", i); layout_text("2a", what, &l.windows[i].name);
        snprintf(what, sizeof what, "window %d size", i); layout_text("2a", what, &l.windows[i].size);
        CHECK(!strcmp(t.window_sizes[i], sizes[i]));
    }
    layout_text("2a", "window note", &l.window_note);
    /* 4x makes 1920x1080: with its title bar the window is reduced to 3x on a 1080p screen, and a line says so. */
    CHECK(!strcmp(t.window_reduced, "Reduced to 3\xc3\x97 to fit this screen") && oracles_display_fit(&nav) == 3);
    layout_text("2a", "reduced", &l.window_reduced);
    layout_box("2a", "Window row", &l.rows[ORACLES_DISPLAY_WINDOW]);
    /* View under it: each level framed with its size, far chosen; its explanation on two lines beside them. */
    static const char *const view_probes[3] = { "Near", "Medium", "Far" };
    CHECK(nav.display.view == 2 && l.view_explanation.count == 2);
    layout_text("2a", "View", &l.labels[ORACLES_DISPLAY_VIEW].lines[0]);
    layout_near("2a", "view explanation", "top", l.view_explanation.lines[0].y);
    layout_near("2a", "view explanation", "width", l.view_explanation.w);
    for (int v = 0; v < 3; v++) {
        char what[32];
        snprintf(what, sizeof what, "view %d", v); layout_text("2a", what, &l.views[v].name);
        snprintf(what, sizeof what, "view %d size", v); layout_text("2a", what, &l.views[v].size);
        snprintf(what, sizeof what, "view %s", view_probes[v]); layout_box("2a", what, &l.views[v].box);
    }
    CHECK(!strcmp(t.view_sizes[1], "384\xc3\x97" "216") && !strcmp(t.profile_sizes[ORACLES_PROFILE_ENHANCED], "480\xc3\x97" "270"));
    layout_box("2a", "View row", &l.rows[ORACLES_DISPLAY_VIEW]);
    /* Color correction: its label and its explanation each on two lines. */
    CHECK(l.labels[ORACLES_DISPLAY_COLOUR].count == 2 && l.explanations[0].count == 2);
    CHECK(!strcmp(l.labels[ORACLES_DISPLAY_COLOUR].text[0], "Color"));
    layout_near("2a", "Color correction", "top", l.labels[ORACLES_DISPLAY_COLOUR].lines[0].y);
    layout_near("2a", "Color correction", "width", l.labels[ORACLES_DISPLAY_COLOUR].w);
    layout_near("2a", "color explanation", "top", l.explanations[0].lines[0].y);
    layout_near("2a", "color explanation", "width", l.explanations[0].w);
    layout_box("2a", "color Off", &l.colour[0].box);
    layout_box("2a", "color On", &l.colour[1].box);
    /* Continuous transitions under it, a row like the others: its label on two lines, its explanation and the note on
     * savestates beside the choices. */
    CHECK(l.labels[ORACLES_DISPLAY_TRANSITIONS].count == 2);
    layout_near("2a", "Continuous transitions", "top", l.labels[ORACLES_DISPLAY_TRANSITIONS].lines[0].y);
    layout_near("2a", "Continuous transitions", "width", l.labels[ORACLES_DISPLAY_TRANSITIONS].w);
    layout_near("2a", "transitions explanation", "top", l.explanations[1].lines[0].y);
    layout_near("2a", "transitions explanation", "width", l.explanations[1].w);
    layout_text("2a", "transitions note", &l.transitions_note);
    layout_box("2a", "transitions Off", &l.transitions[0].box);
    layout_box("2a", "transitions On", &l.transitions[1].box);
    layout_box("2a", "transitions row", &l.rows[ORACLES_DISPLAY_TRANSITIONS]);
    /* The transitions are the panel's last row; Advanced sits under the diagram, its highlight past its label on both
     * sides; the Advanced rows are not laid out. */
    layout_box("2a", "panel", &l.panel);
    layout_text("2a", "Advanced", &l.advanced_label);
    layout_near("2a", "Advanced", "top", l.advanced_label.y);
    layout_box("2a", "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);
    CHECK(l.rows[ORACLES_DISPLAY_CORE].w == 0.0f && l.rows[ORACLES_DISPLAY_VSYNC].w == 0.0f && l.rows[ORACLES_DISPLAY_WORKERS].w == 0.0f);
    CHECK(!strcmp(t.section, "Display"));

    /* 2g: opened from the game, a note under the title says what the next Play takes, once for the page; color
     * correction applies at once; the core is the game's, its row dimmed and inert. */
    nav.in_game = 1;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    layout_text("2g", "page note", &l.page_note);
    layout_near("2g", "Window row", "y", l.rows[ORACLES_DISPLAY_WINDOW].y);
    layout_near("2g", "View row", "y", l.rows[ORACLES_DISPLAY_VIEW].y);
    layout_near("2g", "View row", "h", l.rows[ORACLES_DISPLAY_VIEW].h);
    layout_near("2g", "Color row", "y", l.rows[ORACLES_DISPLAY_COLOUR].y);
    CHECK(l.explanations[0].count == 2);
    layout_near("2g", "transitions note", "top", l.transitions_note.y);
    layout_box("2g", "transitions Off", &l.transitions[0].box);
    layout_box("2g", "panel", &l.panel);
    /* Advanced under the diagram, which the note moved down. */
    layout_text("2g", "Advanced", &l.advanced_label);
    layout_box("2g", "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);
    /* The panel ends above the pause's key hints, which start at 1025 in 1080p. */
    CHECK(l.panel.y + l.panel.h <= 1025.0f);
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
    layout_box("2b", "transitions Off", &l.transitions[0].box);
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
 * Vsync; the neighbour workers, their label and explanation on two lines, Auto naming the two of an eight-core device. */
static void display_advanced_rows(const char *frame, const OraclesUiDisplayTexts *t, const OraclesUiDisplayLayout *l)
{
    layout_text(frame, "section", &l->head.section);
    CHECK(!strcmp(t->section, "Display \xe2\x80\xba Advanced"));
    CHECK(l->rows[ORACLES_DISPLAY_PROFILE].w == 0.0f && l->rows[ORACLES_DISPLAY_TRANSITIONS].w == 0.0f && l->rows[ORACLES_DISPLAY_ADVANCED].w == 0.0f
          && l->advanced_label.w == 0.0f);
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
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_16_9, &t, &l);
    display_advanced_rows("9c", &t, &l);
    layout_text("9c", "page note", &l.page_note);
    layout_box("9c", "diagram", &l.diagram);
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
    /* The window goes round 2x, 3x, 4x, fullscreen. */
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
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_DISPLAY_COLOUR && oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.colour == 1);
    /* The transitions toggle in Enhanced, and not in Faithful. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_DISPLAY_TRANSITIONS && nav.display.transitions == 1);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.display.transitions == 0);
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.display.transitions == 0);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_TRANSITIONS, 1) == ORACLES_HOME_STAY);
    /* Advanced after the transitions, last: left does nothing there, right and OK open its rows on Core.  Down from it
     * wraps to Profile, up from Profile to it. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_DISPLAY_ADVANCED && !nav.advanced);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && !nav.advanced);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_PROFILE);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_ADVANCED);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.advanced && nav.row == ORACLES_DISPLAY_CORE);
    CHECK(!strcmp(oracles_display_section(&nav), "Display \xe2\x80\xba Advanced"));
    /* Its rows: Core, Vsync, the workers, going round; Display's own are not there. */
    CHECK(oracles_display_row_shown(&nav, ORACLES_DISPLAY_WORKERS) && !oracles_display_row_shown(&nav, ORACLES_DISPLAY_PROFILE)
          && !oracles_display_row_shown(&nav, ORACLES_DISPLAY_ADVANCED));
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_PROFILE, 0) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_CORE);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_WORKERS);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_CORE);
    /* The two cores by a key and by a click; from a game the core is the game's and does not change. */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.core == 1);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_CORE, 0) == ORACLES_HOME_STORE && nav.display.core == 0);
    nav.in_game = 1;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.display.core == 0);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_CORE, 1) == ORACLES_HOME_STAY && nav.display.core == 0);
    nav.in_game = 0;
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_DISPLAY_VSYNC && oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.display.vsync == 2);
    /* Vsync changes from a game too. */
    nav.in_game = 1;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.vsync == 0);
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
    /* Back closes them on Advanced; a click on Advanced opens them again, Back closes them. */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_DISPLAY && !nav.advanced
          && nav.row == ORACLES_DISPLAY_ADVANCED && !strcmp(oracles_display_section(&nav), "Display"));
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_ADVANCED, -1) == ORACLES_HOME_STAY && nav.advanced && nav.row == ORACLES_DISPLAY_CORE);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_VSYNC, -1) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_VSYNC);
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(!nav.advanced && nav.row == ORACLES_DISPLAY_ADVANCED);
    /* From a game Advanced opens on Vsync, the one of its rows that changes there, by a key as by a click. */
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
    CHECK(!strcmp(reduced, "Reduced to 3\xc3\x97 to fit this screen"));
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
    layout_line("10d", "ROM status", &l.rom_status);
    layout_box("10d", "ROM dot", &l.rom_dot);
    layout_line("10d", "Choose ROM", &l.rom_button);
    layout_line("10d", "Choose ROM dot", &l.rom_button_dot);
    layout_box("10d", "ROM row", &l.rows[ORACLES_ROW_ROM]);
    layout_line("10d", "save file", &l.save_file);
    layout_line("10d", "save line", &l.save_line);
    layout_line("10d", "Open folder", &l.save_button);
    layout_line("10d", "Open folder dot", &l.save_button_dot);
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
                                              st->mods_empty, st->mods_note };
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
    layout_line("10e", "base status", &l.rom_status);
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

/* Display in 4:3 on the mockup's screen of `width` x `height`.  The mockup's far view in 4:3 is 480x270, the size the
 * port gives the 16:9 shape (480x360 in 4:3, docs/PLAYING.md): the frames' texts are laid out with aspect=16:9, the
 * diagram's box, the screen's shape, checked apart. */
static void display_4_3_nav(OraclesHomeNav *nav, int width, int height)
{
    oracles_home_init(nav);
    game_1h(&nav->games[0]);
    nav->layout = ORACLES_UI_LAYOUT_4_3;
    nav->screen = ORACLES_SCREEN_DISPLAY;
    nav->display.profile = ORACLES_PROFILE_ENHANCED;
    nav->display.window = 2;
    nav->display.cores = 8;
    nav->display.aspect = 1;
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
    /* 10f: the rows in one column, the explanation of the highlighted one, Profile, once under them. */
    layout_line("10f", "section", &l.head.section);
    CHECK(!strcmp(l.head.title.text[0], "Oracle of Ages") && l.head.state.h == 0.0f && l.page_note.h == 0.0f);
    layout_line("10f", "title", &l.head.title.lines[0]);
    layout_near("10f", "diagram", "y", l.diagram.y);
    layout_near("10f", "diagram", "h", l.diagram.h);
    layout_near("10f", "diagram", "x", l.diagram.x + l.diagram.w - layout_reference("10f", "diagram", "w"));
    layout_near("10f", "diagram window", "h", l.diagram_window.h);
    layout_line("10f", "diagram label", &l.diagram_label);
    layout_line("10f", "Profile", &l.labels[ORACLES_DISPLAY_PROFILE].lines[0]);
    static const char *const profiles[2] = { "Faithful", "Enhanced" };
    for (int p = 0; p < ORACLES_PROFILES; p++) {
        char what[32];
        snprintf(what, sizeof what, "profile %d", p); layout_line("10f", what, &l.profiles[p].name);
        snprintf(what, sizeof what, "profile %d size", p); layout_line("10f", what, &l.profiles[p].size);
        snprintf(what, sizeof what, "profile %s", profiles[p]); layout_box("10f", what, &l.profiles[p].box);
    }
    layout_box("10f", "Profile row", &l.rows[ORACLES_DISPLAY_PROFILE]);
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
    CHECK(l.labels[ORACLES_DISPLAY_COLOUR].count == 2 && l.labels[ORACLES_DISPLAY_TRANSITIONS].count == 2);
    layout_line("10f", "Color correction", &l.labels[ORACLES_DISPLAY_COLOUR].lines[0]);
    layout_line("10f", "color Off text", &l.colour[0].name);
    layout_box("10f", "color Off", &l.colour[0].box);
    layout_box("10f", "color On", &l.colour[1].box);
    layout_box("10f", "Color row", &l.rows[ORACLES_DISPLAY_COLOUR]);
    layout_line("10f", "Continuous transitions", &l.labels[ORACLES_DISPLAY_TRANSITIONS].lines[0]);
    layout_box("10f", "transitions Off", &l.transitions[0].box);
    layout_box("10f", "transitions On", &l.transitions[1].box);
    layout_box("10f", "transitions row", &l.rows[ORACLES_DISPLAY_TRANSITIONS]);
    CHECK(l.explanations[0].count == 0 && l.view_explanation.count == 0 && l.transitions_note.h == 0.0f);
    layout_line("10f", "Advanced", &l.advanced_label);
    layout_line("10f", "advanced text", &l.advanced_text);
    layout_box("10f", "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);
    CHECK(l.advanced_arrow.x == l.advanced_text.x + l.advanced_text.w + 16.0f && l.advanced_arrow.w == 13.0f && l.advanced_arrow.h == 18.0f);
    CHECK(!strcmp(t.help, "The wide world, drawn back, with continuous transitions and the smooth camera.") && !t.help_note);
    layout_line("10f", "help text", &l.help.lines[0]);
    layout_near("10f", "help", "y", l.help.lines[0].y);
    for (unsigned r = 0; r <= ORACLES_DISPLAY_ADVANCED; r++) CHECK(l.rows[r].h >= 90.0f);
    /* The diagram's box on a 4:3 screen, the shape the sizes take there: 162 wide. */
    nav.display.aspect = 0;
    OraclesUiDisplayTexts shaped;
    OraclesUiDisplayLayout sl;
    oracles_ui_display_texts(&nav, &shaped);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &shaped, &sl);
    layout_box("10f", "diagram", &sl.diagram);
    nav.display.aspect = 1;

    /* 10g: View highlighted, its explanation under the rows; the window's note and the reduced line under Window's. */
    nav.row = ORACLES_DISPLAY_VIEW;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    layout_line("10g", "help text", &l.help.lines[0]);
    layout_near("10g", "help", "y", l.help.lines[0].y);
    nav.row = ORACLES_DISPLAY_WINDOW;
    oracles_ui_display_texts(&nav, &t);
    CHECK(!strcmp(t.help, oracles_display_window_note) && !strcmp(t.help_note, "Reduced to 3\xc3\x97 to fit this screen"));
    nav.row = ORACLES_DISPLAY_ADVANCED;
    oracles_ui_display_texts(&nav, &t);
    CHECK(!strcmp(t.help, "Core, Vsync and neighbour workers.") && !t.help_note);

    /* 10o: from the pause menu, the note at the state's place; the rows where they were. */
    nav.row = ORACLES_DISPLAY_PROFILE;
    nav.in_game = 1;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    layout_line("10o", "page note", &l.page_note);
    layout_box("10o", "Window row", &l.rows[ORACLES_DISPLAY_WINDOW]);
    layout_box("10o", "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);
    layout_near("10o", "help", "y", l.help.lines[0].y);

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
        layout_near(frame, "help", "y", l.help.lines[0].y);
        layout_line(frame, "help note", &l.help_note);
        CHECK(l.rows[ORACLES_DISPLAY_PROFILE].w == 0.0f && l.rows[ORACLES_DISPLAY_ADVANCED].w == 0.0f && l.advanced_label.h == 0.0f);
        if (in_game) { layout_line(frame, "page note", &l.page_note); continue; }
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
        CHECK(l.labels[ORACLES_DISPLAY_WORKERS].count == 2);
        layout_line(frame, "Neighbour workers", &l.labels[ORACLES_DISPLAY_WORKERS].lines[0]);
        static const char *const workers[3] = { "Auto", "One", "Two" };
        for (int i = 0; i < 3; i++) {
            char what[32];
            snprintf(what, sizeof what, "workers %d", i); layout_line(frame, what, &l.workers[i].name);
            snprintf(what, sizeof what, "workers %s", workers[i]); layout_box(frame, what, &l.workers[i].box);
        }
        layout_line(frame, "help text", &l.help.lines[0]);
    }

    /* 10s: a 640x480 screen, where every window comes to one size: the Window row taller with its note, inert. */
    display_4_3_nav(&nav, 640, 480);
    CHECK(oracles_display_one_size(&nav));
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    CHECK(t.window_one && !strcmp(t.window_reduced, oracles_display_one_size_note));
    layout_line("10s", "diagram label", &l.diagram_label);
    layout_line("10s", "Window", &l.labels[ORACLES_DISPLAY_WINDOW].lines[0]);
    layout_box("10s", "Window row", &l.rows[ORACLES_DISPLAY_WINDOW]);
    for (int i = 0; i < 4; i++) {
        char what[32];
        snprintf(what, sizeof what, "window %d box", i); layout_box("10s", what, &l.windows[i].box);
    }
    layout_line("10s", "window 3 size", &l.windows[3].size);
    layout_line("10s", "one size", &l.window_one);
    layout_box("10s", "View row", &l.rows[ORACLES_DISPLAY_VIEW]);
    layout_box("10s", "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);
    layout_near("10s", "help", "y", l.help.lines[0].y);
    nav.display.aspect = 0;
    oracles_ui_display_texts(&nav, &t);
    oracles_ui_layout_display(ORACLES_UI_LAYOUT_4_3, &t, &l);
    layout_box("10s", "diagram", &l.diagram);
}

/* Where every window comes to one size, the Window row keeps the highlight but changes no more, by a key or a click,
 * and says so under the windows in 16:9 too; a platform always fullscreen likewise. */
static void one_size(void)
{
    OraclesHomeNav nav;
    display_4_3_nav(&nav, 640, 480);
    nav.layout = ORACLES_UI_LAYOUT_16_9;
    nav.row = ORACLES_DISPLAY_PROFILE;
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
    display_2a_2b();
    display_9a_9b_9c();
    display_2q();
    display_2m();
    long_paths();
    display_navigation();
    layout_10d();
    layout_10e();
    display_4_3();
    one_size();
    if (layout_failures()) { fprintf(stderr, "%d failure(s)\n", layout_failures()); return 1; }
    printf("launcher pages: Cartridge and Display match the layout reference within %.1f px in both layouts, and their rows and options behave as expected\n",
           (double)LAYOUT_TOLERANCE);
    return 0;
}
