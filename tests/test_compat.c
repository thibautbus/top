/* Compatibility selection uses authenticated ROM identity only. */
#include "profile.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

static OraclesRomInfo fixture(OraclesGame game, int known)
{
    OraclesRomInfo info;
    memset(&info, 0, sizeof info);
    info.game = game;
    info.known = known;
    return info;
}

int main(void)
{
    const OraclesRomInfo unknown_ages = fixture(ORACLES_GAME_AGES, 0);
    const OraclesRomInfo unversioned_ages = fixture(ORACLES_GAME_AGES, 1);
    const OraclesRomInfo unknown_game = fixture(ORACLES_GAME_UNKNOWN, 1);
    const OraclesRomInfo ages_info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_AGES_US, .known = 1 };
    const OraclesRomInfo seasons_info = { .game = ORACLES_GAME_SEASONS, .revision = ORACLES_ROM_REVISION_SEASONS_US, .known = 1 };
    const OraclesRomInfo moonrise_info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_MOONRISE_1_0_6, .known = 1 };
    const OraclesRomInfo moonrise_wrong_family = { .game = ORACLES_GAME_SEASONS, .revision = ORACLES_ROM_REVISION_MOONRISE_1_0_6, .known = 1 };
    const OraclesCompatProfile *ages = oracles_compat_find(&ages_info);
    const OraclesCompatProfile *seasons = oracles_compat_find(&seasons_info);
    const OraclesCompatProfile *moonrise = oracles_compat_find(&moonrise_info);
    CHECK(ages != NULL && seasons != NULL && moonrise != NULL);
    CHECK(ages && oracles_compat_family(ages) == ORACLES_GAME_AGES);
    CHECK(seasons && oracles_compat_family(seasons) == ORACLES_GAME_SEASONS);
    CHECK(moonrise && oracles_compat_family(moonrise) == ORACLES_GAME_AGES);
    /* Each rule is named in the profile; Moonrise inherits none from Ages' family. */
    CHECK(ages && oracles_compat_sea_level_rules(ages) && oracles_compat_forest_scrambler_rules(ages)
          && !oracles_compat_seasons_rules(ages) && !oracles_compat_lost_woods_rules(ages));
    CHECK(seasons && oracles_compat_seasons_rules(seasons) && oracles_compat_lost_woods_rules(seasons)
          && !oracles_compat_sea_level_rules(seasons) && !oracles_compat_forest_scrambler_rules(seasons));
    CHECK(moonrise && !oracles_compat_seasons_rules(moonrise) && !oracles_compat_sea_level_rules(moonrise)
          && !oracles_compat_forest_scrambler_rules(moonrise) && !oracles_compat_lost_woods_rules(moonrise)
          && !oracles_compat_maku_tree_rules(moonrise) && !oracles_compat_maku_isolation_rules(moonrise));
    CHECK(ages && oracles_compat_guest_tables(ages) == &oracles_guest_tables_ages);
    CHECK(seasons && oracles_compat_guest_tables(seasons) == &oracles_guest_tables_seasons);
    CHECK(moonrise && oracles_compat_guest_tables(moonrise) != &oracles_guest_tables_ages);
    CHECK(ages && oracles_compat_data_tables(ages) == &oracles_tables_ages);
    CHECK(seasons && oracles_compat_data_tables(seasons) == &oracles_tables_seasons);
    CHECK(moonrise && oracles_compat_data_tables(moonrise) != &oracles_tables_ages);
    CHECK(ages && oracles_compat_room_flag_page(ages, 2) == 0 && oracles_compat_map_width(ages) == 14);
    CHECK(seasons && oracles_compat_room_flag_page(seasons, 2) == 1 && oracles_compat_map_height(seasons) == 16);
    CHECK(moonrise && oracles_compat_room_flag_page(moonrise, 2) == 0 && oracles_compat_map_width(moonrise) == 8
          && oracles_compat_map_height(moonrise) == 8);
    CHECK(ages && oracles_compat_open_water_rules(ages) && oracles_compat_maku_isolation_rules(ages));
    CHECK(seasons && !oracles_compat_open_water_rules(seasons) && !oracles_compat_maku_isolation_rules(seasons));
    CHECK(ages && oracles_compat_maku_tree_rules(ages));
    CHECK(seasons && oracles_compat_maku_tree_rules(seasons));
    CHECK(moonrise && oracles_compat_open_water_rules(moonrise) && !oracles_compat_maku_tree_rules(moonrise)
          && !oracles_compat_maku_isolation_rules(moonrise));
    CHECK(ages && strcmp(oracles_compat_id(ages), "ages-original") == 0);
    CHECK(seasons && strcmp(oracles_compat_name(seasons), "Oracle of Seasons") == 0);
    CHECK(ages && oracles_compat_continuous_transitions(ages));
    CHECK(seasons && oracles_compat_continuous_transitions(seasons));
    CHECK(moonrise && oracles_compat_continuous_transitions(moonrise));
    /* The swim reads the tile types in the ROM: located in the originals only. */
    CHECK(oracles_compat_continuous_swim(ages) && oracles_compat_continuous_swim(seasons) && !oracles_compat_continuous_swim(moonrise));
    CHECK(!oracles_compat_continuous_swim(NULL));
    /* The drawn-back view is qualified on the three; the item hotkeys on the originals only. */
    CHECK(oracles_compat_item_hotkeys(ages) && oracles_compat_item_hotkeys(seasons) && !oracles_compat_item_hotkeys(moonrise));
    CHECK(oracles_compat_zoom_out(ages) && oracles_compat_zoom_out(seasons) && oracles_compat_zoom_out(moonrise));
    /* The overworld groups' maps by their tileset: Seasons' (Holodrum 16 x 16, Subrosia 11 x 8) and Moonrise's (8 x 8
     * each); Ages keeps the grid, both of its overworlds outdoors across its 14 x 14. */
    CHECK(!oracles_compat_tileset_map_rules(ages) && oracles_compat_tileset_map_rules(seasons) && oracles_compat_tileset_map_rules(moonrise));
    CHECK(oracles_compat_group_map_width(ages, 1) == 14 && oracles_compat_group_map_height(ages, 1) == 14);
    CHECK(oracles_compat_group_map_width(seasons, 0) == 16 && oracles_compat_group_map_height(seasons, 0) == 16);
    CHECK(oracles_compat_group_map_width(seasons, 1) == 11 && oracles_compat_group_map_height(seasons, 1) == 8);
    CHECK(oracles_compat_group_map_width(moonrise, 1) == 8 && oracles_compat_group_map_height(moonrise, 0) == 8);
    CHECK(!oracles_compat_item_hotkeys(NULL) && !oracles_compat_zoom_out(NULL));
    CHECK(oracles_compat_find(&unknown_ages) == NULL);
    CHECK(oracles_compat_find(&unversioned_ages) == NULL);
    CHECK(oracles_compat_find(&unknown_game) == NULL);
    CHECK(oracles_compat_find(&moonrise_wrong_family) == NULL);
    /* Temple of Seasons: Seasons' family and rules on hack-base's engine, its own tables, no item hotkeys. */
    const OraclesRomInfo temple_info = { .game = ORACLES_GAME_SEASONS, .revision = ORACLES_ROM_REVISION_TEMPLE_1_073, .known = 1 };
    const OraclesRomInfo temple_wrong_family = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_TEMPLE_1_073, .known = 1 };
    const OraclesCompatProfile *temple = oracles_compat_find(&temple_info);
    CHECK(temple && oracles_compat_family(temple) == ORACLES_GAME_SEASONS && strcmp(oracles_compat_id(temple), "temple-1.073") == 0);
    CHECK(temple && oracles_compat_seasons_rules(temple) && oracles_compat_lost_woods_rules(temple) && oracles_compat_maku_tree_rules(temple)
          && !oracles_compat_sea_level_rules(temple) && !oracles_compat_forest_scrambler_rules(temple) && !oracles_compat_open_water_rules(temple)
          && !oracles_compat_maku_isolation_rules(temple));
    CHECK(temple && oracles_compat_guest_tables(temple) != &oracles_guest_tables_seasons && oracles_compat_data_tables(temple) != &oracles_tables_seasons);
    CHECK(temple && oracles_compat_room_flag_page(temple, 2) == 1 && oracles_compat_map_width(temple) == 16 && oracles_compat_map_height(temple) == 12);
    CHECK(temple && oracles_compat_tileset_map_rules(temple) && oracles_compat_group_map_width(temple, 1) == 11 && oracles_compat_group_map_height(temple, 1) == 8
          && oracles_compat_group_map_left(temple, 1) == 4 && oracles_compat_group_map_top(temple, 1) == 0);
    CHECK(temple && oracles_compat_group_map_width(temple, 0) == 16 && oracles_compat_group_map_height(temple, 0) == 12
          && oracles_compat_group_map_left(temple, 0) == 13 && oracles_compat_group_map_top(temple, 0) == 3);
    CHECK(oracles_compat_group_map_left(seasons, 1) == 0 && oracles_compat_group_map_top(seasons, 0) == 0 && oracles_compat_group_map_left(moonrise, 0) == 0);
    CHECK(temple && oracles_compat_continuous_transitions(temple) && oracles_compat_zoom_out(temple) && !oracles_compat_item_hotkeys(temple));
    CHECK(oracles_compat_find(&temple_wrong_family) == NULL);
    /* Grass outside group 0: every tile of the range in the originals, $f9 left out on hack-base's engine. */
    CHECK(oracles_guest_tables_seasons.grass_tile_last_other_groups == oracles_guest_tables_seasons.grass_tile_last
          && oracles_guest_tables_ages.grass_tile_last_other_groups == oracles_guest_tables_ages.grass_tile_last);
    CHECK(temple && oracles_compat_guest_tables(temple)->grass_tile_last == 0xf9 && oracles_compat_guest_tables(temple)->grass_tile_last_other_groups == 0xf8);
    /* Gifts of Kinomi: Ages' family on hack-base, its own tables and 8 x 8 map, none of the originals' routing or season rules. */
    const OraclesRomInfo kinomi_info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_KINOMI_1_1_2, .known = 1 };
    const OraclesRomInfo kinomi_wrong_family = { .game = ORACLES_GAME_SEASONS, .revision = ORACLES_ROM_REVISION_KINOMI_1_1_2, .known = 1 };
    const OraclesCompatProfile *kinomi = oracles_compat_find(&kinomi_info);
    CHECK(kinomi && oracles_compat_family(kinomi) == ORACLES_GAME_AGES && strcmp(oracles_compat_id(kinomi), "kinomi-1.1.2") == 0);
    CHECK(kinomi && oracles_compat_open_water_rules(kinomi) && oracles_compat_maku_tree_rules(kinomi) && oracles_compat_tileset_map_rules(kinomi)
          && !oracles_compat_seasons_rules(kinomi) && !oracles_compat_lost_woods_rules(kinomi) && !oracles_compat_forest_scrambler_rules(kinomi)
          && !oracles_compat_sea_level_rules(kinomi) && !oracles_compat_maku_isolation_rules(kinomi) && oracles_compat_own_routing_rules(kinomi));
    CHECK(!oracles_compat_own_routing_rules(ages) && !oracles_compat_own_routing_rules(seasons) && !oracles_compat_own_routing_rules(moonrise)
          && !oracles_compat_own_routing_rules(temple));
    CHECK(kinomi && oracles_compat_guest_tables(kinomi) != &oracles_guest_tables_ages && oracles_compat_data_tables(kinomi) != &oracles_tables_ages);
    CHECK(kinomi && oracles_compat_room_flag_page(kinomi, 2) == 0 && oracles_compat_group_map_width(kinomi, 1) == 8 && oracles_compat_group_map_height(kinomi, 0) == 8);
    CHECK(kinomi && oracles_compat_continuous_transitions(kinomi) && oracles_compat_zoom_out(kinomi) && !oracles_compat_item_hotkeys(kinomi));
    CHECK(oracles_compat_find(&kinomi_wrong_family) == NULL);
    CHECK(oracles_compat_find(NULL) == NULL);
    if (failures) return 1;
    puts("test_compat: ok");
    return 0;
}
