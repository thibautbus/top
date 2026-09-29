#include "hotkey_lines.h"

#include <stdio.h>
#include <string.h>

static const char *const game_names[2] = { "ages", "seasons" };

static int hex_byte(const char *text, uint8_t *out)
{
    unsigned value = 0;
    for (unsigned i = 0; i < 2u; i++) {
        const char c = text[i];
        unsigned digit;
        if (c >= '0' && c <= '9') digit = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') digit = (unsigned)(c - 'a') + 10u;
        else if (c >= 'A' && c <= 'F') digit = (unsigned)(c - 'A') + 10u;
        else return 0;
        value = value * 16u + digit;
    }
    *out = (uint8_t)value;
    return 1;
}

int oracles_hotkey_line_parse(const char *name, const char *value, unsigned *game, unsigned *n, OraclesHotkeySlot *slot)
{
    if (strncmp(name, "hotkey_", 7) != 0) return 0;
    unsigned g = 2;
    for (unsigned i = 0; i < 2u; i++) {
        const size_t length = strlen(game_names[i]);
        if (!strncmp(name + 7, game_names[i], length) && name[7 + length] == '_') { g = i; name += 7 + length + 1; break; }
    }
    if (g == 2u || name[0] < '1' || name[0] > '0' + (int)ORACLES_HOTKEY_SLOTS || name[1]) return 0;
    memset(slot, 0, sizeof *slot);
    *game = g;
    *n = (unsigned)(name[0] - '1');
    if (!strcmp(value, "-")) return 1;
    /* <b|a>:<item>:<variant or --> */
    if ((value[0] != 'b' && value[0] != 'a') || value[1] != ':' || strlen(value) != 7u || value[4] != ':') return 0;
    uint8_t item = 0, variant = ORACLES_HOTKEY_NO_VARIANT;
    if (!hex_byte(value + 2, &item) || !item) return 0;
    if (strcmp(value + 5, "--") != 0 && !hex_byte(value + 5, &variant)) return 0;
    slot->set = 1;
    slot->item = item;
    slot->variant = variant;
    slot->target = value[0] == 'a' ? ORACLES_INVENTORY_SLOT_A : ORACLES_INVENTORY_SLOT_B;
    return 1;
}

size_t oracles_hotkey_line_format(char *out, size_t capacity, unsigned game, unsigned n, const OraclesHotkeySlot *slot)
{
    if (game > 1u || n >= ORACLES_HOTKEY_SLOTS) return 0;
    int written;
    if (!slot->set) written = snprintf(out, capacity, "hotkey_%s_%u=-", game_names[game], n + 1u);
    else if (slot->variant == ORACLES_HOTKEY_NO_VARIANT)
        written = snprintf(out, capacity, "hotkey_%s_%u=%c:%02x:--", game_names[game], n + 1u, slot->target == ORACLES_INVENTORY_SLOT_A ? 'a' : 'b', slot->item);
    else written = snprintf(out, capacity, "hotkey_%s_%u=%c:%02x:%02x", game_names[game], n + 1u, slot->target == ORACLES_INVENTORY_SLOT_A ? 'a' : 'b', slot->item, slot->variant);
    return written > 0 && (size_t)written < capacity ? (size_t)written : 0;
}
