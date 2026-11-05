/* The home screen's navigation (ui_home_nav.h): entries, the list of fan
 * games, disabled items and their reasons, going back, the pointer; Display's rows from the pause; the newer version's
 * notice. */
#include "ui_font.h"
#include "ui_home_nav.h"
#include "ui_page_layout.h"
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
    CHECK(oracles_home_items(&nav, items) == 7);
    static const char *const labels[7] = { "Start game", "Cartridge", "Controls", "Display", "Mods", "Cheats", "Exit" };
    for (int i = 0; i < 7; i++) CHECK(!strcmp(items[i].label, labels[i]));
    /* Without a ROM Start game is disabled, with its reason; Cartridge, Controls, Display, Mods and Cheats open their
     * pages. */
    CHECK(items[0].disabled && !strcmp(items[0].reason, "Choose a ROM in Cartridge first"));
    CHECK(!items[1].disabled && !items[2].disabled && !items[3].disabled);
    CHECK(!items[4].disabled && items[4].note == NULL);   /* no mod active: no note */
    CHECK(!items[5].disabled && !items[6].disabled);
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
    /* Cheats opens on No damage; Back returns to Cheats in the menu. */
    nav.focus = 5;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_CHEATS && nav.row == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_HOME && oracles_home_focus(&nav) == 5);
    /* Up from the first item reaches the version number, then Exit, which exits. */
    nav.focus = 0;
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(oracles_home_focus(&nav) == 7 && oracles_home_version_focused(&nav));
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(oracles_home_focus(&nav) == 6 && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_EXIT);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
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
    /* Its patch there but refused, or the image it makes refused (another fan game's): refused, not missing. */
    nav.games[ORACLES_HOME_GAME_TEMPLE].patch = ORACLES_ROM_REFUSED;
    CHECK(oracles_home_items(&nav, items) == 4 && !strcmp(items[2].note, "Patch refused"));
    nav.games[ORACLES_HOME_GAME_TEMPLE].patch = ORACLES_ROM_ORIGINAL;
    nav.games[ORACLES_HOME_GAME_TEMPLE].image = ORACLES_ROM_REFUSED;
    CHECK(oracles_home_items(&nav, items) == 4 && !strcmp(items[2].note, "Patch refused"));
    nav.games[ORACLES_HOME_GAME_TEMPLE].patch = ORACLES_ROM_NONE;
    nav.games[ORACLES_HOME_GAME_TEMPLE].image = ORACLES_ROM_NONE;
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
    CHECK(items[5].id == ORACLES_HOME_ITEM_EXIT);   /* no Cheats for a fan game */
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
    CHECK(oracles_home_click(&nav, 6) == ORACLES_HOME_EXIT && oracles_home_focus(&nav) == 6);
    CHECK(oracles_home_click(&nav, 0) == ORACLES_HOME_STAY && oracles_home_focus(&nav) == 0);
    /* The highlight is kept within a shorter menu. */
    nav.focus = 5;
    oracles_home_select(&nav, ORACLES_HOME_FAN);
    nav.focus = 5;
    CHECK(oracles_home_focus(&nav) == 3);
}

/* The toast after a game that ran slowly: what it suggests, from Display's quality or its view; the first input takes it
 * away, changing nothing; a click on it opens Display on the row it names. */
