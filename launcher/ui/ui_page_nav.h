/* Cartridge and Display, without
 * SDL: their rows, what the keys and the pointer do there, and their texts.
 *
 * Cartridge, per game: ROM (Choose ROM opens the system's file dialog; a ROM
 * that allows only Faithful, with Enhanced chosen in Display, says so under
 * its status), Save, Play.  Save has two actions, which left and right move
 * between: Open folder (Android: Export save…, the document picker for a
 * copy of the save, greyed without one) and Import save…, greyed without a
 * usable ROM and from a game, which opens the system's file dialog for a
 * save, then a confirmation over the page, Replace or Cancel (Import or
 * Cancel while the game has no save yet; left and right, OK; Back cancels): the import's success or its refusal stays as the row's
 * status until the page is left, in place of the note an Oracle's row has on
 * the European ROM.  A fan game made as a patch
 * (Gifts of Kinomi, Moonrise Regalia, Temple of Seasons) has a Base ROM row in place of ROM,
 * its Oracle's ROM that the Oracle's page shows too, then a Patch row (Choose
 * patch), its folder and the image the two make.  An Oracle whose ROM is the
 * original has two more rows after Save: European ROM (Choose…, or Change…
 * and Remove, which left and right move between; a file that is not the
 * game's European ROM says so) and Language: English (US), the US ROM's own,
 * and the five languages of the European ROM written in their own, greyed
 * with English (US) alone shown until the European ROM is recognized; left,
 * right and OK change it.  Removing the European ROM, or choosing another
 * file, sets the language back to English (US).  With nav->fan_languages on, a
 * fan game has the Language row after Save, its languages those of the
 * European ROM of its Oracle's page, with its Oracle's original ROM.
 * Up and down move through the rows and wrap; OK
 * activates the row; Back returns to the home screen, on Cartridge.  The
 * item hotkeys' mode is set in Controls and kept in settings.txt.
 *
 * Display, for every game: the profile (Faithful or
 * Enhanced), the quality (Low to Max: the view, the core and the neighbour
 * workers set at once; Custom when they make none, which is shown and never
 * chosen; dimmed and inert in Faithful and from a game), the window (the profile's surface at 2x, 3x, 4x, or fullscreen
 * at the largest whole scale of the display), the view, the HUD (the status bar as the game draws it, its colour
 * across the whole view, or laid out on it, hearts left and items right; dimmed and inert in Faithful), Text (the link to Display › Text, ui_page_text.h, its
 * settings summed up beside it, "Compact · Wide · Normal"), Picture (the link to Display › Picture, its settings summed
 * up beside it, "Sharp · No margin · Color off"), then Advanced
 * under the diagram (4:3: beside it, in the header), which opens Display's Advanced rows in place of those:
 * the core, continuous transitions (on by default, dimmed in Faithful), vsync, the neighbour workers, the camera (Pixel,
 * Smooth: the sub-pixel camera; dimmed and inert in Faithful; the platform's default shown until one is chosen).
 * Display › Picture, which Picture opens in their place, the diagram kept: the scaling (Sharp, the whole scales; Fill,
 * the whole screen in the image's proportions, in fullscreen only), the screen margin (Off, Auto, Small, Large: the band
 * of the screen the picture keeps to), color correction, the three in either profile, then Far size where it changes
 * something and Menus & scenes (below).  Left and right change the option of the row, OK takes its
 * next one (Quality stops at Low and at Max; from Custom, right takes Low and
 * left Max); on Advanced, OK and right open it, on Core, or from a game on
 * Vsync; on Picture, on Scaling; on Text, OK and right open Display › Text, on its Text row.  Where
 * every window comes to one size (a 640x480 screen, a
 * platform always fullscreen), the Window row is dimmed and changes no
 * more, and says so; it keeps the highlight.  The choices apply at the
 * next Play; Back returns from the Advanced rows to Advanced, from the Picture
 * rows to Picture, from the Text rows to Text, from Display to Display in the menu.  From a game a row acts
 * at once or not at all: the profile, the quality, the view, the core, the
 * transitions, the workers, the text, the text box and the far size, which the
 * game keeps, are dimmed and inert, "Change it from the home screen."; the
 * window, color correction, the HUD, Vsync, the scaling, the camera, the screen
 * margin, the menus and the text size apply at once there, the text speed when
 * the game resumes. */
