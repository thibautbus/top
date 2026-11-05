/* Display's Text link and Display › Text (ui_page_text.h, ui_page_layout.h): their layouts against the layout
 * reference, tests/launcher_layout_reference.json (layout_reference.h), the test's argument, their rows' rules and the
 * preview. The reference's frames are turn 26's: the page with Compact and Wide chosen, Text box highlighted, in 16:9
 * (26a), in 4:3 (26b) and at 640x480 on Android (26c), Text box's savestate note under it (the help's note line in
 * 4:3); Display with the link highlighted (26e, text link 4:3); from the pause (26g, 26g 4:3 on Text, turn 29's), Text
 * and Text box dimmed with their reason, Text size live; in Faithful (26h); and Moonrise Regalia's (26i), where the
 * canvas leaves the page out (its rows live there since the fan games have the wide box and Text size, measured again);
 * each measured again from turn 31, which
 * adds Text speed under Text size, in 4:3 under a smaller preview.  Turn 31's own: Instant chosen and highlighted in
 * 16:9 (31a), in 4:3 (31c), at 640x480 on Android (31d), in the pause (31g); in Faithful, Display's link live (31e) and
 * the page in 4:3, Text speed alone live (31e 4:3, 31e text 4:3 on Text); a fan game's page (31f); the link's summary
 * naming Instant (31h, 31h 4:3), left out with Text speed dimmed for an unrecognised ROM.  The canvas's Lines row is not
 * there: the engine draws two lines alone. */
#include "home_fixture.h"
#include "layout_reference.h"
#include "ui_page_layout.h"
#include "ui_page_nav.h"
#include "text_box_enlarge.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); layout_fail(); } } while (0)

static const char *const ids[ORACLES_DISPLAY_TEXT_ROWS] = { "text", "box", "tsize", "tspeed" };
static const char *const choice_ids[ORACLES_DISPLAY_TEXT_ROWS][3] = { { "classic", "compact", NULL }, { "standard", "wide", NULL }, { "normal", "large", "xlarge" },
                                                                      { "classic", "instant", NULL } };

/* Oracle of Ages with its original ROM; Display at a 1920x1080 (or `width` x `height`) screen's sizes. */
static void ages(OraclesHomeNav *nav, OraclesUiLayout layout, int width, int height)
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
    oracles_home_input(nav, ORACLES_INPUT_KEYBOARD);
}

