/* The warp data of bank 04, rebuilt with the houses of the mods: the
 * destination tables, the main source lists of the groups and the lists of
 * positions their pointer entries lead to (ROM_DATA_FORMATS, section 6;
 * code/bank4.s, findWarpSourceAndDest and findScreenEdgeWarpSource).
 *
 * The lists fill the bank from the destination tables to its free tail, and
 * the houses need more than that tail in one place: a table grows, a list
 * gains entries.  So the area is read as units (a table or a list, or lists
 * that fall through into the next, which stay together), each with the
 * places other data points to (its labels) and the pointers it holds (its
 * references).  The houses edit the units in memory; the units are then laid
 * out again, in their order, around the two tables of pointers the code
 * names (warpDestTable, warpSourcesTable), and every pointer is written from
 * the labels.  Bytes of the area that no table or list accounts for may be
 * read by the code at their address (Seasons' warpSource7653, which
 * getLinkedHerosCaveSideEntranceRoom reads): they stay where they are,
 * pinned with the list they may fall through into, and the others are laid
 * out around them.  A list of positions that points further is refused. */
#include "mod_warps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BANK 0x4000u
#define MAX_UNITS 160u
#define MAX_LABELS 192u
#define MAX_REFS 192u
#define MAX_ENTRIES 512u

typedef enum { LABEL_DEST, LABEL_MAIN, LABEL_POSITIONS } label_kind;

typedef struct unit {
    uint8_t *bytes;
    unsigned size, capacity;
    unsigned start;                               /* where it was read, 0 for a new one */
    unsigned placed;                              /* where it is written */
    int pinned;                                   /* holds bytes no table accounts for: stays at `start`, never grows */
} unit;

typedef struct label { label_kind kind; unsigned id; unsigned unit, offset; } label;   /* id: the group, or the list's number */
typedef struct ref { unsigned unit, offset, target; } ref;                               /* a pointer entry and its list's number */

struct mod_warps {
    uint8_t *data;
    const OraclesTables *t;
    unsigned bank;
    unit units[MAX_UNITS];
    unsigned unit_count;
    label labels[MAX_LABELS];
    unsigned label_count;
    ref refs[MAX_REFS];
    unsigned ref_count;
    unsigned positions;                           /* lists of positions numbered so far */
    unsigned dest_count[8];
    uint8_t taken[8][256];                        /* destinations a source sends to, or given to a house */
    unsigned free_bytes;                          /* after the layout */
    int ages;                                     /* Ages' lookup: $ff ends a list without a default */
};

static uint8_t *at(const mod_warps *w, unsigned addr) { return w->data + (size_t)w->bank * BANK + (addr - BANK); }
static unsigned u16(const mod_warps *w, unsigned addr) { return at(w, addr)[0] | at(w, addr)[1] << 8; }
static void put16(mod_warps *w, unsigned addr, unsigned v) { at(w, addr)[0] = (uint8_t)v; at(w, addr)[1] = (uint8_t)(v >> 8); }
static unsigned dest_root(const mod_warps *w) { return w->t->warp_dest_table.addr; }
static unsigned source_root(const mod_warps *w) { return w->t->warp_sources_table.addr; }
static int is_pointer(const uint8_t *entry) { return entry[0] != 0xffu && (entry[0] & 0x40u); }

/* The bytes of a list from `bytes` to the entry that ends it ($ff, or bit 7: a default), within `limit`; 0 without. */
static unsigned list_size(const uint8_t *bytes, unsigned limit)
{
    for (unsigned n = 1; n <= MAX_ENTRIES && n * 4u <= limit; n++) {
        const uint8_t flags = bytes[(n - 1u) * 4u];
        if (flags == 0xffu || (flags & 0x80u)) return n * 4u;
    }
    return 0;
}

static label *find_label(mod_warps *w, label_kind kind, unsigned id)
{
    for (unsigned i = 0; i < w->label_count; i++) if (w->labels[i].kind == kind && w->labels[i].id == id) return &w->labels[i];
    return NULL;
}

/* ---- reading the area -------------------------------------------------------------------------- */

typedef struct span { unsigned start, end; label_kind kind; unsigned id; } span;

static int by_start(const void *a, const void *b)
{
    const span *x = a, *y = b;
    return x->start < y->start ? -1 : x->start > y->start;
}

