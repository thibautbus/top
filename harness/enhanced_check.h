/* Enhanced check: along a replayed route, the Enhanced surface
 * of every frame is composed by the view and hashed; the camera is watched
 * for holes and jumps; the modes are counted.  Writes DIR/frames.tsv (one
 * line per frame: hash, mode, camera, room, Link, transition state) and
 * prints the report with the hash of the whole run, which two replays must
 * reproduce. */
#ifndef ORACLES_ENHANCED_CHECK_H
#define ORACLES_ENHANCED_CHECK_H

#include "view.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OraclesEnhancedCheck OraclesEnhancedCheck;

/* `ghost_budget` 0: the ghost runs in its worker thread, as in play (the
 * composition is then not reproducible); N: synchronous, N guest frames per
 * host frame. */
OraclesEnhancedCheck *oracles_enhanced_check_start(OraclesCore *core, OraclesGuest *guest,
                                                   const uint8_t *rom, size_t rom_size, const char *dir, unsigned ghost_budget);
/* At `frame`, the core's state and the view's host state are saved; 120
 * frames later both are loaded back. Outside a scroll, neighbours are
 * compared with the saved topology. If the save was during a scroll, the
 * natural topology immediately before the load is used once play has settled
 * on the destination identity captured at F. In both cases the post-load
 * wait is 300 frames (the route diverges after the load, so the run hash is
 * not the plain one). */
void oracles_enhanced_check_reload_at(OraclesEnhancedCheck *check, uint32_t frame);
/* Writes the surface composed at `frame` to `path` as a PPM, sixteen at most: to look at what a figure says. */
void oracles_enhanced_check_surface_at(OraclesEnhancedCheck *check, uint32_t frame, const char *path);
/* The view under check, for what feeds it from outside the check (the hotbar's model). */
struct OraclesEnhancedView *oracles_enhanced_check_view(OraclesEnhancedCheck *check);
/* The camera profile (1 or 2), before the first frame. */
void oracles_enhanced_check_set_camera_profile(OraclesEnhancedCheck *check, unsigned profile);
/* The drawn-back view's surface, 480x270, before the first frame. */
void oracles_enhanced_check_set_zoom_out(OraclesEnhancedCheck *check, int enabled);
/* The surface of a view's level in a screen's shape (oracles_enhanced_view_size), before the first frame. */
void oracles_enhanced_check_set_size(OraclesEnhancedCheck *check, OraclesEnhancedSize size);
/* Each frame held to the Game Boy's period (59.7275 Hz), as a session plays
 * it: what the ghost in its thread delivers in a frame of play. */
void oracles_enhanced_check_set_paced(OraclesEnhancedCheck *check, int paced);
/* Draw the objects of the neighbours. */
void oracles_enhanced_check_set_neighbour_objects(OraclesEnhancedCheck *check, int enabled);
void oracles_enhanced_check_event(OraclesEnhancedCheck *check, const OraclesGuestEvent *event);
void oracles_enhanced_check_frame_end(OraclesEnhancedCheck *check, uint32_t frame);
void oracles_enhanced_check_report(OraclesEnhancedCheck *check, FILE *out);
/* The figures as key=value lines (enhanced.*), for tools/check_routes.py. */
void oracles_enhanced_check_summary(OraclesEnhancedCheck *check, FILE *out);
void oracles_enhanced_check_stop(OraclesEnhancedCheck *check);

#ifdef __cplusplus
}
#endif

/* Whether the savestate of --enhanced-reload-at was loaded back on this frame. */
int oracles_enhanced_check_reloaded_at(const OraclesEnhancedCheck *c, uint32_t frame);

#endif
