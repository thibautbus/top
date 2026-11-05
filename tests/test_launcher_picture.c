/* Display's Picture link and Display › Picture (ui_page_nav.h, ui_page_layout.h): their layouts against the layout
 * reference, tests/launcher_layout_reference.json (layout_reference.h), the test's argument, and their rules.  The
 * reference's frames are turn 34's: Display with Picture highlighted, its summary beside it, its hints saying Open, in
 * 16:9 (34a) and in 4:3 (34b); the page, Scaling highlighted, without Far size at 1920x1080 (34c); in 4:3, Screen margin
 * highlighted (34d); at 640x480 on Android with a controller, Large Android's default (34e); on a 1280x960 handheld,
 * Far size with its note on the help's note line (34f); from the pause, every row live (34g); in Faithful, Menus &
 * scenes dimmed, "Enhanced only." (34h); and Advanced's five rows, Camera highlighted (34i, 34i 4:3).  Turn 35's: the
 * page's last row, Unvisited rooms, Shown and Hidden (35a, 35b), in 4:3 (35c), at 640x480 on Android (35d), from the
 * pause (35e), in Faithful (35f), and Picture's summary naming the rooms hidden, or counting the extras (35g). */
#include "layout_reference.h"
#include "ui_page_layout.h"
#include "ui_page_nav.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); layout_fail(); } } while (0)

static const char *const ids[5] = { "scaling", "margin", "color", "menus", "unvisited" };
static const unsigned rows_of[5] = { ORACLES_DISPLAY_SCALING, ORACLES_DISPLAY_MARGIN, ORACLES_DISPLAY_COLOUR, ORACLES_DISPLAY_MENUS, ORACLES_DISPLAY_UNVISITED };
static const char *const choice_ids[5][4] = { { "sharp", "fill" }, { "off", "auto", "small", "large" }, { "off", "on" }, { "view", "large" }, { "shown", "hidden" } };

/* Oracle of Ages with its original ROM; Display at a `width` x `height` screen's sizes, after a key (or a controller's
 * button, `gamepad`). */
static void ages(OraclesHomeNav *nav, OraclesUiLayout layout, int width, int height, int gamepad)
{
    oracles_home_init(nav);
    OraclesHomeGame *g = &nav->games[0];
    g->usable = 1;
    g->rom = ORACLES_ROM_ORIGINAL;
    nav->layout = layout;
    nav->screen = ORACLES_SCREEN_DISPLAY;
    nav->display.profile = ORACLES_PROFILE_ENHANCED;
    nav->display.window = layout == ORACLES_UI_LAYOUT_4_3 ? 2 : 1;
    nav->display.cores = 8;
    nav->display.screen_w = nav->display.room_w = width;
    nav->display.screen_h = height;
    nav->display.room_h = height - 64;
    oracles_home_input(nav, gamepad ? ORACLES_INPUT_GAMEPAD : ORACLES_INPUT_KEYBOARD);
}

