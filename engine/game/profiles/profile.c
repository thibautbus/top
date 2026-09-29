#include "profile.h"
#include "moonrise-regalia/identity.h"
#include "temple-of-seasons/identity.h"
#include "gifts-of-kinomi/identity.h"

extern const OraclesGuestTables oracles_guest_tables_moonrise;
extern const OraclesTables oracles_tables_moonrise;
extern const OraclesGuestTables oracles_guest_tables_temple;
extern const OraclesTables oracles_tables_temple;
extern const OraclesGuestTables oracles_guest_tables_kinomi;
extern const OraclesTables oracles_tables_kinomi;

static const uint8_t ages_room_flag_pages[8] = { 0, 1, 0, 1, 2, 3, 2, 3 };
static const uint8_t seasons_room_flag_pages[8] = { 0, 1, 1, 1, 2, 3, 2, 3 };
static const uint8_t moonrise_room_flag_pages[8] = { 0, 1, 0, 1, 2, 3, 2, 3 };
static const uint8_t temple_room_flag_pages[8] = { 0, 1, 1, 1, 2, 3, 2, 3 };   /* hack-base's flagLocationGroupTable, as Seasons' */
static const uint8_t kinomi_room_flag_pages[8] = { 0, 1, 0, 1, 2, 3, 2, 3 };   /* its source's flagLocationGroupTable, as Ages' */
/* OVERWORLD_WIDTH/HEIGHT from constants/common/other.s. */

static const OraclesCompatProfile profiles[] = {
    { .id = "ages-original", .name = "Oracle of Ages", .revision = ORACLES_ROM_REVISION_AGES_US, .family = ORACLES_GAME_AGES,
      .continuous_transitions = 1, .continuous_swim = 1, .item_hotkeys = 1, .zoom_out = 1,
      .guest_tables = &oracles_guest_tables_ages, .data_tables = &oracles_tables_ages, .room_flag_pages = ages_room_flag_pages,
      .map_width = 14, .map_height = 14,
      .open_water_rules = 1, .maku_tree_rules = 1, .maku_isolation_rules = 1, .sea_level_rules = 1, .forest_scrambler_rules = 1 },
    { .id = "seasons-original", .name = "Oracle of Seasons", .revision = ORACLES_ROM_REVISION_SEASONS_US, .family = ORACLES_GAME_SEASONS,
      .continuous_transitions = 1, .continuous_swim = 1, .item_hotkeys = 1, .zoom_out = 1,
      .guest_tables = &oracles_guest_tables_seasons, .data_tables = &oracles_tables_seasons, .room_flag_pages = seasons_room_flag_pages,
      .map_width = 16, .map_height = 16, .second_map_width = 11, .second_map_height = 8,   /* SUBROSIA_WIDTH, _HEIGHT */
      .maku_tree_rules = 1, .seasons_rules = 1, .lost_woods_rules = 1, .tileset_map_rules = 1 },
    /* Moonrise: the rules its map and data were shown to follow, none inherited from Ages' family. */
    { .id = "moonrise-1.0.6", .name = "Moonrise Regalia 1.0.6", .revision = ORACLES_ROM_REVISION_MOONRISE_1_0_6, .family = ORACLES_GAME_AGES,
      .continuous_transitions = 1, .zoom_out = 1,
      .guest_tables = &oracles_guest_tables_moonrise, .data_tables = &oracles_tables_moonrise, .room_flag_pages = moonrise_room_flag_pages,
      .map_width = 8, .map_height = 8,
      .open_water_rules = 1, .tileset_map_rules = 1 },
    /* Temple of Seasons: built on hack-base's Seasons engine, Holodrum kept with its seasons, its Lost Woods routing and the
     * Maku tree (temple-of-seasons/manifest.json); features qualified only once its routes held them. */
    { .id = "temple-1.073", .name = "Temple of Seasons 1.073", .revision = ORACLES_ROM_REVISION_TEMPLE_1_073, .family = ORACLES_GAME_SEASONS,
      .continuous_transitions = 1, .zoom_out = 1,
      .guest_tables = &oracles_guest_tables_temple, .data_tables = &oracles_tables_temple, .room_flag_pages = temple_room_flag_pages,
      /* Its maps are walled inside the grid by an outdoor filler tileset ($64, $e4) and bound its map screen's cursor:
       * Holodrum's columns 13 to 15 and rows 3 to 11, Subrosia's columns 4 to 10 and rows 0 to 7. */
      .map_width = 16, .map_height = 12, .map_left = 13, .map_top = 3,
      .second_map_width = 11, .second_map_height = 8, .second_map_left = 4, .second_map_top = 0,
      .maku_tree_rules = 1, .seasons_rules = 1, .lost_woods_rules = 1, .tileset_map_rules = 1 },
    /* Gifts of Kinomi: Ages' engine on hack-base, bound from a build of its published source
     * (gifts-of-kinomi/manifest.json).  Its 8 x 8 map is its source's OVERWORLD_WIDTH/HEIGHT; the rooms its
     * tilesets put outdoors past it are reached only by the transitions it routes itself.  Its seasons follow the room
     * pack (checkRoomPackAfterWarp_body, triggerFadeoutTransition) and its rod reloads the room: a byte of the coarse list,
     * none of Seasons' rules for the season the band holds.  Its routines of mapTransitionGroupTable are its own (its
     * Lost Woods, sword and shrine rooms, forest scrambler, sea, eye puzzle): each room of the table is taken as routing
     * elsewhere from the start, never a start for the room beyond it, and its answers are asked again after every
     * transition.  Features qualified only once its routes held them. */
    { .id = "kinomi-1.1.2", .name = "Gifts of Kinomi 1.1.2", .revision = ORACLES_ROM_REVISION_KINOMI_1_1_2, .family = ORACLES_GAME_AGES,
      .continuous_transitions = 1, .zoom_out = 1,
      .guest_tables = &oracles_guest_tables_kinomi, .data_tables = &oracles_tables_kinomi, .room_flag_pages = kinomi_room_flag_pages,
      .map_width = 8, .map_height = 8,
      .open_water_rules = 1, .maku_tree_rules = 1, .tileset_map_rules = 1, .own_routing_rules = 1 }
};

