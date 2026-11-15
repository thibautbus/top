/* RetroAchievements in the launcher's screens, in a build with ORACLES_RETROACHIEVEMENTS (launcher/achievements.h), without
 * SDL: the home menu's page, the login over it with the launcher's own keyboard, the toasts over a game and the pause's
 * list of the game's achievements.  The state is nav->achievements (ui_home_nav.h); a build without them never sets it,
 * and compiles none of this into the launcher.
 *
 * The home menu's Achievements, above Cheats, opens the page for the game chosen, on an Oracle's original ROM alone.  Its
 * rows are Account (the name the server spells, or "Not logged in"; Log out, or Log in…), RetroAchievements (Off or On),
 * Privacy (Player's guide, the full text in the system's browser) and Unlock sound (On or Off: a short sound with each
 * unlock's toast, ui_achievements_sound.h).  Turning it On without an account opens the login, whose Cancel leaves it Off;
 * logging out leaves it On, nothing being sent without an account.  Under Account, the unlocks waiting to be sent, or the
 * server's refusal of the token kept.  Under the rows, the game's list as the pause shows it, its summary then its
 * achievements, unlocked and remaining, the page scrolling as one block (in 4:3 by whole achievements, the help under the
 * page giving the highlighted one's description, then its date, its progress or Locked); until the list is there, a line
 * says what to do or why not: not logged in, Off, loading, the server unreachable.  Back returns to the home menu, on
 * Achievements.
 *
 * Both lists, the page and the pause's, scroll apart from their highlight: up and down move the highlight, the list
 * following it as little as brings it wholly in sight (moving down past the foot puts it at the foot, up past the top at
 * the top); a click or a touch highlights the row under it where it is; the wheel and a drag move the list, the highlight
 * staying on its row while it is in sight, else on the nearest row in sight.
 *
 * An achievement's detail, on the page as on the pause's list: its badge in large, title, points, rarity, tag, whole
 * description and state.  16:9: the left column shows the highlighted achievement's, with no key, the page's name and the
 * motif coming back on a row of settings.  4:3, and a pause too small to show that column (oracles_achievements_detail_view):
 * Enter, A, a click or a touch on an achievement opens it in a panel over the list; Back or a touch outside closes it on the
 * same achievement, and up and down go to the next one without closing it, stopping at the list's ends.  A drag never opens a
 * row (oracles_achievements_drag_end).
 *
 * The login: User name and Password, the password masked; a physical keyboard types into the field highlighted (Tab or
 * Down to the next field, Shift+Tab or Up back, Enter to the password then to log in, Escape cancels, Backspace erases,
 * Ctrl+V (Cmd+V on macOS) or Shift+Insert pastes, a text refused said in the state line);
 * Android's own keyboard comes up for a touch, the dialog then at the top; a controller opens the launcher's keyboard
 * under the fields: the D-pad moves, A types the key highlighted, B erases (on an empty field it puts the keyboard away,
 * on Log in), Y is Shift, LB and RB the page of symbols, Start Done (to the password, then to Log in), and its Paste key
 * pastes.  A key of a physical keyboard puts it away.  At the right end of the password, once it is not empty, an eye:
 * open to show the password, crossed to hide it, the text and the caret stopping before it.  A controller's X toggles it
 * wherever the highlight is; with a keyboard it is a Tab stop after Password, which Enter or Space toggles; a click or a
 * touch on it toggles it; while it logs in it is faded and inert.  "Logging in…", the error of a wrong name or password,
 * the server unreachable; once in, the dialog closes on the account's name. */
#ifndef ORACLES_UI_ACHIEVEMENTS_H
#define ORACLES_UI_ACHIEVEMENTS_H

#include "ui_page_layout.h"

#include <stdint.h>

/* ---- the page ---------------------------------------------------------------------------------------------- */

enum { ORACLES_ACHIEVEMENTS_ROW_ACCOUNT, ORACLES_ACHIEVEMENTS_ROW_ON, ORACLES_ACHIEVEMENTS_ROW_PRIVACY, ORACLES_ACHIEVEMENTS_ROW_SOUND,
       ORACLES_ACHIEVEMENTS_ROWS };

/* Why they stopped for the session (nav->achievements.suspended). */
typedef enum OraclesAchievementsSuspension {
    ORACLES_ACHIEVEMENTS_RUNNING,
    ORACLES_ACHIEVEMENTS_BY_CHEAT,
    ORACLES_ACHIEVEMENTS_BY_MOD
} OraclesAchievementsSuspension;

