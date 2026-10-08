#include "view_internal.h"

/* ---- start / stop ---------------------------------------------------------------------- */

static void on_view_event(void *opaque, const OraclesGuestEvent *event)
{
    ev_large_objects_event((OraclesEnhancedView *)opaque, event);
    ev_hotbar_event((OraclesEnhancedView *)opaque, event);
}

OraclesEnhancedView *oracles_enhanced_view_start(OraclesCore *core, OraclesGuest *guest,
                                                 const uint8_t *rom, size_t rom_size)
{
    OraclesEnhancedView *v = calloc(1, sizeof *v);
    if (v) for (unsigned i = 0; i < ORACLES_PPU_COLOURS; i++) v->raw_colours[i] = i;
    if (!v) return NULL;
    v->core = core;
    v->guest = guest;
    v->camera = oracles_enhanced_camera_start(guest);
    if (!v->camera) { free(v); return NULL; }
    v->size = oracles_enhanced_size(0);
    v->band_height = oracles_enhanced_band_height(v->size);
    v->shown_width = v->size.width; v->shown_height = v->band_height;
    v->slot_count = NORMAL_SLOTS;
    v->slots = calloc(v->slot_count, sizeof *v->slots);
    v->surface = calloc((size_t)v->size.width * v->size.height, sizeof *v->surface);
    if (!v->slots || !v->surface) { free(v->slots); free(v->surface); oracles_enhanced_camera_stop(v->camera); free(v); return NULL; }
    const OraclesCompatProfile *profile = oracles_guest_profile(guest);
    v->observer.map_width = oracles_compat_map_width(profile);
    if (rom && rom_size) {
        v->rom = malloc(rom_size);
        if (v->rom) { memcpy(v->rom, rom, rom_size); v->rom_size = rom_size; }
        v->ghost = oracles_ghost_create(rom, rom_size, profile, oracles_core_kind(core));
        v->state_size = oracles_core_state_size(core);
        v->snapshot = malloc(v->state_size);
        if (!v->ghost || !v->snapshot || v->state_size != oracles_ghost_state_size(v->ghost)) {
            if (v->ghost) oracles_ghost_destroy(v->ghost);
            free(v->snapshot);
            v->ghost = NULL; v->snapshot = NULL;   /* no neighbours: black */
        } else {
            oracles_ghost_set_trace(v->ghost, 1);  /* the bytes each room's substitutions read: its invalidation */
            oracles_ghost_set_prerun(v->ghost, GHOST_PRERUN_FRAMES);
        }
        v->job = malloc(sizeof *v->job);
        if (v->ghost && !v->job) { oracles_ghost_destroy(v->ghost); free(v->snapshot); v->ghost = NULL; v->snapshot = NULL; }
        v->lane_count = 1;
        v->lanes[0].ghost = v->ghost;
        v->lanes[0].job = v->job;
        v->rom_profile = profile;
        v->ghost_core = oracles_core_kind(core);
    }
    v->key_count = oracles_ghost_key_ranges(oracles_guest_profile(guest), v->key, sizeof v->key / sizeof v->key[0]);
    v->colours_pipeline = -1;
    v->level_change = -1;
    v->large_objects_listening = oracles_guest_add_event_listener(guest, on_view_event, v) == 0;
    /* The game's animation steps, for the neighbours' own: without
     * the hook they are stepped once a composition. */
    v->animation_steps_counted = v->large_objects_listening
        && oracles_guest_add_hook(guest, oracles_guest_tables(guest)->update_animations, ORACLES_EVENT_ANIMATIONS, 0) == 0;
    /* The objects' drawing, to know a drawing the game skipped (a large room's objects). */
    if (v->large_objects_listening && oracles_sprites_add_hooks(guest) != 0) {
        oracles_guest_remove_event_listener(guest, on_view_event, v);
        v->large_objects_listening = 0;
        v->animation_steps_counted = 0;   /* no listener: one step a composition */
    }
    return v;
}

static void on_live_event(void *opaque, const OraclesGuestEvent *event);

void oracles_enhanced_view_stop(OraclesEnhancedView *v)
{
    if (!v) return;
    if (v->large_objects_listening) oracles_guest_remove_event_listener(v->guest, on_view_event, v);
    if (v->live_sprites) {
        oracles_guest_remove_event_listener(v->guest, on_live_event, v);
        oracles_sprites_destroy(v->live_sprites);
        oracles_objects_destroy(v->live_objects);
        v->live_sprites = NULL;
        v->live_objects = NULL;
    }
    ev_lane_serve(v, 0);
    for (unsigned l = 0; l < v->lane_count; l++) {
        if (v->lanes[l].ghost) oracles_ghost_destroy(v->lanes[l].ghost);
        if (l) free(v->lanes[l].job);   /* lane 0's is the view's own, freed below */
    }
    free(v->job);
    for (unsigned i = 0; i < v->slot_count; i++) free(v->slots[i].settled_state);
    free(v->slots);
    free(v->surface);
    free(v->run_plan);
    free(v->sea_plan);
    free(v->hotbar_icons);
    free(v->snapshot);
    free(v->rom);
    oracles_enhanced_camera_stop(v->camera);
    free(v);
}

uint32_t oracles_enhanced_view_width(const OraclesEnhancedView *v) { return v->size.width; }
uint32_t oracles_enhanced_view_height(const OraclesEnhancedView *v) { return v->size.height; }

void oracles_enhanced_view_set_zoom_out(OraclesEnhancedView *v, int enabled) { oracles_enhanced_view_set_size(v, oracles_enhanced_size(enabled)); }

