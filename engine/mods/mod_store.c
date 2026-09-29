/* A mod's storage (mod.storage): a table saved with the game's files.
 *
 * The table follows the game's own file operations (fileManagementFunction,
 * code/fileManagement.s): when the game creates a file its storage starts
 * empty, when it saves one the table is encoded into that file's slot, when
 * it loads one the table becomes what that slot holds, when it erases one the
 * slot is emptied.  A copy of a file (a load, then a save to the other slot)
 * copies the storage with it.  The host keeps the encoded slots and writes
 * them beside the save (mod.h, oracles_mod_set_storage_save).
 *
 * The encoding is a text the same on every platform: booleans (t, f),
 * integers (i<decimal>;), floats by their bits (d<16 hex digits>;), strings
 * (s<length>:<bytes>) and tables ({ key value ... }, the keys sorted: whole
 * numbers first, then strings by their bytes).  Other values, other keys, a
 * table deeper than MOD_STORAGE_DEPTH or wider than MOD_STORAGE_KEYS, or an
 * encoding over MOD_STORAGE_LIMIT bytes are refused: the mod stops, with the
 * reason, and its slots keep what they held. */
#include "mod_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct buffer { char *data; size_t size; } buffer;   /* data holds MOD_STORAGE_LIMIT bytes */

static void emit(lua_State *L, buffer *b, const void *bytes, size_t n)
{
    if (b->size + n > MOD_STORAGE_LIMIT) luaL_error(L, "mod.storage holds more than %d bytes once saved", (int)MOD_STORAGE_LIMIT);
    memcpy(b->data + b->size, bytes, n);
    b->size += n;
}

typedef struct key { int is_string; lua_Integer integer; const char *text; size_t length; } key;

static int by_key(const void *a, const void *b)
{
    const key *x = a, *y = b;
    if (x->is_string != y->is_string) return x->is_string - y->is_string;
    if (!x->is_string) return x->integer < y->integer ? -1 : x->integer > y->integer;
    const size_t n = x->length < y->length ? x->length : y->length;
    const int c = memcmp(x->text, y->text, n);
    return c ? c : (x->length < y->length ? -1 : x->length > y->length);
}

static void encode_value(lua_State *L, int index, buffer *b, int depth);

static void encode_string(lua_State *L, buffer *b, const char *text, size_t length)
{
    char head[32];
    const int n = snprintf(head, sizeof head, "s%zu:", length);
    emit(L, b, head, (size_t)n);
    emit(L, b, text, length);
}

static void encode_integer(lua_State *L, buffer *b, lua_Integer value)
{
    char text[32];
    const int n = snprintf(text, sizeof text, "i%lld;", (long long)value);
    emit(L, b, text, (size_t)n);
}

/* The table at `index`: its keys collected, checked and sorted, then each pair. */
static void encode_table(lua_State *L, int index, buffer *b, int depth)
{
    if (depth >= (int)MOD_STORAGE_DEPTH) luaL_error(L, "mod.storage has tables more than %d deep", (int)MOD_STORAGE_DEPTH);
    luaL_checkstack(L, 8, "mod.storage is too deep");
    index = lua_absindex(L, index);
    size_t count = 0;
    lua_pushnil(L);
    while (lua_next(L, index)) {
        lua_pop(L, 1);
        if (++count > MOD_STORAGE_KEYS) luaL_error(L, "a table of mod.storage has more than %d keys", (int)MOD_STORAGE_KEYS);
    }
    key *keys = lua_newuserdatauv(L, (count ? count : 1u) * sizeof *keys, 0);   /* freed by the collector, even on an error */
    size_t n = 0;
    lua_pushnil(L);
    while (lua_next(L, index)) {
        lua_pop(L, 1);
        if (lua_type(L, -1) == LUA_TSTRING) {
            keys[n].is_string = 1;
            keys[n].text = lua_tolstring(L, -1, &keys[n].length);   /* the table keeps the string alive */
        } else if (lua_isinteger(L, -1)) {
            keys[n].is_string = 0;
            keys[n].integer = lua_tointeger(L, -1);
        } else {
            luaL_error(L, "a key of mod.storage is a %s: keys are whole numbers or strings", luaL_typename(L, -1));
        }
        n++;
    }
    qsort(keys, n, sizeof *keys, by_key);
    emit(L, b, "{", 1);
    for (size_t i = 0; i < n; i++) {
        if (keys[i].is_string) {
            encode_string(L, b, keys[i].text, keys[i].length);
            lua_pushlstring(L, keys[i].text, keys[i].length);
        } else {
            encode_integer(L, b, keys[i].integer);
            lua_pushinteger(L, keys[i].integer);
        }
        lua_rawget(L, index);
        encode_value(L, -1, b, depth + 1);
        lua_pop(L, 1);
    }
    emit(L, b, "}", 1);
    lua_pop(L, 1);   /* the keys */
}

