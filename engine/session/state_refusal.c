#include "state_refusal.h"

#include "route.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

int oracles_state_transitions_refusal(int taken_with_transitions, int enhanced, int transitions,
                                      char *message, size_t message_capacity, char *detail, size_t detail_capacity)
{
    if (message_capacity) message[0] = 0;
    if (detail_capacity) detail[0] = 0;
    transitions = enhanced && transitions;
    if ((taken_with_transitions != 0) == (transitions != 0)) return 0;
    if (taken_with_transitions && !enhanced) {
        /* In Faithful the transitions are greyed: the profile is what to change. */
        snprintf(message, message_capacity, "Refused: taken in Enhanced: choose Enhanced in Display's Profile from the home screen");
        snprintf(detail, detail_capacity, "the savestate was taken in Enhanced with continuous transitions, and this session is Faithful "
                 "(--continuous-transitions, or the Enhanced profile in the launcher's Display): it loads only there");
        return 1;
    }
    const char *taken = taken_with_transitions ? "on" : "off", *now = transitions ? "on" : "off";
    snprintf(message, message_capacity, "Refused: taken with transitions %s: choose %s in Display \xe2\x80\xba Advanced \xe2\x80\xba Continuous transitions from the home screen", taken,
             taken_with_transitions ? "On" : "Off");
    snprintf(detail, detail_capacity, "the savestate was taken with continuous transitions %s, and this session has them %s "
             "(--continuous-transitions, or Continuous transitions in the launcher's Display): it loads only with them %s", taken, now, taken);
    return 1;
}

/* A core's name for the player, by its version's family ("sameboy-...", "mgba-..."), and the name --core takes. */
static const char *core_label(const char *version) { return !strncmp(version, "mgba-", 5) ? "Fast (mGBA)" : "Accurate (SameBoy)"; }
static const char *core_option(const char *version) { return !strncmp(version, "mgba-", 5) ? "mgba" : "sameboy"; }
static const char *core_name(const char *version) { return !strncmp(version, "mgba-", 5) ? "Fast" : "Accurate"; }

int oracles_state_core_refusal(const char *taken_on, const char *now, char *message, size_t message_capacity, char *detail, size_t detail_capacity)
{
    if (message_capacity) message[0] = 0;
    if (detail_capacity) detail[0] = 0;
    if (!strcmp(core_option(taken_on), core_option(now))) return 0;
    snprintf(message, message_capacity, "Refused: taken on %s: choose %s in Display \xe2\x80\xba Advanced \xe2\x80\xba Core from the home screen", core_label(taken_on),
             core_name(taken_on));
    snprintf(detail, detail_capacity, "the savestate was taken on %s (%s) and this game runs on %s (%s): choose %s in Display's Core, or --core %s",
             core_label(taken_on), taken_on, core_label(now), now, core_label(taken_on), core_option(taken_on));
    return 1;
}

/* The text a set of mods and gameplay options lays out (its word text-classic|compact[-WxL]@digest, apart by commas or
 * spaces; text-compact@ for the compact text in the game's box): compact or classic, in the wide box or the game's,
 * the word; the classic text in the game's box, the word empty, without one. */
typedef struct text_layout { int compact, wide; char word[64]; } text_layout;

static text_layout layout_of(const char *mods)
{
    text_layout l = { 0, 0, "" };
    for (const char *at = mods; (at = strstr(at, "text-")) != NULL; at += 5) {
        if (at != mods && at[-1] != ',' && at[-1] != ' ') continue;
        const char *p = at + 5;
        const int compact = !strncmp(p, "compact", 7);
        if (!compact && strncmp(p, "classic", 7)) continue;
        p += 7;
        int wide = 0;
        if (*p == '-') {   /* the box's size, WxL */
            const char *x = p + 1;
            while (isdigit((unsigned char)*x)) x++;
            if (x == p + 1 || *x != 'x' || !isdigit((unsigned char)x[1])) continue;
            for (p = x + 1; isdigit((unsigned char)*p);) p++;
            wide = 1;
        }
        if (*p != '@') continue;
        size_t n = 0;
        while (n + 1u < sizeof l.word && at[n] && at[n] != ',' && at[n] != ' ') { l.word[n] = at[n]; n++; }
        l.word[n] = 0;
        l.compact = compact;
        l.wide = wide;
        return l;
    }
    return l;
}

int oracles_state_text_taken(const char *mods, int *compact, int *wide)
{
    const text_layout l = layout_of(mods);
    *compact = l.compact;
    *wide = l.wide;
    return l.word[0] != 0;
}

/* `mods` without its continuous transitions and its text's word, into `out`. */
static void without_display(const char *mods, char *out, size_t capacity)
{
    size_t used = 0;
    out[0] = 0;
    for (const char *at = mods; *at;) {
        const size_t length = strcspn(at, ", ");
        const int display = (length == strlen(ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS) && !strncmp(at, ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS, length))
                         || !strncmp(at, "text-", 5);
        if (!display && length && used + length + 2u < capacity) used += (size_t)snprintf(out + used, capacity - used, "%s%.*s", used ? "," : "", (int)length, at);
        at += length;
        while (*at == ',' || *at == ' ') at++;
    }
}

int oracles_state_display_only(const char *taken_mods, const char *mods, const char *text_word)
{
    char taken[1100], now[1100];
    without_display(taken_mods, taken, sizeof taken);
    without_display(mods, now, sizeof now);
    return !strcmp(taken, now) && !strcmp(layout_of(taken_mods).word, text_word);
}

