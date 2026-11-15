/* Display › Picture's Filter (ui_page_nav.h, ui_page_layout.h): its layout against the layout reference,
 * tests/launcher_layout_reference.json (layout_reference.h), the test's argument, and its rules.  The reference's frames are
 * turn 43's: the row on Off with the preview of the last picture, in 16:9 and at 640x480 on Android in Near (43a, 43a
 * 640x480); Scanlines kept with Scaling highlighted, and Display's summary naming it (43b, 43b display, 43b display
 * 640x480); LCD from the pause, its note, the preview of the game (43c, 43c 640x480); the last picture with xBR, the sample
 * picture with Scanlines, and in 4:3 (43d, 43d sample, 43d 640x480, 43d sample 640x480); a device without the shader
 * filters (43e, 43e 640x480); a picture too small for the filter shown, a scale of 2.5, a kept filter in Near and then in
 * Far (43f, 43f lcd, 43f 2.5, 43f near, 43f far). */
#include "layout_reference.h"
#include "ui_page_layout.h"
#include "ui_page_nav.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); layout_fail(); } } while (0)

enum { OFF, SCANLINES, CRT, LCD, SMOOTH, XBR, HQ2X, OMNISCALE };

/* Oracle of Ages with its original ROM in Enhanced, Display › Picture open on Filter, at a `width` x `height` screen;
 * 4:3 as Android's 640x480 with a controller, in Near, else 16:9 at the keyboard. */
