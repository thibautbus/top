/* The fan games as the launcher's host finds them, for the ROM-free tests of its pages: what their profiles do not
 * qualify (OraclesHomeGame's refuses, read by the host in each profile). */
#ifndef ORACLES_TESTS_HOME_FIXTURE_H
#define ORACLES_TESTS_HOME_FIXTURE_H

#include "ui_home_nav.h"

#define FAN_REFUSES (ORACLES_HOME_REFUSES_SWIM_HOLD | ORACLES_HOME_REFUSES_HOTKEYS)
/* As the mockup draws the fan games, before they had cheats: their menu and pause without Cheats. */
#define MOCKUP_FAN_REFUSES (FAN_REFUSES | ORACLES_HOME_REFUSES_CHEATS)

static inline void fan_profiles_refusing(OraclesHomeNav *nav, unsigned refuses)
{
    for (unsigned f = 0; f < ORACLES_HOME_FAN_GAMES; f++) nav->games[oracles_home_fan_games[f].game].refuses = refuses;
}
static inline void fan_profiles(OraclesHomeNav *nav) { fan_profiles_refusing(nav, FAN_REFUSES); }
static inline void mockup_fan_profiles(OraclesHomeNav *nav) { fan_profiles_refusing(nav, MOCKUP_FAN_REFUSES); }

#endif
