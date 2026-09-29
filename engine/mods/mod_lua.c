/* The mod's Lua state: opened with the libraries a mod may use, bounded in
 * memory and in instructions, and given the host's primitives through the
 * prelude (prelude.lua), which builds everything a mod's files see.
 *
 * The state is created with a fixed string seed (luai_makeseed, CMakeLists)
 * and never sees a clock, a file or an address: two runs with the same keys
 * run the same Lua. */
#include "mod_internal.h"

#include "lauxlib.h"
#include "lualib.h"

#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

extern const unsigned char oracles_mod_prelude[];   /* prelude.lua, embedded at configure (CMakeLists) */

void mod_fault(OraclesMod *mod, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(mod->error, sizeof mod->error, format, args);
    va_end(args);
    mod->faulted = 1;
    mod->fault_frames = MOD_FAULT_FRAMES;
    mod->lua_errors++;
    fprintf(stderr, "oracles: mod %s stopped: %s\n", mod->id, mod->error);
    mod_lua_drop_conversation(mod);
}

/* ---- bounds ----------------------------------------------------------------------- */

static void *bounded_alloc(void *opaque, void *block, size_t old_size, size_t new_size)
{
    OraclesMod *mod = opaque;
    const size_t held = block ? old_size : 0;
    if (new_size == 0) {
        mod->memory -= held;
        free(block);
        return NULL;
    }
    /* While an error is reported, a margin lets the message be written: a mod at its ceiling is stopped, not the game. */
    if (mod->memory - held + new_size > MOD_MEMORY_LIMIT + (mod->reporting ? MOD_MEMORY_MARGIN : 0u)) return NULL;
    /* What the mod's Lua allocates is work (a copy of a long string is one instruction): counted, and refused past the
     * budget, which Lua raises as an error the mod stops on. */
    if (mod->charging && !mod->reporting && new_size > held && mod->budget >= 0) {   /* once exhausted, the error is raised */
        mod->budget -= (long)((new_size - held) / MOD_BYTES_PER_INSTRUCTION);
        if (mod->budget < 0) { mod->over_budget = 1; return NULL; }
    }
    void *grown = realloc(block, new_size);
    if (!grown) return NULL;
    mod->memory = mod->memory - held + new_size;
    return grown;
}

static void count_hook(lua_State *L, lua_Debug *debug)
{
    (void)debug;
    OraclesMod *mod = mod_of(L);
    mod->budget -= MOD_HOOK_STEP;
    if (mod->budget < 0) luaL_error(L, "the mod ran more Lua instructions than a frame allows (%d)", MOD_FRAME_BUDGET);
}

/* charge(n): work a C function of the libraries does (a copy, a move, a search), counted against the frame's budget. */
static int host_charge(lua_State *L)
{
    OraclesMod *mod = mod_of(L);
    const lua_Number n = luaL_checknumber(L, 1);
    if (!(n >= 0) || n > (lua_Number)mod->budget) {
        mod->budget = -1;
        return luaL_error(L, "the mod asked for more work than a frame allows (%d instructions)", MOD_FRAME_BUDGET);
    }
    mod->budget -= (long)n;
    return 0;
}

/* ---- primitives: the package ------------------------------------------------------ */

static int host_print(lua_State *L)
{
    fprintf(stderr, "mod %s: %s\n", mod_of(L)->id, luaL_checkstring(L, 1));
    return 0;
}

/* run_file(name, env): the file's chunk, run with env as its _ENV; its first result. */
static int host_run_file(lua_State *L)
{
    OraclesMod *mod = mod_of(L);
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    for (unsigned i = 0; i < mod->file_count; i++) {
        if (strcmp(mod->files[i].name, name) != 0) continue;
        char chunk[80];
        snprintf(chunk, sizeof chunk, "@%s", name);
        if (luaL_loadbufferx(L, mod->files[i].text, mod->files[i].size, chunk, "t") != LUA_OK) return lua_error(L);
        lua_pushvalue(L, 2);
        lua_setupvalue(L, -2, 1);   /* a main chunk's one upvalue is _ENV */
        lua_call(L, 0, 1);
        return 1;
    }
    return luaL_error(L, "the mod has no file %s", name);
}

