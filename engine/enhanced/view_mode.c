/* The mode of a frame: the wide world band in play, the
 * framed core for the menus, the cutscenes that are not the world, a game
 * area drawn elsewhere than its camera, and a load not yet placed. */
#include "view_internal.h"

/* The largest offset a shake adds to a scroll register (updateScreenShake@data: magnitude 2). */
#define SHAKE_REACH 3
#define SHAKE_HISTORY 0x0fu   /* the observations a shaken image may come from, the last four */

static int shaking(OraclesEnhancedView *v)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    return oracles_guest_read8(v->guest, t->screen_shake_counter_y) != 0 || oracles_guest_read8(v->guest, t->screen_shake_counter_x) != 0;
}

/* The scroll registers of each line of the game area, less the camera the
 * frame was drawn with (the journal, the stat mode 0 write landing on the
 * next line).  The game scrolls a line at a time to make the screen ripple
 * (under water, the return by the strange force: hLcdInterruptBehaviour 0
 * writes SCX and 1 SCY from wBigBuffer during the scan), which the whole
 * band can follow; a shift the same on every line is the area drawn elsewhere (a
 * cutscene's pan), which it cannot, but for the screen's shake: a gate that
 * opens, a bomb, a boss, add up to 3 pixels to the registers past the camera
 * while wScreenShakeCounterY or X counts down (updateScreenShake), and the
 * band shakes with them.  Returns 1 and fills `shift` for a ripple or a
 * shake, 0 otherwise. */
static int line_scroll_shifts(OraclesEnhancedView *v, int16_t shift[ORACLES_ENHANCED_AREA_HEIGHT], int16_t shift_y[ORACLES_ENHANCED_AREA_HEIGHT])
{
    size_t count = 0;
    const OraclesGuestRegWrite *j = oracles_guest_journal(v->guest, &count);
    const OraclesEnhancedObservation *ob = &v->observation;
    int scx = -1, scy = -1;
    for (size_t i = 0; i < count; i++) if (j[i].ly >= ORACLES_PPU_HEIGHT) { if (j[i].reg == IO_SCX) scx = j[i].value; if (j[i].reg == IO_SCY) scy = j[i].value; }
    int varies = 0;
    v->wave_lines = 0;
    for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) {
        for (size_t i = 0; i < count; i++) {
            if (j[i].ly >= ORACLES_PPU_HEIGHT) continue;
            const unsigned effective = j[i].stat_mode == 0 ? (unsigned)j[i].ly + 1u : j[i].ly;
            if (effective != ly) continue;
            if (j[i].reg == IO_SCX) scx = j[i].value;
            if (j[i].reg == IO_SCY) scy = j[i].value;
        }
        if (ly < ORACLES_ENHANCED_HUD_HEIGHT) continue;
        if (scx < 0 || scy < 0) return 0;
        const int dx = (int8_t)(uint8_t)(scx - ob->drawn_camera_x - ob->drawn_offset_x);
        const int dy = (int8_t)(uint8_t)(scy - ob->drawn_camera_y - ob->drawn_offset_y + (int)ORACLES_ENHANCED_HUD_HEIGHT);
        const unsigned line = ly - ORACLES_ENHANCED_HUD_HEIGHT;
        shift[line] = (int16_t)dx;
        shift_y[line] = (int16_t)dy;
        if (dx != shift[0] || dy != shift_y[0]) varies = 1;
    }
    v->wave_lines = varies;
    if (varies) return 1;
    /* The frame on screen shows the registers of an earlier logic, which
     * shook them and then counted down: the shake shows while the counter
     * ran before that logic, two observations back, or more when a logic ran
     * past the vblank and the image showed the same registers again (a
     * lagging frame).  The last four observations stand for it. */
    const int shaken = (shaking(v) || (v->shake_history & SHAKE_HISTORY)) && (shift[0] || shift_y[0])
        && shift[0] >= -SHAKE_REACH && shift[0] <= SHAKE_REACH && shift_y[0] >= -SHAKE_REACH && shift_y[0] <= SHAKE_REACH;
    if (shaken) v->shake_frames++;
    return shaken;
}

