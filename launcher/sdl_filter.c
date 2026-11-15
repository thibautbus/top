#include "sdl_filter.h"

#include "backends.h"
#include "ui_page_nav.h"

#include "shaders/crt.frag.dxil.h"
#include "shaders/crt.frag.msl.h"
#include "shaders/crt.frag.spv.h"
#include "shaders/hq2x.frag.dxil.h"
#include "shaders/hq2x.frag.msl.h"
#include "shaders/hq2x.frag.spv.h"
#include "shaders/lcd.frag.dxil.h"
#include "shaders/lcd.frag.msl.h"
#include "shaders/lcd.frag.spv.h"
#include "shaders/omniscale.frag.dxil.h"
#include "shaders/omniscale.frag.msl.h"
#include "shaders/omniscale.frag.spv.h"
#include "shaders/scanlines.frag.dxil.h"
#include "shaders/scanlines.frag.msl.h"
#include "shaders/scanlines.frag.spv.h"
#include "shaders/xbr.frag.dxil.h"
#include "shaders/xbr.frag.msl.h"
#include "shaders/xbr.frag.spv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include <EGL/egl.h>
#include <vulkan/vulkan.h>

#include <SDL3/SDL_vulkan.h>
#endif

typedef struct FilterCode {
    const char *name;
    const unsigned char *spirv, *dxil, *msl;
    size_t spirv_size, dxil_size, msl_size;
    Uint32 uniform_buffers;
    int layer;   /* oracles_sdl_filter_layer's */
    int grid;    /* its shader reads `grid` (oracles_sdl_filter_part): 1 its rows, 2 its cells, which hold a texel's colour each */
    int bends;   /* it reads where its curve moves it (oracles_sdl_filter_bend) */
} FilterCode;

/* The choices after off, in the order the cycle takes them.  Smooth is no shader: the usual renderer's bilinear
 * sampling, which every renderer has. */
static const FilterCode filters[] = {
    { "scanlines", scanlines_frag_spv, scanlines_frag_dxil, scanlines_frag_msl, sizeof scanlines_frag_spv, sizeof scanlines_frag_dxil, sizeof scanlines_frag_msl, 1, ORACLES_SDL_LAYER_PREMULTIPLIED, 1, 0 },
    { "crt", crt_frag_spv, crt_frag_dxil, crt_frag_msl, sizeof crt_frag_spv, sizeof crt_frag_dxil, sizeof crt_frag_msl, 1, ORACLES_SDL_LAYER_OVER_GROUND, 2, 1 },
    { "lcd", lcd_frag_spv, lcd_frag_dxil, lcd_frag_msl, sizeof lcd_frag_spv, sizeof lcd_frag_dxil, sizeof lcd_frag_msl, 1, ORACLES_SDL_LAYER_OVER_GROUND, 2, 0 },
    { "smooth", NULL, NULL, NULL, 0, 0, 0, 0, ORACLES_SDL_LAYER_ALONE, 0, 0 },
    { "xbr", xbr_frag_spv, xbr_frag_dxil, xbr_frag_msl, sizeof xbr_frag_spv, sizeof xbr_frag_dxil, sizeof xbr_frag_msl, 0, ORACLES_SDL_LAYER_OVER_GROUND, 0, 0 },
    { "hq2x", hq2x_frag_spv, hq2x_frag_dxil, hq2x_frag_msl, sizeof hq2x_frag_spv, sizeof hq2x_frag_dxil, sizeof hq2x_frag_msl, 1, ORACLES_SDL_LAYER_OVER_GROUND, 0, 0 },
    { "omniscale", omniscale_frag_spv, omniscale_frag_dxil, omniscale_frag_msl, sizeof omniscale_frag_spv, sizeof omniscale_frag_dxil, sizeof omniscale_frag_msl, 1, ORACLES_SDL_LAYER_OVER_GROUND, 0, 0 },
};

/* Kept until the renderer goes (oracles_sdl_filter_forget): SDL's GPU renderer caches its pipelines by the shader's
 * address, which a shader made again could take with other code. */
struct OraclesSdlFilter {
    SDL_Renderer *renderer;
    SDL_GPUDevice *device;
    const FilterCode *code;
    /* The shaders' common constants: strength, thickness, the grid's move, the picture's place, and the screen pixels of
     * one of its pixels with whether the masks stand on them (a part drawn at another scale). */
    float uniforms[12];
    float part_uniforms[2][12];   /* the same for the ground and Link drawn apart, their grids their own */
    float cells[2];               /* the screen pixels of one of the picture's pixels, as placed last */
    int checked;         /* oracles_sdl_filter_check: 1 before the first present, 2 after */
    int fits;            /* the picture placed last is large enough for it (oracles_display_filter_min_scale) */
    SDL_GPUShader *shader;
    SDL_GPURenderState *state;
    /* The ground and Link drawn apart (oracles_sdl_filter_part), each with its own constants, for a shader that reads
     * the grid: changing those of a state a draw of this frame holds already would have SDL draw what it holds first,
     * another pass over the window for a tiled GPU. */
    SDL_GPURenderState *parts[2];
    OraclesSdlFilter *next;
};

static OraclesSdlFilter *kept;

/* The filter chosen: settings.txt's at the start, then oracles_sdl_renderer_swap's; NULL for none. */
static const FilterCode *chosen;

static const FilterCode *filter_named(const char *name)
{
    for (size_t i = 0; name && i < sizeof filters / sizeof filters[0]; i++) if (!strcmp(name, filters[i].name)) return &filters[i];
    return NULL;
}

static const FilterCode *filter_chosen(void) { return chosen; }

void oracles_sdl_filter_start(const char *name) { chosen = filter_named(name); }

int oracles_sdl_filter_index(void)
{
    const FilterCode *code = filter_chosen();
    return code ? (int)(code - filters) + 1 : 0;
}

