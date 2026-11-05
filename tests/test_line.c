/* The transitions at the line (guest.h): the bound each edge's comparison is given, and the search of the four
 * comparisons and of Ages' overworld edges in a ROM (screenTransitionState2's bytes, as the US images hold them). */
#include "guest.h"
#include "guest_struct_offsets.h"
#include "oracles_rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

static OraclesGuestEdgeTest on_foot(unsigned direction, uint8_t a, uint8_t position)
{
    OraclesGuestEdgeTest t;
    memset(&t, 0, sizeof t);
    t.direction = direction;
    t.a = a;
    t.position = position;
    t.link = 1;
    t.room_width = 20;
    t.room_height = 16;
    t.room = 0x55u;
    return t;
}

static int holds(const OraclesGuestEdgeTest *t, uint8_t a)
{
    return (t->direction == 0u || t->direction == 3u) ? t->position <= a : t->position > a;
}

static void test_bounds(void)
{
    /* The top and the left: at 1 to 5 Link stands (the game would have put him back on 6); at 0 the comparison holds
     * and puts him back on 0 ($ff + 1); below zero too, never taken for one past the bottom or the right. */
    for (unsigned d = 0; d < 4u; d += 3u) {
        for (unsigned p = 1; p <= 5u; p++) {
            OraclesGuestEdgeTest t = on_foot(d, 5u, (uint8_t)p);
            const uint8_t a = oracles_guest_edge_test_a(&t);
            CHECK(!holds(&t, a) && holds(&t, 5u));
        }
        OraclesGuestEdgeTest zero = on_foot(d, 5u, 0u);
        CHECK(oracles_guest_edge_test_a(&zero) == 0xffu && (uint8_t)(0xffu + 1u) == 0u);
        /* Wrapped below zero: the upper half of what lies past the room, from 192 up and down, from 208 across; below
         * that, past the other edge, which the other comparison takes. */
        const uint8_t from = d == 3u ? 208u : 192u;
        OraclesGuestEdgeTest below = on_foot(d, 5u, 0xfau), far = on_foot(d, 5u, from), other = on_foot(d, 5u, (uint8_t)(from - 1u));
        CHECK(oracles_guest_edge_test_a(&below) == 0xffu && oracles_guest_edge_test_a(&far) == 0xffu);
        CHECK(!holds(&other, oracles_guest_edge_test_a(&other)));
    }
    /* The bottom and the right: the room's last row and column, small and large rooms. */
    OraclesGuestEdgeTest down = on_foot(2u, 121u, 127u), right = on_foot(1u, 154u, 159u);
    CHECK(oracles_guest_edge_test_a(&down) == 127u && !holds(&down, 127u));
    CHECK(oracles_guest_edge_test_a(&right) == 159u && !holds(&right, 159u));
    down.position = 128u; right.position = 160u;
    CHECK(holds(&down, oracles_guest_edge_test_a(&down)) && holds(&right, oracles_guest_edge_test_a(&right)));
    /* A large room (240 by 176, the dungeons): the game's own bounds, at every position up to its last pixel. */
    for (unsigned p = 0; p < 240u; p++) {
        for (unsigned d = 0; d < 4u; d++) {
            const uint8_t bound = d == 1u ? 234u : d == 2u ? 169u : 5u;
            OraclesGuestEdgeTest large = on_foot(d, bound, (uint8_t)p);
            large.room_is_large = 1; large.room_width = 30; large.room_height = 22;
            CHECK(oracles_guest_edge_test_a(&large) == bound);
        }
    }
    /* A room whose size is not the small room's, wRoomIsLarge clear: the game's too. */
    OraclesGuestEdgeTest odd = on_foot(3u, 5u, 200u);
    odd.room_width = 30;
    CHECK(oracles_guest_edge_test_a(&odd) == 5u);
}

