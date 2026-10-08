/* --launcher-screenshot: the home screen drawn offscreen, for the captures
 * and for the test that runs without a display. */
#include "home.h"

#include "pause.h"
#include "ui_home.h"
#include "ui_page_nav.h"

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

int oracles_home_screenshot(const OraclesHomeHost *host, const char *path, int width, int height, const char *inputs,
                            const char *frame, char *error, size_t capacity)
{
    OraclesHomeNav nav;
    oracles_home_init(&nav);
    if (host && host->refresh) host->refresh(host->opaque, &nav, NULL);
    /* Over a game's frame, the pause's layout, as the game shows it. */
    nav.layout = frame ? oracles_ui_layout_pause(width, height, nav.display.aspect) : oracles_ui_layout_choose(width, height, nav.display.aspect);
    if (frame) {
        const int profile = oracles_page_profile(&nav.games[0], nav.display.profile);
        oracles_home_pause(&nav, ORACLES_HOME_GAME_AGES, profile < 0 ? ORACLES_PROFILE_FAITHFUL : (OraclesProfile)profile,
                           oracles_ui_layout_narrow(nav.layout, width));
    }
    /* The inputs move the navigation only: a game is not started, the launcher does not exit. */
    for (const char *p = inputs; p && *p;) {
        const size_t length = strcspn(p, ",");
        OraclesHomeAction action;
        if (!parse_action(p, length, &action)) { snprintf(error, capacity, "unknown input '%.*s' (up, down, left, right, ok, back)", (int)length, p); return 1; }
        oracles_home_act(&nav, action);
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
    } else if (frame) {
        OraclesUiHome view;
        oracles_ui_home_start(&view, &nav);
        oracles_pause_paint(draw, renderer, image, frame_width, frame_height, width, height, &view, &nav, 0.0);
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
