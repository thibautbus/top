/* A neighbour's objects animated in place.  A capture shows each object in
 * the pose the game creates it in; the interactions and the parts whose
 * animation loops over several poses go on through that loop, a step at each
 * update of the live game's objects in normal play, the cadence the game
 * animates its own at, from the game's data (object_animation.h), when their
 * own code advanced it so at the capture's probe.  Their
 * place does not change: the pose's sprites are built where the capture drew
 * the captured pose's (object_sprites.h), and the neighbour's OAM is composed
 * again with them in the order of its tags.  At a transition's start the
 * objects of the room entered go back to their captured pose, the one the
 * game freezes them in during the scroll.  The Low quality leaves them frozen.  Nothing
 * is written into the game: the state is the host's. */
#include "view_internal.h"

#include "object_animation.h"

#define OAM_ENTRIES 40u

/* What a neighbour shows of an object (view_render.c's filter, the design's): numbered, not placed at random, not
 * the era's or the season's blurb, which the view draws itself. */
static int shown(const OraclesObjectRecord *r)
{
    if (!r || !r->number || r->randomly_placed) return 0;
    return !(r->kind == ORACLES_OBJECT_KIND_INTERACTION && r->id == INTERAC_ERA_OR_SEASON_INFO);
}

static const OraclesObjectRecord *record_of(const entry *e, const OraclesSpriteTag *tag)
{
    for (unsigned k = 0; k < e->object_count; k++)
        if (e->objects[k].kind == tag->kind && e->objects[k].slot == tag->slot) return &e->objects[k];
    return NULL;
}

int ev_entry_shows_objects(const entry *e)
{
    for (unsigned i = 0; i < e->tag_count; i++)
        if (e->tags[i].kind && e->tags[i].count && shown(record_of(e, &e->tags[i]))) return 1;
    return 0;
}

/* The tag of an object's own drawing (its grass or puddle, then its sprites) in a frame's tags. */
static const OraclesSpriteTag *own_tag(const OraclesSpriteTag *tags, unsigned count, uint8_t kind, uint8_t slot)
{
    for (unsigned j = 0; j < count; j++)
        if (!tags[j].terrain_effect && tags[j].kind == kind && tags[j].slot == slot && tags[j].count) return &tags[j];
    return NULL;
}

static ev_pose *pose_of(entry *e, uint8_t kind, uint8_t slot)
{
    for (unsigned k = 0; k < e->pose_count; k++) if (e->poses[k].kind == kind && e->poses[k].slot == slot) return &e->poses[k];
    return NULL;
}

/* Whether a tag's entries put a sprite on the screen: an object the game hides on a frame may leave its entries
 * below it. */
static int on_screen(const uint8_t *oam, const OraclesSpriteTag *tag)
{
    for (unsigned n = 0; n < tag->count && tag->first + n < OAM_ENTRIES; n++) {
        const uint8_t *o = oam + (tag->first + n) * 4u;
        if (o[0] > 0 && o[0] < 160u && o[1] > 0 && o[1] < 168u) return 1;
    }
    return 0;
}

/* The object's own sprites in that pose, where the frame its capture keeps draws it (`r` as drawn,
 * oracles_objects_as_drawn): those @drawObject writes (inside the window). */
static int pose_sprites(const OraclesEnhancedView *v, const entry *e, const OraclesObjectRecord *r, unsigned lower, uint16_t pose,
                        OraclesObjectSprite out[ORACLES_OBJECT_SPRITES_MAX])
{
    const OraclesObjectPlace place = { r->yh, r->xh, (uint8_t)(r->zh + lower), (uint8_t)e->camera_y, (uint8_t)e->camera_x, r->oam_flags, r->oam_tile_base };
    OraclesObjectSprite built[ORACLES_OBJECT_SPRITES_MAX];
    const int n = oracles_object_sprites_pose(oracles_guest_tables(v->guest), v->rom, v->rom_size, &place, pose, built, ORACLES_OBJECT_SPRITES_MAX);
    if (n < 0) return -1;
    int written = 0;
    for (int i = 0; i < n; i++) if (built[i].in_window) out[written++] = built[i];
    return written;
}

