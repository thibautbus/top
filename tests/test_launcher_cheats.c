/* Cheats (ui_page_nav.h, ui_page_layout.h): its layout against the layout
 * reference, tests/launcher_layout_reference.json (layout_reference.h), the
 * test's argument, in its frames 22h (No damage on, highlighted) at
 * 1920x1080, 22i (Rupees never run out highlighted) in the 4:3 layout at
 * 1440x1080, and 22j (Rupees never run out highlighted too, at 640x480 on
 * Android with a controller); a text is compared by its left, its width, its
 * line box and its baseline, an element by its box.  And the page's keys and
 * pointer, from the home screen and from the pause.  About, laid out as Cheats:
 * turn 32's 32g about in 16:9 and 32g about 4:3, 1.2.0 with 1.3.0 announced. */
#include "home_fixture.h"
#include "layout_reference.h"
#include "ui_page_layout.h"
#include "ui_page_nav.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); layout_fail(); } } while (0)

static const char *const ids[ORACLES_CHEATS_ROWS] = { "damage", "ammo", "rupees" };

/* The page of Ages, played on 21 Sep 2026, opened from its menu, No damage on, the highlight on `row`. */
static void page(OraclesHomeNav *nav, unsigned row)
{
    oracles_home_init(nav);
    OraclesHomeGame *g = &nav->games[ORACLES_HOME_GAME_AGES];
    g->usable = 1;
    g->rom = ORACLES_ROM_ORIGINAL;
    snprintf(g->last_session, sizeof g->last_session, "21 Sep 2026");
    g->cheats = 1u;
    nav->focus = 5;
    CHECK(oracles_home_act(nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav->screen == ORACLES_SCREEN_CHEATS && nav->row == 0);
    nav->row = row;
}

/* Each row's label, Off and On, the probes both layouts have. */
static void rows(const char *frame, const OraclesUiCheatsLayout *l)
{
    for (int r = 0; r < ORACLES_CHEATS_ROWS; r++) {
        char name[32];
        snprintf(name, sizeof name, "%s label", ids[r]); layout_line(frame, name, &l->labels[r]);
        snprintf(name, sizeof name, "%s off", ids[r]); layout_box(frame, name, &l->choices[r][0].box);
        snprintf(name, sizeof name, "%s on", ids[r]); layout_box(frame, name, &l->choices[r][1].box);
        snprintf(name, sizeof name, "%s off text", ids[r]); layout_line(frame, name, &l->choices[r][0].name);
        snprintf(name, sizeof name, "%s on text", ids[r]); layout_line(frame, name, &l->choices[r][1].name);
    }
}

static void frame_22h(void)
{
    OraclesHomeNav nav;
    page(&nav, 0);
    OraclesUiCheatsLayout l;
    oracles_ui_layout_cheats(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    layout_line("22h", "section", &l.head.section);
    layout_line("22h", "over", &l.head.over);
    layout_line("22h", "title", &l.head.title.lines[0]);
    /* The note on the save under the title, broken in the column's width. */
    layout_line("22h", "note", &l.note.lines[0]);
    layout_near("22h", "note", "lines", (float)l.note.count);
    layout_near("22h", "note", "width", l.note.w);
    layout_near("22h", "note box", "y", l.note.lines[0].y);
    layout_near("22h", "note box", "h", l.note.h);
    layout_box("22h", "panel", &l.panel);
    rows("22h", &l);
    for (int r = 0; r < ORACLES_CHEATS_ROWS; r++) {
        char name[32];
        snprintf(name, sizeof name, "%s row", ids[r]); layout_box("22h", name, &l.rows[r]);
        snprintf(name, sizeof name, "%s", ids[r]);
        layout_near("22h", name, "y", l.rows[r].y);
        layout_near("22h", name, "h", l.rows[r].h);
        if (!oracles_cheats_row_live(&nav, (unsigned)r)) continue;
        /* The explanation with its note, beside the choices, on as many lines as the browser breaks it into. */
        snprintf(name, sizeof name, "%s explanation", ids[r]);
        layout_line("22h", name, &l.explanations[r].lines[0]);
        layout_near("22h", name, "lines", (float)l.explanations[r].count);
        layout_near("22h", name, "width", l.explanations[r].w);
    }
    CHECK(l.help.h == 0.0f && l.help_note.h == 0.0f);
}

/* 4:3: one column of rows, the highlighted row's explanation and its note under them; the head and the note on one
 * line each; no panel, the rows and the help the mockup's panel.  `help` is the reference's explanation. */
static void frame_4_3(const char *frame, unsigned row, const char *help)
{
    OraclesHomeNav nav;
    page(&nav, row);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    OraclesUiCheatsLayout l;
    oracles_ui_layout_cheats(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    layout_line(frame, "section", &l.head.section);
    layout_line(frame, "title", &l.head.title.lines[0]);
    CHECK(l.head.over.h == 0.0f && !strcmp(l.head.title.text[0], "Oracle of Ages"));
    layout_line(frame, "note", &l.note.lines[0]);
    layout_near(frame, "head", "y", l.head.section.y);
    layout_near(frame, "head", "h", l.note.lines[0].y + l.note.lines[0].h - l.head.section.y);
    CHECK(l.panel.w == 0.0f);
    layout_near(frame, "panel", "y", l.rows[0].y);
    layout_near(frame, "panel", "h", l.help_note.y + l.help_note.h - l.rows[0].y);
    rows(frame, &l);
    for (int r = 0; r < ORACLES_CHEATS_ROWS; r++) {
        char name[32];
        snprintf(name, sizeof name, "%s", ids[r]); layout_box(frame, name, &l.rows[r]);
    }
    layout_near(frame, "help", "y", l.help.y);
    layout_near(frame, "help", "h", l.help.h);
    layout_line(frame, "help text", &l.help);
    CHECK(!strcmp(oracles_cheats_texts[row], help));
    layout_near(frame, "help note", "y", l.help_note.y);
    layout_near(frame, "help note", "h", l.help_note.h);
    if (row == 2) layout_line(frame, "help note text", &l.help_note);
}

/* 22j: the help bar with a controller, the page's four hints. */
static void hints_22j(void)
{
    OraclesHomeNav nav;
    page(&nav, 2);
    nav.android = 1;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    OraclesUiHintLayout h[ORACLES_HOME_MAX_HINTS];
    const unsigned count = oracles_home_hints(&nav, hints);
    CHECK(count == 4 && !strcmp(hints[0].key, "D-pad \xe2\x86\x91\xe2\x86\x93") && !strcmp(hints[3].label, "Back"));
    if (count != 4) return;
    oracles_ui_layout_hints(ORACLES_UI_LAYOUT_4_3, hints, count, h);
    for (unsigned i = 0; i < count; i++) {
        char name[32];
        snprintf(name, sizeof name, "hint %u", i); layout_box("22j", name, &h[i].box);
        snprintf(name, sizeof name, "hint %u key", i); layout_box("22j", name, &h[i].key_box);
        snprintf(name, sizeof name, "hint %u key text", i); layout_line("22j", name, &h[i].key);
        snprintf(name, sizeof name, "hint %u label", i); layout_line("22j", name, &h[i].label);
    }
}

/* The keys and the pointer: up and down wrap over the three rows; left, right and OK turn a cheat off and on, each
 * change a command to store the settings; Back returns to Cheats in the menu, or in the pause from a game. */
static void keys(void)
{
    OraclesHomeNav nav;
    page(&nav, 0);
    CHECK(oracles_cheats_on(&nav, 0) && !oracles_cheats_on(&nav, 1));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.row == 2);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && !oracles_cheats_on(&nav, 0) && nav.games[0].cheats == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && oracles_cheats_on(&nav, 0));
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.games[0].cheats == 5u);
    /* Down on Rupees never run out wraps to No damage: no row after it. */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == 0);
    CHECK(!oracles_cheats_row_live(&nav, ORACLES_CHEATS_ROWS) && !oracles_cheats_on(&nav, ORACLES_CHEATS_ROWS));
    /* A click on a row highlights it; on Off or On, sets it; the same choice again changes nothing. */
    CHECK(oracles_cheats_click(&nav, 1, -1) == ORACLES_HOME_STAY && nav.row == 1);
    CHECK(oracles_cheats_click(&nav, 1, 1) == ORACLES_HOME_STORE && nav.games[0].cheats == 7u);
    CHECK(oracles_cheats_click(&nav, 1, 1) == ORACLES_HOME_STAY && oracles_cheats_click(&nav, 0, 0) == ORACLES_HOME_STORE && nav.games[0].cheats == 6u);
    CHECK(oracles_cheats_click(&nav, ORACLES_CHEATS_ROWS, 1) == ORACLES_HOME_STAY && nav.games[0].cheats == 6u);
    /* Seasons' are its own. */
    CHECK(nav.games[ORACLES_HOME_GAME_SEASONS].cheats == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_HOME && oracles_home_focus(&nav) == 5);
    oracles_home_select(&nav, ORACLES_HOME_SEASONS);
    nav.focus = 5;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_CHEATS && !oracles_cheats_on(&nav, 1));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.games[ORACLES_HOME_GAME_SEASONS].cheats == 1u && nav.games[0].cheats == 6u);
    /* The host writes the cheats of the games the page changed, those alone. */
    CHECK(nav.cheats_changed == (1u << ORACLES_HOME_GAME_AGES | 1u << ORACLES_HOME_GAME_SEASONS));
    /* The page's hints: move, change, select, back. */
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    CHECK(oracles_home_hints(&nav, hints) == 4 && !strcmp(hints[1].label, "Change") && hints[3].back);

    /* From the pause, on Cheats after Display; the change is the same command, which the pause applies at once. */
    oracles_home_init(&nav);
    oracles_home_pause(&nav, ORACLES_HOME_GAME_SEASONS, ORACLES_PROFILE_FAITHFUL, 0);
    nav.focus = 5;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_CHEATS && nav.row == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.games[ORACLES_HOME_GAME_SEASONS].cheats == 1u);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_PAUSE && oracles_home_focus(&nav) == 5);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_RESUME);

    /* --cheats for the run: the page shows them, every row dimmed and inert, nothing to store. */
    page(&nav, 1);
    nav.cheats_fixed = 1;
    CHECK(oracles_cheats_on(&nav, 0) && !oracles_cheats_row_live(&nav, 0) && !oracles_cheats_row_live(&nav, 2));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && oracles_cheats_click(&nav, 0, 0) == ORACLES_HOME_STAY);
    CHECK(nav.games[0].cheats == 1u && nav.cheats_changed == 0);
}

