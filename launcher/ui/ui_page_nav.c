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
            else snprintf(out, capacity, game->combined ? "Oracles Combined image, Faithful only; see Fan games" : "Unrecognized ROM, Faithful only");
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
            snprintf(out, capacity, "No patch: the BPS patch of %s is expected",
                     !game->fan ? "the game" : game->fan->patch_expected ? game->fan->patch_expected : game->fan->image_name);
            *tone = ORACLES_TONE_NONE;
            break;
        case ORACLES_ROM_REFUSED: snprintf(out, capacity, "Refused: %s.", game->patch_reason); *tone = ORACLES_TONE_ERROR; break;
        default: snprintf(out, capacity, "Patch file read"); *tone = ORACLES_TONE_OK; break;
    }
}

/* An image's language, from the ROM's tables, in its own name as the Language rows write it. */
static const char *own_language_name(const char *language)
{
    static const char *const names[][2] = {
        { "English", "English" }, { "French", "Fran\xc3\xa7" "ais" }, { "German", "Deutsch" }, { "Italian", "Italiano" }, { "Spanish", "Espa\xc3\xb1" "ol" }
    };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) if (!strcmp(language, names[i][0])) return names[i][1];
    return language;
}

void oracles_page_image_status(const OraclesHomeGame *game, char *out, size_t capacity, OraclesTone *tone)
{
    switch (game->image) {
        case ORACLES_ROM_ORIGINAL:
            snprintf(out, capacity, "Patched image recognized: %s%s%s", game->fan ? game->fan->image_name : "?", game->image_language ? " \xc2\xb7 " : "",
                     game->image_language ? own_language_name(game->image_language) : "");
            *tone = ORACLES_TONE_OK;
            break;
        case ORACLES_ROM_UNRECOGNISED: snprintf(out, capacity, "Patched image not recognized, Faithful only"); *tone = ORACLES_TONE_WARN; break;
        case ORACLES_ROM_REFUSED: snprintf(out, capacity, "Refused: %s.", game->image_reason); *tone = ORACLES_TONE_ERROR; break;
        default:
            snprintf(out, capacity, "%s", game->fan && game->fan->joined ? "The game needs both European ROMs and the patch." : "The game needs both files.");
            *tone = ORACLES_TONE_NONE;
            break;
    }
}

unsigned oracles_page_image_notes(const OraclesHomeGame *game, int four_three, char out[ORACLES_PAGE_IMAGE_NOTES][ORACLES_HOME_TEXT_LENGTH])
{
    if (!game->fan || !game->fan->joined) return 0;
    snprintf(out[0], ORACLES_HOME_TEXT_LENGTH, "%s",
             four_three ? "The language comes with the patch." : "The language comes with the patch: choose another patch to change it.");
    return 1;
}

static const char *const joined_names[2] = { "Ages", "Seasons" };

const char *oracles_page_joined_label(int half) { return half ? "Seasons ROM (EU)" : "Ages ROM (EU)"; }

const char *oracles_page_joined_file(const OraclesHomeGame *game, int half) { return game->joined[half].file[0] ? game->joined[half].file : "None"; }

int oracles_page_joined_set(const OraclesHomeGame *game, int half) { return game->joined[half].state == ORACLES_ROM_ORIGINAL; }

void oracles_page_joined_status(const OraclesHomeGame *game, int half, int four_three, char *out, size_t capacity, OraclesTone *tone)
{
    const OraclesHomeEuropean *e = &game->joined[half];
    const char *name = joined_names[half];
    switch (e->state) {
        case ORACLES_ROM_ORIGINAL:
            if (four_three) snprintf(out, capacity, "Recognized \xc2\xb7 set in Oracle of %s\xe2\x80\x99s Cartridge", name);
            else snprintf(out, capacity, "Recognized");
            *tone = ORACLES_TONE_OK;
            break;
        case ORACLES_ROM_UNRECOGNISED: snprintf(out, capacity, "Not the European ROM of Oracle of %s", name); *tone = ORACLES_TONE_ERROR; break;
        case ORACLES_ROM_REFUSED: snprintf(out, capacity, "Refused: %s.", e->reason); *tone = ORACLES_TONE_ERROR; break;
        default:
            snprintf(out, capacity, "Not chosen yet: Oracle of %s\xe2\x80\x99s European ROM is used", name);
            *tone = ORACLES_TONE_NONE;
            break;
    }
}

