/* The drawn-back view without a ROM: the geometry of a 480x270
 * surface in the camera's world math, the compositor and the view on a
 * synthetic core, the refusal of a savestate taken at the other size, and,
 * at both sizes, the scenes of both games' intros the band shows or frames, and the
 * ghost's captures of a fade, never kept. */
#include "camera.h"
#include "compositor.h"
#include "guest_struct_offsets.h"
#include "view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const OraclesCompatProfile *original_profile(OraclesGame game)
{
    const OraclesRomInfo info = { .game = game, .revision = game == ORACLES_GAME_SEASONS ? ORACLES_ROM_REVISION_SEASONS_US : ORACLES_ROM_REVISION_AGES_US, .known = 1 };
    return oracles_compat_find(&info);
}

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

#define W 480u
#define H 270u
#define BAND_H 254u
#define HUD_X 160u

static void world_math(void)
{
    OraclesEnhancedWorld w;
    /* Outdoors the map is the extent at any band: the viewport only says whether it pans. */
    CHECK(oracles_enhanced_world_from_room(0, 0x23, 0, 40, 50, 16, 14, 0, 0, 0, 0, 0, 0x23, W, &w) == 1);
    CHECK(w.bounds_width == 14 * 160 && w.bounds_origin_x == 0 && w.wide && w.viewport == 480);
    CHECK(oracles_enhanced_world_from_room(1, 0x23, 0, 40, 50, 16, 14, 0, 0, 0, 0, 0, 0x23, W, &w) == 1 && w.viewport == 480);
    CHECK(oracles_enhanced_world_from_room(0, 0x23, 0, 40, 50, 16, 14, 0, 0, 0, 0, 0, 0x23, 0, &w) == 1 && w.viewport == 256);
    /* A room shown alone outdoors (the Maku tree's screen) stands centred in the whole band. */
    CHECK(oracles_enhanced_world_from_room(0, 0x38, 0, 80, 40, 16, 14, 0, 0, 0, 0, 1, 0x38, W, &w) == 1);
    CHECK(w.bounds_width == 480 && w.bounds_origin_x == 8 * 160 - 160 && w.wide);
    /* An interior keeps the normal band's 256. */
    CHECK(oracles_enhanced_world_from_room(2, 0x11, 0, 80, 40, 16, 14, 0, 0, 0, 0, 0, 0x11, W, &w) == 1);
    CHECK(w.bounds_width == 256 && w.bounds_origin_x == 160 - 48 && w.wide && w.viewport == 256);
    /* A room of Ages' open sea (group 2, rows 9 to 13, columns 0 to 13): the sea is its extent, the whole band its viewport... */
    const uint8_t sea[4] = { 0, 9, 14, 5 };
    CHECK(oracles_enhanced_world_from_room(2, 0xb6, 0, 80, 64, 16, 14, 0, 0, 0, 0xfu, 0, 0xb6, W, &w) == 1 && w.viewport == 256);
    oracles_enhanced_world_extend_to_sea(&w, sea, W);
    CHECK(w.viewport == 480 && w.wide && w.bounds_origin_x == 0 && w.bounds_width == 14 * 160 && w.bounds_origin_y == 9 * 128 && w.bounds_height == 5 * 128 && !w.alone);
    CHECK(w.link_x == 6 * 160 + 80 && w.link_y == 11 * 128 + 64);
    /* ... but in the normal band, which keeps the room and the sea beside it. */
    CHECK(oracles_enhanced_world_from_room(2, 0xb6, 0, 80, 64, 16, 14, 0, 0, 0, 0xfu, 0, 0xb6, 0, &w) == 1);
    const OraclesEnhancedWorld normal = w;
    oracles_enhanced_world_extend_to_sea(&w, sea, 0);
    CHECK(memcmp(&w, &normal, sizeof w) == 0 && w.bounds_width == 3 * 160 && w.viewport == 256);
}

