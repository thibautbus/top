#include "object_animation_check_internal.h"

static const char *const cause_names[CAUSES] = { "not_animated", "several_calls", "pose_advanced", "animation_set", "pose_written", "other" };
static const char *const kind_names[KINDS] = { "interactions", "enemies", "parts" };
static const char *const place_names[PLACES] = { "", "_moving" };

OraclesObjectAnimationCheck *oracles_object_animation_check_start(OraclesGuest *guest, const uint8_t *rom, size_t rom_size, const char *dir)
{
    OraclesObjectAnimationCheck *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->guest = guest;
    c->rom = malloc(rom_size);
    if (!c->rom) { free(c); return NULL; }
    memcpy(c->rom, rom, rom_size);
    c->rom_size = rom_size;
    if (dir) {
        char path[4096];
        snprintf(path, sizeof path, "%s/object_animation.tsv", dir);
        c->tsv = fopen(path, "w");
        if (c->tsv) fprintf(c->tsv, "frame\troom\tkind\tid\tsubid\tnumber\tposes\tperiod\tchanges\tpaired\tposes_equal\tposes_different\tposes_unseen\tanimated\n");
        snprintf(path, sizeof path, "%s/object_animation_misses.tsv", dir);
        c->misses = fopen(path, "w");
        if (c->misses) fprintf(c->misses, "frame\troom\tkind\tid\tsubid\tslot\tmoved\tcause\tlast\tstep\tgame\n");
    }
    return c;
}

void oracles_object_animation_check_stop(OraclesObjectAnimationCheck *c)
{
    if (!c) return;
    oac_end_tracks(c);
    oac_finish_capture(c);
    if (c->tsv) fclose(c->tsv);
    if (c->misses) fclose(c->misses);
    free(c->rom);
    free(c);
}

int oac_same_state(const OraclesObjectAnimation *a, const OraclesObjectAnimation *b)
{
    return a->counter == b->counter && a->pointer == b->pointer && a->oam_data_address == b->oam_data_address;
}

int oac_shown(const OraclesGuestTables *t, const OraclesObjectRecord *r)
{
    if (r->kind < 1u || r->kind > KINDS || !r->number || r->randomly_placed || !(r->visible & 0x80u)) return 0;
    if (r->kind == ORACLES_OBJECT_KIND_INTERACTION && r->id == INTERAC_ERA_OR_SEASON_INFO) return 0;
    return oracles_object_animation_resolved(t, r->kind);
}

/* A followed object's figures, added to those of its kind and place once it is no longer followed. */
static void end_track(OraclesObjectAnimationCheck *c, unsigned kind, track *tr)
{
    if (tr->active && tr->rule == RULE_UNCAPTURED && tr->first_judged == ORACLES_GHOST_PROBE_UPDATES && !tr->first_near) {
        tr->rule = (kind == ORACLES_OBJECT_KIND_INTERACTION - 1u || kind == ORACLES_OBJECT_KIND_PART - 1u) && tr->first_one_step == ORACLES_GHOST_PROBE_UPDATES
                 ? RULE_ANIMATED : RULE_FROZEN;
        c->decided_live++;
    }
    if (tr->active) {
        tally *to = tr->rule == RULE_FROZEN ? &c->frozen : tr->rule == RULE_UNCAPTURED ? &c->uncaptured : &c->totals[kind][tr->moved ? PLACE_MOVING : PLACE_FIXED];
        const tally *from = &tr->t;
        to->objects += from->objects;
        to->multi_pose += from->multi_pose;
        to->changed_live += from->changed_live;
        to->judged += from->judged;
        to->step_equal += from->step_equal;
        to->predicted += from->predicted;
        to->matched += from->matched;
        to->live += from->live;
        to->predicted_far += from->predicted_far;
        to->matched_far += from->matched_far;
        for (unsigned x = 0; x < CAUSES; x++) {
            to->causes[x] += from->causes[x];
            to->predicted_by[x] += from->predicted_by[x];
            to->live_by[x] += from->live_by[x];
        }
    }
    memset(tr, 0, sizeof *tr);
}

