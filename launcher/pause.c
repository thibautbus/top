#include "pause.h"

#include "home_input.h"
#include "safe_area.h"
#include "touch_controls.h"
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
/* The 4:3 pages over the game (Display, Controls) have no panels: a second layer takes the image nearly to black. */
#define PAGE_SHADE 0x09080cu
#define PAGE_SHADE_ALPHA 0.86f
#define NARROW_MIN_SCALE 0.5f   /* the reduced menu's smallest scale: 27 px labels, a menu 360 px wide */
#define PAD_TOAST_SHOWN_MS 4000.0   /* the reminder over a game, whatever the player presses, then its fade */
#define PAD_TOAST_FADE_MS 600.0
#define STATUS_BAR_LINES 16.0f   /* the status bar's, at the top of the game's image */

struct OraclesPause {
    OraclesUiDraw *draw;
    const OraclesHomeHost *host;
    int game;                   /* ORACLES_HOME_GAME_* */
    OraclesProfile playing;
    int aspect;                 /* the settings' aspect= */
    OraclesTouchEdges edges;    /* the screen margin and what it was read from (oracles_safe_area_edges): the menu keeps to its band */
    int prepared_width, prepared_height, prepared_top, prepared_band;   /* the output and the band the glyphs were rasterised for */
    unsigned late_glyphs;
    int reminder;               /* the reminder over the game, waiting for the status bar or shown (oracles_pause_reminder) */
    char menu_pad[ORACLES_HOME_NAME_LENGTH];   /* the controller's input it names */
    double reminder_since_ms;   /* its first frame's time, 0 before it */
    int reminder_drawn;         /* it was drawn over a frame */
    OraclesInputDevice device;  /* used last, from the home screen to the session's last pause; a key pressed there */
    int keyboard_used;
    int android;
};

/* Every glyph a screen of the pause may draw: the printable ASCII and the launcher's signs, and each character after an
 * f and after two, the runs whose glyphs the fonts' ligatures and Alegreya's contextual f change. */
static const char glyph_signs[] = "\xc3\x97\xc2\xb7\xe2\x80\xa6\xe2\x80\x94\xe2\x86\x90\xe2\x86\x91\xe2\x86\x92\xe2\x86\x93\xe2\x80\x99\xe2\x80\xba";

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

/* The pause's scale floor: the reduced menu keeps its text readable in a small window; the 4:3 layout, taken from
 * about 640x480 only (oracles_ui_layout_pause), fits. */
static float min_scale(int narrow) { return narrow ? NARROW_MIN_SCALE : 0.0f; }

OraclesPause *oracles_pause_create(OraclesUiDraw *draw, const OraclesHomeHost *host, int game, OraclesProfile playing, int aspect, int margin,
                                   const OraclesHomeNav *input)
{
    OraclesPause *pause = calloc(1, sizeof *pause);
    if (!pause) return NULL;
    pause->draw = draw;
    pause->host = host;
    pause->game = game;
    pause->playing = playing;
    pause->aspect = aspect;
    pause->edges.margin = margin;
    pause->device = input->device;
    pause->keyboard_used = input->keyboard_used;
    pause->android = input->android;
    return pause;
}

void oracles_pause_input(const OraclesPause *pause, OraclesHomeNav *nav)
{
    nav->device = pause->device;
    nav->keyboard_used = pause->keyboard_used;
}

void oracles_pause_destroy(OraclesPause *pause) { free(pause); }

void oracles_pause_reminder(OraclesPause *pause, const char *menu_pad)
{
    pause->reminder = 1;
    snprintf(pause->menu_pad, sizeof pause->menu_pad, "%s", menu_pad ? menu_pad : "");
    pause->reminder_since_ms = 0.0;
    pause->reminder_drawn = 0;
}

int oracles_pause_reminder_pending(const OraclesPause *pause) { return pause && pause->reminder; }

int oracles_pause_reminder_drawn(const OraclesPause *pause) { return pause && pause->reminder_drawn; }

unsigned oracles_pause_late_glyphs(const OraclesPause *pause) { return pause ? pause->late_glyphs : 0; }

static double now_ms(void) { return (double)SDL_GetPerformanceCounter() * 1000.0 / (double)SDL_GetPerformanceFrequency(); }