static void ages(OraclesHomeNav *nav, int four_three)
{
    oracles_home_init(nav);
    OraclesHomeGame *g = &nav->games[0];
    g->usable = 1;
    g->rom = ORACLES_ROM_ORIGINAL;
    nav->screen = ORACLES_SCREEN_DISPLAY;
    nav->display.profile = ORACLES_PROFILE_ENHANCED;
    nav->display.cores = 8;
    nav->layout = four_three ? ORACLES_UI_LAYOUT_4_3 : ORACLES_UI_LAYOUT_16_9;
    nav->display.screen_w = nav->display.room_w = four_three ? 640 : 1920;
    nav->display.screen_h = four_three ? 480 : 1080;
    nav->display.room_h = nav->display.screen_h - 64;
    if (four_three) {
        nav->android = nav->display.fullscreen_only = 1;
        nav->display.menus = 1;
        nav->display.view = 0;
        nav->display.window = 3;
    }
    oracles_home_input(nav, four_three ? ORACLES_INPUT_GAMEPAD : ORACLES_INPUT_KEYBOARD);
    snprintf(nav->display.last_picture, sizeof nav->display.last_picture, "Oracle of Ages");
    nav->row = ORACLES_DISPLAY_PICTURE;
    CHECK(oracles_display_act(nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav->picture_page);
    nav->row = ORACLES_DISPLAY_FILTER;
}

static void lay_out(const OraclesHomeNav *nav, OraclesUiDisplayTexts *t, OraclesUiDisplayLayout *l)
{
    oracles_ui_display_texts(nav, t);
    oracles_ui_layout_display(nav->layout, t, l);
}

/* The rows, Filter's value, arrows, name and marks, its explanation and note (4:3: the help's), the preview. */
static void filter_row(const char *frame, const OraclesUiDisplayTexts *t, const OraclesUiDisplayLayout *l, int four_three)
{
    static const char *const ids[6] = { "scaling", "filter", "margin", "color", "menus", "unvisited" };
    static const unsigned rows[6] = { ORACLES_DISPLAY_SCALING, ORACLES_DISPLAY_FILTER, ORACLES_DISPLAY_MARGIN, ORACLES_DISPLAY_COLOUR,
                                      ORACLES_DISPLAY_MENUS, ORACLES_DISPLAY_UNVISITED };
    char what[48];
    for (unsigned i = 0; i < 6u; i++) {
        snprintf(what, sizeof what, "%s row", ids[i]); layout_box(frame, what, &l->rows[rows[i]]);
        snprintf(what, sizeof what, "%s grid", ids[i]); layout_near(frame, what, "y", l->rows[rows[i]].y);
    }
    if (four_three) layout_line(frame, "filter label", &l->labels[ORACLES_DISPLAY_FILTER].lines[0]);
    else layout_text(frame, "filter label", &l->labels[ORACLES_DISPLAY_FILTER].lines[0]);
    layout_box(frame, "filter value", &l->filter_value);
    layout_box(frame, "filter prev", &l->filter_prev);
    layout_box(frame, "filter next", &l->filter_next);
    const OraclesUiBox *first = &l->filter_marks[0], *last = &l->filter_marks[ORACLES_DISPLAY_FILTERS - 1];
    const float marks_h = four_three ? 20.0f : 14.0f;
    const OraclesUiBox marks = { first->x, first->y - (marks_h - first->h) * 0.5f, last->x + last->w - first->x, marks_h };
    layout_box(frame, "filter marks", &marks);
    layout_near(frame, "filter name", "y", l->filter_name.y);
    if (four_three) {
        layout_line(frame, "filter name text", &l->filter_name);
        layout_line(frame, "help text", &l->help);
        if (t->filter_note[0]) layout_line(frame, "help note text", &l->help_note);
    } else {
        layout_text(frame, "filter name text", &l->filter_name);
        layout_text(frame, "filter explanation", &l->filter_explanation.lines[0]);
        if (t->filter_note[0]) layout_text(frame, "filter note", &l->filter_note);
        layout_box(frame, "panel", &l->panel);
    }
    if (t->filter_preview) {
        layout_box(frame, "preview", &l->filter_preview);
        if (four_three) layout_line(frame, "preview label text", &l->filter_caption.lines[0]);
        else layout_text(frame, "preview label text", &l->filter_caption.lines[0]);
    }
}

static void layouts(void)
{
    OraclesHomeNav nav;
    OraclesUiDisplayTexts t;
    OraclesUiDisplayLayout l;

    /* 43a: Off, its line, the preview of the last picture in the diagram's place. */
    ages(&nav, 0);
    lay_out(&nav, &t, &l);
    CHECK(t.filter == OFF && t.filter_ok && t.filter_preview && !strcmp(t.filter_explanation, "No filter: the game's own pixels.") && !t.filter_note[0]);
    CHECK(!strcmp(t.filter_caption, "Oracle of Ages, where you left it"));
    filter_row("43a", &t, &l, 0);
    ages(&nav, 1);
    lay_out(&nav, &t, &l);
    filter_row("43a 640x480", &t, &l, 1);
    CHECK(!strcmp(t.help, "No filter: the game's own pixels."));

    /* Every game's caption holds in the preview's width: on one line in 16:9, on two at most in 4:3, which wraps it. */
    static const char *const games[] = { "Oracle of Ages", "Oracle of Seasons", "Gifts of Kinomi", "Moonrise Regalia", "Temple of Seasons",
                                         "Oracles Combined" };
    for (int four_three = 0; four_three < 2; four_three++)
        for (unsigned i = 0; i < sizeof games / sizeof games[0]; i++) {
            ages(&nav, four_three);
            snprintf(nav.display.last_picture, sizeof nav.display.last_picture, "%s", games[i]);
            lay_out(&nav, &t, &l);
            float widest = 0.0f;
            for (unsigned n = 0; n < l.filter_caption.count; n++) if (l.filter_caption.lines[n].w > widest) widest = l.filter_caption.lines[n].w;
            CHECK(widest <= (four_three ? ORACLES_UI_FILTER_PREVIEW_W_4_3 : ORACLES_UI_FILTER_PREVIEW_W) && l.filter_caption.count <= (four_three ? 2u : 1u));
        }

    /* 43b: Scanlines kept, Scaling highlighted: no preview, the diagram back; Display's summary names it. */
    ages(&nav, 0);
    nav.display.filter = SCANLINES;
    nav.row = ORACLES_DISPLAY_SCALING;
    lay_out(&nav, &t, &l);
    CHECK(t.filter == SCANLINES && !t.filter_preview && l.filter_preview.w == 0.0f);
    filter_row("43b", &t, &l, 0);
    layout_box("43b", "diagram", &l.diagram);
    nav.picture_page = 0;
    nav.row = ORACLES_DISPLAY_PICTURE;
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.picture_summary, "Sharp \xc2\xb7 Scanlines \xc2\xb7 No margin \xc2\xb7 Color off"));
    layout_text("43b display", "picture summary text", &l.picture_summary);
    layout_text("43b display", "picture explanation", &l.picture_explanation.lines[0]);
    layout_box("43b display", "picture row", &l.rows[ORACLES_DISPLAY_PICTURE]);
    layout_box("43b display", "panel", &l.panel);
    ages(&nav, 1);
    nav.display.filter = SCANLINES;
    nav.picture_page = 0;
    nav.row = ORACLES_DISPLAY_PICTURE;
    lay_out(&nav, &t, &l);
    layout_line("43b display 640x480", "picture summary text", &l.picture_summary);
    layout_line("43b display 640x480", "help text", &l.help);
    layout_box("43b display 640x480", "picture row", &l.rows[ORACLES_DISPLAY_PICTURE]);

    /* 43c: LCD from the pause, "Applies at once.", the preview of the game. */
    ages(&nav, 0);
    nav.in_game = 1;
    nav.playing = ORACLES_PROFILE_ENHANCED;
    nav.display.filter = LCD;
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.filter_note, "Applies at once.") && !strcmp(t.filter_caption, "Your game, as it resumes"));
    filter_row("43c", &t, &l, 0);
    layout_text("43c", "page note", &l.page_note.lines[0]);
    ages(&nav, 1);
    nav.in_game = 1;
    nav.playing = ORACLES_PROFILE_ENHANCED;
    nav.display.filter = LCD;
    lay_out(&nav, &t, &l);
    filter_row("43c 640x480", &t, &l, 1);

    /* 43d: the last picture with xBR; the sample picture before any game, with Scanlines. */
    ages(&nav, 0);
    nav.display.filter = XBR;
    lay_out(&nav, &t, &l);
    filter_row("43d", &t, &l, 0);
    nav.display.filter = SCANLINES;
    nav.display.last_picture[0] = 0;
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.filter_caption, "Sample picture: play to see your own."));
    filter_row("43d sample", &t, &l, 0);
    ages(&nav, 1);
    nav.display.filter = LCD;
    lay_out(&nav, &t, &l);
    filter_row("43d 640x480", &t, &l, 1);
    nav.display.filter = XBR;
    nav.display.last_picture[0] = 0;
    lay_out(&nav, &t, &l);
    filter_row("43d sample 640x480", &t, &l, 1);

    /* 43e: no shader filter on this device: Scanlines and LCD shown greyed with its reason; Smooth after them kept. */
    ages(&nav, 0);
    nav.display.filters_basic = 1;
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.display.filter == OFF && nav.display.filter_shown == SCANLINES);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.display.filter == OFF && nav.display.filter_shown == CRT);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.display.filter == OFF && nav.display.filter_shown == LCD);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.filter == SMOOTH && nav.display.filter_shown == -1);
    nav.display.filter = OFF;
    nav.display.filter_shown = SCANLINES;
    lay_out(&nav, &t, &l);
    CHECK(t.filter == SCANLINES && !t.filter_ok && !strcmp(t.filter_note, "Not available on this device."));
    for (int f = 0; f < ORACLES_DISPLAY_FILTERS; f++) CHECK(!oracles_display_filter_reason(&nav, f) == (f == OFF || f == SMOOTH));
    filter_row("43e", &t, &l, 0);
    ages(&nav, 1);
    nav.display.filters_basic = 1;
    nav.display.filter_shown = LCD;
    lay_out(&nav, &t, &l);
    filter_row("43e 640x480", &t, &l, 1);

    /* 43f: Far filling 640x480 from the pause, 1.33: Scanlines and LCD too small; at 2.5 CRT still; Scanlines kept in
     * Near (3) runs, and in Far stays kept, greyed with the reason, the game drawn without it. */
    static const struct { const char *frame; int view, filter, shown; float scale; } small[] = {
        { "43f", 2, OFF, SCANLINES, 0.0f }, { "43f lcd", 2, OFF, LCD, 0.0f }, { "43f 2.5", 2, SCANLINES, CRT, 2.5f },
        { "43f near", 0, SCANLINES, -1, 0.0f }, { "43f far", 2, SCANLINES, -1, 0.0f },
    };
    for (unsigned i = 0; i < sizeof small / sizeof small[0]; i++) {
        ages(&nav, 1);
        nav.in_game = 1;
        nav.playing = ORACLES_PROFILE_ENHANCED;
        nav.display.scaling = 1;
        nav.display.view = small[i].view;
        nav.display.filter = small[i].filter;
        nav.display.filter_shown = small[i].shown;
        nav.display.picture_scale = small[i].scale;
        lay_out(&nav, &t, &l);
        filter_row(small[i].frame, &t, &l, 1);
    }
}

