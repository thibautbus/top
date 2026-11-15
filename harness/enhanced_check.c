#include "enhanced_check_internal.h"
#include "floating.h"
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
    c->band_top = oracles_enhanced_view_band_top(c->view);
    c->band_height = oracles_enhanced_view_height(c->view) - c->band_top;
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

void oracles_enhanced_check_half_switched(OraclesEnhancedCheck *c, const uint8_t *rom, size_t rom_size)
{
    if (c->link_sprites) { oracles_sprites_forget(c->link_sprites); oracles_sprites_add_hooks(c->guest); }
    c->have_camera = 0;   /* the view's camera starts afresh, as at a teleport */
    c->have_last_observation = 0;
    c->have_drawn_link = 0;
    c->blurb_drawn_count = 0;
    c->pending_transition = 0;
    c->load_last_kind = 0;
    memset(c->last_shown, 0, sizeof c->last_shown);
    if (c->witness) {
        OraclesGhost *ghost = oracles_ghost_create(rom, rom_size, oracles_guest_profile(c->guest), oracles_core_kind(c->core));
        if (ghost) { oracles_ghost_destroy(c->witness); c->witness = ghost; oracles_ghost_set_capture(ghost, 1); }
    }
}

uint64_t oracles_enhanced_check_last_hash(const OraclesEnhancedCheck *c) { return c->last_hash; }
void oracles_enhanced_check_reload_at(OraclesEnhancedCheck *c, uint32_t frame) { c->reload_at = frame; }
void oracles_enhanced_check_hotbar_forget_at(OraclesEnhancedCheck *c, uint32_t frame) { c->hotbar_forget_at = frame; }
void oracles_enhanced_check_reload_in_place(OraclesEnhancedCheck *c) { c->reload_in_place = 1; }
void oracles_enhanced_check_reload_after(OraclesEnhancedCheck *c, uint32_t frames) { c->reload_after = frames; }

void oracles_enhanced_check_surfaces(OraclesEnhancedCheck *c, const char *out, uint32_t from, const char *against)
{
    if (out) c->surfaces_out = fopen(out, "wb");
    if (against) c->surfaces_against = fopen(against, "rb");
    c->surfaces_from = from;
}

#define SURFACE_STEP 8u   /* a frame in eight: a row's file stays in the tens of megabytes */

#define AGAINST_NEAR 3   /* pixels: the camera a load leaves a few pixels from the replay's, held short of rooms still black */

/* A world frame whose camera stands a few pixels from the replay's: the world band held against the replay's shifted by
 * that gap, where both show the same place of the world, the status bar above it as it is. */
static void measure_near(OraclesEnhancedCheck *c, const uint32_t *surface, const uint32_t *other, int32_t dx, int32_t dy, uint32_t frame)
{
    const int32_t width = (int32_t)c->width, height = (int32_t)oracles_enhanced_view_height(c->view), hud = (int32_t)c->band_top;
    unsigned wrong = 0, black = 0;
    for (int32_t y = 0; y < height; y++) {
        const int32_t oy = y < hud ? y : y + dy;
        if (oy < (y < hud ? 0 : hud) || oy >= (y < hud ? hud : height)) continue;
        for (int32_t x = 0; x < width; x++) {
            const int32_t ox = y < hud ? x : x + dx;
            if (ox < 0 || ox >= width) continue;
            const uint32_t mine = surface[(size_t)y * (size_t)width + (size_t)x], theirs = other[(size_t)oy * (size_t)width + (size_t)ox];
            if (mine == theirs) continue;
            if (ec_under_panel(c, (unsigned)x, (unsigned)y) || ec_under_panel(c, (unsigned)ox, (unsigned)oy)) continue;   /* the status bar floating, unmoved over both */
            if (mine & 0x00ffffffu) wrong++; else black++;
        }
    }
    c->against_near_compared++;
    if (wrong) { c->against_near_wrong_frames++; c->against_near_last_wrong = frame; if (wrong > c->against_near_wrong_max) c->against_near_wrong_max = wrong; }
    if (black) { c->against_near_last_black = frame; if (black > c->against_near_black_max) c->against_near_black_max = black; }
}

/* From `from`, every eighth frame's camera and surface: written, or held against those of the replay without the load
 * from the frame after the load.  A pixel lit that differs is a wrong picture; a pixel left black is what the view has
 * not filled yet.  The frames whose camera stands elsewhere than in that replay, and those of a fade, which lights the
 * black, say nothing of what is drawn. */
