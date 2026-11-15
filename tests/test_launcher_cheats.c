/* Cheats (ui_page_nav.h, ui_page_layout.h): its layout against the layout
 * reference, tests/launcher_layout_reference.json (layout_reference.h), the
 * test's argument, in its frames 22h (No damage on, highlighted) at
 * 1920x1080, 22i (Rupees never run out highlighted) in the 4:3 layout at
 * 1440x1080, and 22j (Rupees never run out highlighted too, at 640x480 on
 * Android with a controller); a text is compared by its left, its width, its
 * line box and its baseline, an element by its box.  And the page's keys and
 * pointer, from the home screen and from the pause.  Settings, laid out as Cheats, its About block under the rows:
 * turn 39's 39q in 16:9 and 39q 4:3, and its Licences' keys; and its Portable mode, with its hint and
 * its status, in 16:9 (portable on, portable copying, portable off kept, portable read only) and in 4:3 (portable 4:3,
 * portable 4:3 kept). */
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
        /* The explanation with its note, beside the choices, on as many lines as the browser breaks it into, one line centred in two. */
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

/* Settings (turn 39) from its button beside the version, on Windows: "The Oracles" over "Project" (16:9) or the whole
 * name on the title's line (4:3), no note, its rows Update check and Portable mode, then the About block: its label, the
 * version, the source, the cores' credits and Licences with its line; in 16:9 Update check highlighted after a key (39q),
 * in 4:3 Licences, two places down, its help under the rows (39q 4:3). */
static void settings(OraclesUiLayout layout, const char *frame)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.layout = layout;
    nav.windows = 1;
    nav.portable_shown = 1;
    nav.games[0].usable = 1;
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    nav.focus = oracles_home_items(&nav, items);
    CHECK(oracles_home_version_focused(&nav));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_ABOUT && nav.row == 0);
    CHECK(oracles_about_rows(&nav) == 3 && oracles_about_row_kind(&nav, 2) == ORACLES_ABOUT_ROW_LICENCES);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    if (layout == ORACLES_UI_LAYOUT_4_3) {
        CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY);
        CHECK(nav.row == 2);
    }
    OraclesUiCheatsLayout l;
    oracles_ui_layout_about(layout, &nav, "v1.5.0", "thibautbus/top", &l);
    const struct OraclesUiAboutBlock *b = &l.about;
    layout_line(frame, "section", &l.head.section);
    layout_line(frame, "title", &l.head.title.lines[0]);
    CHECK(l.note.count <= 1u && l.note.w == 0.0f);
    layout_line(frame, "update label", &l.labels[0]);
    layout_box(frame, "update on", &l.choices[0][0].box);
    layout_box(frame, "update off", &l.choices[0][1].box);
    layout_line(frame, "portable label", &l.labels[1]);
    CHECK(l.rows[2].w == 0.0f && b->shown);
    CHECK(!strcmp(b->title_text, "The Oracles Project v1.5.0"));
    layout_near(frame, "about", "y", b->rule.y);
    layout_near(frame, "about", "x", b->rule.x);
    layout_near(frame, "about", "w", b->rule.w);
    layout_box(frame, "about row", &b->row);
    layout_line(frame, "about label text", &b->label);
    layout_line(frame, "about title text", &b->title);
    layout_line(frame, "source text", &b->source);
    layout_line(frame, "credits text", &b->credits);
    layout_line(frame, "licences text", &b->licences);
    layout_line(frame, "licences line text", &b->list);
    layout_near(frame, "licences", "y", b->link.y);
    layout_near(frame, "licences", "h", b->link.h);
    layout_near(frame, "licences", "x", b->link.x);
    CHECK(b->dot.x > b->licences.x + b->licences.w && b->list.x > b->dot.x + b->dot.w);
    if (layout == ORACLES_UI_LAYOUT_4_3) {
        CHECK(l.head.over.h == 0.0f && !strcmp(l.head.title.text[0], "The Oracles Project"));
        layout_box(frame, "update", &l.rows[0]);
        layout_box(frame, "portable", &l.rows[1]);
        layout_near(frame, "panel", "y", l.rows[0].y);
        layout_near(frame, "panel", "h", b->row.y + b->row.h - l.rows[0].y);
        layout_near(frame, "about", "h", b->row.h);
        layout_line(frame, "help text", &l.help);
        layout_line(frame, "help note text", &l.help_note);
        CHECK(!strcmp(l.help_text, oracles_about_licences_help) && !strcmp(l.help_note_text, oracles_about_licences_help_note));
        return;
    }
    layout_near(frame, "about", "h", b->row.y + b->row.h - b->rule.y);
    layout_line(frame, "over", &l.head.over);
    layout_box(frame, "panel", &l.panel);
    layout_box(frame, "update row", &l.rows[0]);
    layout_box(frame, "portable row", &l.rows[1]);
    layout_line(frame, "update explanation", &l.explanations[0].lines[0]);
    CHECK(l.help.h == 0.0f);
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    OraclesUiHintLayout h[ORACLES_HOME_MAX_HINTS];
    CHECK(oracles_home_hints(&nav, hints) == 4);
    oracles_ui_layout_hints(layout, hints, 4, h);
    for (unsigned i = 0; i < 4u; i++) {
        char name[32];
        snprintf(name, sizeof name, "hint %u", i); layout_box(frame, name, &h[i].box);
        snprintf(name, sizeof name, "hint %u label", i); layout_line(frame, name, &h[i].label);
    }
}

