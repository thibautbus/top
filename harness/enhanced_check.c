#include "enhanced_check_internal.h"

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
    c->threaded = ghost_budget == 0;
    c->run_hash = ORACLES_HASH_SEED;
    c->width = oracles_enhanced_view_width(c->view);
    c->band_height = oracles_enhanced_view_height(c->view) - ORACLES_ENHANCED_HUD_HEIGHT;
    c->link_edge_margin_min = (int32_t)c->width;   /* no world frame measured yet */
    char path[4096];
    snprintf(path, sizeof path, "%s/frames.tsv", dir);
    c->tsv = fopen(path, "w");
    if (c->tsv) fprintf(c->tsv, "frame\thash\tmode\tcamera_x\tgroup\troom\tlink_x\tlink_y\tscroll_mode\ttransition_state\tlink_world_x\twindow_left\tepoch\tleft\tright\tuncovered\tdrawn_scx\tdrawn_scy\thcamera_x\thcamera_y\tscreen_offset_x\tscreen_offset_y\toam_used\toam0_y\toam0_x\tlcdc\tcamera_y\tlink_world_y\twindow_top\tmenu\tfade\tisolated\tlarge\torigin_x\torigin_y\n");
    return c;
}

void oracles_enhanced_check_reload_at(OraclesEnhancedCheck *c, uint32_t frame) { c->reload_at = frame; }
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

static void write_surface(const OraclesEnhancedCheck *c, const uint32_t *surface, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    const unsigned height = oracles_enhanced_view_height(c->view);
    fprintf(f, "P6\n%u %u\n255\n", c->width, height);
    for (unsigned i = 0; i < c->width * height; i++) { const uint8_t rgb[3] = { (uint8_t)(surface[i] >> 16), (uint8_t)(surface[i] >> 8), (uint8_t)surface[i] }; fwrite(rgb, 1, 3, f); }
    fclose(f);
}
void oracles_enhanced_check_set_neighbour_objects(OraclesEnhancedCheck *c, int enabled) { oracles_enhanced_view_set_neighbour_objects(c->view, enabled); }
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

void oracles_enhanced_check_frame_end(OraclesEnhancedCheck *c, uint32_t frame)
{
    OraclesEnhancedMode mode;
    int32_t camera_x = 0, camera_y = 0;
    const double compose_start = ec_monotonic_us();
    const uint32_t *surface = oracles_enhanced_view_compose(c->view, &mode, &camera_x, &camera_y);
    {   /* A renderer of objects is judged on the peak, which puts a frame late, not on the mean. */
        const double spent = ec_monotonic_us() - compose_start;
        c->compose_us_total += spent;
        if (spent > c->compose_us_max) c->compose_us_max = spent;
    }
    const uint64_t hash = oracles_guest_hash((const uint8_t *)surface, (size_t)c->width * oracles_enhanced_view_height(c->view) * sizeof *surface, ORACLES_HASH_SEED);
    c->run_hash = oracles_guest_hash((const uint8_t *)&hash, sizeof hash, c->run_hash);
    for (unsigned i = 0; i < c->surfaces; i++) if (c->surface_at[i] == frame) write_surface(c, surface, c->surface_path[i]);
    c->frames++;
    const OraclesEnhancedObservation *ob = oracles_enhanced_view_observation(c->view);
    int blank = surface != NULL;   /* one colour: the screen faded out, where a camera that moves shows nothing moving */
    for (size_t i = 1, n = (size_t)c->width * oracles_enhanced_view_height(c->view); blank && i < n; i++) blank = surface[i] == surface[0];
    ec_measure_camera(c, ob, mode, camera_x, camera_y, blank);

    int shown[4];
    uint8_t room[4];
    unsigned shown_count = 0;
    for (unsigned d = 0; d < 4; d++) { oracles_enhanced_view_neighbour(c->view, d, &shown[d], &room[d]); shown_count += shown[d] ? 1u : 0u; }
    if (mode == ORACLES_ENHANCED_WORLD) c->neighbours_frames[shown_count]++;
    const unsigned uncovered = oracles_enhanced_view_uncovered(c->view);
    if (mode == ORACLES_ENHANCED_WORLD && uncovered) ec_measure_coverage(c, frame, uncovered);
    ec_reload_step(c, frame, camera_x, shown, room);
    ec_measure_scroll(c, ob, mode, uncovered);
    ec_measure_seams(c, ob, mode);
    if (ob->playing && !ob->in_transition) {
        for (unsigned d = 0; d < 4; d++) {
            c->last_shown[d] = shown[d]; c->last_room[d] = room[d];
            c->last_layout_valid[d] = oracles_enhanced_view_neighbour_layout(c->view, d, c->last_layout[d]);
        }
        c->last_play_room = ob->room;
    }

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
    else if (!c->last_shown[direction]) {
        c->transitions_black++;
        const size_t used = strlen(c->black_list);
        if (used + 12 < sizeof c->black_list)
            snprintf(c->black_list + used, sizeof c->black_list - used, "%s%02x>%02x", used ? " " : "", c->last_play_room, room);
    }
}
