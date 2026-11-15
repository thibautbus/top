/* RetroAchievements, in casual mode alone: rcheevos' rc_client (third_party/rcheevos) watching the live core's memory
 * once a frame, between frames, and asking RetroAchievements' server through a transport the caller gives.  The
 * player's account: a password is sent once, at a login, and never kept; the token the server answers is what the
 * caller stores, and every later session logs in with it.
 *
 * Nothing here opens a connection or a file: the transport carries every request, and the caller keeps the token. */
#ifndef ORACLES_LAUNCHER_ACHIEVEMENTS_H
#define ORACLES_LAUNCHER_ACHIEVEMENTS_H

#include "core.h"

#include <stddef.h>
#include <stdint.h>

/* The product's part of the User-Agent every request carries, before the core's and rcheevos' own: the application
 * and its version, nothing of the player or the machine. */
#define ORACLES_ACHIEVEMENTS_PRODUCT "the-oracles-project"
/* From this id up, an entry of the game's set is the server's warning to a client it does not know, not an achievement
 * (rcheevos' RC_CLIENT_ACHIEVEMENT_WARNING_ID): it says hardcore cannot be earned here, and this launcher is casual. */
#define ORACLES_ACHIEVEMENTS_WARNING_ID 101000001u

/* A transport's answer to one request: the HTTP status and the body (`length` bytes, not terminated), or status 0 and
 * no body when no server answered (offline, a refused connection, a timeout), or -1 when the transport gave the
 * request up (it is being destroyed). */
typedef void (*OraclesAchievementsAnswer)(void *answer_opaque, int http_status, const uint8_t *body, size_t length);

typedef struct OraclesAchievementsTransport {
    /* Sends one HTTPS request to `url`: a POST of `post_data` as `content_type` when post_data is not NULL, a GET
     * otherwise (a badge's image); `user_agent` its User-Agent.  `answer` is called once for each request, from send
     * itself or from a later poll, never from another thread. */
    void (*send)(void *opaque, const char *url, const char *post_data, const char *content_type, const char *user_agent,
                 OraclesAchievementsAnswer answer, void *answer_opaque);
    /* Calls the answers of the requests finished since the last poll; NULL for a transport that answers in send. */
    void (*poll)(void *opaque);
    void (*destroy)(void *opaque);   /* NULL: nothing to free; the answers not called yet are, with -1 */
    void *opaque;
    /* A GET of an image the transport writes to `path` itself, off the game's thread, when it answers status 200 with a
     * PNG, none asked when `path` exists already; `answer` gets the status and no body.  NULL: the client writes it. */
    void (*save)(void *opaque, const char *url, const char *path, const char *user_agent, OraclesAchievementsAnswer answer, void *answer_opaque);
} OraclesAchievementsTransport;

/* What the session learns, as it happens: said on the standard error, and given to the caller. */
typedef enum OraclesAchievementsEventKind {
    ORACLES_ACHIEVEMENTS_LOGGED_IN,      /* text: the name shown; user: the account's own name and token its token, to keep */
    ORACLES_ACHIEVEMENTS_LOGIN_FAILED,   /* text: why; token_refused: the token kept is no longer good, to forget; unreachable:
                                          * no server answered */
    ORACLES_ACHIEVEMENTS_GAME_LOADED,    /* text: the game's title; count, unlocked: its achievements, those already earned */
    ORACLES_ACHIEVEMENTS_GAME_FAILED,    /* text: why (a ROM RetroAchievements does not know, no network: unreachable) */
    ORACLES_ACHIEVEMENTS_UNLOCKED,       /* text: the achievement's title; id, points; badge: its image's name; delivered: its award
                                          * already settled for good, nothing left to send (AWARD_SETTLED came first) */
    ORACLES_ACHIEVEMENTS_GAME_COMPLETED, /* text: the game's title */
    ORACLES_ACHIEVEMENTS_SERVER_ERROR,   /* text: the request and the server's message */
    /* An unlock's award (rc_client's, or one sent again) answered for good by the server's JSON: id; delivered, the
     * server has it (one it already had too); else refused, text the server's reason, token_refused when the token was
     * (HTTP 401 or 403).  No answer, or one that is not the server's JSON (a captive portal's page, an error 5xx), is
     * no event: the unlock stays to be sent again. */
    ORACLES_ACHIEVEMENTS_AWARD_SETTLED,
    ORACLES_ACHIEVEMENTS_OFFLINE         /* an unlock could not be sent: it is sent again when the server answers */
} OraclesAchievementsEventKind;

