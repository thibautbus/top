#include "route.h"

#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t capacity, const char *message, unsigned line)
{
    if (error && capacity) snprintf(error, capacity, "route line %u: %s", line, message);
}

/* Copies a header value, truncated to the field; a truncated hash will simply not match. */
static void copy_field(char *field, size_t capacity, const char *value)
{
    size_t length = strlen(value);
    if (length >= capacity) length = capacity - 1;
    memcpy(field, value, length);
    field[length] = 0;
}

static int push_event(OraclesRoute *route, uint32_t frame, unsigned mask)
{
    if (route->count >= ORACLES_ROUTE_MAX_EVENTS) return -1;
    if (route->count == route->capacity) {
        const size_t capacity = route->capacity ? route->capacity * 2 : 256;
        OraclesRouteEvent *events = realloc(route->events, capacity * sizeof *events);
        if (!events) return -1;
        route->events = events;
        route->capacity = capacity;
    }
    route->events[route->count].frame = frame;
    route->events[route->count].mask = mask;
    route->count++;
    return 0;
}

static const char *const variant_names[4] = { "", "satchel", "shooter", "harp" };

static int slot_from_name(const char *name)
{
    if (strcmp(name, "b") == 0) return 0;
    if (strcmp(name, "a") == 0) return 1;
    if (name[0] != 's' || !name[1]) return -1;
    char *end = NULL;
    const unsigned long n = strtoul(name + 1, &end, 10);
    return *end == 0 && n < 16ul ? (int)n + 2 : -1;
}

static void slot_name(unsigned slot, char out[8])
{
    if (slot == 0) snprintf(out, 8, "b"); else if (slot == 1) snprintf(out, 8, "a"); else snprintf(out, 8, "s%u", slot - 2u);
}

static int hex_byte(const char *text, uint8_t *out)
{
    char *end = NULL;
    const unsigned long v = strtoul(text, &end, 16);
    if (!text[0] || *end != 0 || v > 0xfful) return 0;
    *out = (uint8_t)v;
    return 1;
}

/* `equip <slot>=<item> <slot>=<item> [<variant>=<value>]`, or `equip <variant>=<value>` alone. */
static int parse_equip(char *rest, OraclesRouteAction *action)
{
    unsigned pairs = 0;
    for (char *token = strtok(rest, " "); token; token = strtok(NULL, " ")) {
        char *equals = strchr(token, '=');
        uint8_t value;
        if (!equals) return 0;
        *equals = 0;
        if (!hex_byte(equals + 1, &value)) return 0;
        const int slot = slot_from_name(token);
        if (slot >= 0) {
            if (pairs == 0) { action->slot_a = (uint8_t)slot; action->item_a = value; }
            else if (pairs == 1) { action->slot_b = (uint8_t)slot; action->item_b = value; }
            else return 0;
            pairs++;
            continue;
        }
        unsigned variant = 0;
        for (unsigned v = 1; v < 4u; v++) if (strcmp(token, variant_names[v]) == 0) variant = v;
        if (!variant || action->variant) return 0;
        action->variant = (uint8_t)variant; action->variant_value = value;
    }
    if (pairs == 1u || (pairs == 0u && !action->variant)) return 0;
    action->swap = pairs == 2u;
    return 1;
}

static int push_action(OraclesRoute *route, const OraclesRouteAction *action)
{
    if (route->action_count >= ORACLES_ROUTE_MAX_EVENTS) return -1;
    if (route->action_count == route->action_capacity) {
        const size_t capacity = route->action_capacity ? route->action_capacity * 2 : 64;
        OraclesRouteAction *actions = realloc(route->actions, capacity * sizeof *actions);
        if (!actions) return -1;
        route->actions = actions;
        route->action_capacity = capacity;
    }
    route->actions[route->action_count++] = *action;
    return 0;
}

/* `cheat <name> on|off`: 1, 0 for a malformed line, -1 for a cheat's frame that does not increase strictly (two cheats
 * may change at a same frame, one cannot twice). */
