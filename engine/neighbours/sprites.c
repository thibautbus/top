#include "sprites.h"

#include "guest_struct_offsets.h"

#include <stdlib.h>
#include <string.h>

#define EFFECT_QUEUE 16u
#define HISTORY 3u   /* frames drawn kept with their tags: the OAM on screen lags wOam by a frame or two */

typedef struct drawn_frame {
    uint8_t oam[160];
    uint8_t tail;
    OraclesSpriteTag tags[ORACLES_SPRITE_TAGS];
    unsigned count;
    int valid;
} drawn_frame;

struct OraclesSprites {
    OraclesGuest *guest;
    drawn_frame history[HISTORY];
    unsigned history_head;
    OraclesSpriteTag tags[ORACLES_SPRITE_TAGS];
    unsigned count;
    unsigned dropped;
    /* the object being drawn */
    int drawing;
    uint8_t draw_kind, draw_slot, draw_first;
    uint8_t nested_end;                 /* end of the last block written inside the object's own drawing */
    uint8_t effects_at_draw;            /* hTerrainEffectsBufferUsedSize at the entry of its drawing */
    /* the objects that queued a terrain effect, in the order the loop writes them */
    uint8_t effect_kind[EFFECT_QUEUE], effect_slot[EFFECT_QUEUE];
    unsigned effect_head, effect_count;
    /* a block of raw OAM data being written */
    int block;
    uint8_t block_first;
    /* the check of the tagging */
    unsigned frames, frames_uncovered, worst_gap, frames_oam_full;
    unsigned shadows, shadows_unowned, shadows_left, shadows_grounded;
};

OraclesSprites *oracles_sprites_create(OraclesGuest *guest)
{
    OraclesSprites *s = calloc(1, sizeof *s);
    if (s) s->guest = guest;
    return s;
}

void oracles_sprites_destroy(OraclesSprites *s) { free(s); }

void oracles_sprites_forget(OraclesSprites *s)
{
    if (!s) return;
    for (unsigned i = 0; i < HISTORY; i++) s->history[i].valid = 0;
    s->count = 0;
    s->drawing = 0;
    s->block = 0;
    s->effect_head = s->effect_count = 0;
}

int oracles_sprites_add_hooks(OraclesGuest *guest)
{
    const OraclesGuestTables *t = oracles_guest_tables(guest);
    if (oracles_guest_add_hook(guest, t->draw_object, ORACLES_EVENT_OBJECT_DRAW, ORACLES_EVENT_OBJECT_DRAW_DONE) != 0) return -1;
    return oracles_guest_add_hook(guest, t->draw_raw_oam_block, ORACLES_EVENT_OAM_BLOCK, ORACLES_EVENT_OAM_BLOCK_DONE);
}

static uint8_t terrain_effects_used(OraclesSprites *s)
{
    return oracles_guest_read8(s->guest, oracles_guest_tables(s->guest)->terrain_effects_used);
}

/* hOamTail counts bytes; four per sprite. */
static uint8_t oam_entries(OraclesSprites *s)
{
    const OraclesGuestTables *t = oracles_guest_tables(s->guest);
    return (uint8_t)(oracles_guest_read8(s->guest, t->oam_tail) / 4u);
}

/* At the entry of @drawObject the object is named by the two bytes of
 * wObjectsToDraw the loop stands on: A holds the low byte, its type offset,
 * and the byte after HL holds the high byte, its slot ($d0 to $df). */
static void object_being_drawn(OraclesSprites *s, const OraclesGuestEvent *event, uint8_t *kind, uint8_t *slot)
{
    /* HL stands on the object's two bytes in wObjectsToDraw: the type offset,
     * then the slot's page. */
    const OraclesGuestSym sym = { 0, (uint16_t)((event->h << 8) | event->l) };
    const uint8_t *entry = oracles_guest_ptr(s->guest, sym, 2);
    const uint8_t offset = entry ? entry[0] : 0, page = entry ? entry[1] : 0;
    *slot = (uint8_t)(page >= (ORACLES_OBJECTS_BASE >> 8) ? page - (ORACLES_OBJECTS_BASE >> 8) : 0);
    /* The byte is the offset of the object's y inside its slot (objectQueueDraw
     * stores the field, not the type): its two high bits are the type. */
    switch (offset & 0xc0u) {
    case 0x40: *kind = 1; break;   /* interaction */
    case 0x80: *kind = 2; break;   /* enemy */
    case 0xc0: *kind = 3; break;   /* part */
    default: *kind = 0; break;     /* Link, the companion, an item */
    }
}

