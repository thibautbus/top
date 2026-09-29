/* What the hotbar shows of the item hotkeys' policy: the model the Enhanced view receives,
 * read-only, from whoever runs the policy, the launcher or the harness. */
#ifndef ORACLES_LAUNCHER_HOTBAR_MODEL_H
#define ORACLES_LAUNCHER_HOTBAR_MODEL_H

#include "item_hotkeys.h"
#include "view.h"

/* `names` are the slots' key names (SDL key names, or controller button names with `controller`). */
void oracles_hotbar_model(const OraclesItemHotkeys *policy, const char *const names[ORACLES_HOTKEY_SLOTS], int controller, OraclesEnhancedHotbar *out);

#endif
