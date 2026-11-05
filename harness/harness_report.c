/* The harness's reports (harness_internal.h): each check's figures, to the standard error and the summary. */
#include "harness_internal.h"

/* "normal,lcd-0-1,..." into a bit set of classes that must not be empty. */
static unsigned parse_expected_classes(const char *list)
{
    unsigned mask = 0;
    while (list && *list) {
        const char *comma = strchr(list, ',');
        const size_t length = comma ? (size_t)(comma - list) : strlen(list);
        for (unsigned i = 0; i < ORACLES_FRAME_CLASSES; i++) {
            const char *name = oracles_frame_class_name((OraclesFrameClass)i);
            if (strlen(name) == length && strncmp(name, list, length) == 0) mask |= 1u << i;
        }
        list = comma ? comma + 1 : list + length;
    }
    return mask;
}

/* The renderer's verdict against the core, written out and to the summary. */
int harness_report_renderer(run *r, const harness_options *o, FILE *summary)
{
    int verdict_failed = 0;
    if (r->check) {
        oracles_frame_check_report(r->check, stderr);
        char reason[256];
        const int pass = oracles_frame_check_verdict(r->check, parse_expected_classes(o->render_expect), reason, sizeof reason);
        if (!pass) {
            fprintf(stderr, "renderer: FAIL: %s\n", reason);
            verdict_failed = 1;
        } else fprintf(stderr, "renderer: PASS (no mismatch in a compared class, expected classes present, ceilings held)\n");
        if (summary) {
            const OraclesFrameStats *stats = oracles_frame_check_stats(r->check);
            fprintf(summary, "render.verdict=%s\n", pass ? "pass" : "fail");
            for (unsigned k = 0; k < ORACLES_FRAME_CLASSES; k++)
                fprintf(summary, "render.%s.frames=%u\nrender.%s.mismatches=%u\n", oracles_frame_class_name((OraclesFrameClass)k), stats->frames[k], oracles_frame_class_name((OraclesFrameClass)k), stats->mismatches[k]);
            fprintf(summary, "render.late_scroll_lines=%u\n", stats->late_scroll_lines);
            fprintf(summary, "render.first_tile_lines=%u\n", stats->first_tile_lines);
        }
        oracles_frame_check_stop(r->check);
    }
    return verdict_failed;
}

/* What each check found, written to stderr, to its own file and to the summary. */
/* The continuous transitions: what the policy did, and the writes it made
 * at the vblank of a frame the game did not finish, by transition state. */
