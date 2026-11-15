/* A session's RetroAchievements (achievements.h), in a build with ORACLES_RETROACHIEVEMENTS: casual mode, achievements=on
 * in settings.txt and an account logged in (achievements.txt, --achievements-login).  The Oracles' own ROMs alone:
 * a fan game has no achievements.  Never with a route replayed or recorded, nor without a window: the routes, the
 * harness and the tests ask no server.  The memory is read between frames, after the frame's hooks have written what
 * they write.  A cheat turned on suspends them until the game starts again; a savestate loaded resets their progress
 * (casual mode allows it); so does a mod's first change of the game (an item given, rupees paid, a flag set).  The
 * unlocks not yet acknowledged are kept (achievements_pending.h), the badges' images fetched into the settings' folder
 * (achievements_badges.h).  With the developer's offline answers, none of the player's files is read or written: the
 * account is "offline", the unlocks and the badges are kept in the answers' directory. */
#include "session_internal.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_timer.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef ORACLES_RETROACHIEVEMENTS

#include "achievements.h"
#include "achievements_account.h"
#include "achievements_badges.h"
#include "achievements_files.h"
#include "achievements_load.h"
#include "achievements_pending.h"
#include "achievements_transport.h"
#include "home_games.h"
#include "achievements_screens.h"
#include "pause.h"

#if defined(_WIN32)
#define PLATFORM "Windows"
#elif defined(__ANDROID__)
#define PLATFORM "Android"
#elif defined(__APPLE__)
#define PLATFORM "macOS"
#elif defined(__linux__)
#define PLATFORM "Linux"
#else
#define PLATFORM "unknown"
#endif

typedef OraclesAchievementsAccount account;

struct OraclesSessionAchievements {
    session *session;
    OraclesAchievements *client;
    oracles_settings *settings;   /* NULL with the offline answers: the player's files are left alone */
    char hash[33];
    account account;   /* as the login answered: the account's own name */
    char pending[ORACLES_SETTINGS_PATH_LENGTH], badges_folder[ORACLES_SETTINGS_PATH_LENGTH];
    OraclesAchievementsBadges badges;
    /* What the player sees: the toasts over the game (the pause draws them) and what they told already; the list the
     * pause shows, made as it opens. */
    OraclesAchievementsToasts *toasts;
    OraclesAchievementsTold told;
    OraclesAchievementsList list;
};

static const char *game_name(const session *s) { return oracles_game_name(s->info.game); }

static void account_path(const oracles_settings *settings, char *path, size_t capacity)
{
    oracles_settings_achievements_path(settings, "achievements.txt", path, capacity);
}

static void read_account(const oracles_settings *settings, account *a)
{
    char path[ORACLES_SETTINGS_PATH_LENGTH];
    account_path(settings, path, sizeof path);
    oracles_achievements_account_read(path, a);
}

static int write_account(const oracles_settings *settings, const account *a)
{
    char path[ORACLES_SETTINGS_PATH_LENGTH];
    account_path(settings, path, sizeof path);
    return oracles_achievements_account_write(path, a);
}

/* The login's answer: the account's own name and token kept (a token refused forgotten), the unlocks of earlier sessions
 * sent again; then, the game known, its badges asked for; each unlock kept until it is acknowledged. */
