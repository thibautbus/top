/* The SDL 3 backend's presentation (backends.h): the frame at its whole scale or filling the screen, the band the
 * sub-pixel camera moves with Link's layer (through the logical presentation, or where the touch controls' and the
 * screen margin's own drawing puts the frame), and the text box Text size shows again larger. */
#include "sdl_backend_internal.h"

/* A frame whose band the sub-pixel camera moves (oracles_host_video_subpixel): the band drawn moved by the
 * fraction, at the screen's own pixels (rounded to the scale the logical presentation draws with), the sliver it
 * uncovers filled with its edge column or line; the lines above it (the status bar) and below it unmoved, over it;
 * then the patch moved with the band and the layer, moved by Link's own fraction, cut to the band; then the fixed
 * rectangles unmoved.  Link's own fraction is rounded down: against the band he stays less than a pixel of the frame
 * off the game's drawing of him.  At the band's own fraction (the band taking his, in a dungeon's large room) he takes
 * the band's pixel: rounded down against the band rounded to the nearest, he would go a pixel of the screen back and
 * forth on it.  Under twice the frame's size the screen has no pixel between two of the frame's to
 * show: the frame is drawn as it is, whole. */
static float screen_fraction(int32_t fraction, float scale)
{
    const float f = (float)fraction / 256.0f;
    return scale >= 1.0f ? SDL_roundf(f * scale) / scale : f;
}

static float screen_fraction_down(int32_t fraction, float scale)
{
    const float f = (float)fraction / 256.0f;
    return scale >= 1.0f ? SDL_floorf(f * scale) / scale : f;
}

/* A texture's part drawn at `dst`, in the frame's pixels: through the logical presentation (`to` NULL), or into `to`,
 * the frame's place in the window's pixels, as the logical presentation maps it there. */
int sb_put(SDL_Renderer *r, SDL_Texture *texture, const SDL_FRect *src, const SDL_FRect *dst, const SDL_FRect *to, float sx, float sy)
{
    if (!to) return SDL_RenderTexture(r, texture, src, dst);
    const SDL_FRect mapped = { to->x + dst->x * sx, to->y + dst->y * sy, dst->w * sx, dst->h * sy };
    return SDL_RenderTexture(r, texture, src, &mapped);
}

/* The band, the patch, the layer and the fixed rectangles of `sp` from the texture and the layer as they were last
 * uploaded (`layered`: the layer's upload succeeded); through the logical presentation, or into `to`, the frame's
 * place in the window's pixels, cut to it. */