const char *oracles_page_joined_from(const OraclesHomeGame *game, int half)
{
    if (!oracles_page_joined_set(game, half)) return "";
    return half ? "Set in Oracle of Seasons\xe2\x80\x99s Cartridge" : "Set in Oracle of Ages\xe2\x80\x99s Cartridge";
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
    /* Oracles Combined's language is its patch's. */
    return game->fan ? nav->fan_languages && !game->fan->joined : oracles_page_eu_shown(game);
}

const char *const oracles_page_language_names[ORACLES_PAGE_LANGUAGES] = {
    "English (US)", "English (EU)", "Fran\xc3\xa7" "ais", "Deutsch", "Italiano", "Espa\xc3\xb1" "ol", "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e", "Polski"
};

int oracles_page_language_available(const OraclesHomeGame *game, int language)
{
    const int european = game->eu == ORACLES_ROM_ORIGINAL && (!game->fan || game->rom == ORACLES_ROM_ORIGINAL);
    if (language == ORACLES_PAGE_JAPANESE || language == ORACLES_PAGE_POLISH)
        return !game->fan && game->editions[language - ORACLES_PAGE_JAPANESE].state == ORACLES_ROM_ORIGINAL;
    return language == 0 || (language > 0 && language < ORACLES_PAGE_EUROPEAN_LANGUAGES && european);
}

int oracles_page_language_count(const OraclesHomeGame *game) { return game->fan ? ORACLES_PAGE_EUROPEAN_LANGUAGES : ORACLES_PAGE_LANGUAGES; }

int oracles_page_language_visible(const OraclesHomeGame *game, int language)
{
    if (language < 0 || language >= oracles_page_language_count(game)) return 0;
    /* A fan game's European languages on its Oracle's original ROM alone, where the European ROM is all they miss. */
    return language == 0 || !game->fan || game->rom == ORACLES_ROM_ORIGINAL;
}

int oracles_page_languages_shown(const OraclesHomeGame *game, int out[ORACLES_PAGE_LANGUAGES])
{
    int count = 0;
    for (int l = 0; l < ORACLES_PAGE_LANGUAGES; l++) if (oracles_page_language_visible(game, l)) out[count++] = l;
    return count;
}

int oracles_page_language_dimmed(const OraclesHomeGame *game)
{
    int shown[ORACLES_PAGE_LANGUAGES];
    return oracles_page_languages_shown(game, shown) < 2;
}

const char *oracles_page_language_reason(const OraclesHomeGame *game, int language)
{
    if (oracles_page_language_available(game, language)) return NULL;
    if (language == ORACLES_PAGE_JAPANESE) return "Needs your Japanese ROM: Cartridge \xe2\x80\xba Editions.";
    if (language == ORACLES_PAGE_POLISH) return "Needs your Polish ROM or patch: Cartridge \xe2\x80\xba Editions.";
    if (language <= 0 || language >= ORACLES_PAGE_EUROPEAN_LANGUAGES) return NULL;
    if (!game->fan) return "Needs your European ROM: Cartridge \xe2\x80\xba Editions.";
    return game->fan->base == ORACLES_HOME_GAME_AGES ? "Needs your European ROM of Oracle of Ages: its Cartridge \xe2\x80\xba Editions."
                                                     : "Needs your European ROM of Oracle of Seasons: its Cartridge \xe2\x80\xba Editions.";
}