/* The tables and lists, as spans of the bank, and the destinations their entries send to. */
static int read_spans(mod_warps *w, span *spans, unsigned *count, char *error, size_t capacity)
{
    for (unsigned g = 0; g < 8u; g++) {
        const unsigned start = u16(w, dest_root(w) + g * 2u);
        const unsigned end = g < 7u ? u16(w, dest_root(w) + (g + 1u) * 2u) : source_root(w);
        if (end <= start || (end - start) % 3u || (end - start) / 3u > 255u) { snprintf(error, capacity, "the warp destinations of group %u have no size", g); return -1; }
        w->dest_count[g] = (end - start) / 3u;
        spans[(*count)++] = (span){ start, end, LABEL_DEST, g };
    }
    for (unsigned g = 0; g < 8u; g++) {
        const unsigned start = u16(w, source_root(w) + g * 2u);
        const unsigned size = start >= BANK && start < 0x8000u ? list_size(at(w, start), 0x8000u - start) : 0;
        if (!size) { snprintf(error, capacity, "the warp sources of group %u have no end", g); return -1; }
        spans[(*count)++] = (span){ start, start + size, LABEL_MAIN, g };
        for (unsigned e = start; e < start + size; e += 4u) {
            const uint8_t *entry = at(w, e);
            if (entry[0] != 0xffu && !is_pointer(entry)) w->taken[entry[3] >> 4 & 7u][entry[2]] = 1;
            if (!is_pointer(entry)) continue;
            const unsigned list = entry[2] | entry[3] << 8;
            int known = 0;
            for (unsigned i = 0; i < *count; i++) known |= spans[i].kind == LABEL_POSITIONS && spans[i].start == list;
            if (known) continue;
            const unsigned n = list >= BANK && list < 0x8000u ? list_size(at(w, list), 0x8000u - list) : 0;
            if (!n || *count == MAX_LABELS) { snprintf(error, capacity, "a warp's list of positions has no end"); return -1; }
            for (unsigned p = list; p < list + n; p += 4u) {
                const uint8_t *position = at(w, p);
                if (position[0] != 0xffu && !is_pointer(position)) w->taken[position[3] >> 4 & 7u][position[2]] = 1;
            }
            spans[(*count)++] = (span){ list, list + n, LABEL_POSITIONS, w->positions++ };
        }
    }
    /* A pointer in a list of positions is followed by the game with nothing to repoint it, unless the list falls
     * through into a main list, whose pointers are its own. */
    for (unsigned i = 0; i < *count; i++) {
        if (spans[i].kind != LABEL_POSITIONS) continue;
        for (unsigned p = spans[i].start; p < spans[i].end; p += 4u) {
            int in_main = 0;
            for (unsigned k = 0; k < *count; k++) in_main |= spans[k].kind == LABEL_MAIN && p >= spans[k].start && p < spans[k].end;
            if (is_pointer(at(w, p)) && !in_main) { snprintf(error, capacity, "a warp's list of positions points further, at %04x", p); return -1; }
        }
    }
    return 0;
}

/* The area as units: spans that overlap (a list falling through into the next) make one; the units must tile the
 * area from the destinations to the free tail, the table of the sources' pointers aside. */
static int cut_units(mod_warps *w, span *spans, unsigned count, char *error, size_t capacity)
{
    qsort(spans, count, sizeof *spans, by_start);
    unsigned expected = dest_root(w) + 16u;
    for (unsigned i = 0; i <= count;) {
        if (expected == source_root(w)) expected = source_root(w) + 16u;
        const unsigned next = i < count ? spans[i].start : w->t->warp_bank_free.addr;
        if (next < expected || w->unit_count == MAX_UNITS) {
            snprintf(error, capacity, "bank 04's warp data overlaps at %04x", next);
            return -1;
        }
        if (i == count && next == expected) break;
        /* Bytes before the next span that no table accounts for: pinned, with the spans they may fall into. */
        const int pinned = next != expected;
        unsigned end = i < count ? spans[i].end : next, k = i < count ? i + 1u : i;
        while (k < count && spans[k].start < end) { if (spans[k].end > end) end = spans[k].end; k++; }
        unit *u = &w->units[w->unit_count];
        u->start = pinned ? expected : spans[i].start;
        u->size = end - u->start;
        u->capacity = u->size + 64u;
        u->pinned = pinned;
        u->bytes = malloc(u->capacity);
        if (!u->bytes) { snprintf(error, capacity, "out of memory"); return -1; }
        memcpy(u->bytes, at(w, u->start), u->size);
        for (unsigned s = i; s < k; s++) w->labels[w->label_count++] = (label){ spans[s].kind, spans[s].id, w->unit_count, spans[s].start - u->start };
        w->unit_count++;
        expected = end;
        if (i == count) break;
        i = k;
    }
    /* The pointer entries of the main lists, by the number of the list they lead to. */
    for (unsigned i = 0; i < w->label_count; i++) {
        const label *l = &w->labels[i];
        if (l->kind != LABEL_MAIN) continue;
        const unit *u = &w->units[l->unit];
        const unsigned end_of_list = l->offset + list_size(u->bytes + l->offset, u->size - l->offset);
        for (unsigned e = l->offset; e < end_of_list; e += 4u) {
            if (!is_pointer(u->bytes + e)) continue;
            const unsigned list = u->bytes[e + 2u] | u->bytes[e + 3u] << 8;
            for (unsigned s = 0; s < count; s++) {
                if (spans[s].kind != LABEL_POSITIONS || spans[s].start != list) continue;
                if (w->ref_count == MAX_REFS) { snprintf(error, capacity, "too many warps' lists of positions"); return -1; }
                w->refs[w->ref_count++] = (ref){ l->unit, e, spans[s].id };
            }
        }
    }
    return 0;
}

