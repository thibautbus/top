/* The thread's ghost asked for a run while its worker still runs one: the request is refused as busy, apart from a
 * failure, and the view keeps the room queued without a failure's wait, its order stopped for the frame; asked again
 * once the worker is free, the run starts.  A real failure stays one.  On a synthetic image of Oracles Combined (8 MiB,
 * zero but for its header), whose states no ghost can prime. */
#include "core.h"
#include "ghost.h"
#include "ghost_internal.h"
#include "guest.h"
#include "view_internal.h"
#include "view_shared.h"

#include <pthread.h>
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

/* The worker's flag, as a run in flight sets it. */
static void set_pending(OraclesGhost *g, int pending)
{
    pthread_mutex_lock(&g->mutex);
    g->job_pending = pending;
    pthread_mutex_unlock(&g->mutex);
}

static void request_refused_busy(const uint8_t *image, const OraclesCompatProfile *ages)
{
    OraclesGhost *g = oracles_ghost_create(image, IMAGE_SIZE, ages, ORACLES_CORE_SAMEBOY);
    CHECK(g != NULL);
    if (!g) return;
    const size_t size = oracles_ghost_state_size(g);
    uint8_t *state = malloc(size);
    CHECK(state != NULL && oracles_core_save_state(g->core, state, size) == 0);
    if (state) {
        set_pending(g, 1);
        CHECK(oracles_ghost_busy(g));
        CHECK(oracles_ghost_request(g, state, size, ORACLES_DIR_UP, 4) == ORACLES_GHOST_BUSY);
        CHECK(oracles_ghost_request(g, state, size - 1u, ORACLES_DIR_UP, 4) == -1);
        set_pending(g, 0);
        OraclesGhostResult result;
        CHECK(oracles_ghost_request(g, state, size, ORACLES_DIR_UP, 4) == 0);
        while (oracles_ghost_poll(g, &result) == 0) {}
    }
    free(state);
    oracles_ghost_destroy(g);
}

static void view_waits_for_the_worker(const uint8_t *image, const OraclesCompatProfile *ages)
{
    const OraclesCoreOptions options = { 0, 0, ORACLES_CORE_SAMEBOY, 0 };
    OraclesCore *core = oracles_core_create(image, IMAGE_SIZE, &options);
    OraclesGuest *guest = core ? oracles_guest_attach(core, ages) : NULL;
    OraclesEnhancedView *v = guest ? oracles_enhanced_view_start(core, guest, image, IMAGE_SIZE) : NULL;
    CHECK(v != NULL && v->ghost != NULL && v->snapshot != NULL && v->sync_budget == 0);
    entry *e = v && v->ghost && v->snapshot ? ev_take_slot(v, 0, 0x12, NULL, 0) : NULL;
    CHECK(e != NULL);
    if (e && oracles_core_save_state(core, v->snapshot, v->state_size) == 0) {
        const unsigned requested = v->requested, failed = v->failed;
        set_pending(v->ghost, 1);
        CHECK(ev_start_job(v, e, v->snapshot, v->state_size, ORACLES_DIR_UP, NULL) == 0);
        CHECK(e->queued && !e->failed && !e->in_flight && v->pending == 0);
        CHECK(v->failed == failed && v->requested == requested && oracles_enhanced_view_busy_refusals(v) == 1u && v->ghost_busy);
        set_pending(v->ghost, 0);
        v->ghost_busy = 0;
        CHECK(ev_start_job(v, e, v->snapshot, v->state_size, ORACLES_DIR_UP, NULL) == 1);
        CHECK(e->in_flight && !e->queued && v->pending == 1 && v->requested == requested + 1u);
        while (v->pending) ev_advance_pending_run(v);
        CHECK(e->failed && e->failed_at == v->frame && v->failed == failed + 1u && oracles_enhanced_view_busy_refusals(v) == 1u);
    }
    oracles_enhanced_view_stop(v);
    oracles_guest_detach(guest);
    oracles_core_destroy(core);
}

int main(void)
{
    uint8_t *image = make_image();
    const OraclesRomInfo info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_COMBINED_1_5_EN, .known = 1 };
    const OraclesCompatProfile *ages = oracles_compat_find(&info);
    CHECK(image != NULL && ages != NULL);
    if (!image || !ages) return 1;
    request_refused_busy(image, ages);
    view_waits_for_the_worker(image, ages);
    free(image);
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_ghost_busy: ok\n");
    return 0;
}
