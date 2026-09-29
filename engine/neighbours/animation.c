#include "animation.h"

#include <string.h>

#define GFX_HEADER_BYTES 6u      /* bank, source (big-endian), destination (big-endian, VRAM bank in bit 0), blocks of 16 bytes minus one */
#define STREAMS 4u
#define LOOP_MARKER 0xffu        /* m_AnimationLoop: $ff, then a signed offset back into the stream */
#define FORCE_UPDATE 0x80u
#define LOADING 0x40u            /* wAnimationState bit 6: an index was loaded this frame */

static size_t rom_offset(unsigned bank, unsigned addr)
{
    return bank ? (size_t)bank * 0x4000u + (addr - 0x4000u) : (size_t)addr;
}

void oracles_animation_read(OraclesGuest *guest, OraclesAnimationState *out)
{
    const OraclesGuestTables *t = oracles_guest_tables(guest);
    memset(out, 0, sizeof *out);
    out->tileset_animation = oracles_guest_read8(guest, t->tileset_animation);
    out->state = oracles_guest_read8(guest, t->animation_state);
    const uint8_t *counters = oracles_guest_ptr(guest, t->animation_counters, sizeof out->counters);
    if (counters) memcpy(out->counters, counters, sizeof out->counters);
    const uint8_t *queue = oracles_guest_ptr(guest, t->animation_queue, sizeof out->queue);
    if (queue) memcpy(out->queue, queue, sizeof out->queue);
    out->head = oracles_guest_read8(guest, t->animation_queue_head);
    out->tail = oracles_guest_read8(guest, t->animation_queue_tail);
}

/* loadAnimationGfxIndex: the copy the index names, as the vblank's DMA does it. */
static int copy_index(const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size, uint8_t index, uint8_t *tiles, uint8_t *touched)
{
    const size_t header = rom_offset(t->animation_gfx_headers.bank, t->animation_gfx_headers.addr) + (size_t)index * GFX_HEADER_BYTES;
    if (header + GFX_HEADER_BYTES > rom_size) return -1;
    const uint8_t *h = rom + header;
    const unsigned bank = h[0], source = (unsigned)(h[1] << 8 | h[2]), destination = (unsigned)(h[3] << 8 | h[4]);
    const size_t bytes = ((size_t)(h[5] & 0x7fu) + 1u) * 16u;
    const unsigned vram_bank = destination & 1u, at = (destination & 0xfff0u) - 0x8000u;
    const size_t from = rom_offset(bank, source);
    if (destination < 0x8000u || at + bytes > ORACLES_ANIMATION_TILE_BYTES || from + bytes > rom_size) return -1;
    memcpy(tiles + vram_bank * ORACLES_ANIMATION_TILE_BYTES + at, rom + from, bytes);
    if (touched)
        for (size_t tile = at / 16u; tile < (at + bytes) / 16u; tile++) {
            const size_t bit = vram_bank * 384u + tile;
            touched[bit >> 3] |= (uint8_t)(1u << (bit & 7u));
        }
    return 1;
}

/* updateAnimationDataPointer past its count: the index at the stream's
 * pointer, then the delay that follows it (a loop marker sends the pointer
 * back first), which becomes the counter. */
static int stream_next(unsigned data_bank, const uint8_t *rom, size_t rom_size, uint8_t counter[3], uint8_t *index)
{
    unsigned pointer = (unsigned)(counter[1] | counter[2] << 8);
    size_t at = rom_offset(data_bank, pointer);
    if (at + 3u > rom_size) return -1;
    *index = rom[at++];
    uint8_t delay = rom[at++];
    pointer += 2u;
    if (delay == LOOP_MARKER) {
        const int8_t back = (int8_t)rom[at];
        pointer = (unsigned)((int)pointer + 0xff00 + (uint8_t)back) & 0xffffu;   /* add hl,bc with b = $ff: hl + $ff00 + c */
        at = rom_offset(data_bank, pointer);
        if (at + 1u > rom_size) return -1;
        delay = rom[at];
        pointer += 1u;
    }
    counter[0] = delay;
    counter[1] = (uint8_t)(pointer & 0xffu);
    counter[2] = (uint8_t)(pointer >> 8);
    return 0;
}

