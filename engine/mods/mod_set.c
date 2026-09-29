/* The mods a session runs together (mod.h, OraclesModSet): their houses
 * composed into one image, one listener on the guest for all of them, and
 * every frame the mods driven in the order of their names, one conversation
 * at a time. */
#include "mod_internal.h"

#include <stdlib.h>
#include <string.h>

#define KEPT_MAX 64u
#define STORAGE_MAGIC "oracles-mod-storage 1\n"

/* A slot of a mod the session did not load, read from the storage file and written back as it was. */
typedef struct kept { char id[64]; unsigned slot; char *data; size_t size; } kept;

struct OraclesModSet {
    OraclesMod *mods[ORACLES_MOD_SET_MAX];
    size_t count;
    char identity[ORACLES_MOD_SET_MAX * 128u];
    OraclesGuest *guest;
    kept kept[KEPT_MAX];
    unsigned kept_count;
    int loaded_slot;                               /* the file the game loaded or created last, -1 before */
    unsigned kept_changes;                         /* the kept slots followed a file operation */
};

static int by_id(const void *a, const void *b)
{
    return strcmp((*(OraclesMod *const *)a)->id, (*(OraclesMod *const *)b)->id);
}

/* Two mods that talk as one of the game's NPCs: the first found, or -1. */
static int shared_npc(const OraclesMod *a, const OraclesMod *b, unsigned *npc)
{
    for (unsigned i = 0; i < a->npc_count; i++)
        for (unsigned k = 0; k < b->npc_count; k++)
            if (a->npcs[i].house < 0 && b->npcs[k].house < 0 && a->npcs[i].group == b->npcs[k].group && a->npcs[i].room == b->npcs[k].room) {
                *npc = i;
                return (int)k;
            }
    return -1;
}

OraclesModSet *oracles_mod_set_create(OraclesMod **mods, size_t count, char *error, size_t capacity)
{
    OraclesModSet *set = calloc(1, sizeof *set);
    if (!set || count > ORACLES_MOD_SET_MAX) {
        snprintf(error, capacity, set ? "at most %u mods at once" : "out of memory", ORACLES_MOD_SET_MAX);
        for (size_t i = 0; i < count; i++) oracles_mod_free(mods[i]);
        free(set);
        return NULL;
    }
    memcpy(set->mods, mods, count * sizeof *mods);
    set->count = count;
    set->loaded_slot = -1;
    qsort(set->mods, count, sizeof *set->mods, by_id);
    for (size_t i = 1; i < count; i++)
        for (size_t k = 0; k < i; k++) {
            unsigned npc = 0;
            const int other = shared_npc(set->mods[k], set->mods[i], &npc);
            if (!strcmp(set->mods[k]->id, set->mods[i]->id)) snprintf(error, capacity, "two mods are named %s", set->mods[i]->id);
            else if (other >= 0)
                snprintf(error, capacity, "mods %s and %s both talk as the NPC of %u/%02x (%s and %s)", set->mods[k]->id, set->mods[i]->id,
                         set->mods[k]->npcs[npc].group, set->mods[k]->npcs[npc].room, set->mods[k]->npcs[npc].name, set->mods[i]->npcs[other].name);
            else continue;
            oracles_mod_set_free(set);
            return NULL;
        }
    size_t at = 0;
    for (size_t i = 0; i < count; i++)
        at += (size_t)snprintf(set->identity + at, sizeof set->identity - at, "%s%s", i ? "," : "", set->mods[i]->identity);
    return set;
}

/* The slots of the mods not loaded follow the file operations as the loaded ones do: a new or erased file empties
 * them, a save to another file than the one loaded (a copy) copies them. */
