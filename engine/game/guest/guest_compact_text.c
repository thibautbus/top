/* The compact text (guest.h): the text box's lines drawn in the font's own widths, at the game's pace.
 *
 * drawLineOfText (textbox.s) reads a line's bytes up to its break and, for each character, calls setLineTextBuffers,
 * which keeps its character, attribute, delay and sounds at column e of the line's sixteen-byte buffers and moves e on,
 * and retrieveTextCharacter, which draws its 8 x 16 cell, in one of the five colours of \col, at bc, two tiles further
 * on each time: character k is tiles 2k and 2k + 1 of w7TextGfxBuffer, whose 32 tiles dmaTextGfxBuffer queues for the
 * VRAM when the routine returns ($9400 the top line's, $9600 the bottom's).  The names, the numbers, the symbols and a
 * choice's cursor place go through the same two routines from the control codes, within the same call, bc unchanged
 * between them; so do a secret's characters (\secret: @controlCodeA prints w7SecretText1 or 2, hl past the character),
 * which take whole tiles, as a symbol does, so that the player copies them as the game shows them.  At
 * setLineTextBuffers' entry the transaction keeps the character before, its buffers as the game wrote them and its cell
 * as the game drew it, and gives de and bc the line's last column and cell for this one: the game's buffers never see
 * more than sixteen characters, whatever the line holds.  At the return, it composes the 32 tiles
 * again from the cells it kept, so that their colours are the game's own: each character's columns of ink are copied at
 * the pen, which then moves by the character's advance; and it writes the line's buffers as the reveal's steps.
 *
 * textControlCodeC_2 keeps a cursor's place as `(e * 2 | $60) + 1`, the bottom tile of the place's column, which
 * updateSelectedTextPosition reads back to put the cursor's tile in the map at that column, and
 * removeCursorFromSelectedTextPosition to put that tile back; it then prints a space there.  At its `ld (hl),b`, the
 * transaction gives b the tile column the place starts at, the next tile's edge after the characters before it: the
 * place takes that whole tile, the space blank under the cursor.
 *
 * displayNextTextCharacter reads column w7NextTextColumnToDisplay of the line's buffers, writes the column's tiles
 * (`col * 2 + w7d0d3`) and attribute in w7TextboxMap at `(w7d0cc + 2 + col) & $1f`, the top line's rows from 0, the
 * bottom's from $40, plays the column's sound unless its character is a space, sets the next column's delay and moves
 * the column on; A or B shows the rest of the line in the same call.  A column is a step of the transaction: once the
 * routine has written it, before it moves on, the transaction writes the line's columns again, the tile columns whole
 * once the step is shown, which its character says, with their tiles and attributes, the others up to the step with the
 * box's blank, as its margin (column w7d0cc + 1) holds it.  The game's reveal goes on a step at a time, at its pace. */
#include "guest_internal.h"

#define TILE_PIXELS 8u
#define LINE_PIXELS (ORACLES_GUEST_LINE_COLUMNS * TILE_PIXELS)
#define LAST_COLUMN (ORACLES_GUEST_LINE_COLUMNS - 1u)
#define CELL_BYTES 32u
#define MAP_ROW 0x20u
#define BOTTOM_LINE_TILES 0x60u   /* w7d0d3 of the bottom line; the top line's is $40 */
#define SPACE 0x20u
#define OPTION_PLACES 8u          /* w7TextboxOptionPositions */

/* Steps s and s + 1 of `bounds` become one. */
static void join(uint8_t *bounds, unsigned *steps, unsigned s)
{
    for (unsigned i = s + 1u; i < *steps; i++) bounds[i] = bounds[i + 1u];
    (*steps)--;
}

/* The steps of a line of more than sixteen characters: a blank character joins the step before it, from the left, then
 * the two neighbours narrowest together, the leftmost of equals, until sixteen are left. */
