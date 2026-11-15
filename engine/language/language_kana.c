/* The Japanese name entry, in kana (ROM_DATA_FORMATS, section 8.5).
 *
 * The US code of the name entry (runTextInput to textInput_getOutputAddressOffset, bank 2) takes a name's character from
 * the grid as $40 + its index, answers SELECT with a `ret`, offers three options below the grid and no dakuten.  The
 * Japanese ROM's, at about the same place of bank 2 and with the same variables, takes a kana in the font's order
 * (hiragana from $60, katakana from $b0, as wFileSelect.kanaMode says, which SELECT and the bottom row's カナ and かな set,
 * the glyphs loaded again), ー, the space, and ゛ or ゜ on the character at the cursor, or else on the one before it, the
 * cursor going back (func_02_494a, in the US ROM too, which never calls it); its cursor moves on its grid of 12 columns
 * in groups of 5, and its bottom row has five options.  The routines that differ are copied from the player's Japanese
 * ROM into the free tail of bank 2, each named by its place, its size and the SHA-1 of its bytes, the addresses of the
 * Japanese routines they call set to the US ones (`relocations`); a few bytes of the port join them (`tail`, below), and
 * nine places of the US code point to them (`sites`).  In a secret's entry (wFileSelect.textInputMode's bit 7), the
 * routines run the US code: SELECT is a `ret` there still, and the secrets' grid and symbols are those of the US code.
 * The grid and the bottom row the code reads are the Japanese ROM's (language_gfx.c, header $a5). */
#include "language_internal.h"

#include "sha1.h"

#include <string.h>

#define BANK 0x4000u
#define CODE_BANK 0x02u

/* The US routines and places the routines copied call or come back to: runTextInput@gotCharacter, @aButton's `cp $02`
 * after its `ld c,$20 / ld a,(hl)`, @selectionLeft, @selectionRight, @startButton, textInput_getCursorPosition after its
 * `ld a,(wFileSelect.cursorPos)`, func_02_494a, textInput_loadCharacterGfx, its `pop af` after the copies, copyTextCharacters,
 * textInput_getOutputAddress and copyTextCharactersFromHlUntilNull; and runTextInput@lowerOptions, whose
 * `ld a,(wFileSelect.textInputMode)` the routines copy. */
enum { US_GOT_CHARACTER, US_A_COMPARE, US_SELECTION_LEFT, US_SELECTION_RIGHT, US_START_BUTTON, US_CURSOR_BACK, US_DAKUTEN, US_LOAD_GLYPHS,
       US_GLYPHS_BACK, US_COPY_CHARACTERS, US_OUTPUT_ADDRESS, US_UNTIL, US_INPUT_MODE, US_ROUTINES };
#define LOAD_BYTES 3u

/* The nine places of the US code: runTextInput's table of buttons, its entry for SELECT; @aButton's `ld c,$20 / ld a,(hl)`;
 * @nameTable, the bottom row's jump table (`rst_jumpTable` and its first entry); the bottom row's options of a name in
 * @leftOrRight (`ldde $03,$0c`, its d); @startButton's option for a name (`ld a,$02`, its operand);
 * textInput_getCursorPosition's `ld a,(wFileSelect.cursorPos)`; drawNameInputCursors' `ld hl,@nameInputOffsets`, its
 * operand; textInput_mapUpperXToLowerX's `ld hl,@nameTable`, its operand; textInput_loadCharacterGfx's `ldbc $3b,$40`. */
enum { SITE_SELECT, SITE_A, SITE_LOWER, SITE_ROWS, SITE_START, SITE_CURSOR, SITE_OFFSETS, SITE_MAP, SITE_GLYPHS };
static const uint8_t site_bytes[LANGUAGE_KANA_SITES] = { 2, 3, 3, 1, 1, 3, 2, 2, 3 };

