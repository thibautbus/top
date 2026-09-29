/* The ghost instance: a second core loaded from an
 * in-memory savestate of the live instance, in which the game itself enters a
 * neighbouring room by its own scrolling transition.  This file runs it; the
 * cache key and the read trace are in ghost_key.c, the tables read from the
 * ROM in ghost_data.c. */
#include "ghost_internal.h"

static int bank0_symbol(OraclesGuestSym sym);
static void freeze_objects(OraclesGhost *g);
static void unfreeze_objects(OraclesGhost *g);

/* ---- events of the ghost's own guest ------------------------------------------- */

static void on_event(void *opaque, const OraclesGuestEvent *event)
{
    OraclesGhost *g = opaque;
    const OraclesGuestTables *t = oracles_guest_tables(g->guest);
    oracles_objects_event(g->objects, event);
    oracles_sprites_event(g->sprites, event);
    switch (event->type) {
    case ORACLES_EVENT_TILE_SUBSTITUTIONS:
        g->in_substitutions = 1;
        if (g->tracing) { oracles_ghost_trace_stack_of_entry(g); oracles_guest_enable_read_trace(g->guest, 1); }
        break;
    case ORACLES_EVENT_TILE_SUBSTITUTIONS_DONE:
        g->in_substitutions = 0;
        oracles_guest_enable_read_trace(g->guest, g->load_counts && g->primed);   /* the load's trace goes on */
        break;
    /* Seasons' applyAllTileSubstitutions loads Subrosia's object graphics on
     * its way (loadSubrosiaObjectGfxHeader): refreshObjectGfx scans the object
     * table and the loaded graphics, over several frames, and reads no tile
     * state.  The trace pauses for it and resumes at its return. */
    case ORACLES_EVENT_OBJECT_GFX_LOAD:
        g->in_object_gfx = 1;   /* the load's trace, when on, counts these reads as the load's */
        if (g->tracing && !g->load_counts) oracles_guest_enable_read_trace(g->guest, 0);
        break;
    case ORACLES_EVENT_OBJECT_GFX_LOAD_DONE:
        g->in_object_gfx = 0;
        if (g->tracing && g->in_substitutions) oracles_guest_enable_read_trace(g->guest, 1);
        break;
    case ORACLES_EVENT_CHECK_ROOM_PACK: {
        /* Seasons, the Enhanced band in one season: the room a forced scroll
         * enters is given the area the ghost is in, so that checkRoomPack keeps
         * the season it holds (the live one, the rod's included) instead of the
         * entered area's own and its fade; the room's tileset was loaded in that
         * season just before (loadTilesetData).  The live game crosses with its
         * flash into the area's season, and the band follows then. */
        if (!g->active || !g->primed || g->primed_hold < 0) break;
        uint8_t *wram = oracles_guest_wram_writable(g->guest, 0);
        if (!wram || !bank0_symbol(t->room_pack) || !bank0_symbol(t->loading_room_pack) || !bank0_symbol(t->active_group)) break;
        if (wram[t->active_group.addr - WRAM_BANK0_BASE] != 0) break;
        /* Not into an area whose season never changes (oracles_ghost_area_holds_season):
         * the game's own fade sets it, and the band shows what the game will. */
        if (g->fixed_season[wram[t->loading_room_pack.addr - WRAM_BANK0_BASE]]) break;
        wram[t->room_pack.addr - WRAM_BANK0_BASE] = wram[t->loading_room_pack.addr - WRAM_BANK0_BASE];
        break;
    }
    case ORACLES_EVENT_ROOM_INITIALIZED:
        if (!g->active || !g->primed || g->initialized || !g->result) break;   /* a pre-run loads the live room first */
        if (g->capture) freeze_objects(g);
        g->initialized = 1;
        g->result->group = oracles_guest_read8(g->guest, t->active_group);
        g->result->room = oracles_guest_read8(g->guest, t->active_room);
        g->result->room_is_large = oracles_guest_read8(g->guest, t->room_is_large);
        {
            const uint8_t *layout = oracles_guest_ptr(g->guest, t->room_layout, ORACLES_GHOST_LAYOUT_BYTES);
            if (layout) memcpy(g->result->layout, layout, ORACLES_GHOST_LAYOUT_BYTES);
            else memset(g->result->layout, 0, ORACLES_GHOST_LAYOUT_BYTES);
        }
        break;
    default:
        break;
    }
}

/* ---- create / destroy ---------------------------------------------------------- */