static void slow_toast(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    char text[ORACLES_HOME_TEXT_LENGTH];
    const char *path = NULL;
    static const struct { int view, core, workers; const char *text, *path; } cases[] = {
        { 2, 1, 0, "The game ran slowly at High: Medium may play smoother.", "Display \xe2\x80\xba Quality" },
        { 2, 0, 2, "The game ran slowly at Max: High may play smoother.", "Display \xe2\x80\xba Quality" },
        { 1, 1, 0, "The game ran slowly at Medium: Low may play smoother.", "Display \xe2\x80\xba Quality" },
        { 2, 0, 0, "The game ran slowly in the Far view: the Medium view may play smoother.", "Display \xe2\x80\xba View" },   /* Custom */
        { 1, 0, 1, "The game ran slowly in the Medium view: the Near view may play smoother.", "Display \xe2\x80\xba View" },
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        nav.display.view = cases[i].view;
        nav.display.core = cases[i].core;
        nav.display.workers = cases[i].workers;
        CHECK(oracles_home_slow_text(&nav, text, sizeof text, &path) && !strcmp(text, cases[i].text) && !strcmp(path, cases[i].path));
    }
    /* Nothing lighter: Low, and Custom in the near view. */
    nav.display.view = 0; nav.display.core = 1; nav.display.workers = 0;
    CHECK(!oracles_home_slow_text(&nav, text, sizeof text, &path));
    nav.display.core = 0;
    CHECK(!oracles_home_slow_text(&nav, text, sizeof text, &path));

    /* The first input takes it away, and does what it does; no setting changes. */
    oracles_home_init(&nav);
    nav.display.core = 1;
    nav.slow = 1;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && !nav.slow && nav.focus == 1);
    CHECK(nav.display.view == 2 && nav.display.core == 1 && nav.display.workers == 0);
    nav.slow = 1;
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(!nav.slow && nav.screen == ORACLES_SCREEN_HOME);
    nav.slow = 1;
    oracles_home_click(&nav, 2);
    CHECK(!nav.slow);
    nav.screen = ORACLES_SCREEN_HOME;
    nav.slow = 1;
    oracles_home_select(&nav, ORACLES_HOME_SEASONS);
    CHECK(!nav.slow);

    /* A click on it: Display, its own rows, on Quality; back on the home screen, Display highlighted. */
    oracles_home_init(&nav);
    nav.display.core = 1;
    nav.slow = 1;
    nav.advanced = 1;
    oracles_home_slow_open(&nav);
    CHECK(!nav.slow && nav.screen == ORACLES_SCREEN_DISPLAY && !nav.advanced && nav.row == ORACLES_DISPLAY_QUALITY);
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(nav.screen == ORACLES_SCREEN_HOME && nav.focus == 3);
    /* Under Custom, on View. */
    nav.display.core = 0;
    nav.slow = 1;
    oracles_home_slow_open(&nav);
    CHECK(nav.screen == ORACLES_SCREEN_DISPLAY && nav.row == ORACLES_DISPLAY_VIEW);
}

/* Display from the pause: the HUD applies at once to the running game, so its row follows that game's profile, not
 * the one chosen for the next Play. */
static void display_in_game(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.row = ORACLES_DISPLAY_HUD;
    CHECK(!oracles_display_hud_applies(&nav) && oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.display.hud == 1);
    oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, 0);
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.row = ORACLES_DISPLAY_HUD;
    CHECK(oracles_display_hud_applies(&nav) && oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.hud == 2);
    oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_FAITHFUL, 0);
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.row = ORACLES_DISPLAY_HUD;
    CHECK(!oracles_display_hud_applies(&nav) && oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.display.hud == 2);
    CHECK(oracles_display_transitions_apply(&nav));   /* the view and the transitions are the next Play's */
    /* Advanced opens on its first row that changes: Vsync from a game, which keeps the core, the transitions and the
     * workers, in either profile chosen; Core on the home screen. */
    nav.row = ORACLES_DISPLAY_ADVANCED;
    CHECK(oracles_display_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.advanced && nav.row == ORACLES_DISPLAY_VSYNC);
    oracles_display_act(&nav, ORACLES_HOME_BACK);
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_ADVANCED, -1) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_VSYNC);
    oracles_display_act(&nav, ORACLES_HOME_BACK);
    nav.in_game = 0;
    CHECK(oracles_display_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_CORE);
    /* The margin --screen-margin gives for the run is shown and does not change, on the home screen as from a game. */
    nav.display.margin = 2;
    nav.display.margin_fixed = 1;
    oracles_display_act(&nav, ORACLES_HOME_BACK);
    nav.row = ORACLES_DISPLAY_PICTURE;
    CHECK(oracles_display_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.picture_page);
    CHECK(oracles_display_row_fixed(&nav, ORACLES_DISPLAY_MARGIN) && oracles_display_click(&nav, ORACLES_DISPLAY_MARGIN, 3) == ORACLES_HOME_STAY);
    nav.row = ORACLES_DISPLAY_MARGIN;
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.display.margin == 2);
    nav.display.margin_fixed = 0;
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.margin == 3);
}

/* Display from the pause steps over the windows whose pause would lose Display (nav->display.windows_refused), which
 * a click leaves alone too, and says that they change from the home screen; on the home screen every window changes. */
