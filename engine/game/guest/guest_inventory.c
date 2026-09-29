/* The inventory transaction of the item hotkeys: the second closed
 * path by which the host writes into the live instance.  An exchange of two
 * of the eighteen inventory slots, one of them a button, as the inventory
 * menu's @equipItem makes it, and a seed or song variant as its submenu
 * writes it; applied at the return of checkReloadStatusBarGraphics from its
 * one call in mainThreadStart, in the states where the player could open the
 * inventory.  Never a new value in a slot, never a register, a stack or a
 * bank. */
#include "guest_internal.h"

static const char *const verdict_names[ORACLES_INVENTORY_VERDICTS] = {
    "applied", "nothing", "not-in-play", "menu-open", "text", "death", "intro", "simulated-input", "minigame", "boxing",
    "biggoron", "inconsistent", "operation", "expectation", "variant",
    "scroll", "menu-disabled", "collisions-disabled", "instrument", "item-in-use", "status-bar-hidden", "no-button" };

const char *oracles_guest_inventory_verdict_name(OraclesGuestInventoryVerdict v)
{
    return (unsigned)v < ORACLES_INVENTORY_VERDICTS ? verdict_names[v] : "?";
}

int oracles_guest_inventory_verdict_postpones(OraclesGuestInventoryVerdict v) { return v >= ORACLES_INVENTORY_POSTPONED_SCROLL && v < ORACLES_INVENTORY_VERDICTS; }

int oracles_guest_inventory_consistent(const uint8_t slots[ORACLES_INVENTORY_SLOTS])
{
    for (unsigned i = 0; i < ORACLES_INVENTORY_SLOTS; i++)
        for (unsigned k = i + 1u; slots[i] && k < ORACLES_INVENTORY_SLOTS; k++)
            if (slots[k] == slots[i]) return 0;
    return 1;
}

/* An item in use while an exchange is asked: the game
 * gives a parent item the button whose slot holds its id, in Item.var03, and
 * only recomputes it when the menu closes.  An exchange is postponed when an
 * item in use answers to an exchanged button or is one of the exchanged
 * items; an item in use on the other button keeps a right var03 and goes on
 * (the shield held while the other button changes), as routes played with
 * the item hotkeys showed on the screen and in the live state.  A variant alone is
 * postponed while its item is in use. */
static unsigned variant_item(const OraclesGuestInventoryState *s, unsigned variant)
{
    switch (variant) {
    case ORACLES_VARIANT_SATCHEL: return s->item_seed_satchel;
    case ORACLES_VARIANT_SHOOTER: return s->item_seed_shooter;
    case ORACLES_VARIANT_NONE: return 0u;
    default: return s->item_harp;
    }
}

static int item_in_use(const OraclesGuestInventoryState *s, const OraclesGuestInventoryOp *op)
{
    const unsigned variant = variant_item(s, op->variant);
    for (unsigned i = 0; i < 5u; i++) {
        if (!s->parents[i].enabled) continue;
        if (variant && s->parents[i].id == variant) return 1;
        if (!op->swap) continue;
        const unsigned slots[2] = { op->slot_a, op->slot_b };
        for (unsigned k = 0; k < 2u; k++) {
            const unsigned button = slots[k] == ORACLES_INVENTORY_SLOT_B ? ORACLES_BUTTON_B : slots[k] == ORACLES_INVENTORY_SLOT_A ? ORACLES_BUTTON_A : 0u;
            if (button && (s->parents[i].button & button)) return 1;
            if (s->slots[slots[k]] && s->slots[slots[k]] == s->parents[i].id) return 1;
        }
    }
    return 0;
}

