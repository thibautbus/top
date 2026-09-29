/* The home screen's navigation (ui_home_nav.h): entries, the list of fan
 * games, disabled items and their reasons, going back, the pointer. */
#include "ui_home_nav.h"
#include "ui_page_nav.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); failures++; } } while (0)

static void entries(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    CHECK(nav.entry == ORACLES_HOME_AGES && oracles_home_hero(&nav) == ORACLES_HOME_HERO_AGES && oracles_home_focus(&nav) == 0);
    OraclesHomeEntry others[2];
    oracles_home_others(&nav, others);
    CHECK(others[0] == ORACLES_HOME_SEASONS && others[1] == ORACLES_HOME_FAN);
    /* Right and left go round the three entries; a change highlights the first item again. */
    nav.focus = 3;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.entry == ORACLES_HOME_SEASONS && nav.focus == 0);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(nav.entry == ORACLES_HOME_FAN);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(nav.entry == ORACLES_HOME_AGES);
    oracles_home_act(&nav, ORACLES_HOME_LEFT);
    CHECK(nav.entry == ORACLES_HOME_FAN);
    oracles_home_others(&nav, others);
    CHECK(others[0] == ORACLES_HOME_AGES && others[1] == ORACLES_HOME_SEASONS);
    /* A click on an entry of the left stack chooses it. */
    oracles_home_select(&nav, ORACLES_HOME_SEASONS);
    CHECK(nav.entry == ORACLES_HOME_SEASONS && nav.focus == 0);
}

static void states(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    char state[ORACLES_HOME_STATE_LENGTH];
    oracles_home_state(&nav, ORACLES_HOME_HERO_AGES, state);
    CHECK(!strcmp(state, "ROM not found"));
    nav.games[0].usable = 1;
    oracles_home_state(&nav, ORACLES_HOME_HERO_AGES, state);
    CHECK(!strcmp(state, "Ready"));
    snprintf(nav.games[0].last_session, sizeof nav.games[0].last_session, "21 Sep 2026");
    oracles_home_state(&nav, ORACLES_HOME_HERO_AGES, state);
    CHECK(!strcmp(state, "Ready \xc2\xb7 last session 21 Sep 2026"));
    oracles_home_state(&nav, ORACLES_HOME_HERO_SEASONS, state);
    CHECK(!strcmp(state, "ROM not found"));
    oracles_home_state(&nav, ORACLES_HOME_HERO_FAN, state);
    CHECK(!strcmp(state, "3 games"));
    /* Moonrise Regalia's, as a game's: its image built from the Ages ROM and its patch, and its save. */
    oracles_home_state(&nav, ORACLES_HOME_HERO_MOONRISE, state);
    CHECK(!strcmp(state, "ROM not found"));
    nav.games[ORACLES_HOME_GAME_MOONRISE].usable = 1;
    snprintf(nav.games[ORACLES_HOME_GAME_MOONRISE].last_session, sizeof nav.games[0].last_session, "18 Sep 2026");
    oracles_home_state(&nav, ORACLES_HOME_HERO_MOONRISE, state);
    CHECK(!strcmp(state, "Ready \xc2\xb7 last session 18 Sep 2026"));
    /* Temple of Seasons' the same, from the Seasons ROM and its patch. */
    oracles_home_state(&nav, ORACLES_HOME_HERO_TEMPLE, state);
    CHECK(!strcmp(state, "ROM not found"));
    /* The one table of the fan games: each at its place, its key, its start command. */
    for (unsigned f = 0; f < ORACLES_HOME_FAN_GAMES; f++)
        CHECK(oracles_home_fan_game(oracles_home_fan_games[f].game) == &oracles_home_fan_games[f]
              && oracles_home_fan_game_started(oracles_home_fan_games[f].start) == &oracles_home_fan_games[f]);
    CHECK(!strcmp(oracles_home_fan_games[ORACLES_HOME_FAN_MOONRISE].key, "moonrise") && !strcmp(oracles_home_fan_games[ORACLES_HOME_FAN_TEMPLE].key, "temple")
          && !strcmp(oracles_home_fan_games[ORACLES_HOME_FAN_KINOMI].key, "kinomi"));
    CHECK(!oracles_home_fan_game_started(ORACLES_HOME_START_AGES) && !oracles_home_fan_game_started(ORACLES_HOME_EXIT));
    CHECK(oracles_home_fan_game(ORACLES_HOME_GAME_TEMPLE)->base == ORACLES_HOME_GAME_SEASONS
          && oracles_home_fan_game(ORACLES_HOME_GAME_MOONRISE)->base == ORACLES_HOME_GAME_AGES && !oracles_home_fan_game(ORACLES_HOME_GAME_AGES));
    /* Gifts of Kinomi's from the Ages ROM, as Moonrise Regalia's. */
    oracles_home_state(&nav, ORACLES_HOME_HERO_KINOMI, state);
    CHECK(!strcmp(state, "ROM not found"));
    CHECK(oracles_home_fan_game(ORACLES_HOME_GAME_KINOMI)->base == ORACLES_HOME_GAME_AGES
          && !strcmp(oracles_home_fan_game(ORACLES_HOME_GAME_KINOMI)->image_name, "Gifts of Kinomi 1.1.2"));
}

