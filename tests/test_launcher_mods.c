/* Mods (ui_page_nav.h, ui_page_layout.h): its layout against the layout
 * reference, tests/launcher_layout_reference.json (layout_reference.h), the
 * test's argument, in its frames 8a (no mod found), 8b (two mods, one
 * active), 8c (a third refused) and 8d (Seasons without its ROM, Play
 * greyed), at 1920x1080 with the vendored fonts, and in the 4:3 layout, 10l
 * (a mod refused) and 10m (none), at 1440x1080; a text is compared by its
 * left, its width, its line box and its baseline, an element by its box.
 * Past six mods (three in 4:3), the list shows those around the highlighted
 * row.  On Android the folder shows from its Android/data, reachable over
 * USB, and the Save row, between the list and Play, exports the game with
 * mods' save: greyed without one and from a game, its note saying why, its
 * status after an export, gone with the page. */
#include "layout_reference.h"
#include "ui_page_layout.h"
#include "ui_page_nav.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); layout_fail(); } } while (0)

static void add_mod(OraclesHomeNav *nav, int game, const char *name, const char *line, int ages, int seasons, int refused, int active)
{
    OraclesHomeMod *m = &nav->mods[game].mods[nav->mods[game].count++];
    memset(m, 0, sizeof *m);
    snprintf(m->name, sizeof m->name, "%s", name);
    snprintf(m->line, sizeof m->line, "%s", line);
    m->houses[ORACLES_HOME_GAME_AGES] = ages;
    m->houses[ORACLES_HOME_GAME_SEASONS] = seasons;
    m->refused = refused;
    m->active = active;
}

/* The page as the reference has it: Ages played on 21 Sep 2026 (or Seasons without a ROM), the mods folder of a
 * Windows player, and the example mods; `set` 0 none, 1 two with the claw game active, 2 both active and a third
 * refused. */
static void page(OraclesHomeNav *nav, int game, int set, unsigned row)
{
    oracles_home_init(nav);
    OraclesHomeGame *g = &nav->games[ORACLES_HOME_GAME_AGES];
    g->usable = 1;
    g->rom = ORACLES_ROM_ORIGINAL;
    snprintf(g->last_session, sizeof g->last_session, "21 Sep 2026");
    snprintf(nav->mods_folder, sizeof nav->mods_folder, "C:\\Users\\sam\\AppData\\Roaming\\the-oracles-project\\mods");
    if (set >= 1) {
        add_mod(nav, game, "claw-game", "A house in Lynna City and in Horon, and a claw game", 1, 1, 0, 1);
        add_mod(nav, game, "fortune-teller", "A fortune teller who remembers each of Link\xe2\x80\x99s visits", 1, 0, 0, set == 2);
    }
    if (set == 2) add_mod(nav, game, "night-market", "Refused: mod night-market has no main.lua", 0, 0, 1, 0);
    nav->entry = game == ORACLES_HOME_GAME_SEASONS ? ORACLES_HOME_SEASONS : ORACLES_HOME_AGES;
    nav->focus = 4;
    CHECK(oracles_home_act(nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && nav->screen == ORACLES_SCREEN_MODS);
    nav->row = row;
}

/* The head, the folder, the count, the note and Play: the probes every frame of the page has. */
static void common(const char *frame, const OraclesUiModsLayout *l)
{
    layout_text(frame, "section", &l->head.section);
    layout_text(frame, "over", &l->head.over);
    layout_box(frame, "panel", &l->panel);
    layout_line(frame, "Folder", &l->label_folder);
    layout_box(frame, "folder row", &l->folder_row);
    layout_line(frame, "folder path", &l->folder_path);
    layout_line(frame, "folder line", &l->folder_line);
    layout_line(frame, "Open folder", &l->folder_button);
    layout_line(frame, "Open folder dot", &l->folder_button_dot);
    layout_line(frame, "Mods", &l->label_mods);
    layout_box(frame, "list", &l->list);
    layout_line(frame, "note", &l->note.lines[0]);
    layout_near(frame, "note", "width", l->note.w);
    layout_near(frame, "note", "lines", (float)l->note.count);
    layout_box(frame, "Play row", &l->play_row);
    layout_line(frame, "Play", &l->play);
    layout_line(frame, "Play dot", &l->play_dot);
}

/* A mod's row: its box, its switch, its name and games, its line and dot. */
static void mod_row(const char *frame, const char *key, const OraclesUiModLayout *m, int games)
{
    char probe[64];
    snprintf(probe, sizeof probe, "%s row", key); layout_box(frame, probe, &m->row);
    snprintf(probe, sizeof probe, "%s switch", key); layout_box(frame, probe, &m->toggle);
    snprintf(probe, sizeof probe, "%s name", key); layout_line(frame, probe, &m->name);
    if (games) { snprintf(probe, sizeof probe, "%s games", key); layout_line(frame, probe, &m->games); }
    snprintf(probe, sizeof probe, "%s dot", key); layout_box(frame, probe, &m->dot);
    snprintf(probe, sizeof probe, "%s description", key); layout_line(frame, probe, &m->line);
}

static void frame_8a(void)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_HOME_GAME_AGES, 0, ORACLES_MODS_ROW_FOLDER);
    OraclesUiModsLayout l;
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    common("8a", &l);
    layout_text("8a", "title", &l.head.title.lines[0]);
    layout_line("8a", "state", &l.head.state);
    CHECK(l.shown == 0 && l.empty.count == 2);
    layout_line("8a", "empty", &l.empty.lines[0]);
    layout_near("8a", "empty", "width", l.empty.w);
    layout_line("8a", "count", &l.count);
    CHECK(!strcmp(l.count_text, "0 of 8 active") && !l.play_note_text[0]);
}

