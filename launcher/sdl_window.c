/* The SDL 3 backend's windows and screens (backends.h): the window's flags and its size on the display, the screen's
 * shape, the renderer, and the gamepads plugged in. */
#include "sdl_backend_internal.h"
#include "sdl_filter.h"

uint64_t oracles_sdl_window_flags(void)
{
    /* A Retina display has two pixels a point, which a window that does not ask for them gets stretched from one, text
     * and pixels blurred (with NSHighResolutionCapable, which the executable's embedded Info.plist says).  Sizes stay
     * in points, the renderer has the pixels.  Windows and X11 count a window in pixels already. */
#ifdef __APPLE__
    return SDL_WINDOW_HIGH_PIXEL_DENSITY;
#else
    return 0;
#endif
}

int oracles_sdl_fullscreen_only(void)
{
#ifdef __ANDROID__
    return 1;
#else
    return 0;
#endif
}

int oracles_sdl_screen_size(struct SDL_Window *window, unsigned *width, unsigned *height)
{
    /* The display's desktop mode, a Retina display's points by its density. */
    *width = *height = 0;
    if (!SDL_WasInit(SDL_INIT_VIDEO) && !SDL_InitSubSystem(SDL_INIT_VIDEO)) return 0;
    const SDL_DisplayID display = window ? SDL_GetDisplayForWindow(window) : SDL_GetPrimaryDisplay();
    const SDL_DisplayMode *mode = display ? SDL_GetDesktopDisplayMode(display) : NULL;
    if (!mode || mode->w <= 0 || mode->h <= 0) return 0;
    const float density = mode->pixel_density > 0.0f ? mode->pixel_density : 1.0f;
    *width = (unsigned)((float)mode->w * density + 0.5f);
    *height = (unsigned)((float)mode->h * density + 0.5f);
    return 1;
}

int oracles_sdl_screen_4_3(struct SDL_Window *window)
{
    /* The display's shape, as the view takes it (oracles_enhanced_screen_aspect). */
    unsigned w, h;
    if (!oracles_sdl_screen_size(window, &w, &h)) return 0;
    return oracles_enhanced_screen_aspect(w, h) == ORACLES_ENHANCED_4_3;
}

float oracles_sdl_pixel_ratio(struct SDL_Window *window)
{
    const float density = window ? SDL_GetWindowPixelDensity(window) : 0.0f;
    return density > 0.0f ? density : 1.0f;
}

float oracles_sdl_point_scale(struct SDL_Window *window)
{
    /* SDL 2, with the DPI scaling it was asked for on Windows, counted a window in points there, 4 for 5 pixels at
     * 125 %; SDL 3 counts it in pixels.  The launcher's own sizes keep SDL 2's points, settings.txt's included: its
     * window opens as large as before on a scaled display.  Elsewhere SDL 3 counts as SDL 2 did. */
#ifdef _WIN32
    const float scale = window ? SDL_GetWindowDisplayScale(window) : SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    return scale > 0.0f ? scale : 1.0f;
#else
    (void)window;
    return 1.0f;
#endif
}

struct SDL_Window *oracles_sdl_create_window(int width, int height, uint64_t flags)
{
    const SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetStringProperty(properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "The Oracles Project");
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width);
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, (Sint64)flags);
    SDL_Window *window = SDL_CreateWindowWithProperties(properties);
    SDL_DestroyProperties(properties);
#if !defined(__APPLE__) && !defined(__ANDROID__)
    /* The application's icon on the window and in the taskbar; macOS takes a bundle's, Android the manifest's. */
    SDL_Surface *icon = window ? SDL_CreateSurfaceFrom(ORACLES_WINDOW_ICON_SIZE, ORACLES_WINDOW_ICON_SIZE, SDL_PIXELFORMAT_RGBA32,
                                                       (void *)oracles_window_icon, ORACLES_WINDOW_ICON_SIZE * 4) : NULL;
    if (icon) {
        SDL_SetWindowIcon(window, icon);
        SDL_DestroySurface(icon);
    }
#endif
    return window;
}

struct SDL_Renderer *oracles_sdl_create_renderer(struct SDL_Window *window, int vsync)
{
    /* ORACLES_RENDERER=gpu: SDL's GPU renderer, which takes the filters' shaders; the usual renderer when it cannot be
     * made (no Vulkan, Direct3D 12 or Metal device). */
    SDL_Renderer *renderer = NULL;
    if (oracles_sdl_gpu_wanted()) {
        renderer = oracles_sdl_gpu_renderer(window, vsync);
        if (!renderer && oracles_sdl_window_away(window)) {
            /* Not the device's doing: the filters stay offered, a later choice tries again. */
            fprintf(stderr, "oracles: no GPU renderer while the app is in the background (%s): the usual one instead\n", SDL_GetError());
        } else if (!renderer) {
            fprintf(stderr, "oracles: no GPU renderer (%s): the usual one instead\n", SDL_GetError());
            oracles_sdl_filters_refused();
        }
    }
    if (!renderer) renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) renderer = SDL_CreateRenderer(window, SDL_SOFTWARE_RENDERER);
    if (renderer && vsync) SDL_SetRenderVSync(renderer, 1);
    oracles_sdl_renderer_report(renderer, "window");
    return renderer;
}

