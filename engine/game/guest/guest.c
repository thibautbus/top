/* The guest bus, the hooks and the events of the game running in the core.
 * The transition transaction of the continuous transitions is in guest_transaction.c. */
#include "guest_internal.h"

/* ---- bus ----------------------------------------------------------------------- */

const uint8_t *oracles_guest_wram(OraclesGuest *guest, unsigned bank)
{
    size_t size = 0;
    uint16_t current = 0;
    const uint8_t *ram = oracles_core_memory(guest->core, ORACLES_CORE_WRAM, &size, &current);
    if (!ram || bank > 7 || size < 0x8000u) return NULL;
    return ram + bank * 0x1000u;
}

const uint8_t *oracles_guest_vram(OraclesGuest *guest, unsigned bank)
{
    size_t size = 0;
    uint16_t current = 0;
    const uint8_t *vram = oracles_core_memory(guest->core, ORACLES_CORE_VRAM, &size, &current);
    if (!vram || bank > 1 || size < 0x4000u) return NULL;
    return vram + bank * 0x2000u;
}

const uint8_t *oracles_guest_oam(OraclesGuest *guest)
{
    if (guest->oam_scanned_valid) return guest->oam_scanned;
    size_t size = 0;
    uint16_t bank = 0;
    return oracles_core_memory(guest->core, ORACLES_CORE_OAM, &size, &bank);
}

const uint8_t *oracles_guest_hram(OraclesGuest *guest)
{
    size_t size = 0;
    uint16_t bank = 0;
    return oracles_core_memory(guest->core, ORACLES_CORE_HRAM, &size, &bank);
}

const uint8_t *oracles_guest_io(OraclesGuest *guest)
{
    size_t size = 0;
    uint16_t bank = 0;
    return oracles_core_memory(guest->core, ORACLES_CORE_IO, &size, &bank);
}

const uint8_t *oracles_guest_ptr(OraclesGuest *guest, OraclesGuestSym sym, size_t length)
{
    if (sym.bank == ORACLES_GUEST_ABSENT || length == 0) return NULL;
    const unsigned addr = sym.addr;
    if (addr >= 0xc000u && addr <= 0xcfffu) {
        if (addr + length > 0xd000u) return NULL;
        const uint8_t *wram = oracles_guest_wram(guest, 0);
        return wram ? wram + (addr - 0xc000u) : NULL;
    }
    if (addr >= 0xd000u && addr <= 0xdfffu) {
        if (addr + length > 0xe000u || sym.bank > 7) return NULL;
        const uint8_t *wram = oracles_guest_wram(guest, sym.bank ? sym.bank : 1);
        return wram ? wram + (addr - 0xd000u) : NULL;
    }
    if (addr >= 0xff80u && addr <= 0xfffeu) {
        if (addr + length > 0xffffu) return NULL;
        const uint8_t *hram = oracles_guest_hram(guest);
        return hram ? hram + (addr - 0xff80u) : NULL;
    }
    if (addr >= 0xff00u && addr <= 0xff7fu) {
        if (addr + length > 0xff80u) return NULL;
        const uint8_t *io = oracles_guest_io(guest);
        return io ? io + (addr - 0xff00u) : NULL;
    }
    return NULL;
}

uint8_t oracles_guest_read8(OraclesGuest *guest, OraclesGuestSym sym)
{
    const uint8_t *p = oracles_guest_ptr(guest, sym, 1);
    return p ? *p : 0;
}

uint16_t oracles_guest_read16(OraclesGuest *guest, OraclesGuestSym sym)
{
    const uint8_t *p = oracles_guest_ptr(guest, sym, 2);
    return p ? (uint16_t)(p[0] | (p[1] << 8)) : 0;
}

const uint8_t *oracles_guest_object(OraclesGuest *guest, unsigned index, unsigned kind)
{
    if (index >= ORACLES_OBJECT_SLOTS || kind >= ORACLES_OBJECTS_PER_SLOT) return NULL;
    const uint8_t *wram = oracles_guest_wram(guest, ORACLES_OBJECTS_BANK);
    return wram ? wram + (ORACLES_OBJECTS_BASE - 0xd000u) + index * 0x100u + kind * ORACLES_OBJECT_SIZE : NULL;
}

uint16_t oracles_guest_sp(OraclesGuest *guest) { return oracles_core_registers(guest->core).sp; }
uint16_t oracles_guest_pc(OraclesGuest *guest) { return oracles_core_registers(guest->core).pc; }

uint16_t oracles_guest_thread_sp(OraclesGuest *guest, unsigned thread)
{
    if (thread >= THREADS) return 0;
    OraclesGuestSym sym = guest->tables->thread_state_buffer;
    sym.addr = (uint16_t)(sym.addr + thread * THREAD_STATE_SIZE + THREAD_STATE_SP_OFFSET);
    return oracles_guest_read16(guest, sym);
}

/* ---- events -------------------------------------------------------------------- */

/* The game leaves the guest's half: nothing of the guest acts on the other half's game, whose memory its tables do not
 * describe.  The returns awaited and the calls queued belong to the thread swapGame restarts. */
static void leave_half(OraclesGuest *guest)
{
    guest->half_returns_dropped += guest->pending_count;
    oracles_guest_clear_hooks(guest);
    guest->call_count = 0;
    guest->call_watch_sp = 0;
    guest->half_left = guest->frame + 1u;
}

/* At swapGame's entry: the switch to another game's half, or the request of the game already running, which swapGame
 * answers by restarting its thread alone.  A ghost's guest (oracles_guest_watch_half) only notes it. */
static void half_switch(OraclesGuest *guest)
{
    const OraclesCompatProfile *p = guest->profile;
    const unsigned asked = oracles_guest_read8(guest, oracles_compat_swap_game_request(p)), running = oracles_guest_read8(guest, oracles_compat_current_game(p));
    if (running == oracles_compat_half_game(p) && (uint8_t)(asked - 2u) == running) return;
    if (guest->half_watched) guest->half_crossed = 1;
    else leave_half(guest);
}

uint32_t oracles_guest_half_left(const OraclesGuest *guest) { return guest ? guest->half_left : 0; }
int oracles_guest_switching_half(const OraclesGuest *guest) { return guest ? guest->half_switching : 0; }

