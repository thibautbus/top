/* The SDL backend with the launcher's window lent, without a ROM or a display
 * (SDL's dummy video, a software renderer): a start that fails, here because
 * the audio driver named does not exist, gives the window and its renderer
 * back as they were, and SDL keeps running for the home screen; a start that
 * succeeds does the same at its stop.  A file or a text dropped on the window
 * during the game is left to SDL, which owns it, by the backend that polls it.
 * A controller unplugged during a session, and one plugged in, are what the
 * home screen reads afterwards (SDL's virtual controllers); a Nintendo
 * controller's button labelled A is settings.txt's "a", as with SDL 2, and the
 * names settings.txt keeps for buttons are SDL 3's too.  Under Linux, SDL's
 * file dialog through zenity (a fake one on the PATH) answers on the queue: the
 * path chosen, a cancel, and a zenity that cannot open, which is no dialog, not
 * a cancel; it opens in the ROM's folder.  Android's background leaves the sound's device paused and empty, SDL
 * starting it again as the application comes back. */
#include "backends.h"
#include "file_dialog.h"
#include "floating.h"
#include "hotkeys_session.h"
#include "sdl_backend_internal.h"
#include "sdl_filter.h"
#include "text_box_enlarge.h"
#include "touch_controls.h"
#include "ui_page_nav.h"

#include <SDL3/SDL.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

static int failures;

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); failures++; } } while (0)

/* SDL's allocations, to see whether a dropped file's name is freed. */
static const void *watched;
static int watched_freed;
static void *test_malloc(size_t size) { return malloc(size); }
static void *test_calloc(size_t count, size_t size) { return calloc(count, size); }
static void *test_realloc(void *memory, size_t size) { return realloc(memory, size); }
static void test_free(void *memory) { if (memory && memory == watched) watched_freed = 1; free(memory); }

/* An event SDL_EVENT_DROP_FILE or SDL_EVENT_DROP_TEXT polled by the session's backend: it gives the game nothing and
 * leaves the text alone, SDL 3 freeing its own. */
static int dropped_and_left(oracles_host_backend *backend, Uint32 type)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = type;
    char *text = SDL_strdup("C:\\Games\\Oracle of Ages.gbc");
    e.drop.data = text;
    watched = text;
    watched_freed = 0;
    if (!SDL_PushEvent(&e)) return 0;
    oracles_host_event event;
    int has_event = 1, events = 0;
    while (has_event) { if (!backend->poll_event(backend->opaque, &event, &has_event)) return 0; events += has_event; }
    const int left = !watched_freed && events == 0;
    watched = NULL;
    SDL_free(text);
    return left;
}

/* A virtual gamepad, `vendor`:`product` (0 for none), whose buttons the test presses. */
static SDL_JoystickID attach_gamepad(Uint16 vendor, Uint16 product)
{
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.vendor_id = vendor;
    desc.product_id = product;
    desc.naxes = 6;
    desc.nbuttons = 15;
    return SDL_AttachVirtualJoystick(&desc);
}

/* The game's buttons the backend's events press, until none is left. */
static unsigned drained(oracles_host_backend *backend)
{
    unsigned mask = 0;
    oracles_host_event event;
    int has_event = 1;
    while (has_event && backend->poll_event(backend->opaque, &event, &has_event))
        if (has_event && event.type == ORACLES_HOST_EVENT_BUTTON && event.pressed) mask |= event.button;
    return mask;
}

/* The game's buttons `button` of the virtual gamepad presses. */
static unsigned pressed_by(oracles_host_backend *backend, SDL_JoystickID pad, SDL_GamepadButton button)
{
    SDL_Joystick *joystick = SDL_GetJoystickFromID(pad);
    SDL_SetJoystickVirtualButton(joystick, (int)button, true);
    SDL_UpdateJoysticks();
    const unsigned mask = drained(backend);
    SDL_SetJoystickVirtualButton(joystick, (int)button, false);
    SDL_UpdateJoysticks();
    drained(backend);
    return mask;
}

/* The events the backend gives the host until none is left: the game's buttons pressed and released, and pauses. */
static void touch_drain(oracles_host_backend *backend, unsigned *pressed, unsigned *released, int *pauses)
{
    *pressed = *released = 0;
    *pauses = 0;
    oracles_host_event event;
    int has_event = 1;
    while (has_event && backend->poll_event(backend->opaque, &event, &has_event)) {
        if (!has_event) break;
        if (event.type == ORACLES_HOST_EVENT_BUTTON) { if (event.pressed) *pressed |= event.button; else *released |= event.button; }
        if (event.type == ORACLES_HOST_EVENT_PAUSE) (*pauses)++;
    }
}

static int output_w = 1, output_h = 1;   /* the lent renderer's output, which fingers are fractions of */

/* A finger's event at (x, y) in the output's pixels, at `ns` (0: now). */
static void finger_at(SDL_EventType type, SDL_FingerID id, float x, float y, Uint64 ns)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = type;
    e.common.timestamp = ns;
    e.tfinger.touchID = 1;
    e.tfinger.fingerID = id;
    e.tfinger.x = x / (float)output_w;
    e.tfinger.y = y / (float)output_h;
    e.tfinger.pressure = 1.0f;
    SDL_PushEvent(&e);
}

static void finger(SDL_EventType type, SDL_FingerID id, float x, float y)
{
    finger_at(type, id, x, y, 0);
}

static void key(SDL_Keycode code)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = code;
    e.key.down = true;
    SDL_PushEvent(&e);
}

/* The game's menus enlarged: a frame of the view's 480x270 that names a part to show alone (the framed core, 160x144)
 * takes that part's own logical size, so its own largest whole scale; the next whole frame takes the surface's again. */
static void enlarged_menus(SDL_Window *window, SDL_Renderer *renderer)
{
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    oracles_host_backend backend;
    if (!oracles_sdl_backend_init(&backend, &options)) { CHECK(0); return; }
    static uint32_t pixels[480 * 270];
    if (backend.start(backend.opaque, 480, 270, 48000, 0)) {
        int w = 0, h = 0;
        oracles_host_video_frame frame = { pixels, 480, 270, 480 * sizeof(uint32_t), 160, 63, 160, 144, NULL, 0, 0, 0, 0, 0, 0, 0 };
        CHECK(backend.present_frame(backend.opaque, &frame));
        CHECK(SDL_GetRenderLogicalPresentation(renderer, &w, &h, NULL) && w == 160 && h == 144);
        frame.crop_w = frame.crop_h = 0;
        CHECK(backend.present_frame(backend.opaque, &frame));
        CHECK(SDL_GetRenderLogicalPresentation(renderer, &w, &h, NULL) && w == 480 && h == 270);
    } else CHECK(0);
    backend.stop(backend.opaque);
    oracles_sdl_backend_release(&backend);
}

/* The touch controls in a session (asked here; on Android always): a finger presses and releases the game's buttons,
 * a cancelled one too; Back (AC_BACK) leaves them shown, a key of the game hides them and lets the d-pad go; the pause
 * asks for the pause menu; a finger held through a pause presses again as it moves.  And the frame they draw directly
 * goes where SDL's logical presentation puts it, integer-scaled, and letterboxed for Scaling's Fill. */
static void touches(SDL_Window *window, SDL_Renderer *renderer)
{
    static const int frames[4][2] = { { 160, 144 }, { 256, 144 }, { 480, 270 }, { 320, 180 } };
    CHECK(SDL_GetRenderOutputSize(renderer, &output_w, &output_h) && output_w > 0 && output_h > 0);
    for (int fill = 0; fill < 2; fill++)
        for (int i = 0; i < 4; i++) {
            SDL_FRect sdl_rect;
            SDL_SetRenderLogicalPresentation(renderer, frames[i][0], frames[i][1],
                                             fill ? SDL_LOGICAL_PRESENTATION_LETTERBOX : SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
            CHECK(SDL_GetRenderLogicalPresentationRect(renderer, &sdl_rect));
            const OraclesTouchBox ours = oracles_touch_frame(output_w, output_h, frames[i][0], frames[i][1], fill);
            CHECK(ours.x == sdl_rect.x && ours.y == sdl_rect.y && ours.w == sdl_rect.w && ours.h == sdl_rect.h);
        }
    SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);

    OraclesTouchLayout l;
    oracles_touch_layout((float)output_w, (float)output_h, &l);
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    options.pause_menu = 1;
    options.touch_controls = 1;
    oracles_host_backend backend;
    if (!oracles_sdl_backend_init(&backend, &options)) { CHECK(0); return; }
    unsigned pressed, released;
    int pauses;
    if (backend.start(backend.opaque, 160, 144, 48000, 0)) {
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        finger(SDL_EVENT_FINGER_DOWN, 1, l.a.x, l.a.y);
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(pressed == ORACLES_KEY_A && released == 0);
        finger(SDL_EVENT_FINGER_CANCELED, 1, l.a.x, l.a.y);
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(pressed == 0 && released == ORACLES_KEY_A);
        finger(SDL_EVENT_FINGER_DOWN, 2, l.dpad.x + 0.7f * l.dpad.r, l.dpad.y);
        key(SDLK_AC_BACK);
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(pressed == ORACLES_KEY_RIGHT && released == 0);
        key(SDLK_X);   /* the game's A, by default */
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(released == ORACLES_KEY_RIGHT && pressed == ORACLES_KEY_A);
        finger(SDL_EVENT_FINGER_UP, 2, l.dpad.x, l.dpad.y);
        finger(SDL_EVENT_FINGER_DOWN, 3, l.pause.x, l.pause.y);
        finger(SDL_EVENT_FINGER_UP, 3, l.pause.x, l.pause.y);
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(pauses == 1 && pressed == 0);
        finger(SDL_EVENT_FINGER_DOWN, 4, l.b.x, l.b.y);
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(pressed == ORACLES_KEY_B);
        oracles_sdl_backend_hold(&backend, 1);   /* the pause menu takes the fingers; the host lets its keys go */
        oracles_sdl_backend_hold(&backend, 0);
        finger(SDL_EVENT_FINGER_MOTION, 4, l.b.x + 1, l.b.y);
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(pressed == ORACLES_KEY_B && released == 0);
        finger(SDL_EVENT_FINGER_UP, 4, l.b.x, l.b.y);
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(released == ORACLES_KEY_B);
        CHECK(oracles_sdl_backend_touch_ticks(&backend) == 0);   /* touch_vibration=off */
    } else {
        CHECK(0);
    }
    backend.stop(backend.opaque);
    oracles_sdl_backend_release(&backend);
}

/* touch_vibration=on: a tick for a finger going down on a button, none for one going up; fingers pressing within 40
 * ms tick once, and the same buttons pressed later tick again. */
static void vibration(SDL_Window *window, SDL_Renderer *renderer)
{
    OraclesTouchLayout l;
    oracles_touch_layout((float)output_w, (float)output_h, &l);
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    options.touch_controls = 1;
    options.touch_vibration = 1;
    oracles_host_backend backend;
    if (!oracles_sdl_backend_init(&backend, &options)) { CHECK(0); return; }
    unsigned pressed, released;
    int pauses;
    if (backend.start(backend.opaque, 160, 144, 48000, 0)) {
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        const Uint64 t0 = 1000000000u, ms = 1000000u;
        finger_at(SDL_EVENT_FINGER_DOWN, 1, l.a.x, l.a.y, t0);
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(pressed == ORACLES_KEY_A && oracles_sdl_backend_touch_ticks(&backend) == 1);
        finger_at(SDL_EVENT_FINGER_DOWN, 2, l.b.x, l.b.y, t0 + 10 * ms);
        finger_at(SDL_EVENT_FINGER_UP, 1, l.a.x, l.a.y, t0 + 20 * ms);
        finger_at(SDL_EVENT_FINGER_UP, 2, l.b.x, l.b.y, t0 + 30 * ms);
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(pressed == ORACLES_KEY_B && oracles_sdl_backend_touch_ticks(&backend) == 1);
        finger_at(SDL_EVENT_FINGER_DOWN, 3, l.b.x, l.b.y, t0 + 60 * ms);
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(pressed == ORACLES_KEY_B && oracles_sdl_backend_touch_ticks(&backend) == 2);
        finger_at(SDL_EVENT_FINGER_UP, 3, l.b.x, l.b.y, t0 + 200 * ms);
        touch_drain(&backend, &pressed, &released, &pauses);
        CHECK(released == ORACLES_KEY_B && oracles_sdl_backend_touch_ticks(&backend) == 2);
    } else {
        CHECK(0);
    }
    backend.stop(backend.opaque);
    oracles_sdl_backend_release(&backend);
}

/* Scaling: Sharp, and Fill in a window, present the whole scales sampled to the nearest pixel; Fill in fullscreen the
 * largest size in the image's proportions, sampled for pixel art.  A session on Fill follows the window into
 * fullscreen and out, and a change from the pause takes the next frame, after the pause set back the presentation it
 * had lifted. */