/* Display › Text open, Compact and Wide chosen, `row` highlighted. */
static void text_page(OraclesHomeNav *nav, unsigned row)
{
    nav->display.text = nav->display.text_box = 1;
    nav->row = ORACLES_DISPLAY_TEXT;
    CHECK(oracles_display_act(nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav->text_page
          && nav->row == (nav->in_game ? ORACLES_DISPLAY_TEXT_SIZE : nav->display.profile == ORACLES_PROFILE_FAITHFUL ? ORACLES_DISPLAY_TEXT_SPEED
                                         : ORACLES_DISPLAY_LETTERS));
    nav->row = row;
}

static void lay_out(const OraclesHomeNav *nav, OraclesUiDisplayTexts *t, OraclesUiDisplayLayout *l)
{
    oracles_ui_display_texts(nav, t);
    oracles_ui_layout_display(nav->layout, t, l);
}

/* The rows, their labels and choices, and the preview; 4:3's texts by their line boxes too. */
static void page_probes(const char *frame, const OraclesUiDisplayLayout *l, int four_three)
{
    const OraclesUiOptionLayout *options[ORACLES_DISPLAY_TEXT_ROWS] = { l->text, l->text_box, l->text_size, l->text_speed };
    char what[48];
    for (unsigned i = 0; i < ORACLES_DISPLAY_TEXT_ROWS; i++) {
        const unsigned row = ORACLES_DISPLAY_LETTERS + i;
        snprintf(what, sizeof what, "%s row", ids[i]); layout_box(frame, what, &l->rows[row]);
        snprintf(what, sizeof what, "%s grid", ids[i]); layout_near(frame, what, "y", l->rows[row].y);
        snprintf(what, sizeof what, "%s label", ids[i]);
        if (four_three) layout_line(frame, what, &l->labels[row].lines[0]); else layout_text(frame, what, &l->labels[row].lines[0]);
        for (unsigned c = 0; c < 3u && choice_ids[i][c]; c++) {
            snprintf(what, sizeof what, "%s %s", ids[i], choice_ids[i][c]); layout_box(frame, what, &options[i][c].box);
            snprintf(what, sizeof what, "%s %s text", ids[i], choice_ids[i][c]);
            if (four_three) layout_line(frame, what, &options[i][c].name); else layout_text(frame, what, &options[i][c].name);
        }
        if (!four_three) {
            snprintf(what, sizeof what, "%s explanation", ids[i]);
            layout_near(frame, what, "top", l->text_explanations[i].lines[0].y);
            layout_near(frame, what, "lines", (float)l->text_explanations[i].count);
            layout_near(frame, what, "width", l->text_explanations[i].w);
        }
    }
    layout_near(frame, "rows", "y", l->rows[ORACLES_DISPLAY_LETTERS].y);
    layout_box(frame, "preview", &l->preview);
    layout_box(frame, "preview frame", &l->preview_frame);
    layout_box(frame, "preview hud", &l->preview_hud);
    layout_box(frame, "preview box", &l->preview_box);
    layout_near(frame, "preview label", "y", l->preview_label.y);
    layout_near(frame, "preview label", "h", l->preview_label.h);
    layout_text(frame, "preview label text", &l->preview_label);
    layout_near(frame, "preview note", "x", l->preview_note.x);
    layout_near(frame, "preview note", "y", l->preview_note.y);
    if (four_three) {
        layout_line(frame, "section", &l->head.section);
        layout_line(frame, "title", &l->title);
        layout_near(frame, "help", "y", l->help.y);
        layout_near(frame, "help note", "y", l->help_note.y);
    } else {
        layout_text(frame, "section", &l->head.section);
        layout_box(frame, "panel", &l->panel);
    }
    /* No diagram, no Advanced on the page. */
    CHECK(l->diagram.w == 0.0f && l->rows[ORACLES_DISPLAY_ADVANCED].w == 0.0f);
}

static void layouts(void)
{
    OraclesHomeNav nav;
    OraclesUiDisplayTexts t;
    OraclesUiDisplayLayout l;

    /* 26a: the page in 16:9, Far's band and the wide box in it. */
    ages(&nav, ORACLES_UI_LAYOUT_16_9, 1920, 1080);
    text_page(&nav, ORACLES_DISPLAY_TEXT_BOX);
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.section, "Display \xe2\x80\xba Text") && t.text_page && !t.later);
    CHECK(!strcmp(t.preview.label, "Far view, 480\xc3\x97" "270 \xc2\xb7 box 208\xc3\x97" "40") && !t.preview.note[0]);
    page_probes("26a", &l, 0);
    for (unsigned i = 0; i < ORACLES_DISPLAY_TEXT_ROWS; i++) CHECK((l.text_notes[i].h == 0.0f) == (i > 1u) && oracles_display_text_row_live(&nav, ORACLES_DISPLAY_LETTERS + i));
    CHECK(!strcmp(t.text_notes[0], "Savestates taken with Compact need Compact."));   /* as Continuous transitions says of its own */
    CHECK(!strcmp(t.text_notes[1], "Savestates taken with Wide need Wide."));
    layout_text("26a", "box note", &l.text_notes[1]);

    /* 31a: the view Medium, Instant chosen and highlighted. */
    nav.display.view = 1;
    nav.display.text_speed = 1;
    nav.row = ORACLES_DISPLAY_TEXT_SPEED;
    lay_out(&nav, &t, &l);
    page_probes("31a", &l, 0);
    layout_text("31a", "preview label text", &l.preview_label);
    CHECK(!t.text_notes[3][0] && !strcmp(t.text_explanations[3], "Classic: letter by letter, at the speed chosen on the file screen. Instant: each line "
                                                                  "appears whole; A still turns the page."));
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_TEXT_SPEED, 0) == ORACLES_HOME_STORE && nav.display.text_speed == 0);
    nav.display.view = 2;

    /* 26h: in Faithful, the rows dimmed and inert but Text speed, "Enhanced only." under them, the game's band and box. */
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    nav.row = ORACLES_DISPLAY_TEXT_BOX;
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.preview.label, "Faithful: the game's box, 144\xc3\x97" "40"));
    page_probes("26h", &l, 0);
    for (unsigned i = 0; i < 3u; i++) CHECK(!oracles_display_text_row_live(&nav, ORACLES_DISPLAY_LETTERS + i) && !strcmp(t.text_notes[i], "Enhanced only."));
    CHECK(oracles_display_text_row_live(&nav, ORACLES_DISPLAY_TEXT_SPEED) && !t.text_notes[3][0] && oracles_display_text_link_applies(&nav));
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.display.text_box == 1);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_LETTERS, 0) == ORACLES_HOME_STAY && nav.display.text == 1);

    /* 26g: from the pause, Text and Text box dimmed and inert, "Change it from the home screen." under them; Text size
     * applies at once. */
    ages(&nav, ORACLES_UI_LAYOUT_16_9, 1920, 1080);
    nav.in_game = 1;
    nav.playing = ORACLES_PROFILE_ENHANCED;
    text_page(&nav, ORACLES_DISPLAY_TEXT_BOX);
    lay_out(&nav, &t, &l);
    page_probes("26g", &l, 0);
    CHECK(!strcmp(t.text_notes[0], "Change it from the home screen.") && !strcmp(t.text_notes[1], t.text_notes[0]) && !t.text_notes[2][0]);
    layout_text("26g", "text note", &l.text_notes[0]);
    layout_text("26g", "box note", &l.text_notes[1]);
    layout_text("26g", "page note", &l.page_note.lines[0]);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.display.text_box == 1);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_LETTERS, 0) == ORACLES_HOME_STAY && nav.display.text == 1);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_TEXT_SIZE, 2) == ORACLES_HOME_STORE && nav.display.text_size == 2);
    /* Text speed takes the game when it resumes: live, without a note (31g, Medium, Instant highlighted). */
    nav.display.text_size = 0;
    nav.display.view = 1;
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_TEXT_SPEED, 1) == ORACLES_HOME_STORE && nav.display.text_speed == 1);
    lay_out(&nav, &t, &l);
    page_probes("31g", &l, 0);
    layout_text("31g", "text note", &l.text_notes[0]);
    layout_text("31g", "box note", &l.text_notes[1]);
    CHECK(!t.text_notes[3][0]);
    /* A game whose profile has no instant text (an unrecognised ROM): dimmed, saying why. */
    nav.text_speed_refused = 1;
    lay_out(&nav, &t, &l);
    CHECK(!oracles_display_text_row_live(&nav, ORACLES_DISPLAY_TEXT_SPEED) && !strcmp(t.text_notes[3], "Needs an original ROM."));
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_TEXT_SPEED, 0) == ORACLES_HOME_STAY && nav.display.text_speed == 1);
    nav.text_speed_refused = 0;
    nav.display.text_size = 2;
    nav.display.view = 2;
    /* By the running game's profile: one in Faithful takes no size. */
    nav.playing = ORACLES_PROFILE_FAITHFUL;
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_TEXT_SIZE, 0) == ORACLES_HOME_STAY && nav.display.text_size == 2);

    /* 26i: Moonrise Regalia's page, its Text, Text box and Text size live as an Oracle's, the wide box in the preview; its
     * title on two lines moves the preview down. */
    ages(&nav, ORACLES_UI_LAYOUT_16_9, 1920, 1080);
    nav.games[ORACLES_HOME_GAME_MOONRISE] = nav.games[0];
    nav.games[ORACLES_HOME_GAME_MOONRISE].refuses = FAN_REFUSES;
    nav.games[ORACLES_HOME_GAME_MOONRISE].fan = oracles_home_fan_game(ORACLES_HOME_GAME_MOONRISE);
    nav.entry = ORACLES_HOME_FAN;
    nav.fan_pick = ORACLES_HOME_HERO_MOONRISE;
    text_page(&nav, ORACLES_DISPLAY_TEXT_BOX);
    lay_out(&nav, &t, &l);
    page_probes("26i", &l, 0);
    CHECK(!strcmp(t.preview.label, "Far view, 480\xc3\x97" "270 \xc2\xb7 box 208\xc3\x97" "40"));
    layout_text("26i", "title", &l.head.title.lines[0]);
    layout_text("26i", "box note", &l.text_notes[1]);
    CHECK(!strcmp(t.text_notes[1], "Savestates taken with Wide need Wide.") && !strcmp(t.text_notes[0], "Savestates taken with Compact need Compact.")
          && !t.text_notes[2][0]);
    CHECK(oracles_display_text_row_live(&nav, ORACLES_DISPLAY_LETTERS) && oracles_display_text_row_live(&nav, ORACLES_DISPLAY_TEXT_BOX)
          && oracles_display_text_row_live(&nav, ORACLES_DISPLAY_TEXT_SIZE));
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_LETTERS, 0) == ORACLES_HOME_STORE && nav.display.text == 0);
    /* Its link sums up its text, its box and its size, and Instant once chosen. */
    nav.display.text_box = 1;
    nav.display.text_size = 2;
    char fan_summary[64];
    oracles_display_text_summary(&nav, fan_summary, sizeof fan_summary);
    CHECK(!strcmp(fan_summary, "Classic \xc2\xb7 Wide \xc2\xb7 Very large"));
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_TEXT_SPEED, 1) == ORACLES_HOME_STORE && nav.display.text_speed == 1);
    oracles_display_text_summary(&nav, fan_summary, sizeof fan_summary);
    CHECK(!strcmp(fan_summary, "Classic \xc2\xb7 Wide \xc2\xb7 Very large \xc2\xb7 Instant"));
    /* By its profile, not by its being a fan game: Text box and Text size refused are dimmed with the reason, inert, left
     * out of the summary, and the preview shows the game's box. */
    nav.games[ORACLES_HOME_GAME_MOONRISE].refuses |= ORACLES_HOME_REFUSES_TEXT_BOX | ORACLES_HOME_REFUSES_TEXT_SIZE;
    CHECK(!oracles_display_text_row_live(&nav, ORACLES_DISPLAY_TEXT_BOX) && !oracles_display_text_row_live(&nav, ORACLES_DISPLAY_TEXT_SIZE));
    CHECK(!strcmp(oracles_display_text_note(&nav, ORACLES_DISPLAY_TEXT_BOX), "Ages and Seasons only for now.")
          && !strcmp(oracles_display_text_note(&nav, ORACLES_DISPLAY_TEXT_SIZE), "Ages and Seasons only for now."));
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_TEXT_BOX, 0) == ORACLES_HOME_STAY && nav.display.text_box == 1);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_TEXT_SIZE, 1) == ORACLES_HOME_STAY && nav.display.text_size == 2);
    oracles_display_text_summary(&nav, fan_summary, sizeof fan_summary);
    CHECK(!strcmp(fan_summary, "Classic \xc2\xb7 Instant"));
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.preview.label, "Far view, 480\xc3\x97" "270 \xc2\xb7 box 144\xc3\x97" "40"));
    nav.games[ORACLES_HOME_GAME_MOONRISE].refuses = FAN_REFUSES;
    /* 31f: Text speed highlighted, Compact chosen. */
    nav.display.text = 1;
    nav.display.text_box = nav.display.text_size = 0;
    nav.row = ORACLES_DISPLAY_TEXT_SPEED;
    lay_out(&nav, &t, &l);
    page_probes("31f", &l, 0);
    layout_text("31f", "box note", &l.text_notes[1]);

    /* 26b, 26c: in 4:3, the preview at the top of the rows, its label and note at its right; the highlighted row's
     * explanation in the header. */
    static const struct { const char *frame; int w, h, android, speed; } frames[] = { { "26b", 1440, 1080, 0, 0 }, { "26c", 640, 480, 1, 0 },
                                                                                      { "31c", 1440, 1080, 0, 1 }, { "31d", 640, 480, 1, 1 } };
    for (size_t f = 0; f < sizeof frames / sizeof frames[0]; f++) {
        ages(&nav, ORACLES_UI_LAYOUT_4_3, frames[f].w, frames[f].h);
        if (frames[f].android) {
            nav.android = nav.display.fullscreen_only = 1;
            nav.display.window = 1;
            oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
        }
        text_page(&nav, frames[f].speed ? ORACLES_DISPLAY_TEXT_SPEED : ORACLES_DISPLAY_TEXT_BOX);
        if (frames[f].speed) nav.display.text_speed = 1, nav.display.view = 1;
        lay_out(&nav, &t, &l);
        CHECK(!strcmp(t.help, frames[f].speed ? "Instant: each line appears whole." : "Wide: longer lines, fewer boxes."));
        CHECK(frames[f].speed ? !t.help_note : t.help_note && !strcmp(t.help_note, "Savestates taken with Wide need Wide."));
        page_probes(frames[f].frame, &l, 1);
        layout_line(frames[f].frame, "help text", &l.help);
        if (!frames[f].speed) layout_line(frames[f].frame, "help note text", &l.help_note);
    }
    /* 31e 4:3: in Faithful, Text speed alone live, highlighted; on Text, "Enhanced only." on the help's note line. */
    ages(&nav, ORACLES_UI_LAYOUT_4_3, 1440, 1080);
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    nav.display.text_speed = 1;
    text_page(&nav, ORACLES_DISPLAY_TEXT_SPEED);
    lay_out(&nav, &t, &l);
    page_probes("31e 4:3", &l, 1);
    layout_line("31e 4:3", "help text", &l.help);
    nav.row = ORACLES_DISPLAY_LETTERS;
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.help_note, "Enhanced only."));
    page_probes("31e text 4:3", &l, 1);
    layout_line("31e text 4:3", "help text", &l.help);
    layout_line("31e text 4:3", "help note text", &l.help_note);
    /* 26g 4:3: from the pause on Text, its reason on the help's note line. */
    ages(&nav, ORACLES_UI_LAYOUT_4_3, 1440, 1080);
    nav.in_game = 1;
    nav.playing = ORACLES_PROFILE_ENHANCED;
    text_page(&nav, ORACLES_DISPLAY_LETTERS);
    lay_out(&nav, &t, &l);
    page_probes("26g 4:3", &l, 1);
    layout_line("26g 4:3", "help text", &l.help);
    layout_line("26g 4:3", "help note text", &l.help_note);
    layout_line("26g 4:3", "page note", &l.page_note.lines[0]);
}

