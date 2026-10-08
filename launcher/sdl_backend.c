/* SDL 3 backend: window with integer scaling, keyboard, gamepad, audio.
 * Keyboard by default: arrows, X = A, Z = B, Return = Start, Backspace =
 * Select; F2 = colour correction, F3 = Enhanced wide/framed, F5 = save state, F7 = load state,
 * F11 = fullscreen, Escape = quit, or the pause menu for a game the home screen started.
 * Controller by default: d-pad or left stick, A, B, Start, Back = Select.
 * Both are rebindable through oracles_sdl_options. */
#include "backends.h"
#include "touch_sdl.h"
#include "window_icon.h"

#include <SDL3/SDL.h>

#include <math.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PENDING_EVENTS 16u
/* The device starts once this much audio is queued, so that a device pull
 * never finds the queue empty during the first frames; above the limit the
 * host is ahead of the device and the chunk is dropped. */
#define AUDIO_PREBUFFER_MS 50u
#define AUDIO_QUEUE_LIMIT_MS 1000u /* only reached if the device stalls; rate control holds the queue near its target */
#define AXIS_THRESHOLD 12000

const char *const oracles_sdl_binding_names[ORACLES_BINDINGS] = { "right", "left", "up", "down", "a", "b", "select", "start" };
const char *const oracles_sdl_default_key_names[ORACLES_BINDINGS] = { "Right", "Left", "Up", "Down", "X", "Z", "Backspace", "Return" };
const char *const oracles_sdl_default_pad_names[4] = { "a", "b", "back", "start" };

typedef struct sdl_backend {
    SDL_Keycode keys[ORACLES_BINDINGS];
    SDL_GamepadButton pad[4];   /* A, B, Select, Start */
    SDL_Keycode hotkey_keys[ORACLES_SDL_HOTKEY_SLOTS], hotkey_bind_b, hotkey_bind_a;   /* SDLK_UNKNOWN: none */
    SDL_GamepadButton hotkey_pad[ORACLES_SDL_HOTKEY_SLOTS];
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    int frame_width, frame_height;   /* the texture's: the session's surface */
    int logical_w, logical_h;        /* the renderer's logical size: the surface's, or the part shown alone */
    int pause_menu;         /* Escape pauses rather than quits */
    int borrowed;           /* the window and the renderer are the launcher's: known from the start, never destroyed here */
    int subsystems;         /* the audio and controller subsystems a borrowed window started, to quit at the end */
    int window_closed;
    OraclesTouchScreen touch;   /* the game's buttons on a touch screen (Android), and those its fingers press */
    unsigned touch_buttons;
    int touch_enabled;
    void (*on_suspend)(void *);   /* the host's store at a suspension (watch_suspend), and its argument */
    void *on_suspend_opaque;
    int suspended;   /* the watch saw a suspension, which the next poll hands to the host */
    SDL_AudioStream *audio;          /* bound to the device, which pulls from it */
    OraclesSdlPads pads;               /* every gamepad plugged in (oracles_sdl_pads_open) */
    uint32_t sample_rate_hz;
    int audio_started;
    unsigned audio_underruns;
    unsigned audio_drops;
    uint64_t last_chunk_ns;          /* when the previous chunk was queued, and what the queue held after it */
    Uint32 last_chunk_queued_bytes;
    uint64_t last_present_ns_spent;  /* the duration of the last SDL_RenderPresent */
    /* A game frame's presentation by step, summed and at most: the texture's upload, the drawing, SDL_RenderPresent. */
    uint64_t step_ns_total[3], step_ns_max[3];
    unsigned stepped_presents;
    oracles_sdl_underrun underrun_log[ORACLES_SDL_UNDERRUNS_LOGGED];
    Uint32 audio_peak_bytes;
    uint64_t audio_queued_sum_bytes;
    uint64_t audio_queued_samples_count;
    unsigned scale;
    int fullscreen;
    int vsync;
    unsigned presents;
    uint64_t first_present_ns, last_present_ns;
    uint64_t hold_started_ns, held_ns;   /* the pauses between two presents: no present waited then */
    char renderer_name[32];   /* the SDL renderer the session presents with, and whether it said it had vsync */
    int renderer_vsync;
    unsigned axis_buttons;  /* ORACLES_KEY_* currently held through the left stick */
    oracles_host_event pending[PENDING_EVENTS];
    unsigned pending_count;
} sdl_backend;

static void push_button(sdl_backend *backend, unsigned button, int pressed)
{
    if (backend->pending_count >= PENDING_EVENTS) return;
    oracles_host_event *event = &backend->pending[backend->pending_count++];
    event->type = ORACLES_HOST_EVENT_BUTTON;
    event->button = button;
    event->pressed = pressed;
}

static void push_event(sdl_backend *backend, oracles_host_event_type type)
{
    if (backend->pending_count >= PENDING_EVENTS) return;
    oracles_host_event *event = &backend->pending[backend->pending_count++];
    memset(event, 0, sizeof *event);
    event->type = type;
}