OraclesGhost *oracles_ghost_create(const uint8_t *rom, size_t rom_size, const OraclesCompatProfile *profile)
{
    OraclesGhost *g = calloc(1, sizeof *g);
    if (!g) return NULL;
    const OraclesCoreOptions options = { 0, 0 };   /* no audio, raw colours: the ghost is never shown */
    g->core = oracles_core_create(rom, rom_size, &options);
    if (!g->core) { free(g); return NULL; }
    g->guest = oracles_guest_attach(g->core, profile);
    if (!g->guest) { oracles_core_destroy(g->core); free(g); return NULL; }
    const OraclesGuestTables *t = oracles_guest_tables(g->guest);
    oracles_guest_clear_hooks(g->guest);
    oracles_guest_add_hook(g->guest, t->initialize_room, ORACLES_EVENT_ROOM_ENTER, ORACLES_EVENT_ROOM_INITIALIZED);
    oracles_guest_add_hook(g->guest, t->apply_all_tile_substitutions, ORACLES_EVENT_TILE_SUBSTITUTIONS, ORACLES_EVENT_TILE_SUBSTITUTIONS_DONE);
    /* The room's objects and the number each receives at its creation. */
    oracles_guest_add_hook(g->guest, t->get_free_interaction_slot, 0, ORACLES_EVENT_INTERACTION_CREATED);
    oracles_guest_add_hook(g->guest, t->get_free_enemy_slot_uncounted, 0, ORACLES_EVENT_ENEMY_CREATED);
    oracles_guest_add_hook(g->guest, t->get_free_part_slot, 0, ORACLES_EVENT_PART_CREATED);
    /* The frame's drawing bounds the tagging of the sprites. */
    oracles_guest_add_hook(g->guest, t->draw_all_sprites, ORACLES_EVENT_FRAME_DONE, ORACLES_EVENT_FRAME_DRAWN);
    int hooks_full = oracles_objects_add_hooks(g->guest) != 0 || oracles_sprites_add_hooks(g->guest) != 0;
    if (oracles_compat_seasons_rules(oracles_guest_profile(g->guest)))
        hooks_full |= oracles_guest_add_hook(g->guest, t->check_room_pack, ORACLES_EVENT_CHECK_ROOM_PACK, 0) != 0;
    hooks_full |= oracles_guest_add_hook(g->guest, t->load_object_gfx_header_to_slot4, ORACLES_EVENT_OBJECT_GFX_LOAD, ORACLES_EVENT_OBJECT_GFX_LOAD_DONE) != 0;
    g->objects = oracles_objects_create(g->guest);
    g->sprites = oracles_sprites_create(g->guest);
    if (hooks_full || !g->objects || !g->sprites) {
        oracles_objects_destroy(g->objects);
        oracles_sprites_destroy(g->sprites);
        oracles_guest_detach(g->guest);
        oracles_core_destroy(g->core);
        free(g);
        return NULL;
    }
    oracles_guest_set_event_sink(g->guest, on_event, g);
    oracles_ghost_find_fixed_seasons(g, rom, rom_size);
    oracles_ghost_find_open_water_rooms(g, rom, rom_size);
    oracles_ghost_find_map_rooms(g, rom, rom_size);
    oracles_ghost_find_self_routed_rooms(g, rom, rom_size);
    g->hold_request = g->job_hold = g->run_hold = g->primed_hold = -1;
    g->level_request = g->job_level = g->run_level = g->await_group = -1;
    g->state_size = oracles_core_state_size(g->core);
    g->layout_start = t->room_layout.addr;
    g->layout_end = (uint16_t)(t->room_layout.addr + 0x100u);   /* the search loops of the substitutions run over the whole page, past wRoomLayoutEnd into wTmpcfc0 */
    g->stacks_start = t->main_stack.addr;
    g->stacks_end = t->thread3_stack_top.addr;
    pthread_mutex_init(&g->mutex, NULL);
    pthread_cond_init(&g->cond, NULL);
    return g;
}

void oracles_ghost_destroy(OraclesGhost *g)
{
    if (!g) return;
    if (g->thread_started) {
        pthread_mutex_lock(&g->mutex);
        g->quit = 1;
        pthread_cond_broadcast(&g->cond);
        pthread_mutex_unlock(&g->mutex);
        pthread_join(g->thread, NULL);
    }
    pthread_cond_destroy(&g->cond);
    pthread_mutex_destroy(&g->mutex);
    oracles_objects_destroy(g->objects);
    oracles_sprites_destroy(g->sprites);
    oracles_guest_detach(g->guest);
    oracles_core_destroy(g->core);
    free(g->full_frame);
    free(g->settled_state);
    free(g->colours_copy);
    free(g->job_colours_copy);
    free(g->job_state);
    free(g->read_counts);
    free(g->load_counts);
    free(g);
}

size_t oracles_ghost_state_size(OraclesGhost *g) { return g->state_size; }

/* ---- priming ------------------------------------------------------------------- */

int oracles_ghost_prerunnable(OraclesGuest *guest, const char **reason)
{
    const OraclesGuestTables *t = oracles_guest_tables(guest);
    const char *why = NULL;
    /* A room is loading, or a warp is under way (wWarpTransition holds its
     * type from the warp's start to its end): the ghost can play the rest of
     * it itself, a cutscene that carries the warp included (the time travel
     * runs its own before the load), and prime in the room reached. */
    /* Bit 7 of wScrollMode: the game's camera stepped inside a large room this frame, not a mode. */
    const int room_loading = (oracles_guest_read8(guest, t->scroll_mode) & 0x7fu) != NORMAL_PLAY_SCROLL_MODE
                          || oracles_guest_read8(guest, t->screen_transition_state) != TRANSITION_STATE_IDLE;
    const int warp_under_way = oracles_guest_read8(guest, t->warp_transition) != 0;
    if (oracles_guest_read8(guest, t->game_state) != GAME_STATE_PLAYING) why = "game state is not 2 (playing)";
    else if (oracles_guest_read8(guest, t->text_is_active) != 0) why = "text is active";
    else if (oracles_guest_read8(guest, t->cutscene_index) > CUTSCENE_INGAME && !room_loading && !warp_under_way) why = "a cutscene is running";
    else if (oracles_guest_read8(guest, t->cutscene_index) != 0 && !room_loading && !warp_under_way) why = "no room is loading";
    if (reason) *reason = why;
    return why == NULL;
}

void oracles_ghost_set_prerun(OraclesGhost *g, unsigned max_frames) { g->prerun_max = max_frames; }
void oracles_ghost_set_held_season(OraclesGhost *g, int season)
{
    pthread_mutex_lock(&g->mutex);
    g->hold_request = (season >= 0 && season <= 3) || season == ORACLES_GHOST_HOLD_OWN ? season : -1;
    if (!g->thread_started) g->run_hold = g->hold_request;   /* synchronous runs read it directly */
    pthread_mutex_unlock(&g->mutex);
}