static void follow_kept(OraclesModSet *set, unsigned operation, unsigned slot)
{
    if (operation == 2u) { set->loaded_slot = (int)slot; return; }
    if (operation == 0u || operation == 3u) {
        for (unsigned i = 0; i < set->kept_count;) {
            if (set->kept[i].slot != slot) { i++; continue; }
            free(set->kept[i].data);
            set->kept[i] = set->kept[--set->kept_count];
            set->kept_changes++;
        }
        if (operation == 0u) set->loaded_slot = (int)slot;
        return;
    }
    if (operation != 1u || set->loaded_slot < 0 || (unsigned)set->loaded_slot == slot) return;
    for (unsigned i = 0; i < set->kept_count;) {   /* the target's own slots go, then the source's are copied */
        if (set->kept[i].slot == slot) { free(set->kept[i].data); set->kept[i] = set->kept[--set->kept_count]; set->kept_changes++; }
        else i++;
    }
    const unsigned sources = set->kept_count;
    for (unsigned i = 0; i < sources && set->kept_count < KEPT_MAX; i++) {
        if (set->kept[i].slot != (unsigned)set->loaded_slot) continue;
        kept *copy = &set->kept[set->kept_count];
        *copy = set->kept[i];
        copy->slot = slot;
        copy->data = set->kept[i].size ? malloc(set->kept[i].size) : NULL;
        if (set->kept[i].size && !copy->data) continue;
        if (copy->data) memcpy(copy->data, set->kept[i].data, set->kept[i].size);
        set->kept_count++;
        set->kept_changes++;
    }
}

void mod_set_file_operation(OraclesModSet *set, unsigned operation, unsigned slot)
{
    if (slot >= MOD_SLOTS) return;
    follow_kept(set, operation, slot);
    for (size_t i = 0; i < set->count; i++) mod_store_file_operation(set->mods[i], operation, slot);
}

static void on_event(void *opaque, const OraclesGuestEvent *event)
{
    OraclesModSet *set = opaque;
    if (event->type == ORACLES_EVENT_FILE_OPERATION) {
        mod_set_file_operation(set, event->c, oracles_guest_read8(set->guest, oracles_guest_tables(set->guest)->active_file_slot));
        return;
    }
    for (size_t i = 0; i < set->count; i++) mod_on_event(set->mods[i], event);
}

void oracles_mod_set_free(OraclesModSet *set)
{
    if (!set) return;
    if (set->guest) oracles_guest_remove_event_listener(set->guest, on_event, set);
    for (size_t i = 0; i < set->count; i++) oracles_mod_free(set->mods[i]);
    for (unsigned i = 0; i < set->kept_count; i++) free(set->kept[i].data);
    free(set);
}

size_t oracles_mod_set_count(const OraclesModSet *set) { return set ? set->count : 0u; }
OraclesMod *oracles_mod_set_mod(const OraclesModSet *set, size_t index) { return set && index < set->count ? set->mods[index] : NULL; }
const char *oracles_mod_set_identity(const OraclesModSet *set) { return set ? set->identity : ""; }

unsigned oracles_mod_set_house_count(const OraclesModSet *set)
{
    unsigned n = 0;
    for (size_t i = 0; set && i < set->count; i++) n += set->mods[i]->house_count;
    return n;
}

int oracles_mod_set_compose(OraclesModSet *set, const uint8_t *rom, size_t rom_size, uint8_t **out, size_t *out_size, char *error, size_t capacity)
{
    return mod_compose(set->mods, set->count, rom, rom_size, out, out_size, error, capacity);
}

int oracles_mod_set_attach(OraclesModSet *set, OraclesGuest *guest, char *error, size_t capacity)
{
    if (oracles_guest_arm_calls(guest) != 0) {
        snprintf(error, capacity, "mods %s: the game's calls cannot be armed in this ROM (an original US ROM is needed)", set->identity);
        return -1;
    }
    const OraclesGuestSym files = oracles_guest_tables(guest)->file_management_function;
    if (!files.addr || oracles_guest_add_hook(guest, files, ORACLES_EVENT_FILE_OPERATION, 0) != 0) {
        snprintf(error, capacity, "mods: the game's file operations cannot be watched (mod.storage)");
        return -1;
    }
    if (oracles_guest_add_event_listener(guest, on_event, set) != 0) {
        snprintf(error, capacity, "mods: the guest has no room for another listener");
        return -1;
    }
    set->guest = guest;
    for (size_t i = 0; i < set->count; i++) mod_attach(set->mods[i], guest);
    return 0;
}

