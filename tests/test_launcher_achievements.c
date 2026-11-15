/* RetroAchievements' screens (ui_achievements.h): their layout against the layout reference,
 * tests/launcher_layout_reference.json (layout_reference.h), the test's argument, and their keys.  The page logged out
 * (39e, 39e 640x480), with the game's list (39f, 39f 640x480), scrolled (39g, 39g 640x480); the
 * login with a keyboard (login), after a wrong password (login wrong), while it logs in (login logging), under Android's
 * keyboard (login android), with the launcher's keyboard at 640x480 (login keyboard 640x480); the toasts over a game
 * (toast unlock, toast 640x480, toast stack); the pause's list (achievements, achievements small pause, achievements
 * paused, achievements 640x480); the eye on the password and the paste's lines (39h to 39p).  In a build with them, the home menu's entry, the pause of eight entries (pause achievements 640x480) and
 * Cheats' note (cheats achievements, cheats achievements 4:3) too.  And the keys: the page's, the login's (its paste, its
 * keys and its password shown included), the launcher's keyboard's, the toasts' queue and the list's scroll; and the
 * unlock's sound itself (ui_achievements_sound.h), its length, peak and ends, and its mixing.  An achievement's detail: its
 * frames (40a to 40f), its texts, its lengths, its keys and the touch. */
#include "layout_reference.h"
#include "ui_achievements.h"
#include "ui_achievements_sound.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); layout_fail(); } } while (0)

/* The page opened from the home menu, its row `row` highlighted: on and logged in as SamLink, `pending` unlocks waiting. */
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

/* The four rows, the probes every frame of the page has. */
static void rows(const char *frame, const OraclesUiCheatsLayout *l)
{
    layout_near(frame, "account", "y", l->rows[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT].y);
    layout_near(frame, "on", "y", l->rows[ORACLES_ACHIEVEMENTS_ROW_ON].y);
    layout_near(frame, "privacy", "y", l->rows[ORACLES_ACHIEVEMENTS_ROW_PRIVACY].y);
    layout_near(frame, "sound", "y", l->rows[ORACLES_ACHIEVEMENTS_ROW_SOUND].y);
    layout_box(frame, "on off", &l->choices[ORACLES_ACHIEVEMENTS_ROW_ON][0].box);
    layout_box(frame, "on on", &l->choices[ORACLES_ACHIEVEMENTS_ROW_ON][1].box);
    layout_box(frame, "sound on", &l->choices[ORACLES_ACHIEVEMENTS_ROW_SOUND][0].box);
    layout_box(frame, "sound off", &l->choices[ORACLES_ACHIEVEMENTS_ROW_SOUND][1].box);
    layout_line(frame, "section", &l->head.section);
}

/* The page logged out (39e), the line under the rows asking to log in; its keys in 16:9, the sound's help in 4:3 at
 * 640x480, the line pushing the help down (39e 640x480). */
static void page_16_9(void)
{
    const char *frame = "39e";
    OraclesHomeNav nav;
    page(&nav, ORACLES_UI_LAYOUT_16_9, 0, ORACLES_ACHIEVEMENTS_ROW_ACCOUNT, 0);
    OraclesUiCheatsLayout l;
    oracles_ui_layout_achievements(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    rows(frame, &l);
    layout_box(frame, "panel", &l.panel);
    layout_line(frame, "over", &l.head.over);
    layout_line(frame, "title", &l.head.title.lines[0]);
    layout_near(frame, "note", "y", l.note.lines[0].y);
    layout_line(frame, "account explanation", &l.explanations[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT].lines[0]);
    layout_line(frame, "sound explanation", &l.explanations[ORACLES_ACHIEVEMENTS_ROW_SOUND].lines[0]);
    layout_near(frame, "sound explanation", "lines", (float)l.explanations[ORACLES_ACHIEVEMENTS_ROW_SOUND].count);
    layout_box(frame, "login", &l.action_boxes[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT]);
    layout_near(frame, "off", "y", l.below_rule.y);
    layout_near(frame, "off", "x", l.below_rule.x);
    layout_near(frame, "off", "w", l.below_rule.w);
    layout_line(frame, "off text", &l.below);
    /* Logged in with two unlocks waiting: under Account. */
    page(&nav, ORACLES_UI_LAYOUT_16_9, 1, ORACLES_ACHIEVEMENTS_ROW_ACCOUNT, 2);
    oracles_ui_layout_achievements(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(l.status.count == 1 && l.status.lines[0].y > l.rows[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT].y && l.status.lines[0].y < l.rows[ORACLES_ACHIEVEMENTS_ROW_ON].y);
    /* With its status, and the state's longest line under the rows, it stays above the hints. */
    OraclesHomeHint h[ORACLES_HOME_MAX_HINTS];
    OraclesUiHintLayout hl[ORACLES_HOME_MAX_HINTS];
    const unsigned count = oracles_home_hints(&nav, h);
    oracles_ui_layout_hints(ORACLES_UI_LAYOUT_16_9, h, count, hl);
    for (unsigned i = 0; i < count; i++) CHECK(l.panel.y + l.panel.h < hl[i].box.y);
    /* Off, logged in: how to see them. */
    nav.achievements.on = 0;
    CHECK(!strcmp(oracles_achievements_page_line(&nav), "Turn RetroAchievements on to see this game\xe2\x80\x99s achievements."));
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
    const char *frame = "39e 640x480";
    OraclesHomeNav nav;
    page(&nav, ORACLES_UI_LAYOUT_4_3, 0, ORACLES_ACHIEVEMENTS_ROW_SOUND, 0);
    nav.android = 1;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    OraclesUiCheatsLayout l;
    oracles_ui_layout_achievements(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    rows(frame, &l);
    layout_line(frame, "title", &l.head.title.lines[0]);
    layout_box(frame, "account", &l.rows[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT]);
    layout_box(frame, "on", &l.rows[ORACLES_ACHIEVEMENTS_ROW_ON]);
    layout_box(frame, "privacy", &l.rows[ORACLES_ACHIEVEMENTS_ROW_PRIVACY]);
    layout_box(frame, "sound", &l.rows[ORACLES_ACHIEVEMENTS_ROW_SOUND]);
    layout_near(frame, "off", "y", l.below_rule.y);
    layout_line(frame, "off text", &l.below);
    layout_line(frame, "help text", &l.help);
    layout_line(frame, "help note text", &l.help_note);
    CHECK(!strcmp(l.help_text, "On: a short sound with each unlock.") && !strcmp(l.help_note_text, "The toast shows either way."));
    /* Above the help bar. */
    OraclesHomeHint h[ORACLES_HOME_MAX_HINTS];
    OraclesUiHintLayout hl[ORACLES_HOME_MAX_HINTS];
    const unsigned count = oracles_home_hints(&nav, h);
    oracles_ui_layout_hints(ORACLES_UI_LAYOUT_4_3, h, count, hl);
    for (unsigned i = 0; i < count; i++) CHECK(l.help_note.y + l.help_note.h < hl[i].box.y);
}

/* Unlock sound's keys: Up and Down through the four rows and round, Left, Right and OK turn it, a click on On or Off sets
 * it; each change a command to store, none for the choice it has; not while the login is open. */
static void sound_keys(void)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_UI_LAYOUT_16_9, 1, ORACLES_ACHIEVEMENTS_ROW_PRIVACY, 0);
    CHECK(nav.achievements.sound);
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_ACHIEVEMENTS_ROW_SOUND);
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_ACHIEVEMENTS_ROW_ACCOUNT);
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
    CHECK(!strcmp(help, "On: a short sound with each unlock."));
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
    static const unsigned keys[][3] = { { 0, 0, 0 }, { 9, 0, 9 }, { 10, 1, 0 }, { 26, 2, 6 }, { 39, 3, 9 } };
    for (unsigned i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        char name[16];
        snprintf(name, sizeof name, "key %u", keys[i][0]);
        layout_box(frame, name, &l.keys[keys[i][1]][keys[i][2]]);
    }
    CHECK(l.key_count[3] == 10 && l.key_count[4] == 6 && !strcmp(l.key_texts[2][6], "j") && !strcmp(l.key_texts[4][1], "#+="));
    layout_line(frame, "key 1 text", &l.key_names[0][0]);
    layout_line(frame, "key j text", &l.key_names[2][6]);
    layout_line(frame, "key shift text", &l.key_names[4][0]);
    layout_line(frame, "key symbols text", &l.key_names[4][1]);
}

/* The eye at the right end of the password, against a frame of the login in `layout`: its box, kept while the password is
 * empty, and the text's before it. */
static void eye_frame(const char *frame, OraclesHomeNav *nav, OraclesUiLoginLayout *l, int shown)
{
    oracles_ui_layout_login(nav->layout, nav, l);
    const OraclesUiLoginField *password = &l->fields[ORACLES_LOGIN_PASSWORD];
    CHECK(password->eye_shown == shown && l->fields[ORACLES_LOGIN_NAME].eye.w == 0.0f);
    layout_box(frame, "eye", &password->eye);
    layout_box(frame, "password", &password->box);
    layout_box(frame, "dialog", &l->dialog);
    layout_box(frame, "state", &l->state);
}

/* The eye: none on an empty password, its room kept (39h user, 39h pass); dots and the hints with Ctrl+V (39i, 39i user);
 * in letters (39j shown); a long password, its end before the eye (39k shown); the eye highlighted with Tab (39l eye); X
 * Show with a controller (39l pad); X Show in the launcher's keyboard's hints, its Paste key (39m); under Android's keyboard,
 * no hints (39n); faded while it logs in (39o); a paste refused (39p multi, 39p long 640x480). */
static void login_eye(void)
{
    OraclesHomeNav nav;
    OraclesUiLoginLayout l;
    const OraclesUiBox *text;
    login(&nav, ORACLES_UI_LAYOUT_16_9, "", "", ORACLES_LOGIN_NAME, ORACLES_LOGIN_TYPING);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    eye_frame("39h user", &nav, &l, 0);
    layout_box("39h user", "user text", &l.fields[ORACLES_LOGIN_NAME].clip);
    layout_box("39h user", "caret", &l.fields[ORACLES_LOGIN_NAME].caret);
    login(&nav, ORACLES_UI_LAYOUT_16_9, "SamLink", "", ORACLES_LOGIN_PASSWORD, ORACLES_LOGIN_TYPING);
    eye_frame("39h pass", &nav, &l, 0);
    layout_box("39h pass", "password text", &l.fields[ORACLES_LOGIN_PASSWORD].clip);
    layout_box("39h pass", "caret", &l.fields[ORACLES_LOGIN_PASSWORD].caret);

    login(&nav, ORACLES_UI_LAYOUT_16_9, "SamLink", "hunter7", ORACLES_LOGIN_PASSWORD, ORACLES_LOGIN_TYPING);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    eye_frame("39i", &nav, &l, 1);
    text = &l.fields[ORACLES_LOGIN_PASSWORD].clip;
    layout_box("39i", "password text", text);
    layout_box("39i", "password value", &(OraclesUiBox){ l.fields[1].value.x, l.fields[1].value.y, l.fields[1].value.w, l.fields[1].value.h });
    layout_box("39i", "caret", &l.fields[ORACLES_LOGIN_PASSWORD].caret);
    hints("39i", &nav, ORACLES_UI_LAYOUT_16_9, 4);
    nav.achievements.field = ORACLES_LOGIN_NAME;
    eye_frame("39i user", &nav, &l, 1);
    layout_box("39i user", "password value", &(OraclesUiBox){ l.fields[1].value.x, l.fields[1].value.y, l.fields[1].value.w, l.fields[1].value.h });

    nav.achievements.field = ORACLES_LOGIN_PASSWORD;
    oracles_achievements_password_toggle(&nav);
    eye_frame("39j shown", &nav, &l, 1);
    CHECK(l.shown);
    layout_line("39j shown", "password value text", &l.fields[ORACLES_LOGIN_PASSWORD].value);
    layout_box("39j shown", "caret", &l.fields[ORACLES_LOGIN_PASSWORD].caret);

    /* The mockup pushes its long text to the right by its length alone, though it fits: here the text moves once it reaches
     * the eye, the caret where the frame has it. */
    static const char long_password[] = "correct-horse-battery-staple-under-the-old-lighthouse-2026-and-its-keeper";
    login(&nav, ORACLES_UI_LAYOUT_16_9, "SamLink", long_password, ORACLES_LOGIN_PASSWORD, ORACLES_LOGIN_TYPING);
    oracles_achievements_password_toggle(&nav);
    eye_frame("39k shown", &nav, &l, 1);
    layout_box("39k shown", "password text", &l.fields[ORACLES_LOGIN_PASSWORD].clip);
    layout_box("39k shown", "caret", &l.fields[ORACLES_LOGIN_PASSWORD].caret);
    CHECK(l.fields[ORACLES_LOGIN_PASSWORD].value.x < l.fields[ORACLES_LOGIN_PASSWORD].clip.x);

    /* Hidden, its dots fit: from the field's left, unlike the frame, the room the same. */
    login(&nav, ORACLES_UI_LAYOUT_16_9, "SamLink", "correct-horse-battery-staple-under-the-old-lighthouse-2026", ORACLES_LOGIN_PASSWORD, ORACLES_LOGIN_TYPING);
    eye_frame("39k hidden", &nav, &l, 1);
    layout_box("39k hidden", "password text", &l.fields[ORACLES_LOGIN_PASSWORD].clip);
    CHECK(l.fields[ORACLES_LOGIN_PASSWORD].value.x == l.fields[ORACLES_LOGIN_PASSWORD].clip.x);

    login(&nav, ORACLES_UI_LAYOUT_16_9, "SamLink", "hunter7", ORACLES_LOGIN_EYE, ORACLES_LOGIN_TYPING);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    eye_frame("39l eye", &nav, &l, 1);
    CHECK(l.fields[ORACLES_LOGIN_PASSWORD].caret.w == 0.0f);
    hints("39l eye", &nav, ORACLES_UI_LAYOUT_16_9, 3);
    nav.achievements.field = ORACLES_LOGIN_OK;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    eye_frame("39l pad", &nav, &l, 1);
    layout_box("39l pad", "ok", &l.buttons[0]);
    hints("39l pad", &nav, ORACLES_UI_LAYOUT_16_9, 4);

    login(&nav, ORACLES_UI_LAYOUT_4_3, "SamLink", "hunter7", ORACLES_LOGIN_PASSWORD, ORACLES_LOGIN_TYPING);
    nav.android = 1;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    nav.achievements.key_row = 4;
    nav.achievements.key_column = 3;
    eye_frame("39m", &nav, &l, 1);
    layout_box("39m", "keyboard", &l.keyboard);
    layout_box("39m", "password text", &l.fields[ORACLES_LOGIN_PASSWORD].clip);
    for (unsigned k = 0; k < 6; k++) {
        char name[16];
        snprintf(name, sizeof name, "key %u", 40u + k);
        layout_box("39m", name, &l.keys[4][k]);
    }
    CHECK(!strcmp(l.key_texts[4][3], "Paste"));
    layout_line("39m", "key space text", &l.key_names[4][2]);
    layout_line("39m", "key paste text", &l.key_names[4][3]);
    hints("39m", &nav, ORACLES_UI_LAYOUT_4_3, 5);

    login(&nav, ORACLES_UI_LAYOUT_16_9, "SamLink", "hunter7", ORACLES_LOGIN_PASSWORD, ORACLES_LOGIN_TYPING);
    nav.android = 1;
    nav.achievements.system_keyboard = 1;
    eye_frame("39n", &nav, &l, 1);
#ifdef ORACLES_RETROACHIEVEMENTS
    OraclesHomeHint none[ORACLES_HOME_MAX_HINTS];
    CHECK(oracles_home_hints(&nav, none) == 0);
#endif

    login(&nav, ORACLES_UI_LAYOUT_16_9, "SamLink", "hunter7", ORACLES_LOGIN_OK, ORACLES_LOGIN_LOGGING);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    eye_frame("39o", &nav, &l, 1);
    layout_line("39o", "state text", &l.state_text);

    login(&nav, ORACLES_UI_LAYOUT_16_9, "SamLink", "hunter7", ORACLES_LOGIN_PASSWORD, ORACLES_LOGIN_TYPING);
    nav.achievements.paste_refused = ORACLES_LOGIN_PASTE_REFUSED;
    eye_frame("39p multi", &nav, &l, 1);
    layout_line("39p multi", "state line", &l.state_text);
    login(&nav, ORACLES_UI_LAYOUT_4_3, "SamLink", "hunter7", ORACLES_LOGIN_OK, ORACLES_LOGIN_TYPING);
    nav.android = 1;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    nav.achievements.paste_refused = ORACLES_LOGIN_PASTE_TOO_LONG;
    eye_frame("39p long 640x480", &nav, &l, 1);
    layout_line("39p long 640x480", "state line", &l.state_text);
}