int mod_warps_open(mod_warps **out, uint8_t *image, size_t size, const OraclesTables *t, int ages, char *error, size_t capacity)
{
    *out = NULL;
    mod_warps *w = calloc(1, sizeof *w);
    span *spans = calloc(MAX_LABELS, sizeof *spans);
    unsigned count = 0;
    if (!w || !spans) { free(w); free(spans); snprintf(error, capacity, "out of memory"); return -1; }
    w->data = image;
    w->t = t;
    w->ages = ages;
    w->bank = t->warp_sources_table.bank;
    int ok = t->warp_dest_table.bank == w->bank && t->warp_bank_free.bank == w->bank && (w->bank + 1u) * BANK <= size;
    if (!ok) snprintf(error, capacity, "the warp tables are not in one bank");
    ok = ok && read_spans(w, spans, &count, error, capacity) == 0 && cut_units(w, spans, count, error, capacity) == 0;
    free(spans);
    if (!ok) { mod_warps_close(w); return -1; }
    *out = w;
    return 0;
}

void mod_warps_close(mod_warps *w)
{
    if (!w) return;
    for (unsigned i = 0; i < w->unit_count; i++) free(w->units[i].bytes);
    free(w);
}

int mod_warps_is_destination(const mod_warps *w, unsigned group, unsigned room)
{
    group &= 7u;
    const unsigned table = u16(w, dest_root(w) + group * 2u);
    for (unsigned i = 0; i < w->dest_count[group]; i++) if (w->taken[group][i] && *at(w, table + i * 3u) == room) return 1;
    return 0;
}

int mod_warps_has_sources(const mod_warps *w, unsigned group, unsigned room)
{
    const unsigned start = u16(w, source_root(w) + (group & 7u) * 2u), end = start + list_size(at(w, start), 0x8000u - start);
    for (unsigned e = start; e < end; e += 4u) if (at(w, e)[0] != 0xffu && at(w, e)[1] == room) return 1;
    return 0;
}

/* ---- editing the units ------------------------------------------------------------------------- */

/* `n` bytes into unit `index` at `offset`; the labels after it and the references from it on follow. */
static int insert(mod_warps *w, unsigned index, unsigned offset, const uint8_t *bytes, unsigned n)
{
    unit *u = &w->units[index];
    if (u->pinned) return -1;                      /* it stays where the code reads it */
    if (u->size + n > u->capacity) {
        uint8_t *grown = realloc(u->bytes, u->size + n + 64u);
        if (!grown) return -1;
        u->bytes = grown;
        u->capacity = u->size + n + 64u;
    }
    memmove(u->bytes + offset + n, u->bytes + offset, u->size - offset);
    memcpy(u->bytes + offset, bytes, n);
    u->size += n;
    for (unsigned i = 0; i < w->label_count; i++) if (w->labels[i].unit == index && w->labels[i].offset > offset) w->labels[i].offset += n;
    for (unsigned i = 0; i < w->ref_count; i++) if (w->refs[i].unit == index && w->refs[i].offset >= offset) w->refs[i].offset += n;
    return 0;
}