static void push_quit(sdl_backend *backend) { push_event(backend, ORACLES_HOST_EVENT_QUIT); }

static void push_command(sdl_backend *backend, int command)
{
    if (backend->pending_count >= PENDING_EVENTS) return;
    oracles_host_event *event = &backend->pending[backend->pending_count++];
    memset(event, 0, sizeof *event);
    event->type = ORACLES_HOST_EVENT_COMMAND;
    event->command = command;
}

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

/* Android's Back is Escape there (SDLK_AC_BACK); elsewhere, a media keyboard's Back is a key like any other. */
#ifdef __ANDROID__
#define ANDROID_BACK(key) ((key) == SDLK_AC_BACK)
#else
#define ANDROID_BACK(key) 0
#endif

int oracles_sdl_fullscreen_only(void)
{
#ifdef __ANDROID__
    return 1;
#else
    return 0;
#endif
}

int oracles_sdl_screen_4_3(struct SDL_Window *window)
{
    /* The display's desktop mode, its shape nearer 4:3 (1.33) than 16:9 (1.78): below their middle, 1.56.  A screen
     * held upright (a phone) counts by its long side over its short one. */
    if (!SDL_WasInit(SDL_INIT_VIDEO) && !SDL_InitSubSystem(SDL_INIT_VIDEO)) return 0;
    const SDL_DisplayID display = window ? SDL_GetDisplayForWindow(window) : SDL_GetPrimaryDisplay();
    const SDL_DisplayMode *mode = display ? SDL_GetDesktopDisplayMode(display) : NULL;
    if (!mode || mode->w <= 0 || mode->h <= 0) return 0;
    const int long_side = mode->w > mode->h ? mode->w : mode->h, short_side = mode->w > mode->h ? mode->h : mode->w;
    return (float)long_side / (float)short_side < (4.0f / 3.0f + 16.0f / 9.0f) / 2.0f;
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
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) renderer = SDL_CreateRenderer(window, SDL_SOFTWARE_RENDERER);
    if (renderer && vsync) SDL_SetRenderVSync(renderer, 1);
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

/* The launcher's window takes the session's size, the surface's integer scale kept whole on the display, around its
 * centre; its smallest size becomes the surface's. */
static int borrow_window(sdl_backend *backend, uint32_t width, uint32_t height)
{
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) return 0;
    backend->subsystems = 1;
    const float ratio = oracles_sdl_pixel_ratio(backend->window);
    SDL_SetWindowMinimumSize(backend->window, (int)ceilf((float)width / ratio), (int)ceilf((float)height / ratio));
    if (SDL_GetWindowFlags(backend->window) & SDL_WINDOW_MAXIMIZED) SDL_RestoreWindow(backend->window);
    if (backend->fullscreen) SDL_SetWindowFullscreen(backend->window, true);
    SDL_SyncWindow(backend->window);   /* the window's state read below is the one asked for */
    if (!(SDL_GetWindowFlags(backend->window) & SDL_WINDOW_FULLSCREEN)) {
        int x, y, w, h;
        SDL_GetWindowPosition(backend->window, &x, &y);
        SDL_GetWindowSize(backend->window, &w, &h);
        oracles_sdl_size_window(backend->window, (int)(width * backend->scale), (int)(height * backend->scale));
        oracles_sdl_place_window(backend->window, x + w / 2, y + h / 2);
    }
    SDL_SetRenderVSync(backend->renderer, backend->vsync ? 1 : 0);
    SDL_SetRenderClipRect(backend->renderer, NULL);
    SDL_SetRenderDrawBlendMode(backend->renderer, SDL_BLENDMODE_NONE);
    return 1;
}