static void windows_refused(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, 0);
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.row = ORACLES_DISPLAY_WINDOW;
    nav.display.window = 3;
    nav.display.windows_refused = 1u << 0 | 1u << 1;   /* 2x and 3x */
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.window == 2);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.display.window == 3);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.display.window == 2);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_WINDOW, 0) == ORACLES_HOME_STAY && nav.display.window == 2);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_WINDOW, 1) == ORACLES_HOME_STAY && nav.display.window == 2);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_WINDOW, 3) == ORACLES_HOME_STORE && nav.display.window == 3);
    char note[ORACLES_HOME_TEXT_LENGTH];
    oracles_display_reduced(&nav, note, sizeof note);
    CHECK(!strcmp(note, oracles_display_windows_refused_note) && !strcmp(note, "Smaller windows change from the home screen."));
    nav.display.windows_refused = 0;
    oracles_display_reduced(&nav, note, sizeof note);
    CHECK(!note[0]);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_WINDOW, 0) == ORACLES_HOME_STORE && nav.display.window == 0);
    /* A window no choice is (F11 out of fullscreen at the home screen's size): none highlighted, and any choice, the
     * first to the right included, is one that the pause applies and stores. */
    nav.display.window = -1;
    nav.display.window_w = 1280;
    nav.display.window_h = 720;
    nav.display.window_chosen = 0;
    float dw, dh;
    char label[64];
    oracles_display_diagram(&nav, 432.0f, 243.0f, &dw, &dh, label, sizeof label);
    CHECK(!strcmp(label, "1280\xc3\x97" "720 on a 1920\xc3\x97" "1080 screen"));
    oracles_display_reduced(&nav, note, sizeof note);
    CHECK(!note[0]);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.window == 0 && nav.display.window_chosen);
    nav.display.window = -1;
    nav.display.window_chosen = 0;
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_WINDOW, 2) == ORACLES_HOME_STORE && nav.display.window == 2 && nav.display.window_chosen);
    nav.row = ORACLES_DISPLAY_HUD;
    nav.display.window_chosen = 0;
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && !nav.display.window_chosen);
    /* Medium at a stored 4x on a 1366x768 laptop, opened at 3x, 2x refused: both notes on one line, which fits the 16:9
     * column's two lines and the 4:3 help's note line. */
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.display.view = 1;
    nav.display.screen_w = 1366;
    nav.display.screen_h = 768;
    nav.display.room_w = 1366;
    nav.display.room_h = 728 - 64;
    nav.display.window = 2;
    nav.display.windows_refused = 1u;
    oracles_display_reduced(&nav, note, sizeof note);
    CHECK(!strcmp(note, "Reduced to 3\xc3\x97 to fit this screen; smaller ones from the home screen."));
    nav.row = ORACLES_DISPLAY_WINDOW;
    for (int layout = 0; layout < 2; layout++) {
        nav.layout = layout ? ORACLES_UI_LAYOUT_4_3 : ORACLES_UI_LAYOUT_16_9;
        OraclesUiDisplayTexts t;
        oracles_ui_display_texts(&nav, &t);
        OraclesUiDisplayLayout l;
        oracles_ui_layout_display(nav.layout, &t, &l);
        if (layout) CHECK(t.help_note == t.window_reduced && l.help_note.x + l.help_note.w <= 1440.0f - 64.0f);
        else CHECK(t.window_fit == t.window_reduced && l.window_reduced.count >= 1 && l.window_reduced.count <= 2);
    }
}

/* The newer version's notice and the version number (turn 32): without the notice, down from Exit reaches the number,
 * down again Start game, and up from Start game the number; with it, down from Exit reaches the notice, then the number,
 * then Start game.  OK, a click or a tap opens the notice's page, the launcher staying, or About from the number, whose
 * Back comes back to it; Select's hint says Open on both.  About's Update check off hides the notice.  On the home screen
 * only: not in the pause, not on a page. */
