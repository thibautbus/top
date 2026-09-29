/* Composite savestate: the guest state of the core, the native
 * state of the host, and the versions that make them meaningful.  A state
 * whose versions do not match the running binary is refused with a message,
 * never silently reinterpreted.  The core serialises to and from memory; the
 * launcher owns the files, as it does for the SRAM.
 *
 * Layout, all integers little-endian:
 *   "ORACLESST" (9 bytes)  format version u32
 *   core version: u32 length, bytes           oracles_core_version()
 *   game: u32 length, bytes                    "ages" / "seasons"
 *   rom_sha1: u32 length, bytes
 *   mods: u32 length, bytes                    comma-separated ids of the mods and of the gameplay options that change the guest state (continuous-transitions), empty for none
 *   guest state: u32 length, bytes             SameBoy's own format
 *   host state: u32 length, bytes              empty in the Faithful profile
 *   mods' storage: u32 length, bytes           the mods' tables saved with the game's files and the live ones
 *                                              (engine/mods, oracles_mod_set_storage_save); absent from the
 *                                              states written before it, which read as empty
 */
#ifndef ORACLES_STATE_H
#define ORACLES_STATE_H

#include "core.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ORACLES_STATE_FORMAT 1u

typedef struct OraclesStateInfo {
    const char *game;       /* "ages" or "seasons" */
    const char *rom_sha1;
    const char *mods;       /* mods and gameplay options, comma-separated; "" when none */
    const uint8_t *host_state;
    size_t host_state_size;
    const uint8_t *mods_storage;   /* written only: read back by oracles_state_mods_storage */
    size_t mods_storage_size;
} OraclesStateInfo;

/* Serialises a composite savestate into a buffer the caller frees. Returns 0, or -1 with a message. */
int oracles_state_serialize(OraclesCore *core, const OraclesStateInfo *info,
                            uint8_t **out, size_t *out_size, char *error, size_t error_capacity);

/* Checks the format, the core version, the game, the ROM and the mods against
 * `info`, then restores the guest state.  The host state is returned in a
 * buffer the caller frees (NULL when empty). Returns 0, or -1 with a message. */
int oracles_state_deserialize(OraclesCore *core, const OraclesStateInfo *info,
                              const uint8_t *data, size_t size,
                              uint8_t **host_state, size_t *host_state_size,
                              char *error, size_t error_capacity);

/* The host state of a composite savestate, read in place without loading
 * anything, for the host to judge before the core is touched: 0, `host_state`
 * pointing into `data` (NULL and 0 when empty), or -1 for a state that is not
 * one of this format. */
int oracles_state_host_state(const uint8_t *data, size_t size, const uint8_t **host_state, size_t *host_state_size);

/* The set of mods and gameplay options a savestate was made with, read the
 * same way into `mods`: 0, or -1 for a state that is not one of this format
 * or a set longer than `capacity`. */
int oracles_state_mods(const uint8_t *data, size_t size, char *mods, size_t capacity);

/* The mods' storage of a composite savestate, read in place: 0 with `storage` pointing into `data` (NULL and 0 when
 * empty or absent), or -1 for a state that is not one of this format. */
int oracles_state_mods_storage(const uint8_t *data, size_t size, const uint8_t **storage, size_t *storage_size);

#ifdef __cplusplus
}
#endif

#endif