static int parse_cheat(const OraclesRoute *route, uint32_t frame, char *rest, const char **name, int *on)
{
    const char *value;
    *name = strtok(rest, " ");
    value = strtok(NULL, " ");
    if (!*name || !value || strtok(NULL, " ") || strlen(*name) >= sizeof route->cheats[0].name) return 0;
    if (strcmp(value, "on") != 0 && strcmp(value, "off") != 0) return 0;
    *on = strcmp(value, "on") == 0;
    for (size_t i = route->cheat_count; i-- > 0;)
        if (!strcmp(route->cheats[i].name, *name)) return route->cheats[i].frame < frame ? 1 : -1;
    return 1;
}

static int push_cheat(OraclesRoute *route, uint32_t frame, const char *name, int on)
{
    if (route->cheat_count >= ORACLES_ROUTE_MAX_EVENTS) return -1;
    if (route->cheat_count == route->cheat_capacity) {
        const size_t capacity = route->cheat_capacity ? route->cheat_capacity * 2 : 16;
        OraclesRouteCheat *cheats = realloc(route->cheats, capacity * sizeof *cheats);
        if (!cheats) return -1;
        route->cheats = cheats;
        route->cheat_capacity = capacity;
    }
    OraclesRouteCheat *cheat = &route->cheats[route->cheat_count++];
    cheat->frame = frame;
    snprintf(cheat->name, sizeof cheat->name, "%s", name);
    cheat->on = on;
    return 0;
}

static const char *const setting_names[ORACLES_ROUTE_SETTINGS] = { "swim-hold", "item-hotkeys", "window", "text-speed" };
static const char *const on_off[2] = { "off", "on" };
static const char *const hotkey_modes[3] = { "off", "use", "equip" };
static const char *const windows[5] = { "full", NULL, "2", "3", "4" };
static const char *const text_speeds[2] = { "classic", "instant" };

const char *oracles_route_setting_name(OraclesRouteSettingName name)
{
    return (unsigned)name < ORACLES_ROUTE_SETTINGS ? setting_names[name] : NULL;
}

const char *oracles_route_setting_value(OraclesRouteSettingName name, int value)
{
    if (name == ORACLES_ROUTE_SET_SWIM_HOLD) return value >= 0 && value < 2 ? on_off[value] : NULL;
    if (name == ORACLES_ROUTE_SET_ITEM_HOTKEYS) return value >= 0 && value < 3 ? hotkey_modes[value] : NULL;
    if (name == ORACLES_ROUTE_SET_WINDOW) return value >= 0 && value < 5 ? windows[value] : NULL;
    if (name == ORACLES_ROUTE_SET_TEXT_SPEED) return value >= 0 && value < 2 ? text_speeds[value] : NULL;
    return NULL;
}

/* `set <name> <value>`: 1, 0 for a malformed line or a name or a value this reader does not know, -1 for a setting's
 * frame that does not increase strictly. */
static int parse_set(const OraclesRoute *route, uint32_t frame, char *rest, OraclesRouteSetting *setting)
{
    const char *name = strtok(rest, " "), *value = strtok(NULL, " ");
    if (!name || !value || strtok(NULL, " ")) return 0;
    setting->frame = frame;
    setting->name = ORACLES_ROUTE_SETTINGS;
    for (unsigned n = 0; n < ORACLES_ROUTE_SETTINGS; n++) if (!strcmp(name, setting_names[n])) setting->name = (OraclesRouteSettingName)n;
    if (setting->name == ORACLES_ROUTE_SETTINGS) return 0;
    if (setting->name == ORACLES_ROUTE_SET_TEXT_SPEED && route->format < ORACLES_ROUTE_FORMAT_TEXT_INSTANT) return 0;   /* a name its format's readers do not know */
    setting->value = -1;
    for (int v = 0; v < 5; v++) {
        const char *text = oracles_route_setting_value(setting->name, v);
        if (text && !strcmp(value, text)) setting->value = v;
    }
    if (setting->value < 0) return 0;
    for (size_t i = route->setting_count; i-- > 0;)
        if (route->settings[i].name == setting->name) return route->settings[i].frame < frame ? 1 : -1;
    return 1;
}