/* Display › Picture open from its link, then `row` highlighted. */
static void picture(OraclesHomeNav *nav, unsigned row)
{
    nav->row = ORACLES_DISPLAY_PICTURE;
    CHECK(oracles_display_act(nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav->picture_page && !nav->advanced && nav->row == ORACLES_DISPLAY_SCALING);
    nav->row = row;
}

static void lay_out(const OraclesHomeNav *nav, OraclesUiDisplayTexts *t, OraclesUiDisplayLayout *l)
{
    oracles_ui_display_texts(nav, t);
    oracles_ui_layout_display(nav->layout, t, l);
}

/* The help bar's hints against the frame's. */
static void hints(const char *frame, const OraclesHomeNav *nav, unsigned count)
{
    OraclesHomeHint h[ORACLES_HOME_MAX_HINTS];
    OraclesUiHintLayout l[ORACLES_HOME_MAX_HINTS];
    CHECK(oracles_home_hints(nav, h) == count);
    oracles_ui_layout_hints(nav->layout, h, count, l);
    for (unsigned i = 0; i < count; i++) {
        char name[32];
        snprintf(name, sizeof name, "hint %u", i); layout_box(frame, name, &l[i].box);
        snprintf(name, sizeof name, "hint %u key text", i); layout_line(frame, name, &l[i].key);
        snprintf(name, sizeof name, "hint %u label", i); layout_line(frame, name, &l[i].label);
    }
}

/* The page's rows, their labels and choices; 16:9's explanations and the section; 4:3's texts by their line boxes. */
static void rows(const char *frame, const OraclesUiDisplayTexts *t, const OraclesUiDisplayLayout *l, int four_three)
{
    const OraclesUiOptionLayout *options[5] = { l->scaling, l->margin, l->colour, l->menus, l->unvisited };
    const unsigned counts[5] = { 2, ORACLES_DISPLAY_MARGINS, 2, 2, 2 };
    char what[48];
    for (unsigned i = 0; i < 5u; i++) {
        const unsigned row = rows_of[i];
        snprintf(what, sizeof what, "%s row", ids[i]); layout_box(frame, what, &l->rows[row]);
        snprintf(what, sizeof what, "%s grid", ids[i]); layout_near(frame, what, "y", l->rows[row].y);
        snprintf(what, sizeof what, "%s label", ids[i]);
        if (four_three) layout_line(frame, what, &l->labels[row].lines[0]); else layout_text(frame, what, &l->labels[row].lines[0]);
        for (unsigned c = 0; c < counts[i]; c++) {
            snprintf(what, sizeof what, "%s %s", ids[i], choice_ids[i][c]); layout_box(frame, what, &options[i][c].box);
        }
    }
    for (unsigned c = 0; c < 2u; c++) {
        snprintf(what, sizeof what, "menus %s text", choice_ids[3][c]);
        if (four_three) layout_line(frame, what, &l->menus[c].name); else layout_text(frame, what, &l->menus[c].name);
        snprintf(what, sizeof what, "unvisited %s text", choice_ids[4][c]);
        if (four_three) layout_line(frame, what, &l->unvisited[c].name); else layout_text(frame, what, &l->unvisited[c].name);
    }
    layout_near(frame, "rows", "y", l->rows[ORACLES_DISPLAY_SCALING].y);
    layout_near(frame, "rows", "h", l->rows[ORACLES_DISPLAY_UNVISITED].y + l->rows[ORACLES_DISPLAY_UNVISITED].h - l->rows[ORACLES_DISPLAY_SCALING].y);
    CHECK(!strcmp(t->section, "Display \xe2\x80\xba Picture") && t->picture && !t->advanced);
    /* No link on the page, nor Display's own rows, nor the Advanced rows. */
    CHECK(l->link == ORACLES_DISPLAY_ROWS && l->rows[ORACLES_DISPLAY_ADVANCED].w == 0.0f && l->rows[ORACLES_DISPLAY_CORE].w == 0.0f
          && l->rows[ORACLES_DISPLAY_PROFILE].w == 0.0f);
    if (four_three) {
        layout_line(frame, "section", &l->head.section);
        layout_line(frame, "title", &l->title);
        layout_near(frame, "help note", "y", l->help_note.y);
        return;
    }
    layout_text(frame, "section", &l->head.section);
    layout_box(frame, "panel", &l->panel);
    layout_box(frame, "diagram", &l->diagram);
    layout_text(frame, "scaling explanation", &l->explanations[5].lines[0]);
    layout_text(frame, "scaling note", &l->scaling_note);
    layout_text(frame, "margin explanation", &l->explanations[7].lines[0]);
    layout_text(frame, "color explanation", &l->explanations[0].lines[0]);
    layout_text(frame, "menus explanation", &l->size_explanations[0].lines[0]);
    layout_near(frame, "menus explanation", "lines", (float)l->size_explanations[0].count);
    layout_text(frame, "unvisited explanation", &l->size_explanations[2].lines[0]);
    layout_near(frame, "unvisited explanation", "lines", (float)l->size_explanations[2].count);
}

static void layouts(void)
{
    OraclesHomeNav nav;
    OraclesUiDisplayTexts t;
    OraclesUiDisplayLayout l;

    /* 34a: the link in Color correction's place, its summary, its hints saying Open; Advanced where it was. */
    ages(&nav, ORACLES_UI_LAYOUT_16_9, 1920, 1080, 0);
    nav.row = ORACLES_DISPLAY_PICTURE;
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.picture_summary, "Sharp \xc2\xb7 No margin \xc2\xb7 Color off") && l.link == ORACLES_DISPLAY_ADVANCED && l.rows[ORACLES_DISPLAY_COLOUR].w == 0.0f);
    layout_text("34a", "Picture", &l.labels[ORACLES_DISPLAY_PICTURE].lines[0]);
    layout_box("34a", "picture row", &l.rows[ORACLES_DISPLAY_PICTURE]);
    layout_box("34a", "picture link", &l.picture_link);
    layout_text("34a", "picture summary text", &l.picture_summary);
    layout_text("34a", "picture explanation", &l.picture_explanation.lines[0]);
    layout_box("34a", "link", &l.text_link);
    layout_box("34a", "text row", &l.rows[ORACLES_DISPLAY_TEXT]);
    layout_text("34a", "Advanced", &l.advanced_label);
    layout_box("34a", "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);
    layout_box("34a", "panel", &l.panel);
    hints("34a", &nav, 3);
    OraclesHomeHint h[ORACLES_HOME_MAX_HINTS];
    oracles_home_hints(&nav, h);
    CHECK(!strcmp(h[1].label, "Open"));

    /* 34c: the page, the desktop's defaults, no Far size at 1920x1080. */
    picture(&nav, ORACLES_DISPLAY_SCALING);
    lay_out(&nav, &t, &l);
    CHECK(!t.far_size && l.rows[ORACLES_DISPLAY_FAR_SIZE].w == 0.0f && !oracles_display_row_shown(&nav, ORACLES_DISPLAY_FAR_SIZE));
    CHECK(!t.size_notes[0][0]);
    rows("34c", &t, &l, 0);
    layout_text("34c", "diagram label", &l.diagram_label);
    hints("34c", &nav, 4);

    /* 34b: 4:3, the link's short help in the header. */
    ages(&nav, ORACLES_UI_LAYOUT_4_3, 1440, 1080, 0);
    nav.row = ORACLES_DISPLAY_PICTURE;
    lay_out(&nav, &t, &l);
    layout_line("34b", "Picture", &l.labels[ORACLES_DISPLAY_PICTURE].lines[0]);
    layout_box("34b", "picture row", &l.rows[ORACLES_DISPLAY_PICTURE]);
    layout_box("34b", "picture link", &l.picture_link);
    layout_line("34b", "picture summary text", &l.picture_summary);
    layout_box("34b", "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);
    layout_line("34b", "help text", &l.help);
    CHECK(!strcmp(t.help, "Scaling, margin, colors, menus and scenes.") && !t.help_note);

    /* 34d: the page in 4:3, Screen margin highlighted. */
    picture(&nav, ORACLES_DISPLAY_MARGIN);
    lay_out(&nav, &t, &l);
    rows("34d", &t, &l, 1);
    layout_line("34d", "help text", &l.help);
    layout_box("34d", "diagram", &l.diagram);
    layout_box("34d", "diagram window", &l.diagram_window);

    /* 34e: 640x480 on Android with a controller, Large Android's default. */
    ages(&nav, ORACLES_UI_LAYOUT_4_3, 640, 480, 1);
    nav.android = 1;
    nav.display.fullscreen_only = 1;
    nav.display.menus = 1;
    picture(&nav, ORACLES_DISPLAY_MENUS);
    lay_out(&nav, &t, &l);
    rows("34e", &t, &l, 1);
    layout_line("34e", "help text", &l.help);
    CHECK(!strcmp(t.help, "Large: menus and scenes at their largest scale."));
    hints("34e", &nav, 4);

    /* 34f: a handheld far is fitted to: Far size before Menus & scenes, its note on the help's note line. */
    ages(&nav, ORACLES_UI_LAYOUT_4_3, 1280, 960, 1);
    nav.android = 1;
    nav.display.fullscreen_only = 1;
    picture(&nav, ORACLES_DISPLAY_FAR_SIZE);
    lay_out(&nav, &t, &l);
    CHECK(t.far_size && !strcmp(t.size_notes[1], "Here Fitted is 426\xc3\x97" "320 at 3\xc3\x97; Fixed is 480\xc3\x97" "360 at 2\xc3\x97."));
    layout_box("34f", "far row", &l.rows[ORACLES_DISPLAY_FAR_SIZE]);
    layout_box("34f", "menus row", &l.rows[ORACLES_DISPLAY_MENUS]);
    layout_box("34f", "unvisited row", &l.rows[ORACLES_DISPLAY_UNVISITED]);
    layout_line("34f", "far label", &l.labels[ORACLES_DISPLAY_FAR_SIZE].lines[0]);
    layout_box("34f", "far screen", &l.far_size[0].box);
    layout_box("34f", "far fixed", &l.far_size[1].box);
    layout_line("34f", "far screen text", &l.far_size[0].name);
    layout_line("34f", "help text", &l.help);
    layout_line("34f", "help note text", &l.help_note);
    CHECK(t.help_note == t.size_notes[1]);
    /* The keys pass over it in its place; aspect= naming a shape keeps far at its size, and the row goes. */
    nav.row = ORACLES_DISPLAY_COLOUR;
    CHECK(oracles_display_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_FAR_SIZE);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_MENUS);
    nav.display.aspect = 2;
    nav.row = ORACLES_DISPLAY_COLOUR;
    CHECK(oracles_display_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_MENUS);
    lay_out(&nav, &t, &l);
    CHECK(!t.far_size && l.rows[ORACLES_DISPLAY_FAR_SIZE].w == 0.0f);

    /* 34g: from the pause, every row live, the page's note under the title. */
    ages(&nav, ORACLES_UI_LAYOUT_16_9, 1920, 1080, 0);
    nav.in_game = 1;
    nav.playing = ORACLES_PROFILE_ENHANCED;
    picture(&nav, ORACLES_DISPLAY_COLOUR);
    lay_out(&nav, &t, &l);
    for (unsigned i = 0; i < 5u; i++) CHECK(!oracles_display_row_fixed(&nav, rows_of[i]));
    CHECK(oracles_display_size_row_live(&nav, ORACLES_DISPLAY_MENUS) && !oracles_display_size_row_live(&nav, ORACLES_DISPLAY_FAR_SIZE));
    rows("34g", &t, &l, 0);
    layout_text("34g", "page note", &l.page_note.lines[0]);

    /* 34h: Faithful, Menus & scenes dimmed, "Enhanced only."; the scaling, the margin and the colors live. */
    ages(&nav, ORACLES_UI_LAYOUT_16_9, 1920, 1080, 0);
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    picture(&nav, ORACLES_DISPLAY_MENUS);
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.size_notes[0], "Enhanced only.") && !oracles_display_size_row_live(&nav, ORACLES_DISPLAY_MENUS));
    rows("34h", &t, &l, 0);
    layout_text("34h", "menus note", &l.size_notes[0]);

    /* 35a, 35b: Unvisited rooms, the page's last row, Shown, then Hidden; Quality and the summary of the menus unmoved. */
    ages(&nav, ORACLES_UI_LAYOUT_16_9, 1920, 1080, 0);
    picture(&nav, ORACLES_DISPLAY_UNVISITED);
    lay_out(&nav, &t, &l);
    CHECK(!t.size_notes[2][0] && oracles_display_size_row_live(&nav, ORACLES_DISPLAY_UNVISITED));
    rows("35a", &t, &l, 0);
    hints("35a", &nav, 4);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.unvisited == 1);
    lay_out(&nav, &t, &l);
    rows("35b", &t, &l, 0);
    /* 35c: 4:3, its short help. */
    ages(&nav, ORACLES_UI_LAYOUT_4_3, 1440, 1080, 0);
    picture(&nav, ORACLES_DISPLAY_UNVISITED);
    lay_out(&nav, &t, &l);
    rows("35c", &t, &l, 1);
    layout_line("35c", "help text", &l.help);
    CHECK(!strcmp(t.help, "Hidden: rooms you never entered stay dark.") && !t.help_note);
    /* 35d: 640x480 on Android with a controller. */
    ages(&nav, ORACLES_UI_LAYOUT_4_3, 640, 480, 1);
    nav.android = 1;
    nav.display.fullscreen_only = 1;
    nav.display.menus = 1;
    picture(&nav, ORACLES_DISPLAY_UNVISITED);
    lay_out(&nav, &t, &l);
    rows("35d", &t, &l, 1);
    layout_line("35d", "help text", &l.help);
    hints("35d", &nav, 4);
    /* 35e: from the pause, live, when the game resumes. */
    ages(&nav, ORACLES_UI_LAYOUT_16_9, 1920, 1080, 0);
    nav.in_game = 1;
    nav.playing = ORACLES_PROFILE_ENHANCED;
    nav.display.unvisited = 1;
    picture(&nav, ORACLES_DISPLAY_UNVISITED);
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.size_notes[2], "Applies when the game resumes.") && oracles_display_size_row_live(&nav, ORACLES_DISPLAY_UNVISITED));
    rows("35e", &t, &l, 0);
    layout_text("35e", "page note", &l.page_note.lines[0]);
    layout_text("35e", "unvisited note", &l.size_notes[2]);
    /* 35f: Faithful, dimmed as Menus & scenes, "Enhanced only.". */
    ages(&nav, ORACLES_UI_LAYOUT_16_9, 1920, 1080, 0);
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    picture(&nav, ORACLES_DISPLAY_UNVISITED);
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.size_notes[2], "Enhanced only.") && !oracles_display_size_row_live(&nav, ORACLES_DISPLAY_UNVISITED));
    rows("35f", &t, &l, 0);
    layout_text("35f", "menus note", &l.size_notes[0]);
    layout_text("35f", "unvisited note", &l.size_notes[2]);
    /* 35g: Picture's summary with the rooms hidden; with Large menus too, counted; the 1280x960 handheld's worst case. */
    ages(&nav, ORACLES_UI_LAYOUT_16_9, 1920, 1080, 0);
    nav.display.unvisited = 1;
    nav.row = ORACLES_DISPLAY_PICTURE;
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.picture_summary, "Sharp \xc2\xb7 No margin \xc2\xb7 Color off \xc2\xb7 Rooms hidden"));
    layout_box("35g", "picture row", &l.rows[ORACLES_DISPLAY_PICTURE]);
    layout_box("35g", "picture link", &l.picture_link);
    layout_text("35g", "picture summary text", &l.picture_summary);
    layout_text("35g", "picture explanation", &l.picture_explanation.lines[0]);
    layout_box("35g", "panel", &l.panel);
    nav.display.menus = 1;
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.picture_summary, "Sharp \xc2\xb7 No margin \xc2\xb7 Color off \xc2\xb7 2 more"));
    layout_text("35g 2 more", "picture summary text", &l.picture_summary);
    ages(&nav, ORACLES_UI_LAYOUT_4_3, 1280, 960, 1);
    nav.android = 1;
    nav.display.fullscreen_only = 1;
    nav.display.far_fixed = 1;
    nav.display.menus = 0;   /* View: away from Android's Large */
    nav.display.unvisited = 1;
    nav.row = ORACLES_DISPLAY_PICTURE;
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.picture_summary, "Sharp \xc2\xb7 No margin \xc2\xb7 Color off \xc2\xb7 3 more"));
    layout_box("35g 4:3", "picture row", &l.rows[ORACLES_DISPLAY_PICTURE]);
    layout_box("35g 4:3", "picture link", &l.picture_link);
    layout_line("35g 4:3", "picture summary text", &l.picture_summary);
    /* In 4:3 two still hold on the line, as turn 34 named them. */
    nav.display.unvisited = 0;
    oracles_display_picture_summary(&nav, t.picture_summary, sizeof t.picture_summary);
    CHECK(!strcmp(t.picture_summary, "Sharp \xc2\xb7 No margin \xc2\xb7 Color off \xc2\xb7 Far fixed \xc2\xb7 View menus"));

    /* 34i: Advanced's five rows, Camera highlighted; no link. */
    for (int four_three = 0; four_three < 2; four_three++) {
        const char *frame = four_three ? "34i 4:3" : "34i";
        ages(&nav, four_three ? ORACLES_UI_LAYOUT_4_3 : ORACLES_UI_LAYOUT_16_9, four_three ? 1440 : 1920, 1080, 0);
        nav.row = ORACLES_DISPLAY_ADVANCED;
        CHECK(oracles_display_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.advanced && nav.row == ORACLES_DISPLAY_CORE);
        nav.row = ORACLES_DISPLAY_CAMERA;
        lay_out(&nav, &t, &l);
        CHECK(l.link == ORACLES_DISPLAY_ROWS && l.rows[ORACLES_DISPLAY_SCALING].w == 0.0f && l.rows[ORACLES_DISPLAY_MARGIN].w == 0.0f);
        layout_box(frame, "cam row", &l.rows[ORACLES_DISPLAY_CAMERA]);
        layout_box(frame, "cam Pixel box", &l.camera[0].box);
        layout_box(frame, "cam Smooth box", &l.camera[1].box);
        layout_box(frame, "workers row", &l.rows[ORACLES_DISPLAY_WORKERS]);
        layout_near(frame, "rows", "y", l.rows[ORACLES_DISPLAY_CORE].y);
        layout_near(frame, "rows", "h", l.rows[ORACLES_DISPLAY_CAMERA].y + l.rows[ORACLES_DISPLAY_CAMERA].h - l.rows[ORACLES_DISPLAY_CORE].y);
        layout_box(frame, "diagram", &l.diagram);
        if (four_three) layout_line(frame, "help text", &l.help);
        else layout_box(frame, "panel", &l.panel);
    }
    CHECK(!strcmp(t.help, "Smooth: diagonal walks glide."));
    nav.row = ORACLES_DISPLAY_ADVANCED;
    nav.advanced = 0;
    oracles_ui_display_texts(&nav, &t);
    CHECK(!strcmp(t.help, "Core, transitions, Vsync, workers, camera."));
}

