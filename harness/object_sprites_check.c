#include "object_sprites_check.h"

#include "guest_struct_offsets.h"
#include "object_sprites.h"
#include "sprites.h"

#include <stdlib.h>
#include <string.h>

#define SCROLL_MODE_TRANSITION 0x08u   /* drawAllSprites then places objects by _getObjectPositionOnScreen_duringScreenTransition */
#define OAM_BYTES 0xa0u

struct OraclesObjectSpritesCheck {
    OraclesGuest *guest;
    OraclesSprites *tags;                /* the tagging, whose shadows' owners are checked here */
    uint8_t *rom;
    size_t rom_size;
    /* the object being drawn */
    int drawing, judged;
    uint8_t kind, slot, first;           /* first: hOamTail once its terrain effect is written */
    OraclesObjectSprite built[ORACLES_OBJECT_SPRITES_MAX];
    int built_count;
    /* the whole drawing */
    int frame_open;
    unsigned frame_first, frame_objects;   /* hOamTail / 4 at drawAllSprites' entry; objects drawn since */
    OraclesDrawnObject frame_list[64];
    OraclesObjectSprite frame_sprites[256];
    unsigned frames_checked, frames_wrong, frames_skipped;
    char frame_wrong_list[96];
    /* figures */
    unsigned objects_checked, objects_wrong, sprites_checked, objects_in_transition, read_failures;
    unsigned objects_beyond_window, sprites_beyond_window;
    char wrong_list[160];
};

OraclesObjectSpritesCheck *oracles_object_sprites_check_start(OraclesGuest *guest, const uint8_t *rom, size_t rom_size)
{
    OraclesObjectSpritesCheck *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->guest = guest;
    c->rom = malloc(rom_size);
    if (!c->rom) { free(c); return NULL; }
    memcpy(c->rom, rom, rom_size);
    c->rom_size = rom_size;
    c->tags = oracles_sprites_create(guest);
    if (!c->tags || oracles_sprites_add_hooks(guest) != 0) { oracles_object_sprites_check_stop(c); return NULL; }   /* the hooks of @drawObject and of the terrain effects' blocks */
    return c;
}

void oracles_object_sprites_check_stop(OraclesObjectSpritesCheck *c)
{
    if (!c) return;
    oracles_sprites_destroy(c->tags);
    free(c->rom);
    free(c);
}

static uint8_t oam_tail(OraclesObjectSpritesCheck *c)
{
    return oracles_guest_read8(c->guest, oracles_guest_tables(c->guest)->oam_tail);
}

static void note_wrong(OraclesObjectSpritesCheck *c, uint32_t frame)
{
    const size_t used = strlen(c->wrong_list);
    if (used + 24 < sizeof c->wrong_list)
        snprintf(c->wrong_list + used, sizeof c->wrong_list - used, "%s%u:%u.%u", used ? " " : "", frame, c->kind, c->slot);
}

/* The object the game is about to draw: its sprites are built from its
 * fields as they stand, the raised floor of Ages applied by the game. */
static void object_draw_begins(OraclesObjectSpritesCheck *c, const OraclesGuestTables *t, const OraclesGuestEvent *event)
{
    /* HL stands on the object's two bytes in wObjectsToDraw: the low
     * byte of its y (its two high bits the object's place in the slot),
     * then the slot's page. */
    const OraclesGuestSym sym = { 0, (uint16_t)((event->h << 8) | event->l) };
    const uint8_t *entry = oracles_guest_ptr(c->guest, sym, 2);
    c->drawing = 0;
    if (!entry || entry[1] < (ORACLES_OBJECTS_BASE >> 8)) return;
    c->kind = (uint8_t)(entry[0] >> 6);
    c->slot = (uint8_t)(entry[1] - (ORACLES_OBJECTS_BASE >> 8));
    c->drawing = 1;
    c->first = oam_tail(c);
    c->frame_objects++;
    c->judged = oracles_guest_read8(c->guest, t->scroll_mode) != SCROLL_MODE_TRANSITION;
    if (!c->judged) { c->objects_in_transition++; return; }
    c->built_count = oracles_object_sprites_build(c->guest, oracles_guest_object(c->guest, c->slot, c->kind), c->rom, c->rom_size,
                                                  c->built, ORACLES_OBJECT_SPRITES_MAX);
}

