/* The text and the surface a conversation draws with (engine/mods/prelude.lua, `talk` and `g`): lines cut for the
 * game's text box, and the drawing primitives over mod_draw.c. */
#include "mod_internal.h"

#include "lauxlib.h"

#include <math.h>
#include <string.h>

/* ---- primitives: text --------------------------------------------------------------- */

/* The characters of a UTF-8 string as the font counts them; an unknown character counts one. */
static size_t text_length(const char *text, size_t *bytes_of_first_n, size_t n)
{
    size_t count = 0;
    const char *at = text;
    while (*at) {
        if (bytes_of_first_n && count == n) break;
        if (mod_font_char(&at) < 0) { at++; while (((unsigned char)*at & 0xc0u) == 0x80u) at++; }
        count++;
    }
    if (bytes_of_first_n) *bytes_of_first_n = (size_t)(at - text);
    return count;
}

static int host_length(lua_State *L)
{
    size_t size = 0;
    const char *text = luaL_checklstring(L, 1, &size);
    mod_charge_bytes(L, size);
    lua_pushinteger(L, (lua_Integer)text_length(text, NULL, 0));
    return 1;
}

static int host_prefix(lua_State *L)
{
    size_t size = 0;
    const char *text = luaL_checklstring(L, 1, &size);
    mod_charge_bytes(L, size);
    const lua_Integer n = luaL_checkinteger(L, 2);
    size_t bytes = 0;
    text_length(text, &bytes, n < 0 ? 0 : (size_t)n);
    lua_pushlstring(L, text, bytes);
    return 1;
}

/* wrap(text): lines of sixteen characters at most, cut at spaces and at "\n"; a character the font lacks, or a word
 * longer than a line, is an error naming it. */
static int host_wrap(lua_State *L)
{
    size_t size = 0;
    const char *text = luaL_checklstring(L, 1, &size);
    mod_charge_bytes(L, size);
    for (const char *at = text; *at;) {
        const char *start = at;
        if (*at == '\n') { at++; continue; }
        if (mod_font_char(&at) < 0) {
            char bad[8];
            size_t length = 1;
            while (length < sizeof bad - 1u && ((unsigned char)start[length] & 0xc0u) == 0x80u) length++;
            memcpy(bad, start, length);
            bad[length] = 0;
            return luaL_error(L, "the game's font has no '%s' (in \"%s\"); the guide lists its characters", bad, text);
        }
    }
    lua_newtable(L);
    lua_Integer lines = 0;
    luaL_Buffer line;
    size_t line_length = 0;
    luaL_buffinit(L, &line);
    const char *at = text;
    while (*at) {
        if (*at == '\n') {
            luaL_pushresult(&line);
            lua_rawseti(L, -2, ++lines);
            luaL_buffinit(L, &line);
            line_length = 0;
            at++;
            continue;
        }
        if (*at == ' ') { at++; continue; }
        const char *word = at;
        while (*at && *at != ' ' && *at != '\n') at++;
        /* A word of punctuation alone (" ?", " !", " :" of French typography) stays with the word before it. */
        for (const char *next = at; *next == ' ';) {
            const char *after = next;
            while (*after == ' ') after++;
            const char *end = after;
            while (*end && strchr("?!:;", *end)) end++;
            if (end == after || (*end && *end != ' ' && *end != '\n')) break;
            at = next = end;
        }
        char copy[256];
        const size_t bytes = (size_t)(at - word) < sizeof copy - 1 ? (size_t)(at - word) : sizeof copy - 1;
        memcpy(copy, word, bytes);
        copy[bytes] = 0;
        const size_t length = text_length(copy, NULL, 0);
        if (length > MOD_TEXT_WIDTH) return luaL_error(L, "the word \"%s\" is longer than a line of the text box (%u characters)", copy, MOD_TEXT_WIDTH);
        if (line_length && line_length + 1u + length > MOD_TEXT_WIDTH) {
            luaL_pushresult(&line);
            lua_rawseti(L, -2, ++lines);
            luaL_buffinit(L, &line);
            line_length = 0;
        }
        if (line_length) { luaL_addchar(&line, ' '); line_length++; }
        luaL_addlstring(&line, word, (size_t)(at - word));
        line_length += length;
    }
    luaL_pushresult(&line);
    if (line_length || lines == 0) lua_rawseti(L, -2, ++lines);
    else lua_pop(L, 1);
    return 1;
}

/* ---- primitives: the surface `g` ---------------------------------------------------- */

/* A position or a size, whole and bounded: what is past the surface draws nothing, however far. */
static int coordinate(lua_State *L, int index)
{
    const lua_Number v = luaL_checknumber(L, index);
    if (!(v >= -1024.0 && v <= 1024.0)) return v > 0 ? 1024 : -1024;   /* NaN and the infinities included */
    return (int)floor(v);
}

static uint32_t colour_arg(lua_State *L, int index, uint32_t fallback)
{
    if (lua_isnoneornil(L, index)) return fallback;
    uint32_t argb;
    if (mod_parse_colour(luaL_checkstring(L, index), &argb) != 0) luaL_error(L, "a colour is written \"#rrggbb\"");
    return argb;
}

static int g_clear(lua_State *L)
{
    mod_draw_rect(mod_of(L), 0, 0, ORACLES_MOD_WIDTH, ORACLES_MOD_HEIGHT, colour_arg(L, 2, 0xff000000u));
    return 0;
}

static int g_rect(lua_State *L)
{
    mod_draw_rect(mod_of(L), coordinate(L, 2), coordinate(L, 3), coordinate(L, 4), coordinate(L, 5), colour_arg(L, 6, 0xffffffffu));
    return 0;
}

static int g_sprite(lua_State *L)
{
    OraclesMod *mod = mod_of(L);
    const char *name = luaL_checkstring(L, 2);
    for (unsigned i = 0; i < mod->sprite_count; i++) {
        if (strcmp(mod->sprites[i].name, name) != 0) continue;
        mod_draw_sprite(mod, &mod->sprites[i], coordinate(L, 3), coordinate(L, 4), lua_toboolean(L, 5));
        return 0;
    }
    return luaL_error(L, "g:sprite: no sprite %s (mod.sprite declares them)", name);
}

static int g_text(lua_State *L)
{
    char bad[8];
    size_t size = 0;
    const char *text = luaL_checklstring(L, 2, &size);
    mod_charge_bytes(L, size);
    if (mod_draw_text(mod_of(L), text, coordinate(L, 3), coordinate(L, 4), colour_arg(L, 5, 0xfff8f8f8u), bad, sizeof bad) != 0)
        return luaL_error(L, "g:text: the game's font has no '%s'", bad);
    return 0;
}

const luaL_Reg mod_text_primitives[] = { { "wrap", host_wrap }, { "length", host_length }, { "prefix", host_prefix }, { NULL, NULL } };
const luaL_Reg mod_surface_methods[] = { { "clear", g_clear }, { "rect", g_rect }, { "sprite", g_sprite }, { "text", g_text }, { NULL, NULL } };