static void menu(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    CHECK(oracles_home_items(&nav, items) == 6);
    static const char *const labels[6] = { "Start game", "Cartridge", "Controls", "Display", "Mods", "Exit" };
    for (int i = 0; i < 6; i++) CHECK(!strcmp(items[i].label, labels[i]));
    /* Without a ROM Start game is disabled, with its reason; Cartridge, Controls, Display and Mods open their pages. */
    CHECK(items[0].disabled && !strcmp(items[0].reason, "Choose a ROM in Cartridge first"));
    CHECK(!items[1].disabled && !items[2].disabled && !items[3].disabled);
    CHECK(!items[4].disabled && items[4].note == NULL);   /* no mod active: no note */
    CHECK(!items[5].disabled);
    /* A disabled item is highlighted like the others and OK does nothing there. */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    /* Controls opens on its first cell; Back returns to Controls in the menu. */
    CHECK(oracles_home_focus(&nav) == 2 && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_CONTROLS);
    CHECK(nav.controls.column == 0 && nav.controls.row == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_HOME && oracles_home_focus(&nav) == 2);
    /* Mods opens on its folder; Back returns to Mods in the menu. */
    nav.focus = 4;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_MODS && nav.row == ORACLES_MODS_ROW_FOLDER);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_HOME && oracles_home_focus(&nav) == 4);
    /* Up from the first item wraps to Exit, which exits. */
    nav.focus = 0;
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(oracles_home_focus(&nav) == 5 && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_EXIT);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(oracles_home_focus(&nav) == 0);
    /* With a ROM, Start game starts the game of the entry. */
    nav.games[0].usable = 1;
    oracles_home_items(&nav, items);
    CHECK(!items[0].disabled && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_START_AGES);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY);   /* Seasons has none */
    nav.games[1].usable = 1;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_START_SEASONS);
}

/* Mods: two mods of Ages and one refused (the layout reference's 8c), switched with the keys and the pointer, eight at
 * most, Play with the mods, and the reasons it cannot. */
static void add_mod(OraclesHomeNav *nav, int game, const char *name, const char *line, int refused, int active)
{
    OraclesHomeMod *m = &nav->mods[game].mods[nav->mods[game].count++];
    memset(m, 0, sizeof *m);
    snprintf(m->name, sizeof m->name, "%s", name);
    snprintf(m->line, sizeof m->line, "%s", line);
    m->refused = refused;
    m->active = active;
    m->houses[ORACLES_HOME_GAME_AGES] = !refused;
}

