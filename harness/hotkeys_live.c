#include "hotkeys_live.h"

#include "hotkey_lines.h"
#include "item_hotkeys.h"
#include "route_effects.h"
#include "hotbar_model.h"

#include <stdlib.h>
#include <string.h>

struct OraclesHotkeysLive {
    OraclesGuest *guest;
    OraclesItemHotkeys *policy;
    OraclesHotkeysLiveOptions options;
    OraclesHotkeySlot slots[ORACLES_HOTKEY_SLOTS];
    OraclesRouteWriter writer;
    int recording, write_failed;
    /* a use is effective when the item of the slot is in use within a second of its press */
    struct { int waiting; uint8_t item; uint32_t until; } watch;
    unsigned uses_effective;
};

int oracles_hotkeys_live_parse_press(OraclesHotkeysLiveOptions *o, const char *text)
{
    unsigned long frame = 0, slot = 0, frames = 0;
    char *end = NULL;
    frame = strtoul(text, &end, 10);
    if (*end != ':') return 0;
    slot = strtoul(end + 1, &end, 10);
    if (*end != ':') return 0;
    frames = strtoul(end + 1, &end, 10);
    if (*end || slot < 1 || slot > ORACLES_HOTKEY_SLOTS || !frames || o->press_count >= HOTKEYS_LIVE_MAX_PRESSES) return 0;
    o->presses[o->press_count].frame = (uint32_t)frame;
    o->presses[o->press_count].slot = (unsigned)slot - 1u;
    o->presses[o->press_count].frames = (uint32_t)frames;
    o->press_count++;
    return 1;
}

static void on_applied(void *opaque, const OraclesGuestInventoryState *before, const OraclesGuestInventoryOp *op)
{
    OraclesHotkeysLive *l = opaque;
    if (!l->recording) return;
    OraclesRouteAction action;
    oracles_route_action_of_exchange(before, op, &action);
    if (oracles_route_writer_action(&l->writer, &action) != 0) l->write_failed = 1;
}

static void on_use_started(void *opaque, uint32_t frame, unsigned button_slot, uint8_t item)
{
    OraclesHotkeysLive *l = opaque;
    l->watch.waiting = 1; l->watch.item = item; l->watch.until = frame + 60u;
    if (!l->recording) return;
    OraclesRouteAction action;
    memset(&action, 0, sizeof action);
    action.frame = frame; action.verb = ORACLES_ROUTE_USE; action.button = (uint8_t)button_slot; action.item = item;
    if (oracles_route_writer_action(&l->writer, &action) != 0) l->write_failed = 1;
}

OraclesHotkeysLive *oracles_hotkeys_live_start(OraclesGuest *guest, const OraclesHotkeysLiveOptions *options, const OraclesRouteHeader *header)
{
    OraclesHotkeysLive *l = calloc(1, sizeof *l);
    if (!l) return NULL;
    l->guest = guest;
    l->options = *options;
    const OraclesHotkeysMode mode = !strcmp(options->mode, "equip") ? ORACLES_HOTKEYS_EQUIP : ORACLES_HOTKEYS_USE;
    l->policy = oracles_item_hotkeys_start(guest, mode);
    if (!l->policy) { free(l); return NULL; }
    for (unsigned n = 0; n < ORACLES_HOTKEY_SLOTS; n++) {
        unsigned game = 0, slot = 0;
        if (!options->slots[n]) continue;
        char name[] = "hotkey_ages_1";
        name[12] = (char)('1' + n);
        if (!oracles_hotkey_line_parse(name, options->slots[n], &game, &slot, &l->slots[n])) { fprintf(stderr, "harness: --hotkey-slot %u: '%s' is not <b|a>:<item>:<variant or -->\n", n + 1u, options->slots[n]); oracles_item_hotkeys_stop(l->policy); free(l); return NULL; }
        oracles_item_hotkeys_set_slot(l->policy, n, &l->slots[n]);
    }
    const OraclesItemHotkeysSink sink = { on_applied, on_use_started, l };
    oracles_item_hotkeys_set_sink(l->policy, &sink);
    if (options->record_path) {
        if (oracles_route_writer_open(&l->writer, options->record_path, header) != 0) { fprintf(stderr, "harness: cannot write %s\n", options->record_path); oracles_item_hotkeys_stop(l->policy); free(l); return NULL; }
        l->recording = 1;
    }
    return l;
}

unsigned oracles_hotkeys_live_keys(OraclesHotkeysLive *l, uint32_t frame, unsigned route_mask)
{
    for (unsigned i = 0; i < l->options.press_count; i++) {
        if (l->options.presses[i].frame == frame) oracles_item_hotkeys_key(l->policy, l->options.presses[i].slot, 1, ORACLES_HOTKEY_PLAIN);
        if (l->options.presses[i].frame + l->options.presses[i].frames == frame) oracles_item_hotkeys_key(l->policy, l->options.presses[i].slot, 0, ORACLES_HOTKEY_PLAIN);
    }
    const unsigned keys = oracles_item_hotkeys_filter_keys(l->policy, frame, route_mask) & 0xffu;
    if (l->recording) oracles_route_writer_record(&l->writer, frame, keys);
    return keys;
}

