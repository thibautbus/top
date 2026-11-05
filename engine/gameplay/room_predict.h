/* The collisions of the room a scroll enters, predicted while the game loads them.
 *
 * The game decides a scrolling transition and loads the room entered in the same frame (cutscene01, bank1.s):
 * getNextActiveRoom names it, loadTilesetData its tileset, wScrollMode is set to 8, then loadTilesetAndRoomLayout
 * decodes its layout and applies its tile substitutions, loadRoomCollisions maps each tile through the tileset's
 * collisions, and generateVramTilesWithRoomChanges draws it.  That frame outlasts two or three vblanks, through which
 * the room's collisions are not known yet, and the continuous transitions' diagonal (room_transition.h) would withhold
 * Link's carry across the axis.  Everything that load reads is fixed once wScrollMode is 8: the layout and the
 * collisions are the ROM's tables (the room's layout, its tileset's collisions) through substitutions driven by bytes
 * of the RAM the load leaves alone (the room's flags, the global flags, the portal).  The prediction rebuilds them
 * from those tables and bytes, as the game does, for the rooms whose load is only that, and replays the code of the
 * small rooms that change their own tiles (applyRoomSpecificTileChanges: a few writes under a flag, which may be
 * another room's, another era's, or a variable of the game's progress); a room whose code changes them after its
 * graphics (applyRoomSpecificTileChangesAfterGfxLoad, but where it only draws), a large room, an image not qualified,
 * is left to the game's own load.  The prediction is compared with what the game
 * loads at every transition it is made for (oracles_room_transition_stats). */
#ifndef ORACLES_GAMEPLAY_ROOM_PREDICT_H
#define ORACLES_GAMEPLAY_ROOM_PREDICT_H

#include "guest.h"
#include "oracles_rom.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Why a room's collisions are not predicted, ORACLES_ROOM_PREDICTED when they are. */
typedef enum OraclesRoomPredictVerdict {
    ORACLES_ROOM_PREDICTED = 0,
    ORACLES_ROOM_PREDICT_UNQUALIFIED,   /* the image is not qualified (its tables not bound), or not Ages or Seasons */
    ORACLES_ROOM_PREDICT_EARLY,         /* wScrollMode not 8 yet: the room entered not named yet */
    ORACLES_ROOM_PREDICT_LARGE,         /* a large room, or a group of large rooms (4 to 7) */
    ORACLES_ROOM_PREDICT_ROOM_CODE,     /* the room's own code changes its tiles (applyRoomSpecificTileChanges), not replayed, or reads a byte not given */
    ORACLES_ROOM_PREDICT_AFTER_GFX,     /* or changes them after its graphics (applyRoomSpecificTileChangesAfterGfxLoad), a Gasha spot planted */
    ORACLES_ROOM_PREDICT_OTHER_LAYOUT,  /* Seasons' Temple Remains filled with lava: the layout is another room's */
    ORACLES_ROOM_PREDICT_UNREADABLE,    /* a table could not be read, or points out of the room */
    ORACLES_ROOM_PREDICT_ROUTED,        /* the room beside: off the map, a scroll of its own, another room pack, a tileset of its own, not the overworld */
    ORACLES_ROOM_PREDICT_SCENE,         /* the room entered: an object that may leave the player without the controls once the scroll beside is decided */
    ORACLES_ROOM_PREDICT_VERDICTS
} OraclesRoomPredictVerdict;

/* The room entered's layout (wRoomLayout) and collisions (wRoomCollisions) as the game loads them, ORACLES_ROOM_LAYOUT_BYTES
 * each, from `state` at a frame of the load (wScrollMode 8, the room entered named and its tileset data loaded) and
 * the ROM's tables `data`; the verdict.  Pure: the ROM and the state alone. */
OraclesRoomPredictVerdict oracles_room_predict(const OraclesRom *rom, const OraclesTables *data, const OraclesGuestTransitionState *state,
                                               uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES], uint8_t collisions[ORACLES_ROOM_LAYOUT_BYTES]);

