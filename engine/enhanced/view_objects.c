/* The objects that appear on entry (design, 6.2.3): what the neighbour did not
 * show (the objects placed at random, those another object created, those the
 * capture could not pair) exists in the live instance from the load of the room
 * and becomes visible once its own initialisation has run.  For sixteen frames
 * from then the view draws it with an opacity rising from zero to one, inside
 * the game's window, over a render of that window without its sprites.
 * And the objects that vanish (design, 6.2.2): the image of a room left shows
 * its objects frozen; when a later transition replaces it, what the room's
 * capture does not show where the image did fades out over as many frames.
 * Nothing is written into the game. */
#include "view_internal.h"

#define FADE_FRAMES 16u          /* in and out, chosen by eye, on captures */
#define VANISH_NONE UINT32_MAX   /* no object's pixel there: the image's are RGB555 */

typedef struct appearing { unsigned oam_index; unsigned alpha_256; } appearing;

void ev_objects_event(OraclesEnhancedView *v, const OraclesGuestEvent *event)
{
    oracles_objects_event(v->live_objects, event);
    ev_poses_event(v, event);
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    if (event->type == ORACLES_EVENT_ROOM_ENTER) {
        /* A room entered by a scrolling transition, the only entry where it was
         * visible before: its objects may fade in.  A warp arrives on a blank
         * screen and shows its room whole. */
        memset(v->appear_seen, 0, sizeof v->appear_seen);
        v->appear_active = (oracles_guest_read8(v->guest, t->scroll_mode) & 0x7fu) == ORACLES_SCROLL_MODE_TRANSITION;   /* cutscene01 writes it before initializeRoom */
        v->appear_ready = 0;
        v->appear_ended = 0;
        for (unsigned i = 0; i < v->handoff_count; i++) if (!v->handoff[i].checked) v->handoff_unseen++;
        v->handoff_count = 0;
        v->handoff_armed = v->appear_active;
    } else if (event->type == ORACLES_EVENT_ROOM_INITIALIZED && v->appear_active) {
        /* Until the room entered is initialised, the OAM on screen is still the
         * room left's (the load waits for vblanks): nothing appears before. */
        v->appear_group = oracles_guest_read8(v->guest, t->active_group);
        v->appear_room = oracles_guest_read8(v->guest, t->active_room);
        v->appear_ready = 1;
    }
}

/* Whether a live object is one the capture of its room showed: paired by its
 * key as the room created it and its rank among the numbered objects of that
 * key (docs/ARCHITECTURE.md, pairing),
 * numbered, not placed at random, and drawn in the capture. */
static const OraclesObjectRecord *paired_shown(const entry *e, const OraclesObjectRecord *live, const OraclesObjectRecord *all, unsigned all_count)
{
    if (!e || !live->number || live->randomly_placed) return NULL;
    unsigned rank = 0;
    for (unsigned k = 0; k < all_count; k++)
        if (all[k].number && all[k].number < live->number && all[k].kind == live->kind && all[k].created_id == live->created_id
            && all[k].created_subid == live->created_subid && all[k].created_var03 == live->created_var03) rank++;
    unsigned seen = 0;
    for (unsigned k = 0; k < e->object_count; k++) {
        const OraclesObjectRecord *c = &e->objects[k];
        if (!c->number || c->kind != live->kind || c->created_id != live->created_id || c->created_subid != live->created_subid
            || c->created_var03 != live->created_var03) continue;
        if (seen++ != rank) continue;
        if (c->randomly_placed) return NULL;
        for (unsigned j = 0; j < e->tag_count; j++)
            if (!e->tags[j].terrain_effect && e->tags[j].kind == c->kind && e->tags[j].slot == c->slot) return c;
        return NULL;
    }
    return NULL;
}

static int shown_by_capture(const entry *e, const OraclesObjectRecord *live, const OraclesObjectRecord *all, unsigned all_count)
{
    return paired_shown(e, live, all, all_count) != NULL;
}

