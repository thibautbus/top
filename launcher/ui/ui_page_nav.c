#include "ui_page_nav.h"

#include <stdio.h>
#include <string.h>

static int page_game(const OraclesHomeNav *nav)
{
    const int game = oracles_home_game(nav);
    return game >= 0 ? game : ORACLES_HOME_GAME_AGES;
}

OraclesHomeGame *oracles_page_game(OraclesHomeNav *nav) { return &nav->games[page_game(nav)]; }
const OraclesHomeGame *oracles_page_game_const(const OraclesHomeNav *nav) { return &nav->games[page_game(nav)]; }

int oracles_page_profile_allowed(const OraclesHomeGame *game, OraclesProfile profile)
{
    /* A fan game's image decides, as a ROM does: the one its patch is made to give plays in every profile. */
    const OraclesRomState state = game->fan ? game->image : game->rom;
    if (state == ORACLES_ROM_ORIGINAL) return 1;
    return state == ORACLES_ROM_UNRECOGNISED && profile == ORACLES_PROFILE_FAITHFUL;
}

int oracles_page_profile(const OraclesHomeGame *game, OraclesProfile chosen)
{
    if (oracles_page_profile_allowed(game, chosen)) return (int)chosen;
    for (int p = 0; p < ORACLES_PROFILES; p++) if (oracles_page_profile_allowed(game, (OraclesProfile)p)) return p;
    return -1;
}

const char *oracles_page_rom_file(const OraclesHomeGame *game) { return game->rom_file[0] ? game->rom_file : "None"; }

void oracles_page_rom_status(const OraclesHomeGame *game, char *out, size_t capacity, OraclesTone *tone)
{
    switch (game->rom) {
        case ORACLES_ROM_ORIGINAL:
            if (game->fan) snprintf(out, capacity, "Original ROM, %s", game->fan->base_name);
            else snprintf(out, capacity, "Original ROM, all profiles");
            *tone = ORACLES_TONE_OK;
            break;
        case ORACLES_ROM_UNRECOGNISED:
            /* A base that is not the original: the patch, made for it, is refused on it. */
            if (game->fan) snprintf(out, capacity, "Unrecognized ROM, not %s", game->fan->base_name);
            else snprintf(out, capacity, "Unrecognized ROM, Faithful only");
            *tone = ORACLES_TONE_WARN;
            break;
        case ORACLES_ROM_REFUSED: snprintf(out, capacity, "Refused: %s.", game->rom_reason); *tone = ORACLES_TONE_ERROR; break;
        default: snprintf(out, capacity, game->fan ? "No base ROM chosen" : "No ROM chosen"); *tone = ORACLES_TONE_NONE; break;
    }
}

const char *oracles_page_patch_file(const OraclesHomeGame *game) { return game->patch_file[0] ? game->patch_file : "None"; }

void oracles_page_patch_status(const OraclesHomeGame *game, char *out, size_t capacity, OraclesTone *tone)
{
    switch (game->patch) {
        case ORACLES_ROM_NONE:   /* the player sees which patch the game expects before choosing one */
            snprintf(out, capacity, "No patch: the BPS patch of %s is expected", game->fan ? game->fan->image_name : "the game");
            *tone = ORACLES_TONE_NONE;
            break;
        case ORACLES_ROM_REFUSED: snprintf(out, capacity, "Refused: %s.", game->patch_reason); *tone = ORACLES_TONE_ERROR; break;
        default: snprintf(out, capacity, "Patch file read"); *tone = ORACLES_TONE_OK; break;
    }
}

void oracles_page_image_status(const OraclesHomeGame *game, char *out, size_t capacity, OraclesTone *tone)
{
    switch (game->image) {
        case ORACLES_ROM_ORIGINAL: snprintf(out, capacity, "Patched image recognized: %s", game->fan ? game->fan->image_name : "?"); *tone = ORACLES_TONE_OK; break;
        case ORACLES_ROM_UNRECOGNISED: snprintf(out, capacity, "Patched image not recognized, Faithful only"); *tone = ORACLES_TONE_WARN; break;
        case ORACLES_ROM_REFUSED: snprintf(out, capacity, "Refused: %s.", game->image_reason); *tone = ORACLES_TONE_ERROR; break;
        default: snprintf(out, capacity, "The game needs both files."); *tone = ORACLES_TONE_NONE; break;
    }
}

