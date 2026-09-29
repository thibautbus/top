#include "pause.h"

#include "home_input.h"
#include "ui_controls.h"
#include "ui_controls_layout.h"
#include "ui_home_layout.h"
#include "ui_page_layout.h"
#include "ui_page_nav.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* While something moves, a frame at most this often: the in-game darkening, left to right. */
#define FRAME_MS 16.0
#ifdef __ANDROID__
/* Android shows a window's surface some frames after it is ready: the single frame drawn at the return from the
 * background can be lost and leave the menu black until an input (home.c does the same). */
#define ANDROID_REDRAWS 30
#else
#define ANDROID_REDRAWS 0
#endif
#define SHADE 0x060609u
#define SHADE_LEFT 0.62f
#define SHADE_RIGHT 0.9f
#define NARROW_WIDTH 960
#define NARROW_MIN_SCALE 0.5f   /* the reduced menu's smallest scale: 27 px labels, a menu 360 px wide */

struct OraclesPause {
    OraclesUiDraw *draw;
    const OraclesHomeHost *host;
    int game;                   /* ORACLES_HOME_GAME_* */
    OraclesProfile playing;
    int prepared_width, prepared_height;
    unsigned late_glyphs;
};

/* Every glyph a screen of the pause may draw: the printable ASCII and the launcher's signs, and each character after an
 * f and after two, the runs whose glyphs the fonts' ligatures and Alegreya's contextual f change. */
static const char glyph_signs[] = "\xc3\x97\xc2\xb7\xe2\x80\xa6\xe2\x80\x94\xe2\x86\x90\xe2\x86\x91\xe2\x86\x92\xe2\x86\x93\xe2\x80\x99";

static void glyph_text(char *out, size_t capacity)
{
    size_t n = 0;
    for (int c = 0x20; c < 0x7f && n + 1 < capacity; c++) out[n++] = (char)c;
    for (size_t i = 0; glyph_signs[i] && n + 1 < capacity; i++) out[n++] = glyph_signs[i];
    for (int c = 0x21; c < 0x7f && n + 8 < capacity; c++) {
        out[n++] = ' '; out[n++] = 'f'; out[n++] = (char)c;
        out[n++] = ' '; out[n++] = 'f'; out[n++] = 'f'; out[n++] = (char)c;
    }
    out[n] = 0;
}

int oracles_pause_narrow(int out_width) { return out_width < NARROW_WIDTH; }

/* The pause's scale floor: the reduced menu keeps its text readable in a small window. */
static float min_scale(int out_width) { return oracles_pause_narrow(out_width) ? NARROW_MIN_SCALE : 0.0f; }

OraclesPause *oracles_pause_create(OraclesUiDraw *draw, const OraclesHomeHost *host, int game, OraclesProfile playing)
{
    OraclesPause *pause = calloc(1, sizeof *pause);
    if (!pause) return NULL;
    pause->draw = draw;
    pause->host = host;
    pause->game = game;
    pause->playing = playing;
    return pause;
}

void oracles_pause_destroy(OraclesPause *pause) { free(pause); }

unsigned oracles_pause_late_glyphs(const OraclesPause *pause) { return pause ? pause->late_glyphs : 0; }

static double now_ms(void) { return (double)SDL_GetPerformanceCounter() * 1000.0 / (double)SDL_GetPerformanceFrequency(); }

/* The styles of the pause menu, of Controls and Display, of the help and the toast. */
static int styles(const OraclesUiTextStyle **out)
{
    const OraclesUiTextStyle *const all[] = {
        &oracles_ui_hero_over, &oracles_ui_hero_title, &oracles_ui_hero_state, &oracles_ui_item_note, &oracles_ui_item_label,
        &oracles_ui_hint_key, &oracles_ui_hint_label, &oracles_ui_version, &oracles_ui_toast,
        &oracles_ui_page_section, &oracles_ui_page_over, &oracles_ui_page_title, &oracles_ui_row_label, &oracles_ui_row_title,
        &oracles_ui_row_text, &oracles_ui_row_note, &oracles_ui_row_button, &oracles_ui_option_name, &oracles_ui_option_size, &oracles_ui_choice,
        &oracles_ui_diagram_label,
        &oracles_ui_controls_title, &oracles_ui_controls_prompt, &oracles_ui_controls_heading, &oracles_ui_controls_name,
        &oracles_ui_controls_cell, &oracles_ui_controls_cell_locked, &oracles_ui_controls_cell_waiting, &oracles_ui_controls_item,
        &oracles_ui_controls_item_empty, &oracles_ui_controls_slots_note, &oracles_ui_controls_shortcut_key,
    };
    const int count = (int)(sizeof all / sizeof all[0]);
    for (int i = 0; i < count; i++) out[i] = all[i];
    return count;
}

