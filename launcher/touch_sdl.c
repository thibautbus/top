#include "touch_sdl.h"

#include "core.h"

#include <SDL3/SDL.h>
#ifdef __ANDROID__
#include <jni.h>
#endif

#include <math.h>
#include <stdio.h>

#define SEGMENTS 40

int oracles_touch_sdl_event(OraclesTouchScreen *screen, SDL_Renderer *renderer, const SDL_Event *event, unsigned *buttons, int *pause)
{
    OraclesTouchFingerKind kind;
    switch (event->type) {
        case SDL_EVENT_FINGER_DOWN: kind = ORACLES_TOUCH_DOWN; break;
        case SDL_EVENT_FINGER_MOTION: kind = ORACLES_TOUCH_MOTION; break;
        case SDL_EVENT_FINGER_UP: case SDL_EVENT_FINGER_CANCELED: kind = ORACLES_TOUCH_UP; break;
        default: return 0;
    }
    int width = 0, height = 0;
    if (!SDL_GetRenderOutputSize(renderer, &width, &height) || width <= 0 || height <= 0) return 0;
    /* A finger's place is a fraction of the window, whose pixels are the output's; the controls lie in the band. */
    const OraclesTouchBox safe = oracles_touch_band(width, height, &screen->edges);
    *buttons = oracles_touch_finger(&screen->touch, safe.w, safe.h, (uint64_t)event->tfinger.fingerID, kind,
                                    event->tfinger.x * (float)width - safe.x, event->tfinger.y * (float)height - safe.y, pause);
    return 1;
}

#ifdef __ANDROID__
/* The ticks are asked of Android on a thread of their own, which the game's wakes: the call into Java, slow on a
 * debuggable build or a cold start, never holds the game.  Ticks asked while one is under way make one more. */
static SDL_Semaphore *tick_wanted;
static SDL_AtomicInt tick_pending, tick_wake_us;

static int SDLCALL tick_thread(void *unused)
{
    (void)unused;
    /* The activity's class and its method looked up at the first tick, kept for the run. */
    jclass activity_class = NULL;
    jmethodID tick = NULL;
    int ticks = 0;
    Uint64 first_ns = 0;
    for (;;) {
        SDL_WaitSemaphore(tick_wanted);
        if (!SDL_SetAtomicInt(&tick_pending, 0)) continue;
        const Uint64 start = SDL_GetTicksNS();
        JNIEnv *env = SDL_GetAndroidJNIEnv();
        if (!env) continue;
        if (!activity_class) {
            jobject activity = SDL_GetAndroidActivity();
            if (!activity) continue;
            jclass class = (*env)->GetObjectClass(env, activity);
            tick = (*env)->GetStaticMethodID(env, class, "touchTick", "()V");
            if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); tick = NULL; }
            activity_class = (jclass)(*env)->NewGlobalRef(env, class);
            (*env)->DeleteLocalRef(env, class);
            (*env)->DeleteLocalRef(env, activity);
        }
        if (!tick) continue;
        (*env)->CallStaticVoidMethod(env, activity_class, tick);
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        /* What a tick costs, said at the second: the first looked the method up. */
        if (ticks == 0) first_ns = SDL_GetTicksNS() - start;
        else if (ticks == 1)
            fprintf(stderr, "oracles: touch vibration: a tick costs the game's thread %d us, asked of Android on its own thread in "
                    "%.0f us (the first, the method looked up, in %.0f us)\n", SDL_GetAtomicInt(&tick_wake_us),
                    (double)(SDL_GetTicksNS() - start) / 1000.0, (double)first_ns / 1000.0);
        if (ticks < 2) ticks++;
    }
    return 0;
}
#endif

void oracles_touch_sdl_tick(void)
{
#ifdef __ANDROID__
    static int started;
    if (!started) {
        started = 1;
        tick_wanted = SDL_CreateSemaphore(0);
        SDL_Thread *thread = tick_wanted ? SDL_CreateThread(tick_thread, "touch vibration", NULL) : NULL;
        if (thread) SDL_DetachThread(thread);
        else { if (tick_wanted) SDL_DestroySemaphore(tick_wanted); tick_wanted = NULL; }
    }
    if (!tick_wanted) return;
    const Uint64 start = SDL_GetTicksNS();
    SDL_SetAtomicInt(&tick_pending, 1);
    SDL_SignalSemaphore(tick_wanted);
    SDL_SetAtomicInt(&tick_wake_us, (int)((SDL_GetTicksNS() - start) / 1000u));
#endif
}

