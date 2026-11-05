/* The host's text box (guest.h): the lines of the box as the host keeps them, read from the game's text engine.
 *
 * drawLineOfText (textbox.s) clears w7TextGfxBuffer and the line's buffers, then for each character setLineTextBuffers
 * keeps it at column e of w7LineTextBuffer and its attribute in w7LineAttributesBuffer, and retrieveTextCharacter draws
 * its 8 x 16 cell at tiles 2e and 2e + 1: at its return, the game's buffers hold the characters and their cells, which
 * a fixed advance of a cell each lays out again where the game drew them.  The compact text's transaction keeps them
 * itself and composes the host's line from them in the font's widths (guest_compact_text.c), at the host's width for
 * the wide box, in the font's widths or at the fixed advance.
 *
 * dmaTextGfxBuffer adds $94 to a, the top line's tiles for 0, the bottom line's for 2, and calls queueDmaTransfer with
 * hl = w7TextGfxBuffer, de = a:$00 (VRAM bank 0), b = $1f, c = TEXT_BANK.  With the LCD on, queueDmaTransfer writes an
 * entry in wVBlankFunctionQueue: vblankDmaFunction's offset, c, h, l, e, d, e, b; the vblank's handler runs the queue only
 * when the main loop waits for it (vblankInterrupt, wVBlankChecker), each entry's function entered with the entry's next
 * byte pushed: vblankDmaFunction pops it and sets SVBK, HDMA1-2, VBK, HDMA3-5 from the bytes that follow.  With the LCD
 * off, queueDmaTransfer copies at once.
 *
 * The reveal of a line wider than the game's cannot be read in the game's map, whose sixteen columns show its first
 * 128 pixels: it is read in the text's state.  While the standard text reveals a line (wTextDisplayMode 0, states 2, 4
 * and $a), w7d0d3 says which ($40 the top line's tiles, $60 the bottom's), w7NextTextColumnToDisplay how many of its
 * steps are shown, and w7CharacterDisplayTimer counts down the delay of the last one shown (w7LineDelaysBuffer, the sum
 * of its characters' delays) before the next: within that step, the host shows its characters one after the other, each
 * once the delays of those before it have passed, never past the step's boundary.  Read at the vblank, after the frame's
 * logic, it is what the next frame shows with the map the vblank copies: kept one vblank more, it is what the frame just
 * scanned showed. */
#include "guest_internal.h"

#define TILE_PIXELS 8u
#define CELL_BYTES 32u
#define GAME_PIXELS (ORACLES_GUEST_LINE_COLUMNS * TILE_PIXELS)
#define TOP_LINE_PAGE 0x94u      /* $9400, the top line's tiles */
#define BOTTOM_LINE_PAGE 0x96u   /* $9600, the bottom line's */
#define LINE_DMA_BLOCKS 0x1fu    /* the line's 512 bytes, in blocks of 16 less one */
#define CELL_DMA_BLOCKS 0x01u    /* a character's cell, 32 bytes */
#define LCDC_ON 0x80u
#define TOP_LINE_TILES 0x40u     /* w7d0d3 of the top line */
#define BOTTOM_LINE_TILES 0x60u
#define STATE_FORMAT 2u

void oracles_guest_text_box_fixed_font(OraclesTextFont *font)
{
    memset(font, 0, sizeof *font);
    for (unsigned c = ORACLES_TEXT_FIRST_CHARACTER; c < 0x100u; c++) {
        font->width[c] = TILE_PIXELS;
        font->advance[c] = TILE_PIXELS;
    }
}

static void forget(OraclesGuest *guest)
{
    for (unsigned i = 0; i < ORACLES_GUEST_TEXT_BOX_LINES; i++) {
        memset(&guest->text_box[i], 0, sizeof guest->text_box[i]);
        guest->text_box[i].columns = (uint8_t)(guest->text_box_pixels / TILE_PIXELS);
        guest->text_box_shown[i] = guest->text_box[i];
    }
    memset(&guest->text_box_reveal_next, 0, sizeof guest->text_box_reveal_next);
    memset(&guest->text_box_reveal_shown, 0, sizeof guest->text_box_reveal_shown);
    guest->text_box_changed = 0;
}