static void measure_surfaces(OraclesEnhancedCheck *c, const uint32_t *surface, int32_t camera_x, int32_t camera_y, uint32_t frame, int world)
{
    if (!surface || (!c->surfaces_out && !c->surfaces_against) || frame < c->surfaces_from || (frame - c->surfaces_from) % SURFACE_STEP) return;
    const size_t pixels = (size_t)c->width * oracles_enhanced_view_height(c->view);
    const int32_t camera[2] = { camera_x, camera_y };
    if (c->surfaces_out) { fwrite(camera, sizeof camera, 1, c->surfaces_out); fwrite(surface, sizeof *surface, pixels, c->surfaces_out); }
    if (!c->surfaces_against || !c->reload_done || frame <= c->reload_frame) return;
    if (!c->surface_other) c->surface_other = malloc(pixels * sizeof *surface);
    int32_t other_camera[2];
    const long record = (long)(sizeof camera + pixels * sizeof *surface);
    if (!c->surface_other || fseek(c->surfaces_against, (long)((frame - c->surfaces_from) / SURFACE_STEP) * record, SEEK_SET) != 0
        || fread(other_camera, sizeof other_camera, 1, c->surfaces_against) != 1
        || fread(c->surface_other, sizeof *surface, pixels, c->surfaces_against) != pixels) { c->against_unmatched++; return; }
    c->against_frames++;
    if (memcmp(camera, other_camera, sizeof camera) != 0) {
        c->against_camera_apart++;
        const int32_t dx = camera_x - other_camera[0], dy = camera_y - other_camera[1];
        if (world && dx >= -AGAINST_NEAR && dx <= AGAINST_NEAR && dy >= -AGAINST_NEAR && dy <= AGAINST_NEAR && !oracles_enhanced_view_faded(c->view))
            measure_near(c, surface, c->surface_other, dx, dy, frame);
        return;
    }
    if (oracles_enhanced_view_faded(c->view)) { c->against_faded++; return; }
    c->against_compared++;
    unsigned wrong = 0, black = 0;
    for (size_t i = 0; i < pixels; i++) {
        if (surface[i] == c->surface_other[i]) continue;
        /* The status bar floating: a panel over a column the other has filled and this one not yet is not a wrong picture. */
        if (ec_under_panel(c, (unsigned)(i % c->width), (unsigned)(i / c->width))) continue;
        if (surface[i] & 0x00ffffffu) wrong++; else black++;
    }
    if (wrong) { c->against_wrong_frames++; c->against_last_wrong = frame; if (wrong > c->against_wrong_max) c->against_wrong_max = wrong; }
    if (black) { c->against_last_black = frame; if (black > c->against_black_max) c->against_black_max = black; }
}

#define BAND_RECORDS 128u

void oracles_enhanced_check_band_surfaces(OraclesEnhancedCheck *c, const char *out, uint32_t from, const char *against)
{
    if (out) c->band_out = fopen(out, "wb");
    if (against) c->band_against = fopen(against, "rb");
    c->band_from = from;
    c->band_rows_min = UINT32_MAX;
}

/* A pixel the band has not lit: the band's black, the game's fade aside (the frames of a fade are not compared). */
static int unlit(uint32_t pixel) { return !(pixel & 0x00ffffffu); }

/* The world band floating against the replay's under the HUD band: a world row shows at line `top + row - camera` of
 * each, and a world column likewise; the panels over the band, and a frame with a text (its box follows the band, not
 * the world), a fade or no world in either, compared with nothing. */
