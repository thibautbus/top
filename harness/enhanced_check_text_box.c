/* The Enhanced check: the text box drawn by the host (--text-box-check) against the core's image.
 *
 * Each frame the view draws the game's box itself (oracles_enhanced_view_host_text_box), at the game's width and lines,
 * from its model of the box and the host's lines; nothing of it is shown.  The box is then laid on the core's image as
 * the core scanned the map: each line of the screen showing the box's map takes the scroll registers it was drawn with
 * (the register journal: the vblank's writes, then the scan's, a write in mode 0 landing on the next line), and each of
 * its pixels in the box's cells is compared with the core's.  A pixel the window covers, or that an object may cover,
 * is left out: the box is the background's.  In the world band the view copies those very pixels of the core into the
 * band (ev_overlay_text_box), and in the framed mode it shows the core's image: the comparison holds for both. */
#include "enhanced_check_internal.h"

#include "ppu.h"
#include "text_box.h"

#define IO_LCDC 0x40u
#define IO_SCY 0x42u
#define IO_SCX 0x43u
#define IO_WY 0x4au
#define IO_WX 0x4bu
#define SCREEN_W 160u
#define SCREEN_H 144u
#define MAP_CELLS 32u

/* The registers each line of the frame just scanned was drawn with. */
static void line_registers(OraclesEnhancedCheck *c, OraclesPpuRegs regs[SCREEN_H])
{
    const uint8_t *io = oracles_guest_io(c->guest);
    OraclesPpuRegs r = { io[IO_LCDC], io[IO_SCY], io[IO_SCX], io[IO_WY], io[IO_WX] };
    size_t count = 0;
    const OraclesGuestRegWrite *j = oracles_guest_journal(c->guest, &count);
    for (size_t i = 0; i < count; i++) {
        if (j[i].ly < SCREEN_H) continue;
        if (j[i].reg == IO_LCDC) r.lcdc = j[i].value;
        else if (j[i].reg == IO_SCY) r.scy = j[i].value;
        else if (j[i].reg == IO_SCX) r.scx = j[i].value;
        else if (j[i].reg == IO_WY) r.wy = j[i].value;
        else if (j[i].reg == IO_WX) r.wx = j[i].value;
    }
    for (unsigned ly = 0; ly < SCREEN_H; ly++) {
        for (size_t i = 0; i < count; i++) {
            if (j[i].ly >= SCREEN_H || (j[i].stat_mode == 0 ? (unsigned)j[i].ly + 1u : j[i].ly) != ly) continue;
            if (j[i].reg == IO_LCDC) r.lcdc = j[i].value;
            else if (j[i].reg == IO_SCY) r.scy = j[i].value;
            else if (j[i].reg == IO_SCX) r.scx = j[i].value;
            else if (j[i].reg == IO_WY) r.wy = j[i].value;
            else if (j[i].reg == IO_WX) r.wx = j[i].value;
        }
        regs[ly] = r;
    }
}

/* Whether an object of the OAM the frame scanned has a pixel of colour at (x, ly). */
static int object_at(const uint8_t *oam, const uint8_t *vram, uint8_t lcdc, unsigned x, unsigned ly)
{
    if (!(lcdc & 0x02u)) return 0;
    const int height = lcdc & 0x04u ? 16 : 8;
    for (unsigned i = 0; i < 40u; i++) {
        const int y = (int)oam[4u * i] - 16, left = (int)oam[4u * i + 1u] - 8;
        if ((int)ly < y || (int)ly >= y + height || (int)x < left || (int)x >= left + 8) continue;
        const uint8_t attribute = oam[4u * i + 3u];
        const unsigned tile = height == 16 ? oam[4u * i + 2u] & 0xfeu : oam[4u * i + 2u];
        const unsigned row = attribute & 0x40u ? (unsigned)(height - 1 - ((int)ly - y)) : (unsigned)((int)ly - y);
        const unsigned column = attribute & 0x20u ? 7u - (unsigned)((int)x - left) : (unsigned)((int)x - left);
        const uint8_t *data = vram + (attribute & 0x08u ? 0x2000u : 0u) + 16u * tile + 2u * row;
        if (((data[0] | data[1]) >> (7u - column)) & 1u) return 1;
    }
    return 0;
}

/* Whether character c's glyph has ink at its cell's column x and row y, in the font of the image (a byte a row, bit 7 the
 * left column, a clear bit ink). */
static int glyph_ink_at(const uint8_t *font, uint8_t c, unsigned x, unsigned y) { return x < 8u && y < 16u && !(font[16u * c + y] & (0x80u >> x)); }

static void note(char *list, size_t capacity, uint32_t frame, unsigned value)
{
    const size_t used = strlen(list);
    if (used + 24u < capacity) snprintf(list + used, capacity - used, "%s%u:%u", used ? " " : "", frame, value);
}

/* A page of the box drawn by the host whole at this frame: the game waiting for a button (state 5) or its text ended
 * ($0f), entered since the last frame drawn; noted "frame:box". */
static int page_whole(OraclesEnhancedCheck *c, uint32_t frame, unsigned box)
{
    const unsigned state = oracles_guest_read8(c->guest, oracles_guest_tables(c->guest)->text_display_state);
    const int whole = (state == 5u || state == 0x0fu) && c->page_state != state;
    c->page_state = state;
    if (whole) note(c->page_list, sizeof c->page_list, frame, box);
    return whole;
}