int mod_warps_destination(mod_warps *w, unsigned group, const uint8_t entry[3], uint8_t *index)
{
    group &= 7u;
    const label *table = find_label(w, LABEL_DEST, group);
    if (!table) return -1;
    for (unsigned i = 0; i < w->dest_count[group]; i++) {
        if (w->taken[group][i]) continue;
        memcpy(w->units[table->unit].bytes + table->offset + i * 3u, entry, 3);
        w->taken[group][i] = 1;
        *index = (uint8_t)i;
        return 0;
    }
    if (w->dest_count[group] >= 255u || insert(w, table->unit, table->offset + w->dest_count[group] * 3u, entry, 3) != 0) return -1;
    w->taken[group][w->dest_count[group]] = 1;
    *index = (uint8_t)w->dest_count[group]++;
    return 0;
}

int mod_warps_append(mod_warps *w, unsigned group, const uint8_t entry[4])
{
    const label *list = find_label(w, LABEL_MAIN, group & 7u);
    if (!list) return -1;
    const unit *u = &w->units[list->unit];
    const unsigned size = list_size(u->bytes + list->offset, u->size - list->offset);
    if (!size) return -1;
    return insert(w, list->unit, list->offset + size - 4u, entry, 4);   /* before the end, which stays last */
}

/* A new unit: a list of positions, numbered. */
static int new_positions(mod_warps *w, const uint8_t *bytes, unsigned size, unsigned *id)
{
    if (w->unit_count == MAX_UNITS || w->label_count == MAX_LABELS) return -1;
    unit *u = &w->units[w->unit_count];
    if (!(u->bytes = malloc(size))) return -1;
    memcpy(u->bytes, bytes, size);
    u->size = u->capacity = size;
    u->start = 0;
    *id = w->positions++;
    w->labels[w->label_count++] = (label){ LABEL_POSITIONS, *id, w->unit_count++, 0 };
    return 0;
}

int mod_warps_door(mod_warps *w, unsigned room, const uint8_t door[4])
{
    const label *list = find_label(w, LABEL_MAIN, 0);
    if (!list) return -1;
    const unsigned main_unit = list->unit, first = list->offset;
    const unsigned end = first + list_size(w->units[main_unit].bytes + first, w->units[main_unit].size - first);
    for (unsigned e = first; e < end; e += 4u) {
        const uint8_t *entry = w->units[main_unit].bytes + e;
        if (entry[0] == 0xffu || entry[1] != room || (!is_pointer(entry) && (entry[0] & 0x0fu))) continue;   /* an edge is no door */
        uint8_t positions[4u * (MAX_ENTRIES + 1u)];
        unsigned size = 4;
        memcpy(positions, door, 4);
        ref *pointer = NULL;
        for (unsigned i = 0; i < w->ref_count; i++) if (w->refs[i].unit == main_unit && w->refs[i].offset == e) pointer = &w->refs[i];
        if (!is_pointer(entry)) {                  /* a standard entry: its destination becomes the default */
            const uint8_t fallback[4] = { 0x80, 0x00, entry[2], entry[3] };
            memcpy(positions + 4, fallback, 4);
            size = 8;
        } else {                                   /* a list of positions: copied, the door first */
            const label *old = pointer ? find_label(w, LABEL_POSITIONS, pointer->target) : NULL;
            if (!old) return -1;
            const unit *from = &w->units[old->unit];
            const unsigned n = list_size(from->bytes + old->offset, from->size - old->offset);
            if (!n || n > sizeof positions - 4u) return -1;
            memcpy(positions + 4, from->bytes + old->offset, n);
            size += n;
        }
        unsigned id = 0;
        if (new_positions(w, positions, size, &id) != 0) return -1;
        uint8_t *bytes = w->units[main_unit].bytes;
        /* The screen's edges are looked up in the same list, and a pointer entry ends that lookup at its list's
         * default (findScreenEdgeWarpSource): the room's edges listed after the entry move before it, in their
         * order, and the entries between them follow.  The lookup of tiles skips edges; other rooms keep their
         * entries' order. */
        for (unsigned f = e + 4u; f < end; f += 4u) {
            const uint8_t *later = bytes + f;
            if (later[0] == 0xffu || later[1] != room || is_pointer(later) || !(later[0] & 0x0fu)) continue;
            uint8_t edge[4];
            memcpy(edge, later, 4);
            memmove(bytes + e + 4u, bytes + e, f - e);
            memcpy(bytes + e, edge, 4);
            for (unsigned i = 0; i < w->ref_count; i++) if (w->refs[i].unit == main_unit && w->refs[i].offset >= e && w->refs[i].offset < f) w->refs[i].offset += 4u;
            e += 4u;
        }
        bytes[e] = (uint8_t)(0x40u | (bytes[e] & 0x80u));   /* the room's entry points to the new positions */
        if (pointer) pointer->target = id;         /* the old list stays, for any other entry that points to it */
        else if (w->ref_count < MAX_REFS) w->refs[w->ref_count++] = (ref){ main_unit, e, id };
        else return -1;
        return 0;
    }
    /* A room without doors: a standard entry, which any warp tile of the room takes. */
    const uint8_t standard[4] = { 0x00, (uint8_t)room, door[2], door[3] };
    return mod_warps_append(w, 0, standard) == 0 ? 1 : -1;
}

