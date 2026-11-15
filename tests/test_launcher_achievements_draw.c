/* An achievement's detail drawn offscreen with SDL's software renderer, in a build with RetroAchievements: its badge, in colour
 * unlocked and grey locked, its dashed frame without an image, the column's motif gone, the page's name back on a row of
 * settings, the panel over the dimmed list; and what the pointer finds there (oracles_ui_home_hit).  The test's argument is a
 * folder for the badges; a second one keeps each frame there as a PNG, to look at.
 *
 *   oracles-test-launcher-achievements-draw FOLDER [FRAMES_FOLDER]
 */
#include "ui_achievements_draw.h"
#include "ui_draw.h"
#include "ui_home.h"
#include "ui_layout.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); failures++; } } while (0)

#define COLOUR 0xc08020u
#define GREY 0x505050u

static OraclesAchievementsList list;

static void item(unsigned i, const char *title, const char *description, unsigned points, const char *date, int unlocked, float rarity, unsigned tag, const char *badge)
{
    OraclesAchievementsItem *it = &list.items[i];
    snprintf(it->title, sizeof it->title, "%s", title);
    snprintf(it->description, sizeof it->description, "%s", description);
    snprintf(it->date, sizeof it->date, "%s", date);
    snprintf(it->badge, sizeof it->badge, "%s", badge);
    it->points = points;
    it->unlocked = unlocked;
    it->rarity = rarity;
    it->tag = tag;
}

/* The badges' two images: flat, so that a pixel tells which was drawn. */
static void write_badge(const char *folder, const char *name, unsigned rgb)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.png", folder, name);
    SDL_Surface *surface = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_ARGB8888);
    CHECK(surface && SDL_FillSurfaceRect(surface, NULL, 0xff000000u | rgb));
    CHECK(surface && SDL_SavePNG(surface, path));
    SDL_DestroySurface(surface);
}

static void make_list(const char *folder)
{
    memset(&list, 0, sizeof list);
    snprintf(list.badges, sizeof list.badges, "%s", folder);
    list.count = 8;
    list.unlocked = 3;
    list.points = 140;
    list.points_unlocked = 25;
    item(0, "Out of the Woods", "Leave the first forest.", 5, "", 1, 86.0f, ORACLES_ACHIEVEMENT_TAG_PROGRESSION, "00001");
    item(1, "Clean Sweep", "Open every chest of the first dungeon in a single visit, without leaving it, without being hit by its guardian and without loading a savestate on the way.",
         10, "", 1, 12.4f, ORACLES_ACHIEVEMENT_TAG_NONE, "00001");
    item(2, "Ten Hearts", "Hold ten heart containers.", 10, "", 1, 31.7f, ORACLES_ACHIEVEMENT_TAG_NONE, "00001");
    item(3, "Seed Gatherer", "Carry all five kinds of seeds.", 10, "", 0, 22.9f, ORACLES_ACHIEVEMENT_TAG_NONE, "00001");
    snprintf(list.items[3].progress, sizeof list.items[3].progress, "3/5");
    list.items[3].percent = 0.6f;
    item(4, "Rupee Hoarder", "Hold 500 rupees at once.", 10, "", 0, 0.0f, ORACLES_ACHIEVEMENT_TAG_NONE, "00001");
    item(5, "Deep Water", "Reach the bottom of the sea.", 25, "", 0, 9.6f, ORACLES_ACHIEVEMENT_TAG_NONE, "");
    item(6, "No Potion Needed", "Defeat the second dungeon\xe2\x80\x99s guardian without drinking a potion, on the first try: once the guardian falls, the chance is gone for this file.",
         25, "", 0, 0.3f, ORACLES_ACHIEVEMENT_TAG_MISSABLE, "00001");
    item(7, "The Last Door", "Finish the game.", 50, "", 0, 14.8f, ORACLES_ACHIEVEMENT_TAG_WIN, "00001");
}

static void page(OraclesHomeNav *nav, OraclesUiLayout layout, unsigned row)
{
    oracles_home_init(nav);
    nav->layout = layout;
    nav->achievements.shown = nav->achievements.on = nav->achievements.sound = 1;
    snprintf(nav->achievements.user, sizeof nav->achievements.user, "SamLink");
    nav->achievements.list = &list;
    nav->achievements.list_state = ORACLES_ACHIEVEMENTS_LIST_READY;
    oracles_achievements_open(nav);
    nav->row = row;
}