static void add_tag(OraclesSprites *s, uint8_t first, uint8_t last, uint8_t kind, uint8_t slot, int terrain)
{
    if (last <= first) return;
    if (s->count >= ORACLES_SPRITE_TAGS) { s->dropped++; return; }
    OraclesSpriteTag *tag = &s->tags[s->count++];
    tag->first = first;
    tag->count = (uint8_t)(last - first);
    tag->kind = kind;
    tag->slot = slot;
    tag->terrain_effect = (uint8_t)(terrain != 0);
}

void oracles_sprites_event(OraclesSprites *s, const OraclesGuestEvent *event)
{
    if (!s) return;
    switch (event->type) {
    case ORACLES_EVENT_FRAME_DONE:   /* entry of drawAllSprites: the frame's sprites start here */
        s->count = 0;
        s->dropped = 0;
        s->drawing = 0;
        s->block = 0;
        s->effect_head = s->effect_count = 0;
        break;
    case ORACLES_EVENT_OBJECT_DRAW:
        object_being_drawn(s, event, &s->draw_kind, &s->draw_slot);
        s->draw_first = oam_entries(s);
        s->nested_end = s->draw_first;
        s->effects_at_draw = terrain_effects_used(s);
        s->drawing = 1;
        break;
    case ORACLES_EVENT_OBJECT_DRAW_DONE:
        /* The grass or the puddle of an object is written inside its own
         * drawing, before its sprites: what follows the last such block is
         * the object's. */
        if (s->drawing) {
            add_tag(s, s->nested_end, oam_entries(s), s->draw_kind, s->draw_slot, 0);
            /* A shadow is queued in wTerrainEffectsBuffer instead (an object
             * in the air, one frame in two), for the blocks written after the
             * loop of objects, in the same order.  _drawObjectTerrainEffects
             * is entered for every object that asks for terrain effects,
             * whatever it does: only the buffer's growth says a shadow was
             * queued, four bytes each. */
            for (uint8_t used = s->effects_at_draw; used < terrain_effects_used(s) && s->effect_count < EFFECT_QUEUE; used = (uint8_t)(used + 4u)) {
                const unsigned at = (s->effect_head + s->effect_count) % EFFECT_QUEUE;
                s->effect_kind[at] = s->draw_kind;
                s->effect_slot[at] = s->draw_slot;
                s->effect_count++;
            }
        }
        s->drawing = 0;
        break;
    case ORACLES_EVENT_OAM_BLOCK:
        s->block_first = oam_entries(s);
        s->block = 1;
        break;
    case ORACLES_EVENT_OAM_BLOCK_DONE: {
        if (!s->block) break;
        if (s->drawing) {   /* inside an object's drawing: its own grass or puddle, queued nowhere */
            add_tag(s, s->block_first, oam_entries(s), s->draw_kind, s->draw_slot, 1);
            s->nested_end = oam_entries(s);
            s->block = 0;
            break;
        }
        uint8_t kind = 0, slot = 0;
        s->shadows++;
        if (s->effect_count) {
            kind = s->effect_kind[s->effect_head];
            slot = s->effect_slot[s->effect_head];
            s->effect_head = (s->effect_head + 1u) % EFFECT_QUEUE;
            s->effect_count--;
            /* Only an object in the air queues a shadow (_drawObjectTerrainEffects@inAir). */
            const uint8_t *owner = oracles_guest_object(s->guest, slot, kind);
            if (owner && !(owner[ORACLES_OBJ_ZH] & 0x80u)) s->shadows_grounded++;
        } else s->shadows_unowned++;
        add_tag(s, s->block_first, oam_entries(s), kind, slot, 1);
        s->block = 0;
        break;
    }
    case ORACLES_EVENT_FRAME_DRAWN: {
        /* Two tags never name the same entry, they follow one another, and none
         * names an entry the frame did not keep: an entry drawn belongs to one
         * object.  What lies before the first tag was written outside the
         * drawing of the objects, by the status bar. */
        const uint8_t tail = oam_entries(s);
        unsigned expected = 0, gap = 0;
        s->shadows_left += s->effect_count;   /* queued by an object, written by no block */
        int broken = 0;
        for (unsigned i = 0; i < s->count; i++) {
            const OraclesSpriteTag *tag = &s->tags[i];
            if (i == 0) expected = tag->first;
            if (tag->first != expected) {
                broken = 1;
                const unsigned d = tag->first > expected ? tag->first - expected : expected - tag->first;
                if (d > gap) gap = d;
            }
            expected = (unsigned)(tag->first + tag->count);
            if (expected > tail) { broken = 1; if (expected - tail > gap) gap = expected - tail; }
        }
        if (s->count && expected != tail) {
            broken = 1;
            const unsigned d = tail > expected ? tail - expected : expected - tail;
            if (d > gap) gap = d;
        }
        s->frames++;
        if (tail >= 40u) s->frames_oam_full++;   /* the game's own limit: forty entries */
        {   /* Kept with the wOam it describes, to be found again by content. */
            const OraclesGuestTables *tables = oracles_guest_tables(s->guest);
            const uint8_t *oam = oracles_guest_ptr(s->guest, tables->oam, 160u);
            drawn_frame *d = &s->history[s->history_head];
            if (oam) memcpy(d->oam, oam, sizeof d->oam); else memset(d->oam, 0, sizeof d->oam);
            d->tail = tail;
            d->count = s->count;
            memcpy(d->tags, s->tags, s->count * sizeof s->tags[0]);
            d->valid = !broken;
            s->history_head = (s->history_head + 1u) % HISTORY;
        }
        if (broken) {
            s->frames_uncovered++;
            if (gap > s->worst_gap) s->worst_gap = gap;
        }
        break;
    }
    default:
        break;
    }
}