/* 26e: Display's own rows, the Text link after HUD, highlighted, its settings summed up beside it; in 4:3 its summary
 * on the help's note line (text link 4:3). */
static void link(void)
{
    OraclesHomeNav nav;
    OraclesUiDisplayTexts t;
    OraclesUiDisplayLayout l;
    ages(&nav, ORACLES_UI_LAYOUT_16_9, 1920, 1080);
    nav.display.text = nav.display.text_box = 1;
    nav.row = ORACLES_DISPLAY_TEXT;
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.text_summary, "Compact \xc2\xb7 Wide \xc2\xb7 Normal"));
    layout_text("26e", "Text", &l.labels[ORACLES_DISPLAY_TEXT].lines[0]);
    layout_box("26e", "text row", &l.rows[ORACLES_DISPLAY_TEXT]);
    layout_box("26e", "link", &l.text_link);
    layout_text("26e", "summary text", &l.text_summary);
    layout_near("26e", "summary", "y", l.text_summary.y);
    layout_near("26e", "summary", "h", l.text_summary.h);
    layout_near("26e", "text explanation", "top", l.explanations[8].lines[0].y);
    layout_near("26e", "text explanation", "width", l.explanations[8].w);
    layout_box("26e", "hud row", &l.rows[ORACLES_DISPLAY_HUD]);
    layout_box("26e", "picture row", &l.rows[ORACLES_DISPLAY_PICTURE]);
    layout_box("26e", "panel", &l.panel);
    layout_box("26e", "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);
    /* Its hints: it opens, nothing changes with left and right. */
    OraclesHomeHint hints[ORACLES_HOME_MAX_HINTS];
    CHECK(oracles_home_hints(&nav, hints) == 3 && !strcmp(hints[1].label, "Open"));

    ages(&nav, ORACLES_UI_LAYOUT_4_3, 1440, 1080);
    nav.display.text = nav.display.text_box = 1;
    nav.row = ORACLES_DISPLAY_TEXT;
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.help, "Letters, box, size and speed, with a preview.") && !strcmp(t.help_note, "Compact \xc2\xb7 Wide \xc2\xb7 Normal."));
    layout_line("text link 4:3", "Text", &l.labels[ORACLES_DISPLAY_TEXT].lines[0]);
    layout_box("text link 4:3", "text row", &l.rows[ORACLES_DISPLAY_TEXT]);
    layout_box("text link 4:3", "link", &l.text_link);
    layout_line("text link 4:3", "summary text", &l.text_summary);
    layout_box("text link 4:3", "hud row", &l.rows[ORACLES_DISPLAY_HUD]);
    layout_box("text link 4:3", "picture row", &l.rows[ORACLES_DISPLAY_PICTURE]);
    layout_box("text link 4:3", "Profile row", &l.rows[ORACLES_DISPLAY_PROFILE]);
    layout_box("text link 4:3", "advanced", &l.rows[ORACLES_DISPLAY_ADVANCED]);
    layout_line("text link 4:3", "help text", &l.help);
    layout_line("text link 4:3", "help note text", &l.help_note);
    /* 31h 4:3: Instant chosen, named last. */
    nav.display.text_speed = 1;
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.help_note, "Compact \xc2\xb7 Wide \xc2\xb7 Normal \xc2\xb7 Instant."));
    layout_box("31h 4:3", "link", &l.text_link);
    layout_line("31h 4:3", "summary text", &l.text_summary);
    layout_line("31h 4:3", "help note text", &l.help_note);
    layout_line("31h 4:3", "help text", &l.help);
    /* 31h and 31e: in 16:9, the same; in Faithful the link live, "As the game · Instant". */
    ages(&nav, ORACLES_UI_LAYOUT_16_9, 1920, 1080);
    nav.display.text = nav.display.text_box = 1;
    nav.display.text_speed = 1;
    nav.row = ORACLES_DISPLAY_TEXT;
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.text_summary, "Compact \xc2\xb7 Wide \xc2\xb7 Normal \xc2\xb7 Instant"));
    layout_box("31h", "link", &l.text_link);
    layout_text("31h", "summary text", &l.text_summary);
    layout_box("31h", "text row", &l.rows[ORACLES_DISPLAY_TEXT]);
    layout_box("31h", "panel", &l.panel);
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.text_summary, "As the game \xc2\xb7 Instant") && oracles_display_text_link_applies(&nav));
    layout_box("31e", "link", &l.text_link);
    layout_text("31e", "summary text", &l.text_summary);
    layout_box("31e", "text row", &l.rows[ORACLES_DISPLAY_TEXT]);
    nav.display.text_speed = 0;
    oracles_display_text_summary(&nav, t.text_summary, sizeof t.text_summary);
    CHECK(!strcmp(t.text_summary, "As the game"));

    /* An unrecognised ROM (a randomizer seed), which plays without the instant text: on the home screen too, Text speed
     * dimmed with the pause's reason, Instant left out of the summary, the link and the page inert. */
    nav.display.text_speed = 1;
    nav.games[0].rom = ORACLES_ROM_UNRECOGNISED;
    lay_out(&nav, &t, &l);
    CHECK(!strcmp(t.text_summary, "As the game") && !oracles_display_text_link_applies(&nav));
    CHECK(!oracles_display_text_row_live(&nav, ORACLES_DISPLAY_TEXT_SPEED) && !strcmp(t.text_notes[3], "Needs an original ROM."));
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_TEXT_SPEED, 0) == ORACLES_HOME_STAY && nav.display.text_speed == 1);
    /* A fan game's page by its image: one its patch does not give is refused alike, the one it gives has Instant. */
    nav.games[0].rom = ORACLES_ROM_ORIGINAL;
    nav.games[ORACLES_HOME_GAME_MOONRISE] = nav.games[0];
    nav.games[ORACLES_HOME_GAME_MOONRISE].refuses = FAN_REFUSES;
    nav.games[ORACLES_HOME_GAME_MOONRISE].fan = oracles_home_fan_game(ORACLES_HOME_GAME_MOONRISE);
    nav.games[ORACLES_HOME_GAME_MOONRISE].image = ORACLES_ROM_UNRECOGNISED;
    nav.entry = ORACLES_HOME_FAN;
    nav.fan_pick = ORACLES_HOME_HERO_MOONRISE;
    CHECK(!oracles_display_text_row_live(&nav, ORACLES_DISPLAY_TEXT_SPEED));
    nav.games[ORACLES_HOME_GAME_MOONRISE].image = ORACLES_ROM_ORIGINAL;
    oracles_display_text_summary(&nav, t.text_summary, sizeof t.text_summary);
    CHECK(oracles_display_text_row_live(&nav, ORACLES_DISPLAY_TEXT_SPEED) && !strcmp(t.text_summary, "As the game \xc2\xb7 Instant"));
}