static int sdl_start(void *opaque, uint32_t width, uint32_t height, uint32_t sample_rate_hz, int audio_enabled)
{
    sdl_backend *backend = opaque;
    if (width > INT_MAX / 8 || height > INT_MAX / 8) return 0;
    if (backend->borrowed) {
        if (!borrow_window(backend, width, height)) return 0;
    } else {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) return 0;
        SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | oracles_sdl_window_flags();
        if (backend->fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
        backend->window = oracles_sdl_create_window((int)(width * backend->scale), (int)(height * backend->scale), flags);
        if (!backend->window) return 0;
        /* Without vsync the host paces frames at the exact Game Boy rate, 59.7275 Hz;
         * with it, the display paces (a 60 Hz display runs the game 0.45% fast, which
         * the audio rate control absorbs), and a continuous scroll has no periodic
         * hiccup from the two clocks beating.  A present that does not wait is
         * caught by the host, which then turns vsync off (sdl_present_unpaced). */
        backend->renderer = oracles_sdl_create_renderer(backend->window, backend->vsync);
        if (!backend->renderer) return 0;
        /* Created in points: --scale N is N times the surface in pixels on a Retina display too. */
        if (!backend->fullscreen && oracles_sdl_pixel_ratio(backend->window) != 1.0f) {
            oracles_sdl_size_window(backend->window, (int)(width * backend->scale), (int)(height * backend->scale));
            SDL_SetWindowPosition(backend->window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        }
    }
    /* Its own renderer or the home screen's, whose vsync borrow_window has set. */
    int vsync = 0;
    SDL_snprintf(backend->renderer_name, sizeof backend->renderer_name, "%s", SDL_GetRendererName(backend->renderer));
    backend->renderer_vsync = SDL_GetRenderVSync(backend->renderer, &vsync) && vsync != 0;
    SDL_SetRenderLogicalPresentation(backend->renderer, (int)width, (int)height, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderDrawColor(backend->renderer, 0, 0, 0, 255);
    backend->texture = SDL_CreateTexture(backend->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                         (int)width, (int)height);
    if (!backend->texture) return 0;
    backend->frame_width = (int)width;
    backend->frame_height = (int)height;
    backend->logical_w = (int)width;
    backend->logical_h = (int)height;
    SDL_SetTextureScaleMode(backend->texture, SDL_SCALEMODE_NEAREST);
    oracles_sdl_pads_open(&backend->pads);
    {
        char pads[256];
        oracles_sdl_pads_describe(&backend->pads, pads, sizeof pads);
        fprintf(stderr, "oracles: game controllers: %s\n", pads);
    }
    if (!audio_enabled) return 1;
    /* The device's buffer SDL 2 was asked for, 512 frames; the stream converts to what the device plays. */
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "512");
    const SDL_AudioSpec wanted = { SDL_AUDIO_S16, 2, (int)sample_rate_hz };
    backend->audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &wanted, NULL, NULL);
    if (!backend->audio) return 0;
    backend->sample_rate_hz = sample_rate_hz;
    /* Paused until the prebuffer is queued, as SDL opens it; see sdl_queue_audio. */
    return 1;
}

static Uint32 audio_bytes_for_ms(const sdl_backend *backend, unsigned ms)
{
    return (Uint32)((uint64_t)backend->sample_rate_hz * 2u * sizeof(int16_t) * ms / 1000u);
}

static int translate_key(const sdl_backend *backend, SDL_Keycode key, unsigned *button)
{
    for (unsigned i = 0; i < ORACLES_BINDINGS; i++) {
        if (backend->keys[i] != SDLK_UNKNOWN && backend->keys[i] == key) { *button = 1u << i; return 1; }
    }
    return 0;
}

static int translate_controller_button(const sdl_backend *backend, SDL_GamepadButton button, unsigned *out)
{
    static const unsigned pad_buttons[4] = { ORACLES_KEY_A, ORACLES_KEY_B, ORACLES_KEY_SELECT, ORACLES_KEY_START };
    switch (button) {
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: *out = ORACLES_KEY_RIGHT; return 1;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: *out = ORACLES_KEY_LEFT; return 1;
        case SDL_GAMEPAD_BUTTON_DPAD_UP: *out = ORACLES_KEY_UP; return 1;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: *out = ORACLES_KEY_DOWN; return 1;
        default: break;
    }
    for (unsigned i = 0; i < 4; i++) {
        if (backend->pad[i] != SDL_GAMEPAD_BUTTON_INVALID && backend->pad[i] == button) { *out = pad_buttons[i]; return 1; }
    }
    return 0;
}

/* Resolves the names of the options; an unknown name keeps the default and is reported once, an empty one leaves
 * the button without a key. */
static void resolve_bindings(sdl_backend *backend, const oracles_sdl_options *options)
{
    for (unsigned i = 0; i < ORACLES_BINDINGS; i++) {
        const char *wanted = options && options->key_names[i] ? options->key_names[i] : oracles_sdl_default_key_names[i];
        if (!wanted[0]) { backend->keys[i] = SDLK_UNKNOWN; continue; }
        SDL_Keycode key = SDL_GetKeyFromName(wanted);
        if (key == SDLK_UNKNOWN) {
            SDL_Log("unknown key name '%s' for %s; using %s", wanted, oracles_sdl_binding_names[i], oracles_sdl_default_key_names[i]);
            key = SDL_GetKeyFromName(oracles_sdl_default_key_names[i]);
        }
        backend->keys[i] = key;
    }
    for (unsigned i = 0; i < 4; i++) {
        const char *wanted = options && options->pad_names[i] ? options->pad_names[i] : oracles_sdl_default_pad_names[i];
        if (!wanted[0]) { backend->pad[i] = SDL_GAMEPAD_BUTTON_INVALID; continue; }
        SDL_GamepadButton button = SDL_GetGamepadButtonFromString(wanted);
        if (button == SDL_GAMEPAD_BUTTON_INVALID) {
            SDL_Log("unknown controller button '%s'; using %s", wanted, oracles_sdl_default_pad_names[i]);
            button = SDL_GetGamepadButtonFromString(oracles_sdl_default_pad_names[i]);
        }
        backend->pad[i] = button;
    }
}