/* The box of an object's sprites in the capture, in room pixels: the ghost's
 * camera is at 0 in a small room, so an OAM entry at (y, x) covers the room's
 * pixels from (y - 16, x - 8).  Read from the ghost's own OAM and tags, an
 * entry off the screen left out: the capture does not show it there (Ages'
 * time portal flickers, and its hidden frame's entry is below the screen). */
static int capture_box(const entry *e, const OraclesObjectRecord *c, int32_t *left, int32_t *top, int32_t *right, int32_t *bottom)
{
    int any = 0;
    for (unsigned j = 0; j < e->tag_count; j++) {
        const OraclesSpriteTag *tag = &e->tags[j];
        if (tag->kind != c->kind || tag->slot != c->slot) continue;
        for (unsigned n = 0; n < tag->count && tag->first + n < 40u; n++) {
            const uint8_t *o = e->oam + (tag->first + n) * 4u;
            if (o[0] == 0 || o[1] == 0 || o[0] >= 160u || o[1] >= 168u) continue;
            const int32_t x = (int32_t)o[1] - 8, y = (int32_t)o[0] - 16;
            if (!any || x < *left) *left = x;
            if (!any || y < *top) *top = y;
            if (!any || x + 8 > *right) *right = x + 8;
            if (!any || y + 16 > *bottom) *bottom = y + 16;
            any = 1;
        }
    }
    return any;
}

/* The hand-off of the objects a capture showed (docs/ARCHITECTURE.md, who draws an object during an entry): once the
 * room entered is initialised, each such object is followed until its live twin
 * is visible.  A jump: the live twin appears elsewhere than the capture showed
 * it.  A hole frame: the object's box overlaps the game's window, where the
 * core draws and the capture does not, while its twin is not visible yet. */
static void measure_handoff(OraclesEnhancedView *v, const entry *captured, const OraclesObjectRecord *live, unsigned live_count)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    if (v->handoff_armed && captured) {
        v->handoff_armed = 0;
        for (unsigned k = 0; k < live_count && v->handoff_count < 48u; k++) {
            if ((live[k].enabled & 3u) != 1u) continue;
            const OraclesObjectRecord *c = paired_shown(captured, &live[k], live, live_count);
            if (!c) continue;
            int32_t l = 0, tp = 0, r = 0, b = 0;
            if (!capture_box(captured, c, &l, &tp, &r, &b)) continue;
            const int32_t origin_x = (int32_t)(v->appear_room & 0x0fu) * SMALL_ROOM_W, origin_y = (int32_t)(v->appear_room >> 4u) * (int32_t)ORACLES_GHOST_AREA_HEIGHT;
            v->handoff[v->handoff_count].kind = live[k].kind;
            v->handoff[v->handoff_count].slot = live[k].slot;
            v->handoff[v->handoff_count].checked = 0;
            v->handoff[v->handoff_count].shown = *c;
            v->handoff[v->handoff_count].box_left = origin_x + l;
            v->handoff[v->handoff_count].box_top = origin_y + tp;
            v->handoff[v->handoff_count].box_right = origin_x + r;
            v->handoff[v->handoff_count].box_bottom = origin_y + b;
            v->handoff_count++;
            v->handoff_shown++;
        }
    }
    for (unsigned i = 0; i < v->handoff_count; i++) {
        if (v->handoff[i].checked) continue;
        const uint8_t *object = oracles_guest_object(v->guest, v->handoff[i].slot, v->handoff[i].kind);
        if (!object || (object[ORACLES_OBJ_ENABLED] & 3u) != 1u) { v->handoff[i].checked = 1; continue; }   /* gone before it showed */
        /* The capture keeps an object once its own initialisation has run
         * (its state left 0); the live twin is followed to the same moment,
         * for an object is visible before its code has placed it (a monkey
         * sets its position after spawning the nine others).  One the
         * capture kept in state 0, waiting for Link, is judged as it is. */
        if ((object[ORACLES_OBJ_VISIBLE] & 0x80u) && (object[ORACLES_OBJ_STATE] != 0 || v->handoff[i].shown.state == 0)) {
            const OraclesObjectRecord *s = &v->handoff[i].shown;
            /* Against where the capture drew it, the frame whose OAM it keeps. */
            if (object[ORACLES_OBJ_Y] != s->drawn_y || object[ORACLES_OBJ_YH] != s->drawn_yh || object[ORACLES_OBJ_X] != s->drawn_x || object[ORACLES_OBJ_XH] != s->drawn_xh) v->handoff_jumps++;
            v->handoff[i].checked = 1;
            continue;
        }
        const int32_t wl = ob->window_left, wt = ob->window_top, wr = wl + (int32_t)ORACLES_ENHANCED_CORE_WIDTH, wb = wt + (int32_t)ORACLES_ENHANCED_AREA_HEIGHT;
        if (v->handoff[i].box_left < wr && v->handoff[i].box_right > wl && v->handoff[i].box_top < wb && v->handoff[i].box_bottom > wt) v->handoff_hole_frames++;
    }
}

