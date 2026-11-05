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
#include "sprites.h"

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

/* The waves line by line on the surface (ec_measure_whole_room). */
/* Link's sprites drawn against his place at the end of the frame before (ec_measure_swim): the frames counted and
 * those whose offset changed by a pixel or more, in a transition and the frames just after it, elsewhere, where he was
 * built from his fields; the largest change. */
typedef struct OraclesEnhancedDrawnCounts {
    unsigned scroll_frames, off_scroll_frames, mid_frames, off_mid_frames, off_built_frames;
    int32_t off_max;
} OraclesEnhancedDrawnCounts;

typedef struct EcRipple {
    unsigned wave_frames, edge_moves, rippled_frames;
    int have_edge;
    int32_t edge, camera_x, camera_y, window_left, window_top;
    int16_t row_edge[512];            /* each band line's edge at the last such frame, -1 none */
    unsigned moving_frames, moving_changes, still_frames, still_changes, changes_max;
    uint64_t epoch;
} EcRipple;

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
    /* The game's window against the band around it, in mean luminance (0..255): a scene
     * that darkens the game's screen and not the rooms around shows as a window much
     * darker than its surroundings (ec_measure_window_darker). */
    unsigned window_luma, ring_luma;
    unsigned window_darker_frames;
    uint32_t window_darker_first, window_darker_last;
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
    /* Link's world place over the last frames, to tell a scroll entered walking diagonally; in such a scroll, the
     * frames his place stood across the scroll's axis two frames running (ec_measure_scroll) */
    int32_t recent_x[8], recent_y[8];
    unsigned recent_count;
    uint64_t recent_epoch;
    int scroll_diagonal;
    int32_t scroll_across_last, scroll_across_step;
    unsigned scrolls_diagonal, scroll_across_stalled;
    char transition_list[512];
    /* the window against the drawn registers */
    unsigned window_checked, window_misplaced;
    unsigned last_wave_frames;        /* the view's count at the previous frame: this frame rippled if it grew */
    /* a room shown whole under the waves: its edge on the screen, line by line */
    EcRipple whole, water;            /* a room shown whole under the waves; any frame under the waves */
    unsigned whole_last_waves;
    unsigned hotbar_cutscene_frames;  /* frames the hotbar was drawn over a framed image or in a cutscene */
    unsigned hotbar_framed_only_frames, framed_only_frames;   /* F3's framed core: its frames, and those with the hotbar drawn */
    /* a scroll's start that opens a room shown alone, the game's window still: the band as shown must stand still */
    unsigned alone_scroll_frames, alone_scroll_band_moves, alone_scroll_cut_frames;
    int alone_have, alone_whole;   /* the last frame in play showed the room whole */
    int alone_play;                  /* the last frame in play showed the room whole, or the band still in an interior's room */
    uint8_t alone_group, alone_room;  /* the room of the last frame in play */
    uint32_t alone_frame;
    uint64_t alone_epoch;
    int32_t alone_band[2];
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
    unsigned enlarged_frames, enlarged_runs;   /* framed frames the large menus show at their own scale (the view's crop), and their runs */
    int last_enlarged;
    unsigned enlarged_framed_drops;   /* framed frames at the view's scale right after one enlarged: the picture shrinking in place */
    uint32_t enlarged_framed_drop_at[8];
    unsigned normal_band_frames;      /* world frames of the drawn-back view showing the normal band, centred in its own (an interior) */
    /* A room's load drawn under the world's HUD band: the last frame's kind (0 another, 1 a world frame, 2 a load so
     * drawn), its band's sixteen lines hashed, its game area's place; the steps between the two kinds, those where the
     * band changed, and those where the game area moved. */
    int load_last_kind;
    uint64_t load_last_band;
    int32_t load_last_left, load_last_top;
    unsigned load_boundaries, load_band_changes, load_area_moves;
    OraclesEnhancedMode last_mode;
    int have_mode;
    int have_last_observation;
    int32_t last_link_x, last_window_left;
    unsigned link_jumps, window_jumps;   /* between consecutive frames, any mode: a seam that is not continuous */
    unsigned link_jumps_y;               /* Link's world y */
    int32_t largest_link_jump_y, last_link_y;
    int32_t largest_link_jump, largest_window_jump;
    /* Link's sprite as the frame on screen draws it (his entries of the OAM
     * that frame showed, tagged by object), placed in the band and less the
     * shown camera: the frames it went a pixel or more back against the step
     * the frame before, he himself not turning (ec_measure_link_backsteps). */
    OraclesSprites *link_sprites;
    int have_drawn_link;
    uint64_t drawn_link_epoch;
    int32_t drawn_screen[2], drawn_world[2], drawn_screen_step[2], drawn_world_step[2];
    unsigned link_drawn_steps, link_backsteps, link_backsteps_y;
    char link_backsteps_list[96];      /* the first frames, "frame:x" or "frame:y" */
    /* Walking diagonally in normal play (his drawn place moved 3 px or more both ways over the last 8 frames, never
     * back): the frames, and those where the camera's step goes across the walk the other way from its last step
     * across (the band zigzagging with his gait) (ec_measure_link_backsteps). */
    int32_t wobble_world[9][2], wobble_camera[2];
    uint32_t wobble_frame[9], wobble_camera_frame;
    unsigned wobble_count;
    int wobble_last_across;
    unsigned diagonal_frames, camera_wobble;
    /* The same walk as the screen shows it at 4x (MEASURE_SCALE), the band moved by the sub-pixel camera's fraction
     * and Link drawn apart where the view does: the stops of the band on an axis between two of its steps (its jerks, an axis at a time), its steps back across the walk by 2 px or more (its zigzag), those Link moves on the
     * screen, and those he goes back there against his step before; and Link's own steps on the ground, the band's
     * added to his on the screen, back across the walk by 2 px or more (his zigzag, x and y crossing their pixels on
     * frames of their own). */
    int32_t shown_band[2], shown_link[2], shown_link_step[2];
    uint32_t shown_frame;
    int shown_band_moved[2];
    int shown_last_across, ground_last_across;
    unsigned band_stops, band_wobble, straight_frames, straight_band_stops, link_screen_moves, link_screen_backsteps, link_wobble;
    /* The band and Link as the screen shows them at 4x where the picture switches (a transition, a text box, Link
     * drawn apart): the change of their step from one frame to the next (a jolt) at the frame of the switch, the one
     * before and the one after, the largest on either axis; and the largest elsewhere (measure_switch_jolts). */
    int32_t jolt_band[2], jolt_link[2], jolt_band_step[2], jolt_link_step[2];
    uint32_t jolt_frame;
    uint64_t jolt_epoch;
    int jolt_have, jolt_have_step, jolt_state;
    unsigned jolt_edge_frames;           /* frames left in which a jolt is a switch's */
    int32_t jolt_pending_band, jolt_pending_link;   /* the last frame's, attributed once the next one says whether a switch follows */
    int jolt_pending;
    int32_t switch_band_jolt_max, switch_link_jolt_max, band_jolt_max, link_jolt_max;
    unsigned switch_edges;
    unsigned jolt_fraction_frames;       /* the last three frames the band had a fraction on, a bit each */
    unsigned layer_objects, layer_objects_off;   /* special objects but Link in the sub-pixel camera's layer, an object a frame; those at a fraction not his (measure_layer_objects) */
    /* Link's place against the ground as the screen at 4x shows it, on each axis: his layer's own move where he is drawn
     * apart, none where he stays in the band.  Where the layer comes or goes in a transition (a scroll, a room's load),
     * the game holding his fraction of a pixel there, he jumps against the ground by up to that fraction: those frames,
     * and the largest jump (measure_switch_jolts). */
    int32_t ground_offset[2];
    int ground_layer, ground_transition;
    /* In a scroll between two small rooms, Link as shown going back on the screen by 3 px or more at 4x against his
     * own step that frame, to the 256th of a pixel, then on again the next frame (a dip: his sprites drawn a pixel
     * behind his place while his fraction runs on, an OAM left uncopied). */
    int scroll_back_have;
    uint32_t scroll_back_frame;
    uint64_t scroll_back_epoch;
    int32_t scroll_back_link[2], scroll_back_place[2];
    int scroll_back_dip[2];
    unsigned scroll_link_dips;
    /* Link's sprites as the screen at 4x shows them, the OAM's entries of his graphics at his place or those the view
     * built from his fields, moved as the backend moves them (with his layer, else with the band): a step of 2 px or
     * more one way, then of 2 or more back at the next frame, he not turning (a zigzag: his sprites drawn a pixel off
     * his place for a frame or every other one). */
    int zig_have;
    uint32_t zig_frame;
    uint64_t zig_epoch;
    int32_t zig_place[2], zig_step[2], zig_world[2], zig_world_step[2];
    unsigned link_drawn_zigzags;
    char zig_list[128];
    /* Link swimming, or on Ages' sea floor: his sprites' place as the frame draws them (oracles_enhanced_view_link_drawn)
     * against his place at the end of the frame before, where the OAM draws him; that offset changing from a frame to
     * the next by a pixel or more on an axis, his pose the same: in a transition and the frames just after it
     * (swim.drawn_off_scroll_frames, of swim.drawn_scroll_frames), elsewhere (swim.drawn_off_mid_frames, of
     * swim.drawn_mid_frames), the largest change (swim.drawn_off_max); those where he was built from his fields, that
     * frame or the one before, apart (swim.drawn_off_built_frames).  The band as shown the same way, with the
     * sub-pixel camera: its step changing by 3 px of the screen at 4x or more (swim.band_jerks_scroll, _mid).  A scroll
     * entered swimming that walks him (swim.walked_scroll_frames), and the game taking him out of the water with his
     * own tile (y+5) past the room's last row (swim.left_water_past_room).  On foot, walking, the same offsets apart
     * (walk.drawn_*). */
    int swim_have, swim_off_have, swim_band_have;
    uint32_t swim_frame;
    uint64_t swim_epoch;
    int32_t swim_last_world[2], swim_off[2], swim_band[2], swim_band_step[2];
    uint16_t swim_pose[2];
    unsigned swim_after;
    int swim_scroll_swimming;
    uint8_t swim_last_state;
    OraclesEnhancedDrawnCounts swim_drawn, walk_drawn;   /* swimming or on Ages' sea floor, and on foot */
    int swim_last_drawn;
    unsigned swim_band_jerks_scroll, swim_band_jerks_mid, swim_walked_scroll_frames, swim_left_water_past_room;
    int smooth;
    unsigned transition_layer_switches;
    int32_t transition_layer_shift_max;
    char jolt_list[256];                 /* the switches where the band's step changed by 2 px or more, "frame:band/link" */
    uint64_t epochs;
    unsigned neighbours_frames[5];       /* world frames with 0 to 4 neighbours shown */
    unsigned transitions, transitions_with_terrain, transitions_black, transitions_vertical;   /* the scrolling transitions judged: was the room entered shown, and true; those on the vertical axis among them */
    unsigned transitions_dark;             /* of the rooms entered not shown, those kept dark as never visited, not black */
    unsigned dark_visited;                 /* rooms kept dark, frame by frame of play, whose flags the game says visited */
    unsigned dark_edge_stray;              /* rooms kept dark, frame by frame of play, lit on a side where nothing in clear stands beyond it */
    unsigned shown_unvisited;              /* rooms drawn in clear, frame by frame of play, whose flags the game says never visited, the room in play aside, and rooms kept dark shown in clear past their edge */
    unsigned transitions_other_room;  /* entered while another room was shown that way: a wrong neighbour, never allowed */
    char other_room_list[256];        /* "from>to=shown" */
    unsigned curtain_frames, curtain_frames_wrong, curtain_frames_unchecked, curtain_pixels_wrong;
    unsigned text_box_frames_in_place;     /* frames the box's own place in the window still shows the box */
    unsigned curtain_window_frames_wrong;  /* curtain frames whose window, in the band, is not the core's image */
    unsigned text_box_frames, text_box_frames_wrong, text_box_pixels_wrong;
    unsigned text_box_trail;               /* compositions left where the box is still on screen after the game's text flag fell */
    unsigned text_box_frames_at_origin;    /* frames the band showed the box where the game drew it, the view having to move it */
    char text_box_list[96];
    /* the text box drawn by the host (--text-box-check), against the core's image */
    int host_box;                            /* armed */
    unsigned host_box_frames, host_box_frames_world, host_box_frames_framed;   /* frames compared, by the view's mode */
    unsigned host_box_frames_wrong, host_box_uncovered, host_box_absent;   /* those with a pixel wrong; a box the host could not draw; a text shown without its box */
    unsigned host_box_unknown, host_box_unexpected;   /* the uncovered frames by why: a line unknown, a cell the model does not express */
    unsigned host_box_absent_run, host_box_absent_longest;   /* the frames in a row of a text without its box shown, the most of them */
    uint32_t host_box_absent_last;
    uint64_t host_box_pixels, host_box_pixels_wrong, host_box_pixels_skipped;   /* compared, wrong, left to an object or the window */
    uint64_t host_box_object_pixels;         /* of those left to an object, the ones whose object shows over the box in the core's image */
    double host_box_us_total, host_box_us_max;   /* the box's model and drawing, a frame */
    double host_box_wide_us_total;               /* the same box drawn again at 192 pixels and three lines */
    unsigned host_box_drawn;                 /* the frames it was drawn */
    char host_box_list[160];                 /* the first frames wrong, "frame:pixels" */
    char host_box_uncovered_list[96];
    char host_box_runs_list[400];            /* the frames of each box drawn, the first ones */
    uint32_t host_box_last_drawn;
    uint8_t saved_text_box[ORACLES_GUEST_TEXT_BOX_STATE_SIZE];   /* the host's lines, saved and loaded back with the reload */
    size_t saved_text_box_size;
    /* the host's wide box in the surface (--text-box wide), against its layout */
    int wide;                                /* drawn by the view */
    unsigned wide_pixels_width, wide_lines;
    unsigned wide_frames, wide_frames_world, wide_frames_framed, wide_frames_wrong;   /* frames it was drawn, by mode; with a pixel wrong */
    uint64_t wide_pixels, wide_pixels_wrong, wide_pixels_skipped;   /* text pixels compared with the layout's glyphs, wrong, left to a symbol or the window */
    unsigned wide_boxes, wide_pages;         /* boxes shown (runs of frames), pages (the game waiting for a button, or the text's end) */
    unsigned wide_reveal_frames, wide_cut_frames, wide_early_frames;   /* frames of a line being revealed; showing a character cut; one whose step the game has not shown */
    unsigned wide_cursor_frames, wide_symbol_tiles;   /* frames with a choice's cursor drawn; whole tiles drawn (symbols, choices' places) */
    unsigned wide_crop_frames, wide_crop_outside;     /* framed frames with the large menus' rectangle; those it does not hold the box in */
    unsigned text_size;                      /* --text-size: ORACLES_ENHANCED_TEXT_* */
    unsigned redraw_frames, redraw_wrong;    /* world frames whose box was drawn again at the other sizes and back; those not given back as composed */
    uint32_t *redraw_copy;
    int large_menus;                         /* --menus-large: the surfaces written as the presenter shows the framed core alone */
    unsigned surface_scale;                  /* --surface-scale */
    unsigned large_frames, large_frames_world, large_frames_framed;   /* frames the box was published for the presenter (Large) */
    unsigned large_frames_xlarge;        /* of those, at Very large (the large menus over the framed core) */
    unsigned long long large_pixels, large_pixels_wrong;   /* its pixels against the box it shows */
    unsigned scaled_frames, scaled_frames_world, scaled_frames_framed, scaled_misplaced;   /* frames of a box drawn twice as large; world ones off the middle */
    unsigned long long scaled_pixels, scaled_pixels_wrong;   /* its pixels compared with those it enlarges, wrong */
    double scaled_us_total, scaled_us_max;   /* the enlargement's copy, timed apart, a frame */
    uint32_t scaled_sink;
    unsigned wide_centred_frames;            /* frames showing a line of a page the game laid out, centred */
    unsigned wide_cursor_centred, wide_cursor_pixels_wrong;   /* a choice's cursor drawn on such a line; its pixels not the game's tile where it goes */
    unsigned wide_cursor_cells;   /* frames with a choice shown whose cursor's places the game draws in its line's tiles, compared */
    unsigned wide_absent, wide_absent_run, wide_absent_longest;
    uint32_t wide_absent_last, wide_last_drawn;
    unsigned wide_state;                     /* w7TextDisplayState at the last frame */
    double wide_us_total, wide_us_max;       /* the box drawn again from its model, a frame: the drawing's cost */
    char wide_list[160];                     /* the first frames wrong, "frame:pixels" */
    char wide_reveal_list[160];              /* the first frames cut or early, "frame:c" or "frame:e" */
    char page_list[400];                     /* the frames a page of the box drawn by the host was whole (the game waiting, or its text's end), the first ones */
    unsigned page_state;                     /* w7TextDisplayState at the last frame drawn */
    char symbol_list[160];                   /* the first frames a symbol's tile was drawn in the wide box */
    char curtain_list[96];
    /* the area blurb, from the game's objects: the last frames the game drew, their wOam and the blurb's slot then
     * (-1: none), and the world frames whose OAM showed its sprites, of those the ones the view left them in the
     * window (ec_check_blurb) */
    uint8_t blurb_drawn_oam[8][160];
    int blurb_drawn_slot[8];
    unsigned blurb_drawn_head, blurb_drawn_count;
    unsigned blurb_frames, blurb_frames_in_window;
    char blurb_list[96];
    unsigned terrains_equal, terrains_different, terrain_tiles_different;   /* the shown terrain against the layout committed after the entry */
    unsigned terrains_old;           /* entered while shown in its old state, the ghost running it again (its old season): not judged */
    int pending_old;
    char black_list[512];                /* the black ones: "from>to" */
    char different_list[512];            /* the different ones: "to:tiles" */
    int threaded;
    int paced;                    /* each frame held to the Game Boy's period, as played */
    /* --beside-witness (enhanced_check_beside.c): once Link has stood in a
     * room, each room drawn beside it from its capture against the game's own
     * entry into it from there, run by a ghost of the check's own. */
    OraclesGhost *witness;
    OraclesGhostResult *witness_result;
    uint8_t *witness_state;
    int witness_have_room, witness_done;
    uint8_t witness_group, witness_room;
    unsigned witness_quiet;
    unsigned beside_checked, beside_orphans;
    char beside_list[256];
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
    int last_dark[4];                      /* kept dark, never visited (--unvisited-rooms hidden) */
    uint8_t last_room[4];
    uint8_t last_play_room;
    int last_playing, last_mode_world;
    /* the reload check */
    uint32_t reload_at;
    uint32_t subpixel_switch_at[8];         /* --enhanced-subpixel-switch, and those done */
    unsigned subpixel_switches, subpixel_switched;
    unsigned surfaces;                      /* --surface-at: composed surfaces to write out, as PPM */
    uint32_t surface_at[16];
    const char *surface_path[16];
    uint8_t *saved_core;
    size_t saved_core_size;
    uint8_t saved_view[1024];   /* two camera records (both axes), 306 bytes each; the launcher keeps 1024 too */
    size_t saved_view_size;
    OraclesRoomTransition *transition;
    uint8_t saved_transition[ORACLES_ROOM_TRANSITION_STATE_SIZE];
    size_t saved_transition_size;
    uint8_t saved_compact[ORACLES_GUEST_COMPACT_STATE_SIZE];   /* the compact text's line, when saved within drawLineOfText */
    size_t saved_compact_size;
    uint32_t reload_track[64];              /* Link's place (y, x, 8.8) at each frame after the save to the end of the transition it was in */
    unsigned reload_tracked, reload_track_open, reload_track_compared, reload_track_differing;
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
void ec_measure_link_backsteps(OraclesEnhancedCheck *c, uint32_t frame, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, int32_t camera_x, int32_t camera_y);
void ec_measure_swim(OraclesEnhancedCheck *c, uint32_t frame, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, int32_t camera_x, int32_t camera_y);
void ec_replay_scroll_registers(OraclesEnhancedCheck *c, int *scx, int *scy);
void ec_check_window_place(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, int rippled, int scx, int scy, uint32_t frame);