int oracles_guest_check_half(OraclesGuest *guest)
{
    if (!guest || !oracles_compat_half_switch(guest->profile)) return 0;
    const OraclesCompatProfile *p = guest->profile;
    const unsigned half = oracles_compat_half_game(p), asked = oracles_guest_read8(guest, oracles_compat_swap_game_request(p));
    /* swapGame writes the byte that names the game last: a state saved at its frame still names this half, and has
     * passed the hook, its request for the other game standing until it ends. */
    if (!guest->half_left && (oracles_guest_read8(guest, oracles_compat_current_game(p)) != half || (asked && (uint8_t)(asked - 2u) != half))) leave_half(guest);
    return guest->half_left != 0;
}

int oracles_guest_half_passage(OraclesGuest *guest)
{
    if (!guest || !oracles_compat_half_switch(guest->profile)) return 0;
    return guest->half_left || guest->half_settling || oracles_guest_read8(guest, oracles_compat_swap_game_request(guest->profile)) != 0;
}

/* The game's thread restarted in the half it went to: its screen still the one it left, until the LCD goes off for the
 * first room (_initializeGame's) or, at the latest, the game plays. */
#define HALF_SETTLE_FRAMES 60u

static void half_settle(OraclesGuest *guest)
{
    const uint8_t *io = oracles_guest_io(guest);
    if (oracles_guest_read8(guest, oracles_compat_swap_game_request(guest->profile))) return;
    if ((io && !(io[0x40] & 0x80u)) || oracles_guest_read8(guest, guest->tables->game_state) != 0u || ++guest->half_settling > HALF_SETTLE_FRAMES) guest->half_settling = 0;
}

int oracles_guest_watch_half(OraclesGuest *guest)
{
    if (!oracles_compat_half_switch(guest->profile)) return 0;
    guest->half_watched = 1;
    return oracles_guest_add_hook(guest, oracles_compat_swap_game(guest->profile), ORACLES_EVENT_HALF_SWITCH, 0);
}

int oracles_guest_half_crossed(const OraclesGuest *guest) { return guest ? guest->half_crossed : 0; }

static void emit(OraclesGuest *guest, OraclesGuestEventType type, uint16_t pc, uint16_t address, uint8_t value)
{
    if (type == ORACLES_EVENT_HALF_SWITCH) { half_switch(guest); return; }
    if (type == ORACLES_EVENT_FRAME_DONE) oracles_guest_apply_transition_entry(guest);
    if (type == ORACLES_EVENT_FRAME_DRAWN) {
        if (!guest->transition_at_entry) oracles_guest_apply_transition_policy(guest, 0);
        guest->transition_at_entry = 0;
        guest->drawn_this_frame = 1;
    }
    /* checkReloadStatusBarGraphics is also reached by tail jumps whose return lies elsewhere: only its call from mainThreadStart is the write point. */
    if (type == ORACLES_EVENT_STATUS_BAR_CHECKED) { if (pc != guest->inventory_write_pc) return; oracles_guest_apply_inventory_policy(guest); }
    if (type == ORACLES_EVENT_STATUS_BAR_CHECK) oracles_guest_call_point(guest, oracles_core_registers(guest->core).sp);
    if (type == ORACLES_EVENT_TEXT_ADDRESS) oracles_guest_serve_text(guest);
    if (type == ORACLES_EVENT_MERMAID_STROKE) oracles_guest_mermaid_stroke(guest);
    if (type == ORACLES_EVENT_FLIPPERS_STROKE) oracles_guest_flippers_stroke(guest);
    if (type >= ORACLES_EVENT_EDGE_TEST_UP && type <= ORACLES_EVENT_EDGE_TEST_LEFT) oracles_guest_edge_test(guest, type);
    if (type == ORACLES_EVENT_CHEAT_DAMAGE || type == ORACLES_EVENT_CHEAT_AMMO || type == ORACLES_EVENT_CHEAT_RUPEES) oracles_guest_cheat_point(guest, type);
    if (type >= ORACLES_EVENT_TEXT_LINE && type <= ORACLES_EVENT_TEXT_COLUMN_SHOWN) oracles_guest_compact_text_point(guest, type);
    if (type == ORACLES_EVENT_TEXT_SKIP_TEST || type == ORACLES_EVENT_TEXT_SKIP_LOOP) oracles_guest_text_skip_point(guest, type);
    if (guest->text_box_armed && (type == ORACLES_EVENT_TEXT_LINE || type == ORACLES_EVENT_TEXT_LINE_DRAWN || type == ORACLES_EVENT_TEXT_GFX_DMA
                                  || type == ORACLES_EVENT_VBLANK_DMA))
        oracles_guest_text_box_point(guest, type);
    if (!guest->sink && !guest->listener_count) return;
    const OraclesCoreRegisters r = oracles_core_registers(guest->core);
    OraclesGuestEvent event;
    event.type = type;
    event.frame = guest->frame;
    event.pc = pc;
    event.address = address;
    event.value = value;
    event.a = (uint8_t)(r.af >> 8);
    event.b = (uint8_t)(r.bc >> 8); event.c = (uint8_t)r.bc;
    event.d = (uint8_t)(r.de >> 8); event.e = (uint8_t)r.de;
    event.h = (uint8_t)(r.hl >> 8); event.l = (uint8_t)r.hl;
    event.f = (uint8_t)r.af;
    if (guest->sink) guest->sink(guest->sink_opaque, &event);
    for (unsigned i = 0; i < guest->listener_count; i++) guest->listeners[i](guest->listener_opaque[i], &event);
}

int oracles_guest_add_event_listener(OraclesGuest *guest, OraclesGuestEventFn listener, void *opaque)
{
    if (!guest || !listener) return -1;
    if (guest->listener_count >= sizeof guest->listeners / sizeof guest->listeners[0]) return -1;
    guest->listeners[guest->listener_count] = listener;
    guest->listener_opaque[guest->listener_count] = opaque;
    guest->listener_count++;
    return 0;
}