/* The page's keys: up and down wrap round its four rows, left and right change the highlighted one, OK takes its next
 * choice; Back returns to Display on the link, and Advanced's Back on Advanced.  The link opens in Faithful too, and from
 * the pause; --text-box holds its row for the run. */
static void keys(void)
{
    OraclesHomeNav nav;
    ages(&nav, ORACLES_UI_LAYOUT_16_9, 1920, 1080);
    nav.row = ORACLES_DISPLAY_TEXT;
    CHECK(oracles_display_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && !nav.text_page);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.text_page && nav.row == ORACLES_DISPLAY_LETTERS);
    CHECK(oracles_display_row_shown(&nav, ORACLES_DISPLAY_TEXT_SIZE) && !oracles_display_row_shown(&nav, ORACLES_DISPLAY_TEXT)
          && !oracles_display_row_shown(&nav, ORACLES_DISPLAY_CORE));
    CHECK(oracles_display_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_TEXT_SPEED);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.text_speed == 1);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.display.text_speed == 0);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_TEXT_SIZE);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_TEXT_SPEED);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_LETTERS);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.display.text == 1);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_TEXT_BOX);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.text_box == 1);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_DISPLAY_TEXT_SIZE);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.display.text_size == 2);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && nav.display.text_size == 0);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && nav.display.text_size == 1);
    char summary[64];
    oracles_display_text_summary(&nav, summary, sizeof summary);
    CHECK(!strcmp(summary, "Compact \xc2\xb7 Wide \xc2\xb7 Large"));
    CHECK(!strcmp(oracles_display_section(&nav), "Display \xe2\x80\xba Text"));
    CHECK(oracles_display_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && !nav.text_page && nav.row == ORACLES_DISPLAY_TEXT);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_HOME);
    /* Advanced and Text: one page in place of Display's rows at a time, each Back on its own link. */
    nav.screen = ORACLES_SCREEN_DISPLAY;
    nav.row = ORACLES_DISPLAY_ADVANCED;
    CHECK(oracles_display_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.advanced && !nav.text_page);
    CHECK(oracles_display_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && !nav.advanced && nav.row == ORACLES_DISPLAY_ADVANCED);
    /* A click on the link opens the page; one on a choice there sets it. */
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_TEXT, -1) == ORACLES_HOME_STAY && nav.text_page);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_TEXT_SIZE, 2) == ORACLES_HOME_STORE && nav.display.text_size == 2 && nav.row == ORACLES_DISPLAY_TEXT_SIZE);
    CHECK(oracles_display_click(&nav, ORACLES_DISPLAY_CORE, 1) == ORACLES_HOME_STAY && nav.display.core == 0);
    /* In Faithful the link opens on Text speed, the one row that changes there. */
    oracles_display_act(&nav, ORACLES_HOME_BACK);
    nav.display.profile = ORACLES_PROFILE_FAITHFUL;
    CHECK(!oracles_display_text_applies(&nav) && oracles_display_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.text_page
          && nav.row == ORACLES_DISPLAY_TEXT_SPEED);
    nav.display.profile = ORACLES_PROFILE_ENHANCED;
    /* From the pause the page opens on Text size, the one row that changes there. */
    oracles_display_act(&nav, ORACLES_HOME_BACK);
    nav.in_game = 1;
    nav.playing = ORACLES_PROFILE_ENHANCED;
    CHECK(oracles_display_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.text_page && nav.row == ORACLES_DISPLAY_TEXT_SIZE);
    nav.in_game = 0;
    /* --text-box holds its row for the run. */
    nav.display.text_box_fixed = 1;
    nav.row = ORACLES_DISPLAY_TEXT_BOX;
    CHECK(oracles_display_row_fixed(&nav, ORACLES_DISPLAY_TEXT_BOX) && oracles_display_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY);
    /* Reopening Display from the menu shows its own rows. */
    nav.screen = ORACLES_SCREEN_HOME;
    nav.focus = 3;
    oracles_home_act(&nav, ORACLES_HOME_OK);
    CHECK(nav.screen == ORACLES_SCREEN_DISPLAY && !nav.text_page && !nav.advanced);
}

