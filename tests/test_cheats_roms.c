/* The rupees' cheat in the games themselves, on the user's ROMs found by their SHA-1 in $ORACLES_ROM_DIR, the fan games
 * made from them by the BPS patches there (skipped, exit 77, without any): the game's own removeRupeeValue, the routine
 * the cheat's point is in, called in the core as a shop calls it, the price in a (the value 4, 10 rupees), from the main
 * loop's wait for the vblank (`halt / nop / bit 7,(hl)`, its hl given back by a `pop hl / ret` of bank 0 on the way
 * out).  The wallet the profile binds, 50 rupees, loses 10 with the cheat off and keeps them with it on, the guest
 * counting the payment cancelled.  The ore chunks' entry of Seasons' engine (`ld hl,<count> / jr` before it) reaches the
 * same call: Seasons' ore chunks are spent with the cheat on, Temple of Seasons', its prices' count, are not.  Both
 * cores. */
#include "bps.h"
#include "core.h"
#include "guest.h"
#include "profile.h"
#include "rom.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

typedef struct rom_file { uint8_t *data; size_t size; } rom_file;

static int identified(const uint8_t *data, size_t size, OraclesRomRevision revision)
{
    char error[128];
    OraclesRomInfo info;
    return data && oracles_rom_identify(data, size, &info, error, sizeof error) == 0 && info.known && info.revision == revision;
}

/* The ROM of `revision` in `dir`: a .gbc, or with `base` a .bps applied to it; empty when there is none. */
static rom_file find(const char *dir, OraclesRomRevision revision, const rom_file *base)
{
    rom_file found = { NULL, 0 };
    DIR *listing = opendir(dir);
    struct dirent *entry;
    while (listing && !found.data && (entry = readdir(listing))) {
        const size_t n = strlen(entry->d_name);
        if (n < 4 || strcmp(entry->d_name + n - 4, base ? ".bps" : ".gbc")) continue;
        char path[1024], error[128];
        snprintf(path, sizeof path, "%s/%s", dir, entry->d_name);
        size_t size = 0, patched = 0;
        uint8_t *data = oracles_rom_read_file(path, &size, error, sizeof error);
        if (base && data) {
            uint8_t *image = oracles_bps_apply(base->data, base->size, data, size, &patched, error, sizeof error);
            free(data);
            data = image;
            size = patched;
        }
        if (identified(data, size, revision)) found = (rom_file){ data, size };
        else free(data);
    }
    if (listing) closedir(listing);
    return found;
}

typedef struct session {
    OraclesCore *core;
    OraclesGuest *guest;
    const rom_file *rom;
    uint16_t pop_hl;   /* a `pop hl / ret` of bank 0 */
} session;

static unsigned bcd16(OraclesGuest *guest, uint16_t addr)
{
    const unsigned word = oracles_guest_read16(guest, (OraclesGuestSym){ 0, addr });
    return (word >> 12 & 15u) * 1000u + (word >> 8 & 15u) * 100u + (word >> 4 & 15u) * 10u + (word & 15u);
}

/* WRAM bank 0, as the core holds it. */
static uint8_t *wram0(session *s)
{
    size_t size = 0;
    uint16_t bank = 0;
    return oracles_core_memory(s->core, ORACLES_CORE_WRAM, &size, &bank);
}

static void set_wallet(session *s, uint16_t addr, uint8_t bcd)
{
    uint8_t *wram = wram0(s);
    wram[addr - 0xc000u] = bcd;
    wram[addr - 0xc000u + 1u] = 0;
}

/* Calls `entry` with a = `a` from the main loop's wait, then plays two frames; 0 when the wait is not found. */
static int call_routine(session *s, uint16_t entry, uint8_t a)
{
    for (int tries = 0; tries < 60; tries++) {
        OraclesCoreRegisters r = oracles_core_registers(s->core);
        const uint8_t *code = s->rom->data + r.pc;
        if (r.pc < 0x3ffdu && r.sp >= 0xc006u && r.sp < 0xd000u && code[0] == 0x00 && code[1] == 0xcb && code[2] == 0x7e) {
            uint8_t *below = wram0(s) + (r.sp - 6u - 0xc000u);
            const uint16_t words[3] = { s->pop_hl, r.hl, r.pc };
            for (unsigned i = 0; i < 3u; i++) { below[2u * i] = (uint8_t)words[i]; below[2u * i + 1u] = (uint8_t)(words[i] >> 8); }
            r.sp = (uint16_t)(r.sp - 6u);
            r.pc = entry;
            r.af = (uint16_t)(a << 8 | (r.af & 0xffu));
            oracles_core_set_registers(s->core, &r);
            oracles_core_run_frame(s->core);
            oracles_core_run_frame(s->core);
            return 1;
        }
        oracles_core_run_frame(s->core);
    }
    return 0;
}

typedef struct game {
    const char *name;
    OraclesRomRevision revision, base;
    int ore_free;   /* -1: no ore chunks' entry; 0: its count spent with the cheat on (Seasons); 1: kept (Temple) */
} game;