/* The lasting conditions, in their order: the state is not one where the player could open the inventory. */
OraclesGuestInventoryVerdict oracles_guest_inventory_refusal(const OraclesGuestInventoryState *s)
{
    /* A scroll between two rooms is play too: the game runs it under CUTSCENE_LOADING_ROOM, and the request made during
     * it waits for its end (the passing condition on wScrollMode below) instead of being refused.  A warp loads its
     * room under the same cutscene without scrolling, and is not play: the game gives it wScrollMode $02, the
     * transition that does not scroll, where a scroll has bit 2 or 3. */
    const int scrolling = s->cutscene_index == s->cutscene_loading_room && (s->scroll_mode & 0x0cu);
    const int in_play = s->game_state == 2u && (s->cutscene_index == s->cutscene_ingame || scrolling
                        || (s->game == ORACLES_GAME_SEASONS && s->cutscene_index == s->cutscene_onox_final_form));
    if (!in_play) return ORACLES_INVENTORY_REFUSED_NOT_IN_PLAY;
    if (s->opened_menu_type) return ORACLES_INVENTORY_REFUSED_MENU_OPEN;
    if (s->text_is_active) return ORACLES_INVENTORY_REFUSED_TEXT;
    if (s->link_death_trigger) return ORACLES_INVENTORY_REFUSED_DEATH;
    if (!s->intro_done) return ORACLES_INVENTORY_REFUSED_INTRO;
    if (s->use_simulated_input == 1u) return ORACLES_INVENTORY_REFUSED_SIMULATED_INPUT;   /* 2 inverts the directions against Ganon: play */
    if (s->minigame_controller) return ORACLES_INVENTORY_REFUSED_MINIGAME;
    if (s->in_boxing_match) return ORACLES_INVENTORY_REFUSED_BOXING;
    if (s->slots[ORACLES_INVENTORY_SLOT_B] == s->item_biggoron_sword || s->slots[ORACLES_INVENTORY_SLOT_A] == s->item_biggoron_sword) return ORACLES_INVENTORY_REFUSED_BIGGORON;
    if (!oracles_guest_inventory_consistent(s->slots)) return ORACLES_INVENTORY_REFUSED_INCONSISTENT;
    return ORACLES_INVENTORY_APPLIED;
}

/* The conditions, in their order: the lasting ones refuse, the passing ones postpone. */
OraclesGuestInventoryVerdict oracles_guest_inventory_check(const OraclesGuestInventoryState *s, const OraclesGuestInventoryOp *op)
{
    if (!s || !op || (!op->swap && op->variant == ORACLES_VARIANT_NONE)) return ORACLES_INVENTORY_NOTHING;
    const OraclesGuestInventoryVerdict refusal = oracles_guest_inventory_refusal(s);
    if (refusal != ORACLES_INVENTORY_APPLIED) return refusal;
    int changes = 0;
    if (op->swap) {
        /* Two different slots of the eighteen, one of them a button: what @equipItem and the menu's three-step A/B exchange produce. */
        if (op->slot_a >= ORACLES_INVENTORY_SLOTS || op->slot_b >= ORACLES_INVENTORY_SLOTS || op->slot_a == op->slot_b
            || (op->slot_a > ORACLES_INVENTORY_SLOT_A && op->slot_b > ORACLES_INVENTORY_SLOT_A)) return ORACLES_INVENTORY_REFUSED_OPERATION;
        const uint8_t a = s->slots[op->slot_a], b = s->slots[op->slot_b];
        if (a >= s->num_inventory_items || b >= s->num_inventory_items) return ORACLES_INVENTORY_REFUSED_OPERATION;
        if (a == s->item_biggoron_sword || b == s->item_biggoron_sword) return ORACLES_INVENTORY_REFUSED_BIGGORON;
        if (op->expect && (a != op->expect_a || b != op->expect_b)) return ORACLES_INVENTORY_REFUSED_EXPECTATION;
        changes |= a != b;
    }
    switch (op->variant) {
    case ORACLES_VARIANT_NONE: break;
    case ORACLES_VARIANT_SATCHEL: case ORACLES_VARIANT_SHOOTER:
        if (op->variant_value > 4u || !(s->obtained_seeds & (1u << op->variant_value))) return ORACLES_INVENTORY_REFUSED_VARIANT;
        changes |= op->variant_value != (op->variant == ORACLES_VARIANT_SATCHEL ? s->satchel_seeds : s->shooter_seeds);
        break;
    case ORACLES_VARIANT_HARP:
        if (s->harp_song == 0xffu || op->variant_value < 1u || op->variant_value > 3u || !(s->obtained_songs & (1u << (op->variant_value - 1u)))) return ORACLES_INVENTORY_REFUSED_VARIANT;
        changes |= op->variant_value != s->harp_song;
        break;
    default: return ORACLES_INVENTORY_REFUSED_OPERATION;
    }
    if (s->scroll_mode & 0x0eu) return ORACLES_INVENTORY_POSTPONED_SCROLL;
    if (s->menu_disabled) return ORACLES_INVENTORY_POSTPONED_MENU_DISABLED;
    if (s->disable_link_collisions_and_menu) return ORACLES_INVENTORY_POSTPONED_COLLISIONS_DISABLED;
    if (s->link_playing_instrument) return ORACLES_INVENTORY_POSTPONED_INSTRUMENT;
    if (item_in_use(s, op)) return ORACLES_INVENTORY_POSTPONED_ITEM_IN_USE;
    if (s->dont_update_status_bar) return ORACLES_INVENTORY_POSTPONED_STATUS_BAR_HIDDEN;
    return changes ? ORACLES_INVENTORY_APPLIED : ORACLES_INVENTORY_NOTHING;
}