static void on_event(void *opaque, const OraclesAchievementsEvent *event)
{
    OraclesSessionAchievements *sa = opaque;
    const int64_t now = (int64_t)time(NULL);
    if (sa->toasts) oracles_achievements_tell(sa->toasts, &sa->told, sa->client, game_name(sa->session), event);
    if (event->kind == ORACLES_ACHIEVEMENTS_LOGGED_IN) {
        account kept = sa->account;
        if (sa->settings) read_account(sa->settings, &kept);
        snprintf(sa->account.user, sizeof sa->account.user, "%s", event->user && event->user[0] ? event->user : kept.user);
        snprintf(sa->account.token, sizeof sa->account.token, "%s", event->token ? event->token : "");
        if (sa->settings && sa->settings->path[0] && (strcmp(kept.user, sa->account.user) != 0 || strcmp(kept.token, sa->account.token) != 0)
            && !write_account(sa->settings, &sa->account))
            fprintf(stderr, "oracles: achievements: the account's token could not be written\n");
        const unsigned sent = oracles_achievements_pending_resend(sa->client, sa->pending, sa->account.user, sa->account.token, now);
        if (sent) fprintf(stderr, "oracles: achievements: %u unlock(s) of earlier games sent again\n", sent);
    } else if (event->kind == ORACLES_ACHIEVEMENTS_LOGIN_FAILED && event->token_refused && sa->settings && sa->settings->path[0]) {
        account kept;
        read_account(sa->settings, &kept);
        kept.token[0] = 0;
        write_account(sa->settings, &kept);
        fprintf(stderr, "oracles: achievements: the token is forgotten; log in again\n");
    } else if (event->kind == ORACLES_ACHIEVEMENTS_GAME_LOADED && sa->badges_folder[0]) {
        if (SDL_CreateDirectory(sa->badges_folder)) oracles_achievements_fetch_badges(sa->client, sa->badges_folder, &sa->badges);
        else fprintf(stderr, "oracles: achievements: %s cannot be made: %s\n", sa->badges_folder, SDL_GetError());
    } else if (sa->pending[0] && sa->account.user[0]) {
        oracles_achievements_pending_follow(sa->pending, sa->account.user, sa->hash, event, now);
    }
}

/* The transport: the developer's answers from a directory, else the platform's. */
static int transport_for(const char *offline, OraclesAchievementsTransport *transport)
{
    if (offline) return oracles_achievements_offline_transport(offline, transport);
    if (oracles_achievements_platform_transport(transport)) return 1;
    fprintf(stderr, "oracles: achievements: the network's thread cannot start; they stay off\n");
    return 0;
}

static OraclesAchievements *client_for(const char *offline, const char *core_version, OraclesAchievementsNotify notify, void *opaque)
{
    OraclesAchievementsTransport transport;
    if (!transport_for(offline, &transport)) return NULL;
    char user_agent[160];
    oracles_achievements_user_agent(ORACLES_PRODUCT_VERSION, PLATFORM, core_version, user_agent, sizeof user_agent);
    OraclesAchievements *a = oracles_achievements_create(&transport, user_agent, notify, opaque);
    if (!a && transport.destroy) transport.destroy(transport.opaque);
    return a;
}

void oracles_session_achievements_hash(session *s, const OraclesSessionOptions *o)
{
    s->achievements_hash[0] = 0;
    if (!o->achievements_offline && !s->settings->achievements) return;
    /* The developer's offline answers ask no server: they may watch a route replayed or a run without a window. */
    if (!o->achievements_offline && (o->no_window || o->play_path || o->record_path)) {
        fprintf(stderr, "oracles: achievements: off while a route is replayed or recorded, and without a window\n");
        return;
    }
    if (o->patch_path || !oracles_rom_is_original(&s->info)) {
        fprintf(stderr, "oracles: achievements: none for this game (RetroAchievements has sets for Ages and Seasons alone)\n");
        return;
    }
    oracles_achievements_hash(s->rom, s->rom_size, s->achievements_hash);
}

void oracles_session_achievements_start(session *s, const OraclesSessionOptions *o)
{
    if (!s->achievements_hash[0]) return;
    account a;
    if (o->achievements_offline) { snprintf(a.user, sizeof a.user, "offline"); snprintf(a.token, sizeof a.token, "offline"); }
    else read_account(s->settings, &a);
    if (!a.user[0] || !a.token[0]) {
        fprintf(stderr, "oracles: achievements: on, but no account is logged in (the-oracles-project --achievements-login NAME)\n");
        return;
    }
    OraclesSessionAchievements *sa = calloc(1, sizeof *sa);
    if (!sa) return;
    sa->account = a;
    sa->session = s;
    snprintf(sa->hash, sizeof sa->hash, "%s", s->achievements_hash);
    if (o->achievements_offline) {
        snprintf(sa->pending, sizeof sa->pending, "%s/achievements-pending.txt", o->achievements_offline);
        snprintf(sa->badges_folder, sizeof sa->badges_folder, "%s/achievement-badges", o->achievements_offline);
    } else {
        sa->settings = s->settings;
        oracles_settings_achievements_path(s->settings, "achievements-pending.txt", sa->pending, sizeof sa->pending);
        oracles_settings_achievements_path(s->settings, "achievement-badges", sa->badges_folder, sizeof sa->badges_folder);
    }
    sa->client = client_for(o->achievements_offline, oracles_core_version(s->core), on_event, sa);
    if (!sa->client) { free(sa); return; }
    s->achievements = sa;
    sa->toasts = oracles_pause_achievements(s->pause, sa->badges_folder);
    if (s->cheats) oracles_session_achievements_cheats(s);
    if (o->start_house) oracles_achievements_suspend(sa->client, ORACLES_ACHIEVEMENTS_MODS_REASON);   /* the save moved by a mod */
    oracles_achievements_login(sa->client, a.user, NULL, a.token);
    oracles_achievements_load_game(sa->client, s->achievements_hash, s->core);
}

