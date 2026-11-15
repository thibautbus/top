#include "profile.h"
#include "moonrise-regalia/identity.h"
#include "temple-of-seasons/identity.h"
#include "gifts-of-kinomi/identity.h"
#include "oracles-combined/ages/identity.h"
#include "oracles-combined/seasons/identity.h"

extern const OraclesGuestTables oracles_guest_tables_moonrise;
extern const OraclesTables oracles_tables_moonrise;
extern const OraclesGuestTables oracles_guest_tables_temple;
extern const OraclesTables oracles_tables_temple;
extern const OraclesGuestTables oracles_guest_tables_kinomi;
extern const OraclesTables oracles_tables_kinomi;
extern const OraclesGuestTables oracles_guest_tables_combined_ages_en, oracles_guest_tables_combined_ages_fr, oracles_guest_tables_combined_ages_de,
    oracles_guest_tables_combined_ages_es, oracles_guest_tables_combined_ages_it;
extern const OraclesGuestTables oracles_guest_tables_combined_seasons_en, oracles_guest_tables_combined_seasons_fr, oracles_guest_tables_combined_seasons_de,
    oracles_guest_tables_combined_seasons_es, oracles_guest_tables_combined_seasons_it;
extern const OraclesTables oracles_tables_combined_ages, oracles_tables_combined_seasons;

/* Oracles Combined's halves as the address spaces see them (oracles_rom.h): a half of the image each, the RAM's bank 1
 * holding its copy of bank $41. */
_Static_assert(ORACLES_COMBINED_AGES_HALF_SIZE == ORACLES_ROM_HALF_SIZE && ORACLES_COMBINED_SEASONS_HALF_SIZE == ORACLES_ROM_HALF_SIZE, "a half of the image");
_Static_assert(ORACLES_COMBINED_AGES_IMAGE_OFFSET == ORACLES_COMBINED_AGES_ROM_BANK_BASE * 0x4000u
               && ORACLES_COMBINED_SEASONS_IMAGE_OFFSET == ORACLES_COMBINED_SEASONS_ROM_BANK_BASE * 0x4000u, "a half starts at its bank base");
_Static_assert(ORACLES_COMBINED_AGES_SRAM_CODE_RAM_BANK < 8u && ORACLES_COMBINED_SEASONS_SRAM_CODE_RAM_BANK < 8u, "a bank of the cartridge's RAM");
_Static_assert(ORACLES_COMBINED_AGES_SRAM_CODE_BANK == ORACLES_SRAM_CODE_BANK && ORACLES_COMBINED_SEASONS_SRAM_CODE_BANK == ORACLES_SRAM_CODE_BANK,
               "the bank the cartridge's RAM runs a copy of");
_Static_assert(ORACLES_COMBINED_AGES_ROM_SIZE == 2u * ORACLES_ROM_HALF_SIZE && ORACLES_COMBINED_SEASONS_ROM_SIZE == ORACLES_COMBINED_AGES_ROM_SIZE, "two halves");

static const uint8_t ages_room_flag_pages[8] = { 0, 1, 0, 1, 2, 3, 2, 3 };
static const uint8_t seasons_room_flag_pages[8] = { 0, 1, 1, 1, 2, 3, 2, 3 };
static const uint8_t moonrise_room_flag_pages[8] = { 0, 1, 0, 1, 2, 3, 2, 3 };
static const uint8_t temple_room_flag_pages[8] = { 0, 1, 1, 1, 2, 3, 2, 3 };   /* hack-base's flagLocationGroupTable, as Seasons' */
static const uint8_t kinomi_room_flag_pages[8] = { 0, 1, 0, 1, 2, 3, 2, 3 };   /* its source's flagLocationGroupTable, as Ages' */
/* Oracles Combined's flagLocationGroupTable in each half, read in the five images: Ages' and Seasons'. */
static const uint8_t combined_ages_room_flag_pages[8] = { 0, 1, 0, 1, 2, 3, 2, 3 };
static const uint8_t combined_seasons_room_flag_pages[8] = { 0, 1, 1, 1, 2, 3, 2, 3 };
/* OVERWORLD_WIDTH/HEIGHT from constants/common/other.s. */