/* npc(name, group, room): one of the game's NPCs; npc(name, house): the keeper of one of the mod's houses. */
static int host_npc(lua_State *L)
{
    OraclesMod *mod = mod_of(L);
    const char *name = luaL_checkstring(L, 1);
    if (mod->npc_count >= MOD_NPCS) return luaL_error(L, "mod.npc: a mod has %u NPCs at most", MOD_NPCS);
    mod_npc *npc = &mod->npcs[mod->npc_count];
    snprintf(npc->name, sizeof npc->name, "%s", name);
    npc->house = -1;
    if (lua_type(L, 2) == LUA_TSTRING) {
        const char *house = lua_tostring(L, 2);
        for (unsigned i = 0; i < mod->house_count; i++)
            if (!strcmp(mod->houses[i].name, house)) { npc->house = (int)i; npc->group = mod->houses[i].interior_group; npc->room = mod->houses[i].interior_room; }
        if (npc->house < 0) return luaL_error(L, "mod.npc: %s: no house %s (mod.house declares it first)", name, house);
        mod->npc_count++;
        return 0;
    }
    const lua_Integer group = luaL_checkinteger(L, 2), room = luaL_checkinteger(L, 3);
    if (group < 0 || group > 7 || room < 0 || room > 0xff) return luaL_error(L, "mod.npc: %s: no room %d/%02x", name, (int)group, (int)room);
    npc->group = (uint8_t)group;
    npc->room = (uint8_t)room;
    mod->npc_count++;
    return 0;
}

/* description(text): the line the launcher's Mods page shows under the mod's name. */
static int host_description(lua_State *L)
{
    OraclesMod *mod = mod_of(L);
    size_t length = 0;
    const char *text = luaL_checklstring(L, 1, &length);
    if (mod->described) return luaL_error(L, "mod.description: a mod has one description");
    if (length > ORACLES_MOD_DESCRIPTION_MAX || memchr(text, '\n', length) || memchr(text, 0, length))
        return luaL_error(L, "mod.description: one line of %d bytes at most", ORACLES_MOD_DESCRIPTION_MAX);
    memcpy(mod->description, text, length);
    mod->description[length] = 0;
    mod->described = 1;
    return 0;
}

/* house(name, room, col, row): a house in overworld room 0/room, its facade's top-left metatile at col, row. */
static int host_house(lua_State *L)
{
    OraclesMod *mod = mod_of(L);
    const char *name = luaL_checkstring(L, 1);
    const lua_Integer room = luaL_checkinteger(L, 2), col = luaL_checkinteger(L, 3), row = luaL_checkinteger(L, 4);
    if (mod->house_count >= MOD_HOUSES) return luaL_error(L, "mod.house: a mod has %u houses at most", MOD_HOUSES);
    for (unsigned i = 0; i < mod->house_count; i++)
        if (!strcmp(mod->houses[i].name, name)) return luaL_error(L, "mod.house: %s is declared twice", name);
    if (room < 0 || room > 0xff || col < 0 || col > 7 || row < 0 || row > 4)
        return luaL_error(L, "mod.house: %s: the facade is three by three metatiles, with its door's front inside the room (col 0 to 7, row 0 to 4)", name);
    mod_house *house = &mod->houses[mod->house_count++];
    snprintf(house->name, sizeof house->name, "%s", name);
    house->room = (uint8_t)room;
    house->col = (uint8_t)col;
    house->row = (uint8_t)row;
    /* Where the interior is until the composition places it: in Ages, the room's own index in group 2. */
    house->interior_group = mod->game == ORACLES_GAME_AGES ? 2u : 3u;
    house->interior_room = mod->game == ORACLES_GAME_AGES ? (uint8_t)room : 0u;
    return 0;
}

/* sprite(name, width, height, pixels, palette): pixels one character each, "." and " " transparent. */
static int host_sprite(lua_State *L)
{
    OraclesMod *mod = mod_of(L);
    const char *name = luaL_checkstring(L, 1);
    const lua_Integer width = luaL_checkinteger(L, 2), height = luaL_checkinteger(L, 3);
    size_t length = 0;
    const char *pixels = luaL_checklstring(L, 4, &length);
    luaL_checktype(L, 5, LUA_TTABLE);
    if (mod->sprite_count >= MOD_SPRITES) return luaL_error(L, "mod.sprite: a mod has %u sprites at most", MOD_SPRITES);
    for (unsigned i = 0; i < mod->sprite_count; i++)
        if (!strcmp(mod->sprites[i].name, name)) return luaL_error(L, "mod.sprite: %s is declared twice", name);
    if (width <= 0 || height <= 0 || width > 160 || height > 144 || (size_t)(width * height) != length)
        return luaL_error(L, "mod.sprite: %s: %dx%d pixels do not fit the screen", name, (int)width, (int)height);
    uint32_t *argb = calloc((size_t)(width * height), sizeof *argb);
    if (!argb) return luaL_error(L, "mod.sprite: out of memory");
    for (size_t i = 0; i < length; i++) {
        if (pixels[i] == '.' || pixels[i] == ' ') continue;
        const char key[2] = { pixels[i], 0 };
        lua_getfield(L, 5, key);
        const char *colour = lua_tostring(L, -1);
        if (!colour || mod_parse_colour(colour, &argb[i]) != 0) {
            free(argb);
            return luaL_error(L, "mod.sprite: %s: the palette gives no colour \"#rrggbb\" to '%s'", name, key);
        }
        lua_pop(L, 1);
    }
    mod_sprite *sprite = &mod->sprites[mod->sprite_count++];
    snprintf(sprite->name, sizeof sprite->name, "%s", name);
    sprite->width = (uint32_t)width;
    sprite->height = (uint32_t)height;
    sprite->pixels = argb;
    return 0;
}