/* The objects the capture of the room just entered showed that the live room
 * has none of, read as the fade's window closes, its objects settled: people
 * the neighbour showed that were not there (a capture older than the story
 * that decides them).  An object gone in the meantime counts too. */
static void count_orphans(OraclesEnhancedView *v)
{
    const entry *captured = ev_find_entry(v, v->appear_group, v->appear_room);
    if (!captured || !ev_entry_drawable(v, captured)) return;
    OraclesObjectRecord live[ORACLES_OBJECT_RECORDS];
    const unsigned live_count = oracles_objects_snapshot(v->live_objects, live, ORACLES_OBJECT_RECORDS);
    uint8_t twin[ORACLES_OBJECT_RECORDS] = { 0 };
    for (unsigned k = 0; k < live_count; k++) {
        const OraclesObjectRecord *c = (live[k].enabled & 3u) == 1u ? paired_shown(captured, &live[k], live, live_count) : NULL;
        if (c) twin[c - captured->objects] = 1;
    }
    for (unsigned k = 0; k < captured->object_count; k++) {
        const OraclesObjectRecord *c = &captured->objects[k];
        if (twin[k] || !c->number || c->randomly_placed) continue;
        for (unsigned j = 0; j < captured->tag_count; j++)
            if (!captured->tags[j].terrain_effect && captured->tags[j].kind == c->kind && captured->tags[j].slot == c->slot) { v->handoff_orphans++; break; }
    }
}

