/* Composite savestates, on each core: round trip, determinism after a load,
 * the cartridge RAM restored, refusal of a state made for another ROM, another
 * set of mods, another core version or the other core.  No ROM: a synthetic
 * image. */
#include "core.h"
#include "state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

static uint64_t hash_pixels(const uint32_t *pixels)
{
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < ORACLES_SCREEN_PIXELS; i++) { h ^= pixels[i]; h *= 0x100000001b3ull; }
    return h;
}

static OraclesCore *make_core(OraclesCoreKind kind)
{
    const size_t size = 1024u * 1024u;
    uint8_t *rom = calloc(size, 1);
    memcpy(rom + 0x134, "ZELDA NAYRU", 11);
    rom[0x143] = 0xc0; rom[0x147] = 0x1b; rom[0x148] = 0x05; rom[0x149] = 0x02;
    /* $0100: nop ; jp $0150.  $0150: ld hl,$c000 ; inc (hl) ; jr -3 : the state changes every frame. */
    rom[0x100] = 0x00; rom[0x101] = 0xc3; rom[0x102] = 0x50; rom[0x103] = 0x01;
    rom[0x150] = 0x21; rom[0x151] = 0x00; rom[0x152] = 0xc0; rom[0x153] = 0x34; rom[0x154] = 0x18; rom[0x155] = 0xfd;
    const OraclesCoreOptions options = { 0, 0, kind, 0 };
    OraclesCore *core = oracles_core_create(rom, size, &options);
    free(rom);
    return core;
}