/* The eye's keys: X wherever the highlight is, on the launcher's keyboard too, the hint following; a Tab stop after Password
 * once there is one to show, Enter and Space toggling it, the D-pad's order unchanged; a click; inert while it logs in.  The
 * paste refused, said in the state line in the warning's tone, gone with the next edit; the launcher's keyboard's Paste key. */
static void login_eye_keys(void)
{
    OraclesHomeNav nav;
    OraclesTone tone;
    login(&nav, ORACLES_UI_LAYOUT_16_9, "SamLink", "hunter7", ORACLES_LOGIN_OK, ORACLES_LOGIN_TYPING);
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    CHECK(oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_X) == ORACLES_HOME_STAY && oracles_achievements_password_shown(&nav));
    OraclesHomeHint h[ORACLES_HOME_MAX_HINTS];
    CHECK(oracles_achievements_hints(&nav, h) == 4 && !strcmp(h[2].key, "X") && !strcmp(h[2].label, "Hide"));
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_X);
    CHECK(!oracles_achievements_password_shown(&nav) && oracles_achievements_hints(&nav, h) == 4 && !strcmp(h[2].label, "Show"));
    /* The D-pad's order: the name, the password, Log in, Cancel. */
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_UP);
    CHECK(nav.achievements.field == ORACLES_LOGIN_PASSWORD);
    nav.achievements.field = ORACLES_LOGIN_OK;
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    /* On the launcher's keyboard too. */
    nav.achievements.field = ORACLES_LOGIN_PASSWORD;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    CHECK(oracles_achievements_keyboard_shown(&nav) && oracles_achievements_hints(&nav, h) == 5 && !strcmp(h[2].label, "Show") && !strcmp(h[3].label, "Shift"));
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_X);
    CHECK(oracles_achievements_password_shown(&nav) && !strcmp(nav.achievements.password, "hunter7"));
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_SHOULDER);
    CHECK(nav.achievements.keyboard_page == 1);   /* LB and RB still go to the symbols */
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_X);
    /* A keyboard: the eye after Password, its hints, Enter and Space. */
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_TAB);
    CHECK(nav.achievements.field == ORACLES_LOGIN_EYE && oracles_achievements_hints(&nav, h) == 3 && !strcmp(h[1].key, "Enter") && !strcmp(h[1].label, "Show"));
    CHECK(oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_ENTER) == ORACLES_HOME_STAY && oracles_achievements_password_shown(&nav));
    CHECK(oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_SPACE) == ORACLES_HOME_STAY && !oracles_achievements_password_shown(&nav));
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_TAB);
    CHECK(nav.achievements.field == ORACLES_LOGIN_OK);
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_BACKTAB);
    CHECK(nav.achievements.field == ORACLES_LOGIN_EYE);
    /* Space elsewhere is a character, not the eye's. */
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_BACKTAB);
    CHECK(nav.achievements.field == ORACLES_LOGIN_PASSWORD);
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_SPACE);
    CHECK(!oracles_achievements_password_shown(&nav) && oracles_achievements_hints(&nav, h) == 4 && !strcmp(h[2].label, "Paste") && h[0].chars == 6);
    /* An empty password: no stop. */
    nav.achievements.password[0] = 0;
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_TAB);
    CHECK(nav.achievements.field == ORACLES_LOGIN_OK);
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_KEY_BACKTAB);
    CHECK(nav.achievements.field == ORACLES_LOGIN_PASSWORD);
    /* The pointer, and inert while it logs in. */
    oracles_achievements_login_text(&nav, "pw");
    CHECK(oracles_achievements_login_click(&nav, ORACLES_LOGIN_EYE) == ORACLES_HOME_STAY && oracles_achievements_password_shown(&nav)
          && nav.achievements.field == ORACLES_LOGIN_PASSWORD);
    oracles_achievements_login_click(&nav, ORACLES_LOGIN_EYE);
    nav.achievements.login = ORACLES_LOGIN_LOGGING;
    CHECK(oracles_achievements_login_click(&nav, ORACLES_LOGIN_EYE) == ORACLES_HOME_STAY && !oracles_achievements_password_shown(&nav));
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_X);
    CHECK(!oracles_achievements_password_shown(&nav));
    nav.achievements.login = ORACLES_LOGIN_TYPING;
    /* A paste refused, said until the next edit. */
    char several[] = "one\ntwo", huge[200];
    memset(huge, 'a', sizeof huge - 1);
    huge[sizeof huge - 1] = 0;
    CHECK(oracles_achievements_login_paste(&nav, several) == ORACLES_LOGIN_PASTE_REFUSED);
    CHECK(!strcmp(oracles_achievements_login_state_text(&nav, &tone), "Not pasted: more than one line") && tone == ORACLES_TONE_WARN);
    CHECK(oracles_achievements_login_paste(&nav, huge) == ORACLES_LOGIN_PASTE_TOO_LONG);
    CHECK(!strcmp(oracles_achievements_login_state_text(&nav, &tone), "Not pasted: too long") && tone == ORACLES_TONE_WARN);
    oracles_achievements_login_text(&nav, "x");
    CHECK(!oracles_achievements_login_state_text(&nav, &tone));
    /* The launcher's keyboard's Paste key: a command to the host, which has the clipboard. */
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    nav.achievements.key_row = 4;
    nav.achievements.key_column = 3;
    CHECK(oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_A) == ORACLES_HOME_ACHIEVEMENTS_PASTE);
    CHECK(oracles_achievements_login_click(&nav, ORACLES_LOGIN_FIELDS + 4 * ORACLES_KEYBOARD_COLUMNS + 3) == ORACLES_HOME_ACHIEVEMENTS_PASTE);
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
        { "The Last Door", "Finish the game.", 50, NULL, "", 0.0f },
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
    /* The column shows the highlighted achievement's detail; the casual mode's note, in a pause too small for it. */
    CHECK(oracles_achievements_detail_column(&nav));
    nav.achievements.view_w = 800.0f;
    CHECK(!oracles_achievements_detail_column(&nav));
    OraclesUiAchievementsLayout small;
    oracles_ui_layout_achievements_list(ORACLES_UI_LAYOUT_16_9, &nav, &small);
    layout_near("achievements small pause", "mode", "y", small.mode.lines[0].y);
    layout_near("achievements small pause", "mode", "h", small.mode.h);
    nav.achievements.view_w = 0.0f;
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
    hints(frame, &nav, ORACLES_UI_LAYOUT_4_3, 3);
}

/* Up and down move the highlight, the list scrolling as the highlight passes its foot; Back returns to the pause on
 * Achievements. */
static void list_keys(void)
{
    OraclesHomeNav nav;
    list_nav(&nav, ORACLES_UI_LAYOUT_16_9, 0, ORACLES_ACHIEVEMENTS_RUNNING);
    CHECK(oracles_achievements_list_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.achievements.list_focus == 0);
    const unsigned fit = oracles_achievements_list_fit(&nav);
    for (unsigned i = 0; i < fit; i++) oracles_achievements_list_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.achievements.list_focus == fit && oracles_achievements_list_first(&nav) == 1);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    CHECK(oracles_achievements_list_first(&nav) == fit + 1u - oracles_achievements_list_fit(&nav));
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

/* ---- an achievement's detail -------------------------------------------------------------------------------- */

static OraclesAchievementsList details;

/* The examples of the frames: an unlocked one with a long description, a locked, Missable and rare one. */
static void detail_examples(void)
{
    static const struct { const char *t, *d; unsigned p; const char *on, *progress; float percent, rarity; unsigned tag; } a[8] = {
        { "Out of the Woods", "Leave the first forest.", 5, "21 Sep 2026", "", 0.0f, 86.0f, ORACLES_ACHIEVEMENT_TAG_PROGRESSION },
        { "Clean Sweep", "Open every chest of the first dungeon in a single visit, without leaving it, without being hit by its guardian and without loading a savestate on the way.",
          10, "7 Nov 2026", "", 0.0f, 12.4f, ORACLES_ACHIEVEMENT_TAG_NONE },
        { "Ten Hearts", "Hold ten heart containers.", 10, "2 Oct 2026", "", 0.0f, 31.7f, ORACLES_ACHIEVEMENT_TAG_NONE },
        { "Seed Gatherer", "Carry all five kinds of seeds.", 10, NULL, "3/5", 0.6f, 22.9f, ORACLES_ACHIEVEMENT_TAG_NONE },
        { "Rupee Hoarder", "Hold 500 rupees at once.", 10, NULL, "312/500", 0.624f, 18.1f, ORACLES_ACHIEVEMENT_TAG_NONE },
        { "Deep Water", "Reach the bottom of the sea.", 25, NULL, "", 0.0f, 9.6f, ORACLES_ACHIEVEMENT_TAG_NONE },
        { "No Potion Needed", "Defeat the second dungeon\xe2\x80\x99s guardian without drinking a potion, on the first try: once the guardian falls, the chance is gone for this file.",
          25, NULL, "", 0.0f, 0.3f, ORACLES_ACHIEVEMENT_TAG_MISSABLE },
        { "The Last Door", "Finish the game.", 50, NULL, "", 0.0f, 14.8f, ORACLES_ACHIEVEMENT_TAG_WIN },
    };
    memset(&details, 0, sizeof details);
    details.count = 8;
    for (unsigned i = 0; i < details.count; i++) {
        OraclesAchievementsItem *item = &details.items[i];
        snprintf(item->title, sizeof item->title, "%s", a[i].t);
        snprintf(item->description, sizeof item->description, "%s", a[i].d);
        item->points = a[i].p;
        item->unlocked = a[i].on != NULL;
        if (a[i].on) snprintf(item->date, sizeof item->date, "%s", a[i].on);
        snprintf(item->progress, sizeof item->progress, "%s", a[i].progress);
        item->percent = a[i].percent;
        item->rarity = a[i].rarity;
        item->tag = a[i].tag;
    }
}

/* The home page's list has no dates; the highlight on achievement `item`. */
static void details_page(OraclesHomeNav *nav, OraclesUiLayout layout, unsigned item)
{
    detail_examples();
    for (unsigned i = 0; i < details.count; i++) details.items[i].date[0] = 0;
    page(nav, layout, 1, ORACLES_ACHIEVEMENTS_ROWS + item, 0);
    nav->achievements.list = &details;
    nav->achievements.list_state = ORACLES_ACHIEVEMENTS_LIST_READY;
}

/* The pause's list, the highlight on achievement `item`. */
static void details_list(OraclesHomeNav *nav, OraclesUiLayout layout, unsigned item)
{
    detail_examples();
    oracles_home_init(nav);
    oracles_home_pause(nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, 0);
    nav->layout = layout;
    nav->achievements.shown = nav->achievements.active = 1;
    nav->achievements.list = &details;
    oracles_achievements_list_open(nav);
    nav->achievements.list_focus = item;
}

