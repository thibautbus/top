/* The session's files and its savestates (F5, F7): split from session.c,
 * whose state they share through session_internal.h. */
#include "session_internal.h"

#include "state.h"
#include "state_display.h"
#include "state_refusal.h"

#include <stdlib.h>
#include <string.h>

/* Writes a file through a temporary name; rename replaces atomically where the
 * platform allows it, and the remove-then-rename fallback covers the rest. */
int oracles_session_write_file(const char *path, const uint8_t *data, size_t size)
{
    char temporary[PATH_MAX_LENGTH + 8];
    snprintf(temporary, sizeof temporary, "%s.tmp", path);
    FILE *f = fopen(temporary, "wb");
    if (!f) return 0;
    const int ok = fwrite(data, 1, size, f) == size && fflush(f) == 0;
    if (fclose(f) != 0 || !ok) { remove(temporary); return 0; }
    if (rename(temporary, path) == 0) return 1;
    remove(path);
    if (rename(temporary, path) == 0) return 1;
    remove(temporary);
    return 0;
}

/* Reads a whole file into a buffer the caller frees. Returns 1, or 0 when the file does not exist or cannot be read. */
int oracles_session_read_file(const char *path, uint8_t **data, size_t *size)
{
    *data = NULL;
    *size = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    const long length = ftell(f);
    if (length < 0) { fclose(f); return 0; }
    rewind(f);
    uint8_t *buffer = malloc((size_t)length + 1);
    if (!buffer || fread(buffer, 1, (size_t)length, f) != (size_t)length) { free(buffer); fclose(f); return 0; }
    fclose(f);
    *data = buffer;
    *size = (size_t)length;
    return 1;
}

/* Whether a savestate's set of mods and gameplay options (words apart by commas or spaces) holds the transitions. */
static int has_transitions(const char *mods)
{
    const size_t length = strlen(ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS);
    for (const char *at = mods; (at = strstr(at, ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS)) != NULL; at += length) {
        const char after = at[length];
        if ((at == mods || at[-1] == ',' || at[-1] == ' ') && (after == 0 || after == ',' || after == ' ')) return 1;
    }
    return 0;
}

/* F5: the savestate written next to the save.  1 when done; otherwise 0, and *message says why, shortly (the pause
 * menu shows it), the whole reason going to stderr. */
int oracles_session_save_state(session *s, char *message, size_t capacity)
{
    char error[256];
    uint8_t *data = NULL;
    size_t size = 0, host_size = 0;
    if (oracles_mod_session_busy(s->mod)) {
        fprintf(stderr, "oracles: no savestate while the mod's conversation runs (its Lua cannot be saved)\n");
        snprintf(message, capacity, "Refused: the mod's conversation is running");
        return 0;
    }
    if (s->view && oracles_enhanced_view_save_state(s->view, s->host_state, sizeof s->host_state, &host_size) == 0) {
        s->state_info.host_state = s->host_state;
        s->state_info.host_state_size = host_size;
    } else {
        s->state_info.host_state = NULL;
        s->state_info.host_state_size = 0;
    }
    /* The mods' storage: every file's, and the live tables, which a load gives back with the SRAM. */
    uint8_t *storage = NULL;
    size_t storage_size = 0;
    if (s->mod && !(storage = oracles_mod_session_storage_state(s->mod, &storage_size, error, sizeof error))) {
        fprintf(stderr, "oracles: no savestate: %s\n", error);
        snprintf(message, capacity, "Refused: %s", error);
        return 0;
    }
    s->state_info.mods_storage = storage;
    s->state_info.mods_storage_size = storage_size;
    /* The transitions' progress through the transition in progress, which the game's state alone does not say. */
    uint8_t transition[ORACLES_ROOM_TRANSITION_STATE_SIZE];
    s->state_info.transition_state = transition;
    s->state_info.transition_state_size = oracles_room_transition_save_state(s->transition, transition, sizeof transition);
    /* The compact text's line, when the state falls within its drawLineOfText call. */
    uint8_t line[ORACLES_GUEST_COMPACT_STATE_SIZE];
    s->state_info.compact_text_state = line;
    s->state_info.compact_text_state_size = oracles_guest_compact_save_state(s->guest, line, sizeof line);
    /* The host's lines of the text box, which the game's memory does not hold; empty without the host's text box. */
    static uint8_t text_box[ORACLES_GUEST_TEXT_BOX_STATE_SIZE];
    s->state_info.text_box_state = text_box;
    s->state_info.text_box_state_size = oracles_guest_text_box_save_state(s->guest, text_box, sizeof text_box);
    int ok = 0;
    if (oracles_state_serialize(s->core, &s->state_info, &data, &size, error, sizeof error) != 0) {
        fprintf(stderr, "oracles: %s\n", error);
        snprintf(message, capacity, "The state could not be saved: %s", error);
    } else if (!oracles_session_write_file(s->state_path, data, size)) {
        fprintf(stderr, "oracles: cannot write %s\n", s->state_path);
        snprintf(message, capacity, "The savestate could not be written");
    } else {
        fprintf(stderr, "oracles: state saved to %s\n", s->state_path);
        ok = 1;
    }
    s->state_info.mods_storage = NULL;
    s->state_info.mods_storage_size = 0;
    s->state_info.transition_state = NULL;
    s->state_info.transition_state_size = 0;
    s->state_info.compact_text_state = NULL;
    s->state_info.compact_text_state_size = 0;
    s->state_info.text_box_state = NULL;
    s->state_info.text_box_state_size = 0;
    free(storage);
    free(data);
    return ok;
}

