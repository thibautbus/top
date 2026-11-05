#include "enhanced_check_internal.h"
#include "text_box_enlarge.h"

/* The monotonic clock in microseconds: a composition is timed by the wall, not
 * by the CPU time of the process, which counts the ghost's thread as well. */
double ec_monotonic_us(void)
{
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1e6 / (double)frequency.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
#endif
}

OraclesEnhancedCheck *oracles_enhanced_check_start(OraclesCore *core, OraclesGuest *guest,
                                                   const uint8_t *rom, size_t rom_size, const char *dir, unsigned ghost_budget)
{
    OraclesEnhancedCheck *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->core = core;
    c->guest = guest;
    c->view = oracles_enhanced_view_start(core, guest, rom, rom_size);
    if (!c->view) { free(c); return NULL; }
    oracles_enhanced_view_set_sync_budget(c->view, ghost_budget);
    oracles_enhanced_view_pose_verify(c->view, 1);   /* each render of a neighbour's poses against the step and a whole render */
    oracles_enhanced_view_check_dark(c->view, 1);    /* each room kept dark past its edge */
    c->threaded = ghost_budget == 0;
    c->run_hash = ORACLES_HASH_SEED;
    c->width = oracles_enhanced_view_width(c->view);
    c->band_height = oracles_enhanced_view_height(c->view) - ORACLES_ENHANCED_HUD_HEIGHT;
    c->link_edge_margin_min = (int32_t)c->width;   /* no world frame measured yet */
    /* Link's own entries of the OAM on screen; without the tagging's hooks, no measure rather than a wrong one. */
    c->link_sprites = oracles_sprites_create(guest);
    if (c->link_sprites && oracles_sprites_add_hooks(guest) != 0) { oracles_sprites_destroy(c->link_sprites); c->link_sprites = NULL; }
    char path[4096];
    snprintf(path, sizeof path, "%s/frames.tsv", dir);
    c->tsv = fopen(path, "w");
    if (c->tsv) fprintf(c->tsv, "frame\thash\tmode\tcamera_x\tgroup\troom\tlink_x\tlink_y\tscroll_mode\ttransition_state\tlink_world_x\twindow_left\tepoch\tleft\tright\tuncovered\tdrawn_scx\tdrawn_scy\thcamera_x\thcamera_y\tscreen_offset_x\tscreen_offset_y\toam_used\toam0_y\toam0_x\tlcdc\tcamera_y\tlink_world_y\twindow_top\tmenu\tfade\tisolated\tlarge\torigin_x\torigin_y\twindow_luma\tring_luma\n");
    return c;
}

void oracles_enhanced_check_reload_at(OraclesEnhancedCheck *c, uint32_t frame) { c->reload_at = frame; }
void oracles_enhanced_check_set_transition(OraclesEnhancedCheck *c, OraclesRoomTransition *transition) { c->transition = transition; }
void oracles_enhanced_check_set_paced(OraclesEnhancedCheck *c, int paced) { c->paced = paced; }

void oracles_enhanced_check_set_zoom_out(OraclesEnhancedCheck *c, int enabled) { oracles_enhanced_check_set_size(c, oracles_enhanced_size(enabled)); }

void oracles_enhanced_check_set_size(OraclesEnhancedCheck *c, OraclesEnhancedSize size)
{
    oracles_enhanced_view_set_size(c->view, size);
    c->width = oracles_enhanced_view_width(c->view);
    c->band_height = oracles_enhanced_view_height(c->view) - ORACLES_ENHANCED_HUD_HEIGHT;
    c->link_edge_margin_min = (int32_t)c->width;
}

void oracles_enhanced_check_surface_at(OraclesEnhancedCheck *c, uint32_t frame, const char *path)
{
    if (c->surfaces >= sizeof c->surface_at / sizeof c->surface_at[0]) return;
    c->surface_at[c->surfaces] = frame;
    c->surface_path[c->surfaces++] = path;
}

struct OraclesEnhancedView *oracles_enhanced_check_view(OraclesEnhancedCheck *c) { return (struct OraclesEnhancedView *)c->view; }

