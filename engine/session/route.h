/* Input routes: the inputs of a session, one key mask per change, anchored to
 * the frame counter of the launcher.  Format: docs/ROUTES.md.
 * Recorded by the launcher from a played session, replayed by the launcher
 * and by the harness. */
#ifndef ORACLES_ROUTE_H
#define ORACLES_ROUTE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define ORACLES_ROUTE_FORMAT 2          /* written without mods; format 1 (the `keys` verb alone) is read too */
#define ORACLES_ROUTE_FORMAT_MODS 3     /* written with a `mods` line: a reader of format 2 would ignore the line and replay without the mods */
#define ORACLES_ROUTE_FORMAT_LANGUAGE 4 /* written with a language among the options: a reader before it would replay in English */
#define ORACLES_ROUTE_FORMAT_SWIM_HOLD 5   /* written with the swim's hold among the options: a reader before it would replay without */
#define ORACLES_ROUTE_FORMAT_DIAGONAL 6    /* written with the transitions' diagonal among the options: the same */
#define ORACLES_ROUTE_FORMAT_CHEATS 7      /* written with a cheat among the options: the same */
#define ORACLES_ROUTE_FORMAT_CHEAT_VERB 8  /* written with a `cheat` line, a cheat changed during the session: a verb a reader before it does not know */
#define ORACLES_ROUTE_FORMAT_TEXT_COMPACT 9   /* written with the compact text among the options: a reader before it would replay without */
#define ORACLES_ROUTE_FORMAT_SET 10        /* written by a session that may change a setting while it runs (a `set` line): a verb a reader before it does not know */
#define ORACLES_ROUTE_FORMAT_TEXT_BOX_WIDE 11  /* written with the wide text box among the options: a reader before it would replay with the game's box */
#define ORACLES_ROUTE_FORMAT_TEXT_INSTANT 12  /* written with the instant text among the options, or a `set text-speed` line: a reader before it would replay the game's pace */
#define ORACLES_ROUTE_FORMAT_STEPS 13      /* written with the transitions' steps among the options: a reader before it would replay without */
#define ORACLES_ROUTE_FORMAT_SOUND 14      /* written by a session that may write a `sound` line (the item hotkeys on, or a pause that turns them on): a verb a reader before it does not know */
#define ORACLES_ROUTE_FORMAT_LATEST ORACLES_ROUTE_FORMAT_SOUND
#define ORACLES_ROUTE_MAX_EVENTS 200000u

typedef struct OraclesRouteHeader {
    char game[16];        /* "ages" or "seasons" */
    char rom_sha1[41];
    char sram_sha1[41];   /* SHA-1 of the SRAM loaded before the run, or "none" */
    char options[192];    /* the gameplay options the session ran with, comma-separated ("continuous-transitions"); empty for none */
    char core[48];        /* how the core ran, comma-separated ("joypad-bouncing-off", "mgba"); empty in the routes recorded before the core's joypad bouncing was cut */
    char mods[1024];      /* the mods the session ran, "NAME@SHA1,..." in the order of their names; empty for none */
    char store_sha1[41];  /* with mods: SHA-1 of the mods' storage the run starts from (ROUTE.store), or "none"; empty in routes before it */
} OraclesRouteHeader;

#define ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS "continuous-transitions"
#define ORACLES_ROUTE_OPTION_CONTINUOUS_SWIM "continuous-swim"   /* with the transitions: Link swimming at the surface too */
#define ORACLES_ROUTE_OPTION_LANGUAGE "language-"   /* and the language's code ("language-fr"): the texts the route was played in */
#define ORACLES_ROUTE_OPTION_SWIM_HOLD "swim-hold"   /* the swim's strokes started by a held button */
#define ORACLES_ROUTE_OPTION_CONTINUOUS_DIAGONAL "continuous-diagonal"   /* with the transitions: a diagonal held carries Link across their axis too */
#define ORACLES_ROUTE_OPTION_CONTINUOUS_STEPS "continuous-steps"   /* with the transitions: Link takes a step a frame through them as the keys give it */
#define ORACLES_ROUTE_OPTION_TEXT_COMPACT "text-compact"   /* the text box's lines drawn in the font's own widths (--text compact) */
#define ORACLES_ROUTE_OPTION_TEXT_BOX_WIDE "text-box-wide"   /* the host's wide text box, its texts laid out again for it, classic or compact (--text-box wide) */
#define ORACLES_ROUTE_OPTION_TEXT_INSTANT "text-instant"   /* each line of the text box whole, as a press of A at its start shows it (--text-speed instant) */
#define ORACLES_ROUTE_OPTION_CHEAT "cheat-"   /* and a cheat's name ("cheat-no-damage"): a cheat on from the start; a `cheat` line changes one later */
/* Whether the header names a gameplay option. */
int oracles_route_has_option(const OraclesRouteHeader *header, const char *name);
/* Whether it names a cheat (an option after ORACLES_ROUTE_OPTION_CHEAT), which the guest's oracles_cheats_parse reads. */
int oracles_route_has_cheat(const OraclesRouteHeader *header);
/* The language the route was played in ("fr"), "" for the game's own English. */
const char *oracles_route_language(const OraclesRouteHeader *header, char *out, size_t capacity);
/* Whether a session in `language` (NULL or "en" for the game's English) replays the route: a text's length paces its
 * dialogue.  A route that names no language replays in any: its rows choose the language (--language). */