/* Each half's applyRoomSpecificTileChanges runs its original's routines for the original's rooms, compared instruction
 * by instruction in the five images (the loads `call lda_*` makes of the variables, the room flags' pages read from the
 * cartridge's RAM, and Ages' checkTreasureObtained_body, whose new branch takes treasures $60 to $67 alone, the same for
 * Ricky's gloves): their replay is the originals' (room_code_replay).  The routines it adds, past the original's jump
 * table, are not among them and stay refused: Ages' $38 for 2:ee and Seasons' $2f, which runs in 3:91 alone (Vasu's
 * shop; Seasons' applyAllTileSubstitutions skips group 2's, 2:91's entry too, as the prediction does). */

#define COMBINED_HALF(HALF, LANG) \
    .revision = ORACLES_ROM_REVISION_COMBINED_1_5_##LANG, .rom_bank_base = ORACLES_COMBINED_##HALF##_ROM_BANK_BASE, \
    .sram_code_banks = 1u << ORACLES_COMBINED_##HALF##_SRAM_CODE_RAM_BANK, .half_switch = 1, .half_game = ORACLES_COMBINED_##HALF##_HALF, \
    .current_game = ORACLES_COMBINED_##HALF##_CURRENT_GAME, .swap_game = ORACLES_COMBINED_##HALF##_SWAP_GAME, \
    .swap_game_request = ORACLES_COMBINED_##HALF##_SWAP_GAME_REQUEST, .late_line_writes = 1, \
    .text_split_index_2 = ORACLES_COMBINED_##HALF##_TEXT_SPLIT_INDEX_2_##LANG, .text_offset_3 = ORACLES_COMBINED_##HALF##_TEXT_OFFSET_3_##LANG, \
    .compact_text = 1, .host_text_box = 1, .text_size = 1, .language_bank = ORACLES_IMAGE_LANGUAGE_BANK, \
    .item_hotkeys = 1, .both_games_items = 1, .room_code_replay = 1
#define COMBINED_AGES(LANG, lang) \
    { .id = "combined-1.5-" #lang "-ages", .name = "Oracles Combined 1.5 (Oracle of Ages)", .family = ORACLES_GAME_AGES, COMBINED_HALF(AGES, LANG), \
      .continuous_transitions = 1, .continuous_swim = 1, .zoom_out = 1, .continuous_diagonal = 1, .predict_collisions = 1, .continuous_steps = 1, .continuous_line = 1, \
      .swim_hold = 1, .cheats = 1, .text_instant = 1, \
      .guest_tables = &oracles_guest_tables_combined_ages_##lang, .data_tables = &oracles_tables_combined_ages, .room_flag_pages = combined_ages_room_flag_pages, \
      .map_width = 14, .map_height = 14, \
      .open_water_rules = 1, .maku_tree_rules = 1, .maku_isolation_rules = 1, .sea_level_rules = 1, .forest_scrambler_rules = 1, .ages_water_rules = 1 }
#define COMBINED_SEASONS(LANG, lang) \
    { .id = "combined-1.5-" #lang "-seasons", .name = "Oracles Combined 1.5 (Oracle of Seasons)", .family = ORACLES_GAME_SEASONS, COMBINED_HALF(SEASONS, LANG), \
      .continuous_transitions = 1, .continuous_swim = 1, .zoom_out = 1, .continuous_diagonal = 1, .predict_collisions = 1, .continuous_steps = 1, .continuous_line = 1, \
      .swim_hold = 1, .cheats = 1, .text_instant = 1, \
      .guest_tables = &oracles_guest_tables_combined_seasons_##lang, .data_tables = &oracles_tables_combined_seasons, .room_flag_pages = combined_seasons_room_flag_pages, \
      .map_width = 16, .map_height = 16, .second_map_width = 11, .second_map_height = 8, \
      .maku_tree_rules = 1, .seasons_rules = 1, .lost_woods_rules = 1, .tileset_map_rules = 1 }

