/* The SDL 3 backend's input (backends.h): the keyboard's and the controller's bindings, the item hotkeys, the menu's
 * button or trigger, the touch screen, and the events handed to the host. */
#include "sdl_backend_internal.h"

/* Android's Back is Escape there (SDLK_AC_BACK); elsewhere, a media keyboard's Back is a key like any other. */
#ifdef __ANDROID__
#define ANDROID_BACK(key) ((key) == SDLK_AC_BACK)
#else
#define ANDROID_BACK(key) 0
#endif

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
void sb_resolve_bindings(sdl_backend *backend, const oracles_sdl_options *options)
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
void sb_resolve_hotkeys(sdl_backend *backend, const oracles_sdl_options *options)
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
    sb_push_command(backend, ORACLES_COMMAND_HOTKEY + (int)(slot * 8u + modifier * 2u + (pressed ? 1u : 0u)));
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

int oracles_sdl_menu_input(const char *name, int *button, int *axis)
{
    *button = SDL_GAMEPAD_BUTTON_INVALID;
    *axis = SDL_GAMEPAD_AXIS_INVALID;
    if (!name || !name[0]) return 1;
    const SDL_GamepadAxis trigger = SDL_GetGamepadAxisFromString(name);
    if (trigger == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || trigger == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) { *axis = trigger; return 1; }
    *button = SDL_GetGamepadButtonFromString(name);
    return *button != SDL_GAMEPAD_BUTTON_INVALID;
}

int oracles_sdl_trigger_pressed(int value, int *held)
{
    const int was = *held;
    if (value > SDL_JOYSTICK_AXIS_MAX / 2) *held = 1;
    else if (value < SDL_JOYSTICK_AXIS_MAX / 4) *held = 0;
    return *held && !was;
}

int oracles_sdl_trigger_moved(const SDL_Event *event, int axis, int *held)
{
    if (event->type != SDL_EVENT_GAMEPAD_AXIS_MOTION || axis == SDL_GAMEPAD_AXIS_INVALID || event->gaxis.axis != axis) return 0;
    int value = event->gaxis.value, count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    for (int i = 0; i < count; i++) {
        SDL_Gamepad *pad = ids[i] == event->gaxis.which ? NULL : SDL_GetGamepadFromID(ids[i]);
        const int other = pad ? SDL_GetGamepadAxis(pad, (SDL_GamepadAxis)axis) : 0;
        if (other > value) value = other;
    }
    SDL_free(ids);
    return oracles_sdl_trigger_pressed(value, held);
}

int oracles_sdl_trigger_held(int axis)
{
    int count = 0, held = 0;
    SDL_JoystickID *ids = axis == SDL_GAMEPAD_AXIS_INVALID ? NULL : SDL_GetGamepads(&count);
    for (int i = 0; i < count && !held; i++) {
        SDL_Gamepad *pad = SDL_GetGamepadFromID(ids[i]);
        held = pad && SDL_GetGamepadAxis(pad, (SDL_GamepadAxis)axis) >= SDL_JOYSTICK_AXIS_MAX / 4;
    }
    SDL_free(ids);
    return held;
}

/* The menu's button or trigger, after the game's buttons and the hotkeys': a button of theirs, or of the d-pad, stays
 * theirs, as a hotkey's button the game has stays the game's, and Guide alone opens the menu; an unknown name keeps
 * the default.  The game reads no trigger. */
void sb_resolve_menu(sdl_backend *backend, const oracles_sdl_options *options)
{
    const char *wanted = options && options->menu_pad_name ? options->menu_pad_name : oracles_sdl_default_menu_pad_name;
    int button, axis;
    if (!oracles_sdl_menu_input(wanted, &button, &axis)) {
        SDL_Log("unknown controller button '%s' for the menu; using %s", wanted, oracles_sdl_default_menu_pad_name);
        oracles_sdl_menu_input(oracles_sdl_default_menu_pad_name, &button, &axis);
    }
    unsigned taken;
    const char *owner = NULL;
    if (button >= SDL_GAMEPAD_BUTTON_DPAD_UP && button <= SDL_GAMEPAD_BUTTON_DPAD_RIGHT) owner = "the d-pad's";
    else if (button != SDL_GAMEPAD_BUTTON_INVALID && translate_controller_button(backend, (SDL_GamepadButton)button, &taken)) owner = "the game's";
    else if (button != SDL_GAMEPAD_BUTTON_INVALID && translate_hotkey_button(backend, (SDL_GamepadButton)button, &taken)) owner = "an item hotkey's";
    if (owner && backend->pause_menu) {
        fprintf(stderr, "oracles: menu_pad=%s is %s already and stays so: Guide alone opens the menu\n",
                SDL_GetGamepadStringForButton((SDL_GamepadButton)button), owner);
    }
    if (owner) button = SDL_GAMEPAD_BUTTON_INVALID;
    backend->menu_pad = (SDL_GamepadButton)button;
    backend->menu_trigger = (SDL_GamepadAxis)axis;
    backend->menu_trigger_held = oracles_sdl_trigger_held(axis);
}

