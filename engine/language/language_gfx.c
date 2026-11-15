/* The graphics that carry text: found in the European ROM, served in the composed image.
 *
 * Both games load a screen's graphics through loadGfxHeader: header h is a list of entries, each a compressed graphic
 * (its bank and compression mode, its address), the place it is written to in VRAM or WRAM and its size
 * (ROM_DATA_FORMATS, sections 4.1 and 9.1).  The European loadGfxHeader reads the same headers with one more kind of
 * entry: a first byte 0, then five sources, one per language (wActiveLanguage), then the destination and the size.
 *
 * Extraction.  A header the European ROM chooses by language is loaded, on paper, as the European game does in each
 * language and as the US game does: the bytes that differ between the languages are those that carry text.  A US
 * entry that is the last to write such bytes names a US graphic (its bank and address); the language's version of
 * that graphic is the US one with every byte it is the last to write taken from the European load in the language.
 * The European ROM split some graphics (the text in one entry, the rest in another) and redrew a few in English too:
 * comparing the loads, not the entries, takes both in.  A US graphic that a header the European ROM does not choose
 * by language also loads is left alone (Seasons' castle tileset shares a header index the European ROM gave to text),
 * as is one two headers would want different.  The pack holds each graphic that differs from the US one, compressed
 * here, in the mode that makes it shortest.
 *
 * Composition.  The 6-bit bank of a header (decompressGraphics: `and $3f`) reaches only the first megabyte, not the
 * language's banks: a language's graphics go where the US graphics they replace were, every header that points to
 * one of those now pointing elsewhere, and in the free tails of the graphics banks (`tails`, below); each header
 * entry that named a replaced graphic is repointed (its bank, mode and address; the destination and the size stay).
 * Data only, no code changed but the operand of a pointer to a table a language moves (pointers, below); nothing at all
 * without a language.
 *
 * The Polish and the Japanese ROMs' graphics are found the same way, their loads against the US ones, in the headers of
 * the screens they translate (polish_headers, japanese_headers, below); and a few tables that no header names come with
 * them, written raw at a pinned place (raw_tables): the Polish title's sprites of PRESS START, the Japanese title's
 * colours and its sprites, the furigana among them, the code's pointer to those set to their new place. */
#include "language_internal.h"

#include "guest_tables.h"
#include "sha1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BANK 0x4000u
#define ENTRY 6u                /* a header's entry */
#define CHOICE 19u              /* a European entry chosen by language: 0, five sources, destination, size */
#define MAX_ENTRIES 64u         /* a header's entries, fall-throughs included */
#define GFX_BYTES 2048u         /* a graphic's largest size: 128 blocks */
#define SPACE_BANKS 16u         /* a destination's bank (its low 4 bits): VRAM 0-1, WRAM 0-7 */
#define SPACE_FIRST 0x8000u     /* VRAM and WRAM: $8000-$dfff */
#define SPACE_BYTES 0x6000u
#define UNWRITTEN 0xffffu

typedef struct gfx_entry {
    unsigned bank, address, mode, dest, blocks;
    size_t at;                  /* where the entry lies in the ROM */
} gfx_entry;

/* ---- the compression (ROM_DATA_FORMATS, section 9.1) -------------------------------------------------------------- */

int language_gfx_decompress(const uint8_t *rom, size_t rom_size, size_t at, unsigned mode, uint8_t *out, size_t size, size_t *consumed)
{
    size_t p = at, n = 0;
    uint8_t v = 0;
#define NEXT(v) do { if (p >= rom_size) return -1; (v) = rom[p++]; } while (0)
#define PUT(i, v) do { if (out) out[i] = (uint8_t)(v); } while (0)
    if (mode == 0) {
        if (size > rom_size - at || at > rom_size) return -1;
        if (out) memcpy(out, rom + at, size);
        p += size;
    } else if (mode == 2) {
        while (n < size) {
            unsigned keys[2], common = 0;
            NEXT(keys[0]);
            NEXT(keys[1]);
            if (!(keys[0] | keys[1])) {
                for (unsigned i = 0; i < 16u; i++) { NEXT(v); PUT(n + i, v); }
            } else {
                NEXT(common);
                for (unsigned i = 0; i < 16u; i++) {
                    if (keys[i / 8u] << (i % 8u) & 0x80u) PUT(n + i, common);
                    else { NEXT(v); PUT(n + i, v); }
                }
            }
            n += 16u;
        }
    } else if (mode == 1 || mode == 3) {
        unsigned flags = 0, left = 0;
        while (n < size) {
            if (!left) { NEXT(flags); left = 8; }
            left--;
            const unsigned reference = flags & 0x80u;
            flags = flags << 1 & 0xffu;
            if (!reference) { NEXT(v); PUT(n, v); n++; continue; }
            unsigned distance, length, b0, b1 = 0;
            NEXT(b0);
            if (mode == 1) {
                distance = (b0 & 0x1fu) + 1u;
                length = b0 >> 5;
                if (length) length++;
                else NEXT(length);
            } else {
                NEXT(b1);
                distance = ((b1 & 0x07u) << 8 | b0) + 1u;
                length = b1 >> 3;
                if (length) length += 2u;
                else NEXT(length);
            }
            if (!length) length = 256u;
            if (out && distance > n) return -1;
            for (unsigned i = 0; i < length && n < size; i++, n++) PUT(n, out[n - distance]);
        }
    } else return -1;
#undef PUT
#undef NEXT
    if (consumed) *consumed = p - at;
    return 0;
}

/* Mode 2: a tile, 16 bytes, is a key of 16 bits and the byte the most frequent in it, then the others; or a key of 0
 * and the 16 bytes. */
static size_t compress_common(const uint8_t *data, size_t size, uint8_t *out, size_t capacity)
{
    size_t n = 0;
    for (size_t t = 0; t < size; t += 16u) {
        const uint8_t *tile = data + t;
        unsigned best = 0, count = 0;
        for (unsigned i = 0; i < 16u; i++) {
            unsigned c = 0;
            for (unsigned j = 0; j < 16u; j++) c += tile[j] == tile[i];
            if (c > count || (c == count && tile[i] < best)) { count = c; best = tile[i]; }
        }
        if (n + 19u > capacity) return 0;
        if (count < 2u) {
            out[n++] = 0;
            out[n++] = 0;
            memcpy(out + n, tile, 16);
            n += 16u;
            continue;
        }
        unsigned key = 0;
        for (unsigned i = 0; i < 16u; i++) if (tile[i] == best) key |= 0x8000u >> i;
        out[n++] = (uint8_t)(key >> 8);
        out[n++] = (uint8_t)key;
        out[n++] = (uint8_t)best;
        for (unsigned i = 0; i < 16u; i++) if (tile[i] != best) out[n++] = tile[i];
    }
    return n;
}

/* Modes 1 and 3: the cheapest sequence of literals and references, by dynamic programming from the end; a reference
 * of length L at the nearest distance whose match is the longest.  Costs in bits: a literal 9, a reference 9 (mode 1,
 * length 2-8) or 17 (mode 3, length 3-33), 8 more with a length byte (1-256).  The distances tried are those of the
 * earlier places that start with the same two bytes, nearest first (a chain per pair of bytes); the search stops at
 * the first match as long as a match can be. */
static size_t compress_lz(const uint8_t *data, size_t size, unsigned mode, uint8_t *out, size_t capacity)
{
    const unsigned far = mode == 3 ? 0x800u : 0x20u;
    uint32_t *cost = malloc((size + 1u) * sizeof *cost);
    uint16_t *length = malloc((size + 1u) * sizeof *length), *distance = malloc((size + 1u) * sizeof *distance);
    uint16_t *earlier = malloc((size + 1u) * sizeof *earlier), *last = malloc(0x10000u * sizeof *last);
    size_t n = 0;
    if (!cost || !length || !distance || !earlier || !last) goto done;
    /* earlier[i]: the last place before i whose two bytes are those at i, UNWRITTEN for none. */
    memset(last, 0xff, 0x10000u * sizeof *last);
    for (size_t i = 0; i + 1u < size; i++) {
        const unsigned pair = (unsigned)data[i] << 8 | data[i + 1u];
        earlier[i] = last[pair];
        last[pair] = (uint16_t)i;
    }
    cost[size] = 0;
    for (size_t i = size; i-- > 0;) {
        cost[i] = 9u + cost[i + 1u];
        length[i] = 1;
        distance[i] = 0;
        unsigned longest = 0, at = 0;
        const unsigned most = size - i < 256u ? (unsigned)(size - i) : 256u;
        for (size_t j = i + 1u < size ? earlier[i] : UNWRITTEN; j != UNWRITTEN && i - j <= far && longest < most; j = earlier[j]) {
            const unsigned d = (unsigned)(i - j);
            unsigned l = 0;
            while (l < most && data[i + l] == data[i + l - d]) l++;
            if (l > longest) { longest = l; at = d; }
        }
        for (unsigned l = 2; l <= longest; l++) {
            const unsigned bits = mode == 3 ? (l >= 3u && l <= 33u ? 17u : 25u) : (l <= 8u ? 9u : 17u);
            if (bits + cost[i + l] < cost[i]) { cost[i] = bits + cost[i + l]; length[i] = (uint16_t)l; distance[i] = (uint16_t)at; }
        }
    }
    size_t flags = 0;
    unsigned item = 8;
    for (size_t i = 0; i < size; i += length[i]) {
        if (n + 4u > capacity) { n = 0; goto done; }
        if (item == 8u) { flags = n; out[n++] = 0; item = 0; }
        const unsigned l = length[i], d = distance[i];
        if (!d) out[n++] = data[i];
        else {
            out[flags] |= (uint8_t)(0x80u >> item);
            if (mode == 3) {
                const unsigned w = (d - 1u) | (l >= 3u && l <= 33u ? (l - 2u) << 11 : 0u);
                out[n++] = (uint8_t)w;
                out[n++] = (uint8_t)(w >> 8);
                if (l < 3u || l > 33u) out[n++] = (uint8_t)l;
            } else if (l <= 8u) out[n++] = (uint8_t)((d - 1u) | (l - 1u) << 5);
            else { out[n++] = (uint8_t)(d - 1u); out[n++] = (uint8_t)l; }
        }
        item++;
    }
done:
    free(cost);
    free(length);
    free(distance);
    free(earlier);
    free(last);
    return n;
}