extern const char oracles_achievements_guide_url[];   /* the player's guide, at its RetroAchievements section */
extern const char oracles_achievements_mod_reason[];  /* "Achievements are paused until the game starts again: a mod changed the game." */

/* Cheats' note while the achievements run for the page's game (on, logged in, an original ROM of an Oracle; from the
 * pause, the session's), NULL otherwise. */
const char *oracles_achievements_cheats_note(const OraclesHomeNav *nav);
/* What settings.txt keeps of the page: achievements= (`*setting`) follows On and Off once no login holds them (the login
 * that On opened keeps it Off until an account is in); with the developer's offline answers (`offline`), never: `*kept`,
 * the memory's, follows them instead. */
void oracles_achievements_store(const OraclesHomeNav *nav, int offline, int *setting, int *kept);
/* The home menu's Achievements opened: the page, its first row. */
void oracles_achievements_open(OraclesHomeNav *nav);
/* The state of the home page's list (nav->achievements.list_state), which the host loads while the page is open, no game
 * running (achievements_load.h): none wanted, on its way, there, no answer, or refused. */
typedef enum OraclesAchievementsListState {
    ORACLES_ACHIEVEMENTS_LIST_NONE,
    ORACLES_ACHIEVEMENTS_LIST_LOADING,
    ORACLES_ACHIEVEMENTS_LIST_READY,
    ORACLES_ACHIEVEMENTS_LIST_UNREACHABLE,
    ORACLES_ACHIEVEMENTS_LIST_FAILED
} OraclesAchievementsListState;
/* The home page shows the game's list: logged in, On, its list there. */
int oracles_achievements_page_list(const OraclesHomeNav *nav);
/* The line under the page's rows while no list shows: what to do to see the game's achievements, or why they do not show. */
const char *oracles_achievements_page_line(const OraclesHomeNav *nav);
/* The page is on screen with its list, no login over it: the wheel and a drag run through it. */
int oracles_achievements_page_list_on(const OraclesHomeNav *nav);
/* The pointer over place `row`: the highlight follows it, unless the page has its list: there a click chooses, the
 * detail's column following the highlight alone. */
void oracles_achievements_page_hover(OraclesHomeNav *nav, unsigned row);
/* The wheel or a drag: the page `steps` achievements down (up when negative), stopping at its ends, the highlight kept in
 * sight; with the detail's panel open, the highlight `steps` achievements on instead. */
void oracles_achievements_page_scroll(OraclesHomeNav *nav, int steps);
/* The page's scroll as it shows: nav->achievements.page_scroll within the page's ends (in 4:3 a whole achievement's),
 * brought to the highlight only once none of it shows (a window changing layout, a shorter list). */
float oracles_ui_achievements_page_view(const OraclesHomeNav *nav);
/* The scroll kept moved as little as brings the highlight wholly in the keys' band: in 16:9 from the rows' top to 970,
 * in 4:3 among five whole achievements. */
float oracles_ui_achievements_page_follow(const OraclesHomeNav *nav);
/* That scroll moved by the wheel or a drag of `steps`, and in *place the highlight then: its own while wholly in the view,
 * else the nearest place that is. */
float oracles_ui_achievements_page_moved(const OraclesHomeNav *nav, int steps, unsigned *place);
/* A press held over the list: its drag scrolls the list a row for each `row` points; past a quarter of that it is no
 * more a tap, and its release clicks nothing, whatever has come under the pointer. */
typedef struct OraclesAchievementsDrag {
    float y, travel;
    int dragged;
} OraclesAchievementsDrag;
void oracles_achievements_drag_start(OraclesAchievementsDrag *drag);
/* The press moved by (dx, dy) points: the places to scroll (oracles_achievements_page_scroll), a finger going up scrolling down. */
int oracles_achievements_drag_move(OraclesAchievementsDrag *drag, float dx, float dy, float row);
/* The same on either list: none while the detail's panel is open, the drag still counted so that its release clicks nothing. */
int oracles_achievements_drag_steps(const OraclesHomeNav *nav, OraclesAchievementsDrag *drag, float dx, float dy, float row);
/* The press let go: 1 when it was a drag, its click dropped. */
int oracles_achievements_drag_end(OraclesAchievementsDrag *drag);
/* Whether the page's badges still want a frame: while the client works (`busy`), then a while longer, a file come at the
 * end being looked for again only after a pause (`*until_ms` the host's). */
int oracles_achievements_badges_redraw(int busy, int waiting, double now_ms, double *until_ms);
/* The places of the page's highlight: its rows, then each achievement of its list. */
unsigned oracles_achievements_page_places(const OraclesHomeNav *nav);