static void group(const OraclesGuestCompactCharacter *characters, OraclesGuestCompactLine *line)
{
    uint8_t bounds[ORACLES_GUEST_LINE_CHARACTERS + 1u];
    unsigned steps = line->count;
    for (unsigned s = 0; s <= steps; s++) bounds[s] = (uint8_t)s;
    for (unsigned s = 1; s < steps && steps > ORACLES_GUEST_LINE_COLUMNS;) {
        const unsigned k = bounds[s];
        if (!line->drawn[k] && !characters[k].whole) join(bounds, &steps, s - 1u);
        else s++;
    }
    while (steps > ORACLES_GUEST_LINE_COLUMNS) {
        unsigned best = 0, narrowest = ~0u;
        for (unsigned s = 0; s + 1u < steps; s++) {
            const unsigned end = s + 2u < steps ? line->start[bounds[s + 2u]] : line->pen, width = end - line->start[bounds[s]];
            if (width < narrowest) { narrowest = width; best = s; }
        }
        join(bounds, &steps, best);
    }
    line->steps = steps;
    memcpy(line->first, bounds, steps + 1u);
}

void oracles_guest_compact_layout(const OraclesGuestCompactCharacter *characters, unsigned count, const OraclesTextFont *font, OraclesGuestCompactLine *line)
{
    oracles_guest_compact_layout_at(characters, count, font, LINE_PIXELS, line);
}

void oracles_guest_compact_layout_at(const OraclesGuestCompactCharacter *characters, unsigned count, const OraclesTextFont *font, unsigned pixels,
                                     OraclesGuestCompactLine *line)
{
    memset(line, 0, sizeof *line);
    if (pixels > ORACLES_TEXT_LINE_TILES * TILE_PIXELS) pixels = ORACLES_TEXT_LINE_TILES * TILE_PIXELS;
    const unsigned tiles = pixels / TILE_PIXELS;
    OraclesTextPen pen;
    memset(&pen, 0, sizeof pen);
    unsigned end = 0;
    if (count > ORACLES_GUEST_LINE_CHARACTERS) count = ORACLES_GUEST_LINE_CHARACTERS;
    for (unsigned k = 0; k < count; k++) {
        const OraclesTextPen before = pen;
        unsigned start = 0, from = 0, drawn = 0;
        oracles_text_place(&pen, font, characters[k].character, characters[k].attribute, characters[k].whole, &start, &from, &drawn);
        if (start >= pixels || start + drawn > pixels) { pen = before; break; }
        line->start[k] = (uint8_t)start;
        line->from[k] = (uint8_t)from;
        line->drawn[k] = (uint8_t)drawn;
        if (drawn) end = start + drawn;
        line->count = k + 1u;
    }
    line->overflows = count - line->count;
    line->pen = pen.pen;
    line->columns = (end + TILE_PIXELS - 1u) / TILE_PIXELS;
    for (unsigned t = 0; t < tiles; t++) line->attribute[t] = pen.marked[t] || !t ? pen.attribute[t] : line->attribute[t - 1u];
    if (line->count > ORACLES_GUEST_LINE_COLUMNS) group(characters, line);
    else {
        line->steps = line->count;
        for (unsigned s = 0; s <= line->count; s++) line->first[s] = (uint8_t)s;
    }
}

void oracles_guest_compact_compose(const OraclesGuestCompactLine *line, const OraclesGuestCompactCharacter *characters, uint8_t *out)
{
    oracles_guest_compact_compose_at(line, characters, LINE_PIXELS, out);
}

void oracles_guest_compact_compose_at(const OraclesGuestCompactLine *line, const OraclesGuestCompactCharacter *characters, unsigned pixels, uint8_t *out)
{
    memset(out, 0xff, pixels / TILE_PIXELS * CELL_BYTES);
    for (unsigned k = 0; k < line->count; k++)
        for (unsigned i = 0; i < line->drawn[k]; i++) {
            const unsigned x = line->from[k] + i, to = line->start[k] + i;
            const uint8_t bit = (uint8_t)(0x80u >> x), into = (uint8_t)(0x80u >> (to % TILE_PIXELS));
            /* A cell's rows are its two tiles' in order: row r is the word at 2r, its two bit planes. */
            for (unsigned at = 0; at < CELL_BYTES; at++) {
                uint8_t *pixel = &out[to / TILE_PIXELS * CELL_BYTES + at];
                if (characters[k].cell[at] & bit) *pixel |= into;
                else *pixel &= (uint8_t)~into;
            }
        }
}

