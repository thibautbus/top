/* A savestate taken with other Display settings: the settings, nearest to the player's, a game started again with
 * would load it with, and the few words that say what changed. */
#ifndef ORACLES_LAUNCHER_STATE_DISPLAY_H
#define ORACLES_LAUNCHER_STATE_DISPLAY_H

#include "settings.h"

#include <stddef.h>

/* What a game plays with, as the refusals of a savestate's load read it: the core (an OraclesCoreKind), Enhanced, the
 * continuous transitions, the compact text and the wide box, and the view's surface (0x0 in Faithful, which has none). */
typedef struct OraclesStatePlay {
    int core, enhanced, transitions, compact, wide;
    unsigned width, height;
} OraclesStatePlay;

/* What a game the home screen starts with `settings` plays with, the command line's options winning over them. */
typedef void (*OraclesStatePlayOf)(void *opaque, const oracles_settings *settings, OraclesStatePlay *out);

/* The choices of Display a savestate's load depends on, as settings.txt keeps them: Profile, Core (-1 for the build's
 * default), Continuous transitions, Text, Text box, View, Far size and Screen margin. */
typedef struct OraclesStateDisplay {
    int profile, core, transitions, compact_text, text_box_wide, view, far_fixed, screen_margin;
} OraclesStateDisplay;
void oracles_state_display_read(const oracles_settings *settings, OraclesStateDisplay *out);
void oracles_state_display_write(oracles_settings *settings, const OraclesStateDisplay *display);
/* Those choices put back as `previous` holds them and stored, as a game started again for its savestate does when it
 * does not load it: the quality suggested is forgotten where View or Core changes back. */
void oracles_state_display_restore(oracles_settings *settings, const OraclesStateDisplay *previous);

/* The number of those choices of *settings changed the least they can be for a game started again with them to load a
 * savestate taken with `taken` (its surface 0x0 when it holds none: the view's camera then starts again, whatever the
 * surface), *notice saying what changed ("Display set to Fast (mGBA) and Near to load this savestate"); 0 when the
 * choices as they are load it, *notice empty.  -1, *settings as it was, when none does. */
int oracles_state_display_settings(const OraclesStatePlay *taken, oracles_settings *settings, OraclesStatePlayOf play_of, void *opaque,
                                   char *notice, size_t capacity);

#endif
