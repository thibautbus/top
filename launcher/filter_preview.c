#include "filter_preview.h"

#include "sdl_filter.h"
#include "ui_assets.h"
#include "ui_draw.h"
#include "ui_page_nav.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* The settings whose folder the files are in, read at each use: Portable mode moves it. */
static const oracles_settings *files_of;

void oracles_filter_preview_files(const oracles_settings *settings) { files_of = settings; }

static void paths(char picture[ORACLES_SETTINGS_PATH_LENGTH], char title[ORACLES_SETTINGS_PATH_LENGTH])
{
    picture[0] = title[0] = 0;
    if (!files_of) return;
    oracles_settings_last_picture(files_of, 0, picture, ORACLES_SETTINGS_PATH_LENGTH);
    oracles_settings_last_picture(files_of, 1, title, ORACLES_SETTINGS_PATH_LENGTH);
}

void oracles_filter_preview_keep(const char *title, const uint32_t *pixels, uint32_t w, uint32_t h, size_t pitch)
{
    char picture_path[ORACLES_SETTINGS_PATH_LENGTH], title_path[ORACLES_SETTINGS_PATH_LENGTH];
    paths(picture_path, title_path);
    if (!picture_path[0] || !pixels || !w || !h || w > 4096 || h > 4096) return;
    /* Each written beside, then put in place, the name last: an interrupted write leaves the pair it found. */
    char picture_new[ORACLES_SETTINGS_PATH_LENGTH + 4], title_new[ORACLES_SETTINGS_PATH_LENGTH + 4];
    snprintf(picture_new, sizeof picture_new, "%s.new", picture_path);
    snprintf(title_new, sizeof title_new, "%s.new", title_path);
    SDL_Surface *surface = SDL_CreateSurfaceFrom((int)w, (int)h, SDL_PIXELFORMAT_XRGB8888, (void *)pixels, (int)pitch);
    int kept = surface && SDL_SavePNG(surface, picture_new);
    SDL_DestroySurface(surface);
    FILE *f = kept ? fopen(title_new, "w") : NULL;
    kept = f && fprintf(f, "%s\n", title ? title : "") > 0;
    if (f && fclose(f) != 0) kept = 0;
    /* The old name gone before the new picture comes: never a picture under another game's name. */
    kept = kept && (SDL_RemovePath(title_path) || !SDL_GetPathInfo(title_path, NULL)) && SDL_RenamePath(picture_new, picture_path)
           && SDL_RenamePath(title_new, title_path);
    if (!kept) {
        fprintf(stderr, "oracles: the last picture not kept in %s: %s\n", picture_path, SDL_GetError());
        SDL_RemovePath(picture_new);
        SDL_RemovePath(title_new);
        SDL_RemovePath(title_path);   /* no name over a picture of another game */
    }
}

void oracles_filter_preview_title(char *out, size_t capacity)
{
    char picture_path[ORACLES_SETTINGS_PATH_LENGTH], title_path[ORACLES_SETTINGS_PATH_LENGTH];
    paths(picture_path, title_path);
    if (capacity) out[0] = 0;
    FILE *f = title_path[0] ? fopen(title_path, "r") : NULL;
    if (!f) return;
    if (fgets(out, (int)capacity, f)) out[strcspn(out, "\r\n")] = 0;
    fclose(f);
    SDL_Surface *picture = out[0] ? SDL_LoadPNG(picture_path) : NULL;   /* a name without a picture that loads is none */
    if (picture) SDL_DestroySurface(picture);
    else if (capacity) out[0] = 0;
}

SDL_Renderer *oracles_filter_apply(SDL_Renderer *renderer, int filter)
{
    const char *name = oracles_settings_filter_names[filter > 0 && filter < ORACLES_DISPLAY_FILTERS ? filter : 0];
    if (!strcmp(name, oracles_sdl_filter_name())) return renderer;
    return oracles_sdl_renderer_swap(renderer, name);
}

void oracles_filter_preview_forget(OraclesFilterPreview *preview)
{
    if (preview->picture) SDL_DestroyTexture(preview->picture);
    preview->picture = NULL;
    preview->renderer = NULL;
}

void oracles_filter_preview_reload(OraclesFilterPreview *preview) { oracles_filter_preview_forget(preview); }

/* The last picture, else the sample one, a texture of `renderer`. */
static void load(OraclesFilterPreview *preview, SDL_Renderer *renderer)
{
    char picture_path[ORACLES_SETTINGS_PATH_LENGTH], title_path[ORACLES_SETTINGS_PATH_LENGTH];
    paths(picture_path, title_path);
    SDL_Surface *surface = NULL;
    char title[8];
    oracles_filter_preview_title(title, sizeof title);
    if (title[0]) surface = SDL_LoadPNG(picture_path);
    if (!surface) {
        const OraclesUiAsset *sample = oracles_ui_asset("sample-picture.png");
        if (sample) surface = SDL_LoadPNG_IO(SDL_IOFromConstMem(sample->data, sample->size), true);
    }
    /* Opaque ARGB, as the game's frame: a shader that passes the texture's alpha on would leave a picture without one
     * transparent. */
    SDL_Surface *argb = surface ? SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ARGB8888) : NULL;
    SDL_DestroySurface(surface);
    surface = argb;
    preview->renderer = renderer;
    preview->picture = surface ? SDL_CreateTextureFromSurface(renderer, surface) : NULL;
    preview->picture_w = surface ? surface->w : 0;
    preview->picture_h = surface ? surface->h : 0;
    SDL_DestroySurface(surface);
}

void oracles_filter_preview_draw(void *opaque, SDL_Renderer *renderer, const SDL_FRect *to)
{
    OraclesFilterPreview *preview = opaque;
    if (preview->renderer != renderer) {
        oracles_filter_preview_forget(preview);
        load(preview, renderer);
    }
    if (!preview->picture) return;
    SDL_FRect at;
    oracles_ui_draw_inset_place(to, preview->picture_w, preview->picture_h, &at);
    const SDL_Rect cut = { (int)to->x, (int)to->y, (int)to->w, (int)to->h };
    const bool clipped = SDL_RenderClipEnabled(renderer);
    SDL_Rect before = { 0, 0, 0, 0 };
    if (clipped) SDL_GetRenderClipRect(renderer, &before);
    SDL_SetRenderClipRect(renderer, &cut);
    SDL_SetTextureScaleMode(preview->picture, preview->drawn == ORACLES_DISPLAY_FILTER_SMOOTH ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
    OraclesSdlFilter *filter = oracles_display_filter_shader(preview->drawn) ? oracles_sdl_filter_open(renderer) : NULL;
    oracles_sdl_filter_place(filter, &at, (float)preview->picture_w, (float)preview->picture_h);
    oracles_sdl_filter_use(filter, 1);
    SDL_RenderTexture(renderer, preview->picture, NULL, &at);
    oracles_sdl_filter_use(filter, 0);
    oracles_sdl_filter_close(filter);
    SDL_SetRenderClipRect(renderer, clipped ? &before : NULL);
}