void ec_check_host_text_box(OraclesEnhancedCheck *c, OraclesEnhancedMode mode, uint32_t frame)
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    OraclesEnhancedHostTextBox box;
    const double start = ec_monotonic_us();
    const int drawn = oracles_enhanced_view_host_text_box(c->view, ORACLES_TEXT_BOX_GAME_PIXELS, ORACLES_TEXT_BOX_GAME_LINES, &box);
    const double spent = ec_monotonic_us() - start;
    if (drawn < 0) {
        c->host_box_uncovered++;
        if (box.read == ORACLES_TEXT_BOX_LINE_UNKNOWN) c->host_box_unknown++;
        else c->host_box_unexpected++;
        note(c->host_box_uncovered_list, sizeof c->host_box_uncovered_list, frame, (unsigned)box.read);
        return;
    }
    if (!drawn) {
        if (oracles_guest_read8(c->guest, t->text_is_active) && oracles_guest_read8(c->guest, t->text_display_mode) <= 1u) {
            c->host_box_absent++;
            c->host_box_absent_run = c->host_box_absent_run && c->host_box_absent_last + 1u == frame ? c->host_box_absent_run + 1u : 1u;
            c->host_box_absent_last = frame;
            if (c->host_box_absent_run > c->host_box_absent_longest) c->host_box_absent_longest = c->host_box_absent_run;
        }
        return;
    }
    if (!c->host_box_drawn || c->host_box_last_drawn + 1u != frame) {
        const size_t used = strlen(c->host_box_runs_list);
        if (!c->host_box_drawn) snprintf(c->host_box_runs_list, sizeof c->host_box_runs_list, "%u", frame);
        else if (used + 24u < sizeof c->host_box_runs_list) snprintf(c->host_box_runs_list + used, sizeof c->host_box_runs_list - used, "-%u %u", c->host_box_last_drawn, frame);
    }
    if (!c->host_box_drawn || c->host_box_last_drawn + 1u != frame) c->wide_boxes++;   /* the boxes, for the pages' notes */
    c->host_box_last_drawn = frame;
    c->host_box_drawn++;
    page_whole(c, frame, c->wide_boxes);
    c->host_box_us_total += spent;
    if (spent > c->host_box_us_max) c->host_box_us_max = spent;
    OraclesPpuRegs regs[SCREEN_H];
    line_registers(c, regs);
    const uint32_t *core = oracles_core_pixels(c->core);
    const uint8_t *oam = oracles_guest_oam(c->guest), *vram = oracles_guest_vram(c->guest, 0);
    if (!core || !oam || !vram) return;
    unsigned compared = 0, wrong = 0, skipped = 0;
    for (unsigned ly = 0; ly < SCREEN_H; ly++) {
        const OraclesPpuRegs *r = &regs[ly];
        if (!(r->lcdc & 0x80u) || ((r->lcdc & 0x08u) ? 0x1c00u : 0x1800u) != box.map) continue;
        const unsigned ym = (ly + r->scy) & 0xffu, row = ((ym >> 3) - box.row) & (MAP_CELLS - 1u);
        if (row >= ORACLES_TEXT_BOX_GAME_ROWS) continue;
        for (unsigned x = 0; x < SCREEN_W; x++) {
            const unsigned xm = (x + r->scx) & 0xffu, column = ((xm >> 3) - box.column) & (MAP_CELLS - 1u);
            if (column >= ORACLES_TEXT_BOX_GAME_COLUMNS) continue;
            const int differs = box.pixels[(row * 8u + (ym & 7u)) * box.width + column * 8u + (xm & 7u)] != core[ly * SCREEN_W + x];
            if ((r->lcdc & 0x20u) && r->wy <= ly && r->wx <= 166u && x + 7u >= r->wx) { skipped++; continue; }
            if (object_at(oam, vram, r->lcdc, x, ly)) { skipped++; c->host_box_object_pixels += (uint64_t)differs; continue; }
            compared++;
            wrong += (unsigned)differs;
        }
    }
    c->host_box_pixels_skipped += skipped;
    {   /* The largest box planned, Wide (192 pixels) with three lines, from the same model: its cost alone. */
        OraclesEnhancedHostTextBox wide;
        const double wide_start = ec_monotonic_us();
        if (oracles_enhanced_view_host_text_box_again(c->view, 192u, 3u, &wide)) c->host_box_wide_us_total += ec_monotonic_us() - wide_start;
    }
    if (!compared) return;
    c->host_box_frames++;
    if (mode == ORACLES_ENHANCED_WORLD) c->host_box_frames_world++;
    else c->host_box_frames_framed++;
    c->host_box_pixels += compared;
    if (wrong) {
        c->host_box_frames_wrong++;
        c->host_box_pixels_wrong += wrong;
        note(c->host_box_list, sizeof c->host_box_list, frame, wrong);
    }
}

/* ---- the host's wide box, against its layout ------------------------------------------------------------------------
 *
 * There is no image of the core to compare a box wider than the game's with: its text is judged against the layout it
 * was composed from.  Each frame the view drew the host's box, each text row of it, where the surface shows it: its
 * line's characters, laid out at their pixels (their start, their cell's first column drawn and how many), their glyphs
 * read in the font of the image the core runs (a clear bit ink), up to the reveal's pixel, are the ink expected; a
 * pixel of the surface is ink when it is not its tile's palette's colour 3, the box's blank.  A symbol's tile (its own
 * graphics, not the font's) and a choice's cursor are left out and counted; over the framed core, the window's pixels
 * are left to it.  The reveal is judged on the surface too: a character with some of its ink shown but not all is cut,
 * one of a step the game has not shown yet (w7NextTextColumnToDisplay, as the frame showed it) early. */

static const uint8_t *image_font(OraclesEnhancedCheck *c)
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    size_t size = 0;
    uint16_t bank = 0;
    const uint8_t *rom = oracles_core_memory(c->core, ORACLES_CORE_ROM, &size, &bank);
    const size_t at = oracles_rom_offset(t->font_start.bank, t->font_start.addr);
    return rom && at + 16u * 0x100u <= size ? rom + at : NULL;
}

static uint32_t box_colour(const OraclesTextBoxSource *s, unsigned palette, unsigned index)
{
    const unsigned at = (palette & 7u) * 8u + index * 2u;
    const uint16_t colour = (uint16_t)((s->bg_palettes[at] | s->bg_palettes[at + 1u] << 8) & 0x7fffu);
    return s->colours ? s->colours[colour] : oracles_ppu_rgb555(colour);
}

