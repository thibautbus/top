/* RetroAchievements' screens (ui_achievements.h): their layout against the layout reference,
 * tests/launcher_layout_reference.json (layout_reference.h), the test's argument, and their keys.  The page, off and not
 * logged in (ra off), on and logged in with two unlocks waiting (ra on), at 640x480 with a controller (ra 640x480); the
 * login with a keyboard (login), after a wrong password (login wrong), while it logs in (login logging), under Android's
 * keyboard (login android), with the launcher's keyboard at 640x480 (login keyboard 640x480); the toasts over a game
 * (toast unlock, toast 640x480, toast stack); the pause's list (achievements, achievements paused, achievements
 * 640x480).  In a build with them, About's link (about ra, about ra 4:3), the pause of eight entries (pause achievements
 * 640x480) and Cheats' note (cheats achievements, cheats achievements 4:3) too.  And the keys: the page's, the login's
 * (its paste, its keys and its password shown included), the launcher's keyboard's, the toasts' queue and the list's
 * scroll.  The page's fourth row, Unlock sound, which no frame of the reference has: after Privacy in the same style, the
 * rest of the page as the frames have it, above the hints at 640x480 too; and the unlock's sound itself
 * (ui_achievements_sound.h), its length, peak and ends, and its mixing. */
#include "layout_reference.h"
#include "ui_achievements.h"
#include "ui_achievements_sound.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); layout_fail(); } } while (0)

/* The page opened from About, its row `row` highlighted: on and logged in as SamLink, `pending` unlocks waiting. */
static void page(OraclesHomeNav *nav, OraclesUiLayout layout, int on, unsigned row, unsigned pending)
{
    oracles_home_init(nav);
    nav->layout = layout;
    nav->achievements.shown = 1;
    nav->achievements.on = on;
    nav->achievements.sound = 1;
    if (on) snprintf(nav->achievements.user, sizeof nav->achievements.user, "SamLink");
    nav->achievements.pending = pending;
    oracles_achievements_open(nav);
    nav->row = row;
}

/* What the fourth row, Unlock sound, adds under Privacy: the frames have three rows. */
static float sound_row_extent(const OraclesUiCheatsLayout *l)
{
    return l->rows[ORACLES_ACHIEVEMENTS_ROW_SOUND].y + l->rows[ORACLES_ACHIEVEMENTS_ROW_SOUND].h
         - (l->rows[ORACLES_ACHIEVEMENTS_ROW_PRIVACY].y + l->rows[ORACLES_ACHIEVEMENTS_ROW_PRIVACY].h);
}

/* The three rows, the probes every frame of the page has; the panel (4:3: the help under the rows) longer by the fourth. */
static void rows(const char *frame, const OraclesUiCheatsLayout *l)
{
    const float extent = sound_row_extent(l);
    CHECK(extent > 0.0f);
    if (l->panel.w > 0.0f) {
        layout_near(frame, "panel", "x", l->panel.x);
        layout_near(frame, "panel", "y", l->panel.y);
        layout_near(frame, "panel", "w", l->panel.w);
        layout_near(frame, "panel", "h", l->panel.h - extent);
    } else {   /* 4:3: no panel, the rows and the help the mockup's */
        layout_near(frame, "panel", "y", l->rows[0].y);
        layout_near(frame, "panel", "h", l->help_note.y + l->help_note.h - l->rows[0].y - extent);
    }
    layout_near(frame, "on", "y", l->rows[0].y);
    layout_near(frame, "account", "y", l->rows[1].y);
    layout_near(frame, "privacy", "y", l->rows[2].y);
    layout_box(frame, "on off", &l->choices[0][0].box);
    layout_box(frame, "on on", &l->choices[0][1].box);
    layout_line(frame, "on off text", &l->choices[0][0].name);
    layout_line(frame, "on on text", &l->choices[0][1].name);
    layout_line(frame, "section", &l->head.section);
    const OraclesUiBox guide = l->action_boxes[ORACLES_ACHIEVEMENTS_ROW_PRIVACY];
    layout_box(frame, "guide", &guide);
    layout_line(frame, "guide text", &l->actions[ORACLES_ACHIEVEMENTS_ROW_PRIVACY]);
}

static void page_16_9(const char *frame, int on, unsigned pending)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_UI_LAYOUT_16_9, on, ORACLES_ACHIEVEMENTS_ROW_ACCOUNT, pending);
    OraclesUiCheatsLayout l;
    oracles_ui_layout_achievements(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    rows(frame, &l);
    layout_line(frame, "over", &l.head.over);
    layout_line(frame, "title", &l.head.title.lines[0]);
    layout_line(frame, "note text", &l.note.lines[0]);
    layout_near(frame, "note text", "lines", (float)l.note.count);
    layout_box(frame, "on row", &l.rows[0]);
    layout_box(frame, "account row", &l.rows[1]);
    layout_box(frame, "privacy row", &l.rows[2]);
    layout_line(frame, "on explanation", &l.explanations[0].lines[0]);
    layout_near(frame, "on explanation", "lines", (float)l.explanations[0].count);
    layout_line(frame, "account explanation", &l.explanations[1].lines[0]);
    layout_line(frame, "privacy explanation", &l.explanations[2].lines[0]);
    layout_near(frame, "privacy explanation", "lines", (float)l.explanations[2].count);
    layout_box(frame, on ? "logout" : "login", &l.action_boxes[1]);
    layout_line(frame, on ? "logout text" : "login text", &l.actions[1]);
    if (pending) {
        layout_near(frame, "account status", "y", l.status.lines[0].y);
        layout_line(frame, "account status text", &l.status.lines[0]);
        /* The account's row and its status, as the mockup's grid holds them. */
        layout_near(frame, "account", "h", l.status.lines[0].y + l.status.h - l.rows[1].y);
    }
}

/* The help bar's hints; RetroAchievements' own (the login's, the list's) only where the home screen dispatches to them, in a
 * build with them. */
static void hints(const char *frame, const OraclesHomeNav *nav, OraclesUiLayout layout, unsigned expected)
{
#ifndef ORACLES_RETROACHIEVEMENTS
    OraclesHomeHint own[ORACLES_HOME_MAX_HINTS];
    if (oracles_achievements_hints(nav, own)) return;
#endif
    OraclesHomeHint h[ORACLES_HOME_MAX_HINTS];
    OraclesUiHintLayout hl[ORACLES_HOME_MAX_HINTS];
    const unsigned count = oracles_home_hints(nav, h);
    CHECK(count == expected);
    if (count != expected) return;
    oracles_ui_layout_hints(layout, h, count, hl);
    for (unsigned i = 0; i < count; i++) {
        char name[32];
        snprintf(name, sizeof name, "hint %u", i); layout_box(frame, name, &hl[i].box);
        snprintf(name, sizeof name, "hint %u key", i); layout_box(frame, name, &hl[i].key_box);
        snprintf(name, sizeof name, "hint %u key text", i); layout_line(frame, name, &hl[i].key);
        snprintf(name, sizeof name, "hint %u label", i); layout_line(frame, name, &hl[i].label);
    }
}

static void page_4_3(void)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_UI_LAYOUT_4_3, 1, ORACLES_ACHIEVEMENTS_ROW_ACCOUNT, 2);
    nav.android = 1;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    OraclesUiCheatsLayout l;
    oracles_ui_layout_achievements(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    const char *frame = "ra 640x480";
    rows(frame, &l);
    layout_line(frame, "title", &l.head.title.lines[0]);
    layout_box(frame, "on", &l.rows[0]);
    layout_box(frame, "account", &l.rows[1]);
    layout_box(frame, "privacy", &l.rows[2]);
    layout_line(frame, "account value text", &l.values[1]);
    layout_box(frame, "logout", &l.action_boxes[1]);
    layout_line(frame, "logout text", &l.actions[1]);
    layout_near(frame, "account status", "y", l.status.lines[0].y);
    layout_line(frame, "account status text", &l.status.lines[0]);
    /* The help under the rows, lower by the fourth row. */
    const float extent = sound_row_extent(&l);
    OraclesUiLine help = l.help, help_note = l.help_note;
    help.y -= extent; help.baseline -= extent;
    help_note.y -= extent; help_note.baseline -= extent;
    layout_line(frame, "help text", &help);
    layout_line(frame, "help note text", &help_note);
    CHECK(!strcmp(l.help_text, "One account for every game."));
    hints(frame, &nav, ORACLES_UI_LAYOUT_4_3, 4);
}

/* The fourth row, Unlock sound, in each layout: Privacy's style, its choices On then Off in the column of RetroAchievements'
 * own, its explanation (16:9) in two lines at most; the page above the help bar's hints, at 640x480 too. */
static void sound_row(OraclesUiLayout layout, int on)
{
    OraclesHomeNav nav;
    page(&nav, layout, on, ORACLES_ACHIEVEMENTS_ROW_SOUND, on ? 2u : 0u);
    if (layout == ORACLES_UI_LAYOUT_4_3) {
        nav.android = 1;
        oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    }
    OraclesUiCheatsLayout l;
    oracles_ui_layout_achievements(layout, &nav, &l);
    const OraclesUiBox *privacy = &l.rows[ORACLES_ACHIEVEMENTS_ROW_PRIVACY], *sound = &l.rows[ORACLES_ACHIEVEMENTS_ROW_SOUND];
    const OraclesUiOptionLayout *choices = l.choices[ORACLES_ACHIEVEMENTS_ROW_SOUND], *first = l.choices[ORACLES_ACHIEVEMENTS_ROW_ON];
    CHECK(!strcmp(oracles_achievements_row_label(ORACLES_ACHIEVEMENTS_ROW_SOUND), "Unlock sound"));
    CHECK(sound->x == privacy->x && sound->w == privacy->w && sound->y > privacy->y + privacy->h);
    CHECK(l.labels[ORACLES_ACHIEVEMENTS_ROW_SOUND].x == l.labels[ORACLES_ACHIEVEMENTS_ROW_PRIVACY].x);
    CHECK(fabsf(l.labels[ORACLES_ACHIEVEMENTS_ROW_SOUND].h - l.labels[ORACLES_ACHIEVEMENTS_ROW_PRIVACY].h) < 0.01f);
    CHECK(choices[0].name.w > 0.0f && choices[1].name.w > 0.0f && choices[0].box.x + choices[0].box.w < choices[1].box.x);
    CHECK(choices[0].box.y >= sound->y && choices[0].box.y + choices[0].box.h <= sound->y + sound->h);
    CHECK(fabsf(choices[0].box.h - first[0].box.h) < 0.01f);
    /* 16:9: the explanation beside them; 4:3: the help under the rows tells the row highlighted. */
    if (layout == ORACLES_UI_LAYOUT_16_9) {
        CHECK(fabsf(choices[1].box.x + choices[1].box.w - (first[1].box.x + first[1].box.w)) < 0.01f);
        CHECK(l.explanations[ORACLES_ACHIEVEMENTS_ROW_SOUND].count >= 1 && l.explanations[ORACLES_ACHIEVEMENTS_ROW_SOUND].count <= 2);
        CHECK(l.explanations[ORACLES_ACHIEVEMENTS_ROW_SOUND].lines[0].x == l.explanations[ORACLES_ACHIEVEMENTS_ROW_PRIVACY].lines[0].x);
        CHECK(l.explanations[ORACLES_ACHIEVEMENTS_ROW_SOUND].lines[0].x + l.explanations[ORACLES_ACHIEVEMENTS_ROW_SOUND].w < choices[0].box.x);
    } else {
        CHECK(fabsf(choices[0].box.x - first[0].box.x) < 0.01f);
        CHECK(!strcmp(l.help_text, "On: a short sound with each unlock's toast.") && !strcmp(l.help_note_text, "Off: the toast alone."));
        CHECK(l.help.y > sound->y + sound->h);
    }
    /* Above the hints, as the scene puts them (1920x1080, 4:3's 1440x1080: 640x480 scaled). */
    OraclesHomeHint h[ORACLES_HOME_MAX_HINTS];
    OraclesUiHintLayout hl[ORACLES_HOME_MAX_HINTS];
    const unsigned count = oracles_home_hints(&nav, h);
    CHECK(count > 0);
    oracles_ui_layout_hints(layout, h, count, hl);
    const float bottom = layout == ORACLES_UI_LAYOUT_16_9 ? l.panel.y + l.panel.h : l.help_note.y + l.help_note.h;
    for (unsigned i = 0; i < count; i++) CHECK(bottom < hl[i].box.y);
    printf("launcher achievements: Unlock sound (%s, %s): row at y %.1f, h %.1f; page down to %.1f, hints from %.1f\n",
           layout == ORACLES_UI_LAYOUT_16_9 ? "16:9" : "4:3", on ? "on" : "off", (double)sound->y, (double)sound->h, (double)bottom,
           (double)(count ? hl[0].box.y : 0.0f));
}

