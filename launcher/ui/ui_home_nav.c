#include "ui_home_nav.h"

#include "ui_controls_nav.h"
#include "ui_page_nav.h"

#include <stdio.h>
#include <string.h>

/* The home screen's texts: each hero's line above its title, and its title. */
static const char *const overs[ORACLES_HOME_HEROES] = { "Oracle of", "Oracle of", "Community", "Fan games", "Fan games", "Fan games" };
static const char *const titles[ORACLES_HOME_HEROES] = { "Ages", "Seasons", "Fan games", "Gifts of Kinomi", "Moonrise Regalia", "Temple of Seasons" };

const OraclesHomeFanGame oracles_home_fan_games[ORACLES_HOME_FAN_GAMES] = {
    [ORACLES_HOME_FAN_KINOMI] = { "kinomi", ORACLES_HOME_GAME_KINOMI, ORACLES_HOME_HERO_KINOMI, ORACLES_HOME_ITEM_KINOMI, ORACLES_HOME_START_KINOMI,
      ORACLES_HOME_GAME_AGES, "Oracle of Ages (USA)", "Gifts of Kinomi 1.1.2", "In Gifts of Kinomi" },
    [ORACLES_HOME_FAN_MOONRISE] = { "moonrise", ORACLES_HOME_GAME_MOONRISE, ORACLES_HOME_HERO_MOONRISE, ORACLES_HOME_ITEM_MOONRISE, ORACLES_HOME_START_MOONRISE,
      ORACLES_HOME_GAME_AGES, "Oracle of Ages (USA)", "Moonrise Regalia 1.0.6", "In Moonrise Regalia" },
    [ORACLES_HOME_FAN_TEMPLE] = { "temple", ORACLES_HOME_GAME_TEMPLE, ORACLES_HOME_HERO_TEMPLE, ORACLES_HOME_ITEM_TEMPLE, ORACLES_HOME_START_TEMPLE,
      ORACLES_HOME_GAME_SEASONS, "Oracle of Seasons (USA)", "Temple of Seasons 1.073", "In Temple of Seasons" },
};

const OraclesHomeFanGame *oracles_home_fan_game_started(OraclesHomeCommand command)
{
    for (unsigned f = 0; f < ORACLES_HOME_FAN_GAMES; f++) if (oracles_home_fan_games[f].start == command) return &oracles_home_fan_games[f];
    return NULL;
}

const OraclesHomeFanGame *oracles_home_fan_game(int game)
{
    for (unsigned f = 0; f < ORACLES_HOME_FAN_GAMES; f++) if (oracles_home_fan_games[f].game == game) return &oracles_home_fan_games[f];
    return NULL;
}
static const char *const profile_states[ORACLES_PROFILES] = { "Profile: Faithful", "Profile: Enhanced" };

void oracles_home_init(OraclesHomeNav *nav)
{
    memset(nav, 0, sizeof *nav);
    nav->entry = ORACLES_HOME_AGES;
    nav->fan_pick = ORACLES_HOME_HERO_FAN;
    nav->display.profile = ORACLES_PROFILE_ENHANCED;
    nav->display.transitions = 1;
    nav->display.hud = 1;   /* Full width, settings.txt's default */
    nav->display.camera = -1;   /* the platform's default */
    nav->display.view = 2;
    nav->display.window = 2;
    nav->update_check = 1;
    nav->display.screen_w = 1920;
    nav->display.screen_h = 1080;
    nav->display.room_w = 1920;
    nav->display.room_h = 1080 - 64;
    oracles_controls_defaults(&nav->controls);
#ifdef __ANDROID__
    nav->android = 1;
#endif
}

