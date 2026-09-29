/* The warp data of bank 04 as the houses of the mods make it grow (mod_warps.c).
 * Not a public interface: mod_compose.c uses it. */
#ifndef ORACLES_MOD_WARPS_H
#define ORACLES_MOD_WARPS_H

#include "oracles_rom.h"

#include <stddef.h>
#include <stdint.h>

typedef struct mod_warps mod_warps;

/* Reads the warp tables of `image` (`ages`: Ages' lookup, where $ff ends a list without a default; Seasons' has
 * none) (the composed image, written in place by mod_warps_commit); -1 with the reason
 * when a list has no end, the tables are not in one bank, or bytes of their area are not theirs. */
int mod_warps_open(mod_warps **out, uint8_t *image, size_t size, const OraclesTables *t, int ages, char *error, size_t capacity);
void mod_warps_close(mod_warps *w);

/* Whether a source of any group sends to room `room` of group `group`, and whether group `group`'s list has warps
 * from that room. */
int mod_warps_is_destination(const mod_warps *w, unsigned group, unsigned room);
int mod_warps_has_sources(const mod_warps *w, unsigned group, unsigned room);

/* A destination of `group` with `entry` (room, yx, parameter): an entry no source sends to, or a new one at the
 * table's end; its index in *index.  -1 when the bank is full. */
int mod_warps_destination(mod_warps *w, unsigned group, const uint8_t entry[3], uint8_t *index);
/* `entry` in group `group`'s main list, before its end. */
int mod_warps_append(mod_warps *w, unsigned group, const uint8_t entry[4]);
/* The door of an overworld room: `door` (a position entry: flags, yx, destination, group and transition) first
 * among the room's tile warps (0), or the room's first tile warp when it has none (1: every warp tile of the room
 * leads into the house); -1 when the bank is full. */
int mod_warps_door(mod_warps *w, unsigned room, const uint8_t door[4]);
/* Lays the tables and lists out again in the bank and writes every pointer to them: the edits above reach the image
 * only here.  -1 with the reason when they do not fit. */
int mod_warps_commit(mod_warps *w, char *error, size_t capacity);
/* What the game's two lookups (findWarpSourceAndDest for a tile, findScreenEdgeWarpSource for an edge) find in every
 * room of every group, at every tile and every edge quadrant: a buffer the caller frees, NULL when out of memory. */
uint64_t *mod_warps_lookups(const mod_warps *w);
/* A house as the check sees it: its overworld room, its door's yx, its interior's index, and whether the room had no
 * door before (mod_warps_door's 1). */
typedef struct mod_warps_house { uint8_t room, door, interior; int appended; } mod_warps_house;
/* Whether the lookups `after` the houses equal those `before`, but where the houses change them: the door of each
 * house leads into it (every tile of a room that had no door), and its interior's bottom edge (the quadrants of
 * `leave`, in group `inside`) leads out onto the door.  -1 with the first difference. */
int mod_warps_check(const uint64_t *before, const uint64_t *after, const mod_warps_house *houses, unsigned count, unsigned inside, unsigned leave,
                    char *error, size_t capacity);
/* Bytes of the bank still free after the commit. */
unsigned mod_warps_free_bytes(const mod_warps *w);

#endif