static void scaling(SDL_Window *window, SDL_Renderer *renderer)
{
    int presentation, scale_mode;
    oracles_sdl_presentation(0, 0, &presentation, &scale_mode);
    CHECK(presentation == SDL_LOGICAL_PRESENTATION_INTEGER_SCALE && scale_mode == SDL_SCALEMODE_NEAREST);
    oracles_sdl_presentation(1, 0, &presentation, &scale_mode);
    CHECK(presentation == SDL_LOGICAL_PRESENTATION_INTEGER_SCALE && scale_mode == SDL_SCALEMODE_NEAREST);
    oracles_sdl_presentation(0, 1, &presentation, &scale_mode);
    CHECK(presentation == SDL_LOGICAL_PRESENTATION_INTEGER_SCALE && scale_mode == SDL_SCALEMODE_NEAREST);
    oracles_sdl_presentation(1, 1, &presentation, &scale_mode);
    CHECK(presentation == SDL_LOGICAL_PRESENTATION_LETTERBOX && scale_mode == SDL_SCALEMODE_PIXELART);

    oracles_sdl_options options = { 0 };
    options.scale = 2;
    options.window = window;
    options.renderer = renderer;
    options.fill = 1;
    oracles_host_backend backend;
    if (!oracles_sdl_backend_init(&backend, &options)) { CHECK(0); return; }
    static uint32_t pixels[480 * 270];
    if (backend.start(backend.opaque, 480, 270, 48000, 0)) {
        SDL_Window *w;
        SDL_Renderer *r;
        SDL_Texture *frame;
        int width, height, fill = 0, filled = 1;
        oracles_sdl_backend_pause_view(&backend, &w, &r, &frame, &width, &height);
        const oracles_host_video_frame image = { pixels, 480, 270, 480 * sizeof(uint32_t), 0, 0, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0 };
        SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
        SDL_ScaleMode sampling = SDL_SCALEMODE_INVALID;
        /* In a window: whole scales. */
        CHECK(backend.present_frame(backend.opaque, &image));
        CHECK(SDL_GetRenderLogicalPresentation(renderer, NULL, NULL, &mode) && mode == SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
        CHECK(SDL_GetTextureScaleMode(frame, &sampling) && sampling == SDL_SCALEMODE_NEAREST);
        oracles_sdl_backend_scaling(&backend, &fill, &filled);
        CHECK(fill == 1 && filled == 0);
        /* Fullscreen (F11): Fill. */
        SDL_SetWindowFullscreen(window, true);
        SDL_SyncWindow(window);
        CHECK(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN);
        CHECK(backend.present_frame(backend.opaque, &image));
        CHECK(SDL_GetRenderLogicalPresentation(renderer, NULL, NULL, &mode) && mode == SDL_LOGICAL_PRESENTATION_LETTERBOX);
        CHECK(SDL_GetTextureScaleMode(frame, &sampling) && sampling == SDL_SCALEMODE_PIXELART);
        oracles_sdl_backend_scaling(&backend, &fill, &filled);
        CHECK(fill == 1 && filled == 1);
        /* Sharp from the pause: the frame's sampling at once; the presentation the pause sets back is replaced at the
         * next frame. */
        SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        oracles_sdl_backend_set_fill(&backend, 0);
        CHECK(SDL_GetTextureScaleMode(frame, &sampling) && sampling == SDL_SCALEMODE_NEAREST);
        SDL_SetRenderLogicalPresentation(renderer, 480, 270, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        CHECK(backend.present_frame(backend.opaque, &image));
        CHECK(SDL_GetRenderLogicalPresentation(renderer, NULL, NULL, &mode) && mode == SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
        oracles_sdl_backend_scaling(&backend, &fill, &filled);
        CHECK(fill == 0 && filled == 0);
        /* Fill again, then out of fullscreen: whole scales. */
        oracles_sdl_backend_set_fill(&backend, 1);
        CHECK(backend.present_frame(backend.opaque, &image));
        CHECK(SDL_GetRenderLogicalPresentation(renderer, NULL, NULL, &mode) && mode == SDL_LOGICAL_PRESENTATION_LETTERBOX);
        SDL_SetWindowFullscreen(window, false);
        SDL_SyncWindow(window);
        CHECK(backend.present_frame(backend.opaque, &image));
        CHECK(SDL_GetRenderLogicalPresentation(renderer, NULL, NULL, &mode) && mode == SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
        CHECK(SDL_GetTextureScaleMode(frame, &sampling) && sampling == SDL_SCALEMODE_NEAREST);
    } else CHECK(0);
    backend.stop(backend.opaque);
    oracles_sdl_backend_release(&backend);
    SDL_SetWindowFullscreen(window, false);
    SDL_SyncWindow(window);
}

/* The pixel at (x, y) of the frame last drawn, opaque black 0xff000000. */
static Uint32 pixel_at(SDL_Renderer *renderer, int x, int y)
{
    const SDL_Rect one = { x, y, 1, 1 };
    SDL_Surface *read = SDL_RenderReadPixels(renderer, &one);
    Uint32 value = 0;
    if (read) {
        Uint8 r = 0, g = 0, b = 0, a = 0;
        SDL_ReadSurfacePixel(read, 0, 0, &r, &g, &b, &a);
        value = (Uint32)a << 24 | (Uint32)r << 16 | (Uint32)g << 8 | b;
        SDL_DestroySurface(read);
    }
    return value;
}

/* The sub-pixel camera's frame, 480x270 at a whole scale k of the window: the band moved half a pixel left (k / 2
 * pixels of the screen), its last column filling the sliver at the right; the status bar unmoved; the patch moved
 * with the band over Link's whole place in it, and the layer drawn at his own place over it, cut where it would cover
 * the status bar; a fixed rectangle unmoved over all.  The frame keeps him at his whole pixels: the pause shows him.
 * At the window's own size (k 1), the frame as it is. */
static void subpixel_band(SDL_Window *window, SDL_Renderer *renderer)
{
    SDL_SetWindowSize(window, 1280, 720);
    SDL_SyncWindow(window);
    oracles_sdl_options options = { 0 };
    options.scale = 2;
    options.window = window;
    options.renderer = renderer;
    oracles_host_backend backend;
    if (!oracles_sdl_backend_init(&backend, &options)) { CHECK(0); return; }
    static uint32_t pixels[480 * 270];
    static uint32_t layer[64 * 64], patch[64 * 64];
    for (unsigned i = 0; i < 480u * 270u; i++) pixels[i] = 0xff000000u;
    for (unsigned y = 16; y < 270; y++) { pixels[y * 480 + 10] = 0xffffffffu; pixels[y * 480 + 479] = 0xff0000ffu; }
    for (unsigned y = 0; y < 16; y++) pixels[y * 480 + 10] = 0xffff0000u;
    pixels[40 * 480 + 20] = pixels[41 * 480 + 20] = 0xff00ff00u;   /* Link, at his whole pixels */
    layer[0] = layer[64] = 0xff00ff00u;                             /* at (20, 40) and (20, 41) */
    patch[0] = patch[64] = 0xff000000u;                             /* the band without him there */
    if (backend.start(backend.opaque, 480, 270, 48000, 0)) {
        /* The band half a pixel left, Link at his own place: his fraction the band's, his move 0 against it. */
        oracles_host_video_subpixel sub = { 128, 0, 16, 254, layer, 20, 40, 1, 2, 64, 0, 0, patch, 1, { { 10, 90, 1, 20 } }, NULL, 0, { { 0, 0, 0, 0 } }, 0, { 0 } };
        const oracles_host_video_frame image = { pixels, 480, 270, 480 * sizeof(uint32_t), 0, 0, 0, 0, &sub, 0, 0, 0, 0, 0, 0, 0 };
        CHECK(backend.present_frame(backend.opaque, &image));
        SDL_FRect area;
        CHECK(SDL_GetRenderLogicalPresentationRect(renderer, &area));
        const int k = (int)(area.w / 480.0f), x0 = (int)area.x, y0 = (int)area.y, half = k / 2;
        CHECK(k == 2);
        /* The band's column 10, half a pixel left; the bar's, where it was. */
        CHECK(pixel_at(renderer, x0 + 10 * k - half, y0 + 120 * k) == 0xffffffffu && pixel_at(renderer, x0 + 10 * k + k - half - 1, y0 + 120 * k) == 0xffffffffu);
        CHECK(pixel_at(renderer, x0 + 10 * k + k - half, y0 + 120 * k) == 0xff000000u && pixel_at(renderer, x0 + 10 * k - half - 1, y0 + 120 * k) == 0xff000000u);
        CHECK(pixel_at(renderer, x0 + 10 * k, y0 + 10 * k) == 0xffff0000u && pixel_at(renderer, x0 + 10 * k + k - 1, y0 + 10 * k) == 0xffff0000u);
        CHECK(pixel_at(renderer, x0 + 10 * k - 1, y0 + 10 * k) == 0xff000000u);
        /* The fixed rectangle, column 10 of lines 90 to 109, unmoved. */
        CHECK(pixel_at(renderer, x0 + 10 * k, y0 + 100 * k) == 0xffffffffu && pixel_at(renderer, x0 + 10 * k + k - 1, y0 + 100 * k) == 0xffffffffu);
        /* The last column over the sliver it uncovers. */
        CHECK(pixel_at(renderer, x0 + 480 * k - 1, y0 + 100 * k) == 0xff0000ffu && pixel_at(renderer, x0 + 479 * k - half, y0 + 100 * k) == 0xff0000ffu);
        /* Link unmoved, the patch hiding his whole place in the band moved, nothing of him twice. */
        CHECK(pixel_at(renderer, x0 + 20 * k, y0 + 40 * k) == 0xff00ff00u && pixel_at(renderer, x0 + 20 * k + k - 1, y0 + 41 * k + k - 1) == 0xff00ff00u);
        CHECK(pixel_at(renderer, x0 + 20 * k - half, y0 + 40 * k) == 0xff000000u);
        /* His fraction 0 against the band's half: drawn with it, half a pixel left. */
        sub.layer_fx = -128;
        CHECK(backend.present_frame(backend.opaque, &image));
        CHECK(pixel_at(renderer, x0 + 20 * k - half, y0 + 40 * k) == 0xff00ff00u && pixel_at(renderer, x0 + 20 * k + k - half, y0 + 40 * k) == 0xff000000u);
        /* His fraction 255: rounded down, half a pixel right of the band's drawing of him, never a whole one. */
        sub.layer_fx = 127;
        CHECK(backend.present_frame(backend.opaque, &image));
        CHECK(pixel_at(renderer, x0 + 20 * k, y0 + 40 * k) == 0xff00ff00u && pixel_at(renderer, x0 + 20 * k + k, y0 + 40 * k) == 0xff000000u);
        CHECK(pixel_at(renderer, x0 + 20 * k - half, y0 + 40 * k) == 0xff000000u);
        /* The status bar's floating panels over it all, unmoved: a pixel opaque, its whole screen pixels, and one half
         * transparent over the band's black, half its colour. */
        static uint32_t panels[480 * 270];
        panels[60 * 480 + 30] = 0xffff0000u;
        panels[60 * 480 + 31] = 0x80808080u;
        sub.layer_fx = 0;
        sub.panels = panels;
        sub.panels_count = 1;
        sub.panels_rects[0][0] = 30; sub.panels_rects[0][1] = 60; sub.panels_rects[0][2] = 2; sub.panels_rects[0][3] = 1;
        CHECK(backend.present_frame(backend.opaque, &image));
        CHECK(pixel_at(renderer, x0 + 30 * k, y0 + 60 * k) == 0xffff0000u && pixel_at(renderer, x0 + 30 * k + k - 1, y0 + 60 * k + k - 1) == 0xffff0000u);
        CHECK(pixel_at(renderer, x0 + 31 * k, y0 + 60 * k) == 0xff808080u && pixel_at(renderer, x0 + 30 * k - 1, y0 + 60 * k) == 0xff000000u);
        sub.panels = NULL;
        CHECK(backend.present_frame(backend.opaque, &image));
        CHECK(pixel_at(renderer, x0 + 30 * k, y0 + 60 * k) == 0xff000000u);
        sub.layer_fx = 127;
        CHECK(backend.present_frame(backend.opaque, &image));
        /* The frame the pause keeps (its texture) has him at his whole place. */
        SDL_Window *pw; SDL_Renderer *pr; SDL_Texture *kept; int kw = 0, kh = 0;
        oracles_sdl_backend_pause_view(&backend, &pw, &pr, &kept, &kw, &kh);
        SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        SDL_RenderClear(renderer);
        const SDL_FRect whole = { 0.0f, 0.0f, 480.0f, 270.0f };
        CHECK(SDL_RenderTexture(renderer, kept, &whole, &whole));
        CHECK(pixel_at(renderer, 20, 40) == 0xff00ff00u && pixel_at(renderer, 20, 41) == 0xff00ff00u && pixel_at(renderer, 10, 100) == 0xffffffffu);
        SDL_SetRenderLogicalPresentation(renderer, 480, 270, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
        const oracles_host_video_subpixel high = { 0, 0, 16, 254, layer, 20, 15, 1, 2, 64, 0, 0, patch, 0, { { 0 } }, NULL, 0, { { 0 } }, 0, { 0 } };
        const oracles_host_video_frame cut = { pixels, 480, 270, 480 * sizeof(uint32_t), 0, 0, 0, 0, &high, 0, 0, 0, 0, 0, 0, 0 };
        CHECK(backend.present_frame(backend.opaque, &cut));
        CHECK(pixel_at(renderer, x0 + 20 * k, y0 + 15 * k) == 0xff000000u && pixel_at(renderer, x0 + 20 * k, y0 + 16 * k) == 0xff00ff00u);
        /* Sampled smoothly (Fill's pixel art), the layer's and the patch's edges blend with their transparent pixels:
         * on a white band, white Link and white patch, nothing darker than white around him. */
        static uint32_t white[480 * 270], white_layer[64 * 64], white_patch[64 * 64];
        for (unsigned i = 0; i < 480u * 270u; i++) white[i] = 0xffffffffu;
        for (unsigned y = 0; y < 3u; y++) {
            white_layer[y * 64] = white_layer[y * 64 + 2] = white_layer[y * 64 + 4] = 0xffffffffu;   /* his pixels, holes between */
            white_patch[y * 64] = white_patch[y * 64 + 2] = white_patch[y * 64 + 4] = 0xffffffffu;
        }
        SDL_SetTextureScaleMode(kept, SDL_SCALEMODE_LINEAR);
        const oracles_host_video_subpixel smooth = { 128, 64, 16, 254, white_layer, 20, 40, 5, 3, 64, -64, 32, white_patch, 0, { { 0 } }, NULL, 0, { { 0 } }, 0, { 0 } };
        const oracles_host_video_frame bright = { white, 480, 270, 480 * sizeof(uint32_t), 0, 0, 0, 0, &smooth, 0, 0, 0, 0, 0, 0, 0 };
        CHECK(backend.present_frame(backend.opaque, &bright));
        int darker = 0;
        for (int y = y0 + 38 * k; y < y0 + 44 * k; y++)
            for (int x = x0 + 18 * k; x < x0 + 24 * k; x++) darker += pixel_at(renderer, x, y) != 0xffffffffu;
        CHECK(darker == 0);
        SDL_SetTextureScaleMode(kept, SDL_SCALEMODE_NEAREST);
        /* At the window's own size, the frame as it is: column 10 and Link where the frame has them. */
        SDL_SetWindowSize(window, 640, 360);
        SDL_SyncWindow(window);
        sub.layer_fx = 64;
        CHECK(backend.present_frame(backend.opaque, &image));
        CHECK(SDL_GetRenderLogicalPresentationRect(renderer, &area) && (int)(area.w / 480.0f) == 1);
        const int x1 = (int)area.x, y1 = (int)area.y;
        CHECK(pixel_at(renderer, x1 + 10, y1 + 120) == 0xffffffffu && pixel_at(renderer, x1 + 9, y1 + 120) == 0xff000000u);
        CHECK(pixel_at(renderer, x1 + 20, y1 + 40) == 0xff00ff00u && pixel_at(renderer, x1 + 21, y1 + 40) == 0xff000000u);
        SDL_SetWindowSize(window, 1280, 720);
        SDL_SyncWindow(window);
    } else CHECK(0);
    backend.stop(backend.opaque);
    oracles_sdl_backend_release(&backend);
}

/* The screen margin, in the lent window sized for 160x144 at five times (800x720): off presents through SDL's logical
 * presentation, as before it, the frame filling the window; small draws the frame itself in the band 24 dp clear at
 * the top and at the bottom (a dp a pixel on the dummy display), black outside, at the band's whole scale, four times,
 * the touch controls laid out in the band; off again from the pause takes the next frame, five times again. */
static void margins(SDL_Window *window, SDL_Renderer *renderer)
{
    static uint32_t pixels[160 * 144];
    for (unsigned i = 0; i < 160u * 144u; i++) pixels[i] = 0xffffffffu;
    const oracles_host_video_frame image = { pixels, 160, 144, 160 * sizeof(uint32_t), 0, 0, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0 };
    static uint32_t with_link[160 * 144], layer[64 * 64], patch[64 * 64];
    for (unsigned i = 0; i < 160u * 144u; i++) with_link[i] = 0xffffffffu;
    for (unsigned y = 40; y < 42; y++) for (unsigned x = 20; x < 22; x++) with_link[y * 160 + x] = 0xff00ff00u;
    for (unsigned y = 16; y < 144; y++) with_link[y * 160 + 10] = 0xff0000ffu;   /* a column of the band */
    for (unsigned y = 0; y < 2; y++) for (unsigned x = 0; x < 2; x++) { layer[y * 64 + x] = 0xff00ff00u; patch[y * 64 + x] = 0xffffffffu; }
    const oracles_host_video_subpixel sub = { 128, 0, 16, 128, layer, 20, 40, 2, 2, 64, -128, 0, patch, 0, { { 0 } }, NULL, 0, { { 0 } }, 0, { 0 } };
    const oracles_host_video_frame moved = { with_link, 160, 144, 160 * sizeof(uint32_t), 0, 0, 0, 0, &sub, 0, 0, 0, 0, 0, 0, 0 };
    for (int margin = ORACLES_SCREEN_MARGIN_OFF; margin <= ORACLES_SCREEN_MARGIN_SMALL; margin += ORACLES_SCREEN_MARGIN_SMALL) {
        oracles_sdl_options options = { 0 };
        options.scale = 5;
        options.window = window;
        options.renderer = renderer;
        options.pause_menu = 1;
        options.touch_controls = 1;
        options.screen_margin = margin;
        oracles_host_backend backend;
        if (!oracles_sdl_backend_init(&backend, &options)) { CHECK(0); return; }
        CHECK(oracles_sdl_backend_whole_scale(&backend) == 0u);   /* before the session starts */
        if (backend.start(backend.opaque, 160, 144, 48000, 0)) {
            SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
            CHECK(SDL_GetRenderOutputSize(renderer, &output_w, &output_h) && output_w == 800 && output_h == 720);
            CHECK(backend.present_frame(backend.opaque, &image));
            CHECK(SDL_GetRenderLogicalPresentation(renderer, NULL, NULL, &mode));
            /* The whole scale Display's Camera follows: five times, four in the margin's band (672 lines of 720). */
            CHECK(oracles_sdl_backend_whole_scale(&backend) == (margin == ORACLES_SCREEN_MARGIN_OFF ? 5u : 4u));
            if (margin == ORACLES_SCREEN_MARGIN_OFF) {
                CHECK(mode == SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
                CHECK(pixel_at(renderer, 400, 0) == 0xffffffffu && pixel_at(renderer, 400, 719) == 0xffffffffu);
            } else {
                /* The band 24..696: 640x576 at 80,72. */
                CHECK(mode == SDL_LOGICAL_PRESENTATION_DISABLED);
                CHECK(pixel_at(renderer, 400, 0) == 0xff000000u && pixel_at(renderer, 400, 71) == 0xff000000u);
                CHECK(pixel_at(renderer, 400, 72) == 0xffffffffu && pixel_at(renderer, 400, 647) == 0xffffffffu);
                CHECK(pixel_at(renderer, 400, 648) == 0xff000000u && pixel_at(renderer, 79, 300) == 0xff000000u
                      && pixel_at(renderer, 80, 300) == 0xffffffffu);
                /* The controls in the band: A pressed where the band's layout puts it. */
                const OraclesTouchEdges edges = { ORACLES_SCREEN_MARGIN_SMALL, 1.0f, 0, 0 };
                const OraclesTouchBox band = oracles_touch_band(output_w, output_h, &edges);
                CHECK(band.y == 24 && band.h == 672);
                OraclesTouchLayout l;
                oracles_touch_layout(band.w, band.h, &l);
                unsigned pressed, released;
                int pauses;
                SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
                finger(SDL_EVENT_FINGER_DOWN, 1, l.a.x, band.y + l.a.y);
                touch_drain(&backend, &pressed, &released, &pauses);
                CHECK(pressed == ORACLES_KEY_A);
                finger(SDL_EVENT_FINGER_UP, 1, l.a.x, band.y + l.a.y);
                touch_drain(&backend, &pressed, &released, &pauses);
                CHECK(released == ORACLES_KEY_A);
                /* A frame of the sub-pixel camera the band draws itself: moved half a pixel left at its four times (2
                 * pixels of the screen), Link with it, black kept beside it. */
                CHECK(backend.present_frame(backend.opaque, &moved));
                CHECK(pixel_at(renderer, 80 + 20 * 4 - 2, 72 + 40 * 4 + 1) == 0xff00ff00u && pixel_at(renderer, 80 + 22 * 4 - 2, 72 + 40 * 4 + 1) == 0xffffffffu);
                CHECK(pixel_at(renderer, 80 + 10 * 4 - 2, 300) == 0xff0000ffu && pixel_at(renderer, 80 + 10 * 4 + 1, 300) == 0xff0000ffu);
                CHECK(pixel_at(renderer, 80 + 10 * 4 + 2, 300) == 0xffffffffu && pixel_at(renderer, 80 + 10 * 4 - 3, 300) == 0xffffffffu);
                CHECK(pixel_at(renderer, 79, 300) == 0xff000000u && pixel_at(renderer, 80 + 640, 300) == 0xff000000u && pixel_at(renderer, 400, 71) == 0xff000000u);
                /* Off from the pause: the whole window's five times at the next frame, the controls over it. */
                oracles_sdl_backend_set_screen_margin(&backend, ORACLES_SCREEN_MARGIN_OFF);
                CHECK(backend.present_frame(backend.opaque, &image));
                CHECK(pixel_at(renderer, 400, 0) == 0xffffffffu && pixel_at(renderer, 400, 719) == 0xffffffffu);
                /* And through the controls' own drawing, a finger having touched: the band moved there too, half a
                 * pixel at five times rounded to 3 pixels of the screen, Link with it. */
                CHECK(backend.present_frame(backend.opaque, &moved));
                CHECK(pixel_at(renderer, 20 * 5 - 3, 40 * 5 + 1) == 0xff00ff00u && pixel_at(renderer, 22 * 5 - 3, 40 * 5 + 1) == 0xffffffffu);
                CHECK(pixel_at(renderer, 10 * 5 - 3, 300) == 0xff0000ffu && pixel_at(renderer, 10 * 5 + 1, 300) == 0xff0000ffu);
                CHECK(pixel_at(renderer, 10 * 5 + 2, 300) == 0xffffffffu && pixel_at(renderer, 10 * 5 - 4, 300) == 0xffffffffu);
                /* A frame without the move, through the same drawing: as it is. */
                CHECK(backend.present_frame(backend.opaque, &image));
                CHECK(pixel_at(renderer, 10 * 5 - 3, 300) == 0xffffffffu);
            }
        } else CHECK(0);
        backend.stop(backend.opaque);
        oracles_sdl_backend_release(&backend);
    }
}

/* The pixels of the picture drawn last, and how many differ between two such. */
static SDL_Surface *picture(SDL_Renderer *renderer)
{
    SDL_Surface *read = SDL_RenderReadPixels(renderer, NULL);
    SDL_Surface *argb = read ? SDL_ConvertSurface(read, SDL_PIXELFORMAT_ARGB8888) : NULL;
    SDL_DestroySurface(read);
    return argb;
}

static long differing(const SDL_Surface *a, const SDL_Surface *b)
{
    if (!a || !b || a->w != b->w || a->h != b->h) return -1;
    long count = 0;
    for (int y = 0; y < a->h; y++) {
        const Uint32 *ra = (const Uint32 *)((const Uint8 *)a->pixels + (size_t)y * (size_t)a->pitch);
        const Uint32 *rb = (const Uint32 *)((const Uint8 *)b->pixels + (size_t)y * (size_t)b->pitch);
        for (int x = 0; x < a->w; x++) count += ra[x] != rb[x];
    }
    return count;
}

/* The pause shows the game's last frame as it was shown: the band at its fraction of a pixel (here half a pixel left
 * and three quarters up, the sliver it uncovers filled), the patch and Link's layer at their places, the status bar
 * and a fixed rectangle unmoved, the text box Text size Large enlarges over them; drawn where the pause puts the image,
 * in the window's pixels, it is the frame's picture pixel for pixel, at 4x in a 1920x1080 window, at 2x in a 960x540
 * one, and with Fill in fullscreen on 1280x720 (8/3), though the view's layer has changed since; so is it without the
 * sub-pixel camera, the box enlarged over the frame as it is.  With the screen margin, whose drawing moves the band
 * too, the same in the margin's band. */
static void pause_image(SDL_Window *window, SDL_Renderer *renderer)
{
    static uint32_t pixels[480 * 270], layer[64 * 64], patch[64 * 64];
    for (unsigned y = 0; y < 270; y++)
        for (unsigned x = 0; x < 480; x++) pixels[y * 480 + x] = y < 16 ? 0xffff0000u : 0xff000000u | (x * 7u % 256u) << 8 | (y * 13u % 256u);
    for (unsigned y = 40; y < 42; y++) for (unsigned x = 20; x < 22; x++) pixels[y * 480 + x] = 0xff00ff00u;   /* Link */
    const int sizes[3][4] = { { 1920, 1080, 4, 0 }, { 960, 540, 2, 0 }, { 1280, 720, 2, 1 } };
    for (int i = 0; i < 3; i++) {
        const int fullscreen = sizes[i][3];
        SDL_SetWindowSize(window, sizes[i][0], sizes[i][1]);
        SDL_SyncWindow(window);
        if (fullscreen) { SDL_SetWindowFullscreen(window, true); SDL_SyncWindow(window); }
        oracles_sdl_options options = { 0 };
        options.scale = (unsigned)sizes[i][2];
        options.window = window;
        options.renderer = renderer;
        options.fill = 1;
        options.pause_menu = 1;
        oracles_host_backend backend;
        if (!oracles_sdl_backend_init(&backend, &options)) { CHECK(0); return; }
        if (backend.start(backend.opaque, 480, 270, 48000, 0)) {
            for (unsigned y = 0; y < 2; y++) for (unsigned x = 0; x < 2; x++) { layer[y * 64 + x] = 0xff00ff00u; patch[y * 64 + x] = 0xff000000u; }
            const oracles_host_video_subpixel sub = { 128, 192, 16, 254, layer, 20, 40, 2, 2, 64, 64, -32, patch, 1, { { 100, 200, 40, 20 } }, NULL, 0, { { 0, 0, 0, 0 } }, 0, { 0 } };
            const oracles_host_video_frame image = { pixels, 480, 270, 480 * sizeof(uint32_t), 0, 0, 0, 0, &sub, 160, 200, 160, 48, 1, 0,
                                                     ORACLES_TEXT_BOX_ENLARGE_LARGE };
            CHECK(backend.present_frame(backend.opaque, &image));
            SDL_FRect area;
            CHECK(SDL_GetRenderLogicalPresentationRect(renderer, &area) && area.w == (fullscreen ? 1280.0f : 480.0f * (float)sizes[i][2]));
            SDL_Surface *played = picture(renderer);
            /* The band moved: the frame as it is is another picture. */
            int logical_w = 0, logical_h = 0, out_w = 0, out_h = 0;
            SDL_RendererLogicalPresentation presentation = SDL_LOGICAL_PRESENTATION_DISABLED;
            SDL_GetRenderLogicalPresentation(renderer, &logical_w, &logical_h, &presentation);
            SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
            CHECK(SDL_GetRenderOutputSize(renderer, &out_w, &out_h));
            const OraclesTouchBox box = oracles_touch_frame(out_w, out_h, 480, 270, fullscreen);
            const SDL_FRect at = { box.x, box.y, box.w, box.h };
            SDL_Window *pw; SDL_Renderer *pr; SDL_Texture *kept; int kw = 0, kh = 0;
            oracles_sdl_backend_pause_view(&backend, &pw, &pr, &kept, &kw, &kh);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            CHECK(SDL_RenderTexture(renderer, kept, NULL, &at));
            SDL_Surface *whole = picture(renderer);
            CHECK(differing(played, whole) > 0);
            /* The pause's image: the picture played, the view's layer since then another. */
            layer[0] = layer[1] = layer[64] = layer[65] = 0xffff00ffu;
            SDL_RenderClear(renderer);
            CHECK(oracles_sdl_backend_pause_image(&backend, &at));
            SDL_Surface *paused = picture(renderer);
            CHECK(differing(played, paused) == 0);
            CHECK(!SDL_RenderClipEnabled(renderer));
            SDL_DestroySurface(played);
            SDL_DestroySurface(whole);
            SDL_DestroySurface(paused);
            SDL_SetRenderLogicalPresentation(renderer, logical_w, logical_h, presentation);
            /* Without the sub-pixel camera: the box enlarged over the frame as it is, in play and in the pause. */
            oracles_host_video_frame flat = image;
            flat.subpixel = NULL;
            CHECK(backend.present_frame(backend.opaque, &flat));
            SDL_Surface *played_flat = picture(renderer);
            SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
            SDL_RenderClear(renderer);
            CHECK(SDL_RenderTexture(renderer, kept, NULL, &at));
            SDL_Surface *unboxed = picture(renderer);
            CHECK(differing(played_flat, unboxed) > 0);
            SDL_RenderClear(renderer);
            CHECK(oracles_sdl_backend_pause_image(&backend, &at));
            SDL_Surface *paused_flat = picture(renderer);
            CHECK(differing(played_flat, paused_flat) == 0);
            SDL_DestroySurface(played_flat);
            SDL_DestroySurface(unboxed);
            SDL_DestroySurface(paused_flat);
            SDL_SetRenderLogicalPresentation(renderer, logical_w, logical_h, presentation);
            /* The screen margin draws the band moved too, in its own place: so does the pause. */
            if (i == 1) {
                oracles_sdl_backend_set_screen_margin(&backend, ORACLES_SCREEN_MARGIN_SMALL);
                CHECK(backend.present_frame(backend.opaque, &image));
                SDL_Surface *margined = picture(renderer);
                CHECK(SDL_GetRenderLogicalPresentation(renderer, NULL, NULL, &presentation) && presentation == SDL_LOGICAL_PRESENTATION_DISABLED);
                const OraclesTouchEdges edges = { ORACLES_SCREEN_MARGIN_SMALL, 1.0f, 0, 0 };
                const OraclesTouchBox band = oracles_touch_band(out_w, out_h, &edges);
                const OraclesTouchBox in = oracles_touch_frame((int)band.w, (int)band.h, 480, 270, 0);
                const SDL_FRect there = { band.x + in.x, band.y + in.y, in.w, in.h };
                SDL_RenderClear(renderer);
                CHECK(SDL_RenderTexture(renderer, kept, NULL, &there));
                SDL_Surface *unmoved = picture(renderer);
                CHECK(differing(margined, unmoved) > 0);
                SDL_RenderClear(renderer);
                CHECK(oracles_sdl_backend_pause_image(&backend, &there));
                SDL_Surface *paused_margin = picture(renderer);
                CHECK(differing(margined, paused_margin) == 0);
                SDL_DestroySurface(margined);
                SDL_DestroySurface(unmoved);
                SDL_DestroySurface(paused_margin);
            }
        } else CHECK(0);
        backend.stop(backend.opaque);
        oracles_sdl_backend_release(&backend);
        if (fullscreen) { SDL_SetWindowFullscreen(window, false); SDL_SyncWindow(window); }
    }
    SDL_SetWindowSize(window, 1280, 720);
    SDL_SyncWindow(window);
}

/* The pause switching the status bar to or from floating composes the frame again: the band another height, the
 * camera another fraction, Link another place, the panels laid over it.  Handed that whole move, the pause's image is
 * the picture the new frame would play, Link once at his new place; with the new pixels alone it keeps the old move,
 * his layer at the old place over the new picture. */
static void pause_subpixel_again(SDL_Window *window, SDL_Renderer *renderer)
{
    static uint32_t before[480 * 270], after[480 * 270], layer[64 * 64], patch[64 * 64], panels[480 * 270];
    for (unsigned y = 0; y < 270; y++)
        for (unsigned x = 0; x < 480; x++) {
            before[y * 480 + x] = y < 16 ? 0xffff0000u : 0xff000000u | (x * 7u % 256u) << 8 | (y * 13u % 256u);
            after[y * 480 + x] = 0xff000000u | (x * 7u % 256u) << 8 | ((y + 8u) * 13u % 256u);
        }
    for (unsigned y = 40; y < 42; y++) for (unsigned x = 20; x < 22; x++) before[y * 480 + x] = 0xff00ff00u;   /* Link */
    for (unsigned y = 32; y < 34; y++) for (unsigned x = 20; x < 22; x++) after[y * 480 + x] = 0xff00ff00u;    /* eight lines higher */
    for (unsigned y = 0; y < 2; y++) for (unsigned x = 0; x < 2; x++) { layer[y * 64 + x] = 0xff00ff00u; patch[y * 64 + x] = 0xff000000u; }
    panels[60 * 480 + 30] = panels[60 * 480 + 31] = 0xffff0000u;
    SDL_SetWindowSize(window, 1920, 1080);
    SDL_SyncWindow(window);
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    options.pause_menu = 1;
    oracles_host_backend backend;
    if (!oracles_sdl_backend_init(&backend, &options)) { CHECK(0); return; }
    if (backend.start(backend.opaque, 480, 270, 48000, 0)) {
        const oracles_host_video_subpixel old_move = { 128, 192, 16, 254, layer, 20, 40, 2, 2, 64, 64, -32, patch, 0, { { 0 } }, NULL, 0, { { 0 } }, 0, { 0 } };
        const oracles_host_video_subpixel new_move = { 64, 128, 0, 270, layer, 20, 32, 2, 2, 64, 0, 0, patch, 0, { { 0 } }, panels, 1, { { 30, 60, 2, 1 } }, 0, { 0 } };
        const oracles_host_video_frame played_before = { before, 480, 270, 480 * sizeof(uint32_t), 0, 0, 0, 0, &old_move, 0, 0, 0, 0, 0, 0, 0 };
        const oracles_host_video_frame played_after = { after, 480, 270, 480 * sizeof(uint32_t), 0, 0, 0, 0, &new_move, 0, 0, 0, 0, 0, 0, 0 };
        int logical_w = 0, logical_h = 0, out_w = 0, out_h = 0;
        SDL_RendererLogicalPresentation presentation = SDL_LOGICAL_PRESENTATION_DISABLED;
        SDL_Surface *paused[3] = { NULL, NULL, NULL };
        for (int i = 0; i < 3; i++) {
            CHECK(backend.present_frame(backend.opaque, i == 2 ? &played_after : &played_before));
            if (i < 2) oracles_sdl_backend_pause_pixels(&backend, after);
            if (i == 1) oracles_sdl_backend_pause_subpixel(&backend, &new_move);
            SDL_GetRenderLogicalPresentation(renderer, &logical_w, &logical_h, &presentation);
            SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
            CHECK(SDL_GetRenderOutputSize(renderer, &out_w, &out_h));
            const OraclesTouchBox box = oracles_touch_frame(out_w, out_h, 480, 270, 0);
            const SDL_FRect at = { box.x, box.y, box.w, box.h };
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            CHECK(oracles_sdl_backend_pause_image(&backend, &at));
            paused[i] = picture(renderer);
            SDL_SetRenderLogicalPresentation(renderer, logical_w, logical_h, presentation);
        }
        CHECK(differing(paused[1], paused[2]) == 0);
        CHECK(differing(paused[0], paused[2]) > 0);
        for (int i = 0; i < 3; i++) SDL_DestroySurface(paused[i]);
        /* Back to no move (the camera at whole pixels): the frame as it is. */
        oracles_sdl_backend_pause_subpixel(&backend, NULL);
        SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        const OraclesTouchBox box = oracles_touch_frame(out_w, out_h, 480, 270, 0);
        const SDL_FRect at = { box.x, box.y, box.w, box.h };
        SDL_RenderClear(renderer);
        CHECK(oracles_sdl_backend_pause_image(&backend, &at));
        SDL_Surface *flat = picture(renderer);
        SDL_Window *pw; SDL_Renderer *pr; SDL_Texture *kept; int kw = 0, kh = 0;
        oracles_sdl_backend_pause_view(&backend, &pw, &pr, &kept, &kw, &kh);
        SDL_RenderClear(renderer);
        CHECK(SDL_RenderTexture(renderer, kept, NULL, &at));
        SDL_Surface *texture = picture(renderer);
        CHECK(differing(flat, texture) == 0);
        SDL_DestroySurface(flat);
        SDL_DestroySurface(texture);
        SDL_SetRenderLogicalPresentation(renderer, logical_w, logical_h, presentation);
    } else CHECK(0);
    backend.stop(backend.opaque);
    oracles_sdl_backend_release(&backend);
    SDL_SetWindowSize(window, 1280, 720);
    SDL_SyncWindow(window);
}

/* The large menus: a framed screen shown alone at its own scale is shown so by the pause too, its box enlarged, at that
 * part's size, and not the whole frame at the view's; the pause changing the large menus shows it at once at the view's
 * scale, as a frame at the view's scale played it, and alone again, from either. */
/* The window's whole picture, the logical presentation lifted for it. */
static SDL_Surface *whole_picture(SDL_Renderer *renderer)
{
    int w = 0, h = 0;
    SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    SDL_GetRenderLogicalPresentation(renderer, &w, &h, &mode);
    SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    SDL_Surface *read = picture(renderer);
    SDL_SetRenderLogicalPresentation(renderer, w, h, mode);
    return read;
}

static void pause_menus(SDL_Window *window, SDL_Renderer *renderer)
{
    static uint32_t pixels[480 * 270];
    for (unsigned y = 0; y < 270; y++)
        for (unsigned x = 0; x < 480; x++) pixels[y * 480 + x] = 0xff000000u | (x * 7u % 256u) << 16 | (y * 13u % 256u) << 8 | ((x + y) % 256u);
    SDL_SetWindowSize(window, 1920, 1080);
    SDL_SyncWindow(window);
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    options.pause_menu = 1;
    oracles_host_backend backend;
    if (!oracles_sdl_backend_init(&backend, &options)) { CHECK(0); return; }
    if (backend.start(backend.opaque, 480, 270, 48000, 0)) {
        const uint32_t framed[4] = { 160, 63, 160, 144 };
        const oracles_host_video_frame view = { pixels, 480, 270, 480 * sizeof(uint32_t), 0, 0, 0, 0, NULL, 160, 160, 160, 47, 1, 0,
                                                ORACLES_TEXT_BOX_ENLARGE_LARGE };
        oracles_host_video_frame alone = view;
        alone.crop_x = framed[0]; alone.crop_y = framed[1]; alone.crop_w = framed[2]; alone.crop_h = framed[3];
        int logical_w = 0, logical_h = 0, out_w = 0, out_h = 0, w = 0, h = 0;
        SDL_RendererLogicalPresentation presentation = SDL_LOGICAL_PRESENTATION_DISABLED;
        CHECK(backend.present_frame(backend.opaque, &view));
        SDL_Surface *played_view = whole_picture(renderer);
        CHECK(backend.present_frame(backend.opaque, &alone));
        SDL_Surface *played_alone = whole_picture(renderer);
        CHECK(differing(played_view, played_alone) > 0);
        SDL_GetRenderLogicalPresentation(renderer, &logical_w, &logical_h, &presentation);
        SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        CHECK(SDL_GetRenderOutputSize(renderer, &out_w, &out_h));
        oracles_sdl_backend_pause_size(&backend, &w, &h);
        CHECK(w == 160 && h == 144);
        const OraclesTouchBox part = oracles_touch_frame(out_w, out_h, 160, 144, 0), whole = oracles_touch_frame(out_w, out_h, 480, 270, 0);
        const SDL_FRect at_part = { part.x, part.y, part.w, part.h }, at_whole = { whole.x, whole.y, whole.w, whole.h };
        SDL_Window *pw; SDL_Renderer *pr; SDL_Texture *kept; int kw = 0, kh = 0;
        oracles_sdl_backend_pause_view(&backend, &pw, &pr, &kept, &kw, &kh);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        CHECK(SDL_RenderTexture(renderer, kept, NULL, &at_whole));
        SDL_Surface *unframed = picture(renderer);
        CHECK(differing(played_alone, unframed) > 0);
        SDL_RenderClear(renderer);
        CHECK(oracles_sdl_backend_pause_image(&backend, &at_part));
        SDL_Surface *paused = picture(renderer);
        CHECK(differing(played_alone, paused) == 0);
        SDL_DestroySurface(paused);
        oracles_sdl_backend_pause_crop(&backend, NULL);
        oracles_sdl_backend_pause_size(&backend, &w, &h);
        CHECK(w == 480 && h == 270);
        SDL_RenderClear(renderer);
        CHECK(oracles_sdl_backend_pause_image(&backend, &at_whole));
        paused = picture(renderer);
        CHECK(differing(played_view, paused) == 0);
        SDL_DestroySurface(paused);
        oracles_sdl_backend_pause_crop(&backend, framed);
        oracles_sdl_backend_pause_size(&backend, &w, &h);
        CHECK(w == 160 && h == 144);
        SDL_RenderClear(renderer);
        CHECK(oracles_sdl_backend_pause_image(&backend, &at_part));
        paused = picture(renderer);
        CHECK(differing(played_alone, paused) == 0);
        SDL_DestroySurface(paused);
        /* Played at the view's scale, the pause taking the large menus. */
        SDL_SetRenderLogicalPresentation(renderer, logical_w, logical_h, presentation);
        CHECK(backend.present_frame(backend.opaque, &view));
        SDL_GetRenderLogicalPresentation(renderer, &logical_w, &logical_h, &presentation);
        SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        oracles_sdl_backend_pause_size(&backend, &w, &h);
        CHECK(w == 480 && h == 270);
        oracles_sdl_backend_pause_crop(&backend, framed);
        oracles_sdl_backend_pause_size(&backend, &w, &h);
        CHECK(w == 160 && h == 144);
        SDL_RenderClear(renderer);
        CHECK(oracles_sdl_backend_pause_image(&backend, &at_part));
        paused = picture(renderer);
        CHECK(differing(played_alone, paused) == 0);
        SDL_DestroySurface(paused);
        SDL_DestroySurface(unframed);
        SDL_DestroySurface(played_view);
        SDL_DestroySurface(played_alone);
    } else CHECK(0);
    backend.stop(backend.opaque);
    oracles_sdl_backend_release(&backend);
    SDL_SetWindowSize(window, 1280, 720);
    SDL_SyncWindow(window);
}

/* The pause changing Text size: the box the presenter enlarges taken away from its image at once, and given back, as a
 * frame played with and without it shows; and changing the status bar's layout, the frame's pixels drawn again, as the
 * frame played with them shows. */
static void pause_settings(SDL_Window *window, SDL_Renderer *renderer)
{
    static uint32_t pixels[480 * 270], other[480 * 270];
    for (unsigned y = 0; y < 270; y++)
        for (unsigned x = 0; x < 480; x++) {
            pixels[y * 480 + x] = 0xff000000u | (x * 7u % 256u) << 16 | (y * 13u % 256u) << 8 | ((x + y) % 256u);
            other[y * 480 + x] = y < 16u ? 0xff204060u : pixels[y * 480 + x];
        }
    SDL_SetWindowSize(window, 1920, 1080);
    SDL_SyncWindow(window);
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    options.pause_menu = 1;
    oracles_host_backend backend;
    if (!oracles_sdl_backend_init(&backend, &options)) { CHECK(0); return; }
    if (backend.start(backend.opaque, 480, 270, 48000, 0)) {
        const uint32_t box[4] = { 160, 160, 160, 47 };
        const oracles_host_video_frame boxed = { pixels, 480, 270, 480 * sizeof(uint32_t), 0, 0, 0, 0, NULL, box[0], box[1], box[2], box[3], 1, 0,
                                                 ORACLES_TEXT_BOX_ENLARGE_LARGE };
        oracles_host_video_frame plain = boxed, laid = boxed;
        plain.box_w = plain.box_h = 0;
        laid.pixels = other;
        CHECK(backend.present_frame(backend.opaque, &plain));
        SDL_Surface *played_plain = whole_picture(renderer);
        CHECK(backend.present_frame(backend.opaque, &laid));
        SDL_Surface *played_laid = whole_picture(renderer);
        CHECK(backend.present_frame(backend.opaque, &boxed));
        SDL_Surface *played_boxed = whole_picture(renderer);
        CHECK(differing(played_plain, played_boxed) > 0 && differing(played_boxed, played_laid) > 0);
        int logical_w = 0, logical_h = 0, out_w = 0, out_h = 0;
        SDL_RendererLogicalPresentation presentation = SDL_LOGICAL_PRESENTATION_DISABLED;
        SDL_GetRenderLogicalPresentation(renderer, &logical_w, &logical_h, &presentation);
        SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        CHECK(SDL_GetRenderOutputSize(renderer, &out_w, &out_h));
        const OraclesTouchBox whole = oracles_touch_frame(out_w, out_h, 480, 270, 0);
        const SDL_FRect at = { whole.x, whole.y, whole.w, whole.h };
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        CHECK(oracles_sdl_backend_pause_image(&backend, &at));
        SDL_Surface *paused = picture(renderer);
        CHECK(differing(played_boxed, paused) == 0);
        SDL_DestroySurface(paused);
        oracles_sdl_backend_pause_box(&backend, NULL, 0, 0, ORACLES_TEXT_BOX_ENLARGE_LARGE);
        SDL_RenderClear(renderer);
        CHECK(oracles_sdl_backend_pause_image(&backend, &at));
        paused = picture(renderer);
        CHECK(differing(played_plain, paused) == 0);
        SDL_DestroySurface(paused);
        oracles_sdl_backend_pause_box(&backend, box, 1, 0, ORACLES_TEXT_BOX_ENLARGE_LARGE);
        SDL_RenderClear(renderer);
        CHECK(oracles_sdl_backend_pause_image(&backend, &at));
        paused = picture(renderer);
        CHECK(differing(played_boxed, paused) == 0);
        SDL_DestroySurface(paused);
        oracles_sdl_backend_pause_pixels(&backend, other);
        SDL_RenderClear(renderer);
        CHECK(oracles_sdl_backend_pause_image(&backend, &at));
        paused = picture(renderer);
        CHECK(differing(played_laid, paused) == 0);
        SDL_DestroySurface(paused);
        SDL_SetRenderLogicalPresentation(renderer, logical_w, logical_h, presentation);
        SDL_DestroySurface(played_plain);
        SDL_DestroySurface(played_laid);
        SDL_DestroySurface(played_boxed);
    } else CHECK(0);
    backend.stop(backend.opaque);
    oracles_sdl_backend_release(&backend);
    SDL_SetWindowSize(window, 1280, 720);
    SDL_SyncWindow(window);
}

/* Text size Large: the frame's text box shown again above it at whole screen pixels, in the lent window sized for 160x144
 * at five times (800x720): eight screen pixels a frame pixel (5 + 3), centred on the box across, against its top edge,
 * or its bottom; every block the box's pixel, nearest; a box the frame does not hold so left at the frame's scale.  Very
 * large at ten, at eight where ten does not fit; the large menus' part widened for the box only at its own scale. */
static void enlarged_box(SDL_Window *window, SDL_Renderer *renderer)
{
    static uint32_t pixels[160 * 144];
    for (unsigned y = 0; y < 144u; y++)
        for (unsigned x = 0; x < 160u; x++) pixels[y * 160u + x] = 0xff000000u | ((x * 37u + y * 101u) * 2654435761u >> 8 & 0xffffffu);
    oracles_sdl_options options = { 0 };
    options.scale = 5;
    options.window = window;
    options.renderer = renderer;
    oracles_host_backend backend;
    if (!oracles_sdl_backend_init(&backend, &options)) { CHECK(0); return; }
    if (backend.start(backend.opaque, 160, 144, 48000, 0)) {
        CHECK(SDL_GetRenderOutputSize(renderer, &output_w, &output_h) && output_w == 800 && output_h == 720);
        /* At the top: 40 x 16 frame pixels from (40, 20), 320 x 128 screen pixels from (140, 100). */
        oracles_host_video_frame image = { pixels, 160, 144, 160 * sizeof(uint32_t), 0, 0, 0, 0, NULL, 40, 20, 40, 16, 0, 0, ORACLES_TEXT_BOX_ENLARGE_LARGE };
        CHECK(backend.present_frame(backend.opaque, &image));
        int same = 1;
        for (unsigned j = 0; j < 16u; j++)
            for (unsigned i = 0; i < 40u; i++)
                for (unsigned corner = 0; corner < 2u; corner++) {
                    const int x = 140 + 8 * (int)i + (corner ? 7 : 0), y = 100 + 8 * (int)j + (corner ? 7 : 0);
                    same &= pixel_at(renderer, x, y) == pixels[(20u + j) * 160u + 40u + i];
                }
        CHECK(same);
        CHECK(pixel_at(renderer, 139, 150) == pixels[30u * 160u + 27u] && pixel_at(renderer, 460, 150) == pixels[30u * 160u + 92u]);   /* beside it, the frame */
        /* With the sub-pixel camera's band moved half a pixel left, the box shown unmoved (a fixed rectangle of the view)
         * comes above the band at the frame's whole place, block for block as without the move. */
        const oracles_host_video_subpixel half = { 128, 0, 0, 144, NULL, 0, 0, 0, 0, 0, 0, 0, NULL, 1, { { 40, 20, 40, 16 } }, NULL, 0, { { 0, 0, 0, 0 } }, 0, { 0 } };
        image.subpixel = &half;
        CHECK(backend.present_frame(backend.opaque, &image));
        same = 1;
        for (unsigned j = 0; j < 16u; j++)
            for (unsigned i = 0; i < 40u; i++)
                for (unsigned corner = 0; corner < 2u; corner++) {
                    const int x = 140 + 8 * (int)i + (corner ? 7 : 0), y = 100 + 8 * (int)j + (corner ? 7 : 0);
                    same &= pixel_at(renderer, x, y) == pixels[(20u + j) * 160u + 40u + i];
                }
        CHECK(same);
        CHECK(pixel_at(renderer, 139, 150) != pixels[30u * 160u + 27u]);   /* beside it, the band moved */
        image.subpixel = NULL;
        /* At the bottom: its bottom edge at the frame's, 580, its top at 452. */
        image.box_y = 100;
        image.box_bottom = 1;
        CHECK(backend.present_frame(backend.opaque, &image));
        CHECK(pixel_at(renderer, 140, 452) == pixels[100u * 160u + 40u] && pixel_at(renderer, 459, 579) == pixels[115u * 160u + 79u]);
        CHECK(pixel_at(renderer, 140, 451) == pixels[90u * 160u + 28u]);
        /* Too wide for the frame at that size (120 x 8 = 960): the frame alone. */
        image.box_x = 20; image.box_w = 120; image.box_bottom = 0; image.box_y = 20;
        CHECK(backend.present_frame(backend.opaque, &image));
        CHECK(pixel_at(renderer, 300, 100) == pixels[20u * 160u + 60u] && pixel_at(renderer, 0, 0) == pixels[0]);
        /* Very large: ten screen pixels a pixel, 40 x 16 from (60, 20) at 400 x 160 from (200, 100). */
        image.box_x = 60; image.box_w = 40; image.box_size = ORACLES_TEXT_BOX_ENLARGE_XLARGE;
        CHECK(backend.present_frame(backend.opaque, &image));
        same = 1;
        for (unsigned j = 0; j < 16u; j++)
            for (unsigned i = 0; i < 40u; i++)
                for (unsigned corner = 0; corner < 2u; corner++)
                    same &= pixel_at(renderer, 200 + 10 * (int)i + (corner ? 9 : 0), 100 + 10 * (int)j + (corner ? 9 : 0)) == pixels[(20u + j) * 160u + 60u + i];
        CHECK(same);
        /* Too wide at ten (100 x 10 = 1000): at eight, Large, 800 from 0. */
        image.box_x = 30; image.box_w = 100;
        CHECK(backend.present_frame(backend.opaque, &image));
        CHECK(pixel_at(renderer, 0, 100) == pixels[20u * 160u + 30u] && pixel_at(renderer, 8 * 99 + 7, 107) == pixels[20u * 160u + 129u]);
        /* The large menus: a part of 80 x 144 at (40, 0), five times.  A box of 48 reserved at Very large (480 screen
         * pixels) widens it to 96 at the same five times, 480 x 720 from (160, 0); one of 120 fits at no size at five
         * (1200, 960) and leaves it 80, 400 x 720 from (200, 0), never shown smaller. */
        image.box_w = 0;
        image.crop_x = 40; image.crop_y = 0; image.crop_w = 80; image.crop_h = 144;
        image.box_reserve_w = 48;
        CHECK(backend.present_frame(backend.opaque, &image));
        CHECK(pixel_at(renderer, 162, 300) == pixels[60u * 160u + 32u] && pixel_at(renderer, 162 + 5 * 95, 300) == pixels[60u * 160u + 127u]);
        image.box_reserve_w = 120;
        CHECK(backend.present_frame(backend.opaque, &image));
        CHECK(pixel_at(renderer, 202, 300) == pixels[60u * 160u + 40u] && pixel_at(renderer, 202 + 5 * 79, 300) == pixels[60u * 160u + 119u]);
    } else CHECK(0);
    backend.stop(backend.opaque);
    oracles_sdl_backend_release(&backend);
}

/* A session in the lent window with only a virtual gamepad `vendor`:`product` plugged in, known by `mapping` (a line
 * of SDL's database after its GUID) when given: the game's buttons its joystick buttons 0 and 1 press, with the default
 * names ("a" for A, "b" for B). */
static void face_buttons(SDL_Window *window, SDL_Renderer *renderer, Uint16 vendor, Uint16 product, const char *mapping,
                         unsigned *south, unsigned *east)
{
    *south = *east = 0;
    const SDL_JoystickID pad = attach_gamepad(vendor, product);
    if (pad && mapping) {
        char guid[64], line[1024];
        SDL_GUIDToString(SDL_GetJoystickGUIDForID(pad), guid, sizeof guid);
        snprintf(line, sizeof line, "%s,%s", guid, mapping);
        SDL_AddGamepadMapping(line);
    }
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    oracles_host_backend backend;
    if (pad && oracles_sdl_backend_init(&backend, &options)) {
        if (backend.start(backend.opaque, 160, 144, 48000, 0)) {
            *south = pressed_by(&backend, pad, SDL_GAMEPAD_BUTTON_SOUTH);
            *east = pressed_by(&backend, pad, SDL_GAMEPAD_BUTTON_EAST);
        }
        backend.stop(backend.opaque);
        oracles_sdl_backend_release(&backend);
    }
    SDL_DetachVirtualJoystick(pad);
}

/* The game's buttons the south and the east buttons of the second of two gamepads press, south's in the low byte. */
static unsigned second_pad_buttons(SDL_Window *window, SDL_Renderer *renderer)
{
    unsigned result = 0;
    const SDL_JoystickID first = attach_gamepad(0, 0), second = attach_gamepad(0, 0);
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    oracles_host_backend backend;
    if (first && second && oracles_sdl_backend_init(&backend, &options)) {
        if (backend.start(backend.opaque, 160, 144, 48000, 0))
            result = pressed_by(&backend, second, SDL_GAMEPAD_BUTTON_SOUTH) | pressed_by(&backend, second, SDL_GAMEPAD_BUTTON_EAST) << 8;
        backend.stop(backend.opaque);
        oracles_sdl_backend_release(&backend);
    }
    SDL_DetachVirtualJoystick(first);
    SDL_DetachVirtualJoystick(second);
    return result;
}

#if !defined(_WIN32) && !defined(__APPLE__)
#include <sys/stat.h>

/* SDL's file dialog of `kind` through a zenity in `folder`, first on the PATH, that prints `output` and exits with
 * `status` (and writes its arguments one a line into folder/arguments), opened on `start` for Seasons: its answer from
 * the queue, the path chosen into `chosen`; -1 when none came for Seasons and that kind. */
static int fake_dialog(const char *folder, OraclesDialogKind kind, const char *start, const char *output, int status, char *chosen, size_t capacity)
{
    char script[1200];
    snprintf(script, sizeof script, "%s/zenity", folder);
    FILE *f = fopen(script, "w");
    if (!f) return -1;
    fprintf(f, "#!/bin/sh\n[ \"$1\" = --version ] && { echo 3.44.0; exit 0; }\nprintf '%%s\\n' \"$@\" > '%s/arguments'\n"
               "printf '%%s' '%s'\nexit %d\n", folder, output, status);
    fclose(f);
    chmod(script, 0700);
    oracles_choose_file(NULL, kind, start, 1);
    for (int i = 0; i < 1000; i++) {
        SDL_Event e;
        OraclesDialogResult result;
        OraclesDialogKind answered;
        int game, copied;
        if (SDL_WaitEventTimeout(&e, 10) && oracles_dialog_answer(&e, &result, &answered, &game, chosen, capacity, &copied))
            return game == 1 && answered == kind && !copied ? (int)result : -1;
    }
    return -1;
}

/* Whether the fake dialog's arguments, read back from folder/arguments, hold `line` right after `after`. */
static int argument_given(const char *folder, const char *after, const char *line)
{
    char name[1200], previous[4200] = "", read[4200];
    snprintf(name, sizeof name, "%s/arguments", folder);
    FILE *f = fopen(name, "r");
    int found = 0;
    while (f && !found && fgets(read, sizeof read, f)) {
        read[strcspn(read, "\n")] = 0;
        found = !strcmp(previous, after) && !strcmp(read, line);
        snprintf(previous, sizeof previous, "%s", read);
    }
    if (f) fclose(f);
    return found;
}

/* The dialog chose a file, was cancelled, could not open: the last is no dialog, which the home screen says with its
 * fallback, "drop the ROM on this window".  It opens in the ROM's folder, titled; a patch's is titled as one. */
static void dialogs(const char *folder)
{
    char chosen[1024], start[1200], opened_in[1300];
    mkdir(folder, 0700);
    snprintf(start, sizeof start, "%s/roms", folder);
    snprintf(opened_in, sizeof opened_in, "%s/", start);
    CHECK(fake_dialog(folder, ORACLES_DIALOG_ROM, start, "/games/Oracle of Seasons.gbc", 0, chosen, sizeof chosen) == ORACLES_DIALOG_CHOSEN);
    CHECK(!strcmp(chosen, "/games/Oracle of Seasons.gbc"));
    CHECK(argument_given(folder, "--filename", opened_in));
    CHECK(argument_given(folder, "--title", "Choose ROM"));
    CHECK(fake_dialog(folder, ORACLES_DIALOG_ROM, "", "", 1, chosen, sizeof chosen) == ORACLES_DIALOG_CANCELLED);
    CHECK(fake_dialog(folder, ORACLES_DIALOG_ROM, "", "", 255, chosen, sizeof chosen) == ORACLES_DIALOG_UNAVAILABLE);
    CHECK(fake_dialog(folder, ORACLES_DIALOG_PATCH, start, "/games/Moonrise.bps", 0, chosen, sizeof chosen) == ORACLES_DIALOG_CHOSEN);
    CHECK(!strcmp(chosen, "/games/Moonrise.bps"));
    CHECK(argument_given(folder, "--title", "Choose patch"));
}
#endif

/* A start then a stop, as oracles_host_run calls them (stop even after a failed start), with this audio driver. */
static int lend(SDL_Window *window, SDL_Renderer *renderer, const char *audio_driver)
{
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, audio_driver);
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    oracles_host_backend backend;
    if (!oracles_sdl_backend_init(&backend, &options)) return -1;
    const int started = backend.start(backend.opaque, 160, 144, 48000, 1);
    if (started) {
        CHECK(dropped_and_left(&backend, SDL_EVENT_DROP_FILE));
        CHECK(dropped_and_left(&backend, SDL_EVENT_DROP_TEXT));
    }
    backend.stop(backend.opaque);
    oracles_sdl_backend_release(&backend);
    return started;
}

static void stored(void *opaque) { (*(int *)opaque)++; }

/* A square wave's frames at 48 kHz queued as the host queues a frame's, `frames` times. */
static void queue_tone(oracles_host_backend *backend, unsigned frames)
{
    static int16_t tone[800 * 2];
    for (unsigned i = 0; i < 800; i++) tone[i * 2] = tone[i * 2 + 1] = (int16_t)((i / 50) % 2 ? 6000 : -6000);
    const oracles_host_audio_chunk chunk = { tone, 1600, 48000 };
    for (unsigned i = 0; i < frames; i++) CHECK(backend->queue_audio(backend->opaque, &chunk));
}

/* Android's background with the sound playing (SDL's dummy device pulls it in time): SDL stops the device as it is and
 * starts it again as the application comes back, so the backend leaves it paused, its stream empty, by the end of the
 * suspension's watch, the save stored there; the host's cut at the suspension finds it so, and the next frames start it
 * again from their prebuffer, without a gap counted.  The host's cut at the pause lets the device play a short fade.
 * After an underrun the sound starts again faded in. */
static void background_audio(SDL_Window *window, SDL_Renderer *renderer)
{
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    options.pause_menu = 1;
    oracles_host_backend backend;
    if (!oracles_sdl_backend_init(&backend, &options)) { CHECK(0); return; }
    if (backend.start(backend.opaque, 160, 144, 48000, 1)) {
        int stores = 0;
        backend.watch_suspend(backend.opaque, stored, &stores);
        queue_tone(&backend, 4);
        CHECK(oracles_sdl_backend_audio_plays(&backend) && backend.audio_queued_frames(backend.opaque) > 0);
        SDL_Event e;
        SDL_zero(e);
        e.type = SDL_EVENT_WILL_ENTER_BACKGROUND;
        const uint64_t since = SDL_GetTicksNS();
        SDL_PushEvent(&e);
        printf("the suspension's watch: %.1f ms\n", (double)(SDL_GetTicksNS() - since) / 1e6);
        CHECK(stores == 1 && !oracles_sdl_backend_audio_plays(&backend) && backend.audio_queued_frames(backend.opaque) == 0);
        queue_tone(&backend, 1);   /* a frame run back from the background, before the poll hands the suspension */
        CHECK(!oracles_sdl_backend_audio_plays(&backend));
        oracles_host_event event;
        int has_event = 0;
        CHECK(backend.poll_event(backend.opaque, &event, &has_event) && has_event && event.type == ORACLES_HOST_EVENT_SUSPEND);
        backend.cut_audio(backend.opaque);
        CHECK(!oracles_sdl_backend_audio_plays(&backend) && backend.audio_queued_frames(backend.opaque) == 0);
        queue_tone(&backend, 4);
        CHECK(oracles_sdl_backend_audio_plays(&backend));
        /* The pause: the device plays on the sound's first 10 ms faded out, then silence, and waits for its prebuffer
         * again from the next frame, without a gap counted. */
        SDL_AudioStream *stream = ((const sdl_backend *)backend.opaque)->audio;
        SDL_LockAudioStream(stream);
        backend.cut_audio(backend.opaque);
        const uint32_t fade = backend.audio_queued_frames(backend.opaque);
        SDL_UnlockAudioStream(stream);
        CHECK(oracles_sdl_backend_audio_plays(&backend) && fade > 0 && fade <= 480);
        SDL_Delay(60);
        CHECK(oracles_sdl_backend_audio_plays(&backend) && backend.audio_queued_frames(backend.opaque) == 0);
        queue_tone(&backend, 1);
        CHECK(!oracles_sdl_backend_audio_plays(&backend) && backend.audio_queued_frames(backend.opaque) == 800);
        queue_tone(&backend, 3);
        CHECK(oracles_sdl_backend_audio_plays(&backend));
        unsigned underruns, drops, peak, average;
        oracles_sdl_backend_audio_report(&backend, &underruns, &drops, &peak, &average);
        CHECK(underruns == 0 && drops == 0);
        /* An underrun: the device played all it had; the frames that start it again fade in from silence. */
        SDL_Delay(150);
        queue_tone(&backend, 1);
        oracles_sdl_backend_audio_report(&backend, &underruns, &drops, &peak, &average);
        CHECK(underruns == 1);
        const OraclesAudioRecent *recent = &((const sdl_backend *)backend.opaque)->recent;
        for (unsigned i = 0; i < 800; i++) {
            const int16_t v = recent->samples[((recent->end + ORACLES_AUDIO_RECENT_FRAMES - 800 + i) % ORACLES_AUDIO_RECENT_FRAMES) * 2];
            const double t = i < 480 ? i / 480.0 : 1.0;
            CHECK(abs(v - (int)(((i / 50) % 2 ? 6000 : -6000) * t * t * t)) <= 1);
        }
        backend.watch_suspend(backend.opaque, NULL, NULL);
    } else CHECK(0);
    backend.stop(backend.opaque);
    oracles_sdl_backend_release(&backend);
}

/* The home screen's window and renderer still work: the window found by its id, SDL's video on, a frame drawn. */
/* The gamepads a game started on the lent window finds open at its start. */
static unsigned pads_at_start(SDL_Window *window, SDL_Renderer *renderer)
{
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    oracles_host_backend backend;
    if (!oracles_sdl_backend_init(&backend, &options)) return 99u;
    unsigned pads = 99u;
    if (backend.start(backend.opaque, 160, 144, 48000, 0)) pads = oracles_sdl_backend_pads_at_start(&backend);
    backend.stop(backend.opaque);
    oracles_sdl_backend_release(&backend);
    return pads;
}

static int intact(SDL_Window *window, SDL_WindowID id, SDL_Renderer *renderer)
{
    if (SDL_GetWindowFromID(id) != window || SDL_GetRenderer(window) != renderer) return 0;
    if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO)) return 0;
    int w = 0, h = 0;
    SDL_RendererLogicalPresentation presentation = SDL_LOGICAL_PRESENTATION_INTEGER_SCALE;
    SDL_GetRenderLogicalPresentation(renderer, &w, &h, &presentation);
    if (w || h || presentation != SDL_LOGICAL_PRESENTATION_DISABLED) return 0;
    return SDL_SetRenderDrawColor(renderer, 5, 5, 7, 255) && SDL_RenderClear(renderer);
}

/* A copy kept in the application's folder (Android's chosen files) never writes over a file: the same bytes under the
 * same name are taken as they are; other bytes, a save that shares the ROM's name or the other game's ROM, get the
 * next free name; no temporary file is left. */
static void copies(const char *base)
{
    char folder[1200], path[1400], expected[1400];
    snprintf(folder, sizeof folder, "%s/copies", base);
    static const char *const leftovers[] = { "Oracle.gbc", "Oracle (2).gbc", "Oracle (3).gbc", "Oracle.sav", "noext", "noext (2)" };
    for (size_t i = 0; i < sizeof leftovers / sizeof leftovers[0]; i++) {
        snprintf(path, sizeof path, "%s/%s", folder, leftovers[i]);
        SDL_RemovePath(path);
    }
    int fresh = -1;
    CHECK(oracles_dialog_store_copy(folder, "Oracle.gbc", "ages", 4, path, sizeof path, &fresh) && fresh == 1);
    snprintf(expected, sizeof expected, "%s/Oracle.gbc", folder);
    CHECK(!strcmp(path, expected));
    CHECK(oracles_dialog_store_copy(folder, "Oracle.gbc", "ages", 4, path, sizeof path, &fresh) && fresh == 0 && !strcmp(path, expected));
    CHECK(oracles_dialog_store_copy(folder, "Oracle.gbc", "seasons", 7, path, sizeof path, &fresh) && fresh == 1);
    snprintf(expected, sizeof expected, "%s/Oracle (2).gbc", folder);
    CHECK(!strcmp(path, expected));
    CHECK(oracles_dialog_store_copy(folder, "Oracle.gbc", "other", 5, path, sizeof path, &fresh) && fresh == 1);
    snprintf(expected, sizeof expected, "%s/Oracle (3).gbc", folder);
    CHECK(!strcmp(path, expected));
    size_t size = 0;
    snprintf(expected, sizeof expected, "%s/Oracle.gbc", folder);
    void *kept = SDL_LoadFile(expected, &size);
    CHECK(kept && size == 4 && !memcmp(kept, "ages", 4));   /* the first file untouched */
    SDL_free(kept);
    snprintf(expected, sizeof expected, "%s/Oracle.sav", folder);
    CHECK(SDL_SaveFile(expected, "save", 4));
    CHECK(oracles_dialog_store_copy(folder, "Oracle.sav", "chosen", 6, path, sizeof path, &fresh) && fresh == 1 && strcmp(path, expected));
    kept = SDL_LoadFile(expected, &size);
    CHECK(kept && size == 4 && !memcmp(kept, "save", 4));   /* a save sharing the name is never written over */
    SDL_free(kept);
    SDL_RemovePath(path);
    CHECK(oracles_dialog_store_copy(folder, "noext", "a", 1, path, sizeof path, &fresh) && fresh == 1);
    CHECK(oracles_dialog_store_copy(folder, "noext", "b", 1, path, sizeof path, &fresh) && fresh == 1);
    snprintf(expected, sizeof expected, "%s/noext (2)", folder);
    CHECK(!strcmp(path, expected));
    snprintf(expected, sizeof expected, "%s/Oracle.gbc.partial", folder);
    SDL_PathInfo info;
    CHECK(!SDL_GetPathInfo(expected, &info));
#ifndef _WIN32
    /* A folder the application cannot write into nor look through (another owner's, on Android): the copy fails with
     * the reason of the write, not that of the partial file's removal, which fails too. */
    char locked[1300];
    snprintf(locked, sizeof locked, "%s/locked", folder);
    CHECK(SDL_CreateDirectory(locked));
    if (geteuid() != 0 && chmod(locked, 0444) == 0) {
        SDL_ClearError();
        CHECK(!oracles_dialog_store_copy(locked, "Oracle.gbc", "ages", 4, path, sizeof path, &fresh) && fresh == 0);
        const char *reason = SDL_GetError();
        CHECK(reason[0] && !strstr(reason, "remove") && strstr(reason, "Permission denied"));
        if (strstr(reason, "remove")) fprintf(stderr, "copy refused for: %s\n", reason);
        chmod(locked, 0755);
    }
    SDL_RemovePath(locked);
#endif
}

/* Under Smooth, Link's layer drawn apart and sampled bilinearly: its texels the layer leaves transparent, whatever colour
 * they carry, and the texture around it never show, at its edges as inside. */
static void smooth_layer(SDL_Window *window, SDL_Renderer *renderer)
{
    static uint32_t pixels[160 * 144], layer[4 * 4], patch[4 * 4];
    for (unsigned i = 0; i < 160u * 144u; i++) pixels[i] = 0xff2040a0u;
    for (unsigned i = 0; i < 16u; i++) { layer[i] = 0x00ff00ffu; patch[i] = 0xff2040a0u; }   /* transparent, magenta under */
    layer[1 * 4 + 1] = 0xff00c000u;   /* Link, one pixel */
    oracles_sdl_filter_start("smooth");
    SDL_SetWindowSize(window, 640, 576);
    SDL_SyncWindow(window);
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    oracles_host_backend backend;
    if (!oracles_sdl_backend_init(&backend, &options)) { CHECK(0); oracles_sdl_filter_start("off"); return; }
    if (backend.start(backend.opaque, 160, 144, 48000, 0)) {
        const oracles_host_video_subpixel sub = { 128, 96, 16, 128, layer, 60, 70, 3, 3, 4, 96, 32, patch, 0, { { 0, 0, 0, 0 } }, NULL, 0, { { 0, 0, 0, 0 } }, 0, { 0 } };
        const oracles_host_video_frame image = { pixels, 160, 144, 160 * sizeof(uint32_t), 0, 0, 0, 0, &sub, 0, 0, 0, 0, 0, 0, 0u };
        CHECK(backend.present_frame(backend.opaque, &image));
        SDL_Surface *shown = picture(renderer);
        long magenta = 0, green = 0;
        for (int y = 0; shown && y < shown->h; y++)
            for (int x = 0; x < shown->w; x++) {
                const uint32_t p = ((const uint32_t *)((const uint8_t *)shown->pixels + (size_t)y * (size_t)shown->pitch))[x];
                const unsigned r = p >> 16 & 0xffu, g = p >> 8 & 0xffu, b = p & 0xffu;
                if (r > 0x40u) magenta++;   /* the background has none, Link none, a blend of the two none */
                if (g > 0x80u && b < 0x80u) green++;
            }
        CHECK(shown && magenta == 0 && green > 0);
        SDL_DestroySurface(shown);
        backend.stop(backend.opaque);
    } else CHECK(0);
    oracles_sdl_backend_release(&backend);
    oracles_sdl_filter_start("off");
}

/* Under Smooth, Link drawn apart at his own fraction of a pixel, a quarter to three quarters across and down, the band
 * still or moved: the frame's own drawing of him, at his whole pixels, shows nowhere around him, at the patch's edges as
 * inside his holes, and he is drawn at his own place.  The frame's Link is red, his layer's green, the ground blue. */
static void smooth_fringe(SDL_Window *window, SDL_Renderer *renderer)
{
    static uint32_t pixels[160 * 144], layer[8 * 8], patch[8 * 8];
    const uint32_t ground = 0xff2040a0u;
    for (unsigned i = 0; i < 160u * 144u; i++) pixels[i] = ground;
    for (unsigned i = 0; i < 64u; i++) layer[i] = patch[i] = 0x00ff00ffu;   /* transparent, magenta under */
    long mask_x = 0, mask_y = 0, mask = 0;
    for (unsigned y = 1; y < 7u; y++)
        for (unsigned x = 1; x < 7u; x++) {
            if ((x == 3u || x == 4u) && y == 4u) continue;   /* a hole in him */
            pixels[(69u + y) * 160u + 59u + x] = 0xffc00000u;
            layer[y * 8u + x] = 0xff00c000u;
            patch[y * 8u + x] = ground;
            mask_x += (long)(59u + x); mask_y += (long)(69u + y); mask++;
        }
    oracles_sdl_filter_start("smooth");
    SDL_SetWindowSize(window, 640, 576);
    SDL_SyncWindow(window);
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    oracles_host_backend backend;
    if (!oracles_sdl_backend_init(&backend, &options)) { CHECK(0); oracles_sdl_filter_start("off"); return; }
    if (backend.start(backend.opaque, 160, 144, 48000, 0)) {
        static const int32_t bands[2][2] = { { 0, 0 }, { 64, 192 } };
        for (unsigned b = 0; b < 2u; b++)
            for (int32_t fy = 0; fy < 256; fy += 64)
                for (int32_t fx = 0; fx < 256; fx += 64) {
                    const oracles_host_video_subpixel sub = { bands[b][0], bands[b][1], 16, 128, layer, 59, 69, 8, 8, 8, fx, fy, patch, 0, { { 0, 0, 0, 0 } }, NULL, 0, { { 0, 0, 0, 0 } }, 0, { 0 } };
                    const oracles_host_video_frame image = { pixels, 160, 144, 160 * sizeof(uint32_t), 0, 0, 0, 0, &sub, 0, 0, 0, 0, 0, 0, 0u };
                    CHECK(backend.present_frame(backend.opaque, &image));
                    SDL_FRect area;
                    CHECK(SDL_GetRenderLogicalPresentationRect(renderer, &area) && (int)(area.w / 160.0f) == 4);
                    SDL_Surface *shown = picture(renderer);
                    long red = 0;
                    double weight = 0.0, at_x = 0.0, at_y = 0.0;
                    for (int y = 0; shown && y < shown->h; y++)
                        for (int x = 0; x < shown->w; x++) {
                            const uint32_t p = ((const uint32_t *)((const uint8_t *)shown->pixels + (size_t)y * (size_t)shown->pitch))[x];
                            const unsigned r = p >> 16 & 0xffu, g = p >> 8 & 0xffu;
                            if (r > 0x24u) red++;   /* the ground has 0x20, Link none, a blend of the two no more */
                            if (g > 0x40u) { weight += g - 0x40u; at_x += (g - 0x40u) * (x + 0.5); at_y += (g - 0x40u) * (y + 0.5); }
                        }
                    CHECK(shown && red == 0 && weight > 0.0);
                    const double want_x = area.x + 4.0 * ((double)mask_x / (double)mask + 0.5 + (double)fx / 256.0);
                    const double want_y = area.y + 4.0 * ((double)mask_y / (double)mask + 0.5 + (double)fy / 256.0);
                    CHECK(weight > 0.0 && SDL_fabs(at_x / weight - want_x) < 0.5 && SDL_fabs(at_y / weight - want_y) < 0.5);
                    SDL_DestroySurface(shown);
                }
        backend.stop(backend.opaque);
    } else CHECK(0);
    oracles_sdl_backend_release(&backend);
    oracles_sdl_filter_start("off");
}

/* One frame of `width` x 144 under the shader filter `name`, presented at four times on an offscreen window through the
 * GPU renderer; `layered`: with Link's layer (the frame's own pixels under it otherwise).  The ground is blue, Link
 * `size` pixels square at (at_x + 1, at_y + 1) of the frame and moved a quarter of a pixel right and down by his own
 * fraction: green (`colours` 0), the ground's colour (1), or red in his layer and green in the frame (2), so that the
 * frame's Link left showing is told from his layer's; or wholly transparent on a ground of every colour (3), the frame
 * without him.  The texels his layer and the patch leave transparent carry magenta.  `band`: the band's move (x, y),
 * NULL for none.  `fill`: in fullscreen with Fill, the window 16/3 times the frame, the picture as the pause then
 * shows it, at the window's size, with Fill's sampling (a picture presented in fullscreen does not read back here).
 * `paused`: then shown by the pause, too small for the filter, its picture written again there with a grey ground (the
 * patch with it), and at the window's size again.  NULL when the GPU renderer or the window cannot be made here. */
static SDL_Surface *filtered_frame(const char *name, int layered, int colours, int at_x, int at_y, int width, int size, const int32_t *band, int fill,
                                   int paused)
{
    static uint32_t pixels[480 * 144], layer[16 * 16], patch[16 * 16];
    const uint32_t ground = 0xff2040a0u, in_frame = colours == 1 ? ground : 0xff00c000u, him = colours == 2 ? 0xffd00000u : colours == 3 ? 0x00ff00ffu : in_frame;
    for (int i = 0; i < width * 144; i++)
        pixels[i] = colours == 3 ? 0xff000000u | ((uint32_t)i * 37u & 0xffu) << 16 | ((uint32_t)(i / width) * 53u & 0xffu) << 8 | ((uint32_t)i * 29u & 0xffu) : ground;
    for (unsigned i = 0; i < 256u; i++) layer[i] = patch[i] = 0x00ff00ffu;
    for (int y = 1; y <= size; y++)
        for (int x = 1; x <= size; x++) {
            layer[y * 16 + x] = him;
            uint32_t *at = &pixels[(at_y + y) * width + at_x + x];
            patch[y * 16 + x] = colours == 3 ? *at : ground;
            if (colours != 3) *at = in_frame;   /* the frame has him at the band's place */
        }
    SDL_Window *window = SDL_CreateWindow("filter", fill ? 16 * width / 3 : 4 * width, fill ? 768 : 576, 0);
    if (window && fill) { SDL_SetWindowFullscreen(window, true); SDL_SyncWindow(window); }
    SDL_Renderer *renderer = window ? oracles_sdl_gpu_renderer(window, 0) : NULL;
    SDL_Surface *shown = NULL;
    oracles_sdl_filter_start(name);
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    options.fill = fill;
    oracles_host_backend backend;
    if (renderer && oracles_sdl_backend_init(&backend, &options)) {
        if (backend.start(backend.opaque, (uint32_t)width, 144, 48000, 0)) {
            const oracles_host_video_subpixel sub = { band ? band[0] : 0, band ? band[1] : 0, 16, 128, layered ? layer : NULL, at_x, at_y, (uint32_t)size + 2,
                                                      (uint32_t)size + 2, 16, 64, 64, patch, 0, { { 0, 0, 0, 0 } }, NULL, 0, { { 0, 0, 0, 0 } }, 0, { 0 } };
            const oracles_host_video_frame image = { pixels, (uint32_t)width, 144, (size_t)width * sizeof(uint32_t), 0, 0, 0, 0, &sub, 0, 0, 0, 0, 0, 0, 0u };
            CHECK(backend.present_frame(backend.opaque, &image));
            if (paused || fill) {
                SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                int out_w = 0, out_h = 0;
                SDL_GetRenderOutputSize(renderer, &out_w, &out_h);
                const SDL_FRect small = { 0.0f, 0.0f, 2.0f * (float)width, 288.0f }, at = { 0.0f, 0.0f, (float)out_w, (float)out_h };
                if (paused) {
                    CHECK(oracles_sdl_backend_pause_image(&backend, &small));   /* too small for the filter */
                    static uint32_t again[480 * 144];   /* a picture written again there, its ground another colour, its layer with it */
                    for (int i = 0; i < width * 144; i++) again[i] = pixels[i] == ground ? 0xff181818u : pixels[i];
                    for (unsigned i = 0; i < 256u; i++) if (patch[i] == ground) patch[i] = 0xff181818u;
                    oracles_sdl_backend_pause_pixels(&backend, again);
                }
                SDL_RenderClear(renderer);
                CHECK(oracles_sdl_backend_pause_image(&backend, &at));
            }
            SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);   /* the whole screen, the black beside the picture too */
            shown = picture(renderer);
            backend.stop(backend.opaque);
        } else CHECK(0);
        oracles_sdl_backend_release(&backend);
    }
    oracles_sdl_filter_start("off");
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    return shown;
}

static uint32_t surface_at(const SDL_Surface *s, int x, int y) { return ((const uint32_t *)((const uint8_t *)s->pixels + (size_t)y * (size_t)s->pitch))[x]; }

/* The pixels of two pictures of a size that differ by more than `levels` in a channel (a curve's rounding apart). */
static long differing_by(const SDL_Surface *a, const SDL_Surface *b, unsigned levels)
{
    if (!a || !b || a->w != b->w || a->h != b->h) return -1;
    long count = 0;
    for (int y = 0; y < a->h; y++)
        for (int x = 0; x < a->w; x++) {
            const uint32_t p = surface_at(a, x, y), q = surface_at(b, x, y);
            for (unsigned shift = 0; shift < 24; shift += 8) {
                const unsigned c = p >> shift & 0xffu, d = q >> shift & 0xffu;
                if ((c > d ? c - d : d - c) > levels) { count++; break; }
            }
        }
    return count;
}

static int greenish(uint32_t p) { return (p >> 8 & 0xffu) > 0x80u && (p >> 16 & 0xffu) < 0x40u && (p & 0xffu) < 0x80u; }

/* The pixels of a picture whose channel at `shift` (16 red, 8 green, 0 blue) stands above the two others (Link, through any
 * filter). */
static long colour_in(const SDL_Surface *s, unsigned shift)
{
    long count = 0;
    for (int y = 0; s && y < s->h; y++)
        for (int x = 0; x < s->w; x++) {
            const uint32_t p = surface_at(s, x, y), c = p >> shift & 0xffu;
            int above = 1;
            for (unsigned other = 0; other < 24u; other += 8u)
                if (other != shift && c <= (p >> other & 0xffu) + 0x30u) above = 0;
            count += above;
        }
    return s ? count : -1;
}

/* Under a shader filter, Link drawn apart at his own fraction of a pixel over the ground the frame has under him: the
 * magenta his transparent texels carry never shows, nor anything darker than the ground or him (a fringe filtered
 * against nothing); his place takes a quarter of a pixel of the frame, one of the window's.  A Link of the ground's own
 * colour is the frame itself under Scanlines: his layer keeps the band's rows.  Under CRT, in the picture's bottom
 * corners, he shows whole and once. */
static void filter_layer(void)
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) { printf("filter layer: no offscreen video, skipped\n"); return; }
    SDL_Surface *shown = filtered_frame("xbr", 1, 0, 60, 70, 160, 4, NULL, 0, 0);
    if (!shown) printf("filter layer: no GPU renderer here (%s), skipped\n", SDL_GetError());
    else {
        long magenta = 0, dark = 0;
        for (int y = 64; y < 512; y++)
            for (int x = 0; x < shown->w; x++) {
                const uint32_t p = surface_at(shown, x, y), r = p >> 16 & 0xffu, g = p >> 8 & 0xffu, b = p & 0xffu;
                if (r > 0x40u) magenta++;
                if (g < 0x30u && b < 0x60u) dark++;
            }
        CHECK(magenta == 0 && dark == 0);
        /* At the band's place he would take 244 to 259 across; at his own, 245 to 260. */
        CHECK(!greenish(surface_at(shown, 244, 291)) && greenish(surface_at(shown, 245, 291)));
        CHECK(greenish(surface_at(shown, 260, 291)) && !greenish(surface_at(shown, 261, 291)));
        SDL_DestroySurface(shown);
        SDL_Surface *apart = filtered_frame("scanlines", 1, 1, 60, 70, 160, 4, NULL, 0, 0), *whole = filtered_frame("scanlines", 0, 1, 60, 70, 160, 4, NULL, 0, 0);
        CHECK(apart && whole && differing_by(apart, whole, 2) == 0);
        SDL_DestroySurface(apart);
        SDL_DestroySurface(whole);
        /* In the band's corners, in a picture as wide as Far's, where CRT's curve moves what it reads by up to ten pixels
         * of the frame: him whole, as much of him red as the frame drawn whole shows green, and once, none of the
         * frame's green left beside him; in play, and in the pause's image after its picture was written again with
         * another ground while too small for CRT, none of the ground before left around him. */
        static const int corners[4][2] = { { -1, 135 }, { 471, 135 }, { -1, 15 }, { 471, 15 } };
        for (int i = 0; i < 8; i++) {
            apart = filtered_frame("crt", 1, 2, corners[i % 4][0], corners[i % 4][1], 480, 8, NULL, 0, i / 4);
            whole = filtered_frame("crt", 0, 2, corners[i % 4][0], corners[i % 4][1], 480, 8, NULL, 0, 0);
            const long red = colour_in(apart, 16), green = colour_in(whole, 8);
            CHECK(green > 400 && red * 10 >= green * 9 && red * 9 <= green * 10 && colour_in(apart, 8) == 0);
            CHECK(i < 4 || colour_in(apart, 0) == 0);
            SDL_DestroySurface(apart);
            SDL_DestroySurface(whole);
        }
        /* Sampled smoothly by the GPU, in play and in the pause's image, in the band's middle and at the frame's edges:
         * none of the frame's green around him at his own fraction, under the Smooth filter in a window, under Fill at
         * the screen's scale, and under both. */
        static const int places[5][3] = { { 60, 70, 0 }, { 60, 70, 1 }, { -1, 70, 0 }, { 155, 70, 0 }, { 60, 139, 0 } };
        for (int i = 0; i < 11; i++) {
            const int *place = places[i < 10 ? i % 5 : 0];
            apart = filtered_frame(i < 5 || i == 10 ? "smooth" : "off", 1, 2, place[0], place[1], 160, 4, NULL, i >= 5, place[2]);
            long green = 0;
            for (int y = 0; apart && y < apart->h; y++)
                for (int x = 0; x < apart->w; x++) green += (surface_at(apart, x, y) >> 8 & 0xffu) > (place[2] ? 0x20u : 0x48u);   /* above the ground's */
            CHECK(apart && green == 0 && colour_in(apart, 16) > 0);
            SDL_DestroySurface(apart);
        }
        /* On a ground of every colour, the ground drawn under a transparent Link the band's to the pixel, under the
         * Smooth filter, Off, Scanlines and CRT: in the band's middle, and at its top, the band moved down by a fraction,
         * where the status bar's last line stays out of the band. */
        static const char *const plain[4] = { "smooth", "off", "scanlines", "crt" };
        static const int32_t moved[2] = { 0, -128 };
        static const int tops[3] = { 70, 16, 13 };
        for (int i = 0; i < 12; i++) {
            const int32_t *band = i % 3 ? moved : NULL;
            apart = filtered_frame(plain[i / 3], 1, 3, 60, tops[i % 3], 160, 4, band, 0, 0);
            whole = filtered_frame(plain[i / 3], 0, 3, 60, tops[i % 3], 160, 4, band, 0, 0);
            CHECK(apart && whole && differing_by(apart, whole, 2) == 0);
            SDL_DestroySurface(apart);
            SDL_DestroySurface(whole);
        }
    }
    oracles_sdl_gpu_device_release();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

/* A frame of `width` x 144, its band the whole of it, a ground of green, with the floating panels as the view makes them:
 * at (at_x, at_y) a panel 24 x 16 of cream at 80 % with two pixels' corners, a block of red 6 x 6 opaque in it; apart
 * in their layer (`layered`, the band at a fraction), or laid over the frame by the CPU.  Presented at four times on an
 * offscreen window through the GPU renderer under the filter `name` (`swap_from`: under that one first, then the
 * renderer told of `name` as a change of filter makes it); `pause`: then shown by the pause, at a small size first
 * (`pause` 2: too small for the filter) and at the window's.  NULL where the GPU renderer cannot be made. */
static SDL_Surface *panels_frame(const char *name, const char *swap_from, int layered, int at_x, int at_y, int width, int pause)
{
    static uint32_t pixels[480 * 144], panels[480 * 144];
    const uint32_t ground = 0xff00c000u, cream = 0xfffaf4dcu, red = 0xffd02020u;
    for (int i = 0; i < width * 144; i++) { pixels[i] = ground; panels[i] = 0; }
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 24; x++) {
            const int corner = (y == 0 || y == 15) ? (x < 2 || x > 21) : (y == 1 || y == 14) ? (x < 1 || x > 22) : 0;
            if (corner) continue;
            const int inside = x >= 9 && x < 15 && y >= 5 && y < 11;
            uint32_t p = 0;
            for (unsigned shift = 0; shift < 32u; shift += 8u) {   /* premultiplied, as the view's put() mixes it */
                const unsigned over = shift == 24u ? 255u : (cream >> shift) & 0xffu;
                p |= ((over * 205u) >> 8) << shift;
            }
            if (inside) p = red;
            uint32_t *at = &panels[(at_y + y) * width + at_x + x];
            *at = p;
            if (!layered) {   /* laid over the frame as the harness and a screenshot fold it */
                const uint32_t under = pixels[(at_y + y) * width + at_x + x], a = p >> 24;
                uint32_t out = 0xff000000u;
                for (unsigned shift = 0; shift < 24u; shift += 8u)
                    out |= ((((p >> shift) & 0xffu) * 255u + ((under >> shift) & 0xffu) * (255u - a) + 127u) / 255u) << shift;
                pixels[(at_y + y) * width + at_x + x] = out;
            }
        }
    SDL_Window *window = SDL_CreateWindow("panels", 4 * width, 576, 0);
    SDL_Renderer *renderer = window ? oracles_sdl_gpu_renderer(window, 0) : NULL;
    SDL_Surface *shown = NULL;
    oracles_sdl_filter_start(swap_from ? swap_from : name);
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    oracles_host_backend backend;
    if (renderer && oracles_sdl_backend_init(&backend, &options)) {
        if (backend.start(backend.opaque, (uint32_t)width, 144, 48000, 0)) {
            oracles_host_video_subpixel sub = { 64, 32, 0, 144, NULL, 0, 0, 0, 0, 0, 0, 0, NULL, 0, { { 0, 0, 0, 0 } }, NULL, 0, { { 0, 0, 0, 0 } }, 0, { 0 } };
            if (layered) { sub.panels = panels; sub.panels_count = 1; sub.panels_rects[0][0] = (uint32_t)at_x; sub.panels_rects[0][1] = (uint32_t)at_y; sub.panels_rects[0][2] = 24; sub.panels_rects[0][3] = 16; }
            const oracles_host_video_frame image = { pixels, (uint32_t)width, 144, (size_t)width * sizeof(uint32_t), 0, 0, 0, 0, &sub, 0, 0, 0, 0, 0, 0, 0u };
            CHECK(backend.present_frame(backend.opaque, &image));
            if (swap_from) {
                oracles_sdl_filter_start(name);
                CHECK(oracles_sdl_renderer_swap(renderer, name) == renderer);
            }
            if (pause || swap_from) {
                SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                int out_w = 0, out_h = 0;
                SDL_GetRenderOutputSize(renderer, &out_w, &out_h);
                const SDL_FRect small = { 0.0f, 0.0f, (float)width, 144.0f }, at = { 0.0f, 0.0f, (float)out_w, (float)out_h };
                SDL_RenderClear(renderer);
                CHECK(oracles_sdl_backend_pause_image(&backend, pause == 2 ? &small : &at));
            }
            SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);   /* the whole screen, the black beside the picture too */
            shown = picture(renderer);
            backend.stop(backend.opaque);
        } else CHECK(0);
        oracles_sdl_backend_release(&backend);
    }
    oracles_sdl_filter_start("off");
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    return shown;
}

