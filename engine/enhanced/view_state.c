/* The Enhanced view's savestate (view.h): the camera and the loops it keeps, the surface it was taken at, and what a
 * load forgets. */
#include "view_internal.h"

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

/* The records after the loops': the surface's size when it is not the normal
 * one, then the camera's whole offsets (a state saved before them has none).
 * 1 when the state ends with them, with their places, 0 for a record absent. */
static int tail_records(const uint8_t *data, size_t size, size_t *size_at, size_t *offsets_at)
{
    size_t at = 2u * ORACLES_E11_STATE_WIRE_SIZE + LOOP_RECORD_SIZE;
    *size_at = 0; *offsets_at = 0;
    if (size < at) return 0;
    if (size - at >= SIZE_RECORD_SIZE && memcmp(data + at, SIZE_RECORD_TAG, 4) == 0) { *size_at = at; at += SIZE_RECORD_SIZE; }
    if (size - at >= OFFSET_RECORD_SIZE && memcmp(data + at, OFFSET_RECORD_TAG, 4) == 0) { *offsets_at = at; at += OFFSET_RECORD_SIZE; }
    return size == at;
}

static void put32(uint8_t *out, int32_t value) { for (unsigned b = 0; b < 4u; b++) out[b] = (uint8_t)((uint32_t)value >> (8u * b)); }
static int32_t get32(const uint8_t *in) { uint32_t value = 0; for (unsigned b = 0; b < 4u; b++) value |= (uint32_t)in[b] << (8u * b); return (int32_t)value; }

static const char *const surface_aspects[ORACLES_ENHANCED_ASPECTS] = { "16:9", "4:3" };

/* The view's level and the screen's shape that give a surface, `fitted` the far view fitted to a screen; 0 for none. */
static int surface_of(OraclesEnhancedSize size, unsigned *level, unsigned *aspect, int *fitted)
{
    if (!oracles_enhanced_size_known(size, fitted)) return 0;
    if (*fitted) { *level = ORACLES_ENHANCED_FAR; *aspect = oracles_enhanced_screen_aspect(size.width, size.height); return 1; }
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
    unsigned l, a; int fitted;
    if (!surface_of(size, &l, &a, &fitted)) snprintf(out, capacity, "a surface of %ux%u", size.width, size.height);
    else if (fitted) snprintf(out, capacity, "the far view fitted to the screen, %ux%u", size.width, size.height);
    else snprintf(out, capacity, "the %s view, %ux%u in %s", levels[l], size.width, size.height, surface_aspects[a]);
}

int oracles_enhanced_view_check_state(const OraclesEnhancedView *v, const uint8_t *data, size_t size, char *why, size_t capacity)
{
    OraclesEnhancedSize saved = oracles_enhanced_size(0);
    size_t at, offsets_at;
    if (tail_records(data, size, &at, &offsets_at) && at) {
        saved.width = data[at + 4] | (unsigned)data[at + 5] << 8;
        saved.height = data[at + 6] | (unsigned)data[at + 7] << 8;
    }
    if (saved.width == v->size.width && saved.height == v->size.height) return 0;
    if (why && capacity) {
        char taken[96], shown[96];
        surface_words(saved, taken, sizeof taken);
        surface_words(v->size, shown, sizeof shown);
        /* The far view at another of its sizes, the settings that show the one it was taken on: its fixed size in the other
         * shape (a far savestate of a 4:3 screen taken before far had its 4:3 size), aspect=; its fixed 4:3 size where it
         * is fitted, far=fixed; its fitted size, far=screen on that screen.  Two fitted sizes are two screens. */
        unsigned saved_level, saved_aspect, level, aspect; int saved_fitted, fitted;
        const int same = surface_of(saved, &saved_level, &saved_aspect, &saved_fitted) && surface_of(v->size, &level, &aspect, &fitted) && saved_level == level;
        char hint[128] = "";
        if (same && saved_fitted && !fitted) snprintf(hint, sizeof hint, "; far=screen and aspect=auto in the settings show that surface on the screen it was taken on");
        else if (same && !saved_fitted && (!fitted || saved_aspect == ORACLES_ENHANCED_16_9)) snprintf(hint, sizeof hint, "; aspect=%s in the settings shows that surface", surface_aspects[saved_aspect]);
        else if (same && !saved_fitted) snprintf(hint, sizeof hint, "; far=fixed in the settings shows that surface");
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
    /* The camera's whole offsets (camera.h): the camera loaded shows the pixel it showed. */
    if (capacity - total < OFFSET_RECORD_SIZE) return -1;
    OraclesEnhancedShownOffset across, down;
    oracles_enhanced_camera_shown_offsets(v->camera, &across, &down);
    memcpy(out + total, OFFSET_RECORD_TAG, 4);
    put32(out + total + 4, across.valid ? across.offset : OFFSET_NONE);
    put32(out + total + 8, down.valid ? down.offset : OFFSET_NONE);
    total += OFFSET_RECORD_SIZE;
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
     * the loops, and the surface's size when it is not the normal one, and
     * the camera's whole offsets. */
    if (oracles_enhanced_view_check_state(v, data, size, NULL, 0) != 0) return -1;
    size_t size_at, offsets_at;
    const int have_loops = tail_records(data, size, &size_at, &offsets_at) && memcmp(data + 2u * ORACLES_E11_STATE_WIRE_SIZE, LOOP_RECORD_TAG, 4) == 0;
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
    if (have_loops && offsets_at) {
        /* For the reducers' epochs and segments restored; an axis without one takes it afresh. */
        const int32_t saved[2] = { get32(data + offsets_at + 4), get32(data + offsets_at + 8) };
        OraclesEnhancedShownOffset across = { saved[0] != OFFSET_NONE, saved[0], state.epoch, state.segment };
        OraclesEnhancedShownOffset down = { 0, 0, 0, 0 };
        if (have_vertical && saved[1] != OFFSET_NONE) { down.valid = 1; down.offset = saved[1]; down.epoch = vertical.epoch; down.segment = vertical.segment; }
        oracles_enhanced_camera_set_shown_offsets(v->camera, &across, &down);
    }
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
     * object, the place and the pre-runs of a load, the sprite bookkeeping. */
    v->have_last_ref = 0; v->have_came_from = 0; v->have_last_scroll_dir = 0;
    v->source_valid = 0; v->large_capture_valid = 0; v->large_prev_valid = 0; v->large_was_in_scroll = 0;
    v->have_ref_collisions = 0; v->ref_maku = 0; v->load_area_held = 0;
    v->blind_mask = 0; v->blind_epoch = 0;
    v->have_live_tiles_prev = 0; v->have_wave = 0; v->shake_history = 0; v->blurb_shown = 0; v->blurb_mask = 0;
    memset(v->blurb_drawings, 0, sizeof v->blurb_drawings);   /* drawings of another moment */
    ev_scroll_reset(v);   /* a scroll's animation (view_scroll.c): the one under way stays frozen, as the state loaded has it */
    v->strip_valid = 0;
    /* The objects' bookkeeping: the image of the room left,
     * the fade and the hand-off of an entry, the live numbering and tags,
     * a large room's drawings. */
    v->left_valid = 0; v->vanish_active = 0;
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
    oracles_enhanced_tint_forget(v->tint_memories);   /* a scene's tint is taken afresh */
    v->transition_mix = 0;
    ev_invalidate_all(v);
    v->coarse_len = 0;   /* the coarse list is taken again from the state loaded */
    v->black_loaded = 1;   /* the band fills again (view_black.c) */
    ev_hidden_forget(v);   /* the rooms the state loaded keeps dark are shown so at once */
    return 0;
}
