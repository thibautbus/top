/* A mod written in Lua, run during the game.
 *
 * A mod is a directory of Lua files: main.lua declares what the mod adds, the
 * other files are modules it requires.  Its Lua runs in a state of its own,
 * with a fixed string seed, no clock, no file, no randomness but the one the
 * host seeds from the game, and a budget of instructions a frame
 * (engine/mods/prelude.lua).
 *
 * A mod names one of the game's NPCs by its room; when a text of
 * that room closes, the NPC's conversation starts, and the host runs it once
 * a frame, before the core's frame, with the player's keys.  While it runs the
 * game receives no key: Link stands, and the conversation draws over the
 * game's screen (a text box, a question, or a whole scene: a minigame).  What
 * it changes in the game goes through the guest's call transaction: the game
 * runs its own giveTreasure or removeRupeeValue (guest.h).
 *
 * Everything the mod does depends on the keys and on the game's state only:
 * a route replays it, the keys it held included (oracles_host_run_config's
 * input_hold). */
#ifndef ORACLES_MOD_H
#define ORACLES_MOD_H

#include "guest.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ORACLES_MOD_WIDTH 160u
#define ORACLES_MOD_HEIGHT 144u

typedef struct OraclesMod OraclesMod;

/* One file of a package, as the launcher read it. */
typedef struct OraclesModFile {
    const char *name;                 /* "main.lua", "claw.lua"... */
    const char *text;
    size_t size;
} OraclesModFile;

/* Takes the package (every NAME.lua of the mod's directory, which the launcher reads: this library opens no file),
 * runs main.lua and keeps what it declares for `game`.  `id` is the directory's name.  The ROM gives the font of the
 * text box (the user's, read here, never shipped).  NULL with the reason on a failure. */
OraclesMod *oracles_mod_load(const char *id, const OraclesModFile *files, size_t count, OraclesGame game, const uint8_t *rom, size_t rom_size,
                             char *error, size_t capacity);
void oracles_mod_free(OraclesMod *mod);

/* The houses the mod declares for its game. */
unsigned oracles_mod_house_count(const OraclesMod *mod);
/* The NPCs it declares for its game (the houses' keepers and the game's NPCs it talks as). */
unsigned oracles_mod_npc_count(const OraclesMod *mod);
/* mod.description: one line for the launcher's Mods page, "" when the mod gives none. */
#define ORACLES_MOD_DESCRIPTION_MAX 100
const char *oracles_mod_description(const OraclesMod *mod);

/* "NAME@SHA1": the directory's name and the SHA-1 of its files, names and contents in the order of their names.  A
 * route and a savestate carry it; any change to a file changes it. */
const char *oracles_mod_identity(const OraclesMod *mod);

/* Once a frame, before the core runs it (a set drives it, oracles_mod_set_frame): starts a conversation when one of
 * the mod's NPCs has just finished a text,
 * runs the conversation's frame with the player's keys, and returns the keys the core receives (none while a
 * conversation runs; a key still held when it ends is kept from the game until released). */
unsigned oracles_mod_frame(OraclesMod *mod, uint32_t frame, unsigned keys);

/* The frame the player sees: `pixels` (width x height, the core's 160x144 or the Enhanced surface) with what the
 * conversation drew this frame over its centre; `pixels` itself when it drew nothing.  The buffer returned lives
 * until the next call. */
const uint32_t *oracles_mod_present(OraclesMod *mod, const uint32_t *pixels, uint32_t width, uint32_t height);

/* A conversation runs: its Lua state cannot be saved, so a savestate is refused meanwhile. */
int oracles_mod_busy(const OraclesMod *mod);
/* After a savestate is loaded: no conversation, no key kept, nothing armed. */
void oracles_mod_reset(OraclesMod *mod);

/* The mod's state after its frame, for the harness and a session's record: the image it drew, the conversation's
 * existence, the keys it holds, its random state and the calls it queued. */
