/* The Enhanced check: the frame rows and the figures written out. */
#include "enhanced_check_internal.h"

/* One row of DIR/frames.tsv. */
void ec_write_frame_row(OraclesEnhancedCheck *c, uint32_t frame, uint64_t hash, OraclesEnhancedMode mode, int32_t camera_x, int32_t camera_y,
                            int left_shown, uint8_t left_room, int right_shown, uint8_t right_room, unsigned uncovered, int scx, int scy, int off_x, int off_y)
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    const OraclesEnhancedObservation *ob = oracles_enhanced_view_observation(c->view);
    const uint8_t *link = oracles_guest_object(c->guest, 0, 0);
    fprintf(c->tsv, "%u\t%016" PRIx64 "\t%s\t%" PRId32 "\t%u\t%02x\t%u\t%u\t%02x\t%02x\t%" PRId32 "\t%" PRId32 "\t%" PRIu64,
            frame, hash, mode == ORACLES_ENHANCED_WORLD ? "world" : "framed", camera_x,
            oracles_guest_read8(c->guest, t->active_group), oracles_guest_read8(c->guest, t->active_room),
            link ? link[ORACLES_OBJ_XH] : 0u, link ? link[ORACLES_OBJ_YH] : 0u,
            oracles_guest_read8(c->guest, t->scroll_mode), oracles_guest_read8(c->guest, t->screen_transition_state),
            ob->world.link_x, ob->window_left, ob->epoch);
    /* The OAM: entries in use, and the first one (Link's first sprite in play). */
    const uint8_t *oam = oracles_guest_oam(c->guest);
    unsigned oam_used = 0;
    for (unsigned i = 0; oam && i < 40u; i++) if (oam[i * 4] != 0 && oam[i * 4] < 160u) oam_used++;
    fprintf(c->tsv, "\t%s%02x\t%s%02x\t%u\t%d\t%d\t%d\t%d\t%d\t%d\t%u\t%u\t%u\t%02x\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%u\t%d\t%d\t%d\t%" PRId32 "\t%" PRId32 "\t%u\t%u\n", left_shown ? "+" : "-", left_room, right_shown ? "+" : "-", right_room, uncovered, scx, scy,
            (int16_t)oracles_guest_read16(c->guest, t->camera_x), (int16_t)oracles_guest_read16(c->guest, t->camera_y), off_x, off_y,
            oam_used, oam ? oam[0] : 0u, oam ? oam[1] : 0u, oracles_guest_io(c->guest) ? oracles_guest_io(c->guest)[0x40] : 0u,
            camera_y, ob->world.link_y, ob->window_top, oracles_guest_read8(c->guest, t->opened_menu_type),
            (int)(int8_t)oracles_guest_read8(c->guest, (OraclesGuestSym){ t->thread_state_buffer.bank, (uint16_t)(t->thread_state_buffer.addr + 0x1fu) }),
            oracles_enhanced_view_isolated(c->view), ob->large, ob->world.origin_x, ob->world.origin_y, c->window_luma, c->ring_luma);
}

/* The figures of the band itself: the frames, the cost of a composition,
 * the objects handed over, the camera and the ghost's runs. */