static void prepare(OraclesPause *pause, int width, int height, const char *when)
{
    const OraclesUiTextStyle *list[64];
    const int count = styles(list);
    static char text[1024];
    glyph_text(text, sizeof text);
    const double started = now_ms();
    const int glyphs = oracles_ui_draw_prepare(pause->draw, width, height, min_scale(width), list, count, text);
    oracles_ui_draw_freeze(pause->draw, 1);
    pause->prepared_width = width;
    pause->prepared_height = height;
    fprintf(stderr, "oracles: pause menu ready for %dx%d %s: %d glyphs in %.1f ms\n", width, height, when, glyphs, now_ms() - started);
}

void oracles_pause_prepare(OraclesPause *pause, SDL_Renderer *renderer)
{
    int w = 0, h = 0;
    if (!pause || !SDL_GetRenderOutputSize(renderer, &w, &h)) return;
    prepare(pause, w, h, "before the first frame");
}

int oracles_pause_paint(OraclesUiDraw *draw, SDL_Renderer *renderer, SDL_Texture *frame, int width, int height,
                        int out_width, int out_height, OraclesUiHome *view, const OraclesHomeNav *nav, double now_ms)
{
    int moving = 0;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    /* The game's image as the session shows it: its whole scale, centred. */
    int k = width > 0 && height > 0 ? (out_width / width < out_height / height ? out_width / width : out_height / height) : 1;
    if (k < 1) k = 1;
    const SDL_FRect image = { (float)((out_width - width * k) / 2), (float)((out_height - height * k) / 2), (float)(width * k), (float)(height * k) };
    if (frame) SDL_RenderTexture(renderer, frame, NULL, &image);
    /* Darkened, more on the right where the menu is. */
    const SDL_FColor left = { (float)((SHADE >> 16) & 0xffu) / 255.0f, (float)((SHADE >> 8) & 0xffu) / 255.0f, (float)(SHADE & 0xffu) / 255.0f,
                              (float)(Uint8)(SHADE_LEFT * 255.0f + 0.5f) / 255.0f };
    const SDL_FColor right = { left.r, left.g, left.b, (float)(Uint8)(SHADE_RIGHT * 255.0f + 0.5f) / 255.0f };
    const SDL_Vertex shade[4] = {
        { { 0.0f, 0.0f }, left, { 0.0f, 0.0f } }, { { (float)out_width, 0.0f }, right, { 0.0f, 0.0f } },
        { { (float)out_width, (float)out_height }, right, { 0.0f, 0.0f } }, { { 0.0f, (float)out_height }, left, { 0.0f, 0.0f } },
    };
    static const int corners[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_RenderGeometry(renderer, NULL, shade, 4, corners, 6);
    if (oracles_ui_draw_begin_over(draw, out_width, out_height, min_scale(out_width), now_ms)) {
        moving = oracles_ui_home_draw(draw, view, nav, now_ms);
        oracles_ui_draw_end(draw);
    }
    return moving;
}

typedef struct pause_loop {
    OraclesPause *pause;
    SDL_Window *window;
    SDL_Renderer *renderer;
    const OraclesPauseSession *session;
    OraclesHomeNav nav;
    OraclesUiHome view;
    int dirty, moving, done;
    int redraws;   /* frames still to draw whatever changed (ANDROID_REDRAWS) */
    OraclesPauseResult result;
} pause_loop;

static void changed(pause_loop *p)
{
    oracles_ui_home_follow(&p->view, &p->nav, now_ms());
    p->dirty = 1;
}

static void load_note(pause_loop *p)
{
    char time[16] = "";
    if (p->session && p->session->state_time) p->session->state_time(p->session->opaque, time, sizeof time);
    if (time[0]) snprintf(p->nav.load_note, sizeof p->nav.load_note, "F7 \xc2\xb7 from %s", time);
    else snprintf(p->nav.load_note, sizeof p->nav.load_note, "F7");
}

static void command(pause_loop *p, OraclesHomeCommand c)
{
    char message[256] = "";
    switch (c) {
        case ORACLES_HOME_RESUME: p->result = ORACLES_PAUSE_RESUME; p->done = 1; return;
        case ORACLES_HOME_QUIT_GAME: p->result = ORACLES_PAUSE_QUIT; p->done = 1; return;
        case ORACLES_HOME_SAVE_STATE:
            if (p->session->save_state(p->session->opaque, message, sizeof message)) snprintf(message, sizeof message, "State saved");
            load_note(p);
            break;
        case ORACLES_HOME_LOAD_STATE:
            /* A state loaded plays on at once: the image behind the menu would be another moment's. */
            if (p->session->load_state(p->session->opaque, message, sizeof message)) { p->result = ORACLES_PAUSE_RESUME; p->done = 1; return; }
            break;
        case ORACLES_HOME_STORE:
            if (p->pause->host && p->pause->host->store) p->pause->host->store(p->pause->host->opaque, &p->nav);
            if (p->session->settings_changed) p->session->settings_changed(p->session->opaque);
            return;
        default: return;
    }
    if (message[0]) oracles_ui_home_toast(&p->view, message, now_ms());
    changed(p);
}

static void act(pause_loop *p, OraclesHomeAction action)
{
    const OraclesHomeCommand c = oracles_home_act(&p->nav, action);
    changed(p);
    command(p, c);
}

static void pointer(pause_loop *p, float x, float y, int click)
{
    int window_w, window_h, output_w, output_h;
    SDL_GetWindowSize(p->window, &window_w, &window_h);
    SDL_GetRenderOutputSize(p->renderer, &output_w, &output_h);
    const float px = x * (window_w > 0 ? (float)output_w / (float)window_w : 1.0f), py = y * (window_h > 0 ? (float)output_h / (float)window_h : 1.0f);
    float sx, sy;
    OraclesUiHomeHit hit;
    if (p->nav.screen == ORACLES_SCREEN_PAUSE) {
        /* The menu and the help may be anchored apart (a small window): each is looked for where it is drawn. */
        oracles_ui_draw_to_scene_anchored(p->pause->draw, px, py, 1.0f, 1.0f, &sx, &sy);
        hit = oracles_ui_home_hit(&p->nav, sx, sy);
        if (hit.kind != ORACLES_UI_HIT_ITEM) {
            oracles_ui_draw_to_scene_anchored(p->pause->draw, px, py, 0.5f, 1.0f, &sx, &sy);
            hit = oracles_ui_home_hit(&p->nav, sx, sy);
            if (hit.kind != ORACLES_UI_HIT_BACK) hit.kind = ORACLES_UI_HIT_NONE;
        }
    } else {
        oracles_ui_draw_to_scene(p->pause->draw, px, py, &sx, &sy);
        hit = oracles_ui_home_hit(&p->nav, sx, sy);
    }
    OraclesHomeCommand c = ORACLES_HOME_STAY;
    if (hit.kind == ORACLES_UI_HIT_CELL) {
        if (!click) { oracles_controls_hover(&p->nav, hit.column, hit.row); changed(p); return; }
        c = oracles_controls_click(&p->nav, hit.column, hit.row, hit.option);
    } else if (hit.kind == ORACLES_UI_HIT_ROW) {
        if (!click) { if (hit.index != p->nav.row) { p->nav.row = hit.index; changed(p); } return; }
        c = oracles_display_click(&p->nav, hit.index, hit.option);
    } else if (hit.kind == ORACLES_UI_HIT_ITEM) {
        if (!click) { if (hit.index != oracles_home_focus(&p->nav)) { oracles_home_hover(&p->nav, hit.index); changed(p); } return; }
        c = oracles_home_click(&p->nav, hit.index);
    } else if (hit.kind == ORACLES_UI_HIT_BACK) {
        if (click) act(p, ORACLES_HOME_BACK);
        return;
    } else {
        return;
    }
    changed(p);
    command(p, c);
}

static void handle(pause_loop *p, const SDL_Event *e)
{
    OraclesHomeAction action;
    OraclesHomeCommand captured;
    if (oracles_home_capture(&p->nav, e, &captured)) { changed(p); command(p, captured); return; }
    switch (e->type) {
        case SDL_EVENT_QUIT: p->result = ORACLES_PAUSE_WINDOW_CLOSED; p->done = 1; break;
        case SDL_EVENT_KEY_DOWN:
            if (oracles_home_key_action(e->key.key, &action) && (!e->key.repeat || action <= ORACLES_HOME_RIGHT)) act(p, action);
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            if (oracles_home_button_action(e, &action)) act(p, action);
            break;
        case SDL_EVENT_MOUSE_MOTION: pointer(p, e->motion.x, e->motion.y, 0); break;
        case SDL_EVENT_MOUSE_BUTTON_UP: if (e->button.button == SDL_BUTTON_LEFT) pointer(p, e->button.x, e->button.y, 1); break;
        default:   /* a drop is not taken in the pause */
            if (e->type >= SDL_EVENT_WINDOW_FIRST && e->type <= SDL_EVENT_WINDOW_LAST) { p->dirty = 1; p->redraws = ANDROID_REDRAWS; }
            break;
    }
}

OraclesPauseResult oracles_pause_run(OraclesPause *pause, SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *frame,
                                     int width, int height, const OraclesPauseSession *session)
{
    pause_loop p;
    memset(&p, 0, sizeof p);
    p.pause = pause;
    p.window = window;
    p.renderer = renderer;
    p.session = session;
    p.result = ORACLES_PAUSE_RESUME;
    /* The game's logical size lifted while the menu draws in the window's pixels; set back at the resume. */
    int logical_w = 0, logical_h = 0, out_w = 0, out_h = 0;
    SDL_RendererLogicalPresentation presentation = SDL_LOGICAL_PRESENTATION_DISABLED;
    SDL_GetRenderLogicalPresentation(renderer, &logical_w, &logical_h, &presentation);
    SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    SDL_GetRenderOutputSize(renderer, &out_w, &out_h);
    if (out_w != pause->prepared_width || out_h != pause->prepared_height) prepare(pause, out_w, out_h, "at the pause (the window's size changed)");
    const unsigned glyphs_before = oracles_ui_draw_glyphs_rasterised(pause->draw);

    oracles_home_init(&p.nav);
    if (pause->host && pause->host->refresh) pause->host->refresh(pause->host->opaque, &p.nav, window);
    oracles_home_pause(&p.nav, pause->game, pause->playing, oracles_pause_narrow(out_w));
    load_note(&p);
    oracles_ui_home_start(&p.view, &p.nav);
    p.dirty = 1;
    double presented = 0.0;
    while (!p.done) {
        const double now = now_ms();
        if (p.dirty || ((p.moving || p.redraws > 0) && now - presented >= FRAME_MS) || (oracles_ui_home_due_ms(&p.view) >= 0.0 && now >= oracles_ui_home_due_ms(&p.view))) {
            SDL_GetRenderOutputSize(renderer, &out_w, &out_h);
            p.nav.narrow = oracles_pause_narrow(out_w);
            p.moving = oracles_pause_paint(pause->draw, renderer, frame, width, height, out_w, out_h, &p.view, &p.nav, now);
            SDL_RenderPresent(renderer);
            presented = now;
            p.dirty = 0;
            if (p.redraws > 0) p.redraws--;
        }
        int wait = -1;
        const double due = oracles_ui_home_due_ms(&p.view);
        if (p.moving || p.redraws > 0) wait = (int)FRAME_MS;
        if (due >= 0.0) { const int until = due > now ? (int)(due - now) + 1 : 0; if (wait < 0 || until < wait) wait = until; }
        SDL_Event e;
        if (wait < 0 ? SDL_WaitEvent(&e) : SDL_WaitEventTimeout(&e, wait)) {
            handle(&p, &e);
            while (!p.done && SDL_PollEvent(&e)) handle(&p, &e);
        }
    }
    pause->late_glyphs += oracles_ui_draw_glyphs_rasterised(pause->draw) - glyphs_before;
    /* Back to the game's own drawing; what the menu left in the queue is not the game's. */
    SDL_SetRenderClipRect(renderer, NULL);
    SDL_SetRenderLogicalPresentation(renderer, logical_w, logical_h, presentation);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_FlushEvents(SDL_EVENT_KEY_DOWN, SDL_EVENT_GAMEPAD_REMAPPED);
    return p.result;
}