/* The pixels of a cursor's tile drawn at (x, y) in the surface that are not the game's tile in its palette. */
static unsigned cursor_wrong(const OraclesEnhancedCheck *c, const OraclesTextBoxSource *s, const OraclesTextBoxMark *m, int32_t x, int32_t y, unsigned k,
                             const uint32_t *surface, unsigned height)
{
    const unsigned bank = m->attribute & 0x08u ? 0x2000u : 0u;
    const unsigned at = s->signed_tiles ? (m->tile < 0x80u ? 0x1000u + 16u * m->tile : 0x800u + 16u * (m->tile - 0x80u)) : 16u * m->tile;
    const uint8_t *data = s->vram + bank + at;
    unsigned wrong = 0;
    for (unsigned ty = 0; ty < 8u; ty++)
        for (unsigned tx = 0; tx < 8u; tx++) {
            const int32_t sx = x + (int32_t)(k * tx), sy = y + (int32_t)(k * ty);
            if (sx < 0 || sx >= (int32_t)c->width || sy < 0 || sy >= (int32_t)height) continue;
            const unsigned row = m->attribute & 0x40u ? 7u - ty : ty, bit = m->attribute & 0x20u ? tx : 7u - tx;
            const unsigned index = ((data[2u * row] >> bit) & 1u) | ((data[2u * row + 1u] >> bit) & 1u) << 1;
            wrong += surface[(size_t)sy * c->width + (size_t)sx] != box_colour(s, m->attribute & 7u, index);
        }
    return wrong;
}

/* A choice's cursor places, where the fan games' engine draws the cursor in the line's tiles (and the space that takes
 * it off): each place's cell as the host's box shows it, at its column moved with a line centred, against the game's
 * cell in the VRAM ($9000 + 16 t, t the place's top tile), its two tiles in the line's attribute; a place where the box
 * draws a cursor from the map (the US engine's) is left to cursor_wrong.  The pixels that differ. */
static unsigned cursor_cells_wrong(OraclesEnhancedCheck *c, const OraclesTextBoxModel *model, const OraclesTextBoxSource *s, int32_t box_x, int32_t box_y,
                                   unsigned zoom, const uint32_t *surface, unsigned height)
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    if (oracles_guest_read8(c->guest, t->text_display_mode) != 1u) return 0;
    const uint8_t *places = oracles_guest_ptr(c->guest, t->textbox_option_positions, 8u);
    if (!places || !s->vram) return 0;
    unsigned wrong = 0, compared = 0;
    for (unsigned r = 0; r < 2u * ORACLES_TEXT_BOX_GAME_LINES; r++) {
        const OraclesTextBoxRow *row = &model->row[r];
        const OraclesGuestTextBoxLine *line = row->line ? s->lines[row->line] : NULL;
        if (!line) continue;
        const unsigned base = row->line == ORACLES_GUEST_TEXT_BOX_BOTTOM ? 0x60u : 0x40u;
        for (unsigned i = 0; i < 8u && places[i]; i++) {
            const unsigned tile = places[i] - 1u, column = (tile - base) / 2u + line->offset / 8u;
            if (tile < base || tile >= base + 32u || column >= line->columns) continue;
            int marked = 0;
            for (unsigned m = 0; m < model->marks; m++)
                marked |= model->mark[m].anchor == ORACLES_TEXT_BOX_TEXT && model->mark[m].row == r && model->mark[m].column + line->offset / 8u == column;
            if (marked) continue;
            const uint8_t *data = s->vram + 0x1000u + 16u * (tile + row->half);
            for (unsigned y = 0; y < 8u; y++)
                for (unsigned x = 0; x < 8u; x++) {
                    const int32_t sx = box_x + (int32_t)(zoom * (8u + 8u * column + x)), sy = box_y + (int32_t)(zoom * (8u * r + y));
                    if (sx < 0 || sx >= (int32_t)c->width || sy < 0 || sy >= (int32_t)height) continue;
                    const unsigned index = ((data[2u * y] >> (7u - x)) & 1u) | ((data[2u * y + 1u] >> (7u - x)) & 1u) << 1;
                    wrong += surface[(size_t)sy * c->width + (size_t)sx] != box_colour(s, line->attribute[column], index);
                    compared++;
                }
        }
    }
    if (compared) c->wide_cursor_cells++;
    return wrong;
}

static void note_reveal(OraclesEnhancedCheck *c, uint32_t frame, char kind)
{
    const size_t used = strlen(c->wide_reveal_list);
    if (used + 16u < sizeof c->wide_reveal_list) snprintf(c->wide_reveal_list + used, sizeof c->wide_reveal_list - used, "%s%u:%c", used ? " " : "", frame, kind);
}

