/* Guest bus, hooks and register journal (docs/GAME_HOOKS.md).
 *
 * A guest attaches to a core and gives the host:
 *   - reads by physical address (WRAM bank and offset, VRAM bank, OAM, HRAM,
 *     IO) through the core's direct access, never through the guest CPU bus;
 *   - events at the entry or return of game functions, at writes to watched
 *     variables, and at the core's vblank;
 *   - a journal of the display register writes of the current frame, each
 *     stamped with LY and the STAT mode at the time of the write;
 *   - the live-WRAM fingerprint of the harness, which excludes only the dead
 *     stack bytes below each thread's stack pointer.
 *
 * Observation hooks never write to the guest.  The write paths are seven closed
 * transactions: the call's (a mod's host scene, further below), the
 * inventory's (the item hotkeys, further below), the language's, the swim's
 * hold, the cheats and the compact text (further below), and the
 * transition's: at the return of
 * drawAllSprites, a policy may ask for a bounded change of the scroll
 * machine's step and of Link's position and walking animation, applied here
 * after its own checks; it never receives a writable view of the RAM.
 * Return hooks fire at the return address captured on entry, so a tail jump
 * out of the function is covered. */
#ifndef ORACLES_GUEST_H
#define ORACLES_GUEST_H

#include "core.h"
#include "profile.h"
#include "guest_tables.h"
#include "oracles_text.h"
#include "rom.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OraclesGuest OraclesGuest;

typedef enum OraclesGuestEventType {
    ORACLES_EVENT_FRAME_DONE = 1,        /* entry of drawAllSprites: the gameplay state of the frame is complete */
    ORACLES_EVENT_ROOM_ENTER,            /* entry of initializeRoom */
    ORACLES_EVENT_ROOM_INITIALIZED,      /* return of initializeRoom */
    ORACLES_EVENT_INTERACTION_CREATED,   /* return of getFreeInteractionSlot with a slot found (Z set); h = its slot */
    ORACLES_EVENT_ENEMY_CREATED,         /* return of getFreeEnemySlot_uncounted with a slot found (Z set): getFreeEnemySlot calls it, and parseObjectData uses it alone for obj_SpecificEnemyB and obj_ItemDrop */
    ORACLES_EVENT_PART_CREATED,          /* return of getFreePartSlot with a slot found (Z set) */
    ORACLES_EVENT_RANDOM_PLACEMENT,      /* entry of objectDataOp6, the opcode that places enemies at random */
    ORACLES_EVENT_RANDOM_PLACEMENT_DONE, /* entry of parseGivenObjectData, where every opcode jumps back: an object created since RANDOM_PLACEMENT owes its position to the RNG */
    ORACLES_EVENT_OBJECT_DRAW,           /* entry of drawAllSpritesUnconditionally@drawObject: a = the object's type offset, (hl+1) = its slot */
    ORACLES_EVENT_OBJECT_DRAW_DONE,      /* its return: the OAM entries written in between are that object's */
    ORACLES_EVENT_TERRAIN_EFFECT,        /* entry of _drawObjectTerrainEffects: every object that asks for terrain effects, whether one is drawn or not */
    ORACLES_EVENT_OAM_BLOCK,             /* entry of func_0eda: writes a queued block into wOam, terrain effects after the loop of objects */
    ORACLES_EVENT_OAM_BLOCK_DONE,        /* its return */
    ORACLES_EVENT_TRANSITION,            /* write of wScreenTransitionDirection by @startTransition (state 3, scroll mode 4); value = direction */
    ORACLES_EVENT_WARP,                  /* return of applyWarpDest */
    ORACLES_EVENT_TEXT,                  /* entry of showText */
    ORACLES_EVENT_TEXT_CHOICE,           /* write of wSelectedTextOption */
    ORACLES_EVENT_FLAG_WRITE,            /* write in the global or room flags */
    ORACLES_EVENT_TREASURE,              /* entry of giveTreasure */
    ORACLES_EVENT_MENU,                  /* entry of openMenu */
    ORACLES_EVENT_SAVE,                  /* entry of saveFile */
    ORACLES_EVENT_INPUT,                 /* return of pollInput */
    ORACLES_EVENT_ENEMY_KILLED,          /* write of 0 into the health of a living enemy; h = its slot */
    ORACLES_EVENT_VBLANK,                /* the core's vblank hook; value = its OraclesVblankType */
    ORACLES_EVENT_TILE_SUBSTITUTIONS,      /* entry of applyAllTileSubstitutions (ghost instance) */
    ORACLES_EVENT_TILE_SUBSTITUTIONS_DONE, /* its return */
    ORACLES_EVENT_FRAME_DRAWN,             /* return of drawAllSprites: the frame's logic is over, the transition transaction runs here */
    ORACLES_EVENT_CHECK_ROOM_PACK,         /* entry of checkRoomPack (ghost instance, Seasons: the area a scroll enters) */
    ORACLES_EVENT_OBJECT_GFX_LOAD,         /* entry of loadObjectGfxHeaderToSlot4 (ghost instance: Seasons' substitutions load Subrosia's object graphics, no tile state read) */
    ORACLES_EVENT_OBJECT_GFX_LOAD_DONE,    /* its return */
    ORACLES_EVENT_ANIMATIONS,              /* entry of updateAnimations: one step of the room's tile animation */
    ORACLES_EVENT_OBJECTS_UPDATED,         /* return of updateAllObjects: the objects of the room have run their own code once more */
    ORACLES_EVENT_STATUS_BAR_CHECKED,      /* return of checkReloadStatusBarGraphics; from its one call in mainThreadStart, the inventory transaction runs here */
    ORACLES_EVENT_EQUIPPED_GFX_LOADED,     /* entry of loadEquippedItemGfx: the status bar reloads the icons of the two buttons */
    ORACLES_EVENT_USE_ITEMS,               /* entry of checkUseItems: the game reads the two item buttons of this turn (the item hotkeys' `use` mode) */
    ORACLES_EVENT_ITEM_STARTED,            /* entry of initializeParentItem: an item starts; e = its identifier, d = the button that fired it */
    ORACLES_EVENT_STATUS_BAR_CHECK,        /* entry of checkReloadStatusBarGraphics; from mainThreadStart, the call transaction arms its return there */
    ORACLES_EVENT_FILE_OPERATION,          /* entry of fileManagementFunction, hooked by the mods: c = 0 create, 1 save, 2 load, 3 erase the file of hActiveFileSlot */
    ORACLES_EVENT_TEXT_ADDRESS,            /* return of getTextAddress, with a language composed: the language transaction runs here */
    ORACLES_EVENT_MERMAID_STROKE,          /* the `and` after linkUpdateVelocity@mermaidSuit's read of wGameKeysJustPressed, with the swim's hold armed: its transaction runs here */
    ORACLES_EVENT_FLIPPERS_STROKE,         /* the `and` after linkUpdateFlippersSpeed@flippersState0's read of wGameKeysJustPressed, the same */
    ORACLES_EVENT_CHEAT_DAMAGE,            /* linkApplyDamage's `or a` of the damage it read, with a cheat armed: the cheats' transaction runs here */
    ORACLES_EVENT_CHEAT_AMMO,              /* the `ld (hl),a` of decNumBombs' end, the same */
    ORACLES_EVENT_CHEAT_RUPEES,            /* removeRupeeValue's `call getRupeeValue`, the same */
    ORACLES_EVENT_TEXT_LINE,               /* entry of drawLineOfText, with the compact text armed: its transaction runs here, */
    ORACLES_EVENT_TEXT_CHARACTER,          /* at setLineTextBuffers' entry, */
    ORACLES_EVENT_TEXT_LINE_DRAWN,         /* at drawLineOfText's return, */
    ORACLES_EVENT_TEXT_CURSOR_PLACE,       /* at textControlCodeC_2's `ld (hl),b`, which keeps a choice's cursor place, */
    ORACLES_EVENT_TEXT_COLUMN_SHOWN,       /* and at displayNextTextCharacter's `ld d,>w7NextTextColumnToDisplay`, a column written */
    ORACLES_EVENT_TEXT_GFX_DMA,            /* entry of dmaTextGfxBuffer, with the host's text box armed: a = 0 the top line, 2 the bottom */
    ORACLES_EVENT_VBLANK_DMA,              /* entry of vblankDmaFunction, the same: the queued DMA it runs, its entry under SP */
    ORACLES_EVENT_TEXT_SKIP_TEST,          /* the `and` after updateCharacterDisplayTimer's read of wKeysJustPressed, with the instant text armed: its transaction runs here, */
    ORACLES_EVENT_TEXT_SKIP_LOOP           /* and at displayNextTextCharacter's `bit 0,a` of w7d0c1, the test of its loop to the line's end */
} OraclesGuestEventType;

