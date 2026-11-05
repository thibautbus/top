#include "view_internal.h"

typedef struct edge_sprite { uint8_t y, x, tile, attr; } edge_sprite;

/* Link's sprite is two 8x16 columns, one OAM entry each, on the same line,
 * 8 px apart, drawn from his own graphics, which the game copies to tiles
 * $70-$73 of VRAM bank 1 (loadLinkAndCompanionAnimationFrame queues them
 * to $8701, the low byte the bank; attribute bit 3): tiles t and t+2
 * (the flipped pose swaps them) walking and swimming across, one tile twice,
 * the second flipped, swimming up or down (his pose's block of sprite data,
 * object_sprites.h, says which).  In the water the game draws him lower, up
 * to 6 px.  Past the edge of the game's screen the game writes no entry for
 * the outer column: during a scrolling transition, when exactly one column
 * stands at Link's place, the other is rebuilt from it, so that the
 * continuous transitions can walk or swim him past the edge
 * without cutting him in two. */
/* Link swimming, or on Ages' sea floor. */
static int link_in_water(OraclesEnhancedView *v)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    return oracles_guest_read8(v->guest, t->link_swimming_state) || (oracles_guest_read8(v->guest, t->tileset_flags) & ORACLES_TILESETFLAG_UNDERWATER);
}

/* Whether an OAM entry stands at Link's place: his left column (0) or his
 * right one (1), else -1.  The OAM lags Link's position by the moves of up
 * to two logic frames (the game's own and the policy's), two pixels either
 * way on the routes measured: each column is taken 4 px either side of its
 * place, the two windows meeting half-way. */
static int link_column(const OraclesEnhancedView *v, const edge_sprite *e)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    const uint8_t *link = oracles_guest_object(v->guest, 0, 0);
    if (!link || (e->tile & 0xfcu) != ORACLES_LINK_TILES || !(e->attr & ORACLES_OAM_ATTR_VRAM_BANK1)) return -1;
    /* At the frame that chains two scrolls his coordinates are the room entered's already, the camera the frame was
     * drawn with still the room left's: his place in that window is taken in the world. */
    const int chained = ob->scroll_chained;
    const unsigned ax = (unsigned)(chained ? ob->world.link_x - ob->window_left : link[ORACLES_OBJ_XH] - ob->drawn_camera_x) & 0xffu;
    const unsigned ay = (unsigned)((chained ? ob->world.link_y - ob->window_top : link[ORACLES_OBJ_YH] - ob->drawn_camera_y) + 24) & 0xffu;
    const int dy = (int8_t)(uint8_t)(e->y - ay), dx = (int8_t)(uint8_t)(e->x - ax);
    if (dy < -2 || dy > 2 + (int)ORACLES_LINK_SWIM_LOWER) return -1;
    if (dx >= -4 && dx <= 3) return 0;
    if (dx >= 4 && dx <= 11) return 1;
    return -1;
}

/* His left column at x 0 standing at its very place, Link on the room's first pixel column (as far as two pixels on, the
 * OAM a frame or two behind him), never a column pushed there from further left. */
static int link_at_left_line(const OraclesEnhancedView *v, const edge_sprite *e)
{
    const uint8_t *link = oracles_guest_object(v->guest, 0, 0);
    if (!link || e->x != 0u || link_column(v, e) != 0) return 0;
    return ((unsigned)(link[ORACLES_OBJ_XH] - v->observation.drawn_camera_x) & 0xffu) <= 2u;
}

/* In the water, his left column at x 0 with his right one at x 8 on its line: Link just past the window's left edge
 * where the frame's camera is not yet the one his coordinates are in (the first frame after a scroll, his coordinates
 * the room's own and the camera still the scroll's); a column pushed there from further left has its twin short of 8. */
static int link_left_of_window(OraclesEnhancedView *v, const uint8_t *oam, const edge_sprite *e)
{
    if (e->x != 0u || (e->tile & 0xfcu) != ORACLES_LINK_TILES || !(e->attr & ORACLES_OAM_ATTR_VRAM_BANK1) || !link_in_water(v)) return 0;
    for (unsigned i = 0; i < 40u; i++)
        if (oam[i * 4] == e->y && oam[i * 4 + 1] == 8u && (oam[i * 4 + 2] & 0xfcu) == ORACLES_LINK_TILES && (oam[i * 4 + 3] & ORACLES_OAM_ATTR_VRAM_BANK1)) return 1;
    return 0;
}

int ev_link_column(const OraclesEnhancedView *v, const uint8_t entry[4])
{
    const edge_sprite e = { entry[0], entry[1], entry[2], entry[3] };
    return link_column(v, &e);
}

/* Whether Link's pose is one tile twice, flipped (swimming up or down). */
static int link_pose_mirrored(OraclesEnhancedView *v)
{
    const uint8_t *link = oracles_guest_object(v->guest, 0, 0);
    if (!link || !v->rom) return 0;
    OraclesObjectSprite sprites[ORACLES_OBJECT_SPRITES_MAX];
    const int built = oracles_object_sprites_build(v->guest, link, v->rom, v->rom_size, sprites, ORACLES_OBJECT_SPRITES_MAX);
    int own[2], n = 0;
    for (int i = 0; i < built && n < 2; i++) if (sprites[i].effect == ORACLES_SPRITE_OWN) own[n++] = i;
    return n == 2 && sprites[own[0]].tile == sprites[own[1]].tile;
}