/* The band's pixels against the core's image (enhanced_check_pixels.c). */
unsigned ec_text_box_pixels(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, uint8_t box[ORACLES_ENHANCED_AREA_HEIGHT][ORACLES_ENHANCED_CORE_WIDTH]);
void ec_check_text_box(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, const uint32_t *surface, int32_t camera_x, int32_t camera_y,
                       uint8_t box[ORACLES_ENHANCED_AREA_HEIGHT][ORACLES_ENHANCED_CORE_WIDTH], uint32_t frame);
void ec_check_window_lines(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, const uint32_t *surface, int32_t camera_x, int32_t camera_y,
                           uint8_t box[ORACLES_ENHANCED_AREA_HEIGHT][ORACLES_ENHANCED_CORE_WIDTH], uint32_t frame);
void ec_check_curtain(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, const uint32_t *surface, int32_t camera_x, int32_t camera_y, uint32_t frame);
void ec_measure_alone_rows(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, int32_t camera_y);
/* The game's window and the ring of the band around it, compared in luminance on a world frame. */
void ec_measure_whole_room(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, const uint32_t *surface,
                           int32_t camera_x, int32_t camera_y);
void ec_measure_window_darker(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, const uint32_t *surface,
                           int32_t camera_x, int32_t camera_y, int blank, uint32_t frame);