/* The detail against its frame: its pieces; the rarity, the tag and the state's text. */
static void detail_frame(const char *frame, const OraclesHomeNav *nav)
{
    const OraclesAchievementsItem *item = oracles_achievements_detail_item(nav);
    CHECK(item != NULL);
    if (!item) return;
    static OraclesUiAchievementDetail d;
    oracles_ui_layout_achievement_detail(nav->layout, item, &d);
    layout_box(frame, "detail badge", &d.badge);
    layout_near(frame, "detail title", "x", d.title[0].x);
    layout_near(frame, "detail title", "y", d.title[0].y);
    layout_near(frame, "detail title", "h", d.title[0].h * (float)d.title_lines);
    layout_near(frame, "detail points", "x", d.points.x);
    layout_near(frame, "detail points", "y", d.points.y);
    layout_near(frame, "detail points", "w", d.points.w);
    layout_near(frame, "detail points", "h", d.points.h);
    layout_line(frame, "rarity text", &d.rarity);
    layout_near(frame, "detail desc", "x", d.description[0].x);
    layout_near(frame, "detail desc", "y", d.description[0].y);
    layout_near(frame, "detail desc", "h", d.description[0].h * (float)d.description_lines);
    layout_near(frame, "detail state", "y", d.state.y);
    layout_near(frame, "detail state", "h", d.state.h);
    layout_line(frame, "state text", &d.state);
    if (d.tag.w > 0.0f) {
        layout_box(frame, "detail tag", &d.tag);
        layout_line(frame, "tag text", &d.tag_name);
    }
    if (nav->layout == ORACLES_UI_LAYOUT_4_3) layout_box(frame, "detail", &d.panel);
    else CHECK(d.panel.w == 0.0f);
}

/* Each frame. */
static void detail_frames(void)
{
    OraclesHomeNav nav;
    details_page(&nav, ORACLES_UI_LAYOUT_16_9, 1u);
    detail_frame("40a", &nav);
    static OraclesUiAchievementDetail d;
    oracles_ui_layout_achievement_detail(nav.layout, oracles_achievements_detail_item(&nav), &d);
    layout_line("40a", "points text", &d.points);
    layout_line("40a", "title text", &d.title[0]);
    layout_line("40a", "desc text", &d.description[0]);
    CHECK(d.description_lines == 3u);
    details_page(&nav, ORACLES_UI_LAYOUT_16_9, 6u);
    detail_frame("40b", &nav);
    oracles_ui_layout_achievement_detail(nav.layout, oracles_achievements_detail_item(&nav), &d);
    layout_line("40b", "title text", &d.title[0]);
    details_page(&nav, ORACLES_UI_LAYOUT_4_3, 1u);
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && oracles_achievements_detail_open(&nav));
    detail_frame("40c", &nav);
    /* Opened by a touch. */
    details_page(&nav, ORACLES_UI_LAYOUT_4_3, 6u);
    oracles_achievements_click(&nav, nav.row, -1);
    detail_frame("40d", &nav);
    /* The pause's, with their date. */
    details_list(&nav, ORACLES_UI_LAYOUT_16_9, 1u);
    CHECK(oracles_achievements_detail_column(&nav));
    detail_frame("40e", &nav);
    details_list(&nav, ORACLES_UI_LAYOUT_4_3, 1u);
    CHECK(oracles_achievements_list_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && oracles_achievements_detail_open(&nav));
    detail_frame("40f", &nav);
}

/* The rarity said, absent, nil, or what would round to nothing; the tag's name; the state, with a date in the pause alone. */
static void detail_texts(void)
{
    char text[40];
    static const struct { float rarity; const char *said; } rarities[] = {
        { 12.4f, "12.4% of players" }, { 0.3f, "0.3% of players" }, { 86.0f, "86.0% of players" }, { 0.0f, "" }, { 0.04f, "" }, { -1.0f, "" }, { 100.0f, "" }, { 99.97f, "" }, { 99.9f, "99.9% of players" },
    };
    for (unsigned i = 0; i < sizeof rarities / sizeof rarities[0]; i++) {
        memset(text, 'x', sizeof text);
        oracles_achievements_rarity_text(rarities[i].rarity, text, sizeof text);
        CHECK(!strcmp(text, rarities[i].said));
    }
    const float nothing = 0.0f;
    oracles_achievements_rarity_text(nothing / nothing, text, sizeof text);
    CHECK(!text[0]);
    CHECK(!strcmp(oracles_achievements_tag_text(ORACLES_ACHIEVEMENT_TAG_MISSABLE), "Missable") && !strcmp(oracles_achievements_tag_text(ORACLES_ACHIEVEMENT_TAG_PROGRESSION), "Progression")
          && !strcmp(oracles_achievements_tag_text(ORACLES_ACHIEVEMENT_TAG_WIN), "Win") && !oracles_achievements_tag_text(ORACLES_ACHIEVEMENT_TAG_NONE) && !oracles_achievements_tag_text(9u));
    OraclesAchievementsItem item;
    memset(&item, 0, sizeof item);
    oracles_achievements_state_text(&item, text, sizeof text);
    CHECK(!strcmp(text, "Locked"));
    snprintf(item.date, sizeof item.date, "7 Nov 2026");
    oracles_achievements_state_text(&item, text, sizeof text);
    CHECK(!strcmp(text, "Locked"));
    item.unlocked = 1;
    oracles_achievements_state_text(&item, text, sizeof text);
    CHECK(!strcmp(text, "Unlocked 7 Nov 2026"));
    item.date[0] = 0;
    oracles_achievements_state_text(&item, text, sizeof text);
    CHECK(!strcmp(text, "Unlocked"));
    /* Nothing laid out for what is not there; a progress beside Locked, none once unlocked. */
    static OraclesUiAchievementDetail d;
    snprintf(item.title, sizeof item.title, "A title");
    oracles_ui_layout_achievement_detail(ORACLES_UI_LAYOUT_16_9, &item, &d);
    CHECK(!d.rarity_text[0] && d.rarity.h == 0.0f && d.tag.w == 0.0f && d.description_lines == 0u && d.bar.w == 0.0f);
    snprintf(item.progress, sizeof item.progress, "3/5");
    oracles_ui_layout_achievement_detail(ORACLES_UI_LAYOUT_16_9, &item, &d);
    CHECK(d.bar.w == 0.0f);
    item.unlocked = 0;
    for (int layout = 0; layout < 2; layout++) {
        oracles_ui_layout_achievement_detail(layout ? ORACLES_UI_LAYOUT_4_3 : ORACLES_UI_LAYOUT_16_9, &item, &d);
        CHECK(d.bar.w > 0.0f && d.bar.x > d.state.x + d.state.w && d.progress.x > d.bar.x + d.bar.w);
    }
}

/* Whatever the server wrote stays in the column or the panel: the longest it may write (255 of the widest letter, and 255 of
 * ordinary words) whole; past that, the last line cut with an ellipsis, the state in sight; a word wider than a line broken
 * between whole characters. */
static void detail_lengths(void)
{
    static const struct { OraclesUiLayout layout; float right, foot; } shapes[2] = {
        { ORACLES_UI_LAYOUT_16_9, 96.0f + 520.0f, 1004.0f }, { ORACLES_UI_LAYOUT_4_3, 1338.0f, 970.0f },
    };
    static OraclesUiAchievementDetail d;
    static OraclesAchievementsItem item;
    for (unsigned s = 0; s < 2u; s++) {
        const OraclesUiLayout layout = shapes[s].layout;
        memset(&item, 0, sizeof item);
        snprintf(item.title, sizeof item.title, "An achievement with the longest title a set may give it");
        /* One word of 255 W. */
        memset(item.description, 'W', 255);
        oracles_ui_layout_achievement_detail(layout, &item, &d);
        size_t kept = 0;
        for (unsigned i = 0; i < d.description_lines; i++) {
            kept += strlen(d.description_text[i]);
            CHECK(d.description[i].x + d.description[i].w <= shapes[s].right + 0.1f);
        }
        CHECK(kept == 255u);
        /* 255 of words: each word kept, in order, no ellipsis. */
        memset(item.description, 0, sizeof item.description);
        while (strlen(item.description) + 8u <= 255u) strcat(item.description, "chest of");
        for (size_t i = 8; i < strlen(item.description); i += 9) {
            memmove(item.description + i + 1, item.description + i, strlen(item.description + i) + 1u);
            item.description[i] = ' ';
        }
        item.description[255] = 0;
        oracles_ui_layout_achievement_detail(layout, &item, &d);
        char joined[ORACLES_ACHIEVEMENT_DESCRIPTION_LENGTH] = "";
        for (unsigned i = 0; i < d.description_lines; i++) {
            if (i) strcat(joined, " ");
            strcat(joined, d.description_text[i]);
            CHECK(d.description[i].x + d.description[i].w <= shapes[s].right + 0.1f);
        }
        CHECK(!strcmp(joined, item.description));
        CHECK(d.state.y + d.state.h <= shapes[s].foot);
        /* More than any line count holds. */
        memset(item.description, 0, sizeof item.description);
        for (size_t i = 0; i + 4u < sizeof item.description; i += 4u) memcpy(item.description + i, "WWW ", 4u);
        oracles_ui_layout_achievement_detail(layout, &item, &d);
        const char *last = d.description_text[d.description_lines - 1u];
        CHECK(strlen(last) >= 3u && !strcmp(last + strlen(last) - 3u, "\xe2\x80\xa6"));
        CHECK(d.description[d.description_lines - 1u].x + d.description[d.description_lines - 1u].w <= shapes[s].right + 0.1f);
        CHECK(d.state.y + d.state.h <= shapes[s].foot);
        CHECK(layout != ORACLES_UI_LAYOUT_4_3 || d.panel.y + d.panel.h <= shapes[s].foot);
        /* A long word of two-byte letters: no letter cut in two. */
        memset(item.description, 0, sizeof item.description);
        for (size_t i = 0; i < 400u; i += 2u) memcpy(item.description + i, "\xc3\x89", 2u);
        oracles_ui_layout_achievement_detail(layout, &item, &d);
        CHECK(d.description_lines > 1u);
        for (unsigned i = 0; i < d.description_lines; i++) CHECK(strlen(d.description_text[i]) % 2u == 0u && (unsigned char)d.description_text[i][0] == 0xc3u);
        /* A line of 255 narrow letters: no more on a line than a text draws. */
        memset(item.description, 0, sizeof item.description);
        memset(item.description, 'i', 255);
        oracles_ui_layout_achievement_detail(layout, &item, &d);
        kept = 0;
        for (unsigned i = 0; i < d.description_lines; i++) {
            kept += strlen(d.description_text[i]);
            CHECK(strlen(d.description_text[i]) <= ORACLES_UI_DETAIL_LINE_GLYPHS);
        }
        CHECK(kept == 255u);
        /* A title past its lines cut too. */
        memset(item.description, 0, sizeof item.description);
        memset(item.title, 0, sizeof item.title);
        for (size_t i = 0; i + 4u < sizeof item.title; i += 4u) memcpy(item.title + i, "WWW ", 4u);
        oracles_ui_layout_achievement_detail(layout, &item, &d);
        CHECK(d.title_lines <= ORACLES_UI_DETAIL_TITLE_LINES && d.state.y + d.state.h <= shapes[s].foot);
    }
}

/* 16:9: the column follows the highlight with no key, the page's name back on a row of settings; no panel, Enter and a click
 * opening none. */
static void detail_column(void)
{
    OraclesHomeNav nav;
    OraclesHomeHint h[ORACLES_HOME_MAX_HINTS];
    details_page(&nav, ORACLES_UI_LAYOUT_16_9, 0u);
    nav.row = ORACLES_ACHIEVEMENTS_ROW_SOUND;
    CHECK(!oracles_achievements_detail_item(&nav) && !oracles_achievements_detail_column(&nav));
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && oracles_achievements_detail_column(&nav));
    CHECK(oracles_achievements_detail_item(&nav) == &details.items[0]);
    oracles_achievements_act(&nav, ORACLES_HOME_DOWN);
    CHECK(oracles_achievements_detail_item(&nav) == &details.items[1]);
    CHECK(oracles_achievements_hints(&nav, h) == 2u && !strcmp(h[0].label, "Scroll") && !strcmp(h[1].label, "Back"));
    oracles_achievements_act(&nav, ORACLES_HOME_OK);
    CHECK(!nav.achievements.detail && !oracles_achievements_detail_open(&nav));
    oracles_achievements_click(&nav, ORACLES_ACHIEVEMENTS_ROWS + 3u, -1);
    CHECK(!nav.achievements.detail && oracles_achievements_detail_item(&nav) == &details.items[3]);
    nav.row = ORACLES_ACHIEVEMENTS_ROW_SOUND;
    CHECK(!oracles_achievements_detail_column(&nav));
    /* The list gone: nothing to show; another screen: nothing either. */
    nav.row = ORACLES_ACHIEVEMENTS_ROWS;
    nav.achievements.list_state = ORACLES_ACHIEVEMENTS_LIST_NONE;
    CHECK(!oracles_achievements_detail_column(&nav));
    nav.achievements.list_state = ORACLES_ACHIEVEMENTS_LIST_READY;
    nav.screen = ORACLES_SCREEN_DISPLAY;
    CHECK(!oracles_achievements_detail_column(&nav));
    /* The pause's, on an empty list: none. */
    details_list(&nav, ORACLES_UI_LAYOUT_16_9, 0u);
    details.count = 0;
    CHECK(!oracles_achievements_detail_column(&nav));
    CHECK(oracles_achievements_list_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && !nav.achievements.detail);
}

/* 4:3: Enter opens on the highlight, up and down go on without closing and stop at the list's ends, Back closes on the same
 * row, then leaves; nothing but those keys does a thing; on the page and on the pause's list. */