static void compositor(void)
{
    uint32_t *core = malloc(160u * 144u * sizeof *core);
    for (unsigned y = 0; y < 144u; y++)
        for (unsigned x = 0; x < 160u; x++) core[y * 160u + x] = 0xff000000u | ((uint32_t)x << 8) | y;
    uint32_t *surface = malloc(ORACLES_ENHANCED_ZOOM_WIDTH * ORACLES_ENHANCED_ZOOM_HEIGHT * sizeof *surface);
    CHECK(oracles_enhanced_size(1).width == W && oracles_enhanced_size(1).height == H);
    CHECK(oracles_enhanced_band_height(oracles_enhanced_size(1)) == BAND_H && oracles_enhanced_hud_x(oracles_enhanced_size(1)) == HUD_X);

    /* Framed: the core's image centred both ways, the border around it. */
    OraclesEnhancedCompose in;
    memset(&in, 0, sizeof in);
    in.mode = ORACLES_ENHANCED_FRAMED;
    in.border = 0xff112233u;
    in.size = oracles_enhanced_size(1);
    const unsigned top = (H - 144u) / 2u;
    CHECK(oracles_enhanced_compose(core, &in, surface) == 0);
    CHECK(surface[top * W + HUD_X] == core[0] && surface[(top + 143u) * W + HUD_X + 159u] == core[143u * 160u + 159u]);
    CHECK(surface[(top - 1u) * W + HUD_X] == 0xff112233u && surface[top * W + HUD_X - 1u] == 0xff112233u && surface[0] == 0xff112233u);

    /* World: the status bar centred at 160; the window where the camera puts it; the rest of the band black, counted. */
    in.mode = ORACLES_ENHANCED_WORLD;
    in.window_world_left = 1000; in.window_world_top = 500;
    in.world_left = 1000 - 160; in.world_top = 500 - 63;
    CHECK(oracles_enhanced_compose(core, &in, surface) == W * BAND_H - 160u * 128u);
    CHECK(surface[HUD_X] == core[0] && surface[HUD_X - 1u] == 0xff112233u);
    CHECK(surface[(16u + 63u) * W + HUD_X] == core[16u * 160u] && surface[(16u + 63u) * W + HUD_X - 1u] == 0xff000000u);
    /* The normal band shown alone, centred (an interior): black around it, and not counted. */
    in.shown_width = 256u; in.shown_height = 128u;
    CHECK(oracles_enhanced_compose(core, &in, surface) == 256u * 128u - 160u * 128u);
    CHECK(surface[(16u + 63u) * W + 112u] == 0xff000000u && surface[(16u + 63u) * W + HUD_X] == core[16u * 160u]);
    /* A large room's own rectangle counted, the black around it decided: here the window, 160 by 128, and 40 lines below it uncovered. */
    in.shown_width = 0; in.shown_height = 0;
    in.counted_left = 1000; in.counted_top = 500; in.counted_width = 160u; in.counted_height = 168u;
    CHECK(oracles_enhanced_compose(core, &in, surface) == 160u * 40u);
    in.counted_width = 0;
    /* Two neighbours over the same pixels: the first wins, then the second where the first does not reach. */
    {
        static uint32_t first[80u * 128u], second[160u * 128u];
        for (unsigned i = 0; i < 80u * 128u; i++) first[i] = 0xff0000aau;
        for (unsigned i = 0; i < 160u * 128u; i++) second[i] = 0xff00bb00u;
        const OraclesEnhancedNeighbour two[2] = { { 1160, 500, first, 80u, 128u, 1 }, { 1160, 500, second, 160u, 128u, 1 } };
        in.neighbours = two; in.neighbour_count = 2;
        CHECK(oracles_enhanced_compose(core, &in, surface) == W * BAND_H - 320u * 128u);
        CHECK(surface[(16u + 63u) * W + HUD_X + 170u] == 0xff0000aau && surface[(16u + 63u) * W + HUD_X + 250u] == 0xff00bb00u);
        in.neighbours = NULL; in.neighbour_count = 0;
    }
    free(core); free(surface);
}

/* The game's underwater ripple over the whole 254-line band.  The game reads
 * a table of 128 steps modulo 128, one step a line from a phase
 * (checkUpdateUnderwaterWaves): the window's 128 lines are one period, and a
 * row above or below the window ripples with the line a taller screen would
 * have there.  Every source holds the same world function, so the band must
 * be that function shifted by the row's line modulo 128, on every row. */
static void ripple(void)
{
    #define WORLD_PIXEL(wx, wy) (0xff000000u | ((uint32_t)((wx) & 0xfff) << 8) | (uint32_t)((wy) & 0xff))
    static const int8_t wave[8] = { 0, 1, 2, 3, 3, 2, 1, 0 };
    int16_t shift_x[128], shift_y[128];
    for (unsigned line = 0; line < 128u; line++) {   /* a steep wave on a slower one: no period shorter than 128 */
        shift_x[line] = (int16_t)(wave[line % 8u] + (int)(line / 32u));
        shift_y[line] = (int16_t)(line / 64u);
    }
    const int32_t window_left = 2000, window_top = 1000;
    const int32_t world_left = window_left - 160, world_top = window_top - 70;   /* 70 rows above the window, 56 below */
    uint32_t *core = malloc(160u * 144u * sizeof *core);
    for (unsigned y = 0; y < 144u; y++)
        for (unsigned x = 0; x < 160u; x++)
            core[y * 160u + x] = y < 16u ? 0xff000000u : WORLD_PIXEL(window_left + (int32_t)x + shift_x[y - 16u], window_top + (int32_t)(y - 16u) + shift_y[y - 16u]);
    const int32_t nb_left = world_left - 16, nb_top = world_top - 16;
    const unsigned nb_w = W + 32u, nb_h = BAND_H + 32u;
    uint32_t *area = malloc((size_t)nb_w * nb_h * sizeof *area);
    for (unsigned ny = 0; ny < nb_h; ny++)
        for (unsigned nx = 0; nx < nb_w; nx++) area[(size_t)ny * nb_w + nx] = WORLD_PIXEL(nb_left + (int32_t)nx, nb_top + (int32_t)ny);
    const OraclesEnhancedNeighbour around = { nb_left, nb_top, area, nb_w, nb_h, 1 };
    OraclesEnhancedCompose in;
    memset(&in, 0, sizeof in);
    in.mode = ORACLES_ENHANCED_WORLD;
    in.size = oracles_enhanced_size(1);
    in.window_world_left = window_left; in.window_world_top = window_top;
    in.world_left = world_left; in.world_top = world_top;
    in.line_shift = shift_x; in.line_shift_y = shift_y;
    in.neighbours = &around; in.neighbour_count = 1;
    uint32_t *surface = malloc(W * H * sizeof *surface);
    CHECK(oracles_enhanced_compose(core, &in, surface) == 0);
    unsigned wrong = 0;
    for (unsigned y = 0; y < BAND_H; y++) {
        const int32_t row_line = world_top + (int32_t)y - window_top, line = ((row_line % 128) + 128) % 128;
        for (unsigned x = 0; x < W; x++) {
            const uint32_t expect = WORLD_PIXEL(world_left + (int32_t)x + shift_x[line], world_top + (int32_t)y + shift_y[line]);
            if (surface[(16u + y) * W + x] != expect) wrong++;
        }
    }
    CHECK(wrong == 0);
    free(surface); free(area); free(core);
    #undef WORLD_PIXEL
}

