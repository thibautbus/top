#include "view_internal.h"

/* ---- start / stop ---------------------------------------------------------------------- */

static void on_view_event(void *opaque, const OraclesGuestEvent *event)
{
    ev_large_objects_event((OraclesEnhancedView *)opaque, event);
    ev_blurb_event((OraclesEnhancedView *)opaque, event);
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
    /* Only the levels' sizes and the far view's fitted to a screen (compositor.h): the view, the camera and the hotbar are made for those. */
    if (!oracles_enhanced_size_known(size, NULL)) return;
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
        oracles_ghost_set_probe(lane->ghost, !v->poses_off);
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
    v->left_valid = 0; v->vanish_active = 0;
    ev_invalidate_all(v);   /* the cached renders were made without the objects */
}

/* The waves under Ages' sea, from the game's own sine (w2WaveScrollValues, where the profile binds it): the
 * window's line i takes the step SCY + the frame counter + i, as checkUpdateUnderwaterWaves counts it, the wave held
 * to the room's rows.  The game's screen shows them filled for the next frame from the registers of the one before,
 * torn where its camera moves: a jolt of its water under a moving screen, a shake of the room the band shows still.
 * 1 with the waves in `out`, 0 without the table. */
static int room_waves(OraclesEnhancedView *v, int16_t out[ORACLES_ENHANCED_AREA_HEIGHT])
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const uint8_t *table = oracles_guest_ptr(v->guest, t->wave_scroll_values, ORACLES_ENHANCED_AREA_HEIGHT);
    if (!table) return 0;
    const OraclesEnhancedObservation *ob = &v->observation;
    const unsigned phase = (unsigned)(ob->drawn_camera_y + ob->drawn_offset_y - (int)ORACLES_ENHANCED_HUD_HEIGHT) + oracles_guest_read8(v->guest, t->frame_counter);
    for (unsigned i = 0; i < ORACLES_ENHANCED_AREA_HEIGHT; i++) out[i] = (int8_t)table[(phase + i) & (ORACLES_ENHANCED_AREA_HEIGHT - 1u)];
    return 1;
}

/* Ages' sea floor and its houses: the tileset under water. */
static int ob_underwater(const OraclesEnhancedView *v)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const uint8_t *flags = oracles_guest_ptr(v->guest, t->tileset_flags, 1);
    return flags && (*flags & ORACLES_TILESETFLAG_UNDERWATER);
}

/* Without the table, the waves' part common to the room's lines, the median of their scroll across over the lines its
 * rows show (the window's, or a taller screen's, its 128 lines repeating): the greater part of its edge where it
 * stands. */
static int32_t wave_sway(const OraclesEnhancedView *v, int32_t world_top)
{
    const OraclesEnhancedWorld *w = &v->observation.world;
    int16_t sorted[2u * ORACLES_ENHANCED_AREA_HEIGHT];
    unsigned n = 0;
    for (int32_t y = w->bounds_origin_y; y < w->bounds_origin_y + w->bounds_height && n < sizeof sorted / sizeof sorted[0]; y++) {
        if (y < world_top || y >= world_top + (int32_t)v->band_height) continue;
        sorted[n++] = v->line_shift[(uint32_t)(y - v->observation.window_top) % ORACLES_ENHANCED_AREA_HEIGHT];
    }
    if (!n) return 0;
    for (unsigned i = 1; i < n; i++)
        for (unsigned j = i; j > 0 && sorted[j - 1] > sorted[j]; j--) { const int16_t t = sorted[j]; sorted[j] = sorted[j - 1]; sorted[j - 1] = t; }
    return sorted[n / 2u];
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
    nb->world_left = left; nb->world_top = top; nb->game_area = area; nb->width = width; nb->height = height; nb->faded = faded; nb->veil = 0; nb->edge_veil = 0;
}

/* Every cached room of the map that the view intersects, at its place on
 * the grid: the neighbours of the row, the room itself under the game's
 * window (what the window leaves behind during a scroll), and the rows above
 * and below, which the vertical camera shows; a room never visited dark, as
 * the places of the grid no entry covers yet that hold one.  Then the capture
 * of the room a scroll left, while the cache has no entry for it. */