OraclesHomeCommand oracles_achievements_act(OraclesHomeNav *nav, OraclesHomeAction action);
/* A click on a row: `option` 0 and 1 its choices (RetroAchievements' Off and On, Unlock sound's On and Off), 2 its action
 * (Log in…, Log out, Player's guide), -1 the row itself. */
OraclesHomeCommand oracles_achievements_click(OraclesHomeNav *nav, unsigned row, int option);
/* Under Account: the unlocks waiting, or the token refused; NULL for none.  `dot` its dot's tone, `tone` its text's. */
const char *oracles_achievements_account_status(const OraclesHomeNav *nav, char *out, size_t capacity, OraclesTone *dot, OraclesTone *tone);
/* The page's rows laid out as Cheats (`status` the account's, made by oracles_achievements_account_status). */
void oracles_ui_layout_achievements(OraclesUiLayout layout, const OraclesHomeNav *nav, OraclesUiCheatsLayout *out);
/* The page's labels, row by row, and the highlighted row's help in 4:3. */
const char *oracles_achievements_row_label(unsigned row);
void oracles_achievements_help(const OraclesHomeNav *nav, const char **help, const char **note);
/* The page's hints: the pages', or the login's (oracles_achievements_login_hints) while it is open; 0 when not its
 * screen. */
unsigned oracles_achievements_hints(const OraclesHomeNav *nav, OraclesHomeHint *hints);

/* ---- the login ------------------------------------------------------------------------------------------- */

typedef enum OraclesAchievementsLoginState {
    ORACLES_LOGIN_CLOSED,
    ORACLES_LOGIN_TYPING,
    ORACLES_LOGIN_LOGGING,
    ORACLES_LOGIN_WRONG,
    ORACLES_LOGIN_UNREACHABLE
} OraclesAchievementsLoginState;
enum { ORACLES_LOGIN_NAME, ORACLES_LOGIN_PASSWORD, ORACLES_LOGIN_EYE, ORACLES_LOGIN_OK, ORACLES_LOGIN_CANCEL, ORACLES_LOGIN_FIELDS };

/* The login's inputs: a physical keyboard's keys, a controller's buttons. */
typedef enum OraclesLoginKey {
    ORACLES_LOGIN_KEY_UP, ORACLES_LOGIN_KEY_DOWN, ORACLES_LOGIN_KEY_LEFT, ORACLES_LOGIN_KEY_RIGHT,
    ORACLES_LOGIN_KEY_TAB, ORACLES_LOGIN_KEY_BACKTAB, ORACLES_LOGIN_KEY_ENTER, ORACLES_LOGIN_KEY_ESCAPE, ORACLES_LOGIN_KEY_BACKSPACE, ORACLES_LOGIN_KEY_SPACE,
    ORACLES_LOGIN_PAD_UP, ORACLES_LOGIN_PAD_DOWN, ORACLES_LOGIN_PAD_LEFT, ORACLES_LOGIN_PAD_RIGHT,
    ORACLES_LOGIN_PAD_A, ORACLES_LOGIN_PAD_B, ORACLES_LOGIN_PAD_X, ORACLES_LOGIN_PAD_Y, ORACLES_LOGIN_PAD_SHOULDER, ORACLES_LOGIN_PAD_START
} OraclesLoginKey;

/* The login opened on User name (`by_on`: On was chosen with no account, which Cancel turns Off again). */
void oracles_achievements_login_open(OraclesHomeNav *nav, int by_on);
/* Closed, what was typed wiped.  Cancel, Escape, B and Back close it too, and while it logs in they give the login sent
 * up: ORACLES_HOME_ACHIEVEMENTS_CANCEL. */
void oracles_achievements_login_close(OraclesHomeNav *nav);
/* The system's text input the login wants, `active` whether it is on now: 0 off, else the field + 1 it is for, whose kind
 * (a user name, a password) it is started with; nav->achievements.text_for is the field it was started for, which its
 * owner sets.  The system's putting it away (Android's Back) holds until a field is chosen again or typed into. */
