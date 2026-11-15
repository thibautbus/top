/* RetroAchievements' screens (ui_achievements.h): their texts, keys and pointer, the toasts' queue and the list. */
#include "ui_achievements.h"

#include <stdio.h>
#include <string.h>

#define MIDDLE_DOT "\xc2\xb7"
#define ELLIPSIS "\xe2\x80\xa6"

const char oracles_achievements_mod_reason[] = "Achievements are paused until the game starts again: a mod changed the game.";
static const char cheat_suspension[] = "Achievements suspended: a cheat is on";
static const char page_note[] = "Casual mode: every launcher feature stays, savestates included.";
static const char *const row_labels[ORACLES_ACHIEVEMENTS_ROWS] = { "RetroAchievements", "Account", "Privacy", "Unlock sound" };
static const char *const choices[2] = { "Off", "On" };
static const char *const sound_choices[2] = { "On", "Off" };
static const char explanation_on[] = "Unlocks the achievements players wrote for Ages and Seasons, as you play.";
static const char explanation_privacy[] = "Sends your user name, the game you play and your unlocks to retroachievements.org.";
static const char explanation_sound[] = "A short sound with each unlock's toast, over the game's own.";
static const char *const helps[ORACLES_ACHIEVEMENTS_ROWS][2] = {
    { "On: achievements for Ages and Seasons, casual mode.", "Nothing is sent while it is off or before you log in." },
    { "One account for every game.", "Only the token the server returns is kept, never the password." },
    { "Sends your user name, game and unlocks to retroachievements.org.", "Player's guide: the full text, in your browser." },
    { "On: a short sound with each unlock's toast.", "Off: the toast alone." },
};
static const char link_help[] = "Achievements for Ages and Seasons, casual mode.";

const char *oracles_achievements_row_label(unsigned row) { return row < ORACLES_ACHIEVEMENTS_ROWS ? row_labels[row] : ""; }

void oracles_achievements_summary(const OraclesHomeNav *nav, char *out, size_t capacity)
{
    const OraclesHomeAchievements *a = &nav->achievements;
    if (!a->on) snprintf(out, capacity, "Off");
    else if (a->user[0]) snprintf(out, capacity, "On " MIDDLE_DOT " %s", a->user);
    else snprintf(out, capacity, "On " MIDDLE_DOT " not logged in");
}

void oracles_achievements_about_row(const OraclesHomeNav *nav, OraclesUiListPage *page, unsigned row)
{
    static char summary[ORACLES_HOME_ACCOUNT_LENGTH + 16];
    oracles_achievements_summary(nav, summary, sizeof summary);
    page->labels[row] = row_labels[ORACLES_ACHIEVEMENTS_ROW_ON];
    page->kinds[row] = ORACLES_UI_LIST_LINK;
    page->summaries[row] = summary;
    snprintf(page->explanations[row], sizeof page->explanations[row], "%s", link_help);
}

void oracles_achievements_about_help(const char **help, const char **note)
{
    *help = link_help;
    *note = helps[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT][0];
}

const char *oracles_achievements_cheats_note(const OraclesHomeNav *nav)
{
    const OraclesHomeAchievements *a = &nav->achievements;
    if (!a->shown) return NULL;
    int running;
    if (nav->in_game) running = a->active;
    else {
        const int g = oracles_home_game(nav);
        running = a->on && a->user[0] && (g == ORACLES_HOME_GAME_AGES || g == ORACLES_HOME_GAME_SEASONS) && nav->games[g].rom == ORACLES_ROM_ORIGINAL;
    }
    return running ? "Turning a cheat on suspends achievements for this game session." : NULL;
}

void oracles_achievements_open(OraclesHomeNav *nav)
{
    nav->screen = ORACLES_SCREEN_ACHIEVEMENTS;
    nav->row = ORACLES_ACHIEVEMENTS_ROW_ON;
}

void oracles_achievements_help(const OraclesHomeNav *nav, const char **help, const char **note)
{
    const unsigned row = nav->row < ORACLES_ACHIEVEMENTS_ROWS ? nav->row : 0u;
    *help = helps[row][0];
    *note = helps[row][1];
}

const char *oracles_achievements_account_status(const OraclesHomeNav *nav, char *out, size_t capacity, OraclesTone *dot, OraclesTone *tone)
{
    const OraclesHomeAchievements *a = &nav->achievements;
    if (a->user[0] && a->pending) {
        snprintf(out, capacity, "%u unlock%s waiting to be sent", a->pending, a->pending == 1u ? "" : "s");
        *dot = ORACLES_TONE_NONE;
        *tone = ORACLES_TONE_OK;
        return out;
    }
    if (!a->user[0] && a->refused) {
        snprintf(out, capacity, "retroachievements.org refused the saved login: log in again.");
        *dot = *tone = ORACLES_TONE_WARN;
        return out;
    }
    return NULL;
}

