/* The transitions at the line (guest.h): a room's edge tests answered in a register.
 *
 * screenTransitionState2 (bank1.s) tests Link's yh against 5 and wScreenTransitionBoundaryY, then his xh against 5 and
 * wScreenTransitionBoundaryX, each time loading the bound in a and comparing it with (hl) (`ld a,$05 / cp (hl) / jr nc`
 * at the top and the left, `ld a,(bound) / cp (hl) / jr c` at the bottom and the right).  Past a bound it puts him back
 * on it (a + 1 at the top and the left, a at the bottom and the right), then asks @transition whether to start.  With
 * the line on, at the `cp`, before it, a becomes the bound of the room's own last pixel: $ff at the top and the left
 * for a coordinate at 0 or below it (from 192 up and down, 208 across), the comparison put back at 0, 0 otherwise; the
 * room's last row or column at the bottom and the right (swimming, at the bottom, the last row his own tile is in);
 * in a small room alone, on foot, on Ages' sea floor, and swimming at the surface with the transitions' swim, never on a
 * current.  The game's own path follows: back on the line, then @transition and every check of its own.  Nothing else
 * reads that register, and nothing is written in memory. */
#include "guest_internal.h"

#define SMALL_WIDTH 20u    /* wRoomWidth, wRoomHeight of a small room, in 8-pixel tiles: 160 by 128 pixels */
#define SMALL_HEIGHT 16u

/* A coordinate wrapped below zero past the top or the left edge, rather than past the other one: the upper half of
 * what lies beyond the room (from 208 across, 192 up and down). */
static unsigned below_zero(unsigned direction) { return direction == 3u ? 160u + (256u - 160u) / 2u : 128u + (256u - 128u) / 2u; }

uint8_t oracles_guest_edge_test_a(const OraclesGuestEdgeTest *t)
{
    if (!t || t->direction > 3u || !t->link || (t->tileset_flags & ORACLES_TILESETFLAG_SIDESCROLL)) return t ? t->a : 0;
    /* Swimming at the surface, with the transitions' swim alone, and never on a current, whose transition (wcc92's low
     * bits, taken without the direction) and push through the scroll stay the game's. */
    if (t->swimming && (!t->swim || t->conveyor)) return t->a;
    /* A small room alone: a large one (240 by 176, the dungeons) leaves no room past its edges to tell a coordinate
     * wrapped below zero from one past the other edge, and its transitions are the game's. */
    if (t->room_is_large || t->room_width != SMALL_WIDTH || t->room_height != SMALL_HEIGHT) return t->a;
    /* Ages' overworld (@transition): no transition past its right, bottom and left edges; Link stays where the game stops him. */
    if ((t->tileset_flags & ORACLES_TILESETFLAG_OUTDOORS) && t->map_last_column) {
        if (t->direction == 1u && (t->room & 0x0fu) == t->map_last_column) return t->a;
        if (t->direction == 2u && t->room >= t->map_last_row) return t->a;
        if (t->direction == 3u && (t->room & 0x0fu) == 0u) return t->a;
    }
    const int up_left = t->direction == 0u || t->direction == 3u;
    /* Swimming, the bottom line is where the tile the game takes as his own (y+5, linkApplyTileTypes) is still the room's
     * last row: past it the game would read the row beyond the room's layout, no water, and take him out of it. */
    const unsigned bottom = t->swimming ? 1u : 6u;
    /* With the transitions disabled (a script: Maple scattering her things, the wind seed, Ricky's jump), the game's
     * own bounds, Link held 6 px short of the edge as the game holds him; a coordinate past the room alone (the room
     * left, walked back into during a scroll) goes back on the line, never taken for one past the other edge. */
    if (t->transitions_disabled) {
        if (up_left) return t->position >= below_zero(t->direction) ? 0xffu : t->a;
        return t->position < below_zero(t->direction == 2u ? 0u : 3u) && t->position > (uint8_t)(t->a + (t->direction == 2u ? bottom : 5u))
                   ? (uint8_t)(t->a + (t->direction == 2u ? bottom : 5u)) : t->a;
    }
    if (up_left) return (t->position == 0u || t->position >= below_zero(t->direction)) ? 0xffu : 0u;
    return (uint8_t)(t->a + (t->direction == 2u ? bottom : 5u));
}

/* `ld l,<w1Link.yh or xh / ld a,$05 / cp (hl) / jr nc`, or `ld a,(bound) / cp (hl) / jr c`: the `cp`. */
uint16_t oracles_guest_find_edge_test(const OraclesGuestTables *t, const uint8_t *rom, size_t size, unsigned direction)
{
    if (!t || direction > 3u || t->screen_transition_state2.bank == ORACLES_GUEST_ABSENT) return 0;
    if (direction == 0u || direction == 3u) {
        const uint8_t test[] = { 0x2e, direction == 0u ? (uint8_t)(ORACLES_OBJ_Y + 1u) : (uint8_t)(ORACLES_OBJ_X + 1u), 0x3e, 0x05, 0xbe, 0x30 };
        const uint16_t at = oracles_guest_find_in_routine(rom, size, t->screen_transition_state2, test, sizeof test);
        return at ? (uint16_t)(at + 4u) : 0;
    }
    const OraclesGuestSym bound = direction == 2u ? t->screen_transition_boundary_y : t->screen_transition_boundary_x;
    if (bound.bank == ORACLES_GUEST_ABSENT) return 0;
    const uint8_t test[] = { 0xfa, (uint8_t)bound.addr, (uint8_t)(bound.addr >> 8), 0xbe, 0x38 };
    const uint16_t at = oracles_guest_find_in_routine(rom, size, t->screen_transition_state2, test, sizeof test);
    return at ? (uint16_t)(at + 3u) : 0;
}

