/* Display › Picture's Filter on SDL's side: the filter chosen applied to the renderer, the preview while the row has the
 * focus (the last picture of the last game played, kept on quitting a game, or the sample picture before any game; from
 * the pause, the game's own picture), and the files of the last picture, beside settings.txt. */
#ifndef ORACLES_FILTER_PREVIEW_H
#define ORACLES_FILTER_PREVIEW_H

#include "settings.h"

#include <SDL3/SDL.h>

#include <stdint.h>
#include <stddef.h>

/* The last picture's files beside settings.txt, last-picture.png and last-picture.txt (the game's name), in the folder
 * `settings` has at each use (Portable mode moves it); none without a settings directory.  Each is written beside first,
 * then put in place, the name last; a write that fails leaves no name, so no caption over another picture. */
void oracles_filter_preview_files(const oracles_settings *settings);
/* The last picture of a game quitting, its name `title`: w x h pixels 0xAARRGGBB, `pitch` bytes a line. */
void oracles_filter_preview_keep(const char *title, const uint32_t *pixels, uint32_t w, uint32_t h, size_t pitch);
/* The name of the game the last picture is of, into `out`; empty without one, or without a picture that loads. */
void oracles_filter_preview_title(char *out, size_t capacity);

/* `renderer` made again for `filter` (an index of oracles_display_filter_names) when it is not the one chosen; the
 * renderer in use, NULL when none could be made. */
SDL_Renderer *oracles_filter_apply(SDL_Renderer *renderer, int filter);

/* The home screen's preview: its picture (the last one, read again by _reload, else the sample) on the renderer it is
 * given, through `drawn` (oracles_display_filter_drawn), its OraclesUiInset. */
typedef struct OraclesFilterPreview {
    SDL_Renderer *renderer;   /* the renderer its texture was made on */
    SDL_Texture *picture;
    int picture_w, picture_h;
    int drawn;
} OraclesFilterPreview;
void oracles_filter_preview_reload(OraclesFilterPreview *preview);
void oracles_filter_preview_draw(void *opaque, SDL_Renderer *renderer, const SDL_FRect *to);
/* The renderer goes (NULL) or another comes: its texture released. */
void oracles_filter_preview_forget(OraclesFilterPreview *preview);

#endif