/* The Japanese ROM's bytes copied: @aButton's `ld a,(hl) / rrca / and $3f / ld c,a`; @notEnteringSecret, up to
 * @gotCharacter; textInput_getCursorPosition after its `ld a,(wFileSelect.cursorPos)`; @selectButton, @selectKatakana
 * and @selectHiragana, up to their path of a secret; the start of textInput_loadCharacterGfx's copies of a name's
 * glyphs (up to its `ld b,$09`); @jpInputOffsets, the bottom row's cursor's places; textInput_mapUpperXToLowerX's
 * @nameTable; data_02_4a28, the last glyphs (ー, the space, ゛ and ゜). */
enum { JP_CHARACTER, JP_NOT_SECRET, JP_CURSOR, JP_SELECT, JP_GLYPHS, JP_OFFSETS, JP_ROWS, JP_GLYPH_DATA };
static const uint8_t block_bytes[LANGUAGE_KANA_BLOCKS] = { 5, 61, 22, 29, 19, 5, 26, 5 };

static const struct kana_game {
    OraclesGame game;
    unsigned us[US_ROUTINES];
    unsigned jp[US_ROUTINES];   /* the Japanese routine a relocated address names, 0 for those the blocks do not call */
    unsigned sites[LANGUAGE_KANA_SITES];
    unsigned blocks[LANGUAGE_KANA_BLOCKS];
    const char *sha1[LANGUAGE_KANA_BLOCKS];
} games[] = {
    { ORACLES_GAME_AGES,
      { 0x4739, 0x4723, 0x4769, 0x473d, 0x47e5, 0x47fe, 0x494a, 0x49a5, 0x49c9, 0x49cd, 0x4a37, 0x410e, 0x474b },
      { 0, 0, 0, 0, 0, 0, 0x498b, 0x49e6, 0, 0x4a1b, 0x4a8a, 0, 0 },
      { 0x46f2, 0x4720, 0x4754, 0x47a1, 0x47f0, 0x47fb, 0x4842, 0x4900, 0x49b9 },
      { 0x4727, 0x473e, 0x4858, 0x47bb, 0x49f4, 0x48b0, 0x4957, 0x4a28 },
      { "8d31629f0a8510c7fc457036438db9abe5608528", "53475179875e8aac9c903d31aa3fe30bcbeaeecf", "3a06dee76e9fc1f3b9c4c1dfc281ecf9a235c46d",
        "978b7a01cd38ad763918a41fedc84fc4a7d24146", "d6eed887f73a25c54088b9a5d3c6559d9fef2d25", "583d69231aafa3da1f86b01a898eab8bf6aa5e57",
        "f36a442fd4365ccb931d3192e3ab4f5426206a72", "06df3a7cc6cee72328bab049e1aeb796ca019eac" } },
    { ORACLES_GAME_SEASONS,
      { 0x46f9, 0x46e3, 0x4729, 0x46fd, 0x47a5, 0x47be, 0x490a, 0x4965, 0x4989, 0x498d, 0x49f7, 0x40ce, 0x470b },
      { 0, 0, 0, 0, 0, 0, 0x494b, 0x49a6, 0, 0x49db, 0x4a4a, 0, 0 },
      { 0x46b2, 0x46e0, 0x4714, 0x4761, 0x47b0, 0x47bb, 0x4802, 0x48c0, 0x4979 },
      { 0x46e7, 0x46fe, 0x4818, 0x477b, 0x49b4, 0x4870, 0x4917, 0x49e8 },
      { "8d31629f0a8510c7fc457036438db9abe5608528", "72d6804eb5552667a5eb646fed525835f0427e33", "3a06dee76e9fc1f3b9c4c1dfc281ecf9a235c46d",
        "b77f9f309147cf804bfbb44c7bac4ea288d24d80", "421af6b9403f280198cd5f83af501f074b66344a", "583d69231aafa3da1f86b01a898eab8bf6aa5e57",
        "f36a442fd4365ccb931d3192e3ab4f5426206a72", "06df3a7cc6cee72328bab049e1aeb796ca019eac" } },
};

