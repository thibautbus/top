/* The tile animation of a scroll, run by the view alone.
 *
 * The game freezes its animation for the whole of a scroll and resumes it
 * where it stopped (neighbours/animation.h).  The view runs the streams of
 * the room in play whole loops from the frozen step, one at least
 * (oracles_scroll_animation_step), from the freeze with the continuous
 * transitions (Link walks through the load), from the scroll's first step
 * without them (the screen stands still), into its own copy of the live tile
 * data, and draws with that copy everything the band shows of the live
 * room's tileset during the scroll: the neighbours in step with the live
 * room, the images of the room left, and the game's own window, whose pixels
 * are taken from a render with the view's tiles wherever a render with the
 * live tiles gives the core's pixel.  At the frame the scroll's counter runs
 * out, the copy stands on the step the game resumes from: nothing jumps when
 * the view goes back to the live tiles.  A scroll into a room of another
 * animation or tileset stays frozen, as the game has it (it starts the new
 * animation from its beginning).  The neighbours of another animation go on
 * at their own pace (ev_advance_neighbour_animations): the game starts theirs
 * again when Link enters them.  The view writes nothing into the game. */
#include "view_internal.h"

#define SCROLL_STEPS_HORIZONTAL 20u   /* wScreenScrollCounter at the start of a horizontal scroll ($14) */
#define SCROLL_STEPS_VERTICAL 16u     /* and of a vertical one ($10) */
#define SCROLL_LOAD_FRAMES 5u         /* the frames before the scroll moves (states 3 and 4), until a shorter one is measured */
#define TRANSITION_SCROLLING 5u       /* wScreenTransitionState: the scroll itself */

const uint8_t *ev_shown_vram(OraclesEnhancedView *v, unsigned bank)
{
    return v->shown_active ? v->shown_tiles + bank * TILE_DATA_BYTES : oracles_guest_vram(v->guest, bank);
}

const uint8_t *ev_shown_animated(const OraclesEnhancedView *v)
{
    return v->shown_active ? v->shown_animated : v->animated;
}

/* The scroll is over: its landing and its pace counted. */
static void finish(OraclesEnhancedView *v)
{
    v->scrolls_animated++;
    v->scroll_running = 0;
    if (v->scroll_gap_last) { v->scroll_gaps++; if (v->scroll_gap_last > v->scroll_gap_max) v->scroll_gap_max = v->scroll_gap_last; }
    v->scroll_tiles_off += v->scroll_tiles_off_last;
    if (!v->scroll_clock.landed_at) return;
    v->scroll_landed++;
    /* Landed on another frame than the plan's: the streams took the rest of
     * their loops in one frame (a shorter scroll), or stood waiting for it (a
     * longer one). */
    if (v->scroll_clock.landed_at != v->scroll_clock.expected) v->scroll_landed_off++;
    for (unsigned n = 0; n < 4u; n++) {
        if (!v->scroll.target[n]) continue;   /* outside its loop, or not in use */
        v->scroll_streams_run++;
        unsigned pace = v->scroll.ran[n] ? v->scroll.target[n] * 100u / v->scroll.ran[n] : SCROLL_PACE_MAX;
        if (pace > SCROLL_PACE_MAX) pace = SCROLL_PACE_MAX;
        v->scroll_pace[pace]++;
    }
}

/* The counter starts at $10 or $14 and counts a step a frame with the
 * continuous transitions, its first value held two frames, or a step every
 * two frames without them (docs/GAME_HOOKS.md, section 5). */
unsigned ev_scroll_expected_frames(unsigned load, unsigned steps, unsigned per_step)
{
    return load + (per_step == 1u ? steps + 2u : 2u * steps + 1u);
}

unsigned ev_scroll_clock_begin(EvScrollClock *c, int horizontal, int continuous)
{
    /* With the continuous transitions Link walks through the load and the
     * streams run from the freeze: over the shortest this scroll can be, the
     * shortest load seen and the doubled step, for a loop started is finished
     * whatever the scroll turns out to be; a longer scroll gives a stream more
     * loops (the plan made again at its first step).  Without them the screen
     * stands still through the load, and so do the streams, as in the game:
     * they run from the scroll's first step, over its frames. */
    if (!c->started) { c->load_frames = SCROLL_LOAD_FRAMES; c->started = 1; }
    c->elapsed = 0;
    c->landed_at = 0;
    c->seen_counter = 0;
    c->continuous = continuous;
    c->expected = ev_scroll_expected_frames(c->load_frames, horizontal ? SCROLL_STEPS_HORIZONTAL : SCROLL_STEPS_VERTICAL, 1u);
    return continuous ? c->expected : 0u;
}