void ev_fade_appearing(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y)
{
    if (!v->neighbour_objects || !v->appear_active || !v->appear_ready || !v->live_sprites || !v->live_objects) return;
    const OraclesEnhancedObservation *ob = &v->observation;
    /* The fade runs through the scroll and for as long as an object may still
     * be fading after it: the window closes FADE_FRAMES frames after the end. */
    if (!ob->in_transition) {
        if (!v->appear_ended) { v->appear_ended = 1; v->appear_end_frame = v->frame; }
        else if (v->frame > v->appear_end_frame + FADE_FRAMES) { v->appear_active = 0; count_orphans(v); return; }
    }
    const uint8_t *oam = oracles_guest_oam(v->guest), *io = oracles_guest_io(v->guest), *palettes = oracles_guest_obj_palettes(v->guest);
    const uint8_t *vram0 = oracles_guest_vram(v->guest, 0), *vram1 = oracles_guest_vram(v->guest, 1);
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const uint8_t *regs = oracles_guest_ptr(v->guest, t->gfx_regs3, 6);
    const uint8_t *bg_palettes = oracles_guest_bg_palettes(v->guest);
    if (!oam || !io || !palettes || !vram0 || !vram1 || !regs || !bg_palettes || !(io[IO_LCDC] & 0x02u)) return;

    OraclesObjectRecord live[ORACLES_OBJECT_RECORDS];
    const unsigned live_count = oracles_objects_snapshot(v->live_objects, live, ORACLES_OBJECT_RECORDS);
    const entry *captured = ev_find_entry(v, v->appear_group, v->appear_room);
    if (captured && !ev_entry_drawable(v, captured)) captured = NULL;
    measure_handoff(v, captured, live, live_count);
    OraclesSpriteTag tags[ORACLES_SPRITE_TAGS];
    const unsigned tag_count = oracles_sprites_tags_for_oam(v->live_sprites, oam, tags, ORACLES_SPRITE_TAGS);
    if (!tag_count) return;

    appearing sprites[40];
    unsigned count = 0;
    for (unsigned i = 0; i < tag_count; i++) {
        const OraclesSpriteTag *tag = &tags[i];
        if (tag->kind < 1u || tag->kind > 3u) continue;   /* Link, the companion, the items */
        const OraclesObjectRecord *record = NULL;
        for (unsigned k = 0; k < live_count; k++) if (live[k].kind == tag->kind && live[k].slot == tag->slot) { record = &live[k]; break; }
        /* Mode 1 only: mode 2 is the room left, cleared at the end of the scroll
         * and shown by its image; mode 3 persists across rooms. */
        if (!record || (record->enabled & 3u) != 1u) continue;
        if (shown_by_capture(captured, record, live, live_count)) continue;
        uint32_t *first = &v->appear_first[tag->kind][tag->slot];
        if (!v->appear_seen[tag->kind][tag->slot]) { v->appear_seen[tag->kind][tag->slot] = 1; *first = v->frame; v->faded_objects++; }
        const uint32_t age = v->frame - *first + 1u;
        if (age >= FADE_FRAMES) continue;
        for (unsigned n = 0; n < tag->count && tag->first + n < 40u && count < 40u; n++) {
            sprites[count].oam_index = tag->first + n;
            sprites[count].alpha_256 = age * 256u / FADE_FRAMES;
            count++;
        }
    }
    if (!count) return;
    v->fade_frames++;

    /* The window as the LCD drew it without the sprites that appear. */
    uint8_t without[160];
    memcpy(without, oam, sizeof without);
    for (unsigned n = 0; n < count; n++) without[sprites[n].oam_index * 4u] = 0;   /* y 0: hidden */
    OraclesPpuInput in;
    in.vram = v->hybrid_vram;
    memcpy(v->hybrid_vram, vram0, 0x2000u);
    memcpy(v->hybrid_vram + 0x2000u, vram1, 0x2000u);
    in.oam = without;
    in.bg_palettes = bg_palettes;
    in.obj_palettes = palettes;
    in.colours = v->colours;
    const OraclesPpuRegs r = { (uint8_t)(regs[0] | 0x80u), (uint8_t)(ob->drawn_camera_y + ob->drawn_offset_y - (int)ORACLES_ENHANCED_HUD_HEIGHT),
                               (uint8_t)(ob->drawn_camera_x + ob->drawn_offset_x), regs[3], regs[4] };
    for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) in.lines[ly] = r;
    oracles_ppu_render(&in, v->popup_frame);

    const unsigned height = (io[IO_LCDC] & 0x04u) ? 16u : 8u;
    for (unsigned n = 0; n < count; n++) {
        const uint8_t *e = oam + sprites[n].oam_index * 4u;
        const int sx = (int)e[1] - 8, sy = (int)e[0] - 16 - (int)ORACLES_ENHANCED_HUD_HEIGHT;
        const unsigned a = sprites[n].alpha_256;
        for (unsigned row = 0; row < height; row++)
            for (unsigned col = 0; col < 8u; col++) {
                const int gx = sx + (int)col, gy = sy + (int)row;
                if (gx < 0 || gx >= (int)ORACLES_ENHANCED_CORE_WIDTH || gy < 0 || gy >= (int)ORACLES_ENHANCED_AREA_HEIGHT) continue;
                const int32_t px = ob->window_left + gx - camera_x, py = ob->window_top + gy - camera_y;
                if (px < 0 || px >= (int32_t)v->size.width || py < 0 || py >= (int32_t)v->band_height) continue;
                uint32_t *dst = ev_band_pixel(v, (unsigned)px, (unsigned)py);
                const uint32_t shown = *dst, under = v->popup_frame[(ORACLES_ENHANCED_HUD_HEIGHT + (unsigned)gy) * ORACLES_PPU_WIDTH + (unsigned)gx];
                if (shown == under) continue;
                uint32_t out = shown & 0xff000000u;
                for (unsigned shift = 0; shift < 24u; shift += 8u) {
                    const unsigned s = (shown >> shift) & 0xffu, u = (under >> shift) & 0xffu;
                    out |= ((u * (256u - a) + s * a) >> 8) << shift;
                }
                *dst = out;
            }
    }
}

