/* A game session's end (session.h): its screenshot, its save written a last time, and the report of its frames on the
 * standard error. */
#include "session_internal.h"

#include "home_choices.h"
#include "view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_screenshot(const char *path, const uint32_t *pixels, uint32_t width, uint32_t height)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fprintf(f, "P6\n%u %u\n255\n", width, height);
    for (size_t i = 0; i < (size_t)width * height; i++) {
        const uint8_t rgb[3] = { (uint8_t)(pixels[i] >> 16), (uint8_t)(pixels[i] >> 8), (uint8_t)pixels[i] };
        fwrite(rgb, 1, 3, f);
    }
    return fclose(f) == 0;
}

uint32_t oracles_session_late_frames(const oracles_host_run_report *report)
{
    return report->frames_late > report->frames_over_budget ? report->frames_late : report->frames_over_budget;
}

/* A game that ran slowly, said in one line: what the home screen suggests after it, or that nothing is lighter. */
static void report_slow(const session *s, const oracles_host_run_report *report)
{
    const uint32_t late = oracles_session_late_frames(report);
    if (!oracles_settings_ran_slowly(s->enhanced, report->frames_presented, late)) return;
    static const char *const views[3] = { "Near", "Medium", "Far" };
    static const char *const qualities[5] = { "Low", "Medium", "High", "Max", "Custom" };
    const OraclesQuality quality = oracles_settings_quality(s->settings);
    const int view = s->settings->view >= 0 && s->settings->view < 3 ? s->settings->view : 2;
    OraclesQuality lighter;
    int lighter_view;
    fprintf(stderr, "oracles: the game ran slowly: %u of %u frames late (%.1f %%, over 2 %% for a minute or more), at the %s quality",
            late, report->frames_presented, 100.0 * late / report->frames_presented, qualities[quality]);
    if (quality == ORACLES_QUALITY_CUSTOM) fprintf(stderr, " in the %s view", views[view]);
    if (!oracles_settings_lighter(s->settings, &lighter, &lighter_view)) fprintf(stderr, "; nothing lighter to suggest\n");
    else if (lighter == ORACLES_QUALITY_CUSTOM) fprintf(stderr, ": the %s view may play smoother (Display > View)\n", views[lighter_view]);
    else fprintf(stderr, ": %s may play smoother (Display > Quality)\n", qualities[lighter]);
}

void oracles_session_report_frames(const session *s, const oracles_host_run_report *report)
{
    fprintf(stderr, "oracles: version %s, build %s, core %s\n", ORACLES_VERSION, ORACLES_BUILD, oracles_core_version(s->core));
    fprintf(stderr, "oracles: %u frames, %u late, %u resyncs, %u saves written to %s\n",
            report->frames_presented, report->frames_late, report->pacing_resyncs, report->saves_written, s->save.path);
    /* The pause holds the loop between frames: its time is in no frame's; none of its glyphs should be rasterised here. */
    if (s->pause)
        fprintf(stderr, "oracles: pause menu opened %u times; glyphs rasterised during the session's pauses: %u\n",
                report->pauses, oracles_pause_late_glyphs(s->pause));
    if (!report->frames_presented || !report->frame_ns_total) return;
    fprintf(stderr, "oracles: frame work %.2f ms on average, %.2f ms at most (budget 4 ms with hooks); %.2f ms of the average is the presentation%s\n",
            (double)report->frame_ns_total / report->frames_presented / 1e6, (double)report->frame_ns_max / 1e6,
            (double)report->present_ns_total / report->frames_presented / 1e6, s->sdl.vsync ? ", which waits for the display under vsync" : "");
    /* Where the time goes, for the peak a frame's budget allows (12 ms). */
    fprintf(stderr, "oracles: the worst frame, %u: core %.2f ms, view %.2f ms, present %.2f ms, end %.2f ms; each phase at most: core %.2f, view %.2f, present %.2f, end %.2f ms; frames over 12 ms: %u, over one frame (16.7 ms): %u\n",
            report->frame_max_index, (double)report->max_frame_core_ns / 1e6, (double)report->max_frame_source_ns / 1e6,
            (double)report->max_frame_present_ns / 1e6, (double)report->max_frame_end_ns / 1e6,
            (double)report->core_ns_max / 1e6, (double)report->source_ns_max / 1e6, (double)report->present_ns_max / 1e6, (double)report->end_ns_max / 1e6,
            report->frames_over_12ms, report->frames_over_16ms);
    /* The frame budget: past the first second, no
     * frame's work over one frame, and at most one frame in a thousand
     * late.  Said here so that a session judges it without arithmetic. */
    const int held = report->frames_over_budget == 0 && (uint64_t)report->frames_late * 1000u <= report->frames_presented;
    fprintf(stderr, "oracles: frame budget %s: %u frames whose work, presentation left out, took over one frame past the first second (0 allowed), %u late of %u (1 in 1000 allowed; none counted when the display paces)\n",
            held ? "held" : "NOT held", report->frames_over_budget, report->frames_late, report->frames_presented);
    report_slow(s, report);
}

