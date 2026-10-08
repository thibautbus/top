#include "view_internal.h"

#define ANIMATION_RENDERS_PER_FRAME 3u   /* neighbours rendered again for their own tiles alone, per frame: the others wait a frame */

#define TILES_SETTLING_FRAMES 8u   /* frames of play after a load where the vblank queue still writes the room's own graphics */

/* ---- the room Link is in ------------------------------------------------------------------- */

/* Whether an entry's terrain is shown: a valid one, one being run again
 * after a season change included.  Past the game's fade its season is the old
 * one until the ghost delivers the new: deliberately, the old
 * season stands for that moment rather than a black
 * room, the less abrupt of the two. */
/* A room run again stays drawn through one failed run; the second drops it (record_failure). */
int ev_entry_drawable(const OraclesEnhancedView *v, const entry *e)
{
    (void)v;
    return e && e->valid;
}

/* In normal play on the map, the room's terrain from the live VRAM (objects
 * left out).  During the scroll that follows, the game rewrites its map with
 * the destination row by row, so only this render can fill the part of the
 * room the game's window leaves behind, when the cache has no entry for the
 * room (the entry, computed back from a neighbour, is preferred). */
/* The object of a tag, in the live instance: NULL for Link, the companion and
 * the items, and for an object that persists across rooms (mode 3), which the
 * live instance draws wherever Link goes. */
static const uint8_t *live_room_object(OraclesEnhancedView *v, const OraclesSpriteTag *tag)
{
    if (tag->kind < 1u || tag->kind > 3u) return NULL;
    const uint8_t *object = oracles_guest_object(v->guest, tag->slot, tag->kind);
    if (!object || (object[ORACLES_OBJ_ENABLED] & 3u) == 3u) return NULL;
    return object;
}

int ev_live_room_objects_oam(OraclesEnhancedView *v, uint8_t out[160])
{
    memset(out, 0, 160);
    const uint8_t *oam = oracles_guest_oam(v->guest);
    if (!v->live_sprites || !oam) return 0;
    OraclesSpriteTag tags[ORACLES_SPRITE_TAGS];
    const unsigned count = oracles_sprites_tags_for_oam(v->live_sprites, oam, tags, ORACLES_SPRITE_TAGS);
    int any = 0;
    for (unsigned i = 0; i < count; i++) {
        if (!live_room_object(v, &tags[i])) continue;
        for (unsigned n = 0; n < tags[i].count && tags[i].first + n < 40u; n++) {
            memcpy(out + (tags[i].first + n) * 4u, oam + (tags[i].first + n) * 4u, 4u);
            any = 1;
        }
    }
    return any;
}

int ev_live_entry_is_room_object(OraclesEnhancedView *v, unsigned index)
{
    const uint8_t *oam = oracles_guest_oam(v->guest);
    if (!v->live_sprites || !oam) return 0;
    OraclesSpriteTag tags[ORACLES_SPRITE_TAGS];
    const unsigned count = oracles_sprites_tags_for_oam(v->live_sprites, oam, tags, ORACLES_SPRITE_TAGS);
    for (unsigned i = 0; i < count; i++)
        if (index >= tags[i].first && index < (unsigned)tags[i].first + tags[i].count) return live_room_object(v, &tags[i]) != NULL;
    return 0;
}

void ev_capture_source_terrain(OraclesEnhancedView *v)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    if (ob->in_transition || ob->large_grid) return;   /* a large room keeps its own whole capture */
    /* The capture of the room a scroll came from is kept until the ghost has
     * delivered that room: it is what stands beside the room entered meanwhile. */
    if (v->source_valid && v->have_came_from && v->source_group == v->observer.ref_group && v->source_room == v->came_from_room) {
        const entry *e = ev_find_entry(v, v->source_group, v->source_room);
        if (!ev_entry_drawable(v, e)) return;
    }
    v->source_valid = 0;
    if (!ob->playing || ob->cutscene || !ev_on_map(v) || !oracles_ghost_primeable(v->guest, NULL)) return;
    ev_refresh_colours(v);
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const uint8_t *regs = oracles_guest_ptr(v->guest, t->gfx_regs3, 6);
    OraclesPpuInput in;
    /* With the neighbours' objects drawn, the room's own objects are in the
     * capture as the game draws them: it becomes the image of the room when a
     * scroll leaves it.  Link, his companion and his items are left out. */
    uint8_t objects_oam[160];
    const int with_objects = v->neighbour_objects && ev_live_room_objects_oam(v, objects_oam);
    in.vram = oracles_guest_vram(v->guest, 0);
    in.oam = with_objects ? objects_oam : oracles_guest_oam(v->guest);
    in.bg_palettes = oracles_guest_bg_palettes(v->guest);
    in.obj_palettes = oracles_guest_obj_palettes(v->guest);
    in.colours = v->raw_colours;   /* RGB555: the compose fades it with the game and converts it */
    if (!regs || !in.vram || !in.oam || !in.bg_palettes || !in.obj_palettes) return;
    const OraclesPpuRegs r = { (uint8_t)(with_objects ? (regs[0] | 0x82u) : ((regs[0] | 0x80u) & ~0x02u)), regs[1], regs[2], regs[3], regs[4] };
    /* Its render waits until it is read (ev_source_area), from these inputs: most captures are never read. */
    v->source_render_regs = r;
    memcpy(v->source_oam, in.oam, sizeof v->source_oam);
    memcpy(v->source_obj_palettes, in.obj_palettes, sizeof v->source_obj_palettes);
    v->source_area_pending = 1;
    ev_scroll_keep_capture(v, &r, in.bg_palettes);
    v->source_shown_version = 0;
    v->source_group = v->observer.ref_group;
    v->source_room = v->observer.ref_room;
    v->source_left = ob->window_left;
    v->source_top = ob->window_top;
    v->source_epoch = ob->epoch;
    v->source_pipeline = v->colours_pipeline;
    v->source_valid = 1;
}

