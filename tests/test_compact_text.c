/* The compact text (guest.h): the font's widths, the layout of a line and its steps, its composition and its reveal,
 * the search of the transaction's points, and the transaction on a synthetic ROM (no game data) on both cores, a
 * savestate taken within a line included.  At
 * each routine's address of the game's tables, in its bank,
 *   drawLineOfText:           hl = the word at $c0f2 ; ld de,w7LineTextBuffer ; ld bc,w7TextGfxBuffer ; loop: ld a,(hli) ; or a ;
 *                             ret z ; cp 1 ; jr nz,+ ; push hl ; ld hl,w7TextboxOptionPositions ; call textControlCodeC_2 ;
 *                             pop hl ; ld a,' ' ; + call setLineTextBuffers ; call drawCell ; jr loop
 *   setLineTextBuffers:       ld (de),a ; push de ; then at e + $10, $20, $30 and $40 the attribute $80, the delay 4, the
 *                             sound $a0 and no sound effect ; pop de ; ld a,(de) ; inc e ; ret
 *   drawCell ($7000):         the character's 16 rows from a copy of the font at $1000, its ink in the low plane, at bc,
 *                             after 65,536 cycles of nothing when $c0f4 says so: a line then spans frames
 *   textControlCodeC_2:       ld a,e ; add a ; or $60 ; ld b,a ; inc b ; ld (hl),b ; ret
 *   displayNextTextCharacter: ld de,$d1f0 ; ld hl,w7LineAttributesBuffer ; ld a,e ; sub $20 ; ld e,a ; ld a,(hl) ;
 *                             ld (de),a ; ld d,>w7NextTextColumnToDisplay ; ld e,<w7NextTextColumnToDisplay ; ld a,(de) ;
 *                             inc a ; ld (de),a ; ret
 * called from a loop as $c0f0 says, WRAM bank 7 mapped, the line's characters written by the test at $c100 (1 a
 * choice's cursor place), or at w7SecretText1 for a secret's; on the user's ROMs too, when $ORACLES_ROM_DIR names them:
 * the points and the font. */
#include "core.h"
#include "guest.h"
#include "state.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

#define FONT_START 0x70720u   /* 1c:4720, both games' gfx_font_start */
#define FONT_COPY 0x1000u     /* the copy drawCell reads, in bank 0 */
#define DRAW_CELL 0x7000u
#define GLYPH_I 0x69u
#define GLYPH_A 0x41u
#define GLYPH_FULL 0x7fu
#define BUTTON_LEFT 0xb8u     /* the A button's halves, one glyph across two cells */
#define BUTTON_RIGHT 0xb9u
#define PLACE 0x01u           /* in the test's line: a choice's cursor place */

/* A font of five glyphs at `at`: 'i' one column of ink (3), 'A' six (1 to 6), $7f all eight, and a button's halves, the
 * left one's ink in columns 3 to 7, the right one's in 0 to 4. */
static void write_font(uint8_t *at)
{
    memset(at, 0xff, 16u * 0x100u);
    for (unsigned row = 2; row < 14u; row++) {
        at[16u * GLYPH_I + row] = 0xef;
        at[16u * GLYPH_A + row] = row & 1u ? 0x81 : 0xbd;
        at[16u * GLYPH_FULL + row] = 0x00;
        at[16u * BUTTON_LEFT + row] = 0xe0;
        at[16u * BUTTON_RIGHT + row] = 0x07;
    }
}

static void test_font(void)
{
    uint8_t *rom = calloc(1024u * 1024u, 1);
    if (!rom) { failures++; return; }
    write_font(rom + FONT_START);
    const OraclesRom image = { rom, 1024u * 1024u };
    OraclesTextFont font;
    CHECK(oracles_text_font(&image, FONT_START, &font) == ORACLES_OK);
    CHECK(font.left[GLYPH_I] == 3 && font.width[GLYPH_I] == 1 && font.advance[GLYPH_I] == 2);
    CHECK(font.left[GLYPH_A] == 1 && font.width[GLYPH_A] == 6 && font.advance[GLYPH_A] == 7);
    CHECK(font.left[GLYPH_FULL] == 0 && font.width[GLYPH_FULL] == 8 && font.advance[GLYPH_FULL] == 8);   /* up to the edge: its width */
    CHECK(font.left[BUTTON_LEFT] == 3 && font.width[BUTTON_LEFT] == 5 && font.advance[BUTTON_LEFT] == 5);   /* it goes on in the next cell */
    CHECK(font.left[BUTTON_RIGHT] == 0 && font.width[BUTTON_RIGHT] == 5 && font.advance[BUTTON_RIGHT] == 6);
    CHECK(font.width[' '] == 0 && font.advance[' '] == ORACLES_TEXT_SPACE_ADVANCE);
    CHECK(font.advance[0x06] == 0 && font.width[0x0f] == 0);   /* control codes: no character */
    const OraclesRom short_image = { rom, FONT_START + 16u * 0xffu };
    CHECK(oracles_text_font(&short_image, FONT_START, &font) == ORACLES_ERR_RANGE);
    free(rom);
}