/* The objects of the room left that fade out (design, 6.2.2): at a scroll the
 * game's window leaves the room, whose objects it froze, and beyond the window
 * the room is shown by its capture, which holds only the objects it paired, at
 * the place it captured them.  Each other object, the companion, Link and his
 * items aside, is kept from the image of the room left, its pixels as the game
 * last drew it in normal play (they do not depend on the tiles the room entered
 * loads since): drawn whole beyond the window until the end of the scroll, as
 * frozen as the live object is, then fading out. */
static void start_vanish(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y)
{
    const entry *captured = ev_find_entry(v, v->left_group, v->left_room);
    if (captured && !ev_entry_drawable(v, captured)) captured = NULL;
    const unsigned height = (v->left_render_regs.lcdc & 0x04u) ? 16u : 8u;
    uint8_t under[160];
    memcpy(under, v->left_oam, sizeof under);
    unsigned objects = 0;
    for (unsigned i = 0; i < v->left_tag_count; i++) {
        const OraclesSpriteTag *tag = &v->left_tags[i];
        const OraclesObjectRecord *record = NULL;
        for (unsigned k = 0; k < v->left_object_count; k++)
            if (v->left_objects[k].kind == tag->kind && v->left_objects[k].slot == tag->slot) { record = &v->left_objects[k]; break; }
        if (!record || (record->kind == ORACLES_OBJECT_KIND_INTERACTION && record->id == INTERAC_ERA_OR_SEASON_INFO)) continue;
        const OraclesObjectRecord *c = paired_shown(captured, record, v->left_objects, v->left_object_count);
        if (c && c->drawn_yh == record->yh && c->drawn_xh == record->xh) continue;   /* the capture shows it there */
        int32_t l = 0, t = 0, r = 0, b = 0;
        int any = 0;
        for (unsigned n = 0; n < tag->count && tag->first + n < 40u; n++) {
            uint8_t *o = under + (tag->first + n) * 4u;
            if (o[0] == 0) continue;
            /* The image is an LCD frame's game area: its line 0 is the screen's 16, the OAM's 32. */
            const int32_t x = (int32_t)o[1] - 8, y = (int32_t)o[0] - 16 - (int32_t)ORACLES_ENHANCED_HUD_HEIGHT;
            if (!any || x < l) l = x;
            if (!any || y < t) t = y;
            if (!any || x + 8 > r) r = x + 8;
            if (!any || y + (int32_t)height > b) b = y + (int32_t)height;
            any = 1;
            o[0] = 0;   /* y 0: hidden */
        }
        if (any && ev_shown_meets(v, camera_x, camera_y, v->left_left + l, v->left_top + t, r - l, b - t, 0)) objects++;
    }
    if (!objects) return;
    /* Their pixels: where the image differs from its render without them. */
    OraclesPpuInput in;
    in.vram = v->left_vram;
    in.oam = under;
    in.bg_palettes = v->left_palettes;
    in.obj_palettes = v->left_obj_palettes;
    in.colours = v->raw_colours;
    for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) in.lines[ly] = v->left_render_regs;
    const unsigned first = ORACLES_PPU_HEIGHT - ORACLES_GHOST_AREA_HEIGHT;
    oracles_ppu_render_wide_lines(&in, v->hybrid_frame, ORACLES_PPU_WIDTH, first, ORACLES_PPU_HEIGHT);
    const uint32_t *without = v->hybrid_frame + first * ORACLES_PPU_WIDTH;
    for (unsigned i = 0; i < ORACLES_GHOST_AREA_WIDTH * ORACLES_GHOST_AREA_HEIGHT; i++)
        v->vanish_area[i] = v->left_area[i] != without[i] ? v->left_area[i] : VANISH_NONE;
    v->vanish_active = 1;
    v->vanish_first = 0;
    v->vanish_left = v->left_left;
    v->vanish_top = v->left_top;
    v->vanish_epoch = v->left_epoch;
    v->faded_out_objects += objects;
}

