/* The software PPU against the core, without a game ROM: the free boot ROM
 * draws and scrolls its logo on a synthetic cartridge, and every vblank the
 * native rendering of the committed display state must equal the core's
 * framebuffer, in every class the check reports. */
#include "core.h"
#include "frame_check.h"
#include "guest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

static const OraclesCompatProfile *fixture_profile(void)
{
    const OraclesRomInfo info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_AGES_US, .known = 1 };
    return oracles_compat_find(&info);
}

int main(void)
{
    const size_t size = 1024u * 1024u;
    uint8_t *rom = calloc(size, 1);
    memcpy(rom + 0x134, "ZELDA NAYRU", 11);
    rom[0x143] = 0xc0; rom[0x147] = 0x1b; rom[0x148] = 0x05; rom[0x149] = 0x02;
    /* $0100: nop ; jp $0150.  $0150, in vblank: tile 1 all colour 3, at the
     * top left of the map; BG palette 0 = saturated green ($03e0, a colour
     * the correction changes) then three blacks, through BCPS/BCPD; LCD on
     * with BG and objects (LCDC $93); loop.  The screen is one black tile
     * on a green ground. */
    static const uint8_t program[] = {
        0xf0, 0x44, 0xfe, 0x90, 0x20, 0xfa,               /* ldh a,($44) ; cp 144 ; jr nz,-6 */
        0x3e, 0xff, 0x21, 0x10, 0x80, 0x0e, 0x10,         /* ld a,$ff ; ld hl,$8010 ; ld c,16 */
        0x22, 0x0d, 0x20, 0xfc,                           /* ld (hl+),a ; dec c ; jr nz,-4 */
        0x3e, 0x01, 0xea, 0x00, 0x98,                     /* ld a,1 ; ld ($9800),a */
        0x3e, 0x80, 0xe0, 0x68,                           /* ld a,$80 ; ldh ($68),a */
        0x3e, 0xe0, 0xe0, 0x69, 0x3e, 0x03, 0xe0, 0x69,   /* colour 0 = $03e0 */
        0xaf, 0xe0, 0x69, 0xe0, 0x69, 0xe0, 0x69,         /* xor a ; colours 1 to 3 = 0 */
        0xe0, 0x69, 0xe0, 0x69, 0xe0, 0x69,
        0x3e, 0x93, 0xe0, 0x40,                           /* ld a,$93 ; ldh ($40),a */
        /* Every frame: SCX = 8 written in the hblank of line 50, SCX = 0 in the
         * hblank of line 100.  The core scrolls lines 51 to 100 only: the
         * journal's rule "a mode-0 write applies to the next line" is
         * exercised without a ROM. */
        0xf0, 0x44, 0xfe, 0x32, 0x20, 0xfa,               /* w1: ldh a,($44) ; cp 50 ; jr nz,w1 */
        0xf0, 0x41, 0xe6, 0x03, 0x20, 0xfa,               /* w2: ldh a,($41) ; and 3 ; jr nz,w2 */
        0x3e, 0x08, 0xe0, 0x43,                           /* ld a,8 ; ldh ($43),a */
        0xf0, 0x44, 0xfe, 0x64, 0x20, 0xfa,               /* w3: ldh a,($44) ; cp 100 ; jr nz,w3 */
        0xf0, 0x41, 0xe6, 0x03, 0x20, 0xfa,               /* w4: ldh a,($41) ; and 3 ; jr nz,w4 */
        0xaf, 0xe0, 0x43,                                 /* xor a ; ldh ($43),a */
        0x18, 0xdf                                        /* jr w1 (-33) */
    };
    rom[0x100] = 0x00; rom[0x101] = 0xc3; rom[0x102] = 0x50; rom[0x103] = 0x01;
    memcpy(rom + 0x150, program, sizeof program);

    const OraclesCoreOptions options = { 0, 0, ORACLES_CORE_SAMEBOY, 0 };
    OraclesCore *core = oracles_core_create(rom, size, &options);
    free(rom);
    CHECK(core != NULL);
    if (!core) return 1;
    OraclesGuest *guest = oracles_guest_attach(core, fixture_profile());
    CHECK(guest != NULL);
    OraclesFrameCheck *check = oracles_frame_check_start(guest, core, NULL, 0);
    CHECK(check != NULL);
    /* The second half runs under the player's colour correction: the native
     * image must follow the core's pipeline, not the raw conversion. */
    for (unsigned frame = 0; frame < 400; frame++) {
        if (frame == 200) oracles_core_set_colour_correction(core, 1);
        oracles_frame_check_frame_begin(check, frame);
        oracles_core_run_frame(core);
    }
    const OraclesFrameStats *s = oracles_frame_check_stats(check);
    uint32_t compared = 0, mismatches = 0;
    for (unsigned i = 0; i < ORACLES_FRAME_CLASSES; i++) {
        if (i == ORACLES_FRAME_MID_SCAN || i == ORACLES_FRAME_NO_SCAN) continue;
        compared += s->frames[i];
        mismatches += s->mismatches[i];
    }
    CHECK(compared >= 300);
    CHECK(s->frames[ORACLES_FRAME_NORMAL] >= 100);   /* the boot logo scan, then the program's own LCD */
    CHECK(mismatches == 0);
    CHECK(s->late_scroll_lines == 0);   /* every line of the program is compared */
    if (mismatches) oracles_frame_check_report(check, stderr);

    /* The scroll took effect: on the last frame, line 60 of the native image
     * is line 60 shifted by eight pixels against line 40, while line 110 is
     * not (the program's ground is uniform, the black tile at the top left
     * makes the shift visible only where its row is: check the journal's
     * effect through the comparison above, which would fail on a wrong rule). */
    /* The native buffer is a real image: not a single colour, and its ground
     * is the program's green as the active pipeline converts it. */
    const uint32_t *native = oracles_frame_check_native(check);
    int distinct = 0, ground = 0;
    CHECK(oracles_core_colour_correction(core) == 1);
    const uint32_t green = oracles_core_convert_rgb555(core, 0x03e0);
    CHECK(green != oracles_ppu_rgb555(0x03e0));
    for (unsigned i = 0; i < ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT; i++) {
        if (native[i] != native[0]) distinct = 1;
        if (native[i] == green) ground++;
    }
    CHECK(distinct);
    CHECK(native[0] == 0xff000000u);
    CHECK(ground > ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT / 2);

    /* The core's conversion: raw, every colour is the pinned conversion. */
    oracles_core_set_colour_correction(core, 0);
    CHECK(oracles_core_colour_correction(core) == 0);
    int raw_equal = 1;
    for (unsigned i = 0; i < ORACLES_PPU_COLOURS && raw_equal; i++)
        if (oracles_core_convert_rgb555(core, (uint16_t)i) != oracles_ppu_rgb555((uint16_t)i)) raw_equal = 0;
    CHECK(raw_equal);

    /* Sensitivity: the PPU on a tampered input gives another image, so the comparison can fail. */
    {
        OraclesPpuInput in;
        uint8_t vram[0x4000], oam[160], bg[64], obj[64];
        memcpy(vram, oracles_guest_vram(guest, 0), sizeof vram);
        memcpy(oam, oracles_guest_oam(guest), sizeof oam);
        memcpy(bg, oracles_guest_bg_palettes(guest), sizeof bg);
        memcpy(obj, oracles_guest_obj_palettes(guest), sizeof obj);
        in.vram = vram; in.oam = oam; in.bg_palettes = bg; in.obj_palettes = obj; in.colours = NULL;
        const OraclesPpuRegs regs = { 0x93, 0, 0, 0, 0 };
        for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) in.lines[ly] = regs;
        static uint32_t a[ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT], b[ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT];
        oracles_ppu_render(&in, a);
        bg[0] ^= 0x1f; /* colour 0 of palette 0 */
        oracles_ppu_render(&in, b);
        CHECK(memcmp(a, b, sizeof a) != 0);
        /* An SCX change on one line moves that line only. */
        bg[0] ^= 0x1f;
        in.lines[50].scx = 4;
        oracles_ppu_render(&in, b);
        CHECK(memcmp(a, b, 50 * ORACLES_PPU_WIDTH * sizeof a[0]) == 0);
        CHECK(memcmp(a + 51 * ORACLES_PPU_WIDTH, b + 51 * ORACLES_PPU_WIDTH, (ORACLES_PPU_HEIGHT - 51) * ORACLES_PPU_WIDTH * sizeof a[0]) == 0);
    }

    oracles_frame_check_stop(check);
    oracles_guest_detach(guest);
    oracles_core_destroy(core);
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_render: ok\n");
    return 0;
}