static OraclesTextFont test_font_table(void)
{
    OraclesTextFont font;
    memset(&font, 0, sizeof font);
    for (unsigned c = ORACLES_TEXT_FIRST_CHARACTER; c < 0x100u; c++) font.advance[c] = ORACLES_TEXT_SPACE_ADVANCE;
    font.left[GLYPH_I] = 3; font.width[GLYPH_I] = 1; font.advance[GLYPH_I] = 2;
    font.left[GLYPH_A] = 1; font.width[GLYPH_A] = 6; font.advance[GLYPH_A] = 7;
    font.width[GLYPH_FULL] = 8; font.advance[GLYPH_FULL] = 8;
    font.left[BUTTON_LEFT] = 3; font.width[BUTTON_LEFT] = 5; font.advance[BUTTON_LEFT] = 5;
    font.width[BUTTON_RIGHT] = 5; font.advance[BUTTON_RIGHT] = 6;
    return font;
}

/* The characters of `text` (PLACE a cursor's place, a space), all of `attribute`. */
static unsigned characters_of(const char *text, uint8_t attribute, OraclesGuestCompactCharacter *out)
{
    unsigned n = 0;
    for (; text[n]; n++) {
        memset(&out[n], 0, sizeof out[n]);
        out[n].character = text[n] == PLACE ? ' ' : (uint8_t)text[n];
        out[n].whole = text[n] == PLACE || (uint8_t)text[n] < ORACLES_TEXT_FIRST_CHARACTER;
        out[n].attribute = attribute;
    }
    return n;
}

static void test_layout(void)
{
    const OraclesTextFont font = test_font_table();
    OraclesGuestCompactCharacter c[ORACLES_GUEST_LINE_CHARACTERS + 4u];
    OraclesGuestCompactLine line;
    oracles_guest_compact_layout(c, characters_of("Ai A", 0x80, c), &font, &line);
    CHECK(line.count == 4 && line.start[0] == 0 && line.start[1] == 7 && line.start[2] == 9 && line.start[3] == 14);
    CHECK(line.from[0] == 1 && line.drawn[0] == 6 && line.from[1] == 3 && line.drawn[1] == 1 && line.drawn[2] == 0);
    CHECK(line.pen == 21 && line.columns == 3 && line.steps == 4 && line.first[3] == 3 && line.overflows == 0);
    CHECK(oracles_guest_compact_shown(&line, 0) == 0 && oracles_guest_compact_shown(&line, 1) == 1);
    CHECK(oracles_guest_compact_shown(&line, 2) == 1 && oracles_guest_compact_shown(&line, 3) == 3);   /* the last: every column */
    /* Ink of another attribute starts the next tile; a blank character does not, and gives its attribute to a tile
     * without ink. */
    characters_of("AA   i", 0x80, c);
    for (unsigned k = 1; k < 5u; k++) c[k].attribute = 0x81;
    oracles_guest_compact_layout(c, 6, &font, &line);
    CHECK(line.start[1] == 8 && line.attribute[0] == 0x80 && line.attribute[1] == 0x81);
    CHECK(line.start[2] == 15 && line.start[4] == 25 && line.start[5] == 30);
    CHECK(line.attribute[2] == 0x81 && line.attribute[3] == 0x80 && line.attribute[15] == 0x80);
    /* A symbol and a cursor's place take a whole tile, from the next tile's edge. */
    oracles_guest_compact_layout(c, characters_of("i\x06i\x01" "A", 0x80, c), &font, &line);
    CHECK(line.start[1] == 8 && line.from[1] == 0 && line.drawn[1] == 8 && line.start[2] == 16);
    CHECK(line.start[3] == 24 && line.drawn[3] == 8 && line.start[4] == 32 && line.columns == 5);
    /* A glyph across two cells: its halves joined, no column between them; a letter after a half that reaches its cell's
     * edge keeps a column before it. */
    oracles_guest_compact_layout(c, characters_of("i\xb8\xb9i\xb8" "A", 0x80, c), &font, &line);
    CHECK(line.start[1] == 2 && line.start[2] == 7 && line.from[2] == 0 && line.start[3] == 13);
    CHECK(line.start[4] == 15 && line.start[5] == 21 && line.from[5] == 1);
    /* Sixteen characters of a whole cell each: the line's 128 pixels, a step each. */
    char full[24];
    memset(full, GLYPH_FULL, 16); full[16] = 0;
    oracles_guest_compact_layout(c, characters_of(full, 0x80, c), &font, &line);
    CHECK(line.count == 16 && line.start[15] == 120 && line.pen == 128 && line.columns == 16 && line.steps == 16);
    /* More: past the 128 pixels, the characters are not drawn. */
    memset(full, GLYPH_FULL, 18); full[18] = 0;
    oracles_guest_compact_layout(c, characters_of(full, 0x80, c), &font, &line);
    CHECK(line.count == 16 && line.overflows == 2 && line.pen == 128);
    /* Twenty-one characters: the space joins the step before it, then the narrowest neighbours, the leftmost first,
     * until sixteen steps; each character in one. */
    oracles_guest_compact_layout(c, characters_of("iiiiiiiiii iiiiiiiiii", 0x80, c), &font, &line);
    CHECK(line.count == 21 && line.pen == 45 && line.columns == 6 && line.steps == 16);
    CHECK(line.first[0] == 0 && line.first[16] == 21);
    unsigned joined_space = 0;
    for (unsigned s = 0; s < line.steps; s++) {
        CHECK(line.first[s] < line.first[s + 1u]);
        if (line.first[s + 1u] == 11u) joined_space = 1;   /* the space (10) ends a step */
    }
    CHECK(joined_space);
    CHECK(oracles_guest_compact_shown(&line, 15) == 6);
    oracles_guest_compact_layout(c, 0, &font, &line);
    CHECK(line.count == 0 && line.columns == 0 && line.steps == 0);
}

