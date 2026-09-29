/* What a mod reads of the game and asks of it (engine/mods/prelude.lua, `game`): the counts it reads through the
 * guest's bus, and the game's own routines it queues through the guest's call transaction (guest.h). */
#include "mod_internal.h"

#include "lauxlib.h"

#include <string.h>

/* ---- primitives: the game --------------------------------------------------------- */

static unsigned bcd_value(unsigned bcd)
{
    unsigned value = 0, scale = 1;
    for (; bcd; bcd >>= 4, scale *= 10) value += (bcd & 0xfu) * scale;
    return value;
}

static int find_name(const char *const *names, unsigned count, const char *name)
{
    for (unsigned i = 0; i < count; i++) if (names[i][0] && !strcmp(names[i], name)) return (int)i;
    return -1;
}

/* The game's tables, known from the mod's load on: names are checked before any game runs. */
static const OraclesGuestTables *tables_of(lua_State *L)
{
    return mod_of(L)->game == ORACLES_GAME_AGES ? &oracles_guest_tables_ages : &oracles_guest_tables_seasons;
}

/* The guest, for what reads or changes the game: during a conversation, not while main.lua declares. */
static OraclesGuest *guest_of(lua_State *L, const char *what)
{
    OraclesMod *mod = mod_of(L);
    if (!mod->guest) luaL_error(L, "%s: the game is not running yet (call it in a conversation, not while main.lua declares)", what);
    return mod->guest;
}

static int treasure_arg(lua_State *L, int index, const char *what)
{
    const char *name = luaL_checkstring(L, index);
    const int treasure = find_name(tables_of(L)->treasure_names, ORACLES_GUEST_TREASURES, name);
    if (treasure <= 0) luaL_error(L, "%s: no treasure named %s (the names of constants/common/treasure.s, lower case, without TREASURE_)", what, name);
    return treasure;
}

static int has_treasure(OraclesMod *mod, uint8_t treasure)
{
    const OraclesGuestTables *t = oracles_guest_tables(mod->guest);
    const uint8_t *flags = oracles_guest_ptr(mod->guest, t->obtained_treasure_flags, t->obtained_treasure_flags_size);
    return flags && treasure / 8u < t->obtained_treasure_flags_size && (flags[treasure / 8u] & (1u << (treasure % 8u)));
}

/* Why the game refuses to give `treasure` with `parameter` (guest_call.c, parameter_allowed), for the message. */
static const char *refusal_of(const OraclesGuestTables *t, const OraclesGuestRom *rom, uint8_t treasure)
{
    switch (oracles_guest_treasure_mode(t, rom, treasure)) {
        case 0x2: return "its count has no bound in the game";
        case 0x5: return "the game sets it to the parameter as given";
        case 0x6: case 0x7: return "it belongs to a dungeon";
        case 0xa: return "it raises the maximum health";
        default: return "the parameter is out of its range (see the guide)";
    }
}

/* Queues one of the game's routines: true when queued, false when the queue is full; an error when the game refuses
 * the call.  *serial numbers it (oracles_guest_queue_call). */
static int queue(lua_State *L, const char *what, OraclesGuestRoutine routine, int a, int c, unsigned *serial)
{
    OraclesMod *mod = mod_of(L);
    OraclesGuest *guest = guest_of(L, what);
    const OraclesGuestCall call = { routine, (uint8_t)a, (uint8_t)c };
    const OraclesGuestTables *t = oracles_guest_tables(guest);
    const OraclesGuestRom rom = oracles_guest_rom(guest);
    if (!oracles_guest_call_allowed(t, &rom, &call)) {
        if (routine == ORACLES_ROUTINE_GIVE_TREASURE) return luaL_error(L, "%s: the game refuses to give %s with %d: %s", what, t->treasure_names[a], c, refusal_of(t, &rom, (uint8_t)a));
        return luaL_error(L, "%s: the game refuses this call", what);
    }
    if (oracles_guest_queue_call(guest, &call, serial) != 0) { mod->calls_refused++; return 0; }
    mod->calls_queued++;
    return 1;
}

/* The rupees Link has, less the payments queued and not yet run. */
static unsigned rupees_now(OraclesMod *mod)
{
    const OraclesGuestTables *t = oracles_guest_tables(mod->guest);
    unsigned held = bcd_value(oracles_guest_read16(mod->guest, t->num_rupees)), pending = 0;
    const unsigned done = oracles_guest_calls_done(mod->guest);
    unsigned kept = 0;
    for (unsigned i = 0; i < mod->payment_count; i++)
        if (mod->payments[i].serial >= done) { pending += mod->payments[i].amount; mod->payments[kept++] = mod->payments[i]; }
    mod->payment_count = kept;
    return held > pending ? held - pending : 0;
}

