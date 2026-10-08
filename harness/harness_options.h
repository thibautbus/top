/* The harness's command line: what a run is asked, and its usage. */
#ifndef ORACLES_HARNESS_OPTIONS_H
#define ORACLES_HARNESS_OPTIONS_H

#include "hotkeys_live.h"

#include <stdint.h>

#define HARNESS_MAX_DUMPS 64u

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
    uint32_t dump_at[HARNESS_MAX_DUMPS];
    const char *dump_path[HARNESS_MAX_DUMPS];
    unsigned dumps;
    uint32_t surface_at[16];                  /* --surface-at, with the Enhanced check */
    const char *surface_path[16];
    unsigned surfaces;
    unsigned objects_lead, enhanced_budget, enhanced_camera, ghost_lead;
    uint32_t enhanced_reload_at, frames, corrupt_at, sample_rate_hz;
    int objects_capture, enhanced_objects, enhanced_threaded, enhanced_paced, enhanced_zoom_out, continuous_transitions, continuous_swim;
    int enhanced_level, enhanced_aspect;      /* --view (near unless --zoom-out) and --aspect: an OraclesEnhancedLevel and OraclesEnhancedAspect */
    int hooks, colour_correction, ghost_threaded, ghost_trace, ghost_trace_load, compare_session;
    int core_kind;                            /* --core: an OraclesCoreKind; without it, the route's (SameBoy, or mGBA for a route that says so) */
    int core_given;
    OraclesHotkeysLiveOptions hotkeys_live;   /* the live policy of the item hotkeys with scripted keys */
} harness_options;

/* The command line into `o`; 0, or -1 when it is not understood. */
int harness_parse_options(int argc, char **argv, harness_options *o);
/* Prints the usage; returns the exit status that goes with it. */
int harness_usage(void);

#endif
