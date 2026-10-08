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
 * Observation hooks never write to the guest.  The write paths are three closed
 * transactions: the call's (a mod's host scene, further below), the
 * inventory's (the item hotkeys, further below) and the transition's: at the return of
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
    ORACLES_EVENT_FILE_OPERATION           /* entry of fileManagementFunction, hooked by the mods: c = 0 create, 1 save, 2 load, 3 erase the file of hActiveFileSlot */
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
    uint8_t active_collisions;                                                /* wActiveCollisions: the list of tileTypesTable its tiles are looked up in */
    uint8_t tile_types_bank;                                                  /* tileTypesTable in the ROM, 0xff when the game has none */
    uint16_t tile_types_table;
    uint8_t link_speed, link_var2f;                                           /* w1Link.speed (an objectSpeedTable row, times 5), var2f (Ages: bit 6 the mermaid suit, bit 7 the sea floor) */
    /* What screenTransitionState2@transition reads in normal play besides the direction held (--continuous-swim): */
    uint8_t transition_direction_raw;                                         /* wScreenTransitionDirection, bit 7 the forced transition */
    uint8_t link_enabled, link_move_angle;                                    /* w1Link.enabled, w1Link.angle (his motion's, 32 steps; bit 7 none) */
    uint8_t link_input_angle;                                                 /* wLinkAngle: the direction held, 32 steps; bit 7 none */
    uint8_t link_stroke;                                                      /* w1Link.var35: a stroke of the flippers, 1 speeding up, 2 slowing down (linkUpdateFlippersSpeed) */
    uint8_t screen_transition_delay, disable_screen_transitions, hole_or_conveyor;   /* wScreenTransitionDelay, wDisableScreenTransitions, wcc92 */
    uint8_t boundary_x, boundary_y, active_room, active_tile_index;           /* wScreenTransitionBoundaryX/Y, wActiveRoom, wActiveTileIndex */
    uint8_t map_width, map_height;                                            /* the overworld's (OVERWORLD_WIDTH/HEIGHT) */
    uint8_t animation_data_bank;
    uint8_t palette_thread_mode, tileset_palette, loaded_tileset_palette;   /* wPaletteThread_mode, wTilesetPalette, wLoadedTilesetPalette */
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
/* Whether a policy is installed: the continuous transitions are on. */
int oracles_guest_has_transition_policy(const OraclesGuest *guest);
/* The frame counter the host gave the guest (oracles_guest_set_frame). */
uint32_t oracles_guest_frame(const OraclesGuest *guest);
/* Transaction writes made at a vblank (a frame the game did not finish), by wScreenTransitionState (0 to 7). */
void oracles_guest_vblank_write_counts(const OraclesGuest *guest, unsigned out[8]);
/* Vblanks of a frame the game did not finish, outside a room load, where the policy was not asked. */
unsigned oracles_guest_vblank_policy_skipped(const OraclesGuest *guest);

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

#endif
