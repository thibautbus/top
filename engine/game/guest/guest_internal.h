/* State the guest's two files share: the bus, the hooks and the events
 * (guest.c), and the closed transition transaction of the continuous transitions
 * (guest_transaction.c).  Not a public interface: the host sees guest.h. */
#ifndef ORACLES_GUEST_INTERNAL_H
#define ORACLES_GUEST_INTERNAL_H

#include "guest.h"
#include "guest_struct_offsets.h"
#include "oracles_rom.h"

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
/* And swapGame's, for a half of an image of two games (oracles_guest_half_left). */

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
    const OraclesCompatProfile *profile;
    const OraclesGuestTables *tables;
    uint16_t rom_bank_base;                          /* the profile's address spaces (profile.h): the ROM bank its banks start at, */
    uint8_t sram_code_banks;                         /* and the cartridge RAM's banks it runs code from */
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
    int transition_steps;                            /* --continuous-steps: and the frame that ends a scroll, at the entry of drawAllSprites */
    int transition_at_entry;                         /* the policy was asked at the entry of drawAllSprites this frame, not at its return */
    OraclesGuestInventoryFn inventory_policy;        /* the item hotkeys */
    OraclesGuestInventoryResultFn inventory_result;
    OraclesGuestInventoryResetFn inventory_reset;
    void *inventory_opaque;
    uint16_t inventory_write_pc;                     /* the return address of the one call of checkReloadStatusBarGraphics in mainThreadStart */
    unsigned inventory_applied, inventory_violations;
    unsigned vblank_writes[8];                       /* transaction writes made at a vblank, by wScreenTransitionState */
    unsigned vblank_policy_skipped;                  /* vblanks of an unfinished frame outside a load, where the policy was not asked */
    unsigned transition_carried;                     /* carries across a transition's axis applied */
    unsigned transition_end_steps;                   /* steps of the frame that ends a scroll applied (--continuous-steps) */
    unsigned transition_respawns;                    /* respawns in the room entered moved where the game would have left Link (--continuous-steps) */
    unsigned transition_put_back;                    /* Link put back on the line, past it with no transition back (--continuous-line) */
    unsigned transition_placed;                      /* Link taken back to the game's place at the end of a scroll it made from the line into a wall (--continuous-line) */
    unsigned transition_back_forced;                 /* transitions back started at the end of a scroll on foot, Link past the line (--continuous-line) */
    unsigned transition_edge_holds;                  /* frames of normal play the game's edge test was asked to wait, a scene possible or begun */
    unsigned transition_cross_started;               /* scrolls into the room beside started at the end of a scroll, Link past the line across (--continuous-line) */
    unsigned transition_swim_steps;                  /* frames Link's swim's state was written in a scroll (--continuous-line) */
    uint32_t policy_not_asked;                       /* such vblanks since the policy was last asked */
    uint8_t transition_last_state;                   /* wScreenTransitionState when the policy was last asked, or as a savestate's load found it */
    hook hooks[64];                                  /* the table of section 15 plus the hooks a consumer adds (tags, object updates, the features' points) */
    unsigned hooks_refused;                          /* hooks asked for once the table was full */
    unsigned hook_count;
    int reader;                                      /* oracles_guest_reader's: no hook, no callback of the core's */
    uint8_t hooked_pc[0x10000u / 8u];                 /* a bit per address some hook enters at: the others skip the table */
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
    uint16_t keys_pressed;                           /* wKeysPressed, which the game's input poll alone writes */
    uint32_t keys_polls;                             /* its writes since the attachment: the polls, two writes each */
    uint16_t objects_start, objects_end;             /* the object slots, WRAM bank 1 */
    int calls_armed;                                 /* the call transaction (guest_call.c) */
    int calls_follow;                                /* armed once: armed again in each half the game goes to */
    OraclesGuestCall calls[ORACLES_GUEST_CALLS];
    unsigned call_count, calls_done;
    uint16_t call_watch_sp;                          /* 0, or SP at the entry of the checkReloadStatusBarGraphics a call returns through */
    const uint8_t *language_index;                   /* the language transaction (guest_text.c): the index in the core's ROM, or NULL */
    char language[ORACLES_LANGUAGE_INDEX_CODE_BYTES];
    unsigned name_capitals;                          /* the Japanese names' letters taken into capitals */
    unsigned name_kana;                              /* and those left as kana, their name without a Latin capital */
    unsigned language_served, language_english, language_missing, language_laid_out;
    unsigned text_wanted;                            /* the text's transactions armed, ORACLES_GUEST_TEXT_*: armed again in each half */
    unsigned text_armed, text_lost;                  /* at the switches: those armed again, those that could not be */
    unsigned mermaid_strokes, flippers_strokes;      /* the swim's hold (guest_swim.c): strokes it started */
    int swim_hold, swim_hold_armed;                  /* the hold on; its tests hooked */
    int swim_hold_follows;                           /* its tests armed once: armed again in each half the game goes to */
    int transition_line, line_armed;                 /* the transitions at the line (guest_line.c): on; its comparisons hooked */
    int scene_walks_taken;                           /* a scene's walk with the game's own keys taken by the transitions, as before (a route of an earlier format) */
    uint8_t map_last_column, map_last_row;           /* Ages' overworld edges @transition refuses, read in the ROM */
    OraclesGuestLineCounts line_counts;
    unsigned cheats, cheats_armed;                   /* the cheats (guest_cheats.c): those on, those whose point is armed */
    unsigned cheats_follow;                          /* those whose point was armed once: armed again in each half the game goes to */
    OraclesGuestSym ammo_scripted_return;            /* where the bomb-upgrade fairy's call of decNumBombs returns; bank ABSENT: none */
    OraclesGuestCheatCounts cheat_counts;
    int compact_armed, compact_open;                 /* the compact text (guest_compact_text.c): armed; within a drawLineOfText call */
    int compact_pending, compact_place_next;         /* the open line's last character not kept yet; the next is a cursor's place */
    unsigned compact_count;                          /* the open line's characters kept */
    uint16_t compact_return, compact_return_sp;      /* where its drawLineOfText call returns, and SP once it has */
    OraclesGuestCompactCharacter compact_line[ORACLES_GUEST_LINE_CHARACTERS];
    OraclesGuestCompactCounts compact_counts;
    OraclesTextFont compact_font;                    /* the image's font, read when it arms, at the fixed advance for the wide box's classic text */
    int compact_fixed;                               /* the transaction composes at the fixed advance */
    int text_box_armed;                              /* the host's text box (guest_text_box.c) */
    unsigned text_box_pixels;                        /* its width of text */
    int text_box_changed;                            /* its top or bottom line changed since the last vblank */
    OraclesGuestTextBoxReveal text_box_reveal_next, text_box_reveal_shown;   /* the text's reveal, read at the last vblank and the one before */
    uint32_t *text_box_pages;                        /* the pages laid out again for the wide box (oracles_guest_set_text_box_pages), */
    unsigned text_box_page_count;                    /* their pairs */
    int text_box_centre;                             /* the line being drawn is of a page kept as the game lays it out */
    OraclesTextFont text_box_fixed;                  /* the fixed advance of the game's layout */
    OraclesGuestTextBoxLine text_box[ORACLES_GUEST_TEXT_BOX_LINES];   /* as they stand */
    OraclesGuestTextBoxLine text_box_shown[ORACLES_GUEST_TEXT_BOX_LINES];   /* as the frame just scanned showed them */
    OraclesGuestTextBoxCounts text_box_counts;
    int text_instant, text_instant_armed;            /* the instant text (guest_text_speed.c): on; its points hooked */
    int text_instant_follows;                        /* its points armed once: armed again in each half the game goes to */
    OraclesGuestTextInstantCounts text_instant_counts;
    uint8_t oam_scanned[160];                        /* the OAM the frame just scanned used (see oracles_guest_oam) */
    int oam_scanned_valid;
    uint16_t link_shown_x, link_shown_y;             /* Link (8.8) where the OAM the frame just scanned drew him (oracles_guest_link_shown) */
    uint16_t link_next_x, link_next_y;               /* and where the OAM the next scan reads does */
    unsigned link_shown_vblanks;                     /* vblanks since a load: both are known from the second */
    uint32_t half_left;                              /* a half of an image of two games: the frame + 1 the game left it (guest.c), 0 */
    int half_watched, half_crossed;                  /* a ghost's guest: swapGame only noted (oracles_guest_watch_half); this run reached it */
    int half_state_loaded;                           /* a state was loaded since the last oracles_guest_follow_half */
    int half_switching;                              /* the reset of a switch is under way, 2 by swapGame (oracles_guest_switching_half) */
    unsigned half_reset_wait;                        /* frames the reset out of the second half has waited for init so far */
    unsigned half_settling;                          /* the frames since the guest followed, while the game's screen is still the half's it left, plus 1; 0 */
    unsigned half_switches, half_hooks_armed, half_returns_dropped, half_features_refused;   /* oracles_guest_half_counts */
    struct { OraclesGuestHalfFn fn; void *opaque; } half_listeners[12];   /* told of a switch, in the order they asked */
    unsigned half_listener_count;
};