static unsigned rebuild_link_column(OraclesEnhancedView *v, edge_sprite *entries, unsigned count)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    if (!ob->in_scroll && !ob->scroll_chained) return count;
    int found = -1, found_left = 0, others = 0;
    for (unsigned i = 0; i < count; i++) {
        const int column = link_column(v, &entries[i]);
        if (column < 0) continue;
        if (found >= 0) others++; else { found = (int)i; found_left = column == 0; }
    }
    if (found < 0 || others) return count;   /* none, or both columns there: nothing to rebuild */
    const edge_sprite *e = &entries[found];
    const int flipped = (e->attr & 0x20u) != 0;
    edge_sprite twin = *e;
    twin.x = (uint8_t)(found_left ? e->x + 8u : e->x - 8u);
    if (link_pose_mirrored(v)) twin.attr = (uint8_t)(e->attr ^ 0x20u);
    else twin.tile = (uint8_t)(found_left ? (flipped ? e->tile - 2 : e->tile + 2) : (flipped ? e->tile + 2 : e->tile - 2));
    entries[count] = twin;
    return count + 1u;
}

enum { ROWS_BELOW_BAR, ROWS_ALL, ROWS_UNDER_BAR };

/* Whether what the band shows at world pixel (x, y) hides a sprite's pixel of attribute `attr` there, as the LCD
 * would: the core's window from the live VRAM (window_background), elsewhere the source the composition drew on top,
 * from its own map (a bridge's edge, an arch at a room's border).  A source without a map (a room kept dark, a large
 * room's older capture) hides nothing. */
/* Whether source `b`'s background at its pixel (bx, by) hides a sprite's pixel of attribute `attr`. */
static int source_hides(const band_background *b, int32_t bx, int32_t by, uint8_t attr)
{
    if (!b->drawn || !(b->regs.lcdc & 0x01u)) return 0;
    /* A large room's lower lines were drawn in a second pass, its SCY 128 further on, at the same lines of the frame. */
    const unsigned pass = (unsigned)by / ORACLES_ENHANCED_AREA_HEIGHT;
    OraclesPpuRegs r = b->regs;
    r.scy = (uint8_t)(r.scy + pass * ORACLES_ENHANCED_AREA_HEIGHT);
    const unsigned ly = ORACLES_ENHANCED_HUD_HEIGHT + (unsigned)by - pass * ORACLES_ENHANCED_AREA_HEIGHT;
    return oracles_ppu_hides_object(r.lcdc, oracles_ppu_background_at(&b->maps, &r, (unsigned)bx, ly), attr);
}

static int background_hides(const OraclesEnhancedView *v, int32_t x, int32_t y, uint8_t attr)
{
    const band_background *w = &v->window_background;
    if (x >= w->left && x < w->left + (int32_t)w->width && y >= w->top && y < w->top + (int32_t)w->height) return source_hides(w, x - w->left, y - w->top, attr);
    for (unsigned n = 0; n < v->background_count; n++) {
        const band_background *b = &v->backgrounds[n];
        const int32_t bx = x - b->left, by = y - b->top;
        if (b->absent || bx < 0 || bx >= (int32_t)b->width || by < 0 || by >= (int32_t)b->height) continue;
        return source_hides(b, bx, by, attr);
    }
    return 0;
}

/* One sprite's pixels outside the core's window, drawn into the world band
 * at their place, with the sprite's palette and flips: the part inside the
 * window is already in the core's image.  `sx`, `sy`: its top left in the
 * game area.  The lines above the game area, under the status bar, are drawn
 * for ROWS_ALL, alone for ROWS_UNDER_BAR, not for ROWS_BELOW_BAR.  `made`: 1
 * for Link's built without the OAM, handed to the sub-pixel camera's layer; 2
 * the same, inside the window too, where the core's image has none of him. */
static void draw_sprite_outside(OraclesEnhancedView *v, int sx, int sy, uint8_t tile, uint8_t attr, unsigned height, int rows,
                                int32_t camera_x, int32_t camera_y, int made)
{
    const uint8_t *palettes = oracles_guest_obj_palettes(v->guest);
    const uint8_t *vram0 = oracles_guest_vram(v->guest, 0), *vram1 = oracles_guest_vram(v->guest, 1);
    if (!palettes || !vram0 || !vram1) return;
    const OraclesEnhancedObservation *ob = &v->observation;
    const uint8_t *bank = (attr & 0x08u) ? vram1 : vram0;
    const uint8_t *palette = palettes + (attr & 0x07u) * 8u;
    for (unsigned row = 0; row < height; row++) {
        const int gy = sy + (int)row;
        if (gy < 0 ? rows == ROWS_BELOW_BAR : rows == ROWS_UNDER_BAR) continue;
        const int inside_y = gy >= 0 && gy < (int)ORACLES_ENHANCED_AREA_HEIGHT;
        const unsigned line = (attr & 0x40u) ? height - 1u - row : row;
        const unsigned t = height == 16u ? ((tile & 0xfeu) + (line >> 3)) : tile;
        const uint8_t lo = bank[t * 16u + (line & 7u) * 2u], hi = bank[t * 16u + (line & 7u) * 2u + 1u];
        for (unsigned col = 0; col < 8; col++) {
            const int gx = sx + (int)col;
            if (made < 2 && inside_y && gx >= 0 && gx < (int)ORACLES_ENHANCED_CORE_WIDTH) continue;     /* inside the window: the core drew it */
            const unsigned bit = (attr & 0x20u) ? col : 7u - col;
            const unsigned colour = ((lo >> bit) & 1u) | (((hi >> bit) & 1u) << 1);
            if (!colour) continue;
            /* A ripple moves the room under the sprite, not the sprite: it keeps its place on the screen. */
            const int32_t py = ob->window_top + gy - camera_y;
            const int32_t px = ob->window_left + gx - camera_x;
            if (px < 0 || px >= (int32_t)v->size.width || py < 0 || py >= (int32_t)v->band_height) continue;
            if (background_hides(v, ob->window_left + gx, ob->window_top + gy, attr)) { v->sprite_hidden_pixels++; continue; }
            const uint16_t rgb = (uint16_t)(palette[colour * 2u] | (palette[colour * 2u + 1u] << 8));
            uint32_t *pixel = ev_band_pixel(v, (unsigned)px, (unsigned)py);
            if (made) ev_subpixel_made(v, px, py, *pixel, v->colours[rgb & 0x7fffu]);   /* his, which the OAM does not hold */
            if (made == 2) v->link_chained_pixels++;
            *pixel = v->colours[rgb & 0x7fffu];
        }
    }
}