int oracles_guest_arm_text_box(OraclesGuest *guest, unsigned pixels)
{
    const OraclesGuestTables *t = guest->tables;
    /* The game's width is the harness's check of the game's box alone; a wider box is the host's, which the profile qualifies. */
    if ((pixels > GAME_PIXELS && !oracles_compat_host_text_box(guest->profile)) || pixels < GAME_PIXELS || pixels > ORACLES_GUEST_TEXT_BOX_COLUMNS_MAX * TILE_PIXELS
        || pixels % TILE_PIXELS || t->draw_line_of_text.bank == ORACLES_GUEST_ABSENT || t->dma_text_gfx_buffer.bank == ORACLES_GUEST_ABSENT
        || t->vblank_dma_function.bank == ORACLES_GUEST_ABSENT || t->character_display_timer.bank == ORACLES_GUEST_ABSENT) return -1;
    if (guest->text_box_armed) return guest->text_box_pixels == pixels ? 0 : -1;
    if (oracles_guest_add_hook(guest, t->draw_line_of_text, ORACLES_EVENT_TEXT_LINE, ORACLES_EVENT_TEXT_LINE_DRAWN) != 0
        || oracles_guest_add_hook(guest, t->dma_text_gfx_buffer, ORACLES_EVENT_TEXT_GFX_DMA, 0) != 0
        || oracles_guest_add_hook(guest, t->vblank_dma_function, ORACLES_EVENT_VBLANK_DMA, 0) != 0) return -1;
    guest->text_box_pixels = pixels;
    oracles_guest_text_box_fixed_font(&guest->text_box_fixed);
    forget(guest);
    guest->text_box_armed = 1;
    return 0;
}

int oracles_guest_text_box_armed(const OraclesGuest *guest) { return guest && guest->text_box_armed; }

unsigned oracles_guest_text_box_pixels(const OraclesGuest *guest) { return guest && guest->text_box_armed ? guest->text_box_pixels : 0u; }

int oracles_guest_arm_wide_text_box(OraclesGuest *guest, unsigned pixels, int fixed)
{
    if (pixels <= GAME_PIXELS) return -1;
    const int was_armed = guest->compact_armed;
    if (was_armed && guest->compact_fixed != (fixed != 0)) return -1;
    if (oracles_guest_arm_text_box(guest, pixels) != 0 || oracles_guest_arm_compact_text(guest) != 0) return -1;
    if (!was_armed && fixed) {
        oracles_text_font_fixed(&guest->compact_font, &guest->compact_font);
        guest->compact_fixed = 1;
    }
    return 0;
}

int oracles_guest_set_text_box_pages(OraclesGuest *guest, const uint32_t *ranges, unsigned count)
{
    free(guest->text_box_pages);
    guest->text_box_pages = NULL;
    guest->text_box_page_count = 0;
    if (!count) return 0;
    if (!(guest->text_box_pages = malloc(2u * count * sizeof *ranges))) return -1;
    memcpy(guest->text_box_pages, ranges, 2u * count * sizeof *ranges);
    guest->text_box_page_count = count;
    return 0;
}

int oracles_guest_text_box_in_pages(const uint32_t *ranges, unsigned count, uint32_t at)
{
    unsigned low = 0, high = count;
    while (low < high) {   /* the first pair that ends past `at` */
        const unsigned mid = (low + high) / 2u;
        if (ranges[2u * mid + 1u] <= at) low = mid + 1u;
        else high = mid;
    }
    return low < count && ranges[2u * low] <= at;
}