/* guest.c: whether the cartridge's RAM maps one of the game's code banks and holds there its own half's copy of
 * `entry`'s bank at `entry` (the other half's copy has other code at the same addresses). */
int oracles_guest_sram_code_mapped(OraclesGuest *guest, OraclesGuestSym entry);

/* Whether the code of `entry`, a hook's or a routine's, is the one the CPU sees at its address now: bank 0's always;
 * at $4000-$7fff when the ROM bank mapped is its own, based at the half's; at $a000-$bfff when the cartridge's RAM
 * holds it (oracles_guest_sram_code_mapped; never in a game without code there); elsewhere the ROM bank, as for a
 * banked address.  The hot path calls it only for an address some hook enters at. */
static inline int oracles_guest_code_mapped(OraclesGuest *guest, OraclesGuestSym entry)
{
    if (entry.addr < 0x4000u) return 1;
    if (oracles_rom_in_sram(entry.addr)) return guest->sram_code_banks && oracles_guest_sram_code_mapped(guest, entry);
    uint16_t bank = 0;
    oracles_core_memory(guest->core, ORACLES_CORE_ROM, NULL, &bank);
    return (unsigned)entry.bank + guest->rom_bank_base == bank;
}

/* Whether a routine of the game is always visible where the game calls it from (bank 0, or the cartridge RAM's code,
 * mapped while the code there runs, in a game that runs code there): the call transaction's catalogue. */