void oracles_ghost_set_level_change(OraclesGhost *g, int group)
{
    pthread_mutex_lock(&g->mutex);
    g->level_request = group >= 0 && group <= 7 ? group : -1;
    if (!g->thread_started) g->run_level = g->level_request;   /* synchronous runs read it directly */
    pthread_mutex_unlock(&g->mutex);
}

int oracles_ghost_primeable(OraclesGuest *guest, const char **reason)
{
    const OraclesGuestTables *t = oracles_guest_tables(guest);
    const char *why = NULL;
    if (oracles_guest_read8(guest, t->game_state) != GAME_STATE_PLAYING) why = "game state is not 2 (playing)";
    else if (oracles_guest_read8(guest, t->cutscene_index) > CUTSCENE_INGAME) why = "a cutscene is running";
    /* Index 0 is the room load's own: the main loop is still in it, and a
     * transition forced then is reset by the initialisation that follows. */
    else if (oracles_guest_read8(guest, t->cutscene_index) < CUTSCENE_INGAME) why = "a room is loading";
    else if (oracles_guest_read8(guest, t->cutscene_trigger) != 0) why = "a cutscene is triggered";
    else if ((oracles_guest_read8(guest, t->scroll_mode) & 0x7fu) != NORMAL_PLAY_SCROLL_MODE) why = "scroll mode is not 1 (normal play)";
    else if (oracles_guest_read8(guest, t->screen_transition_state) != TRANSITION_STATE_IDLE) why = "a transition is in progress";
    else if (oracles_guest_read8(guest, t->text_is_active) != 0) why = "text is active";
    /* The frame Start opens the inventory, the game's state is still the room's:
     * a run from it plays the menu instead of the transition, and times out. */
    else if (oracles_guest_read8(guest, t->opened_menu_type) != 0) why = "a menu is open";
    else if (oracles_guest_read8(guest, t->disable_screen_transitions) != 0) why = "transitions are disabled";
    else if (oracles_guest_read8(guest, t->link_force_state) != 0) why = "Link's state is forced";
    /* Seasons: the rod of seasons set a season (setSeason writes wcc4c): the
     * main loop reloads the room through a fade before it looks at any
     * transition, and a forced one would run after that reload, elsewhere. */
    else if (oracles_compat_seasons_rules(oracles_guest_profile(guest))
             && oracles_guest_read8(guest, t->season_reload) != 0) why = "a season change is reloading the room";
    if (reason) *reason = why;
    return why == NULL;
}

static const OraclesGuestTables *t_of(const OraclesGhost *g) { return oracles_guest_tables(g->guest); }

static int bank0_symbol(OraclesGuestSym sym)
{
    return sym.bank != ORACLES_GUEST_ABSENT && sym.addr >= WRAM_BANK0_BASE && sym.addr < WRAM_BANKED_BASE;
}

/* The forced transition is decided by screenTransitionState2, but cutscene01
 * checks the warp tile under Link (func_60e9, initiateWarp) before
 * getNextActiveRoom: with Link standing on the door he has just come out
 * of, the ghost would warp instead of scrolling.  wDisableWarpTiles inhibits
 * that check; the ghost raises it for its run and restores the value before
 * keeping its settled state, so a chained run starts from a normal state.
 * The live instance is never touched. */