/* ---- laying the units out again ------------------------------------------------------------------ */

int mod_warps_commit(mod_warps *w, char *error, size_t capacity)
{
    /* The free places: from the destinations' pointers to the sources' pointers, and from those to the bank's end,
     * less the pinned units, which stay; the others go, in their order, to the first place they fit. */
    typedef struct place { unsigned at, end; } place;
    place places[2u + MAX_UNITS];
    unsigned place_count = 0, missing = 0;
    const unsigned starts[2] = { dest_root(w) + 16u, source_root(w) + 16u }, ends[2] = { source_root(w), 0x8000u };
    for (unsigned p = 0; p < 2u; p++) {
        unsigned from = starts[p];
        for (unsigned guard = 0; guard <= w->unit_count; guard++) {   /* the pinned units in this range, in address order */
            const unit *first = NULL;
            for (unsigned i = 0; i < w->unit_count; i++) {
                const unit *u = &w->units[i];
                if (u->pinned && u->start >= from && u->start < ends[p] && (!first || u->start < first->start)) first = u;
            }
            if (!first) break;
            if (first->start > from) places[place_count++] = (place){ from, first->start };
            from = first->start + first->size;
        }
        if (ends[p] > from) places[place_count++] = (place){ from, ends[p] };
    }
    for (unsigned i = 0; i < w->unit_count; i++) {
        unit *u = &w->units[i];
        if (u->pinned) { u->placed = u->start; continue; }
        unsigned p = 0;
        while (p < place_count && places[p].end - places[p].at < u->size) p++;
        if (p == place_count) { missing += u->size; continue; }
        u->placed = places[p].at;
        places[p].at += u->size;
    }
    if (missing) {
        snprintf(error, capacity, "bank 04 is full: the warps of the houses need %u bytes more", missing);
        return -1;
    }
    for (unsigned i = 0; i < w->unit_count; i++) memcpy(at(w, w->units[i].placed), w->units[i].bytes, w->units[i].size);
    for (unsigned i = 0; i < w->label_count; i++) {
        const label *l = &w->labels[i];
        const unsigned address = w->units[l->unit].placed + l->offset;
        if (l->kind == LABEL_DEST) put16(w, dest_root(w) + l->id * 2u, address);
        else if (l->kind == LABEL_MAIN) put16(w, source_root(w) + l->id * 2u, address);
    }
    for (unsigned i = 0; i < w->ref_count; i++) {
        const ref *r = &w->refs[i];
        const label *target = find_label(w, LABEL_POSITIONS, r->target);
        if (!target) { snprintf(error, capacity, "a warp points to a list of positions that is gone"); return -1; }
        put16(w, w->units[r->unit].placed + r->offset + 2u, w->units[target->unit].placed + target->offset);
    }
    w->free_bytes = 0;
    for (unsigned p = 0; p < place_count; p++) w->free_bytes += places[p].end - places[p].at;
    return 0;
}

unsigned mod_warps_free_bytes(const mod_warps *w) { return w->free_bytes; }

/* ---- the lookups of the game, to check a composition ------------------------------------------ */

#define LOOKUP_STEPS 1024u
#define EDGES 4u
#define PROBES (256u + EDGES)

/* A warp found: its source's group and transition byte, and the destination's three bytes; 1 for a destination past
 * its table, 0 for none found, 2 for a lookup that never ends. */
