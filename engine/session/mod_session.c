/* The mods of a run of the host (mod_session.h). */
#include "mod_session.h"

#include "mod_folder.h"
#include "sha1.h"

#include <stdlib.h>
#include <string.h>

struct OraclesModSession {
    OraclesModSet *set;
    OraclesCore *core;
    const uint32_t *(*inner_source)(void *opaque);   /* the frame source the mod draws over, NULL for the core's screen */
    void *inner_opaque;
    uint32_t width, height;
    FILE *trace;
    char storage_path[4200];                        /* where mod.storage is written, empty when it is not (a replay) */
    unsigned storage_written;                       /* the set's count of changes the file holds */
};

static char *read_whole(const char *path, size_t *size)
{
    FILE *in = fopen(path, "rb");
    if (!in) return NULL;
    char *text = NULL;
    if (fseek(in, 0, SEEK_END) == 0) {
        const long length = ftell(in);
        if (length >= 0 && length <= 1024L * 1024L && fseek(in, 0, SEEK_SET) == 0 && (text = malloc((size_t)length + 1u)) != NULL) {
            if (fread(text, 1, (size_t)length, in) == (size_t)length) { text[length] = 0; *size = (size_t)length; }
            else { free(text); text = NULL; }
        }
    }
    fclose(in);
    return text;
}

OraclesModSession *oracles_mod_session_start(const char *const *dirs, size_t count, OraclesGame game, const uint8_t *rom, size_t rom_size,
                                             char *error, size_t capacity)
{
    OraclesModSession *session = calloc(1, sizeof *session);
    OraclesMod *mods[ORACLES_MOD_SET_MAX];
    size_t loaded = 0;
    if (!session) { snprintf(error, capacity, "out of memory"); return NULL; }
    if (count > ORACLES_MOD_SET_MAX) { snprintf(error, capacity, "at most %u mods at once", ORACLES_MOD_SET_MAX); free(session); return NULL; }
    for (; loaded < count; loaded++) {
        mods[loaded] = oracles_mod_folder_load(dirs[loaded], game, rom, rom_size, error, capacity);
        if (!mods[loaded]) break;
        fprintf(stderr, "oracles: mod %s loaded from %s\n", oracles_mod_identity(mods[loaded]), dirs[loaded]);
        if (!oracles_mod_npc_count(mods[loaded])) fprintf(stderr, "oracles: mod %s declares no NPC for %s\n", oracles_mod_identity(mods[loaded]), game == ORACLES_GAME_AGES ? "Ages" : "Seasons");
    }
    if (loaded < count) {
        for (size_t i = 0; i < loaded; i++) oracles_mod_free(mods[i]);
        free(session);
        return NULL;
    }
    session->set = oracles_mod_set_create(mods, count, error, capacity);
    if (!session->set) { free(session); return NULL; }
    return session;
}

int oracles_mod_session_compose(OraclesModSession *session, int original, uint8_t **rom, size_t *rom_size, char *error, size_t capacity)
{
    uint8_t *image = NULL;
    size_t size = 0;
    if (!original && oracles_mod_set_house_count(session->set)) {
        snprintf(error, capacity, "mods %s add a house, which needs the US ROM of the game as the port recognises it; this ROM is not one",
                 oracles_mod_set_identity(session->set));
        return -1;
    }
    if (oracles_mod_set_compose(session->set, *rom, *rom_size, &image, &size, error, capacity) != 0) return -1;
    if (!image) return 0;
    free(*rom);
    *rom = image;
    *rom_size = size;
    return 0;
}

int oracles_mod_session_attach(OraclesModSession *session, OraclesGuest *guest, char *error, size_t capacity)
{
    return oracles_mod_set_attach(session->set, guest, error, capacity);
}

const char *oracles_mod_session_identity(const OraclesModSession *session)
{
    return session ? oracles_mod_set_identity(session->set) : "";
}

int oracles_mod_session_matches_route(const OraclesModSession *session, const char *route_mods, char *error, size_t capacity)
{
    const char *mine = oracles_mod_session_identity(session);
    if (strcmp(mine, route_mods) == 0) return 1;
    if (!route_mods[0]) snprintf(error, capacity, "the route was recorded without mods; replay it without --mods");
    else if (!mine[0]) snprintf(error, capacity, "the route was recorded with the mods %s; replay it with --mods and those mods", route_mods);
    else snprintf(error, capacity, "the route was recorded with the mods %s, not %s: the set of mods or a mod's files changed since", route_mods, mine);
    return 0;
}

static unsigned hold(void *opaque, uint32_t frame, unsigned mask)
{
    OraclesModSession *session = opaque;
    return oracles_mod_set_frame(session->set, frame, mask);
}