const char *oracles_sdl_filter_name(void)
{
    const FilterCode *code = filter_chosen();
    return code ? code->name : "off";
}

static int runnable = -1;   /* oracles_sdl_filters_runnable's */
static int lost_in_a_row;   /* oracles_sdl_renderer_present's: renderers made again for a lost surface since a present that lost none */
static int gpu_again;       /* and the GPU renderer to try again, the usual one made instead while the app went away */
#ifdef __ANDROID__
static int follow_refused;  /* swapchain_follow's: swapchains refused in a row before any went, the renderer's */
static int follow_broken;   /* and a swapchain gone with a failure, the window left without one */
#endif

/* A shader filter chosen, where one can run (a device that could not be made is not asked for again). */
static int shader_chosen(void)
{
    const FilterCode *code = filter_chosen();
    return code && code->spirv && runnable != 0;
}

int oracles_sdl_gpu_wanted(void)
{
    const char *wanted = SDL_getenv("ORACLES_RENDERER");
    return (wanted && !strcmp(wanted, "gpu")) || shader_chosen();
}

int oracles_sdl_filter_smooth(void)
{
    const FilterCode *code = filter_chosen();
    return code && !code->spirv;
}

static int is_gpu(SDL_Renderer *renderer)
{
    const char *name = renderer ? SDL_GetRendererName(renderer) : NULL;
    return name && !strcmp(name, SDL_GPU_RENDERER);
}

typedef struct Listener {
    void (*heard)(void *opaque, SDL_Renderer *renderer);
    void *opaque;
} Listener;

#define LISTENERS 4
static Listener listeners[LISTENERS];

void oracles_sdl_renderer_listen(void (*heard)(void *opaque, SDL_Renderer *renderer), void *opaque)
{
    for (int i = 0; i < LISTENERS; i++) if (!listeners[i].heard) { listeners[i] = (Listener){ heard, opaque }; return; }
}

void oracles_sdl_renderer_unlisten(void *opaque)
{
    for (int i = 0; i < LISTENERS; i++) if (listeners[i].opaque == opaque) listeners[i] = (Listener){ NULL, NULL };
}

static unsigned changes, remade;

unsigned oracles_sdl_filter_changes(int renderers) { return renderers ? remade : changes; }

static void tell_renderer(SDL_Renderer *renderer)
{
    for (int i = 0; i < LISTENERS; i++) if (listeners[i].heard) listeners[i].heard(listeners[i].opaque, renderer);
}

/* Whether `renderer` draws `code`: a shader filter needs the GPU renderer. */
static int drawable(const FilterCode *code, SDL_Renderer *renderer) { return !code || !code->spirv || is_gpu(renderer); }

static int taken_back_kept;   /* oracles_sdl_filter_kept's */

int oracles_sdl_filter_kept(void) { return taken_back_kept; }

/* The filter before chosen again, when `renderer` draws it; else none (a shader filter kept from the start where the GPU
 * renderer was refused, which it cannot draw either). */
static void take_back(const FilterCode *before, SDL_Renderer *renderer)
{
    const char *asked = oracles_sdl_filter_name();
    taken_back_kept = drawable(before, renderer);
    chosen = taken_back_kept ? before : NULL;
    fprintf(stderr, "oracles: the filter %s cannot be drawn by the renderer %s: the filter %s %s\n", asked, renderer ? SDL_GetRendererName(renderer) : "none",
            oracles_sdl_filter_name(), taken_back_kept ? "again" : "instead");
}

/* `renderer` destroyed, every texture made on it released by its users, who hear NULL, and the one
 * oracles_sdl_create_renderer makes for its window, with its vsync (its report then says the present mode it has); its
 * window and vsync said, for another try. */
static SDL_Renderer *make_again(SDL_Renderer *renderer, SDL_Window **window, int *vsync)
{
    *window = SDL_GetRenderWindow(renderer);
    *vsync = 0;
    SDL_GetRenderVSync(renderer, vsync);
    tell_renderer(NULL);
    oracles_sdl_filter_forget(renderer);
    SDL_DestroyRenderer(renderer);
#ifdef __ANDROID__
    follow_refused = follow_broken = 0;
#endif
    return oracles_sdl_create_renderer(*window, *vsync != 0);
}

SDL_Renderer *oracles_sdl_renderer_swap(SDL_Renderer *renderer, const char *filter)
{
    const uint64_t started = SDL_GetTicksNS();
    const FilterCode *before = filter_chosen();
    chosen = filter_named(filter);
    taken_back_kept = 1;
    const int gpu_now = is_gpu(renderer), gpu_wanted = oracles_sdl_gpu_wanted();
    changes++;
    if (gpu_now == gpu_wanted) {
        /* The same renderer: only its users' filter changes, unless it cannot draw it (a shader filter where the GPU
         * renderer could not be made). */
        if (!drawable(chosen, renderer)) take_back(before, renderer);
        tell_renderer(renderer);
        fprintf(stderr, "oracles: filter %s -> %s on the same renderer, %.2f ms\n", before ? before->name : "off", oracles_sdl_filter_name(),
                (double)(SDL_GetTicksNS() - started) / 1e6);
        return renderer;
    }
    SDL_Window *window;
    int vsync;
    renderer = make_again(renderer, &window, &vsync);
    if (renderer && !drawable(chosen, renderer)) {
        /* The usual renderer instead of the GPU one, which could not be made: it draws the filter before, as a moment
         * ago, the choice taken back. */
        take_back(before, renderer);
    } else if (!renderer && before != chosen) {
        /* None for this filter: the renderer of the one before, which held a moment ago, the choice taken back. */
        fprintf(stderr, "oracles: no renderer for the filter %s (%s): the filter %s again\n", oracles_sdl_filter_name(), SDL_GetError(),
                before ? before->name : "off");
        chosen = before;
        renderer = oracles_sdl_create_renderer(window, vsync != 0);
    }
    if (renderer && vsync) SDL_SetRenderVSync(renderer, vsync);
    if (renderer) tell_renderer(renderer);   /* made again on the new one */
    remade++;
    fprintf(stderr, "oracles: filter %s -> %s, renderer made again, %.2f ms\n", before ? before->name : "off", oracles_sdl_filter_name(),
            (double)(SDL_GetTicksNS() - started) / 1e6);
    return renderer;
}