void oracles_enhanced_view_set_size(OraclesEnhancedView *v, OraclesEnhancedSize size)
{
    /* Before the first composition: the cache is empty, the surface unwritten.  A band wider than the normal one
     * draws back, and shows more rooms at once. */
    /* Only the sizes of the levels (compositor.h): the view, the camera and the hotbar are made for those. */
    int known = 0;
    for (unsigned l = 0; l < ORACLES_ENHANCED_LEVELS; l++)
        for (unsigned a = 0; a < ORACLES_ENHANCED_ASPECTS; a++) {
            const OraclesEnhancedSize s = oracles_enhanced_view_size((OraclesEnhancedLevel)l, (OraclesEnhancedAspect)a);
            known |= s.width == size.width && s.height == size.height;
        }
    if (!known) return;
    const unsigned slots = size.width > ORACLES_ENHANCED_NARROW_WIDTH ? SLOTS : NORMAL_SLOTS;
    entry *fresh = calloc(slots, sizeof *fresh);
    uint32_t *surface = calloc((size_t)size.width * size.height, sizeof *surface);
    if (!fresh || !surface) { free(fresh); free(surface); return; }   /* the view keeps its size */
    for (unsigned i = 0; i < v->slot_count; i++) free(v->slots[i].settled_state);
    free(v->slots); free(v->surface);
    v->slots = fresh; v->surface = surface;
    v->size = size;
    v->band_height = oracles_enhanced_band_height(v->size);
    v->observer.band_width = v->size.width;
    v->shown_width = v->size.width; v->shown_height = v->band_height;
    v->slot_count = slots;
    oracles_enhanced_camera_set_band(v->camera, v->size.width, v->band_height);
}

void oracles_enhanced_view_set_sync_budget(OraclesEnhancedView *v, unsigned frames_per_host_frame) { v->sync_budget = frames_per_host_frame; }

int oracles_enhanced_view_set_ghosts(OraclesEnhancedView *v, unsigned count)
{
    /* Synchronous, one: the run order, and the suite's hashes, are a single ghost's. */
    if (!v->ghost || v->sync_budget || count <= 1u || v->lane_count >= count) return (int)v->lane_count;
    ev_lane_serve(v, 0);
    while (v->lane_count < count && v->lane_count < EV_LANES) {
        struct ev_lane *lane = &v->lanes[v->lane_count];
        memset(lane, 0, sizeof *lane);
        lane->ghost = oracles_ghost_create(v->rom, v->rom_size, v->rom_profile, v->ghost_core);
        lane->job = malloc(sizeof *lane->job);
        if (!lane->ghost || !lane->job || oracles_ghost_state_size(lane->ghost) != v->state_size) {
            if (lane->ghost) oracles_ghost_destroy(lane->ghost);
            free(lane->job);
            memset(lane, 0, sizeof *lane);
            break;
        }
        oracles_ghost_set_trace(lane->ghost, 1);
        oracles_ghost_set_prerun(lane->ghost, GHOST_PRERUN_FRAMES);
        oracles_ghost_set_capture(lane->ghost, v->neighbour_objects);
        v->lane_count++;
    }
    return (int)v->lane_count;
}

unsigned oracles_enhanced_view_ghosts(const OraclesEnhancedView *v) { return v->lane_count; }
void oracles_enhanced_view_toggle(OraclesEnhancedView *v) { v->framed_only = !v->framed_only; }
int oracles_enhanced_view_framed_only(const OraclesEnhancedView *v) { return v->framed_only; }
void oracles_enhanced_view_set_camera_profile(OraclesEnhancedView *v, unsigned profile) { oracles_enhanced_camera_set_profile(v->camera, profile); }

/* ---- composition ----------------------------------------------------------------------- */

static void on_live_event(void *opaque, const OraclesGuestEvent *event)
{
    OraclesEnhancedView *v = opaque;
    oracles_sprites_event(v->live_sprites, event);
    ev_objects_event(v, event);
}

void oracles_enhanced_view_set_neighbour_objects(OraclesEnhancedView *v, int enabled)
{
    if (v->neighbour_objects == (enabled != 0)) return;
    v->neighbour_objects = enabled != 0;
    for (unsigned l = 0; l < v->lane_count; l++) oracles_ghost_set_capture(l == v->lane_now ? v->ghost : v->lanes[l].ghost, v->neighbour_objects);
    if (v->neighbour_objects && !v->live_sprites) {
        /* The live sprites tagged by object: which of the OAM entries on screen
         * are the room's objects, for the image of a room left and for the
         * overlay during a transition.  Observation only. */
        v->live_sprites = oracles_sprites_create(v->guest);
        v->live_objects = oracles_objects_create(v->guest);
        if (v->live_sprites && v->live_objects) {
            /* Without its hooks the tagging would lose entries silently: no live tags at all rather than wrong ones. */
            if (oracles_sprites_add_hooks(v->guest) != 0 || oracles_objects_add_hooks(v->guest) != 0
                || oracles_guest_add_event_listener(v->guest, on_live_event, v) != 0) {
                oracles_sprites_destroy(v->live_sprites); v->live_sprites = NULL;
                oracles_objects_destroy(v->live_objects); v->live_objects = NULL;
            }
        } else {
            oracles_sprites_destroy(v->live_sprites); v->live_sprites = NULL;
            oracles_objects_destroy(v->live_objects); v->live_objects = NULL;
        }
    }
    v->left_valid = 0;
    ev_invalidate_all(v);   /* the cached renders were made without the objects */
}

/* A ripple reads up to its largest shift past the band's own rectangle. */
static int32_t wave_reach(const OraclesEnhancedView *v)
{
    int32_t reach = 0;
    if (!v->have_wave) return 0;
    for (unsigned i = 0; i < ORACLES_ENHANCED_AREA_HEIGHT; i++) {
        const int32_t ax = v->line_shift[i] < 0 ? -v->line_shift[i] : v->line_shift[i];
        const int32_t ay = v->line_shift_y[i] < 0 ? -v->line_shift_y[i] : v->line_shift_y[i];
        if (ax > reach) reach = ax;
        if (ay > reach) reach = ay;
    }
    return reach;
}

int ev_shown_meets(const OraclesEnhancedView *v, int32_t band_left, int32_t band_top, int32_t left, int32_t top, int32_t width, int32_t height, int32_t reach)
{
    const int32_t shown_left = band_left + (int32_t)(v->size.width - v->shown_width) / 2 - reach;
    const int32_t shown_top = band_top + (int32_t)(v->band_height - v->shown_height) / 2 - reach;
    return left < shown_left + (int32_t)v->shown_width + 2 * reach && left + width > shown_left
        && top < shown_top + (int32_t)v->shown_height + 2 * reach && top + height > shown_top;
}