static void measure_band(OraclesEnhancedCheck *c, const uint32_t *surface, int32_t camera_x, int32_t camera_y, uint32_t frame, int world)
{
    if (!surface || (!c->band_out && !c->band_against) || frame < c->band_from || (frame - c->band_from) % SURFACE_STEP) return;
    const uint32_t index = (frame - c->band_from) / SURFACE_STEP;
    if (index >= BAND_RECORDS) return;
    const unsigned width = c->width, height = oracles_enhanced_view_height(c->view);
    const size_t pixels = (size_t)width * height;
    const int text = oracles_guest_read8(c->guest, oracles_guest_tables(c->guest)->text_is_active) != 0;
    const int32_t head[4] = { camera_x, camera_y, world && !text && !oracles_enhanced_view_faded(c->view), (int32_t)c->band_top };
    if (c->band_out) { fwrite(head, sizeof head, 1, c->band_out); fwrite(surface, sizeof *surface, pixels, c->band_out); }
    if (!c->band_against) return;
    if (!c->band_other) c->band_other = malloc(pixels * sizeof *surface);
    int32_t other[4];
    const long record = (long)(sizeof head + pixels * sizeof *surface);
    if (!c->band_other || fseek(c->band_against, (long)index * record, SEEK_SET) != 0 || fread(other, sizeof other, 1, c->band_against) != 1
        || fread(c->band_other, sizeof *surface, pixels, c->band_against) != pixels) { c->band_unmatched++; return; }
    c->band_frames++;
    if (!head[2] || !other[2]) return;
    c->band_compared++;
    if (camera_y == other[1] - (int32_t)(other[3] - head[3]) / 2) c->band_eight++;
    uint32_t panels[ORACLES_ENHANCED_FLOATING_PANELS][4];
    const unsigned panel_count = oracles_enhanced_view_floating_scale(c->view) == 1u ? oracles_enhanced_view_floating_panels(c->view, panels) : 0u;   /* else none in the surface */
    unsigned wrong = 0, dark = 0, rows = 0;
    for (unsigned y = c->band_top; y < height; y++) {
        const int32_t oy = other[3] + (camera_y + (int32_t)(y - c->band_top)) - other[1];
        if (oy < other[3] || oy >= (int32_t)height) continue;
        rows++;
        for (unsigned x = 0; x < width; x++) {
            const int32_t ox = (int32_t)x + camera_x - other[0];
            if (ox < 0 || ox >= (int32_t)width) continue;
            int covered = 0;
            for (unsigned p = 0; p < panel_count && !covered; p++)
                covered = x >= panels[p][0] && x < panels[p][0] + panels[p][2] && y >= panels[p][1] && y < panels[p][1] + panels[p][3];
            if (covered) continue;
            const uint32_t mine = surface[(size_t)y * width + x], theirs = c->band_other[(size_t)oy * width + (size_t)ox];
            if (mine == theirs) continue;
            if (unlit(mine) || unlit(theirs)) dark++; else wrong++;
        }
    }
    if (rows < c->band_rows_min) c->band_rows_min = rows;
    if (wrong) { c->band_wrong_frames++; c->band_last_wrong = frame; if (wrong > c->band_wrong_max) c->band_wrong_max = wrong; }
    if (dark > c->band_unlit_max) c->band_unlit_max = dark;
}

void oracles_enhanced_check_floating_avoid(OraclesEnhancedCheck *c, const int32_t (*rects)[4], unsigned count)
{
    c->floating_avoid_count = count < 4u ? count : 4u;
    if (c->floating_avoid_count) memcpy(c->floating_avoid, rects, c->floating_avoid_count * sizeof *rects);
    oracles_enhanced_view_set_floating_avoid(c->view, rects, count);
}

/* A panel's place on the surface (x, y, w, h) meets a rectangle of it. */
static int floating_meets(const float at[4], const int32_t r[4])
{
    return at[0] < (float)(r[0] + r[2]) && (float)r[0] < at[0] + at[2] && at[1] < (float)(r[1] + r[3]) && (float)r[1] < at[1] + at[3];
}

/* The surface's pixels a pixel (lx, ly) of a panel `rect` of the layer at half the game's scale meets, the panel at
 * `place` on the surface: from (x0, y0) to (x1, y1), exclusive. */
static void texel_span(const float place[4], const uint32_t rect[4], uint32_t lx, uint32_t ly, int32_t span[4])
{
    const float ux = place[2] / (float)rect[2], uy = place[3] / (float)rect[3];
    const float fx = place[0] + (float)(lx - rect[0]) * ux, fy = place[1] + (float)(ly - rect[1]) * uy;
    span[0] = (int32_t)floorf(fx + 0.001f); span[2] = (int32_t)ceilf(fx + ux - 0.001f);
    span[1] = (int32_t)floorf(fy + 0.001f); span[3] = (int32_t)ceilf(fy + uy - 0.001f);
}

static int span_on_blurb(const OraclesEnhancedCheck *c, const int32_t span[4])
{
    for (int32_t y = span[1]; y < span[3]; y++)
        for (int32_t x = span[0]; x < span[2]; x++)
            if (oracles_enhanced_view_blurb_pixel(c->view, x, y, NULL)) return 1;
    return 0;
}

/* The area blurb drawn apart under the panels: each of its pixels in a panel's place shown as drawn, over the panel, in
 * the surface as the presenter shows it (`surface`), and at half the game's scale with no pixel of the panels' layer
 * over it. */