static const game games[] = {
    { "ages", ORACLES_ROM_REVISION_AGES_US, ORACLES_ROM_REVISION_UNKNOWN, -1 },
    { "seasons", ORACLES_ROM_REVISION_SEASONS_US, ORACLES_ROM_REVISION_UNKNOWN, 0 },
    { "kinomi", ORACLES_ROM_REVISION_KINOMI_1_1_2, ORACLES_ROM_REVISION_AGES_US, -1 },
    { "moonrise", ORACLES_ROM_REVISION_MOONRISE_1_0_6, ORACLES_ROM_REVISION_AGES_US, -1 },
    { "temple", ORACLES_ROM_REVISION_TEMPLE_1_073, ORACLES_ROM_REVISION_SEASONS_US, 1 },
};

static void check_game(const game *g, const rom_file *rom, OraclesCoreKind kind)
{
    char error[128];
    OraclesRomInfo info;
    CHECK(oracles_rom_identify(rom->data, rom->size, &info, error, sizeof error) == 0);
    const OraclesCompatProfile *profile = oracles_compat_find(&info);
    CHECK(profile && oracles_compat_cheats(profile));
    if (!profile) return;
    const OraclesGuestTables *t = oracles_compat_guest_tables(profile);
    OraclesCoreOptions options = { 0 };
    options.no_video = 1;
    options.kind = kind;
    session s = { oracles_core_create(rom->data, rom->size, &options), NULL, rom, 0 };
    CHECK(s.core != NULL);
    if (!s.core) return;
    s.guest = oracles_guest_attach(s.core, profile);
    CHECK(s.guest != NULL);
    for (uint16_t at = 0; !s.pop_hl && at < 0x3fffu; at++) if (rom->data[at] == 0xe1 && rom->data[at + 1u] == 0xc9) s.pop_hl = at;
    const OraclesGuestRom grom = oracles_guest_rom(s.guest);
    CHECK(s.pop_hl && oracles_guest_rupee_amount(t, &grom, 4) == 10);
    for (int f = 0; f < 1200; f++) oracles_core_run_frame(s.core);

    const uint16_t wallet = t->num_rupees.addr, remove = t->remove_rupee_value.addr;
    set_wallet(&s, wallet, 0x50);
    CHECK(call_routine(&s, remove, 4) && bcd16(s.guest, wallet) == 40);   /* the cheat off: the game takes 10 */
    CHECK(oracles_guest_set_cheats(s.guest, ORACLES_CHEAT_RUPEES) == 0);
    CHECK(call_routine(&s, remove, 4) && bcd16(s.guest, wallet) == 40);   /* on: nothing taken */
    OraclesGuestCheatCounts counts;
    oracles_guest_cheat_counts(s.guest, &counts);
    CHECK(counts.payments == 1 && counts.rupees_kept == 10 && counts.calls_charged == 0);

    if (g->ore_free >= 0) {
        /* The ore chunks' entry, `ld hl,<count> / jr` to the call, just before removeRupeeValue. */
        const uint8_t *ore = rom->data + remove - 5u;
        const uint16_t count = (uint16_t)(ore[1] | ore[2] << 8);
        CHECK(ore[0] == 0x21 && ore[3] == 0x18 && ore[4] == 0x03 && count >= 0xc000u && count < 0xd000u);
        CHECK((count == wallet) == (g->ore_free == 1));
        set_wallet(&s, count, 0x50);
        CHECK(call_routine(&s, (uint16_t)(remove - 5u), 4) && bcd16(s.guest, count) == (g->ore_free ? 50u : 40u));
        oracles_guest_cheat_counts(s.guest, &counts);
        CHECK(counts.payments == (g->ore_free ? 2u : 1u));
    }
    printf("cheats-roms: %s on %s: a price of 10 rupees taken, then kept with the cheat%s\n", g->name, kind == ORACLES_CORE_MGBA ? "mGBA" : "SameBoy",
           g->ore_free < 0 ? "" : g->ore_free ? "; its ore chunks' entry kept too" : "; its ore chunks spent");
    oracles_guest_detach(s.guest);
    oracles_core_destroy(s.core);
}

int main(void)
{
    const char *dir = getenv("ORACLES_ROM_DIR");
    if (!dir || !*dir) { printf("cheats-roms: ORACLES_ROM_DIR is not set: skipped\n"); return 77; }
    rom_file roms[sizeof games / sizeof games[0]];
    unsigned checked = 0;
    for (size_t i = 0; i < sizeof games / sizeof games[0]; i++) {
        const rom_file *base = NULL;
        for (size_t j = 0; j < i; j++) if (games[j].revision == games[i].base) base = &roms[j];
        roms[i] = games[i].base == ORACLES_ROM_REVISION_UNKNOWN ? find(dir, games[i].revision, NULL)
                  : base && base->data ? find(dir, games[i].revision, base) : (rom_file){ NULL, 0 };
        if (!roms[i].data) { printf("cheats-roms: %s not in %s: not checked\n", games[i].name, dir); continue; }
        check_game(&games[i], &roms[i], ORACLES_CORE_SAMEBOY);
        check_game(&games[i], &roms[i], ORACLES_CORE_MGBA);
        checked++;
    }
    for (size_t i = 0; i < sizeof games / sizeof games[0]; i++) free(roms[i].data);
    if (failures) fprintf(stderr, "%d failure(s)\n", failures);
    if (!failures && !checked) { printf("cheats-roms: no game in %s: skipped\n", dir); return 77; }
    return failures ? 1 : 0;
}