static void summary_of_the_band(OraclesEnhancedCheck *c, FILE *out)
{
    unsigned requested = 0, completed = 0, failed = 0, reasons[5] = { 0, 0, 0, 0, 0 };
    oracles_enhanced_view_ghost_counts(c->view, &requested, &completed, &failed);
    oracles_enhanced_view_ghost_failures(c->view, reasons);
    fprintf(out, "enhanced.run_hash=%016" PRIx64 "\nenhanced.frames=%u\nenhanced.world_frames=%u\nenhanced.framed_frames=%u\n", c->run_hash, c->frames, c->world_frames, c->framed_frames);
    fprintf(out, "enhanced.enlarged_frames=%u\nenhanced.enlarged_runs=%u\nenhanced.enlarged_framed_drops=%u\n", c->enlarged_frames, c->enlarged_runs, c->enlarged_framed_drops);
    fprintf(out, "enhanced.normal_band_frames=%u\n", c->normal_band_frames);
    fprintf(out, "enhanced.compose_us_mean=%.0f\nenhanced.compose_us_max=%.0f\n", c->frames ? c->compose_us_total / c->frames : 0.0, c->compose_us_max);
    {
        unsigned fade_frames = 0, faded = 0;
        oracles_enhanced_view_fade_counts(c->view, &fade_frames, &faded);
        fprintf(out, "enhanced.fade_frames=%u\nenhanced.faded_objects=%u\n", fade_frames, faded);
        oracles_enhanced_view_fade_out_counts(c->view, &fade_frames, &faded);
        fprintf(out, "enhanced.fade_out_frames=%u\nenhanced.faded_out_objects=%u\n", fade_frames, faded);
        unsigned tint_frames = 0;
        uint32_t tint_first = 0, tint_last = 0;
        oracles_enhanced_view_scene_tint_counts(c->view, &tint_frames, &tint_first, &tint_last);
        fprintf(out, "enhanced.scene_tint_frames=%u\nenhanced.scene_tint_first=%" PRIu32 "\nenhanced.scene_tint_last=%" PRIu32 "\n", tint_frames, tint_first, tint_last);
        fprintf(out, "enhanced.window_darker_frames=%u\nenhanced.window_darker_first=%" PRIu32 "\nenhanced.window_darker_last=%" PRIu32 "\n", c->window_darker_frames, c->window_darker_first, c->window_darker_last);
        unsigned shown = 0, jumps = 0, holes = 0, unseen = 0;
        oracles_enhanced_view_handoff_counts(c->view, &shown, &jumps, &holes, &unseen);
        fprintf(out, "enhanced.handoff_shown=%u\nenhanced.handoff_jumps=%u\nenhanced.handoff_hole_frames=%u\nenhanced.handoff_unseen=%u\n", shown, jumps, holes, unseen);
    }
    fprintf(out, "enhanced.link_unshown_frames=%u\nenhanced.link_built_frames=%u\n", oracles_enhanced_view_link_unshown_frames(c->view),
            oracles_enhanced_view_link_built_frames(c->view));
    {
        unsigned built = 0, unshown = 0;
        oracles_enhanced_view_link_chained_frames(c->view, &built, &unshown);
        fprintf(out, "enhanced.link_chained_frames=%u\nenhanced.link_chained_unshown=%u\n", built, unshown);
    }
    fprintf(out, "enhanced.link_left_line_frames=%u\n", oracles_enhanced_view_link_left_line_frames(c->view));
    fprintf(out, "enhanced.camera_jumps=%u\nenhanced.largest_jump=%" PRId32 "\nenhanced.camera_jumps_y=%u\nenhanced.largest_jump_y=%" PRId32 "\n", c->camera_jumps, c->largest_jump, c->camera_jumps_y, c->largest_jump_y);
    fprintf(out, "enhanced.link_jumps=%u\nenhanced.link_jumps_y=%u\nenhanced.window_jumps=%u\n", c->link_jumps, c->link_jumps_y, c->window_jumps);
    fprintf(out, "enhanced.link_drawn_steps=%u\nenhanced.link_backsteps=%u\nenhanced.link_backsteps_y=%u\n", c->link_drawn_steps, c->link_backsteps, c->link_backsteps_y);
    {
        unsigned moved = 0, engaged = 0;
        oracles_enhanced_view_link_phase_counts(c->view, &moved, &engaged);
        fprintf(out, "enhanced.diagonal_frames=%u\nenhanced.camera_wobble=%u\nenhanced.link_phase_frames=%u\nenhanced.link_phase_engaged=%u\n", c->diagonal_frames, c->camera_wobble, moved, engaged);
        unsigned sub = 0, layered = 0, refused = 0, left_pixels = 0, wrong_pixels = 0, alike_pixels = 0;
        oracles_enhanced_view_subpixel_counts(c->view, &sub, &layered, &refused);
        oracles_enhanced_view_subpixel_pixel_counts(c->view, &left_pixels, &wrong_pixels, &alike_pixels);
        fprintf(out, "enhanced.band_stops=%u\nenhanced.band_wobble=%u\nenhanced.straight_frames=%u\nenhanced.straight_band_stops=%u\nenhanced.link_screen_moves=%u\nenhanced.link_screen_backsteps=%u\n"
                     "enhanced.subpixel_frames=%u\nenhanced.subpixel_layer_frames=%u\nenhanced.subpixel_refused_frames=%u\nenhanced.subpixel_left_pixels=%u\nenhanced.subpixel_wrong_pixels=%u\nenhanced.subpixel_alike_pixels=%u\n",
                c->band_stops, c->band_wobble, c->straight_frames, c->straight_band_stops, c->link_screen_moves, c->link_screen_backsteps, sub, layered, refused, left_pixels, wrong_pixels, alike_pixels);
        fprintf(out, "enhanced.link_wobble=%u\nenhanced.scroll_link_dips=%u\nenhanced.link_drawn_zigzags=%u\n", c->link_wobble, c->scroll_link_dips, c->link_drawn_zigzags);
        fprintf(out, "swim.drawn_scroll_frames=%u\nswim.drawn_off_scroll_frames=%u\nswim.drawn_mid_frames=%u\nswim.drawn_off_mid_frames=%u\nswim.drawn_off_max=%" PRId32 "\nswim.drawn_off_built_frames=%u\n"
                     "swim.band_jerks_scroll=%u\nswim.band_jerks_mid=%u\nswim.walked_scroll_frames=%u\nswim.left_water_past_room=%u\n",
                c->swim_drawn.scroll_frames, c->swim_drawn.off_scroll_frames, c->swim_drawn.mid_frames, c->swim_drawn.off_mid_frames, c->swim_drawn.off_max, c->swim_drawn.off_built_frames,
                c->swim_band_jerks_scroll, c->swim_band_jerks_mid, c->swim_walked_scroll_frames, c->swim_left_water_past_room);
        fprintf(out, "walk.drawn_scroll_frames=%u\nwalk.drawn_off_scroll_frames=%u\nwalk.drawn_mid_frames=%u\nwalk.drawn_off_mid_frames=%u\nwalk.drawn_off_max=%" PRId32 "\nwalk.drawn_off_built_frames=%u\n",
                c->walk_drawn.scroll_frames, c->walk_drawn.off_scroll_frames, c->walk_drawn.mid_frames, c->walk_drawn.off_mid_frames, c->walk_drawn.off_max, c->walk_drawn.off_built_frames);
        fprintf(out, "enhanced.subpixel_covered_pixels=%u\nenhanced.subpixel_unexplained_frames=%u\n", oracles_enhanced_view_subpixel_covered_pixels(c->view),
                oracles_enhanced_view_subpixel_unexplained_frames(c->view));
        fprintf(out, "enhanced.subpixel_objects=%u\nenhanced.subpixel_objects_off=%u\nenhanced.subpixel_objects_left=%u\n", c->layer_objects, c->layer_objects_off,
                oracles_enhanced_view_subpixel_objects_left(c->view));
        fprintf(out, "enhanced.switch_edges=%u\nenhanced.switch_band_jolt_max=%" PRId32 "\nenhanced.switch_link_jolt_max=%" PRId32 "\nenhanced.band_jolt_max=%" PRId32 "\nenhanced.link_jolt_max=%" PRId32 "\n",
                c->switch_edges, c->switch_band_jolt_max, c->switch_link_jolt_max, c->band_jolt_max, c->link_jolt_max);
        fprintf(out, "enhanced.alone_scroll_frames=%u\nenhanced.alone_scroll_band_moves=%u\nenhanced.alone_scroll_cut_frames=%u\n", c->alone_scroll_frames, c->alone_scroll_band_moves,
                c->alone_scroll_cut_frames);
        fprintf(out, "enhanced.transition_layer_switches=%u\nenhanced.transition_layer_shift_max=%" PRId32 "\nenhanced.subpixel_switched=%u\n", c->transition_layer_switches,
                c->transition_layer_shift_max, c->subpixel_switched);
    }
    fprintf(out, "enhanced.link_edge_margin_min=%" PRId32 "\nenhanced.link_near_edge_frames=%u\n", c->link_edge_margin_min, c->link_near_edge_frames);
    fprintf(out, "enhanced.ghost_requested=%u\nenhanced.ghost_completed=%u\nenhanced.ghost_failed=%u\nenhanced.ghost_not_primeable=%u\nenhanced.ghost_load_failed=%u\nenhanced.ghost_never_settled=%u\nenhanced.ghost_wrong_room=%u\nenhanced.ghost_other=%u\n",
            requested, completed, failed, reasons[0], reasons[1], reasons[2], reasons[3], reasons[4]);
}

