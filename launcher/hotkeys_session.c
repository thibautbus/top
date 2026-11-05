#include "hotkeys_session.h"

#include "route_effects.h"
#include "hotbar_model.h"

#include <stdlib.h>
#include <string.h>

struct OraclesHotkeysSession {
    OraclesGuest *guest;
    unsigned game;                       /* 0 ages, 1 seasons: the slots of settings.txt */
    OraclesHotkeysMode mode, next;       /* next: the mode asked for, taken at rest */
    oracles_settings *settings;
    OraclesItemHotkeys *live;            /* NULL while a route is replayed */
    OraclesItemHotkeysReplay *replay;
    OraclesRouteWriter *writer;
    int controller;                      /* the last slot key came from a controller: the hotbar shows its buttons */
};

static void on_applied(void *opaque, const OraclesGuestInventoryState *before, const OraclesGuestInventoryOp *op)
{
    OraclesHotkeysSession *s = opaque;
    if (!s->writer) return;
    OraclesRouteAction action;
    oracles_route_action_of_exchange(before, op, &action);
    oracles_route_writer_action(s->writer, &action);
}

static void on_use_started(void *opaque, uint32_t frame, unsigned button_slot, uint8_t item)
{
    OraclesHotkeysSession *s = opaque;
    if (!s->writer) return;
    OraclesRouteAction action;
    memset(&action, 0, sizeof action);
    action.frame = frame;
    action.verb = ORACLES_ROUTE_USE;
    action.button = (uint8_t)button_slot;
    action.item = item;
    oracles_route_writer_action(s->writer, &action);
}

static void on_sound(void *opaque, uint32_t frame, uint8_t sound)
{
    OraclesHotkeysSession *s = opaque;
    if (!s->writer) return;
    OraclesRouteAction action;
    memset(&action, 0, sizeof action);
    action.frame = frame;
    action.verb = ORACLES_ROUTE_SOUND;
    action.item = sound;
    oracles_route_writer_action(s->writer, &action);
}

static void on_throw_started(void *opaque, uint32_t frame, unsigned button_slot)
{
    OraclesHotkeysSession *s = opaque;
    if (!s->writer) return;
    OraclesRouteAction action;
    memset(&action, 0, sizeof action);
    action.frame = frame;
    action.verb = ORACLES_ROUTE_THROW;
    action.button = (uint8_t)button_slot;
    oracles_route_writer_action(s->writer, &action);
}

static int start_live(OraclesHotkeysSession *s)
{
    s->live = oracles_item_hotkeys_start(s->guest, s->mode);
    if (!s->live) return 0;
    const OraclesItemHotkeysSink sink = { on_applied, on_use_started, on_sound, on_throw_started, s };
    oracles_item_hotkeys_set_sink(s->live, &sink);
    for (unsigned n = 0; n < ORACLES_HOTKEY_SLOTS; n++) oracles_item_hotkeys_set_slot(s->live, n, &s->settings->hotkeys[s->game][n]);
    return 1;
}

OraclesHotkeysSession *oracles_hotkeys_session_start(OraclesGuest *guest, OraclesGame game, OraclesHotkeysMode mode,
                                                     oracles_settings *settings, const OraclesRoute *route)
{
    OraclesHotkeysSession *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->guest = guest;
    s->game = game == ORACLES_GAME_AGES ? 0u : 1u;
    s->mode = s->next = mode;
    s->settings = settings;
    if (route) {
        OraclesItemHotkeysEffect *effects = NULL;
        const size_t count = oracles_route_effects(route, &effects);
        s->replay = oracles_item_hotkeys_replay_start(guest, effects, count);
        free(effects);
        if (!s->replay) { free(s); return NULL; }
        OraclesItemHotkeysSound *sounds = NULL;
        const size_t sound_count = oracles_route_sounds(route, &sounds);
        const int armed = oracles_item_hotkeys_replay_sounds(s->replay, sounds, sound_count);
        free(sounds);
        if (armed != 0) { oracles_item_hotkeys_replay_stop(s->replay); free(s); return NULL; }
    } else if (!start_live(s)) { free(s); return NULL; }
    return s;
}

static void end_replay(OraclesHotkeysSession *s)
{
    OraclesItemHotkeysStats stats;
    oracles_item_hotkeys_replay_stats(s->replay, &stats);
    if (stats.applied || stats.sounds || stats.replay_mismatch)
        fprintf(stderr, "oracles: item hotkeys: %u exchanges and %u sounds of the route applied, %u not applied as noted\n", stats.applied, stats.sounds, stats.replay_mismatch);
    oracles_item_hotkeys_replay_stop(s->replay);
    s->replay = NULL;
}

void oracles_hotkeys_session_route_over(OraclesHotkeysSession *s)
{
    if (!s || !s->replay) return;
    end_replay(s);
    if (s->mode != ORACLES_HOTKEYS_OFF && !start_live(s)) fprintf(stderr, "oracles: item hotkeys: the policy could not start after the route\n");
}

void oracles_hotkeys_session_record(OraclesHotkeysSession *s, OraclesRouteWriter *writer) { if (s) s->writer = writer; }