void oracles_guest_remove_event_listener(OraclesGuest *guest, OraclesGuestEventFn listener, void *opaque)
{
    if (!guest) return;
    for (unsigned i = 0; i < guest->listener_count; i++) {
        if (guest->listeners[i] != listener || guest->listener_opaque[i] != opaque) continue;
        for (unsigned k = i + 1; k < guest->listener_count; k++) {
            guest->listeners[k - 1] = guest->listeners[k];
            guest->listener_opaque[k - 1] = guest->listener_opaque[k];
        }
        guest->listener_count--;
        return;
    }
}

/* The bytes the RAM's copy is compared on: enough to tell the two halves' copies apart at a function's entry. */
#define SRAM_COPY_CHECK 8u

int oracles_guest_sram_code_mapped(OraclesGuest *guest, OraclesGuestSym entry)
{
    size_t ram_size = 0, rom_size = 0;
    uint16_t ram_bank = 0, rom_bank = 0;
    const uint8_t *ram = oracles_core_memory(guest->core, ORACLES_CORE_CART_RAM, &ram_size, &ram_bank);
    if (!ram || ram_bank >= 8u || !(guest->sram_code_banks >> ram_bank & 1u)) return 0;
    const uint8_t *rom = oracles_core_memory(guest->core, ORACLES_CORE_ROM, &rom_size, &rom_bank);
    const size_t within = entry.addr - ORACLES_SRAM_CODE_START;
    const size_t left = ORACLES_SRAM_CODE_END - entry.addr, length = left < SRAM_COPY_CHECK ? left : SRAM_COPY_CHECK;
    const size_t at_rom = ((size_t)entry.bank + guest->rom_bank_base) * 0x4000u + within, at_ram = (size_t)ram_bank * 0x2000u + within;
    return rom && at_rom + length <= rom_size && at_ram + length <= ram_size && memcmp(rom + at_rom, ram + at_ram, length) == 0;
}

/* The word at the stack pointer, read through the bus. */
static int stack_word(OraclesGuest *guest, uint16_t sp, uint16_t *word)
{
    const OraclesGuestSym sym = { 0, sp };
    const uint8_t *p = oracles_guest_ptr(guest, sym, 2);
    if (!p) return 0;
    *word = (uint16_t)(p[0] | (p[1] << 8));
    return 1;
}

/* Whether two stack pointers are on the same stack (the main stack or a
 * thread's, from the generated tables); when neither table names them, yes. */
static int same_stack(const OraclesGuest *guest, uint16_t a, uint16_t b)
{
    const OraclesGuestTables *t = guest->tables;
    const OraclesGuestSym stacks[5][2] = { { t->main_stack, t->main_stack_top }, { t->thread0_stack, t->thread0_stack_top },
                                           { t->thread1_stack, t->thread1_stack_top }, { t->thread2_stack, t->thread2_stack_top }, { t->thread3_stack, t->thread3_stack_top } };
    for (unsigned i = 0; i < 5; i++) {
        if (stacks[i][0].bank == ORACLES_GUEST_ABSENT) continue;
        const int in_a = a >= stacks[i][0].addr && a <= stacks[i][1].addr, in_b = b >= stacks[i][0].addr && b <= stacks[i][1].addr;
        if (in_a != in_b) return 0;
        if (in_a) return 1;
    }
    return 1;
}

/* Out of line: the address spaces' test is reached only for an address a hook enters at, and keeps the registers of the
 * instruction's path to itself. */
__attribute__((noinline)) static int hook_code_mapped(OraclesGuest *guest, OraclesGuestSym entry) { return oracles_guest_code_mapped(guest, entry); }

static void on_execute(void *opaque, uint16_t pc, uint8_t opcode, uint16_t sp)
{
    OraclesGuest *guest = opaque;

    /* Interrupt depth for the read trace: the handlers of the game end with reti (bank0.s). */
    if (pc == 0x40u || pc == 0x48u || pc == 0x50u || pc == 0x58u || pc == 0x60u) guest->interrupt_depth++;
    else if (opcode == 0xd9u && guest->interrupt_depth) guest->interrupt_depth--;

    /* The boot ROM shares the low addresses with the game's functions: no hook
     * until it is unmapped.  It never executes $0100-$01ff, the cartridge's
     * entry, so the first instruction there ends it. */
    if (!guest->boot_finished) {
        if (pc >= 0x100u && pc < 0x200u) guest->boot_finished = 1;
        else return;
    }

    /* The call transaction: the return it turns into a jump to the game's routine (guest_call.c). */
    if (guest->call_watch_sp) oracles_guest_call_step(guest, sp, opcode);

    /* Returns: the captured address is reached with the stack back where the
     * call left it.  The pending frames of every thread share one list: a
     * hooked function that yields (the game waits for a vblank inside it:
     * Seasons' Subrosia loads object graphics from applyAllTileSubstitutions)
     * leaves its frame under the frames another thread pushes meanwhile, so
     * each frame is matched wherever it stands in the list, not only on top. */
    for (unsigned i = guest->pending_count; i-- > 0;) {
        const pending_return p = guest->pending[i];
        const int returned = pc == p.address && sp == p.sp_after;
        /* The frame is gone when the stack has unwound past it on the same
         * stack: returned elsewhere.  On another thread's stack it waits. */
        const int unwound = !returned && sp > p.sp_after && same_stack(guest, sp, p.sp_after);
        if (!returned && !unwound) continue;
        memmove(&guest->pending[i], &guest->pending[i + 1u], (guest->pending_count - i - 1u) * sizeof guest->pending[0]);
        guest->pending_count--;
        if (unwound) { guest->returns_purged++; continue; }
        /* getFreeInteractionSlot, getFreeEnemySlot_uncounted and getFreePartSlot
         * answer Z when a slot was found (bank0.s): the events say "created", so
         * they fire on success only, and a refusal is counted. */
        if (p.type == ORACLES_EVENT_INTERACTION_CREATED || p.type == ORACLES_EVENT_ENEMY_CREATED || p.type == ORACLES_EVENT_PART_CREATED) {
            if (!(oracles_core_registers(guest->core).af & 0x80u)) {
                guest->slot_failures[p.type == ORACLES_EVENT_INTERACTION_CREATED ? 0 : p.type == ORACLES_EVENT_ENEMY_CREATED ? 1 : 2]++;
                continue;
            }
        }
        emit(guest, p.type, pc, 0, 0);
    }

    if (!(guest->hooked_pc[pc >> 3] & (1u << (pc & 7u)))) return;
    for (unsigned i = 0; i < guest->hook_count; i++) {
        const hook *h = &guest->hooks[i];
        if (pc != h->entry.addr) continue;
        if (pc >= 0x4000u && !hook_code_mapped(guest, h->entry)) continue;
        if (h->on_entry) emit(guest, h->on_entry, pc, 0, 0);
        if (h->on_return) {
            if (guest->pending_count >= PENDING_RETURNS) { guest->returns_overflow++; continue; }
            uint16_t address;
            if (stack_word(guest, sp, &address)) {
                pending_return *p = &guest->pending[guest->pending_count++];
                p->address = address;
                p->sp_after = (uint16_t)(sp + 2);
                p->type = h->on_return;
            }
        }
    }
}

