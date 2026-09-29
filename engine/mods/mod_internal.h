/* State the mod's files share: the package and its driver (mod.c), the Lua
 * state and the host's primitives (mod_lua.c), the drawing (mod_draw.c).
 * Not a public interface: the host sees mod.h. */
#ifndef ORACLES_MOD_INTERNAL_H
#define ORACLES_MOD_INTERNAL_H

#include "mod.h"

#include "lua.h"
#include "lauxlib.h"

#define MOD_FILES 32u
#define MOD_NPCS 8u
#define MOD_SPRITES 64u
#define MOD_FONT_CHARS 256u
#define MOD_TEXT_WIDTH 16u                          /* characters a line of the text box holds, as the game's */
#define MOD_MEMORY_LIMIT (32u * 1024u * 1024u)      /* bytes the Lua state may hold */
#define MOD_MEMORY_MARGIN (1024u * 1024u)           /* more, while the error that stops the mod is reported */
#define MOD_LOAD_BUDGET 50000000                   /* Lua instructions main.lua and its modules may run */
#define MOD_FRAME_BUDGET 2000000                   /* Lua instructions a conversation may run in one frame */
#define MOD_HOOK_STEP 1000                         /* the count hook's period */
#define MOD_BYTES_PER_INSTRUCTION 16               /* bytes allocated, or read by a C function, counted as one instruction */
#define MOD_FAULT_FRAMES 240                       /* frames the notice of a stopped mod stays on the screen */
#define MOD_PAYMENTS 16u                           /* payments queued and not yet run the game, by the mod */
#define MOD_SLOTS 3u                               /* the game's files, each with its own mod.storage */
#define MOD_STORAGE_LIMIT 16384u                   /* bytes of mod.storage once encoded */
#define MOD_STORAGE_DEPTH 16u                      /* tables within tables */
#define MOD_STORAGE_KEYS 4096u                     /* keys of one table */

typedef struct mod_file {
    char name[64];
    char *text;
    size_t size;
} mod_file;

#define MOD_HOUSES 4u

typedef struct mod_npc {
    char name[64];
    uint8_t group, room;
    int house;                                     /* the keeper of this house (-1: one of the game's NPCs, by its texts) */
} mod_npc;

/* A house composed into the image: its facade's top-left metatile in an overworld room (of the present in Ages), and
 * its interior, which the composition places (mod_compose.c). */
typedef struct mod_house {
    char name[64];
    uint8_t room, col, row;
    uint8_t interior_group, interior_room;
} mod_house;

typedef struct mod_sprite {
    char name[64];
    uint32_t width, height;
    uint32_t *pixels;                              /* ARGB, alpha 0 transparent */
} mod_sprite;

struct OraclesMod {
    char id[64];
    char identity[128];
    OraclesGame game;
    char description[ORACLES_MOD_DESCRIPTION_MAX + 1];   /* mod.description, empty when it gives none */
    int described;
    mod_file files[MOD_FILES];
    unsigned file_count;
    uint8_t font[MOD_FONT_CHARS][16];              /* the text box's 8x16 characters, one byte a row, a set bit blank */
    lua_State *L;
    size_t memory;                                 /* bytes the Lua state holds */
    int reporting;                                 /* an error is being reported: the allocation has its margin */
    long budget;                                   /* instructions left in this frame (or the load) */
    int charging;                                  /* the mod's Lua runs: its allocations are counted against the budget */
    int over_budget;                               /* an allocation failed for the budget: the error says so */
    int entry_ref;                                 /* the prelude's entries */
    int surface_ref;                               /* the table `g` the conversation draws with */
    lua_State *conversation;                       /* the running conversation's coroutine, or NULL */
    int conversation_ref;
    mod_npc npcs[MOD_NPCS];
    unsigned npc_count;
    mod_house houses[MOD_HOUSES];
    unsigned house_count;
    mod_sprite sprites[MOD_SPRITES];
    unsigned sprite_count;
    OraclesGuest *guest;
    int armed;                                     /* the NPC one of whose texts is shown, -1 for none */
    unsigned previous_keys;                        /* the keys of the conversation's last frame: a key pressed is one not held then */
    unsigned gate;                                 /* keys held when a conversation ended, kept from the game until released */
    unsigned raw_previous;                         /* the player's keys of the last frame, a conversation or not */
    uint32_t rng;                                  /* xorshift32, seeded from the game when a conversation starts */
    uint32_t frame;
    uint32_t surface[ORACLES_MOD_WIDTH * ORACLES_MOD_HEIGHT];   /* this frame's drawing, alpha 0 where the game shows */
    int drawn;                                     /* something was drawn this frame */
    uint32_t *presented;                           /* the frame with the drawing over it */
    size_t presented_capacity;
    struct { unsigned serial, amount; } payments[MOD_PAYMENTS];   /* game.pay's calls not yet run: rupees already spent */
    unsigned payment_count;
    char error[512];
    int faulted;
    unsigned fault_frames;                         /* frames left of the notice that the mod stopped */
    unsigned conversations, frames_held, calls_queued, calls_refused, lua_errors;
    int storage_ref;                               /* mod.storage, the live table of the file loaded */
    char *stored[MOD_SLOTS];                       /* each file's mod.storage, encoded (mod_store.c); NULL for an empty one */
    size_t stored_size[MOD_SLOTS];
    int slot;                                      /* the file the game loaded, -1 before */
    unsigned storage_changes;                      /* slots written since the start: the host writes them beside the save */
    uint64_t storage_hash;                         /* of the encoded slots, for the fingerprint */
};