/* The pixels of a picture whose red stands above half, none of the green ground's. */
static long red_channel(const SDL_Surface *s)
{
    long count = 0;
    for (int y = 0; s && y < s->h; y++)
        for (int x = 0; x < s->w; x++) count += (surface_at(s, x, y) >> 16 & 0xffu) > 0x80u;
    return count;
}

/* The pixels darker than the ground and the panel, in a part of a picture (x, y, w, h) and `ring` around it. */
static long dark_around(const SDL_Surface *s, int x0, int y0, int w, int h, int ring)
{
    long count = 0;
    for (int y = SDL_max(y0 - ring, 0); s && y < SDL_min(y0 + h + ring, s->h); y++)
        for (int x = SDL_max(x0 - ring, 0); x < SDL_min(x0 + w + ring, s->w); x++) {
            const uint32_t p = surface_at(s, x, y);
            count += (p >> 16 & 0xffu) < 0x50u && (p >> 8 & 0xffu) < 0x50u && (p & 0xffu) < 0x50u;
        }
    return count;
}

/* Each channel's mean over a part of a picture (x, y, w, h). */
static void mean_of(const SDL_Surface *s, int x0, int y0, int w, int h, double mean[3])
{
    mean[0] = mean[1] = mean[2] = 0.0;
    for (int y = y0; s && y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++)
            for (unsigned c = 0; c < 3u; c++) mean[c] += (double)(surface_at(s, x, y) >> (16u - 8u * c) & 0xffu);
    for (unsigned c = 0; c < 3u; c++) mean[c] /= (double)(w * h);
}