int sb_draw_subpixel(sdl_backend *backend, const oracles_host_video_subpixel *sp, int layered, const SDL_FRect *to)
{
    SDL_Renderer *r = backend->renderer;
    const float w = (float)backend->frame_width, top = (float)sp->band_top, bh = (float)sp->band_height, h = (float)backend->frame_height;
    SDL_FRect area;
    float scale = 0.0f;
    if (to) scale = to->w / w;
    else if (SDL_GetRenderLogicalPresentationRect(r, &area) && area.w > 0.0f) scale = area.w / w;
    const float sx = to ? to->w / w : 1.0f, sy = to ? to->h / h : 1.0f;
    if (scale < 2.0f) return SDL_RenderTexture(r, backend->texture, NULL, to);
    const float fx = screen_fraction(sp->x, scale), fy = screen_fraction(sp->y, scale);
    SDL_FRect src = { 0.0f, top, w, bh }, dst = { -fx, top - fy, w, bh };
    if (!sb_put(r, backend->texture, &src, &dst, to, sx, sy)) return 0;
    if (fx != 0.0f) {
        src = (SDL_FRect){ fx > 0.0f ? w - 1.0f : 0.0f, top, 1.0f, bh };
        dst = (SDL_FRect){ fx > 0.0f ? w - fx : 0.0f, top - fy, fx > 0.0f ? fx : -fx, bh };
        if (!sb_put(r, backend->texture, &src, &dst, to, sx, sy)) return 0;
    }
    if (fy != 0.0f) {
        src = (SDL_FRect){ 0.0f, fy > 0.0f ? top + bh - 1.0f : top, w, 1.0f };
        dst = (SDL_FRect){ 0.0f, fy > 0.0f ? top + bh - fy : top, w, fy > 0.0f ? fy : -fy };
        if (!sb_put(r, backend->texture, &src, &dst, to, sx, sy)) return 0;
    }
    if (top > 0.0f) {
        src = (SDL_FRect){ 0.0f, 0.0f, w, top };
        if (!sb_put(r, backend->texture, &src, &src, to, sx, sy)) return 0;
    }
    if (h > top + bh) {
        src = (SDL_FRect){ 0.0f, top + bh, w, h - top - bh };
        if (!sb_put(r, backend->texture, &src, &src, to, sx, sy)) return 0;
    }
    if (layered) {
        SDL_ScaleMode mode = SDL_SCALEMODE_NEAREST;
        SDL_GetTextureScaleMode(backend->texture, &mode);
        SDL_SetTextureScaleMode(backend->layer, mode);
        /* The patch where the band puts the place he stands, then him at his own place; cut to the band: none of
         * him over the status bar or below the band. */
        const int half = (int)sp->layer_pitch;
        const float lx = sp->layer_fx ? screen_fraction_down(sp->layer_fx + sp->x, scale) - fx : 0.0f;
        const float ly = sp->layer_fy ? screen_fraction_down(sp->layer_fy + sp->y, scale) - fy : 0.0f;
        for (int pass = 0; pass < 2; pass++) {
            const float x0 = (float)sp->layer_x + (pass ? lx : -fx), y0 = (float)sp->layer_y + (pass ? ly : -fy);
            const float cut = top > y0 ? top - y0 : 0.0f;
            float lh = (float)sp->layer_h - cut;
            if (y0 + cut + lh > top + bh) lh = top + bh - y0 - cut;
            if (lh <= 0.0f) continue;
            src = (SDL_FRect){ pass ? 0.0f : (float)half, cut, (float)sp->layer_w, lh };
            dst = (SDL_FRect){ x0, y0 + cut, (float)sp->layer_w, lh };
            if (!sb_put(r, backend->layer, &src, &dst, to, sx, sy)) return 0;
        }
    }
    for (uint32_t i = 0; i < sp->fixed_count && i < ORACLES_HOST_SUBPIXEL_FIXED; i++) {
        src = (SDL_FRect){ (float)sp->fixed[i][0], (float)sp->fixed[i][1], (float)sp->fixed[i][2], (float)sp->fixed[i][3] };
        if (!sb_put(r, backend->texture, &src, &src, to, sx, sy)) return 0;
    }
    return 1;
}

/* The layer and the patch uploaded, the frame's move kept for the pause, which shows it as it was. */
static void take_subpixel(sdl_backend *backend, const oracles_host_video_frame *frame)
{
    const oracles_host_video_subpixel *sp = frame->subpixel;
    SDL_Renderer *r = backend->renderer;
    int layered = 0;
    if (sp->layer && sp->patch && sp->layer_w && sp->layer_h && sp->layer_pitch) {
        /* One texture, the layer in its left half, the patch in its right. */
        if (!backend->layer || backend->layer_w < 2 * (int)sp->layer_pitch || backend->layer_h < (int)sp->layer_pitch) {
            if (backend->layer) SDL_DestroyTexture(backend->layer);
            backend->layer_w = 2 * (int)sp->layer_pitch; backend->layer_h = (int)sp->layer_pitch;
            backend->layer = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, backend->layer_w, backend->layer_h);
            if (!backend->layer) backend->layer_w = backend->layer_h = 0;   /* him with the band for a frame rather than no frame */
            /* Premultiplied (its pixels opaque or wholly transparent, 0): Fill's smooth sampling blends his edges
             * toward what lies under them, not toward black. */
            else SDL_SetTextureBlendMode(backend->layer, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
        }
        const int half = (int)sp->layer_pitch;
        const SDL_Rect part = { 0, 0, (int)sp->layer_w, (int)sp->layer_h }, under = { half, 0, (int)sp->layer_w, (int)sp->layer_h };
        layered = backend->layer && SDL_UpdateTexture(backend->layer, &part, sp->layer, (int)(sp->layer_pitch * sizeof(uint32_t)))
                  && SDL_UpdateTexture(backend->layer, &under, sp->patch, (int)(sp->layer_pitch * sizeof(uint32_t)));
    }
    backend->shown = *sp;
    backend->shown.layer = backend->shown.patch = NULL;   /* the view's, which change: the layer's texture keeps them */
    backend->shown_layered = layered;
    backend->shown_subpixel = 1;
}