/* The cell of a character as the game draws it in white (lo plane the font's row, hi plane $ff): ink `row_bits`. */
static void classic_cell(uint8_t *cell, uint8_t row_bits)
{
    for (unsigned r = 0; r < 16u; r++) {
        cell[2u * r] = (uint8_t)~row_bits;
        cell[2u * r + 1u] = 0xff;
    }
}

/* Pixel x, row r of composed tiles: its colour (0 to 3). */
static unsigned pixel(const uint8_t *gfx, unsigned x, unsigned r)
{
    const unsigned at = x / 8u * 32u + 2u * r, bit = 0x80u >> (x % 8u);
    return ((gfx[at] & bit) ? 1u : 0u) | ((gfx[at + 1u] & bit) ? 2u : 0u);
}

static void test_compose(void)
{
    const OraclesTextFont font = test_font_table();
    OraclesGuestCompactCharacter c[4];
    characters_of("AiA", 0x80, c);
    classic_cell(c[0].cell, 0x7e);   /* 'A': ink in columns 1 to 6 */
    classic_cell(c[1].cell, 0x10);   /* 'i': column 3 */
    classic_cell(c[2].cell, 0x42);   /* 'A': 1 and 6 */
    OraclesGuestCompactLine line;
    oracles_guest_compact_layout(c, 3, &font, &line);
    uint8_t out[ORACLES_GUEST_LINE_GFX_BYTES];
    oracles_guest_compact_compose(&line, c, out);
    for (unsigned r = 0; r < 16u; r++) {
        CHECK(pixel(out, 0, r) == 2 && pixel(out, 5, r) == 2 && pixel(out, 6, r) == 3);   /* the first A, then the gap */
        CHECK(pixel(out, 7, r) == 2 && pixel(out, 8, r) == 3);                             /* the i, its column 3 at the pen */
        CHECK(pixel(out, 9, r) == 2 && pixel(out, 10, r) == 3 && pixel(out, 13, r) == 3 && pixel(out, 14, r) == 2);
        CHECK(pixel(out, 15, r) == 3 && pixel(out, 127, r) == 3);
    }
    /* The A button's halves (ink in columns 3 to 7, then 0 to 4): one glyph of ten columns of ink, none blank inside. */
    characters_of("\xb8\xb9", 0x80, c);
    classic_cell(c[0].cell, 0x1f);
    classic_cell(c[1].cell, 0xf8);
    oracles_guest_compact_layout(c, 2, &font, &line);
    oracles_guest_compact_compose(&line, c, out);
    for (unsigned r = 0; r < 16u; r++) {
        for (unsigned x = 0; x < 10u; x++) CHECK(pixel(out, x, r) == 2);
        CHECK(pixel(out, 10, r) == 3);
    }
}

static void put(uint8_t *rom, size_t at, const uint8_t *bytes, size_t count) { memcpy(rom + at, bytes, count); }

static void test_find(const OraclesGuestTables *t)
{
    const size_t size = 1024u * 1024u;
    uint8_t *rom = calloc(size, 1);
    if (!rom) { failures++; return; }
    const uint8_t written[] = { 0x7b, 0xd6, 0x20, 0x5f, 0x7e, 0x12, 0x16, 0xd0, 0x1e, 0xd0 };
    const uint8_t kept[] = { 0x7b, 0x87, 0xf6, 0x60, 0x47, 0x04, 0x70 };
    const size_t reveal_at = oracles_rom_offset(t->display_next_text_character.bank, t->display_next_text_character.addr);
    const size_t place_at = oracles_rom_offset(t->text_control_code_option.bank, t->text_control_code_option.addr);
    CHECK(oracles_guest_find_reveal_point(t, rom, size) == 0 && oracles_guest_find_cursor_place_point(t, rom, size) == 0);
    put(rom, reveal_at + 0x37u, written, sizeof written);   /* where the games have them */
    put(rom, place_at + 0x0cu, kept, sizeof kept);
    CHECK(oracles_guest_find_reveal_point(t, rom, size) == t->display_next_text_character.addr + 0x3du);
    CHECK(oracles_guest_find_cursor_place_point(t, rom, size) == t->text_control_code_option.addr + 0x12u);
    put(rom, reveal_at + 0x10u, written, sizeof written);   /* twice: none taken */
    CHECK(oracles_guest_find_reveal_point(t, rom, size) == 0);
    memset(rom + reveal_at + 0x10u, 0, sizeof written);
    rom[reveal_at + 0x37u + 9u] = 0xd1;   /* another variable than w7NextTextColumnToDisplay */
    CHECK(oracles_guest_find_reveal_point(t, rom, size) == 0);
    memset(rom + place_at + 0x0cu, 0, sizeof kept);
    put(rom, place_at + 0x40u, kept, sizeof kept);   /* past the routine's first $40 bytes */
    CHECK(oracles_guest_find_cursor_place_point(t, rom, size) == 0);
    free(rom);
}

static uint8_t *emit16(uint8_t *at, uint8_t opcode, unsigned value)
{
    *at++ = opcode;
    *at++ = (uint8_t)value;
    *at++ = (uint8_t)(value >> 8);
    return at;
}