unsigned oracles_enhanced_view_link_unshown_frames(const OraclesEnhancedView *v) { return v->link_unshown_frames; }
unsigned oracles_enhanced_view_link_built_frames(const OraclesEnhancedView *v) { return v->link_built_frames; }
void oracles_enhanced_view_link_chained_frames(const OraclesEnhancedView *v, unsigned *built, unsigned *unshown)
{
    *built = v->link_chained_frames; *unshown = v->link_chained_unshown;
}
void oracles_enhanced_view_sprite_hidden(const OraclesEnhancedView *v, unsigned *pixels, unsigned *frames)
{
    *pixels = v->sprite_hidden_pixels; *frames = v->sprite_hidden_frames;
}
unsigned oracles_enhanced_view_link_left_line_frames(const OraclesEnhancedView *v) { return v->link_left_line_frames; }
int oracles_enhanced_view_link_drawn(const OraclesEnhancedView *v, int *x, int *y)
{
    *x = v->link_drawn_x; *y = v->link_drawn_y;
    return v->link_drawn;
}

/* The sprites the game placed beyond the edges of its 160x128 game area:
 * the OAM still holds them (the game culls objects only past the room's
 * edge), the LCD does not show them, or hides them under the status bar.
 * They are drawn into the world band at their place, over the neighbours'
 * terrain, in OAM order (the first entry on top), with the object's palette
 * and flips; the part inside the window is already in the core's image.
 * Link walking past an edge during a continuous transition, an
 * enemy half out of the window, keep their whole sprite. */