/* What the session's parts have to say at its end, and their release. */
void oracles_session_finish(session *s, const OraclesSessionOptions *o)
{
    if (o->screenshot && s->ran) {
        const uint32_t width = s->view ? oracles_enhanced_view_width(s->view) : ORACLES_SCREEN_WIDTH;
        const uint32_t height = s->view ? oracles_enhanced_view_height(s->view) : ORACLES_SCREEN_HEIGHT;
        const uint32_t *pixels = s->view ? oracles_enhanced_view_surface(s->view) : oracles_core_pixels(s->core);
        const int ok = write_screenshot(o->screenshot, oracles_mod_session_screen(s->mod, pixels, width, height), width, height);
        if (!ok) fprintf(stderr, "oracles: cannot write %s\n", o->screenshot);
    }
    if (s->check) { if (s->ran) oracles_frame_check_report(s->check, stderr); oracles_frame_check_stop(s->check); }
    if (s->transition) {
        OraclesRoomTransitionStats stats;
        oracles_room_transition_stats(s->transition, &stats);
        oracles_session_report_transitions(&stats, stderr);
        oracles_room_transition_stop(s->transition);
    }
    oracles_hotkeys_session_stop(s->hotkeys, stderr);
    if (s->view) {
        /* What the session showed black, counted as the harness counts it (enhanced.black_pixels_mean). */
        OraclesEnhancedBlack black;
        oracles_enhanced_view_black(s->view, &black);
        fprintf(stderr, "oracles: black in the band: %.0f pixels a world frame on average, %u at most, over %u world frames from the first full one (frame %u); rooms of the overworld's grid black whole (in the band by a pixel at least, nothing drawn there) for more than %u world frames in a row: %u",
                black.full_world_frames ? (double)black.pixels / black.full_world_frames : 0.0, black.max, black.full_world_frames, black.first_full,
                ORACLES_ENHANCED_BLACK_ROOM_FRAMES, black.rooms_over);
        for (unsigned i = 0; i < black.rooms_over && i < ORACLES_ENHANCED_BLACK_ROOMS_KEPT; i++)
            fprintf(stderr, "%s%u:%02x from frame %u, %u frames", i ? ", " : " (", black.longest[i].group, black.longest[i].room, black.longest[i].from, black.longest[i].frames);
        fprintf(stderr, "%s\n", black.rooms_over ? ")" : "");
        fprintf(stderr, "oracles: black episodes by cause (episodes, world frames with one, the longest in frames, begun in a transition):");
        for (unsigned k = 0; k < ORACLES_ENHANCED_BLACK_KINDS; k++)
            fprintf(stderr, "%s %s %u, %u, %u, %u", k ? ";" : "", oracles_enhanced_black_kind_name(k), black.kinds[k].episodes, black.kinds[k].frames, black.kinds[k].longest, black.kinds[k].in_transition);
        fprintf(stderr, "\n");
        {   /* Display's Camera: the frames the band was moved by a fraction of a pixel, those Link was drawn apart on */
            unsigned moved = 0, layered = 0, refused = 0;
            oracles_enhanced_view_subpixel_counts(s->view, &moved, &layered, &refused);
            fprintf(stderr, "oracles: camera %s at the end: the band moved by a fraction of a pixel on %u frames, Link drawn apart on %u, refused on %u\n",
                    oracles_enhanced_view_subpixel_on(s->view) ? "smooth" : "pixel", moved, layered, refused);
        }
        if (s->text_box_wide) {
            unsigned world = 0, framed = 0, uncovered = 0;
            oracles_enhanced_view_text_box_counts(s->view, &world, &framed, &uncovered);
            fprintf(stderr, "oracles: wide text box: drawn on %u frames (%u in the world, %u over the framed core), %u frames of a text whose box it could not draw\n",
                    world + framed, world, framed, uncovered);
        }
        oracles_enhanced_view_stop(s->view);
    }
    if (s->diag) oracles_diagnostics_stop(s->diag);
    if (s->ran && s->cheats_used) {
        OraclesGuestCheatCounts c;
        oracles_guest_cheat_counts(s->guest, &c);
        fprintf(stderr, "oracles: cheats: %u hits cost no heart, %u seeds, bombs or Bombchus kept, %u payments cost nothing (%u rupees kept), %u payments of a mod charged\n",
                c.hits, c.ammo_kept, c.payments, c.rupees_kept, c.calls_charged);
    }
    if (s->ran && oracles_guest_text_instant_armed(s->guest)) {
        OraclesGuestTextInstantCounts c;
        oracles_guest_text_instant_counts(s->guest, &c);
        fprintf(stderr, "oracles: instant text: %u presses of A for the text's lines, %u voices kept\n", c.presses, c.voices_kept);
    }
    if (s->ran && (s->compact_text || s->text_box_wide)) {
        OraclesGuestCompactCounts c;
        oracles_guest_compact_counts(s->guest, &c);
        fprintf(stderr, "oracles: %s text: %u lines, %u characters in %u tile columns\n", s->compact_text ? "compact" : "classic", c.lines, c.characters, c.columns);
    }
    oracles_mod_session_stop(s->mod, NULL);
    if (s->ran) oracles_language_session_report(s->language, s->guest, "oracles: ", stderr);
    oracles_language_session_stop(s->language);
    if (s->guest) oracles_guest_detach(s->guest);
    oracles_guest_detach(s->bar_reader);
    if (s->ran && !o->no_window) {
        unsigned underruns = 0, drops = 0, peak_ms = 0, average_ms = 0;
        oracles_sdl_backend_audio_report(&s->backend, &underruns, &drops, &peak_ms, &average_ms);
        fprintf(stderr, "oracles: audio: %u underruns (gaps heard), %u chunks dropped, queue %u ms on average, %u ms at most (target 70 ms; the average is the latency)\n",
                underruns, drops, average_ms, peak_ms);
        const oracles_sdl_underrun *log = NULL;
        const unsigned logged = oracles_sdl_backend_underruns(&s->backend, &log);
        for (unsigned i = 0; i < logged; i++)
            fprintf(stderr, "oracles: audio underrun at frame %u: %.1f ms since the previous chunk, which left %u ms queued; the last presentation took %.1f ms\n",
                    log[i].frame, (double)log[i].since_previous_chunk_ns / 1e6, log[i].previous_chunk_left_ms, (double)log[i].last_present_ns / 1e6);
        if (underruns > logged) fprintf(stderr, "oracles: audio: %u more underruns not listed\n", underruns - logged);
        unsigned presents = 0;
        double seconds = 0.0;
        const char *renderer = NULL;
        int renderer_vsync = 0;
        oracles_sdl_backend_present_report(&s->backend, &presents, &seconds, &renderer, &renderer_vsync);
        fprintf(stderr, "oracles: vsync %s: renderer %s (vsync %s), %u frames presented in %.1f s, %.2f Hz; ",
                s->sdl.vsync ? "on" : "off", renderer, renderer_vsync ? "reported" : "not reported",
                presents, seconds, seconds > 0.0 ? (double)(presents - 1u) / seconds : 0.0);
        if (!s->sdl.vsync) fprintf(stderr, "the host paced the whole run\n");
        else if (s->display_unpaced_frame)
            fprintf(stderr, "the presentation did not wait for the display: vsync off and the host paced from frame %u\n", s->display_unpaced_frame);
        else fprintf(stderr, "the display paced the whole run\n");
        double step_average[3], step_max[3];
        oracles_sdl_backend_present_steps(&s->backend, step_average, step_max);
        fprintf(stderr, "oracles: presentation by step: texture upload %.2f ms, drawing %.2f ms, SDL_RenderPresent %.2f ms on average; at most %.2f, %.2f and %.2f ms\n",
                step_average[0], step_average[1], step_average[2], step_max[0], step_max[1], step_max[2]);
        int fill = 0, filled = 0;
        oracles_sdl_backend_scaling(&s->backend, &fill, &filled);
        fprintf(stderr, "oracles: scaling %s: %s\n", fill ? "fill" : "sharp",
                filled ? "the image filled the screen in its proportions, sampled for pixel art"
                : fill ? "the image at its largest whole scale in a window (Fill applies in fullscreen)" : "the image at its largest whole scale");
    }
    oracles_session_stop_recording(s, "finished");
    if (s->fingerprints) fclose(s->fingerprints);
    oracles_route_free(&s->play);
    if (s->backend_ready) { if (o->no_window) oracles_null_backend_release(&s->backend); else oracles_sdl_backend_release(&s->backend); }
    if (s->core) oracles_core_destroy(s->core);
    free(s->rom);
}