static const uint32_t *present(void *opaque)
{
    OraclesModSession *session = opaque;
    const uint32_t *pixels = session->inner_source ? session->inner_source(session->inner_opaque) : oracles_core_pixels(session->core);
    return oracles_mod_set_present(session->set, pixels, session->width, session->height);
}

void oracles_mod_session_configure(OraclesModSession *session, oracles_host_run_config *config)
{
    config->input_hold = hold;
    config->input_hold_opaque = session;
    session->core = config->core;
    session->inner_source = config->frame_source;
    session->inner_opaque = config->frame_source_opaque;
    session->width = config->frame_width ? config->frame_width : ORACLES_MOD_WIDTH;
    session->height = config->frame_height ? config->frame_height : ORACLES_MOD_HEIGHT;
    config->frame_source = present;
    config->frame_source_opaque = session;
}

int oracles_mod_session_trace(OraclesModSession *session, const char *path)
{
    session->trace = fopen(path, "wb");
    return session->trace != NULL;
}

void oracles_mod_session_frame_end(OraclesModSession *session, uint32_t frame)
{
    if (session && session->trace) fprintf(session->trace, "%u\t%016llx\n", frame, (unsigned long long)oracles_mod_set_fingerprint(session->set));
}

const uint32_t *oracles_mod_session_screen(OraclesModSession *session, const uint32_t *pixels, uint32_t width, uint32_t height)
{
    return session ? oracles_mod_set_present(session->set, pixels, width, height) : pixels;
}

int oracles_mod_session_busy(const OraclesModSession *session) { return session && oracles_mod_set_busy(session->set); }
void oracles_mod_session_reset(OraclesModSession *session) { if (session) oracles_mod_set_reset(session->set); }

void oracles_mod_session_stop(OraclesModSession *session, FILE *summary)
{
    if (!session) return;
    oracles_mod_session_storage_sync(session);
    if (summary) oracles_mod_set_summary(session->set, summary);
    oracles_mod_set_summary(session->set, stderr);
    if (session->trace) fclose(session->trace);
    oracles_mod_set_free(session->set);
    free(session);
}

/* ---- mod.storage ------------------------------------------------------------------------------ */

void oracles_mod_session_storage_path(const char *save_path, char *out, size_t capacity)
{
    snprintf(out, capacity, "%s", save_path);
    const size_t length = strlen(out);
    if (length > 4 && strcmp(out + length - 4, ".sav") == 0) out[length - 4] = 0;
    const size_t stem = strlen(out);
    snprintf(out + stem, capacity - stem, ".store");
}

static int write_whole(const char *path, const uint8_t *data, size_t size)
{
    char temporary[4300];
    snprintf(temporary, sizeof temporary, "%s.tmp", path);
    FILE *f = fopen(temporary, "wb");
    if (!f) return 0;
    const int ok = fwrite(data, 1, size, f) == size && fflush(f) == 0;
    if (fclose(f) != 0 || !ok) { remove(temporary); return 0; }
    if (rename(temporary, path) == 0) return 1;
    remove(path);
    return rename(temporary, path) == 0;
}

int oracles_mod_session_storage_open(OraclesModSession *session, const char *path, int writable, char *error, size_t capacity)
{
    size_t size = 0;
    char *text = read_whole(path, &size);
    if (text) {
        const int status = oracles_mod_set_storage_load(session->set, (const uint8_t *)text, size, 0, error, capacity);
        free(text);
        if (status != 0) return -1;
        fprintf(stderr, "oracles: mods' storage read from %s\n", path);
    }
    session->storage_path[0] = 0;
    if (writable) snprintf(session->storage_path, sizeof session->storage_path, "%s", path);
    session->storage_written = oracles_mod_set_storage_changes(session->set);
    return 0;
}

int oracles_mod_session_storage_copy(OraclesModSession *session, const char *path)
{
    char error[256];
    size_t size = 0;
    uint8_t *data = oracles_mod_set_storage_save(session->set, 0, &size, error, sizeof error);
    const int ok = data && write_whole(path, data, size);
    free(data);
    return ok;
}

void oracles_mod_session_storage_sync(OraclesModSession *session)
{
    if (!session || !session->storage_path[0]) return;
    const unsigned changes = oracles_mod_set_storage_changes(session->set);
    if (changes == session->storage_written) return;
    char error[256];
    size_t size = 0;
    uint8_t *data = oracles_mod_set_storage_save(session->set, 0, &size, error, sizeof error);
    if (data && write_whole(session->storage_path, data, size)) {
        session->storage_written = changes;
        fprintf(stderr, "oracles: mods' storage written to %s\n", session->storage_path);
    } else {
        fprintf(stderr, "oracles: cannot write the mods' storage to %s%s%s\n", session->storage_path, data ? "" : ": ", data ? "" : error);
        session->storage_written = changes;   /* said once, not every frame */
    }
    free(data);
}