/* Ages' @transition: `ld e,a / and $0f / cp OVERWORLD_WIDTH-1 / jr nz / ld a,c / cp DIR_RIGHT / ret z / ld a,e /
 * cp (OVERWORLD_HEIGHT-1)*16 / jr c`, the map's last column and its last row's first room; 0 when the game has none. */
void oracles_guest_find_map_edges(const OraclesGuestTables *t, const uint8_t *rom, size_t size, uint8_t *column, uint8_t *row)
{
    *column = *row = 0;
    if (!t || t->screen_transition_state2.bank == ORACLES_GUEST_ABSENT) return;
    const uint8_t right[] = { 0x20, 0x04, 0x79, 0xfe, 0x01, 0xc8, 0x7b, 0xfe };
    const uint16_t at = oracles_guest_find_in_routine_within(rom, size, t->screen_transition_state2, right, sizeof right, 0xc0u);
    if (!at || at < 0x4002u) return;
    const size_t base = (size_t)t->screen_transition_state2.bank * 0x4000u + (at - 0x4000u);
    if (base + sizeof right + 1u > size || rom[base - 2u] != 0xfeu) return;
    *column = rom[base - 1u];
    *row = rom[base + sizeof right];
}

static int arm_tests(OraclesGuest *guest)
{
    const OraclesGuestTables *t = guest->tables;
    if (!oracles_compat_continuous_line(guest->profile)) return -1;
    size_t size = 0;
    uint16_t bank = 0;
    const uint8_t *rom = oracles_core_memory(guest->core, ORACLES_CORE_ROM, &size, &bank);
    uint16_t at[4];
    for (unsigned d = 0; d < 4u; d++) if (!(at[d] = oracles_guest_find_edge_test(t, rom, size, d))) return -1;
    for (unsigned d = 0; d < 4u; d++)
        if (oracles_guest_add_hook(guest, (OraclesGuestSym){ t->screen_transition_state2.bank, at[d] }, (OraclesGuestEventType)(ORACLES_EVENT_EDGE_TEST_UP + d), 0) != 0)
            return -1;
    oracles_guest_find_map_edges(t, rom, size, &guest->map_last_column, &guest->map_last_row);
    return 0;
}

int oracles_guest_set_transition_line(OraclesGuest *guest, int on)
{
    if (!guest) return -1;
    if (on && !guest->line_armed) {
        if (arm_tests(guest) != 0) return -1;
        guest->line_armed = 1;
    }
    guest->transition_line = on != 0;
    return 0;
}

int oracles_guest_transition_line(const OraclesGuest *guest) { return guest && guest->transition_line; }

void oracles_guest_line_counts(const OraclesGuest *guest, OraclesGuestLineCounts *out)
{
    if (!out) return;
    if (guest) *out = guest->line_counts; else memset(out, 0, sizeof *out);
}

void oracles_guest_edge_test(OraclesGuest *guest, OraclesGuestEventType type)
{
    if (!guest->transition_line) return;
    const OraclesGuestTables *t = guest->tables;
    OraclesCoreRegisters r = oracles_core_registers(guest->core);
    const uint8_t *link = oracles_guest_object(guest, 0, 0);
    if (!link) return;
    OraclesGuestEdgeTest e;
    e.direction = (unsigned)(type - ORACLES_EVENT_EDGE_TEST_UP);
    e.a = (uint8_t)(r.af >> 8);
    /* (hl): the object Link steers, his own or an animal's (wLinkObjectIndex). */
    e.link = (r.hl >> 8) == (ORACLES_OBJECTS_BASE >> 8) && ((r.hl & 0xffu) == ORACLES_OBJ_Y + 1u || (r.hl & 0xffu) == ORACLES_OBJ_X + 1u);
    e.position = e.link ? link[r.hl & 0xffu] : 0;
    e.tileset_flags = oracles_guest_read8(guest, t->tileset_flags);
    e.swimming = oracles_guest_read8(guest, t->link_swimming_state);
    e.swim = guest->transition_swim;
    e.conveyor = oracles_guest_read8(guest, t->hole_or_conveyor);
    e.transitions_disabled = oracles_guest_read8(guest, t->disable_screen_transitions);
    e.room = oracles_guest_read8(guest, t->active_room);
    e.room_is_large = oracles_guest_read8(guest, t->room_is_large);
    e.room_width = oracles_guest_read8(guest, t->room_width);
    e.room_height = oracles_guest_read8(guest, t->room_height);
    e.map_last_column = guest->map_last_column;
    e.map_last_row = guest->map_last_row;
    const uint8_t a = oracles_guest_edge_test_a(&e);
    const int up_left = e.direction == 0u || e.direction == 3u;
    const int native = up_left ? e.position <= e.a : e.position > e.a, fires = up_left ? e.position <= a : e.position > a;
    if (a == e.a) { if (native && e.link) guest->line_counts.native_backs++; return; }
    if (native && !fires) guest->line_counts.rests++;
    if (fires) {
        guest->line_counts.clamps++;
        if (up_left && e.position >= below_zero(e.direction)) guest->line_counts.wraps++;
    }
    r.af = (uint16_t)((r.af & 0xffu) | (uint16_t)a << 8);
    oracles_core_set_registers(guest->core, &r);
}