static void write_sprite(uint8_t *at, const OraclesObjectSprite *s)
{
    at[0] = s->oam_y; at[1] = s->oam_x; at[2] = s->tile; at[3] = s->attr;
}

/* Whether one of the poses of its loop, placed so, is the last entries of the object's tag: its own sprites, its
 * grass or puddle before them.  The captured pose first; an object the game animates while the ghost settles (the
 * Subrosians, the spurt of Horon's fountain) may have been drawn a pose or two past the state its record holds.
 * Returns how many of the tag's entries are its own sprites and puts in `shown` the pose they show, -1 when no pose
 * is these. */
static int placed(const OraclesEnhancedView *v, const entry *e, const OraclesObjectRecord *r, unsigned lower, const uint8_t *oam, const OraclesSpriteTag *tag,
                  const uint16_t poses[], unsigned pose_count, uint16_t *shown)
{
    if ((unsigned)tag->first + tag->count > OAM_ENTRIES) return -1;
    for (unsigned k = 0; k < pose_count; k++) {
        OraclesObjectSprite s[ORACLES_OBJECT_SPRITES_MAX];
        const int n = pose_sprites(v, e, r, lower, poses[k], s);
        if (n <= 0 || (unsigned)n > tag->count) continue;
        const unsigned first = (unsigned)tag->first + tag->count - (unsigned)n;
        int same = 1;
        for (int i = 0; i < n && same; i++) {
            uint8_t expected[4];
            write_sprite(expected, &s[i]);
            same = memcmp(oam + (first + (unsigned)i) * 4u, expected, 4u) == 0;
        }
        if (same) { *shown = poses[k]; return n; }
    }
    return -1;
}

void ev_poses_deliver(OraclesEnhancedView *v, entry *e)
{
    e->pose_count = 0;
    e->poses_reset = 0;
    e->pose_lag_since = 0;
    memset(e->rendered_oam, 0, sizeof e->rendered_oam);
    if (!v->neighbour_objects || !v->rom || e->large) return;
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    for (unsigned k = 0; k < e->object_count; k++) {
        const OraclesObjectRecord drawn = oracles_objects_as_drawn(&e->objects[k]), *r = &drawn;
        const OraclesSpriteTag *tag = own_tag(e->tags, e->tag_count, r->kind, r->slot);
        /* Drawn: its own sprites on the screen in a frame kept.  Its record's look is the frame just drawn's, a frame
         * after the OAM kept (ghost.c): an object the game shows one frame in two (Ages' time portal) reads hidden on
         * the frame it shows, and may be hidden on the frame kept, its entries below the screen. */
        const OraclesSpriteTag *before = e->tag_count_before ? own_tag(e->tags_before, e->tag_count_before, r->kind, r->slot) : NULL;
        const int kept_on = tag && on_screen(e->oam, tag), before_on = before && on_screen(e->oam_before, before);
        if (!shown(r) || !(kept_on || before_on) || r->kind < ORACLES_OBJECT_KIND_INTERACTION || r->kind > ORACLES_OBJECT_KIND_PART) continue;
        OraclesObjectAnimation a;
        oracles_object_animation_from_record(&a, r);
        uint16_t poses[ORACLES_OBJECT_ANIMATION_POSES];
        const unsigned count = oracles_object_animation_poses(&a, t, v->rom, v->rom_size, poses, ORACLES_OBJECT_ANIMATION_POSES, NULL, NULL);
        if (!count && r->anim_pointer && oracles_object_animation_resolved(t, r->kind)) v->poses.frozen_unreadable++;   /* an animation set, its data out of reach */
        if (count < 2u) continue;   /* one pose, or its tables unknown */
        if (r->kind == ORACLES_OBJECT_KIND_ENEMY) { v->poses.frozen_enemies++; continue; }
        if (!oracles_object_animation_animated(t, r)) { v->poses.frozen_code++; continue; }
        /* Its place: a pose of its loop built there must be what the capture drew, in each frame kept that shows
         * it; a puddle lowers it by a pixel.  A frame that hides it keeps its entries as captured. */
        int own = -1, own_before = 0;
        unsigned lower = 0;
        uint16_t shown[2] = { a.oam_data_address, a.oam_data_address };
        for (; lower < 2u && own < 0; lower++) {
            own = kept_on ? placed(v, e, r, lower, e->oam, tag, poses, count, &shown[0]) : 0;
            shown[1] = shown[0];
            own_before = 0;
            if (own >= 0 && before_on) own_before = placed(v, e, r, lower, e->oam_before, before, poses, count, &shown[1]);
            if (!kept_on) shown[0] = shown[1];
            if (own_before < 0) own = -1;
        }
        if (own < 0) { v->poses.frozen_unplaced++; continue; }
        if (v->poses_off) { v->poses.frozen_low++; continue; }
        ev_pose *p = &e->poses[e->pose_count++];
        memset(p, 0, sizeof *p);
        p->kind = r->kind;
        p->slot = r->slot;
        p->record = (uint8_t)k;
        p->lower = (uint8_t)(lower - 1u);
        p->own[0] = (uint8_t)own;
        p->own[1] = (uint8_t)own_before;
        p->hidden[0] = (uint8_t)!kept_on;
        p->hidden[1] = (uint8_t)(before && !before_on);
        /* One frame kept, which shows it, and its record (the frame after) hides it: the game shows it one frame in
         * two, and so does the neighbour. */
        p->flicker = (uint8_t)(!e->tag_count_before && kept_on && !(r->visible & 0x80u));
        p->captured = p->now = a;
        p->shown[0] = shown[0];
        p->shown[1] = shown[1];
        p->drawn = shown[0];
        v->poses.animated++;
    }
}