int oracles_route_language_matches(const OraclesRouteHeader *header, const char *language);
/* A route recorded before the core's joypad bouncing was cut ran with it, and only replays with it; one recorded since
 * says `core joypad-bouncing-off`. */
#define ORACLES_ROUTE_CORE_JOYPAD_BOUNCING_OFF "joypad-bouncing-off"
int oracles_route_joypad_bouncing(const OraclesRouteHeader *header);
/* A route recorded on mGBA says `core joypad-bouncing-off,mgba` (mGBA has no joypad bouncing): it replays on mGBA, one
 * without it on SameBoy, the core it was recorded on, unless the replay names another. */
#define ORACLES_ROUTE_CORE_MGBA "mgba"
int oracles_route_core_mgba(const OraclesRouteHeader *header);

typedef struct OraclesRouteEvent {
    uint32_t frame;
    unsigned mask;
} OraclesRouteEvent;

/* Format 2, the verbs that change the game's state (the item hotkeys).  `equip`: an
 * exchange of two inventory slots as the write point applied it, each slot
 * with the item it held before, and a variant written; `use`: the start of a
 * simulated button press of the `use` mode, a note for the harness (the
 * press itself is in the `keys` mask).  Format 14, `sound`: a sound the game
 * was asked to play through playSound from the start of that frame (the
 * inventory's equip sound, for an item the hotkeys equipped); `throw`: the start
 * of a press of the `use` mode that throws what Link holds, a note as `use`
 * is (the press is in the `keys` mask), on its button. */
typedef enum OraclesRouteVerb { ORACLES_ROUTE_EQUIP = 1, ORACLES_ROUTE_USE, ORACLES_ROUTE_SOUND, ORACLES_ROUTE_THROW } OraclesRouteVerb;
typedef struct OraclesRouteAction {
    uint32_t frame;
    OraclesRouteVerb verb;
    uint8_t swap, slot_a, item_a, slot_b, item_b;   /* slots: 0 B, 1 A, 2 to 17 the storage (s0 to s15) */
    uint8_t variant, variant_value;                 /* 0 none, 1 satchel, 2 shooter, 3 harp */
    uint8_t button, item;                           /* use: the carrier slot (0 B, 1 A) and the item; throw: the button; sound: the sound in `item` */
} OraclesRouteAction;

/* Format 8, `cheat <name> on|off`: a cheat changed at the start of that frame (from the pause menu), its name as the
 * guest's oracles_cheats_parse reads it ("rupees"), which the replayer checks. */
typedef struct OraclesRouteCheat {
    uint32_t frame;
    char name[16];
    int on;
} OraclesRouteCheat;

/* Format 10, `set <name> <value>`: a setting changed at the start of that frame (from the pause menu).  The names and
 * their values, which the reader checks, an unknown one stopping it as an unknown verb does:
 *   swim-hold off|on          the swim's hold (the options' swim-hold, from the start); it changes the game
 *   item-hotkeys off|use|equip  the item hotkeys' mode: a note, their exchanges being the `equip` lines
 *   window 2|3|4|full         the window's scale, or fullscreen (settings.txt's window_scale=): presentation alone
 *   text-speed classic|instant  the instant text (the options' text-instant, from the start); it changes the game; format 12 */
