#include "settings.h"

#include "core.h"

#include "hotkey_lines.h"
#include "guest_tables.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const pad_setting_names[4] = { "a", "b", "select", "start" };
/* Under the left hand that already holds Z (B) and X (A); on a controller, the buttons the launcher did not use. */
/* The defaults are Controls' (ui_controls_nav.c): its Reset to defaults and a new file agree. */

static const char *const game_names[ORACLES_SETTINGS_GAMES] = { "ages", "seasons" };
static const char *const profile_names[ORACLES_PROFILES] = { "faithful", "enhanced" };
static const char *const view_names[3] = { "near", "medium", "far" };   /* OraclesEnhancedLevel's order */
const char *const oracles_settings_core_names[2] = { "sameboy", "mgba" };   /* OraclesCoreKind's order */
const char *const oracles_settings_quality_names[5] = { "low", "medium", "high", "max", "custom" };   /* OraclesQuality's order */
const char *const oracles_settings_language_names[ORACLES_SETTINGS_LANGUAGES] = { "en", "en-eu", "fr", "de", "it", "es" };

/* Each profile's view's level (an OraclesEnhancedLevel), core (an OraclesCoreKind) and neighbour workers (0 auto). */
static const struct { int view, core, ghosts; } qualities[ORACLES_QUALITIES] = {
    { 0, ORACLES_CORE_MGBA, 0 },      /* Low: near */
    { 1, ORACLES_CORE_MGBA, 0 },      /* Medium */
    { 2, ORACLES_CORE_MGBA, 0 },      /* High: far */
    { 2, ORACLES_CORE_SAMEBOY, 2 },   /* Max */
};

static int default_core(void)
{
#ifdef ORACLES_DEFAULT_CORE_KIND
    return ORACLES_DEFAULT_CORE_KIND;
#else
    return ORACLES_CORE_SAMEBOY;   /* a build that names none, as the tests' */
#endif
}

OraclesQuality oracles_settings_quality_of(int view, int core, int ghosts)
{
    for (int q = 0; q < ORACLES_QUALITIES; q++)
        if (view == qualities[q].view && core == qualities[q].core && ghosts == qualities[q].ghosts) return (OraclesQuality)q;
    return ORACLES_QUALITY_CUSTOM;
}

OraclesQuality oracles_settings_quality(const oracles_settings *s)
{
    return oracles_settings_quality_of(s->view, oracles_settings_core(s), s->ghosts);
}

void oracles_settings_apply_quality(oracles_settings *s, OraclesQuality quality)
{
    if (quality < ORACLES_QUALITY_LOW || quality >= ORACLES_QUALITY_CUSTOM) return;
    s->view = qualities[quality].view;
    s->ghosts = qualities[quality].ghosts;
    s->core = s->core < 0 && qualities[quality].core == default_core() ? -1 : qualities[quality].core;
}

OraclesQuality oracles_settings_default_quality(int fullscreen_only, int processor_threads, int ram_mb)
{
    if (!fullscreen_only) return ORACLES_QUALITY_MAX;
    return processor_threads >= 8 && ram_mb >= 6 * 1024 - 512 ? ORACLES_QUALITY_HIGH : ORACLES_QUALITY_MEDIUM;   /* "6 GB" devices report a little less */
}

int oracles_settings_auto_vsync(unsigned display_hz, int android)
{
    return !android && display_hz >= 59u && display_hz <= 61u;
}

OraclesQuality oracles_settings_lighter_quality(OraclesQuality quality)
{
    return quality > ORACLES_QUALITY_LOW && quality < ORACLES_QUALITY_CUSTOM ? (OraclesQuality)(quality - 1) : ORACLES_QUALITY_CUSTOM;
}

int oracles_settings_ran_slowly(int enhanced, uint32_t frames, uint32_t late)
{
    return enhanced && frames >= ORACLES_SLOW_FRAMES && (uint64_t)late * 50u > frames;
}

int oracles_settings_lighter(const oracles_settings *s, OraclesQuality *quality, int *view)
{
    *quality = oracles_settings_quality(s);
    *view = s->view;
    if (*quality != ORACLES_QUALITY_CUSTOM) {
        *quality = oracles_settings_lighter_quality(*quality);
        return *quality != ORACLES_QUALITY_CUSTOM;
    }
    if (s->view <= 0 || s->view > 2) return 0;
    *view = s->view - 1;
    return 1;
}

int oracles_settings_slow_hint(oracles_settings *s, int enhanced, uint32_t frames, uint32_t late)
{
    OraclesQuality lighter;
    int view;
    const OraclesQuality quality = oracles_settings_quality(s);
    if (!oracles_settings_ran_slowly(enhanced, frames, late) || !oracles_settings_lighter(s, &lighter, &view) || s->quality_hint == (int)quality) return 0;
    s->quality_hint = (int)quality;
    return 1;
}