size_t language_gfx_compress(const uint8_t *data, size_t size, uint8_t *out, size_t capacity, unsigned *mode)
{
    if (!size || size % 16u || size > GFX_BYTES || capacity < size) return 0;
    uint8_t *trial = malloc(size);
    if (!trial) return 0;
    memcpy(out, data, size);
    size_t best = size;
    *mode = 0;
    for (unsigned m = 1; m <= 3u; m++) {
        const size_t n = m == 2 ? compress_common(data, size, trial, size) : compress_lz(data, size, m, trial, size);
        if (n && n < best) { best = n; *mode = m; memcpy(out, trial, n); }
    }
    free(trial);
    return best;
}

/* ---- the headers ------------------------------------------------------------------------------------------------- */

/* The entries of header `h` of the table at `table` (physical); a European entry chosen by language gives its source in
 * the language of index `european` (language_european_index).  The number of entries, or -1 when the header leaves the
 * ROM or does not end. */
static int header_entries(const uint8_t *rom, size_t size, size_t table, unsigned h, unsigned european, gfx_entry *out)
{
    if (table + 2u * h + 2u > size) return -1;
    const unsigned pointer = rom[table + 2u * h] | rom[table + 2u * h + 1u] << 8;
    if (pointer < BANK || pointer >= 2u * BANK) return -1;
    size_t at = table / BANK * BANK + (pointer - BANK);
    for (unsigned n = 0; n < MAX_ENTRIES; n++) {
        if (at + CHOICE > size) return -1;
        const uint8_t *e = rom + at;
        gfx_entry *x = &out[n];
        x->at = at;
        const uint8_t *source = e, *rest = e + 3;
        if (!e[0]) { source = e + 1 + 3u * european; rest = e + 16; at += CHOICE; }
        else at += ENTRY;
        x->bank = source[0] & 0x3fu;
        x->mode = source[0] >> 6;
        x->address = (unsigned)source[1] << 8 | source[2];
        x->dest = (unsigned)rest[0] << 8 | rest[1];
        x->blocks = rest[2] & 0x7fu;
        if (!(rest[2] & 0x80u)) return (int)n + 1;
    }
    return -1;
}

/* Whether a header of the European ROM has an entry chosen by language. */
static int chosen_by_language(const uint8_t *rom, size_t size, size_t table, unsigned h)
{
    gfx_entry entries[MAX_ENTRIES];
    const int n = header_entries(rom, size, table, h, 0, entries);
    for (int i = 0; i < n; i++) if (!rom[entries[i].at]) return 1;
    return 0;
}

static size_t source_of(const gfx_entry *e) { return oracles_rom_offset(e->bank, e->address); }

/* The place of a destination's byte i in the memory model, or SIZE_MAX outside VRAM and WRAM. */
static size_t place(unsigned dest, unsigned i)
{
    const unsigned address = (dest & 0xfff0u) + i;
    if (address < SPACE_FIRST || address >= SPACE_FIRST + SPACE_BYTES) return (size_t)-1;
    return (size_t)(dest & 0xfu) * SPACE_BYTES + (address - SPACE_FIRST);
}

/* Loads the entries into `memory` (UNWRITTEN where nothing was written), or the index of the last entry to write each
 * byte into `writer`; 0, or -1 when an entry does not decompress or writes outside VRAM and WRAM. */
static int load(const uint8_t *rom, size_t size, const gfx_entry *entries, int count, uint16_t *memory, int16_t *writer)
{
    uint8_t data[GFX_BYTES];
    for (int j = 0; j < count; j++) {
        const gfx_entry *e = &entries[j];
        const size_t bytes = 16u * (e->blocks + 1u);
        if (!e->bank || language_gfx_decompress(rom, size, source_of(e), e->mode, data, bytes, NULL) != 0) return -1;
        for (unsigned i = 0; i < bytes; i++) {
            const size_t k = place(e->dest, i);
            if (k == (size_t)-1) return -1;
            if (memory) memory[k] = data[i];
            if (writer) writer[k] = (int16_t)j;
        }
    }
    return 0;
}

/* Clears what the entries wrote. */
static void unload(const gfx_entry *entries, int count, uint16_t *memory, int16_t *writer)
{
    for (int j = 0; j < count; j++)
        for (unsigned i = 0; i < 16u * (entries[j].blocks + 1u); i++) {
            const size_t k = place(entries[j].dest, i);
            if (k == (size_t)-1) continue;
            if (memory) memory[k] = UNWRITTEN;
            if (writer) writer[k] = -1;
        }
}

/* ---- the extraction ---------------------------------------------------------------------------------------------- */

typedef struct graphic {
    unsigned bank, address, mode, blocks;
    int excluded;                                  /* also loaded where the European ROM does not choose by language (where
                                                      the Polish ROM translates nothing) */
    int found;                                     /* a header the European ROM chooses by language loads text into it */
    int conflict;                                  /* two such headers want different bytes */
    int raw;                                       /* a table written in place, raw (press_starts) */
    uint8_t *data[ORACLES_LANGUAGE_COUNT];         /* the US graphic [ORACLES_LANGUAGE_ENGLISH], each language's */
    uint8_t *set[ORACLES_LANGUAGE_COUNT];          /* the bytes a European (Polish) load decided */
} graphic;

typedef struct extraction {
    const OraclesRom *eu, *us;                     /* the European ROM, or the Polish one */
    size_t eu_table, us_table;
    graphic *graphics;
    unsigned count, capacity;
    uint16_t *memory[EUROPEAN_LANGUAGES];          /* by the European ROM's index of a language; the Polish load, the US one */
    int16_t *writer;
} extraction;

static graphic *graphic_of(extraction *x, const gfx_entry *e)
{
    for (unsigned i = 0; i < x->count; i++)
        if (x->graphics[i].bank == e->bank && x->graphics[i].address == e->address) return &x->graphics[i];
    if (x->count == x->capacity) {
        const unsigned capacity = x->capacity ? 2u * x->capacity : 64u;
        graphic *grown = realloc(x->graphics, capacity * sizeof *grown);
        if (!grown) return NULL;
        x->graphics = grown;
        x->capacity = capacity;
    }
    graphic *g = &x->graphics[x->count++];
    memset(g, 0, sizeof *g);
    g->bank = e->bank;
    g->address = e->address;
    g->mode = e->mode;
    g->blocks = e->blocks;
    return g;
}

/* What a header is to an extraction: one whose text it reads, another, or one the US games never load. */
enum { HEADER_OTHER = 0, HEADER_TEXT, HEADER_UNUSED };

/* The US graphics every US header loads, the largest size each is loaded with, and those also loaded by a header the
 * European ROM does not choose by language (`language_headers`, HEADER_TEXT for those it does). */
static int list_graphics(extraction *x, const uint8_t *language_headers)
{
    gfx_entry entries[MAX_ENTRIES];
    for (unsigned h = 0; h < GFX_HEADERS; h++) {
        if (language_headers[h] == HEADER_UNUSED) continue;
        const int n = header_entries(x->us->data, x->us->size, x->us_table, h, 0, entries);
        if (n < 0) return -1;
        for (int j = 0; j < n; j++) {
            graphic *g = graphic_of(x, &entries[j]);
            if (!g) return -1;
            if (entries[j].blocks > g->blocks) g->blocks = entries[j].blocks;
            if (entries[j].mode != g->mode) g->excluded = 1;
            if (!language_headers[h]) g->excluded = 1;
        }
    }
    return 0;
}

/* Header h, which the European ROM chooses by language: the bytes of each US graphic that carry text there. */
static int extract_header(extraction *x, unsigned h)
{
    gfx_entry us[MAX_ENTRIES], eu[EUROPEAN_LANGUAGES][MAX_ENTRIES];
    int eu_count[EUROPEAN_LANGUAGES];
    const int us_count = header_entries(x->us->data, x->us->size, x->us_table, h, 0, us);
    int status = us_count > 0 ? load(x->us->data, x->us->size, us, us_count, NULL, x->writer) : -1;
    for (unsigned l = 0; l < EUROPEAN_LANGUAGES; l++) {
        eu_count[l] = header_entries(x->eu->data, x->eu->size, x->eu_table, h, l, eu[l]);
        if (status == 0) status = eu_count[l] > 0 ? load(x->eu->data, x->eu->size, eu[l], eu_count[l], x->memory[l], NULL) : -1;
    }
    for (int j = 0; status == 0 && j < us_count; j++) {
        const gfx_entry *e = &us[j];
        const unsigned bytes = 16u * (e->blocks + 1u);
        int text = 0;
        for (unsigned i = 0; i < bytes && !text; i++) {
            const size_t k = place(e->dest, i);
            if (x->writer[k] != j) continue;
            for (unsigned l = 1; l < EUROPEAN_LANGUAGES; l++) text |= x->memory[l][k] != x->memory[0][k];
        }
        if (!text) continue;
        graphic *g = graphic_of(x, e);
        if (!g) { status = -1; break; }
        if (g->excluded) continue;
        const size_t size = 16u * (g->blocks + 1u);
        if (!g->found) {
            for (unsigned l = 0; l < ORACLES_LANGUAGE_EUROPEAN_END && status == 0; l++) {
                g->data[l] = malloc(size);
                g->set[l] = calloc(size, 1);
                if (!g->data[l] || !g->set[l]) status = -1;
            }
            uint8_t *us_graphic = g->data[ORACLES_LANGUAGE_ENGLISH];
            if (status == 0 && language_gfx_decompress(x->us->data, x->us->size, source_of(e), g->mode, us_graphic, size, NULL) != 0) status = -1;
            for (unsigned l = ORACLES_LANGUAGE_ENGLISH_EU; l < ORACLES_LANGUAGE_EUROPEAN_END && status == 0; l++) memcpy(g->data[l], us_graphic, size);
            g->found = 1;
        }
        for (unsigned i = 0; i < bytes && status == 0; i++) {
            const size_t k = place(e->dest, i);
            if (x->writer[k] != j) continue;
            for (unsigned l = ORACLES_LANGUAGE_ENGLISH_EU; l < ORACLES_LANGUAGE_EUROPEAN_END; l++) {
                const uint16_t *memory = x->memory[language_european_index((OraclesLanguage)l)];
                if (memory[k] == UNWRITTEN) continue;
                if (g->set[l][i] && g->data[l][i] != memory[k]) g->conflict = 1;
                g->data[l][i] = (uint8_t)memory[k];
                g->set[l][i] = 1;
            }
        }
    }
    if (us_count > 0) unload(us, us_count, NULL, x->writer);
    for (unsigned l = 0; l < EUROPEAN_LANGUAGES; l++) if (eu_count[l] > 0) unload(eu[l], eu_count[l], x->memory[l], NULL);
    return status;
}