typedef struct OraclesAchievementsEvent {
    OraclesAchievementsEventKind kind;
    const char *text, *user, *token, *badge;
    unsigned id, points, count, unlocked;
    int token_refused, delivered, unreachable;
} OraclesAchievementsEvent;

typedef void (*OraclesAchievementsNotify)(void *opaque, const OraclesAchievementsEvent *event);

/* The live core's memory where rcheevos' map of the Game Boy Color puts it: $0000-$FFFF as the CPU sees it, but $D000
 * always WRAM bank 1 and $A000 the cartridge RAM's bank 0, then WRAM banks 2 to 7 at $10000-$15FFF and the cartridge
 * RAM's banks 1 to 15 at $16000-$33FFF.  Taken from the core before each frame's evaluation (the banks it maps). */
typedef struct OraclesAchievementsMemory {
    const uint8_t *rom, *wram, *vram, *oam, *hram, *cart_ram;
    size_t rom_size, cart_ram_size;
    uint16_t rom_bank, vram_bank;
    OraclesCore *core;   /* the registers, $FF00-$FF7F and $FFFF, through oracles_core_peek; NULL: they read 0 */
} OraclesAchievementsMemory;

/* The memory of `core`, the live instance (never a ghost's). */
void oracles_achievements_memory_of(OraclesCore *core, OraclesAchievementsMemory *memory);
/* `count` bytes from `address` of that map into `buffer`: the number read, fewer at the first address nothing backs
 * (a cartridge RAM's bank the cartridge does not have, past $33FFF), which rcheevos takes for an invalid address. */
uint32_t oracles_achievements_read(const OraclesAchievementsMemory *memory, uint32_t address, uint8_t *buffer, uint32_t count);
/* The address in that map of the CPU's `address` with WRAM bank `wram_bank` at $D000 (the tables' bank). */
uint32_t oracles_achievements_address(uint8_t wram_bank, uint16_t address);

/* The game's identity for RetroAchievements: the MD5 of the image as the cartridge holds it (a fan game's after its
 * patch), before a mod's houses or a language change it; 32 hex digits and a 0. */
void oracles_achievements_hash(const uint8_t *rom, size_t size, char hash[33]);

/* The User-Agent: ORACLES_ACHIEVEMENTS_PRODUCT/`version` (`platform`), the core's name and version, rcheevos'. */
void oracles_achievements_user_agent(const char *version, const char *platform, const char *core_version, char *out, size_t capacity);

typedef struct OraclesAchievements OraclesAchievements;

/* A client, casual mode, nobody logged in, no game; NULL out of memory.  The transport is the client's from here:
 * destroy frees it. */
OraclesAchievements *oracles_achievements_create(const OraclesAchievementsTransport *transport, const char *user_agent,
                                                 OraclesAchievementsNotify notify, void *notify_opaque);
void oracles_achievements_destroy(OraclesAchievements *a);
/* Logs in with the account's name and `token`, or, once, with `password` (token NULL); the answer comes as an event. */
void oracles_achievements_login(OraclesAchievements *a, const char *user, const char *password, const char *token);
/* Asks for the game whose identity is `hash` (oracles_achievements_hash), after the login; the answer comes as an
 * event.  `memory` is where its achievements read, refreshed by each frame. */
void oracles_achievements_load_game(OraclesAchievements *a, const char hash[33], OraclesCore *memory);
/* The memory the achievements read, refreshed from the core of load_game before each frame; filled by the caller where
 * there is no core (the tests). */
