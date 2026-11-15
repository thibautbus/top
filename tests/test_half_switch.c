/* Oracles Combined's switch of halves between the view and its ghosts, on a synthetic image of two halves (8 MiB, zero
 * but for its header): its ROM's tilesets read so, the first half's rooms are all on their map (Ages' engine, no tileset
 * map rules) and the second's none (Seasons' rules, no outdoor tileset).  A ghost whose run outlasts the switch follows
 * it after the run; the view reads the rooms of the half the guest is in at once, never the ghost's.  A switch the
 * worker has not made yet is forgotten by one made in place, and the worker makes the last one asked.  The view's lane
 * waits for a run of the half left still in the worker, rather than ask the ghost for runs it would refuse, and drops
 * its result once the worker is free (as a stale one), the second lane of two ghosts as the first. */
#include "core.h"
#include "ghost.h"
#include "ghost_internal.h"
#include "guest.h"
#include "view_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

#define IMAGE_SIZE (8u * 1024u * 1024u)

static uint8_t *make_image(void)
{
    uint8_t *image = calloc(IMAGE_SIZE, 1);
    if (!image) return NULL;
    memcpy(image + 0x134, "TWO HALVES", 10);
    image[0x143] = 0xc0; image[0x147] = 0x1b; image[0x148] = 0x08; image[0x149] = 0x03;
    image[0x100] = 0x00; image[0x101] = 0x18; image[0x102] = 0xfe;   /* nop ; jr -2 */
    return image;
}

static const OraclesCompatProfile *profile_of(const OraclesGhost *g) { return oracles_guest_profile(g->guest); }

static void view_reads_its_half(const uint8_t *image, const OraclesCompatProfile *ages, const OraclesCompatProfile *seasons)
{
    const OraclesCoreOptions options = { 0, 0, ORACLES_CORE_SAMEBOY, 0 };
    OraclesCore *core = oracles_core_create(image, IMAGE_SIZE, &options);
    OraclesGuest *guest = core ? oracles_guest_attach(core, ages) : NULL;
    OraclesEnhancedView *v = guest ? oracles_enhanced_view_start(core, guest, image, IMAGE_SIZE) : NULL;
    CHECK(v != NULL && v->ghost != NULL);
    if (v && v->ghost) {
        CHECK(!ev_off_map(v, 0, 0x00));   /* Ages' half: no tileset map rules, every room on the map */
        v->ghost->job_pending = 1;        /* its run outlasts the passage */
        CHECK(oracles_guest_switch_to(guest, seasons) == 0);
        CHECK(profile_of(v->ghost) == ages && v->ghost->half_pending == seasons);
        CHECK(oracles_ghost_rooms(v->ghost)->map_room[0][0x00] == 1);   /* the ghost's rooms still Ages' */
        CHECK(ev_off_map(v, 0, 0x00));    /* the view's are Seasons': no room on the map of this image */
        CHECK(v->pending == 1);           /* the lane waits for the run, its result of another generation */
        v->ghost->job_pending = 0;        /* the run over, the worker moves the ghost (made here in place) */
        CHECK(oracles_ghost_switch_half(v->ghost, image, IMAGE_SIZE, seasons) == 0 && profile_of(v->ghost) == seasons);
        const unsigned stale = v->stale;
        ev_advance_pending_run(v);        /* nothing delivered after the switch: the lane free, the run dropped */
        CHECK(v->pending == 0 && v->stale == stale + 1u);
        CHECK(oracles_guest_switch_to(guest, ages) == 0);
        CHECK(profile_of(v->ghost) == ages && v->pending == 0);   /* no run in flight: the lane is free at once */
    }
    oracles_enhanced_view_stop(v);
    oracles_guest_detach(guest);
    oracles_core_destroy(core);
}