static int prime(OraclesGhost *g, OraclesGhostDirection direction)
{
    const OraclesGuestTables *t = oracles_guest_tables(g->guest);
    const OraclesGuestSym sym = t->screen_transition_direction, warp = t->disable_warp_tiles;
    if (!bank0_symbol(sym) || !bank0_symbol(warp)) return -1;
    uint8_t *wram = oracles_guest_wram_writable(g->guest, 0);
    if (!wram) return -1;
    g->prior_warp_tiles = wram[warp.addr - WRAM_BANK0_BASE];
    wram[warp.addr - WRAM_BANK0_BASE] = 1;
    g->warp_tiles_guarded = 1;
    wram[sym.addr - WRAM_BANK0_BASE] = (uint8_t)(FORCED_TRANSITION | ((unsigned)direction & 3u));
    /* Seasons, the band in one season: the run starts in the season the game
     * holds, whatever the state it starts from (a neighbour kept in an area of
     * a fixed season): an area that follows the season keeps it
     * (ORACLES_EVENT_CHECK_ROOM_PACK), a fixed one takes its own by the game's fade. */
    g->primed_hold = -1;
    if (g->run_hold != -1 && oracles_compat_seasons_rules(oracles_guest_profile(g->guest))
        && bank0_symbol(t->room_state_modifier) && bank0_symbol(t->active_group)
        && bank0_symbol(t->active_room) && wram[t->active_group.addr - WRAM_BANK0_BASE] == 0) {
        const unsigned room = wram[t->active_room.addr - WRAM_BANK0_BASE];
        const int step = direction == ORACLES_DIR_UP ? -16 : direction == ORACLES_DIR_DOWN ? 16 : direction == ORACLES_DIR_LEFT ? -1 : 1;
        const unsigned destination = (unsigned)((int)room + step) & 0xffu;
        g->primed_hold = g->run_hold == ORACLES_GHOST_HOLD_OWN ? wram[t->room_state_modifier.addr - WRAM_BANK0_BASE] : g->run_hold;
        /* Into a room of a fixed area from the same area the scroll sets no
         * season (checkRoomPack returns at once): the start room's own, the
         * area's, stands; from another area the fade sets it. */
        if (!(g->fixed_season[g->room_pack_of[destination]] && g->room_pack_of[destination] == g->room_pack_of[room]))
            wram[t->room_state_modifier.addr - WRAM_BANK0_BASE] = (uint8_t)g->primed_hold;
    }
    /* Link placed on the edge as screenTransitionState1 places him when he
     * touches it (5 + 1 up and left, the room's boundary down and right),
     * which the forced transition skips.  In a large room always: the
     * scroll's first step computes the camera from his position
     * (resetCamera), and the scroll ends only on a tile-aligned register (SCY
     * or SCX & 7, 4 px a step), so from the middle of the room it never ended.
     * In a small room only when the room reached would start another
     * transition: the scroll carries him by the room's size less the 15 px
     * (16 vertically) it walks him (finishScrollingTransition), and from far
     * enough from the edge he lands past one of the room's edges, which the
     * game crosses without input in the air, on a conveyor or a minecart (from
     * the top of a room, on the rod of seasons' stump, a scroll down went two
     * rooms down).
     * Elsewhere he keeps his place: the edge can hold a door the game's own
     * Link never stands on (Ages 0:57, a house's door at its top). */
    const OraclesGuestSym large = t->room_is_large, index = t->link_object_index;
    /* The object the game moves is the one at wLinkObjectIndex: Link, or what he rides ($d1). */
    const unsigned object = bank0_symbol(index) ? wram[index.addr - WRAM_BANK0_BASE] : 0u;
    uint8_t *objects = oracles_guest_wram_writable(g->guest, ORACLES_OBJECTS_BANK);
    const OraclesGuestSym boundary = (direction & 1u) ? t->screen_transition_boundary_x : t->screen_transition_boundary_y;
    if (objects && bank0_symbol(boundary) && bank0_symbol(large) && object >= (ORACLES_OBJECTS_BASE >> 8) && object <= 0xdfu) {
        uint8_t *axis = &objects[(object - (ORACLES_OBJECTS_BASE >> 8)) * 0x100u + ((direction & 1u) ? ORACLES_OBJ_XH : ORACLES_OBJ_YH)];
        const unsigned edge = (direction == ORACLES_DIR_UP || direction == ORACLES_DIR_LEFT) ? 6u : wram[boundary.addr - WRAM_BANK0_BASE];
        int place = wram[large.addr - WRAM_BANK0_BASE] != 0;
        if (!place) {
            const int size = (direction & 1u) ? 160 : 128, walked = (direction & 1u) ? 15 : 16;
            const int landing = (direction == ORACLES_DIR_UP || direction == ORACLES_DIR_LEFT) ? *axis - walked + size : *axis + walked - size;
            /* The room reached checks both edges of the axis (screenTransitionState1),
             * and starts a transition at one without input only on a minecart, or
             * not over a hole and in the air or, not knocked back, on a conveyor
             * (@transition): the ghost has no input, its Link's angle is none. */
            const unsigned land = (unsigned)landing & 0xffu;
            const int past = land <= 5u || land > wram[boundary.addr - WRAM_BANK0_BASE];
            const uint8_t *companion = oracles_guest_object(g->guest, 1, 0);
            const int minecart = companion && companion[ORACLES_OBJ_ID] == SPECIALOBJECT_MINECART;
            const unsigned ground = bank0_symbol(t->hole_or_conveyor) ? wram[t->hole_or_conveyor.addr - WRAM_BANK0_BASE] : 0x80u;
            const int in_air = bank0_symbol(t->link_in_air) && (wram[t->link_in_air.addr - WRAM_BANK0_BASE] & 0x80u);
            const int knocked = oracles_guest_read8(g->guest, t->link_knockback_counter) != 0;
            place = past && (minecart || (!(ground & 0x80u) && (in_air || (!knocked && (ground & 0x7fu)))));
        }
        if (place) *axis = (uint8_t)edge;
    }
    return 0;
}

/* The capture: the objects of the room entered are frozen where the
 * game creates them, and Link and his companion are hidden, so that the room
 * settles with its objects in their state of apparition and its sprites are
 * the room's own.  The bits are those the three update routines read; bit 7
 * freezes Link, the companion and the items as well (docs/GAME_HOOKS.md, section 4.3). */
#define DISABLE_OBJECTS_MASK 0x8eu

static void freeze_objects(OraclesGhost *g)
{
    const OraclesGuestTables *t = oracles_guest_tables(g->guest);
    uint8_t *wram = oracles_guest_wram_writable(g->guest, 0);
    if (!wram || !bank0_symbol(t->disabled_objects) || g->frozen) return;
    g->prior_disabled_objects = wram[t->disabled_objects.addr - WRAM_BANK0_BASE];
    wram[t->disabled_objects.addr - WRAM_BANK0_BASE] = (uint8_t)(g->prior_disabled_objects | DISABLE_OBJECTS_MASK);
    uint8_t *objects = oracles_guest_wram_writable(g->guest, ORACLES_OBJECTS_BANK);
    for (unsigned slot = 0; slot < 2u; slot++) {   /* Link, then his companion */
        if (!objects) break;
        uint8_t *visible = &objects[slot * 0x100u + ORACLES_OBJ_VISIBLE];
        g->prior_visible[slot] = *visible;
        *visible = (uint8_t)(*visible & 0x7fu);
    }
    g->frozen = 1;
}

static void unfreeze_objects(OraclesGhost *g)
{
    if (!g->frozen) return;
    const OraclesGuestTables *t = oracles_guest_tables(g->guest);
    uint8_t *wram = oracles_guest_wram_writable(g->guest, 0);
    if (wram && bank0_symbol(t->disabled_objects)) wram[t->disabled_objects.addr - WRAM_BANK0_BASE] = g->prior_disabled_objects;
    uint8_t *objects = oracles_guest_wram_writable(g->guest, ORACLES_OBJECTS_BANK);
    if (objects) for (unsigned slot = 0; slot < 2u; slot++) objects[slot * 0x100u + ORACLES_OBJ_VISIBLE] = g->prior_visible[slot];
    g->frozen = 0;
}