/* A ROM whose profile has no cheats, an unrecognised one: the menu has no Cheats, nor the pause of a game whose session
 * says its profile has none; their other entries as before. */
static void refused(void)
{
    OraclesHomeNav nav;
    page(&nav, 0);
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    nav.games[ORACLES_HOME_GAME_AGES].rom = ORACLES_ROM_UNRECOGNISED;
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    CHECK(oracles_home_items(&nav, items) == 6 && items[4].id == ORACLES_HOME_ITEM_MODS && items[5].id == ORACLES_HOME_ITEM_EXIT);
    oracles_home_init(&nav);
    oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_FAITHFUL, 0);
    CHECK(oracles_home_items(&nav, items) == 7 && !items[5].disabled);
    nav.cheats_refused = 1;
    nav.focus = 5;
    CHECK(oracles_home_items(&nav, items) == 6 && items[5].id == ORACLES_HOME_ITEM_QUIT_GAME);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_QUIT_GAME);
}

/* The fan games, whose profiles have cheats, Gifts of Kinomi from its menu, Temple of Seasons and Moonrise Regalia from
 * their pause: the page changes the game's own, not its Oracle's nor another fan game's of the same Oracle,
 * and leaves the mods out of the rupees' note. */
static void fan_games(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    fan_profiles(&nav);
    nav.games[ORACLES_HOME_GAME_AGES].cheats = 4u;
    CHECK(oracles_home_game(&nav) == ORACLES_HOME_GAME_AGES && !strcmp(oracles_cheats_row_note(&nav, 2), "Mods still charge."));
    oracles_home_select(&nav, ORACLES_HOME_FAN);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && oracles_home_game(&nav) == ORACLES_HOME_GAME_KINOMI);
    nav.focus = 5;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_CHEATS && !oracles_cheats_on(&nav, 2));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && oracles_cheats_on(&nav, 0));
    CHECK(nav.games[ORACLES_HOME_GAME_KINOMI].cheats == 1u && nav.games[ORACLES_HOME_GAME_AGES].cheats == 4u);
    CHECK(nav.cheats_changed == 1u << ORACLES_HOME_GAME_KINOMI);
    /* A fan game has no mods: Rupees never run out says nothing of theirs; an Oracle's still does. */
    char explanation[256];
    oracles_cheats_explanation(&nav, 2, explanation, sizeof explanation);
    CHECK(!strcmp(oracles_cheats_row_note(&nav, 2), "") && !strcmp(explanation, "Buying costs nothing."));
    CHECK(!strcmp(oracles_cheats_row_note(&nav, 0), "Link is still knocked back."));
    oracles_home_init(&nav);
    fan_profiles(&nav);
    oracles_home_pause(&nav, ORACLES_HOME_GAME_TEMPLE, ORACLES_PROFILE_ENHANCED, 0);
    nav.games[ORACLES_HOME_GAME_TEMPLE].cheats = 2u;
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    CHECK(oracles_home_items(&nav, items) == 7 && items[5].id == ORACLES_HOME_ITEM_CHEATS);
    nav.focus = 5;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_CHEATS && oracles_cheats_on(&nav, 1));
    nav.row = 2;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.games[ORACLES_HOME_GAME_TEMPLE].cheats == 6u);
    CHECK(nav.games[ORACLES_HOME_GAME_SEASONS].cheats == 0 && nav.cheats_changed == 1u << ORACLES_HOME_GAME_TEMPLE);
    oracles_home_pause(&nav, ORACLES_HOME_GAME_MOONRISE, ORACLES_PROFILE_ENHANCED, 0);
    nav.cheats_changed = 0;
    CHECK(oracles_home_items(&nav, items) == 7 && items[5].id == ORACLES_HOME_ITEM_CHEATS);
    nav.focus = 5;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_CHEATS && !oracles_cheats_on(&nav, 0));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.games[ORACLES_HOME_GAME_MOONRISE].cheats == 1u);
    CHECK(nav.games[ORACLES_HOME_GAME_KINOMI].cheats == 0 && nav.cheats_changed == 1u << ORACLES_HOME_GAME_MOONRISE);
}