/* What a game the home screen starts again with `settings` would play with, as start() in home_games.c starts it
 * (oracles_session_display_options, the command line's options winning), each text and the drawn-back view where the
 * game's profile has them, as check_profile leaves them. */
static void play_of(void *opaque, const oracles_settings *settings, OraclesStatePlay *out)
{
    const session *s = opaque;
    OraclesSessionOptions o = *s->options->command_line;
    OraclesEnhancedSize surface;
    const OraclesProfile profile = oracles_session_display_options(&o, settings, s->options->enhanced_allowed, s->options->sdl.window, &surface);
    memset(out, 0, sizeof *out);
    out->core = o.core ? o.core - 1 : oracles_settings_core(settings);
    out->enhanced = profile == ORACLES_PROFILE_ENHANCED;
    if (!out->enhanced) return;
    out->transitions = o.continuous_transitions || o.continuous_swim || o.continuous_diagonal || o.continuous_steps || o.continuous_line;
    out->compact = o.compact_text == 2 && oracles_compat_compact_text(s->profile);
    out->wide = o.text_box_wide == 2 && oracles_compat_host_text_box(s->profile);
    const int level = o.view ? o.view - 1 : o.zoom_out ? ORACLES_ENHANCED_FAR : ORACLES_ENHANCED_NEAR;
    if (level != ORACLES_ENHANCED_NEAR && !oracles_compat_zoom_out(s->profile)) surface = oracles_session_view_size(&o, ORACLES_ENHANCED_NEAR);
    out->width = surface.width;
    out->height = surface.height;
}

/* A savestate refused here, in a game the home screen started: the choices of Display nearest to the player's that
 * load it, when a game started again with them would load it whole, its identity besides Display (the game, the ROM,
 * the core's very version, the mods, the language and the text's layout) the session's.  1 when so; then, with `apply`,
 * the choices stored, those before kept for the game started again (s->state_previous), and the session asked to end
 * for it.  Choices that already load it start the game again unchanged, once: never in a game that was started again. */