/* Settings' keys and pointer: Licences, its last place, opened by OK, right and a click on its line, not by left or a
 * click elsewhere in the block; Back to the home screen, on the Settings button. */
static void licences(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.games[0].usable = 1;
    nav.screen = ORACLES_SCREEN_ABOUT;
    CHECK(oracles_about_rows(&nav) == 2 && oracles_about_row_kind(&nav, 1) == ORACLES_ABOUT_ROW_LICENCES);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.row == 1);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_OPEN_LICENCES && oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_OPEN_LICENCES);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && nav.update_check);
    CHECK(oracles_about_click(&nav, 1, -1) == ORACLES_HOME_STAY && oracles_about_click(&nav, 1, 2) == ORACLES_HOME_OPEN_LICENCES);
    CHECK(oracles_about_click(&nav, 0, 2) == ORACLES_HOME_STAY && nav.update_check);
    CHECK(!oracles_home_command_needs_game(ORACLES_HOME_OPEN_LICENCES));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_HOME && oracles_home_version_focused(&nav));
}

/* About's Portable mode, on Windows or Linux's archive, highlighted under Update check: On with a game outside the
 * launcher's folder, its hint under the row (portable on); while it copies, "Copying…" (portable copying); Off after a
 * switch that kept the user folder's settings, the longest message, two lines in the warning's tone (portable off kept);
 * the launcher's folder not writable, two lines in the error's (portable read only); in 4:3 after a key, the hint and
 * "Settings and mods copied to the game folder." (portable 4:3), and the longest message (portable 4:3 kept), the row's
 * help under them. */
static void portable(OraclesUiLayout layout, const char *frame, int on, int status, unsigned outside, OraclesInputDevice device)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.layout = layout;
    nav.windows = 1;   /* the prototype's wording */
    nav.games[0].usable = 1;
    nav.portable_shown = 1;
    nav.focus = 7;   /* the Settings button */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_ABOUT);
    oracles_home_input(&nav, device);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_ABOUT_ROW_PORTABLE);
    nav.portable = on;
    nav.portable_status = status;
    nav.games_outside = outside;
    OraclesUiCheatsLayout l;
    oracles_ui_layout_about(layout, &nav, "v0.1.0", "thibautbus/top", &l);
    layout_line(frame, "update label", &l.labels[0]);
    layout_line(frame, "portable label", &l.labels[1]);
    layout_box(frame, "portable off", &l.choices[1][0].box);
    layout_box(frame, "portable on", &l.choices[1][1].box);
    layout_line(frame, "portable off text", &l.choices[1][0].name);
    layout_line(frame, "portable on text", &l.choices[1][1].name);
    const float bottom = l.status.count ? l.status.lines[0].y + l.status.h : l.hint.count ? l.hint.lines[0].y + l.hint.h : l.rows[1].y + l.rows[1].h;
    if (outside && on) {
        layout_near(frame, "portable hint", "y", l.hint.lines[0].y);
        layout_near(frame, "portable hint", "h", l.hint.h);
        layout_line(frame, "portable hint text", &l.hint.lines[0]);
        layout_near(frame, "portable hint text", "lines", (float)l.hint.count);
    } else CHECK(l.hint.count == 0);
    if (status) {
        layout_near(frame, "portable status", "y", l.status.lines[0].y);
        layout_near(frame, "portable status", "h", l.status.h);
        layout_line(frame, "portable status text", &l.status.lines[0]);
        layout_near(frame, "portable status text", "lines", (float)l.status.count);
        layout_near(frame, "portable status text", "width", l.status.w);
        CHECK(l.status_dot.x + l.status_dot.w < l.status.lines[0].x && l.status_dot.y + l.status_dot.h == l.status.lines[0].baseline);
    } else CHECK(l.status.count == 0);
    if (layout == ORACLES_UI_LAYOUT_4_3) {
        layout_box(frame, "update", &l.rows[0]);
        layout_box(frame, "portable", &l.rows[1]);
        layout_near(frame, "panel", "y", l.rows[0].y);
        layout_near(frame, "panel", "h", l.help_note.y + l.help_note.h - l.rows[0].y);
        layout_near(frame, "help", "y", l.help.y);
        layout_line(frame, "help text", &l.help);
        layout_line(frame, "help note text", &l.help_note);
        return;
    }
    /* The panel as 1.4.0 drew it, to its rows: Settings' About block starts 20 under them (settings, 39q). */
    OraclesUiBox rows_panel = l.panel;
    rows_panel.h = l.about.rule.y - 20.0f + 24.0f + 1.0f - l.panel.y;
    layout_box(frame, "panel", &rows_panel);
    layout_box(frame, "update row", &l.rows[0]);
    layout_box(frame, "portable row", &l.rows[1]);
    layout_near(frame, "update", "y", l.rows[0].y);
    layout_near(frame, "update", "h", l.rows[0].h);
    layout_near(frame, "portable", "y", l.rows[1].y);
    layout_near(frame, "portable", "h", bottom - l.rows[1].y);
    layout_line(frame, "portable explanation", &l.explanations[1].lines[0]);
    layout_near(frame, "portable explanation", "lines", (float)l.explanations[1].count);
    CHECK(l.help.h == 0.0f);
    if (status != ORACLES_PORTABLE_STATUS_NONE || device != ORACLES_INPUT_GAMEPAD) return;
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    OraclesUiHintLayout h[ORACLES_HOME_MAX_HINTS];
    CHECK(oracles_home_hints(&nav, hints) == 4);
    oracles_ui_layout_hints(layout, hints, 4, h);
    for (unsigned i = 0; i < 4u; i++) {
        char name[32];
        snprintf(name, sizeof name, "hint %u", i); layout_box(frame, name, &h[i].box);
        snprintf(name, sizeof name, "hint %u label", i); layout_line(frame, name, &h[i].label);
    }
}

