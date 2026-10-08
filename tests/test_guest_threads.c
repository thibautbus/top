/* Return hooks across the game's threads, on a synthetic ROM (no game data).
 * The bus keeps one list of pending returns for every stack: a hooked
 * function that yields leaves its frame under those another thread pushes.
 *   $0150: ld sp,$c1f0      thread 1's stack (Ages: $c180-$c220)
 *   $0153: call $0300       F, hooked, on thread 1
 *   $0156: ld sp,$c0fe      back to the main stack, where G's frame waits
 *   $0159: ret              G returns, to $0306
 *   $0300: ld sp,$c100      F yields to the main stack ($c0b0-$c110)
 *   $0303: call $0310       G, hooked, on the main stack
 *   $0306: jp $0150
 *   $0310: ld sp,$c1ee      G yields back to thread 1, F's frame on top
 *   $0313: ret              F returns, to $0156
 * F returns while G's frame is the last one pushed, on another stack; then G
 * returns.  Every loop must fire both returns, and no frame be purged. */
#include "core.h"
#include "guest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const OraclesCompatProfile *original_profile(OraclesGame game)
{
    const OraclesRomInfo info = { .game = game, .revision = game == ORACLES_GAME_SEASONS ? ORACLES_ROM_REVISION_SEASONS_US : ORACLES_ROM_REVISION_AGES_US, .known = 1 };
    return oracles_compat_find(&info);
}

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

static unsigned f_entries, f_returns, g_entries, g_returns;

static void on_event(void *opaque, const OraclesGuestEvent *e)
{
    (void)opaque;
    switch (e->type) {
        case ORACLES_EVENT_TEXT: f_entries++; break;
        case ORACLES_EVENT_WARP: f_returns++; CHECK(e->pc == 0x0156); break;
        case ORACLES_EVENT_MENU: g_entries++; break;
        case ORACLES_EVENT_SAVE: g_returns++; CHECK(e->pc == 0x0306); break;
        default: break;
    }
}

int main(void)
{
    const size_t size = 1024u * 1024u;
    uint8_t *rom = calloc(size, 1);
    memcpy(rom + 0x134, "ZELDA NAYRU", 11);
    rom[0x143] = 0xc0; rom[0x147] = 0x1b; rom[0x148] = 0x05; rom[0x149] = 0x02;
    rom[0x100] = 0x00; rom[0x101] = 0xc3; rom[0x102] = 0x50; rom[0x103] = 0x01;      /* nop ; jp $0150 */
    const uint8_t main_loop[] = { 0x31, 0xf0, 0xc1, 0xcd, 0x00, 0x03, 0x31, 0xfe, 0xc0, 0xc9 };
    const uint8_t f[] = { 0x31, 0x00, 0xc1, 0xcd, 0x10, 0x03, 0xc3, 0x50, 0x01 };
    const uint8_t g[] = { 0x31, 0xee, 0xc1, 0xc9 };
    memcpy(rom + 0x150, main_loop, sizeof main_loop);
    memcpy(rom + 0x300, f, sizeof f);
    memcpy(rom + 0x310, g, sizeof g);

    const OraclesCoreOptions options = { 0, 0, ORACLES_CORE_SAMEBOY };
    OraclesCore *core = oracles_core_create(rom, size, &options);
    free(rom);
    CHECK(core != NULL);
    if (!core) return 1;
    OraclesGuest *guest = oracles_guest_attach(core, original_profile(ORACLES_GAME_AGES));   /* Ages tables: the stacks' bounds */
    CHECK(guest != NULL);
    if (!guest) return 1;
    oracles_guest_set_event_sink(guest, on_event, NULL);
    oracles_guest_clear_hooks(guest);
    CHECK(oracles_guest_add_hook(guest, (OraclesGuestSym){ 0, 0x0300 }, ORACLES_EVENT_TEXT, ORACLES_EVENT_WARP) == 0);
    CHECK(oracles_guest_add_hook(guest, (OraclesGuestSym){ 0, 0x0310 }, ORACLES_EVENT_MENU, ORACLES_EVENT_SAVE) == 0);

    for (unsigned i = 0; i < 200; i++) oracles_core_run_frame(core);   /* the boot ROM */
    f_entries = f_returns = g_entries = g_returns = 0;
    unsigned overflow_before = 0, purged_before = 0;
    oracles_guest_dropped_returns(guest, &overflow_before, &purged_before);
    oracles_core_run_frame(core);
    unsigned overflow = 0, purged = 0;
    oracles_guest_dropped_returns(guest, &overflow, &purged);

    CHECK(f_entries > 100u);                        /* the loop runs many times a frame */
    CHECK(g_entries == f_entries || g_entries == f_entries - 1u);
    CHECK(f_returns + 1u >= f_entries && f_returns <= f_entries);
    CHECK(g_returns + 1u >= g_entries && g_returns <= g_entries);
    CHECK(purged == purged_before);
    CHECK(overflow == overflow_before);

    oracles_guest_detach(guest);
    oracles_core_destroy(core);
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_guest_threads: ok (%u loops)\n", f_entries);
    return 0;
}
