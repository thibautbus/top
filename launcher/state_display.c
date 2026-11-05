#include "state_display.h"

#include "ui_page_nav.h"
#include "ui_page_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The settings a savestate's load depends on, each as a value of its own; the screen's shape (aspect=) is not among
 * them: it is no choice of Display. */
enum { PROFILE, CORE, TRANSITIONS, TEXT, BOX, VIEW, FAR, MARGIN, FIELDS };
/* Profile weighs more than all the others together: a savestate that they load leaves the player's profile alone. */
static const unsigned field_cost[FIELDS] = { FIELDS, 1u, 1u, 1u, 1u, 1u, 1u, 1u };
/* Past this many changes the notice counts them rather than naming them. */
#define NAMED_CHANGES 4

void oracles_state_display_read(const oracles_settings *settings, OraclesStateDisplay *out)
{
    *out = (OraclesStateDisplay){ (int)settings->profile, settings->core, settings->transitions, settings->compact_text, settings->text_box_wide,
                                  settings->view, settings->far_fixed, settings->screen_margin };
}

void oracles_state_display_write(oracles_settings *settings, const OraclesStateDisplay *display)
{
    settings->profile = (OraclesProfile)display->profile;
    settings->core = display->core;
    settings->transitions = display->transitions;
    settings->compact_text = display->compact_text;
    settings->text_box_wide = display->text_box_wide;
    settings->view = display->view;
    settings->far_fixed = display->far_fixed;
    settings->screen_margin = display->screen_margin;
}

void oracles_state_display_restore(oracles_settings *settings, const OraclesStateDisplay *previous)
{
    const int view = settings->view, core = oracles_settings_core(settings);
    oracles_state_display_write(settings, previous);
    oracles_settings_quality_changed(settings, view, core, settings->ghosts);
    oracles_settings_store(settings);
}

static void read_fields(const oracles_settings *s, int out[FIELDS])
{
    out[PROFILE] = (int)s->profile;
    out[CORE] = oracles_settings_core(s);
    out[TRANSITIONS] = s->transitions != 0;
    out[TEXT] = s->compact_text != 0;
    out[BOX] = s->text_box_wide != 0;
    out[VIEW] = s->view;
    out[FAR] = s->far_fixed != 0;
    out[MARGIN] = s->screen_margin;
}

static void write_fields(oracles_settings *s, const int in[FIELDS])
{
    s->profile = (OraclesProfile)in[PROFILE];
    if (in[CORE] != oracles_settings_core(s)) s->core = in[CORE];
    s->transitions = in[TRANSITIONS];
    s->compact_text = in[TEXT];
    s->text_box_wide = in[BOX];
    s->view = in[VIEW];
    s->far_fixed = in[FAR];
    s->screen_margin = in[MARGIN];
}

/* The load's refusals (state_refusal.h, the view's check) as the play would meet them, the surface aside. */
static int loads_but_surface(const OraclesStatePlay *taken, const OraclesStatePlay *play)
{
    const int text = taken->compact || taken->wide;
    if (play->core != taken->core) return 0;
    if ((taken->transitions != 0) != (play->enhanced && play->transitions)) return 0;
    if (text) return play->enhanced && (play->compact != 0) == (taken->compact != 0) && (play->wide != 0) == (taken->wide != 0);
    return !(play->enhanced && (play->compact || play->wide));
}

static int loads_on_surface(const OraclesStatePlay *taken, const OraclesStatePlay *play)
{
    return !play->enhanced || !taken->width || (play->width == taken->width && play->height == taken->height);
}

/* A field's new value as Display names it. */
static void field_word(int field, int value, char *out, size_t capacity)
{
    switch (field) {
        case PROFILE: snprintf(out, capacity, "%s", oracles_profile_names[value]); break;
        case CORE: snprintf(out, capacity, "%s", oracles_display_core_choices[value]); break;
        case TRANSITIONS: snprintf(out, capacity, "Continuous transitions %s", value ? "On" : "Off"); break;
        case TEXT: snprintf(out, capacity, "%s text", oracles_display_text_choices[value]); break;
        case BOX: snprintf(out, capacity, "%s text box", oracles_display_text_box_choices[value]); break;
        case VIEW: snprintf(out, capacity, "%s", oracles_display_view_names[value]); break;
        case FAR: snprintf(out, capacity, "Far size %s", oracles_display_far_size_choices[value]); break;
        default: snprintf(out, capacity, "Screen margin %s", oracles_display_margin_choices[value]); break;
    }
}

