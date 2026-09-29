/* Input routes: the inputs of a session, one key mask per change, anchored to
 * the frame counter of the launcher.  Format: docs/ROUTES.md.
 * Recorded by the launcher from a played session, replayed by the launcher
 * and by the harness. */
#ifndef ORACLES_ROUTE_H
#define ORACLES_ROUTE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define ORACLES_ROUTE_FORMAT 2          /* written without mods; format 1 (the `keys` verb alone) is read too */
#define ORACLES_ROUTE_FORMAT_MODS 3     /* written with a `mods` line: a reader of format 2 would ignore the line and replay without the mods */
#define ORACLES_ROUTE_MAX_EVENTS 200000u

typedef struct OraclesRouteHeader {
    char game[16];        /* "ages" or "seasons" */
    char rom_sha1[41];
    char sram_sha1[41];   /* SHA-1 of the SRAM loaded before the run, or "none" */
    char options[64];     /* the gameplay options the session ran with, comma-separated ("continuous-transitions"); empty for none */
    char core[48];        /* how the core ran, comma-separated ("joypad-bouncing-off"); empty in the routes recorded before the core's joypad bouncing was cut */
    char mods[1024];      /* the mods the session ran, "NAME@SHA1,..." in the order of their names; empty for none */
    char store_sha1[41];  /* with mods: SHA-1 of the mods' storage the run starts from (ROUTE.store), or "none"; empty in routes before it */
} OraclesRouteHeader;

#define ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS "continuous-transitions"
#define ORACLES_ROUTE_OPTION_CONTINUOUS_SWIM "continuous-swim"   /* with the transitions: Link swimming at the surface too */
/* Whether the header names a gameplay option. */
int oracles_route_has_option(const OraclesRouteHeader *header, const char *name);
/* A route recorded before the core's joypad bouncing was cut ran with it, and only replays with it; one recorded since
 * says `core joypad-bouncing-off`. */
#define ORACLES_ROUTE_CORE_JOYPAD_BOUNCING_OFF "joypad-bouncing-off"
int oracles_route_joypad_bouncing(const OraclesRouteHeader *header);

typedef struct OraclesRouteEvent {
    uint32_t frame;
    unsigned mask;
} OraclesRouteEvent;

/* Format 2, the verbs that change the game's state (the item hotkeys).  `equip`: an
 * exchange of two inventory slots as the write point applied it, each slot
 * with the item it held before, and a variant written; `use`: the start of a
 * simulated button press of the `use` mode, a note for the harness (the
 * press itself is in the `keys` mask). */
typedef enum OraclesRouteVerb { ORACLES_ROUTE_EQUIP = 1, ORACLES_ROUTE_USE } OraclesRouteVerb;
typedef struct OraclesRouteAction {
    uint32_t frame;
    OraclesRouteVerb verb;
    uint8_t swap, slot_a, item_a, slot_b, item_b;   /* slots: 0 B, 1 A, 2 to 17 the storage (s0 to s15) */
    uint8_t variant, variant_value;                 /* 0 none, 1 satchel, 2 shooter, 3 harp */
    uint8_t button, item;                           /* use: the carrier slot (0 B, 1 A) and the item */
} OraclesRouteAction;

typedef struct OraclesRoute {
    OraclesRouteHeader header;
    unsigned format;
    OraclesRouteEvent *events;
    size_t count;
    size_t capacity;
    OraclesRouteAction *actions;            /* in the file's order: non-decreasing frames */
    size_t action_count, action_capacity;
} OraclesRoute;

/* Reads a route file; returns 0, or -1 with a message. The caller frees with oracles_route_free. */
int oracles_route_read(const char *path, OraclesRoute *route, char *error, size_t error_capacity);
void oracles_route_free(OraclesRoute *route);

/* The actions of `frame`: the index of the first and their number (in the file's order). */
size_t oracles_route_actions_at(const OraclesRoute *route, uint32_t frame, size_t *first);

/* Key mask in force at `frame`; returns 0 when the route has ended (frame is past its last event). */
/* The frame of the route's last line, of whatever verb: the route lasts until then. */
uint32_t oracles_route_last_frame(const OraclesRoute *route);
int oracles_route_mask_at(const OraclesRoute *route, uint32_t frame, unsigned *mask);

/* Recording: writes the header, then one line per mask change. */
typedef struct OraclesRouteWriter {
    FILE *file;
    unsigned last_mask;
    int has_last;
} OraclesRouteWriter;

int oracles_route_writer_open(OraclesRouteWriter *writer, const char *path, const OraclesRouteHeader *header);
void oracles_route_writer_record(OraclesRouteWriter *writer, uint32_t frame, unsigned mask);
/* An `equip` or `use` line; 0, or -1 when the action cannot be written (a slot or a variant out of range). */
int oracles_route_writer_action(OraclesRouteWriter *writer, const OraclesRouteAction *action);
int oracles_route_writer_close(OraclesRouteWriter *writer);

#endif