int oracles_achievements_text_wanted(OraclesHomeNav *nav, int active);
/* The launcher's keyboard is up: the login opened or used with a controller, on a field. */
int oracles_achievements_keyboard_shown(const OraclesHomeNav *nav);
/* A field takes typing: the login open on User name or Password, not while it logs in. */
int oracles_achievements_login_typing(const OraclesHomeNav *nav);
OraclesHomeCommand oracles_achievements_login_key(OraclesHomeNav *nav, OraclesLoginKey key);
/* Text typed into the field highlighted (a physical keyboard's, Android's), whole characters while they fit in 127 bytes. */
void oracles_achievements_login_text(OraclesHomeNav *nav, const char *utf8);
/* Pasted text (the clipboard's) added at the end of a field of `capacity` bytes: the blanks around it dropped (a password
 * manager's copy ends with a line break, a web page's may bring a no-break or zero-width space, a document's a byte
 * order mark), then the whole of it or nothing.  Refused, the field as it was: a control character, a line or paragraph
 * separator or a byte that is no UTF-8 character left in it (a text of several lines is not a password), or more than
 * the field holds (a password cut short would be a wrong one). */
typedef enum OraclesLoginPaste {
    ORACLES_LOGIN_PASTE_DONE,
    ORACLES_LOGIN_PASTE_EMPTY,       /* nothing but blanks */
    ORACLES_LOGIN_PASTE_REFUSED,
    ORACLES_LOGIN_PASTE_TOO_LONG
} OraclesLoginPaste;
OraclesLoginPaste oracles_login_paste_text(char *field, size_t capacity, const char *pasted);
/* The clipboard's text pasted into the field highlighted while one takes typing, as oracles_login_paste_text says, then
 * `pasted` wiped up to its first NUL (its owner frees it): the launcher keeps no other copy, though SDL and the system
 * may have had their own on the way. */
OraclesLoginPaste oracles_achievements_login_paste(OraclesHomeNav *nav, char *pasted);
/* The system's text input: one character as typed (oracles_achievements_login_text), more (a system keyboard's paste or
 * a word it commits) by the paste's rules, whose answer it returns (ORACLES_LOGIN_PASTE_DONE for one character). */
OraclesLoginPaste oracles_achievements_login_typed(OraclesHomeNav *nav, const char *utf8);
/* The few keys the paste and the system keyboard's commits care about, and the modifiers held, in the launcher's terms
 * (home_achievements.c gives SDL's). */
typedef enum OraclesLoginRawKey {
    ORACLES_LOGIN_RAW_OTHER, ORACLES_LOGIN_RAW_V, ORACLES_LOGIN_RAW_INSERT, ORACLES_LOGIN_RAW_PASTE, ORACLES_LOGIN_RAW_RETURN, ORACLES_LOGIN_RAW_TAB
} OraclesLoginRawKey;
enum { ORACLES_LOGIN_MOD_SHIFT = 1u, ORACLES_LOGIN_MOD_CTRL = 2u, ORACLES_LOGIN_MOD_ALT = 4u, ORACLES_LOGIN_MOD_GUI = 8u };
/* Paste's keys: Ctrl+V (Cmd+V too where `apple`), never with Alt (AltGr is Ctrl+Alt on Windows), Shift+Insert alone, a
 * keyboard's Paste key.  1 to paste, -1 for their repeat (held, they paste once), 0 another key. */
int oracles_login_paste_chord(OraclesLoginRawKey key, unsigned mods, int repeat, int apple);
/* A key the system's keyboard made up for a character of the text it commits (Android's: no keyboard, no raw code, sent
 * before the text), Enter or Tab while that text waits in the queue: 1, dropped, so that a paste ending with a line
 * break neither moves to the next field nor logs in before its text is in. */
int oracles_login_made_up_key(OraclesLoginRawKey key, unsigned keyboard, unsigned raw, int text_waiting);
/* The application's window going away: the password hidden again. */
typedef enum OraclesLoginWindow {
    ORACLES_LOGIN_WINDOW_OTHER,        /* nothing that hides it */
    ORACLES_LOGIN_WINDOW_BACKGROUND,   /* the application to the background (Android, iOS) */
    ORACLES_LOGIN_WINDOW_FOCUS_LOST,
    ORACLES_LOGIN_WINDOW_MINIMIZED,
    ORACLES_LOGIN_WINDOW_HIDDEN
} OraclesLoginWindow;
void oracles_achievements_window_changed(OraclesHomeNav *nav, OraclesLoginWindow change);
/* The password shown in letters rather than dots: hidden when the login opens and closes (a successful login included),
 * toggled while it is open, hidden again by oracles_achievements_password_hide (the application in the background). */