/* What the overlays drew beyond the part of the band shown, black again. */
static void black_outside_shown(OraclesEnhancedView *v)
{
    if (v->shown_width == v->size.width && v->shown_height == v->band_height) return;
    const unsigned x0 = (v->size.width - v->shown_width) / 2u, y0 = (v->band_height - v->shown_height) / 2u;
    const uint32_t black = v->fade_effective ? *ev_band_pixel(v, 0, 0) : 0xff000000u;   /* the compose's own, faded */
    for (unsigned y = 0; y < v->band_height; y++)
        for (unsigned x = 0; x < v->size.width; x++)
            if (x < x0 || x >= x0 + v->shown_width || y < y0 || y >= y0 + v->shown_height) *ev_band_pixel(v, x, y) = black;
}

static void add_source(OraclesEnhancedNeighbour *nb, int32_t left, int32_t top, const uint32_t *area, unsigned width, unsigned height, int faded)
{
    nb->world_left = left; nb->world_top = top; nb->game_area = area; nb->width = width; nb->height = height; nb->faded = faded;
}

/* Every cached room of the map that the view intersects, at its place on
 * the grid: the neighbours of the row, the room itself under the game's
 * window (what the window leaves behind during a scroll), and the rows above
 * and below, which the vertical camera shows.  Then the capture of the room
 * a scroll left, while the cache has no entry for it. */
static unsigned add_grid_sources(OraclesEnhancedView *v, OraclesEnhancedNeighbour *nbs, unsigned count, int32_t camera_x, int32_t camera_y)
{
    const int32_t reach = wave_reach(v);
    int plain_shown = 0;
    memset(v->drawn_places, 0, sizeof v->drawn_places);
    for (unsigned i = 0; i < v->slot_count && ev_on_map(v); i++) {
        entry *e = &v->slots[i];
        OraclesGhostDirection dir = ORACLES_DIR_UP;
        if (!e->used || !ev_entry_drawable(v, e) || e->group != v->observer.ref_group || !ev_entry_connected(v, e, &dir)) continue;
        const int32_t w = e->large ? (int32_t)LARGE_ROOM_W : SMALL_ROOM_W, h = e->large ? (int32_t)LARGE_ROOM_H : (int32_t)ORACLES_GHOST_AREA_HEIGHT;
        int32_t left, top;
        if (e->routed) {
            /* What the game would load that way, drawn where Link would
             * arrive: beside the room in play, not at the place its own
             * index gives it (the Lost Woods shows its entrance beside
             * itself). */
            ev_routed_place(v->observer.ref_room, (OraclesGhostDirection)e->routed_dir, &left, &top);
        } else if (v->observation.large_grid) {
            /* The room a scroll enters: beside the reference room, in the scroll's direction. */
            left = v->observation.world.origin_x + (dir == ORACLES_DIR_RIGHT ? w : dir == ORACLES_DIR_LEFT ? -w : 0);
            top = v->observation.world.origin_y + (dir == ORACLES_DIR_DOWN ? h : dir == ORACLES_DIR_UP ? -h : 0);
        } else {
            left = (int32_t)(e->room & 0x0fu) * w;
            top = (int32_t)(e->room >> 4u) * h;
        }
        if (!ev_shown_meets(v, camera_x, camera_y, left, top, w, h, reach)) continue;
        add_source(&nbs[count++], left, top, ev_neighbour_pixels(v, e), (unsigned)w, (unsigned)h, 1);
        if (!e->routed && !v->observation.large_grid) v->drawn_places[e->room >> 5u] |= 1u << (e->room & 31u);
        if (oracles_enhanced_capture_blank(e->bg_palettes, e->base_bg_palettes, e->palette_offset, NULL, 0)) plain_shown = 1;
    }
    if (plain_shown) v->plain_shown_frames++;
    for (unsigned d = 0; d < 4; d++) if (v->shown_slot[d] < SLOTS) v->shown[d] = 1;
    const entry *captured = ev_on_map(v) && v->source_valid ? ev_find_entry(v, v->source_group, v->source_room) : NULL;
    if (ev_source_is_scroll_source(v) && !ev_entry_drawable(v, captured))
        add_source(&nbs[count++], v->source_left, v->source_top, ev_scroll_capture(v, 0), 0, 0, 0);
    /* The image of the room left, drawn over its capture of apparition while it
     * stays in the band; dropped once it leaves the band, once Link is back in
     * it (the game recreates its objects), or on a new epoch (a warp, a load). */
    if (v->left_valid) {
        const int back_in_it = !v->observation.in_transition && v->observer.ref_group == v->left_group && v->observer.ref_room == v->left_room;
        const int out_of_band = !ev_shown_meets(v, camera_x, camera_y, v->left_left, v->left_top, SMALL_ROOM_W, (int32_t)ORACLES_GHOST_AREA_HEIGHT, reach);
        if (back_in_it || out_of_band || v->left_epoch != v->observation.epoch || v->left_pipeline != v->colours_pipeline || !ev_on_map(v)) v->left_valid = 0;
        else {
            add_source(&nbs[count++], v->left_left, v->left_top, ev_scroll_capture(v, 1), 0, 0, 0);
            if (v->left_group == v->observer.ref_group) v->drawn_places[v->left_room >> 5u] |= 1u << (v->left_room & 31u);
        }
    }
    return count;
}

/* A large room (a dungeon) off the grid: the room being left during a
 * scroll, as it stood before the scroll rewrote the map; on the first frame
 * out of a scroll, the ghost's whole room, because the game has written the
 * room's columns outside its window during the scroll's last steps but the
 * vblank that follows drains them into the VRAM (the DMA queue) and the live
 * render would show the room left beside the window for that frame; else
 * the live render of the whole room, captured for the next scroll; and the
 * room left by the last scroll, kept drawn at its place while the camera
 * still shows part of it. */
/* A room of the overworld beside the reference room, the diagonals
 * included, that the band shows but no delivered terrain covers yet: the
 * black patch the ghost has not filled, counted by frame
 * for the harness. */