static uint8_t *synthetic_rom(const OraclesGuestTables *t, OraclesGame game, size_t size)
{
    uint8_t *rom = calloc(size, 1);
    if (!rom) return NULL;
    memcpy(rom + 0x134, game == ORACLES_GAME_AGES ? "ZELDA NAYRU" : "ZELDA DIN", game == ORACLES_GAME_AGES ? 11 : 9);
    rom[0x143] = 0xc0; rom[0x147] = 0x1b; rom[0x148] = 0x05; rom[0x149] = 0x02;
    rom[0x100] = 0x00; rom[0x101] = 0xc3; rom[0x102] = 0x50; rom[0x103] = 0x01;   /* nop ; jp $0150 */
    uint8_t *at = rom + 0x150;
    at = emit16(at, 0x31, 0xc200);                                                 /* ld sp,$c200 */
    *at++ = 0x3e; *at++ = 0x07; *at++ = 0xe0; *at++ = 0x70;                        /* ld a,7 ; ldh (<SVBK),a */
    *at++ = 0x3e; *at++ = t->draw_line_of_text.bank; at = emit16(at, 0xea, 0x2000); /* ld a,bank ; ld ($2000),a */
    const unsigned loop = (unsigned)(at - rom);
    at = emit16(at, 0xfa, 0xc0f0);                                                 /* loop: ld a,($c0f0) */
    *at++ = 0xb7; *at++ = 0x28; *at++ = 0xfa;                                      /* or a ; jr z,loop */
    *at++ = 0xfe; *at++ = 0x01; *at++ = 0xf5;                                      /* cp 1 ; push af */
    *at++ = 0xaf; at = emit16(at, 0xea, 0xc0f0); *at++ = 0xf1;                     /* xor a ; ld ($c0f0),a ; pop af */
    *at++ = 0x20; *at++ = 0x05;                                                    /* jr nz,+5 */
    at = emit16(at, 0xcd, t->draw_line_of_text.addr);                             /* call drawLineOfText */
    *at++ = 0x18; *at++ = 0x03;                                                    /* jr +3 */
    at = emit16(at, 0xcd, t->display_next_text_character.addr);                   /* + call displayNextTextCharacter */
    emit16(at, 0xc3, loop);                                                        /* jp loop */
    write_font(rom + FONT_COPY);
    at = rom + oracles_rom_offset(t->draw_line_of_text.bank, t->draw_line_of_text.addr);
    at = emit16(at, 0xfa, 0xc0f2); *at++ = 0x6f; at = emit16(at, 0xfa, 0xc0f3); *at++ = 0x67;   /* hl = the word at $c0f2 */
    at = emit16(at, 0x11, t->line_text_buffer.addr);
    at = emit16(at, 0x01, t->text_gfx_buffer.addr);
    uint8_t *const top = at;
    *at++ = 0x2a; *at++ = 0xb7; *at++ = 0xc8;                                      /* loop: ld a,(hli) ; or a ; ret z */
    *at++ = 0xfe; *at++ = PLACE; *at++ = 0x20; *at++ = 0x0a;                       /* cp PLACE ; jr nz,+ */
    *at++ = 0xe5; at = emit16(at, 0x21, t->textbox_option_positions.addr);        /* push hl ; ld hl,w7TextboxOptionPositions */
    at = emit16(at, 0xcd, t->text_control_code_option.addr);
    *at++ = 0xe1; *at++ = 0x3e; *at++ = ' ';                                       /* pop hl ; ld a,' ' */
    at = emit16(at, 0xcd, t->set_line_text_buffers.addr);                         /* + call setLineTextBuffers */
    at = emit16(at, 0xcd, DRAW_CELL);
    *at = 0x18; at[1] = (uint8_t)(top - (at + 2));                                 /* jr loop */
    const uint8_t keep[] = { 0x12, 0xd5,                                           /* ld (de),a ; push de */
                             0x7b, 0xc6, 0x10, 0x5f, 0x3e, 0x80, 0x12,             /* the attribute $80 at e + $10 */
                             0x7b, 0xc6, 0x10, 0x5f, 0x3e, 0x04, 0x12,             /* the delay 4 at e + $20 */
                             0x7b, 0xc6, 0x10, 0x5f, 0x3e, 0xa0, 0x12,             /* the sound $a0 at e + $30 */
                             0x7b, 0xc6, 0x10, 0x5f, 0xaf, 0x12,                   /* no sound effect at e + $40 */
                             0xd1, 0x1a, 0x1c, 0xc9 };                             /* pop de ; ld a,(de) ; inc e ; ret */
    put(rom, oracles_rom_offset(t->set_line_text_buffers.bank, t->set_line_text_buffers.addr), keep, sizeof keep);
    const uint8_t draw[] = { 0xf5, 0xfa, 0xf4, 0xc0, 0xb7, 0x28, 0x0c,               /* push af ; ld a,($c0f4) ; or a ; jr z,+ */
                             0xc5, 0x06, 0x10, 0x0e, 0x00, 0x0d, 0x20, 0xfd, 0x05, 0x20, 0xfa, 0xc1,   /* 16 x 256 dec c */
                             0xf1,                                                      /* + pop af */
                             0xe5, 0xd5, 0x6f, 0x26, 0x00, 0x29, 0x29, 0x29, 0x29,   /* push hl ; push de ; hl = 16 a */
                             0x11, (uint8_t)FONT_COPY, (uint8_t)(FONT_COPY >> 8), 0x19, 0x1e, 0x10,   /* + the font's copy ; ld e,16 */
                             0x2a, 0x02, 0x03, 0x3e, 0xff, 0x02, 0x03, 0x1d, 0x20, 0xf6,   /* a row: its ink low, $ff high */
                             0xd1, 0xe1, 0xc9 };
    put(rom, oracles_rom_offset(t->draw_line_of_text.bank, DRAW_CELL), draw, sizeof draw);
    const uint8_t place[] = { 0x7b, 0x87, 0xf6, 0x60, 0x47, 0x04, 0x70, 0xc9 };
    put(rom, oracles_rom_offset(t->text_control_code_option.bank, t->text_control_code_option.addr), place, sizeof place);
    at = rom + oracles_rom_offset(t->display_next_text_character.bank, t->display_next_text_character.addr);
    at = emit16(at, 0x11, 0xd1f0);
    at = emit16(at, 0x21, t->line_attributes_buffer.addr);
    const uint8_t reveal[] = { 0x7b, 0xd6, 0x20, 0x5f, 0x7e, 0x12, 0x16, (uint8_t)(t->next_text_column.addr >> 8), 0x1e, (uint8_t)t->next_text_column.addr,
                               0x1a, 0x3c, 0x12, 0xc9 };
    memcpy(at, reveal, sizeof reveal);
    write_font(rom + FONT_START);
    return rom;
}

