#include "hotkeys_check.h"

#include "item_hotkeys.h"
#include "route_effects.h"
#include "core.h"

#include <stdlib.h>
#include <string.h>

struct OraclesHotkeysCheck {
    OraclesGuest *guest;
    OraclesItemHotkeysReplay *replay;
    FILE *tsv;
    unsigned applied_seen;             /* the replay's count at the last write point */
    int refresh_pending;               /* an exchange was applied: loadEquippedItemGfx is due before the next write point */
    uint32_t refresh_pending_frame;
    int menu_opened_since;             /* a menu opened at that turn: the bar is repaired on the way out */
    unsigned refreshes_seen, refresh_missed;
    uint32_t first_refresh_missed_frame;
    unsigned write_points;
    unsigned write_points_this_frame;   /* a frame the game's loop did not finish has none: no exchange can be noted there */
    /* each `use` of the route: did the game put that item in use before the press was over? */
    const OraclesRoute *route;
    struct { int active, fired; uint8_t item; unsigned key; uint32_t since, released_at; } use;
    unsigned uses, uses_not_fired, uses_already_in_use, uses_side_view, uses_underwater;   /* the last two: presses begun in those states of fact 10 */
    unsigned read_at_write_point;      /* the item buttons the game read, as the policy sees them at the write point; 0xff: none this frame */
    uint32_t uses_started_to;          /* the frames whose `use` lines have been taken up, this one excluded */
    char not_fired_list[256];
};

static void start_uses(OraclesHotkeysCheck *c, uint32_t frame);

#define USE_GRACE 8u   /* frames after the button comes up in which the item may still appear: the press is read a turn after the mask */

OraclesHotkeysCheck *oracles_hotkeys_check_start(OraclesGuest *guest, const OraclesRoute *route, const char *dir)
{
    OraclesHotkeysCheck *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->guest = guest;
    c->route = route;
    OraclesItemHotkeysEffect *effects = NULL;
    const size_t count = oracles_route_effects(route, &effects);
    c->replay = oracles_item_hotkeys_replay_start(guest, effects, count);
    free(effects);
    if (!c->replay) { free(c); return NULL; }
    if (dir) {
        char path[4096];
        snprintf(path, sizeof path, "%s/inventory.tsv", dir);
        c->tsv = fopen(path, "w");
        if (c->tsv) fprintf(c->tsv, "frame\tkeys\twrite_points\texchange_b\texchange_a\titems_in_use\tfirst_storage\tslots\tparents\titem_buttons\tswimming\tbuttons_at_write_point\n");
    }
    return c;
}

void oracles_hotkeys_check_event(OraclesHotkeysCheck *c, const OraclesGuestEvent *event)
{
    start_uses(c, event->frame);
    if (event->type == ORACLES_EVENT_ITEM_STARTED) { if (c->use.active && event->e == c->use.item) c->use.fired = 1; return; }
    if (event->type == ORACLES_EVENT_EQUIPPED_GFX_LOADED) { c->refreshes_seen++; c->refresh_pending = 0; return; }
    if (event->type == ORACLES_EVENT_MENU) { c->menu_opened_since = 1; return; }
    if (event->type != ORACLES_EVENT_STATUS_BAR_CHECKED) return;
    /* A write point: the turn after an exchange is over, and the status bar should have reloaded its icons. */
    c->write_points++;
    c->write_points_this_frame++;
    if (c->tsv) { OraclesGuestInventoryState at; oracles_guest_inventory_state(c->guest, &at); c->read_at_write_point = oracles_guest_item_buttons(&at); }
    /* A menu open at this turn took the place of the status bar's update, whenever openMenu was entered (the player
     * may press Start on the very frame of an exchange, before its write point): the bar is repaired on the way out. */
    if (c->refresh_pending && !c->menu_opened_since) {
        OraclesGuestInventoryState now;
        oracles_guest_inventory_state(c->guest, &now);
        if (now.opened_menu_type) c->menu_opened_since = 1;
    }
    if (c->refresh_pending && !c->menu_opened_since) {
        if (!c->refresh_missed) c->first_refresh_missed_frame = c->refresh_pending_frame;
        c->refresh_missed++;
    }
    c->refresh_pending = 0;
    OraclesItemHotkeysStats stats;
    oracles_item_hotkeys_replay_stats(c->replay, &stats);
    if (stats.applied != c->applied_seen) { c->applied_seen = stats.applied; c->refresh_pending = 1; c->refresh_pending_frame = event->frame; c->menu_opened_since = 0; }
}