/* The menu's button or trigger, or Guide, went down on a controller: the pause, and which input of which controller
 * asked, for a device whose buttons are not those expected.  Without a pause menu (--rom) nothing: Escape alone ends
 * such a session, as before the controller had an Escape. */
static void push_menu(sdl_backend *backend, SDL_JoystickID id, const char *input)
{
    if (!backend->pause_menu) return;
    const char *name = SDL_GetGamepadNameForID(id);
    fprintf(stderr, "oracles: controller input %s of %s (%s) opens the pause menu\n", input, name ? name : "an unnamed controller",
            SDL_GetGamepadStringForType(SDL_GetGamepadTypeForID(id)));
    backend->menu_source = ORACLES_SDL_MENU_FROM_PAD;
    sb_push_event(backend, ORACLES_HOST_EVENT_PAUSE);
}

/* Android's Back, said with the device it came from, as the activity saw it: a handheld's Back is often its built-in
 * controller's. */
static void push_android_back(sdl_backend *backend)
{
    char source[256];
    oracles_android_activity_text("backSource", source, sizeof source);
    fprintf(stderr, "oracles: Back of %s %s\n", source[0] ? source : "an unknown device", backend->pause_menu ? "opens the pause menu" : "ends the session");
    sb_push_event(backend, backend->pause_menu ? ORACLES_HOST_EVENT_PAUSE : ORACLES_HOST_EVENT_QUIT);
}

static void update_axis(sdl_backend *backend, unsigned negative, unsigned positive, Sint16 value)
{
    const unsigned wanted = value < -AXIS_THRESHOLD ? negative : value > AXIS_THRESHOLD ? positive : 0;
    const unsigned held = backend->axis_buttons & (negative | positive);
    if (held == wanted) return;
    if (held) sb_push_button(backend, held, 0);
    if (wanted) sb_push_button(backend, wanted, 1);
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
        if ((now ^ backend->touch_buttons) & bit) sb_push_button(backend, bit, (now & bit) != 0);
    backend->touch_buttons = now;
    if (pause && backend->pause_menu) { backend->menu_source = ORACLES_SDL_MENU_FROM_TOUCH; sb_push_event(backend, ORACLES_HOST_EVENT_PAUSE); }
    return finger;
}