/* The game folder not writable on Linux, /opt for Program Files: one line where Windows takes two, in both layouts,
 * the About block moving up by that line, 31 px in 16:9 and 38 in 4:3 (turn 46 of the mockup). */
static void read_only_linux(void)
{
    for (int layout = ORACLES_UI_LAYOUT_16_9; layout <= ORACLES_UI_LAYOUT_4_3; layout++) {
        OraclesUiCheatsLayout l[2];
        for (int windows = 0; windows < 2; windows++) {
            OraclesHomeNav nav;
            oracles_home_init(&nav);
            nav.layout = (OraclesUiLayout)layout;
            nav.windows = windows;
            nav.portable_shown = 1;
            nav.portable_status = ORACLES_PORTABLE_STATUS_READ_ONLY;
            nav.screen = ORACLES_SCREEN_ABOUT;
            OraclesTone tone;
            CHECK(strstr(oracles_about_portable_status(&nav, &tone), windows ? "(such as Program Files)" : "(such as /opt)") != NULL);
            oracles_ui_layout_about((OraclesUiLayout)layout, &nav, "v0.1.0", "thibautbus/top", &l[windows]);
        }
        const float line = layout == ORACLES_UI_LAYOUT_16_9 ? 31.0f : 38.0f;
        CHECK(l[0].status.count == 1 && l[1].status.count == 2);
        CHECK(l[1].status.h - l[0].status.h == line && l[1].about.rule.y - l[0].about.rule.y == line);
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
    settings(ORACLES_UI_LAYOUT_16_9, "39q");
    settings(ORACLES_UI_LAYOUT_4_3, "39q 4:3");
    licences();
    portable(ORACLES_UI_LAYOUT_16_9, "portable on", 1, ORACLES_PORTABLE_STATUS_NONE, 2, ORACLES_INPUT_GAMEPAD);
    portable(ORACLES_UI_LAYOUT_16_9, "portable copying", 0, ORACLES_PORTABLE_STATUS_COPYING, 2, ORACLES_INPUT_GAMEPAD);
    portable(ORACLES_UI_LAYOUT_16_9, "portable off kept", 0, ORACLES_PORTABLE_STATUS_KEPT, 2, ORACLES_INPUT_GAMEPAD);
    portable(ORACLES_UI_LAYOUT_16_9, "portable read only", 0, ORACLES_PORTABLE_STATUS_READ_ONLY, 0, ORACLES_INPUT_GAMEPAD);
    portable(ORACLES_UI_LAYOUT_4_3, "portable 4:3", 1, ORACLES_PORTABLE_STATUS_COPIED, 2, ORACLES_INPUT_KEYBOARD);
    portable(ORACLES_UI_LAYOUT_4_3, "portable 4:3 kept", 0, ORACLES_PORTABLE_STATUS_KEPT, 2, ORACLES_INPUT_KEYBOARD);
    read_only_linux();
    if (layout_failures()) { fprintf(stderr, "%d failure(s)\n", layout_failures()); return 1; }
    printf("launcher cheats: the page matches the layout reference within %.1f px, and its keys and pointer do what they say\n", (double)LAYOUT_TOLERANCE);
    return 0;
}