static void detail_panel(void)
{
    OraclesHomeNav nav;
    details_page(&nav, ORACLES_UI_LAYOUT_4_3, 1u);
    const unsigned first = ORACLES_ACHIEVEMENTS_ROWS, row = first + 1u;
    CHECK(!oracles_achievements_detail_open(&nav));
    nav.row = ORACLES_ACHIEVEMENTS_ROW_SOUND;
    const int sound = nav.achievements.sound;
    oracles_achievements_act(&nav, ORACLES_HOME_OK);
    CHECK(!oracles_achievements_detail_open(&nav) && nav.achievements.sound != sound);   /* a row of settings acts as ever */
    nav.row = row;
    OraclesHomeHint own[ORACLES_HOME_MAX_HINTS];
    CHECK(oracles_achievements_hints(&nav, own) == 3u && !strcmp(own[1].label, "Details"));
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && oracles_achievements_detail_open(&nav) && nav.row == row);
    CHECK(oracles_achievements_hints(&nav, own) == 2u && !strcmp(own[0].label, "Next") && !strcmp(own[1].label, "Close") && own[1].back);
    oracles_achievements_act(&nav, ORACLES_HOME_DOWN);
    CHECK(oracles_achievements_detail_open(&nav) && nav.row == row + 1u && oracles_achievements_detail_item(&nav) == &details.items[2]);
    oracles_achievements_act(&nav, ORACLES_HOME_UP);
    oracles_achievements_act(&nav, ORACLES_HOME_UP);
    oracles_achievements_act(&nav, ORACLES_HOME_UP);
    oracles_achievements_act(&nav, ORACLES_HOME_UP);
    CHECK(oracles_achievements_detail_open(&nav) && nav.row == first);   /* never out on a row of settings */
    nav.row = first + 7u;
    oracles_achievements_act(&nav, ORACLES_HOME_DOWN);
    CHECK(oracles_achievements_detail_open(&nav) && nav.row == first + 7u);   /* the list's end: no way round */
    oracles_achievements_act(&nav, ORACLES_HOME_LEFT);
    oracles_achievements_act(&nav, ORACLES_HOME_RIGHT);
    oracles_achievements_act(&nav, ORACLES_HOME_OK);
    CHECK(oracles_achievements_detail_open(&nav) && nav.row == first + 7u);
    /* The wheel: the next one, at the ends the same. */
    oracles_achievements_page_scroll(&nav, -40);
    CHECK(oracles_achievements_detail_open(&nav) && nav.row == first);
    oracles_achievements_page_scroll(&nav, 2);
    CHECK(nav.row == first + 2u);
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && !oracles_achievements_detail_open(&nav));
    CHECK(nav.screen == ORACLES_SCREEN_ACHIEVEMENTS && nav.row == first + 2u);
    /* A click on a row opens it on that row, a second Back leaves, and the page opens closed. */
    oracles_achievements_click(&nav, first + 4u, -1);
    CHECK(oracles_achievements_detail_open(&nav) && nav.row == first + 4u);
    oracles_achievements_act(&nav, ORACLES_HOME_BACK);
    CHECK(nav.screen == ORACLES_SCREEN_ACHIEVEMENTS);
    oracles_achievements_act(&nav, ORACLES_HOME_BACK);
    CHECK(nav.screen == ORACLES_SCREEN_HOME && !nav.achievements.detail);
    /* The window turned 16:9 with the panel open: no panel, the keys the page's; opened again closed. */
    details_page(&nav, ORACLES_UI_LAYOUT_4_3, 2u);
    oracles_achievements_act(&nav, ORACLES_HOME_OK);
    nav.layout = ORACLES_UI_LAYOUT_16_9;
    CHECK(!oracles_achievements_detail_open(&nav) && oracles_achievements_detail_column(&nav));
    oracles_achievements_act(&nav, ORACLES_HOME_BACK);
    CHECK(nav.screen == ORACLES_SCREEN_HOME);
    nav.achievements.detail = 1;
    oracles_achievements_open(&nav);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    nav.row = first;
    CHECK(!oracles_achievements_detail_open(&nav));

    /* Round by 16:9 and back, a key pressed between: the panel never opens by itself, nor on the next achievement. */
    details_page(&nav, ORACLES_UI_LAYOUT_4_3, 0u);
    oracles_achievements_act(&nav, ORACLES_HOME_OK);
    nav.layout = ORACLES_UI_LAYOUT_16_9;
    oracles_achievements_act(&nav, ORACLES_HOME_UP);
    CHECK(nav.row == first - 1u && !nav.achievements.detail);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    oracles_achievements_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == first && !oracles_achievements_detail_open(&nav));
    /* With no key: closed for good as the owner of the layout follows it. */
    oracles_achievements_act(&nav, ORACLES_HOME_OK);
    CHECK(oracles_achievements_detail_open(&nav));
    oracles_achievements_detail_follow(&nav);
    CHECK(oracles_achievements_detail_open(&nav));   /* still shown: it stays */
    nav.layout = ORACLES_UI_LAYOUT_16_9;
    oracles_achievements_detail_follow(&nav);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    CHECK(!oracles_achievements_detail_open(&nav));
    /* The list gone. */
    oracles_achievements_act(&nav, ORACLES_HOME_OK);
    nav.achievements.list_state = ORACLES_ACHIEVEMENTS_LIST_NONE;
    oracles_achievements_detail_follow(&nav);
    nav.achievements.list_state = ORACLES_ACHIEVEMENTS_LIST_READY;
    CHECK(!oracles_achievements_detail_open(&nav));
    details_list(&nav, ORACLES_UI_LAYOUT_4_3, 2u);
    oracles_achievements_list_act(&nav, ORACLES_HOME_OK);
    nav.layout = ORACLES_UI_LAYOUT_16_9;
    oracles_achievements_list_act(&nav, ORACLES_HOME_DOWN);
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    CHECK(nav.achievements.list_focus == 3u && !oracles_achievements_detail_open(&nav));

    /* The pause's list. */
    details_list(&nav, ORACLES_UI_LAYOUT_4_3, 6u);
    CHECK(!oracles_achievements_detail_open(&nav));
    oracles_achievements_list_act(&nav, ORACLES_HOME_OK);
    CHECK(oracles_achievements_detail_open(&nav) && nav.achievements.list_focus == 6u);
    oracles_achievements_list_act(&nav, ORACLES_HOME_DOWN);
    oracles_achievements_list_act(&nav, ORACLES_HOME_DOWN);
    CHECK(oracles_achievements_detail_open(&nav) && nav.achievements.list_focus == 7u);
    CHECK(oracles_achievements_list_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && !oracles_achievements_detail_open(&nav));
    CHECK(nav.screen == ORACLES_SCREEN_ACHIEVEMENTS_LIST && nav.achievements.list_focus == 7u);
    oracles_achievements_list_click(&nav, 3u);
    CHECK(oracles_achievements_detail_open(&nav) && nav.achievements.list_focus == 3u);
    oracles_achievements_list_click(&nav, 200u);
    CHECK(nav.achievements.list_focus == 3u);
    oracles_achievements_list_act(&nav, ORACLES_HOME_BACK);
    oracles_achievements_list_act(&nav, ORACLES_HOME_BACK);
    CHECK(nav.screen == ORACLES_SCREEN_PAUSE && !nav.achievements.detail);
    oracles_achievements_list_open(&nav);
    CHECK(!oracles_achievements_detail_open(&nav));
    /* 16:9: a click highlights, Enter does nothing, no panel. */
    details_list(&nav, ORACLES_UI_LAYOUT_16_9, 0u);
    oracles_achievements_list_click(&nav, 3u);
    CHECK(nav.achievements.list_focus == 3u && !nav.achievements.detail);
    oracles_achievements_list_act(&nav, ORACLES_HOME_OK);
    CHECK(!nav.achievements.detail);
}

/* The pointer on the pause's list finds what the layout puts there, row by row, gaps and the scroll bar's strip included, scrolled
 * or not; and it measures no text: twenty looks on long descriptions cost less than one layout. */
static void list_hit_geometry(void)
{
    static OraclesAchievementsList longest;
    memset(&longest, 0, sizeof longest);
    longest.count = 40;
    for (unsigned i = 0; i < longest.count; i++) {
        OraclesAchievementsItem *item = &longest.items[i];
        snprintf(item->title, sizeof item->title, "An achievement");
        for (size_t c = 0; c + 10u < 240u; c += 10u) memcpy(item->description + c, "wide chest", 10u);
    }
    static const OraclesUiLayout layouts[2] = { ORACLES_UI_LAYOUT_16_9, ORACLES_UI_LAYOUT_4_3 };
    static OraclesUiAchievementsLayout l;
    for (unsigned s = 0; s < 2u; s++)
        for (unsigned focus = 0; focus < longest.count; focus += 13u) {
            OraclesHomeNav nav;
            details_list(&nav, layouts[s], focus);
            nav.achievements.list = &longest;
            oracles_ui_layout_achievements_list(layouts[s], &nav, &l);
            CHECK(oracles_achievements_list_first(&nav) == l.first);
            for (float y = l.list.y - 8.0f; y < l.list.y + l.list.h + 8.0f; y += 7.0f)
                for (float x = l.list.x - 8.0f; x < l.list.x + l.list.w + 8.0f; x += 83.0f) {
                    int want = -1;
                    const OraclesUiBox *b = &l.list;
                    if (x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h)
                        for (unsigned i = 0; i < l.shown; i++) {
                            const OraclesUiBox *r = &l.rows[i].row;
                            if (x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h) want = (int)l.rows[i].index;
                        }
                    CHECK(oracles_ui_achievements_list_hit(&nav, x, y) == want);
                }
            /* The scroll bar's strip takes no row. */
            CHECK(oracles_ui_achievements_list_hit(&nav, l.list.x + l.list.w - 2.0f, l.rows[0].row.y + 8.0f) == -1);
            const clock_t laid = clock();
            oracles_ui_layout_achievements_list(layouts[s], &nav, &l);
            const clock_t looked = clock();
            for (unsigned i = 0; i < 20u; i++) (void)oracles_ui_achievements_list_hit(&nav, l.rows[0].row.x + 8.0f, l.rows[0].row.y + 8.0f);
            const clock_t done = clock();
            CHECK(done - looked <= looked - laid);
        }
}

/* The column or the panel, by the size of the pause: a reduced 16:9 menu keeps the column while its middle of the scene shows it
 * whole (900x506), and the panel takes over below (640x360, 480x432, a 4:3 window too small for the 4:3 layout), at the bounds of
 * width and height; the keys and hints then the 4:3's; the panel within what shows, and above the hints, however long its text. */
static void detail_sizes(void)
{
    static const struct { int width, height, panel; } sizes[] = {
        { 1920, 1080, 0 }, { 1280, 720, 0 }, { 960, 540, 0 }, { 1000, 300, 0 }, { 900, 506, 0 }, { 864, 500, 0 },
        { 863, 500, 1 }, { 864, 499, 1 }, { 800, 450, 1 }, { 640, 360, 1 }, { 480, 432, 1 }, { 320, 288, 1 },
        { 1440, 1080, 1 }, { 640, 480, 1 },
    };
    OraclesHomeNav nav;
    OraclesHomeHint own[ORACLES_HOME_MAX_HINTS];
    static OraclesUiAchievementDetail d;
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        const OraclesUiLayout layout = oracles_ui_layout_pause(sizes[i].width, sizes[i].height, 0);
        details_list(&nav, layout, 6u);
        oracles_achievements_detail_view(&nav, sizes[i].width, sizes[i].height);
        CHECK(oracles_achievements_detail_panel(&nav) == sizes[i].panel && oracles_achievements_detail_column(&nav) == !sizes[i].panel);
        CHECK(oracles_achievements_hints(&nav, own) == (sizes[i].panel ? 3u : 2u));
        oracles_achievements_list_act(&nav, ORACLES_HOME_OK);
        CHECK(oracles_achievements_detail_open(&nav) == sizes[i].panel);
        if (sizes[i].panel) {
            CHECK(oracles_achievements_hints(&nav, own) == 2u && !strcmp(own[0].label, "Next") && !strcmp(own[1].label, "Close"));
            oracles_achievements_list_act(&nav, ORACLES_HOME_DOWN);
            CHECK(oracles_achievements_detail_open(&nav) && nav.achievements.list_focus == 7u);
            CHECK(oracles_achievements_list_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_ACHIEVEMENTS_LIST);
            oracles_achievements_list_click(&nav, 1u);
            CHECK(oracles_achievements_detail_open(&nav));
        } else {
            oracles_achievements_list_click(&nav, 1u);
            CHECK(!nav.achievements.detail);
        }
        if (!nav.achievements.view_w) continue;
        /* What shows: centred, the hints at its foot. */
        const float view_w = nav.achievements.view_w, view_h = nav.achievements.view_h, left = (1920.0f - view_w) * 0.5f, top = (1080.0f - view_h) * 0.5f;
        CHECK(view_w == (float)sizes[i].width * 2.0f && view_h == (float)sizes[i].height * 2.0f);
        const float hints_top = top + view_h - (1080.0f - layout_reference("40a", "hint 0", "y"));
        static OraclesAchievementsItem item;
        for (unsigned kind = 0; kind < 3u; kind++) {
            memset(&item, 0, sizeof item);
            snprintf(item.title, sizeof item.title, "%s", kind ? "An achievement with the longest title a set may give it" : "Short");
            memset(item.description, kind == 2u ? 'W' : 'a', kind ? 255u : 20u);
            if (kind == 1u) for (size_t c = 7; c < 255u; c += 8u) item.description[c] = ' ';
            item.points = 100;
            item.rarity = 99.9f;
            item.tag = ORACLES_ACHIEVEMENT_TAG_PROGRESSION;
            snprintf(item.progress, sizeof item.progress, "312/500");
            nav.achievements.list = &details;
            oracles_ui_layout_achievement_panel(view_w, view_h, &item, &d);
            CHECK(d.panel.x >= left && d.panel.x + d.panel.w <= left + view_w && d.panel.y >= top && d.panel.y + d.panel.h <= hints_top);
            const float right = d.panel.x + d.panel.w, foot = d.panel.y + d.panel.h;
            CHECK(d.badge.x >= d.panel.x && d.badge.y >= d.panel.y && d.state.y + d.state.h <= foot);
            CHECK(d.points.x + d.points.w <= right && d.rarity.x + d.rarity.w <= right && d.tag.x + d.tag.w <= right && d.tag.y + d.tag.h <= d.description[0].y);
            for (unsigned line = 0; line < d.description_lines; line++) CHECK(d.description[line].x + d.description[line].w <= right);
            for (unsigned line = 0; line < d.title_lines; line++) CHECK(d.title[line].x + d.title[line].w <= right);
        }
    }
}