static unsigned add_grid_sources(OraclesEnhancedView *v, OraclesEnhancedNeighbour *nbs, unsigned count, int32_t camera_x, int32_t camera_y)
{
    const int32_t reach = wave_reach(v);
    int plain_shown = 0;
    memset(v->drawn_places, 0, sizeof v->drawn_places);
    ev_hidden_begin(v);
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
        nbs[count - 1u].veil = ev_room_veil(v, e->room);
        if (nbs[count - 1u].veil >= 256u) nbs[count - 1u].edge_veil = ev_room_edge_veil(v, e->room);
        if (!e->routed && !v->observation.large_grid) v->drawn_places[e->room >> 5u] |= 1u << (e->room & 31u);
        if (nbs[count - 1u].veil < 256u && oracles_enhanced_capture_blank(e->bg_palettes, e->base_bg_palettes, e->palette_offset, NULL, 0)) plain_shown = 1;
    }
    if (plain_shown) v->plain_shown_frames++;
    for (unsigned d = 0; d < 4; d++)
        if (v->shown_slot[d] < SLOTS && !ev_room_hidden(v, v->slots[v->shown_slot[d]].group, v->slots[v->shown_slot[d]].room)) v->shown[d] = 1;
    const entry *captured = ev_on_map(v) && v->source_valid ? ev_find_entry(v, v->source_group, v->source_room) : NULL;
    if (ev_source_is_scroll_source(v) && !ev_entry_drawable(v, captured)) {
        add_source(&nbs[count++], v->source_left, v->source_top, ev_scroll_capture(v, 0), 0, 0, 0);
        if (v->source_group == v->observer.ref_group && !v->observation.large_grid) v->drawn_places[v->source_room >> 5u] |= 1u << (v->source_room & 31u);
    }
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
    /* After the captures, which show their rooms where the grid's places would be dark. */
    if (ev_on_map(v)) count = ev_add_hidden_places(v, nbs, count, SLOTS, camera_x, camera_y, reach);
    ev_hidden_end(v);
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
        if (ev_room_hidden(v, v->observer.ref_group, rooms[i])) continue;   /* dark as decided */
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
    v->observer.exact = oracles_enhanced_view_subpixel_on(v);
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
    ev_track_scene_tint(v);
    ev_phase_observe(v);
    int32_t dark[4];
    ev_dark_limits(v, dark);
    oracles_enhanced_camera_limit(v->camera, dark);
    int tracking = oracles_enhanced_camera_reduce(v->camera, v->frame, &v->observation, &camera_x, &camera_y);
    const int reducer_tracking = tracking;   /* the camera the reducer's own, its fraction the sub-pixel camera's */
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
    ev_hold_camera(v, &camera_x, &camera_y);
    v->frame++;
    ev_check_reference_visited(v);
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
    if (v->neighbour_objects && v->observation.in_transition && !v->was_in_transition && v->source_valid && !v->observation.large_grid)
        ev_keep_left_image(v, camera_x, camera_y);
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
        /* The objects of the room entered in the poses the game freezes them in during the scroll; the game has
         * already made it its active room on this frame (getNextActiveRoom). */
        const OraclesGuestTables *t = oracles_guest_tables(v->guest);
        ev_poses_reset(v, ev_find_entry(v, oracles_guest_read8(v->guest, t->active_group), oracles_guest_read8(v->guest, t->active_room)));
        v->scroll_from_valid = v->source_valid;   /* the capture of the room in play: the room being left */
        v->scroll_from_group = v->source_group;
        v->scroll_from_room = v->source_room;
        v->scroll_saw_scroll = 0;                 /* a warp is not a scroll: it teaches nothing about a direction */
    }
    v->was_in_transition = v->observation.in_transition;
    ev_capture_source_terrain(v);

    OraclesEnhancedCompose in;
    memset(&in, 0, sizeof in);
    in.border = BORDER_COLOUR;
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
        v->wave_held = 0;
        /* Under Ages' sea the game stops its waves for a scroll between two rooms (checkDisableUnderwaterWaves, until
         * the room entered is loaded): the band goes on drawing them through it, with the game's own sine and phase
         * (room_waves), which the game takes up again where they stand, the neighbours rippling with them. */
        const int continued = !v->have_wave && ob_underwater(v) && v->observation.playing && !v->observation.cutscene
            && (v->observation.in_scroll || v->waves_continued) && room_waves(v, v->held_shift);
        v->waves_continued = continued;
        if (continued) {
            memset(v->line_shift, 0, sizeof v->line_shift);
            memset(v->line_shift_y, 0, sizeof v->line_shift_y);
            in.line_shift = v->held_shift; in.line_shift_y = v->line_shift_y;
            v->wave_held = 1;
            v->wave_frames++; v->continued_wave_frames++;
        }
        if (v->have_wave) {
            in.line_shift = v->line_shift; in.line_shift_y = v->line_shift_y; v->wave_frames++;
            /* Ages' waves under the sea are drawn from the game's sine (room_waves), held to the world's rows: the game's
             * own lines, filled from the registers of the frame before and torn where its camera moves, would crawl
             * and jolt over a room the band shows still while that camera moves inside it (a room shown whole, a
             * dungeon's large room).  Elsewhere, or without the table, a room shown whole keeps the game's lines,
             * their common part taken off (wave_sway): another effect's lines there (a split of the screen, a warp's
             * ripple) are not that sine.  The core's image is drawn again so. */
            const int whole = ev_room_whole(v), underwater = ob_underwater(v);
            if (v->wave_lines && (whole || underwater)) {
                if (underwater && room_waves(v, v->held_shift)) { in.line_shift = v->held_shift; v->wave_held = 1; }
                else if (whole) { in.sway = wave_sway(v, camera_y); v->wave_held = 1; }
                if (v->wave_held) v->sway_frames++;
            }
        }
        v->sway = in.sway;
        v->shown_shift = in.line_shift;
        in.neighbours = nbs;
        in.neighbour_count = count;
        in.soft_edges = !v->unvisited_hard;
        in.edge_seed = v->observer.ref_group;
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
    const int load_band = ev_load_band(v, &in, tracking, camera_x, camera_y);
    in.hud_fill = ev_hud_fill(v, in.mode, 1);
    in.hud_place = ev_hud_place(v, in.mode, 1);
    if (in.mode != ORACLES_ENHANCED_WORLD) { v->sway = 0; v->wave_held = 0; }
    v->sub_x = v->sub_y = 0;
    v->sub_layer_w = v->sub_layer_h = 0;
    v->sub_fixed_count = 0;
    v->uncovered_edges = 0;
    in.uncovered_edges = &v->uncovered_edges;
    v->uncovered = oracles_enhanced_compose(in.mode == ORACLES_ENHANCED_WORLD && v->wave_held ? ev_sway_window(v, ev_scroll_window(v)) : ev_scroll_window(v), &in, v->surface);
    const int dark_read = v->dark_check && v->hidden_count && in.mode == ORACLES_ENHANCED_WORLD && !load_band;
    v->dark_lit = dark_read ? oracles_enhanced_dark_lit(&in, v->surface) : 0u;
    v->dark_stray = dark_read ? oracles_enhanced_dark_stray(&in, v->surface) : 0u;
    if (load_band) { in.mode = ORACLES_ENHANCED_FRAMED; v->uncovered = 0; v->uncovered_edges = 0; }
    v->wide_shown = v->scaled_shown = v->enlarged_noted = 0;
    ev_find_area_blurb(v);
    if (in.mode == ORACLES_ENHANCED_WORLD) {
        ev_overlay_link_phase(v, camera_x, camera_y);
        ev_hold_note(v, camera_x, camera_y);
        ev_subpixel_layer(v, reducer_tracking, camera_x, camera_y);
        ev_fade_vanishing(v, camera_x, camera_y);
        ev_overlay_large_objects(v, camera_x, camera_y);
        ev_overlay_edge_sprites(v, camera_x, camera_y);
        ev_overlay_area_blurb(v, camera_x, camera_y);
        ev_fade_appearing(v, camera_x, camera_y);
        ev_overlay_warp_curtain(v, camera_x, camera_y);
        ev_overlay_text_box(v, camera_x, camera_y);
        black_outside_shown(v);
        ev_overlay_text_box_above(v);
        ev_subpixel_settle(v);
    } else if (!load_band) ev_overlay_text_box_framed(v);
    /* In the HUD band's gutters, whatever the mode: they exist in the framed
     * surface too, beside the status bar wherever the image stands (centred
     * in the drawn-back surface). */
    v->hud_top = in.mode == ORACLES_ENHANCED_FRAMED && !load_band ? (v->size.height - ORACLES_PPU_HEIGHT) / 2u : 0u;
    v->hotbar_framed = in.mode == ORACLES_ENHANCED_FRAMED && !load_band;
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
    if (v->have_view || v->framed_load) return 0;
    rect[0] = oracles_enhanced_hud_x(v->size);   /* as the compositor frames it */
    rect[1] = (v->size.height - ORACLES_PPU_HEIGHT) / 2u;
    rect[2] = ORACLES_ENHANCED_CORE_WIDTH;
    rect[3] = ORACLES_PPU_HEIGHT;
    /* With the host's wide box, the union of the frame and the box at its size, centred on it, whether a box shows or
     * not: the same scale throughout, whatever the Text size, which the presenter applies within it. */
    const unsigned box = v->wide_pixels ? v->wide_pixels + 16u : 8u * ORACLES_TEXT_BOX_GAME_COLUMNS;
    if (box > rect[2] && rect[0] >= (box - rect[2]) / 2u) {
        rect[0] -= (box - rect[2]) / 2u;
        rect[2] = box;
    }
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
unsigned oracles_enhanced_view_render_wait_max(const OraclesEnhancedView *v) { return v->render_wait_max; }
unsigned oracles_enhanced_view_renders_forced(const OraclesEnhancedView *v) { return v->renders_forced; }
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
unsigned oracles_enhanced_view_world_off_camera_frames(const OraclesEnhancedView *v) { return v->world_off_camera_frames; }
unsigned oracles_enhanced_view_wave_frames(const OraclesEnhancedView *v) { return v->wave_frames; }
unsigned oracles_enhanced_view_sway_frames(const OraclesEnhancedView *v) { return v->sway_frames; }
unsigned oracles_enhanced_view_continued_wave_frames(const OraclesEnhancedView *v) { return v->continued_wave_frames; }
int oracles_enhanced_view_room_whole(const OraclesEnhancedView *v) { return ev_room_whole(v); }
unsigned oracles_enhanced_view_shake_frames(const OraclesEnhancedView *v) { return v->shake_frames; }
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
int oracles_enhanced_view_blurb_drawn(const OraclesEnhancedView *v) { return v->blurb_shown && v->blurb_mask != 0; }
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

OraclesEnhancedCamera *oracles_enhanced_view_camera(OraclesEnhancedView *v) { return v->camera; }

const char *oracles_enhanced_view_degraded(const OraclesEnhancedView *v, char *out, size_t capacity)
{
    if (!v || !out || !capacity) return NULL;
    out[0] = 0;
    if (!v->large_objects_listening) snprintf(out, capacity, "a large room's objects not followed as the game skips their drawing");
    if (!v->animation_steps_counted) {
        const size_t used = strlen(out);
        snprintf(out + used, capacity - used, "%sthe neighbours' tile animations stepped once a composition", used ? "; " : "");
    }
    if (v->hotbar_shown && !v->hotbar_hooked) {
        const size_t used = strlen(out);
        snprintf(out + used, capacity - used, "%sthe hotbar's icons not told when the bar reloads its own", used ? "; " : "");
    }
    return out[0] ? out : NULL;
}
