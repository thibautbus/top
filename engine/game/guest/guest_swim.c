/* The swim's hold (guest.h): a held button starts the strokes a player would tap.
 *
 * linkUpdateVelocity@mermaidSuit (link.s, Ages) loads wGameKeysJustPressed in a and tests its direction bits: on a
 * new press it plays the stroke's splash and sets w1Link.var3e to 4, and while var3e, decremented each frame, stays at
 * or above zero, each frame steps Link's speed towards his cap (w1Link.speedTmp) and his angle towards the direction
 * held (wLinkAngle); once it is spent, a frame in w1Link.var13 takes a step of speed away (w1Link.var12 counts them),
 * Link keeping his angle.  The transaction adds the directions held only where a stroke serves: the last one spent or
 * ending, and Link below his cap, his angle not the one held, or his next frame losing a step.  Holding thus swims as
 * the best tapping does, with no more splashes than it needs.
 *
 * linkUpdateFlippersSpeed@flippersState0 (both games) tests A in wGameKeysJustPressed: a press starts a stroke, 13
 * frames raising speedTmp, then 12 lowering it, through which the routine reads no press, and back to state 0.  The
 * transaction adds A when A or a direction is held, A not newly pressed: a stroke follows the last as soon as it
 * ends, so that a direction held is the whole gesture, as with the suit; a direction counts where it gives an angle.  Both ways out of the test load a again
 * (@pressedA its state, the other wLinkAngle): the register is read by the test alone.
 *
 * Both run at the `and` that tests a, before it. */
#include "guest_internal.h"

uint16_t oracles_guest_find_stroke_test(const uint8_t *rom, size_t size, OraclesGuestSym routine, OraclesGuestSym keys_just_pressed, uint8_t mask)
{
    if (keys_just_pressed.bank == ORACLES_GUEST_ABSENT) return 0;
    const uint8_t test[] = { 0xfa, (uint8_t)keys_just_pressed.addr, (uint8_t)(keys_just_pressed.addr >> 8), 0xe6, mask };
    const uint16_t at = oracles_guest_find_in_routine(rom, size, routine, test, sizeof test);
    return at ? (uint16_t)(at + 3u) : 0;
}

/* The test of `routine` hooked with `event`; 0, or -1 when the game has the routine and its test is not found once. */
static int arm_test(OraclesGuest *guest, const uint8_t *rom, size_t size, OraclesGuestSym routine, uint8_t mask, OraclesGuestEventType event)
{
    if (routine.bank == ORACLES_GUEST_ABSENT) return 0;
    const uint16_t test = oracles_guest_find_stroke_test(rom, size, routine, guest->tables->game_keys_just_pressed, mask);
    if (!test) return -1;
    const OraclesGuestSym at = { routine.bank, test };
    return oracles_guest_add_hook(guest, at, event, 0) != 0 ? -1 : 0;
}

static int arm_tests(OraclesGuest *guest)
{
    const OraclesGuestTables *t = guest->tables;
    if (!oracles_compat_swim_hold(guest->profile)) return -1;
    size_t size = 0;
    uint16_t bank = 0;
    const uint8_t *rom = oracles_core_memory(guest->core, ORACLES_CORE_ROM, &size, &bank);
    if (arm_test(guest, rom, size, t->link_update_velocity_mermaid, ORACLES_GAME_DIRECTIONS, ORACLES_EVENT_MERMAID_STROKE) != 0) return -1;
    if (t->link_update_flippers_state0.bank == ORACLES_GUEST_ABSENT) return -1;
    return arm_test(guest, rom, size, t->link_update_flippers_state0, ORACLES_GAME_BUTTON_A, ORACLES_EVENT_FLIPPERS_STROKE);
}

int oracles_guest_set_swim_hold(OraclesGuest *guest, int on)
{
    if (!guest) return -1;
    if (on && !guest->swim_hold_armed) {
        if (arm_tests(guest) != 0) return -1;
        guest->swim_hold_armed = 1;
    }
    guest->swim_hold = on != 0;
    return 0;
}

int oracles_guest_arm_swim_hold(OraclesGuest *guest) { return oracles_guest_set_swim_hold(guest, 1); }

int oracles_guest_swim_hold(const OraclesGuest *guest) { return guest && guest->swim_hold; }

void oracles_guest_swim_strokes(const OraclesGuest *guest, unsigned *mermaid, unsigned *flippers)
{
    if (mermaid) *mermaid = guest ? guest->mermaid_strokes : 0;
    if (flippers) *flippers = guest ? guest->flippers_strokes : 0;
}

/* The registers at a test, when the routine works on Link's object (d): the fields read are his. */
static int at_link(OraclesGuest *guest, OraclesCoreRegisters *r)
{
    *r = oracles_core_registers(guest->core);
    return (r->de >> 8) == (guest->tables->link_speed_cap.addr >> 8);
}

static void add_to_a(OraclesGuest *guest, OraclesCoreRegisters *r, uint8_t bits)
{
    r->af = (uint16_t)((r->af & 0xffu) | (uint16_t)((r->af >> 8) | bits) << 8);
    oracles_core_set_registers(guest->core, r);
}

void oracles_guest_mermaid_stroke(OraclesGuest *guest)
{
    const OraclesGuestTables *t = guest->tables;
    OraclesCoreRegisters r;
    if (!guest->swim_hold) return;
    const uint8_t *link = oracles_guest_object(guest, 0, 0);
    if (!link || !at_link(guest, &r)) return;
    OraclesGuestStrokeState s;
    s.just_pressed = (uint8_t)(r.af >> 8);
    s.pressed = oracles_guest_read8(guest, t->game_keys_pressed);
    s.input_angle = oracles_guest_read8(guest, t->link_angle);
    s.angle = link[ORACLES_OBJ_ANGLE];
    s.speed = link[ORACLES_OBJ_SPEED];
    s.speed_cap = oracles_guest_read8(guest, t->link_speed_cap);
    s.step_counter = oracles_guest_read8(guest, t->link_speed_step_counter);
    s.step_period = oracles_guest_read8(guest, t->link_speed_step_period);
    s.stroke_counter = oracles_guest_read8(guest, t->link_stroke_counter);
    const uint8_t add = oracles_guest_mermaid_press(&s);
    if (!add) return;
    add_to_a(guest, &r, add);
    guest->mermaid_strokes++;
}

void oracles_guest_flippers_stroke(OraclesGuest *guest)
{
    OraclesCoreRegisters r;
    if (!guest->swim_hold || !at_link(guest, &r)) return;
    const OraclesGuestTables *t = guest->tables;
    const uint8_t add = oracles_guest_flippers_press((uint8_t)(r.af >> 8), oracles_guest_read8(guest, t->game_keys_pressed),
                                                     oracles_guest_read8(guest, t->link_angle));
    if (!add) return;
    add_to_a(guest, &r, add);
    guest->flippers_strokes++;
}