static int by_source(const void *a, const void *b)
{
    const graphic *x = a, *y = b;
    const unsigned p = x->bank << 16 | x->address, q = y->bank << 16 | y->address;
    return (p > q) - (p < q);
}

/* The graphics found, by increasing source, added to the pack in each language from `first` to `end` (excluded) whose
 * version differs from the US one; 0 or -1. */
static int add_graphics(OraclesLanguagePack *pack, extraction *x, OraclesLanguage first, OraclesLanguage end)
{
    uint8_t *compressed = malloc(GFX_BYTES);
    int status = compressed ? 0 : -1;
    if (status == 0) qsort(x->graphics, x->count, sizeof *x->graphics, by_source);
    for (unsigned i = 0; i < x->count && status == 0; i++) {
        const graphic *g = &x->graphics[i];
        if (!g->found || g->conflict) continue;
        const size_t size = 16u * (g->blocks + 1u);
        for (unsigned l = first; l < end && status == 0; l++) {
            if (!memcmp(g->data[l], g->data[ORACLES_LANGUAGE_ENGLISH], size)) continue;
            unsigned mode = 0;
            size_t length = size;
            if (g->raw) memcpy(compressed, g->data[l], size);
            else length = language_gfx_compress(g->data[l], size, compressed, GFX_BYTES, &mode);
            const OraclesLanguageGraphic made = { (uint8_t)g->bank, (uint16_t)g->address, (uint8_t)g->blocks, (uint8_t)mode, (uint16_t)length, compressed };
            if (!length || language_pack_add_graphic(pack, (OraclesLanguage)l, &made) != 0) status = -1;
        }
    }
    free(compressed);
    return status;
}

static void extraction_free(extraction *x)
{
    for (unsigned i = 0; i < x->count; i++)
        for (unsigned l = 0; l < ORACLES_LANGUAGE_COUNT; l++) { free(x->graphics[i].data[l]); free(x->graphics[i].set[l]); }
    free(x->graphics);
    for (unsigned l = 0; l < EUROPEAN_LANGUAGES; l++) free(x->memory[l]);
    free(x->writer);
}

int language_gfx_extract(OraclesLanguagePack *pack, const OraclesRom *eu, const OraclesRom *us, char *error, size_t capacity)
{
    extraction x = { eu, us, language_gfx_headers_european(pack->game), language_gfx_headers_us(pack->game), NULL, 0, 0, { NULL }, NULL };
    uint8_t language_headers[GFX_HEADERS];
    int status = x.eu_table && x.us_table ? 0 : -1;
    for (unsigned l = 0; l < EUROPEAN_LANGUAGES && status == 0; l++) {
        x.memory[l] = malloc(SPACE_BANKS * SPACE_BYTES * sizeof *x.memory[l]);
        if (!x.memory[l]) status = -1;
        else for (size_t i = 0; i < SPACE_BANKS * SPACE_BYTES; i++) x.memory[l][i] = UNWRITTEN;
    }
    if (status == 0 && !(x.writer = malloc(SPACE_BANKS * SPACE_BYTES * sizeof *x.writer))) status = -1;
    if (status == 0) for (size_t i = 0; i < SPACE_BANKS * SPACE_BYTES; i++) x.writer[i] = -1;
    for (unsigned h = 0; h < GFX_HEADERS && status == 0; h++) language_headers[h] = (uint8_t)chosen_by_language(eu->data, eu->size, x.eu_table, h);
    if (status == 0) status = list_graphics(&x, language_headers);
    for (unsigned h = 0; h < GFX_HEADERS && status == 0; h++) if (language_headers[h]) status = extract_header(&x, h);
    if (status == 0) status = add_graphics(pack, &x, ORACLES_LANGUAGE_ENGLISH_EU, ORACLES_LANGUAGE_EUROPEAN_END);
    extraction_free(&x);
    if (status != 0) snprintf(error, capacity, "the graphics of the European ROM cannot be read");
    return status;
}

/* The objects' graphics headers (objectGfxHeaderTable) and the trees' (treeGfxHeaderTable), which loadObjectGfx reads: 3
 * bytes a header, its graphic's bank and mode, then its address, big-endian, the high bit of which chains the object's next
 * header.  Each loads 32 blocks (512 bytes), whatever its graphic's size: the load of a smaller graphic reads on past
 * its end (ROM_DATA_FORMATS, sections 4.1 and 16).  In bank $3f, where the code names them: `ld hl,objectGfxHeaderTable+1`
 * ($41d4), `ld hl,treeGfxHeaderTable` ($42f5) and `ld hl,objectGfxHeaderTable` ($42ff), in both games. */
#define OBJECT_BLOCKS 32u
#define OBJECT_BANK 0x3fu
#define OBJECT_HEADERS_MAX 0x100u
static const struct object_tables { OraclesGame game; unsigned objects, object_count, trees, tree_count; } object_tables[] = {
    { ORACLES_GAME_AGES, 0x5a8a, 0xe0, 0x5d2a, 0x0b },
    { ORACLES_GAME_SEASONS, 0x5afb, 0xc7, 0x5d50, 0x0b },
};

/* The places of the image's objects' and trees' headers, 3 bytes each, into `at`; their number, or 0 when the image's
 * code does not name the US tables. */
static unsigned object_headers_of(OraclesGame game, const uint8_t *image, size_t size, size_t at[OBJECT_HEADERS_MAX])
{
    const struct object_tables *t = NULL;
    for (size_t i = 0; i < sizeof object_tables / sizeof object_tables[0]; i++) if (object_tables[i].game == game) t = &object_tables[i];
    if (!t || t->object_count + t->tree_count > OBJECT_HEADERS_MAX) return 0;
    const struct { unsigned address, names; } operands[] = { { 0x41d4, t->objects + 1u }, { 0x42f5, t->trees }, { 0x42ff, t->objects } };
    for (size_t i = 0; i < sizeof operands / sizeof operands[0]; i++) {
        const size_t o = oracles_rom_offset(OBJECT_BANK, operands[i].address);
        if (o + 3u > size || image[o] != 0x21u || ((unsigned)image[o + 1u] | (unsigned)image[o + 2u] << 8) != operands[i].names) return 0;
    }
    unsigned n = 0;
    for (unsigned i = 0; i < t->object_count; i++) at[n++] = oracles_rom_offset(OBJECT_BANK, t->objects + 3u * i);
    for (unsigned i = 0; i < t->tree_count; i++) at[n++] = oracles_rom_offset(OBJECT_BANK, t->trees + 3u * i);
    return at[n - 1u] + 3u <= size ? n : 0;
}

/* ---- the Polish and Japanese ROMs' graphics ------------------------------------------------------------------- */

/* The headers of the screens the Polish ROM translates: the title, the file select and its screens, the save menu, the
 * secrets' screens, the end of the game and the error, the dungeons' names.  The Polish ROM is rebuilt from the
 * disassembly: its headers are the US ones, at another place and pointing elsewhere, and it redraws graphics that carry
 * no text too (the HUD, the inventory, the rings, tilesets), which those headers leave out.  The Japanese intro's header,
 * which only the Japanese ROMs load, counts as no header. */
static void polish_headers(OraclesGame game, uint8_t marks[GFX_HEADERS])
{
    static const uint8_t screens[] = { 0x02, 0x03, 0x05, 0x06, 0x07, 0x97, 0x9f, 0xba };
    memset(marks, HEADER_OTHER, GFX_HEADERS);
    for (size_t i = 0; i < sizeof screens; i++) marks[screens[i]] = HEADER_TEXT;
    for (unsigned h = 0x10; h <= 0x1f; h++) marks[h] = HEADER_TEXT;
    for (unsigned h = 0xa0; h <= 0xaf; h++) marks[h] = HEADER_TEXT;
    if (game == ORACLES_GAME_SEASONS) marks[0x84] = HEADER_TEXT;
    marks[0x04] = HEADER_UNUSED;
}

/* The headers of the screens the Japanese ROM draws in Japanese: the title and the end of the intro, the secrets'
 * screens, the error, the inventory's labels (the save's, Ages' present and past, Seasons' seasons), Ages' maps' label of
 * the present and the past, the dungeons' names, the secret for the linked game, the file select and its screens, the
 * save menu.  The Japanese ROM's headers are the US ones, at another place, with the same entries, destinations and
 * sizes; its graphics differ in screens without text too, or drawn otherwise (the credits' fonts, "to be continued",
 * which the US edition added, the DMG screen, Seasons' pirate ship), which those headers leave out.  Header $04 is the
 * Japanese intro, which the US games never load. */