/* Its drawing is over: the sprites built, cut as the game cuts them, against
 * the entries it wrote for it. */
static void object_draw_ends(OraclesObjectSpritesCheck *c, const OraclesGuestTables *t, const OraclesGuestEvent *event)
{
    if (!c->drawing) return;
    c->drawing = 0;
    if (!c->judged) return;
    if (c->built_count < 0) { c->read_failures++; return; }
    const uint8_t end = oam_tail(c);
    const uint8_t *oam = oracles_guest_ptr(c->guest, t->oam, OAM_BYTES);
    if (!oam || end < c->first || end > OAM_BYTES) { c->read_failures++; return; }
    /* The game writes its grass or puddle whole if the OAM has room for
     * it, then its own sprites inside the window, in the block's order,
     * as long as the OAM has room; its shadow comes after the loop. */
    unsigned tail = c->first / 4u, expected = 0, beyond = 0, ground = 0;
    for (int i = 0; i < c->built_count; i++) if (c->built[i].effect == ORACLES_SPRITE_GROUND) ground++;
    const int ground_fits = tail + ground <= OAM_BYTES / 4u;
    int wrong = 0;
    for (int i = 0; i < c->built_count; i++) {
        const OraclesObjectSprite *s = &c->built[i];
        if (s->effect == ORACLES_SPRITE_SHADOW) continue;
        if (s->effect == ORACLES_SPRITE_GROUND ? !ground_fits : !s->in_window) { if (s->effect == ORACLES_SPRITE_OWN) beyond++; continue; }
        if (tail >= OAM_BYTES / 4u) continue;
        const unsigned at = tail * 4u;
        if (at + 4u > end || oam[at] != s->oam_y || oam[at + 1] != s->oam_x || oam[at + 2] != s->tile || oam[at + 3] != s->attr) wrong = 1;
        tail++;
        expected++;
    }
    if (tail * 4u != end) wrong = 1;
    c->objects_checked++;
    c->sprites_checked += expected;
    if (beyond) { c->objects_beyond_window++; c->sprites_beyond_window += beyond; }
    if (wrong) { c->objects_wrong++; note_wrong(c, event->frame); }
}

/* The whole drawing, rebuilt out of the game as the view rebuilds it,
 * against wOam. */
static void drawing_ends(OraclesObjectSpritesCheck *c, const OraclesGuestTables *t, const OraclesGuestEvent *event)
{
    /* The whole drawing, rebuilt out of it as the view rebuilds it: the
     * queue's order and its sixteen a level, Link alone with the text
     * box's alternative palette, Ages' raised floor, the grass and
     * puddles, the shadows after the loop, the OAM's room.  A frame the
     * game did not draw (wc4b6) or drew during a scrolling transition is
     * not judged. */
    if (!c->frame_open) return;
    c->frame_open = 0;
    if (!c->frame_objects || oracles_guest_read8(c->guest, t->scroll_mode) == SCROLL_MODE_TRANSITION) { c->frames_skipped++; return; }
    unsigned count = 0;
    if (oracles_object_sprites_all(c->guest, c->rom, c->rom_size, c->frame_first, c->frame_list, 64u, c->frame_sprites, 256u, &count) < 0) { c->read_failures++; return; }
    const uint8_t *oam = oracles_guest_ptr(c->guest, t->oam, OAM_BYTES);
    if (!oam) { c->read_failures++; return; }
    unsigned end = c->frame_first;
    int wrong = 0;
    for (unsigned i = 0; i < count; i++) {
        const OraclesObjectSprite *s = &c->frame_sprites[i];
        if (!s->written) continue;
        const uint8_t *e = oam + s->oam_index * 4u;
        if (s->oam_index != end || e[0] != s->oam_y || e[1] != s->oam_x || e[2] != s->tile || e[3] != s->attr) wrong = 1;
        end = s->oam_index + 1u;
    }
    if (end * 4u != oam_tail(c)) wrong = 1;
    c->frames_checked++;
    if (wrong) {
        c->frames_wrong++;
        const size_t used = strlen(c->frame_wrong_list);
        if (used + 12 < sizeof c->frame_wrong_list) snprintf(c->frame_wrong_list + used, sizeof c->frame_wrong_list - used, "%s%u", used ? " " : "", event->frame);
    }
}