/* The styles of the pause menu, of Controls and Display, of the help and the toast, in `layout` (Controls' tabs
 * NULL in 16:9: left out). */
static int styles(OraclesUiLayout layout, const OraclesUiTextStyle **out)
{
    const OraclesUiHomeStyles *home = oracles_ui_home_styles(layout);
    const OraclesUiPageStyles *page = oracles_ui_page_styles(layout);
    const OraclesUiControlsStyles *controls = oracles_ui_controls_styles(layout);
    const OraclesUiTextStyle *const all[] = {
        home->hero_over, home->hero_title, home->hero_state, home->item_note, home->item_label,
        home->hint_key, home->hint_label, home->version, home->toast, home->pad_toast,
        page->section, page->over, page->title, page->row_label, page->row_title, page->row_text, page->row_note, page->row_button,
        page->option_name, page->option_size, page->choice, page->diagram_label, page->help, page->help_note, page->page_note, page->quality_sets,
        controls->title, controls->prompt, controls->heading, controls->name, controls->cell, controls->cell_locked, controls->cell_waiting,
        controls->item, controls->item_empty, controls->slots_note, controls->shortcut_key, controls->shortcut_label, controls->tab,
        controls->menu_cell, controls->menu_cell_waiting,
    };
    int count = 0;
    for (size_t i = 0; i < sizeof all / sizeof all[0]; i++) if (all[i]) out[count++] = all[i];
    return count;
}

/* The drawing's band in an output of width x height: the screen margin's (oracles_touch_band). */
static OraclesTouchBox band(const OraclesPause *pause, int width, int height)
{
    const OraclesTouchBox safe = oracles_touch_band(width, height, &pause->edges);
    oracles_ui_draw_band(pause->draw, (int)safe.y, (int)safe.h);
    return safe;
}

static void prepare(OraclesPause *pause, int width, int height, const char *when)
{
    const OraclesTouchBox safe = band(pause, width, height);
    const OraclesUiLayout layout = oracles_ui_layout_pause(width, height, pause->aspect);
    const OraclesUiTextStyle *list[64];
    const int count = styles(layout, list);
    static char text[1024];
    glyph_text(text, sizeof text);
    const double started = now_ms();
    oracles_ui_draw_layout(pause->draw, layout);
    const int glyphs = oracles_ui_draw_prepare(pause->draw, width, height, min_scale(oracles_ui_layout_narrow(layout, width)), list, count, text);
    oracles_ui_draw_freeze(pause->draw, 1);
    pause->prepared_width = width;
    pause->prepared_height = height;
    pause->prepared_top = (int)safe.y;
    pause->prepared_band = (int)safe.h;
    fprintf(stderr, "oracles: pause menu ready for %dx%d in %s %s: %d glyphs in %.1f ms\n", width, height,
            layout == ORACLES_UI_LAYOUT_4_3 ? "4:3" : "16:9", when, glyphs, now_ms() - started);
}

void oracles_pause_prepare(OraclesPause *pause, SDL_Renderer *renderer)
{
    int w = 0, h = 0;
    if (!pause || !SDL_GetRenderOutputSize(renderer, &w, &h)) return;
    oracles_safe_area_edges(SDL_GetRenderWindow(renderer), pause->edges.margin, &pause->edges);
    prepare(pause, w, h, "before the first frame");
}