int oracles_page_language_focus(const OraclesHomeNav *nav, const OraclesHomeGame *game)
{
    const int shown = nav->language_shown;
    if (nav->row == ORACLES_ROW_LANGUAGE && !nav->editions && shown >= 0 && oracles_page_language_visible(game, shown)) return shown;
    return oracles_page_language(nav, game);
}

const char *const oracles_page_edition_labels[ORACLES_PAGE_EDITIONS] = { "European ROM", "Japanese ROM", "Polish ROM or patch" };
const char oracles_page_editions_explanation[] = "Other editions of the game, and fan translations.";

OraclesRomState oracles_page_edition_state(const OraclesHomeGame *game, int edition)
{
    return edition == 0 ? game->eu : edition > 0 && edition < ORACLES_PAGE_EDITIONS ? game->editions[edition - 1].state : ORACLES_ROM_NONE;
}

void oracles_page_editions_summary(const OraclesHomeGame *game, char *out, size_t capacity)
{
    static const char *const names[ORACLES_PAGE_EDITIONS] = { "European", "Japanese", "Polish" };
    size_t used = 0;
    if (capacity) out[0] = 0;
    for (int e = 0; e < ORACLES_PAGE_EDITIONS && used < capacity; e++)
        if (oracles_page_edition_state(game, e) == ORACLES_ROM_ORIGINAL) used += (size_t)snprintf(out + used, capacity - used, "%s%s", used ? ", " : "", names[e]);
    if (!used) snprintf(out, capacity, "None");
}

const char *oracles_page_section(const OraclesHomeNav *nav) { return nav->editions ? "Cartridge \xe2\x80\xba Editions" : "Cartridge"; }

unsigned oracles_page_edition_row(int edition) { return ORACLES_ROW_EU_ROM + (unsigned)edition; }
int oracles_page_row_edition(unsigned row) { return row >= ORACLES_ROW_EU_ROM && row <= ORACLES_ROW_PL_ROM ? (int)(row - ORACLES_ROW_EU_ROM) : -1; }

const char *oracles_page_edition_file(const OraclesHomeGame *game, int edition)
{
    if (edition == 0) return oracles_page_eu_file(game);
    const OraclesHomeEdition *e = &game->editions[edition - 1];
    return e->state != ORACLES_ROM_NONE && e->file[0] ? e->file : "None";
}

const char *oracles_page_edition_folder(const OraclesHomeGame *game, int edition)
{
    if (edition == 0) return game->eu_folder;
    return game->editions[edition - 1].state != ORACLES_ROM_NONE ? game->editions[edition - 1].folder : "";
}

void oracles_page_edition_status(const OraclesHomeNav *nav, int edition, char *out, size_t capacity, OraclesTone *tone)
{
    if (edition == 0) { oracles_page_eu_status(nav, out, capacity, tone); return; }
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    const OraclesHomeEdition *e = &game->editions[edition - 1];
    const char *name = page_game(nav) == ORACLES_HOME_GAME_SEASONS ? "Seasons" : "Ages";
    const int polish = edition == 2;
    switch (e->state) {
        case ORACLES_ROM_ORIGINAL:
            if (!polish) snprintf(out, capacity, "Recognized: Oracle of %s (Japan)", name);
            else snprintf(out, capacity, "%s", e->patch == 2 ? "Kaxus's patch, from its zip, applied to your US ROM"
                                             : e->patch ? "Kaxus's patch, applied to your US ROM" : "Recognized: Kaxus's translation, already in this ROM");
            *tone = ORACLES_TONE_OK;
            break;
        case ORACLES_ROM_UNRECOGNISED:
            if (polish) snprintf(out, capacity, "Not Kaxus's Polish translation of Oracle of %s", name);
            else snprintf(out, capacity, "Not the Japanese ROM of Oracle of %s", name);
            *tone = ORACLES_TONE_ERROR;
            break;
        case ORACLES_ROM_REFUSED: snprintf(out, capacity, "Refused: %s.", e->reason); *tone = ORACLES_TONE_ERROR; break;
        default:
            snprintf(out, capacity, "%s", polish ? "Optional: Polski, Kaxus's fan translation" : "Optional: \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");
            *tone = ORACLES_TONE_NONE;
            break;
    }
}