/* At drawLineOfText's entry, in the wide box: whether the line starts in a page laid out again, else it is centred. */
static void line_place(OraclesGuest *guest)
{
    const OraclesGuestTables *t = guest->tables;
    guest->text_box_centre = 0;
    if (guest->text_box_pixels <= GAME_PIXELS || !oracles_guest_wram_writable(guest, 7)) return;
    const unsigned bank = oracles_guest_read8(guest, t->active_text_bank), addr = oracles_guest_read16(guest, t->text_address);
    const uint32_t at = addr < 0x4000u ? addr : addr < 0x8000u ? (uint32_t)bank * 0x4000u + (addr - 0x4000u) : 0xffffffffu;
    guest->text_box_centre = at != 0xffffffffu && !oracles_guest_text_box_in_pages(guest->text_box_pages, guest->text_box_page_count, at);
}

const OraclesGuestTextBoxLine *oracles_guest_text_box_line(const OraclesGuest *guest, unsigned which)
{
    if (!guest || !guest->text_box_armed || which >= ORACLES_GUEST_TEXT_BOX_LINES) return NULL;
    return which == ORACLES_GUEST_TEXT_BOX_COMPOSED ? &guest->text_box[which] : &guest->text_box_shown[which];
}

const OraclesGuestTextBoxLine *oracles_guest_text_box_next(const OraclesGuest *guest, unsigned which)
{
    if (!guest || !guest->text_box_armed || which >= ORACLES_GUEST_TEXT_BOX_LINES) return NULL;
    return &guest->text_box[which];
}

void oracles_guest_text_box_counts(const OraclesGuest *guest, OraclesGuestTextBoxCounts *counts)
{
    memset(counts, 0, sizeof *counts);
    if (guest) *counts = guest->text_box_counts;
}

void oracles_guest_text_box_reveal_state(const OraclesGuest *guest, OraclesGuestTextBoxReveal *reveal)
{
    memset(reveal, 0, sizeof *reveal);
    if (guest && guest->text_box_armed) *reveal = guest->text_box_reveal_shown;
}

unsigned oracles_guest_text_box_reveal_pixels(const OraclesGuestTextBoxLine *line, unsigned shown, unsigned elapsed)
{
    if (shown >= line->steps) return ORACLES_GUEST_TEXT_BOX_WHOLE;
    if (!shown) return 0;
    const unsigned s = shown - 1u, last = line->first[s + 1u];
    unsigned k = line->first[s], passed = 0;
    while (k + 1u < last && passed + line->character[k].delay <= elapsed) passed += line->character[k++].delay;
    return k + 1u < last ? line->character[k + 1u].start : line->boundary[s];
}

void oracles_guest_text_box_compose(OraclesGuest *guest, const OraclesGuestCompactLine *line, const OraclesGuestCompactCharacter *characters, const uint8_t *gfx)
{
    OraclesGuestTextBoxLine *composed = &guest->text_box[ORACLES_GUEST_TEXT_BOX_COMPOSED];
    composed->columns = (uint8_t)(guest->text_box_pixels / TILE_PIXELS);
    memcpy(composed->gfx, gfx, composed->columns * CELL_BYTES);
    memcpy(composed->attribute, line->attribute, composed->columns);
    composed->count = (uint8_t)line->count;
    composed->steps = (uint8_t)line->steps;
    memset(composed->first, 0, sizeof composed->first);
    memset(composed->boundary, 0, sizeof composed->boundary);
    memset(composed->character, 0, sizeof composed->character);
    memcpy(composed->first, line->first, line->steps + 1u);
    for (unsigned s = 0; s < line->steps; s++) composed->boundary[s] = (uint16_t)oracles_guest_compact_boundary(line, s);
    for (unsigned k = 0; k < line->count; k++) {
        OraclesGuestTextBoxCharacter *c = &composed->character[k];
        c->character = characters[k].character;
        c->whole = characters[k].whole;
        c->attribute = characters[k].attribute;
        c->delay = characters[k].delay;
        c->from = line->from[k];
        c->drawn = line->drawn[k];
        c->start = line->start[k];
    }
    /* A page the game laid out for its 128 pixels, in a wider box: centred, a whole number of tiles further. */
    const unsigned offset = guest->text_box_centre ? (guest->text_box_pixels - GAME_PIXELS) / 2u / TILE_PIXELS * TILE_PIXELS : 0u;
    composed->offset = 0;
    if (offset && 8u * line->columns + offset <= guest->text_box_pixels) {
        const unsigned shift = offset / TILE_PIXELS;
        memmove(composed->gfx + shift * CELL_BYTES, composed->gfx, (composed->columns - shift) * CELL_BYTES);
        memset(composed->gfx, 0xff, shift * CELL_BYTES);
        memmove(composed->attribute + shift, composed->attribute, composed->columns - shift);
        memset(composed->attribute, composed->attribute[shift], shift);
        for (unsigned s = 0; s < line->steps; s++) composed->boundary[s] = (uint16_t)(composed->boundary[s] + offset);
        for (unsigned k = 0; k < line->count; k++) composed->character[k].start = (uint16_t)(composed->character[k].start + offset);
        composed->offset = (uint8_t)offset;
        guest->text_box_counts.centred++;
    }
    composed->known = 1;
    guest->text_box_counts.composed++;
    guest->text_box_counts.overflows += line->overflows;
    if (line->columns > ORACLES_GUEST_LINE_COLUMNS) guest->text_box_counts.wide_lines++;
    for (unsigned k = 0; k < line->count; k++) guest->text_box_counts.symbols += characters[k].character < ORACLES_TEXT_FIRST_CHARACTER;
}