static int room_waiting_in_view(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y)
{
    if (!ev_on_overworld(v) || v->observation.large_grid) return 0;
    const uint8_t ref = v->observer.ref_room;
    static const OraclesGhostDirection sides[2] = { ORACLES_DIR_LEFT, ORACLES_DIR_RIGHT }, verticals[2] = { ORACLES_DIR_UP, ORACLES_DIR_DOWN };
    /* In the zone the game routes itself there are four places to fill, each
     * beside the room in play, and no diagonal: the room a direction leads to
     * is only known once the ghost has run it. */
    if (ev_in_routed_zone(v)) {
        for (unsigned d = 0; d < 4; d++) {
            int32_t left, top;
            ev_routed_place(ref, (OraclesGhostDirection)d, &left, &top);
            if (!ev_shown_meets(v, camera_x, camera_y, left, top, SMALL_ROOM_W, (int32_t)ORACLES_GHOST_AREA_HEIGHT, 0)) continue;
            if (!ev_entry_drawable(v, ev_find_routed_entry(v, v->observer.ref_group, ref, (OraclesGhostDirection)d))) return 1;
        }
        return 0;
    }
    uint8_t rooms[8];
    unsigned count = 0;
    for (unsigned d = 0; d < 4; d++) if (ev_room_toward(v, ref, (OraclesGhostDirection)d, &rooms[count])) count++;
    for (unsigned u = 0; u < 2; u++) {
        uint8_t row;
        if (!ev_room_toward(v, ref, verticals[u], &row)) continue;
        for (unsigned s = 0; s < 2; s++) if (ev_room_toward(v, row, sides[s], &rooms[count])) count++;
    }
    for (unsigned i = 0; i < count; i++) {
        const int32_t left = (int32_t)(rooms[i] & 0x0fu) * SMALL_ROOM_W, top = (int32_t)(rooms[i] >> 4u) * (int32_t)ORACLES_GHOST_AREA_HEIGHT;
        if (!ev_shown_meets(v, camera_x, camera_y, left, top, SMALL_ROOM_W, (int32_t)ORACLES_GHOST_AREA_HEIGHT, 0)) continue;
        if (!ev_entry_drawable(v, ev_find_entry(v, v->observer.ref_group, rooms[i]))) return 1;
    }
    return 0;
}

static unsigned add_large_room_sources(OraclesEnhancedView *v, OraclesEnhancedNeighbour *nbs, unsigned count)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    if (ob->grid || !ob->large) return count;
    const int capture_is_this_room = v->large_capture_valid && v->large_capture_group == v->observer.ref_group && v->large_capture_room == v->observer.ref_room;
    if (v->large_capture_valid && !capture_is_this_room) {
        /* The reference changed (a scroll ended): the capture is the room left. */
        memcpy(v->large_prev, v->large_capture, sizeof v->large_prev);
        v->large_prev_left = v->large_capture_left; v->large_prev_top = v->large_capture_top;
        v->large_prev_epoch = v->large_capture_epoch; v->large_prev_pipeline = v->large_capture_pipeline;
        v->large_prev_valid = 1;
        v->large_capture_valid = 0;
    }
    if (ob->in_scroll && v->large_capture_valid && v->large_capture_epoch == ob->epoch && v->large_capture_pipeline == v->colours_pipeline) {
        add_source(&nbs[count++], v->large_capture_left, v->large_capture_top, v->large_capture, LARGE_ROOM_W, LARGE_ROOM_H, 0);
    } else if (!ob->in_scroll) {
        entry *entered = v->large_was_in_scroll && v->large_scroll_epoch == ob->epoch ? ev_find_entry(v, ob->group, ob->room) : NULL;
        const int from_ghost = entered && entered->valid && entered->large;
        if (from_ghost) add_source(&nbs[count++], ob->world.origin_x, ob->world.origin_y, ev_neighbour_pixels(v, entered), LARGE_ROOM_W, LARGE_ROOM_H, 1);
        else ev_render_large_room(v);
        if (!from_ghost && v->strip_valid) {
            add_source(&nbs[count++], ob->world.origin_x, ob->world.origin_y, v->strip_area, LARGE_ROOM_W, LARGE_ROOM_H, 1);
            if (!ob->in_transition) {
                memcpy(v->large_capture, v->strip_raw, sizeof v->large_capture);
                v->large_capture_left = ob->world.origin_x; v->large_capture_top = ob->world.origin_y;
                v->large_capture_epoch = ob->epoch; v->large_capture_pipeline = v->colours_pipeline;
                v->large_capture_group = v->observer.ref_group; v->large_capture_room = v->observer.ref_room;
                v->large_capture_valid = 1;
            }
        }
    }
    if (v->large_prev_valid && v->large_prev_epoch == ob->epoch && v->large_prev_pipeline == v->colours_pipeline)
        add_source(&nbs[count++], v->large_prev_left, v->large_prev_top, v->large_prev, LARGE_ROOM_W, LARGE_ROOM_H, 0);
    return count;
}