/* The figures of what the band showed: the neighbours entered, the scrolls,
 * the curtain, the text box, the animation and the objects of a large room. */
static void summary_of_what_was_shown(OraclesEnhancedCheck *c, FILE *out)
{
    fprintf(out, "enhanced.transitions=%u\nenhanced.transitions_vertical=%u\nenhanced.transitions_shown=%u\nenhanced.transitions_black=%u\nenhanced.transitions_other_room=%u\nenhanced.transitions_large=%u\nenhanced.large_scroll_black=%u\nenhanced.terrains_equal=%u\nenhanced.terrains_different=%u\n",
            c->transitions, c->transitions_vertical, c->transitions_with_terrain, c->transitions_black, c->transitions_other_room, c->transitions_large, c->large_scroll_black, c->terrains_equal, c->terrains_different);
    fprintf(out, "enhanced.terrains_old=%u\n", c->terrains_old);
    fprintf(out, "enhanced.scrolls_diagonal=%u\nenhanced.scroll_across_stalled=%u\n", c->scrolls_diagonal, c->scroll_across_stalled);
    fprintf(out, "enhanced.scrolls=%u\nenhanced.scroll_frames=%u\nenhanced.scroll_frozen_frames=%u\nenhanced.scroll_solid_arrivals=%u\nenhanced.window_checked=%u\nenhanced.window_misplaced=%u\nenhanced.off_camera_frames=%u\nenhanced.world_off_camera_frames=%u\nenhanced.wave_frames=%u\nenhanced.shake_frames=%u\nenhanced.window_lines_checked=%u\nenhanced.window_lines_wrong=%u\n",
            c->transitions_seen, c->transition_frames_total, c->transition_frozen_total, c->transition_arrivals_solid, c->window_checked, c->window_misplaced, oracles_enhanced_view_off_camera_frames(c->view), oracles_enhanced_view_world_off_camera_frames(c->view), oracles_enhanced_view_wave_frames(c->view), oracles_enhanced_view_shake_frames(c->view), c->window_lines_checked, c->window_lines_wrong);
    fprintf(out, "enhanced.curtain_frames=%u\nenhanced.curtain_frames_wrong=%u\nenhanced.curtain_frames_unchecked=%u\nenhanced.curtain_pixels_wrong=%u\n",
            c->curtain_frames, c->curtain_frames_wrong, c->curtain_frames_unchecked, c->curtain_pixels_wrong);
    fprintf(out, "enhanced.text_box_frames_in_place=%u\nenhanced.curtain_window_frames_wrong=%u\n", c->text_box_frames_in_place, c->curtain_window_frames_wrong);
    fprintf(out, "enhanced.blurb_frames=%u\nenhanced.blurb_frames_in_window=%u\n", c->blurb_frames, c->blurb_frames_in_window);
    fprintf(out, "enhanced.neighbour_waiting_frames=%u\n", oracles_enhanced_view_waiting_frames(c->view));
    {   /* the rooms never visited: those kept dark, and the reference room the game marks visited, whatever the setting */
        OraclesEnhancedUnvisitedCounts u;
        oracles_enhanced_view_unvisited_counts(c->view, &u);
        fprintf(out, "enhanced.hidden_frames=%u\nenhanced.hidden_rooms_max=%u\nenhanced.reveal_fades=%u\nenhanced.edge_fades=%u\nenhanced.transitions_dark=%u\n"
                "enhanced.dark_visited=%u\nenhanced.shown_unvisited=%u\nenhanced.dark_edge_stray=%u\nenhanced.unvisited_reference=%u\n", u.hidden_frames, u.hidden_rooms_max,
                u.reveal_fades, u.edge_fades, c->transitions_dark, c->dark_visited, c->shown_unvisited, c->dark_edge_stray, u.unvisited_reference);
        if (u.unvisited_reference) fprintf(out, "enhanced.unvisited_reference_first=%u:%u:%02x\n", u.first_frame, u.first_group, u.first_room);
    }
    fprintf(out, "enhanced.continued_wave_frames=%u\n", oracles_enhanced_view_continued_wave_frames(c->view));
    fprintf(out, "enhanced.sway_frames=%u\n", oracles_enhanced_view_sway_frames(c->view));
    for (unsigned k = 0; k < 2u; k++) {
        const EcRipple *r = k ? &c->water : &c->whole;
        const char *name = k ? "water" : "whole_room";
        fprintf(out, "enhanced.%s_wave_frames=%u\nenhanced.%s_rippled_frames=%u\nenhanced.%s_edge_moves=%u\n", name, r->wave_frames, name, r->rippled_frames, name, r->edge_moves);
        fprintf(out, "enhanced.%s_row_changes_moving=%u\nenhanced.%s_row_changes_still=%u\nenhanced.%s_row_changes_max=%u\n", name,
                r->moving_frames ? 100u * r->moving_changes / r->moving_frames : 0u, name, r->still_frames ? 100u * r->still_changes / r->still_frames : 0u, name, r->changes_max);
    }
    fprintf(out, "enhanced.routed_neighbours=%u\n", oracles_enhanced_view_routed_delivered(c->view));
    {   /* the hotbar of the item hotkeys, when the run shows one */
        unsigned captured = 0, shown = 0, checked = 0, wrong = 0, outside = 0;
        oracles_enhanced_view_hotbar_counts(c->view, &captured, &shown);
        oracles_enhanced_view_hotbar_proof(c->view, &checked, &wrong, &outside);
        fprintf(out, "enhanced.hotbar_icons_captured=%u\nenhanced.hotbar_icons_checked=%u\nenhanced.hotbar_icon_wrong=%u\nenhanced.hotbar_outside_gutters=%u\n", captured, checked, wrong, outside);
        fprintf(out, "enhanced.hotbar_fade_checked=%u\nenhanced.hotbar_cutscene_frames=%u\n", oracles_enhanced_view_hotbar_fade_checked(c->view), c->hotbar_cutscene_frames);
        fprintf(out, "enhanced.framed_only_frames=%u\nenhanced.hotbar_framed_only_frames=%u\n", c->framed_only_frames, c->hotbar_framed_only_frames);
        fprintf(out, "enhanced.hotbar_icons_decoded=%u\nenhanced.hotbar_icons_refound=%u\n", oracles_enhanced_view_hotbar_decoded(c->view), oracles_enhanced_view_hotbar_refound(c->view));
        unsigned pictures = 0, pictures_wrong = 0;
        oracles_enhanced_view_hotbar_pictures(c->view, &pictures, &pictures_wrong);
        fprintf(out, "enhanced.hotbar_pictures_checked=%u\nenhanced.hotbar_picture_wrong=%u\n", pictures, pictures_wrong);
    }
    {   /* the status bar drawn full width, when the run asks it */
        unsigned frames = 0, kept = 0;
        oracles_enhanced_view_hud_counts(c->view, &frames, &kept);
        fprintf(out, "enhanced.hud_full_frames=%u\nenhanced.hud_lines_kept=%u\n", frames, kept);
    }
    {   /* the status bar laid out, when the run asks it */
        unsigned laid = 0, centred = 0, one_row = 0;
        oracles_enhanced_view_hud_wide_counts(c->view, &laid, &centred, &one_row);
        fprintf(out, "enhanced.hud_wide_frames=%u\nenhanced.hud_wide_centred=%u\nenhanced.hud_wide_one_row=%u\n", laid, centred, one_row);
    }
    {   /* a room's load drawn under the world's HUD band */
        unsigned band = 0, blank = 0;
        oracles_enhanced_view_load_counts(c->view, &band, &blank);
        fprintf(out, "enhanced.load_band_frames=%u\nenhanced.load_blank_frames=%u\nenhanced.load_band_steps=%u\nenhanced.load_band_changes=%u\nenhanced.load_area_moves=%u\n",
                band, blank, c->load_boundaries, c->load_band_changes, c->load_area_moves);
    }
    fprintf(out, "enhanced.plain_shown_frames=%u\n", oracles_enhanced_view_plain_shown_frames(c->view));
    {
        unsigned drawn = 0, unmatched = 0;
        oracles_enhanced_view_large_object_counts(c->view, &drawn, &unmatched);
        fprintf(out, "enhanced.large_object_sprites_drawn=%u\nenhanced.large_object_frames_unmatched=%u\n", drawn, unmatched);
        fprintf(out, "enhanced.neighbour_objects_outdated=%u\nenhanced.refresh_given_up=%u\nenhanced.parents_run_for_kills=%u\n", oracles_enhanced_view_objects_outdated(c->view), oracles_enhanced_view_refresh_given_up(c->view), oracles_enhanced_view_parents_run_for_kills(c->view));
    }
    {
        unsigned own = 0, other = 0, in_step_tiles = 0;
        oracles_enhanced_view_animation_counts(c->view, &own, &other, &in_step_tiles);
        fprintf(out, "enhanced.neighbour_animation_copies=%u\nenhanced.neighbour_other_animation_copies=%u\nenhanced.neighbour_tiles_in_step_by_image=%u\n", own, other, in_step_tiles);
        unsigned matched = 0, followed = 0;
        oracles_enhanced_view_stream_counts(c->view, &matched, &followed);
        fprintf(out, "enhanced.neighbour_streams_matched=%u\nenhanced.neighbour_streams_followed=%u\n", matched, followed);
        /* A scroll's animation run by the view: where it landed against the game, and its pace. */
        OraclesEnhancedScrollCounts sc;
        oracles_enhanced_view_scroll_animation_counts(c->view, &sc);
        fprintf(out, "enhanced.scroll_animation_scrolls=%u\nenhanced.scroll_animation_scrolls_still=%u\nenhanced.scroll_animation_landed=%u\nenhanced.scroll_animation_landed_off=%u\nenhanced.scroll_animation_gaps=%u\nenhanced.scroll_animation_gap_max=%u\n"
                     "enhanced.scroll_animation_tiles_off=%u\nenhanced.scroll_animation_tiles_given_back=%u\nenhanced.scroll_animation_streams_run=%u\n"
                     "enhanced.scroll_animation_pace_min=%u\nenhanced.scroll_animation_pace_median=%u\nenhanced.scroll_animation_pace_max=%u\nenhanced.scroll_animation_window_pixels=%u\n",
                sc.scrolls, sc.scrolls_still, sc.landed, sc.landed_off, sc.gaps, sc.gap_max, sc.tiles_off, sc.tiles_given_back, sc.streams_run, sc.pace_min, sc.pace_median, sc.pace_max, sc.window_pixels);
    }
    fprintf(out, "enhanced.text_box_frames=%u\nenhanced.text_box_frames_wrong=%u\nenhanced.text_box_pixels_wrong=%u\nenhanced.text_box_frames_at_origin=%u\n",
            c->text_box_frames, c->text_box_frames_wrong, c->text_box_pixels_wrong, c->text_box_frames_at_origin);
    /* What the player sees black, per world frame from the band's first full one, and the cold fill before it (the view's count, the launcher's too). */
    OraclesEnhancedBlack black;
    oracles_enhanced_view_black(c->view, &black);
    fprintf(out, "enhanced.black_pixels_mean=%.0f\nenhanced.black_pixels_max=%u\nenhanced.cold_fill_frames=%d\nenhanced.black_rooms_over=%u\n",
            black.full_world_frames ? (double)black.pixels / black.full_world_frames : 0.0, black.max,
            black.have_full ? (int)(black.first_full - black.first_world) : -1, black.rooms_over);
    /* The same black in episodes, by cause, a scroll's frames included (the session's report says them too). */
    for (unsigned k = 0; k < ORACLES_ENHANCED_BLACK_KINDS; k++) {
        const char *name = oracles_enhanced_black_kind_name(k);
        fprintf(out, "enhanced.black_%s_episodes=%u\nenhanced.black_%s_frames=%u\nenhanced.black_%s_longest=%u\nenhanced.black_%s_in_transition=%u\n",
                name, black.kinds[k].episodes, name, black.kinds[k].frames, name, black.kinds[k].longest, name, black.kinds[k].in_transition);
    }
    /* The cached terrains thrown away because a byte of the cache key they read changed, and the times the coarse list threw the whole cache. */
    fprintf(out, "enhanced.drops=%u\n", oracles_enhanced_view_drop_log(c->view, NULL, 0));
    fprintf(out, "enhanced.sea_runs=%u\n", oracles_enhanced_view_sea_runs(c->view));   /* runs across the sea's surface, ahead of a dive or a return (Ages) */
    fprintf(out, "enhanced.uncovered_frames=%u\nenhanced.uncovered_max=%u\nenhanced.live_diff_max=%u\nenhanced.plain_renders=%u\nenhanced.stale_results=%u\nenhanced.season_rejected=%u\nenhanced.coarse_drops=%u\n",
            c->uncovered_frames, c->uncovered_max, oracles_enhanced_view_live_diff_max(c->view), oracles_enhanced_view_plain_renders(c->view), oracles_enhanced_view_stale_results(c->view), oracles_enhanced_view_season_rejected(c->view),
            oracles_enhanced_view_coarse_drops(c->view));
    fprintf(out, "enhanced.beside_refused=%u\n", oracles_enhanced_view_beside_refused(c->view));
    fprintf(out, "enhanced.render_wait_max=%u\nenhanced.renders_forced=%u\n", oracles_enhanced_view_render_wait_max(c->view), oracles_enhanced_view_renders_forced(c->view));   /* a neighbour's own-animation render: the most frames it waited for the frame's budget, and those made past it */
    {   /* a neighbour's objects animated in place */
        OraclesEnhancedPoseCounts p;
        oracles_enhanced_view_pose_counts(c->view, &p);
        fprintf(out, "enhanced.poses_animated=%u\nenhanced.poses_frozen_code=%u\nenhanced.poses_frozen_enemies=%u\nenhanced.poses_frozen_low=%u\nenhanced.poses_frozen_unplaced=%u\n"
                     "enhanced.poses_frozen_unreadable=%u\nenhanced.pose_steps=%u\nenhanced.pose_changes=%u\nenhanced.pose_changes_drawn=%u\nenhanced.pose_renders=%u\n"
                     "enhanced.pose_lines=%u\nenhanced.pose_resets=%u\nenhanced.poses_drawn_wrong=%u\nenhanced.pose_lines_wrong=%u\nenhanced.pose_lag_max=%u\n"
                     "enhanced.pose_probes=%u\nenhanced.pose_probe_frames=%u\n",
                p.animated, p.frozen_code, p.frozen_enemies, p.frozen_low, p.frozen_unplaced, p.frozen_unreadable, p.steps, p.changes, p.changes_drawn,
                p.renders, p.lines, p.resets, p.drawn_wrong, p.lines_wrong, p.lag_max, p.probes, p.probe_frames);
    }
    if (c->reload_at)
        fprintf(out, "enhanced.reload=%s\nenhanced.reload_camera_delta=%" PRId32 "\nenhanced.reload_camera_frozen=%u\nenhanced.reload_transition_frames=%u\nenhanced.reload_transition_differing=%u\n",
                !c->reload_done ? "not-performed" : c->reload_settled == 1 ? "neighbours-back" : "neighbours-not-back", c->reload_camera_delta,
                c->reload_done && c->reload_link_travel > 16 && c->reload_camera_travel == 0 ? 1u : 0u, c->reload_track_compared, c->reload_track_differing);
}