void ev_poses_event(OraclesEnhancedView *v, const OraclesGuestEvent *event)
{
    if (event->type != ORACLES_EVENT_OBJECTS_UPDATED) return;
    /* Normal play: the objects ran their code, nothing froze them (a scroll, a text, a menu, wDisabledObjects, a
     * palette fade): the frames the game animates its own on. */
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    if ((oracles_guest_read8(v->guest, t->scroll_mode) & 0x7fu) != 0x01u || oracles_guest_read8(v->guest, t->screen_transition_state) != 0x02u
        || oracles_guest_read8(v->guest, t->text_is_active) || oracles_guest_read8(v->guest, t->opened_menu_type)
        || oracles_guest_read8(v->guest, t->disabled_objects) || oracles_guest_read8(v->guest, t->palette_thread_mode)) return;
    v->object_updates++;
}

void ev_poses_advance(OraclesEnhancedView *v)
{
    unsigned steps = v->object_updates;
    v->object_updates = 0;
    if (steps > ANIMATION_STEPS_MAX) steps = ANIMATION_STEPS_MAX;
    if (!steps || !v->rom) return;
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    v->poses.steps += steps;
    for (unsigned i = 0; i < v->slot_count; i++) {
        entry *e = &v->slots[i];
        if (!e->used || !e->valid) continue;
        for (unsigned k = 0; k < e->pose_count; k++) {
            ev_pose *p = &e->poses[k];
            for (unsigned s = 0; s < steps; s++) {
                const uint16_t before = p->now.oam_data_address;
                if (oracles_object_animation_step(&p->now, t, v->rom, v->rom_size) < 0) { p->now = p->captured; break; }
                if (p->now.oam_data_address != before) v->poses.changes++;
            }
        }
    }
}

/* The room a transition enters: its objects back in their captured poses, the render that shows them taking one of
 * the frame's renders of animation, kept for it so that it waits for none.  The other neighbours go on. */
void ev_poses_reset(OraclesEnhancedView *v, entry *e)
{
    v->object_updates = 0;
    if (!e || !e->used || !e->valid) return;
    for (unsigned k = 0; k < e->pose_count; k++) {
        ev_pose *p = &e->poses[k];
        if (p->now.oam_data_address != p->captured.oam_data_address) { v->poses.resets++; e->poses_reset = 1; }
        p->now = p->captured;
    }
    if (e->poses_reset) v->animation_renders++;
}

/* Whether an object the game shows one frame in two is off on this frame (`off`: an odd one). */
static int flickered_off(entry *e, const OraclesSpriteTag *tag, int off)
{
    const ev_pose *p = off ? pose_of(e, tag->kind, tag->slot) : NULL;
    return p && p->flicker;
}