static void measure_floating_blurb(OraclesEnhancedCheck *c, const uint32_t *surface, const uint32_t rects[][4], const float place[][4], unsigned count, unsigned k,
                                   uint32_t frame)
{
    if (!oracles_enhanced_view_blurb_drawn(c->view)) return;
    const unsigned width = c->width, height = oracles_enhanced_view_height(c->view);
    uint32_t at[ORACLES_ENHANCED_FLOATING_PANELS][4];
    unsigned drawn = 0;
    const uint32_t *layer = k > 1u ? oracles_enhanced_view_floating_layer(c->view, at, &drawn) : NULL;
    const size_t layer_w = (size_t)k * width;
    int under = 0, hidden = 0;
    for (unsigned i = 0; i < count; i++) {
        const int32_t x0 = (int32_t)floorf(place[i][0]), y0 = (int32_t)floorf(place[i][1]);
        const int32_t x1 = (int32_t)ceilf(place[i][0] + place[i][2]), y1 = (int32_t)ceilf(place[i][1] + place[i][3]);
        for (int32_t y = y0 < 0 ? 0 : y0; y < y1 && y < (int32_t)height; y++)
            for (int32_t x = x0 < 0 ? 0 : x0; x < x1 && x < (int32_t)width; x++) {
                uint32_t colour;
                if (!oracles_enhanced_view_blurb_pixel(c->view, x, y, &colour)) continue;
                under = 1;
                if (surface[(size_t)y * width + (size_t)x] != colour) hidden = 1;
                if (!layer) continue;
                const float ux = place[i][2] / (float)rects[i][2], uy = place[i][3] / (float)rects[i][3];
                const int32_t ax = (int32_t)rects[i][0] + (int32_t)floorf(((float)x - place[i][0]) / ux) - 1, ay = (int32_t)rects[i][1] + (int32_t)floorf(((float)y - place[i][1]) / uy) - 1;
                for (uint32_t ly = ay > (int32_t)rects[i][1] ? (uint32_t)ay : rects[i][1]; ly < rects[i][1] + rects[i][3] && (int32_t)ly <= ay + 3; ly++)
                    for (uint32_t lx = ax > (int32_t)rects[i][0] ? (uint32_t)ax : rects[i][0]; lx < rects[i][0] + rects[i][2] && (int32_t)lx <= ax + 3; lx++) {
                        int32_t span[4];
                        texel_span(place[i], rects[i], lx, ly, span);
                        if (x >= span[0] && x < span[2] && y >= span[1] && y < span[3] && layer[ly * layer_w + lx] >> 24) hidden = 1;
                    }
            }
    }
    c->floating_blurb_frames += (unsigned)under;
    if (hidden) { c->floating_blurb_hidden++; c->floating_blurb_last_hidden = frame; }
}

/* The status bar floating: the panels clear of the touch controls, and each block of the bar a panel shows whole, the
 * core's own pixels at their place but its background (the colour most of the bar has), which the panel replaces. */