const uint32_t *oracles_enhanced_view_compose(OraclesEnhancedView *v, OraclesEnhancedMode *mode_out, int32_t *camera_x_out, int32_t *camera_y_out)
{
    int32_t camera_x = 0, camera_y = 0;
    oracles_enhanced_observe(v->guest, &v->observer, &v->observation);
    /* The interior's open edges come from the cache as it stands, for the
     * reference room this observation settled on: at the end of a scroll the
     * reference changes, and the extent must be the new room's on that very
     * frame, or the reducer would find its camera outside the bounds and
     * desynchronise for a frame (a framed frame, then a snap). */
    ev_update_reference_edges(v);
    oracles_enhanced_observation_rebound(&v->observer, &v->observation);
    ev_track_animated_tiles(v);
    ev_scroll_animation(v);
    ev_advance_neighbour_animations(v);
    v->animation_renders = 0;
    ev_track_fade(v);
    int tracking = oracles_enhanced_camera_reduce(v->camera, v->frame, &v->observation, &camera_x, &camera_y);
    /* A scroll out of a room the game routes itself toward a side off the map
     * (the Lost Woods' west, which brings Link back into the woods from the
     * east): the game's window leaves the map, where the camera does not go.
     * The band stays where it stood, the room left drawn at its place, and
     * Link walks out of it, to come back from the other side at the arrival;
     * the framed image would move the picture instead.  The drawn-back band
     * alone: it stands against the map's edge before and after the scroll
     * (three rooms across from its first column), where the camera takes it
     * up again without a jump; the normal band's camera follows Link inside
     * the room and would jump back to him. */
    uint8_t beyond_edge;
    if (!tracking && v->have_view && ev_drawn_back(v) && v->observation.in_scroll && ev_in_routed_zone(v)
        && !ev_room_toward(v, v->observer.ref_room, (OraclesGhostDirection)(v->observation.scroll_direction & 3u), &beyond_edge)) {
        camera_x = v->view_left; camera_y = v->view_top;
        tracking = 1;
    }
    oracles_enhanced_camera_shown(v->camera, &v->shown_width, &v->shown_height);
    v->frame++;
    ev_update_neighbours(v);
    /* The first frame of a transition: the room in play becomes a neighbour,
     * its animation going on from where the live game has it rather than from
     * the phase its ghost captured (the room entered, the game restarts its
     * own animation there when it differs: that phase is the game's). */
    if (v->observation.in_transition && !v->was_in_transition && v->rom && v->source_valid && !v->observation.large_grid) {
        /* The room in play by its last capture: the game has already made the
         * room entered its active one on this frame (getNextActiveRoom). */
        entry *left = ev_find_entry(v, v->source_group, v->source_room);
        if (left && left->valid) ev_hand_live_animation(v, left);
    }
    /* The first frame of a transition: the last capture of the room in play,
     * its objects in it, becomes the image of the room left (design, 6.2.2). */
    if (v->neighbour_objects && v->observation.in_transition && !v->was_in_transition && v->source_valid && !v->observation.large_grid) {
        memcpy(v->left_area, ev_source_area(v), sizeof v->left_area);
        v->left_group = v->source_group;
        v->left_room = v->source_room;
        v->left_left = v->source_left;
        v->left_top = v->source_top;
        v->left_epoch = v->source_epoch;
        v->left_pipeline = v->source_pipeline;
        memcpy(v->left_vram, v->source_vram, sizeof v->left_vram);
        v->left_regs = v->source_regs;
        memcpy(v->left_palettes, v->source_palettes, sizeof v->left_palettes);
        v->left_shown_version = 0;
        v->left_valid = 1;
    }
    /* What the live game itself showed: Link scrolled out of a room and
     * arrived somewhere.  When the room he left is one the game may route,
     * the arrival says whether it routed, and no ghost run had to ask --
     * including when the game sent him back into the room he was leaving,
     * which changes no reference and which nothing else would ever teach. */
    if (!v->observation.in_transition && v->was_in_transition && v->scroll_from_valid && v->scroll_saw_scroll
        && v->scroll_from_group == v->observer.ref_group)
        ev_note_routing(v, v->observer.ref_group, v->scroll_from_room, (OraclesGhostDirection)v->scroll_dir_seen, v->observer.ref_room);
    if (v->observation.in_scroll) { v->scroll_saw_scroll = 1; v->scroll_dir_seen = (uint8_t)(v->observation.scroll_direction & 3u); }
    /* A transition changes what the game would load from a room it routes
     * itself (the Lost Woods counts the directions Link strings together):
     * its answers are asked again, but those whose routing key holds (the
     * sequence's step unchanged, ev_check_routing_keys). */
    if (v->observation.in_transition && !v->was_in_transition) {
        ev_drop_routed_entries(v);
        v->scroll_from_valid = v->source_valid;   /* the capture of the room in play: the room being left */
        v->scroll_from_group = v->source_group;
        v->scroll_from_room = v->source_room;
        v->scroll_saw_scroll = 0;                 /* a warp is not a scroll: it teaches nothing about a direction */
    }
    v->was_in_transition = v->observation.in_transition;
    ev_capture_source_terrain(v);

    OraclesEnhancedCompose in;
    memset(&in, 0, sizeof in);
    in.border = 0xff101010u;
    in.size = v->size;
    /* The game's palette fade is in the core's window already; the rest of
     * the band takes it too, the frame's gutters included, so that a flash
     * covers the whole screen and not the room alone. */
    in.fade = v->fade_effective;
    in.colours = v->colours;
    for (unsigned d = 0; d < 4; d++) v->shown[d] = 0;
    in.mode = ev_choose_mode(v, tracking);
    OraclesEnhancedNeighbour nbs[SLOTS + 4u];   /* the cached rooms in view, the scroll's capture, a large room's three sources */
    if (in.mode == ORACLES_ENHANCED_WORLD) {
        in.window_world_left = v->observation.window_left;
        in.window_world_top = v->observation.window_top;
        in.world_left = camera_x;
        in.world_top = camera_y;
        unsigned count = add_grid_sources(v, nbs, 0, camera_x, camera_y);
        count = add_large_room_sources(v, nbs, count);
        if (room_waiting_in_view(v, camera_x, camera_y)) v->waiting_frames++;
        v->large_was_in_scroll = !v->observation.grid && v->observation.large && v->observation.in_scroll;
        v->large_scroll_epoch = v->observation.epoch;
        if (v->have_wave) { in.line_shift = v->line_shift; in.line_shift_y = v->line_shift_y; v->wave_frames++; }
        in.neighbours = nbs;
        in.neighbour_count = count;
        in.shown_width = v->shown_width;
        in.shown_height = v->shown_height;
        if (v->observation.large_grid && ev_drawn_back(v)) {
            /* The whole large room, and during a scroll the room it enters: black
             * there is waiting; around them it is decided (no neighbours shown). */
            const OraclesEnhancedObservation *ob = &v->observation;
            const unsigned d = ob->scroll_direction & 3u;
            in.counted_left = ob->world.origin_x - (ob->in_scroll && d == 3u ? (int32_t)LARGE_ROOM_W : 0);
            in.counted_top = ob->world.origin_y - (ob->in_scroll && d == 0u ? (int32_t)LARGE_ROOM_H : 0);
            in.counted_width = LARGE_ROOM_W * (ob->in_scroll && (d & 1u) ? 2u : 1u);
            in.counted_height = LARGE_ROOM_H * (ob->in_scroll && !(d & 1u) ? 2u : 1u);
        }
    } else v->large_was_in_scroll = 0;
    v->uncovered = oracles_enhanced_compose(ev_scroll_window(v), &in, v->surface);
    ev_find_area_blurb(v);
    if (in.mode == ORACLES_ENHANCED_WORLD) {
        ev_overlay_large_objects(v, camera_x, camera_y);
        ev_overlay_edge_sprites(v, camera_x, camera_y);
        ev_overlay_area_blurb(v, camera_x, camera_y);
        ev_fade_appearing(v, camera_x, camera_y);
        ev_overlay_warp_curtain(v, camera_x, camera_y);
        ev_overlay_text_box(v, camera_x, camera_y);
        black_outside_shown(v);
    }
    /* In the HUD band's gutters, whatever the mode: they exist in the framed
     * surface too, beside the status bar wherever the image stands (centred
     * in the drawn-back surface). */
    v->hud_top = in.mode == ORACLES_ENHANCED_FRAMED ? (v->size.height - ORACLES_PPU_HEIGHT) / 2u : 0u;
    ev_draw_hotbar(v);
    ev_measure_black(v, in.mode, camera_x, camera_y);
    v->have_view = in.mode == ORACLES_ENHANCED_WORLD;
    v->view_left = camera_x; v->view_top = camera_y;
    if (mode_out) *mode_out = in.mode;
    if (camera_x_out) *camera_x_out = camera_x;
    if (camera_y_out) *camera_y_out = camera_y;
    return v->surface;
}

