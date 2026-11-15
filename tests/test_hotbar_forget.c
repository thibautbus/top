/* The hotbar's icons forgotten when another half of the game comes under the view, on a synthetic core: the guest's
 * hooks cleared as the half's switch clears them, the bar's reload of its icons is hooked again by the forget alone
 * (a replay gives the view its slots once, at the start). */
#include "core.h"
#include "guest.h"
#include "oracles_rom.h"
#include "view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

static const OraclesCompatProfile *ages_profile(void)
{
    const OraclesRomInfo info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_AGES_US, .known = 1 };
    return oracles_compat_find(&info);
}

static void forget_on(OraclesCoreKind kind)
{
    const size_t size = 1024u * 1024u;
    uint8_t *rom = calloc(size, 1);
    CHECK(rom != NULL);
    if (!rom) return;
    memcpy(rom + 0x134, "SYNTHETIC", 9);
    rom[0x143] = 0xc0; rom[0x147] = 0x1b; rom[0x148] = 0x05; rom[0x149] = 0x02;
    rom[0x100] = 0x00; rom[0x101] = 0xc3; rom[0x102] = 0x50; rom[0x103] = 0x01;
    rom[0x150] = 0x18; rom[0x151] = 0xfe;   /* jr -2 */
    const OraclesCoreOptions options = { 0, 0, kind, 0 };
    OraclesCore *core = oracles_core_create(rom, size, &options);
    OraclesGuest *guest = core ? oracles_guest_attach(core, ages_profile()) : NULL;
    OraclesEnhancedView *view = guest ? oracles_enhanced_view_start(core, guest, NULL, 0) : NULL;
    CHECK(view != NULL);
    if (view) {
        /* No hotbar: nothing hooked, and the forget hooks nothing. */
        oracles_guest_clear_hooks(guest);
        oracles_enhanced_view_hotbar_forget(view);
        CHECK(!oracles_enhanced_view_hotbar_hooked(view) && oracles_guest_hook_count(guest) == 0);
        OraclesEnhancedHotbar hotbar;
        memset(&hotbar, 0, sizeof hotbar);
        hotbar.slots[0].set = 1; hotbar.slots[0].item = 0x05; hotbar.slots[0].variant = 0xff;
        oracles_enhanced_view_set_hotbar(view, &hotbar);
        CHECK(oracles_enhanced_view_hotbar_hooked(view) && oracles_guest_hook_count(guest) == 1);
        /* The half's switch: its hooks cleared, then the forget, which arms the bar's reload again on its own. */
        oracles_guest_clear_hooks(guest);
        CHECK(oracles_guest_hook_count(guest) == 0);
        oracles_enhanced_view_hotbar_forget(view);
        CHECK(oracles_enhanced_view_hotbar_hooked(view) && oracles_guest_hook_count(guest) == 1);
        /* Forgotten again on the same guest: the hook is not armed twice. */
        oracles_enhanced_view_hotbar_forget(view);
        CHECK(oracles_enhanced_view_hotbar_hooked(view) && oracles_guest_hook_count(guest) == 1);
        oracles_enhanced_view_stop(view);
    }
    if (guest) oracles_guest_detach(guest);
    oracles_core_destroy(core);
    free(rom);
}

int main(void)
{
    forget_on(ORACLES_CORE_SAMEBOY);
    forget_on(ORACLES_CORE_MGBA);
    if (failures) { fprintf(stderr, "test_hotbar_forget: %d failure(s)\n", failures); return 1; }
    printf("test_hotbar_forget: ok\n");
    return 0;
}
