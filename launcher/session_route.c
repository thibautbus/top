/* A session's routes (docs/ROUTES.md): the one --play replays, its options taken on, and the one --record writes. */
#include "session_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int refuse(char *error, size_t capacity, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(error, capacity, format, arguments);
    va_end(arguments);
    return 0;
}

int oracles_session_replay_route(session *s, const OraclesSessionOptions *o, char *error, size_t capacity)
{
    char reason[1024];
    if (oracles_route_read(o->play_path, &s->play, reason, sizeof reason) != 0) return refuse(error, capacity, "%s", reason);
    if (strcmp(s->play.header.rom_sha1, s->info.sha1) != 0 || strcmp(s->play.header.sram_sha1, s->save.sha1) != 0)
        return refuse(error, capacity, "the route was recorded with another ROM or another starting SRAM (route: rom %s, sram %s; now: rom %s, sram %s from %s)",
                      s->play.header.rom_sha1, s->play.header.sram_sha1, s->info.sha1, s->save.sha1, s->route_sram_path);
    if (!oracles_mod_session_matches_route(s->mod, s->play.header.mods, reason, sizeof reason)
        || !oracles_mod_session_matches_store(o->play_path, s->play.header.store_sha1, reason, sizeof reason)) return refuse(error, capacity, "%s", reason);
    s->playing = 1;
    fprintf(stderr, "oracles: replaying %zu input changes from %s; the session's save file is not touched\n", s->play.count, o->play_path);
    /* The route's gameplay options are part of the replay: a route played
     * with --continuous-transitions diverges without it, and the other way round. */
    /* A route recorded before the core's joypad bouncing was cut (the header's core line) ran with it and only replays with it. */
    if (oracles_route_joypad_bouncing(&s->play.header)) {
        if (oracles_core_set_joypad_bouncing(s->core, 1) == 0)
            fprintf(stderr, "oracles: the route was recorded with joypad bouncing: it is on for the replay, which may still differ from the session it was\n");
        else
            fprintf(stderr, "oracles: the route was recorded with SameBoy's joypad bouncing, which %s does not emulate: replayed without it\n",
                    oracles_core_version(s->core));
    }
    const int route_continuous = oracles_route_has_option(&s->play.header, ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS);
    if (route_continuous && s->profile && !oracles_compat_continuous_transitions(s->profile))
        return refuse(error, capacity, "the route was recorded with --continuous-transitions, which %s does not support", oracles_compat_name(s->profile));
    if (route_continuous && !s->continuous_transitions) { s->continuous_transitions = 1; s->enhanced = 1; fprintf(stderr, "oracles: the route was recorded with --continuous-transitions: the option is on for the replay\n"); }
    else if (!route_continuous && s->continuous_transitions) fprintf(stderr, "oracles: the route was recorded without --continuous-transitions: the replay diverges after its first transition\n");
    const int route_swim = oracles_route_has_option(&s->play.header, ORACLES_ROUTE_OPTION_CONTINUOUS_SWIM);
    if (route_swim && s->profile && !oracles_compat_continuous_swim(s->profile))
        return refuse(error, capacity, "the route was recorded with --continuous-swim, which %s does not support", oracles_compat_name(s->profile));
    if (route_swim && !s->continuous_swim) { s->continuous_swim = s->continuous_transitions = 1; s->enhanced = 1; fprintf(stderr, "oracles: the route was recorded with --continuous-swim: the option is on for the replay\n"); }
    else if (!route_swim && s->continuous_swim) fprintf(stderr, "oracles: the route was recorded without --continuous-swim: the replay diverges after its first transition swum\n");
    /* The transitions' diagonal, which the home screen's Continuous transitions take: the route's, whatever it is. */
    const int route_diagonal = oracles_route_has_option(&s->play.header, ORACLES_ROUTE_OPTION_CONTINUOUS_DIAGONAL);
    if (route_diagonal && s->profile && !oracles_compat_continuous_diagonal(s->profile))
        return refuse(error, capacity, "the route was recorded with --continuous-diagonal, which %s does not support", oracles_compat_name(s->profile));
    if (route_diagonal && !s->continuous_diagonal) { s->continuous_diagonal = s->continuous_transitions = 1; s->enhanced = 1; fprintf(stderr, "oracles: the route was recorded with --continuous-diagonal: the option is on for the replay\n"); }
    else if (!route_diagonal && s->continuous_diagonal) { s->continuous_diagonal = 0; fprintf(stderr, "oracles: the route was recorded without --continuous-diagonal: the option is off for the replay\n"); }
    /* The transitions' steps, which the home screen's Continuous transitions take: the route's, whatever it is. */
    const int route_steps = oracles_route_has_option(&s->play.header, ORACLES_ROUTE_OPTION_CONTINUOUS_STEPS);
    if (route_steps && s->profile && !oracles_compat_continuous_steps(s->profile))
        return refuse(error, capacity, "the route was recorded with --continuous-steps, which %s does not support", oracles_compat_name(s->profile));
    if (route_steps && !s->continuous_steps) { s->continuous_steps = s->continuous_transitions = 1; s->enhanced = 1; fprintf(stderr, "oracles: the route was recorded with --continuous-steps: the option is on for the replay\n"); }
    else if (!route_steps && s->continuous_steps) { s->continuous_steps = 0; fprintf(stderr, "oracles: the route was recorded without --continuous-steps: the option is off for the replay\n"); }
    /* The transitions at the line, which the home screen's Continuous transitions take: the route's, whatever it is. */
    const int route_line = oracles_route_has_option(&s->play.header, ORACLES_ROUTE_OPTION_CONTINUOUS_LINE);
    if (route_line && s->profile && !oracles_compat_continuous_line(s->profile))
        return refuse(error, capacity, "the route was recorded with --continuous-line, which %s does not support", oracles_compat_name(s->profile));
    if (route_line && !s->continuous_line) { s->continuous_line = s->continuous_steps = s->continuous_transitions = 1; s->enhanced = 1; fprintf(stderr, "oracles: the route was recorded with --continuous-line: the option is on for the replay\n"); }
    else if (!route_line && s->continuous_line) { s->continuous_line = 0; fprintf(stderr, "oracles: the route was recorded without --continuous-line: the option is off for the replay\n"); }
    /* The swim's hold, a setting the player may have on for every game: the route's, whatever it is. */
    const int route_swim_hold = oracles_route_has_option(&s->play.header, ORACLES_ROUTE_OPTION_SWIM_HOLD);
    if (route_swim_hold && !oracles_compat_swim_hold(s->profile))
        return refuse(error, capacity, "the route was recorded with --swim-hold, which %s does not support", s->profile ? oracles_compat_name(s->profile) : "this image");
    if (route_swim_hold != s->swim_hold)
        fprintf(stderr, "oracles: the route was recorded %s --swim-hold: the replay plays %s it\n", route_swim_hold ? "with" : "without", route_swim_hold ? "with" : "without");
    s->swim_hold = route_swim_hold;
    /* And as its `set swim-hold` lines change it, at their frames. */
    s->swim_route = oracles_route_sets(&s->play, ORACLES_ROUTE_SET_SWIM_HOLD);
    if (s->swim_route && !oracles_compat_swim_hold(s->profile))
        return refuse(error, capacity, "the route changes the swim's hold, which %s does not support", s->profile ? oracles_compat_name(s->profile) : "this image");
    /* The instant text, a setting of every game: the route's, whatever it is, and as its `set text-speed` lines change it. */
    const int route_instant = oracles_route_has_option(&s->play.header, ORACLES_ROUTE_OPTION_TEXT_INSTANT);
    s->text_speed_route = oracles_route_sets(&s->play, ORACLES_ROUTE_SET_TEXT_SPEED);
    if ((route_instant || s->text_speed_route) && !oracles_compat_text_instant(s->profile))
        return refuse(error, capacity, "the route was recorded with the instant text, which %s does not support", s->profile ? oracles_compat_name(s->profile) : "this image");
    if (route_instant != s->text_instant)
        fprintf(stderr, "oracles: the route was recorded with the %s text speed: the replay plays with it\n", route_instant ? "instant" : "classic");
    s->text_instant = route_instant;
    /* The compact text, a setting of every game: the route's, whatever it is (check_profile refuses it in Faithful). */
    const int route_compact = oracles_route_has_option(&s->play.header, ORACLES_ROUTE_OPTION_TEXT_COMPACT);
    if (route_compact != s->compact_text)
        fprintf(stderr, "oracles: the route was recorded with the %s text: the replay plays with it\n", route_compact ? "compact" : "classic");
    s->compact_text = route_compact;
    /* The wide text box: the same. */
    const int route_wide = oracles_route_has_option(&s->play.header, ORACLES_ROUTE_OPTION_TEXT_BOX_WIDE);
    if (route_wide != s->text_box_wide)
        fprintf(stderr, "oracles: the route was recorded with the %s text box: the replay plays with it\n", route_wide ? "wide" : "game's");
    s->text_box_wide = route_wide;
    /* The cheats, a setting of each game: the route's, whatever they are. */
    unsigned route_cheats = 0;
    char names[64];
    if (oracles_cheats_parse(s->play.header.options, ORACLES_ROUTE_OPTION_CHEAT, &route_cheats) != 0)
        return refuse(error, capacity, "the route was recorded with a cheat this version does not know (%s)", s->play.header.options);
    if (route_cheats && !oracles_compat_cheats(s->profile))
        return refuse(error, capacity, "the route was recorded with --cheats, which %s does not support", s->profile ? oracles_compat_name(s->profile) : "this image");
    if (route_cheats != s->cheats && route_cheats)
        fprintf(stderr, "oracles: the route was recorded with --cheats %s: the replay plays with them\n", oracles_cheats_format(route_cheats, NULL, names, sizeof names));
    else if (route_cheats != s->cheats) fprintf(stderr, "oracles: the route was recorded without cheats: the replay plays without them\n");
    s->cheats = route_cheats;
    for (size_t i = 0; i < s->play.cheat_count; i++) {   /* and those its `cheat` lines change, at their frames */
        unsigned bit = 0;
        if (oracles_cheats_parse(s->play.cheats[i].name, NULL, &bit) != 0)
            return refuse(error, capacity, "the route changes a cheat this version does not know (%s)", s->play.cheats[i].name);
        s->cheats_route |= bit;
    }
    if (s->cheats_route && !oracles_compat_cheats(s->profile))
        return refuse(error, capacity, "the route changes a cheat, which %s does not support", s->profile ? oracles_compat_name(s->profile) : "this image");
    char recorded[8];   /* a route played in a language replays in it; one without plays in any, as the hand-written ones do */
    if (oracles_route_language(&s->play.header, recorded, sizeof recorded)[0] && !oracles_route_language_matches(&s->play.header, o->language))
        return refuse(error, capacity, "the route was played in a language (%s): replay it with --language %s and its European ROM", recorded, recorded);
    if (!recorded[0] && o->language && strcmp(o->language, "en") != 0)
        fprintf(stderr, "oracles: the route names no language: a route played in English may diverge in another, its texts' lengths pacing the dialogues\n");
    return 1;
}