/* About from the version number of 1.2.0, 1.3.0 announced: "The Oracles" over "Project" (16:9) or the whole name on the
 * title's line (4:3), the version and the notice as the note, the row Update check, On chosen, always highlighted. */
static void about(OraclesUiLayout layout, const char *frame)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.layout = layout;
    nav.games[0].usable = 1;
    snprintf(nav.update, sizeof nav.update, "1.3.0");
    nav.focus = 8;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_ABOUT);
    char note[ORACLES_HOME_TEXT_LENGTH];
    OraclesUiCheatsLayout l;
    oracles_ui_layout_about(layout, &nav, "v1.2.0 (abc1234)", note, sizeof note, &l);
    CHECK(!strcmp(note, "v1.2.0 (abc1234) \xc2\xb7 1.3.0 available"));
    layout_line(frame, "section", &l.head.section);
    layout_line(frame, "title", &l.head.title.lines[0]);
    layout_line(frame, "note", &l.note.lines[0]);
    layout_near(frame, "note box", "y", l.note.lines[0].y);
    layout_line(frame, "update label", &l.labels[0]);
    layout_box(frame, "update on", &l.choices[0][0].box);
    layout_box(frame, "update off", &l.choices[0][1].box);
    layout_line(frame, "update on text", &l.choices[0][0].name);
    layout_line(frame, "update off text", &l.choices[0][1].name);
    CHECK(l.rows[1].w == 0.0f && l.rows[2].w == 0.0f);
    if (layout == ORACLES_UI_LAYOUT_4_3) {
        CHECK(l.head.over.h == 0.0f && !strcmp(l.head.title.text[0], "The Oracles Project"));
        layout_near(frame, "head", "h", l.note.lines[0].y + l.note.lines[0].h - l.head.section.y);
        layout_box(frame, "update", &l.rows[0]);
        layout_near(frame, "panel", "y", l.rows[0].y);
        layout_near(frame, "panel", "h", l.help_note.y + l.help_note.h - l.rows[0].y);
        layout_line(frame, "help text", &l.help);
        layout_line(frame, "help note text", &l.help_note);
        return;
    }
    layout_line(frame, "over", &l.head.over);
    layout_box(frame, "panel", &l.panel);
    layout_box(frame, "update row", &l.rows[0]);
    layout_near(frame, "update", "y", l.rows[0].y);
    layout_near(frame, "update", "h", l.rows[0].h);
    layout_line(frame, "update explanation", &l.explanations[0].lines[0]);
    layout_near(frame, "update explanation", "lines", (float)l.explanations[0].count);
    layout_near(frame, "update explanation", "width", l.explanations[0].w);
    CHECK(l.help.h == 0.0f);
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    OraclesUiHintLayout h[ORACLES_HOME_MAX_HINTS];
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    CHECK(oracles_home_hints(&nav, hints) == 4);
    oracles_ui_layout_hints(layout, hints, 4, h);
    for (unsigned i = 0; i < 4u; i++) {
        char name[32];
        snprintf(name, sizeof name, "hint %u", i); layout_box(frame, name, &h[i].box);
        snprintf(name, sizeof name, "hint %u label", i); layout_line(frame, name, &h[i].label);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2 || !layout_reference_load(argv[1])) { fprintf(stderr, "usage: oracles-test-launcher-cheats launcher_layout_reference.json\n"); return 2; }
    if (!oracles_ui_fonts_load()) { fprintf(stderr, "FAIL the embedded fonts do not load\n"); return 1; }
    frame_22h();
    frame_4_3("22i", 2, "Buying costs nothing.");
    frame_4_3("22j", 2, "Buying costs nothing.");
    hints_22j();
    keys();
    refused();
    fan_games();
    about(ORACLES_UI_LAYOUT_16_9, "32g about");
    about(ORACLES_UI_LAYOUT_4_3, "32g about 4:3");
    if (layout_failures()) { fprintf(stderr, "%d failure(s)\n", layout_failures()); return 1; }
    printf("launcher cheats: the page matches the layout reference within %.1f px, and its keys and pointer do what they say\n", (double)LAYOUT_TOLERANCE);
    return 0;
}
