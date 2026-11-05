/* --launcher-screenshot: the home screen drawn offscreen, for the captures
 * and for the test that runs without a display. */
#include "home.h"

#include "backends.h"
#include "pause.h"
#include "touch_controls.h"
#include "ui_controls_nav.h"
#include "ui_home.h"
#include "ui_page_nav.h"
#include "update_check.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_action(const char *word, size_t length, OraclesHomeAction *action)
{
    static const char *const names[] = { "up", "down", "left", "right", "ok", "back" };
    for (int i = 0; i < 6; i++)
        if (strlen(names[i]) == length && !strncmp(word, names[i], length)) { *action = (OraclesHomeAction)i; return 1; }
    return 0;
}

static int write_ppm(const char *path, const SDL_Surface *surface)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fprintf(f, "P6\n%d %d\n255\n", surface->w, surface->h);
    for (int y = 0; y < surface->h; y++) {
        const Uint32 *row = (const Uint32 *)((const Uint8 *)surface->pixels + (size_t)y * (size_t)surface->pitch);
        for (int x = 0; x < surface->w; x++) {
            const Uint8 rgb[3] = { (Uint8)(row[x] >> 16), (Uint8)(row[x] >> 8), (Uint8)row[x] };
            fwrite(rgb, 1, 3, f);
        }
    }
    return fclose(f) == 0;
}

/* A binary PPM, as --screenshot writes the game's image, into a texture of `renderer`. */
static SDL_Texture *read_ppm(SDL_Renderer *renderer, const char *path, int *width, int *height)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    int maximum = 0;
    SDL_Texture *texture = NULL;
    /* The one whitespace byte after the header, then the pixels. */
    if (fscanf(f, "P6 %d %d %d", width, height, &maximum) == 3 && maximum == 255 && *width > 0 && *height > 0 && *width <= 4096 && *height <= 4096
        && fgetc(f) != EOF) {
        const size_t count = (size_t)*width * (size_t)*height;
        unsigned char *rgb = malloc(count * 3u);
        uint32_t *pixels = malloc(count * sizeof *pixels);
        if (rgb && pixels && fread(rgb, 3, count, f) == count) {
            for (size_t i = 0; i < count; i++) pixels[i] = 0xff000000u | (uint32_t)rgb[3 * i] << 16 | (uint32_t)rgb[3 * i + 1] << 8 | rgb[3 * i + 2];
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, *width, *height);
            if (texture) SDL_UpdateTexture(texture, NULL, pixels, *width * 4);
            if (texture) SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);   /* the session's, SDL 3 defaulting to linear */
        }
        free(rgb);
        free(pixels);
    }
    fclose(f);
    return texture;
}

/* The pause of `game` over its frame, in the profile its page plays. */
static void pause(OraclesHomeNav *nav, int game, int width)
{
    const int profile = oracles_page_profile(&nav->games[game], nav->display.profile);
    oracles_home_pause(nav, game, profile < 0 ? ORACLES_PROFILE_FAITHFUL : (OraclesProfile)profile, oracles_ui_layout_narrow(nav->layout, width));
}

