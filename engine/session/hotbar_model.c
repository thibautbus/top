#include "hotbar_model.h"

#include <ctype.h>
#include <string.h>

/* Two characters for a key or a controller's button: the shoulders have their usual names, anything else its first letters. */
static void key_text(const char *name, int controller, char out[3])
{
    static const char *const pads[][2] = { { "leftshoulder", "LB" }, { "rightshoulder", "RB" }, { "leftstick", "LS" }, { "rightstick", "RS" }, { "back", "BK" }, { "start", "ST" } };
    out[0] = out[1] = out[2] = 0;
    for (unsigned i = 0; controller && i < sizeof pads / sizeof pads[0]; i++) if (!strcmp(name, pads[i][0])) { memcpy(out, pads[i][1], 2); return; }
    const size_t length = strlen(name);
    for (unsigned i = 0; i < 2u && i < length && (length <= 2u || i < 2u); i++) out[i] = (char)toupper((unsigned char)name[i]);
    if (length > 2u && !controller) out[1] = 0;   /* a named key (Space): its initial */
}

void oracles_hotbar_model(const OraclesItemHotkeys *policy, const char *const names[ORACLES_HOTKEY_SLOTS], int controller, OraclesEnhancedHotbar *out)
{
    memset(out, 0, sizeof *out);
    for (unsigned n = 0; n < ORACLES_HOTKEY_SLOTS; n++) {
        OraclesHotkeySlot slot;
        int pending = 0;
        unsigned dropped = 0;
        oracles_item_hotkeys_slot(policy, n, &slot);
        oracles_item_hotkeys_slot_status(policy, n, &pending, &dropped);
        out->slots[n].set = slot.set; out->slots[n].item = slot.item; out->slots[n].variant = slot.variant;
        out->slots[n].pending = (uint8_t)pending;
        out->slots[n].refusals = dropped;
        if (names[n]) key_text(names[n], controller, out->slots[n].key);
    }
}