static void encode_value(lua_State *L, int index, buffer *b, int depth)
{
    switch (lua_type(L, index)) {
    case LUA_TBOOLEAN:
        emit(L, b, lua_toboolean(L, index) ? "t" : "f", 1);
        return;
    case LUA_TNUMBER:
        if (lua_isinteger(L, index)) { encode_integer(L, b, lua_tointeger(L, index)); return; }
        {
            const double value = (double)lua_tonumber(L, index);
            if (!isfinite(value)) luaL_error(L, "mod.storage holds a number that is not finite");
            uint64_t bits;
            memcpy(&bits, &value, sizeof bits);
            char text[32];
            const int n = snprintf(text, sizeof text, "d%016llx;", (unsigned long long)bits);
            emit(L, b, text, (size_t)n);
        }
        return;
    case LUA_TSTRING: {
        size_t length = 0;
        const char *text = lua_tolstring(L, index, &length);
        encode_string(L, b, text, length);
        return;
    }
    case LUA_TTABLE:
        encode_table(L, index, b, depth);
        return;
    default:
        luaL_error(L, "mod.storage holds a %s: only booleans, numbers, strings and tables are saved", luaL_typename(L, index));
    }
}

static int encode_protected(lua_State *L)
{
    encode_value(L, 2, lua_touserdata(L, 1), 0);
    return 0;
}

/* ---- decoding ------------------------------------------------------------------------------------ */

typedef struct reader { const char *data; size_t size, at; } reader;

static long long read_number(lua_State *L, reader *r, int base)
{
    char text[32];
    size_t n = 0;
    while (r->at < r->size && r->data[r->at] != ';' && r->data[r->at] != ':' && n + 1u < sizeof text) text[n++] = r->data[r->at++];
    if (r->at >= r->size || n == 0) luaL_error(L, "the storage is cut short");
    r->at++;   /* ; or : */
    text[n] = 0;
    char *end = NULL;
    const unsigned long long magnitude = strtoull(text[0] == '-' ? text + 1 : text, &end, base);
    if (*end) luaL_error(L, "the storage has a bad number");
    return (long long)(text[0] == '-' ? 0ull - magnitude : magnitude);
}

static void decode_value(lua_State *L, reader *r, int depth)
{
    if (r->at >= r->size) luaL_error(L, "the storage is cut short");
    luaL_checkstack(L, 8, "the storage is too deep");
    const char tag = r->data[r->at++];
    switch (tag) {
    case 't': lua_pushboolean(L, 1); return;
    case 'f': lua_pushboolean(L, 0); return;
    case 'i': lua_pushinteger(L, (lua_Integer)read_number(L, r, 10)); return;
    case 'd': {
        const uint64_t bits = (uint64_t)read_number(L, r, 16);
        double value;
        memcpy(&value, &bits, sizeof value);
        lua_pushnumber(L, value);
        return;
    }
    case 's': {
        const long long length = read_number(L, r, 10);
        if (length < 0 || (size_t)length > r->size - r->at) luaL_error(L, "the storage is cut short");
        lua_pushlstring(L, r->data + r->at, (size_t)length);
        r->at += (size_t)length;
        return;
    }
    case '{':
        if (depth >= (int)MOD_STORAGE_DEPTH) luaL_error(L, "the storage is too deep");
        lua_newtable(L);
        while (r->at < r->size && r->data[r->at] != '}') {
            decode_value(L, r, depth + 1);
            if (lua_type(L, -1) != LUA_TSTRING && !lua_isinteger(L, -1)) luaL_error(L, "the storage has a bad key");
            decode_value(L, r, depth + 1);
            lua_rawset(L, -3);
        }
        if (r->at >= r->size) luaL_error(L, "the storage is cut short");
        r->at++;
        return;
    default:
        luaL_error(L, "the storage has a bad value");
    }
}