const uint32_t *oracles_enhanced_view_frame_source(void *opaque)
{
    return oracles_enhanced_view_compose((OraclesEnhancedView *)opaque, NULL, NULL, NULL);
}

int oracles_enhanced_view_framed_crop(void *opaque, uint32_t rect[4])
{
    const OraclesEnhancedView *v = opaque;
    if (v->have_view) return 0;
    rect[0] = oracles_enhanced_hud_x(v->size);   /* as the compositor frames it */
    rect[1] = (v->size.height - ORACLES_PPU_HEIGHT) / 2u;
    rect[2] = ORACLES_ENHANCED_CORE_WIDTH;
    rect[3] = ORACLES_PPU_HEIGHT;
    return 1;
}

const uint32_t *oracles_enhanced_view_surface(const OraclesEnhancedView *v) { return v->surface; }
const OraclesEnhancedObservation *oracles_enhanced_view_observation(const OraclesEnhancedView *v) { return &v->observation; }

void oracles_enhanced_view_neighbour(const OraclesEnhancedView *v, unsigned direction, int *shown, uint8_t *room)
{
    uint8_t target = 0xff;
    if (direction > 3u || !ev_on_map(v) || !ev_room_toward(v, v->observer.ref_room, (OraclesGhostDirection)direction, &target)) target = 0xff;
    /* The room drawn that way: the grid's neighbour, or, beside a room the game
     * routes itself, the answer the ghost gave for that direction. */
    if (direction <= 3u && v->shown[direction] && v->shown_slot[direction] < v->slot_count) target = v->slots[v->shown_slot[direction]].room;
    if (shown) *shown = direction <= 3u ? v->shown[direction] : 0;
    if (room) *room = target;
}

void oracles_enhanced_view_ghost_counts(const OraclesEnhancedView *v, unsigned *requested, unsigned *completed, unsigned *failed)
{
    if (requested) *requested = v->requested;
    if (completed) *completed = v->completed;
    if (failed) *failed = v->failed;
}

void oracles_enhanced_view_ghost_failures(const OraclesEnhancedView *v, unsigned reasons[5])
{
    if (reasons) memcpy(reasons, v->failure_reasons, sizeof v->failure_reasons);
}

const char *oracles_enhanced_view_failure_log(const OraclesEnhancedView *v) { return v->failure_log; }
unsigned oracles_enhanced_view_stale_results(const OraclesEnhancedView *v) { return v->stale; }
unsigned oracles_enhanced_view_coarse_drops(const OraclesEnhancedView *v) { return v->coarse_drops; }
unsigned oracles_enhanced_view_sea_runs(const OraclesEnhancedView *v) { return v->sea_runs; }
unsigned oracles_enhanced_view_chained_results(const OraclesEnhancedView *v) { return v->chained; }
unsigned oracles_enhanced_view_refreshed_parents(const OraclesEnhancedView *v) { return v->refreshed; }

unsigned oracles_enhanced_view_drop_log(const OraclesEnhancedView *v, char *out, size_t capacity)
{
    size_t used = 0;
    if (capacity) out[0] = 0;
    for (unsigned i = 0; i < DROP_LOG && v->drop_count[i]; i++) {
        const int n = snprintf(capacity > used ? out + used : NULL, capacity > used ? capacity - used : 0, "%s%04x:%u", used ? " " : "", v->drop_addr[i], v->drop_count[i]);
        if (n < 0) break;
        used += (size_t)n;
    }
    return v->drops;
}
unsigned oracles_enhanced_view_live_renders(const OraclesEnhancedView *v) { return v->live_renders; }
unsigned oracles_enhanced_view_live_renders_kept(const OraclesEnhancedView *v) { return v->live_renders_kept; }
unsigned oracles_enhanced_view_live_lines_kept(const OraclesEnhancedView *v) { return v->live_lines_kept; }
unsigned oracles_enhanced_view_live_diff_max(const OraclesEnhancedView *v) { return v->live_diff_max; }
unsigned oracles_enhanced_view_plain_renders(const OraclesEnhancedView *v) { return v->plain_renders; }
unsigned oracles_enhanced_view_season_rejected(const OraclesEnhancedView *v) { return v->season_rejected; }
unsigned oracles_enhanced_view_beside_refused(const OraclesEnhancedView *v) { return v->beside_refused; }
const char *oracles_enhanced_view_plain_log(const OraclesEnhancedView *v) { return v->plain_log; }
int oracles_enhanced_view_isolated(const OraclesEnhancedView *v) { return ev_isolated_room(v); }
unsigned oracles_enhanced_view_uncovered(const OraclesEnhancedView *v) { return v->uncovered; }
void oracles_enhanced_view_shown(const OraclesEnhancedView *v, unsigned *width, unsigned *height) { *width = v->shown_width; *height = v->shown_height; }
unsigned oracles_enhanced_view_off_camera_frames(const OraclesEnhancedView *v) { return v->off_camera_frames; }
unsigned oracles_enhanced_view_wave_frames(const OraclesEnhancedView *v) { return v->wave_frames; }
unsigned oracles_enhanced_view_waiting_frames(const OraclesEnhancedView *v) { return v->waiting_frames; }
unsigned oracles_enhanced_view_routed_delivered(const OraclesEnhancedView *v) { return v->routed_delivered; }
unsigned oracles_enhanced_view_plain_shown_frames(const OraclesEnhancedView *v) { return v->plain_shown_frames; }