unsigned oracles_mod_set_frame(OraclesModSet *set, uint32_t frame, unsigned keys)
{
    OraclesMod *busy = NULL;
    for (size_t i = 0; i < set->count; i++) if (set->mods[i]->conversation) busy = set->mods[i];
    if (busy) {
        for (size_t i = 0; i < set->count; i++) if (set->mods[i] != busy) mod_idle_frame(set->mods[i], frame, keys);
        return oracles_mod_frame(busy, frame, keys);
    }
    unsigned out = keys;
    int started = 0;
    for (size_t i = 0; i < set->count; i++) {
        if (started) { mod_idle_frame(set->mods[i], frame, keys); continue; }   /* one conversation at a time */
        out &= oracles_mod_frame(set->mods[i], frame, keys);
        started = set->mods[i]->conversation != NULL;
    }
    return started ? 0u : out;
}

const uint32_t *oracles_mod_set_present(OraclesModSet *set, const uint32_t *pixels, uint32_t width, uint32_t height)
{
    for (size_t i = 0; set && i < set->count; i++) pixels = oracles_mod_present(set->mods[i], pixels, width, height);
    return pixels;
}

int oracles_mod_set_busy(const OraclesModSet *set)
{
    for (size_t i = 0; set && i < set->count; i++) if (oracles_mod_busy(set->mods[i])) return 1;
    return 0;
}

void oracles_mod_set_reset(OraclesModSet *set)
{
    for (size_t i = 0; set && i < set->count; i++) oracles_mod_reset(set->mods[i]);
}

uint64_t oracles_mod_set_fingerprint(const OraclesModSet *set)
{
    uint64_t prints[ORACLES_MOD_SET_MAX];
    for (size_t i = 0; i < set->count; i++) prints[i] = oracles_mod_fingerprint(set->mods[i]);
    return set->count == 1u ? prints[0] : oracles_guest_hash((const uint8_t *)prints, set->count * sizeof prints[0], ORACLES_HASH_SEED);
}

void oracles_mod_set_summary(const OraclesModSet *set, FILE *out)
{
    fprintf(out, "mod.identity=%s\n", set->identity);
    for (size_t i = 0; i < set->count; i++) oracles_mod_summary(set->mods[i], out);
    fprintf(out, "mod.calls_run=%u\n", set->guest ? oracles_guest_calls_done(set->guest) : 0u);
}

/* ---- storage ------------------------------------------------------------------------------------ */

static OraclesMod *mod_named(const OraclesModSet *set, const char *id)
{
    for (size_t i = 0; i < set->count; i++) if (!strcmp(set->mods[i]->id, id)) return set->mods[i];
    return NULL;
}

unsigned oracles_mod_set_storage_changes(const OraclesModSet *set)
{
    unsigned n = set ? set->kept_changes : 0u;
    for (size_t i = 0; set && i < set->count; i++) n += set->mods[i]->storage_changes;
    return n;
}

/* One entry of the storage's text, read in place. */
typedef struct entry { int live; char id[64]; int slot; const char *bytes; size_t size; } entry;

/* The entries of the text, checked whole: every line, and every live table a loaded mod would decode.  NULL with the
 * reason; *count the entries. */