void ec_check_wide_text_box(OraclesEnhancedCheck *c, OraclesEnhancedMode mode, const uint32_t *surface, uint32_t frame)
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    const int active = oracles_guest_read8(c->guest, t->text_is_active) && oracles_guest_read8(c->guest, t->text_display_mode) <= 1u;
    const unsigned state = oracles_guest_read8(c->guest, t->text_display_state);
    OraclesEnhancedHostTextBox box;
    int32_t box_x = 0, box_y = 0;
    if (!surface || !oracles_enhanced_view_text_box_shown(c->view, &box, &box_x, &box_y)) {
        if (active) {
            c->wide_absent++;
            c->wide_absent_run = c->wide_absent_run && c->wide_absent_last + 1u == frame ? c->wide_absent_run + 1u : 1u;
            c->wide_absent_last = frame;
            if (c->wide_absent_run > c->wide_absent_longest) c->wide_absent_longest = c->wide_absent_run;
        }
        c->wide_state = state;
        return;
    }
    const unsigned zoom = oracles_enhanced_view_text_box_shown_scale(c->view);   /* the box's scale: Text size */
    if (!c->wide_frames || c->wide_last_drawn + 1u != frame) c->wide_boxes++;
    c->wide_last_drawn = frame;
    c->wide_frames++;
    if (mode == ORACLES_ENHANCED_WORLD) c->wide_frames_world++;
    else c->wide_frames_framed++;
    if (page_whole(c, frame, c->wide_boxes)) c->wide_pages++;
    c->wide_state = state;
    {   /* Over the framed core, the large menus' rectangle (the union of the frame and the box) holds the box whole, at its
         * size as the large menus show it. */
        uint32_t crop[4];
        if (mode != ORACLES_ENHANCED_WORLD && zoom == 1u && oracles_enhanced_view_framed_crop(c->view, crop)) {
            c->wide_crop_frames++;
            if (box_x < (int32_t)crop[0] || box_x + (int32_t)(zoom * box.width) > (int32_t)(crop[0] + crop[2]) || box_y < (int32_t)crop[1]
                || box_y + (int32_t)(zoom * box.height) > (int32_t)(crop[1] + crop[3])) c->wide_crop_outside++;
        }
    }
    {   /* The drawing's cost: the box drawn again from its model. */
        OraclesEnhancedHostTextBox again;
        const double start = ec_monotonic_us();
        oracles_enhanced_view_host_text_box_again(c->view, c->wide_pixels_width, c->wide_lines, &again);
        const double spent = ec_monotonic_us() - start;
        c->wide_us_total += spent;
        if (spent > c->wide_us_max) c->wide_us_max = spent;
    }
    const OraclesTextBoxSource *source = NULL;
    const OraclesTextBoxModel *model = oracles_enhanced_view_text_box_model(c->view, &source);
    const uint8_t *font = image_font(c);
    if (!model || !source || !source->bg_palettes || !font) return;
    OraclesGuestTextBoxReveal reveal;
    oracles_guest_text_box_reveal_state(c->guest, &reveal);
    if (reveal.line) c->wide_reveal_frames++;
    OraclesPpuRegs regs[SCREEN_H];
    if (mode != ORACLES_ENHANCED_WORLD) line_registers(c, regs);
    const unsigned height = oracles_enhanced_view_height(c->view);
    const OraclesEnhancedSize size = { c->width, height };
    const int32_t frame_x = (int32_t)oracles_enhanced_hud_x(size), frame_y = (int32_t)(height - SCREEN_H) / 2;
    for (unsigned i = 0; i < model->marks; i++) if (model->mark[i].anchor == ORACLES_TEXT_BOX_TEXT) { c->wide_cursor_frames++; break; }
    /* A choice's cursor where the game puts it in its line, moved with the line when it is centred: its tile's pixels
     * in the surface. */
    for (unsigned i = 0; i < model->marks; i++) {
        const OraclesTextBoxMark *m = &model->mark[i];
        if (m->anchor != ORACLES_TEXT_BOX_TEXT || m->row >= 2u * ORACLES_TEXT_BOX_GAME_LINES) continue;
        const OraclesTextBoxRow *own = &model->row[m->row], *pair = &model->row[m->row ^ 1u];
        const unsigned which = own->line ? own->line : pair->line;
        const unsigned offset = which && source->lines[which] ? source->lines[which]->offset : 0u;
        if (offset) c->wide_cursor_centred++;
        c->wide_cursor_pixels_wrong += cursor_wrong(c, source, m, box_x + (int32_t)(zoom * (8u + 8u * m->column + offset)), box_y + (int32_t)(zoom * 8u * m->row), zoom, surface, height);
    }
    if (mode == ORACLES_ENHANCED_WORLD || zoom > 1u) c->wide_cursor_pixels_wrong += cursor_cells_wrong(c, model, source, box_x, box_y, zoom, surface, height);
    {   /* The lines shown centred, a page the game laid out. */
        int centred = 0;
        for (unsigned r = 0; r < 2u * ORACLES_TEXT_BOX_GAME_LINES; r++)
            centred |= model->row[r].line && source->lines[model->row[r].line] && source->lines[model->row[r].line]->offset;
        c->wide_centred_frames += (unsigned)centred;
    }
    uint64_t compared = 0, wrong = 0, skipped = 0;
    int cut = 0, early = 0;
    for (unsigned r = 0; r < 2u * ORACLES_TEXT_BOX_GAME_LINES; r++) {
        const OraclesTextBoxRow *row = &model->row[r];
        const OraclesGuestTextBoxLine *line = row->line ? source->lines[row->line] : NULL;
        if (!line) continue;
        unsigned ink_total[ORACLES_GUEST_LINE_CHARACTERS] = { 0 }, ink_shown[ORACLES_GUEST_LINE_CHARACTERS] = { 0 };
        for (unsigned x = 0; x < 8u * line->columns && x + 16u <= box.width; x++) {
            const unsigned j = x / 8u;
            int marked = 0;
            for (unsigned i = 0; i < model->marks; i++)
                marked |= model->mark[i].anchor == ORACLES_TEXT_BOX_TEXT && model->mark[i].row == r && model->mark[i].column + line->offset / 8u == j;
            /* The character drawn there, from the layout: a whole one takes its tile. */
            int k = -1, whole = 0;
            for (unsigned n = 0; n < line->count && k < 0; n++) {
                const OraclesGuestTextBoxCharacter *ch = &line->character[n];
                const unsigned from = ch->start, to = ch->start + (ch->whole ? 8u : ch->drawn);
                if (x >= from && x < to && (ch->whole || ch->drawn)) { k = (int)n; whole = ch->whole || ch->character < ORACLES_TEXT_FIRST_CHARACTER; }
            }
            if (whole && x % 8u == 0 && !row->half && x < row->pixels) {
                c->wide_symbol_tiles++;
                if (k >= 0 && line->character[k].character < ORACLES_TEXT_FIRST_CHARACTER && !strstr(c->symbol_list, " ")) note(c->symbol_list, sizeof c->symbol_list, frame, (unsigned)k);
            }
            const uint32_t blank = box_colour(source, line->attribute[j], 3u);
            for (unsigned y = 0; y < 8u; y++) {
                const int32_t sx = box_x + (int32_t)(zoom * (8u + x)), sy = box_y + (int32_t)(zoom * (r * 8u + y));   /* a pixel's block's first, at the box's scale */
                const unsigned glyph_row = row->half * 8u + y;
                const OraclesGuestTextBoxCharacter *ch = k >= 0 ? &line->character[k] : NULL;
                const int expected_ink = ch && !whole && glyph_ink_at(font, ch->character, ch->from + (x - ch->start), glyph_row);
                if (ch && !whole && expected_ink) ink_total[k]++;
                if (marked || whole || sx < 0 || sx >= (int32_t)c->width || sy < 0 || sy >= (int32_t)height) { skipped++; continue; }
                if (mode != ORACLES_ENHANCED_WORLD && zoom == 1u) {
                    const int32_t ly = sy - frame_y, fx = sx - frame_x;
                    const OraclesPpuRegs *lr = ly >= 0 && ly < (int32_t)SCREEN_H ? &regs[ly] : NULL;
                    if (lr && (lr->lcdc & 0x20u) && lr->wy <= ly && lr->wx <= 166u && fx >= 0 && fx < (int32_t)SCREEN_W && fx + 7 >= (int32_t)lr->wx) { skipped++; continue; }
                }
                const int drawn_ink = surface[(size_t)sy * c->width + (size_t)sx] != blank;
                if (ch && !whole && expected_ink && drawn_ink) ink_shown[k]++;
                compared++;
                wrong += (uint64_t)(drawn_ink != (expected_ink && x < row->pixels));
            }
        }
        for (unsigned n = 0; n < line->count; n++) {
            if (!ink_total[n] || !ink_shown[n]) continue;
            if (ink_shown[n] < ink_total[n]) cut = 1;
            if (row->line == reveal.line) {
                unsigned s = 0;
                while (s + 1u < line->steps && line->first[s + 1u] <= n) s++;
                if (s >= reveal.shown) early = 1;
            }
        }
    }
    c->wide_pixels += compared;
    c->wide_pixels_skipped += skipped;
    if (cut) { c->wide_cut_frames++; note_reveal(c, frame, 'c'); }
    if (early) { c->wide_early_frames++; note_reveal(c, frame, 'e'); }
    if (wrong) {
        c->wide_frames_wrong++;
        c->wide_pixels_wrong += wrong;
        note(c->wide_list, sizeof c->wide_list, frame, (unsigned)wrong);
    }
}