/* Whether two paths name the same file: the one to write would truncate the one being read. */
static int same_file(const char *a, const char *b)
{
    struct stat sa, sb;
    if (strcmp(a, b) == 0) return 1;
    return stat(a, &sa) == 0 && stat(b, &sb) == 0 && sa.st_ino != 0 && sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

int oracles_session_record_route(session *s, const OraclesSessionOptions *o, char *error, size_t capacity)
{
    if (o->play_path) {
        /* An extension records the replayed inputs, not the replayed inventory exchanges
         * (their effects are applied, never written): such a route would lose them. */
        if (same_file(o->play_path, o->record_path)) return refuse(error, capacity, "--play and --record name the same route");
        if (s->play.action_count) return refuse(error, capacity, "%s has inventory exchanges, which an extension would not carry: it cannot be extended", o->play_path);
        /* Its replay takes a scene's walk as the transitions did; the extension, in the format of today, would replay it as the game. */
        if (oracles_route_has_option(&s->play.header, ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS) && s->play.format < ORACLES_ROUTE_FORMAT_SCENE_WALKS)
            return refuse(error, capacity, "%s was written before scene walks were left to the game (format %u): it cannot be extended", o->play_path, s->play.format);
    }
    if (strcmp(s->save.sha1, "none") != 0) {
        const size_t size = oracles_core_sram_size(s->core);
        uint8_t *sram = malloc(size);
        const int ok = sram && oracles_core_save_sram(s->core, sram, size) == 0 && oracles_session_write_file(s->record_sram_path, sram, size);
        free(sram);
        if (!ok) return refuse(error, capacity, "cannot write the starting SRAM to %s", s->record_sram_path);
    } else {
        remove(s->record_sram_path);
    }
    OraclesRouteHeader header;
    snprintf(header.game, sizeof header.game, "%s", s->state_info.game);
    snprintf(header.rom_sha1, sizeof header.rom_sha1, "%s", s->info.sha1);
    snprintf(header.sram_sha1, sizeof header.sram_sha1, "%s", s->save.sha1);
    snprintf(header.options, sizeof header.options, "%s", s->continuous_swim ? ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS "," ORACLES_ROUTE_OPTION_CONTINUOUS_SWIM
                                                          : s->continuous_transitions ? ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS : "");
    if (s->continuous_diagonal) {   /* a diagonal crossing carries Link elsewhere without it */
        const size_t used = strlen(header.options);
        snprintf(header.options + used, sizeof header.options - used, "%s" ORACLES_ROUTE_OPTION_CONTINUOUS_DIAGONAL, used ? "," : "");
    }
    if (s->continuous_steps) {   /* and every transition ends him elsewhere */
        const size_t used = strlen(header.options);
        snprintf(header.options + used, sizeof header.options - used, "%s" ORACLES_ROUTE_OPTION_CONTINUOUS_STEPS, used ? "," : "");
    }
    if (s->continuous_line) {   /* and a scroll starts elsewhere, or not at all */
        const size_t used = strlen(header.options);
        snprintf(header.options + used, sizeof header.options - used, "%s" ORACLES_ROUTE_OPTION_CONTINUOUS_LINE, used ? "," : "");
    }
    if (oracles_language_session_translated(s->language)) {   /* the texts' lengths pace the dialogues: a replay plays in the same language */
        const size_t used = strlen(header.options);
        snprintf(header.options + used, sizeof header.options - used, "%s" ORACLES_ROUTE_OPTION_LANGUAGE "%s", used ? "," : "", o->language);
    }
    if (s->swim_hold) {   /* a stroke started by the hold is not in the keys: a replay starts it too */
        const size_t used = strlen(header.options);
        snprintf(header.options + used, sizeof header.options - used, "%s" ORACLES_ROUTE_OPTION_SWIM_HOLD, used ? "," : "");
    }
    if (s->compact_text) {   /* its pages pace the dialogues: a replay plays with it */
        const size_t used = strlen(header.options);
        snprintf(header.options + used, sizeof header.options - used, "%s" ORACLES_ROUTE_OPTION_TEXT_COMPACT, used ? "," : "");
    }
    if (s->text_box_wide) {   /* the same: its texts laid out again pace the dialogues */
        const size_t used = strlen(header.options);
        snprintf(header.options + used, sizeof header.options - used, "%s" ORACLES_ROUTE_OPTION_TEXT_BOX_WIDE, used ? "," : "");
    }
    if (s->text_instant) {   /* its pages pace the dialogues: a replay plays with it */
        const size_t used = strlen(header.options);
        snprintf(header.options + used, sizeof header.options - used, "%s" ORACLES_ROUTE_OPTION_TEXT_INSTANT, used ? "," : "");
    }
    if (s->cheats) {   /* a cheat's answers are not in the keys: a replay gives them too */
        const size_t used = strlen(header.options);
        snprintf(header.options + used, sizeof header.options - used, "%s", used ? "," : "");
        oracles_cheats_format(s->cheats, ORACLES_ROUTE_OPTION_CHEAT, header.options + strlen(header.options), sizeof header.options - strlen(header.options));
    }
    snprintf(header.mods, sizeof header.mods, "%s", oracles_mod_session_identity(s->mod));
    header.store_sha1[0] = 0;
    if (s->mod) {   /* the mods' storage the route starts from, as ROUTE.sram, named in the header */
        char path[PATH_MAX_LENGTH + 16];
        snprintf(path, sizeof path, "%s.store", o->record_path);
        if (!oracles_mod_session_storage_copy(s->mod, path)) return refuse(error, capacity, "cannot write %s", path);
        oracles_mod_session_file_sha1(path, header.store_sha1);
    }
    /* As every session runs, joypad bouncing off; an extension runs as the route it
     * extends, whose replay keeps the joypad bouncing of a route recorded before.  A session on mGBA, which has no
     * joypad bouncing, says so: its routes replay on mGBA. */
    if (oracles_core_kind(s->core) == ORACLES_CORE_MGBA)
        snprintf(header.core, sizeof header.core, "%s,%s", ORACLES_ROUTE_CORE_JOYPAD_BOUNCING_OFF, ORACLES_ROUTE_CORE_MGBA);
    else
        snprintf(header.core, sizeof header.core, "%s", o->play_path && oracles_route_joypad_bouncing(&s->play.header) ? "" : ORACLES_ROUTE_CORE_JOYPAD_BOUNCING_OFF);
    /* A session whose pause may change a setting, or that extends a route that changes some, may write `cheat` and `set`
     * lines, and one with the item hotkeys on `sound` lines: its header says the format they need from the start. */
    const int changes = s->pause || (o->play_path && (s->play.cheat_count || s->play.setting_count)) || s->hotkeys_mode != ORACLES_HOTKEYS_OFF;
    /* With the transitions on, a scene's walk with the game's own keys was the game's: a replay leaves it to the game too. */
    const int scene_walks = oracles_route_has_option(&header, ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS) ? ORACLES_ROUTE_FORMAT_SCENE_WALKS : 0;
    if (oracles_route_writer_open_format(&s->record, o->record_path, &header, changes ? ORACLES_ROUTE_FORMAT_LATEST : scene_walks) != 0)
        return refuse(error, capacity, "cannot write %s", o->record_path);
    if (s->mod) {
        char path[PATH_MAX_LENGTH + 16];
        snprintf(path, sizeof path, "%s.mod.tsv", o->record_path);
        if (!oracles_mod_session_trace(s->mod, path)) return refuse(error, capacity, "cannot write %s", path);
    }
    s->recording = 1;
    return 1;
}
