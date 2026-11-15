#include "object_animation.h"

#include <string.h>

#define LOOP_MARKER 0xffu
#define ENTRY_BYTES 3u
#define INTERACTION_OAM_BIT 0x40u   /* _interactionNextAnimationFrame and _partNextAnimationFrame: `and $3f ; or $40` */
#define SET_ANIMATIONS_MAX 64u      /* the animations of one id xxxSetAnimation may name: their list has no length */

/* The kind's two tables; the animation's data and the OAM data table are read
 * in the bank the routine selects, its animation table's. */
static int tables(const OraclesGuestTables *t, unsigned kind, OraclesGuestSym *animations, OraclesGuestSym *oam)
{
    switch (kind) {
    case ORACLES_OBJECT_KIND_INTERACTION: *animations = t->interaction_animation_table; *oam = t->interaction_oam_data_table; break;
    case ORACLES_OBJECT_KIND_ENEMY: *animations = t->enemy_animation_table; *oam = t->enemy_oam_data_table; break;
    case ORACLES_OBJECT_KIND_PART: *animations = t->part_animation_table; *oam = t->part_oam_data_table; break;
    default: return 0;
    }
    return animations->bank != ORACLES_GUEST_ABSENT && oam->bank != ORACLES_GUEST_ABSENT;
}

int oracles_object_animation_resolved(const OraclesGuestTables *t, unsigned kind)
{
    OraclesGuestSym animations, oam;
    return t && tables(t, kind, &animations, &oam);
}

void oracles_object_animation_from_record(OraclesObjectAnimation *a, const OraclesObjectRecord *r)
{
    memset(a, 0, sizeof *a);
    a->kind = r->kind;
    a->id = r->id;
    a->counter = r->anim_counter;
    a->parameter = r->anim_parameter;
    a->pointer = r->anim_pointer;
    a->oam_data_address = r->oam_data_address;
}

/* A switchable bank's address in the ROM, -1 outside $4000-$7fff or the ROM. */
static long offset(unsigned bank, unsigned address, size_t length, size_t rom_size)
{
    if (address < 0x4000u || address + length > 0x8000u) return -1;
    const size_t at = (size_t)bank * 0x4000u + (address - 0x4000u);
    return at + length <= rom_size ? (long)at : -1;
}

static unsigned word(const uint8_t *rom, long at) { return (unsigned)(rom[at] | rom[at + 1] << 8); }

int oracles_object_animation_step(OraclesObjectAnimation *a, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size)
{
    OraclesGuestSym animations, oam;
    if (!a || !t || !rom || !tables(t, a->kind, &animations, &oam)) return -1;
    const uint8_t counter = (uint8_t)(a->counter - 1u);
    if (counter) { a->counter = counter; return 0; }
    /* _xxxNextAnimationFrame: the duration, or the loop marker and the low
     * byte of an offset whose high byte is $ff, added to the offset's own
     * address; then the pose's index and animParameter. */
    unsigned pointer = a->pointer;
    long at = offset(animations.bank, pointer, 1u, rom_size);
    if (at < 0) return -1;
    if (rom[at] == LOOP_MARKER) {
        if (offset(animations.bank, pointer + 1u, 1u, rom_size) < 0) return -1;
        pointer = (pointer + 1u + (0xff00u | rom[at + 1])) & 0xffffu;
        at = offset(animations.bank, pointer, ENTRY_BYTES, rom_size);
    } else at = offset(animations.bank, pointer, ENTRY_BYTES, rom_size);
    if (at < 0) return -1;
    const uint8_t duration = rom[at], index = rom[at + 1], parameter = rom[at + 2];
    /* The pose: the id's list in the OAM data table, the index added. */
    const long row = offset(animations.bank, oam.addr + 2u * a->id, 2u, rom_size);
    if (row < 0) return -1;
    const long block = offset(animations.bank, word(rom, row) + index, 2u, rom_size);
    if (block < 0) return -1;
    unsigned address = word(rom, block) & 0x3fffu;
    if (a->kind != ORACLES_OBJECT_KIND_ENEMY) address |= INTERACTION_OAM_BIT << 8;
    a->counter = duration;
    a->parameter = parameter;
    a->pointer = (uint16_t)(pointer + ENTRY_BYTES);
    a->oam_data_address = (uint16_t)address;
    return 1;
}