/* The item hotkeys' keys: an unknown or absent name leaves the slot without a key. */
static void resolve_hotkeys(sdl_backend *backend, const oracles_sdl_options *options)
{
    for (unsigned i = 0; i < ORACLES_SDL_HOTKEY_SLOTS; i++) {
        backend->hotkey_keys[i] = options && options->hotkey_key_names[i] ? SDL_GetKeyFromName(options->hotkey_key_names[i]) : SDLK_UNKNOWN;
        backend->hotkey_pad[i] = options && options->hotkey_pad_names[i] ? SDL_GetGamepadButtonFromString(options->hotkey_pad_names[i]) : SDL_GAMEPAD_BUTTON_INVALID;
        /* A key or a button the game already has stays the game's. */
        for (unsigned k = 0; k < ORACLES_BINDINGS; k++) if (backend->hotkey_keys[i] == backend->keys[k]) backend->hotkey_keys[i] = SDLK_UNKNOWN;
        for (unsigned k = 0; k < 4; k++) if (backend->hotkey_pad[i] == backend->pad[k]) backend->hotkey_pad[i] = SDL_GAMEPAD_BUTTON_INVALID;
        if (options && options->hotkey_key_names[i] && !options->hotkey_key_names[i][0]) backend->hotkey_keys[i] = SDLK_UNKNOWN;
        if (options && options->hotkey_pad_names[i] && !options->hotkey_pad_names[i][0]) backend->hotkey_pad[i] = SDL_GAMEPAD_BUTTON_INVALID;
    }
    backend->hotkey_bind_b = options && options->hotkey_bind_b ? SDL_GetKeyFromName(options->hotkey_bind_b) : SDLK_UNKNOWN;
    backend->hotkey_bind_a = options && options->hotkey_bind_a ? SDL_GetKeyFromName(options->hotkey_bind_a) : SDLK_UNKNOWN;
}

static int key_is_down(SDL_Keycode key)
{
    if (key == SDLK_UNKNOWN) return 0;
    const SDL_Scancode code = SDL_GetScancodeFromKey(key, NULL);
    int count = 0;
    const bool *state = SDL_GetKeyboardState(&count);
    return code != SDL_SCANCODE_UNKNOWN && (int)code < count && state[code];
}

static void push_hotkey(sdl_backend *backend, unsigned slot, int pressed, int keyboard)
{
    const unsigned modifier = !keyboard ? 3u : !pressed ? 0u : key_is_down(backend->hotkey_bind_a) ? 2u : key_is_down(backend->hotkey_bind_b) ? 1u : 0u;
    push_command(backend, ORACLES_COMMAND_HOTKEY + (int)(slot * 8u + modifier * 2u + (pressed ? 1u : 0u)));
}

static int translate_hotkey(const sdl_backend *backend, SDL_Keycode key, unsigned *slot)
{
    for (unsigned i = 0; i < ORACLES_SDL_HOTKEY_SLOTS; i++) if (key != SDLK_UNKNOWN && backend->hotkey_keys[i] == key) { *slot = i; return 1; }
    return 0;
}

static int translate_hotkey_button(const sdl_backend *backend, SDL_GamepadButton button, unsigned *slot)
{
    for (unsigned i = 0; i < ORACLES_SDL_HOTKEY_SLOTS; i++)
        if (backend->hotkey_pad[i] != SDL_GAMEPAD_BUTTON_INVALID && backend->hotkey_pad[i] == button) { *slot = i; return 1; }
    return 0;
}

static void update_axis(sdl_backend *backend, unsigned negative, unsigned positive, Sint16 value)
{
    const unsigned wanted = value < -AXIS_THRESHOLD ? negative : value > AXIS_THRESHOLD ? positive : 0;
    const unsigned held = backend->axis_buttons & (negative | positive);
    if (held == wanted) return;
    if (held) push_button(backend, held, 0);
    if (wanted) push_button(backend, wanted, 1);
    backend->axis_buttons = (backend->axis_buttons & ~(negative | positive)) | wanted;
}

static void toggle_fullscreen(sdl_backend *backend)
{
    if (oracles_sdl_fullscreen_only()) return;
    backend->fullscreen = !backend->fullscreen;
    SDL_SetWindowFullscreen(backend->window, backend->fullscreen != 0);
}

/* A finger shows the game's buttons over the game and presses them (touch_controls.h); a key of the game, a controller's
 * button or its stick hide them, not Back, a media key or a pointer.  1 when the event was a finger's. */
static int touch_event(sdl_backend *backend, const SDL_Event *e)
{
    if (!backend->touch_enabled) return 0;
    unsigned now = backend->touch_buttons, button;
    int pause = 0;
    const int finger = oracles_touch_sdl_event(&backend->touch, backend->renderer, e, &now, &pause);
    if (!finger) {
        const int game = (e->type == SDL_EVENT_KEY_DOWN && (translate_key(backend, e->key.key, &button) || translate_hotkey(backend, e->key.key, &button)))
                         || e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
                         || (e->type == SDL_EVENT_GAMEPAD_AXIS_MOTION && (e->gaxis.value > AXIS_THRESHOLD || e->gaxis.value < -AXIS_THRESHOLD));
        if (!backend->touch.touch.shown || !game) return 0;
        oracles_touch_hide(&backend->touch.touch);
        now = 0;
    }
    for (unsigned bit = 1; bit <= ORACLES_KEY_START; bit <<= 1)
        if ((now ^ backend->touch_buttons) & bit) push_button(backend, bit, (now & bit) != 0);
    backend->touch_buttons = now;
    if (pause && backend->pause_menu) push_event(backend, ORACLES_HOST_EVENT_PAUSE);
    return finger;
}