/* The line `text` drawn by the synthetic drawLineOfText, read at $c100, or in w7SecretText1 (`secret`). */
static void draw_text(OraclesCore *core, OraclesGuest *guest, const OraclesGuestTables *t, const char *text, int secret)
{
    uint8_t *wram0 = oracles_guest_wram_writable(guest, 0), *wram7 = oracles_guest_wram_writable(guest, 7);
    memset(wram7 + 0x400u, 0, 0x60u);   /* the line's buffers, which clearLineTextBuffer empties */
    memset(wram7 + (t->text_gfx_buffer.addr - 0xd000u), 0xff, ORACLES_GUEST_LINE_GFX_BYTES);
    const unsigned from = secret ? t->secret_text.addr : 0xc100u;
    memcpy(secret ? wram7 + (from - 0xd000u) : wram0 + (from - 0xc000u), text, strlen(text) + 1u);
    wram0[0xf2] = (uint8_t)from;
    wram0[0xf3] = (uint8_t)(from >> 8);
    wram0[0xf0] = 1;
    oracles_core_run_frame(core);
    CHECK(wram0[0xf0] == 0);
}

static void draw_line(OraclesCore *core, OraclesGuest *guest, const OraclesGuestTables *t, const char *text) { draw_text(core, guest, t, text, 0); }

