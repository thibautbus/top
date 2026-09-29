/* The home screen's layout against the layout reference,
 * tests/launcher_layout_reference.json (layout_reference.h), the test's
 * argument, in its frames 1a (Ages), 1c (Fan games), 1d (Moonrise Regalia
 * picked in the list), 1f (Seasons, no ROM) and 8e (Moonrise Regalia's Mods,
 * greyed), and a toast: a text is compared by its left edge, its width, the top
 * and height of its line box, and its baseline; an element by its box. */
#include "layout_reference.h"
#include "ui_home_layout.h"
#include "ui_home_nav.h"

#include <stdio.h>
#include <string.h>

/* The menu as the home screen lays it out: a disabled item shows its reason while highlighted, on a background. */
static unsigned menu(const OraclesHomeNav *nav, OraclesUiItemLayout *out)
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
    oracles_ui_layout_menu(notes, reasons, labels, count, out);
    return count;
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
    oracles_ui_layout_hero("Oracle of", "Ages", state, &hero);
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
    if (menu(&nav, items) != 6) { fprintf(stderr, "FAIL 1a: six items expected\n"); layout_fail(); return; }
    for (int i = 0; i < 6; i++) {
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
    oracles_ui_layout_hints(hints, 3, h);
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof name, "hint %d key", i); layout_box("1a", name, &h[i].key_box);
        snprintf(name, sizeof name, "hint %d key text", i); layout_line("1a", name, &h[i].key);
        snprintf(name, sizeof name, "hint %d label", i); layout_line("1a", name, &h[i].label);
        snprintf(name, sizeof name, "hint %d", i); layout_near("1a", name, "y", h[i].box.y);
    }
    OraclesUiLine version;
    oracles_ui_layout_version("v1.0.0", &version);
    layout_line("1a", "version", &version);
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
    oracles_ui_layout_hero(oracles_home_over(ORACLES_HOME_HERO_FAN), oracles_home_title(ORACLES_HOME_HERO_FAN), state, &hero);
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
    oracles_ui_layout_hero(oracles_home_over(ORACLES_HOME_HERO_MOONRISE), oracles_home_title(ORACLES_HOME_HERO_MOONRISE), state, &hero);
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
    oracles_ui_layout_hints(hints, 4, h);
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
    oracles_ui_layout_hero(oracles_home_over(hero_id), oracles_home_title(hero_id), state, &hero);
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
    oracles_ui_layout_hero("Oracle of", "Seasons", state, &hero);
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
    oracles_ui_layout_toast("Refused: not an Oracle of Ages or Seasons ROM", &t);
    layout_box("toast", "toast", &t.box);
    layout_line("toast", "toast text", &t.text);
}

int main(int argc, char **argv)
{
    if (argc < 2 || !layout_reference_load(argv[1])) { fprintf(stderr, "usage: oracles-test-launcher-layout launcher_layout_reference.json\n"); return 2; }
    if (!oracles_ui_fonts_load()) { fprintf(stderr, "FAIL the embedded fonts do not load\n"); return 1; }
    frame_1a();
    frame_1c();
    frame_1d();
    frame_fan_pick("1m", ORACLES_HOME_GAME_TEMPLE, ORACLES_HOME_HERO_TEMPLE, "27 Sep 2026");
    frame_fan_pick("1p", ORACLES_HOME_GAME_KINOMI, ORACLES_HOME_HERO_KINOMI, "27 Sep 2026");
    frame_1f();
    frame_8e();
    toast();
    if (layout_failures()) { fprintf(stderr, "%d failure(s)\n", layout_failures()); return 1; }
    printf("launcher layout: the home screen matches the layout reference within %.1f px\n", (double)LAYOUT_TOLERANCE);
    return 0;
}