void oracles_home_pause(OraclesHomeNav *nav, int game, OraclesProfile playing, int narrow)
{
    nav->entry = game == ORACLES_HOME_GAME_AGES ? ORACLES_HOME_AGES : game == ORACLES_HOME_GAME_SEASONS ? ORACLES_HOME_SEASONS : ORACLES_HOME_FAN;
    const OraclesHomeFanGame *fan = oracles_home_fan_game(game);
    nav->fan_pick = fan ? fan->hero : ORACLES_HOME_HERO_FAN;
    nav->screen = ORACLES_SCREEN_PAUSE;
    nav->in_game = 1;
    nav->playing = playing;
    nav->narrow = narrow;
    nav->focus = 0;
}

OraclesHomeHero oracles_home_entry_hero(OraclesHomeEntry entry)
{
    return entry == ORACLES_HOME_AGES ? ORACLES_HOME_HERO_AGES : entry == ORACLES_HOME_SEASONS ? ORACLES_HOME_HERO_SEASONS : ORACLES_HOME_HERO_FAN;
}

OraclesHomeHero oracles_home_hero(const OraclesHomeNav *nav)
{
    return nav->entry == ORACLES_HOME_FAN ? nav->fan_pick : oracles_home_entry_hero(nav->entry);
}

const char *oracles_home_over(OraclesHomeHero hero) { return overs[hero]; }
const char *oracles_home_title(OraclesHomeHero hero) { return titles[hero]; }

/* The game a hero shows, or -1 for the list of fan games. */
static int hero_game(OraclesHomeHero hero)
{
    switch (hero) {
        case ORACLES_HOME_HERO_AGES: return ORACLES_HOME_GAME_AGES;
        case ORACLES_HOME_HERO_SEASONS: return ORACLES_HOME_GAME_SEASONS;
        default:
            for (unsigned f = 0; f < ORACLES_HOME_FAN_GAMES; f++) if (oracles_home_fan_games[f].hero == hero) return oracles_home_fan_games[f].game;
            return -1;
    }
}

void oracles_home_state(const OraclesHomeNav *nav, OraclesHomeHero hero, char out[ORACLES_HOME_STATE_LENGTH])
{
    if (nav->screen == ORACLES_SCREEN_PAUSE) {
        snprintf(out, ORACLES_HOME_STATE_LENGTH, "Paused \xc2\xb7 %s", profile_states[nav->playing == ORACLES_PROFILE_ENHANCED]);
        return;
    }
    if (hero == ORACLES_HOME_HERO_FAN) { snprintf(out, ORACLES_HOME_STATE_LENGTH, "%u games", ORACLES_HOME_FAN_GAMES); return; }
    const OraclesHomeGame *game = &nav->games[hero_game(hero)];
    /* A fan game whose Oracle's ROM is there misses its patch. */
    const int fan = hero != ORACLES_HOME_HERO_AGES && hero != ORACLES_HOME_HERO_SEASONS;
    const int base_found = fan && (game->rom == ORACLES_ROM_ORIGINAL || game->rom == ORACLES_ROM_UNRECOGNISED);
    /* A patch there but refused (not a BPS patch, another fan game's) says so. */
    const int patch_refused = base_found && (game->patch == ORACLES_ROM_REFUSED || game->image == ORACLES_ROM_REFUSED);
    if (!game->usable) snprintf(out, ORACLES_HOME_STATE_LENGTH, "%s", patch_refused ? "Patch refused" : base_found ? "Patch not found" : "ROM not found");
    else if (game->last_session[0]) snprintf(out, ORACLES_HOME_STATE_LENGTH, "Ready \xc2\xb7 last session %s", game->last_session);
    else snprintf(out, ORACLES_HOME_STATE_LENGTH, "Ready");
}

static OraclesHomeItem item(OraclesHomeItemId id, const char *label, const char *note, int disabled, const char *reason)
{
    const OraclesHomeItem it = { id, label, note, disabled, reason, "" };
    return it;
}

