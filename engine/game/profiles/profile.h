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

/* The checkTilesetOverride an image's room beside a scroll follows (tileset_override_rules). */
enum { ORACLES_TILESET_OVERRIDE_BASE = 1, ORACLES_TILESET_OVERRIDE_SEASONS = 2 };

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
    int swim_hold;                   /* --swim-hold: the swim's strokes from a held button, the mermaid suit's direction and the flippers' A */
    int continuous_diagonal;         /* --continuous-diagonal: a diagonal held carries Link across the transitions' axis (the room's collisions) */
    int predict_collisions;          /* the room a scroll enters: its collisions predicted from the ROM's tables while it loads (the diagonal's carry) */
    int room_code_replay;            /* and its own code (applyRoomSpecificTileChanges) replayed there, the routines of the original game alone
                                        (the US images', and Oracles Combined's halves, which keep them) */
    int continuous_steps;            /* --continuous-steps: a step a frame through a transition as the keys give it, the frame that ends the scroll written (finishScrollingTransition) */
    int continuous_line;             /* --continuous-line: the scroll at the line between two rooms, at screenTransitionState2's four comparisons */
    int cheats;                      /* --cheats: the cheats' answers at the instructions that read Link's damage, the ammunition used and the rupees taken */
    int compact_text;                /* --text compact: the text box's lines composed in the font's own widths (drawLineOfText, displayNextTextCharacter) */
    int host_text_box;               /* the text box the host keeps and draws (dmaTextGfxBuffer, vblankDmaFunction): --text-box wide */
    int text_size;                   /* --text-size: the game's box enlarged from the core's image, where its variables place it (w7TextboxVramPos, w7d0cc), and the host's */
    int text_instant;                /* --text-speed instant: each line of the text box whole, at the reads of A and B that skip it (updateCharacterDisplayTimer, displayNextTextCharacter) */
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
    int ages_water_rules;            /* the water of Ages' engine: deep water and sea water swum with the mermaid suit alone, whirlpools */
    int sea_level_rules;             /* a dive or a surfacing crosses two groups (Ages) */
    int forest_scrambler_rules;      /* a forest whose rooms route by a scrambled order (Ages) */
    int lost_woods_rules;            /* rooms of the routing table route elsewhere for good (Seasons) */
    int own_routing_rules;           /* the routing table's routines are the game's own: its rooms are taken as routing elsewhere
                                        from the start, as Seasons', with no routing key (Gifts of Kinomi) */
    int tileset_map_rules;           /* a room of groups 0 and 1 is on its map by its tileset (outdoors) and its place within the map;
                                        the others (houses, a dungeon, areas past the map) are interiors */
    int tileset_override_rules;      /* with the expanded tilesets on Ages' engine, the checkTilesetOverride the room beside a scroll
                                        follows: ORACLES_TILESET_OVERRIDE_BASE, the base engine's (the Maku Tree's screen, Nuun
                                        Highlands' pack $7f: Moonrise Regalia), ORACLES_TILESET_OVERRIDE_SEASONS, and every pack
                                        from $7f by the season (Gifts of Kinomi); 0 none followed */
    int late_line_writes;            /* its lcdInterrupt reads a byte of its own before the per-line scroll write of behaviours 0 and 1
                                        (Oracles Combined's wFrHudBottom), which then falls inside the line's drawing on every line */
    int interior_area_rules;         /* the interiors (groups 2 and 3) are areas of small rooms joined by scrolls, its caves among them:
                                        laid out as a room off an overworld's map, in the drawn-back view's whole band (Gifts of Kinomi) */
    int scene_wait_rules;            /* interaction $22's subids 0 to 7 (Seasons' engine) start their script, which takes the controls, only
                                        past returnIfScrollMode01Unset, in normal play: never at a first frame the scroll beside is decided
                                        in, their room's own scene not a scene there (Temple of Seasons, its script its own) */
    /* A language's banks (docs/ROM_DATA_FORMATS.md, section 16): from this one, its index at its start, to bank $7f, banks
     * the image leaves empty (the extension of an original, which a language composes to 2 MiB). */
    uint8_t language_bank;
    /* Where the game's code lives (oracles_rom.h, address spaces): the ROM bank its own banks start at, $100 for the
     * Seasons half of Oracles Combined, 0 for every other image; and the banks of the cartridge's RAM it runs code from
     * at $a000-$bfff, a bit a bank, 0 for none: a hook there never fires, a routine there is never called. */
    uint16_t rom_bank_base;
    uint8_t sram_code_banks;
    /* An image of two games, each a half of it (Oracles Combined), a profile a half: `half_switch` set, `half_game` the
     * value of the byte that names the game running (`current_game`, wCurrentGame) while this half runs, and the
     * routine that switches to the other half (`swap_game`, swapGame), which the byte `swap_game_request` (wSwapGame,
     * the game asked for plus 2) sends the game to; all in bank 0's memory, common to the halves. */
    int half_switch;
    /* Each half's engine holds both games' items (Oracles Combined): the inventory common to the halves, the harp, the
     * rod, the gloves, the shooter and the slingshot (its seeds its own) in either, their icons on the status bar as
     * the half's bank2.s draws them. */
    int both_games_items;
    uint8_t half_game;
    OraclesGuestSym current_game, swap_game, swap_game_request;
    /* The text's third base (Oracles Combined's TEXT_OFFSET_3, getTextAddress): the internal groups from
     * `text_split_index_2` on count from `text_offset_3`, the CPU address of a bank; a split of 0 for none. */
    uint8_t text_split_index_2;
    OraclesGuestSym text_offset_3;
} OraclesCompatProfile;

