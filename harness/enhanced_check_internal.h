/* The Enhanced check, shared between its files: its state, the
 * constants of the game it reads, and the measures each file contributes to
 * a frame.  Split by domain as the view is: the camera and the scrolls, the
 * pixels judged against the core, the report.  Nothing here is called from
 * outside the harness. */
#ifndef ORACLES_HARNESS_ENHANCED_CHECK_INTERNAL_H
#define ORACLES_HARNESS_ENHANCED_CHECK_INTERNAL_H

#include "enhanced_check.h"

#include "ghost.h"
#include "guest_struct_offsets.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_CAMERA_STEP 4   /* the largest step of a tracking camera: the first profile's max_speed is 4 px a tick, the second's 2 */
#define SCROLL_MODE_TRANSITION_LOAD 0x08u
#define CURTAIN_SCROLL_MODE 0x02u        /* the screen opening after a warp: wScrollMode */
#define CURTAIN_TRANSITION_STATE 0x01u   /* wScreenTransitionState */
#define CURTAIN_SUBSTATE 0x02u           /* wScreenTransitionState2, the substate that draws a column a frame */
#define RELOAD_SETTLE_TIMEOUT 300u       /* the post-load topology wait, in host frames */

struct OraclesEnhancedCheck {
    OraclesCore *core;
    OraclesGuest *guest;
    OraclesEnhancedView *view;
    FILE *tsv;
    uint64_t run_hash;
    double compose_us_total, compose_us_max;   /* the time of each composition, the view's whole work in a frame */
    unsigned frames, world_frames, framed_frames;
    int have_camera;
    int32_t last_camera;
    unsigned camera_jumps;            /* |delta| > MAX_CAMERA_STEP between two world frames */
    int32_t largest_jump;
    unsigned camera_jumps_y;          /* the same, on the vertical camera */
    int32_t largest_jump_y, last_camera_y;
    int last_blank;                          /* the last world frame's surface was of one colour */
    /* the feel of the camera, over consecutive world frames of one epoch, outside cutscenes */
    unsigned camera_steps[5];         /* frames by |delta|: 0, 1, 2, 3, 4 px and more */
    unsigned camera_still_link_moving;/* frames with the camera still while Link's world x changed */
    unsigned camera_step_changes;     /* frames where |delta| differs from the previous frame's */
    int have_camera_delta;
    int32_t last_camera_delta;
    uint64_t offset_sum;              /* |Link - (camera + framing)| summed over world frames */
    int32_t offset_max;
    int32_t link_edge_margin_min;     /* the least room the camera left Link to the band's edge, outside cutscenes and transitions */
    unsigned link_near_edge_frames;   /* frames of that where he was within 8 px of it */
    unsigned offset_frames;
    unsigned camera_profile;
    /* every scrolling transition, from state 3 to normal play: its length and the frames Link stood still */
    int in_transition;
    unsigned transition_frames, transition_frozen;
    int32_t transition_last_x, transition_last_y;
    char transition_frozen_at[64];
    unsigned transitions_seen, transition_frames_total, transition_frozen_total, transition_frames_max, transition_arrivals_solid;
    char transition_list[512];
    /* the window against the drawn registers */
    unsigned window_checked, window_misplaced;
    unsigned last_wave_frames;        /* the view's count at the previous frame: this frame rippled if it grew */
    unsigned window_lines_checked, window_lines_wrong;   /* the window's own lines found in the band where the camera puts them */
    char window_lines_list[256];
    int have_last_offset, last_offset_x, last_offset_y;
    char window_misplaced_list[256];
    /* coverage: world frames with pixels no source covered */
    unsigned uncovered_frames;
    uint64_t uncovered_pixels;
    unsigned uncovered_max;
    char uncovered_list[256];
    unsigned world_runs;              /* runs of consecutive world frames */
    unsigned fallback_runs;           /* runs of consecutive framed frames */
    OraclesEnhancedMode last_mode;
    int have_mode;
    int have_last_observation;
    int32_t last_link_x, last_window_left;
    unsigned link_jumps, window_jumps;   /* between consecutive frames, any mode: a seam that is not continuous */
    unsigned link_jumps_y;               /* Link's world y */
    int32_t largest_link_jump_y, last_link_y;
    int32_t largest_link_jump, largest_window_jump;
    uint64_t epochs;
    unsigned neighbours_frames[5];       /* world frames with 0 to 4 neighbours shown */
    unsigned transitions, transitions_with_terrain, transitions_black, transitions_vertical;   /* the scrolling transitions judged: was the room entered shown, and true; those on the vertical axis among them */
    unsigned transitions_other_room;  /* entered while another room was shown that way: a wrong neighbour, never allowed */
    char other_room_list[256];        /* "from>to=shown" */
    unsigned curtain_frames, curtain_frames_wrong, curtain_frames_unchecked, curtain_pixels_wrong;
    unsigned text_box_frames_in_place;     /* frames the box's own place in the window still shows the box */
    unsigned curtain_window_frames_wrong;  /* curtain frames whose window, in the band, is not the core's image */
    unsigned text_box_frames, text_box_frames_wrong, text_box_pixels_wrong;
    unsigned text_box_trail;               /* compositions left where the box is still on screen after the game's text flag fell */
    unsigned text_box_frames_at_origin;    /* frames the band showed the box where the game drew it, the view having to move it */
    char text_box_list[96];
    char curtain_list[96];
    unsigned terrains_equal, terrains_different, terrain_tiles_different;   /* the shown terrain against the layout committed after the entry */
    unsigned terrains_old;           /* entered while shown in its old state, the ghost running it again (its old season): not judged */
    int pending_old;
    char black_list[512];                /* the black ones: "from>to" */
    char different_list[512];            /* the different ones: "to:tiles" */
    int threaded;
    int paced;                    /* each frame held to the Game Boy's period, as played */
    double paced_start;
    unsigned width, band_height;  /* the surface's width and its world band's height (oracles_enhanced_band_height) */
    unsigned transitions_large;   /* horizontal transitions in large rooms: the room entered is shown during the scroll, not before */
    unsigned large_scroll_black;  /* world frames of a large room's scroll with pixels no source covers beyond the room's gutters: the room entered missing */
    /* the transition in progress: what was shown for the room entered */
    int pending_transition, pending_shown;
    uint8_t pending_room;
    uint8_t pending_layout[ORACLES_GHOST_LAYOUT_BYTES];
    /* the neighbours shown in each direction (the game's: 0 up, 1 right, 2
     * down, 3 left) at the last frame of normal play (the room load spans a
     * lag frame), the layouts behind them, and the room that frame was in */
    int last_layout_valid[4];
    uint8_t last_layout[4][ORACLES_GHOST_LAYOUT_BYTES];
    int last_shown[4];
    uint8_t last_room[4];
    uint8_t last_play_room;
    int last_playing, last_mode_world;
    /* the reload check */
    uint32_t reload_at;
    unsigned surfaces;                      /* --surface-at: composed surfaces to write out, as PPM */
    uint32_t surface_at[16];
    const char *surface_path[16];
    uint8_t *saved_core;
    size_t saved_core_size;
    uint8_t saved_view[1024];   /* two camera records (both axes), 306 bytes each; the launcher keeps 1024 too */
    size_t saved_view_size;
    int32_t saved_camera;
    int saved_shown[4];
    uint8_t saved_room[4];
    int reload_saved_in_scroll;
    uint8_t reload_destination_group;
    uint8_t reload_destination_room;
    int reload_expected_valid;
    int reload_expected_shown[4];
    uint8_t reload_expected_room[4];
    int reload_saved, reload_done, reload_settled;
    uint32_t reload_frame, reload_settled_after;
    int32_t reload_camera_delta;
    int32_t reload_link_start, reload_camera_start;   /* after the load: does the camera still follow Link? */
    int32_t reload_link_travel, reload_camera_travel;
};