void oracles_settings_quality_changed(oracles_settings *s, int view, int core, int ghosts)
{
    if (s->view != view || oracles_settings_core(s) != core || s->ghosts != ghosts) s->quality_hint = -1;
}

int oracles_settings_core(const oracles_settings *s)
{
    if (s->core == ORACLES_CORE_SAMEBOY || s->core == ORACLES_CORE_MGBA) return s->core;
    return default_core();
}

int oracles_settings_default_subpixel_camera(int fullscreen_only, unsigned scale)
{
    return !fullscreen_only || scale >= ORACLES_SETTINGS_SUBPIXEL_SCALE;
}

unsigned oracles_settings_subpixel_default_from(void)
{
#ifdef __ANDROID__
    return ORACLES_SETTINGS_SUBPIXEL_SCALE;
#else
    return 0u;
#endif
}

int oracles_settings_subpixel_camera(const oracles_settings *s, unsigned scale)
{
    if (s->subpixel_camera >= 0) return s->subpixel_camera != 0;
#ifdef __ANDROID__
    return oracles_settings_default_subpixel_camera(1, scale);
#else
    return oracles_settings_default_subpixel_camera(0, scale);
#endif
}

const char *const oracles_settings_aspect_names[3] = { "auto", "16:9", "4:3" };   /* ORACLES_ASPECT_AUTO's order */
const char *const oracles_settings_hud_names[3] = { "original", "full", "wide" };
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

/* `name` beside settings.txt; empty when there is no settings directory. */
static void beside(const oracles_settings *s, const char *name, char *out, size_t capacity)
{
    const char *slash = strrchr(s->path, '/'), *backslash = strrchr(s->path, '\\');
    if (backslash && (!slash || backslash > slash)) slash = backslash;
    if (!s->path[0] || !slash) { if (capacity) out[0] = 0; return; }
    snprintf(out, capacity, "%.*s%s", (int)(slash - s->path + 1), s->path, name);
}

void oracles_settings_mods_folder(const oracles_settings *s, char *out, size_t capacity)
{
    if (s->mods_path[0]) snprintf(out, capacity, "%s", s->mods_path);
    else beside(s, "mods", out, capacity);
}

void oracles_settings_language_pack(const oracles_settings *s, OraclesSettingsGame g, char *out, size_t capacity)
{
    char name[32];
    snprintf(name, sizeof name, "language-%s.pack", game_names[g]);
    beside(s, name, out, capacity);
}

void oracles_settings_fan_language_pack(const oracles_settings *s, unsigned f, char *out, size_t capacity)
{
    char name[32];
    snprintf(name, sizeof name, "language-%s.pack", oracles_home_fan_games[f].key);
    beside(s, name, out, capacity);
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
    copy_name(s->menu_pad, oracles_controls_default_menu_pad);
    for (unsigned i = 0; i < ORACLES_HOTKEY_SLOTS; i++) {
        copy_name(s->hotkey_key_names[i], oracles_controls_default_hotkey_keys[i]);
        copy_name(s->hotkey_pad_names[i], oracles_controls_default_hotkey_pads[i]);
    }
    copy_name(s->hotkey_bind_b, oracles_controls_default_hotkey_keys[ORACLES_HOTKEY_SLOTS]);
    copy_name(s->hotkey_bind_a, oracles_controls_default_hotkey_keys[ORACLES_HOTKEY_SLOTS + 1]);
    memset(s->hotkeys, 0, sizeof s->hotkeys);
    memset(s->rom, 0, sizeof s->rom);
    memset(s->patch, 0, sizeof s->patch);
    memset(s->eu_rom, 0, sizeof s->eu_rom);
    memset(s->language, 0, sizeof s->language);   /* English, the US ROM's own */
    memset(s->fan_language, 0, sizeof s->fan_language);
    memset(s->mods, 0, sizeof s->mods);
    s->mods_path[0] = 0;   /* beside settings.txt, until Android's shared storage names it */
    memset(s->cheats, 0, sizeof s->cheats);   /* the game as it plays */
    /* A first opening plays Enhanced, the view drawn back with the continuous transitions it carries, fullscreen. */
    s->profile = ORACLES_PROFILE_ENHANCED;
    s->transitions = 1;
    s->swim_hold = 0;   /* the game's own taps */
    s->compact_text = 0;   /* the game's own fixed width */
    s->text_box_wide = 0;  /* the game's own box */
    s->text_size = 0;   /* the game's own size */
    s->text_instant = 0;   /* the game's own pace */
    s->view = 2;   /* far: the view drawn back, as Enhanced was before the levels */
    s->aspect = ORACLES_ASPECT_AUTO;
    s->far_fixed = 0;   /* fitted to the screen */
    s->unvisited_hidden = 0;   /* every room around shown */
    s->ghosts = 0;   /* auto */
    s->hud = 1;   /* full: the bar across the whole view, not a strip between two dark bands */
#ifdef __ANDROID__
    s->menus_large = 1;   /* a small screen: the framed core at the view's scale, often 1 or 2, is hard to read */
#else
    s->menus_large = 0;
#endif
    /* No core chosen: the build's default plays, and is not written, so that a build with another default (an update
     * of the application, a build on the other core) plays its own. */
    s->core = -1;
    s->subpixel_camera = -1;   /* not chosen: the platform's default, not written, so that another default can come */
    for (unsigned g = 0; g < ORACLES_SETTINGS_GAMES; g++) s->item_hotkeys[g] = ORACLES_HOTKEYS_OFF;
    s->launcher_width = LAUNCHER_WIDTH;
    s->launcher_height = LAUNCHER_HEIGHT;
    s->window_scale = 0;   /* fullscreen; --rom keeps its own scale, 4 */
    s->scaling_fill = 0;   /* sharp, Android included */
    s->screen_margin = ORACLES_SCREEN_MARGIN_OFF;
    s->quality_hint = -1;
    s->menu_toast_shown = 0;
    s->update_check = 1;
    s->first_run = 0;
}