static void measure_floating(OraclesEnhancedCheck *c, const uint32_t *surface, OraclesEnhancedMode mode, uint32_t frame)
{
    if (c->band_top || !surface) return;
    if (mode == ORACLES_ENHANCED_WORLD) c->floating_band_frames++;
    uint32_t rects[ORACLES_ENHANCED_FLOATING_PANELS][4];
    int32_t sources[ORACLES_ENHANCED_FLOATING_PANELS][6];
    const unsigned count = oracles_enhanced_view_floating_panels(c->view, rects);
    oracles_enhanced_view_floating_sources(c->view, sources);
    /* At half the game's scale, each panel where the presenter draws it on the screen the view is laid out for, in the
     * surface's pixels: kept to its corner, at the screen's unit (oracles_floating_surface_place). */
    const unsigned k = oracles_enhanced_view_floating_scale(c->view);
    const uint32_t layer_w = k * c->width, layer_h = k * oracles_enhanced_view_height(c->view);
    const float screen = c->surface_scale ? (float)c->surface_scale : 2.0f;
    float place[ORACLES_ENHANCED_FLOATING_PANELS][4];
    for (unsigned i = 0; i < count; i++) {
        const int32_t rect[4] = { (int32_t)rects[i][0], (int32_t)rects[i][1], (int32_t)rects[i][2], (int32_t)rects[i][3] };
        if (k == 1u) for (unsigned j = 0; j < 4u; j++) place[i][j] = (float)rect[j];
        else oracles_floating_surface_place(rect, oracles_floating_corner(rect, layer_w, layer_h), layer_w, layer_h, screen, place[i]);
    }
    int met = 0;
    for (unsigned i = 0; i < count; i++)
        for (unsigned a = 0; a < c->floating_avoid_count; a++) met |= floating_meets(place[i], c->floating_avoid[a]);
    c->floating_avoid_met += (unsigned)met;
    {   /* the text box over the band: no panel drawn over it, not even fading */
        unsigned shown[ORACLES_ENHANCED_FLOATING_PANELS];
        int32_t box[4];
        if (count && oracles_enhanced_view_text_box_rect(c->view, box)) {
            c->floating_box_frames++;
            oracles_enhanced_view_floating_shown(c->view, shown);
            for (unsigned i = 0; i < count; i++)
                if (floating_meets(place[i], box)) {
                    if (shown[i]) c->floating_box_met++;
                    else c->floating_box_hid++;
                }
        }
    }
    measure_floating_blurb(c, surface, (const uint32_t(*)[4])rects, (const float(*)[4])place, count, k, frame);
    const uint32_t *core = oracles_core_pixels(c->core);
    if (!core || !count) return;
    uint32_t seen[16];
    unsigned times[16], kinds = 0;
    for (unsigned i = 0; i < ORACLES_ENHANCED_HUD_HEIGHT * ORACLES_ENHANCED_CORE_WIDTH; i++) {
        unsigned k = 0;
        while (k < kinds && seen[k] != core[i]) k++;
        if (k == kinds && kinds < 16u) { seen[kinds] = core[i]; times[kinds++] = 0; }
        if (k < kinds) times[k]++;
    }
    unsigned best = 0;
    for (unsigned k = 1; k < kinds; k++) if (times[k] > times[best]) best = k;
    const uint32_t background = seen[best];
    unsigned width = c->width, height = oracles_enhanced_view_height(c->view);
    if (k > 1) {   /* the bar's pixels in the panels' layer, whole: opaque, the core's own */
        uint32_t at[ORACLES_ENHANCED_FLOATING_PANELS][4];
        unsigned drawn = 0;
        surface = oracles_enhanced_view_floating_layer(c->view, at, &drawn);
        width *= (unsigned)k;
        height *= (unsigned)k;
        if (!surface) return;
    }
    unsigned wrong = 0;
    for (unsigned i = 0; i < count; i++) {
        const int32_t *s = sources[i];
        for (int32_t y = 0; y < s[3]; y++)
            for (int32_t x = 0; x < s[2]; x++) {
                const uint32_t pixel = core[(unsigned)(s[1] + y) * ORACLES_ENHANCED_CORE_WIDTH + (unsigned)(s[0] + x)];
                const int32_t sx = s[4] + x, sy = s[5] + y;
                if (pixel == background || sx < 0 || sy < 0 || sx >= (int32_t)width || sy >= (int32_t)height) continue;
                /* The area blurb over the panel: its pixel, measured apart (measure_floating_blurb). */
                int32_t span[4] = { sx, sy, sx + 1, sy + 1 };
                if (k > 1) texel_span(place[i], rects[i], (uint32_t)sx, (uint32_t)sy, span);
                if (span_on_blurb(c, span)) continue;
                c->floating_content_compared++;
                wrong += surface[(size_t)sy * width + (size_t)sx] != pixel;
            }
    }
    if (wrong) { c->floating_content_wrong_frames++; c->floating_content_last_wrong = frame; }
}

void oracles_enhanced_check_hud_switch_at(OraclesEnhancedCheck *c, const uint32_t *frames, unsigned count, int base)
{
    c->hud_switches = count < 8u ? count : 8u;
    memcpy(c->hud_switch_at, frames, c->hud_switches * sizeof *frames);
    c->hud_base = base == ORACLES_ENHANCED_HUD_FLOATING ? ORACLES_ENHANCED_HUD_ORIGINAL : base;
}

void oracles_enhanced_check_set_transition(OraclesEnhancedCheck *c, OraclesRoomTransition *transition) { c->transition = transition; }
void oracles_enhanced_check_set_paced(OraclesEnhancedCheck *c, int paced) { c->paced = paced; }

void oracles_enhanced_check_set_zoom_out(OraclesEnhancedCheck *c, int enabled) { oracles_enhanced_check_set_size(c, oracles_enhanced_size(enabled)); }

void oracles_enhanced_check_set_size(OraclesEnhancedCheck *c, OraclesEnhancedSize size)
{
    oracles_enhanced_view_set_size(c->view, size);
    c->width = oracles_enhanced_view_width(c->view);
    c->band_top = oracles_enhanced_view_band_top(c->view);
    c->band_height = oracles_enhanced_view_height(c->view) - c->band_top;
    c->link_edge_margin_min = (int32_t)c->width;
}

void oracles_enhanced_check_surface_at(OraclesEnhancedCheck *c, uint32_t frame, const char *path)
{
    if (c->surfaces >= sizeof c->surface_at / sizeof c->surface_at[0]) return;
    c->surface_at[c->surfaces] = frame;
    c->surface_path[c->surfaces++] = path;
}