/* The preview's sizes: Very large where twice the box fits across the band, else Large where the box at
 * k + ceil(k / 2) screen pixels a pixel fits the frame shown at k, else Normal, with a note when the choice falls back. */
static void preview_sizes(void)
{
    OraclesHomeNav nav;
    OraclesDisplayTextPreview p;
    ages(&nav, ORACLES_UI_LAYOUT_16_9, 1920, 1080);
    nav.display.text_box = 1;
    nav.display.text_size = 2;
    oracles_display_text_preview(&nav, &p);   /* Far, 480 wide: the wide box twice, 416 */
    CHECK(p.size == 2 && p.scale_num == 2u && p.scale_den == 1u && !p.note[0] && strstr(p.label, ", very large (\xc3\x97" "2)"));
    nav.display.view = 1;   /* Medium, 384 wide: twice the wide box does not fit across, Large does */
    oracles_display_text_preview(&nav, &p);
    CHECK(p.size == 1 && !strcmp(p.note, "Large in this view: no room for Very large.")
          && !strcmp(oracles_display_text_note(&nav, ORACLES_DISPLAY_TEXT_SIZE), p.note));
    nav.display.view = 0;   /* Near at 3x, 768 screen pixels across: the wide box at 5 a pixel does not fit either */
    oracles_display_text_preview(&nav, &p);
    CHECK(p.size == 0 && !strcmp(p.note, "Normal in this view: no room for Very large."));
    nav.display.text_size = 1;
    nav.display.view = 2;
    oracles_display_text_preview(&nav, &p);   /* Far at 3x: the box at 5 screen pixels a pixel, five thirds */
    CHECK(p.size == 1 && p.scale_num == 5u && p.scale_den == 3u && strstr(p.label, ", large (\xc3\x97" "1.67)"));
    nav.display.text_size = 0;
    oracles_display_text_preview(&nav, &p);
    CHECK(p.size == 0 && p.scale_num == 1u && !p.note[0] && !oracles_display_text_note(&nav, ORACLES_DISPLAY_TEXT_SIZE)[0]);
    CHECK(p.box_w == 208 && p.text_w == 192 && p.compact == 0);
    /* Large's factor is the presenter's: oracles_text_box_enlarge's for the frame shown at 3x, the box at its bottom. */
    nav.display.text_size = 1;
    oracles_display_text_preview(&nav, &p);
    int32_t rect[4];
    unsigned factor = 0;
    CHECK(oracles_text_box_enlarge(0.0, 0.0, 480.0 * 3.0, 270.0 * 3.0, 3.0, (480u - 208u) / 2u, 270u - 8u - 40u, 208u, 40u, ORACLES_TEXT_BOX_ENLARGE_LARGE, 1, rect, &factor)
          && factor == p.scale_num && p.scale_den == 3u);
    /* On a 640x480 handheld, Medium in 4:3 (320x240) with the wide box: Large at 2x fullscreen, but the screen margin's
     * 48 lines leave the frame at 1x, where the box at 2 a pixel does not fit: Normal, as the game shows it. */
    ages(&nav, ORACLES_UI_LAYOUT_4_3, 640, 480);
    nav.android = nav.display.fullscreen_only = 1;
    nav.display.window = 3;
    nav.display.view = 1;
    nav.display.text_box = 1;
    nav.display.text_size = 1;
    oracles_display_text_preview(&nav, &p);
    CHECK(p.size == 1 && p.scale_num == 3u && p.scale_den == 2u);
    nav.display.margin = 2;
    nav.display.margin_lines[2] = 48;
    oracles_display_text_preview(&nav, &p);
    CHECK(p.size == 0 && !strcmp(p.note, "Normal in this view: no room for Large."));
    nav.display.margin_lines[2] = 0;   /* a margin the screen does not apply */
    oracles_display_text_preview(&nav, &p);
    CHECK(p.size == 1);
}