static int push_setting(OraclesRoute *route, const OraclesRouteSetting *setting)
{
    if (route->setting_count >= ORACLES_ROUTE_MAX_EVENTS) return -1;
    if (route->setting_count == route->setting_capacity) {
        const size_t capacity = route->setting_capacity ? route->setting_capacity * 2 : 16;
        OraclesRouteSetting *settings = realloc(route->settings, capacity * sizeof *settings);
        if (!settings) return -1;
        route->settings = settings;
        route->setting_capacity = capacity;
    }
    route->settings[route->setting_count++] = *setting;
    return 0;
}

int oracles_route_read(const char *path, OraclesRoute *route, char *error, size_t error_capacity)
{
    memset(route, 0, sizeof *route);
    FILE *f = fopen(path, "r");
    if (!f) { set_error(error, error_capacity, "cannot open the route file", 0); return -1; }
    char line[1100];
    unsigned number = 0;
    int in_inputs = 0, format_seen = 0;
    uint32_t last_frame = 0, last_any = 0, last_of_verb[8] = { 0 };
    int seen_any = 0, seen_verb[8] = { 0 };
    while (fgets(line, sizeof line, f)) {
        number++;
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0] || line[0] == '#') continue;
        if (!in_inputs) {
            char key[32], value[1024];
            if (sscanf(line, "%31s %1023s", key, value) != 2) {
                if (strcmp(line, "inputs") == 0) { in_inputs = 1; continue; }
                set_error(error, error_capacity, "expected 'name value' or 'inputs'", number);
                goto fail;
            }
            if (strcmp(key, "oracles-route") == 0) {
                route->format = (unsigned)strtoul(value, NULL, 10);
                if (route->format < 1u || route->format > ORACLES_ROUTE_FORMAT_LATEST) { set_error(error, error_capacity, "unsupported route format", number); goto fail; }
                format_seen = 1;
            } else if (strcmp(key, "game") == 0) copy_field(route->header.game, sizeof route->header.game, value);
            else if (strcmp(key, "rom_sha1") == 0) copy_field(route->header.rom_sha1, sizeof route->header.rom_sha1, value);
            else if (strcmp(key, "sram_sha1") == 0) copy_field(route->header.sram_sha1, sizeof route->header.sram_sha1, value);
            else if (strcmp(key, "store_sha1") == 0) copy_field(route->header.store_sha1, sizeof route->header.store_sha1, value);
            else if (strcmp(key, "options") == 0) {
                copy_field(route->header.options, sizeof route->header.options, value);
                /* A route of format 3 or less cannot name a language: its readers replay it in English. */
                char language[16];
                if (oracles_route_language(&route->header, language, sizeof language)[0] && route->format < ORACLES_ROUTE_FORMAT_LANGUAGE) {
                    set_error(error, error_capacity, "a language option needs format 4", number);
                    goto fail;
                }
                /* Nor the swim's hold: its readers replay it without. */
                if (oracles_route_has_option(&route->header, ORACLES_ROUTE_OPTION_SWIM_HOLD) && route->format < ORACLES_ROUTE_FORMAT_SWIM_HOLD) {
                    set_error(error, error_capacity, "the swim-hold option needs format 5", number);
                    goto fail;
                }
                /* Nor the transitions' diagonal. */
                if (oracles_route_has_option(&route->header, ORACLES_ROUTE_OPTION_CONTINUOUS_DIAGONAL) && route->format < ORACLES_ROUTE_FORMAT_DIAGONAL) {
                    set_error(error, error_capacity, "the continuous-diagonal option needs format 6", number);
                    goto fail;
                }
                /* Nor a cheat. */
                if (oracles_route_has_cheat(&route->header) && route->format < ORACLES_ROUTE_FORMAT_CHEATS) {
                    set_error(error, error_capacity, "a cheat option needs format 7", number);
                    goto fail;
                }
                /* Nor the compact text. */
                if (oracles_route_has_option(&route->header, ORACLES_ROUTE_OPTION_TEXT_COMPACT) && route->format < ORACLES_ROUTE_FORMAT_TEXT_COMPACT) {
                    set_error(error, error_capacity, "the text-compact option needs format 9", number);
                    goto fail;
                }
                /* Nor the wide text box. */
                if (oracles_route_has_option(&route->header, ORACLES_ROUTE_OPTION_TEXT_BOX_WIDE) && route->format < ORACLES_ROUTE_FORMAT_TEXT_BOX_WIDE) {
                    set_error(error, error_capacity, "the text-box-wide option needs format 11", number);
                    goto fail;
                }
                /* Nor the instant text. */
                if (oracles_route_has_option(&route->header, ORACLES_ROUTE_OPTION_TEXT_INSTANT) && route->format < ORACLES_ROUTE_FORMAT_TEXT_INSTANT) {
                    set_error(error, error_capacity, "the text-instant option needs format 12", number);
                    goto fail;
                }
                /* Nor the transitions' steps. */
                if (oracles_route_has_option(&route->header, ORACLES_ROUTE_OPTION_CONTINUOUS_STEPS) && route->format < ORACLES_ROUTE_FORMAT_STEPS) {
                    set_error(error, error_capacity, "the continuous-steps option needs format 13", number);
                    goto fail;
                }
                if (oracles_route_has_option(&route->header, ORACLES_ROUTE_OPTION_CONTINUOUS_LINE) && route->format < ORACLES_ROUTE_FORMAT_LINE) {
                    set_error(error, error_capacity, "the continuous-line option needs format 15", number);
                    goto fail;
                }
            }
            else if (strcmp(key, "mods") == 0) {
                /* A route of format 2 or less cannot name mods: its readers replay it without them. */
                if (route->format < ORACLES_ROUTE_FORMAT_MODS) { set_error(error, error_capacity, "a mods line needs format 3", number); goto fail; }
                copy_field(route->header.mods, sizeof route->header.mods, value);
            }
            else if (strcmp(key, "core") == 0) {
                /* How the core ran changes the game: a setting this reader does not know stops it, as an unknown verb does. */
                copy_field(route->header.core, sizeof route->header.core, value);
                for (const char *p = route->header.core; *p;) {
                    const size_t length = strcspn(p, ",");
                    const int known = (length == strlen(ORACLES_ROUTE_CORE_JOYPAD_BOUNCING_OFF) && !memcmp(p, ORACLES_ROUTE_CORE_JOYPAD_BOUNCING_OFF, length))
                                   || (length == strlen(ORACLES_ROUTE_CORE_MGBA) && !memcmp(p, ORACLES_ROUTE_CORE_MGBA, length));
                    if (!known) { set_error(error, error_capacity, "unknown core setting", number); goto fail; }
                    p += length + (p[length] == ',');
                }
                /* mGBA has no joypad bouncing: a route of it says so too, and one that does not is not of this format. */
                if (oracles_route_core_mgba(&route->header) && oracles_route_joypad_bouncing(&route->header)) {
                    set_error(error, error_capacity, "a core of mgba needs joypad-bouncing-off", number);
                    goto fail;
                }
            }
            /* other names are reserved for later formats and ignored */
            continue;
        }
        /* `<frame> <verb> ...`.  The frames never decrease from a line to the
         * next and increase strictly for a same verb; at a same frame the
         * lines apply in the file's order.  A verb the reader does not know
         * stops it: every verb but `keys` changes the game's state. */
        unsigned long frame;
        char verb[16];
        int consumed = 0;
        if (sscanf(line, "%lu %15s %n", &frame, verb, &consumed) < 2) { set_error(error, error_capacity, "expected '<frame> <verb> ...'", number); goto fail; }
        const unsigned which = strcmp(verb, "keys") == 0 ? 0u : strcmp(verb, "equip") == 0 ? 1u : strcmp(verb, "use") == 0 ? 2u
                             : strcmp(verb, "cheat") == 0 ? 3u : strcmp(verb, "set") == 0 ? 5u : strcmp(verb, "sound") == 0 ? 6u : strcmp(verb, "throw") == 0 ? 7u : 4u;
        if (which == 4u || (which != 0u && route->format < 2u) || (which == 3u && route->format < ORACLES_ROUTE_FORMAT_CHEAT_VERB)
            || (which == 5u && route->format < ORACLES_ROUTE_FORMAT_SET) || (which >= 6u && route->format < ORACLES_ROUTE_FORMAT_SOUND)) {
            set_error(error, error_capacity, "unknown verb for this route format", number);
            goto fail;
        }
        if ((seen_any && frame < last_any) || ((which < 3u || which >= 6u) && seen_verb[which] && frame <= last_of_verb[which])) { set_error(error, error_capacity, "frames must increase", number); goto fail; }
        char *rest = line + consumed;
        if (which == 5u) {
            OraclesRouteSetting setting;
            const int parsed = parse_set(route, (uint32_t)frame, rest, &setting);
            if (parsed <= 0) { set_error(error, error_capacity, parsed < 0 ? "frames must increase" : "expected '<frame> set <name> <value>', a setting and a value this version knows", number); goto fail; }
            if (push_setting(route, &setting) != 0) { set_error(error, error_capacity, "too many events", number); goto fail; }
            seen_any = 1; last_any = (uint32_t)frame;
            continue;
        }
        if (which == 3u) {
            const char *name;
            int on;
            const int parsed = parse_cheat(route, (uint32_t)frame, rest, &name, &on);
            if (parsed <= 0) { set_error(error, error_capacity, parsed < 0 ? "frames must increase" : "expected '<frame> cheat <name> on|off'", number); goto fail; }
            if (push_cheat(route, (uint32_t)frame, name, on) != 0) { set_error(error, error_capacity, "too many events", number); goto fail; }
            seen_any = 1; last_any = (uint32_t)frame;
            continue;
        }
        seen_any = 1; last_any = (uint32_t)frame; seen_verb[which] = 1; last_of_verb[which] = (uint32_t)frame;
        if (which == 0u) {
            unsigned mask;
            if (sscanf(rest, "%x", &mask) != 1 || mask > 0xffu) { set_error(error, error_capacity, "expected '<frame> keys <hex mask>'", number); goto fail; }
            if (push_event(route, (uint32_t)frame, mask) != 0) { set_error(error, error_capacity, "too many events", number); goto fail; }
            last_frame = (uint32_t)frame;
            continue;
        }
        OraclesRouteAction action;
        memset(&action, 0, sizeof action);
        action.frame = (uint32_t)frame;
        action.verb = which == 1u ? ORACLES_ROUTE_EQUIP : which == 6u ? ORACLES_ROUTE_SOUND : which == 7u ? ORACLES_ROUTE_THROW : ORACLES_ROUTE_USE;
        if (which == 7u) {
            char button[8];
            const int slot = sscanf(rest, "%7s", button) == 1 ? slot_from_name(button) : -1;
            if (slot < 0 || slot > 1) { set_error(error, error_capacity, "expected '<frame> throw <b|a>'", number); goto fail; }
            action.button = (uint8_t)slot;
        } else if (which == 6u) {
            char sound[8];
            if (sscanf(rest, "%7s", sound) != 1 || !hex_byte(sound, &action.item) || !action.item) { set_error(error, error_capacity, "expected '<frame> sound <hex sound>'", number); goto fail; }
        } else if (which == 1u) {
            if (!parse_equip(rest, &action)) { set_error(error, error_capacity, "expected '<frame> equip <slot>=<item> <slot>=<item> [<variant>=<value>]'", number); goto fail; }
        } else {
            char button[8], item[8];
            const int slot = sscanf(rest, "%7s %7s", button, item) == 2 ? slot_from_name(button) : -1;
            if (slot < 0 || slot > 1 || !hex_byte(item, &action.item)) { set_error(error, error_capacity, "expected '<frame> use <b|a> <item>'", number); goto fail; }
            action.button = (uint8_t)slot;
        }
        if (push_action(route, &action) != 0) { set_error(error, error_capacity, "too many events", number); goto fail; }
    }
    (void)last_frame;
    fclose(f);
    if (!format_seen || !in_inputs) { set_error(error, error_capacity, "missing 'oracles-route 1' header or 'inputs' line", number); oracles_route_free(route); return -1; }
    return 0;
