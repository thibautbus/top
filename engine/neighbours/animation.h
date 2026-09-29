/* A room's tile animation, advanced from the game's data (docs/GAME_HOOKS.md,
 * section 5).
 *
 * The game animates a tileset by up to four streams (animationGroupTable,
 * indexed by wTilesetAnimation): each is a counter and a pointer into a list
 * of graphics indices and delays, and each index names a copy from the ROM
 * into the VRAM (animationGfxHeaders).  Every frame of normal play,
 * updateAnimations loads one index from its queue, then counts the streams
 * down and queues the index of each one that runs out.
 *
 * A neighbour's animation state is taken from the ghost with its terrain,
 * and advanced here, one call per frame the live game animates, writing the
 * copies into the neighbour's own tiles.  The state is read from the game;
 * only the stepping is done here, and the harness judges it against the game
 * itself, which animates the live room by the same data. */
#ifndef ORACLES_NEIGHBOURS_ANIMATION_H
#define ORACLES_NEIGHBOURS_ANIMATION_H

#include "guest.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ORACLES_ANIMATION_QUEUE 32u
#define ORACLES_ANIMATION_TILE_BYTES 0x1800u   /* the tile data of a VRAM bank, $8000-$97ff */

typedef struct OraclesAnimationState {
    uint8_t tileset_animation;                 /* wTilesetAnimation; $ff: none */
    uint8_t state;                             /* wAnimationState: bits 0-3 the streams in use, bit 7 forces them all */
    uint8_t counters[12];                      /* wAnimationCounter1 to 4: counter, then the pointer, little-endian */
    uint8_t queue[ORACLES_ANIMATION_QUEUE];    /* w2AnimationQueue */
    uint8_t head, tail;                        /* wAnimationQueueHead and Tail */
} OraclesAnimationState;

/* The live state of the instance given. */
void oracles_animation_read(OraclesGuest *guest, OraclesAnimationState *out);

/* One frame of updateAnimations, for a frame where the game animates (normal
 * play): the index queued first, if any, is copied into `tiles` (two banks of
 * ORACLES_ANIMATION_TILE_BYTES), then the streams are counted down.  Returns
 * how many copies were written (0 or 1), -1 when the data cannot be read
 * (the state is then left as it was). */
int oracles_animation_step(OraclesAnimationState *a, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size,
                           uint8_t *tiles, uint8_t touched[2u * 384u / 8u]);

/* The same, the streams in `skip` (bit n: stream n) left alone: a
 * neighbour's streams that follow the live room's take their steps and their
 * images from the following, not from their own count. */
int oracles_animation_step_except(OraclesAnimationState *a, unsigned skip, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size,
                                  uint8_t *tiles, uint8_t touched[2u * 384u / 8u]);

/* The images the animation can put in each tile it writes: for every
 * graphics index its streams reach (and those already queued), each tile of
 * the copy with the ROM offset of its 16 bytes.  `tiles` holds the tile as
 * bank * 384 + index.  Returns how many pairs were written. */
unsigned oracles_animation_images(const OraclesAnimationState *a, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size,
                                  uint16_t tiles[], uint32_t sources[], unsigned max);

#define ORACLES_ANIMATION_CYCLE_MAX 32u

/* The streams of a neighbour that run the same loop as streams of the live
 * room.  Two rooms may draw the same element with other tiles (the sea of
 * Seasons: the same waves in tile 140 of one room and 142 of the next), so
 * tiles cannot be matched one by one; streams can.  A stream of the
 * neighbour is the same as a stream of the live room when their loops have
 * the same length and, for some rotation, the same delays step by step and
 * copies that share an image. */
typedef struct OraclesAnimationFollow {
    unsigned count;
    struct {
        uint8_t stream, live_stream, rotation, length;
        uint16_t pointer[ORACLES_ANIMATION_CYCLE_MAX];        /* the neighbour's loop, from its step when matched */
        uint8_t index[ORACLES_ANIMATION_CYCLE_MAX];
        uint16_t live_pointer[ORACLES_ANIMATION_CYCLE_MAX];   /* the live loop, from its step when matched */
        uint8_t live_index[ORACLES_ANIMATION_CYCLE_MAX];
        uint16_t followed;                                    /* the step it was last put at (0: none yet) */
        uint8_t waiting, waiting_step;                        /* an image to write once the twin's copy is in the live VRAM */
    } streams[4];
} OraclesAnimationFollow;