/* The tail, in order: the port's bytes (B), a US routine's address (US), the US bytes of a site (SITE), a block copied
 * (BLOCK), the tail's address of a block's byte (IN), a label of the port's (LABEL), its tail's address (AT), and the
 * US code's `ld a,(nn)` at a US place (LOAD). */
enum { OP_BYTE = 0 << 12, OP_US = 1 << 12, OP_SITE = 2 << 12, OP_BLOCK = 3 << 12, OP_IN = 4 << 12, OP_LABEL = 5 << 12, OP_AT = 6 << 12,
       OP_LOAD = 7 << 12, OP_KIND = 15 << 12 };
#define B(b) (OP_BYTE | (b))
#define US(r) (OP_US | (r))
#define SITE(s) (OP_SITE | (s))
#define BLOCK(k) (OP_BLOCK | (k))
#define IN(k, offset) (OP_IN | (k) << 8 | (offset))
#define LABEL(l) (OP_LABEL | (l))
#define AT(l) (OP_AT | (l))
#define LOAD(r) (OP_LOAD | (r))

/* The port's routines: kanaA, at @aButton's character, kanaCursor, at textInput_getCursorPosition, kanaSelect, SELECT's,
 * kanaLower, the bottom row's jump table, and kanaGlyphs, at textInput_loadCharacterGfx's copies of a name's glyphs.
 * Each of the first three, in a secret's entry, does what the US code did and goes back to it:
 *   kanaA:      ld a,(wFileSelect.textInputMode) / rlca / jr nc,+6 / the US bytes / jp @aButton's cp $02
 *               the Japanese character, its jumps to @gotCharacter landing on / jp @gotCharacter
 *   kanaCursor: ld a,(wFileSelect.textInputMode) / rlca / the US bytes / jp c,the US routine's rest / the Japanese rest
 *   kanaSelect: ld a,(wFileSelect.textInputMode) / rlca / ret c / the Japanese routine, its jump to its path of a secret
 *               landing on / ret
 *   kanaLower:  rst_jumpTable / ←, →, カナ (@selectKatakana), かな (@selectHiragana), fin (@startButton)
 *   kanaGlyphs: the Japanese copies' start / call copyTextCharacters / call copyTextCharactersFromHlUntilNull /
 *               jp the US routine's `pop af`
 * then the Japanese tables. */
enum { LABEL_A, LABEL_CURSOR, LABEL_SELECT, LABEL_LOWER, LABEL_GLYPHS, LABELS };
#define SELECT_KATAKANA 7u
#define SELECT_HIRAGANA 11u
static const uint16_t tail[] = {
    LABEL(LABEL_A), LOAD(US_INPUT_MODE), B(0x07), B(0x30), B(0x06), SITE(SITE_A), B(0xc3), US(US_A_COMPARE),
    BLOCK(JP_CHARACTER), BLOCK(JP_NOT_SECRET), B(0xc3), US(US_GOT_CHARACTER),
    LABEL(LABEL_CURSOR), LOAD(US_INPUT_MODE), B(0x07), SITE(SITE_CURSOR), B(0xda), US(US_CURSOR_BACK), BLOCK(JP_CURSOR),
    LABEL(LABEL_SELECT), LOAD(US_INPUT_MODE), B(0x07), B(0xd8), BLOCK(JP_SELECT), B(0xc9),
    LABEL(LABEL_LOWER), B(0xc7), US(US_SELECTION_LEFT), US(US_SELECTION_RIGHT), IN(JP_SELECT, SELECT_KATAKANA), IN(JP_SELECT, SELECT_HIRAGANA),
    US(US_START_BUTTON),
    LABEL(LABEL_GLYPHS), BLOCK(JP_GLYPHS), B(0xcd), US(US_COPY_CHARACTERS), B(0xcd), US(US_UNTIL), B(0xc3), US(US_GLYPHS_BACK),
    BLOCK(JP_OFFSETS), BLOCK(JP_ROWS), BLOCK(JP_GLYPH_DATA),
};