/* The floating panels, apart in their layer under the sub-pixel camera, through each filter as the frame with the panels
 * laid over it goes: their colour over the ground the same (not darkened by a second premultiplication), none of them
 * lost, no edge darker than the ground and them (filtered against black); in CRT's corners, where its curve moves them,
 * as much of their red as the frame drawn whole shows; drawn by the pause too small for the filter; and after a change of
 * filter, written again for the new one. */
static void filter_panels(void)
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) { printf("filter panels: no offscreen video, skipped\n"); return; }
    static const char *const names[8] = { "off", "smooth", "scanlines", "crt", "lcd", "xbr", "hq2x", "omniscale" };
    for (int i = 0; i < 10; i++) {
        const char *name = i < 8 ? names[i] : "xbr";
        const int pause = i == 9 ? 2 : 0;
        SDL_Surface *apart = panels_frame(name, i == 8 ? "scanlines" : NULL, 1, 40, 60, 160, pause);
        if (!apart && !i) { printf("filter panels: no GPU renderer here (%s), skipped\n", SDL_GetError()); break; }
        SDL_Surface *whole = panels_frame(name, NULL, 0, 40, 60, 160, pause);
        CHECK(apart && whole);
        if (!apart || !whole) { SDL_DestroySurface(apart); SDL_DestroySurface(whole); continue; }
        const int k = pause == 2 ? 1 : 4;
        double a[3], b[3];
        mean_of(apart, (40 + 2) * k, (60 + 2) * k, 20 * k, 3 * k, a);   /* the panel's cream over the ground, the red apart */
        mean_of(whole, (40 + 2) * k, (60 + 2) * k, 20 * k, 3 * k, b);
        int near = 1;
        for (unsigned c = 0; c < 3u; c++) near &= a[c] - b[c] < 6.0 && b[c] - a[c] < 6.0;
        if (!near) fprintf(stderr, "filter panels %s (%d): %.1f %.1f %.1f against %.1f %.1f %.1f\n", name, i, a[0], a[1], a[2], b[0], b[1], b[2]);
        CHECK(near);
        const long red = colour_in(apart, 16), red_whole = colour_in(whole, 16);
        const long dark = dark_around(apart, 40 * k, 60 * k, 24 * k, 16 * k, 3 * k), dark_whole = dark_around(whole, 40 * k, 60 * k, 24 * k, 16 * k, 3 * k);
        if (dark * 4 > dark_whole * 5 + 2 * k || red * 10 < red_whole * 9) fprintf(stderr, "filter panels %s (%d): %ld dark against %ld, red %ld against %ld\n", name, i, dark, dark_whole, red, red_whole);
        CHECK(dark * 4 <= dark_whole * 5 + 2 * k && red * 10 >= red_whole * 9 && red * 9 <= red_whole * 10);
        SDL_DestroySurface(apart);
        SDL_DestroySurface(whole);
    }
    static const int corners[4][2] = { { 0, 0 }, { 456, 0 }, { 0, 128 }, { 456, 128 } };
    for (int i = 0; i < 4; i++) {
        SDL_Surface *apart = panels_frame("crt", NULL, 1, corners[i][0], corners[i][1], 480, 0), *whole = panels_frame("crt", NULL, 0, corners[i][0], corners[i][1], 480, 0);
        const long shown = red_channel(apart), shown_whole = red_channel(whole);   /* the panel, cream and red, on the green */
        if (apart && whole && (shown * 10 < shown_whole * 9 || shown * 9 > shown_whole * 10)) fprintf(stderr, "filter panels crt corner %d: %ld against %ld\n", i, shown, shown_whole);
        CHECK(!apart || (whole && shown_whole > 1000 && shown * 10 >= shown_whole * 9 && shown * 9 <= shown_whole * 10));
        SDL_DestroySurface(apart);
        SDL_DestroySurface(whole);
    }
    oracles_sdl_gpu_device_release();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