void oracles_session_achievements_frame(session *s)
{
    OraclesSessionAchievements *sa = s->achievements;
    if (!sa) return;
    /* The calls a mod asked for this frame are queued or done: none of their effects is ever evaluated. */
    if (oracles_mod_session_changed_game(s->mod)) oracles_achievements_suspend(sa->client, ORACLES_ACHIEVEMENTS_MODS_REASON);
    oracles_achievements_frame(sa->client);
    /* A suspension once the game runs, said once: a mod's at once, a cheat's as the game resumes from the pause. */
    if (sa->toasts) oracles_achievements_tell_suspension(sa->toasts, &sa->told, sa->client);
}

void oracles_session_achievements_idle(void *opaque)
{
    session *s = opaque;
    if (s->achievements) oracles_achievements_idle(s->achievements->client);
}

void oracles_session_achievements_reset(session *s)
{
    if (s->achievements) oracles_achievements_reset(s->achievements->client);
}

void oracles_session_achievements_cheats(session *s)
{
    if (s->achievements) oracles_achievements_suspend(s->achievements->client, ORACLES_ACHIEVEMENTS_CHEATS_REASON);
}

const char *oracles_session_achievements_suspension(const session *s)
{
    return s->achievements ? oracles_achievements_suspension(s->achievements->client) : NULL;
}

/* The unlocks of `user` waiting in `path`. */
static unsigned pending_of(const char *path, const char *user)
{
    if (!path[0] || !user[0]) return 0;
    OraclesAchievementsPendingUnlock *kept = malloc(ORACLES_ACHIEVEMENTS_PENDING_MAX * sizeof *kept);
    if (!kept) return 0;
    const size_t count = oracles_achievements_pending_read(path, kept, ORACLES_ACHIEVEMENTS_PENDING_MAX);
    unsigned mine = 0;
    for (size_t i = 0; i < count; i++) mine += !strcmp(kept[i].user, user);
    free(kept);
    return mine;
}

unsigned oracles_session_achievements_pending(const session *s)
{
    return s->achievements ? pending_of(s->achievements->pending, s->achievements->account.user) : 0;
}

static void list_date(int64_t when, char *out, size_t capacity) { oracles_home_format_date((time_t)when, out, capacity); }

/* The list the pause shows, made as it opens. */
const OraclesAchievementsList *oracles_session_achievements_list(session *s)
{
    OraclesSessionAchievements *sa = s->achievements;
    if (!sa) return NULL;
    return oracles_achievements_list_make(sa->client, sa->badges_folder, list_date, &sa->list) ? &sa->list : NULL;
}

int oracles_session_achievements_suspended(const session *s) { return s->achievements ? oracles_achievements_suspension_kind(s->achievements->client) : 0; }

void oracles_session_achievements_stop(session *s)
{
    if (!s->achievements) return;
    oracles_achievements_destroy(s->achievements->client);   /* the transport first: no answer comes after */
    free(s->achievements);
    s->achievements = NULL;
}

/* The answers waited for, a tenth of a second at a time, for `seconds` at most. */
static void wait_for(OraclesAchievements *a, const int *done, unsigned seconds)
{
    for (unsigned i = 0; !*done && i < seconds * 10u; i++) {
        oracles_achievements_idle(a);
        if (!*done) SDL_Delay(100);
    }
}

typedef struct login {
    oracles_settings *settings;   /* NULL with the offline answers: nothing written */
    int done, ok;
} login;