/* What each site becomes: the SELECT entry kanaSelect; jp kanaA; jp kanaLower; 5 options; the fifth; jp kanaCursor; the
 * Japanese places of the cursor; the Japanese @nameTable; jp kanaGlyphs. */
static const uint16_t site_writes[LANGUAGE_KANA_SITES][2] = {
    { AT(LABEL_SELECT), 0 }, { B(0xc3), AT(LABEL_A) }, { B(0xc3), AT(LABEL_LOWER) }, { B(0x05), 0 }, { B(0x04), 0 },
    { B(0xc3), AT(LABEL_CURSOR) }, { IN(JP_OFFSETS, 0), 0 }, { IN(JP_ROWS, 0), 0 }, { B(0xc3), AT(LABEL_GLYPHS) },
};

/* The addresses in the blocks set to the US routines (US) or to the tail (IN): @notEnteringSecret's calls of
 * textInput_getOutputAddress and, twice, func_02_494a; @selectButton's call of textInput_loadCharacterGfx; the glyphs'
 * call of copyTextCharacters and their `ld hl,data_02_4a28`.  The Japanese address is checked before it is set.  The
 * blocks call nothing else in bank 2; playSound and loadUncompressedGfxHeader, in bank 0, are at the same places and
 * the same in both ROMs. */
static const struct relocation { uint8_t block, offset; uint16_t to; } relocations[] = {
    { JP_NOT_SECRET, 17, US(US_OUTPUT_ADDRESS) }, { JP_NOT_SECRET, 21, US(US_DAKUTEN) }, { JP_NOT_SECRET, 34, US(US_DAKUTEN) },
    { JP_SELECT, 22, US(US_LOAD_GLYPHS) }, { JP_GLYPHS, 12, US(US_COPY_CHARACTERS) }, { JP_GLYPHS, 15, IN(JP_GLYPH_DATA, 0) },
};

static const struct kana_game *game_of(OraclesGame game)
{
    for (size_t i = 0; i < sizeof games / sizeof games[0]; i++) if (games[i].game == game) return &games[i];
    return NULL;
}

/* The address an operation names, from the tail's places of the blocks and labels. */
static unsigned address_of(const struct kana_game *g, uint16_t op, const unsigned *blocks, const unsigned *labels)
{
    switch (op & OP_KIND) {
    case OP_US: return g->us[op & 0xffu];
    case OP_IN: return blocks[op >> 8 & 0xfu] + (op & 0xffu);
    default: return labels[op & 0xffu];
    }
}