fail:
    fclose(f);
    oracles_route_free(route);
    return -1;
}

void oracles_route_free(OraclesRoute *route)
{
    free(route->events);
    free(route->actions);
    free(route->cheats);
    free(route->settings);
    memset(route, 0, sizeof *route);
}

/* Whether a comma-separated list holds `name`. */
static int listed(const char *list, const char *name)
{
    const char *p = list;
    const size_t n = strlen(name);
    while (*p) {
        const char *end = strchr(p, ',');
        const size_t length = end ? (size_t)(end - p) : strlen(p);
        if (length == n && memcmp(p, name, n) == 0) return 1;
        if (!end) break;
        p = end + 1;
    }
    return 0;
}

int oracles_route_has_option(const OraclesRouteHeader *header, const char *name) { return listed(header->options, name); }

int oracles_route_scene_walks_taken(const OraclesRoute *route)
{
    return route && route->format < ORACLES_ROUTE_FORMAT_SCENE_WALKS && oracles_route_has_option(&route->header, ORACLES_ROUTE_OPTION_CONTINUOUS_TRANSITIONS);
}

int oracles_route_has_cheat(const OraclesRouteHeader *header)
{
    const size_t prefix = strlen(ORACLES_ROUTE_OPTION_CHEAT);
    for (const char *p = header->options; *p;) {
        const size_t length = strcspn(p, ",");
        if (length > prefix && !strncmp(p, ORACLES_ROUTE_OPTION_CHEAT, prefix)) return 1;
        p += length + (p[length] == ',');
    }
    return 0;
}