unsigned oracles_guest_compact_shown(const OraclesGuestCompactLine *line, unsigned shown)
{
    return oracles_guest_compact_boundary(line, shown) / TILE_PIXELS;
}

unsigned oracles_guest_compact_boundary(const OraclesGuestCompactLine *line, unsigned shown)
{
    const unsigned all = line->columns * TILE_PIXELS;
    if (shown + 1u >= line->steps) return all;
    const unsigned next = line->start[line->first[shown + 1u]];
    return next < all ? next : all;
}

void oracles_guest_compact_step_columns(uint8_t step, int last, unsigned *whole, unsigned *shown)
{
    const unsigned boundary = step & (uint8_t)~ORACLES_GUEST_COMPACT_STEP;
    *shown = (boundary + TILE_PIXELS - 1u) / TILE_PIXELS;
    *whole = last ? *shown : boundary / TILE_PIXELS;
}

void oracles_guest_compact_mask(const uint8_t *tile, unsigned pixels, uint8_t *out)
{
    const uint8_t blank = (uint8_t)(0xffu >> pixels);
    for (unsigned at = 0; at < CELL_BYTES; at++) out[at] = (uint8_t)(tile[at] | blank);
}

uint16_t oracles_guest_find_reveal_point(const OraclesGuestTables *t, const uint8_t *rom, size_t size)
{
    if (t->next_text_column.bank == ORACLES_GUEST_ABSENT) return 0;
    const uint8_t written[] = { 0x7b, 0xd6, 0x20, 0x5f, 0x7e, 0x12, 0x16, (uint8_t)(t->next_text_column.addr >> 8), 0x1e, (uint8_t)t->next_text_column.addr };
    const uint16_t at = oracles_guest_find_in_routine(rom, size, t->display_next_text_character, written, sizeof written);
    return at ? (uint16_t)(at + 6u) : 0;
}

uint16_t oracles_guest_find_cursor_place_point(const OraclesGuestTables *t, const uint8_t *rom, size_t size)
{
    const uint8_t kept[] = { 0x7b, 0x87, 0xf6, 0x60, 0x47, 0x04, 0x70 };   /* ld a,e / add a / or $60 / ld b,a / inc b / ld (hl),b */
    const uint16_t at = oracles_guest_find_in_routine(rom, size, t->text_control_code_option, kept, sizeof kept);
    return at ? (uint16_t)(at + 6u) : 0;
}

int oracles_guest_arm_compact_text(OraclesGuest *guest)
{
    const OraclesGuestTables *t = guest->tables;
    if (!oracles_compat_compact_text(guest->profile) || t->draw_line_of_text.bank == ORACLES_GUEST_ABSENT || t->set_line_text_buffers.bank == ORACLES_GUEST_ABSENT
        || t->font_start.bank == ORACLES_GUEST_ABSENT || t->textbox_option_positions.bank == ORACLES_GUEST_ABSENT) return -1;
    if (guest->compact_armed) return 0;
    const OraclesGuestRom rom = oracles_guest_rom(guest);
    const OraclesRom image = { rom.data, rom.size };
    const OraclesGuestSym reveal = { t->display_next_text_character.bank, oracles_guest_find_reveal_point(t, rom.data, rom.size) };
    const OraclesGuestSym place = { t->text_control_code_option.bank, oracles_guest_find_cursor_place_point(t, rom.data, rom.size) };
    if (!reveal.addr || !place.addr || oracles_text_font(&image, oracles_rom_offset(t->font_start.bank, t->font_start.addr), &guest->compact_font) != ORACLES_OK)
        return -1;
    if (oracles_guest_add_hook(guest, t->draw_line_of_text, ORACLES_EVENT_TEXT_LINE, ORACLES_EVENT_TEXT_LINE_DRAWN) != 0
        || oracles_guest_add_hook(guest, t->set_line_text_buffers, ORACLES_EVENT_TEXT_CHARACTER, 0) != 0
        || oracles_guest_add_hook(guest, place, ORACLES_EVENT_TEXT_CURSOR_PLACE, 0) != 0
        || oracles_guest_add_hook(guest, reveal, ORACLES_EVENT_TEXT_COLUMN_SHOWN, 0) != 0) return -1;
    guest->compact_armed = 1;
    return 0;
}

