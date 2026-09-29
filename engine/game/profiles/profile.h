/* Immutable compatibility descriptors of the authenticated images. */
#ifndef ORACLES_COMPAT_PROFILE_H
#define ORACLES_COMPAT_PROFILE_H

#include "guest_tables.h"
#include "oracles_rom.h"
#include "rom.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OraclesCompatProfile {
    const char *id;
    const char *name;
    OraclesRomRevision revision;
    OraclesGame family;              /* the header's game: the route and savestate labels, never a licence to inherit a rule */
    int continuous_transitions;
    /* Features qualified on this image: without, they are refused or left off, never tried on unverified addresses. */
    int item_hotkeys;
    int continuous_swim;             /* --continuous-swim: the tile types of the water read in the ROM */
    int zoom_out;
    const OraclesGuestTables *guest_tables;
    const OraclesTables *data_tables;
    const uint8_t *room_flag_pages;
    /* An overworld group's map on the 16 x 16 grid: its first column and row (`map_left`, `map_top`, 0 but where a game
     * walls a smaller map inside the grid) and its edges past the last (`map_width`, `map_height`: its columns and rows
     * when it starts at 0).  The second group's (`second_map_*`) when it has its own (Subrosia's 11 x 8), 0: the first's. */
    uint8_t map_width, map_height, map_left, map_top;
    uint8_t second_map_width, second_map_height, second_map_left, second_map_top;
    /* The game's own rules, each named: the engine asks for a rule, never for a game. */
    int open_water_rules;            /* open seas mapped inside the interior groups */
    int maku_tree_rules;             /* the Maku tree's screen stands alone */
    int maku_isolation_rules;        /* and so do the rooms under minimap group 2 (Ages' tree inside) */
    int seasons_rules;               /* the season decides a room's terrain (Seasons) */
    int sea_level_rules;             /* a dive or a surfacing crosses two groups (Ages) */
    int forest_scrambler_rules;      /* a forest whose rooms route by a scrambled order (Ages) */
    int lost_woods_rules;            /* rooms of the routing table route elsewhere for good (Seasons) */
    int own_routing_rules;           /* the routing table's routines are the game's own: its rooms are taken as routing elsewhere
                                        from the start, as Seasons', with no routing key (Gifts of Kinomi) */
    int tileset_map_rules;           /* a room of groups 0 and 1 is on its map by its tileset (outdoors) and its place within the map;
                                        the others (houses, a dungeon, areas past the map) are interiors */
} OraclesCompatProfile;

/* Returns an original profile only for an authenticated, known ROM identity. */
const OraclesCompatProfile *oracles_compat_find(const OraclesRomInfo *info);
const char *oracles_compat_id(const OraclesCompatProfile *profile);
const char *oracles_compat_name(const OraclesCompatProfile *profile);
OraclesGame oracles_compat_family(const OraclesCompatProfile *profile);
int oracles_compat_continuous_transitions(const OraclesCompatProfile *profile);
int oracles_compat_item_hotkeys(const OraclesCompatProfile *profile);
int oracles_compat_continuous_swim(const OraclesCompatProfile *profile);
int oracles_compat_zoom_out(const OraclesCompatProfile *profile);
const OraclesGuestTables *oracles_compat_guest_tables(const OraclesCompatProfile *profile);
const OraclesTables *oracles_compat_data_tables(const OraclesCompatProfile *profile);
uint8_t oracles_compat_room_flag_page(const OraclesCompatProfile *profile, unsigned group);
uint8_t oracles_compat_map_width(const OraclesCompatProfile *profile);
uint8_t oracles_compat_map_height(const OraclesCompatProfile *profile);
/* The columns and rows of the map of an overworld group (0 or 1): the second's when it has its own. */
uint8_t oracles_compat_group_map_width(const OraclesCompatProfile *profile, unsigned group);
uint8_t oracles_compat_group_map_height(const OraclesCompatProfile *profile, unsigned group);
/* Its first column and row: 0 but for a map walled inside the grid. */
uint8_t oracles_compat_group_map_left(const OraclesCompatProfile *profile, unsigned group);
uint8_t oracles_compat_group_map_top(const OraclesCompatProfile *profile, unsigned group);
int oracles_compat_tileset_map_rules(const OraclesCompatProfile *profile);
int oracles_compat_open_water_rules(const OraclesCompatProfile *profile);
int oracles_compat_maku_tree_rules(const OraclesCompatProfile *profile);
int oracles_compat_maku_isolation_rules(const OraclesCompatProfile *profile);
int oracles_compat_seasons_rules(const OraclesCompatProfile *profile);
int oracles_compat_sea_level_rules(const OraclesCompatProfile *profile);
int oracles_compat_forest_scrambler_rules(const OraclesCompatProfile *profile);
int oracles_compat_lost_woods_rules(const OraclesCompatProfile *profile);
int oracles_compat_own_routing_rules(const OraclesCompatProfile *profile);

#ifdef __cplusplus
}
#endif

#endif