void ev_overlay_edge_sprites(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y)
{
    v->edge_link_made = v->edge_link_built = 0;
    v->link_drawn = 0;
    v->sub_made_count = 0; v->sub_made_lost = 0;
    const uint8_t *oam = oracles_guest_oam(v->guest), *io = oracles_guest_io(v->guest);
    if (!oam || !io || !(io[IO_LCDC] & 0x02u)) return;
    const unsigned height = (io[IO_LCDC] & 0x04u) ? 16u : 8u;
    const OraclesEnhancedObservation *ob = &v->observation;
    edge_sprite entries[41];
    unsigned count = 0;
    int link_present = 0, link_shown = 0, left_line = -1, on_line = 0;
    for (unsigned i = 0; i < 40u; i++) {
        if (oam[i * 4] == 0) continue;
        const edge_sprite probe = { oam[i * 4], oam[i * 4 + 1], oam[i * 4 + 2], oam[i * 4 + 3] };
        const int link_entry = link_column(v, &probe) >= 0;
        /* x 0 is the game's hidden column as much as y 0 or 224: the LCD shows
         * neither, and a column pushed past the left edge lands on 0 whatever
         * its true place; but Link standing on the room's left line (xh 0, the
         * transitions at the line) has his left column's true place at x 0,
         * 8 px left of the window, as has Link in the water with his right
         * column at x 8 (link_left_of_window). */
        const int at_line = oam[i * 4 + 1] == 0 && link_at_left_line(v, &probe);
        if (oam[i * 4 + 1] == 0 && !at_line && !link_left_of_window(v, oam, &probe)) continue;
        if (link_entry) link_present = 1;
        /* His graphics on the LCD, at his place or where an older drawing left them: the core's image shows him. */
        if ((probe.tile & 0xfcu) == ORACLES_LINK_TILES && (probe.attr & ORACLES_OAM_ATTR_VRAM_BANK1) && probe.y < 160u && probe.x < 168u) link_shown = 1;
        /* Past the screen's bottom (160 to 223), or past its top (224 and over, wrapped): Link's alone, which a
         * continuous transition takes there, swimming down or up across the edge (up to 30 px past it). */
        if (oam[i * 4] >= 160u && !link_entry) continue;
        if (v->blurb_mask & (1ull << i)) continue;   /* the area blurb: drawn apart (ev_overlay_area_blurb) */
        if (v->large_objects_entries & (1ull << i)) continue;   /* a large room's object: drawn from its model (ev_overlay_large_objects) */
        /* During a transition, the objects of the two rooms beyond the window
         * belong to the capture of the room entered and to the image of the
         * room left (docs/ARCHITECTURE.md, who draws an object during an entry): only Link and what persists across
         * rooms is drawn from the live OAM there. */
        if (v->neighbour_objects && ob->in_transition && ev_live_entry_is_room_object(v, i)) continue;
        if (oam[i * 4 + 1] == 0) { left_line = (int)count; on_line = at_line; }
        entries[count].y = oam[i * 4]; entries[count].x = oam[i * 4 + 1]; entries[count].tile = oam[i * 4 + 2]; entries[count].attr = oam[i * 4 + 3];
        count++;
    }
    unsigned rebuilt = 41u;   /* the column rebuilt, the last entry, when there is one */
    if (height == 16u) {
        const unsigned found = count;
        count = rebuild_link_column(v, entries, count);
        v->edge_link_made = count != found;
        if (v->edge_link_made) rebuilt = count - 1u;
    }
    for (int i = (int)count - 1; i >= 0; i--) {
        const uint8_t oy = entries[i].y, ox = entries[i].x;
        /* The x byte wraps: past 208 it stands for the left of the window; the y byte past 224, for its top. */
        const int sx = ox >= 208u ? (int)ox - 8 - 256 : (int)ox - 8;
        const int sy = (oy >= 224u ? (int)oy - 256 : (int)oy) - 16 - (int)ORACLES_ENHANCED_HUD_HEIGHT;   /* game-area row of its top line */
        if (link_column(v, &entries[i]) >= 0) {
            link_shown = 1;
            if (!v->link_drawn || sx < v->link_drawn_x) v->link_drawn_x = sx;
            if (!v->link_drawn || sy < v->link_drawn_y) v->link_drawn_y = sy;
            v->link_drawn = 1;
        }
        if (sx >= 0 && sx + 8 <= (int)ORACLES_ENHANCED_CORE_WIDTH && sy >= 0 && sy + (int)height <= (int)ORACLES_ENHANCED_AREA_HEIGHT) continue;   /* wholly inside the window */
        /* The lines under the status bar: the bar's own sprites (the item
         * icons) live there, so only Link's sprite, walking up past the
         * room's top, is drawn into the world above the room. */
        const int link_entry = sy < 0 && link_column(v, &entries[i]) >= 0;
        draw_sprite_outside(v, sx, sy, entries[i].tile, entries[i].attr, height, link_entry ? ROWS_ALL : ROWS_BELOW_BAR, camera_x, camera_y,
                            (unsigned)i == rebuilt || i == left_line);
    }
    if (left_line >= 0) {
        if (on_line) v->link_left_line_frames++;
        /* In the water his layer takes that column too, as it takes a rebuilt one: his halves at one fraction. */
        if (link_in_water(v)) v->edge_link_made = 1;
    }
    /* Further past the screen's edge the game writes none of his sprites (an own sprite only above OAM y $a0 and left
     * of x $a8, object_sprites.h; the OAM on screen holds them hidden at 224), while a continuous transition takes him
     * on, up to 30 px past it (a column at x 0, his last on the left, is the hidden one): in a scroll, when the OAM on
     * screen has nothing of him on the screen nor past it, his sprites are built from his fields and the game's data
     * and drawn at his place, placed by the game's window as it stands less the one the frame on screen was drawn with,
     * both in the world (across a scroll's end the game's camera is the room entered's, the window drawn the room
     * left's), and moved back by his last step: his fields are a logic frame ahead of the frame on screen, which draws
     * him where the OAM would have him (link_step), as it draws his fraction of a pixel and the camera follows him.
     * His position is a byte, which wraps past the room's left or top edge (247 for -9) where the builder, made for
     * large rooms, unwraps only past 248: his place on the screen is taken from -64 to 191, the screen and 30 px around
     * it with his sprite's size.  So too at the frame that chains two scrolls, which counts as the room entered's: the
     * game draws it as a scroll's first, the OAM holding nothing of him, while the window already stands where he is
     * inside it, so that he is drawn there too, the core's image having none of him; and in play, past the window's
     * edge (the frame between two scrolls, where he turned back across the line during the first). */
    const uint8_t *link = oracles_guest_object(v->guest, 0, 0);
    const int32_t at_x = ob->world.link_x - ob->link_step_x - ob->window_left, at_y = ob->world.link_y - ob->link_step_y - ob->window_top;
    const int outside = at_x < 0 || at_x >= (int32_t)ORACLES_ENHANCED_CORE_WIDTH || at_y < 0 || at_y >= (int32_t)ORACLES_ENHANCED_AREA_HEIGHT;
    if ((ob->in_scroll || ob->scroll_chained || (!ob->in_transition && outside)) && ob->playing && !ob->cutscene && link && v->rom && height == 16u) {
        OraclesObjectSprite sprites[ORACLES_OBJECT_SPRITES_MAX];
        const int built = oracles_object_sprites_build(v->guest, link, v->rom, v->rom_size, sprites, ORACLES_OBJECT_SPRITES_MAX);
        int own = 0;
        for (int i = 0; i < built; i++) own += sprites[i].effect == ORACLES_SPRITE_OWN;
        if (own && !link_present && !link_shown) {
            const OraclesGuestTables *t = oracles_guest_tables(v->guest);
            const int chained = ob->scroll_chained;   /* the scroll not begun: the core's image has none of him in the window either */
            v->link_chained_pixels = 0;
            const int dx = ob->world.origin_x + (int16_t)oracles_guest_read16(v->guest, t->camera_x) - ob->window_left - ob->link_step_x;
            const int dy = ob->world.origin_y + (int16_t)oracles_guest_read16(v->guest, t->camera_y) - ob->window_top - ob->link_step_y;
            for (int i = built - 1; i >= 0; i--) {
                if (sprites[i].effect != ORACLES_SPRITE_OWN) continue;
                const int sx = ((sprites[i].x + 64) & 0xff) - 64, sy = ((sprites[i].y + 64) & 0xff) - 64;
                if (v->link_drawn != 2 || sx + dx < v->link_drawn_x) v->link_drawn_x = sx + dx;
                if (v->link_drawn != 2 || sy - (int)ORACLES_ENHANCED_HUD_HEIGHT + dy < v->link_drawn_y) v->link_drawn_y = sy - (int)ORACLES_ENHANCED_HUD_HEIGHT + dy;
                v->link_drawn = 2;
                draw_sprite_outside(v, sx + dx, sy - (int)ORACLES_ENHANCED_HUD_HEIGHT + dy, sprites[i].tile, sprites[i].attr, height, ROWS_ALL,
                                    camera_x, camera_y, chained ? 2 : 1);
            }
            link_shown = 1;
            v->edge_link_made = v->edge_link_built = 1;
            v->link_built_frames++;
            if (chained) { v->link_chained_frames++; v->link_chained_unshown += !v->link_chained_pixels; }
        }
        if (own && !link_shown) v->link_unshown_frames++;
    }
}