#ifndef ORACLES_UI_PAGE_NAV_H
#define ORACLES_UI_PAGE_NAV_H

#include "ui_home_nav.h"

/* The rows in the page's order; a game that is not a patch has no Patch row, and only an Oracle whose ROM is the
 * original has the European ROM and Language rows. */
typedef enum OraclesGameRow {
    ORACLES_ROW_ROM,             /* a fan game's: its base ROM */
    ORACLES_ROW_PATCH,
    ORACLES_ROW_SAVE,
    ORACLES_ROW_EU_ROM,
    ORACLES_ROW_LANGUAGE,
    ORACLES_ROW_PLAY,
    ORACLES_GAME_ROWS
} OraclesGameRow;

/* The colour of a status line. */
typedef enum OraclesTone {
    ORACLES_TONE_OK,
    ORACLES_TONE_WARN,
    ORACLES_TONE_ERROR,
    ORACLES_TONE_NONE
} OraclesTone;

/* The game of the page (the right half must show a game, not the list of fan games). */
OraclesHomeGame *oracles_page_game(OraclesHomeNav *nav);
const OraclesHomeGame *oracles_page_game_const(const OraclesHomeNav *nav);

/* The profiles a ROM allows: all for an original, Faithful for an unrecognised one, none otherwise. */
int oracles_page_profile_allowed(const OraclesHomeGame *game, OraclesProfile profile);
/* The profile the game starts in: the one chosen in Display when its ROM allows it, else the first allowed; -1
 * without a usable ROM. */
int oracles_page_profile(const OraclesHomeGame *game, OraclesProfile chosen);

/* The page's texts. */
const char *oracles_page_rom_file(const OraclesHomeGame *game);         /* "None" without a ROM */
void oracles_page_rom_status(const OraclesHomeGame *game, char *out, size_t capacity, OraclesTone *tone);
/* A fan game's: its patch's file ("None" without one) and status, and the line on the image the two files make. */
const char *oracles_page_patch_file(const OraclesHomeGame *game);
void oracles_page_patch_status(const OraclesHomeGame *game, char *out, size_t capacity, OraclesTone *tone);
void oracles_page_image_status(const OraclesHomeGame *game, char *out, size_t capacity, OraclesTone *tone);
/* Under the status: that the ROM plays in Faithful, when Enhanced is chosen and the ROM does not allow it; else empty. */
const char *oracles_page_rom_note(const OraclesHomeNav *nav);
/* Under it: that the ROM plays without the item hotkeys Controls has on, when a session found they could not attach
 * to it; else empty. */
const char *oracles_page_rom_hotkeys_note(const OraclesHomeNav *nav);
/* A folder as the page shows it: on Android, a folder on the shared storage from its Android/data, as a computer sees
 * it over USB ("Android/data/org.theoraclesproject/files/roms"), and a folder of the application's private files
 * (/data/data/<id>/files/NAME), out of the player's reach, named "App storage › NAME"; any other folder, and every
 * folder elsewhere, by its path. */
void oracles_page_folder(int android, const char *folder, char *out, size_t capacity);
const char *oracles_page_save_file(const OraclesHomeGame *game);        /* "No save yet" without one */
void oracles_page_save_line(const OraclesHomeGame *game, char *out, size_t capacity);
/* The Save row's actions, 0 "Open folder" (Android: "Export save…") and 1 "Import save…", and whether each acts. */
#define ORACLES_PAGE_SAVE_ACTIONS 2
const char *oracles_page_save_action(const OraclesHomeNav *nav, int action);
int oracles_page_save_action_live(const OraclesHomeNav *nav, int action);
/* The note under the row's line, an Oracle's, the shorter one in 4:3: that the save works with the European ROM in
 * other emulators, beginning on Android with the export; on Android with Import save… highlighted, that it takes a save
 * from elsewhere, the European ROM's too; from a game, on Import, why it is greyed; empty for a fan game and while the
 * row shows a status. */
