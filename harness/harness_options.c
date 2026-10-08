#include "harness_options.h"

#include "core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int harness_usage(void)
{
    fprintf(stderr, "usage: oracles-harness --rom ROM [--patch PATCH.bps] --route ROUTE [--frames N] [--hooks on|off] [--colour-correction on|off] [--out FILE] [--corrupt-thread1-at FRAME] [--sample-rate HZ (0)] [--core sameboy|mgba]\n"
                    "                       [--sram-out FILE]   (the cartridge RAM at the end of the replay, raw, as a .sav: a save the game made on one core read on the other)\n"
                    "                       [--positions FILE]   (group, room and Link's position after every frame: tools/compare_positions.py compares the rooms of two of them, from two cores, to find where they part)\n"
                    "                       [--keys-read FILE]   (the keys the game read in every frame, in a route's order, and whether it read them in that frame: tools/debounce_route.py)\n"
                    "                       [--render-check SAMPLES_DIR] [--render-expect CLASSES]   (native renderer against the core, samples written there; exit 1 on a mismatch, an empty expected class or a ceiling)\n"
                    "                       [--ghost-check DIR] [--ghost-lead FRAMES] [--ghost-threaded] [--ghost-trace] [--ghost-trace-load]   (ghost instance against every scrolling transition; the second trace counts the whole load's reads)\n"
                    "                       [--enhanced-check DIR] [--enhanced-ghost-budget FRAMES] [--enhanced-threaded] [--enhanced-paced] [--enhanced-reload-at FRAME] [--enhanced-camera 1|2] [--enhanced-neighbours off|static (static)]   (the Enhanced surface of every frame composed and hashed)\n"
                    "                       [--zoom-out]   (with --enhanced-check: the drawn-back view's 480x270 surface, as the launcher's --zoom-out; --view far)\n"
                    "                       [--view near|medium|far] [--aspect 16:9|4:3]   (with --enhanced-check: the view's level and the screen's shape, docs/PLAYING.md)\n"
                    "                       [--mods DIR]... [--mod-trace FILE]   (the mods the route was recorded with, and their state after every frame, to --compare with the session's ROUTE.mod.tsv)\n"
                    "                       (--enhanced-paced holds each frame to the Game Boy's period by a busy wait: a core a replay, as in play)\n"
                    "                       [--continuous-transitions]   (the gameplay policy of the continuous transitions, both games; on by itself for a route recorded with it)\n"
                    "                       [--continuous-swim]   (the same with Link swimming at the surface too, implies --continuous-transitions; on by itself for a route recorded with it)\n"
                    "                       [--neighbour-objects-check DIR] [--neighbour-objects-lead FRAMES] [--neighbour-capture]   (the objects of every neighbour, ghost against the real entry)\n"
                    "                       [--hotkeys-check DIR]   (item hotkeys: the inventory frame by frame in DIR; a route's exchanges are applied either way)\n"
                    "                       [--hotkeys-live use|equip] [--hotkey-slot N=<b|a>:<item>:<variant or -->]... [--hotkey-press FRAME:SLOT:FRAMES]... [--hotkeys-record ROUTE]\n"
                    "                                                (the live policy with scripted keys over the route's inputs, and the format 2 route of that session)\n"
                    "                       [--surface-at FRAME FILE.ppm]...   (with --enhanced-check: the surface composed at that frame, to look at)\n"
                    "                       [--dump-at FRAME FILE]...   (the live state at the end of that frame, dead memory zeroed: WRAM bank 0 then HRAM, to name the bytes two replays differ by)\n"
                    "                       [--summary FILE]   (the figures of every check as key=value lines, for tools/check_routes.py)\n"
                    "       oracles-harness --compare A.tsv B.tsv\n"
                    "       oracles-harness --compare-session ROUTE.session.tsv REPLAY.tsv   (a recorded session against the replay of its route)\n");
    return 2;
}