/* ---- the state ------------------------------------------------------------------------ */

int mod_lua_open(OraclesMod *mod, char *error, size_t capacity)
{
    mod->L = lua_newstate(bounded_alloc, mod);
    if (!mod->L) { snprintf(error, capacity, "cannot create the Lua state"); return -1; }
    lua_State *L = mod->L;
    *(OraclesMod **)lua_getextraspace(L) = mod;
    static const luaL_Reg libraries[] = {
        { LUA_GNAME, luaopen_base }, { LUA_COLIBNAME, luaopen_coroutine }, { LUA_TABLIBNAME, luaopen_table },
        { LUA_STRLIBNAME, luaopen_string }, { LUA_MATHLIBNAME, luaopen_math }, { LUA_UTF8LIBNAME, luaopen_utf8 },
    };
    for (size_t i = 0; i < sizeof libraries / sizeof libraries[0]; i++) {
        luaL_requiref(L, libraries[i].name, libraries[i].func, 1);
        lua_pop(L, 1);
    }
    mod->budget = MOD_LOAD_BUDGET;
    lua_sethook(L, count_hook, LUA_MASKCOUNT, MOD_HOOK_STEP);

    const char *prelude = (const char *)oracles_mod_prelude;
    if (luaL_loadbufferx(L, prelude, strlen(prelude), "=prelude", "t") != LUA_OK) goto failed;
    lua_newtable(L);   /* the primitives */
    static const luaL_Reg primitives[] = {
        { "print", host_print }, { "run_file", host_run_file }, { "npc", host_npc }, { "house", host_house }, { "sprite", host_sprite },
        { "description", host_description }, { "charge", host_charge }, { NULL, NULL },
    };
    luaL_setfuncs(L, primitives, 0);
    luaL_setfuncs(L, mod_game_primitives, 0);
    luaL_setfuncs(L, mod_text_primitives, 0);
    lua_pushstring(L, mod->game == ORACLES_GAME_AGES ? "ages" : "seasons");
    lua_setfield(L, -2, "game");
    lua_newtable(L);
    luaL_setfuncs(L, mod_surface_methods, 0);
    lua_pushvalue(L, -1);
    mod->surface_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_setfield(L, -2, "surface");
    lua_newtable(L);                               /* mod.storage: filled when the game loads a file (mod_store.c) */
    lua_pushvalue(L, -1);
    mod->storage_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_setfield(L, -2, "storage");
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) goto failed;
    mod->entry_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_rawgeti(L, LUA_REGISTRYINDEX, mod->entry_ref);
    lua_getfield(L, -1, "load_main");
    mod->charging = 1;
    const int loaded = lua_pcall(L, 0, 0, 0);
    mod->charging = 0;
    if (loaded != LUA_OK) goto failed;
    lua_pop(L, 1);
    return 0;
failed:
    snprintf(error, capacity, "%s", mod_error_message(mod, lua_tostring(L, -1) ? lua_tostring(L, -1) : "an error without a message"));
    return -1;
}

void mod_lua_close(OraclesMod *mod)
{
    if (mod->L) lua_close(mod->L);
    mod->L = NULL;
    mod->conversation = NULL;
}

void mod_lua_drop_conversation(OraclesMod *mod)
{
    if (!mod->conversation) return;
    luaL_unref(mod->L, LUA_REGISTRYINDEX, mod->conversation_ref);
    mod->conversation = NULL;
    mod->conversation_ref = LUA_NOREF;
}

