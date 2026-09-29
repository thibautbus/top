/* The mods on disk: a mod's directory read into its package, and the launcher's mods folder described for its Mods
 * page (one directory per mod, named after it).  The files are read here, with the C library; engine/mods opens none. */
#ifndef ORACLES_MOD_FOLDER_H
#define ORACLES_MOD_FOLDER_H

#include "mod.h"

/* The mod of the directory `dir` (every NAME.lua in it; the directory's name names the mod), loaded for `game`
 * (oracles_mod_load); NULL with the reason. */
OraclesMod *oracles_mod_folder_load(const char *dir, OraclesGame game, const uint8_t *rom, size_t rom_size, char *error, size_t capacity);

/* A mod of the folder, as the Mods page shows it. */
typedef struct OraclesModEntry {
    char name[64];                                    /* the directory's name */
    char description[ORACLES_MOD_DESCRIPTION_MAX + 1];   /* mod.description, "" when it gives none or is refused */
    unsigned houses[2];                               /* the houses it declares in Ages, in Seasons */
    char refusal[600];                                /* "Refused: " and the loader's reason for the game shown; "" when it loads */
} OraclesModEntry;

/* The directories of `folder` (none whose name starts with "."), in the order of their names, each loaded without a
 * ROM for `game` (its refusal) and for both games (its houses); at most `capacity` of them.  0 when the folder does
 * not exist. */
size_t oracles_mod_folder_list(const char *folder, OraclesGame game, OraclesModEntry *entries, size_t capacity);

#endif