/* The rupee value of getRupeeValue that stands for n rupees, read in the game's ROM, or -1. */
static int rupee_value(OraclesGuest *guest, lua_Integer n)
{
    const OraclesGuestTables *t = oracles_guest_tables(guest);
    const OraclesGuestRom rom = oracles_guest_rom(guest);
    for (unsigned i = 1; i < t->rupee_value_count; i++) if (oracles_guest_rupee_amount(t, &rom, (uint8_t)i) == n) return (int)i;
    return -1;
}

static int host_rupees(lua_State *L)
{
    guest_of(L, "game.rupees");
    lua_pushinteger(L, (lua_Integer)rupees_now(mod_of(L)));
    return 1;
}

static int host_pay(lua_State *L)
{
    OraclesMod *mod = mod_of(L);
    const lua_Integer n = luaL_checkinteger(L, 1);
    const int value = rupee_value(guest_of(L, "game.pay"), n);
    if (value < 0) return luaL_error(L, "game.pay: the game has no rupee value of %d (rupeeValues.s)", (int)n);
    unsigned serial = 0;
    if (rupees_now(mod) < (unsigned)n || mod->payment_count == MOD_PAYMENTS || !queue(L, "game.pay", ORACLES_ROUTINE_REMOVE_RUPEES, value, 0, &serial)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    mod->payments[mod->payment_count].serial = serial;
    mod->payments[mod->payment_count++].amount = (unsigned)n;
    lua_pushboolean(L, 1);
    return 1;
}

/* The prizes by kind: rupees (an amount of rupeeValues.s), heart (a full refill), seeds (ember seeds, with the
 * satchel), gasha_seed, ring (by name, with the ring box). */
static int host_give(lua_State *L)
{
    OraclesMod *mod = mod_of(L);
    const char *kind = luaL_checkstring(L, 1);
    const OraclesGuestTables *t = tables_of(L);
    OraclesGuest *guest = guest_of(L, "game.give");
    int given = 0;
    if (!strcmp(kind, "rupees")) {
        const int value = rupee_value(guest, luaL_checkinteger(L, 2));
        if (value < 0) return luaL_error(L, "game.give: the game has no rupee value of %d (rupeeValues.s)", (int)lua_tointeger(L, 2));
        given = queue(L, "game.give", ORACLES_ROUTINE_GIVE_TREASURE, t->treasure_rupees, value, NULL);
    } else if (!strcmp(kind, "heart")) {
        given = queue(L, "game.give", ORACLES_ROUTINE_GIVE_TREASURE, t->treasure_heart_refill, 0x40, NULL);   /* sixteen hearts, capped by the maximum: a full refill */
    } else if (!strcmp(kind, "seeds")) {
        const lua_Integer n = luaL_checkinteger(L, 2);
        if (n < 1 || n > 99) return luaL_error(L, "game.give: 1 to 99 seeds");
        given = has_treasure(mod, t->treasure_seed_satchel) && queue(L, "game.give", ORACLES_ROUTINE_GIVE_TREASURE, t->treasure_ember_seeds, (int)((n / 10) << 4 | n % 10), NULL);
    } else if (!strcmp(kind, "gasha_seed")) {
        given = queue(L, "game.give", ORACLES_ROUTINE_GIVE_TREASURE, t->treasure_gasha_seed, 0x01, NULL);
    } else if (!strcmp(kind, "ring")) {
        const char *name = luaL_checkstring(L, 2);
        const int ring = find_name(t->ring_names, ORACLES_GUEST_RINGS, name);
        if (ring < 0) return luaL_error(L, "game.give: no ring named %s (the names of constants/common/rings.s, lower case)", name);
        given = has_treasure(mod, t->treasure_ring_box) && queue(L, "game.give", ORACLES_ROUTINE_GIVE_TREASURE, t->treasure_ring, ring, NULL);
    } else {
        return luaL_error(L, "game.give: no prize %s; rupees, heart, seeds, gasha_seed or ring (game.give_treasure gives any treasure)", kind);
    }
    lua_pushboolean(L, given);
    return 1;
}

/* The game's routines by the names of the disassembly. */
static int host_give_treasure(lua_State *L)
{
    const int treasure = treasure_arg(L, 1, "game.give_treasure");
    const lua_Integer parameter = luaL_optinteger(L, 2, 0);
    if (parameter < 0 || parameter > 0xff) return luaL_error(L, "game.give_treasure: a parameter is 0 to 255");
    lua_pushboolean(L, queue(L, "game.give_treasure", ORACLES_ROUTINE_GIVE_TREASURE, treasure, (int)parameter, NULL));
    return 1;
}

static int host_lose_treasure(lua_State *L)
{
    lua_pushboolean(L, queue(L, "game.lose_treasure", ORACLES_ROUTINE_LOSE_TREASURE, treasure_arg(L, 1, "game.lose_treasure"), 0, NULL));
    return 1;
}

static int host_has(lua_State *L)
{
    const int treasure = treasure_arg(L, 1, "game.has");
    guest_of(L, "game.has");
    lua_pushboolean(L, has_treasure(mod_of(L), (uint8_t)treasure));
    return 1;
}

static int sound_arg(lua_State *L, const char *what)
{
    const char *name = luaL_checkstring(L, 1);
    const int sound = find_name(tables_of(L)->sound_names, ORACLES_GUEST_SOUNDS, name);
    if (sound <= 0) luaL_error(L, "%s: no sound named %s (constants/common/music.s, lower case: snd_... or mus_...)", what, name);
    return sound;
}

static int host_play_sound(lua_State *L)
{
    const int sound = sound_arg(L, "game.play_sound");
    lua_pushboolean(L, queue(L, "game.play_sound", ORACLES_ROUTINE_PLAY_SOUND, sound, 0, NULL));
    return 1;
}

static int flag_arg(lua_State *L, const char *what)
{
    const char *name = luaL_checkstring(L, 1);
    const int flag = find_name(tables_of(L)->global_flag_names, ORACLES_GUEST_GLOBAL_FLAGS, name);
    if (flag < 0) luaL_error(L, "%s: no global flag named %s (constants/common/globalFlags.s, lower case, without GLOBALFLAG_)", what, name);
    return flag;
}

static int host_flag(lua_State *L)
{
    const int flag = flag_arg(L, "game.flag");
    OraclesGuest *guest = guest_of(L, "game.flag");
    const OraclesGuestTables *t = oracles_guest_tables(guest);
    const uint8_t *flags = oracles_guest_ptr(guest, t->global_flags, t->global_flags_size);
    lua_pushboolean(L, flags && (flags[flag / 8] & (1u << (flag % 8))));
    return 1;
}

static int host_set_flag(lua_State *L)
{
    lua_pushboolean(L, queue(L, "game.set_flag", ORACLES_ROUTINE_SET_GLOBAL_FLAG, flag_arg(L, "game.set_flag"), 0, NULL));
    return 1;
}

static int host_unset_flag(lua_State *L)
{
    lua_pushboolean(L, queue(L, "game.unset_flag", ORACLES_ROUTINE_UNSET_GLOBAL_FLAG, flag_arg(L, "game.unset_flag"), 0, NULL));
    return 1;
}

/* random(n): 1 to n, from the conversation's generator, seeded from the game when it started. */
static int host_random(lua_State *L)
{
    OraclesMod *mod = mod_of(L);
    const lua_Integer n = luaL_checkinteger(L, 1);
    if (n < 1) return luaL_error(L, "random: n is 1 or more");
    uint32_t x = mod->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    mod->rng = x;
    lua_pushinteger(L, (lua_Integer)(x % (uint32_t)n) + 1);
    return 1;
}

/* The names a mod checks while main.lua declares, before any game runs: the name itself, or an error that names the
 * file and the line of the mod. */
static int host_sound(lua_State *L) { sound_arg(L, "game.sound"); lua_settop(L, 1); return 1; }
static int host_treasure(lua_State *L) { treasure_arg(L, 1, "game.treasure"); lua_settop(L, 1); return 1; }
static int host_global_flag(lua_State *L) { flag_arg(L, "game.global_flag"); lua_settop(L, 1); return 1; }

const luaL_Reg mod_game_primitives[] = {
    { "rupees", host_rupees }, { "pay", host_pay }, { "give", host_give }, { "has", host_has }, { "random", host_random },
    { "give_treasure", host_give_treasure }, { "lose_treasure", host_lose_treasure }, { "play_sound", host_play_sound },
    { "flag", host_flag }, { "set_flag", host_set_flag }, { "unset_flag", host_unset_flag },
    { "sound", host_sound }, { "treasure", host_treasure }, { "global_flag", host_global_flag }, { NULL, NULL },
};
