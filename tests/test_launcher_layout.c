/* The home screen's layout against the layout reference,
 * tests/launcher_layout_reference.json (layout_reference.h), the test's
 * argument, in its frames 1a (Ages), 1c (Fan games), 1d (Moonrise Regalia
 * picked in the list), 1f (Seasons, no ROM) and 8e (Moonrise Regalia's Mods,
 * greyed), and a toast; in the 4:3 layout, 10a (Ages), 10b (Fan games), 10c
 * (Seasons, no ROM), a toast, and the pause menu, 10n, and 10t in a 640x480
 * window; the toast after a game that ran slowly, 11c, 11f and 11h; the language beside Cartridge, 14f, 15d and
 * 16f; the reminder over a game started, 18d and 18e, and with each device and HUD, 37c to 37f; the pause menu's line on
 * the item hotkeys under its list, 37g and 37h; the help bar after a key or a
 * controller's button, at home (21a to 21d), in the pause menu (21e, 21f, its entries naming no key: 21y) and on
 * Display (21g, 21h); Cheats in the menus, 22g, 22k, 22q and 22r, and a fan game's menu without it, 23g; the newer
 * version's notice by the version, 24a to 24d, and the version alone, 24e; the version number highlighted, 32g and
 * 32g 4:3: a text is
 * compared by its left edge, its width, the top and height of its line box,
 * and its baseline; an element by its box.  And the choice of the layout by the
 * window's shape. */
#include "layout_reference.h"
#include "ui_controls_nav.h"
#include "ui_home_layout.h"
#include "ui_home_nav.h"
#include "ui_layout.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); layout_fail(); } } while (0)

/* The menu as the home screen lays it out: a disabled item shows its reason while highlighted, on a background; the
 * pause menu's line on the item hotkeys under it while it shows, into `line`. */
static unsigned menu_with(const OraclesHomeNav *nav, OraclesUiItemLayout *out, OraclesUiMenuLineLayout *line)
{
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    const char *notes[ORACLES_HOME_MAX_ITEMS], *labels[ORACLES_HOME_MAX_ITEMS];
    int reasons[ORACLES_HOME_MAX_ITEMS];
    const unsigned count = oracles_home_items(nav, items), focus = oracles_home_focus(nav);
    for (unsigned i = 0; i < count; i++) {
        reasons[i] = !items[i].note && i == focus && items[i].disabled;
        notes[i] = items[i].note ? items[i].note : reasons[i] ? items[i].reason : NULL;
        labels[i] = items[i].label;
    }
    oracles_ui_layout_menu(nav->layout, notes, reasons, labels, count, oracles_controls_hotkeys_line(nav) ? line : NULL, out);
    return count;
}

static unsigned menu(const OraclesHomeNav *nav, OraclesUiItemLayout *out)
{
    OraclesUiMenuLineLayout line;
    return menu_with(nav, out, &line);
}

static void frame_1a(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.games[0].usable = 1;
    snprintf(nav.games[0].last_session, sizeof nav.games[0].last_session, "21 Sep 2026");
    char state[ORACLES_HOME_STATE_LENGTH];
    oracles_home_state(&nav, ORACLES_HOME_HERO_AGES, state);
    OraclesUiTitleLayout hero;
    oracles_ui_layout_hero(ORACLES_UI_LAYOUT_16_9, 0, "Oracle of", "Ages", state, &hero);
    layout_line("1a", "hero over", &hero.over);
    layout_line("1a", "hero title", &hero.title);
    layout_line("1a", "hero state", &hero.state);
    layout_box("1a", "hero", &hero.box);

    const char *over[2] = { "Oracle of", "Community" }, *title[2] = { "Seasons", "Fan games" }, *states[2] = { "ROM not found", "3 games" };
    OraclesUiTitleLayout others[2];
    oracles_ui_layout_others(over, title, states, others);
    layout_line("1a", "seasons over", &others[0].over);
    layout_line("1a", "seasons title", &others[0].title);
    layout_line("1a", "seasons state", &others[0].state);
    layout_line("1a", "fan over", &others[1].over);
    layout_line("1a", "fan title", &others[1].title);
    layout_line("1a", "fan state", &others[1].state);

    OraclesUiItemLayout items[ORACLES_HOME_MAX_ITEMS];
    if (menu(&nav, items) != 7) { fprintf(stderr, "FAIL 1a: seven items expected\n"); layout_fail(); return; }
    for (int i = 0; i < 7; i++) {
        char name[32];
        snprintf(name, sizeof name, "item %d", i); layout_box("1a", name, &items[i].box);
        snprintf(name, sizeof name, "item %d label", i); layout_line("1a", name, &items[i].label);
        snprintf(name, sizeof name, "item %d dot", i); layout_line("1a", name, &items[i].dot);
    }
    /* Beside Display, the profile the game plays in. */
    layout_line("1a", "item 3 note", &items[3].note);

    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    OraclesUiHintLayout h[ORACLES_HOME_MAX_HINTS];
    if (oracles_home_hints(&nav, hints) != 3) { fprintf(stderr, "FAIL 1a: three hints expected\n"); layout_fail(); return; }
    oracles_ui_layout_hints(ORACLES_UI_LAYOUT_16_9, hints, 3, h);
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof name, "hint %d key", i); layout_box("1a", name, &h[i].key_box);
        snprintf(name, sizeof name, "hint %d key text", i); layout_line("1a", name, &h[i].key);
        snprintf(name, sizeof name, "hint %d label", i); layout_line("1a", name, &h[i].label);
        snprintf(name, sizeof name, "hint %d", i); layout_near("1a", name, "y", h[i].box.y);
    }
    OraclesUiLine version;
    oracles_ui_layout_version(ORACLES_UI_LAYOUT_16_9, "v1.0.0", &version);
    layout_line("1a", "version", &version);
}

/* The language beside Cartridge, as the profile beside Display: Ages playing in French, its European ROM recognized (14f),
 * in English (EU) (15d); Moonrise Regalia in French, the European ROM of Ages recognized, with the fan games' languages
 * on (16f).  English (US), or a European ROM not recognized, names none. */
