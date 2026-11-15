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
 * a cancel; it opens in the ROM's folder. */
#include "backends.h"
#include "file_dialog.h"
#include "text_box_enlarge.h"
#include "touch_controls.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        oracles_host_video_subpixel sub = { 128, 0, 16, 254, layer, 20, 40, 1, 2, 64, 0, 0, patch, 1, { { 10, 90, 1, 20 } } };
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
        /* The frame the pause keeps (its texture) has him at his whole place. */
        SDL_Window *pw; SDL_Renderer *pr; SDL_Texture *kept; int kw = 0, kh = 0;
        oracles_sdl_backend_pause_view(&backend, &pw, &pr, &kept, &kw, &kh);
        SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        SDL_RenderClear(renderer);
        const SDL_FRect whole = { 0.0f, 0.0f, 480.0f, 270.0f };
        CHECK(SDL_RenderTexture(renderer, kept, &whole, &whole));
        CHECK(pixel_at(renderer, 20, 40) == 0xff00ff00u && pixel_at(renderer, 20, 41) == 0xff00ff00u && pixel_at(renderer, 10, 100) == 0xffffffffu);
        SDL_SetRenderLogicalPresentation(renderer, 480, 270, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
        const oracles_host_video_subpixel high = { 0, 0, 16, 254, layer, 20, 15, 1, 2, 64, 0, 0, patch, 0, { { 0 } } };
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
        const oracles_host_video_subpixel smooth = { 128, 64, 16, 254, white_layer, 20, 40, 5, 3, 64, -64, 32, white_patch, 0, { { 0 } } };
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
    const oracles_host_video_subpixel sub = { 128, 0, 16, 128, layer, 20, 40, 2, 2, 64, -128, 0, patch, 0, { { 0 } } };
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
            const oracles_host_video_subpixel sub = { 128, 192, 16, 254, layer, 20, 40, 2, 2, 64, 64, -32, patch, 1, { { 100, 200, 40, 20 } } };
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
        const oracles_host_video_subpixel half = { 128, 0, 0, 144, NULL, 0, 0, 0, 0, 0, 0, 0, NULL, 1, { { 40, 20, 40, 16 } } };
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

/* The home screen's window and renderer still work: the window found by its id, SDL's video on, a frame drawn. */
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


    /* In play, "a" is the south button of a pad like Xbox's, and the button labelled A, on the right, of a Nintendo
     * Switch Pro Controller (057e:2009), as SDL 2 had it; "b" the other. */
    unsigned south = 0, east = 0;
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    touches(window, renderer);
    vibration(window, renderer);
    enlarged_menus(window, renderer);
    subpixel_band(window, renderer);
    pause_image(window, renderer);
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
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("oracles-launcher-sdl-backend: ok\n");
    return 0;
}