/* The room beside the room entered, across the scroll's axis on the side `across` (0 up, 1 right, 2 down, 3 left): the
 * room the scroll that leaves the room entered that way enters (`*room`), its layout and collisions as that scroll
 * loads them, from `state` at a frame where the game has named the room entered (wScrollMode 8) and the ROM's tables;
 * the verdict.  loadTilesetData's choice is made again for it, the room entered's left alone: its tileset from
 * roomTilesetsGroupTable (Ages: the group of its layout swapped by its flags; Seasons: the season, which a scroll keeps
 * within a room pack; with the expanded tilesets, its tileset by its own byte and its layout group by its group, in
 * Seasons' group 0 by the season), then the steps of oracles_room_predict on its own flags.  Only on an overworld (Ages' two,
 * Seasons' Holodrum and Subrosia, within the group's map: `state`'s map_left, map_top, map_width and map_height), between two rooms
 * outdoors of one room pack (Subrosia's compare none: checkRoomPack acts in Seasons' group 0 only), or on Ages' sea
 * floor (groups 2 and 3, between two rooms of underwater tilesets: no map, no pack), neither with a transition of its own
 * (mapTransitionGroupTable: `special` says, 1 when unknown) nor a tileset chosen by the game's own code (Ages' Maku
 * Tree, Nuun Highlands, with the expanded tilesets every pack from $7f; Seasons' Moblin's Keep, in Holodrum, by the image's moblinKeepRooms), within the map: ORACLES_ROOM_PREDICT_ROUTED otherwise.  The frame after the first scroll, the room entered's objects run
 * their first and the scroll beside is already decided, which none of them can refuse then: refused,
 * ORACLES_ROOM_PREDICT_SCENE, where an object of the room entered may leave the player without the controls
 * (oracles_room_predict_scene on `state`'s live objects; ORACLES_ROOM_PREDICT_EARLY while they are not given).  The
 * room beside's objects run theirs once a scroll has entered it, as at any scroll.  `active_collisions`, unless NULL,
 * takes its tileset's list of tile types (wActiveCollisions as its load sets it).  Pure. */
OraclesRoomPredictVerdict oracles_room_predict_beside(const OraclesRom *rom, const OraclesTables *data, const OraclesGuestTransitionState *state,
                                                      unsigned across, int (*special)(const OraclesGuestTransitionState *state, uint8_t room, void *opaque),
                                                      void *opaque, uint8_t *room, uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES],
                                                      uint8_t collisions[ORACLES_ROOM_LAYOUT_BYTES], uint8_t *active_collisions);

/* The objects whose first frames may leave the player without the controls once the scroll after them is decided
 * (room_scenes.c, generated from the disassembly's sources by gen_room_scenes.py): by kind (interactions, enemies,
 * parts) a bit an id, for every subid; and the interactions marked for some subids alone, ended by an id 0. */
typedef struct OraclesRoomSceneSubids {
    uint8_t id;
    uint64_t subids;   /* a bit a subid, 0 to 63 */
} OraclesRoomSceneSubids;
typedef struct OraclesRoomScenes {
    const uint8_t (*ids)[32];
    const OraclesRoomSceneSubids *subids;
} OraclesRoomScenes;
extern const uint8_t oracles_room_scenes_ages[3][32], oracles_room_scenes_seasons[3][32];
extern const OraclesRoomSceneSubids oracles_room_scene_subids_ages[], oracles_room_scene_subids_seasons[];

/* Whether the room's live objects (`objects`, WRAM bank 1 from $d000: interactions, enemies and parts of the room
 * entered, created by its load) hold one marked in `scenes`; -1 when they are not known (NULL). */
int oracles_room_predict_scene(const OraclesRoomScenes *scenes, const uint8_t *objects);

/* The steps, testable alone. */
/* The substitutions of applyAllTileSubstitutions on `layout`, the room's as loadRoomLayout decoded it, a small room of
 * groups 0 to 3, its own code's included, which may read `table`, its tileset's collisions (w3TileCollisions; NULL
 * refuses the rooms that read it); the verdict (ORACLES_ROOM_PREDICT_UNREADABLE when a table cannot be read). */
OraclesRoomPredictVerdict oracles_room_predict_substitute(const OraclesRom *rom, const OraclesTables *data, const OraclesGuestTransitionState *state,
                                                          const uint8_t *table, uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES]);
/* Whether the room's own code leaves its tiles to the prediction, or is replayed by it: ORACLES_ROOM_PREDICTED, or why not. */
OraclesRoomPredictVerdict oracles_room_predict_excluded(const OraclesRom *rom, const OraclesTables *data, const OraclesGuestTransitionState *state);
/* loadRoomCollisions: each tile's collision in `table` (w3TileCollisions), the bytes past a small room's last row and
 * column, and past a row's sixteenth, $ff. */
void oracles_room_predict_collisions(const uint8_t table[ORACLES_TILESET_COLLISIONS_BYTES], const uint8_t layout[ORACLES_ROOM_LAYOUT_BYTES],
                                     int small, uint8_t collisions[ORACLES_ROOM_LAYOUT_BYTES]);
/* The verdict's name, for the reports. */
const char *oracles_room_predict_verdict_name(OraclesRoomPredictVerdict verdict);

#ifdef __cplusplus
}
#endif

#endif