static void frame_language(const char *frame, int game, int language, const char *name)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.fan_languages = 1;   /* the mockup's fanLang, for 16f */
    OraclesHomeGame *g = &nav.games[game];
    g->usable = 1;
    g->rom = g->eu = ORACLES_ROM_ORIGINAL;
    g->language = language;
    if (game != ORACLES_HOME_GAME_AGES) {
        g->fan = oracles_home_fan_game(game);
        g->patch = g->image = ORACLES_ROM_ORIGINAL;
        nav.entry = ORACLES_HOME_FAN;
        nav.fan_pick = ORACLES_HOME_HERO_MOONRISE;
    }
    nav.focus = 1;
    OraclesUiItemLayout items[ORACLES_HOME_MAX_ITEMS];
    const unsigned count = oracles_home_fan_game(game) ? 6u : 7u;   /* a fan game's menu has no Cheats */
    if (menu(&nav, items) != count) { fprintf(stderr, "FAIL %s: %u items expected\n", frame, count); layout_fail(); return; }
    for (int i = 0; i < (int)count; i++) {
        char probe[32];
        snprintf(probe, sizeof probe, "item %d", i);
        layout_box(frame, probe, &items[i].box);
    }
    layout_line(frame, "item 1 label", &items[1].label);
    layout_line(frame, "item 1 note", &items[1].note);
    layout_line(frame, "item 3 note", &items[3].note);
    OraclesHomeItem it[ORACLES_HOME_MAX_ITEMS];
    oracles_home_items(&nav, it);
    CHECK(it[1].note && !strcmp(it[1].note, name));
    g->eu = ORACLES_ROM_UNRECOGNISED;
    oracles_home_items(&nav, it);
    CHECK(it[1].note == NULL);
    g->eu = ORACLES_ROM_ORIGINAL;
    /* The fan games' languages off, as in the product: a fan game names none; an Oracle still does. */
    nav.fan_languages = 0;
    oracles_home_items(&nav, it);
    CHECK(g->fan ? it[1].note == NULL : it[1].note != NULL);
    g->language = 0;
    oracles_home_items(&nav, it);
    CHECK(it[1].note == NULL);
}

static void frame_1c(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.entry = ORACLES_HOME_FAN;
    /* Each fan game with its state beside it, as frame 1c holds them: ready, Moonrise Regalia played on 18 September,
     * Temple of Seasons and Gifts of Kinomi on the 27th. */
    nav.games[ORACLES_HOME_GAME_MOONRISE].usable = 1;
    snprintf(nav.games[ORACLES_HOME_GAME_MOONRISE].last_session, sizeof nav.games[0].last_session, "18 Sep 2026");
    nav.games[ORACLES_HOME_GAME_TEMPLE].usable = 1;
    snprintf(nav.games[ORACLES_HOME_GAME_TEMPLE].last_session, sizeof nav.games[0].last_session, "27 Sep 2026");
    nav.games[ORACLES_HOME_GAME_KINOMI].usable = 1;
    snprintf(nav.games[ORACLES_HOME_GAME_KINOMI].last_session, sizeof nav.games[0].last_session, "27 Sep 2026");
    char state[ORACLES_HOME_STATE_LENGTH];
    oracles_home_state(&nav, ORACLES_HOME_HERO_FAN, state);
    OraclesUiTitleLayout hero;
    oracles_ui_layout_hero(ORACLES_UI_LAYOUT_16_9, 0, oracles_home_over(ORACLES_HOME_HERO_FAN), oracles_home_title(ORACLES_HOME_HERO_FAN), state, &hero);
    layout_line("1c", "hero over", &hero.over);
    layout_line("1c", "hero title", &hero.title);
    layout_line("1c", "hero state", &hero.state);   /* "3 games" */
    OraclesUiItemLayout items[ORACLES_HOME_MAX_ITEMS];
    if (menu(&nav, items) != 4) { fprintf(stderr, "FAIL 1c: four items expected\n"); layout_fail(); return; }
    layout_box("1c", "kinomi", &items[0].box);
    layout_line("1c", "kinomi note", &items[0].note);
    layout_line("1c", "kinomi label", &items[0].label);
    layout_box("1c", "moonrise", &items[1].box);
    layout_line("1c", "moonrise note", &items[1].note);
    layout_line("1c", "moonrise label", &items[1].label);
    layout_box("1c", "temple", &items[2].box);
    layout_line("1c", "temple note", &items[2].note);
    layout_line("1c", "temple label", &items[2].label);
    if (items[0].note_box.w != 0.0f) { fprintf(stderr, "FAIL 1c: the note has a background, as a reason\n"); layout_fail(); }
    layout_box("1c", "exit", &items[3].box);
}

static void frame_1d(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.entry = ORACLES_HOME_FAN;
    nav.fan_pick = ORACLES_HOME_HERO_MOONRISE;
    OraclesHomeGame *g = &nav.games[ORACLES_HOME_GAME_MOONRISE];
    g->usable = 1;
    g->fan = oracles_home_fan_game(ORACLES_HOME_GAME_MOONRISE);
    g->rom = g->patch = g->image = ORACLES_ROM_ORIGINAL;
    snprintf(g->last_session, sizeof g->last_session, "18 Sep 2026");
    char state[ORACLES_HOME_STATE_LENGTH];
    oracles_home_state(&nav, ORACLES_HOME_HERO_MOONRISE, state);
    OraclesUiTitleLayout hero;
    oracles_ui_layout_hero(ORACLES_UI_LAYOUT_16_9, 0, oracles_home_over(ORACLES_HOME_HERO_MOONRISE), oracles_home_title(ORACLES_HOME_HERO_MOONRISE), state, &hero);
    layout_line("1d", "hero over", &hero.over);
    layout_line("1d", "hero title", &hero.title);
    layout_line("1d", "hero state", &hero.state);
    layout_box("1d", "hero", &hero.box);
    /* The menu of a game, its profile beside Display. */
    OraclesUiItemLayout items[ORACLES_HOME_MAX_ITEMS];
    if (menu(&nav, items) != 6) { fprintf(stderr, "FAIL 1d: six items expected\n"); layout_fail(); return; }
    for (int i = 0; i < 6; i++) {
        char name[32];
        snprintf(name, sizeof name, "item %d", i); layout_box("1d", name, &items[i].box);
        snprintf(name, sizeof name, "item %d label", i); layout_line("1d", name, &items[i].label);
    }
    layout_line("1d", "item 3 note", &items[3].note);
    /* The fourth hint goes back to the list. */
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    OraclesUiHintLayout h[ORACLES_HOME_MAX_HINTS];
    if (oracles_home_hints(&nav, hints) != 4) { fprintf(stderr, "FAIL 1d: four hints expected\n"); layout_fail(); return; }
    oracles_ui_layout_hints(ORACLES_UI_LAYOUT_16_9, hints, 4, h);
    layout_box("1d", "hint 3 key", &h[3].key_box);
    layout_line("1d", "hint 3 key text", &h[3].key);
    layout_line("1d", "hint 3 label", &h[3].label);
}

/* A fan game picked in the list, played on `date`: the same menu under its title (1m Temple of Seasons, 1p Gifts of
 * Kinomi). */
static void frame_fan_pick(const char *frame, int game, OraclesHomeHero hero_id, const char *date)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.entry = ORACLES_HOME_FAN;
    nav.fan_pick = hero_id;
    OraclesHomeGame *g = &nav.games[game];
    g->usable = 1;
    g->fan = oracles_home_fan_game(game);
    g->rom = g->patch = g->image = ORACLES_ROM_ORIGINAL;
    snprintf(g->last_session, sizeof g->last_session, "%s", date);
    char state[ORACLES_HOME_STATE_LENGTH];
    oracles_home_state(&nav, hero_id, state);
    OraclesUiTitleLayout hero;
    oracles_ui_layout_hero(ORACLES_UI_LAYOUT_16_9, 0, oracles_home_over(hero_id), oracles_home_title(hero_id), state, &hero);
    layout_line(frame, "hero over", &hero.over);
    layout_line(frame, "hero title", &hero.title);
    layout_line(frame, "hero state", &hero.state);
    layout_box(frame, "hero", &hero.box);
    OraclesUiItemLayout items[ORACLES_HOME_MAX_ITEMS];
    if (menu(&nav, items) != 6) { fprintf(stderr, "FAIL %s: six items expected\n", frame); layout_fail(); return; }
    for (int i = 0; i < 6; i++) {
        char name[32];
        snprintf(name, sizeof name, "item %d", i); layout_box(frame, name, &items[i].box);
        snprintf(name, sizeof name, "item %d label", i); layout_line(frame, name, &items[i].label);
    }
}