static void on_write(void *opaque, uint16_t address, uint8_t value)
{
    OraclesGuest *guest = opaque;
    if (address >= 0xff40u && address <= 0xff6bu) {
        const int journaled = (address <= 0xff4bu) || address == 0xff46u
                           || (address >= 0xff51u && address <= 0xff55u)
                           || address == 0xff69u || address == 0xff6bu || address == 0xff4fu;
        if (journaled) {
            if (guest->journal_count < JOURNAL_CAPACITY) {
                const uint8_t *io = oracles_guest_io(guest);
                OraclesGuestRegWrite *w = &guest->journal[guest->journal_count++];
                w->ly = io ? io[0x44] : 0;
                w->stat_mode = io ? (uint8_t)(io[0x41] & 3u) : 0;
                w->reg = (uint8_t)address;
                w->value = value;
            } else guest->journal_dropped++;
        }
        return;
    }
    if (!guest->boot_finished || guest->half_left) return;
    if (address == guest->keys_pressed) { guest->keys_polls++; return; }
    if (address == guest->transition_direction) {
        /* @startTransition (bank1.s) writes state 3, scroll mode 4, then the direction: the decision. */
        const OraclesGuestSym state = { 0, guest->transition_state };
        const OraclesGuestTables *t = guest->tables;
        if (oracles_guest_read8(guest, state) == 3u && oracles_guest_read8(guest, t->scroll_mode) == 4u)
            emit(guest, ORACLES_EVENT_TRANSITION, oracles_core_registers(guest->core).pc, address, value);
        return;
    }
    if (address >= guest->objects_start && address < guest->objects_end && value == 0
        && (address & 0xffu) == (ORACLES_OBJECTS_PER_SLOT - 2u) * ORACLES_OBJECT_SIZE + ORACLES_OBJ_HEALTH) {
        /* An enemy (the third object of a slot) whose health goes from alive to zero, in the objects' bank. */
        const uint8_t *io = oracles_guest_io(guest);
        const unsigned svbk = io ? (io[0x70] & 7u) : 1u;
        const uint8_t *wram = oracles_guest_wram(guest, ORACLES_OBJECTS_BANK);
        if ((svbk == ORACLES_OBJECTS_BANK || svbk == 0u) && wram && wram[address - 0xd000u] != 0)
            emit(guest, ORACLES_EVENT_ENEMY_KILLED, oracles_core_registers(guest->core).pc, address, value);
        return;
    }
    if (address >= guest->global_flags_start && address < guest->global_flags_end)
        emit(guest, ORACLES_EVENT_FLAG_WRITE, oracles_core_registers(guest->core).pc, address, value);
    else if (address >= guest->room_flags_start && address < guest->room_flags_end)
        emit(guest, ORACLES_EVENT_FLAG_WRITE, oracles_core_registers(guest->core).pc, address, value);
    else if (address == guest->selected_text_option)
        emit(guest, ORACLES_EVENT_TEXT_CHOICE, oracles_core_registers(guest->core).pc, address, value);
}

static void on_read(void *opaque, uint16_t address, uint16_t sp)
{
    OraclesGuest *guest = opaque;
    /* The interrupt vectors ($40-$60) are read only as the first opcode fetch
     * of a handler, which precedes the execution callback that raises the
     * depth; gameplay never reads them as data.  Excluding them removes that
     * one-fetch leak, and the depth excludes the rest of the handler. */
    const int in_vector = address >= 0x40u && address <= 0x60u && (address & 7u) == 0u;
    if (guest->read_enabled && guest->interrupt_depth == 0 && !in_vector && guest->read_fn)
        guest->read_fn(guest->read_opaque, address, sp);
}

static void on_vblank(void *opaque, OraclesVblankType type);

/* The core's hooks: the read hook only while a read trace is set. */
static void set_core_hooks(OraclesGuest *guest)
{
    const OraclesCoreHooks hooks = { on_execute, guest->read_fn ? on_read : NULL, on_write, on_vblank };
    oracles_core_set_hooks(guest->core, &hooks, guest);
}

void oracles_guest_set_read_trace(OraclesGuest *guest, OraclesGuestReadFn fn, void *opaque)
{
    guest->read_fn = fn;
    guest->read_opaque = opaque;
    set_core_hooks(guest);
}

void oracles_guest_enable_read_trace(OraclesGuest *guest, int enabled) { guest->read_enabled = enabled != 0; }