static uint8_t *bank7(OraclesGuest *guest, OraclesGuestSym sym) { return oracles_guest_wram_writable(guest, 7) + (sym.addr - 0xd000u); }

/* Without the compact text: the characters the game kept in the line's buffers and the cells it drew for them, at the
 * fixed advance. */
static void compose_classic(OraclesGuest *guest)
{
    const OraclesGuestTables *t = guest->tables;
    if (!oracles_guest_wram_writable(guest, 7)) return;
    const uint8_t *text = bank7(guest, t->line_text_buffer), *attributes = bank7(guest, t->line_attributes_buffer), *gfx = bank7(guest, t->text_gfx_buffer);
    const uint8_t *delays = bank7(guest, t->line_delays_buffer);
    OraclesGuestCompactCharacter characters[ORACLES_GUEST_LINE_COLUMNS];
    unsigned count = 0;
    for (; count < ORACLES_GUEST_LINE_COLUMNS && text[count]; count++) {
        OraclesGuestCompactCharacter *c = &characters[count];
        memset(c, 0, sizeof *c);
        c->character = text[count];
        c->whole = c->character < ORACLES_TEXT_FIRST_CHARACTER;
        c->attribute = attributes[count];
        c->delay = delays[count];
        memcpy(c->cell, gfx + count * CELL_BYTES, CELL_BYTES);
    }
    OraclesGuestCompactLine line;
    uint8_t out[ORACLES_GUEST_TEXT_BOX_COLUMNS_MAX * CELL_BYTES];
    oracles_guest_compact_layout_at(characters, count, &guest->text_box_fixed, guest->text_box_pixels, &line);
    oracles_guest_compact_compose_at(&line, characters, guest->text_box_pixels, out);
    oracles_guest_text_box_compose(guest, &line, characters, out);
}

/* The line composed copied to the top or bottom line, as the game's DMA copies w7TextGfxBuffer. */
static void copy_line(OraclesGuest *guest, unsigned which)
{
    const OraclesGuestTextBoxLine *composed = &guest->text_box[ORACLES_GUEST_TEXT_BOX_COMPOSED];
    guest->text_box[which] = *composed;
    guest->text_box_changed = 1;
    guest->text_box_counts.copies++;
    const uint8_t *gfx = oracles_guest_ptr(guest, guest->tables->text_gfx_buffer, ORACLES_GUEST_LINE_GFX_BYTES);
    const unsigned shift = composed->offset / TILE_PIXELS;   /* a line centred holds the game's from that tile column */
    if (composed->known && gfx && (shift + ORACLES_GUEST_LINE_COLUMNS > composed->columns
                                   || memcmp(composed->gfx + shift * CELL_BYTES, gfx, ORACLES_GUEST_LINE_GFX_BYTES) != 0)) guest->text_box_counts.copies_unlike++;
}