static void test_left_to_the_game(void)
{
    OraclesGuestEdgeTest t = on_foot(0u, 5u, 3u);
    t.link = 0;   /* an animal's object */
    CHECK(oracles_guest_edge_test_a(&t) == 5u);
    t = on_foot(0u, 5u, 3u);
    t.swimming = 3u;
    CHECK(oracles_guest_edge_test_a(&t) == 5u);
    /* Swimming with the transitions' swim: the line, at the bottom where his own tile (y+5) is still the room's last
     * row (122); on a current (wcc92), the game's bound. */
    t.swim = 1;
    CHECK(oracles_guest_edge_test_a(&t) == 0u);
    t = on_foot(2u, 121u, 122u);
    t.swimming = 3u; t.swim = 1;
    CHECK(oracles_guest_edge_test_a(&t) == 122u && !holds(&t, 122u));
    t.position = 123u;
    CHECK(holds(&t, oracles_guest_edge_test_a(&t)));
    t = on_foot(1u, 154u, 159u);
    t.swimming = 3u; t.swim = 1;
    CHECK(oracles_guest_edge_test_a(&t) == 159u);
    t.conveyor = 0x01u;
    CHECK(oracles_guest_edge_test_a(&t) == 154u);
    t = on_foot(1u, 154u, 157u);
    t.tileset_flags = ORACLES_TILESETFLAG_SIDESCROLL;
    CHECK(oracles_guest_edge_test_a(&t) == 154u);
    /* The transitions disabled (a script): the game's own bounds, Link held 6 px short of the edge; past the room
     * alone, back on the line, a coordinate wrapped below zero never taken for one past the other edge. */
    t = on_foot(2u, 121u, 125u);
    t.transitions_disabled = 1u;
    CHECK(oracles_guest_edge_test_a(&t) == 121u);
    t.position = 140u;
    CHECK(oracles_guest_edge_test_a(&t) == 127u);
    t = on_foot(0u, 5u, 3u);
    t.transitions_disabled = 1u;
    CHECK(oracles_guest_edge_test_a(&t) == 5u);
    t.position = 0xf0u;
    CHECK(oracles_guest_edge_test_a(&t) == 0xffu);
    t = on_foot(1u, 154u, 157u);
    t.transitions_disabled = 1u;
    CHECK(oracles_guest_edge_test_a(&t) == 154u);
    t.position = 190u;
    CHECK(oracles_guest_edge_test_a(&t) == 159u);
    /* The sea floor: Link walks there (wLinkSwimmingState zero). */
    t = on_foot(3u, 5u, 2u);
    t.tileset_flags = ORACLES_TILESETFLAG_UNDERWATER;
    CHECK(oracles_guest_edge_test_a(&t) == 0u);
    /* Ages' overworld: its right, bottom and left edges stay the game's, its top and every inner edge are the line's. */
    t = on_foot(1u, 154u, 157u);
    t.tileset_flags = ORACLES_TILESETFLAG_OUTDOORS; t.map_last_column = 0x0du; t.map_last_row = 0xd0u; t.room = 0x3du;
    CHECK(oracles_guest_edge_test_a(&t) == 154u);
    t.room = 0x3cu;
    CHECK(oracles_guest_edge_test_a(&t) == 159u);
    t.direction = 2u; t.a = 121u; t.room = 0xd4u;
    CHECK(oracles_guest_edge_test_a(&t) == 121u);
    t.room = 0xc4u;
    CHECK(oracles_guest_edge_test_a(&t) == 127u);
    t.direction = 3u; t.a = 5u; t.position = 3u; t.room = 0x40u;
    CHECK(oracles_guest_edge_test_a(&t) == 5u);
    t.direction = 0u; t.room = 0x05u;
    CHECK(oracles_guest_edge_test_a(&t) == 0u);
    t.tileset_flags = 0u; t.direction = 1u; t.a = 154u; t.position = 157u; t.room = 0x3du;   /* indoors: no map edge */
    CHECK(oracles_guest_edge_test_a(&t) == 159u);
    /* A scene walking him with the game's own keys (wUseSimulatedInput, its reading over at $ff): the game's bound; the
     * player's keys reversed against Ganon (2) are play. */
    t = on_foot(0u, 5u, 3u);
    t.simulated_input = 1u;
    CHECK(oracles_guest_edge_test_a(&t) == 5u);
    t.simulated_input = 0xffu;
    CHECK(oracles_guest_edge_test_a(&t) == 5u);
    t.simulated_input = 2u;
    CHECK(oracles_guest_edge_test_a(&t) == 0u);
}