static void second_lane_waits(const uint8_t *image, const OraclesCompatProfile *ages, const OraclesCompatProfile *seasons)
{
    const OraclesCoreOptions options = { 0, 0, ORACLES_CORE_SAMEBOY, 0 };
    OraclesCore *core = oracles_core_create(image, IMAGE_SIZE, &options);
    OraclesGuest *guest = core ? oracles_guest_attach(core, ages) : NULL;
    OraclesEnhancedView *v = guest ? oracles_enhanced_view_start(core, guest, image, IMAGE_SIZE) : NULL;
    CHECK(v != NULL && v->ghost != NULL);
    if (v && v->ghost && oracles_enhanced_view_set_ghosts(v, 2) == 2) {
        OraclesGhost *second = v->lanes[1].ghost;
        second->job_pending = 1;          /* the second ghost's run outlasts the passage, the first idle */
        CHECK(oracles_guest_switch_to(guest, seasons) == 0);
        CHECK(v->lane_now == 0 && v->pending == 0 && v->lanes[1].pending == 1);
        CHECK(profile_of(v->ghost) == seasons && profile_of(second) == ages && second->half_pending == seasons);
        second->job_pending = 0;
        CHECK(oracles_ghost_switch_half(second, image, IMAGE_SIZE, seasons) == 0 && profile_of(second) == seasons);
        const unsigned stale = v->stale;
        ev_lane_serve(v, 1);
        ev_advance_pending_run(v);
        CHECK(v->pending == 0 && v->stale == stale + 1u);
        ev_lane_serve(v, 0);
        CHECK(v->lanes[1].pending == 0);
    } else CHECK(!"two ghosts");
    oracles_enhanced_view_stop(v);
    oracles_guest_detach(guest);
    oracles_core_destroy(core);
}

static void three_switches(const uint8_t *image, const OraclesCompatProfile *ages, const OraclesCompatProfile *seasons)
{
    OraclesGhost *g = oracles_ghost_create(image, IMAGE_SIZE, ages, ORACLES_CORE_SAMEBOY);
    CHECK(g != NULL);
    if (!g) return;
    /* Two switches asked while a run is in flight: the last one stands. */
    g->job_pending = 1;
    CHECK(oracles_ghost_busy(g));
    CHECK(oracles_ghost_switch_half(g, image, IMAGE_SIZE, seasons) == 0 && profile_of(g) == ages && g->half_pending == seasons);
    CHECK(oracles_ghost_switch_half(g, image, IMAGE_SIZE, ages) == 0 && g->half_pending == ages);
    g->job_pending = 0;
    CHECK(!oracles_ghost_busy(g));
    /* A third, made in place: the one the worker had not made is forgotten. */
    CHECK(oracles_ghost_switch_half(g, image, IMAGE_SIZE, seasons) == 0 && profile_of(g) == seasons && g->half_pending == NULL);
    CHECK(oracles_ghost_rooms(g)->map_room[0][0x00] == 0);
    /* A run then: the worker keeps the ghost where it is. */
    const size_t size = oracles_ghost_state_size(g);
    uint8_t *state = malloc(size);
    OraclesGhostResult result;
    CHECK(state != NULL && oracles_core_save_state(g->core, state, size) == 0);
    if (state && oracles_ghost_request(g, state, size, ORACLES_DIR_UP, 4) == 0)
        while (oracles_ghost_poll(g, &result) == 0) {}
    CHECK(profile_of(g) == seasons && !oracles_ghost_half_failed(g));
    /* A switch asked during that kind of run is made by the worker after it, its result dropped. */
    pthread_mutex_lock(&g->mutex);
    g->half_pending = ages;
    g->half_rom = image;
    g->half_rom_size = IMAGE_SIZE;
    pthread_mutex_unlock(&g->mutex);
    if (state && oracles_ghost_request(g, state, size, ORACLES_DIR_UP, 4) == 0)
        while (oracles_ghost_poll(g, &result) == 0) {}
    CHECK(profile_of(g) == ages && g->half_pending == NULL && oracles_ghost_rooms(g)->map_room[0][0x00] == 1);
    free(state);
    oracles_ghost_destroy(g);
}

int main(void)
{
    uint8_t *image = make_image();
    const OraclesRomInfo info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_COMBINED_1_5_EN, .known = 1 };
    const OraclesCompatProfile *ages = oracles_compat_find(&info), *seasons = oracles_compat_half(ages, 1);
    CHECK(image != NULL && ages != NULL && seasons != NULL);
    if (!image || !ages || !seasons) return 1;
    view_reads_its_half(image, ages, seasons);
    second_lane_waits(image, ages, seasons);
    three_switches(image, ages, seasons);
    free(image);
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_half_switch: ok\n");
    return 0;
}