int sb_draw_subpixel_cut(sdl_backend *backend, const SDL_FRect *to)
{
    /* At its whole pixels and cut to it, as the logical presentation's viewport places and cuts the frame. */
    const SDL_FRect at = { SDL_floorf(to->x), SDL_floorf(to->y), to->w, to->h };
    const SDL_Rect cut = { (int)at.x, (int)at.y, (int)at.w, (int)at.h };
    const bool clipped = SDL_RenderClipEnabled(backend->renderer);
    SDL_Rect before = { 0, 0, 0, 0 };
    if (clipped) SDL_GetRenderClipRect(backend->renderer, &before);
    SDL_SetRenderClipRect(backend->renderer, &cut);
    const int drawn = sb_draw_subpixel(backend, &backend->shown, backend->shown_layered, &at);
    SDL_SetRenderClipRect(backend->renderer, clipped ? &before : NULL);
    return drawn;
}

/* The touch controls' and the screen margin's own drawing of the frame (touch_sdl.h's draw_frame), at `to` in the
 * screen's pixels: the band moved by the sub-pixel camera there, as through the logical presentation.  A part shown
 * alone, or a frame without a move, is left to the texture. */
static int draw_frame_direct(void *opaque, SDL_Renderer *renderer, const SDL_FRect *part, const SDL_FRect *to)
{
    (void)renderer;
    sdl_backend *backend = opaque;
    const oracles_host_video_frame *frame = backend->presenting;
    if (!frame || !frame->subpixel || part) return 0;
    take_subpixel(backend, frame);
    if (!sb_draw_subpixel_cut(backend, to)) { backend->shown_subpixel = 0; return 0; }
    return 1;
}

/* Text size Large or Very large: the frame's text box (oracles_host_video_frame's box) shown again above the frame, in
 * whole screen pixels, at its size or the next down where the frame drawn at `to` (screen pixels) holds it; `part` the
 * frame's part shown alone, NULL for the whole frame; `direct` when the screen's pixels are drawn to already, else the
 * logical presentation is set aside for it.  1 when it is drawn. */
static int draw_box_of(sdl_backend *backend, const oracles_host_video_frame *frame, const SDL_FRect *part, const SDL_FRect *to, int direct)
{
    if (!frame || !frame->box_w || !frame->box_h) return 0;
    const float ox = part ? part->x : 0.0f, oy = part ? part->y : 0.0f, lw = part ? part->w : (float)frame->width, lh = part ? part->h : (float)frame->height;
    if ((float)frame->box_x < ox || (float)frame->box_y < oy || (float)(frame->box_x + frame->box_w) > ox + lw || (float)(frame->box_y + frame->box_h) > oy + lh
        || lw <= 0.0f || to->w <= 0.0f) return 0;
    int32_t out[4];
    unsigned factor = 0;
    if (!oracles_text_box_enlarge(to->x, to->y, to->w, to->h, (double)to->w / (double)lw, frame->box_x - (uint32_t)ox, frame->box_y - (uint32_t)oy, frame->box_w,
                                  frame->box_h, frame->box_size, frame->box_bottom, out, &factor)) return 0;
    int logical_w = 0, logical_h = 0;
    SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    if (!direct) {
        SDL_GetRenderLogicalPresentation(backend->renderer, &logical_w, &logical_h, &mode);
        SDL_SetRenderLogicalPresentation(backend->renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);   /* the screen's pixels */
    }
    SDL_ScaleMode sampling = SDL_SCALEMODE_NEAREST;
    SDL_GetTextureScaleMode(backend->texture, &sampling);
    SDL_SetTextureScaleMode(backend->texture, SDL_SCALEMODE_NEAREST);   /* a whole number of screen pixels a pixel */
    const SDL_FRect source = { (float)frame->box_x, (float)frame->box_y, (float)frame->box_w, (float)frame->box_h };
    const SDL_FRect drawn = { (float)out[0], (float)out[1], (float)out[2], (float)out[3] };
    SDL_RenderTexture(backend->renderer, backend->texture, &source, &drawn);
    SDL_SetTextureScaleMode(backend->texture, sampling);
    if (!direct) SDL_SetRenderLogicalPresentation(backend->renderer, logical_w, logical_h, mode);
    return 1;
}