void oracles_object_sprites_check_event(OraclesObjectSpritesCheck *c, const OraclesGuestEvent *event)
{
    if (!c) return;
    oracles_sprites_event(c->tags, event);
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    switch (event->type) {
    case ORACLES_EVENT_OBJECT_DRAW:
        object_draw_begins(c, t, event);
        break;
    case ORACLES_EVENT_OAM_BLOCK_DONE:
        break;
    case ORACLES_EVENT_OBJECT_DRAW_DONE:
        object_draw_ends(c, t, event);
        break;
    case ORACLES_EVENT_FRAME_DONE:   /* entry of drawAllSprites */
        c->frame_open = 1;
        c->frame_first = oam_tail(c) / 4u;
        c->frame_objects = 0;
        break;
    case ORACLES_EVENT_FRAME_DRAWN:
        drawing_ends(c, t, event);
        break;
    default:
        break;
    }
}

void oracles_object_sprites_check_reset(OraclesObjectSpritesCheck *c)
{
    if (c) { c->drawing = 0; c->frame_open = 0; }
}

void oracles_object_sprites_check_summary(const OraclesObjectSpritesCheck *c, FILE *out)
{
    if (!c || !out) return;
    fprintf(out, "object_sprites.objects_checked=%u\nobject_sprites.objects_wrong=%u\nobject_sprites.sprites_checked=%u\nobject_sprites.objects_beyond_window=%u\nobject_sprites.sprites_beyond_window=%u\nobject_sprites.read_failures=%u\n",
            c->objects_checked, c->objects_wrong, c->sprites_checked, c->objects_beyond_window, c->sprites_beyond_window, c->read_failures);
    fprintf(out, "object_sprites.frames_checked=%u\nobject_sprites.frames_wrong=%u\nobject_sprites.frames_skipped=%u\n", c->frames_checked, c->frames_wrong, c->frames_skipped);
    unsigned shadows = 0, unowned = 0, left = 0, grounded = 0;
    oracles_sprites_shadow_stats(c->tags, &shadows, &unowned, &left, &grounded);
    fprintf(out, "sprites.shadows=%u\nsprites.shadows_unowned=%u\nsprites.shadows_left=%u\nsprites.shadows_grounded=%u\n", shadows, unowned, left, grounded);
}

void oracles_object_sprites_check_report(const OraclesObjectSpritesCheck *c, FILE *out)
{
    if (!c || !out) return;
    fprintf(out, "  objects' sprites built from the game's data against the live OAM: %u drawings of objects, %u sprites; %u not equal to the entries the game wrote; %u with sprites beyond the window (%u sprites); %u during scrolling transitions, not judged; unreadable data %u%s%s\n",
            c->objects_checked, c->sprites_checked, c->objects_wrong, c->objects_beyond_window, c->sprites_beyond_window, c->objects_in_transition, c->read_failures,
            c->wrong_list[0] ? "; first (frame:kind.slot): " : "", c->wrong_list);
    fprintf(out, "  whole drawings rebuilt against the live OAM: %u frames, %u not equal (%u not drawn or in a scrolling transition)%s%s\n",
            c->frames_checked, c->frames_wrong, c->frames_skipped, c->frame_wrong_list[0] ? "; first frames: " : "", c->frame_wrong_list);
}