/* The preview on the output's pixels: where the box's pixel comes to one output pixel or more, the band fills its
 * frame and keeps its size whatever the text size, the box growing with it, at 1920x1080 (16:9, the scene at 1) Far at
 * 13/12, 65/36 Large and 13/6 Very large, and at 1280x720 (the scene at 2/3) Far Large and Very large; under one, at
 * 640x480 (4:3, the scene at 4/9) and at 1280x720 Far Normal, a pixel of the box a screen pixel, the band's bottom
 * around the box, the standard box centred, the wide one from the frame's left at 640x480. */
static int whole(float v) { return v == floorf(v); }

static void preview_at(const OraclesHomeNav *nav, float scale, OraclesUiDisplayLayout *l, OraclesUiTextPreviewPixels *px)
{
    OraclesUiDisplayTexts t;
    lay_out(nav, &t, l);
    const OraclesUiBox *f = &l->preview_frame;
    const OraclesUiBox frame = { f->x * scale, f->y * scale, f->w * scale, f->h * scale };
    oracles_ui_text_preview_pixels(&t.preview, frame, l->pixel * scale, px);
    const OraclesUiBox *const boxes[4] = { &px->clip, &px->view, &px->hud, &px->box };
    for (int i = 0; i < 4; i++) CHECK(whole(boxes[i]->x) && whole(boxes[i]->y) && whole(boxes[i]->w) && whole(boxes[i]->h));
    CHECK(px->pixel == fmaxf(1.0f, l->pixel * scale));
    CHECK(fabsf(px->box.w - (float)t.preview.box_w * px->pixel) <= 1.0f && fabsf(px->box.h - 40.0f * px->pixel) <= 1.0f);
    if (px->pixel > 1.0f)
        CHECK(!px->letterbox && !memcmp(&px->view, &px->clip, sizeof px->view) && px->box.x >= px->clip.x && px->box.y >= px->clip.y
              && px->box.x + px->box.w <= px->clip.x + px->clip.w && px->box.y + px->box.h <= px->clip.y + px->clip.h
              && fabsf(px->box.x + px->box.w * 0.5f - (px->clip.x + px->clip.w * 0.5f)) <= 1.0f);
}