static void test_transaction(OraclesCoreKind kind, OraclesGame game)
{
    const OraclesGuestTables *t = game == ORACLES_GAME_AGES ? &oracles_guest_tables_ages : &oracles_guest_tables_seasons;
    const size_t size = 1024u * 1024u;
    uint8_t *rom = synthetic_rom(t, game, size);
    if (!rom) { failures++; return; }
    const OraclesCoreOptions options = { 0, 0, kind, 0 };
    OraclesCore *core = oracles_core_create(rom, size, &options);
    free(rom);
    CHECK(core != NULL);
    if (!core) return;
    const OraclesRomInfo info = { .game = game, .revision = game == ORACLES_GAME_AGES ? ORACLES_ROM_REVISION_AGES_US : ORACLES_ROM_REVISION_SEASONS_US,
                                  .known = 1 };
    OraclesGuest *guest = oracles_guest_attach(core, oracles_compat_find(&info));
    CHECK(guest && oracles_guest_arm_compact_text(guest) == 0 && oracles_guest_arm_compact_text(guest) == 0);
    if (!guest) { oracles_core_destroy(core); return; }
    for (unsigned f = 0; f < 200u; f++) oracles_core_run_frame(core);   /* the boot ROM */
    uint8_t *wram0 = oracles_guest_wram_writable(guest, 0), *wram7 = oracles_guest_wram_writable(guest, 7);
    wram0[0xf4] = 0;
    uint8_t *text = wram7 + (t->line_text_buffer.addr - 0xd000u), *gfx = wram7 + (t->text_gfx_buffer.addr - 0xd000u);
    uint8_t *delays = wram7 + (t->line_delays_buffer.addr - 0xd000u), *sounds = wram7 + (t->line_sounds_buffer.addr - 0xd000u);
    /* The line "iiii", a cursor's place, "A": the place at the tile column after "iiii" (8 pixels), 1, a step each. */
    draw_line(core, guest, t, "iiii\x01" "A");
    CHECK(wram7[t->textbox_option_positions.addr - 0xd000u] == ((0x60u | 1u << 1) + 1u));
    const unsigned boundary[6] = { 2, 4, 6, 8, 16, 24 }, expected[6] = { 1, 1, 1, 1, 2, 3 };
    for (unsigned s = 0; s < 6u; s++) {
        CHECK(text[s] == (ORACLES_GUEST_COMPACT_STEP | boundary[s]) && delays[s] == 4u);
        CHECK(sounds[s] == (s == 4u ? 0u : 0xa0u));   /* the place's space: none */
    }
    CHECK(text[6] == 0);
    for (unsigned r = 0; r < 16u; r++) {
        CHECK(pixel(gfx, 0, r) == (r >= 2u && r < 14u ? 2u : 3u) && pixel(gfx, 1, r) == 3 && pixel(gfx, 6, r) == pixel(gfx, 0, r) && pixel(gfx, 7, r) == 3);
        CHECK(pixel(gfx, 15, r) == 3 && pixel(gfx, 16, r) == pixel(gfx, 0, r) && pixel(gfx, 21, r) == pixel(gfx, 0, r) && pixel(gfx, 22, r) == 3);
    }
    OraclesGuestCompactCounts counts;
    oracles_guest_compact_counts(guest, &counts);
    CHECK(counts.lines == 1 && counts.characters == 6 && counts.columns == 3 && counts.places == 1 && counts.long_lines == 0 && counts.overflows == 0);
    /* The reveal of the top line, the box's column at 0: the map shows the tile columns the characters shown reach (the
     * VRAM holds only their pixels: below), the others blank as the box's margin (column 1). */
    uint8_t *map = wram7 + (t->textbox_map.addr - 0xd000u), *attributes = wram7 + (t->textbox_attributes.addr - 0xd000u);
    memset(map, 0x02, 5u * 0x20u);   /* the box's five rows, below the text's variables */
    memset(attributes, 0x80, 5u * 0x20u);
    wram7[t->text_line_tiles.addr - 0xd000u] = 0x40;
    wram7[t->textbox_column.addr - 0xd000u] = 0;
    for (unsigned k = 0; k < 6u; k++) {
        CHECK(wram7[t->next_text_column.addr - 0xd000u] == k);
        map[2u + k] = 0x99;   /* the game's own write of the column, which the transaction takes back */
        wram0[0xf0] = 2;
        oracles_core_run_frame(core);
        for (unsigned j = 0; j < 8u; j++) {
            const int tile = j < expected[k];
            CHECK(map[2u + j] == (tile ? 0x40u + 2u * j : 0x02u) && map[0x22u + j] == (tile ? 0x41u + 2u * j : 0x02u));
            CHECK(attributes[2u + j] == 0x80u);
        }
    }
    /* The bottom line, the box against the map's right edge: its rows from $40, its columns wrapping. */
    wram7[t->next_text_column.addr - 0xd000u] = 0;
    wram7[t->text_line_tiles.addr - 0xd000u] = 0x60;
    wram7[t->textbox_column.addr - 0xd000u] = 29;
    for (unsigned k = 0; k < 6u; k++) {
        wram0[0xf0] = 2;
        oracles_core_run_frame(core);
    }
    for (unsigned j = 0; j < 6u; j++) {
        const unsigned cell = ((29u + 2u + j) & 0x1fu) + 0x40u;
        CHECK(map[cell] == (j < 3u ? 0x60u + 2u * j : 0x02u) && map[cell + 0x20u] == (j < 3u ? 0x61u + 2u * j : 0x02u));
    }
    /* Twenty-one characters, more than the game's buffers hold: sixteen steps, whose delays add up to the
     * characters', and the second word's ink where the pen puts it. */
    draw_line(core, guest, t, "iiiiiiiiii iiiiiiiiii");
    unsigned delay = 0;
    for (unsigned s = 0; s < 16u; s++) {
        CHECK(text[s] & ORACLES_GUEST_COMPACT_STEP);
        delay += delays[s];
    }
    CHECK(delay == 21u * 4u && (text[15] & ~ORACLES_GUEST_COMPACT_STEP) == 48u);
    for (unsigned x = 0; x < 48u; x++) CHECK(pixel(gfx, x, 5) == ((x < 20u && x % 2u == 0) || (x >= 25u && x < 45u && x % 2u == 1) ? 2u : 3u));
    /* A secret's characters (read from w7SecretText1) on whole tiles, as the game draws them: "iii" a cell each, its ink
     * in the third column of each tile, where the same letters elsewhere take two pixels each. */
    draw_text(core, guest, t, "iii", 1);
    for (unsigned s = 0; s < 3u; s++) CHECK(text[s] == (ORACLES_GUEST_COMPACT_STEP | (s < 2u ? 8u * (s + 1u) : 24u)));
    for (unsigned x = 0; x < 24u; x++) CHECK(pixel(gfx, x, 5) == (x % 8u == 3u ? 2u : 3u));
    draw_line(core, guest, t, "iii");
    CHECK(text[0] == (ORACLES_GUEST_COMPACT_STEP | 2u) && text[1] == (ORACLES_GUEST_COMPACT_STEP | 4u));
    /* Twenty whole cells: the line's 128 pixels hold sixteen, the others are counted, not drawn. */
    draw_line(core, guest, t, "\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f");
    oracles_guest_compact_counts(guest, &counts);
    CHECK(counts.lines == 5 && counts.long_lines == 1 && counts.overflows == 4 && counts.characters == 6u + 21u + 3u + 3u + 16u);
    CHECK((text[15] & ~ORACLES_GUEST_COMPACT_STEP) == 127u && pixel(gfx, 127, 5) == 2);
    /* The reveal to the pixel: "AAA", the second A across the edge of tiles 0 and 1 (7 to 12), the third from 14, revealed
     * by the game's standard text in state 2.  At each vblank the VRAM holds, in the tile columns the map shows, the
     * characters shown whole and nothing of the next: never a letter cut, never one early. */
    draw_line(core, guest, t, "AAA");
    memset(map, 0x02, 5u * 0x20u);
    wram7[t->next_text_column.addr - 0xd000u] = 0;
    wram7[t->text_line_tiles.addr - 0xd000u] = 0x40;
    wram7[t->textbox_column.addr - 0xd000u] = 0;
    wram7[t->text_display_state.addr - 0xd000u] = 2;
    wram0[t->text_is_active.addr - 0xc000u] = 1;
    wram0[t->text_display_mode.addr - 0xc000u] = 0;
    size_t vram_size = 0;
    uint16_t vram_bank = 0;
    const uint8_t *vram = oracles_core_memory(core, ORACLES_CORE_VRAM, &vram_size, &vram_bank);
    CHECK(vram != NULL && vram_size >= 0x1600u);
    const unsigned reached[3] = { 7, 14, 24 }, tiles_shown[3] = { 1, 2, 3 };
    OraclesGuestCompactCounts before;
    oracles_guest_compact_counts(guest, &before);
    for (unsigned k = 0; k < 3u && vram; k++) {
        wram0[0xf0] = 2;
        oracles_core_run_frame(core);
        for (unsigned j = 0; j < 4u; j++) CHECK(map[2u + j] == (j < tiles_shown[k] ? 0x40u + 2u * j : 0x02u));
        for (unsigned x = 0; x < 8u * tiles_shown[k]; x++)
            for (unsigned r = 0; r < 16u; r++) CHECK(pixel(vram + 0x1400u, x, r) == (x < reached[k] ? pixel(gfx, x, r) : 3u));
    }
    oracles_guest_compact_counts(guest, &counts);
    CHECK(counts.reveal_frames - before.reveal_frames >= 3u && counts.partial_writes - before.partial_writes >= 2u);
    CHECK(counts.cut_frames == before.cut_frames && counts.early_frames == before.early_frames);
    /* Pure: the step's columns and the mask of a tile. */
    unsigned whole = 0, visible = 0;
    oracles_guest_compact_step_columns(ORACLES_GUEST_COMPACT_STEP | 7u, 0, &whole, &visible);
    CHECK(whole == 0 && visible == 1);
    oracles_guest_compact_step_columns(ORACLES_GUEST_COMPACT_STEP | 127u, 1, &whole, &visible);
    CHECK(whole == 16 && visible == 16);
    uint8_t tile[32], masked[32];
    memset(tile, 0x00, sizeof tile);
    oracles_guest_compact_mask(tile, 3, masked);
    CHECK(masked[0] == 0x1f && masked[31] == 0x1f);
    oracles_guest_detach(guest);
    oracles_core_destroy(core);
}

