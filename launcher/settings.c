#include "settings.h"

#include "hotkey_lines.h"
#include "guest_tables.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const pad_setting_names[4] = { "a", "b", "select", "start" };
/* Under the left hand that already holds Z (B) and X (A); on a controller, the buttons the launcher did not use. */
/* The defaults are Controls' (ui_controls_nav.c): its Reset to defaults and a new file agree. */

static const char *const game_names[ORACLES_SETTINGS_GAMES] = { "ages", "seasons" };
static const char *const profile_names[ORACLES_PROFILES] = { "faithful", "enhanced" };
static const char *const view_names[3] = { "near", "medium", "far" };   /* OraclesEnhancedLevel's order */
/* The launcher window at its first opening. */
#define LAUNCHER_WIDTH 1280
#define LAUNCHER_HEIGHT 720
/* A line holds a path of the full length after its name. */
#define LINE_LENGTH (ORACLES_SETTINGS_PATH_LENGTH + 64)

static void copy_name(char *out, const char *value) { snprintf(out, ORACLES_SETTINGS_NAME_LENGTH, "%s", value); }

/* A mod's name as its folder gives it. */
static int mod_name_ok(const char *name, size_t length)
{
    if (!length || length >= ORACLES_SETTINGS_MOD_NAME) return 0;
    for (size_t i = 0; i < length; i++)
        if (!((name[i] >= 'a' && name[i] <= 'z') || (name[i] >= '0' && name[i] <= '9') || name[i] == '-' || name[i] == '_')) return 0;
    return 1;
}

static int by_name(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }

/* The names of `text` (any order, any repeat, names that are none dropped), sorted, once each: its first eight. */
static size_t parse_mods(const char *text, char names[ORACLES_SETTINGS_MODS_MAX][ORACLES_SETTINGS_MOD_NAME])
{
    size_t count = 0;
    while (*text) {
        const size_t length = strcspn(text, ",");
        if (mod_name_ok(text, length)) {
            int known = 0;
            for (size_t i = 0; i < count; i++) known |= strlen(names[i]) == length && !strncmp(names[i], text, length);
            if (!known && count < ORACLES_SETTINGS_MODS_MAX) snprintf(names[count++], ORACLES_SETTINGS_MOD_NAME, "%.*s", (int)length, text);
        }
        text += length + (text[length] == ',');
    }
    if (count) qsort(names, count, sizeof names[0], by_name);
    return count;
}

static void join_mods(char list[ORACLES_SETTINGS_MODS_LENGTH], char names[][ORACLES_SETTINGS_MOD_NAME], size_t count)
{
    list[0] = 0;
    for (size_t i = 0; i < count; i++) {
        if (i) strcat(list, ",");
        strcat(list, names[i]);
    }
}

size_t oracles_settings_mod_names(const char *list, char names[][ORACLES_SETTINGS_MOD_NAME], size_t capacity)
{
    char all[ORACLES_SETTINGS_MODS_MAX][ORACLES_SETTINGS_MOD_NAME];
    const size_t count = parse_mods(list, all);
    for (size_t i = 0; i < count && i < capacity; i++) memcpy(names[i], all[i], sizeof all[i]);
    return count < capacity ? count : capacity;
}

int oracles_settings_mod_active(const char *list, const char *name)
{
    char names[ORACLES_SETTINGS_MODS_MAX][ORACLES_SETTINGS_MOD_NAME];
    const size_t count = oracles_settings_mod_names(list, names, ORACLES_SETTINGS_MODS_MAX);
    for (size_t i = 0; i < count; i++) if (!strcmp(names[i], name)) return 1;
    return 0;
}

int oracles_settings_mod_set(char list[ORACLES_SETTINGS_MODS_LENGTH], const char *name, int on)
{
    char names[ORACLES_SETTINGS_MODS_MAX][ORACLES_SETTINGS_MOD_NAME];
    size_t count = parse_mods(list, names), at = 0;
    while (at < count && strcmp(names[at], name) != 0) at++;
    if (on && at == count) {
        if (count == ORACLES_SETTINGS_MODS_MAX) return 0;
        if (!mod_name_ok(name, strlen(name))) return 1;
        snprintf(names[count++], ORACLES_SETTINGS_MOD_NAME, "%s", name);
        qsort(names, count, sizeof names[0], by_name);
    } else if (!on && at < count) {
        memmove(names[at], names[at + 1], (count - at - 1) * sizeof names[0]);
        count--;
    }
    join_mods(list, names, count);
    return 1;
}