static void pause_list(OraclesHomeNav *nav, OraclesUiLayout layout, unsigned focus)
{
    oracles_home_init(nav);
    oracles_home_pause(nav, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, 0);
    nav->layout = layout;
    nav->achievements.shown = nav->achievements.active = 1;
    nav->achievements.list = &list;
    oracles_achievements_list_open(nav);
    nav->achievements.list_focus = focus;
}

typedef struct canvas {
    SDL_Surface *surface;
    SDL_Renderer *renderer;
    OraclesUiDraw *draw;
    int width, height;
} canvas;

static void canvas_open(canvas *c, int width, int height)
{
    c->width = width;
    c->height = height;
    c->surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ARGB8888);
    c->renderer = c->surface ? SDL_CreateSoftwareRenderer(c->surface) : NULL;
    c->draw = c->renderer ? oracles_ui_draw_create(c->renderer) : NULL;
}

static void canvas_close(canvas *c)
{
    oracles_ui_achievements_forget_badges();
    oracles_ui_draw_destroy(c->draw);
    SDL_DestroyRenderer(c->renderer);
    SDL_DestroySurface(c->surface);
}

/* The screen drawn once its tweens are over, kept as NAME.png in `frames` when given. */
static void paint(canvas *c, const OraclesHomeNav *nav, const char *frames, const char *name)
{
    OraclesUiHome view;
    oracles_ui_home_start(&view, nav);
    SDL_SetRenderDrawColor(c->renderer, 9, 9, 13, 255);
    SDL_RenderClear(c->renderer);
    oracles_ui_draw_layout(c->draw, oracles_ui_home_scene(nav));
    for (double now = 1.0; now < 12000.0; now += 5000.0) {
        CHECK(oracles_ui_draw_begin(c->draw, c->width, c->height, now));
        oracles_ui_home_draw(c->draw, &view, nav, now);
        oracles_ui_draw_end(c->draw);
    }
    SDL_RenderPresent(c->renderer);
    if (frames) {
        char path[1024];
        snprintf(path, sizeof path, "%s/%s.png", frames, name);
        CHECK(SDL_SavePNG(c->surface, path));
    }
}

/* The colour at a point of the scene. */
static unsigned at(const canvas *c, const OraclesHomeNav *nav, float x, float y)
{
    const float scale = (float)c->height / 1080.0f, left = ((float)c->width - oracles_ui_scene_width(oracles_ui_home_scene(nav)) * scale) * 0.5f;
    const int px = (int)(left + x * scale), py = (int)(y * scale);
    const Uint32 *row = (const Uint32 *)((const Uint8 *)c->surface->pixels + (size_t)py * (size_t)c->surface->pitch);
    return row[px] & 0xffffffu;
}

/* How much light text a box of the scene holds, in thousandths of its pixels. */
static unsigned light(const canvas *c, const OraclesHomeNav *nav, const OraclesUiBox *b, unsigned luma)
{
    unsigned lit = 0, seen = 0;
    for (float y = b->y; y < b->y + b->h; y += 2.0f)
        for (float x = b->x; x < b->x + b->w; x += 2.0f) {
            const unsigned rgb = at(c, nav, x, y);
            lit += ((rgb >> 16 & 0xffu) + (rgb >> 8 & 0xffu) + (rgb & 0xffu)) / 3u >= luma;
            seen++;
        }
    return seen ? lit * 1000u / seen : 0u;
}

static OraclesUiBox centre_of(const OraclesUiBox *b) { const OraclesUiBox c = { b->x + b->w * 0.4f, b->y + b->h * 0.4f, b->w * 0.2f, b->h * 0.2f }; return c; }

static unsigned badge_colour(const canvas *c, const OraclesHomeNav *nav)
{
    static OraclesUiAchievementDetail d;
    oracles_ui_layout_achievement_detail(nav->layout, oracles_achievements_detail_item(nav), &d);
    const OraclesUiBox b = centre_of(&d.badge);
    return at(c, nav, b.x, b.y);
}

/* 16:9: the detail in the column, unlocked in colour, locked in grey, the dashed frame without an image; the game's title gone
 * while it shows, back on a row of settings; the pointer's rows; the motif faded out. */
