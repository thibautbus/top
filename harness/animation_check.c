#include "animation_check.h"

#include "animation.h"

#include <stdlib.h>
#include <string.h>

#define NORMAL_PLAY_BIT 0x01u     /* updateAnimations runs when bit 0 of wScrollMode is set */
#define PENDING_COPIES 8u
#define COPY_WAIT_FRAMES 3u       /* a copy reaches the VRAM a vblank or two after the frame that loads it */
#define GFX_HEADER_BYTES 6u

typedef struct pending_copy {
    unsigned bank;               /* VRAM bank */
    size_t at, from, bytes;      /* in the tile data; in the ROM; length */
    unsigned frames_left;
} pending_copy;

struct OraclesAnimationCheck {
    OraclesGuest *guest;
    uint8_t *rom;
    size_t rom_size;
    int have_last;
    int last_animating;
    uint8_t last_group, last_room;
    OraclesAnimationState last;
    pending_copy pending[PENDING_COPIES];
    unsigned pending_count;
    uint8_t scratch[2u * ORACLES_ANIMATION_TILE_BYTES];
    /* figures */
    unsigned frames_checked, frames_unaligned, state_wrong, read_failures;
    unsigned copies_checked, copies_wrong, copies_overwritten, copies_interrupted;
    char wrong_list[160];
};

OraclesAnimationCheck *oracles_animation_check_start(OraclesGuest *guest, const uint8_t *rom, size_t rom_size)
{
    OraclesAnimationCheck *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->guest = guest;
    c->rom = malloc(rom_size);
    if (!c->rom) { free(c); return NULL; }
    memcpy(c->rom, rom, rom_size);
    c->rom_size = rom_size;
    return c;
}

void oracles_animation_check_stop(OraclesAnimationCheck *c)
{
    if (!c) return;
    free(c->rom);
    free(c);
}

static size_t rom_offset(unsigned bank, unsigned addr)
{
    return bank ? (size_t)bank * 0x4000u + (addr - 0x4000u) : (size_t)addr;
}

/* Two states equal as the game uses them: the flags, the streams, the queue's
 * ends and the indices still waiting in it. */
static int same_state(const OraclesAnimationState *a, const OraclesAnimationState *b)
{
    if (a->state != b->state || a->head != b->head || a->tail != b->tail || memcmp(a->counters, b->counters, sizeof a->counters) != 0) return 0;
    for (uint8_t i = a->head; i != a->tail; i = (uint8_t)((i + 1u) & (ORACLES_ANIMATION_QUEUE - 1u)))
        if (a->queue[(i + 1u) & (ORACLES_ANIMATION_QUEUE - 1u)] != b->queue[(i + 1u) & (ORACLES_ANIMATION_QUEUE - 1u)]) return 0;
    return 1;
}

static void note_wrong(OraclesAnimationCheck *c, const char *what, uint32_t frame, uint8_t room)
{
    const size_t used = strlen(c->wrong_list);
    if (used + 24 < sizeof c->wrong_list) snprintf(c->wrong_list + used, sizeof c->wrong_list - used, "%s%s@%u:%02x", used ? " " : "", what, frame, room);
}

/* The copies the game loaded since the last frame, decoded from the ROM as
 * the reader decodes them: each is looked for in the VRAM for a few frames. */
static void queue_copies(OraclesAnimationCheck *c, const OraclesGuestTables *t, const OraclesAnimationState *before, const OraclesAnimationState *now)
{
    for (uint8_t i = before->head; i != now->head; i = (uint8_t)((i + 1u) & (ORACLES_ANIMATION_QUEUE - 1u))) {
        const uint8_t index = now->queue[(i + 1u) & (ORACLES_ANIMATION_QUEUE - 1u)];
        const size_t header = rom_offset(t->animation_gfx_headers.bank, t->animation_gfx_headers.addr) + (size_t)index * GFX_HEADER_BYTES;
        if (header + GFX_HEADER_BYTES > c->rom_size || c->pending_count >= PENDING_COPIES) continue;
        const uint8_t *h = c->rom + header;
        const unsigned destination = (unsigned)(h[3] << 8 | h[4]);
        pending_copy p;
        p.bank = destination & 1u;
        p.at = (size_t)((destination & 0xfff0u) - 0x8000u);
        p.from = rom_offset(h[0], (unsigned)(h[1] << 8 | h[2]));
        p.bytes = ((size_t)(h[5] & 0x7fu) + 1u) * 16u;
        p.frames_left = COPY_WAIT_FRAMES;
        if (p.at + p.bytes > ORACLES_ANIMATION_TILE_BYTES || p.from + p.bytes > c->rom_size) continue;
        /* A copy the next one to the same place overwrites before it could be seen is not judged. */
        int replaced = 0;
        for (unsigned k = 0; k < c->pending_count && !replaced; k++)
            if (c->pending[k].bank == p.bank && c->pending[k].at == p.at) { c->pending[k] = p; c->copies_overwritten++; replaced = 1; }
        if (!replaced) c->pending[c->pending_count++] = p;
    }
}