/* The GPU device, made once and kept until the end (oracles_sdl_gpu_device_release): making the GPU renderer again then
 * takes no new device, the larger part of the wait.  Windows asks Vulkan only, unless ORACLES_GPU_DRIVER names another
 * (direct3d12): no Vulkan, no filter, the usual renderer staying.  The device takes the settings SDL's GPU renderer
 * gives one it makes itself, for the most hardware. */
static SDL_GPUDevice *kept_device;


/* The device's settings, which the device is made with and the probe asks about; `driver` the one asked for, if any. */
static SDL_PropertiesID device_properties(const char **driver)
{
    const SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN, true);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN, true);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, SDL_GetHintBoolean(SDL_HINT_RENDER_GPU_DEBUG, false));
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_D3D12_ALLOW_FEWER_RESOURCE_SLOTS_BOOLEAN, true);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_CLIP_DISTANCE_BOOLEAN, false);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_DEPTH_CLAMPING_BOOLEAN, false);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_INDIRECT_DRAW_FIRST_INSTANCE_BOOLEAN, false);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_ANISOTROPY_BOOLEAN, false);
    SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_METAL_ALLOW_MACFAMILY1_BOOLEAN, false);
    *driver = getenv("ORACLES_GPU_DRIVER");
#ifdef _WIN32
    if (!*driver || !**driver) *driver = "vulkan";
#endif
    if (*driver && **driver) SDL_SetStringProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, *driver);
    else *driver = NULL;
    return properties;
}

static SDL_GPUDevice *gpu_device(void)
{
    if (kept_device) return kept_device;
    const char *driver;
    const SDL_PropertiesID properties = device_properties(&driver);
    const uint64_t started = SDL_GetTicksNS();
    kept_device = SDL_CreateGPUDeviceWithProperties(properties);
    SDL_DestroyProperties(properties);
    runnable = kept_device != NULL;
    if (kept_device) fprintf(stderr, "oracles: GPU device %s made in %.1f ms, kept\n", SDL_GetGPUDeviceDriver(kept_device), (double)(SDL_GetTicksNS() - started) / 1e6);
    else fprintf(stderr, "oracles: no GPU device%s%s (%s): the filters that need it are off\n", driver ? " for " : "", driver ? driver : "", SDL_GetError());
    return kept_device;
}

/* -1 until known; 0 once no GPU device could be made, or SDL says none can. */
void oracles_sdl_filters_refused(void)
{
    if (runnable != 0) fprintf(stderr, "oracles: the GPU renderer cannot be made here: the filters that need it are off\n");
    runnable = 0;
}

static unsigned refused_codes;   /* oracles_sdl_filters_refused_codes' */

unsigned oracles_sdl_filters_refused_codes(void) { return refused_codes; }

int oracles_sdl_filters_runnable(void)
{
    if (runnable == 0) return 0;   /* refused, the device made or not */
    if (kept_device) return 1;
    if (runnable < 0) {
        /* Asked of SDL without making the device, which the first filter chosen makes. */
        const char *driver;
        const SDL_PropertiesID properties = device_properties(&driver);
        const uint64_t started = SDL_GetTicksNS();
        runnable = SDL_GPUSupportsProperties(properties);
        SDL_DestroyProperties(properties);
        fprintf(stderr, "oracles: the GPU filters %s here%s%s, asked in %.1f ms\n", runnable ? "can run" : "cannot run", driver ? ", on " : "",
                driver ? driver : "", (double)(SDL_GetTicksNS() - started) / 1e6);
    }
    return runnable;
}

void oracles_sdl_gpu_device_release(void)
{
    if (kept_device) SDL_DestroyGPUDevice(kept_device);
    kept_device = NULL;
    /* What the device's renderers learned goes with it: a window made after asks again. */
    runnable = -1;
    lost_in_a_row = gpu_again = 0;
}

int oracles_sdl_window_away(SDL_Window *window)
{
#ifdef __ANDROID__
    /* SDL lets the activity's native window go when the app goes to the background (onNativeSurfaceDestroyed) and takes
     * the new one when it comes back, on the activity's thread, under a lock of its own: a GPU renderer made meanwhile
     * finds none ("native window is not available"), which says nothing of the device.  The window's property follows
     * it; SDL's own events for the background come later, from the event loop. */
    return window && !SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, NULL);
#else
    (void)window;
    return 0;
#endif
}

#ifdef __ANDROID__
/* Android has one window, which SDL gives an EGL surface when the usual renderer makes it again for OpenGL ES, and
 * which keeps that surface, and its SDL_WINDOW_OPENGL, when that renderer goes.  SDL's GPU renderer makes its Vulkan
 * surface without turning the window to Vulkan: Vulkan cannot take the surface while EGL holds it
 * (VK_ERROR_NATIVE_WINDOW_IN_USE_KHR), SDL makes the EGL surface again when the app comes back from the background
 * (its Vulkan surface then cannot be made again), and the usual renderer, made again later, takes the window as its
 * own still.  Before the GPU renderer is made, the EGL surface is destroyed and the window turned to Vulkan by
 * SDL_Vulkan_CreateSurface, for a moment, on an instance of its own; 1 when the window is Vulkan's, -1 when the EGL
 * surface went but the window still says OpenGL ES, 0 when it was not OpenGL ES's.
 * SDL keeps the destroyed surface's handle in its window data: it is never used again only because turning the window
 * unloads EGL (SDL_GL_UnloadLibrary, the launcher holding no other load), and SDL_EGL_DestroySurface does nothing
 * without it (SDL_egl.c:1378).  EGL still loaded after the turn is said in the log. */