static void handle_event(sdl_backend *backend, const SDL_Event *e)
{
    unsigned button;
    if (touch_event(backend, e)) return;
    /* The window's focus and screen changes, said on the standard error: on a device of two screens, the input follows
     * the screen that has the focus. */
    if (e->type == SDL_EVENT_WINDOW_FOCUS_GAINED || e->type == SDL_EVENT_WINDOW_FOCUS_LOST || e->type == SDL_EVENT_WINDOW_DISPLAY_CHANGED)
        fprintf(stderr, "oracles: game window: %s\n", e->type == SDL_EVENT_WINDOW_FOCUS_GAINED ? "focus gained"
                : e->type == SDL_EVENT_WINDOW_FOCUS_LOST ? "focus lost" : "moved to another display");
    switch (e->type) {
        case SDL_EVENT_QUIT: backend->window_closed = 1; push_quit(backend); break;
        case SDL_EVENT_KEY_DOWN:
            if (e->key.repeat) break;
            if (e->key.key == SDLK_ESCAPE || ANDROID_BACK(e->key.key)) {
                push_event(backend, backend->pause_menu ? ORACLES_HOST_EVENT_PAUSE : ORACLES_HOST_EVENT_QUIT);
                break;
            }
            if (e->key.key == SDLK_F11) { toggle_fullscreen(backend); break; }
            if (e->key.key == SDLK_F2) { push_command(backend, ORACLES_COMMAND_TOGGLE_COLOUR_CORRECTION); break; }
            if (e->key.key == SDLK_F3) { push_command(backend, ORACLES_COMMAND_TOGGLE_ENHANCED); break; }
            if (e->key.key == SDLK_F5) { push_command(backend, ORACLES_COMMAND_SAVE_STATE); break; }
            if (e->key.key == SDLK_F7) { push_command(backend, ORACLES_COMMAND_LOAD_STATE); break; }
            if (translate_key(backend, e->key.key, &button)) push_button(backend, button, 1);
            else if (translate_hotkey(backend, e->key.key, &button)) push_hotkey(backend, button, 1, 1);
            break;
        case SDL_EVENT_KEY_UP:
            if (translate_key(backend, e->key.key, &button)) push_button(backend, button, 0);
            else if (translate_hotkey(backend, e->key.key, &button)) push_hotkey(backend, button, 0, 1);
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            if (translate_controller_button(backend, (SDL_GamepadButton)oracles_sdl_pad_button(e), &button)) push_button(backend, button, e->gbutton.down);
            else if (translate_hotkey_button(backend, (SDL_GamepadButton)oracles_sdl_pad_button(e), &button)) push_hotkey(backend, button, e->gbutton.down, 0);
            break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (e->gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) update_axis(backend, ORACLES_KEY_LEFT, ORACLES_KEY_RIGHT, e->gaxis.value);
            else if (e->gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) update_axis(backend, ORACLES_KEY_UP, ORACLES_KEY_DOWN, e->gaxis.value);
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
            oracles_sdl_pads_event(&backend->pads, e);
            break;
        default: break;   /* a file or a text dropped on the window during the game: SDL keeps and frees its copy */
    }
}

static int sdl_poll_event(void *opaque, oracles_host_event *event, int *has_event)
{
    sdl_backend *backend = opaque;
    SDL_Event e;
    *has_event = 0;
    if (backend->suspended) { backend->suspended = 0; push_event(backend, ORACLES_HOST_EVENT_SUSPEND); }
    while (backend->pending_count == 0 && SDL_PollEvent(&e)) handle_event(backend, &e);
    if (backend->pending_count == 0) return 1;
    *event = backend->pending[0];
    memmove(backend->pending, backend->pending + 1, (backend->pending_count - 1) * sizeof backend->pending[0]);
    backend->pending_count--;
    *has_event = 1;
    return 1;
}

static uint64_t sdl_monotonic_ns(void *opaque);