static void pads_drop(OraclesSdlPads *pads, unsigned i)
{
    SDL_CloseGamepad(pads->pad[i]);
    pads->pad[i] = pads->pad[--pads->count];
    pads->pad[pads->count] = NULL;
}

static void pads_take(OraclesSdlPads *pads, SDL_JoystickID id)
{
    for (unsigned i = 0; i < pads->count; i++) if (SDL_GetGamepadID(pads->pad[i]) == id) return;
    if (pads->count >= ORACLES_SDL_PADS) return;
    SDL_Gamepad *pad = SDL_OpenGamepad(id);
    if (pad) pads->pad[pads->count++] = pad;
}

void oracles_sdl_pads_open(OraclesSdlPads *pads)
{
    for (unsigned i = pads->count; i-- > 0;) if (!SDL_GamepadConnected(pads->pad[i])) pads_drop(pads, i);
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    for (int i = 0; i < count; i++) pads_take(pads, ids[i]);
    SDL_free(ids);
}

void oracles_sdl_pads_event(OraclesSdlPads *pads, const SDL_Event *event)
{
    if (event->type == SDL_EVENT_GAMEPAD_ADDED) pads_take(pads, event->gdevice.which);
    else if (event->type == SDL_EVENT_GAMEPAD_REMOVED)
        for (unsigned i = 0; i < pads->count; i++) if (SDL_GetGamepadID(pads->pad[i]) == event->gdevice.which) { pads_drop(pads, i); break; }
}

void oracles_sdl_pads_close(OraclesSdlPads *pads)
{
    while (pads->count) pads_drop(pads, pads->count - 1u);
}

void oracles_sdl_pads_describe(const OraclesSdlPads *pads, char *out, size_t capacity)
{
    size_t used = (size_t)snprintf(out, capacity, "%u open", pads->count);
    for (unsigned i = 0; i < pads->count && used < capacity; i++) {
        const char *name = SDL_GetGamepadName(pads->pad[i]);
        used += (size_t)snprintf(out + used, capacity - used, "%s%s", i ? ", " : ": ", name ? name : "unnamed");
    }
}

int oracles_sdl_pad_button(const SDL_Event *event)
{
    /* SDL 2 named a Nintendo controller's face buttons by their labels (SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, on
     * by default); SDL 3 names them by their places, "a" being the south button whatever it reads.  The names of
     * settings.txt keep SDL 2's meaning: a face button labelled with a letter is that letter's, as on an Xbox pad. */
    /* The label from the gamepad's own mapping when it is open: a pad SDL knows by a line of its database (8BitDo, a
     * SNES pad) has a standard type whose labels would be Xbox's. */
    const SDL_GamepadButton button = (SDL_GamepadButton)event->gbutton.button;
    SDL_Gamepad *gamepad = SDL_GetGamepadFromID(event->gbutton.which);
    const SDL_GamepadButtonLabel label = gamepad ? SDL_GetGamepadButtonLabel(gamepad, button)
                                                 : SDL_GetGamepadButtonLabelForType(SDL_GetGamepadTypeForID(event->gbutton.which), button);
    switch (label) {
        case SDL_GAMEPAD_BUTTON_LABEL_A: return SDL_GAMEPAD_BUTTON_SOUTH;
        case SDL_GAMEPAD_BUTTON_LABEL_B: return SDL_GAMEPAD_BUTTON_EAST;
        case SDL_GAMEPAD_BUTTON_LABEL_X: return SDL_GAMEPAD_BUTTON_WEST;
        case SDL_GAMEPAD_BUTTON_LABEL_Y: return SDL_GAMEPAD_BUTTON_NORTH;
        default: return button;
    }
}

void oracles_sdl_size_window(struct SDL_Window *window, int width, int height)
{
    /* Sizes are in points where the display has more pixels than points (a Retina display: 1 point for 2 pixels): the
     * points that give at least those pixels, one more where the display's rounding falls short, so that the
     * renderer's integer scale is the one asked for.  Elsewhere points are pixels and nothing changes.  SDL 3 may
     * apply a size after the call returns: it is waited for before it is read back. */
    const float ratio = oracles_sdl_pixel_ratio(window);
    int w = (int)ceilf((float)width / ratio - 0.001f), h = (int)ceilf((float)height / ratio - 0.001f);
    SDL_SetWindowSize(window, w, h);
    SDL_SyncWindow(window);
    int pixel_w, pixel_h;
    if (ratio != 1.0f && SDL_GetWindowSizeInPixels(window, &pixel_w, &pixel_h) && (pixel_w < width || pixel_h < height)) {
        SDL_SetWindowSize(window, w + (pixel_w < width), h + (pixel_h < height));
        SDL_SyncWindow(window);
    }
}

void oracles_sdl_place_window(struct SDL_Window *window, int centre_x, int centre_y)
{
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    int x = centre_x - w / 2, y = centre_y - h / 2;
    SDL_Rect usable;
    const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (display && SDL_GetDisplayUsableBounds(display, &usable)) {
        if (x + w > usable.x + usable.w) x = usable.x + usable.w - w;
        if (y + h > usable.y + usable.h) y = usable.y + usable.h - h;
        if (x < usable.x) x = usable.x;
        if (y < usable.y) y = usable.y;
    }
    SDL_SetWindowPosition(window, x, y);
}