const char *oracles_page_rom_note(const OraclesHomeNav *nav)
{
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    /* A fan game's image line says Faithful only already. */
    if (!game->usable || game->fan || nav->display.profile != ORACLES_PROFILE_ENHANCED || oracles_page_profile_allowed(game, ORACLES_PROFILE_ENHANCED)) return "";
    return "Enhanced needs an original ROM: this one plays in Faithful";
}

const char *oracles_page_rom_hotkeys_note(const OraclesHomeNav *nav)
{
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    if (!game->usable || !game->hotkeys || !game->hotkeys_failed) return "";
    return "Item hotkeys need an original ROM: this one plays without them";
}

/* What follows "<id>/files" in a path of the application's storage, `id` its first character; NULL when it is not one. */
static const char *after_files(const char *id)
{
    const char *slash = strchr(id, '/');
    if (!slash || slash == id || strncmp(slash + 1, "files", 5) || (slash[6] && slash[6] != '/')) return NULL;
    return slash + 6;
}

void oracles_page_folder(int android, const char *folder, char *out, size_t capacity)
{
    const char *rest = NULL;
    const char *shared = android ? strstr(folder, "/Android/data/") : NULL;
    if (shared) { snprintf(out, capacity, "%s", shared + 1); return; }
    if (android && !strncmp(folder, "/data/data/", 11)) {
        rest = after_files(folder + 11);
    } else if (android && !strncmp(folder, "/data/user/", 11) && strchr(folder + 11, '/')) {
        rest = after_files(strchr(folder + 11, '/') + 1);
    }
    if (!rest) { snprintf(out, capacity, "%s", folder); return; }
    size_t length = (size_t)snprintf(out, capacity, "%s", "App storage");
    while (*rest == '/' && length < capacity) {
        const size_t part = strcspn(rest + 1, "/");
        if (part) length += (size_t)snprintf(out + length, capacity - length, " \xe2\x80\xba %.*s", (int)part, rest + 1);
        rest += part + 1;
    }
}

const char *oracles_page_save_file(const OraclesHomeGame *game) { return game->save_file[0] ? game->save_file : "No save yet"; }

void oracles_page_save_line(const OraclesHomeGame *game, char *out, size_t capacity)
{
    if (game->save_file[0]) snprintf(out, capacity, "Last written %s \xc2\xb7 savestates (.state) sit next to it", game->save_written);
    else if (game->usable) snprintf(out, capacity, "Created next to the %s on first save \xc2\xb7 savestates (.state) too", game->fan ? "patch" : "ROM");
    else snprintf(out, capacity, "%s", game->fan ? "Choose both files first" : "Choose a ROM first");
}

const char *oracles_page_save_action(const OraclesHomeNav *nav, int action)
{
    if (action == 1) return "Import save\xe2\x80\xa6";
    return nav->android ? "Export save\xe2\x80\xa6" : "Open folder";
}

int oracles_page_save_action_live(const OraclesHomeNav *nav, int action)
{
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    /* A running game holds its save: no import under it. */
    if (action == 1) return game->usable && !nav->in_game;
    return nav->android ? game->save_file[0] != 0 : game->usable || game->save_file[0];
}

const char *oracles_page_save_note(const OraclesHomeNav *nav, int four_three)
{
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    if (nav->save_flow.state == ORACLES_SAVE_FLOW_DONE || nav->save_flow.state == ORACLES_SAVE_FLOW_REFUSED) return "";
    const int import = nav->row == ORACLES_ROW_SAVE && nav->save_action == 1;
    if (nav->in_game && import) return oracles_display_pause_reason;
    if (game->fan) return "";
    /* Android's two actions each have their own: Import save… says what it takes, Export save… where the save goes. */
    if (nav->android && import)
        return four_three ? "Import a save from elsewhere, the European ROM's too."
                          : "Import a save from another emulator or another device, also one made with the European ROM: its language is then "
                            "set to English for this game.";
    if (nav->android)
        return four_three ? "Export a copy to play it elsewhere, also with the European ROM."
                          : "Export a copy to play it elsewhere: this save also works with the European ROM in other emulators, beside that ROM "
                            "under the same name.";
    return four_three ? "Also works with the European ROM in other emulators, named like it."
                      : "This save also works with the European ROM in other emulators: put it beside that ROM, under the same name.";
}