void oracles_achievements_password_toggle(OraclesHomeNav *nav);
int oracles_achievements_password_shown(const OraclesHomeNav *nav);
void oracles_achievements_password_hide(OraclesHomeNav *nav);
/* A click: a field (ORACLES_LOGIN_NAME, _PASSWORD) highlighted, the eye (_EYE) toggled, Log in or Cancel pressed; a key of
 * the launcher's keyboard, ORACLES_LOGIN_FIELDS + its row * 10 + its column. */
OraclesHomeCommand oracles_achievements_login_click(OraclesHomeNav *nav, int target);
/* The server's answer: `ok` 1 with the account's name as it spells it, 0 a wrong name or password, -1 no answer. */
void oracles_achievements_login_answered(OraclesHomeNav *nav, int ok, const char *user);
const char *oracles_achievements_login_state_text(const OraclesHomeNav *nav, OraclesTone *tone);

/* The launcher's keyboard: five rows, the last Shift, the page's switch, Space, Paste, the eraser and Done. */
#define ORACLES_KEYBOARD_ROWS 5
#define ORACLES_KEYBOARD_COLUMNS 10
/* The keys of `row` on the page shown (Shift's capitals on the letters), each a text; returns how many. */
unsigned oracles_achievements_keyboard_keys(const OraclesHomeNav *nav, unsigned row, char keys[ORACLES_KEYBOARD_COLUMNS][8]);

typedef struct OraclesUiLoginField {
    OraclesUiBox box, frame;          /* its label and frame together; its frame */
    OraclesUiLine label, value, placeholder;
    OraclesUiBox caret;               /* zero: none */
    OraclesUiBox clip;                /* within its border, before the eye: the value drawn there alone, moved left to keep its end in sight */
    OraclesUiBox eye;                 /* the password's, its room kept while it is empty (`eye_shown` 0); zero: none */
    int eye_shown;
    float eye_size;                   /* the glyph's, in the middle of the box */
} OraclesUiLoginField;
typedef struct OraclesUiLoginLayout {
    OraclesUiBox dialog;
    OraclesUiLine title;
    OraclesUiLoginField fields[2];
    char masked[128 * 3 + 1];         /* the password's dots */
    int shown;                        /* the password in letters: `value` drawn from it, not from `masked` */
    OraclesUiLine note;               /* zero while the keyboard is up */
    OraclesUiBox state;
    OraclesUiBox state_dot;
    OraclesUiLine state_text;
    OraclesUiBox buttons[2];          /* Log in, Cancel; zero while the keyboard is up */
    OraclesUiLine button_names[2];
    OraclesUiBox keyboard;            /* zero while it is down */
    unsigned key_count[ORACLES_KEYBOARD_ROWS];
    OraclesUiBox keys[ORACLES_KEYBOARD_ROWS][ORACLES_KEYBOARD_COLUMNS];
    OraclesUiLine key_names[ORACLES_KEYBOARD_ROWS][ORACLES_KEYBOARD_COLUMNS];
    char key_texts[ORACLES_KEYBOARD_ROWS][ORACLES_KEYBOARD_COLUMNS][8];
} OraclesUiLoginLayout;
void oracles_ui_layout_login(OraclesUiLayout layout, const OraclesHomeNav *nav, OraclesUiLoginLayout *out);
/* The styles of the login and of the keyboard, the pause's glyphs prepared for them. */
const OraclesUiTextStyle *oracles_ui_login_style(OraclesUiLayout layout, int which);
enum { ORACLES_UI_LOGIN_TITLE, ORACLES_UI_LOGIN_LABEL, ORACLES_UI_LOGIN_VALUE, ORACLES_UI_LOGIN_NOTE, ORACLES_UI_LOGIN_STATE, ORACLES_UI_LOGIN_BUTTON,
       ORACLES_UI_LOGIN_KEY, ORACLES_UI_LOGIN_KEY_WORD, ORACLES_UI_LOGIN_STYLES };

/* ---- the toasts over a game --------------------------------------------------------------------------- */

/* One toast: the small line over its text, its text, and an unlock's badge (empty: none), drawn or not as its first frame
 * found it (badge_shown 1 drawn, -1 not, 0 before that frame); an unlock's sound, due at that frame (`sound`). */
typedef struct OraclesAchievementsToast {
    char over[96], text[192], badge[24];
    int badge_shown;
    int sound;
} OraclesAchievementsToast;
#define ORACLES_ACHIEVEMENTS_TOASTS 8
#define ORACLES_ACHIEVEMENTS_TOAST_SHOWN_MS 4000.0
#define ORACLES_ACHIEVEMENTS_TOAST_FADE_MS 600.0
#define ORACLES_ACHIEVEMENTS_TOAST_GAP_MS 250.0
/* A queue: one toast at a time, shown 4 s then faded out over 0.6 s, the next one after it; the oldest waiting dropped
 * past eight.  Their time runs while they are drawn: past a quarter of a second between two frames that drew them (the
 * pause open, a notice over them), the toast shown waits. */