/* Protected: the table encoded at argument 1 (a reader), replacing the contents of the table at argument 2. */
static int decode_protected(lua_State *L)
{
    reader *r = lua_touserdata(L, 1);
    if (r->size) decode_value(L, r, 0);
    else lua_newtable(L);
    if (!lua_istable(L, -1) || r->at != r->size) luaL_error(L, "the storage is not one table");
    lua_newtable(L);                               /* 4: the live table's keys, collected before it is cleared */
    int n = 0;
    lua_pushnil(L);
    while (lua_next(L, 2)) { lua_pop(L, 1); lua_pushvalue(L, -1); lua_rawseti(L, 4, ++n); }
    for (int i = 1; i <= n; i++) { lua_rawgeti(L, 4, i); lua_pushnil(L); lua_rawset(L, 2); }
    lua_pop(L, 1);
    lua_pushnil(L);
    while (lua_next(L, 3)) { lua_pushvalue(L, -2); lua_insert(L, -2); lua_rawset(L, 2); }
    return 0;
}

/* Protected: the table encoded at argument 1 (a reader), decoded and dropped, to check it. */
static int check_protected(lua_State *L)
{
    reader *r = lua_touserdata(L, 1);
    if (r->size) decode_value(L, r, 0);
    else lua_newtable(L);
    if (!lua_istable(L, -1) || r->at != r->size) luaL_error(L, "the storage is not one table");
    return 0;
}

int mod_store_check(OraclesMod *mod, const char *data, size_t size, char *error, size_t capacity)
{
    lua_State *L = mod->L;
    reader r = { data, size, 0 };
    const long budget = mod->budget;
    mod->budget = MOD_FRAME_BUDGET;
    lua_pushcfunction(L, check_protected);
    lua_pushlightuserdata(L, &r);
    const int status = lua_pcall(L, 1, 0, 0);
    mod->budget = budget;
    if (status == LUA_OK) return 0;
    snprintf(error, capacity, "%s", lua_tostring(L, -1) ? lua_tostring(L, -1) : "the storage cannot be read");
    lua_pop(L, 1);
    return -1;
}

/* ---- the slots ------------------------------------------------------------------------------------ */

static uint64_t storage_hash(const OraclesMod *mod)
{
    uint64_t h = ORACLES_HASH_SEED;
    for (unsigned slot = 0; slot < MOD_SLOTS; slot++) {
        const uint32_t size = (uint32_t)mod->stored_size[slot];
        h = oracles_guest_hash((const uint8_t *)&size, sizeof size, h);
        if (size) h = oracles_guest_hash((const uint8_t *)mod->stored[slot], size, h);
    }
    return h;
}

static void set_slot(OraclesMod *mod, unsigned slot, char *data, size_t size)
{
    free(mod->stored[slot]);
    mod->stored[slot] = data;
    mod->stored_size[slot] = data ? size : 0;
    mod->storage_hash = storage_hash(mod);
}

char *mod_store_encode_live(OraclesMod *mod, size_t *size, char *error, size_t capacity)
{
    buffer b = { malloc(MOD_STORAGE_LIMIT), 0 };
    *size = 0;
    if (!b.data || !mod->L) { free(b.data); snprintf(error, capacity, "out of memory"); return NULL; }
    lua_State *L = mod->L;
    const long budget = mod->budget;
    mod->budget = MOD_FRAME_BUDGET;
    lua_pushcfunction(L, encode_protected);
    lua_pushlightuserdata(L, &b);
    lua_rawgeti(L, LUA_REGISTRYINDEX, mod->storage_ref);
    mod->over_budget = 0;
    mod->charging = 1;
    const int status = lua_pcall(L, 2, 0, 0);
    mod->charging = 0;
    if (status != LUA_OK) {
        snprintf(error, capacity, "%s", mod_error_message(mod, lua_tostring(L, -1) ? lua_tostring(L, -1) : "mod.storage cannot be saved"));
        lua_pop(L, 1);
        mod->budget = budget;
        free(b.data);
        return NULL;
    }
    mod->budget = budget;
    *size = b.size;
    return b.data;
}