void oac_end_tracks(OraclesObjectAnimationCheck *c)
{
    for (unsigned k = 0; k < KINDS; k++) for (unsigned s = 0; s < SLOTS; s++) end_track(c, k, &c->tracks[k][s]);
}


/* `once`: one call from `last`, `read` whether it read an entry. */
static unsigned cause_of(OraclesObjectAnimationCheck *c, const OraclesObjectAnimation *last, const OraclesObjectAnimation *now, const OraclesObjectAnimation *once, int read)
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    if (oac_same_state(last, now)) return CAUSE_NOT_ANIMATED;
    /* The calls that follow, up to the next entry read. */
    OraclesObjectAnimation s = *once;
    for (unsigned calls = 2; !read && calls <= 256u; calls++) {
        const int rc = oracles_object_animation_step(&s, t, c->rom, c->rom_size);
        if (rc < 0) break;
        read = rc == 1;
        if (!oac_same_state(&s, now)) continue;
        if (read) return CAUSE_POSE_ADVANCED;
        return calls <= SEVERAL_CALLS_MAX ? CAUSE_SEVERAL_CALLS : CAUSE_OTHER;
    }
    if (oracles_object_animation_is_set(now, now->pointer, t, c->rom, c->rom_size)) return CAUSE_ANIMATION_SET;
    if ((now->pointer == last->pointer && now->counter == last->counter) || (now->pointer == once->pointer && now->counter == once->counter)) return CAUSE_POSE_WRITTEN;
    return CAUSE_OTHER;
}

/* A captured object's first updates after the scroll, before Link comes near it: animated by its code as the
 * capture's probe said, or not. */
static void against_probe(OraclesObjectAnimationCheck *c, const track *tr)
{
    if (!tr->captured || tr->link_near) return;
    const captured_object *o = &c->captured[tr->captured - 1u];
    if (o->pose_count < 2u || (o->record.kind != ORACLES_OBJECT_KIND_INTERACTION && o->record.kind != ORACLES_OBJECT_KIND_PART)) return;
    const int live = tr->first_one_step == ORACLES_GHOST_PROBE_UPDATES, probe = tr->rule == RULE_ANIMATED;
    if (live == probe) c->probe_agree++;
    else if (probe) c->probe_animated_live_not++;
    else c->probe_frozen_live_animated++;
}

/* Whether Link stands within LINK_NEAR of the object on both axes (objectCheckLinkWithinDistance). */
static int link_near(OraclesObjectAnimationCheck *c, const OraclesObjectRecord *r)
{
    const uint8_t *link = oracles_guest_object(c->guest, 0, 0);
    if (!link) return 0;
    const int dy = (int)link[ORACLES_OBJ_YH] - (int)r->yh, dx = (int)link[ORACLES_OBJ_XH] - (int)r->xh;
    return (dy < 0 ? -dy : dy) < (int)LINK_NEAR && (dx < 0 ? -dx : dx) < (int)LINK_NEAR;
}