/* The frame being presented's box; kept for the pause, which shows it again over its image of the frame. */
static void draw_box(sdl_backend *backend, const SDL_FRect *to, int direct)
{
    const oracles_host_video_frame *frame = backend->presenting;
    if (!draw_box_of(backend, frame, backend->presenting_cropped ? &backend->presenting_part : NULL, to, direct)) return;
    backend->shown_box = *frame;
    backend->shown_box.pixels = NULL;
    backend->shown_box.subpixel = NULL;
    backend->boxes_enlarged++;
}

int sb_draw_shown_box(sdl_backend *backend, const SDL_FRect *to)
{
    return draw_box_of(backend, &backend->shown_box, backend->shown_cropped ? &backend->shown_part : NULL, to, 1);
}

/* The part of `frame` shown alone, its crop taking in Text size's box enlarged where the part's own scale holds it; 0
 * for the whole frame. */
static int part_of(sdl_backend *backend, const oracles_host_video_frame *frame, SDL_FRect *part)
{
    if (!frame->crop_w || !frame->crop_h) return 0;
    uint32_t crop_x = frame->crop_x, crop_w = frame->crop_w;
    int output_w = 0, output_h = 0;
    if (frame->box_reserve_w && SDL_GetRenderOutputSize(backend->renderer, &output_w, &output_h) && output_w > 0 && output_h > 0) {
        const OraclesTouchBox band = oracles_touch_band(output_w, output_h, &backend->touch.edges);   /* where the frame goes */
        oracles_text_box_reserve(frame->width, (uint32_t)band.w, (uint32_t)band.h, backend->fill && sb_window_fullscreen(backend), frame->box_reserve_w,
                                 frame->box_size, &crop_x, &crop_w, frame->crop_h);
    }
    *part = (SDL_FRect){ (float)crop_x, (float)frame->crop_y, (float)crop_w, (float)frame->crop_h };
    return 1;
}

void sb_show_crop(sdl_backend *backend, const uint32_t rect[4])
{
    oracles_host_video_frame *frame = &backend->shown_frame;
    if (!rect && !backend->shown_cropped) return;
    frame->crop_x = rect ? rect[0] : 0;
    frame->crop_y = rect ? rect[1] : 0;
    frame->crop_w = rect ? rect[2] : 0;
    frame->crop_h = rect ? rect[3] : 0;
    backend->shown_cropped = part_of(backend, frame, &backend->shown_part);
    backend->shown_box = *frame;   /* drawn again where the image holds it */
}

void sb_show_box(sdl_backend *backend, const uint32_t rect[4], int bottom, uint32_t reserve_w, unsigned size)
{
    oracles_host_video_frame *frame = &backend->shown_frame;
    const int inside = rect && rect[2] && rect[3] && rect[0] + rect[2] <= frame->width && rect[1] + rect[3] <= frame->height;   /* as the host hands a frame's */
    frame->box_x = inside ? rect[0] : 0;
    frame->box_y = inside ? rect[1] : 0;
    frame->box_w = inside ? rect[2] : 0;
    frame->box_h = inside ? rect[3] : 0;
    frame->box_bottom = inside && bottom;
    frame->box_reserve_w = reserve_w <= frame->width ? reserve_w : 0;
    frame->box_size = size;
    backend->shown_cropped = part_of(backend, frame, &backend->shown_part);   /* the part widened for the box, or no longer */
    backend->shown_box = *frame;   /* drawn again where the image holds it */
}