void oracles_enhanced_check_summary(OraclesEnhancedCheck *c, FILE *out)
{
    summary_of_the_band(c, out);
    summary_of_what_was_shown(c, out);
    if (c->host_box || c->wide || c->text_size) ec_host_text_box_summary(c, out);
}

/* The band itself: the modes, the camera, the ghost's runs, the seams, the
 * scrolls, the window, the text box, the curtain and the coverage. */
static void report_the_band(OraclesEnhancedCheck *c, FILE *out)
{
    fprintf(out, "enhanced: %u frames composed, run hash %016" PRIx64 "\n", c->frames, c->run_hash);
    fprintf(out, "  world %u frames in %u runs, framed fallback %u frames in %u runs, of which the large menus enlarge %u frames in %u runs\n",
            c->world_frames, c->world_runs, c->framed_frames, c->fallback_runs, c->enlarged_frames, c->enlarged_runs);
    fprintf(out, "  framed frames at the view's scale right after an enlarged one: %u", c->enlarged_framed_drops);
    for (unsigned i = 0; i < c->enlarged_framed_drops && i < 8u; i++) fprintf(out, "%s%" PRIu32, i ? ", " : " (at ", c->enlarged_framed_drop_at[i]);
    fprintf(out, "%s\n", c->enlarged_framed_drops ? ")" : "");
    fprintf(out, "  world frames showing the normal band centred in a wider one (an interior in the drawn-back view): %u\n", c->normal_band_frames);
    fprintf(out, "  camera jumps over %d px between consecutive world frames: %u (largest %" PRId32 " px); vertical: %u (largest %" PRId32 " px)\n",
            MAX_CAMERA_STEP, c->camera_jumps, c->largest_jump, c->camera_jumps_y, c->largest_jump_y);
    fprintf(out, "  camera profile %u, motion over world frames outside cutscenes: steps of 0 px %u, 1 px %u, 2 px %u, 3 px %u, 4 px and more %u; still while Link moves (not at the row's edge) %u frames; step changes %u; Link's offset from the centre %.1f px on average, %" PRId32 " px at most\n",
            c->camera_profile ? c->camera_profile : 1u, c->camera_steps[0], c->camera_steps[1], c->camera_steps[2], c->camera_steps[3], c->camera_steps[4],
            c->camera_still_link_moving, c->camera_step_changes,
            c->offset_frames ? (double)c->offset_sum / (double)c->offset_frames : 0.0, c->offset_max);
    fprintf(out, "  room the camera left Link to the band's edge, outside transitions: %" PRId32 " px at the least, %u frames under 8 px\n",
            c->link_edge_margin_min, c->link_near_edge_frames);
    fprintf(out, "  seams: Link's world x jumps over 8 px within an epoch: %u (largest %" PRId32 "), his world y: %u (largest %" PRId32 "); the game window's: %u (largest %" PRId32 "); last epoch id %" PRIu64 "\n",
            c->link_jumps, c->largest_link_jump, c->link_jumps_y, c->largest_link_jump_y, c->window_jumps, c->largest_window_jump, c->epochs);
    fprintf(out, "  Link's sprite as drawn, on the screen in normal play: %u steps between frames, %u back against the step before across and %u down, he not turning%s%s\n",
            c->link_drawn_steps, c->link_backsteps, c->link_backsteps_y, c->link_backsteps_list[0] ? "; first: " : "", c->link_backsteps_list);
    {
        unsigned moved = 0, engaged = 0;
        oracles_enhanced_view_link_phase_counts(c->view, &moved, &engaged);
        fprintf(out, "  walking diagonally: %u frames, %u where the camera stepped back across the walk; Link drawn in phase on %u frames, off the game's sprite on %u\n",
                c->diagonal_frames, c->camera_wobble, engaged, moved);
        unsigned sub = 0, layered = 0, refused = 0;
        oracles_enhanced_view_subpixel_counts(c->view, &sub, &layered, &refused);
        fprintf(out, "  as shown at 4x: the band stopping between two steps %u times an axis in those frames (%u in %u walking straight); Link moving on the screen on %u, back against his step before %u times; "
                     "sub-pixel camera on %u frames, Link drawn apart on %u, refused on %u\n",
                c->band_stops, c->straight_band_stops, c->straight_frames, c->link_screen_moves, c->link_screen_backsteps, sub, layered, refused);
        fprintf(out, "  as shown at 4x, a step changing from one frame to the next: at the %u switches (a transition, a text box, Link drawn apart) by %" PRId32 " px at most for the band, %" PRId32 " for Link (%s); elsewhere by %" PRId32 " and %" PRId32 "\n",
                c->switch_edges, c->switch_band_jolt_max, c->switch_link_jolt_max, c->jolt_list[0] ? c->jolt_list : "none of 2 px", c->band_jolt_max, c->link_jolt_max);
        fprintf(out, "  as shown at 4x, Link drawn apart or no longer in a transition, the band at a fraction: %u times, he moving against the ground by %" PRId32 " px at most\n",
                c->transition_layer_switches, c->transition_layer_shift_max);
        fprintf(out, "  as shown at 4x, Link's sprites stepping 2 px or more one way then back at the next frame, he not turning: %u times%s%s\n",
                c->link_drawn_zigzags, c->zig_list[0] ? "; first: " : "", c->zig_list);
    }
    unsigned requested = 0, completed = 0, failed = 0;
    oracles_enhanced_view_ghost_counts(c->view, &requested, &completed, &failed);
    unsigned reasons[5] = { 0, 0, 0, 0, 0 };
    oracles_enhanced_view_ghost_failures(c->view, reasons);
    fprintf(out, "  neighbours: world frames with four %u, three %u, two %u, one %u, none %u; ghost runs %u requested, %u completed, %u failed (not primeable %u, load %u, never settled %u, wrong room %u, other %u)\n",
            c->neighbours_frames[4], c->neighbours_frames[3], c->neighbours_frames[2], c->neighbours_frames[1], c->neighbours_frames[0], requested, completed, failed,
            reasons[0], reasons[1], reasons[2], reasons[3], reasons[4]);
    if (reasons[0] || reasons[2] || reasons[3]) fprintf(out, "  failed runs (from>expected=got, from>room?: never settled, from>room!reason: not primeable): %s\n", oracles_enhanced_view_failure_log(c->view));
    fprintf(out, "  scrolling transitions %u: %u frames in all (%u at most), Link's position unchanged on %u of them; arrivals on a tile with a collision: %u; room:frames/frozen@x,y: %s\n",
            c->transitions_seen, c->transition_frames_total, c->transition_frames_max, c->transition_frozen_total, c->transition_arrivals_solid, c->transition_list);
    fprintf(out, "  window against the drawn scroll registers: %u world frames checked, misplaced %u%s%s; frames framed because the game drew its area elsewhere than its camera: %u (in a cutscene shown in the band all the same: %u); frames shown shaken with the game's screen shake: %u\n",
            c->window_checked, c->window_misplaced, c->window_misplaced ? ": frame:dx,dy " : "", c->window_misplaced_list, oracles_enhanced_view_off_camera_frames(c->view), oracles_enhanced_view_world_off_camera_frames(c->view), oracles_enhanced_view_shake_frames(c->view));
    fprintf(out, "  text box in the middle of the band: %u world frames, %u with the wrong pixels (%u pixels), %u showing it where the game drew it%s%s\n",
            c->text_box_frames, c->text_box_frames_wrong, c->text_box_pixels_wrong, c->text_box_frames_at_origin, c->text_box_frames_wrong ? ": frame:pixels " : "", c->text_box_list);
    fprintf(out, "  curtain of a warp over the band: %u world frames, %u with the wrong pixels (%u pixels), %u with the window fully drawn (colour unchecked)%s%s\n",
            c->curtain_frames, c->curtain_frames_wrong, c->curtain_pixels_wrong, c->curtain_frames_unchecked, c->curtain_frames_wrong ? ": frame:pixels " : "", c->curtain_list);
    fprintf(out, "  area blurb taken out of the window: %u world frames showing it, %u left in the window%s%s\n",
            c->blurb_frames, c->blurb_frames_in_window, c->blurb_frames_in_window ? ": frames " : "", c->blurb_list);
    fprintf(out, "  coverage: world frames with pixels no source covered %u (%" PRIu64 " pixels, %u at most in a frame)%s%s\n",
            c->uncovered_frames, c->uncovered_pixels, c->uncovered_max, c->uncovered_frames ? ": frame:pixels " : "", c->uncovered_list);
    OraclesEnhancedBlack black;
    oracles_enhanced_view_black(c->view, &black);
    fprintf(out, "  black from the band's first full frame (%u, %u frames after the first world frame): %.0f pixels a world frame on average, %u at most; rooms of the overworld's grid black whole (in the band by a pixel at least, nothing drawn there) for more than %u world frames in a row: %u",
            black.first_full, black.have_full ? black.first_full - black.first_world : 0u,
            black.full_world_frames ? (double)black.pixels / black.full_world_frames : 0.0, black.max, ORACLES_ENHANCED_BLACK_ROOM_FRAMES, black.rooms_over);
    for (unsigned i = 0; i < black.rooms_over && i < ORACLES_ENHANCED_BLACK_ROOMS_KEPT; i++)
        fprintf(out, "%s%u:%02x from %u, %u frames", i ? ", " : " (", black.longest[i].group, black.longest[i].room, black.longest[i].from, black.longest[i].frames);
    fprintf(out, "%s\n", black.rooms_over ? ")" : "");
    fprintf(out, "  black episodes by cause (episodes, world frames with one, longest, begun in a transition):");
    for (unsigned k = 0; k < ORACLES_ENHANCED_BLACK_KINDS; k++)
        fprintf(out, "%s %s %u, %u frames, %u, %u", k ? ";" : "", oracles_enhanced_black_kind_name(k), black.kinds[k].episodes, black.kinds[k].frames, black.kinds[k].longest, black.kinds[k].in_transition);
    const char *log = oracles_enhanced_view_black_log(c->view);
    fprintf(out, "%s%s\n", log[0] ? "; corners (c) and other rooms (n) outside the fill, from+frames: " : "", log);
}

