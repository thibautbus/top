/* The swim's hold (guest.h, guest_swim.c): what a held button adds at a stroke's test, pure, which the continuous
 * transitions' swim at the line steps through a scroll too (room_transition.c). */
#include "guest.h"

uint8_t oracles_guest_mermaid_press(const OraclesGuestStrokeState *s)
{
    const uint8_t held = (uint8_t)(s->pressed & ORACLES_GAME_DIRECTIONS);
    /* Two opposite directions give no angle (@directionButtonToAngle, specialObjects.s): a stroke would only slow Link. */
    if (!held || (s->input_angle & 0x80u) || (s->just_pressed & ORACLES_GAME_DIRECTIONS)) return 0;
    if (!((uint8_t)(s->stroke_counter - 1u) & 0x80u)) return 0;   /* the stroke goes on this frame */
    const int below_cap = s->speed < s->speed_cap;
    const int turning = s->angle != s->input_angle;
    const int losing = (uint8_t)(s->step_counter + 1u) >= s->step_period;
    return below_cap || turning || losing ? held : 0;
}

uint8_t oracles_guest_flippers_press(uint8_t just_pressed, uint8_t pressed, uint8_t input_angle)
{
    /* A direction strokes only where it gives an angle: two opposite ones would send Link the way he faces. */
    const int held = (pressed & ORACLES_GAME_BUTTON_A) || ((pressed & ORACLES_GAME_DIRECTIONS) && !(input_angle & 0x80u));
    return held && !(just_pressed & ORACLES_GAME_BUTTON_A) ? ORACLES_GAME_BUTTON_A : 0;
}