static void handle_event(sdl_backend *backend, const SDL_Event *e)
{
    unsigned button;
    if (touch_event(backend, e)) { backend->last_input = ORACLES_SDL_MENU_FROM_TOUCH; return; }
    if (e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || (e->type == SDL_EVENT_GAMEPAD_AXIS_MOTION && (e->gaxis.value > AXIS_THRESHOLD || e->gaxis.value < -AXIS_THRESHOLD)))
        backend->last_input = ORACLES_SDL_MENU_FROM_PAD;
    /* The window's focus and screen changes, said on the standard error: on a device of two screens, the input follows
     * the screen that has the focus. */
    oracles_safe_area_event(backend->window, "game", e, &backend->touch.edges);
    if (e->type == SDL_EVENT_WINDOW_FOCUS_GAINED || e->type == SDL_EVENT_WINDOW_FOCUS_LOST || e->type == SDL_EVENT_WINDOW_DISPLAY_CHANGED)
        fprintf(stderr, "oracles: game window: %s\n", e->type == SDL_EVENT_WINDOW_FOCUS_GAINED ? "focus gained"
                : e->type == SDL_EVENT_WINDOW_FOCUS_LOST ? "focus lost" : "moved to another display");
    switch (e->type) {
        case SDL_EVENT_QUIT: backend->window_closed = 1; sb_push_quit(backend); break;
        case SDL_EVENT_KEY_DOWN:
            if (e->key.repeat) break;
            if (ANDROID_BACK(e->key.key)) { backend->menu_source = ORACLES_SDL_MENU_FROM_BACK; push_android_back(backend); break; }
            backend->last_input = ORACLES_SDL_MENU_FROM_KEY;
            if (e->key.key == SDLK_ESCAPE) {
                backend->menu_source = ORACLES_SDL_MENU_FROM_KEY;
                sb_push_event(backend, backend->pause_menu ? ORACLES_HOST_EVENT_PAUSE : ORACLES_HOST_EVENT_QUIT);
                break;
            }
            if (e->key.key == SDLK_F11) { toggle_fullscreen(backend); break; }
            if (e->key.key == SDLK_F2) { sb_push_command(backend, ORACLES_COMMAND_TOGGLE_COLOUR_CORRECTION); break; }
            if (e->key.key == SDLK_F3) { sb_push_command(backend, ORACLES_COMMAND_TOGGLE_ENHANCED); break; }
            if (e->key.key == SDLK_F5) { sb_push_command(backend, ORACLES_COMMAND_SAVE_STATE); break; }
            if (e->key.key == SDLK_F7) { sb_push_command(backend, ORACLES_COMMAND_LOAD_STATE); break; }
            if (translate_key(backend, e->key.key, &button)) sb_push_button(backend, button, 1);
            else if (translate_hotkey(backend, e->key.key, &button)) push_hotkey(backend, button, 1, 1);
            break;
        case SDL_EVENT_KEY_UP:
            if (translate_key(backend, e->key.key, &button)) sb_push_button(backend, button, 0);
            else if (translate_hotkey(backend, e->key.key, &button)) push_hotkey(backend, button, 0, 1);
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
            const SDL_GamepadButton pressed = (SDL_GamepadButton)oracles_sdl_pad_button(e);
            if (translate_controller_button(backend, pressed, &button)) sb_push_button(backend, button, e->gbutton.down);
            else if (translate_hotkey_button(backend, pressed, &button)) push_hotkey(backend, button, e->gbutton.down, 0);
            /* Escape's, never the game's: its release, perhaps after the pause, is nobody's. */
            else if ((pressed == backend->menu_pad || pressed == SDL_GAMEPAD_BUTTON_GUIDE) && e->gbutton.down)
                push_menu(backend, e->gbutton.which, SDL_GetGamepadStringForButton(pressed));
            break;
        }
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (e->gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) update_axis(backend, ORACLES_KEY_LEFT, ORACLES_KEY_RIGHT, e->gaxis.value);
            else if (e->gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) update_axis(backend, ORACLES_KEY_UP, ORACLES_KEY_DOWN, e->gaxis.value);
            else if (oracles_sdl_trigger_moved(e, backend->menu_trigger, &backend->menu_trigger_held))
                push_menu(backend, e->gaxis.which, SDL_GetGamepadStringForAxis((SDL_GamepadAxis)e->gaxis.axis));
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
            oracles_sdl_pads_event(&backend->pads, e);
            break;
        default: break;   /* a file or a text dropped on the window during the game: SDL keeps and frees its copy */
    }
}

int sb_sdl_poll_event(void *opaque, oracles_host_event *event, int *has_event)
{
    sdl_backend *backend = opaque;
    SDL_Event e;
    *has_event = 0;
    /* The pause a suspension brings was asked by no device: its hints keep the one used last. */
    if (backend->suspended) { backend->suspended = 0; backend->menu_source = ORACLES_SDL_MENU_FROM_NONE; sb_push_event(backend, ORACLES_HOST_EVENT_SUSPEND); }
    while (backend->pending_count == 0 && SDL_PollEvent(&e)) handle_event(backend, &e);
    if (backend->pending_count == 0) return 1;
    *event = backend->pending[0];
    memmove(backend->pending, backend->pending + 1, (backend->pending_count - 1) * sizeof backend->pending[0]);
    backend->pending_count--;
    *has_event = 1;
    return 1;
}