void oracles_settings_mods_folder(const oracles_settings *s, char *out, size_t capacity)
{
    const char *slash = strrchr(s->path, '/'), *backslash = strrchr(s->path, '\\');
    if (backslash && (!slash || backslash > slash)) slash = backslash;
    if (!s->path[0] || !slash) { if (capacity) out[0] = 0; return; }
    snprintf(out, capacity, "%.*smods", (int)(slash - s->path + 1), s->path);
}

const char *oracles_settings_profile_name(OraclesProfile profile) { return profile_names[profile]; }
const char *oracles_settings_game_name(OraclesSettingsGame game) { return game_names[game]; }

void oracles_settings_defaults(oracles_settings *s)
{
    s->colour_correction = 1;   /* SameBoy's rendering, close to an original screen */
    snprintf(s->vsync, sizeof s->vsync, "auto");
    s->camera = 2;
    for (unsigned i = 0; i < ORACLES_BINDINGS; i++) copy_name(s->key_names[i], oracles_controls_default_keys[i]);
    for (unsigned i = 0; i < 4; i++) copy_name(s->pad_names[i], oracles_controls_default_pads[i]);
    for (unsigned i = 0; i < ORACLES_HOTKEY_SLOTS; i++) {
        copy_name(s->hotkey_key_names[i], oracles_controls_default_hotkey_keys[i]);
        copy_name(s->hotkey_pad_names[i], oracles_controls_default_hotkey_pads[i]);
    }
    copy_name(s->hotkey_bind_b, oracles_controls_default_hotkey_keys[ORACLES_HOTKEY_SLOTS]);
    copy_name(s->hotkey_bind_a, oracles_controls_default_hotkey_keys[ORACLES_HOTKEY_SLOTS + 1]);
    memset(s->hotkeys, 0, sizeof s->hotkeys);
    memset(s->rom, 0, sizeof s->rom);
    memset(s->patch, 0, sizeof s->patch);
    memset(s->mods, 0, sizeof s->mods);
    /* A first opening plays Enhanced, the view drawn back with the continuous transitions it carries, fullscreen. */
    s->profile = ORACLES_PROFILE_ENHANCED;
    s->transitions = 1;
    s->view = 2;   /* far: the view drawn back, as Enhanced was before the levels */
    for (unsigned g = 0; g < ORACLES_SETTINGS_GAMES; g++) s->item_hotkeys[g] = ORACLES_HOTKEYS_OFF;
    s->launcher_width = LAUNCHER_WIDTH;
    s->launcher_height = LAUNCHER_HEIGHT;
    s->window_scale = 0;   /* fullscreen; --rom keeps its own scale, 4 */
}

/* `rom_<game>=`, `patch_<fan game>=`, `profile=`, `transitions=`, `view=`, `item_hotkeys_<game>=`, `mods_<game>=`, `window_scale=` and `launcher_window=`: 1 when the
 * line was one of them.  A value the launcher does not know leaves the key's default. */
static int load_launcher(oracles_settings *s, const char *name, const char *value)
{
    if (!strcmp(name, "profile")) {
        for (unsigned p = 0; p < ORACLES_PROFILES; p++) if (!strcmp(value, profile_names[p])) s->profile = (OraclesProfile)p;
        return 1;
    }
    if (!strcmp(name, "transitions")) {
        if (!strcmp(value, "on") || !strcmp(value, "off")) s->transitions = !strcmp(value, "on");
        return 1;
    }
    if (!strcmp(name, "view")) {
        for (int v = 0; v < 3; v++) if (!strcmp(value, view_names[v])) s->view = v;
        return 1;
    }
    for (unsigned g = 0; g < ORACLES_SETTINGS_GAMES; g++) {
        char expected[32];
        snprintf(expected, sizeof expected, "rom_%s", game_names[g]);
        if (!strcmp(name, expected)) { snprintf(s->rom[g], sizeof s->rom[g], "%s", value); return 1; }
        snprintf(expected, sizeof expected, "mods_%s", game_names[g]);
        if (!strcmp(name, expected)) {
            char names[ORACLES_SETTINGS_MODS_MAX][ORACLES_SETTINGS_MOD_NAME];
            join_mods(s->mods[g], names, parse_mods(value, names));
            return 1;
        }
        snprintf(expected, sizeof expected, "item_hotkeys_%s", game_names[g]);
        if (!strcmp(name, expected)) {
            if (!strcmp(value, "off") || !strcmp(value, "use") || !strcmp(value, "equip")) s->item_hotkeys[g] = oracles_settings_hotkeys_mode(value);
            return 1;
        }
    }
    for (unsigned f = 0; f < ORACLES_HOME_FAN_GAMES; f++) {
        char expected[32];
        snprintf(expected, sizeof expected, "patch_%s", oracles_home_fan_games[f].key);
        if (!strcmp(name, expected)) { snprintf(s->patch[f], sizeof s->patch[f], "%s", value); return 1; }
    }
    if (!strcmp(name, "window_scale")) {
        if (!strcmp(value, "full")) s->window_scale = 0;
        else if (!strcmp(value, "2") || !strcmp(value, "3") || !strcmp(value, "4")) s->window_scale = atoi(value);
        return 1;
    }
    if (strcmp(name, "launcher_window") != 0) return 0;
    int width = 0, height = 0;
    if (sscanf(value, "%dx%d", &width, &height) == 2 && width > 0 && height > 0) { s->launcher_width = width; s->launcher_height = height; }
    return 1;
}