static void judge(OraclesObjectAnimationCheck *c, track *tr, const OraclesObjectRecord *r, const OraclesObjectAnimation *now, uint32_t frame)
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    OraclesObjectAnimation once = tr->last;
    const int read = oracles_object_animation_step(&once, t, c->rom, c->rom_size);
    if (read < 0) { c->read_failures++; tr->followed = 0; return; }
    tally *y = &tr->t;
    y->judged++;
    const int predicted = once.oam_data_address != tr->last.oam_data_address, changed = now->oam_data_address != tr->last.oam_data_address;
    if (predicted) y->predicted++;
    if (predicted && !tr->link_near) y->predicted_far++;
    if (changed) y->live++;
    if (changed && !y->changed_live) y->changed_live = 1;
    const int one_step = oac_same_state(&once, now);
    if (tr->first_judged < ORACLES_GHOST_PROBE_UPDATES) {
        tr->first_judged++;
        tr->first_one_step += (unsigned)one_step;
        if (tr->first_judged == ORACLES_GHOST_PROBE_UPDATES) { tr->first_near = tr->link_near; against_probe(c, tr); }
    }
    if (one_step) {
        y->step_equal++;
        if (predicted) y->matched++;
        if (predicted && !tr->link_near) y->matched_far++;
        return;
    }
    const unsigned cause = cause_of(c, &tr->last, now, &once, read == 1);
    y->causes[cause]++;
    if (predicted) y->predicted_by[cause]++;
    if (changed) y->live_by[cause]++;
    if (!predicted && !changed) return;
    if (c->by_id[r->kind - 1u][cause][r->id] < 0xffffu) c->by_id[r->kind - 1u][cause][r->id]++;
    if (c->misses)
        fprintf(c->misses, "%u\t%u:%02x\t%u\t%02x\t%02x\t%u\t%d\t%s\t%02x,%04x,%04x\t%02x,%04x,%04x\t%02x,%04x,%04x\n", frame, c->group, c->room, r->kind, r->id,
                r->subid, r->slot, tr->moved, cause_names[cause], tr->last.counter, tr->last.pointer, tr->last.oam_data_address,
                once.counter, once.pointer, once.oam_data_address, now->counter, now->pointer, now->oam_data_address);
}


void oracles_object_animation_check_updated(OraclesObjectAnimationCheck *c, const OraclesObjectRecord *now, unsigned count, uint32_t frame)
{
    if (!c) return;
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    const uint8_t group = oracles_guest_read8(c->guest, t->active_group), room = oracles_guest_read8(c->guest, t->active_room);
    if (group != c->group || room != c->room) {
        if (c->have_capture && (group != c->capture_group || room != c->capture_room)) oac_finish_capture(c);
        oac_end_tracks(c);
        c->group = group;
        c->room = room;
        c->updates_in_room = 0;
    }
    /* Normal play: the objects run their code, nothing freezes them. */
    const int play = (oracles_guest_read8(c->guest, t->scroll_mode) & 0x7fu) == SCROLL_MODE_NORMAL
        && oracles_guest_read8(c->guest, t->screen_transition_state) == TRANSITION_STATE_IDLE
        && !oracles_guest_read8(c->guest, t->text_is_active) && !oracles_guest_read8(c->guest, t->opened_menu_type)
        && !oracles_guest_read8(c->guest, t->disabled_objects) && !oracles_guest_read8(c->guest, t->palette_thread_mode);
    if (!play) {
        c->updates_skipped++;
        for (unsigned k = 0; k < KINDS; k++) for (unsigned s = 0; s < SLOTS; s++) c->tracks[k][s].followed = 0;
        return;
    }
    c->updates_judged++;
    c->updates_in_room++;
    uint8_t seen[KINDS][SLOTS] = { { 0 } };
    for (unsigned i = 0; i < count; i++) {
        const OraclesObjectRecord *r = &now[i];
        if (!oac_shown(t, r) || r->slot >= SLOTS) continue;
        const unsigned kind = r->kind - 1u;
        track *tr = &c->tracks[kind][r->slot];
        seen[kind][r->slot] = 1;
        OraclesObjectAnimation a;
        oracles_object_animation_from_record(&a, r);
        const int same_object = tr->active && tr->number == r->number && tr->id == r->id && tr->created_id == r->created_id
            && tr->created_subid == r->created_subid && tr->created_var03 == r->created_var03;
        if (!same_object) {
            /* A new object in the slot: counted once, its loop read from the state it is first seen in. */
            end_track(c, kind, tr);
            tr->active = 1;
            tr->number = r->number;
            tr->id = r->id;
            tr->created_id = r->created_id;
            tr->created_subid = r->created_subid;
            tr->created_var03 = r->created_var03;
            tr->yh = r->yh;
            tr->xh = r->xh;
            tr->captured = oac_find_captured(c, r);
            tr->rule = !tr->captured ? RULE_UNCAPTURED
                     : oracles_object_animation_animated(t, &c->captured[tr->captured - 1u].record) ? RULE_ANIMATED : RULE_FROZEN;
            tr->t.objects = 1;
            uint16_t poses[POSES];
            tr->t.multi_pose = oracles_object_animation_poses(&a, t, c->rom, c->rom_size, poses, POSES, NULL, NULL) > 1u;
        } else if (tr->followed) {
            if (link_near(c, r)) tr->link_near = 1;
            judge(c, tr, r, &a, frame);
        }
        if (r->yh != tr->yh || r->xh != tr->xh) tr->moved = 1;
        oac_against_capture(c, tr, r, &a);
        tr->last = a;
        tr->followed = 1;
    }
    for (unsigned k = 0; k < KINDS; k++) for (unsigned s = 0; s < SLOTS; s++) if (!seen[k][s]) c->tracks[k][s].followed = 0;
}