/* The touch: a tap opens, a drag scrolls and opens nothing, wherever it ends; the panel holds the list still under a drag, whose
 * release closes nothing either; a tap in the panel does nothing, outside it closes. */
static void detail_touch(void)
{
    static const float pitch = 96.0f;
    OraclesHomeNav nav;
    OraclesAchievementsDrag drag;
    details_page(&nav, ORACLES_UI_LAYOUT_4_3, 0u);
    const unsigned first = ORACLES_ACHIEVEMENTS_ROWS;
    /* A press that stays is a tap. */
    oracles_achievements_drag_start(&drag);
    oracles_achievements_page_scroll(&nav, oracles_achievements_drag_steps(&nav, &drag, 1.0f, 2.0f, pitch));
    CHECK(nav.row == first && !oracles_achievements_drag_end(&drag));
    /* A short drag, not a row's worth: no row moved, yet no tap. */
    oracles_achievements_drag_start(&drag);
    oracles_achievements_page_scroll(&nav, oracles_achievements_drag_steps(&nav, &drag, 0.0f, -pitch * 0.3f, pitch));
    CHECK(nav.row == first && oracles_achievements_drag_end(&drag));
    /* A drag of two rows: past the rows, then an achievement, the highlight on the first in sight. */
    oracles_achievements_drag_start(&drag);
    for (int i = 0; i < 21; i++) oracles_achievements_page_scroll(&nav, oracles_achievements_drag_steps(&nav, &drag, 0.0f, -pitch * 0.1f, pitch));
    CHECK(nav.row == first + 1u && oracles_achievements_drag_end(&drag) && !oracles_achievements_detail_open(&nav));
    static OraclesUiAchievementsPageLayout page;
    oracles_ui_layout_achievements_page(nav.layout, &nav, &page);
    CHECK(page.scroll > 0.0f && page.rows[0].index == 1u && page.rows[0].row.y > page.clip.y);
    /* In the panel: the drag moves nothing, and counts as one. */
    oracles_achievements_click(&nav, first + 2u, -1);
    CHECK(oracles_achievements_detail_open(&nav));
    oracles_achievements_drag_start(&drag);
    for (int i = 0; i < 20; i++) CHECK(oracles_achievements_drag_steps(&nav, &drag, 0.0f, -pitch * 0.1f, pitch) == 0);
    CHECK(nav.row == first + 2u && oracles_achievements_drag_end(&drag) && oracles_achievements_detail_open(&nav));
    /* In the panel and out of it. */
    static OraclesUiAchievementDetail d;
    oracles_ui_layout_achievement_detail(nav.layout, oracles_achievements_detail_item(&nav), &d);
    CHECK(oracles_ui_achievements_detail_hit(&nav, d.panel.x + 4.0f, d.panel.y + 4.0f));
    CHECK(oracles_ui_achievements_detail_hit(&nav, d.panel.x + d.panel.w - 4.0f, d.panel.y + d.panel.h - 4.0f));
    CHECK(!oracles_ui_achievements_detail_hit(&nav, d.panel.x - 4.0f, d.panel.y + 4.0f));
    CHECK(!oracles_ui_achievements_detail_hit(&nav, d.panel.x + 4.0f, d.panel.y + d.panel.h + 4.0f));
    /* The pause's list: a row under the point, none outside it. */
    details_list(&nav, ORACLES_UI_LAYOUT_4_3, 0u);
    static OraclesUiAchievementsLayout l;
    oracles_ui_layout_achievements_list(nav.layout, &nav, &l);
    CHECK(oracles_ui_achievements_list_hit(&nav, l.rows[2].row.x + 8.0f, l.rows[2].row.y + 8.0f) == 2);
    CHECK(oracles_ui_achievements_list_hit(&nav, 8.0f, 8.0f) == -1);
    details_list(&nav, ORACLES_UI_LAYOUT_16_9, 0u);
    oracles_ui_layout_achievements_list(nav.layout, &nav, &l);
    CHECK(oracles_ui_achievements_list_hit(&nav, l.rows[3].row.x + 8.0f, l.rows[3].row.y + 8.0f) == 3);
    CHECK(oracles_ui_achievements_list_hit(&nav, 8.0f, 8.0f) == -1);
}

#ifdef ORACLES_RETROACHIEVEMENTS
/* The hints as the home screen gives them: 4:3's list with Details, then the panel's, with a controller; both devices' keys
 * after a touch on Android; 16:9's list as it was. */
static void detail_hints(void)
{
    OraclesHomeNav nav;
    OraclesHomeHint h[ORACLES_HOME_MAX_HINTS];
    OraclesUiHintLayout hl[ORACLES_HOME_MAX_HINTS];
    details_page(&nav, ORACLES_UI_LAYOUT_4_3, 1u);
    nav.android = 1;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    unsigned count = oracles_home_hints(&nav, h);
    CHECK(count == 3u && !strcmp(h[1].key, "A") && !strcmp(h[1].label, "Details") && !strcmp(h[2].key, "B") && !strcmp(h[2].label, "Back"));
    oracles_ui_layout_hints(ORACLES_UI_LAYOUT_4_3, h, count, hl);
    layout_box("40c list", "hint 0", &hl[0].box);
    layout_box("40c list", "hint 1", &hl[1].box);
    layout_box("40c list", "hint 2", &hl[2].box);
    oracles_achievements_act(&nav, ORACLES_HOME_OK);
    count = oracles_home_hints(&nav, h);
    CHECK(count == 2u && !strcmp(h[0].label, "Next") && !strcmp(h[1].key, "B") && !strcmp(h[1].label, "Close") && h[1].back);
    oracles_ui_layout_hints(ORACLES_UI_LAYOUT_4_3, h, count, hl);
    layout_box("40c", "hint 0", &hl[0].box);
    layout_box("40c", "hint 1", &hl[1].box);
    details_list(&nav, ORACLES_UI_LAYOUT_4_3, 1u);
    nav.android = 1;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    oracles_achievements_list_click(&nav, 1u);
    count = oracles_home_hints(&nav, h);
    oracles_ui_layout_hints(ORACLES_UI_LAYOUT_4_3, h, count, hl);
    layout_box("40f", "hint 0", &hl[0].box);
    layout_box("40f", "hint 1", &hl[1].box);
    details_page(&nav, ORACLES_UI_LAYOUT_4_3, 6u);
    nav.android = 1;
    oracles_home_input(&nav, ORACLES_INPUT_POINTER);
    oracles_achievements_click(&nav, nav.row, -1);
    count = oracles_home_hints(&nav, h);
    CHECK(count == 2u && !strcmp(h[1].key, "Esc / B") && !strcmp(h[1].label, "Close"));
    oracles_ui_layout_hints(ORACLES_UI_LAYOUT_4_3, h, count, hl);
    layout_box("40d", "hint 0", &hl[0].box);
    layout_box("40d", "hint 1", &hl[1].box);
    details_page(&nav, ORACLES_UI_LAYOUT_16_9, 1u);
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    count = oracles_home_hints(&nav, h);
    CHECK(count == 2u && !strcmp(h[0].label, "Scroll") && !strcmp(h[1].key, "Esc") && !strcmp(h[1].label, "Back"));
    oracles_ui_layout_hints(ORACLES_UI_LAYOUT_16_9, h, count, hl);
    layout_box("40a", "hint 0", &hl[0].box);
    layout_box("40a", "hint 1", &hl[1].box);
    /* A row of settings: the page's four, the name back. */
    nav.row = ORACLES_ACHIEVEMENTS_ROW_SOUND;
    CHECK(oracles_home_hints(&nav, h) == 4u);
}
#endif

/* ---- the home page's list ---------------------------------------------------------------------------------- */

/* The page logged in, the mockup's list there, the highlight on place `row`. */
static void list_page(OraclesHomeNav *nav, OraclesUiLayout layout, unsigned row)
{
    page(nav, layout, 1, row, 0);
    nav->achievements.list = &list;
    nav->achievements.list_state = ORACLES_ACHIEVEMENTS_LIST_READY;
    nav->row = row;
}

static void page_item(const char *frame, const OraclesUiAchievementRow *row, int four_three)
{
    char name[32];
    const unsigned i = row->index;
    if (i >= 8u) return;
    snprintf(name, sizeof name, "item %u", i); layout_box(frame, name, &row->row);
    snprintf(name, sizeof name, "badge %u", i); layout_box(frame, name, &row->badge);
    snprintf(name, sizeof name, "title %u", i); layout_near(frame, name, "x", row->title.x); layout_near(frame, name, "y", row->title.y);
    snprintf(name, sizeof name, "desc %u", i); layout_near(frame, name, "x", row->description.x); layout_near(frame, name, "y", row->description.y);
    snprintf(name, sizeof name, "points %u", i); layout_near(frame, name, "x", row->points.x); layout_near(frame, name, "y", row->points.y);
    if (!four_three) return;
    const OraclesUiLine *second = row->progress.h > 0.0f ? &row->progress : &row->date;
    snprintf(name, sizeof name, "state %u", i); layout_near(frame, name, "x", second->x); layout_near(frame, name, "y", second->y);
}

/* The list under the settings, the first achievement highlighted (39f, 39f 640x480), scrolled to the last the frame draws
 * (39g, its eighth; 39g 640x480, its sixth, the page by whole achievements, the highlight's last of five); the home
 * page's unlocks without a date. */
static void page_frame(const char *frame, OraclesUiLayout layout, unsigned row)
{
    OraclesHomeNav nav;
    list_page(&nav, layout, row);
    static OraclesAchievementsList dateless;
    dateless = list;
    for (unsigned i = 0; i < dateless.count; i++) dateless.items[i].date[0] = 0;
    nav.achievements.list = &dateless;
    const int four_three = layout == ORACLES_UI_LAYOUT_4_3;
    nav.android = four_three;
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    static OraclesUiAchievementsPageLayout l;
    oracles_ui_layout_achievements_page(layout, &nav, &l);
    CHECK(l.list && l.page.below.h == 0.0f);
    rows(frame, &l.page);
    layout_line(frame, "summary line", &l.summary_text);
    CHECK(l.shown > 0);
    for (unsigned i = 0; i < l.shown; i++) page_item(frame, &l.rows[i], four_three);
    layout_near(frame, "panel", "y", four_three ? l.clip.y : l.page.panel.y);
    if (four_three) {
        layout_near(frame, "help", "y", l.page.help.y);
        layout_near(frame, "help note", "y", l.page.help_note.y);
    } else layout_box(frame, "panel", &l.page.panel);
    /* The first achievement highlighted (39f, 39f 640x480): its texts, the date's place saying "Unlocked" in 16:9; in 4:3
     * the help, its description, and the help's note, its state. */
    if (row == ORACLES_ACHIEVEMENTS_ROWS) {
        CHECK(l.rows[0].index == 0);
        if (four_three) {
            CHECK(!strcmp(l.help, "Leave the first forest.") && !strcmp(l.help_note, "Unlocked"));
            const OraclesUiLine *lines[2] = { &l.page.help, &l.page.help_note };
            const OraclesUiTextStyle *styles[2] = { &oracles_ui_help_4_3, &oracles_ui_row_note_4_3 };
            const char *texts[2] = { l.help, l.help_note }, *probes[2] = { "help text", "help note text" };
            for (unsigned i = 0; i < 2; i++) {
                layout_near(frame, probes[i], "x", lines[i]->x);
                layout_near(frame, probes[i], "w", oracles_ui_text_width(styles[i], texts[i]));
                layout_near(frame, probes[i], "top", lines[i]->y);
                layout_near(frame, probes[i], "h", lines[i]->h);
                layout_near(frame, probes[i], "baseline", lines[i]->baseline);
            }
        } else {
            CHECK(!strcmp(l.rows[0].date_text, "Unlocked"));
            layout_line(frame, "title 0 text", &l.rows[0].title);
            layout_line(frame, "desc 0 text", &l.rows[0].description);
            layout_line(frame, "points 0 text", &l.rows[0].points);
            layout_line(frame, "date 0 text", &l.rows[0].date);
        }
    }
    hints(frame, &nav, layout, four_three ? 3u : 2u);
    CHECK(oracles_achievements_hints(&nav, (OraclesHomeHint[ORACLES_HOME_MAX_HINTS]){ { 0 } }) == (four_three ? 3u : 2u));
}

/* Down from Unlock sound into the list, up out of it, round its ends; the list's rows read only; the help in 4:3, a click on an
 * achievement highlighting it; each state of the line while no list shows. */