unsigned ev_scroll_clock_frame(EvScrollClock *c, int scrolling, unsigned counter, uint8_t delta, int *replan, unsigned *plan)
{
    *replan = 0;
    const unsigned size = delta & 0x80u ? 0x100u - delta : delta, per_step = size >= 8u ? 1u : 2u;
    /* The scroll's first step: its length is known from here (the continuous
     * transitions double the step, wcd14, at its first frame or, the screen
     * shaken, the next one). */
    if (scrolling && !c->seen_counter) {
        c->seen_counter = 1;
        c->first_step = c->elapsed;
        c->first_counter = counter;
        c->per_step = per_step;
        if (c->elapsed < c->load_frames) c->load_frames = c->elapsed;
        c->expected = ev_scroll_expected_frames(c->elapsed, counter, per_step);
        *replan = 1;
        *plan = c->continuous ? c->expected : c->expected - c->elapsed;
    } else if (scrolling && !c->landed_at && counter == c->first_counter && per_step != c->per_step) {
        /* The step changed before the counter moved: the continuous
         * transitions double it only on aligned hardware registers, which
         * show the game's a frame late, so a screen shaken at the freeze has
         * it doubled a frame after the first step; the scroll is the doubled
         * one from that first step all the same, the screen not moving yet. */
        c->per_step = per_step;
        c->expected = ev_scroll_expected_frames(c->first_step, counter, per_step);
        *replan = 1;
        *plan = c->continuous ? c->expected : c->expected - c->first_step;
    }
    const int lands = c->landed_at || (scrolling && counter == 0);
    const unsigned remaining = lands ? 0u : c->expected > c->elapsed + 2u ? c->expected - c->elapsed - 1u : 1u;
    c->elapsed++;
    if (lands && !c->landed_at) c->landed_at = c->elapsed;
    return remaining;
}

static void begin(OraclesEnhancedView *v, const OraclesAnimationState *live)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const unsigned frames = ev_scroll_clock_begin(&v->scroll_clock, (int)(v->observation.scroll_direction & 1u), oracles_guest_has_transition_policy(v->guest));
    oracles_scroll_animation_begin(&v->scroll, live, frames, t, v->rom, v->rom_size);
    memset(v->scroll_touched, 0, sizeof v->scroll_touched);
    memset(v->scroll_game_wrote, 0, sizeof v->scroll_game_wrote);
    memcpy(v->scroll_begin_tiles, oracles_guest_vram(v->guest, 0), TILE_DATA_BYTES);
    memcpy(v->scroll_begin_tiles + TILE_DATA_BYTES, oracles_guest_vram(v->guest, 1), TILE_DATA_BYTES);
    v->scroll_running = 1;
    v->scroll_gap_last = 0;
    v->scroll_tiles_off_last = 0;
    v->scroll_version++;
    v->scroll_wrote = 0;
}

#define TILE_BIT(bit) ((uint8_t)(1u << ((bit) & 7u)))

unsigned ev_scroll_note_game_writes(const uint8_t *live0, const uint8_t *live1, const uint8_t *begin, const uint8_t *touched, uint8_t *game_wrote)
{
    unsigned given_back = 0;
    for (unsigned bit = 0; bit < 2u * 384u; bit++) {
        const unsigned bank = bit / 384u, tile = bit % 384u;
        if (!(game_wrote[bit >> 3] & TILE_BIT(bit)) && memcmp((bank ? live1 : live0) + tile * 16u, begin + bank * TILE_DATA_BYTES + tile * 16u, 16u) != 0) {
            game_wrote[bit >> 3] |= TILE_BIT(bit);
            if (touched[bit >> 3] & TILE_BIT(bit)) given_back++;
        }
    }
    return given_back;
}

void ev_scroll_overlay(uint8_t *tiles, size_t stride, const uint8_t *images, const uint8_t *touched, const uint8_t *game_wrote)
{
    for (unsigned bit = 0; bit < 2u * 384u; bit++)
        if (touched[bit >> 3] & ~game_wrote[bit >> 3] & TILE_BIT(bit))
            memcpy(tiles + (bit / 384u) * stride + (bit % 384u) * 16u, images + (bit / 384u) * TILE_DATA_BYTES + (bit % 384u) * 16u, 16u);
}