unsigned oracles_object_animation_poses(const OraclesObjectAnimation *a, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size,
                                        uint16_t poses[], unsigned max, unsigned *period, unsigned *changes)
{
    if (period) *period = 0;
    if (changes) *changes = 0;
    if (!a || !max) return 0;
    OraclesObjectAnimation s = *a;
    unsigned count = 0;
    poses[count++] = s.oam_data_address;
    /* After an entry read, the state is the pointer and the counter that entry
     * set: the loop is closed when one comes back. */
    uint16_t seen_pointer[ORACLES_OBJECT_ANIMATION_POSES];
    uint8_t seen_counter[ORACLES_OBJECT_ANIMATION_POSES];
    unsigned seen_calls[ORACLES_OBJECT_ANIMATION_POSES], seen_changes[ORACLES_OBJECT_ANIMATION_POSES], reads = 0, calls = 0, moved = 0;
    for (;;) {
        uint16_t before = s.oam_data_address;
        int rc;
        do { rc = oracles_object_animation_step(&s, t, rom, rom_size); calls++; } while (rc == 0);
        if (rc < 0) return 0;
        if (s.oam_data_address != before) moved++;
        for (unsigned k = 0; k < reads; k++)
            if (seen_pointer[k] == s.pointer && seen_counter[k] == s.counter) {
                if (period) *period = calls - seen_calls[k];
                if (changes) *changes = moved - seen_changes[k];
                return count;
            }
        if (reads >= ORACLES_OBJECT_ANIMATION_POSES) return count;
        seen_pointer[reads] = s.pointer;
        seen_counter[reads] = s.counter;
        seen_calls[reads] = calls;
        seen_changes[reads] = moved;
        reads++;
        int known = 0;
        for (unsigned k = 0; k < count && !known; k++) known = poses[k] == s.oam_data_address;
        if (!known && count < max) poses[count++] = s.oam_data_address;
    }
}

int oracles_object_animation_is_set(const OraclesObjectAnimation *a, uint16_t pointer, const OraclesGuestTables *t, const uint8_t *rom, size_t rom_size)
{
    OraclesGuestSym animations, oam;
    if (!a || !t || !rom || !tables(t, a->kind, &animations, &oam)) return 0;
    const long row = offset(animations.bank, animations.addr + 2u * a->id, 2u, rom_size);
    if (row < 0) return 0;
    /* xxxSetAnimation: the id's list of animations, two bytes each, the
     * animation's data at the one chosen; its first entry read (or the entry
     * after the loop marker), the pointer stands past it. */
    const unsigned list = word(rom, row);
    for (unsigned n = 0; n < SET_ANIMATIONS_MAX; n++) {
        const long entry = offset(animations.bank, list + 2u * n, 2u, rom_size);
        if (entry < 0) return 0;
        const unsigned data = word(rom, entry);
        if (((data + ENTRY_BYTES) & 0xffffu) == pointer) return 1;
        const long first = offset(animations.bank, data, 2u, rom_size);
        if (first >= 0 && rom[first] == LOOP_MARKER && ((data + 1u + (0xff00u | rom[first + 1]) + ENTRY_BYTES) & 0xffffu) == pointer) return 1;
    }
    return 0;
}

int oracles_object_animation_one_step(const OraclesObjectAnimation *last, const OraclesObjectAnimation *now, const OraclesGuestTables *t,
                                      const uint8_t *rom, size_t rom_size)
{
    if (!last || !now || last->kind != now->kind || last->id != now->id) return 0;
    OraclesObjectAnimation once = *last;
    if (oracles_object_animation_step(&once, t, rom, rom_size) < 0) return 0;
    return once.counter == now->counter && once.pointer == now->pointer && once.oam_data_address == now->oam_data_address;
}

int oracles_object_animation_restarted(const OraclesObjectAnimation *last, const OraclesObjectAnimation *now, const OraclesGuestTables *t,
                                       const uint8_t *rom, size_t rom_size)
{
    if (!last || !now || last->kind != now->kind || last->id != now->id || !oracles_object_animation_is_set(now, now->pointer, t, rom, rom_size)) return 0;
    /* The entries of the loop `last` plays, each read once at most. */
    OraclesObjectAnimation s = *last;
    for (unsigned reads = 0; reads < ORACLES_OBJECT_ANIMATION_POSES; reads++) {
        int rc;
        do rc = oracles_object_animation_step(&s, t, rom, rom_size); while (rc == 0);
        if (rc < 0) return 0;
        if (s.pointer == now->pointer && s.oam_data_address == now->oam_data_address) return 1;
    }
    return 0;
}

int oracles_object_animation_probed(unsigned kind) { return kind == ORACLES_OBJECT_KIND_INTERACTION || kind == ORACLES_OBJECT_KIND_PART; }

int oracles_object_animation_animated(const OraclesGuestTables *t, const OraclesObjectRecord *r)
{
    if (!t || !r || !oracles_object_animation_probed(r->kind)) return 0;
    return oracles_object_animation_resolved(t, r->kind) && r->animated_by_code;
}