/* Unlock sound's keys: Up and Down through the four rows and round, Left, Right and OK turn it, a click on On or Off sets
 * it; each change a command to store, none for the choice it has; not while the login is open. */
static void sound_keys(void)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_UI_LAYOUT_16_9, 1, ORACLES_ACHIEVEMENTS_ROW_PRIVACY, 0);
    CHECK(nav.achievements.sound);
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_ACHIEVEMENTS_ROW_SOUND);
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_ACHIEVEMENTS_ROW_ON);
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.row == ORACLES_ACHIEVEMENTS_ROW_SOUND);
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STORE && !nav.achievements.sound);
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && nav.achievements.sound);
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STORE && !nav.achievements.sound);
    CHECK(nav.achievements.on && !nav.achievements.login);   /* RetroAchievements' own On untouched */
    nav.row = ORACLES_ACHIEVEMENTS_ROW_ON;
    CHECK(oracles_achievements_click(&nav, ORACLES_ACHIEVEMENTS_ROW_SOUND, 0) == ORACLES_HOME_STORE && nav.achievements.sound && nav.row == ORACLES_ACHIEVEMENTS_ROW_SOUND);
    CHECK(oracles_achievements_click(&nav, ORACLES_ACHIEVEMENTS_ROW_SOUND, 0) == ORACLES_HOME_STAY && nav.achievements.sound);
    CHECK(oracles_achievements_click(&nav, ORACLES_ACHIEVEMENTS_ROW_SOUND, 1) == ORACLES_HOME_STORE && !nav.achievements.sound);
    CHECK(oracles_achievements_click(&nav, ORACLES_ACHIEVEMENTS_ROW_SOUND, 2) == ORACLES_HOME_STAY && !nav.achievements.sound);
    CHECK(oracles_achievements_click(&nav, ORACLES_ACHIEVEMENTS_ROW_SOUND, -1) == ORACLES_HOME_STAY && !nav.achievements.sound);
    const char *help, *note;
    oracles_achievements_help(&nav, &help, &note);
    CHECK(!strcmp(help, "On: a short sound with each unlock's toast."));
    /* Off with no account: the login opened by On takes the keys. */
    page(&nav, ORACLES_UI_LAYOUT_16_9, 0, ORACLES_ACHIEVEMENTS_ROW_ON, 0);
    oracles_achievements_act(&nav, ORACLES_HOME_OK);
    CHECK(nav.achievements.login);
    CHECK(oracles_achievements_click(&nav, ORACLES_ACHIEVEMENTS_ROW_SOUND, 1) == ORACLES_HOME_STAY && nav.achievements.sound);
}

/* ---- the unlock's sound ----------------------------------------------------------------------------------- */

static float unlock[48000];

/* At the launcher's 48 kHz: about half a second, its last frame silent; its peak at -12 dBFS, far from clipping; 0 at
 * both ends; too small a buffer renders nothing. */
static void sound_rendered(void)
{
    const size_t frames = oracles_sound_frames(&oracles_unlock_sound, 48000u);
    CHECK(frames > 24000u && frames < 25500u && frames <= sizeof unlock / sizeof unlock[0]);
    CHECK(oracles_sound_render(&oracles_unlock_sound, 48000u, unlock, frames - 1u) == 0);
    CHECK(oracles_sound_render(&oracles_unlock_sound, 0u, unlock, frames) == 0);
    CHECK(oracles_sound_render(&oracles_unlock_sound, 48000u, unlock, frames) == frames);
    float peak = 0.0f;
    for (size_t n = 0; n < frames; n++) if (fabsf(unlock[n]) > peak) peak = fabsf(unlock[n]);
    const double dbfs = 20.0 * log10((double)peak);
    CHECK(dbfs > -12.01 && dbfs < -11.99);
    CHECK(peak * 32767.0f < 32767.0f * 0.26f);
    CHECK(unlock[0] == 0.0f && unlock[frames - 1u] == 0.0f);
    /* No click: the rise from silence over its first frames, the fall nearly silent over its last millisecond. */
    float start = 0.0f, end = 0.0f;
    for (size_t n = 0; n < 5u; n++) start = fmaxf(start, fabsf(unlock[n]));
    for (size_t n = 0; n < 48u; n++) end = fmaxf(end, fabsf(unlock[frames - 1u - n]));
    CHECK(start * 32767.0f < 64.0f && end * 32767.0f < 16.0f);
    printf("launcher achievements: unlock sound, %zu frames (%.3f s) at 48 kHz, peak %.2f dBFS\n", frames, (double)frames / 48000.0, dbfs);
}

/* Mixed chunk by chunk of 800 stereo frames, as the game's: the cursor moves on across them, the sound whole and the
 * same in both channels; past its end, or with no sound playing, a chunk is left as it is; on a loud chunk it saturates. */
static void sound_mixed(void)
{
    const size_t frames = oracles_sound_frames(&oracles_unlock_sound, 48000u);
    oracles_sound_render(&oracles_unlock_sound, 48000u, unlock, frames);
    static int16_t chunk[800 * 2], copy[800 * 2];
    size_t cursor = 0, total = 0, chunks = 0, mixed;
    int same = 1, exact = 1;
    do {
        memset(chunk, 0, sizeof chunk);
        const size_t at = cursor;
        mixed = oracles_sound_mix_s16(unlock, frames, &cursor, chunk, 800u, 2u);
        for (size_t i = 0; i < mixed; i++) {
            same &= chunk[2 * i] == chunk[2 * i + 1];
            exact &= chunk[2 * i] == (int16_t)lrintf(unlock[at + i] * 32767.0f);
        }
        for (size_t i = mixed; i < 800u; i++) same &= chunk[2 * i] == 0 && chunk[2 * i + 1] == 0;
        total += mixed;
        chunks++;
        CHECK(cursor == total);
    } while (mixed == 800u);
    CHECK(same && exact);
    CHECK(total == frames && cursor == frames && chunks == (frames + 799u) / 800u);
    /* Over: the next chunk untouched. */
    for (size_t i = 0; i < 1600u; i++) chunk[i] = copy[i] = (int16_t)(i * 37u);
    CHECK(oracles_sound_mix_s16(unlock, frames, &cursor, chunk, 800u, 2u) == 0 && cursor == frames && !memcmp(chunk, copy, sizeof chunk));
    /* Loud: saturating at either end, never wrapping round. */
    cursor = 0;
    int high = 0, low = 0, wrapped = 0;
    while (cursor < frames) {
        for (size_t i = 0; i < 800u; i++) { chunk[2 * i] = 32000; chunk[2 * i + 1] = -32000; }
        const size_t at = cursor;
        mixed = oracles_sound_mix_s16(unlock, frames, &cursor, chunk, 800u, 2u);
        for (size_t i = 0; i < mixed; i++) {
            const long add = lrintf(unlock[at + i] * 32767.0f);
            high += chunk[2 * i] == 32767;
            low += chunk[2 * i + 1] == -32768;
            wrapped += add >= 0 ? chunk[2 * i] < 32000 || chunk[2 * i + 1] < -32000 : chunk[2 * i] > 32000 || chunk[2 * i + 1] > -32000;
        }
    }
    CHECK(high > 0 && low > 0 && !wrapped);
}

/* The queue emptied by the pause: a sound all still queued starts again from its start, one partly heard stops, no sound
 * playing stays so. */
static void sound_cleared(void)
{
    const size_t frames = oracles_sound_frames(&oracles_unlock_sound, 48000u);
    CHECK(oracles_sound_cursor_cleared(0u, frames, 3360u) == 0u);
    CHECK(oracles_sound_cursor_cleared(800u, frames, 3360u) == 0u);
    CHECK(oracles_sound_cursor_cleared(3360u, frames, 3360u) == 0u);
    CHECK(oracles_sound_cursor_cleared(3361u, frames, 3360u) == frames);
    CHECK(oracles_sound_cursor_cleared(9600u, frames, 3360u) == frames);
    CHECK(oracles_sound_cursor_cleared(800u, frames, 0u) == frames);
    CHECK(oracles_sound_cursor_cleared(frames, frames, 3360u) == frames);
    CHECK(oracles_sound_cursor_cleared(frames, frames, frames * 2u) == frames);
    CHECK(oracles_sound_cursor_cleared(0u, 0u, 3360u) == 0u);
    /* Restarted, it mixes whole again. */
    oracles_sound_render(&oracles_unlock_sound, 48000u, unlock, frames);
    static int16_t chunk[800 * 2];
    size_t cursor = oracles_sound_cursor_cleared(1600u, frames, 3360u), total = 0, mixed;
    do {
        memset(chunk, 0, sizeof chunk);
        total += mixed = oracles_sound_mix_s16(unlock, frames, &cursor, chunk, 800u, 2u);
    } while (mixed == 800u);
    CHECK(total == frames);
}

/* ---- the login ---------------------------------------------------------------------------------------------- */