/* The surface's fields, their set `i` of all of them. */
enum { SURFACES = 3 * 2 * ORACLES_SCREEN_MARGINS };
static void surface_fields(int i, int fields[FIELDS])
{
    fields[VIEW] = i % 3;
    fields[FAR] = i / 3 % 2;
    fields[MARGIN] = i / 6;
}

int oracles_state_display_settings(const OraclesStatePlay *taken, oracles_settings *settings, OraclesStatePlayOf play_of, void *opaque,
                                   char *notice, size_t capacity)
{
    if (capacity) notice[0] = 0;
    oracles_settings *candidate = malloc(sizeof *candidate);
    if (!candidate) return -1;
    int now[FIELDS], best[FIELDS], at[FIELDS];
    read_fields(settings, now);
    /* The surface's fields matter in Enhanced alone, whatever the others: each of their sets is played once, Enhanced. */
    OraclesStatePlay surfaces[SURFACES];
    int surfaces_read = 0;
    unsigned best_cost = ~0u;
    for (unsigned rest = 0; rest < 1u << 5; rest++) {
        for (int f = PROFILE; f <= BOX; f++) at[f] = rest >> f & 1u ? !now[f] : now[f];
        unsigned cost = 0;
        for (int f = PROFILE; f <= BOX; f++) cost += at[f] != now[f] ? field_cost[f] : 0u;
        if (best_cost != ~0u && cost >= best_cost) continue;
        memcpy(candidate, settings, sizeof *candidate);
        for (int f = VIEW; f < FIELDS; f++) at[f] = now[f];
        write_fields(candidate, at);
        OraclesStatePlay play;
        play_of(opaque, candidate, &play);
        if (!loads_but_surface(taken, &play)) continue;
        if (loads_on_surface(taken, &play)) {
            best_cost = cost;
            memcpy(best, at, sizeof best);
            continue;
        }
        /* Another surface: the view's level, Far size and the screen margin, the fewest changed. */
        if (!surfaces_read) {
            int set[FIELDS];
            memcpy(set, now, sizeof set);
            set[PROFILE] = ORACLES_PROFILE_ENHANCED;
            for (int i = 0; i < SURFACES; i++) {
                surface_fields(i, set);
                memcpy(candidate, settings, sizeof *candidate);
                write_fields(candidate, set);
                play_of(opaque, candidate, &surfaces[i]);
            }
            surfaces_read = 1;
        }
        for (int i = 0; i < SURFACES; i++) {
            if (!loads_on_surface(taken, &surfaces[i])) continue;
            int with[FIELDS];
            memcpy(with, at, sizeof with);
            surface_fields(i, with);
            unsigned total = cost;
            for (int f = VIEW; f < FIELDS; f++) total += with[f] != now[f] ? field_cost[f] : 0u;
            if (best_cost != ~0u && total >= best_cost) continue;
            best_cost = total;
            memcpy(best, with, sizeof best);
        }
    }
    free(candidate);
    if (best_cost == ~0u) return -1;
    write_fields(settings, best);
    /* "Display set to A, B and C to load this savestate", or past a few, how many. */
    char words[FIELDS][48];
    unsigned count = 0;
    for (int f = 0; f < FIELDS; f++)
        if (best[f] != now[f]) field_word(f, best[f], words[count++], sizeof words[0]);
    if (!count) return 0;
    if (count > NAMED_CHANGES) {
        snprintf(notice, capacity, "Display changed in %u rows to load this savestate", count);
        return (int)count;
    }
    size_t length = (size_t)snprintf(notice, capacity, "Display set to ");
    for (unsigned i = 0; i < count && length < capacity; i++)
        length += (size_t)snprintf(notice + length, capacity - length, "%s%s", i == 0 ? "" : i + 1u == count ? " and " : ", ", words[i]);
    if (length < capacity) snprintf(notice + length, capacity - length, " to load this savestate");
    return (int)count;
}