/* ---- the live instance -------------------------------------------------------------------- */

static uint8_t *bank0_byte(OraclesGuest *guest, OraclesGuestSym sym, size_t size)
{
    if (sym.bank != 0 || sym.addr < 0xc000u || (size_t)(sym.addr - 0xc000u) + size > 0x1000u) return NULL;
    uint8_t *wram = oracles_guest_wram_writable(guest, 0);
    return wram ? wram + (sym.addr - 0xc000u) : NULL;
}

void oracles_guest_inventory_state(OraclesGuest *guest, OraclesGuestInventoryState *s)
{
    const OraclesGuestTables *t = guest->tables;
    memset(s, 0, sizeof *s);
    s->game = oracles_compat_family(guest->profile);   /* the profile's family, not the identity of its table */
    s->frame = guest->frame;
    /* wInventoryB, wInventoryA and the sixteen bytes of wInventoryStorage are contiguous (wram.s). */
    const uint8_t *slots = oracles_guest_ptr(guest, t->inventory_b, ORACLES_INVENTORY_SLOTS);
    if (slots && t->inventory_a.addr == t->inventory_b.addr + 1u && t->inventory_storage.addr == t->inventory_b.addr + 2u) memcpy(s->slots, slots, sizeof s->slots);
    else memset(s->slots, 0xff, sizeof s->slots);   /* never consistent: every operation is refused */
    s->satchel_seeds = oracles_guest_read8(guest, t->satchel_selected_seeds);
    s->shooter_seeds = oracles_guest_read8(guest, t->shooter_selected_seeds);
    s->harp_song = t->selected_harp_song.bank == ORACLES_GUEST_ABSENT ? 0xffu : oracles_guest_read8(guest, t->selected_harp_song);
    /* wObtainedTreasureFlags is a bit array by treasure id: the five seeds from TREASURE_EMBER_SEEDS, then the three songs. */
    const unsigned first = t->treasure_ember_seeds;
    for (unsigned i = 0; i < 8u; i++) {
        const unsigned id = first + i;
        const OraclesGuestSym at = { t->obtained_treasure_flags.bank, (uint16_t)(t->obtained_treasure_flags.addr + id / 8u) };
        const int obtained = (oracles_guest_read8(guest, at) >> (id % 8u)) & 1u;
        if (i < 5u) s->obtained_seeds |= (uint8_t)(obtained << i);
        else if (t->treasure_tune_of_echoes == first + 5u) s->obtained_songs |= (uint8_t)(obtained << (i - 5u));
    }
    s->game_state = oracles_guest_read8(guest, t->game_state);
    s->cutscene_index = oracles_guest_read8(guest, t->cutscene_index);
    s->opened_menu_type = oracles_guest_read8(guest, t->opened_menu_type);
    s->text_is_active = oracles_guest_read8(guest, t->text_is_active);
    s->link_death_trigger = oracles_guest_read8(guest, t->link_death_trigger);
    {
        const OraclesGuestSym at = { t->global_flags.bank, (uint16_t)(t->global_flags.addr + t->globalflag_intro_done / 8u) };
        s->intro_done = (oracles_guest_read8(guest, at) >> (t->globalflag_intro_done % 8u)) & 1u;
    }
    s->use_simulated_input = oracles_guest_read8(guest, t->use_simulated_input);
    s->in_boxing_match = t->in_boxing_match.bank == ORACLES_GUEST_ABSENT ? 0u : oracles_guest_read8(guest, t->in_boxing_match);
    /* The minigames that rewrite the buttons without disabling the menu: their controller is an interaction of the room. */
    const uint8_t controller = s->game == ORACLES_GAME_AGES ? t->interac_wild_tokay_controller : t->interac_golden_cave_subrosian;
    for (unsigned i = 0; controller != 0xffu && i < ORACLES_OBJECT_SLOTS; i++) {
        const uint8_t *interaction = oracles_guest_object(guest, i, 1);
        if (interaction && interaction[ORACLES_OBJ_ENABLED] && interaction[ORACLES_OBJ_ID] == controller) s->minigame_controller = 1;
    }
    s->scroll_mode = oracles_guest_read8(guest, t->scroll_mode);
    s->menu_disabled = oracles_guest_read8(guest, t->menu_disabled);
    s->disable_link_collisions_and_menu = oracles_guest_read8(guest, t->disable_link_collisions_and_menu);
    s->link_playing_instrument = oracles_guest_read8(guest, t->link_playing_instrument);
    s->dont_update_status_bar = oracles_guest_read8(guest, t->dont_update_status_bar);
    s->status_bar_needs_refresh = oracles_guest_read8(guest, t->status_bar_needs_refresh);
    const OraclesGuestSym parents[5] = { t->parent_item2_enabled, t->parent_item3_enabled, t->parent_item4_enabled, t->parent_item5_enabled, t->weapon_item_enabled };
    for (unsigned i = 0; i < 5u; i++) {
        const uint8_t *item = oracles_guest_ptr(guest, parents[i], ORACLES_OBJ_VAR03 + 1u);
        if (!item) continue;
        s->parents[i].enabled = item[ORACLES_OBJ_ENABLED]; s->parents[i].id = item[ORACLES_OBJ_ID]; s->parents[i].button = item[ORACLES_OBJ_VAR03];
    }
    s->num_inventory_items = t->num_inventory_items; s->item_biggoron_sword = t->item_biggoron_sword;
    s->item_seed_satchel = t->item_seed_satchel; s->item_harp = t->item_harp;
    s->item_seed_shooter = s->game == ORACLES_GAME_AGES ? t->item_shooter : t->item_slingshot;
    s->cutscene_ingame = t->cutscene_ingame; s->cutscene_onox_final_form = t->cutscene_onox_final_form;
    s->cutscene_loading_room = t->cutscene_loading_room;
    oracles_guest_item_buttons_state(guest, s);
}