uint64_t oracles_mod_fingerprint(const OraclesMod *mod);
/* The error that stopped the mod, or NULL. */
const char *oracles_mod_error(const OraclesMod *mod);
/* key=value lines, mod.NAME.KEY: conversations started, frames held, calls queued and refused, errors. */
void oracles_mod_summary(const OraclesMod *mod, FILE *out);

/* The mods a session runs together (mod_set.c): the houses of all composed into one image, one listener on the
 * guest's texts for all, one conversation at a time.  Every frame the mods are driven in the order of their names:
 * while one converses the others wait; otherwise each sees the player's keys, and the core receives the keys none
 * of them holds. */
#define ORACLES_MOD_SET_MAX 8u

typedef struct OraclesModSet OraclesModSet;

/* Takes the mods (freed with the set, and on a failure), sorted by name.  Refuses two mods of one name, and two that
 * talk as one of the game's NPCs. */
OraclesModSet *oracles_mod_set_create(OraclesMod **mods, size_t count, char *error, size_t capacity);
void oracles_mod_set_free(OraclesModSet *set);
size_t oracles_mod_set_count(const OraclesModSet *set);
OraclesMod *oracles_mod_set_mod(const OraclesModSet *set, size_t index);
unsigned oracles_mod_set_house_count(const OraclesModSet *set);
/* The identities of the mods, in the order of their names, separated by commas: a route's header and a savestate
 * carry it. */
const char *oracles_mod_set_identity(const OraclesModSet *set);
/* The image the game runs: `rom` with the houses of every mod composed in, in a buffer the caller frees; *out NULL
 * when no mod adds a house, the game then running `rom` itself.  -1 with the reason (the mod and its house) when a
 * house cannot be placed. */
int oracles_mod_set_compose(OraclesModSet *set, const uint8_t *rom, size_t rom_size, uint8_t **out, size_t *out_size, char *error, size_t capacity);
/* Listens to the guest's texts and arms its call transaction; -1 with the reason when the game's profile does not
 * qualify it. */
int oracles_mod_set_attach(OraclesModSet *set, OraclesGuest *guest, char *error, size_t capacity);
unsigned oracles_mod_set_frame(OraclesModSet *set, uint32_t frame, unsigned keys);
const uint32_t *oracles_mod_set_present(OraclesModSet *set, const uint32_t *pixels, uint32_t width, uint32_t height);
int oracles_mod_set_busy(const OraclesModSet *set);
void oracles_mod_set_reset(OraclesModSet *set);
uint64_t oracles_mod_set_fingerprint(const OraclesModSet *set);
/* mod.storage (a table per mod and per file of the game, saved when the game saves) as the host keeps it: a text, the
 * same on every platform, of each mod's slots, and with `state` of each mod's live table and the file loaded (a
 * savestate).  Loading replaces what the loaded mods hold; the slots of mods not loaded are kept, and written back
 * (not with `state`).  -1 / NULL with the reason. */
int oracles_mod_set_storage_load(OraclesModSet *set, const uint8_t *data, size_t size, int state, char *error, size_t capacity);
uint8_t *oracles_mod_set_storage_save(OraclesModSet *set, int state, size_t *size, char *error, size_t capacity);
/* Grows when a slot changes (a save, an erase, a load of the storage): the host writes the file again. */
unsigned oracles_mod_set_storage_changes(const OraclesModSet *set);

/* The overworld room of a mod's house (group 0) and the pixel position in front of its door, by the house's name, or
 * MOD/NAME when two mods have one of that name: where --start-at-house puts Link. */
int oracles_mod_set_house_door(const OraclesModSet *set, const char *name, uint8_t *room, uint8_t *y, uint8_t *x, char *error, size_t capacity);

/* key=value lines: the set's identity, each mod's counters (mod.NAME.conversations...), the calls the game ran. */
void oracles_mod_set_summary(const OraclesModSet *set, FILE *out);

#ifdef __cplusplus
}
#endif

#endif