const char *oracles_page_save_note(const OraclesHomeNav *nav, int four_three);
/* The status an import or an export left (nav->save_flow), its tone and its second line; NULL while there is none. */
const char *oracles_page_save_status(const OraclesHomeNav *nav, OraclesTone *tone, const char **second);
/* What an import moved to Backups (save_transfer.h): the save it replaced, its savestate, and the game with mods' save,
 * savestate and mods' storage. */
enum {
    ORACLES_PAGE_KEPT_SAVE = 1u,
    ORACLES_PAGE_KEPT_STATE = 2u,
    ORACLES_PAGE_KEPT_MODS_SAVE = 4u,
    ORACLES_PAGE_KEPT_MODS_STATE = 8u,
    ORACLES_PAGE_KEPT_MODS_STORE = 16u
};
/* The status of an import done, on two lines of ORACLES_HOME_TEXT_LENGTH bytes that each hold in the row's room at
 * 640x480: that it is imported, and its language set to English (`english`); then what went to Backups, each file `kept`
 * names (ORACLES_PAGE_KEPT_*) said as it is, singular or plural, the game with mods' set as "with mods too" when it
 * matches the save's, and beside the ROM or a fan game's patch (`fan`) when only the save and its savestate went;
 * `second` empty when nothing went. */
void oracles_page_import_done(unsigned kept, int fan, int english, char *first, char *second);
const char *oracles_page_play_note(const OraclesHomeNav *nav);          /* shown beside Play; empty mostly */

/* The European ROM row: an Oracle's (not a fan game's), with its original US ROM.  The Language row: that Oracle's, or a
 * fan game's while nav->fan_languages is on. */
int oracles_page_eu_shown(const OraclesHomeGame *game);
int oracles_page_language_shown(const OraclesHomeNav *nav, const OraclesHomeGame *game);
/* The languages, each named in its own language, by index (oracles_settings_language_names' order): English (US), the
 * US ROM's, then English (EU), French, German, Italian and Spanish, the European ROM's.  English (US) is always there;
 * the others with the European ROM recognized (a fan game's: its Oracle's, with its Oracle's original ROM). */
#define ORACLES_PAGE_LANGUAGES 6
extern const char *const oracles_page_language_names[ORACLES_PAGE_LANGUAGES];
int oracles_page_language_available(const OraclesHomeGame *game, int language);
/* The language the game plays in: its choice where the rows show and the European ROM is recognized, else 0. */
int oracles_page_language(const OraclesHomeNav *nav, const OraclesHomeGame *game);
/* The European ROM's file ("None" without one) and status: optional, recognized with its languages, not the game's
 * ("Not the European ROM of Oracle of Ages"), or refused with the loader's reason. */
const char *oracles_page_eu_file(const OraclesHomeGame *game);
void oracles_page_eu_status(const OraclesHomeNav *nav, char *out, size_t capacity, OraclesTone *tone);
/* Its buttons: "Choose…" without a file, else "Change…" and "Remove". Returns their number. */
unsigned oracles_page_eu_buttons(const OraclesHomeGame *game, const char *labels[2]);
/* Language's explanation, and its note, empty in English (US): an Oracle's corrections, the share of a fan game's texts
 * the language translates. */
const char *oracles_page_language_explanation(const OraclesHomeGame *game);
void oracles_page_language_note(const OraclesHomeGame *game, char *out, size_t capacity);
extern const char *const oracles_profile_names[ORACLES_PROFILES];
/* The size written under a profile: Faithful's, or Enhanced's at the view chosen in the screen's shape. */
void oracles_profile_size(const OraclesHomeNav *nav, int profile, char *out, size_t capacity);

OraclesHomeCommand oracles_page_act(OraclesHomeNav *nav, OraclesHomeAction action);
/* The pointer over a row, or over one of its options (oracles_page_click's, -1 for none): the row highlighted, and Save's
 * action, European ROM's button or the confirmation's button under it chosen, as left and right would choose it. */
void oracles_page_hover(OraclesHomeNav *nav, unsigned row, int option);
/* On a game's page whose rows changed (a ROM found otherwise at a refresh, one dropped or chosen), the highlight moved
 * from a row the page no longer has to the nearest one it has, the one above first. */