static void update_notice(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.games[0].usable = 1;
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    CHECK(!oracles_home_update_shown(&nav) && nav.update_check);
    nav.focus = 6;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && oracles_home_focus(&nav) == 7 && oracles_home_version_focused(&nav)
          && !oracles_home_update_focused(&nav));
    CHECK(oracles_home_hints(&nav, hints) == 3 && !strcmp(hints[2].label, "Open"));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && oracles_home_focus(&nav) == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && oracles_home_version_focused(&nav));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && oracles_home_focus(&nav) == 6);
    oracles_home_hover(&nav, 8);
    CHECK(oracles_home_focus(&nav) == 6 && oracles_home_click(&nav, 8) == ORACLES_HOME_STAY);
    /* The number opens About, on its row; Back returns to the number. */
    CHECK(oracles_home_click(&nav, 7) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_ABOUT && nav.row == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_HOME && oracles_home_version_focused(&nav));

    snprintf(nav.update, sizeof nav.update, "1.3.0");
    nav.focus = 6;
    CHECK(oracles_home_update_shown(&nav) && !oracles_home_update_focused(&nav));
    CHECK(oracles_home_hints(&nav, hints) == 3 && !strcmp(hints[2].label, "Select"));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && oracles_home_focus(&nav) == 7 && oracles_home_update_focused(&nav));
    CHECK(oracles_home_hints(&nav, hints) == 3 && !strcmp(hints[2].label, "Open") && !strcmp(hints[2].key, "Enter / A"));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_OPEN_UPDATE && nav.screen == ORACLES_SCREEN_HOME && oracles_home_update_focused(&nav));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && oracles_home_focus(&nav) == 8 && oracles_home_version_focused(&nav));
    CHECK(oracles_home_hints(&nav, hints) == 3 && !strcmp(hints[2].label, "Open"));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_ABOUT);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && oracles_home_focus(&nav) == 8);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && oracles_home_focus(&nav) == 0 && !oracles_home_update_focused(&nav));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && oracles_home_version_focused(&nav));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && oracles_home_update_focused(&nav));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && oracles_home_focus(&nav) == 6);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_EXIT);
    /* The pointer: hovered, they take the highlight; clicked or tapped, they open.  Past them, nothing. */
    oracles_home_hover(&nav, 7);
    CHECK(oracles_home_update_focused(&nav));
    oracles_home_hover(&nav, 9);
    CHECK(oracles_home_focus(&nav) == 7);
    oracles_home_hover(&nav, 8);
    CHECK(oracles_home_version_focused(&nav));
    oracles_home_hover(&nav, 2);
    CHECK(oracles_home_click(&nav, 7) == ORACLES_HOME_OPEN_UPDATE && oracles_home_update_focused(&nav) && nav.screen == ORACLES_SCREEN_HOME);
    CHECK(oracles_home_click(&nav, 9) == ORACLES_HOME_STAY);
    /* Another entry highlights its first item. */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.entry == ORACLES_HOME_SEASONS && oracles_home_focus(&nav) == 0);
    /* The list of fan games: after its Exit. */
    oracles_home_select(&nav, ORACLES_HOME_FAN);
    nav.focus = 3;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && oracles_home_focus(&nav) == 4 && oracles_home_update_focused(&nav));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_OPEN_UPDATE && oracles_home_hero(&nav) == ORACLES_HOME_HERO_FAN);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && oracles_home_focus(&nav) == 5 && oracles_home_version_focused(&nav));

    /* About's Update check: left, right and OK turn it off and on, kept at once; off, the notice goes, the number
     * taking its place; a click on On or Off sets it. */
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_ABOUT);
    char note[64];
    oracles_about_note(&nav, "v1.2.0 (abc1234)", note, sizeof note);
    CHECK(!strcmp(note, "v1.2.0 (abc1234) \xc2\xb7 1.3.0 available"));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == 0 && nav.update_check);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && !nav.update_check);
    oracles_about_note(&nav, "v1.2.0 (abc1234)", note, sizeof note);
    CHECK(!strcmp(note, "v1.2.0 (abc1234)"));
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.update_check);
    CHECK(oracles_about_click(&nav, 1) == ORACLES_HOME_STORE && !nav.update_check);
    CHECK(oracles_about_click(&nav, 1) == ORACLES_HOME_STAY && oracles_about_click(&nav, -1) == ORACLES_HOME_STAY && !nav.update_check);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_HOME && !oracles_home_update_shown(&nav)
          && oracles_home_focus(&nav) == 4 && oracles_home_version_focused(&nav));
    CHECK(oracles_about_click(&nav, 0) == ORACLES_HOME_STORE && nav.update_check && oracles_home_update_shown(&nav));

    /* Not on a page, where Select stays Select. */
    oracles_home_select(&nav, ORACLES_HOME_AGES);
    nav.focus = 3;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_DISPLAY && !oracles_home_update_shown(&nav)
          && !oracles_home_version_focused(&nav));
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(nav.screen == ORACLES_SCREEN_HOME && oracles_home_focus(&nav) == 3 && oracles_home_update_shown(&nav));
    /* Nor in the pause: its menu wraps from Quit to launcher to Resume. */
    oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, 0);
    CHECK(!oracles_home_update_shown(&nav));
    nav.focus = 6;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && oracles_home_focus(&nav) == 0 && !oracles_home_version_focused(&nav));
    CHECK(oracles_home_click(&nav, 7) == ORACLES_HOME_STAY && oracles_home_hints(&nav, hints) == 3 && !strcmp(hints[1].label, "Select"));
}

int main(void)
{
    if (!oracles_ui_fonts_load()) { fprintf(stderr, "FAIL the embedded fonts do not load\n"); return 1; }
    entries();
    states();
    menu();
    fan_games();
    mods();
    pointer();
    slow_toast();
    display_in_game();
    windows_refused();
    update_notice();
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("launcher navigation: entries, fan games, disabled items and going back behave as expected\n");
    return 0;
}