typedef struct OraclesGuestEvent {
    OraclesGuestEventType type;
    uint32_t frame;        /* host frame counter, as told by oracles_guest_set_frame */
    uint16_t pc;           /* instruction address for function events */
    uint16_t address;      /* written address for write events */
    uint8_t value;         /* written value, or vblank type */
    /* CPU registers at the event, for the identifiers the game passes in them */
    uint8_t a, b, c, d, e, h, l;
    uint8_t f;             /* flags: bit 7 Z, bit 4 C */
} OraclesGuestEvent;

typedef void (*OraclesGuestEventFn)(void *opaque, const OraclesGuestEvent *event);

/* The game's values the transition policy, its bus and the view share (constants/common/tilesetFlags.s,
 * linkAnimations.s; link.s linkUpdateSwimming). */
#define ORACLES_TILESETFLAG_SIDESCROLL 0x20u    /* a side view */
#define ORACLES_TILESETFLAG_UNDERWATER 0x40u    /* Ages' sea floor; unused in Seasons */
#define ORACLES_LINK_ANIM_MODE_SWIM 0x0bu
#define ORACLES_LINK_ANIM_MODE_DIVE 0x0cu
#define ORACLES_LINK_ANIM_MODE_WALK 0x10u
#define ORACLES_SWIMMING_NORMAL 0x03u           /* wLinkSwimmingState's bits 0-3 in linkUpdateSwimming's state 3, bit 6 (lava) clear; bit 7 (diving) free */
#define ORACLES_LINK_SWIM_LOWER 6u              /* px: the lowest of Link's swimming poses below the walk's line, measured against the OAM */
#define ORACLES_TRANSITION_WALK_STEP_MAX 0x100  /* 8.8: the most the transition transaction moves Link along an axis in a frame on foot, a pixel */
#define ORACLES_TRANSITION_SWIM_STEP_MAX 0x160  /* in the water: SPEED_160 along an axis, the fastest swim (the mermaid suit with the swimmer's ring) */

/* The transition transaction.  The state is a snapshot of the
 * scroll machine and of Link's gait at the return of drawAllSprites; the
 * mutation is what the policy asks for.  The guest applies it only when the
 * state is one where it is safe (a scrolling transition of a small room in
 * normal play, Link in his normal state, no item, no text), and only within
 * the bounds below. */
typedef struct OraclesGuestTransitionState {
    OraclesGame game;
    uint8_t active_group, room_is_large, game_state, cutscene_index, text_is_active, tileset_flags;
    uint8_t scroll_mode, transition_state, transition_substate, transition_phase, transition_direction;
    uint8_t screen_scroll_delta, screen_scroll_counter, scroll_alignment;   /* alignment: SCX & 7 or SCY & 7 for the axis */
    uint8_t link_force_state, link_object_index, link_id, link_state, link_visible, link_anim_mode;
    uint8_t link_anim_counter, link_anim_parameter, link_animation_frame;
    uint16_t link_anim_pointer;
    uint8_t link_in_air, link_swimming_state, link_grab_state, magnet_glove_state, link_immobilized;
    uint8_t link_pushing_direction, link_playing_instrument, link_turning_disabled, force_link_push_animation;
    uint8_t using_shield, pegasus_seed_counter_nonzero, active_item, link_invincibility_counter, link_knockback_counter, link_stun_counter;
    uint16_t camera_x, camera_y, link_x, link_y;                              /* 8.8 */
    uint8_t room_width, room_height;                                          /* in 8x8 tiles */
    const uint8_t *room_collisions;                                           /* 176 bytes: the room being entered once it is loaded */
    const uint8_t *room_layout;                                               /* 176 bytes: its tiles (wRoomLayout), loaded with the collisions */
    const uint8_t *play_collisions, *play_layout;                             /* the room's own in normal play (wScrollMode 1), at the return of drawAllSprites: at the frame
                                                                                 that ends a scroll, the room it entered; NULL otherwise */
    uint8_t active_collisions;                                                /* wActiveCollisions: the list of tileTypesTable its tiles are looked up in */
    uint8_t tile_types_bank;                                                  /* tileTypesTable in the ROM, 0xff when the game has none */
    uint16_t tile_types_table;
    uint8_t link_speed, link_var2f;                                           /* w1Link.speed (an objectSpeedTable row, times 5), var2f (Ages: bit 6 the mermaid suit, bit 7 the sea floor) */
    /* What screenTransitionState2@transition reads in normal play besides the direction held (--continuous-swim): */
    uint8_t transition_direction_raw;                                         /* wScreenTransitionDirection, bit 7 the forced transition */
    uint8_t link_enabled, link_move_angle;                                    /* w1Link.enabled, w1Link.angle (his motion's, 32 steps; bit 7 none) */
    uint8_t link_input_angle;                                                 /* wLinkAngle: the direction held, 32 steps; bit 7 none */
    uint8_t link_direction;                                                   /* w1Link.direction: the way he faces, 0 up, 1 right, 2 down, 3 left */
    uint8_t link_stroke;                                                      /* w1Link.var35: a stroke of the flippers, 1 speeding up, 2 slowing down (linkUpdateFlippersSpeed) */
    uint8_t screen_transition_delay, disable_screen_transitions, hole_or_conveyor;   /* wScreenTransitionDelay, wDisableScreenTransitions, wcc92 */
    uint8_t boundary_x, boundary_y, active_room, active_tile_index;           /* wScreenTransitionBoundaryX/Y, wActiveRoom, wActiveTileIndex */
    uint8_t map_width, map_height;                                            /* the overworld's (OVERWORLD_WIDTH/HEIGHT) */
    uint8_t animation_data_bank;
    uint8_t palette_thread_mode, tileset_palette, loaded_tileset_palette;   /* wPaletteThread_mode, wTilesetPalette, wLoadedTilesetPalette */
    /* What the load of the room entered reads besides the fields above (active_group, active_room, tileset_flags,
     * active_collisions) to build its layout and collisions, each written before wScrollMode is 8 or left alone by the
     * load: the prediction of them (room_predict.h). */
    struct {
        uint8_t tileset_layout, tileset_layout_group;   /* wTilesetLayout, wTilesetLayoutGroup (loadTilesetData) */
        uint8_t room_flags;                             /* the room's own flags (getThisRoomFlags) */
        uint8_t room_state_modifier;                    /* the season (Seasons) */
        uint8_t is_linked_game;                         /* applySingleTileChanges (Ages) */
        uint8_t portal_group, portal_room, portal_pos;  /* replaceBreakableTileOverPortal (Ages) */
        uint8_t warp_transition, warp_dest_pos, link_time_warp_tile;   /* the time warp's arrival and return (Ages) */
        const uint8_t *global_flags, *gasha_spots_planted;   /* wGlobalFlags, wGashaSpotsPlantedBitset */
        uint16_t global_flags_size, gasha_spots_planted_size;
        /* the game's constants (generated tables), 0xff for none */
        uint8_t chest_opened_tile, roomflag_item, timewarp_transition, pollution_fixed_flag, finished_game_flag, temple_lava_flag,
            pirate_ship_flag, intro_done_flag;
    } load;
    uint32_t frame;
    uint32_t frames_not_asked;             /* frames since the policy was last asked where it was not (a vblank of an unfinished frame outside a load): no break in the frames */
} OraclesGuestTransitionState;