/* A synthetic core running `jr -2`, whose WRAM the test writes as the game would. */
typedef struct rig { uint8_t *rom; OraclesCore *core; OraclesGuest *guest; } rig;
static int rig_start_game(rig *r, OraclesGame game)
{
    const size_t size = 1024u * 1024u;
    r->rom = calloc(size, 1);
    memcpy(r->rom + 0x134, "ZELDA NAYRU", 11);
    r->rom[0x143] = 0xc0; r->rom[0x147] = 0x1b; r->rom[0x148] = 0x05; r->rom[0x149] = 0x02;
    r->rom[0x100] = 0x00; r->rom[0x101] = 0xc3; r->rom[0x102] = 0x50; r->rom[0x103] = 0x01;
    r->rom[0x150] = 0x18; r->rom[0x151] = 0xfe;
    const OraclesCoreOptions options = { 0, 0, ORACLES_CORE_SAMEBOY };
    r->core = oracles_core_create(r->rom, size, &options);
    r->guest = r->core ? oracles_guest_attach(r->core, original_profile(game)) : NULL;
    return r->guest != NULL;
}
static int rig_start(rig *r) { return rig_start_game(r, ORACLES_GAME_AGES); }
static void rig_stop(rig *r)
{
    if (r->guest) oracles_guest_detach(r->guest);
    if (r->core) oracles_core_destroy(r->core);
    free(r->rom);
}

static void frames(rig *r, OraclesEnhancedView *view, unsigned n, OraclesEnhancedMode *mode, int32_t *cam, int32_t *cam_y)
{
    for (unsigned i = 0; i < n; i++) { oracles_core_run_frame(r->core); oracles_enhanced_view_compose(view, mode, cam, cam_y); }
}

static void view_and_savestate(void)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    OraclesEnhancedView *zoom = r.guest ? oracles_enhanced_view_start(r.core, r.guest, NULL, 0) : NULL;
    OraclesEnhancedView *normal = r.guest ? oracles_enhanced_view_start(r.core, r.guest, NULL, 0) : NULL;
    CHECK(zoom && normal);
    if (!zoom || !normal) { rig_stop(&r); return; }
    oracles_enhanced_view_set_zoom_out(zoom, 1);
    CHECK(oracles_enhanced_view_width(zoom) == W && oracles_enhanced_view_height(zoom) == H);
    CHECK(oracles_enhanced_view_width(normal) == 256u && oracles_enhanced_view_height(normal) == 144u);

    /* Outdoors the whole band is shown and Link stands at its middle. */
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    uint8_t *w0 = oracles_guest_wram_writable(r.guest, 0), *w1 = oracles_guest_wram_writable(r.guest, 1);
    uint8_t *link = w1 + (ORACLES_OBJECTS_BASE - 0xd000u);
    #define W0(sym) w0[(sym).addr - 0xc000u]
    W0(t->game_state) = 2; W0(t->cutscene_index) = 1; W0(t->scroll_mode) = 1; W0(t->screen_transition_state) = 2;
    W0(t->active_group) = 0; W0(t->active_room) = 0x45; W0(t->room_is_large) = 0;
    link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_STATE] = 1; link[ORACLES_OBJ_XH] = 80; link[ORACLES_OBJ_YH] = 64;
    OraclesEnhancedMode mode = ORACLES_ENHANCED_FRAMED;
    int32_t cam = 0, cam_y = 0;
    unsigned shown_w = 0, shown_h = 0;
    frames(&r, zoom, 40, &mode, &cam, &cam_y);
    oracles_enhanced_view_shown(zoom, &shown_w, &shown_h);
    CHECK(mode == ORACLES_ENHANCED_WORLD && shown_w == W && shown_h == BAND_H);
    CHECK(cam == 5 * 160 + 80 - 240 && cam_y == 4 * 128 + 64 - 127);

    /* The state names its size; the other size refuses it, and says why. */
    uint8_t wire[1024];
    size_t written = 0;
    char why[256];
    CHECK(oracles_enhanced_view_save_state(zoom, wire, sizeof wire, &written) == 0 && written == 2u * ORACLES_E11_STATE_WIRE_SIZE + 12u + 8u);
    CHECK(oracles_enhanced_view_check_state(zoom, wire, written, why, sizeof why) == 0);
    CHECK(oracles_enhanced_view_load_state(zoom, wire, written) == 0);
    /* It names both surfaces by the view's level, as Display does, and the screen's shape where the level has two. */
    CHECK(oracles_enhanced_view_check_state(normal, wire, written, why, sizeof why) != 0);
    CHECK(!strcmp(why, "the savestate was taken on the far view, 480x270, "
                       "and this session shows the near view, 256x144 in 16:9: it loads only on its own surface"));
    CHECK(oracles_enhanced_view_load_state(normal, wire, written) != 0);
    CHECK(oracles_enhanced_view_save_state(normal, wire, sizeof wire, &written) == 0 && written == 2u * ORACLES_E11_STATE_WIRE_SIZE + 12u);
    CHECK(oracles_enhanced_view_check_state(zoom, wire, written, why, sizeof why) != 0 && strstr(why, "the near view, 256x144 in 16:9, and this session shows the far view"));
    CHECK(oracles_enhanced_view_load_state(zoom, wire, written) != 0);
    CHECK(oracles_enhanced_view_load_state(zoom, wire, 2u * ORACLES_E11_STATE_WIRE_SIZE) != 0);   /* a state from before the size record: the normal surface's */
    CHECK(oracles_enhanced_view_load_state(normal, wire, written) == 0);

    /* Indoors the normal band, centred in the surface's: the room's left edge at 160 + 48, its top at 63. */
    W0(t->scroll_mode) = 2; frames(&r, zoom, 1, &mode, &cam, &cam_y);
    W0(t->active_group) = 2; W0(t->active_room) = 0x10;
    frames(&r, zoom, 1, &mode, &cam, &cam_y);
    W0(t->scroll_mode) = 1;
    frames(&r, zoom, 40, &mode, &cam, &cam_y);
    oracles_enhanced_view_shown(zoom, &shown_w, &shown_h);
    CHECK(mode == ORACLES_ENHANCED_WORLD && shown_w == 256u && shown_h == 128u);
    CHECK(cam == 0 - 48 - 112 && cam_y == 128 - 63);
    const uint32_t *surface = oracles_enhanced_view_surface(zoom);
    const uint32_t black = surface[16u * W];   /* the band's corner, outside the part shown: black, with the game's fade */
    CHECK(surface[(16u + 10u) * W + 111u] == black && surface[(16u + 62u) * W + 240u] == black && surface[(16u + 191u) * W + 240u] == black);

    /* A large room (a dungeon's, 4:10 at column 0 and row 1 of 240x176 cells):
     * the whole band shown, the room centred both ways in it. */
    W0(t->scroll_mode) = 2; frames(&r, zoom, 1, &mode, &cam, &cam_y);
    W0(t->active_group) = 4; W0(t->active_room) = 0x10; W0(t->dungeon_index) = 0xff;
    frames(&r, zoom, 1, &mode, &cam, &cam_y);
    W0(t->scroll_mode) = 1;
    frames(&r, zoom, 40, &mode, &cam, &cam_y);
    oracles_enhanced_view_shown(zoom, &shown_w, &shown_h);
    CHECK(mode == ORACLES_ENHANCED_WORLD && shown_w == W && shown_h == BAND_H);
    CHECK(cam == 0 - 120 && cam_y == 176 - 39);
    #undef W0
    oracles_enhanced_view_stop(zoom);
    oracles_enhanced_view_stop(normal);
    rig_stop(&r);
}