/* The first frame of a transition: the last capture of the room in play, its
 * objects in it, becomes the image of the room left (design, 6.2.2), drawn
 * where no capture of that room is; at a scroll, what the capture does not
 * show of its objects is kept from it to fade out.  A warp shows nothing of
 * the room it leaves. */
void ev_keep_left_image(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y)
{
    memcpy(v->left_area, ev_source_area(v), sizeof v->left_area);
    v->left_group = v->source_group;
    v->left_room = v->source_room;
    v->left_entered_side = v->ref_entered_side;
    v->left_left = v->source_left;
    v->left_top = v->source_top;
    v->left_epoch = v->source_epoch;
    v->left_pipeline = v->source_pipeline;
    memcpy(v->left_vram, v->source_vram, sizeof v->left_vram);
    v->left_regs = v->source_regs;
    memcpy(v->left_palettes, v->source_palettes, sizeof v->left_palettes);
    v->left_render_regs = v->source_render_regs;
    memcpy(v->left_oam, v->source_oam, sizeof v->left_oam);
    memcpy(v->left_obj_palettes, v->source_obj_palettes, sizeof v->left_obj_palettes);
    memcpy(v->left_tags, v->source_tags, v->source_tag_count * sizeof v->left_tags[0]);
    v->left_tag_count = v->source_tag_count;
    memcpy(v->left_objects, v->source_objects, v->source_object_count * sizeof v->left_objects[0]);
    v->left_object_count = v->source_object_count;
    v->left_shown_version = 0;
    v->left_valid = 1;
    v->vanish_active = 0;
    /* The room the window still shows on a scroll's first frame: the room left
     * (a capture kept for another room stands beside it). */
    const OraclesEnhancedObservation *ob = &v->observation;
    if (ob->in_scroll && v->left_epoch == ob->epoch && v->left_left == ob->window_left && v->left_top == ob->window_top) start_vanish(v, camera_x, camera_y);
}

/* The objects kept from the image of the room left, over whatever the band
 * draws at their room's place (its capture): whole until the scroll ends, then
 * with an opacity falling from one to zero over FADE_FRAMES frames; outside
 * the game's window, where the core draws the live game, and inside their room. */