static void mods(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.games[ORACLES_HOME_GAME_AGES].usable = 1;
    nav.games[ORACLES_HOME_GAME_AGES].rom = ORACLES_ROM_ORIGINAL;
    add_mod(&nav, ORACLES_HOME_GAME_AGES, "claw-game", "A house in Lynna City and in Horon, and a claw game", 0, 1);
    add_mod(&nav, ORACLES_HOME_GAME_AGES, "fortune-teller", "", 0, 0);
    add_mod(&nav, ORACLES_HOME_GAME_AGES, "night-market", "Refused: mod night-market has no main.lua", 1, 0);
    nav.mods[ORACLES_HOME_GAME_AGES].mods[0].houses[ORACLES_HOME_GAME_SEASONS] = 1;
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    char text[ORACLES_HOME_TEXT_LENGTH];

    /* The menu says how many are active; Seasons has none. */
    oracles_home_items(&nav, items);
    CHECK(!items[4].disabled && items[4].note && !strcmp(items[4].note, "1 active"));
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    oracles_home_items(&nav, items);
    CHECK(!items[4].disabled && items[4].note == NULL);
    oracles_home_act(&nav, ORACLES_HOME_LEFT);
    nav.focus = 4;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_MODS);
    CHECK(oracles_mods_rows(&nav) == 5 && oracles_mods_row_play(&nav) == 4);

    /* The texts: the games of each mod, none for a refused one; the count; Play's note, empty on an original ROM. */
    oracles_mods_games(&nav.mods[0].mods[0], text, sizeof text);
    CHECK(!strcmp(text, "Houses in Ages and Seasons"));
    oracles_mods_games(&nav.mods[0].mods[1], text, sizeof text);
    CHECK(!strcmp(text, "Houses in Ages"));
    oracles_mods_games(&nav.mods[0].mods[2], text, sizeof text);
    CHECK(!strcmp(text, ""));
    oracles_mods_count(&nav, text, sizeof text);
    CHECK(!strcmp(text, "1 of 8 active \xc2\xb7 they play in the order of their names"));
    CHECK(!strcmp(oracles_mods_play_note(&nav), ""));

    /* OK on Folder opens it; up from Folder wraps to Play, which plays Ages with its mods. */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_OPEN_MODS);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY);
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(nav.row == 4 && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_START_AGES_MODS);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == ORACLES_MODS_ROW_FOLDER);

    /* A mod's row: right switches it on, left off, OK switches it; each change is kept. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == 2 && oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.mods[0].mods[1].active);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY);   /* on already */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && !nav.mods[0].mods[1].active);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.mods[0].mods[1].active);
    CHECK(oracles_mods_active(&nav, ORACLES_HOME_GAME_AGES) == 2);
    /* A refused mod does not switch on. */
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY);
    CHECK(!nav.mods[0].mods[2].active);

    /* The pointer: a row hovered is highlighted, a mod clicked switches, Play clicked plays. */
    oracles_mods_hover(&nav, 1);
    CHECK(nav.row == 1 && oracles_mods_click(&nav, 1) == ORACLES_HOME_STORE && !nav.mods[0].mods[0].active);
    CHECK(oracles_mods_click(&nav, 4) == ORACLES_HOME_START_AGES_MODS && nav.row == 4);
    CHECK(oracles_mods_click(&nav, 5) == ORACLES_HOME_STAY && nav.row == 4);   /* no such row */

    /* Eight at most: a ninth stays off, and the screen says why. */
    for (int i = 0; i < 7; i++) {
        char name[16];
        snprintf(name, sizeof name, "extra-%d", i);
        add_mod(&nav, ORACLES_HOME_GAME_AGES, name, "", 0, 1);
    }
    CHECK(oracles_mods_active(&nav, ORACLES_HOME_GAME_AGES) == 8);
    CHECK(oracles_mods_click(&nav, 1) == ORACLES_HOME_MODS_LIMIT && !nav.mods[0].mods[0].active);
    CHECK(!strcmp(oracles_mods_limit, "Eight mods at most play together"));

    /* Play cannot start without a ROM, or on a ROM that is not an original. */
    nav.games[ORACLES_HOME_GAME_AGES].rom = ORACLES_ROM_UNRECOGNISED;
    CHECK(!strcmp(oracles_mods_play_note(&nav), "Mods play on the original ROMs"));
    CHECK(oracles_mods_click(&nav, oracles_mods_row_play(&nav)) == ORACLES_HOME_STAY);
    nav.games[ORACLES_HOME_GAME_AGES].usable = 0;
    nav.games[ORACLES_HOME_GAME_AGES].rom = ORACLES_ROM_NONE;
    CHECK(!strcmp(oracles_mods_play_note(&nav), "Choose a ROM in Cartridge first"));
    nav.row = oracles_mods_row_play(&nav);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY);

    /* Seasons' page lists its own mods: none here, Folder and Play only; Play starts Seasons. */
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(nav.screen == ORACLES_SCREEN_HOME && oracles_home_focus(&nav) == 4);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    nav.games[ORACLES_HOME_GAME_SEASONS].usable = 1;
    nav.games[ORACLES_HOME_GAME_SEASONS].rom = ORACLES_ROM_ORIGINAL;
    nav.focus = 4;
    oracles_home_act(&nav, ORACLES_HOME_OK);
    CHECK(nav.screen == ORACLES_SCREEN_MODS && oracles_mods_rows(&nav) == 2);
    oracles_mods_count(&nav, text, sizeof text);
    CHECK(!strcmp(text, "0 of 8 active"));
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == 1 && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_START_SEASONS_MODS);

    /* A fan game's menu greys Mods, with the reason. */
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    oracles_home_act(&nav, ORACLES_HOME_OK);   /* Moonrise Regalia, in the list */
    CHECK(oracles_home_hero(&nav) == ORACLES_HOME_HERO_MOONRISE);
    oracles_home_items(&nav, items);
    CHECK(items[4].disabled && !strcmp(items[4].reason, "Mods play on the original ROMs") && items[4].note == NULL);
    nav.focus = 4;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_HOME);
}