static entry *read_entries(OraclesModSet *set, const uint8_t *data, size_t size, int state, size_t *count, char *error, size_t capacity)
{
    const size_t magic = strlen(STORAGE_MAGIC);
    *count = 0;
    if (size < magic || memcmp(data, STORAGE_MAGIC, magic) != 0) { snprintf(error, capacity, "not a storage of mods (%s)", "oracles-mod-storage 1"); return NULL; }
    size_t lines = 1;
    for (size_t i = magic; i < size; i++) lines += data[i] == '\n';
    entry *entries = calloc(lines, sizeof *entries);
    if (!entries) { snprintf(error, capacity, "out of memory"); return NULL; }
    for (size_t at = magic; at < size;) {
        const uint8_t *end = memchr(data + at, '\n', size - at);
        char line[160], kind[8];
        entry *e = &entries[*count];
        unsigned long long length = 0;
        if (!end || (size_t)(end - (data + at)) >= sizeof line) { snprintf(error, capacity, "a line of the storage is cut short"); free(entries); return NULL; }
        memcpy(line, data + at, (size_t)(end - (data + at)));
        line[end - (data + at)] = 0;
        at = (size_t)(end - data) + 1u;
        if (sscanf(line, "%7s %63s %d %llu", kind, e->id, &e->slot, &length) != 4 || e->slot < -1 || e->slot >= (int)MOD_SLOTS || length > MOD_STORAGE_LIMIT
            || length + 1u > size - at || data[at + length] != '\n' || (strcmp(kind, "slot") != 0 && strcmp(kind, "live") != 0)
            || (!strcmp(kind, "slot") && e->slot < 0)) {
            snprintf(error, capacity, "the storage has a bad line: %s", line);
            free(entries);
            return NULL;
        }
        e->live = !strcmp(kind, "live");
        e->bytes = (const char *)data + at;
        e->size = (size_t)length;
        at += (size_t)length + 1u;
        OraclesMod *mod = mod_named(set, e->id);
        if (e->live && state && mod && !mod->faulted) {
            char reason[256];
            if (mod_store_check(mod, e->bytes, e->size, reason, sizeof reason) != 0) {
                snprintf(error, capacity, "mod %s: %s", e->id, reason);
                free(entries);
                return NULL;
            }
        }
        (*count)++;
    }
    return entries;
}

int oracles_mod_set_storage_load(OraclesModSet *set, const uint8_t *data, size_t size, int state, char *error, size_t capacity)
{
    size_t count = 0;
    entry *entries = read_entries(set, data, size, state, &count, error, capacity);
    if (!entries) return -1;                       /* nothing replaced */
    /* What the data holds replaces what the loaded mods hold: a savestate carries every slot and the live tables. */
    for (size_t i = 0; i < set->count; i++) {
        OraclesMod *mod = set->mods[i];
        for (unsigned slot = 0; slot < MOD_SLOTS; slot++) mod_store_set_slot(mod, slot, NULL, 0, error, capacity);
        if (state && !mod->faulted) mod_store_decode_live(mod, "", 0, error, capacity);
        if (state) mod->slot = -1;
    }
    if (!state) {
        for (unsigned i = 0; i < set->kept_count; i++) free(set->kept[i].data);
        set->kept_count = 0;
    }
    int status = 0;
    for (size_t i = 0; i < count; i++) {
        const entry *e = &entries[i];
        OraclesMod *mod = mod_named(set, e->id);
        if (!e->live) {
            if (mod) { if (mod_store_set_slot(mod, (unsigned)e->slot, e->bytes, e->size, error, capacity) != 0) status = -1; continue; }
            if (state || set->kept_count == KEPT_MAX) continue;
            kept *k = &set->kept[set->kept_count];
            k->data = e->size ? malloc(e->size) : NULL;
            if (e->size && !k->data) { snprintf(error, capacity, "out of memory"); status = -1; continue; }
            if (e->size) memcpy(k->data, e->bytes, e->size);
            snprintf(k->id, sizeof k->id, "%s", e->id);
            k->slot = (unsigned)e->slot;
            k->size = e->size;
            set->kept_count++;
        } else if (state && mod && !mod->faulted) {
            char reason[256];
            if (mod_store_decode_live(mod, e->bytes, e->size, reason, sizeof reason) != 0) { snprintf(error, capacity, "mod %s: %s", e->id, reason); status = -1; continue; }
            mod->slot = e->slot;
        }
    }
    free(entries);
    for (size_t i = 0; i < set->count; i++) set->mods[i]->storage_changes++;   /* what the host writes changed */
    return status;
}