/* screenTransitionState2 from its start to the Ages-only checks of @transition, as the US images hold it. */
static const uint8_t AGES_STATE2[] = {
    0xfa, 0x5c, 0xcc, 0x87, 0x38, 0x07, 0x28, 0x05, 0x3e, 0x04, 0xea, 0x15, 0xcd, 0xfa, 0x02, 0xcd,
    0xcb, 0x7f, 0x28, 0x06, 0xe6, 0x7f, 0x4f, 0xc3, 0xd6, 0x41, 0xfa, 0x2c, 0xcc, 0x67, 0x2e, 0x0b,
    0x3e, 0x05, 0xbe, 0x30, 0x14, 0xfa, 0x0d, 0xcd, 0xbe, 0x38, 0x19, 0x2e, 0x0d, 0x3e, 0x05, 0xbe,
    0x30, 0x1c, 0xfa, 0x0c, 0xcd, 0xbe, 0x38, 0x1e, 0xc9, 0x3c, 0x77, 0x06, 0x40, 0x0e, 0x00, 0xcd,
    0x60, 0x41, 0x18, 0xe7, 0x77, 0x06, 0x80, 0x0e, 0x02, 0xcd, 0x60, 0x41, 0x18, 0xdd, 0x3c, 0x77,
    0x06, 0x20, 0x0e, 0x03, 0x18, 0x05, 0x77, 0x06, 0x10, 0x0e, 0x01, 0xfa, 0x00, 0xd0, 0xb7, 0xc8,
    0xfa, 0x91, 0xcc, 0xb7, 0xc0, 0xfa, 0x15, 0xcd, 0xb7, 0x28, 0x05, 0x3d, 0xea, 0x15, 0xcd, 0xc9,
    0xfa, 0x01, 0xd1, 0xfe, 0x0a, 0x28, 0x5a, 0xfa, 0x92, 0xcc, 0x87, 0xd8, 0xfa, 0x5c, 0xcc, 0x87,
    0x38, 0x4f, 0xfa, 0x2d, 0xd0, 0xb7, 0xc0, 0xfa, 0x92, 0xcc, 0x87, 0x20, 0x05, 0xcd, 0x02, 0x2a,
    0xa0, 0xc8, 0xfa, 0x34, 0xcc, 0xe6, 0x01, 0x28, 0x20, 0xfa, 0x30, 0xcc, 0x5f, 0xe6, 0x0f, 0xfe,
    0x0d, 0x20, 0x04, 0x79, 0xfe, 0x01, 0xc8, 0x7b, 0xfe, 0xd0, 0x38, 0x04, 0x79, 0xfe, 0x02, 0xc8,
};

static void test_find(void)
{
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    const size_t size = 1024u * 1024u;
    uint8_t *rom = calloc(size, 1);
    if (!rom) { failures++; return; }
    const OraclesGuestSym routine = t->screen_transition_state2;
    for (unsigned d = 0; d < 4u; d++) CHECK(oracles_guest_find_edge_test(t, rom, size, d) == 0);
    uint8_t column = 1, row = 1;
    oracles_guest_find_map_edges(t, rom, size, &column, &row);
    CHECK(column == 0 && row == 0);
    memcpy(rom + oracles_rom_offset(routine.bank, routine.addr), AGES_STATE2, sizeof AGES_STATE2);
    /* The `cp (hl)` of each comparison: $4127 the top, $412d the bottom, $4134 the left, $413a the right. */
    CHECK(oracles_guest_find_edge_test(t, rom, size, 0u) == 0x4127u);
    CHECK(oracles_guest_find_edge_test(t, rom, size, 1u) == 0x413au);
    CHECK(oracles_guest_find_edge_test(t, rom, size, 2u) == 0x412du);
    CHECK(oracles_guest_find_edge_test(t, rom, size, 3u) == 0x4134u);
    CHECK(oracles_guest_find_edge_test(t, rom, size, 4u) == 0);
    oracles_guest_find_map_edges(t, rom, size, &column, &row);
    CHECK(column == 0x0du && row == 0xd0u);
    /* Seasons' @transition has no such check. */
    rom[oracles_rom_offset(routine.bank, routine.addr) + 0xa1u] = 0x00u;
    oracles_guest_find_map_edges(t, rom, size, &column, &row);
    CHECK(column == 0 && row == 0);
    free(rom);
}

int main(void)
{
    test_bounds();
    test_left_to_the_game();
    test_find();
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("line: ok\n");
    return 0;
}