typedef struct OraclesGuestTransitionMutation {
    uint8_t set_scroll_delta;              /* write wcd14: +-4 (the game's) or +-8 */
    uint8_t scroll_delta;
    int16_t link_x_delta, link_y_delta;    /* 8.8, at most one pixel each on foot, at most the fastest swim swimming */
    uint8_t update_walk_animation;         /* write Link's animation counter, parameter, pointer and frame */
    uint8_t link_anim_counter, link_anim_parameter, link_animation_frame;
    uint16_t link_anim_pointer;
    uint8_t refresh_bg_palettes;           /* BG palettes (bits 2-7) marked dirty in hDirtyBgPalettes, out of a transition, the palette thread stopped and the room's palettes loaded */
    uint8_t force_transition;              /* --continuous-swim, in normal play: 1 raise wScreenTransitionDirection's bit 7 to `forced_direction`, 2 lower it */
    uint8_t forced_direction;              /* 0 up, 1 right, 2 down, 3 left */
    uint8_t carried;                       /* the delta across the transition's axis is a carry (--continuous-diagonal): counted when applied */
    uint8_t set_facing;                    /* --continuous-steps, on foot in a scroll: write w1Link.direction (`facing`, 0 to 3), the way the keys turn him */
    uint8_t facing;
    uint8_t scroll_end;                    /* --continuous-steps: the move is Link's step of the frame whose logic ended the scroll, back in normal play after
                                              linkState01 skipped his own: applied at the entry of its drawAllSprites only */
    uint8_t set_respawn;                   /* --continuous-steps, at that frame: write wLinkLocalRespawnY/X (`respawn_y`, `respawn_x`, pixels in the room entered),
                                              which finishScrollingTransition took from where the policy's steps left Link */
    uint8_t respawn_y, respawn_x;
} OraclesGuestTransitionMutation;

typedef void (*OraclesGuestTransitionFn)(void *opaque, const OraclesGuestTransitionState *state, OraclesGuestTransitionMutation *mutation);
typedef void (*OraclesGuestTransitionResetFn)(void *opaque);

/* Installs the policy (NULL removes it); `reset` is called when the guest's
 * execution state is reset (a savestate load), so the policy forgets its
 * progress. */
void oracles_guest_set_transition_policy(OraclesGuest *guest, OraclesGuestTransitionFn policy, OraclesGuestTransitionResetFn reset, void *opaque);
/* With --continuous-swim, the transaction also takes Link swimming at the surface in his normal swimming state
 * (linkUpdateSwimming's state 3, diving or not, never in lava), his swimming or diving animation advanced. */
void oracles_guest_set_transition_swim(OraclesGuest *guest, int on);
/* With --continuous-steps, the frame whose logic ended a scroll is asked about at the entry of drawAllSprites instead of
 * its return, and the transaction takes Link's step there, in normal play: the game draws him, and his terrain's effects
 * with him, where he then is. */
void oracles_guest_set_transition_steps(OraclesGuest *guest, int on);
/* Whether a policy is installed: the continuous transitions are on. */
int oracles_guest_has_transition_policy(const OraclesGuest *guest);
/* The frame counter the host gave the guest (oracles_guest_set_frame). */
uint32_t oracles_guest_frame(const OraclesGuest *guest);
/* Transaction writes made at a vblank (a frame the game did not finish), by wScreenTransitionState (0 to 7). */
void oracles_guest_vblank_write_counts(const OraclesGuest *guest, unsigned out[8]);
/* Vblanks of a frame the game did not finish, outside a room load, where the policy was not asked. */
unsigned oracles_guest_vblank_policy_skipped(const OraclesGuest *guest);
/* The carries across a transition's axis the transaction applied (OraclesGuestTransitionMutation.carried). */
unsigned oracles_guest_transition_carried(const OraclesGuest *guest);
/* The steps of the frame that ends a scroll the transaction applied (--continuous-steps). */
unsigned oracles_guest_transition_end_steps(const OraclesGuest *guest);
/* The respawns in the room entered the transaction moved where the game would have left Link (--continuous-steps). */
unsigned oracles_guest_transition_respawns(const OraclesGuest *guest);
/* wScreenTransitionState as the policy was last asked about it, given back by the policy's savestate data after a load
 * (oracles_guest_reset_execution_state takes it from the state loaded otherwise). */
void oracles_guest_set_transition_last_state(OraclesGuest *guest, uint8_t state);

/* The inventory transaction (item hotkeys): the second closed
 * write path.  At the return of checkReloadStatusBarGraphics from its one
 * call in mainThreadStart (after the frame's only clearing of
 * wStatusBarNeedsRefresh, before resumeThreadNextFrame), a policy receives a
 * snapshot of the eighteen inventory slots, the variants and what says
 * whether the player could open the inventory, and may ask for one
 * operation: an exchange of two slots, one of them a button, and a variant.
 * The guest applies it only after its own checks, to states the game's own
 * menu can produce, and tells the policy what it did. */
#define ORACLES_INVENTORY_SLOTS 18u        /* 0: B, 1: A, 2 to 17: wInventoryStorage */
#define ORACLES_INVENTORY_SLOT_B 0u
#define ORACLES_INVENTORY_SLOT_A 1u
#define ORACLES_BUTTON_A 0x01u             /* the joypad bits the game keeps in Item.var03 */
#define ORACLES_BUTTON_B 0x02u

typedef enum OraclesGuestVariant { ORACLES_VARIANT_NONE = 0, ORACLES_VARIANT_SATCHEL, ORACLES_VARIANT_SHOOTER, ORACLES_VARIANT_HARP } OraclesGuestVariant;

typedef struct OraclesGuestInventoryState {
    OraclesGame game;
    uint32_t frame;
    uint8_t slots[ORACLES_INVENTORY_SLOTS];
    uint8_t satchel_seeds, shooter_seeds, harp_song;       /* harp_song 0xff when the game has no harp */
    uint8_t obtained_seeds, obtained_songs;                /* bit n: seed n (0 to 4), song n + 1 (1 to 3) obtained */
    uint8_t game_state, cutscene_index, opened_menu_type, text_is_active, link_death_trigger, intro_done,
            use_simulated_input, in_boxing_match, minigame_controller, scroll_mode, menu_disabled,
            disable_link_collisions_and_menu, link_playing_instrument, dont_update_status_bar, status_bar_needs_refresh;
    struct { uint8_t enabled, id, button; } parents[5];   /* w1ParentItem2 to 5, then w1WeaponItem: the items in use and the button each answers to */
    /* what checkUseItems reads to decide which of A and B use an item in this state (fact 10) */
    uint8_t items_disabled, in_shop, link_in_air, link_in_spinner, link_grabbed, link_grab_state, link_climbing_vine,
            tileset_flags, link_swimming_state, link_var2f, disabled_objects, link_object_index, companion_id, palette_thread_mode;
    /* constants of the game, for the policies (generated tables) */
    uint8_t num_inventory_items, item_biggoron_sword, item_seed_satchel, item_seed_shooter, item_harp, cutscene_ingame, cutscene_onox_final_form,
            tilesetflag_bit_sidescroll, tilesetflag_bit_underwater,   /* the second 0xff in Seasons, which has no underwater tileset */
            menu_inventory, specialobject_minecart, specialobject_raft, cutscene_loading_room;
} OraclesGuestInventoryState;

typedef struct OraclesGuestInventoryOp {
    uint8_t swap;                          /* exchange slot_a and slot_b */
    uint8_t slot_a, slot_b;
    uint8_t expect;                        /* a replayed exchange: the two slots must hold expect_a and expect_b */
    uint8_t expect_a, expect_b;
    uint8_t variant, variant_value;        /* OraclesGuestVariant; seeds 0 to 4, song 1 to 3 */
} OraclesGuestInventoryOp;