static void column(const char *frames)
{
    canvas c;
    canvas_open(&c, 1920, 1080);
    CHECK(c.draw != NULL);
    if (!c.draw) return;
    OraclesHomeNav nav;
    static OraclesUiAchievementDetail d;
    const OraclesUiBox title_place = { 96.0f, 180.0f, 300.0f, 100.0f };
    page(&nav, ORACLES_UI_LAYOUT_16_9, ORACLES_ACHIEVEMENTS_ROW_SOUND);
    paint(&c, &nav, frames, "settings");
    CHECK(light(&c, &nav, &title_place, 180u) > 20u);   /* the game's title */
    const OraclesUiBox clear = { 96.0f, 560.0f, 520.0f, 420.0f };
    page(&nav, ORACLES_UI_LAYOUT_16_9, ORACLES_ACHIEVEMENTS_ROWS + 1u);
    paint(&c, &nav, frames, "40a");
    CHECK(badge_colour(&c, &nav) == COLOUR);
    oracles_ui_layout_achievement_detail(nav.layout, oracles_achievements_detail_item(&nav), &d);
    const OraclesUiBox text = { d.description[0].x, d.description[0].y, d.description[0].w, d.description[0].h };
    CHECK(light(&c, &nav, &text, 150u) > 20u);
    CHECK(light(&c, &nav, &clear, 30u) == 0u);   /* no motif under the column */
    /* The casual mode's note gone: its place is free beside a short description. */
    const OraclesUiBox note_place = { 330.0f, 345.0f, 280.0f, 50.0f };
    page(&nav, ORACLES_UI_LAYOUT_16_9, ORACLES_ACHIEVEMENTS_ROW_SOUND);
    paint(&c, &nav, NULL, NULL);
    CHECK(light(&c, &nav, &note_place, 110u) > 0u);
    page(&nav, ORACLES_UI_LAYOUT_16_9, ORACLES_ACHIEVEMENTS_ROWS);
    paint(&c, &nav, NULL, NULL);
    CHECK(light(&c, &nav, &note_place, 110u) == 0u);
    page(&nav, ORACLES_UI_LAYOUT_16_9, ORACLES_ACHIEVEMENTS_ROWS + 6u);
    paint(&c, &nav, frames, "40b");
    CHECK(badge_colour(&c, &nav) == GREY);
    page(&nav, ORACLES_UI_LAYOUT_16_9, ORACLES_ACHIEVEMENTS_ROWS + 5u);
    paint(&c, &nav, frames, "no-badge");
    CHECK((badge_colour(&c, &nav) >> 16 & 0xffu) < 0x20u);   /* the dashed frame's inside */
    /* A badge not downloaded yet: looked for again. */
    snprintf(list.items[5].badge, sizeof list.items[5].badge, "00009");
    paint(&c, &nav, NULL, NULL);
    CHECK(oracles_ui_achievements_list_waiting());
    list.items[5].badge[0] = 0;
    page(&nav, ORACLES_UI_LAYOUT_16_9, ORACLES_ACHIEVEMENTS_ROWS + 3u);
    paint(&c, &nav, frames, "progress");
    /* The pause's, with its date. */
    pause_list(&nav, ORACLES_UI_LAYOUT_16_9, 1u);
    snprintf(list.items[1].date, sizeof list.items[1].date, "7 Nov 2026");
    paint(&c, &nav, frames, "40e");
    CHECK(badge_colour(&c, &nav) == COLOUR);
    /* The pointer finds the row. */
    static OraclesUiAchievementsLayout l;
    oracles_ui_layout_achievements_list(nav.layout, &nav, &l);
    const OraclesUiHomeHit row = oracles_ui_home_hit(&nav, l.rows[4].row.x + 40.0f, l.rows[4].row.y + 20.0f);
    CHECK(row.kind == ORACLES_UI_HIT_ACHIEVEMENT && row.index == 4u);
    list.items[1].date[0] = 0;
    canvas_close(&c);
}

/* 4:3: the list under the panel dimmed, the panel's badge and description; a tap in the panel nothing, outside it or on Close
 * back, on the page and on the pause's list. */