static int window_for_vulkan(SDL_Window *window)
{
    if (!window || !(SDL_GetWindowFlags(window) & SDL_WINDOW_OPENGL)) return 0;
    const uint64_t started = SDL_GetTicksNS();
    const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    const EGLSurface egl_surface = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_ANDROID_SURFACE_POINTER, NULL);
    const EGLDisplay display = egl_surface ? SDL_EGL_GetCurrentDisplay() : EGL_NO_DISPLAY;
    const char *egl = "none to destroy";
    if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        const int destroyed = eglDestroySurface(display, egl_surface) == EGL_TRUE;
        if (destroyed) SDL_SetPointerProperty(properties, SDL_PROP_WINDOW_ANDROID_SURFACE_POINTER, NULL);
        egl = destroyed ? "destroyed" : "not destroyed";
    }
    const char *made = "no Vulkan instance";
    if (SDL_Vulkan_LoadLibrary(NULL)) {
        const PFN_vkGetInstanceProcAddr address = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
        const PFN_vkCreateInstance create = address ? (PFN_vkCreateInstance)address(VK_NULL_HANDLE, "vkCreateInstance") : NULL;
        Uint32 count = 0;
        const char *const *extensions = SDL_Vulkan_GetInstanceExtensions(&count);
        const VkApplicationInfo application = { VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "The Oracles Project", 1, NULL, 0, VK_API_VERSION_1_0 };
        const VkInstanceCreateInfo info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &application, 0, NULL, count, extensions };
        VkInstance instance = VK_NULL_HANDLE;
        if (create && extensions && create(&info, NULL, &instance) == VK_SUCCESS) {
            VkSurfaceKHR surface = VK_NULL_HANDLE;
            made = "Vulkan surface made";
            if (SDL_Vulkan_CreateSurface(window, instance, NULL, &surface)) SDL_Vulkan_DestroySurface(instance, surface, NULL);
            else {
                made = "Vulkan surface not made";
                fprintf(stderr, "oracles: the window's Vulkan surface not made: %s\n", SDL_GetError());
            }
            const PFN_vkDestroyInstance destroy = (PFN_vkDestroyInstance)address(instance, "vkDestroyInstance");
            if (destroy) destroy(instance, NULL);
        }
        SDL_Vulkan_UnloadLibrary();
    }
    const int vulkan = (SDL_GetWindowFlags(window) & SDL_WINDOW_VULKAN) != 0 && !(SDL_GetWindowFlags(window) & SDL_WINDOW_OPENGL);
    const int egl_loaded = SDL_EGL_GetCurrentDisplay() != EGL_NO_DISPLAY;
    if (!egl_loaded) SDL_ClearError();   /* "There is no current EGL display", the answer hoped for */
    fprintf(stderr, "oracles: the window for the GPU renderer: EGL surface %s, %s, window %s, EGL %s, %.1f ms\n", egl, made,
            vulkan ? "Vulkan's" : "still OpenGL ES's", egl_loaded ? "STILL LOADED (the old surface's handle may be destroyed again)" : "unloaded",
            (double)(SDL_GetTicksNS() - started) / 1e6);
    return vulkan ? 1 : -1;
}
#endif

