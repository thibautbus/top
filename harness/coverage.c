#include "coverage.h"

#include "guest_struct_offsets.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define KILL_MEMORY 8u        /* the game remembers the enemies killed in the last eight rooms (docs/GAME_HOOKS.md, section 4.3) */
#define TURNAROUND_FRAMES 30u /* a transition back through the same edge within thirty frames counts as one */
#define ITEM_FIRST_SLOT 6u    /* $d6: the item slots of the offset $00 (docs/GAME_HOOKS.md, section 4.1) */
#define ITEM_LAST_SLOT 15u    /* $df */

struct OraclesCoverage {
    OraclesGuest *guest;
    uint32_t frame;
    unsigned transitions[4];            /* by direction: up, right, down, left */
    unsigned riding, carrying, item_active;   /* the state Link was in at the transition */
    unsigned kill_then_reentry;         /* a room entered again after an enemy was killed in it, no warp in between */
    unsigned turnarounds[4];            /* a transition back through the edge just crossed, within thirty frames, by the direction going back */
    /* the rooms where an enemy was killed, since the last warp */
    uint8_t killed_group[KILL_MEMORY], killed_room[KILL_MEMORY];
    unsigned killed_count;
    uint8_t last_direction;
    int have_last_transition;
    int in_scroll;                      /* a transition was decided and its scroll has not ended */
    uint32_t scroll_end_frame;          /* the frame the last scroll ended on */
    int have_scroll_end;
    uint32_t closest_turnaround;        /* the shortest wait seen before going back through the edge just crossed */
};

OraclesCoverage *oracles_coverage_create(OraclesGuest *guest)
{
    OraclesCoverage *c = calloc(1, sizeof *c);
    if (c) { c->guest = guest; c->closest_turnaround = UINT32_MAX; }
    return c;
}

void oracles_coverage_destroy(OraclesCoverage *c) { free(c); }

void oracles_coverage_frame_end(OraclesCoverage *c, uint32_t frame)
{
    if (!c) return;
    c->frame = frame;
    /* The end of a scroll: the game is back in normal play (scroll mode 1,
     * transition state 2), which is where a turnaround's thirty frames start. */
    if (c->in_scroll && c->guest) {
        const OraclesGuestTables *t = oracles_guest_tables(c->guest);
        if ((oracles_guest_read8(c->guest, t->scroll_mode) & 0x7fu) == 0x01u && oracles_guest_read8(c->guest, t->screen_transition_state) == 0x02u) {
            c->in_scroll = 0;
            c->scroll_end_frame = frame;
            c->have_scroll_end = 1;
        }
    }
}

/* Link rides a companion when wLinkObjectIndex names another slot than his own. */
static int link_rides(OraclesCoverage *c)
{
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    const uint8_t index = oracles_guest_read8(c->guest, t->link_object_index);
    return index != 0 && index != (ORACLES_OBJECTS_BASE >> 8);
}

/* An item of Link's is in flight when one of his item slots is enabled. */
static int item_in_flight(OraclesCoverage *c)
{
    for (unsigned slot = ITEM_FIRST_SLOT; slot <= ITEM_LAST_SLOT; slot++) {
        const uint8_t *item = oracles_guest_object(c->guest, slot, 0);
        if (item && item[ORACLES_OBJ_ENABLED]) return 1;
    }
    return 0;
}

static void remember_kill(OraclesCoverage *c, uint8_t group, uint8_t room)
{
    for (unsigned i = 0; i < c->killed_count; i++)
        if (c->killed_group[i] == group && c->killed_room[i] == room) return;
    if (c->killed_count < KILL_MEMORY) {
        c->killed_group[c->killed_count] = group;
        c->killed_room[c->killed_count] = room;
        c->killed_count++;
    }
}

static int room_had_a_kill(const OraclesCoverage *c, uint8_t group, uint8_t room)
{
    for (unsigned i = 0; i < c->killed_count; i++)
        if (c->killed_group[i] == group && c->killed_room[i] == room) return 1;
    return 0;
}

void oracles_coverage_event(OraclesCoverage *c, const OraclesGuestEvent *event)
{
    if (!c || !c->guest) return;
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    switch (event->type) {
    case ORACLES_EVENT_TRANSITION: {
        const uint8_t direction = event->value & 3u;
        c->transitions[direction]++;
        if (link_rides(c)) c->riding++;
        if (oracles_guest_read8(c->guest, t->link_grab_state) != 0) c->carrying++;
        if (item_in_flight(c)) c->item_active++;
        /* Back through the edge just crossed: the opposite direction, decided
         * within thirty frames of the end of the scroll that brought Link in. */
        if (c->have_last_transition && c->have_scroll_end && ((c->last_direction + 2u) & 3u) == direction) {
            const uint32_t waited = event->frame - c->scroll_end_frame;
            if (waited < c->closest_turnaround) c->closest_turnaround = waited;
            if (waited <= TURNAROUND_FRAMES) c->turnarounds[direction]++;
        }
        c->last_direction = direction;
        c->have_last_transition = 1;
        c->in_scroll = 1;
        break;
    }
    case ORACLES_EVENT_ENEMY_KILLED:
        remember_kill(c, oracles_guest_read8(c->guest, t->active_group), oracles_guest_read8(c->guest, t->active_room));
        break;
    case ORACLES_EVENT_WARP:
        /* A warp empties the game's own list of killed enemies (docs/GAME_HOOKS.md, section 4.3). */
        c->killed_count = 0;
        break;
    case ORACLES_EVENT_ROOM_INITIALIZED: {
        const uint8_t group = oracles_guest_read8(c->guest, t->active_group);
        const uint8_t room = oracles_guest_read8(c->guest, t->active_room);
        if (room_had_a_kill(c, group, room)) c->kill_then_reentry++;
        break;
    }
    default:
        break;
    }
}

void oracles_coverage_summary(const OraclesCoverage *c, FILE *out)
{
    if (!c || !out) return;
    static const char *const names[4] = { "up", "right", "down", "left" };
    for (unsigned d = 0; d < 4; d++) fprintf(out, "coverage.transitions_%s=%u\n", names[d], c->transitions[d]);
    fprintf(out, "coverage.transition_riding=%u\ncoverage.transition_carrying=%u\ncoverage.transition_item_active=%u\n",
            c->riding, c->carrying, c->item_active);
    fprintf(out, "coverage.kill_then_reentry=%u\n", c->kill_then_reentry);
    for (unsigned d = 0; d < 4; d++) fprintf(out, "coverage.turnarounds_%s=%u\n", names[d], c->turnarounds[d]);
    /* How close the route came to one, when it has none. */
    if (c->closest_turnaround != UINT32_MAX) fprintf(out, "coverage.closest_turnaround_frames=%u\n", c->closest_turnaround);
}