int oracles_hotkeys_slots_hotbar(const OraclesHotkeysLiveOptions *options, struct OraclesEnhancedView *view)
{
    static const char keys[ORACLES_HOTKEY_SLOTS] = { 'A', 'S', 'Q', 'W' };
    OraclesEnhancedHotbar hotbar;
    memset(&hotbar, 0, sizeof hotbar);
    for (unsigned n = 0; n < ORACLES_HOTKEY_SLOTS; n++) {
        hotbar.slots[n].key[0] = keys[n];
        hotbar.slots[n].variant = ORACLES_HOTKEY_NO_VARIANT;
        if (!options->slots[n]) continue;
        unsigned game = 0, slot = 0;
        OraclesHotkeySlot parsed;
        char name[] = "hotkey_ages_1";
        name[12] = (char)('1' + n);
        if (!oracles_hotkey_line_parse(name, options->slots[n], &game, &slot, &parsed)) return 0;
        hotbar.slots[n].set = parsed.set; hotbar.slots[n].item = parsed.item; hotbar.slots[n].variant = parsed.variant;
    }
    oracles_enhanced_view_set_hotbar((OraclesEnhancedView *)view, &hotbar);
    oracles_enhanced_view_hotbar_verify((OraclesEnhancedView *)view, 1);
    return 1;
}

void oracles_hotkeys_live_hotbar(OraclesHotkeysLive *l, struct OraclesEnhancedView *view)
{
    if (!view) return;
    static const char *const names[ORACLES_HOTKEY_SLOTS] = { "A", "S", "Q", "W" };
    OraclesEnhancedHotbar hotbar;
    oracles_hotbar_model(l->policy, names, 0, &hotbar);
    oracles_enhanced_view_set_hotbar((OraclesEnhancedView *)view, &hotbar);
    oracles_enhanced_view_hotbar_verify((OraclesEnhancedView *)view, 1);
}

void oracles_hotkeys_live_frame_end(OraclesHotkeysLive *l, uint32_t frame)
{
    if (!l->watch.waiting) return;
    OraclesGuestInventoryState s;
    oracles_guest_inventory_state(l->guest, &s);
    for (unsigned i = 0; i < 5u; i++) if (s.parents[i].enabled && s.parents[i].id == l->watch.item) { l->uses_effective++; l->watch.waiting = 0; return; }
    if (frame >= l->watch.until) l->watch.waiting = 0;
}

void oracles_hotkeys_live_summary(OraclesHotkeysLive *l, FILE *out)
{
    OraclesItemHotkeysLiveStats stats;
    unsigned applied = 0, violations = 0;
    oracles_item_hotkeys_live_stats(l->policy, &stats);
    oracles_guest_inventory_counts(l->guest, &applied, &violations);
    fprintf(out, "hotkeys.live.requests=%u\nhotkeys.live.applied=%u\nhotkeys.live.uses=%u\nhotkeys.live.uses_effective=%u\nhotkeys.live.returns=%u\n"
                 "hotkeys.live.returns_given_up=%u\nhotkeys.live.dropped_postponed=%u\nhotkeys.live.dropped_refused=%u\nhotkeys.live.inventory_violations=%u\n"
                 "hotkeys.live.returns_pending_at_end=%u\nhotkeys.live.return_longest_delay=%u\n",
            stats.requests, stats.applied, stats.uses, l->uses_effective, stats.returns, stats.returns_abandoned, stats.dropped_postponed, stats.dropped_refused, violations,
            stats.returns_pending, stats.return_longest_delay);
    for (unsigned v = 0; v < ORACLES_INVENTORY_VERDICTS; v++)
        if (stats.dropped_by[v]) fprintf(out, "hotkeys.live.dropped.%s=%u\n", oracles_guest_inventory_verdict_name((OraclesGuestInventoryVerdict)v), stats.dropped_by[v]);
}

int oracles_hotkeys_live_stop(OraclesHotkeysLive *l, FILE *report)
{
    OraclesItemHotkeysLiveStats stats;
    unsigned applied = 0, violations = 0;
    oracles_item_hotkeys_live_stats(l->policy, &stats);
    oracles_guest_inventory_counts(l->guest, &applied, &violations);
    if (report) fprintf(report, "item hotkeys, live: %u key presses, %u exchanges applied, %u uses of which %u put the item in use, %u returns (%u given up), %u dropped after waiting, %u refused; inventory violations %u\n",
                        stats.requests, stats.applied, stats.uses, l->uses_effective, stats.returns, stats.returns_abandoned, stats.dropped_postponed, stats.dropped_refused, violations);
    int failed = violations != 0 || l->write_failed;
    if (l->recording && oracles_route_writer_close(&l->writer) != 0) failed = 1;
    oracles_item_hotkeys_stop(l->policy);
    free(l);
    return failed;
}
