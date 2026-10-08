/* The Moonrise profile's cache: its key read through its own tables, and its loaded tileset's identity; no game ROM or emulation is exercised. */
#include "view_internal.h"

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

static void check_loaded_tileset_identity(OraclesEnhancedView *view)
{
    const OraclesRomInfo identity = { .game = ORACLES_GAME_AGES,
        .revision = ORACLES_ROM_REVISION_MOONRISE_1_0_6, .known = 1 };
    view->guest = oracles_guest_attach(view->core, oracles_compat_find(&identity));
    CHECK(view->guest != NULL);
    if (!view->guest) return;
    const OraclesGuestTables *t = oracles_guest_tables(view->guest);
    CHECK(t->tileset_gfx.bank == t->loaded_tileset_unique_gfx.bank);
    CHECK(t->tileset_gfx.addr == t->loaded_tileset_unique_gfx.addr);
    uint8_t *wram = oracles_guest_wram_writable(view->guest, 0);
    CHECK(wram != NULL);
    if (wram) {
        /* Only the loaded expanded identity changes; all other state,
         * including the obsolete graphics header, remains identical. */
        uint8_t *loaded = wram + t->loaded_tileset_unique_gfx.addr - 0xc000u;
        *loaded = 2;
        ev_track_animated_tiles(view);
        CHECK(view->animated_tileset_gfx == 2);
        view->animated[0] = 1;
        *loaded = 3;
        ev_track_animated_tiles(view);
        CHECK(view->animated_tileset_gfx == 3);
        CHECK(view->animated[0] == 0);
    }
    oracles_guest_detach(view->guest);
}

extern const OraclesGuestTables oracles_guest_tables_moonrise;

static int key_has(const OraclesGhostKeyRange *r, unsigned n, uint16_t address)
{
    for (unsigned i = 0; i < n; i++) if (address >= r[i].address && address < r[i].address + r[i].length) return 1;
    return 0;
}

/* Moonrise keys its neighbours as the originals do, through its own tables:
 * an address moved in the Moonrise table moves in its key, whatever Ages has. */
static void check_moonrise_traced_key(void)
{
    const OraclesRomInfo identity = { .game = ORACLES_GAME_AGES,
        .revision = ORACLES_ROM_REVISION_MOONRISE_1_0_6, .known = 1 };
    const OraclesCompatProfile *moonrise = oracles_compat_find(&identity);
    CHECK(moonrise != NULL && oracles_compat_guest_tables(moonrise) == &oracles_guest_tables_moonrise);
    if (!moonrise) return;
    OraclesGuestTables moved = oracles_guest_tables_moonrise;
    const uint16_t before = moved.toggle_blocks_state.addr, elsewhere = 0xd3f0u;   /* in no range of the key */
    moved.toggle_blocks_state.addr = elsewhere;
    OraclesCompatProfile profile = *moonrise;
    profile.guest_tables = &moved;
    OraclesGhostKeyRange own[ORACLES_GHOST_KEY_RANGES], shifted[ORACLES_GHOST_KEY_RANGES];
    const unsigned n = oracles_ghost_key_ranges(moonrise, own, ORACLES_GHOST_KEY_RANGES);
    const unsigned m = oracles_ghost_key_ranges(&profile, shifted, ORACLES_GHOST_KEY_RANGES);
    CHECK(n > 0 && key_has(own, n, before) && !key_has(own, n, elsewhere));
    CHECK(m == n && key_has(shifted, m, elsewhere) && !key_has(shifted, m, before));
}

int main(void)
{
    check_moonrise_traced_key();
    const OraclesRomInfo identity = { .game = ORACLES_GAME_AGES,
        .revision = ORACLES_ROM_REVISION_AGES_US, .known = 1 };
    const OraclesCompatProfile *profile = oracles_compat_find(&identity);

    const size_t size = 1024u * 1024u;
    uint8_t *rom = calloc(size, 1);
    OraclesEnhancedView *view = calloc(1, sizeof *view);
    entry *slots = calloc(NORMAL_SLOTS, sizeof *slots);
    if (!rom || !view || !slots) { free(rom); free(view); free(slots); return 1; }
    view->slots = slots;
    view->slot_count = NORMAL_SLOTS;
    rom[0x143] = 0xc0; rom[0x147] = 0x1b; rom[0x148] = 0x05; rom[0x149] = 0x02;
    const OraclesCoreOptions options = { 0, 0, ORACLES_CORE_SAMEBOY, 0 };
    view->core = oracles_core_create(rom, size, &options);
    view->guest = view->core ? oracles_guest_attach(view->core, profile) : NULL;
    CHECK(view->guest != NULL);
    if (view->guest) {
        oracles_guest_detach(view->guest);
        check_loaded_tileset_identity(view);
    }
    oracles_core_destroy(view->core);
    free(view);
    free(slots);
    free(rom);
    if (failures) return 1;
    puts("test_cache_key: ok");
    return 0;
}
