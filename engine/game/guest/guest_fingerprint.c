#include "guest_fingerprint.h"

#include <inttypes.h>

void oracles_guest_write_fingerprint(OraclesGuest *guest, OraclesCore *core, uint32_t frame, FILE *out)
{
    const uint32_t *pixels = oracles_core_pixels(core);
    const uint64_t fb = oracles_guest_hash((const uint8_t *)pixels, ORACLES_SCREEN_PIXELS * sizeof *pixels, ORACLES_HASH_SEED);
    const uint64_t wram = oracles_guest_live_wram_hash(guest);
    const uint64_t hram = oracles_guest_hash(oracles_guest_hram(guest), 127, ORACLES_HASH_SEED);
    const uint64_t oam = oracles_guest_hash(oracles_guest_oam(guest), 160, ORACLES_HASH_SEED);
    const uint64_t vram = oracles_guest_hash(oracles_guest_vram(guest, 0), 0x4000, ORACLES_HASH_SEED);
    fprintf(out, "%u\t%016" PRIx64 "\t%016" PRIx64 "\t%016" PRIx64 "\t%016" PRIx64 "\t%016" PRIx64 "\n", frame, fb, wram, hram, oam, vram);
}