const OraclesCompatProfile *oracles_compat_find(const OraclesRomInfo *info)
{
    if (!info || !info->known) return NULL;
    for (size_t i = 0; i < sizeof profiles / sizeof profiles[0]; i++)
        if (profiles[i].revision == info->revision && profiles[i].family == info->game) return &profiles[i];
    return NULL;
}

const char *oracles_compat_id(const OraclesCompatProfile *profile) { return profile ? profile->id : NULL; }
const char *oracles_compat_name(const OraclesCompatProfile *profile) { return profile ? profile->name : NULL; }
OraclesGame oracles_compat_family(const OraclesCompatProfile *profile) { return profile ? profile->family : ORACLES_GAME_UNKNOWN; }
int oracles_compat_continuous_transitions(const OraclesCompatProfile *profile)
{
    return profile ? profile->continuous_transitions : 0;
}
int oracles_compat_item_hotkeys(const OraclesCompatProfile *profile)
{
    return profile ? profile->item_hotkeys : 0;
}
int oracles_compat_continuous_swim(const OraclesCompatProfile *profile)
{
    return profile ? profile->continuous_swim : 0;
}
int oracles_compat_zoom_out(const OraclesCompatProfile *profile)
{
    return profile ? profile->zoom_out : 0;
}
const OraclesGuestTables *oracles_compat_guest_tables(const OraclesCompatProfile *profile)
{
    return profile ? profile->guest_tables : NULL;
}
const OraclesTables *oracles_compat_data_tables(const OraclesCompatProfile *profile)
{
    return profile ? profile->data_tables : NULL;
}
uint8_t oracles_compat_room_flag_page(const OraclesCompatProfile *profile, unsigned group)
{
    return profile ? profile->room_flag_pages[group & 7u] : 0;
}
uint8_t oracles_compat_map_width(const OraclesCompatProfile *profile)
{
    return profile ? profile->map_width : 0;
}
uint8_t oracles_compat_map_height(const OraclesCompatProfile *profile)
{
    return profile ? profile->map_height : 0;
}
uint8_t oracles_compat_group_map_width(const OraclesCompatProfile *profile, unsigned group)
{
    if (!profile) return 0;
    return (group & 7u) == 1u && profile->second_map_width ? profile->second_map_width : profile->map_width;
}
uint8_t oracles_compat_group_map_height(const OraclesCompatProfile *profile, unsigned group)
{
    if (!profile) return 0;
    return (group & 7u) == 1u && profile->second_map_height ? profile->second_map_height : profile->map_height;
}
uint8_t oracles_compat_group_map_left(const OraclesCompatProfile *profile, unsigned group)
{
    if (!profile) return 0;
    return (group & 7u) == 1u && profile->second_map_width ? profile->second_map_left : profile->map_left;
}
uint8_t oracles_compat_group_map_top(const OraclesCompatProfile *profile, unsigned group)
{
    if (!profile) return 0;
    return (group & 7u) == 1u && profile->second_map_height ? profile->second_map_top : profile->map_top;
}
int oracles_compat_tileset_map_rules(const OraclesCompatProfile *profile)
{
    return profile ? profile->tileset_map_rules : 0;
}
int oracles_compat_open_water_rules(const OraclesCompatProfile *profile)
{
    return profile ? profile->open_water_rules : 0;
}
int oracles_compat_maku_tree_rules(const OraclesCompatProfile *profile)
{
    return profile ? profile->maku_tree_rules : 0;
}
int oracles_compat_maku_isolation_rules(const OraclesCompatProfile *profile)
{
    return profile ? profile->maku_isolation_rules : 0;
}
int oracles_compat_seasons_rules(const OraclesCompatProfile *profile)
{
    return profile ? profile->seasons_rules : 0;
}
int oracles_compat_sea_level_rules(const OraclesCompatProfile *profile)
{
    return profile ? profile->sea_level_rules : 0;
}
int oracles_compat_forest_scrambler_rules(const OraclesCompatProfile *profile)
{
    return profile ? profile->forest_scrambler_rules : 0;
}
int oracles_compat_lost_woods_rules(const OraclesCompatProfile *profile)
{
    return profile ? profile->lost_woods_rules : 0;
}
int oracles_compat_own_routing_rules(const OraclesCompatProfile *profile)
{
    return profile ? profile->own_routing_rules : 0;
}