int oracles_animation_step(OraclesAnimationState *a, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size,
                           uint8_t *tiles, uint8_t touched[2u * 384u / 8u])
{
    return oracles_animation_step_except(a, 0u, t, rom, rom_size, tiles, touched);
}

unsigned oracles_animation_followed_streams(const OraclesAnimationFollow *f)
{
    unsigned mask = 0;
    for (unsigned n = 0; f && n < f->count; n++) mask |= 1u << f->streams[n].stream;
    return mask;
}

int oracles_animation_step_except(OraclesAnimationState *a, unsigned skip, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size,
                                  uint8_t *tiles, uint8_t touched[2u * 384u / 8u])
{
    if (a->tileset_animation == 0xffu) return 0;
    OraclesAnimationState next = *a;
    int copies = 0;
    next.state &= (uint8_t)~LOADING;
    /* updateAnimationQueue: one index a frame. */
    if (next.head != next.tail) {
        next.head = (uint8_t)((next.head + 1u) & (ORACLES_ANIMATION_QUEUE - 1u));
        const int rc = copy_index(t, rom, rom_size, next.queue[next.head], tiles, touched);
        if (rc < 0) return -1;
        copies += rc;
        next.state |= LOADING;
    }
    /* updateAnimationData: each stream in use counts down; one that runs out
     * queues its graphics index and takes its next delay. */
    const unsigned data_bank = t->animation_group_table.bank;
    for (unsigned s = 0; s < STREAMS; s++) {
        if (!(next.state & (1u << s)) || (skip & (1u << s))) continue;
        uint8_t *counter = &next.counters[s * 3u];
        if (!(next.state & FORCE_UPDATE) && --counter[0] != 0) continue;
        uint8_t index;
        if (stream_next(data_bank, rom, rom_size, counter, &index) < 0) return -1;
        const uint8_t tail = (uint8_t)((next.tail + 1u) & (ORACLES_ANIMATION_QUEUE - 1u));
        if (tail == next.head) continue;   /* the queue is full: the game drops the index */
        next.tail = tail;
        next.queue[tail] = index;
    }
    next.state &= (uint8_t)~FORCE_UPDATE;
    *a = next;
    return copies;
}

static unsigned add_images(const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size, uint8_t index,
                           uint16_t tiles[], uint32_t sources[], unsigned count, unsigned max)
{
    const size_t header = rom_offset(t->animation_gfx_headers.bank, t->animation_gfx_headers.addr) + (size_t)index * GFX_HEADER_BYTES;
    if (header + GFX_HEADER_BYTES > rom_size) return count;
    const uint8_t *h = rom + header;
    const unsigned destination = (unsigned)(h[3] << 8 | h[4]);
    const size_t bytes = ((size_t)(h[5] & 0x7fu) + 1u) * 16u, from = rom_offset(h[0], (unsigned)(h[1] << 8 | h[2]));
    if (destination < 0x8000u || ((destination & 0xfff0u) - 0x8000u) + bytes > ORACLES_ANIMATION_TILE_BYTES || from + bytes > rom_size) return count;
    const unsigned first = ((destination & 0xfff0u) - 0x8000u) / 16u, bank = destination & 1u;
    for (unsigned k = 0; k < bytes / 16u && count < max; k++) {
        const uint16_t tile = (uint16_t)(bank * 384u + first + k);
        const uint32_t source = (uint32_t)(from + k * 16u);
        int seen = 0;
        for (unsigned j = 0; j < count && !seen; j++) seen = tiles[j] == tile && sources[j] == source;
        if (!seen) { tiles[count] = tile; sources[count] = source; count++; }
    }
    return count;
}