void harness_report_transitions(run *r, FILE *summary, uint8_t *rom)
{
    /* The guest's table of hooks: a feature whose points did not fit is missing them. */
    if (r->guest) {
        if (oracles_guest_hooks_refused(r->guest))
            fprintf(stderr, "harness: WARNING: the guest's table of hooks is full: %u hook(s) not installed (%u installed)\n", oracles_guest_hooks_refused(r->guest),
                    oracles_guest_hook_count(r->guest));
        if (summary) fprintf(summary, "guest.hooks_installed=%u\nguest.hooks_refused=%u\n", oracles_guest_hook_count(r->guest), oracles_guest_hooks_refused(r->guest));
    }
    if (r->transition) {
        OraclesRoomTransitionStats stats;
        oracles_room_transition_stats(r->transition, &stats);
        fprintf(stderr, "continuous transitions: %u taken over, Link moved on %u frames, held back by a wall ahead on %u, left alone on %u (game/room %u, state %u, Link's state %u, not walking %u, airborne %u, swimming %u, under water %u, holding %u, item or hurt %u, animation data %u, a savestate loaded without the policy's progress %u), no animation %u\n",
                stats.transitions, stats.walked_frames, stats.capped_frames, stats.refused_frames, stats.refused_by[0], stats.refused_by[1], stats.refused_by[2],
                stats.refused_by[3], stats.refused_by[4], stats.refused_by[8], stats.refused_by[9], stats.refused_by[5], stats.refused_by[6], stats.refused_by[7], stats.refused_by[10],
                stats.unanimated_frames);
        fprintf(stderr, "continuous transitions: palettes refreshed after a palette transition on %u\n", stats.palette_refreshes);
        fprintf(stderr, "continuous transitions: swimming (--continuous-swim): %u taken over, Link moved on %u frames; %u started by his momentum at an edge, the direction not held\n", stats.swim_transitions, stats.swum_frames, stats.forced_transitions);
        unsigned by_state[8];
        oracles_guest_vblank_write_counts(r->guest, by_state);
        fprintf(stderr, "continuous transitions: writes at the vblank of a frame the game did not finish: %u during the load (states 3 and 4), %u during the scroll (state 5), %u elsewhere\n",
                by_state[3] + by_state[4], by_state[5], by_state[0] + by_state[1] + by_state[2] + by_state[6] + by_state[7]);
        if (summary) fprintf(summary, "transitions.vblank_policy_skipped=%u\n", oracles_guest_vblank_policy_skipped(r->guest));
        if (summary) fprintf(summary, "transitions.swim_taken=%u\ntransitions.swum_frames=%u\ntransitions.swim_forced=%u\n", stats.swim_transitions, stats.swum_frames, stats.forced_transitions);
        fprintf(stderr, "continuous transitions: transitions Link swam into and came out of on land where the straight path stays in water: %u\n", r->swim_dropped);
        if (summary) fprintf(summary, "transitions.swim_dropped=%u\n", r->swim_dropped);
        if (r->wall_measured) {
            fprintf(stderr, "continuous transitions: on foot on land, scrolls that left Link in a wall of the room entered, where the game's walk never stands him: %u (%d px deep at most); "
                            "frames of a scroll he was so in one, its collisions loaded, the game's own move through it included: %u\n", r->wall_arrivals, r->wall_arrival_depth, r->wall_scroll_frames);
            if (summary) fprintf(summary, "transitions.wall_arrivals=%u\ntransitions.wall_arrival_depth=%d\ntransitions.wall_scroll_frames=%u\n", r->wall_arrivals, r->wall_arrival_depth, r->wall_scroll_frames);
        }
        fprintf(stderr, "continuous transitions: glides in the water, keys let go until his motion stops, against the game's swim of that motion alone: in a room %u (%u otherwise: a wall), "
                        "across a scroll %u (%u otherwise, by %d/256 px at most)\n", r->glides, r->glides_unlike, r->edge_glides, r->edge_glides_unlike, r->edge_glide_off_max);
        if (summary) fprintf(summary, "swim.glides=%u\nswim.glides_unlike=%u\nswim.edge_glides=%u\nswim.edge_glides_unlike=%u\nswim.edge_glide_off_max=%d\n",
                             r->glides, r->glides_unlike, r->edge_glides, r->edge_glides_unlike, r->edge_glide_off_max);
        fprintf(stderr, "continuous transitions: frames of a scroll swum %u, of them moved otherwise than his motion %u\n", r->swim_scroll_frames, r->swim_scroll_off_frames);
        if (summary) fprintf(summary, "swim.scroll_frames=%u\nswim.scroll_off_frames=%u\n", r->swim_scroll_frames, r->swim_scroll_off_frames);
        fprintf(stderr, "continuous transitions: a diagonal held (--continuous-diagonal) carried Link across the axis on %u frames, withheld on %u (a collision across, the room's collisions not loaded)\n", oracles_guest_transition_carried(r->guest), stats.carry_withheld_frames);
        if (summary) fprintf(summary, "transitions.carried_frames=%u\ntransitions.carry_asked_frames=%u\ntransitions.carry_withheld_frames=%u\n", oracles_guest_transition_carried(r->guest), stats.carry_asked_frames, stats.carry_withheld_frames);
        if (summary) fprintf(summary, "transitions.resumed_frames=%u\n", stats.refused_by[10]);
        fprintf(stderr, "continuous transitions: frames of a scene's walk across an edge with the game's own keys, left to the game: %u\n", stats.simulated_frames);
        if (summary) fprintf(summary, "transitions.simulated_frames=%u\n", stats.simulated_frames);
        fprintf(stderr, "continuous transitions: scrolls whose last frame gave Link his step (--continuous-steps) %u, left him still going on %u; starts that moved him further than a step %u; "
                        "frames with no direction held toward the edge, the game's own move (--continuous-steps) %u; carries held short of the edge across %u; "
                        "respawns in the room entered moved where the game would have left him %u\n",
                oracles_guest_transition_end_steps(r->guest), stats.end_stalls, stats.doubled_starts, stats.released_frames, stats.carry_edge_frames,
                oracles_guest_transition_respawns(r->guest));
        if (summary) fprintf(summary, "transitions.end_steps=%u\ntransitions.end_stalls=%u\ntransitions.doubled_starts=%u\ntransitions.released_frames=%u\ntransitions.carry_edge_frames=%u\n"
                                      "transitions.respawns_moved=%u\n",
                             oracles_guest_transition_end_steps(r->guest), stats.end_stalls, stats.doubled_starts, stats.released_frames, stats.carry_edge_frames,
                             oracles_guest_transition_respawns(r->guest));
        if (oracles_guest_transition_line(r->guest)) {
            OraclesGuestLineCounts line;
            oracles_guest_line_counts(r->guest, &line);
            fprintf(stderr, "continuous transitions: at the line (--continuous-line), edge tests the game's bound would have held with Link inside the room %u, held at the line %u "
                            "(below zero %u), left to the game and held %u; frames of a scroll he stood with no direction held %u (in the game's standing pose %u), walked back %u (held at the line %u, past it into the room left %u, held there by a wall %u; transitions back forced from there %u of %u asked, kept untaken %u frames, lowered %u; put back on the line %u)\n",
                    line.rests, line.clamps, line.wraps, line.native_backs, stats.stopped_frames, stats.standing_frames, stats.back_frames, stats.back_line_frames,
                    stats.back_past_frames, stats.back_wall_frames, oracles_guest_transition_back_forced(r->guest), stats.back_reversals, stats.back_kept_frames, stats.back_lowered, oracles_guest_transition_put_back(r->guest));
            if (summary) fprintf(summary, "transitions.line_rests=%u\ntransitions.line_clamps=%u\ntransitions.line_wraps=%u\ntransitions.line_native_backs=%u\n"
                                          "transitions.stopped_frames=%u\ntransitions.standing_frames=%u\ntransitions.back_frames=%u\ntransitions.back_line_frames=%u\ntransitions.back_past_frames=%u\ntransitions.back_wall_frames=%u\ntransitions.back_reversals=%u\ntransitions.back_forced=%u\ntransitions.back_lowered=%u\ntransitions.back_kept_frames=%u\ntransitions.put_back=%u\n",
                                 line.rests, line.clamps, line.wraps, line.native_backs, stats.stopped_frames, stats.standing_frames, stats.back_frames, stats.back_line_frames,
                                 stats.back_past_frames, stats.back_wall_frames, stats.back_reversals, oracles_guest_transition_back_forced(r->guest), stats.back_lowered, stats.back_kept_frames, oracles_guest_transition_put_back(r->guest));
            fprintf(stderr, "continuous transitions: rooms beside the room entered across (--continuous-line) predicted %u (%u the same as the game's load of them, %u not, %u not entered next), "
                            "left to the slowed carry %u (", stats.beside_predicted, stats.beside_matched, stats.beside_mismatched, stats.beside_unchecked, stats.beside_fallbacks);
            for (unsigned v = 1, listed = 0; v < ORACLES_ROOM_PREDICT_VERDICTS; v++)
                if (stats.beside_fallback_by[v]) fprintf(stderr, "%s%s %u", listed++ ? ", " : "", oracles_room_predict_verdict_name((OraclesRoomPredictVerdict)v), stats.beside_fallback_by[v]);
            fprintf(stderr, "); carried past the line into them on %u frames, held as far as it goes on %u, by their walls, holes or water on %u, walked back to the line, the transitions disabled, on %u; scrolls into them started from where he stood %u of %u asked\n",
                    stats.beside_past_frames, stats.beside_held_frames, stats.beside_wall_frames, stats.beside_returned_frames, oracles_guest_transition_cross_started(r->guest), stats.beside_crossings);
            if (summary) fprintf(summary, "transitions.beside_predicted=%u\ntransitions.beside_fallbacks=%u\ntransitions.beside_matched=%u\ntransitions.beside_mismatched=%u\ntransitions.beside_unchecked=%u\n"
                                          "transitions.beside_past_frames=%u\ntransitions.beside_held_frames=%u\ntransitions.beside_wall_frames=%u\ntransitions.beside_crossings=%u\ntransitions.cross_started=%u\n"
                                          "transitions.beside_scene_rooms=%u\ntransitions.beside_returned_frames=%u\n",
                                 stats.beside_predicted, stats.beside_fallbacks, stats.beside_matched, stats.beside_mismatched, stats.beside_unchecked,
                                 stats.beside_past_frames, stats.beside_held_frames, stats.beside_wall_frames, stats.beside_crossings, oracles_guest_transition_cross_started(r->guest),
                                 stats.beside_fallback_by[ORACLES_ROOM_PREDICT_SCENE], stats.beside_returned_frames);
            fprintf(stderr, "continuous transitions: swims at the line (--continuous-line), the game's swim stepped by the policy against the game's own on %u frames of normal play, "
                            "%u different (the first at %u); scroll frames swum by that step %u (a move held %u, made across untested %u), the motion written %u\n",
                    stats.swim_model_frames, stats.swim_model_mismatched, stats.swim_model_first_mismatch, stats.swim_line_frames, stats.swim_held_frames, stats.swim_untested_frames,
                    oracles_guest_transition_swim_steps(r->guest));
            fprintf(stderr, "continuous transitions: on foot past the game's own place (--continuous-line), steps along the axis held by a wall of the room entered on %u frames, by its collisions unknown on %u\n",
                    stats.line_wall_frames, stats.line_unknown_frames);
            if (summary) fprintf(summary, "transitions.line_wall_frames=%u\ntransitions.line_unknown_frames=%u\n", stats.line_wall_frames, stats.line_unknown_frames);
            if (summary) fprintf(summary, "transitions.swim_model_frames=%u\ntransitions.swim_model_mismatched=%u\ntransitions.swim_line_frames=%u\ntransitions.swim_held_frames=%u\n"
                                          "transitions.swim_untested_frames=%u\ntransitions.swim_steps=%u\ntransitions.swim_lifted_frames=%u\n",
                                 stats.swim_model_frames, stats.swim_model_mismatched, stats.swim_line_frames, stats.swim_held_frames, stats.swim_untested_frames,
                                 oracles_guest_transition_swim_steps(r->guest), stats.swim_lifted_frames);
        }
        fprintf(stderr, "continuous transitions: the room entered's collisions predicted at %u transitions while the game loaded them (%u the same as the game's, %u not), "
                        "%u still loading as the replay ends, %u left to the load (", stats.predicted_transitions, stats.predicted_matched, stats.predicted_mismatched,
                stats.predicted_pending, stats.predict_fallbacks);
        for (unsigned v = 1, listed = 0; v < ORACLES_ROOM_PREDICT_VERDICTS; v++)
            if (stats.predict_fallback_by[v]) fprintf(stderr, "%s%s %u", listed++ ? ", " : "", oracles_room_predict_verdict_name((OraclesRoomPredictVerdict)v), stats.predict_fallback_by[v]);
        fprintf(stderr, "); a carry tested on them on %u frames, withheld for want of them on %u (%u before the game named the room)\n", stats.carry_predicted_frames,
                stats.carry_unknown_frames, stats.carry_unnamed_frames);
        if (summary) fprintf(summary, "transitions.predicted=%u\ntransitions.predicted_matched=%u\ntransitions.predicted_mismatched=%u\ntransitions.predicted_pending=%u\n"
                                      "transitions.predict_fallbacks=%u\ntransitions.carry_predicted_frames=%u\ntransitions.carry_unknown_frames=%u\n",
                             stats.predicted_transitions, stats.predicted_matched, stats.predicted_mismatched, stats.predicted_pending, stats.predict_fallbacks,
                             stats.carry_predicted_frames, stats.carry_unknown_frames);
        if (summary) fprintf(summary, "transitions.taken=%u\ntransitions.walked_frames=%u\ntransitions.refused_frames=%u\ntransitions.unanimated_frames=%u\ntransitions.palette_refreshes=%u\ntransitions.vblank_load_writes=%u\ntransitions.vblank_scroll_writes=%u\ntransitions.vblank_other_writes=%u\n",
                             stats.transitions, stats.walked_frames, stats.refused_frames, stats.unanimated_frames, stats.palette_refreshes,
                             by_state[3] + by_state[4], by_state[5], by_state[0] + by_state[1] + by_state[2] + by_state[6] + by_state[7]);
        oracles_room_transition_stop(r->transition);
        free(rom);
    }
}