/* The line's graphics and buffers, the secrets' texts after them included ($d200 to $d48b), as the line leaves them. */
#define LINE_STATE_BYTES (0x48cu - 0x200u)
static void line_state(OraclesGuest *guest, const OraclesGuestTables *t, uint8_t *out)
{
    memcpy(out, oracles_guest_wram_writable(guest, 7) + (t->text_gfx_buffer.addr - 0xd000u), LINE_STATE_BYTES);
}

/* A savestate taken within drawLineOfText, a vblank falling between two of its characters: loaded, the line goes on and
 * is composed at its return as it was without the load, its characters before the load and after it, nothing written
 * past the line's buffers; loaded without its line (as the states before it were), the game writes the rest past them. */
static void test_load_within_line(OraclesCoreKind kind, OraclesGame game)
{
    const OraclesGuestTables *t = game == ORACLES_GAME_AGES ? &oracles_guest_tables_ages : &oracles_guest_tables_seasons;
    const size_t size = 1024u * 1024u;
    uint8_t *rom = synthetic_rom(t, game, size);
    if (!rom) { failures++; return; }
    const OraclesCoreOptions options = { 0, 0, kind, 0 };
    OraclesCore *core = oracles_core_create(rom, size, &options);
    free(rom);
    const OraclesRomInfo info = { .game = game, .revision = game == ORACLES_GAME_AGES ? ORACLES_ROM_REVISION_AGES_US : ORACLES_ROM_REVISION_SEASONS_US,
                                  .known = 1 };
    OraclesGuest *guest = core ? oracles_guest_attach(core, oracles_compat_find(&info)) : NULL;
    CHECK(guest && oracles_guest_arm_compact_text(guest) == 0);
    if (!guest) { if (core) oracles_core_destroy(core); return; }
    for (unsigned f = 0; f < 200u; f++) oracles_core_run_frame(core);
    uint8_t *wram0 = oracles_guest_wram_writable(guest, 0), *wram7 = oracles_guest_wram_writable(guest, 7);
    memset(wram7 + 0x400u, 0, 0x8cu);
    memset(wram7 + (t->text_gfx_buffer.addr - 0xd000u), 0xff, ORACLES_GUEST_LINE_GFX_BYTES);
    memcpy(wram0 + 0x100u, "AiAiAiA", 8);
    wram0[0xf2] = 0x00; wram0[0xf3] = 0xc1; wram0[0xf4] = 1;
    wram0[0xf0] = 1;
    uint8_t line[ORACLES_GUEST_COMPACT_STATE_SIZE];
    size_t line_size = 0;
    for (unsigned f = 0; f < 3u && !line_size; f++) {
        oracles_core_run_frame(core);
        line_size = oracles_guest_compact_save_state(guest, line, sizeof line);
    }
    CHECK(line_size > 7u);
    const OraclesStateInfo state_info = { "ages", "0123456789abcdef0123456789abcdef01234567", "", NULL, 0, NULL, 0, NULL, 0, line, line_size, NULL, 0, NULL, 0 };
    char error[256];
    uint8_t *state = NULL, *host = NULL;
    size_t state_size = 0, host_size = 0;
    CHECK(oracles_state_serialize(core, &state_info, &state, &state_size, error, sizeof error) == 0);
    for (unsigned f = 0; f < 20u; f++) oracles_core_run_frame(core);
    CHECK(oracles_guest_compact_save_state(guest, line, sizeof line) == 0);
    uint8_t whole[LINE_STATE_BYTES], loaded[LINE_STATE_BYTES];
    line_state(guest, t, whole);
    OraclesGuestCompactCounts counts;
    oracles_guest_compact_counts(guest, &counts);
    CHECK(counts.lines == 1 && counts.characters == 7 && counts.resumed == 0);
    for (unsigned variant = 0; variant < 2u && state; variant++) {
        CHECK(oracles_state_deserialize(core, &state_info, state, state_size, &host, &host_size, error, sizeof error) == 0);
        oracles_guest_reset_execution_state(guest);
        const uint8_t *saved = NULL;
        size_t saved_size = 0;
        CHECK(oracles_state_compact_text_state(state, state_size, &saved, &saved_size) == 0 && saved_size == line_size);
        if (!variant) CHECK(oracles_guest_compact_load_state(guest, saved, saved_size) == 0);
        for (unsigned f = 0; f < 20u; f++) oracles_core_run_frame(core);
        line_state(guest, t, loaded);
        CHECK(variant ? memcmp(loaded, whole, sizeof whole) != 0 : memcmp(loaded, whole, sizeof whole) == 0);
    }
    oracles_guest_compact_counts(guest, &counts);
    CHECK(counts.lines == 2 && counts.characters == 14 && counts.resumed == 1);
    CHECK(oracles_guest_compact_load_state(guest, line, 3) == -1);
    free(state);
    oracles_guest_detach(guest);
    oracles_core_destroy(core);
}