int oracles_pause_paint(OraclesUiDraw *draw, SDL_Renderer *renderer, SDL_Texture *frame, int width, int height, int fill,
                        int out_width, int out_height, OraclesUiHome *view, const OraclesHomeNav *nav, double now_ms,
                        const OraclesPauseSession *session)
{
    int moving = 0;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    /* The game's image as the session shows it: its whole scale, or Fill's size, centred in the drawing's band. */
    if (session && session->game_size) session->game_size(session->opaque, &width, &height);
    if (frame && width > 0 && height > 0) {
        int band_top, band_height;
        oracles_ui_draw_band_of(draw, out_height, &band_top, &band_height);
        const OraclesTouchBox box = oracles_touch_frame(out_width, band_height, width, height, fill);
        const SDL_FRect image = { box.x, (float)band_top + box.y, box.w, box.h };
        if (session && session->game_image) session->game_image(session->opaque, renderer, &image);
        else SDL_RenderTexture(renderer, frame, NULL, &image);
    }
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
    if (oracles_ui_home_scene(nav) == ORACLES_UI_LAYOUT_4_3 && nav->screen != ORACLES_SCREEN_PAUSE) {
        SDL_SetRenderDrawColor(renderer, (Uint8)(PAGE_SHADE >> 16), (Uint8)(PAGE_SHADE >> 8), (Uint8)PAGE_SHADE, (Uint8)(PAGE_SHADE_ALPHA * 255.0f + 0.5f));
        SDL_RenderFillRect(renderer, NULL);
    }
    oracles_ui_draw_layout(draw, oracles_ui_home_scene(nav));
    if (oracles_ui_draw_begin_over(draw, out_width, out_height, min_scale(nav->narrow), now_ms)) {
        moving = oracles_ui_home_draw(draw, view, nav, now_ms);
        oracles_ui_draw_end(draw);
    }
    return moving;
}

int oracles_pause_overlay(OraclesPause *pause, SDL_Renderer *renderer, const OraclesPauseFrame *frame)
{
    if (!pause->reminder) return 0;
    const double now = now_ms();
    if (pause->reminder_since_ms <= 0.0) {
        if (!frame->in_play) return 1;   /* a title, a menu, a cutscene: it waits for the status bar */
        pause->reminder_since_ms = now;
    }
    const double shown = now - pause->reminder_since_ms;
    if (shown >= PAD_TOAST_SHOWN_MS + PAD_TOAST_FADE_MS) { pause->reminder = 0; return 0; }
    const float opacity = shown < PAD_TOAST_SHOWN_MS ? 1.0f : 1.0f - oracles_ui_curve_at(&oracles_ui_ease, (float)((shown - PAD_TOAST_SHOWN_MS) / PAD_TOAST_FADE_MS));
    char text[ORACLES_HOME_NAME_LENGTH + 32];
    oracles_controls_menu_tip(frame->device >= 0 ? frame->device : (int)pause->device, pause->android, pause->menu_pad, text, sizeof text);
    /* In the window's pixels, as the pause draws: the game's logical size lifted, then set back. */
    int logical_w = 0, logical_h = 0, w = 0, h = 0;
    SDL_RendererLogicalPresentation presentation = SDL_LOGICAL_PRESENTATION_DISABLED;
    SDL_GetRenderLogicalPresentation(renderer, &logical_w, &logical_h, &presentation);
    SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    SDL_GetRenderOutputSize(renderer, &w, &h);
    const OraclesUiLayout layout = oracles_ui_layout_pause(w, h, pause->aspect);
    band(pause, w, h);
    oracles_ui_draw_layout(pause->draw, layout);
    if (oracles_ui_draw_begin_over(pause->draw, w, h, min_scale(oracles_ui_layout_narrow(layout, w)), now)) {
        oracles_ui_draw_anchor(pause->draw, 0.5f, 0.0f);
        /* Under the status bar, the image's first sixteen lines, where the session shows the image (as the pause does). */
        int band_top = 0, band_height = h;
        oracles_ui_draw_band_of(pause->draw, h, &band_top, &band_height);
        float bar_bottom = 0.0f, x = 0.0f;
        if (frame->width > 0 && frame->height > 0) {
            const OraclesTouchBox image = oracles_touch_frame(w, band_height, frame->width, frame->height, frame->fill);
            oracles_ui_draw_to_scene_anchored(pause->draw, 0.0f, (float)band_top + image.y + image.h * STATUS_BAR_LINES / (float)frame->height,
                                              0.5f, 0.0f, &x, &bar_bottom);
        }
        oracles_ui_home_pad_toast(pause->draw, layout, text, bar_bottom, opacity);
        oracles_ui_draw_end(pause->draw);
        pause->reminder_drawn = 1;
    }
    SDL_SetRenderClipRect(renderer, NULL);
    SDL_SetRenderLogicalPresentation(renderer, logical_w, logical_h, presentation);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    return 1;
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
    int trigger_held;   /* the menu's trigger, when menu_pad= names one (oracles_home_button_action) */
    unsigned glyphs_before;   /* the glyphs rasterised when the pause began, those the margin's change prepared added */
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
    if (time[0]) snprintf(p->nav.load_note, sizeof p->nav.load_note, "from %s", time);
    else p->nav.load_note[0] = 0;
}