static void frame_1f(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.entry = ORACLES_HOME_SEASONS;
    char state[ORACLES_HOME_STATE_LENGTH];
    oracles_home_state(&nav, ORACLES_HOME_HERO_SEASONS, state);
    OraclesUiTitleLayout hero;
    oracles_ui_layout_hero(ORACLES_UI_LAYOUT_16_9, 0, "Oracle of", "Seasons", state, &hero);
    layout_line("1f", "hero title", &hero.title);
    layout_line("1f", "hero state", &hero.state);
    /* Start game highlighted and disabled, with its reason on the highlight's background, 10 px on each side and 2
     * above and below: the item grows past 720. */
    OraclesUiItemLayout items[ORACLES_HOME_MAX_ITEMS];
    menu(&nav, items);
    layout_box("1f", "start", &items[0].box);
    layout_line("1f", "start reason", &items[0].note);
    layout_box("1f", "start reason background", &items[0].note_box);
}

/* A fan game's menu, Mods highlighted: greyed, with its reason. */
static void frame_8e(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.entry = ORACLES_HOME_FAN;
    nav.fan_pick = ORACLES_HOME_HERO_MOONRISE;
    nav.focus = 4;
    OraclesUiItemLayout items[ORACLES_HOME_MAX_ITEMS];
    if (menu(&nav, items) != 6) { fprintf(stderr, "FAIL 8e: six items expected\n"); layout_fail(); return; }
    layout_box("8e", "item 4", &items[4].box);
    layout_line("8e", "item 4 label", &items[4].label);
    layout_line("8e", "item 4 reason", &items[4].note);
}

static void toast(void)
{
    OraclesUiToastLayout t;
    oracles_ui_layout_toast(ORACLES_UI_LAYOUT_16_9, "Refused: not an Oracle of Ages or Seasons ROM", &t);
    layout_box("toast", "toast", &t.box);
    layout_line("toast", "toast text", &t.text);
    oracles_ui_layout_toast(ORACLES_UI_LAYOUT_4_3, "Refused: not an Oracle of Ages or Seasons ROM", &t);
    layout_box("toast 4:3", "toast", &t.box);
    layout_line("toast 4:3", "toast text", &t.text);
}

/* The reminder over a game started, centred under the status bar: the far view's, 480x270 at 4x in 16:9 (its bar 64
 * high), 480x360 at 3x in 4:3 (48), and at 640x480 at 1x, 60 px down (the scene's 171).  With a controller (18d, 18e,
 * 37c, its HUD Original in 16:9 and Wide in 4:3, where the bar keeps its sixteen lines), the keyboard (37d), a finger on
 * Android (37e, clear of the pause button in the top right corner) and a controller there (37f). */
static void hints_of(const char *frame, const OraclesHomeNav *nav, OraclesUiLayout layout, unsigned expected);

static void reminder_frame(const char *frame, OraclesUiLayout layout, const char *text, float bar_bottom)
{
    OraclesUiToastLayout t;
    oracles_ui_layout_pad_toast(layout, text, bar_bottom, &t);
    layout_box(frame, "toast", &t.box);
    layout_line(frame, "toast text", &t.text);
    if (!strncmp(text, "The pause", 9))
        CHECK(t.box.x + t.box.w < layout_reference(frame, "pause button", "x") && t.box.y >= layout_reference(frame, "pause button", "y"));
}

static void reminders(void)
{
    char text[64];
    oracles_controls_menu_tip(ORACLES_INPUT_GAMEPAD, 0, "leftstick", text, sizeof text);
    reminder_frame("18d", ORACLES_UI_LAYOUT_16_9, text, 64.0f);
    reminder_frame("18e", ORACLES_UI_LAYOUT_4_3, text, 48.0f);
    reminder_frame("37c", ORACLES_UI_LAYOUT_16_9, text, 64.0f);
    reminder_frame("37c 4:3", ORACLES_UI_LAYOUT_4_3, text, 48.0f);
    oracles_controls_menu_tip(ORACLES_INPUT_KEYBOARD, 0, "leftstick", text, sizeof text);
    reminder_frame("37d", ORACLES_UI_LAYOUT_16_9, text, 64.0f);
    reminder_frame("37d 4:3", ORACLES_UI_LAYOUT_4_3, text, 48.0f);
    oracles_controls_menu_tip(ORACLES_INPUT_POINTER, 1, "leftstick", text, sizeof text);
    reminder_frame("37e", ORACLES_UI_LAYOUT_16_9, text, 64.0f);
    reminder_frame("37e 640x480", ORACLES_UI_LAYOUT_4_3, text, 171.0f);
    oracles_controls_menu_tip(ORACLES_INPUT_GAMEPAD, 1, "leftstick", text, sizeof text);
    reminder_frame("37f", ORACLES_UI_LAYOUT_4_3, text, 171.0f);
    reminder_frame("37f 16:9", ORACLES_UI_LAYOUT_16_9, text, 64.0f);
}

/* Ages' pause with its item hotkeys off, in 16:9 from a key (37g) and at 640x480 from a controller (37h): the line under
 * the list, which moves up by it.  Due where the pause has Controls, three openings at most; not for a fan game, nor a
 * ROM refusing the hotkeys; gone once they are on. */
static void hotkeys_line(const char *frame, int width, int height, OraclesInputDevice device)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.layout = oracles_ui_layout_choose(width, height, 0);
    oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, oracles_ui_layout_narrow(nav.layout, width));
    oracles_home_input(&nav, device);
    CHECK(!nav.games[ORACLES_HOME_GAME_AGES].hotkeys && oracles_controls_hotkeys_tip_due(&nav));
    nav.hotkeys_line = 1;
    OraclesUiItemLayout items[ORACLES_HOME_MAX_ITEMS];
    OraclesUiMenuLineLayout line;
    if (menu_with(&nav, items, &line) != 7) { fprintf(stderr, "FAIL %s: seven items expected\n", frame); layout_fail(); return; }
    for (int i = 0; i < 7; i++) {
        char name[32];
        snprintf(name, sizeof name, "item %d", i); layout_box(frame, name, &items[i].box);
        snprintf(name, sizeof name, "item %d label", i); layout_line(frame, name, &items[i].label);
    }
    layout_box(frame, "line", &line.box);
    layout_line(frame, "line text", &line.lead);
    layout_line(frame, "line path", &line.path);
    hints_of(frame, &nav, nav.layout, 3);
    nav.games[ORACLES_HOME_GAME_AGES].hotkeys = 1;
    CHECK(!oracles_controls_hotkeys_line(&nav) && !oracles_controls_hotkeys_tip_due(&nav));
}