typedef enum OraclesGuestInventoryVerdict {
    ORACLES_INVENTORY_APPLIED = 0,
    ORACLES_INVENTORY_NOTHING,             /* no operation asked, or one that changes nothing */
    /* refusals: a lasting condition, or an operation the menu could not make */
    ORACLES_INVENTORY_REFUSED_NOT_IN_PLAY, ORACLES_INVENTORY_REFUSED_MENU_OPEN, ORACLES_INVENTORY_REFUSED_TEXT,
    ORACLES_INVENTORY_REFUSED_DEATH, ORACLES_INVENTORY_REFUSED_INTRO, ORACLES_INVENTORY_REFUSED_SIMULATED_INPUT,
    ORACLES_INVENTORY_REFUSED_MINIGAME, ORACLES_INVENTORY_REFUSED_BOXING, ORACLES_INVENTORY_REFUSED_BIGGORON,
    ORACLES_INVENTORY_REFUSED_INCONSISTENT, ORACLES_INVENTORY_REFUSED_OPERATION, ORACLES_INVENTORY_REFUSED_EXPECTATION,
    ORACLES_INVENTORY_REFUSED_VARIANT,
    /* postponements: a passing condition, the policy may ask again */
    ORACLES_INVENTORY_POSTPONED_SCROLL, ORACLES_INVENTORY_POSTPONED_MENU_DISABLED, ORACLES_INVENTORY_POSTPONED_COLLISIONS_DISABLED,
    ORACLES_INVENTORY_POSTPONED_INSTRUMENT, ORACLES_INVENTORY_POSTPONED_ITEM_IN_USE, ORACLES_INVENTORY_POSTPONED_STATUS_BAR_HIDDEN,
    ORACLES_INVENTORY_POSTPONED_NO_BUTTON,   /* the `use` mode: no item button is read in this state (fact 10); the policy's own, the guest never answers it */
    ORACLES_INVENTORY_VERDICTS
} OraclesGuestInventoryVerdict;

const char *oracles_guest_inventory_verdict_name(OraclesGuestInventoryVerdict verdict);
int oracles_guest_inventory_verdict_postpones(OraclesGuestInventoryVerdict verdict);
/* The lasting condition that refuses any operation in this state, or ORACLES_INVENTORY_APPLIED when there is none.  Pure. */
OraclesGuestInventoryVerdict oracles_guest_inventory_refusal(const OraclesGuestInventoryState *state);
/* Pure, testable without a ROM: what the guest would answer to `op` in `state`, and the exchange itself on a copy of the slots. */
OraclesGuestInventoryVerdict oracles_guest_inventory_check(const OraclesGuestInventoryState *state, const OraclesGuestInventoryOp *op);
/* The item buttons checkUseItems would read in this state (ORACLES_BUTTON_A, ORACLES_BUTTON_B, both or none), as its
 * code decides it (fact 10); none also where A and B do something else than use an item: on a mount, in a shop, and
 * while Link's update stops before it (wDisabledObjects & $81).  Pure. */
unsigned oracles_guest_item_buttons(const OraclesGuestInventoryState *state);
/* The variant an item carries now (seeds 0 to 4, song 1 to 3), or 0xff when it has none.  Pure. */
uint8_t oracles_guest_item_variant(const OraclesGuestInventoryState *state, uint8_t item);
/* Whether every non-empty identifier of the eighteen slots stands once. */
int oracles_guest_inventory_consistent(const uint8_t slots[ORACLES_INVENTORY_SLOTS]);

typedef void (*OraclesGuestInventoryFn)(void *opaque, const OraclesGuestInventoryState *state, OraclesGuestInventoryOp *op);
/* What the guest did with the operation asked at this write point (not called when none was asked). */
typedef void (*OraclesGuestInventoryResultFn)(void *opaque, const OraclesGuestInventoryState *before, const OraclesGuestInventoryOp *op, OraclesGuestInventoryVerdict verdict);
typedef void (*OraclesGuestInventoryResetFn)(void *opaque);
/* Installs the policy (NULL removes it) and arms the write point, found in the
 * user's ROM: the one `call checkReloadStatusBarGraphics` of mainThreadStart.
 * Returns -1 when the ROM has none or several, or the hook table is full. */
int oracles_guest_set_inventory_policy(OraclesGuest *guest, OraclesGuestInventoryFn policy, OraclesGuestInventoryResultFn result,
                                       OraclesGuestInventoryResetFn reset, void *opaque);
/* The snapshot the policy would receive now (any moment: the harness, the launcher's bindings). */
void oracles_guest_inventory_state(OraclesGuest *guest, OraclesGuestInventoryState *out);
/* The inventory open on its item submenu and waiting for an input (point 7): 1 with the storage slot under the cursor
 * (2 to 17), 0 otherwise — the seed and song submenus included, whose ranks are the game's own computation. */
int oracles_guest_inventory_cursor(OraclesGuest *guest, unsigned *slot);
/* Exchanges applied, and violations of the inventory's consistency seen after one (none expected). */
void oracles_guest_inventory_counts(const OraclesGuest *guest, unsigned *applied, unsigned *violations);

/* The call transaction (a mod's host scene): the third closed write
 * path.  The host never writes the game's variables: it asks the game to run
 * one of its own routines, from a closed catalogue, with a parameter checked
 * here, so that a mod does whatever the game's own code does (its sound, its
 * status bar, its counters, its lists), and no more.
 *
 * The routine runs in the one call of checkReloadStatusBarGraphics that
 * mainThreadStart makes (the inventory's write point), whose return is turned
 * into a jump to the routine: one word, the routine's address, is pushed
 * under the return address, and the routine returns where
 * checkReloadStatusBarGraphics would have, to `call resumeThreadNextFrame`,
 * which reads no register.  This is the one place the host writes the stack
 * and SP (the one exception to the rule of the hooks).  One call a frame,
 * only in a state where the inventory transaction has no lasting refusal (in
 * play, no text, no menu, no death, no cutscene); a call waits in the queue
 * otherwise. */
typedef enum OraclesGuestRoutine {
    ORACLES_ROUTINE_GIVE_TREASURE = 1,     /* giveTreasure: a = a named treasure, c = its parameter (treasureCollectionBehaviours.s) */
    ORACLES_ROUTINE_LOSE_TREASURE,         /* loseTreasure: a = a named treasure */
    ORACLES_ROUTINE_REMOVE_RUPEES,         /* removeRupeeValue: a = a rupee value (rupeeValues.s) */
    ORACLES_ROUTINE_PLAY_SOUND,            /* playSound: a = a named sound or music */
    ORACLES_ROUTINE_SET_GLOBAL_FLAG,       /* setGlobalFlag: a = a named global flag */
    ORACLES_ROUTINE_UNSET_GLOBAL_FLAG      /* unsetGlobalFlag: a = a named global flag */
} OraclesGuestRoutine;

typedef struct OraclesGuestCall {
    OraclesGuestRoutine routine;
    uint8_t a, c;
} OraclesGuestCall;

#define ORACLES_GUEST_CALLS 16u

/* Arms the write point, found in the user's ROM as the inventory's; -1 when the
 * image's profile does not qualify the inventory's addresses, the ROM has no
 * such point, or the hook table is full.  Called once, before any call is queued. */
int oracles_guest_arm_calls(OraclesGuest *guest);
/* The ROM image the core runs, flat (bank n from n * $4000), as the call transaction reads the treasures' tables in it. */
typedef struct OraclesGuestRom {
    const uint8_t *data;
    size_t size;
} OraclesGuestRom;
OraclesGuestRom oracles_guest_rom(OraclesGuest *guest);
/* Pure: how giveTreasure applies `treasure`'s parameter, read in `rom` (the low nibble of the second byte of its
 * treasureCollectionBehaviourTable entry); -1 when the treasure or the table is out of the image. */
int oracles_guest_treasure_mode(const OraclesGuestTables *tables, const OraclesGuestRom *rom, uint8_t treasure);
/* Pure: the rupees rupee value `value` stands for, read in `rom` (getRupeeValue's BCD word); -1 when out of range. */
int oracles_guest_rupee_amount(const OraclesGuestTables *tables, const OraclesGuestRom *rom, uint8_t value);
/* Pure: whether the call is one of the catalogue with a parameter it accepts: a treasure only with a parameter whose
 * effect the game bounds (guest_call.c, parameter_allowed, from the tables read in `rom`); the dungeons' treasures never. */
int oracles_guest_call_allowed(const OraclesGuestTables *tables, const OraclesGuestRom *rom, const OraclesGuestCall *call);
/* Queues a call; -1 when the transaction is not armed, the queue is full, or the call is not allowed.  *serial, when
 * given, numbers the call: it has run once oracles_guest_calls_done is past it. */
