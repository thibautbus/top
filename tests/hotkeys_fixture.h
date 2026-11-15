/* A snapshot of the inventory in normal play, for the ROM-free tests of the item hotkeys. */
#ifndef ORACLES_TESTS_HOTKEYS_FIXTURE_H
#define ORACLES_TESTS_HOTKEYS_FIXTURE_H

#include "guest.h"

#include <string.h>

static inline OraclesGuestInventoryState playable(OraclesGame game)
{
    OraclesGuestInventoryState s;
    memset(&s, 0, sizeof s);
    s.game = game;
    s.slot_count = 18; s.slingshot_seeds = 0xff;
    s.game_state = 2; s.cutscene_index = 1; s.cutscene_ingame = 1; s.cutscene_loading_room = 0; s.cutscene_onox_final_form = 0x13; s.intro_done = 1;
    s.num_inventory_items = 0x20; s.item_biggoron_sword = 0x0c; s.item_seed_satchel = 0x19; s.item_harp = 0x11;
    s.item_bomb = 0x03; s.item_bracelet = 0x16;
    s.item_seed_shooter = game == ORACLES_GAME_AGES ? 0x0f : 0x13;
    s.harp_song = game == ORACLES_GAME_AGES ? 1 : 0xff;
    s.obtained_seeds = 0x03; s.obtained_songs = 0x03;
    s.slots[0] = 0x05; s.slots[1] = 0x01;            /* sword on B, shield on A */
    s.menu_inventory = 1; s.specialobject_minecart = 0x0a; s.specialobject_raft = game == ORACLES_GAME_AGES ? 0x13 : 0xff;
    s.tilesetflag_bit_sidescroll = 5; s.tilesetflag_bit_underwater = game == ORACLES_GAME_AGES ? 6 : 0xff;
    s.slots[2] = 0x0a; s.slots[3] = 0x19; s.slots[4] = 0x11; s.slots[5] = 0x17;   /* switch hook, satchel, harp, feather */
    return s;
}

/* Oracles Combined's half `game`: twenty storage slots, both games' items in either half, the slingshot's own seeds, the
 * harp's song in Seasons' half too. */
static inline OraclesGuestInventoryState playable_combined(OraclesGame game)
{
    OraclesGuestInventoryState s = playable(game);
    s.slot_count = 22; s.slingshot_seeds = 0; s.item_slingshot = 0x13; s.item_seed_shooter = 0x0f; s.harp_song = 1;
    s.slots[6] = 0x0f;                               /* the shooter */
    s.slots[18] = 0x13; s.slots[19] = 0x07;          /* the slingshot and the rod past the sixteenth storage slot */
    return s;
}

#endif