int language_kana_make(OraclesGame game, const OraclesRom *jp, const OraclesRom *us, unsigned tail_address, language_kana *out)
{
    const struct kana_game *g = game_of(game);
    unsigned blocks[LANGUAGE_KANA_BLOCKS] = { 0 }, labels[LABELS] = { 0 };
    memset(out, 0, sizeof *out);
    if (!g || tail_address < BANK || tail_address >= 2u * BANK) return -1;
    /* The Japanese blocks, as their SHA-1 names them; the US bytes of the sites. */
    for (unsigned k = 0; k < LANGUAGE_KANA_BLOCKS; k++) {
        const size_t at = oracles_rom_offset(CODE_BANK, g->blocks[k]);
        char sha1[41];
        if (at + block_bytes[k] > jp->size) return -1;
        oracles_sha1_hex(jp->data + at, block_bytes[k], sha1);
        if (strcmp(sha1, g->sha1[k])) return -1;
    }
    for (unsigned s = 0; s < LANGUAGE_KANA_SITES; s++)
        if (oracles_rom_offset(CODE_BANK, g->sites[s]) + site_bytes[s] > us->size) return -1;
    const size_t load = oracles_rom_offset(CODE_BANK, g->us[US_INPUT_MODE]);
    if (load + LOAD_BYTES > us->size || us->data[load] != 0xfau) return -1;
    /* The places of the blocks and the labels, then the bytes. */
    for (unsigned pass = 0; pass < 2u; pass++) {
        size_t n = 0;
        for (size_t i = 0; i < sizeof tail / sizeof tail[0]; i++) {
            const uint16_t op = tail[i];
            const unsigned value = op & 0xffu, here = tail_address + (unsigned)n;
            switch (op & OP_KIND) {
            case OP_BYTE: if (pass) out->tail[n] = (uint8_t)value; n++; break;
            case OP_SITE:
                if (pass) memcpy(out->tail + n, us->data + oracles_rom_offset(CODE_BANK, g->sites[value]), site_bytes[value]);
                n += site_bytes[value];
                break;
            case OP_BLOCK:
                if (n + block_bytes[value] > LANGUAGE_KANA_TAIL_MAX) return -1;
                blocks[value] = here;
                if (pass) {
                    memcpy(out->tail + n, jp->data + oracles_rom_offset(CODE_BANK, g->blocks[value]), block_bytes[value]);
                    out->block_jp[value] = g->blocks[value];
                    out->block_at[value] = here;
                    out->block_bytes[value] = block_bytes[value];
                }
                n += block_bytes[value];
                break;
            case OP_LABEL: labels[value] = here; break;
            case OP_LOAD:
                if (pass) memcpy(out->tail + n, us->data + oracles_rom_offset(CODE_BANK, g->us[value]), LOAD_BYTES);
                n += LOAD_BYTES;
                break;
            default:
                if (pass) {
                    const unsigned address = address_of(g, op, blocks, labels);
                    out->tail[n] = (uint8_t)address;
                    out->tail[n + 1u] = (uint8_t)(address >> 8);
                }
                n += 2u;
            }
            if (n > LANGUAGE_KANA_TAIL_MAX) return -1;
        }
        out->length = n;
    }
    /* The addresses relocated, each where the Japanese ROM names the routine or the table it stands for. */
    for (size_t i = 0; i < sizeof relocations / sizeof relocations[0]; i++) {
        const struct relocation *r = &relocations[i];
        const size_t at = blocks[r->block] - tail_address + r->offset;
        const unsigned named = out->tail[at] | out->tail[at + 1u] << 8;
        const unsigned japanese = (r->to & OP_KIND) == OP_US ? g->jp[r->to & 0xffu] : g->blocks[r->to >> 8 & 0xfu] + (r->to & 0xffu);
        if (!japanese || named != japanese || r->offset + 2u > block_bytes[r->block]) return -1;
        const unsigned address = address_of(g, r->to, blocks, labels);
        out->tail[at] = (uint8_t)address;
        out->tail[at + 1u] = (uint8_t)(address >> 8);
        out->relocated[out->relocations++] = (unsigned)at;
    }
    /* The sites' new bytes. */
    for (unsigned s = 0; s < LANGUAGE_KANA_SITES; s++) {
        out->site[s] = g->sites[s];
        out->site_bytes[s] = site_bytes[s];
        unsigned n = 0;
        for (unsigned w = 0; w < 2u && site_writes[s][w]; w++) {
            const uint16_t op = site_writes[s][w];
            if ((op & OP_KIND) == OP_BYTE) out->written[s][n++] = (uint8_t)op;
            else {
                const unsigned address = address_of(g, op, blocks, labels);
                out->written[s][n++] = (uint8_t)address;
                out->written[s][n++] = (uint8_t)(address >> 8);
            }
        }
        if (n != site_bytes[s]) return -1;
    }
    return 0;
}

int language_kana_patch(const language_kana *kana, unsigned address, uint8_t *code, size_t size)
{
    for (unsigned s = 0; s < LANGUAGE_KANA_SITES; s++) {
        if (kana->site[s] < address || kana->site[s] + kana->site_bytes[s] > address + size) return -1;
        memcpy(code + (kana->site[s] - address), kana->written[s], kana->site_bytes[s]);
    }
    return 0;
}