static void rules(void)
{
    OraclesHomeNav nav;
    static const char *const names[ORACLES_DISPLAY_FILTERS] = { "Off", "Scanlines", "CRT", "LCD", "Smooth", "xBR", "HQ2x", "OmniScale" };
    for (int f = 0; f < ORACLES_DISPLAY_FILTERS; f++) CHECK(!strcmp(oracles_display_filter_names[f], names[f]));
    CHECK(SMOOTH == ORACLES_DISPLAY_FILTER_SMOOTH && !strcmp(oracles_display_filter_explanation(SMOOTH), "Softens the pixels."));
    for (int f = -1; f <= ORACLES_DISPLAY_FILTERS; f++)
        CHECK(oracles_display_filter_shader(f) == (f != OFF && f != SMOOTH && f >= 0 && f < ORACLES_DISPLAY_FILTERS));
    CHECK(!strcmp(oracles_display_filter_explanation(LCD), "The grid of a Game Boy screen."));

    /* The scale: Sharp's whole step, Fill's real one (the smaller of the two), Android's step filling the screen. */
    ages(&nav, 1);
    nav.display.view = 2;
    nav.display.scaling = 1;
    const float fill = oracles_display_filter_scale(&nav);
    CHECK(fill > 1.33f && fill < 1.34f);
    nav.display.scaling = 0;
    CHECK(oracles_display_filter_scale(&nav) == 1.0f);
    nav.display.view = 0;
    CHECK(oracles_display_filter_scale(&nav) == 3.0f);
    nav.display.picture_scale = 2.5f;   /* the pause's own */
    CHECK(oracles_display_filter_scale(&nav) == 2.5f);

    /* Off and Smooth everywhere; under 2 the four, under 3 CRT and LCD too; the device's reason first. */
    nav.display.picture_scale = 1.5f;
    for (int f = 0; f < ORACLES_DISPLAY_FILTERS; f++)
        CHECK((oracles_display_filter_reason(&nav, f) == NULL) == (f == OFF || f == SMOOTH)
              && (f == OFF || f == SMOOTH || oracles_display_filter_reason(&nav, f) == oracles_display_filter_size_reason));
    nav.display.picture_scale = 2.0f;
    for (int f = 0; f < ORACLES_DISPLAY_FILTERS; f++) CHECK((oracles_display_filter_reason(&nav, f) == NULL) == (f != CRT && f != LCD));
    nav.display.picture_scale = 3.0f;
    for (int f = 0; f < ORACLES_DISPLAY_FILTERS; f++) CHECK(!oracles_display_filter_reason(&nav, f));
    nav.display.filters_basic = 1;
    nav.display.picture_scale = 1.0f;
    for (int f = 0; f < ORACLES_DISPLAY_FILTERS; f++)
        CHECK(f == OFF || f == SMOOTH ? !oracles_display_filter_reason(&nav, f) : oracles_display_filter_reason(&nav, f) == oracles_display_filter_device_reason);

    /* A kept filter the picture becomes too small for: kept, shown greyed with the reason, drawn as Off; back when the
     * picture is large enough again. */
    ages(&nav, 1);
    nav.display.filter = SCANLINES;
    CHECK(oracles_display_filter_drawn(&nav) == SCANLINES && oracles_display_filter_shown(&nav) == SCANLINES);
    nav.display.view = 2;
    nav.display.scaling = 1;
    CHECK(nav.display.filter == SCANLINES && oracles_display_filter_shown(&nav) == SCANLINES && oracles_display_filter_drawn(&nav) == OFF);
    CHECK(!strcmp(oracles_display_filter_note(&nav), "Needs a larger picture: try a nearer view or Faithful."));
    nav.display.view = 0;
    CHECK(oracles_display_filter_drawn(&nav) == SCANLINES && !oracles_display_filter_note(&nav)[0]);
    nav.display.filters_basic = 1;
    CHECK(oracles_display_filter_drawn(&nav) == OFF && !strcmp(oracles_display_filter_note(&nav), "Not available on this device."));

    /* Picture's summary: the filter after the scaling; in 16:9 the extras then neither named nor counted; in 4:3 named
     * while they hold, else counted. */
    char summary[96];
    ages(&nav, 0);
    nav.display.filter = CRT;
    nav.display.unvisited = 1;
    oracles_display_picture_summary(&nav, summary, sizeof summary);
    CHECK(!strcmp(summary, "Sharp \xc2\xb7 CRT \xc2\xb7 No margin \xc2\xb7 Color off"));
    nav.display.filter = OFF;
    oracles_display_picture_summary(&nav, summary, sizeof summary);
    CHECK(!strcmp(summary, "Sharp \xc2\xb7 No margin \xc2\xb7 Color off \xc2\xb7 Rooms hidden"));
    ages(&nav, 1);
    nav.display.filter = LCD;
    nav.display.unvisited = 1;
    oracles_display_picture_summary(&nav, summary, sizeof summary);
    CHECK(!strcmp(summary, "Sharp \xc2\xb7 LCD \xc2\xb7 No margin \xc2\xb7 Color off \xc2\xb7 Rooms hidden"));
    nav.display.menus = 0;   /* View menus, Android's extra */
    oracles_display_picture_summary(&nav, summary, sizeof summary);
    CHECK(!strcmp(summary, "Sharp \xc2\xb7 LCD \xc2\xb7 No margin \xc2\xb7 Color off \xc2\xb7 2 more"));
}