unsigned oracles_page_edition_buttons(const OraclesHomeGame *game, int edition, const char *labels[2])
{
    if (edition == 0) return oracles_page_eu_buttons(game, labels);
    if (game->editions[edition - 1].state == ORACLES_ROM_NONE) {
        labels[0] = "Choose\xe2\x80\xa6";
        labels[1] = NULL;
        return 1;
    }
    labels[0] = "Change\xe2\x80\xa6";
    labels[1] = "Remove";
    return 2;
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
            for (int l = 1; l < ORACLES_PAGE_EUROPEAN_LANGUAGES && used < capacity; l++)
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
            for (int l = 1; l < ORACLES_PAGE_EUROPEAN_LANGUAGES && used < capacity; l++)
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
    const int language = chosen_language(game);
    if (language == ORACLES_PAGE_JAPANESE) return "The Japanese edition's text and font, from your Japanese ROM.";
    if (language == ORACLES_PAGE_POLISH) return "Polish fan translation by Kaxus, from your Polish ROM or patch.";
    return language ? "The European edition's text, from your European ROM." : "The original text of your US ROM.";
}

void oracles_page_language_note(const OraclesHomeNav *nav, const OraclesHomeGame *game, char *out, size_t capacity)
{
    const char *reason = oracles_page_language_reason(game, oracles_page_language_focus(nav, game));
    const int language = chosen_language(game);
    if (reason) snprintf(out, capacity, "%s", reason);
    else if (!language || language >= ORACLES_PAGE_EUROPEAN_LANGUAGES) snprintf(out, capacity, "%s", "");
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
    /* Cartridge › Editions: its three rows alone. */
    if (nav->editions && oracles_page_eu_shown(game)) return oracles_page_row_edition(row) >= 0;
    if (oracles_page_row_edition(row) >= 0) return 0;
    if (row == ORACLES_ROW_EDITIONS) return oracles_page_eu_shown(game);
    if (row == ORACLES_ROW_LANGUAGE) return oracles_page_language_shown(nav, game);
    if (row == ORACLES_ROW_JOINED) return game->fan && game->fan->joined;
    return row < ORACLES_GAME_ROWS && (row != ORACLES_ROW_PATCH || game->fan);
}

/* The European ROM gone: no file, English (US), for the Oracle and the fan games made on it; the host forgets them
 * (ORACLES_HOME_STORE). */
static OraclesHomeCommand remove_eu(OraclesHomeNav *nav)
{
    OraclesHomeGame *game = oracles_page_game(nav);
    game->eu = ORACLES_ROM_NONE;
    game->eu_file[0] = game->eu_folder[0] = game->eu_reason[0] = 0;
    if (game->language < ORACLES_PAGE_EUROPEAN_LANGUAGES) game->language = 0;
    for (unsigned f = 0; f < ORACLES_HOME_FAN_GAMES; f++)
        if (&nav->games[oracles_home_fan_games[f].base] == game) nav->games[oracles_home_fan_games[f].game].eu = ORACLES_ROM_NONE;
    nav->eu_action = 0;
    return ORACLES_HOME_STORE;
}

/* The Japanese ROM or the Polish source gone: no file, and English (US) for a game in its language; the host forgets them
 * (ORACLES_HOME_STORE). */
static OraclesHomeCommand remove_edition(OraclesHomeNav *nav, int edition)
{
    OraclesHomeGame *game = oracles_page_game(nav);
    OraclesHomeEdition *e = &game->editions[edition - 1];
    e->state = ORACLES_ROM_NONE;
    e->patch = 0;
    e->file[0] = e->folder[0] = e->reason[0] = 0;
    if (game->language == ORACLES_PAGE_JAPANESE + edition - 1) game->language = 0;
    nav->eu_action = 0;
    return ORACLES_HOME_STORE;
}