/* Whether the frame on screen drew its game area where the game's camera
 * says (SCX = camera + wScreenOffsetX, SCY = camera + wScreenOffsetY - 16,
 * updateGfxRegs2Scroll): the scroll registers in effect at its first
 * game-area line, replayed from the register journal (the vblank writes,
 * then the scan's up to line 16), against the camera and offsets the frame
 * was drawn with.  Cutscenes pan the area or flash it from other registers
 * for tens of frames: the world band would place that image at the wrong
 * spot.  Without a scroll write in the journal (the LCD off, a reload of
 * the tile map, the journal consumed by the native renderer) the frame
 * cannot be judged and is taken as agreeing. */
static int area_drawn_off_camera(OraclesEnhancedView *v)
{
    size_t count = 0;
    const OraclesGuestRegWrite *j = oracles_guest_journal(v->guest, &count);
    int scx = -1, scy = -1;
    for (size_t i = 0; i < count; i++) if (j[i].ly >= ORACLES_PPU_HEIGHT) { if (j[i].reg == IO_SCX) scx = j[i].value; if (j[i].reg == IO_SCY) scy = j[i].value; }
    for (unsigned ly = 0; ly <= ORACLES_ENHANCED_HUD_HEIGHT; ly++)
        for (size_t i = 0; i < count; i++) {
            if (j[i].ly >= ORACLES_PPU_HEIGHT) continue;
            const unsigned effective = j[i].stat_mode == 0 ? (unsigned)j[i].ly + 1u : j[i].ly;
            if (effective != ly) continue;
            if (j[i].reg == IO_SCX) scx = j[i].value;
            if (j[i].reg == IO_SCY) scy = j[i].value;
        }
    if (scx < 0 || scy < 0) return 0;
    const OraclesEnhancedObservation *ob = &v->observation;
    const unsigned dx = (unsigned)(scx - ob->drawn_camera_x - ob->drawn_offset_x) & 0xffu;
    const unsigned dy = (unsigned)(scy - ob->drawn_camera_y - ob->drawn_offset_y + (int)ORACLES_ENHANCED_HUD_HEIGHT) & 0xffu;
    return dx != 0 || dy != 0;
}

/* Whether the status bar is off the screen: the game splits the screen below
 * the bar (wGfxRegs1.LYC, 15 in play: the LCD interrupt loads the game area's
 * registers there); a screen of its own splits it nowhere (the registers of
 * the first line hold for all 144, gfxRegisterStates 4, 9 or $13) or on the
 * first line (blackTowerCompleteCutsceneHandler writes 0). */
static int status_bar_off(OraclesEnhancedView *v)
{
    const uint8_t *regs = oracles_guest_ptr(v->guest, oracles_guest_tables(v->guest)->gfx_regs1, 6u);
    return regs && (regs[5] == 0 || regs[5] >= ORACLES_PPU_HEIGHT);
}

/* The cutscenes that show full-screen pictures, or a room without its status
 * bar, between rooms they play with it (the endings, the credits, the black
 * tower's explanations, the flames lit in the room of rites): framed while
 * the bar is off, in the band with the rooms.  So is the fade-out before a
 * warp (cutscene03 to cutscene05) while the LCD is on: it shows the screen
 * left, a picture's warp out, never the bar of play; the blank after it is
 * a load's, as any warp's. */
static int picture_cutscene(const OraclesGuestTables *t, uint8_t cutscene, int lcd_on)
{
    if (cutscene == t->cutscene_03 || cutscene == t->cutscene_04 || cutscene == t->cutscene_05) return lcd_on;
    return cutscene == t->cutscene_black_tower_explanation || cutscene == t->cutscene_black_tower_escape
        || cutscene == t->cutscene_flame_of_sorrow || cutscene == t->cutscene_flame_of_despair
        || cutscene == t->cutscene_black_tower_complete || cutscene == t->cutscene_din_crystal_descending
        || cutscene == t->cutscene_flame_of_destruction || cutscene == t->cutscene_princess_kidnapped
        || cutscene == t->cutscene_room_of_rites_collapse || cutscene == t->cutscene_credits
        || cutscene == t->cutscene_the_end_screen;
}