static void on_login(void *opaque, const OraclesAchievementsEvent *event)
{
    login *l = opaque;
    if (event->kind != ORACLES_ACHIEVEMENTS_LOGGED_IN && event->kind != ORACLES_ACHIEVEMENTS_LOGIN_FAILED) return;
    l->done = 1;
    l->ok = event->kind == ORACLES_ACHIEVEMENTS_LOGGED_IN && event->user && event->user[0] && event->token && event->token[0];
    if (!l->ok) return;
    if (!l->settings) { fprintf(stderr, "oracles: achievements: logged in as %s with the offline answers: nothing kept\n", event->user); return; }
    account a;
    snprintf(a.user, sizeof a.user, "%s", event->user);   /* the account's own name, as the server writes it */
    snprintf(a.token, sizeof a.token, "%s", event->token);
    if (!write_account(l->settings, &a)) { fprintf(stderr, "oracles: achievements: the account could not be written\n"); l->ok = 0; }
}

/* Not left in the stack's memory: the compiler may not drop a store through a volatile pointer. */
static void wipe(char *text, size_t size)
{
    volatile char *v = text;
    while (size--) *v++ = 0;
}

/* A line of the standard input without its echo where it is a terminal (a console on Windows). */
static int read_hidden(char *out, size_t capacity)
{
#if defined(_WIN32)
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    const int console = in != INVALID_HANDLE_VALUE && GetConsoleMode(in, &mode);
    if (console) SetConsoleMode(in, mode & ~(DWORD)ENABLE_ECHO_INPUT);
#else
    struct termios mode, hidden;
    const int console = isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &mode) == 0;
    if (console) { hidden = mode; hidden.c_lflag &= ~(tcflag_t)ECHO; tcsetattr(STDIN_FILENO, TCSAFLUSH, &hidden); }
#endif
    const int got = fgets(out, (int)capacity, stdin) != NULL;
#if defined(_WIN32)
    if (console) SetConsoleMode(in, mode);
#else
    if (console) tcsetattr(STDIN_FILENO, TCSAFLUSH, &mode);
#endif
    if (console) fputc('\n', stderr);
    return got;
}

int oracles_session_achievements_login(oracles_settings *settings, const char *user, const char *offline)
{
    if (!settings->path[0]) { fprintf(stderr, "oracles: achievements: no settings directory to keep the account in\n"); return 1; }
    fprintf(stderr, "oracles: the RetroAchievements password of %s, on the standard input (sent once to log in, never kept): ", user);
    char password[ORACLES_ACHIEVEMENTS_ACCOUNT_FIELD];
    if (!read_hidden(password, sizeof password)) { fprintf(stderr, "\noracles: achievements: no password read\n"); return 1; }
    password[strcspn(password, "\r\n")] = 0;
    login l = { offline ? NULL : settings, 0, 0 };
    OraclesAchievements *a = client_for(offline, NULL, on_login, &l);
    if (!a) { wipe(password, sizeof password); return 1; }
    oracles_achievements_login(a, user, password, NULL);
    wipe(password, sizeof password);
    wait_for(a, &l.done, 30u);
    oracles_achievements_destroy(a);
    if (!l.done) fprintf(stderr, "oracles: achievements: the server did not answer\n");
    if (l.ok && !offline) { settings->achievements = 1; oracles_settings_store(settings); fprintf(stderr, "oracles: achievements: on; the token is kept, the password is not\n"); }
    return l.ok ? 0 : 1;
}

/* ---- the home screen's account (session.h) ---------------------------------------------------------------- */

void oracles_session_achievements_account(const oracles_settings *settings, char *user, size_t capacity, int *refused, unsigned *pending)
{
    account a;
    read_account(settings, &a);
    /* A name without its token: the server refused the token, which was forgotten. */
    snprintf(user, capacity, "%.*s", (int)capacity - 1, a.token[0] ? a.user : "");
    *refused = a.user[0] && !a.token[0];
    char path[ORACLES_SETTINGS_PATH_LENGTH];
    oracles_settings_achievements_path(settings, "achievements-pending.txt", path, sizeof path);
    *pending = a.token[0] ? pending_of(path, a.user) : 0u;
}