static void hotkeys_line_rules(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    oracles_home_pause(&nav, ORACLES_HOME_GAME_SEASONS, ORACLES_PROFILE_FAITHFUL, 0);
    CHECK(oracles_controls_hotkeys_tip_due(&nav));
    nav.controls.hotkeys_tip = ORACLES_HOTKEYS_TIPS - 1;
    CHECK(oracles_controls_hotkeys_tip_due(&nav));
    nav.controls.hotkeys_tip = ORACLES_TIP_OFF;
    CHECK(!oracles_controls_hotkeys_tip_due(&nav));
    nav.controls.hotkeys_tip = 0;
    nav.hotkeys_refused = 1;   /* an unrecognised ROM */
    CHECK(!oracles_controls_hotkeys_tip_due(&nav));
    nav.hotkeys_refused = 0;
    oracles_home_pause(&nav, ORACLES_HOME_GAME_SEASONS, ORACLES_PROFILE_ENHANCED, 1);   /* a 16:9 window under 960 pixels */
    CHECK(!oracles_controls_hotkeys_tip_due(&nav));
    nav.hotkeys_line = 1;
    CHECK(!oracles_controls_hotkeys_line(&nav));
    oracles_home_pause(&nav, ORACLES_HOME_GAME_MOONRISE, ORACLES_PROFILE_ENHANCED, 0);
    CHECK(!oracles_controls_hotkeys_tip_due(&nav));
    nav.screen = ORACLES_SCREEN_HOME;
    nav.in_game = 0;
    oracles_home_select(&nav, ORACLES_HOME_AGES);
    CHECK(!oracles_controls_hotkeys_tip_due(&nav) && !oracles_controls_hotkeys_line(&nav));   /* the home screen's menu */
    /* Three pause menus opened by its key show it, a fourth not; the pause Android's background brings (`by_key` 0) shows
     * it without counting it, nor ends the reminder.  Opened by its key, Back on Android included, the menu is done with
     * the reminder. */
    oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, 0);
    nav.controls.hotkeys_tip = 0;
    nav.controls.menu_tip = 4;
    CHECK(!oracles_controls_tips_opened(&nav, 0) && nav.hotkeys_line && nav.controls.hotkeys_tip == 0 && nav.controls.menu_tip == 4);
    CHECK(oracles_controls_tips_opened(&nav, 1) && nav.controls.menu_tip == ORACLES_TIP_OFF && nav.controls.hotkeys_tip == 1);
    int shown = 1;
    for (int opening = 0; opening < 4; opening++) {
        const int changed = oracles_controls_tips_opened(&nav, 1);
        shown += nav.hotkeys_line;
        CHECK(changed == nav.hotkeys_line);
    }
    CHECK(shown == ORACLES_HOTKEYS_TIPS && nav.controls.hotkeys_tip == ORACLES_TIP_OFF);
    CHECK(!oracles_controls_tips_opened(&nav, 1) && !nav.hotkeys_line);
    /* Turning them on in Controls is done with the line. */
    nav.controls.hotkeys_tip = 0;
    oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, 0);
    oracles_controls_open(&nav);
    nav.controls.row = ORACLES_CONTROLS_ROW_MODE;
    CHECK(oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_MODE, 1) == ORACLES_HOME_STORE && nav.games[ORACLES_HOME_GAME_AGES].hotkeys == 1);
    CHECK(nav.controls.hotkeys_tip == ORACLES_TIP_OFF);
    /* In the same pause, on then Off again: the line stays gone. */
    nav.controls.hotkeys_tip = 0;
    oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, 0);
    nav.games[ORACLES_HOME_GAME_AGES].hotkeys = 0;
    oracles_controls_tips_opened(&nav, 1);
    CHECK(oracles_controls_hotkeys_line(&nav));
    oracles_controls_open(&nav);
    CHECK(oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_MODE, 1) == ORACLES_HOME_STORE);
    CHECK(oracles_controls_click(&nav, 2, ORACLES_CONTROLS_ROW_MODE, 0) == ORACLES_HOME_STORE && !nav.games[ORACLES_HOME_GAME_AGES].hotkeys);
    nav.screen = ORACLES_SCREEN_PAUSE;
    CHECK(!oracles_controls_hotkeys_line(&nav));
}

/* The toast after a game that ran slowly at High, at the bottom left (11c), in 4:3 on two lines (11f), and in a 640x480
 * window (11h), which takes 4:3's scene: Display's Quality suggests Medium, and opens on its row. */
static void slow_toast(const char *frame, int width, int height, unsigned lines)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.layout = oracles_ui_layout_choose(width, height, 0);
    nav.display.view = 2;
    nav.display.core = 1;
    char text[ORACLES_HOME_TEXT_LENGTH];
    const char *path = NULL;
    CHECK(oracles_home_slow_text(&nav, text, sizeof text, &path));
    CHECK(!strcmp(text, "The game ran slowly at High: Medium may play smoother.") && !strcmp(path, "Display \xe2\x80\xba Quality"));
    OraclesUiSlowLayout l;
    oracles_ui_layout_slow(nav.layout, text, path, &l);
    layout_box(frame, "toast", &l.box);
    layout_line(frame, "toast text", &l.text_lines[0]);
    layout_line(frame, "toast path", &l.path);
    CHECK(l.lines == lines && (unsigned)layout_reference(frame, "toast text", "lines") == lines);
    /* text-wrap: pretty: "smoother." alone on the last line takes "play" with it. */
    CHECK(lines == 1 || !strcmp(l.text[1], "play smoother."));
    float widest = 0.0f;
    for (unsigned i = 0; i < l.lines; i++) {
        if (l.text_lines[i].w > widest) widest = l.text_lines[i].w;
        CHECK(l.text_lines[i].x == l.text_lines[0].x && (i == 0 || l.text_lines[i].y == l.text_lines[i - 1].y + l.text_lines[i - 1].h));
    }
    layout_near(frame, "toast text", "width", widest);
}

/* The help bar of a frame: each hint's box, its key's box and text, its label. */
static void hints_of(const char *frame, const OraclesHomeNav *nav, OraclesUiLayout layout, unsigned expected)
{
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    OraclesUiHintLayout h[ORACLES_HOME_MAX_HINTS];
    const unsigned count = oracles_home_hints(nav, hints);
    if (count != expected) { fprintf(stderr, "FAIL %s: %u hints expected, %u given\n", frame, expected, count); layout_fail(); return; }
    oracles_ui_layout_hints(layout, hints, count, h);
    for (unsigned i = 0; i < count; i++) {
        char name[32];
        snprintf(name, sizeof name, "hint %u", i); layout_box(frame, name, &h[i].box);
        snprintf(name, sizeof name, "hint %u key", i); layout_box(frame, name, &h[i].key_box);
        snprintf(name, sizeof name, "hint %u key text", i); layout_line(frame, name, &h[i].key);
        snprintf(name, sizeof name, "hint %u label", i); layout_line(frame, name, &h[i].label);
    }
}