int oracles_guest_queue_call(OraclesGuest *guest, const OraclesGuestCall *call, unsigned *serial);
/* Calls queued and not yet run; calls run since the attach. */
unsigned oracles_guest_calls_pending(const OraclesGuest *guest);
unsigned oracles_guest_calls_done(const OraclesGuest *guest);

/* ---- the language transaction ------------------------------------------------------------
 *
 * A language composed into the image (engine/language: its texts and its
 * index in the banks of oracles_rom.h's ORACLES_IMAGE_LANGUAGE_BANK) is served
 * at the return of getTextAddress, which every text, fragment, call and jump
 * of the text engine goes through: the routine has put the US text's address
 * in hl and its bank in w7ActiveBank (and a); when the index has the
 * language's text for wTextIndexH:wTextIndexL, the transaction writes its
 * address and bank there instead, and the engine reads it as it would have
 * the US one.  A dictionary fragment, and a text the language keeps in
 * English or lacks, are left as the routine found them.  Nothing else is
 * written. */

/* Arms the transaction when the image the core runs holds a language's index; 0 also when it holds none (nothing is
 * armed: the game reads its own texts), -1 when the profile has no getTextAddress or the hook table is full. */
int oracles_guest_arm_language(OraclesGuest *guest);
/* The language served ("fr"), "" when none is armed. */
const char *oracles_guest_language(const OraclesGuest *guest);
/* The texts the game looked up since the attach: served in the language, kept in English by it, absent from it. */
void oracles_guest_language_counts(const OraclesGuest *guest, unsigned *served, unsigned *english, unsigned *missing);
/* Of those served, the texts laid out again for the compact text (ORACLES_LANGUAGE_LAID_OUT in their entry). */
unsigned oracles_guest_language_laid_out(const OraclesGuest *guest);

/* ---- the swim's hold ------------------------------------------------------------------
 *
 * Swimming, Link speeds up by strokes, each started by a new press (wGameKeysJustPressed, link.s): of a direction with
 * Ages' mermaid suit (linkUpdateVelocity@mermaidSuit), which speeds him up for the five frames after it, of A with the
 * flippers (linkUpdateFlippersSpeed, state 0, the stroke's 25 frames reading no press); the player taps.  With the hold
 * armed, at the one instruction of each routine that tests the bits the routine has just loaded in a, the transaction
 * adds the bits held when a stroke would serve.  The suit: no stroke started and the last one ending, and Link below
 * his speed cap, moving elsewhere than the direction held, or a frame from losing a step of speed.  The flippers: A or
 * a direction held, A not newly pressed, the routine being at its state 0, waiting for a stroke; it adds A.  A direction
 * counts only where it gives Link an angle (wLinkAngle's bit 7 clear): two opposite ones give none, and Link no stroke.
 * The routine then plays its stroke as for a press; nothing else reads that register, and nothing is written in
 * memory. */

/* What the suit's stroke test reads, at the instruction: the register and Link's fields (link.s, linkUpdateVelocity). */
typedef struct OraclesGuestStrokeState {
    uint8_t just_pressed;            /* a: wGameKeysJustPressed */
    uint8_t pressed;                 /* wGameKeysPressed */
    uint8_t input_angle;             /* wLinkAngle: the direction held, 32 steps; bit 7 none */
    uint8_t angle, speed, speed_cap; /* w1Link.angle, speed, speedTmp */
    uint8_t step_counter, step_period, stroke_counter;   /* w1Link.var12, var13, var3e */
} OraclesGuestStrokeState;
#define ORACLES_GAME_DIRECTIONS 0xf0u   /* BTN_RIGHT | BTN_LEFT | BTN_UP | BTN_DOWN, the game's own bits */
#define ORACLES_GAME_BUTTON_A 0x01u     /* BTN_A */
#define ORACLES_GAME_BUTTON_B 0x02u     /* BTN_B */

/* Pure: the direction bits to add to a at the suit's test, 0 for none. */
uint8_t oracles_guest_mermaid_press(const OraclesGuestStrokeState *state);
/* Pure: the A bit to add to a at the flippers' test (`just_pressed`, a; `pressed`, wGameKeysPressed; `input_angle`,
 * wLinkAngle): A held, or a direction held that gives an angle, A not newly pressed; 0 for none. */
uint8_t oracles_guest_flippers_press(uint8_t just_pressed, uint8_t pressed, uint8_t input_angle);
/* Pure: the address of a stroke's test in `rom` (the flat image, bank n from n * $4000): the `and` of the one
 * `ld a,(wGameKeysJustPressed) / and mask` within the routine's first $40 bytes; 0 when there is none or several. */
uint16_t oracles_guest_find_stroke_test(const uint8_t *rom, size_t size, OraclesGuestSym routine, OraclesGuestSym keys_just_pressed, uint8_t mask);
/* Arms the transaction at each routine the game has; -1 when the profile does not qualify it, a routine's test is not
 * found once, or the hook table is full. */
int oracles_guest_arm_swim_hold(OraclesGuest *guest);
/* Turns the hold on or off from the next test it meets, arming the tests the first time it is on (the same -1 as
 * above, the hold unchanged); once armed they stay so, and answer only while it is on.  A stroke under way when it
 * goes off is the game's to finish. */
int oracles_guest_set_swim_hold(OraclesGuest *guest, int on);
int oracles_guest_swim_hold(const OraclesGuest *guest);
/* The strokes the transaction started since the attach, with the mermaid suit and with the flippers. */
void oracles_guest_swim_strokes(const OraclesGuest *guest, unsigned *mermaid, unsigned *flippers);

/* ---- the cheats ------------------------------------------------------------------------
 *
 * Each cheat answers in a register, at the one instruction of a routine that reads it, before it runs, found in the
 * user's ROM within the routine's first $40 bytes (exactly once, else the cheat is refused); nothing is written in
 * memory.  No damage: at linkApplyDamage's `or a` (bank 6), with d on Link's object, a hit's damage (a negative
 * w1Link.damageToApply, which the routine has just loaded in a and cleared) becomes 0: Link loses no heart, and the
 * knockback, the sound and the invincibility frames of the hit are the game's.  Ammo never runs out: at the `ld (hl),a`
 * of decNumBombs' end, which decNumActiveSeeds and decNumBombchus share, the count less one becomes the count (hl)
 * holds: the counter is written back as it was; at zero the routine returns before; Ages' bomb-upgrade fairy taking
 * every bomb (bombUpgradeFairy_loseAllBombs' call, found in the ROM) is left to the game.  Rupees never run out: at
 * removeRupeeValue's `call getRupeeValue`, with hl on wNumRupees, the value to take away becomes 0, whose amount is
 * none; Seasons' ore chunks, and a call of the call transaction (a mod's game.pay), are left as they are.  A cheat
 * changes from one frame to the next: its point stays armed once armed, and answers only while the cheat is on. */
typedef enum OraclesGuestCheat {
    ORACLES_CHEAT_NO_DAMAGE = 1u << 0,
    ORACLES_CHEAT_AMMO = 1u << 1,
    ORACLES_CHEAT_RUPEES = 1u << 2
} OraclesGuestCheat;
#define ORACLES_CHEATS 3u   /* the cheats, bits 0 to ORACLES_CHEATS - 1 */

/* The name of the cheat of bit 1 << `index` ("no-damage", "ammo", "rupees"), as the settings, the command line and,
 * after "cheat-", a route's options write it; NULL past the last. */
const char *oracles_cheat_name(unsigned index);
/* Pure: the cheats a comma-separated list names into *cheats; with `prefix`, only its items that start with it name a
 * cheat ("cheat-", a route's options), the others skipped.  0, or -1 when an item names no cheat, *cheats then holding
 * those the others name (a later version's list read by this one). */
int oracles_cheats_parse(const char *list, const char *prefix, unsigned *cheats);
/* Pure: `cheats` as such a list, each name after `prefix` (NULL for none), in `out`; returns `out`. */
const char *oracles_cheats_format(unsigned cheats, const char *prefix, char *out, size_t capacity);
/* Pure: the address of the one occurrence of `sequence` starting within the first $40 bytes of `routine` in `rom` (the
 * flat image, bank n from n * $4000), inside its bank; 0 when there is none or several. */