/* The blurb the game shows at the top of the screen on entering an area
 * (INTERAC_ERA_OR_SEASON_INFO, eraOrSeasonInfo.s: the season in Seasons,
 * the era in Ages).  It is an object of the room: y $0a, x sliding from $b0
 * to $10, then away to the left; with the room's camera at 0 its place in the
 * room is its place on the screen.  In the wide world the room is wherever
 * the camera puts it, and the blurb with it, off the band as often as not.
 * It is taken out of the core's window (its pixels repainted from the
 * software PPU without its sprites) and drawn at the top left of the band,
 * its slide in stretched so that it enters from the band's right edge
 * (deliberately, as the game slides it in from its own screen's right edge). */
static const uint8_t *area_blurb(OraclesEnhancedView *v)
{
    for (unsigned k = 0; k < 16u; k++) {
        const uint8_t *o = oracles_guest_object(v->guest, k, 1);
        if (o && o[ORACLES_OBJ_ENABLED] && o[ORACLES_OBJ_ID] == INTERAC_ERA_OR_SEASON_INFO) return o;
    }
    return NULL;
}

void ev_blurb_event(OraclesEnhancedView *v, const OraclesGuestEvent *event)
{
    /* drawAllSprites returning at once leaves wOam as it was (ev_large_objects_event). */
    if (event->type != ORACLES_EVENT_FRAME_DRAWN || !v->large_objects_drawn) return;
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const uint8_t *woam = oracles_guest_ptr(v->guest, t->oam, 160u);
    if (!woam) return;
    blurb_drawing *d = &v->blurb_drawings[v->blurb_drawings_head];
    v->blurb_drawings_head = (v->blurb_drawings_head + 1u) % BLURB_DRAWINGS;
    const uint8_t *blurb = area_blurb(v);
    memcpy(d->oam, woam, sizeof d->oam);
    d->valid = 1;
    d->there = blurb != NULL;
    if (blurb) { d->x = blurb[ORACLES_OBJ_XH]; d->y = blurb[ORACLES_OBJ_YH]; d->state = blurb[ORACLES_OBJ_STATE]; }
    d->camera_x = (int16_t)oracles_guest_read16(v->guest, t->camera_x);
    d->camera_y = (int16_t)oracles_guest_read16(v->guest, t->camera_y);
}

void ev_find_area_blurb(OraclesEnhancedView *v)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    const uint8_t *oam = oracles_guest_oam(v->guest);
    v->blurb_shown = 0;
    v->blurb_mask = 0;
    if (!oam) return;
    /* The OAM on screen was written by one of the game's last drawings: the
     * frame before's, where the slide stood 4 or 6 px further right, or an
     * older one through a room's load, which draws none while the camera
     * moves on.  The blurb as it stood then, placed by that drawing's camera;
     * on the frame after the game deletes it, at the end of its slide out,
     * that drawing still holds its last column.  Without one (a state just
     * loaded), the blurb as it is, by the camera of the frame on screen. */
    const blurb_drawing *drawn = NULL;
    for (unsigned k = 0; k < BLURB_DRAWINGS && !drawn; k++) {
        const blurb_drawing *d = &v->blurb_drawings[(v->blurb_drawings_head + BLURB_DRAWINGS - 1u - k) % BLURB_DRAWINGS];
        if (d->valid && memcmp(d->oam, oam, sizeof d->oam) == 0) drawn = d;
    }
    const uint8_t *blurb = drawn ? NULL : area_blurb(v);
    if (drawn ? !drawn->there : !blurb) return;
    int built_x = drawn ? drawn->x : blurb[ORACLES_OBJ_XH];
    const uint8_t built_y = drawn ? drawn->y : blurb[ORACLES_OBJ_YH], state = drawn ? drawn->state : blurb[ORACLES_OBJ_STATE];
    const int camera_x = drawn ? drawn->camera_x : ob->drawn_camera_x, camera_y = drawn ? drawn->camera_y : ob->drawn_camera_y;
    /* The slide out (state 3) runs the byte past 0 to the left; the slide in starts at $b0, to the right. */
    if (state == 3u && built_x >= 128) built_x -= 256;
    v->blurb_shown = 1;
    v->blurb_built_x = built_x;
    v->blurb_built_y = built_y;
    v->blurb_built_camera_x = camera_x;
    /* Its sprites: one 8x16 column every 8 px from OAM x = x (screen x - 8),
     * on the OAM line y + 24, less the camera the OAM was built with; a column
     * past the left edge is not written. */
    const unsigned oy = (unsigned)(built_y - camera_y + 24) & 0xffu;
    const int left = built_x - camera_x - 8;   /* the OAM x of its first column */
    for (unsigned i = 0; i < 40u; i++) {
        const uint8_t *e = oam + i * 4u;
        const int dx = (int)e[1] - left;
        if (e[0] == oy && e[1] != 0 && dx >= 0 && dx < 32 && dx % 8 == 0) v->blurb_mask |= 1ull << i;
    }
}