static const OraclesCompatProfile profiles[] = {
    { .id = "ages-original", .name = "Oracle of Ages", .revision = ORACLES_ROM_REVISION_AGES_US, .family = ORACLES_GAME_AGES,
      .continuous_transitions = 1, .continuous_swim = 1, .item_hotkeys = 1, .zoom_out = 1, .swim_hold = 1, .continuous_diagonal = 1, .predict_collisions = 1, .room_code_replay = 1, .continuous_steps = 1, .continuous_line = 1, .cheats = 1, .compact_text = 1, .host_text_box = 1, .text_size = 1, .text_instant = 1,
      .guest_tables = &oracles_guest_tables_ages, .data_tables = &oracles_tables_ages, .room_flag_pages = ages_room_flag_pages,
      .map_width = 14, .map_height = 14,
      .open_water_rules = 1, .maku_tree_rules = 1, .maku_isolation_rules = 1, .sea_level_rules = 1, .forest_scrambler_rules = 1, .ages_water_rules = 1,
      .language_bank = ORACLES_IMAGE_LANGUAGE_BANK },
    { .id = "seasons-original", .name = "Oracle of Seasons", .revision = ORACLES_ROM_REVISION_SEASONS_US, .family = ORACLES_GAME_SEASONS,
      .continuous_transitions = 1, .continuous_swim = 1, .item_hotkeys = 1, .zoom_out = 1, .swim_hold = 1, .continuous_diagonal = 1, .predict_collisions = 1, .room_code_replay = 1, .continuous_steps = 1, .continuous_line = 1, .cheats = 1, .compact_text = 1, .host_text_box = 1, .text_size = 1, .text_instant = 1,
      .guest_tables = &oracles_guest_tables_seasons, .data_tables = &oracles_tables_seasons, .room_flag_pages = seasons_room_flag_pages,
      .map_width = 16, .map_height = 16, .second_map_width = 11, .second_map_height = 8,   /* SUBROSIA_WIDTH, _HEIGHT */
      .maku_tree_rules = 1, .seasons_rules = 1, .lost_woods_rules = 1, .tileset_map_rules = 1,
      .language_bank = ORACLES_IMAGE_LANGUAGE_BANK },
    /* Moonrise: the rules its map and data were shown to follow, none inherited from Ages' family. */
    { .id = "moonrise-1.0.6", .name = "Moonrise Regalia 1.0.6", .revision = ORACLES_ROM_REVISION_MOONRISE_1_0_6, .family = ORACLES_GAME_AGES,
      .continuous_transitions = 1, .zoom_out = 1, .continuous_diagonal = 1, .predict_collisions = 1, .continuous_steps = 1, .continuous_line = 1, .compact_text = 1, .text_instant = 1, .cheats = 1,
      .guest_tables = &oracles_guest_tables_moonrise, .data_tables = &oracles_tables_moonrise, .room_flag_pages = moonrise_room_flag_pages,
      .text_size = 1, .host_text_box = 1,
      .map_width = 8, .map_height = 8,
      .open_water_rules = 1, .tileset_map_rules = 1, .tileset_override_rules = ORACLES_TILESET_OVERRIDE_BASE,
      .language_bank = 0x71 },   /* bank $70 holds its data; $71 to $7f are empty */
    /* Temple of Seasons: built on hack-base's Seasons engine, Holodrum kept with its seasons, its Lost Woods routing and the
     * Maku tree (temple-of-seasons/manifest.json); features qualified only once its routes held them. */
    { .id = "temple-1.073", .name = "Temple of Seasons 1.073", .revision = ORACLES_ROM_REVISION_TEMPLE_1_073, .family = ORACLES_GAME_SEASONS,
      .continuous_transitions = 1, .continuous_swim = 1, .swim_hold = 1, .zoom_out = 1, .continuous_diagonal = 1, .predict_collisions = 1, .continuous_steps = 1, .continuous_line = 1, .compact_text = 1, .text_instant = 1, .cheats = 1,
      .guest_tables = &oracles_guest_tables_temple, .data_tables = &oracles_tables_temple, .room_flag_pages = temple_room_flag_pages,
      .text_size = 1, .host_text_box = 1,
      /* Its maps are walled inside the grid by an outdoor filler tileset ($64, $e4) and bound its map screen's cursor:
       * Holodrum's columns 13 to 15 and rows 3 to 11, Subrosia's columns 4 to 10 and rows 0 to 7. */
      .map_width = 16, .map_height = 12, .map_left = 13, .map_top = 3,
      .second_map_width = 11, .second_map_height = 8, .second_map_left = 4, .second_map_top = 0,
      .maku_tree_rules = 1, .seasons_rules = 1, .lost_woods_rules = 1, .tileset_map_rules = 1, .scene_wait_rules = 1,
      .language_bank = ORACLES_IMAGE_LANGUAGE_BANK },   /* its 4 MiB leave $5f to $7f empty */
    /* Gifts of Kinomi: Ages' engine on hack-base, bound from a build of its published source
     * (gifts-of-kinomi/manifest.json).  Its 8 x 8 map is its source's OVERWORLD_WIDTH/HEIGHT; the rooms its
     * tilesets put outdoors past it are reached only by the transitions it routes itself.  Its seasons follow the room
     * pack (checkRoomPackAfterWarp_body, triggerFadeoutTransition) and its rod reloads the room: a byte of the coarse list,
     * none of Seasons' rules for the season the band holds.  Its routines of mapTransitionGroupTable are its own (its
     * Lost Woods, sword and shrine rooms, forest scrambler, sea, eye puzzle): each room of the table is taken as routing
     * elsewhere from the start, never a start for the room beyond it, and its answers are asked again after every
     * transition.  Its caves are interiors of small rooms joined by scrolls (3:38 below 3:28, entered from 0:37): the
     * drawn-back view shows them as the rooms off its map, in its whole band.  Features qualified only once its routes
     * held them. */
    { .id = "kinomi-1.1.2", .name = "Gifts of Kinomi 1.1.2", .revision = ORACLES_ROM_REVISION_KINOMI_1_1_2, .family = ORACLES_GAME_AGES,
      .continuous_transitions = 1, .continuous_swim = 1, .swim_hold = 1, .zoom_out = 1, .continuous_diagonal = 1, .predict_collisions = 1, .continuous_steps = 1, .continuous_line = 1, .compact_text = 1, .text_instant = 1, .cheats = 1,
      .guest_tables = &oracles_guest_tables_kinomi, .data_tables = &oracles_tables_kinomi, .room_flag_pages = kinomi_room_flag_pages,
      .text_size = 1, .host_text_box = 1,
      .map_width = 8, .map_height = 8,
      .open_water_rules = 1, .maku_tree_rules = 1, .tileset_map_rules = 1, .own_routing_rules = 1, .interior_area_rules = 1, .tileset_override_rules = ORACLES_TILESET_OVERRIDE_SEASONS, .ages_water_rules = 1,
      .language_bank = 0x72 },   /* banks $70 and $71 hold its data; $72 to $7f are empty */
    /* Oracles Combined 1.5, the release's five images: Ages' game in the first half, Seasons' in the second, the game
     * switching halves at Vasu's doors and when a file of Seasons is loaded (swapGame).  A half's tables are the same
     * in the five languages but for the text's bases.  Each half: the wide view, its transitions, neighbours and
     * zoom, with its game's rules (oracles-combined/ages/manifest.json, seasons/manifest.json), those of the sea, the
     * scrambled forest, the water, the seasons' rod and the Lost Woods taken from the game, no route of them there yet;
     * the transitions' swim (the flippers and the mermaid suit, which Seasons' half swims with as Ages' does), the swim's
     * hold, the cheats, the instant text and the item hotkeys in each half; the compact text, the host's box and the text's size,
     * in the language the image holds, its texts laid out a half at a time in the banks from $70 of each half (and read
     * from the three bases of its language); the other features not qualified yet.  The guest, the view and the
     * transitions follow the game from half to half (oracles_guest_follow_half), and so do the features the guest armed. */
    COMBINED_AGES(EN, en), COMBINED_AGES(FR, fr), COMBINED_AGES(DE, de), COMBINED_AGES(ES, es), COMBINED_AGES(IT, it),
    COMBINED_SEASONS(EN, en), COMBINED_SEASONS(FR, fr), COMBINED_SEASONS(DE, de), COMBINED_SEASONS(ES, es), COMBINED_SEASONS(IT, it)
};