/* A case of the near view's panels at half the game's scale: the filter `name` (`swap_from`: under that one first, then
 * the renderer told of `name`), the band still (`smooth` 0) or moved by a fraction, an offscreen window `w` x `h` (the
 * frame's whole scale: the start sizes it so) or `fullscreen` (the offscreen display's 1024 x 768) and a frame
 * `frame_w` x 144; `fill`: Scaling's Fill; `margin`: the screen margin, whose drawing presents the frame itself (the
 * touch controls' path); `pause`: then shown by the pause at the picture's place (1), or at the frame's size (2). */
typedef struct half_case {
    const char *name, *swap_from;
    int smooth, w, h, frame_w, fullscreen, fill, margin, pause;
} half_case;

/* What half_frame presents: the panels over the frame, or their layer as a frame of its own with the panels apart on
 * its grid as the far view has them, or the panels opaque and of the ground's own grey, or that ground without them, or
 * the same opaque panels over black, or the frame itself black with their grey at half their place and size, or the
 * third panel alone. */
enum { HALF_SHOWN, HALF_FAR, HALF_SOLID, HALF_BARE, HALF_SOLID_DARK, HALF_BAKED, HALF_THIRD };

/* The near view's panels at half the game's scale: a frame of the case's, a ground of black, its panels in a layer twice
 * its size, one at the top-left and one kept to the bottom-right, each 24 x 16 at 80 % with a block of red 6 x 6 opaque
 * from (9, 5), the first cream, the second a pale blue, and a third of that blue without a block two pixels right of
 * the first.  Presented through the GPU renderer as the case says; `box`: where the picture stands on the screen, as
 * the touch controls' layout places it.  HALF_FAR: their layer as a frame of its own, the same ground, presented as the
 * case's window presents it, neither swapped nor paused.  `pause` 3: shown by the pause too small for the filter, the
 * renderer told of the same filter again (which keeps the place it was last given), then shown by the pause at the
 * picture's place.  HALF_SOLID and HALF_BARE: the band still, a ground of grey
 * with the same panels whole and opaque of that grey, or none; HALF_SOLID_DARK those panels over black, HALF_BAKED that
 * grey drawn in the frame itself, black, at half the panels' place and size, none apart.  NULL where the GPU renderer cannot be made. */