const uint32_t *ev_source_area(OraclesEnhancedView *v)
{
    if (!v->source_area_pending) return v->source_area;
    v->source_area_pending = 0;
    OraclesPpuInput in;
    in.vram = v->source_vram;   /* bank 0 then bank 1, as the capture copied them */
    in.oam = v->source_oam;
    in.bg_palettes = v->source_palettes;
    in.obj_palettes = v->source_obj_palettes;
    in.colours = v->raw_colours;
    for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) in.lines[ly] = v->source_render_regs;
    const unsigned first = ORACLES_PPU_HEIGHT - ORACLES_GHOST_AREA_HEIGHT;
    oracles_ppu_render_wide_lines(&in, v->hybrid_frame, ORACLES_PPU_WIDTH, first, ORACLES_PPU_HEIGHT);   /* the game area only */
    memcpy(v->source_area, v->hybrid_frame + first * ORACLES_PPU_WIDTH, sizeof v->source_area);
    return v->source_area;
}

/* The capture stands for the room being left during its scrolling
 * transition, and, once the scroll is over, for that room beside the one
 * entered until the ghost has delivered it. */
int ev_source_is_scroll_source(const OraclesEnhancedView *v)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    if (!v->source_valid || v->source_pipeline != v->colours_pipeline || v->source_epoch != ob->epoch || v->source_group != v->observer.ref_group) return 0;
    if (ob->in_scroll) return v->source_room == v->observer.ref_room;
    return v->have_came_from && v->source_room == v->came_from_room;
}

/* ---- rendering the neighbours --------------------------------------------------------------- */

/* A neighbour whose tileset is the one the live room has loaded is rendered
 * from its own map with the live tiles and palettes, so its animated tiles
 * (water, flowers, torches) move in step with the room's; rendered again
 * whenever the live tiles or palettes change.  Otherwise the ghost's render. */
/* Which tiles the live VRAM animates: compared with the previous frame of
 * normal play under the same tileset. */
void ev_track_animated_tiles(OraclesEnhancedView *v)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const uint8_t gfx = oracles_guest_read8(v->guest, t->tileset_gfx);
    const uint8_t *vram0 = oracles_guest_vram(v->guest, 0), *vram1 = oracles_guest_vram(v->guest, 1);
    v->have_live_tiles_hash = 0;
    if (!vram0 || !vram1) return;
    /* The live tiles change once a frame at most: hashed here once, for the
     * renders of every neighbour of the same tileset this frame. */
    v->live_tiles_hash = oracles_guest_hash(vram1, TILE_DATA_BYTES, oracles_guest_hash(vram0, TILE_DATA_BYTES, ORACLES_HASH_SEED));
    v->have_live_tiles_hash = 1;
    if (gfx != v->animated_tileset_gfx) {
        memset(v->animated, 0, sizeof v->animated); memset(v->tile_changes, 0, sizeof v->tile_changes);
        v->animated_tileset_gfx = gfx; v->have_live_tiles_prev = 0;
    }
    /* A text box loads its font, a menu its tiles: not frames of play. */
    const int text = oracles_guest_read8(v->guest, t->text_is_active) != 0, menu = oracles_guest_read8(v->guest, t->opened_menu_type) != 0;
    if (v->observation.in_transition || !v->observation.playing || text || menu) {
        v->have_live_tiles_prev = 0;
        v->tiles_settling = TILES_SETTLING_FRAMES;
        return;
    }
    /* The graphics a room owns (a shop's sign, a house's front) reach the
     * VRAM through the vblank queue, a few frames after the scroll has
     * ended, on frames of play: counted as changes they would look animated
     * after two entries, and a neighbour of the same tileset would take the
     * live room's sign for its own.  The frames that follow a load are not
     * counted; a tile that truly animates is counted on the frames after. */
    if (v->tiles_settling) v->tiles_settling--;
    if (v->have_live_tiles_prev && !v->tiles_settling) {
        for (unsigned bank = 0; bank < 2; bank++) {
            const uint8_t *now = bank ? vram1 : vram0, *prev = v->live_tiles_prev + bank * ORACLES_GHOST_TILE_BYTES;
            for (unsigned tile = 0; tile < 384u; tile++)
                if (memcmp(now + tile * 16u, prev + tile * 16u, 16u) != 0) {
                    uint8_t *changes = &v->tile_changes[bank * 384u + tile];
                    if (*changes < 255u) (*changes)++;
                    if (*changes >= 2u) v->animated[(bank * 384u + tile) >> 3] |= (uint8_t)(1u << ((bank * 384u + tile) & 7u));
                }
        }
    }
    memcpy(v->live_tiles_prev, vram0, ORACLES_GHOST_TILE_BYTES);
    memcpy(v->live_tiles_prev + ORACLES_GHOST_TILE_BYTES, vram1, ORACLES_GHOST_TILE_BYTES);
    v->have_live_tiles_prev = 1;
}

/* Each cached neighbour's own tile animation, as many steps as the game took
 * for its own room (normal play: bit 0 of wScrollMode), written into the
 * neighbour's tiles.  A neighbour in step with the live room is
 * advanced too, its render taking the live tiles meanwhile. */
