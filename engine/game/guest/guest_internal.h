/* State the guest's two files share: the bus, the hooks and the events
 * (guest.c), and the closed transition transaction of the continuous transitions
 * (guest_transaction.c).  Not a public interface: the host sees guest.h. */
#ifndef ORACLES_GUEST_INTERNAL_H
#define ORACLES_GUEST_INTERNAL_H

#include "guest.h"
#include "guest_struct_offsets.h"

#include "gb.h"

#include <stdlib.h>
#include <string.h>

#define JOURNAL_CAPACITY 4096u
#define PENDING_RETURNS 16u
#define THREADS 4u
#define THREAD_STATE_SIZE 8u
#define THREAD_STATE_SP_OFFSET 2u
/* Hooks oracles_guest_attach poses: drawAllSprites, initializeRoom, the three
 * slot allocators, applyWarpDest, showText, giveTreasure, openMenu, saveFile
 * and pollInput.  A hook refused makes the attach fail (guest.c). */
#define ORACLES_GUEST_ATTACHED_HOOKS 11u

typedef struct hook {
    OraclesGuestSym entry;
    OraclesGuestEventType on_entry;    /* 0: no event on entry */
    OraclesGuestEventType on_return;   /* 0: no return capture */
} hook;

typedef struct pending_return {
    uint16_t address;
    uint16_t sp_after;                 /* SP once the return has popped its address */
    OraclesGuestEventType type;
} pending_return;

struct OraclesGuest {
    OraclesCore *core;
    GB_gameboy_t *gb;
    const OraclesCompatProfile *profile;
    const OraclesGuestTables *tables;
    OraclesGuestEventFn sink;
    void *sink_opaque;
    OraclesGuestEventFn listeners[4];                /* observers besides the sink (the Enhanced view's objects) */
    void *listener_opaque[4];
    unsigned listener_count;
    uint32_t frame;
    OraclesGuestTransitionFn transition_policy;   /* the continuous transitions */
    int drawn_this_frame;
    OraclesGuestTransitionResetFn transition_reset;
    void *transition_opaque;
    int transition_swim;                             /* --continuous-swim: the transaction takes Link swimming at the surface as well */
    OraclesGuestInventoryFn inventory_policy;        /* the item hotkeys */
    OraclesGuestInventoryResultFn inventory_result;
    OraclesGuestInventoryResetFn inventory_reset;
    void *inventory_opaque;
    uint16_t inventory_write_pc;                     /* the return address of the one call of checkReloadStatusBarGraphics in mainThreadStart */
    unsigned inventory_applied, inventory_violations;
    unsigned vblank_writes[8];                       /* transaction writes made at a vblank, by wScreenTransitionState */
    unsigned vblank_policy_skipped;                  /* vblanks of an unfinished frame outside a load, where the policy was not asked */
    uint32_t policy_not_asked;                       /* such vblanks since the policy was last asked */
    hook hooks[32];                                  /* the table of section 15 plus the hooks a consumer adds (tags, object updates) */
    unsigned hook_count;
    pending_return pending[PENDING_RETURNS];
    unsigned pending_count;
    OraclesGuestRegWrite journal[JOURNAL_CAPACITY];
    size_t journal_count;
    OraclesGuestVblankFn vblank_hook;
    void *vblank_opaque;
    uint16_t global_flags_start, global_flags_end;   /* [start, end) */
    uint16_t room_flags_start, room_flags_end;
    uint16_t selected_text_option;
    OraclesGuestReadFn read_fn;
    void *read_opaque;
    int read_enabled;
    unsigned interrupt_depth;                        /* vectors entered minus reti executed */
    int boot_finished;                               /* the boot ROM is unmapped: hooks fire on the cartridge's code only */
    size_t journal_dropped;
    unsigned returns_overflow, returns_purged;
    unsigned slot_failures[3];                       /* interactions, enemies, parts refused for want of a slot */
    uint16_t transition_state, transition_direction;
    uint16_t objects_start, objects_end;             /* the object slots, WRAM bank 1 */
    int calls_armed;                                 /* the call transaction (guest_call.c) */
    OraclesGuestCall calls[ORACLES_GUEST_CALLS];
    unsigned call_count, calls_done;
    uint16_t call_watch_sp;                          /* 0, or SP at the entry of the checkReloadStatusBarGraphics a call returns through */
    uint8_t oam_scanned[160];                        /* the OAM the frame just scanned used (see oracles_guest_oam) */
    int oam_scanned_valid;
};

/* guest_transaction.c */
void oracles_guest_apply_transition_policy(OraclesGuest *guest, int at_vblank);
/* guest_inventory.c */
void oracles_guest_apply_inventory_policy(OraclesGuest *guest);
void oracles_guest_item_buttons_state(OraclesGuest *guest, OraclesGuestInventoryState *state);   /* guest_item_buttons.c */
/* guest_call.c: at the entry of checkReloadStatusBarGraphics, and before every instruction while a call waits for its return. */
void oracles_guest_call_point(OraclesGuest *guest, uint16_t sp);
void oracles_guest_call_step(OraclesGuest *guest, uint16_t sp, uint8_t opcode);

#endif