static void check_core(OraclesCoreKind kind)
{
    OraclesCore *core = make_core(kind);
    CHECK(core != NULL);
    if (!core) return;
    for (unsigned i = 0; i < 200; i++) oracles_core_run_frame(core);
    size_t sram_size = 0;
    uint8_t *sram = oracles_core_memory(core, ORACLES_CORE_CART_RAM, &sram_size, NULL);
    CHECK(sram != NULL && sram_size == oracles_core_sram_size(core) && sram_size >= 0x2000u);
    if (sram) sram[0x123] = 0x5a;

    const OraclesStateInfo info = { "ages", "0123456789abcdef0123456789abcdef01234567", "", NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0 };
    char error[256];
    uint8_t *state = NULL;
    size_t state_size = 0;
    CHECK(oracles_state_serialize(core, &info, &state, &state_size, error, sizeof error) == 0);
    CHECK(state != NULL && state_size > 64);

    /* Continue, then load: the next frames must equal the ones after the save. */
    uint64_t after_save[10];
    for (unsigned i = 0; i < 10; i++) { oracles_core_run_frame(core); after_save[i] = hash_pixels(oracles_core_pixels(core)); }
    for (unsigned i = 0; i < 300; i++) oracles_core_run_frame(core);
    if (sram) sram[0x123] = 0xa5;   /* the cartridge RAM written since: the state brings the saved byte back */
    uint8_t *host = NULL;
    size_t host_size = 0;
    CHECK(oracles_state_deserialize(core, &info, state, state_size, &host, &host_size, error, sizeof error) == 0);
    CHECK(host == NULL && host_size == 0);
    int same = 1;
    for (unsigned i = 0; i < 10; i++) { oracles_core_run_frame(core); if (hash_pixels(oracles_core_pixels(core)) != after_save[i]) same = 0; }
    CHECK(same);
    CHECK(sram && sram[0x123] == 0x5a);

    /* The other core refuses the state, by its version. */
    {
        OraclesCore *other = make_core(kind == ORACLES_CORE_SAMEBOY ? ORACLES_CORE_MGBA : ORACLES_CORE_SAMEBOY);
        CHECK(other != NULL);
        if (other) {
            CHECK(oracles_state_deserialize(other, &info, state, state_size, &host, &host_size, error, sizeof error) == -1);
            CHECK(strstr(error, "core version") != NULL);
            oracles_core_destroy(other);
        }
    }

    /* Another ROM, another set of mods: refused with a message that says so. */
    const OraclesStateInfo other_rom = { "ages", "ffffffffffffffffffffffffffffffffffffffff", "", NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0 };
    CHECK(oracles_state_deserialize(core, &other_rom, state, state_size, &host, &host_size, error, sizeof error) == -1);
    CHECK(strstr(error, "ROM") != NULL);
    const OraclesStateInfo other_mods = { "ages", "0123456789abcdef0123456789abcdef01234567", "demo-mod", NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0 };
    CHECK(oracles_state_deserialize(core, &other_mods, state, state_size, &host, &host_size, error, sizeof error) == -1);
    CHECK(strstr(error, "mods") != NULL);
    /* The set a state was made with reads without loading it; an empty set is said as none. */
    CHECK(!strcmp(error, "the savestate was made with another set of mods and gameplay options (none, now demo-mod)"));
    char mods[32];
    CHECK(oracles_state_mods(state, state_size, mods, sizeof mods) == 0 && mods[0] == 0);
    /* The core it was taken on, read without loading it: the launcher refuses another core's state by its name. */
    char version[64];
    CHECK(oracles_state_core_version(state, state_size, version, sizeof version) == 0 && !strcmp(version, oracles_core_version(core)));
    CHECK(oracles_state_core_version(state, state_size, version, 4) == -1);   /* longer than the room given */
    CHECK(oracles_state_core_version((const uint8_t *)"not a state", 11, version, sizeof version) == -1);
    {
        const OraclesStateInfo with_mod = { "ages", "0123456789abcdef0123456789abcdef01234567", "demo-mod", NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0 };
        uint8_t *modded = NULL;
        size_t modded_size = 0;
        CHECK(oracles_state_serialize(core, &with_mod, &modded, &modded_size, error, sizeof error) == 0);
        CHECK(oracles_state_mods(modded, modded_size, mods, sizeof mods) == 0 && !strcmp(mods, "demo-mod"));
        CHECK(oracles_state_mods(modded, modded_size, mods, 4) == -1);   /* longer than the room given */
        CHECK(oracles_state_mods((const uint8_t *)"not a state", 11, mods, sizeof mods) == -1);
        free(modded);
    }

    /* Another core version: the version field follows the 9-byte magic and the u32 format. */
    {
        uint8_t *tampered = malloc(state_size);
        memcpy(tampered, state, state_size);
        const size_t version_length = (size_t)tampered[13] | ((size_t)tampered[14] << 8);
        CHECK(version_length == strlen(oracles_core_version(core)));
        tampered[17 + version_length - 1] ^= 0x01; /* last character of the version */
        CHECK(oracles_state_deserialize(core, &info, tampered, state_size, &host, &host_size, error, sizeof error) == -1);
        CHECK(strstr(error, "core version") != NULL);
        free(tampered);
    }
    free(state);

    /* A host state travels with the buffer. */
    const uint8_t blob[5] = { 1, 2, 3, 4, 5 };
    const OraclesStateInfo with_host = { "ages", "0123456789abcdef0123456789abcdef01234567", "", blob, sizeof blob, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0 };
    CHECK(oracles_state_serialize(core, &with_host, &state, &state_size, error, sizeof error) == 0);
    CHECK(oracles_state_deserialize(core, &info, state, state_size, &host, &host_size, error, sizeof error) == 0);
    CHECK(host_size == 5 && host && memcmp(host, blob, 5) == 0);
    free(host);
    /* ... and is read in place without loading anything (the launcher judges it before the core). */
    const uint8_t *peek = NULL;
    size_t peek_size = 0;
    CHECK(oracles_state_host_state(state, state_size, &peek, &peek_size) == 0 && peek_size == 5 && memcmp(peek, blob, 5) == 0);
    CHECK(oracles_state_host_state(state, state_size - 21, &peek, &peek_size) == -1);   /* cut into the host state (the mods' storage, the transitions' state, the compact text's line, the text box's state and the view's scroll, 4 bytes each, follow) */
    CHECK(oracles_state_mods_storage(state, state_size, &peek, &peek_size) == 0 && peek == NULL && peek_size == 0);
    CHECK(oracles_state_mods_storage(state, state_size - 20, &peek, &peek_size) == 0 && peek == NULL);   /* a state from before the field */
    CHECK(oracles_state_transition_state(state, state_size, &peek, &peek_size) == 0 && peek == NULL && peek_size == 0);
    CHECK(oracles_state_transition_state(state, state_size - 16, &peek, &peek_size) == 0 && peek == NULL);   /* from before that one */
    CHECK(oracles_state_compact_text_state(state, state_size, &peek, &peek_size) == 0 && peek == NULL && peek_size == 0);
    CHECK(oracles_state_compact_text_state(state, state_size - 12, &peek, &peek_size) == 0 && peek == NULL);   /* from before that one */
    CHECK(oracles_state_text_box_state(state, state_size, &peek, &peek_size) == 0 && peek == NULL && peek_size == 0);
    CHECK(oracles_state_text_box_state(state, state_size - 8, &peek, &peek_size) == 0 && peek == NULL);   /* from before the text box's */
    CHECK(oracles_state_view_scroll_state(state, state_size, &peek, &peek_size) == 0 && peek == NULL && peek_size == 0);
    CHECK(oracles_state_view_scroll_state(state, state_size - 4, &peek, &peek_size) == 0 && peek == NULL);   /* from before the view's scroll */
    CHECK(oracles_state_view_scroll_state((const uint8_t *)"not a state", 11, &peek, &peek_size) == -1);
    CHECK(oracles_state_host_state((const uint8_t *)"not a state", 11, &peek, &peek_size) == -1);

    /* The mods' storage travels with the buffer too. */
    const uint8_t storage[] = "oracles-mod-storage 1\n";
    const OraclesStateInfo with_storage = { "ages", "0123456789abcdef0123456789abcdef01234567", "", NULL, 0, storage, sizeof storage - 1, NULL, 0, NULL, 0, NULL, 0, NULL, 0 };
    uint8_t *stored = NULL;
    size_t stored_size = 0;
    CHECK(oracles_state_serialize(core, &with_storage, &stored, &stored_size, error, sizeof error) == 0);
    CHECK(oracles_state_mods_storage(stored, stored_size, &peek, &peek_size) == 0 && peek_size == sizeof storage - 1 && memcmp(peek, storage, peek_size) == 0);
    CHECK(oracles_state_mods_storage(stored, stored_size - 12, &peek, &peek_size) == 0 && peek_size == sizeof storage - 1);   /* a state from before the transitions' */
    free(stored);

    /* The continuous transitions' progress too, the state loading as before with it. */
    const uint8_t progress[] = { 1, 2, 3, 4, 5, 6, 7 };
    const OraclesStateInfo with_progress = { "ages", "0123456789abcdef0123456789abcdef01234567", "", NULL, 0, NULL, 0, progress, sizeof progress, NULL, 0, NULL, 0, NULL, 0 };
    CHECK(oracles_state_serialize(core, &with_progress, &stored, &stored_size, error, sizeof error) == 0);
    CHECK(oracles_state_transition_state(stored, stored_size, &peek, &peek_size) == 0 && peek_size == sizeof progress && memcmp(peek, progress, peek_size) == 0);
    CHECK(oracles_state_mods_storage(stored, stored_size, &peek, &peek_size) == 0 && peek == NULL);
    CHECK(oracles_state_deserialize(core, &info, stored, stored_size, &host, &host_size, error, sizeof error) == 0 && host == NULL);
    CHECK(oracles_state_text_box_state(stored, stored_size, &peek, &peek_size) == 0 && peek == NULL);
    free(stored);

    /* The host's lines of the text box too. */
    const uint8_t lines[] = { 1, 16, 0, 0, 9 };
    OraclesStateInfo with_lines = info;
    with_lines.text_box_state = lines;
    with_lines.text_box_state_size = sizeof lines;
    CHECK(oracles_state_serialize(core, &with_lines, &stored, &stored_size, error, sizeof error) == 0);
    CHECK(oracles_state_text_box_state(stored, stored_size, &peek, &peek_size) == 0 && peek_size == sizeof lines && memcmp(peek, lines, peek_size) == 0);
    CHECK(oracles_state_transition_state(stored, stored_size, &peek, &peek_size) == 0 && peek == NULL);
    CHECK(oracles_state_deserialize(core, &info, stored, stored_size, &host, &host_size, error, sizeof error) == 0 && host == NULL);
    free(stored);

    /* The compact text's line too, after the transitions' progress. */
    const uint8_t line[] = { 1, 0, 0, 0x40, 0x12, 0xf0, 0xdf };
    const OraclesStateInfo with_line = { "ages", "0123456789abcdef0123456789abcdef01234567", "", NULL, 0, NULL, 0, progress, sizeof progress, line, sizeof line, NULL, 0, NULL, 0 };
    CHECK(oracles_state_serialize(core, &with_line, &stored, &stored_size, error, sizeof error) == 0);
    CHECK(oracles_state_compact_text_state(stored, stored_size, &peek, &peek_size) == 0 && peek_size == sizeof line && memcmp(peek, line, peek_size) == 0);
    CHECK(oracles_state_transition_state(stored, stored_size, &peek, &peek_size) == 0 && peek_size == sizeof progress);
    CHECK(oracles_state_deserialize(core, &info, stored, stored_size, &host, &host_size, error, sizeof error) == 0 && host == NULL);
    free(stored);

    /* The view's record of a scroll, the last field: a build that reads the fields before it by their index, as one
     * written before it does, loads the state whole and finds every other field where it was. */
    const uint8_t scroll[] = { 'S', 'C', 'R', 'L', 1, 0, 0, 0 };
    OraclesStateInfo with_scroll = with_lines;
    with_scroll.host_state = blob;
    with_scroll.host_state_size = sizeof blob;
    with_scroll.view_scroll_state = scroll;
    with_scroll.view_scroll_state_size = sizeof scroll;
    uint8_t *without = NULL;
    size_t without_size = 0;
    OraclesStateInfo before_scroll = with_scroll;
    before_scroll.view_scroll_state = NULL;
    before_scroll.view_scroll_state_size = 0;
    CHECK(oracles_state_serialize(core, &with_scroll, &stored, &stored_size, error, sizeof error) == 0);
    CHECK(oracles_state_serialize(core, &before_scroll, &without, &without_size, error, sizeof error) == 0);
    CHECK(without && stored_size == without_size + sizeof scroll && !memcmp(stored, without, without_size - 4u));   /* up to its length, the bytes of a state written before the field */
    CHECK(oracles_state_view_scroll_state(stored, stored_size, &peek, &peek_size) == 0 && peek_size == sizeof scroll && !memcmp(peek, scroll, sizeof scroll));
    CHECK(oracles_state_host_state(stored, stored_size, &peek, &peek_size) == 0 && peek_size == sizeof blob && !memcmp(peek, blob, sizeof blob));
    CHECK(oracles_state_text_box_state(stored, stored_size, &peek, &peek_size) == 0 && peek_size == sizeof lines && !memcmp(peek, lines, sizeof lines));
    CHECK(oracles_state_deserialize(core, &info, stored, stored_size, &host, &host_size, error, sizeof error) == 0 && host_size == sizeof blob && host && !memcmp(host, blob, sizeof blob));
    free(host);
    free(stored);
    free(without);

    /* Truncated and foreign buffers are refused. */
    CHECK(oracles_state_deserialize(core, &info, state, state_size / 2, &host, &host_size, error, sizeof error) == -1);
    CHECK(oracles_state_deserialize(core, &info, (const uint8_t *)"not a state", 11, &host, &host_size, error, sizeof error) == -1);
    free(state);

    oracles_core_destroy(core);
}

int main(void)
{
    check_core(ORACLES_CORE_SAMEBOY);
    check_core(ORACLES_CORE_MGBA);
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_state: ok\n");
    return 0;
}
