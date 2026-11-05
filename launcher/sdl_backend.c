/* SDL 3 backend: window with integer scaling (Scaling's Fill: in fullscreen, the whole screen in the image's
 * proportions), keyboard, gamepad, audio.
 * Keyboard by default: arrows, X = A, Z = B, Return = Start, Backspace =
 * Select; F2 = colour correction, F3 = Enhanced wide/framed, F5 = save state, F7 = load state,
 * F11 = fullscreen, Escape = quit, or the pause menu for a game the home screen started.
 * Controller by default: d-pad or left stick, A, B, Start, Back = Select; the left stick's click (L3) and Guide open
 * the pause menu of a game the home screen started, and do nothing with --rom, which has none.
 * Both are rebindable through oracles_sdl_options. */
#include "sdl_backend_internal.h"

const char *const oracles_sdl_binding_names[ORACLES_BINDINGS] = { "right", "left", "up", "down", "a", "b", "select", "start" };
const char *const oracles_sdl_default_key_names[ORACLES_BINDINGS] = { "Right", "Left", "Up", "Down", "X", "Z", "Backspace", "Return" };
const char *const oracles_sdl_default_pad_names[4] = { "a", "b", "back", "start" };
const char *const oracles_sdl_default_menu_pad_name = "leftstick";

void sb_push_button(sdl_backend *backend, unsigned button, int pressed)
{
    if (backend->pending_count >= PENDING_EVENTS) return;
    oracles_host_event *event = &backend->pending[backend->pending_count++];
    event->type = ORACLES_HOST_EVENT_BUTTON;
    event->button = button;
    event->pressed = pressed;
}

void sb_push_event(sdl_backend *backend, oracles_host_event_type type)
{
    if (backend->pending_count >= PENDING_EVENTS) return;
    oracles_host_event *event = &backend->pending[backend->pending_count++];
    memset(event, 0, sizeof *event);
    event->type = type;
}

void sb_push_quit(sdl_backend *backend) { sb_push_event(backend, ORACLES_HOST_EVENT_QUIT); }