static SDL_Surface *half_frame(const half_case *hc, int variant, SDL_FRect *box)
{
    static uint32_t pixels[512 * 288], panels[512 * 288];
    const int reference = variant == HALF_FAR, grey = variant == HALF_SOLID || variant == HALF_BARE || variant == HALF_SOLID_DARK || variant == HALF_BAKED;
    const int lw = 2 * hc->frame_w, fw = reference ? lw : hc->frame_w, fh = reference ? 288 : 144;
    const uint32_t ground = grey && variant != HALF_SOLID_DARK && variant != HALF_BAKED ? 0xff8a7a6au : 0xff000000u, creams[2] = { 0xfffaf4dcu, 0xffdce4fau }, red = 0xffd02020u;
    for (int i = 0; i < fw * fh; i++) pixels[i] = ground;
    memset(panels, 0, sizeof panels);
    const int places[3][2] = { { 6, 6 }, { lw - 6 - 24, 288 - 6 - 16 }, { 6 + 24 + 2, 6 } };
    for (int p = 0; p < 3; p++)
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 24; x++) {
                const int corner = (y == 0 || y == 15) ? (x < 2 || x > 21) : (y == 1 || y == 14) ? (x < 1 || x > 22) : 0;
                if (corner && !grey) continue;
                uint32_t c = 0;
                for (unsigned shift = 0; shift < 32u; shift += 8u) c |= ((((shift == 24u ? 255u : (creams[p ? 1 : 0] >> shift) & 0xffu)) * 205u) >> 8) << shift;
                if (p < 2 && x >= 9 && x < 15 && y >= 5 && y < 11) c = red;
                panels[(places[p][1] + y) * lw + places[p][0] + x] = !grey ? c : variant == HALF_SOLID || variant == HALF_BARE ? ground : 0xff8a7a6au;
                if (variant == HALF_BAKED && !(x & 1) && !(y & 1)) pixels[(places[p][1] + y) / 2 * fw + (places[p][0] + x) / 2] = 0xff8a7a6au;
            }
    if (variant == HALF_THIRD) {   /* the third panel alone, the others' pixels gone with them */
        for (int y = 0; y < 288; y++)
            for (int x = 0; x < lw; x++)
                if (x < places[2][0] || x >= places[2][0] + 24 || y < places[2][1] || y >= places[2][1] + 16) panels[y * lw + x] = 0;
    }
    SDL_Window *window = SDL_CreateWindow("half panels", hc->w, hc->h, 0);
    if (window && hc->fullscreen) { SDL_SetWindowFullscreen(window, true); SDL_SyncWindow(window); }
    SDL_Renderer *renderer = window ? oracles_sdl_gpu_renderer(window, 0) : NULL;
    SDL_Surface *shown = NULL;
    const char *swap_from = reference ? NULL : hc->swap_from;
    oracles_sdl_filter_start(swap_from ? swap_from : hc->name);
    oracles_sdl_options options = { 0 };
    options.scale = (unsigned)(hc->w / fw);
    options.window = window;
    options.renderer = renderer;
    options.fill = hc->fill;
    options.screen_margin = hc->margin ? ORACLES_SCREEN_MARGIN_SMALL : ORACLES_SCREEN_MARGIN_OFF;
    oracles_host_backend backend;
    if (renderer && oracles_sdl_backend_init(&backend, &options)) {
        int out_w = 0, out_h = 0;
        const int started = backend.start(backend.opaque, (unsigned)fw, (unsigned)fh, 48000, 0);
        CHECK(SDL_GetRenderOutputSize(renderer, &out_w, &out_h));
        const OraclesTouchEdges edges = { hc->margin ? ORACLES_SCREEN_MARGIN_SMALL : ORACLES_SCREEN_MARGIN_OFF, 1.0f, 0, 0 };
        const OraclesTouchBox band = oracles_touch_band(out_w, out_h, &edges), in = oracles_touch_frame((int)band.w, (int)band.h, fw, fh, hc->fill);
        *box = (SDL_FRect){ band.x + in.x, band.y + in.y, in.w, in.h };
        if (started) {
            oracles_host_video_subpixel sub;
            memset(&sub, 0, sizeof sub);
            const int moved = hc->smooth && (variant == HALF_SHOWN || variant == HALF_THIRD);
            sub.x = moved ? 64 : 0; sub.y = moved ? 32 : 0; sub.band_height = (uint32_t)fh;
            sub.panels = panels; sub.panels_count = variant == HALF_BARE || variant == HALF_BAKED ? 0 : 3; sub.panels_scale = reference ? 0 : 2;
            for (int p = 0; p < 3; p++) {
                sub.panels_rects[p][0] = (uint32_t)places[p][0]; sub.panels_rects[p][1] = (uint32_t)places[p][1];
                sub.panels_rects[p][2] = 24; sub.panels_rects[p][3] = 16;
            }
            sub.panels_anchor[1] = ORACLES_FLOATING_RIGHT | ORACLES_FLOATING_BOTTOM;
            if (variant == HALF_THIRD) {   /* the only panel there is */
                memcpy(sub.panels_rects[0], sub.panels_rects[2], sizeof sub.panels_rects[0]);
                sub.panels_count = 1;
                sub.panels_anchor[0] = 0;
            }
            const oracles_host_video_frame image = { pixels, (uint32_t)fw, (uint32_t)fh, (size_t)fw * sizeof(uint32_t), 0, 0, 0, 0, &sub, 0, 0, 0, 0, 0, 0, 0u };
            CHECK(backend.present_frame(backend.opaque, &image));
            if (swap_from) {
                oracles_sdl_filter_start(hc->name);
                CHECK(oracles_sdl_renderer_swap(renderer, hc->name) == renderer);
            }
            if (!reference && hc->pause == 3) {   /* drawn by the pause too small for the filter, its renderer then told of it again */
                SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
                const SDL_FRect small = { 0.0f, 0.0f, (float)fw, 144.0f };
                CHECK(oracles_sdl_backend_pause_image(&backend, &small));
                CHECK(oracles_sdl_renderer_swap(renderer, hc->name) == renderer);
            }
            if (!reference && (hc->pause || swap_from)) {
                SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                const SDL_FRect small = { 0.0f, 0.0f, (float)fw, 144.0f };
                SDL_RenderClear(renderer);
                CHECK(oracles_sdl_backend_pause_image(&backend, hc->pause == 2 ? &small : box));
            }
            SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);   /* the whole screen, the black beside the picture too */
            shown = picture(renderer);
            backend.stop(backend.opaque);
        } else CHECK(0);
        oracles_sdl_backend_release(&backend);
    }
    oracles_sdl_filter_start("off");
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    return shown;
}