static void frame_8b(void)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_HOME_GAME_AGES, 1, 1);
    OraclesUiModsLayout l;
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    common("8b", &l);
    layout_text("8b", "title", &l.head.title.lines[0]);
    CHECK(l.shown == 2 && !strcmp(l.mods[0].games_text, "Houses in Ages and Seasons") && !strcmp(l.mods[1].games_text, "Houses in Ages"));
    mod_row("8b", "claw", &l.mods[0], 1);
    mod_row("8b", "fortune", &l.mods[1], 1);
    layout_line("8b", "count", &l.count);
    /* The knob at the right of the switch when on, at its left when off. */
    CHECK(l.mods[0].knob.x == l.mods[0].toggle.x + 34.0f && l.mods[1].knob.x == l.mods[1].toggle.x + 4.0f);
    CHECK(l.mods[0].knob.y == l.mods[0].toggle.y + 4.0f && l.mods[0].knob.w == 26.0f);
}

static void frame_8c(void)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_HOME_GAME_AGES, 2, 3);
    OraclesUiModsLayout l;
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    common("8c", &l);
    CHECK(l.shown == 3 && !l.mods[2].games_text[0]);
    mod_row("8c", "claw", &l.mods[0], 1);
    mod_row("8c", "fortune", &l.mods[1], 1);
    mod_row("8c", "refused", &l.mods[2], 0);
    layout_line("8c", "count", &l.count);
    CHECK(!strcmp(l.count_text, "2 of 8 active \xc2\xb7 they play in the order of their names"));
}

static void frame_8d(void)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_HOME_GAME_SEASONS, 1, 3);
    OraclesUiModsLayout l;
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    common("8d", &l);
    layout_text("8d", "title", &l.head.title.lines[0]);
    layout_line("8d", "state", &l.head.state);
    mod_row("8d", "claw", &l.mods[0], 1);
    layout_line("8d", "play state", &l.play_note);
}

/* Past six mods, the list shows six: from the first while the highlight is among them, then the six ending at the
 * highlighted one; Play shows the last six. */
static void many(void)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_HOME_GAME_AGES, 0, 0);
    for (int i = 0; i < 9; i++) {
        char name[16];
        snprintf(name, sizeof name, "mod-%d", i);
        add_mod(&nav, ORACLES_HOME_GAME_AGES, name, "", 1, 0, 0, 0);
    }
    OraclesUiModsLayout l;
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(l.shown == ORACLES_UI_MODS_SHOWN && l.mods[0].index == 0 && l.mods[5].index == 5);
    nav.row = 8;   /* mod-7 */
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(l.shown == ORACLES_UI_MODS_SHOWN && l.mods[0].index == 2 && l.mods[5].index == 7);
    nav.row = oracles_mods_row_play(&nav);
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(l.mods[0].index == 3 && l.mods[5].index == 8);
    CHECK(l.panel.y + l.panel.h < 1000.0f);   /* above the help bar */
    /* A long name and a long line are cut to their room. */
    memset(nav.mods[0].mods[8].name, 'w', sizeof nav.mods[0].mods[8].name - 1);
    char line[ORACLES_HOME_TEXT_LENGTH];
    memset(line, 'w', sizeof line - 1);
    line[sizeof line - 1] = 0;
    snprintf(nav.mods[0].mods[8].line, sizeof nav.mods[0].mods[8].line, "%s", line);
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    const float right = l.folder_row.x + l.folder_row.w - 20.0f;
    CHECK(l.mods[5].name.x + l.mods[5].name.w <= right + 0.5f && l.mods[5].line.x + l.mods[5].line.w <= right + 0.5f);
}