/* Text size Large: the box drawn twice as large judged against the box it enlarges, each pixel of the surface's 2 x 2
 * block the one it comes from, the core's image's (the game's box) or the host's box (the wide one); in the world band
 * the box centred across. */
void ec_check_scaled_text_box(OraclesEnhancedCheck *c, OraclesEnhancedMode mode, const uint32_t *surface)
{
    if (!surface) return;
    const unsigned height = oracles_enhanced_view_height(c->view);
    {   /* Large: the box the presenter shows again larger, at its size in the surface, the core's or the host's. */
        uint32_t rect[4], reserve = 0;
        int bottom = 0, from_core = 0;
        int32_t source[2];
        unsigned size = 0;
        if (oracles_enhanced_view_text_box_enlarged(c->view, rect, &bottom, &reserve, &size) && oracles_enhanced_view_text_box_enlarged_source(c->view, source, &from_core)) {
            const uint32_t *core = oracles_core_pixels(c->core);
            OraclesEnhancedHostTextBox box;
            const int host = !from_core && oracles_enhanced_view_text_box_shown(c->view, &box, NULL, NULL);
            unsigned long long compared = 0, wrong = 0;
            for (unsigned j = 0; j < rect[3]; j++)
                for (unsigned i = 0; i < rect[2]; i++) {
                    const uint32_t shown = surface[(size_t)(rect[1] + j) * c->width + rect[0] + i];
                    if (from_core && core) {
                        const int32_t x = source[0] + (int32_t)i, y = source[1] + (int32_t)j;
                        if (x < 0 || x >= (int32_t)SCREEN_W || y < 0 || y >= (int32_t)SCREEN_H) continue;
                        compared++;
                        wrong += shown != core[(size_t)y * SCREEN_W + (size_t)x];
                    } else if (host && i < box.width && j < box.height && mode == ORACLES_ENHANCED_WORLD) {   /* framed: the window may stand over it */
                        compared++;
                        wrong += shown != box.pixels[j * box.width + i];
                    }
                }
            c->large_frames++;
            if (mode == ORACLES_ENHANCED_WORLD) c->large_frames_world++;
            else c->large_frames_framed++;
            if (size == ORACLES_ENHANCED_TEXT_XLARGE) c->large_frames_xlarge++;
            c->large_pixels += compared;
            c->large_pixels_wrong += wrong;
        }
    }
    int32_t dst[2], src[2];
    unsigned size[2];
    unsigned long long compared = 0, wrong = 0;
    const uint32_t *core = oracles_core_pixels(c->core);
    if (oracles_enhanced_view_text_box_scaled(c->view, dst, src, size) && core) {
        for (unsigned j = 0; j < 2u * size[1]; j++)
            for (unsigned i = 0; i < 2u * size[0]; i++) {
                const int32_t x = dst[0] + (int32_t)i, y = dst[1] + (int32_t)j, cx = src[0] + (int32_t)(i / 2u), cy = src[1] + (int32_t)(j / 2u);
                if (x < 0 || x >= (int32_t)c->width || y < 0 || y >= (int32_t)height || cx < 0 || cx >= (int32_t)SCREEN_W || cy < 0 || cy >= (int32_t)SCREEN_H) continue;
                compared++;
                wrong += surface[(size_t)y * c->width + (size_t)x] != core[(size_t)cy * SCREEN_W + (size_t)cx];
            }
        if (mode == ORACLES_ENHANCED_WORLD && dst[0] != ((int32_t)c->width - (int32_t)(2u * size[0])) / 2) c->scaled_misplaced++;
    } else {
        OraclesEnhancedHostTextBox box;
        int32_t x0 = 0, y0 = 0;
        if (!oracles_enhanced_view_text_box_shown(c->view, &box, &x0, &y0) || oracles_enhanced_view_text_box_shown_scale(c->view) != 2u) return;
        for (unsigned j = 0; j < 2u * box.height; j++)
            for (unsigned i = 0; i < 2u * box.width; i++) {
                const int32_t x = x0 + (int32_t)i, y = y0 + (int32_t)j;
                if (x < 0 || x >= (int32_t)c->width || y < 0 || y >= (int32_t)height) continue;
                compared++;
                wrong += surface[(size_t)y * c->width + (size_t)x] != box.pixels[(j / 2u) * box.width + i / 2u];
            }
        if (mode == ORACLES_ENHANCED_WORLD && x0 != ((int32_t)c->width - (int32_t)(2u * box.width)) / 2) c->scaled_misplaced++;
    }
    {   /* The enlargement's cost: the same copy of every pixel of the box into a 2 x 2 block, into a buffer, timed. */
        static uint32_t scratch[2u * ORACLES_TEXT_BOX_WIDTH_MAX * 2u * ORACLES_TEXT_BOX_HEIGHT_MAX];
        int32_t d[2], s[2];
        unsigned z[2];
        OraclesEnhancedHostTextBox box;
        const uint32_t *from = NULL;
        unsigned w = 0, h = 0, stride = 0;
        if (oracles_enhanced_view_text_box_scaled(c->view, d, s, z) && core) { from = core + (size_t)s[1] * SCREEN_W + (size_t)s[0]; w = z[0]; h = z[1]; stride = SCREEN_W; }
        else if (oracles_enhanced_view_text_box_shown(c->view, &box, NULL, NULL)) { from = box.pixels; w = box.width; h = box.height; stride = box.width; }
        if (from && w <= ORACLES_TEXT_BOX_WIDTH_MAX && h <= ORACLES_TEXT_BOX_HEIGHT_MAX) {
            const double start = ec_monotonic_us();
            for (unsigned j = 0; j < h; j++) {   /* as the view draws it: a line of blocks, then its copy */
                uint32_t *row = scratch + (size_t)2u * j * 2u * w;
                for (unsigned i = 0; i < 2u * w; i++) row[i] = from[j * stride + i / 2u];
                memcpy(row + 2u * w, row, 2u * w * sizeof *row);
            }
            const double spent = ec_monotonic_us() - start;
            c->scaled_sink ^= scratch[0] ^ scratch[(2u * h - 1u) * 2u * w + w];   /* the copy read back: it is done */
            c->scaled_us_total += spent;
            if (spent > c->scaled_us_max) c->scaled_us_max = spent;
        }
    }
    c->scaled_frames++;
    if (mode == ORACLES_ENHANCED_WORLD) c->scaled_frames_world++;
    else c->scaled_frames_framed++;
    c->scaled_pixels += compared;
    c->scaled_pixels_wrong += wrong;
}