void oracles_session_achievements_logout(const oracles_settings *settings)
{
    char path[ORACLES_SETTINGS_PATH_LENGTH];
    account_path(settings, path, sizeof path);
    if (path[0] && !SDL_RemovePath(path) && oracles_achievements_file_exists(path)) fprintf(stderr, "oracles: achievements: %s cannot be removed: %s\n", path, SDL_GetError());
    else fprintf(stderr, "oracles: achievements: logged out\n");
}

OraclesSessionLogin *oracles_session_achievements_login_begin(oracles_settings *settings, const char *user, const char *password, const char *offline)
{
    char path[ORACLES_SETTINGS_PATH_LENGTH] = "", user_agent[160];
    if (!offline) account_path(settings, path, sizeof path);   /* the offline answers keep nothing */
    oracles_achievements_user_agent(ORACLES_PRODUCT_VERSION, PLATFORM, NULL, user_agent, sizeof user_agent);
    OraclesAchievementsTransport transport;
    const int ready = transport_for(offline, &transport);
    return oracles_achievements_home_login_begin(ready ? &transport : NULL, user_agent, offline ? NULL : path, user, password, (double)SDL_GetTicks());
}

int oracles_session_achievements_login_poll(OraclesSessionLogin *l, char *user, size_t capacity)
{
    return oracles_achievements_home_login_poll(l, (double)SDL_GetTicks(), user, capacity);
}

void oracles_session_achievements_login_end(OraclesSessionLogin *l) { oracles_achievements_home_login_end(l); }

/* ---- the home screen's list (session.h) --------------------------------------------------------------------- */

OraclesAchievementsLoad *oracles_session_achievements_load_begin(oracles_settings *settings, const char *rom_path, const char *offline)
{
    account a;
    if (offline) { snprintf(a.user, sizeof a.user, "offline"); snprintf(a.token, sizeof a.token, "offline"); }
    else read_account(settings, &a);
    char hash[33], path[ORACLES_SETTINGS_PATH_LENGTH] = "", badges[ORACLES_SETTINGS_PATH_LENGTH], user_agent[160];
    if (!a.user[0] || !a.token[0] || !oracles_achievements_load_hash(rom_path, hash)) return NULL;
    if (offline) snprintf(badges, sizeof badges, "%s/achievement-badges", offline);
    else {
        account_path(settings, path, sizeof path);
        oracles_settings_achievements_path(settings, "achievement-badges", badges, sizeof badges);
    }
    if (!SDL_CreateDirectory(badges)) { fprintf(stderr, "oracles: achievements: %s cannot be made: %s\n", badges, SDL_GetError()); badges[0] = 0; }
    oracles_achievements_user_agent(ORACLES_PRODUCT_VERSION, PLATFORM, NULL, user_agent, sizeof user_agent);
    /* The page's client: leaving the page never waits for a request under way. */
    OraclesAchievementsTransport transport;
    if (offline ? !oracles_achievements_offline_transport(offline, &transport) : !oracles_achievements_page_transport(&transport)) return NULL;
    return oracles_achievements_load_begin(&transport, user_agent, &a, path, hash, badges, (double)SDL_GetTicks());
}

#else

void oracles_session_achievements_hash(session *s, const OraclesSessionOptions *o)
{
    s->achievements_hash[0] = 0;
    if (s->settings->achievements || o->achievements_offline) fprintf(stderr, "oracles: achievements: this build has none (ORACLES_RETROACHIEVEMENTS)\n");
}

void oracles_session_achievements_start(session *s, const OraclesSessionOptions *o) { (void)s; (void)o; }
void oracles_session_achievements_frame(session *s) { (void)s; }
void oracles_session_achievements_idle(void *opaque) { (void)opaque; }
void oracles_session_achievements_reset(session *s) { (void)s; }
void oracles_session_achievements_cheats(session *s) { (void)s; }
const char *oracles_session_achievements_suspension(const session *s) { (void)s; return NULL; }
unsigned oracles_session_achievements_pending(const session *s) { (void)s; return 0; }
void oracles_session_achievements_stop(session *s) { (void)s; }

int oracles_session_achievements_login(oracles_settings *settings, const char *user, const char *offline)
{
    (void)settings; (void)user; (void)offline;
    fprintf(stderr, "oracles: achievements: this build has none (ORACLES_RETROACHIEVEMENTS)\n");
    return 1;
}

#endif