/* The text box drawn by the host against the core's image (enhanced_check_text_box.c). */
void ec_check_host_text_box(OraclesEnhancedCheck *c, OraclesEnhancedMode mode, uint32_t frame);
/* The host's wide box where the view drew it, against its layout (enhanced_check_text_box.c). */
void ec_check_wide_text_box(OraclesEnhancedCheck *c, OraclesEnhancedMode mode, const uint32_t *surface, uint32_t frame);
/* The text box drawn twice as large (Text size Large) against the box it enlarges (enhanced_check_text_box.c). */
void ec_check_scaled_text_box(OraclesEnhancedCheck *c, OraclesEnhancedMode mode, const uint32_t *surface);
void ec_check_text_size_redraw(OraclesEnhancedCheck *c, OraclesEnhancedMode mode);
void ec_host_text_box_summary(const OraclesEnhancedCheck *c, FILE *out);
void ec_host_text_box_report(const OraclesEnhancedCheck *c, FILE *out);
void ec_note_blurb_drawn(OraclesEnhancedCheck *c);
void ec_check_blurb(OraclesEnhancedCheck *c, OraclesEnhancedMode mode, uint32_t frame);

/* The rooms drawn beside the room in play against the game's own entry into them (enhanced_check_beside.c). */
void ec_check_beside(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, uint32_t frame);

/* The rows and the figures written out (enhanced_check_report.c). */
void ec_write_frame_row(OraclesEnhancedCheck *c, uint32_t frame, uint64_t hash, OraclesEnhancedMode mode, int32_t camera_x, int32_t camera_y,
                        int shown_left, uint8_t room_left, int shown_right, uint8_t room_right, unsigned uncovered,
                        int scx, int scy, int off_x, int off_y);

#endif
