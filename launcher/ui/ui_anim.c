#include "ui_anim.h"

#include <math.h>

const OraclesUiCurve oracles_ui_ease = { 0.25f, 0.1f, 0.25f, 1.0f };
const OraclesUiCurve oracles_ui_ease_swift = { 0.2f, 0.7f, 0.2f, 1.0f };

static float bezier(float p1, float p2, float s)
{
    const float u = 1.0f - s;
    return 3.0f * u * u * s * p1 + 3.0f * u * s * s * p2 + s * s * s;
}

static float bezier_slope(float p1, float p2, float s)
{
    const float u = 1.0f - s;
    return 3.0f * u * u * p1 + 6.0f * u * s * (p2 - p1) + 3.0f * s * s * (1.0f - p2);
}

float oracles_ui_curve_at(const OraclesUiCurve *curve, float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    /* Solve x(s) = t for the curve parameter: Newton's method, bisection when the slope is flat. */
    float s = t;
    for (int i = 0; i < 8; i++) {
        const float error = bezier(curve->x1, curve->x2, s) - t;
        if (fabsf(error) < 1e-6f) return bezier(curve->y1, curve->y2, s);
        const float slope = bezier_slope(curve->x1, curve->x2, s);
        if (fabsf(slope) < 1e-6f) break;
        s -= error / slope;
    }
    float low = 0.0f, high = 1.0f;
    s = t;
    for (int i = 0; i < 40; i++) {
        const float x = bezier(curve->x1, curve->x2, s);
        if (fabsf(x - t) < 1e-6f) break;
        if (x < t) low = s; else high = s;
        s = (low + high) * 0.5f;
    }
    return bezier(curve->y1, curve->y2, s);
}

void oracles_ui_tween_jump(OraclesUiTween *tween, float value)
{
    tween->from = tween->to = value;
    tween->start_ms = 0.0;
    tween->duration_ms = 0.0f;
    tween->curve = &oracles_ui_ease;
}

void oracles_ui_tween_to(OraclesUiTween *tween, float target, double now_ms, float duration_ms, const OraclesUiCurve *curve)
{
    if (tween->to == target) return;
    tween->from = oracles_ui_tween_value(tween, now_ms);
    tween->to = target;
    tween->start_ms = now_ms;
    tween->duration_ms = duration_ms;
    tween->curve = curve;
}

float oracles_ui_tween_value(const OraclesUiTween *tween, double now_ms)
{
    if (tween->duration_ms <= 0.0f || now_ms >= tween->start_ms + tween->duration_ms) return tween->to;
    const float t = now_ms <= tween->start_ms ? 0.0f : (float)((now_ms - tween->start_ms) / tween->duration_ms);
    return tween->from + (tween->to - tween->from) * oracles_ui_curve_at(tween->curve, t);
}

int oracles_ui_tween_running(const OraclesUiTween *tween, double now_ms)
{
    return tween->duration_ms > 0.0f && now_ms < tween->start_ms + tween->duration_ms && tween->from != tween->to;
}