static void japanese_headers(OraclesGame game, uint8_t marks[GFX_HEADERS])
{
    /* $a2 and $a6 load nothing the Japanese ROM draws otherwise: in the range, without effect. */
    static const uint8_t screens[] = { 0x02, 0x03, 0x05, 0x06, 0x07, 0x08, 0x97, 0x9f, 0xba };
    memset(marks, HEADER_OTHER, GFX_HEADERS);
    for (size_t i = 0; i < sizeof screens; i++) marks[screens[i]] = HEADER_TEXT;
    for (unsigned h = 0x10; h <= (game == ORACLES_GAME_AGES ? 0x1fu : 0x1bu); h++) marks[h] = HEADER_TEXT;
    for (unsigned h = 0xa0; h <= 0xaf; h++) marks[h] = HEADER_TEXT;
    if (game == ORACLES_GAME_AGES) marks[0x0d] = marks[0x0e] = HEADER_TEXT;
    if (game == ORACLES_GAME_SEASONS) marks[0x84] = HEADER_TEXT;
    marks[0x04] = HEADER_UNUSED;
}

/* The entries of those screens that keep the US graphics: the layout of the secret entry's grid (the US code takes the
 * character at the cursor's place from the secrets' symbols, where the Japanese grid has kana, which its own code
 * picks), which the US layout made for the Japanese font replaces instead (japanese_grids, below).  The name entry's grid
 * and bottom row are the Japanese ROM's, which the Japanese routines read (language_kana.c).  In both games, by header
 * and entry. */
static const struct kept { uint8_t header, entry; } japanese_kept[] = {
    { 0xab, 2 }, { 0xab, 3 },                             /* map_secret_entry_middle, flg_secret_entry_middle */
};

/* The tables written raw at a pinned place of the US ROM, which no header names, each a raw graphic of whole blocks no
 * larger than the place: the Polish ROM's title's sprites of PRESS START (titlescreenPressStartSprites), drawn in fewer
 * sprites, over the US table (the Polish table, then the US bytes past it); the Japanese ROM's title's colours,
 * PALH_03's (eight palettes for the background, eight for the sprites, which no other palette header reads), over the US
 * ones; the Japanese ROM's title's sprites (titlescreenMakuSeedSprite: the seed, the logo's sprites and the furigana
 * over its kanji, 25 where the US ROMs have 19 and 18), in the free tail of the bank the title screen reads them from
 * (ROM_DATA_FORMATS, section 15), the code's pointer to them pointed there (pointers, below); the Japanese ROM's
 * secrets: their cipher (secretXorCipher), over the US one, and their 64 symbols (secretSymbols, bank 0: its place and
 * its terminator, which textInput_loadCharacterGfx copies up to, held, the terminator kept), hiragana and digits where the US ROMs have Latin letters, digits and signs, which the US code reads as it does
 * its own when it makes a secret, reads one typed and shows the secret entry's grid; and the names of the secrets' list
 * (secretTextTable, bank 2: its 26 pointers, then the names), its first, which only the Japanese ROM's screen for the
 * DMG prints, set to the second, the dashes, as in Ages' US ROM; the name entry in kana (language_kana.c): the US code
 * of the name entry (runTextInput to textInput_getOutputAddressOffset), its nine sites changed (its first 880 bytes
 * written, a whole number of blocks past the last site), and the routines in the free tail of bank 2 (ROM_DATA_FORMATS,
 * section 15), all of which the image must still hold filled, with the US routine they call outside the US code,
 * copyTextCharactersFromHlUntilNull (RAW_KANA_CALLED, which nothing writes).  Each is written only where the image
 * still holds the US bytes, which their SHA-1 pins. */
enum { RAW_PRESS_START, RAW_TITLE_COLOURS, RAW_TITLE_SPRITES, RAW_SECRET_CIPHER, RAW_SECRET_SYMBOLS, RAW_SECRET_NAMES, RAW_KANA_ENTRY, RAW_KANA_TAIL,
       RAW_KANA_CALLED };
static const struct raw_table { OraclesGame game; unsigned kind, bank, address, size; const char *us; } raw_tables[] = {
    { ORACLES_GAME_AGES, RAW_PRESS_START, 0x3f, 0x59aa, 0x29, "76528423897159cc4b023c8928b698f63b5db095" },
    { ORACLES_GAME_SEASONS, RAW_PRESS_START, 0x03, 0x4e65, 0x29, "76528423897159cc4b023c8928b698f63b5db095" },
    { ORACLES_GAME_AGES, RAW_TITLE_COLOURS, 0x17, 0x4018, 0x80, "1dfbbdffa73ea37e7091b5476c2a3541a20621d9" },
    { ORACLES_GAME_SEASONS, RAW_TITLE_COLOURS, 0x16, 0x4018, 0x80, "96d20b80edaf19616a1de56535a0630bfd554a0a" },
    { ORACLES_GAME_AGES, RAW_TITLE_SPRITES, 0x3f, 0x7d0a, 0x70, "5c0168621a045c606b075a182ad39370e6343683" },      /* $00 */
    { ORACLES_GAME_SEASONS, RAW_TITLE_SPRITES, 0x03, 0x7dd7, 0x70, "a3f3332167da1a09252db14b30e1f00549d277c2" },   /* $03 */
    { ORACLES_GAME_AGES, RAW_SECRET_CIPHER, 0x03, 0x4ada, 0x30, "b817447e8c977de34331b277a9b698d4814aeea7" },
    { ORACLES_GAME_SEASONS, RAW_SECRET_CIPHER, 0x03, 0x4ada, 0x30, "b817447e8c977de34331b277a9b698d4814aeea7" },
    { ORACLES_GAME_AGES, RAW_SECRET_SYMBOLS, 0x00, 0x00a1, 0x41, "317adb9b9d924228602477b7974762ebc139e57a" },
    { ORACLES_GAME_SEASONS, RAW_SECRET_SYMBOLS, 0x00, 0x00a1, 0x41, "317adb9b9d924228602477b7974762ebc139e57a" },
    { ORACLES_GAME_AGES, RAW_SECRET_NAMES, 0x02, 0x4e2e, 0xe9, "434eac56676d9ed0a4a07f85402866896a5e6c15" },
    { ORACLES_GAME_SEASONS, RAW_SECRET_NAMES, 0x02, 0x4dee, 0xea, "93dadbfdf71a329796a208f774b2ad0d794db8b4" },
    { ORACLES_GAME_AGES, RAW_KANA_ENTRY, 0x02, 0x46d6, 0x374, "0877e9f6284a0a7a49d4c70a18effef647d537cd" },
    { ORACLES_GAME_SEASONS, RAW_KANA_ENTRY, 0x02, 0x4696, 0x374, "7fcdce122bc8daafb6637bcb11fdc9c2860893bd" },
    { ORACLES_GAME_AGES, RAW_KANA_TAIL, 0x02, 0x7e95, 0x16b, "02aed5708b7674546448b2a144f77327765f721f" },       /* $00, after bank02End.s */
    { ORACLES_GAME_SEASONS, RAW_KANA_TAIL, 0x02, 0x75bb, 0xa45, "68a3a4a0e5e646df9beef01d67de42f2db429559" },    /* $02 */
    { ORACLES_GAME_AGES, RAW_KANA_CALLED, 0x02, 0x410e, 0x20, "ef322c23f02ec38eff447d88e27250ff359e08ff" },
    { ORACLES_GAME_SEASONS, RAW_KANA_CALLED, 0x02, 0x40ce, 0x20, "7fdc3aad0ac8dfceac0865f365183fc0c7af7894" },
};
#define PRESS_START_BLOCKS 1u

/* The code's pointers to a table a language moves: intro_titlescreen's `ld hl,titlescreenMakuSeedSprite` (03:4d91;
 * then `ld e,$3f` in Ages, the bank of the table's new place too), its operand, the US table's address, set to the
 * table's place. */
static const struct pointer { OraclesGame game; unsigned kind, bank, address, us; } pointers[] = {
    { ORACLES_GAME_AGES, RAW_TITLE_SPRITES, 0x03, 0x4d92, 0x595d },
    { ORACLES_GAME_SEASONS, RAW_TITLE_SPRITES, 0x03, 0x4d92, 0x4e1c },
};

static const struct raw_table *raw_table_of(OraclesGame game, unsigned kind)
{
    for (size_t i = 0; i < sizeof raw_tables / sizeof raw_tables[0]; i++)
        if (raw_tables[i].game == game && raw_tables[i].kind == kind) return &raw_tables[i];
    return NULL;
}

/* Whether the ROM holds the US bytes of a table's place. */
static int holds_us_table(const uint8_t *rom, size_t size, const struct raw_table *t)
{
    const size_t at = oracles_rom_offset(t->bank, t->address);
    char sha1[41];
    if (at + t->size > size) return 0;
    oracles_sha1_hex(rom + at, t->size, sha1);
    return !strcmp(sha1, t->us);
}

/* A table `t`, as a raw graphic of `bytes` over the US bytes of its place: in `language`, its first `length` bytes from
 * `from`.  0, or -1 when it does not fit, a header names its place or the US ROM does not hold it. */