void oracles_ui_layout_achievements(OraclesUiLayout layout, const OraclesHomeNav *nav, OraclesUiCheatsLayout *out)
{
    static char status[96];
    const OraclesHomeAchievements *a = &nav->achievements;
    const int four_three = layout == ORACLES_UI_LAYOUT_4_3;
    OraclesTone dot, tone;
    OraclesUiListPage page;
    memset(&page, 0, sizeof page);
    page.section = "About \xe2\x80\xba RetroAchievements";
    page.over = four_three ? NULL : "The Oracles";
    page.title = four_three ? "The Oracles Project" : "Project";
    page.note = page_note;
    page.rows = ORACLES_ACHIEVEMENTS_ROWS;
    oracles_achievements_help(nav, &page.help, &page.help_note);
    page.status = oracles_achievements_account_status(nav, status, sizeof status, &dot, &tone);
    page.extras_row = ORACLES_ACHIEVEMENTS_ROW_ACCOUNT;
    page.centred = 1;
    for (unsigned r = 0; r < ORACLES_ACHIEVEMENTS_ROWS; r++) page.labels[r] = row_labels[r];
    page.choices[ORACLES_ACHIEVEMENTS_ROW_ON] = choices;
    snprintf(page.explanations[ORACLES_ACHIEVEMENTS_ROW_ON], sizeof page.explanations[0], "%s", explanation_on);
    /* The account's name as the server spells it, in the row's title style; 4:3 has it before its action. */
    const char *account = a->user[0] ? a->user : "Not logged in";
    page.kinds[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT] = ORACLES_UI_LIST_ACTION;
    snprintf(page.explanations[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT], sizeof page.explanations[0], "%s", account);
    if (a->user[0]) page.explanation_styles[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT] = &oracles_ui_row_title;
    page.values[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT] = account;
    page.actions[ORACLES_ACHIEVEMENTS_ROW_ACCOUNT] = a->user[0] ? "Log out" : "Log in" ELLIPSIS;
    page.kinds[ORACLES_ACHIEVEMENTS_ROW_PRIVACY] = ORACLES_UI_LIST_ACTION;
    snprintf(page.explanations[ORACLES_ACHIEVEMENTS_ROW_PRIVACY], sizeof page.explanations[0], "%s", explanation_privacy);
    page.actions[ORACLES_ACHIEVEMENTS_ROW_PRIVACY] = "Player's guide";
    page.choices[ORACLES_ACHIEVEMENTS_ROW_SOUND] = sound_choices;
    snprintf(page.explanations[ORACLES_ACHIEVEMENTS_ROW_SOUND], sizeof page.explanations[0], "%s", explanation_sound);
    oracles_ui_layout_list(layout, &page, out);
}

/* On, Off: On without an account opens the login, which keeps it On while it is open. */
static OraclesHomeCommand set_on(OraclesHomeNav *nav, int on)
{
    OraclesHomeAchievements *a = &nav->achievements;
    if (on == a->on) return ORACLES_HOME_STAY;
    if (on && !a->user[0]) {
        a->on = 1;
        oracles_achievements_login_open(nav, 1);
        return ORACLES_HOME_STAY;
    }
    a->on = on;
    return ORACLES_HOME_STORE;
}

void oracles_achievements_store(const OraclesHomeNav *nav, int offline, int *setting, int *kept)
{
    if (nav->achievements.login) return;   /* the login that On opened keeps it Off until an account is in */
    *(offline ? kept : setting) = nav->achievements.on;
}

/* Unlock sound: On or Off, at once; the next unlock's toast plays it or not. */
static OraclesHomeCommand set_sound(OraclesHomeNav *nav, int on)
{
    if ((nav->achievements.sound != 0) == on) return ORACLES_HOME_STAY;
    nav->achievements.sound = on;
    return ORACLES_HOME_STORE;
}

static OraclesHomeCommand account(OraclesHomeNav *nav)
{
    if (nav->achievements.user[0]) return ORACLES_HOME_ACHIEVEMENTS_LOGOUT;
    oracles_achievements_login_open(nav, 0);
    return ORACLES_HOME_STAY;
}

static OraclesHomeCommand cancel(OraclesHomeNav *nav);

OraclesHomeCommand oracles_achievements_act(OraclesHomeNav *nav, OraclesHomeAction action)
{
    /* The login takes its own keys (oracles_achievements_login_key); Back, its hint's click, cancels it. */
    if (nav->achievements.login) return action == ORACLES_HOME_BACK ? cancel(nav) : ORACLES_HOME_STAY;
    if (nav->row >= ORACLES_ACHIEVEMENTS_ROWS) nav->row = 0;
    const unsigned row = nav->row;
    switch (action) {
        case ORACLES_HOME_UP: nav->row = (row + ORACLES_ACHIEVEMENTS_ROWS - 1u) % ORACLES_ACHIEVEMENTS_ROWS; return ORACLES_HOME_STAY;
        case ORACLES_HOME_DOWN: nav->row = (row + 1u) % ORACLES_ACHIEVEMENTS_ROWS; return ORACLES_HOME_STAY;
        case ORACLES_HOME_LEFT:
        case ORACLES_HOME_RIGHT:
            if (row == ORACLES_ACHIEVEMENTS_ROW_SOUND) return set_sound(nav, !nav->achievements.sound);
            return row == ORACLES_ACHIEVEMENTS_ROW_ON ? set_on(nav, !nav->achievements.on) : ORACLES_HOME_STAY;
        case ORACLES_HOME_OK:
            if (row == ORACLES_ACHIEVEMENTS_ROW_ON) return set_on(nav, !nav->achievements.on);
            if (row == ORACLES_ACHIEVEMENTS_ROW_SOUND) return set_sound(nav, !nav->achievements.sound);
            return row == ORACLES_ACHIEVEMENTS_ROW_ACCOUNT ? account(nav) : ORACLES_HOME_ACHIEVEMENTS_GUIDE;
        case ORACLES_HOME_BACK:
            /* Back to About, on its link: its last row. */
            nav->screen = ORACLES_SCREEN_ABOUT;
            nav->row = oracles_about_rows(nav) - 1u;
            return ORACLES_HOME_STAY;
    }
    return ORACLES_HOME_STAY;
}

