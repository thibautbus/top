/* A mod's package and its driver (mod.h): the files read once, the Lua state
 * opened on them, the texts of the game watched for the mod's NPCs, and once
 * a frame the conversation run and the keys held from the game. */
#include "mod_internal.h"

#include "guest_struct_offsets.h"
#include "sha1.h"

#include "lauxlib.h"

#include <stdlib.h>
#include <string.h>

static int by_name(const void *a, const void *b)
{
    return strcmp(((const mod_file *)a)->name, ((const mod_file *)b)->name);
}

static int ends_with(const char *text, const char *suffix)
{
    const size_t a = strlen(text), b = strlen(suffix);
    return a >= b && strcmp(text + a - b, suffix) == 0;
}

/* The package's files, copied (their line ends as LF) and sorted by name; the identity hashes them in that order,
 * with the mod's name. */
static int take_package(OraclesMod *mod, const char *id, const OraclesModFile *files, size_t count, char *error, size_t capacity)
{
    snprintf(mod->id, sizeof mod->id, "%s", id);
    for (const char *c = id; *c; c++)
        if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '-' || *c == '_') || strlen(id) >= sizeof mod->id) {
            snprintf(error, capacity, "a mod's directory is named with a-z, 0-9, - and _ (%s)", id);
            return -1;
        }
    if (count > MOD_FILES) { snprintf(error, capacity, "mod %s: a mod has at most %u files", id, MOD_FILES); return -1; }
    for (size_t i = 0; i < count; i++) {
        if (!ends_with(files[i].name, ".lua") || strlen(files[i].name) >= sizeof mod->files[0].name || files[i].size > 1024u * 1024u) {
            snprintf(error, capacity, "mod %s: %s is not a Lua file of a name under %zu characters and a size under 1 MiB", id, files[i].name, sizeof mod->files[0].name);
            return -1;
        }
        mod_file *file = &mod->files[mod->file_count];
        snprintf(file->name, sizeof file->name, "%s", files[i].name);
        file->text = malloc(files[i].size + 1u);
        if (!file->text) { snprintf(error, capacity, "out of memory"); return -1; }
        /* Line ends as LF: a checkout that writes CRLF (Git for Windows) has the same mod, the same identity. */
        size_t size = 0;
        for (size_t k = 0; k < files[i].size; k++)
            if (!(files[i].text[k] == '\r' && k + 1u < files[i].size && files[i].text[k + 1u] == '\n')) file->text[size++] = files[i].text[k];
        file->text[size] = 0;
        file->size = size;
        mod->file_count++;
    }
    qsort(mod->files, mod->file_count, sizeof mod->files[0], by_name);
    int has_main = 0;
    for (unsigned i = 0; i < mod->file_count; i++) has_main |= !strcmp(mod->files[i].name, "main.lua");
    if (!has_main) { snprintf(error, capacity, "mod %s has no main.lua", id); return -1; }
    size_t total = 0;
    for (unsigned i = 0; i < mod->file_count; i++) total += strlen(mod->files[i].name) + 1u + mod->files[i].size + 1u;
    char *all = malloc(total ? total : 1u);
    if (!all) { snprintf(error, capacity, "out of memory"); return -1; }
    size_t at = 0;
    for (unsigned i = 0; i < mod->file_count; i++) {
        const size_t name = strlen(mod->files[i].name) + 1u;
        memcpy(all + at, mod->files[i].name, name);
        at += name;
        memcpy(all + at, mod->files[i].text, mod->files[i].size + 1u);
        at += mod->files[i].size + 1u;
    }
    char sha1[41];
    oracles_sha1_hex((const uint8_t *)all, total, sha1);
    free(all);
    snprintf(mod->identity, sizeof mod->identity, "%s@%s", mod->id, sha1);
    return 0;
}

OraclesMod *oracles_mod_load(const char *id, const OraclesModFile *files, size_t count, OraclesGame game, const uint8_t *rom, size_t rom_size,
                             char *error, size_t capacity)
{
    OraclesMod *mod = calloc(1, sizeof *mod);
    if (!mod) { snprintf(error, capacity, "out of memory"); return NULL; }
    mod->game = game;
    mod->armed = -1;
    mod->conversation_ref = LUA_NOREF;
    mod->storage_ref = LUA_NOREF;
    mod->slot = -1;
    const OraclesGuestTables *t = game == ORACLES_GAME_AGES ? &oracles_guest_tables_ages : &oracles_guest_tables_seasons;
    mod_font_load(mod, rom, rom_size, t->font_start.bank, t->font_start.addr);
    char reason[512];
    if (take_package(mod, id, files, count, error, capacity) != 0 || mod_lua_open(mod, reason, sizeof reason) != 0) {
        if (mod->L) snprintf(error, capacity, "mod %s: %s", mod->id, reason);
        oracles_mod_free(mod);
        return NULL;
    }
    return mod;
}