typedef struct text { uint8_t *data; size_t size, capacity; int failed; } text;

static void put(text *t, const void *bytes, size_t n)
{
    if (t->failed) return;
    if (t->size + n > t->capacity) {
        size_t capacity = t->capacity ? t->capacity * 2u : 4096u;
        while (capacity < t->size + n) capacity *= 2u;
        uint8_t *grown = realloc(t->data, capacity);
        if (!grown) { t->failed = 1; return; }
        t->data = grown;
        t->capacity = capacity;
    }
    memcpy(t->data + t->size, bytes, n);
    t->size += n;
}

static void put_entry(text *t, const char *kind, const char *id, int slot, const char *bytes, size_t size)
{
    char line[160];
    const int n = snprintf(line, sizeof line, "%s %s %d %zu\n", kind, id, slot, size);
    put(t, line, (size_t)n);
    if (size) put(t, bytes, size);
    put(t, "\n", 1);
}

uint8_t *oracles_mod_set_storage_save(OraclesModSet *set, int state, size_t *size, char *error, size_t capacity)
{
    text t = { NULL, 0, 0, 0 };
    *size = 0;
    put(&t, STORAGE_MAGIC, strlen(STORAGE_MAGIC));
    for (size_t i = 0; i < set->count; i++) {
        OraclesMod *mod = set->mods[i];
        for (unsigned slot = 0; slot < MOD_SLOTS; slot++)
            if (mod->stored[slot]) put_entry(&t, "slot", mod->id, (int)slot, mod->stored[slot], mod->stored_size[slot]);
        if (!state || mod->faulted) continue;
        size_t live_size = 0;
        char reason[256];
        char *live = mod_store_encode_live(mod, &live_size, reason, sizeof reason);
        if (!live) { snprintf(error, capacity, "mod %s: %s", mod->id, reason); free(t.data); return NULL; }
        put_entry(&t, "live", mod->id, mod->slot, live, live_size);
        free(live);
    }
    for (unsigned i = 0; !state && i < set->kept_count; i++) put_entry(&t, "slot", set->kept[i].id, (int)set->kept[i].slot, set->kept[i].data, set->kept[i].size);
    if (t.failed) { free(t.data); snprintf(error, capacity, "out of memory"); return NULL; }
    *size = t.size;
    return t.data;
}

int oracles_mod_set_house_door(const OraclesModSet *set, const char *name, uint8_t *room, uint8_t *y, uint8_t *x, char *error, size_t capacity)
{
    const char *slash = strchr(name, '/');
    int found = 0;
    for (size_t i = 0; i < set->count; i++) {
        const OraclesMod *mod = set->mods[i];
        if (slash && (strncmp(mod->id, name, (size_t)(slash - name)) != 0 || mod->id[slash - name])) continue;
        for (unsigned h = 0; h < mod->house_count; h++) {
            const mod_house *house = &mod->houses[h];
            if (strcmp(house->name, slash ? slash + 1 : name) != 0) continue;
            if (found++) { snprintf(error, capacity, "two mods have a house named %s: name it MOD/%s", name, name); return -1; }
            *room = house->room;
            *y = (uint8_t)((house->row + 3u) * 16u + 8u);   /* the middle of the tile in front of the door */
            *x = (uint8_t)((house->col + 1u) * 16u + 8u);
        }
    }
    if (!found) snprintf(error, capacity, "no mod loaded has a house named %s", name);
    return found ? 0 : -1;
}