void oracles_guest_compact_counts(const OraclesGuest *guest, OraclesGuestCompactCounts *counts)
{
    memset(counts, 0, sizeof *counts);
    if (guest) *counts = guest->compact_counts;
}

static uint8_t *bank7(OraclesGuest *guest, OraclesGuestSym sym) { return oracles_guest_wram_writable(guest, 7) + (sym.addr - 0xd000u); }

/* The character the game has just kept at the line's last column and drawn in its last cell, kept by the host. */
static void keep_pending(OraclesGuest *guest)
{
    if (!guest->compact_pending) return;
    guest->compact_pending = 0;
    const OraclesGuestTables *t = guest->tables;
    OraclesGuestCompactCharacter *c = &guest->compact_line[guest->compact_count++];
    c->attribute = bank7(guest, t->line_attributes_buffer)[LAST_COLUMN];
    c->delay = bank7(guest, t->line_delays_buffer)[LAST_COLUMN];
    c->sound = bank7(guest, t->line_sounds_buffer)[LAST_COLUMN];
    c->effect = bank7(guest, t->line_sound_effects_buffer)[LAST_COLUMN];
    memcpy(c->cell, bank7(guest, t->text_gfx_buffer) + LAST_COLUMN * CELL_BYTES, CELL_BYTES);
}

/* Whether the character setLineTextBuffers receives is a secret's, hl past it. */
static int from_secret(const OraclesGuestTables *t, uint16_t hl)
{
    const uint16_t at = (uint16_t)(hl - 1u);
    return t->secret_text.bank != ORACLES_GUEST_ABSENT && t->secret_text_end.bank != ORACLES_GUEST_ABSENT
        && at >= t->secret_text.addr && at < t->secret_text_end.addr;
}

static void keep_character(OraclesGuest *guest)
{
    keep_pending(guest);
    const OraclesGuestTables *t = guest->tables;
    OraclesCoreRegisters r = oracles_core_registers(guest->core);
    if (guest->compact_count < ORACLES_GUEST_LINE_CHARACTERS) {
        OraclesGuestCompactCharacter *c = &guest->compact_line[guest->compact_count];
        memset(c, 0, sizeof *c);
        c->character = (uint8_t)(r.af >> 8);
        c->whole = (uint8_t)(guest->compact_place_next || c->character < ORACLES_TEXT_FIRST_CHARACTER || from_secret(t, r.hl));
        guest->compact_pending = 1;
    } else guest->compact_counts.overflows++;
    guest->compact_place_next = 0;
    r.de = (uint16_t)(t->line_text_buffer.addr + LAST_COLUMN);
    r.bc = (uint16_t)(t->text_gfx_buffer.addr + LAST_COLUMN * CELL_BYTES);
    oracles_core_set_registers(guest->core, &r);
}

/* The width the transaction lays a line out at: the host's wide box's, else the game's line. */
static unsigned line_pixels(const OraclesGuest *guest) { return guest->text_box_armed ? guest->text_box_pixels : LINE_PIXELS; }

static void place_cursor(OraclesGuest *guest)
{
    keep_pending(guest);
    OraclesGuestCompactLine before;
    oracles_guest_compact_layout_at(guest->compact_line, guest->compact_count, &guest->compact_font, line_pixels(guest), &before);
    const unsigned tile = (before.pen + TILE_PIXELS - 1u) / TILE_PIXELS;
    if (tile >= ORACLES_GUEST_LINE_COLUMNS) return;
    OraclesCoreRegisters r = oracles_core_registers(guest->core);
    const unsigned place = r.bc >> 8;
    r.bc = (uint16_t)(((place & ~0x1eu) | tile << 1) << 8 | (r.bc & 0xffu));
    oracles_core_set_registers(guest->core, &r);
    guest->compact_place_next = 1;
    guest->compact_counts.places++;
}