/* 4:3: the head, the folder (its path on two lines, broken after a hyphen), the list, the count, the note and Play. */
static void common_4_3(const char *frame, const OraclesUiModsLayout *l)
{
    layout_line(frame, "section", &l->head.section);
    layout_line(frame, "title", &l->head.title.lines[0]);
    layout_line(frame, "state", &l->head.state);
    CHECK(l->panel.w == 0.0f && l->head.over.w == 0.0f);
    layout_line(frame, "Folder", &l->label_folder);
    layout_box(frame, "folder row", &l->folder_row);
    CHECK(l->folder_lines.count == 2 && !strcmp(l->folder_lines.text[0], "C:\\Users\\sam\\AppData\\Roaming\\the-oracles-"));
    layout_line(frame, "folder path", &l->folder_lines.lines[0]);
    layout_near(frame, "folder path", "lines", (float)l->folder_lines.count);
    layout_line(frame, "folder line", &l->folder_line);
    layout_line(frame, "Open folder", &l->folder_button);
    layout_line(frame, "Open folder dot", &l->folder_button_dot);
    layout_line(frame, "Mods", &l->label_mods);
    layout_box(frame, "list", &l->list);
    layout_line(frame, "note", &l->note.lines[0]);
    layout_near(frame, "note", "width", l->note.w);
    layout_near(frame, "note", "lines", (float)l->note.count);
    layout_box(frame, "Play row", &l->play_row);
    layout_line(frame, "Play", &l->play);
    layout_line(frame, "Play dot", &l->play_dot);
}

static void frame_10l_10m(void)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_HOME_GAME_AGES, 2, ORACLES_MODS_ROW_FOLDER);
    OraclesUiModsLayout l;
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    common_4_3("10l", &l);
    CHECK(l.shown == 3);
    mod_row("10l", "claw", &l.mods[0], 1);
    mod_row("10l", "fortune", &l.mods[1], 1);
    mod_row("10l", "refused", &l.mods[2], 0);
    layout_line("10l", "count", &l.count);
    /* The knob, 42 across, at the right of the switch when on. */
    CHECK(l.mods[0].knob.x == l.mods[0].toggle.x + 46.0f && l.mods[2].knob.x == l.mods[2].toggle.x + 4.0f && l.mods[0].knob.w == 42.0f);
    for (unsigned i = 0; i < l.shown; i++) CHECK(l.mods[i].row.h >= 90.0f);
    CHECK(l.folder_row.h >= 90.0f && l.play_row.h >= 90.0f);

    page(&nav, ORACLES_HOME_GAME_AGES, 0, ORACLES_MODS_ROW_FOLDER);
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    common_4_3("10m", &l);
    CHECK(l.shown == 0 && l.empty.count == 2);
    layout_line("10m", "empty", &l.empty.lines[0]);
    layout_near("10m", "empty", "width", l.empty.w);
    layout_line("10m", "count", &l.count);

    /* Past three mods, 4:3 shows three, around the highlighted one, Play above the help bar; a path too long for two
     * lines keeps its end. */
    page(&nav, ORACLES_HOME_GAME_AGES, 0, 0);
    for (int i = 0; i < 5; i++) {
        char name[16];
        snprintf(name, sizeof name, "mod-%d", i);
        add_mod(&nav, ORACLES_HOME_GAME_AGES, name, "", 1, 0, 0, 0);
    }
    nav.row = 5;   /* mod-4 */
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    CHECK(l.shown == ORACLES_UI_MODS_SHOWN_4_3 && l.mods[0].index == 2 && l.mods[2].index == 4);
    CHECK(l.play_row.y + l.play_row.h <= 982.0f);
    char path[600] = "C:\\Users\\sam";
    for (int i = 0; i < 20; i++) strcat(path, "\\a-long-folder");
    strcat(path, "\\the end");
    snprintf(nav.mods_folder, sizeof nav.mods_folder, "%s", path);
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    CHECK(l.folder_lines.count == 2 && strstr(l.folder_lines.text[1], "the end") != NULL);
    for (unsigned i = 0; i < 2; i++) CHECK(l.folder_lines.lines[i].x + l.folder_lines.lines[i].w <= l.folder_button.x - 24.0f + LAYOUT_TOLERANCE);
}