static void page_list_keys(void)
{
    OraclesHomeNav nav;
    list_page(&nav, ORACLES_UI_LAYOUT_4_3, ORACLES_ACHIEVEMENTS_ROW_SOUND);
    CHECK(oracles_achievements_page_places(&nav) == ORACLES_ACHIEVEMENTS_ROWS + 55u);
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == ORACLES_ACHIEVEMENTS_ROWS);
    CHECK(oracles_achievements_page_item(&nav) == &list.items[0]);
    static OraclesUiAchievementsPageLayout l;
    oracles_ui_layout_achievements_page(nav.layout, &nav, &l);
    CHECK(!strcmp(l.help, "Leave the first forest.") && !strcmp(l.help_note, "Unlocked 21 Sep 2026"));
    /* The home page's unlocks come without a date. */
    static OraclesAchievementsList dateless;
    dateless = list;
    dateless.items[0].date[0] = 0;
    nav.achievements.list = &dateless;
    oracles_ui_layout_achievements_page(nav.layout, &nav, &l);
    CHECK(!strcmp(l.help_note, "Unlocked") && !strcmp(l.rows[0].date_text, "Unlocked"));
    nav.layout = ORACLES_UI_LAYOUT_16_9;
    oracles_ui_layout_achievements_page(nav.layout, &nav, &l);
    CHECK(!strcmp(l.rows[0].date_text, "Unlocked"));
    nav.layout = ORACLES_UI_LAYOUT_4_3;
    nav.achievements.list = &list;
    nav.row = ORACLES_ACHIEVEMENTS_ROWS + 3u;
    oracles_ui_layout_achievements_page(nav.layout, &nav, &l);
    CHECK(!strcmp(l.help_note, "Progress 3/5"));
    nav.row = ORACLES_ACHIEVEMENTS_ROWS + 5u;
    oracles_ui_layout_achievements_page(nav.layout, &nav, &l);
    CHECK(!strcmp(l.help_note, "Locked"));
    const unsigned sound = nav.achievements.sound;
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && oracles_achievements_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.achievements.sound == (int)sound);
    CHECK(oracles_achievements_detail_open(&nav) && oracles_achievements_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY);   /* OK's panel */
    nav.row = oracles_achievements_page_places(&nav) - 1u;
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == 0);
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.row == oracles_achievements_page_places(&nav) - 1u);
    CHECK(oracles_achievements_click(&nav, ORACLES_ACHIEVEMENTS_ROWS + 7u, -1) == ORACLES_HOME_STAY && nav.row == ORACLES_ACHIEVEMENTS_ROWS + 7u);
    CHECK(oracles_achievements_page_list_on(&nav));
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_ACHIEVEMENTS);   /* the click's panel */
    /* The list's rows and the rows above are never drawn outside the view. */
    nav.row = oracles_achievements_page_places(&nav) - 1u;
    oracles_ui_layout_achievements_page(nav.layout, &nav, &l);
    CHECK(l.shown > 0 && l.rows[l.shown - 1u].row.y < l.clip.y + l.clip.h && l.page.rows[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT].y < l.clip.y);
    /* Back from the list: the home menu, on the entry. */
    nav.games[ORACLES_HOME_GAME_AGES].rom = ORACLES_ROM_ORIGINAL;
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_HOME);
    /* While no list shows: what the line says, the rows alone taking the highlight. */
    list_page(&nav, ORACLES_UI_LAYOUT_16_9, ORACLES_ACHIEVEMENTS_ROW_ACCOUNT);
    nav.achievements.list = NULL;
    nav.achievements.list_state = ORACLES_ACHIEVEMENTS_LIST_NONE;
    CHECK(!oracles_achievements_page_line(&nav));   /* none wanted: nothing is on its way */
    nav.achievements.list_state = ORACLES_ACHIEVEMENTS_LIST_LOADING;
    CHECK(!strcmp(oracles_achievements_page_line(&nav), "Loading this game\xe2\x80\x99s achievements\xe2\x80\xa6"));
    nav.achievements.list_state = ORACLES_ACHIEVEMENTS_LIST_UNREACHABLE;
    CHECK(!strncmp(oracles_achievements_page_line(&nav), "retroachievements.org cannot be reached", 39));
    nav.achievements.list_state = ORACLES_ACHIEVEMENTS_LIST_FAILED;
    CHECK(!strcmp(oracles_achievements_page_line(&nav), "This game\xe2\x80\x99s achievements could not be read."));
    CHECK(oracles_achievements_page_places(&nav) == ORACLES_ACHIEVEMENTS_ROWS && !oracles_achievements_page_list(&nav));
    nav.row = ORACLES_ACHIEVEMENTS_ROWS + 2u;
    CHECK(!oracles_achievements_page_item(&nav));
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && nav.row == 1);
}

/* The row of the page's layout showing achievement `index`, NULL when none does. */
static const OraclesUiAchievementRow *page_row(const OraclesUiAchievementsPageLayout *l, unsigned index)
{
    for (unsigned i = 0; i < l->shown; i++) if (l->rows[i].index == index) return &l->rows[i];
    return NULL;
}

/* The page's highlight wholly in its view: an achievement from the rows' top to 970 (4:3: the view's), a row under the view's
 * top. */
static int page_in_sight(const OraclesHomeNav *nav, const OraclesUiAchievementsPageLayout *l, float top)
{
    const float foot = nav->layout == ORACLES_UI_LAYOUT_4_3 ? l->clip.y + l->clip.h : 970.0f;
    if (nav->row < ORACLES_ACHIEVEMENTS_ROWS) return l->page.rows[nav->row].y >= top - 0.5f;
    const OraclesUiAchievementRow *r = page_row(l, nav->row - ORACLES_ACHIEVEMENTS_ROWS);
    return r && r->row.y >= top - 0.5f && r->row.y + r->row.h <= foot + 0.5f;
}

/* The page's highlight wholly in its view's clip, where the wheel keeps it. */
static int page_in_view(const OraclesHomeNav *nav, const OraclesUiAchievementsPageLayout *l)
{
    const float top = l->clip.y, foot = l->clip.y + l->clip.h;
    if (nav->row < ORACLES_ACHIEVEMENTS_ROWS) return l->page.rows[nav->row].y >= top - 0.5f && l->page.rows[nav->row].y + l->page.rows[nav->row].h <= foot + 0.5f;
    const OraclesUiAchievementRow *r = page_row(l, nav->row - ORACLES_ACHIEVEMENTS_ROWS);
    return r && r->row.y >= top - 0.5f && r->row.y + r->row.h <= foot + 0.5f;
}

/* The pointer as the home screen gives it: a motion (hover) then a release, or a press, a drag and a release. */
static OraclesHomeCommand tap(OraclesHomeNav *nav, float x, float y)
{
    int option;
    const int hovered = oracles_ui_achievements_page_hit(nav, x, y, &option);
    if (hovered >= 0) oracles_achievements_page_hover(nav, (unsigned)hovered);
    const int row = oracles_ui_achievements_page_hit(nav, x, y, &option);
    return row >= 0 ? oracles_achievements_click(nav, (unsigned)row, option) : ORACLES_HOME_STAY;
}

/* A tap on an achievement of the scrolled list never lands on what the page brought under the finger: no Log out, no Off,
 * no guide; the finger's first motion, which SDL sends without a button, moves nothing.  A drag scrolls, stopping at the
 * ends, and its release clicks nothing wherever it ends; a press that stays is a tap. */
static void page_pointer(void)
{
    static const struct { OraclesUiLayout layout; unsigned item; float x, y; } taps[] = {
        { ORACLES_UI_LAYOUT_4_3, 2, 930.0f, 258.0f }, { ORACLES_UI_LAYOUT_16_9, 9, 1745.0f, 124.0f },
        { ORACLES_UI_LAYOUT_16_9, 8, 1700.0f, 224.0f }, { ORACLES_UI_LAYOUT_16_9, 7, 1771.0f, 310.0f },
    };
    OraclesHomeNav nav;
    for (unsigned i = 0; i < sizeof taps / sizeof taps[0]; i++) {
        list_page(&nav, taps[i].layout, ORACLES_ACHIEVEMENTS_ROWS + taps[i].item);
        static OraclesUiAchievementsPageLayout before;
        oracles_ui_layout_achievements_page(nav.layout, &nav, &before);
        int option;
        const int under = oracles_ui_achievements_page_hit(&nav, taps[i].x, taps[i].y, &option);
        const OraclesHomeCommand c = tap(&nav, taps[i].x, taps[i].y);
        /* What was under the finger is what took the tap, an achievement here. */
        CHECK(under >= ORACLES_ACHIEVEMENTS_ROWS && nav.row == (unsigned)under);
        CHECK(c == ORACLES_HOME_STAY && nav.achievements.on && nav.achievements.user[0]);
    }
    /* A real tap on a row in sight still acts. */
    list_page(&nav, ORACLES_UI_LAYOUT_16_9, ORACLES_ACHIEVEMENTS_ROW_ACCOUNT);
    static OraclesUiAchievementsPageLayout l;
    oracles_ui_layout_achievements_page(nav.layout, &nav, &l);
    const OraclesUiBox *logout = &l.page.action_boxes[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT];
    CHECK(tap(&nav, logout->x + 4.0f, logout->y + 4.0f) == ORACLES_HOME_ACHIEVEMENTS_LOGOUT);
    /* A drag back to the top, its release over Log out: nothing. */
    list_page(&nav, ORACLES_UI_LAYOUT_16_9, ORACLES_ACHIEVEMENTS_ROWS + 3u);
    OraclesAchievementsDrag drag;
    oracles_achievements_drag_start(&drag);
    for (unsigned i = 0; i < 60; i++) oracles_achievements_page_scroll(&nav, oracles_achievements_drag_move(&drag, 0.0f, 12.0f, 72.0f));
    oracles_ui_layout_achievements_page(nav.layout, &nav, &l);
    CHECK(l.scroll == 0.0f && nav.row == ORACLES_ACHIEVEMENTS_ROWS + 3u);   /* stopping there, not wrapping round, the highlight in sight */
    CHECK(oracles_achievements_drag_end(&drag));
    oracles_achievements_drag_start(&drag);
    for (unsigned i = 0; i < 400; i++) oracles_achievements_page_scroll(&nav, oracles_achievements_drag_move(&drag, 0.0f, -12.0f, 72.0f));
    oracles_ui_layout_achievements_page(nav.layout, &nav, &l);
    CHECK(l.shown > 0 && l.rows[l.shown - 1u].index == list.count - 1u && fabsf(l.rows[l.shown - 1u].row.y + l.rows[l.shown - 1u].row.h - 970.0f) < 0.5f);
    CHECK(nav.row >= ORACLES_ACHIEVEMENTS_ROWS && page_row(&l, nav.row - ORACLES_ACHIEVEMENTS_ROWS) && oracles_achievements_drag_end(&drag));
    /* A tap trembling a little stays a tap; the flag does not outlive its press. */
    oracles_achievements_drag_start(&drag);
    CHECK(oracles_achievements_drag_move(&drag, 2.0f, -3.0f, 72.0f) == 0 && !oracles_achievements_drag_end(&drag));
    /* Sideways counts as much. */
    oracles_achievements_drag_start(&drag);
    oracles_achievements_drag_move(&drag, 30.0f, 0.0f, 72.0f);
    CHECK(oracles_achievements_drag_end(&drag) && !oracles_achievements_drag_end(&drag));
    /* The wheel: the same ends. */
    list_page(&nav, ORACLES_UI_LAYOUT_16_9, 0);
    oracles_achievements_page_scroll(&nav, -1);
    CHECK(nav.row == 0);
    /* Without the list the rows follow the pointer, as every page's. */
    page(&nav, ORACLES_UI_LAYOUT_16_9, 0, ORACLES_ACHIEVEMENTS_ROW_ACCOUNT, 0);
    oracles_achievements_page_hover(&nav, ORACLES_ACHIEVEMENTS_ROW_PRIVACY);
    CHECK(nav.row == ORACLES_ACHIEVEMENTS_ROW_PRIVACY);
}

/* The mouse resting over the list after the wheel: small moves leave the highlight and the page where they are, in both
 * layouts, wherever it rests. */
static void page_hover_stays(void)
{
    static const struct { OraclesUiLayout layout; float x, y; } rests[] = { { ORACLES_UI_LAYOUT_16_9, 900.0f, 300.0f }, { ORACLES_UI_LAYOUT_4_3, 700.0f, 400.0f } };
    for (unsigned r = 0; r < 2; r++) {
        OraclesHomeNav nav;
        list_page(&nav, rests[r].layout, ORACLES_ACHIEVEMENTS_ROWS + 30u);
        static OraclesUiAchievementsPageLayout first, now;
        oracles_ui_layout_achievements_page(nav.layout, &nav, &first);
        CHECK(first.scroll > 0.0f);
        for (unsigned i = 0; i < 12; i++) {
            int option;
            const int row = oracles_ui_achievements_page_hit(&nav, rests[r].x + (float)(i % 3u), rests[r].y + (float)(i % 2u), &option);
            if (row >= 0) oracles_achievements_page_hover(&nav, (unsigned)row);
            oracles_ui_layout_achievements_page(nav.layout, &nav, &now);
            CHECK(now.scroll == first.scroll && nav.row == ORACLES_ACHIEVEMENTS_ROWS + 30u);
        }
    }
}

/* Each achievement showing on the page, whole or cut by the view, tapped where it shows: highlighted, the page still (4:3:
 * its panel opened, then closed); `taps` counts them. */
static void page_taps_still(const OraclesHomeNav *nav, unsigned *taps)
{
    static OraclesUiAchievementsPageLayout before, after;
    oracles_ui_layout_achievements_page(nav->layout, nav, &before);
    for (unsigned i = 0; i < before.shown; i++) {
        const OraclesUiAchievementRow *r = &before.rows[i];
        const float from = fmaxf(r->row.y, before.clip.y), to = fminf(r->row.y + r->row.h, before.clip.y + before.clip.h);
        if (to - from < 8.0f) continue;
        static OraclesHomeNav t;
        t = *nav;
        tap(&t, r->row.x + r->row.w * 0.5f, (from + to) * 0.5f);
        CHECK(t.row == ORACLES_ACHIEVEMENTS_ROWS + r->index && oracles_achievements_detail_open(&t) == (nav->layout == ORACLES_UI_LAYOUT_4_3));
        if (oracles_achievements_detail_open(&t)) oracles_achievements_act(&t, ORACLES_HOME_BACK);
        oracles_ui_layout_achievements_page(t.layout, &t, &after);
        const OraclesUiAchievementRow *same = page_row(&after, r->index);
        CHECK(after.scroll == before.scroll && same && same->row.y == r->row.y);
        (*taps)++;
    }
}

/* The page's list scrolls apart from its highlight, in both layouts: a click highlights the achievement under it where it
 * is, up and down scroll only when the highlight would leave the view, a whole achievement then, the wheel and a drag move
 * the view and keep the highlight in it; the view stays within the page's ends when the window changes layout or the list
 * gets shorter. */