/* A fan game's profile qualifies the compact text, but its routines are not where it binds them in this image (Ages'
 * synthetic one): refused, no point hooked. */
static void test_refused(void)
{
    const size_t size = 1024u * 1024u;
    uint8_t *rom = synthetic_rom(&oracles_guest_tables_ages, ORACLES_GAME_AGES, size);
    if (!rom) { failures++; return; }
    const OraclesCoreOptions options = { 0, 0, ORACLES_CORE_SAMEBOY, 0 };
    OraclesCore *core = oracles_core_create(rom, size, &options);
    free(rom);
    if (!core) { failures++; return; }
    const OraclesRomInfo info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_MOONRISE_1_0_6, .known = 1 };
    OraclesGuest *guest = oracles_guest_attach(core, oracles_compat_find(&info));
    CHECK(guest != NULL && oracles_compat_compact_text(oracles_compat_find(&info)));
    if (guest) {
        CHECK(oracles_guest_arm_compact_text(guest) == -1);
        CHECK(oracles_guest_arm_compact_text(guest) == -1);
        oracles_guest_detach(guest);
    }
    oracles_core_destroy(core);
}

/* The user's US ROMs in $ORACLES_ROM_DIR: each point found once, and the font's widths as the study measured them. */
static void test_roms(const char *dir)
{
    DIR *listing = opendir(dir);
    struct dirent *entry;
    unsigned found = 0;
    while (listing && (entry = readdir(listing))) {
        const size_t n = strlen(entry->d_name);
        if (n < 4 || strcmp(entry->d_name + n - 4, ".gbc")) continue;
        char path[1024], error[128];
        snprintf(path, sizeof path, "%s/%s", dir, entry->d_name);
        size_t size = 0;
        uint8_t *data = oracles_rom_read_file(path, &size, error, sizeof error);
        OraclesRomInfo info;
        const OraclesCompatProfile *profile = data && oracles_rom_identify(data, size, &info, error, sizeof error) == 0 ? oracles_compat_find(&info) : NULL;
        if (profile && oracles_compat_compact_text(profile)) {
            const OraclesGuestTables *t = oracles_compat_guest_tables(profile);
            CHECK(oracles_guest_find_reveal_point(t, data, size) == t->display_next_text_character.addr + 0x3du);
            CHECK(oracles_guest_find_cursor_place_point(t, data, size) == t->text_control_code_option.addr + 0x11u);
            const OraclesRom image = { data, size };
            OraclesTextFont font;
            CHECK(oracles_text_font(&image, oracles_rom_offset(t->font_start.bank, t->font_start.addr), &font) == ORACLES_OK);
            CHECK(font.width['A'] == 6 && font.width['a'] == 6 && font.width['e'] == 5 && font.width['m'] == 6);
            CHECK(font.width['i'] == 1 && font.width['l'] == 1 && font.advance['.'] == 3 && font.advance[' '] == 5);
            found++;
        }
        free(data);
    }
    if (listing) closedir(listing);
    printf("compact text: %u US ROMs in %s\n", found, dir);
}

int main(void)
{
    test_font();
    test_layout();
    test_compose();
    test_find(&oracles_guest_tables_ages);
    test_find(&oracles_guest_tables_seasons);
    const OraclesCoreKind kinds[] = { ORACLES_CORE_SAMEBOY, ORACLES_CORE_MGBA };
    for (unsigned k = 0; k < 2u; k++) {
        test_transaction(kinds[k], ORACLES_GAME_AGES);
        test_transaction(kinds[k], ORACLES_GAME_SEASONS);
        test_load_within_line(kinds[k], ORACLES_GAME_AGES);
        test_load_within_line(kinds[k], ORACLES_GAME_SEASONS);
    }
    test_refused();
    const char *dir = getenv("ORACLES_ROM_DIR");
    if (dir && *dir) test_roms(dir);
    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("compact text: all tests passed\n");
    return 0;
}