void harness_report_checks(run *r, const harness_options *o, FILE *summary, uint8_t *rom)
{
    harness_report_transitions(r, summary, rom);
    if (o->swim_hold || r->swim_changed) {
        unsigned mermaid = 0, flippers = 0;
        oracles_guest_swim_strokes(r->guest, &mermaid, &flippers);
        fprintf(stderr, "swim's hold: strokes started by a held button: %u with the mermaid suit, %u with the flippers\n", mermaid, flippers);
        if (summary) fprintf(summary, "swim.mermaid_strokes=%u\nswim.flippers_strokes=%u\n", mermaid, flippers);
    }
    if (o->text_instant || r->text_speed_changed) {
        oracles_text_speed_check_report(&r->text_speed, r->guest, stderr);
        if (summary) oracles_text_speed_check_summary(&r->text_speed, r->guest, summary);
    }
    if (o->cheats || r->cheats_changed) {
        OraclesGuestCheatCounts c;
        oracles_guest_cheat_counts(r->guest, &c);
        fprintf(stderr, "cheats: %u hits cost no heart (%u quarters kept), %u seeds, bombs or Bombchus kept, %u payments cost nothing (%u rupees kept), %u payments of a mod charged\n",
                c.hits, c.health_kept, c.ammo_kept, c.payments, c.rupees_kept, c.calls_charged);
        if (summary) fprintf(summary, "cheats.damage_cancelled=%u\ncheats.health_kept=%u\ncheats.ammo_kept=%u\ncheats.payments_cancelled=%u\ncheats.rupees_kept=%u\ncheats.mod_payments=%u\n",
                             c.hits, c.health_kept, c.ammo_kept, c.payments, c.rupees_kept, c.calls_charged);
    }
    if (o->compact_text || o->text_box_wide) {
        OraclesGuestCompactCounts c;
        oracles_guest_compact_counts(r->guest, &c);
        fprintf(stderr, "compact text: %u lines, %u characters in %u tile columns, %u cursor places moved, %u lines of more than sixteen characters, %u characters past a line\n",
                c.lines, c.characters, c.columns, c.places, c.long_lines, c.overflows);
        if (summary) fprintf(summary, "text.compact_lines=%u\ntext.compact_characters=%u\ntext.compact_columns=%u\ntext.compact_places=%u\ntext.compact_long_lines=%u\ntext.compact_overflows=%u\n",
                             c.lines, c.characters, c.columns, c.places, c.long_lines, c.overflows);
        fprintf(stderr, "compact text's reveal: %u vblanks of a line revealed, %u tile columns written with the pixels revealed alone, %u that would show a letter cut, "
                        "%u a letter early\n", c.reveal_frames, c.partial_writes, c.cut_frames, c.early_frames);
        if (summary) fprintf(summary, "text.reveal_frames=%u\ntext.reveal_partial_writes=%u\ntext.reveal_cut_frames=%u\ntext.reveal_early_frames=%u\n",
                             c.reveal_frames, c.partial_writes, c.cut_frames, c.early_frames);
    }
    if (r->guest) {
        unsigned overflow = 0, purged = 0;
        oracles_guest_dropped_returns(r->guest, &overflow, &purged);
        if (overflow || oracles_guest_journal_dropped(r->guest))
            fprintf(stderr, "harness: hooks lost %u return captures to overflow; the journal dropped %zu writes\n", overflow, oracles_guest_journal_dropped(r->guest));
    }
    if (r->enhanced) {
        oracles_enhanced_check_report(r->enhanced, stderr);
        char path[4096];
        snprintf(path, sizeof path, "%s/report.txt", o->enhanced_dir);
        FILE *f = fopen(path, "w");
        if (f) { oracles_enhanced_check_report(r->enhanced, f); fclose(f); }
        if (summary) oracles_enhanced_check_summary(r->enhanced, summary);
        oracles_enhanced_check_stop(r->enhanced);
    }
    if (r->ghost) {
        oracles_ghost_check_report(r->ghost, stderr);
        oracles_ghost_check_write_reads(r->ghost, o->ghost_dir);
        char path[4096];
        snprintf(path, sizeof path, "%s/report.txt", o->ghost_dir);
        FILE *f = fopen(path, "w");
        if (f) { oracles_ghost_check_report(r->ghost, f); fclose(f); }
        if (summary) oracles_ghost_check_summary(r->ghost, summary);
        oracles_ghost_check_stop(r->ghost);
    }
    if (r->neighbours) {
        oracles_neighbour_check_report(r->neighbours, stdout);
        if (summary) oracles_neighbour_check_summary(r->neighbours, summary);
        oracles_neighbour_check_stop(r->neighbours);
    }
    if (r->object_animation) {
        oracles_object_animation_check_report(r->object_animation, stdout);
        if (summary) oracles_object_animation_check_summary(r->object_animation, summary);
        oracles_object_animation_check_stop(r->object_animation);
    }
    if (r->animation) {
        oracles_animation_check_report(r->animation, stderr);
        if (summary) oracles_animation_check_summary(r->animation, summary);
        oracles_animation_check_stop(r->animation);
        oracles_object_sprites_check_report(r->object_sprites, stderr);
        if (summary) oracles_object_sprites_check_summary(r->object_sprites, summary);
        oracles_object_sprites_check_stop(r->object_sprites);
    }
    if (r->coverage) {
        if (summary) oracles_coverage_summary(r->coverage, summary);
        oracles_coverage_destroy(r->coverage);
    }
}