void ev_advance_neighbour_animations(OraclesEnhancedView *v)
{
    /* As many steps as the game took since the last composition: none on a
     * frame whose logic ran long past the vblank, two on the next. */
    unsigned steps = v->animation_steps_counted ? v->animation_steps : 1u;
    v->animation_steps = 0;
    if (steps > ANIMATION_STEPS_MAX) steps = ANIMATION_STEPS_MAX;
    if (!v->rom || !v->observation.playing) return;
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    /* During a scroll the game's animation stands still, and so does this
     * one, unless the view runs the scroll's (view_scroll.c): the neighbours
     * of another animation then go on a step a frame, their streams that
     * follow the live room's following the view's. */
    const int scroll = v->shown_active;
    if (!scroll && !(oracles_guest_read8(v->guest, t->scroll_mode) & 0x01u)) return;
    if (scroll) steps = 1u;
    const uint8_t gfx = oracles_guest_read8(v->guest, t->tileset_gfx), animation = oracles_guest_read8(v->guest, t->tileset_animation);
    OraclesAnimationState live;
    if (scroll) live = v->scroll.now;
    else oracles_animation_read(v->guest, &live);
    const uint8_t *live_vram[2] = { ev_shown_vram(v, 0), ev_shown_vram(v, 1) };
    const uint32_t key = 0x80000000u | (uint32_t)v->observation.group << 16 | (uint32_t)v->observation.room << 8 | animation;
    for (unsigned i = 0; i < v->slot_count; i++) {
        entry *e = &v->slots[i];
        if (!e->used || !e->valid) continue;
        const int in_step = e->tileset_gfx == gfx && e->animation.tileset_animation == animation;
        if (scroll && in_step) continue;   /* drawn with the view's tiles; its own count stays the game's */
        /* Its streams that follow the live room's are not stepped by their own
         * count: their steps and their images come from the following below,
         * on the frame the live VRAM takes the twin's copy. */
        const unsigned followed = e->align_key == key ? oracles_animation_followed_streams(&e->follow) : 0u;
        int copied = 0;
        for (unsigned step = 0; step < steps && copied >= 0; step++) {
            const int c = oracles_animation_step_except(&e->animation, followed, t, v->rom, v->rom_size, e->tiles, e->own_animated);
            copied = c < 0 ? -1 : copied + c;
        }
        if (copied < 0) { e->animation.tileset_animation = 0xffu; continue; }   /* data it cannot read: left still */
        if (copied) {
            e->animation_version++;
            v->own_animation_copies++;
            if (e->tileset_gfx != gfx || e->animation.tileset_animation != animation) v->other_animation_copies++;
        }
        /* Its streams that run the same loop as the live room's follow them,
         * step for step, after this frame's own step: matched when it
         * arrives and when the room in play changes. */
        if (in_step) continue;
        if (e->align_key != key) {
            v->streams_matched += oracles_animation_match(&e->animation, &live, t, v->rom, v->rom_size, &e->follow);
            e->align_key = key;
        }
        if (!e->follow.count) continue;
        if (oracles_animation_follow(&e->animation, &live, &e->follow, t, v->rom, v->rom_size, e->tiles, e->own_animated, live_vram)) {
            e->animation_version++;
            v->streams_followed++;
        }
    }
}

/* The live room's animation handed to its own entry as it becomes a
 * neighbour: the game's state, and the live tiles its animation writes. */
void ev_hand_live_animation(OraclesEnhancedView *v, entry *e)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const uint8_t *vram0 = oracles_guest_vram(v->guest, 0), *vram1 = oracles_guest_vram(v->guest, 1);
    if (!vram0 || !vram1 || e->tileset_gfx != oracles_guest_read8(v->guest, t->tileset_gfx)) return;
    oracles_animation_read(v->guest, &e->animation);
    e->image_count = oracles_animation_images(&e->animation, t, v->rom, v->rom_size, e->image_tiles, e->image_sources, ENTRY_ANIMATION_IMAGES);
    for (unsigned k = 0; k < e->image_count; k++) {
        const unsigned bank = e->image_tiles[k] / 384u, tile = e->image_tiles[k] % 384u;
        memcpy(e->tiles + bank * ORACLES_GHOST_TILE_BYTES + tile * 16u, (bank ? vram1 : vram0) + tile * 16u, 16u);
    }
    e->animation_version++;
}

/* The game's palette fade as displayed: the offset the palette thread
 * added to every channel of the tileset's base palettes
 * (w2TilesetBgPalettes) to make the live ones (updateFadingPalettes:
 * saturated to white or black past the range).  An unsaturated channel
 * shows the offset exactly: the offset most channels agree on (a palette
 * set apart from the base by other code, white in Seasons' outdoors, is
 * outvoted); channels all saturated are a full fade. */
int ev_displayed_fade(const uint8_t live[64], const uint8_t base[64])
{
    unsigned votes[63] = { 0 }, white = 0, black = 0;
    for (unsigned i = 0; i < 32u; i++) {
        const unsigned a = (unsigned)(live[i * 2u] | (live[i * 2u + 1u] << 8)), b = (unsigned)(base[i * 2u] | (base[i * 2u + 1u] << 8));
        for (unsigned shift = 0; shift < 15u; shift += 5u) {
            const int l = (int)((a >> shift) & 31u), d = l - (int)((b >> shift) & 31u);
            if (l == 31) white++;
            else if (l == 0) black++;
            else votes[d + 31]++;
        }
    }
    int best = 0;
    unsigned best_votes = 0;
    for (int d = 0; d <= 31; d++)
        for (int sign = 1; sign >= -1; sign -= 2) {
            const int o = d * sign;
            if (votes[o + 31] > best_votes) { best_votes = votes[o + 31]; best = o; }
        }
    if (best_votes) return best;
    if (white && !black) return 31;
    if (black && !white) return -32;
    return 0;
}