uint16_t oracles_guest_find_in_routine(const uint8_t *rom, size_t size, OraclesGuestSym routine, const uint8_t *sequence, size_t length);
/* The same within the routine's first `window` bytes. */
uint16_t oracles_guest_find_in_routine_within(const uint8_t *rom, size_t size, OraclesGuestSym routine, const uint8_t *sequence, size_t length,
                                              size_t window);
/* Pure: the instruction `cheat` answers at in `rom`: `ld h,d / ld l,<w1Link.damageToApply / ld a,(hl) / ld (hl),$00 /
 * or a` in linkApplyDamage, its `or a`; `ld a,(hl) / sub $01 / daa / ld (hl),a / or h / ret` in decNumBombs, its
 * `ld (hl),a`; `ld hl,wNumRupees / call getRupeeValue` in removeRupeeValue, its call; 0 when not found once. */
uint16_t oracles_guest_find_cheat_point(const OraclesGuestTables *tables, const uint8_t *rom, size_t size, OraclesGuestCheat cheat);
/* Sets the cheats on (a mask of OraclesGuestCheat, 0 for none), arming the point of each the first time; -1, the cheats
 * unchanged, when the profile does not qualify them, a point is not found once, or the hook table is full. */
int oracles_guest_set_cheats(OraclesGuest *guest, unsigned cheats);
unsigned oracles_guest_cheats(const OraclesGuest *guest);
/* What the cheats did since the attach. */
typedef struct OraclesGuestCheatCounts {
    unsigned hits, health_kept;          /* hits whose damage was cancelled, and that damage, in quarters of a heart */
    unsigned ammo_kept;                  /* seeds, bombs and Bombchus used and not taken away */
    unsigned payments, rupees_kept;      /* payments of rupees cancelled, and their rupees */
    unsigned calls_charged;              /* payments of the call transaction (a mod's game.pay) left to the game */
} OraclesGuestCheatCounts;
void oracles_guest_cheat_counts(const OraclesGuest *guest, OraclesGuestCheatCounts *counts);

/* ---- the compact text -------------------------------------------------------------------
 *
 * drawLineOfText (textbox.s) composes a line of the text box whole, in one frame: for each character setLineTextBuffers
 * keeps its character, attribute, delay and sounds at column e of the line's sixteen-byte buffers, and
 * retrieveTextCharacter draws its 8 x 16 cell at bc in w7TextGfxBuffer, which dmaTextGfxBuffer copies to the VRAM after
 * it; displayNextTextCharacter then reveals a column at a time, writing its two tiles and their attribute in
 * w7TextboxMap.  The compact text keeps the line's characters, their delays and sounds, its commands, its choices and its
 * pages, and draws it in the font's own widths (oracles_text_font), more than sixteen characters when its 128 pixels
 * hold them (the texts laid out again for it, language.h):
 *   - at setLineTextBuffers' entry, the host keeps the character before, its cell and its buffers as the game wrote them,
 *     and has the game write this one at the line's last column and cell, which the next one overwrites; a secret's
 *     character (\secret, read from w7SecretText1 or 2) takes a whole tile, as a symbol does;
 *   - at textControlCodeC_2's `ld (hl),b`, the cursor's place a choice keeps, the tile of column e, becomes the tile
 *     column the place will start at, the next tile's edge after the characters before it; the place takes that whole
 *     tile;
 *   - at drawLineOfText's return, the line's tiles are composed again from the cells the game drew, each character's
 *     ink at the pen (oracles_text_place), and the line's buffers become its reveal's steps: a character each, sixteen at
 *     most, a blank character joining the one before it, then the narrowest neighbours, when the line holds more; a
 *     step's delay is the sum of its characters' delays, its sound its first character's that is not a space, its
 *     sound effect its first character's, and its character the pixel where the next step starts, past the last one's
 *     ink for the last (ORACLES_GUEST_COMPACT_STEP); the attributes buffer holds each tile column's attribute;
 *   - at displayNextTextCharacter's `ld d,>w7NextTextColumnToDisplay`, once it wrote step k's column, the line's columns
 *     in the map become the tile columns step k's characters reach, with their attributes, and the others up to k the
 *     blank of the box's margin;
 *   - at the vblank (oracles_guest_compact_vblank), while the game reveals the line, its tiles in the VRAM become the
 *     ones whole once the last step shown is, and the one the next step starts in with only the pixels before it: every
 *     character shown whole, none not yet shown, at the game's pace, its characters per second and its sounds.
 * Only the open line's characters, kept by the host within drawLineOfText's call, are not in the WRAM: a savestate taken
 * within the call carries them (oracles_guest_compact_save_state), and its load gives them back with the call's return,
 * so that the line goes on as if no load had come between; what the reveal needs is in the line's buffers.  In the
 * host's wide box (oracles_guest_arm_wide_text_box), the line is laid out and grouped in steps at the host's width, in
 * the font's widths or at the fixed advance; the game's buffers and w7TextGfxBuffer hold its first 128 pixels as above,
 * and the host's line the whole of it (the host's text box, below). */
#define ORACLES_GUEST_LINE_COLUMNS 16u        /* a line's buffers' columns, and its tile columns */
#define ORACLES_GUEST_LINE_GFX_BYTES 0x200u   /* w7TextGfxBuffer: a column's two tiles, 32 bytes */
#define ORACLES_GUEST_LINE_CHARACTERS ORACLES_TEXT_LINE_CHARACTERS_MAX   /* a line's characters at most */
#define ORACLES_GUEST_COMPACT_STEP 0x80u      /* in w7LineTextBuffer, a step: the pixel the next one starts at in the low bits */
typedef struct OraclesGuestCompactCharacter {
    uint8_t character;      /* as setLineTextBuffers received it ($06 a symbol) */
    uint8_t whole;          /* a symbol, a cursor's place or a secret's character: a whole tile */
    uint8_t attribute, delay, sound, effect;   /* the line's buffers, as the game wrote them */
    uint8_t cell[32];       /* its 8 x 16 cell, as retrieveTextCharacter drew it */
} OraclesGuestCompactCharacter;
typedef struct OraclesGuestCompactLine {
    unsigned count;                                     /* characters laid out: those the line's 128 pixels hold */
    unsigned overflows;                                 /* those after them, not drawn */
    unsigned pen;                                       /* where the next character would go */
    unsigned columns;                                   /* the tile columns the drawn pixels take */
    uint8_t start[ORACLES_GUEST_LINE_CHARACTERS];       /* where each character's first column drawn goes */
    uint8_t from[ORACLES_GUEST_LINE_CHARACTERS];        /* the first column of its cell drawn, */
    uint8_t drawn[ORACLES_GUEST_LINE_CHARACTERS];       /* and how many: its ink's, 0 for a blank character, a cell for a whole one */
    uint8_t attribute[ORACLES_TEXT_LINE_TILES];         /* each tile column's attribute */
    unsigned steps;                                    /* the reveal's steps, sixteen at most */
    uint8_t first[ORACLES_GUEST_LINE_COLUMNS + 1u];     /* step s: characters first[s] to first[s + 1] - 1 */
} OraclesGuestCompactLine;
/* Pure: `count` characters laid out in `font` (oracles_text_place) up to the first the line's 128 pixels do not hold,
 * and grouped in steps.  A tile column's attribute is its ink's, else a blank character's on it, else the column's
 * before it. */
void oracles_guest_compact_layout(const OraclesGuestCompactCharacter *characters, unsigned count, const OraclesTextFont *font, OraclesGuestCompactLine *line);
/* Pure: the same up to the first character `pixels` do not hold (a multiple of 8, ORACLES_TEXT_LINE_TILES tiles at
 * most): the host's text box lays its lines out at its own width; the steps are grouped at sixteen whatever it is. */
void oracles_guest_compact_layout_at(const OraclesGuestCompactCharacter *characters, unsigned count, const OraclesTextFont *font, unsigned pixels,
                                     OraclesGuestCompactLine *line);