/* 4:3: the three games as tabs along the top, the chosen one's line along its bottom. */
static void tabs_4_3(const char *frame)
{
    static const char *const labels[ORACLES_HOME_ENTRIES] = { "Oracle of Ages", "Oracle of Seasons", "Fan games" };
    OraclesUiTabLayout tabs[ORACLES_HOME_ENTRIES];
    oracles_ui_layout_tabs(labels, tabs);
    for (int i = 0; i < ORACLES_HOME_ENTRIES; i++) {
        char name[32];
        snprintf(name, sizeof name, "tab %d", i); layout_box(frame, name, &tabs[i].box);
        snprintf(name, sizeof name, "tab %d label", i); layout_line(frame, name, &tabs[i].label);
        CHECK(tabs[i].line.x == tabs[i].box.x && tabs[i].line.w == tabs[i].box.w && tabs[i].line.h == 3.0f
              && tabs[i].line.y + tabs[i].line.h == tabs[i].box.y + tabs[i].box.h);
    }
}

static void frame_10a(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    nav.games[0].usable = 1;
    snprintf(nav.games[0].last_session, sizeof nav.games[0].last_session, "21 Sep 2026");
    tabs_4_3("10a");
    char state[ORACLES_HOME_STATE_LENGTH];
    oracles_home_state(&nav, ORACLES_HOME_HERO_AGES, state);
    OraclesUiTitleLayout hero;
    oracles_ui_layout_hero(ORACLES_UI_LAYOUT_4_3, 0, "Oracle of", "Ages", state, &hero);
    CHECK(hero.over.h == 0.0f);   /* the tab says "Oracle of Ages" */
    layout_line("10a", "hero title", &hero.title);
    layout_line("10a", "hero state", &hero.state);
    layout_box("10a", "hero", &hero.box);
    OraclesUiItemLayout items[ORACLES_HOME_MAX_ITEMS];
    if (menu(&nav, items) != 7) { fprintf(stderr, "FAIL 10a: seven items expected\n"); layout_fail(); return; }
    for (int i = 0; i < 7; i++) {
        char name[32];
        snprintf(name, sizeof name, "item %d", i); layout_box("10a", name, &items[i].box);
        snprintf(name, sizeof name, "item %d label", i); layout_line("10a", name, &items[i].label);
        snprintf(name, sizeof name, "item %d dot", i); layout_line("10a", name, &items[i].dot);
    }
    layout_line("10a", "item 3 note", &items[3].note);
    hints_of("10a", &nav, ORACLES_UI_LAYOUT_4_3, 3);
    OraclesUiLine version;
    oracles_ui_layout_version(ORACLES_UI_LAYOUT_4_3, "v1.0.0", &version);
    layout_line("10a", "version", &version);
}

static void frame_10b(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    nav.entry = ORACLES_HOME_FAN;
    nav.games[ORACLES_HOME_GAME_MOONRISE].usable = 1;
    snprintf(nav.games[ORACLES_HOME_GAME_MOONRISE].last_session, sizeof nav.games[0].last_session, "18 Sep 2026");
    nav.games[ORACLES_HOME_GAME_TEMPLE].usable = 1;
    snprintf(nav.games[ORACLES_HOME_GAME_TEMPLE].last_session, sizeof nav.games[0].last_session, "27 Sep 2026");
    nav.games[ORACLES_HOME_GAME_KINOMI].usable = 1;
    snprintf(nav.games[ORACLES_HOME_GAME_KINOMI].last_session, sizeof nav.games[0].last_session, "27 Sep 2026");
    tabs_4_3("10b");
    char state[ORACLES_HOME_STATE_LENGTH];
    oracles_home_state(&nav, ORACLES_HOME_HERO_FAN, state);
    OraclesUiTitleLayout hero;
    oracles_ui_layout_hero(ORACLES_UI_LAYOUT_4_3, 0, oracles_home_over(ORACLES_HOME_HERO_FAN), oracles_home_title(ORACLES_HOME_HERO_FAN), state, &hero);
    CHECK(hero.over.h == 0.0f);
    layout_line("10b", "hero title", &hero.title);
    layout_line("10b", "hero state", &hero.state);
    OraclesUiItemLayout items[ORACLES_HOME_MAX_ITEMS];
    if (menu(&nav, items) != 4) { fprintf(stderr, "FAIL 10b: four items expected\n"); layout_fail(); return; }
    static const char *const names[3] = { "kinomi", "moonrise", "temple" };
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof name, "%s", names[i]); layout_box("10b", name, &items[i].box);
        snprintf(name, sizeof name, "%s note", names[i]); layout_line("10b", name, &items[i].note);
        snprintf(name, sizeof name, "%s label", names[i]); layout_line("10b", name, &items[i].label);
    }
    layout_box("10b", "exit", &items[3].box);
}

static void frame_10c(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    nav.entry = ORACLES_HOME_SEASONS;
    char state[ORACLES_HOME_STATE_LENGTH];
    oracles_home_state(&nav, ORACLES_HOME_HERO_SEASONS, state);
    OraclesUiTitleLayout hero;
    oracles_ui_layout_hero(ORACLES_UI_LAYOUT_4_3, 0, "Oracle of", "Seasons", state, &hero);
    layout_line("10c", "hero title", &hero.title);
    layout_line("10c", "hero state", &hero.state);
    OraclesUiItemLayout items[ORACLES_HOME_MAX_ITEMS];
    menu(&nav, items);
    layout_box("10c", "start", &items[0].box);
    layout_line("10c", "start reason", &items[0].note);
    layout_box("10c", "start reason background", &items[0].note_box);
}

/* The pause menu in 4:3 (10n), and in a 640x480 window (10t): no narrow menu, Controls, Display and Cheats stay. */
static void pause_4_3(const char *frame, int width, int height)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.layout = oracles_ui_layout_choose(width, height, 0);
    CHECK(nav.layout == ORACLES_UI_LAYOUT_4_3);
    oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, oracles_ui_layout_narrow(nav.layout, width));
    OraclesUiItemLayout items[ORACLES_HOME_MAX_ITEMS];
    if (menu(&nav, items) != 7) { fprintf(stderr, "FAIL %s: seven items expected\n", frame); layout_fail(); return; }
    for (int i = 0; i < 7; i++) {
        char name[32];
        snprintf(name, sizeof name, "item %d", i); layout_box(frame, name, &items[i].box);
        snprintf(name, sizeof name, "item %d label", i); layout_line(frame, name, &items[i].label);
    }
    if (width < 1440) return;
    char state[ORACLES_HOME_STATE_LENGTH];
    oracles_home_state(&nav, ORACLES_HOME_HERO_AGES, state);
    OraclesUiTitleLayout hero;
    oracles_ui_layout_hero(nav.layout, 1, "Oracle of", "Ages", state, &hero);
    CHECK(hero.over.h == 0.0f);
    layout_line(frame, "hero title", &hero.title);
    layout_line(frame, "hero state", &hero.state);
    layout_box(frame, "hero", &hero.box);
    hints_of(frame, &nav, nav.layout, 3);
    OraclesUiLine version;
    oracles_ui_layout_version(nav.layout, "v1.0.0", &version);
    layout_line(frame, "version", &version);
}

