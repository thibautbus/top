/* Native rendering of every frame from the committed display state, its
 * classification, and the comparison with the core's framebuffer.
 * Attached to a guest: it snapshots the state at the core's vblank
 * callback, replays the register journal into per-line registers, renders,
 * and compares. */
#ifndef ORACLES_FRAME_CHECK_H
#define ORACLES_FRAME_CHECK_H

#include "guest.h"
#include "ppu.h"

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OraclesFrameClass {
    ORACLES_FRAME_NORMAL = 0,        /* compared */
    ORACLES_FRAME_LCD_OFF,           /* compared: white */
    ORACLES_FRAME_LCD_ON_FIRST,      /* compared: the core paints the first frame after LCD on white */
    ORACLES_FRAME_LCD01,             /* LCD behaviours 0 and 1 with their per-line SCX or SCY writes in the journal: compared */
    ORACLES_FRAME_LCD2_6,            /* LCD behaviours 5 and 6 (ring menu, special fights): compared, reported apart */
    ORACLES_FRAME_MID_SCAN,          /* DMA or palette writes during the scan: out of scope, not compared */
    ORACLES_FRAME_NO_SCAN,           /* vblank types artificial and repeat: no new scan, not compared */
    ORACLES_FRAME_CLASSES
} OraclesFrameClass;

typedef struct OraclesFrameStats {
    uint32_t frames[ORACLES_FRAME_CLASSES];
    uint32_t mismatches[ORACLES_FRAME_CLASSES];
    uint32_t first_mismatch[ORACLES_FRAME_CLASSES];   /* host frame of the first mismatch, UINT32_MAX if none */
    uint32_t window_frames;                             /* frames where the window was scanned (a subset of the above) */
    uint64_t mid_scan_pixels;                           /* differing pixels over the mid-scan frames, which are not compared */
    uint32_t late_scroll_lines;                         /* lines of the compared frames whose SCX or SCY was written late, while
                                                           they were drawn (frame_check.c): not compared, the rest of the frame is */
    uint32_t first_tile_lines;                          /* lines whose SCX or SCY changed at the start of their drawing: their first
                                                           tile is the core's fetch timing, not compared (frame_check.c) */
    uint32_t compared_lines;                            /* lines of the compared frames, those above included */
} OraclesFrameStats;

typedef struct OraclesFrameCheck OraclesFrameCheck;

/* `samples_dir`, when not NULL, receives up to `samples_per_class` native /
 * core / difference PPM triplets per class, spread over the run. */
OraclesFrameCheck *oracles_frame_check_start(OraclesGuest *guest, OraclesCore *core,
                                             const char *samples_dir, unsigned samples_per_class);
void oracles_frame_check_stop(OraclesFrameCheck *check);

/* Host frame boundaries. */
void oracles_frame_check_frame_begin(void *opaque, uint32_t frame);
/* After a savestate load: the journal and the previous vblank describe a state that is gone. */
void oracles_frame_check_reset(OraclesFrameCheck *check);

/* The renderer's criterion as a verdict: 1 when no compared class has a mismatch,
 * every class of `expected_classes` (a bit per OraclesFrameClass) has at
 * least one frame, mid-scan frames are within 2% of the vblanks, the
 * frames not compared within 10% and the lines with a late scroll write
 * within 1% of the lines of the compared frames; 0 otherwise, with the first reason. */
int oracles_frame_check_verdict(const OraclesFrameCheck *check, unsigned expected_classes, char *reason, size_t capacity);

/* The native image of the last vblank (white when nothing was scanned). */
const uint32_t *oracles_frame_check_native(const OraclesFrameCheck *check);
const OraclesFrameStats *oracles_frame_check_stats(const OraclesFrameCheck *check);
const char *oracles_frame_class_name(OraclesFrameClass c);

/* Writes the report: counts and mismatches per class, the fallback share. */
void oracles_frame_check_report(const OraclesFrameCheck *check, FILE *out);

#ifdef __cplusplus
}
#endif

#endif