int oracles_enhanced_capture_blank(const uint8_t bg_palettes[64], const uint8_t base_palettes[64], int kept_offset, const uint32_t *area, unsigned pixels)
{
    /* Palettes equal to the base are no fade, even all white or all black
     * (3:f0 and 3:f2 of Ages, whose own palettes are white): every channel
     * saturated would read as a full fade. */
    const int fade = memcmp(bg_palettes, base_palettes, 64u) == 0 ? 0 : ev_displayed_fade(bg_palettes, base_palettes);
    if (fade - kept_offset > 2 || fade - kept_offset < -2) return 1;
    unsigned same = 0;
    for (unsigned i = 0; area && i < pixels; i++) if (area[i] == area[0]) same++;
    return area && pixels && same > pixels * 9u / 10u;
}

/* Read off the palettes on screen rather than from the thread, which is a
 * frame ahead of the hardware and forgets its offset once a fade ends,
 * while the palettes stay white until the next room's are loaded. */
void ev_track_fade(OraclesEnhancedView *v)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const uint8_t *live = oracles_guest_bg_palettes(v->guest), *base = oracles_guest_ptr(v->guest, t->tileset_bg_palettes, 64u);
    v->fade_effective = live && base ? ev_displayed_fade(live, base) : 0;
}

/* A palette set with the displayed fade applied, as the palette thread does it. */
static void faded_palettes(const uint8_t *own, int fade, uint8_t *out)
{
    for (unsigned i = 0; i < 32u; i++) {
        const unsigned c = (unsigned)(own[i * 2u] | (own[i * 2u + 1u] << 8));
        unsigned f = 0;
        for (unsigned shift = 0; shift < 15u; shift += 5u) {
            const int ch = (int)((c >> shift) & 31u) + fade;
            f |= (unsigned)(ch < 0 ? 0 : ch > 31 ? 31 : ch) << shift;
        }
        out[i * 2u] = (uint8_t)f; out[i * 2u + 1u] = (uint8_t)(f >> 8);
    }
}

/* Whether a neighbour of the live room's palette header is drawn with the
 * palettes on screen rather than its own: a smooth palette transition
 * (paletteTransitions.s, paletteFadeHandler08) leaves the room it enters at
 * its last displayed mix, 1/16 of the palette left and 15/16 of the one
 * entered, truncated (a channel up to 2 apart), until the palettes are
 * refreshed at the end of the scroll (Yoll Graveyard's edge: the last
 * dozen frames of it).  The same header, no palette thread running and every channel
 * within 2 of the neighbour's own: that tint, not another palette (the load
 * sets wTilesetPalette before it loads the palettes).  Palettes equal to its
 * own are no tint: the own path draws them and checks the render. */
static int neighbour_takes_live_palettes(OraclesEnhancedView *v, const entry *e, const uint8_t *live)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    if (!live || e->tileset_palette != oracles_guest_read8(v->guest, t->tileset_palette)
        || oracles_guest_read8(v->guest, t->palette_thread_mode) != 0) return 0;
    int differs = 0;
    for (unsigned i = 0; i < 32u; i++) {
        const unsigned a = (unsigned)(live[i * 2u] | (live[i * 2u + 1u] << 8)), b = (unsigned)(e->bg_palettes[i * 2u] | (e->bg_palettes[i * 2u + 1u] << 8));
        for (unsigned shift = 0; shift < 15u; shift += 5u) {
            const int d = (int)((a >> shift) & 31u) - (int)((b >> shift) & 31u);
            if (d < -2 || d > 2) return 0;
            differs |= d != 0;
        }
    }
    return differs;
}

/* The room's terrain rendered by the view from what the ghost delivered:
 * its own map, tiles and palettes (the whole room, 240x176 for a large
 * one), the palettes carrying the fade the game displays.  Under the same
 * tileset as the live room, the tiles the live VRAM animates are taken
 * live, so that the water and the flowers move in step. */
/* The OAM a neighbour is drawn with: the sprites of the objects it may show.
 * Left out, by the tags the capture carries: Link, his companion and his items
 * (the live instance draws its own), the objects placed at random on entry and
 * those another object created, whose place the real entry would not reproduce
 *, and the blurb of the era or the season, which the view draws
 * itself at the top of the band.  Returns 0 when nothing is left to draw. */
static int neighbour_oam(OraclesEnhancedView *v, const entry *e, uint8_t out[160])
{
    memset(out, 0, 160);
    if (!v->neighbour_objects || !e->tag_count) return 0;
    /* The two frames captured in turn, one a frame: an object the game draws
     * one frame in two (a fountain's spurt) flickers in the neighbour as it
     * does in the room, instead of being there or not depending on the frame
     * the capture fell on.  The others are the same in both, frozen. */
    const int before = e->tag_count_before && (v->frame & 1u);
    const uint8_t *oam = before ? e->oam_before : e->oam;
    const OraclesSpriteTag *tags = before ? e->tags_before : e->tags;
    const unsigned tag_count = before ? e->tag_count_before : e->tag_count;
    int any = 0;
    for (unsigned i = 0; i < tag_count; i++) {
        const OraclesSpriteTag *tag = &tags[i];
        if (tag->kind == 0) continue;   /* Link, the companion, an item */
        const OraclesObjectRecord *object = NULL;
        for (unsigned k = 0; k < e->object_count; k++)
            if (e->objects[k].kind == tag->kind && e->objects[k].slot == tag->slot) { object = &e->objects[k]; break; }
        if (!object || !object->number || object->randomly_placed) continue;
        if (object->kind == ORACLES_OBJECT_KIND_INTERACTION && object->id == INTERAC_ERA_OR_SEASON_INFO) continue;
        for (unsigned n = 0; n < tag->count; n++) {
            const unsigned index = (unsigned)tag->first + n;
            if (index >= 40u) break;
            memcpy(out + index * 4u, oam + index * 4u, 4u);
            any = 1;
        }
    }
    return any;
}

/* The tiles a neighbour's map draws with, once for its map: what its render
 * takes from the live tiles is among them. */