unsigned oracles_home_items(const OraclesHomeNav *nav, OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS])
{
    const OraclesHomeHero hero = oracles_home_hero(nav);
    if (nav->screen == ORACLES_SCREEN_PAUSE) {
        /* Narrow (a 16:9 window under 960 pixels wide), the menu keeps the session's own entries; the settings stay in
         * the launcher.  The 4:3 layout keeps them all at every size. */
        unsigned n = 0;
        items[n++] = item(ORACLES_HOME_ITEM_RESUME, "Resume", NULL, 0, NULL);
        items[n++] = item(ORACLES_HOME_ITEM_SAVE_STATE, "Save state", NULL, 0, NULL);
        items[n++] = nav->load_note[0] ? item(ORACLES_HOME_ITEM_LOAD_STATE, "Load state", nav->load_note, 0, NULL)
                                       : item(ORACLES_HOME_ITEM_LOAD_STATE, "Load state", NULL, 1, "No savestate yet");
        if (!nav->narrow) {
            items[n++] = item(ORACLES_HOME_ITEM_CONTROLS, "Controls", NULL, 0, NULL);
            items[n++] = item(ORACLES_HOME_ITEM_DISPLAY, "Display", NULL, 0, NULL);
            /* Cheats where they apply: not for a game whose profile has none. */
            if (!oracles_home_refuses(nav, ORACLES_HOME_REFUSES_CHEATS) && !nav->cheats_refused) items[n++] = item(ORACLES_HOME_ITEM_CHEATS, "Cheats", NULL, 0, NULL);
        }
        items[n++] = item(ORACLES_HOME_ITEM_QUIT_GAME, "Quit to launcher", NULL, 0, NULL);
        return n;
    }
    if (hero == ORACLES_HOME_HERO_FAN) {
        /* Each fan game with its state, as its title shows it once picked; its version is on its Cartridge page. */
        unsigned n = 0;
        for (unsigned f = 0; f < ORACLES_HOME_FAN_GAMES; f++, n++) {
            const OraclesHomeFanGame *fan = &oracles_home_fan_games[f];
            items[n] = item(fan->item, titles[fan->hero], NULL, 0, NULL);
            oracles_home_state(nav, fan->hero, items[n].text);
            items[n].note = items[n].text;
        }
        items[n++] = item(ORACLES_HOME_ITEM_EXIT, "Exit", NULL, 0, NULL);
        return n;
    }
    const int g = hero_game(hero);
    if (g < 0) return 0;   /* every hero but the list names a game */
    const OraclesHomeGame *game = &nav->games[g];
    const int fan = oracles_home_fan_game(g) != NULL;
    items[0] = item(ORACLES_HOME_ITEM_START, "Start game", NULL, !game->usable, fan ? "Choose both files in Cartridge first" : "Choose a ROM in Cartridge first");
    /* Beside Cartridge, the language the game plays in when it is not English. */
    const int language = oracles_page_language(nav, game);
    items[1] = item(ORACLES_HOME_ITEM_GAME, "Cartridge", language ? oracles_page_language_names[language] : NULL, 0, NULL);
    /* Controls and Display hold for every game: open from every entry, a fan game's included. */
    items[2] = item(ORACLES_HOME_ITEM_CONTROLS, "Controls", NULL, 0, NULL);
    /* Beside Display, the profile the game plays in: Display's, or Faithful for a ROM that allows nothing else. */
    const int profile = oracles_page_profile(game, nav->display.profile);
    items[3] = item(ORACLES_HOME_ITEM_DISPLAY, "Display", profile_states[profile < 0 ? (int)nav->display.profile : profile], 0, NULL);
    /* Mods, for an Oracle, with how many are active when any is; a fan game plays without them. */
    items[4] = item(ORACLES_HOME_ITEM_MODS, "Mods", NULL, fan, "Mods play on the original ROMs");
    const unsigned active = oracles_mods_active(nav, g);
    if (active) {
        snprintf(items[4].text, sizeof items[4].text, "%u active", active);
        items[4].note = items[4].text;
    }
    /* Cheats, for a game whose profile has them: an unrecognised ROM's menu keeps its six entries. */
    unsigned n = 5;
    if (!(game->refuses & ORACLES_HOME_REFUSES_CHEATS) && game->rom != ORACLES_ROM_UNRECOGNISED) items[n++] = item(ORACLES_HOME_ITEM_CHEATS, "Cheats", NULL, 0, NULL);
    items[n++] = item(ORACLES_HOME_ITEM_EXIT, "Exit", NULL, 0, NULL);
    return n;
}

