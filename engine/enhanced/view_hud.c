/* The HUD band in the view: the status bar as the game keeps it, read from
 * its maps in the RAM, and the gutters beside it taking the bar's own
 * background with the bar drawn full width (hud=full), or the bar cut into
 * its blocks and laid out across the band on that background (hud=wide). */
#include "view_internal.h"

#include "hotbar.h"

/* The status bar as the game keeps it: the maps of its bar, the LCDC it gives those lines, the VRAM and the palettes on
 * screen (oracles_enhanced_bar_pixel). */
static int status_bar(OraclesGuest *guest, OraclesEnhancedBar *bar)
{
    const OraclesGuestTables *t = oracles_guest_tables(guest);
    const uint8_t *regs = oracles_guest_ptr(guest, t->gfx_regs1, 1u);
    bar->tiles = oracles_guest_ptr(guest, t->status_bar_tile_map, 0x40u);
    bar->attributes = oracles_guest_ptr(guest, t->status_bar_attribute_map, 0x40u);
    bar->vram0 = oracles_guest_vram(guest, 0);
    bar->vram1 = oracles_guest_vram(guest, 1);
    bar->palettes = oracles_guest_bg_palettes(guest);
    bar->lcdc = regs ? regs[0] : 0u;
    return regs && bar->tiles && bar->attributes && bar->vram0 && bar->vram1 && bar->palettes;
}

int ev_status_bar(OraclesEnhancedView *v, OraclesEnhancedBar *bar) { return status_bar(v->guest, bar); }

static uint32_t core_colour(void *opaque, uint16_t rgb555) { return oracles_core_convert_rgb555(((OraclesEnhancedView *)opaque)->core, rgb555); }
static uint32_t core_colour_of(void *opaque, uint16_t rgb555) { return oracles_core_convert_rgb555(opaque, rgb555); }

int oracles_enhanced_bar_drawn(OraclesGuest *guest, OraclesCore *core)
{
    OraclesEnhancedBar bar;
    const uint32_t *pixels = oracles_core_pixels(core);
    if (!guest || !pixels || !status_bar(guest, &bar)) return 0;
    const OraclesEnhancedBarLayout layout = oracles_enhanced_bar_layout(&bar);
    if (layout == ORACLES_ENHANCED_BAR_NONE || !oracles_enhanced_bar_layout_shown(&bar, layout, core_colour_of, core, pixels)) return 0;
    /* Neither the bar nor the area under it one colour: the blank between two rooms, a screen faded out. */
    const unsigned bar_end = ORACLES_ENHANCED_HUD_HEIGHT * ORACLES_ENHANCED_CORE_WIDTH, end = ORACLES_PPU_HEIGHT * ORACLES_ENHANCED_CORE_WIDTH;
    unsigned i = 1, j = bar_end + 1;
    while (i < bar_end && pixels[i] == pixels[0]) i++;
    while (j < end && pixels[j] == pixels[bar_end]) j++;
    return i < bar_end && j < end;
}

int oracles_enhanced_bar_in_play(OraclesGuest *guest, OraclesCore *core)
{
    if (!oracles_enhanced_bar_drawn(guest, core)) return 0;
    /* Not in a cutscene, a menu (the inventory keeps the bar at its top) nor a dialogue. */
    const OraclesGuestTables *t = oracles_guest_tables(guest);
    return oracles_guest_read8(guest, t->cutscene_index) <= 1u && !oracles_guest_read8(guest, t->opened_menu_type)
           && !oracles_guest_read8(guest, t->text_is_active);
}

/* The game's status bar on screen in the core's image: a layout its map signs that the image shows, or the one last
 * shown while its map still signs it (the palettes changed since the frame was drawn, in a fade or a flash; a sprite
 * over its frame).  Not over the title, the file select, a menu, a cutscene that clears the bar. */
int ev_status_bar_shown(OraclesEnhancedView *v)
{
    OraclesEnhancedBar bar;
    const uint32_t *core = oracles_core_pixels(v->core);
    if (!core || !ev_status_bar(v, &bar)) { v->bar_shown_layout = ORACLES_ENHANCED_BAR_NONE; return 0; }
    const OraclesEnhancedBarLayout map = oracles_enhanced_bar_layout(&bar);
    const int shown = map != ORACLES_ENHANCED_BAR_NONE && (map == v->bar_shown_layout || oracles_enhanced_bar_layout_shown(&bar, map, core_colour, v, core));
    v->bar_shown_layout = shown ? map : ORACLES_ENHANCED_BAR_NONE;
    return shown;
}

/* The gutters beside the status bar with the bar drawn full width: in the world band alone, and a room's load drawn
 * under it, the framed core's bar standing in the middle of the drawn-back surface, or being no bar at all (a menu, a
 * cutscene).  The counts are the world frames'. */
const uint32_t *ev_hud_fill(OraclesEnhancedView *v, OraclesEnhancedMode mode, int counted)
{
    OraclesEnhancedBar bar;
    const uint32_t *core = oracles_core_pixels(v->core);   /* the bar's lines, which a scroll's redrawn window copies as they are */
    if (v->hud == ORACLES_ENHANCED_HUD_ORIGINAL || mode != ORACLES_ENHANCED_WORLD || !core || !ev_status_bar(v, &bar)) return NULL;
    const unsigned filled = oracles_enhanced_hud_fill(&bar, core_colour, v, core, v->hud_fill);
    if (counted && !v->load_band) {
        if (filled) v->hud_frames++;
        v->hud_lines_kept += 2u * ORACLES_ENHANCED_HUD_HEIGHT - filled;
    }
    return filled ? v->hud_fill : NULL;
}