typedef struct OraclesAchievementsToasts {
    OraclesAchievementsToast queue[ORACLES_ACHIEVEMENTS_TOASTS];
    unsigned first, count;
    double since_ms;   /* the first's first frame, 0 before it */
    double drawn_ms;   /* the last frame that asked */
} OraclesAchievementsToasts;
void oracles_achievements_toast_push(OraclesAchievementsToasts *toasts, const char *over, const char *text, const char *badge);
/* The toast shown at `now`, with its opacity, NULL when none waits; the queue moves on as one fades out. */
OraclesAchievementsToast *oracles_achievements_toast_now(OraclesAchievementsToasts *toasts, double now_ms, float *opacity);
/* Whether the toast is laid out with its badge: decided at its first frame (`loaded`, its image there), kept after. */
int oracles_achievements_toast_badge(OraclesAchievementsToast *toast, int loaded);
/* 1 once for an unlock's toast (the first call, at its first frame): its sound is due then, held with it while the
 * toasts wait (the pause); 0 otherwise. */
int oracles_achievements_toast_sound(OraclesAchievementsToast *toast);

/* The toasts' texts: an unlock's ("Achievement unlocked · 10 points" over its title), the game's start ("Oracle of Ages · 12
 * of 55 achievements · Casual", "… · all 55 achievements · Completed · Casual", or suspended by a cheat on at the start),
 * offline, the server unreachable as the game starts, a cheat on, a mod's change. */
void oracles_achievements_toast_unlock(OraclesAchievementsToasts *toasts, const char *title, unsigned points, const char *badge);
void oracles_achievements_toast_start(OraclesAchievementsToasts *toasts, const char *game, unsigned unlocked, unsigned count, int suspended);
void oracles_achievements_toast_offline(OraclesAchievementsToasts *toasts);
void oracles_achievements_toast_unreachable(OraclesAchievementsToasts *toasts);
void oracles_achievements_toast_cheat(OraclesAchievementsToasts *toasts);
void oracles_achievements_toast_mod(OraclesAchievementsToasts *toasts);

/* A toast laid out at the top left of the game's image, (`left`, `bar_bottom`) its left and the bottom of its status bar
 * in the scene, under the reminder while it shows (`reminder`); `badge`, its badge is drawn. */
typedef struct OraclesUiAchievementToastLayout {
    OraclesUiBox box, badge;
    OraclesUiLine over, text;
    float radius;
} OraclesUiAchievementToastLayout;
void oracles_ui_layout_achievement_toast(OraclesUiLayout layout, const OraclesAchievementsToast *toast, int badge, float left, float bar_bottom, int reminder,
                                         OraclesUiAchievementToastLayout *out);
const OraclesUiTextStyle *oracles_ui_toast_style(OraclesUiLayout layout, int over);

/* ---- the pause's list ------------------------------------------------------------------------------------ */

#define ORACLES_ACHIEVEMENTS_MAX 256
/* The server's description is 255 characters at most, of 4 bytes at worst. */
#define ORACLES_ACHIEVEMENT_DESCRIPTION_LENGTH 1024
/* The tag an achievement may have: rcheevos' RC_ACHIEVEMENT_TYPE_*. */
typedef enum OraclesAchievementTag {
    ORACLES_ACHIEVEMENT_TAG_NONE,
    ORACLES_ACHIEVEMENT_TAG_MISSABLE,
    ORACLES_ACHIEVEMENT_TAG_PROGRESSION,
    ORACLES_ACHIEVEMENT_TAG_WIN
} OraclesAchievementTag;
typedef struct OraclesAchievementsItem {
    char title[128], description[ORACLES_ACHIEVEMENT_DESCRIPTION_LENGTH];
    char badge[24];         /* its badge's name, empty: none */
    unsigned points;
    int unlocked;
    char date[16];          /* "21 Sep 2026", unlocked; empty on the home page, which has no session to give it */
    char progress[24];      /* "3/5", empty: none */
    float percent;          /* the progress, 0 to 1 */
    float rarity;           /* the share of players who have it in casual mode, in percent; 0 or 100: none known */
    unsigned tag;           /* OraclesAchievementTag */
} OraclesAchievementsItem;
struct OraclesAchievementsList {
    unsigned count, unlocked, points, points_unlocked;
    unsigned generation;    /* made again: one more */
    char badges[4096];      /* the folder of the badges' images (achievements_badges.h) */
    OraclesAchievementsItem items[ORACLES_ACHIEVEMENTS_MAX];
};