static void close_use(OraclesHotkeysCheck *c)
{
    if (!c->use.active) return;
    c->use.active = 0;
    if (c->use.fired) return;
    c->uses_not_fired++;
    const size_t used = strlen(c->not_fired_list);
    if (used + 16 < sizeof c->not_fired_list) snprintf(c->not_fired_list + used, sizeof c->not_fired_list - used, "%s%u:%02x", used ? " " : "", c->use.since, c->use.item);
}

/* A simulated press the route noted is followed until its button comes up, and a little after: the game must have
 * started that item meanwhile (initializeParentItem, seen as it runs: the feather of Ages starts and ends within one
 * turn, which a look at the end of the frame misses).  One the game declines by itself is listed with its frame, to be
 * explained: the item already in use (a second press while the boomerang flies), an empty satchel. */
/* The `use` lines of a frame are taken up at its first event, before the game reads the joypad: the item starts
 * within the very frame the press begins. */
static void start_uses(OraclesHotkeysCheck *c, uint32_t frame)
{
    if (frame < c->uses_started_to) return;
    c->uses_started_to = frame + 1u;
    size_t first = 0;
    const size_t count = oracles_route_actions_at(c->route, frame, &first);
    for (size_t i = 0; i < count; i++) {
        const OraclesRouteAction *a = &c->route->actions[first + i];
        if (a->verb != ORACLES_ROUTE_USE) continue;
        close_use(c);
        c->uses++;
        c->use.active = 1; c->use.fired = 0; c->use.item = a->item; c->use.since = frame; c->use.released_at = 0;
        /* The item already in use when the press begins (the shield kept up by its key across a scroll): the press
         * keeps it in use, and the game starts nothing anew. */
        OraclesGuestInventoryState s;
        oracles_guest_inventory_state(c->guest, &s);
        for (unsigned p = 0; p < 5u; p++) if (s.parents[p].enabled && s.parents[p].id == a->item) { c->use.fired = 1; c->uses_already_in_use++; }
        if (s.tileset_flags & (1u << s.tilesetflag_bit_sidescroll)) c->uses_side_view++;
        if (s.tilesetflag_bit_underwater != 0xffu && (s.tileset_flags & (1u << s.tilesetflag_bit_underwater))) c->uses_underwater++;
        c->use.key = a->button == ORACLES_INVENTORY_SLOT_B ? ORACLES_KEY_B : ORACLES_KEY_A;
    }
}

static void follow_uses(OraclesHotkeysCheck *c, uint32_t frame, unsigned keys)
{
    start_uses(c, frame);
    if (!c->use.active) return;
    if (keys & c->use.key) c->use.released_at = 0;
    else if (!c->use.released_at) c->use.released_at = frame;
    if (c->use.fired || (c->use.released_at && frame >= c->use.released_at + USE_GRACE)) close_use(c);
}

/* What the guest would answer now to the exchange of each button with the
 * first occupied storage slot, and how many items are in use: where a
 * working route may place an exchange. */
void oracles_hotkeys_check_frame_end(OraclesHotkeysCheck *c, uint32_t frame, unsigned keys)
{
    const unsigned points = c->write_points_this_frame;
    c->write_points_this_frame = 0;
    OraclesGuestInventoryState s;
    oracles_guest_inventory_state(c->guest, &s);
    follow_uses(c, frame, keys);
    if (!c->tsv) return;
    unsigned storage = ORACLES_INVENTORY_SLOTS;
    for (unsigned i = 2; i < ORACLES_INVENTORY_SLOTS && storage == ORACLES_INVENTORY_SLOTS; i++) if (s.slots[i] && s.slots[i] < s.num_inventory_items && s.slots[i] != s.item_biggoron_sword) storage = i;   /* the Biggoron sword takes both buttons: never exchanged */
    const char *verdicts[2] = { "-", "-" };
    unsigned in_use = 0;
    for (unsigned i = 0; i < 5u; i++) in_use += s.parents[i].enabled != 0;
    for (unsigned k = 0; storage < ORACLES_INVENTORY_SLOTS && k < 2u; k++) {
        OraclesGuestInventoryOp op;
        memset(&op, 0, sizeof op);
        op.swap = 1; op.slot_a = (uint8_t)k; op.slot_b = (uint8_t)storage;
        verdicts[k] = oracles_guest_inventory_verdict_name(oracles_guest_inventory_check(&s, &op));
    }
    fprintf(c->tsv, "%u\t%02x\t%u\t%s\t%s\t%u\t%d\t", frame, keys & 0xffu, points, verdicts[0], verdicts[1], in_use, storage < ORACLES_INVENTORY_SLOTS ? (int)storage : -1);
    for (unsigned i = 0; i < ORACLES_INVENTORY_SLOTS; i++) fprintf(c->tsv, "%s%02x", i ? " " : "", s.slots[i]);
    fprintf(c->tsv, "\t");
    for (unsigned i = 0; i < 5u; i++) fprintf(c->tsv, "%s%02x:%02x:%02x", i ? " " : "", s.parents[i].enabled, s.parents[i].id, s.parents[i].button);
    const unsigned read = oracles_guest_item_buttons(&s);   /* the buttons checkUseItems would read now (fact 10) */
    fprintf(c->tsv, "\t%s%s%s\t%02x\t", read & ORACLES_BUTTON_B ? "B" : "", read & ORACLES_BUTTON_A ? "A" : "", read ? "" : "-", s.link_swimming_state);
    if (c->read_at_write_point == 0xffu) fprintf(c->tsv, "none\n");
    else fprintf(c->tsv, "%s%s%s\n", c->read_at_write_point & ORACLES_BUTTON_B ? "B" : "", c->read_at_write_point & ORACLES_BUTTON_A ? "A" : "", c->read_at_write_point ? "" : "-");
    c->read_at_write_point = 0xffu;
}