/* The launcher's choices, the settings' lines and the options that lay a state's text out, as a list. */
typedef struct text_needs { const char *choice[3], *setting[3], *option[3]; unsigned choices, options; } text_needs;

static void need_choice(text_needs *n, const char *choice, const char *setting)
{
    n->choice[n->choices] = choice;
    n->setting[n->choices++] = setting;
}

/* "A", "A and B", "A, B and C". */
static void listed(const char *const *items, unsigned count, char *out, size_t capacity)
{
    out[0] = 0;
    for (unsigned i = 0; i < count; i++) {
        const size_t length = strlen(out);
        snprintf(out + length, capacity - length, "%s%s", i == 0 ? "" : i + 1u == count ? " and " : ", ", items[i]);
    }
}

/* A state's text as a sentence names it: its letters, its box, or both ("the Compact text in the Wide box"); the
 * launcher's names (`named`), else the settings' words. */
static void described(const text_layout *l, int letters, int box, int named, char *out, size_t capacity)
{
    const char *text = l->compact ? named ? "Compact" : "compact" : named ? "Classic" : "classic";
    const char *in = l->wide ? named ? "Wide" : "wide" : named ? "Standard" : "game's";
    if (letters && box) snprintf(out, capacity, "the %s text in the %s box", text, in);
    else if (letters) snprintf(out, capacity, "the %s text", text);
    else snprintf(out, capacity, "the %s box", in);
}

int oracles_state_text_refusal(const char *taken_mods, const char *mods, int enhanced, int compact_chosen, int wide_chosen, char *message,
                               size_t message_capacity, char *detail, size_t detail_capacity)
{
    if (message_capacity) message[0] = 0;
    if (detail_capacity) detail[0] = 0;
    const text_layout taken = layout_of(taken_mods), now = layout_of(mods);
    if (!strcmp(taken.word, now.word)) return 0;
    if (taken.word[0] && now.word[0] && taken.compact == now.compact && taken.wide == now.wide) {
        snprintf(message, message_capacity, "Refused: taken with another version of the %s", taken.wide ? "Wide box's text" : "compact text");
        snprintf(detail, detail_capacity, "the savestate was taken with the texts laid out by another version of the %s (%s, now %s): it loads "
                 "only with that version", taken.wide ? "wide box" : "compact text", taken.word, now.word);
        return 1;
    }
    text_needs needs = { { NULL }, { NULL }, { NULL }, 0u, 0u };
    char what[64], choices[64], settings[64], options[64], played[64];
    /* A running game keeps its text and its profile: they change from the home screen. */
    if (!enhanced && taken.word[0]) {
        /* The compact text and the wide box play in Enhanced alone: in Faithful the profile is what to change, and the
         * text and the box chosen where they are not the state's. */
        const int letters = taken.compact != (compact_chosen != 0), box = taken.wide != (wide_chosen != 0);
        need_choice(&needs, "Enhanced", NULL);
        if (letters) need_choice(&needs, taken.compact ? "Compact" : "Classic", NULL);
        if (box) need_choice(&needs, taken.wide ? "Wide" : "Standard", NULL);
        described(&taken, taken.compact || letters, taken.wide || box, 1, what, sizeof what);
        listed(needs.choice, needs.choices, choices, sizeof choices);
        if (needs.choices == 1u) snprintf(message, message_capacity, "Refused: taken in Enhanced with %s: choose Enhanced in Display's Profile from the home screen", what);
        else snprintf(message, message_capacity, "Refused: taken in Enhanced with %s: choose %s in Display from the home screen", what, choices);
        described(&taken, 1, 1, 0, what, sizeof what);
        snprintf(detail, detail_capacity, "the savestate was taken in Enhanced with %s, and this session is Faithful, which plays the classic text in "
                 "the game's box: it loads only in Enhanced with them (%s in the launcher's Display, or --enhanced%s%s)", what, choices,
                 taken.compact ? " --text compact" : "", taken.wide ? " --text-box wide" : "");
        return 1;
    }
    const int letters = taken.compact != now.compact, box = taken.wide != now.wide;
    if (letters) {
        need_choice(&needs, taken.compact ? "Compact" : "Classic", taken.compact ? "text=compact" : "text=classic");
        needs.option[needs.options++] = taken.compact ? "--text compact" : "--text classic";
    }
    if (box) {
        need_choice(&needs, taken.wide ? "Wide" : "Standard", taken.wide ? "text_box=wide" : "text_box=standard");
        needs.option[needs.options++] = taken.wide ? "--text-box wide" : "--text-box standard";
    }
    described(&taken, letters, box, 1, what, sizeof what);
    listed(needs.choice, needs.choices, choices, sizeof choices);
    snprintf(message, message_capacity, "Refused: taken with %s: choose %s in Display's %s from the home screen", what, choices,
             letters ? "Text" : "Text box");
    listed(needs.setting, needs.choices, settings, sizeof settings);
    snprintf(options, sizeof options, "%s%s%s", needs.option[0], needs.options > 1u ? " " : "", needs.options > 1u ? needs.option[1] : "");
    described(&taken, letters, box, 0, what, sizeof what);
    described(&now, letters, box, 0, played, sizeof played);
    snprintf(detail, detail_capacity, "the savestate was taken with %s and this session plays %s: it loads only with %s (%s in the launcher's "
             "Display's Text, %s in its settings, or %s)", what, played, letters && box ? "both" : what, choices, settings, options);
    return 1;
}