/* The achievement the page's highlight is on, NULL on a row. */
const OraclesAchievementsItem *oracles_achievements_page_item(const OraclesHomeNav *nav);
/* The achievement highlighted on the screen shown, the page or the pause's list; NULL on a row, or any other screen. */
const OraclesAchievementsItem *oracles_achievements_detail_item(const OraclesHomeNav *nav);
/* 16:9: the left column shows the highlighted achievement's detail. */
int oracles_achievements_detail_column(const OraclesHomeNav *nav);
/* The detail shows in a panel, not in the column. */
int oracles_achievements_detail_panel(const OraclesHomeNav *nav);
/* The detail's panel is open over the list. */
int oracles_achievements_detail_open(const OraclesHomeNav *nav);
/* The pause in `width` x `height` pixels, nav->layout set: a reduced 16:9 menu shows only the middle of the scene, at the
 * scale floor; when that cuts the left column off, the detail goes to a panel, kept within what shows. */
void oracles_achievements_detail_view(OraclesHomeNav *nav, int width, int height);
/* The panel closed for good once it no longer shows: a window turned 16:9, the list gone, so that it never opens by itself
 * later on.  The owner of nav->layout calls it after setting it. */
void oracles_achievements_detail_follow(OraclesHomeNav *nav);
/* The rarity as the detail says it, "12.4% of players", or an empty text for none known: 0, what rounds to 0.0, or the 100
 * rcheevos gives for a server's answer without it, what rounds to 100.0 going with it. */
void oracles_achievements_rarity_text(float rarity, char *out, size_t capacity);
/* The tag's name, NULL for none. */
const char *oracles_achievements_tag_text(unsigned tag);
/* The state as the detail says it: "Locked", "Unlocked", or with a date (the pause's) "Unlocked 7 Nov 2026". */
void oracles_achievements_state_text(const OraclesAchievementsItem *item, char *out, size_t capacity);
/* The list opened from the pause, its first achievement highlighted. */
void oracles_achievements_list_open(OraclesHomeNav *nav);
OraclesHomeCommand oracles_achievements_list_act(OraclesHomeNav *nav, OraclesHomeAction action);
/* A click on the pause's achievement `index`: highlighted where it is, and in 4:3 its detail opened. */
void oracles_achievements_list_click(OraclesHomeNav *nav, unsigned index);
/* The wheel or a drag on the pause's list: `steps` rows down (up when negative), stopping at its ends, the highlight kept
 * in sight; with the detail's panel open, the highlight `steps` achievements on instead. */
void oracles_achievements_list_scroll(OraclesHomeNav *nav, int steps);
/* The list's first row shown: nav->achievements.list_first, never past the last rows that fill the list, brought to the
 * highlight only once none of it shows. */
unsigned oracles_achievements_list_first(const OraclesHomeNav *nav);
/* The rows the list shows whole. */
unsigned oracles_achievements_list_fit(const OraclesHomeNav *nav);
/* The summary over the list, and whether it is in the warning's tone. */
void oracles_achievements_list_summary(const OraclesHomeNav *nav, char *out, size_t capacity, int *warning);

#define ORACLES_UI_ACHIEVEMENTS_SHOWN 12
typedef struct OraclesUiAchievementRow {
    unsigned index;
    OraclesUiBox row, badge;
    OraclesUiLine title, description, points, date, progress;
    OraclesUiBox bar;                     /* 16:9's progress bar, zero without */
    char title_text[128], description_text[256], points_text[24], date_text[32];
} OraclesUiAchievementRow;
typedef struct OraclesUiAchievementsLayout {
    OraclesUiPageHead head;               /* 16:9: its section, words and title; 4:3: its section and title */
    OraclesUiWrapped mode;                /* 16:9's note on the casual mode */
    OraclesUiBox panel, list, summary, summary_dot, track, thumb;
    OraclesUiLine summary_text;
    char summary_line[160];
    int warning;
    unsigned first, shown;                /* the first row shown, the rows shown */
    OraclesUiAchievementRow rows[ORACLES_UI_ACHIEVEMENTS_SHOWN];
} OraclesUiAchievementsLayout;
void oracles_ui_layout_achievements_list(OraclesUiLayout layout, const OraclesHomeNav *nav, OraclesUiAchievementsLayout *out);
/* The home page whole: its rows (`page`, moved up by `scroll`, oracles_ui_achievements_page_view), the part that scrolls
 * kept to `clip`; with the list, the rule, the summary and the achievements in sight.  16:9: the panel 948 high, the
 * highlight's achievement in it; 4:3: the rows and the list 662 high, the help at their foot, whole achievements scrolled,
 * five in sight. */