static void draw_box_direct(void *opaque, SDL_Renderer *renderer, const SDL_FRect *to)
{
    (void)renderer;
    draw_box(opaque, to, 1);
}

int sb_sdl_present_frame(void *opaque, const oracles_host_video_frame *frame)
{
    sdl_backend *backend = opaque;
    if (frame->pitch_bytes > INT_MAX) return 0;
    const uint64_t upload_started_ns = sb_sdl_monotonic_ns(backend);
    if (!SDL_UpdateTexture(backend->texture, NULL, frame->pixels, (int)frame->pitch_bytes)) return 0;
    const uint64_t draw_started_ns = sb_sdl_monotonic_ns(backend);
    /* A part shown alone (the game's menus enlarged) takes its own whole scale: the logical size follows it.  Fill
     * follows the window into fullscreen and out (F11), and a change of Scaling from the pause, whose menu lifted the
     * presentation and set back the one it found. */
    SDL_FRect part = { 0.0f, 0.0f, 0.0f, 0.0f };
    const int cropped = part_of(backend, frame, &part);   /* Text size: the part takes in the box enlarged where its own scale holds it */
    const int filled = backend->fill && sb_window_fullscreen(backend);
    const int logical_w = cropped ? (int)part.w : backend->frame_width, logical_h = cropped ? (int)part.h : backend->frame_height;
    backend->presenting = frame;
    backend->presenting_part = part;
    backend->presenting_cropped = cropped;
    backend->touch.over_frame = draw_box_direct;
    backend->touch.over_frame_opaque = backend;
    backend->touch.draw_frame = draw_frame_direct;
    backend->touch.draw_frame_opaque = backend;
    if (logical_w != backend->logical_w || logical_h != backend->logical_h || filled != backend->filled) sb_present_as(backend, logical_w, logical_h);
    backend->shown_subpixel = 0;
    backend->shown_box.box_w = 0;
    backend->shown_frame = *frame;
    backend->shown_frame.pixels = NULL;
    backend->shown_frame.subpixel = NULL;
    backend->shown_part = part;
    backend->shown_cropped = cropped;
    if (!oracles_touch_sdl_frame(&backend->touch, backend->renderer, backend->texture, cropped ? &part : NULL)) {   /* the touch controls' own drawing */
        if (!SDL_RenderClear(backend->renderer)) return 0;
        if (frame->subpixel && !cropped) {
            take_subpixel(backend, frame);
            if (!sb_draw_subpixel(backend, &backend->shown, backend->shown_layered, NULL)) return 0;
        }
        else if (!SDL_RenderTexture(backend->renderer, backend->texture, cropped ? &part : NULL, NULL)) return 0;
        SDL_FRect to;   /* the box above the band, Link and the fixed rectangles, at the frame's whole place */
        if (SDL_GetRenderLogicalPresentationRect(backend->renderer, &to)) draw_box(backend, &to, 0);
    }
    backend->presenting = NULL;
    if (backend->overlay && !backend->overlay(backend->overlay_opaque, backend->renderer)) backend->overlay = NULL;
    const uint64_t present_started_ns = sb_sdl_monotonic_ns(backend);
    SDL_RenderPresent(backend->renderer);
    backend->last_present_ns = sb_sdl_monotonic_ns(backend);
    backend->last_present_ns_spent = backend->last_present_ns - present_started_ns;
    const uint64_t steps[3] = { draw_started_ns - upload_started_ns, present_started_ns - draw_started_ns, backend->last_present_ns_spent };
    for (unsigned i = 0; i < 3; i++) {
        backend->step_ns_total[i] += steps[i];
        if (steps[i] > backend->step_ns_max[i]) backend->step_ns_max[i] = steps[i];
    }
    backend->stepped_presents++;
    if (!backend->presents) backend->first_present_ns = backend->last_present_ns;
    backend->presents++;
    return 1;
}