static void fan_games(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.games[0].usable = nav.games[1].usable = 1;
    oracles_home_select(&nav, ORACLES_HOME_FAN);
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    CHECK(oracles_home_hero(&nav) == ORACLES_HOME_HERO_FAN);
    /* The fan games in the list, each with its state, then Exit. */
    CHECK(oracles_home_items(&nav, items) == 4);
    CHECK(!strcmp(items[0].label, "Gifts of Kinomi") && !strcmp(items[0].note, "ROM not found"));
    CHECK(!strcmp(items[1].label, "Moonrise Regalia") && !strcmp(items[1].note, "ROM not found"));
    CHECK(!strcmp(items[2].label, "Temple of Seasons") && !strcmp(items[2].note, "ROM not found"));
    CHECK(!strcmp(items[3].label, "Exit"));
    /* With its Oracle's ROM there, a fan game misses its patch. */
    nav.games[ORACLES_HOME_GAME_TEMPLE].rom = ORACLES_ROM_ORIGINAL;
    CHECK(oracles_home_items(&nav, items) == 4 && !strcmp(items[2].note, "Patch not found") && !strcmp(items[0].note, "ROM not found"));
    nav.games[ORACLES_HOME_GAME_TEMPLE].rom = ORACLES_ROM_NONE;
    CHECK(oracles_home_hints(&nav, hints) == 3);
    /* Back in the list stays in the list. */
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(nav.entry == ORACLES_HOME_FAN && oracles_home_hero(&nav) == ORACLES_HOME_HERO_FAN);
    /* A fan game opens with the game's menu: Start game waits for its image, whatever the Ages ROM alone; Cartridge
     * opens. */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY);
    CHECK(oracles_home_hero(&nav) == ORACLES_HOME_HERO_KINOMI && oracles_home_focus(&nav) == 0);
    CHECK(oracles_home_game(&nav) == ORACLES_HOME_GAME_KINOMI);
    CHECK(oracles_home_items(&nav, items) == 6 && items[0].disabled && !strcmp(items[0].reason, "Choose both files in Cartridge first") && !items[1].disabled);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY);
    CHECK(oracles_home_hints(&nav, hints) == 4 && hints[3].back && !strcmp(hints[3].label, "Fan games"));
    nav.games[ORACLES_HOME_GAME_KINOMI].usable = 1;
    oracles_home_items(&nav, items);
    CHECK(!items[0].disabled && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_START_KINOMI);
    nav.games[ORACLES_HOME_GAME_KINOMI].usable = 0;
    /* The pause over its session keeps it picked, and is left for the list. */
    OraclesHomeNav paused;
    oracles_home_init(&paused);
    oracles_home_pause(&paused, ORACLES_HOME_GAME_KINOMI, ORACLES_PROFILE_ENHANCED, 0);
    CHECK(paused.entry == ORACLES_HOME_FAN && oracles_home_hero(&paused) == ORACLES_HOME_HERO_KINOMI && oracles_home_game(&paused) == ORACLES_HOME_GAME_KINOMI);
    /* Back returns to the list with the game's line highlighted. */
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(oracles_home_hero(&nav) == ORACLES_HOME_HERO_FAN && oracles_home_focus(&nav) == 0);
    CHECK(oracles_home_click(&nav, 0) == ORACLES_HOME_STAY && oracles_home_hero(&nav) == ORACLES_HOME_HERO_KINOMI);
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(oracles_home_focus(&nav) == 0);
    /* The second line opens Moonrise Regalia, whose Start game gives its own command. */
    CHECK(oracles_home_click(&nav, 1) == ORACLES_HOME_STAY && oracles_home_hero(&nav) == ORACLES_HOME_HERO_MOONRISE);
    CHECK(oracles_home_game(&nav) == ORACLES_HOME_GAME_MOONRISE);
    nav.games[ORACLES_HOME_GAME_MOONRISE].usable = 1;
    nav.focus = 0;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_START_MOONRISE);
    nav.games[ORACLES_HOME_GAME_MOONRISE].usable = 0;
    oracles_home_pause(&paused, ORACLES_HOME_GAME_MOONRISE, ORACLES_PROFILE_ENHANCED, 0);
    CHECK(oracles_home_hero(&paused) == ORACLES_HOME_HERO_MOONRISE && oracles_home_game(&paused) == ORACLES_HOME_GAME_MOONRISE);
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    /* The third, Temple of Seasons, the same with its own. */
    CHECK(oracles_home_click(&nav, 2) == ORACLES_HOME_STAY && oracles_home_hero(&nav) == ORACLES_HOME_HERO_TEMPLE);
    CHECK(oracles_home_game(&nav) == ORACLES_HOME_GAME_TEMPLE);
    nav.games[ORACLES_HOME_GAME_TEMPLE].usable = 1;
    nav.focus = 0;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_START_TEMPLE);
    nav.games[ORACLES_HOME_GAME_TEMPLE].usable = 0;
    oracles_home_pause(&paused, ORACLES_HOME_GAME_TEMPLE, ORACLES_PROFILE_ENHANCED, 0);
    CHECK(oracles_home_hero(&paused) == ORACLES_HOME_HERO_TEMPLE && oracles_home_game(&paused) == ORACLES_HOME_GAME_TEMPLE);
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    /* Changing entry from a fan game leaves the fan game. */
    oracles_home_click(&nav, 0);
    oracles_home_act(&nav, ORACLES_HOME_LEFT);
    CHECK(nav.entry == ORACLES_HOME_SEASONS);
    oracles_home_act(&nav, ORACLES_HOME_RIGHT);
    CHECK(oracles_home_hero(&nav) == ORACLES_HOME_HERO_FAN);
}

static void pointer(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    oracles_home_hover(&nav, 4);
    CHECK(oracles_home_focus(&nav) == 4);
    oracles_home_hover(&nav, 9);   /* past the menu: nothing */
    CHECK(oracles_home_focus(&nav) == 4);
    CHECK(oracles_home_click(&nav, 5) == ORACLES_HOME_EXIT && oracles_home_focus(&nav) == 5);
    CHECK(oracles_home_click(&nav, 0) == ORACLES_HOME_STAY && oracles_home_focus(&nav) == 0);
    /* The highlight is kept within a shorter menu. */
    nav.focus = 5;
    oracles_home_select(&nav, ORACLES_HOME_FAN);
    nav.focus = 5;
    CHECK(oracles_home_focus(&nav) == 3);
}

int main(void)
{
    entries();
    states();
    menu();
    fan_games();
    mods();
    pointer();
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("launcher navigation: entries, fan games, disabled items and going back behave as expected\n");
    return 0;
}
