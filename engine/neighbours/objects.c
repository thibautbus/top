#include "objects.h"

#include "guest_struct_offsets.h"

#include <stdlib.h>
#include <string.h>

#define SLOTS 16u
#define KINDS 3u   /* interactions, enemies, parts */

struct OraclesObjects {
    OraclesGuest *guest;
    int in_initialisation;              /* between ROOM_ENTER and ROOM_INITIALIZED */
    int in_settling;                    /* from there to the first drawing: an object of the room creates its own there (the nine monkeys of a monkey) */
    uint8_t next_number;
    uint8_t number[KINDS][SLOTS];       /* 0: no number */
    uint16_t created_pc[KINDS][SLOTS];
    uint8_t random_placed[KINDS][SLOTS];
    uint8_t created_key[KINDS][SLOTS][3];   /* id, subid, var03 at the end of the room's initialisation */
    int in_random_placement;            /* between the entry and the return of the random-placement opcode */
    OraclesObjectRecord latched[KINDS][SLOTS];   /* each numbered object as it stood once initialised */
    uint8_t have_latched[KINDS][SLOTS];
};

OraclesObjects *oracles_objects_create(OraclesGuest *guest)
{
    OraclesObjects *o = calloc(1, sizeof *o);
    if (o) o->guest = guest;
    return o;
}

void oracles_objects_destroy(OraclesObjects *o) { free(o); }

void oracles_objects_forget(OraclesObjects *o)
{
    if (!o) return;
    OraclesGuest *guest = o->guest;
    memset(o, 0, sizeof *o);
    o->guest = guest;
}

static int kind_index(OraclesGuestEventType type, unsigned *kind)
{
    switch (type) {
    case ORACLES_EVENT_INTERACTION_CREATED: *kind = 0; return 1;
    case ORACLES_EVENT_ENEMY_CREATED: *kind = 1; return 1;
    case ORACLES_EVENT_PART_CREATED: *kind = 2; return 1;
    default: return 0;
    }
}

void oracles_objects_event(OraclesObjects *o, const OraclesGuestEvent *event)
{
    if (!o) return;
    unsigned kind = 0;
    if (event->type == ORACLES_EVENT_ROOM_ENTER) {
        memset(o->number, 0, sizeof o->number);
        memset(o->created_pc, 0, sizeof o->created_pc);
        o->next_number = 0;
        o->in_initialisation = 1;
        o->in_settling = 0;
        o->in_random_placement = 0;
        memset(o->random_placed, 0, sizeof o->random_placed);
        memset(o->created_key, 0, sizeof o->created_key);
        memset(o->have_latched, 0, sizeof o->have_latched);
    } else if (event->type == ORACLES_EVENT_RANDOM_PLACEMENT) {
        o->in_random_placement = 1;
    } else if (event->type == ORACLES_EVENT_RANDOM_PLACEMENT_DONE) {
        o->in_random_placement = 0;
    } else if (event->type == ORACLES_EVENT_ROOM_INITIALIZED) {
        o->in_initialisation = 0;
        o->in_settling = 1;
        o->in_random_placement = 0;
        /* The objects' own code has not run yet: their fields are the room's list's. */
        for (unsigned k = 0; k < KINDS; k++)
            for (unsigned slot = 0; slot < SLOTS; slot++) {
                const uint8_t *p = oracles_guest_object(o->guest, slot, k + 1u);
                if (!o->number[k][slot] || !p) continue;
                o->created_key[k][slot][0] = p[ORACLES_OBJ_ID];
                o->created_key[k][slot][1] = p[ORACLES_OBJ_SUBID];
                o->created_key[k][slot][2] = p[ORACLES_OBJ_VAR03];
            }
    } else if (event->type == ORACLES_EVENT_OBJECTS_UPDATED && o->in_settling) {
        /* The first update of the objects after the room's initialisation
         * (the game draws once in between, before any object has run): the
         * objects of the room have run their own code once, and what they
         * created there belongs to the room as much as they do (the nine
         * monkeys a monkey spawns).  Their fields are read now, their own
         * initialisation done, as the room's are read at its end. */
        o->in_settling = 0;
        for (unsigned k = 0; k < KINDS; k++)
            for (unsigned slot = 0; slot < SLOTS; slot++) {
                const uint8_t *p = oracles_guest_object(o->guest, slot, k + 1u);
                if (!o->number[k][slot] || !p || o->created_key[k][slot][0]) continue;
                o->created_key[k][slot][0] = p[ORACLES_OBJ_ID];
                o->created_key[k][slot][1] = p[ORACLES_OBJ_SUBID];
                o->created_key[k][slot][2] = p[ORACLES_OBJ_VAR03];
            }
    } else if (kind_index(event->type, &kind) && !o->in_initialisation && !o->in_settling) {
        /* Created later still, in the room's own time, into a slot a numbered
         * object may have left: never numbered, never paired (docs/ARCHITECTURE.md, pairing). */
        const unsigned slot = (unsigned)(event->h - (ORACLES_OBJECTS_BASE >> 8));
        if (slot >= SLOTS) return;
        o->number[kind][slot] = 0;
        o->created_pc[kind][slot] = 0;
        o->random_placed[kind][slot] = 0;
        memset(o->created_key[kind][slot], 0, sizeof o->created_key[kind][slot]);
        o->have_latched[kind][slot] = 0;
    } else if (kind_index(event->type, &kind)) {
        /* The allocators answer with the slot's high byte in H ($d0 to $df):
         * during the room's initialisation, and the settling that follows. */
        const unsigned slot = (unsigned)(event->h - (ORACLES_OBJECTS_BASE >> 8));
        if (slot >= SLOTS || o->next_number == 0xffu) return;
        o->number[kind][slot] = ++o->next_number;
        o->created_pc[kind][slot] = event->pc;
        /* The slot may have held an object that deleted itself (a monkey of
         * the room's list, the intro over): its key is not this one's. */
        memset(o->created_key[kind][slot], 0, sizeof o->created_key[kind][slot]);
        o->have_latched[kind][slot] = 0;
        /* The random-placement opcode allocates through getFreeEnemySlot, which
         * calls the variant the hook sits on: the caller's address names that
         * allocator, not the opcode, so the opcode is known by its own hooks. */
        o->random_placed[kind][slot] = (uint8_t)(o->in_random_placement != 0);
    }
}

