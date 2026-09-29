/* Every text style of the launcher, the ones its sources define, prepared at
 * one scale by one drawing, as the home screen and the pause menu share it
 * when the game's window has the home screen's scale: all fit in the
 * drawing's atlases, and none of their text would be left undrawn.  The list
 * is generated from the sources at configure (launcher_styles.h). */
#include "launcher_styles.h"

#include <SDL3/SDL.h>
#include <stdio.h>

int main(void)
{
    static const OraclesUiTextStyle *const all[] = { ORACLES_LAUNCHER_STYLES };
    const int count = (int)(sizeof all / sizeof all[0]);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    SDL_Window *window = SDL_CreateWindow("atlases", 1920, 1080, 0);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, SDL_SOFTWARE_RENDERER) : NULL;
    OraclesUiDraw *draw = renderer ? oracles_ui_draw_create(renderer) : NULL;
    if (!draw) { fprintf(stderr, "no drawing: %s\n", SDL_GetError()); return 1; }
    int failures = 0;
    static const int sizes[3][2] = { { 1920, 1080 }, { 960, 540 }, { 2880, 1800 } };
    for (int s = 0; s < 3; s++) {
        const int glyphs = oracles_ui_draw_prepare(draw, sizes[s][0], sizes[s][1], 0.0f, all, count, "Ag");
        if (glyphs < 0) { fprintf(stderr, "FAIL: %d styles do not fit the atlases at %dx%d\n", count, sizes[s][0], sizes[s][1]); failures++; }
    }
    oracles_ui_draw_destroy(draw);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (failures) return 1;
    printf("test_launcher_atlases: ok (%d styles)\n", count);
    return 0;
}