const char *oracles_page_save_status(const OraclesHomeNav *nav, OraclesTone *tone, const char **second)
{
    const OraclesSaveFlow state = nav->save_flow.state;
    *second = NULL;
    if (state != ORACLES_SAVE_FLOW_DONE && state != ORACLES_SAVE_FLOW_REFUSED) return NULL;
    *tone = state == ORACLES_SAVE_FLOW_DONE ? ORACLES_TONE_OK : ORACLES_TONE_ERROR;
    if (nav->save_flow.text2[0]) *second = nav->save_flow.text2;
    return nav->save_flow.text;
}

void oracles_page_import_done(unsigned kept, int fan, int english, char *first, char *second)
{
    const int save = (kept & ORACLES_PAGE_KEPT_SAVE) != 0, state = (kept & ORACLES_PAGE_KEPT_STATE) != 0;
    const int mods_save = (kept & ORACLES_PAGE_KEPT_MODS_SAVE) != 0, mods_state = (kept & ORACLES_PAGE_KEPT_MODS_STATE) != 0;
    const int mods_store = (kept & ORACLES_PAGE_KEPT_MODS_STORE) != 0;
    snprintf(first, ORACLES_HOME_TEXT_LENGTH, "Save imported.%s", english ? " Its language was set to English for this game." : "");
    second[0] = 0;
    /* The save's files, then the game with mods': one part when both hold the same ("with mods too"). */
    const char *own = save && state ? "the old save and its savestate" : save ? "the old save" : state ? "the old savestate" : NULL;
    const char *mods = mods_save ? (mods_state && mods_store ? "the save with mods, its savestate and data"
                                    : mods_state            ? "the save with mods and its savestate"
                                    : mods_store            ? "the save with mods and its data"
                                                            : "the save with mods")
                     : mods_state && mods_store ? "the mods' savestate and data"
                     : mods_state               ? "the savestate with mods"
                     : mods_store               ? "the mods' data"
                                                : NULL;
    if (!own && !mods) return;
    char parts[160];
    if (own && save == mods_save && state == mods_state)
        snprintf(parts, sizeof parts, "%s, with mods too%s", own, mods_store ? ", and the mods' data" : "");
    else if (own && mods)
        snprintf(parts, sizeof parts, "%s%s%s", own, strstr(own, " and ") || strstr(mods, " and ") || strchr(mods, ',') ? ", and " : " and ", mods);
    else if (own)
        snprintf(parts, sizeof parts, "%s", own);
    else
        snprintf(parts, sizeof parts, "the old %s", mods + 4);   /* "the old save with mods" */
    const char *where = mods ? "" : fan ? ", beside the patch" : ", beside the ROM";
    snprintf(second, ORACLES_HOME_TEXT_LENGTH, "In Backups%s: %s.", where, parts);
}

const char *oracles_page_play_note(const OraclesHomeNav *nav)
{
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    if (nav->row != ORACLES_ROW_PLAY || game->usable) return "";
    return oracles_home_fan_game(page_game(nav)) ? "Choose both files first" : "Choose a ROM first";
}

int oracles_page_eu_shown(const OraclesHomeGame *game) { return !game->fan && game->rom == ORACLES_ROM_ORIGINAL; }

int oracles_page_language_shown(const OraclesHomeNav *nav, const OraclesHomeGame *game)
{
    return game->fan ? nav->fan_languages : oracles_page_eu_shown(game);
}

const char *const oracles_page_language_names[ORACLES_PAGE_LANGUAGES] = {
    "English (US)", "English (EU)", "Fran\xc3\xa7" "ais", "Deutsch", "Italiano", "Espa\xc3\xb1" "ol"
};

int oracles_page_language_available(const OraclesHomeGame *game, int language)
{
    const int european = game->eu == ORACLES_ROM_ORIGINAL && (!game->fan || game->rom == ORACLES_ROM_ORIGINAL);
    return language == 0 || (language > 0 && language < ORACLES_PAGE_LANGUAGES && european);
}