/* A text shown in the room of one of the mod's NPCs arms its conversation, which starts once the text closes. */
void mod_on_event(OraclesMod *mod, const OraclesGuestEvent *event)
{
    if (event->type != ORACLES_EVENT_TEXT || mod->conversation || mod->faulted) return;
    const OraclesGuestTables *t = oracles_guest_tables(mod->guest);
    const uint8_t group = oracles_guest_read8(mod->guest, t->active_group), room = oracles_guest_read8(mod->guest, t->active_room);
    for (unsigned i = 0; i < mod->npc_count; i++)
        if (mod->npcs[i].group == group && mod->npcs[i].room == room) mod->armed = (int)i;
}

void oracles_mod_free(OraclesMod *mod)
{
    if (!mod) return;
    mod_lua_close(mod);
    mod_store_free(mod);
    for (unsigned i = 0; i < mod->file_count; i++) free(mod->files[i].text);
    for (unsigned i = 0; i < mod->sprite_count; i++) free(mod->sprites[i].pixels);
    free(mod->presented);
    free(mod);
}

const char *oracles_mod_identity(const OraclesMod *mod) { return mod ? mod->identity : ""; }
unsigned oracles_mod_house_count(const OraclesMod *mod) { return mod ? mod->house_count : 0u; }
unsigned oracles_mod_npc_count(const OraclesMod *mod) { return mod ? mod->npc_count : 0u; }
const char *oracles_mod_description(const OraclesMod *mod) { return mod ? mod->description : ""; }
const char *oracles_mod_error(const OraclesMod *mod) { return mod && mod->faulted ? mod->error : NULL; }
int oracles_mod_busy(const OraclesMod *mod) { return mod && mod->conversation != NULL; }

void mod_attach(OraclesMod *mod, OraclesGuest *guest) { mod->guest = guest; }

void mod_idle_frame(OraclesMod *mod, uint32_t frame, unsigned keys)
{
    mod->frame = frame;
    mod->drawn = 0;
    mod->raw_previous = keys;
    mod->gate &= keys;
}

void oracles_mod_reset(OraclesMod *mod)
{
    if (!mod) return;
    mod_lua_drop_conversation(mod);
    mod->armed = -1;
    mod->gate = 0;
    mod->previous_keys = 0;
}

/* Whether the armed NPC's conversation starts now: 1 when the game is in play in its room and shows no text, 0 while
 * it must wait, -1 when Link has left the room (the NPC is disarmed). */
static int may_start(OraclesMod *mod)
{
    const OraclesGuestTables *t = oracles_guest_tables(mod->guest);
    const mod_npc *npc = &mod->npcs[mod->armed];
    if (oracles_guest_read8(mod->guest, t->active_group) != npc->group || oracles_guest_read8(mod->guest, t->active_room) != npc->room) return -1;
    if (oracles_guest_read8(mod->guest, t->text_is_active) != 0) return 0;
    OraclesGuestInventoryState state;
    oracles_guest_inventory_state(mod->guest, &state);
    return oracles_guest_inventory_refusal(&state) == ORACLES_INVENTORY_APPLIED;
}

/* The keeper of a house whose counter Link faces, pressing A this frame, in play: its index, or -1.  The press is
 * held from the game in the same frame, so that A does not use the item it carries. */
static int keeper_called(OraclesMod *mod, unsigned keys)
{
    if (!(keys & 0x10u) || (mod->raw_previous & 0x10u)) return -1;
    const OraclesGuestTables *t = oracles_guest_tables(mod->guest);
    const uint8_t group = oracles_guest_read8(mod->guest, t->active_group), room = oracles_guest_read8(mod->guest, t->active_room);
    const uint8_t *link = oracles_guest_object(mod->guest, 0, 0);
    if (!link || oracles_guest_read8(mod->guest, t->text_is_active) != 0) return -1;
    const unsigned y = link[ORACLES_OBJ_YH], x = link[ORACLES_OBJ_XH], facing_up = link[ORACLES_OBJ_DIRECTION] == 0u;
    for (unsigned i = 0; i < mod->npc_count; i++) {
        const mod_npc *npc = &mod->npcs[i];
        if (npc->house < 0 || npc->group != group || npc->room != room || !facing_up) continue;
        if (y / 16u != MOD_HOUSE_COUNTER_ROW || x / 16u < MOD_HOUSE_COUNTER_COL_FIRST || x / 16u > MOD_HOUSE_COUNTER_COL_LAST) continue;
        OraclesGuestInventoryState state;
        oracles_guest_inventory_state(mod->guest, &state);
        if (oracles_guest_inventory_refusal(&state) == ORACLES_INVENTORY_APPLIED) return (int)i;
    }
    return -1;
}

static void start_conversation(OraclesMod *mod, uint32_t frame, unsigned keys, int npc)
{
    /* The generator's seed: the frame and the game's own counter, the same in every replay. */
    const uint8_t counter = oracles_guest_read8(mod->guest, oracles_guest_tables(mod->guest)->frame_counter);
    mod->rng = (frame * 2654435761u) ^ ((uint32_t)counter << 8) ^ 0x9e3779b9u;
    if (!mod->rng) mod->rng = 1;
    mod->previous_keys = keys;   /* a key already held is not a press */
    if (mod_lua_start_conversation(mod, (unsigned)npc) == 0) {
        mod->conversations++;
        fprintf(stderr, "oracles: mod %s: frame %u, %s's conversation\n", mod->id, frame, mod->npcs[npc].name);
    }
}