/* One object of a slot, as the bus reads it now; 0 when the slot is free. */
static int read_object(const OraclesObjects *o, unsigned kind, unsigned slot, OraclesObjectRecord *r)
{
    const uint8_t *p = oracles_guest_object(o->guest, slot, kind + 1u);
    if (!p || !p[ORACLES_OBJ_ENABLED]) return 0;
    memset(r, 0, sizeof *r);
    r->kind = (uint8_t)(kind + 1u);
    r->slot = (uint8_t)slot;
    r->number = o->number[kind][slot];
    r->created_pc = o->created_pc[kind][slot];
    r->randomly_placed = o->random_placed[kind][slot];
    r->created_id = o->created_key[kind][slot][0];
    r->created_subid = o->created_key[kind][slot][1];
    r->created_var03 = o->created_key[kind][slot][2];
    r->enabled = p[ORACLES_OBJ_ENABLED];
    r->id = p[ORACLES_OBJ_ID];
    r->subid = p[ORACLES_OBJ_SUBID];
    r->var03 = p[ORACLES_OBJ_VAR03];
    r->state = p[ORACLES_OBJ_STATE];
    r->direction = p[ORACLES_OBJ_DIRECTION];
    r->visible = p[ORACLES_OBJ_VISIBLE];
    r->oam_flags = p[ORACLES_OBJ_OAM_FLAGS];
    r->oam_tile_base = p[ORACLES_OBJ_OAM_TILE_INDEX_BASE];
    r->y = p[ORACLES_OBJ_Y];
    r->yh = p[ORACLES_OBJ_YH];
    r->x = p[ORACLES_OBJ_X];
    r->xh = p[ORACLES_OBJ_XH];
    r->zh = p[ORACLES_OBJ_ZH];
    r->oam_data_address = (uint16_t)(p[ORACLES_OBJ_OAM_DATA_ADDRESS] | (p[ORACLES_OBJ_OAM_DATA_ADDRESS + 1] << 8));
    return 1;
}