static void disc(SDL_Renderer *renderer, float x, float y, float radius, SDL_FColor colour)
{
    SDL_Vertex vertices[SEGMENTS * 3];
    for (int i = 0; i < SEGMENTS; i++) {
        const float a0 = 6.2831853f * (float)i / SEGMENTS, a1 = 6.2831853f * (float)(i + 1) / SEGMENTS;
        vertices[3 * i] = (SDL_Vertex){ { x, y }, colour, { 0, 0 } };
        vertices[3 * i + 1] = (SDL_Vertex){ { x + radius * cosf(a0), y + radius * sinf(a0) }, colour, { 0, 0 } };
        vertices[3 * i + 2] = (SDL_Vertex){ { x + radius * cosf(a1), y + radius * sinf(a1) }, colour, { 0, 0 } };
    }
    SDL_RenderGeometry(renderer, NULL, vertices, SEGMENTS * 3, NULL, 0);
}

static void box(SDL_Renderer *renderer, float x, float y, float w, float h, SDL_FColor colour)
{
    SDL_SetRenderDrawColorFloat(renderer, colour.r, colour.g, colour.b, colour.a);
    const SDL_FRect rect = { x, y, w, h };
    SDL_RenderFillRect(renderer, &rect);
}

/* A stroke of `width` through the points (x, y) pairs, relative to a centre and in units of a radius. */
static void strokes(SDL_Renderer *renderer, float cx, float cy, float unit, const float *points, int count, float width, SDL_FColor colour)
{
    for (int i = 0; i + 1 < count; i++) {
        const float x0 = cx + points[2 * i] * unit, y0 = cy + points[2 * i + 1] * unit;
        const float x1 = cx + points[2 * i + 2] * unit, y1 = cy + points[2 * i + 3] * unit;
        const float dx = x1 - x0, dy = y1 - y0, length = sqrtf(dx * dx + dy * dy);
        if (length <= 0.0f) continue;
        const float nx = -dy / length * width * 0.5f, ny = dx / length * width * 0.5f;
        const SDL_Vertex quad[4] = { { { x0 + nx, y0 + ny }, colour, { 0, 0 } }, { { x1 + nx, y1 + ny }, colour, { 0, 0 } },
                                     { { x1 - nx, y1 - ny }, colour, { 0, 0 } }, { { x0 - nx, y0 - ny }, colour, { 0, 0 } } };
        static const int order[6] = { 0, 1, 2, 0, 2, 3 };
        SDL_RenderGeometry(renderer, NULL, quad, 4, order, 6);
    }
}

/* The frame where the logical presentation puts it in the band, centred, black beside it. */
static void frame_direct(OraclesTouchScreen *screen, SDL_Renderer *renderer, SDL_Texture *frame, const SDL_FRect *part, const OraclesTouchBox *safe, int fill)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    const int w = part ? (int)part->w : frame->w, h = part ? (int)part->h : frame->h;
    const OraclesTouchBox box = oracles_touch_frame((int)safe->w, (int)safe->h, w, h, fill);
    const SDL_FRect to = { safe->x + box.x, safe->y + box.y, box.w, box.h };
    if (!screen->draw_frame || !screen->draw_frame(screen->draw_frame_opaque, renderer, part, &to)) SDL_RenderTexture(renderer, frame, part, &to);
    if (screen->over_frame) screen->over_frame(screen->over_frame_opaque, renderer, &to);
}