/* Display changed the screen margin: the menu takes its band at once, its glyphs rasterised for it (none counted late). */
static void margin_changed(pause_loop *p, int margin)
{
    int w = 0, h = 0;
    oracles_safe_area_edges(p->window, margin, &p->pause->edges);
    if (!SDL_GetRenderOutputSize(p->renderer, &w, &h)) return;
    const unsigned before = oracles_ui_draw_glyphs_rasterised(p->pause->draw);
    prepare(p->pause, w, h, "at the pause (the screen margin changed)");
    p->glyphs_before += oracles_ui_draw_glyphs_rasterised(p->pause->draw) - before;
    p->dirty = 1;
}

/* Display changed the window: the menu takes its new size at once, its glyphs rasterised for it (none counted late). */
static void window_changed(pause_loop *p)
{
    int w = 0, h = 0;
    if (!SDL_GetRenderOutputSize(p->renderer, &w, &h) || (w == p->pause->prepared_width && h == p->pause->prepared_height)) return;
    oracles_safe_area_edges(p->window, p->pause->edges.margin, &p->pause->edges);
    const unsigned before = oracles_ui_draw_glyphs_rasterised(p->pause->draw);
    prepare(p->pause, w, h, "at the pause (the window changed)");
    p->glyphs_before += oracles_ui_draw_glyphs_rasterised(p->pause->draw) - before;
    p->dirty = 1;
}

/* As the pause opens: the reminder over the game goes once shown, or when the player opened the menu by its key; the
 * tips' counts change (oracles_controls_tips_opened), and settings.txt keeps them. */