uint8_t *oracles_mod_session_storage_state(OraclesModSession *session, size_t *size, char *error, size_t capacity)
{
    return oracles_mod_set_storage_save(session->set, 1, size, error, capacity);
}

int oracles_mod_session_storage_restore(OraclesModSession *session, const uint8_t *data, size_t size, int state, char *error, size_t capacity)
{
    return oracles_mod_set_storage_load(session->set, data, size, state, error, capacity);
}

/* ---- --start-at-house ------------------------------------------------------------------------- */

/* The save of both games (ROM_DATA_FORMATS, section 14): three files, each in two copies; a copy is valid when its
 * checksum (the sum of the $2a7 words from +2) and its signature (Ages' or Seasons') match.  The respawn point is at +$7b (group, room, state modifier,
 * direction, y, x), the minimap's room at +$8a. */
static const unsigned file_copies[3][2] = { { 0x0010, 0x1000 }, { 0x0560, 0x1550 }, { 0x0ab0, 0x1aa0 } };
#define FILE_WORDS 0x2a7u

static unsigned file_checksum(const uint8_t *sram, unsigned at)
{
    unsigned sum = 0;
    for (unsigned i = 0; i < FILE_WORDS; i++) sum += sram[at + 2u + 2u * i] | sram[at + 3u + 2u * i] << 8;
    return sum & 0xffffu;
}

int oracles_mod_session_start_at_house(OraclesModSession *session, const char *name, OraclesCore *core, char *error, size_t capacity)
{
    uint8_t room = 0, y = 0, x = 0;
    if (oracles_mod_set_house_door(session->set, name, &room, &y, &x, error, capacity) != 0) return -1;
    const size_t size = oracles_core_sram_size(core);
    uint8_t *sram = malloc(size);
    if (!sram || size < 0x2000u || oracles_core_save_sram(core, sram, size) != 0) { free(sram); snprintf(error, capacity, "the save cannot be read"); return -1; }
    unsigned placed = 0;
    for (unsigned file = 0; file < 3u; file++)
        for (unsigned copy = 0; copy < 2u; copy++) {
            const unsigned at = file_copies[file][copy];
            if ((unsigned)(sram[at] | sram[at + 1u] << 8) != file_checksum(sram, at)
                || (memcmp(sram + at + 2u, "Z21216-0", 8) != 0 && memcmp(sram + at + 2u, "Z11216-0", 8) != 0)) continue;
            sram[at + 0x7bu] = 0x00;                  /* group 0, the room, facing up, at the door; the state (Seasons' season) kept */
            sram[at + 0x7cu] = room;
            sram[at + 0x7eu] = 0x00;
            sram[at + 0x7fu] = y;
            sram[at + 0x80u] = x;
            sram[at + 0x8au] = 0x00;
            sram[at + 0x8bu] = room;
            const unsigned sum = file_checksum(sram, at);
            sram[at] = (uint8_t)sum;
            sram[at + 1u] = (uint8_t)(sum >> 8);
            placed |= 1u << file;
        }
    const int ok = placed && oracles_core_load_sram(core, sram, size) == 0;
    free(sram);
    if (!placed) { snprintf(error, capacity, "the save has no file to start in front of %s: start a game first", name); return -1; }
    if (!ok) { snprintf(error, capacity, "the save cannot be written back"); return -1; }
    fprintf(stderr, "oracles: every file of the save starts in front of %s's door (0/%02x)\n", name, room);
    return 0;
}

void oracles_mod_session_file_sha1(const char *path, char out[41])
{
    size_t size = 0;
    char *text = read_whole(path, &size);
    if (!text) { snprintf(out, 41, "none"); return; }
    oracles_sha1_hex((const uint8_t *)text, size, out);
    free(text);
}

int oracles_mod_session_matches_store(const char *route_path, const char *expected, char *error, size_t capacity)
{
    if (!expected[0]) return 1;                    /* a route written before the line */
    char path[4200], sha1[41];
    snprintf(path, sizeof path, "%s.store", route_path);
    oracles_mod_session_file_sha1(path, sha1);
    if (!strcmp(sha1, expected)) return 1;
    snprintf(error, capacity, "the route starts from the mods' storage %s, and %s is %s: missing or changed since", expected, path, sha1);
    return 0;
}