unsigned oracles_enhanced_view_objects_outdated(const OraclesEnhancedView *v) { return v->objects_outdated; }
unsigned oracles_enhanced_view_refresh_given_up(const OraclesEnhancedView *v) { return v->refresh_given_up; }
unsigned oracles_enhanced_view_parents_run_for_kills(const OraclesEnhancedView *v) { return v->parents_run_for_kills; }

void oracles_enhanced_view_large_object_counts(const OraclesEnhancedView *v, unsigned *sprites_drawn, unsigned *frames_unmatched)
{
    if (sprites_drawn) *sprites_drawn = v->large_object_sprites_drawn;
    if (frames_unmatched) *frames_unmatched = v->large_object_frames_unmatched;
}
void oracles_enhanced_view_animation_counts(const OraclesEnhancedView *v, unsigned *own, unsigned *other, unsigned *in_step_tiles) { if (own) *own = v->own_animation_copies; if (other) *other = v->other_animation_copies; if (in_step_tiles) *in_step_tiles = v->image_live_tiles; }
void oracles_enhanced_view_stream_counts(const OraclesEnhancedView *v, unsigned *matched, unsigned *followed) { if (matched) *matched = v->streams_matched; if (followed) *followed = v->streams_followed; }
int oracles_enhanced_view_blurb_drawn(const OraclesEnhancedView *v) { return v->blurb != NULL && v->blurb_mask != 0; }
void oracles_enhanced_view_blind_runs(const OraclesEnhancedView *v, unsigned *filed, unsigned *dropped)
{
    if (filed) *filed = v->blind_results;
    if (dropped) *dropped = v->blind_dropped;
}

int oracles_enhanced_view_neighbour_layout(const OraclesEnhancedView *v, unsigned direction, uint8_t *out)
{
    if (direction > 3u || !v->shown[direction] || v->shown_slot[direction] >= SLOTS) return 0;
    const entry *e = &v->slots[v->shown_slot[direction]];
    if (out) memcpy(out, e->layout, ORACLES_GHOST_LAYOUT_BYTES);
    /* A room run again for a season change deliberately shows the old season
     * until it is delivered, less abrupt than black; one run again for a killed
     * enemy shows the terrain it must show. */
    return e->refresh && e->refresh_season ? 2 : 1;
}

/* ---- savestate ------------------------------------------------------------------------- */

/* The surface a state was taken at: named by a record after the loops' when
 * it is not the normal one, so that a state of the normal surface is what it
 * was before the drawn-back view.  The camera's positions are the
 * places of that surface's corner: at another size they mean another frame. */
static void size_record(OraclesEnhancedSize size, uint8_t out[SIZE_RECORD_SIZE])
{
    memcpy(out, SIZE_RECORD_TAG, 4);
    out[4] = (uint8_t)size.width; out[5] = (uint8_t)(size.width >> 8);
    out[6] = (uint8_t)size.height; out[7] = (uint8_t)(size.height >> 8);
}

static const char *const surface_aspects[ORACLES_ENHANCED_ASPECTS] = { "16:9", "4:3" };

/* The view's level and the screen's shape that give a surface; 0 for a size of no level. */
static int surface_of(OraclesEnhancedSize size, unsigned *level, unsigned *aspect)
{
    for (unsigned l = ORACLES_ENHANCED_LEVELS; l-- > 0;)
        for (unsigned a = 0; a < ORACLES_ENHANCED_ASPECTS; a++) {
            const OraclesEnhancedSize s = oracles_enhanced_view_size((OraclesEnhancedLevel)l, (OraclesEnhancedAspect)a);
            if (s.width == size.width && s.height == size.height) { *level = l; *aspect = a; return 1; }
        }
    return 0;
}

/* A surface as the player chose it: the view's level and the screen's shape that give it. */
static void surface_words(OraclesEnhancedSize size, char *out, size_t capacity)
{
    static const char *const levels[ORACLES_ENHANCED_LEVELS] = { "near", "medium", "far" };
    unsigned l, a;
    if (surface_of(size, &l, &a)) snprintf(out, capacity, "the %s view, %ux%u in %s", levels[l], size.width, size.height, surface_aspects[a]);
    else snprintf(out, capacity, "a surface of %ux%u", size.width, size.height);
}

int oracles_enhanced_view_check_state(const OraclesEnhancedView *v, const uint8_t *data, size_t size, char *why, size_t capacity)
{
    OraclesEnhancedSize saved = oracles_enhanced_size(0);
    const size_t at = 2u * ORACLES_E11_STATE_WIRE_SIZE + LOOP_RECORD_SIZE;
    if (size == at + SIZE_RECORD_SIZE && memcmp(data + at, SIZE_RECORD_TAG, 4) == 0) {
        saved.width = data[at + 4] | (unsigned)data[at + 5] << 8;
        saved.height = data[at + 6] | (unsigned)data[at + 7] << 8;
    }
    if (saved.width == v->size.width && saved.height == v->size.height) return 0;
    if (why && capacity) {
        char taken[96], shown[96];
        surface_words(saved, taken, sizeof taken);
        surface_words(v->size, shown, sizeof shown);
        /* The same level in the other shape (a far savestate of a 4:3 screen taken before far had its 4:3 size): the
         * setting that shows the surface it was taken on. */
        unsigned saved_level, saved_aspect, level, aspect;
        char hint[64] = "";
        if (surface_of(saved, &saved_level, &saved_aspect) && surface_of(v->size, &level, &aspect) && saved_level == level)
            snprintf(hint, sizeof hint, "; aspect=%s in the settings shows that surface", surface_aspects[saved_aspect]);
        snprintf(why, capacity, "the savestate was taken on %s, and this session shows %s: it loads only on its own surface%s", taken, shown, hint);
    }
    return -1;
}