/* The keys and the pointer: the link opens the page on Scaling, Back returns to the link; left and right change a live
 * row, a click its choice; nothing changes a dimmed one; the summary follows. */
static void keys(void)
{
    OraclesHomeNav nav;
    char summary[96];
    ages(&nav, ORACLES_UI_LAYOUT_16_9, 1920, 1080, 0);
    /* Display's own: Text, then Picture, then Advanced, going round; Color correction is no more among them. */
    nav.row = ORACLES_DISPLAY_TEXT;
    CHECK(oracles_display_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_PICTURE);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_ADVANCED);
    CHECK(!oracles_display_row_shown(&nav, ORACLES_DISPLAY_COLOUR) && oracles_display_row_shown(&nav, ORACLES_DISPLAY_PICTURE));
    nav.row = ORACLES_DISPLAY_PICTURE;
    CHECK(oracles_display_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && !nav.picture_page);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.picture_page && nav.row == ORACLES_DISPLAY_SCALING);
    CHECK(oracles_display_row_shown(&nav, ORACLES_DISPLAY_COLOUR) && !oracles_display_row_shown(&nav, ORACLES_DISPLAY_FAR_SIZE)
          && !oracles_display_row_shown(&nav, ORACLES_DISPLAY_PICTURE) && !oracles_display_row_shown(&nav, ORACLES_DISPLAY_CORE));
    CHECK(oracles_display_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_UNVISITED);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_MENUS);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_UNVISITED);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_SCALING);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.scaling == 1);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_MARGIN, 2) == ORACLES_HOME_STORE && nav.display.margin == 2);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_COLOUR, 1) == ORACLES_HOME_STORE && nav.display.colour == 1);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_MENUS, 1) == ORACLES_HOME_STORE && nav.display.menus == 1);
    oracles_display_picture_summary(&nav, summary, sizeof summary);
    CHECK(!strcmp(summary, "Fill \xc2\xb7 Small margin \xc2\xb7 Color on \xc2\xb7 Large menus"));
    /* Back: to the link. */
    CHECK(oracles_display_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && !nav.picture_page && nav.row == ORACLES_DISPLAY_PICTURE);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_PICTURE, -1) == ORACLES_HOME_STAY && nav.picture_page);
    /* Faithful: the scaling, the margin and the colors change, Menus & scenes does not, nor appears in the summary. */
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_COLOUR, 0) == ORACLES_HOME_STORE && nav.display.colour == 0);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_MENUS, 0) == ORACLES_HOME_STAY && nav.display.menus == 1);
    oracles_display_picture_summary(&nav, summary, sizeof summary);
    CHECK(!strcmp(summary, "Fill \xc2\xb7 Small margin \xc2\xb7 Color off"));
    /* The menus in the summary only away from the platform's default: Large on the desktops, View on Android. */
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.display.menus = 0;
    oracles_display_picture_summary(&nav, summary, sizeof summary);
    CHECK(!strcmp(summary, "Fill \xc2\xb7 Small margin \xc2\xb7 Color off"));
    nav.android = 1;
    oracles_display_picture_summary(&nav, summary, sizeof summary);
    CHECK(!strcmp(summary, "Fill \xc2\xb7 Small margin \xc2\xb7 Color off \xc2\xb7 View menus"));
    nav.display.menus = 1;
    oracles_display_picture_summary(&nav, summary, sizeof summary);
    CHECK(!strcmp(summary, "Fill \xc2\xb7 Small margin \xc2\xb7 Color off"));
    nav.android = 0;
    /* On a screen far is fitted to, Far size from the home screen, never from the pause; Far fixed in the summary. */
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    nav.display.screen_w = nav.display.room_w = 1280;
    nav.display.screen_h = 960;
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_FAR_SIZE, 1) == ORACLES_HOME_STORE && nav.display.far_fixed == 1);
    oracles_display_picture_summary(&nav, summary, sizeof summary);
    CHECK(!strcmp(summary, "Fill \xc2\xb7 Small margin \xc2\xb7 Color off \xc2\xb7 2 more"));   /* past the 16:9 line's twelve characters */
    nav.in_game = 1;
    nav.playing = ORACLES_PROFILE_ENHANCED;
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_FAR_SIZE, 0) == ORACLES_HOME_STAY && nav.display.far_fixed == 1);
    char note[ORACLES_HOME_TEXT_LENGTH];
    oracles_display_size_note(&nav, ORACLES_DISPLAY_FAR_SIZE, note, sizeof note);
    CHECK(!strcmp(note, "Change it from the home screen."));
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_MENUS, 0) == ORACLES_HOME_STORE && nav.display.menus == 0);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_SCALING, 0) == ORACLES_HOME_STORE && nav.display.scaling == 0);
    /* Unvisited rooms from the pause: it changes, when the game resumes; held by --unvisited-rooms, it does not, and says nothing. */
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_UNVISITED, 1) == ORACLES_HOME_STORE && nav.display.unvisited == 1);
    oracles_display_size_note(&nav, ORACLES_DISPLAY_UNVISITED, note, sizeof note);
    CHECK(!strcmp(note, "Applies when the game resumes."));
    nav.display.unvisited_fixed = 1;
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_UNVISITED, 0) == ORACLES_HOME_STAY && nav.display.unvisited == 1);
    oracles_display_size_note(&nav, ORACLES_DISPLAY_UNVISITED, note, sizeof note);
    CHECK(!note[0]);
    nav.display.unvisited_fixed = 0;
    /* A game running in Faithful: Menus & scenes dimmed whatever the profile chosen. */
    nav.playing = ORACLES_PROFILE_FAITHFUL;
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_MENUS, 1) == ORACLES_HOME_STAY);
    oracles_display_size_note(&nav, ORACLES_DISPLAY_MENUS, note, sizeof note);
    CHECK(!strcmp(note, "Enhanced only."));
    /* Display opened again shows its own rows. */
    nav.in_game = 0;
    nav.screen = ORACLES_SCREEN_HOME;
    nav.focus = 3;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_DISPLAY && !nav.advanced && !nav.picture_page);
}

int main(int argc, char **argv)
{
    if (argc < 2 || !layout_reference_load(argv[1])) { fprintf(stderr, "usage: oracles-test-launcher-picture launcher_layout_reference.json\n"); return 2; }
    if (!oracles_ui_fonts_load()) { fprintf(stderr, "FAIL the embedded fonts do not load\n"); return 1; }
    layouts();
    keys();
    if (layout_failures()) {
        fprintf(stderr, "%d failures\n", layout_failures());
        return 1;
    }
    printf("launcher picture: ok\n");
    return 0;
}