int oracles_page_languages_available(const OraclesHomeGame *game)
{
    int count = 0;
    for (int l = 0; l < ORACLES_PAGE_LANGUAGES; l++) count += oracles_page_language_available(game, l);
    return count;
}

/* The language `language`, when the game has it; one the row shows greyed is shown with its reason and not kept. */
static OraclesHomeCommand language_set(OraclesHomeNav *nav, int language)
{
    OraclesHomeGame *game = oracles_page_game(nav);
    if (!oracles_page_language_available(game, language)) {
        if (oracles_page_language_visible(game, language)) nav->language_shown = language;
        return ORACLES_HOME_STAY;
    }
    nav->language_shown = -1;
    if (language == chosen_language(game)) return ORACLES_HOME_STAY;
    game->language = language;
    return ORACLES_HOME_STORE;
}

/* The next language the row shows (`by` 1) or the one before (-1), from the one it shows, wrapping, greyed ones
 * included, with English alone too. */
static OraclesHomeCommand language_step(OraclesHomeNav *nav, int by)
{
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    int shown[ORACLES_PAGE_LANGUAGES];
    const int count = oracles_page_languages_shown(game, shown), now = oracles_page_language_focus(nav, game);
    int i = 0;
    while (i < count && shown[i] != now) i++;
    return language_set(nav, shown[((i < count ? i : 0) + by + count) % count]);
}