OraclesHomeCommand oracles_achievements_click(OraclesHomeNav *nav, unsigned row, int option)
{
    if (nav->achievements.login || row >= ORACLES_ACHIEVEMENTS_ROWS) return ORACLES_HOME_STAY;
    nav->row = row;
    if (row == ORACLES_ACHIEVEMENTS_ROW_ON) return option == 0 || option == 1 ? set_on(nav, option) : ORACLES_HOME_STAY;
    if (row == ORACLES_ACHIEVEMENTS_ROW_SOUND) return option == 0 || option == 1 ? set_sound(nav, option == 0) : ORACLES_HOME_STAY;
    if (option != 2) return ORACLES_HOME_STAY;
    return row == ORACLES_ACHIEVEMENTS_ROW_ACCOUNT ? account(nav) : ORACLES_HOME_ACHIEVEMENTS_GUIDE;
}

/* ---- the login ---------------------------------------------------------------------------------------------- */

/* The keyboard's pages: digits, then letters with @ . - _; the printable symbols left on the second. */
static const char *const letters[4] = { "1234567890", "qwertyuiop", "asdfghjkl@", "zxcvbnm.-_" };
static const char *const symbols[4] = { "1234567890", "!\"#$%&'()*", "+,/:;<=>?[", "\\]^`{|}~" };
#define ERASER "\xe2\x8c\xab"   /* ⌫ */

static void wipe(char *text, size_t size)
{
    volatile char *v = text;
    while (size--) *v++ = 0;
}

void oracles_achievements_login_open(OraclesHomeNav *nav, int by_on)
{
    OraclesHomeAchievements *a = &nav->achievements;
    a->login = ORACLES_LOGIN_TYPING;
    a->field = ORACLES_LOGIN_NAME;
    a->opened_by_on = by_on;
    a->password_shown = 0;
    wipe(a->name, sizeof a->name);
    wipe(a->password, sizeof a->password);
    a->keyboard_page = a->keyboard_shift = 0;
    a->key_row = 1;
    a->key_column = 0;
}

void oracles_achievements_login_close(OraclesHomeNav *nav)
{
    OraclesHomeAchievements *a = &nav->achievements;
    if (a->login && a->opened_by_on && !a->user[0]) a->on = 0;   /* Cancel leaves it as it was: Off */
    a->login = ORACLES_LOGIN_CLOSED;
    a->opened_by_on = 0;
    a->text_dismissed = 0;
    a->password_shown = 0;
    wipe(a->name, sizeof a->name);
    wipe(a->password, sizeof a->password);
}

/* Closed; while it logs in, the login sent given up too. */
static OraclesHomeCommand cancel(OraclesHomeNav *nav)
{
    const int sent = nav->achievements.login == ORACLES_LOGIN_LOGGING;
    oracles_achievements_login_close(nav);
    return sent ? ORACLES_HOME_ACHIEVEMENTS_CANCEL : ORACLES_HOME_STAY;
}

int oracles_achievements_text_wanted(OraclesHomeNav *nav, int active)
{
    OraclesHomeAchievements *a = &nav->achievements;
    const int field = nav->screen == ORACLES_SCREEN_ACHIEVEMENTS && oracles_achievements_login_typing(nav) && !oracles_achievements_keyboard_shown(nav)
                      ? a->field + 1 : 0;
    if (field && a->text_for == field && !active) a->text_dismissed = field;   /* put away by the system */
    if (a->text_dismissed != field) a->text_dismissed = 0;                     /* another field, or none */
    return a->text_dismissed ? 0 : field;
}

int oracles_achievements_login_typing(const OraclesHomeNav *nav)
{
    const OraclesHomeAchievements *a = &nav->achievements;
    return a->login && a->login != ORACLES_LOGIN_LOGGING && (a->field == ORACLES_LOGIN_NAME || a->field == ORACLES_LOGIN_PASSWORD);
}

int oracles_achievements_keyboard_shown(const OraclesHomeNav *nav)
{
    return oracles_achievements_login_typing(nav) && nav->device == ORACLES_INPUT_GAMEPAD;
}