int oracles_home_update_shown(const OraclesHomeNav *nav)
{
    return nav->update[0] && nav->update_check && nav->screen == ORACLES_SCREEN_HOME && !nav->in_game;
}

/* The places after the menu's items: the newer version's notice when shown, then the version number, on the home
 * screen alone. */
static unsigned extra_places(const OraclesHomeNav *nav)
{
    if (nav->screen != ORACLES_SCREEN_HOME || nav->in_game) return 0;
    return oracles_home_update_shown(nav) ? 2u : 1u;
}

unsigned oracles_home_focus(const OraclesHomeNav *nav)
{
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    const unsigned count = oracles_home_items(nav, items);
    return nav->focus < count + extra_places(nav) ? nav->focus : count - 1u;
}

void oracles_home_focus_item(OraclesHomeNav *nav, OraclesHomeItemId id)
{
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    const unsigned count = oracles_home_items(nav, items);
    nav->focus = 0;
    for (unsigned i = 0; i < count; i++) if (items[i].id == id) nav->focus = i;
}

int oracles_home_update_focused(const OraclesHomeNav *nav)
{
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    return oracles_home_update_shown(nav) && nav->focus == oracles_home_items(nav, items);
}

int oracles_home_version_focused(const OraclesHomeNav *nav)
{
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    return extra_places(nav) && nav->focus == oracles_home_items(nav, items) + extra_places(nav) - 1u;
}

/* About opened from the version number. */
static OraclesHomeCommand open_about(OraclesHomeNav *nav)
{
    nav->screen = ORACLES_SCREEN_ABOUT;
    nav->row = 0;
    return ORACLES_HOME_STAY;
}

void oracles_home_others(const OraclesHomeNav *nav, OraclesHomeEntry others[2])
{
    unsigned n = 0;
    for (int e = 0; e < ORACLES_HOME_ENTRIES; e++)
        if ((OraclesHomeEntry)e != nav->entry) others[n++] = (OraclesHomeEntry)e;
}

#define UP_DOWN "\xe2\x86\x91\xe2\x86\x93"              /* ↑↓ */
#define LEFT_RIGHT "\xe2\x86\x90\xe2\x86\x92"           /* ←→ */

/* The hints of the screen, their keys both devices'. */
static unsigned hints_of(const OraclesHomeNav *nav, OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS])
{
    if (nav->screen == ORACLES_SCREEN_PAUSE) {
        static const OraclesHomeHint pause[3] = {
            { UP_DOWN, "Menu", 0, 0 },
            { "Enter / A", "Select", 0, 0 },
            { "Esc / B", "Resume", 1, 0 },
        };
        memcpy(hints, pause, sizeof pause);
        return 3;
    }
    if (nav->screen == ORACLES_SCREEN_CONTROLS) {
        static const OraclesHomeHint capture[1] = { { "Esc", "Cancel", 1, 0 } };
        static const OraclesHomeHint controls[3] = {
            { UP_DOWN LEFT_RIGHT, "Move", 0, 0 },
            { "Enter / A", "Rebind", 0, 0 },
            { "Esc / B", "Back", 1, 0 },
        };
        if (nav->controls.capturing) { memcpy(hints, capture, sizeof capture); return 1; }
        memcpy(hints, controls, sizeof controls);
        return 3;
    }
    /* Display's Advanced, Picture or Text highlighted opens its rows: nothing there changes with left and right. */
    if (nav->screen == ORACLES_SCREEN_DISPLAY && !nav->advanced && !nav->text_page && !nav->picture_page
        && (nav->row == ORACLES_DISPLAY_ADVANCED || nav->row == ORACLES_DISPLAY_TEXT || nav->row == ORACLES_DISPLAY_PICTURE)) {
        static const OraclesHomeHint advanced[3] = {
            { UP_DOWN, "Move", 0, 0 },
            { "Enter / A", "Open", 0, 0 },
            { "Esc / B", "Back", 1, 0 },
        };
        memcpy(hints, advanced, sizeof advanced);
        return 3;
    }
    if (nav->screen != ORACLES_SCREEN_HOME) {
        static const OraclesHomeHint page[4] = {
            { UP_DOWN, "Move", 0, 0 },
            { LEFT_RIGHT, "Change", 0, 0 },
            { "Enter / A", "Select", 0, 0 },
            { "Esc / B", "Back", 1, 0 },
        };
        memcpy(hints, page, sizeof page);
        return 4;
    }
    static const OraclesHomeHint base[4] = {
        { LEFT_RIGHT, "Game", 0, 0 },
        { UP_DOWN, "Menu", 0, 0 },
        { "Enter / A", "Select", 0, 0 },
        { "Esc / B", "Fan games", 1, 0 },
    };
    memcpy(hints, base, sizeof base);
    if (oracles_home_update_focused(nav) || oracles_home_version_focused(nav)) hints[2].label = "Open";   /* the newer version's page, About */
    return nav->entry != ORACLES_HOME_FAN || nav->fan_pick == ORACLES_HOME_HERO_FAN ? 3 : 4;
}

