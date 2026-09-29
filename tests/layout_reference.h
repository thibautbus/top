/* The launcher's layout reference, for the layout tests: tests/launcher_layout_reference.json, the measures the
 * layout tests compare with; to change the layout on purpose, update the values of the probes it moves.  Each
 * frame of the file holds named probes: a text's left, width, line box and
 * baseline, or an element's box, in the scene's pixels at 1920x1080; "texts" holds the fonts' own measures.
 *
 * A test loads the file (its path is the test's argument), compares its layout with a probe by name, within
 * LAYOUT_TOLERANCE, and a difference says which probe and how to update it. */
#ifndef ORACLES_TESTS_LAYOUT_REFERENCE_H
#define ORACLES_TESTS_LAYOUT_REFERENCE_H

#include "ui_home_layout.h"

#define LAYOUT_TOLERANCE 0.1f

/* 1 when the file is read; 0, with the reason on stderr, otherwise. */
int layout_reference_load(const char *path);
/* A quantity of a probe ("x", "w", "top", "h", "baseline", "width", "glyph_top", "glyph_h", "y"); a probe or a
 * quantity the file lacks is a failure, and gives 0. */
float layout_reference(const char *frame, const char *probe, const char *quantity);
/* `actual` against a quantity of a probe, within LAYOUT_TOLERANCE or `tolerance`. */
void layout_near(const char *frame, const char *probe, const char *quantity, float actual);
void layout_near_within(const char *frame, const char *probe, const char *quantity, float actual, float tolerance);
/* A line against a text probe: left, width, line box top and height, baseline. */
void layout_line(const char *frame, const char *probe, const OraclesUiLine *line);
/* Left, width and baseline only (a line whose box the layout does not keep). */
void layout_text(const char *frame, const char *probe, const OraclesUiLine *line);
/* Left, width and top of its line box. */
void layout_line_top(const char *frame, const char *probe, const OraclesUiLine *line);
/* A box against an element's. */
void layout_box(const char *frame, const char *probe, const OraclesUiBox *box);
/* Failures so far, the checks of a test's own included. */
int layout_failures(void);
void layout_fail(void);

#endif