unsigned oracles_achievements_keyboard_keys(const OraclesHomeNav *nav, unsigned row, char keys[ORACLES_KEYBOARD_COLUMNS][8])
{
    const OraclesHomeAchievements *a = &nav->achievements;
    if (row >= ORACLES_KEYBOARD_ROWS) return 0;
    if (row == ORACLES_KEYBOARD_ROWS - 1u) {
        static const char *const last[5] = { "Shift", "#+=", "Space", ERASER, "Done" };
        for (unsigned i = 0; i < 5; i++) snprintf(keys[i], 8, "%s", i == 1 && a->keyboard_page ? "abc" : last[i]);
        return 5;
    }
    const char *chars = (a->keyboard_page ? symbols : letters)[row];
    unsigned n = 0;
    for (; chars[n] && n < ORACLES_KEYBOARD_COLUMNS; n++) {
        char c = chars[n];
        if (a->keyboard_shift && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        keys[n][0] = c;
        keys[n][1] = 0;
    }
    return n;
}

static char *field_text(OraclesHomeAchievements *a, size_t *capacity)
{
    *capacity = a->field == ORACLES_LOGIN_PASSWORD ? sizeof a->password : sizeof a->name;
    return a->field == ORACLES_LOGIN_PASSWORD ? a->password : a->name;
}

/* An edit clears an error shown. */
static void edited(OraclesHomeAchievements *a)
{
    if (a->login == ORACLES_LOGIN_WRONG || a->login == ORACLES_LOGIN_UNREACHABLE) a->login = ORACLES_LOGIN_TYPING;
}

static void erase(OraclesHomeAchievements *a)
{
    size_t capacity;
    char *text = field_text(a, &capacity);
    size_t n = strlen(text);
    while (n > 0 && ((unsigned char)text[n - 1] & 0xc0u) == 0x80u) text[--n] = 0;   /* a whole character */
    if (n > 0) text[n - 1] = 0;
    edited(a);
}

/* The bytes of the UTF-8 character `c` starts: 0 for a byte no character starts with. */
static size_t character_length(unsigned char c)
{
    if (c < 0x80u) return 1;
    if (c >= 0xc2u && c <= 0xdfu) return 2;
    if (c >= 0xe0u && c <= 0xefu) return 3;
    if (c >= 0xf0u && c <= 0xf4u) return 4;
    return 0;
}

void oracles_achievements_login_text(OraclesHomeNav *nav, const char *utf8)
{
    OraclesHomeAchievements *a = &nav->achievements;
    if (!oracles_achievements_login_typing(nav) || !utf8) return;
    a->text_dismissed = 0;   /* typed into: the system's input again */
    size_t capacity;
    char *text = field_text(a, &capacity);
    size_t n = strlen(text);
    /* Whole characters only, the controls left out: a field full keeps no half of one. */
    for (const unsigned char *c = (const unsigned char *)utf8; *c;) {
        size_t length = character_length(*c), k = 1;
        while (k < length && (c[k] & 0xc0u) == 0x80u) k++;
        if (!length || k < length) { c += k; continue; }
        if (n + length >= capacity) break;
        if (length > 1 || (*c >= 0x20u && *c != 0x7fu)) { memcpy(text + n, c, length); n += length; }
        c += length;
    }
    text[n] = 0;
    edited(a);
}

/* The UTF-8 character at `c`, `end` its text's end: its bytes, 0 when it is not one (a stray or missing byte, an
 * overlong form, a surrogate, past U+10FFFF) or when it is a control (C0, DEL, C1) or a line or paragraph separator
 * (U+2028, U+2029). */
static size_t pasted_character(const unsigned char *c, const unsigned char *end)
{
    const size_t length = character_length(*c);
    if (!length || (size_t)(end - c) < length) return 0;
    for (size_t k = 1; k < length; k++) if ((c[k] & 0xc0u) != 0x80u) return 0;
    if (length == 1) return *c >= 0x20u && *c != 0x7fu ? 1 : 0;
    if (c[0] == 0xc2u && c[1] < 0xa0u) return 0;                                 /* U+0080..U+009F */
    if ((c[0] == 0xe0u && c[1] < 0xa0u) || (c[0] == 0xedu && c[1] >= 0xa0u)) return 0;
    if ((c[0] == 0xf0u && c[1] < 0x90u) || (c[0] == 0xf4u && c[1] >= 0x90u)) return 0;
    if (c[0] == 0xe2u && c[1] == 0x80u && (c[2] == 0xa8u || c[2] == 0xa9u)) return 0;
    return length;
}

/* The blanks a copy brings around its text: ASCII's spaces and line breaks, and the no-break space (U+00A0), the
 * byte order mark (U+FEFF) and the zero-width space (U+200B) of a web page's or a document's. */
static const char *const blanks[] = { " ", "\t", "\n", "\r", "\v", "\f", "\xc2\xa0", "\xef\xbb\xbf", "\xe2\x80\x8b" };

/* The bytes of the blank `start` begins with (`at_end` 0) or `end` ends with (1), 0 for none. */
static size_t blank_at(const unsigned char *start, const unsigned char *end, int at_end)
{
    for (size_t i = 0; i < sizeof blanks / sizeof blanks[0]; i++) {
        const size_t length = strlen(blanks[i]);
        if ((size_t)(end - start) >= length && !memcmp(at_end ? end - length : start, blanks[i], length)) return length;
    }
    return 0;
}

OraclesLoginPaste oracles_login_paste_text(char *field, size_t capacity, const char *pasted)
{
    if (!field || !capacity || !pasted) return ORACLES_LOGIN_PASTE_EMPTY;
    const unsigned char *start = (const unsigned char *)pasted, *end = start + strlen(pasted);
    for (size_t k; (k = blank_at(start, end, 0)) != 0;) start += k;
    for (size_t k; (k = blank_at(start, end, 1)) != 0;) end -= k;
    if (start == end) return ORACLES_LOGIN_PASTE_EMPTY;
    for (const unsigned char *c = start; c < end;) {
        const size_t length = pasted_character(c, end);
        if (!length) return ORACLES_LOGIN_PASTE_REFUSED;
        c += length;
    }
    const size_t n = strlen(field), length = (size_t)(end - start);
    if (n + length >= capacity) return ORACLES_LOGIN_PASTE_TOO_LONG;
    memcpy(field + n, start, length);
    field[n + length] = 0;
    return ORACLES_LOGIN_PASTE_DONE;
}

/* `text` into the field highlighted by the paste's rules. */
static OraclesLoginPaste paste_into(OraclesHomeNav *nav, const char *text)
{
    OraclesHomeAchievements *a = &nav->achievements;
    if (!oracles_achievements_login_typing(nav) || !text) return ORACLES_LOGIN_PASTE_EMPTY;
    size_t capacity;
    char *field = field_text(a, &capacity);
    const OraclesLoginPaste done = oracles_login_paste_text(field, capacity, text);
    if (done == ORACLES_LOGIN_PASTE_DONE) {
        a->text_dismissed = 0;
        edited(a);
    }
    return done;
}

OraclesLoginPaste oracles_achievements_login_paste(OraclesHomeNav *nav, char *pasted)
{
    const OraclesLoginPaste done = paste_into(nav, pasted);
    if (pasted) wipe(pasted, strlen(pasted));
    return done;
}

OraclesLoginPaste oracles_achievements_login_typed(OraclesHomeNav *nav, const char *utf8)
{
    unsigned characters = 0;
    for (const unsigned char *c = (const unsigned char *)(utf8 ? utf8 : ""); *c && characters < 2u; c++) characters += (*c & 0xc0u) != 0x80u;
    if (characters > 1u) return paste_into(nav, utf8);
    oracles_achievements_login_text(nav, utf8);
    return ORACLES_LOGIN_PASTE_DONE;
}

int oracles_login_paste_chord(OraclesLoginRawKey key, unsigned mods, int repeat, int apple)
{
    const unsigned command = ORACLES_LOGIN_MOD_CTRL | (apple ? ORACLES_LOGIN_MOD_GUI : 0u);
    const int chord = key == ORACLES_LOGIN_RAW_PASTE
                      || (key == ORACLES_LOGIN_RAW_V && (mods & command) && !(mods & ORACLES_LOGIN_MOD_ALT))   /* not AltGr's Ctrl+Alt */
                      || (key == ORACLES_LOGIN_RAW_INSERT && (mods & ORACLES_LOGIN_MOD_SHIFT)
                          && !(mods & (ORACLES_LOGIN_MOD_CTRL | ORACLES_LOGIN_MOD_ALT | ORACLES_LOGIN_MOD_GUI)));
    return !chord ? 0 : repeat ? -1 : 1;
}

int oracles_login_made_up_key(OraclesLoginRawKey key, unsigned keyboard, unsigned raw, int text_waiting)
{
    return (key == ORACLES_LOGIN_RAW_RETURN || key == ORACLES_LOGIN_RAW_TAB) && keyboard == 0u && raw == 0u && text_waiting;
}

/* ---- the password shown -------------------------------------------------------------------------------- */

void oracles_achievements_password_toggle(OraclesHomeNav *nav)
{
    OraclesHomeAchievements *a = &nav->achievements;
    if (nav->screen == ORACLES_SCREEN_ACHIEVEMENTS && a->login) a->password_shown = !a->password_shown;
}

int oracles_achievements_password_shown(const OraclesHomeNav *nav)
{
    return nav->screen == ORACLES_SCREEN_ACHIEVEMENTS && nav->achievements.login && nav->achievements.password_shown;
}

void oracles_achievements_password_hide(OraclesHomeNav *nav) { nav->achievements.password_shown = 0; }

void oracles_achievements_window_changed(OraclesHomeNav *nav, OraclesLoginWindow change)
{
    if (change != ORACLES_LOGIN_WINDOW_OTHER) oracles_achievements_password_hide(nav);
}

static OraclesHomeCommand submit(OraclesHomeNav *nav)
{
    OraclesHomeAchievements *a = &nav->achievements;
    if (!a->name[0] || !a->password[0]) {   /* a field to fill first */
        a->field = a->name[0] ? ORACLES_LOGIN_PASSWORD : ORACLES_LOGIN_NAME;
        return ORACLES_HOME_STAY;
    }
    a->login = ORACLES_LOGIN_LOGGING;
    return ORACLES_HOME_ACHIEVEMENTS_LOGIN;
}

/* A key of the launcher's keyboard pressed. */
static OraclesHomeCommand press(OraclesHomeNav *nav, unsigned row, unsigned column)
{
    OraclesHomeAchievements *a = &nav->achievements;
    char keys[ORACLES_KEYBOARD_COLUMNS][8];
    const unsigned count = oracles_achievements_keyboard_keys(nav, row, keys);
    if (column >= count) return ORACLES_HOME_STAY;
    const char *k = keys[column];
    if (!strcmp(k, "Shift")) { a->keyboard_shift = !a->keyboard_shift; return ORACLES_HOME_STAY; }
    if (!strcmp(k, "#+=") || !strcmp(k, "abc")) { a->keyboard_page = !a->keyboard_page; return ORACLES_HOME_STAY; }
    if (!strcmp(k, "Done")) { a->field = a->field == ORACLES_LOGIN_NAME ? ORACLES_LOGIN_PASSWORD : ORACLES_LOGIN_OK; return ORACLES_HOME_STAY; }
    if (!strcmp(k, ERASER)) { erase(a); return ORACLES_HOME_STAY; }
    oracles_achievements_login_text(nav, !strcmp(k, "Space") ? " " : k);
    a->keyboard_shift = 0;
    return ORACLES_HOME_STAY;
}

static OraclesHomeCommand keyboard_key(OraclesHomeNav *nav, OraclesLoginKey key)
{
    OraclesHomeAchievements *a = &nav->achievements;
    char keys[ORACLES_KEYBOARD_COLUMNS][8];
    unsigned row = (unsigned)a->key_row < ORACLES_KEYBOARD_ROWS ? (unsigned)a->key_row : 0u;
    unsigned count = oracles_achievements_keyboard_keys(nav, row, keys);
    unsigned column = (unsigned)a->key_column < count ? (unsigned)a->key_column : count - 1u;
    switch (key) {
        case ORACLES_LOGIN_PAD_UP:
        case ORACLES_LOGIN_PAD_DOWN:
            if (key == ORACLES_LOGIN_PAD_UP && row > 0) row--;
            else if (key == ORACLES_LOGIN_PAD_DOWN && row + 1u < ORACLES_KEYBOARD_ROWS) row++;
            count = oracles_achievements_keyboard_keys(nav, row, keys);
            a->key_row = (int)row;
            a->key_column = (int)(column < count ? column : count - 1u);
            return ORACLES_HOME_STAY;
        case ORACLES_LOGIN_PAD_LEFT: a->key_row = (int)row; a->key_column = (int)((column + count - 1u) % count); return ORACLES_HOME_STAY;
        case ORACLES_LOGIN_PAD_RIGHT: a->key_row = (int)row; a->key_column = (int)((column + 1u) % count); return ORACLES_HOME_STAY;
        case ORACLES_LOGIN_PAD_A: return press(nav, row, column);
        case ORACLES_LOGIN_PAD_B: {
            /* An empty field puts the keyboard away, on Log in. */
            size_t capacity;
            if (!field_text(a, &capacity)[0]) a->field = ORACLES_LOGIN_OK;
            else erase(a);
            return ORACLES_HOME_STAY;
        }
        case ORACLES_LOGIN_PAD_Y: a->keyboard_shift = !a->keyboard_shift; return ORACLES_HOME_STAY;
        case ORACLES_LOGIN_PAD_SHOULDER: a->keyboard_page = !a->keyboard_page; return ORACLES_HOME_STAY;
        case ORACLES_LOGIN_PAD_START: a->field = a->field == ORACLES_LOGIN_NAME ? ORACLES_LOGIN_PASSWORD : ORACLES_LOGIN_OK; return ORACLES_HOME_STAY;
        default: return ORACLES_HOME_STAY;
    }
}

OraclesHomeCommand oracles_achievements_login_key(OraclesHomeNav *nav, OraclesLoginKey key)
{
    OraclesHomeAchievements *a = &nav->achievements;
    if (!a->login) return ORACLES_HOME_STAY;
    if (a->login == ORACLES_LOGIN_LOGGING) {   /* while it logs in, Cancel alone */
        const int on_cancel = a->field == ORACLES_LOGIN_CANCEL && (key == ORACLES_LOGIN_KEY_ENTER || key == ORACLES_LOGIN_PAD_A);
        return key == ORACLES_LOGIN_KEY_ESCAPE || key == ORACLES_LOGIN_PAD_B || on_cancel ? cancel(nav) : ORACLES_HOME_STAY;
    }
    const int in_field = a->field == ORACLES_LOGIN_NAME || a->field == ORACLES_LOGIN_PASSWORD;
    if (key >= ORACLES_LOGIN_PAD_UP && in_field && oracles_achievements_keyboard_shown(nav)) return keyboard_key(nav, key);
    switch (key) {
        case ORACLES_LOGIN_KEY_ESCAPE:
        case ORACLES_LOGIN_PAD_B: oracles_achievements_login_close(nav); return ORACLES_HOME_STAY;
        case ORACLES_LOGIN_KEY_ENTER:
            if (a->field == ORACLES_LOGIN_CANCEL) { oracles_achievements_login_close(nav); return ORACLES_HOME_STAY; }
            if (a->field == ORACLES_LOGIN_NAME) { a->field = ORACLES_LOGIN_PASSWORD; return ORACLES_HOME_STAY; }
            return submit(nav);
        case ORACLES_LOGIN_KEY_TAB:
        case ORACLES_LOGIN_KEY_DOWN: a->field = (a->field + 1) % ORACLES_LOGIN_FIELDS; return ORACLES_HOME_STAY;
        case ORACLES_LOGIN_KEY_BACKTAB:
        case ORACLES_LOGIN_KEY_UP: a->field = (a->field + ORACLES_LOGIN_FIELDS - 1) % ORACLES_LOGIN_FIELDS; return ORACLES_HOME_STAY;
        case ORACLES_LOGIN_KEY_BACKSPACE: if (in_field) erase(a); return ORACLES_HOME_STAY;
        case ORACLES_LOGIN_KEY_LEFT:
        case ORACLES_LOGIN_KEY_RIGHT:
        case ORACLES_LOGIN_PAD_LEFT:
        case ORACLES_LOGIN_PAD_RIGHT:
            if (!in_field) a->field = a->field == ORACLES_LOGIN_OK ? ORACLES_LOGIN_CANCEL : ORACLES_LOGIN_OK;
            return ORACLES_HOME_STAY;
        case ORACLES_LOGIN_PAD_UP: if (a->field > 0) a->field--; return ORACLES_HOME_STAY;
        case ORACLES_LOGIN_PAD_DOWN: if (a->field + 1 < ORACLES_LOGIN_FIELDS) a->field++; return ORACLES_HOME_STAY;
        case ORACLES_LOGIN_PAD_A:
            if (a->field == ORACLES_LOGIN_OK) return submit(nav);
            if (a->field == ORACLES_LOGIN_CANCEL) { oracles_achievements_login_close(nav); return ORACLES_HOME_STAY; }
            a->field++;
            return ORACLES_HOME_STAY;
        case ORACLES_LOGIN_PAD_START: return submit(nav);
        default: return ORACLES_HOME_STAY;
    }
}

OraclesHomeCommand oracles_achievements_login_click(OraclesHomeNav *nav, int target)
{
    OraclesHomeAchievements *a = &nav->achievements;
    if (!a->login || target < 0) return ORACLES_HOME_STAY;
    if (a->login == ORACLES_LOGIN_LOGGING) return target == ORACLES_LOGIN_CANCEL ? cancel(nav) : ORACLES_HOME_STAY;
    if (target == ORACLES_LOGIN_NAME || target == ORACLES_LOGIN_PASSWORD) {
        /* Touched: the system's input started again, its keyboard up again however it went away. */
        a->field = target;
        a->text_dismissed = a->text_for = 0;
        return ORACLES_HOME_STAY;
    }
    if (target == ORACLES_LOGIN_OK) { a->field = target; return submit(nav); }
    if (target == ORACLES_LOGIN_CANCEL) { oracles_achievements_login_close(nav); return ORACLES_HOME_STAY; }
    const int key = target - ORACLES_LOGIN_FIELDS;
    if (key < 0 || key >= ORACLES_KEYBOARD_ROWS * ORACLES_KEYBOARD_COLUMNS) return ORACLES_HOME_STAY;
    a->key_row = key / ORACLES_KEYBOARD_COLUMNS;
    a->key_column = key % ORACLES_KEYBOARD_COLUMNS;
    return press(nav, (unsigned)a->key_row, (unsigned)a->key_column);
}

void oracles_achievements_login_answered(OraclesHomeNav *nav, int ok, const char *user)
{
    OraclesHomeAchievements *a = &nav->achievements;
    if (a->login != ORACLES_LOGIN_LOGGING) return;
    if (ok > 0) {
        snprintf(a->user, sizeof a->user, "%.*s", (int)sizeof a->user - 1, user && user[0] ? user : a->name);
        a->on = 1;
        a->refused = 0;
        a->opened_by_on = 0;
        oracles_achievements_login_close(nav);
        nav->row = ORACLES_ACHIEVEMENTS_ROW_ACCOUNT;
        return;
    }
    a->login = ok == 0 ? ORACLES_LOGIN_WRONG : ORACLES_LOGIN_UNREACHABLE;
    a->field = ok == 0 ? ORACLES_LOGIN_PASSWORD : ORACLES_LOGIN_OK;
}

const char *oracles_achievements_login_state_text(const OraclesHomeNav *nav, OraclesTone *tone)
{
    switch ((OraclesAchievementsLoginState)nav->achievements.login) {
        case ORACLES_LOGIN_LOGGING: *tone = ORACLES_TONE_NONE; return "Logging in" ELLIPSIS;
        case ORACLES_LOGIN_WRONG: *tone = ORACLES_TONE_ERROR; return "Wrong user name or password.";
        case ORACLES_LOGIN_UNREACHABLE: *tone = ORACLES_TONE_ERROR; return "retroachievements.org cannot be reached: check your connection, then try again.";
        default: *tone = ORACLES_TONE_NONE; return NULL;
    }
}

/* ---- hints ---------------------------------------------------------------------------------------------- */

#define UP_DOWN "\xe2\x86\x91\xe2\x86\x93"
#define LEFT_RIGHT "\xe2\x86\x90\xe2\x86\x92"

unsigned oracles_achievements_hints(const OraclesHomeNav *nav, OraclesHomeHint *hints)
{
    /* The login's keys, each framed for five characters: the launcher's keyboard's, a controller's, the keyboard's. */
    if (nav->screen == ORACLES_SCREEN_ACHIEVEMENTS && nav->achievements.login) {
        static const OraclesHomeHint keyboard[5] = { { "A", "Type", 0, 5 }, { "B", "Erase", 0, 5 }, { "Y", "Shift", 0, 5 }, { "LB RB", "Symbols", 0, 5 },
                                                     { "Start", "Done", 0, 5 } };
        static const OraclesHomeHint pad[3] = { { "D-pad", "Move", 0, 5 }, { "A", "Select", 0, 5 }, { "B", "Cancel", 1, 5 } };
        static const OraclesHomeHint keys[3] = { { "Tab", "Next field", 0, 5 }, { "Enter", "Log in", 0, 5 }, { "Esc", "Cancel", 1, 5 } };
        if (oracles_achievements_keyboard_shown(nav)) { memcpy(hints, keyboard, sizeof keyboard); return 5; }
        if (nav->device == ORACLES_INPUT_GAMEPAD) { memcpy(hints, pad, sizeof pad); return 3; }
        memcpy(hints, keys, sizeof keys);
        return 3;
    }
    if (nav->screen == ORACLES_SCREEN_ACHIEVEMENTS_LIST) {
        static const OraclesHomeHint list[2] = { { UP_DOWN, "Scroll", 0, 0 }, { "Esc / B", "Back", 1, 0 } };
        memcpy(hints, list, sizeof list);
        return 2;
    }
    return 0;
}

/* ---- the toasts ----------------------------------------------------------------------------------------- */

void oracles_achievements_toast_push(OraclesAchievementsToasts *toasts, const char *over, const char *text, const char *badge)
{
    if (toasts->count == ORACLES_ACHIEVEMENTS_TOASTS) {   /* the oldest waiting goes: the one shown stays */
        for (unsigned i = 1; i + 1u < ORACLES_ACHIEVEMENTS_TOASTS; i++)
            toasts->queue[(toasts->first + i) % ORACLES_ACHIEVEMENTS_TOASTS] = toasts->queue[(toasts->first + i + 1u) % ORACLES_ACHIEVEMENTS_TOASTS];
        toasts->count--;
    }
    if (!toasts->count) toasts->since_ms = 0.0;
    OraclesAchievementsToast *t = &toasts->queue[(toasts->first + toasts->count) % ORACLES_ACHIEVEMENTS_TOASTS];
    memset(t, 0, sizeof *t);
    snprintf(t->over, sizeof t->over, "%s", over ? over : "");
    snprintf(t->text, sizeof t->text, "%s", text ? text : "");
    snprintf(t->badge, sizeof t->badge, "%s", badge ? badge : "");
    toasts->count++;
}

OraclesAchievementsToast *oracles_achievements_toast_now(OraclesAchievementsToasts *toasts, double now_ms, float *opacity)
{
    /* The clock runs while they are drawn: a gap between two frames (the pause, a notice over them, the window away)
     * moves the toast shown on by none of it. */
    if (toasts->drawn_ms > 0.0 && toasts->since_ms > 0.0 && now_ms - toasts->drawn_ms > ORACLES_ACHIEVEMENTS_TOAST_GAP_MS)
        toasts->since_ms += now_ms - toasts->drawn_ms;
    toasts->drawn_ms = now_ms;
    while (toasts->count) {
        if (toasts->since_ms <= 0.0) toasts->since_ms = now_ms > 0.0 ? now_ms : 1.0;
        const double shown = now_ms - toasts->since_ms;
        if (shown < ORACLES_ACHIEVEMENTS_TOAST_SHOWN_MS + ORACLES_ACHIEVEMENTS_TOAST_FADE_MS) {
            const float t = shown <= ORACLES_ACHIEVEMENTS_TOAST_SHOWN_MS ? 0.0f : (float)((shown - ORACLES_ACHIEVEMENTS_TOAST_SHOWN_MS) / ORACLES_ACHIEVEMENTS_TOAST_FADE_MS);
            *opacity = 1.0f - t * t * (3.0f - 2.0f * t);
            return &toasts->queue[toasts->first];
        }
        toasts->first = (toasts->first + 1u) % ORACLES_ACHIEVEMENTS_TOASTS;
        toasts->count--;
        toasts->since_ms = now_ms > 0.0 ? now_ms : 1.0;
    }
    toasts->since_ms = 0.0;   /* the next toast pushed starts at its first frame */
    *opacity = 0.0f;
    return NULL;
}

int oracles_achievements_toast_badge(OraclesAchievementsToast *toast, int loaded)
{
    if (!toast->badge_shown) toast->badge_shown = toast->badge[0] && loaded ? 1 : -1;
    return toast->badge_shown > 0;
}

int oracles_achievements_toast_sound(OraclesAchievementsToast *toast)
{
    if (!toast || !toast->sound) return 0;
    toast->sound = 0;
    return 1;
}

static const char service[] = "RetroAchievements";

void oracles_achievements_toast_unlock(OraclesAchievementsToasts *toasts, const char *title, unsigned points, const char *badge)
{
    char over[96];
    snprintf(over, sizeof over, "Achievement unlocked " MIDDLE_DOT " %u point%s", points, points == 1u ? "" : "s");
    oracles_achievements_toast_push(toasts, over, title, badge);
    toasts->queue[(toasts->first + toasts->count - 1u) % ORACLES_ACHIEVEMENTS_TOASTS].sound = 1;
}

void oracles_achievements_toast_start(OraclesAchievementsToasts *toasts, const char *game, unsigned unlocked, unsigned count, int suspended)
{
    char text[192];
    if (suspended == ORACLES_ACHIEVEMENTS_BY_MOD) snprintf(text, sizeof text, "%s", oracles_achievements_mod_reason);
    else if (suspended) snprintf(text, sizeof text, "%s " MIDDLE_DOT " %s", game, cheat_suspension);
    else if (count && unlocked >= count) snprintf(text, sizeof text, "%s " MIDDLE_DOT " all %u achievements " MIDDLE_DOT " Completed " MIDDLE_DOT " Casual", game, count);
    else snprintf(text, sizeof text, "%s " MIDDLE_DOT " %u of %u achievements " MIDDLE_DOT " Casual", game, unlocked, count);
    oracles_achievements_toast_push(toasts, service, text, NULL);
}

void oracles_achievements_toast_offline(OraclesAchievementsToasts *toasts) { oracles_achievements_toast_push(toasts, service, "Offline: unlocks will be sent later", NULL); }
void oracles_achievements_toast_unreachable(OraclesAchievementsToasts *toasts)
{
    oracles_achievements_toast_push(toasts, service, "RetroAchievements unreachable: achievements are off for this game.", NULL);
}
void oracles_achievements_toast_cheat(OraclesAchievementsToasts *toasts) { oracles_achievements_toast_push(toasts, service, cheat_suspension, NULL); }
void oracles_achievements_toast_mod(OraclesAchievementsToasts *toasts) { oracles_achievements_toast_push(toasts, service, oracles_achievements_mod_reason, NULL); }

/* ---- the list ---------------------------------------------------------------------------------------------- */

void oracles_achievements_list_open(OraclesHomeNav *nav)
{
    nav->screen = ORACLES_SCREEN_ACHIEVEMENTS_LIST;
    nav->achievements.list_focus = 0;
}

static unsigned list_count(const OraclesHomeNav *nav) { return nav->achievements.list ? nav->achievements.list->count : 0u; }

OraclesHomeCommand oracles_achievements_list_act(OraclesHomeNav *nav, OraclesHomeAction action)
{
    const unsigned count = list_count(nav);
    unsigned *focus = &nav->achievements.list_focus;
    switch (action) {
        case ORACLES_HOME_UP: if (*focus > 0) (*focus)--; return ORACLES_HOME_STAY;
        case ORACLES_HOME_DOWN: if (*focus + 1u < count) (*focus)++; return ORACLES_HOME_STAY;
        case ORACLES_HOME_BACK:
            nav->screen = ORACLES_SCREEN_PAUSE;
            oracles_home_focus_item(nav, ORACLES_HOME_ITEM_ACHIEVEMENTS);
            return ORACLES_HOME_STAY;
        default: return ORACLES_HOME_STAY;
    }
}

void oracles_achievements_list_summary(const OraclesHomeNav *nav, char *out, size_t capacity, int *warning)
{
    const OraclesAchievementsList *l = nav->achievements.list;
    const unsigned unlocked = l ? l->unlocked : 0u, count = l ? l->count : 0u;
    *warning = nav->achievements.suspended != ORACLES_ACHIEVEMENTS_RUNNING;
    if (nav->achievements.suspended == ORACLES_ACHIEVEMENTS_BY_MOD) snprintf(out, capacity, "%s", oracles_achievements_mod_reason);
    else if (*warning) snprintf(out, capacity, "Suspended for this session: a cheat is on " MIDDLE_DOT " %u of %u " MIDDLE_DOT " Casual", unlocked, count);
    else snprintf(out, capacity, "%u of %u achievements " MIDDLE_DOT " %u of %u points " MIDDLE_DOT " Casual", unlocked, count, l ? l->points_unlocked : 0u,
                  l ? l->points : 0u);
}
