/* The launcher's text measurement and transitions (ui_font.h, ui_gsub.h,
 * ui_anim.h): widths and line boxes against the "texts" of the layout
 * reference, tests/launcher_layout_reference.json (layout_reference.h), the
 * test's argument, measured as a browser lays text out with the vendored
 * fonts; glyphs as HarfBuzz shapes them; and the CSS timing curves, from their
 * definition. */
#include "layout_reference.h"
#include "ui_anim.h"
#include "ui_font.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void near(const char *what, float actual, float expected, float tolerance)
{
    if (fabsf(actual - expected) <= tolerance) return;
    fprintf(stderr, "FAIL %s: %.4f, expected %.4f\n", what, actual, expected);
    layout_fail();
}

/* A text's width against the reference's, the text alone in a block of that style. */
static void width(const char *probe, const OraclesUiTextStyle *style, const char *text)
{
    layout_near_within("texts", probe, "w", oracles_ui_text_width(style, text), 0.02f);
}

/* A style's line box against the reference's: its height, and its baseline from the line's top. */
static void line_box(const char *probe, const OraclesUiTextStyle *style, float line_height)
{
    float height, baseline;
    oracles_ui_line_box(style, line_height, &height, &baseline);
    layout_near("texts", probe, "h", height);
    const float expected = layout_reference("texts", probe, "baseline") - layout_reference("texts", probe, "top");
    if (fabsf(baseline - expected) > 0.0f) {
        fprintf(stderr, "FAIL %s baseline: %.3f from the line's top, the reference %.3f\n", probe, (double)baseline, (double)expected);
        layout_fail();
    }
}

static void widths(void)
{
    /* As a browser does without font hinting: advances and kerning scaled linearly, letter spacing after every character. */
    const OraclesUiTextStyle label = { ORACLES_UI_FONT_SERIF, 54.0f, 0.0f, 0 };
    const OraclesUiTextStyle title = { ORACLES_UI_FONT_SERIF, 120.0f, 0.01f, 0 };
    const OraclesUiTextStyle state = { ORACLES_UI_FONT_SANS, 24.0f, 0.16f, 1 };
    const OraclesUiTextStyle key = { ORACLES_UI_FONT_MONO, 19.0f, 0.0f, 0 };
    width("Start game, Marcellus 54", &label, "Start game");
    width("Ages, Marcellus 120, 0.01 em", &title, "Ages");
    width("state, Alegreya Sans 24, 0.16 em, uppercase", &state, "Ready \xc2\xb7 last session 21 Sep 2026");
    width("Enter / A, JetBrains Mono 19", &key, "Enter / A");
}

static void ligatures(void)
{
    /* "fi" is one glyph in Alegreya Sans, as a browser shapes it, and not once letter-spaced. */
    const OraclesUiTextStyle note = { ORACLES_UI_FONT_SANS, 24.0f, 0.0f, 0 };
    const OraclesUiTextStyle spaced = { ORACLES_UI_FONT_SANS, 24.0f, 0.16f, 0 };
    OraclesUiGlyph glyphs[16];
    if (oracles_ui_text_glyphs(&note, "first", glyphs, 16, NULL) != 4) { fprintf(stderr, "FAIL first: fi is not a ligature\n"); layout_fail(); }
    if (oracles_ui_text_glyphs(&spaced, "first", glyphs, 16, NULL) != 5) { fprintf(stderr, "FAIL first, letter-spaced: a ligature\n"); layout_fail(); }
    width("a ligature, Alegreya Sans 24", &note, "Drop the ROM on this window first");
    /* The contextual f of "ft" is another glyph than the f of "fa". */
    OraclesUiGlyph a[4], b[4];
    oracles_ui_text_glyphs(&note, "ft", a, 4, NULL);
    oracles_ui_text_glyphs(&note, "fa", b, 4, NULL);
    if (a[0].glyph == b[0].glyph) { fprintf(stderr, "FAIL ft: the contextual f is not substituted\n"); layout_fail(); }
}

static void line_boxes(void)
{
    /* Ascent, descent and line gap rounded each at the size; the leading's first half rounded down. */
    const OraclesUiTextStyle serif36 = { ORACLES_UI_FONT_SERIF, 36.0f, 0.0f, 0 };
    line_box("Marcellus 36", &serif36, 0.0f);
    const OraclesUiTextStyle serif72 = { ORACLES_UI_FONT_SERIF, 72.0f, 0.0f, 0 };
    line_box("Marcellus 72, line-height 1.05", &serif72, 72.0f * 1.05f);
    const OraclesUiTextStyle sans22 = { ORACLES_UI_FONT_SANS, 22.0f, 0.0f, 0 };
    line_box("Alegreya Sans 22", &sans22, 0.0f);   /* typographic metrics */
    layout_near("texts", "Alegreya Sans 22", "content_h", oracles_ui_content_height(&sans22));
    const OraclesUiTextStyle mono19 = { ORACLES_UI_FONT_MONO, 19.0f, 0.0f, 0 };
    line_box("JetBrains Mono 19", &mono19, 0.0f);
}

static void curves(void)
{
    /* Values of the CSS timing functions, from their definition. */
    near("ease at 0.5", oracles_ui_curve_at(&oracles_ui_ease, 0.5f), 0.8024f, 0.001f);
    near("ease at 0.25", oracles_ui_curve_at(&oracles_ui_ease, 0.25f), 0.4085f, 0.001f);
    near("cubic-bezier(.2,.7,.2,1) at 0.5", oracles_ui_curve_at(&oracles_ui_ease_swift, 0.5f), 0.9296f, 0.001f);
    near("curve at 0", oracles_ui_curve_at(&oracles_ui_ease_swift, 0.0f), 0.0f, 0.0f);
    near("curve at 1", oracles_ui_curve_at(&oracles_ui_ease_swift, 1.0f), 1.0f, 0.0f);
    /* A value retargeted half-way starts from where it is. */
    OraclesUiTween t;
    oracles_ui_tween_jump(&t, 0.0f);
    oracles_ui_tween_to(&t, 1.0f, 1000.0, 400.0f, &oracles_ui_ease);
    const float mid = oracles_ui_tween_value(&t, 1200.0);
    near("tween half-way", mid, 0.8024f, 0.001f);
    oracles_ui_tween_to(&t, 0.0f, 1200.0, 400.0f, &oracles_ui_ease);
    near("tween retargeted", oracles_ui_tween_value(&t, 1200.0), mid, 0.0001f);
    near("tween done", oracles_ui_tween_value(&t, 1600.0), 0.0f, 0.0f);
    if (oracles_ui_tween_running(&t, 1600.0)) { fprintf(stderr, "FAIL tween still running\n"); layout_fail(); }
}

int main(int argc, char **argv)
{
    if (argc < 2 || !layout_reference_load(argv[1])) { fprintf(stderr, "usage: oracles-test-launcher-text launcher_layout_reference.json\n"); return 2; }
    if (!oracles_ui_fonts_load()) { fprintf(stderr, "FAIL the embedded fonts do not load\n"); return 1; }
    widths();
    ligatures();
    line_boxes();
    curves();
    if (layout_failures()) { fprintf(stderr, "%d failure(s)\n", layout_failures()); return 1; }
    printf("launcher text: widths, ligatures, line boxes and timing curves as the browser's\n");
    return 0;
}