/* Protected: the prelude's coroutine for NPC `npc` (argument 1, its name as a light pointer). */
static int start_protected(lua_State *L)
{
    OraclesMod *mod = mod_of(L);
    const char *name = lua_touserdata(L, 1);
    lua_rawgeti(L, LUA_REGISTRYINDEX, mod->entry_ref);
    lua_getfield(L, -1, "conversation");
    lua_pushstring(L, name);
    lua_call(L, 1, 1);
    mod->conversation = lua_tothread(L, -1);
    mod->conversation_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

/* The error that ends a conversation, reported once the conversation's own memory is freed. */
static void report(OraclesMod *mod, const char *message)
{
    char copy[sizeof mod->error];
    snprintf(copy, sizeof copy, "%s", message);
    mod->reporting = 1;
    lua_settop(mod->L, 0);
    if (mod->conversation) lua_settop(mod->conversation, 0);
    mod_lua_drop_conversation(mod);
    lua_gc(mod->L, LUA_GCCOLLECT);
    mod->reporting = 0;
    mod_fault(mod, "%s", copy);
}

/* The message on top of a state's stack, or `fallback`. */
static const char *message_of(lua_State *L, const char *fallback)
{
    return lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : fallback;
}

int mod_lua_start_conversation(OraclesMod *mod, unsigned npc)
{
    lua_State *L = mod->L;
    mod->budget = MOD_FRAME_BUDGET;
    mod->over_budget = 0;
    lua_pushcfunction(L, start_protected);
    lua_pushlightuserdata(L, mod->npcs[npc].name);
    mod->charging = 1;
    const int started = lua_pcall(L, 1, 0, 0);
    mod->charging = 0;
    if (started != LUA_OK) {
        report(mod, mod_error_message(mod, message_of(L, "the conversation could not start")));
        return -1;
    }
    lua_settop(L, 0);
    return 0;
}

static void push_keys(lua_State *L, unsigned mask)
{
    static const struct { unsigned bit; const char *name; } keys[] = {
        { 0x01u, "right" }, { 0x02u, "left" }, { 0x04u, "up" }, { 0x08u, "down" },
        { 0x10u, "a" }, { 0x20u, "b" }, { 0x40u, "select" }, { 0x80u, "start" },
    };
    lua_createtable(L, 0, 8);
    for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        lua_pushboolean(L, (mask & keys[i].bit) != 0);
        lua_setfield(L, -2, keys[i].name);
    }
}

/* Protected: the input of the frame, { held, pressed, released }, from the keys (1) and the last frame's (2). */
static int input_protected(lua_State *L)
{
    const unsigned keys = (unsigned)lua_tointeger(L, 1), previous = (unsigned)lua_tointeger(L, 2);
    lua_createtable(L, 0, 3);
    push_keys(L, keys);
    lua_setfield(L, -2, "held");
    push_keys(L, keys & ~previous);
    lua_setfield(L, -2, "pressed");
    push_keys(L, previous & ~keys);
    lua_setfield(L, -2, "released");
    return 1;
}

/* Protected: the traceback of the failed coroutine (1) under the message (2). */
static int traceback_protected(lua_State *L)
{
    luaL_traceback(L, lua_touserdata(L, 1), lua_touserdata(L, 2), 0);
    return 1;
}

int mod_lua_conversation_frame(OraclesMod *mod, unsigned keys)
{
    if (!mod->conversation) return 0;
    lua_State *L = mod->L, *co = mod->conversation;
    mod->budget = MOD_FRAME_BUDGET;
    mod->over_budget = 0;
    lua_pushcfunction(L, input_protected);
    lua_pushinteger(L, keys);
    lua_pushinteger(L, mod->previous_keys);
    if (lua_pcall(L, 2, 1, 0) != LUA_OK) { report(mod, message_of(L, "out of memory")); return 0; }
    lua_xmove(L, co, 1);
    int results = 0;
    mod->charging = 1;
    const int status = lua_resume(co, L, 1, &results);
    mod->charging = 0;
    if (status == LUA_YIELD) { lua_pop(co, results); return 1; }
    if (status == LUA_OK) { mod_lua_drop_conversation(mod); return 0; }
    /* The traceback when memory allows it; the message alone otherwise. */
    char message[sizeof mod->error];
    snprintf(message, sizeof message, "%s", mod_error_message(mod, message_of(co, "an error without a message")));
    mod->reporting = 1;
    lua_pushcfunction(L, traceback_protected);
    lua_pushlightuserdata(L, co);
    lua_pushlightuserdata(L, message);
    if (lua_pcall(L, 2, 1, 0) == LUA_OK && lua_type(L, -1) == LUA_TSTRING) snprintf(message, sizeof message, "%s", lua_tostring(L, -1));
    report(mod, message);
    lua_settop(L, 0);
    return 0;
}