unsigned oracles_animation_images(const OraclesAnimationState *a, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size,
                                  uint16_t tiles[], uint32_t sources[], unsigned max)
{
    if (!a || a->tileset_animation == 0xffu) return 0;
    unsigned count = 0;
    for (uint8_t i = a->head; i != a->tail; i = (uint8_t)((i + 1u) & (ORACLES_ANIMATION_QUEUE - 1u)))
        count = add_images(t, rom, rom_size, a->queue[(i + 1u) & (ORACLES_ANIMATION_QUEUE - 1u)], tiles, sources, count, max);
    const unsigned data_bank = t->animation_group_table.bank;
    for (unsigned s = 0; s < STREAMS; s++) {
        if (!(a->state & (1u << s))) continue;
        const unsigned start = (unsigned)(a->counters[s * 3u + 1u] | a->counters[s * 3u + 2u] << 8);
        unsigned pointer = start;
        /* The list from the entry in course, round its loop back to it. */
        for (unsigned step = 0; step < 64u; step++) {
            size_t at = rom_offset(data_bank, pointer);
            if (at + 3u > rom_size) break;
            count = add_images(t, rom, rom_size, rom[at], tiles, sources, count, max);
            pointer += 2u;
            if (rom[at + 1u] == LOOP_MARKER) pointer = ((pointer + 0xff00u + rom[at + 2u]) & 0xffffu) + 1u;
            if (pointer == start) break;
        }
    }
    return count;
}

unsigned oracles_animation_queued_tiles(const OraclesAnimationState *a, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size, uint8_t mask[2u * 384u / 8u])
{
    uint16_t tiles[ORACLES_ANIMATION_QUEUE * 8u];
    uint32_t sources[ORACLES_ANIMATION_QUEUE * 8u];
    unsigned count = 0;
    for (uint8_t i = a->head; i != a->tail; i = (uint8_t)((i + 1u) & (ORACLES_ANIMATION_QUEUE - 1u)))
        count = add_images(t, rom, rom_size, a->queue[(i + 1u) & (ORACLES_ANIMATION_QUEUE - 1u)], tiles, sources, count, sizeof tiles / sizeof tiles[0]);
    for (unsigned k = 0; k < count; k++) mask[tiles[k] >> 3] |= (uint8_t)(1u << (tiles[k] & 7u));
    return count;
}

#define CYCLE_MAX ORACLES_ANIMATION_CYCLE_MAX

typedef struct cycle_step {
    unsigned pointer;      /* the index's address: where the stream's pointer stands before it loads it */
    uint8_t index, delay;  /* the graphics it loads, the delay it waits then */
} cycle_step;

/* A stream's loop, from the step it stands at, round to it. */
static unsigned stream_cycle(const OraclesAnimationState *a, unsigned stream, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size,
                             cycle_step out[CYCLE_MAX])
{
    const unsigned data_bank = t->animation_group_table.bank;
    uint8_t counter[3] = { 0, a->counters[stream * 3u + 1u], a->counters[stream * 3u + 2u] };
    const unsigned start = (unsigned)(counter[1] | counter[2] << 8);
    for (unsigned n = 0; n < CYCLE_MAX; n++) {
        out[n].pointer = (unsigned)(counter[1] | counter[2] << 8);
        if (stream_next(data_bank, rom, rom_size, counter, &out[n].index) < 0) return 0;
        out[n].delay = counter[0];
        if ((unsigned)(counter[1] | counter[2] << 8) == start) return n + 1u;
    }
    return 0;
}

/* Whether two copies share a tile image, wherever they put it. */
static int copies_share_image(const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size, uint8_t a, uint8_t b)
{
    const size_t headers = rom_offset(t->animation_gfx_headers.bank, t->animation_gfx_headers.addr);
    const size_t ha = headers + (size_t)a * GFX_HEADER_BYTES, hb = headers + (size_t)b * GFX_HEADER_BYTES;
    if (ha + GFX_HEADER_BYTES > rom_size || hb + GFX_HEADER_BYTES > rom_size) return 0;
    const uint8_t *x = rom + ha, *y = rom + hb;
    const size_t fa = rom_offset(x[0], (unsigned)(x[1] << 8 | x[2])), fb = rom_offset(y[0], (unsigned)(y[1] << 8 | y[2]));
    const size_t na = ((size_t)(x[5] & 0x7fu) + 1u) * 16u, nb = ((size_t)(y[5] & 0x7fu) + 1u) * 16u;
    if (fa + na > rom_size || fb + nb > rom_size) return 0;
    for (size_t i = 0; i < na; i += 16u)
        for (size_t j = 0; j < nb; j += 16u)
            if (memcmp(rom + fa + i, rom + fb + j, 16u) == 0) return 1;
    return 0;
}