static uint64_t found(const mod_warps *w, const uint8_t *entry)
{
    const unsigned group = entry[3] >> 4 & 7u;
    if (entry[2] >= w->dest_count[group]) return 1;
    const uint8_t *dest = at(w, u16(w, dest_root(w) + group * 2u) + entry[2] * 3u);
    return (uint64_t)1 << 40 | (uint64_t)entry[3] << 24 | (uint64_t)dest[0] << 16 | (uint64_t)dest[1] << 8 | dest[2];
}

/* findWarpSourceAndDest (bank4.s): a warp tile at `yx` of the room.  Seasons has no $ff end: bit 7 is the default. */
static uint64_t lookup_tile(const mod_warps *w, unsigned group, unsigned room, unsigned yx)
{
    unsigned a = u16(w, source_root(w) + group * 2u), key = room;
    for (unsigned step = 0; step < LOOKUP_STEPS && a >= BANK && a + 4u <= 0x8000u; step++, a += 4u) {
        const uint8_t *entry = at(w, a);
        if (w->ages && entry[0] == 0xffu) return 0;
        if (entry[0] & 0x80u) return found(w, entry);
        if (entry[0] & 0x40u) {
            if (entry[1] == key) { a = (entry[2] | entry[3] << 8) - 4u; key = yx; }
            continue;
        }
        if (!(entry[0] & 0x0fu) && entry[1] == key) return found(w, entry);
    }
    return 2;
}

/* findScreenEdgeWarpSource (bank4.s): leaving by the quadrant of bit `bit`. */
static uint64_t lookup_edge(const mod_warps *w, unsigned group, unsigned room, unsigned bit)
{
    unsigned a = u16(w, source_root(w) + group * 2u);
    for (unsigned step = 0; step < LOOKUP_STEPS && a >= BANK && a + 4u <= 0x8000u; step++, a += 4u) {
        const uint8_t *entry = at(w, a);
        if (entry[0] & 0x80u) return 0;
        if (entry[1] != room) continue;
        if (entry[0] & 0x40u) { a = (entry[2] | entry[3] << 8) - 4u; continue; }
        if (entry[0] & bit) return found(w, entry);
    }
    return 2;
}

uint64_t *mod_warps_lookups(const mod_warps *w)
{
    uint64_t *all = calloc((size_t)8u * 256u * PROBES, sizeof *all);
    if (!all) return NULL;
    for (unsigned g = 0; g < 8u; g++)
        for (unsigned room = 0; room < 256u; room++) {
            uint64_t *probe = all + ((size_t)g * 256u + room) * PROBES;
            for (unsigned yx = 0; yx < 256u; yx++) probe[yx] = lookup_tile(w, g, room, yx);
            for (unsigned edge = 0; edge < EDGES; edge++) probe[256u + edge] = lookup_edge(w, g, room, 1u << edge);
        }
    return all;
}

int mod_warps_check(const uint64_t *before, const uint64_t *after, const mod_warps_house *houses, unsigned count, unsigned inside, unsigned leave,
                    char *error, size_t capacity)
{
    for (unsigned g = 0; g < 8u; g++)
        for (unsigned room = 0; room < 256u; room++) {
            const uint64_t *was = before + ((size_t)g * 256u + room) * PROBES, *is = after + ((size_t)g * 256u + room) * PROBES;
            const mod_warps_house *outside = NULL, *in = NULL;
            for (unsigned i = 0; i < count; i++) {
                if (g == 0u && houses[i].room == room) outside = &houses[i];
                if (g == inside && houses[i].interior == room) in = &houses[i];
            }
            for (unsigned probe = 0; probe < PROBES; probe++) {
                uint64_t expected = was[probe];
                if (outside && probe < 256u && (probe == outside->door || outside->appended))   /* into the house */
                    expected = (uint64_t)1 << 40 | (uint64_t)(inside << 4 | 4u) << 24 | (uint64_t)outside->interior << 16 | 0xff93u;
                if (in && probe >= 256u && (leave >> (probe - 256u) & 1u))                        /* out, on the door */
                    expected = (uint64_t)1 << 40 | (uint64_t)0x03u << 24 | (uint64_t)in->room << 16 | (uint64_t)in->door << 8 | 0x01u;
                if (is[probe] == expected) continue;
                snprintf(error, capacity, "the composed warps of %u/%02x differ from the game's at %s %02x (%010llx, expected %010llx)", g, room,
                         probe < 256u ? "the tile" : "the edge", probe < 256u ? probe : probe - 256u, (unsigned long long)is[probe], (unsigned long long)expected);
                return -1;
            }
        }
    return 0;
}