/* A room of the overworld shown alone (the Maku tree's screen), drawn back:
 * the camera follows it the same whether its edges are open or not, so that a
 * scroll opening one moves the band without a jump, on either axis. */
static void isolated_room_edges(void)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    OraclesEnhancedCamera *camera = r.guest ? oracles_enhanced_camera_start(r.guest) : NULL;
    CHECK(camera != NULL);
    if (!camera) { rig_stop(&r); return; }
    oracles_enhanced_camera_set_band(camera, W, BAND_H);
    OraclesEnhancedObservation ob;
    memset(&ob, 0, sizeof ob);
    ob.playing = 1; ob.grid = 1; ob.epoch = 8;
    int32_t cam = 0, cam_y = 0, last = 0, last_y = 0, worst = 0, worst_y = 0;
    unsigned lost = 0;
    for (uint32_t frame = 0; frame < 180u; frame++) {
        const unsigned edges = frame < 60u ? 0u : frame < 120u ? 1u : 1u | 8u;   /* then up, then up and left */
        CHECK(oracles_enhanced_world_from_room(0, 0x38, 0, 80, 20, 16, 14, 0, 0, 0, edges, 1, 0x38, W, &ob.world) == 1);
        ob.window_left = ob.world.origin_x; ob.window_top = ob.world.origin_y;
        if (!oracles_enhanced_camera_reduce(camera, frame, &ob, &cam, &cam_y)) lost++;
        if (frame > 0) {
            const int32_t d = cam > last ? cam - last : last - cam, dy = cam_y > last_y ? cam_y - last_y : last_y - cam_y;
            if (d > worst) worst = d;
            if (dy > worst_y) worst_y = dy;
        }
        last = cam; last_y = cam_y;
        if (frame == 59u) CHECK(cam_y == 3 * 128 - 63);   /* closed: the room centred in the band */
    }
    CHECK(lost == 0 && worst <= 4 && worst_y <= 4);
    CHECK(oracles_enhanced_camera_vertical_tracking(camera));
    oracles_enhanced_camera_stop(camera);
    rig_stop(&r);
}

/* A camera state saved outdoors, loaded while the camera frames a house: it
 * resumes where it was, the reducers not restarted for the change of
 * viewport the load brings. */
