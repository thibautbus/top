/* The rooms drawn beside the room in play, against the game's own entry into
 * them from there: a ghost of the check's own, apart from the view's, enters
 * each from the live state once Link has stood in the room, and the objects
 * the band shows of the room's capture that this entry does not create are
 * counted.  The view's cache keeps a capture under the bytes it knows its
 * objects read; this measure knows none of them. */
#include "enhanced_check_internal.h"

#include <stdio.h>

#define BESIDE_QUIET_FRAMES 30u     /* frames of normal play in the room before its neighbours are judged: a run of the view's delivered */
#define BESIDE_SETTLE_FRAMES 160u   /* a load and a scroll, with margin: the view's own budget */

int oracles_enhanced_check_set_beside_witness(OraclesEnhancedCheck *c, const uint8_t *rom, size_t rom_size)
{
    c->witness = oracles_ghost_create(rom, rom_size, oracles_guest_profile(c->guest), oracles_core_kind(c->core));
    c->witness_result = malloc(sizeof *c->witness_result);
    c->witness_state = malloc(oracles_core_state_size(c->core));
    if (!c->witness || !c->witness_result || !c->witness_state || oracles_ghost_state_size(c->witness) != oracles_core_state_size(c->core)) {
        if (c->witness) oracles_ghost_destroy(c->witness);
        free(c->witness_result);
        free(c->witness_state);
        c->witness = NULL; c->witness_result = NULL; c->witness_state = NULL;
        return -1;
    }
    oracles_ghost_set_capture(c->witness, 1);
    return 0;
}

void ec_check_beside(OraclesEnhancedCheck *c, const OraclesEnhancedObservation *ob, OraclesEnhancedMode mode, uint32_t frame)
{
    if (!c->witness_have_room || ob->group != c->witness_group || ob->room != c->witness_room) {
        c->witness_have_room = 1;
        c->witness_group = ob->group;
        c->witness_room = ob->room;
        c->witness_quiet = 0;
        c->witness_done = 0;
    }
    if (c->witness_done) return;
    if (mode != ORACLES_ENHANCED_WORLD || !ob->playing || ob->in_transition || ob->cutscene || !oracles_ghost_primeable(c->guest, NULL)) { c->witness_quiet = 0; return; }
    if (++c->witness_quiet < BESIDE_QUIET_FRAMES) return;
    c->witness_done = 1;
    if (oracles_core_save_state(c->core, c->witness_state, oracles_core_state_size(c->core)) != 0) return;
    const OraclesGuestTables *t = oracles_guest_tables(c->guest);
    for (unsigned d = 0; d < 4u; d++) {
        int shown = 0;
        uint8_t room = 0;
        oracles_enhanced_view_neighbour(c->view, d, &shown, &room);
        if (!shown) continue;
        /* Seasons: the run in the live season, as the view's are. */
        oracles_ghost_set_held_season(c->witness, oracles_compat_seasons_rules(oracles_guest_profile(c->guest)) ? oracles_guest_read8(c->guest, t->room_state_modifier) : -1);
        const int rc = oracles_ghost_run_ex(c->witness, c->witness_state, oracles_core_state_size(c->core), (OraclesGhostDirection)d, 1, NULL, BESIDE_SETTLE_FRAMES, c->witness_result);
        if (rc != 1 || c->witness_result->status != ORACLES_GHOST_OK || !c->witness_result->settled) continue;
        const int orphans = oracles_enhanced_view_beside_orphans(c->view, d, c->witness_result);
        if (orphans < 0) continue;
        c->beside_checked++;
        if (!orphans) continue;
        c->beside_orphans += (unsigned)orphans;
        const size_t used = strlen(c->beside_list);
        if (used + 24u < sizeof c->beside_list)
            snprintf(c->beside_list + used, sizeof c->beside_list - used, "%s%u:%02x>%u:%02x@%u x%d", used ? ", " : "", ob->group, ob->room, c->witness_result->group, c->witness_result->room, frame, orphans);
    }
}