void oracles_ghost_set_capture(OraclesGhost *g, int enabled) { g->capture = enabled != 0; }

void oracles_ghost_sprite_stats(const OraclesGhost *g, unsigned *frames, unsigned *frames_uncovered, unsigned *worst_gap, unsigned *tags_dropped)
{
    oracles_sprites_stats(g->sprites, frames, frames_uncovered, worst_gap, tags_dropped);
}

unsigned oracles_ghost_frames_oam_full(const OraclesGhost *g) { return oracles_sprites_frames_oam_full(g->sprites); }

static void restore_warp_tiles(OraclesGhost *g)
{
    if (!g->warp_tiles_guarded) return;
    const OraclesGuestSym warp = oracles_guest_tables(g->guest)->disable_warp_tiles;
    uint8_t *wram = oracles_guest_wram_writable(g->guest, 0);
    if (wram && bank0_symbol(warp)) wram[warp.addr - WRAM_BANK0_BASE] = g->prior_warp_tiles;
    g->warp_tiles_guarded = 0;
}

/* The room's game area as the game displays it: the committed game-area
 * registers (wGfxRegs3), the ghost's VRAM and palettes, objects left out. */
static void render_terrain(OraclesGhost *g, uint32_t *out)
{
    const OraclesGuestTables *t = oracles_guest_tables(g->guest);
    const uint8_t *regs = oracles_guest_ptr(g->guest, t->gfx_regs3, 6);
    if (!regs) { memset(out, 0, ORACLES_GHOST_AREA_WIDTH * ORACLES_GHOST_AREA_HEIGHT * sizeof *out); return; }
    if (!g->full_frame) g->full_frame = malloc(ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT * sizeof *g->full_frame);
    if (!g->full_frame) return;
    OraclesPpuInput in;
    in.vram = oracles_guest_vram(g->guest, 0);
    in.oam = oracles_guest_oam(g->guest);
    in.bg_palettes = oracles_guest_bg_palettes(g->guest);
    in.obj_palettes = oracles_guest_obj_palettes(g->guest);
    in.colours = g->colours;
    /* GfxRegsStruct: LCDC, SCY, SCX, WINY, WINX, LYC.  LCD on, objects off. */
    const OraclesPpuRegs r = { (uint8_t)((regs[0] | 0x80u) & ~0x02u), regs[1], regs[2], regs[3], regs[4] };
    for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) in.lines[ly] = r;
    oracles_ppu_render(&in, g->full_frame);
    memcpy(out, g->full_frame + (ORACLES_PPU_HEIGHT - ORACLES_GHOST_AREA_HEIGHT) * ORACLES_PPU_WIDTH,
           ORACLES_GHOST_AREA_WIDTH * ORACLES_GHOST_AREA_HEIGHT * sizeof *out);
}

static int run_finished(const OraclesGhost *g) { return g->initialized && (!g->settle || g->settled); }

/* What the host needs besides the pixels: the map, the registers, the tileset, the state. */
static void capture_settled(OraclesGhost *g, OraclesGhostResult *result)
{
    restore_warp_tiles(g);
    if (g->capture) {
        const uint8_t *oam = oracles_guest_oam(g->guest);
        if (oam) memcpy(result->oam, oam, sizeof result->oam);
        /* The OAM kept is the one the LCD shows, a frame behind the one just
         * drawn, and the game may order its sprites differently from one
         * frame to the next: the tags are those of the drawn frame whose
         * OAM it is, found by content, as the view does for the live room;
         * the tags of the frame just drawn would fall on other entries and
         * cut an object in part, on some captures and not others. */
        result->tag_count = oracles_sprites_tags_for_oam(g->sprites, result->oam, result->tags, ORACLES_SPRITE_TAGS);
        if (!result->tag_count) result->tag_count = oracles_sprites_tags(g->sprites, result->tags, ORACLES_SPRITE_TAGS);
        result->tag_count_before = oracles_sprites_frame_before(g->sprites, result->oam, result->oam_before, result->tags_before, ORACLES_SPRITE_TAGS);
        const uint8_t *palettes = oracles_guest_obj_palettes(g->guest);
        if (palettes) memcpy(result->obj_palettes, palettes, sizeof result->obj_palettes);
        unfreeze_objects(g);   /* the state kept is a normal one: a chained run starts from it */
    }
    const OraclesGuestTables *t = oracles_guest_tables(g->guest);
    const uint8_t *vram0 = oracles_guest_vram(g->guest, 0), *vram1 = oracles_guest_vram(g->guest, 1);
    if (vram0 && vram1) {
        memcpy(result->bg_map, vram0 + 0x1800u, 0x800u);
        memcpy(result->bg_map + 0x800u, vram1 + 0x1800u, 0x800u);
        memcpy(result->tiles, vram0, ORACLES_GHOST_TILE_BYTES);
        memcpy(result->tiles + ORACLES_GHOST_TILE_BYTES, vram1, ORACLES_GHOST_TILE_BYTES);
    }
    const uint8_t *bg_palettes = oracles_guest_bg_palettes(g->guest);
    if (bg_palettes) memcpy(result->bg_palettes, bg_palettes, sizeof result->bg_palettes);
    else memset(result->bg_palettes, 0, sizeof result->bg_palettes);
    const uint8_t *base = oracles_guest_ptr(g->guest, t->tileset_bg_palettes, sizeof result->base_bg_palettes);
    if (base) memcpy(result->base_bg_palettes, base, sizeof result->base_bg_palettes);
    else memcpy(result->base_bg_palettes, result->bg_palettes, sizeof result->base_bg_palettes);
    result->palette_offset = (int8_t)oracles_guest_read8(g->guest, t->palette_thread_parameter);
    result->camera_x = (int16_t)oracles_guest_read16(g->guest, t->camera_x);
    result->camera_y = (int16_t)oracles_guest_read16(g->guest, t->camera_y);
    const uint8_t *regs = oracles_guest_ptr(g->guest, t->gfx_regs3, 6);
    if (regs) memcpy(result->regs3, regs, 6);
    const uint8_t *collisions = oracles_guest_ptr(g->guest, t->room_collisions, ORACLES_GHOST_LAYOUT_BYTES);
    if (collisions) memcpy(result->collisions, collisions, ORACLES_GHOST_LAYOUT_BYTES);
    else memset(result->collisions, 0xff, ORACLES_GHOST_LAYOUT_BYTES);
    oracles_objects_latch_remaining(g->objects);   /* an object still waiting in state 0 is kept as it stands */
    result->object_count = oracles_objects_latched(g->objects, result->objects, ORACLES_OBJECT_RECORDS);
    oracles_objects_note_drawn(g->objects, result->objects, result->object_count);
    result->killed_enemies = oracles_objects_killed_enemies(g->guest, oracles_guest_read8(g->guest, t->active_room));
    result->tileset_gfx = oracles_guest_read8(g->guest, t->tileset_gfx);
    result->tileset_palette = oracles_guest_read8(g->guest, t->tileset_palette);
    result->tileset_unique_gfx = oracles_guest_read8(g->guest, t->loaded_tileset_unique_gfx);
    result->room_state_modifier = oracles_guest_read8(g->guest, t->room_state_modifier);
    result->room_pack = oracles_guest_read8(g->guest, t->room_pack);
    oracles_animation_read(g->guest, &result->animation);
    if (!g->settled_state) g->settled_state = malloc(g->state_size);
    g->settled_size = g->settled_state && oracles_core_save_state(g->core, g->settled_state, g->state_size) == 0 ? g->state_size : 0;
}