/* The language chosen, when the game has it; its row shown or not. */
static int chosen_language(const OraclesHomeGame *game) { return oracles_page_language_available(game, game->language) ? game->language : 0; }

int oracles_page_language(const OraclesHomeNav *nav, const OraclesHomeGame *game)
{
    return oracles_page_language_shown(nav, game) ? chosen_language(game) : 0;
}

const char *oracles_page_eu_file(const OraclesHomeGame *game) { return game->eu != ORACLES_ROM_NONE && game->eu_file[0] ? game->eu_file : "None"; }

void oracles_page_eu_status(const OraclesHomeNav *nav, char *out, size_t capacity, OraclesTone *tone)
{
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    switch (game->eu) {
        case ORACLES_ROM_ORIGINAL: {
            size_t used = (size_t)snprintf(out, capacity, "Recognized: ");
            for (int l = 1; l < ORACLES_PAGE_LANGUAGES && used < capacity; l++)
                used += (size_t)snprintf(out + used, capacity - used, "%s%s", l > 1 ? ", " : "", oracles_page_language_names[l]);
            *tone = ORACLES_TONE_OK;
            break;
        }
        case ORACLES_ROM_UNRECOGNISED:
            snprintf(out, capacity, "Not the European ROM of Oracle of %s", page_game(nav) == ORACLES_HOME_GAME_SEASONS ? "Seasons" : "Ages");
            *tone = ORACLES_TONE_ERROR;
            break;
        case ORACLES_ROM_REFUSED: snprintf(out, capacity, "Refused: %s.", game->eu_reason); *tone = ORACLES_TONE_ERROR; break;
        default: {
            size_t used = (size_t)snprintf(out, capacity, "Optional: ");
            for (int l = 1; l < ORACLES_PAGE_LANGUAGES && used < capacity; l++)
                used += (size_t)snprintf(out + used, capacity - used, "%s%s", l > 1 ? ", " : "", oracles_page_language_names[l]);
            *tone = ORACLES_TONE_NONE;
            break;
        }
    }
}

unsigned oracles_page_eu_buttons(const OraclesHomeGame *game, const char *labels[2])
{
    if (game->eu == ORACLES_ROM_NONE) {
        labels[0] = "Choose\xe2\x80\xa6";
        labels[1] = NULL;
        return 1;
    }
    labels[0] = "Change\xe2\x80\xa6";
    labels[1] = "Remove";
    return 2;
}

const char *oracles_page_language_explanation(const OraclesHomeGame *game)
{
    if (game->fan) {
        const int ages = game->fan->base == ORACLES_HOME_GAME_AGES;
        if (!oracles_page_language_available(game, 1))
            return ages ? "Choose the European ROM of Oracle of Ages in its Cartridge to play it in another language."
                        : "Choose the European ROM of Oracle of Seasons in its Cartridge to play it in another language.";
        if (!chosen_language(game)) return "The original text of your fan game.";
        return ages ? "Texts the fan game shares with Oracle of Ages, from its European edition."
                    : "Texts the fan game shares with Oracle of Seasons, from its European edition.";
    }
    return chosen_language(game) ? "The European edition's text, from your European ROM." : "The original text of your US ROM.";
}

void oracles_page_language_note(const OraclesHomeGame *game, char *out, size_t capacity)
{
    if (!chosen_language(game)) snprintf(out, capacity, "%s", "");
    else if (!game->fan) snprintf(out, capacity, "Where the official translation was wrong, the port corrects it.");
    else
        snprintf(out, capacity, "Only the texts it shares with Oracle of %s are translated; its own dialogues stay in English.",
                 game->fan->base == ORACLES_HOME_GAME_AGES ? "Ages" : "Seasons");
}

/* The page has the row: the Patch row is a fan game's only, European ROM an Oracle's with its original ROM, Language
 * that Oracle's and a fan game's. */
static int has_row(const OraclesHomeNav *nav, unsigned row)
{
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    if (row == ORACLES_ROW_EU_ROM) return oracles_page_eu_shown(game);
    if (row == ORACLES_ROW_LANGUAGE) return oracles_page_language_shown(nav, game);
    return row < ORACLES_GAME_ROWS && (row != ORACLES_ROW_PATCH || game->fan);
}