/* The red of a half-scale panel's block, `unit` screen pixels a pixel: every pixel of its square of 6 units from
 * (x, y) the block's own red, unfiltered, and the pixels beside its edges not. */
static int red_square(const SDL_Surface *s, int x, int y, int unit)
{
    int whole = 1;
    for (int j = 0; j < 6 * unit && whole; j++)
        for (int i = 0; i < 6 * unit && whole; i++) whole = (surface_at(s, x + i, y + j) & 0xffffffu) == 0xd02020u;
    const uint32_t beside[4] = { surface_at(s, x - 1, y), surface_at(s, x + 6 * unit, y), surface_at(s, x, y - 1), surface_at(s, x, y + 6 * unit) };
    for (unsigned k = 0; k < 4u; k++) whole &= (beside[k] & 0xffffffu) != 0xd02020u;
    return whole;
}

/* The red pixels of `s` (colour_in's) inside x0..x1, y0..y1 (excluded), cut to it. */
static long red_in(const SDL_Surface *s, int x0, int y0, int x1, int y1)
{
    long count = 0;
    for (int y = SDL_max(y0, 0); y < SDL_min(y1, s->h); y++)
        for (int x = SDL_max(x0, 0); x < SDL_min(x1, s->w); x++) {
            const uint32_t p = surface_at(s, x, y), r = p >> 16 & 0xffu;
            count += r > (p >> 8 & 0xffu) + 0x30u && r > (p & 0xffu) + 0x30u;
        }
    return count;
}

/* The red pixels of `s` (red_in's) outside `count` parts (x, y, w, h) widened by `reach` on every side. */
static long red_outside(const SDL_Surface *s, const int (*parts)[4], int count, int reach)
{
    long outside = 0;
    for (int y = 0; y < s->h; y++)
        for (int x = 0; x < s->w; x++) {
            int in = 0;
            for (int i = 0; i < count && !in; i++)
                in = x >= parts[i][0] - reach && y >= parts[i][1] - reach && x < parts[i][0] + parts[i][2] + reach && y < parts[i][1] + parts[i][3] + reach;
            if (!in) outside += red_in(s, x, y, x + 1, y + 1);
        }
    return outside;
}

/* The pixels of a part (x, y, w, h) of two pictures of a size that differ by more than `levels` in a channel. */
static long part_differing(const SDL_Surface *a, const SDL_Surface *b, int x0, int y0, int w, int h, unsigned levels)
{
    if (!a || !b || a->w != b->w || a->h != b->h) return -1;
    long count = 0;
    for (int y = SDL_max(y0, 0); y < SDL_min(y0 + h, a->h); y++)
        for (int x = SDL_max(x0, 0); x < SDL_min(x0 + w, a->w); x++) {
            const uint32_t p = surface_at(a, x, y), q = surface_at(b, x, y);
            for (unsigned shift = 0; shift < 24u; shift += 8u) {
                const int d = (int)(p >> shift & 0xffu) - (int)(q >> shift & 0xffu);
                if (d > (int)levels || -d > (int)levels) { count++; break; }
            }
        }
    return count;
}

/* The near view's panels at half the game's scale over the picture through each filter, with the band still and moved,
 * at seven, eight, four and three times (a unit of 4, 4, 2, 2), and after a change of filter, and in the pause's image at
 * the picture's place.  Off, or a shader the picture is too small for: crisp, each pixel a square of the unit, their red
 * block whole and the game's own red, kept to their corners, only there.  Through a filter (Smooth's sampling, or a
 * shader the picture is large enough for): their red where their blocks stand, as far as CRT's curve moves it, none
 * outside their panels.  Under Scanlines, CRT and LCD, at any unit, the screen's masks running on from the picture into
 * them: panels opaque of the ground's own grey show as the ground alone does; the third panel, two pixels from the
 * first, as it shows alone, the first reading none of it; and under CRT, at a scale twice the unit, each bent over as
 * much of the screen as the frame's own pixels of its grey at half its place and size.  Under Smooth, xBR, HQ2x and
 * OmniScale, where their layer drawn as a frame of its own at the unit takes the picture's very place (a scale twice
 * the unit, an even unit large enough for the shader: no screen pixel's centre on a texel's middle, where a shader's
 * rounding goes either way), their panels as the far view's apart show them through the same filter at that frame's
 * grid, pixel for pixel, over black: each texel its own, drawn once, kept to their own colour against a neighbour two
 * pixels away, which the far view draws with them.  Then off, through CRT and through xBR, still and moved: on a screen
 * the picture does not fill (1024 x 768 in fullscreen: 256 x 144 at four times from 0, 96, a unit of 2; 192 x 144 at
 * five times from 32, 24, a unit of 3), at twice (a unit of 1, too small for CRT), with Fill (192 x 144 at 16/3 on
 * 1024 x 768, a unit of 3), and through the screen margin's own drawing (the touch controls' path: in 1792 x 1008, the
 * band 24 lines clear at the top and the bottom, six times from 128, 72, a unit of 3).  In the pause's image at the
 * frame's size, half a pixel of the screen a pixel, too small for any filter, each block where its panel stands, at its
 * corner, and nowhere else. */