static void scaled_summary(const OraclesEnhancedCheck *c, FILE *out)
{
    fprintf(out, "text_box.scaled_frames=%u\ntext_box.scaled_frames_world=%u\ntext_box.scaled_frames_framed=%u\ntext_box.scaled_pixels=%llu\n"
                 "text_box.scaled_pixels_wrong=%llu\ntext_box.scaled_misplaced=%u\ntext_box.scaled_us_mean=%.2f\ntext_box.scaled_us_max=%.1f\n",
            c->scaled_frames, c->scaled_frames_world, c->scaled_frames_framed, c->scaled_pixels, c->scaled_pixels_wrong, c->scaled_misplaced,
            c->scaled_frames ? c->scaled_us_total / c->scaled_frames : 0.0, c->scaled_us_max);
    fprintf(out, "text_box.enlarged_frames=%u\ntext_box.enlarged_frames_world=%u\ntext_box.enlarged_frames_framed=%u\ntext_box.enlarged_frames_xlarge=%u\n"
                 "text_box.enlarged_pixels=%llu\ntext_box.enlarged_pixels_wrong=%llu\n",
            c->large_frames, c->large_frames_world, c->large_frames_framed, c->large_frames_xlarge, c->large_pixels, c->large_pixels_wrong);
    fprintf(out, "text_box.redraw_frames=%u\ntext_box.redraw_wrong=%u\n", c->redraw_frames, c->redraw_wrong);
}