static inline OraclesMod *mod_of(lua_State *L) { return *(OraclesMod **)lua_getextraspace(L); }

/* The work of a C function over `bytes` of a string, counted against the budget. */
static inline void mod_charge_bytes(lua_State *L, size_t bytes)
{
    OraclesMod *mod = mod_of(L);
    mod->budget -= (long)(bytes / MOD_BYTES_PER_INSTRUCTION);
    if (mod->budget < 0) luaL_error(L, "the mod asked for more work than a frame allows (%d instructions)", MOD_FRAME_BUDGET);
}

/* The message of an error that ended the mod's Lua: the budget's, when an allocation failed for it. */
static inline const char *mod_error_message(OraclesMod *mod, const char *message)
{
    return mod->over_budget ? "the mod did more work than a frame allows (its allocations counted)" : message;
}

/* The primitives the prelude receives (mod_game.c, mod_text.c) and the methods of the surface `g`. */
extern const luaL_Reg mod_game_primitives[], mod_text_primitives[], mod_surface_methods[];

/* mod_lua.c */
int mod_lua_open(OraclesMod *mod, char *error, size_t capacity);
void mod_lua_close(OraclesMod *mod);
/* Starts the conversation of NPC `npc`; 0, or -1 when the mod faulted. */
int mod_lua_start_conversation(OraclesMod *mod, unsigned npc);
/* Runs the conversation's frame with the keys; 1 while it runs, 0 when it ended (or faulted). */
int mod_lua_conversation_frame(OraclesMod *mod, unsigned keys);
void mod_lua_drop_conversation(OraclesMod *mod);
void mod_fault(OraclesMod *mod, const char *format, ...);

/* mod.c, for the set: the guest's event, given to every mod; the guest the mod reads; a frame in which another mod
 * converses (the mod only sees the keys, so that a key held then is no press after). */
void mod_on_event(OraclesMod *mod, const OraclesGuestEvent *event);
void mod_attach(OraclesMod *mod, OraclesGuest *guest);
void mod_idle_frame(OraclesMod *mod, uint32_t frame, unsigned keys);

/* mod_compose.c: the image with the houses of the mods (a buffer the caller frees), or *out NULL when they have none;
 * -1 with the reason when a house cannot be placed. */
int mod_compose(OraclesMod *const *mods, size_t count, const uint8_t *base, size_t base_size, uint8_t **out, size_t *out_size, char *error, size_t capacity);
/* The tile in front of the houses' counter (the keeper's trigger), for the driver. */
#define MOD_HOUSE_COUNTER_ROW 4u
#define MOD_HOUSE_COUNTER_COL_FIRST 3u
#define MOD_HOUSE_COUNTER_COL_LAST 6u

/* mod_store.c: mod.storage follows the game's file operation `operation` (0 create, 1 save, 2 load, 3 erase) on `slot`;
 * the live table encoded (a buffer the caller frees), or replaced by an encoded one; a slot set from the host's file. */
void mod_store_file_operation(OraclesMod *mod, unsigned operation, unsigned slot);
char *mod_store_encode_live(OraclesMod *mod, size_t *size, char *error, size_t capacity);
int mod_store_decode_live(OraclesMod *mod, const char *data, size_t size, char *error, size_t capacity);
/* Whether an encoded table decodes (a savestate's live table, checked before anything is replaced). */
int mod_store_check(OraclesMod *mod, const char *data, size_t size, char *error, size_t capacity);
int mod_store_set_slot(OraclesMod *mod, unsigned slot, const char *data, size_t size, char *error, size_t capacity);
void mod_store_free(OraclesMod *mod);
/* mod_set.c: the game's file operation `operation` on `slot`, followed by every mod and by the slots of the mods not
 * loaded. */
void mod_set_file_operation(OraclesModSet *set, unsigned operation, unsigned slot);

/* mod_draw.c */
void mod_draw_clear(OraclesMod *mod);
int mod_parse_colour(const char *text, uint32_t *argb);
void mod_draw_rect(OraclesMod *mod, int x, int y, int width, int height, uint32_t argb);
void mod_draw_sprite(OraclesMod *mod, const mod_sprite *sprite, int x, int y, int flip_x);
/* Text in the game's font: the UTF-8 string's characters, each 8x16; -1 with the character when one is not in the font. */
int mod_draw_text(OraclesMod *mod, const char *utf8, int x, int y, uint32_t argb, char *bad, size_t bad_capacity);
/* The font's byte for the UTF-8 character at *text (advanced past it); -1 for one the US font does not have. */
int mod_font_char(const char **text);
void mod_font_load(OraclesMod *mod, const uint8_t *rom, size_t rom_size, uint8_t bank, uint16_t address);

#endif