static void half_panels(void)
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) { printf("half panels: no offscreen video, skipped\n"); return; }
    static const char *const names[8] = { "off", "smooth", "scanlines", "crt", "lcd", "xbr", "hq2x", "omniscale" };
    static const int settings_index[8] = { 0, 4, 1, 2, 3, 5, 6, 7 };   /* oracles_display_filter_min_scale's */
    static const int windows[4][2] = { { 1792, 1008 }, { 768, 432 }, { 2048, 1152 }, { 1024, 576 } };
    static const int others[5][6] = { { 1024, 768, 256, 1, 0, 0 }, { 1024, 768, 192, 1, 0, 0 }, { 512, 288, 256, 0, 0, 0 }, { 1024, 768, 192, 1, 1, 0 },
                                      { 1792, 1008, 256, 0, 0, 1 } };
    static const char *const others_names[3] = { "off", "crt", "xbr" };
    half_case cases[4 * 8 * 2 + 4 + 3 + 5 * 3 * 2];
    int count = 0;
    for (int i = 0; i < 4 * 8 * 2; i++)
        cases[count++] = (half_case){ names[(i / 2) % 8], NULL, i % 2, windows[i / 16][0], windows[i / 16][1], 256, 0, 0, 0, 0 };
    cases[count++] = (half_case){ "xbr", "scanlines", 1, 2048, 1152, 256, 0, 0, 0, 0 };
    for (int pause = 1; pause <= 2; pause++) cases[count++] = (half_case){ "xbr", NULL, 1, 2048, 1152, 256, 0, 0, 0, pause };
    cases[count++] = (half_case){ "crt", NULL, 1, 2048, 1152, 256, 0, 0, 0, 1 };
    for (int f = 0; f < 3; f++) cases[count++] = (half_case){ f == 0 ? "crt" : f == 1 ? "lcd" : "xbr", NULL, 0, 2048, 1152, 256, 0, 0, 0, 3 };
    for (int i = 0; i < 5 * 3 * 2; i++) {
        const int *o = others[i / 6];
        cases[count++] = (half_case){ others_names[(i / 2) % 3], NULL, i % 2, o[0], o[1], o[2], o[3], o[4], o[5], 0 };
    }
    unsigned compared = 0, seamless = 0;
    for (int i = 0; i < count; i++) {
        const half_case *hc = &cases[i];
        SDL_FRect box = { 0.0f, 0.0f, 0.0f, 0.0f };
        SDL_Surface *shown = half_frame(hc, HALF_SHOWN, &box);
        if (!shown && !i) { printf("half panels: no GPU renderer here (%s), skipped\n", SDL_GetError()); break; }
        CHECK(shown);
        if (!shown) continue;
        const long red = colour_in(shown, 16);
        if (hc->pause == 2) {   /* at the frame's size: each block a square of 3, (7, 5) and (245, 135), a pixel either way */
            const long top_left = red_in(shown, 6, 4, 11, 9), bottom_right = red_in(shown, 244, 134, 249, 139);
            if (top_left < 4 || bottom_right < 4 || top_left + bottom_right != red) fprintf(stderr, "half panels paused small (%d): %ld %ld, red %ld\n", i, top_left, bottom_right, red);
            CHECK(top_left >= 4 && top_left <= 16 && bottom_right >= 4 && bottom_right <= 16 && top_left + bottom_right == red);
            /* Their texels straight for the shader the frame was shown through, blended straight without it: the cream
             * at 80 % over the black, as premultiplied. */
            const uint32_t cream = surface_at(shown, 4, 6), want[3] = { 0xfau * 205u >> 8, 0xf4u * 205u >> 8, 0xdcu * 205u >> 8 };
            int as_cream = 1;
            for (unsigned c = 0; c < 3u; c++) as_cream &= (int)(cream >> (16u - 8u * c) & 0xffu) - (int)want[c] <= 3 && (int)want[c] - (int)(cream >> (16u - 8u * c) & 0xffu) <= 3;
            if (!as_cream) fprintf(stderr, "half panels paused small (%d): cream %06x\n", i, cream & 0xffffffu);
            CHECK(as_cream);
            SDL_DestroySurface(shown);
            continue;
        }
        if (hc->pause == 3) {   /* as the frame showed them, though the filter was last placed too small for itself */
            half_case first = *hc;
            first.pause = 0;
            SDL_FRect at = { 0.0f, 0.0f, 0.0f, 0.0f };
            SDL_Surface *presented = half_frame(&first, HALF_SHOWN, &at);
            const long off = part_differing(shown, presented, 0, 0, shown->w, shown->h, 2);
            if (off) fprintf(stderr, "half panels %s (%d): %ld pixels of the pause off the frame shown\n", hc->name, i, off);
            CHECK(presented && !off);
            SDL_DestroySurface(presented);
            SDL_DestroySurface(shown);
            continue;
        }
        int f = 0;
        while (f < 7 && strcmp(names[f], hc->name)) f++;
        const float scale = box.w / (float)hc->frame_w;
        const int unit = (int)SDL_floorf(scale / 2.0f + 0.5f), x0 = (int)SDL_floorf(box.x), y0 = (int)SDL_floorf(box.y);
        const int pw = (int)box.w, ph = (int)box.h, min_scale = oracles_display_filter_min_scale[settings_index[f]];
        const int shader = f >= 2, filtered = f == 1 || (shader && scale + 1e-3f >= (float)min_scale);
        const int blocks[2][2] = { { x0 + (6 + 9) * unit, y0 + (6 + 5) * unit }, { x0 + pw - (24 + 6 - 9) * unit, y0 + ph - (16 + 6 - 5) * unit } };
        const int rects[3][4] = { { x0 + 6 * unit, y0 + 6 * unit, 24 * unit, 16 * unit }, { x0 + pw - (24 + 6) * unit, y0 + ph - (16 + 6) * unit, 24 * unit, 16 * unit },
                                  { x0 + 32 * unit, y0 + 6 * unit, 24 * unit, 16 * unit } };
        if (!filtered) {
            const int top_left = red_square(shown, blocks[0][0], blocks[0][1], unit), bottom_right = red_square(shown, blocks[1][0], blocks[1][1], unit);
            if (!top_left || !bottom_right || red != 2L * 36 * unit * unit)
                fprintf(stderr, "half panels %s (%d, %dx%d, at %.1f,%.1f %.1fx%.1f): %d %d, red %ld\n", hc->name, i, hc->w, hc->h, (double)box.x, (double)box.y, (double)box.w,
                        (double)box.h, top_left, bottom_right, red);
            CHECK(top_left && bottom_right && red == 2L * 36 * unit * unit);
            SDL_DestroySurface(shown);
            continue;
        }
        /* Their red where their blocks stand, as far as CRT's curve takes it, and none outside their panels. */
        const int reach = unit + (f == 3 ? (int)SDL_ceilf(0.021f * (float)SDL_max(pw, ph)) + 1 : 0);
        for (int b = 0; b < 2; b++) {
            const long r = red_in(shown, blocks[b][0] - reach, blocks[b][1] - reach, blocks[b][0] + 6 * unit + reach, blocks[b][1] + 6 * unit + reach);
            if (r < 18L * unit * unit) fprintf(stderr, "half panels %s (%d): block %d red %ld\n", hc->name, i, b, r);
            CHECK(r >= 18L * unit * unit);
        }
        const long outside = red_outside(shown, rects, 3, reach);
        if (outside) fprintf(stderr, "half panels %s (%d): red %ld outside the panels\n", hc->name, i, outside);
        CHECK(!outside);
        if (f == 2 || f == 3 || f == 4) CHECK(!red_square(shown, blocks[0][0], blocks[0][1], unit) && !red_square(shown, blocks[1][0], blocks[1][1], unit));
        if (f == 2 || f == 3 || f == 4) {   /* the screen's masks running on from the picture into them, whatever the unit */
            SDL_FRect at = { 0.0f, 0.0f, 0.0f, 0.0f }, bare_at = at;
            SDL_Surface *solid = half_frame(hc, HALF_SOLID, &at), *bare = half_frame(hc, HALF_BARE, &bare_at);
            long seams = 0;
            for (int b = 0; solid && bare && b < 3; b++)
                seams += part_differing(solid, bare, rects[b][0] - reach, rects[b][1] - reach, rects[b][2] + 2 * reach, rects[b][3] + 2 * reach, 2);
            if (!solid || !bare || seams) fprintf(stderr, "half panels %s (%d): %ld pixels of the panels off the picture's own mask\n", hc->name, i, seams);
            CHECK(solid && bare && !seams);
            seamless++;
            SDL_DestroySurface(solid);
            SDL_DestroySurface(bare);
            /* The third panel as it shows alone, where it stands as CRT bends it, the first, two pixels away, reading none
             * of it. */
            SDL_Surface *third = half_frame(hc, HALF_THIRD, &at);
            long doubled = third ? 0 : -1;
            for (int y = SDL_max(rects[2][1] - reach, 0); third && y < SDL_min(rects[2][1] + rects[2][3] + reach, third->h); y++)
                for (int x = SDL_max(rects[2][0] - reach, 0); x < SDL_min(rects[2][0] + rects[2][2] + reach, third->w); x++)
                    if (surface_at(third, x, y) & 0xffffffu) doubled += part_differing(shown, third, x, y, 1, 1, 2);
            if (doubled) fprintf(stderr, "half panels %s (%d): %ld pixels of the third panel off its own\n", hc->name, i, doubled);
            CHECK(third && !doubled);
            SDL_DestroySurface(third);
            if (f == 3 && !hc->fill && 2 * unit == (int)scale && (float)(int)scale == scale) {
                /* Under CRT, each panel bent over as much of the screen as the frame's own pixels of its grey at half its place. */
                SDL_Surface *dark = half_frame(hc, HALF_SOLID_DARK, &at), *baked = half_frame(hc, HALF_BAKED, &bare_at);
                for (int b = 0; dark && baked && b < 3; b++) {
                    int lit[2][4];
                    for (int v = 0; v < 2; v++) {
                        const SDL_Surface *s = v ? baked : dark;
                        lit[v][0] = lit[v][1] = INT_MAX; lit[v][2] = lit[v][3] = -1;
                        for (int y = SDL_max(rects[b][1] - reach, 0); y < SDL_min(rects[b][1] + rects[b][3] + reach, s->h); y++)
                            for (int x = SDL_max(rects[b][0] - reach, 0); x < SDL_min(rects[b][0] + rects[b][2] + reach, s->w); x++) {
                                const uint32_t p = surface_at(s, x, y);
                                if ((p >> 16 & 0xffu) < 0x18u && (p >> 8 & 0xffu) < 0x18u && (p & 0xffu) < 0x18u) continue;
                                lit[v][0] = SDL_min(lit[v][0], x); lit[v][1] = SDL_min(lit[v][1], y); lit[v][2] = SDL_max(lit[v][2], x); lit[v][3] = SDL_max(lit[v][3], y);
                            }
                    }
                    int near = lit[0][2] >= 0 && lit[1][2] >= 0;
                    for (int k = 0; k < 4; k++) near &= lit[0][k] - lit[1][k] <= unit + 1 && lit[1][k] - lit[0][k] <= unit + 1;
                    if (!near) fprintf(stderr, "half panels crt (%d): panel %d lit over %d,%d-%d,%d against the frame's %d,%d-%d,%d\n", i, b, lit[0][0], lit[0][1], lit[0][2], lit[0][3],
                                       lit[1][0], lit[1][1], lit[1][2], lit[1][3]);
                    CHECK(near);
                }
                CHECK(dark && baked);
                SDL_DestroySurface(dark);
                SDL_DestroySurface(baked);
            }
        } else if (!hc->fill && 2 * unit == (int)scale && (float)(int)scale == scale && unit % 2 == 0 && unit >= min_scale) {
            SDL_FRect at = { 0.0f, 0.0f, 0.0f, 0.0f };
            SDL_Surface *apart = half_frame(hc, HALF_FAR, &at);
            if (!apart || at.x != box.x || at.y != box.y || at.w != box.w || at.h != box.h)
                fprintf(stderr, "half panels %s (%d): the far view's frame at %.1f,%.1f %.1fx%.1f\n", hc->name, i, (double)at.x, (double)at.y, (double)at.w, (double)at.h);
            CHECK(apart && at.x == box.x && at.y == box.y && at.w == box.w && at.h == box.h);
            const int around = f == 1 ? 0 : reach;   /* Smooth's edges drawn to the panel, the far view's between two it draws as one */
            for (int b = 0; apart && b < 3; b++) {
                const long panel = part_differing(shown, apart, rects[b][0] - around, rects[b][1] - around, rects[b][2] + 2 * around, rects[b][3] + 2 * around, 2);
                if (panel) fprintf(stderr, "half panels %s (%d): panel %d against the far view's: %ld pixels differ\n", hc->name, i, b, panel);
                CHECK(!panel);
            }
            compared++;
            SDL_DestroySurface(apart);
        }
        SDL_DestroySurface(shown);
    }
    if (count && compared) printf("half panels: %u cases held against the far view's panels, %u against the picture's own mask\n", compared, seamless);
    oracles_sdl_gpu_device_release();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

/* Under `name`, at four times on 1024 x 576, a frame of 256 x 144 with Link's layer apart, 8 x 8 at (x, 60) and a quarter
 * of a pixel right of the band, over a ground of blue; presented after a frame with the near view's panels at half the
 * game's scale (`panels_first`), or alone.  NULL where the GPU renderer cannot be made. */
static SDL_Surface *link_after_panels(const char *name, int panels_first, int x)
{
    static uint32_t pixels[256 * 144], layer[16 * 16], patch[16 * 16], panels[512 * 288];
    for (int i = 0; i < 256 * 144; i++) pixels[i] = 0xff4060a0u;
    for (int i = 0; i < 16 * 16; i++) { layer[i] = (i % 16) < 8 && i / 16 < 8 ? 0xffe0b020u : 0u; patch[i] = 0xff4060a0u; }
    memset(panels, 0, sizeof panels);
    for (int y = 6; y < 22; y++) for (int px = 6; px < 30; px++) panels[y * 512 + px] = 0xffffffffu;
    SDL_Window *window = SDL_CreateWindow("cells", 1024, 576, 0);
    SDL_Renderer *renderer = window ? oracles_sdl_gpu_renderer(window, 0) : NULL;
    SDL_Surface *shown = NULL;
    oracles_sdl_filter_start(name);
    oracles_sdl_options options = { 0 };
    options.scale = 4;
    options.window = window;
    options.renderer = renderer;
    oracles_host_backend backend;
    if (renderer && oracles_sdl_backend_init(&backend, &options)) {
        if (backend.start(backend.opaque, 256, 144, 48000, 0)) {
            oracles_host_video_subpixel sub;
            memset(&sub, 0, sizeof sub);
            sub.band_height = 144;
            if (panels_first) {
                sub.panels = panels; sub.panels_count = 1; sub.panels_scale = 2;
                sub.panels_rects[0][0] = 6; sub.panels_rects[0][1] = 6; sub.panels_rects[0][2] = 24; sub.panels_rects[0][3] = 16;
                const oracles_host_video_frame first = { pixels, 256, 144, 256 * sizeof(uint32_t), 0, 0, 0, 0, &sub, 0, 0, 0, 0, 0, 0, 0u };
                CHECK(backend.present_frame(backend.opaque, &first));
            }
            memset(&sub, 0, sizeof sub);
            sub.band_height = 144;
            sub.layer = layer; sub.layer_x = x; sub.layer_y = 60; sub.layer_w = 10; sub.layer_h = 10; sub.layer_pitch = 16;
            sub.layer_fx = 64; sub.patch = patch;
            const oracles_host_video_frame next = { pixels, 256, 144, 256 * sizeof(uint32_t), 0, 0, 0, 0, &sub, 0, 0, 0, 0, 0, 0, 0u };
            CHECK(backend.present_frame(backend.opaque, &next));
            SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
            shown = picture(renderer);
            backend.stop(backend.opaque);
        } else CHECK(0);
        oracles_sdl_backend_release(&backend);
    }
    oracles_sdl_filter_start("off");
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    return shown;
}

/* The near view's panels set the shader's masks on the picture's pixels for themselves only: Link drawn apart through the
 * same state at the next frame, at a fraction of a pixel, keeps his own grid, as in a frame without panels before. */
static void cells_reset(void)
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) { printf("cells reset: no offscreen video, skipped\n"); return; }
    static const char *const names[3] = { "crt", "lcd", "scanlines" };
    for (int i = 0; i < 3 * 4; i++) {
        SDL_Surface *after = link_after_panels(names[i / 4], 1, 100 + i % 4), *alone = link_after_panels(names[i / 4], 0, 100 + i % 4);
        if (!after && !i) { printf("cells reset: no GPU renderer here (%s), skipped\n", SDL_GetError()); break; }
        const long off = differing_by(after, alone, 2);
        if (off) fprintf(stderr, "cells reset %s at %d: %ld pixels off\n", names[i / 4], 100 + i % 4, off);
        CHECK(after && alone && !off);
        SDL_DestroySurface(after);
        SDL_DestroySurface(alone);
    }
    oracles_sdl_gpu_device_release();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

int main(int argc, char **argv)
{
#if !defined(_WIN32) && !defined(__APPLE__)
    /* SDL's zenity is the first on the PATH, which the fake one heads, as SDL's own environment has it. */
    char path[4096];
    if (argc > 1) {
        snprintf(path, sizeof path, "%s:%s", argv[1], getenv("PATH") ? getenv("PATH") : "/usr/bin:/bin");
        SDL_setenv_unsafe("PATH", path, 1);
        SDL_SetHint(SDL_HINT_FILE_DIALOG_DRIVER, "zenity");
    }
#else
    (void)argc; (void)argv;
#endif
    SDL_SetMemoryFunctions(test_malloc, test_calloc, test_realloc, test_free);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    SDL_Window *window = SDL_CreateWindow("home", 1280, 720, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, SDL_SOFTWARE_RENDERER) : NULL;
    if (!renderer) { fprintf(stderr, "window: %s\n", SDL_GetError()); return 1; }
    const SDL_WindowID id = SDL_GetWindowID(window);
#if !defined(_WIN32) && !defined(__APPLE__)
    if (argc > 1) dialogs(argv[1]);
#endif
    if (argc > 1) copies(argv[1]);

    /* The names settings.txt has always kept for buttons (the defaults, the hotkeys', Controls' captures) are SDL 3's. */
    static const char *const names[] = { "a", "b", "x", "y", "back", "start", "leftshoulder", "rightshoulder", "leftstick", "rightstick", "guide" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        const SDL_GamepadButton button = SDL_GetGamepadButtonFromString(names[i]);
        CHECK(button != SDL_GAMEPAD_BUTTON_INVALID && !strcmp(SDL_GetGamepadStringForButton(button), names[i]));
    }

    /* No audio driver by that name: the loan fails before touching the window. */
    CHECK(lend(window, renderer, "none") == 0);
    CHECK(intact(window, id, renderer));
    CHECK(!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO));

    /* The dummy driver: the session starts, and its stop quits only the audio it started. */
    CHECK(lend(window, renderer, "dummy") == 1);
    CHECK(intact(window, id, renderer));
    background_audio(window, renderer);
    CHECK(intact(window, id, renderer));
    CHECK(!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO));
    CHECK(SDL_WasInit(SDL_INIT_GAMEPAD) & SDL_INIT_GAMEPAD);

    /* A failure again after a session that ran. */
    CHECK(lend(window, renderer, "none") == 0);
    CHECK(intact(window, id, renderer));

    /* A controller at the start; unplugged during a session, which consumes the removal; another plugged in there. */
    const SDL_JoystickID first = attach_gamepad(0, 0);
    OraclesSdlPads pads = { { NULL }, 0 };
    oracles_sdl_pads_open(&pads);
    CHECK(first && pads.count == 1 && SDL_GamepadConnected(pads.pad[0]));
    SDL_DetachVirtualJoystick(first);
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    CHECK(pads.count == 1 && !SDL_GamepadConnected(pads.pad[0]));
    const SDL_JoystickID second = attach_gamepad(0, 0);
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    oracles_sdl_pads_open(&pads);
    CHECK(second && pads.count == 1 && SDL_GamepadConnected(pads.pad[0]) && SDL_GetGamepadID(pads.pad[0]) == second);
    /* Two plugged in: both open, opened once each however often asked. */
    const SDL_JoystickID third = attach_gamepad(0, 0);
    oracles_sdl_pads_open(&pads);
    oracles_sdl_pads_open(&pads);
    CHECK(third && pads.count == 2);
    /* Unplugged with none left: the handles go. */
    SDL_DetachVirtualJoystick(second);
    SDL_DetachVirtualJoystick(third);
    oracles_sdl_pads_open(&pads);
    CHECK(pads.count == 0);
    oracles_sdl_pads_close(&pads);
    /* The hotbar's device: a controller's from the start where a gamepad was open when the game started (a handheld's own
     * controls), the keyboard's otherwise; then the last slot key's, whatever was open at the start. */
    CHECK(pads_at_start(window, renderer) == 0u);
    const SDL_JoystickID handheld = attach_gamepad(0, 0);
    CHECK(handheld && pads_at_start(window, renderer) == 1u);
    SDL_DetachVirtualJoystick(handheld);
    OraclesHotkeysDevice device = { 0, 0, 0 };
    oracles_hotkeys_device_start(&device, 0u);
    CHECK(!device.controller);
    device = (OraclesHotkeysDevice){ 0, 0, 0 };
    oracles_hotkeys_device_start(&device, 1u);
    CHECK(device.controller);
    oracles_hotkeys_device_key(&device, 0);
    oracles_hotkeys_device_start(&device, 1u);
    CHECK(!device.controller);
    oracles_hotkeys_device_key(&device, 1);
    CHECK(device.controller);
    device = (OraclesHotkeysDevice){ 0, 0, 0 };
    oracles_hotkeys_device_key(&device, 0);   /* a key before the start is known: the key's device */
    oracles_hotkeys_device_start(&device, 2u);
    CHECK(!device.controller);


    /* In play, "a" is the south button of a pad like Xbox's, and the button labelled A, on the right, of a Nintendo
     * Switch Pro Controller (057e:2009), as SDL 2 had it; "b" the other. */
    unsigned south = 0, east = 0;
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    touches(window, renderer);
    vibration(window, renderer);
    enlarged_menus(window, renderer);
    subpixel_band(window, renderer);
    pause_image(window, renderer);
    pause_subpixel_again(window, renderer);
    smooth_layer(window, renderer);
    smooth_fringe(window, renderer);
    pause_menus(window, renderer);
    pause_settings(window, renderer);
    scaling(window, renderer);
    margins(window, renderer);
    enlarged_box(window, renderer);
    face_buttons(window, renderer, 0, 0, NULL, &south, &east);
    CHECK(south == ORACLES_KEY_A && east == ORACLES_KEY_B);
    /* In play, with two gamepads plugged in, the buttons of the second are the game's too, as the first's: SDL gives a
     * pad that is not open as keys (on Android, its A as Enter, the game's Start). */
    CHECK(second_pad_buttons(window, renderer) == (ORACLES_KEY_A | (ORACLES_KEY_B << 8)));
    face_buttons(window, renderer, 0x057e, 0x2009, NULL, &south, &east);
    CHECK(south == ORACLES_KEY_B && east == ORACLES_KEY_A);
    /* An 8BitDo SN30 Pro known by SDL 3's database line, of a standard type: its joystick button 0, labelled A, is "a". */
    face_buttons(window, renderer, 0x2dc8, 0x6101, "8BitDo SN30 Pro,a:b1,b:b0,back:b10,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,"
                 "dpup:h0.1,leftshoulder:b6,leftstick:b13,lefttrigger:b8,leftx:a0,lefty:a1,rightshoulder:b7,rightstick:b14,"
                 "righttrigger:b9,rightx:a3,righty:a4,start:b11,x:b4,y:b3,hint:!SDL_GAMECONTROLLER_USE_BUTTON_LABELS:=1,", &south, &east);
    CHECK(south == ORACLES_KEY_A && east == ORACLES_KEY_B);
    CHECK(intact(window, id, renderer));

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    filter_layer();
    SDL_Quit();
    filter_panels();
    SDL_Quit();
    half_panels();
    SDL_Quit();
    cells_reset();
    SDL_Quit();
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("oracles-launcher-sdl-backend: ok\n");
    return 0;
}