const char *oracles_route_language(const OraclesRouteHeader *header, char *out, size_t capacity)
{
    const size_t prefix = strlen(ORACLES_ROUTE_OPTION_LANGUAGE);
    if (capacity) out[0] = 0;
    for (const char *p = header->options; *p;) {
        const char *end = strchr(p, ',');
        const size_t length = end ? (size_t)(end - p) : strlen(p);
        if (length > prefix && strncmp(p, ORACLES_ROUTE_OPTION_LANGUAGE, prefix) == 0 && length - prefix < capacity) {
            memcpy(out, p + prefix, length - prefix);
            out[length - prefix] = 0;
            break;
        }
        p += length + (end != NULL);
    }
    return out;
}

int oracles_route_language_matches(const OraclesRouteHeader *header, const char *language)
{
    char recorded[8];
    if (!oracles_route_language(header, recorded, sizeof recorded)[0]) return 1;
    const char *session = language && strcmp(language, "en") != 0 ? language : "";
    return strcmp(recorded, session) == 0;
}
int oracles_route_joypad_bouncing(const OraclesRouteHeader *header) { return !listed(header->core, ORACLES_ROUTE_CORE_JOYPAD_BOUNCING_OFF); }
int oracles_route_core_mgba(const OraclesRouteHeader *header) { return listed(header->core, ORACLES_ROUTE_CORE_MGBA); }

