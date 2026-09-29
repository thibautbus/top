/* The home screen's keys of settings.txt: each game's ROM, profile and
 * gameplay options and the launcher window's size are written and read back,
 * a file without them keeps the defaults, a value the launcher does not know
 * leaves its key alone, and names that were never the launcher's (zoom-out as
 * a profile, per-game profile and transitions keys) are neither read nor
 * written. */
#include "session.h"
#include "settings.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define rmdir _rmdir
#else
#include <unistd.h>
#endif

static int failures;

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); failures++; } } while (0)

static oracles_settings written, read_back;

static void write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs(text, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: oracles-test-launcher-settings TEMPORARY_FILE\n"); return 2; }
    const char *path = argv[1];

    oracles_settings_defaults(&written);
    CHECK(written.rom[ORACLES_SETTINGS_AGES][0] == 0 && written.rom[ORACLES_SETTINGS_SEASONS][0] == 0);
    CHECK(written.profile == ORACLES_PROFILE_FAITHFUL);
    CHECK(written.launcher_width == 1280 && written.launcher_height == 720);
    /* The transitions come with Enhanced: on by default. */
    CHECK(written.transitions && written.item_hotkeys[0] == ORACLES_HOTKEYS_OFF && written.item_hotkeys[1] == ORACLES_HOTKEYS_OFF);
    CHECK(written.window_scale == 4);

    /* Paths as players have them: spaces, an equals sign, backslashes, and as long as the field holds. */
    snprintf(written.path, sizeof written.path, "%s", path);
    snprintf(written.rom[ORACLES_SETTINGS_AGES], sizeof written.rom[0], "C:\\Users\\sam\\Games\\Oracle of Ages (USA).gbc");
    memset(written.rom[ORACLES_SETTINGS_SEASONS], 'x', sizeof written.rom[1] - 1);
    memcpy(written.rom[ORACLES_SETTINGS_SEASONS], "/home/sam/roms/a=b ", 19);
    written.rom[ORACLES_SETTINGS_SEASONS][sizeof written.rom[1] - 1] = 0;
    snprintf(written.patch[ORACLES_HOME_FAN_MOONRISE], sizeof written.patch[0], "C:\\Users\\sam\\Games\\Moonrise Regalia 1.0.6.bps");
    snprintf(written.patch[ORACLES_HOME_FAN_TEMPLE], sizeof written.patch[0], "/home/sam/roms/Temple_of_Seasons_1.073.bps");
    snprintf(written.patch[ORACLES_HOME_FAN_KINOMI], sizeof written.patch[0], "/home/sam/roms/Gifts_of_Kinomi_1.1.2.bps");
    written.profile = ORACLES_PROFILE_ENHANCED;
    written.launcher_width = 1600;
    written.launcher_height = 900;
    written.camera = 1;
    /* Controls' keys and buttons as it writes them: renamed, and one left without a key after another cell took it. */
    snprintf(written.key_names[0], sizeof written.key_names[0], "D");
    written.key_names[5][0] = 0;
    snprintf(written.pad_names[0], sizeof written.pad_names[0], "x");
    written.hotkey_pad_names[0][0] = 0;
    snprintf(written.hotkey_bind_b, sizeof written.hotkey_bind_b, "Right Shift");
    written.transitions = 0;
    written.item_hotkeys[ORACLES_SETTINGS_AGES] = ORACLES_HOTKEYS_EQUIP;
    written.item_hotkeys[ORACLES_SETTINGS_SEASONS] = ORACLES_HOTKEYS_USE;
    written.window_scale = 0;   /* fullscreen */
    /* The Mods page's choices: kept in the order of their names, whatever the order they were made in. */
    CHECK(written.mods[ORACLES_SETTINGS_AGES][0] == 0 && written.mods[ORACLES_SETTINGS_SEASONS][0] == 0);
    CHECK(oracles_settings_mod_set(written.mods[ORACLES_SETTINGS_AGES], "fortune-teller", 1));
    CHECK(oracles_settings_mod_set(written.mods[ORACLES_SETTINGS_AGES], "claw-game", 1));
    CHECK(oracles_settings_mod_set(written.mods[ORACLES_SETTINGS_AGES], "claw-game", 1));   /* once */
    CHECK(!strcmp(written.mods[ORACLES_SETTINGS_AGES], "claw-game,fortune-teller"));
    CHECK(oracles_settings_mod_active(written.mods[ORACLES_SETTINGS_AGES], "claw-game") && !oracles_settings_mod_active(written.mods[ORACLES_SETTINGS_AGES], "claw"));
    CHECK(oracles_settings_mod_set(written.mods[ORACLES_SETTINGS_SEASONS], "claw-game", 1));
    oracles_settings_store(&written);

    oracles_settings_defaults(&read_back);
    snprintf(read_back.path, sizeof read_back.path, "%s", path);
    oracles_settings_load(&read_back);
    CHECK(!strcmp(read_back.rom[ORACLES_SETTINGS_AGES], written.rom[ORACLES_SETTINGS_AGES]));
    CHECK(!strcmp(read_back.rom[ORACLES_SETTINGS_SEASONS], written.rom[ORACLES_SETTINGS_SEASONS]));
    CHECK(!strcmp(read_back.patch[ORACLES_HOME_FAN_MOONRISE], written.patch[ORACLES_HOME_FAN_MOONRISE]));
    CHECK(!strcmp(read_back.patch[ORACLES_HOME_FAN_TEMPLE], written.patch[ORACLES_HOME_FAN_TEMPLE]));
    CHECK(!strcmp(read_back.patch[ORACLES_HOME_FAN_KINOMI], written.patch[ORACLES_HOME_FAN_KINOMI]));
    CHECK(read_back.profile == ORACLES_PROFILE_ENHANCED);
    CHECK(read_back.launcher_width == 1600 && read_back.launcher_height == 900);
    CHECK(read_back.camera == 1);   /* the keys around them still read */
    CHECK(!strcmp(read_back.key_names[0], "D") && read_back.key_names[5][0] == 0 && !strcmp(read_back.key_names[1], "Left"));
    CHECK(!strcmp(read_back.pad_names[0], "x") && read_back.hotkey_pad_names[0][0] == 0 && !strcmp(read_back.hotkey_pad_names[1], "y"));
    CHECK(!strcmp(read_back.hotkey_bind_b, "Right Shift") && !strcmp(read_back.hotkey_bind_a, "Left Ctrl"));
    CHECK(read_back.transitions == 0);
    CHECK(read_back.item_hotkeys[ORACLES_SETTINGS_AGES] == ORACLES_HOTKEYS_EQUIP && read_back.item_hotkeys[ORACLES_SETTINGS_SEASONS] == ORACLES_HOTKEYS_USE);
    CHECK(read_back.window_scale == 0);
    CHECK(!strcmp(read_back.mods[ORACLES_SETTINGS_AGES], "claw-game,fortune-teller") && !strcmp(read_back.mods[ORACLES_SETTINGS_SEASONS], "claw-game"));
    char names[ORACLES_SETTINGS_MODS_MAX][ORACLES_SETTINGS_MOD_NAME];
    CHECK(oracles_settings_mod_names(read_back.mods[ORACLES_SETTINGS_AGES], names, ORACLES_SETTINGS_MODS_MAX) == 2
          && !strcmp(names[0], "claw-game") && !strcmp(names[1], "fortune-teller"));
    CHECK(oracles_settings_mod_set(read_back.mods[ORACLES_SETTINGS_AGES], "claw-game", 0) && !strcmp(read_back.mods[ORACLES_SETTINGS_AGES], "fortune-teller"));
    /* Eight at most: a ninth is refused and the list stays. */
    for (int i = 0; i < 7; i++) {
        char name[8];
        snprintf(name, sizeof name, "m%d", i);
        CHECK(oracles_settings_mod_set(read_back.mods[ORACLES_SETTINGS_AGES], name, 1));
    }
    CHECK(!oracles_settings_mod_set(read_back.mods[ORACLES_SETTINGS_AGES], "zz", 1));
    CHECK(!strcmp(read_back.mods[ORACLES_SETTINGS_AGES], "fortune-teller,m0,m1,m2,m3,m4,m5,m6"));
    CHECK(oracles_settings_mod_set(read_back.mods[ORACLES_SETTINGS_AGES], "m3", 0) && oracles_settings_mod_set(read_back.mods[ORACLES_SETTINGS_AGES], "zz", 1));
    CHECK(!strcmp(read_back.mods[ORACLES_SETTINGS_AGES], "fortune-teller,m0,m1,m2,m4,m5,m6,zz"));

    /* The names of the file. */
    FILE *f = fopen(path, "r");
    char line[8192];
    int seen = 0;
    while (f && fgets(line, sizeof line, f)) {
        if (!strcmp(line, "profile=enhanced\n")) seen |= 1;
        if (!strncmp(line, "profile_", 8) || !strncmp(line, "transitions_", 12)) seen |= 128;   /* no per-game keys */
        if (!strcmp(line, "camera=1\n")) seen |= 2;
        if (!strcmp(line, "launcher_window=1600x900\n")) seen |= 4;
        if (!strncmp(line, "rom_ages=C:\\Users\\sam\\Games\\", 28)) seen |= 8;
        if (!strcmp(line, "transitions=off\n")) seen |= 16;
        if (!strcmp(line, "item_hotkeys_ages=equip\n")) seen |= 32;
        if (!strcmp(line, "window_scale=full\n")) seen |= 64;
        if (!strcmp(line, "patch_moonrise=C:\\Users\\sam\\Games\\Moonrise Regalia 1.0.6.bps\n")) seen |= 256;
        if (!strcmp(line, "patch_temple=/home/sam/roms/Temple_of_Seasons_1.073.bps\n")) seen |= 512;
        if (!strcmp(line, "patch_kinomi=/home/sam/roms/Gifts_of_Kinomi_1.1.2.bps\n")) seen |= 1024;
    }
    if (f) fclose(f);
    CHECK(seen == (127 | 256 | 512 | 1024));   /* and not 128: no per-game profile or transitions */

    /* A file of an earlier version, without the keys: the defaults. */
    write_text(path, "colour_correction=1\nvsync=off\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.colour_correction == 1 && read_back.rom[0][0] == 0 && read_back.patch[ORACLES_HOME_FAN_MOONRISE][0] == 0 && read_back.patch[ORACLES_HOME_FAN_TEMPLE][0] == 0
          && read_back.patch[ORACLES_HOME_FAN_KINOMI][0] == 0 && read_back.profile == ORACLES_PROFILE_FAITHFUL);
    CHECK(read_back.transitions == 1);
    CHECK(read_back.launcher_width == 1280 && read_back.launcher_height == 720);

    /* The mods folder beside settings.txt, whichever separator its path uses; none without a settings directory. */
    oracles_settings mods_at;
    char folder[ORACLES_SETTINGS_PATH_LENGTH];
    oracles_settings_defaults(&mods_at);
    snprintf(mods_at.path, sizeof mods_at.path, "/home/sam/.local/share/the-oracles-project/settings.txt");
    oracles_settings_mods_folder(&mods_at, folder, sizeof folder);
    CHECK(!strcmp(folder, "/home/sam/.local/share/the-oracles-project/mods"));
    snprintf(mods_at.path, sizeof mods_at.path, "C:\\Users\\sam\\AppData\\Roaming\\the-oracles-project\\settings.txt");
    oracles_settings_mods_folder(&mods_at, folder, sizeof folder);
    CHECK(!strcmp(folder, "C:\\Users\\sam\\AppData\\Roaming\\the-oracles-project\\mods"));
    mods_at.path[0] = 0;
    oracles_settings_mods_folder(&mods_at, folder, sizeof folder);
    CHECK(folder[0] == 0);

    /* Mods' Play gives a session the mods --mods would, in the same order, and the same save of its own; a mod no
     * longer in the folder is left out.  Start game, without them, keeps the usual save. */
    {
        char dir[ORACLES_SETTINGS_PATH_LENGTH + 32], dirs[ORACLES_SETTINGS_MODS_MAX][ORACLES_SETTINGS_PATH_LENGTH], home_folder[ORACLES_SETTINGS_PATH_LENGTH];
        oracles_settings home;
        oracles_settings_defaults(&home);
        snprintf(home.path, sizeof home.path, "%s", path);   /* the mods folder beside the test's settings file */
        oracles_settings_mods_folder(&home, home_folder, sizeof home_folder);
        mkdir(home_folder, 0700);
        const char *const names[3] = { "claw-game", "fortune-teller", "gone" };
        for (int i = 0; i < 2; i++) { snprintf(dir, sizeof dir, "%s/%s", home_folder, names[i]); mkdir(dir, 0700); }
        for (int i = 2; i >= 0; i--) CHECK(oracles_settings_mod_set(home.mods[ORACLES_SETTINGS_AGES], names[i], 1));
        OraclesSessionOptions from_page, from_command_line;
        oracles_session_defaults(&from_page);
        oracles_session_defaults(&from_command_line);
        from_page.rom_path = from_command_line.rom_path = "/home/sam/roms/Oracle of Ages (USA).gbc";
        char claw[ORACLES_SETTINGS_PATH_LENGTH + 32], fortune[ORACLES_SETTINGS_PATH_LENGTH + 32];
        snprintf(claw, sizeof claw, "%s/claw-game", home_folder);
        snprintf(fortune, sizeof fortune, "%s/fortune-teller", home_folder);
        from_command_line.mods_dirs[from_command_line.mods_count++] = claw;       /* --mods .../claw-game */
        from_command_line.mods_dirs[from_command_line.mods_count++] = fortune;    /* --mods .../fortune-teller */
        CHECK(oracles_session_home_mods(&from_page, &home, ORACLES_SETTINGS_AGES, dirs) == 2 && from_page.mods_count == from_command_line.mods_count);
        for (unsigned i = 0; i < from_page.mods_count && i < from_command_line.mods_count; i++) CHECK(!strcmp(from_page.mods_dirs[i], from_command_line.mods_dirs[i]));
        char save_page[ORACLES_SETTINGS_PATH_LENGTH], save_command_line[ORACLES_SETTINGS_PATH_LENGTH];
        oracles_session_save_path(&from_page, save_page, sizeof save_page);
        oracles_session_save_path(&from_command_line, save_command_line, sizeof save_command_line);
        CHECK(!strcmp(save_page, save_command_line) && !strcmp(save_page, "/home/sam/roms/Oracle of Ages (USA).mods.sav"));
        /* Seasons has none active; Start game plays without mods, on the usual save; --save is kept as given. */
        CHECK(oracles_session_home_mods(&from_page, &home, ORACLES_SETTINGS_SEASONS, dirs) == 0);
        oracles_session_save_path(&from_page, save_page, sizeof save_page);
        CHECK(!strcmp(save_page, "/home/sam/roms/Oracle of Ages (USA).sav"));
        from_command_line.save_path = "/tmp/other.sav";
        oracles_session_save_path(&from_command_line, save_command_line, sizeof save_command_line);
        CHECK(!strcmp(save_command_line, "/tmp/other.sav"));
        /* A fan game's save is beside its patch. */
        from_page.patch_path = "/home/sam/roms/Moonrise Regalia 1.0.6.bps";
        oracles_session_save_path(&from_page, save_page, sizeof save_page);
        CHECK(!strcmp(save_page, "/home/sam/roms/Moonrise Regalia 1.0.6.sav"));
        for (int i = 0; i < 2; i++) { snprintf(dir, sizeof dir, "%s/%s", home_folder, names[i]); rmdir(dir); }
        rmdir(home_folder);
    }

    /* A mods line written by hand: its names sorted, once each, those that name no folder a mod can have dropped. */
    write_text(path, "mods_ages=zeta,Bad Name,alpha,,zeta,../up,beta\nmods_seasons=\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(!strcmp(read_back.mods[ORACLES_SETTINGS_AGES], "alpha,beta,zeta") && read_back.mods[ORACLES_SETTINGS_SEASONS][0] == 0);

    /* Values it does not know leave the default; an empty ROM clears it. */
    write_text(path, "profile=widescreen\nlauncher_window=big\nrom_seasons=\ntransitions=yes\nitem_hotkeys_seasons=always\nwindow_scale=5\n");
    oracles_settings_defaults(&read_back);
    snprintf(read_back.rom[1], sizeof read_back.rom[1], "old.gbc");
    oracles_settings_load(&read_back);
    CHECK(read_back.profile == ORACLES_PROFILE_FAITHFUL);
    CHECK(read_back.launcher_width == 1280 && read_back.launcher_height == 720);
    CHECK(read_back.rom[ORACLES_SETTINGS_SEASONS][0] == 0);
    CHECK(read_back.transitions == 1 && read_back.item_hotkeys[ORACLES_SETTINGS_SEASONS] == ORACLES_HOTKEYS_OFF);
    CHECK(read_back.window_scale == 4);
    /* zoom-out is not a profile, and the per-game keys are not the launcher's: the defaults stay, and the next store
     * writes the global keys alone. */
    write_text(path, "profile=zoom-out\nprofile_ages=enhanced\ntransitions_ages=off\nprofile_seasons=enhanced\n");
    oracles_settings_defaults(&read_back);
    oracles_settings_load(&read_back);
    CHECK(read_back.profile == ORACLES_PROFILE_FAITHFUL && read_back.transitions == 1);
    oracles_settings_store(&read_back);
    f = fopen(path, "r");
    seen = 0;
    while (f && fgets(line, sizeof line, f)) {
        if (!strcmp(line, "profile=faithful\n")) seen |= 1;
        if (!strcmp(line, "transitions=on\n")) seen |= 2;
        if (strstr(line, "zoom-out") || !strncmp(line, "profile_", 8) || !strncmp(line, "transitions_", 12)) seen |= 4;
    }
    if (f) fclose(f);
    CHECK(seen == 3);

    /* The file is written through a temporary one: none is left, and a write that cannot start leaves the old file. */
    char temporary[1100];
    snprintf(temporary, sizeof temporary, "%s.tmp", path);
    f = fopen(temporary, "r");
    CHECK(f == NULL);
    if (f) fclose(f);
    write_text(path, "profile=enhanced\n");
    CHECK(mkdir(temporary, 0700) == 0);   /* a directory where the temporary file would go */
    oracles_settings_defaults(&written);
    snprintf(written.path, sizeof written.path, "%s", path);
    CHECK(oracles_settings_store(&written) == 0);
    oracles_settings_defaults(&read_back);
    snprintf(read_back.path, sizeof read_back.path, "%s", path);
    oracles_settings_load(&read_back);
    CHECK(read_back.profile == ORACLES_PROFILE_ENHANCED);
    rmdir(temporary);
    CHECK(oracles_settings_store(&written) == 1);

    remove(path);
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("launcher settings: ROMs, the profile, the active mods and the window's size round-trip through settings.txt\n");
    return 0;
}