static void tips(pause_loop *p, int by_key)
{
    if (p->pause->reminder_since_ms > 0.0 || by_key) p->pause->reminder = 0;
    if (oracles_controls_tips_opened(&p->nav, by_key) && p->pause->host && p->pause->host->store) p->pause->host->store(p->pause->host->opaque, &p->nav);
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
            if (p->nav.display.window_chosen && p->session->window_chosen)
                p->session->window_chosen(p->session->opaque, p->nav.display.window < 3 ? p->nav.display.window + 2 : 0);
            p->nav.display.window_chosen = 0;
            if (p->nav.screen == ORACLES_SCREEN_CHEATS && p->session->cheats_changed)
                p->session->cheats_changed(p->session->opaque, p->nav.games[p->pause->game].cheats);
            if (p->session->settings_changed) {
                const int margin = p->session->settings_changed(p->session->opaque);
                if (margin != p->pause->edges.margin) margin_changed(p, margin);
                else window_changed(p);
            }
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
        c = p->nav.screen == ORACLES_SCREEN_CHEATS ? oracles_cheats_click(&p->nav, hit.index, hit.option) : oracles_display_click(&p->nav, hit.index, hit.option);
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
    OraclesInputDevice device;
    if (oracles_home_input_device(e, &device) && oracles_home_input(&p->nav, device)) p->dirty = 1;
    if (oracles_home_capture(&p->nav, e, &captured)) {
        /* A trigger taken for the menu is held still: it counts once let go. */
        if (captured == ORACLES_HOME_STORE) p->trigger_held = oracles_home_trigger_held(&p->nav);
        changed(p);
        command(p, captured);
        return;
    }
    switch (e->type) {
        case SDL_EVENT_QUIT: p->result = ORACLES_PAUSE_WINDOW_CLOSED; p->done = 1; break;
        case SDL_EVENT_KEY_DOWN:
            if (oracles_home_key_action(e->key.key, &action) && (!e->key.repeat || action <= ORACLES_HOME_RIGHT)) act(p, action);
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (oracles_home_button_action(&p->nav, e, &p->trigger_held, &action)) act(p, action);
            break;
        case SDL_EVENT_MOUSE_MOTION: pointer(p, e->motion.x, e->motion.y, 0); break;
        case SDL_EVENT_MOUSE_BUTTON_UP: if (e->button.button == SDL_BUTTON_LEFT) pointer(p, e->button.x, e->button.y, 1); break;
        default:   /* a drop is not taken in the pause */
            if (e->type == SDL_EVENT_WINDOW_SAFE_AREA_CHANGED || e->type == SDL_EVENT_WINDOW_DISPLAY_CHANGED)
                oracles_safe_area_edges(p->window, p->pause->edges.margin, &p->pause->edges);
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
    oracles_safe_area_edges(window, pause->edges.margin, &pause->edges);
    const OraclesTouchBox safe = oracles_touch_band(out_w, out_h, &pause->edges);
    if (out_w != pause->prepared_width || out_h != pause->prepared_height || (int)safe.y != pause->prepared_top || (int)safe.h != pause->prepared_band)
        prepare(pause, out_w, out_h, "at the pause (the window's size or its band changed)");
    p.glyphs_before = oracles_ui_draw_glyphs_rasterised(pause->draw);

    oracles_home_init(&p.nav);
    if (pause->host && pause->host->refresh) pause->host->refresh(pause->host->opaque, &p.nav, window);
    p.nav.layout = oracles_ui_layout_pause(out_w, out_h, p.nav.display.aspect);
    oracles_home_pause(&p.nav, pause->game, pause->playing, oracles_ui_layout_narrow(p.nav.layout, out_w));
    p.nav.display.core = session->core;
    p.nav.display.workers = session->ghosts;
    /* Cheats shows the running game's, not the settings', and changes them. */
    p.nav.cheats_fixed = 0;
    p.nav.cheats_refused = !session->has_cheats;
    p.nav.text_speed_refused = !session->has_text_instant;
    if (pause->game == ORACLES_HOME_GAME_AGES || pause->game == ORACLES_HOME_GAME_SEASONS) {
        p.nav.swim_refused = !session->has_swim;
        p.nav.hotkeys_refused = !session->has_hotkeys;
    }
    /* Window shows the window the game has, and leaves alone those that would take Display from the pause. */
    if (session->window != -1) p.nav.display.window = session->window == -2 ? -1 : session->window ? session->window - 2 : 3;
    p.nav.display.window_w = session->window_w;
    p.nav.display.window_h = session->window_h;
    p.nav.display.windows_refused = session->windows_refused;
    if (pause->game == ORACLES_HOME_GAME_AGES || pause->game == ORACLES_HOME_GAME_SEASONS) p.nav.games[pause->game].cheats = session->cheats;
    /* The hints follow the device that opened the pause, else the one used last before. */
    oracles_pause_input(pause, &p.nav);
    if (session->device >= 0) oracles_home_input(&p.nav, (OraclesInputDevice)session->device);
    load_note(&p);
    tips(&p, session->by_key);
    p.trigger_held = oracles_home_trigger_held(&p.nav);
    oracles_ui_home_start(&p.view, &p.nav);
    p.dirty = 1;
    double presented = 0.0;
    while (!p.done) {
        const double now = now_ms();
        if (p.dirty || ((p.moving || p.redraws > 0) && now - presented >= FRAME_MS) || (oracles_ui_home_due_ms(&p.view) >= 0.0 && now >= oracles_ui_home_due_ms(&p.view))) {
            SDL_GetRenderOutputSize(renderer, &out_w, &out_h);
            p.nav.layout = oracles_ui_layout_pause(out_w, out_h, p.nav.display.aspect);
            p.nav.narrow = oracles_ui_layout_narrow(p.nav.layout, out_w);
            /* Fill as the session presents it (oracles_sdl_presentation): Scaling, which Display changes here, in fullscreen. */
            const int fill = p.nav.display.scaling && window && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN);
            band(pause, out_w, out_h);
            p.moving = oracles_pause_paint(pause->draw, renderer, frame, width, height, fill, out_w, out_h, &p.view, &p.nav, now, session);
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
    pause->late_glyphs += oracles_ui_draw_glyphs_rasterised(pause->draw) - p.glyphs_before;
    pause->device = p.nav.device;
    pause->keyboard_used = p.nav.keyboard_used;
    /* Back to the game's own drawing; what the menu left in the queue is not the game's. */
    SDL_SetRenderClipRect(renderer, NULL);
    SDL_SetRenderLogicalPresentation(renderer, logical_w, logical_h, presentation);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_FlushEvents(SDL_EVENT_KEY_DOWN, SDL_EVENT_GAMEPAD_REMAPPED);
    return p.result;
}