void oracles_hotkeys_check_summary(OraclesHotkeysCheck *c, FILE *out)
{
    OraclesItemHotkeysStats stats;
    unsigned applied = 0, violations = 0;
    oracles_item_hotkeys_replay_stats(c->replay, &stats);
    oracles_guest_inventory_counts(c->guest, &applied, &violations);
    close_use(c);
    fprintf(out, "hotkeys.uses=%u\nhotkeys.use_not_fired=%u\nhotkeys.use_already_in_use=%u\nhotkeys.uses_side_view=%u\nhotkeys.uses_underwater=%u\n", c->uses, c->uses_not_fired, c->uses_already_in_use, c->uses_side_view, c->uses_underwater);
    fprintf(out, "hotkeys.applied=%u\nhotkeys.replay_mismatch=%u\nhotkeys.missed_write_point=%u\nhotkeys.inventory_violations=%u\nhotkeys.refresh_missed=%u\nhotkeys.refreshes=%u\nhotkeys.write_points=%u\n",
            stats.applied, stats.replay_mismatch, stats.missed_write_point, violations, c->refresh_missed, c->refreshes_seen, c->write_points);
    for (unsigned v = ORACLES_INVENTORY_NOTHING; v < ORACLES_INVENTORY_VERDICTS; v++)
        if (stats.verdicts[v]) fprintf(out, "hotkeys.refused.%s=%u\n", oracles_guest_inventory_verdict_name((OraclesGuestInventoryVerdict)v), stats.verdicts[v]);
}

void oracles_hotkeys_check_report(OraclesHotkeysCheck *c, FILE *out)
{
    OraclesItemHotkeysStats stats;
    unsigned applied = 0, violations = 0;
    oracles_item_hotkeys_replay_stats(c->replay, &stats);
    oracles_guest_inventory_counts(c->guest, &applied, &violations);
    fprintf(out, "item hotkeys: %u exchanges of the route applied over %u write points, %u not applied as noted", stats.applied, c->write_points, stats.replay_mismatch);
    if (stats.replay_mismatch) fprintf(out, " (the first at frame %u: %s)", stats.first_mismatch_frame, oracles_guest_inventory_verdict_name(stats.first_mismatch_verdict));
    fprintf(out, "; inventory violations %u; status bar refreshes missed at the turn after an exchange %u", violations, c->refresh_missed);
    if (c->refresh_missed) fprintf(out, " (the first after frame %u)", c->first_refresh_missed_frame);
    fprintf(out, "\n");
    close_use(c);
    if (c->uses) fprintf(out, "item hotkeys: %u simulated presses of the route, %u after which the item was not seen in use%s%s\n", c->uses, c->uses_not_fired, c->uses_not_fired ? " (frame:item) " : "", c->not_fired_list);
}

int oracles_hotkeys_check_failed(OraclesHotkeysCheck *c)
{
    OraclesItemHotkeysStats stats;
    unsigned applied = 0, violations = 0;
    oracles_item_hotkeys_replay_stats(c->replay, &stats);
    oracles_guest_inventory_counts(c->guest, &applied, &violations);
    return stats.replay_mismatch || stats.missed_write_point || violations || c->refresh_missed;
}

void oracles_hotkeys_check_stop(OraclesHotkeysCheck *c)
{
    if (!c) return;
    if (c->tsv) fclose(c->tsv);
    oracles_item_hotkeys_replay_stop(c->replay);
    free(c);
}