/* The European ROM gone: no file, English (US), for the Oracle and the fan games made on it; the host forgets them
 * (ORACLES_HOME_STORE). */
static OraclesHomeCommand remove_eu(OraclesHomeNav *nav)
{
    OraclesHomeGame *game = oracles_page_game(nav);
    game->eu = ORACLES_ROM_NONE;
    game->eu_file[0] = game->eu_folder[0] = game->eu_reason[0] = 0;
    game->language = 0;
    for (unsigned f = 0; f < ORACLES_HOME_FAN_GAMES; f++)
        if (&nav->games[oracles_home_fan_games[f].base] == game) nav->games[oracles_home_fan_games[f].game].eu = ORACLES_ROM_NONE;
    nav->eu_action = 0;
    return ORACLES_HOME_STORE;
}

/* The language `language`, when the game has it. */
static OraclesHomeCommand language_set(OraclesHomeNav *nav, int language)
{
    OraclesHomeGame *game = oracles_page_game(nav);
    if (!oracles_page_language_available(game, language) || language == game->language) return ORACLES_HOME_STAY;
    game->language = language;
    return ORACLES_HOME_STORE;
}

/* The next language (`by` 1) or the one before (-1), wrapping; English alone changes nothing. */
static OraclesHomeCommand language_step(OraclesHomeNav *nav, int by)
{
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    if (!oracles_page_language_available(game, 1)) return ORACLES_HOME_STAY;
    return language_set(nav, ((game->language + by) % ORACLES_PAGE_LANGUAGES + ORACLES_PAGE_LANGUAGES) % ORACLES_PAGE_LANGUAGES);
}

static OraclesHomeCommand activate(OraclesHomeNav *nav, unsigned row)
{
    const OraclesHomeGame *game = oracles_page_game(nav);
    switch (row) {
        case ORACLES_ROW_ROM: return ORACLES_HOME_CHOOSE_ROM;
        case ORACLES_ROW_PATCH: return ORACLES_HOME_CHOOSE_PATCH;
        case ORACLES_ROW_SAVE:
            if (!oracles_page_save_action_live(nav, nav->save_action)) return ORACLES_HOME_STAY;
            return nav->save_action == 1 ? ORACLES_HOME_IMPORT_SAVE : nav->android ? ORACLES_HOME_EXPORT_SAVE : ORACLES_HOME_OPEN_FOLDER;
        case ORACLES_ROW_EU_ROM: return game->eu != ORACLES_ROM_NONE && nav->eu_action == 1 ? remove_eu(nav) : ORACLES_HOME_CHOOSE_EU_ROM;
        case ORACLES_ROW_LANGUAGE: return language_step(nav, 1);
        case ORACLES_ROW_PLAY: return game->usable ? oracles_home_start_command(page_game(nav)) : ORACLES_HOME_STAY;
        default: return ORACLES_HOME_STAY;
    }
}

/* The next row of the page from `row`, one step up or down, wrapping. */
static unsigned step(const OraclesHomeNav *nav, unsigned row, unsigned by)
{
    do row = (row + by) % ORACLES_GAME_ROWS; while (!has_row(nav, row));
    return row;
}

/* The import's confirmation, open over the page: left and right move between Replace and Cancel, OK takes one, Back
 * cancels; nothing else acts. */
static OraclesHomeCommand confirm_act(OraclesHomeNav *nav, OraclesHomeAction action)
{
    OraclesHomeSaveFlow *flow = &nav->save_flow;
    if (action == ORACLES_HOME_LEFT || action == ORACLES_HOME_RIGHT) flow->choice = action == ORACLES_HOME_RIGHT;
    else if (action == ORACLES_HOME_OK && flow->choice == 0) return ORACLES_HOME_REPLACE_SAVE;
    else if (action == ORACLES_HOME_OK || action == ORACLES_HOME_BACK) flow->state = ORACLES_SAVE_FLOW_NONE;
    return ORACLES_HOME_STAY;
}