/* The streams a following moves, as a mask for oracles_animation_step_except. */
unsigned oracles_animation_followed_streams(const OraclesAnimationFollow *f);

/* The streams of `neighbour` that match streams of `live`; returns how many. */
unsigned oracles_animation_match(const OraclesAnimationState *neighbour, const OraclesAnimationState *live, const OraclesGuestTables *t,
                                 const uint8_t *rom, size_t rom_size, OraclesAnimationFollow *out);

/* Each matched stream of `neighbour` put where its live twin stands (the
 * step now due and its counter).  When that step changes, the image the twin
 * loaded last is written into `tiles` on the frame the twin's own copy is in
 * the live VRAM (`live_vram`, two banks of tile data; NULL: at once): the
 * game loads one copy a frame and the VRAM takes it at the next vblank, a
 * frame or two after the step.  Returns how many copies were written. */
unsigned oracles_animation_follow(OraclesAnimationState *neighbour, const OraclesAnimationState *live, OraclesAnimationFollow *f,
                                  const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size, uint8_t *tiles, uint8_t touched[2u * 384u / 8u],
                                  const uint8_t *live_vram[2]);


/* ---- A scroll's animation, run by the view alone ----------------------------
 * The game freezes its animation for the whole of a scroll (updateAnimations
 * returns while bit 0 of wScrollMode is clear) and resumes it where it
 * stopped: the frames of the scroll are not counted (docs/GAME_HOOKS.md,
 * section 5).  The view runs each stream a whole number of its loops from that
 * frozen step, one at least, spread over the scroll, and lands on it at the
 * frame the scroll's counter runs out: exactly the step the game resumes
 * from, at a pace that is not the game's (a loop longer than the scroll runs
 * faster). */
#define ORACLES_ANIMATION_UNREACHED 0xffffu   /* a gap no stepping closes: another animation, or a step outside the loop */

typedef struct OraclesScrollAnimation {
    OraclesAnimationState frozen;   /* the game's, where it resumes */
    OraclesAnimationState now;      /* the view's: the frozen state, its streams run on */
    unsigned period[4];             /* each stream's loop in frames, 0 when it stands outside a loop */
    uint32_t target[4];             /* the frames it runs over the scroll: whole loops, 0 when it stays still (outside a loop) */
    uint32_t phase[4];              /* the frames it has run, 16.16 */
    unsigned ran[4];                /* the frames of the scroll it has moved on: its pace is target / ran */
} OraclesScrollAnimation;

/* Frames until stream `stream` stands again where it stands; 0 when it does
 * not come back within ORACLES_ANIMATION_CYCLE_MAX steps (not in its loop). */
unsigned oracles_animation_stream_period(const OraclesAnimationState *a, unsigned stream, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size);

/* The plan for a scroll expected to animate for `frames` frames: each stream
 * runs round(frames / period) loops, one at least.  Returns the streams that
 * run. */
unsigned oracles_scroll_animation_begin(OraclesScrollAnimation *s, const OraclesAnimationState *frozen, unsigned frames,
                                        const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size);

/* The plan again, once the scroll's length is better known: the same rule,
 * a stream finishing at least the loop it has started. */
unsigned oracles_scroll_animation_plan(OraclesScrollAnimation *s, unsigned frames);

/* One frame of the scroll, `remaining` frames still to come before the one
 * that must land on the frozen step (0: this frame lands on it): each running
 * stream takes its share of what is left of its loops, and each index it
 * loads is copied into `tiles` (may be NULL), but its last ones when the
 * frozen queue still holds them (the game loads those copies after the
 * scroll).  Returns the copies, -1 on data it cannot read. */
int oracles_scroll_animation_step(OraclesScrollAnimation *s, unsigned remaining, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size,
                                  uint8_t *tiles, uint8_t touched[2u * 384u / 8u]);

/* The tiles the copies still waiting in the queue go to, marked in `mask`
 * (bank * 384 + tile): during a scroll the game holds them back too, and
 * loads them once it animates again.  Returns how many tiles. */
unsigned oracles_animation_queued_tiles(const OraclesAnimationState *a, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size, uint8_t mask[2u * 384u / 8u]);

/* Frames between the streams of `view` and those of `live`, ahead or behind,
 * the most over the streams: 0 when every stream stands where the game's does. */
unsigned oracles_animation_gap(const OraclesAnimationState *view, const OraclesAnimationState *live, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size);

#ifdef __cplusplus
}
#endif

#endif