void oracles_page_keep_row(OraclesHomeNav *nav);
/* A click on a row, or on one of its options: European ROM's buttons (0 Choose… or Change…, 1 Remove), a language
 * (its index), Save's actions (0, 1) and, while the import's confirmation is open, its Replace (2) and Cancel (3);
 * `option` -1 for the row itself. */
OraclesHomeCommand oracles_page_click(OraclesHomeNav *nav, unsigned row, int option);

/* Display's rows: its own, Advanced the last for the keys, Picture before it (Color correction's former place); the
 * Advanced rows, which show in their place; the Text rows (Display › Text, ui_page_text.h); and the Picture rows,
 * Scaling, Screen margin, Color correction, Far size and Menus & scenes for the keys (Display › Picture), which Picture
 * opens in their place. */
typedef enum OraclesDisplayRow {
    ORACLES_DISPLAY_PROFILE,
    ORACLES_DISPLAY_QUALITY,
    ORACLES_DISPLAY_WINDOW,
    ORACLES_DISPLAY_VIEW,
    ORACLES_DISPLAY_HUD,
    ORACLES_DISPLAY_TEXT,          /* the link to Display › Text, its settings summed up beside it */
    ORACLES_DISPLAY_COLOUR,        /* Display › Picture's Color correction */
    ORACLES_DISPLAY_ADVANCED,
    ORACLES_DISPLAY_CORE,
    ORACLES_DISPLAY_TRANSITIONS,
    ORACLES_DISPLAY_VSYNC,
    ORACLES_DISPLAY_WORKERS,
    ORACLES_DISPLAY_SCALING,
    ORACLES_DISPLAY_CAMERA,
    ORACLES_DISPLAY_MARGIN,
    ORACLES_DISPLAY_LETTERS,       /* Display › Text's Text: Classic or Compact */
    ORACLES_DISPLAY_TEXT_BOX,
    ORACLES_DISPLAY_TEXT_SIZE,
    ORACLES_DISPLAY_TEXT_SPEED,    /* Display › Text's Text speed: Classic or Instant */
    ORACLES_DISPLAY_PICTURE,       /* the link to Display › Picture, its settings summed up beside it */
    ORACLES_DISPLAY_MENUS,         /* Display › Picture's Menus & scenes: View or Large */
    ORACLES_DISPLAY_FAR_SIZE,      /* its Far size: Fitted or Fixed, where it changes something */
    ORACLES_DISPLAY_ROWS
} OraclesDisplayRow;

/* The row shows: one of Display's own, of the Advanced rows, of the Text rows or of the Picture rows, those of the page
 * open. */
int oracles_display_row_shown(const OraclesHomeNav *nav, unsigned row);
/* The workers Auto takes: two on a device of four processor cores or more, one otherwise, as the session counts them. */
int oracles_display_auto_workers(const OraclesHomeNav *nav);

/* Continuous transitions carry the Enhanced view: they apply in Enhanced only, as View and the HUD do. */
int oracles_display_transitions_apply(const OraclesHomeNav *nav);
/* The HUD applies in Enhanced: the profile chosen, or from the pause the running game's, which takes it at once. */
int oracles_display_hud_applies(const OraclesHomeNav *nav);
/* The screen is nearer 4:3 than 16:9: the Enhanced view takes the 4:3 sizes (docs/PLAYING.md). */
int oracles_display_screen_4_3(const OraclesHomeNav *nav);
/* View's choices and the size of each, the view's level in the screen's shape, far fitted to the screen as a game
 * started from here takes it (oracles_enhanced_screen_view_size). */
extern const char *const oracles_display_view_names[3];
void oracles_display_view_size(const OraclesHomeNav *nav, int view, char *out, size_t capacity);
/* Where a game started from here fits far to the screen (oracles_enhanced_far_fitting, far=screen, aspect=auto), the
 * note under View's help in 4:3, "Far fits this screen: 426x320 at 3x."; else empty. */
void oracles_display_far_note(const OraclesHomeNav *nav, char *out, size_t capacity);
/* Quality's choices, Low, Medium, High and Max, then Custom, which is never chosen: the view, the core and the workers
 * make it when they make no quality.  Each sets View, and Core and Neighbour workers in Advanced, as settings.c's
 * oracles_settings_apply_quality does: Low near, Medium medium, High far, all three on the Fast core with the workers
 * on Auto; Max far on the Accurate core with two workers. */
