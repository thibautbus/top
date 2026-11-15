/* The capture's probe (ghost.h): which of a capture's interactions and parts their own code animates, asked over two
 * guest frames run with the enemies frozen, after the settled state is kept. */
#include "ghost_internal.h"

#include <string.h>

/* The capture's probe (ghost.h): each record's animation as the object
 * stands now; 0 when its slot no longer holds it. */
#define DISABLE_ENEMIES 0x04u   /* the bit of wDisabledObjects updateEnemies reads */

static int probe_read(OraclesGhost *g, const OraclesObjectRecord *r, OraclesObjectAnimation *a)
{
    const uint8_t *p = oracles_object_animation_probed(r->kind) ? oracles_guest_object(g->guest, r->slot, r->kind) : NULL;
    if (!p || !p[ORACLES_OBJ_ENABLED] || p[ORACLES_OBJ_ID] != r->id) return 0;
    OraclesObjectRecord now = *r;
    now.anim_counter = p[ORACLES_OBJ_ANIM_COUNTER];
    now.anim_parameter = p[ORACLES_OBJ_ANIM_PARAMETER];
    now.anim_pointer = (uint16_t)(p[ORACLES_OBJ_ANIM_POINTER] | (p[ORACLES_OBJ_ANIM_POINTER + 1] << 8));
    now.oam_data_address = (uint16_t)(p[ORACLES_OBJ_OAM_DATA_ADDRESS] | (p[ORACLES_OBJ_OAM_DATA_ADDRESS + 1] << 8));
    oracles_object_animation_from_record(a, &now);
    return 1;
}

/* An object asked one update more: its first set again the animation it plays. */
static int restart_pending(const OraclesGhost *g)
{
    const OraclesGhostResult *result = g->result;
    for (unsigned i = 0; result && i < result->object_count; i++) if (g->probe_ok[i] && g->probe_restarted[i]) return 1;
    return 0;
}

/* An update of the objects during the probe: an object stays animated while each update is one call of the step, the
 * first of which may set again the animation it plays. */
void oracles_ghost_probe_update(OraclesGhost *g)
{
    OraclesGhostResult *result = g->result;
    if (!result || g->probe_updates >= ORACLES_GHOST_PROBE_UPDATES + (unsigned)restart_pending(g)) return;
    const OraclesGuestTables *t = oracles_guest_tables(g->guest);
    const OraclesGuestRom rom = oracles_guest_rom(g->guest);
    g->probe_updates++;
    for (unsigned i = 0; i < result->object_count; i++) {
        if (!g->probe_ok[i] || g->probe_updates > ORACLES_GHOST_PROBE_UPDATES + g->probe_restarted[i]) continue;
        OraclesObjectAnimation now;
        if (!probe_read(g, &result->objects[i], &now)) g->probe_ok[i] = 0;
        else if (!oracles_object_animation_one_step(&g->probe_last[i], &now, t, rom.data, rom.size)) {
            g->probe_restarted[i] = (uint8_t)(g->probe_updates == 1u && oracles_object_animation_restarted(&g->probe_last[i], &now, t, rom.data, rom.size));
            g->probe_ok[i] = g->probe_restarted[i];
        }
        g->probe_last[i] = now;
    }
}

void oracles_ghost_probe_animation(OraclesGhost *g, OraclesGhostResult *result)
{
    const OraclesGuestTables *t = oracles_guest_tables(g->guest);
    uint8_t *wram = oracles_guest_wram_writable(g->guest, 0);
    unsigned probed = 0;
    for (unsigned i = 0; i < result->object_count; i++) {
        result->objects[i].animated_by_code = 0;
        g->probe_restarted[i] = 0;
        g->probe_ok[i] = (uint8_t)probe_read(g, &result->objects[i], &g->probe_last[i]);
        probed += g->probe_ok[i];
    }
    result->probe_frames = 0;
    result->probe_restarts = 0;
    if (!probed || !wram || !oracles_ghost_bank0_symbol(t->disabled_objects)) return;   /* no interaction nor part to ask about */
    uint8_t *disabled = &wram[t->disabled_objects.addr - WRAM_BANK0_BASE];
    const uint8_t prior = *disabled;
    *disabled = (uint8_t)(prior | DISABLE_ENEMIES);
    /* Link left on a warp tile of the room (hold_warp_tiles) does not take it during the probe either: the warp
     * would disable the objects probed. */
    uint8_t *warp_tiles = oracles_ghost_bank0_symbol(t->disable_warp_tiles) ? &wram[t->disable_warp_tiles.addr - WRAM_BANK0_BASE] : NULL;
    const uint8_t warp_prior = warp_tiles ? *warp_tiles : 0;
    g->probing = 1;
    g->probe_updates = 0;
    for (unsigned f = 0;; f++) {
        const int more = g->probe_updates >= ORACLES_GHOST_PROBE_UPDATES && restart_pending(g);
        if (more ? g->probe_updates > ORACLES_GHOST_PROBE_UPDATES || f >= ORACLES_GHOST_PROBE_FRAMES + ORACLES_GHOST_PROBE_RESTART_FRAMES
                 : g->probe_updates >= ORACLES_GHOST_PROBE_UPDATES || f >= ORACLES_GHOST_PROBE_FRAMES) break;
        if (warp_tiles) *warp_tiles = 1;
        oracles_guest_set_frame(g->guest, g->frames_run + f);
        oracles_core_run_frame(g->core);
        result->probe_frames++;
    }
    g->probing = 0;
    *disabled = prior;
    if (warp_tiles) *warp_tiles = warp_prior;
    for (unsigned i = 0; i < result->object_count; i++) {
        result->objects[i].animated_by_code = (uint8_t)(g->probe_ok[i] && g->probe_updates >= ORACLES_GHOST_PROBE_UPDATES + g->probe_restarted[i]);
        result->probe_restarts += (unsigned)(result->objects[i].animated_by_code && g->probe_restarted[i]);
    }
}