/* The device used last (turn 21): the hints name the keyboard's keys alone after a key, the controller's alone after a
 * button or a stick, both after a click or a touch, each key's frame as wide as its widest label; on the home screen
 * (21a to 21d), in the pause menu (21e, 21f) and on a page (Display, 21g, 21h). */
static void devices(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    /* Before any input, both devices' keys, each frame holding the controller's: "D-pad ←→" is 8 characters. */
    CHECK(nav.device == ORACLES_INPUT_POINTER && !nav.keyboard_used);
    CHECK(oracles_home_hints(&nav, hints) == 3 && !strcmp(hints[2].key, "Enter / A") && hints[0].chars == 8 && hints[2].chars == 9);
    CHECK(oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD) && nav.keyboard_used && !oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD));
    CHECK(oracles_home_hints(&nav, hints) == 3 && !strcmp(hints[0].key, "\xe2\x86\x90\xe2\x86\x92") && !strcmp(hints[2].key, "Enter") && hints[0].chars == 8);
    hints_of("21a", &nav, ORACLES_UI_LAYOUT_16_9, 3);
    hints_of("21c", &nav, ORACLES_UI_LAYOUT_4_3, 3);
    /* A controller's input: its keys alone; the key pressed before still counts. */
    CHECK(oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD) && nav.keyboard_used);
    CHECK(oracles_home_hints(&nav, hints) == 3 && !strcmp(hints[0].key, "D-pad \xe2\x86\x90\xe2\x86\x92") && !strcmp(hints[2].key, "A"));
    hints_of("21b", &nav, ORACLES_UI_LAYOUT_16_9, 3);
    oracles_home_select(&nav, ORACLES_HOME_FAN);
    nav.fan_pick = ORACLES_HOME_HERO_MOONRISE;
    hints_of("21d", &nav, ORACLES_UI_LAYOUT_4_3, 4);
    /* A click brings both back. */
    CHECK(oracles_home_input(&nav, ORACLES_INPUT_POINTER) && oracles_home_hints(&nav, hints) == 4 && !strcmp(hints[3].key, "Esc / B"));

    /* The pause menu, from a controller (21e) and from a key (21f). */
    oracles_home_init(&nav);
    oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, 0);
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    hints_of("21e", &nav, ORACLES_UI_LAYOUT_16_9, 3);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    hints_of("21f", &nav, ORACLES_UI_LAYOUT_4_3, 3);
    /* Its entries name no key: Save state without a note, Load state with its time alone (21y). */
    snprintf(nav.load_note, sizeof nav.load_note, "from 22:14");
    OraclesUiItemLayout items[ORACLES_HOME_MAX_ITEMS];
    nav.layout = ORACLES_UI_LAYOUT_16_9;
    if (menu(&nav, items) == 7) {
        for (int i = 0; i < 7; i++) {
            char name[32];
            snprintf(name, sizeof name, "item %d", i); layout_box("21y", name, &items[i].box);
            snprintf(name, sizeof name, "item %d label", i); layout_line("21y", name, &items[i].label);
        }
        CHECK(items[1].note.w == 0.0f);
        layout_line("21y", "load note", &items[2].note);
    } else CHECK(0);

    /* Display, its first row: four hints, the D-pad's two. */
    oracles_home_init(&nav);
    nav.screen = ORACLES_SCREEN_DISPLAY;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    hints_of("21g", &nav, ORACLES_UI_LAYOUT_16_9, 4);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    hints_of("21h", &nav, ORACLES_UI_LAYOUT_4_3, 4);
    /* 23a: the same in 4:3 on Android with a controller. */
    nav.android = 1;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    hints_of("23a", &nav, ORACLES_UI_LAYOUT_4_3, 4);
}

/* The menu's items of `frame`: their boxes and labels, and their dots when `dots`. */
static void items_of(const char *frame, const OraclesHomeNav *nav, int dots)
{
    OraclesUiItemLayout items[ORACLES_HOME_MAX_ITEMS];
    if (menu(nav, items) != 7) { fprintf(stderr, "FAIL %s: seven items expected\n", frame); layout_fail(); return; }
    for (int i = 0; i < 7; i++) {
        char name[32];
        snprintf(name, sizeof name, "item %d", i); layout_box(frame, name, &items[i].box);
        snprintf(name, sizeof name, "item %d label", i); layout_line(frame, name, &items[i].label);
        if (dots) { snprintf(name, sizeof name, "item %d dot", i); layout_line(frame, name, &items[i].dot); }
    }
}

/* Cheats, the seventh entry: an Oracle's menu after Mods, highlighted, after a key (22g), and in 4:3, where the title
 * has no words above it so that the seven fit (22q); the pause's after Display (22k, 22r); none in a fan game's menu,
 * which keeps its six entries (23g), nor in its pause. */
static void cheats_entry(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.games[0].usable = 1;
    snprintf(nav.games[0].last_session, sizeof nav.games[0].last_session, "21 Sep 2026");
    nav.focus = 5;
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    OraclesHomeItem it[ORACLES_HOME_MAX_ITEMS];
    CHECK(oracles_home_items(&nav, it) == 7 && it[5].id == ORACLES_HOME_ITEM_CHEATS && !strcmp(it[5].label, "Cheats") && !it[5].disabled
          && it[6].id == ORACLES_HOME_ITEM_EXIT);
    char state[ORACLES_HOME_STATE_LENGTH];
    oracles_home_state(&nav, ORACLES_HOME_HERO_AGES, state);
    OraclesUiTitleLayout hero;
    oracles_ui_layout_hero(ORACLES_UI_LAYOUT_16_9, 0, "Oracle of", "Ages", state, &hero);
    layout_line("22g", "hero title", &hero.title);
    layout_line("22g", "hero state", &hero.state);
    layout_box("22g", "hero", &hero.box);
    items_of("22g", &nav, 1);
    hints_of("22g", &nav, ORACLES_UI_LAYOUT_16_9, 3);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    tabs_4_3("22q");
    oracles_ui_layout_hero(ORACLES_UI_LAYOUT_4_3, 0, "Oracle of", "Ages", state, &hero);
    CHECK(hero.over.h == 0.0f);
    layout_line("22q", "hero title", &hero.title);
    layout_line("22q", "hero state", &hero.state);
    layout_box("22q", "hero", &hero.box);
    items_of("22q", &nav, 0);

    oracles_home_init(&nav);
    oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, 0);
    nav.focus = 5;
    CHECK(oracles_home_items(&nav, it) == 7 && it[4].id == ORACLES_HOME_ITEM_DISPLAY && it[5].id == ORACLES_HOME_ITEM_CHEATS && !it[5].disabled);
    oracles_home_state(&nav, ORACLES_HOME_HERO_AGES, state);
    oracles_ui_layout_hero(ORACLES_UI_LAYOUT_16_9, 1, "Oracle of", "Ages", state, &hero);
    layout_line("22k", "hero over", &hero.over);
    layout_line("22k", "hero title", &hero.title);
    layout_line("22k", "hero state", &hero.state);
    items_of("22k", &nav, 0);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    oracles_ui_layout_hero(ORACLES_UI_LAYOUT_4_3, 1, "Oracle of", "Ages", state, &hero);
    CHECK(hero.over.h == 0.0f);
    layout_line("22r", "hero title", &hero.title);
    layout_line("22r", "hero state", &hero.state);
    layout_box("22r", "hero", &hero.box);
    items_of("22r", &nav, 0);
    /* Narrow, the settings stay in the launcher, Cheats with them. */
    oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, 1);
    CHECK(oracles_home_items(&nav, it) == 4);

    oracles_home_init(&nav);
    nav.entry = ORACLES_HOME_FAN;
    nav.fan_pick = ORACLES_HOME_HERO_MOONRISE;
    nav.focus = 5;
    OraclesUiItemLayout items[ORACLES_HOME_MAX_ITEMS];
    if (menu(&nav, items) != 6) { fprintf(stderr, "FAIL 23g: six items expected\n"); layout_fail(); return; }
    for (int i = 0; i < 6; i++) {
        char name[32];
        snprintf(name, sizeof name, "item %d", i); layout_box("23g", name, &items[i].box);
        snprintf(name, sizeof name, "item %d label", i); layout_line("23g", name, &items[i].label);
    }
    CHECK(oracles_home_items(&nav, it) == 6 && it[4].id == ORACLES_HOME_ITEM_MODS && it[5].id == ORACLES_HOME_ITEM_EXIT);
    /* Nor has a fan game's pause: its profile has no cheats. */
    oracles_home_pause(&nav, ORACLES_HOME_GAME_MOONRISE, ORACLES_PROFILE_ENHANCED, 0);
    CHECK(oracles_home_items(&nav, it) == 6 && it[4].id == ORACLES_HOME_ITEM_DISPLAY && it[5].id == ORACLES_HOME_ITEM_QUIT_GAME);
}