void ev_entry_used_tiles(entry *e)
{
    memset(e->used_tiles, 0, sizeof e->used_tiles);
    const int unsigned_tiles = (e->regs3[0] & 0x10u) != 0;
    for (unsigned i = 0; i < 0x400u; i++) {
        const uint8_t n = e->bg_map[i], attr = e->bg_map[0x800u + i];
        const unsigned tile = unsigned_tiles ? n : (n < 128u ? 256u + n : n), bank = (attr >> 3) & 1u;
        e->used_tiles[(bank * 384u + tile) >> 3] |= (uint8_t)(1u << ((bank * 384u + tile) & 7u));
    }
}

/* What a neighbour's render reads beyond what its delivery fixed: the live
 * tiles it takes, the tiles of its own its animation has written, its
 * objects, the fade and the palettes.  Equal, the render it made is the one
 * it would make again. */
static uint64_t render_inputs(const OraclesEnhancedView *v, const entry *e, int in_step, const uint8_t *vram0, const uint8_t *vram1,
                              const uint8_t *animated, int draw_objects, const uint8_t oam[160], int use_live, const uint8_t *live_palettes)
{
    uint64_t key = ORACLES_HASH_SEED;
    const uint8_t flags[4] = { (uint8_t)in_step, (uint8_t)draw_objects, (uint8_t)use_live, (uint8_t)v->fade_effective };
    key = oracles_guest_hash(flags, sizeof flags, key);
    for (unsigned i = 0; i < sizeof e->used_tiles; i++) {
        /* The live tiles it takes: in step, the animated ones its map draws
         * with; otherwise those of its animation's images its map draws with. */
        uint8_t live = 0;
        if (in_step) live = (uint8_t)(animated[i] & e->used_tiles[i]);
        /* Its own tiles that may differ from its delivery: its animation's,
         * but those the live ones replace; all of them while it draws objects,
         * whose tiles its map need not use. */
        const uint8_t own = (uint8_t)(e->own_animated[i] & (draw_objects ? 0xffu : e->used_tiles[i]) & ~live);
        if (!live && !own) continue;
        const uint8_t place[3] = { (uint8_t)i, live, own };
        key = oracles_guest_hash(place, sizeof place, key);
        for (unsigned b = 0; b < 8u; b++) {
            const unsigned bit = i * 8u + b, bank = bit / 384u, tile = bit % 384u;
            if (live & (1u << b)) key = oracles_guest_hash((bank ? vram1 : vram0) + tile * 16u, 16u, key);
            if (own & (1u << b)) key = oracles_guest_hash(e->tiles + bank * ORACLES_GHOST_TILE_BYTES + tile * 16u, 16u, key);
        }
    }
    for (unsigned k = 0; k < e->image_count; k++) {
        /* An image's tile, written by the hand-over of the live animation, and
         * the live one it may take instead. */
        const unsigned bit = e->image_tiles[k], bank = bit / 384u, tile = bit % 384u;
        const int used = (e->used_tiles[bit >> 3] & (1u << (bit & 7u))) != 0, replaced = in_step && used && (animated[bit >> 3] & (1u << (bit & 7u)));
        if ((used || draw_objects) && !replaced) key = oracles_guest_hash(e->tiles + bank * ORACLES_GHOST_TILE_BYTES + tile * 16u, 16u, key);
        if (!in_step && used) key = oracles_guest_hash((bank ? vram1 : vram0) + tile * 16u, 16u, key);
    }
    if (draw_objects) key = oracles_guest_hash(oam, 160u, key);
    if (use_live) key = oracles_guest_hash(live_palettes, 64u, key);
    if (draw_objects && use_live) key = oracles_guest_hash(oracles_guest_obj_palettes(v->guest), 64u, key);
    const uint8_t pipeline = (uint8_t)v->colours_pipeline;
    return oracles_guest_hash(&pipeline, 1, key);
}

/* The lines of a small room's render (0 to its height) that show one of the
 * `changed` tiles: in its map, at the place its registers show, or in one of its
 * objects.  Every other line reads the same tiles, palettes and objects as when it
 * was drawn, and would be drawn the same. */
static unsigned changed_lines(const entry *e, const uint8_t *oam, int draw_objects, uint8_t lcdc, const uint8_t changed[2u * 384u / 8u],
                              uint8_t lines[ORACLES_GHOST_AREA_HEIGHT])
{
    memset(lines, 0, ORACLES_GHOST_AREA_HEIGHT);
    const unsigned scy = (uint8_t)(e->regs3[1] - e->camera_y), scx = (uint8_t)(e->regs3[2] - e->camera_x), unsigned_tiles = (e->regs3[0] & 0x10u) != 0;
    uint32_t rows_changed = 0;   /* the map's rows of 8 lines showing a changed tile in the columns shown */
    for (unsigned row = 0; row < 32u; row++)
        for (unsigned x = 0; x <= ORACLES_GHOST_AREA_WIDTH; x += 8u) {
            const unsigned col = ((scx + x) >> 3) & 31u;
            const uint8_t n = e->bg_map[row * 32u + col], attr = e->bg_map[0x800u + row * 32u + col];
            const unsigned bit = ((attr >> 3) & 1u) * 384u + (unsigned_tiles ? n : (n < 128u ? 256u + n : n));
            if (changed[bit >> 3] & (1u << (bit & 7u))) { rows_changed |= 1u << row; break; }
        }
    unsigned count = 0;
    for (unsigned y = 0; y < ORACLES_GHOST_AREA_HEIGHT; y++)
        if (rows_changed & (1u << (((scy + ORACLES_ENHANCED_HUD_HEIGHT + y) >> 3) & 31u))) { lines[y] = 1; count++; }
    if (!draw_objects) return count;
    const unsigned height = (lcdc & 0x04u) ? 16u : 8u;
    for (unsigned i = 0; i < 40u; i++) {
        const uint8_t *o = oam + i * 4u;
        const unsigned bank = (o[3] >> 3) & 1u, tile = height == 16u ? o[2] & 0xfeu : o[2];
        int uses = 0;
        for (unsigned t = tile; t < tile + height / 8u; t++) if (changed[(bank * 384u + t) >> 3] & (1u << ((bank * 384u + t) & 7u))) uses = 1;
        if (!uses) continue;
        for (unsigned k = 0; k < height; k++) {
            const int y = (int)o[0] - 16 + (int)k - (int)ORACLES_ENHANCED_HUD_HEIGHT;
            if (y >= 0 && y < (int)ORACLES_GHOST_AREA_HEIGHT && !lines[y]) { lines[y] = 1; count++; }
        }
    }
    return count;
}