struct OraclesEnhancedView *oracles_enhanced_check_view(OraclesEnhancedCheck *c) { return (struct OraclesEnhancedView *)c->view; }

/* The panels at half the game's scale, apart from the surface, which the presenter lays over the picture: their places,
 * their corners and their pixels hashed after the surface's (`hash`), as the player sees them at any scale. */
static uint64_t half_panels_hash(const OraclesEnhancedCheck *c, uint64_t hash)
{
    uint32_t at[ORACLES_ENHANCED_FLOATING_PANELS][4];
    uint8_t anchors[ORACLES_ENHANCED_FLOATING_PANELS];
    unsigned count = 0;
    const uint32_t *layer = oracles_enhanced_view_floating_layer(c->view, at, &count);
    if (!layer || oracles_enhanced_view_floating_scale(c->view) < 2u) return hash;
    oracles_enhanced_view_floating_anchors(c->view, anchors);
    const size_t pitch = 2u * (size_t)c->width;
    hash = oracles_guest_hash((const uint8_t *)at, count * sizeof at[0], hash);
    hash = oracles_guest_hash(anchors, count, hash);
    for (unsigned i = 0; i < count; i++)
        for (uint32_t y = at[i][1]; y < at[i][1] + at[i][3]; y++) hash = oracles_guest_hash((const uint8_t *)(layer + y * pitch + at[i][0]), at[i][2] * sizeof *layer, hash);
    return hash;
}

/* The surface as the presenter shows it: the floating panels it lays over the band with the sub-pixel camera folded in
 * (`*panels` 2), or the panels at half the game's scale apart, on a layer of their own (1); else the surface itself (0). */
static const uint32_t *presented_surface(OraclesEnhancedCheck *c, const uint32_t *surface, int *panels)
{
    uint32_t at[ORACLES_ENHANCED_FLOATING_PANELS][4];
    unsigned count = 0;
    *panels = 0;
    if (!surface) return surface;
    if (oracles_enhanced_view_floating_layer(c->view, at, &count) && oracles_enhanced_view_floating_scale(c->view) > 1u) { *panels = 1; return surface; }
    if (!count) return surface;
    const size_t pixels = (size_t)oracles_enhanced_view_width(c->view) * oracles_enhanced_view_height(c->view);
    if (c->folded_pixels != pixels) { free(c->folded); c->folded = malloc(pixels * sizeof *c->folded); c->folded_pixels = c->folded ? pixels : 0; }
    if (!c->folded) return surface;
    oracles_enhanced_view_fold_floating(c->view, surface, c->folded);
    *panels = 2;
    return c->folded;
}

/* The hash of a frame as the presenter shows it: the surface presented_surface gives, then the panels at half scale. */
static uint64_t presented_hash(const OraclesEnhancedCheck *c, const uint32_t *surface)
{
    const uint64_t hash = oracles_guest_hash((const uint8_t *)surface, (size_t)c->width * oracles_enhanced_view_height(c->view) * sizeof *surface, ORACLES_HASH_SEED);
    return surface ? half_panels_hash(c, hash) : hash;
}

uint64_t oracles_enhanced_check_held_frame(OraclesEnhancedCheck *c, const uint32_t *surface, uint32_t frame)
{
    int panels;
    surface = presented_surface(c, surface, &panels);
    measure_surfaces(c, surface, 0, 0, frame, 0);
    return presented_hash(c, surface);
}

/* The pixel of the panels at half the game's scale over (x, y) of a picture of the surface `w` x `h` (screen pixels), at
 * its unit (oracles_floating_screen_place), premultiplied; 0 where none stands. */
static uint32_t half_panel_pixel(const OraclesEnhancedCheck *c, int32_t x, int32_t y, unsigned w, unsigned h)
{
    uint32_t at[ORACLES_ENHANCED_FLOATING_PANELS][4];
    uint8_t anchors[ORACLES_ENHANCED_FLOATING_PANELS];
    unsigned count = 0;
    const uint32_t *layer = oracles_enhanced_view_floating_layer(c->view, at, &count);
    if (!layer || oracles_enhanced_view_floating_scale(c->view) < 2u) return 0;
    oracles_enhanced_view_floating_anchors(c->view, anchors);
    const uint32_t layer_w = 2u * c->width, layer_h = 2u * oracles_enhanced_view_height(c->view);
    for (unsigned i = 0; i < count; i++) {
        const int32_t rect[4] = { (int32_t)at[i][0], (int32_t)at[i][1], (int32_t)at[i][2], (int32_t)at[i][3] };
        float place[4];
        oracles_floating_screen_place(rect, anchors[i], layer_w, layer_h, (float)w, (float)h, place);
        const float unit = oracles_floating_screen_unit((float)w, layer_w);
        if ((float)x < place[0] || (float)y < place[1] || (float)x >= place[0] + place[2] || (float)y >= place[1] + place[3]) continue;
        const uint32_t lx = at[i][0] + (uint32_t)(((float)x - place[0]) / unit), ly = at[i][1] + (uint32_t)(((float)y - place[1]) / unit);
        return layer[(size_t)ly * layer_w + lx];
    }
    return 0;
}