static int extract_raw(extraction *x, const struct raw_table *t, OraclesLanguage language, const uint8_t *from, size_t length, size_t bytes)
{
    const size_t at = t ? oracles_rom_offset(t->bank, t->address) : 0;
    if (!t || !bytes || bytes % 16u || length > bytes || bytes > t->size || !holds_us_table(x->us->data, x->us->size, t)) return -1;
    for (unsigned i = 0; i < x->count; i++)
        if (x->graphics[i].bank == t->bank && x->graphics[i].address == t->address) return -1;   /* a header names it */
    const gfx_entry e = { t->bank, t->address, 0, 0, (unsigned)(bytes / 16u - 1u), 0 };
    graphic *g = graphic_of(x, &e);
    if (!g) return -1;
    g->data[ORACLES_LANGUAGE_ENGLISH] = malloc(bytes);
    g->data[language] = malloc(bytes);
    if (!g->data[ORACLES_LANGUAGE_ENGLISH] || !g->data[language]) return -1;
    memcpy(g->data[ORACLES_LANGUAGE_ENGLISH], x->us->data + at, bytes);
    memcpy(g->data[language], x->us->data + at, bytes);
    memcpy(g->data[language], from, length);
    g->found = g->raw = 1;
    return 0;
}

/* The Polish table of the title's sprites of PRESS START (their count, then 4 bytes a sprite: y, x, tile and
 * attributes); 0 or -1. */
static int extract_press_start(extraction *x, OraclesGame game)
{
    const size_t from = language_press_start_polish(game);
    if (!from || from >= x->eu->size) return -1;
    const size_t length = 1u + 4u * x->eu->data[from];
    if (from + length > x->eu->size) return -1;
    return extract_raw(x, raw_table_of(game, RAW_PRESS_START), ORACLES_LANGUAGE_POLISH, x->eu->data + from, length, 16u * (PRESS_START_BLOCKS + 1u));
}

/* Palette header h of the table at `table` (physical): its entries (a control byte and the address of its colours in
 * their bank), at most `capacity`; their number, or -1. */
static int palette_entries(const uint8_t *rom, size_t size, size_t table, unsigned h, unsigned capacity, unsigned *control, unsigned *colours)
{
    if (!table || table + 2u * h + 2u > size) return -1;
    size_t at = table / BANK * BANK + (size_t)(((unsigned)rom[table + 2u * h] | rom[table + 2u * h + 1u] << 8) & 0x3fffu);
    for (unsigned n = 0; n < capacity && at + 3u <= size; n++, at += 3u) {
        control[n] = rom[at];
        colours[n] = rom[at + 1u] | rom[at + 2u] << 8;
        if (!(control[n] & 0x80u)) return (int)n + 1;
    }
    return -1;
}

/* The Japanese title's colours, PALH_03's: the Japanese header's entries the US one's, their colours one after the
 * other where the US ones lie; 0 or -1. */
static int extract_title_colours(extraction *x, OraclesGame game)
{
    enum { TITLE_PALETTES = 0x03, MAX = 4 };
    const struct raw_table *t = raw_table_of(game, RAW_TITLE_COLOURS);
    unsigned bank = 0, us_control[MAX], us_colours[MAX], jp_control[MAX], jp_colours[MAX];
    const size_t us_table = language_palette_headers_us(game, &bank), jp_table = language_palette_headers_japanese(game);
    const int n = palette_entries(x->us->data, x->us->size, us_table, TITLE_PALETTES, MAX, us_control, us_colours);
    if (!t || n <= 0 || bank != t->bank || palette_entries(x->eu->data, x->eu->size, jp_table, TITLE_PALETTES, MAX, jp_control, jp_colours) != n) return -1;
    unsigned next = t->address;
    for (int i = 0; i < n; i++) {
        if (us_control[i] != jp_control[i] || us_colours[i] != next || jp_colours[i] != next) return -1;
        next += 8u * ((us_control[i] & 7u) + 1u);
    }
    const size_t at = oracles_rom_offset(t->bank, t->address);
    if (next - t->address != t->size || at + t->size > x->eu->size) return -1;
    return extract_raw(x, t, ORACLES_LANGUAGE_JAPANESE, x->eu->data + at, t->size, t->size);
}

/* The Japanese title's sprites, their table where the US code's pointer to the US one lies; 0 or -1. */
static int extract_title_sprites(extraction *x, OraclesGame game)
{
    const struct raw_table *t = raw_table_of(game, RAW_TITLE_SPRITES);
    const size_t from = language_title_sprites_japanese(game);
    for (size_t i = 0; i < sizeof pointers / sizeof pointers[0]; i++) {
        const struct pointer *p = &pointers[i];
        const size_t at = oracles_rom_offset(p->bank, p->address);
        if (p->game == game && (at + 2u > x->us->size || x->us->data[at - 1u] != 0x21u || ((unsigned)x->us->data[at] | (unsigned)x->us->data[at + 1u] << 8) != p->us)) return -1;
    }
    if (!t || !from || from >= x->eu->size) return -1;
    const size_t length = 1u + 4u * x->eu->data[from];
    if (from + length > x->eu->size) return -1;
    return extract_raw(x, t, ORACLES_LANGUAGE_JAPANESE, x->eu->data + from, length, t->size);
}

/* The Japanese secrets' cipher and symbols, where the Japanese ROM has them: the cipher's 48 values of 6 bits, the 64
 * symbols each a character of the font ($20 on) once; 0 or -1. */
#define SECRET_SYMBOLS 0x40u
static int extract_secret_tables(extraction *x, OraclesGame game)
{
    const struct raw_table *c = raw_table_of(game, RAW_SECRET_CIPHER), *t = raw_table_of(game, RAW_SECRET_SYMBOLS);
    const size_t cipher = language_secret_cipher_japanese(game), symbols = language_secret_symbols_japanese(game);
    if (!c || !t || !cipher || !symbols || t->size < SECRET_SYMBOLS || cipher + c->size > x->eu->size || symbols + SECRET_SYMBOLS > x->eu->size) return -1;
    for (unsigned i = 0; i < c->size; i++) if (x->eu->data[cipher + i] > 0x3fu) return -1;
    for (unsigned i = 0; i < SECRET_SYMBOLS; i++) {
        if (x->eu->data[symbols + i] < 0x20u) return -1;
        for (unsigned j = 0; j < i; j++) if (x->eu->data[symbols + j] == x->eu->data[symbols + i]) return -1;
    }
    if (extract_raw(x, c, ORACLES_LANGUAGE_JAPANESE, x->eu->data + cipher, c->size, c->size) != 0) return -1;
    return extract_raw(x, t, ORACLES_LANGUAGE_JAPANESE, x->eu->data + symbols, SECRET_SYMBOLS, SECRET_SYMBOLS);
}

/* The names of the Japanese secrets' list, made for the US table's place: its 26 pointers (secretTextTable, the dashes,
 * the word after each name and the names), the first set to the second, then each name of the Japanese ROM from the
 * second, its terminator included, the pointers to them where they now lie; 0, or -1 when they do not fit. */
#define SECRET_NAMES 26u
#define SECRET_NAME_MAX 32u
static int extract_secret_names(extraction *x, OraclesGame game)
{
    const struct raw_table *t = raw_table_of(game, RAW_SECRET_NAMES);
    const size_t table = language_secret_names_japanese(game);
    uint8_t names[0x100];
    if (!t || !table || t->size > sizeof names || table + 2u * SECRET_NAMES > x->eu->size) return -1;
    size_t length = 2u * SECRET_NAMES;
    for (unsigned i = 1; i < SECRET_NAMES; i++) {
        const unsigned pointer = x->eu->data[table + 2u * i] | x->eu->data[table + 2u * i + 1u] << 8;
        if (pointer < BANK || pointer >= 2u * BANK) return -1;
        const size_t from = table / BANK * BANK + (pointer - BANK);
        size_t n = 0;
        while (n < SECRET_NAME_MAX && from + n < x->eu->size && x->eu->data[from + n]) n++;
        if (n == SECRET_NAME_MAX || from + n >= x->eu->size || length + n + 1u > t->size) return -1;
        const unsigned at = t->address + (unsigned)length;
        names[2u * i] = (uint8_t)at;
        names[2u * i + 1u] = (uint8_t)(at >> 8);
        memcpy(names + length, x->eu->data + from, n + 1u);
        length += n + 1u;
    }
    names[0] = names[2];
    names[1] = names[3];
    const size_t bytes = (length + 15u) / 16u * 16u;
    return bytes <= t->size ? extract_raw(x, t, ORACLES_LANGUAGE_JAPANESE, names, length, bytes) : -1;
}

/* The name entry in kana: the routines for the tail of bank 2 and the US code with its sites (language_kana.c), as
 * raw tables, where the US ROM holds the US bytes of both and of the routine the routines call; 0 or -1. */
static int extract_kana(extraction *x, OraclesGame game)
{
    const struct raw_table *entry = raw_table_of(game, RAW_KANA_ENTRY), *tail = raw_table_of(game, RAW_KANA_TAIL), *called = raw_table_of(game, RAW_KANA_CALLED);
    static language_kana kana;
    static uint8_t code[0x400];
    if (!entry || !tail || !called || entry->size > sizeof code || !holds_us_table(x->us->data, x->us->size, called)
        || language_kana_make(game, x->eu, x->us, tail->address, &kana) != 0) return -1;
    const size_t bytes = entry->size / 16u * 16u, at = oracles_rom_offset(entry->bank, entry->address);
    if (at + bytes > x->us->size) return -1;
    memcpy(code, x->us->data + at, bytes);
    if (language_kana_patch(&kana, entry->address, code, bytes) != 0) return -1;
    if (extract_raw(x, entry, ORACLES_LANGUAGE_JAPANESE, code, bytes, bytes) != 0) return -1;
    return extract_raw(x, tail, ORACLES_LANGUAGE_JAPANESE, kana.tail, kana.length, (kana.length + 15u) / 16u * 16u);
}

/* ---- the grid of the secret entry ---------------------------------------------------------------------------------- */

/* The US code takes a secret's character at the cursor from the tile of its cell in the grid's map:
 * secretSymbols[tile / 2 & $3f], whose glyphs textInput_loadCharacterGfx copies to those tiles in the order of the
 * symbols (bank2.s).  A cell is two rows of the map (a glyph's tiles, 2 i + $80 and the next) and of the attributes' map;
 * the grid's cells are the rows and the columns of the screen (the map's first 20) where one shows a glyph, those that
 * show none holding the grid's blank, tile $02.  Made for the Japanese font, from the US layout: the grid with its 64
 * symbols in the order of the Japanese font, the kana and then the digits, its blank between them. */