static const char *slot_text(const OraclesHotkeySlot *slot, char *out, size_t capacity)
{
    if (!slot->set) snprintf(out, capacity, "empty");
    else if (slot->variant == ORACLES_HOTKEY_NO_VARIANT) snprintf(out, capacity, "item %02x, target %c", slot->item, slot->target == ORACLES_INVENTORY_SLOT_A ? 'A' : 'B');
    else snprintf(out, capacity, "item %02x with variant %02x, target %c", slot->item, slot->variant, slot->target == ORACLES_INVENTORY_SLOT_A ? 'A' : 'B');
    return out;
}

void oracles_hotkeys_session_command(OraclesHotkeysSession *s, int command)
{
    if (!s || !s->live || command < ORACLES_COMMAND_HOTKEY || command >= ORACLES_COMMAND_HOTKEY_END) return;
    const unsigned code = (unsigned)(command - ORACLES_COMMAND_HOTKEY), n = code / 8u, modifier = (code % 8u) / 2u;
    if (code & 1u) s->controller = modifier == 3u;
    const OraclesHotkeyOutcome outcome = oracles_item_hotkeys_key(s->live, n, (int)(code & 1u),
        modifier == 2u ? ORACLES_HOTKEY_BIND_A : modifier == 1u ? ORACLES_HOTKEY_BIND_B : ORACLES_HOTKEY_PLAIN);
    if (outcome != ORACLES_HOTKEY_ASSIGNED && outcome != ORACLES_HOTKEY_EMPTIED) return;
    /* An assignment writes nothing in the game: it is the launcher's, and it is remembered. */
    oracles_item_hotkeys_slot(s->live, n, &s->settings->hotkeys[s->game][n]);
    oracles_settings_store(s->settings);
    char text[64];
    fprintf(stderr, "oracles: item hotkey %u: %s\n", n + 1u, slot_text(&s->settings->hotkeys[s->game][n], text, sizeof text));
}

void oracles_hotkeys_session_frame(OraclesHotkeysSession *s, struct OraclesEnhancedView *view)
{
    if (!s || !view) return;
    if (!s->live) { oracles_enhanced_view_set_hotbar((OraclesEnhancedView *)view, NULL); return; }   /* a route replayed: the keys do nothing */
    const char *names[ORACLES_HOTKEY_SLOTS];
    for (unsigned n = 0; n < ORACLES_HOTKEY_SLOTS; n++) names[n] = s->controller ? s->settings->hotkey_pad_names[n] : s->settings->hotkey_key_names[n];
    OraclesEnhancedHotbar hotbar;
    oracles_hotbar_model(s->live, names, s->controller, &hotbar);
    oracles_enhanced_view_set_hotbar((OraclesEnhancedView *)view, &hotbar);
}

unsigned oracles_hotkeys_session_filter(void *opaque, uint32_t frame, unsigned mask)
{
    OraclesHotkeysSession *s = opaque;
    if (s && s->replay) oracles_item_hotkeys_replay_frame(s->replay, frame);
    return s && s->live ? oracles_item_hotkeys_filter_keys(s->live, frame, mask) : mask;
}

static void stop_live(OraclesHotkeysSession *s, FILE *report)
{
    OraclesItemHotkeysLiveStats stats;
    unsigned applied = 0, violations = 0;
    oracles_item_hotkeys_live_stats(s->live, &stats);
    oracles_guest_inventory_counts(s->guest, &applied, &violations);
    if (report) fprintf(report, "oracles: item hotkeys: %u key presses, %u exchanges applied, %u uses, %u returns (%u given up, %u still waiting at the end, the latest %u write points after its quiet spell), %u requests dropped after waiting, %u refused, %u equip sounds; inventory violations %u\n",
                        stats.requests, stats.applied, stats.uses, stats.returns, stats.returns_abandoned, stats.returns_pending, stats.return_longest_delay, stats.dropped_postponed, stats.dropped_refused, stats.sounds, violations);
    oracles_item_hotkeys_stop(s->live);
    s->live = NULL;
}

void oracles_hotkeys_session_set_mode(OraclesHotkeysSession *s, OraclesHotkeysMode mode)
{
    if (!s) return;
    s->next = mode;
    if (!s->live) { if (s->replay) s->mode = mode; return; }
    /* The policy stops: a key held now would keep a request or a press going, its release never coming. */
    if (mode != s->mode)
        for (unsigned n = 0; n < ORACLES_HOTKEY_SLOTS; n++) oracles_item_hotkeys_key(s->live, n, 0, ORACLES_HOTKEY_PLAIN);
}

int oracles_hotkeys_session_settle(OraclesHotkeysSession *s, FILE *report)
{
    if (!s || s->replay || s->next == s->mode) return 0;
    if (s->live) {
        if (!oracles_item_hotkeys_at_rest(s->live)) return 0;
        stop_live(s, report);
    }
    s->mode = s->next;
    if (s->mode != ORACLES_HOTKEYS_OFF && !start_live(s)) { s->mode = s->next = ORACLES_HOTKEYS_OFF; return -1; }
    return 1;
}

OraclesHotkeysMode oracles_hotkeys_session_mode(const OraclesHotkeysSession *s) { return s ? s->mode : ORACLES_HOTKEYS_OFF; }
OraclesHotkeysMode oracles_hotkeys_session_next_mode(const OraclesHotkeysSession *s) { return s ? s->next : ORACLES_HOTKEYS_OFF; }

void oracles_hotkeys_session_stop(OraclesHotkeysSession *s, FILE *report)
{
    if (!s) return;
    if (s->replay) end_replay(s);
    if (s->live) stop_live(s, report);
    free(s);
}