/* On Android, the mods folder on the shared storage, from its Android/data as a computer sees it over USB, and the line
 * under it saying so: in 16:9 on one line, in the row, its line, Open folder and the list where the reference's desktop
 * path has them; in 4:3 the whole path in its lines, the line under them. */
static void android_folder(void)
{
    static const char shared[] = "Android/data/org.theoraclesproject/files/mods";
    OraclesHomeNav nav;
    OraclesUiModsLayout desktop, android;
    char shown[ORACLES_HOME_TEXT_LENGTH * 4];
    page(&nav, ORACLES_HOME_GAME_AGES, 0, ORACLES_MODS_ROW_FOLDER);
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_16_9, &nav, &desktop);
    oracles_mods_folder(&nav, shown, sizeof shown);
    CHECK(!strcmp(shown, nav.mods_folder));
    CHECK(!strcmp(oracles_mods_folder_line(&nav), "Beside the launcher\xe2\x80\x99s settings \xc2\xb7 one folder per mod"));
    snprintf(nav.mods_folder, sizeof nav.mods_folder, "/storage/emulated/0/%s", shared);
    nav.android = 1;
    oracles_mods_folder(&nav, shown, sizeof shown);
    CHECK(!strcmp(shown, shared));
    CHECK(!strcmp(oracles_mods_folder_line(&nav), "Reachable over USB \xc2\xb7 one folder per mod"));
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_16_9, &nav, &android);
    CHECK(!strcmp(android.folder_text, shared));
    CHECK(android.folder_path.x == desktop.folder_path.x && android.folder_path.y == desktop.folder_path.y
          && android.folder_path.h == desktop.folder_path.h && android.folder_path.baseline == desktop.folder_path.baseline);
    CHECK(android.folder_line.x == desktop.folder_line.x && android.folder_line.y == desktop.folder_line.y);
    CHECK(!memcmp(&android.folder_row, &desktop.folder_row, sizeof desktop.folder_row));
    CHECK(!memcmp(&android.folder_button, &desktop.folder_button, sizeof desktop.folder_button));
    CHECK(!memcmp(&android.list, &desktop.list, sizeof desktop.list));

    nav.android = 0;
    page(&nav, ORACLES_HOME_GAME_AGES, 0, ORACLES_MODS_ROW_FOLDER);
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_4_3, &nav, &desktop);
    snprintf(nav.mods_folder, sizeof nav.mods_folder, "/storage/emulated/0/%s", shared);
    nav.android = 1;
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_4_3, &nav, &android);
    char joined[ORACLES_HOME_TEXT_LENGTH * 4] = "";
    for (unsigned i = 0; i < android.folder_lines.count; i++) strcat(joined, android.folder_lines.text[i]);
    CHECK(android.folder_lines.count >= 1 && android.folder_lines.count <= 2 && !strcmp(joined, shared));
    CHECK(android.folder_path.x == desktop.folder_path.x && android.folder_path.baseline == desktop.folder_path.baseline);
    CHECK(!memcmp(&android.folder_button, &desktop.folder_button, sizeof desktop.folder_button));
    const OraclesUiLine *last = &android.folder_lines.lines[android.folder_lines.count - 1];
    CHECK(android.folder_line.x == desktop.folder_line.x && android.folder_line.y >= last->y + last->h);
}