static inline int oracles_guest_always_mapped(const OraclesGuest *guest, OraclesGuestSym entry)
{
    if (entry.bank == ORACLES_GUEST_ABSENT) return 0;
    if (entry.addr < 0x4000u) return entry.bank == 0u;
    return guest->sram_code_banks && oracles_rom_in_sram(entry.addr);
}

/* guest_transaction.c */
void oracles_guest_apply_transition_policy(OraclesGuest *guest, int at_vblank);
void oracles_guest_apply_transition_entry(OraclesGuest *guest);
/* guest_inventory.c */
void oracles_guest_apply_inventory_policy(OraclesGuest *guest);
void oracles_guest_item_buttons_state(OraclesGuest *guest, OraclesGuestInventoryState *state);   /* guest_item_buttons.c */
/* guest_call.c: the return address of the one `call checkReloadStatusBarGraphics` of mainThreadStart, read in the
 * user's ROM, in bank 0 or in the ROM's copy of the cartridge RAM's code; 0 when it is not found once.  The inventory
 * transaction's write point and the call transaction's. */
uint16_t oracles_guest_find_status_bar_call(OraclesGuest *guest);
/* guest_call.c: at the entry of checkReloadStatusBarGraphics, and before every instruction while a call waits for its return. */
void oracles_guest_call_point(OraclesGuest *guest, uint16_t sp);
void oracles_guest_call_step(OraclesGuest *guest, uint16_t sp, uint8_t opcode);
/* guest_text.c: at the return of getTextAddress. */
void oracles_guest_serve_text(OraclesGuest *guest);
/* guest_text.c: at a point of the Japanese names. */
void oracles_guest_name_point(OraclesGuest *guest, OraclesGuestEventType type);
/* guest.c: `listener` told of each switch of halves with the guest, once however often it asks (a feature arming its
 * points again in the half the game goes to); 0, or -1 when the guest has no room for it. */
int oracles_guest_listen_half(OraclesGuest *guest, OraclesGuestHalfFn listener);
/* guest_swim.c: at a stroke's test, the mermaid suit's or the flippers'. */
void oracles_guest_mermaid_stroke(OraclesGuest *guest);
void oracles_guest_flippers_stroke(OraclesGuest *guest);
/* guest_line.c: at an edge's comparison in screenTransitionState2. */
void oracles_guest_edge_test(OraclesGuest *guest, OraclesGuestEventType type);
/* guest_cheats.c: at a cheat's point. */
void oracles_guest_cheat_point(OraclesGuest *guest, OraclesGuestEventType type);
/* guest_text.c: the text's transactions, armed again in the half the game goes to (oracles_guest_add_half_listener). */
#define ORACLES_GUEST_TEXT_LANGUAGE 1u
#define ORACLES_GUEST_TEXT_COMPACT 2u
#define ORACLES_GUEST_TEXT_BOX 4u
/* The transaction `armed` noted, the listener added once: -1 when an image of two games has no room left for it (a
 * transaction that would not follow the game across the door is refused); a ghost's guest, which never switches, adds none. */
int oracles_guest_text_follow_half(OraclesGuest *guest, unsigned armed);

/* guest_compact_text.c: at the compact text's points. */
void oracles_guest_compact_text_point(OraclesGuest *guest, OraclesGuestEventType type);
/* guest_text_speed.c: at the skip's test and at its loop's. */
void oracles_guest_text_skip_point(OraclesGuest *guest, OraclesGuestEventType type);
/* guest_text_box.c: at drawLineOfText's return, at dmaTextGfxBuffer's and vblankDmaFunction's entries; the line composed
 * from its characters laid out at the host's width and its tiles (`gfx`, the host's width's, the compact text's
 * transaction's); the compact text's reveal written in the VRAM's line `which` at the vblank, its first `whole` tile
 * columns and `pixels` of the next (8: none), but the columns of `skipped` (a choice's cursor places, which the game
 * draws in the fan games); the lines and the reveal as the frame just scanned showed them, at the
 * vblank before its writes; and forgotten at a reset. */
void oracles_guest_text_box_point(OraclesGuest *guest, OraclesGuestEventType type);
void oracles_guest_text_box_compose(OraclesGuest *guest, const OraclesGuestCompactLine *line, const OraclesGuestCompactCharacter *characters, const uint8_t *gfx);
void oracles_guest_text_box_reveal(OraclesGuest *guest, unsigned which, unsigned whole, unsigned pixels, uint32_t skipped);
void oracles_guest_text_box_vblank(OraclesGuest *guest);
void oracles_guest_text_box_reset(OraclesGuest *guest);

#endif