static void state_across_viewports(void)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    OraclesEnhancedCamera *camera = r.guest ? oracles_enhanced_camera_start(r.guest) : NULL;
    CHECK(camera != NULL);
    if (!camera) { rig_stop(&r); return; }
    oracles_enhanced_camera_set_band(camera, W, BAND_H);
    OraclesEnhancedObservation out, in;
    memset(&out, 0, sizeof out);
    out.playing = 1; out.grid = 1; out.epoch = 8;
    in = out; in.epoch = 16 + 2;   /* a warp into the interiors' domain */
    int32_t cam = 0, cam_y = 0;
    uint32_t frame = 0;
    for (int x = 10; frame < 45u; frame++, x += 3) {   /* Link runs right: the camera trails him */
        CHECK(oracles_enhanced_world_from_room(0, 0x45, 0, x, 64, 16, 14, 0, 0, 0, 0, 0, 0x45, W, &out.world) == 1);
        oracles_enhanced_camera_reduce(camera, frame, &out, &cam, &cam_y);
    }
    const int32_t saved = cam, centred = out.world.link_x - 240;
    const OraclesE11State state = *oracles_enhanced_camera_state(camera), vertical = *oracles_enhanced_camera_state_vertical(camera);
    CHECK(saved - centred > 4 || centred - saved > 4);   /* a restart would bootstrap there, farther than a step of the reducer */
    CHECK(oracles_enhanced_world_from_room(2, 0x10, 0, 80, 64, 16, 14, 0, 0, 0, 0, 0, 0x10, W, &in.world) == 1);
    for (unsigned i = 0; i < 20u; i++, frame++) oracles_enhanced_camera_reduce(camera, frame, &in, &cam, &cam_y);
    oracles_enhanced_camera_set_state(camera, &state, &vertical);
    CHECK(oracles_enhanced_camera_reduce(camera, (uint32_t)state.last_ordinal + 1u, &out, &cam, &cam_y) == 1 && cam - saved <= 4 && saved - cam <= 4);
    oracles_enhanced_camera_stop(camera);
    rig_stop(&r);
}

/* The scenes the game plays in its own rooms (Ages' intro and cutscenes),
 * at both sizes: a scene in the world stays in the band, the pregame intro,
 * drawn on a screen the game cleared, is framed; Link taken away by a
 * scene holds the camera on his last place, or frames the rooms he has none in. */
static void cutscenes(void)
{
    for (int drawn_back = 0; drawn_back < 2; drawn_back++) {
        rig r;
        memset(&r, 0, sizeof r);
        CHECK(rig_start(&r));
        OraclesEnhancedView *view = r.guest ? oracles_enhanced_view_start(r.core, r.guest, NULL, 0) : NULL;
        CHECK(view != NULL);
        if (!view) { rig_stop(&r); return; }
        oracles_enhanced_view_set_zoom_out(view, drawn_back);
        const OraclesGuestTables *t = &oracles_guest_tables_ages;
        uint8_t *w0 = oracles_guest_wram_writable(r.guest, 0), *w1 = oracles_guest_wram_writable(r.guest, 1);
        uint8_t *link = w1 + (ORACLES_OBJECTS_BASE - 0xd000u);
        #define W0(sym) w0[(sym).addr - 0xc000u]
        W0(t->game_state) = 2; W0(t->cutscene_index) = 1; W0(t->scroll_mode) = 1; W0(t->screen_transition_state) = 2;
        W0(t->active_group) = 0; W0(t->active_room) = 0x45; W0(t->room_is_large) = 0;
        link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_STATE] = 1; link[ORACLES_OBJ_XH] = 80; link[ORACLES_OBJ_YH] = 64;
        OraclesEnhancedMode mode = ORACLES_ENHANCED_FRAMED;
        int32_t cam = 0, cam_y = 0;
        frames(&r, view, 40, &mode, &cam, &cam_y);
        CHECK(mode == ORACLES_ENHANCED_WORLD);
        const int32_t cam0 = cam, cam_y0 = cam_y;

        /* The pregame intro (CUTSCENE_PREGAME_INTRO): its orb over a cleared
         * screen, the game's scroll stopped (wScrollMode 0), in the room loaded. */
        W0(t->cutscene_index) = t->cutscene_pregame_intro; W0(t->scroll_mode) = 0;
        unsigned world = 0;
        for (unsigned i = 0; i < 10u; i++) { frames(&r, view, 1, &mode, &cam, &cam_y); world += mode == ORACLES_ENHANCED_WORLD; }
        CHECK(world == 0);
        W0(t->cutscene_index) = 1; W0(t->scroll_mode) = 1;
        frames(&r, view, 5, &mode, &cam, &cam_y);
        CHECK(mode == ORACLES_ENHANCED_WORLD && cam == cam0 && cam_y == cam_y0);

        /* A scene played in the room itself (Nayru's song, CUTSCENE_NAYRU_SINGING): the band. */
        W0(t->cutscene_index) = 6;
        for (unsigned i = 0; i < 10u; i++) { frames(&r, view, 1, &mode, &cam, &cam_y); world += mode == ORACLES_ENHANCED_WORLD; }
        CHECK(world == 10u && cam == cam0 && cam_y == cam_y0);

        /* The song takes Link away: his object cleared (enabled 0, at the
         * room's corner), first in his room (the time portal does the same),
         * where the camera holds on his last place... */
        link[ORACLES_OBJ_ENABLED] = 0; link[ORACLES_OBJ_XH] = 0; link[ORACLES_OBJ_YH] = 0;
        unsigned moved = 0;
        for (unsigned i = 0; i < 60u; i++) { frames(&r, view, 1, &mode, &cam, &cam_y); moved += mode != ORACLES_ENHANCED_WORLD || cam != cam0 || cam_y != cam_y0; }
        CHECK(moved == 0);
        /* ... then in the rooms the song shows (0:98), where he has no place:
         * framed, never the band centred on the corner ... */
        W0(t->active_room) = 0x98;
        world = 0;
        for (unsigned i = 0; i < 30u; i++) { frames(&r, view, 1, &mode, &cam, &cam_y); world += mode == ORACLES_ENHANCED_WORLD; }
        CHECK(world == 0);
        /* ... and back in his room, the band on his last place, where he appears. */
        W0(t->active_room) = 0x45;
        for (unsigned i = 0; i < 30u; i++) { frames(&r, view, 1, &mode, &cam, &cam_y); moved += mode != ORACLES_ENHANCED_WORLD || cam != cam0 || cam_y != cam_y0; }
        link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_XH] = 80; link[ORACLES_OBJ_YH] = 64;
        for (unsigned i = 0; i < 30u; i++) { frames(&r, view, 1, &mode, &cam, &cam_y); moved += mode != ORACLES_ENHANCED_WORLD || cam != cam0 || cam_y != cam_y0; }
        CHECK(moved == 0);

        /* Out of play (a file loading, wGameState 3) the game sets Link's
         * place while his object is still disabled: that place is read, and
         * back in play, Link warping in, it is the one that holds, so that
         * the observed Link does not jump when he steps out of the warp. */
        W0(t->game_state) = 3;
        link[ORACLES_OBJ_ENABLED] = 0; link[ORACLES_OBJ_XH] = 24; link[ORACLES_OBJ_YH] = 24;
        frames(&r, view, 10, &mode, &cam, &cam_y);
        W0(t->game_state) = 2; link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_STATE] = 0x0a;   /* LINK_STATE_WARPING */
        const OraclesEnhancedObservation *ob = oracles_enhanced_view_observation(view);
        int32_t last_x = 0, last_y = 0, jump = 0;
        for (unsigned i = 0; i < 20u; i++) {
            if (i == 5u) link[ORACLES_OBJ_STATE] = 1;
            frames(&r, view, 1, &mode, &cam, &cam_y);
            const int32_t dx = ob->world.link_x - last_x, dy = ob->world.link_y - last_y;
            if (i > 0u && (dx > 8 || dx < -8 || dy > 8 || dy < -8)) jump++;
            last_x = ob->world.link_x; last_y = ob->world.link_y;
        }
        CHECK(jump == 0 && last_x == 5 * 160 + 24 && last_y == 4 * 128 + 24);
        #undef W0
        oracles_enhanced_view_stop(view);
        rig_stop(&r);
    }
}