/* Android's Save row: its place among the rows, what OK and a click ask, its note and its status, by layout. */
static void android_save(void)
{
    OraclesHomeNav nav;
    page(&nav, ORACLES_HOME_GAME_AGES, 1, ORACLES_MODS_ROW_FOLDER);
    CHECK(oracles_mods_row_save(&nav) == ORACLES_MODS_ROW_NONE && oracles_mods_rows(&nav) == 4 && oracles_mods_row_play(&nav) == 3);
    OraclesUiModsLayout desktop;
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_16_9, &nav, &desktop);
    CHECK(desktop.save_row.h == 0.0f && desktop.label_save.w == 0.0f);

    /* Folder, the two mods, Save, Play, wrapping; left and right do nothing on Save. */
    nav.android = 1;
    CHECK(oracles_mods_rows(&nav) == 5 && oracles_mods_row_save(&nav) == 3 && oracles_mods_row_play(&nav) == 4);
    nav.row = 2;
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == 3);
    CHECK(oracles_home_act(&nav, ORACLES_HOME_LEFT) == ORACLES_HOME_STAY && oracles_home_act(&nav, ORACLES_HOME_RIGHT) == ORACLES_HOME_STAY);
    oracles_home_act(&nav, ORACLES_HOME_DOWN);
    CHECK(nav.row == 4);
    nav.row = ORACLES_MODS_ROW_FOLDER;
    oracles_home_act(&nav, ORACLES_HOME_UP);
    CHECK(nav.row == 4);

    /* No save with mods yet: greyed, saying why, on one line in both layouts, whole in 4:3. */
    nav.row = 3;
    CHECK(!oracles_mods_save_live(&nav) && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY && oracles_mods_click(&nav, 3) == ORACLES_HOME_STAY);
    CHECK(!strcmp(oracles_mods_save_note(&nav, 0), "No save with mods yet: Play with a mod on makes one.") && !strcmp(oracles_mods_save_note(&nav, 1), oracles_mods_save_note(&nav, 0)));
    OraclesUiModsLayout greyed;
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_16_9, &nav, &greyed);
    CHECK(greyed.save_note.count == 1);
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_4_3, &nav, &greyed);
    CHECK(greyed.save_note.count == 1 && !strcmp(greyed.save_note.text[0], oracles_mods_save_note(&nav, 1)));
    /* With one, OK and a click ask for the document picker; the note says what the copy is. */
    nav.games[ORACLES_HOME_GAME_AGES].mods_save = 1;
    CHECK(oracles_mods_save_live(&nav) && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_EXPORT_MODS_SAVE);
    nav.row = 0;
    CHECK(oracles_mods_click(&nav, 3) == ORACLES_HOME_EXPORT_MODS_SAVE && nav.row == 3);
    CHECK(!strcmp(oracles_mods_save_action, "Export save with mods\xe2\x80\xa6"));
    CHECK(!strcmp(oracles_mods_save_note(&nav, 0), "Export a copy of the game you played with mods, to play it elsewhere. The mods' own data stays here."));
    CHECK(!strcmp(oracles_mods_save_note(&nav, 1), "Export a copy of the game played with mods."));
    /* Seasons' page reads Seasons' save with mods. */
    nav.games[ORACLES_HOME_GAME_SEASONS].mods_save = 0;
    nav.entry = ORACLES_HOME_SEASONS;
    CHECK(!oracles_mods_save_live(&nav));
    nav.entry = ORACLES_HOME_AGES;
    /* From a game, never. */
    nav.in_game = 1;
    CHECK(!oracles_mods_save_live(&nav) && oracles_home_act(&nav, ORACLES_HOME_OK) == ORACLES_HOME_STAY);
    CHECK(!strcmp(oracles_mods_save_note(&nav, 0), oracles_display_pause_reason));
    nav.in_game = 0;

    /* 16:9: the row under the list, its label in the labels' column, its action where Open folder is, centred on the note
     * beside it, on two lines. */
    OraclesUiModsLayout l;
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(l.save_row.h > 0.0f && l.save_row.y >= l.list.y + l.list.h && l.play_row.y >= l.save_row.y + l.save_row.h);
    CHECK(!memcmp(&l.list, &desktop.list, sizeof l.list) && l.play_row.y > desktop.play_row.y);
    CHECK(l.label_save.x == l.label_folder.x && l.save_row.x == l.folder_row.x && l.save_row.w == l.folder_row.w);
    CHECK(l.save_button_dot.x + l.save_button_dot.w == l.folder_button_dot.x + l.folder_button_dot.w);
    CHECK(l.save_note.count == 2 && !l.save_status_text && l.save_note.lines[0].x == l.folder_path.x);
    for (unsigned i = 0; i < l.save_note.count; i++) CHECK(l.save_note.lines[i].x + l.save_note.lines[i].w <= l.save_button.x - 24.0f + LAYOUT_TOLERANCE);
    const float middle = l.save_note.lines[0].y + l.save_note.h * 0.5f;
    CHECK(l.save_button.y + l.save_button.h * 0.5f > middle - 1.0f && l.save_button.y + l.save_button.h * 0.5f < middle + 1.0f);
    /* Five mods, the row taking the room of a sixth, above the help bar; Save highlighted shows the last ones. */
    for (int i = 0; i < 7; i++) {
        char name[16];
        snprintf(name, sizeof name, "mod-%d", i);
        add_mod(&nav, ORACLES_HOME_GAME_AGES, name, "", 1, 0, 0, 0);
    }
    nav.row = oracles_mods_row_save(&nav);
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(l.shown == ORACLES_UI_MODS_SHOWN - 1u && l.mods[4].index == 8 && l.panel.y + l.panel.h < 1000.0f);
    nav.row = 1;
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_16_9, &nav, &l);
    CHECK(l.mods[0].index == 0 && l.mods[4].index == 4);

    /* 4:3: two of the three mods, the row and Play above the help bar; the label and the action on one baseline, the
     * note on one line under them, whole. */
    page(&nav, ORACLES_HOME_GAME_AGES, 2, 4);
    nav.android = 1;
    nav.games[ORACLES_HOME_GAME_AGES].mods_save = 1;
    CHECK(nav.row == oracles_mods_row_save(&nav));
    oracles_ui_layout_mods(ORACLES_UI_LAYOUT_4_3, &nav, &l);
    CHECK(l.shown == ORACLES_UI_MODS_SHOWN_4_3 - 1u && l.mods[0].index == 1 && l.mods[1].index == 2);
    CHECK(l.save_row.y >= l.list.y + l.list.h && l.play_row.y >= l.save_row.y + l.save_row.h);
    CHECK(l.play_row.y + l.play_row.h <= 982.0f && l.save_row.h >= 100.0f);
    CHECK(l.label_save.baseline == l.save_button.baseline && l.label_save.x == l.label_folder.x);
    CHECK(l.save_button_dot.x + l.save_button_dot.w == l.folder_button_dot.x + l.folder_button_dot.w);
    CHECK(l.save_note.count == 1 && !strcmp(l.save_note.text[0], "Export a copy of the game played with mods."));
    CHECK(l.save_note.lines[0].y >= l.save_button.y + l.save_button.h && l.save_note.lines[0].x == l.count.x);

    /* An export done: its status in place of the note, until the page is left. */
    snprintf(nav.save_flow.text, sizeof nav.save_flow.text, "Save with mods exported.");
    nav.save_flow.state = ORACLES_SAVE_FLOW_DONE;
    CHECK(!oracles_mods_save_note(&nav, 0)[0]);
    for (int layout = 0; layout < 2; layout++) {
        oracles_ui_layout_mods(layout ? ORACLES_UI_LAYOUT_4_3 : ORACLES_UI_LAYOUT_16_9, &nav, &l);
        CHECK(l.save_status_text && !strcmp(l.save_status_text, "Save with mods exported.") && l.save_tone == ORACLES_TONE_OK);
        CHECK(l.save_note.count == 0 && l.save_status.count == 1 && l.save_dot.w > 0.0f);
    }
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(nav.screen == ORACLES_SCREEN_HOME && nav.save_flow.state == ORACLES_SAVE_FLOW_NONE);
}

int main(int argc, char **argv)
{
    if (argc < 2 || !layout_reference_load(argv[1])) { fprintf(stderr, "usage: oracles-test-launcher-mods launcher_layout_reference.json\n"); return 2; }
    if (!oracles_ui_fonts_load()) { fprintf(stderr, "FAIL the embedded fonts do not load\n"); return 1; }
    frame_8a();
    frame_8b();
    frame_8c();
    frame_8d();
    many();
    frame_10l_10m();
    android_folder();
    android_save();
    if (layout_failures()) { fprintf(stderr, "%d failure(s)\n", layout_failures()); return 1; }
    printf("launcher mods: the page matches the layout reference within %.1f px in both layouts\n", (double)LAYOUT_TOLERANCE);
    return 0;
}