/* The surface written as a PPM `surface_scale` times as large, as the presenter shows it on a screen of that size: the
 * whole surface at that scale, or with the large menus over the framed core the part shown alone
 * (oracles_enhanced_view_framed_crop), widened for the box where its own whole scale holds it (oracles_text_box_reserve),
 * at that scale, centred, black around; the text box the presenter enlarges drawn over it larger, at its size or the
 * next down where the part shown holds it (oracles_text_box_enlarge). */
static void write_surface(const OraclesEnhancedCheck *c, const uint32_t *surface, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    const unsigned height = oracles_enhanced_view_height(c->view), k = c->surface_scale > 1u ? c->surface_scale : 1u, w = c->width * k, h = height * k;
    uint32_t part[4] = { 0, 0, c->width, height }, rect[4], reserve = 0;
    int bottom = 0;
    int32_t out[4] = { 0, 0, 0, 0 };
    unsigned factor = 1, size = 0, scale = k;
    const int published = oracles_enhanced_view_text_box_enlarged(c->view, rect, &bottom, &reserve, &size);
    if (c->large_menus && oracles_enhanced_view_framed_crop(c->view, part)) {
        oracles_text_box_reserve(c->width, w, h, 0, reserve, size, &part[0], &part[2], part[3]);
        const double whole = oracles_text_box_part_scale(part[2], part[3], w, h, 0);
        scale = whole >= 1.0 ? (unsigned)whole : 1u;
    }
    const int32_t ox = ((int32_t)w - (int32_t)(part[2] * scale)) / 2, oy = ((int32_t)h - (int32_t)(part[3] * scale)) / 2;
    const int box = published && rect[0] >= part[0] && rect[1] >= part[1] && rect[0] + rect[2] <= part[0] + part[2] && rect[1] + rect[3] <= part[1] + part[3]
                    && oracles_text_box_enlarge((double)ox, (double)oy, (double)(part[2] * scale), (double)(part[3] * scale), (double)scale, rect[0] - part[0],
                                                rect[1] - part[1], rect[2], rect[3], size, bottom, out, &factor);
    fprintf(f, "P6\n%u %u\n255\n", w, h);
    for (unsigned y = 0; y < h; y++)
        for (unsigned x = 0; x < w; x++) {
            const int32_t px = (int32_t)x - ox, py = (int32_t)y - oy;
            uint32_t p = 0xff000000u;
            if (px >= 0 && py >= 0 && px < (int32_t)(part[2] * scale) && py < (int32_t)(part[3] * scale))
                p = surface[(part[1] + (unsigned)py / scale) * c->width + part[0] + (unsigned)px / scale];
            if (box && (int32_t)x >= out[0] && (int32_t)x < out[0] + out[2] && (int32_t)y >= out[1] && (int32_t)y < out[1] + out[3])
                p = surface[(rect[1] + (unsigned)((int32_t)y - out[1]) / factor) * c->width + rect[0] + (unsigned)((int32_t)x - out[0]) / factor];
            const uint8_t rgb[3] = { (uint8_t)(p >> 16), (uint8_t)(p >> 8), (uint8_t)p };
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
}
void oracles_enhanced_check_set_neighbour_objects(OraclesEnhancedCheck *c, int enabled) { oracles_enhanced_view_set_neighbour_objects(c->view, enabled); }
void oracles_enhanced_check_set_link_phase(OraclesEnhancedCheck *c, int enabled) { oracles_enhanced_view_set_link_phase(c->view, enabled); }
void oracles_enhanced_check_set_subpixel(OraclesEnhancedCheck *c, int enabled) { oracles_enhanced_view_set_subpixel(c->view, enabled); c->smooth = enabled != 0; }

void oracles_enhanced_check_subpixel_switch_at(OraclesEnhancedCheck *c, const uint32_t *frames, unsigned count)
{
    c->subpixel_switches = count < 8u ? count : 8u;
    memcpy(c->subpixel_switch_at, frames, c->subpixel_switches * sizeof *frames);
}
void oracles_enhanced_check_set_text_box(OraclesEnhancedCheck *c, int enabled) { c->host_box = enabled != 0; }
void oracles_enhanced_check_set_text_size(OraclesEnhancedCheck *c, unsigned size)
{
    oracles_enhanced_view_set_text_size(c->view, size);
    c->text_size = size;
}
void oracles_enhanced_check_set_surface_scale(OraclesEnhancedCheck *c, unsigned scale) { c->surface_scale = scale; }
void oracles_enhanced_check_set_large_menus(OraclesEnhancedCheck *c, int large)
{
    oracles_enhanced_view_set_large_menus(c->view, large);
    c->large_menus = large != 0;
}
void oracles_enhanced_check_set_wide_text_box(OraclesEnhancedCheck *c, unsigned pixels, unsigned lines)
{
    oracles_enhanced_view_set_text_box(c->view, pixels, lines);
    c->wide = 1;
    c->wide_pixels_width = pixels;
    c->wide_lines = lines;
}
void oracles_enhanced_check_set_camera_profile(OraclesEnhancedCheck *c, unsigned profile)
{
    oracles_enhanced_view_set_camera_profile(c->view, profile);
    c->camera_profile = profile;
}

void oracles_enhanced_check_stop(OraclesEnhancedCheck *c)
{
    if (!c) return;
    if (c->tsv) fclose(c->tsv);
    free(c->saved_core);
    free(c->redraw_copy);
    oracles_sprites_destroy(c->link_sprites);
    if (c->witness) oracles_ghost_destroy(c->witness);
    free(c->witness_result);
    free(c->witness_state);
    oracles_enhanced_view_stop(c->view);
    free(c);
}

int oracles_enhanced_check_reloaded_at(const OraclesEnhancedCheck *c, uint32_t frame) { return c && c->reload_done && c->reload_frame == frame; }

/* The pixels judged against the core's image, the window's place, and the
 * frame's row: only for a run that writes its rows. */
static void check_pixels_and_row(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode,
                                 const uint32_t *surface, int32_t camera_x, int32_t camera_y, uint32_t frame, uint64_t hash,
                                 const int shown[4], const uint8_t room[4], unsigned uncovered)
{
    if (!c->tsv) return;
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    int scx, scy;
    ec_replay_scroll_registers(c, &scx, &scy);
    const int off_x = oracles_guest_read8(c->guest, t->screen_offset_x), off_y = oracles_guest_read8(c->guest, t->screen_offset_y);
    static uint8_t box[ORACLES_ENHANCED_AREA_HEIGHT][ORACLES_ENHANCED_CORE_WIDTH];
    if (mode == ORACLES_ENHANCED_WORLD && surface) {
        if (ec_text_box_pixels(c, ob, box)) ec_check_text_box(c, ob, surface, camera_x, camera_y, box, frame);
    } else memset(box, 0, sizeof box);
    if (mode == ORACLES_ENHANCED_WORLD && surface && !oracles_enhanced_view_blurb_drawn(c->view)) ec_check_window_lines(c, ob, surface, camera_x, camera_y, box, frame);
    if (mode == ORACLES_ENHANCED_WORLD && surface) ec_check_curtain(c, ob, surface, camera_x, camera_y, frame);
    const unsigned waves = oracles_enhanced_view_wave_frames(c->view);
    const int rippled = waves != c->last_wave_frames;
    c->last_wave_frames = waves;
    ec_check_window_place(c, ob, mode, rippled, scx, scy, frame);
    c->last_offset_x = off_x; c->last_offset_y = off_y; c->have_last_offset = 1;
    ec_write_frame_row(c, frame, hash, mode, camera_x, camera_y, shown[3], room[3], shown[1], room[1], uncovered, scx, scy, off_x, off_y);
}

/* The visited bit of a room's flags, its page read from the game family's flagLocationGroupTable as the disassembly
 * has it (bank0.s), not from the profile's own table the view reads through. */
static int room_visited(OraclesEnhancedCheck *c, uint8_t group, uint8_t room)
{
    static const uint8_t ages_pages[8] = { 0, 1, 0, 1, 2, 3, 2, 3 }, seasons_pages[8] = { 0, 1, 1, 1, 2, 3, 2, 3 };
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    const uint8_t *pages = oracles_compat_family(oracles_guest_profile(c->guest)) == ORACLES_GAME_SEASONS ? seasons_pages : ages_pages;
    const OraclesGuestSym flags = { t->group0_room_flags.bank, (uint16_t)(t->group0_room_flags.addr + pages[group & 7u] * 0x100u + room) };
    return (oracles_guest_read8(c->guest, flags) & t->roomflag_visited) != 0;
}

/* The rooms the composition just drew, against the game's own record: one kept dark that the game says visited, one
 * drawn in clear that it says never visited, the room in play aside (the leak the setting exists to prevent).  A room
 * kept dark shows its edge beside a room in clear, its terrain over a few pixels along their frontier: kept dark all
 * the same, unless it shows in clear past the edge's depth, a room drawn in clear then; and never on a side where
 * nothing in clear stands beyond it. */
static void check_drawn_rooms(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob)
{
    uint8_t group;
    uint32_t dark[8], shown[8];
    if (!oracles_enhanced_view_drawn_rooms(c->view, &group, dark, shown)) return;
    c->shown_unvisited += oracles_enhanced_view_dark_lit(c->view);
    c->dark_edge_stray += oracles_enhanced_view_dark_stray(c->view);
    for (unsigned r = 0; r < 256u; r++) {
        const uint32_t bit = 1u << (r & 31u);
        if (dark[r >> 5u] & bit) { if (room_visited(c, group, (uint8_t)r)) c->dark_visited++; }
        else if ((shown[r >> 5u] & bit) && !(group == ob->group && r == ob->room) && !room_visited(c, group, (uint8_t)r)) c->shown_unvisited++;
    }
}

void oracles_enhanced_check_frame_end(OraclesEnhancedCheck *c, uint32_t frame)
{
    OraclesEnhancedMode mode;
    int32_t camera_x = 0, camera_y = 0;
    for (unsigned i = 0; i < c->subpixel_switches; i++)
        if (c->subpixel_switch_at[i] == frame) {
            oracles_enhanced_view_switch_subpixel(c->view, !oracles_enhanced_view_subpixel_on(c->view));
            c->subpixel_switched++;
        }
    const double compose_start = ec_monotonic_us();
    const uint32_t *surface = oracles_enhanced_view_compose(c->view, &mode, &camera_x, &camera_y);
    {   /* A renderer of objects is judged on the peak, which puts a frame late, not on the mean. */
        const double spent = ec_monotonic_us() - compose_start;
        c->compose_us_total += spent;
        if (spent > c->compose_us_max) c->compose_us_max = spent;
    }
    if (c->text_size) ec_check_scaled_text_box(c, mode, surface);
    if (c->text_size) ec_check_text_size_redraw(c, mode);
    if (c->wide) ec_check_wide_text_box(c, mode, surface, frame);   /* the frame just composed, before a reload replaces the state it shows */
    else if (c->host_box) ec_check_host_text_box(c, mode, frame);
    const uint64_t hash = oracles_guest_hash((const uint8_t *)surface, (size_t)c->width * oracles_enhanced_view_height(c->view) * sizeof *surface, ORACLES_HASH_SEED);
    c->run_hash = oracles_guest_hash((const uint8_t *)&hash, sizeof hash, c->run_hash);
    for (unsigned i = 0; i < c->surfaces; i++) if (c->surface_at[i] == frame) write_surface(c, surface, c->surface_path[i]);
    c->frames++;
    const OraclesEnhancedObservation *ob = oracles_enhanced_view_observation(c->view);
    int blank = surface != NULL;   /* one colour: the screen faded out, where a camera that moves shows nothing moving */
    for (size_t i = 1, n = (size_t)c->width * oracles_enhanced_view_height(c->view); blank && i < n; i++) blank = surface[i] == surface[0];
    ec_measure_camera(c, ob, mode, camera_x, camera_y, blank || oracles_enhanced_view_washed_out(c->view));
    ec_measure_window_darker(c, ob, mode, surface, camera_x, camera_y, blank, frame);
    ec_measure_whole_room(c, ob, mode, surface, camera_x, camera_y);
    ec_measure_alone_rows(c, ob, mode, camera_y);
    {   /* the hotbar over a menu, a cutscene's picture or in a cutscene, a room's load apart */
        int32_t left = 0, top = 0;
        if (oracles_enhanced_view_hotbar_drawn(c->view) && !oracles_enhanced_view_load_band(c->view, &left, &top) && (mode != ORACLES_ENHANCED_WORLD || ob->cutscene)
            && !(ob->in_transition && !ob->in_scroll)) c->hotbar_cutscene_frames++;
        if (oracles_enhanced_view_framed_only(c->view)) {   /* F3: the core framed as in Faithful, which has no hotbar */
            c->framed_only_frames++;
            if (oracles_enhanced_view_hotbar_drawn(c->view)) c->hotbar_framed_only_frames++;
        }
    }
    {   /* each run is a jump to the large menus' scale and back */
        uint32_t crop[4];
        const int enlarged = oracles_enhanced_view_framed_crop(c->view, crop);
        if (enlarged) { c->enlarged_frames++; if (!c->last_enlarged) c->enlarged_runs++; }
        else if (c->last_enlarged && mode != ORACLES_ENHANCED_WORLD) {   /* the framed image shrinking in place */
            if (c->enlarged_framed_drops < 8u) c->enlarged_framed_drop_at[c->enlarged_framed_drops] = frame;
            c->enlarged_framed_drops++;
        }
        c->last_enlarged = enlarged;
    }
    {   /* a room's load under the world's HUD band: from a world frame to a load and back, the band and the game area stay */
        int32_t left = 0, top = 0;
        const int kind = oracles_enhanced_view_load_band(c->view, &left, &top) ? 2 : mode == ORACLES_ENHANCED_WORLD ? 1 : 0;
        const uint64_t band = oracles_guest_hash((const uint8_t *)surface, (size_t)c->width * ORACLES_ENHANCED_HUD_HEIGHT * sizeof *surface, ORACLES_HASH_SEED);
        if (kind && c->load_last_kind && kind != c->load_last_kind) {
            c->load_boundaries++;
            if (band != c->load_last_band) c->load_band_changes++;
            if (left != c->load_last_left || top != c->load_last_top) c->load_area_moves++;
        }
        c->load_last_kind = kind;
        c->load_last_band = band;
        c->load_last_left = left;
        c->load_last_top = top;
    }

    int shown[4];
    uint8_t room[4];
    unsigned shown_count = 0;
    for (unsigned d = 0; d < 4; d++) { oracles_enhanced_view_neighbour(c->view, d, &shown[d], &room[d]); shown_count += shown[d] ? 1u : 0u; }
    if (mode == ORACLES_ENHANCED_WORLD) c->neighbours_frames[shown_count]++;
    const unsigned uncovered = oracles_enhanced_view_uncovered(c->view);
    if (mode == ORACLES_ENHANCED_WORLD && uncovered) ec_measure_coverage(c, frame, uncovered);
    if (mode == ORACLES_ENHANCED_WORLD && ob->playing && !ob->in_transition) check_drawn_rooms(c, ob);   /* before a reload replaces the state composed */
    ec_reload_step(c, frame, camera_x, shown, room);
    ec_measure_scroll(c, ob, mode, uncovered);
    ec_measure_seams(c, ob, mode);
    ec_measure_link_backsteps(c, frame, ob, mode, camera_x, camera_y);
    ec_measure_swim(c, frame, ob, mode, camera_x, camera_y);
    /* What the band showed at the last world frame: a framed one (a menu, a text framed) shows no neighbour and says
     * nothing of them, and a scroll may start at the first world frame after it. */
    if (ob->playing && !ob->in_transition && mode == ORACLES_ENHANCED_WORLD) {
        for (unsigned d = 0; d < 4; d++) {
            c->last_shown[d] = shown[d]; c->last_room[d] = room[d];
            c->last_dark[d] = oracles_enhanced_view_neighbour_dark(c->view, d, NULL);
            c->last_layout_valid[d] = oracles_enhanced_view_neighbour_layout(c->view, d, c->last_layout[d]);
        }
        c->last_play_room = ob->room;
    } else if (oracles_enhanced_view_framed_only(c->view))   /* F3: the core framed for as long as the player keeps it, no neighbour shown */
        for (unsigned d = 0; d < 4; d++) c->last_shown[d] = 0;

    ec_check_blurb(c, mode, frame);
    if (c->witness) ec_check_beside(c, ob, mode, frame);
    check_pixels_and_row(c, ob, mode, surface, camera_x, camera_y, frame, hash, shown, room, uncovered);
    if (c->paced) {   /* the frame's end at its time of play, waited for on the clock alone */
        if (!c->paced_start) c->paced_start = ec_monotonic_us() - (double)frame * 16742.706;
        while (ec_monotonic_us() < c->paced_start + (double)(frame + 1u) * 16742.706) {}
    }
}

/* The room entered is loaded: the terrain that was shown for it against the
 * layout the game commits, tile for tile, the entry-dependent shutters set
 * aside.  A room shown in its old state on purpose (the ghost running it
 * again after a season change) is counted apart. */
static void judge_shown_terrain(OraclesEnhancedCheck *c, const OraclesGuestTables *t)
{
    c->pending_transition = 0;
    if (!c->pending_shown) return;
    const uint8_t *live = oracles_guest_ptr(c->guest, t->room_layout, ORACLES_GHOST_LAYOUT_BYTES);
    if (!live) return;
    unsigned different = 0;
    for (unsigned i = 0; i < ORACLES_GHOST_LAYOUT_BYTES; i++) {
        if ((i & 0x0fu) >= 15u || live[i] == c->pending_layout[i]) continue;
        if (oracles_ghost_entry_dependent_tile(t, live[i]) || oracles_ghost_entry_dependent_tile(t, c->pending_layout[i])) continue;
        different++;
    }
    if (different == 0) c->terrains_equal++;
    else if (c->pending_old) c->terrains_old++;   /* shown in its old state on purpose: the change pops in at the entry */
    else {
        c->terrains_different++;
        c->terrain_tiles_different += different;
        const size_t used = strlen(c->different_list);
        if (used + 12 < sizeof c->different_list)
            snprintf(c->different_list + used, sizeof c->different_list - used, "%s%02x:%u", used ? " " : "", c->pending_room, different);
    }
}

/* At the load of a scrolling transition, in any of the four directions: was
 * the room entered already shown as a neighbour, and is the terrain that was
 * shown the one the game commits after the real entry (tile for tile, the
 * entry-dependent shutters set aside)?  A neighbour shown is judged black or
 * equal; it must never be different. */
void oracles_enhanced_check_event(OraclesEnhancedCheck *c, const OraclesGuestEvent *event)
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    if (c->link_sprites) oracles_sprites_event(c->link_sprites, event);
    if (event->type == ORACLES_EVENT_FRAME_DRAWN) ec_note_blurb_drawn(c);
    if (event->type == ORACLES_EVENT_ROOM_INITIALIZED && c->pending_transition) { judge_shown_terrain(c, t); return; }
    if (event->type != ORACLES_EVENT_ROOM_ENTER) return;
    if (oracles_guest_read8(c->guest, t->scroll_mode) != SCROLL_MODE_TRANSITION_LOAD) return;
    /* The game's direction: 0 up, 1 right, 2 down, 3 left; the neighbour that
     * way is the room entered.  The four are judged: the vertical camera shows
     * the rows above and below as the horizontal one shows the sides. */
    const uint8_t direction = (uint8_t)(oracles_guest_read8(c->guest, t->screen_transition_direction) & 3u);
    const uint8_t room = oracles_guest_read8(c->guest, t->active_room);
    /* A large room (a dungeon, its side views) shows the room entered during
     * the scroll only, deliberately: not a neighbour shown before. */
    if (oracles_enhanced_view_observation(c->view)->large_grid) { c->transitions_large++; return; }
    c->transitions++;
    if (direction == 0u || direction == 2u) c->transitions_vertical++;
    const int shown = c->last_shown[direction] && c->last_room[direction] == room;
    if (c->last_shown[direction] && c->last_room[direction] != room) {
        /* Something was drawn that way, and it was not the room the game loads. */
        c->transitions_other_room++;
        const size_t used = strlen(c->other_room_list);
        if (used + 16 < sizeof c->other_room_list)
            snprintf(c->other_room_list + used, sizeof c->other_room_list - used, "%s%02x>%02x=%02x", used ? " " : "", c->last_play_room, room, c->last_room[direction]);
    }
    c->pending_transition = 1;
    c->pending_room = room;
    c->pending_shown = shown && c->last_layout_valid[direction];
    c->pending_old = shown && c->last_layout_valid[direction] == 2;
    if (c->pending_shown) memcpy(c->pending_layout, c->last_layout[direction], sizeof c->pending_layout);
    if (shown) c->transitions_with_terrain++;
    else if (c->last_dark[direction]) c->transitions_dark++;
    else if (!c->last_shown[direction]) {
        c->transitions_black++;
        const size_t used = strlen(c->black_list);
        if (used + 12 < sizeof c->black_list)
            snprintf(c->black_list + used, sizeof c->black_list - used, "%s%02x>%02x", used ? " " : "", c->last_play_room, room);
    }
}