/* Seasons' intro, at both sizes: Din's dance is played in the room and stays
 * in the band; Din taken to Onox's castle (the tower rising under a dark sky),
 * the temple sinking and the pregame intro draw their own screens and are
 * framed.  The indices are Seasons' alone: Ages has none of them. */
static void seasons_scenes(void)
{
    CHECK(oracles_guest_tables_ages.cutscene_din_imprisoned == 0xffu && oracles_guest_tables_ages.cutscene_temple_sinking == 0xffu && oracles_guest_tables_ages.cutscene_onox_taunting == 0xffu);
    for (int drawn_back = 0; drawn_back < 2; drawn_back++) {
        rig r;
        memset(&r, 0, sizeof r);
        CHECK(rig_start_game(&r, ORACLES_GAME_SEASONS));
        OraclesEnhancedView *view = r.guest ? oracles_enhanced_view_start(r.core, r.guest, NULL, 0) : NULL;
        CHECK(view != NULL);
        if (!view) { rig_stop(&r); return; }
        oracles_enhanced_view_set_zoom_out(view, drawn_back);
        const OraclesGuestTables *t = &oracles_guest_tables_seasons;
        uint8_t *w0 = oracles_guest_wram_writable(r.guest, 0), *w1 = oracles_guest_wram_writable(r.guest, 1);
        uint8_t *link = w1 + (ORACLES_OBJECTS_BASE - 0xd000u);
        #define W0(sym) w0[(sym).addr - 0xc000u]
        W0(t->game_state) = 2; W0(t->cutscene_index) = 1; W0(t->scroll_mode) = 1; W0(t->screen_transition_state) = 2;
        W0(t->active_group) = 0; W0(t->active_room) = 0x98; W0(t->room_is_large) = 0;
        link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_STATE] = 1; link[ORACLES_OBJ_XH] = 80; link[ORACLES_OBJ_YH] = 64;
        OraclesEnhancedMode mode = ORACLES_ENHANCED_FRAMED;
        int32_t cam = 0, cam_y = 0;
        frames(&r, view, 40, &mode, &cam, &cam_y);
        CHECK(mode == ORACLES_ENHANCED_WORLD);
        const uint8_t scenes[4] = { t->cutscene_din_imprisoned, t->cutscene_temple_sinking, t->cutscene_pregame_intro, t->cutscene_onox_taunting };
        W0(t->cutscene_index) = 6;   /* CUTSCENE_S_DIN_DANCING */
        unsigned world = 0;
        for (unsigned i = 0; i < 10u; i++) { frames(&r, view, 1, &mode, &cam, &cam_y); world += mode == ORACLES_ENHANCED_WORLD; }
        CHECK(world == 10u);
        for (unsigned k = 0; k < 4u; k++) {
            W0(t->cutscene_index) = scenes[k];   /* the game keeps wScrollMode 1 through most of them */
            world = 0;
            for (unsigned i = 0; i < 10u; i++) { frames(&r, view, 1, &mode, &cam, &cam_y); world += mode == ORACLES_ENHANCED_WORLD; }
            CHECK(world == 0);
        }
        W0(t->cutscene_index) = 1;
        frames(&r, view, 5, &mode, &cam, &cam_y);
        CHECK(mode == ORACLES_ENHANCED_WORLD);
        #undef W0
        oracles_enhanced_view_stop(view);
        rig_stop(&r);
    }
}