void oracles_guest_reset_execution_state(OraclesGuest *guest)
{
    guest->pending_count = 0;
    guest->interrupt_depth = 0;
    guest->boot_finished = 1;   /* a saved state is always past the boot ROM */
    guest->half_crossed = 0;
    guest->half_state_loaded = 1;
    guest->oam_scanned_valid = 0;
    guest->link_shown_vblanks = 0;
    {   /* where the OAM of the loaded state draws Link, not the execution it replaced: the replay is the same whatever ran before the load */
        const uint8_t *link = oracles_guest_object(guest, 0, 0);
        guest->link_next_x = link ? (uint16_t)(link[ORACLES_OBJ_X] | (link[ORACLES_OBJ_XH] << 8)) : 0;
        guest->link_next_y = link ? (uint16_t)(link[ORACLES_OBJ_Y] | (link[ORACLES_OBJ_YH] << 8)) : 0;
    }
    guest->policy_not_asked = 0;
    guest->transition_at_entry = 0;
    guest->transition_last_state = guest->tables ? oracles_guest_read8(guest, guest->tables->screen_transition_state) : 0u;   /* the state the saved frame's logic left */
    guest->call_count = 0;      /* the calls queued belong to the execution the load replaced */
    guest->call_watch_sp = 0;
    guest->compact_open = 0;    /* a line the load interrupted is given back by oracles_guest_compact_load_state */
    guest->compact_pending = guest->compact_place_next = 0;
    guest->compact_count = 0;
    oracles_guest_text_box_reset(guest);   /* the host's lines belong to the execution the load replaced */
    if (guest->transition_reset) guest->transition_reset(guest->transition_opaque);
    if (guest->inventory_reset) guest->inventory_reset(guest->inventory_opaque);
}

uint32_t oracles_guest_frame(const OraclesGuest *guest) { return guest ? guest->frame : 0; }

int oracles_guest_link_shown(const OraclesGuest *guest, uint16_t *x, uint16_t *y)
{
    if (!guest || guest->link_shown_vblanks < 2u) return 0;
    if (x) *x = guest->link_shown_x;
    if (y) *y = guest->link_shown_y;
    return 1;
}

size_t oracles_guest_journal_dropped(const OraclesGuest *guest) { return guest->journal_dropped; }
uint32_t oracles_guest_keys_polls(const OraclesGuest *guest) { return guest ? guest->keys_polls : 0; }

void oracles_guest_slot_failures(const OraclesGuest *guest, unsigned out[3])
{
    for (unsigned i = 0; i < 3; i++) out[i] = guest->slot_failures[i];
}

void oracles_guest_dropped_returns(const OraclesGuest *guest, unsigned *overflow, unsigned *purged)
{
    if (overflow) *overflow = guest->returns_overflow;
    if (purged) *purged = guest->returns_purged;
}

uint8_t *oracles_guest_wram_writable(OraclesGuest *guest, unsigned bank)
{
    size_t size = 0;
    uint16_t current = 0;
    uint8_t *ram = oracles_core_memory(guest->core, ORACLES_CORE_WRAM, &size, &current);
    if (!ram || bank > 7 || size < 0x8000u) return NULL;
    return ram + bank * 0x1000u;
}

static void on_vblank(void *opaque, OraclesVblankType type)
{
    OraclesGuest *guest = opaque;
    /* The OAM as the scan that just ended read it, kept for the hosts that
     * compose this frame: the transaction below may move Link's entries for
     * the next scan, and the game's vblank handler copies wOam right after. */
    {
        size_t size = 0;
        uint16_t bank = 0;
        const uint8_t *oam = oracles_core_memory(guest->core, ORACLES_CORE_OAM, &size, &bank);
        if (oam && size >= sizeof guest->oam_scanned) { memcpy(guest->oam_scanned, oam, sizeof guest->oam_scanned); guest->oam_scanned_valid = 1; }
    }
    /* The game between two halves: the frame for those who read the screen, nothing of the game. */
    if (guest->half_left || (guest->half_switches && oracles_guest_half_passage(guest))) {
        if (guest->vblank_hook) guest->vblank_hook(guest->vblank_opaque, type);
        return;
    }
    /* Link where the frame just scanned drew him, and where the next will: the game's vblank handler copies wOam
     * (vblankInterrupt, hOamFunc) only when the main loop waits for it, wVBlankChecker $ff; a frame whose logic ends
     * past the vblank leaves the OAM as it was, but for the transaction's moves at the vblank. */
    const uint8_t *link = oracles_guest_object(guest, 0, 0);
    const int copies = oracles_guest_read8(guest, guest->tables->vblank_checker) == 0xffu;
    guest->link_shown_x = guest->link_next_x; guest->link_shown_y = guest->link_next_y;
    if (guest->link_shown_vblanks < 2u) guest->link_shown_vblanks++;
    if (guest->vblank_hook) guest->vblank_hook(guest->vblank_opaque, type);
    /* The compact text's reveal in the VRAM, after the hosts read the frame just scanned and before the game's vblank
     * handler copies the map the text thread wrote: the next frame shows both. */
    if (guest->text_box_armed) oracles_guest_text_box_vblank(guest);   /* the host's lines as the frame just scanned showed them */
    if (guest->compact_armed) oracles_guest_compact_vblank(guest);
    /* A frame the game's logic did not finish (a room load spans a few): the
     * transaction runs at the vblank instead, so that Link walks through the
     * load as well; it writes only while the transition loads the room
     * (states 3 and 4), whose code does not touch the fields it writes, and
     * counts its writes by state for the harness (docs/GAME_HOOKS.md, section 6). */
    if (guest->transition_policy && !guest->drawn_this_frame) oracles_guest_apply_transition_policy(guest, 1);
    guest->drawn_this_frame = 0;
    const uint16_t after_x = link ? (uint16_t)(link[ORACLES_OBJ_X] | (link[ORACLES_OBJ_XH] << 8)) : 0;
    const uint16_t after_y = link ? (uint16_t)(link[ORACLES_OBJ_Y] | (link[ORACLES_OBJ_YH] << 8)) : 0;
    /* Without a copy, the transaction moves the OAM to where it moves him, and says so. */
    if (copies) { guest->link_next_x = after_x; guest->link_next_y = after_y; }
    emit(guest, ORACLES_EVENT_VBLANK, 0, 0, (uint8_t)type);
}

void oracles_guest_set_vblank_hook(OraclesGuest *guest, OraclesGuestVblankFn hook, void *opaque)
{
    guest->vblank_hook = hook;
    guest->vblank_opaque = opaque;
}

const uint8_t *oracles_guest_bg_palettes(OraclesGuest *guest)
{
    size_t size = 0;
    uint16_t bank = 0;
    return oracles_core_memory(guest->core, ORACLES_CORE_BG_PALETTES, &size, &bank);
}

const uint8_t *oracles_guest_obj_palettes(OraclesGuest *guest)
{
    size_t size = 0;
    uint16_t bank = 0;
    return oracles_core_memory(guest->core, ORACLES_CORE_OBJ_PALETTES, &size, &bank);
}

