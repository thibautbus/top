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

int oracles_route_read(const char *path, OraclesRoute *route, char *error, size_t error_capacity)
{
    memset(route, 0, sizeof *route);
    FILE *f = fopen(path, "r");
    if (!f) { set_error(error, error_capacity, "cannot open the route file", 0); return -1; }
    char line[1100];
    unsigned number = 0;
    int in_inputs = 0, format_seen = 0;
    uint32_t last_frame = 0, last_any = 0, last_of_verb[3] = { 0, 0, 0 };
    int seen_any = 0, seen_verb[3] = { 0, 0, 0 };
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
                if (route->format < 1u || route->format > ORACLES_ROUTE_FORMAT_MODS) { set_error(error, error_capacity, "unsupported route format", number); goto fail; }
                format_seen = 1;
            } else if (strcmp(key, "game") == 0) copy_field(route->header.game, sizeof route->header.game, value);
            else if (strcmp(key, "rom_sha1") == 0) copy_field(route->header.rom_sha1, sizeof route->header.rom_sha1, value);
            else if (strcmp(key, "sram_sha1") == 0) copy_field(route->header.sram_sha1, sizeof route->header.sram_sha1, value);
            else if (strcmp(key, "store_sha1") == 0) copy_field(route->header.store_sha1, sizeof route->header.store_sha1, value);
            else if (strcmp(key, "options") == 0) copy_field(route->header.options, sizeof route->header.options, value);
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
        const unsigned which = strcmp(verb, "keys") == 0 ? 0u : strcmp(verb, "equip") == 0 ? 1u : strcmp(verb, "use") == 0 ? 2u : 3u;
        if (which == 3u || (which != 0u && route->format < 2u)) { set_error(error, error_capacity, "unknown verb for this route format", number); goto fail; }
        if ((seen_any && frame < last_any) || (seen_verb[which] && frame <= last_of_verb[which])) { set_error(error, error_capacity, "frames must increase", number); goto fail; }
        seen_any = 1; last_any = (uint32_t)frame; seen_verb[which] = 1; last_of_verb[which] = (uint32_t)frame;
        char *rest = line + consumed;
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
        action.verb = which == 1u ? ORACLES_ROUTE_EQUIP : ORACLES_ROUTE_USE;
        if (which == 1u) {
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

uint32_t oracles_route_last_frame(const OraclesRoute *route)
{
    const uint32_t keys = route->count ? route->events[route->count - 1].frame : 0u;
    const uint32_t actions = route->action_count ? route->actions[route->action_count - 1].frame : 0u;
    return keys > actions ? keys : actions;
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
    memset(writer, 0, sizeof *writer);
    writer->file = fopen(path, "wb"); /* LF on every platform */
    if (!writer->file) return -1;
    fprintf(writer->file, "oracles-route %d\ngame %s\nrom_sha1 %s\nsram_sha1 %s\n", header->mods[0] ? ORACLES_ROUTE_FORMAT_MODS : ORACLES_ROUTE_FORMAT,
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

int oracles_route_writer_close(OraclesRouteWriter *writer)
{
    if (!writer->file) return 0;
    const int ok = fclose(writer->file) == 0;
    writer->file = NULL;
    return ok ? 0 : -1;
}