SDL_Renderer *oracles_sdl_gpu_renderer(SDL_Window *window, int vsync)
{
    if (oracles_sdl_window_away(window)) {
        SDL_SetError("the app is in the background, its window without a surface");
        return NULL;
    }
    SDL_GPUDevice *device = gpu_device();
    if (!device) return NULL;
#ifdef __ANDROID__
    if (window_for_vulkan(window) < 0) {
        /* The window still says OpenGL ES without its surface: no GPU renderer on it, which would leave SDL making the EGL
         * surface again when the app comes back.  The usual renderer, asked for another context than its own, makes the
         * window again (SDL_render_gles2.c). */
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
        SDL_SetError("the window could not be turned to Vulkan");
        return NULL;
    }
#endif
    const SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetStringProperty(properties, SDL_PROP_RENDERER_CREATE_NAME_STRING, SDL_GPU_RENDERER);
    if (window) SDL_SetPointerProperty(properties, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
    SDL_SetPointerProperty(properties, SDL_PROP_RENDERER_CREATE_GPU_DEVICE_POINTER, device);
    SDL_SetNumberProperty(properties, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, vsync ? 1 : 0);
    SDL_Renderer *renderer = SDL_CreateRendererWithProperties(properties);
    SDL_DestroyProperties(properties);
    return renderer;
}

int oracles_sdl_surface_lost(const char *error) { return error && strstr(error, "VK_ERROR_SURFACE_LOST_KHR") != NULL; }

static unsigned recoveries;   /* oracles_sdl_renderer_recoveries' */
static bool (SDLCALL *render_present)(SDL_Renderer *renderer) = SDL_RenderPresent;   /* oracles_sdl_renderer_present_with's */

unsigned oracles_sdl_renderer_recoveries(void) { return recoveries; }

void oracles_sdl_renderer_present_with(bool (SDLCALL *present)(SDL_Renderer *renderer)) { render_present = present ? present : SDL_RenderPresent; }

/* The GPU renderer's present mode, as SDL's GPU renderer chooses it (SDL_render_gpu.c, ChoosePresentMode): FIFO under
 * vsync; without, MAILBOX, else IMMEDIATE, else FIFO all the same, which waits for the display. */
static SDL_GPUPresentMode gpu_present_mode(SDL_Renderer *renderer, SDL_GPUDevice *device, SDL_Window *window)
{
    int vsync = 0;
    SDL_GetRenderVSync(renderer, &vsync);
    if (vsync) return SDL_GPU_PRESENTMODE_VSYNC;
    if (SDL_WindowSupportsGPUPresentMode(device, window, SDL_GPU_PRESENTMODE_MAILBOX)) return SDL_GPU_PRESENTMODE_MAILBOX;
    if (SDL_WindowSupportsGPUPresentMode(device, window, SDL_GPU_PRESENTMODE_IMMEDIATE)) return SDL_GPU_PRESENTMODE_IMMEDIATE;
    return SDL_GPU_PRESENTMODE_VSYNC;
}

/* What a present does after swapchain_follow. */
enum { FOLLOW_SKIP, FOLLOW_PRESENT, FOLLOW_REMAKE };

#ifdef __ANDROID__
/* SDL's Vulkan backend takes the window's new pixel size in a window event watch, called on the thread that sends the
 * event, the activity's on Android, and asks there for a new swapchain; a swapchain being made meanwhile on the app's
 * thread, at a present, takes the size before and clears that request as it ends (SDL_gpu_vulkan.c,
 * VULKAN_INTERNAL_OnWindowResize and VULKAN_INTERNAL_CreateSwapchain).  The activity coming back through portrait (from
 * Android's home screen or its file picker) sends two sizes a moment apart, the second while the swapchain of the first
 * is made: the swapchain keeps the portrait size under the landscape window, SDL's GPU renderer drawing the landscape
 * picture into it, its corner stretched over the screen, until the app goes to the background again.  Each size SDL
 * sends is counted by an event watch, which SDL calls after its window event watches; a present that finds a new count
 * has the swapchain made again first, on the app's thread, with the parameters it has (SDL_SetGPUSwapchainParameters
 * makes it again even unchanged): the last size sent is the swapchain's.  The swapchain SDL would make at that present
 * for the new size is this one, its request cleared.
 * A swapchain refused before any went (the parameters "not supported", the old surface queried after Android took it
 * away: SDL, which saw it lost at an acquire, makes it again with the swapchain at the present) is presented as it is
 * and followed again at the next present; refused again, the renderer is made again, as for a lost surface
 * (oracles_sdl_renderer_present).  One that cannot be made even once more leaves the window without a swapchain, which
 * SDL's request, if cleared by the race, would not make again: the present is skipped and the renderer made again,
 * and while the window is away every present is skipped until a swapchain is made.  Returns FOLLOW_PRESENT,
 * FOLLOW_SKIP or FOLLOW_REMAKE (SDL's error saying why). */
static SDL_AtomicInt sizes_sent;
static int sizes_followed, sizes_watched;

static bool SDLCALL size_sent(void *opaque, SDL_Event *e)
{
    (void)opaque;
    if (e->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) SDL_AddAtomicInt(&sizes_sent, 1);
    return true;
}

static int swapchain_refused(const char *error)
{
    return strstr(error, "Swapchain composition not supported!") || strstr(error, "Present mode not supported!")
        || strstr(error, "Cannot set swapchain parameters on unclaimed window!");
}

static int swapchain_follow(SDL_Renderer *renderer)
{
    if (!sizes_watched) {
        sizes_watched = SDL_AddEventWatch(size_sent, NULL);
        sizes_followed = SDL_GetAtomicInt(&sizes_sent);
        return FOLLOW_PRESENT;
    }
    const int sent = SDL_GetAtomicInt(&sizes_sent);
    if (sent == sizes_followed) return FOLLOW_PRESENT;
    SDL_Window *window = SDL_GetRenderWindow(renderer);
    SDL_GPUDevice *device = is_gpu(renderer) ? SDL_GetGPURendererDevice(renderer) : NULL;
    if (!window || !device) {
        sizes_followed = sent;
        return FOLLOW_PRESENT;
    }
    /* Followed once the window has its surface back; without a swapchain meanwhile, nothing is presented. */
    if (oracles_sdl_window_away(window)) return follow_broken ? FOLLOW_SKIP : FOLLOW_PRESENT;
    const uint64_t started = SDL_GetTicksNS();
    const SDL_Colorspace colorspace = (SDL_Colorspace)SDL_GetNumberProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_OUTPUT_COLORSPACE_NUMBER, SDL_COLORSPACE_SRGB);
    const SDL_GPUSwapchainComposition composition = colorspace == SDL_COLORSPACE_SRGB_LINEAR ? SDL_GPU_SWAPCHAINCOMPOSITION_HDR_EXTENDED_LINEAR
        : colorspace == SDL_COLORSPACE_HDR10 ? SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2084 : SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    int made = SDL_SetGPUSwapchainParameters(device, window, composition, gpu_present_mode(renderer, device, window));
    const int gone = !made && strstr(SDL_GetError(), "vkCreateSwapchainKHR") != NULL;   /* the old swapchain with it */
    if (gone) {
        /* The swapchain it replaced went with the failure: once more without it, which a window that the old one still
         * held (VK_ERROR_NATIVE_WINDOW_IN_USE_KHR, the window's surface made again by Android) takes. */
        fprintf(stderr, "oracles: the GPU renderer's swapchain not made again (%s): once more\n", SDL_GetError());
        SDL_ClearError();
        made = SDL_SetGPUSwapchainParameters(device, window, composition, gpu_present_mode(renderer, device, window));
    }
    const int refused = !made && !gone && !follow_broken && swapchain_refused(SDL_GetError());
    fprintf(stderr, "oracles: the window's size changed (%dx%d pixels): the GPU renderer's swapchain %s%s%s, %.2f ms\n", w, h,
            made ? "made again" : "not made again: ", made ? "" : SDL_GetError(),
            refused && follow_refused == 0 ? ", presented as it is and followed again at the next present" : "",
            (double)(SDL_GetTicksNS() - started) / 1e6);
    if (made) {
        sizes_followed = sent;
        follow_refused = follow_broken = 0;
        return FOLLOW_PRESENT;
    }
    if (refused && follow_refused++ == 0) {
        SDL_ClearError();
        return FOLLOW_PRESENT;
    }
    if (!refused) follow_broken = 1;
    return FOLLOW_REMAKE;
}
#endif