#define MAP_ROW 32u
#define MAP_SHOWN 20u
#define GRID_BLANK 0x02u
#define GRID_FIRST 0x80u
#define NO_CELL 0xffffu

typedef struct grid {
    uint8_t tiles[GFX_BYTES], attributes[GFX_BYTES];
    size_t size;
    unsigned cells[80], count;   /* the top tile's place of each cell, in reading order */
} grid;

/* The US map and attributes of header h's entries `map` and `map + 1`, their cells; their graphics in *g and *flg.  0 or -1. */
static int grid_load(extraction *x, unsigned h, unsigned map, grid *out, graphic **g, graphic **flg)
{
    gfx_entry entries[MAX_ENTRIES];
    const int n = header_entries(x->us->data, x->us->size, x->us_table, h, 0, entries);
    if (n <= (int)map + 1) return -1;
    const gfx_entry *m = &entries[map], *f = &entries[map + 1u];
    out->size = 16u * (m->blocks + 1u);
    if (out->size != 16u * (f->blocks + 1u) || out->size % MAP_ROW
        || language_gfx_decompress(x->us->data, x->us->size, source_of(m), m->mode, out->tiles, out->size, NULL) != 0
        || language_gfx_decompress(x->us->data, x->us->size, source_of(f), f->mode, out->attributes, out->size, NULL) != 0) return -1;
    *g = graphic_of(x, m);
    *flg = *g ? graphic_of(x, f) : NULL;
    if (!*flg || (*g)->blocks != m->blocks || (*flg)->blocks != f->blocks) return -1;
    uint8_t rows[MAP_ROW] = { 0 }, columns[MAP_ROW] = { 0 };
    for (size_t i = 0; i + MAP_ROW < out->size; i++)
        if (i % MAP_ROW < MAP_SHOWN && out->tiles[i] >= GRID_FIRST && !(out->tiles[i] & 1u)) {
            if (out->tiles[i + MAP_ROW] != out->tiles[i] + 1u) return -1;
            rows[i / MAP_ROW] = columns[i % MAP_ROW] = 1;
        }
    out->count = 0;
    for (unsigned r = 0; r < out->size / MAP_ROW; r++)
        for (unsigned c = 0; rows[r] && c < MAP_ROW; c++) {
            if (!columns[c]) continue;
            const unsigned at = r * MAP_ROW + c, top = out->tiles[at];
            if (out->count == sizeof out->cells / sizeof out->cells[0]) return -1;
            if (top < GRID_FIRST && (top != GRID_BLANK || out->tiles[at + MAP_ROW] != GRID_BLANK)) return -1;
            out->cells[out->count++] = at;
        }
    return 0;
}

/* Cell `from` of `us` copied to cell `to` of `made`, its two tiles and their attributes. */
static void grid_copy(grid *made, unsigned to, const grid *us, unsigned from)
{
    made->tiles[to] = us->tiles[from];
    made->tiles[to + MAP_ROW] = us->tiles[from + MAP_ROW];
    made->attributes[to] = us->attributes[from];
    made->attributes[to + MAP_ROW] = us->attributes[from + MAP_ROW];
}

/* A map and its attributes made for Japanese, the US ones beside them: graphics of the pack. */
static int grid_add(const grid *made, const grid *us, graphic *g, graphic *flg)
{
    graphic *both[2] = { g, flg };
    const uint8_t *data[2][2] = { { us->tiles, made->tiles }, { us->attributes, made->attributes } };
    for (unsigned k = 0; k < 2u; k++) {
        if (both[k]->found) return -1;
        both[k]->data[ORACLES_LANGUAGE_ENGLISH] = malloc(us->size);
        both[k]->data[ORACLES_LANGUAGE_JAPANESE] = malloc(us->size);
        if (!both[k]->data[ORACLES_LANGUAGE_ENGLISH] || !both[k]->data[ORACLES_LANGUAGE_JAPANESE]) return -1;
        memcpy(both[k]->data[ORACLES_LANGUAGE_ENGLISH], data[k][0], us->size);
        memcpy(both[k]->data[ORACLES_LANGUAGE_JAPANESE], data[k][1], us->size);
        both[k]->found = 1;
    }
    return 0;
}

/* Whether a symbol is a digit, which the secret entry's grid shows last. */
static unsigned symbol_rank(uint8_t symbol) { return (symbol >= '0' && symbol <= '9') << 8 | symbol; }

static int japanese_grids(extraction *x, OraclesGame game)
{
    static grid us, made;
    graphic *g = NULL, *flg = NULL;
    /* The secret entry's ($ab, and $ac, which loads the same): each symbol's cell, then the blank's, the symbols in their
     * order. */
    const struct raw_table *t = raw_table_of(game, RAW_SECRET_SYMBOLS);
    const size_t symbols = language_secret_symbols_japanese(game);
    if (!t || !symbols || grid_load(x, 0xab, 2, &us, &g, &flg) != 0 || us.count != SECRET_SYMBOLS + 1u) return -1;
    unsigned of[SECRET_SYMBOLS + 1], order[SECRET_SYMBOLS + 1], placed = 0;
    for (unsigned i = 0; i <= SECRET_SYMBOLS; i++) of[i] = NO_CELL;
    for (unsigned i = 0; i < us.count; i++) {
        const unsigned tile = us.tiles[us.cells[i]], index = tile >= GRID_FIRST ? (tile - GRID_FIRST) / 2u : SECRET_SYMBOLS;
        if (index > SECRET_SYMBOLS || of[index] != NO_CELL) return -1;
        of[index] = us.cells[i];
    }
    for (unsigned i = 0; i < SECRET_SYMBOLS; i++) order[i] = i;
    for (unsigned i = 1; i < SECRET_SYMBOLS; i++)
        for (unsigned j = i; j > 0 && symbol_rank(x->eu->data[symbols + order[j - 1u]]) > symbol_rank(x->eu->data[symbols + order[j]]); j--) {
            const unsigned k = order[j]; order[j] = order[j - 1u]; order[j - 1u] = k;
        }
    made = us;
    for (unsigned i = 0; i < SECRET_SYMBOLS; i++) {
        const int digit = symbol_rank(x->eu->data[symbols + order[i]]) >> 8;
        if (digit && placed == i) grid_copy(&made, us.cells[placed++], &us, of[SECRET_SYMBOLS]);
        grid_copy(&made, us.cells[placed++], &us, of[order[i]]);
    }
    if (placed != us.count) return -1;
    return grid_add(&made, &us, g, flg);
}

/* The graphics found that an object's or a tree's header of the US ROM names: each made the 32 blocks that header
 * loads, as the Japanese ROM's header of the same index loads them (the Japanese tables are the US ones, at another
 * place, the trees' after the objects' in both), the graphic found their first bytes; the composition repoints that
 * header with the graphic's other headers.  The inventory's symbols of the present and the past (Ages, object $70) and of
 * the seasons (Seasons, $55), which the popup shown on entering a time or a season loads.  0, or -1 when the Japanese
 * headers do not load the graphic found, or load it otherwise. */
static int japanese_objects(extraction *x, OraclesGame game)
{
    size_t at[OBJECT_HEADERS_MAX];
    const unsigned n = object_headers_of(game, x->us->data, x->us->size, at);
    const size_t table = language_object_headers_japanese(game);
    if (!n || !table || table + 3u * n > x->eu->size) return -1;
    static uint8_t load[16u * OBJECT_BLOCKS], other[16u * OBJECT_BLOCKS];
    for (unsigned i = 0; i < x->count; i++) {
        graphic *g = &x->graphics[i];
        if (!g->found || g->raw || g->conflict) continue;
        const size_t size = 16u * (g->blocks + 1u);
        unsigned named = 0;
        for (unsigned k = 0; k < n; k++) {
            const uint8_t *o = x->us->data + at[k], *j = x->eu->data + table + 3u * k;
            if ((o[0] & 0x3fu) != g->bank || ((unsigned)(o[1] & 0x7fu) << 8 | o[2]) != g->address) continue;
            uint8_t *made = named ? other : load;
            if (size > sizeof load || (j[1] & 0x7fu) < BANK >> 8
                || language_gfx_decompress(x->eu->data, x->eu->size, oracles_rom_offset(j[0] & 0x3fu, (unsigned)(j[1] & 0x7fu) << 8 | j[2]), j[0] >> 6, made,
                                           sizeof load, NULL) != 0
                || memcmp(made, g->data[ORACLES_LANGUAGE_JAPANESE], size) || (named && memcmp(other, load, sizeof load))) return -1;
            named++;
        }
        if (!named || size == sizeof load) continue;
        const OraclesLanguage both[2] = { ORACLES_LANGUAGE_ENGLISH, ORACLES_LANGUAGE_JAPANESE };
        for (unsigned l = 0; l < 2u; l++) {
            uint8_t *grown = realloc(g->data[both[l]], sizeof load);
            if (!grown) return -1;
            g->data[both[l]] = grown;
            memcpy(grown + size, load + size, sizeof load - size);
        }
        uint8_t *set = realloc(g->set[ORACLES_LANGUAGE_JAPANESE], sizeof load);
        if (!set) return -1;
        g->set[ORACLES_LANGUAGE_JAPANESE] = set;
        memset(set + size, 1, sizeof load - size);
        g->blocks = OBJECT_BLOCKS - 1u;
    }
    return 0;
}