/* `rom_<game>=`, `patch_<fan game>=`, `eu_rom_<game>=`, `language_<game>=`, `language_<fan game>=`, `profile=`, `transitions=`, `swim_hold=`, `text=`, `text_box=`, `text_size=`, `text_speed=`, `view=`, `aspect=`, `far=`, `unvisited_rooms=`, `ghosts=`, `menus=`, `hud=`, `core=`, `item_hotkeys_<game>=`, `mods_<game>=`, `cheats_<game>=`, `window_scale=`, `scaling=`, `screen_margin=`, `quality_hint=`, `menu_toast=`, `update_check=` and `launcher_window=`: 1 when the
 * line was one of them.  `value` is the line's after its =, without the spaces around it (the paths'), `word` the same in lower
 * case (the other keys'): `update_check = OFF` reads as update_check=off.  A value the launcher does not know leaves the
 * key's default. */
static const char *const text_size_names[3] = { "normal", "large", "xlarge" };

static int load_launcher(oracles_settings *s, const char *name, const char *value, const char *word)
{
    if (!strcmp(name, "profile")) {
        for (unsigned p = 0; p < ORACLES_PROFILES; p++) if (!strcmp(word, profile_names[p])) s->profile = (OraclesProfile)p;
        return 1;
    }
    if (!strcmp(name, "transitions")) {
        if (!strcmp(word, "on") || !strcmp(word, "off")) s->transitions = !strcmp(word, "on");
        return 1;
    }
    if (!strcmp(name, "swim_hold")) {
        if (!strcmp(word, "on") || !strcmp(word, "off")) s->swim_hold = !strcmp(word, "on");
        return 1;
    }
    if (!strcmp(name, "text")) {
        if (!strcmp(word, "classic") || !strcmp(word, "compact")) s->compact_text = !strcmp(word, "compact");
        return 1;
    }
    if (!strcmp(name, "text_size")) {
        for (int size = 0; size < 3; size++) if (!strcmp(word, text_size_names[size])) s->text_size = size;
        return 1;
    }
    if (!strcmp(name, "text_speed")) {
        if (!strcmp(word, "classic") || !strcmp(word, "instant")) s->text_instant = !strcmp(word, "instant");
        return 1;
    }
    if (!strcmp(name, "text_box")) {
        if (!strcmp(word, "standard") || !strcmp(word, "wide")) s->text_box_wide = !strcmp(word, "wide");
        return 1;
    }
    if (!strcmp(name, "view")) {
        for (int v = 0; v < 3; v++) if (!strcmp(word, view_names[v])) s->view = v;
        return 1;
    }
    if (!strcmp(name, "core")) {
        for (int c = 0; c < 2; c++) if (!strcmp(word, oracles_settings_core_names[c])) s->core = c;
        return 1;
    }
    if (!strcmp(name, "menus")) {
        if (!strcmp(word, "large") || !strcmp(word, "view")) s->menus_large = !strcmp(word, "large");
        return 1;
    }
    if (!strcmp(name, "hud")) {
        for (int i = 0; i < 3; i++) if (!strcmp(word, oracles_settings_hud_names[i])) s->hud = i;
        return 1;
    }
    if (!strcmp(name, "menu_toast")) {
        if (!strcmp(word, "due") || !strcmp(word, "shown")) s->menu_toast_shown = !strcmp(word, "shown");
        return 1;
    }
    if (!strcmp(name, "update_check")) {
        if (!strcmp(word, "on") || !strcmp(word, "off")) s->update_check = !strcmp(word, "on");
        return 1;
    }
    if (!strcmp(name, "ghosts")) {
        if (!strcmp(word, "auto") || !strcmp(word, "1") || !strcmp(word, "2")) s->ghosts = !strcmp(word, "auto") ? 0 : word[0] - '0';
        return 1;
    }
    if (!strcmp(name, "aspect")) {
        for (int a = 0; a < 3; a++) if (!strcmp(word, oracles_settings_aspect_names[a])) s->aspect = a;
        return 1;
    }
    if (!strcmp(name, "far")) {
        if (!strcmp(word, "screen") || !strcmp(word, "fixed")) s->far_fixed = !strcmp(word, "fixed");
        return 1;
    }
    if (!strcmp(name, "unvisited_rooms")) {
        if (!strcmp(word, "shown") || !strcmp(word, "hidden")) s->unvisited_hidden = !strcmp(word, "hidden");
        return 1;
    }
    for (unsigned g = 0; g < ORACLES_SETTINGS_GAMES; g++) {
        char expected[32];
        snprintf(expected, sizeof expected, "rom_%s", game_names[g]);
        if (!strcmp(name, expected)) { snprintf(s->rom[g], sizeof s->rom[g], "%s", value); return 1; }
        snprintf(expected, sizeof expected, "eu_rom_%s", game_names[g]);
        if (!strcmp(name, expected)) { snprintf(s->eu_rom[g], sizeof s->eu_rom[g], "%s", value); return 1; }
        snprintf(expected, sizeof expected, "language_%s", game_names[g]);
        if (!strcmp(name, expected)) {
            for (int l = 0; l < ORACLES_SETTINGS_LANGUAGES; l++) if (!strcmp(word, oracles_settings_language_names[l])) s->language[g] = l;
            return 1;
        }
        snprintf(expected, sizeof expected, "mods_%s", game_names[g]);
        if (!strcmp(name, expected)) {
            char names[ORACLES_SETTINGS_MODS_MAX][ORACLES_SETTINGS_MOD_NAME];
            join_mods(s->mods[g], names, parse_mods(word, names));
            return 1;
        }
        snprintf(expected, sizeof expected, "cheats_%s", game_names[g]);
        if (!strcmp(name, expected)) {
            /* A later version's list (a cheat this one does not know): the cheats it knows. */
            unsigned cheats = 0;
            if (oracles_cheats_parse(word, NULL, &cheats) != 0) fprintf(stderr, "oracles: %s=%s names a cheat this version does not know; it is dropped\n", name, value);
            s->cheats[g] = cheats;
            return 1;
        }
        snprintf(expected, sizeof expected, "item_hotkeys_%s", game_names[g]);
        if (!strcmp(name, expected)) {
            if (!strcmp(word, "off") || !strcmp(word, "use") || !strcmp(word, "equip")) s->item_hotkeys[g] = oracles_settings_hotkeys_mode(word);
            return 1;
        }
    }
    for (unsigned f = 0; f < ORACLES_HOME_FAN_GAMES; f++) {
        char expected[32];
        snprintf(expected, sizeof expected, "patch_%s", oracles_home_fan_games[f].key);
        if (!strcmp(name, expected)) { snprintf(s->patch[f], sizeof s->patch[f], "%s", value); return 1; }
        snprintf(expected, sizeof expected, "language_%s", oracles_home_fan_games[f].key);
        if (!strcmp(name, expected)) {
            for (int l = 0; l < ORACLES_SETTINGS_LANGUAGES; l++) if (!strcmp(word, oracles_settings_language_names[l])) s->fan_language[f] = l;
            return 1;
        }
    }
    if (!strcmp(name, "window_scale")) {
        if (!strcmp(word, "full")) s->window_scale = 0;
        else if (!strcmp(word, "2") || !strcmp(word, "3") || !strcmp(word, "4")) s->window_scale = atoi(word);
        return 1;
    }
    if (!strcmp(name, "scaling")) {
        if (!strcmp(word, "sharp") || !strcmp(word, "fill")) s->scaling_fill = !strcmp(word, "fill");
        return 1;
    }
    if (!strcmp(name, "screen_margin")) {
        s->screen_margin = ORACLES_SCREEN_MARGIN_OFF;   /* unknown: off, the whole screen */
        for (int m = 0; m < ORACLES_SCREEN_MARGINS; m++) if (!strcmp(word, oracles_screen_margin_names[m])) s->screen_margin = m;
        return 1;
    }
    if (!strcmp(name, "quality_hint")) {
        for (int q = 0; q <= ORACLES_QUALITY_CUSTOM; q++) if (!strcmp(word, oracles_settings_quality_names[q])) s->quality_hint = q;
        return 1;
    }
    if (strcmp(name, "launcher_window") != 0) return 0;
    int width = 0, height = 0;
    if (sscanf(word, "%dx%d", &width, &height) == 2 && width > 0 && height > 0) { s->launcher_width = width; s->launcher_height = height; }
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

/* `text` without the spaces and tabs at its ends, in place. */
static char *trim(char *text)
{
    while (*text == ' ' || *text == '\t') text++;
    size_t length = strlen(text);
    while (length && (text[length - 1] == ' ' || text[length - 1] == '\t')) text[--length] = 0;
    return text;
}

static void lower(char *text)
{
    for (; *text; text++) if (*text >= 'A' && *text <= 'Z') *text = (char)(*text - 'A' + 'a');
}

/* A line whose key this version does not know, kept as it was after the known key `after`. */
static void keep_unknown(oracles_settings *s, const char *after, const char *line)
{
    size_t used = strlen(s->unknown), lines = 0;
    for (const char *p = s->unknown; *p; p++) lines += *p == '\n';
    const size_t length = strlen(after) + strlen(line) + 2;
    if (lines >= ORACLES_SETTINGS_UNKNOWN_LINES || used + length >= sizeof s->unknown) {
        fprintf(stderr, "oracles: settings.txt: too many keys this version does not know; %s is dropped\n", line);
        return;
    }
    snprintf(s->unknown + used, sizeof s->unknown - used, "%s\t%s\n", after, line);
}

void oracles_settings_load(oracles_settings *s)
{
    if (!s->path[0]) return;
    FILE *f = fopen(s->path, "r");
    if (!f) { s->first_run = 1; return; }
    s->unknown[0] = 0;
    char line[LINE_LENGTH], kept[LINE_LENGTH], trimmed[LINE_LENGTH], word[LINE_LENGTH], after[64] = "";
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *equals = strchr(line, '=');
        if (!equals || line[strspn(line, " \t")] == '#') continue;
        snprintf(kept, sizeof kept, "%s", line);
        *equals = 0;
        char *name = trim(line);
        lower(name);
        snprintf(trimmed, sizeof trimmed, "%s", equals + 1);
        const char *value = trim(trimmed);
        snprintf(word, sizeof word, "%s", value);
        lower(word);
        int known = 1;
        if (!strcmp(name, "colour_correction")) s->colour_correction = atoi(word) != 0;
        else if (!strcmp(name, "vsync")) { if (!strcmp(word, "on") || !strcmp(word, "off") || !strcmp(word, "auto")) snprintf(s->vsync, sizeof s->vsync, "%.4s", word); }
        else if (!strcmp(name, "camera")) s->camera = atoi(word) == 1 ? 1 : 2;
        else if (!strcmp(name, "link_phase")) s->link_phase = atoi(word) != 0;
        else if (!strcmp(name, "subpixel_camera")) s->subpixel_camera = atoi(word) != 0;   /* set either way */
        else if (load_launcher(s, name, value, word)) {}
        else if (load_name(name, value, "key_hotkey_bind_", "b", s->hotkey_bind_b) || load_name(name, value, "key_hotkey_bind_", "a", s->hotkey_bind_a)) {}
        else {
            unsigned game = 0, n = 0;
            OraclesHotkeySlot slot;
            known = 0;
            if (oracles_hotkey_line_parse(name, value, &game, &n, &slot)) { s->hotkeys[game][n] = slot; known = 1; }
            for (unsigned i = 0; !known && i < ORACLES_BINDINGS; i++) known = load_name(name, value, "key_", oracles_sdl_binding_names[i], s->key_names[i]);
            for (unsigned i = 0; !known && i < 4; i++) known = load_name(name, value, "pad_", pad_setting_names[i], s->pad_names[i]);
            if (!known) known = load_name(name, value, "menu_", "pad", s->menu_pad);
            for (unsigned i = 0; !known && i < ORACLES_HOTKEY_SLOTS; i++) {
                const char digit[2] = { (char)('1' + i), 0 };
                known = load_name(name, value, "key_hotkey", digit, s->hotkey_key_names[i]) || load_name(name, value, "pad_hotkey", digit, s->hotkey_pad_names[i]);
            }
        }
        if (known) snprintf(after, sizeof after, "%s", name);
        else keep_unknown(s, after, kept);
    }
    fclose(f);
}

