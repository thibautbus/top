/* The session's files and its savestates (F5, F7): split from session.c,
 * whose state they share through session_internal.h. */
#include "session_internal.h"

#include "state.h"
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
    free(storage);
    free(data);
    return ok;
}

/* F7: the savestate loaded, as save_state says. */
int oracles_session_load_state(session *s, char *message, size_t capacity)
{
    char error[256];
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
    char taken_on[64], core_detail[320];
    if (oracles_state_core_version(data, size, taken_on, sizeof taken_on) == 0
        && oracles_state_core_refusal(taken_on, oracles_core_version(s->core), message, capacity, core_detail, sizeof core_detail)) {
        fprintf(stderr, "oracles: %s\n", core_detail);
        free(data);
        return 0;
    }
    /* A state taken with the transitions and loaded without them, or the other way round, or taken on the other
     * surface, is refused whole before the core is touched, with the options (and the launcher's choices) that
     * make each: a player of the launcher and one of the command line meet the same refusals. */
    int refused = 0;
    char saved_mods[1100], detail[320];
    if (oracles_state_mods(data, size, saved_mods, sizeof saved_mods) == 0
        && oracles_state_transitions_refusal(has_transitions(saved_mods), s->enhanced, s->continuous_transitions, message, capacity, detail, sizeof detail)) {
        fprintf(stderr, "oracles: %s\n", detail);
        refused = 1;
    }
    const uint8_t *peek = NULL;
    size_t peek_size = 0;
    if (s->view && oracles_state_host_state(data, size, &peek, &peek_size) == 0 && peek_size
        && oracles_enhanced_view_check_state(s->view, peek, peek_size, error, sizeof error) != 0) {
        fprintf(stderr, "oracles: %s\n", error);
        if (!refused) snprintf(message, capacity, "Refused: the savestate was taken on another surface");
        refused = 1;
    }
    if (refused) { free(data); return 0; }
    int ok = 0;
    if (oracles_state_deserialize(s->core, &s->state_info, data, size, &host_state, &host_state_size, error, sizeof error) == 0) {
        fprintf(stderr, "oracles: state loaded from %s\n", s->state_path);
        if (s->guest) oracles_guest_reset_execution_state(s->guest);
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
        snprintf(message, capacity, "Refused: %s", error);
    }
    if (host_state && s->view && oracles_enhanced_view_load_state(s->view, host_state, host_state_size) != 0)
        fprintf(stderr, "oracles: the savestate carries no usable Enhanced camera state; the camera restarts\n");
    free(host_state); /* Faithful has no native state to restore */
    free(data);
    return ok;
}