/* The surface written as a PPM `surface_scale` times as large, as the presenter shows it on a screen of that size: the
 * whole surface at that scale, or with the large menus over the framed core the part shown alone
 * (oracles_enhanced_view_framed_crop), widened for the box where its own whole scale holds it (oracles_text_box_reserve),
 * at that scale, centred, black around; the text box the presenter enlarges drawn over it larger, at its size or the
 * next down where the part shown holds it (oracles_text_box_enlarge).  The panels at half the game's scale at the
 * presenter's unit over the picture, under the box; at once, four of their pixels for one by their mean. */
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
    /* At once, the picture with the panels folded at half: four of their pixels for one, by their mean. */
    uint32_t at[ORACLES_ENHANCED_FLOATING_PANELS][4];
    unsigned drawn = 0;
    const int halved = oracles_enhanced_view_floating_layer(c->view, at, &drawn) && oracles_enhanced_view_floating_scale(c->view) > 1u && !c->large_menus;
    uint32_t *folded = halved && k == 1u ? malloc((size_t)c->width * height * sizeof *folded) : NULL;
    if (folded) oracles_enhanced_view_fold_floating_at(c->view, surface, folded, 1u);
    fprintf(f, "P6\n%u %u\n255\n", w, h);
    for (unsigned y = 0; y < h; y++)
        for (unsigned x = 0; x < w; x++) {
            const int32_t px = (int32_t)x - ox, py = (int32_t)y - oy;
            uint32_t p = 0xff000000u;
            if (px >= 0 && py >= 0 && px < (int32_t)(part[2] * scale) && py < (int32_t)(part[3] * scale))
                p = surface[(part[1] + (unsigned)py / scale) * c->width + part[0] + (unsigned)px / scale];
            if (folded) p = folded[(size_t)y * c->width + x];
            else if (halved) {
                const uint32_t panel = half_panel_pixel(c, (int32_t)x, (int32_t)y, w, h), a = panel >> 24;
                uint32_t mixed = 0xff000000u;
                for (unsigned shift = 0; shift < 24u; shift += 8u)
                    mixed |= ((((panel >> shift) & 0xffu) * 255u + ((p >> shift) & 0xffu) * (255u - a) + 127u) / 255u) << shift;
                if (a) p = mixed;
            }
            if (box && (int32_t)x >= out[0] && (int32_t)x < out[0] + out[2] && (int32_t)y >= out[1] && (int32_t)y < out[1] + out[3])
                p = surface[(rect[1] + (unsigned)((int32_t)y - out[1]) / factor) * c->width + rect[0] + (unsigned)((int32_t)x - out[0]) / factor];
            const uint8_t rgb[3] = { (uint8_t)(p >> 16), (uint8_t)(p >> 8), (uint8_t)p };
            fwrite(rgb, 1, 3, f);
        }
    free(folded);
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
void oracles_enhanced_check_set_surface_scale(OraclesEnhancedCheck *c, unsigned scale)
{
    c->surface_scale = scale;
    oracles_enhanced_view_set_floating_screen(c->view, (float)scale);   /* the panels at half the game's scale laid out for that screen */
}
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
    free(c->redraw_layer);
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

/* The rooms of a dungeon the view gives as those its room in play opens onto, against the layout of that room's own
 * floor (w2DungeonLayout, $40 bytes a floor), wherever the layout names it, whatever the floor the game is on: each must
 * be the room the layout names beside it that way.  The game changes wDungeonFloor before it leaves the room by a hole
 * or Ages' stairs without a warp of their own, the room in play still the reference through the fade; the rooms
 * around it are still that floor's (enhanced.dungeon_neighbours_off_floor, compositions with one that is not). */