/* settings.txt as it is written: after each key's line, the unknown lines read after that key (oracles_settings.unknown). */
typedef struct writer {
    FILE *f;
    const oracles_settings *s;
    unsigned char placed[ORACLES_SETTINGS_UNKNOWN_LINES];
} writer;

/* The unknown lines kept after the key `name` (`length` characters; "" for the file's start), not yet written; NULL: all
 * of them that are left, at the file's end. */
static void place(writer *w, const char *name, size_t length)
{
    unsigned index = 0;
    for (const char *entry = w->s->unknown; *entry && index < ORACLES_SETTINGS_UNKNOWN_LINES; index++) {
        const size_t end = strcspn(entry, "\n"), tab = strcspn(entry, "\t");
        if (tab < end && !w->placed[index] && (!name || (tab == length && !strncmp(entry, name, length)))) {
            fprintf(w->f, "%.*s\n", (int)(end - tab - 1), entry + tab + 1);
            w->placed[index] = 1;
        }
        entry += end + (entry[end] == '\n');
    }
}

static void put(writer *w, const char *format, ...)
{
    char text[LINE_LENGTH * 2];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof text, format, args);
    va_end(args);
    for (const char *line = text; *line;) {
        const size_t length = strcspn(line, "\n");
        fprintf(w->f, "%.*s\n", (int)length, line);
        const char *equals = memchr(line, '=', length);
        if (line[0] != '#' && equals) place(w, line, (size_t)(equals - line));
        line += length + (line[length] == '\n');
    }
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
    writer w = { f, s, { 0 } };
    put(&w, "# The Oracles Project settings. Key names are SDL key names (Right, X, Return, Space, F1...);\n"
               "# pad names are SDL game controller buttons (a, b, x, y, back, start, leftshoulder...); an empty name is none.\n");
    place(&w, "", 0);
    put(&w, "colour_correction=%d\n", s->colour_correction ? 1 : 0);
    put(&w, "vsync=%s\n", s->vsync);
    put(&w, "camera=%d\n", s->camera);
    if (s->link_phase) put(&w, "link_phase=1\n");
    if (s->subpixel_camera >= 0) put(&w, "subpixel_camera=%d\n", s->subpixel_camera != 0);
    put(&w, "# The home screen: each game's ROM (Cartridge's Choose ROM, or a file dropped on the window) and its item hotkeys\n"
               "# (off|use, Controls' Off or On, for the games the home screen starts; equip as --item-hotkeys=equip), and the\n"
               "# launcher window's size.\n");
    static const char *const hotkey_names[3] = { "off", "use", "equip" };
    for (unsigned g = 0; g < ORACLES_SETTINGS_GAMES; g++) put(&w, "rom_%s=%s\n", game_names[g], s->rom[g]);
    put(&w, "# Each fan game's BPS patch (Cartridge's Choose patch), applied to its Oracle's ROM: Gifts of Kinomi's to\n"
               "# rom_ages, Moonrise Regalia's to rom_ages, Temple of Seasons' to rom_seasons.\n");
    for (unsigned g = 0; g < ORACLES_HOME_FAN_GAMES; g++) put(&w, "patch_%s=%s\n", oracles_home_fan_games[g].key, s->patch[g]);
    put(&w, "# Each Oracle's European ROM (Cartridge's European ROM), whose English and official translations it plays in, and\n"
               "# its language: en (English (US), the US ROM's own), en-eu (English (EU), the European ROM's), fr, de, it or es,\n"
               "# which need that ROM.\n");
    for (unsigned g = 0; g < ORACLES_SETTINGS_GAMES; g++) put(&w, "eu_rom_%s=%s\n", game_names[g], s->eu_rom[g]);
    for (unsigned g = 0; g < ORACLES_SETTINGS_GAMES; g++)
        put(&w, "language_%s=%s\n", game_names[g],
                oracles_settings_language_names[s->language[g] > 0 && s->language[g] < ORACLES_SETTINGS_LANGUAGES ? s->language[g] : 0]);
    put(&w, "# Each fan game's language: its texts that are its Oracle's in the translation of its Oracle's European ROM, its\n"
               "# own in English; en, en-eu, fr, de, it or es.\n");
    for (unsigned g = 0; g < ORACLES_HOME_FAN_GAMES; g++)
        put(&w, "language_%s=%s\n", oracles_home_fan_games[g].key,
                oracles_settings_language_names[s->fan_language[g] > 0 && s->fan_language[g] < ORACLES_SETTINGS_LANGUAGES ? s->fan_language[g] : 0]);
    for (unsigned g = 0; g < ORACLES_SETTINGS_GAMES; g++) put(&w, "item_hotkeys_%s=%s\n", game_names[g], hotkey_names[s->item_hotkeys[g]]);
    put(&w, "# Each game's active mods (its Mods page), the names of their folders in the mods folder beside this file.\n");
    for (unsigned g = 0; g < ORACLES_SETTINGS_GAMES; g++) put(&w, "mods_%s=%s\n", game_names[g], s->mods[g]);
    put(&w, "# Each game's cheats, in both profiles: no-damage (Link loses no heart), ammo (seeds, bombs and Bombchus are not\n"
               "# used up) and rupees (buying costs nothing; a mod's prices still apply), separated by commas; empty for none.\n");
    for (unsigned g = 0; g < ORACLES_SETTINGS_GAMES; g++) {
        char names[64];
        put(&w, "cheats_%s=%s\n", game_names[g], oracles_cheats_format(s->cheats[g], NULL, names, sizeof names));
    }
    put(&w, "# Display, for the games the home screen starts: the profile (faithful or enhanced), the view in enhanced (near,\n"
               "# medium or far, how much of the world it shows, in the screen's shape), the continuous transitions (off|on, in\n"
               "# enhanced only), and the window, 2, 3 or 4 times its surface or full (the screen); enhanced, far, on and full at\n"
               "# the first opening.\n");
    put(&w, "profile=%s\n", profile_names[s->profile]);
    put(&w, "view=%s\n", view_names[s->view >= 0 && s->view < 3 ? s->view : 2]);
    put(&w, "# The core the games run on: sameboy (Accurate, the reference) or mgba (Fast, lighter, for small devices);\n"
               "# a savestate loads only on the core it was taken on.  Without a core= line, the application's default (%s).\n",
            oracles_settings_core_names[oracles_settings_core(s)]);
    if (s->core == ORACLES_CORE_SAMEBOY || s->core == ORACLES_CORE_MGBA) put(&w, "core=%s\n", oracles_settings_core_names[s->core]);
    put(&w, "# The game's menus, its map and its cutscenes in Enhanced: large (at their own largest whole scale, as Faithful\n"
               "# shows them) or view (framed at the view's scale).\n");
    put(&w, "menus=%s\n", s->menus_large ? "large" : "view");
    put(&w, "# The status bar in Enhanced: original (as the game draws it, dark beside it), full (across the whole view,\n"
               "# beside it its own background, which follows the game's colours) or wide (on that background, the hearts\n"
               "# and the money at the left corner, the items at the right).\n");
    put(&w, "hud=%s\n", oracles_settings_hud_names[s->hud >= 0 && s->hud < 3 ? s->hud : 1]);
    put(&w, "# The view's shape: auto (the screen's, nearer 4:3 or 16:9), 16:9 or 4:3, whatever the screen.\n");
    put(&w, "aspect=%s\n", oracles_settings_aspect_names[s->aspect >= 0 && s->aspect < 3 ? s->aspect : 0]);
    put(&w, "# The far view: screen (fitted to a screen its size fills badly, 426x320 on 1280x960 and 412x360 on 1240x1080,\n"
               "# three times, with aspect=auto) or fixed (480x270 or 480x360, whatever the screen).\n");
    put(&w, "far=%s\n", s->far_fixed ? "fixed" : "screen");
    put(&w, "# The rooms around Link that he has never entered, in enhanced: shown, or hidden (kept dark until he enters\n"
               "# them, as on the game's map).\n");
    put(&w, "unvisited_rooms=%s\n", s->unvisited_hidden ? "hidden" : "shown");
    put(&w, "# The ghosts that prepare the rooms around in Enhanced, each on its own processor core: auto (two on a device of\n"
               "# four processor threads or more), 1 or 2. Two fill the view faster after a warp or a load.\n");
    if (s->ghosts == 1 || s->ghosts == 2) put(&w, "ghosts=%d\n", s->ghosts); else put(&w, "ghosts=auto\n");
    if (s->quality_hint >= 0 && s->quality_hint <= ORACLES_QUALITY_CUSTOM) {
        put(&w, "# The quality a game that ran slowly was last suggested to leave: the suggestion is made once a quality, until\n"
                   "# the view, the core or the ghosts change.\n");
        put(&w, "quality_hint=%s\n", oracles_settings_quality_names[s->quality_hint]);
    }
    put(&w, "transitions=%s\n", s->transitions ? "on" : "off");
    put(&w, "# Swimming: off (a stroke for each tap, of A with the flippers, of a direction with Ages' mermaid suit, as the\n"
               "# game plays) or on (a direction held swims as the best tapping does, A held too with the flippers), in both profiles.\n");
    put(&w, "swim_hold=%s\n", s->swim_hold ? "on" : "off");
    put(&w, "# The text box: classic (the game's fixed width) or compact (its lines in the font's own widths, at the game's\n"
               "# pace), in enhanced on Ages, Seasons and the fan games; from the home screen only, the pause showing it greyed.\n");
    put(&w, "text=%s\n", s->compact_text ? "compact" : "classic");
    put(&w, "# The text box: standard (the game's) or wide (192 pixels of text, drawn by the port, the texts laid out again\n"
               "# for it, classic or compact), in enhanced on Ages and Seasons; from the home screen only, the pause showing it\n"
               "# greyed.\n");
    put(&w, "text_box=%s\n", s->text_box_wide ? "wide" : "standard");
    put(&w, "# The text box's size: normal, large (three halves, in whole screen pixels) or xlarge (twice as large), where the\n"
               "# view holds it, else the next size down, in enhanced; at once, from the pause too.\n");
    put(&w, "text_size=%s\n", text_size_names[s->text_size >= 0 && s->text_size < 3 ? s->text_size : 0]);
    put(&w, "# The text's speed: classic (letter by letter, at the speed chosen on the file screen) or instant (each line\n"
               "# whole; A still turns the page), in both profiles; from the pause too, when the game resumes.\n");
    put(&w, "text_speed=%s\n", s->text_instant ? "instant" : "classic");
    if (s->window_scale) put(&w, "window_scale=%d\n", s->window_scale); else put(&w, "window_scale=full\n");
    put(&w, "# The game in fullscreen: sharp (its largest whole scale, every pixel exact) or fill (the whole screen in its\n"
               "# proportions, sampled for pixel art); a window keeps its whole scales.\n");
    put(&w, "scaling=%s\n", s->scaling_fill ? "fill" : "sharp");
    put(&w, "# The band of the screen the game and the launcher keep to: off (the whole screen), auto (clear of a notch or a\n"
               "# curved edge Android reports above or below), small or large (24 or 48 dp clear at the top and at the bottom,\n"
               "# for a screen whose edges hide part of the picture, auto's too where it is larger).\n");
    put(&w, "screen_margin=%s\n", oracles_screen_margin_names[s->screen_margin >= 0 && s->screen_margin < ORACLES_SCREEN_MARGINS ? s->screen_margin : 0]);
    put(&w, "# The update check: on (when the home screen opens, one anonymous request to GitHub for the latest release, which\n"
               "# the home screen names when it is newer; nothing is sent but the request) or off (no request).\n");
    put(&w, "update_check=%s\n", s->update_check ? "on" : "off");
    put(&w, "launcher_window=%dx%d\n", s->launcher_width, s->launcher_height);
    for (unsigned i = 0; i < ORACLES_BINDINGS; i++) put(&w, "key_%s=%s\n", oracles_sdl_binding_names[i], s->key_names[i]);
    for (unsigned i = 0; i < 4; i++) put(&w, "pad_%s=%s\n", pad_setting_names[i], s->pad_names[i]);
    put(&w, "# The controller's Escape in a game, the pause menu, as Guide is too: a button, or lefttrigger or righttrigger;\n"
               "# one the game's buttons or the item hotkeys have stays theirs; empty: Guide alone.\n");
    put(&w, "menu_pad=%s\n", s->menu_pad);
    put(&w, "# The toast that names that button, over the first game started with a controller: due, or shown already.\n");
    put(&w, "menu_toast=%s\n", s->menu_toast_shown ? "shown" : "due");
    put(&w, "# Item hotkeys (--item-hotkeys=use|equip, for the run that asks): a key per slot; held with the bind keys it takes the item of B or of A,\n"
               "# and in the inventory it takes the item under the cursor. A slot is <b|a>:<item>:<variant or -->, in the ROM's own identifiers.\n");
    for (unsigned i = 0; i < ORACLES_HOTKEY_SLOTS; i++) put(&w, "key_hotkey%u=%s\n", i + 1u, s->hotkey_key_names[i]);
    put(&w, "key_hotkey_bind_b=%s\nkey_hotkey_bind_a=%s\n", s->hotkey_bind_b, s->hotkey_bind_a);
    for (unsigned i = 0; i < ORACLES_HOTKEY_SLOTS; i++) put(&w, "pad_hotkey%u=%s\n", i + 1u, s->hotkey_pad_names[i]);
    for (unsigned game = 0; game < 2u; game++)
        for (unsigned i = 0; i < ORACLES_HOTKEY_SLOTS; i++) {
            char line[64];
            /* The item's name above its line, from the generated tables; an identifier they do not know (a modified ROM) stays bare. */
            const OraclesHotkeySlot *slot = &s->hotkeys[game][i];
            const OraclesGuestTables *tables = game == 0 ? &oracles_guest_tables_ages : &oracles_guest_tables_seasons;
            if (slot->set && slot->item < ORACLES_GUEST_ITEM_LABELS && tables->item_names[slot->item] && tables->item_names[slot->item][0])
                put(&w, "# %s\n", tables->item_names[slot->item]);
            if (oracles_hotkey_line_format(line, sizeof line, game, i, slot)) put(&w, "%s\n", line);
        }
    place(&w, NULL, 0);
    const int written = !ferror(f) && fflush(f) == 0;
    if (fclose(f) != 0 || !written) { remove(temporary); return 0; }
    if (rename(temporary, s->path) == 0) return 1;
    remove(s->path);   /* where rename does not replace a file */
    if (rename(temporary, s->path) == 0) return 1;
    remove(temporary);
    return 0;
}