void ev_overlay_area_blurb(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    const uint8_t *oam = oracles_guest_oam(v->guest), *io = oracles_guest_io(v->guest), *palettes = oracles_guest_obj_palettes(v->guest);
    const uint8_t *vram0 = oracles_guest_vram(v->guest, 0), *vram1 = oracles_guest_vram(v->guest, 1);
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    const uint8_t *regs = oracles_guest_ptr(v->guest, t->gfx_regs3, 6);
    if (!v->blurb_shown || !v->blurb_mask || !oam || !io || !palettes || !vram0 || !vram1 || !regs || !(io[IO_LCDC] & 0x02u)) return;
    const int built_x = v->blurb_built_x;
    uint8_t without[160];
    memcpy(without, oam, sizeof without);
    edge_sprite entries[40];
    unsigned count = 0;
    for (unsigned i = 0; i < 40u; i++) {
        if (!(v->blurb_mask & (1ull << i))) continue;
        const uint8_t *e = oam + i * 4u;
        entries[count].y = e[0]; entries[count].x = e[1]; entries[count].tile = e[2]; entries[count].attr = e[3];
        count++;
        without[i * 4u] = 0;   /* y 0: hidden */
    }
    if (!count) return;
    const unsigned height = (io[IO_LCDC] & 0x04u) ? 16u : 8u;
    /* The window's pixels under it: the frame as the LCD drew it, without its sprites. */
    OraclesPpuInput in;
    in.vram = v->hybrid_vram;
    memcpy(v->hybrid_vram, vram0, 0x2000u);
    memcpy(v->hybrid_vram + 0x2000u, vram1, 0x2000u);
    in.oam = without;
    in.bg_palettes = oracles_guest_bg_palettes(v->guest);
    in.obj_palettes = palettes;
    in.colours = v->colours;
    const OraclesPpuRegs r = { (uint8_t)(regs[0] | 0x80u), (uint8_t)(ob->drawn_camera_y + ob->drawn_offset_y - (int)ORACLES_ENHANCED_HUD_HEIGHT),
                               (uint8_t)(ob->drawn_camera_x + ob->drawn_offset_x), regs[3], regs[4] };
    if (!in.bg_palettes) return;
    for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) in.lines[ly] = r;
    oracles_ppu_render(&in, v->popup_frame);
    for (unsigned n = 0; n < count; n++) {
        const int sx = (int)entries[n].x - 8, sy = (int)entries[n].y - 16 - (int)ORACLES_ENHANCED_HUD_HEIGHT;
        for (unsigned row = 0; row < height; row++)
            for (unsigned col = 0; col < 8u; col++) {
                const int gx = sx + (int)col, gy = sy + (int)row;
                if (gx < 0 || gx >= (int)ORACLES_ENHANCED_CORE_WIDTH || gy < 0 || gy >= (int)ORACLES_ENHANCED_AREA_HEIGHT) continue;
                const int32_t px = ob->window_left + gx - camera_x, py = ob->window_top + gy - camera_y;
                if (px < 0 || px >= (int32_t)v->size.width || py < 0 || py >= (int32_t)v->band_height) continue;
                *ev_band_pixel(v, (unsigned)px, (unsigned)py) =
                    v->popup_frame[(ORACLES_ENHANCED_HUD_HEIGHT + (unsigned)gy) * ORACLES_PPU_WIDTH + (unsigned)gx];
            }
    }
    /* Drawn at the top left of the band: the room's x at rest ($10) is the
     * band's; on the way in, beyond it, the distance is stretched from the
     * game's 160 px to the band's width, so that it enters from the band's edge. */
    const int x = built_x;
    const int band_x = x <= 16 ? x : 16 + (x - 16) * (int)v->size.width / (int)ORACLES_ENHANCED_CORE_WIDTH;
    const int shift = band_x - (x - v->blurb_built_camera_x);   /* band column minus screen column */
    for (unsigned n = 0; n < count; n++) {
        /* The line its room y gives on the LCD with the room's camera at 0: the top of the game area. */
        const int sx = (int)entries[n].x - 8 + shift, sy = (int)v->blurb_built_y + 24 - 16 - (int)ORACLES_ENHANCED_HUD_HEIGHT;
        ev_subpixel_fixed(v, sx, sy, 8, (int32_t)height);   /* it stands on the screen */
        const uint8_t tile = entries[n].tile, attr = entries[n].attr;
        const uint8_t *bank = (attr & 0x08u) ? vram1 : vram0;
        const uint8_t *palette = palettes + (attr & 0x07u) * 8u;
        for (unsigned row = 0; row < height; row++) {
            const int py = sy + (int)row;
            if (py < 0 || py >= (int)v->band_height) continue;
            const unsigned line = (attr & 0x40u) ? height - 1u - row : row;
            const unsigned tt = height == 16u ? ((tile & 0xfeu) + (line >> 3)) : tile;
            const uint8_t lo = bank[tt * 16u + (line & 7u) * 2u], hi = bank[tt * 16u + (line & 7u) * 2u + 1u];
            for (unsigned col = 0; col < 8u; col++) {
                const int px = sx + (int)col;
                if (px < 0 || px >= (int)v->size.width) continue;
                const unsigned bit = (attr & 0x20u) ? col : 7u - col;
                const unsigned colour = ((lo >> bit) & 1u) | (((hi >> bit) & 1u) << 1);
                if (!colour) continue;
                const uint16_t rgb = (uint16_t)(palette[colour * 2u] | (palette[colour * 2u + 1u] << 8));
                *ev_band_pixel(v, (unsigned)px, (unsigned)py) = v->colours[rgb & 0x7fffu];
            }
        }
    }
}