#define ORACLES_DISPLAY_QUALITIES 4
#define ORACLES_DISPLAY_QUALITY_CUSTOM 4
extern const char *const oracles_display_quality_names[ORACLES_DISPLAY_QUALITIES + 1];
/* The quality the view, the core and the workers make, ORACLES_DISPLAY_QUALITY_CUSTOM when none. */
int oracles_display_quality(const OraclesHomeNav *nav);
/* A quality applies in Enhanced chosen, and not from a game, whose core is fixed: the row is dimmed and inert else. */
int oracles_display_quality_applies(const OraclesHomeNav *nav);
/* What a choice sets, under its name: "Far · Fast", "By hand" for Custom. */
void oracles_display_quality_sets(int quality, char *out, size_t capacity);
/* The quality in effect said in full ("Far view, Fast core, neighbour workers on auto.", "Set by hand: ...") or short
 * (4:3's help: "Far view, Fast core, workers on auto."), and its note: that a hand change makes it Custom, or that a
 * quality sets the three again. */
void oracles_display_quality_text(const OraclesHomeNav *nav, int short_form, char *out, size_t capacity);
const char *oracles_display_quality_note(const OraclesHomeNav *nav, int short_form);
/* The profile the game shown plays in: the one chosen, or Faithful for an unrecognised ROM (the chosen one for a game
 * without a usable ROM, a fan game's included). */
OraclesProfile oracles_display_played_profile(const OraclesHomeNav *nav);
/* The surface of that profile, which the window's sizes count in. */
void oracles_display_surface(const OraclesHomeNav *nav, int *width, int *height);
/* The scale the game is shown at, its surface's on the screen: the window's, reduced to fit as Play reduces it, or
 * fullscreen's whole scale; with Fill in fullscreen, the screen's in the surface's proportions. */
float oracles_display_shown_scale(const OraclesHomeNav *nav);
/* The same in the band the screen margin leaves the game (margin_lines), as the game shows it there. */
float oracles_display_band_scale(const OraclesHomeNav *nav);
/* The whole scale of a choice of window: 2, 3, 4, or the largest the display holds. */
int oracles_display_scale(const OraclesHomeNav *nav, int window);
/* The largest whole scale at which a window of the surface fits the display's room, at least 1. */
int oracles_display_fit(const OraclesHomeNav *nav);
/* Every choice of Window comes to the same scale (2x, 3x and 4x reduced to the fit, fullscreen's no larger), or the
 * platform plays fullscreen only: the row does not change.  _for: on `display`, for the surface w x h. */
int oracles_display_one_size(const OraclesHomeNav *nav);
int oracles_display_one_size_for(const OraclesHomeDisplay *display, int w, int h);
/* The game plays fullscreen: Fullscreen chosen, the platform's only way, or Fill chosen at one size, where a window
 * would be no larger than the screen's whole scale and Fill takes the whole screen (the home screen's host starts it
 * so). */
int oracles_display_fullscreen(const OraclesHomeNav *nav);
extern const char oracles_display_one_size_note[];   /* "One size only here; Scaling in Advanced can fill the screen." */
/* From the pause, under windows it refuses (nav->display.windows_refused): "Smaller windows change from the home screen." */
extern const char oracles_display_windows_refused_note[];
/* After "Reduced to 3x to fit this screen" while the pause refuses some: "; smaller ones from the home screen." */
extern const char oracles_display_windows_refused_tail[];
/* The line under the windows: oracles_display_one_size_note at one size, else that a windowed scale chosen above the
 * fit is reduced to it, "Reduced to 3x to fit this screen.", else oracles_display_windows_refused_note while the pause
 * refuses some, the two on one line when both hold; empty when it fits. */