unsigned ev_scroll_tiles_off(const uint8_t *live0, const uint8_t *live1, const uint8_t *images, const uint8_t *touched, const uint8_t *held, const uint8_t *game_wrote)
{
    unsigned off = 0;
    for (unsigned bit = 0; bit < 2u * 384u; bit++) {
        const unsigned bank = bit / 384u, tile = bit % 384u;
        if ((touched[bit >> 3] & ~held[bit >> 3] & ~game_wrote[bit >> 3] & TILE_BIT(bit))
            && memcmp((bank ? live1 : live0) + tile * 16u, images + bank * TILE_DATA_BYTES + tile * 16u, 16u) != 0) off++;
    }
    return off;
}

/* The landing's count of tiles off, the copies the game's queue holds back
 * set aside (the stream has loaded them in the view as in the game, which
 * shows them once it animates). */
static unsigned tiles_off(OraclesEnhancedView *v)
{
    uint8_t held[2u * 384u / 8u];
    memset(held, 0, sizeof held);
    oracles_animation_queued_tiles(&v->scroll.frozen, oracles_guest_tables(v->guest), v->rom, v->rom_size, held);
    return ev_scroll_tiles_off(oracles_guest_vram(v->guest, 0), oracles_guest_vram(v->guest, 1), v->scroll_images, v->scroll_touched, held, v->scroll_game_wrote);
}

void ev_scroll_reset(OraclesEnhancedView *v)
{
    v->scroll_running = 0;
    v->scroll_decided = 0;
    v->have_play_tileset = 0;
    memset(&v->scroll_clock, 0, sizeof v->scroll_clock);
}

void ev_scroll_animation(OraclesEnhancedView *v)
{
    v->shown_active = 0;
    if (!v->rom) return;
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const uint8_t gfx = oracles_guest_read8(v->guest, t->tileset_gfx), animation = oracles_guest_read8(v->guest, t->tileset_animation);
    if (!oracles_guest_vram(v->guest, 0) || !oracles_guest_vram(v->guest, 1)) return;
    OraclesAnimationState live;
    oracles_animation_read(v->guest, &live);
    const uint8_t scroll_mode = oracles_guest_read8(v->guest, t->scroll_mode);
    if (scroll_mode & 0x01u) {
        /* The game animates again: the scroll is over. */
        if (v->scroll_running) finish(v);
        v->scroll_decided = 0;
        if (v->observation.playing && !v->observation.in_transition) { v->play_tileset_gfx = gfx; v->play_tileset_animation = animation; v->have_play_tileset = 1; }
        return;
    }
    /* $08: a scroll between two rooms under way (screenTransitionState3 waits
     * for it); $04 while the edge is touched and the room not loaded yet, which
     * may still turn into a warp ($00). */
    const uint8_t phase = scroll_mode & 0x7fu;
    if (!v->scroll_decided) {
        if (phase == ORACLES_SCROLL_MODE_DECIDED && v->observation.in_scroll) return;
        v->scroll_decided = 1;
        if (phase != ORACLES_SCROLL_MODE_TRANSITION || !v->observation.in_scroll) return;
        /* The room entered has loaded its tileset: another animation or
         * tileset is started again by the game, and stays frozen. */
        const int same = v->have_play_tileset && gfx == v->play_tileset_gfx && animation == v->play_tileset_animation && animation != 0xffu;
        if (!same || !v->observation.playing || !ev_on_map(v) || v->observation.large || v->observation.large_grid) { v->scrolls_still++; return; }
        begin(v, &live);
    }
    if (!v->scroll_running) return;
    if (phase != ORACLES_SCROLL_MODE_TRANSITION) { finish(v); return; }   /* no longer a scroll: counted where it stood */
    const int scrolling = oracles_guest_read8(v->guest, t->screen_transition_state) == TRANSITION_SCROLLING
        && oracles_guest_read8(v->guest, t->screen_transition_substate) != 0;
    const unsigned counter = oracles_guest_read8(v->guest, t->screen_scroll_counter);
    int replan;
    unsigned plan;
    const unsigned remaining = ev_scroll_clock_frame(&v->scroll_clock, scrolling, counter, oracles_guest_read8(v->guest, t->screen_scroll_delta), &replan, &plan);
    if (replan) oracles_scroll_animation_plan(&v->scroll, plan);   /* the plan made on the shortest scroll made again */
    const int copies = oracles_scroll_animation_step(&v->scroll, remaining, t, v->rom, v->rom_size, v->scroll_images, v->scroll_touched);
    if (copies < 0) {   /* data it cannot read: the game's frozen tiles, the scroll counted as not landed */
        v->scroll_clock.landed_at = 0;
        finish(v);
        return;
    }
    if (copies) { v->scroll_version++; v->scroll_wrote = 1; }
    if (!remaining && v->scroll_clock.landed_at == v->scroll_clock.elapsed) {
        /* Landed on the game's step: what the view's streams wrote, the held
         * copies aside, is what the live VRAM shows (counted), and from here
         * on the band shows the live tiles themselves. */
        v->scroll_tiles_off_last = tiles_off(v);
        memset(v->scroll_touched, 0, sizeof v->scroll_touched);
        v->scroll_version++;
        v->scroll_wrote = 0;
    }
    /* The live tiles, those the view's streams wrote taken from its images,
     * but those the game has written since the freeze (the graphics of the
     * room entered, loaded over an animated tile): the game's own. */
    const uint8_t *live0 = oracles_guest_vram(v->guest, 0), *live1 = oracles_guest_vram(v->guest, 1);
    v->scroll_tiles_given_back += ev_scroll_note_game_writes(live0, live1, v->scroll_begin_tiles, v->scroll_touched, v->scroll_game_wrote);
    memcpy(v->shown_tiles, live0, TILE_DATA_BYTES);
    memcpy(v->shown_tiles + TILE_DATA_BYTES, live1, TILE_DATA_BYTES);
    ev_scroll_overlay(v->shown_tiles, TILE_DATA_BYTES, v->scroll_images, v->scroll_touched, v->scroll_game_wrote);
    /* Where the view stands against the game, at each frozen frame: the last
     * one is what the game resumes from. */
    v->scroll_gap_last = oracles_animation_gap(&v->scroll.now, &live, t, v->rom, v->rom_size);
    for (unsigned i = 0; i < sizeof v->shown_animated; i++) v->shown_animated[i] = (uint8_t)(v->animated[i] | (v->scroll_touched[i] & ~v->scroll_game_wrote[i]));
    v->shown_active = 1;
    v->live_tiles_hash = oracles_guest_hash(v->shown_tiles + TILE_DATA_BYTES, TILE_DATA_BYTES, oracles_guest_hash(v->shown_tiles, TILE_DATA_BYTES, ORACLES_HASH_SEED));
    v->have_live_tiles_hash = 1;
}