static void write_operation(OraclesGuest *guest, const OraclesGuestInventoryOp *op)
{
    const OraclesGuestTables *t = guest->tables;
    uint8_t *slots = bank0_byte(guest, t->inventory_b, ORACLES_INVENTORY_SLOTS);
    uint8_t *refresh = bank0_byte(guest, t->status_bar_needs_refresh, 1);
    if (!slots || !refresh) return;
    if (op->swap) { const uint8_t held = slots[op->slot_a]; slots[op->slot_a] = slots[op->slot_b]; slots[op->slot_b] = held; }
    const OraclesGuestSym variant = op->variant == ORACLES_VARIANT_SATCHEL ? t->satchel_selected_seeds
                                  : op->variant == ORACLES_VARIANT_SHOOTER ? t->shooter_selected_seeds : t->selected_harp_song;
    uint8_t *value = op->variant != ORACLES_VARIANT_NONE ? bank0_byte(guest, variant, 1) : NULL;
    if (value) *value = op->variant_value;
    *refresh |= 0x01u;   /* as loseTreasure_helper does: updateStatusBar reloads the two buttons' icons at the next turn */
    guest->inventory_applied++;
    if (!oracles_guest_inventory_consistent(slots)) guest->inventory_violations++;
}

void oracles_guest_apply_inventory_policy(OraclesGuest *guest)
{
    if (!guest->inventory_policy) return;
    OraclesGuestInventoryState state;
    OraclesGuestInventoryOp op;
    oracles_guest_inventory_state(guest, &state);
    memset(&op, 0, sizeof op);
    guest->inventory_policy(guest->inventory_opaque, &state, &op);
    if (!op.swap && op.variant == ORACLES_VARIANT_NONE) return;
    const OraclesGuestInventoryVerdict verdict = oracles_guest_inventory_check(&state, &op);
    if (verdict == ORACLES_INVENTORY_APPLIED) write_operation(guest, &op);
    if (guest->inventory_result) guest->inventory_result(guest->inventory_opaque, &state, &op, verdict);
}