#define CURTAIN_SCROLL_MODE 0x02u        /* wScrollMode while the screen opens (screenTransitionState1) */
#define CURTAIN_TRANSITION_STATE 0x01u   /* wScreenTransitionState */
#define CURTAIN_SUBSTATE 0x02u           /* wScreenTransitionState2: the substate that draws a column a frame */
#define CURTAIN_COLUMNS 32u              /* the columns of the game's map, the width of the world band */
#define CURTAIN_SAMPLES 8u               /* lines sampled in a column of the window to read the curtain's colour */

/* ---- the curtain of a warp ------------------------------------------------------------------- */

/* Coming out of a house, out of a cave, back from a portal, the game does not
 * show the room at once: in state 1 of its screen transition it redraws the
 * thirty-two columns of its map outward from the centre of its window, one
 * per frame and one side at a time, and a column not yet drawn shows the
 * blank tile.  Only twenty of those columns are on the core's screen, so the
 * curtain stopped at the window's edges and the band around it stayed drawn
 * in full.  The view paints the columns the game has not
 * drawn yet over the whole band, in the colour the blank tile has on screen:
 * the curtain then opens across the band, and ends with the game's own.
 *
 * `wScreenScrollRow` holds the next column to draw on the left, its
 * counterpart `wScreenScrollDirection` the next one on the right (the two
 * variables of a scrolling transition, used here as the two edges); the
 * columns still to draw run from the right one forward to the left one. */
static int curtain_open_range(OraclesEnhancedView *v, uint8_t scx, int *left, int *right)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    if (oracles_guest_read8(v->guest, t->scroll_mode) != CURTAIN_SCROLL_MODE
        || oracles_guest_read8(v->guest, t->screen_transition_state) != CURTAIN_TRANSITION_STATE
        || oracles_guest_read8(v->guest, t->screen_transition_substate) != CURTAIN_SUBSTATE) return 0;
    /* The game starts from the column of the middle of its window and walks
     * outward, so the columns drawn run from the one left of the middle to
     * the one the right edge has reached; counted in columns rather than
     * read off the map, the two edges never wrap round it. */
    const unsigned next_left = oracles_guest_read8(v->guest, t->screen_scroll_row) & (CURTAIN_COLUMNS - 1u);
    const unsigned next_right = oracles_guest_read8(v->guest, t->screen_scroll_column_right) & (CURTAIN_COLUMNS - 1u);
    const unsigned middle = (((unsigned)scx + ORACLES_ENHANCED_CORE_WIDTH / 2u) >> 3) & (CURTAIN_COLUMNS - 1u);
    const unsigned drawn_left = ((middle - 1u - next_left) & (CURTAIN_COLUMNS - 1u));
    const unsigned drawn_right = ((next_right - middle) & (CURTAIN_COLUMNS - 1u));
    const int middle_px = (int)(((middle * 8u) - scx) & 0xffu);   /* the middle column's left edge, in the window */
    *left = middle_px - (int)drawn_left * 8;
    *right = middle_px + (int)drawn_right * 8;
    return 1;
}

/* The curtain's colour, taken from the core's own image rather than computed:
 * a column of the window the game has not drawn yet is the curtain, and the
 * colour that covers most of it is the blank tile's (a sprite may cross it).
 * Once the window is open the band still has columns to draw: the colour of
 * the frames before is kept for them. */
static uint32_t curtain_colour(OraclesEnhancedView *v, int left, int right, uint8_t scx)
{
    const uint32_t *core = oracles_core_pixels(v->core);
    (void)scx;
    for (unsigned gx = 4; core && gx < ORACLES_ENHANCED_CORE_WIDTH; gx += 8) {
        if ((int)gx >= left && (int)gx < right) continue;
        uint32_t seen[CURTAIN_SAMPLES]; unsigned times[CURTAIN_SAMPLES], kinds = 0;
        for (unsigned i = 0; i < CURTAIN_SAMPLES; i++) {
            const unsigned y = ORACLES_ENHANCED_HUD_HEIGHT + (i * ORACLES_ENHANCED_AREA_HEIGHT) / CURTAIN_SAMPLES;
            const uint32_t pixel = core[y * ORACLES_PPU_WIDTH + gx];
            unsigned k = 0;
            while (k < kinds && seen[k] != pixel) k++;
            if (k == kinds) { seen[kinds] = pixel; times[kinds++] = 1; } else times[k]++;
        }
        unsigned best = 0;
        for (unsigned k = 1; k < kinds; k++) if (times[k] > times[best]) best = k;
        v->curtain_colour = seen[best];
        v->curtain_colour_valid = 1;
        return v->curtain_colour;
    }
    return v->curtain_colour_valid ? v->curtain_colour : 0xff000000u;
}

