/* The harness's run (harness.c): what one replay holds, shared with its reports (harness_report.c). */
#ifndef ORACLES_HARNESS_INTERNAL_H
#define ORACLES_HARNESS_INTERNAL_H

#include "backends.h"
#include "core.h"
#include "enhanced_check.h"
#include "room_transition.h"
#include "animation_check.h"
#include "object_sprites_check.h"
#include "hotkeys_check.h"
#include "harness_options.h"
#include "coverage.h"
#include "neighbour_check.h"
#include "object_animation_check.h"
#include "frame_check.h"
#include "ghost_check.h"
#include "guest.h"
#include "guest_fingerprint.h"
#include "guest_struct_offsets.h"
#include "mod_session.h"
#include "language_session.h"
#include "bps.h"
#include "rom.h"
#include "route.h"
#include "sha1.h"
#include "text_box.h"
#include "text_speed_check.h"
#include "view.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

typedef struct run {
    OraclesCore *core;
    const OraclesCompatProfile *profile;
    OraclesGuest *guest;      /* NULL with --hooks off */
    OraclesFrameCheck *check; /* NULL without --render-check */
    OraclesGhostCheck *ghost; /* NULL without --ghost-check */
    OraclesEnhancedCheck *enhanced; /* NULL without --enhanced-check */
    OraclesRoomTransition *transition; /* NULL without --continuous-transitions */
    OraclesCoverage *coverage; /* what the route covers, with a guest */
    OraclesAnimationCheck *animation; /* the neighbours' animation reader against the live game, with the Enhanced check */
    OraclesObjectSpritesCheck *object_sprites; /* the objects' sprites built from the game's data against the live OAM, with the Enhanced check */
    OraclesNeighbourCheck *neighbours; /* NULL without --neighbour-objects-check or --object-animation-check */
    OraclesObjectAnimationCheck *object_animation; /* NULL without --object-animation-check */
    OraclesHotkeysCheck *hotkeys; /* with --hotkeys-check, or when the route carries inventory exchanges */
    OraclesHotkeysLive *hotkeys_live; /* with --hotkeys-live: scripted keys over the route's inputs */
    unsigned dumps;               /* --dump-at, as many times as wanted */
    uint32_t dump_at[HARNESS_MAX_DUMPS];
    const char *dump_path[HARNESS_MAX_DUMPS];
    harness_poke pokes[HARNESS_MAX_POKES];   /* --poke-at */
    unsigned poke_count;
    OraclesModSession *mod;   /* --mods */
    OraclesLanguageSession *language;   /* --language */
    OraclesGame game;
    OraclesRoute route;
    FILE *out;
    FILE *positions;          /* --positions */
    FILE *keys_read;          /* --keys-read */
    FILE *frame_times;        /* --frame-times */
    uint32_t keys_polls;      /* the game's input polls counted by the guest at the last frame's end */
    uint32_t corrupt_at;      /* 0: never */
    uint32_t colour_toggle_at; /* --colour-toggle-at, F2 in the launcher; 0: never */
    uint32_t framed_toggle[8]; /* --framed-toggle-at, F3 in the launcher */
    unsigned framed_toggles;
    unsigned events;
    size_t journal_writes;
    const uint8_t *rom;       /* the ROM the transitions read, with them */
    size_t rom_size;
    int swam_before;          /* Link swimming at the last frame of normal play, with the continuous transitions */
    int swam_into;            /* and at the start of the transition in progress */
    int in_transition, landing;   /* a transition seen in progress; Link swam into the one that just ended */
    int carried_off;          /* 8.8: what the last transition carried him off its straight path, across its axis */
    unsigned landing_direction;
    unsigned swim_dropped;    /* transitions he swam into and came out of on land where the straight path stays in water */
    /* With the continuous transitions, a glide in the water: from a frame no key is held (directions, A), his motion
     * left to slow down, how far he went until it stopped, against how far the game's swim routines take that motion
     * alone (oracles_room_transition_swim_step), the glide given up if a key is pressed again or he leaves the water. */
    int glide;                /* a glide under way */
    int glide_scrolled;       /* it went through a scroll */
    uint16_t glide_x, glide_y;   /* where the last frame left him, 8.8 */
    int32_t glide_dx, glide_dy;  /* how far he went since it began */
    int32_t glide_ex, glide_ey;  /* how far his motion takes him alone */
    unsigned glides, glides_unlike, edge_glides, edge_glides_unlike;
    int32_t edge_glide_off_max;  /* the most an edge glide went otherwise, along an axis, 8.8 */
    /* Swimming through a scroll: his frames, and those he went otherwise than his motion's move (objectSpeedTable), on an
     * axis by more than 3/256 px: a push, an axis held or frozen (a wall, the line across, a frame the game did not finish). */
    unsigned swim_scroll_frames, swim_scroll_off_frames;
    unsigned swim_load_frames, swim_load_off_frames;   /* those of the load before it, counted once it scrolls (not a warp's) */
    int swim_scroll_known;    /* glide_x, glide_y hold where the last frame left him, swimming */
    uint16_t swim_scroll_x, swim_scroll_y;
    unsigned cheats_changed;  /* the cheats the route's `cheat` lines change, their points armed from the start */
    /* --reminder-check: the frames the status bar is in play (oracles_enhanced_bar_in_play), and those it is drawn with a
     * menu open or a dialogue under way, and of them in play (none expected); the run of frames in play, and the frame the
     * launcher's reminder would start at (ORACLES_ENHANCED_BAR_SETTLE in a row), 0 for none. */
    int reminder_check;
    unsigned reminder_in_play, reminder_menu, reminder_in_play_menu, reminder_text, reminder_in_play_text, reminder_run;
    uint32_t reminder_start;
    int swim_changed;         /* the route's `set swim-hold` lines change the swim's hold, its tests armed from the start */
    int scene_walks_taken;    /* a route of a format before scene walks were left to the game: the transitions take them */
    int text_speed_changed;   /* the route's `set text-speed` lines change the instant text, its points armed from the start */
    OraclesTextSpeedCheck text_speed;   /* with the instant text armed: its lines and pages */
} run;

/* harness_report.c: each check's figures, to the standard error and the summary (--summary): the renderer's verdict (1
 * when it passes), the continuous transitions', the Enhanced view's and the other checks', the item hotkeys' (1 when
 * they hold), the language's. */
int harness_report_renderer(run *r, const harness_options *o, FILE *summary);
void harness_report_transitions(run *r, FILE *summary, uint8_t *rom);
void harness_report_checks(run *r, const harness_options *o, FILE *summary, uint8_t *rom);
int harness_report_hotkeys(run *r, FILE *summary);
void harness_report_language(run *r, FILE *summary);

#endif