void oracles_display_reduced(const OraclesHomeNav *nav, char *out, size_t capacity);
/* The page's texts: a window's name and size, the note under the windows, the diagram's line, the profile's note. */
void oracles_display_window_texts(const OraclesHomeNav *nav, int window, char *name, char *size, size_t capacity);
extern const char oracles_display_window_note[];
/* The characters of the longest size a choice of Window shows over every surface of the screen's shape, Faithful's
 * and each view's: its frame keeps that width whatever the profile and the view. */
int oracles_display_window_size_length(const OraclesHomeNav *nav, int window);
/* The scaling's choices, Sharp and Fill, and the size of the image in fullscreen with each: the largest whole scale
 * of the surface, or the largest size in its proportions (1920x1080). */
extern const char *const oracles_display_scaling_choices[2];
void oracles_display_scaling_size(const OraclesHomeNav *nav, int scaling, char *out, size_t capacity);
/* Camera's choices, "Pixel" and "Smooth" (settings.txt's subpixel_camera= 0 and 1), which apply in Enhanced as the HUD
 * does; the one shown: the player's, else the platform's default at the whole scale the game is shown at
 * (oracles_display_camera_scale, the band's scale less its fraction); and its note where that scale is under twice, where
 * the camera does nothing, "No effect at this size: the view fills the screen at 1x.", else empty. */
extern const char *const oracles_display_camera_choices[2];
int oracles_display_camera_applies(const OraclesHomeNav *nav);
unsigned oracles_display_camera_scale(const OraclesHomeNav *nav);
int oracles_display_camera(const OraclesHomeNav *nav);
const char *oracles_display_camera_note(const OraclesHomeNav *nav);
/* The diagram: the window the game will have, reduced to fit when it must, on the screen; with Fill chosen, when the
 * game plays fullscreen (oracles_display_fullscreen), the image filling the screen in its proportions. */
void oracles_display_diagram(const OraclesHomeNav *nav, float box_w, float box_h, float *w, float *h, char *label, size_t capacity);
/* The chosen profile's note, or that the game shown plays in Faithful. */
const char *oracles_display_profile_note(const OraclesHomeNav *nav);
extern const char *const oracles_display_labels[ORACLES_DISPLAY_ROWS];
extern const char *const oracles_display_colour_choices[2];
extern const char *const oracles_display_hud_choices[3];    /* "Original", "Full width", "Wide layout": settings.txt's hud= original, full, wide */
/* "Off", "Auto", "Small", "Large": settings.txt's screen_margin=, an OraclesScreenMargin. */
#define ORACLES_DISPLAY_MARGINS 4
extern const char *const oracles_display_margin_choices[ORACLES_DISPLAY_MARGINS];
extern const char *const oracles_display_vsync_choices[3];
extern const char *const oracles_display_core_choices[2];   /* "Accurate (SameBoy)", "Fast (mGBA)": an OraclesCoreKind */
extern const char *const oracles_display_core_names[2];     /* "Accurate", "Fast" */
/* The workers' choices: "Auto · 2" (the count Auto takes), "1", "2". */
void oracles_display_workers_choice(const OraclesHomeNav *nav, int choice, char *out, size_t capacity);
/* Display's section name: "Display", or "Display › Advanced", "Display › Picture" or "Display › Text" while those rows
 * show. */
const char *oracles_display_section(const OraclesHomeNav *nav);
/* From the pause a row acts at once or not at all: Profile, Quality, View, Core, Continuous transitions, Neighbor
 * workers, Text, Text box and Far size, which a running game keeps, are dimmed and inert there, saying why
 * (oracles_display_pause_reason). */
int oracles_display_pause_locked(const OraclesHomeNav *nav, unsigned row);
extern const char oracles_display_pause_reason[];   /* "Change it from the home screen." */
/* What a fan game has not yet (Display › Text's Text box and Text size, Controls' Swimming and Item hotkeys from its
 * pause): "Ages and Seasons only for now." */
extern const char oracles_display_fan_reason[];
/* A row dimmed and inert: locked from the pause (oracles_display_pause_locked), or the screen margin --screen-margin
 * gives for the run, and the text, the text box and the text size --text, --text-box and --text-size give. */
int oracles_display_row_fixed(const OraclesHomeNav *nav, unsigned row);
/* A row's explanation beside its choices (16:9; 4:3 says shorter ones in its header); Advanced's and Text's say what
 * they open. */