void ev_overlay_warp_curtain(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    const uint8_t scx = (uint8_t)(ob->drawn_camera_x + ob->drawn_offset_x);
    int left = 0, right = 0;
    if (!curtain_open_range(v, scx, &left, &right)) return;
    v->curtain_frames++;
    const uint32_t colour = curtain_colour(v, left, right, scx);
    for (unsigned x = 0; x < v->size.width; x++) {
        const int window_x = (int)(camera_x + (int32_t)x - ob->window_left);
        const int inside_x = window_x >= 0 && window_x < (int)ORACLES_ENHANCED_CORE_WIDTH;
        if (inside_x && window_x >= left && window_x < right) continue;               /* drawn, and the core's */
        for (unsigned y = 0; y < v->band_height; y++) {
            /* The core draws its own window; the band around it, the rows
             * above and below it that the vertical camera shows included. */
            const int window_y = (int)(camera_y + (int32_t)y - ob->window_top);
            if (inside_x && window_y >= 0 && window_y < (int)ORACLES_ENHANCED_AREA_HEIGHT) continue;
            if (window_x >= left && window_x < right) continue;                       /* already drawn */
            *ev_band_pixel(v, x, y) = colour;
        }
    }
}

/* ---- the objects of a large room beyond the window ---------------------------- */

void ev_large_objects_event(OraclesEnhancedView *v, const OraclesGuestEvent *event)
{
    const OraclesGuestTables *t = oracles_guest_tables(v->guest);
    if (event->type == ORACLES_EVENT_ANIMATIONS) { v->animation_steps++; return; }
    if (event->type == ORACLES_EVENT_FRAME_DONE) { v->large_objects_first = oracles_guest_read8(v->guest, t->oam_tail) / 4u; v->large_objects_drawn = 0; return; }
    if (event->type == ORACLES_EVENT_OBJECT_DRAW) { v->large_objects_drawn++; return; }
    if (event->type != ORACLES_EVENT_FRAME_DRAWN) return;
    /* drawAllSprites returns at once when wc4b6 says so, the OAM as it was:
     * a drawing rebuilt now would not be the one on screen (a shadow's
     * parity has turned meanwhile).  Nothing is kept, as the harness judges
     * nothing there. */
    if (!v->large_objects_drawn) return;
    const uint8_t *oam = oracles_guest_ptr(v->guest, t->oam, 160u);
    if (!oam || !oracles_guest_read8(v->guest, t->room_is_large) || oracles_guest_read8(v->guest, t->scroll_mode) == ORACLES_SCROLL_MODE_TRANSITION) return;   /* the game then places objects by another routine: nothing kept */
    /* The return of drawAllSprites: the objects stand where the game drew
     * them, the camera with them. */
    OraclesDrawnObject objects[64];
    unsigned count = 0;
    large_object_frame *f = &v->large_objects[v->large_objects_head];
    v->large_objects_head = (v->large_objects_head + 1u) % LARGE_OBJECT_FRAMES;
    f->valid = oracles_object_sprites_all(v->guest, v->rom, v->rom_size, v->large_objects_first, objects, 64u, f->sprites, LARGE_OBJECT_SPRITES, &count) >= 0;
    f->count = f->valid ? count : 0;
    f->entries = 0;
    for (unsigned i = 0; i < f->count; i++) if (f->sprites[i].written) f->entries |= 1ull << f->sprites[i].oam_index;
    f->group = oracles_guest_read8(v->guest, t->active_group);
    f->room = oracles_guest_read8(v->guest, t->active_room);
    memcpy(f->oam, oam, sizeof f->oam);
}

/* In a large room, every object's sprites outside the game's window, from
 * the drawing whose wOam is the OAM on screen, so that they move with the
 * ones the core shows: those the game did not write because they fall
 * beyond its window (an enemy of a wide dungeon room, Farore's book in the
 * Maku Tree), the parts beyond the window of those it wrote, the lines under
 * the status bar, the grass, puddles and shadows, at their true place (the
 * OAM's wrapped bytes would put an effect beyond the window elsewhere), in the
 * game's order, the first on top.  The edge overlay leaves them the entries
 * they account for. */
void ev_overlay_large_objects(OraclesEnhancedView *v, int32_t camera_x, int32_t camera_y)
{
    const OraclesEnhancedObservation *ob = &v->observation;
    const uint8_t *oam = oracles_guest_oam(v->guest), *io = oracles_guest_io(v->guest);
    v->large_objects_entries = 0;
    /* A warp's fade and the frames around it are no transition for the
     * objects: the game places them as in play, the drawings kept follow
     * them.  Only the scroll between two rooms places them otherwise, and
     * no drawing of it is kept. */
    if (!ob->large || ob->in_scroll || !oam || !io || !(io[IO_LCDC] & 0x02u) || !v->rom) return;
    const unsigned height = (io[IO_LCDC] & 0x04u) ? 16u : 8u;
    const large_object_frame *f = NULL;
    for (unsigned k = 0; k < LARGE_OBJECT_FRAMES && !f; k++) {
        const unsigned at = (v->large_objects_head + LARGE_OBJECT_FRAMES - 1u - k) % LARGE_OBJECT_FRAMES;
        const large_object_frame *g = &v->large_objects[at];
        /* A drawing of another room is not this one's, whatever its OAM. */
        if (g->valid && g->group == ob->group && g->room == ob->room && memcmp(g->oam, oam, 160u) == 0) f = g;
    }
    if (!f) { v->large_object_frames_unmatched++; return; }
    v->large_objects_entries = f->entries;
    for (int i = (int)f->count - 1; i >= 0; i--) {
        const OraclesObjectSprite *s = &f->sprites[i];
        draw_sprite_outside(v, s->x, s->y - (int)ORACLES_ENHANCED_HUD_HEIGHT, s->tile, s->attr, height, ROWS_ALL, camera_x, camera_y, 0);
        if (!s->written) v->large_object_sprites_drawn++;
    }
}