unsigned oracles_sprites_frame_before(const OraclesSprites *s, const uint8_t oam[160], uint8_t out_oam[160], OraclesSpriteTag out[], unsigned max)
{
    if (!s || !oam || !out_oam || !out) return 0;
    /* The frame matched as in oracles_sprites_tags_for_oam, then the one drawn
     * just before it. */
    for (unsigned back = 1; back < HISTORY; back++) {
        const drawn_frame *d = &s->history[(s->history_head + HISTORY - back) % HISTORY];
        if (!d->valid || memcmp(d->oam, oam, sizeof d->oam) != 0) continue;
        const drawn_frame *before = &s->history[(s->history_head + HISTORY - back - 1u) % HISTORY];
        if (!before->valid) return 0;
        memcpy(out_oam, before->oam, 160u);
        const unsigned n = before->count < max ? before->count : max;
        memcpy(out, before->tags, n * sizeof *out);
        return n;
    }
    return 0;
}

unsigned oracles_sprites_tags_for_oam(const OraclesSprites *s, const uint8_t oam[160], OraclesSpriteTag out[], unsigned max)
{
    if (!s || !oam || !out) return 0;
    /* The newest frame drawn whose wOam is the OAM given, entry for entry up to
     * its tail: the tags that describe what is on screen. */
    for (unsigned back = 1; back <= HISTORY; back++) {
        const drawn_frame *d = &s->history[(s->history_head + HISTORY - back) % HISTORY];
        if (!d->valid || memcmp(d->oam, oam, sizeof d->oam) != 0) continue;
        const unsigned n = d->count < max ? d->count : max;
        memcpy(out, d->tags, n * sizeof *out);
        return n;
    }
    return 0;
}

unsigned oracles_sprites_tags(const OraclesSprites *s, OraclesSpriteTag out[], unsigned max)
{
    if (!s || !out) return 0;
    const unsigned n = s->count < max ? s->count : max;
    memcpy(out, s->tags, n * sizeof *out);
    return n;
}

void oracles_sprites_stats(const OraclesSprites *s, unsigned *frames, unsigned *frames_uncovered, unsigned *worst_gap, unsigned *tags_dropped)
{
    if (!s) return;
    (void)0;
    if (frames) *frames = s->frames;
    if (frames_uncovered) *frames_uncovered = s->frames_uncovered;
    if (worst_gap) *worst_gap = s->worst_gap;
    if (tags_dropped) *tags_dropped = s->dropped;
}

unsigned oracles_sprites_frames_oam_full(const OraclesSprites *s) { return s ? s->frames_oam_full : 0; }

void oracles_sprites_shadow_stats(const OraclesSprites *s, unsigned *shadows, unsigned *unowned, unsigned *left, unsigned *grounded)
{
    if (shadows) *shadows = s ? s->shadows : 0;
    if (unowned) *unowned = s ? s->shadows_unowned : 0;
    if (left) *left = s ? s->shadows_left : 0;
    if (grounded) *grounded = s ? s->shadows_grounded : 0;
}
