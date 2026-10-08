/* Cartridge and Display, without
 * SDL: their rows, what the keys and the pointer do there, and their texts.
 *
 * Cartridge, per game: ROM (Choose ROM opens the system's file dialog; a ROM
 * that allows only Faithful, with Enhanced chosen in Display, says so under
 * its status), Save (Open folder), Play.  A fan game made as a patch
 * (Gifts of Kinomi, Moonrise Regalia, Temple of Seasons) has a Base ROM row in place of ROM,
 * its Oracle's ROM that the Oracle's page shows too, then a Patch row (Choose
 * patch), its folder and the image the two make.  Up and down move through the rows and wrap; OK
 * activates the row; Back returns to the home screen, on Cartridge.  The
 * item hotkeys' mode is set in Controls and kept in settings.txt.
 *
 * Display, for every game: the profile (Faithful or
 * Enhanced), the quality (Low to Max: the view, the core and the neighbour
 * workers set at once; Custom when they make none, which is shown and never
 * chosen; dimmed and inert in Faithful and from a game), the window (the profile's surface at 2x, 3x, 4x, or fullscreen
 * at the largest whole scale of the display), the view, color correction,
 * continuous transitions (on by default, dimmed in Faithful), then Advanced
 * under the diagram (4:3: beside it, in the header), which opens Display's Advanced rows in place of those:
 * the core, vsync, the neighbour workers.  No camera: Enhanced takes the
 * smooth one.  Left and right change the option of the row, OK takes its
 * next one (Quality stops at Low and at Max; from Custom, right takes Low and
 * left Max); on Advanced, OK and right open it, on Core, or from a game on
 * Vsync.  Where every window comes to one size (a 640x480 screen, a
 * platform always fullscreen), the Window row is dimmed and changes no
 * more, and says so; it keeps the highlight.  The choices apply at the
 * next Play; Back returns from the Advanced rows to Advanced, from Display
 * to Display in the menu.  From a game the core and the workers are the
 * game's, dimmed and inert, and so is the quality. */
#ifndef ORACLES_UI_PAGE_NAV_H
#define ORACLES_UI_PAGE_NAV_H

#include "ui_home_nav.h"