/* The black the band shows, counted by the view for the harness and the
 * launcher: without a ghost every room around the one in play stays black,
 * and each is a room black whole once past the threshold, counted from the
 * first frame it was shown so; a framed frame ends the runs. */
static void black_count(void)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    OraclesEnhancedView *view = r.guest ? oracles_enhanced_view_start(r.core, r.guest, NULL, 0) : NULL;
    CHECK(view != NULL);
    if (!view) { rig_stop(&r); return; }
    oracles_enhanced_view_set_zoom_out(view, 1);
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    uint8_t *w0 = oracles_guest_wram_writable(r.guest, 0), *w1 = oracles_guest_wram_writable(r.guest, 1);
    uint8_t *link = w1 + (ORACLES_OBJECTS_BASE - 0xd000u);
    #define W0(sym) w0[(sym).addr - 0xc000u]
    W0(t->game_state) = 2; W0(t->cutscene_index) = 1; W0(t->scroll_mode) = 1; W0(t->screen_transition_state) = 2;
    W0(t->active_group) = 0; W0(t->active_room) = 0x45; W0(t->room_is_large) = 0;
    link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_STATE] = 1; link[ORACLES_OBJ_XH] = 80; link[ORACLES_OBJ_YH] = 64;
    OraclesEnhancedMode mode = ORACLES_ENHANCED_FRAMED;
    int32_t cam = 0, cam_y = 0;
    OraclesEnhancedBlack black;
    unsigned world = 0;
    for (unsigned i = 0; i < 100u; i++) { frames(&r, view, 1, &mode, &cam, &cam_y); world += mode == ORACLES_ENHANCED_WORLD; }
    oracles_enhanced_view_black(view, &black);
    CHECK(world > 60u && black.have_world && !black.have_full && black.rooms_over == 0);   /* never a full frame: no mean yet */
    frames(&r, view, 90, &mode, &cam, &cam_y);
    oracles_enhanced_view_black(view, &black);
    CHECK(mode == ORACLES_ENHANCED_WORLD && black.rooms_over >= 8u);   /* the eight rooms around it, and more drawn back */
    CHECK(black.longest[0].group == 0 && black.longest[0].room != 0x45 && black.longest[0].frames > ORACLES_ENHANCED_BLACK_ROOM_FRAMES);
    CHECK(black.longest[0].from == black.first_world && black.longest[0].frames == 190u - black.first_world);
    const unsigned over = black.rooms_over;
    W0(t->opened_menu_type) = 1;   /* framed: the runs end where they stood */
    frames(&r, view, 5, &mode, &cam, &cam_y);
    oracles_enhanced_view_black(view, &black);
    CHECK(mode == ORACLES_ENHANCED_FRAMED && black.rooms_over == over);
    #undef W0
    oracles_enhanced_view_stop(view);
    rig_stop(&r);
}

/* A neighbour's settled state that may start a run while a cutscene holds
 * the game: its key the live one but for other rooms' flags (0:98 visited
 * since, as Din dances there), never the room's own nor anything else. */
static void parent_keys(void)
{
    uint8_t parent[64], live[64];
    for (unsigned i = 0; i < 64u; i++) parent[i] = live[i] = (uint8_t)(i * 7u);
    const long flags_at = 16, own = 16 + 0x17;   /* four pages of room flags would be longer: a short run of them serves */
    CHECK(oracles_enhanced_parent_key_holds(parent, live, 64u, flags_at, 32u, own));
    live[16 + 0x18] ^= 0x50u;                    /* another room visited since: holds */
    CHECK(oracles_enhanced_parent_key_holds(parent, live, 64u, flags_at, 32u, own));
    live[own] ^= 0x10u;                          /* the room's own flags: not */
    CHECK(!oracles_enhanced_parent_key_holds(parent, live, 64u, flags_at, 32u, own));
    live[own] ^= 0x10u; live[3] ^= 1u;           /* a byte outside the flags: not */
    CHECK(!oracles_enhanced_parent_key_holds(parent, live, 64u, flags_at, 32u, own));
    live[3] ^= 1u; live[16 + 32] ^= 1u;          /* the byte just past them: not */
    CHECK(!oracles_enhanced_parent_key_holds(parent, live, 64u, flags_at, 32u, own));
    live[16 + 32] ^= 1u;
    CHECK(!oracles_enhanced_parent_key_holds(parent, live, 64u, -1, 0u, own));   /* the flags not in the key: only an equal key */
    live[16 + 0x18] ^= 0x50u;
    CHECK(oracles_enhanced_parent_key_holds(parent, live, 64u, -1, 0u, -1));
}