static void compose_line(OraclesGuest *guest)
{
    keep_pending(guest);
    const OraclesGuestTables *t = guest->tables;
    const OraclesGuestCompactCharacter *characters = guest->compact_line;
    const unsigned pixels = line_pixels(guest);
    OraclesGuestCompactLine line;
    uint8_t gfx[ORACLES_TEXT_LINE_TILES * CELL_BYTES];
    oracles_guest_compact_layout_at(characters, guest->compact_count, &guest->compact_font, pixels, &line);
    oracles_guest_compact_compose_at(&line, characters, pixels, gfx);
    if (guest->text_box_armed) oracles_guest_text_box_compose(guest, &line, characters, gfx);
    if (!guest->compact_count) return;
    memcpy(bank7(guest, t->text_gfx_buffer), gfx, ORACLES_GUEST_LINE_GFX_BYTES);   /* its first 128 pixels, the game's */
    uint8_t *text = bank7(guest, t->line_text_buffer), *attributes = bank7(guest, t->line_attributes_buffer), *delays = bank7(guest, t->line_delays_buffer);
    uint8_t *sounds = bank7(guest, t->line_sounds_buffer), *effects = bank7(guest, t->line_sound_effects_buffer);
    for (unsigned s = 0; s < ORACLES_GUEST_LINE_COLUMNS; s++) {
        attributes[s] = line.attribute[s];
        if (s >= line.steps) { text[s] = 0; continue; }
        unsigned delay = 0;
        uint8_t sound = 0, effect = 0;
        for (unsigned k = line.first[s]; k < line.first[s + 1u]; k++) {
            delay += characters[k].delay;
            if (!sound && characters[k].character != SPACE) sound = characters[k].sound;
            if (!effect) effect = characters[k].effect;
        }
        const unsigned boundary = oracles_guest_compact_boundary(&line, s);
        text[s] = (uint8_t)(ORACLES_GUEST_COMPACT_STEP | (boundary < ORACLES_GUEST_COMPACT_STEP ? boundary : ORACLES_GUEST_COMPACT_STEP - 1u));
        delays[s] = (uint8_t)(delay < 0xffu ? delay : 0xffu);
        sounds[s] = sound;
        effects[s] = effect;
    }
    guest->compact_counts.lines++;
    guest->compact_counts.characters += line.count;
    guest->compact_counts.columns += line.columns;
    guest->compact_counts.overflows += line.overflows;
    if (line.count > ORACLES_GUEST_LINE_COLUMNS) guest->compact_counts.long_lines++;
}

/* Whether step `s` of the line's buffers is its last. */
static int last_step(const uint8_t *steps, unsigned s) { return s + 1u >= ORACLES_GUEST_LINE_COLUMNS || !(steps[s + 1u] & ORACLES_GUEST_COMPACT_STEP); }

static void reveal(OraclesGuest *guest)
{
    const OraclesGuestTables *t = guest->tables;
    const unsigned shown = *bank7(guest, t->next_text_column), tiles = *bank7(guest, t->text_line_tiles), column = *bank7(guest, t->textbox_column);
    if (shown >= ORACLES_GUEST_LINE_COLUMNS) return;
    const uint8_t *steps = bank7(guest, t->line_text_buffer);
    if (!(steps[shown] & ORACLES_GUEST_COMPACT_STEP)) return;
    unsigned whole = 0, visible = 0;
    oracles_guest_compact_step_columns(steps[shown], last_step(steps, shown), &whole, &visible);
    const unsigned row = tiles == BOTTOM_LINE_TILES ? 2u * MAP_ROW : 0, last = visible > shown + 1u ? visible - 1u : shown;
    const uint8_t *line_attributes = bank7(guest, t->line_attributes_buffer);
    uint8_t *map = bank7(guest, t->textbox_map), *attributes = bank7(guest, t->textbox_attributes);
    const unsigned margin = ((column + 1u) & (MAP_ROW - 1u)) + row;
    for (unsigned j = 0; j <= last && j < ORACLES_GUEST_LINE_COLUMNS; j++) {
        const unsigned cell = ((column + 2u + j) & (MAP_ROW - 1u)) + row;
        for (unsigned half = 0; half < 2u; half++) {
            const unsigned at = cell + half * MAP_ROW;
            map[at] = j < visible ? (uint8_t)(tiles + 2u * j + half) : map[margin + half * MAP_ROW];
            attributes[at] = j < visible ? line_attributes[j] : attributes[margin + half * MAP_ROW];
        }
    }
}

