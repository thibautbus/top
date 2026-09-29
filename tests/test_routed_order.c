/* The order in which the four directions of a room the game routes itself are
 * asked of the ghost: the sides before up and down, the nearer of
 * each pair first, a place off the map last; and the grid beyond an answer
 * that is an ordinary room.  And the order of the drawn-back band's runs:
 * the most shown, then the nearest. */
#include "camera.h"
#include "view.h"

#include <stdio.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

enum { UP = 0, RIGHT = 1, DOWN = 2, LEFT = 3 };

static int order_is(const int32_t gap[4], const int on_map[4], unsigned a, unsigned b, unsigned c, unsigned d)
{
    unsigned out[4];
    oracles_enhanced_routed_order(gap, on_map, out);
    return out[0] == a && out[1] == b && out[2] == c && out[3] == d;
}

int main(void)
{
    const int all[4] = { 1, 1, 1, 1 };
    /* Link right of the middle, above it: right, left, up, down. */
    const int32_t right_high[4] = { 30, 50, 98, 110 };
    CHECK(order_is(right_high, all, RIGHT, LEFT, UP, DOWN));
    /* Left of the middle, below it: left, right, down, up. */
    const int32_t left_low[4] = { 100, 120, 28, 40 };
    CHECK(order_is(left_low, all, LEFT, RIGHT, DOWN, UP));
    /* Against the top edge (a strip of the row above in the band): the sides still come first. */
    const int32_t top_edge[4] = { 0, 80, 128, 80 };
    CHECK(order_is(top_edge, all, RIGHT, LEFT, UP, DOWN));
    /* The Lost Woods (0:40, the map's first column): its left is off the map,
     * the camera never shows it at rest, so it comes last, even with Link
     * nearer that side. */
    const int left_off[4] = { 1, 1, 1, 0 };
    CHECK(order_is(left_low, left_off, RIGHT, DOWN, UP, LEFT));
    CHECK(order_is(right_high, left_off, RIGHT, UP, DOWN, LEFT));
    /* Off the map above and to the right: those two last, in the pairs' order. */
    const int corner[4] = { 0, 0, 1, 1 };
    CHECK(order_is(right_high, corner, LEFT, DOWN, RIGHT, UP));
    /* The drawn-back band: a side half shown after a diagonal wholly shown; of
     * two rooms shown alike, the nearer; rooms alike in both keep the normal order. */
    {
        const int32_t shown[6] = { 10240, 0, 20480, 20480, 0, 20480 }, distance[6] = { 100, 900, 400, 900, 800, 400 };
        unsigned out[6];
        oracles_enhanced_band_order(shown, distance, 6, out);
        CHECK(out[0] == 2 && out[1] == 5 && out[2] == 3 && out[3] == 0 && out[4] == 4 && out[5] == 1);
    }
    /* The grid beyond an answer of the Lost Woods (Seasons' map, 16 columns and 16 rows),
     * read through it: right of 0:40, 0:41, an ordinary room, has 0:42 beyond it,
     * 0:31 and 0:51 beside it, 0:32 and 0:52 beside the room beyond. */
    {
        uint8_t rooms[5];
        int have[5];
        enum { W = 16, H = 16 };   /* OVERWORLD_WIDTH and OVERWORLD_HEIGHT of Seasons */
        CHECK(oracles_enhanced_routed_beyond(0x40, RIGHT, 0x41, 0, W, H, rooms, have) == 5);
        CHECK(rooms[0] == 0x42 && rooms[1] == 0x31 && rooms[2] == 0x51 && rooms[3] == 0x32 && rooms[4] == 0x52);
        /* Up, when the sequence north succeeds: 0:30, with nothing left of the map's first column. */
        CHECK(oracles_enhanced_routed_beyond(0x40, UP, 0x30, 0, W, H, rooms, have) == 3);
        CHECK(have[0] && rooms[0] == 0x20 && !have[1] && have[2] && rooms[2] == 0x31 && !have[3] && have[4] && rooms[4] == 0x21);
        /* Never through an answer the game routes itself (the woods again), nor one that is not the grid's neighbour there. */
        CHECK(oracles_enhanced_routed_beyond(0x40, LEFT, 0x40, 1, W, H, rooms, have) == 0 && !have[0]);
        CHECK(oracles_enhanced_routed_beyond(0x40, RIGHT, 0x41, 1, W, H, rooms, have) == 0);
        CHECK(oracles_enhanced_routed_beyond(0x40, DOWN, 0x40, 0, W, H, rooms, have) == 0);
        CHECK(oracles_enhanced_routed_beyond(0x40, LEFT, 0x3f, 0, W, H, rooms, have) == 0);   /* off the map that way */
        /* At the map's last column, nothing beyond. */
        CHECK(oracles_enhanced_routed_beyond(0x4e, RIGHT, 0x4f, 0, W, H, rooms, have) == 2 && !have[0] && !have[3] && !have[4]);
        CHECK(oracles_enhanced_routed_beyond(0x4e, RIGHT, 0x4f, 0, 15, H, rooms, have) == 0);   /* a map one column narrower: 0:4f off it */
    }
    /* An answer of the Lost Woods holds while the sequence's counters it was
     * asked under hold; the direction being taken holds through its transition. */
    {
        const uint8_t asked[2] = { 0, 0 }, same[2] = { 0, 0 }, moved[2] = { 1, 1 };
        CHECK(oracles_enhanced_routed_answer_holds(UP, -1, asked, same, 2));
        CHECK(!oracles_enhanced_routed_answer_holds(UP, -1, asked, moved, 2));
        CHECK(!oracles_enhanced_routed_answer_holds(UP, LEFT, asked, moved, 2));
        CHECK(oracles_enhanced_routed_answer_holds(LEFT, LEFT, asked, moved, 2));
    }
    /* A dialogue's box holds the frame the game's flag falls, while its cells
     * are still in the displayed map, and no longer once they are restored. */
    {
        unsigned trail = 0;
        CHECK(oracles_enhanced_text_box_holds(1, 90, &trail) && oracles_enhanced_text_box_holds(1, 90, &trail));
        CHECK(oracles_enhanced_text_box_holds(0, 90, &trail));      /* the flag fell, the box still drawn */
        CHECK(oracles_enhanced_text_box_holds(0, 90, &trail));      /* a second frame at most */
        CHECK(!oracles_enhanced_text_box_holds(0, 90, &trail));
        trail = 0;
        CHECK(oracles_enhanced_text_box_holds(1, 90, &trail) && !oracles_enhanced_text_box_holds(0, 0, &trail));   /* the room restored: gone at once */
        CHECK(!oracles_enhanced_text_box_holds(0, 90, &trail));     /* and not taken up again */
    }
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_routed_order: ok\n");
    return 0;
}