/* Normal, Large and Very large in turn at `scale`: the band the same, the box's pixel never smaller. */
static void preview_sizes_at(OraclesHomeNav *nav, float scale, OraclesUiTextPreviewPixels out[3])
{
    OraclesUiDisplayLayout l;
    for (int size = 0; size < 3; size++) {
        nav->display.text_size = size;
        preview_at(nav, scale, &l, &out[size]);
        if (size) CHECK(out[size].pixel >= out[size - 1].pixel && (out[size - 1].pixel == 1.0f || !memcmp(&out[size].view, &out[size - 1].view, sizeof out[size].view)));
    }
    nav->display.text_size = 0;
}

static void preview_pixels(void)
{
    OraclesHomeNav nav;
    OraclesUiDisplayLayout l;
    OraclesUiTextPreviewPixels px, sizes[3];
    ages(&nav, ORACLES_UI_LAYOUT_4_3, 640, 480);
    nav.android = nav.display.fullscreen_only = 1;
    nav.display.view = 1;
    text_page(&nav, ORACLES_DISPLAY_LETTERS);
    nav.display.text_box = 0;
    preview_at(&nav, 4.0f / 9.0f, &l, &px);
    CHECK(px.pixel == 1.0f && !px.letterbox && px.view.w == 320.0f && px.view.y + px.view.h == px.clip.y + px.clip.h);
    CHECK(fabsf(px.box.x + px.box.w * 0.5f - (px.clip.x + px.clip.w * 0.5f)) <= 1.0f && px.box.x + 8.0f >= px.clip.x
          && px.box.x + 8.0f + 128.0f <= px.clip.x + px.clip.w);
    nav.display.text_box = 1;
    preview_at(&nav, 4.0f / 9.0f, &l, &px);
    CHECK(px.pixel == 1.0f && px.box.x == px.clip.x && px.box.w == 208.0f);
    nav.display.window = 3;
    nav.display.text_size = 1;   /* Large, three halves: the box's pixel a screen pixel still, the band's two thirds */
    preview_at(&nav, 4.0f / 9.0f, &l, &px);
    CHECK(px.pixel == 1.0f && px.box.h == 40.0f && px.view.w == 213.0f && px.view.h == 160.0f && px.view.y + px.view.h == px.clip.y + px.clip.h);
    preview_sizes_at(&nav, 4.0f / 9.0f, sizes);
    CHECK(sizes[0].pixel == 1.0f && sizes[1].pixel == 1.0f && sizes[2].pixel == 1.0f);
    ages(&nav, ORACLES_UI_LAYOUT_16_9, 1920, 1080);
    text_page(&nav, ORACLES_DISPLAY_LETTERS);
    nav.display.text_box = 0;
    preview_at(&nav, 1.0f, &l, &px);   /* Far, 480x270 filling 520x293 */
    CHECK(px.view.w == 520.0f && px.view.h == 293.0f && px.box.w == 156.0f && px.box.h == 43.0f && px.box.y + px.box.h <= px.clip.y + px.clip.h - 8.0f);
    preview_sizes_at(&nav, 1.0f, sizes);
    CHECK(sizes[0].pixel < sizes[1].pixel && sizes[1].pixel < sizes[2].pixel && fabsf(sizes[2].pixel - 2.0f * sizes[0].pixel) < 1e-4f
          && sizes[2].box.w == 312.0f);
    nav.display.view = 0;
    preview_at(&nav, 1.0f, &l, &px);
    CHECK(px.pixel > 2.0f);
    nav.display.view = 1;
    nav.display.text_size = 1;
    preview_at(&nav, 1.0f, &l, &px);   /* Medium Large, five thirds of 520/384 */
    CHECK(fabsf(px.pixel - 520.0f / 384.0f * 5.0f / 3.0f) < 1e-3f);
    nav.display.text_box = 1;
    preview_at(&nav, 2.0f / 3.0f, &l, &px);   /* at 1280x720, the wide box whole in the band */
    CHECK(px.pixel > 1.0f);
    nav.display.view = 2;
    nav.display.text_box = 0;
    preview_sizes_at(&nav, 2.0f / 3.0f, sizes);   /* Far at 1280x720: Normal under a screen pixel, Large and Very large over */
    CHECK(sizes[0].pixel == 1.0f && sizes[0].view.w > sizes[0].clip.w && sizes[0].view.y + sizes[0].view.h == sizes[0].clip.y + sizes[0].clip.h
          && sizes[1].pixel > 1.0f && sizes[2].pixel > sizes[1].pixel && !memcmp(&sizes[1].view, &sizes[2].view, sizeof sizes[1].view));
}

