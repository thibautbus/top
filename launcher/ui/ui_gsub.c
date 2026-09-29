#include "ui_gsub.h"

#include <string.h>

#define MAX_NESTING 4

static unsigned u16(const unsigned char *p) { return (unsigned)p[0] << 8 | p[1]; }
static uint32_t u32(const unsigned char *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

/* `length` bytes at `offset` of the table, or NULL past its end (the fonts are ours, but a bad offset must not read away). */
static const unsigned char *at(const OraclesUiGsub *g, size_t offset, size_t length)
{
    return offset + length <= g->size ? g->table + offset : NULL;
}

/* The glyph's index in a coverage table, or -1. */
static int coverage(const OraclesUiGsub *g, size_t offset, int glyph)
{
    const unsigned char *c = at(g, offset, 4);
    if (!c) return -1;
    const unsigned count = u16(c + 2);
    if (u16(c) == 1) {
        if (!at(g, offset + 4, 2u * count)) return -1;
        for (unsigned i = 0; i < count; i++) if ((int)u16(c + 4 + 2 * i) == glyph) return (int)i;
    } else if (u16(c) == 2) {
        if (!at(g, offset + 4, 6u * count)) return -1;
        for (unsigned i = 0; i < count; i++) {
            const unsigned char *r = c + 4 + 6 * i;
            if (glyph >= (int)u16(r) && glyph <= (int)u16(r + 2)) return (int)(u16(r + 4) + (unsigned)glyph - u16(r));
        }
    }
    return -1;
}

static int apply_lookup(const OraclesUiGsub *g, unsigned index, int *glyphs, size_t *count, size_t position, int depth);

static int single(const OraclesUiGsub *g, size_t sub, int *glyphs, size_t position)
{
    const unsigned char *s = at(g, sub, 6);
    if (!s) return 0;
    const int covered = coverage(g, sub + u16(s + 2), glyphs[position]);
    if (covered < 0) return 0;
    if (u16(s) == 1) { glyphs[position] = (int)((unsigned)(glyphs[position] + (int)(int16_t)u16(s + 4)) & 0xffffu); return 1; }
    if (u16(s) == 2 && (unsigned)covered < u16(s + 4) && at(g, sub + 6, 2u * u16(s + 4))) { glyphs[position] = (int)u16(s + 6 + 2 * covered); return 1; }
    return 0;
}

static int ligature(const OraclesUiGsub *g, size_t sub, int *glyphs, size_t *count, size_t position)
{
    const unsigned char *s = at(g, sub, 6);
    if (!s || u16(s) != 1) return 0;
    const int covered = coverage(g, sub + u16(s + 2), glyphs[position]);
    if (covered < 0 || (unsigned)covered >= u16(s + 4) || !at(g, sub + 6, 2u * u16(s + 4))) return 0;
    const size_t set = sub + u16(s + 6 + 2 * covered);
    const unsigned char *ls = at(g, set, 2);
    if (!ls || !at(g, set + 2, 2u * u16(ls))) return 0;
    for (unsigned i = 0; i < u16(ls); i++) {
        const size_t lig = set + u16(ls + 2 + 2 * i);
        const unsigned char *l = at(g, lig, 4);
        if (!l) continue;
        const unsigned components = u16(l + 2);
        if (components == 0 || position + components > *count || !at(g, lig + 4, 2u * (components - 1u))) continue;
        unsigned k = 1;
        while (k < components && (int)u16(l + 4 + 2 * (k - 1)) == glyphs[position + k]) k++;
        if (k < components) continue;
        glyphs[position] = (int)u16(l);
        memmove(glyphs + position + 1, glyphs + position + components, (*count - position - components) * sizeof *glyphs);
        *count -= components - 1u;
        return 1;
    }
    return 0;
}

/* Chained context, format 3: a coverage per glyph behind, in and ahead of the input. */
static int chain(const OraclesUiGsub *g, size_t sub, int *glyphs, size_t *count, size_t position, int depth)
{
    const unsigned char *s = at(g, sub, 4);
    if (!s || u16(s) != 3) return 0;
    const unsigned backtrack = u16(s + 2);
    size_t p = sub + 4;
    if (!at(g, p, 2u * backtrack + 2u) || backtrack > position) return 0;
    for (unsigned i = 0; i < backtrack; i++)
        if (coverage(g, sub + u16(g->table + p + 2 * i), glyphs[position - 1 - i]) < 0) return 0;
    p += 2u * backtrack;
    const unsigned input = u16(g->table + p);
    p += 2;
    if (!at(g, p, 2u * input + 2u) || input == 0 || position + input > *count) return 0;
    for (unsigned i = 0; i < input; i++)
        if (coverage(g, sub + u16(g->table + p + 2 * i), glyphs[position + i]) < 0) return 0;
    p += 2u * input;
    const unsigned ahead = u16(g->table + p);
    p += 2;
    if (!at(g, p, 2u * ahead + 2u) || position + input + ahead > *count) return 0;
    for (unsigned i = 0; i < ahead; i++)
        if (coverage(g, sub + u16(g->table + p + 2 * i), glyphs[position + input + i]) < 0) return 0;
    p += 2u * ahead;
    const unsigned records = u16(g->table + p);
    p += 2;
    if (!at(g, p, 4u * records)) return 0;
    for (unsigned i = 0; i < records; i++) {
        const unsigned sequence = u16(g->table + p + 4 * i), lookup = u16(g->table + p + 4 * i + 2);
        if (sequence < input) apply_lookup(g, lookup, glyphs, count, position + sequence, depth + 1);
    }
    return 1;
}

static int apply_lookup(const OraclesUiGsub *g, unsigned index, int *glyphs, size_t *count, size_t position, int depth)
{
    if (depth > MAX_NESTING || position >= *count) return 0;
    const unsigned char *header = at(g, 0, 10);
    const size_t list = header ? u16(header + 8) : 0;
    const unsigned char *l = at(g, list, 2);
    if (!l || index >= u16(l) || !at(g, list + 2, 2u * u16(l))) return 0;
    const size_t lookup = list + u16(l + 2 + 2 * index);
    const unsigned char *k = at(g, lookup, 6);
    if (!k || !at(g, lookup + 6, 2u * u16(k + 4))) return 0;
    for (unsigned i = 0; i < u16(k + 4); i++) {
        size_t sub = lookup + u16(k + 6 + 2 * i);
        unsigned type = u16(k);
        if (type == 7) {
            const unsigned char *e = at(g, sub, 8);
            if (!e || u16(e) != 1) continue;
            type = u16(e + 2);
            sub += u32(e + 4);
        }
        int applied = 0;
        if (type == 1) applied = single(g, sub, glyphs, position);
        else if (type == 4) applied = ligature(g, sub, glyphs, count, position);
        else if (type == 6) applied = chain(g, sub, glyphs, count, position, depth);
        if (applied) return 1;
    }
    return 0;
}

/* The LangSys of the Latin script, or of the default one. */
static size_t lang_sys(const OraclesUiGsub *g)
{
    const unsigned char *header = at(g, 0, 10);
    if (!header) return 0;
    const size_t scripts = u16(header + 4);
    const unsigned char *list = at(g, scripts, 2);
    if (!list || !at(g, scripts + 2, 6u * u16(list))) return 0;
    size_t chosen = 0;
    for (unsigned i = 0; i < u16(list); i++) {
        const unsigned char *record = list + 2 + 6 * i;
        if (!memcmp(record, "latn", 4) || (!chosen && !memcmp(record, "DFLT", 4))) chosen = scripts + u16(record + 4);
        if (!memcmp(record, "latn", 4)) break;
    }
    const unsigned char *script = chosen ? at(g, chosen, 2) : NULL;
    return script && u16(script) ? chosen + u16(script) : 0;
}

void oracles_ui_gsub_init(OraclesUiGsub *gsub, const unsigned char *font, size_t size, const char *feature)
{
    memset(gsub, 0, sizeof *gsub);
    if (size < 12) return;
    for (unsigned i = 0; i < u16(font + 4) && 12u + 16u * (i + 1u) <= size; i++) {
        const unsigned char *entry = font + 12 + 16 * i;
        if (memcmp(entry, "GSUB", 4) != 0) continue;
        const uint32_t offset = u32(entry + 8), length = u32(entry + 12);
        if ((size_t)offset + length <= size) { gsub->table = font + offset; gsub->size = length; }
    }
    const size_t langsys = gsub->table ? lang_sys(gsub) : 0;
    const unsigned char *ls = langsys ? at(gsub, langsys, 6) : NULL;
    const unsigned char *header = at(gsub, 0, 10);
    if (!ls || !header || !at(gsub, langsys + 6, 2u * u16(ls + 4))) { gsub->table = NULL; return; }
    const size_t features = u16(header + 6);
    const unsigned char *fl = at(gsub, features, 2);
    for (unsigned i = 0; fl && i < u16(ls + 4); i++) {
        const unsigned index = u16(ls + 6 + 2 * i);
        const unsigned char *record = index < u16(fl) ? at(gsub, features + 2 + 6u * index, 6) : NULL;
        if (!record || memcmp(record, feature, 4) != 0) continue;
        const size_t f = features + u16(record + 4);
        const unsigned char *table = at(gsub, f, 4);
        if (!table || !at(gsub, f + 4, 2u * u16(table + 2))) continue;
        for (unsigned j = 0; j < u16(table + 2) && gsub->lookup_count < ORACLES_UI_GSUB_MAX_LOOKUPS; j++)
            gsub->lookups[gsub->lookup_count++] = (uint16_t)u16(table + 4 + 2 * j);
    }
    /* Lookups apply in the order of the lookup list. */
    for (unsigned i = 1; i < gsub->lookup_count; i++)
        for (unsigned j = i; j > 0 && gsub->lookups[j - 1] > gsub->lookups[j]; j--) {
            const uint16_t swap = gsub->lookups[j];
            gsub->lookups[j] = gsub->lookups[j - 1];
            gsub->lookups[j - 1] = swap;
        }
}

size_t oracles_ui_gsub_apply(const OraclesUiGsub *gsub, int *glyphs, size_t count)
{
    if (!gsub->table) return count;
    for (unsigned i = 0; i < gsub->lookup_count; i++)
        for (size_t position = 0; position < count; position++)
            apply_lookup(gsub, gsub->lookups[i], glyphs, &count, position, 0);
    return count;
}
