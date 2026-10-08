/* Route files: write, read back, look up masks by frame. */
#include "route.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "test_route.tmp";
    OraclesRouteHeader header = { "ages", "880374fb978b18af4aa529e2e32f7ffb4d7dd2f4", "none", "continuous-transitions", ORACLES_ROUTE_CORE_JOYPAD_BOUNCING_OFF,
                                  "claw-game@2fd4e1c67a2d28fced849ee1bb76e7391b93eb12", "da39a3ee5e6b4b0d3255bfef95601890afd80709" };
    OraclesRouteWriter writer;
    CHECK(oracles_route_writer_open(&writer, path, &header) == 0);
    oracles_route_writer_record(&writer, 0, 0x00);
    oracles_route_writer_record(&writer, 1, 0x00);   /* unchanged: not written */
    oracles_route_writer_record(&writer, 480, 0x80);
    oracles_route_writer_record(&writer, 484, 0x00);
    oracles_route_writer_record(&writer, 1000, 0x11);
    CHECK(oracles_route_writer_close(&writer) == 0);

    OraclesRoute route;
    char error[128];
    CHECK(oracles_route_read(path, &route, error, sizeof error) == 0);
    CHECK(route.count == 4);
    CHECK(strcmp(route.header.game, "ages") == 0);
    CHECK(strcmp(route.header.rom_sha1, header.rom_sha1) == 0);
    CHECK(strcmp(route.header.sram_sha1, "none") == 0);
    /* The gameplay options the session ran with, written and read back; a name is matched whole. */
    CHECK(strcmp(route.header.options, "continuous-transitions") == 0);
    CHECK(strcmp(route.header.mods, "claw-game@2fd4e1c67a2d28fced849ee1bb76e7391b93eb12") == 0);   /* the mod the session ran */
    CHECK(route.format == ORACLES_ROUTE_FORMAT_MODS);   /* with mods, format 3: a reader of format 2 refuses it */
    CHECK(strcmp(route.header.store_sha1, "da39a3ee5e6b4b0d3255bfef95601890afd80709") == 0);   /* and the storage it starts from */
    CHECK(oracles_route_has_option(&route.header, ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS));
    CHECK(!oracles_route_joypad_bouncing(&route.header));   /* written with its `core` line, read back */
    CHECK(!oracles_route_has_option(&route.header, "continuous"));

    unsigned mask = 99;
    CHECK(oracles_route_mask_at(&route, 0, &mask) == 1 && mask == 0x00);
    CHECK(oracles_route_mask_at(&route, 479, &mask) == 1 && mask == 0x00);
    CHECK(oracles_route_mask_at(&route, 480, &mask) == 1 && mask == 0x80);
    CHECK(oracles_route_mask_at(&route, 483, &mask) == 1 && mask == 0x80);
    CHECK(oracles_route_mask_at(&route, 484, &mask) == 1 && mask == 0x00);
    CHECK(oracles_route_mask_at(&route, 1000, &mask) == 1 && mask == 0x11);
    CHECK(oracles_route_mask_at(&route, 1001, &mask) == 0);
    oracles_route_free(&route);

    /* Malformed files are refused with a line number. */
    FILE *f = fopen(path, "w");
    fprintf(f, "oracles-route 1\ngame ages\ninputs\n10 keys 80\n5 keys 00\n");
    fclose(f);
    CHECK(oracles_route_read(path, &route, error, sizeof error) == -1);
    CHECK(strstr(error, "line 5") != NULL);
    f = fopen(path, "w");
    fprintf(f, "oracles-route 4\ninputs\n");
    fclose(f);
    CHECK(oracles_route_read(path, &route, error, sizeof error) == -1);
    /* Format 2: the verbs that change the game's state, written and read back.  Two verbs may share a
     * frame, in the file's order; a same verb may not; a verb the reader does not know stops it. */
    {
        OraclesRouteWriter w2;
        const OraclesRouteAction exchange = { 1840, ORACLES_ROUTE_EQUIP, 1, 0, 0x05, 5, 0x0a, 3, 0x02, 0, 0 };
        const OraclesRouteAction variant_only = { 1900, ORACLES_ROUTE_EQUIP, 0, 0, 0, 0, 0, 1, 0x01, 0, 0 };
        const OraclesRouteAction use = { 1840, ORACLES_ROUTE_USE, 0, 0, 0, 0, 0, 0, 0, 1, 0x0a };
        const OraclesRouteAction bad = { 1, ORACLES_ROUTE_EQUIP, 1, 18, 0, 0, 0, 0, 0, 0, 0 };
        CHECK(oracles_route_writer_open(&w2, path, &header) == 0);
        oracles_route_writer_record(&w2, 0, 0x00);
        CHECK(oracles_route_writer_action(&w2, &exchange) == 0);
        CHECK(oracles_route_writer_action(&w2, &use) == 0);
        oracles_route_writer_record(&w2, 1841, 0x02);
        CHECK(oracles_route_writer_action(&w2, &variant_only) == 0);
        CHECK(oracles_route_writer_action(&w2, &bad) == -1);
        CHECK(oracles_route_writer_close(&w2) == 0);
        CHECK(oracles_route_read(path, &route, error, sizeof error) == 0);
        CHECK(route.format == ORACLES_ROUTE_FORMAT_MODS && route.count == 2 && route.action_count == 3);   /* the header names a mod: format 3, which carries the verbs of format 2 */
        size_t first = 99;
        CHECK(oracles_route_actions_at(&route, 1840, &first) == 2 && first == 0);
        CHECK(route.actions[0].verb == ORACLES_ROUTE_EQUIP && route.actions[0].swap && route.actions[0].slot_a == 0 && route.actions[0].item_a == 0x05
              && route.actions[0].slot_b == 5 && route.actions[0].item_b == 0x0a && route.actions[0].variant == 3 && route.actions[0].variant_value == 2);
        CHECK(route.actions[1].verb == ORACLES_ROUTE_USE && route.actions[1].button == 1 && route.actions[1].item == 0x0a);
        CHECK(oracles_route_actions_at(&route, 1900, &first) == 1 && first == 2 && !route.actions[2].swap && route.actions[2].variant == 1);
        CHECK(oracles_route_actions_at(&route, 5, &first) == 0);
        /* The route lasts to its last line, an exchange noted after the last change of keys included, the keys staying what they were. */
        unsigned held = 0;
        CHECK(oracles_route_last_frame(&route) == 1900);
        CHECK(oracles_route_mask_at(&route, 1900, &held) == 1 && held == 0x02 && oracles_route_mask_at(&route, 1901, &held) == 0);
        oracles_route_free(&route);
    }
    static const char *const refused[] = {
        "oracles-route 2\ninputs\n10 equip b=05 s3=0a\n10 equip b=0a s3=05\n",   /* a same verb twice at a frame */
        "oracles-route 2\ninputs\n10 equip b=05 s3=0a\n9 keys 01\n",             /* a frame going back */
        "oracles-route 2\ninputs\n10 inject 1 2 3\n",                            /* an unknown verb: never ignored */
        "oracles-route 1\ninputs\n10 equip b=05 s3=0a\n",                         /* format 1 has the keys alone */
        "oracles-route 2\ninputs\n10 equip b=05\n",                               /* half an exchange */
        "oracles-route 2\ninputs\n10 equip b=05 s16=0a\n",                        /* no such slot */
        "oracles-route 2\ninputs\n10 equip\n",
        "oracles-route 2\ninputs\n10 use s3 0a\n",                               /* a use is carried by a button */
        "oracles-route 2\nmods claw-game@2fd4e1c67a2d28fced849ee1bb76e7391b93eb12\ninputs\n0 keys 00\n",   /* mods need format 3 */
        "oracles-route 4\ninputs\n0 keys 00\n",                                /* a format to come */
    };
    for (unsigned i = 0; i < sizeof refused / sizeof refused[0]; i++) {
        f = fopen(path, "w"); fputs(refused[i], f); fclose(f);
        CHECK(oracles_route_read(path, &route, error, sizeof error) == -1);
    }
    /* A route without options (the format's first files) has none; several options are comma-separated. */
    f = fopen(path, "w");
    fprintf(f, "oracles-route 1\ngame seasons\nrom_sha1 x\nsram_sha1 none\ninputs\n0 keys 00\n");
    fclose(f);
    CHECK(oracles_route_read(path, &route, error, sizeof error) == 0);
    CHECK(route.header.options[0] == 0 && !oracles_route_has_option(&route.header, ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS));
    /* A route recorded before the core's joypad bouncing was cut has no `core` line: it ran with it, and replays with it. */
    CHECK(route.header.core[0] == 0 && oracles_route_joypad_bouncing(&route.header));
    CHECK(route.header.mods[0] == 0);   /* no mods line: no mod */
    oracles_route_free(&route);
    /* A `core` setting this reader does not know: refused, not replayed as something else. */
    f = fopen(path, "w");
    fprintf(f, "oracles-route 2\ngame seasons\nrom_sha1 x\nsram_sha1 none\ncore joypad-bouncing-off-and-more\ninputs\n0 keys 00\n");
    fclose(f);
    CHECK(oracles_route_read(path, &route, error, sizeof error) == -1);
    f = fopen(path, "w");
    fprintf(f, "oracles-route 2\ngame seasons\nrom_sha1 x\nsram_sha1 none\ncore joypad-bouncing-off,mgba,gambatte\ninputs\n0 keys 00\n");
    fclose(f);
    CHECK(oracles_route_read(path, &route, error, sizeof error) == -1);
    f = fopen(path, "w");   /* mgba alone would read as a route with joypad bouncing, which mGBA never had */
    fprintf(f, "oracles-route 2\ngame seasons\nrom_sha1 x\nsram_sha1 none\ncore mgba\ninputs\n0 keys 00\n");
    fclose(f);
    CHECK(oracles_route_read(path, &route, error, sizeof error) == -1);
    /* A route recorded on mGBA says so beside the joypad bouncing it never had; one without the word was SameBoy's. */
    f = fopen(path, "w");
    fprintf(f, "oracles-route 2\ngame seasons\nrom_sha1 x\nsram_sha1 none\ncore joypad-bouncing-off,mgba\ninputs\n0 keys 00\n");
    fclose(f);
    CHECK(oracles_route_read(path, &route, error, sizeof error) == 0);
    CHECK(oracles_route_core_mgba(&route.header) && !oracles_route_joypad_bouncing(&route.header));
    oracles_route_free(&route);
    f = fopen(path, "w");
    fprintf(f, "oracles-route 2\ngame seasons\nrom_sha1 x\nsram_sha1 none\ncore joypad-bouncing-off\ninputs\n0 keys 00\n");
    fclose(f);
    CHECK(oracles_route_read(path, &route, error, sizeof error) == 0);
    CHECK(!oracles_route_core_mgba(&route.header) && !oracles_route_joypad_bouncing(&route.header));
    oracles_route_free(&route);
    OraclesRouteHeader two = { "ages", "x", "none", "a-mod,continuous-transitions", "", "", "" };
    CHECK(oracles_route_has_option(&two, "a-mod") && oracles_route_has_option(&two, ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS) && !oracles_route_has_option(&two, "mod"));

    remove(path);
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_route: ok\n");
    return 0;
}