static void check_copies(OraclesAnimationCheck *c, uint32_t frame, uint8_t room)
{
    unsigned kept = 0;
    for (unsigned k = 0; k < c->pending_count; k++) {
        pending_copy *p = &c->pending[k];
        const uint8_t *vram = oracles_guest_vram(c->guest, p->bank);
        if (vram && memcmp(vram + p->at, c->rom + p->from, p->bytes) == 0) { c->copies_checked++; continue; }
        if (--p->frames_left == 0) { c->copies_checked++; c->copies_wrong++; note_wrong(c, "copy", frame, room); continue; }
        c->pending[kept++] = *p;
    }
    c->pending_count = kept;
}

void oracles_animation_check_frame(OraclesAnimationCheck *c, uint32_t frame)
{
    if (!c || !c->guest) return;
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    const uint8_t *io = oracles_guest_io(c->guest);
    const uint8_t scroll_mode = oracles_guest_read8(c->guest, t->scroll_mode);
    const uint8_t group = oracles_guest_read8(c->guest, t->active_group), room = oracles_guest_read8(c->guest, t->active_room);
    OraclesAnimationState now;
    oracles_animation_read(c->guest, &now);
    /* A menu or a text box writes its own tiles into the VRAM: not play. */
    const int animating = (scroll_mode & NORMAL_PLAY_BIT) && io && (io[0x40] & 0x80u) && now.tileset_animation != 0xffu
        && oracles_guest_read8(c->guest, t->opened_menu_type) == 0 && oracles_guest_read8(c->guest, t->text_is_active) == 0;
    /* A copy waiting to be seen when the room changes or play stops (a
     * scroll, a load, a menu, a text box) is not judged: they write over the
     * VRAM. */
    if (c->pending_count && (!animating || (c->have_last && (group != c->last_group || room != c->last_room)))) {
        c->copies_interrupted += c->pending_count;
        c->pending_count = 0;
    }
    check_copies(c, frame, room);
    if (c->have_last && now.head != c->last.head) queue_copies(c, t, &c->last, &now);
    /* The state: one step of the reader from the game's state at the last
     * frame gives the game's state now, on every frame of normal play that
     * follows another in the same room and the same animation.  The frame
     * boundary the harness sees can fall inside the game's logic when a frame
     * of the game runs long: the game has then stepped zero or two times
     * since, counted apart. */
    if (c->have_last && c->last_animating && animating && group == c->last_group && room == c->last_room
        && now.tileset_animation == c->last.tileset_animation) {
        OraclesAnimationState once = c->last, twice;
        const int rc1 = oracles_animation_step(&once, t, c->rom, c->rom_size, c->scratch, NULL);
        twice = once;
        const int rc2 = rc1 < 0 ? -1 : oracles_animation_step(&twice, t, c->rom, c->rom_size, c->scratch, NULL);
        if (rc1 < 0 || rc2 < 0) c->read_failures++;
        else {
            c->frames_checked++;
            if (same_state(&once, &now)) { /* the frame the game played */ }
            else if (same_state(&c->last, &now) || same_state(&twice, &now)) c->frames_unaligned++;
            else { c->state_wrong++; note_wrong(c, "state", frame, room); }
        }
    }
    c->last = now;
    c->have_last = 1;
    c->last_animating = animating;
    c->last_group = group;
    c->last_room = room;
}

void oracles_animation_check_reset(OraclesAnimationCheck *c)
{
    if (!c) return;
    c->have_last = 0;
    c->pending_count = 0;
}

void oracles_animation_check_summary(const OraclesAnimationCheck *c, FILE *out)
{
    if (!c || !out) return;
    fprintf(out, "animation.frames_checked=%u\nanimation.frames_unaligned=%u\nanimation.state_wrong=%u\nanimation.copies_checked=%u\nanimation.copies_wrong=%u\nanimation.read_failures=%u\n",
            c->frames_checked, c->frames_unaligned, c->state_wrong, c->copies_checked, c->copies_wrong, c->read_failures);
}

void oracles_animation_check_report(const OraclesAnimationCheck *c, FILE *out)
{
    if (!c || !out) return;
    fprintf(out, "  tile animation from the game's data against the live game: %u frames of play, the reader's step equal to the game's on all but %u (%u where the game stepped zero or two times between two frame ends); %u copies decoded, %u not found in the VRAM (%u overwritten before they could be seen, %u interrupted by a load); unreadable data %u%s%s\n",
            c->frames_checked, c->state_wrong, c->frames_unaligned, c->copies_checked, c->copies_wrong, c->copies_overwritten, c->copies_interrupted, c->read_failures,
            c->wrong_list[0] ? "; first: " : "", c->wrong_list);
}
