/* The colour table the cores share (engine/core/colour.c) equals SameBoy's own conversion on all 32768 RGB555 colours,
 * raw and corrected, without any ROM: a synthetic cartridge only lets a SameBoy core be created. */
#include "colour.h"
#include "core.h"

#include <stdio.h>
#include <stdlib.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

int main(void)
{
    const size_t size = 0x8000u;
    uint8_t *rom = calloc(size, 1);
    if (!rom) return 1;
    rom[0x143] = 0xc0;
    const OraclesCoreOptions options = { 0, 0, ORACLES_CORE_SAMEBOY };
    OraclesCore *core = oracles_core_create(rom, size, &options);
    CHECK(core != NULL);
    if (!core) return 1;
    static uint32_t table[ORACLES_COLOURS];
    for (int correction = 0; correction <= 1; correction++) {
        oracles_core_set_colour_correction(core, correction);
        oracles_colour_fill(table, correction);
        unsigned differ = 0, first = 0;
        for (unsigned c = 0; c < ORACLES_COLOURS; c++) {
            if (table[c] == oracles_core_convert_rgb555(core, (uint16_t)c)) continue;
            if (!differ++) first = c;
        }
        if (differ) fprintf(stderr, "correction %d: %u colours differ, the first $%04x: %08x, SameBoy %08x\n", correction,
                            differ, first, (unsigned)table[first], (unsigned)oracles_core_convert_rgb555(core, (uint16_t)first));
        CHECK(differ == 0);
        CHECK(oracles_colour_convert(0x7fffu, correction) == 0xffffffffu);
    }
    oracles_core_destroy(core);
    free(rom);
    if (failures) return 1;
    puts("test_colour: ok");
    return 0;
}