unsigned oracles_mod_frame(OraclesMod *mod, uint32_t frame, unsigned keys)
{
    mod->frame = frame;
    mod->drawn = 0;
    if (!mod->guest) return keys;
    const unsigned raw = keys;
    if (!mod->conversation && !mod->faulted) {
        const int keeper = keeper_called(mod, keys);
        if (keeper >= 0) start_conversation(mod, frame, keys, keeper);   /* the A that called is held, not pressed, in its first frame */
    }
    if (!mod->conversation && mod->armed >= 0 && !mod->faulted) {
        const int npc = mod->armed, start = may_start(mod);
        if (start) mod->armed = -1;
        if (start > 0) {
            start_conversation(mod, frame, keys, npc);
        }
    }
    mod->raw_previous = raw;
    if (mod->conversation) {
        mod_draw_clear(mod);
        const int running = mod_lua_conversation_frame(mod, keys);
        mod->previous_keys = keys;
        mod->frames_held++;
        if (!running) {
            mod->gate = keys;   /* the key that ended it must not reach the game as a press */
            mod->drawn = 0;
        }
        return 0;
    }
    /* A mod that stopped says so on the screen for a few seconds, the reason being in the log. */
    if (mod->fault_frames) {
        mod->fault_frames--;
        mod_draw_clear(mod);
        mod_draw_rect(mod, 0, 0, ORACLES_MOD_WIDTH, 18, 0xff681818u);
        char line[64];
        snprintf(line, sizeof line, "%.10s stopped", mod->id);
        mod_draw_text(mod, line, 4, 1, 0xfff8f8f8u, NULL, 0);
    }
    mod->gate &= keys;
    return keys & ~mod->gate;
}

const uint32_t *oracles_mod_present(OraclesMod *mod, const uint32_t *pixels, uint32_t width, uint32_t height)
{
    if (!mod || !mod->drawn || width < ORACLES_MOD_WIDTH || height < ORACLES_MOD_HEIGHT) return pixels;
    const size_t count = (size_t)width * height;
    if (mod->presented_capacity < count) {
        uint32_t *grown = realloc(mod->presented, count * sizeof *grown);
        if (!grown) return pixels;
        mod->presented = grown;
        mod->presented_capacity = count;
    }
    /* A scene that covers the whole screen of the game (a minigame) darkens what a wider surface shows around it. */
    size_t opaque = 0;
    for (size_t i = 0; i < (size_t)ORACLES_MOD_WIDTH * ORACLES_MOD_HEIGHT; i++) opaque += (mod->surface[i] >> 24) != 0;
    const int dim = opaque == (size_t)ORACLES_MOD_WIDTH * ORACLES_MOD_HEIGHT;
    for (size_t i = 0; i < count; i++) mod->presented[i] = dim ? 0xff000000u | ((pixels[i] >> 2) & 0x3f3f3fu) : pixels[i];
    const uint32_t left = (width - ORACLES_MOD_WIDTH) / 2u, top = (height - ORACLES_MOD_HEIGHT) / 2u;
    for (uint32_t y = 0; y < ORACLES_MOD_HEIGHT; y++)
        for (uint32_t x = 0; x < ORACLES_MOD_WIDTH; x++) {
            const uint32_t argb = mod->surface[y * ORACLES_MOD_WIDTH + x];
            if (argb >> 24) mod->presented[(size_t)(top + y) * width + left + x] = argb;
        }
    return mod->presented;
}

uint64_t oracles_mod_fingerprint(const OraclesMod *mod)
{
    uint64_t h = ORACLES_HASH_SEED;
    const uint32_t state[] = { mod->conversation != NULL, (uint32_t)(mod->armed + 1), mod->gate, mod->rng, (uint32_t)mod->drawn,
                               mod->calls_queued, mod->calls_refused, (uint32_t)mod->faulted, (uint32_t)(mod->slot + 1) };
    h = oracles_guest_hash((const uint8_t *)state, sizeof state, h);
    h = oracles_guest_hash((const uint8_t *)&mod->storage_hash, sizeof mod->storage_hash, h);
    if (mod->drawn) h = oracles_guest_hash((const uint8_t *)mod->surface, sizeof mod->surface, h);
    return h;
}

void oracles_mod_summary(const OraclesMod *mod, FILE *out)
{
    const char *id = mod->id;
    fprintf(out, "mod.%s.conversations=%u\nmod.%s.frames_held=%u\nmod.%s.calls_queued=%u\nmod.%s.calls_refused=%u\nmod.%s.errors=%u\n",
            id, mod->conversations, id, mod->frames_held, id, mod->calls_queued, id, mod->calls_refused, id, mod->lua_errors);
    if (mod->faulted) fprintf(out, "mod.%s.error=%s\n", id, mod->error);
}
