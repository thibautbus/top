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
    s.game_state = 2; s.cutscene_index = 1; s.cutscene_ingame = 1; s.cutscene_loading_room = 0; s.cutscene_onox_final_form = 0x13; s.intro_done = 1;
    s.num_inventory_items = 0x20; s.item_biggoron_sword = 0x0c; s.item_seed_satchel = 0x19; s.item_harp = 0x11;
    s.item_seed_shooter = game == ORACLES_GAME_AGES ? 0x0f : 0x13;
    s.harp_song = game == ORACLES_GAME_AGES ? 1 : 0xff;
    s.obtained_seeds = 0x03; s.obtained_songs = 0x03;
    s.slots[0] = 0x05; s.slots[1] = 0x01;            /* sword on B, shield on A */
    s.menu_inventory = 1; s.specialobject_minecart = 0x0a; s.specialobject_raft = game == ORACLES_GAME_AGES ? 0x13 : 0xff;
    s.tilesetflag_bit_sidescroll = 5; s.tilesetflag_bit_underwater = game == ORACLES_GAME_AGES ? 6 : 0xff;
    s.slots[2] = 0x0a; s.slots[3] = 0x19; s.slots[4] = 0x11; s.slots[5] = 0x17;   /* switch hook, satchel, harp, feather */
    return s;
}

#endif