/* Whether a language's graphic is a table written raw at its place (raw_tables), no larger than the place; NULL else. */
static const struct raw_table *written_in_place(OraclesGame game, const pack_graphic *g)
{
    for (size_t i = 0; i < sizeof raw_tables / sizeof raw_tables[0]; i++) {
        const struct raw_table *t = &raw_tables[i];
        if (t->game == game && g->bank == t->bank && g->address == t->address && g->mode == 0 && g->length == 16u * (g->blocks + 1u) && g->length <= t->size)
            return t;
    }
    return NULL;
}

/* Header h, as the US game and the language's ROM load it: each US graphic that is the last to write bytes the
 * language's ROM loads otherwise takes the bytes of that ROM it is the last to write.  -1 when that ROM's header does
 * not write the bytes the US one writes. */
static int extract_header_alone(extraction *x, unsigned h, OraclesLanguage language)
{
    gfx_entry us[MAX_ENTRIES], its[MAX_ENTRIES];
    uint16_t *loaded = x->memory[0], *english = x->memory[1];
    const int us_count = header_entries(x->us->data, x->us->size, x->us_table, h, 0, us);
    const int its_count = header_entries(x->eu->data, x->eu->size, x->eu_table, h, 0, its);
    int status = us_count > 0 && its_count > 0 ? 0 : -1;
    if (status == 0) status = load(x->us->data, x->us->size, us, us_count, english, x->writer);
    if (status == 0) status = load(x->eu->data, x->eu->size, its, its_count, loaded, NULL);
    for (int j = 0; status == 0 && j < us_count; j++) {
        const gfx_entry *e = &us[j];
        const unsigned bytes = 16u * (e->blocks + 1u);
        int text = 0;
        for (unsigned i = 0; i < bytes && status == 0; i++) {
            const size_t k = place(e->dest, i);
            if (x->writer[k] != j) continue;
            if (loaded[k] == UNWRITTEN) status = -1;
            text |= loaded[k] != english[k];
        }
        if (!text || status != 0) continue;
        graphic *g = graphic_of(x, e);
        if (!g) { status = -1; break; }
        if (g->excluded) continue;
        const size_t size = 16u * (g->blocks + 1u);
        if (!g->found) {
            g->data[ORACLES_LANGUAGE_ENGLISH] = malloc(size);
            g->data[language] = malloc(size);
            g->set[language] = calloc(size, 1);
            if (!g->data[ORACLES_LANGUAGE_ENGLISH] || !g->data[language] || !g->set[language]
                || language_gfx_decompress(x->us->data, x->us->size, source_of(e), g->mode, g->data[ORACLES_LANGUAGE_ENGLISH], size, NULL) != 0) {
                status = -1;
                break;
            }
            memcpy(g->data[language], g->data[ORACLES_LANGUAGE_ENGLISH], size);
            g->found = 1;
        }
        for (unsigned i = 0; i < bytes; i++) {
            const size_t k = place(e->dest, i);
            if (x->writer[k] != j) continue;
            if (g->set[language][i] && g->data[language][i] != loaded[k]) g->conflict = 1;
            g->data[language][i] = (uint8_t)loaded[k];
            g->set[language][i] = 1;
        }
    }
    if (us_count > 0) unload(us, us_count, english, x->writer);
    if (its_count > 0) unload(its, its_count, loaded, NULL);
    return status;
}

/* An extraction from the ROM of an edition that translates its screens alone (`table`, its graphics headers'), as the
 * US ROM's headers `marks` says; its memories and the US graphics listed.  0 or -1. */
static int extraction_alone(extraction *x, const uint8_t marks[GFX_HEADERS])
{
    int status = x->eu_table && x->us_table ? 0 : -1;
    for (unsigned l = 0; l < 2u && status == 0; l++) {
        x->memory[l] = malloc(SPACE_BANKS * SPACE_BYTES * sizeof *x->memory[l]);
        if (!x->memory[l]) status = -1;
        else for (size_t i = 0; i < SPACE_BANKS * SPACE_BYTES; i++) x->memory[l][i] = UNWRITTEN;
    }
    if (status == 0 && !(x->writer = malloc(SPACE_BANKS * SPACE_BYTES * sizeof *x->writer))) status = -1;
    if (status == 0) for (size_t i = 0; i < SPACE_BANKS * SPACE_BYTES; i++) x->writer[i] = -1;
    if (status == 0) status = list_graphics(x, marks);
    return status;
}

int language_gfx_extract_polish(OraclesLanguagePack *pack, const OraclesRom *pl, const OraclesRom *us, char *error, size_t capacity)
{
    extraction x = { pl, us, language_gfx_headers_polish(pack->game), language_gfx_headers_us(pack->game), NULL, 0, 0, { NULL }, NULL };
    uint8_t marks[GFX_HEADERS];
    polish_headers(pack->game, marks);
    int status = extraction_alone(&x, marks);
    for (unsigned h = 0; h < GFX_HEADERS && status == 0; h++) if (marks[h] == HEADER_TEXT) status = extract_header_alone(&x, h, ORACLES_LANGUAGE_POLISH);
    if (status == 0) status = extract_press_start(&x, pack->game);
    if (status == 0) status = add_graphics(pack, &x, ORACLES_LANGUAGE_POLISH, ORACLES_LANGUAGE_POLISH + 1);
    extraction_free(&x);
    if (status != 0) snprintf(error, capacity, "the graphics of the Polish ROM cannot be read");
    return status;
}

int language_gfx_extract_japanese(OraclesLanguagePack *pack, const OraclesRom *jp, const OraclesRom *us, char *error, size_t capacity)
{
    extraction x = { jp, us, language_gfx_headers_japanese(pack->game), language_gfx_headers_us(pack->game), NULL, 0, 0, { NULL }, NULL };
    uint8_t marks[GFX_HEADERS];
    japanese_headers(pack->game, marks);
    int status = extraction_alone(&x, marks);
    gfx_entry entries[MAX_ENTRIES];
    for (size_t i = 0; i < sizeof japanese_kept / sizeof japanese_kept[0] && status == 0; i++) {
        const int n = header_entries(us->data, us->size, x.us_table, japanese_kept[i].header, 0, entries);
        graphic *g = n > japanese_kept[i].entry ? graphic_of(&x, &entries[japanese_kept[i].entry]) : NULL;
        if (!g) status = -1;
        else g->excluded = 1;
    }
    for (unsigned h = 0; h < GFX_HEADERS && status == 0; h++) if (marks[h] == HEADER_TEXT) status = extract_header_alone(&x, h, ORACLES_LANGUAGE_JAPANESE);
    if (status == 0) status = extract_title_colours(&x, pack->game);
    if (status == 0) status = extract_title_sprites(&x, pack->game);
    if (status == 0) status = extract_secret_tables(&x, pack->game);
    if (status == 0) status = extract_secret_names(&x, pack->game);
    if (status == 0) status = extract_kana(&x, pack->game);
    if (status == 0) status = japanese_grids(&x, pack->game);
    if (status == 0) status = japanese_objects(&x, pack->game);
    if (status == 0) status = add_graphics(pack, &x, ORACLES_LANGUAGE_JAPANESE, ORACLES_LANGUAGE_JAPANESE + 1);
    extraction_free(&x);
    if (status != 0) snprintf(error, capacity, "the graphics of the Japanese ROM cannot be read");
    return status;
}

/* ---- the composition --------------------------------------------------------------------------------------------- */

/* The free tails of the graphics banks: what follows the last graphic of a bank, filled ($00 in Ages, the bank's
 * number in Seasons) and read by nothing (ROM_DATA_FORMATS, section 15); used only when the image still has the fill. */
static const struct tail { OraclesGame game; unsigned bank, address; } tails[] = {
    { ORACLES_GAME_AGES, 0x38, 0x6aec },      /* after oth_twinrova_normal_layout */
    { ORACLES_GAME_SEASONS, 0x38, 0x7e58 },   /* after spr_caravan */
    { ORACLES_GAME_SEASONS, 0x1a, 0x70e0 },   /* after spr_magnet_gloves */
};

typedef struct range { size_t start, end; } range;

static int by_start(const void *a, const void *b)
{
    const range *x = a, *y = b;
    return x->start < y->start ? -1 : x->start > y->start;
}

typedef struct placed { unsigned index; size_t length, at; } placed;

static int by_length(const void *a, const void *b)
{
    const placed *x = a, *y = b;
    return x->length > y->length ? -1 : x->length < y->length ? 1 : (x->index > y->index) - (x->index < y->index);
}