size_t oracles_route_actions_at(const OraclesRoute *route, uint32_t frame, size_t *first)
{
    size_t low = 0, high = route->action_count;
    while (low < high) {   /* the first action at or after `frame` */
        const size_t mid = low + (high - low) / 2;
        if (route->actions[mid].frame < frame) low = mid + 1; else high = mid;
    }
    size_t count = 0;
    while (low + count < route->action_count && route->actions[low + count].frame == frame) count++;
    if (first) *first = low;
    return count;
}

size_t oracles_route_cheats_at(const OraclesRoute *route, uint32_t frame, size_t *first)
{
    size_t low = 0, high = route->cheat_count;
    while (low < high) {
        const size_t mid = low + (high - low) / 2;
        if (route->cheats[mid].frame < frame) low = mid + 1; else high = mid;
    }
    size_t count = 0;
    while (low + count < route->cheat_count && route->cheats[low + count].frame == frame) count++;
    if (first) *first = low;
    return count;
}

size_t oracles_route_settings_at(const OraclesRoute *route, uint32_t frame, size_t *first)
{
    size_t low = 0, high = route->setting_count;
    while (low < high) {
        const size_t mid = low + (high - low) / 2;
        if (route->settings[mid].frame < frame) low = mid + 1; else high = mid;
    }
    size_t count = 0;
    while (low + count < route->setting_count && route->settings[low + count].frame == frame) count++;
    if (first) *first = low;
    return count;
}