/* The box's picture: its ground, its text's ink from (8, 4), the arrow 3 pixels from its right, 2 from its bottom. */
static void preview_picture(void)
{
    OraclesHomeNav nav;
    OraclesDisplayTextPreview p;
    static uint32_t out[ORACLES_UI_PREVIEW_PICTURE_W * ORACLES_UI_PREVIEW_PICTURE_H];
    const uint32_t colors[3] = { 1u, 2u, 3u };
    ages(&nav, ORACLES_UI_LAYOUT_16_9, 1920, 1080);
    for (int wide = 0; wide < 2; wide++) {
        nav.display.text_box = wide;
        oracles_display_text_preview(&nav, &p);
        const int w = oracles_ui_text_preview_picture(&p, colors, out);
        CHECK(w == p.box_w && w == (wide ? 208 : 144));
        unsigned ink = 0, sample = 0;
        for (int y = 0; y < 40; y++)
            for (int x = 0; x < w; x++) {
                const uint32_t c = out[y * w + x];
                const int in_text = x >= 8 && x < 8 + p.text_w && y >= 4 && y < 36, arrow = y >= 35 && y < 38 && x >= w - 9 + (y - 35) && x < w - 3 - (y - 35);
                CHECK(c == (arrow ? 3u : in_text && p.sample[y - 4][x - 8] ? 2u : 1u));
                ink += c == 2u;
                sample += in_text && p.sample[y - 4][x - 8] && !arrow;
            }
        CHECK(ink && ink == sample);
    }
}

/* The sample: without a font, its words as bars; Compact fits more of them than Classic in the two lines; in a font, each
 * letter's ink at its place. */
static void sample(void)
{
    static uint8_t out[ORACLES_DISPLAY_SAMPLE_H][ORACLES_DISPLAY_SAMPLE_W];
    unsigned ink[2][2] = { { 0 } };
    for (int compact = 0; compact < 2; compact++) {
        oracles_display_text_sample(NULL, compact, 128u, out);
        for (unsigned row = 0; row < ORACLES_DISPLAY_SAMPLE_H; row++)
            for (unsigned col = 0; col < ORACLES_DISPLAY_SAMPLE_W; col++) {
                CHECK(!out[row][col] || col < 128u);
                ink[compact][row / 16u] += out[row][col];
            }
    }
    CHECK(ink[0][0] && ink[0][1] && ink[1][0] + ink[1][1] > ink[0][0] + ink[0][1]);
    /* A font whose every character is a full cell: Classic's first letter, L, fills its cell's 16 rows. */
    static OraclesHomeFont font;
    memset(&font, 0, sizeof font);
    font.read = 1;
    for (int c = 0; c < ORACLES_HOME_FONT_CHARACTERS; c++) {
        memset(font.rows[c], c == 0 ? 0 : 0xff, 16);
        font.width[c] = c == 0 ? 0 : 8;
        font.advance[c] = c == 0 ? 5 : 8;
    }
    oracles_display_text_sample(&font, 0, 128u, out);
    unsigned first = 0;
    for (unsigned row = 0; row < 16u; row++) for (unsigned col = 0; col < 8u; col++) first += out[row][col];
    CHECK(first == 128u);
}

int main(int argc, char **argv)
{
    if (argc < 2 || !layout_reference_load(argv[1])) { fprintf(stderr, "usage: oracles-test-launcher-text-page launcher_layout_reference.json\n"); return 2; }
    if (!oracles_ui_fonts_load()) { fprintf(stderr, "FAIL the embedded fonts do not load\n"); return 1; }
    layouts();
    link();
    keys();
    preview_sizes();
    preview_pixels();
    preview_picture();
    sample();
    if (layout_failures()) {
        fprintf(stderr, "%d failures\n", layout_failures());
        return 1;
    }
    printf("launcher text page: ok\n");
    return 0;
}