/* The warp a dive into the sea or a return to its surface sets, where Link
 * stands (checkForUnderwaterTransition@initializeWarp, link.s): the room of
 * the same index in the other group, at his tile; the game plays it from its
 * next frame (applyWarpTransition2).  Written in the ghost only. */
static int set_level_change(OraclesGhost *g)
{
    const OraclesGuestTables *t = oracles_guest_tables(g->guest);
    const OraclesGuestSym dest_group = t->warp_dest_group, dest_room = t->warp_dest_room, dest_pos = t->warp_dest_pos,
                          transition = t->warp_transition, transition2 = t->warp_transition2, room = t->active_room, tile = t->active_tile_pos;
    if (!bank0_symbol(dest_group) || !bank0_symbol(dest_room) || !bank0_symbol(dest_pos) || !bank0_symbol(transition)
        || !bank0_symbol(transition2) || !bank0_symbol(room) || !bank0_symbol(tile)) return -1;
    uint8_t *wram = oracles_guest_wram_writable(g->guest, 0);
    if (!wram) return -1;
    wram[dest_room.addr - WRAM_BANK0_BASE] = wram[room.addr - WRAM_BANK0_BASE];
    wram[dest_group.addr - WRAM_BANK0_BASE] = (uint8_t)((unsigned)g->run_level | WARP_DEST_SET);
    wram[dest_pos.addr - WRAM_BANK0_BASE] = wram[tile.addr - WRAM_BANK0_BASE];
    wram[transition.addr - WRAM_BANK0_BASE] = 0;
    wram[transition2.addr - WRAM_BANK0_BASE] = LEVEL_CHANGE_WARP_TRANSITION2;
    return 0;
}

int oracles_ghost_begin_ex(OraclesGhost *g, const uint8_t *state, size_t size, OraclesGhostDirection direction,
                           int settle, const uint32_t *colours, OraclesGhostResult *result)
{
    memset(result, 0, sizeof *result);
    g->active = 0;
    g->initialized = 0;
    g->settle = settle;
    g->settled = 0;
    g->settle_pending = 0;
    g->settled_size = 0;
    /* The caller's table may change while the run lasts (the player's F2): the run keeps its own copy. */
    g->colours = NULL;
    if (colours) {
        if (!g->colours_copy) g->colours_copy = malloc(ORACLES_PPU_COLOURS * sizeof *g->colours_copy);
        if (g->colours_copy) { memcpy(g->colours_copy, colours, ORACLES_PPU_COLOURS * sizeof *g->colours_copy); g->colours = g->colours_copy; }
    }
    g->frames_run = 0;
    g->reads_this_run = 0;
    g->result = result;
    memset(g->seen, 0, sizeof g->seen);
    oracles_guest_enable_read_trace(g->guest, 0);
    if (oracles_core_load_state(g->core, state, size) != 0) { result->status = ORACLES_GHOST_LOAD_FAILED; return -1; }
    g->warp_tiles_guarded = 0;   /* the loaded state replaces any abandoned run */
    oracles_guest_reset_execution_state(g->guest);   /* the loaded state is not the one the hooks were following */
    /* Nor is a run that ended inside the substitutions (a timeout): its
     * pending return is gone, and the next room's own graphics load must not
     * turn the trace back on with its stack filter. */
    g->in_substitutions = 0;
    g->in_object_gfx = 0;
    g->trace_stack_start = g->trace_stack_top = 0;
    const char *reason = NULL;
    g->last_reason = "";
    g->direction = direction;
    g->primed = 0;
    g->primed_at = 0;
    g->await_group = -1;
    if (g->run_level >= 0) {
        /* A run to the other side of the sea: the warp is set on a state in
         * normal play, and the run primes once it has reached that group. */
        if (!g->prerun_max || !oracles_ghost_primeable(g->guest, &reason) || set_level_change(g) != 0) {
            g->last_reason = reason ? reason : "the warp cannot be set";
            result->status = ORACLES_GHOST_NOT_PRIMEABLE;
            return -1;
        }
        g->await_group = g->run_level;
    } else if (oracles_ghost_primeable(g->guest, &reason)) {
        if (prime(g, direction) != 0) { result->status = ORACLES_GHOST_NOT_PRIMEABLE; return -1; }
        g->primed = 1;
        if (g->tracing && g->load_counts) oracles_guest_enable_read_trace(g->guest, 1);
        result->key_len = oracles_ghost_key_snapshot(g->guest, result->key, sizeof result->key);
        result->from_group = oracles_guest_read8(g->guest, t_of(g)->active_group);
        result->from_room = oracles_guest_read8(g->guest, t_of(g)->active_room);
    } else if (!g->prerun_max || !oracles_ghost_prerunnable(g->guest, NULL)) {
        g->last_reason = reason ? reason : "?";
        result->status = ORACLES_GHOST_NOT_PRIMEABLE;
        return -1;
    }
    g->active = 1;
    return 0;
}