int mod_store_decode_live(OraclesMod *mod, const char *data, size_t size, char *error, size_t capacity)
{
    lua_State *L = mod->L;
    reader r = { data, size, 0 };
    const long budget = mod->budget;
    mod->budget = MOD_FRAME_BUDGET;
    lua_pushcfunction(L, decode_protected);
    lua_pushlightuserdata(L, &r);
    lua_rawgeti(L, LUA_REGISTRYINDEX, mod->storage_ref);
    mod->over_budget = 0;
    mod->charging = 1;
    const int status = lua_pcall(L, 2, 0, 0);
    mod->charging = 0;
    mod->budget = budget;
    if (status == LUA_OK) return 0;
    snprintf(error, capacity, "%s", mod_error_message(mod, lua_tostring(L, -1) ? lua_tostring(L, -1) : "the storage cannot be read"));
    lua_pop(L, 1);
    return -1;
}

/* A stopped mod's slots follow the file operations without its Lua: a new or erased file empties its slot, a save to
 * another file than the one loaded (a copy) copies the slot, a save of the file loaded keeps what it last saved. */
static void follow_stopped(OraclesMod *mod, unsigned operation, unsigned slot)
{
    if (operation == 0u || operation == 3u) {
        set_slot(mod, slot, NULL, 0);
        if (operation == 0u) mod->slot = (int)slot;
        mod->storage_changes++;
    } else if (operation == 1u && mod->slot >= 0 && (unsigned)mod->slot != slot) {
        const unsigned from = (unsigned)mod->slot;
        char *copy = mod->stored_size[from] ? malloc(mod->stored_size[from]) : NULL;
        if (mod->stored_size[from] && !copy) return;
        if (copy) memcpy(copy, mod->stored[from], mod->stored_size[from]);
        set_slot(mod, slot, copy, mod->stored_size[from]);
        mod->storage_changes++;
    } else if (operation == 2u) {
        mod->slot = (int)slot;
    }
}

void mod_store_file_operation(OraclesMod *mod, unsigned operation, unsigned slot)
{
    if (slot >= MOD_SLOTS || !mod->L) return;
    if (mod->faulted) { follow_stopped(mod, operation, slot); return; }
    char error[256];
    switch (operation) {
    case 0:                                        /* create: an empty storage, saved at once (initializeFile runs into saveFile) */
        if (mod_store_decode_live(mod, "", 0, error, sizeof error) != 0) { mod_fault(mod, "mod.storage: %s", error); return; }
        mod->slot = (int)slot;
        set_slot(mod, slot, NULL, 0);
        mod->storage_changes++;
        return;
    case 1: {                                      /* save */
        size_t size = 0;
        char *data = mod_store_encode_live(mod, &size, error, sizeof error);
        if (!data) { mod_fault(mod, "mod.storage cannot be saved: %s", error); return; }
        set_slot(mod, slot, data, size);
        mod->storage_changes++;
        return;
    }
    case 2:                                        /* load */
        if (mod_store_decode_live(mod, mod->stored[slot] ? mod->stored[slot] : "", mod->stored_size[slot], error, sizeof error) != 0) {
            mod_fault(mod, "mod.storage of file %u cannot be read: %s", slot + 1u, error);
            return;
        }
        mod->slot = (int)slot;
        return;
    case 3:                                        /* erase */
        set_slot(mod, slot, NULL, 0);
        mod->storage_changes++;
        return;
    default:
        return;
    }
}

int mod_store_set_slot(OraclesMod *mod, unsigned slot, const char *data, size_t size, char *error, size_t capacity)
{
    if (slot >= MOD_SLOTS) { snprintf(error, capacity, "no file %u", slot + 1u); return -1; }
    if (size > MOD_STORAGE_LIMIT) { snprintf(error, capacity, "the storage of file %u is over %u bytes", slot + 1u, MOD_STORAGE_LIMIT); return -1; }
    /* Kept encoded: a slot that does not decode stops the mod when the game loads that file. */
    char *copy = size ? malloc(size) : NULL;
    if (size && !copy) { snprintf(error, capacity, "out of memory"); return -1; }
    if (size) memcpy(copy, data, size);
    set_slot(mod, slot, copy, size);
    return 0;
}

void mod_store_free(OraclesMod *mod)
{
    for (unsigned slot = 0; slot < MOD_SLOTS; slot++) { free(mod->stored[slot]); mod->stored[slot] = NULL; mod->stored_size[slot] = 0; }
}
