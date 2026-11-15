/* SDL's GPU renderer (Vulkan, Direct3D 12, Metal), which a shader filter takes (Display › Picture's Filter, filter= in
 * settings.txt; ORACLES_RENDERER=gpu takes it without one), and the filters it draws the game's picture through: a
 * fragment shader set around the picture's own draws, never the text box Text size enlarges, the menus or the toasts. */
#ifndef ORACLES_SDL_FILTER_H
#define ORACLES_SDL_FILTER_H

#include <SDL3/SDL.h>

#include <stddef.h>

typedef struct OraclesSdlFilter OraclesSdlFilter;

/* 1 when ORACLES_RENDERER asks for the GPU renderer, or a filter is chosen. */
int oracles_sdl_gpu_wanted(void);

/* The filter chosen ("off" for none): settings.txt's filter= (oracles_settings_filter_names), set at the start without
 * making the renderer, then oracles_sdl_renderer_swap's. */
const char *oracles_sdl_filter_name(void);
/* The same as settings.txt's index (oracles_settings_filter_names): the filter in force, which a swap that made no
 * renderer leaves as it was. */
int oracles_sdl_filter_index(void);
void oracles_sdl_filter_start(const char *name);
/* The filter changes since the start (oracles_sdl_renderer_swap), or those that made the renderer again (`renderers`). */
unsigned oracles_sdl_filter_changes(int renderers);

/* `filter` chosen ("off" or an unknown name for none): when it needs another renderer (the GPU renderer for a filter,
 * the usual one without), every listener hears NULL and releases what it made on `renderer`, which is destroyed, and the
 * renderer made for the same window, with the same vsync, is told to them; else they hear `renderer` again, to take the
 * new filter.  When the renderer in use cannot draw it (the usual renderer for a shader filter, the GPU renderer refused),
 * the filter before is chosen again; when none can be made for it, the filter before is chosen again and its renderer
 * made.  Returns the renderer now in use, NULL when none could be made at all. */
SDL_Renderer *oracles_sdl_renderer_swap(SDL_Renderer *renderer, const char *filter);
/* After a swap that took the filter asked for back: 1 when the filter in force before is kept, 0 when the renderer
 * cannot draw that one either and none is chosen. */
int oracles_sdl_filter_kept(void);
void oracles_sdl_renderer_listen(void (*heard)(void *opaque, SDL_Renderer *renderer), void *opaque);
void oracles_sdl_renderer_unlisten(void *opaque);

/* The GPU renderer for `window`, or an offscreen one for NULL; NULL when it cannot be made, SDL_GetError saying why. */
SDL_Renderer *oracles_sdl_gpu_renderer(SDL_Window *window, int vsync);
/* SDL_RenderPresent, then, when it left the GPU renderer's window surface lost (a Vulkan surface Android took away, which
 * SDL does not make again), the renderer made again as a swap does, its users hearing NULL then the new one, with the
 * same filter and vsync; the usual renderer instead once three made again in a row lose it still.  The usual renderer
 * made instead because the app went to the background meanwhile, the GPU one is made again at a later present.  On
 * Android, the GPU renderer's swapchain is made again first after the window's size changed.  Returns the renderer now
 * in use, NULL when none could be made (SDL_GetError saying why). */
SDL_Renderer *oracles_sdl_renderer_present(SDL_Renderer *renderer);
/* What oracles_sdl_renderer_present presents with, SDL_RenderPresent for NULL (a test's present that loses the surface). */
void oracles_sdl_renderer_present_with(bool (SDLCALL *present)(SDL_Renderer *renderer));
/* 1 when SDL's error says the window's Vulkan surface is lost. */
int oracles_sdl_surface_lost(const char *error);
/* The renderers made again for a lost surface since the start. */
unsigned oracles_sdl_renderer_recoveries(void);
/* 1 while `window` has no surface to draw to because the app is in the background (Android): a GPU renderer that could
 * not be made then says nothing of the device. */
int oracles_sdl_window_away(SDL_Window *window);

/* Whether the shader filters can run here: a GPU device was made, or SDL says one can be (asked once, without making it);
 * 0 too once one could not be made. */
int oracles_sdl_filters_runnable(void);

/* The GPU renderer could not be made where the device could: the shader filters are off for the run.  The filters a
 * device refused (its shader or its render state), each refused for the run: a mask of settings.txt's indices. */
void oracles_sdl_filters_refused(void);
unsigned oracles_sdl_filters_refused_codes(void);

/* The GPU device the GPU renderers share, released at the end, after the last of them; whether the filters run and the
 * lost surfaces counted go with it, asked again by a window made after. */