/* A pre-run has reached normal play: prime, and start the run's trace afresh
 * (the room the pre-run loaded had substitutions of its own). */
static int prime_after_prerun(OraclesGhost *g, OraclesGhostResult *result)
{
    if (prime(g, g->direction) != 0) return -1;
    g->primed = 1;
    if (g->tracing && g->load_counts) oracles_guest_enable_read_trace(g->guest, 1);
    g->primed_at = g->frames_run;
    result->prerun_frames = g->frames_run;
    result->key_len = oracles_ghost_key_snapshot(g->guest, result->key, sizeof result->key);
    result->from_group = oracles_guest_read8(g->guest, t_of(g)->active_group);
    result->from_room = oracles_guest_read8(g->guest, t_of(g)->active_room);
    memset(g->seen, 0, sizeof g->seen);
    result->read_count = 0;
    result->reads_dropped = 0;
    g->reads_this_run = 0;
    return 0;
}

int oracles_ghost_begin(OraclesGhost *g, const uint8_t *state, size_t size, OraclesGhostDirection direction, OraclesGhostResult *result)
{
    return oracles_ghost_begin_ex(g, state, size, direction, 0, NULL, result);
}

int oracles_ghost_step(OraclesGhost *g, unsigned frame_budget, unsigned max_frames, OraclesGhostResult *result)
{
    if (!g->active) return -1;
    g->result = result;
    const OraclesGuestTables *t = oracles_guest_tables(g->guest);
    for (unsigned i = 0; i < frame_budget && !run_finished(g); i++) {
        if (!g->primed) {
            const char *reason = NULL;
            if (oracles_ghost_primeable(g->guest, &reason) && (g->await_group < 0 || (oracles_guest_read8(g->guest, t->active_group) & 7u) == (unsigned)g->await_group)) {
                if (prime_after_prerun(g, result) != 0) { g->active = 0; result->status = ORACLES_GHOST_NOT_PRIMEABLE; return -1; }
            } else if (g->frames_run >= g->prerun_max) {
                g->last_reason = reason ? reason : "?";
                g->active = 0;
                result->status = ORACLES_GHOST_TIMEOUT;
                result->guest_frames = g->frames_run;
                return -1;
            }
        }
        if (g->primed && g->frames_run - g->primed_at >= max_frames) {
            restore_warp_tiles(g);
            g->active = 0;
            result->status = ORACLES_GHOST_TIMEOUT;
            result->guest_frames = g->frames_run;
            return -1;
        }
        oracles_guest_set_frame(g->guest, g->frames_run);
        oracles_core_run_frame(g->core);
        g->frames_run++;
        /* Each object of the room entered, as it stands once its own
         * initialisation has run: what the neighbour shows. */
        oracles_objects_latch(g->objects);
        if (g->initialized && g->settle && !g->settled) {
            /* A palette fade still running (wPaletteThread_mode: the fade
             * between two areas' palettes that a transition applies, in
             * Seasons on every outdoor change of area) would be captured
             * mid-way, the room white: the capture waits for the thread. */
            const int palette_busy = oracles_guest_read8(g->guest, t->palette_thread_mode) != 0
                /* A season change's reload still to come (Seasons, wcc4c): the state kept would not be primeable. */
                || (oracles_compat_seasons_rules(oracles_guest_profile(g->guest))
                    && oracles_guest_read8(g->guest, t->season_reload) != 0);
            if (oracles_guest_read8(g->guest, t->text_is_active) != 0) {
                /* A text box opened on the way (the dungeon's name, shown by an
                 * interaction Link was made to cross): the map holds the box
                 * for as long as it stays, and the room after it is not the
                 * room in play.  Not a result; run again later. */
                restore_warp_tiles(g);
                g->active = 0;
                result->status = ORACLES_GHOST_INTERRUPTED;
                result->guest_frames = g->frames_run;
                return -1;
            }
            if (g->settle_pending && g->frames_run > g->settled_frame && !palette_busy) {
                /* One frame after the transition ended: the vblank handler of
                 * that frame has drained the DMA queue of the last rows and of
                 * the unique graphics; the VRAM is the room's. */
                g->settled = 1;
                render_terrain(g, result->game_area);
                capture_settled(g, result);
                result->settled = 1;
            } else if (!g->settle_pending
                       && (oracles_guest_read8(g->guest, t->scroll_mode) & 0x7fu) == NORMAL_PLAY_SCROLL_MODE
                       && oracles_guest_read8(g->guest, t->screen_transition_state) == TRANSITION_STATE_IDLE) {
                g->settle_pending = 1;
                g->settled_frame = g->frames_run;
            }
        }
    }
    if (!run_finished(g)) return 0;
    restore_warp_tiles(g);
    unfreeze_objects(g);
    g->active = 0;
    result->status = ORACLES_GHOST_OK;
    result->guest_frames = g->frames_run;
    result->traced_reads = g->reads_this_run;
    return 1;
}

