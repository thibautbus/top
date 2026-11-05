#include "harness_options.h"

#include "core.h"
#include "guest.h"
#include "view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int harness_usage(void)
{
    fprintf(stderr, "usage: oracles-harness --rom ROM [--patch PATCH.bps] --route ROUTE [--frames N] [--hooks on|off] [--colour-correction on|off] [--colour-toggle-at FRAME] [--out FILE] [--corrupt-thread1-at FRAME] [--sample-rate HZ (0)] [--core sameboy|mgba]\n"
                    "                       [--frame-times FILE]   (each frame's phases in microseconds: core with its hooks, source, presentation, end with the Enhanced check)\n"
                    "                       [--sram-out FILE]   (the cartridge RAM at the end of the replay, raw, as a .sav: a save the game made on one core read on the other)\n"
                    "                       [--positions FILE]   (group, room and Link's position after every frame: tools/compare_positions.py compares the rooms of two of them, from two cores, to find where they part)\n"
                    "                       [--keys-read FILE]   (the keys the game read in every frame, in a route's order, and whether it read them in that frame: tools/debounce_route.py)\n"
                    "                       [--render-check SAMPLES_DIR] [--render-expect CLASSES]   (native renderer against the core, samples written there; exit 1 on a mismatch, an empty expected class or a ceiling)\n"
                    "                       [--ghost-check DIR] [--ghost-lead FRAMES] [--ghost-threaded] [--ghost-trace] [--ghost-trace-load]   (ghost instance against every scrolling transition; the second trace counts the whole load's reads)\n"
                    "                       [--enhanced-check DIR] [--enhanced-ghost-budget FRAMES] [--enhanced-threaded] [--enhanced-ghosts 1|2 (with --enhanced-threaded)] [--enhanced-paced] [--enhanced-reload-at FRAME] [--enhanced-camera 1|2] [--enhanced-link-phase] [--enhanced-subpixel] [--enhanced-subpixel-switch FRAME[,FRAME]...] [--enhanced-neighbours off|static (static)] [--neighbour-animation on|off (on; off as the Low quality: the neighbours' objects frozen, the rooms never visited cut straight)]   (the Enhanced surface of every frame composed and hashed)\n"
                    "                       [--zoom-out]   (with --enhanced-check: the drawn-back view's 480x270 surface, 480x360 with --aspect 4:3, as the launcher's --zoom-out; --view far)\n"
                    "                       [--view near|medium|far] [--aspect 16:9|4:3]   (with --enhanced-check: the view's level and the screen's shape, docs/PLAYING.md)\n"
                    "                       [--screen WxH]   (with --enhanced-check: the screen's shape, unless --aspect names one, and the far view fitted to that screen, as the launcher's far=screen)\n"
                    "                       [--hud original|full|wide]   (with --enhanced-check: the gutters beside the status bar dark, or the bar's own background, or the bar laid out on it, as the launcher's hud=; original by default)\n"
                    "                       [--unvisited-rooms shown|hidden]   (with --enhanced-check: the rooms the game has never seen Link enter drawn, or kept dark, as the launcher's unvisited_rooms=; shown by default)\n"
                    "                       [--mods DIR]... [--mod-trace FILE]   (the mods the route was recorded with, and their state after every frame, to --compare with the session's ROUTE.mod.tsv)\n"
                    "                       [--language en|en-eu|fr|de|it|es [--eu-rom FILE|DIR] [--language-pack FILE]]   (the European ROM's English or a translation, from that ROM or its pack, served at getTextAddress; a fan game's too, with --patch)\n"
                    "                       (--enhanced-paced holds each frame to the Game Boy's period by a busy wait: a core a replay, as in play)\n"
                    );
    fprintf(stderr,
                    "                       [--continuous-transitions]   (the gameplay policy of the continuous transitions, both games; on by itself for a route recorded with it)\n"
                    "                       [--continuous-swim]   (the same with Link swimming at the surface too, implies --continuous-transitions; on by itself for a route recorded with it)\n"
                    "                       [--continuous-diagonal]   (a diagonal held carries Link across the transitions' axis too, implies --continuous-transitions; on by itself for a route recorded with it)\n"
                    "                       [--swim-hold]   (the swim's strokes from a held button: the mermaid suit's direction, the flippers' A; on by itself for a route recorded with it)\n"
                    "                       [--cheats no-damage,ammo,rupees]   (the cheats, Ages and Seasons: Link loses no heart, seeds and bombs are not used up, buying costs nothing\n"
                    "                                                (a mod's game.pay still charges); with those a route was recorded with, which are on by themselves)\n"
                    "                       [--text classic|compact]   (the text box's lines in the font's own widths, Oracles, fan games, with --enhanced-check alone; on by itself for a route recorded with it)\n"
                    "                       [--text-speed classic|instant]   (each line of the text box whole, as a press of A at its start shows it, Oracles, fan games, Faithful and Enhanced; on by itself for a route recorded with it)\n");
    fprintf(stderr,
                    "                       [--text-box-check]   (with --enhanced-check, Ages and Seasons: the text box drawn by the host each frame, at the game's width, against the core's image; nothing shown changes;\n"
                    "                                             with --text-box wide, the wide box against its layout)\n"
                    "                       [--text-box standard|wide]   (with --enhanced-check, Ages and Seasons: the host's box of 192 pixels of text, the texts laid out again for it, classic or compact; on by itself for a route recorded with it)\n"
                    "                       [--text-size normal|large|xlarge]   (with --enhanced-check: the text box three halves as large, the presenter's, or twice, where the view holds it)\n"
                    "                       [--surface-scale K]   (the surfaces of --surface-at K times as large, the text box with Large as the presenter shows it)\n"
                    "                       [--menus-large]   (with --enhanced-check: the framed core shown alone, as menus=large, its text box enlarged by the presenter at every Text size)\n");
    fprintf(stderr,
                    "                       [--neighbour-objects-check DIR] [--neighbour-objects-lead FRAMES] [--neighbour-capture]   (the objects of every neighbour, ghost against the real entry)\n"
                    "                       [--object-animation-check]   (the objects' animation stepped from the game's data against the live game, each room entered captured as a neighbour: its poses' tiles; in DIR of --neighbour-objects-check if given)\n"
                    "                       [--hotkeys-check DIR]   (item hotkeys: the inventory frame by frame in DIR; a route's exchanges are applied either way)\n"
                    "                       [--hotkeys-live use|equip] [--hotkey-slot N=<b|a>:<item>:<variant or -->]... [--hotkey-press FRAME:SLOT:FRAMES]... [--hotkeys-record ROUTE]\n"
                    "                                                (the live policy with scripted keys over the route's inputs, and the format 2 route of that session)\n"
                    "                       [--surface-at FRAME FILE.ppm]...   (with --enhanced-check: the surface composed at that frame, to look at)\n"
                    "                       [--poke-at FRAME BANK:ADDR=VALUE]...   (a byte of WRAM written at the end of that frame, in hexadecimal (0:cc04=08): a test's way into a scene no route reaches, the variables the game's own caller sets)\n"
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
        else if (!strcmp(argv[i], "--frame-times") && i + 1 < argc) o->frame_times_path = argv[++i];
        else if (!strcmp(argv[i], "--keys-read") && i + 1 < argc) o->keys_read_path = argv[++i];
        else if (!strcmp(argv[i], "--core") && i + 1 < argc) {
            const char *name = argv[++i];
            o->core_given = 1;
            if (!strcmp(name, "sameboy")) o->core_kind = ORACLES_CORE_SAMEBOY;
            else if (!strcmp(name, "mgba")) o->core_kind = ORACLES_CORE_MGBA;
            else return harness_usage();
        }
        else if (!strcmp(argv[i], "--colour-toggle-at") && i + 1 < argc) o->colour_toggle_at = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--corrupt-thread1-at") && i + 1 < argc) o->corrupt_at = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--compare") && i + 2 < argc) { o->compare_a = argv[++i]; o->compare_b = argv[++i]; }
        else if (!strcmp(argv[i], "--compare-session") && i + 2 < argc) { o->compare_a = argv[++i]; o->compare_b = argv[++i]; o->compare_session = 1; }
        else if (!strcmp(argv[i], "--render-check") && i + 1 < argc) o->samples_dir = argv[++i];
        else if (!strcmp(argv[i], "--render-expect") && i + 1 < argc) o->render_expect = argv[++i];
        else if (!strcmp(argv[i], "--ghost-check") && i + 1 < argc) o->ghost_dir = argv[++i];
        else if (!strcmp(argv[i], "--mods") && i + 1 < argc && o->mods_count < 8u) o->mods_dirs[o->mods_count++] = argv[++i];
        else if (!strcmp(argv[i], "--mod-trace") && i + 1 < argc) o->mod_trace_path = argv[++i];
        else if (!strcmp(argv[i], "--language") && i + 1 < argc) o->language = argv[++i];
        else if (!strcmp(argv[i], "--eu-rom") && i + 1 < argc) o->eu_rom_path = argv[++i];
        else if (!strcmp(argv[i], "--language-pack") && i + 1 < argc) o->language_pack = argv[++i];
        else if (!strcmp(argv[i], "--ghost-lead") && i + 1 < argc) o->ghost_lead = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--ghost-threaded")) o->ghost_threaded = 1;
        else if (!strcmp(argv[i], "--ghost-trace")) o->ghost_trace = 1;
        else if (!strcmp(argv[i], "--ghost-trace-load")) o->ghost_trace = o->ghost_trace_load = 1;
        else if (!strcmp(argv[i], "--enhanced-check") && i + 1 < argc) o->enhanced_dir = argv[++i];
        else if (!strcmp(argv[i], "--enhanced-ghost-budget") && i + 1 < argc) o->enhanced_budget = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--enhanced-reload-at") && i + 1 < argc) o->enhanced_reload_at = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--enhanced-threaded")) o->enhanced_threaded = 1;
        else if (!strcmp(argv[i], "--enhanced-ghosts") && i + 1 < argc) o->enhanced_ghosts = (unsigned)strtoul(argv[++i], NULL, 10);
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
            o->aspect_given = 1;
        }
        else if (!strcmp(argv[i], "--screen") && i + 1 < argc) {
            const char *size = argv[++i];
            char x = 0, rest = 0;
            if (sscanf(size, "%u%c%u%c", &o->screen_w, &x, &o->screen_h, &rest) != 3 || x != 'x' || !o->screen_w || !o->screen_h) {
                fprintf(stderr, "harness: --screen is a size, 1280x960, not %s\n", size);
                return -1;
            }
        }
        else if (!strcmp(argv[i], "--hud") && i + 1 < argc) {
            const char *name = argv[++i];
            if (!strcmp(name, "original")) o->enhanced_hud = ORACLES_ENHANCED_HUD_ORIGINAL;
            else if (!strcmp(name, "full")) o->enhanced_hud = ORACLES_ENHANCED_HUD_FULL;
            else if (!strcmp(name, "wide")) o->enhanced_hud = ORACLES_ENHANCED_HUD_WIDE;
            else { fprintf(stderr, "harness: --hud is original, full or wide, not %s\n", name); return -1; }
        }
        else if (!strcmp(argv[i], "--unvisited-rooms") && i + 1 < argc) {
            const char *name = argv[++i];
            if (strcmp(name, "shown") && strcmp(name, "hidden")) { fprintf(stderr, "harness: --unvisited-rooms is shown or hidden, not %s\n", name); return -1; }
            o->unvisited_hidden = !strcmp(name, "hidden");
        }
        else if (!strcmp(argv[i], "--enhanced-camera") && i + 1 < argc) o->enhanced_camera = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--enhanced-link-phase")) o->enhanced_link_phase = 1;
        else if (!strcmp(argv[i], "--enhanced-subpixel")) o->enhanced_subpixel = 1;
        else if (!strcmp(argv[i], "--enhanced-subpixel-switch") && i + 1 < argc) {
            /* Up to eight frames, increasing: the camera switched off and on again at each, as Display's Camera from the pause. */
            for (char *p = argv[++i], *end; *p && o->enhanced_subpixel_switches < 8u; p = *end == ',' ? end + 1 : end) {
                const unsigned long frame = strtoul(p, &end, 10);
                if (end == p) break;
                o->enhanced_subpixel_switch[o->enhanced_subpixel_switches++] = (uint32_t)frame;
            }
        }
        else if (!strcmp(argv[i], "--continuous-transitions")) o->continuous_transitions = 1;
        else if (!strcmp(argv[i], "--continuous-swim")) o->continuous_transitions = o->continuous_swim = 1;
        else if (!strcmp(argv[i], "--continuous-diagonal")) o->continuous_transitions = o->continuous_diagonal = 1;
        else if (!strcmp(argv[i], "--swim-hold")) o->swim_hold = 1;
        else if (!strcmp(argv[i], "--cheats") && i + 1 < argc) {
            if (oracles_cheats_parse(argv[++i], NULL, &o->cheats) != 0) { fprintf(stderr, "harness: --cheats names no-damage, ammo or rupees, not %s\n", argv[i]); return -1; }
        }
        else if (!strcmp(argv[i], "--text") && i + 1 < argc) {
            const char *v = argv[++i];
            if (strcmp(v, "classic") != 0 && strcmp(v, "compact") != 0) { fprintf(stderr, "harness: --text is classic or compact, not %s\n", v); return -1; }
            o->compact_text = !strcmp(v, "compact");
        }
        else if (!strcmp(argv[i], "--text-speed") && i + 1 < argc) {
            const char *v = argv[++i];
            if (strcmp(v, "classic") != 0 && strcmp(v, "instant") != 0) { fprintf(stderr, "harness: --text-speed is classic or instant, not %s\n", v); return -1; }
            o->text_instant = !strcmp(v, "instant");
        }
        else if (!strcmp(argv[i], "--text-box-check")) o->text_box_check = 1;
        else if (!strcmp(argv[i], "--text-size") && i + 1 < argc) {
            const char *v = argv[++i];
            if (strcmp(v, "normal") != 0 && strcmp(v, "large") != 0 && strcmp(v, "xlarge") != 0) { fprintf(stderr, "harness: --text-size is normal, large or xlarge, not %s\n", v); return -1; }
            o->text_size = !strcmp(v, "xlarge") ? 2 : !strcmp(v, "large");
        }
        else if (!strcmp(argv[i], "--menus-large")) o->menus_large = 1;
        else if (!strcmp(argv[i], "--surface-scale") && i + 1 < argc) {
            o->surface_scale = (unsigned)strtoul(argv[++i], NULL, 10);
            if (o->surface_scale < 1u || o->surface_scale > 8u) { fprintf(stderr, "harness: --surface-scale is 1 to 8\n"); return -1; }
        }
        else if (!strcmp(argv[i], "--text-box") && i + 1 < argc) {
            const char *v = argv[++i];
            if (strcmp(v, "standard") != 0 && strcmp(v, "wide") != 0) { fprintf(stderr, "harness: --text-box is standard or wide, not %s\n", v); return -1; }
            o->text_box_wide = !strcmp(v, "wide");
        }
        else if (!strcmp(argv[i], "--hotkeys-check") && i + 1 < argc) o->hotkeys_dir = argv[++i];
        else if (!strcmp(argv[i], "--hotkeys-live") && i + 1 < argc) { o->hotkeys_live.mode = argv[++i]; if (strcmp(o->hotkeys_live.mode, "use") != 0 && strcmp(o->hotkeys_live.mode, "equip") != 0) return -1; }
        else if (!strcmp(argv[i], "--hotkey-slot") && i + 1 < argc) { const char *v = argv[++i]; if (v[0] < '1' || v[0] > '4' || v[1] != '=') return -1; o->hotkeys_live.slots[v[0] - '1'] = v + 2; }
        else if (!strcmp(argv[i], "--hotkey-press") && i + 1 < argc) { if (!oracles_hotkeys_live_parse_press(&o->hotkeys_live, argv[++i])) return -1; }
        else if (!strcmp(argv[i], "--hotkeys-record") && i + 1 < argc) o->hotkeys_live.record_path = argv[++i];
        else if (!strcmp(argv[i], "--dump-at") && i + 2 < argc && o->dumps < HARNESS_MAX_DUMPS) { o->dump_at[o->dumps] = (uint32_t)strtoul(argv[++i], NULL, 10); o->dump_path[o->dumps++] = argv[++i]; }
        else if (!strcmp(argv[i], "--poke-at") && i + 2 < argc && o->poke_count < HARNESS_MAX_POKES) {
            harness_poke *p = &o->pokes[o->poke_count];
            unsigned bank = 0, addr = 0, value = 0;
            char rest = 0;
            p->frame = (uint32_t)strtoul(argv[++i], NULL, 10);
            if (sscanf(argv[++i], "%x:%x=%x%c", &bank, &addr, &value, &rest) != 3 || addr < 0xc000u || addr > 0xdfffu || bank > 7u || value > 0xffu) {
                fprintf(stderr, "harness: --poke-at FRAME BANK:ADDR=VALUE, in hexadecimal, a byte of WRAM ($c000-$dfff), not %s\n", argv[i]);
                return -1;
            }
            p->bank = (uint8_t)bank; p->addr = (uint16_t)addr; p->value = (uint8_t)value;
            o->poke_count++;
        }
        else if (!strcmp(argv[i], "--surface-at") && i + 2 < argc && o->surfaces < 16u) { o->surface_at[o->surfaces] = (uint32_t)strtoul(argv[++i], NULL, 10); o->surface_path[o->surfaces++] = argv[++i]; }
        else if (!strcmp(argv[i], "--summary") && i + 1 < argc) o->summary_path = argv[++i];
        else if (!strcmp(argv[i], "--neighbour-objects-check") && i + 1 < argc) o->objects_dir = argv[++i];
        else if (!strcmp(argv[i], "--neighbour-objects-lead") && i + 1 < argc) o->objects_lead = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--neighbour-capture")) o->objects_capture = 1;
        else if (!strcmp(argv[i], "--object-animation-check")) o->object_animation = 1;
        else if (!strcmp(argv[i], "--enhanced-neighbours") && i + 1 < argc) {
            const char *level = argv[++i];
            if (!strcmp(level, "static")) o->enhanced_objects = 1;
            else if (!strcmp(level, "off")) o->enhanced_objects = 0;
            else return -1;
        }
        else if (!strcmp(argv[i], "--neighbour-animation") && i + 1 < argc) {
            const char *animation = argv[++i];
            if (strcmp(animation, "on") && strcmp(animation, "off")) return -1;
            o->enhanced_frozen = !strcmp(animation, "off");
        }
        else return -1;
    }
    if (o->screen_w && !o->aspect_given) o->enhanced_aspect = (int)oracles_enhanced_screen_aspect(o->screen_w, o->screen_h);
    return 0;
}

OraclesEnhancedSize harness_view_size(const harness_options *o)
{
    const OraclesEnhancedAspect aspect = o->screen_w && !o->aspect_given ? ORACLES_ENHANCED_ASPECTS : (OraclesEnhancedAspect)o->enhanced_aspect;
    return oracles_enhanced_screen_view_size((OraclesEnhancedLevel)o->enhanced_level, aspect, 0, o->screen_w, o->screen_h);
}