/* What the band showed: the rooms entered, their terrain against the game's,
 * the runs chained ahead and the savestate reload. */
static void report_what_was_shown(OraclesEnhancedCheck *c, FILE *out)
{
    fprintf(out, "  scrolls entered walking diagonally: %u; their frames with Link standing across the axis two frames running: %u\n", c->scrolls_diagonal, c->scroll_across_stalled);
    fprintf(out, "  horizontal transitions in large rooms (the room entered shown during the scroll): %u; frames of a large room's scroll with the room entered missing: %u\n", c->transitions_large, c->large_scroll_black);
    fprintf(out, "  scrolling transitions of the grid %u (%u vertical): the room entered was shown as a neighbour before %u times, black %u times%s%s, kept dark as never visited %u times, another room shown %u times%s%s\n",
            c->transitions, c->transitions_vertical, c->transitions_with_terrain, c->transitions_black, c->transitions_black ? ": " : "", c->black_list, c->transitions_dark,
            c->transitions_other_room, c->transitions_other_room ? ": " : "", c->other_room_list);
    fprintf(out, "  rooms entered while shown in their old state (the ghost running them again, not judged): %u\n", c->terrains_old);
    fprintf(out, "  shown terrain against the layout committed after the entry: equal %u, different %u (%u tiles)%s%s; stale ghost results dropped %u; ghost %s\n",
            c->terrains_equal, c->terrains_different, c->terrain_tiles_different, c->terrains_different ? ": " : "", c->different_list,
            oracles_enhanced_view_stale_results(c->view), c->threaded ? "in its thread, as played" : "synchronous");
    char drops[512];
    const unsigned dropped = oracles_enhanced_view_drop_log(c->view, drops, sizeof drops);
    unsigned blind_filed = 0, blind_dropped = 0;
    oracles_enhanced_view_blind_runs(c->view, &blind_filed, &blind_dropped);
    fprintf(out, "  neighbours computed ahead from another neighbour's settled state: %u, sources run again after a key change: %u; pre-runs while a room loaded: %u filed, %u dropped; renders of a neighbour with the live tiles (animated in step): %u, %u more kept, what they read unchanged, a render put off by the frame's budget for %u frames at most (%u made past it), and %u lines of those made kept, their tiles unchanged (differing from the ghost's render outside the animated tiles by at most %u blocks of 8x8); captures of no terrain refused (a fade caught, the LCD off): %u%s%s; terrains dropped by a key byte they read: %u%s%s\n",
            oracles_enhanced_view_chained_results(c->view), oracles_enhanced_view_refreshed_parents(c->view), blind_filed, blind_dropped,
            oracles_enhanced_view_live_renders(c->view), oracles_enhanced_view_live_renders_kept(c->view), oracles_enhanced_view_render_wait_max(c->view), oracles_enhanced_view_renders_forced(c->view), oracles_enhanced_view_live_lines_kept(c->view), oracles_enhanced_view_live_diff_max(c->view), oracles_enhanced_view_plain_renders(c->view), oracles_enhanced_view_plain_renders(c->view) ? " " : "", oracles_enhanced_view_plain_log(c->view),
            dropped, dropped ? " " : "", drops);
    if (c->reload_at) {
        if (c->reload_track_compared)
            fprintf(out, "  reload in a transition: Link's place on the %u frames after the load to its end, against the run before the load: %u different\n",
                    c->reload_track_compared, c->reload_track_differing);
        if (!c->reload_done) fprintf(out, "  reload at frame %u: not performed (the route is shorter, or the state could not be saved)\n", c->reload_at);
        else if (!c->reload_saved_in_scroll)
            fprintf(out, "  reload at frame %u, loaded back at %u: camera %" PRId32 " px from the saved point on the next frame; neighbours %s after %u frames (saved %s%02x %s%02x); over the 300 frames after: Link moved %" PRId32 " px, the camera %" PRId32 " px%s\n",
                    c->reload_at, c->reload_frame, c->reload_camera_delta,
                    c->reload_settled == 1 ? "back to the saved ones" : "NOT back", c->reload_settled_after,
                    c->saved_shown[3] ? "+" : "-", c->saved_room[3], c->saved_shown[1] ? "+" : "-", c->saved_room[1],
                    c->reload_link_travel, c->reload_camera_travel,
                    c->reload_link_travel > 16 && c->reload_camera_travel == 0 ? " (FROZEN camera)" : "");
        else if (!c->reload_expected_valid)
            fprintf(out, "  reload at frame %u, loaded back at %u: camera %" PRId32 " px from the saved point on the next frame; neighbours NOT back (the natural arrival topology was not stable on the saved destination before load); over the 300 frames after: Link moved %" PRId32 " px, the camera %" PRId32 " px%s\n",
                    c->reload_at, c->reload_frame, c->reload_camera_delta,
                    c->reload_link_travel, c->reload_camera_travel,
                    c->reload_link_travel > 16 && c->reload_camera_travel == 0 ? " (FROZEN camera)" : "");
        else
            fprintf(out, "  reload at frame %u, loaded back at %u: camera %" PRId32 " px from the saved point on the next frame; neighbours %s after %u frames (natural arrival %s%02x %s%02x); over the 300 frames after: Link moved %" PRId32 " px, the camera %" PRId32 " px%s\n",
                    c->reload_at, c->reload_frame, c->reload_camera_delta,
                    c->reload_settled == 1 ? "back to the natural arrival" : "NOT back", c->reload_settled_after,
                    c->reload_expected_shown[3] ? "+" : "-", c->reload_expected_room[3], c->reload_expected_shown[1] ? "+" : "-", c->reload_expected_room[1],
                    c->reload_link_travel, c->reload_camera_travel,
                    c->reload_link_travel > 16 && c->reload_camera_travel == 0 ? " (FROZEN camera)" : "");
    }
}

void oracles_enhanced_check_report(OraclesEnhancedCheck *c, FILE *out)
{
    report_the_band(c, out);
    report_what_was_shown(c, out);
    if (c->host_box || c->wide || c->text_size) ec_host_text_box_report(c, out);
}