const uint32_t *ev_neighbour_pixels(OraclesEnhancedView *v, entry *e)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    /* A neighbour of the live room's tileset and animation takes the tiles the
     * live VRAM animates, in step with the room across the edge; any other is
     * animated by its own animation, advanced from the game's data in its own
     * tiles (ev_advance_neighbour_animations). */
    const int in_step = e->tileset_gfx == oracles_guest_read8(v->guest, t->tileset_gfx)
        && e->animation.tileset_animation == oracles_guest_read8(v->guest, t->tileset_animation);
    /* During a scroll the view animates by itself, the tiles of the live
     * tileset are its own (view_scroll.c). */
    const uint8_t *vram0 = ev_shown_vram(v, 0), *vram1 = ev_shown_vram(v, 1), *animated = ev_shown_animated(v);
    if (!vram0 || !vram1) return e->game_area;
    uint64_t hash = ORACLES_HASH_SEED;
    const uint64_t live_tiles = v->have_live_tiles_hash ? v->live_tiles_hash : oracles_guest_hash(vram1, TILE_DATA_BYTES, oracles_guest_hash(vram0, TILE_DATA_BYTES, ORACLES_HASH_SEED));
    if (in_step) {
        hash = live_tiles;
        hash = oracles_guest_hash(animated, sizeof v->animated, hash);
    }
    uint8_t oam[160];
    const int draw_objects = neighbour_oam(v, e, oam);
    if (draw_objects) hash = oracles_guest_hash(oam, sizeof oam, hash);
    const uint8_t fade_byte = (uint8_t)v->fade_effective, pipeline = (uint8_t)v->colours_pipeline;
    hash = oracles_guest_hash(&fade_byte, 1, hash);
    hash = oracles_guest_hash(&pipeline, 1, hash);
    /* While the game fades, the whole band is drawn with the live palettes,
     * the fade in them, as the game draws whatever is on its screen (the
     * room being entered included, in the palettes of the room left): every
     * room reaches white or black on the same frame.  A room of the live
     * palette header takes the tint a palette transition left on screen.
     * Otherwise its own. */
    const uint8_t *live_palettes = oracles_guest_bg_palettes(v->guest);
    const int fading = v->fade_effective != 0 && live_palettes != NULL;
    const int use_live = fading || neighbour_takes_live_palettes(v, e, live_palettes);
    const uint8_t use_live_byte = (uint8_t)use_live;
    hash = oracles_guest_hash(&use_live_byte, 1, hash);
    if (use_live) hash = oracles_guest_hash(live_palettes, 64u, hash);
    /* Its own animation: rendered again when a copy has changed its tiles or
     * the live tiles it takes have changed, a few such neighbours a frame
     * (the others a frame later), so that rooms whose tiles turn on the same
     * frame do not all render at once. */
    const uint64_t base = hash;
    if (!in_step) {
        const uint8_t version[4] = { (uint8_t)e->animation_version, (uint8_t)(e->animation_version >> 8), (uint8_t)(e->animation_version >> 16), (uint8_t)(e->animation_version >> 24) };
        hash = oracles_guest_hash(version, sizeof version, hash);
        /* The live tiles it takes when they show one of its images. */
        if (e->image_count) {
            const uint8_t live[8] = { (uint8_t)live_tiles, (uint8_t)(live_tiles >> 8), (uint8_t)(live_tiles >> 16), (uint8_t)(live_tiles >> 24),
                                      (uint8_t)(live_tiles >> 32), (uint8_t)(live_tiles >> 40), (uint8_t)(live_tiles >> 48), (uint8_t)(live_tiles >> 56) };
            hash = oracles_guest_hash(live, sizeof live, hash);
        }
    }
    const int animation_only = e->live_valid && !in_step && e->live_base_hash == base && e->live_tiles_hash != hash;
    if (animation_only && v->animation_renders >= ANIMATION_RENDERS_PER_FRAME) return e->live_area;
    if (!e->live_valid || e->live_tiles_hash != hash) {
        if (animation_only) v->animation_renders++;
        e->live_base_hash = base;
        /* The live tiles change every frame the game streams its objects' in:
         * a render reads few of them, and is made again only when they change. */
        const uint64_t inputs = render_inputs(v, e, in_step, vram0, vram1, animated, draw_objects, oam, use_live, live_palettes);
        if (e->live_valid && e->live_inputs == inputs) {
            e->live_tiles_hash = hash;
            v->live_renders_kept++;
            return e->live_area;
        }
        e->live_inputs = inputs;
        memcpy(v->hybrid_vram, e->tiles, TILE_DATA_BYTES);
        memcpy(v->hybrid_vram + MAP_OFFSET, e->bg_map, 0x800u);
        memcpy(v->hybrid_vram + 0x2000u, e->tiles + ORACLES_GHOST_TILE_BYTES, TILE_DATA_BYTES);
        memcpy(v->hybrid_vram + 0x2000u + MAP_OFFSET, e->bg_map + 0x800u, 0x800u);
        /* Only the tiles the neighbour's own map draws with: the live VRAM
         * also animates the tiles of the objects the game is drawing (their
         * frames reach it through the vblank queue), and those belong to the
         * live room's objects, not to the neighbour's, which take theirs
         * from the ghost. */
        if (in_step || e->image_count) {
            const uint8_t *used = e->used_tiles;
            if (in_step) {
                for (unsigned bank = 0; bank < 2; bank++)
                    for (unsigned tile = 0; tile < 384u; tile++) {
                        const unsigned bit = bank * 384u + tile;
                        if ((animated[bit >> 3] & used[bit >> 3] & (uint8_t)(1u << (bit & 7u))) != 0)
                            memcpy(v->hybrid_vram + bank * 0x2000u + tile * 16u, (bank ? vram1 : vram0) + tile * 16u, 16u);
                    }
            } else {
                /* Its own animation, but the live tile wherever the live room
                 * shows on it an image this animation can put there: the same
                 * element, kept in step with the room across the edge. */
                for (unsigned k = 0; k < e->image_count; k++) {
                    const unsigned bit = e->image_tiles[k], bank = bit / 384u, tile = bit % 384u;
                    if (!(used[bit >> 3] & (1u << (bit & 7u)))) continue;
                    const uint8_t *live = (bank ? vram1 : vram0) + tile * 16u;
                    if (e->image_sources[k] + 16u <= v->rom_size && memcmp(live, v->rom + e->image_sources[k], 16u) == 0)
                    { memcpy(v->hybrid_vram + bank * 0x2000u + tile * 16u, live, 16u); v->image_live_tiles++; }
                }
            }
        }
        uint8_t palettes[64];
        if (use_live) memcpy(palettes, live_palettes, sizeof palettes);
        else faded_palettes(e->bg_palettes, v->fade_effective, palettes);
        OraclesPpuInput in;
        in.vram = v->hybrid_vram;
        in.oam = draw_objects ? oam : oracles_guest_oam(v->guest);
        in.bg_palettes = palettes;
        /* The neighbour's own sprite palettes while it draws its own objects;
         * the live ones while the game fades, as for the background. */
        in.obj_palettes = draw_objects && !use_live ? e->obj_palettes : oracles_guest_obj_palettes(v->guest);
        in.colours = v->colours;
        const unsigned width = e->large ? LARGE_ROOM_W : ORACLES_GHOST_AREA_WIDTH, height = e->large ? LARGE_ROOM_H : ORACLES_GHOST_AREA_HEIGHT;
        /* Bit 1 of LCDC: the objects are drawn only when the neighbour has
         * some to show, and never in a large room, which the view shows alone. */
        const uint8_t lcdc = (uint8_t)(draw_objects ? (e->regs3[0] | 0x82u) : ((e->regs3[0] | 0x80u) & ~0x02u));
        /* What the render reads besides its tiles; the same as the last render's,
         * only the lines showing a tile that changed since are drawn again.  A
         * large room is drawn whole, and so is a room under a window, whose lines
         * the window's own count ties together. */
        uint64_t rest = oracles_guest_hash(palettes, sizeof palettes, ORACLES_HASH_SEED);
        rest = oracles_guest_hash(&lcdc, 1, rest);
        if (draw_objects) rest = oracles_guest_hash(in.obj_palettes, 64u, oracles_guest_hash(oam, 160u, rest));
        uint8_t lines[ORACLES_GHOST_AREA_HEIGHT];
        unsigned count = ORACLES_GHOST_AREA_HEIGHT;
        if (e->live_valid && !e->large && e->rendered_rest == rest && !((lcdc & 0x20u) && e->regs3[4] <= 166u)) {
            uint8_t changed[2u * 384u / 8u];
            memset(changed, 0, sizeof changed);
            for (unsigned bank = 0; bank < 2u; bank++)
                for (unsigned tile = 0; tile < 384u; tile++)
                    if (memcmp(v->hybrid_vram + bank * 0x2000u + tile * 16u, e->rendered_tiles + bank * ORACLES_GHOST_TILE_BYTES + tile * 16u, 16u) != 0)
                        changed[(bank * 384u + tile) >> 3] |= (uint8_t)(1u << ((bank * 384u + tile) & 7u));
            count = changed_lines(e, oam, draw_objects, lcdc, changed, lines);
        } else memset(lines, 1, sizeof lines);
        e->rendered_rest = rest;
        memcpy(e->rendered_tiles, v->hybrid_vram, ORACLES_GHOST_TILE_BYTES);
        memcpy(e->rendered_tiles + ORACLES_GHOST_TILE_BYTES, v->hybrid_vram + 0x2000u, ORACLES_GHOST_TILE_BYTES);
        if (count < ORACLES_GHOST_AREA_HEIGHT) {
            v->live_lines_kept += ORACLES_GHOST_AREA_HEIGHT - count;
            const OraclesPpuRegs r = { lcdc, (uint8_t)(e->regs3[1] - e->camera_y), (uint8_t)(e->regs3[2] - e->camera_x), e->regs3[3], e->regs3[4] };
            for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) in.lines[ly] = r;
            for (unsigned y = 0; y < ORACLES_GHOST_AREA_HEIGHT;) {   /* each run of lines to draw */
                if (!lines[y]) { y++; continue; }
                unsigned end = y;
                while (end < ORACLES_GHOST_AREA_HEIGHT && lines[end]) end++;
                oracles_ppu_render_wide_lines(&in, v->strip, width, ORACLES_ENHANCED_HUD_HEIGHT + y, ORACLES_ENHANCED_HUD_HEIGHT + end);
                memcpy(e->live_area + y * width, v->strip + (ORACLES_ENHANCED_HUD_HEIGHT + y) * width, (end - y) * width * sizeof *v->strip);
                y = end;
            }
        } else for (unsigned pass = 0; pass * ORACLES_ENHANCED_AREA_HEIGHT < height; pass++) {
            const OraclesPpuRegs r = { lcdc, (uint8_t)(e->regs3[1] - e->camera_y + pass * ORACLES_ENHANCED_AREA_HEIGHT), (uint8_t)(e->regs3[2] - e->camera_x), e->regs3[3], e->regs3[4] };
            for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) in.lines[ly] = r;
            const unsigned rows = height - pass * ORACLES_ENHANCED_AREA_HEIGHT < ORACLES_ENHANCED_AREA_HEIGHT ? height - pass * ORACLES_ENHANCED_AREA_HEIGHT : ORACLES_ENHANCED_AREA_HEIGHT;
            oracles_ppu_render_wide_lines(&in, v->strip, width, ORACLES_ENHANCED_HUD_HEIGHT, ORACLES_ENHANCED_HUD_HEIGHT + rows);   /* the lines kept */
            memcpy(e->live_area + pass * ORACLES_ENHANCED_AREA_HEIGHT * width, v->strip + ORACLES_ENHANCED_HUD_HEIGHT * width, rows * width * sizeof *v->strip);
        }
        /* Blocks that differ from the ghost's render although their tile is
         * not one the live VRAM animates: none if the live render is right.
         * The ghost's render has no objects, so a neighbour drawing its own
         * is not compared. */
        if (in_step && !e->large && !use_live && !draw_objects) {
            unsigned blocks = 0;
            const unsigned scy = e->regs3[1], scx = e->regs3[2], unsigned_tiles = (e->regs3[0] & 0x10u) != 0;
            for (unsigned by = 0; by < ORACLES_GHOST_AREA_HEIGHT / 8u; by++)
                for (unsigned bx = 0; bx < ORACLES_GHOST_AREA_WIDTH / 8u; bx++) {
                    const unsigned row = ((scy + ORACLES_ENHANCED_HUD_HEIGHT + by * 8u) >> 3) & 31u, col = ((scx + bx * 8u) >> 3) & 31u;
                    const uint8_t n = e->bg_map[row * 32u + col], attr = e->bg_map[0x800u + row * 32u + col];
                    const unsigned tile = unsigned_tiles ? n : (n < 128u ? 256u + n : n), bank = (attr >> 3) & 1u;
                    if (animated[(bank * 384u + tile) >> 3] & (1u << ((bank * 384u + tile) & 7u))) continue;
                    if (e->own_animated[(bank * 384u + tile) >> 3] & (1u << ((bank * 384u + tile) & 7u))) continue;   /* its own animation moved it */
                    int differs = 0;
                    for (unsigned y = 0; y < 8u && !differs; y++)
                        if (memcmp(e->live_area + (by * 8u + y) * ORACLES_GHOST_AREA_WIDTH + bx * 8u, e->game_area + (by * 8u + y) * ORACLES_GHOST_AREA_WIDTH + bx * 8u, 8u * sizeof *e->live_area) != 0) differs = 1;
                    blocks += (unsigned)differs;
                }
            if (blocks > v->live_diff_max) v->live_diff_max = blocks;
        }
        e->live_tiles_hash = hash;
        e->live_valid = 1;
        v->live_renders++;
    }
    return e->live_area;
}