static double percent(unsigned part, unsigned whole) { return whole ? 100.0 * (double)part / (double)whole : 100.0; }

/* The pose changes the game makes by its data, its code's own choices aside (an animation chosen, a pose written). */
static unsigned data_changes(const tally *y)
{
    return y->live - y->live_by[CAUSE_ANIMATION_SET] - y->live_by[CAUSE_POSE_WRITTEN];
}

/* The step's pose changes the game does not make, while Link has not yet come near the object: what a neighbour,
 * Link away, would show wrong.  A change its code makes then, an animation it sets or resets included, counts. */
static unsigned wrong_changes(const tally *y)
{
    return y->predicted_far - y->matched_far;
}

void oracles_object_animation_check_summary(OraclesObjectAnimationCheck *c, FILE *out)
{
    if (!c || !out) return;
    oac_end_tracks(c);
    oac_finish_capture(c);
    fprintf(out, "object_animation.updates_judged=%u\nobject_animation.updates_skipped=%u\nobject_animation.read_failures=%u\n",
            c->updates_judged, c->updates_skipped, c->read_failures);
    for (unsigned k = 0; k < KINDS; k++)
        for (unsigned p = 0; p < PLACES; p++) {
            const tally *y = &c->totals[k][p];
            const char *n = kind_names[k], *q = place_names[p];
            fprintf(out, "object_animation.%s%s.objects=%u\nobject_animation.%s%s.multi_pose=%u\nobject_animation.%s%s.changed_live=%u\n",
                    n, q, y->objects, n, q, y->multi_pose, n, q, y->changed_live);
            fprintf(out, "object_animation.%s%s.judged=%u\nobject_animation.%s%s.step_equal=%u\n", n, q, y->judged, n, q, y->step_equal);
            fprintf(out, "object_animation.%s%s.pose_predicted=%u\nobject_animation.%s%s.pose_matched=%u\nobject_animation.%s%s.pose_live=%u\nobject_animation.%s%s.pose_live_by_data=%u\n",
                    n, q, y->predicted, n, q, y->matched, n, q, y->live, n, q, data_changes(y));
            /* The share of the predicted changes the game makes on the same update, and of the game's own by its data the step makes. */
            fprintf(out, "object_animation.%s%s.pose_wrong=%u\n", n, q, wrong_changes(y));
            fprintf(out, "object_animation.%s%s.predicted_matched_permille=%u\nobject_animation.%s%s.data_matched_permille=%u\n",
                    n, q, y->predicted ? (unsigned)(1000.0 * y->matched / y->predicted) : 1000u, n, q, data_changes(y) ? (unsigned)(1000.0 * y->matched / data_changes(y)) : 1000u);
            for (unsigned x = 0; x < CAUSES; x++)
                fprintf(out, "object_animation.%s%s.%s=%u\nobject_animation.%s%s.%s_predicted=%u\nobject_animation.%s%s.%s_live=%u\n", n, q, cause_names[x], y->causes[x],
                        n, q, cause_names[x], y->predicted_by[x], n, q, cause_names[x], y->live_by[x]);
        }
    const tally *apart[2] = { &c->frozen, &c->uncaptured };
    const char *const apart_names[2] = { "frozen", "uncaptured" };
    for (unsigned n = 0; n < 2u; n++)
        fprintf(out, "object_animation.%s.objects=%u\nobject_animation.%s.multi_pose=%u\nobject_animation.%s.pose_predicted=%u\nobject_animation.%s.pose_matched=%u\n"
                     "object_animation.%s.pose_live=%u\nobject_animation.%s.pose_live_by_data=%u\nobject_animation.%s.pose_wrong=%u\n",
                apart_names[n], apart[n]->objects, apart_names[n], apart[n]->multi_pose, apart_names[n], apart[n]->predicted, apart_names[n], apart[n]->matched,
                apart_names[n], apart[n]->live, apart_names[n], data_changes(apart[n]), apart_names[n], wrong_changes(apart[n]));
    fprintf(out, "object_animation.decided_live=%u\n", c->decided_live);
    fprintf(out, "object_animation.probe_agree=%u\nobject_animation.probe_animated_live_not=%u\nobject_animation.probe_frozen_live_animated=%u\n",
            c->probe_agree, c->probe_animated_live_not, c->probe_frozen_live_animated);
    fprintf(out, "object_animation.captures=%u\nobject_animation.captured_unreadable=%u\nobject_animation.captured_unpaired=%u\n",
            c->captures, c->captured_unreadable, c->captured_unpaired);
    for (unsigned k = 0; k < KINDS; k++)
        fprintf(out, "object_animation.captured_%s=%u\nobject_animation.captured_%s_multi_pose=%u\nobject_animation.captured_%s_animated=%u\n", kind_names[k],
                c->captured_shown[k], kind_names[k], c->captured_multi_pose[k], kind_names[k], c->captured_animated[k]);
    fprintf(out, "object_animation.poses_checked=%u\nobject_animation.poses_tiles_equal=%u\nobject_animation.tiles_wrong=%u\nobject_animation.poses_unseen=%u\n",
            c->poses_checked, c->poses_equal, c->poses_different, c->poses_unseen);
    fprintf(out, "object_animation.entry_equal=%u\nobject_animation.entry_different=%u\n", c->entry_equal, c->entry_different);
}