/* A key of the hints as `device` names it: a controller's D-pad, A and B, and for Escape alone the menu's button (Guide
 * without one); the keyboard's Enter and Esc; both devices' as written for a pointer. */
static const char *device_key(const OraclesHomeNav *nav, const char *key, OraclesInputDevice device)
{
    static const char *const both[5] = { UP_DOWN, LEFT_RIGHT, UP_DOWN LEFT_RIGHT, "Enter / A", "Esc / B" };
    static const char *const gamepad[5] = { "D-pad " UP_DOWN, "D-pad " LEFT_RIGHT, "D-pad", "A", "B" };
    static const char *const keyboard[5] = { UP_DOWN, LEFT_RIGHT, UP_DOWN LEFT_RIGHT, "Enter", "Esc" };
    if (device == ORACLES_INPUT_POINTER) return key;
    if (!strcmp(key, "Esc"))
        return device == ORACLES_INPUT_KEYBOARD ? key : nav->controls.menu_pad[0] ? oracles_controls_button_label(nav->controls.menu_pad) : "Guide";
    for (int i = 0; i < 5; i++) if (!strcmp(key, both[i])) return device == ORACLES_INPUT_GAMEPAD ? gamepad[i] : keyboard[i];
    return key;
}

/* A UTF-8 text's characters. */
static int characters(const char *s)
{
    int n = 0;
    for (; *s; s++) n += ((unsigned char)*s & 0xc0u) != 0x80u;
    return n;
}

unsigned oracles_home_hints(const OraclesHomeNav *nav, OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS])
{
    const unsigned count = hints_of(nav, hints);
    for (unsigned i = 0; i < count; i++) {
        /* The keyboard's keys are never wider than both devices': the frame holds the controller's too. */
        const int both = characters(hints[i].key), gamepad = characters(device_key(nav, hints[i].key, ORACLES_INPUT_GAMEPAD));
        hints[i].chars = both > gamepad ? both : gamepad;
        hints[i].key = device_key(nav, hints[i].key, nav->device);
    }
    return count;
}

int oracles_home_keys_shown(const OraclesHomeNav *nav)
{
    return nav->keyboard_used || (!nav->android && nav->device != ORACLES_INPUT_GAMEPAD);
}

int oracles_home_input(OraclesHomeNav *nav, OraclesInputDevice device)
{
    const int keyboard = device == ORACLES_INPUT_KEYBOARD;
    if (nav->device == device && (!keyboard || nav->keyboard_used)) return 0;
    nav->device = device;
    if (keyboard) nav->keyboard_used = 1;
    return 1;
}