/* SDL's GPU renderer keeps the Vulkan surface it made for the window until it goes.  Android destroys the window's
 * surface when the app goes to the background and makes another when it comes back: a present then finds the old one
 * lost, which SDL makes again when the swapchain's image is asked for (SDL_gpu_vulkan.c, VULKAN_INTERNAL_AcquireSwapchainTexture),
 * but not when the window changed size meanwhile (the activity turned to portrait for Android's file picker, and back):
 * the swapchain is made again first, on the lost surface, which fails at every present after, the window black for
 * good.  A present that leaves that error makes the renderer again, with the filter and vsync it had; after three made
 * again in a row that lose it still, the GPU renderer is given up for the run, as one that cannot be made
 * (oracles_sdl_filters_refused): the usual renderer, the shader filters greyed.  The app gone to the background
 * between the two, the usual renderer made instead says nothing of the device: the GPU one is tried again at the next
 * present the window has its surface back for.
 * SDL's error is read for this present alone, cleared before it, and the error before it set again when it leaves none
 * (oracles_sdl_filter_check reads what the draws before it left). */
SDL_Renderer *oracles_sdl_renderer_present(SDL_Renderer *renderer)
{
    if (!renderer) return NULL;
    char before[256];
    snprintf(before, sizeof before, "%s", SDL_GetError());
    SDL_ClearError();
#ifdef __ANDROID__
    const int follow = swapchain_follow(renderer);
#else
    const int follow = FOLLOW_PRESENT;
#endif
    if (follow == FOLLOW_PRESENT) render_present(renderer);
    const int followed = follow != FOLLOW_REMAKE;
    const int lost = is_gpu(renderer) && (!followed || oracles_sdl_surface_lost(SDL_GetError()));
    const int again = gpu_again && !is_gpu(renderer) && oracles_sdl_gpu_wanted();
    if (!lost && !again) {
        if (!SDL_GetError()[0] && before[0]) SDL_SetError("%s", before);
        lost_in_a_row = 0;
        gpu_again = gpu_again && oracles_sdl_gpu_wanted();
        return renderer;
    }
    SDL_Window *window = SDL_GetRenderWindow(renderer);
    if (!window || oracles_sdl_window_away(window)) return renderer;   /* made again once the window has its surface back */
    const uint64_t started = SDL_GetTicksNS();
    char why[200];
    if (!followed) snprintf(why, sizeof why, "the swapchain was not made again for the window's new size (%s)", SDL_GetError());
    else if (lost) snprintf(why, sizeof why, "the window's surface was lost (%s)", SDL_GetError());
    else snprintf(why, sizeof why, "the usual renderer was made while the app went away");
    if (lost && ++lost_in_a_row > 3) {
        fprintf(stderr, "oracles: the window's surface lost again after %d GPU renderers made again: the usual renderer instead\n", lost_in_a_row - 1);
        oracles_sdl_filters_refused();
    }
    int vsync;
    renderer = make_again(renderer, &window, &vsync);
    if (renderer && vsync) SDL_SetRenderVSync(renderer, vsync);
    if (renderer) tell_renderer(renderer);
    recoveries++;
    gpu_again = renderer && !is_gpu(renderer) && oracles_sdl_gpu_wanted() && oracles_sdl_window_away(window);
    if (renderer) SDL_ClearError();   /* else SDL's reason, for the renderer's users */
    char name[96];
    oracles_sdl_renderer_describe(renderer, name, sizeof name);
    fprintf(stderr, "oracles: %s: renderer %s made again, the filter %s %s%s, %.2f ms\n", why, name, oracles_sdl_filter_name(),
            drawable(chosen, renderer) ? "kept" : "not drawn", gpu_again ? ", the GPU renderer tried again at the window's return" : "",
            (double)(SDL_GetTicksNS() - started) / 1e6);
    return renderer;
}

/* ORACLES_SDL_LOG: SDL's own messages on the standard error with ours (Windows sends them to the debugger only), the
 * GPU's at every priority; with SDL_RENDER_GPU_DEBUG=1, the GPU backend's errors are written too. */
static void SDLCALL log_line(void *opaque, int category, SDL_LogPriority priority, const char *message)
{
    (void)opaque;
    fprintf(stderr, "oracles: SDL (category %d, priority %d): %s\n", category, (int)priority, message);
}

void oracles_sdl_log_start(void)
{
    if (!getenv("ORACLES_SDL_LOG")) return;
    SDL_SetLogOutputFunction(log_line, NULL);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, SDL_LOG_PRIORITY_VERBOSE);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_RENDER, SDL_LOG_PRIORITY_VERBOSE);
}

static const char *present_mode(SDL_Renderer *renderer, SDL_GPUDevice *device)
{
    SDL_Window *window = SDL_GetRenderWindow(renderer);
    int vsync = 0;
    SDL_GetRenderVSync(renderer, &vsync);
    if (!window) return "offscreen";
    switch (gpu_present_mode(renderer, device, window)) {
        case SDL_GPU_PRESENTMODE_MAILBOX: return "mailbox";
        case SDL_GPU_PRESENTMODE_IMMEDIATE: return "immediate";
        default: return vsync ? "fifo" : "fifo, the window having neither mailbox nor immediate";
    }
}