/* The command line into `o`; 0, or the usage status. */
int harness_parse_options(int argc, char **argv, harness_options *o)
{
    memset(o, 0, sizeof *o);
    o->enhanced_objects = 1;   /* as the launcher: a neighbour's objects unless --enhanced-neighbours off */
    o->render_expect = "normal,lcd-off,lcd-on-first";
    o->enhanced_budget = 4;
    o->enhanced_camera = 2;
    o->hooks = 1;
    o->ghost_lead = 30;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--rom") && i + 1 < argc) o->rom_path = argv[++i];
        else if (!strcmp(argv[i], "--patch") && i + 1 < argc) o->patch_path = argv[++i];
        else if (!strcmp(argv[i], "--route") && i + 1 < argc) o->route_path = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) o->frames = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--hooks") && i + 1 < argc) o->hooks = strcmp(argv[++i], "off") != 0;
        else if (!strcmp(argv[i], "--colour-correction") && i + 1 < argc) o->colour_correction = strcmp(argv[++i], "on") == 0;
        else if (!strcmp(argv[i], "--out") && i + 1 < argc) o->out_path = argv[++i];
        else if (!strcmp(argv[i], "--sample-rate") && i + 1 < argc) o->sample_rate_hz = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--positions") && i + 1 < argc) o->positions_path = argv[++i];
        else if (!strcmp(argv[i], "--sram-out") && i + 1 < argc) o->sram_out_path = argv[++i];
        else if (!strcmp(argv[i], "--keys-read") && i + 1 < argc) o->keys_read_path = argv[++i];
        else if (!strcmp(argv[i], "--core") && i + 1 < argc) {
            const char *name = argv[++i];
            o->core_given = 1;
            if (!strcmp(name, "sameboy")) o->core_kind = ORACLES_CORE_SAMEBOY;
            else if (!strcmp(name, "mgba")) o->core_kind = ORACLES_CORE_MGBA;
            else return harness_usage();
        }
        else if (!strcmp(argv[i], "--corrupt-thread1-at") && i + 1 < argc) o->corrupt_at = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--compare") && i + 2 < argc) { o->compare_a = argv[++i]; o->compare_b = argv[++i]; }
        else if (!strcmp(argv[i], "--compare-session") && i + 2 < argc) { o->compare_a = argv[++i]; o->compare_b = argv[++i]; o->compare_session = 1; }
        else if (!strcmp(argv[i], "--render-check") && i + 1 < argc) o->samples_dir = argv[++i];
        else if (!strcmp(argv[i], "--render-expect") && i + 1 < argc) o->render_expect = argv[++i];
        else if (!strcmp(argv[i], "--ghost-check") && i + 1 < argc) o->ghost_dir = argv[++i];
        else if (!strcmp(argv[i], "--mods") && i + 1 < argc && o->mods_count < 8u) o->mods_dirs[o->mods_count++] = argv[++i];
        else if (!strcmp(argv[i], "--mod-trace") && i + 1 < argc) o->mod_trace_path = argv[++i];
        else if (!strcmp(argv[i], "--ghost-lead") && i + 1 < argc) o->ghost_lead = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--ghost-threaded")) o->ghost_threaded = 1;
        else if (!strcmp(argv[i], "--ghost-trace")) o->ghost_trace = 1;
        else if (!strcmp(argv[i], "--ghost-trace-load")) o->ghost_trace = o->ghost_trace_load = 1;
        else if (!strcmp(argv[i], "--enhanced-check") && i + 1 < argc) o->enhanced_dir = argv[++i];
        else if (!strcmp(argv[i], "--enhanced-ghost-budget") && i + 1 < argc) o->enhanced_budget = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--enhanced-reload-at") && i + 1 < argc) o->enhanced_reload_at = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--enhanced-threaded")) o->enhanced_threaded = 1;
        else if (!strcmp(argv[i], "--enhanced-paced")) o->enhanced_paced = 1;
        else if (!strcmp(argv[i], "--zoom-out")) { o->enhanced_zoom_out = 1; o->enhanced_level = 2; }
        else if (!strcmp(argv[i], "--view") && i + 1 < argc) {
            const char *name = argv[++i];
            if (!strcmp(name, "near")) o->enhanced_level = 0;
            else if (!strcmp(name, "medium")) o->enhanced_level = 1;
            else if (!strcmp(name, "far")) o->enhanced_level = 2;
            else { fprintf(stderr, "harness: --view is near, medium or far, not %s\n", name); return -1; }
            o->enhanced_zoom_out = o->enhanced_level == 2;
        }
        else if (!strcmp(argv[i], "--aspect") && i + 1 < argc) {
            const char *name = argv[++i];
            if (!strcmp(name, "16:9")) o->enhanced_aspect = 0;
            else if (!strcmp(name, "4:3")) o->enhanced_aspect = 1;
            else { fprintf(stderr, "harness: --aspect is 16:9 or 4:3, not %s\n", name); return -1; }
        }
        else if (!strcmp(argv[i], "--enhanced-camera") && i + 1 < argc) o->enhanced_camera = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--continuous-transitions")) o->continuous_transitions = 1;
        else if (!strcmp(argv[i], "--continuous-swim")) o->continuous_transitions = o->continuous_swim = 1;
        else if (!strcmp(argv[i], "--hotkeys-check") && i + 1 < argc) o->hotkeys_dir = argv[++i];
        else if (!strcmp(argv[i], "--hotkeys-live") && i + 1 < argc) { o->hotkeys_live.mode = argv[++i]; if (strcmp(o->hotkeys_live.mode, "use") != 0 && strcmp(o->hotkeys_live.mode, "equip") != 0) return -1; }
        else if (!strcmp(argv[i], "--hotkey-slot") && i + 1 < argc) { const char *v = argv[++i]; if (v[0] < '1' || v[0] > '4' || v[1] != '=') return -1; o->hotkeys_live.slots[v[0] - '1'] = v + 2; }
        else if (!strcmp(argv[i], "--hotkey-press") && i + 1 < argc) { if (!oracles_hotkeys_live_parse_press(&o->hotkeys_live, argv[++i])) return -1; }
        else if (!strcmp(argv[i], "--hotkeys-record") && i + 1 < argc) o->hotkeys_live.record_path = argv[++i];
        else if (!strcmp(argv[i], "--dump-at") && i + 2 < argc && o->dumps < HARNESS_MAX_DUMPS) { o->dump_at[o->dumps] = (uint32_t)strtoul(argv[++i], NULL, 10); o->dump_path[o->dumps++] = argv[++i]; }
        else if (!strcmp(argv[i], "--surface-at") && i + 2 < argc && o->surfaces < 16u) { o->surface_at[o->surfaces] = (uint32_t)strtoul(argv[++i], NULL, 10); o->surface_path[o->surfaces++] = argv[++i]; }
        else if (!strcmp(argv[i], "--summary") && i + 1 < argc) o->summary_path = argv[++i];
        else if (!strcmp(argv[i], "--neighbour-objects-check") && i + 1 < argc) o->objects_dir = argv[++i];
        else if (!strcmp(argv[i], "--neighbour-objects-lead") && i + 1 < argc) o->objects_lead = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--neighbour-capture")) o->objects_capture = 1;
        else if (!strcmp(argv[i], "--enhanced-neighbours") && i + 1 < argc) {
            const char *level = argv[++i];
            if (!strcmp(level, "static")) o->enhanced_objects = 1;
            else if (!strcmp(level, "off")) o->enhanced_objects = 0;
            else return -1;
        }
        else return -1;
    }
    return 0;
}