/* The tile columns of the line of tiles `tiles` that hold a choice's cursor place (w7TextboxOptionPositions: the bottom
 * tile of each place, its line's top tile $40 or $60 plus twice its column, plus 1), a bit each. */
static uint32_t cursor_places(OraclesGuest *guest, unsigned tiles)
{
    const uint8_t *places = bank7(guest, guest->tables->textbox_option_positions);
    uint32_t columns = 0;
    for (unsigned i = 0; i < OPTION_PLACES && places[i]; i++) {
        const unsigned tile = places[i] - 1u;
        if (tile >= tiles && tile < tiles + 2u * ORACLES_GUEST_LINE_COLUMNS) columns |= 1u << ((tile - tiles) / 2u);
    }
    return columns;
}

/* The line's tile columns `from` to `to` - 1 written in the VRAM from its graphics, but those of `skipped`. */
static void write_columns(OraclesGuest *guest, size_t vram_line, const uint8_t *gfx, unsigned from, unsigned to, uint32_t skipped)
{
    while (from < to) {
        if (skipped & 1u << from) { from++; continue; }
        unsigned end = from + 1u;
        while (end < to && !(skipped & 1u << end)) end++;
        oracles_core_write_vram(guest->core, vram_line + from * CELL_BYTES, gfx + from * CELL_BYTES, (end - from) * CELL_BYTES);
        from = end;
    }
}

/* Whether a pixel of a 2-bit tile's row holds ink: the game's blank is colour 3, both planes set. */
static int ink_at(const uint8_t *tile, unsigned row, unsigned x) { return (tile[2u * row] & tile[2u * row + 1u] & 0x80u >> x) == 0; }

/* The line being revealed in the VRAM, at the vblank (no state of the host: all from the WRAM).  While the game reveals
 * it (standard text, states 2, 4 and $a), the tile columns whole once its last step shown is are written as the line's
 * graphics hold them, and the one the next step starts in with only the pixels before that step, the others blank: the
 * map shows it from that step on (reveal), so that every character shown is whole and none not yet shown appears.  Once
 * the line is whole (the states after it, or a choice that follows it), its columns are written whole again, but while
 * a choice is shown the tiles of its cursor places, blank in the line's graphics, where the fan games' engine draws the
 * cursor (updateSelectedTextPosition in hack-base), the US one putting it in the map.  Then the line as the screen
 * will show it is checked against the steps shown: a revealed character's ink not shown, or a later one's shown. */
