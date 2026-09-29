/* The item hotkeys in settings.txt: one line a slot and a
 * game, `hotkey_<game>_<n>=<target>:<item>:<variant>`, in hexadecimal, the
 * identifiers being the ROM's own — `hotkey_ages_1=b:0a:--` is the switch
 * hook on B, `hotkey_ages_2=b:19:01` the satchel with its second seeds.  An
 * empty slot is `-`.  No SDL here: tested without it. */
#ifndef ORACLES_LAUNCHER_HOTKEY_LINES_H
#define ORACLES_LAUNCHER_HOTKEY_LINES_H

#include "item_hotkeys.h"

#include <stddef.h>

/* `name` and `value` are the two sides of a settings line.  Returns 1 with the game (0 ages, 1 seasons), the slot
 * (0 to 3) and its content when the line is a hotkey's and well formed; 0 otherwise, a malformed value included. */
int oracles_hotkey_line_parse(const char *name, const char *value, unsigned *game, unsigned *n, OraclesHotkeySlot *slot);
/* Writes `hotkey_<game>_<n>=…` without a newline; returns the length, 0 when `capacity` is too small. */
size_t oracles_hotkey_line_format(char *out, size_t capacity, unsigned game, unsigned n, const OraclesHotkeySlot *slot);

#endif