static void login(OraclesHomeNav *nav, OraclesUiLayout layout, const char *name, const char *password, int field, int state)
{
    page(nav, layout, 0, ORACLES_ACHIEVEMENTS_ROW_ACCOUNT, 0);
    oracles_achievements_login_open(nav, 0);
    oracles_achievements_login_text(nav, name);
    nav->achievements.field = ORACLES_LOGIN_PASSWORD;
    oracles_achievements_login_text(nav, password);
    nav->achievements.field = field;
    nav->achievements.login = state;
}

static void login_fields(const char *frame, const OraclesUiLoginLayout *l)
{
    layout_box(frame, "dialog", &l->dialog);
    layout_line(frame, "title", &l->title);
    layout_line(frame, "user label", &l->fields[0].label);
    layout_line(frame, "password label", &l->fields[1].label);
    layout_box(frame, "user", &l->fields[0].box);
    layout_box(frame, "password", &l->fields[1].box);
    layout_box(frame, "state", &l->state);
    layout_line(frame, "user value", &l->fields[0].value);
}

static void login_16_9(const char *frame, int state, int field)
{
    OraclesHomeNav nav;
    login(&nav, ORACLES_UI_LAYOUT_16_9, "SamLink", "hunter7", field, state);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    OraclesUiLoginLayout l;
    oracles_ui_layout_login(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    login_fields(frame, &l);
    layout_box(frame, "note", &(OraclesUiBox){ l.note.x, l.note.y, l.state.w, l.note.h });
    layout_line(frame, "note text", &l.note);
    layout_box(frame, "ok", &l.buttons[0]);
    layout_box(frame, "cancel", &l.buttons[1]);
    layout_line(frame, "ok text", &l.button_names[0]);
    layout_line(frame, "cancel text", &l.button_names[1]);
    if (state == ORACLES_LOGIN_TYPING) {
        layout_box(frame, "password value", &(OraclesUiBox){ l.fields[1].value.x, l.fields[1].value.y, l.fields[1].value.w, l.fields[1].value.h });
        layout_box(frame, "caret", &l.fields[1].caret);
        hints(frame, &nav, ORACLES_UI_LAYOUT_16_9, 3);
    } else {
        layout_line(frame, "state text", &l.state_text);
        if (state == ORACLES_LOGIN_LOGGING) CHECK(l.fields[0].caret.w == 0.0f && l.fields[1].caret.w == 0.0f);
    }
}

static void login_android(void)
{
    OraclesHomeNav nav;
    login(&nav, ORACLES_UI_LAYOUT_16_9, "SamLi", "", ORACLES_LOGIN_NAME, ORACLES_LOGIN_TYPING);
    nav.android = 1;
    nav.achievements.system_keyboard = 1;
    OraclesUiLoginLayout l;
    oracles_ui_layout_login(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    const char *frame = "login android";
    layout_box(frame, "dialog", &l.dialog);
    layout_line(frame, "title", &l.title);
    layout_box(frame, "user", &l.fields[0].box);
    layout_box(frame, "password", &l.fields[1].box);
    layout_box(frame, "state", &l.state);
    layout_line(frame, "user value", &l.fields[0].value);
    layout_box(frame, "caret", &l.fields[0].caret);
    layout_box(frame, "ok", &l.buttons[0]);
    layout_box(frame, "cancel", &l.buttons[1]);
}

static void login_keyboard(void)
{
    OraclesHomeNav nav;
    login(&nav, ORACLES_UI_LAYOUT_4_3, "SamLink", "hunt", ORACLES_LOGIN_PASSWORD, ORACLES_LOGIN_TYPING);
    nav.android = 1;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    nav.achievements.key_row = 2;
    nav.achievements.key_column = 6;
    OraclesUiLoginLayout l;
    oracles_ui_layout_login(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    const char *frame = "login keyboard 640x480";
    login_fields(frame, &l);
    CHECK(l.note.h == 0.0f && l.buttons[0].w == 0.0f);   /* under the keyboard while it is up */
    layout_box(frame, "password value", &(OraclesUiBox){ l.fields[1].value.x, l.fields[1].value.y, l.fields[1].value.w, l.fields[1].value.h });
    layout_box(frame, "caret", &l.fields[1].caret);
    layout_box(frame, "keyboard", &l.keyboard);
    static const unsigned keys[][3] = { { 0, 0, 0 }, { 9, 0, 9 }, { 10, 1, 0 }, { 26, 2, 6 }, { 39, 3, 9 }, { 40, 4, 0 }, { 41, 4, 1 }, { 42, 4, 2 },
                                        { 43, 4, 3 }, { 44, 4, 4 } };
    for (unsigned i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        char name[16];
        snprintf(name, sizeof name, "key %u", keys[i][0]);
        layout_box(frame, name, &l.keys[keys[i][1]][keys[i][2]]);
    }
    CHECK(l.key_count[3] == 10 && l.key_count[4] == 5 && !strcmp(l.key_texts[2][6], "j") && !strcmp(l.key_texts[4][1], "#+="));
    layout_line(frame, "key 1 text", &l.key_names[0][0]);
    layout_line(frame, "key j text", &l.key_names[2][6]);
    layout_line(frame, "key shift text", &l.key_names[4][0]);
    layout_line(frame, "key symbols text", &l.key_names[4][1]);
    layout_line(frame, "key space text", &l.key_names[4][2]);
    layout_line(frame, "key done text", &l.key_names[4][4]);
    hints(frame, &nav, ORACLES_UI_LAYOUT_4_3, 5);
}

/* ---- the toasts --------------------------------------------------------------------------------------------- */

static void toasts_layout(void)
{
    OraclesAchievementsToasts q;
    memset(&q, 0, sizeof q);
    oracles_achievements_toast_unlock(&q, "Ten Hearts", 10, "12345");
    OraclesUiAchievementToastLayout l;
    oracles_ui_layout_achievement_toast(ORACLES_UI_LAYOUT_16_9, &q.queue[0], 1, 0.0f, 64.0f, 0, &l);
    layout_box("toast unlock", "toast", &l.box);
    layout_box("toast unlock", "badge", &l.badge);
    layout_line("toast unlock", "over", &l.over);
    layout_line("toast unlock", "text", &l.text);
    oracles_ui_layout_achievement_toast(ORACLES_UI_LAYOUT_4_3, &q.queue[0], 0, 180.0f, 162.0f, 0, &l);
    layout_box("toast 640x480", "toast", &l.box);
    layout_line("toast 640x480", "over", &l.over);
    layout_line("toast 640x480", "text", &l.text);
    /* The game's start, offline, a cheat, a mod: each toast in turn at the same place, as the stack shows them one under
     * the other. */
    memset(&q, 0, sizeof q);
    oracles_achievements_toast_start(&q, "Oracle of Ages", 12, 55, ORACLES_ACHIEVEMENTS_RUNNING);
    oracles_achievements_toast_offline(&q);
    oracles_achievements_toast_cheat(&q);
    oracles_achievements_toast_mod(&q);
    static const char *const names[4] = { "start", "offline", "suspended", "mod" };
    float y = 64.0f;
    for (unsigned i = 0; i < 4; i++) {
        char name[32];
        oracles_ui_layout_achievement_toast(ORACLES_UI_LAYOUT_16_9, &q.queue[i], 0, 0.0f, y, 0, &l);
        layout_box("toast stack", names[i], &l.box);
        snprintf(name, sizeof name, "%s text", names[i]);
        layout_line("toast stack", name, &l.text);
        if (!i) layout_line("toast stack", "start over", &l.over);
        y = l.box.y + l.box.h - 16.0f + 12.0f;   /* the next under it, 12 apart */
    }
    CHECK(!strcmp(q.queue[0].text, "Oracle of Ages \xc2\xb7 12 of 55 achievements \xc2\xb7 Casual"));
    /* A completed game, a cheat on at the start, a mod's. */
    memset(&q, 0, sizeof q);
    oracles_achievements_toast_start(&q, "Oracle of Ages", 55, 55, ORACLES_ACHIEVEMENTS_RUNNING);
    oracles_achievements_toast_start(&q, "Oracle of Ages", 3, 55, ORACLES_ACHIEVEMENTS_BY_CHEAT);
    oracles_achievements_toast_start(&q, "Oracle of Ages", 3, 55, ORACLES_ACHIEVEMENTS_BY_MOD);
    CHECK(!strcmp(q.queue[0].text, "Oracle of Ages \xc2\xb7 all 55 achievements \xc2\xb7 Completed \xc2\xb7 Casual"));
    CHECK(!strcmp(q.queue[1].text, "Oracle of Ages \xc2\xb7 Achievements suspended: a cheat is on"));
    CHECK(!strcmp(q.queue[2].text, oracles_achievements_mod_reason));
    /* The reminder over the game shows: the toast under it. */
    oracles_ui_layout_achievement_toast(ORACLES_UI_LAYOUT_16_9, &q.queue[0], 0, 0.0f, 64.0f, 1, &l);
    CHECK(l.box.y == 64.0f + 16.0f + 72.0f);
}

/* The toast shown at `until`, the frames drawn every 16 ms from `*clock`. */
static OraclesAchievementsToast *frames_until(OraclesAchievementsToasts *q, double *clock, double until, float *opacity)
{
    for (; *clock < until; *clock += 16.0) oracles_achievements_toast_now(q, *clock, opacity);
    *clock = until;
    return oracles_achievements_toast_now(q, until, opacity);
}

/* One toast at a time, 4 s then a fade of 0.6 s, the next after it; past eight, the oldest waiting goes.  Their time runs
 * while they are drawn: the pause or a notice over them stops it.  A toast's badge, as its first frame found it. */
static void toasts_queue(void)
{
    OraclesAchievementsToasts q;
    memset(&q, 0, sizeof q);
    float opacity;
    double clock = 1000.0;
    CHECK(!oracles_achievements_toast_now(&q, 1000.0, &opacity) && opacity == 0.0f);
    oracles_achievements_toast_unlock(&q, "First", 5, "");
    oracles_achievements_toast_unlock(&q, "Second", 1, "");
    const OraclesAchievementsToast *t = oracles_achievements_toast_now(&q, 1000.0, &opacity);
    CHECK(t && !strcmp(t->text, "First") && opacity == 1.0f && !strcmp(t->over, "Achievement unlocked \xc2\xb7 5 points"));
    t = frames_until(&q, &clock, 4900.0, &opacity);
    CHECK(t && !strcmp(t->text, "First") && opacity == 1.0f);
    t = frames_until(&q, &clock, 5300.0, &opacity);
    CHECK(t && !strcmp(t->text, "First") && opacity > 0.0f && opacity < 1.0f);
    t = frames_until(&q, &clock, 5700.0, &opacity);
    CHECK(t && !strcmp(t->text, "Second") && opacity == 1.0f && !strcmp(t->over, "Achievement unlocked \xc2\xb7 1 point"));
    CHECK(!frames_until(&q, &clock, 5700.0 + 4700.0, &opacity));
    clock = 20000.0;
    for (unsigned i = 0; i < 10; i++) {
        char title[16];
        snprintf(title, sizeof title, "T%u", i);
        oracles_achievements_toast_unlock(&q, title, i, "");
        if (!i) oracles_achievements_toast_now(&q, 20000.0, &opacity);   /* T0 shown */
    }
    CHECK(q.count == ORACLES_ACHIEVEMENTS_TOASTS);
    t = oracles_achievements_toast_now(&q, 20000.0, &opacity);
    CHECK(t && !strcmp(t->text, "T0"));
    t = frames_until(&q, &clock, 24700.0, &opacity);
    CHECK(t && !strcmp(t->text, "T3"));   /* T1 and T2, the oldest waiting, went for T8 and T9 */
    /* T3 shown for 2 s, then a minute of pause: shown 2 s more, as long as it was before it. */
    t = frames_until(&q, &clock, 26700.0, &opacity);
    CHECK(t && !strcmp(t->text, "T3"));
    clock += 60000.0;
    t = frames_until(&q, &clock, clock + 1900.0, &opacity);
    CHECK(t && !strcmp(t->text, "T3") && opacity == 1.0f);
    t = frames_until(&q, &clock, clock + 700.0, &opacity);
    CHECK(t && !strcmp(t->text, "T4"));
    /* Its badge as the first frame finds it: drawn then without, drawn after with; never laid out again. */
    OraclesAchievementsToast toast;
    memset(&toast, 0, sizeof toast);
    snprintf(toast.badge, sizeof toast.badge, "00001");
    CHECK(!oracles_achievements_toast_badge(&toast, 0) && !oracles_achievements_toast_badge(&toast, 1));
    memset(&toast, 0, sizeof toast);
    snprintf(toast.badge, sizeof toast.badge, "00001");
    CHECK(oracles_achievements_toast_badge(&toast, 1) && oracles_achievements_toast_badge(&toast, 0));
    memset(&toast, 0, sizeof toast);
    CHECK(!oracles_achievements_toast_badge(&toast, 1));
    /* An unlock's sound, due once at its toast's first frame; a toast of the game's start has none.  A second unlock's
     * waits for its own toast; the pause holding the toasts holds it too. */
    memset(&q, 0, sizeof q);
    clock = 1000.0;
    oracles_achievements_toast_start(&q, "Oracle of Ages", 1, 55, 0);
    oracles_achievements_toast_unlock(&q, "First", 5, "");
    oracles_achievements_toast_unlock(&q, "Second", 5, "");
    OraclesAchievementsToast *shown = oracles_achievements_toast_now(&q, clock, &opacity);
    CHECK(shown && !oracles_achievements_toast_sound(shown));
    shown = frames_until(&q, &clock, 5700.0, &opacity);
    CHECK(shown && !strcmp(shown->text, "First") && oracles_achievements_toast_sound(shown) && !oracles_achievements_toast_sound(shown));
    shown = frames_until(&q, &clock, 10000.0, &opacity);
    CHECK(shown && !strcmp(shown->text, "First") && !oracles_achievements_toast_sound(shown));
    clock += 60000.0;   /* the pause */
    shown = frames_until(&q, &clock, clock + 400.0, &opacity);
    CHECK(shown && !strcmp(shown->text, "Second") && oracles_achievements_toast_sound(shown));
    CHECK(!oracles_achievements_toast_sound(NULL));
}

/* ---- the list ------------------------------------------------------------------------------------------------- */

static OraclesAchievementsList list;

static void stand_ins(void)
{
    static const struct { const char *t, *d; unsigned p; const char *on, *progress; float percent; } a[8] = {
        { "Out of the Woods", "Leave the first forest.", 5, "21 Sep 2026", "", 0.0f },
        { "Clean Sweep", "Open every chest of the first dungeon.", 10, "22 Sep 2026", "", 0.0f },
        { "Ten Hearts", "Hold ten heart containers.", 10, "2 Oct 2026", "", 0.0f },
        { "Seed Gatherer", "Carry all five kinds of seeds.", 10, NULL, "3/5", 0.6f },
        { "Rupee Hoarder", "Hold 500 rupees at once.", 10, NULL, "312/500", 0.624f },
        { "Deep Water", "Reach the bottom of the sea.", 25, NULL, "", 0.0f },
        { "No Potion Needed", "Defeat a dungeon guardian without drinking a potion.", 25, NULL, "", 0.0f },
        { "Night Walker", "Cross the whole map between two saves.", 25, NULL, "", 0.0f },
    };
    memset(&list, 0, sizeof list);
    list.count = 55;
    list.unlocked = 12;
    list.points = 514;
    list.points_unlocked = 120;
    for (unsigned i = 0; i < list.count; i++) {
        OraclesAchievementsItem *item = &list.items[i];
        const unsigned k = i < 8 ? i : 7;
        snprintf(item->title, sizeof item->title, "%s", a[k].t);
        snprintf(item->description, sizeof item->description, "%s", a[k].d);
        item->points = a[k].p;
        item->unlocked = a[k].on != NULL;
        if (a[k].on) snprintf(item->date, sizeof item->date, "%s", a[k].on);
        snprintf(item->progress, sizeof item->progress, "%s", a[k].progress);
        item->percent = a[k].percent;
    }
}

static void list_nav(OraclesHomeNav *nav, OraclesUiLayout layout, unsigned focus, int suspended)
{
    oracles_home_init(nav);
    oracles_home_pause(nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, 0);
    nav->layout = layout;
    nav->achievements.shown = nav->achievements.active = 1;
    nav->achievements.list = &list;
    nav->achievements.suspended = suspended;
    oracles_achievements_list_open(nav);
    nav->achievements.list_focus = focus;
}

static void rows_of(const char *frame, const OraclesUiAchievementsLayout *l)
{
    layout_box(frame, "panel", &l->panel);
    for (unsigned i = 0; i < 6; i++) {
        char name[16];
        snprintf(name, sizeof name, "item %u", i); layout_box(frame, name, &l->rows[i].row);
        snprintf(name, sizeof name, "badge %u", i); layout_box(frame, name, &l->rows[i].badge);
    }
    layout_line(frame, "title 0", &l->rows[0].title);
    layout_line(frame, "desc 0", &l->rows[0].description);
    layout_line(frame, "points 0", &l->rows[0].points);
    layout_line(frame, "title 3", &l->rows[3].title);
    layout_line(frame, "points 3", &l->rows[3].points);
    layout_line(frame, "summary line", &l->summary_text);
}

static void list_16_9(void)
{
    OraclesHomeNav nav;
    list_nav(&nav, ORACLES_UI_LAYOUT_16_9, 3, ORACLES_ACHIEVEMENTS_RUNNING);
    OraclesUiAchievementsLayout l;
    oracles_ui_layout_achievements_list(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    const char *frame = "achievements";
    rows_of(frame, &l);
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    CHECK(l.shown >= 8 && l.rows[0].index == 0 && !l.warning);
    layout_box(frame, "summary", &l.summary);
    layout_line(frame, "section", &l.head.section);
    layout_near(frame, "mode", "y", l.mode.lines[0].y);
    layout_near(frame, "mode", "h", l.mode.h);
    layout_line(frame, "date 0", &l.rows[0].date);
    layout_line(frame, "progress 3 text", &l.rows[3].progress);
    layout_box(frame, "progress 3", &(OraclesUiBox){ l.rows[3].bar.x, l.rows[3].progress.y, l.rows[3].progress.x + l.rows[3].progress.w - l.rows[3].bar.x,
                                                       l.rows[3].progress.h });
    hints(frame, &nav, ORACLES_UI_LAYOUT_16_9, 2);
    /* Paused by a mod: the summary says so, in the warning's tone. */
    list_nav(&nav, ORACLES_UI_LAYOUT_16_9, 3, ORACLES_ACHIEVEMENTS_BY_MOD);
    oracles_ui_layout_achievements_list(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    layout_line("achievements paused", "summary line", &l.summary_text);
    CHECK(l.warning);
}

static void list_4_3(void)
{
    OraclesHomeNav nav;
    list_nav(&nav, ORACLES_UI_LAYOUT_4_3, 0, ORACLES_ACHIEVEMENTS_BY_CHEAT);
    nav.android = 1;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    OraclesUiAchievementsLayout l;
    oracles_ui_layout_achievements_list(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    const char *frame = "achievements 640x480";
    rows_of(frame, &l);
    layout_line(frame, "title", &l.head.title.lines[0]);
    layout_box(frame, "summary", &l.summary);
    layout_line(frame, "state 0", &l.rows[0].date);
    layout_line(frame, "state 3", &l.rows[3].progress);
    layout_line(frame, "state 5", &l.rows[5].date);
    CHECK(!strcmp(l.rows[5].date_text, "Locked") && l.warning);
    hints(frame, &nav, ORACLES_UI_LAYOUT_4_3, 2);
}

/* Up and down move the highlight, the list scrolling past its sixth row (4:3: its fourth); Back returns to the pause on
 * Achievements. */
static void list_keys(void)
{
    OraclesHomeNav nav;
    list_nav(&nav, ORACLES_UI_LAYOUT_16_9, 0, ORACLES_ACHIEVEMENTS_RUNNING);
    CHECK(oracles_achievements_list_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.achievements.list_focus == 0);
    for (unsigned i = 0; i < 7; i++) oracles_achievements_list_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.achievements.list_focus == 7 && oracles_achievements_list_first(&nav) == 1);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    CHECK(oracles_achievements_list_first(&nav) == 3);
    for (unsigned i = 0; i < 100; i++) oracles_achievements_list_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.achievements.list_focus == 54);
    /* The end: the last row at the bottom, no empty row under it, the thumb at the bottom of its track. */
    for (unsigned layout = 0; layout < 2; layout++) {
        nav.layout = layout ? ORACLES_UI_LAYOUT_4_3 : ORACLES_UI_LAYOUT_16_9;
        OraclesUiAchievementsLayout l;
        oracles_ui_layout_achievements_list(nav.layout, &nav, &l);
        const OraclesUiAchievementRow *last = &l.rows[l.shown - 1u];
        const float bottom = l.list.y + l.list.h, under = bottom - (last->row.y + last->row.h);
        CHECK(l.first == oracles_achievements_list_first(&nav) && last->index == 54 && under >= 0.0f && under < last->row.h);
        CHECK(l.thumb.y >= l.track.y && fabsf(l.thumb.y + l.thumb.h - (l.track.y + l.track.h)) < 0.5f);
        nav.achievements.list_focus = 0;
        oracles_ui_layout_achievements_list(nav.layout, &nav, &l);
        CHECK(l.first == 0 && fabsf(l.thumb.y - l.track.y) < 0.5f && l.thumb.h < l.track.h);
        nav.achievements.list_focus = 54;
    }
    nav.layout = ORACLES_UI_LAYOUT_16_9;
#ifdef ORACLES_RETROACHIEVEMENTS
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_PAUSE);
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    const unsigned count = oracles_home_items(&nav, items);
    CHECK(count == 8 && items[nav.focus].id == ORACLES_HOME_ITEM_ACHIEVEMENTS && items[7].id == ORACLES_HOME_ITEM_QUIT_GAME);
    /* The reduced pause keeps it: it is the session's. */
    nav.narrow = 1;
    CHECK(oracles_home_items(&nav, items) == 5 && items[3].id == ORACLES_HOME_ITEM_ACHIEVEMENTS);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY);
    nav.focus = 3;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_ACHIEVEMENTS_LIST && nav.achievements.list_focus == 0);
    /* Without them for the session, no entry. */
    nav.screen = ORACLES_SCREEN_PAUSE;
    nav.achievements.active = 0;
    CHECK(oracles_home_items(&nav, items) == 4);
#endif
}

/* ---- the page's keys and the login's ------------------------------------------------------------------------- */

static void page_keys(void)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_UI_LAYOUT_16_9, 0, ORACLES_ACHIEVEMENTS_ROW_ON, 0);
    /* On with no account opens the login, On while it is open; Cancel leaves it Off. */
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.achievements.login == ORACLES_LOGIN_TYPING && nav.achievements.on);
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_ACHIEVEMENTS_ROW_ON);   /* the login's */
    CHECK(oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_ESCAPE) == ORACLES_HOME_STAY && !nav.achievements.login && !nav.achievements.on);
    /* The login answered: in, On, the account's name as the server spells it; a command to store. */
    oracles_achievements_act(&nav, ORACLES_HOME_OK);
    oracles_achievements_login_text(&nav, "samlink");
    CHECK(oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_ENTER) == ORACLES_HOME_STAY && nav.achievements.field == ORACLES_LOGIN_PASSWORD);
    CHECK(oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_ENTER) == ORACLES_HOME_STAY);   /* no password yet */
    oracles_achievements_login_text(&nav, "pw");
    CHECK(oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_ENTER) == ORACLES_HOME_ACHIEVEMENTS_LOGIN && nav.achievements.login == ORACLES_LOGIN_LOGGING);
    CHECK(oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_TAB) == ORACLES_HOME_STAY && nav.achievements.login == ORACLES_LOGIN_LOGGING);
    oracles_achievements_login_answered(&nav, 1, "SamLink");
    CHECK(!nav.achievements.login && nav.achievements.on && !strcmp(nav.achievements.user, "SamLink") && !nav.achievements.password[0]);
    CHECK(nav.row == ORACLES_ACHIEVEMENTS_ROW_ACCOUNT);
    char summary[96];
    oracles_achievements_summary(&nav, summary, sizeof summary);
    CHECK(!strcmp(summary, "On \xc2\xb7 SamLink"));
    /* Log out: a command; Off and On store; Player's guide opens. */
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_ACHIEVEMENTS_LOGOUT);
    nav.row = ORACLES_ACHIEVEMENTS_ROW_ON;
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && !nav.achievements.on);
    CHECK(oracles_achievements_click(&nav, ORACLES_ACHIEVEMENTS_ROW_ON, 1) == ORACLES_HOME_STORE && nav.achievements.on);
    CHECK(oracles_achievements_click(&nav, ORACLES_ACHIEVEMENTS_ROW_PRIVACY, 2) == ORACLES_HOME_ACHIEVEMENTS_GUIDE && nav.row == ORACLES_ACHIEVEMENTS_ROW_PRIVACY);
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.row == ORACLES_ACHIEVEMENTS_ROW_ACCOUNT);
    nav.achievements.user[0] = 0;
    oracles_achievements_summary(&nav, summary, sizeof summary);
    CHECK(!strcmp(summary, "On \xc2\xb7 not logged in"));
    /* Logged out with On: Log in… opens the login, whose Cancel keeps On. */
    CHECK(oracles_achievements_click(&nav, ORACLES_ACHIEVEMENTS_ROW_ACCOUNT, 2) == ORACLES_HOME_STAY && nav.achievements.login);
    oracles_achievements_login_close(&nav);
    CHECK(nav.achievements.on);
    /* The server's refusal of the token kept, under Account. */
    nav.achievements.refused = 1;
    char status[96];
    OraclesTone dot, tone;
    CHECK(oracles_achievements_account_status(&nav, status, sizeof status, &dot, &tone) && tone == ORACLES_TONE_WARN
          && !strcmp(status, "retroachievements.org refused the saved login: log in again."));
    /* Back to About, on its last row. */
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_ABOUT && nav.row == oracles_about_rows(&nav) - 1u);
}