void oracles_objects_latch(OraclesObjects *o)
{
    if (!o) return;
    for (unsigned kind = 0; kind < KINDS; kind++)
        for (unsigned slot = 0; slot < SLOTS; slot++) {
            OraclesObjectRecord r;
            const int alive = read_object(o, kind, slot, &r);
            /* A freed slot loses its number: the next object to take it was
             * created by another object, after the room's initialisation, and is
             * never paired (docs/ARCHITECTURE.md, pairing).  The eye statues of Seasons spawn
             * their children that way, into the slots their spawner leaves. */
            if (!alive) { o->number[kind][slot] = 0; o->created_pc[kind][slot] = 0; o->random_placed[kind][slot] = 0; continue; }
            if (o->have_latched[kind][slot] || !o->number[kind][slot] || r.state == 0) continue;
            o->latched[kind][slot] = r;
            o->have_latched[kind][slot] = 1;
        }
}

void oracles_objects_latch_remaining(OraclesObjects *o)
{
    if (!o) return;
    for (unsigned kind = 0; kind < KINDS; kind++)
        for (unsigned slot = 0; slot < SLOTS; slot++) {
            if (o->have_latched[kind][slot] || !o->number[kind][slot]) continue;
            OraclesObjectRecord r;
            if (!read_object(o, kind, slot, &r)) continue;
            o->latched[kind][slot] = r;
            o->have_latched[kind][slot] = 1;
        }
}

unsigned oracles_objects_latched(const OraclesObjects *o, OraclesObjectRecord out[], unsigned max)
{
    if (!o || !out) return 0;
    unsigned count = 0;
    for (unsigned kind = 0; kind < KINDS; kind++)
        for (unsigned slot = 0; slot < SLOTS && count < max; slot++)
            if (o->have_latched[kind][slot]) out[count++] = o->latched[kind][slot];
    return count;
}

unsigned oracles_objects_snapshot(const OraclesObjects *o, OraclesObjectRecord out[], unsigned max)
{
    if (!o || !out) return 0;
    unsigned count = 0;
    for (unsigned kind = 0; kind < KINDS; kind++)
        for (unsigned slot = 0; slot < SLOTS && count < max; slot++)
            if (read_object(o, kind, slot, &out[count])) count++;
    return count;
}

int oracles_objects_randomly_placed(const OraclesObjects *o, const OraclesObjectRecord *record)
{
    (void)o;
    return record && record->randomly_placed;
}

void oracles_objects_note_drawn(const OraclesObjects *o, OraclesObjectRecord records[], unsigned count)
{
    if (!o) return;
    for (unsigned i = 0; i < count; i++) {
        OraclesObjectRecord now;
        OraclesObjectRecord *r = &records[i];
        const int alive = r->kind >= 1u && r->kind <= KINDS && read_object(o, r->kind - 1u, r->slot, &now) && now.number == r->number;
        r->drawn_y = alive ? now.y : r->y;
        r->drawn_yh = alive ? now.yh : r->yh;
        r->drawn_x = alive ? now.x : r->x;
        r->drawn_xh = alive ? now.xh : r->xh;
    }
}

uint8_t oracles_objects_killed_in_list(const uint8_t list[16], uint8_t room)
{
    for (unsigned i = 0; i < 8u; i++) if (list[i * 2u] == room) return list[i * 2u + 1u];
    return 0;
}

uint8_t oracles_objects_killed_enemies(OraclesGuest *guest, uint8_t room)
{
    const uint8_t *list = oracles_guest_ptr(guest, oracles_guest_tables(guest)->enemies_killed_list, 16u);
    return list ? oracles_objects_killed_in_list(list, room) : 0;
}

int oracles_objects_add_hooks(OraclesGuest *guest)
{
    const OraclesGuestTables *t = oracles_guest_tables(guest);
    /* The opcodes are reached by a jump table and end by jumping back to the
     * parser (objectLoading.s): none returns, and a return hook on the opcode
     * would take whatever the stack holds (a pushed list pointer) for its
     * return address.  The parser's next entry ends the opcode. */
    if (oracles_guest_add_hook(guest, t->object_data_op6, ORACLES_EVENT_RANDOM_PLACEMENT, 0) != 0) return -1;
    if (oracles_guest_add_hook(guest, t->update_all_objects, 0, ORACLES_EVENT_OBJECTS_UPDATED) != 0) return -1;
    return oracles_guest_add_hook(guest, t->parse_given_object_data, ORACLES_EVENT_RANDOM_PLACEMENT_DONE, 0);
}