unsigned oracles_animation_match(const OraclesAnimationState *neighbour, const OraclesAnimationState *live, const OraclesGuestTables *t,
                                 const uint8_t *rom, size_t rom_size, OraclesAnimationFollow *out)
{
    out->count = 0;
    if (!neighbour || !live || neighbour->tileset_animation == 0xffu || live->tileset_animation == 0xffu) return 0;
    uint8_t live_taken = 0;   /* a live stream stands for one neighbour stream */
    for (unsigned s = 0; s < STREAMS; s++) {
        if (!(neighbour->state & (1u << s))) continue;
        cycle_step mine[CYCLE_MAX];
        const unsigned length = stream_cycle(neighbour, s, t, rom, rom_size, mine);
        if (!length) continue;
        int done = 0;
        for (unsigned l = 0; l < STREAMS && !done; l++) {
            if (!(live->state & (1u << l)) || (live_taken & (1u << l))) continue;
            cycle_step theirs[CYCLE_MAX];
            if (stream_cycle(live, l, t, rom, rom_size, theirs) != length) continue;
            for (unsigned r = 0; r < length && !done; r++) {
                int same = 1;
                for (unsigned k = 0; k < length && same; k++) {
                    const cycle_step *p = &mine[k], *q = &theirs[(k + r) % length];
                    same = p->delay == q->delay && copies_share_image(t, rom, rom_size, p->index, q->index);
                }
                if (!same) continue;
                const unsigned n = out->count++;
                out->streams[n].stream = (uint8_t)s;
                out->streams[n].live_stream = (uint8_t)l;
                out->streams[n].rotation = (uint8_t)r;
                out->streams[n].length = (uint8_t)length;
                for (unsigned k = 0; k < length; k++) {
                    out->streams[n].pointer[k] = (uint16_t)mine[k].pointer;
                    out->streams[n].index[k] = mine[k].index;
                    out->streams[n].live_pointer[k] = (uint16_t)theirs[k].pointer;
                    out->streams[n].live_index[k] = theirs[k].index;
                }
                out->streams[n].followed = 0;
                out->streams[n].waiting = 0;
                live_taken |= (uint8_t)(1u << l);
                done = 1;
            }
        }
    }
    return out->count;
}

/* Whether the copy an index names is what the VRAM holds where it goes. */
static int copy_in_vram(const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size, uint8_t index, const uint8_t *vram[2])
{
    const size_t header = rom_offset(t->animation_gfx_headers.bank, t->animation_gfx_headers.addr) + (size_t)index * GFX_HEADER_BYTES;
    if (header + GFX_HEADER_BYTES > rom_size) return 1;
    const uint8_t *h = rom + header;
    const unsigned destination = (unsigned)(h[3] << 8 | h[4]);
    const size_t bytes = ((size_t)(h[5] & 0x7fu) + 1u) * 16u, at = (size_t)((destination & 0xfff0u) - 0x8000u);
    const size_t from = rom_offset(h[0], (unsigned)(h[1] << 8 | h[2]));
    if (destination < 0x8000u || at + bytes > ORACLES_ANIMATION_TILE_BYTES || from + bytes > rom_size || !vram[destination & 1u]) return 1;
    return memcmp(vram[destination & 1u] + at, rom + from, bytes) == 0;
}