OraclesAchievementsMemory *oracles_achievements_memory(OraclesAchievements *a);
/* Between two frames, after one ran: the achievements evaluated against the memory as the frame left it. */
void oracles_achievements_frame(OraclesAchievements *a);
/* While no frame runs (the pause): the answers that came, and what rc_client has to send; about once a second. */
void oracles_achievements_idle(OraclesAchievements *a);
/* The game's state replaced (a savestate loaded): each achievement's progress starts again from that state, and one
 * whose conditions hold there waits for them to fail once before it can unlock (rc_client_reset). */
void oracles_achievements_reset(OraclesAchievements *a);
/* The reasons that suspend the achievements, for the player: a cheat turned on, and a mod's first change of the game
 * (an item given or taken, rupees paid, a flag set; its houses, texts and drawings do not suspend them). */
#define ORACLES_ACHIEVEMENTS_CHEATS_REASON "Achievements are paused until the game starts again: a cheat was turned on."
#define ORACLES_ACHIEVEMENTS_MODS_REASON "Achievements are paused until the game starts again: a mod changed the game."
/* No more evaluation for the rest of the session, `why` said (the player's reason, for the interface); the unlocks
 * waiting are still sent.  Once only: the first reason stays. */
void oracles_achievements_suspend(OraclesAchievements *a, const char *why);
/* The reason, or NULL while the achievements are evaluated. */
const char *oracles_achievements_suspension(const OraclesAchievements *a);
/* An unlock kept from an earlier session sent again: the account's `user` and `token`, the game's `hash`, the
 * achievement, earned `seconds` ago; its answer comes as AWARD_SETTLED, or none when no server answers. */
void oracles_achievements_resend(OraclesAchievements *a, const char *user, const char *token, const char *hash, uint32_t id, uint32_t seconds);
/* Each achievement of the game loaded with a badge: its name and the addresses of its two images, earned and locked. */
typedef void (*OraclesAchievementsBadge)(void *opaque, const char *name, const char *unlocked_url, const char *locked_url);
void oracles_achievements_badges(OraclesAchievements *a, OraclesAchievementsBadge badge, void *opaque);
/* The tests' server ("http://127.0.0.1:PORT") in place of RetroAchievements'. */
void oracles_achievements_set_host(OraclesAchievements *a, const char *host);
/* That server, NULL for RetroAchievements'. */
const char *oracles_achievements_host(const OraclesAchievements *a);
/* A request of the caller's through the client's transport, answered as the transport's, for what rc_client does not ask. */
void oracles_achievements_send(OraclesAchievements *a, const char *url, const char *post_data, const char *content_type, OraclesAchievementsAnswer answer, void *answer_opaque);
/* A badge's image at `url` kept as `path` (the transport's save, else written here), its status to `answer`. */
void oracles_achievements_save(OraclesAchievements *a, const char *url, const char *path, OraclesAchievementsAnswer answer, void *answer_opaque);
/* An achievement of the game loaded, as the pause's list shows it: its title, description and badge's name, its points,
 * whether it is unlocked and when, its progress ("3/10", empty without) and its fraction, the share of players who have it
 * in casual mode (as rcheevos reads it: 100 when the server gave none) and its type (RC_ACHIEVEMENT_TYPE_*). */
typedef struct OraclesAchievementsEntry {
    const char *title, *description, *badge, *progress;
    unsigned id, points;
    int unlocked;
    int64_t unlock_time;
    float percent, rarity;
    unsigned type;
} OraclesAchievementsEntry;
typedef void (*OraclesAchievementsEach)(void *opaque, const OraclesAchievementsEntry *entry);
/* Each achievement of the game's core set: the unlocked first (those of the last minutes first, then in the set's order),
 * then those in progress (the nearest to done first), then the locked in the set's order (an achievement rcheevos cannot
 * evaluate, an address the map does not have, among them); and how many there are, unlocked, and their points. */
void oracles_achievements_each(OraclesAchievements *a, OraclesAchievementsEach each, void *opaque);
void oracles_achievements_progress(OraclesAchievements *a, unsigned *count, unsigned *unlocked, unsigned *points, unsigned *points_unlocked);

/* 1 once a game's achievements are loaded and watched. */
int oracles_achievements_watching(const OraclesAchievements *a);

#endif