OraclesHomeCommand oracles_page_act(OraclesHomeNav *nav, OraclesHomeAction action)
{
    if (nav->save_flow.state == ORACLES_SAVE_FLOW_CONFIRM) return confirm_act(nav, action);
    const unsigned row = has_row(nav, nav->row) ? nav->row : 0;
    switch (action) {
        case ORACLES_HOME_UP: nav->row = step(nav, row, ORACLES_GAME_ROWS - 1u); return ORACLES_HOME_STAY;
        case ORACLES_HOME_DOWN: nav->row = step(nav, row, 1u); return ORACLES_HOME_STAY;
        case ORACLES_HOME_LEFT:
        case ORACLES_HOME_RIGHT:
            /* European ROM: between Change and Remove, once there is a file; Language: the language before or after. */
            if (row == ORACLES_ROW_EU_ROM && oracles_page_game_const(nav)->eu != ORACLES_ROM_NONE) nav->eu_action = action == ORACLES_HOME_RIGHT;
            if (row == ORACLES_ROW_SAVE) nav->save_action = action == ORACLES_HOME_RIGHT;
            if (row == ORACLES_ROW_LANGUAGE) return language_step(nav, action == ORACLES_HOME_RIGHT ? 1 : -1);
            return ORACLES_HOME_STAY;
        case ORACLES_HOME_OK: return activate(nav, row);
        case ORACLES_HOME_BACK:
            /* Back to the home screen, on the item that opened the page; the Save row's status goes with it. */
            nav->save_flow.state = ORACLES_SAVE_FLOW_NONE;
            nav->screen = ORACLES_SCREEN_HOME;
            nav->focus = 1;
            return ORACLES_HOME_STAY;
    }
    return ORACLES_HOME_STAY;
}

void oracles_page_keep_row(OraclesHomeNav *nav)
{
    if (nav->screen != ORACLES_SCREEN_GAME || has_row(nav, nav->row)) return;
    for (unsigned d = 1; d < ORACLES_GAME_ROWS; d++) {
        if (nav->row >= d && has_row(nav, nav->row - d)) { nav->row -= d; return; }
        if (nav->row + d < ORACLES_GAME_ROWS && has_row(nav, nav->row + d)) { nav->row += d; return; }
    }
    nav->row = ORACLES_ROW_ROM;
}

void oracles_page_hover(OraclesHomeNav *nav, unsigned row, int option)
{
    /* The confirmation open: its buttons alone take the highlight. */
    if (nav->save_flow.state == ORACLES_SAVE_FLOW_CONFIRM) {
        if (row == ORACLES_ROW_SAVE && option >= 2) nav->save_flow.choice = option - 2;
        return;
    }
    if (!has_row(nav, row)) return;
    nav->row = row;
    /* An action of Save, or a button of European ROM with a file, under the pointer is the one OK takes. */
    if (row == ORACLES_ROW_SAVE && option >= 0) nav->save_action = option == 1;
    if (row == ORACLES_ROW_EU_ROM && option >= 0 && oracles_page_game_const(nav)->eu != ORACLES_ROM_NONE) nav->eu_action = option == 1;
}

OraclesHomeCommand oracles_page_click(OraclesHomeNav *nav, unsigned row, int option)
{
    /* The confirmation open: its buttons alone act. */
    if (nav->save_flow.state == ORACLES_SAVE_FLOW_CONFIRM) {
        if (row != ORACLES_ROW_SAVE || option < 2) return ORACLES_HOME_STAY;
        nav->save_flow.choice = option - 2;
        return confirm_act(nav, ORACLES_HOME_OK);
    }
    if (!has_row(nav, row)) return ORACLES_HOME_STAY;
    nav->row = row;
    /* A language clicked is chosen; the row itself only takes the highlight. */
    if (row == ORACLES_ROW_LANGUAGE) return option >= 0 ? language_set(nav, option) : ORACLES_HOME_STAY;
    /* European ROM: Remove clicked removes the file; elsewhere on the row, the dialog opens. */
    if (row == ORACLES_ROW_EU_ROM) nav->eu_action = option == 1 && oracles_page_game_const(nav)->eu != ORACLES_ROM_NONE;
    /* Save: an action clicked acts; elsewhere on the row, the one left and right chose. */
    if (row == ORACLES_ROW_SAVE && option >= 0) nav->save_action = option == 1;
    return activate(nav, row);
}