/* Returns an original profile only for an authenticated, known ROM identity. */
const OraclesCompatProfile *oracles_compat_find(const OraclesRomInfo *info);
const char *oracles_compat_id(const OraclesCompatProfile *profile);
const char *oracles_compat_name(const OraclesCompatProfile *profile);
OraclesGame oracles_compat_family(const OraclesCompatProfile *profile);
int oracles_compat_continuous_transitions(const OraclesCompatProfile *profile);
int oracles_compat_item_hotkeys(const OraclesCompatProfile *profile);
int oracles_compat_both_games_items(const OraclesCompatProfile *profile);
int oracles_compat_continuous_swim(const OraclesCompatProfile *profile);
int oracles_compat_zoom_out(const OraclesCompatProfile *profile);
int oracles_compat_swim_hold(const OraclesCompatProfile *profile);
int oracles_compat_continuous_diagonal(const OraclesCompatProfile *profile);
int oracles_compat_continuous_steps(const OraclesCompatProfile *profile);
int oracles_compat_continuous_line(const OraclesCompatProfile *profile);
int oracles_compat_predict_collisions(const OraclesCompatProfile *profile);
int oracles_compat_room_code_replay(const OraclesCompatProfile *profile);
int oracles_compat_cheats(const OraclesCompatProfile *profile);
int oracles_compat_compact_text(const OraclesCompatProfile *profile);
int oracles_compat_host_text_box(const OraclesCompatProfile *profile);
int oracles_compat_text_size(const OraclesCompatProfile *profile);
int oracles_compat_text_instant(const OraclesCompatProfile *profile);
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
int oracles_compat_ages_water_rules(const OraclesCompatProfile *profile);
int oracles_compat_sea_level_rules(const OraclesCompatProfile *profile);
int oracles_compat_forest_scrambler_rules(const OraclesCompatProfile *profile);
int oracles_compat_lost_woods_rules(const OraclesCompatProfile *profile);
int oracles_compat_own_routing_rules(const OraclesCompatProfile *profile);
int oracles_compat_interior_area_rules(const OraclesCompatProfile *profile);
int oracles_compat_tileset_override_rules(const OraclesCompatProfile *profile);
int oracles_compat_late_line_writes(const OraclesCompatProfile *profile);
int oracles_compat_scene_wait_rules(const OraclesCompatProfile *profile);
/* The first of a language's banks in the image (ORACLES_IMAGE_LANGUAGE_BANK for an original), 0 for none. */
unsigned oracles_compat_language_bank(const OraclesCompatProfile *profile);
/* The ROM bank the game's banks start at (0 but for a half based past the image's start), and its code banks of the
 * cartridge's RAM (0: none). */
unsigned oracles_compat_rom_bank_base(const OraclesCompatProfile *profile);
unsigned oracles_compat_sram_code_banks(const OraclesCompatProfile *profile);
/* A half of an image of two games: 1, with its game's value of the byte that names the game running and the addresses
 * of that byte, of swapGame and of its request; 0 for every other image. */
int oracles_compat_half_switch(const OraclesCompatProfile *profile);
unsigned oracles_compat_half_game(const OraclesCompatProfile *profile);
OraclesGuestSym oracles_compat_current_game(const OraclesCompatProfile *profile);
OraclesGuestSym oracles_compat_swap_game(const OraclesCompatProfile *profile);
OraclesGuestSym oracles_compat_swap_game_request(const OraclesCompatProfile *profile);
/* The profile of the half of the same image whose game runs while the byte that names it holds `game`: the profile
 * itself for its own; NULL for an image of one game, or a value no half has. */
const OraclesCompatProfile *oracles_compat_half(const OraclesCompatProfile *profile, unsigned game);
/* The text's third base: 1 with its first internal group and its bank and address, 0 for an image of two bases. */
int oracles_compat_text_third_base(const OraclesCompatProfile *profile, unsigned *split, OraclesGuestSym *base);

#ifdef __cplusplus
}
#endif

#endif