void ev_fade_vanishing(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y)
{
    if (!v->vanish_active) return;
    const OraclesEnhancedObservation *ob = &v->observation;
    if (ob->epoch != v->vanish_epoch) { v->vanish_active = 0; return; }
    if (!v->vanish_first && !ob->in_scroll) v->vanish_first = v->frame;
    const uint32_t age = v->vanish_first ? v->frame - v->vanish_first + 1u : 0u;
    if (age >= FADE_FRAMES) { v->vanish_active = 0; return; }
    const unsigned a = (FADE_FRAMES - age) * 256u / FADE_FRAMES;
    const int32_t wl = ob->window_left, wt = ob->window_top, wr = wl + (int32_t)ORACLES_ENHANCED_CORE_WIDTH, wb = wt + (int32_t)ORACLES_ENHANCED_AREA_HEIGHT;
    int drew = 0;
    for (unsigned y = 0; y < ORACLES_GHOST_AREA_HEIGHT; y++)
        for (unsigned x = 0; x < ORACLES_GHOST_AREA_WIDTH; x++) {
            const uint32_t colour = v->vanish_area[y * ORACLES_GHOST_AREA_WIDTH + x];
            if (colour == VANISH_NONE) continue;
            const int32_t wx = v->vanish_left + (int32_t)x, wy = v->vanish_top + (int32_t)y;
            if (wx >= wl && wx < wr && wy >= wt && wy < wb) continue;
            const int32_t px = wx - camera_x, py = wy - camera_y;
            if (px < 0 || px >= (int32_t)v->size.width || py < 0 || py >= (int32_t)v->band_height) continue;
            uint32_t *dst = ev_band_pixel(v, (unsigned)px, (unsigned)py);
            const uint32_t shown = *dst, object = oracles_enhanced_faded_pixel(v->colours, colour, v->fade_effective);
            uint32_t out = shown & 0xff000000u;
            for (unsigned shift = 0; shift < 24u; shift += 8u) {
                const unsigned s = (shown >> shift) & 0xffu, o = (object >> shift) & 0xffu;
                out |= ((s * (256u - a) + o * a) >> 8) << shift;
            }
            *dst = out;
            drew = 1;
        }
    if (drew && age) v->fade_out_frames++;
}

void oracles_enhanced_view_fade_out_counts(const OraclesEnhancedView *v, unsigned *frames, unsigned *objects)
{
    if (frames) *frames = v->fade_out_frames;
    if (objects) *objects = v->faded_out_objects;
}

void oracles_enhanced_view_fade_counts(const OraclesEnhancedView *v, unsigned *frames, unsigned *objects)
{
    if (frames) *frames = v->fade_frames;
    if (objects) *objects = v->faded_objects;
}

void oracles_enhanced_view_handoff_counts(const OraclesEnhancedView *v, unsigned *shown, unsigned *jumps, unsigned *hole_frames, unsigned *unseen)
{
    if (shown) *shown = v->handoff_shown;
    if (jumps) *jumps = v->handoff_jumps;
    if (hole_frames) *hole_frames = v->handoff_hole_frames;
    if (unseen) *unseen = v->handoff_unseen;
}

unsigned oracles_enhanced_view_handoff_orphans(const OraclesEnhancedView *v) { return v->handoff_orphans; }

int oracles_enhanced_view_beside_orphans(const OraclesEnhancedView *v, unsigned direction, const struct OraclesGhostResult *witness)
{
    if (direction > 3u || !v->shown[direction] || v->shown_slot[direction] >= v->slot_count || !witness) return -1;
    const entry *e = &v->slots[v->shown_slot[direction]];
    if (!ev_entry_drawable(v, e) || e->refresh || e->rerun || witness->group != e->group || witness->room != e->room) return -1;   /* run again already: judged once delivered */
    uint8_t twin[ORACLES_OBJECT_RECORDS] = { 0 };
    for (unsigned k = 0; k < witness->object_count; k++) {
        const OraclesObjectRecord *w = &witness->objects[k];
        int drawn = 0;
        for (unsigned j = 0; j < witness->tag_count && !drawn; j++)
            drawn = !witness->tags[j].terrain_effect && witness->tags[j].kind == w->kind && witness->tags[j].slot == w->slot;
        const OraclesObjectRecord *c = drawn ? paired_shown(e, w, witness->objects, witness->object_count) : NULL;
        if (c) twin[c - e->objects] = 1;
    }
    int orphans = 0;
    for (unsigned k = 0; k < e->object_count; k++) {
        const OraclesObjectRecord *c = &e->objects[k];
        if (twin[k] || !c->number || c->randomly_placed) continue;
        for (unsigned j = 0; j < e->tag_count; j++)
            if (!e->tags[j].terrain_effect && e->tags[j].kind == c->kind && e->tags[j].slot == c->slot) { orphans++; break; }
    }
    return orphans;
}