static void keys(void)
{
    OraclesHomeNav nav;
    /* Picture opens on Scaling; Filter under it, Screen margin under that. */
    ages(&nav, 0);
    nav.row = ORACLES_DISPLAY_SCALING;
    CHECK(oracles_display_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_FILTER);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_MARGIN);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_FILTER);
    /* Right and OK step forward through the eight, left back, round both ends; each kept. */
    for (int f = 1; f < ORACLES_DISPLAY_FILTERS; f++) CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.filter == f);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.display.filter == OFF);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.display.filter == OMNISCALE);
    /* ‹ and ›, by a click: the row highlighted and stepped. */
    nav.row = ORACLES_DISPLAY_SCALING;
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_FILTER, 1) == ORACLES_HOME_STORE && nav.row == ORACLES_DISPLAY_FILTER && nav.display.filter == OFF);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_FILTER, 0) == ORACLES_HOME_STORE && nav.display.filter == OMNISCALE);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_FILTER, -1) == ORACLES_HOME_STAY && nav.display.filter == OMNISCALE);
    /* A picture too small (1.33): the choices it refuses are shown, not kept; stepping goes on from the one shown; the
     * kept one shows again once the row loses the focus. */
    ages(&nav, 1);
    nav.display.view = 2;
    nav.display.scaling = 1;
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.display.filter == OFF && oracles_display_filter_shown(&nav) == SCANLINES);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && oracles_display_filter_shown(&nav) == CRT);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && oracles_display_filter_shown(&nav) == LCD);
    CHECK(oracles_display_filter_drawn(&nav) == OFF);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.filter == SMOOTH);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.display.filter == SMOOTH && oracles_display_filter_shown(&nav) == XBR);
    CHECK(oracles_display_filter_drawn(&nav) == SMOOTH);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && oracles_display_filter_shown(&nav) == SMOOTH);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && oracles_display_filter_shown(&nav) == SMOOTH);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && nav.display.filter == SMOOTH && oracles_display_filter_shown(&nav) == LCD);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && oracles_display_filter_shown(&nav) == CRT);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && oracles_display_filter_shown(&nav) == SCANLINES);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.display.filter == OFF);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && nav.display.filter == OFF && oracles_display_filter_shown(&nav) == OMNISCALE);
    /* Back leaves the page on Picture, the kept one shown. */
    CHECK(oracles_display_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && !nav.picture_page && nav.display.filter_shown == -1);
    /* From the pause it is live, in either profile. */
    ages(&nav, 0);
    nav.in_game = 1;
    nav.playing = ORACLES_PROFILE_FAITHFUL;
    CHECK(!oracles_display_row_fixed(&nav, ORACLES_DISPLAY_FILTER));
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.filter == SCANLINES);
    CHECK(!strcmp(oracles_display_filter_note(&nav), "Applies at once."));
}

int main(int argc, char **argv)
{
    if (argc < 2 || !layout_reference_load(argv[1])) { fprintf(stderr, "usage: oracles-test-launcher-filter launcher_layout_reference.json\n"); return 2; }
    if (!oracles_ui_fonts_load()) { fprintf(stderr, "FAIL the embedded fonts do not load\n"); return 1; }
    layouts();
    rules();
    keys();
    if (layout_failures()) {
        fprintf(stderr, "%d failures\n", layout_failures());
        return 1;
    }
    printf("launcher filter: ok\n");
    return 0;
}
