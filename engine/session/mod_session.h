/* The mods of a run of the host: the launcher's
 * session and the harness's replay load them (one directory each, --mods
 * repeated), give them the keys of every frame, draw them over the frame
 * presented, and write their state frame by frame beside a recorded route
 * (ROUTE.mod.tsv) or where the harness is told, for
 * `oracles-harness --compare`. */
#ifndef ORACLES_MOD_SESSION_H
#define ORACLES_MOD_SESSION_H

#include "mod.h"
#include "oracles_host.h"

#include <stdio.h>

typedef struct OraclesModSession OraclesModSession;

/* Loads the mod of each directory, a set of them (mod.h, OraclesModSet). */
OraclesModSession *oracles_mod_session_start(const char *const *dirs, size_t count, OraclesGame game, const uint8_t *rom, size_t rom_size,
                                             char *error, size_t capacity);
/* Replaces *rom with the image that has the houses of the mods (the old buffer freed), when they add any; before the
 * core is created, so that every reader of the ROM (the core, the ghost, the view) sees that image.  A house needs
 * `original`, the user's ROM being one of the US ROMs the port recognises.  -1 with the reason when a house cannot be
 * placed. */
int oracles_mod_session_compose(OraclesModSession *session, int original, uint8_t **rom, size_t *rom_size, char *error, size_t capacity);
int oracles_mod_session_attach(OraclesModSession *session, OraclesGuest *guest, char *error, size_t capacity);
const char *oracles_mod_session_identity(const OraclesModSession *session);
/* Whether a route recorded with `route_mods` (its header's `mods`, empty for none) replays with this session's mods
 * (NULL: none); 0 with the reason otherwise. */
int oracles_mod_session_matches_route(const OraclesModSession *session, const char *route_mods, char *error, size_t capacity);
/* The run's hold on the keys and the frame presented: set after every other frame source (the native renderer, the
 * Enhanced view), which the mod's drawing goes over. */
void oracles_mod_session_configure(OraclesModSession *session, oracles_host_run_config *config);
/* Writes the mods' state after every frame to `path` (a recorded route's ROUTE.mod.tsv, or the harness's trace). */
int oracles_mod_session_trace(OraclesModSession *session, const char *path);
void oracles_mod_session_frame_end(OraclesModSession *session, uint32_t frame);
int oracles_mod_session_busy(const OraclesModSession *session);
/* The last frame as the player saw it: `pixels` with the mods' last drawing over it (a screenshot). */
const uint32_t *oracles_mod_session_screen(OraclesModSession *session, const uint32_t *pixels, uint32_t width, uint32_t height);
void oracles_mod_session_reset(OraclesModSession *session);
/* mod.storage beside the save: `save_path` without its .sav, then .store.  Open reads `path` when it exists, and with
 * `writable` writes it back whenever a mod's slot changes (sync, once a frame, and at the stop); a replay reads the
 * route's copy and writes nothing.  Copy writes what the mods hold now to `path` (a recorded route's ROUTE.store).  A
 * savestate carries it too: state gives it with the live tables, restore takes it back (`state`) or a file's. */
void oracles_mod_session_storage_path(const char *save_path, char *out, size_t capacity);
int oracles_mod_session_storage_open(OraclesModSession *session, const char *path, int writable, char *error, size_t capacity);
int oracles_mod_session_storage_copy(OraclesModSession *session, const char *path);
void oracles_mod_session_storage_sync(OraclesModSession *session);
uint8_t *oracles_mod_session_storage_state(OraclesModSession *session, size_t *size, char *error, size_t capacity);
int oracles_mod_session_storage_restore(OraclesModSession *session, const uint8_t *data, size_t size, int state, char *error, size_t capacity);
/* --start-at-house NAME: every file of the save (Ages or Seasons) respawns in front of the door of the house NAME (or MOD/NAME),
 * facing it: the SRAM the core holds is changed, never the save's file until the game saves. */
int oracles_mod_session_start_at_house(OraclesModSession *session, const char *name, OraclesCore *core, char *error, size_t capacity);
/* The SHA-1 of a file ("none" when it cannot be read), and whether a route's storage (ROUTE.store beside
 * `route_path`) is the one its header names (`expected`, empty for a route written before the line); 0 with the
 * reason otherwise. */
void oracles_mod_session_file_sha1(const char *path, char out[41]);
int oracles_mod_session_matches_store(const char *route_path, const char *expected, char *error, size_t capacity);
/* The mods' summary (key=value lines) to `summary` when given, and to stderr; frees the session. */
void oracles_mod_session_stop(OraclesModSession *session, FILE *summary);

#endif