/* The clipboard's text pasted: its blanks around dropped, then all or nothing; into the field highlighted, at its end,
 * the copy wiped; the password still masked. */
static void login_paste(void)
{
    char field[16] = "ab";
    CHECK(oracles_login_paste_text(field, sizeof field, "  hunter7\r\n") == ORACLES_LOGIN_PASTE_DONE && !strcmp(field, "abhunter7"));
    CHECK(oracles_login_paste_text(field, sizeof field, "\t\n \r") == ORACLES_LOGIN_PASTE_EMPTY && !strcmp(field, "abhunter7"));
    CHECK(oracles_login_paste_text(field, sizeof field, "") == ORACLES_LOGIN_PASTE_EMPTY && !strcmp(field, "abhunter7"));
    field[0] = 0;
    CHECK(oracles_login_paste_text(field, sizeof field, "a b\xc3\xa9!") == ORACLES_LOGIN_PASTE_DONE && !strcmp(field, "a b\xc3\xa9!"));   /* inner spaces kept */
    /* A control character or a byte that starts no character inside it: refused, the field as it was. */
    static const char *const refused[] = { "two\nlines", "tab\there", "del\x7f", "c1\xc2\x85", "stray\x80", "cut\xc3", "over\xc0\xaf",
                                          "over\xe0\x80\xaf", "surrogate\xed\xa0\x80", "past\xf4\x90\x80\x80", "bell\x07" };
    for (unsigned i = 0; i < sizeof refused / sizeof refused[0]; i++) {
        snprintf(field, sizeof field, "keep");
        CHECK(oracles_login_paste_text(field, sizeof field, refused[i]) == ORACLES_LOGIN_PASTE_REFUSED && !strcmp(field, "keep"));
    }
    /* What the field holds, 15 bytes here: up to it, past it refused whole (a password cut short is a wrong one). */
    snprintf(field, sizeof field, "abcde");
    CHECK(oracles_login_paste_text(field, sizeof field, "fghijklmno\n") == ORACLES_LOGIN_PASTE_DONE && !strcmp(field, "abcdefghijklmno"));
    snprintf(field, sizeof field, "abcde");
    CHECK(oracles_login_paste_text(field, sizeof field, "fghijklmnop") == ORACLES_LOGIN_PASTE_TOO_LONG && !strcmp(field, "abcde"));
    snprintf(field, sizeof field, "abcdefghijklmn");
    CHECK(oracles_login_paste_text(field, sizeof field, "\xc3\xa9") == ORACLES_LOGIN_PASTE_TOO_LONG && !strcmp(field, "abcdefghijklmn"));
    CHECK(oracles_login_paste_text(field, sizeof field, NULL) == ORACLES_LOGIN_PASTE_EMPTY && oracles_login_paste_text(NULL, 4, "x") == ORACLES_LOGIN_PASTE_EMPTY);
    /* A web page's or a document's blanks around it dropped too: no-break space, byte order mark, zero-width space. */
    field[0] = 0;
    CHECK(oracles_login_paste_text(field, sizeof field, "\xef\xbb\xbf\xe2\x80\x8b pw\xc2\xa0x\xc2\xa0\xe2\x80\x8b\r\n") == ORACLES_LOGIN_PASTE_DONE
          && !strcmp(field, "pw\xc2\xa0x"));   /* inside, kept */
    field[0] = 0;
    CHECK(oracles_login_paste_text(field, sizeof field, "\xc2\xa0\xef\xbb\xbf\xe2\x80\x8b") == ORACLES_LOGIN_PASTE_EMPTY && !field[0]);
    /* A line or paragraph separator, anywhere: refused. */
    static const char *const separators[] = { "one\xe2\x80\xa8two", "one\xe2\x80\xa9two", "pw\xe2\x80\xa8", "\xe2\x80\xa9pw" };
    for (unsigned i = 0; i < sizeof separators / sizeof separators[0]; i++)
        CHECK(oracles_login_paste_text(field, sizeof field, separators[i]) == ORACLES_LOGIN_PASTE_REFUSED && !field[0]);
    /* The real fields: 127 bytes fit, 128 do not. */
    char most[130];
    memset(most, 'p', 127);
    most[127] = '\n';
    most[128] = 0;
    OraclesHomeNav full;
    page(&full, ORACLES_UI_LAYOUT_16_9, 0, ORACLES_ACHIEVEMENTS_ROW_ACCOUNT, 0);
    oracles_achievements_login_open(&full, 0);
    full.achievements.field = ORACLES_LOGIN_PASSWORD;
    CHECK(oracles_achievements_login_paste(&full, most) == ORACLES_LOGIN_PASTE_DONE && strlen(full.achievements.password) == 127u);
    oracles_achievements_login_open(&full, 0);
    full.achievements.field = ORACLES_LOGIN_PASSWORD;
    memset(most, 'p', 128);
    most[128] = 0;
    CHECK(oracles_achievements_login_paste(&full, most) == ORACLES_LOGIN_PASTE_TOO_LONG && !full.achievements.password[0] && !most[0]);

    OraclesHomeNav nav;
    page(&nav, ORACLES_UI_LAYOUT_16_9, 0, ORACLES_ACHIEVEMENTS_ROW_ACCOUNT, 0);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    char clipboard[64] = { 0 };
    snprintf(clipboard, sizeof clipboard, "SamLink");
    CHECK(oracles_achievements_login_paste(&nav, clipboard) == ORACLES_LOGIN_PASTE_EMPTY && !clipboard[0]);   /* no login open: nothing, the copy wiped */
    oracles_achievements_login_open(&nav, 0);
    snprintf(clipboard, sizeof clipboard, " SamLink\n");
    CHECK(oracles_achievements_login_paste(&nav, clipboard) == ORACLES_LOGIN_PASTE_DONE && !strcmp(nav.achievements.name, "SamLink"));
    for (unsigned i = 0; i < sizeof clipboard; i++) CHECK(!clipboard[i]);
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_TAB);
    oracles_achievements_login_text(&nav, "pw");
    snprintf(clipboard, sizeof clipboard, "-hunter7\r\n");
    CHECK(oracles_achievements_login_paste(&nav, clipboard) == ORACLES_LOGIN_PASTE_DONE && !strcmp(nav.achievements.password, "pw-hunter7") && !clipboard[0]);
    OraclesUiLoginLayout l;
    oracles_ui_layout_login(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(!strcmp(l.masked, "\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2"));
    /* Refused or too long: the field untouched, the copy wiped all the same. */
    snprintf(clipboard, sizeof clipboard, "line one\nline two");
    CHECK(oracles_achievements_login_paste(&nav, clipboard) == ORACLES_LOGIN_PASTE_REFUSED && !strcmp(nav.achievements.password, "pw-hunter7") && !clipboard[0]);
    char longer[200];
    memset(longer, 'x', sizeof longer - 1u);
    longer[sizeof longer - 1u] = 0;
    CHECK(oracles_achievements_login_paste(&nav, longer) == ORACLES_LOGIN_PASTE_TOO_LONG && !strcmp(nav.achievements.password, "pw-hunter7") && !longer[0]);
    /* A paste clears a wrong password's error, as typing does; while it logs in, nothing. */
    nav.achievements.login = ORACLES_LOGIN_WRONG;
    snprintf(clipboard, sizeof clipboard, "!");
    CHECK(oracles_achievements_login_paste(&nav, clipboard) == ORACLES_LOGIN_PASTE_DONE && nav.achievements.login == ORACLES_LOGIN_TYPING);
    nav.achievements.login = ORACLES_LOGIN_LOGGING;
    snprintf(clipboard, sizeof clipboard, "more");
    CHECK(oracles_achievements_login_paste(&nav, clipboard) == ORACLES_LOGIN_PASTE_EMPTY && !strcmp(nav.achievements.password, "pw-hunter7!") && !clipboard[0]);
    /* On Log in or Cancel no field takes it. */
    nav.achievements.login = ORACLES_LOGIN_TYPING;
    nav.achievements.field = ORACLES_LOGIN_OK;
    snprintf(clipboard, sizeof clipboard, "more");
    CHECK(oracles_achievements_login_paste(&nav, clipboard) == ORACLES_LOGIN_PASTE_EMPTY && !strcmp(nav.achievements.password, "pw-hunter7!"));
}