const char *oracles_display_explanation(unsigned row);
/* Color correction's, 16:9's or 4:3's shorter one, which names F2 only where a keyboard is at hand
 * (oracles_home_keys_shown). */
const char *oracles_display_colour_explanation(const OraclesHomeNav *nav, int four_three);
/* Display › Picture's last two: Far size, "Fitted" or "Fixed" (far=screen|fixed), the far view fitted to a screen its
 * size fills badly or kept at its own, a row only where far is fitted to the screen (oracles_display_far_size_shown:
 * aspect=auto, 1280x960 or 1240x1080); Menus & scenes, "View" or "Large" (menus=view|large, Large Android's default),
 * the game's menus, map, cutscenes and file screen framed at the view's scale or shown alone at their own largest one.
 * Both apply in Enhanced, as the HUD does (the profile chosen, from the pause the running game's): dimmed and inert
 * elsewhere, "Enhanced only.".  From the pause Menus & scenes applies at once; Far size, which changes the view, is dimmed
 * there.  A row's note, under its explanation (16:9) or on the help's note line (4:3): that reason, else Far size's sizes
 * on this screen, "Here Fitted is 426x320 at 3x; Fixed is 480x360 at 2x."; empty for none.  Picture's summary: the
 * scaling, the margin and the colors, "Sharp · No margin · Color off", then " · Far fixed" and " · Large menus" (" · View
 * menus" on Android) where they apply and differ from the platform's default. */
int oracles_display_far_size_shown(const OraclesHomeNav *nav);
void oracles_display_picture_summary(const OraclesHomeNav *nav, char *out, size_t capacity);
extern const char *const oracles_display_menus_choices[2];
extern const char *const oracles_display_far_size_choices[2];
int oracles_display_size_row_live(const OraclesHomeNav *nav, unsigned row);
void oracles_display_size_note(const OraclesHomeNav *nav, unsigned row, char *out, size_t capacity);

OraclesHomeCommand oracles_display_act(OraclesHomeNav *nav, OraclesHomeAction action);
/* A click on a row, or on one of its options (a profile, a quality, a window, Off/On, Auto/On/Off): `option` -1 for the
 * row itself; one on Advanced, Picture or Text opens it. */
OraclesHomeCommand oracles_display_click(OraclesHomeNav *nav, unsigned row, int option);

/* ---- Mods ------------------------------------------------------------------------
 *
 * Mods, for an Oracle (a fan game's menu greys it): the mods folder (Open folder
 * shows it in the system's file manager), the mods found there with a switch
 * each, how many are active, and Play, which starts the game with the active
 * mods on its own save, NAME.mods.sav.  A refused mod says why and does not
 * switch on; eight at most are active.  Up and down move through the Folder,
 * each mod and Play, and wrap; left and right switch the highlighted mod off
 * and on, OK switches it, opens the folder or plays; Back returns to the home
 * screen, on Mods.  The choices are kept in settings.txt at once. */

/* The page's rows: Folder, each mod, Play. */
#define ORACLES_MODS_ROW_FOLDER 0u
unsigned oracles_mods_rows(const OraclesHomeNav *nav);
unsigned oracles_mods_row_play(const OraclesHomeNav *nav);
/* The mods of the page's game (Ages' or Seasons'). */
const OraclesHomeMods *oracles_mods_list(const OraclesHomeNav *nav);
/* The mods active for an Oracle (ORACLES_HOME_GAME_AGES or _SEASONS). */
unsigned oracles_mods_active(const OraclesHomeNav *nav, int game);
/* The page's texts: a mod's games ("Houses in Ages and Seasons", empty for a refused mod or one without a house),
 * the count under the list, the note on the save, and why Play cannot start (empty when it can). */
void oracles_mods_games(const OraclesHomeMod *mod, char *out, size_t capacity);
void oracles_mods_count(const OraclesHomeNav *nav, char *out, size_t capacity);
extern const char oracles_mods_empty[], oracles_mods_note[], oracles_mods_limit[];
const char *oracles_mods_play_note(const OraclesHomeNav *nav);
/* The folder the page shows, as oracles_page_folder shows a folder (on Android, from its Android/data), and the line
 * under it: beside the launcher's settings, or on Android reachable over USB. */