int oracles_home_screenshot(const OraclesHomeHost *host, const char *path, int width, int height, const char *inputs,
                            const char *frame, char *error, size_t capacity)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    if (host && host->refresh) host->refresh(host->opaque, &nav, NULL);
    /* The capture stands for a screen of its size: Display's sizes are those of that screen. */
    nav.display.screen_w = nav.display.room_w = width;
    nav.display.screen_h = height;
    nav.display.room_h = height - 64;
    /* Over a game's frame, the pause's layout, as the game shows it. */
    nav.layout = frame ? oracles_ui_layout_pause(width, height, nav.display.aspect) : oracles_ui_layout_choose(width, height, nav.display.aspect);
    /* The update check's answer, which main waited for: a newer version shown at rest. */
    OraclesUpdateStatus update;
    oracles_update_check_status(&update);
    if (update.state == ORACLES_UPDATE_NEWER) snprintf(nav.update, sizeof nav.update, "%s", update.version);
    if (frame) pause(&nav, ORACLES_HOME_GAME_AGES, width);
    int reminder = 0;
    /* The inputs move the navigation only: a game is not started, the launcher does not exit.  `slow` is the return
     * from a game that ran slowly, its toast shown until the next input; `key` and `pad`, a key or a controller's
     * button as the device used last, which the hints follow; `android`, the screens as Android has them; `moonrise`,
     * over a game's frame, the pause of Moonrise Regalia rather than Ages'; `import`, on a game's page, the import's
     * confirmation as the page's own save chosen would open it; `imported`, there, the row's status after an import
     * that kept the save and its savestate in Backups and set the language to English; `mods` before either, the game
     * with mods having a save, a savestate and mods' data of its own, which the import puts in Backups too, and which
     * Mods' Save row (`android`) exports; `reminder`,
     * over a game's frame, the game in play with the reminder over it that names the menu's key, in place of the pause. */
    int mods = 0;
    for (const char *p = inputs; p && *p;) {
        const size_t length = strcspn(p, ",");
        OraclesHomeAction action;
        if (length == 4 && !strncmp(p, "slow", 4)) nav.slow = 1;
        else if (length == 3 && !strncmp(p, "key", 3)) oracles_home_input(&nav, ORACLES_INPUT_KEYBOARD);
        else if (length == 3 && !strncmp(p, "pad", 3)) oracles_home_input(&nav, ORACLES_INPUT_GAMEPAD);
        else if (length == 7 && !strncmp(p, "android", 7)) nav.android = nav.display.fullscreen_only = 1;
        else if (length == 8 && !strncmp(p, "moonrise", 8) && frame) pause(&nav, ORACLES_HOME_GAME_MOONRISE, width);
        else if (length == 4 && !strncmp(p, "mods", 4)) mods = nav.games[ORACLES_HOME_GAME_AGES].mods_save = nav.games[ORACLES_HOME_GAME_SEASONS].mods_save = 1;
        else if (length == 8 && !strncmp(p, "reminder", 8) && frame) reminder = 1;
        else if (length == 6 && !strncmp(p, "import", 6) && nav.screen == ORACLES_SCREEN_GAME) {
            const OraclesHomeGame *game = oracles_page_game_const(&nav);
            nav.save_flow.state = ORACLES_SAVE_FLOW_CONFIRM;
            nav.save_flow.replace = mods ? 2 : game->save_file[0] != 0;
            /* Without a save, the ROM's name with the save's extension. */
            const char *dot = strrchr(game->rom_file, '.');
            if (game->save_file[0]) snprintf(nav.save_flow.file, sizeof nav.save_flow.file, "%s", game->save_file);
            else snprintf(nav.save_flow.file, sizeof nav.save_flow.file, "%.*s.sav", dot ? (int)(dot - game->rom_file) : (int)strlen(game->rom_file), game->rom_file);
            /* Its folder; on Android, as the document picker gives a save from the downloads. */
            if (nav.android) snprintf(nav.save_flow.folder, sizeof nav.save_flow.folder, "Download");
            else oracles_page_folder(0, game->rom_folder, nav.save_flow.folder, sizeof nav.save_flow.folder);
        }
        else if (length == 8 && !strncmp(p, "imported", 8) && nav.screen == ORACLES_SCREEN_GAME) {
            nav.save_flow.state = ORACLES_SAVE_FLOW_DONE;
            oracles_page_import_done(mods ? 31u : ORACLES_PAGE_KEPT_SAVE | ORACLES_PAGE_KEPT_STATE, oracles_page_game_const(&nav)->fan != NULL, 1,
                                     nav.save_flow.text, nav.save_flow.text2);
        }
        else if (!parse_action(p, length, &action)) {
            snprintf(error, capacity, "unknown input '%.*s' (up, down, left, right, ok, back, slow, key, pad, android, moonrise, mods, import, imported, reminder)",
                     (int)length, p);
            return 1;
        }
        else oracles_home_act(&nav, action);
        p += length + (p[length] == ',');
    }
    SDL_Surface *surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer *renderer = surface ? SDL_CreateSoftwareRenderer(surface) : NULL;
    OraclesUiDraw *draw = renderer ? oracles_ui_draw_create(renderer) : NULL;
    int frame_width = 0, frame_height = 0;
    SDL_Texture *image = draw && frame ? read_ppm(renderer, frame, &frame_width, &frame_height) : NULL;
    int status = 1;
    if (!draw) {
        snprintf(error, capacity, "cannot draw offscreen: %s", SDL_GetError());
    } else if (frame && !image) {
        snprintf(error, capacity, "cannot read the game's image %s (a binary PPM, as --screenshot writes)", frame);
    } else if (frame && reminder) {
        /* The game's image at its whole scale, centred, as a game in play shows it, and the reminder over it. */
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        const OraclesTouchBox box = oracles_touch_frame(width, height, frame_width, frame_height, 0);
        const SDL_FRect at = { box.x, box.y, box.w, box.h };
        SDL_RenderTexture(renderer, image, NULL, &at);
        OraclesPause *over = oracles_pause_create(draw, NULL, ORACLES_HOME_GAME_AGES, ORACLES_PROFILE_ENHANCED, nav.display.aspect, 0, &nav);
        const OraclesPauseFrame shown = { 1, -1, frame_width, frame_height, 0 };
        if (over) {
            oracles_pause_reminder(over, nav.controls.menu_pad);
            oracles_pause_overlay(over, renderer, &shown);
        }
        oracles_pause_destroy(over);
        SDL_RenderPresent(renderer);
        if (over && write_ppm(path, surface)) status = 0;
        else snprintf(error, capacity, "cannot write %s", path);
    } else if (frame) {
        /* The game fullscreen when Display says so, Scaling's Fill then filling the capture as it fills the screen. */
        int presentation, scale_mode;
        oracles_sdl_presentation(oracles_display_fullscreen(&nav), nav.display.scaling, &presentation, &scale_mode);
        SDL_SetTextureScaleMode(image, (SDL_ScaleMode)scale_mode);
        OraclesUiHome view;
        oracles_controls_tips_opened(&nav, 0);   /* the pause as it opens, its line on the item hotkeys when due */
        oracles_ui_home_start(&view, &nav);
        oracles_pause_paint(draw, renderer, image, frame_width, frame_height, presentation == SDL_LOGICAL_PRESENTATION_LETTERBOX, width, height,
                            &view, &nav, 0.0, NULL);
        SDL_RenderPresent(renderer);
        if (write_ppm(path, surface)) status = 0;
        else snprintf(error, capacity, "cannot write %s", path);
    } else {
        OraclesUiHome view;
        oracles_ui_home_start(&view, &nav);
        oracles_ui_draw_layout(draw, oracles_ui_home_scene(&nav));
        if (!oracles_ui_draw_begin(draw, width, height, 0.0)) {
            snprintf(error, capacity, "cannot draw offscreen: %s", SDL_GetError());
        } else {
            oracles_ui_home_draw(draw, &view, &nav, 0.0);
            oracles_ui_draw_end(draw);
            SDL_RenderPresent(renderer);
            if (write_ppm(path, surface)) status = 0;
            else snprintf(error, capacity, "cannot write %s", path);
        }
    }
    if (image) SDL_DestroyTexture(image);
    oracles_ui_draw_destroy(draw);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (surface) SDL_DestroySurface(surface);
    return status;
}