OraclesHotkeysMode oracles_settings_hotkeys_mode(const char *text)
{
    if (text && !strcmp(text, "use")) return ORACLES_HOTKEYS_USE;
    if (text && !strcmp(text, "equip")) return ORACLES_HOTKEYS_EQUIP;
    return ORACLES_HOTKEYS_OFF;
}

/* `prefix<suffix>=value`: 1 when the line was that name.  An empty value is a key or a button left without one
 * (Controls gives a key taken by another cell away); a name SDL does not know is the backend's to report. */
static int load_name(const char *name, const char *value, const char *prefix, const char *suffix, char *out)
{
    char expected[ORACLES_SETTINGS_NAME_LENGTH * 2];
    snprintf(expected, sizeof expected, "%s%s", prefix, suffix);
    if (strcmp(name, expected) != 0) return 0;
    copy_name(out, value);
    return 1;
}

void oracles_settings_load(oracles_settings *s)
{
    if (!s->path[0]) return;
    FILE *f = fopen(s->path, "r");
    if (!f) return;
    char line[LINE_LENGTH];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *equals = strchr(line, '=');
        if (!equals || line[0] == '#') continue;
        *equals = 0;
        const char *name = line, *value = equals + 1;
        if (!strcmp(name, "colour_correction")) { s->colour_correction = atoi(value) != 0; continue; }
        if (!strcmp(name, "vsync")) { if (!strcmp(value, "on") || !strcmp(value, "off") || !strcmp(value, "auto")) snprintf(s->vsync, sizeof s->vsync, "%s", value); continue; }
        if (!strcmp(name, "camera")) { s->camera = atoi(value) == 1 ? 1 : 2; continue; }
        if (load_launcher(s, name, value)) continue;
        if (load_name(name, value, "key_hotkey_bind_", "b", s->hotkey_bind_b) || load_name(name, value, "key_hotkey_bind_", "a", s->hotkey_bind_a)) continue;
        unsigned game = 0, n = 0;
        OraclesHotkeySlot slot;
        if (oracles_hotkey_line_parse(name, value, &game, &n, &slot)) { s->hotkeys[game][n] = slot; continue; }
        for (unsigned i = 0; i < ORACLES_BINDINGS; i++) load_name(name, value, "key_", oracles_sdl_binding_names[i], s->key_names[i]);
        for (unsigned i = 0; i < 4; i++) load_name(name, value, "pad_", pad_setting_names[i], s->pad_names[i]);
        for (unsigned i = 0; i < ORACLES_HOTKEY_SLOTS; i++) {
            const char digit[2] = { (char)('1' + i), 0 };
            load_name(name, value, "key_hotkey", digit, s->hotkey_key_names[i]);
            load_name(name, value, "pad_hotkey", digit, s->hotkey_pad_names[i]);
        }
    }
    fclose(f);
}