/* The monotonic clock in microseconds (enhanced_check.c). */
double ec_monotonic_us(void);

/* The camera, the scrolls and the savestate reload (enhanced_check_camera.c). */
void ec_reload_step(OraclesEnhancedCheck *c, uint32_t frame, int32_t camera_x, const int shown[4], const uint8_t room[4]);
/* `blank`: the composed surface is of one colour (a fade at its end): a camera that moves to or from it shows no jump. */
void ec_measure_camera(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, int32_t camera_x, int32_t camera_y, int blank);
void ec_measure_coverage(OraclesEnhancedCheck *c, uint32_t frame, unsigned uncovered);
void ec_measure_scroll(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, unsigned uncovered);
void ec_measure_seams(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode);
void ec_replay_scroll_registers(OraclesEnhancedCheck *c, int *scx, int *scy);
void ec_check_window_place(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, int rippled, int scx, int scy, uint32_t frame);

/* The band's pixels against the core's image (enhanced_check_pixels.c). */
unsigned ec_text_box_pixels(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, uint8_t box[ORACLES_ENHANCED_AREA_HEIGHT][ORACLES_ENHANCED_CORE_WIDTH]);
void ec_check_text_box(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, const uint32_t *surface, int32_t camera_x, int32_t camera_y,
                       uint8_t box[ORACLES_ENHANCED_AREA_HEIGHT][ORACLES_ENHANCED_CORE_WIDTH], uint32_t frame);
void ec_check_window_lines(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, const uint32_t *surface, int32_t camera_x, int32_t camera_y,
                           uint8_t box[ORACLES_ENHANCED_AREA_HEIGHT][ORACLES_ENHANCED_CORE_WIDTH], uint32_t frame);
void ec_check_curtain(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, const uint32_t *surface, int32_t camera_x, int32_t camera_y, uint32_t frame);

/* The rows and the figures written out (enhanced_check_report.c). */
void ec_write_frame_row(OraclesEnhancedCheck *c, uint32_t frame, uint64_t hash, OraclesEnhancedMode mode, int32_t camera_x, int32_t camera_y,
                        int shown_left, uint8_t room_left, int shown_right, uint8_t room_right, unsigned uncovered,
                        int scx, int scy, int off_x, int off_y);

#endif
