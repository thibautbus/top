#include "ui_page_nav.h"

#include <stdio.h>
#include <string.h>

const char oracles_mods_folder_line[] = "Beside the launcher\xe2\x80\x99s settings \xc2\xb7 one folder per mod";
const char oracles_mods_empty[] = "No mods found. Put each mod in its own folder inside the mods folder, named with a\xe2\x80\x93z, 0\xe2\x80\x93" "9, - and _, "
                                  "with a main.lua in it.";
const char oracles_mods_note[] = "A game with mods plays on its own save, NAME.mods.sav beside the ROM, copied from the game's save the first time; "
                                 "the save without mods is never written by a game with mods.";
const char oracles_mods_limit[] = "Eight mods at most play together";

/* The page's game: an Oracle, the one the menu showed (a fan game's menu greys Mods). */
static int mods_game(const OraclesHomeNav *nav)
{
    return oracles_home_game(nav) == ORACLES_HOME_GAME_SEASONS ? ORACLES_HOME_GAME_SEASONS : ORACLES_HOME_GAME_AGES;
}

const OraclesHomeMods *oracles_mods_list(const OraclesHomeNav *nav) { return &nav->mods[mods_game(nav)]; }
unsigned oracles_mods_rows(const OraclesHomeNav *nav) { return oracles_mods_list(nav)->count + 2u; }
unsigned oracles_mods_row_play(const OraclesHomeNav *nav) { return oracles_mods_list(nav)->count + 1u; }

unsigned oracles_mods_active(const OraclesHomeNav *nav, int game)
{
    if (game != ORACLES_HOME_GAME_AGES && game != ORACLES_HOME_GAME_SEASONS) return 0;
    unsigned n = 0;
    for (unsigned i = 0; i < nav->mods[game].count; i++) n += nav->mods[game].mods[i].active && !nav->mods[game].mods[i].refused;
    return n;
}

void oracles_mods_games(const OraclesHomeMod *mod, char *out, size_t capacity)
{
    const int ages = mod->houses[ORACLES_HOME_GAME_AGES] > 0, seasons = mod->houses[ORACLES_HOME_GAME_SEASONS] > 0;
    if (mod->refused || (!ages && !seasons)) snprintf(out, capacity, "%s", "");
    else snprintf(out, capacity, "Houses in %s%s%s", ages ? "Ages" : "", ages && seasons ? " and " : "", seasons ? "Seasons" : "");
}

void oracles_mods_count(const OraclesHomeNav *nav, char *out, size_t capacity)
{
    const unsigned active = oracles_mods_active(nav, mods_game(nav));
    if (oracles_mods_list(nav)->count) snprintf(out, capacity, "%u of %u active \xc2\xb7 they play in the order of their names", active, ORACLES_HOME_MODS_ACTIVE);
    else snprintf(out, capacity, "0 of %u active", ORACLES_HOME_MODS_ACTIVE);
}

/* Mods play on an original ROM: a house is composed into the image the port recognises. */
const char *oracles_mods_play_note(const OraclesHomeNav *nav)
{
    const OraclesHomeGame *game = &nav->games[mods_game(nav)];
    if (!game->usable) return "Choose a ROM in Cartridge first";
    if (game->rom != ORACLES_ROM_ORIGINAL) return "Mods play on the original ROMs";
    return "";
}

/* A mod switched on or off: a refused one stays off, a ninth stays off with the reason. */
static OraclesHomeCommand switch_mod(OraclesHomeNav *nav, unsigned index, int on)
{
    OraclesHomeMod *mod = &nav->mods[mods_game(nav)].mods[index];
    if (mod->refused || mod->active == on) return ORACLES_HOME_STAY;
    if (on && oracles_mods_active(nav, mods_game(nav)) >= ORACLES_HOME_MODS_ACTIVE) return ORACLES_HOME_MODS_LIMIT;
    mod->active = on;
    return ORACLES_HOME_STORE;
}

static OraclesHomeCommand activate(OraclesHomeNav *nav, unsigned row)
{
    if (row == ORACLES_MODS_ROW_FOLDER) return ORACLES_HOME_OPEN_MODS;
    if (row == oracles_mods_row_play(nav)) {
        if (oracles_mods_play_note(nav)[0]) return ORACLES_HOME_STAY;
        return mods_game(nav) == ORACLES_HOME_GAME_SEASONS ? ORACLES_HOME_START_SEASONS_MODS : ORACLES_HOME_START_AGES_MODS;
    }
    const OraclesHomeMod *mod = &oracles_mods_list(nav)->mods[row - 1u];
    return switch_mod(nav, row - 1u, !mod->active);
}

OraclesHomeCommand oracles_mods_act(OraclesHomeNav *nav, OraclesHomeAction action)
{
    const unsigned rows = oracles_mods_rows(nav);
    if (nav->row >= rows) nav->row = rows - 1u;   /* the list may have shrunk under it */
    const unsigned row = nav->row;
    switch (action) {
        case ORACLES_HOME_UP: nav->row = (row + rows - 1u) % rows; return ORACLES_HOME_STAY;
        case ORACLES_HOME_DOWN: nav->row = (row + 1u) % rows; return ORACLES_HOME_STAY;
        case ORACLES_HOME_LEFT:
        case ORACLES_HOME_RIGHT:
            if (row == ORACLES_MODS_ROW_FOLDER || row == oracles_mods_row_play(nav)) return ORACLES_HOME_STAY;
            return switch_mod(nav, row - 1u, action == ORACLES_HOME_RIGHT);
        case ORACLES_HOME_OK: return activate(nav, row);
        case ORACLES_HOME_BACK:
            nav->screen = ORACLES_SCREEN_HOME;
            nav->focus = 4;   /* Mods, in the game's menu */
            return ORACLES_HOME_STAY;
    }
    return ORACLES_HOME_STAY;
}

void oracles_mods_hover(OraclesHomeNav *nav, unsigned row)
{
    if (row < oracles_mods_rows(nav)) nav->row = row;
}

OraclesHomeCommand oracles_mods_click(OraclesHomeNav *nav, unsigned row)
{
    if (row >= oracles_mods_rows(nav)) return ORACLES_HOME_STAY;
    nav->row = row;
    return activate(nav, row);
}