/* The newer version's notice (turn 24), GitHub's 1.3.0 read by 1.2.0, on the home screen: after the version on its line
 * in 16:9 (24a, Start game highlighted after a key; 24b, the notice highlighted, down from Exit, after a controller's
 * button, Select's hint saying Open), on its own line above it in 4:3 (24c) and at 640x480 on Android (24d); without a
 * newer version, the version alone (24e).  The menu and the help bar keep their places.  Turn 24's prototype puts the
 * version in a padded box, `lift` pixels lower than turn 23 drew it (16:9: 2, and 3 without the notice, whose frame makes
 * the line 2 px taller; 4:3: 5): the launcher keeps the version where 1a and 10a measure it, and lays the notice out from
 * it as the prototype does, its box and its text `lift` higher than the frame's. */
static void update_frame(const char *frame, OraclesUiLayout layout, OraclesInputDevice device, int android, int focused, const char *release, float lift)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.layout = layout;
    nav.android = android;
    nav.games[0].usable = 1;
    if (release) snprintf(nav.update, sizeof nav.update, "%s", release);
    oracles_home_input(&nav, device);
    if (focused) {
        nav.focus = 6;
        oracles_home_act(&nav, ORACLES_HOME_DOWN);
    }
    CHECK(oracles_home_focus(&nav) == (focused ? 7u : 0u) && oracles_home_update_focused(&nav) == focused);
    OraclesUiItemLayout items[ORACLES_HOME_MAX_ITEMS];
    if (menu(&nav, items) != 7) { fprintf(stderr, "FAIL %s: seven items expected\n", frame); layout_fail(); return; }
    for (int i = 0; i < 7; i++) {
        char name[32];
        snprintf(name, sizeof name, "item %d", i);
        layout_box(frame, name, &items[i].box);
    }
    hints_of(frame, &nav, layout, 3);
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    oracles_home_hints(&nav, hints);
    CHECK(!strcmp(hints[2].label, focused ? "Open" : "Select"));
    OraclesUiLine version;
    oracles_ui_layout_version(layout, "v1.2.0 (abc1234)", &version);
    layout_near(frame, "version", "x", version.x);
    layout_near(frame, "version", "w", version.w);
    layout_near(frame, "version", "baseline", version.baseline + lift);
    CHECK(oracles_home_update_shown(&nav) == (release != NULL));
    if (!release) return;
    OraclesUiUpdateLayout update;
    oracles_ui_layout_update(layout, &version, release, &update);
    OraclesUiBox box = update.box;
    box.y += lift;
    layout_box(frame, "update", &box);
    OraclesUiLine text = update.text;
    text.y += lift;
    text.baseline += lift;
    layout_line(frame, "update text", &text);
}

/* The version number highlighted (turn 32), down from the notice: after a key in 16:9 (32g), at 640x480 on Android
 * with a controller (32g 4:3); its outline's box drawn inside its padding, `lift` higher than the prototype's as the
 * notice's (update_frame), the number in its place, Select's hint saying Open. */
static void version_frame(const char *frame, OraclesUiLayout layout, OraclesInputDevice device, int android, float lift)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.layout = layout;
    nav.android = android;
    nav.games[0].usable = 1;
    snprintf(nav.update, sizeof nav.update, "1.3.0");
    oracles_home_input(&nav, device);
    nav.focus = 6;
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(oracles_home_focus(&nav) == 8 && oracles_home_version_focused(&nav) && !oracles_home_update_focused(&nav));
    OraclesUiItemLayout items[ORACLES_HOME_MAX_ITEMS];
    if (menu(&nav, items) != 7) { fprintf(stderr, "FAIL %s: seven items expected\n", frame); layout_fail(); return; }
    for (int i = 0; i < 7; i++) {
        char name[32];
        snprintf(name, sizeof name, "item %d", i);
        layout_box(frame, name, &items[i].box);
    }
    hints_of(frame, &nav, layout, 3);
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    oracles_home_hints(&nav, hints);
    CHECK(!strcmp(hints[2].label, "Open"));
    OraclesUiLine version;
    oracles_ui_layout_version(layout, "v1.2.0 (abc1234)", &version);
    layout_near(frame, "version", "x", version.x);
    layout_near(frame, "version", "baseline", version.baseline + lift);
    OraclesUiUpdateLayout update;
    oracles_ui_layout_update(layout, &version, "1.3.0", &update);
    update.box.y += lift;
    layout_box(frame, "update", &update.box);
    OraclesUiBox link;
    float outline;
    oracles_ui_layout_version_link(layout, &version, &link, &outline);
    link.y += lift;
    layout_box(frame, "version link", &link);
    CHECK(outline == (layout == ORACLES_UI_LAYOUT_4_3 ? 2.0f : 1.0f));
}