int oracles_touch_sdl_frame(OraclesTouchScreen *screen, SDL_Renderer *renderer, SDL_Texture *frame, const SDL_FRect *part)
{
    if (!screen->touch.shown && !screen->direct && screen->edges.margin == ORACLES_SCREEN_MARGIN_OFF) return 0;
    int width = 0, height = 0;
    const int sized = SDL_GetRenderOutputSize(renderer, &width, &height) && width > 0 && height > 0;
    const OraclesTouchBox safe = oracles_touch_band(width, height, &screen->edges);
    if (!screen->touch.shown && !screen->direct && (!sized || safe.h >= (float)height)) return 0;
    if (!screen->direct) {
        SDL_GetRenderLogicalPresentation(renderer, &screen->logical_w, &screen->logical_h, NULL);
        SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);   /* the screen's pixels */
        screen->direct = 1;
    }
    if (!sized) return 1;
    frame_direct(screen, renderer, frame, part, &safe, screen->fill);
    if (screen->touch.shown) {
        OraclesTouchLayout l;
        oracles_touch_layout(safe.w, safe.h, &l);
        l.dpad.y += safe.y; l.a.y += safe.y; l.b.y += safe.y; l.pause.y += safe.y; l.select.y += safe.y; l.start.y += safe.y;
        const unsigned pressed = oracles_touch_pressed(&screen->touch);
        const SDL_FColor rest = { 1, 1, 1, 0.14f }, arm = { 1, 1, 1, 0.26f }, held = { 1, 1, 1, 0.45f }, mark = { 1, 1, 1, 0.6f };
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        /* The d-pad: a disc and a cross, each arm brighter while its direction is held. */
        const OraclesTouchCircle *d = &l.dpad;
        const float long_side = 0.78f * d->r, short_side = 0.17f * d->r;
        disc(renderer, d->x, d->y, d->r, rest);
        box(renderer, d->x + short_side, d->y - short_side, long_side - short_side, 2 * short_side, pressed & ORACLES_KEY_RIGHT ? held : arm);
        box(renderer, d->x - long_side, d->y - short_side, long_side - short_side, 2 * short_side, pressed & ORACLES_KEY_LEFT ? held : arm);
        box(renderer, d->x - short_side, d->y - long_side, 2 * short_side, long_side - short_side, pressed & ORACLES_KEY_UP ? held : arm);
        box(renderer, d->x - short_side, d->y + short_side, 2 * short_side, long_side - short_side, pressed & ORACLES_KEY_DOWN ? held : arm);
        box(renderer, d->x - short_side, d->y - short_side, 2 * short_side, 2 * short_side, arm);
        /* A and B, their letters drawn in strokes. */
        static const float letter_a[] = { -0.3f, 0.36f, 0.0f, -0.38f, 0.3f, 0.36f };
        static const float bar_a[] = { -0.17f, 0.06f, 0.17f, 0.06f };
        static const float letter_b[] = { -0.2f, 0.0f, 0.1f, 0.0f, 0.2f, -0.09f, 0.2f, -0.27f, 0.1f, -0.36f, -0.22f, -0.36f,
                                          -0.22f, 0.36f, 0.12f, 0.36f, 0.24f, 0.26f, 0.24f, 0.1f, 0.12f, 0.0f };
        const float stroke = 0.09f;
        disc(renderer, l.a.x, l.a.y, l.a.r, pressed & ORACLES_KEY_A ? held : rest);
        strokes(renderer, l.a.x, l.a.y, l.a.r, letter_a, 3, stroke * l.a.r, mark);
        strokes(renderer, l.a.x, l.a.y, l.a.r, bar_a, 2, stroke * l.a.r, mark);
        disc(renderer, l.b.x, l.b.y, l.b.r, pressed & ORACLES_KEY_B ? held : rest);
        strokes(renderer, l.b.x, l.b.y, l.b.r, letter_b, 11, stroke * l.b.r, mark);
        /* Select and Start, as on the Game Boy: two small bars. */
        box(renderer, l.select.x, l.select.y, l.select.w, l.select.h, pressed & ORACLES_KEY_SELECT ? held : arm);
        box(renderer, l.start.x, l.start.y, l.start.w, l.start.h, pressed & ORACLES_KEY_START ? held : arm);
        /* The pause: a disc and two bars. */
        static const float left_bar[] = { -0.22f, -0.35f, -0.22f, 0.35f }, right_bar[] = { 0.22f, -0.35f, 0.22f, 0.35f };
        disc(renderer, l.pause.x, l.pause.y, l.pause.r, rest);
        strokes(renderer, l.pause.x, l.pause.y, l.pause.r, left_bar, 2, 0.2f * l.pause.r, mark);
        strokes(renderer, l.pause.x, l.pause.y, l.pause.r, right_bar, 2, 0.2f * l.pause.r, mark);
    }
    return 1;
}