/* A character the game DMAs itself into a cell of a line's tiles, VRAM bank 0 (the fan games' engine draws a choice's
 * cursor so, and the space that takes it off: dmaTextCharacter in the fan games' engine, where the US one puts its tile in the map):
 * the cell kept in the host's top or bottom line, at its tile column there, the line's offset added. */
static void copy_cell(OraclesGuest *guest, const uint8_t *e)
{
    if ((e[3] & 1u) || e[6] != CELL_DMA_BLOCKS || e[4] < TOP_LINE_PAGE || e[4] > BOTTOM_LINE_PAGE + 1u || e[5] % CELL_BYTES) return;
    const OraclesGuestSym from = { e[0], (uint16_t)(e[1] << 8 | e[2]) };
    const uint8_t *cell = oracles_guest_ptr(guest, from, CELL_BYTES);
    OraclesGuestTextBoxLine *line = &guest->text_box[e[4] < BOTTOM_LINE_PAGE ? ORACLES_GUEST_TEXT_BOX_TOP : ORACLES_GUEST_TEXT_BOX_BOTTOM];
    const unsigned column = ((e[4] & 1u) << 8 | e[5]) / CELL_BYTES + line->offset / TILE_PIXELS;
    if (!cell || !line->known || column >= line->columns) return;
    memcpy(line->gfx + column * CELL_BYTES, cell, CELL_BYTES);
    guest->text_box_changed = 1;
    guest->text_box_counts.cells++;
}

void oracles_guest_text_box_point(OraclesGuest *guest, OraclesGuestEventType type)
{
    const OraclesGuestTables *t = guest->tables;
    const OraclesCoreRegisters r = oracles_core_registers(guest->core);
    if (type == ORACLES_EVENT_TEXT_LINE) line_place(guest);
    else if (type == ORACLES_EVENT_TEXT_LINE_DRAWN) {
        if (!guest->compact_armed) compose_classic(guest);
    } else if (type == ORACLES_EVENT_TEXT_GFX_DMA) {
        const uint8_t *io = oracles_guest_io(guest);
        const unsigned a = r.af >> 8;
        if (io && !(io[0x40] & LCDC_ON) && (a == 0 || a == 2u)) copy_line(guest, a ? ORACLES_GUEST_TEXT_BOX_BOTTOM : ORACLES_GUEST_TEXT_BOX_TOP);
    } else if (type == ORACLES_EVENT_VBLANK_DMA) {
        const OraclesGuestSym top = { 0, r.sp };
        const uint8_t *pushed = oracles_guest_ptr(guest, top, 2);
        if (!pushed) return;
        const OraclesGuestSym at = { 0, (uint16_t)(pushed[0] | pushed[1] << 8) };
        const uint8_t *e = oracles_guest_ptr(guest, at, 7);
        if (!e) return;
        if (e[0] != t->text_gfx_buffer.bank || (unsigned)(e[1] << 8 | e[2]) != t->text_gfx_buffer.addr || (e[3] & 1u) || e[5] != 0
            || e[6] != LINE_DMA_BLOCKS || (e[4] != TOP_LINE_PAGE && e[4] != BOTTOM_LINE_PAGE)) copy_cell(guest, e);
        else copy_line(guest, e[4] == TOP_LINE_PAGE ? ORACLES_GUEST_TEXT_BOX_TOP : ORACLES_GUEST_TEXT_BOX_BOTTOM);
    }
}

void oracles_guest_text_box_reveal(OraclesGuest *guest, unsigned which, unsigned whole, unsigned pixels, uint32_t skipped)
{
    const OraclesGuestTextBoxLine *composed = &guest->text_box[ORACLES_GUEST_TEXT_BOX_COMPOSED];
    OraclesGuestTextBoxLine *line = &guest->text_box[which];
    if (whole > line->columns) return;
    for (unsigned c = 0; c < whole; c++)
        if (!(skipped & 1u << c)) memcpy(line->gfx + c * CELL_BYTES, composed->gfx + c * CELL_BYTES, CELL_BYTES);
    if (pixels < TILE_PIXELS && whole < line->columns) oracles_guest_compact_mask(composed->gfx + whole * CELL_BYTES, pixels, line->gfx + whole * CELL_BYTES);
    line->known = (uint8_t)(line->known && composed->known);
    guest->text_box_changed = 1;
    guest->text_box_counts.reveal_writes++;
}