static void wide_summary(const OraclesEnhancedCheck *c, FILE *out)
{
    OraclesGuestTextBoxCounts g;
    oracles_guest_text_box_counts(c->guest, &g);
    unsigned world = 0, framed = 0, uncovered = 0;
    oracles_enhanced_view_text_box_counts(c->view, &world, &framed, &uncovered);
    fprintf(out, "text_box.wide_frames=%u\ntext_box.wide_frames_world=%u\ntext_box.wide_frames_framed=%u\ntext_box.boxes=%u\ntext_box.pages=%u\n",
            c->wide_frames, c->wide_frames_world, c->wide_frames_framed, c->wide_boxes, c->wide_pages);
    fprintf(out, "text_box.text_pixels=%" PRIu64 "\ntext_box.text_pixels_wrong=%" PRIu64 "\ntext_box.text_frames_wrong=%u\ntext_box.text_pixels_skipped=%" PRIu64 "\n",
            c->wide_pixels, c->wide_pixels_wrong, c->wide_frames_wrong, c->wide_pixels_skipped);
    fprintf(out, "text_box.uncovered_frames=%u\ntext_box.absent_frames=%u\ntext_box.absent_longest=%u\ntext_box.reveal_frames=%u\ntext_box.reveal_cut_frames=%u\n"
                 "text_box.reveal_early_frames=%u\ntext_box.cursor_frames=%u\ntext_box.whole_tiles=%u\n",
            uncovered, c->wide_absent, c->wide_absent_longest, c->wide_reveal_frames, c->wide_cut_frames, c->wide_early_frames, c->wide_cursor_frames, c->wide_symbol_tiles);
    fprintf(out, "text_box.lines_composed=%u\ntext_box.wide_lines=%u\ntext_box.overflows=%u\ntext_box.line_copies=%u\ntext_box.copies_unlike=%u\ntext_box.symbols=%u\n"
                 "text_box.wide_us_mean=%.2f\ntext_box.wide_us_max=%.1f\ntext_box.crop_frames=%u\ntext_box.crop_outside=%u\n"
                 "text_box.centred_lines=%u\ntext_box.centred_frames=%u\ntext_box.cursor_centred=%u\ntext_box.cursor_pixels_wrong=%u\n",
            g.composed, g.wide_lines, g.overflows, g.copies, g.copies_unlike, g.symbols, c->wide_frames ? c->wide_us_total / c->wide_frames : 0.0, c->wide_us_max,
            c->wide_crop_frames, c->wide_crop_outside, g.centred, c->wide_centred_frames, c->wide_cursor_centred, c->wide_cursor_pixels_wrong);
    fprintf(out, "text_box.cells=%u\ntext_box.cursor_cells=%u\n", g.cells, c->wide_cursor_cells);
}

static void wide_report(const OraclesEnhancedCheck *c, FILE *out)
{
    OraclesGuestTextBoxCounts g;
    oracles_guest_text_box_counts(c->guest, &g);
    unsigned world = 0, framed = 0, uncovered = 0;
    oracles_enhanced_view_text_box_counts(c->view, &world, &framed, &uncovered);
    fprintf(out, "  wide text box (%u pixels, %u lines): drawn on %u frames (%u world, %u framed) in %u boxes, %u pages; %" PRIu64 " text pixels against the layout, "
                 "%" PRIu64 " wrong in %u frames%s%s; %" PRIu64 " left to a symbol, a cursor or the window\n",
            c->wide_pixels_width, c->wide_lines, c->wide_frames, c->wide_frames_world, c->wide_frames_framed, c->wide_boxes, c->wide_pages, c->wide_pixels,
            c->wide_pixels_wrong, c->wide_frames_wrong, c->wide_frames_wrong ? ": frame:pixels " : "", c->wide_list, c->wide_pixels_skipped);
    fprintf(out, "  wide text box: %u frames of a text whose box the host could not draw, %u of a text without its box (%u at most in a row); %u frames of a line revealed, "
                 "%u showing a character cut, %u one early%s%s; %u frames with a choice's cursor, %u whole tiles drawn (symbols, choices' places); drawn in %.2f us on average, %.1f at most\n",
            uncovered, c->wide_absent, c->wide_absent_longest, c->wide_reveal_frames, c->wide_cut_frames, c->wide_early_frames, c->wide_reveal_list[0] ? ": " : "",
            c->wide_reveal_list, c->wide_cursor_frames, c->wide_symbol_tiles, c->wide_frames ? c->wide_us_total / c->wide_frames : 0.0, c->wide_us_max);
    fprintf(out, "  the host's lines: %u composed (%u wider than the game's 128 pixels, %u characters past the box, %u symbols), %u copied at the game's DMA (%u unlike w7TextGfxBuffer)\n",
            g.composed, g.wide_lines, g.overflows, g.symbols, g.copies, g.copies_unlike);
    fprintf(out, "  wide text box: pages whole at frames (frame:box) %s; a symbol drawn first at (frame:character) %s\n", c->page_list, c->symbol_list[0] ? c->symbol_list : "none");
}

/* The pause's change of Text size (oracles_enhanced_view_redraw_text_box): each world frame with a box, drawn again at
 * each other size, then at the row's, must give back the surface, the rects the sub-pixel camera holds still and the box
 * published for the presenter as the composition left them. */
typedef struct redraw_state {
    unsigned fixed_count;
    uint32_t fixed[ORACLES_ENHANCED_SUBPIXEL_FIXED][4];
    int enlarged, bottom;
    uint32_t rect[4], reserve;
    unsigned size;
} redraw_state;

static void redraw_state_of(OraclesEnhancedCheck *c, redraw_state *s)
{
    memset(s, 0, sizeof *s);
    const uint32_t (*fixed)[4] = NULL;
    const unsigned count = oracles_enhanced_view_subpixel_fixed(c->view, &fixed);
    s->fixed_count = count < ORACLES_ENHANCED_SUBPIXEL_FIXED ? count : ORACLES_ENHANCED_SUBPIXEL_FIXED;
    if (s->fixed_count) memcpy(s->fixed, fixed, s->fixed_count * sizeof s->fixed[0]);
    s->enlarged = oracles_enhanced_view_text_box_enlarged(c->view, s->rect, &s->bottom, &s->reserve, &s->size);
}