static void panel(const char *frames, int width, int height, const char *name)
{
    canvas c;
    canvas_open(&c, width, height);
    CHECK(c.draw != NULL);
    if (!c.draw) return;
    char frame[64];
    OraclesHomeNav nav;
    static OraclesUiAchievementDetail d;
    page(&nav, ORACLES_UI_LAYOUT_4_3, ORACLES_ACHIEVEMENTS_ROWS + 1u);
    snprintf(frame, sizeof frame, "%s-list", name);
    paint(&c, &nav, frames, frame);
    const OraclesUiBox foot = { 64.0f, 640.0f, 1312.0f, 300.0f };
    const unsigned open_before = light(&c, &nav, &foot, 110u);
    oracles_home_act(&nav, ORACLES_HOME_OK);
    CHECK(oracles_achievements_detail_open(&nav));
    snprintf(frame, sizeof frame, "%s-panel", name);
    paint(&c, &nav, frames, frame);
    CHECK(light(&c, &nav, &foot, 110u) * 2u < open_before + 1u);   /* dimmed */
    if (height >= 960) CHECK(badge_colour(&c, &nav) == COLOUR);
    oracles_ui_layout_achievement_detail(nav.layout, oracles_achievements_detail_item(&nav), &d);
    OraclesUiHomeHit hit = oracles_ui_home_hit(&nav, d.panel.x + 500.0f, d.panel.y + 100.0f);
    CHECK(hit.kind == ORACLES_UI_HIT_NONE);
    hit = oracles_ui_home_hit(&nav, d.panel.x + 500.0f, d.panel.y + d.panel.h + 40.0f);   /* over a row of the list */
    CHECK(hit.kind == ORACLES_UI_HIT_BACK);
    hit = oracles_ui_home_hit(&nav, 500.0f, 60.0f);
    CHECK(hit.kind == ORACLES_UI_HIT_BACK);
    /* Locked, Missable: grey, the next one without closing. */
    for (int i = 0; i < 5; i++) oracles_home_act(&nav, ORACLES_HOME_DOWN);
    snprintf(frame, sizeof frame, "%s-missable", name);
    paint(&c, &nav, frames, frame);
    CHECK(oracles_achievements_detail_open(&nav) && oracles_achievements_detail_item(&nav) == &list.items[6]);
    if (height >= 960) CHECK(badge_colour(&c, &nav) == GREY);
    /* Closed: the row of the list takes the pointer again. */
    oracles_home_act(&nav, ORACLES_HOME_BACK);
    CHECK(!oracles_achievements_detail_open(&nav) && nav.screen == ORACLES_SCREEN_ACHIEVEMENTS);
    static OraclesUiAchievementsPageLayout shown;
    oracles_ui_layout_achievements_page(nav.layout, &nav, &shown);
    const OraclesUiBox *under = &shown.rows[shown.shown - 1u].row;
    hit = oracles_ui_home_hit(&nav, under->x + 40.0f, under->y + 20.0f);
    CHECK(hit.kind == ORACLES_UI_HIT_ROW && hit.index == ORACLES_ACHIEVEMENTS_ROWS + shown.rows[shown.shown - 1u].index);
    /* The pause's. */
    pause_list(&nav, ORACLES_UI_LAYOUT_4_3, 1u);
    snprintf(list.items[1].date, sizeof list.items[1].date, "7 Nov 2026");
    static OraclesUiAchievementsLayout l;
    oracles_ui_layout_achievements_list(nav.layout, &nav, &l);
    hit = oracles_ui_home_hit(&nav, l.rows[2].row.x + 40.0f, l.rows[2].row.y + 20.0f);
    CHECK(hit.kind == ORACLES_UI_HIT_ACHIEVEMENT && hit.index == 2u);
    oracles_achievements_list_click(&nav, 1u);
    snprintf(frame, sizeof frame, "%s-pause", name);
    paint(&c, &nav, frames, frame);
    hit = oracles_ui_home_hit(&nav, l.rows[6].row.x + 40.0f, l.rows[6].row.y + 20.0f);
    CHECK(hit.kind == ORACLES_UI_HIT_BACK);
    list.items[1].date[0] = 0;
    canvas_close(&c);
}

/* The last line of the detail shown, and its state, hold light text. */
static void last_lines_lit(const canvas *c, const OraclesHomeNav *nav)
{
    static OraclesUiAchievementDetail d;
    oracles_ui_layout_achievement_detail(nav->layout, oracles_achievements_detail_item(nav), &d);
    CHECK(d.description_lines > 4u || nav->layout == ORACLES_UI_LAYOUT_4_3);
    const OraclesUiLine *last = &d.description[d.description_lines - 1u];
    const OraclesUiBox text = { last->x, last->y, last->w, last->h }, state = { d.state.x, d.state.y, d.state.w, d.state.h };
    CHECK(light(c, nav, &text, 150u) > 20u);
    CHECK(light(c, nav, &state, 150u) > 20u);
}

/* A pause too small for the column, drawn as the pause draws it: the reduced menu's scale floor, the scene centred and cut; the
 * panel within the window, the badge in sight, hints readable under it.  The column at 900x506. */