int oracles_settings_store(const oracles_settings *s)
{
    if (!s->path[0]) return 0;
    /* Through a temporary file, as the SRAM: a write cut short (a full disk, the machine stopping) leaves the settings
     * as they were rather than half a file. */
    char temporary[sizeof s->path + 8];
    snprintf(temporary, sizeof temporary, "%s.tmp", s->path);
    FILE *f = fopen(temporary, "w");
    if (!f) return 0;
    fprintf(f, "# The Oracles Project settings. Key names are SDL key names (Right, X, Return, Space, F1...);\n"
               "# pad names are SDL game controller buttons (a, b, x, y, back, start, leftshoulder...); an empty name is none.\n");
    fprintf(f, "colour_correction=%d\n", s->colour_correction ? 1 : 0);
    fprintf(f, "vsync=%s\n", s->vsync);
    fprintf(f, "camera=%d\n", s->camera);
    fprintf(f, "# The home screen: each game's ROM (Cartridge's Choose ROM, or a file dropped on the window) and its item hotkeys\n"
               "# (off|use, Controls' Off or On, for the games the home screen starts; equip as --item-hotkeys=equip), and the\n"
               "# launcher window's size.\n");
    static const char *const hotkey_names[3] = { "off", "use", "equip" };
    for (unsigned g = 0; g < ORACLES_SETTINGS_GAMES; g++) fprintf(f, "rom_%s=%s\n", game_names[g], s->rom[g]);
    fprintf(f, "# Each fan game's BPS patch (Cartridge's Choose patch), applied to its Oracle's ROM: Gifts of Kinomi's to\n"
               "# rom_ages, Moonrise Regalia's to rom_ages, Temple of Seasons' to rom_seasons.\n");
    for (unsigned g = 0; g < ORACLES_HOME_FAN_GAMES; g++) fprintf(f, "patch_%s=%s\n", oracles_home_fan_games[g].key, s->patch[g]);
    for (unsigned g = 0; g < ORACLES_SETTINGS_GAMES; g++) fprintf(f, "item_hotkeys_%s=%s\n", game_names[g], hotkey_names[s->item_hotkeys[g]]);
    fprintf(f, "# Each game's active mods (its Mods page), the names of their folders in the mods folder beside this file.\n");
    for (unsigned g = 0; g < ORACLES_SETTINGS_GAMES; g++) fprintf(f, "mods_%s=%s\n", game_names[g], s->mods[g]);
    fprintf(f, "# Display, for the games the home screen starts: the profile (faithful or enhanced), the view in enhanced (near,\n"
               "# medium or far, how much of the world it shows, in the screen's shape), the continuous transitions (off|on, in\n"
               "# enhanced only), and the window, 2, 3 or 4 times its surface or full (the screen); enhanced, far, on and full at\n"
               "# the first opening.\n");
    fprintf(f, "profile=%s\n", profile_names[s->profile]);
    fprintf(f, "view=%s\n", view_names[s->view >= 0 && s->view < 3 ? s->view : 2]);
    fprintf(f, "transitions=%s\n", s->transitions ? "on" : "off");
    if (s->window_scale) fprintf(f, "window_scale=%d\n", s->window_scale); else fprintf(f, "window_scale=full\n");
    fprintf(f, "launcher_window=%dx%d\n", s->launcher_width, s->launcher_height);
    for (unsigned i = 0; i < ORACLES_BINDINGS; i++) fprintf(f, "key_%s=%s\n", oracles_sdl_binding_names[i], s->key_names[i]);
    for (unsigned i = 0; i < 4; i++) fprintf(f, "pad_%s=%s\n", pad_setting_names[i], s->pad_names[i]);
    fprintf(f, "# Item hotkeys (--item-hotkeys=use|equip, for the run that asks): a key per slot; held with the bind keys it takes the item of B or of A,\n"
               "# and in the inventory it takes the item under the cursor. A slot is <b|a>:<item>:<variant or -->, in the ROM's own identifiers.\n");
    for (unsigned i = 0; i < ORACLES_HOTKEY_SLOTS; i++) fprintf(f, "key_hotkey%u=%s\n", i + 1u, s->hotkey_key_names[i]);
    fprintf(f, "key_hotkey_bind_b=%s\nkey_hotkey_bind_a=%s\n", s->hotkey_bind_b, s->hotkey_bind_a);
    for (unsigned i = 0; i < ORACLES_HOTKEY_SLOTS; i++) fprintf(f, "pad_hotkey%u=%s\n", i + 1u, s->hotkey_pad_names[i]);
    for (unsigned game = 0; game < 2u; game++)
        for (unsigned i = 0; i < ORACLES_HOTKEY_SLOTS; i++) {
            char line[64];
            /* The item's name above its line, from the generated tables; an identifier they do not know (a modified ROM) stays bare. */
            const OraclesHotkeySlot *slot = &s->hotkeys[game][i];
            const OraclesGuestTables *tables = game == 0 ? &oracles_guest_tables_ages : &oracles_guest_tables_seasons;
            if (slot->set && slot->item < ORACLES_GUEST_ITEM_LABELS && tables->item_names[slot->item] && tables->item_names[slot->item][0])
                fprintf(f, "# %s\n", tables->item_names[slot->item]);
            if (oracles_hotkey_line_format(line, sizeof line, game, i, slot)) fprintf(f, "%s\n", line);
        }
    const int written = !ferror(f) && fflush(f) == 0;
    if (fclose(f) != 0 || !written) { remove(temporary); return 0; }
    if (rename(temporary, s->path) == 0) return 1;
    remove(s->path);   /* where rename does not replace a file */
    if (rename(temporary, s->path) == 0) return 1;
    remove(temporary);
    return 0;
}