void ec_check_text_size_redraw(OraclesEnhancedCheck *c, OraclesEnhancedMode mode)
{
    if (mode != ORACLES_ENHANCED_WORLD) return;
    const size_t pixels = (size_t)c->width * oracles_enhanced_view_height(c->view);
    if (!c->redraw_copy && !(c->redraw_copy = malloc(pixels * sizeof *c->redraw_copy))) return;
    const uint32_t *surface = oracles_enhanced_view_surface(c->view);
    memcpy(c->redraw_copy, surface, pixels * sizeof *surface);
    redraw_state before, after;
    redraw_state_of(c, &before);
    int drawn = 0, wrong = 0;
    for (unsigned size = ORACLES_ENHANCED_TEXT_NORMAL; size <= ORACLES_ENHANCED_TEXT_XLARGE; size++) {
        if (size == c->text_size) continue;
        oracles_enhanced_view_set_text_size(c->view, size);
        drawn = oracles_enhanced_view_redraw_text_box(c->view);
        oracles_enhanced_view_set_text_size(c->view, c->text_size);
        if (!drawn) break;
        oracles_enhanced_view_redraw_text_box(c->view);
        redraw_state_of(c, &after);
        if (memcmp(c->redraw_copy, surface, pixels * sizeof *surface) != 0 || memcmp(&before, &after, sizeof before) != 0) wrong = 1;
    }
    if (!drawn) return;
    c->redraw_frames++;
    c->redraw_wrong += (unsigned)wrong;
}

void ec_host_text_box_summary(const OraclesEnhancedCheck *c, FILE *out)
{
    if (c->text_size) scaled_summary(c, out);
    if (c->wide) { wide_summary(c, out); return; }
    if (!c->host_box) return;
    OraclesGuestTextBoxCounts g;
    oracles_guest_text_box_counts(c->guest, &g);
    fprintf(out, "text_box.host_frames=%u\ntext_box.host_frames_world=%u\ntext_box.host_frames_framed=%u\ntext_box.host_pixels=%" PRIu64 "\n"
                 "text_box.host_pixels_wrong=%" PRIu64 "\ntext_box.host_frames_wrong=%u\ntext_box.host_pixels_skipped=%" PRIu64 "\ntext_box.object_pixels_shown=%" PRIu64 "\n",
            c->host_box_frames, c->host_box_frames_world, c->host_box_frames_framed, c->host_box_pixels, c->host_box_pixels_wrong, c->host_box_frames_wrong,
            c->host_box_pixels_skipped, c->host_box_object_pixels);
    fprintf(out, "text_box.uncovered_frames=%u\ntext_box.uncovered_line_unknown=%u\ntext_box.uncovered_unexpected=%u\ntext_box.absent_frames=%u\ntext_box.absent_longest=%u\n",
            c->host_box_uncovered, c->host_box_unknown, c->host_box_unexpected, c->host_box_absent, c->host_box_absent_longest);
    fprintf(out, "text_box.host_drawn_frames=%u\ntext_box.host_us_mean=%.2f\ntext_box.host_us_max=%.1f\ntext_box.wide_us_mean=%.2f\n", c->host_box_drawn,
            c->host_box_drawn ? c->host_box_us_total / c->host_box_drawn : 0.0, c->host_box_us_max, c->host_box_drawn ? c->host_box_wide_us_total / c->host_box_drawn : 0.0);
    fprintf(out, "text_box.lines_composed=%u\ntext_box.line_copies=%u\ntext_box.reveal_writes=%u\ntext_box.copies_unlike=%u\ntext_box.symbols=%u\ntext_box.cells=%u\n",
            g.composed, g.copies, g.reveal_writes, g.copies_unlike, g.symbols, g.cells);
}

void ec_host_text_box_report(const OraclesEnhancedCheck *c, FILE *out)
{
    if (c->text_size)
        fprintf(out, "  text box for the presenter (Large): published on %u frames (%u world, %u framed; %u at Very large), %llu pixels against the box it shows, %llu wrong\n",
                c->large_frames, c->large_frames_world, c->large_frames_framed, c->large_frames_xlarge, c->large_pixels, c->large_pixels_wrong);
    if (c->text_size)
        fprintf(out, "  text box twice as large: drawn on %u frames (%u world, %u framed), %llu pixels against the box it enlarges, %llu wrong; %u world frames off the band's middle\n",
                c->scaled_frames, c->scaled_frames_world, c->scaled_frames_framed, c->scaled_pixels, c->scaled_pixels_wrong, c->scaled_misplaced);
    if (c->wide) { wide_report(c, out); return; }
    if (!c->host_box) return;
    OraclesGuestTextBoxCounts g;
    oracles_guest_text_box_counts(c->guest, &g);
    fprintf(out, "  text box drawn by the host: %u frames compared with the core's image (%u world, %u framed), %" PRIu64 " pixels, %" PRIu64 " wrong in %u frames%s%s; "
                 "%" PRIu64 " pixels left to an object or the window\n",
            c->host_box_frames, c->host_box_frames_world, c->host_box_frames_framed, c->host_box_pixels, c->host_box_pixels_wrong, c->host_box_frames_wrong,
            c->host_box_frames_wrong ? ": frame:pixels " : "", c->host_box_list, c->host_box_pixels_skipped);
    fprintf(out, "  text box drawn by the host: %u frames uncovered (%u a line unknown, %u a cell unexpected)%s%s, %u frames of a text without its box shown (%u at most in a row); "
                 "drawn on %u frames in %.2f us on average, %.1f at most; at 192 pixels and three lines, %.2f us on average\n",
            c->host_box_uncovered, c->host_box_unknown, c->host_box_unexpected, c->host_box_uncovered ? ": frame:why " : "", c->host_box_uncovered_list,
            c->host_box_absent, c->host_box_absent_longest, c->host_box_drawn, c->host_box_drawn ? c->host_box_us_total / c->host_box_drawn : 0.0, c->host_box_us_max,
            c->host_box_drawn ? c->host_box_wide_us_total / c->host_box_drawn : 0.0);
    fprintf(out, "  text box drawn by the host in frames: %s-%u; pages whole at frames (frame:box) %s\n", c->host_box_runs_list, c->host_box_last_drawn, c->page_list);
    fprintf(out, "  the host's lines: %u composed (%u symbols in them), %u copied at the game's DMA (%u unlike w7TextGfxBuffer), %u reveal writes\n",
            g.composed, g.symbols, g.copies, g.copies_unlike, g.reveal_writes);
}