/* ---- attach --------------------------------------------------------------------- */

unsigned oracles_guest_hook_count(const OraclesGuest *guest) { return guest ? guest->hook_count : 0; }
unsigned oracles_guest_hooks_refused(const OraclesGuest *guest) { return guest ? guest->hooks_refused : 0; }

int oracles_guest_add_hook(OraclesGuest *guest, OraclesGuestSym entry, OraclesGuestEventType on_entry, OraclesGuestEventType on_return)
{
    if (entry.bank == ORACLES_GUEST_ABSENT) return 0; /* the game has no such function: nothing to hook */
    if (guest->half_left) return -1;                  /* the game is no longer the guest's (oracles_guest_half_left) */
    /* Two consumers may ask for the same hook: it is armed once, its events reach them both. */
    for (unsigned i = 0; i < guest->hook_count; i++)
        if (guest->hooks[i].entry.bank == entry.bank && guest->hooks[i].entry.addr == entry.addr
            && guest->hooks[i].on_entry == on_entry && guest->hooks[i].on_return == on_return) return 0;
    if (guest->hook_count >= sizeof guest->hooks / sizeof guest->hooks[0]) { guest->hooks_refused++; return -1; }
    hook *h = &guest->hooks[guest->hook_count++];
    h->entry = entry;
    h->on_entry = on_entry;
    h->on_return = on_return;
    guest->hooked_pc[entry.addr >> 3] |= (uint8_t)(1u << (entry.addr & 7u));
    return 0;
}

void oracles_guest_clear_hooks(OraclesGuest *guest)
{
    guest->hook_count = 0;
    guest->pending_count = 0;
    memset(guest->hooked_pc, 0, sizeof guest->hooked_pc);
}

/* What a guest takes of its profile: the tables and the address spaces. */
static void bind_profile(OraclesGuest *guest, const OraclesCompatProfile *profile)
{
    guest->profile = profile;
    guest->tables = oracles_compat_guest_tables(profile);
    guest->rom_bank_base = (uint16_t)oracles_compat_rom_bank_base(profile);
    guest->sram_code_banks = (uint8_t)oracles_compat_sram_code_banks(profile);
}

/* The hooks of a guest's every attachment, and the addresses it watches.  A hook refused for want of room in the table
 * would be missing in silence: -1, and the caller says so. */
static int arm_base(OraclesGuest *guest)
{
    const OraclesCompatProfile *profile = guest->profile;
    const OraclesGuestTables *t = guest->tables;
    oracles_guest_add_hook(guest, t->draw_all_sprites, ORACLES_EVENT_FRAME_DONE, ORACLES_EVENT_FRAME_DRAWN);
    oracles_guest_add_hook(guest, t->initialize_room, ORACLES_EVENT_ROOM_ENTER, ORACLES_EVENT_ROOM_INITIALIZED);
    /* objectCreateInteraction calls getFreeInteractionSlot: one hook covers every creation. */
    oracles_guest_add_hook(guest, t->get_free_interaction_slot, 0, ORACLES_EVENT_INTERACTION_CREATED);
    /* getFreeEnemySlot calls getFreeEnemySlot_uncounted, which parseObjectData
     * also calls by itself: the hook goes on the variant, and sees both. */
    oracles_guest_add_hook(guest, t->get_free_enemy_slot_uncounted, 0, ORACLES_EVENT_ENEMY_CREATED);
    oracles_guest_add_hook(guest, t->get_free_part_slot, 0, ORACLES_EVENT_PART_CREATED);
    oracles_guest_add_hook(guest, t->apply_warp_dest, 0, ORACLES_EVENT_WARP);
    oracles_guest_add_hook(guest, t->show_text, ORACLES_EVENT_TEXT, 0);
    oracles_guest_add_hook(guest, t->give_treasure, ORACLES_EVENT_TREASURE, 0);
    oracles_guest_add_hook(guest, t->open_menu, ORACLES_EVENT_MENU, 0);
    oracles_guest_add_hook(guest, t->save_file, ORACLES_EVENT_SAVE, 0);
    oracles_guest_add_hook(guest, t->poll_input, 0, ORACLES_EVENT_INPUT);
    if (guest->hook_count != ORACLES_GUEST_ATTACHED_HOOKS) return -1;
    if (oracles_compat_half_switch(profile)
        && (oracles_compat_swap_game(profile).addr >= 0x4000u || oracles_guest_add_hook(guest, oracles_compat_swap_game(profile), ORACLES_EVENT_HALF_SWITCH, 0) != 0)) return -1;
    guest->global_flags_start = t->global_flags.addr;
    guest->global_flags_end = (uint16_t)(t->global_flags.addr + t->global_flags_size);
    guest->room_flags_start = t->group0_room_flags.addr;
    guest->room_flags_end = (uint16_t)(t->group0_room_flags.addr + t->room_flags_size);
    guest->selected_text_option = t->selected_text_option.addr;
    guest->transition_state = t->screen_transition_state.addr;
    guest->transition_direction = t->screen_transition_direction.addr;
    guest->keys_pressed = t->keys_pressed.addr;
    guest->objects_start = ORACLES_OBJECTS_BASE;
    guest->objects_end = (uint16_t)(ORACLES_OBJECTS_BASE + ORACLES_OBJECT_SLOTS * 0x100u);
    return 0;
}

OraclesGuest *oracles_guest_attach(OraclesCore *core, const OraclesCompatProfile *profile)
{
    if (!oracles_compat_guest_tables(profile)) return NULL;
    OraclesGuest *guest = calloc(1, sizeof *guest);
    if (!guest) return NULL;
    guest->core = core;
    guest->half_state_loaded = 1;   /* attached late, to a game in another half: as after a load */
    bind_profile(guest, profile);
    if (arm_base(guest) != 0) { free(guest); return NULL; }
    /* Attached after the boot ROM has run (tests, late attaches): the low
     * addresses then read as the cartridge's own bytes. */
    {
        size_t rom_size = 0;
        uint16_t bank = 0;
        const uint8_t *rom = oracles_core_memory(core, ORACLES_CORE_ROM, &rom_size, &bank);
        int same = rom && rom_size >= 16u;
        for (unsigned i = 0; same && i < 16u; i++) if (oracles_core_peek(core, (uint16_t)i) != rom[i]) same = 0;
        guest->boot_finished = same;
    }
    set_core_hooks(guest);
    return guest;
}