/* The text's reveal as the game's state stands: the line it is revealing, its steps shown and the frames since the last. */
static void read_reveal(OraclesGuest *guest, OraclesGuestTextBoxReveal *reveal)
{
    memset(reveal, 0, sizeof *reveal);
    const OraclesGuestTables *t = guest->tables;
    if (!oracles_guest_wram_writable(guest, 7)) return;
    const unsigned active = oracles_guest_read8(guest, t->text_is_active), mode = oracles_guest_read8(guest, t->text_display_mode);
    const unsigned state = *bank7(guest, t->text_display_state), tiles = *bank7(guest, t->text_line_tiles), shown = *bank7(guest, t->next_text_column);
    if (!active || active == 0x80u || mode != 0 || (state != 2u && state != 4u && state != 0x0au) || (tiles != TOP_LINE_TILES && tiles != BOTTOM_LINE_TILES)
        || shown >= ORACLES_GUEST_LINE_COLUMNS || !bank7(guest, t->line_text_buffer)[shown]) return;
    reveal->line = (uint8_t)(tiles == BOTTOM_LINE_TILES ? ORACLES_GUEST_TEXT_BOX_BOTTOM : ORACLES_GUEST_TEXT_BOX_TOP);
    reveal->shown = (uint8_t)shown;
    if (shown) {
        const unsigned delay = bank7(guest, t->line_delays_buffer)[shown - 1u], timer = *bank7(guest, t->character_display_timer);
        reveal->elapsed = (uint8_t)(timer < delay ? delay - timer : 0u);
    }
}

void oracles_guest_text_box_vblank(OraclesGuest *guest)
{
    guest->text_box_reveal_shown = guest->text_box_reveal_next;
    read_reveal(guest, &guest->text_box_reveal_next);
    if (!guest->text_box_changed) return;
    guest->text_box_shown[ORACLES_GUEST_TEXT_BOX_TOP] = guest->text_box[ORACLES_GUEST_TEXT_BOX_TOP];
    guest->text_box_shown[ORACLES_GUEST_TEXT_BOX_BOTTOM] = guest->text_box[ORACLES_GUEST_TEXT_BOX_BOTTOM];
    guest->text_box_changed = 0;
}

void oracles_guest_text_box_reset(OraclesGuest *guest)
{
    if (guest->text_box_armed) forget(guest);
}

/* The field: the format, the tile columns of a line, two bytes for later; the reveal read at the last vblank (its line,
 * its steps shown, the frames since); then each line's known flag, its columns, its attributes and its tiles, its
 * characters and steps' count, its steps' first characters and boundaries (little-endian), and its characters (the
 * character, whole, attribute, delay, from, drawn, and start in two bytes). */
static size_t state_size(unsigned columns)
{
    return 4u + 3u + ORACLES_GUEST_TEXT_BOX_LINES * (2u + columns * (1u + CELL_BYTES) + ORACLES_GUEST_TEXT_BOX_LINE_LAYOUT_SIZE);
}