/* The password shown in letters: hidden when the login opens, toggled while it is open, hidden again when it closes
 * (Cancel, a successful login) and when the application goes to the background; never outside the login. */
static void password_shown(void)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_UI_LAYOUT_16_9, 0, ORACLES_ACHIEVEMENTS_ROW_ACCOUNT, 0);
    oracles_achievements_password_toggle(&nav);
    CHECK(!oracles_achievements_password_shown(&nav) && !nav.achievements.password_shown);   /* no login: nothing to show */
    oracles_achievements_login_open(&nav, 0);
    CHECK(!oracles_achievements_password_shown(&nav));
    oracles_achievements_password_toggle(&nav);
    CHECK(oracles_achievements_password_shown(&nav));
    oracles_achievements_password_toggle(&nav);
    CHECK(!oracles_achievements_password_shown(&nav));
    oracles_achievements_password_toggle(&nav);
    oracles_achievements_password_hide(&nav);   /* the background */
    CHECK(!oracles_achievements_password_shown(&nav));
    /* The window going away hides it; anything else leaves it. */
    static const OraclesLoginWindow away[4] = { ORACLES_LOGIN_WINDOW_BACKGROUND, ORACLES_LOGIN_WINDOW_FOCUS_LOST, ORACLES_LOGIN_WINDOW_MINIMIZED,
                                                ORACLES_LOGIN_WINDOW_HIDDEN };
    for (unsigned i = 0; i < 4; i++) {
        oracles_achievements_password_toggle(&nav);
        oracles_achievements_window_changed(&nav, ORACLES_LOGIN_WINDOW_OTHER);
        CHECK(oracles_achievements_password_shown(&nav));
        oracles_achievements_window_changed(&nav, away[i]);
        CHECK(!oracles_achievements_password_shown(&nav));
    }
    /* Opened again over a login left shown (no close between): hidden. */
    oracles_achievements_password_toggle(&nav);
    oracles_achievements_login_open(&nav, 0);
    CHECK(!oracles_achievements_password_shown(&nav) && !nav.achievements.password_shown);
    oracles_achievements_password_toggle(&nav);
    oracles_achievements_login_close(&nav);
    CHECK(!oracles_achievements_password_shown(&nav) && !nav.achievements.password_shown);
    oracles_achievements_login_open(&nav, 0);
    CHECK(!oracles_achievements_password_shown(&nav));   /* opened again: hidden */
    /* A successful login closes it, hidden; a wrong one keeps it as it was. */
    oracles_achievements_login_text(&nav, "SamLink");
    nav.achievements.field = ORACLES_LOGIN_PASSWORD;
    oracles_achievements_login_text(&nav, "pw");
    oracles_achievements_password_toggle(&nav);
    CHECK(oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_ENTER) == ORACLES_HOME_ACHIEVEMENTS_LOGIN);
    oracles_achievements_login_answered(&nav, 0, NULL);
    CHECK(oracles_achievements_password_shown(&nav));
    nav.achievements.field = ORACLES_LOGIN_OK;
    CHECK(oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_ENTER) == ORACLES_HOME_ACHIEVEMENTS_LOGIN);
    oracles_achievements_login_answered(&nav, 1, "SamLink");
    CHECK(!nav.achievements.login && !oracles_achievements_password_shown(&nav) && !nav.achievements.password_shown);
    /* Another screen: never shown there. */
    oracles_achievements_login_open(&nav, 0);
    oracles_achievements_password_toggle(&nav);
    nav.screen = ORACLES_SCREEN_ACHIEVEMENTS_LIST;
    CHECK(!oracles_achievements_password_shown(&nav));
}