/* The replay of a route's exchanges: 1 when it failed. */
int harness_report_hotkeys(run *r, FILE *summary)
{
    if (r->hotkeys_live) {
        if (summary) oracles_hotkeys_live_summary(r->hotkeys_live, summary);
        return oracles_hotkeys_live_stop(r->hotkeys_live, stderr);
    }
    if (!r->hotkeys) return 0;
    oracles_hotkeys_check_report(r->hotkeys, stderr);
    const int failed = oracles_hotkeys_check_failed(r->hotkeys);
    if (failed) fprintf(stderr, "item hotkeys: FAIL\n");
    if (summary) oracles_hotkeys_check_summary(r->hotkeys, summary);
    oracles_hotkeys_check_stop(r->hotkeys);
    return failed;
}

/* The language, the texts the game looked up in it and the graphics composed in it, to stderr and the summary. */
void harness_report_language(run *r, FILE *summary)
{
    if (!r->language) return;
    oracles_language_session_report(r->language, r->guest, "harness: ", stderr);
    unsigned served = 0, english = 0, missing = 0;
    oracles_guest_language_counts(r->guest, &served, &english, &missing);
    if (summary) fprintf(summary, "language.code=%s\nlanguage.texts_served=%u\nlanguage.texts_english=%u\nlanguage.texts_missing=%u\nlanguage.graphics=%u\n",
                         oracles_guest_language(r->guest), served, english, missing, oracles_language_session_graphics(r->language));
    /* The compact text's layout of the language's texts, and those of them the game looked up. */
    const OraclesLanguageReport *c = oracles_language_session_composed(r->language);
    if (summary && c && (c->laid_out || c->layout_kept || c->layout_failed))
        fprintf(summary, "text.laid_out=%u\ntext.layout_kept=%u\ntext.layout_failed=%u\ntext.boxes=%u\ntext.boxes_laid_out=%u\ntext.laid_out_served=%u\n",
                c->laid_out, c->layout_kept, c->layout_failed, c->boxes, c->boxes_laid_out, oracles_guest_language_laid_out(r->guest));
    oracles_language_session_stop(r->language);
}