/* One frame's OAM with the objects' poses: each tag's entries in order, an animated object's own sprites built for
 * its pose after its grass or puddle, the entries after them moved by the difference, forty at most.  `at` and
 * `count` receive where each pose's sprites went (count 0: not in this frame). */
static void compose(OraclesEnhancedView *v, entry *e, const uint8_t *oam, const OraclesSpriteTag *tags, unsigned tag_count, int before, int off,
                    uint8_t out[160], uint8_t at[ORACLES_OBJECT_RECORDS], uint8_t count[ORACLES_OBJECT_RECORDS])
{
    memset(out, 0, 160);
    memset(count, 0, ORACLES_OBJECT_RECORDS);
    int shift = 0;
    for (unsigned i = 0; i < tag_count; i++) {
        const OraclesSpriteTag *tag = &tags[i];
        if (tag->kind == 0) continue;
        const OraclesObjectRecord *object = record_of(e, tag);
        if (!shown(object) || flickered_off(e, tag, off)) continue;
        ev_pose *p = tag->terrain_effect ? NULL : pose_of(e, tag->kind, tag->slot);
        OraclesObjectSprite s[ORACLES_OBJECT_SPRITES_MAX];
        int built = -1;
        if (p && !p->hidden[before] && p->now.oam_data_address != p->shown[before]) {
            const OraclesObjectRecord drawn = oracles_objects_as_drawn(&e->objects[p->record]);
            built = pose_sprites(v, e, &drawn, p->lower, p->now.oam_data_address, s);
        }
        const unsigned own = p ? p->own[before] : 0u, kept = built >= 0 && own <= tag->count ? tag->count - own : tag->count;
        int index = (int)tag->first + shift;
        for (unsigned n = 0; n < kept; n++, index++)
            if (index >= 0 && index < (int)OAM_ENTRIES) memcpy(out + index * 4, oam + (tag->first + n) * 4u, 4u);
        if (built < 0) {
            if (p && index >= 0) { at[p - e->poses] = (uint8_t)(index - (int)own); count[p - e->poses] = (uint8_t)own; }
            continue;
        }
        if (index >= 0) { at[p - e->poses] = (uint8_t)index; count[p - e->poses] = (uint8_t)built; }
        for (int n = 0; n < built; n++, index++) if (index >= 0 && index < (int)OAM_ENTRIES) write_sprite(out + index * 4, &s[n]);
        shift += built - (int)own;
    }
}

int ev_neighbour_oam(OraclesEnhancedView *v, entry *e, uint8_t captured[160], uint8_t posed[160])
{
    memset(captured, 0, 160);
    memset(posed, 0, 160);
    if (!v->neighbour_objects || !e->tag_count) return 0;
    /* A full fade hides the rooms with their objects: the game shows only the
     * sprites it keeps (Link, while the Maku Tree speaks from afar). */
    if (v->fade_effective <= -32 || v->fade_effective >= 31) return 0;
    /* The two frames captured in turn, one a frame: an object the game draws
     * one frame in two (a fountain's spurt) flickers in the neighbour as it
     * does in the room, instead of being there or not depending on the frame
     * the capture fell on.  The others are the same in both. */
    const int before = e->tag_count_before && (v->frame & 1u), off = !e->tag_count_before && (v->frame & 1u);
    const uint8_t *oam = before ? e->oam_before : e->oam;
    const OraclesSpriteTag *tags = before ? e->tags_before : e->tags;
    const unsigned tag_count = before ? e->tag_count_before : e->tag_count;
    int any = 0;
    for (unsigned i = 0; i < tag_count; i++) {
        const OraclesSpriteTag *tag = &tags[i];
        if (tag->kind == 0 || !shown(record_of(e, tag))) continue;   /* Link, the companion, an item; what a neighbour does not show */
        if (flickered_off(e, tag, off)) continue;
        for (unsigned n = 0; n < tag->count; n++) {
            const unsigned index = (unsigned)tag->first + n;
            if (index >= OAM_ENTRIES) break;
            memcpy(captured + index * 4u, oam + index * 4u, 4u);
            any = 1;
        }
    }
    memcpy(posed, captured, 160);
    unsigned moved = 0;
    for (unsigned k = 0; k < e->pose_count; k++) moved += !e->poses[k].hidden[before] && e->poses[k].now.oam_data_address != e->poses[k].shown[before];
    if (any && moved) compose(v, e, oam, tags, tag_count, before, off, posed, v->pose_at, v->pose_count_at);
    else for (unsigned k = 0; k < e->pose_count; k++) v->pose_count_at[k] = 0;
    return any;
}