/* Pure: the line's tiles composed from its characters' cells into `out` (ORACLES_GUEST_LINE_GFX_BYTES), blank where
 * nothing is drawn. */
void oracles_guest_compact_compose(const OraclesGuestCompactLine *line, const OraclesGuestCompactCharacter *characters, uint8_t *out);
/* Pure: the same into `out` of `pixels` / 8 tile columns of 32 bytes. */
void oracles_guest_compact_compose_at(const OraclesGuestCompactLine *line, const OraclesGuestCompactCharacter *characters, unsigned pixels, uint8_t *out);
/* Pure: the tile columns whole once step `shown` is: those the next step's first character does not reach, all of them
 * past the last step. */
unsigned oracles_guest_compact_shown(const OraclesGuestCompactLine *line, unsigned shown);
/* Pure: the pixel step `shown`'s characters end before: where the next step's first character starts, past the drawn
 * pixels after the last step (a tile column's edge); what its byte in the line's buffers holds, 127 at most. */
unsigned oracles_guest_compact_boundary(const OraclesGuestCompactLine *line, unsigned shown);
/* Pure: from a step's byte, the tile columns whole once it is shown and those it reaches (one more when the next step
 * starts within a tile); `last` for the line's last step, whose byte rounds the line's end down to 127. */
void oracles_guest_compact_step_columns(uint8_t step, int last, unsigned *whole, unsigned *shown);
/* Pure: a 32-byte tile column of the line with its first `pixels` columns kept, the others blank. */
void oracles_guest_compact_mask(const uint8_t *tile, unsigned pixels, uint8_t *out);
/* The vblank's part of the transaction (guest.c calls it at every vblank, after the hosts that read the frame just
 * scanned): the line being revealed in the VRAM, $9400 the top line's, $9600 the bottom's, as the steps shown allow. */
void oracles_guest_compact_vblank(OraclesGuest *guest);
/* Pure: displayNextTextCharacter's `ld d,>w7NextTextColumnToDisplay`, after `ld a,e / sub $20 / ld e,a / ld a,(hl) /
 * ld (de),a`, and textControlCodeC_2's `ld (hl),b`, after `ld a,e / add a / or $60 / ld b,a / inc b`, each starting
 * within its routine's first $40 bytes in `rom` (the flat image); 0 when not found once. */
uint16_t oracles_guest_find_reveal_point(const OraclesGuestTables *tables, const uint8_t *rom, size_t size);
uint16_t oracles_guest_find_cursor_place_point(const OraclesGuestTables *tables, const uint8_t *rom, size_t size);
/* Arms the transaction, the font's widths read in the image the core runs (a language's glyphs included); -1 when the
 * profile does not qualify it, a point is not found once, or the hook table is full. */
int oracles_guest_arm_compact_text(OraclesGuest *guest);
typedef struct OraclesGuestCompactCounts {
    unsigned lines, characters;   /* lines composed, and their characters */
    unsigned columns;             /* the tile columns they took, where the game takes one a character */
    unsigned places;              /* choices' cursor places moved */
    unsigned long_lines;          /* lines of more than sixteen characters, more than the game's buffers hold */
    unsigned overflows;           /* characters past a line's 128 pixels, not drawn */
    unsigned reveal_frames;       /* vblanks of a line being revealed */
    unsigned partial_writes;      /* tile columns written in the VRAM with only their pixels revealed */
    unsigned cut_frames;          /* of those vblanks, those whose screen would show a revealed character cut */
    unsigned early_frames;        /* and those that would show a character not revealed yet */
    unsigned resumed;             /* lines a savestate's load gave back within drawLineOfText's call */
} OraclesGuestCompactCounts;
void oracles_guest_compact_counts(const OraclesGuest *guest, OraclesGuestCompactCounts *counts);
/* The line drawLineOfText is composing, for a savestate taken within its call: its bytes in `out`, at most
 * ORACLES_GUEST_COMPACT_STATE_SIZE; 0 outside a call, or without the compact text. */
#define ORACLES_GUEST_COMPACT_STATE_SIZE (7u + ORACLES_GUEST_LINE_CHARACTERS * sizeof(OraclesGuestCompactCharacter))
size_t oracles_guest_compact_save_state(const OraclesGuest *guest, uint8_t *out, size_t capacity);
/* That line given back after the load of the state it was saved with, once oracles_guest_reset_execution_state has run:
 * its characters, and the call's return, where it is composed.  0, or -1 for bytes that are not such a state, or a
 * guest without the compact text. */
int oracles_guest_compact_load_state(OraclesGuest *guest, const uint8_t *data, size_t size);

/* ---- the instant text --------------------------------------------------------------------
 *
 * The text box reveals a line a character at a time, at the speed of the file screen (wTextSpeed, kept in the save); a
 * press of A or B during the reveal shows the rest of the line in the same frame, and the next line, after its first
 * delay, whole too; a second press turns the page.  With the instant text on, at updateCharacterDisplayTimer's test of
 * A and B (textbox.s), the one instruction that reads them for the reveal, the transaction adds A where neither is
 * pressed: each line shows whole, as a player pressing A at its start gets it, through the game's own path, its scroll,
 * its arrow, its wait for A, its choices and their cursor's delay.  \slow (the essences) keeps the game's lock, the test
 * not reached while it runs.  At the test of displayNextTextCharacter's loop to the line's end (its `bit 0,a` of
 * w7d0c1), the transaction stops the loop on a character with a sound effect (w7LineSoundEffectsBuffer: a Goron's, a
 * Tokay's, Ricky's voice) by taking the loop's bit away from a: the game plays it, and the line's rest comes the next
 * frame.  Nothing is written in memory, wTextSpeed and the save are left as they are, and the transaction has no state:
 * a savestate loads with it on or off.  It changes from one frame to the next: its points stay armed once armed, and
 * answer only while it is on. */

/* Pure: the bit to add to a (wKeysJustPressed) at the skip's test, A where neither A nor B is pressed; 0 for none. */
uint8_t oracles_guest_text_skip_press(uint8_t keys_just_pressed);
/* Pure: a (w7d0c1) at the loop's test, its bit 0 taken away when the character just written has a sound effect. */
uint8_t oracles_guest_text_skip_flags(uint8_t flags, uint8_t sound_effect);
/* Pure: updateCharacterDisplayTimer's `and BTN_A|BTN_B` after `ld a,(wKeysJustPressed)`, within its first $40 bytes, and
 * displayNextTextCharacter's `bit 0,a` after `ld e,<w7d0c1 / ld a,(de)`, within its first $60, in `rom` (the flat image);
 * 0 when not found once. */
uint16_t oracles_guest_find_text_skip_test(const OraclesGuestTables *tables, const uint8_t *rom, size_t size);
uint16_t oracles_guest_find_text_skip_loop(const OraclesGuestTables *tables, const uint8_t *rom, size_t size);
/* Turns the instant text on or off from the next test it meets, arming the points the first time it is on; -1, the
 * text unchanged, when the profile does not qualify it, a point is not found once, or the hook table is full. */
int oracles_guest_set_text_instant(OraclesGuest *guest, int on);
int oracles_guest_text_instant(const OraclesGuest *guest);
/* Whether its points are armed: on now, or once. */
int oracles_guest_text_instant_armed(const OraclesGuest *guest);
/* What it did since the attach. */
typedef struct OraclesGuestTextInstantCounts {
    unsigned presses;        /* tests it answered with A */
    unsigned voices_kept;    /* loops it stopped on a character with a sound effect */
    unsigned voices_lost;    /* characters with a sound effect the loop went past while it was on: none */
} OraclesGuestTextInstantCounts;
void oracles_guest_text_instant_counts(const OraclesGuest *guest, OraclesGuestTextInstantCounts *counts);

typedef struct OraclesGuestRegWrite {
    uint8_t ly;
    uint8_t stat_mode;     /* 0 hblank, 1 vblank, 2 OAM scan, 3 drawing */
    uint8_t reg;           /* low byte of the IO address */
    uint8_t value;
} OraclesGuestRegWrite;