void oracles_sdl_renderer_describe(SDL_Renderer *renderer, char *out, size_t capacity)
{
    const char *name = renderer ? SDL_GetRendererName(renderer) : NULL;
    SDL_GPUDevice *device = name && !strcmp(name, SDL_GPU_RENDERER) ? SDL_GetGPURendererDevice(renderer) : NULL;
    const char *driver = device ? SDL_GetGPUDeviceDriver(device) : NULL;
    if (driver) snprintf(out, capacity, "%s/%s, present %s", name, driver, present_mode(renderer, device));
    else snprintf(out, capacity, "%s", name ? name : "none");
}

void oracles_sdl_renderer_report(SDL_Renderer *renderer, const char *what)
{
    char name[96];
    oracles_sdl_renderer_describe(renderer, name, sizeof name);
    SDL_GPUDevice *device = renderer && !strncmp(name, "gpu/", 4) ? SDL_GetGPURendererDevice(renderer) : NULL;
    const SDL_PropertiesID properties = device ? SDL_GetGPUDeviceProperties(device) : 0;
    const char *device_name = properties ? SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_NAME_STRING, NULL) : NULL;
    const char *driver = properties ? SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING, NULL) : NULL;
    const char *version = properties ? SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING, NULL) : NULL;
    if (device) fprintf(stderr, "oracles: %s renderer %s, device %s, driver %s %s\n", what, name, device_name ? device_name : "unknown", driver ? driver : "unknown",
                        version ? version : "");
    else fprintf(stderr, "oracles: %s renderer %s\n", what, name);
}

OraclesSdlFilter *oracles_sdl_filter_open(SDL_Renderer *renderer)
{
    const FilterCode *code = filter_chosen();
    if (!code || !code->spirv) return NULL;
    const unsigned bit = 1u << ((unsigned)(code - filters) + 1u);   /* settings.txt's index, after Off */
    SDL_GPUDevice *device = renderer ? SDL_GetGPURendererDevice(renderer) : NULL;
    if (!device || (refused_codes & bit)) return NULL;   /* no GPU renderer (the filters greyed), or a shader refused already */
    const SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) { info.format = SDL_GPU_SHADERFORMAT_SPIRV; info.code = code->spirv; info.code_size = code->spirv_size; }
    else if (formats & SDL_GPU_SHADERFORMAT_DXIL) { info.format = SDL_GPU_SHADERFORMAT_DXIL; info.code = code->dxil; info.code_size = code->dxil_size; }
    else if (formats & SDL_GPU_SHADERFORMAT_MSL) { info.format = SDL_GPU_SHADERFORMAT_MSL; info.code = code->msl; info.code_size = code->msl_size; }
    else {
        fprintf(stderr, "oracles: filter %s: no shader format the device takes\n", code->name);
        refused_codes |= bit;
        return NULL;
    }
    info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    info.num_samplers = 1;
    info.num_uniform_buffers = code->uniform_buffers;
    OraclesSdlFilter *filter = NULL;
    for (OraclesSdlFilter *f = kept; f && !filter; f = f->next) if (f->renderer == renderer && f->code == code) filter = f;
    if (!filter) {
        filter = calloc(1, sizeof *filter);
        if (!filter) return NULL;
        filter->renderer = renderer;
        filter->device = device;
        filter->code = code;
        filter->fits = 1;
        filter->shader = SDL_CreateGPUShader(device, &info);
        fprintf(stderr, "oracles: filter %s: shader %s (%s, %u bytes)%s%s\n", code->name, filter->shader ? "made" : "refused",
                info.format == SDL_GPU_SHADERFORMAT_SPIRV ? "SPIR-V" : info.format == SDL_GPU_SHADERFORMAT_DXIL ? "DXIL" : "MSL", (unsigned)info.code_size,
                filter->shader ? "" : ": ", filter->shader ? "" : SDL_GetError());
        SDL_GPURenderStateCreateInfo create;
        SDL_zero(create);
        create.fragment_shader = filter->shader;
        filter->state = filter->shader ? SDL_CreateGPURenderState(renderer, &create) : NULL;
        if (filter->shader && !filter->state) fprintf(stderr, "oracles: filter %s: render state refused: %s\n", code->name, SDL_GetError());
        for (int i = 0; i < 2 && filter->state && code->grid; i++) filter->parts[i] = SDL_CreateGPURenderState(renderer, &create);
        filter->next = kept;
        kept = filter;
    }
    /* Scanlines' strength and the thickness of its lines (a quarter of a source line, at least a pixel of the screen). */
    const float uniforms[12] = { 0.5f, 0.25f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    memcpy(filter->uniforms, uniforms, sizeof uniforms);
    int parts = filter->parts[0] && filter->parts[1];
    for (int i = 0; i < 2 && parts; i++) {
        memcpy(filter->part_uniforms[i], uniforms, sizeof uniforms);
        parts = SDL_SetGPURenderStateFragmentUniforms(filter->parts[i], 0, uniforms, sizeof uniforms);
    }
    for (int i = 0; i < 2 && !parts; i++) {
        /* The parts drawn through the picture's state, its constants changed between them. */
        if (filter->parts[i]) SDL_DestroyGPURenderState(filter->parts[i]);
        filter->parts[i] = NULL;
    }
    if (!filter->state || (code->uniform_buffers && !SDL_SetGPURenderStateFragmentUniforms(filter->state, 0, uniforms, sizeof uniforms))) {
        /* Refused once, refused for the run: the row greys the choice, its reason the device's. */
        fprintf(stderr, "oracles: filter %s refused by the device: %s\n", code->name, SDL_GetError());
        refused_codes |= bit;
        return NULL;
    }
    return filter;
}

void oracles_sdl_filter_close(OraclesSdlFilter *filter)
{
    if (filter) SDL_SetGPURenderState(filter->renderer, NULL);
}