int oracles_route_sets(const OraclesRoute *route, OraclesRouteSettingName name)
{
    for (size_t i = 0; i < route->setting_count; i++) if (route->settings[i].name == name) return 1;
    return 0;
}

uint32_t oracles_route_last_frame(const OraclesRoute *route)
{
    const uint32_t keys = route->count ? route->events[route->count - 1].frame : 0u;
    const uint32_t actions = route->action_count ? route->actions[route->action_count - 1].frame : 0u;
    const uint32_t cheats = route->cheat_count ? route->cheats[route->cheat_count - 1].frame : 0u;
    const uint32_t settings = route->setting_count ? route->settings[route->setting_count - 1].frame : 0u;
    uint32_t last = keys > actions ? keys : actions;
    if (cheats > last) last = cheats;
    return last > settings ? last : settings;
}

int oracles_route_mask_at(const OraclesRoute *route, uint32_t frame, unsigned *mask)
{
    /* A route lasts to its last line, whatever its verb: an exchange noted after the last change of keys (the return
     * of a button's item, thirty quiet frames after a use) is part of it, the keys staying what they were. */
    if (route->count == 0 || frame > oracles_route_last_frame(route)) return 0;
    /* Last event at or before frame; events are sorted. */
    size_t low = 0, high = route->count;
    while (high - low > 1) {
        const size_t mid = low + (high - low) / 2;
        if (route->events[mid].frame <= frame) low = mid; else high = mid;
    }
    *mask = route->events[low].frame <= frame ? route->events[low].mask : 0;
    return 1;
}

int oracles_route_writer_open(OraclesRouteWriter *writer, const char *path, const OraclesRouteHeader *header)
{
    return oracles_route_writer_open_format(writer, path, header, 0);
}

int oracles_route_writer_open_format(OraclesRouteWriter *writer, const char *path, const OraclesRouteHeader *header, int at_least)
{
    memset(writer, 0, sizeof *writer);
    writer->file = fopen(path, "wb"); /* LF on every platform */
    if (!writer->file) return -1;
    char language[16];
    const int lowest = oracles_route_has_option(header, ORACLES_ROUTE_OPTION_CONTINUOUS_LINE) ? ORACLES_ROUTE_FORMAT_LINE
                     : oracles_route_has_option(header, ORACLES_ROUTE_OPTION_CONTINUOUS_STEPS) ? ORACLES_ROUTE_FORMAT_STEPS
                     : oracles_route_has_option(header, ORACLES_ROUTE_OPTION_TEXT_INSTANT) ? ORACLES_ROUTE_FORMAT_TEXT_INSTANT
                     : oracles_route_has_option(header, ORACLES_ROUTE_OPTION_TEXT_BOX_WIDE) ? ORACLES_ROUTE_FORMAT_TEXT_BOX_WIDE
                     : oracles_route_has_option(header, ORACLES_ROUTE_OPTION_TEXT_COMPACT) ? ORACLES_ROUTE_FORMAT_TEXT_COMPACT
                     : oracles_route_has_cheat(header) ? ORACLES_ROUTE_FORMAT_CHEATS
                     : oracles_route_has_option(header, ORACLES_ROUTE_OPTION_CONTINUOUS_DIAGONAL) ? ORACLES_ROUTE_FORMAT_DIAGONAL
                     : oracles_route_has_option(header, ORACLES_ROUTE_OPTION_SWIM_HOLD) ? ORACLES_ROUTE_FORMAT_SWIM_HOLD
                     : oracles_route_language(header, language, sizeof language)[0] ? ORACLES_ROUTE_FORMAT_LANGUAGE
                     : header->mods[0] ? ORACLES_ROUTE_FORMAT_MODS : ORACLES_ROUTE_FORMAT;
    const int format = at_least > lowest ? at_least : lowest;
    writer->format = format;
    fprintf(writer->file, "oracles-route %d\ngame %s\nrom_sha1 %s\nsram_sha1 %s\n", format,
            header->game, header->rom_sha1, header->sram_sha1);
    if (header->options[0]) fprintf(writer->file, "options %s\n", header->options);   /* a replay runs with them: without, it diverges */
    if (header->core[0]) fprintf(writer->file, "core %s\n", header->core);
    if (header->mods[0]) fprintf(writer->file, "mods %s\n", header->mods);   /* a replay runs the same mods: without them, it diverges at their first conversation */
    if (header->mods[0] && header->store_sha1[0]) fprintf(writer->file, "store_sha1 %s\n", header->store_sha1);   /* and from the same storage */
    fprintf(writer->file, "inputs\n");
    return 0;
}