void oracles_object_animation_check_report(OraclesObjectAnimationCheck *c, FILE *out)
{
    if (!c || !out) return;
    oac_end_tracks(c);
    oac_finish_capture(c);
    fprintf(out, "  objects' animation from the game's data against the live game: %u updates of play judged (%u skipped), unreadable %u\n",
            c->updates_judged, c->updates_skipped, c->read_failures);
    for (unsigned k = 0; k < KINDS; k++)
        for (unsigned p = 0; p < PLACES; p++) {
            const tally *y = &c->totals[k][p];
            if (!y->objects) continue;
            fprintf(out, "    %s%s, animated by the rule: %u objects (%u with a loop of several poses, %u changed pose live), %u updates, the step equal to the game's on %u (%.2f %%);"
                         " pose changes: %u predicted, %u live (%u by the data), %u matched (%.2f %% of the predicted, %.2f %% of the live by the data); otherwise:",
                    kind_names[k], p == PLACE_MOVING ? " that moved" : " at their place", y->objects, y->multi_pose, y->changed_live, y->judged, y->step_equal,
                    percent(y->step_equal, y->judged), y->predicted, y->live, data_changes(y), y->matched, percent(y->matched, y->predicted), percent(y->matched, data_changes(y)));
            for (unsigned x = 0; x < CAUSES; x++) fprintf(out, " %s %u (pose changes: predicted %u, live %u)", cause_names[x], y->causes[x], y->predicted_by[x], y->live_by[x]);
            fprintf(out, "\n");
        }
    fprintf(out, "    pose changes wrong while Link was away, animated by the rule:");
    for (unsigned k = 0; k < KINDS; k++) for (unsigned p = 0; p < PLACES; p++) fprintf(out, " %s%s %u", kind_names[k], place_names[p], wrong_changes(&c->totals[k][p]));
    fprintf(out, "\n");
    for (unsigned n = 0; n < 2u; n++) {
        const tally *y = n ? &c->uncaptured : &c->frozen;
        fprintf(out, "    %s: %u objects (%u of several poses), pose changes: %u predicted, %u live (%u by the data), %u matched, %u wrong while Link was away\n",
                n ? "undecided (no capture, Link near or too few updates)" : "kept still by the rule", y->objects, y->multi_pose, y->predicted, y->live, data_changes(y), y->matched, wrong_changes(y));
    }
    fprintf(out, "    the rule judged on the game for %u objects no capture paired; the captures' probe against the game's first updates: %u the same,"
                 " %u animated by the probe alone, %u by the game alone; kept still by the probe, of several poses: %s\n",
            c->decided_live, c->probe_agree, c->probe_animated_live_not, c->probe_frozen_live_animated, c->probe_frozen_list[0] ? c->probe_frozen_list : "none");
    for (unsigned k = 0; k < KINDS; k++)
        for (unsigned x = 0; x < CAUSES; x++) {
            uint16_t by_id[256];
            memcpy(by_id, c->by_id[k][x], sizeof by_id);
            unsigned any = 0;
            for (unsigned id = 0; id < 256u; id++) any += by_id[id];
            if (!any) continue;
            fprintf(out, "      %s, pose changes missed, %s, by id:", kind_names[k], cause_names[x]);
            for (unsigned round = 0; round < 8u; round++) {
                unsigned best = 0, best_id = 0;
                for (unsigned id = 0; id < 256u; id++) if (by_id[id] > best) { best = by_id[id]; best_id = id; }
                if (!best) break;
                fprintf(out, " %02x x%u", best_id, best);
                by_id[best_id] = 0;
            }
            fprintf(out, "\n");
        }
    fprintf(out, "    captures: %u rooms; shown %u interactions (%u of several poses, %u animated), %u enemies (%u), %u parts (%u, %u animated); unreadable %u, never live %u\n",
            c->captures, c->captured_shown[0], c->captured_multi_pose[0], c->captured_animated[0], c->captured_shown[1], c->captured_multi_pose[1], c->captured_shown[2],
            c->captured_multi_pose[2], c->captured_animated[2], c->captured_unreadable, c->captured_unpaired);
    fprintf(out, "    tiles of the captured poses against the live VRAM: %u poses shown live, %u equal, %u different%s%s%s; %u never shown live\n",
            c->poses_checked, c->poses_equal, c->poses_different, c->tiles_wrong_list[0] ? " (room kind.id:subid@pose: " : "", c->tiles_wrong_list,
            c->tiles_wrong_list[0] ? ")" : "", c->poses_unseen);
    fprintf(out, "    the capture's state against the game's first update after the scroll: %u equal, %u different\n", c->entry_equal, c->entry_different);
    fprintf(out, "    cost: the step %.4f us a call (%u timed); a pose's lines redrawn %.2f us (%.1f lines, %u timed); the game area's 128 lines %.2f us (%u timed);"
                 " the room of most pose changes %u:%02x, %.1f a second\n",
            c->steps_timed ? c->step_us / c->steps_timed : 0.0, c->steps_timed, c->redraws_timed ? c->redraw_us / c->redraws_timed : 0.0,
            c->redraws_timed ? c->redraw_lines / c->redraws_timed : 0.0, c->redraws_timed, c->fulls_timed ? c->full_us / c->fulls_timed : 0.0, c->fulls_timed,
            c->rate_max_group, c->rate_max_room, c->rate_max);
}