/* Installs the core callbacks. Only one guest per core. */
OraclesGuest *oracles_guest_attach(OraclesCore *core, const OraclesCompatProfile *profile);
void oracles_guest_detach(OraclesGuest *guest);
const OraclesGuestTables *oracles_guest_tables(const OraclesGuest *guest);
const OraclesCompatProfile *oracles_guest_profile(const OraclesGuest *guest);

void oracles_guest_set_event_sink(OraclesGuest *guest, OraclesGuestEventFn sink, void *opaque);
/* An observer called after the sink with every event, for a consumer that
 * does not own the sink (the Enhanced view, whichever of the diagnostics panel
 * or the harness holds it).  At most four; returns -1 when full, 0 otherwise.
 * Removing one passes the same function and opaque. */
int oracles_guest_add_event_listener(OraclesGuest *guest, OraclesGuestEventFn listener, void *opaque);
void oracles_guest_remove_event_listener(OraclesGuest *guest, OraclesGuestEventFn listener, void *opaque);
void oracles_guest_set_frame(OraclesGuest *guest, uint32_t frame);

/* The hook table is filled from the game's tables at attach; tests replace it.
 * on_entry / on_return are event types, 0 for none; a hook already armed with
 * the same events is not armed twice. Returns 0, or -1 when full. */
void oracles_guest_clear_hooks(OraclesGuest *guest);
int oracles_guest_add_hook(OraclesGuest *guest, OraclesGuestSym entry, OraclesGuestEventType on_entry, OraclesGuestEventType on_return);

/* ---- bus: physical reads -------------------------------------------------- */
const uint8_t *oracles_guest_wram(OraclesGuest *guest, unsigned bank);   /* 4 KiB; bank 0 is $c000, banks 1-7 are $d000 */
const uint8_t *oracles_guest_vram(OraclesGuest *guest, unsigned bank);   /* 8 KiB */
const uint8_t *oracles_guest_oam(OraclesGuest *guest);                   /* 160 bytes: the OAM the last scanned frame used (the core's OAM before any frame ran) */
/* Link's position (8.8) as the OAM the last scanned frame used drew him: the one he had when the game's vblank handler
 * last copied wOam, which it does only with the main loop waiting for it (wVBlankChecker), moved by the transition
 * transaction's writes to the core's OAM since; 0 until two vblanks have run since the start or a load. */
int oracles_guest_link_shown(const OraclesGuest *guest, uint16_t *x, uint16_t *y);
const uint8_t *oracles_guest_hram(OraclesGuest *guest);                  /* 127 bytes from $ff80 */
const uint8_t *oracles_guest_io(OraclesGuest *guest);                    /* 128 bytes from $ff00 */
/* A symbol's bytes: $c000-$cfff in WRAM bank 0, $d000-$dfff in the symbol's bank, $ff80+ in HRAM.
 * NULL for an absent symbol or a range that leaves its region. */
const uint8_t *oracles_guest_ptr(OraclesGuest *guest, OraclesGuestSym sym, size_t length);
uint8_t oracles_guest_read8(OraclesGuest *guest, OraclesGuestSym sym);
uint16_t oracles_guest_read16(OraclesGuest *guest, OraclesGuestSym sym);
/* Object `index` (0-15) of kind 0 item, 1 interaction, 2 enemy, 3 part: 64 bytes (guest_struct_offsets.h). */
const uint8_t *oracles_guest_object(OraclesGuest *guest, unsigned index, unsigned kind);

/* Palette RAM of the PPU: 64 bytes each, eight palettes of four RGB555 little-endian colours. */
const uint8_t *oracles_guest_bg_palettes(OraclesGuest *guest);
const uint8_t *oracles_guest_obj_palettes(OraclesGuest *guest);

/* CPU state, for the harness. */
uint16_t oracles_guest_sp(OraclesGuest *guest);
uint16_t oracles_guest_pc(OraclesGuest *guest);

/* Called from the core's vblank callback, before the guest's vblank handler
 * runs, with the OraclesVblankType: the moment the display state that the
 * finished scan used is readable (the native renderer). */
typedef void (*OraclesGuestVblankFn)(void *opaque, OraclesVblankType type);
void oracles_guest_set_vblank_hook(OraclesGuest *guest, OraclesGuestVblankFn hook, void *opaque);

/* ---- register journal -------------------------------------------------------- */
const OraclesGuestRegWrite *oracles_guest_journal(OraclesGuest *guest, size_t *count);
void oracles_guest_journal_clear(OraclesGuest *guest);
/* Writes the journal could not hold since the last clear (capacity 4096). */
size_t oracles_guest_journal_dropped(const OraclesGuest *guest);
/* The writes of wKeysPressed since the attachment, which only the game's input poll (pollInput) makes, twice a poll:
 * a frame in which it changes is one in which the game read the keys. */
uint32_t oracles_guest_keys_polls(const OraclesGuest *guest);
/* Return hooks lost: `overflow` when more than sixteen were pending at a
 * capture, `purged` when the stack frame of a pending return was gone (a
 * thread switch or an early exit) before it fired. */
void oracles_guest_dropped_returns(const OraclesGuest *guest, unsigned *overflow, unsigned *purged);

/* Allocations refused for want of a free slot, counted on the same returns as
 * the creation events (NZ), by kind: 0 interactions, 1 enemies, 2 parts.  An
 * object missing on one side of a comparison is told apart from one the room
 * never created by these counters. */
void oracles_guest_slot_failures(const OraclesGuest *guest, unsigned out[3]);

/* ---- fingerprints ----------------------------------------------------------------- */
/* FNV-1a of WRAM without the dead bytes: in each stack, the bytes strictly
 * below the stack pointer of its thread (the CPU's when it runs in that
 * stack, the saved one otherwise; a stack with no known pointer counts as
 * dead entirely). Saved thread contexts are included. */
uint64_t oracles_guest_live_wram_hash(OraclesGuest *guest);
/* The live state as bytes, for naming what two replays differ by: WRAM bank 0
 * (0x1000 bytes) then HRAM (127), with what no code will read again zeroed:
 * the dead stack bytes of the fingerprint above, the entries of the vblank
 * function queue past its tail (consumed at the last vblank), and the
 * scratch the game's sources name as such: the general-purpose HRAM
 * variables hFF8A to hFF93 and wTmpcec0 up to the room's layout. */
#define ORACLES_GUEST_LIVE_DUMP_BYTES (0x1000u + 127u)
int oracles_guest_live_dump(OraclesGuest *guest, uint8_t out[ORACLES_GUEST_LIVE_DUMP_BYTES]);
uint64_t oracles_guest_hash(const uint8_t *data, size_t size, uint64_t seed);
#define ORACLES_HASH_SEED 0xcbf29ce484222325ull

/* The saved stack pointer of a thread (0-3) from wThreadStateBuffer; 0 if none. */
uint16_t oracles_guest_thread_sp(OraclesGuest *guest, unsigned thread);

/* After a savestate is loaded into the core, the pending return hooks
 * (captured stack addresses) and the interrupt depth of the read trace
 * describe an execution that no longer exists: the host calls this right
 * after the load, on the live guest as on a ghost's. */
void oracles_guest_reset_execution_state(OraclesGuest *guest);

/* ---- ghost instance ---------------------------------------------------------- */
/* Read trace: `fn` receives every CPU read outside the interrupt handlers
 * while the trace is enabled (an interrupt is entered at its vector and left
 * by reti), with SP, which tells the thread.  The core's read hook is armed
 * only while `fn` is set. */
typedef void (*OraclesGuestReadFn)(void *opaque, uint16_t address, uint16_t sp);
void oracles_guest_set_read_trace(OraclesGuest *guest, OraclesGuestReadFn fn, void *opaque);
void oracles_guest_enable_read_trace(OraclesGuest *guest, int enabled);
/* WRAM of a ghost instance, writable.  The live instance is never written
 * (docs/GAME_HOOKS.md, section 6): only the ghost, a disposable copy, calls this. */
uint8_t *oracles_guest_wram_writable(OraclesGuest *guest, unsigned bank);

#ifdef __cplusplus
}
#endif

#include "guest_text_box.h"   /* the host's text box, below the compact text */

#endif