void oracles_guest_compact_vblank(OraclesGuest *guest)
{
    const OraclesGuestTables *t = guest->tables;
    uint8_t *wram7 = oracles_guest_wram_writable(guest, 7);
    if (!guest->compact_armed || !wram7) return;
    const unsigned active = oracles_guest_read8(guest, t->text_is_active), mode = oracles_guest_read8(guest, t->text_display_mode);
    const unsigned state = *bank7(guest, t->text_display_state), shown = *bank7(guest, t->next_text_column), tiles = *bank7(guest, t->text_line_tiles);
    const uint8_t *steps = bank7(guest, t->line_text_buffer);
    if (!active || active == 0x80u || mode > 1u || !shown || shown > ORACLES_GUEST_LINE_COLUMNS || !(steps[0] & ORACLES_GUEST_COMPACT_STEP)
        || (tiles != BOTTOM_LINE_TILES && tiles != BOTTOM_LINE_TILES - 2u * ORACLES_GUEST_LINE_COLUMNS)) return;
    const int done = shown == ORACLES_GUEST_LINE_COLUMNS || !(steps[shown] & ORACLES_GUEST_COMPACT_STEP);
    const int revealing = mode == 0 && (state == 2u || state == 4u || state == 0x0au);
    if (!(revealing || (done && (mode == 1u || state == 3u || state == 5u || state == 0x0bu || state == 0x0fu)))) return;
    unsigned whole = 0, visible = 0;
    oracles_guest_compact_step_columns(steps[shown - 1u], done, &whole, &visible);
    const unsigned boundary = done ? visible * TILE_PIXELS : steps[shown - 1u] & (uint8_t)~ORACLES_GUEST_COMPACT_STEP;
    const uint8_t *gfx = bank7(guest, t->text_gfx_buffer);
    const size_t vram_line = (tiles == BOTTOM_LINE_TILES ? 0x1600u : 0x1400u);
    /* A choice shown keeps its cursor's tiles: blank in the line's graphics, an engine may draw the cursor there. */
    const uint32_t cursors = mode == 1u ? cursor_places(guest, tiles) : 0u;
    write_columns(guest, vram_line, gfx, 0, whole, cursors);
    if (visible > whole) {
        uint8_t partial[CELL_BYTES];
        oracles_guest_compact_mask(gfx + whole * CELL_BYTES, boundary % TILE_PIXELS, partial);
        oracles_core_write_vram(guest->core, vram_line + whole * CELL_BYTES, partial, CELL_BYTES);
        guest->compact_counts.partial_writes++;
    }
    if (guest->text_box_armed && guest->text_box_pixels == LINE_PIXELS)   /* a wider box's lines stay whole: its reveal is read from the state */
        oracles_guest_text_box_reveal(guest, tiles == BOTTOM_LINE_TILES ? ORACLES_GUEST_TEXT_BOX_BOTTOM : ORACLES_GUEST_TEXT_BOX_TOP, whole,
                                      visible > whole ? boundary % TILE_PIXELS : TILE_PIXELS, cursors);
    if (!revealing) return;
    /* The check: the map the game copies at this vblank, the tiles now in the VRAM, the line's graphics. */
    size_t size = 0;
    uint16_t bank = 0;
    const uint8_t *vram = oracles_core_memory(guest->core, ORACLES_CORE_VRAM, &size, &bank);
    if (!vram || size < vram_line + ORACLES_GUEST_LINE_GFX_BYTES) return;
    const unsigned column = *bank7(guest, t->textbox_column), row = tiles == BOTTOM_LINE_TILES ? 2u * MAP_ROW : 0;
    const uint8_t *map = bank7(guest, t->textbox_map);
    unsigned cut = 0, early = 0;
    for (unsigned j = 0; j < ORACLES_GUEST_LINE_COLUMNS; j++) {
        const int on_screen = map[((column + 2u + j) & (MAP_ROW - 1u)) + row] == tiles + 2u * j;
        for (unsigned x = 0; x < TILE_PIXELS; x++) {
            int ink = 0, drawn = 0;
            for (unsigned r = 0; r < 16u; r++) {
                ink |= ink_at(gfx + j * CELL_BYTES, r, x);
                drawn |= on_screen && ink_at(vram + vram_line + j * CELL_BYTES, r, x);
            }
            if (j * TILE_PIXELS + x < boundary) cut += ink && !drawn;
            else early += drawn;
        }
    }
    guest->compact_counts.reveal_frames++;
    if (cut) guest->compact_counts.cut_frames++;
    if (early) guest->compact_counts.early_frames++;
}