/* The render of a neighbour drew its objects with `posed`: the poses it shows are their steps', the changes counted;
 * with the harness's check, each object's sprites in it are those its step's pose builds, whether built for it or
 * kept from the capture. */
void ev_poses_drawn(OraclesEnhancedView *v, entry *e, const uint8_t posed[160], int moved)
{
    for (unsigned k = 0; k < e->pose_count; k++) {
        ev_pose *p = &e->poses[k];
        if (p->drawn != p->now.oam_data_address) v->poses.changes_drawn++;
        p->drawn = p->now.oam_data_address;
        if (!v->pose_verify || !moved || !v->pose_count_at[k]) continue;
        OraclesObjectSprite s[ORACLES_OBJECT_SPRITES_MAX];
        const OraclesObjectRecord drawn = oracles_objects_as_drawn(&e->objects[p->record]);
        const int n = pose_sprites(v, e, &drawn, p->lower, p->now.oam_data_address, s);
        int wrong = n != (int)v->pose_count_at[k];
        for (int i = 0; i < n && !wrong; i++) {
            uint8_t expected[4];
            write_sprite(expected, &s[i]);
            const unsigned index = (unsigned)v->pose_at[k] + (unsigned)i;
            wrong = index < OAM_ENTRIES && memcmp(posed + index * 4u, expected, 4u) != 0;
        }
        v->poses.drawn_wrong += (unsigned)wrong;
    }
    e->poses_reset = 0;
}

/* A neighbour shown this frame: how long it has drawn a pose behind its objects' steps. */
void ev_poses_shown(OraclesEnhancedView *v, entry *e, int drawing)
{
    int behind = 0;
    if (!drawing) { e->pose_lag_since = 0; return; }   /* its objects hidden (a full fade) */
    for (unsigned k = 0; k < e->pose_count && !behind; k++) behind = e->poses[k].drawn != e->poses[k].now.oam_data_address;
    if (!behind) { e->pose_lag_since = 0; return; }
    if (!e->pose_lag_since) e->pose_lag_since = v->frame + 1u;
    const unsigned lag = v->frame + 2u - e->pose_lag_since;
    if (lag > v->poses.lag_max) v->poses.lag_max = lag;
}

/* The harness's check of a render that drew some lines alone: the whole area drawn again from the same inputs, the
 * lines that differ counted. */
void ev_poses_check_lines(OraclesEnhancedView *v, entry *e, const OraclesPpuInput *in, unsigned width)
{
    if (!v->pose_verify) return;
    oracles_ppu_render_wide_lines(in, v->strip, width, ORACLES_ENHANCED_HUD_HEIGHT, ORACLES_ENHANCED_HUD_HEIGHT + ORACLES_GHOST_AREA_HEIGHT);
    for (unsigned y = 0; y < ORACLES_GHOST_AREA_HEIGHT; y++)
        if (memcmp(e->live_area + y * width, v->strip + (ORACLES_ENHANCED_HUD_HEIGHT + y) * width, width * sizeof *v->strip) != 0) v->poses.lines_wrong++;
}

void oracles_enhanced_view_set_neighbour_animation(OraclesEnhancedView *v, int enabled)
{
    if (v->poses_off == !enabled) return;
    v->poses_off = !enabled;
    /* Objects kept in their captured poses need no probe: the ghost runs none. */
    for (unsigned l = 0; l < v->lane_count; l++) {
        OraclesGhost *ghost = l == v->lane_now ? v->ghost : v->lanes[l].ghost;
        if (ghost) oracles_ghost_set_probe(ghost, enabled);
    }
    ev_invalidate_all(v);   /* the captures delivered are animated or not from their delivery */
}

void oracles_enhanced_view_pose_verify(OraclesEnhancedView *v, int enabled) { v->pose_verify = enabled != 0; }
void oracles_enhanced_view_pose_counts(const OraclesEnhancedView *v, OraclesEnhancedPoseCounts *out) { *out = v->poses; }
