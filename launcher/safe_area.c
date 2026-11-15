#include "safe_area.h"

#include "backends.h"

#include <SDL3/SDL.h>
#ifdef __ANDROID__
#include <jni.h>
#endif

#include <stdio.h>

void oracles_android_activity_text(const char *method, char *out, size_t capacity)
{
    out[0] = 0;
#ifdef __ANDROID__
    JNIEnv *env = SDL_GetAndroidJNIEnv();
    jobject activity = SDL_GetAndroidActivity();
    if (!env || !activity) return;
    jclass class = (*env)->GetObjectClass(env, activity);
    jmethodID id = (*env)->GetStaticMethodID(env, class, method, "()Ljava/lang/String;");
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    jstring text = id ? (*env)->CallStaticObjectMethod(env, class, id) : NULL;
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    if (text) {
        const char *chars = (*env)->GetStringUTFChars(env, text, NULL);
        if (chars) { snprintf(out, capacity, "%s", chars); (*env)->ReleaseStringUTFChars(env, text, chars); }
        (*env)->DeleteLocalRef(env, text);
    }
    (*env)->DeleteLocalRef(env, class);
    (*env)->DeleteLocalRef(env, activity);
#else
    (void)method;
    (void)capacity;
#endif
}

void oracles_android_secure_window(int on)
{
#ifdef __ANDROID__
    JNIEnv *env = SDL_GetAndroidJNIEnv();
    jobject activity = SDL_GetAndroidActivity();
    if (!env || !activity) return;
    jclass class = (*env)->GetObjectClass(env, activity);
    jmethodID id = (*env)->GetStaticMethodID(env, class, "secureWindow", "(Z)V");
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    if (id) (*env)->CallStaticVoidMethod(env, class, id, on ? JNI_TRUE : JNI_FALSE);
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    (*env)->DeleteLocalRef(env, class);
    (*env)->DeleteLocalRef(env, activity);
#else
    (void)on;
#endif
}

void oracles_safe_area_edges(SDL_Window *window, int margin, OraclesTouchEdges *edges)
{
    char text[32];
    *edges = (OraclesTouchEdges){ margin, 1.0f, 0, 0 };
    if (margin == ORACLES_SCREEN_MARGIN_OFF) return;
    oracles_android_activity_text("cutoutInsets", text, sizeof text);
    if (sscanf(text, "%d %d", &edges->inset_top, &edges->inset_bottom) != 2 || edges->inset_top < 0 || edges->inset_bottom < 0)
        edges->inset_top = edges->inset_bottom = 0;
#ifdef __ANDROID__
    (void)window;
    oracles_android_activity_text("displayDensity", text, sizeof text);
    if (sscanf(text, "%f", &edges->density) != 1) edges->density = 1.0f;
#else
    edges->density = SDL_GetDisplayContentScale(window ? SDL_GetDisplayForWindow(window) : SDL_GetPrimaryDisplay());
#endif
    if (!(edges->density > 0.0f)) edges->density = 1.0f;
}

void oracles_safe_area_report(SDL_Window *window, const char *who, const char *when, const OraclesTouchEdges *edges)
{
    if (!window) return;
    SDL_Rect safe = { 0, 0, 0, 0 }, bounds = { 0, 0, 0, 0 }, usable = { 0, 0, 0, 0 };
    int w = 0, h = 0, pw = 0, ph = 0;
    SDL_GetWindowSafeArea(window, &safe);
    SDL_GetWindowSize(window, &w, &h);
    SDL_GetWindowSizeInPixels(window, &pw, &ph);
    const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    SDL_GetDisplayBounds(display, &bounds);
    SDL_GetDisplayUsableBounds(display, &usable);
    const SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    char android[768], margin[96] = "";
    oracles_android_activity_text("safeArea", android, sizeof android);
    if (edges->margin != ORACLES_SCREEN_MARGIN_OFF)
        snprintf(margin, sizeof margin, " (cutout insets %d above, %d below; %.2f pixels a dp)", edges->inset_top, edges->inset_bottom, (double)edges->density);
    const OraclesTouchBox band = oracles_touch_band(pw, ph, edges);
    fprintf(stderr, "oracles: %s %s: safe area %d,%d %dx%d of the window's %dx%d (%dx%d pixels), %s; display %u %d,%d %dx%d, usable %d,%d %dx%d; "
            "screen margin %s%s: band %d,%d %dx%d%s%s\n",
            who, when, safe.x, safe.y, safe.w, safe.h, w, h, pw, ph, (flags & SDL_WINDOW_FULLSCREEN) ? "fullscreen" : "windowed",
            (unsigned)display, bounds.x, bounds.y, bounds.w, bounds.h, usable.x, usable.y, usable.w, usable.h,
            oracles_screen_margin_names[edges->margin >= 0 && edges->margin < ORACLES_SCREEN_MARGINS ? edges->margin : 0], margin,
            (int)band.x, (int)band.y, (int)band.w, (int)band.h, android[0] ? "; " : "", android);
}

int oracles_safe_area_event(SDL_Window *window, const char *who, const SDL_Event *event, OraclesTouchEdges *edges)
{
    if (!window) return 0;
    const char *when = NULL;
    switch (event->type) {
        case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED: when = "safe area changed"; break;
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED: when = "display changed"; break;
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED: when = "display scale changed"; break;
        case SDL_EVENT_WINDOW_RESIZED: when = "resized"; break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED: when = "focus gained"; break;
        /* Android 10 (API 29) gives no inset event when the screen turns by 180 degrees, its cutout changing sides. */
        case SDL_EVENT_DISPLAY_ORIENTATION: when = "display turned"; break;
        default: return 0;
    }
    if (event->type == SDL_EVENT_DISPLAY_ORIENTATION ? event->display.displayID != SDL_GetDisplayForWindow(window)
                                                     : event->window.windowID != SDL_GetWindowID(window)) return 0;
    const int read = event->type == SDL_EVENT_WINDOW_SAFE_AREA_CHANGED || event->type == SDL_EVENT_WINDOW_DISPLAY_CHANGED
                  || event->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED || event->type == SDL_EVENT_DISPLAY_ORIENTATION;
    if (read) oracles_safe_area_edges(window, edges->margin, edges);
    /* A desktop's window resized by hand changes its safe area at each step, the whole window still: nothing to say. */
    if (!oracles_sdl_fullscreen_only()) {
        int w = 0, h = 0;
        SDL_Rect safe = { 0, 0, 0, 0 };
        SDL_GetWindowSize(window, &w, &h);
        SDL_GetWindowSafeArea(window, &safe);
        if (event->type != SDL_EVENT_WINDOW_SAFE_AREA_CHANGED || (safe.x == 0 && safe.y == 0 && safe.w == w && safe.h == h)) return read;
    }
    oracles_safe_area_report(window, who, when, edges);
    return read;
}