static int restart_for(session *s, const uint8_t *data, size_t size, const char *taken_on, const char *saved_mods, int apply)
{
    if (!s->options || !s->options->command_line || !taken_on[0]) return 0;
    char game[16], rom_sha1[48];
    if (oracles_state_game(data, size, game, sizeof game, rom_sha1, sizeof rom_sha1) != 0 || strcmp(game, s->state_info.game)
        || strcmp(rom_sha1, s->state_info.rom_sha1)) return 0;
    OraclesStatePlay taken = { 0 };
    taken.core = !strncmp(taken_on, "mgba-", 5) ? ORACLES_CORE_MGBA : ORACLES_CORE_SAMEBOY;
    taken.transitions = has_transitions(saved_mods);
    oracles_state_text_taken(saved_mods, &taken.compact, &taken.wide);
    const uint8_t *host = NULL;
    size_t host_size = 0;
    if (oracles_state_host_state(data, size, &host, &host_size) == 0 && host_size) {
        const OraclesEnhancedSize surface = oracles_enhanced_view_state_surface(host, host_size);
        taken.width = surface.width;
        taken.height = surface.height;
    }
    oracles_settings *chosen = malloc(sizeof *chosen);
    if (!chosen) return 0;
    memcpy(chosen, s->settings, sizeof *chosen);
    char notice[sizeof s->state_notice];
    const int changes = oracles_state_display_settings(&taken, chosen, play_of, s, notice, sizeof notice);
    OraclesStatePlay play;
    if (changes >= 0) play_of(s, chosen, &play);
    /* What the game started again lays its text out with, by its word. */
    char word[80] = "";
    OraclesLanguageLayout layout = { 0u, 0, 0u, NULL };
    if (changes >= 0 && oracles_session_text_layout(oracles_compat_compact_text(s->profile), oracles_compat_host_text_box(s->profile), play.enhanced,
                                                    play.compact, play.wide, &layout.width, &layout.fixed))
        oracles_language_layout_word(&layout, s->info.revision, word, sizeof word);
    const int loads = changes > 0 || (changes == 0 && !s->options->load_state);
    if (!loads || strcmp(taken_on, oracles_core_kind_version((OraclesCoreKind)play.core)) || !oracles_state_display_only(saved_mods, s->state_info.mods, word)) {
        free(chosen);
        return 0;
    }
    if (apply) {
        oracles_settings *settings = s->settings;
        const int view = settings->view, core = oracles_settings_core(settings);
        OraclesStateDisplay display;
        oracles_state_display_read(settings, &s->state_previous);
        oracles_state_display_read(chosen, &display);
        oracles_state_display_write(settings, &display);
        oracles_settings_quality_changed(settings, view, core, settings->ghosts);
        oracles_settings_store(settings);
        if (changes) snprintf(s->state_notice, sizeof s->state_notice, "%s", notice);
        else snprintf(s->state_notice, sizeof s->state_notice, "The game started again with Display's settings to load this savestate");
        fprintf(stderr, "oracles: %s: the game starts again\n", s->state_notice);
        s->state_restart = 1;
    }
    free(chosen);
    return 1;
}

/* A savestate refused here: with `restart`, the pause's, the game asked for again with the choices of Display that load
 * it (1); else, where they would, *message pointing to the pause's Load state instead of the home screen (0). */
static int refused_for_display(session *s, const uint8_t *data, size_t size, const char *taken_on, const char *saved_mods, int restart,
                               char *message, size_t capacity)
{
    if (restart) return restart_for(s, data, size, taken_on, saved_mods, 1);
    if (s->pause && !s->load_at_start && restart_for(s, data, size, taken_on, saved_mods, 0))
        snprintf(message, capacity, "Refused here: Load state in the menu starts the game again with the Display settings it needs");
    return 0;
}

/* F7: the savestate loaded, as save_state says; `restart`, the pause's, a savestate refused for Display's settings
 * asks for the game again with them (restart_for). */