/* The ring menu and cutscenes (LCD behaviours 5 and 6), a scanned window and
 * a game area drawn elsewhere than the camera are not part of the wide
 * world: they use the framed fallback. */
static int framed_frame_menus(OraclesEnhancedView *v)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const uint8_t behaviour = oracles_guest_read8(v->guest, t->lcd_interrupt_behaviour);
    if (behaviour == 5 || behaviour == 6) return 1;
    /* The cutscenes that draw their own screens over the room still loaded,
     * which stands for none of it: the pregame intro (CUTSCENE_PREGAME_INTRO,
     * both games: its orb, Link's object, and its text on a screen it
     * cleared), and in Seasons Din taken to Onox's castle (the tower rising
     * under a dark sky, then inside), the temple sinking and Onox's taunt
     * (a scene's graphics header and tile map, introCutscenes.s).  A scene
     * played in the room itself (Din's dance, Nayru's song after its picture,
     * which is drawn off the camera) moves objects over the room's own map
     * and stays in the band. */
    const uint8_t cutscene = oracles_guest_read8(v->guest, t->cutscene_index);
    if (v->observation.playing && cutscene != 0xffu
        && (cutscene == t->cutscene_pregame_intro || cutscene == t->cutscene_din_imprisoned
            || cutscene == t->cutscene_temple_sinking || cutscene == t->cutscene_onox_taunting)) return 1;
    const uint8_t *io = oracles_guest_io(v->guest);
    if (v->observation.playing && cutscene != 0xffu && picture_cutscene(t, cutscene, io && (io[IO_LCDC] & 0x80u)) && status_bar_off(v)) return 1;
    /* An open menu (wOpenedMenuType: the inventory, the map, save and quit,
     * the ring appraisal, the warp menu...) draws its screens with the
     * game's own background map and registers: not part of the world. */
    if (oracles_guest_read8(v->guest, t->opened_menu_type) != 0) return 1;
    if (io && (io[IO_LCDC] & 0x20u) && io[IO_WY] < 144u && io[IO_WX] <= 166u) return 1;
    return 0;
}

/* The game area drawn elsewhere than its camera, counted: the frames it frames. */
static int framed_area(OraclesEnhancedView *v)
{
    if (!area_drawn_off_camera(v)) return 0;
    v->off_camera_frames++;
    return 1;
}

/* The mode of the frame: the wide world band in play, the framed core
 * otherwise.  Loading a room (a warp, the first room) is shown in the world
 * as soon as the observer's reference is the room the game has, that is once
 * Link is placed in it (the door he walks into, the blank and the fade-in of
 * the room he arrives in), with whatever neighbours the ghost has run ahead
 * for; until then, and off the map, the framed core.  A load draws its room
 * with the registers it likes (the reveal from the centre): the off-camera
 * rule waits for play.  A game area scrolled line by line (a ripple) is
 * shown in the band with its waves: only the off-camera rule gives way to
 * it, never the menus and cutscenes, which scroll their own screens by line
 * too.  A room's load framed (the game loading it, its blank and its reveal,
 * Link walking in through the door until the camera takes him) is the
 * world's, shown at the view's scale before and after: the large menus do
 * not enlarge it (v->framed_load), or the room would jump to their scale and
 * back. */
