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
     * played in the room itself (Din's dance, Nayru's song) moves objects
     * over the room's own map and stays in the band. */
    const uint8_t cutscene = oracles_guest_read8(v->guest, t->cutscene_index);
    if (v->observation.playing && cutscene != 0xffu
        && (cutscene == t->cutscene_pregame_intro || cutscene == t->cutscene_din_imprisoned
            || cutscene == t->cutscene_temple_sinking || cutscene == t->cutscene_onox_taunting)) return 1;
    /* An open menu (wOpenedMenuType: the inventory, the map, save and quit,
     * the ring appraisal, the warp menu...) draws its screens with the
     * game's own background map and registers: not part of the world. */
    if (oracles_guest_read8(v->guest, t->opened_menu_type) != 0) return 1;
    const uint8_t *io = oracles_guest_io(v->guest);
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
 * too. */
OraclesEnhancedMode ev_choose_mode(OraclesEnhancedView *v, int tracking)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    const uint8_t *io = oracles_guest_io(v->guest);
    const int lcd_on = io && (io[IO_LCDC] & 0x80u);
    const int room_load = ob->in_transition && !ob->in_scroll;
    const int loading = (room_load
        && !(lcd_on && ev_on_grid(v) && v->observer.ref_group == ob->group && v->observer.ref_room == ob->room))
        || ob->cell_pending;   /* a dungeon room not yet placed on its floor's map */
    v->have_wave = ob->playing && !loading && !framed_frame_menus(v) && line_scroll_shifts(v, v->line_shift, v->line_shift_y);
    v->shake_history = (v->shake_history << 1) | (unsigned)shaking(v);
    const int off_camera = !room_load && !v->have_wave && framed_area(v);
    if (v->framed_only || !ob->playing || loading || framed_frame_menus(v) || off_camera || !tracking) return ORACLES_ENHANCED_FRAMED;
    return ORACLES_ENHANCED_WORLD;
}