static int load_state(session *s, char *message, size_t capacity, int restart)
{
    char error[320];   /* the refusal of another surface names two and a setting */
    uint8_t *data = NULL, *host_state = NULL;
    size_t size = 0, host_state_size = 0;
    if (!oracles_session_read_file(s->state_path, &data, &size)) {
        fprintf(stderr, "oracles: no savestate file %s\n", s->state_path);
        snprintf(message, capacity, "No savestate yet");
        return 0;
    }
    /* A state taken on the other core is refused first, by the core's name: the core cannot read the other's state,
     * and the player changes cores in Display (or with --core), not in a game.  Another version of the same core is
     * refused by the load itself (oracles_state_deserialize). */
    char taken_on[64] = "", core_detail[320];
    char saved_mods[1100] = "", detail[320];
    const int mods_read = oracles_state_mods(data, size, saved_mods, sizeof saved_mods) == 0;
    if (oracles_state_core_version(data, size, taken_on, sizeof taken_on) != 0) taken_on[0] = 0;
    if (taken_on[0] && oracles_state_core_refusal(taken_on, oracles_core_version(s->core), message, capacity, core_detail, sizeof core_detail)) {
        fprintf(stderr, "oracles: %s\n", core_detail);
        const int restarting = refused_for_display(s, data, size, taken_on, saved_mods, restart, message, capacity);
        free(data);
        return restarting;
    }
    /* A state taken with the transitions and loaded without them, or the other way round, with the other text, or
     * taken on the other surface, is refused whole before the core is touched, with the options (and the launcher's
     * choices) that make each: a player of the launcher and one of the command line meet the same refusals. */
    int refused = 0;
    if (mods_read
        && oracles_state_transitions_refusal(has_transitions(saved_mods), s->enhanced, s->continuous_transitions, message, capacity, detail, sizeof detail)) {
        fprintf(stderr, "oracles: %s\n", detail);
        refused = 1;
    }
    if (!refused && mods_read
        && oracles_state_text_refusal(saved_mods, s->state_info.mods, s->enhanced, s->settings && s->settings->compact_text,
                                      s->settings && s->settings->text_box_wide && oracles_compat_host_text_box(s->profile), message, capacity, detail,
                                      sizeof detail)) {
        fprintf(stderr, "oracles: %s\n", detail);
        refused = 1;
    }
    const uint8_t *peek = NULL;
    size_t peek_size = 0;
    if (s->view && oracles_state_host_state(data, size, &peek, &peek_size) == 0 && peek_size
        && oracles_enhanced_view_check_state(s->view, peek, peek_size, error, sizeof error) != 0) {
        fprintf(stderr, "oracles: %s\n", error);
        if (!refused) oracles_enhanced_view_state_message(s->view, peek, peek_size, &s->far_choice, message, capacity);
        refused = 1;
    }
    if (refused) {
        const int restarting = refused_for_display(s, data, size, taken_on, saved_mods, restart, message, capacity);
        free(data);
        return restarting;
    }
    int ok = 0;
    if (oracles_state_deserialize(s->core, &s->state_info, data, size, &host_state, &host_state_size, error, sizeof error) == 0) {
        fprintf(stderr, "oracles: state loaded from %s\n", s->state_path);
        if (s->guest) oracles_guest_reset_execution_state(s->guest);
        const uint8_t *transition = NULL;
        size_t transition_size = 0;
        if (s->transition && oracles_state_transition_state(data, size, &transition, &transition_size) == 0 && transition_size
            && oracles_room_transition_load_state(s->transition, transition, transition_size) != 0)
            fprintf(stderr, "oracles: the savestate's transitions' state is refused: a transition it was saved in is left to the game's scroll\n");
        const uint8_t *line = NULL;
        size_t line_size = 0;
        if (s->guest && oracles_state_compact_text_state(data, size, &line, &line_size) == 0 && line_size
            && oracles_guest_compact_load_state(s->guest, line, line_size) != 0)
            fprintf(stderr, "oracles: the savestate's compact text's line is refused\n");
        const uint8_t *text_box = NULL;
        size_t text_box_size = 0;
        if (oracles_guest_text_box_armed(s->guest) && oracles_state_text_box_state(data, size, &text_box, &text_box_size) == 0 && text_box_size
            && oracles_guest_text_box_load_state(s->guest, text_box, text_box_size) != 0)
            fprintf(stderr, "oracles: the savestate's text box state is refused: the host's lines wait for the game's next ones\n");
        oracles_mod_session_reset(s->mod);   /* no conversation survives a load */
        const uint8_t *storage = NULL;
        size_t storage_size = 0;
        if (s->mod && oracles_state_mods_storage(data, size, &storage, &storage_size) == 0 && storage_size
            && oracles_mod_session_storage_restore(s->mod, storage, storage_size, 1, error, sizeof error) != 0)
            fprintf(stderr, "oracles: the savestate's mods' storage is refused (%s); the mods keep theirs\n", error);
        if (s->check) oracles_frame_check_reset(s->check);
        /* A state load is a jump the route format cannot express: the route ends here. */
        if (s->playing) { s->playing = 0; fprintf(stderr, "oracles: route replay stopped by the state load\n"); }
        oracles_session_stop_recording(s, "stopped by the state load");
        oracles_hotkeys_session_route_over(s->hotkeys);   /* the guest's reset has stopped the route's exchanges and emptied the policy */
        ok = 1;
    } else {
        fprintf(stderr, "oracles: %s\n", error);
        /* On the screen, what differs without the values (a digest of forty digits), which stderr has. */
        const char *values = strstr(error, " (");
        snprintf(message, capacity, "Refused: %.*s", values ? (int)(values - error) : (int)strlen(error), error);
    }
    if (host_state && s->view && oracles_enhanced_view_load_state(s->view, host_state, host_state_size) != 0)
        fprintf(stderr, "oracles: the savestate carries no usable Enhanced camera state; the camera restarts\n");
    free(host_state); /* Faithful has no native state to restore */
    free(data);
    return ok;
}

int oracles_session_load_state(session *s, char *message, size_t capacity) { return load_state(s, message, capacity, 0); }

int oracles_session_load_state_or_restart(session *s, char *message, size_t capacity) { return load_state(s, message, capacity, 1); }