OraclesEnhancedMode ev_choose_mode(OraclesEnhancedView *v, int tracking)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    v->enlarged_before = !v->have_view && !v->framed_load;
    const uint8_t *io = oracles_guest_io(v->guest);
    const int lcd_on = io && (io[IO_LCDC] & 0x80u);
    const int room_load = ob->in_transition && !ob->in_scroll;
    const int loading = (room_load
        && !(lcd_on && ev_on_grid(v) && v->observer.ref_group == ob->group && v->observer.ref_room == ob->room))
        || ob->cell_pending   /* a dungeon room not yet placed on its floor's map */
        || ev_maku_pending(v);
    const int menus = framed_frame_menus(v);
    v->have_wave = ob->playing && !loading && !menus && line_scroll_shifts(v, v->line_shift, v->line_shift_y);
    v->shake_history = (v->shake_history << 1) | (unsigned)shaking(v);
    const int off_camera = !room_load && !v->have_wave && framed_area(v);
    v->framed_load = !v->framed_only && !menus && !off_camera && (ob->loading_room || (ob->playing && (loading || room_load || ob->link_warping)));
    if (v->framed_only || !ob->playing || loading || menus || off_camera || !tracking) return ORACLES_ENHANCED_FRAMED;
    v->framed_load = 0;
    /* A cutscene's world frame drawn elsewhere than the camera, which the off-camera rule let through as a load:
     * counted, never meant (the room left loaded is the camera's, the image on screen is not). */
    if (ob->cutscene && !v->have_wave && area_drawn_off_camera(v)) v->world_off_camera_frames++;
    return ORACLES_ENHANCED_WORLD;
}

/* A room's load under the HUD band the world frames draw (the status bar, its gutters as hud= paints them, the
 * hotbar), so that the bar neither moves nor changes at a door: below it the game area alone, where the world shows
 * it, the rest black as the world band leaves what no source covers.  Its place: the last world frame's while the
 * image still shows the room left; after the load's blank, the camera's for a large room, which it frames from the
 * room itself, else centred, where the camera frames an interior of one room (its observation of Link held outside
 * the room until the walk in ends, the camera says nothing of the room entered before).  The blank itself (the LCD
 * off, a full fade: one colour over the whole image) stays framed whole, as the game shows it; after a frame the large
 * menus enlarge (a picture's fade to white, a scene's room, the map) it is that screen's blank, and keeps their scale
 * until the room shows, or it would shrink in place.  Any other framed frame lets the room left go. */
int ev_load_band(OraclesEnhancedView *v, OraclesEnhancedCompose *in, int tracking, int32_t camera_x, int32_t camera_y)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    v->load_band = 0;
    const int world = in->mode == ORACLES_ENHANCED_WORLD;
    if (world || (v->framed_load && !v->load_area_held && tracking && ob->large_grid)) {
        v->load_area_left = ob->window_left - camera_x;
        v->load_area_top = (int32_t)ORACLES_ENHANCED_HUD_HEIGHT + ob->window_top - camera_y;
        v->load_area_held = world;
        if (world) return 0;
    } else if (!v->framed_load) { v->load_area_held = 0; return 0; }
    else if (!v->load_area_held) {
        v->load_area_left = (int32_t)oracles_enhanced_hud_x(v->size);
        v->load_area_top = (int32_t)((v->size.height - ORACLES_PPU_HEIGHT) / 2u + ORACLES_ENHANCED_HUD_HEIGHT);
    }
    const uint32_t *image = ev_scroll_window(v);
    size_t i = 1;
    while (i < (size_t)ORACLES_ENHANCED_CORE_WIDTH * ORACLES_PPU_HEIGHT && image[i] == image[0]) i++;
    if (i == (size_t)ORACLES_ENHANCED_CORE_WIDTH * ORACLES_PPU_HEIGHT) {
        v->load_area_held = 0;
        v->load_blank_frames++;
        if (v->enlarged_before) v->framed_load = 0;
        return 0;
    }
    in->mode = ORACLES_ENHANCED_WORLD;
    in->world_left = 0;
    in->world_top = 0;
    in->window_world_left = v->load_area_left;
    in->window_world_top = v->load_area_top - (int32_t)ORACLES_ENHANCED_HUD_HEIGHT;
    v->load_band = 1;
    v->load_band_frames++;
    return 1;
}