/* The return address of the one `call checkReloadStatusBarGraphics` of mainThreadStart, read in the user's ROM. */
static uint16_t find_write_point(OraclesGuest *guest)
{
    const OraclesGuestTables *t = guest->tables;
    size_t size = 0;
    uint16_t bank = 0;
    const uint8_t *rom = GB_get_direct_access(guest->gb, GB_DIRECT_ACCESS_ROM, &size, &bank);
    const uint16_t start = t->main_thread_start.addr, target = t->check_reload_status_bar_graphics.addr;
    if (!rom || t->main_thread_start.bank != 0 || t->check_reload_status_bar_graphics.bank != 0 || (size_t)start + 0x80u > size || start + 0x80u > 0x4000u) return 0;
    uint16_t found = 0;
    unsigned count = 0;
    for (uint16_t at = start; at < start + 0x80u - 2u; at++)
        if (rom[at] == 0xcdu && rom[at + 1u] == (uint8_t)target && rom[at + 2u] == (uint8_t)(target >> 8)) { found = (uint16_t)(at + 3u); count++; }
    return count == 1u ? found : 0;
}

int oracles_guest_set_inventory_policy(OraclesGuest *guest, OraclesGuestInventoryFn policy, OraclesGuestInventoryResultFn result,
                                       OraclesGuestInventoryResetFn reset, void *opaque)
{
    if (!guest) return -1;
    if (policy) {
        /* Only where the inventory's addresses are verified (the profile says so). */
        if (!oracles_compat_item_hotkeys(guest->profile)) return -1;
        guest->inventory_write_pc = find_write_point(guest);
        if (!guest->inventory_write_pc) return -1;
        if (oracles_guest_add_hook(guest, guest->tables->check_reload_status_bar_graphics, 0, ORACLES_EVENT_STATUS_BAR_CHECKED) != 0
            || oracles_guest_add_hook(guest, guest->tables->load_equipped_item_gfx, ORACLES_EVENT_EQUIPPED_GFX_LOADED, 0) != 0
            || oracles_guest_add_hook(guest, guest->tables->check_use_items, ORACLES_EVENT_USE_ITEMS, 0) != 0
            || oracles_guest_add_hook(guest, guest->tables->initialize_parent_item, ORACLES_EVENT_ITEM_STARTED, 0) != 0) return -1;
    }
    guest->inventory_policy = policy;
    guest->inventory_result = policy ? result : NULL;
    guest->inventory_reset = policy ? reset : NULL;
    guest->inventory_opaque = opaque;
    return 0;
}

void oracles_guest_inventory_counts(const OraclesGuest *guest, unsigned *applied, unsigned *violations)
{
    if (applied) *applied = guest->inventory_applied;
    if (violations) *violations = guest->inventory_violations;
}