void sb_push_command(sdl_backend *backend, int command)
{
    if (backend->pending_count >= PENDING_EVENTS) return;
    oracles_host_event *event = &backend->pending[backend->pending_count++];
    memset(event, 0, sizeof *event);
    event->type = ORACLES_HOST_EVENT_COMMAND;
    event->command = command;
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

void oracles_sdl_presentation(int fullscreen, int fill, int *presentation, int *scale_mode)
{
    const int filled = fullscreen && fill;
    *presentation = filled ? SDL_LOGICAL_PRESENTATION_LETTERBOX : SDL_LOGICAL_PRESENTATION_INTEGER_SCALE;
    *scale_mode = filled ? SDL_SCALEMODE_PIXELART : SDL_SCALEMODE_NEAREST;
}

int sb_window_fullscreen(const sdl_backend *backend)
{
    return backend->window && (SDL_GetWindowFlags(backend->window) & SDL_WINDOW_FULLSCREEN) != 0;
}

/* The presentation of a logical size for the window's state and Scaling: SDL's logical presentation (none once the
 * touch controls draw the frame themselves, where SDL would put it) and the frame's sampling. */
void sb_present_as(sdl_backend *backend, int logical_w, int logical_h)
{
    int presentation, scale_mode;
    oracles_sdl_presentation(sb_window_fullscreen(backend), backend->fill, &presentation, &scale_mode);
    if (!backend->touch.direct)
        SDL_SetRenderLogicalPresentation(backend->renderer, logical_w, logical_h, (SDL_RendererLogicalPresentation)presentation);
    SDL_SetTextureScaleMode(backend->texture, (SDL_ScaleMode)scale_mode);
    backend->filled = backend->touch.fill = presentation == SDL_LOGICAL_PRESENTATION_LETTERBOX;
    backend->logical_w = logical_w;
    backend->logical_h = logical_h;
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
    SDL_SetRenderDrawColor(backend->renderer, 0, 0, 0, 255);
    backend->texture = SDL_CreateTexture(backend->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                         (int)width, (int)height);
    if (!backend->texture) return 0;
    backend->frame_width = (int)width;
    backend->frame_height = (int)height;
    oracles_safe_area_edges(backend->window, backend->touch.edges.margin, &backend->touch.edges);
    sb_present_as(backend, (int)width, (int)height);
    oracles_safe_area_report(backend->window, "game", "started", &backend->touch.edges);
    oracles_sdl_pads_open(&backend->pads);
    {
        char pads[256];
        oracles_sdl_pads_describe(&backend->pads, pads, sizeof pads);
        fprintf(stderr, "oracles: game controllers: %s\n", pads);
    }
    backend->menu_trigger_held = oracles_sdl_trigger_held(backend->menu_trigger);   /* a trigger held from the home screen asks nothing */
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

void oracles_sdl_backend_set_vsync(const oracles_host_backend *backend, int vsync)
{
    sdl_backend *state = backend->opaque;
    state->vsync = vsync != 0;
    if (state->renderer) SDL_SetRenderVSync(state->renderer, state->vsync ? 1 : 0);
}

void oracles_sdl_backend_set_fill(const oracles_host_backend *backend, int fill)
{
    sdl_backend *state = backend->opaque;
    state->fill = fill != 0;
    int presentation, scale_mode;
    oracles_sdl_presentation(sb_window_fullscreen(state), state->fill, &presentation, &scale_mode);
    if (state->texture) SDL_SetTextureScaleMode(state->texture, (SDL_ScaleMode)scale_mode);   /* the pause's image of the game too */
}

void oracles_sdl_backend_set_window(const oracles_host_backend *backend, int fullscreen, unsigned scale)
{
    sdl_backend *state = backend->opaque;
    if (!state || !state->window || oracles_sdl_fullscreen_only() || !scale) return;
    state->fullscreen = fullscreen != 0;
    state->scale = scale;
    SDL_SetWindowFullscreen(state->window, state->fullscreen != 0);
    SDL_SyncWindow(state->window);   /* the window's state read below is the one asked for */
    if (!(SDL_GetWindowFlags(state->window) & SDL_WINDOW_FULLSCREEN)) {
        int x, y, w, h;
        SDL_GetWindowPosition(state->window, &x, &y);
        SDL_GetWindowSize(state->window, &w, &h);
        oracles_sdl_size_window(state->window, (int)((unsigned)state->frame_width * scale), (int)((unsigned)state->frame_height * scale));
        oracles_sdl_place_window(state->window, x + w / 2, y + h / 2);
    }
    int presentation, scale_mode;
    oracles_sdl_presentation(sb_window_fullscreen(state), state->fill, &presentation, &scale_mode);
    if (state->texture) SDL_SetTextureScaleMode(state->texture, (SDL_ScaleMode)scale_mode);   /* the pause's image of the game too */
}

void oracles_sdl_backend_set_screen_margin(const oracles_host_backend *backend, int margin)
{
    sdl_backend *state = backend->opaque;
    oracles_safe_area_edges(state->window, margin, &state->touch.edges);
    oracles_safe_area_report(state->window, "game", "screen margin changed", &state->touch.edges);
}

void oracles_sdl_backend_scaling(const oracles_host_backend *backend, int *chosen, int *in_effect)
{
    const sdl_backend *state = backend->opaque;
    *chosen = state && state->fill;
    *in_effect = state && state->filled;
}

unsigned oracles_sdl_backend_whole_scale(const oracles_host_backend *backend)
{
    const sdl_backend *state = backend->opaque;
    int w = 0, h = 0;
    if (!state || !state->renderer || state->frame_width <= 0 || state->frame_height <= 0 || !SDL_GetRenderOutputSize(state->renderer, &w, &h) || w <= 0 || h <= 0)
        return 0;
    const OraclesTouchBox band = oracles_touch_band(w, h, &state->touch.edges);
    const float kx = band.w / (float)state->frame_width, ky = band.h / (float)state->frame_height, k = kx < ky ? kx : ky;
    return k >= 1.0f ? (unsigned)k : 1u;
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
    const uint64_t now = sb_sdl_monotonic_ns(backend);
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

uint64_t sb_sdl_monotonic_ns(void *opaque)
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
    const uint64_t start = sb_sdl_monotonic_ns(opaque);
    if (duration_ns >= UINT64_C(2000000)) SDL_Delay((Uint32)(duration_ns / UINT64_C(1000000) - 1));
    while (sb_sdl_monotonic_ns(opaque) - start < duration_ns) { /* spin */ }
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
 * stores the save at once; the next poll, back from the background, hands it the sb_suspension, which pauses. */
bool SDLCALL sb_suspension(void *opaque, SDL_Event *e)
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
    if (backend->on_suspend) SDL_RemoveEventWatch(sb_suspension, backend);
    backend->on_suspend = callback;
    backend->on_suspend_opaque = callback_opaque;
    if (callback) SDL_AddEventWatch(sb_suspension, backend);
}

static void sdl_stop(void *opaque)
{
    sdl_backend *backend = opaque;
    if (backend->audio) { SDL_DestroyAudioStream(backend->audio); backend->audio = NULL; }   /* its device closes with it */
    oracles_sdl_pads_close(&backend->pads);
    if (backend->texture) SDL_DestroyTexture(backend->texture);
    backend->texture = NULL;
    if (backend->layer) SDL_DestroyTexture(backend->layer);
    backend->layer = NULL; backend->layer_w = backend->layer_h = 0;
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
    state->fill = options && options->fill;
    state->overlay = options ? options->overlay : NULL;
    state->overlay_opaque = options ? options->overlay_opaque : NULL;
    state->touch.edges.margin = options ? options->screen_margin : ORACLES_SCREEN_MARGIN_OFF;
#ifdef __ANDROID__
    state->touch_enabled = 1;
#else
    state->touch_enabled = options && options->touch_controls;
#endif
    sb_resolve_bindings(state, options);
    sb_resolve_hotkeys(state, options);
    sb_resolve_menu(state, options);
    memset(backend, 0, sizeof *backend);
    backend->opaque = state;
    backend->start = sdl_start;
    backend->poll_event = sb_sdl_poll_event;
    backend->present_frame = sb_sdl_present_frame;
    backend->queue_audio = sdl_queue_audio;
    backend->audio_queued_frames = sdl_audio_queued_frames;
    backend->monotonic_ns = sb_sdl_monotonic_ns;
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

void oracles_sdl_backend_pause_size(const oracles_host_backend *backend, int *width, int *height)
{
    const sdl_backend *state = backend->opaque;
    *width = state->shown_cropped ? (int)state->shown_part.w : state->frame_width;
    *height = state->shown_cropped ? (int)state->shown_part.h : state->frame_height;
}

void oracles_sdl_backend_pause_crop(const oracles_host_backend *backend, const uint32_t rect[4])
{
    sb_show_crop(backend->opaque, rect);
}

void oracles_sdl_backend_pause_box(const oracles_host_backend *backend, const uint32_t rect[4], int bottom, uint32_t reserve_w, unsigned size)
{
    sb_show_box(backend->opaque, rect, bottom, reserve_w, size);
}

void oracles_sdl_backend_pause_pixels(const oracles_host_backend *backend, const uint32_t *pixels)
{
    const sdl_backend *state = backend->opaque;
    if (state->texture) SDL_UpdateTexture(state->texture, NULL, pixels, state->frame_width * (int)sizeof *pixels);
}

int oracles_sdl_backend_pause_image(const oracles_host_backend *backend, const struct SDL_FRect *image)
{
    sdl_backend *state = backend->opaque;
    if (!state->texture) return 0;
    if (!state->shown_subpixel) {
        if (!SDL_RenderTexture(state->renderer, state->texture, state->shown_cropped ? &state->shown_part : NULL, image)) return 0;
        sb_draw_shown_box(state, image);   /* Text size Large's box, which the presenter drew over the frame */
        return 1;
    }
    const int drawn = sb_draw_subpixel_cut(state, image);
    if (drawn) sb_draw_shown_box(state, image);
    return drawn;
}

oracles_sdl_menu_source oracles_sdl_backend_menu_source(const oracles_host_backend *backend)
{
    const sdl_backend *state = backend->opaque;
    return state->menu_source;
}

oracles_sdl_menu_source oracles_sdl_backend_last_input(const oracles_host_backend *backend)
{
    const sdl_backend *state = backend->opaque;
    return state->last_input;
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
    const uint64_t now = sb_sdl_monotonic_ns(state);
    if (held) state->hold_started_ns = now;
    else if (state->hold_started_ns && state->presents) state->held_ns += now - state->hold_started_ns;
    if (!held) state->hold_started_ns = 0;
    if (!held) state->suspended = 0;   /* a suspension during the pause: the player comes back to the game, not to a pause */
    if (held) { oracles_touch_release(&state->touch.touch); state->touch_buttons = 0; }   /* the pause's menu takes the fingers */
    /* The pause's menu had the plugs and unplugs, and left none in the queue: those unplugged are let go, those plugged in are taken. */
    if (!held) oracles_sdl_pads_open(&state->pads);
    /* Nor the trigger's moves: one that closed the pause is still held, one let go there is released. */
    if (!held) state->menu_trigger_held = oracles_sdl_trigger_held(state->menu_trigger);
    /* Nor the safe area's and the display's changes, which the pause's loop took: the margin's band is read again. */
    if (!held && state->window) {
        const OraclesTouchEdges before = state->touch.edges;
        oracles_safe_area_edges(state->window, before.margin, &state->touch.edges);
        if (memcmp(&before, &state->touch.edges, sizeof before)) oracles_safe_area_report(state->window, "game", "resumed", &state->touch.edges);
    }
}

void oracles_sdl_backend_rebind(const oracles_host_backend *backend, const oracles_sdl_options *options)
{
    sdl_backend *state = backend->opaque;
    sb_resolve_bindings(state, options);
    sb_resolve_hotkeys(state, options);
    sb_resolve_menu(state, options);
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