/* A room run from beside during a cutscene, kept only if the bytes its run
 * read are the live ones, the visited bit of its own flags aside. */
static void beside_reads(void)
{
    const uint16_t addresses[3] = { 0xc797, 0xc798, 0xcc4e };   /* its own flags, a neighbour's, the season */
    uint8_t seen[3] = { 0x00, 0x00, 0x02 }, live[3] = { 0x10, 0x00, 0x02 };
    CHECK(oracles_enhanced_reads_hold(3, addresses, seen, live, 0xc797, 0x10));    /* only its visited bit: holds */
    live[1] = 0x50;
    CHECK(!oracles_enhanced_reads_hold(3, addresses, seen, live, 0xc797, 0x10));   /* a flag it read changed since: refused */
    live[1] = 0x10;
    CHECK(!oracles_enhanced_reads_hold(3, addresses, seen, live, 0xc797, 0x10));   /* another room's visited bit: refused, the bit is its own room's alone */
    live[1] = 0x00; live[0] = 0x30;
    CHECK(!oracles_enhanced_reads_hold(3, addresses, seen, live, 0xc797, 0x10));   /* another bit of its own flags: refused */
    CHECK(oracles_enhanced_reads_hold(0, addresses, seen, live, 0xc797, 0x10));    /* nothing read */
}

/* A ghost's capture of no terrain, never kept: Seasons' intro fades to white
 * as Din starts dancing, and a run settled in it captured every colour white
 * but for the outlines (71 % of its area one colour, under the nine tenths
 * that caught a blank screen).  A room the game darkens, or whose own
 * palettes are white, is its own look and kept. */
static void blank_captures(void)
{
    uint8_t base[64], live[64];
    for (unsigned i = 0; i < 32u; i++) {   /* a room's own palettes: every channel between 4 and 26 */
        const unsigned c = (4u + i % 20u) | (6u + i % 17u) << 5 | (8u + i % 13u) << 10;
        base[i * 2u] = (uint8_t)c; base[i * 2u + 1u] = (uint8_t)(c >> 8);
    }
    #define SET_OFFSET(o) for (unsigned i = 0; i < 32u; i++) { \
        const unsigned c = (unsigned)(base[i * 2u] | (base[i * 2u + 1u] << 8)); unsigned f = 0; \
        for (unsigned sh = 0; sh < 15u; sh += 5u) { const int ch = (int)((c >> sh) & 31u) + (o); f |= (unsigned)(ch < 0 ? 0 : ch > 31 ? 31 : ch) << sh; } \
        live[i * 2u] = (uint8_t)f; live[i * 2u + 1u] = (uint8_t)(f >> 8); }
    SET_OFFSET(0);
    CHECK(!oracles_enhanced_capture_blank(live, base, 0, NULL, 0));          /* the room's own */
    SET_OFFSET(2);
    CHECK(!oracles_enhanced_capture_blank(live, base, 0, NULL, 0));          /* a smooth palette transition's last mix, 2 apart */
    SET_OFFSET(28);
    CHECK(oracles_enhanced_capture_blank(live, base, 0, NULL, 0));           /* a fade to white nearly done */
    SET_OFFSET(-12);
    CHECK(oracles_enhanced_capture_blank(live, base, 0, NULL, 0));           /* a fade to black */
    SET_OFFSET(0);
    live[0] = 0xff; live[1] = 0x7f; live[2] = 0xff; live[3] = 0x7f;       /* a palette set white by other code (Seasons' outdoors): outvoted */
    CHECK(!oracles_enhanced_capture_blank(live, base, 0, NULL, 0));
    SET_OFFSET(-16);   /* a room the game darkens (checkDarkenRoom): -16 kept on it, its own look */
    CHECK(!oracles_enhanced_capture_blank(live, base, -16, NULL, 0));
    CHECK(oracles_enhanced_capture_blank(live, base, 0, NULL, 0));        /* the same palettes on a room kept at 0: a fade caught */
    SET_OFFSET(28);
    CHECK(oracles_enhanced_capture_blank(live, base, -16, NULL, 0));      /* a fade to white over a dark room */
    #undef SET_OFFSET
    uint8_t white[64];   /* a room whose own palettes are all white (3:f0 and 3:f2 of Ages): no fade */
    memset(white, 0xff, sizeof white);
    for (unsigned i = 1; i < 64u; i += 2u) white[i] = 0x7f;
    CHECK(!oracles_enhanced_capture_blank(white, white, 0, NULL, 0));
    static uint32_t area[160u * 128u];
    for (unsigned i = 0; i < 160u * 128u; i++) area[i] = i % 10u == 0u ? 0xff101010u : 0xffe0e0e0u;
    CHECK(!oracles_enhanced_capture_blank(base, base, 0, area, 160u * 128u));   /* nine tenths exactly: a terrain */
    area[0] = 0xffe0e0e0u; area[10] = 0xffe0e0e0u;
    CHECK(oracles_enhanced_capture_blank(base, base, 0, area, 160u * 128u));    /* more: the LCD off */
}

int main(void)
{
    world_math();
    compositor();
    ripple();
    view_and_savestate();
    isolated_room_edges();
    state_across_viewports();
    cutscenes();
    seasons_scenes();
    blank_captures();
    black_count();
    parent_keys();
    beside_reads();
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_zoom_out: ok\n");
    return 0;
}