typedef struct OraclesUiAchievementsPageLayout {
    OraclesUiCheatsLayout page;
    int list;                             /* 0: the line under the rows in its place */
    float scroll;
    OraclesUiBox clip, rule, summary_dot;
    OraclesUiLine summary_text;
    char summary_line[160];
    int warning;
    unsigned shown;
    OraclesUiAchievementRow rows[ORACLES_UI_ACHIEVEMENTS_SHOWN];
    char help[256], help_note[40];
} OraclesUiAchievementsPageLayout;
void oracles_ui_layout_achievements_page(OraclesUiLayout layout, const OraclesHomeNav *nav, OraclesUiAchievementsPageLayout *out);
/* An achievement's detail.  Its lines hold whatever the server wrote: a word wider than a line is broken, and a text past
 * the room left is cut with an ellipsis, the state staying in sight. */
#define ORACLES_UI_DETAIL_TITLE_LINES 4
#define ORACLES_UI_DETAIL_LINES 20
#define ORACLES_UI_DETAIL_LINE_LENGTH 256
#define ORACLES_UI_DETAIL_LINE_GLYPHS 128   /* what the drawing of one text holds */
typedef struct OraclesUiAchievementDetail {
    OraclesUiBox panel;                   /* 4:3's, over the list; zero in 16:9's column */
    OraclesUiBox badge, tag, state_dot, bar;   /* `tag` zero without one, `bar` zero without progress */
    OraclesUiLine points, rarity, tag_name, state, progress;
    unsigned title_lines, description_lines;
    OraclesUiLine title[ORACLES_UI_DETAIL_TITLE_LINES], description[ORACLES_UI_DETAIL_LINES];
    char title_text[ORACLES_UI_DETAIL_TITLE_LINES][ORACLES_UI_DETAIL_LINE_LENGTH], description_text[ORACLES_UI_DETAIL_LINES][ORACLES_UI_DETAIL_LINE_LENGTH];
    char points_text[24], rarity_text[32], state_text[32];
} OraclesUiAchievementDetail;
void oracles_ui_layout_achievement_detail(OraclesUiLayout layout, const OraclesAchievementsItem *item, OraclesUiAchievementDetail *out);
/* The panel of a pause showing `view_w` by `view_h` of the 16:9 scene, centred: 24 inside that, above the hints, in 4:3's
 * sizes, read at the scale floor; its row of points wraps when the panel is short of room. */
void oracles_ui_layout_achievement_panel(float view_w, float view_h, const OraclesAchievementsItem *item, OraclesUiAchievementDetail *out);
/* The highlighted achievement's, as the screen shows it. */
void oracles_ui_achievement_detail_of(const OraclesHomeNav *nav, OraclesUiAchievementDetail *out);
/* Under the point of the scene, with the panel open: 1 in the panel. */
int oracles_ui_achievements_detail_hit(const OraclesHomeNav *nav, float x, float y);
/* The pause's achievement under the point of the scene, or -1. */
int oracles_ui_achievements_list_hit(const OraclesHomeNav *nav, float x, float y);
/* The place of the page under a point of the scene, or -1, and in *option its first (0) or second (1) choice, its action (2). */
int oracles_ui_achievements_page_hit(const OraclesHomeNav *nav, float x, float y, int *option);
const OraclesUiTextStyle *oracles_ui_achievements_style(OraclesUiLayout layout, int which);
enum { ORACLES_UI_ACH_SUMMARY, ORACLES_UI_ACH_TITLE, ORACLES_UI_ACH_DESCRIPTION, ORACLES_UI_ACH_POINTS, ORACLES_UI_ACH_DATE, ORACLES_UI_ACH_PROGRESS,
       ORACLES_UI_ACH_DETAIL_TITLE, ORACLES_UI_ACH_DETAIL_POINTS, ORACLES_UI_ACH_DETAIL_RARITY, ORACLES_UI_ACH_DETAIL_TAG, ORACLES_UI_ACH_DETAIL_TEXT,
       ORACLES_UI_ACH_DETAIL_PROGRESS, ORACLES_UI_ACH_STYLES };

#endif