void oracles_sdl_gpu_device_release(void);

/* ORACLES_SDL_LOG: SDL's own log on the standard error. */
void oracles_sdl_log_start(void);

/* The renderer's name, with the GPU device's driver and present mode for the GPU renderer: "gpu/vulkan, present
 * mailbox". */
void oracles_sdl_renderer_describe(SDL_Renderer *renderer, char *out, size_t capacity);
/* That name on stderr, with the GPU device's name and driver: `what` names the renderer's user. */
void oracles_sdl_renderer_report(SDL_Renderer *renderer, const char *what);

/* The filter chosen, for `renderer`; NULL for none, Smooth, a renderer without shaders, or a filter the device refused
 * (oracles_sdl_filters_refused_codes), which is said once. */
OraclesSdlFilter *oracles_sdl_filter_open(SDL_Renderer *renderer);
void oracles_sdl_filter_close(OraclesSdlFilter *filter);
/* The filters made for `renderer` released, before it is destroyed. */
void oracles_sdl_filter_forget(SDL_Renderer *renderer);

/* 1 when Smooth is chosen: no shader, the frame sampled bilinearly by the usual renderer (oracles_sdl_presentation). */
int oracles_sdl_filter_smooth(void);

/* The picture's place in the window's pixels, which the CRT filter bends, drawn from source_w x source_h pixels: under the
 * filter's scale (oracles_display_filter_min_scale, the smaller of the two) the picture is drawn without it.  The next
 * draws take it. */
void oracles_sdl_filter_place(OraclesSdlFilter *filter, const SDL_FRect *image, float source_w, float source_h);
/* The shader draws the picture placed last: a filter, and the picture large enough for it. */
int oracles_sdl_filter_active(const OraclesSdlFilter *filter);

/* How Link's layer under the sub-pixel camera goes through a filter: drawn alone (no shader, or Smooth), or apart at
 * his own place over the ground drawn again under him, premultiplied for a shader that blends the texels it reads, or
 * over that ground for one that keeps the colour to the coverage of the texel it reads it at. */
enum { ORACLES_SDL_LAYER_ALONE, ORACLES_SDL_LAYER_PREMULTIPLIED, ORACLES_SDL_LAYER_OVER_GROUND };
/* The way of the filter in force: ORACLES_SDL_LAYER_ALONE where no shader draws (oracles_sdl_filter_active 0). */
int oracles_sdl_filter_layer(const OraclesSdlFilter *filter);
/* The way of the filter chosen whether the picture placed last is large enough for it or not, as a picture drawn later
 * may be (the pause's): ORACLES_SDL_LAYER_ALONE without a shader. */
int oracles_sdl_filter_chosen_layer(const OraclesSdlFilter *filter);
/* The most texels the filter moves where it reads a picture of width x height pixels by (CRT's curve), 0 for the
 * others; whether the picture placed last is large enough for it or not, as the pause may draw it larger. */
int oracles_sdl_filter_bend(const OraclesSdlFilter *filter, int width, int height);

/* The draws that follow go through the filter's state for a part drawn apart (`part` 0 the ground under Link, 1 Link),
 * whose texels lie x, y texels from the picture's under them, where the picture placed last is large enough for it;
 * `part` -1 for the picture's own state again.  Scanlines' rows line up with the picture's; CRT's and LCD's cells, each
 * worked out from the colour of the texel it holds, stay the part's own, only their alternation following the
 * picture's (the whole texels of the move).  Each part has a state of its own, its constants changed without making
 * SDL draw first what the frame holds. */
void oracles_sdl_filter_part(OraclesSdlFilter *filter, int part, float x, float y);

/* The draws that follow go through the filter's state for a part drawn apart (`part` 0 or 1, as above) at another scale
 * than the picture's: Scanlines' rows, CRT's mask and LCD's grid on the picture's own pixels where the fragment stands
 * (the picture placed last, its scale), so that they run on from the picture into the part, its texels only read; the
 * others (xBR, HQ2x, OmniScale) on the part's own texels.  `part` -1 (oracles_sdl_filter_part) for the picture's state. */
void oracles_sdl_filter_part_cells(OraclesSdlFilter *filter, int part);

/* SDL's error around the first present through the filter: cleared before it (`presented` 0), written after it. */
void oracles_sdl_filter_check(OraclesSdlFilter *filter, int presented);

/* The draws that follow go through the filter (`on`) where the picture placed last is large enough for it, or no longer;
 * nothing without a filter. */
void oracles_sdl_filter_use(OraclesSdlFilter *filter, int on);

#endif
