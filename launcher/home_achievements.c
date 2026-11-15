/* RetroAchievements' login on the home screen (home_achievements.h). */
#include "home_achievements.h"

#include "backends.h"

static int key_of(SDL_Keycode key, SDL_Keymod mod, OraclesLoginKey *out)
{
    switch (key) {
        case SDLK_UP: *out = ORACLES_LOGIN_KEY_UP; return 1;
        case SDLK_DOWN: *out = ORACLES_LOGIN_KEY_DOWN; return 1;
        case SDLK_LEFT: *out = ORACLES_LOGIN_KEY_LEFT; return 1;
        case SDLK_RIGHT: *out = ORACLES_LOGIN_KEY_RIGHT; return 1;
        case SDLK_TAB: *out = (mod & SDL_KMOD_SHIFT) ? ORACLES_LOGIN_KEY_BACKTAB : ORACLES_LOGIN_KEY_TAB; return 1;
        case SDLK_RETURN: case SDLK_KP_ENTER: *out = ORACLES_LOGIN_KEY_ENTER; return 1;
        case SDLK_ESCAPE: case SDLK_AC_BACK: *out = ORACLES_LOGIN_KEY_ESCAPE; return 1;
        case SDLK_BACKSPACE: *out = ORACLES_LOGIN_KEY_BACKSPACE; return 1;
        default: return 0;
    }
}

static int button_of(const OraclesHomeNav *nav, int button, OraclesLoginKey *out)
{
    int menu_button, menu_trigger;
    oracles_sdl_menu_input(nav->controls.menu_pad, &menu_button, &menu_trigger);
    switch (button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP: *out = ORACLES_LOGIN_PAD_UP; return 1;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: *out = ORACLES_LOGIN_PAD_DOWN; return 1;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: *out = ORACLES_LOGIN_PAD_LEFT; return 1;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: *out = ORACLES_LOGIN_PAD_RIGHT; return 1;
        case SDL_GAMEPAD_BUTTON_SOUTH: *out = ORACLES_LOGIN_PAD_A; return 1;
        case SDL_GAMEPAD_BUTTON_EAST: *out = ORACLES_LOGIN_PAD_B; return 1;
        case SDL_GAMEPAD_BUTTON_NORTH: *out = ORACLES_LOGIN_PAD_Y; return 1;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: *out = ORACLES_LOGIN_PAD_SHOULDER; return 1;
        case SDL_GAMEPAD_BUTTON_START: *out = ORACLES_LOGIN_PAD_START; return 1;
        case SDL_GAMEPAD_BUTTON_GUIDE: *out = ORACLES_LOGIN_KEY_ESCAPE; return 1;
        default:
            if (button != menu_button) return 0;
            *out = ORACLES_LOGIN_KEY_ESCAPE;   /* the controller's Escape cancels */
            return 1;
    }
}

/* SDL's key and modifiers in the login's terms (ui_achievements.h). */
static OraclesLoginRawKey raw_key(SDL_Keycode key)
{
    switch (key) {
        case SDLK_V: return ORACLES_LOGIN_RAW_V;
        case SDLK_INSERT: return ORACLES_LOGIN_RAW_INSERT;
        case SDLK_PASTE: return ORACLES_LOGIN_RAW_PASTE;
        case SDLK_RETURN: return ORACLES_LOGIN_RAW_RETURN;
        case SDLK_TAB: return ORACLES_LOGIN_RAW_TAB;
        default: return ORACLES_LOGIN_RAW_OTHER;
    }
}

static unsigned raw_mods(SDL_Keymod mod)
{
    return ((mod & SDL_KMOD_SHIFT) ? ORACLES_LOGIN_MOD_SHIFT : 0u) | ((mod & SDL_KMOD_CTRL) ? ORACLES_LOGIN_MOD_CTRL : 0u)
         | ((mod & SDL_KMOD_ALT) ? ORACLES_LOGIN_MOD_ALT : 0u) | ((mod & SDL_KMOD_GUI) ? ORACLES_LOGIN_MOD_GUI : 0u);
}

static OraclesLoginWindow window_change(Uint32 type)
{
    switch (type) {
        case SDL_EVENT_WILL_ENTER_BACKGROUND: case SDL_EVENT_DID_ENTER_BACKGROUND: return ORACLES_LOGIN_WINDOW_BACKGROUND;
        case SDL_EVENT_WINDOW_FOCUS_LOST: return ORACLES_LOGIN_WINDOW_FOCUS_LOST;
        case SDL_EVENT_WINDOW_MINIMIZED: return ORACLES_LOGIN_WINDOW_MINIMIZED;
        case SDL_EVENT_WINDOW_HIDDEN: return ORACLES_LOGIN_WINDOW_HIDDEN;
        default: return ORACLES_LOGIN_WINDOW_OTHER;
    }
}