static void page_scroll_apart(void)
{
    static OraclesUiAchievementsPageLayout l, k;
    unsigned taps = 0, kept = 0;
    for (unsigned s = 0; s < 2u; s++) {
        const OraclesUiLayout layout = s ? ORACLES_UI_LAYOUT_4_3 : ORACLES_UI_LAYOUT_16_9;
        OraclesHomeNav nav;
        list_page(&nav, layout, ORACLES_ACHIEVEMENTS_ROW_SOUND);
        oracles_ui_layout_achievements_page(layout, &nav, &l);
        const float top = s ? l.clip.y + 1.0f : l.page.rows[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT].y;
        CHECK(l.scroll == 0.0f && l.shown >= 2u);
        /* As it opens: the achievements under the rows, the one cut by the view's foot too. */
        const OraclesUiAchievementRow *cut = &l.rows[l.shown - 1u];
        CHECK(cut->row.y + cut->row.h > l.clip.y + l.clip.h);
        page_taps_still(&nav, &taps);
        /* That one tapped, a wheel up that scrolls nothing leaves it highlighted. */
        {
            static OraclesHomeNav t;
            t = nav;
            const float from = cut->row.y, to = l.clip.y + l.clip.h;
            tap(&t, cut->row.x + cut->row.w * 0.5f, (from + to) * 0.5f);
            if (oracles_achievements_detail_open(&t)) oracles_achievements_act(&t, ORACLES_HOME_BACK);
            CHECK(t.row == ORACLES_ACHIEVEMENTS_ROWS + cut->index);
            oracles_achievements_page_scroll(&t, -1);
            oracles_ui_layout_achievements_page(layout, &t, &k);
            CHECK(k.scroll == 0.0f && t.row == ORACLES_ACHIEVEMENTS_ROWS + cut->index);
        }
        const float pitch = l.rows[1].row.y - l.rows[0].row.y;
        /* Down into the list and on: the page still until the highlight reaches the view's foot. */
        unsigned downs = 0;
        do {
            oracles_achievements_act(&nav, ORACLES_HOME_DOWN);
            oracles_ui_layout_achievements_page(layout, &nav, &l);
            CHECK(page_in_sight(&nav, &l, top));
            downs++;
        } while (l.scroll == 0.0f && downs < 40u);
        CHECK(l.scroll > 0.0f && nav.row >= ORACLES_ACHIEVEMENTS_ROWS);
        for (unsigned i = 0; i < 9u; i++) oracles_achievements_act(&nav, ORACLES_HOME_DOWN);
        oracles_ui_layout_achievements_page(layout, &nav, &l);
        CHECK(page_in_sight(&nav, &l, top));
        /* The highlight at the foot: one more down scrolls one achievement. */
        const float at_foot = l.scroll;
        oracles_achievements_act(&nav, ORACLES_HOME_DOWN);
        oracles_ui_layout_achievements_page(layout, &nav, &k);
        CHECK(fabsf(k.scroll - at_foot - pitch) < 0.5f && page_in_sight(&nav, &k, top));
        CHECK(fabsf(nav.achievements.page_scroll - k.scroll) < 0.5f);
        page_taps_still(&nav, &taps);
        /* A click on an achievement in the middle of the view: highlighted, nothing moving. */
        const OraclesUiAchievementRow *middle = &k.rows[k.shown / 2u];
        const unsigned index = middle->index, first = k.rows[0].index;
        const float y = middle->row.y;
        oracles_achievements_click(&nav, ORACLES_ACHIEVEMENTS_ROWS + index, -1);
        CHECK(nav.row == ORACLES_ACHIEVEMENTS_ROWS + index && oracles_achievements_detail_open(&nav) == (int)s);
        if (s) oracles_achievements_act(&nav, ORACLES_HOME_BACK);
        oracles_ui_layout_achievements_page(layout, &nav, &l);
        const OraclesUiAchievementRow *clicked = page_row(&l, index);
        CHECK(l.scroll == k.scroll && l.rows[0].index == first && clicked && clicked->row.y == y);
        /* Up and down from there: the page still while the highlight is in sight. */
        oracles_achievements_act(&nav, ORACLES_HOME_UP);
        oracles_achievements_act(&nav, ORACLES_HOME_DOWN);
        oracles_achievements_act(&nav, ORACLES_HOME_DOWN);
        oracles_ui_layout_achievements_page(layout, &nav, &l);
        CHECK(l.scroll == k.scroll && nav.row == ORACLES_ACHIEVEMENTS_ROWS + index + 1u);
        /* Up to the view's top, then one more: one achievement up, the highlight at the top. */
        unsigned whole = 0;
        while (whole < k.shown && k.rows[whole].row.y < top - 0.5f) whole++;
        CHECK(whole < k.shown);
        nav.row = ORACLES_ACHIEVEMENTS_ROWS + k.rows[whole].index;
        oracles_ui_layout_achievements_page(layout, &nav, &l);
        CHECK(l.scroll == k.scroll);
        const unsigned at_top = nav.row - ORACLES_ACHIEVEMENTS_ROWS;
        oracles_achievements_act(&nav, ORACLES_HOME_UP);
        oracles_ui_layout_achievements_page(layout, &nav, &l);
        const OraclesUiAchievementRow *up = page_row(&l, at_top - 1u);
        CHECK(up && fabsf(up->row.y - top) < 0.5f && l.scroll < k.scroll);
        page_taps_still(&nav, &taps);
        /* The wheel: an achievement a step, the highlight where it was while in sight. */
        const float before = l.scroll;
        const unsigned row = nav.row;
        oracles_achievements_page_scroll(&nav, -1);
        oracles_ui_layout_achievements_page(layout, &nav, &k);
        CHECK(fabsf(before - k.scroll - pitch) < 0.5f && nav.row == row);
        oracles_achievements_page_scroll(&nav, 1);
        oracles_ui_layout_achievements_page(layout, &nav, &l);
        CHECK(fabsf(l.scroll - before) < 0.5f && nav.row == row);
        /* Its row gone over the top: the highlight on the first achievement in sight. */
        oracles_achievements_page_scroll(&nav, 1);
        oracles_ui_layout_achievements_page(layout, &nav, &l);
        CHECK(fabsf(l.scroll - before - pitch) < 0.5f && nav.row == row + 1u && page_in_view(&nav, &l));
        oracles_achievements_page_scroll(&nav, -1);
        /* Up to the top: the highlight on the last place in sight, the next down going on from it with no jump. */
        oracles_achievements_page_scroll(&nav, -100);
        oracles_ui_layout_achievements_page(layout, &nav, &l);
        CHECK(l.scroll == 0.0f && nav.row < row && page_in_view(&nav, &l));
        CHECK(nav.row + 1u >= oracles_achievements_page_places(&nav) || !page_in_view(&(OraclesHomeNav){ .row = nav.row + 1u, .layout = layout }, &l));
        oracles_achievements_act(&nav, ORACLES_HOME_DOWN);
        oracles_ui_layout_achievements_page(layout, &nav, &k);
        CHECK(k.scroll > 0.0f && (s || k.scroll <= 2.0f * pitch) && page_in_sight(&nav, &k, top));
        /* Down to the end, past it the same. */
        oracles_achievements_page_scroll(&nav, 100);
        oracles_ui_layout_achievements_page(layout, &nav, &l);
        const float end = l.scroll;
        const OraclesUiAchievementRow *last = page_row(&l, list.count - 1u);
        CHECK(last && page_in_view(&nav, &l) && nav.row >= ORACLES_ACHIEVEMENTS_ROWS);
        oracles_achievements_page_scroll(&nav, 1);
        oracles_ui_layout_achievements_page(layout, &nav, &l);
        CHECK(l.scroll == end);
        /* The window turned to the other layout: the view within its ends, whole achievements in 4:3, the highlight showing;
         * up and down then go on from the view shown, still while their row is in sight. */
        nav.layout = s ? ORACLES_UI_LAYOUT_16_9 : ORACLES_UI_LAYOUT_4_3;
        oracles_ui_layout_achievements_page(nav.layout, &nav, &k);
        const float other_top = s ? k.page.rows[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT].y : k.clip.y + 1.0f;
        CHECK(page_row(&k, nav.row - ORACLES_ACHIEVEMENTS_ROWS) != NULL);
        if (!s) CHECK(fabsf(k.rows[0].row.y - other_top) < 0.5f || k.scroll == 0.0f);
        for (int step = -1; step <= 1; step += 2) {
            static OraclesHomeNav moved;
            moved = nav;
            const unsigned to = (unsigned)((int)nav.row + step);
            if (!page_in_sight(&(OraclesHomeNav){ .row = to, .layout = nav.layout }, &k, other_top)) continue;
            oracles_achievements_act(&moved, step < 0 ? ORACLES_HOME_UP : ORACLES_HOME_DOWN);
            oracles_ui_layout_achievements_page(moved.layout, &moved, &l);
            CHECK(moved.row == to && l.scroll == k.scroll);
            kept++;
        }
        nav.layout = layout;
        /* A shorter list: the view no lower than its end, its last achievement at the foot. */
        static OraclesAchievementsList shorter;
        shorter = list;
        shorter.count = 20u;
        nav.achievements.list = &shorter;
        nav.row = ORACLES_ACHIEVEMENTS_ROWS + 19u;
        oracles_ui_layout_achievements_page(layout, &nav, &l);
        last = page_row(&l, 19u);
        CHECK(l.scroll < end && last && page_in_sight(&nav, &l, top));
        oracles_achievements_page_scroll(&nav, 1);
        oracles_ui_layout_achievements_page(layout, &nav, &k);
        CHECK(k.scroll == l.scroll);
    }
    CHECK(taps >= 30u && kept >= 1u);
}

/* Each achievement showing on the pause's list, whole or cut by its foot, clicked where the pointer finds it: highlighted, the
 * list still (4:3: its panel opened, then closed); `taps` counts them. */
static void list_taps_still(const OraclesHomeNav *nav, unsigned *taps)
{
    static OraclesUiAchievementsLayout before, after;
    oracles_ui_layout_achievements_list(nav->layout, nav, &before);
    for (unsigned i = 0; i < before.shown; i++) {
        const OraclesUiAchievementRow *r = &before.rows[i];
        const float to = fminf(r->row.y + r->row.h, before.list.y + before.list.h);
        if (to - r->row.y < 4.0f) continue;
        static OraclesHomeNav t;
        t = *nav;
        const int hit = oracles_ui_achievements_list_hit(&t, r->row.x + 8.0f, (r->row.y + to) * 0.5f);
        CHECK(hit == (int)r->index);
        if (hit < 0) continue;
        oracles_achievements_list_click(&t, (unsigned)hit);
        CHECK(t.achievements.list_focus == r->index && oracles_achievements_detail_open(&t) == (nav->layout == ORACLES_UI_LAYOUT_4_3));
        if (oracles_achievements_detail_open(&t)) oracles_achievements_list_act(&t, ORACLES_HOME_BACK);
        oracles_ui_layout_achievements_list(t.layout, &t, &after);
        CHECK(after.first == before.first && after.rows[i].index == r->index && after.rows[i].row.y == r->row.y);
        (*taps)++;
    }
}

/* The pause's list the same, by whole rows: a click keeps its first row, down at the foot scrolls a row, the wheel moves it and
 * keeps the highlight in sight, its ends hold through a change of layout and a shorter list; with the detail's panel, the
 * wheel goes to the next achievement. */