static void update_notice(void)
{
    update_frame("24a", ORACLES_UI_LAYOUT_16_9, ORACLES_INPUT_KEYBOARD, 0, 0, "1.3.0", 2.0f);
    update_frame("24b", ORACLES_UI_LAYOUT_16_9, ORACLES_INPUT_GAMEPAD, 0, 1, "1.3.0", 2.0f);
    update_frame("24c", ORACLES_UI_LAYOUT_4_3, ORACLES_INPUT_KEYBOARD, 0, 0, "1.3.0", 5.0f);
    update_frame("24d", ORACLES_UI_LAYOUT_4_3, ORACLES_INPUT_GAMEPAD, 1, 1, "1.3.0", 5.0f);
    update_frame("24e", ORACLES_UI_LAYOUT_16_9, ORACLES_INPUT_KEYBOARD, 0, 0, NULL, 3.0f);
    version_frame("32g", ORACLES_UI_LAYOUT_16_9, ORACLES_INPUT_KEYBOARD, 0, 2.0f);
    version_frame("32g 4:3", ORACLES_UI_LAYOUT_4_3, ORACLES_INPUT_GAMEPAD, 1, 5.0f);
    /* The version stays where it was: 24e's is 1a's, but for its text. */
    OraclesUiLine before, after;
    oracles_ui_layout_version(ORACLES_UI_LAYOUT_16_9, "v1.0.0", &before);
    oracles_ui_layout_version(ORACLES_UI_LAYOUT_16_9, "v1.2.0 (abc1234)", &after);
    CHECK(before.x == after.x && before.y == after.y && before.h == after.h && before.baseline == after.baseline);
}

/* The layout of a window's shape: the nearer of 4:3 and 16:9 by their ratio, unless aspect= names one; the narrow
 * pause menu in 16:9 only, and the pause in 4:3 from 640x480 only. */
static void layout_choice(void)
{
    CHECK(oracles_ui_layout_choose(1920, 1080, 0) == ORACLES_UI_LAYOUT_16_9);
    CHECK(oracles_ui_layout_choose(1280, 720, 0) == ORACLES_UI_LAYOUT_16_9);
    CHECK(oracles_ui_layout_choose(1280, 800, 0) == ORACLES_UI_LAYOUT_16_9);   /* 16:10 */
    CHECK(oracles_ui_layout_choose(1440, 1080, 0) == ORACLES_UI_LAYOUT_4_3);
    CHECK(oracles_ui_layout_choose(640, 480, 0) == ORACLES_UI_LAYOUT_4_3);
    CHECK(oracles_ui_layout_choose(1024, 768, 0) == ORACLES_UI_LAYOUT_4_3);
    CHECK(oracles_ui_layout_choose(720, 1280, 0) == ORACLES_UI_LAYOUT_4_3);   /* portrait */
    /* The middle, sqrt(4/3 x 16/9) = 1.5396. */
    CHECK(oracles_ui_layout_choose(1539, 1000, 0) == ORACLES_UI_LAYOUT_4_3);
    CHECK(oracles_ui_layout_choose(1540, 1000, 0) == ORACLES_UI_LAYOUT_16_9);
    CHECK(oracles_ui_layout_choose(640, 480, 1) == ORACLES_UI_LAYOUT_16_9);
    CHECK(oracles_ui_layout_choose(1920, 1080, 2) == ORACLES_UI_LAYOUT_4_3);
    CHECK(oracles_ui_layout_choose(0, 0, 0) == ORACLES_UI_LAYOUT_16_9);
    CHECK(oracles_ui_scene_width(ORACLES_UI_LAYOUT_16_9) == 1920.0f && oracles_ui_scene_width(ORACLES_UI_LAYOUT_4_3) == 1440.0f);
    CHECK(oracles_ui_layout_narrow(ORACLES_UI_LAYOUT_16_9, 959) && !oracles_ui_layout_narrow(ORACLES_UI_LAYOUT_16_9, 960));
    CHECK(!oracles_ui_layout_narrow(ORACLES_UI_LAYOUT_4_3, 640));
    /* The pause: 4:3 from about 640x480 only; Faithful 2x and 3x and Near 4:3 at 2x take the narrow 16:9 menu. */
    CHECK(oracles_ui_layout_pause(640, 480, 0) == ORACLES_UI_LAYOUT_4_3);
    CHECK(oracles_ui_layout_pause(640, 576, 0) == ORACLES_UI_LAYOUT_4_3);   /* Faithful 4x */
    CHECK(oracles_ui_layout_pause(480, 432, 0) == ORACLES_UI_LAYOUT_16_9 && oracles_ui_layout_narrow(ORACLES_UI_LAYOUT_16_9, 480));
    CHECK(oracles_ui_layout_pause(320, 288, 0) == ORACLES_UI_LAYOUT_16_9);
    CHECK(oracles_ui_layout_pause(426, 320, 0) == ORACLES_UI_LAYOUT_16_9);
    CHECK(oracles_ui_layout_pause(639, 480, 0) == ORACLES_UI_LAYOUT_4_3);   /* Near 4:3 at 3x */
    CHECK(oracles_ui_layout_pause(480, 432, 2) == ORACLES_UI_LAYOUT_16_9);   /* aspect=4:3 too */
    CHECK(oracles_ui_layout_pause(1920, 1080, 0) == ORACLES_UI_LAYOUT_16_9 && oracles_ui_layout_pause(1440, 1080, 0) == ORACLES_UI_LAYOUT_4_3);
}

int main(int argc, char **argv)
{
    if (argc < 2 || !layout_reference_load(argv[1])) { fprintf(stderr, "usage: oracles-test-launcher-layout launcher_layout_reference.json\n"); return 2; }
    if (!oracles_ui_fonts_load()) { fprintf(stderr, "FAIL the embedded fonts do not load\n"); return 1; }
    frame_1a();
    frame_language("14f", ORACLES_HOME_GAME_AGES, 2, "Fran\xc3\xa7" "ais");
    frame_language("15d", ORACLES_HOME_GAME_AGES, 1, "English (EU)");
    frame_language("16f", ORACLES_HOME_GAME_MOONRISE, 2, "Fran\xc3\xa7" "ais");
    frame_1c();
    frame_1d();
    frame_fan_pick("1m", ORACLES_HOME_GAME_TEMPLE, ORACLES_HOME_HERO_TEMPLE, "27 Sep 2026");
    frame_fan_pick("1p", ORACLES_HOME_GAME_KINOMI, ORACLES_HOME_HERO_KINOMI, "27 Sep 2026");
    frame_1f();
    frame_8e();
    toast();
    reminders();
    hotkeys_line("37g", 1920, 1080, ORACLES_INPUT_KEYBOARD);
    hotkeys_line("37h", 640, 480, ORACLES_INPUT_GAMEPAD);
    hotkeys_line_rules();
    slow_toast("11c", 1920, 1080, 1);
    frame_10a();
    frame_10b();
    frame_10c();
    pause_4_3("10n", 1440, 1080);
    pause_4_3("10t", 640, 480);
    slow_toast("11f", 1440, 1080, 2);
    slow_toast("11h", 640, 480, 2);
    devices();
    cheats_entry();
    update_notice();
    layout_choice();
    if (layout_failures()) { fprintf(stderr, "%d failure(s)\n", layout_failures()); return 1; }
    printf("launcher layout: the home screen matches the layout reference within %.1f px\n", (double)LAYOUT_TOLERANCE);
    return 0;
}