size_t oracles_guest_text_box_save_state(const OraclesGuest *guest, uint8_t *out, size_t capacity)
{
    if (!guest || !guest->text_box_armed) return 0;
    const unsigned columns = guest->text_box_pixels / TILE_PIXELS;
    const size_t size = state_size(columns);
    if (capacity < size) return 0;
    uint8_t *at = out;
    *at++ = STATE_FORMAT;
    *at++ = (uint8_t)columns;
    *at++ = 0;
    *at++ = 0;
    *at++ = guest->text_box_reveal_next.line;
    *at++ = guest->text_box_reveal_next.shown;
    *at++ = guest->text_box_reveal_next.elapsed;
    for (unsigned i = 0; i < ORACLES_GUEST_TEXT_BOX_LINES; i++) {
        const OraclesGuestTextBoxLine *line = &guest->text_box[i];
        *at++ = line->known;
        *at++ = line->columns;
        memcpy(at, line->attribute, columns);
        at += columns;
        memcpy(at, line->gfx, columns * CELL_BYTES);
        at += columns * CELL_BYTES;
        *at++ = line->count;
        *at++ = line->steps;
        *at++ = line->offset;
        memcpy(at, line->first, sizeof line->first);
        at += sizeof line->first;
        for (unsigned s = 0; s < ORACLES_GUEST_LINE_COLUMNS; s++) {
            *at++ = (uint8_t)line->boundary[s];
            *at++ = (uint8_t)(line->boundary[s] >> 8);
        }
        for (unsigned k = 0; k < ORACLES_GUEST_LINE_CHARACTERS; k++) {
            const OraclesGuestTextBoxCharacter *c = &line->character[k];
            const uint8_t bytes[8] = { c->character, c->whole, c->attribute, c->delay, c->from, c->drawn, (uint8_t)c->start, (uint8_t)(c->start >> 8) };
            memcpy(at, bytes, sizeof bytes);
            at += sizeof bytes;
        }
    }
    return size;
}

int oracles_guest_text_box_load_state(OraclesGuest *guest, const uint8_t *data, size_t size)
{
    if (!guest || !guest->text_box_armed) return -1;
    forget(guest);
    const unsigned columns = guest->text_box_pixels / TILE_PIXELS;
    if (size != state_size(columns) || data[0] != STATE_FORMAT || data[1] != columns) return -1;
    const uint8_t *at = data + 4;
    if (at[0] > ORACLES_GUEST_TEXT_BOX_BOTTOM || at[1] > ORACLES_GUEST_LINE_COLUMNS) return -1;
    OraclesGuestTextBoxReveal reveal = { at[0], at[1], at[2] };
    at += 3;
    for (unsigned i = 0; i < ORACLES_GUEST_TEXT_BOX_LINES; i++) {
        OraclesGuestTextBoxLine *line = &guest->text_box[i];
        const uint8_t *layout = at + 2u + columns * (1u + CELL_BYTES);
        if (at[1] != columns || layout[0] > ORACLES_GUEST_LINE_CHARACTERS || layout[1] > ORACLES_GUEST_LINE_COLUMNS) { forget(guest); return -1; }
        line->known = at[0] ? 1 : 0;
        line->columns = at[1];
        at += 2;
        memcpy(line->attribute, at, columns);
        at += columns;
        memcpy(line->gfx, at, columns * CELL_BYTES);
        at += columns * CELL_BYTES;
        line->count = *at++;
        line->steps = *at++;
        line->offset = *at++;
        memcpy(line->first, at, sizeof line->first);
        at += sizeof line->first;
        /* The reveal reads the characters its steps' first ones name, and a line centred holds the game's from its
         * offset: a field that names a character past the line's or an offset past the box's spare width is refused. */
        int bounded = line->offset % TILE_PIXELS == 0u && line->offset <= guest->text_box_pixels - GAME_PIXELS && line->first[line->steps] <= line->count;
        for (unsigned s = 0; s < line->steps; s++) bounded &= line->first[s] <= line->first[s + 1u];
        if (!bounded) { forget(guest); return -1; }
        for (unsigned s = 0; s < ORACLES_GUEST_LINE_COLUMNS; s++, at += 2) line->boundary[s] = (uint16_t)(at[0] | at[1] << 8);
        for (unsigned k = 0; k < ORACLES_GUEST_LINE_CHARACTERS; k++, at += 8) {
            OraclesGuestTextBoxCharacter *c = &line->character[k];
            c->character = at[0];
            c->whole = at[1];
            c->attribute = at[2];
            c->delay = at[3];
            c->from = at[4];
            c->drawn = at[5];
            c->start = (uint16_t)(at[6] | at[7] << 8);
        }
        guest->text_box_shown[i] = *line;
    }
    guest->text_box_reveal_next = reveal;
    guest->text_box_reveal_shown = reveal;
    return 0;
}