typedef enum OraclesRouteSettingName { ORACLES_ROUTE_SET_SWIM_HOLD = 0, ORACLES_ROUTE_SET_ITEM_HOTKEYS, ORACLES_ROUTE_SET_WINDOW, ORACLES_ROUTE_SET_TEXT_SPEED,
                                       ORACLES_ROUTE_SETTINGS } OraclesRouteSettingName;
typedef struct OraclesRouteSetting {
    uint32_t frame;
    OraclesRouteSettingName name;
    int value;   /* swim-hold 0 or 1; item-hotkeys an OraclesHotkeysMode (0 off, 1 use, 2 equip); window 2 to 4, 0 fullscreen; text-speed 0 classic, 1 instant */
} OraclesRouteSetting;
/* The name a `set` line writes ("swim-hold"), NULL past the last; `value`'s text for that name ("on"), NULL for a value
 * it does not have. */
const char *oracles_route_setting_name(OraclesRouteSettingName name);
const char *oracles_route_setting_value(OraclesRouteSettingName name, int value);

typedef struct OraclesRoute {
    OraclesRouteHeader header;
    unsigned format;
    OraclesRouteEvent *events;
    size_t count;
    size_t capacity;
    OraclesRouteAction *actions;            /* in the file's order: non-decreasing frames */
    size_t action_count, action_capacity;
    OraclesRouteCheat *cheats;              /* the `cheat` lines, apart from the item hotkeys' actions, in the file's order */
    size_t cheat_count, cheat_capacity;
    OraclesRouteSetting *settings;          /* the `set` lines, in the file's order */
    size_t setting_count, setting_capacity;
} OraclesRoute;

/* Reads a route file; returns 0, or -1 with a message. The caller frees with oracles_route_free. */
int oracles_route_read(const char *path, OraclesRoute *route, char *error, size_t error_capacity);
void oracles_route_free(OraclesRoute *route);

/* The actions of `frame`: the index of the first and their number (in the file's order). */
size_t oracles_route_actions_at(const OraclesRoute *route, uint32_t frame, size_t *first);
/* The `cheat` lines of `frame`, the same. */
size_t oracles_route_cheats_at(const OraclesRoute *route, uint32_t frame, size_t *first);
/* The `set` lines of `frame`, the same. */
size_t oracles_route_settings_at(const OraclesRoute *route, uint32_t frame, size_t *first);
/* Whether the route has a `set` line of `name`. */
int oracles_route_sets(const OraclesRoute *route, OraclesRouteSettingName name);

/* Key mask in force at `frame`; returns 0 when the route has ended (frame is past its last event). */
/* The frame of the route's last line, of whatever verb: the route lasts until then. */
uint32_t oracles_route_last_frame(const OraclesRoute *route);
int oracles_route_mask_at(const OraclesRoute *route, uint32_t frame, unsigned *mask);

/* Recording: writes the header, then one line per mask change. */
typedef struct OraclesRouteWriter {
    FILE *file;
    unsigned last_mask;
    int has_last;
    int format;   /* the header's */
} OraclesRouteWriter;

/* The header in the lowest format that says it, or in `at_least` when that is higher: a session that may write a
 * `cheat` or `set` line later (one with a pause menu, or the extension of a route that has some) opens its route in
 * ORACLES_ROUTE_FORMAT_SET at least, the header being written once, before the first line. */
int oracles_route_writer_open(OraclesRouteWriter *writer, const char *path, const OraclesRouteHeader *header);
int oracles_route_writer_open_format(OraclesRouteWriter *writer, const char *path, const OraclesRouteHeader *header, int at_least);
void oracles_route_writer_record(OraclesRouteWriter *writer, uint32_t frame, unsigned mask);
/* An `equip`, `use`, `sound` or `throw` line; 0, or -1 when the action cannot be written (a slot or a variant out of
 * range, a sound 0, or a sound or a throw in a route opened in a format before 14). */
int oracles_route_writer_action(OraclesRouteWriter *writer, const OraclesRouteAction *action);
/* A `cheat` line; 0, or -1 for a name the line cannot carry or a route opened in a format before 8. */
int oracles_route_writer_cheat(OraclesRouteWriter *writer, uint32_t frame, const char *name, int on);
/* A `set` line; 0, or -1 for a value the name does not have or a route opened in a format before 10 (12 for text-speed). */
int oracles_route_writer_set(OraclesRouteWriter *writer, uint32_t frame, OraclesRouteSettingName name, int value);
int oracles_route_writer_close(OraclesRouteWriter *writer);

#endif