/* The system's text input: one character typed as it is, more (a system keyboard's paste, a word it commits) by the
 * paste's rules; Android's Enter and Tab made up for that text's line breaks dropped while it waits. */
static void login_typed(void)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_UI_LAYOUT_16_9, 0, ORACLES_ACHIEVEMENTS_ROW_ACCOUNT, 0);
    oracles_achievements_login_open(&nav, 0);
    CHECK(oracles_achievements_login_typed(&nav, " ") == ORACLES_LOGIN_PASTE_DONE && !strcmp(nav.achievements.name, " "));   /* a space typed stays */
    CHECK(oracles_achievements_login_typed(&nav, "\xc3\xa9") == ORACLES_LOGIN_PASTE_DONE && !strcmp(nav.achievements.name, " \xc3\xa9"));
    oracles_achievements_login_open(&nav, 0);
    CHECK(oracles_achievements_login_typed(&nav, "SamLink\n") == ORACLES_LOGIN_PASTE_DONE && !strcmp(nav.achievements.name, "SamLink"));
    CHECK(oracles_achievements_login_typed(&nav, "a\nb") == ORACLES_LOGIN_PASTE_REFUSED && !strcmp(nav.achievements.name, "SamLink"));
    char longer[140];
    memset(longer, 'x', sizeof longer - 1u);
    longer[sizeof longer - 1u] = 0;
    CHECK(oracles_achievements_login_typed(&nav, longer) == ORACLES_LOGIN_PASTE_TOO_LONG && !strcmp(nav.achievements.name, "SamLink"));   /* not cut */
    CHECK(oracles_achievements_login_typed(&nav, NULL) == ORACLES_LOGIN_PASTE_DONE && !strcmp(nav.achievements.name, "SamLink"));
    nav.achievements.field = ORACLES_LOGIN_OK;
    CHECK(oracles_achievements_login_typed(&nav, "pw") == ORACLES_LOGIN_PASTE_EMPTY);
    /* The made-up keys: Enter and Tab with no keyboard and no raw code while text waits; a real key, or no text, kept. */
    CHECK(oracles_login_made_up_key(ORACLES_LOGIN_RAW_RETURN, 0u, 0u, 1));
    CHECK(oracles_login_made_up_key(ORACLES_LOGIN_RAW_TAB, 0u, 0u, 1));
    CHECK(!oracles_login_made_up_key(ORACLES_LOGIN_RAW_RETURN, 0u, 0u, 0));
    CHECK(!oracles_login_made_up_key(ORACLES_LOGIN_RAW_RETURN, 1u, 66u, 1));
    CHECK(!oracles_login_made_up_key(ORACLES_LOGIN_RAW_RETURN, 1u, 0u, 1));
    CHECK(!oracles_login_made_up_key(ORACLES_LOGIN_RAW_RETURN, 0u, 66u, 1));
    CHECK(!oracles_login_made_up_key(ORACLES_LOGIN_RAW_V, 0u, 0u, 1));
    CHECK(!oracles_login_made_up_key(ORACLES_LOGIN_RAW_OTHER, 0u, 0u, 1));
}

/* Paste's keys: Ctrl+V, Cmd+V on Apple's systems alone, never with Alt; Shift+Insert alone; the Paste key; held, once. */
static void paste_chords(void)
{
    const unsigned shift = ORACLES_LOGIN_MOD_SHIFT, ctrl = ORACLES_LOGIN_MOD_CTRL, alt = ORACLES_LOGIN_MOD_ALT, gui = ORACLES_LOGIN_MOD_GUI;
    for (int apple = 0; apple < 2; apple++) {
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_V, ctrl, 0, apple) == 1);
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_V, ctrl | shift, 0, apple) == 1);
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_V, ctrl | alt, 0, apple) == 0);   /* AltGr */
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_V, 0u, 0, apple) == 0);
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_V, shift, 0, apple) == 0);
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_V, 0u, 1, apple) == 0);           /* a v held types on */
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_V, ctrl, 1, apple) == -1);
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_V, gui, 0, apple) == apple);
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_V, gui | alt, 0, apple) == 0);
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_INSERT, shift, 0, apple) == 1);
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_INSERT, shift, 1, apple) == -1);
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_INSERT, 0u, 0, apple) == 0);
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_INSERT, shift | ctrl, 0, apple) == 0);
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_INSERT, shift | alt, 0, apple) == 0);
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_INSERT, shift | gui, 0, apple) == 0);
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_INSERT, ctrl, 0, apple) == 0);
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_PASTE, 0u, 0, apple) == 1);
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_PASTE, 0u, 1, apple) == -1);
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_RETURN, ctrl, 0, apple) == 0);
        CHECK(oracles_login_paste_chord(ORACLES_LOGIN_RAW_OTHER, ctrl, 0, apple) == 0);
    }
}