/* The bar laid out, in the world band and a room's load drawn under it: in the layout its map signs where the core's
 * image shows that layout's frame; else in the last one laid out while the image shows it still (the map changes a turn
 * or two before the screen: a cutscene hiding the bar, a fifteenth heart) or while the map signs it still (a sprite over
 * the frame, the LCD off, a full fade); else centred.  Biggoron's sword frames B and A as one block, tightened with more
 * than fourteen hearts (in quarters, as loadStatusBarMap counts them), read where the profile binds wLinkMaxHealth,
 * else as the last layout of the bar's own told.  The counts are the world frames'. */
const OraclesEnhancedHudPlace *ev_hud_place(OraclesEnhancedView *v, OraclesEnhancedMode mode, int counted)
{
    OraclesEnhancedBar bar;
    const uint32_t *core = oracles_core_pixels(v->core);
    v->hud_laid = 0;
    if (v->hud != ORACLES_ENHANCED_HUD_WIDE || mode != ORACLES_ENHANCED_WORLD || !core || !ev_status_bar(v, &bar)) return NULL;
    const OraclesEnhancedBarLayout map = oracles_enhanced_bar_layout(&bar), last = v->hud_layout;
    OraclesEnhancedBarLayout layout = ORACLES_ENHANCED_BAR_NONE;
    if (map != ORACLES_ENHANCED_BAR_NONE && oracles_enhanced_bar_layout_shown(&bar, map, core_colour, v, core)) layout = map;
    else if (last != ORACLES_ENHANCED_BAR_NONE && (last == map || oracles_enhanced_bar_layout_shown(&bar, last, core_colour, v, core))) layout = last;
    v->hud_layout = layout;
    if (layout == ORACLES_ENHANCED_BAR_NORMAL || layout == ORACLES_ENHANCED_BAR_TIGHT) {
        v->hud_tight = layout == ORACLES_ENHANCED_BAR_TIGHT;
        v->hud_tight_known = 1;
    } else if (layout == ORACLES_ENHANCED_BAR_BIGGORON) {
        const uint8_t *max_health = oracles_guest_ptr(v->guest, oracles_guest_tables(v->guest)->link_max_health, 1u);
        if (max_health) { v->hud_tight = *max_health >= 14u * 4u + 1u; v->hud_tight_known = 1; }
    }
    const int laid = layout != ORACLES_ENHANCED_BAR_NONE && v->hud_tight_known;
    /* The hotbar in the game's look where the band holds it beside the bar's blocks; else, and without a hotbar shown,
     * the compact look's width, the layout a player without the hotkeys has always had. */
    if (laid) {
        const unsigned buttons = v->hud_tight ? ORACLES_ENHANCED_BAR_BUTTONS_TIGHT : ORACLES_ENHANCED_BAR_BUTTONS;
        v->hud_place = oracles_enhanced_hud_place(v->size.width, buttons, oracles_hotbar_width(ORACLES_HOTBAR_GAME));
        v->hotbar_look = ORACLES_HOTBAR_GAME;
        if (!v->hotbar_shown || !v->hud_place.hotbar) {
            v->hud_place = oracles_enhanced_hud_place(v->size.width, buttons, oracles_hotbar_width(ORACLES_HOTBAR_COMPACT));
            v->hotbar_look = ORACLES_HOTBAR_COMPACT;
        }
    }
    if (counted && !v->load_band) {
        if (laid) v->hud_wide_frames++;
        else v->hud_wide_centred++;
        if (laid && v->hud_place.one_row) v->hud_wide_one_row++;
    }
    v->hud_laid = laid;
    return laid ? &v->hud_place : NULL;
}

void oracles_enhanced_view_set_hud(OraclesEnhancedView *v, int hud)
{
    v->hud = hud == ORACLES_ENHANCED_HUD_FULL || hud == ORACLES_ENHANCED_HUD_WIDE ? hud : ORACLES_ENHANCED_HUD_ORIGINAL;
}
int oracles_enhanced_view_hud(const OraclesEnhancedView *v) { return v->hud; }

int oracles_enhanced_view_redraw_hud(OraclesEnhancedView *v)
{
    const uint32_t *core = oracles_core_pixels(v->core);   /* its status bar's lines, which the window drawn again keeps as they are */
    if ((!v->have_view && !v->load_band) || !core) return 0;
    OraclesEnhancedCompose in;
    memset(&in, 0, sizeof in);
    in.mode = ORACLES_ENHANCED_WORLD;
    in.border = BORDER_COLOUR;
    in.size = v->size;
    in.fade = v->fade_effective;
    in.colours = v->colours;
    in.hud_fill = ev_hud_fill(v, in.mode, 0);
    in.hud_place = ev_hud_place(v, in.mode, 0);
    oracles_enhanced_compose_hud(core, &in, v->surface);
    ev_draw_hotbar(v);
    return 1;
}

void oracles_enhanced_view_hud_counts(const OraclesEnhancedView *v, unsigned *frames, unsigned *lines_kept)
{
    if (frames) *frames = v->hud_frames;
    if (lines_kept) *lines_kept = v->hud_lines_kept;
}

void oracles_enhanced_view_hud_wide_counts(const OraclesEnhancedView *v, unsigned *laid, unsigned *centred, unsigned *one_row)
{
    if (laid) *laid = v->hud_wide_frames;
    if (centred) *centred = v->hud_wide_centred;
    if (one_row) *one_row = v->hud_wide_one_row;
}