const OraclesCompatProfile *oracles_compat_find(const OraclesRomInfo *info)
{
    if (!info || !info->known) return NULL;
    /* An image of two games always starts in its first half's game (Oracles Combined's sramBootstrap): that half's. */
    for (size_t i = 0; i < sizeof profiles / sizeof profiles[0]; i++)
        if (profiles[i].revision == info->revision && profiles[i].family == info->game && (!profiles[i].half_switch || profiles[i].half_game == 0u))
            return &profiles[i];
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
int oracles_compat_both_games_items(const OraclesCompatProfile *profile)
{
    return profile ? profile->both_games_items : 0;
}
int oracles_compat_continuous_swim(const OraclesCompatProfile *profile)
{
    return profile ? profile->continuous_swim : 0;
}
int oracles_compat_zoom_out(const OraclesCompatProfile *profile)
{
    return profile ? profile->zoom_out : 0;
}
int oracles_compat_swim_hold(const OraclesCompatProfile *profile)
{
    return profile ? profile->swim_hold : 0;
}
int oracles_compat_continuous_diagonal(const OraclesCompatProfile *profile)
{
    return profile ? profile->continuous_diagonal : 0;
}
int oracles_compat_continuous_steps(const OraclesCompatProfile *profile)
{
    return profile ? profile->continuous_steps : 0;
}
int oracles_compat_continuous_line(const OraclesCompatProfile *profile)
{
    return profile ? profile->continuous_line : 0;
}
int oracles_compat_predict_collisions(const OraclesCompatProfile *profile)
{
    return profile ? profile->predict_collisions : 0;
}

int oracles_compat_room_code_replay(const OraclesCompatProfile *profile)
{
    return profile ? profile->room_code_replay : 0;
}
int oracles_compat_cheats(const OraclesCompatProfile *profile)
{
    return profile ? profile->cheats : 0;
}
int oracles_compat_compact_text(const OraclesCompatProfile *profile)
{
    return profile ? profile->compact_text : 0;
}
int oracles_compat_host_text_box(const OraclesCompatProfile *profile)
{
    return profile ? profile->host_text_box : 0;
}
int oracles_compat_text_size(const OraclesCompatProfile *profile)
{
    return profile ? profile->text_size : 0;
}
int oracles_compat_text_instant(const OraclesCompatProfile *profile)
{
    return profile ? profile->text_instant : 0;
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
int oracles_compat_ages_water_rules(const OraclesCompatProfile *profile)
{
    return profile ? profile->ages_water_rules : 0;
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
int oracles_compat_interior_area_rules(const OraclesCompatProfile *profile)
{
    return profile ? profile->interior_area_rules : 0;
}
int oracles_compat_tileset_override_rules(const OraclesCompatProfile *profile)
{
    return profile ? profile->tileset_override_rules : 0;
}
int oracles_compat_late_line_writes(const OraclesCompatProfile *profile)
{
    return profile ? profile->late_line_writes : 0;
}
int oracles_compat_scene_wait_rules(const OraclesCompatProfile *profile)
{
    return profile ? profile->scene_wait_rules : 0;
}
unsigned oracles_compat_language_bank(const OraclesCompatProfile *profile)
{
    return profile ? profile->language_bank : 0;
}

unsigned oracles_compat_rom_bank_base(const OraclesCompatProfile *profile) { return profile ? profile->rom_bank_base : 0; }
unsigned oracles_compat_sram_code_banks(const OraclesCompatProfile *profile) { return profile ? profile->sram_code_banks : 0; }

int oracles_compat_half_switch(const OraclesCompatProfile *profile) { return profile ? profile->half_switch : 0; }
unsigned oracles_compat_half_game(const OraclesCompatProfile *profile) { return profile ? profile->half_game : 0; }

static const OraclesGuestSym absent = { ORACLES_GUEST_ABSENT, 0 };
OraclesGuestSym oracles_compat_current_game(const OraclesCompatProfile *profile) { return profile && profile->half_switch ? profile->current_game : absent; }
OraclesGuestSym oracles_compat_swap_game(const OraclesCompatProfile *profile) { return profile && profile->half_switch ? profile->swap_game : absent; }
OraclesGuestSym oracles_compat_swap_game_request(const OraclesCompatProfile *profile)
{
    return profile && profile->half_switch ? profile->swap_game_request : absent;
}

int oracles_compat_text_third_base(const OraclesCompatProfile *profile, unsigned *split, OraclesGuestSym *base)
{
    if (!profile || !profile->text_split_index_2) return 0;
    *split = profile->text_split_index_2;
    *base = profile->text_offset_3;
    return 1;
}

const OraclesCompatProfile *oracles_compat_half(const OraclesCompatProfile *profile, unsigned game)
{
    if (!profile || !profile->half_switch) return NULL;
    for (size_t i = 0; i < sizeof profiles / sizeof profiles[0]; i++)
        if (profiles[i].half_switch && profiles[i].revision == profile->revision && profiles[i].half_game == game) return &profiles[i];
    return NULL;
}