int oracles_home_slow_text(const OraclesHomeNav *nav, char *text, size_t capacity, const char **path)
{
    const int quality = oracles_display_quality(nav), view = nav->display.view;
    if (quality == ORACLES_DISPLAY_QUALITY_CUSTOM) {
        if (view < 1 || view > 2) return 0;
        snprintf(text, capacity, "The game ran slowly in the %s view: the %s view may play smoother.", oracles_display_view_names[view],
                 oracles_display_view_names[view - 1]);
        *path = "Display \xe2\x80\xba View";
        return 1;
    }
    if (quality < 1) return 0;
    snprintf(text, capacity, "The game ran slowly at %s: %s may play smoother.", oracles_display_quality_names[quality],
             oracles_display_quality_names[quality - 1]);
    *path = "Display \xe2\x80\xba Quality";
    return 1;
}

void oracles_home_slow_open(OraclesHomeNav *nav)
{
    nav->slow = 0;
    nav->screen = ORACLES_SCREEN_DISPLAY;
    nav->advanced = nav->text_page = nav->picture_page = 0;
    nav->row = oracles_display_quality(nav) == ORACLES_DISPLAY_QUALITY_CUSTOM ? ORACLES_DISPLAY_VIEW : ORACLES_DISPLAY_QUALITY;
}

void oracles_home_select(OraclesHomeNav *nav, OraclesHomeEntry entry)
{
    nav->slow = 0;
    if (nav->entry == entry) return;
    nav->entry = entry;
    nav->fan_pick = ORACLES_HOME_HERO_FAN;
    nav->focus = 0;
}

void oracles_home_hover(OraclesHomeNav *nav, unsigned item)
{
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    const unsigned count = oracles_home_items(nav, items);
    if (item < count + extra_places(nav)) nav->focus = item;
}

static OraclesHomeCommand activate(OraclesHomeNav *nav, const OraclesHomeItem *it)
{
    if (it->disabled) return ORACLES_HOME_STAY;
    switch (it->id) {
        case ORACLES_HOME_ITEM_START: return oracles_home_start_command(oracles_home_game(nav));
        case ORACLES_HOME_ITEM_EXIT: return ORACLES_HOME_EXIT;
        case ORACLES_HOME_ITEM_GAME: nav->screen = ORACLES_SCREEN_GAME; nav->row = 0; nav->eu_action = 0; return ORACLES_HOME_STAY;
        case ORACLES_HOME_ITEM_DISPLAY:
            nav->screen = ORACLES_SCREEN_DISPLAY;
            nav->row = 0;
            nav->advanced = nav->text_page = nav->picture_page = 0;
            return ORACLES_HOME_STAY;
        case ORACLES_HOME_ITEM_CONTROLS: oracles_controls_open(nav); return ORACLES_HOME_STAY;
        case ORACLES_HOME_ITEM_MODS: nav->screen = ORACLES_SCREEN_MODS; nav->row = 0; return ORACLES_HOME_STAY;
        case ORACLES_HOME_ITEM_CHEATS: nav->screen = ORACLES_SCREEN_CHEATS; nav->row = 0; return ORACLES_HOME_STAY;
        case ORACLES_HOME_ITEM_RESUME: return ORACLES_HOME_RESUME;
        case ORACLES_HOME_ITEM_SAVE_STATE: return ORACLES_HOME_SAVE_STATE;
        case ORACLES_HOME_ITEM_LOAD_STATE: return ORACLES_HOME_LOAD_STATE;
        case ORACLES_HOME_ITEM_QUIT_GAME: return ORACLES_HOME_QUIT_GAME;
        default:
            for (unsigned f = 0; f < ORACLES_HOME_FAN_GAMES; f++)
                if (oracles_home_fan_games[f].item == it->id) { nav->fan_pick = oracles_home_fan_games[f].hero; nav->focus = 0; }
            return ORACLES_HOME_STAY;
    }
}