void oracles_sdl_filter_forget(SDL_Renderer *renderer)
{
    for (OraclesSdlFilter **at = &kept; *at;) {
        OraclesSdlFilter *filter = *at;
        if (filter->renderer != renderer) { at = &filter->next; continue; }
        *at = filter->next;
        SDL_SetGPURenderState(renderer, NULL);
        if (filter->state) SDL_DestroyGPURenderState(filter->state);
        for (int i = 0; i < 2; i++) if (filter->parts[i]) SDL_DestroyGPURenderState(filter->parts[i]);
        if (filter->shader) SDL_ReleaseGPUShader(filter->device, filter->shader);
        free(filter);
    }
}

void oracles_sdl_filter_place(OraclesSdlFilter *filter, const SDL_FRect *image, float source_w, float source_h)
{
    if (!filter || !image) return;
    const float kx = source_w > 0.0f ? image->w / source_w : 0.0f, ky = source_h > 0.0f ? image->h / source_h : 0.0f;
    const int index = (int)(filter->code - filters) + 1;   /* settings.txt's, after Off */
    filter->fits = (kx < ky ? kx : ky) + 1e-3f >= (float)oracles_display_filter_min_scale[index];
    filter->cells[0] = kx; filter->cells[1] = ky;
    if (!filter->code->uniform_buffers) return;
    const float place[4] = { image->x, image->y, image->w, image->h };
    if (!memcmp(filter->uniforms + 4, place, sizeof place)) return;
    memcpy(filter->uniforms + 4, place, sizeof place);
    SDL_SetGPURenderStateFragmentUniforms(filter->state, 0, filter->uniforms, sizeof filter->uniforms);
}

int oracles_sdl_filter_active(const OraclesSdlFilter *filter) { return filter && filter->fits; }

/* `uniforms` with the grid moved by x, y texels, or (`cells`) the masks on the picture's own pixels, set to `state`,
 * when they change. */
static void set_grid(OraclesSdlFilter *filter, SDL_GPURenderState *state, float *uniforms, float x, float y, int cells)
{
    const float cell[4] = { cells ? filter->cells[0] : 0.0f, cells ? filter->cells[1] : 0.0f, cells ? 1.0f : 0.0f, 0.0f };
    if (filter->code->grid == 2) { x = SDL_floorf(x + 1e-3f); y = SDL_floorf(y + 1e-3f); }   /* a move's fraction is a screen pixel's at least */
    const float grid[2] = { x - 2.0f * SDL_floorf(x / 2.0f), y - 2.0f * SDL_floorf(y / 2.0f) };   /* CRT's columns alternate */
    if (!memcmp(uniforms + 2, grid, sizeof grid) && !memcmp(uniforms + 4, filter->uniforms + 4, 4 * sizeof *uniforms) && !memcmp(uniforms + 8, cell, sizeof cell))
        return;
    memcpy(uniforms + 2, grid, sizeof grid);
    memcpy(uniforms + 4, filter->uniforms + 4, 4 * sizeof *uniforms);   /* the picture's place, as placed last */
    memcpy(uniforms + 8, cell, sizeof cell);
    SDL_SetGPURenderStateFragmentUniforms(state, 0, uniforms, 12 * sizeof *uniforms);
}

static void use_part(OraclesSdlFilter *filter, int part, float x, float y, int cells)
{
    if (!filter) return;
    SDL_GPURenderState *state = filter->state;
    if (part >= 0 && part < 2 && filter->code->grid) {
        if (filter->parts[part]) set_grid(filter, state = filter->parts[part], filter->part_uniforms[part], x, y, cells);
        else set_grid(filter, state, filter->uniforms, x, y, cells);
    } else if (filter->code->grid && !filter->parts[0]) set_grid(filter, state, filter->uniforms, 0.0f, 0.0f, 0);
    SDL_SetGPURenderState(filter->renderer, filter->fits ? state : NULL);
}

void oracles_sdl_filter_part(OraclesSdlFilter *filter, int part, float x, float y) { use_part(filter, part, x, y, 0); }

void oracles_sdl_filter_part_cells(OraclesSdlFilter *filter, int part) { use_part(filter, part, 0.0f, 0.0f, 1); }

int oracles_sdl_filter_layer(const OraclesSdlFilter *filter) { return oracles_sdl_filter_active(filter) ? filter->code->layer : ORACLES_SDL_LAYER_ALONE; }

int oracles_sdl_filter_chosen_layer(const OraclesSdlFilter *filter) { return filter ? filter->code->layer : ORACLES_SDL_LAYER_ALONE; }

int oracles_sdl_filter_bend(const OraclesSdlFilter *filter, int width, int height)
{
    if (!filter || !filter->code->bends) return 0;   /* the filter chosen, large enough or not: a picture drawn later may be */
    /* CRT's curve moves a read by 0.0206 of the picture's side at most (crt.frag.hlsl, CURVENESS 0.3). */
    return (int)SDL_ceilf(0.021f * (float)(width > height ? width : height)) + 1;
}

void oracles_sdl_filter_use(OraclesSdlFilter *filter, int on)
{
    if (filter && !SDL_SetGPURenderState(filter->renderer, on && filter->fits ? filter->state : NULL))
        fprintf(stderr, "oracles: filter %s: SDL_SetGPURenderState: %s\n", filter->code->name, SDL_GetError());
}

void oracles_sdl_filter_check(OraclesSdlFilter *filter, int presented)
{
    /* Before the first present through the filter, SDL's error cleared; after it, whatever SDL set, which a pipeline
     * the backend refused leaves (the draw is dropped without a word). */
    if (!filter || filter->checked > 1) return;
    if (!presented) { SDL_ClearError(); filter->checked = 1; return; }
    const char *error = SDL_GetError();
    fprintf(stderr, "oracles: filter %s: first present %s%s\n", filter->code->name, error[0] ? "with SDL error: " : "without SDL error", error);
    filter->checked = 2;
}