static void list_scroll_apart(void)
{
    static OraclesUiAchievementsLayout l;
    unsigned taps = 0;
    for (unsigned s = 0; s < 2u; s++) {
        const OraclesUiLayout layout = s ? ORACLES_UI_LAYOUT_4_3 : ORACLES_UI_LAYOUT_16_9;
        OraclesHomeNav nav;
        list_nav(&nav, layout, 0, ORACLES_ACHIEVEMENTS_RUNNING);
        const unsigned fit = oracles_achievements_list_fit(&nav);
        CHECK(fit >= 4u && fit < list.count);
        list_taps_still(&nav, &taps);
        /* The row cut at the foot clicked, a wheel up that scrolls nothing leaves it highlighted. */
        oracles_ui_layout_achievements_list(layout, &nav, &l);
        if (l.shown > fit) {
            oracles_achievements_list_click(&nav, fit);
            if (s) oracles_achievements_list_act(&nav, ORACLES_HOME_BACK);
            oracles_achievements_list_scroll(&nav, -1);
            CHECK(nav.achievements.list_focus == fit && oracles_achievements_list_first(&nav) == 0u);
            taps++;
            nav.achievements.list_focus = 0;
        }
        for (unsigned i = 0; i + 1u < fit; i++) oracles_achievements_list_act(&nav, ORACLES_HOME_DOWN);
        CHECK(nav.achievements.list_focus == fit - 1u && oracles_achievements_list_first(&nav) == 0u);
        oracles_achievements_list_act(&nav, ORACLES_HOME_DOWN);
        CHECK(oracles_achievements_list_first(&nav) == 1u && nav.achievements.list_first == 1u);
        for (unsigned i = 0; i < 10u; i++) oracles_achievements_list_act(&nav, ORACLES_HOME_DOWN);
        CHECK(nav.achievements.list_focus == fit + 10u && oracles_achievements_list_first(&nav) == 11u);
        list_taps_still(&nav, &taps);
        /* A click in the middle: highlighted where it is. */
        oracles_ui_layout_achievements_list(layout, &nav, &l);
        const unsigned index = l.rows[fit / 2u].index;
        const float y = l.rows[fit / 2u].row.y;
        oracles_achievements_list_click(&nav, index);
        CHECK(nav.achievements.list_focus == index && oracles_achievements_detail_open(&nav) == (int)s);
        if (s) oracles_achievements_list_act(&nav, ORACLES_HOME_BACK);
        oracles_ui_layout_achievements_list(layout, &nav, &l);
        CHECK(l.first == 11u && l.rows[fit / 2u].index == index && l.rows[fit / 2u].row.y == y);
        /* Up to the first row and past it: a row up. */
        while (nav.achievements.list_focus > 11u) oracles_achievements_list_act(&nav, ORACLES_HOME_UP);
        CHECK(oracles_achievements_list_first(&nav) == 11u);
        oracles_achievements_list_act(&nav, ORACLES_HOME_UP);
        CHECK(nav.achievements.list_focus == 10u && oracles_achievements_list_first(&nav) == 10u);
        list_taps_still(&nav, &taps);
        /* The wheel: a row a step, the highlight kept while in sight, else on the nearest row in sight. */
        oracles_achievements_list_scroll(&nav, -1);
        CHECK(oracles_achievements_list_first(&nav) == 9u && nav.achievements.list_focus == 10u);
        oracles_achievements_list_scroll(&nav, 3);
        CHECK(oracles_achievements_list_first(&nav) == 12u && nav.achievements.list_focus == 12u);
        oracles_achievements_list_scroll(&nav, -100);
        CHECK(oracles_achievements_list_first(&nav) == 0u && nav.achievements.list_focus == fit - 1u);
        oracles_achievements_list_act(&nav, ORACLES_HOME_DOWN);
        CHECK(nav.achievements.list_focus == fit && oracles_achievements_list_first(&nav) == 1u);
        oracles_achievements_list_scroll(&nav, 100);
        CHECK(oracles_achievements_list_first(&nav) == list.count - fit && nav.achievements.list_focus == list.count - fit);
        /* A drag: the same steps. */
        OraclesAchievementsDrag drag;
        oracles_achievements_drag_start(&drag);
        for (int i = 0; i < 21; i++) oracles_achievements_list_scroll(&nav, oracles_achievements_drag_steps(&nav, &drag, 0.0f, 9.6f, 96.0f));
        CHECK(oracles_achievements_drag_end(&drag) && oracles_achievements_list_first(&nav) == list.count - fit - 2u);
        CHECK(nav.achievements.list_focus == list.count - fit);
        /* The other layout: the highlight in sight, the first row within the ends. */
        nav.achievements.list_focus = list.count - 1u;
        nav.achievements.list_first = list.count - fit;
        nav.layout = s ? ORACLES_UI_LAYOUT_16_9 : ORACLES_UI_LAYOUT_4_3;
        const unsigned other = oracles_achievements_list_fit(&nav);
        CHECK(oracles_achievements_list_first(&nav) == list.count - other);
        oracles_achievements_list_act(&nav, ORACLES_HOME_UP);   /* from the list as it shows: still */
        CHECK(nav.achievements.list_focus == list.count - 2u && nav.achievements.list_first == list.count - other);
        nav.achievements.list_focus = list.count - fit;
        CHECK(oracles_achievements_list_first(&nav) + other > nav.achievements.list_focus && oracles_achievements_list_first(&nav) <= nav.achievements.list_focus);
        nav.layout = layout;
        /* A shorter list. */
        static OraclesAchievementsList shorter;
        shorter = list;
        shorter.count = fit + 3u;
        nav.achievements.list = &shorter;
        nav.achievements.list_focus = fit + 2u;
        CHECK(oracles_achievements_list_first(&nav) == 3u);
        oracles_achievements_list_scroll(&nav, 5);
        CHECK(oracles_achievements_list_first(&nav) == 3u && nav.achievements.list_first == 3u);
        nav.achievements.list = &list;
        /* The panel open: the wheel goes on to the next achievement. */
        if (s) {
            nav.achievements.list_focus = 20u;
            oracles_achievements_list_act(&nav, ORACLES_HOME_OK);
            CHECK(oracles_achievements_detail_open(&nav));
            oracles_achievements_list_scroll(&nav, 1);
            CHECK(oracles_achievements_detail_open(&nav) && nav.achievements.list_focus == 21u);
            oracles_achievements_list_scroll(&nav, -100);
            CHECK(nav.achievements.list_focus == 0u && oracles_achievements_list_first(&nav) == 0u);
        }
    }
    CHECK(taps >= 30u);
}

/* The last badges looked for after the client goes, a while, not for ever. */
static void badges_redraw(void)
{
    double until = 0.0;
    CHECK(oracles_achievements_badges_redraw(1, 1, 1000.0, &until));
    CHECK(!oracles_achievements_badges_redraw(1, 0, 1100.0, &until));
    CHECK(oracles_achievements_badges_redraw(0, 1, 2500.0, &until));   /* past the draw's two seconds */
    CHECK(!oracles_achievements_badges_redraw(0, 1, 5000.0, &until));
    CHECK(!oracles_achievements_badges_redraw(0, 1, 0.0, &(double){ 0.0 }));
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
    /* Log out: a command; Off and On store; Player's guide opens. */
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_ACHIEVEMENTS_LOGOUT);
    nav.row = ORACLES_ACHIEVEMENTS_ROW_ON;
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STORE && !nav.achievements.on);
    CHECK(oracles_achievements_click(&nav, ORACLES_ACHIEVEMENTS_ROW_ON, 1) == ORACLES_HOME_STORE && nav.achievements.on);
    CHECK(oracles_achievements_click(&nav, ORACLES_ACHIEVEMENTS_ROW_PRIVACY, 2) == ORACLES_HOME_ACHIEVEMENTS_GUIDE && nav.row == ORACLES_ACHIEVEMENTS_ROW_PRIVACY);
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_UP) == ORACLES_HOME_STAY && nav.row == ORACLES_ACHIEVEMENTS_ROW_ON);
    nav.achievements.user[0] = 0;
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
    /* Back to the home menu, on Achievements. */
    nav.games[ORACLES_HOME_GAME_AGES].rom = ORACLES_ROM_ORIGINAL;
    CHECK(oracles_achievements_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_HOME);
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    CHECK(oracles_home_items(&nav, items) == 8 && items[nav.focus].id == ORACLES_HOME_ITEM_ACHIEVEMENTS);
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
    CHECK(nav.achievements.key_row == 4 && nav.achievements.key_column == 5);   /* Done: the last row's last */
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_LEFT);
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_A);   /* the eraser */
    CHECK(!strcmp(nav.achievements.name, "P"));
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_LEFT);
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_LEFT);
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_A);   /* Space */
    oracles_achievements_login_key(&nav, ORACLES_LOGIN_PAD_SHOULDER);
    char keys[ORACLES_KEYBOARD_COLUMNS][8];
    CHECK(oracles_achievements_keyboard_keys(&nav, 1, keys) == 10 && !strcmp(keys[0], "!") && oracles_achievements_keyboard_keys(&nav, 4, keys) == 6
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
/* ---- in a build with them: the home menu's entry, the pause's entries, Cheats' note ------------------------------ */

/* Achievements above Cheats on an Oracle's original ROM, opening the page; greyed with its reason on any other ROM and in
 * a fan game's; Oracles Combined's and an unrecognised ROM's seven entries, without Cheats; nothing in a build without
 * them (`shown` 0); the list of fan games as it was. */
static void home_entry(void)
{
    OraclesHomeNav nav;
    OraclesHomeItem items[ORACLES_HOME_MAX_ITEMS];
    oracles_home_init(&nav);
    nav.games[ORACLES_HOME_GAME_AGES].usable = 1;
    nav.games[ORACLES_HOME_GAME_AGES].rom = ORACLES_ROM_ORIGINAL;
    CHECK(oracles_home_items(&nav, items) == 7 && items[5].id == ORACLES_HOME_ITEM_CHEATS);
    nav.achievements.shown = 1;
    CHECK(oracles_home_items(&nav, items) == 8 && items[5].id == ORACLES_HOME_ITEM_ACHIEVEMENTS && !items[5].disabled && !strcmp(items[5].label, "Achievements"));
    CHECK(items[6].id == ORACLES_HOME_ITEM_CHEATS && items[7].id == ORACLES_HOME_ITEM_EXIT);
    nav.focus = 5;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_ACHIEVEMENTS && nav.row == ORACLES_ACHIEVEMENTS_ROW_ACCOUNT);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_BACK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_HOME && nav.focus == 5);
    /* The Settings button after the eighth entry, the notice before it. */
    nav.focus = 7;
    snprintf(nav.update, sizeof nav.update, "1.5.1");
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && oracles_home_update_focused(&nav) && oracles_home_focus(&nav) == 8);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_DOWN) == ORACLES_HOME_STAY && oracles_home_version_focused(&nav) && oracles_home_focus(&nav) == 9);
    nav.update[0] = 0;
    nav.focus = 0;
    /* An unrecognised ROM: greyed, no Cheats. */
    nav.games[ORACLES_HOME_GAME_AGES].rom = ORACLES_ROM_UNRECOGNISED;
    CHECK(oracles_home_items(&nav, items) == 7 && items[5].disabled && items[6].id == ORACLES_HOME_ITEM_EXIT);
    CHECK(!strcmp(items[5].reason, "Needs an original ROM"));
    nav.focus = 5;
    CHECK(oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav.screen == ORACLES_SCREEN_HOME);
    /* Moonrise Regalia: greyed, then Cheats. */
    nav.entry = ORACLES_HOME_FAN;
    nav.fan_pick = ORACLES_HOME_HERO_MOONRISE;
    nav.games[ORACLES_HOME_GAME_MOONRISE].rom = ORACLES_ROM_ORIGINAL;
    CHECK(oracles_home_items(&nav, items) == 8 && items[5].disabled && items[6].id == ORACLES_HOME_ITEM_CHEATS);
    CHECK(!strcmp(items[5].reason, "Not available for this game"));
    /* Oracles Combined: greyed, its profile without cheats. */
    nav.fan_pick = ORACLES_HOME_HERO_COMBINED;
    nav.games[ORACLES_HOME_GAME_COMBINED].refuses = ORACLES_HOME_REFUSES_CHEATS;
    CHECK(oracles_home_items(&nav, items) == 7 && items[5].disabled && items[6].id == ORACLES_HOME_ITEM_EXIT);
    CHECK(!strcmp(items[5].reason, "Not available for this game"));
    /* The list of fan games has no entry of theirs. */
    nav.fan_pick = ORACLES_HOME_HERO_FAN;
    CHECK(oracles_home_items(&nav, items) == 5);
    /* The Oracle with no ROM yet: greyed too, asking for one. */
    nav.entry = ORACLES_HOME_SEASONS;
    CHECK(oracles_home_items(&nav, items) == 8 && items[5].disabled && !strcmp(items[5].reason, "Needs an original ROM"));
    /* A ROM that cannot be read: the same as an unrecognised one. */
    nav.games[ORACLES_HOME_GAME_SEASONS].rom = ORACLES_ROM_REFUSED;
    CHECK(oracles_home_items(&nav, items) == 8 && items[5].disabled && !strcmp(items[5].reason, "Needs an original ROM"));
}

static void login_hints(void)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    nav.achievements.shown = 1;
    /* The login's hints follow the device: the keyboard's, a controller's, the launcher's keyboard's. */
    nav.achievements.user[0] = 0;
    oracles_achievements_open(&nav);
    nav.row = ORACLES_ACHIEVEMENTS_ROW_ACCOUNT;
    oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
    oracles_achievements_act(&nav, ORACLES_HOME_OK);
    OraclesHomeHint h[ORACLES_HOME_MAX_HINTS];
    CHECK(oracles_home_hints(&nav, h) == 4 && !strcmp(h[0].key, "Tab") && h[0].chars == 6 && !strcmp(h[3].label, "Cancel") && h[3].back);
    oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
    CHECK(oracles_home_hints(&nav, h) == 5 && !strcmp(h[2].key, "X") && !strcmp(h[4].label, "Done"));
    nav.achievements.field = ORACLES_LOGIN_OK;
    CHECK(oracles_home_hints(&nav, h) == 4 && !strcmp(h[0].key, "D-pad") && !strcmp(h[0].label, "Move"));
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
    /* 4:3: on the line under the note. */
    oracles_ui_layout_cheats(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    const char *frame = "cheats achievements 4:3";
    layout_near(frame, "note", "y", l.note.lines[0].y);
    layout_near(frame, "warning", "y", l.note2.lines[0].y);
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
    page_16_9();
    page_4_3();
    login_16_9("login", ORACLES_LOGIN_TYPING, ORACLES_LOGIN_PASSWORD);
    login_16_9("login wrong", ORACLES_LOGIN_WRONG, ORACLES_LOGIN_PASSWORD);
    login_16_9("login logging", ORACLES_LOGIN_LOGGING, ORACLES_LOGIN_OK);
    login_android();
    login_keyboard();
    login_eye();
    login_eye_keys();
    toasts_layout();
    toasts_queue();
    sound_keys();
    sound_rendered();
    sound_mixed();
    sound_cleared();
    stand_ins();
    list_16_9();
    list_4_3();
    list_keys();
    page_frame("39f", ORACLES_UI_LAYOUT_16_9, ORACLES_ACHIEVEMENTS_ROWS);
    page_frame("39f 640x480", ORACLES_UI_LAYOUT_4_3, ORACLES_ACHIEVEMENTS_ROWS);
    page_frame("39g", ORACLES_UI_LAYOUT_16_9, ORACLES_ACHIEVEMENTS_ROWS + 7u);
    page_frame("39g 640x480", ORACLES_UI_LAYOUT_4_3, ORACLES_ACHIEVEMENTS_ROWS + 5u);
    page_list_keys();
    page_pointer();
    detail_frames();
    detail_texts();
    detail_lengths();
    detail_column();
    detail_panel();
    detail_touch();
    list_hit_geometry();
    detail_sizes();
    page_hover_stays();
    page_scroll_apart();
    list_scroll_apart();
    badges_redraw();
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
    home_entry();
    login_hints();
    pause_eight();
    cheats_note();
    detail_hints();
#endif
    if (layout_failures()) { fprintf(stderr, "%d failure(s)\n", layout_failures()); return 1; }
    printf("launcher achievements: the screens match the layout reference within %.1f px, and their keys do what they say\n", (double)LAYOUT_TOLERANCE);
    return 0;
}