static void login_keys(void)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_UI_LAYOUT_16_9, 0, ORACLES_ACHIEVEMENTS_ROW_ACCOUNT, 0);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    oracles_achievements_act(&nav, ORACLES_HOME_OK);
    CHECK(nav.achievements.login == ORACLES_LOGIN_TYPING && nav.achievements.field == ORACLES_LOGIN_NAME && !oracles_achievements_keyboard_shown(&nav));
    /* A physical keyboard: Tab and Down to the next field, Shift+Tab and Up back, around the four; Backspace erases a
     * whole character. */
    oracles_achievements_login_text(&nav, "Sam\xc3\xa9");
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_BACKSPACE);
    CHECK(!strcmp(nav.achievements.name, "Sam"));
    static const OraclesLoginKey next[4] = { ORACLES_LOGIN_KEY_TAB, ORACLES_LOGIN_KEY_DOWN, ORACLES_LOGIN_KEY_TAB, ORACLES_LOGIN_KEY_DOWN };
    static const int fields[4] = { ORACLES_LOGIN_PASSWORD, ORACLES_LOGIN_OK, ORACLES_LOGIN_CANCEL, ORACLES_LOGIN_NAME };
    for (unsigned i = 0; i < 4; i++) { oracles_achievements_login_key(&nav, next[i]); CHECK(nav.achievements.field == fields[i]); }
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_BACKTAB);
    CHECK(nav.achievements.field == ORACLES_LOGIN_CANCEL);
    CHECK(oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_ENTER) == ORACLES_HOME_STAY && !nav.achievements.login);
    /* A wrong password: the error, on the password; typing again clears it. */
    oracles_achievements_login_open(&nav, 0);
    oracles_achievements_login_text(&nav, "SamLink");
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_TAB);
    oracles_achievements_login_text(&nav, "wrong");
    CHECK(oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_ENTER) == ORACLES_HOME_ACHIEVEMENTS_LOGIN);
    oracles_achievements_login_answered(&nav, 0, NULL);
    OraclesTone tone;
    CHECK(nav.achievements.login == ORACLES_LOGIN_WRONG && nav.achievements.field == ORACLES_LOGIN_PASSWORD
          && !strcmp(oracles_achievements_login_state_text(&nav, &tone), "Wrong user name or password.") && tone == ORACLES_TONE_ERROR);
    oracles_achievements_login_text(&nav, "!");
    CHECK(nav.achievements.login == ORACLES_LOGIN_TYPING && !strcmp(nav.achievements.password, "wrong!"));
    /* No answer: the error that the server cannot be reached, on Log in. */
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_ENTER);
    oracles_achievements_login_answered(&nav, -1, NULL);
    CHECK(nav.achievements.login == ORACLES_LOGIN_UNREACHABLE && nav.achievements.field == ORACLES_LOGIN_OK
          && strstr(oracles_achievements_login_state_text(&nav, &tone), "cannot be reached") != NULL);
    /* The pointer: a field, Cancel. */
    CHECK(oracles_achievements_login_click(&nav, ORACLES_LOGIN_NAME) == ORACLES_HOME_STAY && nav.achievements.field == ORACLES_LOGIN_NAME);
    CHECK(oracles_achievements_login_click(&nav, ORACLES_LOGIN_CANCEL) == ORACLES_HOME_STAY && !nav.achievements.login && !nav.achievements.name[0]);
    /* Back (the hint "Esc / B Cancel" clicked) closes it; while it logs in, Escape, B, Back and Cancel give the login up. */
    oracles_achievements_login_open(&nav, 0);
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && !nav.achievements.login && nav.screen == ORACLES_SCREEN_ACHIEVEMENTS);
    static const OraclesLoginKey cancels[2] = { ORACLES_LOGIN_KEY_ESCAPE, ORACLES_LOGIN_PAD_B };
    for (unsigned i = 0; i < 4; i++) {
        oracles_achievements_login_open(&nav, 1);
        nav.achievements.on = 1;
        oracles_achievements_login_text(&nav, "SamLink");
        nav.achievements.field = ORACLES_LOGIN_PASSWORD;
        oracles_achievements_login_text(&nav, "pw");
        CHECK(oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_ENTER) == ORACLES_HOME_ACHIEVEMENTS_LOGIN);
        CHECK(oracles_achievements_login_click(&nav, ORACLES_LOGIN_NAME) == ORACLES_HOME_STAY && nav.achievements.login == ORACLES_LOGIN_LOGGING);
        const OraclesHomeCommand c = i < 2 ? oracles_achievements_login_key(&nav, cancels[i])
                                   : i == 2 ? oracles_achievements_act(&nav, ORACLES_HOME_BACK) : oracles_achievements_login_click(&nav, ORACLES_LOGIN_CANCEL);
        CHECK(c == ORACLES_HOME_ACHIEVEMENTS_CANCEL && !nav.achievements.login && !nav.achievements.on && !nav.achievements.password[0]);
        oracles_achievements_login_answered(&nav, 1, "SamLink");   /* an answer come after: dropped */
        CHECK(!nav.achievements.user[0]);
    }
}

/* The fields: whole UTF-8 characters within 127 bytes; a text longer than the field drawn within it, its end in sight. */
static void login_fields_text(void)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_UI_LAYOUT_16_9, 0, ORACLES_ACHIEVEMENTS_ROW_ACCOUNT, 0);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    oracles_achievements_login_open(&nav, 0);
    char long_name[127];
    memset(long_name, 'a', 126);
    long_name[126] = 0;
    oracles_achievements_login_text(&nav, long_name);
    oracles_achievements_login_text(&nav, "\xc3\xa9");   /* two bytes: no room */
    CHECK(strlen(nav.achievements.name) == 126);
    oracles_achievements_login_text(&nav, "\xc3\xa9" "b");
    CHECK(strlen(nav.achievements.name) == 126);
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_BACKSPACE);
    oracles_achievements_login_text(&nav, "\xc3\xa9" "b");
    CHECK(strlen(nav.achievements.name) == 127 && !strcmp(nav.achievements.name + 125, "\xc3\xa9"));
    oracles_achievements_login_text(&nav, "\x01\xc3");   /* a control and a character cut: neither */
    CHECK(strlen(nav.achievements.name) == 127);
    nav.achievements.field = ORACLES_LOGIN_PASSWORD;
    for (unsigned i = 0; i < 100; i++) oracles_achievements_login_text(&nav, "x");
    for (unsigned layout = 0; layout < 2; layout++) {
        nav.layout = layout ? ORACLES_UI_LAYOUT_4_3 : ORACLES_UI_LAYOUT_16_9;
        OraclesUiLoginLayout l;
        oracles_ui_layout_login(nav.layout, &nav, &l);
        for (int f = 0; f < 2; f++) {
            const OraclesUiLoginField *field = &l.fields[f];
            const float right = field->clip.x + field->clip.w;
            CHECK(field->clip.x >= field->frame.x && right <= field->frame.x + field->frame.w);
            CHECK(field->value.x < field->clip.x && field->value.x + field->value.w <= right);   /* longer than the field, its end in it */
            if (f) CHECK(field->caret.w > 0.0f && field->caret.x + field->caret.w <= right);
        }
    }
}

/* The system's text input: for the field highlighted, of its kind; put away by the system (Android's Back), not again
 * until a field is chosen or typed into; none for the launcher's keyboard or while it logs in. */
static void login_text_input(void)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_UI_LAYOUT_16_9, 0, ORACLES_ACHIEVEMENTS_ROW_ACCOUNT, 0);
    oracles_home_input(&nav, ORACLES_INPUT_POINTER);
    CHECK(oracles_achievements_text_wanted(&nav, 0) == 0);
    oracles_achievements_login_open(&nav, 0);
    int want = oracles_achievements_text_wanted(&nav, 0);
    CHECK(want == ORACLES_LOGIN_NAME + 1);
    nav.achievements.text_for = want;   /* started */
    CHECK(oracles_achievements_text_wanted(&nav, 1) == ORACLES_LOGIN_NAME + 1);
    CHECK(oracles_achievements_text_wanted(&nav, 0) == 0);   /* put away */
    nav.achievements.text_for = 0;
    CHECK(oracles_achievements_text_wanted(&nav, 0) == 0);
    oracles_achievements_login_click(&nav, ORACLES_LOGIN_NAME);
    want = oracles_achievements_text_wanted(&nav, 0);
    CHECK(want == ORACLES_LOGIN_NAME + 1);
    nav.achievements.text_for = want;
    CHECK(oracles_achievements_text_wanted(&nav, 0) == 0);
    nav.achievements.text_for = 0;
    oracles_achievements_login_text(&nav, "S");
    CHECK(oracles_achievements_text_wanted(&nav, 0) == ORACLES_LOGIN_NAME + 1);
    nav.achievements.text_for = ORACLES_LOGIN_NAME + 1;
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_TAB);
    CHECK(oracles_achievements_text_wanted(&nav, 1) == ORACLES_LOGIN_PASSWORD + 1);   /* started again as a password */
    nav.achievements.text_for = ORACLES_LOGIN_PASSWORD + 1;
    CHECK(oracles_achievements_text_wanted(&nav, 0) == 0);
    nav.achievements.text_for = 0;
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_BACKTAB);
    CHECK(oracles_achievements_text_wanted(&nav, 0) == ORACLES_LOGIN_NAME + 1);   /* another field chosen */
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_TAB);
    oracles_achievements_login_text(&nav, "pw");
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_ENTER);
    CHECK(oracles_achievements_text_wanted(&nav, 1) == 0);   /* logging in */
    oracles_achievements_login_answered(&nav, -1, NULL);
    nav.achievements.field = ORACLES_LOGIN_NAME;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    CHECK(oracles_achievements_text_wanted(&nav, 1) == 0);   /* the launcher's keyboard */
}

/* With the developer's offline answers, On and Off stay in memory: settings.txt's achievements= is never written. */
static void store_offline(void)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_UI_LAYOUT_16_9, 0, ORACLES_ACHIEVEMENTS_ROW_ON, 0);
    int setting = 0, kept = 0;
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.achievements.login);
    oracles_achievements_store(&nav, 1, &setting, &kept);
    oracles_achievements_store(&nav, 0, &setting, &kept);
    CHECK(!setting && !kept);   /* the login open: Off until an account is in */
    oracles_achievements_login_text(&nav, "offline");
    nav.achievements.field = ORACLES_LOGIN_PASSWORD;
    oracles_achievements_login_text(&nav, "pw");
    CHECK(oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_ENTER) == ORACLES_HOME_ACHIEVEMENTS_LOGIN);
    oracles_achievements_login_answered(&nav, 1, "offline");
    oracles_achievements_store(&nav, 1, &setting, &kept);
    CHECK(!setting && kept);
    nav.row = ORACLES_ACHIEVEMENTS_ROW_ON;
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE);
    setting = 1;
    oracles_achievements_store(&nav, 1, &setting, &kept);
    CHECK(setting && !kept);
    oracles_achievements_store(&nav, 0, &setting, &kept);
    CHECK(!setting);   /* the player's own: written */
}

/* The launcher's keyboard, a controller's: the D-pad moves (left and right around its row, up and down clamped, the
 * column kept within the row), A types, B erases and on an empty field puts it away on Log in, Y is Shift for the next
 * letter, a shoulder the page of symbols, Start Done; a key of a physical keyboard puts it away. */
