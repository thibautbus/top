/* The harness's command line: what a run is asked, and its usage. */
#ifndef ORACLES_HARNESS_OPTIONS_H
#define ORACLES_HARNESS_OPTIONS_H

#include "compositor.h"
#include "hotkeys_live.h"

#include <stdint.h>

#define HARNESS_MAX_DUMPS 64u
#define HARNESS_MAX_POKES 64u

/* A byte of WRAM written at the end of a frame (--poke-at): a test's way into a scene no route reaches. */
typedef struct harness_poke { uint32_t frame; uint8_t bank; uint16_t addr; uint8_t value; } harness_poke;

/* What the command line asks of a run. */
typedef struct harness_options {
    const char *rom_path, *patch_path, *route_path, *out_path, *compare_a, *compare_b, *samples_dir;
    const char *ghost_dir, *enhanced_dir, *summary_path, *objects_dir, *render_expect, *hotkeys_dir;
    const char *positions_path;               /* --positions: group, room and Link's position after every frame */
    const char *frame_times_path;             /* --frame-times: each frame's phases, timed */
    const char *sram_out_path;                /* --sram-out: the cartridge RAM at the end of the replay */
    const char *keys_read_path;               /* --keys-read: the keys the game read in every frame */
    const char *mods_dirs[8], *mod_trace_path;   /* the mods replayed with the route (--mods, repeated), and their state frame by frame */
    unsigned mods_count;
    const char *language, *eu_rom_path, *language_pack;   /* --language, its European ROM (a file or a directory) and its pack */
    uint32_t dump_at[HARNESS_MAX_DUMPS];
    const char *dump_path[HARNESS_MAX_DUMPS];
    unsigned dumps;
    harness_poke pokes[HARNESS_MAX_POKES];
    unsigned poke_count;
    uint32_t surface_at[16];                  /* --surface-at, with the Enhanced check */
    const char *surface_path[16];
    unsigned surfaces;
    unsigned objects_lead, enhanced_budget, enhanced_camera, ghost_lead;
    int enhanced_link_phase, enhanced_subpixel;
    int beside_witness;   /* --beside-witness: the rooms drawn beside the room in play against the game's own entry into them */
    uint32_t enhanced_subpixel_switch[8];   /* --enhanced-subpixel-switch: the frames the sub-pixel camera switches at, as from the pause */
    unsigned enhanced_subpixel_switches;
    uint32_t enhanced_reload_at, frames, corrupt_at, sample_rate_hz, colour_toggle_at;
    uint32_t framed_toggle[8];               /* --framed-toggle-at: the frames F3 switches between the wide world and the framed core */
    unsigned framed_toggles;
    unsigned enhanced_ghosts;                 /* --enhanced-ghosts: ghosts working at once, threaded (1 by default) */
    int object_animation;                  /* --object-animation-check: the objects' animation from the game's data against the live game */
    int enhanced_frozen;                   /* --neighbour-animation off: a neighbour's objects frozen and the rooms never visited cut straight, as the Low quality has them */
    int objects_capture, enhanced_objects, enhanced_threaded, enhanced_paced, enhanced_zoom_out, continuous_transitions, continuous_swim, continuous_diagonal, continuous_steps, continuous_line;
    int swim_hold;                         /* --swim-hold: the swim's strokes from a held button */
    int text_instant;                      /* --text-speed instant: each line of the text box whole */
    unsigned cheats;                       /* --cheats: a mask of OraclesGuestCheat */
    int compact_text;                      /* --text compact: the text box's lines in the font's own widths */
    int text_box_check;                    /* --text-box-check: the text box drawn by the host, against the core's image */
    int reminder_check;                    /* --reminder-check: the frames the launcher's reminder over a game counts as in play */
    int text_box_wide;                     /* --text-box wide: the host's wide box, 192 pixels of text, classic or compact */
    int text_size;                         /* --text-size: 0 normal, 1 large (three halves, the presenter's), 2 xlarge (twice, in the band) */
    int menus_large;                       /* --menus-large: the framed core shown alone, its box the presenter's at every Text size */
    unsigned surface_scale;                /* --surface-scale K: the surfaces written K times as large, the box with Large as the presenter shows it */
    int enhanced_level, enhanced_aspect;      /* --view (near unless --zoom-out) and --aspect: an OraclesEnhancedLevel and OraclesEnhancedAspect */
    int aspect_given;                         /* --aspect named the shape */
    unsigned screen_w, screen_h;              /* --screen: the screen the view's shape comes from, which far fits to; 0: none */
    int enhanced_hud;                         /* --hud full|wide: the status bar drawn full width, or laid out (OraclesEnhancedHud); original (0) by default, which every row hashed before it keeps */
    int unvisited_hidden;                     /* --unvisited-rooms hidden: the rooms never visited kept dark, as the launcher's unvisited_rooms=hidden */
    int hooks, colour_correction, ghost_threaded, ghost_trace, ghost_trace_load, compare_session;
    int core_kind;                            /* --core: an OraclesCoreKind; without it, the route's (SameBoy, or mGBA for a route that says so) */
    int core_given;
    OraclesHotkeysLiveOptions hotkeys_live;   /* the live policy of the item hotkeys with scripted keys */
} harness_options;

/* The command line into `o`; 0, or -1 when it is not understood. */
int harness_parse_options(int argc, char **argv, harness_options *o);
/* Prints the usage; returns the exit status that goes with it. */
int harness_usage(void);
/* The Enhanced view's surface the options ask: the level's in the shape, or the far view fitted to --screen's
 * (compositor.h, oracles_enhanced_far_fitting), as the launcher sizes it. */
OraclesEnhancedSize harness_view_size(const harness_options *o);

#endif