unsigned oracles_animation_follow(OraclesAnimationState *neighbour, const OraclesAnimationState *live, OraclesAnimationFollow *f,
                                  const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size, uint8_t *tiles, uint8_t touched[2u * 384u / 8u],
                                  const uint8_t *live_vram[2])
{
    unsigned copies = 0;
    for (unsigned n = 0; n < f->count; n++) {
        const unsigned s = f->streams[n].stream, l = f->streams[n].live_stream, length = f->streams[n].length, r = f->streams[n].rotation;
        const unsigned at = (unsigned)(live->counters[l * 3u + 1u] | live->counters[l * 3u + 2u] << 8);
        unsigned m = length;
        for (unsigned k = 0; k < length; k++) if (f->streams[n].live_pointer[k] == at) { m = k; break; }
        if (m == length) continue;   /* the live stream is not where its loop was: left as it is */
        /* mine[k] is the twin of theirs[(k + r) % length]: theirs[m] is due now. */
        const unsigned now = (m + length - r) % length, last = (now + length - 1u) % length;
        uint8_t *counter = &neighbour->counters[s * 3u];
        counter[0] = live->counters[l * 3u];
        counter[1] = (uint8_t)(f->streams[n].pointer[now] & 0xffu);
        counter[2] = (uint8_t)(f->streams[n].pointer[now] >> 8);
        if (f->streams[n].followed != f->streams[n].pointer[now]) {
            f->streams[n].followed = f->streams[n].pointer[now];
            f->streams[n].waiting = 1;
            f->streams[n].waiting_step = (uint8_t)last;
        }
        if (!f->streams[n].waiting) continue;
        /* The twin of mine[k] is theirs[(k + r) % length]. */
        const uint8_t twin = f->streams[n].live_index[(f->streams[n].waiting_step + r) % length];
        if (live_vram && !copy_in_vram(t, rom, rom_size, twin, live_vram)) continue;
        f->streams[n].waiting = 0;
        if (tiles && copy_index(t, rom, rom_size, f->streams[n].index[f->streams[n].waiting_step], tiles, touched) > 0) copies++;
    }
    return copies;
}

/* ---- a scroll's animation, run by the view alone ---------------------------------------------- */

unsigned oracles_animation_stream_period(const OraclesAnimationState *a, unsigned stream, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size)
{
    if (!a || stream >= STREAMS || a->tileset_animation == 0xffu || !(a->state & (1u << stream))) return 0;
    cycle_step steps[CYCLE_MAX];
    const unsigned length = stream_cycle(a, stream, t, rom, rom_size, steps);
    unsigned frames = 0;
    for (unsigned n = 0; n < length; n++) frames += steps[n].delay ? steps[n].delay : 256u;   /* a delay of 0 counts down from 256 */
    return frames;
}

/* Whether the queue of `a` still holds the copies `seq` (in the order they
 * were queued, the last one latest). */
static int queue_holds(const OraclesAnimationState *a, const uint8_t *seq, unsigned count)
{
    for (uint8_t i = a->tail; count && i != a->head; i = (uint8_t)((i - 1u) & (ORACLES_ANIMATION_QUEUE - 1u)))
        if (a->queue[i] == seq[count - 1u]) count--;
    return count == 0;
}

/* Whether the index a stream has just loaded at frame `at` of its run
 * (`counter` its state after the load), with every load it makes after it up
 * to frame `end`, is still in the queue of `held`: those are the copies the
 * stream queued before the freeze and the game has not loaded yet. */
static int held_to_the_end(const OraclesAnimationState *held, uint8_t index, const uint8_t counter[3], unsigned at, unsigned end,
                           const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size)
{
    uint8_t seq[ORACLES_ANIMATION_QUEUE], c[3] = { counter[0], counter[1], counter[2] };
    unsigned count = 0;
    seq[count++] = index;
    for (unsigned next = at + (c[0] ? c[0] : 256u); next <= end; next += c[0] ? c[0] : 256u) {
        if (count == ORACLES_ANIMATION_QUEUE || stream_next(t->animation_group_table.bank, rom, rom_size, c, &seq[count]) < 0) return 0;
        count++;
    }
    return queue_holds(held, seq, count);
}

/* One stream `frames` frames on, as updateAnimationData moves it, each index
 * it loads copied at once into `tiles`.  With `held` (a scroll's frozen
 * state), `ran` the frames the stream has run before and `end` those it runs
 * in all: the last indices it loads, up to `end`, are not copied when that
 * state's queue still holds them in that order.  They are the copies the
 * stream queued before the freeze, which the game loads once it animates
 * again: until then the game's tile shows the image before them, and so does
 * the view's.  Returns the copies, -1 on data it cannot read. */
static int stream_advance(OraclesAnimationState *a, unsigned s, unsigned frames, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size,
                          uint8_t *tiles, uint8_t touched[2u * 384u / 8u], const OraclesAnimationState *held, unsigned ran, unsigned end)
{
    int copies = 0;
    uint8_t *counter = &a->counters[s * 3u];
    for (unsigned f = 0; f < frames; f++) {
        if (--counter[0] != 0) continue;
        uint8_t index;
        if (stream_next(t->animation_group_table.bank, rom, rom_size, counter, &index) < 0) return -1;
        if (held && held_to_the_end(held, index, counter, ran + f + 1u, end, t, rom, rom_size)) continue;
        if (tiles) {
            const int rc = copy_index(t, rom, rom_size, index, tiles, touched);
            if (rc < 0) return -1;
            copies += rc;
        }
    }
    return copies;
}