static void keyboard_keys(void)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_UI_LAYOUT_4_3, 0, ORACLES_ACHIEVEMENTS_ROW_ACCOUNT, 0);
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    oracles_achievements_act(&nav, ORACLES_HOME_OK);
    CHECK(oracles_achievements_keyboard_shown(&nav) && nav.achievements.key_row == 1 && nav.achievements.key_column == 0);
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_LEFT);
    CHECK(nav.achievements.key_column == 9);   /* p */
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_Y);
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_A);
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_A);
    CHECK(!strcmp(nav.achievements.name, "Pp"));
    for (unsigned i = 0; i < 6; i++) oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_DOWN);
    CHECK(nav.achievements.key_row == 4 && nav.achievements.key_column == 4);   /* Done: the last row's last */
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_LEFT);
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_A);   /* the eraser */
    CHECK(!strcmp(nav.achievements.name, "P"));
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_LEFT);
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_A);   /* Space */
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_SHOULDER);
    char keys[ORACLES_KEYBOARD_COLUMNS][8];
    CHECK(oracles_achievements_keyboard_keys(&nav, 1, keys) == 10 && !strcmp(keys[0], "!") && oracles_achievements_keyboard_keys(&nav, 4, keys) == 5
          && !strcmp(keys[1], "abc"));
    CHECK(oracles_achievements_keyboard_keys(&nav, 3, keys) == 8 && !strcmp(keys[0], "\\"));
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_UP);
    CHECK(nav.achievements.key_row == 3 && nav.achievements.key_column == 2);
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_A);
    CHECK(!strcmp(nav.achievements.name, "P ^"));
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_B);
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_START);
    CHECK(nav.achievements.field == ORACLES_LOGIN_PASSWORD && !strcmp(nav.achievements.name, "P "));
    /* B on the empty password: the keyboard away, on Log in, whose A sends nothing without a password. */
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_B);
    CHECK(nav.achievements.field == ORACLES_LOGIN_OK && !oracles_achievements_keyboard_shown(&nav));
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_LEFT);
    CHECK(nav.achievements.field == ORACLES_LOGIN_CANCEL);
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_UP);
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_UP);
    CHECK(nav.achievements.field == ORACLES_LOGIN_PASSWORD && oracles_achievements_keyboard_shown(&nav));
    /* A key of a physical keyboard: the device the hints follow, the launcher's keyboard away. */
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    CHECK(!oracles_achievements_keyboard_shown(&nav));
    oracles_achievements_login_text(&nav, "pw");
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    CHECK(oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_START) == ORACLES_HOME_STAY && nav.achievements.field == ORACLES_LOGIN_OK);
    CHECK(oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_START) == ORACLES_HOME_ACHIEVEMENTS_LOGIN);
    /* A click on a key types it. */
    oracles_achievements_login_answered(&nav, 0, NULL);
    CHECK(oracles_achievements_login_click(&nav, ORACLES_LOGIN_FIELDS + 0 * ORACLES_KEYBOARD_COLUMNS + 4) == ORACLES_HOME_STAY);
    CHECK(!strcmp(nav.achievements.password, "pw5"));
}

#ifdef ORACLES_RETROACHIEVEMENTS
/* ---- in a build with them: About's link, the pause's entries, Cheats' note -------------------------------------- */

static void about(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.portable_shown = 1;
    nav.achievements.shown = nav.achievements.on = 1;
    snprintf(nav.achievements.user, sizeof nav.achievements.user, "SamLink");
    nav.screen = ORACLES_SCREEN_ABOUT;
    nav.row = 2;
    char note[ORACLES_HOME_TEXT_LENGTH];
    OraclesUiCheatsLayout l;
    oracles_ui_layout_about(ORACLES_UI_LAYOUT_16_9, &nav, "v0.1.0", note, sizeof note, &l);
    const char *frame = "about ra";
    layout_box(frame, "panel", &l.panel);
    layout_near(frame, "update", "y", l.rows[0].y);
    layout_near(frame, "portable", "y", l.rows[1].y);
    layout_box(frame, "ra row", &l.rows[2]);
    layout_line(frame, "ra explanation", &l.explanations[2].lines[0]);
    layout_box(frame, "ra link", &l.link_boxes[2]);
    layout_line(frame, "ra summary text", &l.summaries[2]);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    hints(frame, &nav, ORACLES_UI_LAYOUT_16_9, 4);
    /* 4:3, Off: the link's summary, its help under the rows. */
    nav.achievements.on = 0;
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    oracles_ui_layout_about(ORACLES_UI_LAYOUT_4_3, &nav, "v0.1.0", note, sizeof note, &l);
    frame = "about ra 4:3";
    layout_box(frame, "ra", &l.rows[2]);
    layout_box(frame, "ra link", &l.link_boxes[2]);
    layout_line(frame, "ra summary text", &l.summaries[2]);
    layout_line(frame, "help text", &l.help);
    layout_line(frame, "help note text", &l.help_note);
    /* The keys: down from Portable mode to the link, right and OK open the page, left does nothing; Back returns on it. */
    nav.row = 1;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == 2);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_ABOUT);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_ACHIEVEMENTS && nav.row == 0);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_ABOUT && nav.row == 2);
    CHECK(oracles_about_click(&nav, 2, -1) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_ACHIEVEMENTS);
    /* Without Portable mode, the link is the second row. */
    nav.screen = ORACLES_SCREEN_ABOUT;
    nav.portable_shown = 0;
    CHECK(oracles_about_rows(&nav) == 2 && oracles_about_row_kind(&nav, 1) == ORACLES_ABOUT_ROW_ACHIEVEMENTS);
    nav.row = 1;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_ACHIEVEMENTS);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.row == 1);
    /* The login's hints follow the device: the keyboard's, a controller's, the launcher's keyboard's. */
    nav.achievements.user[0] = 0;
    oracles_achievements_open(&nav);
    nav.row = ORACLES_ACHIEVEMENTS_ROW_ACCOUNT;
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    oracles_achievements_act(&nav, ORACLES_HOME_OK);
    OraclesHomeHint h[ORACLES_HOME_MAX_HINTS];
    CHECK(oracles_home_hints(&nav, h) == 3 && !strcmp(h[0].key, "Tab") && h[0].chars == 5 && !strcmp(h[2].label, "Cancel") && h[2].back);
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    CHECK(oracles_home_hints(&nav, h) == 5 && !strcmp(h[3].key, "LB RB") && !strcmp(h[4].label, "Done"));
    nav.achievements.field = ORACLES_LOGIN_OK;
    CHECK(oracles_home_hints(&nav, h) == 3 && !strcmp(h[0].key, "D-pad") && !strcmp(h[0].label, "Move"));
}

/* 4:3's pause with Cheats and Achievements: eight entries, the title at the top left. */
static void pause_eight(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, 0);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    nav.achievements.active = 1;
    snprintf(nav.load_note, sizeof nav.load_note, "from 22:14");
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    CHECK(oracles_home_items(&nav, items) == 8 && items[6].id == ORACLES_HOME_ITEM_ACHIEVEMENTS);
    OraclesUiTitleLayout l;
    char state[ORACLES_HOME_STATE_LENGTH];
    oracles_home_state(&nav, ORACLES_HOME_HERO_AGES, state);
    oracles_ui_layout_hero(ORACLES_UI_LAYOUT_4_3, 1, oracles_home_over(ORACLES_HOME_HERO_AGES), oracles_home_title(ORACLES_HOME_HERO_AGES), state, &l);
    oracles_ui_layout_hero_left(ORACLES_UI_LAYOUT_4_3, &l);
    layout_line("pause achievements 640x480", "title", &l.title);
    layout_box("pause achievements 640x480", "hero", &l.box);
}

static void cheats_note(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    OraclesHomeGame *g = &nav.games[ORACLES_HOME_GAME_AGES];
    g->usable = 1;
    g->rom = ORACLES_ROM_ORIGINAL;
    nav.screen = ORACLES_SCREEN_CHEATS;
    nav.achievements.shown = 1;
    OraclesUiCheatsLayout l;
    oracles_ui_layout_cheats(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(l.note2.count == 0);   /* off: none */
    nav.achievements.on = 1;
    snprintf(nav.achievements.user, sizeof nav.achievements.user, "SamLink");
    oracles_ui_layout_cheats(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    layout_near("cheats achievements", "note", "y", l.note.lines[0].y);
    layout_near("cheats achievements", "warning", "y", l.note2.lines[0].y);
    layout_near("cheats achievements", "warning", "h", l.note2.h);
    layout_line("cheats achievements", "warning text", &l.note2.lines[0]);
    layout_near("cheats achievements", "warning text", "lines", (float)l.note2.count);
    /* 4:3: on the line under the note (the mockup's head lacks the game's title: compared under its note). */
    oracles_ui_layout_cheats(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    const char *frame = "cheats achievements 4:3";
    layout_near_within(frame, "warning", "y", l.note2.lines[0].y - l.note.lines[0].y + layout_reference(frame, "note", "y"), LAYOUT_TOLERANCE);
    layout_near(frame, "warning text", "x", l.note2.lines[0].x);
    layout_near(frame, "warning text", "w", l.note2.lines[0].w);
    layout_near(frame, "warning text", "h", l.note2.lines[0].h);
    /* A fan game's page, an unrecognised ROM: none. */
    g->rom = ORACLES_ROM_UNRECOGNISED;
    oracles_ui_layout_cheats(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(l.note2.count == 0);
    /* From the pause, the session's. */
    oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_FAITHFUL, 0);
    nav.screen = ORACLES_SCREEN_CHEATS;
    nav.achievements.active = 1;
    oracles_ui_layout_cheats(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(l.note2.count == 2);
}
#endif

int main(int argc, char **argv)
{
    if (argc < 2 || !layout_reference_load(argv[1])) { fprintf(stderr, "usage: oracles-test-launcher-achievements launcher_layout_reference.json\n"); return 2; }
    if (!oracles_ui_fonts_load()) { fprintf(stderr, "FAIL the embedded fonts do not load\n"); return 1; }
    page_16_9("ra off", 0, 0);
    page_16_9("ra on", 1, 2);
    page_4_3();
    login_16_9("login", ORACLES_LOGIN_TYPING, ORACLES_LOGIN_PASSWORD);
    login_16_9("login wrong", ORACLES_LOGIN_WRONG, ORACLES_LOGIN_PASSWORD);
    login_16_9("login logging", ORACLES_LOGIN_LOGGING, ORACLES_LOGIN_OK);
    login_android();
    login_keyboard();
    toasts_layout();
    toasts_queue();
    sound_row(ORACLES_UI_LAYOUT_16_9, 0);
    sound_row(ORACLES_UI_LAYOUT_16_9, 1);
    sound_row(ORACLES_UI_LAYOUT_4_3, 1);
    sound_keys();
    sound_rendered();
    sound_mixed();
    sound_cleared();
    stand_ins();
    list_16_9();
    list_4_3();
    list_keys();
    page_keys();
    login_keys();
    login_paste();
    login_typed();
    paste_chords();
    password_shown();
    login_fields_text();
    login_text_input();
    store_offline();
    keyboard_keys();
#ifdef ORACLES_RETROACHIEVEMENTS
    about();
    pause_eight();
    cheats_note();
#endif
    if (layout_failures()) { fprintf(stderr, "%d failure(s)\n", layout_failures()); return 1; }
    printf("launcher achievements: the screens match the layout reference within %.1f px, and their keys do what they say\n", (double)LAYOUT_TOLERANCE);
    return 0;
}