/* Pixels of `image` (width x rows from line `first` of a 160x144 render)
 * drawn again with the view's tiles: a render with the VRAM the image was
 * drawn from (`vram0`, `vram1`, tiles and maps), one with the same VRAM, the
 * view's images laid over it, from the same registers and palettes, and the
 * view's pixel wherever the first render gives the image's own (a sprite over
 * it does not).  Returns the pixels changed. */
static unsigned redraw(OraclesEnhancedView *v, const uint32_t *image, uint32_t *out, unsigned first, unsigned rows,
                       const uint8_t *vram0, const uint8_t *vram1, const OraclesPpuRegs *regs, const uint8_t *palettes, const uint32_t *colours)
{
    static const uint8_t no_sprites[160];
    OraclesPpuInput in;
    in.oam = no_sprites;
    in.bg_palettes = palettes;
    in.obj_palettes = palettes;
    in.colours = colours;
    for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) in.lines[ly] = *regs;
    in.vram = v->hybrid_vram;
    memcpy(v->hybrid_vram, vram0, 0x2000u);
    memcpy(v->hybrid_vram + 0x2000u, vram1, 0x2000u);
    oracles_ppu_render(&in, v->hybrid_frame);
    ev_scroll_overlay(v->hybrid_vram, 0x2000u, v->scroll_images, v->scroll_touched, v->scroll_game_wrote);
    oracles_ppu_render(&in, v->scroll_frame);
    unsigned changed = 0;
    for (unsigned y = 0; y < rows; y++)
        for (unsigned x = 0; x < ORACLES_PPU_WIDTH; x++) {
            const unsigned at = (first + y) * ORACLES_PPU_WIDTH + x, own = y * ORACLES_PPU_WIDTH + x;
            const uint32_t now = v->hybrid_frame[at], shown = v->scroll_frame[at];
            out[own] = image[own];
            if (now != shown && image[own] == now) { out[own] = shown; changed++; }
        }
    return changed;
}

