/* mGBA's Game Boy core, as vendored and patched (third_party/mgba/VERSION.md), boots the free CGB boot ROM the engine
 * embeds for it (SameBoy's, its logo reading MGBA: cgb_boot_rom_mgba.c), without any ROM: a synthetic cartridge whose program, once the boot ROM hands over, stores A and a marker in
 * WRAM, once for each of the two CGB flags.  The boot ROM must be kept at reset (mGBA drops one whose CRC it does not
 * know: the patch adds ours), mapped over the cartridge, run (its logo animation drawn), then unmapped with the CGB's
 * value of A, in CGB mode.  Built with the definitions the library exports, the test also shows that the library and
 * its reader agree on mGBA's structures. */
#include "cgb_boot_rom_mgba.h"

#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/gb/core.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/sm83/sm83.h>
#include <mgba-util/vfs.h>

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

enum { WIDTH = 160, HEIGHT = 144, STRIDE = 256, MAX_FRAMES = 600 };

static uint8_t rom[0x8000];
static mColor video[STRIDE * 224];

/* At $150: ld ($c000),a; ld a,$5a; ld ($c001),a; jr @ (the entry point at $100 jumps there). */
static const uint8_t program[] = { 0xea, 0x00, 0xc0, 0x3e, 0x5a, 0xea, 0x01, 0xc0, 0x18, 0xfe };

static int frame_has_two_colours(void)
{
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            if (video[y * STRIDE + x] != video[0]) return 1;
    return 0;
}

/* Boots the synthetic cartridge with the given CGB flag, the model given to mGBA by the one configuration key its reset
 * reads for that flag. */
static void boot(uint8_t cgb_flag, const char *model_key)
{
    /* The header: nop; jp $150; a logo area holding a pattern (the boot ROM draws whatever is there); the CGB flag;
     * no mapper, 32 KiB. */
    memset(rom, 0, sizeof rom);
    rom[0x100] = 0x00; rom[0x101] = 0xc3; rom[0x102] = 0x50; rom[0x103] = 0x01;
    for (int i = 0x104; i < 0x134; i++) rom[i] = (uint8_t)(0x11u * (unsigned)(i & 7));
    memcpy(&rom[0x134], "MGBA BOOT", 9);
    rom[0x143] = cgb_flag;
    memcpy(&rom[0x150], program, sizeof program);

    struct mCore *core = GBCoreCreate();
    CHECK(core != NULL);
    if (!core) return;
    CHECK(core->init(core));
    mCoreInitConfig(core, NULL);
    /* A CGB, as SameBoy is created: with the model left to detection, mGBA would also drop an unknown boot ROM. */
    mCoreConfigSetValue(&core->config, model_key, "CGB");
    core->setVideoBuffer(core, video, STRIDE);
    CHECK(core->loadROM(core, VFileFromConstMemory(rom, sizeof rom)));
    CHECK(core->loadBIOS(core, VFileFromConstMemory(oracles_cgb_boot_rom_mgba, oracles_cgb_boot_rom_mgba_size), 0));
    core->reset(core);

    struct GB *gb = core->board;
    struct SM83Core *cpu = core->cpu;
    CHECK(gb->model == GB_MODEL_CGB);
    CHECK(gb->biosVf != NULL);   /* kept: GBIsCompatibleBIOS knows its CRC */
    CHECK(gb->memory.romBase != gb->memory.rom);
    CHECK(cpu->pc == 0);
    /* Mapped at $0000-$00ff and $0200-$08ff, the cartridge's header showing through at $0100-$01ff. */
    CHECK(memcmp(gb->memory.romBase, oracles_cgb_boot_rom_mgba, 0x100) == 0);
    CHECK(memcmp(&gb->memory.romBase[0x100], &rom[0x100], 0x100) == 0);
    CHECK(memcmp(&gb->memory.romBase[0x200], &oracles_cgb_boot_rom_mgba[0x200], oracles_cgb_boot_rom_mgba_size - 0x200) == 0);

    int first_drawn = -1, handed_over = -1;
    for (int frame = 0; frame < MAX_FRAMES && handed_over < 0; frame++) {
        core->runFrame(core);
        if (first_drawn < 0 && frame_has_two_colours()) first_drawn = frame;
        if (gb->memory.wram[1] == 0x5a) handed_over = frame;
    }
    printf("test_mgba_boot: CGB flag $%02x, logo drawn at frame %d, cartridge running at frame %d\n", cgb_flag, first_drawn, handed_over);
    CHECK(first_drawn >= 0);
    CHECK(handed_over > first_drawn);
    CHECK(handed_over >= 60);   /* the boot ROM ran its animation: a skipped boot hands over at once */
    CHECK(gb->memory.wram[0] == 0x11);   /* A as a CGB's boot ROM leaves it */
    CHECK(gb->memory.romBase == gb->memory.rom);   /* unmapped */
    CHECK(gb->model == GB_MODEL_CGB);   /* still in CGB mode: unmapping falls back to DMG for a cartridge without the flag */
    CHECK(cpu->pc >= 0x150 && cpu->pc < 0x150 + sizeof program);
    core->deinit(core);
}

int main(void)
{
    boot(0xc0, "cgb.model");          /* CGB only, as the Oracle games and their fan games */
    boot(0x80, "cgb.hybridModel");    /* CGB-enhanced, playable on a DMG */
    if (failures) return 1;
    puts("test_mgba_boot: ok");
    return 0;
}