void oracles_guest_compact_text_point(OraclesGuest *guest, OraclesGuestEventType type)
{
    if (!guest->compact_armed || !oracles_guest_wram_writable(guest, 7)) return;
    if (type == ORACLES_EVENT_TEXT_LINE) {
        guest->compact_open = 1;
        guest->compact_pending = guest->compact_place_next = 0;
        guest->compact_count = 0;
        const uint16_t sp = oracles_core_registers(guest->core).sp;
        const OraclesGuestSym top = { 0, sp };
        guest->compact_return = oracles_guest_read16(guest, top);
        guest->compact_return_sp = (uint16_t)(sp + 2u);
    } else if (type == ORACLES_EVENT_TEXT_CHARACTER) {
        if (guest->compact_open) keep_character(guest);
    } else if (type == ORACLES_EVENT_TEXT_CURSOR_PLACE) {
        if (guest->compact_open) place_cursor(guest);
    } else if (type == ORACLES_EVENT_TEXT_LINE_DRAWN) {
        if (guest->compact_open) compose_line(guest);
        else if (guest->text_box_armed) guest->text_box[ORACLES_GUEST_TEXT_BOX_COMPOSED].known = 0;   /* a load within the call: not composed */
        guest->compact_open = guest->compact_pending = guest->compact_place_next = 0;
        guest->compact_count = 0;
    } else if (type == ORACLES_EVENT_TEXT_COLUMN_SHOWN) reveal(guest);
}

/* The open line's state: a version, whether its last character is kept yet and the next is a cursor's place, its
 * characters, where its call returns and SP once it has, then the characters kept (the last one too, its character and
 * its whole alone until the next setLineTextBuffers keeps the rest from the WRAM). */
#define COMPACT_STATE_VERSION 1u
#define COMPACT_STATE_HEAD 7u

size_t oracles_guest_compact_save_state(const OraclesGuest *guest, uint8_t *out, size_t capacity)
{
    if (!guest || !guest->compact_armed || !guest->compact_open) return 0;
    const unsigned kept = guest->compact_count + (guest->compact_pending ? 1u : 0u);
    const size_t size = COMPACT_STATE_HEAD + kept * sizeof guest->compact_line[0];
    if (size > capacity || kept > ORACLES_GUEST_LINE_CHARACTERS) return 0;
    out[0] = COMPACT_STATE_VERSION;
    out[1] = (uint8_t)((guest->compact_pending ? 1u : 0u) | (guest->compact_place_next ? 2u : 0u));
    out[2] = (uint8_t)guest->compact_count;
    out[3] = (uint8_t)guest->compact_return;
    out[4] = (uint8_t)(guest->compact_return >> 8);
    out[5] = (uint8_t)guest->compact_return_sp;
    out[6] = (uint8_t)(guest->compact_return_sp >> 8);
    memcpy(out + COMPACT_STATE_HEAD, guest->compact_line, kept * sizeof guest->compact_line[0]);
    return size;
}

int oracles_guest_compact_load_state(OraclesGuest *guest, const uint8_t *data, size_t size)
{
    if (!guest || !guest->compact_armed || size < COMPACT_STATE_HEAD || data[0] != COMPACT_STATE_VERSION) return -1;
    const unsigned count = data[2], kept = count + (data[1] & 1u);
    if (kept > ORACLES_GUEST_LINE_CHARACTERS || size != COMPACT_STATE_HEAD + kept * sizeof guest->compact_line[0]
        || guest->pending_count >= PENDING_RETURNS) return -1;
    memcpy(guest->compact_line, data + COMPACT_STATE_HEAD, kept * sizeof guest->compact_line[0]);
    guest->compact_open = 1;
    guest->compact_pending = data[1] & 1u;
    guest->compact_place_next = (data[1] & 2u) != 0;
    guest->compact_count = count;
    guest->compact_return = (uint16_t)(data[3] | data[4] << 8);
    guest->compact_return_sp = (uint16_t)(data[5] | data[6] << 8);
    /* The return the load's reset forgot: the line is composed there, as if no load had come between. */
    pending_return *p = &guest->pending[guest->pending_count++];
    p->address = guest->compact_return;
    p->sp_after = guest->compact_return_sp;
    p->type = ORACLES_EVENT_TEXT_LINE_DRAWN;
    guest->compact_counts.resumed++;
    return 0;
}