const uint32_t *ev_scroll_window(OraclesEnhancedView *v)
{
    const uint32_t *core = oracles_core_pixels(v->core);
    if (!v->shown_active || !v->scroll_wrote || v->have_wave || !core) return core;
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const uint8_t *regs = oracles_guest_ptr(v->guest, t->gfx_regs3, 6), *palettes = oracles_guest_bg_palettes(v->guest);
    if (!regs || !palettes) return core;
    const OraclesEnhancedObservation *ob = &v->observation;
    /* The registers the frame on screen was drawn with, as for the text box (view_textbox.c). */
    const OraclesPpuRegs r = { (uint8_t)((regs[0] | 0x80u) & ~0x02u), (uint8_t)(ob->drawn_camera_y + ob->drawn_offset_y - (int)ORACLES_ENHANCED_HUD_HEIGHT),
                               (uint8_t)(ob->drawn_camera_x + ob->drawn_offset_x), regs[3], regs[4] };
    memcpy(v->scroll_window, core, ORACLES_ENHANCED_HUD_HEIGHT * ORACLES_PPU_WIDTH * sizeof *core);
    v->scroll_window_pixels += redraw(v, core + ORACLES_ENHANCED_HUD_HEIGHT * ORACLES_PPU_WIDTH, v->scroll_window + ORACLES_ENHANCED_HUD_HEIGHT * ORACLES_PPU_WIDTH,
                                      ORACLES_ENHANCED_HUD_HEIGHT, ORACLES_ENHANCED_AREA_HEIGHT, oracles_guest_vram(v->guest, 0), oracles_guest_vram(v->guest, 1), &r, palettes, v->colours);
    return v->scroll_window;
}

/* The capture of the room in play (ev_capture_source_terrain) keeps what it
 * was drawn from, its tiles with its maps: the copy the game's queue loads
 * after the capture reaches the VRAM during the freeze, and a render with the
 * frozen tiles would leave the capture's pixels of the tiles it changed. */
void ev_scroll_keep_capture(OraclesEnhancedView *v, const OraclesPpuRegs *regs, const uint8_t *palettes)
{
    memcpy(v->source_vram, oracles_guest_vram(v->guest, 0), 0x2000u);
    memcpy(v->source_vram + 0x2000u, oracles_guest_vram(v->guest, 1), 0x2000u);
    v->source_regs = *regs;
    v->source_regs.lcdc = (uint8_t)((regs->lcdc | 0x80u) & ~0x02u);
    memcpy(v->source_palettes, palettes, sizeof v->source_palettes);
}

/* The image of the room left (`left`) or the capture of the room in play,
 * drawn with the view's tiles during the scroll; itself otherwise. */
const uint32_t *ev_scroll_capture(OraclesEnhancedView *v, int left)
{
    const uint32_t *area = left ? v->left_area : ev_source_area(v);
    if (!v->shown_active || !v->scroll_wrote) return area;
    uint32_t *out = left ? v->left_shown : v->source_shown;
    unsigned *version = left ? &v->left_shown_version : &v->source_shown_version;
    if (*version == v->scroll_version) return out;
    *version = v->scroll_version;
    const uint8_t *vram = left ? v->left_vram : v->source_vram;
    redraw(v, area, out, ORACLES_PPU_HEIGHT - ORACLES_GHOST_AREA_HEIGHT, ORACLES_GHOST_AREA_HEIGHT, vram, vram + 0x2000u,
           left ? &v->left_regs : &v->source_regs, left ? v->left_palettes : v->source_palettes, v->raw_colours);
    return out;
}

void oracles_enhanced_view_scroll_animation_counts(const OraclesEnhancedView *v, OraclesEnhancedScrollCounts *out)
{
    memset(out, 0, sizeof *out);
    out->scrolls = v->scrolls_animated; out->scrolls_still = v->scrolls_still; out->landed = v->scroll_landed; out->landed_off = v->scroll_landed_off;
    out->gaps = v->scroll_gaps; out->gap_max = v->scroll_gap_max; out->tiles_off = v->scroll_tiles_off;
    out->tiles_given_back = v->scroll_tiles_given_back;
    out->streams_run = v->scroll_streams_run; out->window_pixels = v->scroll_window_pixels;
    unsigned seen = 0, half = (v->scroll_streams_run + 1u) / 2u;
    for (unsigned p = 0; p <= SCROLL_PACE_MAX; p++) {
        if (!v->scroll_pace[p]) continue;
        if (!seen) out->pace_min = p;
        if (seen < half && seen + v->scroll_pace[p] >= half) out->pace_median = p;
        seen += v->scroll_pace[p];
        out->pace_max = p;
    }
}