static OraclesHomeCommand activate(OraclesHomeNav *nav, unsigned row)
{
    const OraclesHomeGame *game = oracles_page_game(nav);
    switch (row) {
        /* Oracles Combined's European ROMs: Choose… while one is missing or not its game's; recognized, it is changed on its
         * Oracle's page. */
        case ORACLES_ROW_ROM: return game->fan && game->fan->joined && oracles_page_joined_set(game, 0) ? ORACLES_HOME_STAY : ORACLES_HOME_CHOOSE_ROM;
        case ORACLES_ROW_JOINED: return oracles_page_joined_set(game, 1) ? ORACLES_HOME_STAY : ORACLES_HOME_CHOOSE_EU_ROM;
        case ORACLES_ROW_PATCH: return ORACLES_HOME_CHOOSE_PATCH;
        case ORACLES_ROW_SAVE:
            if (!oracles_page_save_action_live(nav, nav->save_action)) return ORACLES_HOME_STAY;
            return nav->save_action == 1 ? ORACLES_HOME_IMPORT_SAVE : nav->android ? ORACLES_HOME_EXPORT_SAVE : ORACLES_HOME_OPEN_FOLDER;
        case ORACLES_ROW_EDITIONS:
            nav->editions = 1;
            nav->row = ORACLES_ROW_EU_ROM;
            nav->eu_action = 0;
            return ORACLES_HOME_STAY;
        case ORACLES_ROW_EU_ROM: return game->eu != ORACLES_ROM_NONE && nav->eu_action == 1 ? remove_eu(nav) : ORACLES_HOME_CHOOSE_EU_ROM;
        case ORACLES_ROW_JP_ROM:
        case ORACLES_ROW_PL_ROM: {
            const int edition = oracles_page_row_edition(row);
            if (game->editions[edition - 1].state != ORACLES_ROM_NONE && nav->eu_action == 1) return remove_edition(nav, edition);
            return edition == 1 ? ORACLES_HOME_CHOOSE_JP_ROM : ORACLES_HOME_CHOOSE_PL_ROM;
        }
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
    if (nav->editions && !oracles_page_eu_shown(oracles_page_game_const(nav))) nav->editions = 0;   /* its game's ROM gone */
    const unsigned row = has_row(nav, nav->row) ? nav->row : nav->editions ? ORACLES_ROW_EU_ROM : 0;
    switch (action) {
        case ORACLES_HOME_UP:
        case ORACLES_HOME_DOWN:
            nav->row = step(nav, row, action == ORACLES_HOME_UP ? ORACLES_GAME_ROWS - 1u : 1u);
            nav->language_shown = -1;
            if (oracles_page_row_edition(nav->row) >= 0) nav->eu_action = 0;
            return ORACLES_HOME_STAY;
        case ORACLES_HOME_LEFT:
        case ORACLES_HOME_RIGHT: {
            /* An edition's row: between Change and Remove, once there is a file; Language: the language before or after;
             * Editions: right opens it, as OK does. */
            const int edition = oracles_page_row_edition(row);
            if (edition >= 0 && oracles_page_edition_state(oracles_page_game_const(nav), edition) != ORACLES_ROM_NONE) nav->eu_action = action == ORACLES_HOME_RIGHT;
            if (row == ORACLES_ROW_SAVE) nav->save_action = action == ORACLES_HOME_RIGHT;
            if (row == ORACLES_ROW_LANGUAGE) return language_step(nav, action == ORACLES_HOME_RIGHT ? 1 : -1);
            if (row == ORACLES_ROW_EDITIONS && action == ORACLES_HOME_RIGHT) return activate(nav, row);
            return ORACLES_HOME_STAY;
        }
        case ORACLES_HOME_OK: return activate(nav, row);
        case ORACLES_HOME_BACK:
            /* From Cartridge › Editions back to Cartridge, on Editions. */
            if (nav->editions) {
                nav->editions = 0;
                nav->row = ORACLES_ROW_EDITIONS;
                nav->eu_action = 0;
                return ORACLES_HOME_STAY;
            }
            /* Back to the home screen, on the item that opened the page; the Save row's status goes with it. */
            nav->language_shown = -1;
            nav->save_flow.state = ORACLES_SAVE_FLOW_NONE;
            nav->screen = ORACLES_SCREEN_HOME;
            nav->focus = 1;
            return ORACLES_HOME_STAY;
    }
    return ORACLES_HOME_STAY;
}

void oracles_page_keep_row(OraclesHomeNav *nav)
{
    if (nav->screen != ORACLES_SCREEN_GAME) return;
    /* Cartridge › Editions closes with its game's original ROM gone, on Save. */
    if (nav->editions && !oracles_page_eu_shown(oracles_page_game_const(nav))) {
        nav->editions = 0;
        nav->row = ORACLES_ROW_SAVE;
    }
    if (has_row(nav, nav->row)) return;
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
    if (row != nav->row) nav->language_shown = -1;
    nav->row = row;
    /* An action of Save, or a button of an edition with a file, under the pointer is the one OK takes. */
    if (row == ORACLES_ROW_SAVE && option >= 0) nav->save_action = option == 1;
    const int edition = oracles_page_row_edition(row);
    if (edition >= 0 && option >= 0 && oracles_page_edition_state(oracles_page_game_const(nav), edition) != ORACLES_ROM_NONE) nav->eu_action = option == 1;
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
    if (row != nav->row) nav->language_shown = -1;
    nav->row = row;
    /* A language clicked is chosen (a greyed one shown with its reason); the row itself only takes the highlight. */
    if (row == ORACLES_ROW_LANGUAGE) return option >= 0 ? language_set(nav, option) : ORACLES_HOME_STAY;
    /* An edition: Remove clicked removes the file; elsewhere on the row, the dialog opens. */
    const int edition = oracles_page_row_edition(row);
    if (edition >= 0) nav->eu_action = option == 1 && oracles_page_edition_state(oracles_page_game_const(nav), edition) != ORACLES_ROM_NONE;
    /* Save: an action clicked acts; elsewhere on the row, the one left and right chose. */
    if (row == ORACLES_ROW_SAVE && option >= 0) nav->save_action = option == 1;
    return activate(nav, row);
}