OraclesHomeCommand oracles_home_click(OraclesHomeNav *nav, unsigned item)
{
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    nav->slow = 0;
    const unsigned count = oracles_home_items(nav, items);
    if (item >= count && item < count + extra_places(nav)) {
        nav->focus = item;
        return oracles_home_update_focused(nav) ? ORACLES_HOME_OPEN_UPDATE : open_about(nav);
    }
    if (item >= count) return ORACLES_HOME_STAY;
    nav->focus = item;
    return activate(nav, &items[item]);
}

int oracles_home_game(const OraclesHomeNav *nav) { return hero_game(oracles_home_hero(nav)); }

int oracles_home_refuses(const OraclesHomeNav *nav, unsigned feature)
{
    const int g = oracles_home_game(nav);
    return g >= 0 && (nav->games[g].refuses & feature) != 0;
}

OraclesHomeCommand oracles_home_start_command(int game)
{
    const OraclesHomeFanGame *fan = oracles_home_fan_game(game);
    return fan ? fan->start : game == ORACLES_HOME_GAME_SEASONS ? ORACLES_HOME_START_SEASONS : ORACLES_HOME_START_AGES;
}

OraclesHomeCommand oracles_home_act(OraclesHomeNav *nav, OraclesHomeAction action)
{
    /* The slow game's toast goes with the first input; it changes no setting. */
    nav->slow = 0;
    if (nav->screen == ORACLES_SCREEN_GAME) return oracles_page_act(nav, action);
    if (nav->screen == ORACLES_SCREEN_DISPLAY) return oracles_display_act(nav, action);
    if (nav->screen == ORACLES_SCREEN_CONTROLS) return oracles_controls_act(nav, action);
    if (nav->screen == ORACLES_SCREEN_MODS) return oracles_mods_act(nav, action);
    if (nav->screen == ORACLES_SCREEN_CHEATS) return oracles_cheats_act(nav, action);
    if (nav->screen == ORACLES_SCREEN_ABOUT) return oracles_about_act(nav, action);
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    const unsigned count = oracles_home_items(nav, items), focus = oracles_home_focus(nav);
    /* The newer version's notice, when shown, and the version number are places after the last item. */
    const unsigned places = count + extra_places(nav);
    if (nav->screen == ORACLES_SCREEN_PAUSE && (action == ORACLES_HOME_LEFT || action == ORACLES_HOME_RIGHT)) return ORACLES_HOME_STAY;
    if (nav->screen == ORACLES_SCREEN_PAUSE && action == ORACLES_HOME_BACK) return ORACLES_HOME_RESUME;   /* Escape, B, the menu button or Guide resumes */
    switch (action) {
        case ORACLES_HOME_LEFT:
        case ORACLES_HOME_RIGHT: {
            const int step = action == ORACLES_HOME_LEFT ? ORACLES_HOME_ENTRIES - 1 : 1;
            oracles_home_select(nav, (OraclesHomeEntry)(((int)nav->entry + step) % ORACLES_HOME_ENTRIES));
            return ORACLES_HOME_STAY;
        }
        case ORACLES_HOME_UP: nav->focus = (focus + places - 1u) % places; return ORACLES_HOME_STAY;
        case ORACLES_HOME_DOWN: nav->focus = (focus + 1u) % places; return ORACLES_HOME_STAY;
        case ORACLES_HOME_OK: return focus < count ? activate(nav, &items[focus]) : oracles_home_update_focused(nav) ? ORACLES_HOME_OPEN_UPDATE : open_about(nav);
        case ORACLES_HOME_BACK:
            /* Back from a fan game to the list, its line highlighted; nothing to go back to elsewhere. */
            if (nav->entry == ORACLES_HOME_FAN && nav->fan_pick != ORACLES_HOME_HERO_FAN) {
                nav->focus = 0;
                nav->fan_pick = ORACLES_HOME_HERO_FAN;
            }
            return ORACLES_HOME_STAY;
    }
    return ORACLES_HOME_STAY;
}