void oracles_route_writer_record(OraclesRouteWriter *writer, uint32_t frame, unsigned mask)
{
    if (!writer->file) return;
    if (writer->has_last && writer->last_mask == mask) return;
    fprintf(writer->file, "%u keys %02x\n", frame, mask & 0xffu);
    writer->last_mask = mask;
    writer->has_last = 1;
}

int oracles_route_writer_action(OraclesRouteWriter *writer, const OraclesRouteAction *action)
{
    if (!writer->file) return 0;
    if (action->verb == ORACLES_ROUTE_THROW) {
        if (action->button > 1u || writer->format < ORACLES_ROUTE_FORMAT_SOUND) return -1;
        fprintf(writer->file, "%u throw %s\n", action->frame, action->button ? "a" : "b");
        return 0;
    }
    if (action->verb == ORACLES_ROUTE_SOUND) {
        if (!action->item || writer->format < ORACLES_ROUTE_FORMAT_SOUND) return -1;   /* the header says the format: one before the verb's cannot carry it */
        fprintf(writer->file, "%u sound %02x\n", action->frame, action->item);
        return 0;
    }
    if (action->verb == ORACLES_ROUTE_USE) {
        if (action->button > 1u) return -1;
        fprintf(writer->file, "%u use %s %02x\n", action->frame, action->button ? "a" : "b", action->item);
        return 0;
    }
    if (action->slot_a >= 18u || action->slot_b >= 18u || action->variant > 3u || (!action->swap && !action->variant)) return -1;
    fprintf(writer->file, "%u equip", action->frame);
    if (action->swap) {
        char a[8], b[8];
        slot_name(action->slot_a, a); slot_name(action->slot_b, b);
        fprintf(writer->file, " %s=%02x %s=%02x", a, action->item_a, b, action->item_b);
    }
    if (action->variant) fprintf(writer->file, " %s=%02x", variant_names[action->variant], action->variant_value);
    fprintf(writer->file, "\n");
    return 0;
}

int oracles_route_writer_cheat(OraclesRouteWriter *writer, uint32_t frame, const char *name, int on)
{
    if (!writer->file) return 0;
    if (!name[0] || strlen(name) >= sizeof ((OraclesRouteCheat *)0)->name || strpbrk(name, " \t\r\n")) return -1;
    /* The header, written at the start, says the format: a route opened in one before the verb's cannot carry it. */
    if (writer->format < ORACLES_ROUTE_FORMAT_CHEAT_VERB) return -1;
    fprintf(writer->file, "%u cheat %s %s\n", frame, name, on ? "on" : "off");
    return 0;
}

int oracles_route_writer_set(OraclesRouteWriter *writer, uint32_t frame, OraclesRouteSettingName name, int value)
{
    if (!writer->file) return 0;
    const char *text = oracles_route_setting_value(name, value);
    if (!text || writer->format < ORACLES_ROUTE_FORMAT_SET || (name == ORACLES_ROUTE_SET_TEXT_SPEED && writer->format < ORACLES_ROUTE_FORMAT_TEXT_INSTANT)) return -1;
    fprintf(writer->file, "%u set %s %s\n", frame, setting_names[name], text);
    return 0;
}

int oracles_route_writer_close(OraclesRouteWriter *writer)
{
    if (!writer->file) return 0;
    const int ok = fclose(writer->file) == 0;
    writer->file = NULL;
    return ok ? 0 : -1;
}