static void small_window(const char *frames, int width, int height, int panel_wanted)
{
    canvas c;
    canvas_open(&c, width, height);
    CHECK(c.draw != NULL);
    if (!c.draw) return;
    OraclesHomeNav nav;
    pause_list(&nav, ORACLES_UI_LAYOUT_16_9, 1u);
    nav.narrow = 1;
    oracles_achievements_detail_view(&nav, width, height);
    CHECK(oracles_achievements_detail_panel(&nav) == panel_wanted);
    if (panel_wanted) oracles_home_act(&nav, ORACLES_HOME_OK);
    snprintf(list.items[1].date, sizeof list.items[1].date, "7 Nov 2026");
    OraclesUiHome view;
    oracles_ui_home_start(&view, &nav);
    SDL_SetRenderDrawColor(c.renderer, 9, 9, 13, 255);
    SDL_RenderClear(c.renderer);
    oracles_ui_draw_layout(c.draw, ORACLES_UI_LAYOUT_16_9);
    CHECK(oracles_ui_draw_begin_over(c.draw, width, height, ORACLES_UI_NARROW_MIN_SCALE, 1.0));
    oracles_ui_home_draw(c.draw, &view, &nav, 1.0);
    oracles_ui_draw_end(c.draw);
    SDL_RenderPresent(c.renderer);
    if (frames) {
        char path[1024];
        snprintf(path, sizeof path, "%s/pause-%dx%d.png", frames, width, height);
        CHECK(SDL_SavePNG(c.surface, path));
    }
    /* The badge's middle where the scene's centre at scale 0.5 puts it. */
    static OraclesUiAchievementDetail d;
    oracles_ui_achievement_detail_of(&nav, &d);
    const float x = (d.badge.x + d.badge.w * 0.5f - 960.0f) * 0.5f + (float)width * 0.5f, y = (d.badge.y + d.badge.h * 0.5f - 540.0f) * 0.5f + (float)height * 0.5f;
    CHECK(x >= 0.0f && x < (float)width && y >= 0.0f && y < (float)height);
    const Uint32 *row = (const Uint32 *)((const Uint8 *)c.surface->pixels + (size_t)y * (size_t)c.surface->pitch);
    CHECK((row[(int)x] & 0xffffffu) == COLOUR);
    if (panel_wanted) {
        /* What the pointer finds there, with the window's coordinates turned to the scene's as the pause turns them. */
        float sx, sy;
        oracles_ui_draw_to_scene_anchored(c.draw, x, y, 0.5f, 0.5f, &sx, &sy);
        CHECK(oracles_ui_achievements_detail_hit(&nav, sx, sy));
        CHECK(oracles_ui_home_hit(&nav, sx, sy).kind == ORACLES_UI_HIT_NONE);
        oracles_ui_draw_to_scene_anchored(c.draw, (float)width * 0.5f, (float)height - 4.0f - 34.0f, 0.5f, 0.5f, &sx, &sy);
        CHECK(!oracles_ui_achievements_detail_hit(&nav, sx, sy));
    }
    list.items[1].date[0] = 0;
    canvas_close(&c);
}

/* The longest description, in both shapes, drawn to its last line, and its state under it. */
static void longest(const char *frames)
{
    memset(list.items[1].description, 0, sizeof list.items[1].description);
    while (strlen(list.items[1].description) + 12u < 255u) strcat(list.items[1].description, "Wide Mummies ");
    canvas c;
    canvas_open(&c, 1920, 1080);
    if (c.draw) {
        OraclesHomeNav nav;
        page(&nav, ORACLES_UI_LAYOUT_16_9, ORACLES_ACHIEVEMENTS_ROWS + 1u);
        paint(&c, &nav, frames, "longest-16-9");
        last_lines_lit(&c, &nav);
        canvas_close(&c);
    }
    canvas_open(&c, 1440, 1080);
    if (c.draw) {
        OraclesHomeNav nav;
        page(&nav, ORACLES_UI_LAYOUT_4_3, ORACLES_ACHIEVEMENTS_ROWS + 1u);
        oracles_home_act(&nav, ORACLES_HOME_OK);
        paint(&c, &nav, frames, "longest-4-3");
        last_lines_lit(&c, &nav);
        canvas_close(&c);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s FOLDER [FRAMES_FOLDER]\n", argv[0]); return 2; }
    const char *frames = argc > 2 ? argv[2] : NULL;
    if (!SDL_Init(0)) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    write_badge(argv[1], "00001", COLOUR);
    write_badge(argv[1], "00001_lock", GREY);
    make_list(argv[1]);
    column(frames);
    panel(frames, 1440, 1080, "4-3");
    panel(frames, 1280, 960, "40d");
    panel(frames, 640, 480, "40c");
    small_window(frames, 480, 432, 1);
    small_window(frames, 640, 360, 1);
    small_window(frames, 900, 506, 0);
    longest(frames);
    SDL_Quit();
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("launcher achievements drawn: the detail's column and panel, and what the pointer finds there\n");
    return 0;
}
