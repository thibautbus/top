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
    /* The language a route was played in: none here, so it replays in any; one named replays only in it. */
    char language[8];
    CHECK(oracles_route_language(&route.header, language, sizeof language)[0] == 0);
    CHECK(oracles_route_language_matches(&route.header, NULL) && oracles_route_language_matches(&route.header, "en"));
    CHECK(oracles_route_language_matches(&route.header, "fr") && oracles_route_language_matches(&route.header, "en-eu"));
    {
        OraclesRouteHeader played = route.header;
        snprintf(played.options, sizeof played.options, "continuous-transitions," ORACLES_ROUTE_OPTION_LANGUAGE "fr");
        CHECK(strcmp(oracles_route_language(&played, language, sizeof language), "fr") == 0);
        CHECK(oracles_route_language_matches(&played, "fr") && !oracles_route_language_matches(&played, "de") && !oracles_route_language_matches(&played, NULL));
        CHECK(oracles_route_has_option(&played, ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS));
        /* Written in format 4, which a reader of format 3 refuses rather than replay it in English; read back. */
        OraclesRouteWriter in_language;
        CHECK(oracles_route_writer_open(&in_language, path, &played) == 0);
        oracles_route_writer_record(&in_language, 0, 0x00);
        CHECK(oracles_route_writer_close(&in_language) == 0);
        OraclesRoute back;
        CHECK(oracles_route_read(path, &back, error, sizeof error) == 0 && back.format == ORACLES_ROUTE_FORMAT_LANGUAGE);
        CHECK(strcmp(oracles_route_language(&back.header, language, sizeof language), "fr") == 0 && strcmp(back.header.mods, played.mods) == 0);
        oracles_route_free(&back);
        FILE *older = fopen(path, "w");
        if (older) { fprintf(older, "oracles-route 3\ngame ages\noptions language-fr\ninputs\n0 keys 00\n"); fclose(older); }
        CHECK(oracles_route_read(path, &back, error, sizeof error) == -1 && strstr(error, "format 4"));
    }
    {
        /* The swim's hold, with the options a session wrote before the transitions' diagonal: format 5, which a reader
         * of format 4 refuses rather than replay it without; an older format naming it is refused. */
        OraclesRouteHeader held = route.header;
        snprintf(held.options, sizeof held.options, "%s", "continuous-transitions,continuous-swim," ORACLES_ROUTE_OPTION_LANGUAGE "en-eu," ORACLES_ROUTE_OPTION_SWIM_HOLD);
        OraclesRouteWriter writer_held;
        CHECK(oracles_route_writer_open(&writer_held, path, &held) == 0);
        oracles_route_writer_record(&writer_held, 0, 0x00);
        CHECK(oracles_route_writer_close(&writer_held) == 0);
        OraclesRoute back;
        CHECK(oracles_route_read(path, &back, error, sizeof error) == 0 && back.format == ORACLES_ROUTE_FORMAT_SWIM_HOLD);
        CHECK(oracles_route_has_option(&back.header, ORACLES_ROUTE_OPTION_SWIM_HOLD) && !strcmp(back.header.options, held.options));
        CHECK(strcmp(oracles_route_language(&back.header, language, sizeof language), "en-eu") == 0);
        oracles_route_free(&back);
        FILE *older = fopen(path, "w");
        if (older) { fprintf(older, "oracles-route 4\ngame ages\noptions swim-hold\ninputs\n0 keys 00\n"); fclose(older); }
        CHECK(oracles_route_read(path, &back, error, sizeof error) == -1 && strstr(error, "format 5"));
        older = fopen(path, "w");
        if (older) { fprintf(older, "oracles-route 17\ngame ages\ninputs\n0 keys 00\n"); fclose(older); }
        CHECK(oracles_route_read(path, &back, error, sizeof error) == -1);   /* a later format */
    }
    {
        /* The transitions' diagonal, with every other option a session writes: format 6, which a reader of format 5
         * refuses rather than replay it without; an older format naming it is refused. */
        OraclesRouteHeader diagonal = route.header;
        snprintf(diagonal.options, sizeof diagonal.options, "%s", "continuous-transitions,continuous-swim," ORACLES_ROUTE_OPTION_CONTINUOUS_DIAGONAL ","
                 ORACLES_ROUTE_OPTION_LANGUAGE "en-eu," ORACLES_ROUTE_OPTION_SWIM_HOLD);
        OraclesRouteWriter writer_diagonal;
        CHECK(oracles_route_writer_open(&writer_diagonal, path, &diagonal) == 0);
        oracles_route_writer_record(&writer_diagonal, 0, 0x00);
        CHECK(oracles_route_writer_close(&writer_diagonal) == 0);
        OraclesRoute back;
        CHECK(oracles_route_read(path, &back, error, sizeof error) == 0 && back.format == ORACLES_ROUTE_FORMAT_DIAGONAL);
        CHECK(oracles_route_has_option(&back.header, ORACLES_ROUTE_OPTION_CONTINUOUS_DIAGONAL) && !strcmp(back.header.options, diagonal.options));
        oracles_route_free(&back);
        FILE *older = fopen(path, "w");
        if (older) { fprintf(older, "oracles-route 5\ngame ages\noptions continuous-transitions,continuous-diagonal\ninputs\n0 keys 00\n"); fclose(older); }
        CHECK(oracles_route_read(path, &back, error, sizeof error) == -1 && strstr(error, "format 6"));
    }
    {
        /* The cheats, with every other option a session writes: format 7, which a reader of format 6 refuses rather than
         * replay it without; an older format naming one is refused. */
        OraclesRouteHeader cheats = route.header;
        snprintf(cheats.options, sizeof cheats.options, "%s", "continuous-transitions,continuous-swim," ORACLES_ROUTE_OPTION_CONTINUOUS_DIAGONAL ","
                 ORACLES_ROUTE_OPTION_LANGUAGE "en-eu," ORACLES_ROUTE_OPTION_SWIM_HOLD "," ORACLES_ROUTE_OPTION_CHEAT "no-damage,"
                 ORACLES_ROUTE_OPTION_CHEAT "ammo," ORACLES_ROUTE_OPTION_CHEAT "rupees");
        OraclesRouteWriter writer_cheats;
        CHECK(oracles_route_writer_open(&writer_cheats, path, &cheats) == 0);
        oracles_route_writer_record(&writer_cheats, 0, 0x00);
        CHECK(oracles_route_writer_close(&writer_cheats) == 0);
        OraclesRoute back;
        CHECK(oracles_route_read(path, &back, error, sizeof error) == 0 && back.format == ORACLES_ROUTE_FORMAT_CHEATS);
        CHECK(oracles_route_has_cheat(&back.header) && !strcmp(back.header.options, cheats.options));
        CHECK(!oracles_route_has_cheat(&route.header));
        oracles_route_free(&back);
        FILE *older = fopen(path, "w");
        if (older) { fprintf(older, "oracles-route 6\ngame ages\noptions continuous-transitions,cheat-ammo\ninputs\n0 keys 00\n"); fclose(older); }
        CHECK(oracles_route_read(path, &back, error, sizeof error) == -1 && strstr(error, "format 7"));
    }
    {
        /* The compact text, with the cheats: format 9, which a reader of format 8 refuses rather than replay it without;
         * an older format naming it is refused, and a `cheat` line leaves the format as it is. */
        OraclesRouteHeader compact = route.header;
        snprintf(compact.options, sizeof compact.options, "%s", "continuous-transitions," ORACLES_ROUTE_OPTION_TEXT_COMPACT "," ORACLES_ROUTE_OPTION_CHEAT "ammo");
        OraclesRouteWriter writer_compact;
        CHECK(oracles_route_writer_open(&writer_compact, path, &compact) == 0 && writer_compact.format == ORACLES_ROUTE_FORMAT_TEXT_COMPACT);
        oracles_route_writer_record(&writer_compact, 0, 0x00);
        CHECK(oracles_route_writer_cheat(&writer_compact, 10, "rupees", 1) == 0 && writer_compact.format == ORACLES_ROUTE_FORMAT_TEXT_COMPACT);
        CHECK(oracles_route_writer_close(&writer_compact) == 0);
        OraclesRoute back;
        CHECK(oracles_route_read(path, &back, error, sizeof error) == 0 && back.format == ORACLES_ROUTE_FORMAT_TEXT_COMPACT && back.cheat_count == 1);
        CHECK(oracles_route_has_option(&back.header, ORACLES_ROUTE_OPTION_TEXT_COMPACT) && !strcmp(back.header.options, compact.options));
        oracles_route_free(&back);
        FILE *older = fopen(path, "w");
        if (older) { fprintf(older, "oracles-route 8\ngame ages\noptions text-compact\ninputs\n0 keys 00\n"); fclose(older); }
        CHECK(oracles_route_read(path, &back, error, sizeof error) == -1 && strstr(error, "format 9"));
    }
    {
        /* The wide text box, classic or with the compact text: format 11, which a reader before it refuses; an older
         * format naming it is refused; a session that may change a setting writes it in 11 too, with its `set` lines. */
        OraclesRouteHeader wide = route.header;
        snprintf(wide.options, sizeof wide.options, "%s", ORACLES_ROUTE_OPTION_TEXT_COMPACT "," ORACLES_ROUTE_OPTION_TEXT_BOX_WIDE);
        OraclesRouteWriter writer_wide;
        CHECK(oracles_route_writer_open(&writer_wide, path, &wide) == 0 && writer_wide.format == ORACLES_ROUTE_FORMAT_TEXT_BOX_WIDE);
        oracles_route_writer_record(&writer_wide, 0, 0x00);
        CHECK(oracles_route_writer_cheat(&writer_wide, 10, "rupees", 1) == 0 && writer_wide.format == ORACLES_ROUTE_FORMAT_TEXT_BOX_WIDE);
        CHECK(oracles_route_writer_close(&writer_wide) == 0);
        OraclesRoute back;
        CHECK(oracles_route_read(path, &back, error, sizeof error) == 0 && back.format == 11u && back.cheat_count == 1);
        CHECK(oracles_route_has_option(&back.header, ORACLES_ROUTE_OPTION_TEXT_BOX_WIDE) && oracles_route_has_option(&back.header, ORACLES_ROUTE_OPTION_TEXT_COMPACT));
        oracles_route_free(&back);
        FILE *older = fopen(path, "w");
        if (older) { fprintf(older, "oracles-route 9\ngame ages\noptions text-box-wide\ninputs\n0 keys 00\n"); fclose(older); }
        CHECK(oracles_route_read(path, &back, error, sizeof error) == -1 && strstr(error, "format 11"));
        CHECK(oracles_route_writer_open_format(&writer_wide, path, &wide, ORACLES_ROUTE_FORMAT_SET) == 0 && writer_wide.format == ORACLES_ROUTE_FORMAT_TEXT_BOX_WIDE);
        oracles_route_writer_record(&writer_wide, 0, 0x00);
        CHECK(oracles_route_writer_set(&writer_wide, 20, ORACLES_ROUTE_SET_SWIM_HOLD, 1) == 0);
        CHECK(oracles_route_writer_close(&writer_wide) == 0);
        CHECK(oracles_route_read(path, &back, error, sizeof error) == 0 && back.format == 11u && back.setting_count == 1
              && oracles_route_has_option(&back.header, ORACLES_ROUTE_OPTION_TEXT_BOX_WIDE));
        oracles_route_free(&back);
    }
    {
        /* The instant text: format 12, which a reader before it refuses; an older format naming it is refused; its `set
         * text-speed` lines are format 12's, a route of 10 or 11 carrying none, and a session that may change a setting
         * writes 12 from the start. */
        OraclesRouteHeader instant = route.header;
        snprintf(instant.options, sizeof instant.options, "%s", ORACLES_ROUTE_OPTION_TEXT_COMPACT "," ORACLES_ROUTE_OPTION_TEXT_BOX_WIDE "," ORACLES_ROUTE_OPTION_TEXT_INSTANT);
        OraclesRouteWriter writer_instant;
        CHECK(oracles_route_writer_open(&writer_instant, path, &instant) == 0 && writer_instant.format == ORACLES_ROUTE_FORMAT_TEXT_INSTANT);
        oracles_route_writer_record(&writer_instant, 0, 0x00);
        CHECK(oracles_route_writer_set(&writer_instant, 30, ORACLES_ROUTE_SET_TEXT_SPEED, 0) == 0);
        CHECK(oracles_route_writer_set(&writer_instant, 40, ORACLES_ROUTE_SET_TEXT_SPEED, 2) == -1);   /* no such speed */
        CHECK(oracles_route_writer_close(&writer_instant) == 0);
        OraclesRoute back;
        CHECK(oracles_route_read(path, &back, error, sizeof error) == 0 && back.format == 12u && back.setting_count == 1
              && back.settings[0].name == ORACLES_ROUTE_SET_TEXT_SPEED && back.settings[0].value == 0 && back.settings[0].frame == 30u);
        CHECK(oracles_route_has_option(&back.header, ORACLES_ROUTE_OPTION_TEXT_INSTANT) && oracles_route_sets(&back, ORACLES_ROUTE_SET_TEXT_SPEED));
        CHECK(!strcmp(oracles_route_setting_name(ORACLES_ROUTE_SET_TEXT_SPEED), "text-speed")
              && !strcmp(oracles_route_setting_value(ORACLES_ROUTE_SET_TEXT_SPEED, 1), "instant"));
        oracles_route_free(&back);
        FILE *older = fopen(path, "w");
        if (older) { fprintf(older, "oracles-route 11\ngame ages\noptions text-instant\ninputs\n0 keys 00\n"); fclose(older); }
        CHECK(oracles_route_read(path, &back, error, sizeof error) == -1 && strstr(error, "format 12"));
        older = fopen(path, "w");
        if (older) { fprintf(older, "oracles-route 11\ngame ages\ninputs\n0 keys 00\n10 set text-speed instant\n"); fclose(older); }
        CHECK(oracles_route_read(path, &back, error, sizeof error) == -1);
        older = fopen(path, "w");
        if (older) { fprintf(older, "oracles-route 12\ngame ages\ninputs\n0 keys 00\n10 set text-speed instant\n20 set text-speed fast\n"); fclose(older); }
        CHECK(oracles_route_read(path, &back, error, sizeof error) == -1);   /* a speed this reader does not know */
        /* A route of format 11 opened by a session: no `set text-speed` line; the session opens the latest. */
        OraclesRouteHeader wide = route.header;
        snprintf(wide.options, sizeof wide.options, "%s", ORACLES_ROUTE_OPTION_TEXT_BOX_WIDE);
        CHECK(oracles_route_writer_open_format(&writer_instant, path, &wide, ORACLES_ROUTE_FORMAT_SET) == 0 && writer_instant.format == ORACLES_ROUTE_FORMAT_TEXT_BOX_WIDE);
        CHECK(oracles_route_writer_set(&writer_instant, 10, ORACLES_ROUTE_SET_TEXT_SPEED, 1) == -1);
        CHECK(oracles_route_writer_close(&writer_instant) == 0);
        CHECK(oracles_route_writer_open_format(&writer_instant, path, &wide, ORACLES_ROUTE_FORMAT_LATEST) == 0 && writer_instant.format == ORACLES_ROUTE_FORMAT_LATEST);
        oracles_route_writer_record(&writer_instant, 0, 0x00);
        CHECK(oracles_route_writer_set(&writer_instant, 10, ORACLES_ROUTE_SET_TEXT_SPEED, 1) == 0);
        CHECK(oracles_route_writer_close(&writer_instant) == 0);
        CHECK(oracles_route_read(path, &back, error, sizeof error) == 0 && back.format == ORACLES_ROUTE_FORMAT_LATEST && back.settings[0].value == 1);
        oracles_route_free(&back);
    }
    {
        /* The transitions' steps: format 13, which a reader before it refuses rather than replay it without; an older
         * format naming it is refused. */
        OraclesRouteHeader steps = route.header;
        snprintf(steps.options, sizeof steps.options, "%s", "continuous-transitions,continuous-swim,continuous-diagonal," ORACLES_ROUTE_OPTION_CONTINUOUS_STEPS ","
                 ORACLES_ROUTE_OPTION_TEXT_INSTANT);
        OraclesRouteWriter writer_steps;
        CHECK(oracles_route_writer_open(&writer_steps, path, &steps) == 0 && writer_steps.format == ORACLES_ROUTE_FORMAT_STEPS);
        oracles_route_writer_record(&writer_steps, 0, 0x00);
        CHECK(oracles_route_writer_set(&writer_steps, 10, ORACLES_ROUTE_SET_TEXT_SPEED, 0) == 0);
        CHECK(oracles_route_writer_close(&writer_steps) == 0);
        OraclesRoute back;
        CHECK(oracles_route_read(path, &back, error, sizeof error) == 0 && back.format == 13u && back.setting_count == 1);
        CHECK(oracles_route_has_option(&back.header, ORACLES_ROUTE_OPTION_CONTINUOUS_STEPS) && !strcmp(back.header.options, steps.options));
        oracles_route_free(&back);
        FILE *older = fopen(path, "w");
        if (older) { fprintf(older, "oracles-route 12\ngame ages\noptions continuous-transitions,continuous-steps\ninputs\n0 keys 00\n"); fclose(older); }
        CHECK(oracles_route_read(path, &back, error, sizeof error) == -1 && strstr(error, "format 13"));
    }
    {
        /* The transitions at the line: format 15, the same way. */
        OraclesRouteHeader line = route.header;
        snprintf(line.options, sizeof line.options, "%s", "continuous-transitions,continuous-swim,continuous-diagonal,continuous-steps," ORACLES_ROUTE_OPTION_CONTINUOUS_LINE);
        OraclesRouteWriter writer_line;
        CHECK(oracles_route_writer_open(&writer_line, path, &line) == 0 && writer_line.format == ORACLES_ROUTE_FORMAT_LINE);
        oracles_route_writer_close(&writer_line);
        /* A session that took a scene's walk as the game does (the launcher, the transitions on) says so: format 16, which
         * a replay leaves to the game; a route before it replays that walk as the transitions took it. */
        CHECK(oracles_route_writer_open_format(&writer_line, path, &line, ORACLES_ROUTE_FORMAT_SCENE_WALKS) == 0 && writer_line.format == ORACLES_ROUTE_FORMAT_SCENE_WALKS);
        oracles_route_writer_record(&writer_line, 0, 0x00);
        CHECK(oracles_route_writer_close(&writer_line) == 0);
        OraclesRoute back;
        CHECK(oracles_route_read(path, &back, error, sizeof error) == 0 && back.format == 16u);
        CHECK(!oracles_route_scene_walks_taken(&back));   /* its walks left to the game, as when it was written */
        OraclesRoute probe = back;
        probe.format = ORACLES_ROUTE_FORMAT_LINE;
        CHECK(oracles_route_scene_walks_taken(&probe));    /* written before: the transitions take them, as they did */
        snprintf(probe.header.options, sizeof probe.header.options, "%s", "swim-hold");
        CHECK(!oracles_route_scene_walks_taken(&probe));   /* no transitions to take them */
        CHECK(!oracles_route_scene_walks_taken(NULL));
        CHECK(oracles_route_has_option(&back.header, ORACLES_ROUTE_OPTION_CONTINUOUS_LINE) && !strcmp(back.header.options, line.options));
        oracles_route_free(&back);
        FILE *older = fopen(path, "w");
        if (older) { fprintf(older, "oracles-route 14\ngame ages\noptions continuous-transitions,continuous-steps,continuous-line\ninputs\n0 keys 00\n"); fclose(older); }
        CHECK(oracles_route_read(path, &back, error, sizeof error) == -1 && strstr(error, "format 15"));
    }
    {
        /* A cheat changed during the session: a `cheat` line, in a route opened in the format a session that may change
         * one writes from the start (a route opened in an earlier one cannot carry it); two cheats at a same frame, one
         * cheat's frames increasing. */
        OraclesRouteHeader plain = route.header;
        plain.mods[0] = 0;
        snprintf(plain.options, sizeof plain.options, "%s", ORACLES_ROUTE_OPTION_CHEAT "ammo");
        OraclesRouteWriter writer_verb;
        CHECK(oracles_route_writer_open(&writer_verb, path, &plain) == 0 && writer_verb.format == ORACLES_ROUTE_FORMAT_CHEATS);
        CHECK(oracles_route_writer_cheat(&writer_verb, 600, "rupees", 1) == -1 && writer_verb.format == ORACLES_ROUTE_FORMAT_CHEATS);
        CHECK(oracles_route_writer_close(&writer_verb) == 0);
        CHECK(oracles_route_writer_open_format(&writer_verb, path, &plain, ORACLES_ROUTE_FORMAT_SET) == 0 && writer_verb.format == ORACLES_ROUTE_FORMAT_SET);
        oracles_route_writer_record(&writer_verb, 0, 0x00);
        CHECK(oracles_route_writer_cheat(&writer_verb, 600, "rupees", 1) == 0 && writer_verb.format == ORACLES_ROUTE_FORMAT_SET);
        CHECK(oracles_route_writer_cheat(&writer_verb, 600, "no-damage", 1) == 0);
        oracles_route_writer_record(&writer_verb, 601, 0x10);
        CHECK(oracles_route_writer_cheat(&writer_verb, 900, "rupees", 0) == 0);
        CHECK(oracles_route_writer_cheat(&writer_verb, 901, "two words", 1) == -1 && oracles_route_writer_cheat(&writer_verb, 901, "", 1) == -1);
        CHECK(oracles_route_writer_close(&writer_verb) == 0);
        OraclesRoute back;
        CHECK(oracles_route_read(path, &back, error, sizeof error) == 0 && back.format == ORACLES_ROUTE_FORMAT_SET);
        CHECK(!strcmp(back.header.options, plain.options) && back.count == 2 && back.action_count == 0 && back.cheat_count == 3);
        size_t first = 99;
        CHECK(oracles_route_cheats_at(&back, 600, &first) == 2 && first == 0);
        CHECK(back.cheat_count == 3 && !strcmp(back.cheats[0].name, "rupees") && back.cheats[0].on && !strcmp(back.cheats[1].name, "no-damage") && back.cheats[1].on);
        CHECK(oracles_route_cheats_at(&back, 900, &first) == 1 && first == 2 && !strcmp(back.cheats[2].name, "rupees") && !back.cheats[2].on);
        CHECK(oracles_route_cheats_at(&back, 601, &first) == 0);
        CHECK(oracles_route_last_frame(&back) == 900);   /* the route lasts to its last line, a cheat's too */
        oracles_route_free(&back);
        static const char *const malformed[] = {
            "oracles-route 7\ninputs\n0 keys 00\n10 cheat rupees on\n",                    /* the verb needs format 8 */
            "oracles-route 8\ninputs\n10 cheat rupees on\n10 cheat rupees off\n",          /* a cheat twice at a frame */
            "oracles-route 8\ninputs\n10 cheat rupees on\n9 cheat ammo on\n",              /* a frame going back */
            "oracles-route 8\ninputs\n10 cheat rupees\n",
            "oracles-route 8\ninputs\n10 cheat rupees yes\n",
            "oracles-route 8\ninputs\n10 cheat rupees on now\n",
            "oracles-route 8\ninputs\n10 cheat a-name-far-too-long on\n",
        };
        for (unsigned i = 0; i < sizeof malformed / sizeof malformed[0]; i++) {
            FILE *bad = fopen(path, "w");
            if (bad) { fputs(malformed[i], bad); fclose(bad); }
            CHECK(oracles_route_read(path, &back, error, sizeof error) == -1);
        }
        FILE *two = fopen(path, "w");   /* two cheats at a frame, one of them again later: read */
        if (two) { fputs("oracles-route 8\ninputs\n0 keys 00\n10 cheat ammo on\n10 cheat rupees on\n11 cheat ammo off\n", two); fclose(two); }
        CHECK(oracles_route_read(path, &back, error, sizeof error) == 0 && back.cheat_count == 3);
        oracles_route_free(&back);
    }
    {
        /* Format 10: a setting changed during the session, `set <name> <value>`, beside the cheats; each name's values
         * written and read back, two settings at a same frame, one setting's frames increasing; a header of format 9,
         * whose options name the compact text, opened in 10 from the start. */
        OraclesRouteHeader compact = route.header;
        compact.mods[0] = 0;
        snprintf(compact.options, sizeof compact.options, "%s", ORACLES_ROUTE_OPTION_TEXT_COMPACT);
        OraclesRouteWriter writer_set;
        CHECK(oracles_route_writer_open(&writer_set, path, &compact) == 0 && writer_set.format == ORACLES_ROUTE_FORMAT_TEXT_COMPACT);
        CHECK(oracles_route_writer_set(&writer_set, 10, ORACLES_ROUTE_SET_SWIM_HOLD, 1) == -1);   /* format 9 cannot carry it */
        CHECK(oracles_route_writer_close(&writer_set) == 0);
        CHECK(oracles_route_writer_open_format(&writer_set, path, &compact, ORACLES_ROUTE_FORMAT_SET) == 0 && writer_set.format == ORACLES_ROUTE_FORMAT_SET);
        oracles_route_writer_record(&writer_set, 0, 0x00);
        CHECK(oracles_route_writer_set(&writer_set, 300, ORACLES_ROUTE_SET_SWIM_HOLD, 1) == 0);
        CHECK(oracles_route_writer_set(&writer_set, 300, ORACLES_ROUTE_SET_ITEM_HOTKEYS, 1) == 0);
        CHECK(oracles_route_writer_cheat(&writer_set, 300, "ammo", 1) == 0);
        CHECK(oracles_route_writer_set(&writer_set, 301, ORACLES_ROUTE_SET_WINDOW, 3) == 0);
        oracles_route_writer_record(&writer_set, 302, 0x10);
        CHECK(oracles_route_writer_set(&writer_set, 500, ORACLES_ROUTE_SET_SWIM_HOLD, 0) == 0);
        CHECK(oracles_route_writer_set(&writer_set, 500, ORACLES_ROUTE_SET_ITEM_HOTKEYS, 0) == 0);
        CHECK(oracles_route_writer_set(&writer_set, 600, ORACLES_ROUTE_SET_WINDOW, 0) == 0);
        CHECK(oracles_route_writer_set(&writer_set, 601, ORACLES_ROUTE_SET_ITEM_HOTKEYS, 2) == 0);
        CHECK(oracles_route_writer_set(&writer_set, 602, ORACLES_ROUTE_SET_SWIM_HOLD, 2) == -1);   /* no such value */
        CHECK(oracles_route_writer_set(&writer_set, 602, ORACLES_ROUTE_SET_WINDOW, 1) == -1);
        CHECK(oracles_route_writer_set(&writer_set, 602, ORACLES_ROUTE_SETTINGS, 0) == -1);
        CHECK(oracles_route_writer_close(&writer_set) == 0);
        FILE *text = fopen(path, "r");
        char lines[512] = "";
        if (text) { const size_t n = fread(lines, 1, sizeof lines - 1, text); lines[n] = 0; fclose(text); }
        CHECK(!strncmp(lines, "oracles-route 10\n", 17) && strstr(lines, "\n300 set swim-hold on\n300 set item-hotkeys use\n300 cheat ammo on\n301 set window 3\n")
              && strstr(lines, "\n600 set window full\n601 set item-hotkeys equip\n"));
        OraclesRoute back;
        CHECK(oracles_route_read(path, &back, error, sizeof error) == 0 && back.format == ORACLES_ROUTE_FORMAT_SET);
        CHECK(back.setting_count == 7 && back.cheat_count == 1 && back.count == 2);
        size_t first = 99;
        CHECK(oracles_route_settings_at(&back, 300, &first) == 2 && first == 0);
        CHECK(back.settings[0].name == ORACLES_ROUTE_SET_SWIM_HOLD && back.settings[0].value == 1 && back.settings[1].name == ORACLES_ROUTE_SET_ITEM_HOTKEYS
              && back.settings[1].value == 1);
        CHECK(oracles_route_settings_at(&back, 301, &first) == 1 && first == 2 && back.settings[2].name == ORACLES_ROUTE_SET_WINDOW && back.settings[2].value == 3);
        CHECK(oracles_route_settings_at(&back, 500, &first) == 2 && first == 3 && back.settings[3].value == 0 && back.settings[4].value == 0);
        CHECK(oracles_route_settings_at(&back, 600, &first) == 1 && back.settings[5].name == ORACLES_ROUTE_SET_WINDOW && back.settings[5].value == 0);
        CHECK(back.settings[6].name == ORACLES_ROUTE_SET_ITEM_HOTKEYS && back.settings[6].value == 2);
        CHECK(oracles_route_settings_at(&back, 302, &first) == 0);
        CHECK(oracles_route_sets(&back, ORACLES_ROUTE_SET_SWIM_HOLD) && oracles_route_sets(&back, ORACLES_ROUTE_SET_WINDOW));
        CHECK(oracles_route_last_frame(&back) == 601);   /* the route lasts to its last line, a setting's too */
        oracles_route_free(&back);
        CHECK(!strcmp(oracles_route_setting_name(ORACLES_ROUTE_SET_ITEM_HOTKEYS), "item-hotkeys") && !oracles_route_setting_name(ORACLES_ROUTE_SETTINGS));
        CHECK(!strcmp(oracles_route_setting_value(ORACLES_ROUTE_SET_WINDOW, 0), "full") && !oracles_route_setting_value(ORACLES_ROUTE_SET_WINDOW, 5));
        static const char *const malformed[] = {
            "oracles-route 9\ninputs\n0 keys 00\n10 set swim-hold on\n",                 /* the verb needs format 10 */
            "oracles-route 10\ninputs\n10 set swim-hold on\n10 set swim-hold off\n",     /* a setting twice at a frame */
            "oracles-route 10\ninputs\n10 set swim-hold on\n9 set window 2\n",           /* a frame going back */
            "oracles-route 10\ninputs\n10 set swim-hold\n",
            "oracles-route 10\ninputs\n10 set swim-hold yes\n",                           /* a value the name does not have */
            "oracles-route 10\ninputs\n10 set window 1\n",
            "oracles-route 10\ninputs\n10 set item-hotkeys on\n",
            "oracles-route 10\ninputs\n10 set view far\n",                                /* a setting this reader does not know */
            "oracles-route 10\ninputs\n10 set swim-hold on now\n",
            "oracles-route 17\ninputs\n0 keys 00\n",                                      /* a later format */
        };
        for (unsigned i = 0; i < sizeof malformed / sizeof malformed[0]; i++) {
            FILE *bad = fopen(path, "w");
            if (bad) { fputs(malformed[i], bad); fclose(bad); }
            CHECK(oracles_route_read(path, &back, error, sizeof error) == -1);
        }
        FILE *two = fopen(path, "w");   /* the routes of every earlier format still read: one of format 8 with its cheat */
        if (two) { fputs("oracles-route 8\ninputs\n0 keys 00\n10 cheat ammo on\n", two); fclose(two); }
        CHECK(oracles_route_read(path, &back, error, sizeof error) == 0 && back.cheat_count == 1 && back.setting_count == 0);
        oracles_route_free(&back);
    }

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
    fprintf(f, "oracles-route 17\ninputs\n");
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
    /* Format 14, the `sound` line: written in a route opened in it alone, read back among the actions. */
    {
        OraclesRouteWriter w14;
        const OraclesRouteAction sound = { 1841, ORACLES_ROUTE_SOUND, 0, 0, 0, 0, 0, 0, 0, 0, 0x56 };
        const OraclesRouteAction silent = { 1842, ORACLES_ROUTE_SOUND, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        const OraclesRouteAction thrown = { 1843, ORACLES_ROUTE_THROW, 0, 0, 0, 0, 0, 0, 0, 1, 0 };
        CHECK(oracles_route_writer_open(&w14, path, &header) == 0);
        CHECK(oracles_route_writer_action(&w14, &sound) == -1);
        CHECK(oracles_route_writer_close(&w14) == 0);
        CHECK(oracles_route_writer_open_format(&w14, path, &header, ORACLES_ROUTE_FORMAT_SOUND) == 0);
        oracles_route_writer_record(&w14, 0, 0x00);
        CHECK(oracles_route_writer_action(&w14, &sound) == 0);
        CHECK(oracles_route_writer_action(&w14, &silent) == -1);
        CHECK(oracles_route_writer_action(&w14, &thrown) == 0);
        CHECK(oracles_route_writer_close(&w14) == 0);
        CHECK(oracles_route_read(path, &route, error, sizeof error) == 0);
        CHECK(route.format == ORACLES_ROUTE_FORMAT_SOUND && route.action_count == 2 && route.actions[0].verb == ORACLES_ROUTE_SOUND
              && route.actions[0].frame == 1841 && route.actions[0].item == 0x56);
        CHECK(route.actions[1].verb == ORACLES_ROUTE_THROW && route.actions[1].frame == 1843 && route.actions[1].button == 1);
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
        "oracles-route 17\ninputs\n0 keys 00\n",                                /* a format to come */
        "oracles-route 13\ninputs\n10 sound 56\n",                             /* the sound needs format 14 */
        "oracles-route 13\ninputs\n10 throw b\n",                              /* and the throw */
        "oracles-route 14\ninputs\n10 throw s3\n",                             /* a throw is on a button */
        "oracles-route 14\ninputs\n10 sound 00\n",                             /* no sound 0 */
        "oracles-route 14\ninputs\n10 sound 56\n10 sound 56\n",               /* a same verb twice at a frame */
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