void oracles_mods_folder(const OraclesHomeNav *nav, char *out, size_t capacity);
const char *oracles_mods_folder_line(const OraclesHomeNav *nav);

OraclesHomeCommand oracles_mods_act(OraclesHomeNav *nav, OraclesHomeAction action);
void oracles_mods_hover(OraclesHomeNav *nav, unsigned row);
/* A click on a row: a mod's switches it. */
OraclesHomeCommand oracles_mods_click(OraclesHomeNav *nav, unsigned row);

/* ---- Cheats ----------------------------------------------------------------------
 *
 * Cheats, for an Oracle whose profile has them (a fan game's menu and pause do
 * not have it, nor an Oracle's on an unrecognised ROM): No damage,
 * Ammo never runs out and Rupees never run out, each Off or On.  Up and down
 * move through the three rows and wrap; left, right and OK turn the
 * highlighted cheat off and on, a click on Off or On sets it.  Each change is kept in
 * settings.txt at once (cheats_<game>=): the next Play takes it, and from the
 * pause the running game at once, the page there starting from the running
 * game's cheats.  With --cheats, which holds for the run, the home screen's
 * page shows its cheats, dimmed and inert.  Back returns to the menu, on
 * Cheats. */

#define ORACLES_CHEATS_ROWS 3
/* The rows are the bits of OraclesHomeGame.cheats, in settings.txt's order: no-damage (1), ammo (2), rupees (4). */
extern const char *const oracles_cheats_labels[ORACLES_CHEATS_ROWS];
/* Each row's explanation and its note (empty: none); 16:9 joins them beside the choices, 4:3 says the highlighted
 * row's under the rows, the note on its own line. */
extern const char *const oracles_cheats_texts[ORACLES_CHEATS_ROWS];
extern const char *const oracles_cheats_notes[ORACLES_CHEATS_ROWS];
extern const char oracles_cheats_note[];          /* under the title: "Cheats play with your usual save: ..." */
/* A row's explanation and note as one text, 16:9's. */
void oracles_cheats_explanation(unsigned row, char *out, size_t capacity);
/* The row's cheat is on for the page's game. */
int oracles_cheats_on(const OraclesHomeNav *nav, unsigned row);
/* The row changes: every row, and none while --cheats holds them for the run (nav->cheats_fixed). */
int oracles_cheats_row_live(const OraclesHomeNav *nav, unsigned row);

OraclesHomeCommand oracles_cheats_act(OraclesHomeNav *nav, OraclesHomeAction action);
/* A click on a row, or on its Off (0) or On (1): `option` -1 for the row itself. */
OraclesHomeCommand oracles_cheats_click(OraclesHomeNav *nav, unsigned row, int option);

/* ---- About -----------------------------------------------------------------------
 *
 * About, opened by the home screen's version number, laid out as Cheats: "The
 * Oracles Project" for its title, the version and, when the home screen shows
 * one, the newer version's notice for its note ("v1.2.0 (abc1234) · 1.3.0
 * available"), and one row, Update check, On or Off (settings.txt's
 * update_check=): Off hides the notice at once, and the next launch asks
 * nothing.  Left, right and OK turn it off and on, a click on On or Off sets
 * it; each change is kept at once.  Back returns to the home screen, on the
 * version number. */

extern const char oracles_about_label[];          /* "Update check" */
extern const char *const oracles_about_choices[2];  /* "On", "Off" */
extern const char oracles_about_explanation[];    /* 16:9's, beside the choices */
extern const char oracles_about_help[], oracles_about_help_note[];   /* 4:3's, under the row */
/* The note under the title: `version` ("v1.2.0 (abc1234)"), and " · 1.3.0 available" while the home screen shows a newer
 * one. */
void oracles_about_note(const OraclesHomeNav *nav, const char *version, char *out, size_t capacity);
OraclesHomeCommand oracles_about_act(OraclesHomeNav *nav, OraclesHomeAction action);
/* A click on the row, or on its On (0) or Off (1): `option` -1 for the row itself. */
OraclesHomeCommand oracles_about_click(OraclesHomeNav *nav, int option);

#endif