#ifdef __APPLE__
#define APPLE 1
#else
#define APPLE 0
#endif

/* The clipboard's text into the field highlighted, SDL's copy of it wiped before it is freed: the launcher keeps no
 * other copy (SDL and the system may have had their own) and writes it nowhere. */
static void paste(OraclesHomeNav *nav)
{
    if (!oracles_achievements_login_typing(nav) || !SDL_HasClipboardText()) return;
    char *text = SDL_GetClipboardText();
    if (!text) return;
    oracles_achievements_login_paste(nav, text);
    SDL_free(text);
}

int oracles_home_achievements_event(OraclesHomeNav *nav, const SDL_Event *event, OraclesHomeCommand *command)
{
    *command = ORACLES_HOME_STAY;
    /* The password hidden again when the application goes to the background or its window away: not taken. */
    oracles_achievements_window_changed(nav, window_change(event->type));
    if (nav->screen != ORACLES_SCREEN_ACHIEVEMENTS || !nav->achievements.login) return 0;
    OraclesLoginKey key;
    switch (event->type) {
        case SDL_EVENT_TEXT_INPUT:
            oracles_achievements_login_typed(nav, event->text.text);
            return 1;
        case SDL_EVENT_KEY_DOWN: {
            if (event->key.key == SDLK_F11) return 0;
            const OraclesLoginRawKey raw = raw_key(event->key.key);
            const int chord = oracles_login_paste_chord(raw, raw_mods(event->key.mod), event->key.repeat, APPLE);
            if (chord) {
                if (chord > 0) paste(nav);
                return 1;
            }
            /* Android's Enter or Tab made up for a line break or a tab of the text its keyboard commits, sent before it. */
            const int text_waiting = SDL_TextInputActive(SDL_GetKeyboardFocus()) && SDL_HasEvent(SDL_EVENT_TEXT_INPUT);
            if (oracles_login_made_up_key(raw, event->key.which, event->key.raw, text_waiting)) return 1;
            if (key_of(event->key.key, event->key.mod, &key)) *command = oracles_achievements_login_key(nav, key);
            else if (!SDL_TextInputActive(SDL_GetKeyboardFocus()) && !(event->key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI))) {
                /* A key while the system's text input is off (the first over the launcher's keyboard, or after Android's
                 * keyboard was put away): its character as the keyboard's layout and its modifiers make it; the
                 * next keys type through SDL_EVENT_TEXT_INPUT. */
                const SDL_Keycode k = SDL_GetKeyFromScancode(event->key.scancode, event->key.mod, false);
                char c[5];
                if (k >= 0x20u && k != 0x7fu && !(k & SDLK_SCANCODE_MASK) && !(k & SDLK_EXTENDED_MASK)) {
                    char *end = SDL_UCS4ToUTF8(k, c);
                    *end = 0;
                    oracles_achievements_login_text(nav, c);
                }
            }
            return 1;
        }
        case SDL_EVENT_KEY_UP: return 1;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            if (button_of(nav, oracles_sdl_pad_button(event), &key)) *command = oracles_achievements_login_key(nav, key);
            return 1;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: return 1;
        default: return 0;
    }
}

/* The system's text input for a user name or a password: neither corrected nor completed nor capitalised, the password
 * hidden (Android's keyboard neither shows nor learns it). */
static void start_text(SDL_Window *window, int password)
{
    const SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, SDL_PROP_TEXTINPUT_TYPE_NUMBER, password ? SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_HIDDEN : SDL_TEXTINPUT_TYPE_TEXT_USERNAME);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTINPUT_CAPITALIZATION_NUMBER, SDL_CAPITALIZE_NONE);
    SDL_SetBooleanProperty(props, SDL_PROP_TEXTINPUT_AUTOCORRECT_BOOLEAN, false);
    SDL_SetBooleanProperty(props, SDL_PROP_TEXTINPUT_MULTILINE_BOOLEAN, false);
    if (!props || !SDL_StartTextInputWithProperties(window, props)) SDL_StartTextInput(window);
    if (props) SDL_DestroyProperties(props);
}

void oracles_home_achievements_text_input(SDL_Window *window, OraclesHomeNav *nav)
{
    /* Android's keyboard for a touch or a key; with a controller, the launcher's own, the system's text input off so
     * that no system's keyboard (Android's, Steam's) comes over it.  Started again for the other field: its kind. */
    OraclesHomeAchievements *a = &nav->achievements;
    const int active = SDL_TextInputActive(window);
    const int wanted = oracles_achievements_text_wanted(nav, active);
    if (active && (!wanted || wanted != a->text_for)) SDL_StopTextInput(window);
    if (wanted && (!active || wanted != a->text_for)) start_text(window, wanted == ORACLES_LOGIN_PASSWORD + 1);
    a->text_for = wanted;
    a->system_keyboard = wanted && nav->android && SDL_ScreenKeyboardShown(window);
}