/* The host state: the horizontal reducer's record, then the vertical one. */
int oracles_enhanced_view_save_state(const OraclesEnhancedView *v, uint8_t *out, size_t capacity, size_t *written)
{
    size_t first = 0, second = 0;
    if (oracles_e11_state_serialize(oracles_enhanced_camera_state(v->camera), out, capacity, &first) != ORACLES_E11_OK) return -1;
    if (oracles_e11_state_serialize(oracles_enhanced_camera_state_vertical(v->camera), out + first, capacity - first, &second) != ORACLES_E11_OK) return -1;
    /* The loops the epoch went through (camera.h): the camera's positions are in the world they shifted. */
    if (capacity - first - second < LOOP_RECORD_SIZE) return -1;
    uint8_t *loop = out + first + second;
    memcpy(loop, LOOP_RECORD_TAG, 4);
    for (unsigned b = 0; b < 4u; b++) { loop[4 + b] = (uint8_t)((uint32_t)v->observer.loop_cols >> (8u * b)); loop[8 + b] = (uint8_t)((uint32_t)v->observer.loop_rows >> (8u * b)); }
    size_t total = first + second + LOOP_RECORD_SIZE;
    if (v->size.width != ORACLES_ENHANCED_NARROW_WIDTH || v->size.height != ORACLES_ENHANCED_NARROW_HEIGHT) {
        if (capacity - total < SIZE_RECORD_SIZE) return -1;
        size_record(v->size, out + total);
        total += SIZE_RECORD_SIZE;
    }
    if (written) *written = total;
    return 0;
}

int oracles_enhanced_view_load_state(OraclesEnhancedView *v, const uint8_t *data, size_t size)
{
    /* The wire state names its configuration by digest: a state saved under
     * the other profile resumes under that profile, the savestate wins. */
    OraclesE11State state, vertical;
    unsigned profile = oracles_enhanced_camera_profile(v->camera);
    /* One record (a state saved before the vertical reducer), two, or two and
     * the loops, and the surface's size when it is not the normal one. */
    if (oracles_enhanced_view_check_state(v, data, size, NULL, 0) != 0) return -1;
    const size_t loops_end = 2u * ORACLES_E11_STATE_WIRE_SIZE + LOOP_RECORD_SIZE;
    const int have_loops = (size == loops_end || size == loops_end + SIZE_RECORD_SIZE) && memcmp(data + 2u * ORACLES_E11_STATE_WIRE_SIZE, LOOP_RECORD_TAG, 4) == 0;
    if (size != ORACLES_E11_STATE_WIRE_SIZE && size != 2u * ORACLES_E11_STATE_WIRE_SIZE && !have_loops) return -1;
    OraclesE11Config config = *oracles_enhanced_camera_config(v->camera);
    if (oracles_e11_state_restore(data, ORACLES_E11_STATE_WIRE_SIZE, &config, &state) != ORACLES_E11_OK) {
        profile = profile == 2u ? 1u : 2u;
        oracles_e11_config_profile(&config, profile);
        if (oracles_e11_state_restore(data, ORACLES_E11_STATE_WIRE_SIZE, &config, &state) != ORACLES_E11_OK) return -1;
        oracles_enhanced_camera_set_profile(v->camera, profile);
    }
    const int have_vertical = size >= 2u * ORACLES_E11_STATE_WIRE_SIZE
        && oracles_e11_state_restore(data + ORACLES_E11_STATE_WIRE_SIZE, ORACLES_E11_STATE_WIRE_SIZE, &config, &vertical) == ORACLES_E11_OK;
    oracles_enhanced_camera_set_state(v->camera, &state, have_vertical ? &vertical : NULL);
    /* The ordinals continue from the saved state: the camera goes on from
     * where it was, no re-bootstrap; the observer resumes at the saved epoch,
     * or the reducer would ignore every observation of a lower one, and its
     * reference room is rebuilt from the guest at the next frame. */
    if (state.last_ordinal_present) v->frame = (uint32_t)(state.last_ordinal + 1u);
    const unsigned map_width = v->observer.map_width, band_width = v->observer.band_width;
    memset(&v->observer, 0, sizeof v->observer);
    v->observer.map_width = map_width;
    v->observer.band_width = band_width;
    v->observer.epoch = state.epoch / 8u;
    /* What the view remembered of the frames before the load is about
     * another place: the room a scroll came from (an unrelated room would be
     * drawn as connected), the captures, the reference room's edges and
     * object, the pre-runs of a load, the sprite bookkeeping. */
    v->have_last_ref = 0; v->have_came_from = 0; v->have_last_scroll_dir = 0;
    v->source_valid = 0; v->large_capture_valid = 0; v->large_prev_valid = 0; v->large_was_in_scroll = 0;
    v->have_ref_collisions = 0; v->ref_maku = 0;
    v->blind_mask = 0; v->blind_epoch = 0;
    v->have_live_tiles_prev = 0; v->have_wave = 0; v->blurb_seen = 0; v->blurb = NULL; v->blurb_mask = 0;
    ev_scroll_reset(v);   /* a scroll's animation (view_scroll.c): the one under way stays frozen, as the state loaded has it */
    v->strip_valid = 0;
    /* The objects' bookkeeping: the image of the room left,
     * the fade and the hand-off of an entry, the live numbering and tags,
     * a large room's drawings. */
    v->left_valid = 0;
    v->appear_active = 0; v->appear_ready = 0; v->appear_ended = 0;
    v->handoff_count = 0; v->handoff_armed = 0;
    oracles_sprites_forget(v->live_sprites);
    oracles_objects_forget(v->live_objects);
    for (unsigned i = 0; i < LARGE_OBJECT_FRAMES; i++) v->large_objects[i].valid = 0;
    if (have_loops) {
        const uint8_t *loop = data + 2u * ORACLES_E11_STATE_WIRE_SIZE;
        uint32_t cols = 0, rows = 0;
        for (unsigned b = 0; b < 4u; b++) { cols |= (uint32_t)loop[4 + b] << (8u * b); rows |= (uint32_t)loop[8 + b] << (8u * b); }
        v->observer.loop_cols = (int32_t)cols; v->observer.loop_rows = (int32_t)rows; v->observer.loop_epoch = v->observer.epoch;
    }
    ev_invalidate_all(v);
    v->coarse_len = 0;   /* the coarse list is taken again from the state loaded */
    return 0;
}

OraclesEnhancedCamera *oracles_enhanced_view_camera(OraclesEnhancedView *v) { return v->camera; }