int language_gfx_compose(const OraclesLanguagePack *pack, OraclesLanguage language, uint8_t *image, size_t size, OraclesLanguageReport *report,
                         char *error, size_t capacity)
{
    const unsigned count = language < ORACLES_LANGUAGE_COUNT ? pack->graphic_count[language] : 0;
    if (!count) return 0;
    const size_t table = language_gfx_headers_us(pack->game);
    const pack_graphic *graphics = pack->graphics[language];
    gfx_entry entries[MAX_ENTRIES];
    /* Every entry of every header: the replaced graphics' entries, and the places the others read. */
    size_t *repoint = malloc(GFX_HEADERS * MAX_ENTRIES * sizeof *repoint);
    unsigned *owner = malloc(GFX_HEADERS * MAX_ENTRIES * sizeof *owner);
    range *pool = malloc((count + sizeof tails / sizeof tails[0] + OBJECT_HEADERS_MAX) * sizeof *pool);
    placed *order = malloc(count * sizeof *order);
    size_t *kept = malloc(GFX_HEADERS * MAX_ENTRIES * sizeof *kept);
    size_t objects[OBJECT_HEADERS_MAX];
    unsigned object_owner[OBJECT_HEADERS_MAX];
    range withheld[OBJECT_HEADERS_MAX];
    unsigned entry_count = 0, kept_count = 0, ranges = 0, object_count = 0, repointed_objects = 0, withheld_count = 0;
    int status = repoint && owner && pool && order && kept && table ? 0 : -1;
    if (status != 0) snprintf(error, capacity, "out of memory");
    for (unsigned h = 0; h < GFX_HEADERS && status == 0; h++) {
        const int n = header_entries(image, size, table, h, 0, entries);
        if (n < 0) { snprintf(error, capacity, "the image's graphics headers are not the US ROM's"); status = -1; break; }
        for (int j = 0; j < n; j++) {
            unsigned g = 0;
            while (g < count && (graphics[g].bank != entries[j].bank || graphics[g].address != entries[j].address)) g++;
            int seen = 0;
            for (unsigned k = 0; k < entry_count && !seen; k++) seen = repoint[k] == entries[j].at;
            for (unsigned k = 0; k < kept_count && !seen; k++) seen = kept[k] == source_of(&entries[j]);
            if (seen) continue;
            if (g < count) { repoint[entry_count] = entries[j].at; owner[entry_count++] = g; }
            else kept[kept_count++] = source_of(&entries[j]);
        }
    }
    /* The objects' and the trees' headers: each that names a replaced graphic of 32 blocks, repointed with it; the bytes
     * each other reads, past its graphic's end too, withheld from the pool. */
    if (status == 0 && !(object_count = object_headers_of(pack->game, image, size, objects))) {
        snprintf(error, capacity, "the image's code does not name the US object graphics headers");
        status = -1;
    }
    for (unsigned i = 0; i < object_count && status == 0; i++) {
        const uint8_t *o = image + objects[i];
        const unsigned bank = o[0] & 0x3fu, address = (unsigned)(o[1] & 0x7fu) << 8 | o[2];
        unsigned g = 0;
        while (g < count && (graphics[g].bank != bank || graphics[g].address != address || written_in_place(pack->game, &graphics[g]))) g++;
        if (g < count && graphics[g].blocks + 1u == OBJECT_BLOCKS) {
            objects[repointed_objects] = objects[i];
            object_owner[repointed_objects++] = g;
            continue;
        }
        size_t consumed = 0;
        const size_t at = oracles_rom_offset(bank, address);
        if (address < BANK || language_gfx_decompress(image, size, at, o[0] >> 6, NULL, 16u * OBJECT_BLOCKS, &consumed) != 0) {
            snprintf(error, capacity, "the image's object graphics header at %02x:%04zx does not load", OBJECT_BANK, BANK + objects[i] % BANK);
            status = -1;
            break;
        }
        withheld[withheld_count++] = (range){ at, at + consumed };
    }
    /* The pool: the replaced graphics' bytes, as the largest of the headers that name them loads them, and the free
     * tails. */
    uint8_t *data = malloc(GFX_BYTES);
    if (!data && status == 0) { snprintf(error, capacity, "out of memory"); status = -1; }
    for (unsigned g = 0; g < count && status == 0; g++) {
        if (written_in_place(pack->game, &graphics[g])) continue;
        int named = 0;
        size_t consumed = 0;
        const size_t at = oracles_rom_offset(graphics[g].bank, graphics[g].address);
        unsigned mode = 4, blocks = 0;
        for (unsigned k = 0; k < entry_count; k++)
            if (owner[k] == g) {
                named = 1;
                mode = image[repoint[k]] >> 6;
                if ((image[repoint[k] + 5u] & 0x7fu) > blocks) blocks = image[repoint[k] + 5u] & 0x7fu;
            }
        if (!named || language_gfx_decompress(image, size, at, mode, data, 16u * (blocks + 1u), &consumed) != 0) {
            snprintf(error, capacity, "the image's graphics headers do not load the US graphic %02x:%04x", graphics[g].bank, graphics[g].address);
            status = -1;
            break;
        }
        pool[ranges++] = (range){ at, at + consumed };
    }
    for (size_t t = 0; t < sizeof tails / sizeof tails[0] && status == 0; t++) {
        if (tails[t].game != pack->game) continue;
        const size_t start = oracles_rom_offset(tails[t].bank, tails[t].address), end = oracles_rom_offset(tails[t].bank, 0x8000u);
        const uint8_t fill = pack->game == ORACLES_GAME_SEASONS ? (uint8_t)tails[t].bank : 0;
        size_t i = start;
        while (i < end && end <= size && image[i] == fill) i++;
        if (i == end) pool[ranges++] = (range){ start, end };
    }
    free(data);
    /* Merged, and none holding a graphic the image keeps. */
    if (status == 0) {
        qsort(pool, ranges, sizeof *pool, by_start);
        unsigned merged = 0;
        for (unsigned r = 0; r < ranges; r++) {
            if (merged && pool[r].start <= pool[merged - 1u].end) { if (pool[r].end > pool[merged - 1u].end) pool[merged - 1u].end = pool[r].end; }
            else pool[merged++] = pool[r];
        }
        ranges = merged;
        for (unsigned k = 0; k < kept_count; k++)
            for (unsigned r = 0; r < ranges; r++)
                if (kept[k] >= pool[r].start && kept[k] < pool[r].end) {
                    snprintf(error, capacity, "the image keeps a graphic at %06zx among those the language replaces", kept[k]);
                    status = -1;
                }
        for (unsigned w = 0; w < withheld_count && status == 0; w++)
            for (unsigned r = 0; r < ranges; r++) {
                range *x = &pool[r];
                const range *y = &withheld[w];
                if (y->start >= x->end || y->end <= x->start) continue;
                if (y->start > x->start && y->end < x->end) pool[ranges++] = (range){ y->end, x->end };
                if (y->start > x->start) x->end = y->start;
                else x->start = y->end < x->end ? y->end : x->end;
            }
        qsort(pool, ranges, sizeof *pool, by_start);
    }
    /* The longest first, each where it first fits; a table written in place, where it is. */
    unsigned moved = 0;
    for (unsigned g = 0; g < count && status == 0; g++)
        if (!written_in_place(pack->game, &graphics[g])) order[moved++] = (placed){ g, graphics[g].length, 0 };
    if (status == 0) qsort(order, moved, sizeof *order, by_length);
    for (unsigned o = 0; o < moved && status == 0; o++) {
        unsigned r = 0;
        while (r < ranges && pool[r].end - pool[r].start < order[o].length) r++;
        if (r == ranges) {
            snprintf(error, capacity, "the %s graphics do not fit the image", oracles_language_code(language));
            status = -1;
            break;
        }
        order[o].at = pool[r].start;
        pool[r].start += order[o].length;
    }
    /* The tables written at their place, and the code's pointers to them, checked before anything is written: each place
     * still holds the US bytes, each pointer still names the US table. */
    for (unsigned g = 0; g < count && status == 0; g++) {
        const struct raw_table *t = written_in_place(pack->game, &graphics[g]);
        if (!t) continue;
        if (!holds_us_table(image, size, t)) {
            snprintf(error, capacity, "the image does not hold the US table at %02x:%04x the %s graphics replace", t->bank, t->address, oracles_language_code(language));
            status = -1;
        }
        const struct raw_table *called = t->kind == RAW_KANA_ENTRY ? raw_table_of(pack->game, RAW_KANA_CALLED) : NULL;
        if (status == 0 && called && !holds_us_table(image, size, called)) {
            snprintf(error, capacity, "the image does not hold the US routine at %02x:%04x the %s name entry calls", called->bank, called->address,
                     oracles_language_code(language));
            status = -1;
        }
        for (size_t i = 0; i < sizeof pointers / sizeof pointers[0] && status == 0; i++) {
            const struct pointer *p = &pointers[i];
            const size_t at = oracles_rom_offset(p->bank, p->address);
            if (p->game != pack->game || p->kind != t->kind) continue;
            if (at + 2u > size || image[at - 1u] != 0x21u || ((unsigned)image[at] | (unsigned)image[at + 1u] << 8) != p->us) {
                snprintf(error, capacity, "the image's code does not name the US table at %02x:%04x", p->bank, p->address);
                status = -1;
            }
        }
    }
    /* Written, then every entry that named a replaced graphic pointed to the language's. */
    for (unsigned o = 0; o < moved && status == 0; o++) {
        const pack_graphic *g = &graphics[order[o].index];
        memcpy(image + order[o].at, pack->bytes + g->at, g->length);
        const unsigned bank = (unsigned)(order[o].at / BANK), address = BANK + (unsigned)(order[o].at % BANK);
        for (unsigned k = 0; k < entry_count; k++) {
            if (owner[k] != order[o].index) continue;
            image[repoint[k]] = (uint8_t)(bank | g->mode << 6);
            image[repoint[k] + 1u] = (uint8_t)(address >> 8);
            image[repoint[k] + 2u] = (uint8_t)address;
        }
        for (unsigned k = 0; k < repointed_objects; k++) {
            if (object_owner[k] != order[o].index) continue;
            image[objects[k]] = (uint8_t)(bank | g->mode << 6);
            image[objects[k] + 1u] = (uint8_t)((image[objects[k] + 1u] & 0x80u) | (address >> 8 & 0x7fu));
            image[objects[k] + 2u] = (uint8_t)address;
        }
        report->graphics++;
        report->graphics_bytes += g->length;
    }
    /* The tables at their place, the pointers to them set there. */
    for (unsigned g = 0; g < count && status == 0; g++) {
        const struct raw_table *t = written_in_place(pack->game, &graphics[g]);
        if (!t) continue;
        for (size_t i = 0; i < sizeof pointers / sizeof pointers[0]; i++) {
            const struct pointer *p = &pointers[i];
            if (p->game != pack->game || p->kind != t->kind) continue;
            const size_t at = oracles_rom_offset(p->bank, p->address);
            image[at] = (uint8_t)t->address;
            image[at + 1u] = (uint8_t)(t->address >> 8);
        }
        memcpy(image + oracles_rom_offset(t->bank, t->address), pack->bytes + graphics[g].at, graphics[g].length);
        report->graphics++;
        report->graphics_bytes += graphics[g].length;
    }
    free(repoint);
    free(owner);
    free(pool);
    free(order);
    free(kept);
    return status;
}