/* The rows in the page's order; a game that is not a patch has no Patch row. */
typedef enum OraclesGameRow {
    ORACLES_ROW_ROM,             /* a fan game's: its base ROM */
    ORACLES_ROW_PATCH,
    ORACLES_ROW_SAVE,
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
const char *oracles_page_save_file(const OraclesHomeGame *game);        /* "No save yet" without one */
void oracles_page_save_line(const OraclesHomeGame *game, char *out, size_t capacity);
const char *oracles_page_play_note(const OraclesHomeNav *nav);          /* shown beside Play; empty mostly */
extern const char *const oracles_profile_names[ORACLES_PROFILES];
/* The size written under a profile: Faithful's, or Enhanced's at the view chosen in the screen's shape. */
void oracles_profile_size(const OraclesHomeNav *nav, int profile, char *out, size_t capacity);

OraclesHomeCommand oracles_page_act(OraclesHomeNav *nav, OraclesHomeAction action);
void oracles_page_hover(OraclesHomeNav *nav, unsigned row);
/* A click on a row. */
OraclesHomeCommand oracles_page_click(OraclesHomeNav *nav, unsigned row);

/* Display's rows in the order of the keys: its own, Advanced last; then the Advanced rows, which show in their place. */
typedef enum OraclesDisplayRow {
    ORACLES_DISPLAY_PROFILE,
    ORACLES_DISPLAY_QUALITY,
    ORACLES_DISPLAY_WINDOW,
    ORACLES_DISPLAY_VIEW,
    ORACLES_DISPLAY_COLOUR,
    ORACLES_DISPLAY_TRANSITIONS,
    ORACLES_DISPLAY_ADVANCED,
    ORACLES_DISPLAY_CORE,
    ORACLES_DISPLAY_VSYNC,
    ORACLES_DISPLAY_WORKERS,
    ORACLES_DISPLAY_ROWS
} OraclesDisplayRow;

/* The row shows: one of Display's own, or of the Advanced rows while they are open. */
int oracles_display_row_shown(const OraclesHomeNav *nav, unsigned row);
/* The workers Auto takes: two on a device of four processor cores or more, one otherwise, as the session counts them. */
int oracles_display_auto_workers(const OraclesHomeNav *nav);

/* Continuous transitions carry the Enhanced view: they apply in Enhanced only, as View does. */
int oracles_display_transitions_apply(const OraclesHomeNav *nav);
/* The screen is nearer 4:3 than 16:9: the Enhanced view takes the 4:3 sizes (docs/PLAYING.md). */
int oracles_display_screen_4_3(const OraclesHomeNav *nav);
/* View's choices and the size of each, the view's level in the screen's shape. */
extern const char *const oracles_display_view_names[3];
void oracles_display_view_size(const OraclesHomeNav *nav, int view, char *out, size_t capacity);
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
/* The whole scale of a choice of window: 2, 3, 4, or the largest the display holds. */
int oracles_display_scale(const OraclesHomeNav *nav, int window);
/* The largest whole scale at which a window of the surface fits the display's room, at least 1. */
int oracles_display_fit(const OraclesHomeNav *nav);
/* Every choice of Window comes to the same scale (2x, 3x and 4x reduced to the fit, fullscreen's no larger), or the
 * platform plays fullscreen only: the row does not change. */
int oracles_display_one_size(const OraclesHomeNav *nav);
extern const char oracles_display_one_size_note[];   /* "This screen shows the game at one size only." */
/* The line under the windows: oracles_display_one_size_note at one size, else that a windowed scale chosen above the
 * fit is reduced to it, "Reduced to 3x to fit this screen."; empty when it fits. */
void oracles_display_reduced(const OraclesHomeNav *nav, char *out, size_t capacity);
/* The page's texts: a window's name and size, the note under the windows, the diagram's line, the profile's note. */
void oracles_display_window_texts(const OraclesHomeNav *nav, int window, char *name, char *size, size_t capacity);
extern const char oracles_display_window_note[];
/* The characters of the longest size a choice of Window shows over every surface of the screen's shape, Faithful's
 * and each view's: its frame keeps that width whatever the profile and the view. */
int oracles_display_window_size_length(const OraclesHomeNav *nav, int window);
/* The diagram: the window the game will have, reduced to fit when it must, on the screen. */
void oracles_display_diagram(const OraclesHomeNav *nav, float box_w, float box_h, float *w, float *h, char *label, size_t capacity);
/* The chosen profile's note, or that the game shown plays in Faithful. */
const char *oracles_display_profile_note(const OraclesHomeNav *nav);
extern const char *const oracles_display_labels[ORACLES_DISPLAY_ROWS];
extern const char *const oracles_display_colour_choices[2];
extern const char *const oracles_display_vsync_choices[3];
extern const char *const oracles_display_core_choices[2];   /* "Accurate (SameBoy)", "Fast (mGBA)": an OraclesCoreKind */
extern const char *const oracles_display_core_names[2];     /* "Accurate", "Fast" */
/* The workers' choices: "Auto · 2" (the count Auto takes), "1", "2". */
void oracles_display_workers_choice(const OraclesHomeNav *nav, int choice, char *out, size_t capacity);
/* Display's section name: "Display", or "Display › Advanced" while the Advanced rows show. */
const char *oracles_display_section(const OraclesHomeNav *nav);
/* The core and the workers are fixed for a running game, and so the quality, which sets them: from it their rows are
 * dimmed and inert. */
int oracles_display_row_fixed(const OraclesHomeNav *nav, unsigned row);
/* A row's explanation beside its choices (16:9; 4:3 says shorter ones under the rows); Advanced's says what it opens. */
const char *oracles_display_explanation(unsigned row);

OraclesHomeCommand oracles_display_act(OraclesHomeNav *nav, OraclesHomeAction action);
/* A click on a row, or on one of its options (a profile, a quality, a window, Off/On, Auto/On/Off): `option` -1 for the
 * row itself; one on Advanced opens it. */
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
extern const char oracles_mods_folder_line[], oracles_mods_empty[], oracles_mods_note[], oracles_mods_limit[];
const char *oracles_mods_play_note(const OraclesHomeNav *nav);

OraclesHomeCommand oracles_mods_act(OraclesHomeNav *nav, OraclesHomeAction action);
void oracles_mods_hover(OraclesHomeNav *nav, unsigned row);
/* A click on a row: a mod's switches it. */
OraclesHomeCommand oracles_mods_click(OraclesHomeNav *nav, unsigned row);

#endif