OraclesGuest *oracles_guest_reader(OraclesCore *core, const OraclesCompatProfile *profile)
{
    if (!oracles_compat_guest_tables(profile)) return NULL;
    OraclesGuest *guest = calloc(1, sizeof *guest);
    if (!guest) return NULL;
    guest->core = core;
    guest->half_state_loaded = 1;
    bind_profile(guest, profile);
    guest->reader = 1;
    return guest;
}

/* The game runs the half `half` now: the guest as attached to it, on the same address for all who hold it.  Nothing of
 * the half left stays armed: a feature's points are armed again by whom asked for them (oracles_guest_add_half_listener). */
static void switch_half(OraclesGuest *guest, const OraclesCompatProfile *half, int door)
{
    bind_profile(guest, half);
    guest->half_left = 0;
    if (guest->reader) return;
    oracles_guest_clear_hooks(guest);
    guest->calls_armed = 0;
    guest->call_count = 0;
    guest->call_watch_sp = 0;
    guest->inventory_write_pc = 0;
    guest->line_armed = 0;
    guest->swim_hold_armed = 0;
    guest->cheats_armed = 0;
    guest->language_index = NULL;
    guest->compact_armed = 0;
    guest->text_box_armed = 0;
    guest->text_instant_armed = 0;
    if (arm_base(guest) != 0) { leave_half(guest); return; }   /* the table holds them at every attachment: never */
    guest->half_switching = door ? 2 : 1;
    oracles_guest_reset_execution_state(guest);
    guest->half_switching = 0;
    guest->half_switches++;
    for (unsigned i = 0; i < guest->half_listener_count; i++) guest->half_listeners[i].fn(guest->half_listeners[i].opaque);
    guest->half_hooks_armed += guest->hook_count;
}

int oracles_guest_switch_to(OraclesGuest *guest, const OraclesCompatProfile *half)
{
    if (!guest || !half || !oracles_compat_guest_tables(half)) return -1;
    switch_half(guest, half, 0);
    return guest->half_left ? -1 : 0;
}

/* The longest the reset's wait for the game's memory to be cleared lasts: init clears it within a few frames. */
#define HALF_RESET_FRAMES 120u

int oracles_guest_follow_half(OraclesGuest *guest)
{
    if (!guest || !oracles_compat_half_switch(guest->profile)) return 0;
    oracles_guest_check_half(guest);
    const int loaded = guest->half_state_loaded;
    guest->half_state_loaded = 0;
    if (guest->half_settling) { if (loaded) guest->half_settling = 0; else half_settle(guest); }
    const OraclesCompatProfile *p = guest->profile;
    const unsigned running = oracles_guest_read8(guest, oracles_compat_current_game(p)), asked = oracles_guest_read8(guest, oracles_compat_swap_game_request(p));
    if (!guest->half_left) {
        /* A state taken in swapGame's copy, its game already this half's: the guest as attached to it again, the screen
         * held until the first room's blank, as for the switch it stands in. */
        if (!loaded || !asked || (uint8_t)(asked - 2u) != running) return 0;
        guest->half_settling = 1;
        guest->half_reset_wait = 0;
        switch_half(guest, p, 0);
        guest->half_state_loaded = 0;   /* the switch's own reset is no load */
        return !guest->half_left;
    }
    /* swapGame has not named the game it switches to yet. */
    if (asked && (uint8_t)(asked - 2u) != running) return 0;
    /* The reset out of the second half (sramBootstrap, no swapGame) names the first and copies its code, the game's memory
     * still the half's it left, until init clears it; not longer than HALF_RESET_FRAMES. */
    if (!asked && !loaded && oracles_guest_read8(guest, guest->tables->game_state) != 0u && ++guest->half_reset_wait <= HALF_RESET_FRAMES) return 0;
    const OraclesCompatProfile *half = oracles_compat_half(p, running);
    if (!half) return 0;
    guest->half_settling = !loaded || asked ? 1u : 0u;
    guest->half_reset_wait = 0;
    switch_half(guest, half, asked && !loaded);   /* swapGame's switch, the file carried across; a reset's or a state's not */
    guest->half_state_loaded = 0;
    return !guest->half_left;
}

int oracles_guest_add_half_listener(OraclesGuest *guest, OraclesGuestHalfFn listener, void *opaque)
{
    if (!guest || !listener || !oracles_compat_half_switch(guest->profile)) return 0;   /* an image of one game has none to tell */
    if (guest->half_listener_count >= sizeof guest->half_listeners / sizeof guest->half_listeners[0]) return -1;
    guest->half_listeners[guest->half_listener_count].fn = listener;
    guest->half_listeners[guest->half_listener_count].opaque = opaque;
    guest->half_listener_count++;
    return 0;
}

int oracles_guest_listen_half(OraclesGuest *guest, OraclesGuestHalfFn listener)
{
    for (unsigned i = 0; i < guest->half_listener_count; i++)
        if (guest->half_listeners[i].fn == listener && guest->half_listeners[i].opaque == guest) return 0;
    return oracles_guest_add_half_listener(guest, listener, guest);
}

void oracles_guest_remove_half_listener(OraclesGuest *guest, OraclesGuestHalfFn listener, void *opaque)
{
    if (!guest) return;
    for (unsigned i = 0; i < guest->half_listener_count; i++) {
        if (guest->half_listeners[i].fn != listener || guest->half_listeners[i].opaque != opaque) continue;
        for (unsigned k = i + 1; k < guest->half_listener_count; k++) guest->half_listeners[k - 1] = guest->half_listeners[k];
        guest->half_listener_count--;
        return;
    }
}

void oracles_guest_half_counts(const OraclesGuest *guest, OraclesGuestHalfCounts *out)
{
    out->switches = guest ? guest->half_switches : 0;
    out->hooks_armed = guest ? guest->half_hooks_armed : 0;
    out->returns_dropped = guest ? guest->half_returns_dropped : 0;
    out->features_refused = guest ? guest->half_features_refused : 0;
    out->text_armed = guest ? guest->text_armed : 0;
    out->text_lost = guest ? guest->text_lost : 0;
}