static void check_dungeon_neighbours(OraclesEnhancedCheck *c)
{
    uint8_t room, beside[4], layout[0x100];
    const unsigned sides = oracles_enhanced_view_dungeon_neighbours(c->view, &room, beside);
    if (!sides) return;
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    for (unsigned i = 0; i < sizeof layout; i++)
        layout[i] = oracles_guest_read8(c->guest, (OraclesGuestSym){ t->dungeon_layout.bank, (uint16_t)(t->dungeon_layout.addr + i) });
    for (unsigned d = 0; d < 4; d++) {
        if (!(sides & (1u << d))) continue;
        int found = 0;
        for (unsigned i = 0; i < sizeof layout && !found; i++) {
            if (layout[i] != room) continue;
            const unsigned col = i & 7u, row = (i >> 3) & 7u, floor = i & ~0x3fu;
            const int to_col = (int)col + (d == ORACLES_DIR_RIGHT) - (d == ORACLES_DIR_LEFT), to_row = (int)row + (d == ORACLES_DIR_DOWN) - (d == ORACLES_DIR_UP);
            found = to_col >= 0 && to_col < 8 && to_row >= 0 && to_row < 8 && layout[floor + (unsigned)to_row * 8u + (unsigned)to_col] == beside[d];
        }
        if (!found) { c->dungeon_neighbours_off_floor++; return; }
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
    if (c->hotbar_forget_at == frame && frame) oracles_enhanced_view_hotbar_forget(c->view);
    for (unsigned i = 0; i < c->hud_switches; i++)
        if (c->hud_switch_at[i] == frame) {
            const int floating = oracles_enhanced_view_hud(c->view) == ORACLES_ENHANCED_HUD_FLOATING;
            oracles_enhanced_view_set_hud(c->view, floating ? c->hud_base : ORACLES_ENHANCED_HUD_FLOATING);
            c->hud_redrawn += (unsigned)oracles_enhanced_view_redraw_hud(c->view);   /* as the pause shows it at once */
            c->hud_switched++;
        }
    const double compose_start = ec_monotonic_us();
    const uint32_t *surface = oracles_enhanced_view_compose(c->view, &mode, &camera_x, &camera_y);
    {   /* A renderer of objects is judged on the peak, which puts a frame late, not on the mean. */
        const double spent = ec_monotonic_us() - compose_start;
        c->compose_us_total += spent;
        if (spent > c->compose_us_max) c->compose_us_max = spent;
    }
    {   /* the floating panels the presenter lays over the band with the sub-pixel camera, folded in as it shows them (the
         * presenter's work, not the composition's) */
        int panels;
        surface = presented_surface(c, surface, &panels);
        c->floating_half_frames += (unsigned)(panels == 1);
        c->floating_layer_frames += (unsigned)(panels == 2);
    }
    c->band_top = oracles_enhanced_view_band_top(c->view);   /* the status bar floating or not, as the composition took it */
    c->panel_count = oracles_enhanced_view_floating_scale(c->view) == 1u ? oracles_enhanced_view_floating_panels(c->view, c->panels) : 0u;   /* else none in the surface */
    c->band_height = oracles_enhanced_view_height(c->view) - c->band_top;
    if (c->text_size) ec_check_scaled_text_box(c, mode, surface);
    if (c->text_size) ec_check_text_size_redraw(c, mode);
    if (c->wide) ec_check_wide_text_box(c, mode, surface, frame);   /* the frame just composed, before a reload replaces the state it shows */
    else if (c->host_box) ec_check_host_text_box(c, mode, frame);
    const uint64_t hash = presented_hash(c, surface);
    c->run_hash = oracles_guest_hash((const uint8_t *)&hash, sizeof hash, c->run_hash);
    c->last_hash = hash;
    for (unsigned i = 0; i < c->surfaces; i++) if (c->surface_at[i] == frame) write_surface(c, surface, c->surface_path[i]);
    measure_surfaces(c, surface, camera_x, camera_y, frame, mode == ORACLES_ENHANCED_WORLD);
    measure_band(c, surface, camera_x, camera_y, frame, mode == ORACLES_ENHANCED_WORLD);
    measure_floating(c, surface, mode, frame);
    c->frames++;
    const OraclesEnhancedObservation *ob = oracles_enhanced_view_observation(c->view);
    int blank = surface != NULL;   /* one colour: the screen faded out, where a camera that moves shows nothing moving */
    for (size_t i = 1, n = (size_t)c->width * oracles_enhanced_view_height(c->view); blank && i < n; i++) blank = surface[i] == surface[0];
    if (ob->rebase_x || ob->rebase_y) { c->loop_rebases++; ec_rebase(c, ob->rebase_x, ob->rebase_y); }
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
        const uint64_t band = oracles_guest_hash((const uint8_t *)surface, (size_t)c->width * c->band_top * sizeof *surface, ORACLES_HASH_SEED);
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
    check_dungeon_neighbours(c);
    ec_reload_step(c, frame, mode, camera_x, shown, room);
    ec_measure_scroll(c, ob, mode, uncovered);
    ec_measure_seams(c, frame, ob, mode);
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