static int sdl_present_frame(void *opaque, const oracles_host_video_frame *frame)
{
    sdl_backend *backend = opaque;
    if (frame->pitch_bytes > INT_MAX) return 0;
    const uint64_t upload_started_ns = sdl_monotonic_ns(backend);
    if (!SDL_UpdateTexture(backend->texture, NULL, frame->pixels, (int)frame->pitch_bytes)) return 0;
    const uint64_t draw_started_ns = sdl_monotonic_ns(backend);
    /* A part shown alone (the game's menus enlarged) takes its own whole scale: the logical size follows it. */
    const SDL_FRect part = { (float)frame->crop_x, (float)frame->crop_y, (float)frame->crop_w, (float)frame->crop_h };
    const int cropped = frame->crop_w > 0 && frame->crop_h > 0;
    const int logical_w = cropped ? (int)frame->crop_w : backend->frame_width, logical_h = cropped ? (int)frame->crop_h : backend->frame_height;
    if (logical_w != backend->logical_w || logical_h != backend->logical_h) {
        if (!backend->touch.direct) SDL_SetRenderLogicalPresentation(backend->renderer, logical_w, logical_h, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
        backend->logical_w = logical_w;
        backend->logical_h = logical_h;
    }
    if (!oracles_touch_sdl_frame(&backend->touch, backend->renderer, backend->texture, cropped ? &part : NULL)) {   /* the touch controls' own drawing */
        if (!SDL_RenderClear(backend->renderer)) return 0;
        if (!SDL_RenderTexture(backend->renderer, backend->texture, cropped ? &part : NULL, NULL)) return 0;
    }
    const uint64_t present_started_ns = sdl_monotonic_ns(backend);
    SDL_RenderPresent(backend->renderer);
    backend->last_present_ns = sdl_monotonic_ns(backend);
    backend->last_present_ns_spent = backend->last_present_ns - present_started_ns;
    const uint64_t steps[3] = { draw_started_ns - upload_started_ns, present_started_ns - draw_started_ns, backend->last_present_ns_spent };
    for (unsigned i = 0; i < 3; i++) {
        backend->step_ns_total[i] += steps[i];
        if (steps[i] > backend->step_ns_max[i]) backend->step_ns_max[i] = steps[i];
    }
    backend->stepped_presents++;
    if (!backend->presents) backend->first_present_ns = backend->last_present_ns;
    backend->presents++;
    return 1;
}

int oracles_sdl_backend_window_closed(const oracles_host_backend *backend)
{
    const sdl_backend *state = backend->opaque;
    return state && state->window_closed;
}

void oracles_sdl_backend_present_report(const oracles_host_backend *backend, unsigned *presents, double *seconds,
                                        const char **renderer, int *renderer_vsync)
{
    const sdl_backend *state = backend->opaque;
    *renderer = state && state->renderer_name[0] ? state->renderer_name : "unknown";
    *renderer_vsync = state ? state->renderer_vsync : 0;
    *presents = state ? state->presents : 0;
    const uint64_t span = state && state->presents > 1 ? state->last_present_ns - state->first_present_ns : 0;
    *seconds = state && span > state->held_ns ? (double)(span - state->held_ns) / 1e9 : 0.0;
}

void oracles_sdl_backend_present_steps(const oracles_host_backend *backend, double average_ms[3], double max_ms[3])
{
    const sdl_backend *state = backend->opaque;
    for (unsigned i = 0; i < 3; i++) {
        average_ms[i] = state && state->stepped_presents ? (double)state->step_ns_total[i] / state->stepped_presents / 1e6 : 0.0;
        max_ms[i] = state ? (double)state->step_ns_max[i] / 1e6 : 0.0;
    }
}

/* What the stream holds that the device has not taken yet, in bytes of the chunks put: SDL 2's queued audio. */
static Uint32 queued_bytes(const sdl_backend *backend)
{
    const int queued = SDL_GetAudioStreamQueued(backend->audio);
    return queued > 0 ? (Uint32)queued : 0u;
}

static int sdl_queue_audio(void *opaque, const oracles_host_audio_chunk *chunk)
{
    sdl_backend *backend = opaque;
    if (!backend->audio) return 1;
    const Uint32 queued = queued_bytes(backend);
    if (queued > backend->audio_peak_bytes) backend->audio_peak_bytes = queued;
    backend->audio_queued_sum_bytes += queued;
    backend->audio_queued_samples_count++;
    const uint64_t now = sdl_monotonic_ns(backend);
    if (backend->audio_started && queued == 0) {
        /* The device drained everything: a gap was heard. Refill before resuming. */
        if (backend->audio_underruns < ORACLES_SDL_UNDERRUNS_LOGGED) {
            oracles_sdl_underrun *u = &backend->underrun_log[backend->audio_underruns];
            u->frame = backend->presents;
            u->since_previous_chunk_ns = now - backend->last_chunk_ns;
            u->previous_chunk_left_ms = backend->last_chunk_queued_bytes / (audio_bytes_for_ms(backend, 1000u) / 1000u);
            u->last_present_ns = backend->last_present_ns_spent;
        }
        backend->audio_underruns++;
        SDL_PauseAudioStreamDevice(backend->audio);
        backend->audio_started = 0;
    }
    if (queued > audio_bytes_for_ms(backend, AUDIO_QUEUE_LIMIT_MS)) { backend->audio_drops++; return 1; }
    if (chunk->sample_count > INT_MAX / sizeof(int16_t)) return 0;
    if (!SDL_PutAudioStreamData(backend->audio, chunk->samples, (int)(chunk->sample_count * sizeof(int16_t)))) return 0;
    backend->last_chunk_ns = now;
    backend->last_chunk_queued_bytes = queued_bytes(backend);
    if (!backend->audio_started && queued_bytes(backend) >= audio_bytes_for_ms(backend, AUDIO_PREBUFFER_MS)) {
        SDL_ResumeAudioStreamDevice(backend->audio);
        backend->audio_started = 1;
    }
    return 1;
}

static uint32_t sdl_audio_queued_frames(void *opaque)
{
    const sdl_backend *backend = opaque;
    if (!backend->audio) return 0;
    return (uint32_t)(queued_bytes(backend) / (2u * sizeof(int16_t)));
}

void oracles_sdl_backend_audio_report(const oracles_host_backend *backend, unsigned *underruns, unsigned *drops,
                                      unsigned *peak_ms, unsigned *average_ms)
{
    const sdl_backend *state = backend->opaque;
    *underruns = state ? state->audio_underruns : 0;
    *drops = state ? state->audio_drops : 0;
    *peak_ms = 0;
    *average_ms = 0;
    if (!state || !state->sample_rate_hz) return;
    const uint64_t bytes_per_ms = (uint64_t)state->sample_rate_hz * 2u * sizeof(int16_t) / 1000u;
    *peak_ms = (unsigned)(state->audio_peak_bytes / bytes_per_ms);
    if (state->audio_queued_samples_count)
        *average_ms = (unsigned)(state->audio_queued_sum_bytes / state->audio_queued_samples_count / bytes_per_ms);
}

unsigned oracles_sdl_backend_underruns(const oracles_host_backend *backend, const oracles_sdl_underrun **log)
{
    const sdl_backend *state = backend->opaque;
    *log = state ? state->underrun_log : NULL;
    if (!state) return 0;
    return state->audio_underruns < ORACLES_SDL_UNDERRUNS_LOGGED ? state->audio_underruns : ORACLES_SDL_UNDERRUNS_LOGGED;
}

static uint64_t sdl_monotonic_ns(void *opaque)
{
    (void)opaque;
    const uint64_t frequency = SDL_GetPerformanceFrequency();
    const uint64_t counter = SDL_GetPerformanceCounter();
    if (!frequency) return 0;
    const uint64_t seconds = counter / frequency;
    const uint64_t rest = counter % frequency;
    return seconds * UINT64_C(1000000000) + rest * UINT64_C(1000000000) / frequency + 1;
}

static int sdl_sleep_ns(void *opaque, uint64_t duration_ns)
{
    /* Sleep for whole milliseconds minus one, then spin on the clock for the
     * rest: SDL_Delay has a granularity of about a millisecond. */
    const uint64_t start = sdl_monotonic_ns(opaque);
    if (duration_ns >= UINT64_C(2000000)) SDL_Delay((Uint32)(duration_ns / UINT64_C(1000000) - 1));
    while (sdl_monotonic_ns(opaque) - start < duration_ns) { /* spin */ }
    return 1;
}

/* The host caught a present that does not wait for the display.  The home screen's renderer, lent to the
 * session, gets its vsync back when the home screen takes its window again. */
static void sdl_present_unpaced(void *opaque)
{
    sdl_backend *backend = opaque;
    if (backend->renderer) SDL_SetRenderVSync(backend->renderer, 0);
}

/* SDL calls its event watches on the thread that sends the event, the host's here, at the moment it sends it: on
 * Android the suspension's event comes from inside a present or a poll, right before SDL blocks the thread while the
 * application is in the background, and SDL puts none of the application's events in the queue there.  The host
 * stores the save at once; the next poll, back from the background, hands it the suspension, which pauses. */
static bool SDLCALL suspension(void *opaque, SDL_Event *e)
{
    sdl_backend *backend = opaque;
    if (e->type != SDL_EVENT_WILL_ENTER_BACKGROUND) return true;
    if (backend->on_suspend) backend->on_suspend(backend->on_suspend_opaque);
    backend->suspended = 1;
    return true;
}

static void sdl_watch_suspend(void *opaque, void (*callback)(void *), void *callback_opaque)
{
    sdl_backend *backend = opaque;
    if (backend->on_suspend) SDL_RemoveEventWatch(suspension, backend);
    backend->on_suspend = callback;
    backend->on_suspend_opaque = callback_opaque;
    if (callback) SDL_AddEventWatch(suspension, backend);
}

static void sdl_stop(void *opaque)
{
    sdl_backend *backend = opaque;
    if (backend->audio) { SDL_DestroyAudioStream(backend->audio); backend->audio = NULL; }   /* its device closes with it */
    oracles_sdl_pads_close(&backend->pads);
    if (backend->texture) SDL_DestroyTexture(backend->texture);
    backend->texture = NULL;
    if (backend->borrowed) {
        /* The launcher's window goes back as it was lent, even when the start failed half way: no logical size, the
         * launcher sets its own size again; only the subsystems the loan started are quit. */
        SDL_SetRenderLogicalPresentation(backend->renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        if (backend->subsystems) SDL_QuitSubSystem(SDL_INIT_AUDIO | SDL_INIT_GAMEPAD);
        backend->subsystems = 0;
        return;
    }
    if (backend->renderer) SDL_DestroyRenderer(backend->renderer);
    if (backend->window) SDL_DestroyWindow(backend->window);
    backend->renderer = NULL; backend->window = NULL;
    if (SDL_WasInit(0)) SDL_Quit();
}

int oracles_sdl_backend_init(oracles_host_backend *backend, const oracles_sdl_options *options)
{
    sdl_backend *state = calloc(1, sizeof *state);
    if (!state) return 0;
    state->scale = options && options->scale ? options->scale : 4;
    state->fullscreen = oracles_sdl_fullscreen_only() || (options && options->fullscreen);
    state->vsync = options ? options->vsync : 0;
    state->window = options ? options->window : NULL;
    state->renderer = options ? options->renderer : NULL;
    state->borrowed = state->window != NULL;
    state->pause_menu = options ? options->pause_menu : 0;
#ifdef __ANDROID__
    state->touch_enabled = 1;
#else
    state->touch_enabled = options && options->touch_controls;
#endif
    resolve_bindings(state, options);
    resolve_hotkeys(state, options);
    memset(backend, 0, sizeof *backend);
    backend->opaque = state;
    backend->start = sdl_start;
    backend->poll_event = sdl_poll_event;
    backend->present_frame = sdl_present_frame;
    backend->queue_audio = sdl_queue_audio;
    backend->audio_queued_frames = sdl_audio_queued_frames;
    backend->monotonic_ns = sdl_monotonic_ns;
    backend->sleep_ns = sdl_sleep_ns;
    backend->present_unpaced = sdl_present_unpaced;
    backend->watch_suspend = sdl_watch_suspend;
    backend->stop = sdl_stop;
    return 1;
}

unsigned oracles_sdl_display_refresh_hz(void)
{
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return 0;
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
    if (!mode || mode->refresh_rate <= 0.0f) return 0;
    return (unsigned)lroundf(mode->refresh_rate);   /* whole hertz, as SDL 2 gave them */
}

void oracles_sdl_backend_pause_view(const oracles_host_backend *backend, struct SDL_Window **window, struct SDL_Renderer **renderer,
                                    struct SDL_Texture **frame, int *width, int *height)
{
    const sdl_backend *state = backend->opaque;
    *window = state->window;
    *renderer = state->renderer;
    *frame = state->texture;
    *width = state->frame_width;
    *height = state->frame_height;
}

void oracles_sdl_backend_suspend_audio(const oracles_host_backend *backend)
{
    sdl_backend *state = backend->opaque;
    if (!state->audio) return;
    SDL_PauseAudioStreamDevice(state->audio);
    SDL_ClearAudioStream(state->audio);
    state->audio_started = 0;   /* sdl_queue_audio starts it again once its prebuffer is queued */
}

void oracles_sdl_backend_hold(const oracles_host_backend *backend, int held)
{
    sdl_backend *state = backend->opaque;
    const uint64_t now = sdl_monotonic_ns(state);
    if (held) state->hold_started_ns = now;
    else if (state->hold_started_ns && state->presents) state->held_ns += now - state->hold_started_ns;
    if (!held) state->hold_started_ns = 0;
    if (!held) state->suspended = 0;   /* a suspension during the pause: the player comes back to the game, not to a pause */
    if (held) { oracles_touch_release(&state->touch.touch); state->touch_buttons = 0; }   /* the pause's menu takes the fingers */
}

void oracles_sdl_backend_rebind(const oracles_host_backend *backend, const oracles_sdl_options *options)
{
    sdl_backend *state = backend->opaque;
    resolve_bindings(state, options);
    resolve_hotkeys(state, options);
    state->axis_buttons = 0;
}

void oracles_sdl_backend_set_window_closed(const oracles_host_backend *backend)
{
    sdl_backend *state = backend->opaque;
    state->window_closed = 1;
}

void oracles_sdl_backend_release(oracles_host_backend *backend)
{
    free(backend->opaque);
    backend->opaque = NULL;
}

int oracles_sdl_settings_dir(char *out, size_t capacity)
{
    /* ORACLES_SETTINGS_DIR, an existing folder, wins: the tests keep off the player's settings on every system
     * (SDL_GetPrefPath reads no environment variable on Windows). */
    const char *forced = getenv("ORACLES_SETTINGS_DIR");
    if (forced && forced[0]) {
        const size_t length = strlen(forced);
        const int separated = forced[length - 1] == '/' || forced[length - 1] == '\\';
        if (length + 2 > capacity) return 0;
        snprintf(out, capacity, "%s%s", forced, separated ? "" : "/");
        return 1;
    }
    char *path = SDL_GetPrefPath("", "the-oracles-project");
    if (!path) return 0;
    const int ok = strlen(path) < capacity;
    if (ok) memcpy(out, path, strlen(path) + 1);
    SDL_free(path);
    return ok;
}