unsigned oracles_scroll_animation_plan(OraclesScrollAnimation *s, unsigned frames)
{
    unsigned running = 0;
    for (unsigned n = 0; n < STREAMS; n++) {
        const unsigned period = s->period[n];
        if (!period || frames == 0) { s->target[n] = 0; continue; }
        const unsigned run = (s->phase[n] + 0xffffu) >> 16;           /* frames already run, a started one counted */
        const unsigned least = (run + period - 1u) / period;          /* loops it must finish, having started them */
        unsigned loops = (frames + period / 2u) / period;
        if (loops == 0) loops = 1;                                    /* one at least, faster than the game's pace */
        if (loops < least) loops = least;
        s->target[n] = loops * period;
        running++;
    }
    return running;
}

unsigned oracles_scroll_animation_begin(OraclesScrollAnimation *s, const OraclesAnimationState *frozen, unsigned frames,
                                        const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size)
{
    memset(s, 0, sizeof *s);
    s->frozen = *frozen;
    s->now = *frozen;
    for (unsigned n = 0; n < STREAMS; n++) s->period[n] = oracles_animation_stream_period(frozen, n, t, rom, rom_size);   /* 0: not inside its loop, it would not come back */
    return oracles_scroll_animation_plan(s, frames);
}

int oracles_scroll_animation_step(OraclesScrollAnimation *s, unsigned remaining, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size,
                                  uint8_t *tiles, uint8_t touched[2u * 384u / 8u])
{
    int copies = 0;
    for (unsigned n = 0; n < STREAMS; n++) {
        if (!s->target[n]) continue;
        const uint32_t end = s->target[n] << 16, before = s->phase[n];
        if (before < end) s->ran[n]++;
        /* What is left of its loops, shared among this frame and those to come. */
        const uint32_t after = remaining == 0 ? end : before + (end - before) / (remaining + 1u);
        const int rc = stream_advance(&s->now, n, (after >> 16) - (before >> 16), t, rom, rom_size, tiles, touched, &s->frozen, before >> 16, s->target[n]);
        if (rc < 0) return -1;
        copies += rc;
        s->phase[n] = after;
    }
    return copies;
}

/* Frames from stream `n` of `from` to that of `to`, stepping `from`; `limit` + 1 when it never gets there. */
static unsigned stream_distance(const OraclesAnimationState *from, const OraclesAnimationState *to, unsigned n, unsigned limit,
                                const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size)
{
    OraclesAnimationState a = *from;
    for (unsigned d = 0; d <= limit; d++) {
        if (memcmp(&a.counters[n * 3u], &to->counters[n * 3u], 3u) == 0) return d;
        if (stream_advance(&a, n, 1u, t, rom, rom_size, NULL, NULL, NULL, 0, 0) < 0) break;
    }
    return limit + 1u;
}

unsigned oracles_animation_gap(const OraclesAnimationState *view, const OraclesAnimationState *live, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size)
{
    if (view->tileset_animation != live->tileset_animation || (view->state & 0x0fu) != (live->state & 0x0fu)) return ORACLES_ANIMATION_UNREACHED;
    unsigned gap = 0;
    for (unsigned n = 0; n < STREAMS; n++) {
        if (!(view->state & (1u << n))) continue;
        const unsigned period = oracles_animation_stream_period(view, n, t, rom, rom_size);
        const unsigned limit = period ? period : ORACLES_ANIMATION_CYCLE_MAX * 256u;
        const unsigned ahead = stream_distance(live, view, n, limit, t, rom, rom_size), behind = stream_distance(view, live, n, limit, t, rom, rom_size);
        unsigned d = ahead < behind ? ahead : behind;
        if (d > limit) d = ORACLES_ANIMATION_UNREACHED;
        if (d > gap) gap = d;
    }
    return gap;
}