int oracles_ghost_run_ex(OraclesGhost *g, const uint8_t *state, size_t size, OraclesGhostDirection direction,
                         int settle, const uint32_t *colours, unsigned max_frames, OraclesGhostResult *result)
{
    if (oracles_ghost_begin_ex(g, state, size, direction, settle, colours, result) != 0) return -1;
    return oracles_ghost_step(g, max_frames, max_frames, result);
}

int oracles_ghost_run(OraclesGhost *g, const uint8_t *state, size_t size, OraclesGhostDirection direction, unsigned max_frames, OraclesGhostResult *result)
{
    return oracles_ghost_run_ex(g, state, size, direction, 0, NULL, max_frames, result);
}

/* ---- worker thread ------------------------------------------------------------- */

static void *worker(void *arg)
{
    OraclesGhost *g = arg;
    pthread_mutex_lock(&g->mutex);
    for (;;) {
        while (!g->quit && !g->job_pending) pthread_cond_wait(&g->cond, &g->mutex);
        if (g->quit) break;
        /* The job's inputs are private to the worker while pending; the mutex is
         * released for the run so the host is never blocked by it. */
        const OraclesGhostDirection direction = g->job_direction;
        const unsigned max_frames = g->job_max_frames;
        const int settle = g->job_settle;
        g->run_hold = g->job_hold;
        g->run_level = g->job_level;
        const uint32_t *colours = g->job_has_colours ? g->job_colours_copy : NULL;
        pthread_mutex_unlock(&g->mutex);
        OraclesGhostResult result;
        int rc = oracles_ghost_run_ex(g, g->job_state, g->job_size, direction, settle, colours, max_frames, &result);
        /* The job's budget spent before the run finished (a pre-run and its
         * run together, or a room that never loads): a timeout, never a
         * result, whose fields would still be those of the start. */
        if (rc == 0) { result.status = ORACLES_GHOST_TIMEOUT; result.guest_frames = g->frames_run; rc = -1; }
        pthread_mutex_lock(&g->mutex);
        g->job_result = result;
        g->job_return = rc;
        g->job_pending = 0;
        g->job_done = 1;
        pthread_cond_broadcast(&g->cond);
    }
    pthread_mutex_unlock(&g->mutex);
    return NULL;
}

int oracles_ghost_request_ex(OraclesGhost *g, const uint8_t *state, size_t size, OraclesGhostDirection direction,
                             int settle, const uint32_t *colours, unsigned max_frames)
{
    if (size != g->state_size) return -1;
    pthread_mutex_lock(&g->mutex);
    if (g->job_pending) { pthread_mutex_unlock(&g->mutex); return -1; }
    if (!g->job_state) g->job_state = malloc(size);
    if (!g->job_state) { pthread_mutex_unlock(&g->mutex); return -1; }
    memcpy(g->job_state, state, size);
    g->job_size = size;
    g->job_direction = direction;
    g->job_settle = settle;
    g->job_has_colours = 0;
    if (colours) {
        if (!g->job_colours_copy) g->job_colours_copy = malloc(ORACLES_PPU_COLOURS * sizeof *g->job_colours_copy);
        if (!g->job_colours_copy) { pthread_mutex_unlock(&g->mutex); return -1; }
        memcpy(g->job_colours_copy, colours, ORACLES_PPU_COLOURS * sizeof *g->job_colours_copy);
        g->job_has_colours = 1;
    }
    g->job_max_frames = max_frames;
    g->job_hold = g->hold_request;
    g->job_level = g->level_request;
    g->job_pending = 1;
    g->job_done = 0;
    if (!g->thread_started) {
        if (pthread_create(&g->thread, NULL, worker, g) != 0) { g->job_pending = 0; pthread_mutex_unlock(&g->mutex); return -1; }
        g->thread_started = 1;
    }
    pthread_cond_broadcast(&g->cond);
    pthread_mutex_unlock(&g->mutex);
    return 0;
}

int oracles_ghost_request(OraclesGhost *g, const uint8_t *state, size_t size, OraclesGhostDirection direction, unsigned max_frames)
{
    return oracles_ghost_request_ex(g, state, size, direction, 0, NULL, max_frames);
}

int oracles_ghost_poll(OraclesGhost *g, OraclesGhostResult *result)
{
    pthread_mutex_lock(&g->mutex);
    int rc;
    if (g->job_pending) rc = 0;
    else if (g->job_done) { *result = g->job_result; g->job_done = 0; rc = 1; }
    else rc = -1;
    pthread_mutex_unlock(&g->mutex);
    return rc;
}

const char *oracles_ghost_last_reason(const OraclesGhost *g) { return g->last_reason ? g->last_reason : ""; }

size_t oracles_ghost_settled_state(const OraclesGhost *g, uint8_t *out, size_t capacity)
{
    if (!g->settled_size || !g->settled_state) return 0;
    if (out && capacity >= g->settled_size) memcpy(out, g->settled_state, g->settled_size);
    return g->settled_size;
}

void oracles_ghost_dropped_returns(const OraclesGhost *g, unsigned *overflow, unsigned *purged) { oracles_guest_dropped_returns(g->guest, overflow, purged); }