void oracles_guest_detach(OraclesGuest *guest)
{
    if (!guest) return;
    if (!guest->reader) oracles_core_set_hooks(guest->core, NULL, NULL);
    free(guest->text_box_pages);
    free(guest);
}

const OraclesGuestTables *oracles_guest_tables(const OraclesGuest *guest) { return guest->tables; }
const OraclesCompatProfile *oracles_guest_profile(const OraclesGuest *guest) { return guest ? guest->profile : NULL; }

void oracles_guest_set_event_sink(OraclesGuest *guest, OraclesGuestEventFn sink, void *opaque)
{
    guest->sink = sink;
    guest->sink_opaque = opaque;
}

void oracles_guest_set_frame(OraclesGuest *guest, uint32_t frame) { guest->frame = frame; }

const OraclesGuestRegWrite *oracles_guest_journal(OraclesGuest *guest, size_t *count)
{
    *count = guest->journal_count;
    return guest->journal;
}

void oracles_guest_journal_clear(OraclesGuest *guest) { guest->journal_count = 0; }

/* ---- fingerprints ----------------------------------------------------------------- */

uint64_t oracles_guest_hash(const uint8_t *data, size_t size, uint64_t seed)
{
    uint64_t h = seed;
    for (size_t i = 0; i < size; i++) { h ^= data[i]; h *= 0x100000001b3ull; }
    return h;
}

typedef struct stack_region { uint16_t base, top; int thread; } stack_region; /* thread -1: main stack */

/* The live part of each stack of bank 0: from its stack pointer (the CPU's
 * for the running one, the saved one of a suspended thread) to its top. */
static void live_stacks(OraclesGuest *guest, stack_region regions[5], uint16_t live_from[5])
{
    const OraclesGuestTables *t = guest->tables;
    const stack_region all[5] = {
        { t->main_stack.addr, t->main_stack_top.addr, -1 },
        { t->thread0_stack.addr, t->thread0_stack_top.addr, 0 },
        { t->thread1_stack.addr, t->thread1_stack_top.addr, 1 },
        { t->thread2_stack.addr, t->thread2_stack_top.addr, 2 },
        { t->thread3_stack.addr, t->thread3_stack_top.addr, 3 },
    };
    const uint16_t cpu_sp = oracles_guest_sp(guest);
    for (unsigned i = 0; i < 5; i++) {
        regions[i] = all[i];
        live_from[i] = 0;
        if (cpu_sp >= all[i].base && cpu_sp < all[i].top) live_from[i] = cpu_sp;
        else if (all[i].thread >= 0) {
            const uint16_t saved = oracles_guest_thread_sp(guest, (unsigned)all[i].thread);
            if (saved >= all[i].base && saved < all[i].top) live_from[i] = saved;
        }
    }
}

int oracles_guest_live_dump(OraclesGuest *guest, uint8_t out[ORACLES_GUEST_LIVE_DUMP_BYTES])
{
    const OraclesGuestTables *t = guest->tables;
    const uint8_t *wram = oracles_guest_wram(guest, 0), *hram = oracles_guest_hram(guest);
    if (!wram || !hram) return -1;
    memcpy(out, wram, 0x1000u);
    memcpy(out + 0x1000u, hram, 127u);
    stack_region regions[5];
    uint16_t live_from[5];
    live_stacks(guest, regions, live_from);
    for (unsigned i = 0; i < 5; i++) {
        const uint16_t dead_end = live_from[i] ? live_from[i] : regions[i].top;   /* no stack pointer in it: nothing of it is live */
        memset(out + (regions[i].base - 0xc000u), 0, (size_t)(dead_end - regions[i].base));
    }
    const uint16_t queue = t->vblank_function_queue.addr, size = t->vblank_function_queue_size;
    if (queue >= 0xc000u && (unsigned)queue + size <= 0xd000u && t->vblank_function_queue_tail.addr >= 0xff80u) {
        const unsigned tail = hram[t->vblank_function_queue_tail.addr - 0xff80u];
        if (tail < size) memset(out + (queue - 0xc000u) + tail, 0, size - tail);
    }
    /* The scratch the game's own sources name as such: hFF8A to hFF93, and wTmpcec0 up to the room's layout. */
    const uint16_t hram_from = t->general_purpose_hram.addr, hram_to = t->general_purpose_hram_end.addr;
    if (hram_from >= 0xff80u && hram_to > hram_from) memset(out + 0x1000u + (hram_from - 0xff80u), 0, (size_t)(hram_to - hram_from));
    const uint16_t tmp_from = t->tmp_cec0.addr, tmp_to = t->tmp_cec0_end.addr;
    if (tmp_from >= 0xc000u && tmp_to > tmp_from && tmp_to <= 0xd000u) memset(out + (tmp_from - 0xc000u), 0, (size_t)(tmp_to - tmp_from));
    return 0;
}

uint64_t oracles_guest_live_wram_hash(OraclesGuest *guest)
{
    const uint8_t *wram = oracles_guest_wram(guest, 0);
    if (!wram) return 0;
    stack_region regions[5];
    uint16_t live_from[5];
    live_stacks(guest, regions, live_from);
    uint64_t h = ORACLES_HASH_SEED;
    unsigned cursor = 0xc000u;
    for (unsigned i = 0; i < 5; i++) {
        const stack_region *r = &regions[i];
        /* everything before this stack */
        h = oracles_guest_hash(wram + (cursor - 0xc000u), r->base - cursor, h);
        /* the live part of the stack: from its stack pointer to its top */
        if (live_from[i]) h = oracles_guest_hash(wram + (live_from[i] - 0xc000u), r->top - live_from[i], h);
        cursor = r->top;
    }
    /* the rest of bank 0, then banks 1 to 7 */
    h = oracles_guest_hash(wram + (cursor - 0xc000u), 0x1000u - (cursor - 0xc000u), h);
    h = oracles_guest_hash(wram + 0x1000u, 0x7000u, h);
    return h;
}
