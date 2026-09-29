/* Interpolated values for the launcher's transitions, as CSS transitions
 * run them: a value moves from where it is to a new target along
 * a cubic Bézier timing curve, restarting from its current value when the
 * target changes mid-way.  Times are milliseconds on any monotonic clock. */
#ifndef ORACLES_UI_ANIM_H
#define ORACLES_UI_ANIM_H

typedef struct OraclesUiCurve {
    float x1, y1, x2, y2;
} OraclesUiCurve;

/* CSS `ease`, the default timing function, and `cubic-bezier(.2,.7,.2,1)`, the launcher's swift one. */
extern const OraclesUiCurve oracles_ui_ease;
extern const OraclesUiCurve oracles_ui_ease_swift;

/* The curve's progress at time fraction t in [0, 1]. */
float oracles_ui_curve_at(const OraclesUiCurve *curve, float t);

typedef struct OraclesUiTween {
    float from, to;
    double start_ms;
    float duration_ms;
    const OraclesUiCurve *curve;
} OraclesUiTween;

/* A value at rest. */
void oracles_ui_tween_jump(OraclesUiTween *tween, float value);
/* Moves towards `target` from the value at `now_ms`; a target already aimed at is left running. */
void oracles_ui_tween_to(OraclesUiTween *tween, float target, double now_ms, float duration_ms, const OraclesUiCurve *curve);
float oracles_ui_tween_value(const OraclesUiTween *tween, double now_ms);
int oracles_ui_tween_running(const OraclesUiTween *tween, double now_ms);

#endif