/* The whole terrain of a large room: the game keeps the room (15x11
 * metatiles, 240x176) in its 32x32 background map, so the software PPU
 * renders it from the live VRAM with the committed game-area registers
 * (wGfxRegs3) less the game's camera, in two passes of 128 and 48 lines,
 * objects left out.  The vertical camera then shows any part of it. */
void ev_render_large_room(OraclesEnhancedView *v)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const uint8_t *regs = oracles_guest_ptr(v->guest, t->gfx_regs3, 6);
    v->strip_valid = 0;
    if (!regs) return;
    ev_refresh_colours(v);
    OraclesPpuInput in;
    in.vram = oracles_guest_vram(v->guest, 0);
    in.oam = oracles_guest_oam(v->guest);
    in.bg_palettes = oracles_guest_bg_palettes(v->guest);
    in.obj_palettes = oracles_guest_obj_palettes(v->guest);
    in.colours = v->raw_colours;   /* RGB555 first: the capture kept for a scroll is faded by the compose with the game */
    const int camera_x = v->observation.drawn_camera_x, camera_y = v->observation.drawn_camera_y;   /* the camera the registers on screen were copied from */
    for (unsigned pass = 0; pass < 2; pass++) {
        const OraclesPpuRegs r = { (uint8_t)((regs[0] | 0x80u) & ~0x02u), (uint8_t)(regs[1] - camera_y + pass * ORACLES_ENHANCED_AREA_HEIGHT), (uint8_t)(regs[2] - camera_x), regs[3], regs[4] };
        for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) in.lines[ly] = r;
        const unsigned rows = pass ? LARGE_ROOM_H - ORACLES_ENHANCED_AREA_HEIGHT : ORACLES_ENHANCED_AREA_HEIGHT;
        oracles_ppu_render_wide_lines(&in, v->strip, LARGE_ROOM_W, ORACLES_ENHANCED_HUD_HEIGHT, ORACLES_ENHANCED_HUD_HEIGHT + rows);   /* the lines kept */
        memcpy(v->strip_raw + pass * ORACLES_ENHANCED_AREA_HEIGHT * LARGE_ROOM_W, v->strip + ORACLES_ENHANCED_HUD_HEIGHT * LARGE_ROOM_W, rows * LARGE_ROOM_W * sizeof *v->strip);
    }
    for (unsigned i = 0; i < LARGE_ROOM_W * LARGE_ROOM_H; i++) v->strip_area[i] = v->colours[v->strip_raw[i] & (ORACLES_PPU_COLOURS - 1u)];
    v->strip_valid = 1;
}
